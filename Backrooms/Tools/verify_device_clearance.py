"""
v4.15 : banc hors moteur des places des mecanismes de mission : le point d'interaction d'un mecanisme (centre de sa boite
visee, ABRMissionDevice::GetInteractPoint) n'est dans aucune collision que le jeu pose autour de lui, et une place debout
a portee de l'hote (340 cm des yeux) le voit (ligne de vue de ABRWorld::ServerMissionAct, qui n'ignore que le joueur et le
mecanisme).

Trois cas, chacun avec la regle d'avant (v4.14) et celle du jeu (v4.15) :
  A. Level 6 : poste de confinement et sa cage sur la meme face (groupes 10 a 13). v4.14 : poste a la place 0 (lateral 0),
     cage a la place 1 (+58 cm) : le point du poste est DANS le volume bloquant de la cage -> ligne de vue toujours coupee
     (actions refusees), but de marche de la relance (60 cm, rayon 35) hors d'atteinte. v4.15 (BRMech::Clearance::PickSlot,
     miroir ici) : la cage d'abord, le poste a cote, hors de son volume.
  B. Level 8 : rochers des parois et des cellules (ABRChunk : boite de collision = bornes de SM_Rock a l'echelle du rocher).
     Grille des grottes rejouee (BRHash, ABRWorld::IsSolid), chaque face de paroi d'une cellule libre comme place d'un treuil
     ou d'une plaque, a chaque decalage lateral du placement. v4.14 : tous les rochers ; v4.15 : pas de rocher dont la boite
     entre dans la zone libre du mecanisme (BRMech::Clearance::KeepOut, miroir ici).
  C. Level 9 : maisons (boite de collision de SM_House). v4.14 : bornes completes (jusqu'a 5,2 m, perron compris) ;
     v4.15 : corps jusqu'a GHouseBodyFront et perron bas (BRChunk.cpp). Faces des lots de maisons vues de la route.

Les constantes sont lues dans les sources (BRMechLogic.h/.cpp, BRMissionWorld.cpp, BRChunk.cpp, BRLevels.cpp, BRWorld.h,
BRCharacter.cpp) et les bornes des modeles dans RawAssets/Meshes/SM_Rock.fbx et SM_House.fbx (comme l'importeur : X = X,
Y = Z du fichier, Z = Y du fichier). Le banc verifie que l'ancienne regle ECHOUE (le defaut est reproduit) et que la regle du
jeu PASSE ; il ne remplace pas la verification dans le decor construit (-BRAutoTestV411 : "v4.15 mission N : places des
mecanismes").

Usage : python Tools/verify_device_clearance.py [--seeds 120] [--rules both|old|new]
Code de sortie : 0 si la regle v4.15 passe partout ET la regle v4.14 echoue la ou les journaux du jeu l'ont vue echouer.
"""
import argparse
import math
import os
import re
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
SRC = os.path.join(ROOT, "Source", "Backrooms")
sys.path.insert(0, HERE)
import verify_pitfalls as vp  # noqa: E402

M32 = 0xFFFFFFFF


def read(rel):
    with open(os.path.join(SRC, rel), encoding="utf-8") as f:
        return f.read()


def num(pattern, text, what):
    m = re.search(pattern, text, re.S)
    if not m:
        raise SystemExit("constante introuvable (%s) : %s" % (what, pattern))
    return [float(g) for g in m.groups()]


