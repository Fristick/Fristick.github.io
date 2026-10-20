"""
v4.16 : banc hors moteur du classement d'un blocage de la marche des tests (BRTestV412::ClassifyBlock, BRAutoTestV412.cpp).

La marche des tests (-BRAutoTestV412, suite complete) est bloquee 8 controles sans progres : ClassifyBlock dit si une entite
barre seule le passage (ecartee par le test, ou NON VERIFIE) ou si le decor bloque (PROBLEME "NON ATTEINT a pied"). Defaut
(final416/full, commit 6d39fe8, Niveau 2, graine du niveau 554144564, vanne V1) : "bloque a la cellule (1, 5) ... devant (0 cm) :
BRChunk_1817 ... entite la plus proche : Smilers (BREntity_43) a 107 cm ; point de passage 1/13 (cellule 0, 5)", 913 images a
moins de 1,5 m d'une entite (le temps du blocage), alors que la meme marche (meme graine, meme commit) passe sans entite
(final416/v412 : 107.0 m, 0 saut).

Scene rejouee ici, en plan :
  - murs du labyrinthe du Niveau 2 autour de la cellule (1, 5) (ABRWorld::MazeOpen, EdgeE/EdgeN, murs de ABRChunk : un mur par
    suite d'aretes fermees, depasse de WallThickness/2 a chaque bout) ; constantes lues dans BRLevels.cpp et BRWorld.cpp ;
  - marcheur : capsule du joueur (BRCharacter.cpp) contre le decor (le rapport : "devant (0 cm)"), regard vers le centre de la
    cellule (0, 5) (StepWalk) ; place deduite de la capture v412_acces_2_bloque.png : les deux aretes du bout du mur qui separe
    les cellules (0, 4) et (1, 4) a 25,5 et 37,2 degres a droite de l'axe (x = 1020 et 1220 sur 1366, champ de 88 degres) ;
  - Smiler (BREntity.cpp : rayon, demi-hauteur, vol a HoverHeight) a 107 cm du centre du marcheur en 3D, donc a plat a la
    distance de contact des deux capsules ; hors du champ de la capture (aucun visage lumineux visible) ; il coince le marcheur
    (l'axe du marcheur entre les normales opposees du bout de mur et du Smiler : aucun glissement possible).
Pour chaque disposition coherente : la regle d'avant (deux segments droits, tout contact arrete) doit dire "decor" (le defaut
est reproduit) et la regle v4.16 (BRTestV412::WalkLikeCharacter : la capsule glisse le long d'un decor touche de biais, comme
le personnage, et vise a nouveau) "entite seule". Puis des scenes du decor (porte fermee en travers, de face ou de biais, cage,
mur a 140 cm devant une creature, comme -BRAutoTestV415Fix) : "decor" avec les deux regles (la regle n'est pas affaiblie) ;
une creature dans un couloir libre : "entite seule" ; une creature contre le marcheur mais derriere lui : jamais "entite seule".

Le banc ne remplace pas l'essai dans le jeu (-BRAutoTest -BRAutoTestStrict -BRSeed=4242, ou -BRAutoTestV415Fix) : les
entites y sont tirees au hasard, et les balayages du moteur (Chaos) ne sont pas rejoues ici (capsules vues en plan).

Usage : python Tools/verify_walk_block.py
Code de sortie : 0 si la regle d'avant ECHOUE sur toutes les dispositions coherentes de la scene, si la regle v4.16 les classe
toutes "entite seule", si les scenes du decor restent "decor", et si la regle v4.16 est dans les sources ; 1 sinon.
"""
import math
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PRIV = os.path.join(ROOT, "Source", "Backrooms", "Private")
PUB = os.path.join(ROOT, "Source", "Backrooms", "Public")
M32 = 0xFFFFFFFF


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def num(pattern, text, what):
    m = re.search(pattern, text, re.S)
    if not m:
        raise SystemExit("introuvable dans les sources : " + what)
    return float(m.group(1))


