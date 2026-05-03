#include "ltr/web/routes/download_routes.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <memory>
#include <string>

#include <httplib.h>

#include "ltr/core/event_bus.hpp"
#include "ltr/core/logger.hpp"
#include "ltr/core/types.hpp"
#include "ltr/web/streaming_zip_source.hpp"
#include "ltr/web/web_service.hpp"
#include "ltr/web/routes/multi_server.hpp"
#include "ltr/web/routes/route_helpers.hpp"

namespace ltr::web::routes {

namespace {

std::string rfc5987Encode(const std::string& s) {
    std::string out;
    for (char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_') {
            out.push_back(c);
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X",
                          static_cast<unsigned char>(c));
            out.append(buf);
        }
    }
    return out;
}

// Throttle des TransferProgressEvent émis vers l'UI.
//   - au moins 100 ms entre 2 events
//   - OU 1 MB poussé depuis le dernier event
constexpr std::chrono::milliseconds kProgressInterval{100};
constexpr std::uint64_t kProgressByteStep = 1 * 1024 * 1024;

// V1.1.8-UX2 : acquiert le flag de cancel partagé via WebService.
// Le provider l'observe à chaque chunk ; le host peut y écrire true
// depuis le thread UI pour interrompre proprement.

// Dispatch : kind=File → stream disque classique.
void streamFile(const DownloadTicket& tkt, WebService& svc,
                httplib::Response& res) {
    std::error_code ec;
    const auto sz = std::filesystem::file_size(tkt.path, ec);
    if (ec) {
        svc.bus().post(core::TransferFailedEvent{
            tkt.sessionId, "file_size failed"});
        res.status = 500;
        return;
    }

    const auto filename = rfc5987Encode(tkt.displayName);
    res.set_header("Content-Disposition",
        "attachment; filename*=UTF-8''" + filename);
    res.set_header("Content-Length", std::to_string(sz));
    res.set_header("Cache-Control", "no-store");

    auto fileStream = std::make_shared<std::ifstream>(
        tkt.path, std::ios::binary);
    if (!fileStream->is_open()) {
        svc.bus().post(core::TransferFailedEvent{
            tkt.sessionId, "open failed"});
        res.status = 500;
        return;
    }

    auto sessionId = tkt.sessionId;
    auto& bus = svc.bus();
    auto sentTotal = std::make_shared<std::uint64_t>(0);
    auto doneEmitted = std::make_shared<bool>(false);
    auto progressStarted = std::make_shared<bool>(false);
    auto lastProgressTime = std::make_shared<std::chrono::steady_clock::time_point>(
        std::chrono::steady_clock::now());
    auto lastProgressBytes = std::make_shared<std::uint64_t>(0);
    auto cancelFlag = svc.acquireCancelFlag(sessionId);
    const auto startTime = std::chrono::steady_clock::now();

    res.set_content_provider(
        sz, "application/octet-stream",
        [fileStream, sentTotal, doneEmitted, progressStarted,
         lastProgressTime, lastProgressBytes, cancelFlag,
         sessionId, sz, startTime, &bus](
            std::size_t /*offset*/, std::size_t length,
            httplib::DataSink& sink) {
            // V1.1.8-UX2 : vérif cancel avant chaque chunk.
            if (cancelFlag->load()) {
                if (!*doneEmitted) {
                    bus.post(core::TransferFailedEvent{sessionId, "cancelled"});
                    *doneEmitted = true;
                }
                return false;
            }

            std::vector<char> buf(
                std::min<std::size_t>(length, core::kHttpChunkSize));
            fileStream->read(buf.data(),
                static_cast<std::streamsize>(buf.size()));
            const auto got = static_cast<std::size_t>(
                fileStream->gcount());
            if (got == 0) {
                if (!*doneEmitted) {
                    bus.post(core::TransferDoneEvent{sessionId});
                    *doneEmitted = true;
                }
                sink.done();
                return false;
            }

            if (!*progressStarted) {
                bus.post(core::TransferProgressEvent{
                    sessionId, 0, 0.0, std::chrono::seconds(0)});
                *progressStarted = true;
            }

            if (!sink.write(buf.data(), got)) {
                if (!*doneEmitted) {
                    bus.post(core::TransferFailedEvent{sessionId, "cancelled"});
                    *doneEmitted = true;
                }
                return false;
            }
            *sentTotal += got;

            const auto now = std::chrono::steady_clock::now();
            const auto sinceLast = now - *lastProgressTime;
            const auto bytesSinceLast = *sentTotal - *lastProgressBytes;
            if (sinceLast >= kProgressInterval
                || bytesSinceLast >= kProgressByteStep) {
                const auto elapsedMs =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - startTime).count();
                const double speedBps = elapsedMs > 0
                    ? (static_cast<double>(*sentTotal) * 1000.0
                       / static_cast<double>(elapsedMs))
                    : 0.0;
                const auto remaining = (sz > *sentTotal)
                    ? (sz - *sentTotal) : 0;
                const auto eta = (speedBps > 0.0)
                    ? std::chrono::seconds(static_cast<long long>(
                        static_cast<double>(remaining) / speedBps))
                    : std::chrono::seconds(0);
                bus.post(core::TransferProgressEvent{
                    sessionId, *sentTotal, speedBps, eta});
                *lastProgressTime  = now;
                *lastProgressBytes = *sentTotal;
            }
            return true;
        });
}

