"""
Generateur de textures procedurales (toutes "tileables") pour le jeu Backrooms.

Usage :  python generate_textures.py            (necessite numpy + pillow)
Sortie : ../RawAssets/Textures/*.jpg / *.png

Toutes les textures sont construites a partir de bruit filtre dans le domaine
de Fourier : le resultat est donc periodique (raccord parfait) par construction.
"""
import os
import numpy as np
from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "RawAssets", "Textures")
os.makedirs(OUT, exist_ok=True)


# ---------------------------------------------------------------------------
# Outils de bruit
# ---------------------------------------------------------------------------
def noise(size, beta=2.0, seed=0, fmin=0.0, fmax=None, aniso=(1.0, 1.0)):
    """Bruit 1/f^beta periodique, moyenne 0, ecart-type 1."""
    r = np.random.default_rng(seed)
    w = r.standard_normal((size, size))
    F = np.fft.rfft2(w)
    fy = np.fft.fftfreq(size)[:, None] * size * aniso[1]
    fx = np.fft.rfftfreq(size)[None, :] * size * aniso[0]
    f = np.sqrt(fx ** 2 + fy ** 2)
    f[0, 0] = 1.0
    filt = 1.0 / f ** (beta / 2.0)
    if fmin > 0:
        filt *= 1.0 / (1.0 + np.exp(-(f - fmin) * 2.0))
    if fmax is not None:
        filt *= np.exp(-((f / fmax) ** 2))
    F *= filt
    F[0, 0] = 0
    n = np.fft.irfft2(F, s=(size, size))
    return (n - n.mean()) / (n.std() + 1e-9)


def n01(n, k=3.0):
    return np.clip(0.5 + n / (2.0 * k), 0.0, 1.0)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def blur_wrap(a, radius=1):
    """Flou periodique simple (boite)"""
    out = a.copy()
    for _ in range(radius):
        out = (out + np.roll(out, 1, 0) + np.roll(out, -1, 0) + np.roll(out, 1, 1) + np.roll(out, -1, 1)) / 5.0
    return out


def colorize(base_rgb, *layers):
    """base_rgb: tuple 0..1 ; layers: liste de (masque HxW, rgb_multiplicateur ou rgb_cible, mode)"""
    return np.array(base_rgb, dtype=np.float32)


# Intensite du relief (normal map "<nom>_N.png" generee a partir de la luminance)
NORMAL_STRENGTH = {
    "T_L0_Carpet": 7.0, "T_L0_Ceiling": 5.0, "T_Concrete": 4.0, "T_ConcreteFloor": 5.0,
    "T_ConcreteDark": 4.0, "T_Brick": 8.0, "T_MetalPanel": 4.0, "T_OfficeCarpet": 6.0, "T_OfficeWall": 2.0,
    "T_HotelCarpet": 5.0, "T_HotelWallpaper": 3.0, "T_Wood": 3.0, "T_PoolTile": 9.0, "T_Rock": 10.0, "T_Dirt": 6.0,
    "T_Asphalt": 5.0, "T_Grass": 6.0, "T_Facade": 4.0, "T_Siding": 7.0, "T_Skin": 4.0,
}


