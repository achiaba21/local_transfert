# Spec métier — Sprint Transfer Resume

**Date :** 2026-04-24
**Statut :** ✅ validé utilisateur
**Analyse de référence :** `.ai-outputs/specs/transfer-resume/ANALYSIS.md`

---

## 1. Contexte

Flow TCP LTR1 actuel : sur erreur réseau, le `.part` côté receveur est
supprimé + le sender émet `TransferFailedEvent` + thread exit. Aucune
reprise possible. Le moindre Wi-Fi blip de 2 s coupe un envoi 5 Go →
5 Go à retransmettre.

Envois simultanés fonctionnent déjà techniquement (threads parallèles),
mais aucune gestion des pannes.

---

## 2. Objectif

Rendre le flow **résilient** :
- Les partials sont conservés
- Les erreurs Network sont classifiées + retentées silencieusement
- Au-delà du retry auto, l'utilisateur peut cliquer « Reprendre »
- Les transferts survivent à un crash/restart de l'app
- Les Wi-Fi bloqués sont détectés en < 20 s via heartbeat (au lieu de 30-120 s TCP OS)

---

## 3. Décisions produit validées

### 8 décisions initiales (ANALYSIS.md § 3)
1. TTL sidecars : **1 jour**
2. Auto-retries : **configurable** (default 2)
3. Cross-restart : **inclus V1**
4. Bouton Ignorer : **passif** (laisser expirer côté receveur)
5. Multi-file resume : **oui**
6. Source modifié : **restart silencieux**
7. Heartbeat : **Phase 1**
8. UI : **bouton par card + Reprendre tout global**

### 3 décisions résiduelles BA
- **Q1 — « Reprendre tout » = séquentiel** (1 par 1, prévisible, pas de
  charge réseau explosive)
- **Q2 — Sidecar = 1 par session globale** (fichier en cours + liste
  des déjà-reçus, pas un sidecar par fichier partial) → simplifie la
  lookup et évite la pollution du downloadDir
- **Q3 — Startup avec sessions en attente** = banner de notif « N
  transferts en attente, [Reprendre tout] [Ignorer] » (pas d'auto-reprise
  silencieuse : l'utilisateur décide)

---

## 4. Livrables

### Backend (serveur + client)
- Sidecar JSON `.ltr-resume-<sessionId>.json` dans `downloadDir/` (1 par
  session, pas par fichier)
- Purge sidecars > 24 h au `TransferServer::start`
- `pending-sessions.json` sender dans le config dir + load au startup
- Nouveaux MessageType : `ResumeOffer` (0x0A), `ResumeResponse` (0x0B),
  `Ping` (0x0C), `Pong` (0x0D)
- `TransferClient::resumeSession(sid)` avec negotiation
- `TransferServer` : gestion `ResumeOffer` → lookup sidecar → réponse
  avec action+skipBytes par fichier (continue | restart | skip)
- `ErrorCategory` enum (Network/Protocol/Permanent/Cancelled) dans
  `TransferFailedEvent`
- Auto-retry silencieux `autoRetryCount`× sur Network avec backoff
  exponentiel (1s, 4s, 16s, cap 5 max)
- Heartbeat Ping/Pong pendant le streaming (tous les 10 s, timeout 20 s)

### Config
- `infra::Config::autoRetryCount` (default 2)
- `infra::Config::resumeSidecarTtlHours` (default 24)

### UI
- Bouton « Reprendre » par card Failed resumable (à côté de Ignorer)
- Bouton « Reprendre tout » dans le header TRANSFERTS (visible si ≥1
  card resumable) → séquentiel 1 par 1
- Sous-statut « Reconnexion 1/N… » pendant auto-retry
- **Banner de notif au startup** (Q3=C) si sessions en attente : pill
  en haut de la fenêtre « 3 transferts en attente · [Reprendre tout]
  [Ignorer] »

### Tests & docs
- Tests unitaires : resume source modifié (restart), peer absent
  (Failed), sidecar corrompu (delete+restart), auto-retry Network
- Smoke tests manuels : Wi-Fi cut mid-send, app kill mid-send
- Doc : `docs-agents/NETWORK.md` (nouveau fichier) avec flow complet +
  limitations V1 + breaking changes protocol

---

## 5. Critères d'acceptation

- Envoi 5 Go, Wi-Fi coupé à 60 %, Wi-Fi rétabli 3 s après → **envoi
  reprend automatiquement** sans intervention utilisateur
- Envoi 5 Go, Wi-Fi coupé à 60 %, Wi-Fi rétabli 60 s après → **2 retries
  silencieux échouent** → card passe Failed avec [Reprendre] visible →
  clic → reprise du byte 3 Go (pas from 0)
- 10 fichiers envoyés, 7 OK + échec au 8e → sidecar liste les 7 OK +
  offset du 8e → resume reprend au 8e (skip 1-7), termine 8-10
- App killée au milieu d'un envoi → redémarrage → banner « 1 transfert
  en attente » → clic « Reprendre tout » → envoi reprend
- Source file modifié pendant l'échec → resume détecte mismatch
  SHA256Prefix → restart silencieux (log warn, redémarrer depuis 0)
- 5 envois parallèles vers 3 peers différents, tous échouent → 5 cards
  Failed → clic « Reprendre tout » global → les 5 reprennent **en
  séquence** (1 par 1)
- 8 tests existants passent sans régression
- Peer legacy LTR1 (sans support ResumeOffer) → fallback vers Offer
  classique (restart complet), aucune erreur

---

## 6. Contraintes

- C++17 strict
- Aucune nouvelle dépendance
- RAII, EventBus pour comm thread → UI
- Aucune régression TCP LTR1 legacy (fallback obligatoire)
- Pas de casse couche web V1.1.8
- Les 8 tests existants doivent continuer de passer
- UI_REQUIRED: true (mockups intégrés dans ANALYSIS.md § 4.6 — pas de
  nouvelle validation UI/UX séparée)

---

## 7. Hors scope (V2)

- Parallélisme intra-fichier (voir `native-transfer-parallel/ANALYSIS.md`)
- Multi-destinataires avec fanout (lecture unique)
- Signature cryptographique des binaires (voir
  `multi-os-installer/ANALYSIS.md`)
- Settings UI pour `autoRetryCount` (config.json édité à la main V1)
