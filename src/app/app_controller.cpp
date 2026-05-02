#include "ltr/app/app_controller.hpp"

#include "ltr/core/logger.hpp"
#include "ltr/core/shell_open.hpp"
#include "ltr/core/types.hpp"
#include "ltr/infra/filesystem_service.hpp"
#include "ltr/ui/clipboard_paste.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>

namespace ltr::app {

namespace {

// V1.1.5 : extrait le nom du dossier/fichier en retirant les séparateurs
// finaux. `std::filesystem::path::filename()` retourne "" si le path finit
// par '/' (ou '\\'), ce qui donnait "[D] /" dans l'UI.
std::string lastPathComponent(const std::filesystem::path& p) {
    auto s = p.string();
    while (!s.empty() && (s.back() == '/' || s.back() == '\\')) {
        s.pop_back();
    }
    const auto pos = s.find_last_of("/\\");
    const auto name = (pos == std::string::npos) ? s : s.substr(pos + 1);
    return name.empty() ? p.string() : name;
}

// V1.4 — Sprint Clipboard Paste : helpers (avant start() qui les utilise).
std::string nowTimestamp() {
    const auto t  = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(t);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

void ensureClipboardTempDir(const std::filesystem::path& dir) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        core::log_warn("clipboard tempDir create: " + ec.message());
    }
}

void purgeOldClipboardTemp(const std::filesystem::path& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;
    const auto now = fs::file_time_type::clock::now();
    const auto threshold = now - std::chrono::hours(24);
    for (auto it = fs::directory_iterator(dir, ec);
         it != fs::directory_iterator(); ++it) {
        std::error_code itec;
        const auto t = fs::last_write_time(it->path(), itec);
        if (!itec && t < threshold) {
            std::error_code rmec;
            fs::remove(it->path(), rmec);
        }
    }
}

std::string formatPin(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() * 2);
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (i) out.push_back(' ');
        out.push_back(raw[i]);
    }
    return out;
}

} // namespace

AppController::AppController() {
    cfg_ = infra::Config::loadOrCreate();
    state_.self.id       = cfg_.deviceId;
    state_.self.name     = cfg_.deviceName;
    state_.self.platform = cfg_.platform;
    state_.self.tcpPort  = core::kTransferPort;
    // V1.1.8-UX4 : hydrate l'état du SharePanel depuis la config.
    state_.sharePanelCollapsed = cfg_.sharePanelCollapsed;
}

AppController::~AppController() {
    stop();
}

void AppController::start() {
    discovery_ = std::make_unique<network::DiscoveryService>(bus_, state_.self);
    server_    = std::make_unique<network::TransferServer>(
        bus_, state_.self, cfg_.downloadDir, core::kTransferPort,
        cfg_.resumeSidecarTtlHours);
    client_    = std::make_unique<network::TransferClient>(bus_, state_.self);
    web_       = std::make_unique<web::WebService>(
        bus_, state_.self, cfg_.downloadDir, cfg_.webAnnounceTimeoutSec);

    server_->start();
    discovery_->start();
    web_->start();

    // V1.4 — Sprint Clipboard Paste : prépare le dossier temp et purge
    // les fichiers > 24h d'une exécution précédente.
    const auto cbDir = clipboardTempDir();
    ensureClipboardTempDir(cbDir);
    purgeOldClipboardTemp(cbDir);

    core::log_info("AppController démarré — device_id=" + cfg_.deviceId);
}

void AppController::stop() {
    shuttingDown_.store(true);
    if (discovery_) discovery_->stop();
    if (server_)    server_->stop();
    if (web_)       web_->stop();

    // Joindre les threads de timeout des probes en attente.
    {
        std::lock_guard<std::mutex> lock(probeMu_);
        for (auto& t : probeThreads_) {
            if (t.joinable()) t.join();
        }
        probeThreads_.clear();
    }

    // Client détruit via unique_ptr → destructeur attend les workers.
    discovery_.reset();
    server_.reset();
    client_.reset();
    web_.reset();
}

