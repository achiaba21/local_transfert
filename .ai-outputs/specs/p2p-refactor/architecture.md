# Architecture — Sprint V1.6.1 Refactor p2p.js

**Date :** 2026-05-03
**Statut :** ✅ Validée

## Pattern
IIFE en cascade. Chaque module expose un namespace privé sous
`window.LTR.p2pTransport / p2pSession / p2pUi`. `p2p.js` = orchestrator
qui re-expose API publique inchangée.

## Map `conns` partagée
Déclarée dans `p2p_transport.js` (transport-owned), accessible aux
autres via `window.LTR.p2pTransport.conns`. `connKey` et `getConn`
exportés depuis transport.

## Dépendances (ordre IIFE)
```
common.js           (clientLog, escapeHtml, formatBytes — déjà en place)
p2p_transport.js    (conns, postSignal, wirePcCommon, connKey, getConn)
p2p_session.js      (cleanup, sendNextFile, wireSenderDc, wireReceiverDc, ...)
p2p_ui.js           (modale, sticky, toast, markCardSending)
p2p.js              (orchestrator : startSendTo, handleSignal, cleanupAll, toast)
```

`p2p_session.js` dépend de `p2p_transport.js` (conns, postSignal) et
`p2p_ui.js` (markCardSending, updateProgress, toast, refreshSticky).
`p2p.js` dépend des 3.

## CONTRAT D'IMPLÉMENTATION

### Fichiers AJOUTER (3)
- [ ] `assets/web/js/p2p_transport.js`
- [ ] `assets/web/js/p2p_session.js`
- [ ] `assets/web/js/p2p_ui.js`

### Fichiers MODIFIER (4)
- [ ] `assets/web/js/p2p.js` — devient orchestrator ~80 LOC
- [ ] `assets/web/html/index.html` — 4 scripts dans l'ordre
- [ ] `CMakeLists.txt` — embed 3 nouveaux JS
- [ ] `src/web/routes/static_routes.cpp` — 3 nouvelles routes

## Contraintes
- IIFE simple (chacun expose un objet sur window.LTR)
- Pas de modules ES6
- Préservation API publique stricte
- Comportement strictement identique

UI_REQUIRED: false
