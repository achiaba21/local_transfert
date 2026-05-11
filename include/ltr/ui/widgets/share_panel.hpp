#pragma once

#include <functional>
#include <string>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>

#include "ltr/ui/widgets/button.hpp"
#include "ltr/ui/widgets/qr_code_view.hpp"

namespace ltr::ui {

// Card à droite du MainScreen. Deux modes (V1.1.8-UX4) :
//   - Expanded (240 px) : QR 220 + URL + 2 boutons Copier + PIN 44
//   - Collapsed (40 px) : rail avec icône QR + badge visiteurs web
//
// Le toggle entre les 2 modes se fait via un clic :
//   - Expanded : clic sur la croix coin haut-droit
//   - Collapsed : clic n'importe où sur le rail
// Le callback `onToggle` doit router vers AppController::toggleSharePanel.
class SharePanel {
public:
    SharePanel();

    SharePanel& setBounds(const sf::FloatRect& r);
    SharePanel& setUrl(const std::string& url);      // "http://ip:port"
    SharePanel& setPin(const std::string& pin6);     // "472931"
    // V1.6.4 — empreinte SHA-256 cert HTTPS, format "AB:CD:..." (32 octets).
    // Vide → la section empreinte n'est pas dessinée (HTTP plain).
    SharePanel& setFingerprint(const std::string& fp);
    SharePanel& setQrImage(const sf::Image& img);

    // V1.1.8-UX4
    SharePanel& setCollapsed(bool c);
    SharePanel& setVisitorCount(int n);
    SharePanel& onToggle(std::function<void()> cb);

    const sf::FloatRect& bounds() const noexcept { return bounds_; }
    bool collapsed() const noexcept { return collapsed_; }

    void handleEvent(const sf::Event& e);
    void draw(sf::RenderTarget& target) const;

private:
    void layoutChildren();

    // Hit areas
    sf::FloatRect collapseBtnRect() const;  // croix haut-droit (expanded)

    // Rendus dédiés
    void drawExpanded(sf::RenderTarget& t) const;
    void drawCollapsed(sf::RenderTarget& t) const;

    sf::FloatRect bounds_{};
    std::string   url_;
    std::string   pin_;
    std::string   fingerprint_;     // V1.6.4

    QrCodeView        qr_;
    mutable Button    copyUrlBtn_;
    mutable Button    copyPinBtn_;
    mutable Button    copyFpBtn_;   // V1.6.4
    mutable float     copiedUrlUntil_{0.f};
    mutable float     copiedPinUntil_{0.f};
    mutable float     copiedFpUntil_{0.f};  // V1.6.4

    bool              collapsed_{false};
    int               visitorCount_{0};
    std::function<void()> toggleCb_;
};

} // namespace ltr::ui
