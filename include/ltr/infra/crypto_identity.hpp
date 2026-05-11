#pragma once

#include <filesystem>
#include <string>

namespace ltr::infra {

// V1.6.4 — Sprint Sécurité (Wave 2 TOFU TCP).
// Calcule une empreinte d'identité stable pour l'instance native.
// Placeholder V1 (pas de vraie crypto E2E) :
//   fingerprint = SHA-256(nonce_persisté || selfId)
// Le nonce 32 octets est généré au 1er run dans `cfgDir/identity.bin` puis
// rechargé identique à chaque démarrage. Ainsi l'empreinte est stable
// tant que l'utilisateur ne supprime pas le fichier.
//
// Format de retour : "AB:CD:EF:..." (32 paires hex en MAJUSCULES,
// séparées par ':'), aligné sur le format du cert HTTPS Wave 1.
//
// V2 ciblé : remplacer par une vraie clé Ed25519 par device, générée
// au 1er boot, signature E2E des sessions TCP.
std::string loadOrGenerateSelfFingerprint(
    const std::filesystem::path& cfgDir,
    const std::string& selfId);

} // namespace ltr::infra
