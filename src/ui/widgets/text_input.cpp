#include "ltr/ui/widgets/text_input.hpp"

#include "ltr/ui/rounded_rect.hpp"
#include "ltr/ui/theme.hpp"
#include "ltr/ui/widgets/label.hpp"

namespace ltr::ui {

namespace {
constexpr float kPaddingX   = Spacing::md;
constexpr float kCaretW     = 1.5f;
constexpr int   kBlinkMs    = 500;
} // namespace

TextInput& TextInput::setBounds(const sf::FloatRect& r)        { bounds_ = r;        return *this; }
TextInput& TextInput::setPlaceholder(const std::string& s)     { placeholder_ = s;   return *this; }
TextInput& TextInput::setValue(const std::string& s)           { value_ = s;         return *this; }
TextInput& TextInput::setMaxLength(std::size_t n)              { maxLength_ = n;     return *this; }
TextInput& TextInput::setAllowedChars(const std::string& s)    { allowedChars_ = s;  return *this; }
TextInput& TextInput::onSubmit(std::function<void(const std::string&)> cb) {
    submitCb_ = std::move(cb);
    return *this;
}

void TextInput::handleEvent(const sf::Event& e) {
    if (e.type == sf::Event::MouseMoved) {
        hover_ = bounds_.contains(
            static_cast<float>(e.mouseMove.x),
            static_cast<float>(e.mouseMove.y));
        return;
    }

    if (e.type == sf::Event::MouseButtonReleased &&
        e.mouseButton.button == sf::Mouse::Left) {
        const bool inside = bounds_.contains(
            static_cast<float>(e.mouseButton.x),
            static_cast<float>(e.mouseButton.y));
        focused_ = inside;
        if (focused_) caretClock_.restart();
        return;
    }

    if (!focused_) return;

    if (e.type == sf::Event::KeyPressed) {
        if (e.key.code == sf::Keyboard::BackSpace) {
            if (!value_.empty()) value_.pop_back();
            caretClock_.restart();
        }
        else if (e.key.code == sf::Keyboard::Enter ||
                 e.key.code == sf::Keyboard::Return) {
            if (submitCb_) submitCb_(value_);
        }
        else if (e.key.code == sf::Keyboard::Escape) {
            value_.clear();
            caretClock_.restart();
        }
        return;
    }

    if (e.type == sf::Event::TextEntered) {
        const auto u = e.text.unicode;
        if (u < 32 || u >= 127) return; // ignorer contrôles + non-ASCII
        const char c = static_cast<char>(u);
        if (allowedChars_.find(c) == std::string::npos) return;
        if (value_.size() >= maxLength_) return;
        value_.push_back(c);
        caretClock_.restart();
    }
}

void TextInput::draw(sf::RenderTarget& target) const {
    RoundedRect box(bounds_.left, bounds_.top,
                    bounds_.width, bounds_.height, Radius::md);
    box.setFillColor(Colors::surface);
    box.setOutline(focused_ ? Colors::accent : Colors::separator,
                   focused_ ? 2.f : 1.f);
    box.draw(target);

    const std::string& displayed = value_.empty() ? placeholder_ : value_;
    const sf::Color textColor    = value_.empty() ? Colors::textSecondary
                                                  : Colors::text;

    Label lbl;
    lbl.setText(displayed)
       .setSize(FontSize::body)
       .setColor(textColor);
    const auto m = lbl.measure();
    const float textY = bounds_.top + (bounds_.height - FontSize::body) / 2.f - 2.f;
    lbl.setPosition(bounds_.left + kPaddingX, textY);
    lbl.draw(target);

    // Caret clignotant uniquement sur focus et hors placeholder.
    if (focused_) {
        const int ms = caretClock_.getElapsedTime().asMilliseconds();
        const bool visible = (ms % (2 * kBlinkMs)) < kBlinkMs;
        if (visible) {
            const float caretX = value_.empty()
                ? bounds_.left + kPaddingX
                : bounds_.left + kPaddingX + m.x + 1.f;
            const float caretH = static_cast<float>(FontSize::body);
            const float caretY = bounds_.top + (bounds_.height - caretH) / 2.f;
            RoundedRect caret(caretX, caretY, kCaretW, caretH, 0.5f);
            caret.setFillColor(Colors::accent);
            caret.draw(target);
        }
    }
}

} // namespace ltr::ui