# ------------------------------------------------------------------------------------------------- constantes du jeu
def read_constants():
    mech_h = read("Public/BRMechLogic.h")
    mech = read("Private/BRMechLogic.cpp")
    world = read("Private/BRMissionWorld.cpp")
    chunk = read("Private/BRChunk.cpp")
    world_h = read("Public/BRWorld.h")
    char = read("Private/BRCharacter.cpp")
    K = {}
    ns = lambda name: mech_h[mech_h.index("namespace %s" % name):]
    for name, keys in (("LidarCage", ("Gap", "Depth", "HalfWidth", "Height")),
                       ("Clearance", ("PlayerRadius", "Margin", "StandDepth", "StepHeight")),
                       ("Device", ("WorkZ",))):
        body = ns(name)
        for k in keys:
            K[name + "." + k] = num(r"constexpr float %s = ([\d.]+)f;" % k, body, name + "." + k)[0]
    K["Clearance.StandGap"] = K["Clearance.PlayerRadius"] + K["Clearance.Margin"]
    Z = K["Device.WorkZ"]
    # Boites visees (BRMech::Clearance::Of) : poste, treuil, plaque murale, entailles (defaut), boitier de rue, marqueur
    K["post"] = num(r"Role == R_Beacon \|\| Role == R_LidarPost\)\s*\{\s*F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);\s*F\.AimExtent = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);", mech, "poste")
    K["winch"] = num(r"F\.AimExtent = V3\(14\.f, 16\.f, 60\.f\);\s*\}\s*else\s*\{\s*F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);\s*F\.AimExtent = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);", mech, "treuil")
    d = num(r"F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, Z\);\s*F\.AimExtent = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);\s*switch \(Kind\)", mech, "boite par defaut")
    K["default"] = [d[0], d[1], Z, d[2], d[3], d[4]]
    p = num(r"else\s*\{\s*F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, Z \+ ([\d.]+)f\);\s*F\.AimExtent = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);\s*\}\s*break;\s*\}\s*case EKind::Observe", mech, "plaque")
    K["plate"] = [p[0], p[1], Z + p[2], p[3], p[4], p[5]]
    m = num(r"Role == R_HouseMarker\)\s*\{\s*F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, Z \+ ([\d.]+)f\);\s*F\.AimExtent = V3\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f\);", mech, "marqueur")
    K["marker"] = [m[0], m[1], Z + m[2], m[3], m[4], m[5]]
    sb = num(r"const float R = Role == R_MillDial \? [\d.]+f : ([\d.]+)f;\s*F\.Aim = V3\(([\d.]+)f, ([\d.]+)f, Z \+ ([\d.]+)f\);\s*F\.AimExtent = V3\(([\d.]+)f, R \+ ([\d.]+)f, R \+ ([\d.]+)f\);", mech, "boitier")
    K["streetbox"] = [sb[1], sb[2], Z + sb[3], sb[4], sb[0] + sb[5], sb[0] + sb[6]]
    # Placement (BRMissionWorld.cpp) : ecart des places d'une face, nombre par face
    K["Spacing"] = num(r"const float Spacing = ([\d.]+)f;", world, "Spacing")[0]
    K["PerFaceMargin"] = num(r"FloorToInt\(\(S - ([\d.]+)f\) / Spacing\) \+ 1, 1, (\d+)\)", world, "PerFace")
    K["Reach"] = num(r"MissionReachServer = ([\d.]+)f;", world_h, "MissionReachServer")[0]
    K["Capsule"] = num(r"InitCapsuleSize\(([\d.]+)f, ([\d.]+)f\)", char, "capsule")
    K["EyeZ"] = num(r"constexpr float StandEyeZ = ([\d.]+)f;", char, "StandEyeZ")[0]
    # Rochers (ABRChunk::StepPlan, etape 5 ; BuildCellProps, grottes)
    wr = num(r"BRHash::Rand\(X \* 4 \+ k, Y, 931, Seed\) < ([\d.]+)f\)\s*\{\s*const float Sc = ([\d.]+)f \+ ([\d.]+)f \* BRHash::Rand\(X, Y \* 4 \+ k, 932, Seed\);"
             r".*?\* S \* ([\d.]+)f\) - FVector\(Dir\.X, Dir\.Y, 0\.f\) \* ([\d.]+)f;.*?FVector\(Sc, Sc, Sc \* ([\d.]+)f\)", chunk, "rocher de paroi")
    K["wall_rock"] = dict(chance=wr[0], sc0=wr[1], sc1=wr[2], spread=wr[3], inset=wr[4], zs=wr[5])
    cr = num(r"R\(930\) < D\.PropDensity \* ([\d.]+)f\)\s*\{\s*const float Sc = ([\d.]+)f \+ R\(931\) \* ([\d.]+)f;\s*const FVector P = C \+ FVector\(\(R\(932\) - 0\.5f\) \* S \* ([\d.]+)f, "
             r"\(R\(933\) - 0\.5f\) \* S \* [\d.]+f, (-?[\d.]+)f\);", chunk, "rocher de cellule")
    K["cell_rock"] = dict(density=cr[0], sc0=cr[1], sc1=cr[2], spread=cr[3], z=cr[4])
    K["rock_keepout_in_game"] = "RockInMissionKeepOut(this, InWorld, RockT)" in chunk and "RockInMissionKeepOut(this, W, RockT)" in chunk
    # Maison (BRChunk.cpp)
    hb = re.search(r"constexpr float GHouseBodyFront = ([\d.]+)f;\s*constexpr float GHousePorchHalfY = ([\d.]+)f;\s*constexpr float GHousePorchTop = ([\d.]+)f;", chunk)
    K["house_body"] = [float(g) for g in hb.groups()] if hb else None
    # Niveaux (BRLevels.cpp)
    levels = open(vp.LEVELS_CPP, encoding="utf-8").read()
    for n in (6, 8, 9):
        body = levels[levels.index("FBRLevelDef Level%d()" % n):]
        body = body[:body.index("return D;")]
        vals = dict((k, float(v)) for k, v in re.findall(r"D\.(\w+)\s*=\s*(-?[\d.]+)f?\s*;", body))
        K["L%d" % n] = vals
    K["WallThicknessDefault"] = num(r"float WallThickness = ([\d.]+)f;", read("Public/BRTypes.h"), "WallThickness")[0]
    return K