void AppController::tick() {
    auto events = bus_.drain();
    for (auto& e : events) {
        onEvent(e);
    }

    // V1.1.8-UX2 : auto-clean des cards terminées. Done → 10 s,
    // Failed/Cancelled → 30 s, Expired reste visible.
    using namespace std::chrono;
    const auto now = steady_clock::now();
    constexpr auto kDoneTtl   = seconds(10);
    constexpr auto kFailedTtl = seconds(30);

    auto it = state_.transfers.begin();
    while (it != state_.transfers.end()) {
        const auto& t = *it;
        if (t.terminalAt.time_since_epoch().count() == 0) { ++it; continue; }
        const auto age = now - t.terminalAt;
        const bool expire =
            (t.status == domain::TransferStatus::Done      && age > kDoneTtl) ||
            (t.status == domain::TransferStatus::Failed    && age > kFailedTtl) ||
            (t.status == domain::TransferStatus::Cancelled && age > kFailedTtl);
        if (expire) it = state_.transfers.erase(it);
        else        ++it;
    }
}

void AppController::toggleSelectPeer(const std::string& deviceId) {
    auto it = state_.selectedPeerIds.find(deviceId);
    if (it == state_.selectedPeerIds.end()) {
        state_.selectedPeerIds.insert(deviceId);
    } else {
        state_.selectedPeerIds.erase(it);
    }
}

void AppController::clearSelection() {
    state_.selectedPeerIds.clear();
}

void AppController::addFiles(
    const std::vector<std::filesystem::path>& paths) {
    for (const auto& p : paths) state_.inputPaths.push_back(p);

    // V1.1.5 : un item visible par chemin d'entrée. Pour un dossier, on
    // calcule la taille totale + nombre de fichiers par un walk récursif,
    // SANS l'expandre dans la liste visible. L'expansion réelle aura lieu
    // au moment du requestSend() via FilesystemService::enumerate().
    for (const auto& p : paths) {
        std::error_code ec;
        SelectedFile entry;
        entry.absolutePath = p;
        entry.checked      = true;

        if (std::filesystem::is_directory(p, ec)) {
            entry.kind        = SelectedFile::Kind::Folder;
            entry.displayName = lastPathComponent(p) + "/";

            // Walk récursif : compter les fichiers + somme tailles.
            std::uint64_t total = 0;
            int count = 0;
            for (auto it = std::filesystem::recursive_directory_iterator(
                     p, std::filesystem::directory_options::skip_permission_denied, ec);
                 it != std::filesystem::recursive_directory_iterator(); ++it) {
                std::error_code ec2;
                if (it->is_regular_file(ec2)) {
                    total += it->file_size(ec2);
                    ++count;
                }
            }
            entry.size      = total;
            entry.fileCount = count;
        } else {
            entry.kind        = SelectedFile::Kind::File;
            entry.displayName = lastPathComponent(p);
            entry.size        = std::filesystem::file_size(p, ec);
            if (ec) entry.size = 0;
            entry.fileCount   = 1;
        }

        state_.selectedFilesCheckedTotal += entry.size;
        ++state_.selectedFilesCheckedCount;
        state_.selectedFiles.push_back(std::move(entry));
    }
}

void AppController::clearFiles() {
    state_.inputPaths.clear();
    state_.selectedFiles.clear();
    state_.selectedFilesCheckedTotal = 0;
    state_.selectedFilesCheckedCount = 0;
}

void AppController::toggleFileCheck(const std::filesystem::path& abs) {
    for (auto& f : state_.selectedFiles) {
        if (f.absolutePath != abs) continue;
        f.checked = !f.checked;
        if (f.checked) {
            state_.selectedFilesCheckedTotal += f.size;
            ++state_.selectedFilesCheckedCount;
        } else {
            state_.selectedFilesCheckedTotal -= f.size;
            if (state_.selectedFilesCheckedCount > 0)
                --state_.selectedFilesCheckedCount;
        }
        return;
    }
}

bool AppController::canSend() const {
    return !state_.selectedPeerIds.empty() &&
           state_.selectedFilesCheckedCount > 0;
}

std::string AppController::makePinCode() const {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> d(0, 9999);
    const int n = d(rng);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%04d", n);
    return std::string(buf);
}

