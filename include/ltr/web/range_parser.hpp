#pragma once

#include <cstdint>
#include <string>

namespace ltr::web {

// V1.6.5 — Sprint Stabilité (Wave 1, item D).
// Parse le header HTTP "Range: bytes=N-M" (RFC 7233). Ne supporte qu'un
// seul range, format "bytes=N-" (jusqu'à la fin) ou "bytes=N-M". Pour un
// Range invalide ou hors-limites, retourne `valid=false` → l'appelant
// répond 200 normal (full file).
//
// Exemples valides :
//   parseRangeHeader("bytes=0-499", 1000)  → {0, 499, true}
//   parseRangeHeader("bytes=500-",  1000)  → {500, 999, true}
//   parseRangeHeader("",            1000)  → {0, 0, false}
//   parseRangeHeader("bytes=0-1000",1000)  → {0, 0, false}  (hors limites)
struct RangeInfo {
    std::uint64_t start{0};
    std::uint64_t end{0};
    bool valid{false};
};

RangeInfo parseRangeHeader(const std::string& header, std::uint64_t totalSize);

} // namespace ltr::web
