#pragma once

namespace ltr::web {
class WebService;
}

namespace ltr::web::routes {

// GET /api/share-info, GET /api/share-qr.png
void registerShare(WebService& svc);

} // namespace ltr::web::routes
