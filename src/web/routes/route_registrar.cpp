#include "ltr/web/routes/route_registrar.hpp"

#include "ltr/web/routes/auth_routes.hpp"
#include "ltr/web/routes/download_routes.hpp"
#include "ltr/web/routes/events_routes.hpp"
#include "ltr/web/routes/logout_routes.hpp"
#include "ltr/web/routes/p2p_routes.hpp"
#include "ltr/web/routes/self_routes.hpp"
#include "ltr/web/routes/static_routes.hpp"
#include "ltr/web/routes/upload_routes.hpp"

namespace ltr::web::routes {

void registerAll(WebService& svc) {
    // Ordre recommandé : static en dernier pour que /api/* ait priorité.
    registerAuth(svc);
    registerLogout(svc);
    registerEvents(svc);
    registerUpload(svc);
    registerDownload(svc);
    registerP2P(svc);   // V1.2 — Sprint Web P2P
    registerSelf(svc);
    registerStatic(svc);
}

} // namespace ltr::web::routes
