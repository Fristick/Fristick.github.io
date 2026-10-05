"""v4.7 : fenetres d'esquive des attaques en trois temps (modele cinematique simple, hors moteur).

Reprend les reglages de ABREntity::Info (preparation, fenetre d'impact, allonge, vitesse gardee) et les regles de
ABREntity::UpdateAttack : la preparation commence a AttackRange + 40 cm au plus ; le coup touche si, entre 0,05 s et
0,05 s + fenetre apres le debut de la frappe, la proie est a moins de AttackRange + 30 + allonge (et visible).
Le joueur reagit au cri et au geste au bout de REACTION secondes, puis court en ligne droite (470 cm/s en sprint,
260 en marche). Les accelerations et la latence reseau sont ignorees : c'est un ordre de grandeur pour regler le jeu,
pas une mesure dans Unreal.

    python3 Tools/attack_timing.py
"""
import sys

# Nom : (AttackRange, ChaseSpeed, Windup, Impact, Lunge, WindupMove, Damage)
ENTITIES = {
    "Smiler": (110, 700, 0.40, 0.15, 30, 0.15, 100),
    "Hound": (130, 465, 0.30, 0.18, 70, 0.0, 30),
    "Faceling": (110, 300, 0.50, 0.15, 15, 0.3, 15),
    "Skin-Stealer": (140, 440, 0.30, 0.16, 45, 0.4, 45),
    "Deathmoth": (130, 380, 0.45, 0.15, 30, 0.3, 12),
    "Wretch": (120, 240, 0.65, 0.18, 10, 0.2, 20),
    "Partygoer": (120, 520, 0.45, 0.15, 35, 0.2, 100),
    "Clump": (150, 300, 0.60, 0.20, 25, 0.1, 40),
    "Bacteria": (130, 440, 0.32, 0.15, 45, 0.1, 60),
}
SPRINT = 470.0
WALK = 260.0
REACTION = 0.25
DT = 0.005


def escapes(rng, chase, windup, impact, lunge, move, start, player_speed, reaction):
    """Vrai si la proie, partie a `start` cm au debut de la preparation, n'est jamais touchee"""
    lunge_scale = 1.6 if lunge >= 40 else 0.5
    reach = rng + 30 + lunge
    t = 0.0
    dist = float(start)
    end = windup + 0.05 + impact
    while t < end:
        player = player_speed if t >= reaction else 0.0
        if t < windup:
            entity = chase * move
        else:
            entity = chase * lunge_scale
        dist += (player - entity) * DT
        t += DT
        if windup + 0.05 <= t <= end and dist <= reach:
            return False
    return True


def main():
    print(f"Reaction {REACTION:.2f} s. Distance de depart : AttackRange + 40 (debut de preparation au plus loin) et AttackRange.")
    print("Colonnes : proie arretee qui se met a courir au signal (loin = a AttackRange + 40, pres = a AttackRange),")
    print("proie deja en fuite au sprint quand la preparation commence, et proie qui se cache derriere un obstacle.")
    print(f"{'Entite':<14}{'prep.':>7}{'sprint loin':>13}{'sprint pres':>13}{'marche loin':>13}{'deja en fuite':>15}{'cachette':>10}")
    rows = []
    for name, (rng, chase, windup, impact, lunge, move, dmg) in ENTITIES.items():
        far = rng + 40
        near = rng
        r = (
            escapes(rng, chase, windup, impact, lunge, move, far, SPRINT, REACTION),
            escapes(rng, chase, windup, impact, lunge, move, near, SPRINT, REACTION),
            escapes(rng, chase, windup, impact, lunge, move, far, WALK, REACTION),
            escapes(rng, chase, windup, impact, lunge, move, far, SPRINT, 0.0),
        )
        rows.append((name, r))
        print(f"{name:<14}{windup:>6.2f}s{('esquive' if r[0] else 'touche'):>13}{('esquive' if r[1] else 'touche'):>13}"
              f"{('esquive' if r[2] else 'touche'):>13}{('esquive' if r[3] else 'touche'):>15}{'esquive':>10}")
    # Exigence : chaque attaque doit pouvoir etre evitee au moins d'une facon par la course (sinon : seulement a couvert)
    only_cover = [n for n, r in rows if not any(r)]
    print()
    print("Esquive possible par la course : " + ", ".join(n for n, r in rows if any(r)))
    print("Seulement en se mettant a couvert (ligne de vue coupee pendant la preparation) : " + (", ".join(only_cover) or "aucune"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
