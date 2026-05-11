#include "ltr/web/http_server.hpp"

#include <httplib.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <system_error>

#include "ltr/core/logger.hpp"

namespace ltr::web {

namespace {

// V1.6.4 audit fix — cross-platform : remplace `mkstemp` + path "/tmp/..."
// hardcodé par std::filesystem::temp_directory_path() (Mac : /tmp,
// Windows : %TEMP%, Linux : /tmp ou $TMPDIR).
std::filesystem::path makeTempPath(const std::string& prefix) {
    namespace fs = std::filesystem;
    static thread_local std::mt19937_64 rng{
        static_cast<std::uint64_t>(std::chrono::steady_clock::now()
            .time_since_epoch().count())};
    std::error_code ec;
    fs::path dir = fs::temp_directory_path(ec);
    if (ec || dir.empty()) dir = fs::path("."); // fallback ultime
    return dir / (prefix + "-" + std::to_string(rng()) + ".pem");
}

// Écriture binaire complète avec contrôle de l'état du flux (corrige le
// `write()` POSIX return value ignored signalé par l'audit).
bool writeFileFull(const std::filesystem::path& p, const std::string& data) {
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
    ofs.close();
    return ofs.good();
}

} // namespace

HttpServer::HttpServer()
    : server_(std::make_unique<httplib::Server>()) {}

// V1.6.4 — Sprint Sécurité : constructeur HTTPS via SSLServer.
// cpp-httplib SSLServer prend des chemins de fichiers, pas des PEM en
// mémoire. On écrit dans le dossier temp système (cross-platform) puis
// on instancie. Les fichiers sont supprimés immédiatement après que
// SSLServer ait chargé les PEM en RAM.
HttpServer::HttpServer(const std::string& certPem, const std::string& keyPem) {
    namespace fs = std::filesystem;
    const auto certPath = makeTempPath("ltr-cert");
    const auto keyPath  = makeTempPath("ltr-key");

    if (!writeFileFull(certPath, certPem) || !writeFileFull(keyPath, keyPem)) {
        core::log_error("HttpServer SSL: écriture PEM temp échouée");
        std::error_code ec;
        fs::remove(certPath, ec);
        fs::remove(keyPath, ec);
        server_ = std::make_unique<httplib::Server>();
        return;
    }

    server_ = std::make_unique<httplib::SSLServer>(
        certPath.string().c_str(), keyPath.string().c_str());

    // Cleanup les fichiers temp (SSLServer a chargé en RAM).
    std::error_code ec;
    fs::remove(certPath, ec);
    fs::remove(keyPath, ec);

    if (!static_cast<httplib::SSLServer*>(server_.get())->is_valid()) {
        core::log_error("HttpServer SSL: SSLServer is_valid()=false");
    } else {
        core::log_info("HttpServer SSL: SSLServer prêt");
    }
}

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
