// V1.2 — Sprint Web P2P
// Vérifie que DisplayName::fromDeviceIdAndUA est déterministe :
//  1. Même deviceId → même name + emoji (idempotence)
//  2. UA différent ne change PAS name/emoji (n'influencent que platformLabel)
//  3. deviceId différent → name+emoji différents (avec haute probabilité)
//  4. UA divers → platformLabel cohérent (iPhone · Safari, etc.)
//  5. deviceId vide → fallback "Visiteur" (ne crash pas)

#include "ltr/web/display_name.hpp"

#include <cassert>
#include <iostream>
#include <set>

int main() {
    using ltr::web::DisplayName;

    // 1. Idempotence
    const auto a1 = DisplayName::fromDeviceIdAndUA(
        "abc-1234", "Mozilla/5.0 (iPhone) Safari/605.1.15");
    const auto a2 = DisplayName::fromDeviceIdAndUA(
        "abc-1234", "Mozilla/5.0 (iPhone) Safari/605.1.15");
    assert(a1.name  == a2.name);
    assert(a1.emoji == a2.emoji);
    assert(!a1.name.empty());
    assert(!a1.emoji.empty());

    // 2. UA différent → même nom (le UA n'influence pas le hash)
    const auto a3 = DisplayName::fromDeviceIdAndUA(
        "abc-1234", "Mozilla/5.0 (Android) Chrome/120.0");
    assert(a3.name  == a1.name);
    assert(a3.emoji == a1.emoji);
    assert(a3.platformLabel != a1.platformLabel);  // mais platform diffère

    // 3. deviceId différent → name probablement différent (3 cas)
    const auto b = DisplayName::fromDeviceIdAndUA(
        "xyz-5678", "Mozilla/5.0 (iPhone) Safari/605.1.15");
    const auto c = DisplayName::fromDeviceIdAndUA(
        "qrs-9012", "Mozilla/5.0 (iPhone) Safari/605.1.15");
    std::set<std::string> names{a1.name, b.name, c.name};
    assert(names.size() >= 2);  // au moins 2 distincts sur 3 (très probable)

    // 4. platformLabel cohérent
    const auto pIphone = DisplayName::fromDeviceIdAndUA(
        "id-1", "Mozilla/5.0 (iPhone; CPU iPhone OS 16) Safari/605.1.15");
    assert(pIphone.platformLabel.find("iPhone") != std::string::npos);
    assert(pIphone.platformLabel.find("Safari") != std::string::npos);

    const auto pAndroid = DisplayName::fromDeviceIdAndUA(
        "id-2", "Mozilla/5.0 (Linux; Android 13) Chrome/120.0");
    assert(pAndroid.platformLabel.find("Android") != std::string::npos);
    assert(pAndroid.platformLabel.find("Chrome") != std::string::npos);

    const auto pMac = DisplayName::fromDeviceIdAndUA(
        "id-3", "Mozilla/5.0 (Macintosh) Firefox/121.0");
    assert(pMac.platformLabel.find("Mac") != std::string::npos);
    assert(pMac.platformLabel.find("Firefox") != std::string::npos);

    // Edge AVANT Chrome dans la détection
    const auto pEdge = DisplayName::fromDeviceIdAndUA(
        "id-4", "Mozilla/5.0 (Windows) Chrome/120.0 Edg/120.0");
    assert(pEdge.platformLabel.find("Edge") != std::string::npos);

    // 5. deviceId vide → fallback safe
    const auto fallback = DisplayName::fromDeviceIdAndUA(
        "", "Mozilla/5.0 (iPhone) Safari/605.1.15");
    assert(fallback.name == "Visiteur");
    assert(!fallback.emoji.empty());

    std::cout << "test_display_name OK\n";
    return 0;
}
