#include "ltr/web/routes/share_routes.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "ltr/core/png_encoder.hpp"
#include "ltr/web/qr_code.hpp"
#include "ltr/web/routes/multi_server.hpp"
#include "ltr/web/routes/route_helpers.hpp"
#include "ltr/web/web_service.hpp"

namespace ltr::web::routes {

namespace {

bool requireAuth(const httplib::Request& req, WebService& svc,
                 httplib::Response& res) {
    const auto token = readTokenCookie(req);
    if (!token.empty() && svc.sessions().validate(token)) return true;
    res.status = 401;
    res.set_header("Cache-Control", "no-store");
    res.set_content("{\"error\":\"unauth\"}", "application/json");
    return false;
}

std::string localHost(const std::string& url) {
    const auto scheme = url.find("://");
    const auto start = scheme == std::string::npos ? 0 : scheme + 3;
    const auto end = url.find('/', start);
    return url.substr(start, end == std::string::npos ? std::string::npos
                                                      : end - start);
}

std::string httpsShareUrl(WebService& svc) {
    const auto p = svc.portHttps();
    if (p == 0) return {};
    const auto http = svc.localUrl();
    const auto host = localHost(http);
    const auto colon = host.rfind(':');
    const auto ip = colon == std::string::npos ? host : host.substr(0, colon);
    if (ip.empty()) return {};
    return "https://" + ip + ":" + std::to_string(p);
}

std::string loginUrl(WebService& svc) {
    const auto baseHttps = httpsShareUrl(svc);
    const auto base = baseHttps.empty() ? svc.localUrl() : baseHttps;
    if (base.empty()) return {};
    return base + "/login?pin=" + svc.accessPin() + "&autologin=1";
}

std::vector<std::uint8_t> imageToRgba(const sf::Image& img) {
    const auto sz = img.getSize();
    const auto* px = img.getPixelsPtr();
    if (!px || sz.x == 0 || sz.y == 0) return {};
    return std::vector<std::uint8_t>(
        px, px + static_cast<std::size_t>(sz.x) * sz.y * 4);
}

} // namespace

void registerShare(WebService& svc) {
    auto server = routes::routerOf(svc);

    server.Get("/api/share-info", [&svc](const httplib::Request& req,
                                          httplib::Response& res) {
        if (!requireAuth(req, svc, res)) return;

        nlohmann::json j;
        j["hostName"]    = svc.self().name;
        j["platform"]    = svc.self().platform;
        j["pin"]         = svc.accessPin();
        j["httpUrl"]     = svc.localUrl();
        j["httpsUrl"]    = httpsShareUrl(svc);
        j["loginUrl"]    = loginUrl(svc);
        j["fingerprint"] = svc.fingerprint();

        res.set_header("Cache-Control", "no-store");
        res.set_content(j.dump(), "application/json");
    });

    server.Get("/api/share-qr.png", [&svc](const httplib::Request& req,
                                            httplib::Response& res) {
        if (!requireAuth(req, svc, res)) return;

        const auto url = loginUrl(svc);
        if (url.empty()) {
            res.status = 503;
            res.set_header("Cache-Control", "no-store");
            res.set_content("{\"error\":\"share_unavailable\"}", "application/json");
            return;
        }

        const auto img = QrCode::render(url, 360);
        const auto sz = img.getSize();
        const auto png = core::encodePng(
            sz.x, sz.y, imageToRgba(img));
        if (png.empty()) {
            res.status = 500;
            res.set_header("Cache-Control", "no-store");
            res.set_content("{\"error\":\"qr_encode_failed\"}", "application/json");
            return;
        }

        res.set_header("Cache-Control", "no-store");
        res.set_content(
            reinterpret_cast<const char*>(png.data()),
            png.size(),
            "image/png");
    });
}

} // namespace ltr::web::routes
