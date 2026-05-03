# Spec métier — Sprint Hardening & Polish (V1.5)

**Date :** 2026-05-03
**Statut :** ✅ Validée

---

## 1. Contexte

Après V1.0 → V1.4, le projet a accumulé une dette technique flaggée par
les audits (p2p.js 900 LOC, encodeur PNG inline, duplication × 4) et
des manques de robustesse (resume P2P, OPFS pour gros fichiers) ainsi
que des angles morts sécurité (HTTP plain, pas de TOFU natif).

## 2. Objectif

4 vagues séquentielles : tech debt (foundation), UX confort, perf P2P,
sécurité.

## 3. Décisions produit validées

| # | Décision | Choix |
|---|---|---|
| 1 | HTTPS | **HTTP+HTTPS coexistants** — 45456 HTTP, 45457 HTTPS |
| 2 | Wizard cert | **Bandeau jaune** /login + empreinte + checkbox |
| 3 | TOFU natif | **Inline warning** sur card, non bloquant |
| 4 | Multi-stream | **K=4 fixe** DataChannels par pc |
| 5 | Notif natives | **Réceptions seulement** côté receveur |

## 4. Livrables par vague

### Wave 1 — Tech debt
- 1.1 Split p2p.js → p2p_transport / p2p_session / p2p_ui
- 1.2 core/png_encoder.hpp/.cpp + réuse clipboard_paste_win
- 1.3 Helper skipFailedFile()
- 1.4 core::log_event JSON + toggle config

### Wave 2 — UX
- 2.1 Aperçu image 40×40 (web staging + desktop card)
- 2.2 Notif natives réceptions Mac/Win + stub Linux
- 2.3 QR avec PIN toggle dans SharePanel + auto-fill /login

### Wave 3 — Perf P2P
- 3.1 OPFS / IndexedDB receveur web
- 3.2 Multi-stream K=4
- 3.3 Resume WebRTC (sidecar IndexedDB)
- 3.4 Re-négo auto disconnected transitoire

### Wave 4 — Sécurité
- 4.1 HTTPS LAN coexistant + cert auto-signé + bandeau /login
- 4.2 TOFU natif TCP + warning inline

## 5. Contraintes

- Nouvelle dep openssl autorisée Wave 4 uniquement
- Aucune régression V1.0 → V1.4
- Audit ≥ 75/100 par vague
- Mac+Win parallèle, Linux stub minimal

## 6. Critères d'acceptation

- [ ] Wave 1 : p2p.js scindé, audit ≥80 Complexité, png_encoder
      réutilisable, logs JSON valides
- [ ] Wave 2 : preview visible, notif réception Mac+Win, QR PIN
      auto-fill /login
- [ ] Wave 3 : transfert >2 Go OK, débit P2P +30 %, reprise après
      cut Wi-Fi 5 s
- [ ] Wave 4 : https://host:45457 → 200, fingerprint visible, TOFU
      warning sur changement
- [ ] 15/15 tests + nouveaux (PNG roundtrip, OPFS smoke, fingerprint
      hash)
- [ ] Build Mac et Win propre