void AppController::requestSend() {
    if (!canSend()) return;

    try {
        currentPinCode_ = makePinCode();

        // V1.1 : ne prendre que les fichiers cochés. Les inputPaths d'origine
        // n'ont pas cette notion, donc on reconstruit la liste à envoyer.
        std::vector<std::filesystem::path> filesToSend;
        std::uint64_t totalChecked = 0;
        for (const auto& f : state_.selectedFiles) {
            if (!f.checked) continue;
            filesToSend.push_back(f.absolutePath);
            totalChecked += f.size;
        }
        if (filesToSend.empty()) return;

        for (const auto& id : state_.selectedPeerIds) {
            domain::Device target;
            for (const auto& p : state_.peers) {
                if (p.id == id) { target = p; break; }
            }
            if (target.id.empty()) {
                core::log_warn("requestSend: pair " + id + " introuvable");
                continue;
            }

            std::string sid;
            if (target.kind == domain::PeerKind::Web) {
                if (!web_) {
                    core::log_warn("requestSend web: WebService indisponible");
                    continue;
                }
                sid = makePinCode() + "-web-" + target.id.substr(0, 8);
                core::log_info("requestSend → web " + target.name
                               + " (session " + target.sessionToken.substr(0,8)
                               + "...)");
                web_->pushFiles(target.sessionToken, sid, filesToSend);
            } else {
                core::log_info("requestSend → " + target.name + " (" +
                               target.ip.toString() + ":" +
                               std::to_string(target.tcpPort) + ")");
                sid = client_->sendFiles(target, filesToSend, currentPinCode_);
            }

            // V1.1 : mémoriser les paths envoyés par session pour auto-clean.
            sessionPaths_[sid] = filesToSend;

            UiTransfer t;
            t.sessionId        = sid;
            t.peerName         = target.name;
            t.direction        = app::TransferDirection::Outgoing;
            t.totalBytes       = totalChecked;
            // V1.1 : statut initial "Proposed" pour web (attendre clic
            // visiteur), WaitingAcceptance pour natif (inchangé).
            t.status           = (target.kind == domain::PeerKind::Web)
                                 ? domain::TransferStatus::Proposed
                                 : domain::TransferStatus::WaitingAcceptance;
            // V1.1.9 : mémoriser les infos nécessaires pour resume.
            t.sourcePaths      = filesToSend;
            t.peerId           = target.id;
            t.pinCode          = currentPinCode_;
            state_.transfers.push_front(std::move(t));
        }
    } catch (const std::exception& e) {
        core::log_error(std::string("requestSend exception: ") + e.what());
        bus_.post(core::LogEvent{"error", std::string("Envoi: ") + e.what()});
    } catch (...) {
        core::log_error("requestSend exception inconnue");
    }
}

// V1.4 — Sprint Clipboard Paste : dossier temp dédié pour fichiers
// texte/image générés depuis le presse-papier. Cleanup au boot
// (>24 h), au shutdown, et après chaque envoi terminé.
std::filesystem::path AppController::clipboardTempDir() const {
    namespace fs = std::filesystem;
    return fs::temp_directory_path() / "ltr-clipboard";
}

void AppController::pasteFromClipboard() {
    namespace fs = std::filesystem;

    auto paste = ui::readClipboard();
    using K = ui::ClipboardPaste::Kind;

    if (paste.kind == K::None) {
        bus_.post(core::LogEvent{"info", "Presse-papier vide"});
        return;
    }

    if (paste.kind == K::Files) {
        addFiles(paste.files);
        bus_.post(core::LogEvent{"info",
            std::to_string(paste.files.size())
            + " fichier(s) ajouté(s) depuis le presse-papier"});
        return;
    }

    // Text ou Image : on écrit un fichier temp et on le passe à addFiles.
    const auto dir = clipboardTempDir();
    ensureClipboardTempDir(dir);

    fs::path target;
    if (paste.kind == K::Text) {
        target = dir / ("clipboard-" + nowTimestamp() + ".txt");
        std::ofstream ofs(target, std::ios::binary);
        if (!ofs) {
            bus_.post(core::LogEvent{"error",
                "Coller: impossible d'écrire le fichier temp"});
            return;
        }
        ofs.write(paste.text.data(),
                  static_cast<std::streamsize>(paste.text.size()));
        ofs.close();
        addFiles({target});
        bus_.post(core::LogEvent{"info", "Texte ajouté · "
            + std::to_string(paste.text.size()) + " octets"});
    } else if (paste.kind == K::Image) {
        const auto ext = paste.imageExt.empty() ? "png" : paste.imageExt;
        target = dir / ("clipboard-" + nowTimestamp() + "." + ext);
        std::ofstream ofs(target, std::ios::binary);
        if (!ofs) {
            bus_.post(core::LogEvent{"error",
                "Coller: impossible d'écrire le fichier temp"});
            return;
        }
        ofs.write(reinterpret_cast<const char*>(paste.imageBytes.data()),
                  static_cast<std::streamsize>(paste.imageBytes.size()));
        ofs.close();
        addFiles({target});
        bus_.post(core::LogEvent{"info",
            "Image " + std::string(ext) + " ajoutée · "
            + std::to_string(paste.imageBytes.size()) + " octets"});
    }
}