// Dispatch : kind=StreamingZip → génération zip à la volée.
void streamZip(const DownloadTicket& tkt, WebService& svc,
               httplib::Response& res) {
    const auto filename = rfc5987Encode(tkt.displayName);
    res.set_header("Content-Disposition",
        "attachment; filename*=UTF-8''" + filename);
    res.set_header("Content-Length", std::to_string(tkt.size));
    res.set_header("Cache-Control", "no-store");

    auto source = std::make_shared<StreamingZipSource>(tkt.zipEntries);
    auto sessionId = tkt.sessionId;
    auto& bus = svc.bus();
    auto doneEmitted = std::make_shared<bool>(false);
    auto progressStarted = std::make_shared<bool>(false);
    auto lastProgressTime = std::make_shared<std::chrono::steady_clock::time_point>(
        std::chrono::steady_clock::now());
    auto lastProgressBytes = std::make_shared<std::uint64_t>(0);
    auto cancelFlag = svc.acquireCancelFlag(sessionId);
    const auto totalSize = tkt.size;
    const auto startTime = std::chrono::steady_clock::now();

    res.set_content_provider(
        tkt.size, "application/zip",
        [source, sessionId, doneEmitted, progressStarted,
         lastProgressTime, lastProgressBytes, cancelFlag,
         totalSize, startTime, &bus](
            std::size_t /*offset*/, std::size_t length,
            httplib::DataSink& sink) {
            // V1.1.8-UX2 : vérif cancel avant le provide.
            if (cancelFlag->load()) {
                if (!*doneEmitted) {
                    bus.post(core::TransferFailedEvent{sessionId, "cancelled"});
                    *doneEmitted = true;
                }
                return false;
            }

            if (!*progressStarted) {
                bus.post(core::TransferProgressEvent{
                    sessionId, 0, 0.0, std::chrono::seconds(0)});
                *progressStarted = true;
            }

            const auto more = source->provide(sink, length);

            if (!more) {
                if (source->errored()) {
                    if (!*doneEmitted) {
                        const auto reason = source->errorMsg()
                            == "sink_write_failed" ||
                            source->errorMsg().find("sink_write") == 0
                            ? "cancelled" : "source_error";
                        bus.post(core::TransferFailedEvent{sessionId, reason});
                        *doneEmitted = true;
                    }
                    return false;
                }
                if (!*doneEmitted) {
                    bus.post(core::TransferDoneEvent{sessionId});
                    *doneEmitted = true;
                }
                sink.done();
                return false;
            }

            const auto sent = source->bytesWritten();
            const auto now = std::chrono::steady_clock::now();
            const auto sinceLast = now - *lastProgressTime;
            const auto bytesSinceLast = sent - *lastProgressBytes;
            if (sinceLast >= kProgressInterval
                || bytesSinceLast >= kProgressByteStep) {
                const auto elapsedMs =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - startTime).count();
                const double speedBps = elapsedMs > 0
                    ? (static_cast<double>(sent) * 1000.0
                       / static_cast<double>(elapsedMs))
                    : 0.0;
                const auto remaining = (totalSize > sent)
                    ? (totalSize - sent) : 0;
                const auto eta = (speedBps > 0.0)
                    ? std::chrono::seconds(static_cast<long long>(
                        static_cast<double>(remaining) / speedBps))
                    : std::chrono::seconds(0);
                bus.post(core::TransferProgressEvent{
                    sessionId, sent, speedBps, eta});
                *lastProgressTime  = now;
                *lastProgressBytes = sent;
            }
            return true;
        });
}

} // namespace

