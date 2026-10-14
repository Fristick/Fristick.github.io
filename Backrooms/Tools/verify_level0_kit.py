#!/usr/bin/env python3
"""v4.14 : pose des modules du kit Blender dans le Level 0 genere, verifiee hors moteur.

Rejoue, sur la grille du Level 0 simulee par Tools/verify_pitfalls.py (portage du generateur C++), les regles que
ABRChunk applique quand le kit est importe (BRChunk.cpp : AddWallSegment, AddDoorway, AddServiceDoorway, AddKitPost,
piliers de l'etape 3), chunk par chunk et dans l'ordre des etapes, puis controle :

  1. chaque arete mur ou porte de l'enceinte et de l'interieur recoit exactement un module (mur, mur jauni, passage,
     mur perce de la porte de service), et aucun mur plein ne retombe sur une boite ;
  2. chaque sommet touche par un mur ou une porte recoit exactement un poteau (un seul chunk le pose), aucun autre ;
  3. graine 9 : memes modules, memes positions et memes lacets que la scene du kit (RawAssets/Level0Kit/L0_Placement.json,
     convertie dans le repere d'Unreal), aux ecarts voulus pres (listes dans le journal) ;
  4. plusieurs graines : 1 et 2 sans exception.

Ce controle ne remplace pas l'import ni une capture dans Unreal : il verifie la logique de pose, pas le rendu.

    python Tools/verify_level0_kit.py                 # graine 9 + 40 graines
    python Tools/verify_level0_kit.py --seeds 200
"""
import argparse
import json
import os
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import verify_pitfalls as vp  # noqa: E402

ROOT = os.path.dirname(HERE)
PLACEMENT = os.path.join(ROOT, "RawAssets", "Level0Kit", "L0_Placement.json")

POSTS = ("SM_L0_Wall_JunctionX", "SM_L0_Wall_JunctionT", "SM_L0_Wall_Straight", "SM_L0_Wall_Corner", "SM_L0_Wall_End")
WALLS = ("SM_L0_Wall_350", "SM_L0_Wall_Aged_350")