void AppController::cancelPending(const std::string& sessionId) {
    // Bouton Annuler côté desktop sur une card Proposed.
    for (auto& t : state_.transfers) {
        if (t.sessionId != sessionId) continue;
        if (t.status == domain::TransferStatus::Proposed) {
            t.status = domain::TransferStatus::Cancelled;
            t.terminalAt = std::chrono::steady_clock::now();
        }
    }
    // Retirer les paths mémorisés pour ce sid.
    sessionPaths_.erase(sessionId);
    // Note : les tickets côté WebService expireront naturellement (15 min).
    // Une invalidation active est possible mais non-bloquante V1.1.
    core::log_info("cancelPending: " + sessionId);
}

void AppController::acceptIncoming() {
    if (!state_.incomingOffer) return;
    const auto sid = state_.incomingOffer->sessionId;
    server_->acceptOffer(sid);

    UiTransfer t;
    t.sessionId        = sid;
    t.peerName         = state_.incomingOffer->senderName;
    t.direction        = app::TransferDirection::Incoming;
    t.totalBytes       = state_.incomingOffer->totalSize();
    t.status           = domain::TransferStatus::Accepted;
    state_.transfers.push_front(std::move(t));

    state_.incomingOffer.reset();
    state_.incomingPinDisplay.clear();
}

void AppController::rejectIncoming() {
    if (!state_.incomingOffer) return;
    server_->rejectOffer(state_.incomingOffer->sessionId, "user_refused");
    state_.incomingOffer.reset();
    state_.incomingPinDisplay.clear();
}

void AppController::acceptWebUpload() {
    if (!state_.pendingWebOffer || !web_) return;
    const auto uploadId = state_.pendingWebOffer->uploadId;

    // Dialog natif injecté par l'UI (tinyfiledialogs reste dans ltr_ui).
    std::string chosen;
    if (folderPicker_) chosen = folderPicker_(cfg_.downloadDir.string());

    if (chosen.empty()) {
        // Pas de handler OU utilisateur a annulé → refus implicite.
        web_->announces().resolveRefuse(uploadId);
        state_.pendingWebOffer.reset();
        return;
    }
    web_->announces().resolveAccept(uploadId, std::filesystem::path(chosen));
    state_.pendingWebOffer.reset();
}

void AppController::refuseWebUpload() {
    if (!state_.pendingWebOffer || !web_) return;
    web_->announces().resolveRefuse(state_.pendingWebOffer->uploadId);
    state_.pendingWebOffer.reset();
}

void AppController::cancelSession(const std::string& sessionId) {
    // 1) Natif TCP (envoi + réception).
    if (client_) client_->cancel(sessionId);
    if (server_) server_->cancelSession(sessionId);
    // 2) V1.1.8-UX2 : web streaming-zip/file (cancel flag atomique).
    if (web_)    web_->cancelSession(sessionId);
    // 3) Marqueur local immédiat (l'event TransferFailed arrivera aussi,
    //    cette mise à jour évite un délai perceptible côté UI).
    for (auto& t : state_.transfers) {
        if (t.sessionId == sessionId &&
            (t.status == domain::TransferStatus::InProgress ||
             t.status == domain::TransferStatus::Accepted)) {
            t.status = domain::TransferStatus::Cancelled;
            t.terminalAt = std::chrono::steady_clock::now();
        }
    }
    core::log_info("cancelSession: " + sessionId);
}

