// Vérifie la logique du WebSessionStore (V1.1) :
// authentification PIN + device_id, validation, touch, remove, snapshot.

#include "ltr/web/web_session_store.hpp"

#include <cassert>
#include <filesystem>
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

    // Persistance host par device_id : alias restauré même si le navigateur
    // ne renvoie plus son localStorage.
    {
        const auto p = std::filesystem::temp_directory_path()
                     / "ltr_web_aliases_test.json";
        std::filesystem::remove(p);
        {
            WebSessionStore s1;
            s1.setCustomNamesPath(p);
            auto t = s1.authenticate("472931", "472931",
                                     iphoneDeviceId, iphoneUA, "Bureau");
            assert(t.has_value());
        }
        {
            WebSessionStore s2;
            s2.setCustomNamesPath(p);
            auto t = s2.authenticate("472931", "472931",
                                     iphoneDeviceId, iphoneUA);
            assert(t.has_value());
            auto s = s2.validate(*t);
            assert(s.has_value());
            assert(s->customName == "Bureau");
            assert(s->displayName.find("(Bureau)") != std::string::npos);
        }
        std::filesystem::remove(p);
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

    // Alias utilisateur : le nom auto reste, l'alias apparaît entre parenthèses.
    {
        assert(store.updateCustomName(token, "Serge"));
        auto s = store.validate(token);
        assert(s.has_value());
        assert(s->customName == "Serge");
        assert(s->displayName.find("(Serge)") != std::string::npos);
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
