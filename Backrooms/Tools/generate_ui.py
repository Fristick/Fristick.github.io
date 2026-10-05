"""
Ressources du menu et de l'interface (v4.0) : logo, icones des boutons, degrades et formes douces.

Usage :  python generate_ui.py        (numpy + pillow ; police Inter, licence SIL OFL, fournie avec Blender)
Sortie : ../RawAssets/Icons/UI_*.png  (importees comme textures d'interface, sans compression)
"""
import glob
import math
import os

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "RawAssets", "Icons")
TEX = os.path.join(HERE, "..", "RawAssets", "Textures")
os.makedirs(OUT, exist_ok=True)


def find_inter():
    """Inter (OFL) : fournie avec Blender (module pip bpy ou installation), sinon DejaVu"""
    pats = ["/usr/local/lib/python3*/dist-packages/bpy/*/datafiles/fonts/Inter.woff2",
            "C:/Program Files/Blender Foundation/Blender*/*/datafiles/fonts/Inter.woff2",
            "/Applications/Blender.app/Contents/Resources/*/datafiles/fonts/Inter.woff2",
            os.path.join(HERE, "Fonts", "Inter*.ttf")]
    for p in pats:
        hits = sorted(glob.glob(p))
        if hits:
            return hits[-1]
    return "DejaVuSans-Bold.ttf"


INTER = find_inter()


def font(size, weight="Regular"):
    f = ImageFont.truetype(INTER, size)
    try:
        f.set_variation_by_name(weight)
    except Exception:
        pass
    return f


def save(img, name):
    path = os.path.join(OUT, name + ".png")
    img.save(path, optimize=True)
    print("  ->", os.path.relpath(path), img.size)


