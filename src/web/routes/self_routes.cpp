#include "ltr/web/routes/self_routes.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <httplib.h>

#include "ltr/core/logger.hpp"
#include "ltr/web/self_binary.hpp"
#include "ltr/web/web_service.hpp"

namespace ltr::web::routes {

void registerSelf(WebService& svc) {
    auto& server = svc.httpServer().raw();

    server.Get("/download/self", [](const httplib::Request&,
                                     httplib::Response& res) {
        std::vector<std::uint8_t> bytes;
        if (!SelfBinary::produceBytes(bytes)) {
            res.status = 500;
            res.set_content(
                "{\"error\":\"self_binary_unavailable\"}",
                "application/json");
            return;
        }
        res.set_header("Content-Disposition",
            "attachment; filename=\""
            + SelfBinary::suggestedDownloadName() + "\"");
        res.set_header("Cache-Control", "no-store");
        res.set_content(
            reinterpret_cast<const char*>(bytes.data()), bytes.size(),
            SelfBinary::mimeType().c_str());
    });
    (void)svc;
}

} // namespace ltr::web::routes
