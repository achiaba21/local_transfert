#include "ltr/web/routes/p2p_routes.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "ltr/core/logger.hpp"
#include "ltr/web/routes/route_helpers.hpp"
#include "ltr/web/web_service.hpp"

namespace ltr::web::routes {

namespace {

// V1.2 — Sprint Web P2P
// Whitelist des `type` autorisés. Empêche un client malicieux d'envoyer
// des évènements arbitraires via le canal SSE du destinataire.
bool isValidSignalType(const std::string& t) {
    return t == "offer"   || t == "answer" || t == "ice"
        || t == "refuse"  || t == "cancel" || t == "bye";
}

} // namespace

void registerP2P(WebService& svc) {
    auto& server = svc.httpServer().raw();

    // POST /api/p2p/signal { to: "<deviceId>", type, payload }
    // - 401 : pas de cookie / session expirée
    // - 400 : JSON invalide ou type inconnu
    // - 404 : destinataire introuvable / hors-ligne
    // - 204 : routé avec succès
    server.Post("/api/p2p/signal", [&svc](const httplib::Request& req,
                                           httplib::Response& res) {
        // 1. Auth expéditeur via cookie
        const auto fromToken = readTokenCookie(req);
        const auto fromSess = svc.sessions().validate(fromToken);
        if (!fromSess) {
            res.status = 401;
            res.set_content("{\"error\":\"unauth\"}", "application/json");
            return;
        }
        svc.sessions().touch(fromToken);

        // 2. Parse body
        std::string toDeviceId, type;
        nlohmann::json payload;
        try {
            const auto body = nlohmann::json::parse(req.body);
            toDeviceId = body.value("to",   "");
            type       = body.value("type", "");
            payload    = body.value("payload", nlohmann::json::object());
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"bad_json\"}", "application/json");
            return;
        }

        if (toDeviceId.empty() || !isValidSignalType(type)) {
            res.status = 400;
            res.set_content("{\"error\":\"bad_signal\"}", "application/json");
            return;
        }

        // 3. Empêche A→A
        if (toDeviceId == fromSess->deviceId) {
            res.status = 400;
            res.set_content("{\"error\":\"self_target\"}", "application/json");
            return;
        }

        // 4. Résolution destinataire
        const auto toToken = svc.sessions().findTokenByDeviceId(toDeviceId);
        if (!toToken) {
            res.status = 404;
            res.set_content("{\"error\":\"target_offline\"}",
                             "application/json");
            return;
        }

        // 5. Construction du message SSE pour le destinataire
        nlohmann::json out;
        out["from"]    = fromSess->deviceId;
        out["type"]    = type;
        out["payload"] = payload;
        const auto sse = "event: p2p-signal\ndata: " + out.dump() + "\n\n";

        svc.broadcaster().send(*toToken, sse);

        core::log_info("[p2p] signal type=" + type
                       + " from=" + fromSess->deviceId.substr(0, 8)
                       + " to=" + toDeviceId.substr(0, 8));

        res.status = 204;
    });
}

} // namespace ltr::web::routes
