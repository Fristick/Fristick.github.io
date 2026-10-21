"""
v4.17 (visuel) : critères mesurables de la passe graphique, sur deux exécutions du banc -BRAutoTestV417Visual.

Les zones et les seuils sont ceux de la QA visuelle (Docs/v417/agents/qa_visuel.md) et de la direction artistique
(direction_artistique.md § 2), pour une fenêtre de 1366 x 768. Luminance relative (sRGB -> linéaire, Rec. 709).

Usage (depuis Backrooms/, Python 3 avec Pillow et numpy) :
    python Tools/Visual/v417_criteria.py --root Saved/TestRuns/v417 --before avant6 --after apres6 [--profiles Qualite,Cinematique,Performance]
Écrit le tableau en Markdown sur la sortie standard. Les captures ne sont pas modifiées.

Mesures :
 - diffuseurs du Level 0 (l0_salle, zone 560-660 x 100-160 ; l0_dos, 600-820 x 100-135 ; pixels de luminance > 0,5) : part des
   pixels au canal le plus fort >= 250/255 et = 255 (cible de la direction artistique : moins de 2 %) ;
 - hiérarchie mur / moquette (médiane des murs / médiane de la moquette au premier plan ; cible <= 1,0) ;
 - tiers haut de l0_salle (plafond) et part de très sombre (< 0,01 ; cible 4 à 10 %) ;
 - Poolrooms (p_vue, image sans les 40 lignes du haut) : p1 (cible <= 0,01) et p99 (cible >= 0,75) ; p_dos : plafond / murs ;
 - coupure avec lampe (l0_coupure_lampe) : pixel maximal (une image noire avec la lampe allumée est un défaut) ;
 - fiches : tension et frayeur au moment de la capture (banc figé : calme, 0).
"""
import argparse
import json
import os
import sys

import numpy as np
from PIL import Image

SIZE = (1366, 768)
ZONES = {
    "v417_l0_salle": {"mur_gauche": (0, 120, 210, 600), "mur_fond": (620, 730, 345, 435), "pilier_milieu": (395, 475, 340, 470),
                      "moquette": (400, 1366, 620, 768), "diffuseur": (560, 660, 100, 160)},
    "v417_l0_dos": {"mur_face": (800, 1050, 330, 570), "mur_face_gauche": (400, 560, 250, 560), "moquette": (0, 1366, 650, 768),
                    "diffuseur": (600, 820, 100, 135)},
    "v417_p_dos": {"plafond": (300, 1000, 40, 150), "mur_gauche": (0, 210, 300, 470), "mur_droit": (1100, 1366, 280, 500)},
}


def load(root, run, prof, name):
    p = os.path.join(root, run, prof, name + ".png")
    if not os.path.isfile(p):
        return None, {}
    im = Image.open(p)
    if im.size != SIZE:
        raise SystemExit("ERREUR : %s : %d x %d au lieu de %d x %d (zones de mesure fausses ; relancer le banc a la bonne taille)"
                         % (p, im.size[0], im.size[1], SIZE[0], SIZE[1]))
    a = np.asarray(im.convert("RGB"), np.float32) / 255.0
    meta = {}
    try:
        with open(p[:-4] + ".json", encoding="utf-8") as f:
            meta = json.load(f)
    except (OSError, ValueError):
        pass
    return a, meta


def lum(a):
    l = np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)
    return 0.2126 * l[..., 0] + 0.7152 * l[..., 1] + 0.0722 * l[..., 2]


def z(arr, box):
    x0, x1, y0, y1 = box
    return arr[y0:y1, x0:x1]


