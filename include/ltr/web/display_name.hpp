#pragma once

#include <string>
#include <string_view>

namespace ltr::web {

// V1.2 — Sprint Web P2P
// Génère un nom lisible et stable pour un device web à partir de son
// deviceId (UUID localStorage) et du User-Agent HTTP.
//
// Le nom est dérivé d'un hash FNV-1a du deviceId → 1 adjectif + 1 animal
// FR + emoji. Même deviceId → toujours même nom (sur 2 sessions
// différentes, navigateurs identiques, etc.).
//
// platformLabel est dérivé du User-Agent : "iPhone · Safari", "Android ·
// Chrome", etc. Sert de sous-titre d'affichage UI.
struct DisplayName {
    struct Result {
        std::string name;           // ex. "Pingouin Bleu"
        std::string emoji;          // ex. "🐧"
        std::string platformLabel;  // ex. "iPhone · Safari"
    };

    static Result fromDeviceIdAndUA(std::string_view deviceId,
                                    std::string_view userAgent);
};

} // namespace ltr::web