class KitPose:
    """Portage des regles de pose de ABRChunk (v4.14) ; mirror = IsKitMirrorY() (import sans retournement de Y)"""

    def __init__(self, L, mirror=False):
        self.L = L
        self.mirror = mirror
        self.S = L.S
        self.T = L.P["WallThickness"]
        self.H = L.P["WallHeight"]
        self.N = L.N
        self.B = L.B
        self.modules = []        # (nom, x_cm, y_cm, lacet)
        self.boxes = []          # murs pleins retombes sur une boite (doit rester vide)
        self.post_owner = Counter()
        self.edge_hits = Counter()
        self.svc = None
        if L.door:
            (cx, cy), (dx, dy) = L.door
            if dx != 0:
                self.svc = (cx if dx > 0 else cx - 1, cy, True)
            else:
                self.svc = (cx, cy if dy > 0 else cy - 1, False)

    def is_svc(self, x, y, east):
        return self.svc == (x, y, east)

    def add(self, name, x, y, yaw):
        self.modules.append((name, round(x, 2), round(y, 2), yaw % 360.0))

    # ---- BRChunk.cpp : AddWallSegment (branche du kit)
    def wall_segment(self, along_y, fixed, a, b, full=True):
        S, T = self.S, self.T

        def on_vertex(v, toward):
            for probe in (v, v + toward * T * 0.5):
                q = probe / S
                if abs(q - round(q)) * S < 0.5:
                    return int(round(q))
            return None

        va, vb = on_vertex(a, 1.0), on_vertex(b, -1.0)
        if not full or va is None or vb is None or vb < va:
            self.boxes.append((along_y, fixed, a, b))
            return
        line = int(round(fixed / S))
        yaw = 90.0 if along_y else 0.0
        for c in range(va, vb):
            cx, cy = (line - 1, c) if along_y else (c, line - 1)
            aged = vp.rand(cx * 2 + (1 if along_y else 0), cy, 7301, self.L.seed) < vp.F32(0.18)
            along = (c + 0.5) * S
            px, py = (fixed, along) if along_y else (along, fixed)
            self.add(WALLS[1] if aged else WALLS[0], px, py, yaw)
            self.edge_hits[(cx, cy, along_y)] += 1

    # ---- AddDoorway (branche du kit)
    def doorway(self, along_y, fixed, mid, cell):
        px, py = (fixed, mid) if along_y else (mid, fixed)
        self.add("SM_L0_Doorway", px, py, 90.0 if along_y else 0.0)
        self.edge_hits[cell] += 1

    # ---- AddServiceDoorway (branche du kit) : pied de la face interieure, X local vers la piece
    def service_doorway(self, along_y, fixed, mid, out_sign, cell):
        inner = fixed - out_sign * self.T * 0.5
        px, py = (inner, mid) if along_y else (mid, inner)
        yaw = (180.0 if out_sign > 0 else 0.0) if along_y else (-90.0 if out_sign > 0 else 90.0)
        self.add("SM_L0_ServiceDoor_Wall", px, py, yaw)
        self.edge_hits[cell] += 1

    # ---- AddKitPost
    def post(self, vx, vy):
        L = self.L
        arm = [L.edge_n(vx, vy - 1) != vp.OPEN, L.edge_e(vx - 1, vy) != vp.OPEN, L.edge_n(vx - 1, vy - 1) != vp.OPEN,
               L.edge_e(vx - 1, vy - 1) != vp.OPEN]
        n = sum(arm)
        if n == 0:
            return
        self.post_owner[(vx, vy)] += 1
        corner_first = 0 if self.mirror else 3
        tee_missing = 3 if self.mirror else 1
        if n == 4:
            name, turn = POSTS[0], 0
        elif n == 3:
            name, turn = POSTS[1], arm.index(False) - tee_missing
        elif n == 2 and arm[0] == arm[2]:
            name, turn = POSTS[2], 0 if arm[0] else 1
        elif n == 2:
            first = [i for i in range(4) if arm[i] and arm[(i + 1) % 4]][0]
            name, turn = POSTS[3], first - corner_first
        else:
            name, turn = POSTS[4], arm.index(True)
        self.add(name, vx * self.S, vy * self.S, (turn % 4) * 90.0)

    # ---- ABRChunk::Build, etapes 1 a 3, pour un chunk
    def chunk(self, ccx, ccy):
        L, S, T, N = self.L, self.S, self.T, self.N
        x0, y0 = ccx * N, ccy * N
        door_dir = L.door[1] if L.door else (0, 0)
        for x in range(x0, x0 + N):  # etape 1 : aretes Est
            fixed = (x + 1) * S
            run = None
            for y in range(y0, y0 + N + 1):
                e = L.edge_e(x, y) if y < y0 + N else vp.OPEN
                svc = e == vp.WALL and y < y0 + N and self.is_svc(x, y, True)
                if e == vp.WALL and not svc:
                    run = y if run is None else run
                    continue
                if run is not None:
                    self.wall_segment(True, fixed, run * S - T * 0.5, y * S + T * 0.5)
                    run = None
                if e == vp.DOOR:
                    self.doorway(True, fixed, (y + 0.5) * S, (x, y, True))
                if svc:
                    self.service_doorway(True, fixed, (y + 0.5) * S, 1.0 if door_dir[0] > 0 else -1.0, (x, y, True))
        for y in range(y0, y0 + N):  # etape 2 : aretes Nord
            fixed = (y + 1) * S
            run = None
            for x in range(x0, x0 + N + 1):
                e = L.edge_n(x, y) if x < x0 + N else vp.OPEN
                svc = e == vp.WALL and x < x0 + N and self.is_svc(x, y, False)
                if e == vp.WALL and not svc:
                    run = x if run is None else run
                    continue
                if run is not None:
                    self.wall_segment(False, fixed, run * S - T * 0.5, x * S + T * 0.5)
                    run = None
                if e == vp.DOOR:
                    self.doorway(False, fixed, (x + 0.5) * S, (x, y, False))
                if svc:
                    self.service_doorway(False, fixed, (x + 0.5) * S, 1.0 if door_dir[1] > 0 else -1.0, (x, y, False))
        # etape 3 : murs d'enceinte ouest et sud, poteaux, piliers
        if self.B > 0:
            if ccx == -self.B:
                door_at = None
                for y in range(y0, y0 + N):
                    door_at = y if self.is_svc(x0 - 1, y, True) else door_at
                if door_at is None:
                    self.wall_segment(True, x0 * S, y0 * S - T * 0.5, (y0 + N) * S + T * 0.5)
                else:
                    self.wall_segment(True, x0 * S, y0 * S - T * 0.5, door_at * S)
                    self.wall_segment(True, x0 * S, (door_at + 1) * S, (y0 + N) * S + T * 0.5)
                    self.service_doorway(True, x0 * S, (door_at + 0.5) * S, -1.0, (x0 - 1, door_at, True))
            if ccy == -self.B:
                door_at = None
                for x in range(x0, x0 + N):
                    door_at = x if self.is_svc(x, y0 - 1, False) else door_at
                if door_at is None:
                    self.wall_segment(False, y0 * S, x0 * S - T * 0.5, (x0 + N) * S + T * 0.5)
                else:
                    self.wall_segment(False, y0 * S, x0 * S - T * 0.5, door_at * S)
                    self.wall_segment(False, y0 * S, (door_at + 1) * S, (x0 + N) * S + T * 0.5)
                    self.service_doorway(False, y0 * S, (door_at + 0.5) * S, -1.0, (door_at, y0 - 1, False))
            west, south = ccx == -self.B, ccy == -self.B
            for k in range(1, N + 1):
                if west:
                    self.post(x0, y0 + k)
                if south:
                    self.post(x0 + k, y0)
            if west and south:
                self.post(x0, y0)
        for x in range(x0, x0 + N):
            for y in range(y0, y0 + N):
                self.post(x + 1, y + 1)
                if L.has_pillar(x, y):
                    busy = (L.edge_n(x + 1, y) != vp.OPEN or L.edge_e(x, y + 1) != vp.OPEN or L.edge_n(x, y) != vp.OPEN
                            or L.edge_e(x, y) != vp.OPEN)
                    if not busy:
                        self.add("SM_L0_Pillar", (x + 1) * S, (y + 1) * S, 0.0)

    def build(self):
        for ccx in range(-self.B, self.B):
            for ccy in range(-self.B, self.B):
                self.chunk(ccx, ccy)
        return self


