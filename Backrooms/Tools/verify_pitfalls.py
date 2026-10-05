"""
v4.6 : verification hors moteur des salles de fosses du Niveau 0 ("Hole Variation").

Ce script REJOUE la generation du Niveau 0 telle que le C++ l'ecrit (meme hachage BRHash, memes regles d'aretes, de
salles de fosses, de cachettes, de cassettes VHS et de sorties, meme decoupage du sol), sans Unreal. Il ne remplace pas
un test dans le moteur : il verifie la LOGIQUE de generation (determinisme, placement, accessibilite, raccords du sol),
pas le rendu, la physique ni le reseau.

Parametres : lus dans Source/Backrooms/Private/BRLevels.cpp (fonction Level0()) pour rester synchronises avec le jeu.

Usage :
    python Tools/verify_pitfalls.py                       # graines 1 a 24 + graine de demonstration, rapport texte
    python Tools/verify_pitfalls.py --seeds 1-50 --maps Docs/v46/cartes_graines.png
    python Tools/verify_pitfalls.py --seed 4605 --json out.json   # boites du chunk de la salle (rendu Blender)

Les graines sont celles de -BRSeed=<n> (et de la commande console BRSeed <n>) : graine du niveau = Mix(n ^ 17).
Code de sortie : 0 si toutes les verifications passent.
"""
import argparse
import json
import math
import os
import re
import sys
from collections import deque

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
LEVELS_CPP = os.path.join(ROOT, "Source", "Backrooms", "Private", "BRLevels.cpp")
WORLD_H = os.path.join(ROOT, "Source", "Backrooms", "Public", "BRWorld.h")
F32 = np.float32
M32 = 0xFFFFFFFF


# ----------------------------------------------------------------------------------------------- BRHash (BRTypes.h)
def mix(x):
    x &= M32
    x ^= x >> 16
    x = (x * 0x7FEB352D) & M32
    x ^= x >> 15
    x = (x * 0x846CA68B) & M32
    x ^= x >> 16
    return x


def bhash(a, b, c, seed):
    h = mix(((a & M32) * 0x9E3779B1 & M32) ^ seed)
    h = mix(h ^ ((b & M32) * 0x85EBCA77 & M32))
    h = mix(h ^ ((c & M32) * 0xC2B2AE3D & M32))
    return h


def rand(a, b, c, seed):
    return F32((bhash(a, b, c, seed) & 0xFFFFFF) / 16777216.0)


def floordiv(a, b):
    return a // b  # identique a BRHash::FloorDiv pour b > 0


def posmod(a, b):
    return a % b


def round_to_int(x):
    return int(math.floor(x + 0.5))


# ----------------------------------------------------------------------------------------------- parametres
def read_level0():
    src = open(LEVELS_CPP, encoding="utf-8").read()
    body = src[src.index("FBRLevelDef Level0()"):]
    body = body[:body.index("return D;")]
    p = {}
    for name, val in re.findall(r"D\.(\w+)\s*=\s*(-?[\d.]+)f?\s*;", body):
        p[name] = float(val)
    for name, val in re.findall(r"D\.(b\w+)\s*=\s*(true|false)\s*;", body):
        p[name] = 1.0 if val == "true" else 0.0
    exits = re.findall(r"X\((\d+),\s*EBRExitStyle::(\w+),\s*([\d.]+)f?\)", body)
    p["Exits"] = [(int(t), s, float(c)) for t, s, c in exits]
    # Valeurs par defaut de FBRLevelDef (BRTypes.h) pour ce qui n'est pas redefini
    defaults = dict(ChunkCells=8, CellSize=350, WallThickness=60, WallHeight=290, SegmentLength=4, WallLineChance=0.55,
                    DoorChance=0.3, DoorWidth=140, PillarChance=0.04, OpenZoneChance=0.15, LightChance=0.45, BrokenChance=0.05,
                    FlickerChance=0.06, ShadowChance=0.25, DarkZoneChance=0.0, HidingSpotChance=0.0, BoundsChunks=0,
                    VHSRequired=6, VHSChance=0.15, PitRoomChance=0.0, PitRoomsMin=1, PitRoomCells=5, PitHoleSize=200,
                    PitPassage=150, PitHoleChance=1.0, PitDepth=1400, PitKillDepth=450, PitLipThickness=30, PitDoorsPerSide=1)
    for k, v in defaults.items():
        p.setdefault(k, v)
    for k in ("ChunkCells", "SegmentLength", "BoundsChunks", "VHSRequired", "PitRoomsMin", "PitRoomCells", "PitDoorsPerSide"):
        p[k] = int(p[k])
    return p


def read_demo_seed():
    m = re.search(r"DemoSeed\s*=\s*(\d+)", open(WORLD_H, encoding="utf-8").read())
    return int(m.group(1)) if m else None


DIRS = [(1, 0), (-1, 0), (0, 1), (0, -1)]
OPEN, WALL, DOOR = 0, 1, 2


