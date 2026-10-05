"""v4.7 : cartes de rugosite (<Texture>_R.png) derivees des textures livrees, sans les modifier.

Chaque carte est RELATIVE a la rugosite nominale de la surface (FBRSurface::Roughness, propre a chaque niveau) :
    rugosite = sat(Roughness + (R - 0.5) * 2 * RoughContrast)     (RoughContrast = 1 quand la carte existe)
Une carte grise a 0,5 ne change donc rien. Les valeurs ci-dessous sont des rugosites ABSOLUES pour la valeur nominale
indiquee (celle du Niveau 0, 1 ou 37) ; le script les convertit en ecarts.

Les masques viennent de la couleur de base et de la normal map livrees (joints, rivets, rouille, taches d'humidite,
bouches). Les fichiers sont des PNG en niveaux de gris, a importer en lineaire (sRGB desactive) : voir
import_textures() dans Content/Python/backrooms_setup.py.

    python3 Tools/generate_roughness.py            # ecrit RawAssets/Textures/*_R.png et Docs/v47/rugosite_*.jpg
    python3 Tools/generate_roughness.py --check    # verifie que les cartes livrees correspondent au script
"""
import argparse
import hashlib
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TEX = os.path.join(ROOT, "RawAssets", "Textures")
DOCS = os.path.join(ROOT, "Docs", "v47")


def load_rgb(name, size):
    im = Image.open(os.path.join(TEX, name + ".jpg")).convert("RGB")
    if im.size != (size, size):
        im = im.resize((size, size), Image.LANCZOS)
    return np.asarray(im).astype(np.float32) / 255.0


def load_normal(name, size):
    path = os.path.join(TEX, name + "_N.png")
    if not os.path.isfile(path):
        return None
    im = Image.open(path).convert("RGB")
    if im.size != (size, size):
        im = im.resize((size, size), Image.LANCZOS)
    n = np.asarray(im).astype(np.float32) / 255.0 * 2.0 - 1.0
    return n


def blur(a, radius):
    im = Image.fromarray(np.clip(a * 255.0, 0, 255).astype(np.uint8))
    return np.asarray(im.filter(ImageFilter.GaussianBlur(radius))).astype(np.float32) / 255.0


def lum(rgb):
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def sat(rgb):
    mx = rgb.max(axis=-1)
    mn = rgb.min(axis=-1)
    return np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0.0)


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def fbm(size, seed, scale=64.0, octaves=4):
    """Bruit fractal periodique (raccord parfait : les textures se repetent)"""
    rng = np.random.default_rng(seed)
    out = np.zeros((size, size), np.float32)
    amp = 1.0
    total = 0.0
    cells = max(2, int(size / scale))
    for _ in range(octaves):
        grid = rng.random((cells, cells)).astype(np.float32)
        # Raccord parfait : la grille est tuilee 3 x 3 avant l'agrandissement, puis on garde la tuile du centre
        tile = np.tile(grid, (3, 3))
        im = Image.fromarray((tile * 255).astype(np.uint8)).resize((size * 3, size * 3), Image.BICUBIC)
        a = np.asarray(im).astype(np.float32)[size:2 * size, size:2 * size] / 255.0
        out += a * amp
        total += amp
        amp *= 0.5
        cells *= 2
    return out / total


def curvature(n):
    """Creux et aretes a partir de la normal map : divergence des pentes (positif = creux)"""
    if n is None:
        return None
    dx = np.roll(n[..., 0], -1, axis=1) - np.roll(n[..., 0], 1, axis=1)
    dy = np.roll(n[..., 1], -1, axis=0) - np.roll(n[..., 1], 1, axis=0)
    return dx + dy


# --------------------------------------------------------------------------------------------------------------------
# Regles par texture : rugosite absolue (pour la valeur nominale indiquee)
# --------------------------------------------------------------------------------------------------------------------
def r_l0_wallpaper(size):
    rgb = load_rgb("T_L0_Wallpaper", size)
    L = lum(rgb)
    low = blur(L, size / 64)
    # Taches d'humidite : zones plus sombres que leur voisinage a grande echelle -> papier humide, un peu satine
    big = blur(L, size / 16)
    damp = smooth(0.015, 0.07, big - blur(L, size / 128))
    # Motifs imprimes (chevrons, rayures) : l'encre est un peu plus lisse que le papier
    ink = smooth(0.02, 0.08, np.abs(L - low))
    fiber = fbm(size, 11, scale=6, octaves=2) - 0.5
    r = 0.80 - 0.06 * ink - 0.22 * damp + 0.05 * fiber
    return r, 0.80


