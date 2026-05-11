// V1.6.5 — Sprint Stabilité (Wave 3 item H).
// Tests du token persistent HMAC SHA-256 (cookie ltr_remember).

#include "ltr/web/web_session_store.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    using ltr::web::WebSessionStore;

    constexpr std::int64_t kFutureSec = 9999999999;  // ~2286
    constexpr std::int64_t kPastSec   = 1000000000;  // 2001

    // Helper : returns now+1h epoch seconds.
    auto futureExp = []() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()
            + 3600;
    };

    // 1. Roundtrip basique : token généré valide pour le PIN actuel.
    {
        WebSessionStore s;
        s.setHmacSecret("MY_FINGERPRINT_SHA256");
        const auto exp = futureExp();
        const auto tok = s.makePersistentToken("device-A", "123456", exp);
        const auto got = s.verifyPersistentToken(tok, "123456");
        assert(got.has_value());
        assert(*got == "device-A");
    }

    // 2. PIN différent → invalide (cas redémarrage host avec nouveau PIN).
    {
        WebSessionStore s;
        s.setHmacSecret("FP1");
        const auto tok = s.makePersistentToken("dev-B", "111111", futureExp());
        const auto got = s.verifyPersistentToken(tok, "999999");
        assert(!got.has_value());
    }

    // 3. Secret HMAC différent → invalide (cas régénération cert HTTPS).
    {
        WebSessionStore s1;
        s1.setHmacSecret("FP_OLD");
        const auto tok = s1.makePersistentToken("dev-C", "222222", futureExp());

        WebSessionStore s2;
        s2.setHmacSecret("FP_NEW");
        const auto got = s2.verifyPersistentToken(tok, "222222");
        assert(!got.has_value());
    }

    // 4. Token expiré (exp dans le passé) → invalide.
    {
        WebSessionStore s;
        s.setHmacSecret("FP");
        const auto tok = s.makePersistentToken("dev-D", "333333", kPastSec);
        const auto got = s.verifyPersistentToken(tok, "333333");
        assert(!got.has_value());
    }

    // 5. Token tampered (1 char modifié dans HMAC) → invalide.
    {
        WebSessionStore s;
        s.setHmacSecret("FP");
        auto tok = s.makePersistentToken("dev-E", "444444", futureExp());
        // Modifie le dernier char du HMAC.
        tok.back() = (tok.back() == 'a') ? 'b' : 'a';
        const auto got = s.verifyPersistentToken(tok, "444444");
        assert(!got.has_value());
    }

    // 6. Token vide / mal formé → invalide (pas de crash).
    {
        WebSessionStore s;
        s.setHmacSecret("FP");
        assert(!s.verifyPersistentToken("", "111111").has_value());
        assert(!s.verifyPersistentToken("foo.bar.baz", "111111").has_value());
        assert(!s.verifyPersistentToken("a.b.c.d.e", "111111").has_value());
    }

    // 7. Différents deviceId → tokens différents.
    {
        WebSessionStore s;
        s.setHmacSecret("FP");
        const auto exp = futureExp();
        const auto t1 = s.makePersistentToken("dev-1", "555555", exp);
        const auto t2 = s.makePersistentToken("dev-2", "555555", exp);
        assert(t1 != t2);
    }

    // 8. Même deviceId + même PIN + même secret + même exp → token déterministe.
    {
        WebSessionStore s;
        s.setHmacSecret("FP");
        const auto t1 = s.makePersistentToken("dev-9", "666666", kFutureSec);
        const auto t2 = s.makePersistentToken("dev-9", "666666", kFutureSec);
        assert(t1 == t2);
    }

    // 9. Format : 4 segments séparés par '.', dernier = 64 hex chars (HMAC).
    {
        WebSessionStore s;
        s.setHmacSecret("FP");
        const auto tok = s.makePersistentToken("dev-X", "777777", kFutureSec);
        int dots = 0;
        for (char c : tok) if (c == '.') ++dots;
        assert(dots == 3);
        const auto lastDot = tok.find_last_of('.');
        const auto hmacPart = tok.substr(lastDot + 1);
        assert(hmacPart.size() == 64);
    }

    std::cout << "test_persistent_token: OK\n";
    return 0;
}
