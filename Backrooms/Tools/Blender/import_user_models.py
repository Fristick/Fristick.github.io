"""
Preparation des modeles fournis par l'utilisateur pour le jeu (decoupage en pieces animables).

Placez les fichiers sources dans Tools/SourceModels/ :
    bacteria_recreation.blend                 (Bacteria, archive "bacteria-lifeform-backrooms.zip")
    peppered moth.obj + texture_0_baseColor.png (Deathmoth, archive "deathmoth-backrooms.zip")
    asyc_hazmat.glb                           (combinaison hazmat du joueur, archive "asyc_hazmat.rar")

Puis :
    python Tools/Blender/import_user_models.py          (module pip "bpy")
    blender -b -P Tools/Blender/import_user_models.py   (avec Blender installe)

Sorties :
    RawAssets/Meshes/SM_Hazmat_*.fbx, SM_BacteriaET_*.fbx, SM_DeathmothET_*.fbx
    RawAssets/Textures/T_Hazmat_Suit.jpg, T_Hazmat_Mask.jpg, T_Deathmoth.jpg
    RawAssets/Icons/I_Silhouette.png, RawAssets/Previews/*.jpg
    RawAssets/Meshes/user_models.json  (positions des articulations, en cm, reperes Unreal : +X avant, +Y droite)

Conventions : l'avant regarde vers +X, pieds a Z = 0, chaque piece a son pivot sur son articulation
(epaule, coude, hanche, genou, cou, racine des ailes). Aux articulations, la piece enfant recoit une
copie legerement rentree des faces voisines du parent : quand le membre tourne, il n'y a pas de trou.
"""
import json
import math
import os
import sys

import bpy  # avant bmesh / mathutils
import bmesh
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(ROOT, "Tools", "SourceModels")
OUT_MESH = os.path.join(ROOT, "RawAssets", "Meshes")
OUT_TEX = os.path.join(ROOT, "RawAssets", "Textures")
OUT_ICON = os.path.join(ROOT, "RawAssets", "Icons")
OUT_PREV = os.path.join(ROOT, "RawAssets", "Previews")
JOINTS = {}


# ---------------------------------------------------------------------------
# Utilitaires
# ---------------------------------------------------------------------------
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def ue(v):
    """Position Blender (m) -> Unreal (cm). L'import FBX inverse l'axe Y."""
    return [round(v.x * 100.0, 2), round(-v.y * 100.0, 2), round(v.z * 100.0, 2)]


def link(obj):
    bpy.context.scene.collection.objects.link(obj)
    return obj


def mesh_object(name, me):
    return link(bpy.data.objects.new(name, me))


def save_texture(image, name, size=2048, quality=90):
    """Image Blender (eventuellement empaquetee dans le .glb) -> JPG 2048"""
    from PIL import Image
    w, h = image.size
    px = np.empty(w * h * 4, dtype=np.float32)
    image.pixels.foreach_get(px)
    arr = (np.clip(px.reshape(h, w, 4)[::-1, :, :3], 0, 1) * 255 + 0.5).astype(np.uint8)
    im = Image.fromarray(arr, "RGB")
    if max(w, h) > size:
        im = im.resize((size, size), Image.LANCZOS)
    os.makedirs(OUT_TEX, exist_ok=True)
    path = os.path.join(OUT_TEX, name + ".jpg")
    im.save(path, quality=quality)
    print("  texture", path, im.size)


