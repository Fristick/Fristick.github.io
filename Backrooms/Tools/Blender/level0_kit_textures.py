"""v4.13 : textures PBR du kit Blender du Level 0 (numpy + Pillow, sans Blender).

Appele par build_level0_kit.py ; utilisable seul :  python Tools/Blender/level0_kit_textures.py <dossier_de_sortie>

Les textures du projet autorisees (RawAssets/Textures/T_L0_*, T_Concrete*, non protegees) servent de base ; elles ne sont
jamais modifiees : chaque carte du kit est un nouveau fichier T_L0K_* dans le dossier de sortie. Tout ce qui est
procedural (raccords de papier peint, humidite, taches, grain, relief) est calcule ici et enregistre en image : rien ne
depend d'un noeud Blender, l'export vers Unreal ne perd rien.

Conventions :
 - BaseColor : sRGB (JPEG qualite 92) ; ORM : R = occlusion ambiante, G = rugosite, B = metal (lineaire) ;
 - surfaces peintes ou rugueuses (papier peint, moquette, dalles, beton, plinthe, luminaire, porte peinte) : ORM
   (JPEG qualite 95) et Normal (JPEG qualite 90) sans sous-echantillonnage des couleurs (4:4:4) : sur ces surfaces peu
   brillantes, les blocs du JPEG ne se voient pas et le kit reste leger ; metal (quincaillerie) : PNG sans perte ;
 - Normal : espace tangent, convention DirectX (vert vers le bas de l'image), comme les cartes du projet
   (Tools/generate_textures.py) ; Unreal l'utilise telle quelle ; le materiau Blender inverse le vert ;
 - Decals : RGBA (alpha = masque), PNG ;
 - 2048 px pour les grandes surfaces (papier peint, moquette, dalles, beton), 1024 px pour les petites (plinthe,
   luminaire, porte, quincaillerie), 512 px pour le diffuseur emissif ;
 - v4.15 : papier peint, moquette et dalles reprennent la couleur, la normal map et la rugosite du projet (meme matiere
   que les surfaces du jeu, voir project_material).
Les materiaux non metalliques ont un canal metal a 0 ; seule la quincaillerie de la porte est metallique.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageFile

ImageFile.MAXBLOCK = 1 << 26  # JPEG optimise d'une grande image : tampon d'ecriture suffisant

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(ROOT, "RawAssets", "Textures")
RNG = np.random.default_rng(4130)


# ----------------------------------------------------------------------------------------------- outils
def load(name, size=None, mode="RGB"):
    im = Image.open(os.path.join(SRC, name)).convert(mode)
    if size and im.size != (size, size):
        im = im.resize((size, size), Image.LANCZOS)
    return np.asarray(im).astype(np.float32) / 255.0


def to_img(a, mode="RGB"):
    return Image.fromarray((np.clip(a, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8), mode)


def blur_wrap(a, r):
    """Flou gaussien periodique (FFT) : la texture reste raccordable"""
    if r <= 0:
        return a
    S0, S1 = a.shape[:2]
    fy = np.fft.fftfreq(S0)[:, None]
    fx = np.fft.fftfreq(S1)[None, :]
    g = np.exp(-2.0 * (np.pi ** 2) * (r ** 2) * (fx ** 2 + fy ** 2))
    if a.ndim == 3:
        return np.stack([np.real(np.fft.ifft2(np.fft.fft2(a[..., c]) * g)) for c in range(a.shape[2])], -1)
    return np.real(np.fft.ifft2(np.fft.fft2(a) * g))


def noise(size, scale, octaves=4, seed=0):
    """Bruit fractal periodique (somme de bruits filtres), dans [0, 1]"""
    rng = np.random.default_rng(seed)
    acc = np.zeros((size, size), np.float32)
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        w = rng.standard_normal((size, size)).astype(np.float32)
        acc += blur_wrap(w, scale / (2 ** o)) * amp * (2 ** o) ** 0.5
        tot += amp
        amp *= 0.55
    acc -= acc.min()
    acc /= max(1e-6, acc.max())
    return acc


def normal_from_height(h, strength):
    """Normale DirectX (x a droite, y vers le bas de l'image) depuis une hauteur periodique, comme le projet"""
    S = h.shape[0]
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    k = strength * S / 512.0
    nx, ny, nz = -dx * k, -dy * k, np.ones_like(h)
    ln = np.sqrt(nx ** 2 + ny ** 2 + nz ** 2)
    return np.stack([nx / ln, ny / ln, nz / ln], -1) * 0.5 + 0.5


def ao_from_height(h, radius, power):
    """Occlusion de cavite : creux plus sombres que leur voisinage"""
    m = blur_wrap(h, radius)
    return np.clip(1.0 - np.maximum(0.0, m - h) * power, 0.0, 1.0)


def quantize(a, levels):
    """Reduit le grain invisible (taille des PNG) sans toucher au relief perceptible"""
    return np.round(a * (levels - 1)) / (levels - 1)


class Writer:
    def __init__(self, out):
        self.out = out
        os.makedirs(out, exist_ok=True)
        self.files = []

    def color(self, name, rgb):
        p = os.path.join(self.out, name + ".jpg")
        to_img(rgb).save(p, quality=92, optimize=True, subsampling=0)
        self.files.append(p)

    def png(self, name, a, mode="RGB"):
        p = os.path.join(self.out, name + ".png")
        to_img(a, mode).save(p, optimize=True, compress_level=9)
        self.files.append(p)

    def data_jpg(self, name, a, quality=95):
        p = os.path.join(self.out, name + ".jpg")
        to_img(a).save(p, quality=quality, optimize=True, subsampling=0)
        self.files.append(p)

    def material(self, name, base, rough, normal, ao=None, metal=None, lossless=False):
        S = base.shape[0]
        ao = np.ones((S, S), np.float32) if ao is None else ao
        metal = np.zeros((S, S), np.float32) if metal is None else metal
        self.color(name + "_BaseColor", base)
        # Le grain d'un pixel de la rugosite et de l'occlusion ne se voit pas (mais pese lourd) : lisse d'un pixel
        orm = np.stack([blur_wrap(ao, 1.0), blur_wrap(rough, 1.0), metal], -1)
        if lossless:
            self.png(name + "_ORM", quantize(orm, 64))
            self.png(name + "_Normal", quantize(normal, 128))
        else:
            self.data_jpg(name + "_ORM", orm)
            self.data_jpg(name + "_Normal", normal, 90)


# ----------------------------------------------------------------------------------------------- materiaux
def load_rough(name, nominal, size):
    """v4.15 : rugosite absolue d'apres la carte relative du projet (<nom>_R.png de Tools/generate_roughness.py : 0,5 = valeur
    nominale de la surface, +-0,5 = +-1)"""
    return nominal + (load(name + "_R.png", size, "L") - 0.5) * 2.0


def project_material(w, kit_name, name, nominal, rough_range, S, ao_radius, ao_power, blur_hi):
    """v4.15 : papier peint, moquette et dalles reprennent la couleur, la normal map et la rugosite du projet
    (Tools/generate_textures.py, generate_roughness.py) : le kit et les boites du jeu ont la meme matiere (raccords des les
    de 53 cm, moquette bouclee, dalles fissurees de 61 cm), a la meme echelle (le materiau du kit les projette dans
    l'espace monde : 106, 160 et 122 cm par repetition). L'occlusion vient du relief de la couleur (creux plus sombres)."""
    base = load(name + ".jpg", S)
    lum = base.mean(-1)
    h = blur_wrap(lum, 1.0) - blur_wrap(lum, blur_hi)
    rough = np.clip(load_rough(name, nominal, S), *rough_range)
    w.material(kit_name, base, rough, load(name + "_N.png", S), ao_from_height(h, ao_radius, ao_power))


def wallpaper(w, S=2048):
    """Papier peint jaune du projet : deux les de 53 cm par repetition (106 cm), raccords et bords souleves inclus"""
    project_material(w, "T_L0K_Wallpaper", "T_L0_Wallpaper", 0.80, (0.5, 0.95), S, 6, 1.6, 24.0)


def carpet(w, S=2048):
    """Moquette bouclee du projet (160 cm par repetition), mate ; les grandes taches humides sont des decalques"""
    project_material(w, "T_L0K_Carpet", "T_L0_Carpet", 0.95, (0.85, 0.99), S, 4, 2.0, 16.0)


def ceiling(w, S=2048):
    """Dalles de plafond fissurees de 61 cm du projet (2 x 2 dalles par repetition de 122 cm) et leur ossature en T"""
    project_material(w, "T_L0K_CeilingTile", "T_L0_Ceiling", 0.90, (0.4, 0.97), S, 5, 2.0, 16.0)


def concrete(w, S=2048):
    """Beton des parois des fosses et du sas (le degrade sombre de la profondeur est dans les couleurs de sommets)"""
    base = load("T_Concrete.jpg", S)
    n1 = noise(S, 18, 4, 41)
    n2 = noise(S, 1.5, 3, 42)
    tone = 0.82 + 0.25 * n1
    base = base * tone[..., None] * np.array([0.92, 0.92, 0.9])
    h = blur_wrap(n2, 0.8) * 0.6 + n1 * 0.4
    h -= blur_wrap(h, 20)
    rough = np.clip(0.82 + (n2 - 0.5) * 0.15, 0.7, 0.95)
    w.material("T_L0K_Concrete", base, rough, normal_from_height(h, 3.0), ao_from_height(h, 6, 2.2))


def small(w, name, color, rough_mean, metal, scale, S=1024, seed=0, streaks=False):
    n = noise(S, scale, 3, seed)
    fine = noise(S, 0.8, 2, seed + 1)
    if streaks:
        # metal brosse : stries dans une direction
        st = blur_wrap(RNG.standard_normal((S, S)).astype(np.float32), 0.5)
        st = blur_wrap(np.repeat(st[:, :1], S, 1) * 0.0 + st, 0.0)
        st = np.real(np.fft.ifft2(np.fft.fft2(st) * np.exp(-((np.fft.fftfreq(S)[None, :] * 600) ** 2))))
        fine = (st - st.min()) / max(1e-6, st.max() - st.min())
    base = np.ones((S, S, 3), np.float32) * np.array(color, np.float32) * (0.93 + 0.1 * n[..., None])
    rough = np.clip(rough_mean + (fine - 0.5) * 0.12 + (n - 0.5) * 0.08, 0.05, 1.0)
    h = fine * 0.4 + n * 0.2
    w.material(name, base, rough, normal_from_height(h, 1.2), ao_from_height(h, 6, 1.0),
               np.full((S, S), 1.0 if metal else 0.0, np.float32), lossless=metal)


def diffuser(w, S=512):
    """Diffuseur des neons : prismes fins ; emissif (blanc chaud tres legerement verdatre)"""
    y = np.arange(S) / S
    prism = 0.85 + 0.15 * np.abs(np.sin(y * np.pi * 48))[:, None] * np.ones((1, S))
    edge = np.ones((S, S), np.float32)
    m = 10
    edge[:m, :] = edge[-m:, :] = 0.75
    edge[:, :m] = edge[:, -m:] = 0.75
    tint = np.array([1.0, 1.0, 0.88], np.float32)
    w.color("T_L0K_Diffuser_BaseColor", (prism * edge)[..., None] * tint * 0.9)
    w.png("T_L0K_Diffuser_Emissive", (prism * edge)[..., None] * tint)


def decal(w, name, S, color, rings, blotch, seed):
    """Aureole, tache de plafond, zone humide : RGBA, l'alpha est le masque (bords irreguliers, cernes)"""
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float32) / S - 0.5
    n = noise(S, 22, 4, seed)
    r = np.sqrt(xx ** 2 + yy ** 2) * 2.0 + (n - 0.5) * 0.55
    body = np.clip(1.0 - r, 0.0, 1.0) ** 0.8 * blotch
    ring = np.zeros_like(r)
    for k in range(rings):
        rr = 0.55 + 0.17 * k
        ring += np.exp(-((r - rr) * 26) ** 2) * (0.7 - 0.15 * k)
    a = np.clip(body * 0.55 + ring * 0.6, 0.0, 1.0) * np.clip((1.0 - r) * 4.0, 0.0, 1.0)
    rgb = np.ones((S, S, 3), np.float32) * np.array(color, np.float32) * (0.85 + 0.3 * n[..., None])
    w.png(name, np.concatenate([rgb, a[..., None]], -1), "RGBA")


