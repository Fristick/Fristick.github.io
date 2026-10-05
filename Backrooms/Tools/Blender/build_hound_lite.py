"""
v4.6 : SK_HoundLite, derive allege du Hound fourni (SK_Hound.fbx reste intact).

Constat (v4.5) : SK_Hound compte 175 201 sommets et 319 000 triangles, dont 281 248 (88 %) pour les cheveux : 7 801 meches,
chacune un ruban de 20 sommets (1 x 9 quads, faces doublees pour les deux cotes). Le corps, la tete, la bouche, les yeux
et la langue ne font que 38 000 triangles.

Le derive garde TOUT sauf une partie des meches : 45 % des meches (choix deterministe), chaque meche gardee deux fois
plus large autour de son axe (la chevelure reste opaque devant le visage). Resultat : 89 381 sommets et 164 524
triangles (-49 % / -48 %), corps, squelette et materiaux identiques (comparaison : Docs/v46/blender_hound_original_vs_lite.jpg). Squelette, poids, UV, materiaux et
geometrie du corps sont ceux du fichier fourni. Le jeu utilise SK_HoundLite dans les profils Performance et Qualite,
SK_Hound en Cinematique (BREntity.cpp, BuildHoundModel), avec repli sur SK_Hound si le derive manque.

Usage : python Tools/Blender/build_hound_lite.py [-- --force] [--keep 0.45] [--widen 2.0] [--preview Docs/v46/hound.jpg]
"""
import math
import os
import sys

import bpy  # (avant bmesh : le module bpy l'enregistre)
import bmesh
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Tools"))
import protect_assets  # noqa: E402

SKEL = os.path.join(ROOT, "RawAssets", "Skeletal")
SRC = os.path.join(SKEL, "SK_Hound.fbx")
DST = os.path.join(SKEL, "SK_HoundLite.fbx")


def arg(name, default):
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if name in a:
        i = a.index(name)
        return a[i + 1] if i + 1 < len(a) else default
    return default


