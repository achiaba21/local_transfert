#pragma once

#include <chrono>
#include <string>

#include "ltr/domain/device.hpp"

namespace ltr::web {

// Session navigateur authentifiée par PIN. Matérialisée dans la liste des
// pairs desktop via un domain::Device{kind=Web}.
//
// V1.1 : séparation claire entre identité stable (deviceId, fourni par le
// navigateur via localStorage) et session éphémère (token, cookie qui meurt
// à la fermeture d'onglet). Permet d'éviter la duplication dans la liste
// desktop quand un même navigateur re-authentifie.
struct WebSession {
    std::string token;        // 32 hex chars — identifiant serveur (éphémère)
    std::string deviceId;     // UUID stable du navigateur (localStorage)
    std::string userAgent;    // User-Agent brut (pour parsing name/platform)
    domain::Device device;    // Device construit à l'auth (kind=Web, id=deviceId)
    std::chrono::steady_clock::time_point lastSeen{};
};

} // namespace ltr::web
