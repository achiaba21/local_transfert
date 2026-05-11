#include "ltr/infra/known_peers.hpp"

#include <fstream>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

#include "ltr/core/logger.hpp"

namespace ltr::infra {

KnownPeers::KnownPeers(std::filesystem::path path)
    : path_(std::move(path)) {}

void KnownPeers::load() {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(path_, ec)) {
        core::log_info("[known_peers] aucun fichier existant, démarrage vide");
        return;
    }

    std::ifstream ifs(path_);
    if (!ifs) {
        core::log_warn("[known_peers] impossible d'ouvrir " + path_.string());
        return;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();

    try {
        const auto j = nlohmann::json::parse(ss.str());
        const auto& peers = j.value("peers", nlohmann::json::object());
        std::lock_guard<std::mutex> lk(mu_);
        peerToFp_.clear();
        for (auto it = peers.begin(); it != peers.end(); ++it) {
            if (it.value().is_string()) {
                peerToFp_[it.key()] = it.value().get<std::string>();
            }
        }
        core::log_info("[known_peers] chargé "
                       + std::to_string(peerToFp_.size())
                       + " pair(s) connu(s) depuis " + path_.string());
    } catch (const std::exception& e) {
        // BA : fichier corrompu → log warn, démarre vide (pas de refus boot).
        core::log_warn(std::string("[known_peers] JSON invalide (")
                       + e.what() + "), démarrage vide");
    }
}

std::optional<std::string> KnownPeers::get(std::string_view peerId) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = peerToFp_.find(std::string(peerId));
    if (it == peerToFp_.end()) return std::nullopt;
    return it->second;
}

KnownPeers::SetResult KnownPeers::set(std::string_view peerId,
                                       std::string_view fingerprint) {
    SetResult result;
    {
        std::lock_guard<std::mutex> lk(mu_);
        const std::string key(peerId);
        const std::string fp(fingerprint);
        auto it = peerToFp_.find(key);
        if (it == peerToFp_.end()) {
            peerToFp_.emplace(key, fp);
            result = SetResult::New;
        } else if (it->second == fp) {
            result = SetResult::Same;
        } else {
            it->second = fp;
            result = SetResult::Changed;
        }
        if (result != SetResult::Same) {
            saveLocked();
        }
    }
    return result;
}

void KnownPeers::save() const {
    std::lock_guard<std::mutex> lk(mu_);
    saveLocked();
}

std::size_t KnownPeers::size() const {
    std::lock_guard<std::mutex> lk(mu_);
    return peerToFp_.size();
}

void KnownPeers::saveLocked() const {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(path_.parent_path(), ec);

    nlohmann::json peers = nlohmann::json::object();
    for (const auto& [k, v] : peerToFp_) peers[k] = v;
    nlohmann::json root;
    root["peers"] = peers;

    // Écriture atomique : tmp → rename.
    const auto tmp = path_;
    const auto tmpFile = path_.string() + ".tmp";
    std::ofstream ofs(tmpFile, std::ios::trunc);
    if (!ofs) {
        core::log_error("[known_peers] impossible d'écrire " + tmpFile);
        return;
    }
    ofs << root.dump(2);
    ofs.close();
    fs::rename(tmpFile, path_, ec);
    if (ec) {
        core::log_error("[known_peers] rename échoué: " + ec.message());
    }
}

} // namespace ltr::infra
