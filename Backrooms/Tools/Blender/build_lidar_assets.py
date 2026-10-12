"""
v4.13 : ressources du scanner LiDAR du Level 6.

Utilisation :  python Tools/Blender/build_lidar_assets.py        (module bpy)
               blender -b -P Tools/Blender/build_lidar_assets.py

Sorties :
  * RawAssets/Meshes/SM_LidarDot, SM_LidarTri, SM_LidarDiamond, SM_LidarCross, SM_LidarChevron, SM_LidarSquare (.fbx) :
    glyphes plats des points du scanner (1 x 1 m, a mettre a l'echelle par instance ; deux faces ; +X vers l'observateur,
    la forme dans le plan YZ). Une forme par categorie : les categories se distinguent sans les couleurs.
  * RawAssets/Meshes/SM_LidarScanner.fbx : le scanner de l'equipe de L., tenu en main (poignee, corps, lentille "Glow").
  * Source/Backrooms/Private/BRLidarShapes.inl : silhouettes en points des cages, DERIVEES des modeles de
    RawAssets/Skeletal (SK_Hound, SK_Faceling, SK_Partygoer, SK_SkinStealer, SK_Wretch, SK_Clump), lus seulement :
    aucun fichier d'origine n'est modifie (empreintes verifiees avant et apres, Tools/protect_assets.py). Echantillonnage
    de la surface proportionnel a l'aire, pose de repos, normalisation a 1,8 m de haut au plus, posee au sol.
  * Docs/v413/lidar_silhouettes.png : planche des silhouettes (vues de face, en points).
"""
import hashlib
import math
import os
import random
import sys

import bpy  # noqa: E402  (avant bmesh / mathutils)
import bmesh  # noqa: E402
from mathutils import Vector  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..")))
import generate_models as gm  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SKEL = os.path.join(ROOT, "RawAssets", "Skeletal")
SHAPES_INL = os.path.join(ROOT, "Source", "Backrooms", "Private", "BRLidarShapes.inl")
SHEET = os.path.join(ROOT, "Docs", "v413", "lidar_silhouettes.png")

# Ordre de BRMission::NumFigures (BRMissionLogic.h) : 0 Hound, 1 Faceling, 2 Partygoer, 3 Skin-Stealer, 4 Wretch, 5 Clump
FIGURES = [("Hound", "SK_Hound.fbx"), ("Faceling", "SK_Faceling.fbx"), ("Partygoer", "SK_Partygoer.fbx"),
           ("SkinStealer", "SK_SkinStealer.fbx"), ("Wretch", "SK_Wretch.fbx"), ("Clump", "SK_Clump.fbx")]
POINTS = 700
MAX_HEIGHT = 1.8


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        h.update(f.read())
    return h.hexdigest()


# ------------------------------------------------------------------------------------------------ glyphes
def flat(name, polys):
    """Glyphe plat dans le plan YZ (x = 0), deux faces : liste de polygones (listes de (y, z))"""
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    for poly in polys:
        front = [bm.verts.new((0.0, y, z)) for y, z in poly]
        bm.faces.new(front)
        back = [bm.verts.new((-0.002, y, z)) for y, z in reversed(poly)]
        bm.faces.new(back)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    o.data.materials.append(gm.mat("Glow"))
    return o


def glyphs():
    out = {}
    n = 8
    out["SM_LidarDot"] = [[(0.5 * math.cos(2 * math.pi * i / n), 0.5 * math.sin(2 * math.pi * i / n)) for i in range(n)]]
    out["SM_LidarSquare"] = [[(-0.42, -0.42), (0.42, -0.42), (0.42, 0.42), (-0.42, 0.42)]]
    out["SM_LidarTri"] = [[(-0.5, -0.4), (0.5, -0.4), (0.0, 0.5)]]
    out["SM_LidarDiamond"] = [[(0.0, -0.5), (0.5, 0.0), (0.0, 0.5), (-0.5, 0.0)]]
    w = 0.16
    out["SM_LidarCross"] = [[(-0.5, -w), (0.5, -w), (0.5, w), (-0.5, w)], [(-w, -0.5), (w, -0.5), (w, -w), (-w, -w)],
                            [(-w, w), (w, w), (w, 0.5), (-w, 0.5)]]
    # Chevron pointe vers +Y (le scanner l'oriente dans le sens du chemin)
    out["SM_LidarChevron"] = [[(-0.1, -0.5), (0.5, 0.0), (0.2, 0.0), (-0.4, -0.5)], [(0.2, 0.0), (0.5, 0.0), (-0.1, 0.5), (-0.4, 0.5)]]
    paths = []
    for name, polys in out.items():
        gm.reset()
        o = flat(name, polys)
        p = gm.export_fbx(o, name)
        paths.append(p)
        print("  glyphe", name, "->", p)
    return paths


def scanner():
    gm.reset()
    parts = [
        gm.box((0.16, 0.07, 0.09), (0.0, 0.0, 0.0), "DarkMetal", bevel=0.01),       # corps
        gm.box((0.05, 0.04, 0.12), (-0.04, 0.0, -0.09), "Rubber", bevel=0.008),     # poignee
        gm.cyl(0.03, 0.03, (0.095, 0.0, 0.01), "Metal", rot=(0, 90, 0), verts=20),  # bague de la lentille
        gm.cyl(0.024, 0.01, (0.112, 0.0, 0.01), "GlowCool", rot=(0, 90, 0), verts=20),  # lentille
        gm.box((0.06, 0.05, 0.004), (-0.02, 0.0, 0.047), "GlowCool"),               # ecran du dessus
        gm.box((0.015, 0.02, 0.02), (-0.02, 0.0, -0.045), "Hazard"),                # gachette
    ]
    o = gm.join(parts, "SM_LidarScanner")
    return gm.export_fbx(o, "SM_LidarScanner")


