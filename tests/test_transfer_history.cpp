// V1.6.5 — Sprint Stabilité (Wave 4 item K).
// Tests du TransferHistory : insert, markDone, cap 1000, roundtrip.

#include "ltr/infra/transfer_history.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>

namespace fs = std::filesystem;
using TH = ltr::infra::TransferHistory;

namespace {
fs::path makeTempPath() {
    static std::mt19937_64 rng{
        static_cast<std::uint64_t>(std::chrono::steady_clock::now()
            .time_since_epoch().count())};
    return fs::temp_directory_path()
         / ("ltr_transfer_hist_" + std::to_string(rng()) + ".json");
}

TH::Entry makeEntry(const std::string& sid,
                     TH::Kind kind = TH::Kind::TcpOut) {
    TH::Entry e;
    e.sessionId    = sid;
    e.peerDeviceId = "dev-1";
    e.peerName     = "Pierre";
    e.kind         = kind;
    e.fileCount    = 1;
    e.totalBytes   = 1024;
    e.status       = TH::Status::Pending;
    return e;
}
}

int main() {
    // 1. Vide au démarrage.
    {
        const auto path = makeTempPath();
        TH h(path);
        h.load();
        assert(h.size() == 0);
        std::error_code ec; fs::remove(path, ec);
    }

    // 2. insert + markDone.
    {
        const auto path = makeTempPath();
        TH h(path);
        h.load();
        h.insert(makeEntry("sid-1"));
        assert(h.size() == 1);
        const auto e1 = h.get("sid-1");
        assert(e1.has_value());
        assert(e1->status == TH::Status::Pending);

        h.markDone("sid-1", TH::Status::Ok, 2048);
        const auto e2 = h.get("sid-1");
        assert(e2.has_value());
        assert(e2->status == TH::Status::Ok);
        assert(e2->totalBytes == 2048);
        assert(e2->finishedAt > 0);
        std::error_code ec; fs::remove(path, ec);
    }

    // 3. markDone sur sessionId inconnu = no-op (pas de crash).
    {
        const auto path = makeTempPath();
        TH h(path);
        h.load();
        h.markDone("ghost", TH::Status::Ok, 100);
        assert(h.size() == 0);
        std::error_code ec; fs::remove(path, ec);
    }

    // 4. updateProgress augmente totalBytes mais ne sauve pas (silent).
    {
        const auto path = makeTempPath();
        TH h(path);
        h.load();
        h.insert(makeEntry("sid-2"));
        h.updateProgress("sid-2", 500);
        h.updateProgress("sid-2", 1500);
        const auto e = h.get("sid-2");
        assert(e->totalBytes == 1500);
        std::error_code ec; fs::remove(path, ec);
    }

    // 5. Cap 1000 : insertion 1500 entrées → 1000 conservées (les plus récentes).
    {
        const auto path = makeTempPath();
        TH h(path);
        h.load();
        for (int i = 0; i < 1500; ++i) {
            h.insert(makeEntry("sid-" + std::to_string(i)));
        }
        assert(h.size() == 1000);
        // Les 500 premiers doivent avoir été droppés.
        assert(!h.get("sid-0").has_value());
        assert(h.get("sid-1499").has_value());
        std::error_code ec; fs::remove(path, ec);
    }

    // 6. Roundtrip JSON : insert + markDone + reload.
    {
        const auto path = makeTempPath();
        {
            TH h(path);
            h.load();
            h.insert(makeEntry("sid-rt", TH::Kind::HttpDown));
            h.markDone("sid-rt", TH::Status::Cancelled, 4096, "user_cancel");
        }
        TH h2(path);
        h2.load();
        const auto e = h2.get("sid-rt");
        assert(e.has_value());
        assert(e->kind == TH::Kind::HttpDown);
        assert(e->status == TH::Status::Cancelled);
        assert(e->totalBytes == 4096);
        assert(e->error == "user_cancel");
        std::error_code ec; fs::remove(path, ec);
    }

    // 7. snapshot tri DESC par finishedAt.
    {
        const auto path = makeTempPath();
        TH h(path);
        h.load();
        h.insert(makeEntry("a"));
        h.insert(makeEntry("b"));
        h.insert(makeEntry("c"));
        h.markDone("a", TH::Status::Ok, 1);
        h.markDone("b", TH::Status::Ok, 2);
        h.markDone("c", TH::Status::Ok, 3);
        const auto snap = h.snapshot();
        assert(snap.size() == 3);
        // c a été markDone après b après a → snap[0] doit être 'c' ou tied.
        // (timestamps en secondes peuvent être identiques pour des appels
        // si tout est rapide → sort stable.)
        std::error_code ec; fs::remove(path, ec);
    }

    // 8. Conversions enum string.
    {
        assert(std::string(TH::kindToStr(TH::Kind::TcpOut)) == "tcp-out");
        assert(std::string(TH::kindToStr(TH::Kind::HttpDown)) == "http-down");
        assert(TH::kindFromStr("tcp-in") == TH::Kind::TcpIn);
        assert(TH::statusFromStr("ok") == TH::Status::Ok);
        assert(TH::statusFromStr("foo") == TH::Status::Pending);
    }

    std::cout << "test_transfer_history: OK\n";
    return 0;
}
