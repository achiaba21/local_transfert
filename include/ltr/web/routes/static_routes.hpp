#pragma once

namespace ltr::web { class WebService; }

namespace ltr::web::routes {

// GET /, /app.js, /style.css, /icons/*.svg — assets web embarqués.
void registerStatic(WebService& svc);

} // namespace ltr::web::routes
