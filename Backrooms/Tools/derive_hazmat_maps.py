"""v4.17 (combinaison) : normal map et carte ORM de la combinaison, derivees de l'atlas protege T_Hazmat_Suit.jpg.

Conception : Docs/v417/agents/materiaux_combinaison.md, § 2.3 a 2.5 (etapes 1 a 10) et § 8 (ecarts). Entrees, lues sans
etre modifiees :
  RawAssets/Textures/T_Hazmat_Suit.jpg        atlas fourni (empreinte comparee a RawAssets/protected_assets.json avant tout)
  RawAssets/Derived/Hazmat/hazmat_uv_*        ilots UV, bords et coutures (Tools/Blender/export_hazmat_uv.py, etape 0)
Sorties (dossier des derives ; jamais RawAssets/Textures) :
  RawAssets/Derived/Textures/T_Hazmat_Suit_N.png     normale en espace tangent, convention Unreal (DirectX : x a droite,
                                                     y vers le bas de l'image, nx = -dh/dx, ny = -dh/dy), plate hors ilots
  RawAssets/Derived/Textures/T_Hazmat_Suit_ORM.png   R occlusion, G rugosite ABSOLUE, B metal (0) ; a importer en lineaire
  RawAssets/Derived/Textures/T_Hazmat_Suit_maps.json provenance : empreintes des entrees, parametres, mesures, empreintes
                                                     des pixels livres

Relief, sans aucun bruit procedural :
  - bandes thermosoudees sur les coutures de PVC (bords d'ilots qui coincident en 3D ; aucune sur le caoutchouc moule des
    gants, des bottes et des semelles), ourlets sur les bords libres et a la jonction avec le masque ;
  - bandes et pastilles noires (caoutchouc applique) ;
  - plis peints de l'albedo (passe-bande de la luminance, seulement sur le PVC jaune propre).
Les bandes et ourlets suivent le contour LISSE de l'ilot (distance signee tiree d'un flou de l'ilot, symetrique de part et
d'autre de la couture) : le bord UV en dents de scie du modele ne se voit pas, et le relief est le meme des deux cotes d'une
couture (controle automatique sur les paires de cotes decrites par l'etape 0).
Rugosite : PVC propre 0,36 ; creux, bandes, gants, bottes, semelles et salissure plus rugueux ; visiere 0,05 au centre, 0,15
au bord. Le script ne tire aucun hasard : meme entree, meme sortie.

    python Tools/derive_hazmat_maps.py                    # calcule, controle, puis ecrit les cartes (2048) et leur provenance
    python Tools/derive_hazmat_maps.py --check            # recalcule, controle, compare aux fichiers livres ; n'ecrit rien
    python Tools/derive_hazmat_maps.py --crinkle 1        # rides fines (desactivees par defaut : voir le rapport, § 8)
    python Tools/derive_hazmat_maps.py --preview <dossier hors depot>   # apercus en lumiere rasante

Controles automatiques (etape 10 ; un echec arrete le script avant toute ecriture, code 1) : pente mediane dans les ilots
<= 1,5 deg, p99 <= 15, aucune pente > 45 ; raccords : pli entre les deux cotes d'une couture (pentes de la normal map
livree, le long de l'arete et en travers) p99 <= 4 deg ;
rugosite dans [0,04 ; 0,85] ; fond plat ; dimensions ; avec --check, pixels identiques a ceux livres.
"""
import argparse
import hashlib
import io
import json
import math
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import protect_assets  # noqa: E402

SCRIPT_VERSION = 1
ATLAS_REL = "RawAssets/Textures/T_Hazmat_Suit.jpg"
ATLAS = os.path.join(ROOT, *ATLAS_REL.split("/"))
MASKS = os.path.join(ROOT, "RawAssets", "Derived", "Hazmat")
MASK_FILES = ("hazmat_uv_islands.png", "hazmat_uv_edges.png", "hazmat_uv_islands.json")
OUT = os.path.join(ROOT, "RawAssets", "Derived", "Textures")
PROTECTED_TEX = os.path.join(ROOT, "RawAssets", "Textures")
NAME_N, NAME_ORM, NAME_JSON = "T_Hazmat_Suit_N.png", "T_Hazmat_Suit_ORM.png", "T_Hazmat_Suit_maps.json"
GLASS_ID = 65535
SEAM, FREE = 255, 128
PX_PER_MM_DEFAULT = 0.70  # 14 px/cm a 4096 : densite du torse, quand les masques UV manquent
SOLE_BONES = ("LeftFoot", "RightFoot", "LeftToeBase", "RightToeBase")
SOLE_ZREL = 0.05  # semelles : ilots des pieds dont le point le plus haut est a moins de 5 % de la hauteur (mesure : 3,2 %)
MOLDED = ("glove", "boot", "sole")  # caoutchouc moule : pas de bande thermosoudee entre deux de ces ilots
KINDS = ("pvc",) + MOLDED

# Valeurs du § 2.2 (rugosite absolue) ; hauteurs en mm ; rayons en px a 2048 (multiplies par taille / 2048)
R_PVC, R_RIDGE, R_HOLLOW, R_TAPE = 0.36, 0.30, 0.46, 0.46
R_ORANGE, R_GLOVE, R_TRIM, R_SOLE, R_DIRT = 0.55, 0.50, 0.55, 0.78, 0.78
R_GLASS_CENTER, R_GLASS_EDGE = 0.05, 0.15
EDGE_SIGMA = 8.0      # flou du contour (px) : efface les dents de scie du bord UV (amplitude 1 a 5 px, periode 10 a 20)
TAPE_H, TAPE_CREST, TAPE_IN, TAPE_OUT, TAPE_CREST_W = 0.30, 0.0, 9.0, 12.0, 3.0   # bande : plateau, crete (0 : voir § 8), bords (mm)
HEM_H, HEM_AT, HEM_W = 0.35, 5.0, 3.0                                             # ourlet : hauteur, position, largeur
TRIM_H = 0.30
FOLD_FINE, FOLD_MID, FOLD_GAIN, FOLD_CLIP = 3.0, 18.0, 0.45, 0.35  # plis : passe-bande sigma 3 / 18 px, 0,45 mm pour 10 %
CHECKS = dict(slope_p50_max=1.5, slope_p99_max=15.0, slope_max=45.0, seam_crease_p99_max_deg=4.0, rough_min=0.04, rough_max=0.85)


# --------------------------------------------------------------------------------------------------- utilitaires
def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def sha256_pixels(arr):
    return hashlib.sha256(np.ascontiguousarray(arr).tobytes()).hexdigest()


def rel(path):
    return os.path.relpath(path, ROOT).replace(os.sep, "/")