def export_fbx(obj, name):
    """FBX "tout integre" : sommets en centimetres (unite du fichier = 1 cm), conversion d'axes appliquee a la
    geometrie, noeud sans rotation ni echelle. Un importeur qui ignore la transformation du noeud (Interchange,
    Unreal 5.5+) obtient ainsi exactement la meme taille et la meme orientation que l'importeur FBX classique."""
    os.makedirs(OUT_MESH, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT_MESH, name + ".fbx")
    sc = bpy.context.scene
    old_unit = sc.unit_settings.scale_length
    bpy.context.view_layer.update()
    mw = obj.matrix_world.copy()
    bake = Matrix.Scale(100.0, 4) @ mw
    obj.data.transform(bake)
    obj.matrix_world = Matrix.Identity(4)
    sc.unit_settings.scale_length = 0.01
    try:
        bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, use_mesh_modifiers=True,
                                 mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False, apply_unit_scale=True,
                                 apply_scale_options="FBX_SCALE_NONE", bake_space_transform=True,
                                 axis_forward="-Z", axis_up="Y", path_mode="STRIP")
    finally:
        obj.data.transform(bake.inverted())
        obj.matrix_world = mw
        obj.data.update()
        sc.unit_settings.scale_length = old_unit
    print("  export", name, len(obj.data.vertices), "sommets")


def set_origin(obj, pivot):
    """Deplace l'origine de l'objet sur le pivot (le maillage garde sa place) puis remet l'objet a l'origine"""
    obj.data.transform(Matrix.Translation(-pivot))
    obj.location = (0, 0, 0)


def decimate(obj, ratio):
    if ratio >= 0.999:
        return
    m = obj.modifiers.new("dec", "DECIMATE")
    m.ratio = ratio
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.ops.object.modifier_apply(modifier=m.name)


def rename_materials(obj, mapping):
    for slot in obj.material_slots:
        if slot.material and slot.material.name in mapping:
            new = mapping[slot.material.name]
            mat = bpy.data.materials.get(new) or bpy.data.materials.new(new)
            slot.material = mat


def split_faces(src, keep):
    """Nouvel objet ne contenant que les faces dont l'indice est dans keep"""
    bm = bmesh.new()
    bm.from_mesh(src.data)
    bm.faces.ensure_lookup_table()
    dead = [f for f in bm.faces if f.index not in keep]
    bmesh.ops.delete(bm, geom=dead, context="FACES")
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    me = bpy.data.meshes.new(src.name + "_part")
    bm.to_mesh(me)
    bm.free()
    for m in src.data.materials:
        me.materials.append(m)
    return mesh_object(src.name + "_part", me)


def preview(objs, name, view=Vector((1.0, -0.7, 0.45))):
    """Rendu d'apercu 256x256 d'un ensemble d'objets"""
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 16
    try:
        sc.cycles.use_denoising = False
    except Exception:
        pass
    sc.render.resolution_x = sc.render.resolution_y = 256
    world = bpy.data.worlds.new("W")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.35, 0.35, 0.38, 1)
    sc.world = world
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    ctr = (mn + mx) / 2
    rad = max((mx - mn).length / 2, 0.05)
    cam = link(bpy.data.objects.new("C", bpy.data.cameras.new("C")))
    cam.data.lens = 50
    cam.location = ctr + view.normalized() * rad * 3.2
    cam.rotation_euler = (ctr - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc.camera = cam
    sun = link(bpy.data.objects.new("L", bpy.data.lights.new("L", "SUN")))
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(30))
    os.makedirs(OUT_PREV, exist_ok=True)
    sc.render.image_settings.file_format = "JPEG"
    sc.render.image_settings.color_mode = "RGB"
    sc.render.image_settings.quality = 90
    sc.render.filepath = os.path.join(OUT_PREV, name + ".jpg")
    hidden = [o for o in sc.objects if o.type == "MESH" and o not in objs]
    for o in hidden:
        o.hide_render = True
    bpy.ops.render.render(write_still=True)
    for o in hidden:
        o.hide_render = False
    bpy.data.objects.remove(cam)
    bpy.data.objects.remove(sun)


def bake_world(obj):
    """Applique la transformation monde (et les modificateurs) dans un nouveau maillage independant"""
    dg = bpy.context.evaluated_depsgraph_get()
    ev = obj.evaluated_get(dg)
    me = bpy.data.meshes.new_from_object(ev, preserve_all_data_layers=True, depsgraph=dg)
    me.transform(obj.matrix_world)
    return me