# --------------------------------------------------------------------------------------------- constantes lues dans le code
def read_constants():
    walk = read(os.path.join(PRIV, "BRAutoTestV412.cpp"))
    c = {}
    c["Look"] = num(r"constexpr float BlockLook = ([\d.]+)f;", walk, "BlockLook (BRAutoTestV412.cpp)")
    c["SlideMin"] = num(r"constexpr float BlockSlideMin = ([\d.]+)f;", walk, "BlockSlideMin")
    c["Moves"] = int(num(r"constexpr int32 BlockMoves = (\d+);", walk, "BlockMoves"))
    c["PullBack"] = num(r"constexpr float BlockPullBack = ([\d.]+)f;", walk, "BlockPullBack")
    body = walk[walk.index("FBlock ClassifyBlock("):]
    c["Lift"] = num(r"constexpr float Lift = ([\d.]+)f;", body, "Lift (ClassifyBlock)")
    reach = re.search(r"IsPitRoomCell\(Cell\.X, Cell\.Y\)\) \? ([\d.]+)f : ([\d.]+)f;", walk)
    if not reach:
        raise SystemExit("introuvable dans les sources : WaypointReach")
    c["Reach"] = float(reach.group(2))
    c["RuleInSources"] = "WalkLikeCharacter(World, W, From, Ways, Shape, Channel, NoEntities, R)" in walk and "Slide.Size() < BlockSlideMin" in walk
    mech = read(os.path.join(PUB, "BRMechLogic.h"))
    c["Margin"] = num(r"constexpr float Margin = ([\d.]+)f;", mech, "BRMech::Clearance::Margin")
    char = read(os.path.join(PRIV, "BRCharacter.cpp"))
    m = re.search(r"InitCapsuleSize\(([\d.]+)f, ([\d.]+)f\)", char)
    if not m:
        raise SystemExit("introuvable dans les sources : capsule du joueur (BRCharacter.cpp)")
    c["PlayerR"], c["PlayerHalf"] = float(m.group(1)), float(m.group(2))
    ent = read(os.path.join(PRIV, "BREntity.cpp"))
    c["SmilerHalf"] = num(r"Smiler\.HalfHeight = ([\d.]+)f;", ent, "Smiler.HalfHeight")
    c["SmilerR"] = num(r"Smiler\.HalfHeight = [\d.]+f; Smiler\.Radius = ([\d.]+)f;", ent, "Smiler.Radius")
    c["HoundR"] = num(r"Hound\.HalfHeight = [\d.]+f; Hound\.Radius = ([\d.]+)f;", ent, "Hound.Radius")
    levels = read(os.path.join(PRIV, "BRLevels.cpp"))
    l2 = levels[levels.index("D.Number = 2;"):]
    l2 = l2[:l2.index("return D;")]
    c["CellSize"] = num(r"D\.CellSize = ([\d.]+)f;", l2, "CellSize du Niveau 2")
    c["WallThickness"] = num(r"D\.WallThickness = ([\d.]+)f;", l2, "WallThickness du Niveau 2")
    c["LoopChance"] = num(r"D\.LoopChance = ([\d.]+)f;", l2, "LoopChance du Niveau 2")
    c["OpenZoneChance"] = num(r"D\.OpenZoneChance = ([\d.]+)f;", l2, "OpenZoneChance du Niveau 2")
    world = read(os.path.join(PRIV, "BRWorld.cpp"))
    maze = world[world.index("bool ABRWorld::MazeOpen("):]
    maze = maze[:maze.index("\n}")]
    for needle in ("Rand(X, Y, 201, Seed) < 0.5f", "bEast ? 202 : 203", "Rand(ZX, ZY, 204, Seed) < D.OpenZoneChance", "PosMod(X, 4) != 3"):
        if needle not in maze:
            raise SystemExit("ABRWorld::MazeOpen a change (" + needle + ") : le banc ne rejoue plus le labyrinthe du Niveau 2")
    return c


# --------------------------------------------------------------------------------------------- labyrinthe du Niveau 2
def mix(x):
    x &= M32
    x ^= x >> 16
    x = (x * 0x7FEB352D) & M32
    x ^= x >> 15
    x = (x * 0x846CA68B) & M32
    x ^= x >> 16
    return x


def brand(a, b, c, seed):
    h = mix(((a & M32) * 0x9E3779B1 & M32) ^ seed)
    h = mix(h ^ ((b & M32) * 0x85EBCA77 & M32))
    h = mix(h ^ ((c & M32) * 0xC2B2AE3D & M32))
    return (h & 0xFFFFFF) / 16777216.0


