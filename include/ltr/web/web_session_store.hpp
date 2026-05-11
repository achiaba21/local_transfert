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
                                            const std::string& userAgent,
                                            const std::string& customName = "");

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

    // V1.2 — Sprint Web P2P
    // PeerInfo public exposé aux autres web devices via SSE web-peers.
    // Ne contient PAS le token (interne) ni le UA brut.
    struct PeerInfo {
        std::string deviceId;
        std::string displayName;
        std::string emoji;
        std::string platformLabel;
    };

    // Retourne la liste des sessions actives sauf celle pointée par
    // excludeToken (typiquement la session du destinataire SSE).
    std::vector<PeerInfo>
    snapshotPeersFor(const std::string& excludeToken) const;

    // Résout un deviceId vers son token actif (utilisé pour le routing
    // signaling P2P : SSE vers le destinataire). std::nullopt si le
    // device n'a pas de session active.
    std::optional<std::string>
    findTokenByDeviceId(const std::string& deviceId) const;

    // Met à jour l'alias lisible d'un navigateur déjà authentifié.
    // Le nom public reste "nom automatique (alias)".
    bool updateCustomName(const std::string& token,
                          const std::string& customName);

    // V1.6.5 — Sprint Stabilité (Wave 3 item H).
    // Configure le secret HMAC utilisé pour signer/vérifier les tokens
    // persistents. À appeler 1 fois au start, après le chargement du
    // certificat HTTPS (le fingerprint sert de secret).
    void setHmacSecret(std::string secret);

    // Génère un token persistent signé HMAC SHA-256 :
    //   "{deviceId}.{expEpochSeconds}.{pinHash8}.{HMAC_HEX}"
    // pinHash8 = 8 premiers chars du SHA-256(pinClair) ; permet de
    // détecter un changement de PIN host → HMAC invalide automatiquement.
    std::string makePersistentToken(const std::string& deviceId,
                                    const std::string& currentPin,
                                    std::int64_t expEpochSeconds) const;

    // Vérifie un token persistent reçu :
    //   - format valide ?
    //   - non expiré ?
    //   - pinHash8 = SHA-256(currentPin) actuel ? (sinon PIN host a changé)
    //   - HMAC valide ?
    // Retourne le deviceId si tout OK, std::nullopt sinon.
    std::optional<std::string>
    verifyPersistentToken(const std::string& token,
                          const std::string& currentPin) const;

private:
    // V1.6.5 — Helper SHA-256 hex puis truncation.
    std::string pinHash8(const std::string& pin) const;
    // HMAC-SHA-256 retourne hex string (64 chars).
    std::string hmacSha256Hex(const std::string& key,
                              const std::string& message) const;

    mutable std::mutex mu_;
    std::map<std::string, WebSession> sessions_;        // token → session
    std::map<std::string, std::string> deviceToToken_;  // deviceId → token
    std::map<std::string, std::string> customNames_;    // deviceId → alias
    std::string hmacSecret_;                            // V1.6.5 Wave 3
};

} // namespace ltr::web