# ---------------------------------------------------------------------------
# Hazmat : combinaison du joueur (rig Mixamo) -> pieces articulees
# ---------------------------------------------------------------------------
HAZMAT_PARTS = {
    "Head": ("Neck", "Head"),
    "UpperArmL": ("LeftArm_",),
    "LowerArmL": ("LeftForeArm", "LeftHand"),
    "UpperArmR": ("RightArm_",),
    "LowerArmR": ("RightForeArm", "RightHand"),
    "ThighL": ("LeftUpLeg",),
    "ShinL": ("LeftLeg", "LeftFoot", "LeftToe"),
    "ThighR": ("RightUpLeg",),
    "ShinR": ("RightLeg", "RightFoot", "RightToe"),
}
HAZMAT_PARENT = {"Head": "Torso", "UpperArmL": "Torso", "UpperArmR": "Torso", "LowerArmL": "UpperArmL",
                 "LowerArmR": "UpperArmR", "ThighL": "Torso", "ThighR": "Torso", "ShinL": "ThighL", "ShinR": "ThighR"}
PART_NAMES = ["Torso"] + list(HAZMAT_PARTS.keys())


def part_of_bone(bone):
    for part, keys in HAZMAT_PARTS.items():
        if any(k in bone for k in keys):
            return part
    return "Torso"


def rotate_pose_bone(arm, pb, axis_world, angle):
    """Rotation d'un os autour d'un axe monde passant par sa tete"""
    mw = arm.matrix_world
    head_w = mw @ pb.head
    rot = Matrix.Translation(head_w) @ Matrix.Rotation(angle, 4, axis_world) @ Matrix.Translation(-head_w)
    pb.matrix = mw.inverted() @ rot @ mw @ pb.matrix
    bpy.context.view_layer.update()


