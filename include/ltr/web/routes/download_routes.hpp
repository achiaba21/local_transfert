#pragma once

namespace ltr::web { class WebService; }

namespace ltr::web::routes {

// GET /api/download/:ticketId — stream d'un fichier vers un visiteur web
// (host → browser). Consomme le ticket. Emet TransferProgress + Done.
void registerDownload(WebService& svc);

} // namespace ltr::web::routes