void AppController::openDownloadDir() {
    core::log_info("openDownloadDir: " + cfg_.downloadDir.string());
    core::openInFileManager(cfg_.downloadDir);
}

// V1.1.9-batch — gestion de l'inbox web (accept/refuse par id + all).
//
// FIX 2026-04-26 : `uploadIdRef` est une const-ref vers la string contenue
// dans state_.webInbox[i]. Si on erase cet élément, la mémoire est détruite
// (ou écrasée par le std::move du suivant) → use-after-free sur les
// usages ultérieurs. Solution : copie locale AVANT erase.
void AppController::acceptWebUpload(const std::string& uploadIdRef) {
    const std::string uploadId = uploadIdRef;  // copy locale, owned

    auto it = std::find_if(state_.webInbox.begin(), state_.webInbox.end(),
        [&](const auto& e){ return e.uploadId == uploadId; });
    if (it == state_.webInbox.end()) return;

    state_.webInbox.erase(it);

    std::string chosen;
    if (folderPicker_) chosen = folderPicker_(cfg_.downloadDir.string());

    if (chosen.empty()) {
        if (web_) web_->announces().resolveRefuse(uploadId);
        core::log_warn("acceptWebUpload picker cancelled, refused "
                       + uploadId.substr(0, 8));
        return;
    }
    if (web_) web_->announces().resolveAccept(uploadId,
        std::filesystem::path(chosen));
    core::log_info("acceptWebUpload " + uploadId.substr(0, 8) + " → " + chosen);
}

// FIX 2026-04-26 : voir acceptWebUpload pour le détail (use-after-erase).
void AppController::refuseWebUpload(const std::string& uploadIdRef) {
    const std::string uploadId = uploadIdRef;  // copy locale

    auto it = std::find_if(state_.webInbox.begin(), state_.webInbox.end(),
        [&](const auto& e){ return e.uploadId == uploadId; });
    if (it == state_.webInbox.end()) return;
    state_.webInbox.erase(it);
    if (web_) web_->announces().resolveRefuse(uploadId);
    core::log_info("refuseWebUpload " + uploadId.substr(0, 8));
}

void AppController::acceptAllWebUploads() {
    core::log_info("[inbox/diag] acceptAllWebUploads CALLED inbox.size="
                   + std::to_string(state_.webInbox.size()));
    if (state_.webInbox.empty()) return;

    std::string chosen;
    if (folderPicker_) chosen = folderPicker_(cfg_.downloadDir.string());
    core::log_info("[inbox/diag] acceptAll folder picker returned: '"
                   + (chosen.empty() ? std::string("<CANCELLED>") : chosen)
                   + "'");
    if (chosen.empty()) return;

    const std::filesystem::path dir(chosen);

    std::vector<std::string> ids;
    ids.reserve(state_.webInbox.size());
    for (const auto& e : state_.webInbox) ids.push_back(e.uploadId);
    state_.webInbox.clear();

    for (const auto& id : ids) {
        if (web_) {
            const bool ok = web_->announces().resolveAccept(id, dir);
            core::log_info("[inbox/diag] acceptAll resolveAccept("
                           + id.substr(0, 8) + ") returned "
                           + (ok ? "true" : "false"));
        }
    }
    core::log_info("[inbox/diag] acceptAllWebUploads × "
                   + std::to_string(ids.size()) + " done");
}

void AppController::refuseAllWebUploads() {
    if (state_.webInbox.empty()) return;
    std::vector<std::string> ids;
    ids.reserve(state_.webInbox.size());
    for (const auto& e : state_.webInbox) ids.push_back(e.uploadId);
    state_.webInbox.clear();
    for (const auto& id : ids) {
        if (web_) web_->announces().resolveRefuse(id);
    }
    core::log_info("refuseAllWebUploads × " + std::to_string(ids.size()));
}

void AppController::toggleWebInboxModal() {
    state_.webInboxModalOpen = !state_.webInboxModalOpen;
}

