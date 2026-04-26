#pragma once

#include <filesystem>
#include <string>

namespace ltr::infra {

// SHA-256 utilitaire. L'implémentation s'appuie sur picosha2 (header-only).
class HashService {
public:
    // Hash incrémental sur un fichier complet. Retourne la représentation hex.
    static std::string sha256File(const std::filesystem::path& path);

    // Hash d'un buffer en mémoire.
    static std::string sha256Bytes(const void* data, std::size_t size);
};

} // namespace ltr::infra
