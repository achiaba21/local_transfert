// Vérifie le hash SHA-256 d'un buffer connu (vecteurs NIST abrégés).

#include "ltr/infra/hash_service.hpp"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    using ltr::infra::HashService;

    // SHA-256("abc")
    // e.g. ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    const std::string a = "abc";
    const auto h = HashService::sha256Bytes(a.data(), a.size());
    assert(h == "ba7816bf8f01cfea414140de5dae2223"
                "b00361a396177a9cb410ff61f20015ad");

    // SHA-256("") =
    // e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    const auto h0 = HashService::sha256Bytes(nullptr, 0);
    assert(h0 == "e3b0c44298fc1c149afbf4c8996fb924"
                 "27ae41e4649b934ca495991b7852b855");

    std::cout << "test_hash OK\n";
    return 0;
}
