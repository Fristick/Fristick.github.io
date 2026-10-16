"""
v4.13 : verification hors moteur de l'aile LiDAR du Level 6 dans la grille du niveau.

Ce script REJOUE la grille du Level 6 telle que le C++ la genere (meme hachage BRHash, memes aretes "Rooms" que
Tools/verify_pitfalls.py, memes regles de l'aile que BRLidarLogic.cpp : PickWing, EdgeBetween, HoleAtCorner), sans
Unreal, et verifie pour chaque graine :
  - l'aile est posee dans l'anneau prevu, a l'ecart du depart ;
  - l'entree de l'aile (la galerie devant ses quatre portes, salle de la mission) est atteinte a pied depuis le depart
    sans entrer dans l'aile (parcours en largeur) ; distance de marche ;
  - depuis la galerie, les quatre portes donnent dans l'aile, et l'aile ne communique pas avec le reste du niveau
    autrement que par ses portes (les trois autres cotes sont des murs pleins).
v4.15 : et le parcours de ABRWorld::PlaceMission lui-meme : plafond de la zone de mission (BaseMaxCells =
ceil(9500 / CellSize), lu dans BRMissionWorld.cpp ; cellules a ce plafond non developpees ; limite de 6000 cellules).
  - regle v4.14 : galerie au-dela du plafond -> "repli" (mecanismes du passage dans une salle ordinaire, sans portes ni
    echelle de l'aile) ; pres d'une graine sur deux ;
  - regle v4.15 : nouveau parcours avec un plafond releve a la longueur du couloir garanti + 2 (BRLidar::CorridorLength)
    pour la seule salle de l'aile -> 0 repli attendu ; la distance a pied de la galerie V = 1 est exactement la longueur
    du couloir (|GX| + |GY|, au plus 41).
La logique fine de l'aile (trous, passages de 140 cm, alcoves, echelle) est verifiee par Tools/Lidar/test_lidar.cpp.

Graines : --seeds a-b : graines -BRSeed (graine du niveau = SeedFromUser(n, 6) = Mix(n ^ (6 * 7919 + 17))) ;
--level-seeds : graines du NIVEAU telles que le jeu les journalise ("graine 2702236446"), sans nouveau Mix ;
--random N --rng-seed K : N graines du niveau tirees sur 32 bits (comme la v4.15, BRWorld.cpp) ;
--crt : les 32768 graines aleatoires de la v4.14 sous Windows (rand() de 0 a 32767 : r * 2654435761 ^ 47531).
Chaque ligne donne la graine -BRSeed (ou "-") et la graine du niveau. Les graines de regression (BRSeed 4136, 6, 208,
159 ; graines du niveau 2702236446, 2878398681, 2600076685, 2622470042) sont toujours verifiees.

Usage :  python Tools/verify_lidar.py [--seeds 1-500] [--level-seeds 2702236446,2878398681] [--random 2000 --rng-seed 7]
         [--crt] [--jobs 8] [--sheet Docs/v413/lidar_ailes_6_graines.png --sheet-seeds 1-6]
Code de sortie : 0 si tout passe (structure de l'aile, regression, 0 repli avec la regle v4.15). La sortie sert de
journal (Docs/v415/correctifs_aile_lidar.txt).
--sheet : planche vue de dessus (simulation, PAS une capture du jeu) : aile, trous, alcove de l'echelle, couloir garanti.
"""
import argparse
import math
import os
import random
import re
import sys
from collections import deque
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import verify_pitfalls as vp  # noqa: E402

WING = 4
RING = 2
OPEN, WALL, DOOR = vp.OPEN, vp.WALL, vp.DOOR
MISSION_CPP = os.path.join(vp.ROOT, "Source", "Backrooms", "Private", "BRMissionWorld.cpp")
# Graines de regression (diag_v413_4) : BRSeed -> regle v4.14 attendue ; graines du niveau journalisees en jeu
REG_BRSEEDS = {4136: "repli", 6: "repli", 208: "repli", 159: "aile"}
REG_LEVEL_SEEDS = {2702236446: "repli", 2878398681: "aile", 2600076685: "aile", 2622470042: "aile"}


