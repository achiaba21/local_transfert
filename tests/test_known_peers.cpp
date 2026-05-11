// V1.6.4 — Sprint Sécurité (Wave 2 TOFU TCP).
// Tests du KnownPeers : roundtrip JSON + sémantique New/Same/Changed.

#include "ltr/infra/known_peers.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>

namespace fs = std::filesystem;

namespace {

fs::path makeTempPath() {
    const auto base = fs::temp_directory_path();
    static std::mt19937_64 rng{
        static_cast<std::uint64_t>(std::chrono::steady_clock::now()
            .time_since_epoch().count())};
    const auto p = base / ("ltr_known_peers_test_"
                            + std::to_string(rng()) + ".json");
    return p;
}

} // namespace

int main() {
    using ltr::infra::KnownPeers;

    // 1. Vide au démarrage.
    {
        const auto path = makeTempPath();
        KnownPeers kp(path);
        kp.load();
        assert(kp.size() == 0);
        assert(!kp.get("foo").has_value());
        std::error_code ec;
        fs::remove(path, ec);
    }

    // 2. SetResult sémantique : New → Same → Changed.
    {
        const auto path = makeTempPath();
        KnownPeers kp(path);
        kp.load();

        const auto r1 = kp.set("peer-A", "AB:CD:EF:01:02:03");
        assert(r1 == KnownPeers::SetResult::New);
        assert(kp.size() == 1);

        const auto r2 = kp.set("peer-A", "AB:CD:EF:01:02:03");
        assert(r2 == KnownPeers::SetResult::Same);
        assert(kp.size() == 1);

        const auto r3 = kp.set("peer-A", "FF:FF:FF:00:00:00");
        assert(r3 == KnownPeers::SetResult::Changed);
        assert(kp.size() == 1);

        const auto fp = kp.get("peer-A");
        assert(fp.has_value());
        assert(*fp == "FF:FF:FF:00:00:00");

        std::error_code ec;
        fs::remove(path, ec);
    }

    // 3. Roundtrip JSON : set → save → re-instancier → get.
    {
        const auto path = makeTempPath();
        {
            KnownPeers kp(path);
            kp.load();
            kp.set("alice", "AA:BB:CC");
            kp.set("bob",   "DD:EE:FF");
            // set() auto-save donc pas besoin d'appeler save() ici.
        }

        // Re-instancier et vérifier la persistance.
        KnownPeers kp2(path);
        kp2.load();
        assert(kp2.size() == 2);

        const auto fa = kp2.get("alice");
        assert(fa.has_value() && *fa == "AA:BB:CC");

        const auto fb = kp2.get("bob");
        assert(fb.has_value() && *fb == "DD:EE:FF");

        std::error_code ec;
        fs::remove(path, ec);
    }

    // 4. Plusieurs pairs indépendants : set d'un nouveau ne perturbe pas
    //    les anciens.
    {
        const auto path = makeTempPath();
        KnownPeers kp(path);
        kp.load();
        kp.set("p1", "11:22:33");
        kp.set("p2", "44:55:66");
        const auto r = kp.set("p3", "77:88:99");
        assert(r == KnownPeers::SetResult::New);
        assert(kp.size() == 3);
        assert(*kp.get("p1") == "11:22:33");
        assert(*kp.get("p2") == "44:55:66");
        std::error_code ec;
        fs::remove(path, ec);
    }

    // 5. Fichier corrompu : log warn + démarre vide (pas de throw).
    {
        const auto path = makeTempPath();
        {
            std::ofstream ofs(path);
            ofs << "{ this is not valid JSON";
        }
        KnownPeers kp(path);
        kp.load();
        assert(kp.size() == 0);
        std::error_code ec;
        fs::remove(path, ec);
    }

    std::cout << "test_known_peers: OK\n";
    return 0;
}
