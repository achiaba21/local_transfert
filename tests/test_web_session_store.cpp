// Vérifie la logique du WebSessionStore (V1.1) :
// authentification PIN + device_id, validation, touch, remove, snapshot.

#include "ltr/web/web_session_store.hpp"

#include <cassert>
#include <iostream>

int main() {
    using ltr::web::WebSessionStore;

    WebSessionStore store;

    const std::string iphoneUA =
        "Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) Safari/17.0";
    const std::string iphoneDeviceId = "11111111-aaaa-4bbb-8ccc-222222222222";

    // Mauvais PIN → nullopt
    {
        auto res = store.authenticate("111111", "472931",
                                      iphoneDeviceId, iphoneUA);
        assert(!res.has_value());
    }

    // device_id vide → nullopt
    {
        auto res = store.authenticate("472931", "472931", "", iphoneUA);
        assert(!res.has_value());
    }

    // Bon PIN + device_id → token non vide + session créée
    std::string token;
    {
        auto res = store.authenticate("472931", "472931",
                                      iphoneDeviceId, iphoneUA);
        assert(res.has_value());
        token = *res;
        assert(!token.empty());
        assert(token.size() == 32);
    }

    // Validate OK, le Device.id == deviceId (stable, V1.1)
    {
        auto s = store.validate(token);
        assert(s.has_value());
        assert(s->deviceId == iphoneDeviceId);
        assert(s->device.id == iphoneDeviceId);
        assert(s->device.kind == ltr::domain::PeerKind::Web);
        assert(s->device.platform == "iOS");
        assert(s->device.name.find("iOS") != std::string::npos);
        assert(s->device.sessionToken == token);
    }

    // Validate avec mauvais token → nullopt
    {
        auto s = store.validate("deadbeef");
        assert(!s.has_value());
    }

    // Snapshot contient 1 session
    {
        auto snap = store.snapshot();
        assert(snap.size() == 1);
    }

    // removeByToken → plus là
    {
        store.removeByToken(token);
        auto s = store.validate(token);
        assert(!s.has_value());
        assert(store.snapshot().empty());
    }

    std::cout << "test_web_session_store OK\n";
    return 0;
}
