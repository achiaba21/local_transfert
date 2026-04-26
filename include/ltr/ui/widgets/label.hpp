#pragma once

#include <string>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>

namespace ltr::ui {

class Label {
public:
    enum class Alignment { Left, Center, Right };

    Label();

    // Existant
    Label& setText(const std::string& s);
    Label& setPosition(float x, float y);
    Label& setSize(unsigned pts);
    Label& setColor(sf::Color c);
    Label& setBold(bool b);

    // V2 — Sprint UI Layout System
    // Largeur max (0 = pas de contrainte). Si setEllipsis(true) et le
    // texte naturel dépasse maxWidth, le texte est tronqué + "…".
    Label& setMaxWidth(float w);
    Label& setEllipsis(bool on);
    Label& setAlignment(Alignment a);
    // Raccourci : positionne le label DANS un rect, applique maxWidth,
    // ajuste la position selon l'alignement courant.
    Label& setBounds(const sf::FloatRect& r);

    // Taille naturelle (sans contrainte de maxWidth). Utilise le cache.
    sf::Vector2f measure() const;
    // Taille rendue (avec ellipsis appliqué si maxWidth dépassé).
    sf::Vector2f measureClamped() const;

    void draw(sf::RenderTarget& t) const;

private:
    // Texte effectivement rendu (avec ellipsis si nécessaire).
    std::string renderedText() const;

    std::string  text_;
    sf::Vector2f pos_{0, 0};
    unsigned     size_{13};
    sf::Color    color_{0, 0, 0};
    bool         bold_{false};
    float        maxWidth_{0.f};   // 0 = no constraint
    bool         ellipsis_{true};
    Alignment    align_{Alignment::Left};
    bool         hasBounds_{false};
    sf::FloatRect bounds_{};
};

} // namespace ltr::ui