def load(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    body = next(o for o in bpy.data.objects if o.type == "MESH")
    return arm, body


def strands(bm, hair_index):
    """Meches de cheveux : composantes connexes des faces du materiau des cheveux"""
    bm.faces.ensure_lookup_table()
    faces = [f for f in bm.faces if f.material_index == hair_index]
    fset = {f.index for f in faces}
    seen, out = set(), []
    for f in faces:
        if f.index in seen:
            continue
        comp, stack = [], [f]
        seen.add(f.index)
        while stack:
            g = stack.pop()
            comp.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index in fset and h.index not in seen:
                        seen.add(h.index)
                        stack.append(h)
        out.append(comp)
    return out


def widen(comp, factor):
    """Ruban 1 x N : paires de sommets (barreaux) reliees par une arete ; chaque paire s'ecarte de son milieu"""
    verts = list({v for f in comp for v in f.verts})
    if len(verts) < 4:
        return False
    co = np.array([tuple(v.co) for v in verts])
    c = co.mean(0)
    _, _, vt = np.linalg.svd(co - c, full_matrices=False)
    t = (co - c) @ vt[0]
    order = np.argsort(t)
    pairs = [(verts[order[i]], verts[order[i + 1]]) for i in range(0, len(order) - 1, 2)]
    for a, b in pairs:  # chaque paire doit etre un barreau du ruban (sinon on ne touche pas a la meche)
        if not any(b in e.verts for e in a.link_edges):
            return False
    for a, b in pairs:
        m = (a.co + b.co) * 0.5
        a.co = m + (a.co - m) * factor
        b.co = m + (b.co - m) * factor
    return True


def build(keep, factor, force):
    if not protect_assets.may_write(DST, force):
        return None
    arm, body = load(SRC)
    hair = next(i for i, s in enumerate(body.material_slots) if s.material and s.material.name.startswith("HoundHair"))
    bm = bmesh.new()
    bm.from_mesh(body.data)
    comps = strands(bm, hair)
    doomed, kept, widened = [], 0, 0
    for i, comp in enumerate(comps):
        # choix deterministe (hachage de Knuth sur le rang de la meche, ordre des faces du fichier fourni)
        if ((i * 2654435761) & 0xFFFFFFFF) / 4294967296.0 < keep:
            kept += 1
            widened += 1 if widen(comp, factor) else 0
        else:
            doomed += comp
    verts_before = len(bm.verts)
    bmesh.ops.delete(bm, geom=doomed, context="FACES")
    loose = [v for v in bm.verts if not v.link_faces]
    if loose:
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bm.to_mesh(body.data)
    bm.free()
    body.data.update()
    print("meches : %d, gardees %d (elargies %d), sommets %d -> %d" % (len(comps), kept, widened, verts_before, len(body.data.vertices)))
    # Retour au repere du fichier (centimetres, comme build_entity_skeletal.export)
    for o in (arm, body):
        o.matrix_world = Matrix.Scale(100.0, 4) @ o.matrix_world
    bpy.ops.object.select_all(action="DESELECT")
    for o in (arm, body):
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    sc = bpy.context.scene
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 0.01
    arm.name = arm.data.name = "Armature"
    bpy.ops.export_scene.fbx(filepath=DST, use_selection=True, object_types={"ARMATURE", "MESH"}, use_mesh_modifiers=False,
                             mesh_smooth_type="FACE", add_leaf_bones=False, use_armature_deform_only=False, bake_anim=False,
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", axis_forward="-Z", axis_up="Y",
                             path_mode="STRIP")
    protect_assets.record(DST, "derive allege de SK_Hound (build_hound_lite.py)")
    return DST


def stats(path):
    arm, body = load(path)
    me = body.data
    me.calc_loop_triangles()
    hair = next(i for i, s in enumerate(body.material_slots) if s.material and s.material.name.startswith("HoundHair"))
    tris = len(me.loop_triangles)
    hair_tris = sum(1 for t in me.loop_triangles if t.material_index == hair)
    co = np.array([tuple(body.matrix_world @ v.co) for v in me.vertices])
    heads = {b.name: tuple(arm.matrix_world @ b.head_local) for b in arm.data.bones}
    return {"verts": len(me.vertices), "tris": tris, "hair_tris": hair_tris, "bbox": (co.min(0), co.max(0)), "heads": heads,
            "groups": sorted(g.name for g in body.vertex_groups), "mats": [s.material.name.split(".")[0] for s in body.material_slots]}


def compare():
    a, b = stats(SRC), stats(DST)
    dmax = max((Vector(a["heads"][k]) - Vector(b["heads"][k])).length for k in a["heads"])
    print("SK_Hound      : %d sommets, %d triangles (cheveux %d)" % (a["verts"], a["tris"], a["hair_tris"]))
    print("SK_HoundLite  : %d sommets, %d triangles (cheveux %d)" % (b["verts"], b["tris"], b["hair_tris"]))
    print("hors cheveux  : %d -> %d triangles (identiques attendus)" % (a["tris"] - a["hair_tris"], b["tris"] - b["hair_tris"]))
    print("ecart maxi des os : %.4f m ; memes groupes de sommets : %s ; memes materiaux : %s" % (dmax, a["groups"] == b["groups"],
          a["mats"] == b["mats"]))
    print("boite englobante (m) : %s -> %s" % (np.round(a["bbox"][1] - a["bbox"][0], 3), np.round(b["bbox"][1] - b["bbox"][0], 3)))
    return a, b


def preview(out):
    """Rendu Blender cote a cote (meme camera, meme lumiere) : SK_Hound a gauche, SK_HoundLite a droite"""
    from PIL import Image, ImageDraw
    tiles = []
    for path in (SRC, DST):
        arm, body = load(path)
        for slot in body.material_slots:
            m = bpy.data.materials.new("m")
            m.use_nodes = True
            bs = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
            n = slot.material.name if slot.material else ""
            col = (0.02, 0.02, 0.02, 1) if "Hair" in n else ((0.75, 0.72, 0.68, 1) if ("Skin" in n or "Face" in n) else (0.3, 0.05, 0.05, 1))
            bs.inputs["Base Color"].default_value = col
            bs.inputs["Roughness"].default_value = 0.5
            slot.material = m
        sc = bpy.context.scene
        sc.render.engine = "CYCLES"
        sc.cycles.device = "CPU"
        sc.cycles.samples = 24
        sc.render.resolution_x, sc.render.resolution_y = 640, 640
        w = bpy.data.worlds.new("W")
        w.use_nodes = True
        w.node_tree.nodes["Background"].inputs[0].default_value = (0.45, 0.45, 0.47, 1)
        sc.world = w
        cam = bpy.data.objects.new("C", bpy.data.cameras.new("C"))
        sc.collection.objects.link(cam)
        sc.camera = cam
        co = np.array([tuple(body.matrix_world @ v.co) for v in body.data.vertices])
        mn, mx = Vector(co.min(0)), Vector(co.max(0))
        ctr = (mn + mx) / 2 + Vector((0.25 * (mx - mn).x, 0, 0.15 * (mx - mn).z))
        cam.data.lens = 50
        cam.location = ctr + Vector((1.0, -0.6, 0.15)).normalized() * (mx - mn).length * 1.25
        cam.rotation_euler = (ctr - cam.location).to_track_quat("-Z", "Y").to_euler()
        sun = bpy.data.objects.new("L", bpy.data.lights.new("L", "SUN"))
        sun.data.energy = 3.5
        sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(30))
        sc.collection.objects.link(sun)
        try:
            sc.render.image_settings.media_type = "IMAGE"
        except Exception:
            pass
        sc.render.image_settings.file_format = "JPEG"
        tile = out + ("_a.jpg" if path == SRC else "_b.jpg")
        sc.render.filepath = tile
        bpy.ops.render.render(write_still=True)
        tiles.append(tile)
    a, b = Image.open(tiles[0]), Image.open(tiles[1])
    sheet = Image.new("RGB", (a.width * 2 + 10, a.height + 40), (12, 12, 12))
    sheet.paste(a, (0, 40))
    sheet.paste(b, (a.width + 10, 40))
    d = ImageDraw.Draw(sheet)
    d.text((8, 6), "RENDU BLENDER (pas Unreal) - gauche : SK_Hound fourni (Cinematique)   droite : SK_HoundLite (Performance, Qualite)",
           fill=(230, 230, 220))
    sheet.save(out, quality=90)
    for t in tiles:
        os.remove(t)
    print("apercu :", out)


if __name__ == "__main__":
    force = "--force" in sys.argv
    keep = float(arg("--keep", "0.45"))
    factor = float(arg("--widen", "2.0"))
    if build(keep, factor, force) or os.path.isfile(DST):
        compare()
    pv = arg("--preview", None)
    if pv:
        preview(os.path.abspath(pv))