def process_hazmat():
    path = os.path.join(SRC, "asyc_hazmat.glb")
    if not os.path.isfile(path):
        print("!! introuvable :", path)
        return
    print("== Hazmat")
    reset()
    bpy.ops.import_scene.gltf(filepath=path)
    for o in list(bpy.data.objects):
        if o.type == "MESH" and o.name.startswith("Icosphere"):
            bpy.data.objects.remove(o)
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    pbs = arm.pose.bones

    def bone(key):
        return [b for b in pbs if key in b.name][0]

    # Textures (avant toute modification)
    for m in bpy.data.materials:
        if m.node_tree:
            for n in m.node_tree.nodes:
                if n.type == "TEX_IMAGE" and n.image and n.image.size[0] > 0:
                    if m.name == "Suit":
                        save_texture(n.image, "T_Hazmat_Suit")
                    elif m.name == "Mask":
                        save_texture(n.image, "T_Hazmat_Mask")

    # Pose de repos "bras le long du corps" (le modele est livre en T) : il regarde vers -Y
    rotate_pose_bone(arm, bone("LeftArm_"), Vector((0, 1, 0)), math.radians(78))
    rotate_pose_bone(arm, bone("RightArm_"), Vector((0, 1, 0)), math.radians(-78))
    # avant-bras tres legerement plies vers l'avant (-Y)
    rotate_pose_bone(arm, bone("LeftForeArm"), Vector((1, 0, 0)), math.radians(-12))
    rotate_pose_bone(arm, bone("RightForeArm"), Vector((1, 0, 0)), math.radians(-12))

    mw = arm.matrix_world
    joints_w = {
        "Torso": mw @ bone("Hips").head,
        "Head": mw @ bone("Neck").head,
        "UpperArmL": mw @ bone("LeftArm_").head, "LowerArmL": mw @ bone("LeftForeArm").head,
        "UpperArmR": mw @ bone("RightArm_").head, "LowerArmR": mw @ bone("RightForeArm").head,
        "ThighL": mw @ bone("LeftUpLeg").head, "ShinL": mw @ bone("LeftLeg").head,
        "ThighR": mw @ bone("RightUpLeg").head, "ShinR": mw @ bone("RightLeg").head,
    }

    # Maillages deformes + poids par piece (attributs de sommet, conserves par la decimation)
    srcs = [o for o in bpy.data.objects if o.type == "MESH" and o.find_armature()]
    merged = []
    for o in srcs:
        names = [g.name for g in o.vertex_groups]
        nv = len(o.data.vertices)
        w = {p: np.zeros(nv, dtype=np.float32) for p in PART_NAMES}
        for v in o.data.vertices:
            tot = 0.0
            for g in v.groups:
                w[part_of_bone(names[g.group])][v.index] += g.weight
                tot += g.weight
            if tot <= 1e-4:
                w["Torso"][v.index] = 1.0
        me = bake_world(o)
        assert len(me.vertices) == nv
        for p in PART_NAMES:
            a = me.attributes.new("w_" + p, "FLOAT", "POINT")
            a.data.foreach_set("value", w[p])
        no = mesh_object("H_" + o.name, me)
        rename_materials(no, {"Suit": "HazmatSuit", "Mask": "HazmatMask", "Glass": "HazmatGlass"})
        # le masque a gaz est tres dense (28 000 sommets) : on allege
        decimate(no, 0.3 if len(me.vertices) > 25000 else (0.6 if len(me.vertices) > 10000 else 1.0))
        merged.append(no)
    bpy.ops.object.select_all(action="DESELECT")
    for o in merged:
        o.select_set(True)
    bpy.context.view_layer.objects.active = merged[0]
    bpy.ops.object.join()
    body = merged[0]
    body.name = "HazmatBody"

    # Repere du jeu : face vers +X, pieds a 0, 1,80 m
    pts = [v.co for v in body.data.vertices]
    minz = min(p.z for p in pts)
    maxz = max(p.z for p in pts)
    scale = 1.80 / (maxz - minz)
    G = Matrix.Diagonal((scale, scale, scale, 1.0)) @ Matrix.Rotation(math.radians(90), 4, "Z") @ Matrix.Translation((0, 0, -minz))
    body.data.transform(G)
    joints = {k: G @ v for k, v in joints_w.items()}
    cx = (joints["ThighL"].x + joints["ThighR"].x) * 0.5
    cy = (joints["ThighL"].y + joints["ThighR"].y) * 0.5
    shift = Matrix.Translation((-cx, -cy, 0))
    body.data.transform(shift)
    joints = {k: shift @ v for k, v in joints.items()}

    # Modele complet : seulement pour l'apercu et la silhouette de l'inventaire (le jeu utilise les pieces)
    full = mesh_object("SM_Hazmat", body.data.copy())
    stale = os.path.join(OUT_MESH, "SM_Hazmat.fbx")
    if os.path.exists(stale):
        os.remove(stale)
    preview([full], "SM_Hazmat")
    render_silhouette(full)
    full.hide_render = True

    # Affectation des faces : piece dominante + recouvrement leger chez l'enfant
    me = body.data
    W = {}
    for p in PART_NAMES:
        a = np.zeros(len(me.vertices), dtype=np.float32)
        me.attributes["w_" + p].data.foreach_get("value", a)
        W[p] = a
    stack = np.stack([W[p] for p in PART_NAMES], 1)
    primary = {p: set() for p in PART_NAMES}
    overlap = {p: set() for p in PART_NAMES}
    for f in me.polygons:
        vs = list(f.vertices)
        s = stack[vs].mean(0)
        main = PART_NAMES[int(np.argmax(s))]
        primary[main].add(f.index)
        for child, parent in HAZMAT_PARENT.items():
            if main == parent and stack[vs, PART_NAMES.index(child)].max() > 0.12:
                overlap[child].add(f.index)

    for p in PART_NAMES:
        part = split_faces(body, primary[p])
        if overlap[p]:
            cover = split_faces(body, overlap[p])
            # copie rentree de 6 mm : invisible au repos, bouche le trou quand le membre tourne
            bm = bmesh.new()
            bm.from_mesh(cover.data)
            bm.normal_update()
            for v in bm.verts:
                v.co -= v.normal * 0.006
            bm.to_mesh(cover.data)
            bm.free()
            bpy.ops.object.select_all(action="DESELECT")
            part.select_set(True)
            cover.select_set(True)
            bpy.context.view_layer.objects.active = part
            bpy.ops.object.join()
        pivot = joints[p]
        set_origin(part, pivot)
        part.name = "SM_Hazmat_" + p
        export_fbx(part, "SM_Hazmat_" + p)
        part.hide_render = True
    JOINTS["Hazmat"] = {k: ue(v) for k, v in joints.items()}
    print("  articulations (cm, Unreal) :", JOINTS["Hazmat"])


