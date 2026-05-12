#include "ltr/web/routes/history_routes.hpp"

#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <httplib.h>

#include "ltr/infra/audit_export_service.hpp"
#include "ltr/infra/transfer_history.hpp"
#include "ltr/web/routes/multi_server.hpp"
#include "ltr/web/routes/route_helpers.hpp"
#include "ltr/web/web_service.hpp"

namespace ltr::web::routes {

void registerHistory(WebService& svc) {
    auto server = routes::routerOf(svc);

    server.Get("/api/host-history", [&svc](const httplib::Request& req,
                                            httplib::Response& res) {
        const auto token = readTokenCookie(req);
        const auto sess = svc.sessions().validate(token);
        if (!sess) {
            res.status = 401;
            res.set_content("{\"error\":\"unauth\"}", "application/json");
            return;
        }
        svc.sessions().touch(token);
        nlohmann::json arr = nlohmann::json::array();
        if (auto* history = svc.transferHistory()) {
            for (const auto& e : history->snapshot()) {
                arr.push_back({
                    {"sessionId", e.sessionId},
                    {"peerDeviceId", e.peerDeviceId},
                    {"peerName", e.peerName},
                    {"kind", infra::TransferHistory::kindToStr(e.kind)},
                    {"fileCount", e.fileCount},
                    {"totalBytes", e.totalBytes},
                    {"status", infra::TransferHistory::statusToStr(e.status)},
                    {"startedAt", e.startedAt},
                    {"finishedAt", e.finishedAt},
                    {"error", e.error},
                });
            }
        }
        nlohmann::json out;
        out["transfers"] = arr;
        res.set_header("Cache-Control", "no-store");
        res.set_content(out.dump(), "application/json");
    });

    server.Get("/api/host-history/export", [&svc](const httplib::Request& req,
                                                   httplib::Response& res) {
        const auto token = readTokenCookie(req);
        const auto sess = svc.sessions().validate(token);
        if (!sess) {
            res.status = 401;
            res.set_content("{\"error\":\"unauth\"}", "application/json");
            return;
        }
        svc.sessions().touch(token);

        std::vector<infra::TransferHistory::Entry> entries;
        if (auto* history = svc.transferHistory()) {
            entries = history->snapshot();
        }

        const auto format = req.has_param("format")
            ? req.get_param_value("format") : std::string{"json"};
        infra::AuditExportService exporter;
        res.set_header("Cache-Control", "no-store");
        if (format == "csv") {
            res.set_header("Content-Disposition",
                "attachment; filename=\"localtransfer-audit.csv\"");
            res.set_content(exporter.exportCsv(entries), "text/csv; charset=utf-8");
        } else {
            res.set_header("Content-Disposition",
                "attachment; filename=\"localtransfer-audit.json\"");
            res.set_content(exporter.exportJson(entries),
                "application/json; charset=utf-8");
        }
    });
}

} // namespace ltr::web::routes
