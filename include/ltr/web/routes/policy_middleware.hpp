#pragma once

namespace httplib { struct Request; struct Response; }

namespace ltr::web { class WebService; }

namespace ltr::web::routes {

// Phase 3 — Contrôle IT.
// Middlewares minces utilisés EN TÊTE des handlers concernés.
// Convention : retournent true si la réponse est ÉCRITE (= route doit
// stopper) ; retournent false sinon (passe-plat).

// Refuse 403 si network.allowP2P=false.
bool rejectIfP2PDisabled(WebService& svc,
                         const httplib::Request& req,
                         httplib::Response& res);

// Refuse 403 si l'IP cliente n'est pas dans la whitelist.
// `localhost` est toujours autorisé. `/health` n'est jamais filtré.
bool rejectIfIpBlocked(WebService& svc,
                       const httplib::Request& req,
                       httplib::Response& res);

// Si requireHttps=true ET isHttpsServer=false → 301 vers HTTPS.
// `isHttpsServer` doit être true pour le handler enregistré côté HTTPS,
// false pour celui enregistré côté HTTP.
bool redirectIfHttpsRequired(WebService& svc,
                              const httplib::Request& req,
                              httplib::Response& res,
                              bool isHttpsServer);

// Enregistre un endpoint GET /api/policy/flags (auth dashboard PIN
// requise) qui retourne un JSON minimal des flags policy lisibles
// par le frontend (utilisé par peers.js pour masquer la section P2P).
void registerPolicyFlags(WebService& svc);

} // namespace ltr::web::routes
