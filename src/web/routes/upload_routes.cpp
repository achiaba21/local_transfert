#include "ltr/web/routes/upload_routes.hpp"

#include <chrono>
#include <fstream>
#include <memory>
#include <random>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "ltr/core/event_bus.hpp"
#include "ltr/core/logger.hpp"
#include "ltr/core/types.hpp"
#include "ltr/domain/transfer_request.hpp"
#include "ltr/infra/filesystem_service.hpp"
#include "ltr/web/routes/route_helpers.hpp"
#include "ltr/web/web_service.hpp"

namespace ltr::web::routes {

namespace {

std::string makeSessionId() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    auto hexByte = [](std::uint8_t v, std::string& out) {
        constexpr char kHex[] = "0123456789abcdef";
        out.push_back(kHex[(v >> 4) & 0xF]);
        out.push_back(kHex[v & 0xF]);
    };
    std::uint8_t b[16];
    for (int i = 0; i < 16; ++i) b[i] = static_cast<std::uint8_t>(rng() & 0xFF);
    b[6] = (b[6] & 0x0F) | 0x40;
    b[8] = (b[8] & 0x3F) | 0x80;
    std::string s; s.reserve(36);
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) s.push_back('-');
        hexByte(b[i], s);
    }
    return s;
}

// Matche le filename annoncé dans le ticket avec le filename multipart
// pour retrouver la taille totale (utile au calcul speed/ETA).
std::uint64_t lookupAnnouncedSize(const AnnounceSnapshot& snap,
                                  const std::string& filename) {
    for (const auto& f : snap.files) {
        if (f.name == filename) return f.size;
    }
    return 0;
}

// V1.1.7 : sanitize un relativePath reçu du client. Rejette les tentatives
// de path traversal (..), de chemin absolu, caractères interdits.
// Retourne "" si invalide.
std::string sanitizeRelativePath(const std::string& raw) {
    if (raw.empty()) return "";
    if (raw.size() > 1024) return "";                // pathologique
    if (raw.front() == '/' || raw.front() == '\\') return "";
    if (raw.find('\0') != std::string::npos) return "";
    // Windows drive letter
    if (raw.size() >= 2 && raw[1] == ':') return "";
    // Découpe en composants et rejette ".."
    std::string out;
    out.reserve(raw.size());
    std::string segment;
    for (char c : raw) {
        if (c == '/' || c == '\\') {
            if (segment == "..") return "";
            if (!segment.empty()) {
                if (!out.empty()) out.push_back('/');
                out += segment;
            }
            segment.clear();
        } else {
            segment.push_back(c);
        }
    }
    if (segment == "..") return "";
    if (!segment.empty()) {
        if (!out.empty()) out.push_back('/');
        out += segment;
    }
    return out;
}

} // namespace

