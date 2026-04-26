#pragma once

namespace ltr::web { class WebService; }

namespace ltr::web::routes {

// GET /api/events — canal SSE long-lived par session. Attache le
// SseChannel correspondant et draine les messages jusqu'à déconnexion.
void registerEvents(WebService& svc);

} // namespace ltr::web::routes
