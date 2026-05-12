#include "ltr/web/routes/deposit_admin_routes.hpp"

#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "ltr/infra/deposit_history.hpp"
#include "ltr/infra/deposit_link_service.hpp"
#include "ltr/web/routes/multi_server.hpp"
#include "ltr/web/routes/route_helpers.hpp"
#include "ltr/web/web_service.hpp"

namespace ltr::web::routes {

namespace {

nlohmann::json linkToJson(const infra::DepositLink& l,
                          infra::DepositLinkService& svc) {
    return {
        {"id",                  l.id},
        {"token",               l.token},
        {"label",               l.label},
        {"consentText",         l.consentText},
        {"maxBytesPerDeposit",  l.maxBytesPerDeposit},
        {"maxFilesPerDeposit",  l.maxFilesPerDeposit},
        {"createdAt",           l.createdAt},
        {"expiresAt",           l.expiresAt},
        {"revoked",             l.revoked},
        {"active",              svc.isActive(l)},
    };
}

nlohmann::json depositEntryToJson(const infra::DepositHistory::Entry& e) {
    return {
        {"receiptId",     e.receiptId},
        {"sessionId",     e.sessionId},
        {"linkId",        e.linkId},
        {"linkLabel",     e.linkLabel},
        {"depositorName", e.depositorName},
        {"fileCount",     e.fileCount},
        {"totalBytes",    e.totalBytes},
        {"status",        infra::DepositHistory::statusToStr(e.status)},
        {"startedAt",     e.startedAt},
        {"finishedAt",    e.finishedAt},
    };
}

bool requireAuth(WebService& svc, const httplib::Request& req,
                 httplib::Response& res) {
    const auto token = readTokenCookie(req);
    const auto sess  = svc.sessions().validate(token);
    if (!sess) {
        res.status = 401;
        res.set_content("{\"error\":\"unauth\"}", "application/json");
        return false;
    }
    svc.sessions().touch(token);
    return true;
}

} // namespace

void registerDepositAdmin(WebService& svc) {
    auto server = routes::routerOf(svc);

    server.Get("/api/host/deposit-links",
        [&svc](const httplib::Request& req, httplib::Response& res) {
            if (!requireAuth(svc, req, res)) return;
            auto* links = svc.depositLinks();
            if (!links) {
                res.set_content("{\"links\":[]}", "application/json");
                return;
            }
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& l : links->list()) arr.push_back(linkToJson(l, *links));
            nlohmann::json j;
            j["links"] = arr;
            res.set_header("Cache-Control", "no-store");
            res.set_content(j.dump(), "application/json; charset=utf-8");
        });

    server.Post("/api/host/deposit-links",
        [&svc](const httplib::Request& req, httplib::Response& res) {
            if (!requireAuth(svc, req, res)) return;
            auto* links = svc.depositLinks();
            if (!links) {
                res.status = 503;
                res.set_content("{\"error\":\"deposits_disabled\"}",
                                "application/json");
                return;
            }
            infra::DepositLinkSpec spec;
            try {
                const auto j = nlohmann::json::parse(req.body);
                spec.label              = j.value("label", std::string{});
                spec.consentText        = j.value("consentText", std::string{});
                spec.maxBytesPerDeposit = j.value("maxBytesPerDeposit",
                                                  std::uint64_t{0});
                spec.maxFilesPerDeposit = j.value("maxFilesPerDeposit", 0);
                spec.expiresAt          = j.value("expiresAt", std::int64_t{0});
            } catch (...) {
                res.status = 400;
                res.set_content("{\"error\":\"bad_json\"}", "application/json");
                return;
            }
            if (spec.label.empty()) {
                res.status = 400;
                res.set_content("{\"error\":\"label_required\"}",
                                "application/json");
                return;
            }
            const auto result = links->create(spec);
            if (!result.ok) {
                nlohmann::json j;
                j["error"] = result.reason;
                res.status = (result.reason == "upsell_required") ? 402 : 400;
                res.set_content(j.dump(), "application/json");
                return;
            }
            res.set_content(linkToJson(result.value, *links).dump(),
                            "application/json; charset=utf-8");
        });

    server.Delete(R"(/api/host/deposit-links/([a-f0-9]+))",
        [&svc](const httplib::Request& req, httplib::Response& res) {
            if (!requireAuth(svc, req, res)) return;
            auto* links = svc.depositLinks();
            if (!links) {
                res.status = 503;
                res.set_content("{\"error\":\"deposits_disabled\"}",
                                "application/json");
                return;
            }
            const auto id = req.matches[1].str();
            if (!links->revoke(id)) {
                res.status = 404;
                res.set_content("{\"error\":\"not_found\"}", "application/json");
                return;
            }
            res.set_content("{\"ok\":true}", "application/json");
        });

    server.Get("/api/host/deposit-history",
        [&svc](const httplib::Request& req, httplib::Response& res) {
            if (!requireAuth(svc, req, res)) return;
            auto* history = svc.depositHistory();
            if (!history) {
                res.set_content("{\"deposits\":[]}", "application/json");
                return;
            }
            const auto linkId = req.has_param("linkId")
                ? req.get_param_value("linkId") : std::string{};
            const auto entries = linkId.empty()
                ? history->snapshot()
                : history->filterByLinkId(linkId);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& e : entries) arr.push_back(depositEntryToJson(e));
            nlohmann::json j;
            j["deposits"] = arr;
            res.set_header("Cache-Control", "no-store");
            res.set_content(j.dump(), "application/json; charset=utf-8");
        });
}

} // namespace ltr::web::routes
