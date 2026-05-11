# Spécification métier — Sprint V1.6.5

**Nom** : Stabilité, sessions résilientes, historique (HTTP + P2P)
**Version cible** : V1.6.5
**Date validation** : 2026-05-05
**Statut** : ✅ Validée

---

## 1. Contexte

Suite au sprint V1.6.4 (HTTPS LAN + TOFU TCP), l'utilisateur a remonté 5 zones d'instabilité affectant les 2 chemins de transfert web :
- **Chemin HTTP** (visiteur ↔ host via `/api/upload`, `/api/download`)
- **Chemin P2P** (visiteur ↔ visiteur via WebRTC DataChannel, signal relayé par le host)

L'analyse code a confirmé 5 lacunes structurelles :
1. Aucune stratégie de resume (HTTP : pas de Range-request, pas de sidecar upload ; P2P : OPFS handle perdu au reload, aucun iceRestart)
2. Téléchargement bloqué à 0% (HTTP : `TransferProgressEvent` émis seulement au 1er chunk lu ; P2P : `phase='sending'` set seulement à `dc.onopen`)
3. Sessions web trop strictes (TTL 30s, cookie session pur, redémarrage host = perte totale, P2P signaling 401 silencieux mid-transfert)
4. PIN jamais mémorisé côté browser (saisie obligatoire à chaque ouverture d'onglet)
5. Aucun historique persistant côté host (pas de `peers_history.json`, pas de `transfer_history.json`)

## 2. Objectif

Sprint XL en **4 vagues** indépendantes pour livrer fiabilité + ergonomie sans introduire de régression sur le périmètre V1.0 → V1.6.4.

## 3. Acteurs

- **Visiteur web** (mobile/desktop accédant via /login) — bénéficie de la stabilité + sessions persistantes + remember-me PIN
- **Host desktop** (utilisateur SFML) — bénéficie de l'historique des pairs + des transferts
- **Pair P2P** (deux visiteurs qui s'envoient des fichiers entre eux) — bénéficie du resume + iceRestart + TOFU P2P

## 4. Périmètre des 4 vagues

### Wave 1 — Stabilité critique (HTTP) — **EN UN SEUL COUP**
- **A.** Bug "bloqué à 0%" download HTTP : émettre `TransferProgressEvent{0}` dès la création du ticket
- **A bis.** Idem P2P : event `phase='connecting'` dès `pc.connectionState='connecting'`
- **B.** Cancel flag leak : nettoyer `cancelFlags_[sessionId]` à la fin du content_provider
- **C.** Resume upload HTTP : multipart + field `resume` + sidecar `.resume.json` + retry 3× exp backoff côté browser
- **D.** Resume download HTTP : Range Requests RFC 7233 + 206 Partial Content

### Wave 2 — Resume P2P
- **E.** Sidecar IndexedDB côté receveur P2P (persiste `bytesWritten` à chaque ack) + bouton "Reprendre" au boot
- **G.** iceRestart automatique (si `connectionState='disconnected'` > 5s)

> ⚠️ **Le sidecar émetteur (F) est ABANDONNÉ** (décision BA Q2.b). Au reload émetteur, le transfert est marqué `failed{session_perdue}` comme aujourd'hui. Raison : sandbox browser empêche persister le `File` brut, le re-pick manuel introduit trop de friction utilisateur pour un cas rare.

### Wave 3 — Sessions résilientes
- **H.** Cookie persistent signé HMAC (`ltr_remember`, Max-Age 30j, secret HMAC dérivé du fingerprint cert HTTPS)
- **H bis.** TTL session active passé de 30s → 300s (5 min)
- **I.** Mémorisation du PIN sur /login : checkbox **visible directement** sous le formulaire (décision BA Q4.a), chiffrement WebCrypto AES-GCM, clé dérivée HKDF du fingerprint cert
- **Logout** : efface `ltr_token`, `ltr_remember`, `ltr-pin-encrypted`, `ltr-pin-iv`

### Wave 4 — Historique persistant
- **J.** `peers_history.json` côté host : pairs offline conservés **30 jours** (décision BA Q5), bouton "Oublier ce pair" sur clic-droit
- **K.** `transfer_history.json` côté host : cap **1000 entries**, auto-purge **> 6 mois**
- **L.** TOFU P2P : `deviceId` stable inclus dans offer, vérif fingerprint DTLS dans IndexedDB browser, toast warning si change

## 5. Règles métier — décisions BA validées

| # | Question | Réponse retenue |
|---|---|---|
| Q1 | Périmètre Wave 1 | **Tout en un seul coup** (A+B+C+D + A bis P2P) |
| Q2 | P2P resume émetteur (F) | **Abandonné** (b) — receveur E uniquement |
| Q3 | Checkbox "Se souvenir de cet appareil" (cookie persistent) | **Cochée par défaut** (ergonomie LAN privé) |
| Q4 | Checkbox "Mémoriser le PIN" | **Visible** sous le formulaire (a) |
| Q5 | Rétention pairs offline | **30 jours** ; transferts cap 1000 / purge 6 mois |

## 6. Cas d'usage principaux

### CU-1 — Visiteur sur Wi-Fi instable
1. Le visiteur drop un fichier 2 GB via /api/upload
2. À 30%, le Wi-Fi coupe 10 secondes
3. Le browser détecte l'erreur, retry automatiquement avec `Content-Range` du dernier offset
4. L'upload reprend depuis 30%, jamais from scratch

### CU-2 — Visiteur P2P avec onglet rechargé
1. Anne envoie un fichier 200 MB à Bob via P2P
2. Bob reçoit 60% (OPFS)
3. Bob recharge accidentellement son onglet
4. Au boot, l'onglet de Bob détecte le sidecar IndexedDB → propose "Reprendre depuis 60%"
5. Bob clique → reconnexion P2P + resume

### CU-3 — Visiteur retourne le lendemain
1. Anne a coché "Se souvenir de cet appareil" + "Mémoriser le PIN" hier
2. Aujourd'hui Anne ouvre l'URL host
3. Le cookie persistent est rejoué → recréation de session côté serveur (PIN host inchangé) → Anne arrive directement sur la page principale, sans /login

### CU-4 — Host consulte l'historique
1. Le host clique sur l'icône "📋 Historique" en haut de la fenêtre
2. Liste : pairs vus + transferts effectués sur les 30 derniers jours
3. Filtres : par pair, par kind (p2p/http-up/http-down/tcp), par date
4. Le host peut "Oublier" un pair (purge entries associées)

### CU-5 — Identité P2P changée
1. Bob a connu "Anne" hier, fingerprint DTLS X
2. Aujourd'hui une autre Anne se présente (deviceId identique mais fingerprint Y)
3. Bob voit un toast warning "L'identité de Anne a changé, vérifie qui c'est"
4. Bob continue ou refuse le transfert

## 7. Cas alternatifs / limites

- **Reload onglet émetteur P2P** → transfert marqué `failed{session_perdue}` (pas de resume émetteur, décision Q2.b)
- **Redémarrage du host** → PIN regénéré → tous les `ltr_remember` deviennent invalides (HMAC ne match plus pinHash) → re-PIN obligatoire
- **Cert HTTPS régénéré** (changement IP LAN par exemple) → tous les `ltr_remember` invalides ET les PIN chiffrés en localStorage indéchiffrables → re-PIN
- **Vol du device** → la mémorisation PIN expose le code mais le risque reste local LAN (l'attaquant doit déjà être sur le réseau Wi-Fi du host) — assumé

## 8. Contraintes techniques (rappel CLAUDE.md)

- C++17 strict, **pas de nouvelle dépendance externe**
- RAII, EventBus pour comm thread → UI
- `utf8()` pour SFML, `RoundedRect` pour widgets
- Pas de `catch(...)` vide, pas de `TODO`/`FIXME` laissés
- Cross-platform Mac+Windows
- **Aucune régression** V1.0 → V1.6.4 : 17/17 tests doivent passer + nouveaux tests
- API publique inchangée (paramètres optionnels uniquement pour Waves 1-2)

## 9. Critères d'acceptation

### Wave 1 — Stabilité
- [ ] Build Release propre Mac (Win à valider plus tard)
- [ ] 17/17 tests V1.6.4 passent + nouveaux tests Range / resume upload
- [ ] Smoke E2E : fichier 50 MB simulé déconnexion à 30% → resume from 30%, pas from 0
- [ ] `cancelFlags_` empty après chaque download (vérifié par log)
- [ ] UI desktop : `TransferProgressEvent{0}` reçu dès l'envoi (pas attente du 1er chunk)
- [ ] Build : `curl --range 0-1000` sur /api/download/:id → 206 Partial Content

### Wave 2 — Resume P2P
- [ ] Smoke E2E : transfert P2P 200 MB → reload onglet receveur à 50% → bouton "Reprendre" → reprise from 50%
- [ ] iceRestart vérifié : couper Wi-Fi 5s → reconnexion auto < 15s, transfert continue

### Wave 3 — Sessions
- [ ] Build : redémarrage app + cookie `ltr_remember` valide → re-création de session sans /login
- [ ] Build : Logout → cookies + localStorage entrées chiffrées vidées
- [ ] Test unit `test_persistent_token` (HMAC valid/invalid/expired/pin-mismatch)
- [ ] Test unit `test_pin_encrypted_storage` (chiffrement réversible, déchiffrement correct, mauvaise clé → fail)

### Wave 4 — Historique
- [ ] Build : pair vu hier visible grisé dans la sidebar
- [ ] Build : transfert effectué visible dans la vue Historique
- [ ] Test unit `test_peers_history` (upsert lastSeen, purge > 30j, totalTransfers/Bytes)
- [ ] Test unit `test_transfer_history` (insert TransferStarted, update Done, cap 1000)
- [ ] TOFU P2P : 2e session avec même peer + même fingerprint = silencieux ; fingerprint changé = toast warning

---

**Statut final** : ✅ **Validé par utilisateur le 2026-05-05.** Prêt pour ÉTAPE 2 — Architecture.
