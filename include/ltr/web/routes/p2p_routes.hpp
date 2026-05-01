#pragma once

namespace ltr::web {
class WebService;
}

namespace ltr::web::routes {

// V1.2 — Sprint Web P2P
// POST /api/p2p/signal — relay des messages WebRTC (offer/answer/ice/
// refuse) entre 2 navigateurs auth. Le host ne stocke rien : routage
// synchrone vers le canal SSE du destinataire.
void registerP2P(WebService& svc);

} // namespace ltr::web::routes
