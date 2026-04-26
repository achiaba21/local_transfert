#pragma once

#include <functional>
#include <vector>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

namespace ltr::ui {

// Mini-DSL de layout type SwiftUI/Flutter pour SFML.
//
// HBox/VBox = conteneur qui dispose ses enfants en ligne / colonne avec
// spacing + padding. Chaque enfant est soit `fixed(size, drawFn)` (taille
// fixe en pixels) soit `expanded(weight, drawFn)` (prend une fraction de
// l'espace restant proportionnelle au poids).
//
// Usage :
//   HBox{}
//       .padding(16.f)
//       .spacing(8.f)
//       .fixed(20.f,  [&](auto& t, const auto& r){ drawIcon(t, r); })
//       .expanded(1,  [&](auto& t, const auto& r){ drawTitle(t, r); })
//       .fixed(80.f,  [&](auto& t, const auto& r){ drawButton(t, r); })
//       .layout(parentRect, target);
//
// Sprint UI Layout System.

class Box {
public:
    enum class Direction { Horizontal, Vertical };

    using DrawFn = std::function<
        void(sf::RenderTarget&, const sf::FloatRect&)>;

    // Configuration
    Box& spacing(float s);
    Box& padding(float p);
    Box& padding(float horizontal, float vertical);

    // Enfant taille fixe en pixels (sur l'axe principal)
    Box& fixed(float size, DrawFn draw);
    // Enfant qui prend `weight / totalWeight` de l'espace restant
    Box& expanded(int weight, DrawFn draw);
    // Espace vide fixe (pas de dessin)
    Box& spacer(float size);

    // Calcule la layout puis appelle les `DrawFn` dans l'ordre.
    void layout(const sf::FloatRect& parent, sf::RenderTarget& target);

    // Variante : retourne les bounds calculés sans dessiner. Utile pour
    // pré-calculer des hit areas avant le dessin.
    std::vector<sf::FloatRect> computeBounds(
        const sf::FloatRect& parent) const;

protected:
    struct Child {
        bool   fixed{false};
        float  fixedSize{0.f};
        int    weight{1};
        DrawFn draw;
    };

    Direction          direction_{Direction::Horizontal};
    float              spacing_{0.f};
    float              padH_{0.f};
    float              padV_{0.f};
    std::vector<Child> children_;
};

class HBox : public Box {
public:
    HBox() { direction_ = Direction::Horizontal; }
};

class VBox : public Box {
public:
    VBox() { direction_ = Direction::Vertical; }
};

} // namespace ltr::ui
