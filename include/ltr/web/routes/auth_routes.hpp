#pragma once

namespace ltr::web {
class WebService;
}

namespace ltr::web::routes {

// POST /api/auth, GET /api/me, GET /api/host-info
void registerAuth(WebService& svc);

} // namespace ltr::web::routes
