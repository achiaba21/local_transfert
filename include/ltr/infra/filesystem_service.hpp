#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "ltr/domain/file_meta.hpp"

namespace ltr::infra {

// Construit la liste des fichiers à envoyer à partir d'un ensemble de chemins
// (fichiers et/ou dossiers). Les dossiers sont parcourus récursivement. Les
// chemins relatifs retournés préservent la structure (un dossier `photos`
// deviendra `photos/vacances/img.jpg`).
struct FileEntry {
    std::filesystem::path absolutePath;
    domain::FileMeta meta;
};

class FilesystemService {
public:
    static std::vector<FileEntry> enumerate(
        const std::vector<std::filesystem::path>& inputs);

    // Calcule un chemin de destination unique dans `targetDir` pour le fichier
    // relatif donné. Si le fichier existe déjà, un suffixe numérique est
    // ajouté (`foo.txt` → `foo (1).txt`).
    static std::filesystem::path uniqueTargetPath(
        const std::filesystem::path& targetDir,
        const std::string& relativePath);

    // Vérifie qu'il reste au moins `requiredBytes` sur le volume de `path`.
    static bool hasSpace(const std::filesystem::path& path,
                         std::uint64_t requiredBytes);
};

} // namespace ltr::infra
