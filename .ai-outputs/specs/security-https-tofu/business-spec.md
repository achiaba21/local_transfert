# Spec métier — Sprint V1.6.4 Sécurité

**Date :** 2026-05-03
**Statut :** ✅ Validée

## Décisions validées

| # | Décision | Choix |
|---|---|---|
| 1 | Validité cert | 5 ans |
| 2 | Port HTTPS | 45457 + auto-fallback 45458/45459 |
| 3 | Bandeau /login | Hybride : checkbox pré-cochée, l'user peut décocher |
| 4 | TOFU natif | Ed25519 par device (vraie identité crypto) |
| 5 | TTL known_peers | Purge > 1 an inactivité |

## Lots
- LOT 1 — HTTPS LAN coexistant
- LOT 2 — TOFU Ed25519

## Critères acceptation
- [ ] https://host:45457 répond 200, cert valide 5 ans
- [ ] Fingerprint SHA-256 visible SharePanel + /login
- [ ] OPFS / clipboard.read activés via HTTPS
- [ ] Bandeau pré-cochée, mémoire localStorage
- [ ] Ed25519 keypair persistant
- [ ] Signature transmise dans Accept TCP
- [ ] known_peers store + warning inline
- [ ] 16/16 + nouveaux tests
