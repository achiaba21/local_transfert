#include "ltr/web/routes/auth_routes.hpp"

#include <chrono>
#include <random>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "ltr/core/logger.hpp"
#include "ltr/core/types.hpp"
#include "ltr/web/routes/route_helpers.hpp"
#include "ltr/web/web_service.hpp"
#include "ltr/web/routes/multi_server.hpp"

namespace ltr::web::routes {

namespace {

// UUIDv4 de secours côté serveur si le client n'a pas fourni de deviceId.
// Le client devrait toujours en fournir un (localStorage), mais on tolère.
std::string makeServerDeviceId() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    auto hexByte = [](std::uint8_t v, std::string& out) {
        constexpr char kHex[] = "0123456789abcdef";
        out.push_back(kHex[(v >> 4) & 0xF]);
        out.push_back(kHex[v & 0xF]);
    };
    std::uint8_t bytes[16];
    for (int i = 0; i < 16; ++i) bytes[i] = static_cast<std::uint8_t>(rng() & 0xFF);
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;
    std::string s; s.reserve(36);
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) s.push_back('-');
        hexByte(bytes[i], s);
    }
    return s;
}

} // namespace

void registerAuth(WebService& svc) {
    auto server = routes::routerOf(svc);

    // GET /api/host-info — identité du host (public, pas d'auth).
    server.Get("/api/host-info", [&svc](const httplib::Request&,
                                         httplib::Response& res) {
        core::log_info("[host-info] GET /api/host-info");
        nlohmann::json j;
        j["name"]            = svc.self().name;
        j["platform"]        = svc.self().platform;
        j["selfDownloadUrl"] = "/download/self";
        res.set_header("Cache-Control", "no-store");
        res.set_content(j.dump(), "application/json");
    });

    // V1.6.4 — Sprint Sécurité : empreinte SHA-256 du cert HTTPS.
    server.Get("/api/cert-info", [&svc](const httplib::Request&,
                                         httplib::Response& res) {
        nlohmann::json j;
        j["fingerprint"] = svc.fingerprint();
        res.set_header("Cache-Control", "no-store");
        res.set_content(j.dump(), "application/json");
    });

    // POST /api/clientlog { level, msg } — logs envoyés par le JS client
    // (debug iOS Safari). Public (pas de cookie requis).
    server.Post("/api/clientlog", [](const httplib::Request& req,
                                       httplib::Response& res) {
        try {
            const auto j = nlohmann::json::parse(req.body);
            const auto level = j.value("level", "info");
            const auto msg = j.value("msg", "");
            const std::string prefix = "[client:" + level + "] ";
            if (level == "error") core::log_error(prefix + msg);
            else if (level == "warn") core::log_warn(prefix + msg);
            else                      core::log_info(prefix + msg);
        } catch (...) {
            // Ignore bad JSON, ne pas bloquer le client.
        }
        res.status = 204;
    });

    // V1.1.2 : POST /api/ping — heartbeat. Le JS navigateur pinge toutes
    // les 10s pour maintenir la session vivante indépendamment de la SSE
    // (qui peut être tuée par iOS Safari en arrière-plan).
    server.Post("/api/ping", [&svc](const httplib::Request& req,
                                     httplib::Response& res) {
        const auto token = readTokenCookie(req);
        const auto sess = svc.sessions().validate(token);
        if (!sess) {
            res.status = 401;
            res.set_content("{\"error\":\"unauth\"}", "application/json");
            return;
        }
        svc.sessions().touch(token);
        res.status = 204;
    });

    // POST /api/auth { pin, device_id, remember? } — vérifie le PIN,
    // crée la session. V1.6.5 : si remember=true, émet aussi un cookie
    // persistent `ltr_remember` valide 30j permettant de recréer la
    // session sans re-saisir le PIN au prochain accès.
    server.Post("/api/auth", [&svc](const httplib::Request& req,
                                     httplib::Response& res) {
        std::string providedPin;
        std::string deviceId;
        std::string customName;
        bool remember = false;
        try {
            const auto body = nlohmann::json::parse(req.body);
            providedPin = body.value("pin", "");
            deviceId    = body.value("device_id", "");
            customName  = body.value("custom_name", "");
            remember    = body.value("remember", false);
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"bad_json\"}", "application/json");
            return;
        }

        // Si le client ne fournit pas de device_id (cas rare : localStorage
        // indisponible), on en génère un serveur-side et on le renvoie.
        bool serverGenerated = false;
        if (deviceId.empty()) {
            deviceId = makeServerDeviceId();
            serverGenerated = true;
        }

        const auto ua = req.get_header_value("User-Agent");
        auto tokenOpt = svc.sessions().authenticate(
            providedPin, svc.accessPinRef(), deviceId, ua, customName);

        if (!tokenOpt) {
            res.status = 401;
            nlohmann::json j;
            j["error"] = "invalid_pin";
            if (serverGenerated) j["device_id"] = deviceId;
            res.set_content(j.dump(), "application/json");
            return;
        }
        const auto token = *tokenOpt;

        core::log_info("auth OK: device_id=" + deviceId.substr(0, 8)
                       + "... token=" + token.substr(0, 8) + "...");

        // Cookie de SESSION (pas d'attribut Max-Age/Expires → meurt à la
        // fermeture de l'onglet). HttpOnly + SameSite=Lax ; pas de Secure
        // car HTTP plain en LAN.
        res.set_header("Set-Cookie",
            "ltr_token=" + token + "; Path=/; HttpOnly; SameSite=Lax");

        // V1.6.5 — Sprint Stabilité (Wave 3 item H) : cookie persistent
        // `ltr_remember` HMAC. Valide 30 jours. Permet de recréer la
        // session sans re-PIN après fermeture d'onglet ou redémarrage
        // browser, tant que le PIN host n'a pas changé.
        if (remember) {
            const auto nowSec = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const auto exp = nowSec + std::chrono::duration_cast<std::chrono::seconds>(
                core::kWebSessionRememberTtl).count();
            const auto persistent = svc.sessions().makePersistentToken(
                deviceId, svc.accessPinRef(), exp);
            const auto maxAge = std::to_string(
                std::chrono::duration_cast<std::chrono::seconds>(
                    core::kWebSessionRememberTtl).count());
            res.set_header("Set-Cookie",
                "ltr_remember=" + persistent
                + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=" + maxAge);
            core::log_info("[auth] ltr_remember émis device=" + deviceId.substr(0, 8)
                           + "... exp=+" + std::to_string(
                               std::chrono::duration_cast<std::chrono::hours>(
                                   core::kWebSessionRememberTtl).count())
                           + "h");
        }

        if (auto s = svc.sessions().validate(token)) {
            svc.bus().post(core::PeerSeenEvent{s->device});
        }

        // V1.2 — Sprint Web P2P : annonce le nouveau pair à toutes les
        // autres sessions (et envoie au nouveau l'annuaire des autres).
        svc.emitWebPeersToAll();

        // V1.1.1 : réponse 200 JSON au lieu de 302.
        // Raison : iOS Safari a un bug où le Set-Cookie d'une réponse 302
        // suivi par fetch(redirect:follow) peut ne pas être stocké. En
        // retournant 200 JSON, le navigateur reçoit et stocke le cookie
        // via l'XHR. Le JS client fait ensuite window.location.href='/'
        // qui est une navigation top-level propre, cookie bien attaché.
        nlohmann::json j;
        j["ok"]   = true;
        j["next"] = "/";
        if (serverGenerated) j["device_id"] = deviceId;
        res.status = 200;
        res.set_content(j.dump(), "application/json");
    });

    // V1.6.5 — Sprint Stabilité (Wave 3 item H).
    // POST /api/auth/refresh — recrée une session active à partir d'un
    // cookie persistent `ltr_remember` valide. Pas de body requis.
    //   - 200 + Set-Cookie ltr_token + JSON {ok:true} si HMAC valide ET
    //     PIN host inchangé.
    //   - 401 sinon (le client doit rediriger vers /login).
    server.Post("/api/auth/refresh", [&svc](const httplib::Request& req,
                                              httplib::Response& res) {
        const auto remember = readRememberCookie(req);
        if (remember.empty()) {
            res.status = 401;
            res.set_content("{\"error\":\"no_remember\"}", "application/json");
            return;
        }
        const auto deviceIdOpt = svc.sessions().verifyPersistentToken(
            remember, svc.accessPinRef());
        if (!deviceIdOpt) {
            res.status = 401;
            res.set_content("{\"error\":\"invalid_remember\"}", "application/json");
            // V1.6.5 : invalide proactivement le cookie corrompu/expiré.
            res.set_header("Set-Cookie",
                "ltr_remember=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0");
            return;
        }
        const auto& deviceId = *deviceIdOpt;
        const auto ua = req.get_header_value("User-Agent");

        // PIN match implicite (vérifié par HMAC), donc on appelle
        // authenticate avec le PIN actuel comme fourni.
        auto tokenOpt = svc.sessions().authenticate(
            svc.accessPinRef(), svc.accessPinRef(), deviceId, ua);
        if (!tokenOpt) {
            res.status = 500;
            res.set_content("{\"error\":\"recreate_failed\"}", "application/json");
            return;
        }
        const auto token = *tokenOpt;
        core::log_info("[auth/refresh] device=" + deviceId.substr(0, 8)
                       + "... → new token=" + token.substr(0, 8) + "...");

        res.set_header("Set-Cookie",
            "ltr_token=" + token + "; Path=/; HttpOnly; SameSite=Lax");

        if (auto s = svc.sessions().validate(token)) {
            svc.bus().post(core::PeerSeenEvent{s->device});
        }
        svc.emitWebPeersToAll();

        nlohmann::json j;
        j["ok"] = true;
        j["device_id"] = deviceId;
        res.set_content(j.dump(), "application/json");
    });

    // GET /api/me — whoami, nécessite cookie.
    server.Get("/api/me", [&svc](const httplib::Request& req,
                                  httplib::Response& res) {
        const auto token = readTokenCookie(req);
        core::log_info(std::string("[me] GET /api/me token=")
                       + (token.empty() ? "<empty>" : token.substr(0, 8)));
        const auto sess = svc.sessions().validate(token);
        if (!sess) {
            core::log_warn("[me] 401 unauth");
            res.status = 401;
            res.set_content("{\"error\":\"unauth\"}", "application/json");
            return;
        }
        svc.sessions().touch(token);

        nlohmann::json j;
        j["token"]    = token;
        j["deviceId"] = sess->deviceId;
        j["name"]     = sess->device.name;
        j["platform"] = sess->device.platform;
        // V1.2 — Sprint Web P2P : nom auto-généré + emoji + sous-titre
        // plateforme. Permet au JS de s'afficher lui-même de manière
        // cohérente avec la liste des autres peers.
        j["displayName"]   = sess->displayName;
        j["customName"]    = sess->customName;
        j["emoji"]         = sess->emoji;
        j["platformLabel"] = sess->platformLabel;
        res.set_content(j.dump(), "application/json");
    });

    // POST /api/me/name { custom_name } — met à jour l'alias du navigateur.
    server.Post("/api/me/name", [&svc](const httplib::Request& req,
                                        httplib::Response& res) {
        const auto token = readTokenCookie(req);
        auto sess = svc.sessions().validate(token);
        if (!sess) {
            res.status = 401;
            res.set_content("{\"error\":\"unauth\"}", "application/json");
            return;
        }
        std::string customName;
        try {
            const auto body = nlohmann::json::parse(req.body);
            customName = body.value("custom_name", "");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"bad_json\"}", "application/json");
            return;
        }
        if (!svc.sessions().updateCustomName(token, customName)) {
            res.status = 404;
            res.set_content("{\"error\":\"session_not_found\"}", "application/json");
            return;
        }
        svc.sessions().touch(token);
        svc.emitWebPeersToAll();

        sess = svc.sessions().validate(token);
        nlohmann::json j;
        j["ok"] = true;
        if (sess) {
            j["displayName"] = sess->displayName;
            j["customName"] = sess->customName;
            j["emoji"] = sess->emoji;
            j["platformLabel"] = sess->platformLabel;
        }
        res.set_content(j.dump(), "application/json");
    });
}

} // namespace ltr::web::routes
