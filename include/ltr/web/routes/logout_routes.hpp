#pragma once

namespace ltr::web { class WebService; }

namespace ltr::web::routes {

// POST /api/logout — invalide la session courante (token du cookie),
// efface le cookie côté navigateur, répond 204 No Content.
void registerLogout(WebService& svc);

} // namespace ltr::web::routes