# ------------------------------------------------------------------------------------------- bornes des modeles (FBX)
def _props(d, o, n):
    out = []
    for _ in range(n):
        t = chr(d[o])
        o += 1
        if t == "Y":
            out.append(struct.unpack_from("<h", d, o)[0])
            o += 2
        elif t == "C":
            out.append(d[o] != 0)
            o += 1
        elif t in "IF":
            out.append(struct.unpack_from("<" + ("i" if t == "I" else "f"), d, o)[0])
            o += 4
        elif t in "DL":
            out.append(struct.unpack_from("<" + ("d" if t == "D" else "q"), d, o)[0])
            o += 8
        elif t in "fdlib":
            ln, enc, cl = struct.unpack_from("<III", d, o)
            o += 12
            raw = d[o:o + cl]
            o += cl
            if enc == 1:
                raw = zlib.decompress(raw)
            fmt = {"f": "f", "d": "d", "l": "q", "i": "i", "b": "B"}[t]
            out.append(list(struct.unpack("<%d%s" % (ln, fmt), raw[:ln * struct.calcsize(fmt)])))
        elif t in "SR":
            ln = struct.unpack_from("<I", d, o)[0]
            o += 4
            out.append(d[o:o + ln])
            o += ln
        else:
            raise ValueError("FBX : type %r" % t)
    return out, o


def _nodes(d, o, end, ver):
    nodes = []
    while o < end:
        if ver >= 7500:
            eo, np_, pl = struct.unpack_from("<QQQ", d, o)
            o += 24
        else:
            eo, np_, pl = struct.unpack_from("<III", d, o)
            o += 12
        nl = d[o]
        o += 1
        if eo == 0:
            break
        name = d[o:o + nl].decode("latin-1")
        o += nl
        props, _ = _props(d, o, np_)
        o2 = o + pl
        nodes.append((name, props, _nodes(d, o2, eo, ver) if o2 < eo else []))
        o = eo
    return nodes


def fbx_bounds(path):
    """Bornes des sommets dans Unreal (X = X, Y = Z, Z = Y du fichier ; fichier Y haut, unite 1 cm, noeud identite)"""
    d = open(path, "rb").read()
    assert d[:20] == b"Kaydara FBX Binary  ", path
    ver = struct.unpack_from("<I", d, 23)[0]
    nodes = _nodes(d, 27, len(d), ver)
    objs = next(n for n in nodes if n[0] == "Objects")[2]
    xs, ys, zs = [], [], []
    for g in objs:
        if g[0] != "Geometry":
            continue
        for c in g[2]:
            if c[0] == "Vertices":
                v = c[1][0]
                xs += v[0::3]
                ys += v[1::3]
                zs += v[2::3]
    return (min(xs), min(zs), min(ys)), (max(xs), max(zs), max(ys))


# ------------------------------------------------------------------------------------------------------- geometrie
def rot(yaw):
    r = math.radians(yaw)
    return math.cos(r), math.sin(r)


def to_world(fr, lx, ly):
    """FFrame::ToWorld (lacet arrondi au quart de tour) : Y local = (-FY, FX)"""
    fx, fy = rot(fr[3])
    fx, fy = round(fx), round(fy)
    return fr[0] + fx * lx - fy * ly, fr[1] + fy * lx + fx * ly


def to_local(fr, wx, wy):
    fx, fy = rot(fr[3])
    fx, fy = round(fx), round(fy)
    dx, dy = wx - fr[0], wy - fr[1]
    return dx * fx + dy * fy, -dx * fy + dy * fx


def rect_overlap(a, b):
    """BRMech::Clearance::Overlap : rectangles orientes (cx, cy, hx, hy, yaw), separation des axes"""
    axes = []
    for r in (a, b):
        c, s = rot(r[4])
        axes.append(((c, s), (-s, c)))
    dx, dy = b[0] - a[0], b[1] - a[1]
    for n in (axes[0][0], axes[0][1], axes[1][0], axes[1][1]):
        dist = abs(dx * n[0] + dy * n[1])
        pa = a[2] * abs(axes[0][0][0] * n[0] + axes[0][0][1] * n[1]) + a[3] * abs(axes[0][1][0] * n[0] + axes[0][1][1] * n[1])
        pb = b[2] * abs(axes[1][0][0] * n[0] + axes[1][0][1] * n[1]) + b[3] * abs(axes[1][1][0] * n[0] + axes[1][1][1] * n[1])
        if dist > pa + pb:
            return False
    return True


def rect_of(fr, x0, y0, x1, y1):
    cx, cy = to_world(fr, (x0 + x1) * 0.5, (y0 + y1) * 0.5)
    return (cx, cy, abs(x1 - x0) * 0.5, abs(y1 - y0) * 0.5, round(fr[3] / 90.0) * 90.0)


def keep_out(K, fr, aim):
    """BRMech::Clearance::KeepOut (mecanisme sans volume)"""
    hy = abs(aim[1]) + aim[4] + K["Clearance.StandGap"]
    return rect_of(fr, -12.0, -hy, aim[0] + max(aim[3], K["Clearance.StandGap"]) + K["Clearance.StandDepth"], hy)