def render_silhouette(obj):
    """Silhouette sombre de la combinaison (panneau EQUIPEMENT de l'inventaire)"""
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 32
    sc.render.resolution_x = 384
    sc.render.resolution_y = 768
    sc.render.film_transparent = True
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    world = bpy.data.worlds.new("WS")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.5, 0.5, 0.52, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
    sc.world = world
    pts = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    ctr = (mn + mx) / 2
    cam = link(bpy.data.objects.new("CS", bpy.data.cameras.new("CS")))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = (mx.z - mn.z) * 1.06
    cam.location = ctr + Vector((6.0, -0.4, 0.2))
    cam.rotation_euler = (ctr - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc.camera = cam
    for e, rot in ((0.7, (60, 0, 90)), (4.0, (80, 0, -100))):
        l = link(bpy.data.objects.new("LS", bpy.data.lights.new("LS", "SUN")))
        l.data.energy = e
        l.rotation_euler = [math.radians(a) for a in rot]
    os.makedirs(OUT_ICON, exist_ok=True)
    path = os.path.join(OUT_ICON, "I_Silhouette.png")
    sc.render.filepath = path
    hidden = [o for o in sc.objects if o.type == "MESH" and o is not obj]
    for o in hidden:
        o.hide_render = True
    bpy.ops.render.render(write_still=True)
    for o in hidden:
        o.hide_render = False
    sc.render.film_transparent = False
    from PIL import Image
    im = Image.open(path).convert("RGBA")
    r, g, b, a = im.split()
    lum = Image.merge("RGB", (r, g, b)).convert("L").point(lambda v: int(v * 0.28))
    Image.merge("RGBA", (lum, lum, lum, a)).save(path)
    print("  silhouette", path)


# ---------------------------------------------------------------------------
# Bacteria : creature en fils -> corps, tete, deux bras
# ---------------------------------------------------------------------------
def loose_parts(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    seen = set()
    comps = []
    for v in bm.verts:
        if v.index in seen:
            continue
        stack = [v]
        seen.add(v.index)
        ids = []
        while stack:
            a = stack.pop()
            ids.append(a.index)
            for e in a.link_edges:
                b = e.other_vert(a)
                if b.index not in seen:
                    seen.add(b.index)
                    stack.append(b)
        comps.append(ids)
    bm.free()
    return comps


def process_bacteria():
    path = os.path.join(SRC, "bacteria_recreation.blend")
    if not os.path.isfile(path):
        print("!! introuvable :", path)
        return
    print("== Bacteria")
    bpy.ops.wm.open_mainfile(filepath=path)
    srcs = [o for o in bpy.data.objects if o.type == "MESH"]
    baked = []
    for o in srcs:
        no = mesh_object("B_" + o.name, bake_world(o))
        baked.append(no)
    for o in srcs:
        bpy.data.objects.remove(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in baked:
        o.select_set(True)
    bpy.context.view_layer.objects.active = baked[0]
    bpy.ops.object.join()
    body = baked[0]
    body.data.materials.clear()
    body.data.materials.append(bpy.data.materials.get("BacteriaSkin") or bpy.data.materials.new("BacteriaSkin"))
    for p in body.data.polygons:
        p.material_index = 0

    # 2,75 m de haut, pieds a 0, face vers +X (le modele est plat selon X)
    co = np.array([tuple(v.co) for v in body.data.vertices])
    minz, maxz = co[:, 2].min(), co[:, 2].max()
    s = 2.75 / (maxz - minz)
    G = Matrix.Diagonal((s, s, s, 1.0)) @ Matrix.Translation((-co[:, 0].mean(), 0, -minz))
    body.data.transform(G)
    co = np.array([tuple(v.co) for v in body.data.vertices])
    top = co[:, 2].max()

    comps = loose_parts(body)
    head, arm_pos, arm_neg = set(), set(), set()
    claws = []
    for ids in comps:
        p = co[ids]
        zmin, zmax = p[:, 2].min(), p[:, 2].max()
        ymin, ymax = p[:, 1].min(), p[:, 1].max()
        if zmin > top * 0.83:
            head.update(ids)                                 # tete + halo
        elif zmax > top * 0.7 and zmin < top * 0.22 and max(abs(ymin), abs(ymax)) > 0.62:
            low = p[p[:, 2] < top * 0.3]
            (arm_pos if low[:, 1].mean() > 0 else arm_neg).update(ids)   # long bras jusqu'au sol
        elif zmax < top * 0.18 and min(abs(ymin), abs(ymax)) > 0.42:
            claws.append(ids)                                # griffes au bout des bras
    for ids in claws:
        (arm_pos if co[ids][:, 1].mean() > 0 else arm_neg).update(ids)

    def faces_of(vset):
        return {f.index for f in body.data.polygons if f.vertices[0] in vset}

    head_f, pos_f, neg_f = faces_of(head), faces_of(arm_pos), faces_of(arm_neg)
    body_f = {f.index for f in body.data.polygons} - head_f - pos_f - neg_f

    def pivot_top(vset):
        p = co[list(vset)]
        hi = p[p[:, 2] > p[:, 2].max() - 0.06]
        return Vector(hi.mean(0))

    hp = co[list(head)]
    head_pivot = Vector((hp[:, 0].mean(), hp[:, 1].mean(), hp[:, 2].min()))
    pieces = [("Body", body_f, Vector((0, 0, 0))), ("Head", head_f, head_pivot),
              ("ArmL", pos_f, pivot_top(arm_pos)), ("ArmR", neg_f, pivot_top(arm_neg))]
    objs = []
    joints = {}
    for name, faces, pivot in pieces:
        part = split_faces(body, faces)
        set_origin(part, pivot)
        part.name = "SM_BacteriaET_" + name
        export_fbx(part, "SM_BacteriaET_" + name)
        part.location = pivot
        objs.append(part)
        joints[name] = ue(pivot)
    body.hide_render = True
    preview(objs, "SM_BacteriaET", Vector((1.0, -0.9, 0.3)))
    JOINTS["Bacteria"] = joints
    print("  articulations (cm, Unreal) :", joints)


# ---------------------------------------------------------------------------
# Deathmoth : papillon de nuit scanne -> corps + deux ailes
# ---------------------------------------------------------------------------
def process_moth():
    path = os.path.join(SRC, "peppered moth.obj")
    if not os.path.isfile(path):
        print("!! introuvable :", path)
        return
    print("== Deathmoth")
    reset()
    bpy.ops.wm.obj_import(filepath=path)
    src = [o for o in bpy.data.objects if o.type == "MESH"][0]
    for m in src.data.materials:
        if m and m.node_tree:
            for n in m.node_tree.nodes:
                if n.type == "TEX_IMAGE" and n.image and n.image.size[0] > 0:
                    save_texture(n.image, "T_Deathmoth")
    moth = mesh_object("Moth", bake_world(src))
    bpy.data.objects.remove(src)
    rename_materials(moth, {m.name: "MothTex" for m in moth.data.materials if m})
    decimate(moth, 0.35)

    # Alignement par analyse en composantes principales : envergure -> Y, corps -> X (tete vers +X), epaisseur -> Z
    co = np.array([tuple(v.co) for v in moth.data.vertices])
    c = co.mean(0)
    w, vec = np.linalg.eigh(np.cov((co - c).T))
    vec = vec[:, np.argsort(w)[::-1]]
    P = (co - c) @ vec
    span = np.ptp(P[:, 0])
    body = np.abs(P[:, 0]) < 0.08 * span
    b = P[body]
    # tete : du cote des ailes anterieures (verifie sur le scan fourni)
    hi, lo = b[b[:, 1] > np.percentile(b[:, 1], 95)], b[b[:, 1] < np.percentile(b[:, 1], 5)]
    head_sign = -1.0 if np.ptp(hi[:, 0]) < np.ptp(lo[:, 0]) else 1.0
    ax_x = vec[:, 1] * head_sign
    ax_y = vec[:, 0]
    ax_z = np.cross(ax_x, ax_y)
    # ailes relevees en V : le bout des ailes doit etre au-dessus du corps
    Pz = (co - c) @ ax_z
    tips = np.abs(P[:, 0]) > 0.4 * span
    if Pz[tips].mean() < Pz[body].mean():
        ax_y, ax_z = -ax_y, -ax_z
    R = Matrix(((*ax_x, 0), (*ax_y, 0), (*ax_z, 0), (0, 0, 0, 1)))
    s = 1.5 / span                                          # 1,50 m d'envergure
    moth.data.transform(Matrix.Diagonal((s, s, s, 1)) @ R @ Matrix.Translation(Vector(-c)))
    co = np.array([tuple(v.co) for v in moth.data.vertices])
    half = 0.08                                             # demi-largeur du thorax (m)
    side = {}
    for f in moth.data.polygons:
        y = co[list(f.vertices)][:, 1].mean()
        side[f.index] = "WingL" if y > half else ("WingR" if y < -half else "Body")
    joints = {}
    objs = []
    for name in ("Body", "WingL", "WingR"):
        faces = {i for i, sd in side.items() if sd == name}
        part = split_faces(moth, faces)
        if name == "Body":
            pivot = Vector((0, 0, 0))
        else:
            sgn = 1.0 if name == "WingL" else -1.0
            root = co[(np.abs(co[:, 1] - sgn * half) < 0.03)]
            pivot = Vector((root[:, 0].mean(), sgn * half, root[:, 2].mean())) if len(root) else Vector((0, sgn * half, 0))
        set_origin(part, pivot)
        part.name = "SM_DeathmothET_" + name
        export_fbx(part, "SM_DeathmothET_" + name)
        part.location = pivot
        objs.append(part)
        joints[name] = ue(pivot)
    moth.hide_render = True
    preview(objs, "SM_DeathmothET", Vector((0.6, -0.8, 0.9)))
    JOINTS["Deathmoth"] = joints
    print("  articulations (cm, Unreal) :", joints)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    todo = args or ["hazmat", "bacteria", "moth"]
    if "hazmat" in todo:
        process_hazmat()
    if "bacteria" in todo:
        process_bacteria()
    if "moth" in todo:
        process_moth()
    out = os.path.join(OUT_MESH, "user_models.json")
    old = {}
    if os.path.isfile(out):
        with open(out) as fh:
            old = json.load(fh)
    old.update(JOINTS)
    with open(out, "w") as fh:
        json.dump(old, fh, indent=2)
    print("articulations ->", out)