def r_l0_carpet(size):
    rgb = load_rgb("T_L0_Carpet", size)
    L = lum(rgb)
    big = blur(L, size / 24)
    # Moquette humide : zones plus sombres a grande echelle, legerement lustrees (fibres collees)
    damp = smooth(0.01, 0.05, blur(L, size / 6) - big + 0.02)
    fiber = fbm(size, 21, scale=3, octaves=2) - 0.5
    r = 0.95 - 0.18 * damp + 0.04 * fiber
    return r, 0.95


def r_l0_ceiling(size):
    rgb = load_rgb("T_L0_Ceiling", size)
    L = lum(rgb)
    S = sat(rgb)
    # Ossature metallique (lignes grises, peu saturees) : bien plus lisse que les dalles en fibre minerale
    grid = smooth(0.10, 0.04, S) * smooth(0.45, 0.65, L)
    grid = np.maximum(grid, 0.0)
    # Aureoles d'infiltration : la fibre gonflee reste mate, un peu plus rugueuse
    stain = smooth(0.18, 0.30, S)
    pores = fbm(size, 31, scale=4, octaves=2) - 0.5
    r = 0.92 - 0.50 * grid + 0.03 * stain + 0.04 * pores
    return r, 0.90


def r_concrete_floor(size):
    rgb = load_rgb("T_ConcreteFloor", size)
    n = load_normal("T_ConcreteFloor", size)
    L = lum(rgb)
    # Taches sombres (huile, eau) : beton lisse et gras ; pores (creux de la normal map) : rugueux ; usure : polie
    oil = smooth(0.03, 0.12, blur(L, size / 4) - L)
    cav = curvature(n)
    pores = smooth(0.05, 0.25, cav) if cav is not None else 0.0
    polish = smooth(0.45, 0.75, fbm(size, 41, scale=size / 6, octaves=3))
    r = 0.62 - 0.28 * oil + 0.18 * pores - 0.10 * polish
    return r, 0.55


def r_concrete(size):
    rgb = load_rgb("T_Concrete", size)
    n = load_normal("T_Concrete", size)
    L = lum(rgb)
    cav = curvature(n)
    pores = smooth(0.05, 0.25, cav) if cav is not None else 0.0
    trowel = fbm(size, 51, scale=size / 10, octaves=3) - 0.5
    r = 0.86 + 0.08 * pores + 0.10 * trowel - 0.05 * smooth(0.2, 0.05, L)
    return r, 0.85


def r_metal_panel(size):
    rgb = load_rgb("T_MetalPanel", size)
    L = lum(rgb)
    R, G, B = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    # Rouille (orange) : tres rugueuse ; rivets (points clairs) : metal poli ; joints (traits sombres) : sale
    rust = smooth(0.04, 0.16, R - G) * smooth(0.02, 0.10, R - B)
    rivet = smooth(0.55, 0.75, L) * smooth(0.25, 0.10, sat(rgb))
    seam = smooth(0.16, 0.08, L)
    scratches = fbm(size, 61, scale=3, octaves=2) - 0.5
    r = 0.45 + 0.38 * rust - 0.20 * rivet + 0.20 * seam + 0.06 * scratches
    return r, 0.45


def r_pool_tile37(size):
    rgb = load_rgb("T_PoolTile37", size)
    L = lum(rgb)
    # Joints gris (plus sombres que l'email) : poreux ; email : brillant, a peine ondule
    grout = smooth(0.80, 0.62, L)
    glaze = fbm(size, 71, scale=size / 8, octaves=3) - 0.5
    edge = blur(grout, 1.5)
    r = 0.10 + 0.70 * grout + 0.10 * np.clip(edge - grout, 0, 1) + 0.04 * glaze
    return r, 0.10


def r_plaster(size):
    rgb = load_rgb("T_Plaster", size)
    L = lum(rgb)
    strokes = fbm(size, 81, scale=size / 12, octaves=4) - 0.5
    r = 0.55 + 0.14 * strokes + 0.05 * (blur(L, size / 64) - L) * 10
    return r, 0.55


