#include "ltr/web/web_service.hpp"

#include <chrono>
#include <filesystem>
#include <random>
#include <sstream>

#include <nlohmann/json.hpp>

#include <SFML/Network/IpAddress.hpp>

#include "ltr/core/logger.hpp"
#include "ltr/core/types.hpp"
#include "ltr/infra/filesystem_service.hpp"
#include "ltr/web/streaming_zip_source.hpp"
#include "ltr/web/routes/route_registrar.hpp"

namespace ltr::web {

using namespace std::chrono;

namespace {

// `path::filename()` renvoie "" si le chemin se termine par '/'. On retire
// les séparateurs finaux pour récupérer le nom réel du dossier.
std::string folderDisplayName(const std::filesystem::path& p) {
    auto s = p.string();
    while (!s.empty() && (s.back() == '/' || s.back() == '\\')) {
        s.pop_back();
    }
    const auto pos = s.find_last_of("/\\");
    const auto name = (pos == std::string::npos) ? s : s.substr(pos + 1);
    return name.empty() ? p.string() : name;
}

} // namespace

WebService::WebService(core::EventBus& bus,
                       domain::Device self,
                       std::filesystem::path downloadDir,
                       int webAnnounceTimeoutSec)
    : bus_(bus),
      self_(std::move(self)),
      downloadDir_(std::move(downloadDir)),
      accessPin_(make6DigitPin()),
      announceTimeoutSec_(webAnnounceTimeoutSec) {}

WebService::~WebService() {
    stop();
}

std::string WebService::make6DigitPin() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> d(0, 999999);
    const int n = d(rng);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%06d", n);
    return std::string(buf);
}

void WebService::start() {
    if (running_.exchange(true)) return;

    // Enregistrement des routes (impl dans routes/route_registrar.cpp).
    routes::registerAll(*this);

    const std::uint16_t p = server_.start(
        "0.0.0.0", core::kWebPort, core::kWebPortFallbackRange);
    if (p == 0) {
        core::log_error("WebService: échec start — aucun port libre");
        running_.store(false);
        return;
    }
    port_.store(p);

    keepaliveThread_ = std::thread([this]{
        try { keepaliveLoop(); }
        catch (const std::exception& e) {
            core::log_error(std::string("WebService::keepalive exception: ") + e.what());
        } catch (...) {
            core::log_error("WebService::keepalive exception inconnue");
        }
    });

    core::log_info("WebService démarré — port=" + std::to_string(p)
                   + " pin=" + accessPin_);
}

void WebService::stop() {
    if (!running_.exchange(false)) return;
    broadcaster_.closeAll();
    server_.stop();
    if (keepaliveThread_.joinable()) keepaliveThread_.join();
}

void WebService::keepaliveLoop() {
    auto nextPing = steady_clock::now();
    int tickCount = 0;
    while (running_.load()) {
        std::this_thread::sleep_for(milliseconds(500));
        if (!running_.load()) break;

        const auto now = steady_clock::now();
        if (now < nextPing) continue;
        nextPing = now + core::kWebKeepaliveInterval;

        const auto snap = sessions_.snapshot();
        ++tickCount;
        if (tickCount % 5 == 1) {
            core::log_debug("[keepalive] tick=" + std::to_string(tickCount)
                            + " sessions=" + std::to_string(snap.size()));
        }

        for (const auto& s : snap) {
            bus_.post(core::PeerSeenEvent{s.device});
        }

        for (const auto& ev : sessions_.evictExpired()) {
            core::log_info("[keepalive] EVICT session token="
                           + ev.token.substr(0, 8)
                           + " device=" + ev.deviceId.substr(0, 8));
            broadcaster_.detach(ev.token);
            bus_.post(core::PeerLostEvent{ev.deviceId});
        }

        for (const auto& tkt : tickets_.evictExpired()) {
            core::log_info("[keepalive] EVICT ticket sess="
                           + tkt.sessionId + " name=" + tkt.displayName);
            bus_.post(core::TransferFailedEvent{tkt.sessionId, "expired"});
        }
    }
}

void WebService::pushFiles(const std::string& sessionToken,
                           const std::string& sessionId,
                           const std::vector<std::filesystem::path>& files) {
    if (sessionToken.empty()) {
        core::log_warn("WebService::pushFiles: sessionToken vide");
        return;
    }
    if (!sessions_.validate(sessionToken)) {
        core::log_warn("WebService::pushFiles: session inconnue/expirée");
        return;
    }

    bus_.post(core::TransferStartedEvent{sessionId});

    // V1.1.7 : le zip de dossier peut prendre plusieurs secondes (mode STORE
    // atténue mais ne supprime pas). On détache un thread worker pour que
    // le thread UI ne freeze pas. Le thread émet ensuite le SSE files-offer
    // une fois tous les tickets prêts.
    std::thread worker([this, sessionToken, sessionId, files]() {
        try {
            zipAndAnnounce(sessionToken, sessionId, files);
        } catch (const std::exception& e) {
            core::log_error(std::string("pushFiles worker exception: ")
                            + e.what());
            bus_.post(core::TransferFailedEvent{sessionId, "zip_failed"});
        } catch (...) {
            core::log_error("pushFiles worker exception inconnue");
            bus_.post(core::TransferFailedEvent{sessionId, "zip_failed"});
        }
    });
    worker.detach();
}

