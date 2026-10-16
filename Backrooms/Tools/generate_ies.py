"""v4.15 (realisme) : profils photometriques IES (LM-63-2002) de la lampe torche et de la frontale.

Le faisceau d'une vraie lampe a reflecteur n'est pas un cone uniforme : un point chaud de quelques degres, un anneau un
peu plus clair la ou le reflecteur renvoie la lumiere (avec un creux juste avant), puis un halo qui s'eteint vers 30
degres. La frontale eclaire plus large et plus doux. Les profils sont symetriques autour de l'axe (un seul plan
horizontal), en photometrie absolue (lumens -1) : Unreal les normalise (le maximum vaut 1) et garde l'intensite de la
lumiere (ABRCharacter : 650 cd pour la lampe), le profil ne fait que dessiner le faisceau, dans le cone de la lumiere.

Fichiers : RawAssets/Lights/IES_Flashlight.ies et IES_Headlamp.ies, importes par Content/Python/backrooms_setup.py
(import_light_profiles) dans /Game/Backrooms/Lights (UTextureLightProfile). Sans eux, la lampe garde son cone simple.

    python Tools/generate_ies.py            # ecrit les deux profils
    python Tools/generate_ies.py --check    # verifie que les fichiers livres correspondent au script
"""
import argparse
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "RawAssets", "Lights")
PEAK_CD = 1000.0  # candela au centre (relatif : Unreal normalise le profil)


def flashlight(theta):
    """Lampe a reflecteur : point chaud (5 degres), creux a 7, anneau a 9,5, halo jusqu'a ~30 degres"""
    hot = math.exp(-(theta / 5.0) ** 2)
    dip = 0.06 * math.exp(-((theta - 7.0) / 1.0) ** 2)
    ring = 0.16 * math.exp(-((theta - 9.5) / 1.6) ** 2)
    spill = 0.45 * math.exp(-((theta / 19.0) ** 2.4))
    # Petites irregularites du reflecteur (anneaux concentriques faibles, fixes)
    ripple = 1.0 + 0.025 * math.sin(theta * 1.9) * math.exp(-theta / 25.0)
    v = (0.55 * hot - dip + ring + spill) * ripple
    if theta > 34.0:
        v *= max(0.0, 1.0 - (theta - 34.0) / 6.0)
    return max(0.0, v)


def headlamp(theta):
    """Frontale : faisceau large et doux (point chaud de 12 degres, halo jusqu'a ~48 degres)"""
    hot = math.exp(-(theta / 12.0) ** 2)
    spill = 0.45 * math.exp(-((theta / 30.0) ** 2.2))
    v = 0.6 * hot + spill
    if theta > 48.0:
        v *= max(0.0, 1.0 - (theta - 48.0) / 8.0)
    return max(0.0, v)


PROFILES = (
    ("IES_Flashlight", flashlight, "Lampe torche a reflecteur (point chaud, anneau, halo)", "BR-FLASH-415"),
    ("IES_Headlamp", headlamp, "Lampe frontale (faisceau large)", "BR-HEAD-415"),
)


def angles():
    """Angles verticaux (degres, croissants) : fins dans le faisceau, plus larges ensuite, jusqu'a 180"""
    out = [i * 0.5 for i in range(0, 121)]          # 0 a 60 par 0,5
    out += [float(a) for a in range(62, 92, 2)]      # 62 a 90 par 2
    out += [120.0, 150.0, 180.0]
    return out


def ies_text(name, fn, label, test):
    va = angles()
    raw = [fn(a) for a in va]
    top = max(raw)
    cd = [PEAK_CD * v / top for v in raw]
    lines = [
        "IESNA:LM-63-2002",
        "[TEST] %s (profil synthetique, Tools/generate_ies.py)" % test,
        "[TESTLAB] The Backrooms",
        "[ISSUEDATE] 2026-10-16",
        "[MANUFAC] The Backrooms (jeu)",
        "[LUMCAT] %s" % name,
        "[LUMINAIRE] %s" % label,
        "[LAMP] LED",
        "TILT=NONE",
        # lampes, lumens par lampe (-1 : photometrie absolue), multiplicateur, angles V, angles H, type C, metres, dimensions
        "1 -1 1.0 %d 1 1 2 0.03 0.03 0" % len(va),
        "1.0 1.0 3",
    ]

    def rows(values, fmt):
        return [" ".join(fmt % v for v in values[i:i + 10]) for i in range(0, len(values), 10)]

    lines += rows(va, "%.1f")
    lines += ["0"]
    lines += rows(cd, "%.2f")
    return "\n".join(lines) + "\n"


def beam(fn, frac):
    """Demi-angle (degres) ou l'intensite tombe sous frac du maximum"""
    top = max(fn(a * 0.1) for a in range(0, 900))
    for a in range(0, 900):
        if fn(a * 0.1) < frac * top:
            return a * 0.1
    return 90.0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true", help="verifie les fichiers livres sans rien ecrire")
    args = ap.parse_args()
    bad = 0
    for name, fn, label, test in PROFILES:
        path = os.path.join(OUT, name + ".ies")
        text = ies_text(name, fn, label, test)
        print("%-15s demi-angle a 50 %% : %4.1f deg, a 10 %% : %4.1f deg, a 1 %% : %4.1f deg" % (name, beam(fn, 0.5), beam(fn, 0.1), beam(fn, 0.01)))
        if args.check:
            try:
                with open(path, encoding="ascii", newline="") as f:
                    # Fins de ligne CRLF possibles apres une extraction git sous Windows (le chargeur d'Unreal lit les deux)
                    same = f.read().replace("\r\n", "\n") == text
            except OSError:
                same = False
            print("  %s : %s" % (os.path.relpath(path, ROOT), "a jour" if same else "DIFFERENT (relancer sans --check)"))
            bad += 0 if same else 1
        else:
            os.makedirs(OUT, exist_ok=True)
            with open(path, "w", encoding="ascii", newline="") as f:
                f.write(text)
            print("  ecrit : %s" % os.path.relpath(path, ROOT))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