class OBox:
    """Boite orientee (lacet quelconque) : centre, demi-tailles, lacet, hauteur"""

    def __init__(self, cx, cy, hx, hy, yaw, z0, z1):
        self.cx, self.cy, self.hx, self.hy, self.yaw, self.z0, self.z1 = cx, cy, hx, hy, yaw, z0, z1
        self.c, self.s = rot(yaw)

    def local(self, x, y):
        dx, dy = x - self.cx, y - self.cy
        return dx * self.c + dy * self.s, -dx * self.s + dy * self.c

    def contains(self, p, grow=0.0):
        lx, ly = self.local(p[0], p[1])
        return abs(lx) <= self.hx + grow and abs(ly) <= self.hy + grow and self.z0 - grow <= p[2] <= self.z1 + grow

    def dist2d(self, x, y):
        lx, ly = self.local(x, y)
        ex, ey = max(abs(lx) - self.hx, 0.0), max(abs(ly) - self.hy, 0.0)
        return math.hypot(ex, ey)

    def hits(self, a, b):
        """Segment a -> b (3D) contre la boite (slabs dans le repere de la boite)"""
        la = self.local(a[0], a[1]) + (a[2],)
        lb = self.local(b[0], b[1]) + (b[2],)
        lo = (-self.hx, -self.hy, self.z0)
        hi = (self.hx, self.hy, self.z1)
        t0, t1 = 0.0, 1.0
        for i in range(3):
            dd = lb[i] - la[i]
            if abs(dd) < 1e-9:
                if la[i] < lo[i] or la[i] > hi[i]:
                    return False
                continue
            u0, u1 = (lo[i] - la[i]) / dd, (hi[i] - la[i]) / dd
            if u0 > u1:
                u0, u1 = u1, u0
            t0, t1 = max(t0, u0), min(t1, u1)
            if t0 > t1:
                return False
        return True

    def rect(self):
        return (self.cx, self.cy, self.hx, self.hy, self.yaw)


def prop_box(bounds, px, py, pz, yaw, sx, sy, sz):
    """Boite de collision de ABRChunk::AddProp : bornes du modele a l'echelle, tournees, centre transforme"""
    (x0, y0, z0), (x1, y1, z1) = bounds
    c, s = rot(yaw)
    bcx, bcy = (x0 + x1) * 0.5 * sx, (y0 + y1) * 0.5 * sy
    return OBox(px + bcx * c - bcy * s, py + bcx * s + bcy * c, (x1 - x0) * 0.5 * sx, (y1 - y0) * 0.5 * sy, yaw, pz + z0 * sz, pz + z1 * sz)


def laterals(K, S):
    per = int(max(1, min(int(K["PerFaceMargin"][1]), math.floor((S - K["PerFaceMargin"][0]) / K["Spacing"]) + 1)))
    out = []
    for k in range(per):
        step = (k + 1) // 2
        out.append((1.0 if k % 2 == 1 else -1.0) * step * K["Spacing"])
    return out


def face_frame(S, c, d, lat, inset):
    """FaceSpot : cellule c, face d, decalage lateral (Side = (-d.y, d.x)), mecanisme tourne vers la cellule"""
    cx, cy = (c[0] + 0.5) * S, (c[1] + 0.5) * S
    sx, sy = -d[1], d[0]
    yaw = math.degrees(math.atan2(-d[1], -d[0]))
    return (cx + d[0] * (S * 0.5 - inset) + sx * lat, cy + d[1] * (S * 0.5 - inset) + sy * lat, 0.0, yaw)


def aim_point(fr, aim):
    x, y = to_world(fr, aim[0], aim[1])
    return (x, y, fr[2] + aim[2])


def forward(fr):
    c, s = rot(fr[3])
    return round(c), round(s)


def stand_near(K, fr, aim, boxes, solid_cells=None, S=0.0):
    """Miroir de BRTestV411::StandNear avec la regle de l'hote : places a 120, 170, 230 et 300 cm du point vise dans huit
    directions (d'abord devant), capsule de 36 cm libre (boites plus hautes qu'une marche, cellules pleines), yeux du
    joueur debout a portee (340 cm) et ligne de vue WorldStatic libre (boites, cellules pleines)"""
    ip = aim_point(fr, aim)
    f = forward(fr)
    eye_z = fr[2] + K["Capsule"][1] + K["EyeZ"]

    def near_solid(x, y, r):
        if not solid_cells:
            return False
        cx, cy = math.floor(x / S), math.floor(y / S)
        for X in range(cx - 1, cx + 2):
            for Y in range(cy - 1, cy + 2):
                if solid_cells(X, Y):
                    dx = max(X * S - x, 0.0, x - (X + 1) * S)
                    dy = max(Y * S - y, 0.0, y - (Y + 1) * S)
                    if math.hypot(dx, dy) < r:
                        return True
        return False

    def los_solid(a, b):
        if not solid_cells:
            return False
        n = max(2, int(math.dist(a, b) / 10.0))
        for i in range(1, n):
            t = i / n
            x, y = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
            if solid_cells(math.floor(x / S), math.floor(y / S)):
                return True
        return False

    for dist in (120.0, 170.0, 230.0, 300.0):
        for turn in range(8):
            ang = math.radians(turn * 45.0)
            dx, dy = f[0] * math.cos(ang) - f[1] * math.sin(ang), f[0] * math.sin(ang) + f[1] * math.cos(ang)
            sx, sy = ip[0] + dx * dist, ip[1] + dy * dist
            if near_solid(sx, sy, 36.0):
                continue
            if any(b.z1 - b.z0 > K["Clearance.StepHeight"] and b.dist2d(sx, sy) < 36.0 for b in boxes):
                continue
            eye = (sx, sy, eye_z)
            if math.dist(eye, ip) > K["Reach"]:
                continue
            if any(b.hits(eye, ip) for b in boxes) or los_solid(eye, ip):
                continue
            return True
    return False