def read_mission_rules(cell_size):
    """Plafond et limite du parcours de ABRWorld::PlaceMission, et presence de la regle v4.15 (plafond de l'aile)"""
    src = open(MISSION_CPP, encoding="utf-8").read()
    body = src[src.index("bool ABRWorld::PlaceMission("):]
    body = body[:body.index("bool ABRWorld::PlaceMissionModule(")]
    m = re.search(r"MaxCells\s*=\s*FMath::CeilToInt\(([\d.]+)f\s*/\s*S\)", body)
    lim = re.search(r"Order\.Num\(\)\s*<\s*(\d+);\s*\+\+Head", body)
    wing_fix = re.search(r"BRLidar::CorridorLength\(LidarWing\)\s*\+\s*FMath::Abs\(Start\.X\)\s*\+\s*FMath::Abs\(Start\.Y\)\s*\+\s*(\d+)", body)
    meters = float(m.group(1))
    return dict(meters=meters, cap=int(math.ceil(meters / cell_size)), limit=int(lim.group(1)) if lim else 6000,
                wing_margin=int(wing_fix.group(1)) if wing_fix else None)


def read_level(n):
    src = open(vp.LEVELS_CPP, encoding="utf-8").read()
    body = src[src.index("FBRLevelDef Level%d()" % n):]
    body = body[:body.index("return D;")]
    p = {}
    for name, val in re.findall(r"D\.(\w+)\s*=\s*(-?[\d.]+)f?\s*;", body):
        p[name] = float(val)
    defaults = dict(ChunkCells=8, CellSize=350, WallThickness=60, SegmentLength=4, WallLineChance=0.55, DoorChance=0.3,
                    OpenZoneChance=0.15, BoundsChunks=0, PitRoomChance=0.0, PitRoomCells=5, PitHoleSize=200, PitPassage=150)
    for k, v in defaults.items():
        p.setdefault(k, v)
    for k in ("ChunkCells", "SegmentLength", "BoundsChunks", "PitRoomCells"):
        p[k] = int(p[k])
    p["bLidar"] = "D.bLidar = true" in body
    p["Exits"] = []
    return p


def pick_wing(seed, n):
    """BRLidar::PickWing"""
    lo, hi = -RING - 1, RING
    cands = [(cx, cy) for cx in range(lo, hi + 1) for cy in range(lo, hi + 1) if cx in (lo, hi) or cy in (lo, hi)]
    ch = cands[vp.bhash(0, 0, 6600, seed) % len(cands)]
    free = n - WING - 2
    mx = ch[0] * n + 1 + vp.bhash(ch[0], ch[1], 6601, seed) % (free + 1)
    my = ch[1] * n + 1 + vp.bhash(ch[0], ch[1], 6602, seed) % (free + 1)
    cx, cy = ch[0] * n + n // 2, ch[1] * n + n // 2
    if abs(cx) >= abs(cy):
        out = (-1 if cx > 0 else 1, 0)
    else:
        out = (0, -1 if cy > 0 else 1)
    ladder = vp.bhash(ch[0], ch[1], 6603, seed) % WING
    return dict(chunk=ch, min=(mx, my), out=out, ladder=ladder)


def contains(w, c):
    return w["min"][0] <= c[0] < w["min"][0] + WING and w["min"][1] <= c[1] < w["min"][1] + WING


def to_local(w, c):
    lx, ly = c[0] - w["min"][0], c[1] - w["min"][1]
    o = w["out"]
    if o == (-1, 0):
        return lx, ly
    if o == (1, 0):
        return WING - 1 - lx, ly
    if o == (0, -1):
        return ly, lx
    return WING - 1 - ly, lx


def to_cell(w, u, v):
    mx, my = w["min"]
    o = w["out"]
    if o == (-1, 0):
        return mx + u, my + v
    if o == (1, 0):
        return mx + WING - 1 - u, my + v
    if o == (0, -1):
        return mx + v, my + u
    return mx + v, my + WING - 1 - u


def corridor_end(w):
    c = to_cell(w, 0, 1)
    return (c[0] + w["out"][0], c[1] + w["out"][1])


