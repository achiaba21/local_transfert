// V1.6.5 — Sprint Stabilité (Wave 1, item D).
// Tests du parser "Range: bytes=N-M" RFC 7233.

#include "ltr/web/range_parser.hpp"

#include <cassert>
#include <iostream>

int main() {
    using ltr::web::parseRangeHeader;

    // 1. Vide ou absent → valid=false
    {
        const auto r = parseRangeHeader("", 1000);
        assert(!r.valid);
    }

    // 2. Pas le prefix "bytes=" → valid=false
    {
        const auto r = parseRangeHeader("items=0-100", 1000);
        assert(!r.valid);
    }

    // 3. Range complet "bytes=0-499" sur 1000 octets
    {
        const auto r = parseRangeHeader("bytes=0-499", 1000);
        assert(r.valid);
        assert(r.start == 0);
        assert(r.end == 499);
    }

    // 4. Range ouvert "bytes=500-" → jusqu'à la fin
    {
        const auto r = parseRangeHeader("bytes=500-", 1000);
        assert(r.valid);
        assert(r.start == 500);
        assert(r.end == 999);
    }

    // 5. Range hors limites end > totalSize → valid=false
    {
        const auto r = parseRangeHeader("bytes=0-2000", 1000);
        assert(!r.valid);
    }

    // 6. Range inversé start > end → valid=false
    {
        const auto r = parseRangeHeader("bytes=999-500", 1000);
        assert(!r.valid);
    }

    // 7. Range avec totalSize=0 → valid=false (rien à servir)
    {
        const auto r = parseRangeHeader("bytes=0-", 0);
        assert(!r.valid);
    }

    // 8. Garbage non-numérique → valid=false
    {
        const auto r = parseRangeHeader("bytes=abc-def", 1000);
        assert(!r.valid);
    }

    // 9. Pas de tiret → valid=false
    {
        const auto r = parseRangeHeader("bytes=500", 1000);
        assert(!r.valid);
    }

    // 10. Cas usuel browser retry : "bytes=1048576-" sur fichier 50 MB
    {
        constexpr std::uint64_t total = 50ULL * 1024 * 1024;
        const auto r = parseRangeHeader("bytes=1048576-", total);
        assert(r.valid);
        assert(r.start == 1048576);
        assert(r.end == total - 1);
    }

    // 11. Range exact = dernier byte
    {
        const auto r = parseRangeHeader("bytes=999-999", 1000);
        assert(r.valid);
        assert(r.start == 999);
        assert(r.end == 999);
    }

    std::cout << "test_range_parser: OK\n";
    return 0;
}
