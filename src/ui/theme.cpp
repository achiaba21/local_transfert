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

// V1.6.6+ : Geist copiée dans <build>/assets/fonts/ par CMake. Recherche
// dans cet ordre :
//   1) "assets/fonts/{name}" relatif au cwd (cas exe lancé depuis build/)
//   2) Inter local si présent (fallback historique)
//   3) Polices système OS
sf::Font loadFontImpl(std::initializer_list<const char*> bundledPaths,
                      std::initializer_list<const char*> systemPaths,
                      const char* logName) {
    sf::Font font;

    for (const auto* p : bundledPaths) {
        if (p && std::filesystem::exists(p) && font.loadFromFile(p)) {
            core::log_info(std::string("[font] ") + logName
                           + " chargé : " + p);
            return font;
        }
    }

    for (const auto* p : systemPaths) {
        if (p && std::filesystem::exists(p) && font.loadFromFile(p)) {
            core::log_warn(std::string("[font] ") + logName
                           + " fallback système : " + p);
            return font;
        }
    }

    core::log_error(std::string("[font] aucune police chargée pour ")
                    + logName);
    return font;
}

sf::Font loadRegular() {
    return loadFontImpl(
        { "assets/fonts/Geist-Regular.ttf",
          "../assets/fonts/Geist-Regular.ttf",
          "assets/fonts/Inter-Regular.ttf",
          "../assets/fonts/Inter-Regular.ttf" },
        {
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
        },
        "Regular");
}

sf::Font loadBold() {
    return loadFontImpl(
        { "assets/fonts/Geist-Bold.ttf",
          "../assets/fonts/Geist-Bold.ttf",
          "assets/fonts/Inter-Bold.ttf",
          "../assets/fonts/Inter-Bold.ttf" },
        {
#ifdef __APPLE__
            "/System/Library/Fonts/HelveticaNeue.ttc",
            "/System/Library/Fonts/Helvetica.ttc",
#elif defined(_WIN32)
            "C:\\Windows\\Fonts\\segoeuib.ttf",
            "C:\\Windows\\Fonts\\arialbd.ttf",
#else
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
#endif
        },
        "Bold");
}

} // namespace

const sf::Font& theme_font() {
    static sf::Font font = loadRegular();
    return font;
}

const sf::Font& theme_font_bold() {
    static sf::Font font = loadBold();
    return font;
}

} // namespace ltr::ui