# ------------------------------------------------------------------------------------------------ A. Level 6
def pick_slot_mirror(K, prints, frames_lat, face_half):
    """BRMech::Clearance::PickSlot (miroir) pour un groupe : les membres a volume d'abord, chacun a la premiere place libre
    sans gene avec ceux deja poses ; un volume mural tient dans la face"""
    order = sorted(range(len(prints)), key=lambda i: 0 if prints[i]["solid"] else 1)
    taken = [False] * len(frames_lat)
    placed = []
    out = [None] * len(prints)
    for i in order:
        pr = prints[i]
        for j, (fr, lat) in enumerate(frames_lat):
            if taken[j]:
                continue
            if pr["solid"] and abs(lat) + max(abs(pr["solid"][1]), abs(pr["solid"][4])) > face_half + 0.01:
                continue
            if any(clash(K, (fr, pr), q) for q in placed):
                continue
            taken[j] = True
            placed.append((fr, pr))
            out[i] = j
            break
    return out


def solid_box(fr, sol):
    x0, y0, z0, x1, y1, z1 = sol
    r = rect_of(fr, x0, y0, x1, y1)
    return OBox(r[0], r[1], r[2], r[3], r[4], fr[2] + z0, fr[2] + z1)


def hinders(K, me, other):
    fr, pr = me
    ofr, opr = other
    if not opr["solid"]:
        return False
    box = solid_box(ofr, opr["solid"])
    ip = aim_point(fr, pr["aim"])
    if box.contains(ip, K["Clearance.Margin"]):
        return True
    if opr["solid"][5] - opr["solid"][2] <= K["Clearance.StepHeight"]:
        return False
    sx, sy = to_world(fr, pr["aim"][0] + K["Clearance.StandGap"], pr["aim"][1])
    return box.dist2d(sx, sy) < K["Clearance.PlayerRadius"]


def clash(K, a, b):
    if hinders(K, a, b) or hinders(K, b, a):
        return True
    if a[1]["solid"] and b[1]["solid"]:
        return rect_overlap(solid_box(a[0], a[1]["solid"]).rect(), solid_box(b[0], b[1]["solid"]).rect())
    return False


def case_level6(K, rules):
    S = K["L6"].get("CellSize", 350.0)
    T = K["L6"].get("WallThickness", K["WallThicknessDefault"])
    face_half = S * 0.5 - T * 0.5
    lats = laterals(K, S)
    R = K["Capsule"][0]
    post = {"aim": K["post"], "solid": None}
    cage = {"aim": None, "solid": (K["LidarCage.Gap"], -K["LidarCage.HalfWidth"], 0.0, K["LidarCage.Gap"] + K["LidarCage.Depth"], K["LidarCage.HalfWidth"], K["LidarCage.Height"])}
    cage["aim"] = [K["LidarCage.Gap"] + K["LidarCage.Depth"] + 4.0, 0.0, 120.0, 8.0, K["LidarCage.HalfWidth"], 90.0]
    # Face nord d'une cellule centree en (0, 0) : d = (0, 1), mur bord a bord (Inset = epaisseur / 2)
    cell = (0, 0)
    d = (0, 1)
    frames = [(face_frame(S, cell, d, lat, T * 0.5), lat) for lat in lats]
    print("A. Level 6 : poste et cage d'un groupe (cellule de %g cm, murs de %g, face utile +-%g ; places %s ; cage %g..%g cm du mur, +-%g)"
          % (S, T, face_half, "/".join("%+g" % v for v in lats), K["LidarCage.Gap"], K["LidarCage.Gap"] + K["LidarCage.Depth"], K["LidarCage.HalfWidth"]))
    results = {}
    for name in rules:
        if name == "old":
            slots = [0, 1]  # membres dans l'ordre du plan (poste 1+K puis cage 5+K), places 0 puis 1
        else:
            slots = pick_slot_mirror(K, [post, cage], frames, face_half)
        if None in slots:
            print("  %s : aucune place (placement impossible)" % name)
            results[name] = False
            continue
        pf, plat = frames[slots[0]]
        cf, clat = frames[slots[1]]
        cbox = solid_box(cf, cage["solid"])
        ip = aim_point(pf, post["aim"])
        inside = cbox.contains(ip)
        # Murs de la cellule (face nord et murs lateraux aux bouts de la face) : distance d'un point au mur
        cx0, cy0 = (cell[0] + 0.5) * S, (cell[1] + 0.5) * S
        walls = [lambda x, y: (cy0 + face_half) - y, lambda x, y: x - (cx0 - face_half), lambda x, y: (cx0 + face_half) - x]
        free = []
        x = cx0 - face_half + R
        while x <= cx0 + face_half - R:
            y = cy0 + face_half - R
            while y >= cy0 + face_half - 300.0:
                if all(w(x, y) >= R for w in walls) and cbox.dist2d(x, y) >= R:
                    free.append((x, y))
                y -= 5.0
            x += 5.0
        eye_z = K["Capsule"][1] + K["EyeZ"]
        reach = [p for p in free if math.dist((p[0], p[1], eye_z), ip) <= K["Reach"]]
        los = [p for p in reach if not cbox.hits((p[0], p[1], eye_z), ip)]
        f = forward(pf)
        g1 = (ip[0] + f[0] * 110.0, ip[1] + f[1] * 110.0)
        g2 = (ip[0] + f[0] * 60.0, ip[1] + f[1] * 60.0)
        m1 = min(math.hypot(p[0] - g1[0], p[1] - g1[1]) for p in free)
        m2 = min(math.hypot(p[0] - g2[0], p[1] - g2[1]) for p in free)
        fits = abs(clat) + K["LidarCage.HalfWidth"] <= face_half
        ok = not inside and len(los) > 0 and m1 <= 70.0 and m2 <= 35.0 and fits
        results[name] = ok
        print("  regle %s : poste a %+g, cage a %+g ; point du poste dans la cage : %s ; places a portee %d, ligne de vue de l'hote libre %d ; "
              "but de marche 110 cm (rayon 70) a %.0f cm, relance 60 cm (rayon 35) a %.0f cm ; cage dans la face : %s -> %s"
              % ("v4.14" if name == "old" else "v4.15", plat, clat, "OUI" if inside else "non", len(reach), len(los), m1, m2, "oui" if fits else "NON",
                 "PASSE" if ok else "ECHOUE"))
    return results


