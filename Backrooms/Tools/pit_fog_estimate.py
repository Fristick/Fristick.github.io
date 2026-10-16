"""v4.7 : estimation du voile de brouillard au fond d'une fosse du Niveau 0, sans puis avec M_BR_PitShade.

Hypotheses (a confirmer dans Unreal, ce n'est pas une mesure) :
- brouillard exponentiel en hauteur d'Unreal, densite du Niveau 0 lue dans BRLevels.cpp (FBRLevelDef::FogDensity ; 0,08 avant la
  v4.15, 0,03 depuis), soit densite / 1000 par cm, quasi constante sur 15 m (FogFalloff = 0,02 : la densite varie de moins de 3 % sur la profondeur du puits) ;
- opacite le long d'un rayon de longueur L : 1 - exp(-densite * L) ; couleur ajoutee : FogColor (0,32 ; 0,29 ; 0,16) ;
- fond de la fosse presque noir (albedo 0,02, tres peu eclaire) ;
- M_BR_PitShade multiplie l'image par lerp(1 ; 0,04 ; T * (2 - T)), T = sat((sol - 60 cm - z) / 600 cm).

    python3 Tools/pit_fog_estimate.py
"""
import math
import os
import re

LEVELS_CPP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Source", "Backrooms", "Private", "BRLevels.cpp")


def level0_fog_density():
    """v4.15 : densite du brouillard du Niveau 0 lue dans le code du jeu (0,08 si la ligne est introuvable)"""
    try:
        src = open(LEVELS_CPP, encoding="utf-8").read()
        body = src[src.index("FBRLevelDef Level0()"):]
        body = body[:body.index("return D;")]
        m = re.search(r"D\.FogDensity\s*=\s*([\d.]+)f?\s*;", body)
        return float(m.group(1)) if m else 0.08
    except (OSError, ValueError):
        return 0.08


FOG_DENSITY = level0_fog_density() / 1000.0  # par cm
FOG_COLOR = (0.32, 0.29, 0.16)
EYE_Z = 170.0
BOTTOM_LIGHT = 0.004  # radiance lineaire du fond, sans brouillard (estimation : fond sombre, eclaire de loin)


def srgb(x):
    x = max(0.0, x)
    return 12.92 * x if x <= 0.0031308 else 1.055 * x ** (1 / 2.4) - 0.055


def shade(z, start=60.0, rng=600.0, floor=0.04):
    t = min(1.0, max(0.0, (0.0 - start - z) / rng))
    e = t * (2.0 - t)
    return 1.0 + (floor - 1.0) * e


def main():
    print("Densite du brouillard du Niveau 0 : %.3f (BRLevels.cpp)" % (FOG_DENSITY * 1000.0))
    print("Point vise           distance   voile   fond sans PitShade (sRGB)   avec PitShade (sRGB)")
    for label, horiz, z in (("paroi a -1 m", 250.0, -100.0), ("paroi a -3 m", 300.0, -300.0), ("paroi a -6 m", 350.0, -600.0),
                            ("fond a -14 m", 400.0, -1400.0), ("fond, de loin", 900.0, -1400.0)):
        dist = math.hypot(horiz, EYE_Z - z)
        f = 1.0 - math.exp(-FOG_DENSITY * dist)
        lum_fog = 0.2126 * FOG_COLOR[0] + 0.7152 * FOG_COLOR[1] + 0.0722 * FOG_COLOR[2]
        before = BOTTOM_LIGHT * (1 - f) + lum_fog * f
        after = before * shade(z)
        print("%-20s %6.1f m  %5.1f %%   %.3f (%3.0f/255)              %.3f (%3.0f/255)" % (
            label, dist / 100.0, f * 100.0, before, srgb(before) * 255, after, srgb(after) * 255))
    print()
    print("Lecture : sans le post-traitement, le fond d'un puits de 14 m apparait gris-jaune (le brouillard y ajoute sa couleur) ;")
    print("avec, il redevient noir, alors que le haut des parois (moins de 1 m sous le sol) garde sa lumiere.")


if __name__ == "__main__":
    main()
