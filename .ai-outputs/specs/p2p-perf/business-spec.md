# Spec métier — Sprint V1.6.3 Performance P2P

**Date :** 2026-05-03
**Statut :** ✅ Validée

## 1. Contexte
Couche P2P fonctionnelle (V1.4-V1.6.1) mais limites perf : crash >1 Go
RAM, débit non saturé, perte connexion = abandon, disconnected
transitoire tue la session.

## 2. Objectif
4 lots perf séquentiels.

## 3. Décisions validées
| # | Décision | Choix |
|---|---|---|
| 1 | OPFS fallback | Cap soft 1 Go (refuse au-delà avec toast) |
| 2 | K parallel streams | Configurable cfg.json p2pParallelStreams, default 4 |
| 3 | TTL sidecars | 24 h purge auto |
| 4 | Échec re-négo | 1 tentative + fallback resume au prochain envoi |
| 5 | Test stress | L'user teste en réel |

## 4. Lots
- **LOT 1** : OPFS receveur + cap fallback 1 Go
- **LOT 2** : Multi-stream K=4 (configurable) + ack par-dcId
- **LOT 3** : Sidecar IndexedDB resume + resume-offer protocol
- **LOT 4** : iceRestart re-négo auto sur disconnected

## 5. Critères acceptation
- [ ] 2+ Go reçu OK Chrome (OPFS)
- [ ] Fallback Blob refuse >1 Go avec toast
- [ ] Débit multi-fichier amélioré vs single stream
- [ ] cfg.json p2pParallelStreams respecté
- [ ] Sidecar à chaque ack (throttle 1 s)
- [ ] Resume détecté + repris
- [ ] Cut Wi-Fi 3 s → re-négo OK
- [ ] Cut >15 s → cleanup + sidecar pour prochain envoi
- [ ] 16/16 tests + aucune régression
