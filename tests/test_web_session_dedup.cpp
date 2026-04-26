// Vérifie la dédup V1.1 : deux authentifications successives avec le même
// device_id créent des tokens DIFFÉRENTS mais UN SEUL Device actif dans le
// store (l'ancien token est invalidé).

#include "ltr/web/web_session_store.hpp"

#include <cassert>
#include <iostream>

int main() {
    using ltr::web::WebSessionStore;

    WebSessionStore store;

    const std::string pin = "123456";
    const std::string device = "device-abc-stable";
    const std::string ua1 = "Mozilla/5.0 (Android) Chrome/120.0";
    const std::string ua2 = "Mozilla/5.0 (Android) Chrome/120.0";

    // 1re auth
    auto t1 = store.authenticate(pin, pin, device, ua1);
    assert(t1.has_value());

    // Vérifie que t1 est valide
    assert(store.validate(*t1).has_value());
    assert(store.snapshot().size() == 1);

    // 2e auth avec le MÊME device_id → nouveau token, ancien invalidé
    auto t2 = store.authenticate(pin, pin, device, ua2);
    assert(t2.has_value());
    assert(*t1 != *t2);

    // t1 n'est plus valide, t2 l'est
    assert(!store.validate(*t1).has_value());
    assert(store.validate(*t2).has_value());

    // UN SEUL device actif (pas de duplication)
    auto snap = store.snapshot();
    assert(snap.size() == 1);
    assert(snap[0].deviceId == device);
    assert(snap[0].token == *t2);

    // Une 3e auth avec un AUTRE device_id → 2 sessions actives
    auto t3 = store.authenticate(pin, pin, "device-xyz-other", ua1);
    assert(t3.has_value());
    assert(store.snapshot().size() == 2);

    // removeByDeviceId supprime la bonne session
    store.removeByDeviceId(device);
    assert(!store.validate(*t2).has_value());
    assert(store.validate(*t3).has_value());
    assert(store.snapshot().size() == 1);

    std::cout << "test_web_session_dedup OK\n";
    return 0;
}
