#include "ltr/web/routes/route_helpers.hpp"

#include <httplib.h>

namespace ltr::web::routes {

std::string readTokenCookie(const httplib::Request& req) {
    const auto h = req.get_header_value("Cookie");
    const std::string key = "ltr_token=";
    const auto pos = h.find(key);
    if (pos == std::string::npos) return {};
    const auto start = pos + key.size();
    const auto end = h.find(';', start);
    return h.substr(start,
        end == std::string::npos ? std::string::npos : end - start);
}

} // namespace ltr::web::routes
