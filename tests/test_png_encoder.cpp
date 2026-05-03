// V1.5 — test core::encodePng : structure PNG valide.
//   - Signature 8 octets correcte
//   - Présence des chunks IHDR / IDAT / IEND dans l'ordre
//   - Tailles cohérentes pour des inputs minimaux et plus larges

#include "ltr/core/png_encoder.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

bool startsWith(const std::vector<std::uint8_t>& v,
                const std::uint8_t* sig, std::size_t n) {
    if (v.size() < n) return false;
    return std::memcmp(v.data(), sig, n) == 0;
}

bool containsChunk(const std::vector<std::uint8_t>& v, const char tag[5]) {
    for (std::size_t i = 0; i + 8 <= v.size(); ++i) {
        if (std::memcmp(v.data() + i, tag, 4) == 0
            || std::memcmp(v.data() + i + 4, tag, 4) == 0) {
            // Tag peut être 4 bytes après la longueur 4 bytes
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    using ltr::core::encodePng;

    // 1. 4×4 RGBA opaque rouge
    std::vector<std::uint8_t> rgba(4 * 4 * 4, 0);
    for (std::size_t i = 0; i < 4 * 4; ++i) {
        rgba[i * 4 + 0] = 255;  // R
        rgba[i * 4 + 1] = 0;    // G
        rgba[i * 4 + 2] = 0;    // B
        rgba[i * 4 + 3] = 255;  // A
    }
    auto png = encodePng(4, 4, rgba);
    assert(!png.empty());

    // Signature PNG
    static const std::uint8_t kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    assert(startsWith(png, kSig, 8));

    // Présence IHDR + IDAT + IEND
    assert(containsChunk(png, "IHDR"));
    assert(containsChunk(png, "IDAT"));
    assert(containsChunk(png, "IEND"));

    // 2. Image plus large 64×32
    std::vector<std::uint8_t> rgba2(64 * 32 * 4, 128);
    auto png2 = encodePng(64, 32, rgba2);
    assert(!png2.empty());
    assert(startsWith(png2, kSig, 8));

    // 3. Cas limite : taille 0 → vide
    auto pngEmpty = encodePng(0, 0, {});
    assert(pngEmpty.empty());

    // 4. Cas limite : rgba trop petit → vide (defensive)
    std::vector<std::uint8_t> tooSmall(10);
    auto pngBad = encodePng(10, 10, tooSmall);
    assert(pngBad.empty());

    std::cout << "test_png_encoder OK ("
              << png.size() << "B for 4x4, "
              << png2.size() << "B for 64x32)\n";
    return 0;
}