def corridor_edge(w, a, b):
    """BRLidar::CorridorEdge"""
    gx, gy = corridor_end(w)
    between = lambda v, lo, hi: (lo <= v <= hi) if lo <= hi else (hi <= v <= lo)
    if w["out"][0] != 0:
        s1 = lambda c: c[0] == 0 and between(c[1], 0, gy)
        s2 = lambda c: c[1] == gy and between(c[0], 0, gx)
    else:
        s1 = lambda c: c[1] == 0 and between(c[0], 0, gx)
        s2 = lambda c: c[0] == gx and between(c[1], 0, gy)
    return (s1(a) and s1(b)) or (s2(a) and s2(b))


def edge_between(w, a, b):
    """BRLidar::EdgeBetween"""
    ia, ib = contains(w, a), contains(w, b)
    if not ia and not ib:
        return OPEN
    if ia != ib:
        d = (b[0] - a[0], b[1] - a[1]) if ia else (a[0] - b[0], a[1] - b[1])
        return DOOR if d == w["out"] else WALL
    ua, va = to_local(w, a)
    ub, vb = to_local(w, b)
    if ua != ub:
        return DOOR if max(ua, ub) == WING - 1 else OPEN
    return WALL if ua == WING - 1 else OPEN


class Level6(vp.Level0):
    """Grille du Level 6 : aretes "Rooms" ordinaires, aile LiDAR a la place des salles de fosses"""

    def __init__(self, P, user_seed, level_seed=None):
        self.P = P
        self.user_seed = user_seed
        # ABRWorld::SeedFromUser(n, 6) ; v4.15 : ou la graine du niveau telle que le jeu la journalise (sans nouveau Mix)
        self.seed = level_seed if level_seed is not None else (vp.mix(user_seed ^ (6 * 7919 + 17)) or 1)
        self.N = P["ChunkCells"]
        self._edges = {}
        self.S = P["CellSize"]
        self.B = 0
        self.pit_rooms = {}
        self.forced = set()
        self.wing = pick_wing(self.seed, self.N)

    def has_pits(self):
        return True

    def is_pit_cell(self, x, y):
        return contains(self.wing, (x, y))

    def pit_edge(self, x, y, east):
        a = (x, y)
        b = (x + 1, y) if east else (x, y + 1)
        ch = self.chunk_of(a)
        if ch != self.chunk_of(b) or ch != self.wing["chunk"]:
            return None
        return edge_between(self.wing, a, b)

    def edge(self, x, y, east):
        """ABRWorld::EdgeE / EdgeN : aretes ordinaires, puis le couloir garanti (un mur tire y recoit une porte)"""
        k = (x, y, east)
        e = self._edges.get(k)
        if e is not None:
            return e
        e = super().edge(x, y, east)
        if e == WALL and not (self.spawn_area(x, y) and self.spawn_area(*((x + 1, y) if east else (x, y + 1)))) \
                and self.pit_edge(x, y, east) is None and corridor_edge(self.wing, (x, y), (x + 1, y) if east else (x, y + 1)):
            e = DOOR
        self._edges[k] = e
        return e


def hole_at_corner(w, x, y):
    """BRLidar::HoleAtCorner : trou au coin (x + 1, y + 1) de la cellule (x, y)"""
    if not contains(w, (x, y)) or not contains(w, (x + 1, y + 1)):
        return False
    return min(to_local(w, (x, y))[0], to_local(w, (x + 1, y + 1))[0]) in (0, 1)


def walk_path(L, goal, radius=48):
    """Plus court chemin a pied du depart a goal, hors de l'aile (parcours en largeur)"""
    parent = {(0, 0): None}
    q = deque([(0, 0)])
    while q:
        c = q.popleft()
        if c == goal:
            break
        for d in vp.DIRS:
            n = (c[0] + d[0], c[1] + d[1])
            if n in parent or abs(n[0]) > radius or abs(n[1]) > radius or L.is_pit_cell(*n) or not L.can_step(c, n):
                continue
            parent[n] = c
            q.append(n)
    if goal not in parent:
        return []
    path, c = [], goal
    while c is not None:
        path.append(c)
        c = parent[c]
    return path[::-1]


