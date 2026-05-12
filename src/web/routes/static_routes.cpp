#include "ltr/web/routes/static_routes.hpp"

#include <httplib.h>
#include <string>

#include "ltr/core/logger.hpp"
#include "ltr/web/routes/multi_server.hpp"
#include "ltr/web/routes/route_helpers.hpp"
#include "ltr/web/static_asset.hpp"
#include "ltr/web/web_service.hpp"

// Headers générés par EmbedFile.cmake — uniquement pour les ressources
// dynamiques qui ont besoin d'une logique custom (auth, redirect…).
// La liste des assets purement statiques vit dans static_asset_registry.cpp.
#include "ltr/web/assets/index_html.hpp"
#include "ltr/web/assets/login_html.hpp"

namespace ltr::web::routes {

namespace {

using namespace ltr::web::assets;

void serveBytes(httplib::Response& res,
                std::string_view bytes,
                std::string_view mime,
                bool noCache) {
    if (noCache) {
        res.set_header("Cache-Control", "no-store, must-revalidate");
        res.set_header("Pragma", "no-cache");
    }
    res.set_content(bytes.data(), bytes.size(), std::string(mime).c_str());
}

bool isAuthed(const httplib::Request& req, WebService& svc) {
    const auto token = readTokenCookie(req);
    return !token.empty() && svc.sessions().validate(token).has_value();
}

} // namespace

void registerStatic(WebService& svc) {
    auto server = routes::routerOf(svc);

    // --- Pages dynamiques (auth-gated ou stateful) -----------------
    // GET / — dashboard, redirect /login si pas authentifié.
    server.Get("/", [&svc](const httplib::Request& req, httplib::Response& res) {
        const auto token = readTokenCookie(req);
        const bool ok = !token.empty() && svc.sessions().validate(token).has_value();
        core::log_info(std::string("[GET /] cookie=")
                       + (token.empty() ? "<absent>" : token.substr(0,8))
                       + " valid=" + (ok ? "1" : "0"));
        if (!ok) {
            res.status = 302;
            res.set_header("Location", "/login");
            return;
        }
        serveBytes(res, IndexHtml, IndexHtmlMime, true);
    });
    server.Get("/index.html", [&svc](const httplib::Request& req, httplib::Response& res) {
        if (!isAuthed(req, svc)) {
            res.status = 302;
            res.set_header("Location", "/login");
            return;
        }
        serveBytes(res, IndexHtml, IndexHtmlMime, true);
    });

    // GET /login — page de connexion (publique).
    server.Get("/login", [](const httplib::Request&, httplib::Response& res) {
        core::log_info("[GET /login]");
        serveBytes(res, LoginHtml, LoginHtmlMime, true);
    });
    server.Get("/login.html", [](const httplib::Request&, httplib::Response& res) {
        serveBytes(res, LoginHtml, LoginHtmlMime, true);
    });

    // --- Assets statiques déclarés dans la registry -----------------
    // SRP : la table d'assets vit dans static_asset_registry.cpp ;
    // ici on ne fait que les enregistrer comme handlers HTTP.
    for (const auto& a : buildStaticAssetTable()) {
        const auto path = std::string(a.path);
        const auto bytes = a.bytes;
        const auto mime  = a.mime;
        const bool noCache = a.noCache;
        server.Get(path, [bytes, mime, noCache]
            (const httplib::Request&, httplib::Response& res) {
                serveBytes(res, bytes, mime, noCache);
            });
    }
}

} // namespace ltr::web::routes
