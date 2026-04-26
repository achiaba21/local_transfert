#pragma once

namespace ltr::web { class WebService; }

namespace ltr::web::routes {

// POST /api/upload — réception de fichiers multipart du visiteur web
// vers le host (écrit dans downloadDir). Emet IncomingOfferEvent,
// TransferStartedEvent, TransferProgressEvent, TransferDoneEvent.
void registerUpload(WebService& svc);

} // namespace ltr::web::routes
