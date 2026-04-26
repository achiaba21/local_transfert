#pragma once

#include <functional>
#include <string>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Window/Event.hpp>

#include "ltr/domain/device.hpp"

namespace ltr::ui {

class DeviceListItem {
public:
    DeviceListItem& setBounds(const sf::FloatRect& r) { bounds_ = r; return *this; }
    DeviceListItem& setDevice(const domain::Device& d) { device_ = d; return *this; }
    DeviceListItem& setSelected(bool b) { selected_ = b; return *this; }
    DeviceListItem& onClick(std::function<void()> cb) { cb_ = std::move(cb); return *this; }

    const sf::FloatRect& bounds() const noexcept { return bounds_; }

    void handleEvent(const sf::Event& e);
    void draw(sf::RenderTarget& target) const;

private:
    sf::FloatRect bounds_{};
    domain::Device device_;
    bool selected_{false};
    bool hover_{false};
    std::function<void()> cb_;
};

} // namespace ltr::ui
