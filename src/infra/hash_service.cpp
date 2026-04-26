#include "ltr/infra/hash_service.hpp"

#include <fstream>
#include <vector>

#include <picosha2.h>

namespace ltr::infra {

std::string HashService::sha256File(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};

    picosha2::hash256_one_by_one hasher;
    hasher.init();

    std::vector<char> buf(64 * 1024);
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const auto got = in.gcount();
        if (got > 0) {
            hasher.process(buf.begin(), buf.begin() + got);
        }
    }
    hasher.finish();
    return picosha2::get_hash_hex_string(hasher);
}

std::string HashService::sha256Bytes(const void* data, std::size_t size) {
    const auto* p = static_cast<const unsigned char*>(data);
    return picosha2::hash256_hex_string(p, p + size);
}

} // namespace ltr::infra
