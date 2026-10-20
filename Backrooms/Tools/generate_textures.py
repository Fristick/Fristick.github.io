"""
Generateur de textures procedurales (toutes "tileables") pour le jeu Backrooms.

Usage :  python generate_textures.py            (necessite numpy + pillow)
         python generate_textures.py T_L0_Wallpaper T_L0_Carpet     (v4.15 : seulement ces textures)
Sortie : ../RawAssets/Textures/*.jpg / *.png

Toutes les textures sont construites a partir de bruit filtre dans le domaine
de Fourier : le resultat est donc periodique (raccord parfait) par construction.
"""
import os
import sys

import numpy as np
from PIL import Image

import protect_assets

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
# v4.15 : la moquette et les dalles du Niveau 0 ont leur propre carte de hauteur (boucles, fissures, ossature)
NORMAL_STRENGTH = {
    "T_Concrete": 4.0, "T_ConcreteFloor": 5.0,
    "T_ConcreteDark": 4.0, "T_Brick": 8.0, "T_MetalPanel": 4.0, "T_OfficeCarpet": 6.0, "T_OfficeWall": 2.0,
    "T_HotelCarpet": 5.0, "T_HotelWallpaper": 3.0, "T_Wood": 3.0, "T_Rock": 10.0, "T_Dirt": 6.0,
    "T_Asphalt": 5.0, "T_Grass": 6.0, "T_Facade": 4.0, "T_Siding": 7.0, "T_Skin": 4.0, "T_Hazard": 3.0,
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
    save_normal_png(name, img)


def save_normal_png(name, img):
    """Normal map sans perte (PNG) : le JPEG laisse des blocs visibles dans les reflets des surfaces brillantes"""
    path = os.path.join(OUT, f"{name}_N.png")
    img.save(path, optimize=True)
    old = os.path.join(OUT, f"{name}_N.jpg")
    if os.path.exists(old):
        os.remove(old)
    print("  ->", os.path.relpath(path))


def save(name, rgb, size=None, fmt="jpg"):
    rgb = np.clip(rgb, 0, 1)
    if name in NORMAL_STRENGTH:
        save_normal(name, rgb, NORMAL_STRENGTH[name], rgb.shape[0])  # meme resolution que la couleur
    img = Image.fromarray((rgb * 255 + 0.5).astype(np.uint8), "RGB" if rgb.ndim == 3 else "L")
    if size:
        img = img.resize((size, size), Image.LANCZOS)
    path = os.path.join(OUT, f"{name}.{fmt}")
    if protect_assets.is_protected(path):
        print("  PROTEGE, non remplace :", name)  # v4.5 : texture d'un modele ou d'une scene fournis
        return
    if fmt == "jpg":
        # qualite 95 sans sous-echantillonnage des couleurs : indiscernable de l'original une fois compresse par Unreal
        img.save(path, quality=95, subsampling=0, optimize=True)
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
def desaturate(rgb255, k):
    """v4.15 : couleur (0..255) rapprochee de son gris de la part k (jaune plus naturel, moins sature)"""
    c = np.array(rgb255, dtype=np.float64) / 255.0
    g = float(c @ np.array([0.299, 0.587, 0.114]))
    return c * (1.0 - k) + g * k


def wrap_dist(coord_px, period_px):
    """Distance (px) au multiple de period_px le plus proche (texture periodique)"""
    m = coord_px % period_px
    return np.minimum(m, period_px - m)


def column_noise(S, seed, beta=2.0, fmax=24.0):
    """Bruit 1D periodique et doux (une valeur par pixel), ecart-type 1 : spectre en 1/f^beta coupe au-dela de fmax
    periodes par tuile"""
    r = np.random.default_rng(seed)
    F = np.fft.rfft(r.standard_normal(S))
    f = np.fft.rfftfreq(S) * S
    f[0] = 1.0
    F *= (1.0 / f ** (beta / 2.0)) * np.exp(-((f / fmax) ** 2))
    F[0] = 0
    n = np.fft.irfft(F, n=S)
    return (n - n.mean()) / (n.std() + 1e-9)


def t_l0_wallpaper():
    """Papier peint du Niveau 0 (modele fourni) : bandes verticales vert-jaune bordees d'un liseré sombre,
    avec des motifs de trois chevrons empiles, decales d'une demi-periode d'une colonne a l'autre.
    v4.15 (realisme) : une repetition = 106 cm (FBRSurface::Scale du mur), soit deux les de 53 cm (largeur d'un rouleau
    europeen) de 7 colonnes de 7,6 cm ; raccord droit, le motif continue d'un le a l'autre (4 rangees de 26,5 cm).
    Raccords des les : joint d'un pixel a peine ouvert et bord du le de droite souleve par endroits (relief de la normal
    map, 1 px ~ 0,5 mm) ; chaque le a son ton (+-1,2 %), variations douces de +-6 % ; couleurs 20 % moins saturees.
    Plus de taches dessinees dans la texture (elles se repetaient a chaque tuile) : la ligne de maree au-dessus de la
    plinthe et la bande sous le plafond sont dans le materiau (hauteur monde), les aureoles sont des decalques.
    v4.17 (direction artistique, Docs/v417/agents/direction_artistique.md, points 1 et 11) : les murs etaient la surface
    la plus claire du Level 0 (1,1 a 2,5 fois la moquette) : bandes claires ~21 % moins lumineuses et tirees vers
    l'ocre-olive (teinte 57 -> 52 degres), ecart bande / lisere reduit de 30 % ; embossage des chevrons et trame deux fois
    plus discrets ; joint des les de 1,8 mm (3,5 px, avant 1 px) un peu encrasse ; ton de chaque le +-2,5 % (avant 1,2)."""
    S = 2048
    x, y = grid_coords(S)
    rolls, per_roll, rows = 2, 7, 4
    cols = rolls * per_roll
    light_v416 = desaturate([185, 180, 98], 0.2)
    light = desaturate([170, 159, 92], 0.2)
    # v4.17 : lisere et teinte intermediaire gardent leur rapport a la bande claire, ecart reduit de 30 %
    mid = [light[i] * (1.0 - 0.7 * (1.0 - desaturate([166, 162, 80], 0.2)[i] / light_v416[i])) for i in range(3)]
    dark = [light[i] * (1.0 - 0.7 * (1.0 - desaturate([146, 142, 60], 0.2)[i] / light_v416[i])) for i in range(3)]
    col = np.floor(x * cols).astype(int)
    u = (x * cols) % 1.0                        # position dans la colonne
    # liseré sombre (bord gauche de chaque colonne), bords adoucis
    border = smoothstep(0.0, 0.03, u) * (1 - smoothstep(0.22, 0.27, u))
    inner_line = np.exp(-((u - 0.125) / 0.025) ** 2)     # trait central un peu plus fonce dans le liseré
    # motif : 3 chevrons "^" dans la bande claire, une rangee sur deux decalee (7 colonnes par le : la parite
    # alterne d'un le a l'autre comme dans une seule bande continue)
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
    # ton propre a chaque le (rouleaux d'un meme lot, un peu passes) et variations douces (+-6 %), trainees verticales
    roll = np.floor(x * rolls).astype(int)
    img *= np.where(roll == 0, 1.025, 0.975)[..., None]
    blot = noise(S, beta=3.0, seed=12)
    img *= (1 + 0.06 * np.clip(blot / 2.0, -1.0, 1.0))[..., None]
    streak = noise(S, beta=2.2, seed=14, aniso=(1.0, 0.06))
    img *= (1 + 0.02 * np.clip(streak / 2.0, -1.0, 1.0))[..., None]
    # raccords des les (x = 0 et 0,5) : joint d'un pixel, encrasse ; bord droit souleve sur une partie de la hauteur
    px = x * S
    roll_px = S / rolls
    right = px % roll_px                         # distance au joint, cote droit (le suivant)
    left = roll_px - right                       # cote gauche
    gap = np.exp(-(np.minimum(right, left) / 1.8) ** 2)
    grime = np.exp(-(np.minimum(right, left) / 6.0) ** 2)
    lift_amount = np.stack([smoothstep(0.3, 1.4, column_noise(S, 15 + k)) for k in range(rolls)], -1)
    lift_v = lift_amount[np.arange(S)[:, None], roll]          # chaque joint a ses propres decollements
    lift = np.exp(-right / 5.0) * (right < 40) * lift_v
    img *= (1 - 0.24 * gap - 0.07 * grime)[..., None]
    img *= (1 + 0.035 * lift)[..., None]       # le bord souleve accroche un peu la lumiere
    save("T_L0_Wallpaper", img)
    # relief : motif tres legerement embosse, liseré en creux, joint et bord souleve (en pixels de hauteur)
    height = 0.17 * motif - 0.2 * border + 0.06 * weave / 3.0 - 1.1 * gap + 0.6 * lift
    save_normal("T_L0_Wallpaper", height, 2.0, S)


def t_noise_lf():
    """v4.3 : bruits doux et independants pour le materiau du monde (taches d'humidite, variation de teinte,
    melange anti-repetition). R : taches (blocs de 2 a 4 m a 16 m l'image), G : variation lente, B : masque de melange."""
    S = 512
    r = noise(S, beta=3.0, seed=301, fmin=4) * 0.9 + noise(S, beta=2.4, seed=302, fmin=10) * 0.22
    g = noise(S, beta=3.4, seed=303, fmin=1.5)
    b = noise(S, beta=3.0, seed=304, fmin=2)
    img = np.stack([n01(r / r.std()), n01(g), n01(b)], -1)
    Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB").save(os.path.join(OUT, "T_NoiseLF.png"), optimize=True)
    print("  ->", os.path.relpath(os.path.join(OUT, "T_NoiseLF.png")))


def t_l0_carpet():
    """v4.15 (realisme) : moquette bouclee commerciale. Une repetition = 160 cm (FBRSurface::Scale du sol), 1 px ~ 0,8 mm :
    rangs de touffetage tous les 2,5 mm, boucles tous les 3,6 mm (decalees d'une demi-boucle d'un rang a l'autre), chaque
    boucle a sa hauteur et son ton (fil chine +-7 %), chaque rang le sien (+-2 %) ; creux entre les boucles plus sombres
    (occlusion dans la couleur et dans le relief). Zones humides diffuses de 15 a 40 cm (plus sombres, la carte de
    rugosite les rend plus lisses) : aucune ne domine la repetition, les grandes taches restent dessinees par le materiau
    a l'echelle du monde (Stains) et par les decalques."""
    S = 2048
    rows_n, loops_n = 640, 448
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float64)
    rv = yy * rows_n / S
    row = np.floor(rv).astype(int)
    fv = rv - row - 0.5
    ru = xx * loops_n / S + (row % 2) * 0.5
    st = np.floor(ru).astype(int)
    fu = ru - st - 0.5
    row %= rows_n
    st %= loops_n
    rng = np.random.default_rng(25)
    loop_h = rng.random((rows_n, loops_n))
    loop_tone = rng.random((rows_n, loops_n))
    row_tone = rng.random(rows_n)
    dome = np.clip(1.0 - (fu / 0.5) ** 2 - (fv / 0.5) ** 2, 0.0, 1.0) ** 0.7
    loop = dome * (0.8 + 0.4 * loop_h[row, st])
    del yy, xx, rv, ru, fu, fv
    base = np.array([0.54, 0.47, 0.30])
    fib = blur_wrap(noise(S, beta=0.2, seed=21), 1)
    g = (1 + 0.14 * (loop_tone[row, st] - 0.5)) * (1 + 0.04 * (row_tone[row] - 0.5)) * (1 + 0.04 * fib)
    g *= 0.8 + 0.2 * loop                      # creux entre les boucles
    img = rgb_from(g, base)
    # Zones humides diffuses : plus sombres, un peu moins saturees
    damp = smoothstep(1.0, 2.6, noise(S, beta=2.4, seed=23, fmin=5))
    img = mix(img, img * np.array([0.88, 0.87, 0.85]), damp * 0.6)
    # v4.3 : salissures sans basses frequences : aucun motif ne domine la repetition
    dirt = noise(S, beta=2.0, seed=24, fmin=6)
    img *= (1 - 0.07 * n01(dirt))[..., None]
    save("T_L0_Carpet", img)
    # Relief : boucles et fibres (hauteur en pixels ; le creux entre deux boucles fait ~0,6 px de pente)
    height = 0.9 * loop + 0.15 * fib / 3.0 - 0.25 * damp
    save_height_normal("T_L0_Carpet", height * 0.9)


