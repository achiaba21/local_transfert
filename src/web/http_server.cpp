#include "ltr/web/http_server.hpp"

#include <httplib.h>

#include "ltr/core/logger.hpp"

namespace ltr::web {

HttpServer::HttpServer()
    : server_(std::make_unique<httplib::Server>()) {}

HttpServer::~HttpServer() {
    stop();
}

httplib::Server& HttpServer::raw() {
    return *server_;
}

std::uint16_t HttpServer::start(const std::string& bindAddr,
                                std::uint16_t basePort,
                                std::uint16_t range) {
    if (running_.exchange(true)) return port_.load();

    std::uint16_t chosen = 0;
    for (std::uint16_t i = 0; i < range; ++i) {
        const std::uint16_t p = static_cast<std::uint16_t>(basePort + i);
        if (server_->bind_to_port(bindAddr.c_str(), p)) {
            chosen = p;
            break;
        }
    }

    if (chosen == 0) {
        core::log_error("HttpServer: aucun port libre dans ["
                        + std::to_string(basePort) + ".."
                        + std::to_string(basePort + range - 1) + "]");
        running_.store(false);
        return 0;
    }

    port_.store(chosen);

    listenerThread_ = std::thread([this]{
        try {
            // listen_after_bind est blocant jusqu'à server_->stop().
            server_->listen_after_bind();
        } catch (const std::exception& e) {
            core::log_error(std::string("HttpServer::listen exception: ") + e.what());
        } catch (...) {
            core::log_error("HttpServer::listen exception inconnue");
        }
    });

    core::log_info("HttpServer démarré sur " + bindAddr + ":"
                   + std::to_string(chosen));
    return chosen;
}

void HttpServer::stop() {
    if (!running_.exchange(false)) return;
    if (server_) server_->stop();
    if (listenerThread_.joinable()) listenerThread_.join();
    port_.store(0);
}

} // namespace ltr::web
