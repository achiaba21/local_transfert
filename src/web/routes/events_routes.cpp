#include "ltr/web/routes/events_routes.hpp"

#include <atomic>
#include <chrono>
#include <memory>

#include <httplib.h>

#include "ltr/core/logger.hpp"
#include "ltr/web/routes/route_helpers.hpp"
#include "ltr/web/web_service.hpp"
#include "ltr/web/routes/multi_server.hpp"

namespace ltr::web::routes {

void registerEvents(WebService& svc) {
    auto server = routes::routerOf(svc);

    server.Get("/api/events", [&svc](const httplib::Request& req,
                                      httplib::Response& res) {
        const auto token = readTokenCookie(req);
        core::log_info(std::string("[SSE] GET /api/events token=")
                       + (token.empty() ? "<empty>" : token.substr(0, 8) + "..."));

        const auto sess = svc.sessions().validate(token);
        if (!sess) {
            core::log_warn("[SSE] 401 unauth (pas de session valide)");
            res.status = 401;
            res.set_content("{\"error\":\"unauth\"}", "application/json");
            return;
        }

        auto ch = svc.broadcaster().attach(token);
        core::log_info("[SSE] attached broadcaster for " + token.substr(0, 8));

        // V1.2 — Sprint Web P2P : push immédiat de l'annuaire au nouveau
        // venu. L'`emitWebPeersToAll` qui suit l'auth est trop tôt — le
        // canal SSE de cette session n'existe pas encore. Sans ce push,
        // le nouveau client ne voit aucun pair tant qu'un 3e n'auth pas.
        svc.emitWebPeersTo(token);

        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("X-Accel-Buffering", "no");

        auto iterCount = std::make_shared<std::atomic<int>>(0);

        res.set_chunked_content_provider(
            "text/event-stream",
            [ch, token, iterCount, &svc](std::size_t /*offset*/,
                                          httplib::DataSink& sink) {
                core::log_info("[SSE] provider started for "
                               + token.substr(0, 8));

                // V1.1.1 : iOS Safari nécessite ~2 KB initiaux pour flusher
                // son buffer interne. On pad le handshake avec un commentaire.
                std::string hello;
                hello.reserve(2600);
                hello += ':';
                hello.append(2048, ' ');
                hello += "\n";
                hello += "retry: 3000\n";
                hello += ": connected\n\n";
                if (!sink.write(hello.c_str(), hello.size())) {
                    core::log_warn("[SSE] hello write failed for "
                                   + token.substr(0, 8));
                    sink.done();
                    return false;
                }
                core::log_info("[SSE] handshake sent (" +
                               std::to_string(hello.size()) + " bytes)");

                while (!ch->isClosed() && sink.is_writable()) {
                    svc.sessions().touch(token);
                    const int n = ++(*iterCount);
                    if (n == 1 || n % 15 == 0) {
                        core::log_debug("[SSE] iter=" + std::to_string(n)
                                        + " touch(" + token.substr(0, 8) + ")");
                    }

                    std::string msg;
                    const bool got = ch->waitAndPop(
                        msg, std::chrono::milliseconds(1000));

                    if (got) {
                        core::log_info("[SSE] write msg "
                                       + std::to_string(msg.size()) + "B");
                        if (!sink.write(msg.c_str(), msg.size())) {
                            core::log_warn("[SSE] sink.write failed — break");
                            break;
                        }
                    } else {
                        const std::string ping = ": keepalive\n\n";
                        if (!sink.write(ping.c_str(), ping.size())) {
                            core::log_warn("[SSE] keepalive write failed — break");
                            break;
                        }
                    }
                }
                core::log_info("[SSE] provider exit for " + token.substr(0, 8)
                               + " (iter=" + std::to_string(iterCount->load())
                               + ", closed=" + (ch->isClosed() ? "1" : "0")
                               + ")");
                sink.done();
                return false;
            },
            [&svc, token](bool success) {
                core::log_info("[SSE] completion for " + token.substr(0, 8)
                               + " success=" + (success ? "1" : "0"));
                svc.broadcaster().detach(token);
            });
    });
}

} // namespace ltr::web::routes