def t_l0_ceiling():
    """v4.15 (realisme) : plafond suspendu en dalles de fibre minerale "fissurees" de 61 cm (2 pi). Une repetition = 122 cm
    (FBRSurface::Scale du plafond) = 2 x 2 dalles, 1 px ~ 0,6 mm. Ossature en T peinte en blanc de 24 mm (15/16 po) aux
    multiples de 61 cm ; entre le profil et la dalle, un jour d'ombre de 2 mm puis le chant de la dalle legerement
    arrondi. Dalles : fissures vermiculees (rainures courtes et sinueuses), trous d'aiguille (1 a 2,5 mm), ton propre a
    chaque dalle (+-2,5 %), pores fins. Les taches d'eau ne sont plus peintes dans la texture (elles se repetaient tous les
    1,2 m) : ce sont des decalques autour de certains plafonniers. La carte de rugosite reconnait l'ossature (gris clair
    peu sature) : couleurs des dalles et du profil gardees dans ces plages."""
    S = 2048
    half = S // 2
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float64)
    dx = wrap_dist(xx, half)
    dy = wrap_dist(yy, half)
    d = np.minimum(dx, dy)                      # distance (px) a l'axe du profil le plus proche
    bw = 20.0                                   # demi-largeur du profil (12 mm)
    bar = 1.0 - smoothstep(bw - 0.8, bw + 0.8, d)
    gap = np.exp(-((d - (bw + 2.0)) / 1.3) ** 2) * (1 - bar)
    bevel = smoothstep(bw + 2.0, bw + 10.0, d)   # 0 au chant de la dalle, 1 sur sa face
    tile_id = (np.floor(xx / half).astype(int) % 2) + 2 * (np.floor(yy / half).astype(int) % 2)
    del xx, yy, dx, dy
    rng = np.random.default_rng(35)
    tone = (1 + 0.05 * (rng.random(4) - 0.5))[tile_id]
    # Fissures vermiculees : lignes de niveau zero d'un bruit a bande etroite (30 a 60 periodes par tuile, traits de 1 a 2 px
    # longs de 1 a 3 cm), coupees en segments par un second bruit de meme echelle
    fiss = np.zeros((S, S))
    for k, (lo, hi) in enumerate(((30, 60), (45, 90))):
        n1 = noise(S, beta=0.0, seed=31 + 6 * k, fmin=lo, fmax=hi)
        contour = 1 - smoothstep(0.0, 0.12, np.abs(n1))
        seg = smoothstep(0.35, 1.1, noise(S, beta=0.0, seed=34 + 6 * k, fmin=hi, fmax=2 * hi))
        fiss = np.maximum(fiss, contour * seg)
    # Trous d'aiguille : impulsions aleatoires flouees (1 a 2 mm), serres et discrets
    imp = np.zeros((S, S))
    pts = rng.integers(0, S, size=(22000, 2))
    imp[pts[:, 0], pts[:, 1]] = 0.5 + 0.8 * rng.random(22000)
    F = np.fft.rfft2(imp)
    fy = np.fft.fftfreq(S)[:, None]
    fx = np.fft.rfftfreq(S)[None, :]
    pins = np.fft.irfft2(F * np.exp(-2.0 * (np.pi ** 2) * (0.8 ** 2) * (fx ** 2 + fy ** 2)), s=(S, S))
    pins = np.clip(pins / 0.2, 0.0, 1.0)
    pores = noise(S, beta=0.4, seed=32)
    face = bevel                                # 1 sur la face de la dalle, 0 sur le chant et le profil
    # Couleur : dalle (multiplications grises : saturation inchangee) ; profil blanc un peu plus propre
    g = tone * (1 + 0.03 * pores) * (1 - 0.2 * fiss * face) * (1 - 0.16 * pins * face)
    g *= 1 - 0.10 * (1 - bevel) * (1 - bar)     # chant de la dalle dans l'ombre du profil
    g *= 1 - 0.45 * gap
    tile_c = rgb_from(g, np.array([0.86, 0.84, 0.77]))
    # Leger voile jaunatre a tres grande echelle (fibre vieillie), discret
    tint = smoothstep(0.5, 2.5, noise(S, beta=3.0, seed=33))
    tile_c = mix(tile_c, tile_c * np.array([0.98, 0.96, 0.9]), tint * 0.6)
    bar_c = rgb_from(1 + 0.02 * noise(S, beta=2.0, seed=36), np.array([0.88, 0.88, 0.86]))
    img = mix(tile_c, bar_c, bar)
    save("T_L0_Ceiling", img)
    # Relief (pixels) : profil en saillie, jour d'ombre en creux, chant arrondi, fissures et trous en creux, pores
    height = 0.8 * bar - 1.2 * gap - 0.8 * (1 - bevel) * (1 - bar) + face * (-0.7 * fiss - 0.9 * pins + 0.04 * pores)
    save_height_normal("T_L0_Ceiling", height)