class Maze:
    """ABRWorld::MazeOpen, EdgeE/EdgeN (zone de depart ouverte) ; murs de ABRChunk (suites d'aretes fermees)"""

    def __init__(self, seed, c):
        self.seed, self.c = seed, c

    def open_edge(self, x, y, east):
        if abs(x) <= 1 and abs(y) <= 1 and abs(x + (1 if east else 0)) <= 1 and abs(y + (0 if east else 1)) <= 1:
            return True
        if (brand(x, y, 201, self.seed) < 0.5) == east:
            return True
        if brand(x, y, 202 if east else 203, self.seed) < self.c["LoopChance"]:
            return True
        if brand(x // 4, y // 4, 204, self.seed) < self.c["OpenZoneChance"]:
            return (x % 4 != 3) if east else (y % 4 != 3)
        return False

    def walls(self, x0, x1, y0, y1):
        s, t = self.c["CellSize"], self.c["WallThickness"]
        out = []
        for x in range(x0 - 1, x1 + 1):
            run = None
            for y in range(y0 - 1, y1 + 2):
                closed = y <= y1 and not self.open_edge(x, y, True)
                if closed and run is None:
                    run = y
                if not closed and run is not None:
                    fx = (x + 1) * s
                    out.append((fx - t / 2, fx + t / 2, run * s - t / 2, y * s + t / 2))
                    run = None
        for y in range(y0 - 1, y1 + 1):
            run = None
            for x in range(x0 - 1, x1 + 2):
                closed = x <= x1 and not self.open_edge(x, y, False)
                if closed and run is None:
                    run = x
                if not closed and run is not None:
                    fy = (y + 1) * s
                    out.append((run * s - t / 2, x * s + t / 2, fy - t / 2, fy + t / 2))
                    run = None
        return out


# --------------------------------------------------------------------------------------------- balayages en plan
def unit(v):
    n = math.hypot(v[0], v[1])
    return (v[0] / n, v[1] / n) if n > 1e-9 else (0.0, 0.0)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1]


def step(a, d, s):
    return (a[0] + d[0] * s, a[1] + d[1] * s)


def closest_on_box(p, b):
    return (min(max(p[0], b[0]), b[1]), min(max(p[1], b[2]), b[3]))


def hit_circle(p, d, centre, rad):
    """Disque de rayon rad (somme des rayons) touche par le centre qui part de p selon d : (t, normale, depart dedans, profondeur)"""
    m = (p[0] - centre[0], p[1] - centre[1])
    dist = math.hypot(*m)
    if dist < rad - 1e-6:
        return 0.0, unit(m), True, rad - dist
    b = dot(m, d)
    cc = dot(m, m) - rad * rad
    if b > 0 and cc > 0:
        return None
    disc = b * b - cc
    if disc < 0:
        return None
    t = max(0.0, -b - math.sqrt(disc))
    q = step(p, d, t)
    return t, unit((q[0] - centre[0], q[1] - centre[1])), False, 0.0


def hit_box(p, d, box, r):
    q = closest_on_box(p, box)
    dist = math.hypot(p[0] - q[0], p[1] - q[1])
    if dist < r - 1e-6:
        n = unit((p[0] - q[0], p[1] - q[1]))
        return 0.0, (n if n != (0.0, 0.0) else (0.0, 1.0)), True, r - dist
    best = None
    x0, x1, y0, y1 = box
    for axis, val, lo, hi, n in ((0, x0 - r, y0, y1, (-1, 0)), (0, x1 + r, y0, y1, (1, 0)), (1, y0 - r, x0, x1, (0, -1)), (1, y1 + r, x0, x1, (0, 1))):
        if abs(d[axis]) < 1e-12 or dot(d, n) >= 0:
            continue
        t = (val - p[axis]) / d[axis]
        if t < -1e-9:
            continue
        o = p[1 - axis] + d[1 - axis] * t
        if lo <= o <= hi and (best is None or t < best[0]):
            best = (max(t, 0.0), n, False, 0.0)
    for corner in ((x0, y0), (x0, y1), (x1, y0), (x1, y1)):
        h = hit_circle(p, d, corner, r)
        if h and (best is None or h[0] < best[0]):
            best = h
    return best


