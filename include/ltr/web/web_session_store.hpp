#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "ltr/web/web_session.hpp"

namespace ltr::web {

// Registre thread-safe des sessions web authentifiées.
//
// V1.1 : double indexation :
//   - sessions_       : token → WebSession  (auth courante)
//   - deviceToToken_  : deviceId → token    (dédup : 1 session par navigateur)
//
// Quand un navigateur ré-authentifie avec un deviceId déjà présent, l'ancien
// token est invalidé avant d'en créer un nouveau.
class WebSessionStore {
public:
    // Retour de evictExpired : on expose le deviceId en plus du token pour
    // permettre au caller (web_service::keepaliveLoop) de nettoyer la table
    // inverse et d'émettre un PeerLostEvent avec la bonne identité.
    struct EvictedSession {
        std::string token;
        std::string deviceId;
    };

    WebSessionStore() = default;

    WebSessionStore(const WebSessionStore&)            = delete;
    WebSessionStore& operator=(const WebSessionStore&) = delete;

    // Vérifie le PIN fourni et, si OK, crée une session pour ce navigateur.
    // `deviceId` doit être un UUID stable fourni par le navigateur (localStorage).
    // Si `deviceId` est déjà associé à une session active, l'ancienne est
    // invalidée (éviction) avant création de la nouvelle.
    // Retourne le nouveau token, ou std::nullopt si PIN incorrect.
    std::optional<std::string> authenticate(const std::string& providedPin,
                                            const std::string& expectedPin,
                                            const std::string& deviceId,
                                            const std::string& userAgent);

    // Récupère la session par token si elle existe et n'est pas expirée.
    std::optional<WebSession> validate(const std::string& token) const;

    // Met à jour lastSeen de la session (à chaque requête authentifiée).
    void touch(const std::string& token);

    // Retire les sessions expirées. Retourne la liste avec token + deviceId.
    std::vector<EvictedSession> evictExpired();

    // Snapshot des sessions actives — utilisé pour le keepalive (ré-émission
    // de PeerSeenEvent toutes les 2s).
    std::vector<WebSession> snapshot() const;

    // Retire explicitement une session par token (logout, shutdown).
    void removeByToken(const std::string& token);

    // Retire la session courante d'un device (ex: si un même navigateur se
    // ré-authentifie, on invalide d'abord l'ancien token).
    void removeByDeviceId(const std::string& deviceId);

private:
    mutable std::mutex mu_;
    std::map<std::string, WebSession> sessions_;        // token → session
    std::map<std::string, std::string> deviceToToken_;  // deviceId → token
};

} // namespace ltr::web