def draw_wing_map(L, res, size=14):
    from PIL import Image, ImageDraw
    w = L.wing
    path = walk_path(L, res["room"]) if res["room"] else []
    xs = [0, w["min"][0], w["min"][0] + WING] + [c[0] for c in path]
    ys = [0, w["min"][1], w["min"][1] + WING] + [c[1] for c in path]
    x0, x1, y0, y1 = min(xs) - 2, max(xs) + 2, min(ys) - 2, max(ys) + 2
    W, H = (x1 - x0) * size + 1, (y1 - y0) * size + 1
    img = Image.new("RGB", (max(W, 260), H + 30), (12, 14, 18))
    d = ImageDraw.Draw(img)

    def px(x, y):  # cellule -> pixels (y vers le haut)
        return (int((x - x0) * size), int((y1 - y) * size))
    for x in range(x0, x1):
        for y in range(y0, y1):
            a = px(x, y + 1)
            col = (40, 46, 56)
            if contains(w, (x, y)):
                u, _v = to_local(w, (x, y))
                col = (70, 60, 40) if u < WING - 1 else (46, 70, 60)
            d.rectangle([a[0], a[1], a[0] + size - 1, a[1] + size - 1], fill=col)
    # couloir garanti et chemin a pied
    if len(path) > 1:
        d.line([px(c[0] + 0.5, c[1] + 0.5) for c in path], fill=(90, 150, 255), width=2)
    # murs et portes
    for x in range(x0, x1):
        for y in range(y0, y1):
            for east in (True, False):
                e = L.edge(x, y, east)
                if e == OPEN:
                    continue
                a, b = (px(x + 1, y), px(x + 1, y + 1)) if east else (px(x, y + 1), px(x + 1, y + 1))
                d.line([a, b], fill=(200, 205, 215) if e == WALL else (90, 200, 120), width=2 if e == WALL else 1)
    # trous (PitHoleSize, aux angles interieurs)
    half = L.P["PitHoleSize"] / L.P["CellSize"] * size * 0.5
    for x in range(w["min"][0], w["min"][0] + WING):
        for y in range(w["min"][1], w["min"][1] + WING):
            if hole_at_corner(w, x, y):
                c = px(x + 1, y + 1)
                d.rectangle([c[0] - half, c[1] - half, c[0] + half, c[1] + half], fill=(255, 140, 30))
    # alcove de l'echelle (et emetteur de parasites), depart
    lc = to_cell(w, WING - 1, w["ladder"])
    c = px(lc[0] + 0.5, lc[1] + 0.5)
    d.ellipse([c[0] - 4, c[1] - 4, c[0] + 4, c[1] + 4], fill=(220, 80, 230))
    c = px(0.5, 0.5)
    d.ellipse([c[0] - 4, c[1] - 4, c[0] + 4, c[1] + 4], fill=(255, 255, 255))
    d.text((3, H + 3), "BRSeed=%d  %s  %d cellules a pied" % (L.user_seed, "OK" if res["ok"] else "ECHEC", res["dist"]), fill=(230, 230, 230))
    d.text((3, H + 15), "simulation, pas une capture du jeu", fill=(150, 150, 150))
    return img