# ------------------------------------------------------------------------------------------------ B. Level 8
class Caves:
    def __init__(self, K, seed):
        L = K["L8"]
        self.S = L["CellSize"]
        self.solid_chance = vp.F32(L["SolidChance"])
        self.density = L.get("PropDensity", 0.0)
        self.seed = seed
        self._solid = {}

    def solid(self, x, y):
        k = (x, y)
        v = self._solid.get(k)
        if v is None:
            if abs(x) <= 1 and abs(y) <= 1:
                v = False
            else:
                r = vp.F32(0.55) * vp.rand(x, y, 601, self.seed) + vp.F32(0.45) * vp.rand(vp.floordiv(x, 3), vp.floordiv(y, 3), 602, self.seed)
                v = bool(r < self.solid_chance)
            self._solid[k] = v
        return v


def rocks_near(K, W, c, bounds, radius=3):
    """Rochers que ABRChunk pose autour de la cellule c : parois des cellules pleines (etape 5) et centre des cellules
    libres voisines (BuildCellProps ; pas dans la cellule du mecanisme, reservee a la mission)"""
    S = W.S
    wr, cr = K["wall_rock"], K["cell_rock"]
    out = []
    for X in range(c[0] - radius, c[0] + radius + 1):
        for Y in range(c[1] - radius, c[1] + radius + 1):
            if W.solid(X, Y):
                for k, d in enumerate(vp.DIRS):
                    if W.solid(X + d[0], Y + d[1]):
                        continue
                    if not vp.rand(X * 4 + k, Y, 931, W.seed) < vp.F32(wr["chance"]):
                        continue
                    sc = wr["sc0"] + wr["sc1"] * float(vp.rand(X, Y * 4 + k, 932, W.seed))
                    fx, fy = (X + 0.5) * S + d[0] * S * 0.5, (Y + 0.5) * S + d[1] * S * 0.5
                    off = (float(vp.rand(X, Y, 933 + k, W.seed)) - 0.5) * S * wr["spread"]
                    px, py = fx - d[1] * off - d[0] * wr["inset"], fy + d[0] * off - d[1] * wr["inset"]
                    yaw = float(vp.rand(X, Y, 937 + k, W.seed)) * 360.0
                    out.append(prop_box(bounds, px, py, 0.0, yaw, sc, sc, sc * wr["zs"]))
            elif (X, Y) != c and not (abs(X) <= 1 and abs(Y) <= 1) and abs(X - c[0]) <= 2 and abs(Y - c[1]) <= 2:
                R = lambda salt: float(vp.rand(X, Y, salt, W.seed))
                if R(930) < W.density * cr["density"]:
                    sc = cr["sc0"] + R(931) * cr["sc1"]
                    px = (X + 0.5) * S + (R(932) - 0.5) * S * cr["spread"]
                    py = (Y + 0.5) * S + (R(933) - 0.5) * S * cr["spread"]
                    out.append(prop_box(bounds, px, py, cr["z"], R(934) * 360.0, sc, sc, sc))
    return out


