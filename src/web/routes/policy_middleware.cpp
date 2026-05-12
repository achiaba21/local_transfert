#include "ltr/web/routes/policy_middleware.hpp"

#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "ltr/core/logger.hpp"
#include "ltr/infra/policy_enforcement.hpp"
#include "ltr/web/routes/multi_server.hpp"
#include "ltr/web/routes/route_helpers.hpp"
#include "ltr/web/web_service.hpp"

namespace ltr::web::routes {

namespace {

std::string clientIp(const httplib::Request& req) {
    // On prend l'IP socket directe — pas de confiance dans X-Forwarded-For
    // pour éviter le spoofing. cpp-httplib expose remote_addr.
    return req.remote_addr;
}

} // namespace

bool rejectIfP2PDisabled(WebService& svc,
                          const httplib::Request&,
                          httplib::Response& res) {
    auto* p = svc.policyEnforcement();
    if (!p) return false;
    if (p->isP2PAllowed()) return false;
    res.status = 403;
    res.set_content("{\"error\":\"p2p_disabled\",\"reason\":\"policy\"}",
                    "application/json");
    return true;
}

bool rejectIfIpBlocked(WebService& svc,
                       const httplib::Request& req,
                       httplib::Response& res) {
    auto* p = svc.policyEnforcement();
    if (!p) return false;
    // /health jamais filtré.
    if (req.path == "/health") return false;
    const auto ip = clientIp(req);
    if (p->isIpAllowed(ip)) return false;
    core::log_warn("[policy] IP refusée : " + ip + " sur " + req.path);
    res.status = 403;
    res.set_content("{\"error\":\"ip_blocked\",\"reason\":\"policy\"}",
                    "application/json");
    return true;
}

bool redirectIfHttpsRequired(WebService& svc,
                              const httplib::Request& req,
                              httplib::Response& res,
                              bool isHttpsServer) {
    auto* p = svc.policyEnforcement();
    if (!p) return false;
    if (!p->httpsForced()) return false;
    if (isHttpsServer) return false;
    const auto httpsPort = svc.portHttps();
    if (httpsPort == 0) {
        // HTTPS indisponible mais requireHttps=true → on refuse plutôt
        // que de laisser passer en clair.
        res.status = 503;
        res.set_content("{\"error\":\"https_unavailable\",\"reason\":\"policy\"}",
                        "application/json");
        return true;
    }
    const auto host = req.get_header_value("Host");
    const auto target = p->buildHttpsRedirect(host, httpsPort, req.target);
    res.status = 301;
    res.set_header("Location", target);
    res.set_header("Cache-Control", "no-store");
    res.set_content("", "text/plain");
    return true;
}

void registerPolicyFlags(WebService& svc) {
    auto server = routes::routerOf(svc);
    server.Get("/api/policy/flags",
        [&svc](const httplib::Request& req, httplib::Response& res) {
            const auto token = readTokenCookie(req);
            const auto sess  = svc.sessions().validate(token);
            if (!sess) {
                res.status = 401;
                res.set_content("{\"error\":\"unauth\"}", "application/json");
                return;
            }
            svc.sessions().touch(token);
            auto* p = svc.policyEnforcement();
            nlohmann::json j;
            j["allowP2P"]      = p ? p->isP2PAllowed() : true;
            j["requireHttps"]  = p ? p->httpsForced()  : false;
            j["historyDays"]   = p ? p->historyRetentionDays() : 180;
            j["receivedFilesDays"] = p ? p->receivedFilesRetentionDays() : 0;
            res.set_header("Cache-Control", "no-store");
            res.set_content(j.dump(), "application/json; charset=utf-8");
        });
}

} // namespace ltr::web::routes
