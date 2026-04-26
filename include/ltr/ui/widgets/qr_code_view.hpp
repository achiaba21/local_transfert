#pragma once

#include <string>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>

namespace ltr::ui {

// Widget affichant un QR code dans un rectangle donné. L'image est
// fournie par QrCode::render() (côté web), puis uploadée en texture
// lazy au premier draw. Si le texte change, regénérer et re-setQrImage().
class QrCodeView {
public:
    QrCodeView() = default;

    QrCodeView& setBounds(const sf::FloatRect& r) { bounds_ = r; return *this; }
    QrCodeView& setImage(const sf::Image& img);

    const sf::FloatRect& bounds() const noexcept { return bounds_; }

    void draw(sf::RenderTarget& t) const;

private:
    sf::FloatRect bounds_{};
    sf::Texture   texture_;
    bool          ready_{false};
};

} // namespace ltr::ui