void registerUpload(WebService& svc) {
    auto& server = svc.httpServer().raw();

    // ---- /api/upload-announce ----
    server.Post("/api/upload-announce", [&svc](const httplib::Request& req,
                                                 httplib::Response& res) {
        const auto token = readTokenCookie(req);
        core::log_info("[announce] POST /api/upload-announce token="
                       + (token.empty() ? std::string("<empty>")
                                        : token.substr(0, 8)));
        const auto sess = svc.sessions().validate(token);
        if (!sess) {
            res.status = 401;
            res.set_content("{\"error\":\"unauth\"}", "application/json");
            return;
        }
        svc.sessions().touch(token);

        std::vector<AnnounceFile> files;
        try {
            const auto j = nlohmann::json::parse(req.body);
            for (const auto& f : j.value("files", nlohmann::json::array())) {
                AnnounceFile af;
                af.name         = f.value("name", "");
                af.size         = f.value("size", std::uint64_t{0});
                af.relativePath = f.value("relativePath", ""); // V1.1.7
                if (!af.name.empty()) files.push_back(af);
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"bad_json\"}", "application/json");
            return;
        }
        if (files.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"no_files\"}", "application/json");
            return;
        }

        const auto uploadId = svc.announces().create(
            token, sess->device.name, files);

        core::log_info("[announce] created id=" + uploadId.substr(0, 8)
                       + " files=" + std::to_string(files.size()));

        core::WebIncomingOfferEvent ev;
        ev.uploadId      = uploadId;
        ev.senderName    = sess->device.name;
        ev.totalBytes    = 0;
        for (const auto& f : files) ev.totalBytes += f.size;
        ev.filesCount    = static_cast<int>(files.size());
        ev.firstFileName = files.front().name;
        svc.bus().post(std::move(ev));

        const auto decision = svc.announces().waitForDecision(
            uploadId, std::chrono::seconds(svc.announceTimeoutSec()));

        nlohmann::json out;
        out["uploadId"] = uploadId;
        switch (decision.decision) {
            case AnnounceDecision::Accepted:
                out["accepted"] = true;
                core::log_info("[announce] accepted id=" + uploadId.substr(0, 8));
                res.status = 200;
                break;
            case AnnounceDecision::Refused:
                out["accepted"] = false;
                out["reason"]   = "refused";
                core::log_info("[announce] refused id=" + uploadId.substr(0, 8));
                svc.announces().remove(uploadId);
                res.status = 403;
                break;
            case AnnounceDecision::TimedOut:
            default:
                out["accepted"] = false;
                out["reason"]   = "timeout";
                core::log_warn("[announce] timeout id=" + uploadId.substr(0, 8));
                // V1.1.9-batch : notifier l'UI pour retirer l'entrée
                // fantôme du webInbox (sinon le badge reste affiché
                // alors que le visiteur a déjà vu « pas de réponse »).
                svc.bus().post(core::WebOfferTimedOutEvent{uploadId});
                svc.announces().remove(uploadId);
                res.status = 408;
                break;
        }
        res.set_content(out.dump(), "application/json");
    });

    // ---- /api/upload (V1.1.3 : STREAMING chunked) ----
    // Utilise le handler 3-args de cpp-httplib (ContentReader) pour lire le
    // multipart au fur et à mesure — écriture disque incrémentale, pas de
    // bufferisation RAM du fichier complet. Supporte fichiers > 2 Go.
    server.Post("/api/upload",
        [&svc](const httplib::Request& req,
               httplib::Response& res,
               const httplib::ContentReader& content_reader) {
        using namespace std::chrono;

        const auto token = readTokenCookie(req);
        const auto sess = svc.sessions().validate(token);
        if (!sess) {
            res.status = 401;
            res.set_content("{\"error\":\"unauth\"}", "application/json");
            return;
        }
        svc.sessions().touch(token);

        std::string uploadId = req.get_header_value("X-Upload-Id");
        if (uploadId.empty() && req.has_param("upload_id")) {
            uploadId = req.get_param_value("upload_id");
        }
        if (uploadId.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"missing_upload_id\"}",
                            "application/json");
            return;
        }

        const auto anPtr = svc.announces().peek(uploadId);
        if (!anPtr) {
            res.status = 404;
            res.set_content("{\"error\":\"no_announce\"}", "application/json");
            return;
        }
        if (anPtr->sessionToken != token) {
            res.status = 403;
            res.set_content("{\"error\":\"wrong_session\"}", "application/json");
            return;
        }
        if (anPtr->decision != AnnounceDecision::Accepted) {
            res.status = 403;
            res.set_content("{\"error\":\"not_accepted\"}", "application/json");
            return;
        }
        if (!req.is_multipart_form_data()) {
            res.status = 400;
            res.set_content("{\"error\":\"not_multipart\"}", "application/json");
            return;
        }

        // État partagé entre les 2 callbacks du content_reader.
        struct UploadState {
            std::filesystem::path dest;
            std::string           displayName;    // nom relatif pour l'UI
            std::ofstream         out;
            std::uint64_t         totalExpected{0};
            std::uint64_t         bytesWritten{0};
            std::uint64_t         lastProgressBytes{0};
            steady_clock::time_point startTime;
            steady_clock::time_point lastProgressTs;
            std::string           sessionId;
            bool                  opened{false};
            bool                  failed{false};
            std::string           error;
        };
        auto state = std::make_shared<UploadState>();
        state->sessionId   = makeSessionId();
        state->startTime   = steady_clock::now();
        state->lastProgressTs = state->startTime;

        auto& bus = svc.bus();
        const auto senderName = sess->device.name;
        const auto targetDir  = anPtr->targetDir;
        const auto snap       = *anPtr;

        // V1.1.7 : relativePath fourni par le client (webkitdirectory).
        // Vide si le client envoie un fichier isolé. Permet de reconstruire
        // l'arborescence d'un dossier uploadé sans zip.
        std::string headerRelPath = req.get_header_value("X-Relative-Path");
        if (headerRelPath.empty() && req.has_param("relative_path")) {
            headerRelPath = req.get_param_value("relative_path");
        }
        const std::string relPathClean = sanitizeRelativePath(headerRelPath);

        // Le premier callback est invoqué au header de chaque part multipart.
        // Le second est invoqué pour chaque CHUNK de la part courante.
        const bool ok = content_reader(
            // --- Part header callback ---
            [state, &bus, senderName, targetDir, &snap, &relPathClean](
                const httplib::MultipartFormData& file) {
                if (file.name != "file") return true; // ignore d'autres parts
                if (state->opened) return true;       // une seule file

                std::filesystem::path rawName(file.filename);
                const auto basename = rawName.filename().string();
                if (basename.empty()) {
                    state->failed = true; state->error = "bad_filename";
                    return false;
                }

                // V1.1.7 : si relPath fourni, utilise-le (reconstruit arbo) ;
                // sinon fichier à plat à la racine.
                std::string effectiveRel = relPathClean.empty()
                    ? basename : relPathClean;
                state->displayName   = effectiveRel;
                state->totalExpected = lookupAnnouncedSize(snap, basename);

                const auto destFull = targetDir / effectiveRel;
                std::error_code ec;
                if (destFull.has_parent_path()) {
                    std::filesystem::create_directories(
                        destFull.parent_path(), ec);
                    if (ec) {
                        state->failed = true; state->error = "mkdir_failed";
                        core::log_error("[upload] mkdir failed: "
                                        + destFull.parent_path().string());
                        return false;
                    }
                }
                state->dest = infra::FilesystemService::uniqueTargetPath(
                    destFull.parent_path(), destFull.filename().string());

                state->out.open(state->dest, std::ios::binary);
                if (!state->out) {
                    state->failed = true; state->error = "open_failed";
                    core::log_error("[upload] open failed: "
                                    + state->dest.string());
                    return false;
                }
                state->opened = true;

                core::log_info("[upload] stream start '" + effectiveRel
                               + "' expected=" + std::to_string(state->totalExpected)
                               + " → " + state->dest.string());

                bus.post(core::WebUploadStartedEvent{
                    state->sessionId, senderName, effectiveRel, state->totalExpected });
                bus.post(core::TransferStartedEvent{state->sessionId});
                // Progress à 0 % pour faire sortir la card de "Proposed".
                bus.post(core::TransferProgressEvent{
                    state->sessionId, 0, 0.0, std::chrono::seconds(0)});
                return true;
            },
            // --- Body chunk callback ---
            [state, &bus](const char* data, std::size_t size) {
                if (state->failed || !state->opened) return !state->failed;
                state->out.write(data, static_cast<std::streamsize>(size));
                if (!state->out) {
                    state->failed = true; state->error = "write_failed";
                    return false;
                }
                state->bytesWritten += size;

                // Progress throttle : émet un event max toutes les 150 ms
                // OU tous les 1 Mo. Évite de noyer l'EventBus sur fichiers
                // rapides, tout en gardant une UX fluide.
                const auto now = steady_clock::now();
                const auto sinceLast = duration_cast<milliseconds>(
                    now - state->lastProgressTs).count();
                const bool byBytes = (state->bytesWritten -
                                      state->lastProgressBytes) >= (1 << 20);
                if (sinceLast >= 150 || byBytes) {
                    const auto elapsed = duration_cast<milliseconds>(
                        now - state->startTime).count();
                    const double speed = elapsed > 0
                        ? static_cast<double>(state->bytesWritten) * 1000.0
                          / static_cast<double>(elapsed)
                        : 0.0;
                    const auto remaining = (state->totalExpected > state->bytesWritten)
                        ? state->totalExpected - state->bytesWritten : 0;
                    const auto eta = (speed > 0.0)
                        ? std::chrono::seconds(
                            static_cast<long long>(remaining / speed))
                        : std::chrono::seconds(0);
                    bus.post(core::TransferProgressEvent{
                        state->sessionId, state->bytesWritten, speed, eta});
                    state->lastProgressTs    = now;
                    state->lastProgressBytes = state->bytesWritten;
                }
                return true;
            });

        // Fermeture + cleanup en cas d'erreur.
        if (state->out.is_open()) state->out.close();

        if (!ok || state->failed) {
            std::error_code ec;
            if (!state->dest.empty()) {
                std::filesystem::remove(state->dest, ec);
            }
            bus.post(core::TransferFailedEvent{
                state->sessionId,
                state->error.empty() ? std::string("upload_aborted") : state->error});
            res.status = 500;
            res.set_content(
                std::string("{\"error\":\"")
                + (state->error.empty() ? "upload_aborted" : state->error)
                + "\"}", "application/json");
            return;
        }
        if (!state->opened) {
            res.status = 400;
            res.set_content("{\"error\":\"no_file\"}", "application/json");
            return;
        }

        // Progress final à 100 % + Done.
        bus.post(core::TransferProgressEvent{
            state->sessionId, state->bytesWritten, 0.0,
            std::chrono::seconds(0)});
        bus.post(core::TransferDoneEvent{state->sessionId});

        core::log_info("[upload] stream done '" + state->displayName
                       + "' bytes=" + std::to_string(state->bytesWritten));

        nlohmann::json j;
        j["sessionId"] = state->sessionId;
        j["bytes"]     = state->bytesWritten;
        j["path"]      = state->dest.filename().string();
        res.set_content(j.dump(), "application/json");
    });
}

} // namespace ltr::web::routes
