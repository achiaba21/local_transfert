#include "ltr/web/routes/logout_routes.hpp"

#include <httplib.h>

#include "ltr/core/logger.hpp"
#include "ltr/web/routes/route_helpers.hpp"
#include "ltr/web/web_service.hpp"
#include "ltr/web/routes/multi_server.hpp"

namespace ltr::web::routes {

void registerLogout(WebService& svc) {
    auto server = routes::routerOf(svc);

    // POST /api/logout — idempotent. Invalide la session courante si cookie
    // valide, efface le cookie navigateur, répond 204.
    server.Post("/api/logout", [&svc](const httplib::Request& req,
                                       httplib::Response& res) {
        const auto token = readTokenCookie(req);
        if (!token.empty()) {
            const auto sess = svc.sessions().validate(token);
            if (sess) {
                // Emet PeerLost immédiatement pour une UX desktop réactive.
                svc.broadcaster().detach(token);
                svc.bus().post(core::PeerLostEvent{sess->deviceId});
            }
            svc.sessions().removeByToken(token);
            // V1.2 — Sprint Web P2P : annoncer le départ aux autres.
            svc.emitWebPeersToAll();
        }
        // Suppression du cookie côté navigateur.
        res.set_header("Set-Cookie",
            "ltr_token=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
        res.status = 204;
    });
}

} // namespace ltr::web::routes
