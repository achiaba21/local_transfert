#include "ltr/web/display_name.hpp"

#include <array>
#include <cstdint>

namespace ltr::web {

namespace {

constexpr std::array<const char*, 30> kAdjectives = {
    "Bleu", "Rouge", "Vert", "Rapide", "Sage", "Fier", "Calme", "Vif",
    "Doux", "Léger", "Brave", "Joyeux", "Fin", "Souple", "Subtil",
    "Lumineux", "Tranquille", "Clair", "Frais", "Tendre", "Hardi",
    "Curieux", "Espiègle", "Malin", "Agile", "Robuste", "Élégant",
    "Discret", "Poli", "Solide"
};

struct Animal {
    const char* name;
    const char* emoji;  // UTF-8 bytes
};

constexpr std::array<Animal, 30> kAnimals = {{
    {"Pingouin",   "\xF0\x9F\x90\xA7"},  // 🐧
    {"Renard",     "\xF0\x9F\xA6\x8A"},  // 🦊
    {"Lapin",      "\xF0\x9F\x90\xB0"},  // 🐰
    {"Loup",       "\xF0\x9F\x90\xBA"},  // 🐺
    {"Tigre",      "\xF0\x9F\x90\xAF"},  // 🐯
    {"Lion",       "\xF0\x9F\xA6\x81"},  // 🦁
    {"Ours",       "\xF0\x9F\x90\xBB"},  // 🐻
    {"Panda",      "\xF0\x9F\x90\xBC"},  // 🐼
    {"Koala",      "\xF0\x9F\x90\xA8"},  // 🐨
    {"Singe",      "\xF0\x9F\x90\xB5"},  // 🐵
    {"Cerf",       "\xF0\x9F\xA6\x8C"},  // 🦌
    {"Cheval",     "\xF0\x9F\x90\xB4"},  // 🐴
    {"Licorne",    "\xF0\x9F\xA6\x84"},  // 🦄
    {"Dauphin",    "\xF0\x9F\x90\xAC"},  // 🐬
    {"Pieuvre",    "\xF0\x9F\x90\x99"},  // 🐙
    {"Crabe",      "\xF0\x9F\xA6\x80"},  // 🦀
    {"Tortue",     "\xF0\x9F\x90\xA2"},  // 🐢
    {"Chouette",   "\xF0\x9F\xA6\x89"},  // 🦉
    {"Aigle",      "\xF0\x9F\xA6\x85"},  // 🦅
    {"Cygne",      "\xF0\x9F\xA6\xA2"},  // 🦢
    {"Papillon",   "\xF0\x9F\xA6\x8B"},  // 🦋
    {"Coccinelle", "\xF0\x9F\x90\x9E"},  // 🐞
    {"Abeille",    "\xF0\x9F\x90\x9D"},  // 🐝
    {"Escargot",   "\xF0\x9F\x90\x8C"},  // 🐌
    {"Hérisson",   "\xF0\x9F\xA6\x94"},  // 🦔
    {"Écureuil",   "\xF0\x9F\x90\xBF"},  // 🐿
    {"Lézard",     "\xF0\x9F\xA6\x8E"},  // 🦎
    {"Crocodile",  "\xF0\x9F\x90\x8A"},  // 🐊
    {"Phoque",     "\xF0\x9F\xA6\xAD"},  // 🦭
    {"Canard",     "\xF0\x9F\xA6\x86"},  // 🦆
}};

constexpr std::uint64_t fnv1a(std::string_view s) noexcept {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (char c : s) {
        h ^= static_cast<std::uint8_t>(c);
        h *= 0x100000001b3ULL;
    }
    return h;
}

bool contains(std::string_view ua, const char* needle) {
    return ua.find(needle) != std::string_view::npos;
}

std::string deriveOs(std::string_view ua) {
    if (contains(ua, "iPhone"))                          return "iPhone";
    if (contains(ua, "iPad"))                            return "iPad";
    if (contains(ua, "Android"))                         return "Android";
    if (contains(ua, "Mac OS X") || contains(ua, "Macintosh")) return "Mac";
    if (contains(ua, "Windows"))                         return "Windows";
    if (contains(ua, "Linux"))                           return "Linux";
    return "Web";
}

std::string deriveBrowser(std::string_view ua) {
    // Edge AVANT Chrome (Edge contient "Chrome" et "Safari").
    if (contains(ua, "Edg/") || contains(ua, "Edge/"))  return "Edge";
    if (contains(ua, "Firefox/"))                        return "Firefox";
    if (contains(ua, "Chrome/"))                         return "Chrome";
    if (contains(ua, "Safari/"))                         return "Safari";
    return "Browser";
}

} // namespace

DisplayName::Result DisplayName::fromDeviceIdAndUA(
    std::string_view deviceId, std::string_view userAgent) {

    Result r;

    if (deviceId.empty()) {
        // Fallback sûr — ne devrait jamais arriver (auth_routes
        // garantit deviceId non-vide via makeServerDeviceId()).
        r.name          = "Visiteur";
        r.emoji         = "\xF0\x9F\x91\xA4";  // 👤
        r.platformLabel.clear();
        return r;
    }

    const auto h      = fnv1a(deviceId);
    const auto aniIdx = static_cast<std::size_t>(h % kAnimals.size());
    const auto adjIdx = static_cast<std::size_t>(
        (h / kAnimals.size()) % kAdjectives.size());

    const auto& ani = kAnimals[aniIdx];
    r.emoji = ani.emoji;
    r.name  = std::string(ani.name) + " " + kAdjectives[adjIdx];

    r.platformLabel = deriveOs(userAgent) + " \xC2\xB7 "  // " · "
                    + deriveBrowser(userAgent);
    return r;
}

} // namespace ltr::web