void AppController::resumeTransfer(const std::string& sessionId) {
    // Retrouver le UiTransfer + le peer dans state_.
    UiTransfer* t = nullptr;
    for (auto& x : state_.transfers) {
        if (x.sessionId == sessionId) { t = &x; break; }
    }
    if (!t || !t->resumable) return;

    domain::Device peer;
    for (const auto& p : state_.peers) {
        if (p.id == t->peerId) { peer = p; break; }
    }
    if (peer.id.empty()) {
        core::log_warn("resumeTransfer: pair " + t->peerId
                       + " introuvable — relance impossible");
        return;
    }

    if (t->sourcePaths.empty()) {
        core::log_warn("resumeTransfer: sourcePaths vide");
        return;
    }

    core::log_info("resumeTransfer → " + t->peerName + " sid="
                   + sessionId.substr(0, 8) + "...");

    // Marquer la card en cours pour feedback immédiat.
    t->status          = domain::TransferStatus::WaitingAcceptance;
    t->resumable       = false;
    t->terminalAt      = {};
    t->error           = "";

    if (client_) {
        client_->resumeSession(sessionId, peer, t->sourcePaths, t->pinCode);
    }
}

void AppController::resumeAllTransfers() {
    // Q1=A : séquentiel 1 par 1. On itère sur une copie car resumeTransfer
    // mute state_.transfers (status change).
    std::vector<std::string> resumables;
    for (const auto& t : state_.transfers) {
        if (t.status == domain::TransferStatus::Failed && t.resumable) {
            resumables.push_back(t.sessionId);
        }
    }
    core::log_info("resumeAllTransfers: " + std::to_string(resumables.size())
                   + " transfert(s) à relancer");
    for (const auto& sid : resumables) {
        resumeTransfer(sid);
    }
}

void AppController::ignoreTransfer(const std::string& sessionId) {
    // Q4 : passif. On retire la card ; le sidecar côté receveur expire
    // naturellement à 24 h.
    for (auto it = state_.transfers.begin(); it != state_.transfers.end();) {
        if (it->sessionId == sessionId) {
            it = state_.transfers.erase(it);
        } else {
            ++it;
        }
    }
}

void AppController::toggleSharePanel() {
    state_.sharePanelCollapsed = !state_.sharePanelCollapsed;
    cfg_.sharePanelCollapsed   = state_.sharePanelCollapsed;
    cfg_.save();
    core::log_info(std::string("sharePanel: ") +
        (state_.sharePanelCollapsed ? "collapsed" : "expanded"));
}

void AppController::rescan() {
    if (discovery_) discovery_->rescan();
}

bool AppController::isScanning() const noexcept {
    return discovery_ ? discovery_->isScanning() : false;
}

