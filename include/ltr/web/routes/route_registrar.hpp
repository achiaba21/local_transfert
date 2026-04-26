#pragma once

namespace ltr::web {
class WebService;
}

namespace ltr::web::routes {

// Point d'entrée unique d'enregistrement des routes HTTP. Appelé depuis
// WebService::start() avant le bind-to-port. Chaque fichier routes/*.cpp
// contribue son sous-ensemble via des fonctions register* libres.
void registerAll(WebService& svc);

} // namespace ltr::web::routes