def sweep(p, d, length, r, walls, ents, ignore=()):
    """Premier impact du disque de rayon r sur length : (t, normale, depart dedans, profondeur, ("mur"|"entite", indice))"""
    best = None
    for i, box in enumerate(walls):
        h = hit_box(p, d, box, r)
        if h and h[0] <= length and (best is None or h[0] < best[0]):
            best = h + (("mur", i),)
    for i, (centre, rad) in enumerate(ents):
        if i in ignore:
            continue
        h = hit_circle(p, d, centre, r + rad)
        if h and h[0] <= length and (best is None or h[0] < best[0]):
            best = h + (("entite", i),)
    return best


# --------------------------------------------------------------------------------------------- les deux regles
def rule_before(pos, ways, r, walls, ents, c):
    """v4.15 (correctifs 4) : deux segments droits (Pos -> Target -> After, Look cm) ; entite si le premier objet touche est une
    entite et si le meme balayage sans entite est libre"""
    legs, left = [pos], c["Look"]
    for w in ways:
        f = legs[-1]
        dd = math.hypot(w[0] - f[0], w[1] - f[1])
        if dd < 1 or left < 1:
            continue
        s = min(dd, left)
        legs.append(step(f, unit((w[0] - f[0], w[1] - f[1])), s))
        left -= s

    def first(ents_):
        for a, b in zip(legs, legs[1:]):
            h = sweep(a, unit((b[0] - a[0], b[1] - a[1])), math.hypot(b[0] - a[0], b[1] - a[1]), r, walls, ents_)
            if h:
                return h
        return None
    h = first(ents)
    if not h:
        return "rien"
    return "entite seule" if h[4][0] == "entite" and first([]) is None else "decor"


def walk_like_character(pos, ways, r, walls, ents, c):
    """Miroir de BRTestV412::WalkLikeCharacter (BRAutoTestV412.cpp)"""
    out = {"free": False, "first": None, "entity": None}
    ignore = set()

    def remaining(a, leg):
        s = math.hypot(a[0] - ways[leg][0], a[1] - ways[leg][1])
        for i in range(leg, len(ways) - 1):
            s += math.hypot(ways[i][0] - ways[i + 1][0], ways[i][1] - ways[i + 1][1])
        return s
    base = remaining(pos, 0)
    goal = min(c["Look"], base)
    at, leg, pull = pos, 0, c["PullBack"]
    for _ in range(c["Moves"]):
        while leg + 1 < len(ways) and math.hypot(at[0] - ways[leg][0], at[1] - ways[leg][1]) < c["Reach"]:
            leg += 1
        left = goal - (base - remaining(at, leg))
        to_way = math.hypot(at[0] - ways[leg][0], at[1] - ways[leg][1])
        if left <= 1 or to_way <= 1:
            out["free"] = True
            return out
        d = unit((ways[leg][0] - at[0], ways[leg][1] - at[1]))
        length = min(to_way, left)
        h = sweep(at, d, length, r, walls, ents, ignore)
        if not h:
            at = step(at, d, length)
            continue
        t, n, inside, depth, who = h
        if inside and who[0] == "entite" and dot(d, n) >= 0:
            ignore.add(who[1])
            continue
        out["first"] = out["first"] or who
        if who[0] == "entite":
            out["entity"] = who
            return out
        if inside:
            at = step(at, n, depth + pull)
            continue
        reached = max(0.0, t - pull)
        at = step(at, d, reached)
        slide = (d[0] - n[0] * dot(d, n), d[1] - n[1] * dot(d, n))
        if math.hypot(*slide) < c["SlideMin"]:
            return out
        sd, slen = unit(slide), max(length - reached, 1.0) * math.hypot(*slide)
        start = step(at, n, pull)
        h2 = sweep(start, sd, slen, r, walls, ents, ignore)
        if not h2:
            at = step(start, sd, slen)
            continue
        t2, n2, inside2, _, who2 = h2
        if who2[0] == "entite" and inside2 and dot(sd, n2) >= 0:
            ignore.add(who2[1])
            continue
        if who2[0] == "entite":
            out["entity"] = who2
            return out
        if not inside2:
            at = step(start, sd, max(0.0, t2 - pull))
    return out


def rule_v416(pos, ways, r, walls, ents, c):
    w = walk_like_character(pos, ways, r, walls, ents, c)
    if w["first"] is None:
        return "rien"
    if w["entity"] is not None and walk_like_character(pos, ways, r, walls, [], c)["free"]:
        return "entite seule"
    return "rien" if w["free"] else "decor"


