#pragma once

namespace ltr::web { class WebService; }

namespace ltr::web::routes {

// GET /download/self — sert le binaire courant (exe, ELF, ou .app.zip).
void registerSelf(WebService& svc);

} // namespace ltr::web::routes
