// Vérifie le DownloadTicketStore : issue, get (rejouable V1.1.8), peek,
// issueStreamingZip.

#include "ltr/web/download_ticket_store.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

int main() {
    using ltr::web::DownloadTicketStore;
    using ltr::web::TicketKind;
    using ltr::web::ZipEntry;

    DownloadTicketStore store;

    const std::filesystem::path fakePath = "/tmp/fake.txt";
    const auto id1 = store.issue("token-abc", "session-1", fakePath,
                                 "fake.txt", 1234);
    assert(!id1.empty());
    assert(id1.size() == 32);

    // Peek : ticket présent
    {
        auto peek = store.peek(id1);
        assert(peek.has_value());
        assert(peek->sessionToken == "token-abc");
        assert(peek->size == 1234);
        assert(peek->kind == TicketKind::File);
    }

    // V1.1.8 : get est non-destructif → rejouable pendant le TTL.
    {
        auto t = store.get(id1);
        assert(t.has_value());
        assert(t->displayName == "fake.txt");
    }
    {
        auto t = store.get(id1);
        assert(t.has_value()); // encore là
        assert(t->displayName == "fake.txt");
    }
    {
        auto p = store.peek(id1);
        assert(p.has_value()); // peek reste OK aussi
    }

    // Ticket inconnu → nullopt
    assert(!store.get("deadbeefdeadbeefdeadbeefdeadbeef").has_value());

    // issueStreamingZip : ticket kind=StreamingZip avec entrées snapshot.
    {
        std::vector<ZipEntry> entries;
        ZipEntry e;
        e.abs = "/tmp/a.txt";
        e.relInZip = "bundle/a.txt";
        e.size = 100;
        entries.push_back(e);
        const auto zipId = store.issueStreamingZip(
            "token-abc", "session-2", std::move(entries), "bundle.zip", 999);
        auto t = store.get(zipId);
        assert(t.has_value());
        assert(t->kind == TicketKind::StreamingZip);
        assert(t->zipEntries.size() == 1);
        assert(t->zipEntries[0].relInZip == "bundle/a.txt");
        assert(t->size == 999);
    }

    std::cout << "test_download_ticket OK\n";
    return 0;
}