# --------------------------------------------------------------------------------------------- scene de final416/full
def capsule_contact(r1, half1, z1, r2, half2, z2):
    """Distance a plat ou deux capsules verticales se touchent (centres z1, z2 ; demi-hauteurs avec les hemispheres)"""
    lo1, hi1 = z1 - (half1 - r1), z1 + (half1 - r1)
    lo2, hi2 = z2 - (half2 - r2), z2 + (half2 - r2)
    gap = max(0.0, lo2 - hi1, lo1 - hi2)
    reach = r1 + r2
    return math.sqrt(reach * reach - gap * gap) if gap < reach else 0.0


def scene_final416(c):
    s = c["CellSize"]
    maze = Maze(554144564, c)
    walls = maze.walls(-3, 4, 2, 8)
    target = (0.5 * s, 5.5 * s)                              # point de passage 1/13 : centre de la cellule (0, 5)
    nexts = [(-0.5 * s, 5.5 * s), (0.5 * s, 4.5 * s)]       # point d'apres (inconnu) : cellule (-1, 5) ou (0, 4)
    stub = [b for b in walls if b[0] < s < b[1] and b[3] == 5 * s + c["WallThickness"] / 2]
    if len(stub) != 1:
        raise SystemExit("scene : bout de mur entre les cellules (0, 4) et (1, 4) introuvable")
    stub = stub[0]
    nw, ne = (stub[0], stub[3]), (stub[1], stub[3])
    pr, ph = c["PlayerR"], c["PlayerHalf"]
    rr, rh = pr - c["Margin"], ph - c["Lift"]                  # capsule du classement
    pz, rz = ph, ph + c["Lift"]                                # centres au-dessus du sol (sol a 0)
    sr, sh = c["SmilerR"], c["SmilerHalf"]
    tally, total = {}, 0

    def deg(a, b):
        return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))

    def rel(a):
        return (a + 180.0) % 360.0 - 180.0

    def wall_gap(p):
        return min(math.hypot(p[0] - closest_on_box(p, b)[0], p[1] - closest_on_box(p, b)[1]) for b in walls)
    for ix in range(int(1.0 * s) * 2, int(2.0 * s) * 2):
        for iy in range(int(5.0 * s) * 2, int(6.0 * s) * 2):
            p = (ix / 2.0, iy / 2.0)
            g = wall_gap(p)
            if not pr - 0.5 <= g <= pr + 1.5:
                continue
            axis = deg(p, target)
            if abs(rel(deg(p, nw) - axis) - 25.5) > 3 or abs(rel(deg(p, ne) - axis) - 37.2) > 3:
                continue
            d = unit((target[0] - p[0], target[1] - p[1]))
            q = closest_on_box(p, stub)
            n1 = unit((p[0] - q[0], p[1] - q[1]))
            for h10 in range(700, 860, 5):
                h = h10 / 10.0
                if h >= 107.0:
                    continue
                sz = pz + math.sqrt(107.0 * 107.0 - h * h)
                if abs(h - capsule_contact(pr, ph, pz, sr, sh, sz)) > 1.0:
                    continue                                   # les deux capsules se touchent
                reduced = capsule_contact(rr, rh, rz, sr, sh, sz)
                for a10 in range(0, 3600, 25):
                    a = math.radians(a10 / 10.0)
                    sm = (p[0] + h * math.cos(a), p[1] + h * math.sin(a))
                    if wall_gap(sm) < sr or abs(rel(a10 / 10.0 - axis)) < 44.0:
                        continue
                    n2 = unit((p[0] - sm[0], p[1] - sm[1]))
                    det = n1[0] * n2[1] - n1[1] * n2[0]
                    if dot(d, n1) >= 0 or dot(d, n2) >= 0 or abs(det) < 1e-9:
                        continue
                    if (-d[0] * n2[1] + d[1] * n2[0]) / det < 0 or (-n1[0] * d[1] + n1[1] * d[0]) / det < 0:
                        continue                               # le marcheur ne glisse ni d'un cote ni de l'autre : coince
                    for after in nexts:
                        ents = [(sm, reduced - rr)]
                        key = (rule_before(p, [target, after], rr, walls, ents, c), rule_v416(p, [target, after], rr, walls, ents, c))
                        tally[key] = tally.get(key, 0) + 1
                        total += 1
    return total, tally


