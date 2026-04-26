#pragma once

#include <cstdint>
#include <string>

namespace ltr::domain {

struct FileMeta {
    std::string relativePath;  // ex: "photos/vacances/img_1.jpg"
    std::uint64_t size{0};
    std::string sha256;        // optionnel (peut rester vide côté émetteur)
};

} // namespace ltr::domain
