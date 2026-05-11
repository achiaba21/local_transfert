// V1.6.5 — Sprint Stabilité (Wave 4 item J).
// Tests du PeersHistory : touch, recordTransfer, purge, forget.

#include "ltr/infra/peers_history.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>

namespace fs = std::filesystem;

namespace {
fs::path makeTempPath() {
    static std::mt19937_64 rng{
        static_cast<std::uint64_t>(std::chrono::steady_clock::now()
            .time_since_epoch().count())};
    return fs::temp_directory_path()
         / ("ltr_peers_hist_test_" + std::to_string(rng()) + ".json");
}
}

int main() {
    using ltr::infra::PeersHistory;

    // 1. Vide au démarrage.
    {
        const auto path = makeTempPath();
        PeersHistory h(path);
        h.load();
        assert(h.size() == 0);
        std::error_code ec; fs::remove(path, ec);
    }

    // 2. touch insère puis met à jour.
    {
        const auto path = makeTempPath();
        PeersHistory h(path);
        h.load();
        h.touch("dev-A", "Mac de Serge", "macOS", "native", "AB:CD");
        assert(h.size() == 1);
        const auto e1 = h.get("dev-A");
        assert(e1.has_value());
        assert(e1->name == "Mac de Serge");
        assert(e1->firstSeen == e1->lastSeen);

        h.touch("dev-A", "Mac de Serge (renamed)", "macOS", "native", "AB:CD");
        const auto e2 = h.get("dev-A");
        assert(e2.has_value());
        assert(e2->name == "Mac de Serge (renamed)");
        assert(e2->lastSeen >= e1->lastSeen);
        assert(e2->firstSeen == e1->firstSeen);  // pas écrasé

        std::error_code ec; fs::remove(path, ec);
    }

    // 3. recordTransfer incrémente compteurs.
    {
        const auto path = makeTempPath();
        PeersHistory h(path);
        h.load();
        h.touch("dev-X", "Pierre", "iOS", "web", "");
        h.recordTransfer("dev-X", 1024 * 1024);
        h.recordTransfer("dev-X", 2 * 1024 * 1024);
        const auto e = h.get("dev-X");
        assert(e.has_value());
        assert(e->totalTransfers == 2);
        assert(e->totalBytes == 3 * 1024 * 1024);
        std::error_code ec; fs::remove(path, ec);
    }

    // 4. recordTransfer sur deviceId inconnu = no-op.
    {
        const auto path = makeTempPath();
        PeersHistory h(path);
        h.load();
        h.recordTransfer("ghost", 1000);
        assert(h.size() == 0);
        std::error_code ec; fs::remove(path, ec);
    }

    // 5. forget supprime l'entry.
    {
        const auto path = makeTempPath();
        PeersHistory h(path);
        h.load();
        h.touch("dev-Q", "X", "Linux", "native", "");
        h.touch("dev-R", "Y", "Linux", "native", "");
        assert(h.size() == 2);
        h.forget("dev-Q");
        assert(h.size() == 1);
        assert(!h.get("dev-Q").has_value());
        assert(h.get("dev-R").has_value());
        std::error_code ec; fs::remove(path, ec);
    }

    // 6. snapshotOffline filtre par âge + exclut online.
    {
        const auto path = makeTempPath();
        PeersHistory h(path);
        h.load();
        h.touch("p-on", "Online", "iOS", "web", "");
        h.touch("p-off", "Offline", "iOS", "web", "");

        // minOfflineSec=0 : tous, mais on exclut "p-on"
        const auto offline1 = h.snapshotOffline(0, {"p-on"});
        assert(offline1.size() == 1);
        assert(offline1[0].deviceId == "p-off");

        // minOfflineSec=10000 : aucun (juste touchés)
        const auto offline2 = h.snapshotOffline(10000, {});
        assert(offline2.empty());

        std::error_code ec; fs::remove(path, ec);
    }

    // 7. Roundtrip JSON : sauve, recharge, vérifie.
    {
        const auto path = makeTempPath();
        {
            PeersHistory h(path);
            h.load();
            h.touch("alice", "Alice", "macOS", "native", "AA:BB");
            h.recordTransfer("alice", 4096);
        }
        PeersHistory h2(path);
        h2.load();
        const auto e = h2.get("alice");
        assert(e.has_value());
        assert(e->name == "Alice");
        assert(e->fingerprint == "AA:BB");
        assert(e->totalTransfers == 1);
        assert(e->totalBytes == 4096);
        std::error_code ec; fs::remove(path, ec);
    }

    std::cout << "test_peers_history: OK\n";
    return 0;
}