def build_all(out):
    w = Writer(out)
    wallpaper(w)
    carpet(w)
    ceiling(w)
    concrete(w)
    small(w, "T_L0K_Baseboard", (0.42, 0.36, 0.22), 0.55, False, 30, seed=51)
    small(w, "T_L0K_FixtureMetal", (0.86, 0.85, 0.80), 0.4, False, 40, seed=61)
    small(w, "T_L0K_DoorSteel", (0.40, 0.46, 0.43), 0.5, False, 26, seed=71)
    small(w, "T_L0K_Hardware", (0.78, 0.77, 0.73), 0.32, True, 30, seed=81, streaks=True)
    diffuser(w)
    decal(w, "T_L0K_Decal_WaterStain", 1024, (0.42, 0.33, 0.16), 3, 0.35, 91)
    decal(w, "T_L0K_Decal_CeilingStain", 1024, (0.48, 0.38, 0.20), 2, 0.5, 92)
    decal(w, "T_L0K_Decal_CarpetDamp", 1024, (0.30, 0.26, 0.17), 1, 0.8, 93)
    return w.files


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "RawAssets", "Level0Kit", "Textures")
    files = build_all(out)
    tot = 0
    for f in files:
        sz = os.path.getsize(f)
        tot += sz
        print("%-48s %7.2f Mo" % (os.path.basename(f), sz / 1e6))
    print("total %.1f Mo, %d fichiers" % (tot / 1e6, len(files)))
