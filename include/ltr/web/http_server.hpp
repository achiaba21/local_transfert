#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace httplib { class Server; }

namespace ltr::web {

// Façade cpp-httplib. Encapsule le bind+listen dans un thread dédié.
// Les handlers sont enregistrés AVANT start().
//
// V1.6.4 — Sprint Sécurité : support HTTPS via constructeur secure().
// L'instance interne devient SSLServer (héritage de Server, mêmes Get/
// Post API).
class HttpServer {
public:
    HttpServer();
    // V1.6.4 — Sprint Sécurité : constructeur HTTPS.
    HttpServer(const std::string& certPem, const std::string& keyPem);
    ~HttpServer();

    HttpServer(const HttpServer&)            = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    // Accès direct au serveur httplib pour enregistrer les routes
    // (Get/Post/Delete...).
    httplib::Server& raw();

    // Démarre sur le premier port libre dans [basePort, basePort + range - 1].
    // Retourne le port effectivement utilisé, ou 0 si aucun libre.
    std::uint16_t start(const std::string& bindAddr,
                        std::uint16_t basePort,
                        std::uint16_t range);

    void stop();

    std::uint16_t port() const noexcept { return port_.load(); }
    bool running()  const noexcept { return running_.load(); }

private:
    std::unique_ptr<httplib::Server> server_;
    std::thread listenerThread_;
    std::atomic<std::uint16_t> port_{0};
    std::atomic<bool> running_{false};
};

} // namespace ltr::web