# ------------------------------------------------------------------------------------------------ silhouettes
def sample_model(path, count, rng):
    """Points de surface (pose de repos), proportionnels a l'aire, dans le repere du jeu (+X devant, Z haut, m)"""
    gm.reset()
    bpy.ops.import_scene.fbx(filepath=path, use_anim=False, ignore_leaf_bones=True)
    tris = []
    for o in bpy.context.scene.objects:
        if o.type != "MESH":
            continue
        dg = bpy.context.evaluated_depsgraph_get()
        eo = o.evaluated_get(dg)
        me = eo.to_mesh()
        mw = o.matrix_world
        me.calc_loop_triangles()
        for t in me.loop_triangles:
            a, b, c = (mw @ me.vertices[i].co for i in t.vertices)
            area = (b - a).cross(c - a).length * 0.5
            if area > 0:
                tris.append((area, a.copy(), b.copy(), c.copy()))
        eo.to_mesh_clear()
    if not tris:
        raise RuntimeError("aucun triangle dans " + path)
    total = sum(t[0] for t in tris)
    cum = []
    acc = 0.0
    for t in tris:
        acc += t[0]
        cum.append(acc)
    pts = []
    import bisect
    for _ in range(count):
        r = rng.random() * total
        a, b, c = tris[bisect.bisect_left(cum, r)][1:]
        u, v = rng.random(), rng.random()
        if u + v > 1:
            u, v = 1 - u, 1 - v
        pts.append(a + (b - a) * u + (c - a) * v)
    # Normalisation : au sol, centree, au plus MAX_HEIGHT de haut
    xs = [p.x for p in pts]
    ys = [p.y for p in pts]
    zs = [p.z for p in pts]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    h = max(zs) - min(zs)
    ext = max(max(xs) - min(xs), max(ys) - min(ys), h)
    k = min(1.0, MAX_HEIGHT / max(h, 1e-3), 1.0 / max(ext, 1e-3) * 1.6)
    z0 = min(zs)
    return [((p.x - cx) * k, (p.y - cy) * k, (p.z - z0) * k) for p in pts], h


def silhouettes():
    rng = random.Random(6613)
    before = {}
    lines = ["// v4.13 : silhouettes en points des cages du Level 6 (genere par Tools/Blender/build_lidar_assets.py, ne pas",
             "// modifier a la main). Derivees des modeles de RawAssets/Skeletal, lus seulement : points de surface de la pose de",
             "// repos, au sol, centres, en millimetres (X devant, Y droite, Z haut), %d points par silhouette." % POINTS,
             "// Ordre de BRMission::NumFigures : Hound, Faceling, Partygoer, Skin-Stealer, Wretch, Clump.",
             "#pragma once", "", "namespace BRLidarShapes", "{",
             "\tconstexpr int PointsPerFigure = %d;" % POINTS, "\tconstexpr int NumFigures = %d;" % len(FIGURES),
             "\t/** [figure][point][x, y, z] en mm */", "\tstatic const short Points[NumFigures][PointsPerFigure][3] = {"]
    sheet = []
    for name, file in FIGURES:
        path = os.path.join(SKEL, file)
        before[path] = sha(path)
        pts, h = sample_model(path, POINTS, rng)
        if name == "Hound":
            # Quadrupede : de profil dans la cage (de face, ce n'est qu'une masse), longueur le long de Y
            pts = [(-y, x, z) for x, y, z in pts]
        sheet.append((name, pts))
        rows = ", ".join("{%d, %d, %d}" % (round(x * 1000), round(y * 1000), round(z * 1000)) for x, y, z in pts)
        lines.append("\t\t// %s (%s, hauteur d'origine %.2f m)" % (name, file, h))
        lines.append("\t\t{ " + rows + " },")
        print("  silhouette %-12s : %d points, hauteur d'origine %.2f m" % (name, len(pts), h))
    lines += ["\t};", "}", ""]
    for path, digest in before.items():
        if sha(path) != digest:
            raise RuntimeError("modele d'origine modifie : " + path)
    with open(SHAPES_INL, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print("  ->", SHAPES_INL, "(modeles d'origine intacts : empreintes identiques avant et apres)")
    return sheet


def draw_sheet(sheet):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("  (Pillow absent : pas de planche)")
        return
    W, H = 300, 340
    img = Image.new("RGB", (W * len(sheet), H), (6, 7, 9))
    d = ImageDraw.Draw(img)
    for i, (name, pts) in enumerate(sheet):
        ox = i * W
        for x, y, z in pts:
            # vue de face (le joueur regarde la cage) : Y vers la droite, Z vers le haut
            px = ox + W / 2 + y * 140
            pz = H - 40 - z * 150
            d.rectangle([px - 1, pz - 1, px + 1, pz + 1], fill=(255, 190, 70))
        d.text((ox + 10, H - 28), name, fill=(220, 220, 220))
    os.makedirs(os.path.dirname(SHEET), exist_ok=True)
    img.save(SHEET)
    print("  ->", SHEET)


def main():
    if "--silhouettes" not in sys.argv:
        print("Glyphes du scanner :")
        glyphs()
        print("Scanner tenu en main :", scanner())
    print("Silhouettes des cages :")
    draw_sheet(silhouettes())


if __name__ == "__main__":
    main()