def sheet(rows, path, cols=3):
    from PIL import Image, ImageDraw
    maps = [draw_wing_map(L, r) for L, r in rows]
    cw = max(m.size[0] for m in maps) + 8
    ch = max(m.size[1] for m in maps) + 8
    nrows = (len(maps) + cols - 1) // cols
    legend = 44
    out = Image.new("RGB", (cols * cw + 8, nrows * ch + 8 + legend), (6, 6, 8))
    for i, m in enumerate(maps):
        out.paste(m, (8 + (i % cols) * cw, 8 + (i // cols) * ch))
    d = ImageDraw.Draw(out)
    y = nrows * ch + 10
    d.text((8, y), "Level 6 : aile LiDAR (Tools/verify_lidar.py). Brun : zone percee ; vert : alcoves ; orange : trous de %g cm ; violet : echelle et parasites ;"
           % rows[0][0].P["PitHoleSize"], fill=(220, 220, 220))
    d.text((8, y + 14), "blanc : depart ; bleu : chemin a pied le plus court jusqu'a l'entree ; traits verts : portes et passages ; traits clairs : murs.", fill=(220, 220, 220))
    d.text((8, y + 28), "Simulation de la grille du jeu, PAS une capture : rendu, eclairage et decor ne sont pas representes.", fill=(160, 160, 160))
    out.save(path)


def check(L, radius=48):
    w = L.wing
    res = {"ok": True, "why": []}
    # Parcours depuis le depart, hors de l'aile (PlaceMission : Allowed exclut les salles de fosses)
    dist = {(0, 0): 0}
    q = deque([(0, 0)])
    while q:
        c = q.popleft()
        for d in vp.DIRS:
            n = (c[0] + d[0], c[1] + d[1])
            if n in dist or abs(n[0]) > radius or abs(n[1]) > radius or L.is_pit_cell(*n) or not L.can_step(c, n):
                continue
            dist[n] = dist[c] + 1
            q.append(n)
    out = w["out"]
    room = None
    for v in (1, 2, 0, 3):
        c = to_cell(w, 0, v)
        g = (c[0] + out[0], c[1] + out[1])
        if g in dist:
            room = g
            break
    res["room"] = room
    res["dist"] = dist.get(room, -1)
    if room is None:
        res["ok"] = False
        res["why"].append("entree de l'aile non atteinte")
    # Portes : chaque cellule d'entree s'ouvre sur la galerie ; aucun autre passage du pourtour
    doors = 0
    leaks = 0
    for u in range(WING):
        for v in range(WING):
            c = to_cell(w, u, v)
            for d in vp.DIRS:
                n = (c[0] + d[0], c[1] + d[1])
                if contains(w, n):
                    continue
                e = L.edge(c[0], c[1], True) if d == (1, 0) else (L.edge(n[0], n[1], True) if d == (-1, 0) else (L.edge(c[0], c[1], False) if d == (0, 1) else L.edge(n[0], n[1], False)))
                if d == out:
                    doors += 1 if e == DOOR else 0
                elif e != WALL:
                    leaks += 1
    res["doors"] = doors
    res["leaks"] = leaks
    if doors != WING:
        res["ok"] = False
        res["why"].append("%d portes d'entree au lieu de 4" % doors)
    if leaks:
        res["ok"] = False
        res["why"].append("%d passages hors des portes" % leaks)
    return res


def mission_bfs(L, cap, limit):
    """Parcours de ABRWorld::PlaceMission (RunBfs) : depuis (0, 0), hors de l'aile, cellules a la distance cap non
    developpees, au plus limit cellules"""
    dist = {(0, 0): 0}
    order = [(0, 0)]
    head = 0
    while head < len(order) and len(order) < limit:
        c = order[head]
        head += 1
        cd = dist[c]
        if cd >= cap:
            continue
        for d in vp.DIRS:
            n = (c[0] + d[0], c[1] + d[1])
            if n in dist or L.is_pit_cell(*n) or not L.can_step(c, n):
                continue
            dist[n] = cd + 1
            order.append(n)
    return dist


def gallery_cell(w, v):
    c = to_cell(w, 0, v)
    return (c[0] + w["out"][0], c[1] + w["out"][1])


def gallery(L, dist, cap=None):
    """Salle de l'aile : premiere galerie atteinte (V = 1, 2, 0, 3), dans le plafond cap s'il est donne"""
    for v in (1, 2, 0, 3):
        g = gallery_cell(L.wing, v)
        if g in dist and (cap is None or dist[g] <= cap):
            return g, v
    return None, None


def placement(L, rules):
    """Salle de la mission du Level 6 selon la regle v4.14 (plafond de base) et la regle v4.15 (plafond releve a la longueur
    du couloir garanti + 2, seulement si la galerie manque). Un seul parcours au plafond releve suffit : ses cellules a
    BaseMaxCells ou moins sont exactement celles du parcours de base (meme ordre, memes distances)"""
    base = rules["cap"]
    gx, gy = corridor_end(L.wing)
    corr = abs(gx) + abs(gy)
    margin = rules["wing_margin"] if rules["wing_margin"] is not None else 2
    cap = max(base, corr + margin)
    dist = mission_bfs(L, cap, rules["limit"])
    g_old, v_old = gallery(L, dist, base)
    g_new, v_new = (g_old, v_old) if g_old is not None else gallery(L, dist)
    return dict(corr=corr, cap=cap if g_old is None else base, old="aile" if g_old else "repli", new="aile" if g_new else "repli",
                v_old=v_old, v_new=v_new, room=g_new, d_v1=dist.get((gx, gy), -1), cells=len(dist),
                gal=[dist.get(gallery_cell(L.wing, v), -1) for v in (1, 2, 0, 3)])


def literal_check(L, rules):
    """Controle du raccourci : le parcours au plafond de base, tel quel, donne le meme verdict v4.14"""
    d0 = mission_bfs(L, rules["cap"], rules["limit"])
    return "aile" if gallery(L, d0)[0] else "repli"


def evaluate(args):
    P, rules, user_seed, level_seed, full = args
    L = Level6(P, user_seed if user_seed is not None else 0, level_seed)
    r = placement(L, rules)
    r["seed"] = L.seed
    r["user"] = user_seed
    r["chunk"] = L.wing["chunk"]
    if full:
        c = check(L)
        r["ok"] = c["ok"]
        r["why"] = c["why"]
        r["walk"] = c["dist"]
    return r


def run_all(jobs, items):
    if jobs > 1 and len(items) > 50:
        with Pool(jobs) as pool:
            return pool.map(evaluate, items, chunksize=8)
    return [evaluate(it) for it in items]


def seed_label(r):
    return "BRSeed %s / graine du niveau %u" % (r["user"] if r["user"] is not None else "-", r["seed"])


def parse_list(text):
    out = []
    for part in text.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part[1:]:
            a, b = part.split("-", 1)
            out.extend(range(int(a), int(b) + 1))
        else:
            out.append(int(part))
    return out


def summary(name, rows):
    n = len(rows)
    old = sum(1 for r in rows if r["old"] == "repli")
    new = sum(1 for r in rows if r["new"] == "repli")
    viol = sum(1 for r in rows if r["d_v1"] != r["corr"] or r["corr"] > 41)
    print("%s : %d graines ; regle v4.14 (plafond seul) : %d repli(s) (%.2f %%) ; regle v4.15 : %d repli(s) ; couloir de %d a %d cellules, "
          "BFS(V=1) different du couloir ou couloir > 41 : %d"
          % (name, n, old, 100.0 * old / max(1, n), new, min((r["corr"] for r in rows), default=0), max((r["corr"] for r in rows), default=0), viol))
    return new, viol


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", default="1-500", help="graines -BRSeed (a-b ou liste)")
    ap.add_argument("--level-seeds", default="", help="graines du niveau (journal du jeu), sans nouveau Mix")
    ap.add_argument("--random", type=int, default=0, help="N graines du niveau tirees sur 32 bits")
    ap.add_argument("--rng-seed", type=int, default=7)
    ap.add_argument("--crt", action="store_true", help="les 32768 graines aleatoires de la v4.14 sous Windows (rand() 0..32767)")
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 1))
    ap.add_argument("--sheet", default=None, help="planche PNG (simulation, pas une capture du jeu)")
    ap.add_argument("--sheet-seeds", default="1-6")
    a = ap.parse_args()
    P = read_level(6)
    rules = read_mission_rules(P["CellSize"])
    print("Level 6 (Tools/verify_lidar.py) : parametres lus dans BRLevels.cpp : bLidar=%s, CellSize=%g, PitHoleSize=%g, PitPassage=%g, WallLineChance=%g, DoorChance=%g"
          % (P["bLidar"], P["CellSize"], P.get("PitHoleSize", 0), P.get("PitPassage", 0), P["WallLineChance"], P["DoorChance"]))
    print("ABRWorld::PlaceMission (BRMissionWorld.cpp) : plafond de base %d cellules (%.0f m / %g cm), limite %d cellules ; regle de l'aile v4.15 dans le source : %s"
          % (rules["cap"], rules["meters"] / 100.0, P["CellSize"], rules["limit"],
             ("oui (couloir + %d)" % rules["wing_margin"]) if rules["wing_margin"] is not None else "NON (repli des que la galerie depasse le plafond)"))
    if not P["bLidar"]:
        print("ECHEC : Level6() sans bLidar")
        return 1
    bad = 0
    # 1. Graines de regression : verdict v4.14 attendu (journaux du jeu), 0 repli en v4.15, parcours litteral identique
    items = [(P, rules, u, None, True) for u in REG_BRSEEDS] + [(P, rules, None, s, True) for s in REG_LEVEL_SEEDS]
    print("\nGraines de regression :")
    for r in run_all(1, items):
        want = REG_BRSEEDS[r["user"]] if r["user"] is not None else REG_LEVEL_SEEDS[r["seed"]]
        lit = literal_check(Level6(P, r["user"] or 0, None if r["user"] is not None else r["seed"]), rules)
        ok = r["old"] == want and lit == want and r["new"] == "aile" and r["d_v1"] == r["corr"] and r["ok"]
        bad += 0 if ok else 1
        print("  %s : aile au chunk %s, couloir %d, galeries V=1,2,0,3 a %s ; v4.14 : %s (attendu %s, parcours litteral : %s) ; v4.15 : %s (plafond %d, galerie V=%s) -> %s"
              % (seed_label(r), r["chunk"], r["corr"], "/".join(str(g) for g in r["gal"]), r["old"], want, lit, r["new"], r["cap"], r["v_new"], "OK" if ok else "ECHEC"))
    # 2. Graines -BRSeed : structure de l'aile (portes, pourtour, entree atteinte) et placement
    seeds = parse_list(a.seeds)
    rows = run_all(a.jobs, [(P, rules, u, None, True) for u in seeds])
    dists = []
    shown = 0
    for r in rows:
        if r["ok"]:
            dists.append(r["walk"])
        else:
            bad += 1
            shown += 1
            if shown <= 12:
                print("ECHEC %s : %s (aile au chunk %s)" % (seed_label(r), ", ".join(r["why"]), r["chunk"]))
    if dists:
        dists.sort()
        print("\nEntree de l'aile atteinte depuis le depart : %d/%d graines ; distance de marche (cellules de %g m) : min %d, mediane %d, max %d"
              % (len(dists), len(rows), P["CellSize"] / 100.0, dists[0], dists[len(dists) // 2], dists[-1]))
    print("Pourtour : 4 portes d'entree et aucun autre passage sur les graines conformes")
    groups = [("BRSeed %s" % a.seeds, rows)]
    # 3. Graines du niveau (journal du jeu), tirages 32 bits, graines Windows de la v4.14
    if a.level_seeds:
        lrows = run_all(a.jobs, [(P, rules, None, s, False) for s in parse_list(a.level_seeds)])
        for r in lrows:
            print("  %s : couloir %d ; v4.14 : %s ; v4.15 : %s (plafond %d)" % (seed_label(r), r["corr"], r["old"], r["new"], r["cap"]))
        groups.append(("graines du niveau %s" % a.level_seeds, lrows))
    if a.random > 0:
        rng = random.Random(a.rng_seed)
        rs = [(rng.getrandbits(32) or 1) for _ in range(a.random)]
        groups.append(("%d graines du niveau sur 32 bits (generateur %d)" % (a.random, a.rng_seed), run_all(a.jobs, [(P, rules, None, s, False) for s in rs])))
    if a.crt:
        cs = [(((r * 2654435761) & 0xFFFFFFFF) ^ (6 * 7919 + 17)) or 1 for r in range(32768)]
        groups.append(("32768 graines aleatoires Windows v4.14 (rand() 0..32767)", run_all(a.jobs, [(P, rules, None, s, False) for s in cs])))
    print()
    for name, g in groups:
        new, viol = summary(name, g)
        bad += new + viol
    if a.sheet:
        slo, shi = (int(x) for x in a.sheet_seeds.split("-"))
        sheet_rows = []
        for sd in range(slo, shi + 1):
            L = Level6(P, sd)
            sheet_rows.append((L, check(L)))
        sheet(sheet_rows, a.sheet)
        print("Planche : %s (graines %d a %d ; simulation, pas une capture du jeu)" % (a.sheet, slo, shi))
    if rules["wing_margin"] is None:
        print("NOTE : le source n'a pas la regle v4.15 (plafond de l'aile) : le jeu se comporte comme la colonne v4.14")
        bad += 1
    print("RESULTAT : %s (%d echec(s))" % ("OK" if bad == 0 else "ECHEC", bad))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