def case_level8(K, rules, bounds, seeds):
    S = K["L8"]["CellSize"]
    lats = laterals(K, S)
    kinds = (("treuil", K["winch"]), ("entailles", K["default"]))
    print("B. Level 8 : rochers (SM_Rock %s..%s cm ; paroi : %.0f %% des faces, echelle %.1f a %.1f, z x%.1f ; cellule : densite %.2f x %.1f, echelle %.2f a %.2f) ; "
          "%d graines, places laterales %s"
          % (tuple(round(v, 1) for v in bounds[0]), tuple(round(v, 1) for v in bounds[1]), 100 * K["wall_rock"]["chance"], K["wall_rock"]["sc0"],
             K["wall_rock"]["sc0"] + K["wall_rock"]["sc1"], K["wall_rock"]["zs"], K["L8"].get("PropDensity", 0), K["cell_rock"]["density"],
             K["cell_rock"]["sc0"], K["cell_rock"]["sc0"] + K["cell_rock"]["sc1"], seeds, "/".join("%+g" % v for v in lats)))
    stats = {r: {k: [0, 0, 0] for k, _ in kinds} for r in rules}  # places, point dans un rocher, sans place debout
    # Place 6 d'une face de 400 cm (+174 cm, a 26 cm du coin) : jamais prise au Level 8 (un seul mecanisme de groupe, a la
    # premiere place libre de la salle ; les mecanismes seuls sont a 0) : comptee a part, pour information
    corner = {r: [0, 0, 0] for r in rules}
    removed = [0, 0]
    for si in range(seeds):
        seed = vp.mix((si + 1) * 2654435761 & M32) or 1
        W = Caves(K, seed)
        for cxi in range(-6, 7, 3):
            for cyi in range(-6, 7, 3):
                c = (cxi, cyi)
                if W.solid(*c) or (abs(c[0]) <= 1 and abs(c[1]) <= 1):
                    continue
                rocks = rocks_near(K, W, c, bounds)
                for d in vp.DIRS:
                    if not W.solid(c[0] + d[0], c[1] + d[1]):
                        continue
                    for lat in lats:
                        fr = face_frame(S, c, d, lat, 0.0)
                        for kname, aim in kinds:
                            ko = keep_out(K, fr, aim)
                            for rname in rules:
                                kept = rocks if rname == "old" else [b for b in rocks if not rect_overlap(ko, b.rect())]
                                if rname == "new" and kname == "treuil":
                                    removed[0] += len(rocks) - len(kept)
                                    removed[1] += len(rocks)
                                ip = aim_point(fr, aim)
                                st = stats[rname][kname] if abs(lat) < S * 0.5 - 40.0 else corner[rname]
                                st[0] += 1
                                buried = any(b.contains(ip) for b in kept)
                                st[1] += 1 if buried else 0
                                st[2] += 0 if (not buried and stand_near(K, fr, aim, kept, W.solid, S)) else 1
    results = {}
    for rname in rules:
        ok = all(st[1] == 0 and st[2] == 0 for st in stats[rname].values())
        results[rname] = ok
        parts = ["%s : %d places, point dans un rocher %d (%.1f %%), sans place debout a portee et a vue %d (%.1f %%)"
                 % (k, st[0], st[1], 100.0 * st[1] / max(1, st[0]), st[2], 100.0 * st[2] / max(1, st[0])) for k, st in stats[rname].items()]
        print("  regle %s : %s -> %s" % ("v4.14" if rname == "old" else "v4.15", " ; ".join(parts), "PASSE" if ok else "ECHOUE"))
        c = corner[rname]
        print("    (pour information, place au coin %+g : %d cas, point dans un rocher %d, sans place debout %d)" % (lats[-1], c[0], c[1], c[2]))
    if "new" in rules:
        print("  regle v4.15 : rochers retires autour d'un treuil (zone libre) : %d sur %d voisins (%.1f %%) ; dans le jeu : %s"
              % (removed[0], removed[1], 100.0 * removed[0] / max(1, removed[1]), "oui (RockInMissionKeepOut)" if K["rock_keepout_in_game"] else "NON"))
    return results