def decor_scenes(c):
    """Couloir de 170 cm le long de X (mur de 90 cm du Niveau 2) ; (nom, marcheur, points, murs, entites, attendu)"""
    r = c["PlayerR"] - c["Margin"]
    hr = c["HoundR"]
    side = [(-400, 900, 85, 130), (-400, 900, -130, -85)]
    door = (100, 110, -85, 85)
    cage = (100, 160, -60, 60)
    wall140 = (140, 230, -85, 85)
    far = [(300, 0), (560, 0)]
    scenes = [
        ("porte fermee en travers, marcheur dans l'axe, creature a cote", (60, 0), far, side + [door], [((40, 70), hr)], "decor"),
        ("porte fermee, marcheur 30 cm de cote, creature derriere", (60, 30), far, side + [door], [((-20, 30), hr)], "decor"),
        ("porte fermee, marcheur contre le mur du couloir, Smiler colle cote couloir", (60, 51), far, side + [door], [((60, -25), c["SmilerR"])], "decor"),
        ("cage au milieu du couloir (25 cm de chaque cote), creature derriere", (40, 0), far, side + [cage], [((-40, 0), hr)], "decor"),
        ("creature devant un mur a 140 cm (-BRAutoTestV415Fix)", (0, 0), [(245, 0), (345, 0)], side + [wall140], [((90, 0), hr)], "decor"),
        ("creature devant le marcheur, couloir libre (-BRAutoTestV415Fix)", (0, 0), [(245, 0), (345, 0)], side, [((90, 0), hr)], "entite seule"),
        ("creature contre le marcheur, derriere lui, couloir libre", (0, 0), far, side, [((-(r + hr) + 0.5, 0), hr)], "rien"),
        ("mur du couloir longe, cible de biais, aucune creature", (0, 51), far, side, [], "rien"),
    ]
    rows = []
    for name, pos, ways, walls, ents, want in scenes:
        rows.append((name, rule_before(pos, ways, r, walls, ents, c), rule_v416(pos, ways, r, walls, ents, c), want))
    return rows


def main():
    c = read_constants()
    ok = True
    print("Constantes lues : regard %.0f cm, glissement minimal %.2f, %d pas, recul %.1f cm, capsule %.0f/%.0f cm (classement : rayon - %.0f, "
          "relevee de %.0f), point de passage a %.0f cm ; Smiler rayon %.0f, demi-hauteur %.0f ; Niveau 2 : cellule %.0f, mur %.0f"
          % (c["Look"], c["SlideMin"], c["Moves"], c["PullBack"], c["PlayerR"], c["PlayerHalf"], c["Margin"], c["Lift"], c["Reach"], c["SmilerR"],
             c["SmilerHalf"], c["CellSize"], c["WallThickness"]))
    total, tally = scene_final416(c)
    print("\nA. final416/full, Niveau 2 (graine du niveau 554144564), cellule (1, 5) : %d disposition(s) coherente(s) (marcheur, Smiler, point d'apres)" % total)
    for (before, after), n in sorted(tally.items()):
        print("   regle d'avant : %-13s regle v4.16 : %-13s %d" % (before, after, n))
    before_fails = total > 0 and all(k[0] != "entite seule" for k in tally)
    after_passes = total > 0 and all(k[1] == "entite seule" for k in tally)
    print("   regle d'avant : %s ; regle v4.16 : %s" % ("ECHOUE (decor, PROBLEME 'NON ATTEINT a pied' : defaut reproduit)" if before_fails else "ne reproduit pas le defaut",
                                                   "PASSE (entite seule : ecartee par le test, la marche reprend)" if after_passes else "ECHOUE"))
    ok &= before_fails and after_passes
    print("\nB. Decor : la regle v4.16 n'est pas affaiblie")
    for name, before, after, want in decor_scenes(c):
        good = after == want and (want != "decor" or before == "decor")
        ok &= good
        print("   %-78s avant : %-12s v4.16 : %-12s attendu %-12s %s" % (name, before, after, want, "OK" if good else "ECHEC"))
    print("\nC. Regle v4.16 dans les sources (BRAutoTestV412.cpp : WalkLikeCharacter avec et sans entites, arret de face) : %s"
          % ("oui" if c["RuleInSources"] else "NON"))
    ok &= c["RuleInSources"]
    print("\nRESULTAT : " + ("OK" if ok else "ECHEC"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
