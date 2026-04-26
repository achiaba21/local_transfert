# Spec métier — Sprint UX-3 Drag & drop OS

**Date :** 2026-04-24
**Statut :** ✅ validé utilisateur
**Scope :** app desktop SFML, intégration native macOS + Windows

---

## 1. Contexte

L'app desktop oblige aujourd'hui l'utilisateur à cliquer `Ajouter ▾` puis
naviguer un dialog système pour choisir les fichiers à envoyer. C'est
inhabituel pour un desktop moderne — le drag & drop depuis Finder/Explorer
est attendu par défaut.

---

## 2. Objectif

Permettre de draguer 1+ fichiers/dossiers depuis Finder (macOS) ou
Explorer (Windows) directement dans la fenêtre LocalTransfer. Les paths
droppés sont ajoutés à `selectedFiles` via l'API existante
`AppController::addFiles(paths)`.

---

## 3. Décisions produit validées

### Q1 — Zone de drop (A)
**Toute la fenêtre** accepte les drops. L'utilisateur ne se préoccupe
pas de viser précisément une zone spécifique. La sidebar, le header, le
SharePanel, la zone transferts — tout est cible valide.

### Q2 — Feedback visuel (C)
**Combinaison** : pendant un drag OS au-dessus de la fenêtre :
- Highlight de la zone centrale : bordure accent 2 px + overlay
  `accentLight` subtil (alpha ~40/255) sur le rectangle central
- Texte inline « Déposer pour ajouter » par-dessus l'empty state du
  centre (ou en bas si déjà des fichiers listés)

Le feedback disparaît au `draggingExited` ou au `performDragOperation`
(drop consommé).

### Q3 — App au 2nd plan (B)
**Laisser tel quel.** L'OS (macOS + Windows) gère déjà naturellement le
comportement drag → focus window. Pas d'intervention programmatique.

### Q4 — Linux support (A)
**Stub no-op pour V1**. Le build Linux compile mais log un warning au
démarrage « Drag & drop : pas encore supporté sur cette plateforme ».
XdndAware éventuellement en V2.

### Q5 — Multi-drop séquentiel (A)
**Append** — chaque drop ajoute au lot existant, cohérent avec le
comportement actuel d'`addFiles` via le menu. L'utilisateur peut
toujours cliquer « Vider » pour repartir à zéro.

---

## 4. Critères d'acceptation

- **macOS** : drag 3 fichiers depuis Finder → dépôt dans la fenêtre → les
  3 apparaissent dans la liste centrale avec checkbox cochée.
- **macOS** : drag un dossier depuis Finder → dépôt → apparition en
  « [icône dossier] Mon Dossier / 42 fichiers ».
- **Windows** : drag 3 fichiers depuis Explorer → dépôt → idem.
- **Feedback visuel** : pendant le drag, la zone centrale est bordée
  accent + tintée légèrement, texte « Déposer pour ajouter » visible.
- **Drop à l'extérieur de la zone mais dans la fenêtre** : les fichiers
  sont quand même ajoutés (Q1=A).
- **Linux** : build passe, warning log au démarrage, `Ajouter ▾` reste
  fonctionnel.
- **Aucune régression** : les 8 tests passent, TCP LTR1 OK, couche web
  OK.

---

## 5. Contraintes techniques

- C++17 (+ Objective-C++ restreint au fichier `drag_drop_mac.mm`)
- Aucune nouvelle dépendance externe
- Utiliser uniquement AppKit/Foundation (macOS), Shell32 (Windows) —
  déjà fournis par le SDK système
- Pas de threading custom : les callbacks OS arrivent sur le thread UI,
  on appelle directement `controller_.addFiles()` + toggle
  `dragActive_` sur MainScreen
- Backward compat : le bouton `Ajouter ▾` reste fonctionnel en parallèle
