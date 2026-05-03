#include "ltr/core/png_encoder.hpp"

#include <miniz.h>

namespace ltr::core {

std::vector<std::uint8_t> encodePng(std::uint32_t w, std::uint32_t h,
                                    const std::vector<std::uint8_t>& rgba) {
    if (w == 0 || h == 0 || rgba.size() < std::size_t{w} * h * 4) return {};

    std::vector<std::uint8_t> out;
    auto put32 = [&](std::uint32_t v) {
        out.push_back(static_cast<std::uint8_t>(v >> 24));
        out.push_back(static_cast<std::uint8_t>(v >> 16));
        out.push_back(static_cast<std::uint8_t>(v >> 8));
        out.push_back(static_cast<std::uint8_t>(v));
    };
    auto putBytes = [&](const std::uint8_t* p, std::size_t n) {
        out.insert(out.end(), p, p + n);
    };
    auto putChunk = [&](const char tag[5],
                         const std::vector<std::uint8_t>& data) {
        put32(static_cast<std::uint32_t>(data.size()));
        const std::size_t crcStart = out.size();
        out.insert(out.end(), tag, tag + 4);
        if (!data.empty()) putBytes(data.data(), data.size());
        const auto crc = mz_crc32(0, out.data() + crcStart,
                                  out.size() - crcStart);
        put32(static_cast<std::uint32_t>(crc));
    };

    static const std::uint8_t kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    putBytes(kSig, 8);

    // IHDR (13 octets) — RGBA 8-bit non entrelacé.
    std::vector<std::uint8_t> ihdr;
    auto put32v = [&](std::uint32_t v) {
        ihdr.push_back(static_cast<std::uint8_t>(v >> 24));
        ihdr.push_back(static_cast<std::uint8_t>(v >> 16));
        ihdr.push_back(static_cast<std::uint8_t>(v >> 8));
        ihdr.push_back(static_cast<std::uint8_t>(v));
    };
    put32v(w);
    put32v(h);
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(6);  // color type RGBA
    ihdr.push_back(0);  // compression
    ihdr.push_back(0);  // filter
    ihdr.push_back(0);  // interlace
    putChunk("IHDR", ihdr);

    // Filtre None (0) en tête de chaque scanline.
    std::vector<std::uint8_t> filtered;
    filtered.reserve(static_cast<std::size_t>(h) * (1 + w * 4));
    for (std::uint32_t y = 0; y < h; ++y) {
        filtered.push_back(0);
        const std::uint8_t* row = rgba.data() + static_cast<std::size_t>(y) * w * 4;
        filtered.insert(filtered.end(), row, row + w * 4);
    }

    mz_ulong destLen = mz_compressBound(static_cast<mz_ulong>(filtered.size()));
    std::vector<std::uint8_t> compressed(destLen);
    if (mz_compress2(compressed.data(), &destLen,
                     filtered.data(),
                     static_cast<mz_ulong>(filtered.size()),
                     MZ_DEFAULT_COMPRESSION) != MZ_OK) {
        return {};
    }
    compressed.resize(destLen);
    putChunk("IDAT", compressed);

    putChunk("IEND", {});
    return out;
}

} // namespace ltr::core
