"""
v4.17 (visuel) : preuves visuelles du banc -BRAutoTestV417Visual pour le depot (Docs/v417/visuels/).

Usage (depuis Backrooms/, Python 3 avec Pillow et numpy) :
  python Tools/Visual/make_v417_media.py pairs --before <dossier avant> --after <dossier apres> --out <dossier> [--names a,b,...]
      planche avant / apres de chaque capture presente des deux cotes (JPEG, 2 x 960 px de large), titre : nom, commit
      et profil lus dans les fiches <capture>.json ; difference de luminance moyenne (L*, approchee) notee sous l'image
  python Tools/Visual/make_v417_media.py convert --src <dossier> --out <dossier> [--names a,b,...] [--width 1366]
      captures PNG en JPEG (qualite 88) avec leur fiche JSON a cote
  python Tools/Visual/make_v417_media.py video --src <dossier> --scene <nom> --out <fichier.webp> [--fps 30] [--width 960]
      images v417_video_<nom>_<nnn>.png en une animation WebP (boucle) ; la cadence est celle du pas fixe du banc (1/30 s)
  python Tools/Visual/make_v417_media.py stats --src <dossier> [--names a,b,...]
      luminance par capture (moyenne, percentiles 5 / 50 / 95, part des pixels brules et bouches), en texte
Aucune image n'est retouchee : les planches juxtaposent les captures telles que le jeu les a ecrites.
"""
import argparse
import glob
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont


def load_meta(png_path):
    meta_path = os.path.splitext(png_path)[0] + ".json"
    try:
        with open(meta_path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def luminance(img):
    """Luminance relative approchee (sRGB -> lineaire, Rec. 709), tableau 0..1"""
    a = np.asarray(img.convert("RGB"), dtype=np.float32) / 255.0
    lin = np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)
    return lin[..., 0] * 0.2126 + lin[..., 1] * 0.7152 + lin[..., 2] * 0.0722


def lstar(y):
    y = np.clip(y, 0.0, 1.0)
    return np.where(y > 216.0 / 24389.0, 116.0 * np.cbrt(y) - 16.0, y * 24389.0 / 27.0)


def stats(img):
    y = luminance(img)
    L = lstar(y)
    return {
        "L_moy": float(L.mean()),
        "L_p5": float(np.percentile(L, 5)),
        "L_p50": float(np.percentile(L, 50)),
        "L_p95": float(np.percentile(L, 95)),
        "brules_pct": float((y > 0.98).mean() * 100.0),
        "bouches_pct": float((y < 0.002).mean() * 100.0),
    }


