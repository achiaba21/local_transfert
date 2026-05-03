# Spec métier — Sprint V1.6.1 Refactor split p2p.js

**Date :** 2026-05-03
**Statut :** ✅ Validée

## 1. Contexte
p2p.js = 905 LOC, flag 🚨 par audits V1.3 et V1.5. Mélange transport
WebRTC, session de transfert, UI.

## 2. Objectif
Refactor strict en 3 modules + orchestrator. Zéro changement
comportemental. API publique inchangée.

## 3. Décisions validées
| # | Décision | Choix |
|---|---|---|
| 1 | Scope | Refactor atomique 1 commit |
| 2 | Bug post-refactor | Fix dans le sprint |

## 4. Découpage
- `p2p_transport.js` (~200 LOC) : RTCPeerConnection, ICE, signaling
- `p2p_session.js` (~450 LOC) : files, chunks, ack, watchdog, fileStatuses
- `p2p_ui.js` (~250 LOC) : modale, sticky, toast, peer card
- `p2p.js` (~80 LOC) : orchestrator, expose API publique

## 5. Critères d'acceptation
- [ ] 4 fichiers chargés dans l'ordre dans index.html
- [ ] `window.LTR.p2p` strictement identique (4 méthodes)
- [ ] 16/16 tests existants passent
- [ ] Build clean
- [ ] Smoke manuel : Cmd+V, drop, paste, send vers Host et P2P
      fonctionnent à l'identique
