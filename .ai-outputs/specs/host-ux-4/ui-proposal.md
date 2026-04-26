# UI/UX — Sprint UX-4 SharePanel mockups

**Date :** 2026-04-24

Les choix haut-niveau sont validés (déplié par défaut, persist config,
rail collapsed 40 px, 2 boutons Copier, toggle intégré au SharePanel).

---

## 1. SharePanel déplié (240 px)

```
┌──────────────────────────────────┐
│ PARTAGE WEB                  ×   │  ← overline textSecondary + bouton
│                                   │      fermer 16×16 (col. droite)
│                                   │
│        ┌─────────────┐            │
│        │             │            │
│        │    QR 220   │            │  ← QR centré horizontalement
│        │             │            │
│        └─────────────┘            │
│                                   │
│ URL                               │  ← overline
│ http://192.168.1.3:45456          │  ← body bold
│                                   │
│ [ Copier URL ]                    │  ← 32 × pleine largeur interne
│ [ Copier PIN ]                    │  ← 32 × pleine largeur interne
│                                   │
│ CODE D'ACCÈS                      │  ← accent corrigé !
│ 4 7 2 9 3 1                       │  ← 44 px bold accent
│                                   │
│ Scan QR ou tapez l'URL dans votre │  ← hint small textSecondary
│ navigateur.                        │
└──────────────────────────────────┘
```

### Détails
- **Bouton fermer (×)** : icône Close 16×16 en textSecondary, hover =
  fond separator. Position : `bounds.right - 12 - 16, bounds.top + 16`.
- **Boutons Copier** : stack vertical, gap `Spacing::sm`. Variant
  Secondary. Label « Copié ! » transitoire 2 s après clic.
- **PIN 44 px** : `setSize(44)`, `setBold(true)`, `color = accent`.
  Kerning via insertion de `' '` entre chiffres (déjà implémenté V1).

---

## 2. SharePanel collapsed (40 px — rail)

### Sans visiteur web
```
┌───┐
│   │
│   │
│ [Q│  ← icône QR 20×20, centrée, ~80px du top
│ R]│
│   │
│   │
│   │
│   │
└───┘
```

### Avec N visiteurs web
```
┌───┐
│   │
│   │
│ [Q│  ← icône QR 20×20
│ R]│
│   │
│ ┌─┐│  ← badge pill accent 24×18
│ │2││    avec count en blanc overline bold
│ └─┘│
│   │
└───┘
```

### Interactions
- **Clic n'importe où sur le rail** → `toggleCb_()`.
- **Hover** : légère variation de fond (surface → sidebar) pour suggérer
  la clicabilité.
- **Largeur 40 px**, même hauteur que déplié.
- Fond `Colors::surface`, bordure gauche 1 px `Colors::separator` (comme
  le panneau déplié).

---

## 3. Transition déplié → collapsed

Pas d'animation V1. Clic sur `×` → `controller_.toggleSharePanel()` →
`state_.sharePanelCollapsed = true` → `cfg_.save()` → `rebuildLayout()`
→ prochain draw affiche le rail.

La zone centrale s'élargit instantanément de 200 px (240 - 40).

---

## 4. Icônes nouvelles

### qr.png (24×24)
Pictogramme QR minimaliste : 3 carrés de positionnement aux coins
(style « finder pattern ») + quelques pixels au centre pour évoquer
les données. Monochrome blanc + alpha — teinte selon `sf::Sprite::setColor`
au rendu :
- Rail collapsed → `Colors::accent` (attire l'œil)
- Si hover → `Colors::accentHover`

---

## 5. Badge visiteurs

```
┌──┐
│ 2│  ← pill Radius::pill (~9)
└──┘    largeur 24 (ou 28 si >9), hauteur 18
        fond Colors::accent
        texte FontSize::overline bold blanc
```

Affiché uniquement si `visitorCount_ > 0`.

---

## 6. Cohérence tokens

- Colors : `accent`, `accentHover`, `accentLight`, `textSecondary`,
  `separator`, `surface`
- Spacing : `lg` (16) pour marges internes, `sm` pour gaps boutons
- Radius : `md` boutons, `pill` badge, `sm` bouton fermer hover
- FontSize : `overline` pour labels secondaires, `body` bold pour URL,
  `44` pour PIN, `small` pour hint

---

Les décisions sont alignées ; **pas de nouvelle validation requise — on
passe au Dev.**
