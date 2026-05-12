# Organisation des assets JavaScript

Les scripts web sont rangés par **responsabilité** (SRP). Chaque sous-dossier
a un rôle clair ; un fichier ne contient qu'une chose ; les dépendances
remontent toujours vers `core/`.

```
assets/web/js/
├── core/         helpers sans dépendance domaine
├── pages/        entry-points (1 par fichier HTML)
├── dashboard/    modules du dashboard host authentifié
└── p2p/          module WebRTC isolé (chargé seulement quand utilisé)
```

## Convention

- Chaque module est un **IIFE** qui expose son API publique via `window.LTR.*`.
- Les modules `dashboard/` et `pages/` **consomment** `core/` via `window.LTR`,
  ils ne réimportent pas leurs implémentations directement (DIP).
- Pas de bundler : les ordres d'inclusion dans le HTML font foi.

## Mapping fichier → URL servie

Côté serveur (`src/web/static_asset_registry.cpp`), la table déclarative
mappe chaque source physique vers une URL stable. Renommer un fichier
source n'oblige pas à changer l'URL publique.

| Source | URL publique |
|---|---|
| `pages/login.js` | `/login.js` |
| `pages/dashboard.js` | `/app.js` *(URL conservée pour stabilité cache)* |
| `pages/deposit.js` | `/deposit.js` |
| `core/common.js` | `/common.js` |
| `core/idb.js` | `/idb.js` |
| `core/pin_storage.js` | `/pin_storage.js` |
| `core/web_profile.js` | `/web_profile.js` |
| `core/transfer_registry.js` | `/transfer_registry.js` |
| `dashboard/peers.js` | `/peers.js` |
| `dashboard/upload.js` | `/upload.js` |
| `dashboard/download.js` | `/download.js` |
| `dashboard/share.js` | `/share.js` |
| `dashboard/host_deposit_links.js` | `/host_deposit_links.js` |
| `p2p/index.js` | `/p2p.js` |
| `p2p/transport.js` | `/p2p_transport.js` |
| `p2p/session.js` | `/p2p_session.js` |
| `p2p/ui.js` | `/p2p_ui.js` |

## Ajouter un nouvel asset

1. Créer le fichier dans le bon sous-dossier (`core/`, `pages/`, `dashboard/`, `p2p/`).
2. Ajouter une ligne dans `CMakeLists.txt` (section `ltr_embed_file`).
3. Ajouter une ligne dans `src/web/static_asset_registry.cpp` (table `buildStaticAssetTable`).
4. Inclure `<script src="/...">` dans le HTML cible.

Aucune modification de `static_routes.cpp` n'est requise : il itère sur la table.
