#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace ltr::infra {

// V1.6.4 — Sprint Sécurité (Wave 2 TOFU TCP).
// Stocke l'empreinte connue de chaque pair vu (peerId → fingerprint).
// Persisté en JSON dans `cfgDir/known_peers.json` :
//   { "peers": { "<peerId>": "AB:CD:EF:..." , ... } }
//
// Cycle TOFU (Trust On First Use) :
//   - 1er set d'un peerId : SetResult::New, mémorisé silencieusement
//   - set ultérieur même fp : SetResult::Same
//   - set ultérieur fp DIFFÉRENT : SetResult::Changed → l'AppController
//     poste un FingerprintChangedEvent et marque le pair en warning
//
// Thread-safe : un mutex interne protège lectures et écritures.
class KnownPeers {
public:
    enum class SetResult { New, Same, Changed };

    explicit KnownPeers(std::filesystem::path path);

    // Charge le fichier JSON s'il existe. Si invalide : log warn,
    // démarre vide (per BA décision : ne pas refuser de démarrer).
    void load();

    // Retourne l'empreinte connue pour `peerId`, ou nullopt si inconnu.
    std::optional<std::string> get(std::string_view peerId) const;

    // Insère ou met à jour l'empreinte. Auto-save après chaque mutation.
    SetResult set(std::string_view peerId, std::string_view fingerprint);

    // Sauvegarde explicite (idempotent, déjà fait en interne par set()).
    void save() const;

    // Pour les tests.
    std::size_t size() const;

private:
    void saveLocked() const;

    std::filesystem::path path_;
    mutable std::mutex mu_;
    std::map<std::string, std::string> peerToFp_;
};

} // namespace ltr::infra
