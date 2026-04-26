#pragma once

#include <string>
#include <SFML/System/String.hpp>

namespace ltr::ui {

// SFML 2.6 interprète `std::string` comme Latin-1 par défaut.
// Cette fonction convertit un `std::string` UTF-8 en `sf::String`
// correctement encodé pour le rendu.
inline sf::String utf8(const std::string& s) {
    return sf::String::fromUtf8(s.begin(), s.end());
}

inline sf::String utf8(const char* s) {
    const std::string str(s);
    return sf::String::fromUtf8(str.begin(), str.end());
}

} // namespace ltr::ui