void AppController::probePeer(const std::string& ipv4) {
    if (!discovery_) return;

    sf::IpAddress ip(ipv4);
    if (ip == sf::IpAddress::None) {
        bus_.post(core::LogEvent{"warn", "IP invalide: " + ipv4});
        return;
    }
    if (ip == sf::IpAddress::Any) {
        bus_.post(core::LogEvent{"warn", "IP 0.0.0.0 rejetée"});
        return;
    }
    if (ip == sf::IpAddress::Broadcast) {
        bus_.post(core::LogEvent{"warn", "Broadcast rejeté"});
        return;
    }
    if ((ip.toInteger() & 0xFF000000u) == 0x7F000000u) {
        bus_.post(core::LogEvent{"warn", "Loopback rejeté"});
        return;
    }

    bus_.post(core::LogEvent{"info", "Probe → " + ipv4});
    discovery_->probe(ip);

    // Thread de timeout : sleep 2s par tranches de 100ms pour permettre un
    // shutdown rapide, puis log si le pair n'a pas répondu.
    std::lock_guard<std::mutex> lock(probeMu_);
    probeThreads_.emplace_back([this, ip, ipv4]() {
        for (int i = 0; i < 20; ++i) {
            if (shuttingDown_.load()) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (shuttingDown_.load()) return;
        if (discovery_ && !discovery_->hasPeer(ip)) {
            bus_.post(core::LogEvent{"warn", "Pair " + ipv4 + " introuvable"});
        }
    });
}

AppController::WebShareInfo AppController::webShareInfo() const {
    WebShareInfo info;
    if (!web_) return info;
    info.url  = web_->localUrl();
    info.pin  = web_->accessPin();
    info.port = web_->port();
    return info;
}

void AppController::onEvent(const core::Event& ev) {
    std::visit([this](auto const& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, core::PeerSeenEvent>) {
            auto& peers = state_.peers;
            auto it = std::find_if(peers.begin(), peers.end(),
                [&](const domain::Device& d){ return d.id == e.device.id; });
            if (it == peers.end()) peers.push_back(e.device);
            else                   *it = e.device;
        }
        else if constexpr (std::is_same_v<T, core::PeerLostEvent>) {
            auto& peers = state_.peers;
            peers.erase(std::remove_if(peers.begin(), peers.end(),
                [&](const domain::Device& d){ return d.id == e.deviceId; }),
                peers.end());
            state_.selectedPeerIds.erase(e.deviceId);
        }
        else if constexpr (std::is_same_v<T, core::IncomingOfferEvent>) {
            state_.incomingOffer     = e.request;
            state_.incomingPinDisplay = formatPin(e.request.pinCode);
        }
        else if constexpr (std::is_same_v<T, core::WebUploadStartedEvent>) {
            // V1.1.2 : upload web (après announce accepté) — crée la
            // UiTransfer pour afficher la progression dans la barre basse.
            UiTransfer t;
            t.sessionId        = e.sessionId;
            t.peerName         = e.senderName;
            t.direction        = app::TransferDirection::Incoming;
            t.totalBytes       = e.totalBytes;
            t.bytesTransferred = 0;
            t.status           = domain::TransferStatus::InProgress;
            state_.transfers.push_front(std::move(t));
        }
        else if constexpr (std::is_same_v<T, core::WebIncomingOfferEvent>) {
            // V1.1.9-batch : push dans la file d'attente inbox.
            AppState::WebInboxEntry entry;
            entry.uploadId      = e.uploadId;
            entry.senderName    = e.senderName;
            entry.totalBytes    = e.totalBytes;
            entry.filesCount    = e.filesCount;
            entry.firstFileName = e.firstFileName;
            state_.webInbox.push_back(std::move(entry));
            state_.webInboxFadeSec = 0.f;
            core::log_info("[inbox] +entry uploadId="
                + e.uploadId.substr(0, 8) + " from " + e.senderName
                + " files=" + std::to_string(e.filesCount)
                + " (inbox total=" + std::to_string(state_.webInbox.size())
                + ")");
        }
        else if constexpr (std::is_same_v<T, core::WebOfferTimedOutEvent>) {
            // V1.1.9-batch : announce timeout côté backend → retirer
            // l'entrée fantôme du webInbox.
            auto it = std::find_if(state_.webInbox.begin(),
                state_.webInbox.end(),
                [&](const auto& x){ return x.uploadId == e.uploadId; });
            if (it != state_.webInbox.end()) {
                state_.webInbox.erase(it);
                core::log_info("[inbox] -entry timeout uploadId="
                    + e.uploadId.substr(0, 8)
                    + " (inbox total=" + std::to_string(state_.webInbox.size())
                    + ")");
            }
        }
        else if constexpr (std::is_same_v<T, core::OfferAnsweredEvent>) {
            for (auto& t : state_.transfers) {
                if (t.sessionId != e.sessionId) continue;
                if (e.accepted) t.status = domain::TransferStatus::InProgress;
                else {
                    t.status = domain::TransferStatus::Rejected;
                    t.error  = e.reason;
                }
            }
        }
        else if constexpr (std::is_same_v<T, core::TransferStartedEvent>) {
            for (auto& t : state_.transfers) {
                if (t.sessionId != e.sessionId) continue;
                if (t.status == domain::TransferStatus::Pending)
                    t.status = domain::TransferStatus::WaitingAcceptance;
            }
        }
        else if constexpr (std::is_same_v<T, core::TransferProgressEvent>) {
            for (auto& t : state_.transfers) {
                if (t.sessionId != e.sessionId) continue;
                t.bytesTransferred = e.bytes;
                t.speedBps         = e.speedBps;
                t.eta              = e.eta;
                // V1.1 : passage Proposed → InProgress dès le 1er Progress.
                if (t.status != domain::TransferStatus::Done &&
                    t.status != domain::TransferStatus::Failed &&
                    t.status != domain::TransferStatus::Cancelled &&
                    t.status != domain::TransferStatus::Expired) {
                    t.status = domain::TransferStatus::InProgress;
                }
            }
        }
        else if constexpr (std::is_same_v<T, core::TransferDoneEvent>) {
            for (auto& t : state_.transfers) {
                if (t.sessionId == e.sessionId) {
                    t.status = domain::TransferStatus::Done;
                    t.terminalAt = std::chrono::steady_clock::now();
                }
            }
            // V1.1 : auto-clean des fichiers envoyés pour cette session.
            auto it = sessionPaths_.find(e.sessionId);
            if (it != sessionPaths_.end()) {
                const auto cbDir = clipboardTempDir();
                for (const auto& p : it->second) {
                    auto& list = state_.selectedFiles;
                    for (auto fit = list.begin(); fit != list.end(); ) {
                        if (fit->absolutePath == p) {
                            if (fit->checked) {
                                if (state_.selectedFilesCheckedTotal >= fit->size)
                                    state_.selectedFilesCheckedTotal -= fit->size;
                                if (state_.selectedFilesCheckedCount > 0)
                                    --state_.selectedFilesCheckedCount;
                            }
                            fit = list.erase(fit);
                        } else {
                            ++fit;
                        }
                    }
                    // V1.4 — Sprint Clipboard Paste : si le path est un
                    // fichier temp clipboard, on le supprime physiquement
                    // après envoi réussi (texte/image collé).
                    std::error_code ec;
                    if (p.parent_path() == cbDir
                        && std::filesystem::exists(p, ec)) {
                        std::filesystem::remove(p, ec);
                    }
                }
                sessionPaths_.erase(it);
            }
        }
        else if constexpr (std::is_same_v<T, core::TransferFailedEvent>) {
            for (auto& t : state_.transfers) {
                if (t.sessionId != e.sessionId) continue;
                // V1.1 : statut Expired si reason == "expired",
                //        Cancelled si reason == "cancelled", sinon Failed.
                if (e.reason == "expired") {
                    t.status = domain::TransferStatus::Expired;
                } else if (e.reason == "cancelled" ||
                           e.category == core::ErrorCategory::Cancelled) {
                    t.status = domain::TransferStatus::Cancelled;
                } else {
                    t.status = domain::TransferStatus::Failed;
                }
                t.error  = e.reason;

                // V1.1.9 : classifier + marquer resumable selon category.
                switch (e.category) {
                    case core::ErrorCategory::Network:
                        t.lastErrorCategory = "network"; t.resumable = true; break;
                    case core::ErrorCategory::Protocol:
                        t.lastErrorCategory = "protocol"; t.resumable = true; break;
                    case core::ErrorCategory::Permanent:
                        t.lastErrorCategory = "permanent"; t.resumable = false; break;
                    case core::ErrorCategory::Cancelled:
                        t.lastErrorCategory = "cancelled"; t.resumable = false; break;
                    case core::ErrorCategory::Unknown:
                    default:
                        t.lastErrorCategory = "unknown";
                        // Unknown par défaut traité comme resumable
                        // (safe default) SAUF si Cancelled détecté via reason.
                        t.resumable = (t.status == domain::TransferStatus::Failed);
                        break;
                }

                // V1.1.8-UX2 : Expired reste affiché (pas de terminalAt) ;
                // Failed/Cancelled s'auto-clean après 30 s.
                if (t.status != domain::TransferStatus::Expired) {
                    t.terminalAt = std::chrono::steady_clock::now();
                }
            }
            // Ne pas auto-clean en cas d'échec : le fichier reste disponible
            // pour retry.
        }
        else if constexpr (std::is_same_v<T, core::LogEvent>) {
            state_.logTail.push_back("[" + e.level + "] " + e.message);
            while (state_.logTail.size() > 50) state_.logTail.pop_front();
        }
    }, ev);
}

} // namespace ltr::app