void registerDownload(WebService& svc) {
    auto server = routes::routerOf(svc);

    server.Get(R"(/api/download/([0-9a-f]{32}))",
               [&svc](const httplib::Request& req, httplib::Response& res) {
        const auto token = readTokenCookie(req);
        const auto sess = svc.sessions().validate(token);
        if (!sess) {
            res.status = 401;
            res.set_content("{\"error\":\"unauth\"}", "application/json");
            return;
        }
        svc.sessions().touch(token);

        const auto ticketId = req.matches[1].str();
        auto tkt = svc.tickets().get(ticketId);
        if (!tkt) {
            res.status = 404;
            res.set_content("{\"error\":\"no_ticket\"}", "application/json");
            return;
        }
        if (tkt->sessionToken != token) {
            res.status = 403;
            res.set_content("{\"error\":\"wrong_session\"}", "application/json");
            return;
        }

        if (tkt->kind == TicketKind::StreamingZip) {
            streamZip(*tkt, svc, res);
        } else {
            streamFile(*tkt, svc, res);
        }
    });

    // V1.1.9-batch : bundle ZIP de TOUS les tickets de la session courante
    // (déterminée par le cookie). Permet « Télécharger tout » côté visiteur.
    server.Get("/api/download/bundle",
               [&svc](const httplib::Request& req, httplib::Response& res) {
        const auto token = readTokenCookie(req);
        const auto sess = svc.sessions().validate(token);
        if (!sess) {
            res.status = 401;
            res.set_content("{\"error\":\"unauth\"}", "application/json");
            return;
        }
        svc.sessions().touch(token);

        const auto tickets = svc.tickets().listBySession(token);
        if (tickets.empty()) {
            res.status = 404;
            res.set_content("{\"error\":\"no_tickets\"}", "application/json");
            return;
        }

        // Fusionner tous les tickets en une seule liste ZipEntry. Les
        // tickets kind=StreamingZip développent leur arbo (relInZip
        // préservé) ; kind=File ajoute une entrée plate.
        std::vector<ZipEntry> entries;
        for (const auto& tkt : tickets) {
            if (tkt.kind == TicketKind::File) {
                ZipEntry e;
                e.abs      = tkt.path;
                e.relInZip = tkt.displayName;
                e.size     = tkt.size;
                entries.push_back(std::move(e));
            } else {
                for (const auto& ze : tkt.zipEntries) {
                    entries.push_back(ze);
                }
            }
        }
        if (entries.empty()) {
            res.status = 404;
            res.set_content("{\"error\":\"no_entries\"}", "application/json");
            return;
        }

        const auto zipSize = StreamingZipSource::computeZipSize(entries);

        // Nom horodaté simple — pas de millis, c'est suffisant.
        const auto t = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        std::tm tm{};
#if defined(_WIN32)
        gmtime_s(&tm, &t);
#else
        gmtime_r(&t, &tm);
#endif
        char ts[32] = {0};
        std::strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", &tm);
        const std::string filename = "LocalTransfer-" + std::string(ts) + ".zip";

        res.set_header("Content-Disposition",
            "attachment; filename*=UTF-8''" + rfc5987Encode(filename));
        res.set_header("Content-Length", std::to_string(zipSize));
        res.set_header("Cache-Control", "no-store");

        auto source = std::make_shared<StreamingZipSource>(std::move(entries));
        res.set_content_provider(zipSize, "application/zip",
            [source](std::size_t /*offset*/, std::size_t length,
                     httplib::DataSink& sink) {
                const bool more = source->provide(sink, length);
                if (!more && !source->errored()) sink.done();
                return more && !source->errored();
            });

        core::log_info("[bundle] " + std::to_string(tickets.size())
                       + " ticket(s) → " + std::to_string(zipSize) + " o");
    });
}

} // namespace ltr::web::routes
