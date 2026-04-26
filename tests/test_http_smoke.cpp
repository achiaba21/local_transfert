// Smoke test de l'HTTP server : bind, GET /api/host-info, POST /api/auth
// mauvais PIN → 401, bon PIN → 200 + cookie.

#include "ltr/core/event_bus.hpp"
#include "ltr/domain/device.hpp"
#include "ltr/web/web_service.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

int main() {
    // Prépare un downloadDir temporaire.
    const auto tmp = std::filesystem::temp_directory_path()
                   / "ltr_test_smoke";
    std::filesystem::create_directories(tmp);

    ltr::core::EventBus bus;
    ltr::domain::Device self;
    self.id       = "test-host";
    self.name     = "TestHost";
    self.platform = "Linux";

    ltr::web::WebService svc(bus, self, tmp);
    svc.start();

    // Laisse le temps au listener thread de démarrer.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    assert(svc.port() != 0);

    httplib::Client cli("127.0.0.1", svc.port());
    cli.set_connection_timeout(std::chrono::seconds(2));
    cli.set_read_timeout(std::chrono::seconds(2));
    cli.set_follow_location(false); // V1.1 : on veut voir le 302

    const std::string kDeviceId = "test-device-uuid";

    // 1) GET /api/host-info → 200 JSON
    {
        auto res = cli.Get("/api/host-info");
        assert(res);
        assert(res->status == 200);
        auto j = nlohmann::json::parse(res->body);
        assert(j["name"] == "TestHost");
        assert(j["platform"] == "Linux");
    }

    // 2) POST /api/auth mauvais PIN + device_id → 401 JSON
    {
        nlohmann::json body;
        body["pin"] = "000000";
        body["device_id"] = kDeviceId;
        auto res = cli.Post("/api/auth", body.dump(), "application/json");
        assert(res);
        assert(res->status == 401);
    }

    // 3) POST /api/auth bon PIN + device_id → 200 JSON + Set-Cookie
    // V1.1.1 : on renvoie 200 au lieu de 302 pour éviter le bug iOS Safari
    // où le Set-Cookie peut se perdre dans un redirect fetch().
    {
        nlohmann::json body;
        body["pin"] = svc.accessPin();
        body["device_id"] = kDeviceId;
        auto res = cli.Post("/api/auth", body.dump(), "application/json");
        assert(res);
        assert(res->status == 200);
        const auto cookie = res->get_header_value("Set-Cookie");
        assert(cookie.find("ltr_token=") != std::string::npos);
        auto j = nlohmann::json::parse(res->body);
        assert(j["ok"] == true);
        assert(j["next"] == "/");
    }

    // 4) GET / sans cookie → 302 Location:/login
    {
        auto res = cli.Get("/");
        assert(res);
        assert(res->status == 302);
        const auto loc = res->get_header_value("Location");
        assert(loc == "/login");
    }

    // 5) GET /login → 200 login.html
    {
        auto res = cli.Get("/login");
        assert(res);
        assert(res->status == 200);
        assert(res->body.find("Se connecter") != std::string::npos);
    }

    svc.stop();
    std::cout << "test_http_smoke OK (port=" << svc.port() << ")\n";
    return 0;
}
