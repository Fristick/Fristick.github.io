"""
v4.17 (accessibilite, agent Accessibility Auditor : Docs/v417/agents/accessibilite.md, M2) : contour clair autour des icones
d'inventaire sombres.

Les icones rendues par Tools/Blender/generate_models.py (render_icon) sont des objets sombres pour certaines (camescope, lampe,
frontale, piles, cassette) : sur le fond presque noir des cases (environ (27, 25, 15), luminance 0,009), leur luminance mediane
donne 1,7 a 2,6:1, sous le seuil de 3:1 des elements graphiques (WCAG 1.4.11). Ce script ajoute derriere l'objet un contour clair
et chaud de RING pixels (sur 256), qui le detache du fond (environ 10:1 contre le fond, 5:1 contre l'objet). Un objet qui
touche le bord de l'image est d'abord reduit au centre pour que le contour tienne.

Usage :
    python Tools/outline_icons.py            ajoute le contour aux icones sombres (deja traitees : ignorees)
    python Tools/outline_icons.py --check    liste les icones et leur contraste, sans rien ecrire (code 1 si une icone sombre
                                             n'a pas de contour)

A relancer apres generate_models.py (qui rend les icones sans contour). Chaque icone traitee porte la marque "BR-Outline" dans
ses metadonnees PNG : le script ne la retraite pas. Les icones sont reimportees par backrooms_setup.py (REICONED).
"""
import glob
import os
import sys

from PIL import Image, ImageFilter
from PIL.PngImagePlugin import PngInfo

HERE = os.path.dirname(os.path.abspath(__file__))
ICONS = os.path.normpath(os.path.join(HERE, "..", "RawAssets", "Icons"))
# Fond d'une case d'inventaire mesure sur les captures (luminance relative) et seuil des elements graphiques
SLOT_BG = 0.009
MIN_CONTRAST = 3.0
# Contour : epaisseur (pixels sur 256), couleur (sRGB, blanc chaud des libelles)
RING = 5
RING_COLOR = (214, 200, 160)
MARK = "BR-Outline"
# La silhouette du personnage n'est pas une icone d'objet (dessinee sur le panneau, non dans une case)
SKIP = {"I_Silhouette"}


def lin(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def median_luminance(im):
    px = im.load()
    w, h = im.size
    values = []
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            r, g, b, a = px[x, y]
            if a > 200:
                values.append(0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b))
    values.sort()
    return values[len(values) // 2] if values else 1.0


def contrast(lum):
    return (lum + 0.05) / (SLOT_BG + 0.05)


def fit_margin(im):
    """Objet qui touche le bord (piles, cassette) : reduit au centre pour laisser la place du contour"""
    bbox = im.getchannel("A").getbbox()
    if not bbox:
        return im
    w, h = im.size
    margin = min(bbox[0], bbox[1], w - bbox[2], h - bbox[3])
    need = RING + 2
    if margin >= need:
        return im
    scale = (w - 2 * need) / float(w - 2 * margin)
    small = im.resize((max(1, round(w * scale)), max(1, round(h * scale))), Image.LANCZOS)
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    out.paste(small, ((w - small.size[0]) // 2, (h - small.size[1]) // 2))
    return out


def outline(im):
    im = fit_margin(im)
    alpha = im.getchannel("A")
    grown = alpha.filter(ImageFilter.MaxFilter(2 * RING + 1)).filter(ImageFilter.GaussianBlur(1.0))
    ring = Image.new("RGBA", im.size, RING_COLOR + (0,))
    ring.putalpha(grown)
    return Image.alpha_composite(ring, im)


def main():
    check = "--check" in sys.argv[1:]
    missing = 0
    for path in sorted(glob.glob(os.path.join(ICONS, "I_*.png"))):
        name = os.path.splitext(os.path.basename(path))[0]
        if name in SKIP:
            continue
        im = Image.open(path)
        done = im.info.get(MARK) == "1"
        im = im.convert("RGBA")
        c = contrast(median_luminance(im))
        dark = c < MIN_CONTRAST
        bbox = im.getchannel("A").getbbox() or (0, 0, 0, 0)
        margin = min(bbox[0], bbox[1], im.size[0] - bbox[2], im.size[1] - bbox[3])
        state = "contour" if done else ("SANS CONTOUR" if dark else "-")
        print("%-16s mediane %.2f:1  %s%s" % (name, c, state, "  (marge %d px)" % margin if dark else ""))
        if not dark or done:
            continue
        if check:
            missing += 1
            continue
        info = PngInfo()
        info.add_text(MARK, "1")
        outline(im).save(path, pnginfo=info)
        print("  ecrit : %s" % os.path.relpath(path, os.path.join(HERE, "..")))
    if check and missing:
        print("%d icone(s) sombre(s) sans contour" % missing)
        sys.exit(1)


if __name__ == "__main__":
    main()
