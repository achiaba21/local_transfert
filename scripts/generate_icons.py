#!/usr/bin/env python3
"""
Génère les icônes PNG du sprint UX-1 dans assets/icons/.

Usage :
    python3 scripts/generate_icons.py

Idempotent : regénère les fichiers à chaque appel.
Icônes monochromes alpha, teintées côté C++ via sf::Sprite::setColor.

Prérequis : Python 3 + Pillow (PIL). Sur macOS :
    pip3 install Pillow
"""
from PIL import Image, ImageDraw
import os
import sys

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "icons")
os.makedirs(OUT, exist_ok=True)

WHITE = (255, 255, 255, 255)
TRANSPARENT = (0, 0, 0, 0)

# Pour toutes les icônes : dessiner en blanc pur sur fond transparent.
# Le côté C++ tinte via sf::Sprite::setColor() avec la bonne Color::accent /
# success / textSecondary selon le contexte.

def new_image(size):
    return Image.new("RGBA", (size, size), TRANSPARENT)

def draw_check(size=24):
    """Coche en 2 segments épais."""
    img = new_image(size)
    d = ImageDraw.Draw(img)
    w = 3
    # Partir bas-gauche (5, 13), passer par coude (10, 18), finir haut-droite (19, 7)
    d.line([(5, 13), (10, 18)], fill=WHITE, width=w)
    d.line([(10, 18), (19, 7)], fill=WHITE, width=w)
    return img

def draw_folder(size=20):
    """Dossier : onglet + corps rectangulaire arrondi."""
    img = new_image(size)
    d = ImageDraw.Draw(img)
    # Onglet supérieur gauche
    d.rectangle([(2, 5), (9, 8)], fill=WHITE)
    # Corps principal
    d.rectangle([(2, 7), (17, 16)], fill=WHITE)
    # Creuser le centre pour effet contour (optionnel pour lisibilité)
    d.rectangle([(3, 9), (16, 15)], fill=TRANSPARENT)
    return img

def draw_close(size=16):
    """Croix fine (2 diagonales)."""
    img = new_image(size)
    d = ImageDraw.Draw(img)
    w = 2
    d.line([(4, 4), (11, 11)], fill=WHITE, width=w)
    d.line([(11, 4), (4, 11)], fill=WHITE, width=w)
    return img

def draw_arrow_up(size=18):
    """Flèche vers le haut (envoi)."""
    img = new_image(size)
    d = ImageDraw.Draw(img)
    # Barre verticale
    w = 2
    d.line([(9, 4), (9, 15)], fill=WHITE, width=w)
    # Pointe en v inversé
    d.line([(4, 8), (9, 4)], fill=WHITE, width=w)
    d.line([(9, 4), (14, 8)], fill=WHITE, width=w)
    return img

def draw_arrow_down(size=18):
    """Flèche vers le bas (réception)."""
    img = new_image(size)
    d = ImageDraw.Draw(img)
    w = 2
    d.line([(9, 4), (9, 14)], fill=WHITE, width=w)
    d.line([(4, 10), (9, 14)], fill=WHITE, width=w)
    d.line([(9, 14), (14, 10)], fill=WHITE, width=w)
    return img

def draw_radar(size=48):
    """Radar : cercles concentriques + point central."""
    img = new_image(size)
    d = ImageDraw.Draw(img)
    cx = cy = size // 2
    # 3 cercles concentriques
    for r in (20, 14, 8):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=WHITE, width=2)
    # Point central plein
    d.ellipse([cx - 3, cy - 3, cx + 3, cy + 3], fill=WHITE)
    return img

def draw_qr(size=24):
    """Pictogramme QR : 3 carrés de positionnement + pixels centraux."""
    img = new_image(size)
    d = ImageDraw.Draw(img)
    # 3 coins de positionnement (finder patterns)
    for (x, y) in [(2, 2), (17, 2), (2, 17)]:
        d.rectangle([(x, y), (x + 5, y + 5)], outline=WHITE, width=1)
        d.rectangle([(x + 2, y + 2), (x + 3, y + 3)], fill=WHITE)
    # Quelques pixels centraux pour évoquer les données QR
    for (px, py) in [(11, 10), (13, 10), (10, 12),
                      (14, 13), (12, 14), (15, 15)]:
        d.rectangle([(px, py), (px + 1, py + 1)], fill=WHITE)
    return img

def draw_no_device(size=48):
    """Globe barré : cercle + ligne diagonale."""
    img = new_image(size)
    d = ImageDraw.Draw(img)
    cx = cy = size // 2
    r = 18
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=WHITE, width=2)
    # Barre diagonale type "no"
    d.line([(cx - 13, cy - 13), (cx + 13, cy + 13)], fill=WHITE, width=2)
    return img


ICONS = {
    "check":      (draw_check,      24),
    "folder":     (draw_folder,     20),
    "close":      (draw_close,      16),
    "arrow-up":   (draw_arrow_up,   18),
    "arrow-down": (draw_arrow_down, 18),
    "radar":      (draw_radar,      48),
    "no-device":  (draw_no_device,  48),
    "qr":         (draw_qr,         24),
}

def main():
    count = 0
    for name, (fn, size) in ICONS.items():
        img = fn(size)
        path = os.path.join(OUT, f"{name}.png")
        img.save(path, "PNG")
        count += 1
        print(f"  {path} ({size}×{size})")
    print(f"\n✅ Generated {count} PNG icons in {OUT}")

if __name__ == "__main__":
    main()
