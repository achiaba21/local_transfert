#pragma once

#include <httplib.h>
#include <string>
#include <vector>

#include "ltr/web/web_service.hpp"

namespace ltr::web::routes {

// V1.6.4 — Sprint Sécurité : proxy qui forward les enregistrements
// de routes (Get/Post) à plusieurs httplib::Server backends (HTTP +
// HTTPS coexistants). Permet d'écrire `server.Get(path, h)` 1 seule
// fois et que les 2 servers reçoivent le handler.
//
// Inclus uniquement par les fichiers route_*.cpp (qui ont accès à
// cpphttplib). Le header public web_service.hpp ne tire pas httplib.h.
class MultiServer {
public:
    void addBackend(httplib::Server* s) {
        if (s) backends_.push_back(s);
    }
    template <typename Handler>
    void Get(const std::string& path, Handler h) {
        for (auto* s : backends_) s->Get(path, h);
    }
    template <typename Handler>
    void Post(const std::string& path, Handler h) {
        for (auto* s : backends_) s->Post(path, h);
    }
    template <typename Handler>
    void Delete(const std::string& path, Handler h) {
        for (auto* s : backends_) s->Delete(path, h);
    }

private:
    std::vector<httplib::Server*> backends_;
};

// Construit un MultiServer regroupant tous les backends HTTP + HTTPS
// actifs du WebService donné.
inline MultiServer routerOf(WebService& svc) {
    MultiServer ms;
    ms.addBackend(&svc.httpServer().raw());
    if (auto* https = svc.httpsServer()) ms.addBackend(&https->raw());
    return ms;
}

} // namespace ltr::web::routes
