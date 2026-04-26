// Sprint UI Layout System — tests Label::measure + ellipsis.
//
// Note : on ne peut pas mesurer le rendu réel sans contexte SFML
// (la police charge nécessite d'avoir un context valide). On teste
// surtout le comportement déterministe des helpers + le LabelCache.

#include "ltr/ui/label_cache.hpp"

#include <cassert>
#include <iostream>

int main() {
    using namespace ltr::ui;

    // Cache vide au démarrage
    LabelCache::clear();
    assert(LabelCache::size() == 0);

    // getOrCompute insère
    int compute_count = 0;
    const sf::Vector2f r1 = LabelCache::getOrCompute(
        LabelCache::Key{"hello", 14, false},
        [&]{ ++compute_count; return sf::Vector2f{42.f, 18.f}; });
    assert(r1.x == 42.f && r1.y == 18.f);
    assert(compute_count == 1);
    assert(LabelCache::size() == 1);

    // getOrCompute hit cache : ne ré-appelle pas le compute
    const sf::Vector2f r2 = LabelCache::getOrCompute(
        LabelCache::Key{"hello", 14, false},
        [&]{ ++compute_count; return sf::Vector2f{0.f, 0.f}; });
    assert(r2.x == 42.f && r2.y == 18.f);
    assert(compute_count == 1);  // pas appelé à nouveau
    assert(LabelCache::size() == 1);

    // Différentes clés → 2 entrées
    LabelCache::getOrCompute(
        LabelCache::Key{"hello", 14, true},   // bold différent
        [&]{ return sf::Vector2f{45.f, 18.f}; });
    LabelCache::getOrCompute(
        LabelCache::Key{"world", 14, false},
        [&]{ return sf::Vector2f{52.f, 18.f}; });
    assert(LabelCache::size() == 3);

    // clear vide tout
    LabelCache::clear();
    assert(LabelCache::size() == 0);

    std::cout << "test_label_ellipsis OK (cache uniquement)\n";
    return 0;
}
