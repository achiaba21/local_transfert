#include "ltr/web/routes/route_helpers.hpp"

#include <httplib.h>

namespace ltr::web::routes {

// V1.6.5 — factorisation : lit un cookie nommé du header.
static std::string readNamedCookie(const httplib::Request& req,
                                    const std::string& name) {
    const auto h = req.get_header_value("Cookie");
    const std::string key = name + "=";
    const auto pos = h.find(key);
    if (pos == std::string::npos) return {};
    const auto start = pos + key.size();
    const auto end = h.find(';', start);
    return h.substr(start,
        end == std::string::npos ? std::string::npos : end - start);
}

std::string readTokenCookie(const httplib::Request& req) {
    return readNamedCookie(req, "ltr_token");
}

// V1.6.5 — Sprint Stabilité (Wave 3 item H).
std::string readRememberCookie(const httplib::Request& req) {
    return readNamedCookie(req, "ltr_remember");
}

} // namespace ltr::web::routes
