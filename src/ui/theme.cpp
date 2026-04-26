#include "ltr/ui/theme.hpp"

#include "ltr/core/logger.hpp"

#include <array>
#include <filesystem>

namespace ltr::ui {

const sf::Color Colors::bg            {250, 250, 251};
const sf::Color Colors::surface       {255, 255, 255};
const sf::Color Colors::sidebar       {241, 243, 246};
const sf::Color Colors::accent        { 99, 102, 241};
const sf::Color Colors::accentHover   { 79,  70, 229};
const sf::Color Colors::accentLight   {238, 242, 255};
const sf::Color Colors::text          { 15,  23,  42};
const sf::Color Colors::textSecondary {100, 116, 139};
const sf::Color Colors::separator     {226, 232, 240};
const sf::Color Colors::success       { 16, 185, 129};
const sf::Color Colors::error         {239,  68,  68};
const sf::Color Colors::warning       {245, 158,  11};
const sf::Color Colors::overlay       {  0,   0,   0, 140};
const sf::Color Colors::shadow        {  0,   0,   0,  25};

namespace {

sf::Font loadFontImpl() {
    sf::Font font;

    // 1) Police fournie dans assets/
    const std::array<const char*, 2> bundled = {
        "assets/fonts/Inter-Regular.ttf",
        "assets/fonts/Inter-Bold.ttf",
    };
    for (const auto* p : bundled) {
        if (std::filesystem::exists(p) && font.loadFromFile(p)) return font;
    }

    // 2) Polices système courantes.
    const std::array<const char*, 6> sys = {
#ifdef __APPLE__
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/Library/Fonts/Arial.ttf",
#elif defined(_WIN32)
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\tahoma.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
#endif
        nullptr, nullptr, nullptr,
    };
    for (const auto* p : sys) {
        if (p && std::filesystem::exists(p) && font.loadFromFile(p)) return font;
    }

    core::log_error("Impossible de charger une police. "
                    "Placez Inter-Regular.ttf dans assets/fonts/");
    return font;
}

} // namespace

const sf::Font& theme_font() {
    static sf::Font font = loadFontImpl();
    return font;
}

} // namespace ltr::ui
