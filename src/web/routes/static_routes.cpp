#include "ltr/web/routes/static_routes.hpp"

#include <httplib.h>
#include <string>

#include "ltr/core/logger.hpp"
#include "ltr/web/routes/route_helpers.hpp"
#include "ltr/web/web_service.hpp"
#include "ltr/web/routes/multi_server.hpp"

// Headers générés par EmbedFile.cmake au build.
#include "ltr/web/assets/index_html.hpp"
#include "ltr/web/assets/login_html.hpp"
#include "ltr/web/assets/app_js.hpp"
#include "ltr/web/assets/common_js.hpp"
#include "ltr/web/assets/upload_js.hpp"
#include "ltr/web/assets/download_js.hpp"
#include "ltr/web/assets/peers_js.hpp"   // V1.2 — Sprint Web P2P
#include "ltr/web/assets/p2p_js.hpp"     // V1.2 — Sprint Web P2P
#include "ltr/web/assets/p2p_transport_js.hpp"  // V1.6.1 — split refactor
#include "ltr/web/assets/p2p_session_js.hpp"    // V1.6.1 — split refactor
#include "ltr/web/assets/p2p_ui_js.hpp"         // V1.6.1 — split refactor
#include "ltr/web/assets/transfer_registry_js.hpp"  // V1.3 — Sprint Web P2P V1.3
#include "ltr/web/assets/login_js.hpp"
#include "ltr/web/assets/style_css.hpp"
#include "ltr/web/assets/icon_upload.hpp"
#include "ltr/web/assets/icon_download.hpp"
#include "ltr/web/assets/icon_file.hpp"

namespace ltr::web::routes {

namespace {

using namespace ltr::web::assets;

void serveStatic(httplib::Response& res,
                 std::string_view bytes,
                 std::string_view mime) {
    // V1.1.1 : pendant le dev, tout en no-store pour éviter les caches
    // navigateurs qui servent d'anciennes versions des assets.
    res.set_header("Cache-Control", "no-store, must-revalidate");
    res.set_header("Pragma", "no-cache");
    res.set_content(bytes.data(), bytes.size(), std::string(mime).c_str());
}

void serveNoCache(httplib::Response& res,
                  std::string_view bytes,
                  std::string_view mime) {
    res.set_header("Cache-Control", "no-store, must-revalidate");
    res.set_header("Pragma", "no-cache");
    res.set_content(bytes.data(), bytes.size(), std::string(mime).c_str());
}

// V1.1 : guard sur / — redirige vers /login si pas de cookie valide.
bool isAuthed(const httplib::Request& req, WebService& svc) {
    const auto token = readTokenCookie(req);
    return !token.empty() && svc.sessions().validate(token).has_value();
}

} // namespace

void registerStatic(WebService& svc) {
    auto server = routes::routerOf(svc);

    // GET / — page principale, protégée par cookie.
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
        serveNoCache(res, IndexHtml, IndexHtmlMime);
    });
    server.Get("/index.html", [&svc](const httplib::Request& req, httplib::Response& res) {
        if (!isAuthed(req, svc)) {
            res.status = 302;
            res.set_header("Location", "/login");
            return;
        }
        serveNoCache(res, IndexHtml, IndexHtmlMime);
    });

    // GET /login — page de connexion (public).
    server.Get("/login", [](const httplib::Request&, httplib::Response& res) {
        core::log_info("[GET /login]");
        serveNoCache(res, LoginHtml, LoginHtmlMime);
    });
    server.Get("/login.html", [](const httplib::Request&, httplib::Response& res) {
        serveNoCache(res, LoginHtml, LoginHtmlMime);
    });

    // GET /login.js — script de la page de connexion (public).
    server.Get("/login.js", [](const httplib::Request&, httplib::Response& res) {
        core::log_info("[GET /login.js]");
        serveStatic(res, LoginJs, LoginJsMime);
    });

    // Assets statiques communs (pas d'auth requise).
    server.Get("/app.js", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, AppJs, AppJsMime);
    });
    server.Get("/common.js", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, CommonJs, CommonJsMime);
    });
    server.Get("/upload.js", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, UploadJs, UploadJsMime);
    });
    server.Get("/download.js", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, DownloadJs, DownloadJsMime);
    });
    // V1.2 — Sprint Web P2P : annuaire JS.
    server.Get("/peers.js", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, PeersJs, PeersJsMime);
    });
    // V1.2 — Sprint Web P2P : module WebRTC DataChannel.
    server.Get("/p2p.js", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, P2pJs, P2pJsMime);
    });
    // V1.6.1 — Sprint refactor : 3 sous-modules (transport / session / ui).
    server.Get("/p2p_transport.js", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, P2pTransportJs, P2pTransportJsMime);
    });
    server.Get("/p2p_session.js", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, P2pSessionJs, P2pSessionJsMime);
    });
    server.Get("/p2p_ui.js", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, P2pUiJs, P2pUiJsMime);
    });
    // V1.3 — Sprint Web P2P V1.3 : registry / liste persistante.
    server.Get("/transfer_registry.js", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, TransferRegistryJs, TransferRegistryJsMime);
    });
    server.Get("/style.css", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, StyleCss, StyleCssMime);
    });
    server.Get("/icons/upload.svg", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, IconUpload, IconUploadMime);
    });
    server.Get("/icons/download.svg", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, IconDownload, IconDownloadMime);
    });
    server.Get("/icons/file.svg", [](const httplib::Request&, httplib::Response& res) {
        serveStatic(res, IconFile, IconFileMime);
    });
}

} // namespace ltr::web::routes
