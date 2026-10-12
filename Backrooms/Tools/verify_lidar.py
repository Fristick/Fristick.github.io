"""
v4.13 : verification hors moteur de l'aile LiDAR du Level 6 dans la grille du niveau.

Ce script REJOUE la grille du Level 6 telle que le C++ la genere (meme hachage BRHash, memes aretes "Rooms" que
Tools/verify_pitfalls.py, memes regles de l'aile que BRLidarLogic.cpp : PickWing, EdgeBetween, HoleAtCorner), sans
Unreal, et verifie pour chaque graine :
  - l'aile est posee dans l'anneau prevu, a l'ecart du depart ;
  - l'entree de l'aile (la galerie devant ses quatre portes, salle de la mission) est atteinte a pied depuis le depart
    sans entrer dans l'aile (parcours en largeur, comme ABRWorld::PlaceMission) ; distance de marche ;
  - depuis la galerie, les quatre portes donnent dans l'aile, et l'aile ne communique pas avec le reste du niveau
    autrement que par ses portes (les trois autres cotes sont des murs pleins).
La logique fine de l'aile (trous, passages de 140 cm, alcoves, echelle) est verifiee par Tools/Lidar/test_lidar.cpp.

Usage :  python Tools/verify_lidar.py [--seeds 1-500]
Code de sortie : 0 si tout passe. La sortie sert de journal (Docs/v413/lidar_aile.txt).
"""
import argparse
import os
import re
import sys
from collections import deque

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import verify_pitfalls as vp  # noqa: E402

WING = 4
RING = 2
OPEN, WALL, DOOR = vp.OPEN, vp.WALL, vp.DOOR


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

    def __init__(self, P, user_seed):
        self.P = P
        self.user_seed = user_seed
        self.seed = vp.mix(user_seed ^ (6 * 7919 + 17)) or 1  # ABRWorld::SeedFromUser(n, 6)
        self.N = P["ChunkCells"]
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
        e = super().edge(x, y, east)
        if e == WALL and not (self.spawn_area(x, y) and self.spawn_area(*((x + 1, y) if east else (x, y + 1)))) \
                and self.pit_edge(x, y, east) is None and corridor_edge(self.wing, (x, y), (x + 1, y) if east else (x, y + 1)):
            return DOOR
        return e


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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", default="1-500")
    a = ap.parse_args()
    lo, hi = (int(x) for x in a.seeds.split("-"))
    P = read_level(6)
    print("Level 6 (Tools/verify_lidar.py) : parametres lus dans BRLevels.cpp : bLidar=%s, CellSize=%g, PitHoleSize=%g, PitPassage=%g, WallLineChance=%g, DoorChance=%g"
          % (P["bLidar"], P["CellSize"], P.get("PitHoleSize", 0), P.get("PitPassage", 0), P["WallLineChance"], P["DoorChance"]))
    if not P["bLidar"]:
        print("ECHEC : Level6() sans bLidar")
        return 1
    bad = 0
    dists = []
    for s in range(lo, hi + 1):
        L = Level6(P, s)
        r = check(L)
        if r["ok"]:
            dists.append(r["dist"])
        else:
            bad += 1
            if bad <= 12:
                print("ECHEC graine %d : %s (aile au chunk %s)" % (s, ", ".join(r["why"]), L.wing["chunk"]))
    n = hi - lo + 1
    if dists:
        dists.sort()
        print("Entree de l'aile atteinte depuis le depart : %d/%d graines ; distance de marche (cellules de %g m) : min %d, mediane %d, max %d"
              % (len(dists), n, P["CellSize"] / 100.0, dists[0], dists[len(dists) // 2], dists[-1]))
    print("Pourtour : 4 portes d'entree et aucun autre passage sur les graines conformes")
    print("RESULTAT : %s (%d echec(s))" % ("OK" if bad == 0 else "ECHEC", bad))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
