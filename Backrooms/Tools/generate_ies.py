"""v4.15 (realisme) : profils photometriques IES (LM-63-2002) de la lampe torche et de la frontale.

Le faisceau d'une vraie lampe a reflecteur n'est pas un cone uniforme : un plateau eclaire par le reflecteur (avec un leger
point chaud au centre et un anneau un peu plus clair sur son bord), puis un halo qui s'eteint vers 30 degres. La frontale
eclaire plus large et plus doux. Les profils sont symetriques autour de l'axe (un seul plan horizontal), en photometrie
absolue (lumens -1).

Unreal normalise le profil (son maximum vaut 1) et applique l'intensite de la lumiere (ABRCharacter : 650 cd pour la lampe)
au maximum seulement : partout ailleurs, la lumiere vaut l'intensite x le profil x l'attenuation du cone (interieur 13,
exterieur 30 degres pour la lampe tenue, 15 / 34 a la ceinture, 22 / 45 pour la frontale). Le profil retire donc de la
lumiere au cone d'avant ; il est dessine pour en garder l'essentiel, et --check le verifie :
  - lampe : au moins 0,8 du maximum jusqu'a 12,5 degres et 0,5 a 14 degres (le Smiler est "dans le faisceau" a moins de
    14 degres : ABREntity, IsLookedAtBy(P, 0.97)) ; flux d'au moins 70 % de celui du cone seul (60 % a la ceinture, cone
    plus large) ;
  - frontale : au moins 0,8 du maximum jusqu'a 22 degres (son cone interieur), flux d'au moins 70 % du cone seul.
(v4.15, premiere version : point chaud de 5 degres, 31 % du flux du cone seul, 27 % a la ceinture, 34 % pour la frontale :
faisceau trop etroit et trop sombre dans le noir d'une coupure.)

Fichiers : RawAssets/Lights/IES_Flashlight.ies et IES_Headlamp.ies, importes par Content/Python/backrooms_setup.py
(import_light_profiles) dans /Game/Backrooms/Lights (UTextureLightProfile). Sans eux, la lampe garde son cone simple.

    python Tools/generate_ies.py            # ecrit les deux profils
    python Tools/generate_ies.py --check    # verifie que les fichiers livres correspondent au script, et les seuils
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
    """Lampe a reflecteur : plateau jusqu'a ~14 degres (leger point chaud au centre, anneau du reflecteur a 11), halo
    jusqu'a ~30 degres"""
    core = math.exp(-((theta / 21.0) ** 5))
    hot = 0.05 * math.exp(-(theta / 4.0) ** 2)
    ring = 0.04 * math.exp(-((theta - 11.0) / 1.5) ** 2)
    spill = 0.32 * math.exp(-((theta / 28.0) ** 3))
    # Petites irregularites du reflecteur (anneaux concentriques faibles, fixes)
    ripple = 1.0 + 0.02 * math.sin(theta * 1.9) * math.exp(-theta / 25.0)
    v = (0.63 * core + hot + ring + spill) * ripple
    if theta > 34.0:
        v *= max(0.0, 1.0 - (theta - 34.0) / 6.0)
    return max(0.0, v)


def headlamp(theta):
    """Frontale : faisceau large et doux (plateau jusqu'a ~22 degres, halo jusqu'a ~48 degres)"""
    core = math.exp(-((theta / 31.0) ** 5))
    spill = 0.3 * math.exp(-((theta / 42.0) ** 3))
    v = 0.7 * core + spill
    if theta > 48.0:
        v *= max(0.0, 1.0 - (theta - 48.0) / 8.0)
    return max(0.0, v)


PROFILES = (
    ("IES_Flashlight", flashlight, "Lampe torche a reflecteur (plateau, anneau, halo)", "BR-FLASH-415"),
    ("IES_Headlamp", headlamp, "Lampe frontale (faisceau large)", "BR-HEAD-415"),
)

# Seuils verifies par --check (voir l'en-tete) : (profil, angle, part du maximum au moins) et (profil, cone interieur,
# cone exterieur, part du flux du cone seul au moins, usage). Cones : ABRCharacter::ApplyLamp
MIN_LEVELS = (
    ("IES_Flashlight", 12.5, 0.8),
    ("IES_Flashlight", 14.0, 0.5),
    ("IES_Headlamp", 22.0, 0.8),
)
MIN_FLUX = (
    ("IES_Flashlight", 13.0, 30.0, 0.7, "lampe tenue"),
    ("IES_Flashlight", 15.0, 34.0, 0.6, "lampe a la ceinture"),
    ("IES_Headlamp", 22.0, 45.0, 0.7, "frontale"),
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


def relative(fn, theta):
    """Part du maximum a l'angle theta (degres)"""
    top = max(fn(a * 0.05) for a in range(0, 1800))
    return fn(theta) / top


def cone(theta, inner, outer):
    """Attenuation du cone d'une lumiere spot d'Unreal : 1 dans le cone interieur, puis carre de la pente en cosinus"""
    c, ci, co = (math.cos(math.radians(a)) for a in (theta, inner, outer))
    return min(1.0, max(0.0, (c - co) / (ci - co))) ** 2


def flux_ratio(fn, inner, outer):
    """Flux de la lumiere avec le profil, rapporte a celui du cone seul (integrale sur l'angle solide)"""
    top = max(fn(a * 0.05) for a in range(0, 1800))
    with_profile = alone = 0.0
    for i in range(0, 1800):
        t = i * 0.05 + 0.025
        w = math.sin(math.radians(t)) * cone(t, inner, outer)
        with_profile += fn(t) / top * w
        alone += w
    return with_profile / alone


def check_thresholds():
    """Seuils de lumiere (MIN_LEVELS, MIN_FLUX) ; renvoie le nombre d'echecs"""
    fns = {name: fn for name, fn, _, _ in PROFILES}
    bad = 0
    for name, theta, low in MIN_LEVELS:
        v = relative(fns[name], theta)
        ok = v >= low
        bad += 0 if ok else 1
        print("  %-15s a %4.1f deg : %.2f du maximum (au moins %.2f) : %s" % (name, theta, v, low, "OK" if ok else "ECHEC"))
    for name, inner, outer, low, use in MIN_FLUX:
        v = flux_ratio(fns[name], inner, outer)
        ok = v >= low
        bad += 0 if ok else 1
        print("  %-15s %s (cone %g / %g deg) : %.0f %% du flux du cone seul (au moins %.0f %%) : %s" % (name, use, inner, outer, v * 100.0,
                                                                                                        low * 100.0, "OK" if ok else "ECHEC"))
    return bad


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
    bad += check_thresholds()
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
