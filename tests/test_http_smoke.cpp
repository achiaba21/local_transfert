// Smoke test de l'HTTP server : bind, GET /api/host-info, POST /api/auth
// mauvais PIN → 401, bon PIN → 200 + cookie.

#include "ltr/core/event_bus.hpp"
#include "ltr/domain/device.hpp"
#include "ltr/web/web_service.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <future>
#include <iostream>
#include <thread>
#include <variant>

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
    std::string authCookie;

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
        const std::string prefix = "ltr_token=";
        const auto start = cookie.find(prefix) + prefix.size();
        const auto end = cookie.find(';', start);
        authCookie = cookie.substr(start, end - start);
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

    // 6) Routes share protégées : sans cookie → 401
    {
        auto res = cli.Get("/api/share-info");
        assert(res);
        assert(res->status == 401);
    }

    // 7) Routes share avec cookie → JSON + QR PNG
    {
        httplib::Headers h{{"Cookie", "ltr_token=" + authCookie}};
        auto infoRes = cli.Get("/api/share-info", h);
        assert(infoRes);
        assert(infoRes->status == 200);
        auto j = nlohmann::json::parse(infoRes->body);
        assert(j["pin"] == svc.accessPin());
        const std::string loginUrl = j["loginUrl"];
        assert(loginUrl.find("/login?pin=" + svc.accessPin())
               != std::string::npos);

        auto qrRes = cli.Get("/api/share-qr.png", h);
        assert(qrRes);
        assert(qrRes->status == 200);
        assert(qrRes->get_header_value("Content-Type").find("image/png")
               != std::string::npos);
        static const unsigned char sig[8] =
            {137, 80, 78, 71, 13, 10, 26, 10};
        assert(qrRes->body.size() > sizeof(sig));
        assert(std::memcmp(qrRes->body.data(), sig, sizeof(sig)) == 0);
    }

    // 8) Announce dossier web : multi-fichiers + relativePath,
    // affichage host = nom du dossier, puis acceptation manuelle.
    {
        httplib::Headers h{{"Cookie", "ltr_token=" + authCookie},
                           {"Content-Type", "application/json"}};
        nlohmann::json body;
        body["bundleKind"] = "folder";
        body["bundleName"] = "Photos";
        body["files"] = nlohmann::json::array({
            {{"name", "a.jpg"}, {"size", 10}, {"relativePath", "Photos/a.jpg"}},
            {{"name", "b.jpg"}, {"size", 20}, {"relativePath", "Photos/sub/b.jpg"}},
        });

        auto fut = std::async(std::launch::async, [&] {
            return cli.Post("/api/upload-announce", h,
                            body.dump(), "application/json");
        });

        std::string uploadId;
        for (int i = 0; i < 30 && uploadId.empty(); ++i) {
            for (const auto& ev : bus.drain()) {
                if (const auto* offer =
                        std::get_if<ltr::core::WebIncomingOfferEvent>(&ev)) {
                    uploadId = offer->uploadId;
                    assert(offer->filesCount == 2);
                    assert(offer->totalBytes == 30);
                    assert(offer->firstFileName == "Photos");
                }
            }
            if (uploadId.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        assert(!uploadId.empty());
        const auto snap = svc.announces().peek(uploadId);
        assert(snap.has_value());
        assert(snap->files.size() == 2);
        assert(snap->files[1].relativePath == "Photos/sub/b.jpg");
        assert(svc.announces().resolveAccept(uploadId, tmp));

        auto res = fut.get();
        assert(res);
        assert(res->status == 200);
        auto j = nlohmann::json::parse(res->body);
        assert(j["accepted"] == true);
    }

    svc.stop();
    std::cout << "test_http_smoke OK (port=" << svc.port() << ")\n";
    return 0;
}
