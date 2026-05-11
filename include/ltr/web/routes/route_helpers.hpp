#pragma once

#include <string>

namespace httplib { struct Request; }

namespace ltr::web::routes {

// Extrait la valeur du cookie "ltr_token" depuis le header "Cookie" d'une
// requête HTTP. Retourne une chaîne vide si absent. Utilisé par toutes
// les routes authentifiées pour éviter la duplication.
std::string readTokenCookie(const httplib::Request& req);

// V1.6.5 — Sprint Stabilité (Wave 3 item H).
// Lit le cookie persistent `ltr_remember` (HMAC token 30j).
std::string readRememberCookie(const httplib::Request& req);

} // namespace ltr::web::routes
