#pragma once

namespace ltr::web { class WebService; }

namespace ltr::web::routes {

// Phase 2 — Routes ADMIN (host) pour la gestion des liens de dépôt.
// Toutes ces routes exigent une session PIN active (cookie ltr_token).
//
//   GET    /api/host/deposit-links           → liste des liens
//   POST   /api/host/deposit-links           → crée un lien
//   DELETE /api/host/deposit-links/:id       → révoque un lien
//   GET    /api/host/deposit-history         → historique dépôts
//   GET    /api/host/deposit-history?linkId= → filtré par lien
void registerDepositAdmin(WebService& svc);

} // namespace ltr::web::routes
