#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include <SFML/System/Vector2.hpp>

namespace ltr::ui {

// Cache global des mesures de Label. Évite de recréer un sf::Text à
// chaque measure() call (91+ Label par frame × 60 FPS = ~5500 mesures
// par seconde, sinon réallouent inutilement).
//
// Clé : (text, sizePx, bold). La measureClamped (avec maxWidth) n'est
// PAS cachée ici — elle dépend du résultat brut + d'un binary search.
//
// Cache vidé à chaque changement DPI ou changement de police.
class LabelCache {
public:
    struct Key {
        std::string text;
        unsigned    sizePx{0};
        bool        bold{false};

        bool operator==(const Key& o) const noexcept {
            return text == o.text && sizePx == o.sizePx && bold == o.bold;
        }
    };

    struct Hash {
        std::size_t operator()(const Key& k) const noexcept {
            std::size_t h = std::hash<std::string>{}(k.text);
            h ^= std::hash<unsigned>{}(k.sizePx) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<bool>{}(k.bold)      + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    // Lookup ou calcul (et insertion). Calcul réel délégué au caller via
    // un foncteur — évite la dépendance circulaire avec Label.
    template <typename ComputeFn>
    static sf::Vector2f getOrCompute(const Key& key, ComputeFn fn) {
        auto& m = map();
        const auto it = m.find(key);
        if (it != m.end()) return it->second;
        const auto v = fn();
        m.emplace(key, v);
        return v;
    }

    static void clear() { map().clear(); }
    static std::size_t size() { return map().size(); }

private:
    static std::unordered_map<Key, sf::Vector2f, Hash>& map() {
        static std::unordered_map<Key, sf::Vector2f, Hash> m;
        return m;
    }
};

} // namespace ltr::ui
