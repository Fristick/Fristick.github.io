"""
v4.17 (animation) : critères mesurables des vidéos du banc -BRAutoTestV417Visual, lus dans les fiches des images.

Les fiches des vidéos `l0_bacteria` et `l0_3p_cote` contiennent, à chaque image (pas fixe de 1/30 s), un objet "mesures"
écrit par le banc : appuis de la Bacteria (ABREntity::GetSupportPoints), sol de référence, lacet et écart du modèle affiché,
allure, bond ; pose du corps du joueur (BRRig::GetBodyPoseInfo : pieds au monde, sol sous chaque pied, écart des jambes).
Ces crochets n'existent qu'à partir de la v4.17 : une exécution « avant » n'a pas de mesures (seulement les images).

Critères (Docs/v417/agents/animation.md, R.1 et R.2) :
 Bacteria : appui posé (à moins de 2 cm du sol, hors bond et hors course) : <= 0,5 cm par image et <= 2 cm sur l'appui ;
            griffe : <= 1 cm par image ; lacet affiché : <= 15 deg par image en course, <= 25 deg par bond ;
            écart entre le modèle affiché et la capsule : <= 40 cm.
 Joueur   : pied d'appui : <= 3 cm de glissement par pas ; double appui : les deux pieds à <= 2 cm du sol.

Usage (depuis Backrooms/) :
    python Tools/Visual/v417_motion.py --src Saved/TestRuns/v417/apres7/Qualite
"""
import argparse
import glob
import json
import math
import os
import re


def frames(src, scene):
    out = []
    for p in sorted(glob.glob(os.path.join(src, "v417_video_%s_*.json" % scene))):
        m = re.search(r"_(\d{3})\.json$", p)
        if not m:
            continue
        with open(p, encoding="utf-8") as f:
            d = json.load(f)
        out.append((int(m.group(1)), d.get("mesures") or {}))
    out.sort()
    return out


def d2(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def yaw_delta(a, b):
    return abs((b - a + 180.0) % 360.0 - 180.0)


def bacteria(src):
    fr = [(i, m.get("entite")) for i, m in frames(src, "l0_bacteria")]
    fr = [(i, e) for i, e in fr if e]
    if not fr:
        print("l0_bacteria : aucune mesure dans les fiches (execution d'avant la v4.17, ou entite absente)")
        return
    n_pts = max(len(e["appuis"]) for _, e in fr)
    slip = [[] for _ in range(n_pts)]
    drift = [0.0] * n_pts
    start = [None] * n_pts
    run_yaw, walk_yaw, lag, hops = [], [], [], 0
    hop_yaw = 0.0
    hop_yaws = []
    prev = None
    dist = 0.0
    for i, e in fr:
        lag.append(e["ecart_affiche_cm"])
        if prev is not None:
            dist += d2((prev["x"], prev["y"]), (e["x"], e["y"]))
            dy = yaw_delta(prev["lacet_affiche"], e["lacet_affiche"])
            if e["bond"]:
                hop_yaw += dy
            elif e["course"] >= 0.5:
                run_yaw.append(dy)
            else:
                walk_yaw.append(dy)
            if prev["bond"] and not e["bond"]:
                hops += 1
                hop_yaws.append(hop_yaw)
                hop_yaw = 0.0
        planted_frame = not e["bond"] and e["course"] < 0.5
        for k, p in enumerate(e["appuis"]):
            planted = planted_frame and p[2] - e["sol_z"] < 2.0
            if planted and prev is not None and k < len(prev["appuis"]):
                q = prev["appuis"][k]
                if q[2] - prev["sol_z"] < 2.0 and not prev["bond"] and prev["course"] < 0.5:
                    slip[k].append(d2(p, q))
            if planted:
                if start[k] is None:
                    start[k] = p
                drift[k] = max(drift[k], d2(p, start[k]))
            else:
                start[k] = None
        prev = e
    print("l0_bacteria : %d images mesurees, %d appuis par image, trajet du corps %.0f cm, %d bond(s)" % (len(fr), n_pts, dist, hops))
    for k in range(n_pts):
        s = sorted(slip[k])
        if not s:
            print("  appui %d : jamais pose" % k)
            continue
        p95 = s[int(0.95 * (len(s) - 1))]
        print("  appui %d : %d images posees, glissement par image median %.2f cm, p95 %.2f, max %.2f (critere <= 0,5 ; griffe <= 1) ; derive sur un appui max %.2f cm (<= 2)"
              % (k, len(s), s[len(s) // 2], p95, s[-1], drift[k]))
    if run_yaw:
        print("  lacet affiche en course : max %.1f deg par image (critere <= 15), %d images" % (max(run_yaw), len(run_yaw)))
    if walk_yaw:
        print("  lacet affiche a l'allure a appuis, hors bond : max %.1f deg par image" % max(walk_yaw))
    if hop_yaws:
        print("  lacet par bond : max %.1f deg (critere <= 25)" % max(hop_yaws))
    print("  ecart modele / capsule : max %.1f cm (critere <= 40), median %.1f" % (max(lag), sorted(lag)[len(lag) // 2]))


def player(src):
    fr = [(i, m.get("corps")) for i, m in frames(src, "l0_3p_cote")]
    fr = [(i, c) for i, c in fr if c]
    if not fr:
        print("l0_3p_cote : aucune mesure dans les fiches")
        return
    floor = min(min(c["pied_gauche"][2], c["pied_droit"][2]) for _, c in fr)
    steps = {0: [], 1: []}
    start = {0: None, 1: None}
    drift = {0: 0.0, 1: 0.0}
    double, double_ok = 0, 0
    yaw_off = []
    for i, c in fr:
        feet = (c["pied_gauche"], c["pied_droit"])
        planted = [f[2] - floor < 2.0 for f in feet]
        if planted[0] and planted[1]:
            double += 1
            double_ok += 1 if max(f[2] - floor for f in feet) <= 2.0 else 0
        for k in (0, 1):
            if planted[k]:
                if start[k] is None:
                    start[k] = feet[k]
                    drift[k] = 0.0
                drift[k] = max(drift[k], d2(feet[k], start[k]))
            elif start[k] is not None:
                steps[k].append(drift[k])
                start[k] = None
        yaw_off.append(abs(c["ecart_jambes"]))
    for k in (0, 1):
        if start[k] is not None:
            steps[k].append(drift[k])
    names = ("gauche", "droit")
    print("l0_3p_cote : %d images mesurees" % len(fr))
    for k in (0, 1):
        s = sorted(steps[k])
        if s:
            print("  pied %s : %d appui(s), glissement par pas median %.2f cm, max %.2f (critere <= 3)" % (names[k], len(s), s[len(s) // 2], s[-1]))
    print("  double appui : %d image(s), les deux pieds a <= 2 cm du sol sur %d" % (double, double_ok))
    print("  ecart des jambes a la visee : max %.1f deg, median %.1f" % (max(yaw_off), sorted(yaw_off)[len(yaw_off) // 2]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    args = ap.parse_args()
    bacteria(args.src)
    player(args.src)


if __name__ == "__main__":
    main()
