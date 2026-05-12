#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <cstdint>
#include <functional>
#include <string>

namespace ltr::ui {

// Card affichant un lien de dépôt côté desktop host. Suit le pattern du
// thème (RoundedRect via Card existant, Colors, Spacing, Radius, FontSize).
//
// 3 zones interactives :
//   - bouton "Copier l'URL"
//   - bouton "QR"
//   - bouton "Révoquer"
class DepositLinkCard {
public:
    struct Model {
        std::string id;
        std::string label;
        std::string url;
        std::string statusBadge;  // "Actif", "Révoqué", "Expiré"
        sf::Color   badgeColor;
        std::string subline;       // "3 dépôts reçus · 84 Mo"
        bool        active{true};
    };

    DepositLinkCard() = default;

    DepositLinkCard& setBounds(const sf::FloatRect& r) {
        bounds_ = r; return *this;
    }
    DepositLinkCard& setModel(Model m) { model_ = std::move(m); return *this; }

    using Callback = std::function<void()>;
    DepositLinkCard& onCopy(Callback cb)   { onCopy_   = std::move(cb); return *this; }
    DepositLinkCard& onQr(Callback cb)     { onQr_     = std::move(cb); return *this; }
    DepositLinkCard& onRevoke(Callback cb) { onRevoke_ = std::move(cb); return *this; }

    const sf::FloatRect& bounds() const noexcept { return bounds_; }
    const Model&         model()  const noexcept { return model_; }

    // Renvoie true si l'évènement a été consommé.
    bool handleClick(sf::Vector2f pos);

    void draw(sf::RenderTarget& target) const;

private:
    sf::FloatRect copyBtnRect() const;
    sf::FloatRect qrBtnRect() const;
    sf::FloatRect revokeBtnRect() const;

    sf::FloatRect bounds_{};
    Model         model_;
    Callback      onCopy_;
    Callback      onQr_;
    Callback      onRevoke_;
};

} // namespace ltr::ui