def save_normal(name, rgb, strength, size=1024):
    """Normal map (convention : x vers la droite, y vers le bas de l'image) depuis la luminance."""
    lum = rgb.mean(-1) if rgb.ndim == 3 else rgb
    S = lum.shape[0]
    h = blur_wrap(lum, 1)
    # passe-haut : on retire les tres basses frequences (taches) pour ne garder que le relief
    F = np.fft.rfft2(h)
    fy = np.fft.fftfreq(S)[:, None] * S
    fx = np.fft.rfftfreq(S)[None, :] * S
    f = np.sqrt(fx ** 2 + fy ** 2)
    F *= 1.0 - np.exp(-((f / 6.0) ** 2))
    h = np.fft.irfft2(F, s=(S, S))
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    k = strength * S / 512.0
    nx, ny, nz = -dx * k, -dy * k, np.ones_like(h)
    l = np.sqrt(nx ** 2 + ny ** 2 + nz ** 2)
    nrm = np.stack([nx / l, ny / l, nz / l], -1) * 0.5 + 0.5
    img = Image.fromarray((np.clip(nrm, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB")
    if img.size[0] != size:
        img = img.resize((size, size), Image.LANCZOS)
    path = os.path.join(OUT, f"{name}_N.jpg")
    img.save(path, quality=93, optimize=True)
    print("  ->", os.path.relpath(path))


def save(name, rgb, size=None, fmt="jpg"):
    rgb = np.clip(rgb, 0, 1)
    if name in NORMAL_STRENGTH:
        save_normal(name, rgb, NORMAL_STRENGTH[name], 1024 if rgb.shape[0] >= 1024 else 512)
    img = Image.fromarray((rgb * 255 + 0.5).astype(np.uint8), "RGB" if rgb.ndim == 3 else "L")
    if size:
        img = img.resize((size, size), Image.LANCZOS)
    path = os.path.join(OUT, f"{name}.{fmt}")
    if fmt == "jpg":
        img.save(path, quality=90, optimize=True)
    else:
        img.save(path, optimize=True)
    print("  ->", os.path.relpath(path))


def rgb_from(gray, color):
    c = np.array(color, dtype=np.float32)
    return gray[..., None] * c[None, None, :]


def mix(a, b, t):
    if t.ndim == 2:
        t = t[..., None]
    return a * (1 - t) + b * t


def grid_coords(size):
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    return x / size, y / size


# ---------------------------------------------------------------------------
# Niveau 0
# ---------------------------------------------------------------------------
def t_l0_wallpaper():
    """Papier peint du Niveau 0 (modele fourni) : bandes verticales vert-jaune bordees d'un liseré sombre,
    avec des motifs de trois chevrons empiles, decales d'une demi-periode d'une colonne a l'autre.
    Une repetition = 16 colonnes (120 cm dans le jeu, soit des bandes de 7,5 cm)."""
    S = 2048
    x, y = grid_coords(S)
    cols, rows = 16, 5
    light = np.array([185, 180, 98]) / 255.0
    mid = np.array([166, 162, 80]) / 255.0
    dark = np.array([146, 142, 60]) / 255.0
    col = np.floor(x * cols).astype(int)
    u = (x * cols) % 1.0                        # position dans la colonne
    # liseré sombre (bord gauche de chaque colonne), bords adoucis
    border = smoothstep(0.0, 0.03, u) * (1 - smoothstep(0.22, 0.27, u))
    inner_line = np.exp(-((u - 0.125) / 0.025) ** 2)     # trait central un peu plus fonce dans le liseré
    # motif : 3 chevrons "^" dans la bande claire, une rangee sur deux decalee
    shift = (col % 2) * 0.5
    v = (y * rows + shift) % 1.0
    cu = (u - 0.62) / 0.36                     # -1..1 sur la largeur du motif
    inside = np.abs(cu) < 1.0
    motif = np.zeros((S, S))
    edge = np.zeros((S, S))
    for k in range(3):
        c0 = 0.10 + k * 0.075                   # hauteur du sommet de chaque chevron
        d = (v - (c0 + np.abs(cu) * 0.12))      # distance verticale a la ligne du chevron
        band = np.exp(-(d / 0.028) ** 4) * inside
        motif = np.maximum(motif, band)
        edge = np.maximum(edge, np.exp(-((np.abs(d) - 0.03) / 0.006) ** 2) * inside)
    # bords du motif adoucis (pointe arrondie)
    motif *= smoothstep(1.0, 0.82, np.abs(cu))
    img = np.ones((S, S, 3)) * light
    img = mix(img, np.ones_like(img) * dark, border * 0.85)
    img = mix(img, img * 0.93, inner_line * border)
    img = mix(img, np.ones_like(img) * dark * 0.98, motif * 0.92)
    img = mix(img, np.ones_like(img) * mid * 1.08, edge * 0.45 * (1 - border))
    # trame textile verticale + fibres
    weave = noise(S, beta=1.0, seed=21, aniso=(0.25, 4.0))
    img *= (1 + 0.025 * weave)[..., None]
    paper = noise(S, beta=0.6, seed=11)
    img *= (1 + 0.03 * paper)[..., None]
    # decolorations larges et taches d'humidite (discretes : la salete du bas de mur est geree par le materiau)
    blot = noise(S, beta=3.0, seed=12)
    img *= (1 - 0.06 * n01(blot, 2.0))[..., None]
    stain = noise(S, beta=2.6, seed=13)
    ring = smoothstep(1.7, 2.0, stain) - smoothstep(2.2, 2.6, stain)
    inside_st = smoothstep(1.8, 2.6, stain)
    img = mix(img, img * np.array([0.82, 0.76, 0.55]), np.clip(inside_st * 0.3 + ring * 0.35, 0, 1))
    # joints des les (tous les 60 cm = 8 colonnes), legerement decolles
    us = (x * 2) % 1.0
    seam = np.exp(-((us - 0.0) / 0.0012) ** 2) + np.exp(-((us - 1.0) / 0.0012) ** 2)
    img *= (1 - 0.15 * seam)[..., None]
    save("T_L0_Wallpaper", img)
    # relief : motif et bandes legerement embossees, liseré en creux
    height = 0.6 * motif - 0.5 * border + 0.15 * weave / 3.0 - 0.6 * seam
    save_normal("T_L0_Wallpaper", height, 6.0, 1024)


def t_l0_carpet():
    S = 2048
    base = np.array([0.56, 0.47, 0.27])
    fib = noise(S, beta=0.2, seed=21)
    fib = blur_wrap(fib, 1)
    loops = noise(S, beta=1.0, seed=22, fmin=120)
    g = 1 + 0.10 * fib + 0.06 * loops
    img = rgb_from(g, base)
    # zones humides (plus sombres et verdatres)
    damp = noise(S, beta=3.2, seed=23)
    m = smoothstep(0.6, 2.2, damp)
    img = mix(img, img * np.array([0.74, 0.74, 0.62]), m * 0.7)
    # salissures
    dirt = noise(S, beta=2.0, seed=24)
    img *= (1 - 0.08 * n01(dirt))[..., None]
    save("T_L0_Carpet", img)


def t_l0_ceiling():
    S = 2048
    x, y = grid_coords(S)
    base = np.array([0.86, 0.84, 0.77])
    # fissures typiques des dalles acoustiques
    fiss = noise(S, beta=1.2, seed=31, fmin=60)
    specks = smoothstep(1.4, 2.2, fiss)
    g = 1 - 0.28 * specks + 0.03 * noise(S, beta=0.4, seed=32)
    img = rgb_from(g, base)
    # taches d'eau jaunes
    st = noise(S, beta=3.0, seed=33)
    stain = smoothstep(1.2, 1.9, st)
    ring = smoothstep(1.15, 1.25, st) - smoothstep(1.3, 1.5, st)
    img = mix(img, img * np.array([0.88, 0.78, 0.55]), np.clip(stain * 0.5 + ring * 0.5, 0, 1))
    # ossature en T (2x2 dalles de 60 cm)
    u = (x * 2) % 1.0
    v = (y * 2) % 1.0
    bw = 0.012
    bar = ((u < bw) | (u > 1 - bw) | (v < bw) | (v > 1 - bw)).astype(np.float32)
    shadow = (((u > bw) & (u < bw * 2.5)) | ((v > bw) & (v < bw * 2.5))).astype(np.float32)
    img = mix(img, np.ones_like(img) * np.array([0.80, 0.80, 0.77]), bar)
    img *= (1 - 0.15 * shadow)[..., None]
    save("T_L0_Ceiling", img)


# ---------------------------------------------------------------------------
# Generiques
# ---------------------------------------------------------------------------
def t_grime():
    S = 512
    a = noise(S, beta=2.2, seed=41)
    b = noise(S, beta=1.2, seed=42)
    g = 0.78 + 0.16 * np.tanh(a * 0.8) + 0.05 * b
    g = np.clip(g, 0.35, 1.0)
    save("T_Grime", np.repeat(g[..., None], 3, 2))


def t_concrete(name="T_Concrete", seed=50, base=(0.52, 0.51, 0.49), S=1024, wet=False, cracks=False):
    a = noise(S, beta=2.0, seed=seed)
    b = noise(S, beta=0.8, seed=seed + 1)
    pores = smoothstep(2.0, 2.8, noise(S, beta=0.3, seed=seed + 2))
    g = 1 + 0.09 * a + 0.05 * b - 0.35 * pores
    img = rgb_from(g, base)
    stains = noise(S, beta=3.0, seed=seed + 3)
    img *= (1 - 0.18 * smoothstep(0.5, 2.0, stains))[..., None]
    if wet:
        w = smoothstep(0.8, 1.6, noise(S, beta=3.4, seed=seed + 4))
        img = mix(img, img * 0.55, w * 0.9)
    if cracks:
        r = np.abs(noise(S, beta=2.2, seed=seed + 5))
        c = 1 - smoothstep(0.0, 0.035, r)
        img *= (1 - 0.55 * c)[..., None]
    save(name, img)


def t_brick():
    S = 1024
    x, y = grid_coords(S)
    rows, cols = 8, 4
    ry = y * rows
    row = np.floor(ry)
    off = (row % 2) * 0.5
    rx = x * cols + off
    col = np.floor(rx)
    fu, fv = rx - col, ry - row
    mort = 0.045
    mortar = ((fu < mort / 2) | (fu > 1 - mort / 2) | (fv < mort) | (fv > 1 - mort)).astype(np.float32)
    rng = np.random.default_rng(61)
    tbl = rng.random((rows + 1, cols + 2))
    hue = rng.random((rows + 1, cols + 2))
    idx_r = row.astype(int) % rows
    idx_c = (col.astype(int)) % cols
    var = tbl[idx_r, idx_c][..., None]
    hv = hue[idx_r, idx_c][..., None]
    brick = (np.array([0.38, 0.20, 0.13]) * (1 - hv) + np.array([0.26, 0.20, 0.17]) * hv) * (0.65 + 0.55 * var)
    n = noise(S, beta=1.0, seed=62)
    brick *= (1 + 0.12 * n)[..., None]
    soot = smoothstep(0.0, 2.0, noise(S, beta=2.8, seed=63))
    brick *= (1 - 0.5 * soot)[..., None]
    mortar_c = np.ones_like(brick) * np.array([0.42, 0.40, 0.37]) * (1 + 0.1 * n)[..., None]
    img = mix(brick, mortar_c, mortar)
    save("T_Brick", img)


def t_metal_panel():
    S = 512
    x, y = grid_coords(S)
    base = np.array([0.33, 0.37, 0.33])
    n = noise(S, beta=1.4, seed=71)
    img = rgb_from(1 + 0.08 * n, base)
    u, v = (x * 2) % 1.0, (y * 2) % 1.0
    seam = ((u < 0.008) | (u > 0.992) | (v < 0.008) | (v > 0.992)).astype(np.float32)
    img *= (1 - 0.6 * seam)[..., None]
    # rivets
    for cx in (0.05, 0.95):
        for cy in np.linspace(0.05, 0.95, 6):
            d = np.sqrt((u - cx) ** 2 + (v - cy) ** 2)
            img = mix(img, np.ones_like(img) * 0.55, (d < 0.012).astype(np.float32))
    # coulures de rouille verticales
    streak = noise(S, beta=2.0, seed=72, aniso=(1.0, 0.06))
    rust = smoothstep(0.6, 2.2, streak)
    img = mix(img, np.ones_like(img) * np.array([0.40, 0.20, 0.08]), rust * 0.75)
    save("T_MetalPanel", img)


def t_office_carpet():
    S = 512
    x, y = grid_coords(S)
    base = np.array([0.32, 0.36, 0.42])
    fib = blur_wrap(noise(S, beta=0.1, seed=81), 1)
    img = rgb_from(1 + 0.12 * fib, base)
    tx, ty = np.floor(x * 4), np.floor(y * 4)
    checker = ((tx + ty) % 2).astype(np.float32)
    img *= (1 - 0.05 * checker)[..., None]
    dirt = smoothstep(0.5, 2.0, noise(S, beta=3.0, seed=82))
    img *= (1 - 0.2 * dirt)[..., None]
    save("T_OfficeCarpet", img)


def t_office_wall():
    S = 512
    base = np.array([0.80, 0.79, 0.74])
    n = noise(S, beta=0.9, seed=91)
    b = noise(S, beta=2.6, seed=92)
    img = rgb_from(1 + 0.025 * n - 0.04 * n01(b), base)
    save("T_OfficeWall", img)


def t_hotel_carpet():
    S = 512
    x, y = grid_coords(S)
    base = np.array([0.36, 0.05, 0.06])
    k = 4
    a = np.cos(2 * np.pi * k * x) * np.cos(2 * np.pi * k * y)
    b = np.cos(2 * np.pi * k * (x + y)) * np.cos(2 * np.pi * k * (x - y))
    rosette = smoothstep(0.55, 0.75, a) + smoothstep(0.82, 0.95, b) * 0.8
    diamond = smoothstep(0.92, 0.98, np.abs(np.sin(np.pi * k * (x + y))) * np.abs(np.sin(np.pi * k * (x - y))) * 1.02)
    gold = np.array([0.55, 0.38, 0.12])
    img = np.ones((S, S, 3)) * base
    img = mix(img, np.ones_like(img) * gold, np.clip(rosette * 0.85, 0, 1))
    img = mix(img, np.ones_like(img) * np.array([0.15, 0.02, 0.03]), diamond)
    fib = blur_wrap(noise(S, beta=0.1, seed=101), 1)
    img *= (1 + 0.12 * fib)[..., None]
    img *= (1 - 0.25 * smoothstep(0.5, 2.0, noise(S, beta=3.0, seed=102)))[..., None]
    save("T_HotelCarpet", img)


def t_hotel_wallpaper():
    S = 512
    x, y = grid_coords(S)
    base = np.array([0.48, 0.47, 0.33])
    light = np.array([0.62, 0.60, 0.44])
    img = np.ones((S, S, 3)) * base
    # damas : motif polaire sur un reseau decale
    acc = np.zeros((S, S))
    for (ox, oy) in [(0.25, 0.25), (0.75, 0.75)]:
        # repetition 2x2, deux motifs decales (reseau en quinconce)
        px = ((x * 2 - ox * 2 + 1.5) % 1.0) - 0.5
        py = ((y * 2 - oy * 2 + 1.5) % 1.0) - 0.5
        r = np.sqrt(px ** 2 + (py * 0.7) ** 2)
        th = np.arctan2(py, px)
        petal = 0.18 + 0.09 * np.cos(4 * th) + 0.04 * np.cos(8 * th)
        acc = np.maximum(acc, smoothstep(0.012, 0.0, r - petal) - smoothstep(0.03, 0.0, r - petal * 0.55) * 0.6)
    img = mix(img, np.ones_like(img) * light, np.clip(acc, 0, 1))
    img *= (1 + 0.03 * noise(S, beta=0.6, seed=111))[..., None]
    img *= (1 - 0.12 * smoothstep(0.5, 2.2, noise(S, beta=3.0, seed=112)))[..., None]
    save("T_HotelWallpaper", img)


def t_wood():
    S = 512
    x, y = grid_coords(S)
    planks = 6
    p = np.floor(x * planks)
    rng = np.random.default_rng(121)
    pv = rng.random(planks + 1)[p.astype(int) % planks]
    grain_n = noise(S, beta=2.0, seed=122, aniso=(1.0, 0.08))
    rings = 0.5 + 0.5 * np.sin((x * planks * 7 + grain_n * 1.5 + pv * 10) * 2 * np.pi)
    base = np.array([0.30, 0.17, 0.09])
    g = 0.75 + 0.25 * rings + 0.1 * (pv - 0.5)
    img = rgb_from(g, base)
    fu = (x * planks) % 1.0
    seam = ((fu < 0.01) | (fu > 0.99)).astype(np.float32)
    img *= (1 - 0.6 * seam)[..., None]
    save("T_Wood", img)


def t_pool_tile():
    S = 1024
    x, y = grid_coords(S)
    n_t = 10
    u, v = (x * n_t) % 1.0, (y * n_t) % 1.0
    gw = 0.045
    grout = ((u < gw) | (u > 1 - gw) | (v < gw) | (v > 1 - gw)).astype(np.float32)
    rng = np.random.default_rng(131)
    tv = rng.random((n_t, n_t))[np.floor(y * n_t).astype(int) % n_t, np.floor(x * n_t).astype(int) % n_t]
    tile = np.ones((S, S, 3)) * np.array([0.90, 0.93, 0.94]) * (0.97 + 0.04 * tv)[..., None]
    # bord legerement plus sombre (biseau)
    edge = np.minimum(np.minimum(u, 1 - u), np.minimum(v, 1 - v))
    tile *= (0.93 + 0.07 * smoothstep(gw, gw + 0.08, edge))[..., None]
    g = np.ones_like(tile) * np.array([0.72, 0.76, 0.76])
    img = mix(tile, g, grout)
    img *= (1 - 0.05 * smoothstep(0.5, 2.0, noise(S, beta=3.0, seed=132)))[..., None]
    save("T_PoolTile", img)


def t_rock():
    S = 512
    a = noise(S, beta=2.4, seed=141)
    b = noise(S, beta=1.2, seed=142)
    strata = np.sin((grid_coords(S)[1] * 9 + a * 0.25) * 2 * np.pi)
    g = 1 + 0.18 * a + 0.08 * b + 0.06 * strata
    img = rgb_from(g, (0.33, 0.29, 0.25))
    ridge = 1 - smoothstep(0.0, 0.06, np.abs(noise(S, beta=2.0, seed=143)))
    img *= (1 - 0.4 * ridge)[..., None]
    save("T_Rock", img)


def t_dirt():
    S = 512
    a = noise(S, beta=2.0, seed=151)
    pebbles = smoothstep(1.6, 2.2, noise(S, beta=0.5, seed=152))
    img = rgb_from(1 + 0.15 * a, (0.36, 0.28, 0.19))
    img = mix(img, np.ones_like(img) * np.array([0.50, 0.46, 0.40]), pebbles * 0.8)
    save("T_Dirt", img)


def t_asphalt():
    S = 512
    a = noise(S, beta=1.8, seed=161)
    agg = noise(S, beta=0.0, seed=162)
    img = rgb_from(1 + 0.08 * a + 0.12 * np.tanh(agg), (0.17, 0.17, 0.18))
    r = np.abs(noise(S, beta=2.2, seed=163))
    img *= (1 - 0.6 * (1 - smoothstep(0.0, 0.03, r)))[..., None]
    save("T_Asphalt", img)


def t_grass():
    S = 512
    blades = noise(S, beta=0.6, seed=171, aniso=(1.0, 0.35))
    patches = noise(S, beta=3.0, seed=172)
    img = rgb_from(1 + 0.18 * blades, (0.20, 0.30, 0.11))
    img = mix(img, img * np.array([1.25, 1.1, 0.8]), smoothstep(0.0, 2.0, patches) * 0.6)
    save("T_Grass", img)


def t_facade():
    S = 1024
    x, y = grid_coords(S)
    wx, wy = 4, 2
    u, v = (x * wx) % 1.0, (y * wy) % 1.0
    ci, ri = np.floor(x * wx).astype(int), np.floor(y * wy).astype(int)
    rng = np.random.default_rng(181)
    lit = rng.random((wy, wx)) < 0.3
    win = ((u > 0.18) & (u < 0.82) & (v > 0.22) & (v < 0.78)).astype(np.float32)
    frame = ((u > 0.15) & (u < 0.85) & (v > 0.19) & (v < 0.81)).astype(np.float32) - win
    wall = rgb_from(1 + 0.06 * noise(S, beta=1.5, seed=182), (0.55, 0.53, 0.50))
    glass_grad = 0.6 + 0.4 * (1 - v)
    glass = np.ones_like(wall) * np.array([0.12, 0.15, 0.19]) * glass_grad[..., None]
    litc = np.ones_like(wall) * np.array([0.85, 0.72, 0.42])
    glass = mix(glass, litc, lit[ri % wy, ci % wx].astype(np.float32) * 0.9)
    img = mix(wall, glass, win)
    img = mix(img, np.ones_like(img) * 0.25, frame)
    img *= (1 - 0.15 * smoothstep(0.5, 2.0, noise(S, beta=2.0, seed=183, aniso=(1, 0.2))))[..., None]
    save("T_Facade", img)


def t_siding():
    S = 512
    x, y = grid_coords(S)
    planks = 10
    v = (y * planks) % 1.0
    shade = 0.82 + 0.18 * v
    gap = (v < 0.05).astype(np.float32)
    img = rgb_from(shade * (1 + 0.03 * noise(S, beta=1.0, seed=191)), (0.72, 0.76, 0.78))
    img *= (1 - 0.5 * gap)[..., None]
    img *= (1 - 0.15 * smoothstep(0.5, 2.0, noise(S, beta=2.6, seed=192)))[..., None]
    save("T_Siding", img)


def t_glitch():
    S = 512
    rng = np.random.default_rng(201)
    img = np.zeros((S, S, 3))
    for _ in range(260):
        w, h = rng.integers(8, 160), rng.integers(2, 40)
        x0, y0 = rng.integers(0, S), rng.integers(0, S)
        col = rng.random(3) ** 0.7
        xs = (np.arange(x0, x0 + w) % S)
        ys = (np.arange(y0, y0 + h) % S)
        img[np.ix_(ys, xs)] = col
    scan = (np.arange(S) % 4 < 2).astype(np.float32)[:, None, None]
    img = img * (0.75 + 0.25 * scan)
    img = 0.5 * img + 0.5 * np.roll(img, 7, axis=1)
    save("T_Glitch", img)


def t_skin():
    S = 512
    a = noise(S, beta=2.6, seed=211)
    b = noise(S, beta=1.0, seed=212)
    veins = 1 - smoothstep(0.0, 0.05, np.abs(noise(S, beta=2.4, seed=213)))
    g = 0.80 + 0.06 * a + 0.04 * b - 0.12 * veins
    save("T_Skin", np.repeat(np.clip(g, 0, 1)[..., None], 3, 2))


def t_water_normal():
    S = 512
    h = noise(S, beta=3.0, seed=221) * 0.6 + noise(S, beta=2.0, seed=222) * 0.4
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    k = 0.6
    nx, ny, nz = -dx * k, -dy * k, np.ones_like(h)
    l = np.sqrt(nx ** 2 + ny ** 2 + nz ** 2)
    nrm = np.stack([nx / l, ny / l, nz / l], -1)
    save("T_WaterNormal", nrm * 0.5 + 0.5, fmt="png")


def t_paper():
    S = 256
    g = 1 + 0.03 * noise(S, beta=0.8, seed=231) - 0.08 * n01(noise(S, beta=3, seed=232))
    img = rgb_from(g, (0.88, 0.85, 0.74))
    x, y = grid_coords(S)
    lines = (((y * 18) % 1.0) < 0.06).astype(np.float32) * (y > 0.12)
    img = mix(img, img * np.array([0.6, 0.7, 0.9]), lines * 0.5)
    save("T_Paper", img)


def t_caustics():
    """Reseau lumineux de caustiques (cellules de Voronoi periodiques)"""
    S = 512
    rng = np.random.default_rng(241)
    pts = rng.random((40, 2))
    x, y = grid_coords(S)
    d1 = np.full((S, S), 9.0)
    d2 = np.full((S, S), 9.0)
    for px, py in pts:
        for ox in (-1, 0, 1):
            for oy in (-1, 0, 1):
                d = np.sqrt((x - px - ox) ** 2 + (y - py - oy) ** 2)
                d2 = np.minimum(d2, np.maximum(d1, d))
                d1 = np.minimum(d1, d)
    edge = d2 - d1
    c = (1.0 - smoothstep(0.0, 0.035, edge)) ** 1.5
    c = blur_wrap(c, 2)
    c = c / c.max()
    save("T_Caustics", np.repeat(c[..., None], 3, 2))


def t_lens_dirt():
    S = 1024
    img = np.zeros((S, S))
    rng = np.random.default_rng(251)
    x, y = grid_coords(S)
    for _ in range(140):
        cx, cy, r = rng.random(), rng.random(), rng.uniform(0.004, 0.05)
        d = np.sqrt((x - cx) ** 2 + (y - cy) ** 2)
        img += np.exp(-(d / r) ** 2) * rng.uniform(0.05, 0.35)
    smudge = smoothstep(0.8, 2.5, noise(S, beta=2.5, seed=252, aniso=(1.0, 0.4)))
    img = np.clip(img + smudge * 0.35, 0, 1)
    save("T_LensDirt", np.repeat(img[..., None], 3, 2))


def t_flat_normal():
    save("T_FlatNormal", np.ones((64, 64, 3)) * np.array([0.5, 0.5, 1.0]), fmt="png")


if __name__ == "__main__":
    print("Generation des textures dans", os.path.abspath(OUT))
    t_l0_wallpaper()
    t_l0_carpet()
    t_l0_ceiling()
    t_grime()
    t_concrete("T_Concrete", 50, (0.52, 0.51, 0.49))
    t_concrete("T_ConcreteFloor", 55, (0.40, 0.39, 0.37), wet=True, cracks=True)
    t_concrete("T_ConcreteDark", 58, (0.20, 0.20, 0.21), S=512, cracks=True)
    t_brick()
    t_metal_panel()
    t_office_carpet()
    t_office_wall()
    t_hotel_carpet()
    t_hotel_wallpaper()
    t_wood()
    t_pool_tile()
    t_rock()
    t_dirt()
    t_asphalt()
    t_grass()
    t_facade()
    t_siding()
    t_glitch()
    t_skin()
    t_water_normal()
    t_paper()
    t_caustics()
    t_lens_dirt()
    t_flat_normal()
    print("Termine.")
