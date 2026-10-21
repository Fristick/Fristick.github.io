"""v4.7 : verifie hors moteur la formule d'appui des pieds de ABREntity::UpdateFootPlanting.

Jambe dans son plan : la cuisse (longueur A) tourne de PlantPitch vers l'avant, le tibia (longueur B) plie le genou
de -PlantPitch par rapport a la cuisse (meme convention que la pose armee : cuisse +, tibia -).
Pour une elevation demandee, le pied doit monter d'autant et rester a l'aplomb de la hanche.
Ce n'est pas un test dans Unreal : la convention de signe des pivots reste a confirmer en jeu (voir le rapport v4.7).

v4.17 (appuis) : verifie aussi l'IK des jambes du corps du joueur (ABRCharacter::AnimateBody) : la cheville vise un
point quelconque du plan de la jambe (avant, haut) ; angles ajoutes a la pose de repos (cuisse, puis tibia par rapport a
la cuisse), genou plie vers l'avant. Pour toute cible a portee, la cheville doit tomber sur la cible.

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


def rot(v, deg):
    """Tangage de deg degres d'un vecteur (avant, haut) : son angle depuis la verticale descendante augmente de deg
    (convention de FRotator : tangage positif, membre tendu vers le bas porte vers l'avant)"""
    t = math.radians(deg)
    x, z = v
    # angle depuis le bas : alpha = atan2(x, -z) ; longueur conservee
    return x * math.cos(t) - z * math.sin(t), x * math.sin(t) + z * math.cos(t)


def body_ik(hip, knee, ankle, target):
    """Formule d'AnimateBody (plan de la jambe) : angles de cuisse et de tibia (degres) ajoutes au repos"""
    t0 = (knee[0] - hip[0], knee[1] - hip[1])
    s0 = (ankle[0] - knee[0], ankle[1] - knee[1])
    l0 = (ankle[0] - hip[0], ankle[1] - hip[1])
    a = max(math.hypot(*t0), 5.0)
    b = max(math.hypot(*s0), 5.0)
    d0 = math.hypot(*l0)
    dx, dz = target[0] - hip[0], target[1] - hip[1]
    d = min(max(math.hypot(dx, dz), abs(a - b) + 2.0), min(a + b - 0.05, d0))
    gamma = math.acos(max(-1.0, min(1.0, (a * a + b * b - d * d) / (2 * a * b))))
    beta = math.acos(max(-1.0, min(1.0, (a * a + d * d - b * b) / (2 * a * d))))
    alpha_t = math.atan2(dx, -dz) + beta
    alpha_s = alpha_t - (math.pi - gamma)
    alpha_t0 = math.atan2(t0[0], -t0[1])
    alpha_s0 = math.atan2(s0[0], -s0[1])
    return math.degrees(alpha_t - alpha_t0), math.degrees((alpha_s - alpha_s0) - (alpha_t - alpha_t0)), d0


def body_check():
    """Repos : hanche a 94 cm, genou a 55 cm (un peu en avant ou en arriere), cheville a 8 cm ; cibles a portee (bassin
    descendu de ce qu'il faut, comme dans le jeu) entre 60 cm derriere et 60 cm devant, de 0 a 15 cm au-dessus du sol"""
    worst, cases = 0.0, 0
    for knee_x in (-1.5, 0.0, 1.5):
        hip, knee, ankle = (0.0, 94.0), (knee_x, 55.0), (0.3, 8.0)
        for tx in range(-60, 61, 5):
            for lift in (0, 5, 10, 15):
                target = (ankle[0] + tx, ankle[1] + lift)
                tp, sp, d0 = body_ik(hip, knee, ankle, target)
                # cible hors de portee : le bassin descend (WalkDrop), ici la hanche
                need = (hip[1] - target[1]) - math.sqrt(max(d0 * d0 - (target[0] - hip[0]) ** 2, 1.0))
                h = (hip[0], hip[1] - max(need, 0.0))
                tp, sp, _ = body_ik(h, (knee[0], knee[1] - (hip[1] - h[1])), (ankle[0], ankle[1] - (hip[1] - h[1])), target)
                kv = rot((knee[0] - hip[0], knee[1] - hip[1]), tp)
                sv = rot((ankle[0] - knee[0], ankle[1] - knee[1]), tp + sp)
                foot_x, foot_z = h[0] + kv[0] + sv[0], h[1] + kv[1] + sv[1]
                worst = max(worst, math.hypot(foot_x - target[0], foot_z - target[1]))
                cases += 1
    print(f"corps du joueur : {cases} cas (genou au repos a -1,5 / 0 / +1,5 cm, cible de -60 a +60 cm, 0 a 15 cm de haut) : "
          f"ecart maximal {worst:.3f} cm")
    return worst < 0.1


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
    body_ok = body_check()
    return 0 if worst < 0.01 and body_ok else 1


if __name__ == "__main__":
    sys.exit(main())