def text_mask(text, fnt, spacing=0, pad=40):
    """Masque (L) d'un texte avec espacement des lettres"""
    widths = [fnt.getbbox(c)[2] - fnt.getbbox(c)[0] if c != " " else fnt.size // 3 for c in text]
    adv = [fnt.getlength(c) for c in text]
    asc, desc = fnt.getmetrics()
    w = int(sum(adv) + spacing * (len(text) - 1) + 2 * pad)
    h = asc + desc + 2 * pad
    m = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(m)
    x = pad
    for c, a in zip(text, adv):
        d.text((x, pad), c, font=fnt, fill=255)
        x += a + spacing
    del widths
    return m.crop(m.getbbox())


# ---------------------------------------------------------------------------
# Logo
# ---------------------------------------------------------------------------
def logo():
    S = 2  # sur-echantillonnage
    big = text_mask("BACKROOMS", font(230 * S, "Black"), spacing=10 * S)
    small = text_mask("T H E", font(70 * S, "SemiBold"), spacing=14 * S)
    pad = 90 * S
    W = big.width + 2 * pad
    gap = 22 * S
    H = small.height + gap + big.height + 2 * pad
    mask = Image.new("L", (W, H), 0)
    mask.paste(small, ((W - small.width) // 2, pad))
    by = pad + small.height + gap
    mask.paste(big, (pad, by))

    # Remplissage : le papier peint du Niveau 0, rechauffe, plus clair en haut
    wall_path = os.path.join(TEX, "T_L0_Wallpaper.jpg")
    if os.path.isfile(wall_path):
        wall = Image.open(wall_path).convert("RGB").resize((W // 2, W // 2), Image.LANCZOS)
        tiles = Image.new("RGB", (W, H))
        for ty in range(0, H, wall.height):
            for tx in range(0, W, wall.width):
                tiles.paste(wall, (tx, ty))
        fill = np.asarray(tiles).astype(np.float32) / 255.0
    else:
        fill = np.ones((H, W, 3), np.float32) * np.array([0.85, 0.78, 0.4])
    yy = np.linspace(0, 1, H)[:, None, None]
    warm = np.array([1.18, 1.05, 0.62])
    fill = np.clip(fill * warm * (1.25 - 0.45 * yy) + 0.06, 0, 1)
    # Reflet de neon : bande claire horizontale sur le haut des lettres
    band = np.exp(-((yy - (by + big.height * 0.22) / H) / 0.035) ** 2)
    fill = np.clip(fill + band * 0.25, 0, 1)

    m = np.asarray(mask).astype(np.float32) / 255.0
    # Contour sombre et ombre portee
    outline = np.asarray(mask.filter(ImageFilter.MaxFilter(9 * S + 1))).astype(np.float32) / 255.0
    shadow = np.asarray(Image.fromarray((outline * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(14 * S))).astype(np.float32) / 255.0
    shadow = np.roll(shadow, 12 * S, axis=0)
    glow = np.asarray(mask.filter(ImageFilter.GaussianBlur(40 * S))).astype(np.float32) / 255.0

    rgb = np.zeros((H, W, 3), np.float32)
    a = np.zeros((H, W), np.float32)
    # lueur jaune (neon) puis ombre, contour, lettres
    glow_c = np.array([1.0, 0.82, 0.35])
    a_g = np.clip(glow * 0.55, 0, 1)
    rgb = rgb * (1 - a_g[..., None]) + glow_c * a_g[..., None]
    a = a + a_g * (1 - a)
    a_s = np.clip(shadow * 0.7, 0, 1)
    rgb = rgb * (1 - a_s[..., None]) + np.array([0.0, 0.0, 0.0]) * a_s[..., None]
    a = a + a_s * (1 - a)
    rgb = rgb * (1 - outline[..., None]) + np.array([0.09, 0.07, 0.02]) * outline[..., None]
    a = a + outline * (1 - a)
    rgb = rgb * (1 - m[..., None]) + fill * m[..., None]

    # Glitch "noclip" : deux fines tranches du mot decalees, frangees de rouge et de cyan hors des lettres
    rng = np.random.default_rng(37)
    for _ in range(2):
        y0 = int(by + rng.uniform(0.25, 0.75) * big.height)
        hh = int(rng.uniform(4, 8) * S)
        dx = int(rng.choice([-1, 1]) * rng.uniform(10, 16) * S)
        sl = slice(y0, min(H, y0 + hh))
        rgb[sl] = np.roll(rgb[sl], dx, axis=1)
        a[sl] = np.roll(a[sl], dx, axis=1)
        inside = np.roll(m[sl], dx, axis=1)
        red = np.roll(m[sl], dx - 5 * S, axis=1) * (1 - inside)
        cyan = np.roll(m[sl], dx + 5 * S, axis=1) * (1 - inside)
        fr = np.clip(red + cyan, 0, 1) * 0.85
        col = (red[..., None] * np.array([1.0, 0.15, 0.1]) + cyan[..., None] * np.array([0.1, 0.9, 1.0])) / np.maximum(red + cyan, 1e-4)[..., None]
        rgb[sl] = rgb[sl] * (1 - fr[..., None]) + col * fr[..., None]
        a[sl] = np.maximum(a[sl], fr)

    img = Image.fromarray(np.dstack([np.clip(rgb, 0, 1), np.clip(a, 0, 1)]).__mul__(255).astype(np.uint8), "RGBA")
    img = img.resize((W // S, H // S), Image.LANCZOS)
    save(img.crop(img.getbbox()), "UI_Logo")


# ---------------------------------------------------------------------------
# Icones (glyphes blancs sur fond transparent, teintes a l'affichage)
# ---------------------------------------------------------------------------
N = 128
SS = 4


def canvas():
    im = Image.new("L", (N * SS, N * SS), 0)
    return im, ImageDraw.Draw(im)


def finish(im, name):
    a = im.resize((N, N), Image.LANCZOS)
    out = Image.merge("RGBA", (Image.new("L", (N, N), 255),) * 3 + (a,))
    save(out, name)


def P(*v):
    return [c * SS for c in v]


def person(d, cx, cy, s, fill=255):
    d.ellipse(P(cx - 17 * s, cy - 44 * s, cx + 17 * s, cy - 10 * s), fill=fill)
    d.pieslice(P(cx - 36 * s, cy - 2 * s, cx + 36 * s, cy + 70 * s), 180, 360, fill=fill)


def icon_solo():
    im, d = canvas()
    person(d, 64, 62, 1.15)
    finish(im, "UI_IconSolo")


def icon_multi():
    im, d = canvas()
    person(d, 44, 60, 0.95)
    person(d, 88, 60, 0.95)
    person(d, 66, 70, 1.25, fill=0)  # detoure le personnage du devant
    person(d, 66, 72, 1.1)
    finish(im, "UI_IconMulti")


def icon_settings():
    im, d = canvas()
    cx, cy = 64 * SS, 64 * SS
    for k in range(8):
        a = k * math.pi / 4
        pts = []
        for da, r in ((-0.2, 30), (-0.13, 50), (0.13, 50), (0.2, 30)):
            pts.append((cx + math.cos(a + da) * r * SS, cy + math.sin(a + da) * r * SS))
        d.polygon(pts, fill=255)
    d.ellipse((cx - 40 * SS, cy - 40 * SS, cx + 40 * SS, cy + 40 * SS), fill=255)
    d.ellipse((cx - 17 * SS, cy - 17 * SS, cx + 17 * SS, cy + 17 * SS), fill=0)
    finish(im, "UI_IconSettings")


def icon_quit():
    im, d = canvas()
    d.rounded_rectangle(P(20, 16, 76, 112), radius=8 * SS, outline=255, width=10 * SS)
    d.rectangle(P(60, 40, 90, 88), fill=0)
    d.line(P(48, 64, 104, 64), fill=255, width=11 * SS)
    d.polygon(P(112, 64, 88, 42, 88, 86), fill=255)
    finish(im, "UI_IconQuit")


def icon_host():
    im, d = canvas()
    d.ellipse(P(52, 52, 76, 76), fill=255)
    for r in (30, 50):
        d.arc(P(64 - r, 64 - r, 64 + r, 64 + r), 140, 220, fill=255, width=10 * SS)
        d.arc(P(64 - r, 64 - r, 64 + r, 64 + r), -40, 40, fill=255, width=10 * SS)
    d.polygon(P(64, 70, 50, 116, 78, 116), fill=255)
    finish(im, "UI_IconHost")


def icon_join():
    im, d = canvas()
    d.rounded_rectangle(P(52, 16, 108, 112), radius=8 * SS, outline=255, width=10 * SS)
    d.rectangle(P(40, 40, 64, 88), fill=0)
    d.line(P(14, 64, 70, 64), fill=255, width=11 * SS)
    d.polygon(P(84, 64, 60, 42, 60, 86), fill=255)
    finish(im, "UI_IconJoin")


def icon_back():
    im, d = canvas()
    d.line(P(78, 26, 40, 64), fill=255, width=14 * SS)
    d.line(P(40, 64, 78, 102), fill=255, width=14 * SS)
    d.ellipse(P(33, 57, 47, 71), fill=255)
    finish(im, "UI_IconBack")


def icon_play():
    im, d = canvas()
    d.polygon(P(36, 20, 108, 64, 36, 108), fill=255)
    finish(im, "UI_IconPlay")


def icon_tip():
    im, d = canvas()
    d.ellipse(P(30, 12, 98, 80), fill=255)
    d.polygon(P(44, 66, 84, 66, 78, 92, 50, 92), fill=255)
    d.rounded_rectangle(P(48, 96, 80, 106), radius=3 * SS, fill=255)
    d.rounded_rectangle(P(52, 110, 76, 118), radius=3 * SS, fill=255)
    d.arc(P(44, 26, 84, 66), 200, 260, fill=0, width=6 * SS)
    finish(im, "UI_IconTip")


def icon_arrow():
    im, d = canvas()
    d.line(P(50, 26, 88, 64), fill=255, width=14 * SS)
    d.line(P(88, 64, 50, 102), fill=255, width=14 * SS)
    d.ellipse(P(81, 57, 95, 71), fill=255)
    finish(im, "UI_IconArrow")


def icon_keys():
    im, d = canvas()
    d.rounded_rectangle(P(8, 30, 120, 98), radius=12 * SS, outline=255, width=8 * SS)
    for row, (y, n, x0) in enumerate(((44, 6, 20), (60, 6, 26))):
        for k in range(n):
            x = x0 + k * 15
            d.rounded_rectangle(P(x, y, x + 10, y + 10), radius=2 * SS, fill=255)
    d.rounded_rectangle(P(34, 77, 94, 86), radius=3 * SS, fill=255)
    finish(im, "UI_IconKeys")


def icon_lock():
    im, d = canvas()
    d.arc(P(38, 14, 90, 70), 180, 360, fill=255, width=12 * SS)
    d.line(P(44, 42, 44, 58), fill=255, width=12 * SS)
    d.line(P(84, 42, 84, 58), fill=255, width=12 * SS)
    d.rounded_rectangle(P(26, 56, 102, 116), radius=10 * SS, fill=255)
    d.ellipse(P(57, 72, 71, 86), fill=0)
    d.rectangle(P(61, 80, 67, 100), fill=0)
    finish(im, "UI_IconLock")


def icon_trash():
    im, d = canvas()
    d.rounded_rectangle(P(20, 26, 108, 38), radius=4 * SS, fill=255)
    d.rounded_rectangle(P(50, 14, 78, 30), radius=4 * SS, fill=255)
    d.polygon(P(30, 44, 98, 44, 92, 116, 36, 116), fill=255)
    for x in (50, 64, 78):
        d.line(P(x, 56, x, 104), fill=0, width=6 * SS)
    finish(im, "UI_IconTrash")


def icon_plus():
    im, d = canvas()
    d.rounded_rectangle(P(56, 18, 72, 110), radius=7 * SS, fill=255)
    d.rounded_rectangle(P(18, 56, 110, 72), radius=7 * SS, fill=255)
    finish(im, "UI_IconPlus")


def icon_check():
    im, d = canvas()
    d.line(P(22, 68, 52, 98), fill=255, width=16 * SS)
    d.line(P(52, 98, 108, 34), fill=255, width=16 * SS)
    d.ellipse(P(44, 90, 60, 106), fill=255)
    finish(im, "UI_IconCheck")


def icon_save():
    im, d = canvas()
    d.rounded_rectangle(P(16, 16, 112, 112), radius=12 * SS, fill=255)
    d.rectangle(P(36, 16, 86, 46), fill=0)
    d.rectangle(P(70, 22, 80, 40), fill=255)
    d.rounded_rectangle(P(32, 62, 96, 104), radius=4 * SS, fill=0)
    d.line(P(42, 76, 86, 76), fill=255, width=5 * SS)
    d.line(P(42, 90, 86, 90), fill=255, width=5 * SS)
    finish(im, "UI_IconSave")


# ---------------------------------------------------------------------------
# Degrades et formes douces (fondus, halos, coins arrondis en 9 tranches)
# ---------------------------------------------------------------------------
def gradients():
    t = np.linspace(0, 1, 256)
    s = 1 - (t * t * (3 - 2 * t))
    a = (s * 255).astype(np.uint8)
    h = np.tile(a[None, :], (4, 1))
    save(Image.merge("RGBA", (Image.new("L", (256, 4), 255),) * 3 + (Image.fromarray(h),)), "UI_GradH")
    save(Image.merge("RGBA", (Image.new("L", (4, 256), 255),) * 3 + (Image.fromarray(h.T.copy()),)), "UI_GradV")
    y, x = np.mgrid[0:256, 0:256] / 255.0 * 2 - 1
    r = np.clip(np.sqrt(x * x + y * y), 0, 1)
    g = (1 - r) ** 2.2
    save(Image.merge("RGBA", (Image.new("L", (256, 256), 255),) * 3 + (Image.fromarray((g * 255).astype(np.uint8)),)), "UI_Radial")
    # Rectangle arrondi 64 px (rayon 16) pour le dessin en 9 tranches
    im = Image.new("L", (64 * SS, 64 * SS), 0)
    ImageDraw.Draw(im).rounded_rectangle((0, 0, 64 * SS - 1, 64 * SS - 1), radius=16 * SS, fill=255)
    a = im.resize((64, 64), Image.LANCZOS)
    save(Image.merge("RGBA", (Image.new("L", (64, 64), 255),) * 3 + (a,)), "UI_Round")
    # Contour seul (trait de 2 px) : liseres des cartes et des boutons
    im = Image.new("L", (64 * SS, 64 * SS), 0)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, 64 * SS - 1, 64 * SS - 1), radius=16 * SS, fill=255)
    d.rounded_rectangle((2 * SS, 2 * SS, 62 * SS - 1, 62 * SS - 1), radius=14 * SS, fill=0)
    a = im.resize((64, 64), Image.LANCZOS)
    save(Image.merge("RGBA", (Image.new("L", (64, 64), 255),) * 3 + (a,)), "UI_RoundLine")


def icon_language():
    """v4.8 : globe (entree Langue du menu)"""
    im, d = canvas()
    w = 9 * SS
    d.ellipse(P(16, 16, 112, 112), outline=255, width=w)
    d.ellipse(P(42, 16, 86, 112), outline=255, width=w)
    d.line(P(64, 18, 64, 110), fill=255, width=w)
    d.line(P(18, 64, 110, 64), fill=255, width=w)
    for y in (38, 90):
        half = math.sqrt(48 ** 2 - (y - 64) ** 2) - 3
        d.line(P(64 - half, y, 64 + half, y), fill=255, width=w)
    finish(im, "UI_IconLanguage")


if __name__ == "__main__":
    print("Interface ->", os.path.abspath(OUT), "(police :", os.path.basename(INTER) + ")")
    logo()
    for fn in (icon_solo, icon_multi, icon_settings, icon_quit, icon_host, icon_join, icon_back, icon_play, icon_tip, icon_arrow, icon_keys,
               icon_lock, icon_trash, icon_plus, icon_check, icon_save, icon_language):
        fn()
    gradients()
