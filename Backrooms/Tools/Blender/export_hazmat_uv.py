"""
v4.17 (combinaison) : etape 0 des cartes derivees de la combinaison (Docs/v417/agents/materiaux_combinaison.md, § 2.4).

Lit RawAssets/Skeletal/SK_Hazmat.fbx SANS LE MODIFIER (empreinte comparee au registre RawAssets/protected_assets.json avant
la lecture, et verifiee de nouveau a la fin) et ecrit seulement dans RawAssets/Derived/Hazmat/ :
  hazmat_uv_islands.png   identifiant d'ilot UV, PNG gris 16 bits : HazmatSuit 1..N, HazmatGlass 65535, 0 hors des ilots
                          (le masque et ses ilots ne sont pas ecrits : il a sa propre texture, T_Hazmat_Mask)
  hazmat_uv_edges.png     bords d'ilots de la combinaison, PNG gris 8 bits, un pixel de large : 255 couture (le bord coincide
                          en 3D avec un autre bord : le maillage est separe le long des coutures UV), 128 bord libre (ourlet :
                          poignets, chevilles, ouverture de la capuche)
  hazmat_uv_islands.json  par ilot : materiau, nombre de faces, os dominant, densite de texels (px/mm a la taille de sortie),
                          hauteur relative (0 aux pieds, 1 au sommet), miroir (aire UV signee), boite UV ; empreinte du FBX

Ces fichiers sont les entrees de Tools/derive_hazmat_maps.py (etapes 1 a 10). Ils ne sont pas importes dans Unreal.

Regles (identiques au prototype qui a valide l'algorithme) :
  - ilot : faces voisines par une arete, memes UV aux deux extremites, meme materiau (union-find) ;
  - couture : arete de bord dont les deux sommets 3D coincident (a 0,01 cm pres) avec ceux d'une autre arete de bord ;
  - densite : px_par_mm = sqrt(aire_UV x taille^2 / aire_3D en mm^2) ;
  - rasterisation (u, v) -> (u x taille, (1 - v) x taille) : un pixel appartient a un triangle si son centre est dedans.

Usage (Blender 5.2 en arriere-plan, sans les reglages de l'utilisateur) :
  blender --background --factory-startup --python Tools/Blender/export_hazmat_uv.py [-- --size 2048]
"""
import hashlib
import json
import math
import os
import struct
import sys
import zlib
from collections import Counter, defaultdict

import bpy  # (avant bmesh : le module bpy l'enregistre)
import bmesh
import numpy as np
from mathutils import kdtree

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Tools"))
import protect_assets  # noqa: E402

SCRIPT_VERSION = 1
SRC = os.path.join(ROOT, "RawAssets", "Skeletal", "SK_Hazmat.fbx")
SRC_REL = "RawAssets/Skeletal/SK_Hazmat.fbx"
OUT = os.path.join(ROOT, "RawAssets", "Derived", "Hazmat")
SUIT, GLASS = "HazmatSuit", "HazmatGlass"
GLASS_ID = 65535
SEAM, FREE = 255, 128
WELD_CM = 0.01  # deux sommets a moins de 0,01 cm sont le meme point 3D (couture)


def arg(name, default):
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if name in a:
        i = a.index(name)
        return a[i + 1] if i + 1 < len(a) else default
    return default


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def write_png_gray(path, img, bits):
    """PNG gris 8 ou 16 bits ecrit a la main (zlib) : Blender n'a pas Pillow, et ses images passeraient par la gestion des
    couleurs. Sortie deterministe (filtre 0, compression 9)."""
    h, w = img.shape
    if bits == 16:
        rows = img.astype(">u2").tobytes()
    else:
        rows = img.astype(np.uint8).tobytes()
    stride = w * (2 if bits == 16 else 1)
    raw = b"".join(b"\x00" + rows[y * stride:(y + 1) * stride] for y in range(h))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, bits, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    if not protect_assets.may_write(path):
        raise SystemExit("ecriture refusee : " + path)
    with open(path, "wb") as fh:
        fh.write(png)


def load_mesh():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SRC)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    if len(meshes) != 1:
        raise SystemExit("SK_Hazmat.fbx : 1 maillage attendu, %d trouves" % len(meshes))
    return meshes[0]


