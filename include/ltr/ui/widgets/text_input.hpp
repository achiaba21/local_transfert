#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>

namespace ltr::ui {

// Champ de saisie texte minimaliste (focus, caret, filtre de caractères).
// Pensé pour la saisie d'adresses IPv4 mais générique via setAllowedChars().
class TextInput {
public:
    TextInput& setBounds(const sf::FloatRect& r);
    TextInput& setPlaceholder(const std::string& s);
    TextInput& setValue(const std::string& s);
    TextInput& setMaxLength(std::size_t n);
    TextInput& setAllowedChars(const std::string& s);
    TextInput& onSubmit(std::function<void(const std::string&)> cb);

    const std::string& value() const noexcept { return value_; }
    bool hasFocus() const noexcept { return focused_; }
    void clear() noexcept { value_.clear(); }

    void handleEvent(const sf::Event& e);
    void draw(sf::RenderTarget& target) const;

private:
    sf::FloatRect bounds_{};
    std::string   value_;
    std::string   placeholder_;
    std::string   allowedChars_{"0123456789."};
    std::size_t   maxLength_{15}; // "255.255.255.255"
    bool          focused_{false};
    bool          hover_{false};
    std::function<void(const std::string&)> submitCb_;

    mutable sf::Clock caretClock_;
};

} // namespace ltr::ui