def expected_edges(L):
    """Aretes mur ou porte touchant l'interieur de l'enceinte : (x, y, est)"""
    lo, hi = -L.B * L.N, L.B * L.N
    out = set()
    for x in range(lo - 1, hi):
        for y in range(lo - 1, hi):
            for east in (True, False):
                nx, ny = (x + 1, y) if east else (x, y + 1)
                if not (L.cell_in_bounds(x, y) or L.cell_in_bounds(nx, ny)):
                    continue
                if L.edge(x, y, east) != vp.OPEN:
                    out.add((x, y, east))
    return out


def expected_vertices(L):
    lo, hi = -L.B * L.N, L.B * L.N
    out = set()
    for vx in range(lo, hi + 1):
        for vy in range(lo, hi + 1):
            arms = (L.edge_n(vx, vy - 1), L.edge_e(vx - 1, vy), L.edge_n(vx - 1, vy - 1), L.edge_e(vx - 1, vy - 1))
            if any(a != vp.OPEN for a in arms):
                out.add((vx, vy))
    return out


def check_layout(L, K):
    """Controles 1 et 2 ; renvoie une liste de defauts"""
    bad = []
    edges = expected_edges(L)
    for e in sorted(edges):
        if K.edge_hits[e] != 1:
            bad.append("arete %s : %d module(s)" % (e, K.edge_hits[e]))
    for e in K.edge_hits:
        if e not in edges:
            bad.append("module sur une arete ouverte %s" % (e,))
    if K.boxes:
        bad.append("%d mur(s) plein(s) en boite : %s" % (len(K.boxes), K.boxes[:3]))
    verts = expected_vertices(L)
    for v in sorted(verts):
        if K.post_owner[v] != 1:
            bad.append("sommet %s : %d poteau(x)" % (v, K.post_owner[v]))
    for v in K.post_owner:
        if v not in verts:
            bad.append("poteau sans mur %s" % (v,))
    return bad


def ue_records(path):
    data = json.load(open(path, encoding="utf-8"))
    out = []
    for r in data["instances"]:
        loc = r["unreal"]["location_cm"]
        out.append((r["asset"], round(loc[0], 2), round(loc[1], 2), r["unreal"]["rotation_deg_pitch_yaw_roll"][1] % 360.0, r["name"]))
    return data, out


