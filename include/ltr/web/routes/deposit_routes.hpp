#pragma once

namespace ltr::web { class WebService; }

namespace ltr::web::routes {

// Phase 2 — Portail client externe (routes PUBLIQUES, sans cookie session).
// Authentification = token de lien + sessionId opaque retourné par begin().
//
//   GET  /deposit/:token                  → HTML page déposant (ou expired)
//   GET  /api/deposit/info?token=...      → métadonnées lien (label, limits)
//   POST /api/deposit/begin?token=...     → ouvre session, retourne sessionId
//   POST /api/deposit/upload?session=...  → upload multipart 1 fichier
//   POST /api/deposit/finalize?session=...→ génère reçu signé
//   GET  /api/deposit/receipt/:id         → reçu JSON signé
void registerDeposit(WebService& svc);

} // namespace ltr::web::routes