def cm_scale(obj):
    """Facteur des coordonnees locales du maillage vers les centimetres. Le FBX est en cm ; selon l'importeur, l'unite est
    portee par les coordonnees locales (facteur 1) ou par l'echelle de l'objet et de la scene. On garde le facteur qui donne
    une combinaison de 150 a 210 cm, sinon on s'arrete."""
    zs = [v.co.z for v in obj.data.vertices]
    span = max(zs) - min(zs)
    s = obj.matrix_world.to_scale()
    world = (abs(s.x) + abs(s.y) + abs(s.z)) / 3.0 * bpy.context.scene.unit_settings.scale_length * 100.0
    for k in (1.0, world):
        if 150.0 <= span * k <= 210.0:
            return k, span * k
    raise SystemExit("hauteur de la combinaison incoherente : %.3f unites locales (facteur monde %.4f)" % (span, world))


def face_uv_area(f, uvl):
    """Aire UV signee d'une face (lacet)"""
    pts = [l[uvl].uv for l in f.loops]
    return sum(pts[i].x * pts[(i + 1) % len(pts)].y - pts[(i + 1) % len(pts)].x * pts[i].y for i in range(len(pts))) / 2.0


def uv_key(loop, uvl):
    u = loop[uvl].uv
    return (round(u.x, 5), round(u.y, 5))


def same_island_edge(e, uvl):
    """Arete interieure d'un ilot : deux faces, meme materiau, memes UV aux deux extremites"""
    if len(e.link_loops) != 2:
        return False
    l1, l2 = e.link_loops
    return (l1.face.material_index == l2.face.material_index and uv_key(l1, uvl) == uv_key(l2.link_loop_next, uvl)
            and uv_key(l1.link_loop_next, uvl) == uv_key(l2, uvl))