def r_creature(name, nominal, seed):
    def rule(size):
        rgb = load_rgb(name, size)
        L = lum(rgb)
        R, G, B = rgb[..., 0], rgb[..., 1], rgb[..., 2]
        # Chair humide (bouches, gencives, plaies : rouge sombre) : luisante ; dents/ongles (clairs, peu satures) :
        # mi-brillants ; peau : grain variable (pores, zones grasses) ; fond de l'atlas (noir) : neutre
        wet = smooth(0.06, 0.22, R - 0.5 * (G + B)) * smooth(0.65, 0.25, L)
        teeth = smooth(0.70, 0.85, L) * smooth(0.18, 0.08, sat(rgb))
        oil = fbm(size, seed, scale=size / 24, octaves=3) - 0.5
        pores = fbm(size, seed + 1, scale=2, octaves=2) - 0.5
        r = nominal + 0.18 * oil + 0.08 * pores - (nominal - 0.18) * wet - (nominal - 0.32) * teeth
        background = L < 0.02
        r = np.where(background, nominal, r)
        return r, nominal
    return rule


# Nom de la texture -> (regle, resolution de la carte). Resolution choisie selon la distance d'observation :
# murs, sols et plafonds vus de 1 a 6 m (1024), carrelage 512 (comme sa couleur), peaux des entites 1024.
MAPS = {
    "T_L0_Wallpaper": (r_l0_wallpaper, 1024),
    "T_L0_Carpet": (r_l0_carpet, 1024),
    "T_L0_Ceiling": (r_l0_ceiling, 1024),
    "T_ConcreteFloor": (r_concrete_floor, 1024),
    "T_Concrete": (r_concrete, 1024),
    "T_MetalPanel": (r_metal_panel, 512),
    "T_PoolTile37": (r_pool_tile37, 512),
    "T_Plaster": (r_plaster, 1024),
    "T_Wretch": (r_creature("T_Wretch", 0.55, 91), 1024),
    "T_Clump": (r_creature("T_Clump", 0.45, 101), 1024),
}


def encode(r_abs, nominal):
    rel = 0.5 + (np.clip(r_abs, 0.02, 1.0) - nominal) / 2.0
    return np.clip(np.round(rel * 255.0), 0, 255).astype(np.uint8)


def build(name):
    rule, size = MAPS[name]
    r_abs, nominal = rule(size)
    return encode(r_abs, nominal), r_abs, nominal


def preview(name, r_abs):
    os.makedirs(DOCS, exist_ok=True)
    base = Image.open(os.path.join(TEX, name + ".jpg")).convert("RGB").resize((384, 384), Image.LANCZOS)
    rough = Image.fromarray(np.clip(r_abs * 255, 0, 255).astype(np.uint8)).resize((384, 384), Image.LANCZOS).convert("RGB")
    sheet = Image.new("RGB", (768, 384))
    sheet.paste(base, (0, 0))
    sheet.paste(rough, (384, 0))
    return sheet


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="verifie les cartes livrees sans rien ecrire")
    args = ap.parse_args()
    bad = 0
    sheets = []
    for name in MAPS:
        data, r_abs, nominal = build(name)
        path = os.path.join(TEX, name + "_R.png")
        if args.check:
            if not os.path.isfile(path):
                print("MANQUE  %s" % path)
                bad += 1
                continue
            have = np.asarray(Image.open(path).convert("L"))
            diff = int(np.abs(have.astype(int) - data.astype(int)).max())
            print("%-7s %s_R.png  ecart max %d/255" % ("OK" if diff <= 1 else "DIFF", name, diff))
            bad += 0 if diff <= 1 else 1
            continue
        Image.fromarray(data, "L").save(path, optimize=True)
        print("%s_R.png  %dx%d  nominale %.2f  rugosite %.2f a %.2f (moyenne %.2f)  %d Ko" % (
            name, data.shape[1], data.shape[0], nominal, float(np.clip(r_abs, 0.02, 1).min()), float(np.clip(r_abs, 0.02, 1).max()),
            float(np.clip(r_abs, 0.02, 1).mean()), os.path.getsize(path) // 1024))
        sheets.append(preview(name, r_abs))
    if sheets:
        cols = 2
        rows = (len(sheets) + cols - 1) // cols
        board = Image.new("RGB", (768 * cols, 384 * rows), (20, 20, 20))
        for i, s in enumerate(sheets):
            board.paste(s, ((i % cols) * 768, (i // cols) * 384))
        out = os.path.join(DOCS, "rugosite_cartes.jpg")
        board.save(out, quality=86)
        print("Planche : %s (couleur de base | rugosite absolue, blanc = mat)" % os.path.relpath(out, ROOT))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