def measure(root, run, prof):
    out = {}
    for name in ("v417_l0_salle", "v417_l0_dos"):
        a, meta = load(root, run, prof, name)
        if a is None:
            continue
        y = lum(a)
        zz = ZONES[name]
        d = (z(a, zz["diffuseur"]) * 255.0).max(axis=2)[z(y, zz["diffuseur"]) > 0.5]
        d = d if d.size else np.zeros(1)
        carpet = float(np.median(z(y, zz["moquette"])))
        walls = {k: float(np.median(z(y, b))) / max(carpet, 1e-6) for k, b in zz.items() if k.startswith(("mur", "pilier"))}
        body = y[40:]
        out[name] = {
            "diff_ge250": float((d >= 250).mean() * 100), "diff_255": float((d >= 254.5).mean() * 100),
            "mur_moquette": walls, "haut": float(y[40:int(768 * 0.35)].mean()), "sombre": float((body < 0.01).mean() * 100),
            "tension": meta.get("tension", "?"), "frayeur": meta.get("frayeur", None),
        }
    a, meta = load(root, run, prof, "v417_p_vue")
    if a is not None:
        y = lum(a)[40:]
        out["v417_p_vue"] = {"p1": float(np.percentile(y, 1)), "p99": float(np.percentile(y, 99)), "sombre": float((y < 0.01).mean() * 100)}
    a, meta = load(root, run, prof, "v417_p_dos")
    if a is not None:
        y = lum(a)
        zz = ZONES["v417_p_dos"]
        ceil = float(np.median(z(y, zz["plafond"])))
        wall = float(np.median(np.concatenate([z(y, zz["mur_gauche"]).ravel(), z(y, zz["mur_droit"]).ravel()])))
        out["v417_p_dos"] = {"plafond_murs": ceil / max(wall, 1e-6)}
    a, meta = load(root, run, prof, "v417_l0_coupure_lampe")
    if a is not None:
        out["v417_l0_coupure_lampe"] = {"max": float(a.max() * 255), "moyenne": float(lum(a).mean()), "tension": meta.get("tension", "?"),
                                        "frayeur": meta.get("frayeur", None), "lampe": meta.get("lampe", None)}
    return out


def f(v, n=2):
    return "—" if v is None else ("%.*f" % (n, v))


def main():
    # Sortie en UTF-8 meme redirigee sous Windows (cp1252 ne connait ni « ≈ » ni « ≤ »)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.join("Saved", "TestRuns", "v417"))
    ap.add_argument("--before", required=True)
    ap.add_argument("--after", required=True)
    ap.add_argument("--profiles", default="Qualite,Cinematique,Performance")
    args = ap.parse_args()
    print("| Profil | Mesure | Cible | %s | %s |" % (args.before, args.after))
    print("|---|---|---|---|---|")
    for prof in args.profiles.split(","):
        A = measure(args.root, args.before, prof)
        B = measure(args.root, args.after, prof)

        def row(label, target, get, n=2):
            try:
                va = get(A)
            except (KeyError, TypeError):
                va = None
            try:
                vb = get(B)
            except (KeyError, TypeError):
                vb = None
            print("| %s | %s | %s | %s | %s |" % (prof, label, target, f(va, n), f(vb, n)))

        for name, short in (("v417_l0_salle", "l0_salle"), ("v417_l0_dos", "l0_dos")):
            row("%s : diffuseur >= 250/255 (%%)" % short, "< 2", lambda R, n=name: R[n]["diff_ge250"], 1)
            row("%s : diffuseur à 255 (%%)" % short, "≈ 0", lambda R, n=name: R[n]["diff_255"], 1)
            for k in ZONES[name]:
                if k.startswith(("mur", "pilier")):
                    row("%s : %s / moquette" % (short, k), "≤ 1,0", lambda R, n=name, k=k: R[n]["mur_moquette"][k])
        row("l0_salle : tiers haut (luminance)", "—", lambda R: R["v417_l0_salle"]["haut"], 3)
        row("l0_salle : très sombre < 0,01 (%)", "4 à 10", lambda R: R["v417_l0_salle"]["sombre"], 1)
        row("p_vue : p1", "≤ 0,01", lambda R: R["v417_p_vue"]["p1"], 3)
        row("p_vue : p99", "≥ 0,75", lambda R: R["v417_p_vue"]["p99"], 3)
        row("p_dos : plafond / murs", "—", lambda R: R["v417_p_dos"]["plafond_murs"])
        row("l0_coupure_lampe : pixel maximal (/255)", "> 2 (lampe visible)", lambda R: R["v417_l0_coupure_lampe"]["max"], 0)
        for run, R in ((args.before, A), (args.after, B)):
            c = R.get("v417_l0_coupure_lampe", {})
            s = R.get("v417_l0_salle", {})
            print("| %s | fiche (%s) : tension l0_salle / coupure_lampe ; frayeur | calme ; 0 | %s / %s ; %s | |"
                  % (prof, run, s.get("tension", "?"), c.get("tension", "?"), f(c.get("frayeur"), 3)))


if __name__ == "__main__":
    main()
