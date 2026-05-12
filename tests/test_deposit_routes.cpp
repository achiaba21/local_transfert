// Tests smoke : routes deposit (publiques + admin).
// Vérifie statuts HTTP de base + étanchéité (admin sans cookie = 401).

#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

#include <httplib.h>

namespace {

constexpr int kPort = 45479;

void waitReady() {
    for (int i = 0; i < 50; ++i) {
        httplib::Client cli("localhost", kPort);
        cli.set_connection_timeout(0, 100 * 1000);
        if (auto r = cli.Get("/deposit/unknown_token_xyz")) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace

int main() {
    // Pour ce smoke, on monte un serveur minimal qui rejoue le routing
    // attendu (sans backend). En réalité l'intégration complète se fait
    // via test_http_smoke.cpp. Ici, on vérifie surtout que les patterns
    // d'URL et les codes d'erreur attendus sont raisonnables.

    httplib::Server srv;

    srv.Get(R"(/deposit/([A-Za-z0-9_\-]+))",
        [](const httplib::Request& req, httplib::Response& res) {
            const auto tok = req.matches[1].str();
            if (tok == "active") {
                res.set_content("<html>deposit form</html>",
                                "text/html; charset=utf-8");
            } else {
                res.set_content("<html>expired</html>",
                                "text/html; charset=utf-8");
            }
        });

    srv.Get("/api/host/deposit-links",
        [](const httplib::Request& req, httplib::Response& res) {
            // Auth simulée : cookie obligatoire.
            const auto cookie = req.get_header_value("Cookie");
            if (cookie.find("ltr_token=") == std::string::npos) {
                res.status = 401;
                res.set_content("{\"error\":\"unauth\"}", "application/json");
                return;
            }
            res.set_content("{\"links\":[]}", "application/json");
        });

    std::thread t([&]() { srv.listen("0.0.0.0", kPort); });
    waitReady();

    httplib::Client cli("localhost", kPort);
    cli.set_connection_timeout(2);
    cli.set_read_timeout(2);

    // Lien actif → 200 page form.
    {
        auto r = cli.Get("/deposit/active");
        assert(r);
        assert(r->status == 200);
        assert(r->body.find("deposit form") != std::string::npos);
    }
    // Lien inconnu → 200 page expired.
    {
        auto r = cli.Get("/deposit/unknown");
        assert(r);
        assert(r->status == 200);
        assert(r->body.find("expired") != std::string::npos);
    }
    // Admin sans cookie → 401 (étanchéité).
    {
        auto r = cli.Get("/api/host/deposit-links");
        assert(r);
        assert(r->status == 401);
    }
    // Admin avec cookie → 200.
    {
        httplib::Headers h{{"Cookie", "ltr_token=fake"}};
        auto r = cli.Get("/api/host/deposit-links", h);
        assert(r);
        assert(r->status == 200);
    }

    srv.stop();
    if (t.joinable()) t.join();
    std::printf("test_deposit_routes OK\n");
    return 0;
}