# ------------------------------------------------------------------------------------------------ C. Level 9
def case_level9(K, rules, bounds, seeds):
    L = K["L9"]
    S, sp = L["CellSize"], int(L["Spacing"])
    lats = laterals(K, S)
    kinds = (("plan de maison", K["plate"]), ("boitier de rue", K["streetbox"]), ("plaque de facade", K["marker"]))
    body = K["house_body"]
    print("C. Level 9 : maisons (SM_House %s..%s cm ; v4.15 : corps jusqu'a %s cm, perron +-%s cm, %s cm de haut) ; lots de %g cm ; %d graines"
          % (tuple(round(v, 1) for v in bounds[0]), tuple(round(v, 1) for v in bounds[1]), body[0] if body else "?", body[1] if body else "?", body[2] if body else "?",
             2 * S, seeds))
    (x0, y0, z0), (x1, y1, z1) = bounds

    def house_boxes(X, Y, yaw, rname):
        hx, hy = (X + 1) * S, (Y + 1) * S
        if rname == "old" or not body or x1 <= body[0]:
            return [prop_box(((x0, y0, z0), (x1, y1, z1)), hx, hy, 0.0, yaw, 1, 1, 1)]
        return [prop_box(((x0, y0, z0), (body[0], y1, z1)), hx, hy, 0.0, yaw, 1, 1, 1),
                prop_box(((body[0], -body[1], z0), (x1, body[1], body[2])), hx, hy, 0.0, yaw, 1, 1, 1)]

    stats = {r: {k: [0, 0, 0] for k, _ in kinds} for r in rules}
    for si in range(seeds):
        seed = vp.mix((si + 1) * 2246822519 & M32) or 1
        has = lambda lx, ly: bool(vp.rand(lx, ly, 801, seed) < vp.F32(0.85))

        def lot_solid(x, y):
            if vp.posmod(x, sp) == 0 or vp.posmod(y, sp) == 0:
                return False
            return has(vp.floordiv(x, sp) * 2 + (vp.posmod(x, sp) - 1) // 2, vp.floordiv(y, sp) * 2 + (vp.posmod(y, sp) - 1) // 2)

        houses = {}
        for X in range(-12, 13):
            for Y in range(-12, 13):
                LX, LY = vp.posmod(X, sp), vp.posmod(Y, sp)
                if LX in (1, 3) and LY in (1, 3):
                    lot = (vp.floordiv(X, sp) * 2 + (LX - 1) // 2, vp.floordiv(Y, sp) * 2 + (LY - 1) // 2)
                    if has(*lot):
                        houses[(X, Y)] = 180.0 if LX == 1 else 0.0
        for X in range(-9, 10):
            for Y in range(-9, 10):
                c = (X, Y)
                if lot_solid(*c):
                    continue
                for d in vp.DIRS:
                    if not lot_solid(c[0] + d[0], c[1] + d[1]):
                        continue
                    for lat in lats:
                        fr = face_frame(S, c, d, lat, 0.0)
                        for kname, aim in kinds:
                            ip = aim_point(fr, aim)
                            for rname in rules:
                                boxes = []
                                for (hxc, hyc), yaw in houses.items():
                                    if abs((hxc + 1) * S - ip[0]) < 1200 and abs((hyc + 1) * S - ip[1]) < 1200:
                                        boxes += house_boxes(hxc, hyc, yaw, rname)
                                st = stats[rname][kname]
                                st[0] += 1
                                buried = any(b.contains(ip) for b in boxes)
                                st[1] += 1 if buried else 0
                                st[2] += 0 if (not buried and stand_near(K, fr, aim, boxes)) else 1
    results = {}
    for rname in rules:
        ok = all(st[1] == 0 and st[2] == 0 for st in stats[rname].values())
        results[rname] = ok
        parts = ["%s : %d places, point dans la maison %d (%.1f %%), sans place debout a portee et a vue %d" % (k, st[0], st[1], 100.0 * st[1] / max(1, st[0]), st[2])
                 for k, st in stats[rname].items()]
        print("  regle %s : %s -> %s" % ("v4.14" if rname == "old" else "v4.15", " ; ".join(parts), "PASSE" if ok else "ECHOUE"))
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=120, help="graines par niveau (grottes, banlieue)")
    ap.add_argument("--rules", default="both", choices=("both", "old", "new"))
    a = ap.parse_args()
    K = read_constants()
    rock = fbx_bounds(os.path.join(ROOT, "RawAssets", "Meshes", "SM_Rock.fbx"))
    house = fbx_bounds(os.path.join(ROOT, "RawAssets", "Meshes", "SM_House.fbx"))
    rules = ("old", "new") if a.rules == "both" else (a.rules,)
    print("Banc des places des mecanismes (v4.15, Tools/verify_device_clearance.py)")
    print("Constantes lues : capsule %g x %g, yeux a %g cm au-dessus du centre, portee de l'hote %g cm ; marge %g, place debout a %g cm, profondeur libre %g ; "
          "poste %s ; treuil %s ; plaque %s"
          % (K["Capsule"][0], K["Capsule"][1], K["EyeZ"], K["Reach"], K["Clearance.Margin"], K["Clearance.StandGap"], K["Clearance.StandDepth"],
             tuple(K["post"][:3]), tuple(K["winch"][:3]), tuple(K["plate"][:3])))
    if K["Capsule"][0] != K["Clearance.PlayerRadius"]:
        print("ECHEC : BRMech::Clearance::PlayerRadius (%g) differe de la capsule de ABRCharacter (%g)" % (K["Clearance.PlayerRadius"], K["Capsule"][0]))
        return 1
    print()
    res = {"A": case_level6(K, rules)}
    print()
    res["B"] = case_level8(K, rules, rock, a.seeds)
    print()
    res["C"] = case_level9(K, rules, house, a.seeds)
    print()
    bad = 0
    for case, r in res.items():
        if "old" in r and r["old"]:
            print("ECHEC : cas %s : l'ancienne regle passe (le defaut n'est plus reproduit par le banc)" % case)
            bad += 1
        if "new" in r and not r["new"]:
            print("ECHEC : cas %s : la regle du jeu echoue" % case)
            bad += 1
    if "new" in rules and not K["rock_keepout_in_game"]:
        print("ECHEC : BRChunk.cpp ne retire pas les rochers de la zone libre des mecanismes")
        bad += 1
    if "new" in rules and not K["house_body"]:
        print("ECHEC : BRChunk.cpp n'a pas la boite du corps de la maison (GHouseBodyFront)")
        bad += 1
    word = lambda r, k: ("passe" if r[k] else "echoue") if k in r else "non jouee"
    print("RESULTAT : %s (%s)" % ("OK" if bad == 0 else "ECHEC", ", ".join("%s v4.14 %s / v4.15 %s" % (c, word(r, "old"), word(r, "new")) for c, r in res.items())))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