def srgb_to_lin(a):
    return np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4).astype(np.float32)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return (t * t * (3.0 - 2.0 * t)).astype(np.float32)


def _box1d(a, r, axis):
    """Moyenne glissante de rayon r par sommes cumulees (float64), bords reflechis"""
    if r <= 0:
        return a
    pad = [(0, 0)] * a.ndim
    pad[axis] = (r + 1, r)
    p = np.pad(a, pad, mode="reflect")
    c = np.cumsum(p, axis=axis, dtype=np.float64)
    n = a.shape[axis]
    hi = np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis=axis)
    lo = np.take(c, np.arange(0, n), axis=axis)
    return ((hi - lo) / (2 * r + 1)).astype(np.float32)


def gblur(a, s):
    """Flou quasi gaussien d'ecart type s (trois flous en boite, numpy seul, sans scipy)"""
    if s <= 0:
        return a.astype(np.float32)
    wi = math.sqrt(12 * s * s / 3 + 1)
    wl = int(math.floor(wi))
    if wl % 2 == 0:
        wl -= 1
    wu = wl + 2
    m = round((12 * s * s - 3 * wl * wl - 12 * wl - 9) / (-4 * wl - 4))
    out = a.astype(np.float32)
    for i in range(3):
        r = max(0, ((wl if i < m else wu) - 1) // 2)
        out = _box1d(_box1d(out, r, 0), r, 1)
    return out


def masked_blur(a, m, s):
    """Flou restreint au masque m : flou(a.m) / flou(m) (aucune fuite du fond ni d'une autre region)"""
    return gblur(a * m, s) / np.maximum(gblur(m, s), 1e-4)


def dilate4(m):
    n = m.copy()
    n[1:, :] |= m[:-1, :]
    n[:-1, :] |= m[1:, :]
    n[:, 1:] |= m[:, :-1]
    n[:, :-1] |= m[:, 1:]
    return n


def dist_from(seed, kmax):
    """Distance (px) aux pixels seed par dilatations alternees carre / croix (metrique octogonale), bornee a kmax"""
    d = np.full(seed.shape, float(kmax), np.float32)
    cur = seed.copy()
    d[cur] = 0.0
    for k in range(1, kmax + 1):
        n = dilate4(cur)
        if k % 2 == 0:
            n[1:, 1:] |= cur[:-1, :-1]
            n[:-1, :-1] |= cur[1:, 1:]
            n[1:, :-1] |= cur[:-1, 1:]
            n[:-1, 1:] |= cur[1:, :-1]
        d[n & ~cur] = float(k)
        cur = n
    return d


def label4(mask):
    """Composantes 4-connexes (numpy seul) : propagation du plus petit indice et saut de pointeur, jusqu'a stabilite"""
    big = np.iinfo(np.int64).max
    lab = np.where(mask, np.arange(mask.size, dtype=np.int64).reshape(mask.shape), big)
    while True:
        new = lab.copy()
        for axis in (0, 1):
            for sh in (1, -1):
                nb = np.roll(lab, sh, axis)
                edge = (0 if sh == 1 else -1)
                if axis == 0:
                    nb[edge, :] = big
                else:
                    nb[:, edge] = big
                new = np.minimum(new, nb)
        new = np.where(mask, new, big)
        flat = new.ravel()
        m = flat != big
        flat[m] = np.minimum(flat[m], flat[flat[m]])
        new = flat.reshape(mask.shape)
        if np.array_equal(new, lab):
            return lab
        lab = new


def detect_trim(inside, mx, sat, excluded, s):
    """Bandes et pastilles noires (caoutchouc applique). Une region sombre et neutre (max RGB < 0,30, saturation < 0,30) est
    retenue entiere si elle est grande (>= 300 px a 2048), noire et neutre en mediane (max RGB < 0,20, saturation < 0,07) et
    faite pour au moins 12 % de noir uni (graines : densite masquee > 0,9 et max RGB < 0,18). La boue, brune et dechiquetee,
    echoue a la mediane de saturation ou aux graines : aucune plaque sur la salissure. Les poignets de cheville, bruns et
    trames dans l'atlas, ne sont pas retenus (sur aucune jambe)."""
    C = inside & (mx < 0.30) & (sat < 0.30) & ~excluded
    dens = masked_blur(C.astype(np.float32), inside.astype(np.float32), 2 * s)
    seeds = C & (dens > 0.9) & (mx < 0.18)
    lab = label4(C)
    u, inv, cnt = np.unique(lab[C], return_inverse=True, return_counts=True)
    seedc = np.bincount(inv, weights=seeds[C].astype(np.float64), minlength=len(u))
    order = np.argsort(inv, kind="stable")
    bounds = np.searchsorted(inv[order], np.arange(len(u) + 1))
    sats, mxs = sat[C], mx[C]
    keep = np.zeros(len(u), bool)
    for k in range(len(u)):
        if cnt[k] < 300 * s * s:
            continue
        sel = order[bounds[k]:bounds[k + 1]]
        keep[k] = np.median(sats[sel]) < 0.07 and np.median(mxs[sel]) < 0.20 and seedc[k] / cnt[k] >= 0.12
    trim = np.zeros_like(C)
    trim[C] = keep[inv]
    closed = trim.copy()
    for _ in range(int(round(2 * s))):  # fermeture : les motifs gris imprimes des pastilles font partie de la pastille
        closed = dilate4(closed)
    for _ in range(int(round(2 * s))):
        closed = ~dilate4(~closed)
    return (gblur((closed | trim).astype(np.float32), 1 * s) > 0.5) & inside & ~excluded, int(keep.sum())


def raster_lines(img, segs, value):
    """Segments d'un pixel de large (echantillonnage tous les 0,25 px), comme Tools/Blender/export_hazmat_uv.py"""
    size = img.shape[0]
    for (ax, ay), (bx, by) in segs:
        n = int(math.ceil(math.hypot(bx - ax, by - ay) * 4.0)) + 1
        t = np.linspace(0.0, 1.0, n)
        xs = np.clip(np.floor(ax + (bx - ax) * t).astype(np.int64), 0, size - 1)
        ys = np.clip(np.floor(ay + (by - ay) * t).astype(np.int64), 0, size - 1)
        img[ys, xs] = np.maximum(img[ys, xs], value)


def gradient(h, m):
    """Derivees en x et y : differences centrales dans le masque, d'un seul cote au bord, 0 pour un pixel isole"""
    out = []
    for axis in (1, 0):
        fwd = np.roll(h, -1, axis) - h
        bwd = h - np.roll(h, 1, axis)
        mf = np.roll(m, -1, axis) & m
        mb = np.roll(m, 1, axis) & m
        if axis == 1:
            mf[:, -1] = False
            mb[:, 0] = False
        else:
            mf[-1, :] = False
            mb[0, :] = False
        g = np.where(mf & mb, 0.5 * (fwd + bwd), np.where(mf, fwd, np.where(mb, bwd, 0.0)))
        out.append(g.astype(np.float32))
    return out


def resize_mask(arr, size, reduce_max):
    """Masque a une autre taille : plus proche voisin, ou maximum par bloc pour reduire des lignes d'un pixel"""
    n = arr.shape[0]
    if n == size:
        return arr
    if reduce_max and n > size and n % size == 0:
        f = n // size
        return arr.reshape(size, f, size, f).max(axis=(1, 3))
    return np.asarray(Image.fromarray(arr).resize((size, size), Image.NEAREST))


def pad_maps(chans, m, rings):
    """Bourrage (etape 9) : les pixels hors ilots prennent la moyenne de leurs voisins deja remplis, anneau par anneau
    (chaque anneau avance d'environ 2 px) ; au-dela, la moyenne de chaque carte. Le gris du fond ne se mele plus aux mips."""
    out = [c.copy() for c in chans]
    mm = m.astype(np.float32)
    for _ in range(rings):
        den = gblur(mm, 1.0)
        fill = (den > 1e-3) & (mm < 0.5)
        if not fill.any():
            break
        for c in out:
            num = gblur(c * mm, 1.0)
            c[fill] = num[fill] / den[fill]
        mm = np.maximum(mm, fill.astype(np.float32))
    rest = mm < 0.5
    for c, src in zip(out, chans):
        c[rest] = float(src[m].mean()) if m.any() else 0.5
    return out


def slope_deg(n):
    return np.degrees(np.arccos(np.clip(n[..., 2], -1.0, 1.0)))


def decode_normal(rgb8):
    n = rgb8.astype(np.float32) / 255.0 * 2.0 - 1.0
    n[..., 2] = np.sqrt(np.clip(1.0 - n[..., 0] ** 2 - n[..., 1] ** 2, 0.0, 1.0))  # comme BC5 : z reconstruit
    return n


def pct(a, qs):
    return [round(float(x), 3) for x in np.percentile(a, qs)] if a.size else [None] * len(qs)


# --------------------------------------------------------------------------------------------------- entrees
def check_atlas():
    reg = protect_assets.load().get("derived", {}).get(ATLAS_REL, {}).get("sha256")
    if not reg:
        raise SystemExit("ECHEC : %s absent du registre protege (RawAssets/protected_assets.json)" % ATLAS_REL)
    if not os.path.isfile(ATLAS):
        raise SystemExit("ECHEC : %s introuvable" % ATLAS_REL)
    got = sha256_file(ATLAS)
    if got != reg:
        raise SystemExit("ECHEC : %s ne correspond pas au registre (%s... au lieu de %s...) : arret, rien n'est ecrit"
                         % (ATLAS_REL, got[:12], reg[:12]))
    return got


def load_masks(size):
    """Ilots, lignes, densites et coutures (etape 0). Sans les masques : None (repli sur l'albedo)."""
    paths = [os.path.join(MASKS, n) for n in MASK_FILES]
    if not all(os.path.isfile(p) for p in paths):
        return None
    with open(paths[2], encoding="utf-8") as fh:
        info = json.load(fh)
    for name in MASK_FILES[:2]:
        if sha256_file(os.path.join(MASKS, name)) != info["files"][name]:
            raise SystemExit("ECHEC : %s ne correspond pas a hazmat_uv_islands.json (relancer export_hazmat_uv.py)" % name)
    src_size = int(info["size"])
    scale = size / float(src_size)
    ids = resize_mask(np.asarray(Image.open(paths[0])).astype(np.uint16), size, False).astype(np.int32)
    edges = resize_mask(np.asarray(Image.open(paths[1])).astype(np.uint8), size, True)
    dens = np.full(GLASS_ID + 1, PX_PER_MM_DEFAULT * size / 2048.0, np.float32)
    sole = np.zeros(GLASS_ID + 1, bool)
    for isl in info["islands"]:
        if isl["px_per_mm"] > 0:
            dens[isl["id"]] = max(0.3 * size / 2048.0, isl["px_per_mm"] * scale)
        sole[isl["id"]] = isl["mat"] == "HazmatSuit" and isl["bone"] in SOLE_BONES and isl["zrel"][1] < SOLE_ZREL
    faces = {int(isl["id"]): int(isl["faces"]) for isl in info["islands"]}
    return dict(ids=ids, free=edges == FREE, seam_any=edges == SEAM, seams=info.get("seams"), dens=dens, sole=sole, faces=faces,
                source="masques UV de SK_Hazmat (export_hazmat_uv.py)", fbx_sha256=info.get("source_sha256"),
                files={n: sha256_file(p) for n, p in zip(MASK_FILES, paths)})


def masks_from_albedo(a, size):
    """Repli sans masques : tout pixel a plus de 0,07 du gris de fond (mediane d'un cadre de 8 px) est un ilot, et tous ses
    bords sont pris pour des coutures. Densite par defaut, ni semelles, ni visiere, ni controle des raccords."""
    frame = np.concatenate([a[:8].reshape(-1, 3), a[-8:].reshape(-1, 3), a[:, :8].reshape(-1, 3), a[:, -8:].reshape(-1, 3)])
    bg = np.median(frame, axis=0)
    inside = np.abs(a - bg).max(-1) > 0.07
    edge = inside & ~(np.roll(inside, 1, 0) & np.roll(inside, -1, 0) & np.roll(inside, 1, 1) & np.roll(inside, -1, 1))
    return dict(ids=inside.astype(np.int32), free=np.zeros_like(inside), seam_any=edge, seams=None, faces={},
                dens=np.full(GLASS_ID + 1, PX_PER_MM_DEFAULT * size / 2048.0, np.float32), sole=np.zeros(GLASS_ID + 1, bool),
                source="SEGMENTATION DE L'ALBEDO (masques UV absents : relancer Tools/Blender/export_hazmat_uv.py)",
                fbx_sha256=None, files={})


def island_kinds(ids, glove, orange, sole_flag):
    """Nature de chaque ilot d'apres l'albedo : gant (sarcelle), botte (orange), semelle (os du pied), PVC sinon"""
    n = int(ids.max()) + 1
    tot = np.bincount(ids.ravel(), minlength=n).astype(np.float64)
    fg = np.bincount(ids.ravel(), weights=glove.ravel().astype(np.float64), minlength=n) / np.maximum(tot, 1)
    fo = np.bincount(ids.ravel(), weights=orange.ravel().astype(np.float64), minlength=n) / np.maximum(tot, 1)
    kinds = {}
    for i in np.nonzero(tot)[0]:
        if i == 0 or i == GLASS_ID:
            continue
        kinds[int(i)] = "sole" if sole_flag[i] else ("glove" if fg[i] > 0.5 else ("boot" if fo[i] > 0.3 else "pvc"))
    return kinds


# --------------------------------------------------------------------------------------------------- calcul
def derive(size, crinkle):
    atlas_sha = check_atlas()
    src = Image.open(ATLAS).convert("RGB")
    if src.size != (size, size):
        src = src.resize((size, size), Image.LANCZOS)
    a = np.asarray(src, dtype=np.float32) / 255.0
    M = load_masks(size) or masks_from_albedo(a, size)
    s = size / 2048.0
    ids = M["ids"]
    suit_ids = np.where(ids == GLASS_ID, 0, ids)
    inside = suit_ids > 0
    glass = ids == GLASS_ID
    sole = M["sole"][ids] & inside
    isl_f = inside.astype(np.float32)
    # Ilot "plein" : les trous d'un ou deux pixels entre un panneau et les fragments du modele (61 ilots de 1 a 29 faces) et les
    # fentes de quelques pixels (coupures dans un meme panneau) sont fermes. Le contour lisse, les bandes noires et les plis
    # sont calcules sur l'ilot plein : sans cela, chaque trou ferait un anneau de bande, et chaque fente un bord de pastille.
    # Les grands ilots sont a plus de 16 px les uns des autres (mesure) : la fermeture de 3 px ne les relie jamais.
    solid = inside.copy()
    for _ in range(int(round(3 * s))):
        solid = dilate4(solid)
    for _ in range(int(round(3 * s))):
        solid = ~dilate4(~solid)
    solid |= inside
    solid_f = solid.astype(np.float32)
    # densite de texels (px/mm) : celle de l'ilot ; un fragment (moins de 30 faces, densite propre sans signification) prend
    # celle du panneau qui l'entoure, sinon sa bande et ses plis changeraient d'echelle sur quelques pixels
    pxmm = M["dens"][ids]
    if M["faces"]:
        small = np.zeros(int(ids.max()) + 1, bool)
        for i, f in M["faces"].items():
            if i < small.size and i != GLASS_ID and f < 30:
                small[i] = True
        frag = small[suit_ids] & inside
        big = inside & ~frag
        around = masked_blur(pxmm, big.astype(np.float32), 6 * s)
        pxmm = np.where(frag, np.where(gblur(big.astype(np.float32), 6 * s) > 1e-3, around, PX_PER_MM_DEFAULT * s), pxmm)
    pxmm = pxmm.astype(np.float32)

    # Etape 1 : preparation (luminance lineaire ; saturation et teinte sur les valeurs sRGB)
    lin = srgb_to_lin(a)
    L = lin @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    mx = a.max(-1)
    mn = a.min(-1)
    sat = (mx - mn) / np.maximum(mx, 1e-3)
    hue = np.degrees(np.arctan2(math.sqrt(3.0) * (a[..., 1] - a[..., 2]), 2.0 * a[..., 0] - a[..., 1] - a[..., 2])) % 360.0

    # Etape 2 : regions
    yellow = inside & (sat > 0.35) & (hue > 38) & (hue < 75)
    orange = inside & (sat > 0.45) & (hue > 5) & (hue <= 38)
    glove = inside & (sat > 0.15) & (hue > 160) & (hue < 240) & (mx < 0.5)
    trim, n_trim = detect_trim(inside, mx, sat, glove | sole, s)
    kinds = island_kinds(suit_ids, glove, orange, M["sole"])
    lut = np.zeros(int(suit_ids.max()) + 1, np.int8)
    for i, kd in kinds.items():
        lut[i] = KINDS.index(kd)
    kind_px = lut[suit_ids]

    # Etape 3 : salissure (plus sombre et moins saturee que la matiere autour)
    base_m = (yellow | orange).astype(np.float32)
    L_loc = masked_blur(L, base_m, 24 * s)
    S_loc = masked_blur(sat, base_m, 24 * s)
    dirt = smoothstep(0.08, 0.30, S_loc - sat) * smoothstep(0.15, 0.45, (L_loc - L) / np.maximum(L_loc, 1e-3))
    dirt = gblur(dirt * (inside & ~trim & ~glove & ~sole), 1 * s)
    dirt_zone = np.clip(gblur((dirt > 0.03).astype(np.float32), 6 * s) * 3.0, 0.0, 1.0)

    # Etape 4 : contour lisse et nature des bords. x = distance signee (mm) au contour lisse de l'ilot, positive dedans :
    # logit du flou de l'ilot (pour un bord droit flou de sigma, logit(S) / 1,702 x sigma est la distance). Le flou ne voit que
    # l'ilot (le reste vaut 0, comme l'autre cote de la couture en 3D) : de part et d'autre d'une couture, x_A = -x_B.
    sig = EDGE_SIGMA * s
    S = np.clip(gblur(solid.astype(np.float32), sig), 1e-4, 1.0 - 1e-4)
    x_mm = (np.log(S / (1.0 - S)) / 1.702 * sig) / pxmm
    taped_l = np.zeros(ids.shape, np.uint8)
    molded_l = np.zeros(ids.shape, np.uint8)
    hem_l = M["free"].astype(np.uint8)
    seam_pairs = []
    if M["seams"] is not None:
        by_key = {}
        segs = ([], [], [])  # bande, ourlet, moule
        for k0, k1, isl, other, u0, v0, u1, v1 in M["seams"]:
            seg = ((u0 * size, (1.0 - v0) * size), (u1 * size, (1.0 - v1) * size))
            if other == 0:
                segs[1].append(seg)  # jonction avec le masque (ou arete non manifold) : ourlet du PVC
            elif kinds.get(isl) in MOLDED and kinds.get(other) in MOLDED:
                segs[2].append(seg)  # caoutchouc moule (gant, botte, semelle) : ni bande ni ourlet
            else:
                segs[0].append(seg)  # couture de PVC, ou PVC contre caoutchouc : la bande couvre la jonction
            by_key.setdefault((k0, k1), []).append((isl, other, (u0, v0, u1, v1)))
        raster_lines(taped_l, segs[0], 1)
        raster_lines(hem_l, segs[1], 1)
        raster_lines(molded_l, segs[2], 1)
        # paires de cotes d'une meme arete 3D entre deux ilots de la combinaison (les fragments de moins de 30 faces, dont le
        # contour lisse n'a pas de sens, sont exclus du controle)
        seam_pairs = [v for v in by_key.values() if len(v) == 2 and v[0][0] > 0 and v[1][0] > 0
                      and M["faces"].get(v[0][0], 0) >= 30 and M["faces"].get(v[1][0], 0) >= 30]
    else:
        taped_l = M["seam_any"].astype(np.uint8)
    # nature du bord le plus proche, en douceur : part de chaque sorte de ligne dans un flou de 6 px (a la jonction du masque,
    # les aretes "ourlet" et "couture" alternent sur quelques millimetres : un choix tranche ferait des grumeaux de bande)
    dens_l = [gblur((l > 0).astype(np.float32), 6 * s) for l in (taped_l, hem_l, molded_l)]
    tot = dens_l[0] + dens_l[1] + dens_l[2]
    w_tape = np.where(tot > 1e-5, dens_l[0] / np.maximum(tot, 1e-5), 0.0).astype(np.float32)
    w_hem = np.where(tot > 1e-5, dens_l[1] / np.maximum(tot, 1e-5), 0.0).astype(np.float32)

    # Etape 5 : hauteurs (mm)
    ax = np.abs(x_mm)
    tape = w_tape * (TAPE_H * (1.0 - smoothstep(TAPE_IN, TAPE_OUT, ax)) + TAPE_CREST * np.exp(-(x_mm / TAPE_CREST_W) ** 2))
    hem = w_hem * HEM_H * np.exp(-((x_mm - HEM_AT) / HEM_W) ** 2)
    closed = trim.copy()
    for _ in range(int(round(3 * s))):
        closed = dilate4(closed)
    for _ in range(int(round(3 * s))):
        closed = ~dilate4(~closed)
    trim_s = trim | (closed & solid & ~inside)  # la bande continue dans les fentes
    trim_h = TRIM_H * masked_blur(trim_s.astype(np.float32), solid_f, 1.5 * s)
    yellow_s = solid & (sat > 0.35) & (hue > 38) & (hue < 75)
    sat_w = smoothstep(0.50, 0.62, masked_blur(sat, solid_f, 2.0 * s))  # salissure legere : moins saturee, pas de pli
    # pas de pli a moins de ~5 cm d'une salissure : au-dessus de la boue, des taches olive (aussi saturees que le PVC, donc
    # hors du masque de salissure) assombrissent l'albedo, et le passe-bande en faisait des grumeaux en relief
    dirt_near = np.clip(gblur((dirt > 0.03).astype(np.float32), 24 * s) * 4.0, 0.0, 1.0)
    fold_m = (yellow_s & ~trim_s).astype(np.float32) * (1.0 - dirt_zone) * (1.0 - dirt_near) * sat_w
    L_fine = masked_blur(L, fold_m, FOLD_FINE * s)
    L_mid = masked_blur(L, fold_m, FOLD_MID * s)
    rel_l = np.clip(L_fine / np.maximum(L_mid, 1e-3) - 1.0, -0.2, 0.2)
    fade = smoothstep(4.0, 14.0, x_mm)
    fold_ms = masked_blur(fold_m, solid_f, 2.5 * s)
    # plis : 0,45 mm pour 10 % de luminance, ecretes en douceur a +-0,35 mm (les plis peints restent entre -0,21 et +0,20 mm a
    # p5 / p95 ; les points sombres isoles, au-dela de -0,4 mm, faisaient des trous)
    fold_h = FOLD_CLIP * np.tanh(rel_l / 0.10 * FOLD_GAIN * fold_ms * fade / FOLD_CLIP)
    L_f2 = masked_blur(L, fold_m, 0.8 * s)
    L_m2 = masked_blur(L, fold_m, 5 * s)
    rel2 = np.clip(L_f2 / np.maximum(L_m2, 1e-3) - 1.0, -0.1, 0.1)
    crinkle_h = np.zeros_like(L)
    if crinkle:
        act = smoothstep(0.03, 0.08, np.sqrt(gblur(rel_l * rel_l, 30 * s)))
        crinkle_h = rel2 / 0.05 * 0.04 * act * fold_ms * fade * (1.0 - dirt)
    dirt_h = 0.025 * dirt * np.clip(rel2 / 0.05, -1.0, 1.0) * smoothstep(2.0, 6.0, x_mm)
    h_mm = ((tape + hem + trim_h + fold_h + crinkle_h + dirt_h) * inside).astype(np.float32)
    h_px = h_mm * pxmm

    # Etape 6 : normale ; differences dans l'ilot (d'un seul cote au bord : pas de falaise au bord UV, pente vraie au raccord)
    dx, dy = gradient(h_px, inside)
    n = np.stack([-dx, -dy, np.ones_like(dx)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    n[~inside] = (0.0, 0.0, 1.0)

    # Etape 7 : rugosite absolue
    rough = np.full(L.shape, R_PVC, np.float32)
    rough += (R_HOLLOW - R_PVC) * smoothstep(0.0, -0.4, fold_h)
    rough -= (R_PVC - R_RIDGE) * smoothstep(0.0, 0.4, fold_h)
    rough = np.where(tape > 0.15, R_TAPE, rough)
    rough = np.where(orange | (kind_px == KINDS.index("boot")), R_ORANGE, rough)
    rough = np.where(glove | (kind_px == KINDS.index("glove")), R_GLOVE, rough)
    rough = np.where(trim, R_TRIM, rough)
    rough = np.where(sole, R_SOLE, rough)
    rough = rough * (1.0 - dirt) + R_DIRT * dirt
    rough = masked_blur(rough, isl_f, 0.7 * s)
    glass_r = np.zeros_like(L)
    if glass.any():
        ys, xs = np.nonzero(glass)
        y0, y1, x0, x1 = max(ys.min() - 2, 0), min(ys.max() + 3, size), max(xs.min() - 2, 0), min(xs.max() + 3, size)
        g = glass[y0:y1, x0:x1]
        d_in = dist_from(~g, int(max(y1 - y0, x1 - x0) // 2 + 2))
        r = 1.0 - d_in / max(float(d_in[g].max()), 1.0)  # 0 au centre de l'ovale, 1 au bord
        glass_r[y0:y1, x0:x1] = np.where(g, R_GLASS_CENTER + (R_GLASS_EDGE - R_GLASS_CENTER) * smoothstep(0.55, 1.0, r), 0.0)
    rough = np.where(glass, glass_r, rough)

    # Etape 8 : occlusion (R) et metal (B)
    ao = np.clip(1.0 - 0.35 * smoothstep(0.0, -0.45, fold_h) - 0.15 * dirt, 0.55, 1.0)
    ao = np.where(inside, ao, 1.0).astype(np.float32)
    metal = np.zeros_like(L)

    # Etape 9 : bourrage d'au moins 16 px
    ao_p, rough_p, metal_p = pad_maps([ao, rough, metal], inside | glass, int(math.ceil(12 * s)))

    n8 = (np.clip(n * 0.5 + 0.5, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
    n8[~inside] = (128, 128, 255)
    orm8 = (np.clip(np.stack([ao_p, rough_p, metal_p], -1), 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
    regions = dict(inside=inside, glass=glass, yellow=yellow, orange=orange, glove=glove, trim=trim, sole=sole, dirt=dirt,
                   taped=taped_l > 0, hem_l=hem_l > 0, ids=suit_ids, lin=lin, pxmm=pxmm, h_mm=h_mm, x_mm=x_mm, kinds=kinds,
                   pairs=seam_pairs, n_trim=n_trim, parts=dict(tape=tape, hem=hem, trim=trim_h, fold=fold_h, crinkle=crinkle_h, dirt=dirt_h))
    return n8, orm8, regions, dict(atlas_sha256=atlas_sha, masks=M)


# --------------------------------------------------------------------------------------------------- controles
def seam_continuity(R, size, n8):
    """Raccords. Pour chaque arete de couture entre deux grands ilots de la combinaison, aux quarts de l'arete : pente de la
    normal map LIVREE le long de l'arete et vers l'interieur de l'ilot, de chaque cote, lue a 1,5 et 3 px du bord puis
    extrapolee au bord (la courbure du relief n'est pas comptee comme un pli). La meme surface vue des deux cotes a la meme
    pente le long de l'arete et des pentes opposees en travers : l'ecart est l'angle du pli que le raccord dessinerait (une
    marche au raccord). L'ecart de hauteur au bord est donne pour information (une marche de hauteur seule ne se voit pas)."""
    ids, h = R["ids"], R["h_mm"]
    n = decode_normal(n8)
    gx, gy = -n[..., 0] / n[..., 2], -n[..., 1] / n[..., 2]  # pente dh/dx, dh/dy dans le repere de l'image (y vers le bas)
    rows = [(ia, ea, ib, eb, t) for (ia, _, ea), (ib, _, eb) in R["pairs"] for t in (0.25, 0.5, 0.75)]
    if not rows:
        return None

    def bilinear(f, isl, px, py):
        """Interpolation sur les seuls pixels de l'ilot (centres a +0,5)"""
        x0, y0 = int(math.floor(px - 0.5)), int(math.floor(py - 0.5))
        fx, fy = px - 0.5 - x0, py - 0.5 - y0
        acc = wsum = 0.0
        for (X, Y, w) in ((x0, y0, (1 - fx) * (1 - fy)), (x0 + 1, y0, fx * (1 - fy)), (x0, y0 + 1, (1 - fx) * fy), (x0 + 1, y0 + 1, fx * fy)):
            if 0 <= X < size and 0 <= Y < size and ids[Y, X] == isl and w > 0:
                acc += w * float(f[Y, X])
                wsum += w
        return acc / wsum if wsum > 0.25 else None

    def side(isl, e, t):
        px = (e[0] + (e[2] - e[0]) * t) * size
        py = (1.0 - (e[1] + (e[3] - e[1]) * t)) * size
        ex, ey = (e[2] - e[0]) * size, -(e[3] - e[1]) * size
        ln = math.hypot(ex, ey)
        if ln < 1e-6:
            return None
        ex, ey = ex / ln, ey / ln
        inward = None
        for sgn in (1.0, -1.0):  # normale a l'arete qui entre dans l'ilot
            qx, qy = int(math.floor(px - sgn * ey * 3.0)), int(math.floor(py + sgn * ex * 3.0))
            if 0 <= qx < size and 0 <= qy < size and ids[qy, qx] == isl:
                inward = (-sgn * ey, sgn * ex)
                break
        if inward is None:
            return None
        vals = []
        for dist in (1.5, 3.0):
            qx, qy = px + inward[0] * dist, py + inward[1] * dist
            a, b = bilinear(gx, isl, qx, qy), bilinear(gy, isl, qx, qy)
            if a is None or b is None:
                return None
            vals.append((a * ex + b * ey, a * inward[0] + b * inward[1]))
        along = 2.0 * vals[0][0] - vals[1][0]
        across = 2.0 * vals[0][1] - vals[1][1]
        hh = bilinear(h, isl, px + inward[0] * 0.75, py + inward[1] * 0.75)
        return along, across, hh if hh is not None else float("nan"), (round(px, 1), round(py, 1))

    crease, dh, where, slit, missing = [], [], [], [], 0
    for ia, ea, ib, eb, t in rows:
        A, B = side(ia, ea, t), side(ib, eb, t)
        if A is None or B is None:
            missing += 1
            continue
        crease.append(math.degrees(math.atan(math.hypot(A[1] + B[1], A[0] - B[0]))))
        dh.append(abs(A[2] - B[2]))
        where.append((ia, A[3], ib, B[3]))
        # fente : les deux cotes se touchent presque dans l'atlas (coupure de quelques pixels dans un meme panneau, souvent au
        # bord d'une pastille) : comptee a part, l'albedo y est deja continu
        slit.append(ia == ib and math.hypot(A[3][0] - B[3][0], A[3][1] - B[3][1]) < 4.0 * size / 2048.0)
    crease, dh, slit = np.array(crease), np.array(dh), np.array(slit, bool)
    seam = ~slit
    worst = [dict(deg=round(float(crease[i]), 2), a=[int(where[i][0])] + list(where[i][1]), b=[int(where[i][2])] + list(where[i][3]))
             for i in np.argsort(-np.where(seam, crease, -1.0), kind="stable")[:8]]
    dh = dh[seam & np.isfinite(dh)]
    return dict(samples=int(seam.sum()), missing=missing, crease_deg=dict(zip(("p50", "p90", "p99", "max"), pct(crease[seam], [50, 90, 99, 100]))),
                dh_mm=dict(zip(("p50", "p90", "p99"), pct(dh, [50, 90, 99]))), worst_px=worst,
                slits=dict(samples=int(slit.sum()), crease_deg=dict(zip(("p50", "p99", "max"), pct(crease[slit], [50, 99, 100])))))


def measure(n8, orm8, R, size):
    """Mesures sur les cartes QUANTIFIEES (8 bits, z reconstruit comme en BC5) : ce que le jeu lira"""
    n = decode_normal(n8)
    sl = slope_deg(n)
    ins = R["inside"]
    rough = orm8[..., 1].astype(np.float32) / 255.0
    ao = orm8[..., 0].astype(np.float32) / 255.0
    dirt_m = ins & (R["dirt"] > 0.5)
    near_tape = ins & (np.abs(R["x_mm"]) < 14.0) & (dist_from(R["taped"], 24) < 24)
    kinds = {}
    for k in R["kinds"].values():
        kinds[k] = kinds.get(k, 0) + 1
    stats = dict(
        slope_deg=dict(zip(("p50", "p90", "p99", "max"), pct(sl[ins], [50, 90, 99, 100]))),
        slope_tape_deg=dict(zip(("p50", "p99", "max"), pct(sl[near_tape], [50, 99, 100]))),
        slope_dirt_deg=dict(zip(("p50", "p99"), pct(sl[dirt_m], [50, 99]))),
        slope_yellow_clean_deg=dict(zip(("p50", "p90", "p99"), pct(sl[R["yellow"] & ~near_tape & (R["dirt"] < 0.1)], [50, 90, 99]))),
        rough=dict(zip(("min", "p5", "p50", "p95", "max"), pct(rough[ins], [0, 5, 50, 95, 100]))),
        rough_all=dict(min=round(float(rough.min()), 3), max=round(float(rough.max()), 3)),
        rough_glass=dict(zip(("min", "p50", "max"), pct(rough[R["glass"]], [0, 50, 100]))),
        rough_regions={k: round(float(np.median(rough[R[k] & ins])), 3) for k in ("yellow", "orange", "glove", "trim", "sole") if (R[k] & ins).any()},
        ao=dict(zip(("min", "p5", "p50"), pct(ao[ins], [0, 5, 50]))),
        fractions=dict(dirt_gt_half=round(float(dirt_m.sum() / max(ins.sum(), 1)), 4),
                       trim=round(float((R["trim"] & ins).sum() / max(ins.sum(), 1)), 4),
                       sole=round(float(R["sole"].sum() / max(ins.sum(), 1)), 4),
                       island_pixels=int(ins.sum()), glass_pixels=int(R["glass"].sum())),
        island_kinds=kinds,
        seams=seam_continuity(R, size, n8),
        background_flat=bool((n8[~ins] == (128, 128, 255)).all()),
        size=[int(n8.shape[1]), int(n8.shape[0])],
    )
    fails = []
    c = CHECKS
    if stats["slope_deg"]["p50"] > c["slope_p50_max"]:
        fails.append("pente mediane %.2f deg > %.1f" % (stats["slope_deg"]["p50"], c["slope_p50_max"]))
    if stats["slope_deg"]["p99"] > c["slope_p99_max"]:
        fails.append("pente p99 %.2f deg > %.1f" % (stats["slope_deg"]["p99"], c["slope_p99_max"]))
    if stats["slope_deg"]["max"] > c["slope_max"]:
        fails.append("pente max %.1f deg > %.0f" % (stats["slope_deg"]["max"], c["slope_max"]))
    sm = stats["seams"]
    if sm is not None and (sm["crease_deg"]["p99"] is None or sm["crease_deg"]["p99"] > c["seam_crease_p99_max_deg"]):
        fails.append("raccords : pli p99 %s deg > %.1f (marche au raccord UV)" % (sm["crease_deg"]["p99"], c["seam_crease_p99_max_deg"]))
    if stats["rough_all"]["min"] < c["rough_min"] or stats["rough_all"]["max"] > c["rough_max"]:
        fails.append("rugosite hors de [%.2f ; %.2f] : %.3f .. %.3f" % (c["rough_min"], c["rough_max"], stats["rough_all"]["min"], stats["rough_all"]["max"]))
    if not stats["background_flat"]:
        fails.append("fond non plat")
    if stats["size"] != [size, size]:
        fails.append("dimensions %s au lieu de %d" % (stats["size"], size))
    return stats, fails


# --------------------------------------------------------------------------------------------------- apercus
def write_previews(folder, n8, orm8, R):
    """Apercus hors depot : lumiere rasante appliquee a l'atlas (deux directions, GGX avec la rugosite de la carte), normale,
    occlusion, rugosite, masques. Le dossier ne peut pas etre dans le depot."""
    folder = os.path.abspath(folder)
    if os.path.commonpath([folder, ROOT]) == ROOT:
        raise SystemExit("ECHEC : les apercus vont hors du depot (scratchpad)")
    os.makedirs(folder, exist_ok=True)
    n = decode_normal(n8)
    lin = R["lin"]
    rough = orm8[..., 1].astype(np.float32) / 255.0
    ao = orm8[..., 0].astype(np.float32) / 255.0
    view = np.array([0.0, 0.0, 1.0], np.float32)
    for tag, ldir in (("rasante_haut", (0.0, -0.80, 0.40)), ("rasante_gauche", (-0.80, 0.15, 0.45))):
        lv = np.array(ldir, np.float32)
        lv /= np.linalg.norm(lv)
        hv = (lv + view) / np.linalg.norm(lv + view)
        ndl = np.clip(n @ lv, 0.0, 1.0)
        ndh = np.clip(n @ hv, 0.0, 1.0)
        a2 = np.maximum(rough * rough, 1e-3) ** 2
        ggx = a2 / (math.pi * (ndh * ndh * (a2 - 1.0) + 1.0) ** 2)
        spec = 0.04 * ggx * ndl * 0.25
        col = lin * (ndl * 2.2 * ao + 0.08)[..., None] + spec[..., None] * 3.0
        Image.fromarray((np.clip(col, 0.0, 1.0) ** (1.0 / 2.2) * 255.0 + 0.5).astype(np.uint8)).save(os.path.join(folder, "apercu_%s.png" % tag))
    Image.fromarray(n8).save(os.path.join(folder, "apercu_normale.png"))
    for i, name in enumerate(("occlusion", "rugosite")):
        Image.fromarray(orm8[..., i]).save(os.path.join(folder, "apercu_%s.png" % name))
    masks = np.stack([np.clip(R["dirt"], 0, 1), R["trim"].astype(np.float32) * 0.8 + R["sole"] * 0.4,
                      R["taped"].astype(np.float32) + R["hem_l"] * 0.5], -1)
    Image.fromarray((np.clip(masks, 0, 1) * 255).astype(np.uint8)).save(os.path.join(folder, "apercu_masques.png"))
    print("apercus ->", folder)


# --------------------------------------------------------------------------------------------------- ecriture
def png_bytes(arr):
    buf = io.BytesIO()
    Image.fromarray(arr, "RGB").save(buf, format="PNG", compress_level=9)
    return buf.getvalue()


def safe_path(name):
    path = os.path.join(OUT, name)
    if os.path.commonpath([os.path.abspath(path), PROTECTED_TEX]) == PROTECTED_TEX:
        raise SystemExit("ECHEC : ecriture refusee dans RawAssets/Textures")
    if not protect_assets.may_write(path):
        raise SystemExit("ECHEC : %s est protege" % rel(path))
    return path


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--size", type=int, default=2048)
    ap.add_argument("--crinkle", type=int, choices=(0, 1), default=0, help="rides fines (0 par defaut)")
    ap.add_argument("--check", action="store_true", help="controles seuls, compare aux cartes livrees, n'ecrit rien")
    ap.add_argument("--preview", default="", help="dossier des apercus (hors du depot)")
    args = ap.parse_args()
    if args.size < 256 or args.size & (args.size - 1):
        raise SystemExit("--size : puissance de deux >= 256")

    n8, orm8, R, prov = derive(args.size, args.crinkle)
    stats, fails = measure(n8, orm8, R, args.size)
    M = prov["masks"]
    report = dict(
        script="Tools/derive_hazmat_maps.py", script_version=SCRIPT_VERSION, numpy=np.__version__, pillow=Image.__version__,
        atlas=ATLAS_REL, atlas_sha256=prov["atlas_sha256"], masks=M["source"], masks_files=M["files"], fbx_sha256=M["fbx_sha256"],
        params=dict(size=args.size, crinkle=args.crinkle,
                    rough=dict(pvc=R_PVC, ridge=R_RIDGE, hollow=R_HOLLOW, tape=R_TAPE, orange=R_ORANGE, glove=R_GLOVE, trim=R_TRIM,
                               sole=R_SOLE, dirt=R_DIRT, glass_center=R_GLASS_CENTER, glass_edge=R_GLASS_EDGE),
                    relief_mm=dict(tape=[TAPE_H, TAPE_CREST, TAPE_IN, TAPE_OUT, TAPE_CREST_W], hem=[HEM_H, HEM_AT, HEM_W], trim=TRIM_H,
                                   fold=[FOLD_FINE, FOLD_MID, FOLD_GAIN, FOLD_CLIP], edge_sigma_px=EDGE_SIGMA),
                    sole_rule="os %s, hauteur relative max < %.2f" % ("/".join(SOLE_BONES), SOLE_ZREL), checks=CHECKS),
        outputs={NAME_N: dict(format="PNG RGB 8 bits, normale tangente DirectX", pixels_sha256=sha256_pixels(n8)),
                 NAME_ORM: dict(format="PNG RGB 8 bits lineaire : R occlusion, G rugosite absolue, B metal", pixels_sha256=sha256_pixels(orm8))},
        import_unreal=dict(N="srgb False, TC_Normalmap (BC5), groupe WorldNormalMap, pas d'inversion du vert",
                           ORM="srgb False, compression par defaut, composite_texture T_Hazmat_Suit_N, CTM_NormalRoughnessToGreen, composite_power 1"),
        stats=stats)
    st = stats
    print("pente (deg) dans les ilots : p50 %.2f p90 %.2f p99 %.2f max %.1f ; PVC propre p99 %.2f ; salissure p99 %s"
          % (st["slope_deg"]["p50"], st["slope_deg"]["p90"], st["slope_deg"]["p99"], st["slope_deg"]["max"],
             st["slope_yellow_clean_deg"]["p99"], st["slope_dirt_deg"]["p99"]))
    if st["seams"]:
        sm = st["seams"]
        print("raccords : %d points (%d non mesures), pli p50 %s p90 %s p99 %s max %s deg ; ecart de hauteur p50 %s p99 %s mm ;"
              " fentes : %d points, pli p99 %s max %s deg"
              % (sm["samples"], sm["missing"], sm["crease_deg"]["p50"], sm["crease_deg"]["p90"], sm["crease_deg"]["p99"],
                 sm["crease_deg"]["max"], sm["dh_mm"]["p50"], sm["dh_mm"]["p99"], sm["slits"]["samples"], sm["slits"]["crease_deg"]["p99"],
                 sm["slits"]["crease_deg"]["max"]))
    print("rugosite dans les ilots : p5 %.3f p50 %.3f p95 %.3f ; visiere %s ; regions %s"
          % (st["rough"]["p5"], st["rough"]["p50"], st["rough"]["p95"], st["rough_glass"], st["rough_regions"]))
    print("salissure > 0,5 : %.1f %% ; bandes : %.1f %% ; semelles : %.1f %% ; ilots %s ; masques : %s"
          % (100 * st["fractions"]["dirt_gt_half"], 100 * st["fractions"]["trim"], 100 * st["fractions"]["sole"], st["island_kinds"], M["source"]))
    if args.preview:
        write_previews(args.preview, n8, orm8, R)

    if args.check:
        p_json = os.path.join(OUT, NAME_JSON)
        for name, arr in ((NAME_N, n8), (NAME_ORM, orm8)):
            path = os.path.join(OUT, name)
            if not os.path.isfile(path):
                fails.append("%s absent" % rel(path))
                continue
            disk = np.asarray(Image.open(path).convert("RGB"))
            if disk.shape != arr.shape or not np.array_equal(disk, arr):
                diff = int((disk != arr).any(-1).sum()) if disk.shape == arr.shape else -1
                fails.append("%s differe du calcul (%d pixels) : relancer le script" % (name, diff))
        if os.path.isfile(p_json):
            with open(p_json, encoding="utf-8") as fh:
                old = json.load(fh)
            old_px = {k: v.get("pixels_sha256") for k, v in old.get("outputs", {}).items()}
            new_px = {k: v["pixels_sha256"] for k, v in report["outputs"].items()}
            if old_px != new_px or old.get("params") != report["params"] or old.get("atlas_sha256") != report["atlas_sha256"]:
                fails.append("%s ne decrit pas les cartes calculees" % NAME_JSON)
        else:
            fails.append("%s absent" % NAME_JSON)
    if sha256_file(ATLAS) != prov["atlas_sha256"]:
        fails.append("l'atlas protege a change pendant le calcul")
    if fails:
        for f in fails:
            print("ECHEC :", f)
        if not args.check:
            print("rien n'est ecrit")
        sys.exit(1)
    if not args.check:
        os.makedirs(OUT, exist_ok=True)
        for name, arr in ((NAME_N, n8), (NAME_ORM, orm8)):
            data = png_bytes(arr)
            with open(safe_path(name), "wb") as fh:
                fh.write(data)
            report["outputs"][name]["file_sha256"] = hashlib.sha256(data).hexdigest()
        with open(safe_path(NAME_JSON), "w", encoding="utf-8") as fh:
            json.dump(report, fh, indent=1, sort_keys=True, ensure_ascii=True)
            fh.write("\n")
        print("->", rel(OUT), "(%s, %s, %s)" % (NAME_N, NAME_ORM, NAME_JSON))
    print("controles : OK")


if __name__ == "__main__":
    main()
