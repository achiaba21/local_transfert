// Sprint Transfer Resume — tests unitaires I/O sidecar.

#include "ltr/infra/resume_sidecar.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;
using namespace ltr::infra;

namespace {

fs::path makeTmpDir() {
    const auto dir = fs::temp_directory_path() / "ltr_test_resume_sidecar";
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

Sidecar makeSidecar(const std::string& sid) {
    Sidecar s;
    s.sessionId      = sid;
    s.senderDeviceId = "uuid-sender";
    s.senderName     = "Mac de Serge";
    s.createdAt      = std::chrono::system_clock::now();
    s.lastUpdateAt   = std::chrono::system_clock::now();

    {
        SidecarFileState f;
        f.relativePath  = "MyFolder/a.txt";
        f.status        = FileResumeStatus::Done;
        f.expectedSize  = 1024;
        f.bytesReceived = 1024;
        f.sha256Prefix  = "abcdef0123456789";
        s.files.push_back(f);
    }
    {
        SidecarFileState f;
        f.relativePath  = "MyFolder/b.bin";
        f.status        = FileResumeStatus::Partial;
        f.expectedSize  = 5242880;
        f.bytesReceived = 3145728;
        f.sha256Prefix  = "1122334455667788";
        f.partialPath   = "MyFolder/b.bin.part";
        s.files.push_back(f);
    }
    return s;
}

} // namespace

int main() {
    const auto dir = makeTmpDir();

    // 1) Roundtrip write → read
    {
        const auto s = makeSidecar("abc123");
        assert(writeSidecar(dir, s));
        auto loaded = readSidecar(dir, "abc123");
        assert(loaded.has_value());
        assert(loaded->sessionId == "abc123");
        assert(loaded->files.size() == 2);
        assert(loaded->files[0].status == FileResumeStatus::Done);
        assert(loaded->files[1].status == FileResumeStatus::Partial);
        assert(loaded->files[1].bytesReceived == 3145728);
        assert(loaded->files[1].partialPath == "MyFolder/b.bin.part");
        std::cout << "  [ok] roundtrip write/read\n";
    }

    // 2) Delete
    {
        deleteSidecar(dir, "abc123");
        auto loaded = readSidecar(dir, "abc123");
        assert(!loaded.has_value());
        std::cout << "  [ok] delete\n";
    }

    // 3) Sidecar corrompu → delete + return nullopt
    {
        const auto p = sidecarPath(dir, "corrupt");
        {
            std::ofstream out(p);
            out << "not json at all {{{ broken";
        }
        auto loaded = readSidecar(dir, "corrupt");
        assert(!loaded.has_value());
        assert(!fs::exists(p));
        std::cout << "  [ok] corrupted JSON deletes + nullopt\n";
    }

    // 4) Purge >0h : tous les sidecars doivent être supprimés
    {
        // Recréer 2 sidecars
        assert(writeSidecar(dir, makeSidecar("sess1")));
        assert(writeSidecar(dir, makeSidecar("sess2")));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        const int removed = purgeOldSidecars(dir, 0);
        assert(removed == 2);
        assert(!readSidecar(dir, "sess1").has_value());
        assert(!readSidecar(dir, "sess2").has_value());
        std::cout << "  [ok] purge ttl=0 removes all\n";
    }

    // 5) Purge TTL large : rien supprimé
    {
        assert(writeSidecar(dir, makeSidecar("sess3")));
        const int removed = purgeOldSidecars(dir, 24);
        assert(removed == 0);
        assert(readSidecar(dir, "sess3").has_value());
        std::cout << "  [ok] purge ttl=24h keeps recent\n";
    }

    // 6) Pending sessions roundtrip
    {
        PendingSession ps;
        ps.sessionId   = "pending-1";
        ps.peerId      = "peer-uuid";
        ps.peerName    = "MacBook Pro";
        ps.peerIp      = "192.168.1.3";
        ps.peerTcpPort = 45455;
        ps.pinCode     = "472931";
        ps.sourcePaths = {"/Users/me/MyFolder", "/Users/me/photo.jpg"};
        ps.totalBytes         = 5'242'880'000ULL;
        ps.bytesTransferred   = 3'221'225'472ULL;
        ps.retryAttempts      = 2;
        ps.lastErrorCategory  = "network";
        ps.createdAt          = std::chrono::system_clock::now();

        assert(savePendingSessions(dir, {ps}));
        auto loaded = loadPendingSessions(dir);
        assert(loaded.size() == 1);
        assert(loaded[0].sessionId == "pending-1");
        assert(loaded[0].sourcePaths.size() == 2);
        assert(loaded[0].bytesTransferred == 3'221'225'472ULL);
        std::cout << "  [ok] pending-sessions roundtrip\n";
    }

    // 7) Pending sessions corrompu → vide + delete
    {
        const auto p = dir / "pending-sessions.json";
        {
            std::ofstream out(p);
            out << "not valid json {{";
        }
        auto loaded = loadPendingSessions(dir);
        assert(loaded.empty());
        assert(!fs::exists(p));
        std::cout << "  [ok] corrupted pending-sessions deletes + empty\n";
    }

    fs::remove_all(dir);
    std::cout << "test_resume_sidecar OK\n";
    return 0;
}
