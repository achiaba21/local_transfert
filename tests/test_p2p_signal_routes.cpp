// V1.2 — Sprint Web P2P
// Smoke test des routes /api/p2p/signal :
//  - 401 sans cookie
//  - 400 type inconnu
//  - 400 self-target (envoyer à soi-même)
//  - 404 destinataire absent
//  - 204 routage OK quand destinataire existe

#include "ltr/core/event_bus.hpp"
#include "ltr/domain/device.hpp"
#include "ltr/web/web_service.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace {

// V1.1 : extrait le cookie ltr_token de la réponse pour le rejouer.
std::string extractCookie(const httplib::Result& res) {
    if (!res || !res->has_header("Set-Cookie")) return "";
    const auto sc = res->get_header_value("Set-Cookie");
    const std::string prefix = "ltr_token=";
    auto pos = sc.find(prefix);
    if (pos == std::string::npos) return "";
    pos += prefix.size();
    auto end = sc.find(';', pos);
    return sc.substr(pos, end - pos);
}

std::string authAs(httplib::Client& cli, const std::string& pin,
                    const std::string& deviceId,
                    const std::string& ua) {
    nlohmann::json body;
    body["pin"]       = pin;
    body["device_id"] = deviceId;
    httplib::Headers h;
    h.emplace("Content-Type", "application/json");
    h.emplace("User-Agent",   ua);
    auto res = cli.Post("/api/auth", h, body.dump(), "application/json");
    assert(res);
    assert(res->status == 200);
    return extractCookie(res);
}

} // namespace

int main() {
    // 1. Setup serveur
    const auto tmp = std::filesystem::temp_directory_path()
                   / "ltr_test_p2p";
    std::filesystem::create_directories(tmp);

    ltr::core::EventBus bus;
    ltr::domain::Device self;
    self.id       = "test-host";
    self.name     = "TestHost";
    self.platform = "Linux";

    ltr::web::WebService svc(bus, self, tmp);
    svc.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    assert(svc.port() != 0);

    httplib::Client cli("127.0.0.1", svc.port());
    cli.set_connection_timeout(std::chrono::seconds(2));
    cli.set_read_timeout(std::chrono::seconds(2));
    cli.set_follow_location(false);

    const auto pin = svc.accessPin();

    // 2. POST /api/p2p/signal SANS cookie → 401
    {
        nlohmann::json body;
        body["to"]   = "any-device";
        body["type"] = "offer";
        auto res = cli.Post("/api/p2p/signal", body.dump(),
                             "application/json");
        assert(res);
        assert(res->status == 401);
    }

    // 3. Auth Alice + Bob
    const auto cookieA = authAs(cli,  pin, "alice-id",
        "Mozilla/5.0 (iPhone) Safari/605.1.15");
    const auto cookieB = authAs(cli,  pin, "bob-id",
        "Mozilla/5.0 (Linux; Android) Chrome/120.0");
    assert(!cookieA.empty());
    assert(!cookieB.empty());

    httplib::Headers hdrA{{"Cookie", "ltr_token=" + cookieA},
                            {"Content-Type", "application/json"}};

    // 4. type inconnu → 400
    {
        nlohmann::json body;
        body["to"]   = "bob-id";
        body["type"] = "evil-type";
        auto res = cli.Post("/api/p2p/signal", hdrA,
                             body.dump(), "application/json");
        assert(res);
        assert(res->status == 400);
    }

    // 5. self-target → 400
    {
        nlohmann::json body;
        body["to"]   = "alice-id";  // Alice envoie à elle-même
        body["type"] = "offer";
        auto res = cli.Post("/api/p2p/signal", hdrA,
                             body.dump(), "application/json");
        assert(res);
        assert(res->status == 400);
    }

    // 6. destinataire absent → 404
    {
        nlohmann::json body;
        body["to"]   = "nonexistent-device";
        body["type"] = "offer";
        auto res = cli.Post("/api/p2p/signal", hdrA,
                             body.dump(), "application/json");
        assert(res);
        assert(res->status == 404);
    }

    // 7. routage OK → 204
    {
        nlohmann::json body;
        body["to"]      = "bob-id";
        body["type"]    = "offer";
        body["payload"] = {{"sdp", "fake-sdp-data"}};
        auto res = cli.Post("/api/p2p/signal", hdrA,
                             body.dump(), "application/json");
        assert(res);
        assert(res->status == 204);
    }

    // 8. Body JSON invalide → 400
    {
        auto res = cli.Post("/api/p2p/signal", hdrA,
                             "{not-json", "application/json");
        assert(res);
        assert(res->status == 400);
    }

    svc.stop();
    std::cout << "test_p2p_signal_routes OK\n";
    return 0;
}
