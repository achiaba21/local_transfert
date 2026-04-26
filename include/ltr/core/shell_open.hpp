#pragma once

#include <filesystem>

namespace ltr::core {

// Ouvre un chemin avec l'application système par défaut.
// - Fichier → ouverture par l'app associée
// - Dossier → Finder (macOS) / Explorer (Windows) / xdg-open (Linux)
// Return true si la commande a pu être lancée (pas forcément succès
// fonctionnel côté OS, juste succès de spawn).
bool openInFileManager(const std::filesystem::path& path);

} // namespace ltr::core
