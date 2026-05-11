# Proposition UI/UX — Sprint V1.6.5

**Statut** : ✅ Validée par utilisateur le 2026-05-05
**Choix retenus** : 1.A · 2.A · 3.A · 4.B · 5.A

---

## Surface 1 — `/login` : checkboxes (Wave 3)

**Choix : Option A — Empilées sous le formulaire**

Layout :
```
┌─────────────────────────────────┐
│  Code d'accès                   │
│  Affiché sur Mac de Serge       │
│                                 │
│  [4] [7] [2] [9] [3] [1]        │
│                                 │
│  [Se connecter]                 │
│                                 │
│  ☑ Se souvenir de cet appareil │
│  ☐ Mémoriser le PIN ⓘ          │
└─────────────────────────────────┘
```

Specs :
- 2 checkboxes empilées verticalement, alignées à gauche
- Espacement `var(--sp-md)` entre formulaire et checkboxes
- "Se souvenir de cet appareil" : **cochée par défaut** (décision BA Q3)
- "Mémoriser le PIN" : **décochée par défaut**, suivie d'une icône info ⓘ qui affiche au hover/tap : « Le PIN est chiffré localement avec une clé liée au certificat de cet appareil. Sera effacé à la déconnexion. »
- Si HTTPS indisponible (HTTP plain) : « Mémoriser le PIN » désactivée + info "Disponible uniquement en HTTPS"

Tokens CSS :
- couleur checkbox accent : `var(--accent)`
- Label `font-size: var(--fs-small)`, color `var(--text)`
- Sub-info `color: var(--text-secondary)`

---

## Surface 2 — Bouton « Reprendre » dans transfer_registry (Wave 2)

**Choix : Option A — Bouton inline dans la card**

Layout :
```
┌──────────────────────────────────────┐
│ ⏸  film.mp4                         │
│    60% · interrompu                 │
│    [▶ Reprendre]   [✕ Annuler]      │
└──────────────────────────────────────┘
```

Specs :
- Card existante du `transfer_registry.js`, ajout d'une rangée d'actions sous la ligne de status
- Bouton "Reprendre" : `class="p2p-resume-btn"`, fond `var(--accent)`, icône ▶ + texte
- Bouton "Annuler" : `class="p2p-cancel-btn"`, secondary (existing pattern)
- Statut "interrompu" : `color: var(--warning)` (orange)
- Affiché uniquement si entry.status == "paused" (pending depuis sidecar IndexedDB)

---

## Surface 3 — Vue Historique desktop (Wave 4)

**Choix : Option A — Écran plein avec onglets + filtres**

Layout :
```
┌──────────────────────────────────────────┐
│ ◀ Retour   Historique                    │
├──────────────────────────────────────────┤
│ [ Pairs ]  [ Transferts ]                │
├──────────────────────────────────────────┤
│ Filtres : [Date ▾] [Kind ▾] [Pair ▾]     │
├──────────────────────────────────────────┤
│ Aujourd'hui                              │
│   → Pierre · 3 fichiers · 140 MB · ✓     │
│   ← Anne   · 2 fichiers ·  42 MB · ✓     │
│ Hier                                     │
│   → Bob    · 1 fichier  ·   8 MB · ✗     │
│ ...                                      │
└──────────────────────────────────────────┘
```

Specs :
- Nouvelle screen `history_screen.cpp/.hpp` (à côté de `main_screen.cpp`)
- Header : back arrow + titre, fixe en haut
- Onglets segmentés ("Pairs" / "Transferts") sous le header, accent souligné sur l'actif
- Barre de filtres : 3 dropdowns (Date : tout/7j/30j/6mois ; Kind : tout/p2p/http/tcp ; Pair : tout/{listed peers})
- Liste scroll vertical, groupée par date (Aujourd'hui / Hier / Cette semaine / mois précédent)
- Card par entry avec : direction icon, nom pair, fichiers, taille totale, status final, timestamp à droite
- Click sur une entry : modal détails (à venir V2)

Accès : nouveau bouton « 📋 Historique » dans le header, à côté du toggle SharePanel.

---

## Surface 4 — Pairs offline dans la sidebar (Wave 4)

**Choix : Option B — Section repliable « Récents (offline) »**

Layout :
```
APPAREILS · 1 en ligne
[●] Mac de Serge        Local   maintenant

▾ Récents (2)
[○] iPhone Anne         Web     hier
[○] PC Bob              Local   il y a 5j
```

Specs :
- Section "Récents" sous la liste des pairs en ligne
- Header repliable : `▾` chevron + texte « Récents (n) »
- État replié par défaut si > 5 entries hors-ligne ; déployé sinon
- Item offline : `[○]` cercle vide gris, opacité 0.5, sub-text « il y a Xj » à droite
- Click sur un item offline : feedback "Pair hors ligne" (toast court), pas d'action
- Clic-droit sur un item offline : menu contextuel "Oublier ce pair" (purge `peers_history.json`)
- Purge auto > 30 jours (décision BA Q5)

---

## Surface 5 — Toast warning TOFU P2P (Wave 4)

**Choix : Option A — Toast persistant bloquant + action user**

Layout :
```
┌──────────────────────────────────────┐
│ ⚠ L'identité de « Anne » a changé.   │
│   Vérifie qui c'est avant de         │
│   recevoir des fichiers.             │
│                                      │
│   [Faire confiance] [Refuser]        │
└──────────────────────────────────────┘
```

Specs :
- Affiché en haut centré, classe `p2p-toast` (existing) + variant `p2p-toast-blocking`
- Fond `var(--warning)` (orange), icône ⚠, texte blanc
- 2 boutons :
  - **Faire confiance** : enregistre le nouveau fingerprint dans IndexedDB `ltr-p2p-known-peers`, ferme le toast, autorise les transferts P2P avec ce pair
  - **Refuser** : ferme le toast SANS update IndexedDB, **bloque les nouveaux transferts P2P** avec ce pair pour la durée de la session (en mémoire), permet de revenir plus tard
- Bloque l'apparition de l'offer modal tant que pas de décision
- Pas d'auto-dismiss : décision explicite obligatoire

Comportement IndexedDB :
- Store `ltr-p2p-known-peers` keyed by deviceId, value `{fingerprint, name, firstSeen, trustedAt}`
- 1ère fois (TOFU) : ajout silencieux, pas de toast
- Match : OK silencieux
- Mismatch : toast bloquant comme ci-dessus

---

## Récapitulatif

| Surface | Choix | Composant à créer / modifier |
|---|---|---|
| 1 — /login checkboxes | A | `assets/web/html/login.html`, `assets/web/js/login.js`, `assets/web/css/style.css` |
| 2 — Reprendre P2P | A | `assets/web/js/transfer_registry.js`, CSS `.p2p-resume-btn` |
| 3 — Vue Historique | A | `src/ui/screens/history_screen.cpp/.hpp`, `include/ltr/ui/widgets/header.hpp` |
| 4 — Pairs offline grisés | B | `src/ui/screens/main_screen.cpp` (section repliable), `include/ltr/app/app_state.hpp` (`hiddenPeers`) |
| 5 — Toast TOFU P2P | A | `assets/web/js/p2p_session.js`, `assets/web/js/p2p_ui.js`, `assets/web/css/style.css` |

---

**Statut final** : ✅ **Validé par utilisateur le 2026-05-05.** Prêt pour ÉTAPE 4 — Plan d'action puis Wave 1 dev.