def islands_of(bm, uvl):
    """Union-find des faces : voisines par une arete, memes UV aux deux extremites, meme materiau"""
    parent = list(range(len(bm.faces)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for e in bm.edges:
        if same_island_edge(e, uvl):
            l1, l2 = e.link_loops
            ra, rb = find(l1.face.index), find(l2.face.index)
            if ra != rb:
                parent[max(ra, rb)] = min(ra, rb)
    groups = defaultdict(list)
    for f in bm.faces:
        groups[find(f.index)].append(f)
    return groups


def dominant_bones(obj):
    names = {g.index: g.name for g in obj.vertex_groups}
    dom = {}
    for v in obj.data.vertices:
        best, bw = None, 0.0
        for ge in v.groups:
            if ge.weight > bw:
                bw, best = ge.weight, names.get(ge.group)
        dom[v.index] = best
    return dom


def weld_ids(bm, k):
    """Representant de chaque sommet : le plus petit indice a moins de WELD_CM (arbre kd, deterministe)"""
    tree = kdtree.KDTree(len(bm.verts))
    for v in bm.verts:
        tree.insert(v.co, v.index)
    tree.balance()
    rep = {}
    for v in bm.verts:
        rep[v.index] = min(i for (_, i, _) in tree.find_range(v.co, WELD_CM / k))
    return rep


def to_px_t(uv, size):
    return (uv[0] * size, (1.0 - uv[1]) * size)


def raster_triangles(ids, tris, value, size):
    """Remplit ids (H x W) avec value la ou le centre du pixel est dans le triangle (bords inclus)"""
    for (ax, ay), (bx, by), (cx, cy) in tris:
        x0 = max(int(math.floor(min(ax, bx, cx) - 0.5)), 0)
        x1 = min(int(math.ceil(max(ax, bx, cx) - 0.5)), size - 1)
        y0 = max(int(math.floor(min(ay, by, cy) - 0.5)), 0)
        y1 = min(int(math.ceil(max(ay, by, cy) - 0.5)), size - 1)
        if x1 < x0 or y1 < y0:
            continue
        den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(den) < 1e-12:
            continue
        px, py = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        w0 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / den
        w1 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / den
        w2 = 1.0 - w0 - w1
        eps = -1e-6
        inside = (w0 >= eps) & (w1 >= eps) & (w2 >= eps)
        sub = ids[y0:y1 + 1, x0:x1 + 1]
        sub[inside] = value


def raster_lines(img, segs, value, size):
    """Segments d'un pixel de large (echantillonnage tous les 0,25 px) ; la couture l'emporte sur le bord libre"""
    for (ax, ay), (bx, by) in segs:
        n = int(math.ceil(math.hypot(bx - ax, by - ay) * 4.0)) + 1
        t = np.linspace(0.0, 1.0, n)
        xs = np.clip(np.floor(ax + (bx - ax) * t).astype(np.int64), 0, size - 1)
        ys = np.clip(np.floor(ay + (by - ay) * t).astype(np.int64), 0, size - 1)
        img[ys, xs] = np.maximum(img[ys, xs], value)


def main():
    size = int(arg("--size", "2048"))
    reg = protect_assets.load().get("derived", {}).get(SRC_REL, {}).get("sha256")
    before = sha256(SRC)
    if reg and reg != before:
        raise SystemExit("SK_Hazmat.fbx ne correspond pas au registre protege (%s... au lieu de %s...) : arret" % (before[:12], reg[:12]))
    os.makedirs(OUT, exist_ok=True)

    obj = load_mesh()
    k, height = cm_scale(obj)
    me = obj.data
    mats = [m.name if m else "" for m in me.materials]
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.faces.ensure_lookup_table()
    bm.verts.ensure_lookup_table()
    uvl = bm.loops.layers.uv[0]
    zs = [v.co.z for v in bm.verts]
    zmin, zmax = min(zs), max(zs)
    dom = dominant_bones(obj)
    groups = islands_of(bm, uvl)

    # 1) description des ilots, ordre stable (materiau, coin haut-gauche de la boite UV, nombre de faces)
    desc = []
    for root, faces in groups.items():
        mat = mats[faces[0].material_index] if faces[0].material_index < len(mats) else ""
        if mat not in (SUIT, GLASS):
            continue
        us = [l[uvl].uv.x for f in faces for l in f.loops]
        vs = [l[uvl].uv.y for f in faces for l in f.loops]
        area3 = sum(f.calc_area() for f in faces) * k * k * 100.0  # mm^2
        areas = [face_uv_area(f, uvl) for f in faces]
        signed, auv = sum(areas), sum(abs(x) for x in areas)
        bones = Counter(dom.get(l.vert.index) for f in faces for l in f.loops)
        fz = [l.vert.co.z for f in faces for l in f.loops]
        desc.append(dict(root=root, mat=mat, faces=faces, umin=min(us), umax=max(us), vmin=min(vs), vmax=max(vs), area_mm2=area3,
                         uv_area=auv, signed=signed, bone=bones.most_common(1)[0][0] if bones else None,
                         zrel=((min(fz) - zmin) / (zmax - zmin), (max(fz) - zmin) / (zmax - zmin))))
    desc.sort(key=lambda d: (d["mat"] != SUIT, round(1.0 - d["vmax"], 4), round(d["umin"], 4), -len(d["faces"])))
    next_id = 1
    for d in desc:
        if d["mat"] == SUIT:
            d["id"] = next_id
            next_id += 1
        else:
            d["id"] = GLASS_ID
    if next_id >= GLASS_ID:
        raise SystemExit("trop d'ilots pour 16 bits")

    # 2) aretes de bord : couture (meme position 3D qu'un autre bord) ou bord libre. Chaque cote de couture de la combinaison
    #    est aussi decrit dans le JSON ("seams") avec l'ilot d'en face : derive_hazmat_maps.py en tire les coutures thermosoudees
    #    (pas de bande sur le caoutchouc moule des gants et des bottes) et controle la continuite du relief de part et d'autre
    rep = weld_ids(bm, k)
    island_of_face = {}
    for d in desc:
        for f in d["faces"]:
            island_of_face[f.index] = d["id"]
    border = []  # (cle 3D, materiau, ilot, (sommet, uv) aux deux extremites)
    for e in bm.edges:
        if not e.link_loops or same_island_edge(e, uvl):
            continue
        key = tuple(sorted((rep[e.verts[0].index], rep[e.verts[1].index])))
        for l in e.link_loops:  # une arete entre deux ilots (UV differents) donne un bord de chaque cote
            ends = sorted([(rep[l.vert.index], tuple(l[uvl].uv)), (rep[l.link_loop_next.vert.index], tuple(l.link_loop_next[uvl].uv))])
            border.append((key, mats[l.face.material_index], island_of_face.get(l.face.index, 0), ends))
    sides = defaultdict(list)
    for b in border:
        sides[b[0]].append(b)
    seams, frees, seam_rec = [], [], []
    for key, m, isl, ends in border:
        if m != SUIT:
            continue
        seg = (to_px_t(ends[0][1], size), to_px_t(ends[1][1], size))
        others = [o for o in sides[key] if not (o[2] == isl and o[3] == ends)]
        if not others:
            frees.append(seg)
            continue
        seams.append(seg)
        # ilot d'en face : un seul cote de la combinaison -> son identifiant ; un autre materiau (masque) -> 0
        other = others[0][2] if len(others) == 1 and others[0][1] == SUIT else 0
        (_, (u0, v0)), (_, (u1, v1)) = ends
        seam_rec.append([key[0], key[1], isl, other, round(u0, 6), round(v0, 6), round(u1, 6), round(v1, 6)])
    seam_rec.sort()

    # 3) rasterisation
    ids = np.zeros((size, size), np.uint16)
    me.calc_loop_triangles()
    uvdata = me.uv_layers[0].data
    by_id = defaultdict(list)
    poly_island = {}
    for d in desc:
        for f in d["faces"]:
            poly_island[f.index] = d["id"]
    for t in me.loop_triangles:
        i = poly_island.get(t.polygon_index)
        if i is None:
            continue
        by_id[i].append(tuple((uvdata[li].uv.x * size, (1.0 - uvdata[li].uv.y) * size) for li in t.loops))
    for d in desc:
        if d["id"] != GLASS_ID:
            raster_triangles(ids, by_id[d["id"]], d["id"], size)
    raster_triangles(ids, by_id[GLASS_ID], GLASS_ID, size)  # la visiere en dernier (ovale distinct de l'atlas)
    edges = np.zeros((size, size), np.uint8)
    raster_lines(edges, frees, FREE, size)
    raster_lines(edges, seams, SEAM, size)

    write_png_gray(os.path.join(OUT, "hazmat_uv_islands.png"), ids, 16)
    write_png_gray(os.path.join(OUT, "hazmat_uv_edges.png"), edges, 8)

    # 4) description JSON (aucune date : le fichier ne change que si le maillage ou ce script changent). Les coutures sont
    #    ecrites une par ligne (fichier lisible, diff court)
    out_isl = []
    for d in desc:
        px = int((ids == d["id"]).sum())
        out_isl.append(dict(id=d["id"], mat=d["mat"], faces=len(d["faces"]), bone=d["bone"],
                            px_per_mm=round(math.sqrt(d["uv_area"] * size * size / d["area_mm2"]), 4) if d["area_mm2"] > 0 else 0.0,
                            area_cm2=round(d["area_mm2"] / 100.0, 3), uv_area=round(d["uv_area"], 7), mirrored=d["signed"] > 0,
                            zrel=[round(d["zrel"][0], 4), round(d["zrel"][1], 4)],
                            uv_box=[round(d["umin"], 5), round(d["vmin"], 5), round(d["umax"], 5), round(d["vmax"], 5)], pixels=px))
    info = dict(
        script="Tools/Blender/export_hazmat_uv.py", script_version=SCRIPT_VERSION, blender=bpy.app.version_string,
        source=SRC_REL, source_sha256=before, size=size, cm_per_unit=round(k, 6), height_cm=round(height, 2),
        glass_id=GLASS_ID, edge_values={"seam": SEAM, "free": FREE}, weld_cm=WELD_CM,
        counts=dict(islands_suit=next_id - 1, islands_glass=sum(1 for d in desc if d["id"] == GLASS_ID),
                    seam_edges=len(seams), free_edges=len(frees), seam_pixels=int((edges == SEAM).sum()),
                    free_pixels=int((edges == FREE).sum()), island_pixels=int(((ids > 0) & (ids != GLASS_ID)).sum()),
                    glass_pixels=int((ids == GLASS_ID).sum())),
        files={n: sha256(os.path.join(OUT, n)) for n in ("hazmat_uv_islands.png", "hazmat_uv_edges.png")},
        islands=out_isl,
        seams_format="[sommet 3D a, sommet 3D b, ilot, ilot d'en face (0 : autre materiau), u_a, v_a, u_b, v_b]",
        seams=seam_rec)
    path = os.path.join(OUT, "hazmat_uv_islands.json")
    if not protect_assets.may_write(path):
        raise SystemExit("ecriture refusee : " + path)
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(info, fh, indent=1, sort_keys=True)
        fh.write("\n")
    bm.free()

    after = sha256(SRC)
    if after != before:
        raise SystemExit("SK_Hazmat.fbx a change pendant la lecture : %s" % after)
    print("ilots : %d (combinaison) + visiere ; coutures %d aretes, bords libres %d ; hauteur %.1f cm"
          % (next_id - 1, len(seams), len(frees), height))
    print("-> %s" % os.path.relpath(OUT, ROOT))


main()