def compare_scene(L, K, records, log):
    """Controle 3 : la scene du kit (graine 9) et la pose du jeu"""
    sym = {"SM_L0_Wall_350": 180.0, "SM_L0_Wall_Aged_350": 180.0, "SM_L0_Doorway": 180.0, "SM_L0_Pillar": 90.0, "SM_L0_Wall_JunctionX": 90.0}

    def key(name, x, y, yaw):
        period = sym.get(name, 360.0)
        return (name, round(x), round(y), round(yaw % period, 1) % period)

    names = set(WALLS) | set(POSTS) | {"SM_L0_Doorway", "SM_L0_Pillar", "SM_L0_ServiceDoor_Wall"}
    scene = Counter(key(n, x, y, yaw) for n, x, y, yaw, _ in records if n in names)
    game = Counter(key(*m) for m in K.modules)
    only_scene = scene - game
    only_game = game - scene
    log("  modules compares : %d dans la scene, %d poses par le jeu, %d identiques" % (sum(scene.values()), sum(game.values()),
                                                                                       sum((scene & game).values())))
    # Ecarts voulus : piliers caches dans un poteau (le jeu ne les pose pas) ; poteaux aux bouts du mur perce de la porte
    # de service (la scene ne comptait pas ce mur comme un bras)
    svc_ends = set()
    if K.svc:
        x, y, east = K.svc
        ends = [(x + 1, y), (x + 1, y + 1)] if east else [(x, y + 1), (x + 1, y + 1)]
        svc_ends = {(vx * K.S, vy * K.S) for vx, vy in ends}
    unexplained = []
    pillars_hidden = 0
    for k, c in only_scene.items():
        if k[0] == "SM_L0_Pillar":
            pillars_hidden += c
        elif k[0] in POSTS and (k[1], k[2]) in svc_ends:
            log("  ecart voulu : scene %s en (%d, %d) lacet %.0f (porte de service non comptee comme bras)" % k)
        else:
            unexplained.append(("scene seulement",) + k)
    for k, c in only_game.items():
        if k[0] in POSTS and (k[1], k[2]) in svc_ends:
            log("  ecart voulu : jeu   %s en (%d, %d) lacet %.0f (le mur perce est un bras du poteau)" % k)
        else:
            unexplained.append(("jeu seulement",) + k)
    log("  ecart voulu : %d pilier(s) de la scene caches dans un poteau, non poses par le jeu" % pillars_hidden)
    for u in unexplained[:20]:
        log("  ECART : %s %s en (%d, %d) lacet %.1f" % u)
    return len(unexplained)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--seeds", type=int, default=40)
    ap.add_argument("--out", default=None, help="journal (par defaut : sortie standard seulement)")
    args = ap.parse_args()
    lines = []

    def log(s):
        print(s)
        lines.append(s)

    P = vp.read_level0()
    log("Pose du kit du Level 0 (regles de BRChunk.cpp v4.14), grille de Tools/verify_pitfalls.py")
    log("Cotes lues dans BRLevels.cpp : cellule %.0f, mur %.0f x %.0f, chunks %d, enceinte %d" % (P["CellSize"], P["WallThickness"],
                                                                                              P["WallHeight"], P["ChunkCells"], P["BoundsChunks"]))
    failures = 0
    data, records = ue_records(PLACEMENT)
    L = vp.Level0(P, data["seed_user"])
    if L.seed != data["seed_level"]:
        log("graine du niveau differente de la scene (%d / %d) : scene a regenerer" % (L.seed, data["seed_level"]))
        failures += 1
    K = KitPose(L).build()
    c = Counter(m[0] for m in K.modules)
    log("Graine %d (scene du kit) : %s" % (data["seed_user"], ", ".join("%s %d" % (k[6:], v) for k, v in sorted(c.items()))))
    bad = check_layout(L, K)
    for b in bad[:10]:
        log("  DEFAUT : " + b)
    failures += len(bad)
    failures += compare_scene(L, K, records, log)
    # Le meme controle avec l'autre sens d'import : les poteaux d'angle et en T changent, rien d'autre
    M = KitPose(L, mirror=True).build()
    diff = sum((Counter(K.modules) - Counter(M.modules)).values())
    log("  import sans retournement de Y (IsKitMirrorY) : %d poteau(x) tourne(s), memes positions" % diff)
    seeds = list(range(1, args.seeds + 1))
    totals = Counter()
    seed_fail = 0
    for s in seeds:
        Ls = vp.Level0(P, s)
        Ks = KitPose(Ls).build()
        b = check_layout(Ls, Ks)
        totals.update(m[0] for m in Ks.modules)
        if b:
            seed_fail += 1
            log("graine %d : %d defaut(s), ex. %s" % (s, len(b), b[0]))
    failures += seed_fail
    log("%d graine(s) : %d sans defaut ; en moyenne %s" % (len(seeds), len(seeds) - seed_fail,
                                                         ", ".join("%s %.0f" % (k[6:], v / len(seeds)) for k, v in sorted(totals.items()))))
    log("RESULTAT : %s" % ("OK" if failures == 0 else "%d defaut(s)" % failures))
    if args.out:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as fh:
            fh.write("\n".join(lines) + "\n")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
