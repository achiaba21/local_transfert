#pragma once

#include <string_view>
#include <vector>

namespace ltr::web {

// Description déclarative d'un asset web statique (HTML, JS, CSS, icône).
// Permet de mapper path → bytes + mime sans hardcoder 30 handlers.
struct StaticAsset {
    std::string_view path;   // ex. "/style.css"
    std::string_view bytes;
    std::string_view mime;
    bool             noCache{true};   // toujours no-store en dev par défaut
};

// Construit la table de tous les assets servis par WebService.
// Centralisée ici pour faciliter l'ajout d'un nouvel asset : 1 ligne dans
// la table, pas un nouveau handler. SRP : la liste des assets vit ici,
// la registration des handlers HTTP vit dans static_routes.cpp.
std::vector<StaticAsset> buildStaticAssetTable();

} // namespace ltr::web
