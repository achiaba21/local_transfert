#include "ltr/ui/icon_library.hpp"

#include "ltr/core/logger.hpp"

// Assets PNG embedded via CMake (EmbedFile.cmake).
#include "ltr/web/assets/icon_check.hpp"
#include "ltr/web/assets/icon_folder.hpp"
#include "ltr/web/assets/icon_close_png.hpp"
#include "ltr/web/assets/icon_arrow_up.hpp"
#include "ltr/web/assets/icon_arrow_down.hpp"
#include "ltr/web/assets/icon_radar.hpp"
#include "ltr/web/assets/icon_no_device.hpp"
#include "ltr/web/assets/icon_qr.hpp"

namespace ltr::ui {

namespace {

// Récupère la string_view des bytes PNG pour un Id donné.
std::string_view iconBytes(IconLibrary::Id id) {
    switch (id) {
        case IconLibrary::Id::Check:     return web::assets::IconCheck;
        case IconLibrary::Id::Folder:    return web::assets::IconFolder;
        case IconLibrary::Id::Close:     return web::assets::IconClose;
        case IconLibrary::Id::ArrowUp:   return web::assets::IconArrowUp;
        case IconLibrary::Id::ArrowDown: return web::assets::IconArrowDown;
        case IconLibrary::Id::Radar:     return web::assets::IconRadar;
        case IconLibrary::Id::NoDevice:  return web::assets::IconNoDevice;
        case IconLibrary::Id::QrCode:    return web::assets::IconQrCode;
    }
    return {};
}

// Cache lazy : une sf::Texture par Id. 7 slots max (taille enum), pas besoin
// d'un map — un tableau statique indexé par l'enum suffit et évite toute
// synchronisation (thread-main only).
constexpr std::size_t kNumIcons = 8;

struct Slot {
    sf::Texture texture;
    bool loaded{false};
};

Slot& slot(IconLibrary::Id id) {
    static Slot slots[kNumIcons];
    return slots[static_cast<std::size_t>(id)];
}

} // namespace

const sf::Texture& IconLibrary::get(Id id) {
    auto& s = slot(id);
    if (s.loaded) return s.texture;

    const auto bytes = iconBytes(id);
    if (!s.texture.loadFromMemory(bytes.data(), bytes.size())) {
        core::log_error("[icons] loadFromMemory failed id="
                        + std::to_string(static_cast<int>(id)));
    } else {
        s.texture.setSmooth(true);
        s.loaded = true;
    }
    return s.texture;
}

} // namespace ltr::ui
