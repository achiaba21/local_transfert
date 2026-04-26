#pragma once

#include <functional>
#include <string>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Window/Event.hpp>

namespace ltr::ui {

class Button {
public:
    enum class Variant { Primary, Secondary, Danger };

    Button();

    Button& setBounds(const sf::FloatRect& r);
    Button& setLabel(const std::string& s);
    Button& setVariant(Variant v);
    Button& setEnabled(bool b);
    Button& onClick(std::function<void()> cb);

    bool enabled() const noexcept { return enabled_; }
    const sf::FloatRect& bounds() const noexcept { return bounds_; }

    void handleEvent(const sf::Event& e);
    void draw(sf::RenderTarget& target) const;

private:
    sf::FloatRect bounds_{};
    std::string   label_;
    Variant       variant_{Variant::Primary};
    bool          enabled_{true};
    bool          hover_{false};
    bool          pressed_{false};
    std::function<void()> cb_;
};

} // namespace ltr::ui
