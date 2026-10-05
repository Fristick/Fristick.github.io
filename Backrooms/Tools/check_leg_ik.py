"""v4.7 : verifie hors moteur la formule d'appui des pieds de ABREntity::UpdateFootPlanting.

Jambe dans son plan : la cuisse (longueur A) tourne de PlantPitch vers l'avant, le tibia (longueur B) plie le genou
de -PlantPitch par rapport a la cuisse (meme convention que la pose armee : cuisse +, tibia -).
Pour une elevation demandee, le pied doit monter d'autant et rester a l'aplomb de la hanche.
Ce n'est pas un test dans Unreal : la convention de signe des pivots reste a confirmer en jeu (voir le rapport v4.7).

    python3 Tools/check_leg_ik.py
"""
import math
import sys


def plant(a, b, lift):
    d0 = a + b - 2.0
    d1 = max(d0 - lift, abs(a - b) + 5.0)

    def hip(d):
        return math.acos(max(-1.0, min(1.0, (a * a + d * d - b * b) / (2 * a * d))))

    def knee(d):
        return math.pi - math.acos(max(-1.0, min(1.0, (a * a + b * b - d * d) / (2 * a * b))))

    return hip(d0), knee(d0), hip(d1) - hip(d0), knee(d1) - knee(d0)


def foot(a, b, phi, k):
    return a * math.sin(phi) + b * math.sin(phi - k), -(a * math.cos(phi) + b * math.cos(phi - k))


def main():
    worst = 0.0
    cases = 0
    for a in range(30, 75, 5):
        for b in range(30, 75, 5):
            for lift in (2, 5, 10, 15, 20, 25, 30, 35):
                p0, k0, dp, dk = plant(a, b, lift)
                x0, y0 = foot(a, b, p0, k0)
                x1, y1 = foot(a, b, p0 + dp, k0 + dk)
                expected = min(lift, (a + b - 2.0) - (abs(a - b) + 5.0))
                err = max(abs((y1 - y0) - expected), abs(x1 - x0))
                worst = max(worst, err)
                cases += 1
    print(f"{cases} cas (cuisse et tibia de 30 a 70 cm, elevation de 2 a 35 cm) : ecart maximal {worst:.4f} cm")
    return 0 if worst < 0.01 else 1


if __name__ == "__main__":
    sys.exit(main())