void WebService::zipAndAnnounce(
    const std::string& sessionToken,
    const std::string& sessionId,
    const std::vector<std::filesystem::path>& files) {

    using nlohmann::json;
    json payload;
    payload["type"] = "files-offer";
    payload["sessionId"] = sessionId;
    payload["files"] = json::array();

    std::uint64_t totalBytes = 0;
    int ticketsCount = 0;

    for (const auto& src : files) {
        std::error_code ec;
        if (std::filesystem::is_directory(src, ec)) {
            // V1.1.8 : plus de fichier temp. On walk le dossier, on snapshot
            // la liste des entrées, on calcule la taille zip précise et on
            // émet un ticket streaming-zip. Le zip sera assemblé à la volée
            // au moment du GET (aucune I/O disque tant que personne ne clique).
            const auto zipName = folderDisplayName(src) + ".zip";
            const auto bundleName = folderDisplayName(src);

            std::vector<ZipEntry> entries;
            std::error_code itEc;
            for (auto it = std::filesystem::recursive_directory_iterator(
                     src, std::filesystem::directory_options::skip_permission_denied,
                     itEc);
                 it != std::filesystem::recursive_directory_iterator();
                 ++it) {
                std::error_code fileEc;
                if (!it->is_regular_file(fileEc) || fileEc) continue;
                const auto rel = std::filesystem::relative(
                    it->path(), src, fileEc);
                if (fileEc) continue;
                const auto sz = std::filesystem::file_size(it->path(), fileEc);
                if (fileEc) continue;
                ZipEntry e;
                e.abs      = it->path();
                e.relInZip = bundleName + "/" + rel.generic_string();
                e.size     = sz;
                entries.push_back(std::move(e));
            }

            if (entries.empty()) {
                core::log_warn("zipAndAnnounce: dossier vide: " + src.string());
                continue;
            }

            const auto zipSize = StreamingZipSource::computeZipSize(entries);
            const auto ticketId = tickets_.issueStreamingZip(
                sessionToken, sessionId, std::move(entries), zipName, zipSize);
            payload["files"].push_back({
                {"ticketId", ticketId},
                {"name", zipName},
                {"size", zipSize},
            });
            totalBytes += zipSize;
            ++ticketsCount;
            core::log_info("zipAndAnnounce: folder streaming-zip, zipSize="
                           + std::to_string(zipSize) + " o, name=" + zipName);
        } else {
            const auto size = std::filesystem::file_size(src, ec);
            const auto displayName = src.filename().string();
            const auto ticketId = tickets_.issue(
                sessionToken, sessionId, src, displayName,
                ec ? 0 : size);
            payload["files"].push_back({
                {"ticketId", ticketId},
                {"name", displayName},
                {"size", ec ? 0 : size},
            });
            totalBytes += ec ? 0 : size;
            ++ticketsCount;
        }
    }

    if (ticketsCount == 0) {
        core::log_warn("zipAndAnnounce: 0 ticket émis");
        bus_.post(core::TransferFailedEvent{sessionId, "no_files"});
        return;
    }

    const auto msg = "event: files-offer\ndata: " + payload.dump() + "\n\n";
    broadcaster_.send(sessionToken, msg);

    core::log_info("zipAndAnnounce → " + std::to_string(ticketsCount)
                   + " ticket(s), session=" + sessionToken.substr(0, 8)
                   + "..., total=" + std::to_string(totalBytes) + " o");
}

void WebService::cancelSession(const std::string& sessionId) {
    std::shared_ptr<std::atomic<bool>> flag;
    {
        std::lock_guard<std::mutex> lock(cancelMu_);
        const auto it = cancelFlags_.find(sessionId);
        if (it == cancelFlags_.end()) return;   // pas de download actif
        flag = it->second;
    }
    flag->store(true);
    core::log_info("[web] cancelSession " + sessionId);
}

std::shared_ptr<std::atomic<bool>>
WebService::acquireCancelFlag(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(cancelMu_);
    auto& slot = cancelFlags_[sessionId];
    if (!slot) slot = std::make_shared<std::atomic<bool>>(false);
    return slot;
}

std::string WebService::localUrl() const {
    const auto p = port_.load();
    if (p == 0) return "";

    // self_.ip peut ne pas être encore renseigné (pas d'injection depuis le
    // beacon UDP). On résout l'IP LAN via SFML — renvoie la première
    // interface non-loopback (ex: 192.168.x.x) ou None si aucune.
    sf::IpAddress ip = self_.ip;
    if (ip == sf::IpAddress::None || ip == sf::IpAddress::Any
        || ip == sf::IpAddress::LocalHost) {
        ip = sf::IpAddress::getLocalAddress();
    }
    if (ip == sf::IpAddress::None || ip == sf::IpAddress::Any) {
        return "http://localhost:" + std::to_string(p);
    }
    return "http://" + ip.toString() + ":" + std::to_string(p);
}

} // namespace ltr::web