def font(size):
    for name in ("segoeui.ttf", "arial.ttf", "DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def names_in(folder, names):
    if names:
        return [n for n in names.split(",") if n]
    out = []
    for p in sorted(glob.glob(os.path.join(folder, "v417_*.png"))):
        n = os.path.splitext(os.path.basename(p))[0]
        if "_video_" not in n:
            out.append(n)
    return out


def label(meta, side):
    if not meta:
        return side
    return "%s  %s  %s  %s" % (side, str(meta.get("commit", "?"))[:7], meta.get("profil", "?"),
                                "x".join(str(v) for v in meta.get("rendu_interne", [])))


def cmd_pairs(a):
    os.makedirs(a.out, exist_ok=True)
    f_title, f_small = font(22), font(16)
    done = 0
    for name in names_in(a.before, a.names):
        pb, pa = os.path.join(a.before, name + ".png"), os.path.join(a.after, name + ".png")
        if not (os.path.exists(pb) and os.path.exists(pa)):
            print("absente d'un cote :", name)
            continue
        ib, ia = Image.open(pb).convert("RGB"), Image.open(pa).convert("RGB")
        w = 960
        hb = round(ib.height * w / ib.width)
        ha = round(ia.height * w / ia.width)
        h = max(hb, ha)
        sheet = Image.new("RGB", (w * 2 + 12, h + 86), (18, 18, 18))
        sheet.paste(ib.resize((w, hb), Image.LANCZOS), (0, 44))
        sheet.paste(ia.resize((w, ha), Image.LANCZOS), (w + 12, 44))
        d = ImageDraw.Draw(sheet)
        sb, sa = stats(ib), stats(ia)
        d.text((8, 8), name, fill=(240, 240, 240), font=f_title)
        d.text((8, h + 50), "%s   L* moyen %.1f  p5 %.1f  p95 %.1f  brules %.2f %%" % (label(load_meta(pb), "AVANT"), sb["L_moy"], sb["L_p5"], sb["L_p95"], sb["brules_pct"]),
               fill=(200, 200, 200), font=f_small)
        d.text((w + 20, h + 50), "%s   L* moyen %.1f  p5 %.1f  p95 %.1f  brules %.2f %%" % (label(load_meta(pa), "APRES"), sa["L_moy"], sa["L_p5"], sa["L_p95"], sa["brules_pct"]),
               fill=(200, 200, 200), font=f_small)
        sheet.save(os.path.join(a.out, name + "_avant_apres.jpg"), quality=86)
        done += 1
    print("%d planche(s) dans %s" % (done, a.out))


def cmd_convert(a):
    os.makedirs(a.out, exist_ok=True)
    done = 0
    for name in names_in(a.src, a.names):
        p = os.path.join(a.src, name + ".png")
        if not os.path.exists(p):
            print("absente :", name)
            continue
        im = Image.open(p).convert("RGB")
        if im.width > a.width:
            im = im.resize((a.width, round(im.height * a.width / im.width)), Image.LANCZOS)
        im.save(os.path.join(a.out, name + ".jpg"), quality=88)
        meta = load_meta(p)
        if meta:
            with open(os.path.join(a.out, name + ".json"), "w", encoding="utf-8", newline="\n") as f:
                json.dump(meta, f, ensure_ascii=False, indent=2)
        done += 1
    print("%d capture(s) dans %s" % (done, a.out))


def cmd_video(a):
    frames = sorted(glob.glob(os.path.join(a.src, "v417_video_%s_[0-9][0-9][0-9].png" % a.scene)))
    if not frames:
        print("aucune image pour", a.scene)
        sys.exit(1)
    ims = []
    for p in frames:
        im = Image.open(p).convert("RGB")
        if im.width > a.width:
            im = im.resize((a.width, round(im.height * a.width / im.width)), Image.LANCZOS)
        ims.append(im)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    ims[0].save(a.out, save_all=True, append_images=ims[1:], duration=round(1000.0 / a.fps), loop=0, quality=72, method=4)
    print("%s : %d images a %d img/s (%.1f s), %.1f Mo" % (a.out, len(ims), a.fps, len(ims) / a.fps, os.path.getsize(a.out) / 1e6))


def cmd_stats(a):
    for name in names_in(a.src, a.names):
        p = os.path.join(a.src, name + ".png")
        if os.path.exists(p):
            s = stats(Image.open(p))
            print("%-40s L* %.1f  p5 %.1f  p50 %.1f  p95 %.1f  brules %.2f %%  bouches %.2f %%" % (
                name, s["L_moy"], s["L_p5"], s["L_p50"], s["L_p95"], s["brules_pct"], s["bouches_pct"]))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("pairs")
    p.add_argument("--before", required=True)
    p.add_argument("--after", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--names", default="")
    p = sub.add_parser("convert")
    p.add_argument("--src", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--names", default="")
    p.add_argument("--width", type=int, default=1366)
    p = sub.add_parser("video")
    p.add_argument("--src", required=True)
    p.add_argument("--scene", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--fps", type=int, default=30)
    p.add_argument("--width", type=int, default=960)
    p = sub.add_parser("stats")
    p.add_argument("--src", required=True)
    p.add_argument("--names", default="")
    a = ap.parse_args()
    {"pairs": cmd_pairs, "convert": cmd_convert, "video": cmd_video, "stats": cmd_stats}[a.cmd](a)


if __name__ == "__main__":
    main()
