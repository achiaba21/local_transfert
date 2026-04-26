#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ltr::web {

// Abstraction OS-spécifique pour servir le binaire de l'app courante.
// Une seule impl compilée selon la plateforme (voir CMakeLists.txt).
namespace SelfBinary {

// Chemin absolu de l'exécutable courant (Windows: .exe ; Linux: ELF ;
// macOS: .../Contents/MacOS/local_transfer).
std::filesystem::path currentExecutablePath();

// Nom de fichier suggéré pour le téléchargement (Content-Disposition).
std::string suggestedDownloadName();

// MIME type approprié.
std::string mimeType();

// Produit le flux complet du binaire (Windows/Linux) ou d'un zip .app
// (macOS, streamé via miniz). Retourne false en cas d'échec.
//
// Le contenu est écrit dans le buffer (accumulation). Pour éviter d'exploser
// la RAM sur macOS, on peut plus tard migrer vers un streaming provider
// cpp-httplib. Pour V1, le binaire max est ~50 Mo, acceptable.
bool produceBytes(std::vector<std::uint8_t>& out);

} // namespace SelfBinary

} // namespace ltr::web