class Level0:
    """Portage fidele de ABRWorld (grille, salles de fosses) et de ABRChunk (cachettes, sorties, cassettes, sol)"""

    def __init__(self, P, user_seed):
        self.P = P
        self.user_seed = user_seed
        self.seed = mix(user_seed ^ (0 * 7919 + 17)) or 1
        self.N = P["ChunkCells"]
        self.S = P["CellSize"]
        self.B = P["BoundsChunks"]
        self.pit_rooms = {}
        if self.has_pits():
            for x in range(-self.B, self.B):
                for y in range(-self.B, self.B):
                    r = self.compute_pit_room((x, y))
                    if r:
                        self.pit_rooms[(x, y)] = r
        self.forced = set()
        self.safe = self.repair() if self.B > 0 else self.flood(True)
        self.full = self.flood(False)

    # ---- enceinte, depart
    def chunk_of(self, c):
        return (floordiv(c[0], self.N), floordiv(c[1], self.N))

    def spawn_area(self, x, y):
        return abs(x) <= 1 and abs(y) <= 1

    def chunk_in_bounds(self, ch):
        return self.B <= 0 or (-self.B <= ch[0] < self.B and -self.B <= ch[1] < self.B)

    def cell_in_bounds(self, x, y):
        return self.B <= 0 or self.chunk_in_bounds(self.chunk_of((x, y)))

    def spawn_chunk(self, ch):
        lo, hi = self.chunk_of((-1, -1)), self.chunk_of((1, 1))
        return lo[0] <= ch[0] <= hi[0] and lo[1] <= ch[1] <= hi[1]

    def bounded_count(self, avoid_spawn):
        return sum(1 for x in range(-self.B, self.B) for y in range(-self.B, self.B) if not (avoid_spawn and self.spawn_chunk((x, y))))

    def chunk_picked(self, ch, salt, count, avoid_spawn):
        if self.B <= 0 or count <= 0 or not self.chunk_in_bounds(ch) or (avoid_spawn and self.spawn_chunk(ch)):
            return False
        mine = bhash(ch[0], ch[1], salt, self.seed)
        before = 0
        for x in range(-self.B, self.B):
            for y in range(-self.B, self.B):
                if (x, y) == ch or (avoid_spawn and self.spawn_chunk((x, y))):
                    continue
                k = bhash(x, y, salt, self.seed)
                if k < mine or (k == mine and (x < ch[0] or (x == ch[0] and y < ch[1]))):
                    before += 1
        return before < count

    # ---- salles de fosses (BRWorld.cpp, v4.6)
    def has_pits(self):
        return self.P["PitRoomChance"] > 0

    def compute_pit_room(self, ch):
        P = self.P
        if not self.has_pits() or not self.chunk_in_bounds(ch) or self.spawn_chunk(ch):
            return None
        if self.B > 0:
            count = max(P["PitRoomsMin"], round_to_int(F32(P["PitRoomChance"]) * F32(self.bounded_count(True))))
            picked = self.chunk_picked(ch, 1950, count, True)
        else:
            picked = rand(ch[0], ch[1], 1950, self.seed) < F32(P["PitRoomChance"])
        if not picked:
            return None
        K = min(max(P["PitRoomCells"], 3), self.N - 2)
        free = self.N - K - 2
        ox = 1 + (bhash(ch[0], ch[1], 1951, self.seed) % (free + 1) if free > 0 else 0)
        oy = 1 + (bhash(ch[0], ch[1], 1952, self.seed) % (free + 1) if free > 0 else 0)
        mn = (ch[0] * self.N + ox, ch[1] * self.N + oy)
        return (mn, (mn[0] + K, mn[1] + K))

    def room_of(self, x, y):
        return self.pit_rooms.get(self.chunk_of((x, y)))

    def is_pit_cell(self, x, y):
        r = self.room_of(x, y)
        return bool(r) and r[0][0] <= x < r[1][0] and r[0][1] <= y < r[1][1]

    def pit_at_corner(self, x, y):
        r = self.room_of(x, y)
        if not r or x < r[0][0] or x + 1 >= r[1][0] or y < r[0][1] or y + 1 >= r[1][1]:
            return False
        return rand(x, y, 1953, self.seed) < F32(self.P["PitHoleChance"])

    def hole_size(self):
        passage = max(120.0, self.P["PitPassage"])
        return min(max(self.P["PitHoleSize"], 40.0), self.S - passage)

    def over_pit(self, px, py, margin=0.0):
        if not self.has_pits():
            return False
        S = self.S
        half = self.hole_size() * 0.5
        gx, gy = round_to_int(px / S), round_to_int(py / S)
        r = min(half + max(0.0, margin), S * 0.5)
        if abs(px - gx * S) >= r or abs(py - gy * S) >= r:
            return False
        return self.pit_at_corner(gx - 1, gy - 1)

    def pit_edge(self, x, y, east):
        a = (x, y)
        b = (x + 1, y) if east else (x, y + 1)
        ch = self.chunk_of(a)
        if ch != self.chunk_of(b) or ch not in self.pit_rooms:
            return None
        r = self.pit_rooms[ch]
        inside = lambda c: r[0][0] <= c[0] < r[1][0] and r[0][1] <= c[1] < r[1][1]
        ia, ib = inside(a), inside(b)
        if ia == ib:
            return OPEN
        K = r[1][0] - r[0][0]
        if east:
            side, along = (0 if ia else 1), y - r[0][1]
        else:
            side, along = (2 if ia else 3), x - r[0][0]
        for d in range(max(1, self.P["PitDoorsPerSide"])):
            if bhash(r[0][0], r[0][1], 1960 + side * 8 + d, self.seed) % max(1, K) == along:
                return DOOR
        return WALL

    # ---- aretes (Rooms)
    def zone_density(self, x, y):
        r = rand(floordiv(x, 6), floordiv(y, 6), 104, self.seed)
        if r < F32(self.P["OpenZoneChance"]):
            return F32(0.25)
        if r > F32(0.85):
            return F32(1.35)
        return F32(1.0)

    def edge(self, x, y, east):
        P = self.P
        nx, ny = (x + 1, y) if east else (x, y + 1)
        if self.B > 0:
            bi = self.cell_in_bounds(x, y)
            if bi != self.cell_in_bounds(nx, ny):
                return WALL
            if not bi:
                return OPEN
        if self.spawn_area(x, y) and self.spawn_area(nx, ny):
            return OPEN
        pe = self.pit_edge(x, y, east)
        if pe is not None:
            return pe
        if (x * 2 + (1 if east else 0), y) in self.forced:
            return DOOR
        seg = max(1, P["SegmentLength"])
        if east:
            off = bhash(x, 0, 101, self.seed) % seg
            idx = floordiv(y + off, seg)
            if rand(x, idx, 102, self.seed) >= F32(P["WallLineChance"]) * self.zone_density(x, y):
                return OPEN
            return DOOR if rand(x, y, 103, self.seed) < F32(P["DoorChance"]) else WALL
        off = bhash(0, y, 111, self.seed) % seg
        idx = floordiv(x + off, seg)
        if rand(idx, y, 112, self.seed) >= F32(P["WallLineChance"]) * self.zone_density(x, y):
            return OPEN
        return DOOR if rand(x, y, 113, self.seed) < F32(P["DoorChance"]) else WALL

    def edge_e(self, x, y):
        return self.edge(x, y, True)

    def edge_n(self, x, y):
        return self.edge(x, y, False)

    def can_step(self, a, b):
        dx, dy = b[0] - a[0], b[1] - a[1]
        if (dx, dy) == (1, 0):
            return self.edge_e(a[0], a[1]) != WALL
        if (dx, dy) == (-1, 0):
            return self.edge_e(b[0], b[1]) != WALL
        if (dx, dy) == (0, 1):
            return self.edge_n(a[0], a[1]) != WALL
        if (dx, dy) == (0, -1):
            return self.edge_n(b[0], b[1]) != WALL
        return False

    def flood(self, avoid_pits):
        seen = {(0, 0)}
        q = deque([(0, 0)])
        while q:
            c = q.popleft()
            for d in DIRS:
                n = (c[0] + d[0], c[1] + d[1])
                if n in seen or not self.cell_in_bounds(*n) or not self.can_step(c, n) or (avoid_pits and self.is_pit_cell(*n)):
                    continue
                seen.add(n)
                q.append(n)
        return seen

    def repair(self):
        """ABRWorld::PrepareLevelLayout : porte percee dans le mur candidat de plus petit hachage tant qu'une zone
        hors salle de fosses reste isolee du depart"""
        safe = {(0, 0)}
        def grow(start):
            q = deque([start])
            while q:
                c = q.popleft()
                for d in DIRS:
                    n = (c[0] + d[0], c[1] + d[1])
                    if n in safe or not self.cell_in_bounds(*n) or not self.can_step(c, n) or self.is_pit_cell(*n):
                        continue
                    safe.add(n)
                    q.append(n)
        grow((0, 0))
        for _ in range(512):
            best = None
            for c in safe:
                for d in DIRS:
                    n = (c[0] + d[0], c[1] + d[1])
                    if not self.cell_in_bounds(*n) or n in safe or self.is_pit_cell(*n):
                        continue
                    east = d[1] == 0
                    a = c if (d[0] > 0 or d[1] > 0) else n
                    key = (a[0] * 2 + (1 if east else 0), a[1])
                    h = bhash(key[0], key[1], 1990, self.seed)
                    cand = (h, key[0], key[1], n)
                    if best is None or cand[:3] < best[:3]:
                        best = cand
            if best is None:
                break
            self.forced.add((best[1], best[2]))
            safe.add(best[3])
            grow(best[3])
        return safe

    def has_pillar(self, x, y):
        P = self.P
        if P["PillarChance"] <= 0:
            return False
        if self.has_pits() and (self.is_pit_cell(x, y) or self.is_pit_cell(x + 1, y) or self.is_pit_cell(x, y + 1) or self.is_pit_cell(x + 1, y + 1)):
            return False
        mult = 2.5 if self.zone_density(x, y) < 0.5 else 1.0
        return rand(x, y, 301, self.seed) < F32(P["PillarChance"]) * F32(mult)

    def cell_light(self, x, y):
        """(a une lampe, decalage x, y) ; seulement ce qui sert aux verifications (position)"""
        P = self.P
        r = self.room_of(x, y)
        if r and r[0][0] <= x < r[1][0] and r[0][1] <= y < r[1][1]:
            if posmod(x - r[0][0], 2) != 0 or posmod(y - r[0][1], 2) != 0:
                return None
            return (0.0, 0.0)
        spawn = self.spawn_area(x, y)
        dark = (not (abs(x) <= 3 and abs(y) <= 3)) and rand(floordiv(x, 5), floordiv(y, 5), 401, self.seed) < F32(P["DarkZoneChance"])
        if not spawn and dark:
            return None
        if not spawn and rand(x, y, 410, self.seed) >= F32(P["LightChance"]):
            return None
        j = self.S * 0.12
        return ((float(rand(x, y, 414, self.seed)) - 0.5) * 2 * j, (float(rand(x, y, 415, self.seed)) - 0.5) * 2 * j)

    # ---- chunk : cachettes, sorties, cassettes (BRChunk.cpp)
    def hiding_cells(self, ch):
        P, N = self.P, self.N
        out = []
        count = int(math.floor(F32(P["HidingSpotChance"]) * F32(2.0) + rand(ch[0], ch[1], 1740, self.seed)))
        for k in range(count):
            for t in range(20):
                h = bhash(ch[0], ch[1], 1742 + k * 37 + t, self.seed)
                cell = (ch[0] * N + h % N, ch[1] * N + (h >> 8) % N)
                d = DIRS[(h >> 16) % 4]
                if self.spawn_area(*cell) or cell in out or self.is_pit_cell(*cell):
                    continue
                e = self.edge_e(cell[0] if d[0] > 0 else cell[0] - 1, cell[1]) if d[0] != 0 else self.edge_n(cell[0], cell[1] if d[1] > 0 else cell[1] - 1)
                if e != WALL:
                    continue
                out.append(cell)
                break
        return out

    def pick_free_cell(self, ch, salt, hiding, full_scan):
        N = self.N
        free = lambda c: c not in hiding and not self.is_pit_cell(*c) and (self.B <= 0 or c in self.safe)
        for t in range(8):
            h = bhash(ch[0], ch[1], salt * 31 + t, self.seed)
            c = (ch[0] * N + h % N, ch[1] * N + (h >> 8) % N)
            if free(c):
                return c
        if not full_scan:
            return None
        count = N * N
        start = bhash(ch[0], ch[1], salt * 31 + 97, self.seed) % count
        for k in range(count):
            i = (start + k) % count
            c = (ch[0] * N + i % N, ch[1] * N + i // N)
            if free(c):
                return c
        return None

    def exits(self, ch, hiding):
        P, N = self.P, self.N
        out = []
        for i, (target, style, chance) in enumerate(P["Exits"]):
            if not self.chunk_picked(ch, 1100 + i, max(1, round_to_int(F32(chance) * F32(self.bounded_count(True)))), True):
                continue
            tries = 24 + N * N * 4 if self.B > 0 else 24
            scan = bhash(ch[0], ch[1], 1500 + i * 64 + 63, self.seed) % (N * N * 4)
            for t in range(tries):
                h = bhash(ch[0], ch[1], 1500 + i * 64 + t, self.seed)
                cell = (ch[0] * N + h % N, ch[1] * N + (h >> 8) % N)
                d = DIRS[(h >> 16) % 4]
                if t >= 24:
                    idx = (scan + t - 24) % (N * N * 4)
                    cell = (ch[0] * N + (idx // 4) % N, ch[1] * N + (idx // 4) // N)
                    d = DIRS[idx % 4]
                if self.spawn_area(*cell) or cell in hiding or self.is_pit_cell(*cell) or (self.B > 0 and cell not in self.safe):
                    continue
                nx, ny = cell[0] + d[0], cell[1] + d[1]
                if d == (1, 0):
                    face = self.edge_e(cell[0], cell[1]) == WALL
                elif d == (-1, 0):
                    face = self.edge_e(nx, ny) == WALL
                elif d == (0, 1):
                    face = self.edge_n(cell[0], cell[1]) == WALL
                else:
                    face = self.edge_n(nx, ny) == WALL
                if face:
                    out.append((target, style, cell, d))
                    break
        return out

    def vhs(self, ch, hiding):
        P = self.P
        if not self.chunk_picked(ch, 1008, P["VHSRequired"] + 2, True):
            return None
        return self.pick_free_cell(ch, 1008, hiding, True)

    # ---- sol du chunk de la salle (BRChunk::BuildPitRoom)
    def holes(self, room):
        S = self.S
        out = []
        for x in range(room[0][0], room[1][0] - 1):
            for y in range(room[0][1], room[1][1] - 1):
                if self.pit_at_corner(x, y):
                    out.append(((x + 1) * S, (y + 1) * S))
        return out

    def slab_rects(self, ch):
        N, S = self.N, self.S
        room = self.pit_rooms[ch]
        half = self.hole_size() * 0.5
        cut = 1.0
        wx0, wy0 = ch[0] * N * S, ch[1] * N * S
        wx1, wy1 = wx0 + N * S, wy0 + N * S
        hs = self.holes(room)
        xs, ys = [wx0, wx1], [wy0, wy1]
        for hx, hy in hs:
            xs += [hx - half - cut, hx + half + cut]
            ys += [hy - half - cut, hy + half + cut]

        def su(v):
            out = []
            for f in sorted(v):
                if not out or f - out[-1] > 0.01:
                    out.append(f)
            return out
        xs, ys = su(xs), su(ys)
        nx, ny = len(xs) - 1, len(ys) - 1
        solid = [[1] * nx for _ in range(ny)]
        for j in range(ny):
            for i in range(nx):
                cx, cy = (xs[i] + xs[i + 1]) / 2, (ys[j] + ys[j + 1]) / 2
                if any(abs(cx - hx) < half + cut and abs(cy - hy) < half + cut for hx, hy in hs):
                    solid[j][i] = 0
        used = [[0] * nx for _ in range(ny)]
        free = lambda i, j: solid[j][i] and not used[j][i]
        rects = []
        for j in range(ny):
            for i in range(nx):
                if not free(i, j):
                    continue
                i1 = i
                while i1 + 1 < nx and free(i1 + 1, j):
                    i1 += 1
                j1, grow = j, True
                while grow and j1 + 1 < ny:
                    grow = all(free(k, j1 + 1) for k in range(i, i1 + 1))
                    j1 += 1 if grow else 0
                for jj in range(j, j1 + 1):
                    for ii in range(i, i1 + 1):
                        used[jj][ii] = 1
                rects.append((xs[i], ys[j], xs[i1 + 1], ys[j1 + 1]))
        return rects, hs


# ----------------------------------------------------------------------------------------------- verifications
def check_seed(P, user_seed, verbose=False):
    L = Level0(P, user_seed)
    S, N, B = L.S, L.N, L.B
    res = {"seed": user_seed, "level_seed": L.seed, "problems": [], "rooms": [], "vhs": [], "exits": [], "hiding": []}
    prob = res["problems"]
    # 1. presence et emplacement des salles
    if L.has_pits() and len(L.pit_rooms) < P["PitRoomsMin"]:
        prob.append("moins de %d salle(s) de fosses" % P["PitRoomsMin"])
    for ch, r in sorted(L.pit_rooms.items()):
        hs = L.holes(r)
        res["rooms"].append({"chunk": ch, "min": r[0], "max": r[1], "holes": len(hs)})
        if L.spawn_chunk(ch):
            prob.append("salle de fosses dans un chunk du depart %s" % (ch,))
        if not hs:
            prob.append("salle %s sans fosse" % (ch,))
        if r[0] not in L.full:
            prob.append("salle %s inaccessible depuis le depart" % (ch,))
        doors = sum(1 for x in range(r[0][0], r[1][0]) for (a, e) in (((x, r[0][1] - 1), False), ((x, r[1][1] - 1), False)) if L.edge(a[0], a[1], e) == DOOR)
        doors += sum(1 for y in range(r[0][1], r[1][1]) for (a, e) in (((r[0][0] - 1, y), True), ((r[1][0] - 1, y), True)) if L.edge(a[0], a[1], e) == DOOR)
        res["rooms"][-1]["doors"] = doors
        if doors == 0:
            prob.append("salle %s sans porte" % (ch,))
        # collisions : aucun mur a l'interieur de la salle (les murs suivent les aretes ; seules les aretes du pourtour,
        # a 2,5 m au moins des fosses, en ont)
        inner = [(x, y, e) for x in range(r[0][0], r[1][0]) for y in range(r[0][1], r[1][1]) for e in (True, False)
                 if (x + 1 < r[1][0] if e else y + 1 < r[1][1])]
        if any(L.edge(x, y, e) != OPEN for x, y, e in inner):
            prob.append("mur ou porte a l'interieur de la salle %s" % (ch,))
        # galerie : les cellules du chunk hors salle restent reliees entre elles sans entrer dans la salle
        ring = [(x, y) for x in range(ch[0] * N, ch[0] * N + N) for y in range(ch[1] * N, ch[1] * N + N) if not L.is_pit_cell(x, y)]
        seen, q = {ring[0]}, deque([ring[0]])
        while q:
            c = q.popleft()
            for d in DIRS:
                n = (c[0] + d[0], c[1] + d[1])
                if n in ring and n not in seen and L.can_step(c, n):
                    seen.add(n)
                    q.append(n)
        if len(seen) != len(ring):
            prob.append("galerie autour de la salle %s coupee" % (ch,))
        # piliers et lampes : jamais au-dessus du vide
        for x in range(ch[0] * N - 1, ch[0] * N + N):
            for y in range(ch[1] * N - 1, ch[1] * N + N):
                if L.has_pillar(x, y) and L.over_pit((x + 1) * S, (y + 1) * S, 30):
                    prob.append("pilier au bord d'une fosse (%d,%d)" % (x, y))
                lt = L.cell_light(x, y)
                if lt and L.over_pit((x + 0.5) * S + lt[0], (y + 0.5) * S + lt[1], 60):
                    prob.append("lampe au-dessus d'une fosse (%d,%d)" % (x, y))
        # sol : les dalles couvrent exactement le chunk moins les ouvertures, sans recouvrement ni collision au-dessus du vide
        rects, hs = L.slab_rects(ch)
        half = L.hole_size() * 0.5
        area = sum((x1 - x0) * (y1 - y0) for x0, y0, x1, y1 in rects)
        expect = (N * S) ** 2 - len(hs) * (2 * half + 2) ** 2
        if abs(area - expect) > 1.0:
            prob.append("sol du chunk %s : surface %.0f au lieu de %.0f" % (ch, area, expect))
        for i, a in enumerate(rects):
            for b in rects[i + 1:]:
                if min(a[2], b[2]) - max(a[0], b[0]) > 0.01 and min(a[3], b[3]) - max(a[1], b[1]) > 0.01:
                    prob.append("dalles qui se recouvrent dans le chunk %s" % (ch,))
            for hx, hy in hs:
                if min(a[2], hx + half) - max(a[0], hx - half) > 0.01 and min(a[3], hy + half) - max(a[1], hy - half) > 0.01:
                    prob.append("dalle au-dessus d'une fosse (%.0f, %.0f)" % (hx, hy))
        # raccords : le sol du chunk touche exactement ses bords (les chunks voisins ont un sol plein du meme niveau)
        x0, y0 = ch[0] * N * S, ch[1] * N * S
        for edge_name, test in (("ouest", lambda r: r[0] == x0), ("est", lambda r: r[2] == x0 + N * S),
                                ("sud", lambda r: r[1] == y0), ("nord", lambda r: r[3] == y0 + N * S)):
            span = sum((r[3] - r[1]) if edge_name in ("ouest", "est") else (r[2] - r[0]) for r in rects if test(r))
            if abs(span - N * S) > 0.5:
                prob.append("raccord %s du chunk %s : %.0f cm couverts sur %.0f" % (edge_name, ch, span, N * S))
        res["rooms"][-1]["slabs"] = len(rects)
    # 2. depart et reapparition (places 0 a 3, ABRWorld::SpawnSpot)
    for slot in range(4):
        px, py = 0.5 * S, 0.5 * S
        if slot > 0:
            a = math.radians(90 + 60 * (slot - 1))
            px += math.cos(a) * min(85, S * 0.3)
            py += math.sin(a) * min(85, S * 0.3)
        if L.over_pit(px, py, 60) or L.is_pit_cell(int(px // S), int(py // S)):
            prob.append("point d'apparition %d au-dessus d'une fosse" % slot)
    # 3. cachettes, sorties, cassettes, chunk par chunk (comme ABRChunk::Build)
    for cx in range(-B, B):
        for cy in range(-B, B):
            ch = (cx, cy)
            hid = L.hiding_cells(ch)
            res["hiding"] += hid
            for c in hid:
                if L.is_pit_cell(*c):
                    prob.append("cachette dans une salle de fosses %s" % (c,))
            for ex in L.exits(ch, hid):
                res["exits"].append(ex)
            v = L.vhs(ch, hid)
            if v:
                res["vhs"].append(v)
    for c in res["vhs"]:
        if L.is_pit_cell(*c) or c not in L.safe:
            prob.append("cassette %s dans une salle de fosses ou inaccessible sans la traverser" % (c,))
    for t, st, c, d in res["exits"]:
        if L.is_pit_cell(*c) or c not in L.safe:
            prob.append("sortie %s %s dans une salle de fosses ou inaccessible" % (st, c))
    if len(res["vhs"]) < P["VHSRequired"]:
        prob.append("%d cassettes placees pour %d exigees" % (len(res["vhs"]), P["VHSRequired"]))
    for t, st, chance in P["Exits"]:
        if not any(e[1] == st for e in res["exits"]):
            prob.append("aucune sortie %s" % st)
    total = (2 * B * N) ** 2
    res["forced"] = len(L.forced)
    nonpit = sum(1 for x in range(-B * N, B * N) for y in range(-B * N, B * N) if not L.is_pit_cell(x, y))
    if len(L.safe) != nonpit:
        prob.append("%d cellules hors salles inaccessibles sans traverser une salle" % (nonpit - len(L.safe)))
    if len(L.full) != (2 * B * N) ** 2:
        prob.append("%d cellules inaccessibles" % ((2 * B * N) ** 2 - len(L.full)))
    res["reach_safe"] = len(L.safe)
    res["reach_full"] = len(L.full)
    res["cells"] = total
    res["L"] = L
    # chemin le plus court depart -> porte de la salle (pour choisir la graine de demonstration)
    best = None
    for ch, r in L.pit_rooms.items():
        dist = bfs_dist(L, (0, 0))
        for x in range(r[0][0], r[1][0]):
            for y in range(r[0][1], r[1][1]):
                if (x, y) in dist:
                    best = dist[(x, y)] if best is None else min(best, dist[(x, y)])
    res["room_dist"] = best
    check_navigation(L, res)
    return res


def segment_crosses_pit(L, a, b, margin):
    """ABRWorld::SegmentCrossesPit : segment (cm) contre fosses elargies de margin (methode des dalles)"""
    S = L.S
    r = min(L.hole_size() * 0.5 + max(0.0, margin), S * 0.5)
    gx0, gx1 = math.floor((min(a[0], b[0]) - r) / S), math.ceil((max(a[0], b[0]) + r) / S)
    gy0, gy1 = math.floor((min(a[1], b[1]) - r) / S), math.ceil((max(a[1], b[1]) + r) / S)
    d = (b[0] - a[0], b[1] - a[1])
    for gx in range(gx0, gx1 + 1):
        for gy in range(gy0, gy1 + 1):
            if not L.pit_at_corner(gx - 1, gy - 1):
                continue
            t0, t1, hit = 0.0, 1.0, True
            for ax in range(2):
                o, dd = a[ax], d[ax]
                lo, hi = (gx if ax == 0 else gy) * S - r, (gx if ax == 0 else gy) * S + r
                if abs(dd) < 1e-4:
                    hit = lo < o < hi
                    if not hit:
                        break
                    continue
                ta, tb = sorted(((lo - o) / dd, (hi - o) / dd))
                t0, t1 = max(t0, ta), min(t1, tb)
                hit = t0 < t1
                if not hit:
                    break
            if hit:
                return True
    return False


def find_path(L, a, b, max_nodes=1500):
    """ABRWorld::FindPath (A* 4 directions ; v4.6 : cellule de salle de fosses = cout 3)"""
    import heapq
    if a == b:
        return [b]
    h = lambda p: abs(p[0] - b[0]) + abs(p[1] - b[1])
    g, came, heap, n = {a: 0.0}, {}, [(h(a), a)], 0
    while heap and n < max_nodes:
        f, cur = heapq.heappop(heap)
        if cur == b:
            path = [b]
            while path[-1] != a:
                path.append(came[path[-1]])
            return path[::-1]
        n += 1
        for d in DIRS:
            nb = (cur[0] + d[0], cur[1] + d[1])
            if not L.cell_in_bounds(*nb) or not L.can_step(cur, nb):
                continue
            ng = g[cur] + (3.0 if L.is_pit_cell(*nb) else 1.0)
            if nb in g and g[nb] <= ng:
                continue
            g[nb] = ng
            came[nb] = cur
            heapq.heappush(heap, (ng + h(nb), nb))
    return None


def check_navigation(L, res, radius=60.0, queries=40):
    """Chemins d'entites terrestres depuis / vers / a travers la salle : aucun segment entre deux centres de cellule ne
    passe a moins de (rayon + 10 cm) d'une fosse (comme IsDirectPathClear) ; le detour par la galerie est prefere"""
    import random
    rng = random.Random(res["seed"])
    N, S = L.N, L.S
    stats = {"chemins": 0, "segments": 0, "au_dessus_du_vide": 0, "par_la_salle": 0}
    for ch, r in L.pit_rooms.items():
        room = [(x, y) for x in range(r[0][0], r[1][0]) for y in range(r[0][1], r[1][1])]
        around = [(x, y) for x in range(ch[0] * N - 4, ch[0] * N + N + 4) for y in range(ch[1] * N - 4, ch[1] * N + N + 4)
                  if L.cell_in_bounds(x, y) and not L.is_pit_cell(x, y)]
        for q in range(queries):
            kind = q % 3
            a = rng.choice(room if kind < 2 else around)
            b = rng.choice(around if kind != 1 else room)
            path = find_path(L, a, b)
            if not path:
                continue
            stats["chemins"] += 1
            stats["par_la_salle"] += 1 if kind == 2 and any(L.is_pit_cell(*c) for c in path) else 0
            for c0, c1 in zip(path, path[1:]):
                p0 = ((c0[0] + 0.5) * S, (c0[1] + 0.5) * S)
                p1 = ((c1[0] + 0.5) * S, (c1[1] + 0.5) * S)
                stats["segments"] += 1
                if segment_crosses_pit(L, p0, p1, radius + 10.0):
                    stats["au_dessus_du_vide"] += 1
    if stats["au_dessus_du_vide"]:
        res["problems"].append("%d segment(s) de chemin trop pres du vide" % stats["au_dessus_du_vide"])
    stats["marge_cm"] = (S - L.hole_size()) / 2 - radius
    res["nav"] = stats


def bfs_dist(L, start):
    dist = {start: 0}
    q = deque([start])
    while q:
        c = q.popleft()
        for d in DIRS:
            n = (c[0] + d[0], c[1] + d[1])
            if n in dist or not L.cell_in_bounds(*n) or not L.can_step(c, n):
                continue
            dist[n] = dist[c] + 1
            q.append(n)
    return dist


# ----------------------------------------------------------------------------------------------- cartes
def draw_map(res, size=8):
    from PIL import Image, ImageDraw
    L = res["L"]
    S, N, B = L.S, L.N, L.B
    n = 2 * B * N
    px = n * size * 2 + 1
    img = Image.new("RGB", (px, px + 14), (24, 22, 16))
    d = ImageDraw.Draw(img)
    def to_px(x, y):  # coordonnees cellule -> pixels (y vers le haut)
        return (int((x + B * N) * size * 2), int((B * N - y) * size * 2))
    for x in range(-B * N, B * N):
        for y in range(-B * N, B * N):
            a = to_px(x, y + 1)
            col = (148, 134, 74) if (x, y) in L.safe else ((120, 100, 60) if (x, y) in L.full else (60, 52, 36))
            if L.is_pit_cell(x, y):
                col = (176, 160, 92)
            d.rectangle([a[0], a[1], a[0] + size * 2 - 1, a[1] + size * 2 - 1], fill=col)
    # fosses
    half = L.hole_size() * 0.5 / S * size * 2
    for ch, r in L.pit_rooms.items():
        for hx, hy in L.holes(r):
            c = to_px(hx / S, hy / S)
            d.rectangle([c[0] - half, c[1] - half, c[0] + half, c[1] + half], fill=(0, 0, 0))
    # murs et portes
    for x in range(-B * N - 1, B * N):
        for y in range(-B * N - 1, B * N):
            for east in (True, False):
                e = L.edge(x, y, east)
                if e == OPEN:
                    continue
                if east:
                    a, b = to_px(x + 1, y), to_px(x + 1, y + 1)
                else:
                    a, b = to_px(x, y + 1), to_px(x + 1, y + 1)
                col = (40, 30, 20) if e == WALL else (90, 160, 210)
                d.line([a, b], fill=col, width=2 if e == WALL else 1)
    def dot(c, col, r=3):
        p = to_px(c[0] + 0.5, c[1] + 0.5)
        d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=col)
    for c in res["hiding"]:
        dot(c, (120, 80, 200), 2)
    for c in res["vhs"]:
        dot(c, (40, 200, 60))
    for t, st, c, dd in res["exits"]:
        dot(c, (220, 60, 220) if st == "Ladder" else (230, 70, 40))
    dot((0, 0), (255, 255, 255), 4)
    ok = "OK" if not res["problems"] else "%d PB" % len(res["problems"])
    d.text((3, px + 1), "BRSeed=%d  %s  salles:%d" % (res["seed"], ok, len(L.pit_rooms)), fill=(230, 230, 220))
    return img


def sheet(results, path, cols=6):
    from PIL import Image, ImageDraw
    maps = [draw_map(r) for r in results]
    w, h = maps[0].size
    rows = (len(maps) + cols - 1) // cols
    legend_h = 36
    out = Image.new("RGB", (cols * (w + 6) + 6, rows * (h + 6) + 6 + legend_h), (10, 10, 10))
    for i, m in enumerate(maps):
        out.paste(m, (6 + (i % cols) * (w + 6), 6 + (i // cols) * (h + 6)))
    d = ImageDraw.Draw(out)
    y = rows * (h + 6) + 10
    d.text((8, y), "SIMULATION PYTHON DE LA GENERATION (pas une capture du jeu) - Niveau 0, 32 x 32 cellules de 3,5 m.  "
           "Noir : fosses.  Clair : salle de fosses.  Jaune : atteignable sans traverser de salle.  Brun : atteignable par les passages.", fill=(230, 230, 220))
    d.text((8, y + 14), "Blanc : depart.  Vert : cassettes VHS.  Rouge : mur glitche (Niveau 1).  Magenta : echelle (Niveau 37).  "
           "Violet : cachettes.  Traits sombres : murs.  Traits bleus : portes.", fill=(230, 230, 220))
    out.save(path)


# ----------------------------------------------------------------------------------------------- export (rendu Blender)
def export_room(res, path):
    """Boites du chunk de la premiere salle et de ses voisins, comme ABRChunk les construit (sol, puits, murs, portes,
    piliers, plafond, lampes) : pour un rendu Blender de la geometrie generee (pas une capture Unreal)"""
    L = res["L"]
    P, S, N = L.P, L.S, L.N
    T, H = P["WallThickness"], P["WallHeight"]
    ch, room = sorted(L.pit_rooms.items())[0]
    half = L.hole_size() * 0.5
    lip = max(10.0, P["PitLipThickness"])
    depth = max(P["PitDepth"], P["PitKillDepth"] + 300)
    boxes = []

    def box(kind, c, s):
        boxes.append({"kind": kind, "c": [float(v) for v in c], "s": [float(v) for v in s]})

    chunks = [(ch[0] + dx, ch[1] + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1) if L.chunk_in_bounds((ch[0] + dx, ch[1] + dy))]
    for c in chunks:
        wx0, wy0 = c[0] * N * S, c[1] * N * S
        if c in L.pit_rooms:
            rects, hs = L.slab_rects(c)
            for x0, y0, x1, y1 in rects:
                box("floor", ((x0 + x1) / 2, (y0 + y1) / 2, -lip / 2), (x1 - x0, y1 - y0, lip))
            z1 = -lip - 150
            z2 = max(-650, -depth * 0.5)
            bands = [(-0.4, -4, "fiber"), (-4, -lip, "slabedge"), (-lip, z1, "upper"), (z1, z2, "mid"), (z2, -depth, "deep")]
            for hx, hy in hs:
                for top, bot, kind in bands:
                    zc, zs = (top + bot) / 2, top - bot
                    box(kind, (hx, hy + half + 15, zc), (2 * (half + 30), 30, zs))
                    box(kind, (hx, hy - half - 15, zc), (2 * (half + 30), 30, zs))
                    box(kind, (hx + half + 15, hy, zc), (30, 2 * half, zs))
                    box(kind, (hx - half - 15, hy, zc), (30, 2 * half, zs))
                box("bottom", (hx, hy, -depth - 10), (2 * (half + 30), 2 * (half + 30), 20))
        else:
            box("floor", (wx0 + N * S / 2, wy0 + N * S / 2, -10), (N * S, N * S, 20))
        box("ceiling", (wx0 + N * S / 2, wy0 + N * S / 2, H + 10), (N * S, N * S, 20))
        X0, Y0 = c[0] * N, c[1] * N

        def seg(along_y, fixed, a, b, zlo, zhi):
            mid = (a + b) / 2
            if along_y:
                box("wall", (fixed, mid, (zlo + zhi) / 2), (T, b - a, zhi - zlo))
            else:
                box("wall", (mid, fixed, (zlo + zhi) / 2), (b - a, T, zhi - zlo))
            if zlo < 1:
                if along_y:
                    box("trim", (fixed, mid, 6), (T + 3, b - a, 12))
                else:
                    box("trim", (mid, fixed, 6), (b - a, T + 3, 12))

        def doorway(along_y, fixed, mid):
            dw = min(P["DoorWidth"], S - 2 * T - 20)
            top = min(225, H - 25)
            seg(along_y, fixed, mid - S / 2 - T / 2, mid - dw / 2, 0, H)
            seg(along_y, fixed, mid + dw / 2, mid + S / 2 + T / 2, 0, H)
            seg(along_y, fixed, mid - dw / 2, mid + dw / 2, top, H)
            if P.get("bDoorCasings", 0):
                cw, cd = 9.0, 2.5  # chambranles (ABRChunk::AddDoorway, v4.6)
                for side in (-1, 1):
                    face = fixed + side * (T / 2 + cd / 2)
                    for j in (-1, 1):
                        along = mid + j * (dw / 2 + cw / 2)
                        box("trim", (face, along, top / 2) if along_y else (along, face, top / 2), (cd, cw, top) if along_y else (cw, cd, top))
                    box("trim", (face, mid, top + cw / 2) if along_y else (mid, face, top + cw / 2),
                        (cd, dw + 2 * cw, cw) if along_y else (dw + 2 * cw, cd, cw))
        for x in range(X0, X0 + N):
            fixed, run = (x + 1) * S, None
            for y in range(Y0, Y0 + N + 1):
                e = L.edge_e(x, y) if y < Y0 + N else OPEN
                if e == WALL:
                    run = y if run is None else run
                    continue
                if run is not None:
                    seg(True, fixed, run * S - T / 2, y * S + T / 2, 0, H)
                    run = None
                if e == DOOR:
                    doorway(True, fixed, (y + 0.5) * S)
        for y in range(Y0, Y0 + N):
            fixed, run = (y + 1) * S, None
            for x in range(X0, X0 + N + 1):
                e = L.edge_n(x, y) if x < X0 + N else OPEN
                if e == WALL:
                    run = x if run is None else run
                    continue
                if run is not None:
                    seg(False, fixed, run * S - T / 2, x * S + T / 2, 0, H)
                    run = None
                if e == DOOR:
                    doorway(False, fixed, (x + 0.5) * S)
        for x in range(X0, X0 + N):
            for y in range(Y0, Y0 + N):
                if L.has_pillar(x, y):
                    box("wall", ((x + 1) * S, (y + 1) * S, H / 2), (45, 45, H))
                lt = L.cell_light(x, y)
                if lt:
                    boxes.append({"kind": "light", "c": [(x + 0.5) * S + lt[0], (y + 0.5) * S + lt[1], H - 1.5], "s": [120, 60, 3],
                                  "room": L.is_pit_cell(x, y)})
    data = {"seed": res["seed"], "level_seed": res["level_seed"], "chunk": ch, "room": room, "cell": S, "height": H,
            "hole": L.hole_size(), "depth": depth, "boxes": boxes}
    with open(path, "w") as fh:
        json.dump(data, fh)
    print("boites exportees :", len(boxes), "->", path)


def parse_seeds(s):
    out = []
    for part in s.split(","):
        if "-" in part:
            a, b = part.split("-")
            out += list(range(int(a), int(b) + 1))
        elif part:
            out.append(int(part))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", default="1-24")
    ap.add_argument("--seed", type=int, default=None, help="une seule graine (avec --json)")
    ap.add_argument("--maps", default=None, help="planche des cartes (PNG)")
    ap.add_argument("--json", default=None, help="export des boites du chunk de la salle (rendu Blender)")
    ap.add_argument("--report", default=None, help="rapport texte (en plus de la console)")
    ap.add_argument("--pick-demo", action="store_true", help="propose une graine de demonstration (salle proche du depart)")
    a = ap.parse_args()
    P = read_level0()
    demo = read_demo_seed()
    seeds = [a.seed] if a.seed is not None else parse_seeds(a.seeds)
    if a.seed is None and demo is not None and demo not in seeds:
        seeds.append(demo)
    lines = []
    def out(t=""):
        print(t)
        lines.append(t)
    out("Verification hors moteur des salles de fosses du Niveau 0 (simulation Python de la generation C++)")
    out("Parametres lus dans BRLevels.cpp : PitRoomChance=%.3f PitRoomsMin=%d PitRoomCells=%d PitHoleSize=%.0f PitPassage=%.0f "
        "PitHoleChance=%.2f PitDepth=%.0f PitKillDepth=%.0f BoundsChunks=%d" % (P["PitRoomChance"], P["PitRoomsMin"], P["PitRoomCells"],
        P["PitHoleSize"], P["PitPassage"], P["PitHoleChance"], P["PitDepth"], P["PitKillDepth"], P["BoundsChunks"]))
    results, bad = [], 0
    out("%-8s %-11s %-6s %-26s %-6s %-5s %-6s %-9s %-11s %-7s %s" % ("BRSeed", "graine niv.", "salles", "salle (cellules)", "fosses", "portes",
        "VHS", "sorties", "accessibles", "percees", "resultat"))
    for sd in seeds:
        r = check_seed(P, sd)
        results.append(r)
        rooms = "; ".join("%d,%d->%d,%d" % (x["min"][0], x["min"][1], x["max"][0] - 1, x["max"][1] - 1) for x in r["rooms"])
        holes = ",".join(str(x["holes"]) for x in r["rooms"])
        doors = ",".join(str(x["doors"]) for x in r["rooms"])
        ok = "OK" if not r["problems"] else "PROBLEME"
        bad += 1 if r["problems"] else 0
        out("%-8d %-11d %-6d %-26s %-6s %-5s %-6d %-9d %4d/%4d   %-7d %s%s" % (sd, r["level_seed"], len(r["rooms"]), rooms, holes, doors, len(r["vhs"]),
            len(r["exits"]), r["reach_safe"], r["reach_full"], r["forced"], ok, " (demo)" if sd == demo else ""))
        for p in r["problems"]:
            out("    - " + p)
        nav = r.get("nav", {})
        out("         navigation : %d chemins (%d segments), %d au-dessus du vide, %d traversees de salle sur %d trajets autour ; marge %.0f cm "
            "(passage - rayon du Clump)" % (nav.get("chemins", 0), nav.get("segments", 0), nav.get("au_dessus_du_vide", 0),
            nav.get("par_la_salle", 0), (nav.get("chemins", 0) + 2) // 3, nav.get("marge_cm", 0)))
    out("")
    out("%d graine(s) verifiee(s), %d avec probleme(s)." % (len(results), bad))
    out("Verifie pour chaque graine : au moins %d salle(s), hors des chunks du depart ; fosses presentes ; portes ; galerie qui "
        "contourne la salle ; salle accessible ; aucun pilier ni neon au-dessus du vide ; sol du chunk = chunk moins les ouvertures "
        "(surface exacte, aucun recouvrement, aucune dalle au-dessus d'une fosse, bords du chunk entierement couverts) ; points "
        "d'apparition hors des fosses ; cachettes, cassettes et sorties hors des salles et atteignables depuis le depart SANS "
        "traverser de salle ; nombre de cassettes >= exige ; chaque sorte de sortie presente." % P["PitRoomsMin"])
    if a.pick_demo:
        cands = [r for r in results if not r["problems"] and r["room_dist"] is not None]
        cands.sort(key=lambda r: (r["room_dist"], r["seed"]))
        if cands:
            out("Graine de demonstration proposee : %d (salle a %d cellules du depart)" % (cands[0]["seed"], cands[0]["room_dist"]))
    if a.maps:
        os.makedirs(os.path.dirname(os.path.abspath(a.maps)), exist_ok=True)
        sheet(results, a.maps)
        out("Cartes : " + a.maps)
    if a.json:
        export_room(results[0], a.json)
    if a.report:
        with open(a.report, "w", encoding="utf-8") as fh:
            fh.write("\n".join(lines) + "\n")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
