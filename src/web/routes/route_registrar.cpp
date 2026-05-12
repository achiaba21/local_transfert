#include "ltr/web/routes/route_registrar.hpp"

#include <httplib.h>

#include "ltr/web/http_server.hpp"
#include "ltr/web/routes/auth_routes.hpp"
#include "ltr/web/routes/deposit_admin_routes.hpp"
#include "ltr/web/routes/deposit_routes.hpp"
#include "ltr/web/routes/download_routes.hpp"
#include "ltr/web/routes/events_routes.hpp"
#include "ltr/web/routes/history_routes.hpp"
#include "ltr/web/routes/logout_routes.hpp"
#include "ltr/web/routes/p2p_routes.hpp"
#include "ltr/web/routes/policy_middleware.hpp"
#include "ltr/web/routes/self_routes.hpp"
#include "ltr/web/routes/share_routes.hpp"
#include "ltr/web/routes/static_routes.hpp"
#include "ltr/web/routes/upload_routes.hpp"
#include "ltr/web/web_service.hpp"

namespace ltr::web::routes {

namespace {

// Phase 3 — Pré-routing combiné : redirect HTTPS (HTTP seul) + whitelist IP.
// Retourne Handled si la requête doit s'arrêter, Unhandled sinon.
httplib::Server::HandlerResponse
preRouting(WebService& svc, const httplib::Request& req,
           httplib::Response& res, bool isHttpsServer) {
    if (redirectIfHttpsRequired(svc, req, res, isHttpsServer)) {
        return httplib::Server::HandlerResponse::Handled;
    }
    if (rejectIfIpBlocked(svc, req, res)) {
        return httplib::Server::HandlerResponse::Handled;
    }
    return httplib::Server::HandlerResponse::Unhandled;
}

} // namespace

void registerAll(WebService& svc) {
    // Phase 3 — installer pré-routing AVANT toute autre route.
    svc.httpServer().raw().set_pre_routing_handler(
        [&svc](const httplib::Request& req, httplib::Response& res) {
            return preRouting(svc, req, res, false);
        });
    if (auto* https = svc.httpsServer()) {
        https->raw().set_pre_routing_handler(
            [&svc](const httplib::Request& req, httplib::Response& res) {
                return preRouting(svc, req, res, true);
            });
    }

    // Ordre recommandé : static en dernier pour que /api/* ait priorité.
    registerAuth(svc);
    registerLogout(svc);
    registerEvents(svc);
    registerUpload(svc);
    registerDownload(svc);
    registerHistory(svc);
    registerP2P(svc);            // V1.2 — Sprint Web P2P
    registerDeposit(svc);        // Phase 2 — Portail Client Externe (public)
    registerDepositAdmin(svc);   // Phase 2 — Admin Portail (host)
    registerPolicyFlags(svc);    // Phase 3 — Contrôle IT
    registerSelf(svc);
    registerShare(svc);
    registerStatic(svc);
}

} // namespace ltr::web::routes
