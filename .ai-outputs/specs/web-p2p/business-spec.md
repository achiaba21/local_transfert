# Spec métier — Sprint Web P2P (V1.2)

**Date :** 2026-05-01
**Statut :** ✅ Validée

---

## 1. Contexte

Plusieurs navigateurs peuvent s'authentifier au même host LocalTransfer
(PIN). Aujourd'hui ils ne se voient pas et ne peuvent pas s'envoyer de
fichiers entre eux. Toute donnée passe par le host.

## 2. Objectif

Permettre aux navigateurs auth d'un même host de :

- **Se voir mutuellement** (liste « Autres appareils »)
- **S'envoyer des fichiers en vrai P2P** via WebRTC DataChannel — les
  fichiers ne transitent pas par le host
- Le host conserve son rôle de **signaling** (relay des SDP offer/answer
  + ICE candidates) et le canal **host↔web V1.1 reste intact**

## 3. Acteurs

- **Émetteur web** : navigateur auth qui choisit un destinataire et envoie
  des fichiers
- **Récepteur web** : navigateur auth qui reçoit une offre et accepte/
  refuse
- **Host LocalTransfer** : annuaire + signaling, ne stocke jamais le
  payload P2P

## 4. Décisions produit validées

| #  | Décision           | Choix                                                                        |
| -- | ------------------ | ---------------------------------------------------------------------------- |
| 1  | Transport          | WebRTC DataChannel (vrai P2P)                                                |
| 2  | Nom appareil       | Auto-généré stable (hash device_id → adjectif+animal FR)                     |
| 3  | Visibilité         | Automatique entre tous les auth (pas d'opt-in)                               |
| 4  | Acceptation        | Confirmation manuelle systématique côté récepteur                            |
| 5  | Périmètre          | host↔web V1.1 conservé, web↔web ajouté en parallèle                          |
| 6  | Affichage nom      | Emoji animal + nom + sous-titre plateforme — « 🦊 Pingouin Bleu / iPhone · Safari » |
| 7  | Concurrence        | A peut envoyer à B et C **en parallèle** (multi-channel sortant)             |
| 8  | Refus              | Notification simple côté émetteur, **pas de retry automatique**              |
| 9  | Host crash         | DataChannel **coupé proprement** dès détection de perte SSE                  |
| 10 | Cross-LAN          | **Hors scope V1** — ICE configuré en LAN-only (pas de STUN public)           |

## 5. Cas d'usage principal

1. Alice ouvre la page web et s'auth avec le PIN du host
2. Bob s'auth aussi → la section « Autres appareils » d'Alice affiche
   « 🦊 Pingouin Bleu · iPhone · Safari »
3. Alice clique sur la card Bob → file picker → choisit 3 photos
4. Une offre P2P est envoyée à Bob via le host (signaling)
5. Bob voit une modale « 🐧 Lapin Vif veut t'envoyer 3 fichiers (12 Mo) →
   Accepter / Refuser »
6. Bob clique Accepter → WebRTC negotiation (offer/answer/ICE via host) →
   DataChannel établi
7. Les fichiers transitent **directement** d'Alice vers Bob, chunkés
   (64 Ko) avec progress
8. Bob télécharge automatiquement les fichiers à la fin

## 6. Cas alternatifs / limites

- **Refus** : Bob clique Refuser → Alice reçoit notification « Bob a
  refusé »
- **TTL** : si Bob ne répond pas en 60 s, l'offre auto-expire côté
  récepteur, Alice notifiée
- **Pas de réponse / DataChannel échec** : message « Connexion P2P
  échouée »
- **Host crash en cours de transfert** : DataChannel fermé, transfert
  annulé, message clair
- **Cross-LAN** : transfert peut échouer (ICE failed) — message d'erreur,
  pas de fallback automatique
- **N≥3 destinataires** : Alice peut envoyer à B + C en parallèle, deux
  DataChannels distincts
- **Logout en cours de transfert** : RTCPeerConnection.close() propre,
  paire notifiée

## 7. Contraintes

- Aucune nouvelle dépendance C++ (signaling = relais HTTP/SSE)
- WebRTC natif navigateurs récents — aucune lib JS externe
- Théming web aligné sur tokens existants
- Sécurité : host valide que l'expéditeur signaling = device de la session
  du token cookie ; refuse tout `to` inexistant
- Pas de fuite cross-session (chaque message routé uniquement au
  destinataire)
- Aucune régression sur host↔web V1.1 (12/12 tests passent)

## 8. Critères d'acceptation

- [ ] 2 navigateurs auth voient l'autre dans « Autres appareils » dans
      les 2 s qui suivent l'auth du second
- [ ] Clic sur une card → file picker → modale de réception apparaît côté
      destinataire
- [ ] Accepter → fichier transféré en P2P (la bande passante upload du
      host reste plate pendant le transfert)
- [ ] Refuser → notif émetteur + nettoyage des deux côtés
- [ ] Auth d'un 3ᵉ device → tous les autres voient le nouveau peer
- [ ] Logout → tous les autres voient le peer disparaître
- [ ] Nom + emoji **stables** : même device_id sur 2 sessions
      différentes → même nom
- [ ] Tests unitaires : nom déterministe (3 cas), signaling refuse
      expéditeur non-auth, signaling refuse destinataire absent
- [ ] Build propre, 12/12 + nouveaux tests passent