def t_decal_atlas():
    """v4.15 (realisme) : atlas des decalques d'usure (MD_BR_Decal, ABRChunk::BuildDecals), RGBA 2048, quatre cases de 1024 :
      (0, 0) aureole d'infiltration descendant du plafond le long d'un mur (fronts d'eau seches, coulures) ;
      (1, 0) tache d'eau de plafond (anneaux bruns concentriques) ;
      (0, 1) zone humide au sol (tache sombre aux bords doux) ;
      (1, 1) eraflures (traces de semelles et de chariots, frottements sombres).
    L'alpha est le masque (opacite du decalque) ; la couleur est definie partout (pas de liseré sombre au filtrage).
    Une marge de 48 px reste transparente autour de chaque case (aucun debordement d'une case sur l'autre dans les mips)."""
    C = 1024
    m = 48.0 / C
    yy, xx = np.mgrid[0:C, 0:C].astype(np.float64)
    cx, cy = (xx + 0.5) / C, (yy + 0.5) / C
    inner = smoothstep(m, m + 0.03, cx) * smoothstep(m, m + 0.03, cy) * smoothstep(m, m + 0.03, 1 - cx) * smoothstep(m, m + 0.03, 1 - cy)
    cells = []

    # (0, 0) aureole de mur : l'eau descend du plafond (haut de la case) ; fronts successifs seches en cernes bruns
    n_edge = blur_wrap(noise(C, beta=3.4, seed=401), 3)
    n_fine = noise(C, beta=2.4, seed=402)
    n_side = noise(C, beta=3.4, seed=405)
    depth = np.clip(0.5 + 0.18 * np.clip(column_noise(C, 403, beta=2.6), -2.0, 2.0), 0.25, 0.85)[None, :]  # selon x
    env = smoothstep(0.0, 0.22, cx) * smoothstep(0.0, 0.22, 1 - cx)
    f = (cy - m) / np.maximum(depth * (0.35 + 0.65 * env), 0.05) + 0.07 * n_edge + 0.004 * n_fine
    # bords lateraux irreguliers et doux (pas de coin carre sous le plafond)
    lat = smoothstep(0.0, 0.14, cx - m + 0.05 * n_side) * smoothstep(0.0, 0.14, 1 - m - cx + 0.05 * n_side)
    wet = 1 - smoothstep(0.92, 1.0, f)
    rings = np.zeros_like(f)
    for r, w, a in ((1.0, 0.012, 0.75), (0.82, 0.01, 0.35), (0.63, 0.009, 0.25), (0.45, 0.008, 0.15)):
        rings = np.maximum(rings, a * np.exp(-((f - r) / w) ** 2))
    drips = np.zeros_like(f)
    rng = np.random.default_rng(404)
    for _ in range(9):
        x0 = rng.uniform(0.2, 0.8)
        width = rng.uniform(0.004, 0.01)
        length = rng.uniform(1.05, 1.35)
        run = smoothstep(0.7, 0.85, f) * (1 - smoothstep(length - 0.15, length, f))
        drips = np.maximum(drips, np.exp(-((cx - x0) / width) ** 2) * run * rng.uniform(0.1, 0.2))
    alpha = np.clip(wet * (0.16 + 0.08 * n01(n_fine)) + rings * (f < 1.03) + drips, 0, 1) * lat * inner * smoothstep(0.0, 0.04, cy - m)
    col = np.ones((C, C, 3)) * np.array([0.47, 0.36, 0.17])
    col = mix(col, np.ones_like(col) * np.array([0.33, 0.23, 0.10]), np.clip(rings * 1.3, 0, 1))
    col *= (0.92 + 0.16 * n01(n_fine))[..., None]
    cells.append(np.concatenate([col, alpha[..., None]], -1))

    # (1, 0) tache de plafond : anneaux concentriques irreguliers, centre plus clair
    r = np.sqrt((cx - 0.5) ** 2 + (cy - 0.5) ** 2) / 0.42
    n_r = blur_wrap(noise(C, beta=3.2, seed=411), 4)
    r = r * (1 + 0.16 * np.clip(n_r / 2.0, -1, 1)) + 0.002 * noise(C, beta=2.4, seed=412)
    inside = 1 - smoothstep(0.97, 1.0, r)
    rings = np.zeros_like(r)
    for rr, w, a in ((1.0, 0.016, 0.85), (0.86, 0.012, 0.5), (0.7, 0.011, 0.35), (0.5, 0.01, 0.2)):
        rings = np.maximum(rings, a * np.exp(-((r - rr) / w) ** 2))
    alpha = np.clip(inside * (0.18 + 0.12 * r) + rings * (r < 1.03), 0, 1) * inner
    col = np.ones((C, C, 3)) * np.array([0.58, 0.45, 0.24])
    col = mix(col, np.ones_like(col) * np.array([0.42, 0.30, 0.14]), np.clip(rings * 1.2, 0, 1))
    cells.append(np.concatenate([col, alpha[..., None]], -1))

    # (0, 1) zone humide : tache sombre, lobes irreguliers, bords tres doux
    r = np.sqrt((cx - 0.5) ** 2 + (cy - 0.5) ** 2) / 0.43
    r = r * (1 + 0.28 * np.clip(noise(C, beta=3.2, seed=421) / 2.0, -1, 1))
    body = 1 - smoothstep(0.4, 1.0, r)
    alpha = np.clip(body * (0.6 + 0.2 * n01(noise(C, beta=3.0, seed=422))), 0, 1) * inner
    col = np.ones((C, C, 3)) * np.array([0.21, 0.18, 0.13])
    cells.append(np.concatenate([col, alpha[..., None]], -1))

    # (1, 1) eraflures : traits horizontaux sombres (semelles, roues), effiles, et quelques frottements
    alpha = np.zeros((C, C))
    rng = np.random.default_rng(431)
    for _ in range(16):
        y0 = rng.uniform(0.2, 0.8)
        x0, x1 = sorted(rng.uniform(0.1, 0.9, 2))
        if x1 - x0 < 0.12:
            x1 = min(0.9, x0 + 0.12)
        bend = rng.uniform(-0.012, 0.012)
        t = np.clip((cx - x0) / (x1 - x0), 0, 1)
        yc = y0 + bend * np.sin(np.pi * t)
        th = rng.uniform(0.002, 0.007) * np.sqrt(np.clip(1 - (2 * t - 1) ** 2, 0, 1))
        stroke = np.exp(-((cy - yc) / np.maximum(th, 1e-4)) ** 2) * ((cx > x0) & (cx < x1))
        alpha = np.maximum(alpha, stroke * rng.uniform(0.3, 0.7))
    for _ in range(5):
        x0, y0 = rng.uniform(0.25, 0.75, 2)
        sx, sy = rng.uniform(0.04, 0.12), rng.uniform(0.01, 0.03)
        alpha = np.maximum(alpha, np.exp(-((cx - x0) / sx) ** 2 - ((cy - y0) / sy) ** 2) * rng.uniform(0.12, 0.25))
    alpha = np.clip(alpha * (0.75 + 0.25 * n01(noise(C, beta=1.0, seed=432))), 0, 1) * inner
    col = np.ones((C, C, 3)) * np.array([0.075, 0.068, 0.062])
    cells.append(np.concatenate([col, alpha[..., None]], -1))

    atlas = np.zeros((2 * C, 2 * C, 4))
    for i, cell in enumerate(cells):
        u, v = i % 2, i // 2
        atlas[v * C:(v + 1) * C, u * C:(u + 1) * C] = cell
    path = os.path.join(OUT, "T_Decal_Atlas.png")
    Image.fromarray((np.clip(atlas, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA").save(path, optimize=True)
    print("  ->", os.path.relpath(path))


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


def save_height_normal(name, h, size=None):
    """Normal map "<nom>_N.jpg" depuis une carte de hauteur exprimee en pixels (meme convention que save_normal)."""
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    nx, ny, nz = -dx, -dy, np.ones_like(h)
    l = np.sqrt(nx ** 2 + ny ** 2 + nz ** 2)
    nrm = np.stack([nx / l, ny / l, nz / l], -1) * 0.5 + 0.5
    img = Image.fromarray((np.clip(nrm, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB")
    if size and img.size[0] != size:
        img = img.resize((size, size), Image.LANCZOS)
    save_normal_png(name, img)


def t_pool_tile():
    """Carrelage des Poolrooms : 10 x 10 carreaux de 10 cm, email blanc a peine bleu-vert, joints fins gris clair,
    bords arrondis. Chaque carreau est tres legerement incline : les reflets des plafonniers se brisent d'un
    carreau a l'autre, comme sur un vrai mur carrele."""
    S = 1024
    n_t = 10
    x, y = grid_coords(S)
    u, v = (x * n_t) % 1.0, (y * n_t) % 1.0
    ix = np.floor(x * n_t).astype(int) % n_t
    iy = np.floor(y * n_t).astype(int) % n_t
    rng = np.random.default_rng(131)
    per = rng.random((n_t, n_t, 4))[iy, ix]
    gw = 0.028  # demi-joint, en fraction de carreau (joint de ~5 mm)
    edge = np.minimum(np.minimum(u, 1 - u), np.minimum(v, 1 - v))
    tile = smoothstep(gw, gw + 0.012, edge)   # 0 dans le joint, 1 sur l'email
    bevel = smoothstep(gw, gw + 0.075, edge)  # arrondi du bord du carreau

    # Email : blanc froid, lots de fabrication un peu differents, voile tres doux
    shade = 0.972 + 0.04 * per[..., 0]
    hue = (per[..., 1] - 0.5) * 0.02
    col = np.stack([0.905 - hue, 0.94 + hue * 0.3, 0.95 + hue], -1) * shade[..., None]
    col *= (1 + 0.012 * noise(S, beta=2.5, seed=133))[..., None]
    col *= (0.95 + 0.05 * bevel)[..., None]
    # Joints : ciment gris clair, plus sombre au fond et sale par endroits
    grout = np.array([0.77, 0.8, 0.8]) * (1 + 0.035 * noise(S, beta=2.8, seed=134))[..., None]
    grout *= (0.9 + 0.1 * smoothstep(0.0, gw, edge))[..., None]
    img = mix(grout, col, tile)
    # Calcaire / salissures a grande echelle, tres legers
    img *= (1 - 0.035 * smoothstep(0.5, 2.0, noise(S, beta=3.0, seed=132)))[..., None]
    save("T_PoolTile", img)

    # Relief (1 pixel ~ 1 mm) : joint en creux, bord arrondi, carreau incline, email legerement ondule
    h = 0.9 * tile + 1.3 * bevel
    tilt = 0.012 * 102.4
    h += tile * ((per[..., 2] - 0.5) * (u - 0.5) + (per[..., 3] - 0.5) * (v - 0.5)) * tilt
    h += 0.06 * noise(S, beta=3.0, seed=135) * tile
    save_height_normal("T_PoolTile", h)


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


def t_hazard():
    """Bandes jaunes et noires a 45 degres (pieds des piliers du parking), peinture usee et ecaillee"""
    S = 1024
    x, y = grid_coords(S)
    stripe = ((x + y) * 4.0) % 1.0  # 4 bandes par repetition, raccord parfait
    yellow = smoothstep(0.47, 0.53, stripe) * (1 - smoothstep(0.97, 1.0, stripe)) + (1 - smoothstep(0.0, 0.03, stripe)) * 0.0
    paint = mix(rgb_from(np.ones((S, S), np.float32), (0.05, 0.05, 0.05)), rgb_from(np.ones((S, S), np.float32), (0.86, 0.66, 0.08)), yellow)
    a = noise(S, beta=1.6, seed=171)
    paint *= (1 + 0.08 * a)[..., None]
    # Ecailles : la peinture est partie, le beton apparait
    chips = smoothstep(1.7, 2.1, noise(S, beta=1.1, seed=172)) + smoothstep(2.0, 2.3, noise(S, beta=0.6, seed=173))
    concrete = rgb_from(1 + 0.1 * noise(S, beta=2.0, seed=174), (0.45, 0.44, 0.42))
    img = mix(paint, concrete, np.clip(chips, 0, 1))
    # Salissures en bas (projections, frottements de pneus)
    dirt = smoothstep(0.3, 1.8, noise(S, beta=2.6, seed=175))
    img *= (1 - 0.35 * dirt)[..., None]
    save("T_Hazard", img)


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
    """Rides de l'eau : ondulations douces, sans grain fin (un grain fin fait scintiller les reflets des lampes)"""
    S = 512
    h = noise(S, beta=3.5, seed=221, fmax=28.0) * 0.7 + noise(S, beta=2.5, seed=222, fmax=48.0) * 0.3
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    k = 0.08 / max(0.5 * (np.abs(dx).mean() + np.abs(dy).mean()), 1e-6)  # meme intensite moyenne qu'avant
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
    """Caustiques de piscine : la lumiere traverse une surface d'eau ondulee (bruit periodique) et chaque rayon
    refracte est suivi jusqu'au fond. Les rayons se concentrent en filaments lumineux courbes autour de cellules
    sombres, exactement comme au fond d'un bassin."""
    S, ss = 512, 4
    M = S * ss
    h = noise(M, beta=3.0, seed=241, fmax=9.0)
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    lap = np.roll(h, 1, 0) + np.roll(h, -1, 0) + np.roll(h, 1, 1) + np.roll(h, -1, 1) - 4 * h
    k = 1.2 / lap.std()  # au-dela de 1 : les rayons se croisent, d'ou les filaments
    yy, xx = np.mgrid[0:M, 0:M].astype(np.float64)
    px = (xx - k * gx) / ss
    py = (yy - k * gy) / ss
    x0, y0 = np.floor(px), np.floor(py)
    fx, fy = px - x0, py - y0
    x0, y0 = x0.astype(np.int64) % S, y0.astype(np.int64) % S
    x1, y1 = (x0 + 1) % S, (y0 + 1) % S
    acc = np.zeros(S * S)
    for xi, yi, w in ((x0, y0, (1 - fx) * (1 - fy)), (x1, y0, fx * (1 - fy)), (x0, y1, (1 - fx) * fy), (x1, y1, fx * fy)):
        acc += np.bincount((yi * S + xi).ravel(), weights=w.ravel(), minlength=S * S)
    c = blur_wrap(acc.reshape(S, S), 1)
    c = c + 0.35 * blur_wrap(c, 4)  # leger halo autour des filaments
    c = np.clip(c / np.percentile(c, 99.5), 0, 1) ** 0.9
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


# v4.15 : nom de texture -> generateur (ligne de commande : ne refaire que certaines textures)
GENERATORS = [
    ("T_L0_Wallpaper", t_l0_wallpaper),
    ("T_L0_Carpet", t_l0_carpet),
    ("T_NoiseLF", t_noise_lf),
    ("T_L0_Ceiling", t_l0_ceiling),
    ("T_Decal_Atlas", t_decal_atlas),
    ("T_Grime", t_grime),
    ("T_Concrete", lambda: t_concrete("T_Concrete", 50, (0.52, 0.51, 0.49))),
    ("T_ConcreteFloor", lambda: t_concrete("T_ConcreteFloor", 55, (0.40, 0.39, 0.37), wet=True, cracks=True)),
    ("T_ConcreteDark", lambda: t_concrete("T_ConcreteDark", 58, (0.20, 0.20, 0.21), S=512, cracks=True)),
    ("T_Brick", t_brick),
    ("T_MetalPanel", t_metal_panel),
    ("T_OfficeCarpet", t_office_carpet),
    ("T_OfficeWall", t_office_wall),
    ("T_HotelCarpet", t_hotel_carpet),
    ("T_HotelWallpaper", t_hotel_wallpaper),
    ("T_Wood", t_wood),
    ("T_PoolTile", t_pool_tile),
    ("T_Rock", t_rock),
    ("T_Dirt", t_dirt),
    ("T_Asphalt", t_asphalt),
    ("T_Hazard", t_hazard),
    ("T_Grass", t_grass),
    ("T_Facade", t_facade),
    ("T_Siding", t_siding),
    ("T_Glitch", t_glitch),
    ("T_Skin", t_skin),
    ("T_WaterNormal", t_water_normal),
    ("T_Paper", t_paper),
    ("T_Caustics", t_caustics),
    ("T_LensDirt", t_lens_dirt),
    ("T_FlatNormal", t_flat_normal),
]


if __name__ == "__main__":
    wanted = sys.argv[1:]
    known = [n for n, _ in GENERATORS]
    unknown = [n for n in wanted if n not in known]
    if unknown:
        print("Textures inconnues : %s (connues : %s)" % (", ".join(unknown), ", ".join(known)))
        sys.exit(2)
    print("Generation des textures dans", os.path.abspath(OUT))
    for name, fn in GENERATORS:
        if not wanted or name in wanted:
            fn()
    print("Termine.")
