"""
Preparation des modeles fournis par l'utilisateur pour le jeu (decoupage en pieces animables).

Placez les fichiers sources dans Tools/SourceModels/ :
    bacteria_recreation.blend                 (Bacteria, archive "bacteria-lifeform-backrooms.zip")
    peppered moth.obj + texture_0_baseColor.png (Deathmoth, archive "deathmoth-backrooms.zip")
    asyc_hazmat.glb                           (combinaison hazmat du joueur, archive "asyc_hazmat.rar")
    skin_stealer.usdz                         (Skin-Stealer, "Skin_Stealer_The_Backrooms_Blender_3.usdz")
    faceling.glb                              (Faceling style PS1, archive "backrooms-faceling-ps1psx-style.zip")
    partygoer.fbx + partygoer_BaseColor.jpeg  (Partygoer, archive "partygoer-from-backrooms-updated.zip")
    hound.blend + hound_Material.png          (Hound, archive "hound-backrooms.zip")
    backrooms_lvl4_office.glb                 (scene du Niveau 4, "backrooms-level-4-abandoned-office.zip")
    poolrooms/pooltile_1.png, pooltile_n_0.png, plaster_4.png, plaster_n_3.png   (scene "poolrooms.zip")
Le Smiler et le Clump sont modelises ici d'apres les images de reference fournies (aucun fichier source).

Puis :
    python Tools/Blender/import_user_models.py          (module pip "bpy")
    blender -b -P Tools/Blender/import_user_models.py   (avec Blender installe)

Sorties :
    RawAssets/Meshes/SM_Hazmat_*.fbx, SM_BacteriaET_*.fbx, SM_DeathmothET_*.fbx, SM_SkinStealerET_*.fbx,
        SM_FacelingET_*.fbx, SM_PartygoerET_*.fbx, SM_HoundET_*.fbx, SM_SmilerET.fbx, SM_ClumpET_*.fbx
    RawAssets/Textures/T_Hazmat_Suit.jpg, T_Hazmat_Mask.jpg, T_Deathmoth.jpg, T_SkinStealer_*.jpg, T_Faceling.jpg,
        T_Partygoer.jpg, T_Hound.jpg
    v3.8 : SM_OfficeDeskET.fbx, SM_OfficeChairET.fbx, SM_WaterCoolerET.fbx ; T_PoolTile37, T_Plaster (+ _N),
        T_OfficeCarpetNavy, T_OfficeCeiling (+ _N)
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


def save_texture(image, name, size=None, quality=95):
    """Image Blender (eventuellement empaquetee dans le .glb) -> JPG a sa resolution d'origine (ou reduite a size)"""
    from PIL import Image
    w, h = image.size
    px = np.empty(w * h * 4, dtype=np.float32)
    image.pixels.foreach_get(px)
    arr = (np.clip(px.reshape(h, w, 4)[::-1, :, :3], 0, 1) * 255 + 0.5).astype(np.uint8)
    im = Image.fromarray(arr, "RGB")
    if size and max(w, h) > size:
        im = im.resize((size, size), Image.LANCZOS)
    os.makedirs(OUT_TEX, exist_ok=True)
    path = os.path.join(OUT_TEX, name + ".jpg")
    im.save(path, quality=quality, subsampling=0)
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
    try:
        sc.render.image_settings.media_type = "IMAGE"  # Blender 4.5+ (certaines scenes fournies sont reglees en video)
    except Exception:
        pass
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
        # maillage complet (v3.9) : le masque a gaz garde ses 28 000 sommets (Nanite dans Unreal)
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
    # maillage scanne complet (v3.9, Nanite dans Unreal)

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


# ---------------------------------------------------------------------------
# Modeles fournis (v3.5) : Skin-Stealer, Faceling, Partygoer (humanoides) et Hound (quadrupede).
# Tous ont un squelette : chaque face va a la piece de l'os qui la deforme le plus.
# ---------------------------------------------------------------------------
HUMAN_PARTS = ["Torso", "Head", "UpperArmL", "LowerArmL", "UpperArmR", "LowerArmR", "ThighL", "ShinL", "ThighR", "ShinR"]
HOUND_PARTS = ["Body", "Head", "FrontUpperL", "FrontLowerL", "FrontUpperR", "FrontLowerR",
               "BackUpperL", "BackLowerL", "BackUpperR", "BackLowerR"]
HOUND_PARENT = {"Head": "Body", "FrontUpperL": "Body", "FrontUpperR": "Body", "BackUpperL": "Body", "BackUpperR": "Body",
                "FrontLowerL": "FrontUpperL", "FrontLowerR": "FrontUpperR", "BackLowerL": "BackUpperL", "BackLowerR": "BackUpperR"}


def load_model(path):
    reset()
    ext = os.path.splitext(path)[1].lower()
    if ext == ".blend":
        bpy.ops.wm.open_mainfile(filepath=path)
    elif ext in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=path)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=path)
    else:
        bpy.ops.wm.usd_import(filepath=path)
    for o in list(bpy.data.objects):
        if o.type == "MESH" and o.name.startswith("Icosphere"):
            bpy.data.objects.remove(o)
    bpy.context.view_layer.update()


def save_texture_file(src, name, size=None, quality=95):
    """Image fournie -> RawAssets/Textures/<name>.jpg, a sa resolution d'origine (ou reduite a size)"""
    from PIL import Image
    im = Image.open(src).convert("RGB")
    if size and max(im.size) > size:
        im = im.resize((size, size), Image.LANCZOS)
    os.makedirs(OUT_TEX, exist_ok=True)
    path = os.path.join(OUT_TEX, name + ".jpg")
    im.save(path, quality=quality, subsampling=0)
    print("  texture", path, im.size)


def apply_shape_modifiers(o):
    """Applique miroir / subdivision / epaisseur (tout sauf l'armature) : les groupes de sommets suivent"""
    if not [m for m in o.modifiers if m.type != "ARMATURE"]:
        return
    o.data = o.data.copy()
    bpy.ops.object.select_all(action="DESELECT")
    o.hide_set(False)
    o.hide_viewport = False
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    for m in list(o.modifiers):
        if m.type != "ARMATURE":
            bpy.ops.object.modifier_apply(modifier=m.name)


def single_uv(o):
    """Une seule couche UV nommee UVMap (sinon la jonction des maillages melange les couches)"""
    uvs = o.data.uv_layers
    if len(uvs) == 0:
        uvs.new(name="UVMap")
        return
    keep = uvs.active or uvs[0]
    for l in [l for l in uvs if l != keep]:
        uvs.remove(l)
    uvs[0].name = "UVMap"


def thin_strands(o, keep):
    """Cheveux en meches separees : on n'en garde qu'une sur 'keep' (avant le calcul des poids)"""
    comps = loose_parts(o)
    if len(comps) < 20:
        return
    drop = set()
    for i, ids in enumerate(sorted(comps, key=lambda c: min(c))):
        if i % keep:
            drop.update(ids)
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.verts[i] for i in drop], context="VERTS")
    bm.to_mesh(o.data)
    bm.free()
    print("  meches :", len(comps), "->", len(comps) - len([1 for i in range(len(comps)) if i % keep]))


def isolated_weights(o, part_of):
    """Sommet dont la piece dominante n'est celle d'aucun de ses voisins (ex. un doigt lie au cou dans le modele
    fourni) : il reprend les poids du voisin le plus representatif. Sinon il resterait en place quand le bras bouge."""
    names = [g.name for g in o.vertex_groups]
    me = o.data

    def dominant(v):
        tot = {}
        for g in v.groups:
            part = part_of(names[g.group].split("/")[-1])
            if part:
                tot[part] = tot.get(part, 0.0) + g.weight
        return max(tot, key=tot.get) if tot else None

    nbr = [[] for _ in me.vertices]
    for e in me.edges:
        a, b = e.vertices
        nbr[a].append(b)
        nbr[b].append(a)
    fixed = 0
    for _ in range(3):
        dom = [dominant(v) for v in me.vertices]
        todo = []
        for v in me.vertices:
            ns = nbr[v.index]
            if not ns or dom[v.index] is None or any(dom[n] == dom[v.index] for n in ns):
                continue
            counts = {}
            for n in ns:
                if dom[n]:
                    counts[dom[n]] = counts.get(dom[n], 0) + 1
            if not counts:
                continue
            best = max(counts, key=counts.get)
            src = next(n for n in ns if dom[n] == best)
            todo.append((v.index, [(g.group, g.weight) for g in me.vertices[src].groups]))
        for vi, ws in todo:
            for g in list(me.vertices[vi].groups):
                o.vertex_groups[g.group].remove([vi])
            for gi, w in ws:
                o.vertex_groups[gi].add([vi], w, "REPLACE")
        fixed += len(todo)
        if not todo:
            break
    if fixed:
        print("  %s : %d sommets isoles rattaches a leurs voisins" % (o.name, fixed))


def clean_weights(o, part_of, parent):
    """Poids parasites du modele d'origine (ex. un sommet de l'epaule lie a la main) : invisibles bras en T, ils
    etirent le maillage une fois les bras baisses. Un sommet ne garde que sa piece dominante et ses voisines."""
    isolated_weights(o, part_of)
    adj = {}
    for c, p in parent.items():
        adj.setdefault(c, set()).add(p)
        adj.setdefault(p, set()).add(c)
    names = [g.name for g in o.vertex_groups]
    bad = []
    for v in o.data.vertices:
        tot = {}
        for g in v.groups:
            part = part_of(names[g.group].split("/")[-1])
            if part:
                tot[part] = tot.get(part, 0.0) + g.weight
        if len(tot) < 2:
            continue
        main = max(tot, key=tot.get)
        ok = {main} | adj.get(main, set())
        for g in v.groups:
            part = part_of(names[g.group].split("/")[-1])
            if part and part not in ok and g.weight > 0.0:
                bad.append((g.group, v.index))
    for gi, vi in bad:
        o.vertex_groups[gi].remove([vi])
    if bad:
        print("  %s : %d poids parasites retires" % (o.name, len(bad)))


def drop_orphans(o):
    """Sommets sans poids dans un maillage rigge : ils resteraient a la pose d'origine (bras en T) et tireraient
    de longues pointes une fois les bras baisses. S'il y en a peu, on les supprime."""
    if not o.vertex_groups:
        return
    orphans = [v.index for v in o.data.vertices if sum(g.weight for g in v.groups) <= 1e-4]
    if not orphans or len(orphans) > 0.05 * len(o.data.vertices):
        return
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.verts[i] for i in orphans], context="VERTS")
    bm.to_mesh(o.data)
    bm.free()
    print("  %s : %d sommets sans os retires" % (o.name, len(orphans)))


def rigged_body(arm, part_of, parts, default, mat_map, default_slot, skip=(), budget=12000, drop_below=None, thin=None, parent=None):
    """Maillages du squelette, deformes (pose courante), joints en un seul objet. Attributs w_<piece> = poids."""
    srcs = [o for o in bpy.data.objects if o.type == "MESH" and o.find_armature() == arm and o.name not in skip
            and len(o.data.vertices) > 0]
    merged = []
    for o in srcs:
        apply_shape_modifiers(o)
        if thin and o.name in thin:
            thin_strands(o, thin[o.name])
        drop_orphans(o)
        if parent:
            clean_weights(o, part_of, parent)
        names = [g.name for g in o.vertex_groups]
        nv = len(o.data.vertices)
        w = {p: np.zeros(nv, dtype=np.float32) for p in parts}
        for v in o.data.vertices:
            tot = 0.0
            for g in v.groups:
                part = part_of(names[g.group].split("/")[-1])
                if part and g.weight > 0.0:
                    w[part][v.index] += g.weight
                    tot += g.weight
            if tot <= 1e-4:
                w[default][v.index] = 1.0
        me = bake_world(o)
        assert len(me.vertices) == nv, (o.name, len(me.vertices), nv)
        for part in parts:
            a = me.attributes.new("w_" + part, "FLOAT", "POINT")
            a.data.foreach_set("value", w[part])
        no = mesh_object("R_" + o.name, me)
        single_uv(no)
        for slot in no.material_slots:
            src_name = slot.material.name if slot.material else ""
            new = mat_map.get(src_name, default_slot)
            slot.material = bpy.data.materials.get(new) or bpy.data.materials.new(new)
        if not no.material_slots:
            no.data.materials.append(bpy.data.materials.get(default_slot) or bpy.data.materials.new(default_slot))
        merged.append(no)
    for o in srcs:
        o.hide_render = True
    bpy.ops.object.select_all(action="DESELECT")
    for o in merged:
        o.select_set(True)
    bpy.context.view_layer.objects.active = merged[0]
    bpy.ops.object.join()
    body = merged[0]
    if drop_below is not None:
        # geometrie parasite loin sous les pieds (sol de la scene d'origine)
        bm = bmesh.new()
        bm.from_mesh(body.data)
        dead = [f for f in bm.faces if f.calc_center_median().z < drop_below]
        bmesh.ops.delete(bm, geom=dead, context="FACES")
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
        bm.to_mesh(body.data)
        bm.free()
    nv = len(body.data.vertices)
    for _ in range(4 if budget else 0):
        cur = len(body.data.vertices)
        if cur <= budget * 1.08:
            break
        decimate(body, max(0.2, budget / cur))
    print("  maillage", nv, "->", len(body.data.vertices), "sommets")
    return body


def to_game_frame(body, joints, forward, height, center_parts=None):
    """Tourne le modele pour qu'il regarde vers +X, pieds a 0, mis a la taille voulue (m)"""
    yaw = math.atan2(forward.y, forward.x)
    R = Matrix.Rotation(-yaw, 4, "Z")
    body.data.transform(R)
    joints = {k: R @ v for k, v in joints.items()}
    co = np.array([tuple(v.co) for v in body.data.vertices])
    minz, maxz = co[:, 2].min(), co[:, 2].max()
    s = height / (maxz - minz)
    G = Matrix.Diagonal((s, s, s, 1.0)) @ Matrix.Translation((0, 0, -minz))
    body.data.transform(G)
    joints = {k: G @ v for k, v in joints.items()}
    if center_parts:
        c = sum((joints[k] for k in center_parts), Vector()) / len(center_parts)
        T = Matrix.Translation((-c.x, -c.y, 0))
        body.data.transform(T)
        joints = {k: T @ v for k, v in joints.items()}
        G = T @ G
    return joints, G @ R


def drop_unused_materials(o):
    """Une piece n'emporte que les materiaux qu'elle utilise (moins d'emplacements dans Unreal)"""
    used = sorted({p.material_index for p in o.data.polygons})
    mats = [o.data.materials[i] for i in used]
    remap = {old: new for new, old in enumerate(used)}
    idx = [remap[p.material_index] for p in o.data.polygons]
    o.data.materials.clear()
    for m in mats:
        o.data.materials.append(m)
    for p, i in zip(o.data.polygons, idx):
        p.material_index = i


def export_parts(body, joints, parts, parent, prefix, budget_note=""):
    """Decoupe par piece dominante (+ recouvrement rentre chez l'enfant), pivot sur l'articulation, export FBX"""
    me = body.data
    W = {}
    for part in parts:
        a = np.zeros(len(me.vertices), dtype=np.float32)
        me.attributes["w_" + part].data.foreach_get("value", a)
        W[part] = a
    stack = np.stack([W[part] for part in parts], 1)
    primary = {part: set() for part in parts}
    overlap = {part: set() for part in parts}
    for f in me.polygons:
        vs = list(f.vertices)
        sm = stack[vs].mean(0)
        main = parts[int(np.argmax(sm))]
        primary[main].add(f.index)
        for child, par in parent.items():
            if main == par and stack[vs, parts.index(child)].max() > 0.12:
                overlap[child].add(f.index)
    objs = []
    for part in parts:
        if not primary[part]:
            print("  !! piece vide :", part)
            continue
        piece = split_faces(body, primary[part])
        if overlap[part]:
            cover = split_faces(body, overlap[part])
            bm = bmesh.new()
            bm.from_mesh(cover.data)
            bm.normal_update()
            for v in bm.verts:
                v.co -= v.normal * 0.006
            bm.to_mesh(cover.data)
            bm.free()
            bpy.ops.object.select_all(action="DESELECT")
            piece.select_set(True)
            cover.select_set(True)
            bpy.context.view_layer.objects.active = piece
            bpy.ops.object.join()
        pivot = joints[part]
        set_origin(piece, pivot)
        piece.name = prefix + "_" + part
        drop_unused_materials(piece)
        export_fbx(piece, prefix + "_" + part)
        piece.location = pivot
        objs.append(piece)
    return objs


def humanoid(key, prefix, part_of, joint_bones, mat_map, default_slot, height, arm_drop=80.0, elbow=12.0, budget=9000,
             skip=(), extra=None):
    """Humanoide a squelette -> pieces Torso, Head, UpperArmL/R, LowerArmL/R, ThighL/R, ShinL/R (pose bras le long du corps)"""
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    pbs = arm.pose.bones
    mw = arm.matrix_world

    def hw(b):
        return mw @ pbs[b].head

    def tw(b):
        return mw @ pbs[b].tail

    up = Vector((0, 0, 1))
    left = hw(joint_bones["UpperArmL"]) - hw(joint_bones["UpperArmR"])
    left.z = 0
    left.normalize()
    forward = left.cross(up).normalized()
    # Bras le long du corps (modeles livres en T ou en A), avant-bras un peu plies vers l'avant
    for side, sgn in (("L", -1.0), ("R", 1.0)):
        b = joint_bones["UpperArm" + side]
        d = tw(b) - hw(b)
        cur = math.degrees(math.atan2(-d.z, Vector((d.x, d.y, 0)).length))
        rotate_pose_bone(arm, pbs[b], forward, math.radians(sgn * (arm_drop - cur)))
        rotate_pose_bone(arm, pbs[joint_bones["LowerArm" + side]], left, math.radians(-elbow))
    bpy.context.view_layer.update()
    joints_w = {part: hw(b) for part, b in joint_bones.items()}

    body = rigged_body(arm, part_of, HUMAN_PARTS, "Torso", mat_map, default_slot, skip=skip, budget=budget, parent=HAZMAT_PARENT)
    joints, M = to_game_frame(body, joints_w, forward, height, center_parts=("ThighL", "ThighR"))
    full = mesh_object(prefix, body.data.copy())
    preview([full], prefix, Vector((1.0, -0.55, 0.3)))
    full.hide_render = True
    if extra:
        extra(M, joints)
    export_parts(body, joints, HUMAN_PARTS, HAZMAT_PARENT, prefix)
    body.hide_render = True
    JOINTS[key] = {k: ue(v) for k, v in joints.items()}
    print("  articulations (cm, Unreal) :", JOINTS[key])


def process_skinstealer():
    path = os.path.join(SRC, "skin_stealer.usdz")
    if not os.path.isfile(path):
        print("!! introuvable :", path)
        return
    print("== Skin-Stealer")
    load_model(path)
    # Os sans nom (n15..n67) : colonne n15-17, tete n18-23, bras n24 / n42 (+ main et doigts), jambes n60 / n64
    groups = [((15, 17), "Torso"), ((18, 23), "Head"), ((24, 24), "UpperArmL"), ((25, 41), "LowerArmL"), ((42, 42), "UpperArmR"),
              ((43, 59), "LowerArmR"), ((60, 60), "ThighL"), ((61, 63), "ShinL"), ((64, 64), "ThighR"), ((65, 67), "ShinR")]

    def part_of(b):
        if not (b.startswith("n") and b[1:].isdigit()):
            return None
        n = int(b[1:])
        for (lo, hi), part in groups:
            if lo <= n <= hi:
                return part
        return None

    joints = {"Torso": "n15", "Head": "n18", "UpperArmL": "n24", "LowerArmL": "n25", "UpperArmR": "n42", "LowerArmR": "n43",
              "ThighL": "n60", "ShinL": "n61", "ThighR": "n64", "ShinR": "n65"}
    tex = os.path.join(SRC, "skin_tex", "0")
    if not os.path.isdir(tex):
        import zipfile
        with zipfile.ZipFile(path) as z:
            z.extractall(os.path.join(SRC, "skin_tex"))
    save_texture_file(os.path.join(tex, "Material.006_baseColor.jpg"), "T_SkinStealer_Flesh")
    save_texture_file(os.path.join(tex, "Material.003_baseColor.jpg"), "T_SkinStealer_Claw")
    save_texture_file(os.path.join(tex, "Material.004_baseColor.jpg"), "T_SkinStealer_Eye")
    mats = {"Material_001": "EyeDark", "Material_002": "SkinStealerFlesh", "Material_003": "StealerClaw", "Material_004": "StealerEye",
            "Material_005": "StealerEye", "Material_006": "SkinStealerFlesh", "Material_007": "SkinStealerFlesh",
            "Material_008": "SkinStealerFlesh"}
    humanoid("SkinStealer", "SM_SkinStealerET", part_of, joints, mats, "SkinStealerFlesh", 2.05, arm_drop=82.0, budget=None)


def process_faceling():
    path = os.path.join(SRC, "faceling.glb")
    if not os.path.isfile(path):
        print("!! introuvable :", path)
        return
    print("== Faceling")
    load_model(path)

    def part_of(b):
        b = b.lower()
        if b.startswith("neck") or b.startswith("head"):
            return "Head"
        for s in ("l", "r"):
            S = s.upper()
            if b.startswith("shoulder." + s):
                return "UpperArm" + S
            if b.startswith("foearm." + s) or b.startswith("forearm." + s) or b.startswith("hand." + s):
                return "LowerArm" + S
            if b.startswith("thigh." + s):
                return "Thigh" + S
            if b.startswith("calf." + s) or b.startswith("foot." + s):
                return "Shin" + S
        return "Torso"

    joints = {"Torso": "spine1_11", "Head": "neck_1", "UpperArmL": "shoulder.L_3", "LowerArmL": "foearm.L_2",
              "UpperArmR": "shoulder.R_5", "LowerArmR": "foearm.R_4", "ThighL": "thigh.L_8", "ShinL": "calf.L_7",
              "ThighR": "thigh.R_10", "ShinR": "calf.R_9"}
    # texture integree au .glb (le PNG livre a cote est retourne verticalement par rapport aux UV)
    for img in bpy.data.images:
        if img.size[0] > 0:
            save_texture(img, "T_Faceling")
            break
    humanoid("Faceling", "SM_FacelingET", part_of, joints, {"H_Body": "FacelingTex"}, "FacelingTex", 1.78, arm_drop=80.0, budget=None)


def process_partygoer():
    path = os.path.join(SRC, "partygoer.fbx")
    if not os.path.isfile(path):
        print("!! introuvable :", path)
        return
    print("== Partygoer")
    load_model(path)
    fingers = ("Thumb", "Index", "Mid", "Ring", "Pinky")

    def part_of(b):
        if any(k in b for k in ("Head", "Neck", "Jaw", "Eye", "Tongue", "Teeth", "Facial")):
            return "Head"
        for S in ("L", "R"):
            if "_%s_Upperarm" % S in b:
                return "UpperArm" + S
            if any("_%s_%s" % (S, k) in b for k in ("Forearm", "Hand", "Elbow") + fingers):
                return "LowerArm" + S
            if "_%s_Thigh" % S in b:
                return "Thigh" + S
            if any("_%s_%s" % (S, k) in b for k in ("Calf", "Foot", "Toe", "Knee")):
                return "Shin" + S
        return "Torso"

    joints = {"Torso": "CC_Base_Hip", "Head": "CC_Base_NeckTwist01", "UpperArmL": "CC_Base_L_Upperarm",
              "LowerArmL": "CC_Base_L_Forearm", "UpperArmR": "CC_Base_R_Upperarm", "LowerArmR": "CC_Base_R_Forearm",
              "ThighL": "CC_Base_L_Thigh", "ShinL": "CC_Base_L_Calf", "ThighR": "CC_Base_R_Thigh", "ShinR": "CC_Base_R_Calf"}
    save_texture_file(os.path.join(SRC, "partygoer_BaseColor.jpeg"), "T_Partygoer")
    balloon = bpy.data.objects.get("Baloon red")

    def export_balloon(M, joints_game):
        # Ballon a part (non rigge) : pivot au bout de la ficelle, tenu par la main droite en jeu
        if not balloon:
            return
        me = bake_world(balloon)
        me.transform(M)
        b = mesh_object("Balloon", me)
        single_uv(b)
        b.data.materials.clear()
        b.data.materials.append(bpy.data.materials.get("Balloon") or bpy.data.materials.new("Balloon"))
        for poly in b.data.polygons:
            poly.material_index = 0
        co = np.array([tuple(v.co) for v in b.data.vertices])
        low = co[co[:, 2] < co[:, 2].min() + 0.02]
        pivot = Vector(low.mean(0))
        set_origin(b, pivot)
        b.name = "SM_PartygoerET_Balloon"
        export_fbx(b, "SM_PartygoerET_Balloon")
        b.hide_render = True
        JOINTS.setdefault("PartygoerBalloon", {})["Height"] = round(float((co[:, 2].max() - co[:, 2].min()) * 100.0), 2)

    humanoid("Partygoer", "SM_PartygoerET", part_of, joints, {"Partygoer_LP.003": "PartygoerTex"}, "PartygoerTex", 1.92,
             arm_drop=80.0, budget=None, skip=("Baloon red",), extra=export_balloon)


def process_hound():
    path = os.path.join(SRC, "hound.blend")
    if not os.path.isfile(path):
        print("!! introuvable :", path)
        return
    print("== Hound")
    load_model(path)
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    pbs = arm.pose.bones
    mw = arm.matrix_world

    def hw(b):
        return mw @ pbs[b].head

    up = Vector((0, 0, 1))
    forward = hw("Bone.003") - hw("Bone")
    forward.z = 0
    forward.normalize()
    left = up.cross(forward)
    center = (hw("Bone.025") + hw("Bone.029") + hw("Bone.017") + hw("Bone.021")) / 4

    def side(b):
        return "L" if (hw(b) - center).dot(left) > 0 else "R"

    legs = {"Front": ("Bone.025", "Bone.029"), "Back": ("Bone.017", "Bone.021")}
    lower = {"Bone.025": ("Bone.026", "Bone.027", "Bone.028", "Bone.030"), "Bone.029": ("Bone.031", "Bone.032", "Bone.033", "Bone.034"),
             "Bone.017": ("Bone.018", "Bone.019", "Bone.020"), "Bone.021": ("Bone.022", "Bone.023", "Bone.024")}
    bone_part = {"Bone": "Body", "Bone.001": "Body", "Bone.002": "Body"}
    joints_w = {"Body": hw("Bone.002"), "Head": hw("Bone.003")}
    for end, (a, b) in legs.items():
        for upper_bone in (a, b):
            S = side(upper_bone)
            bone_part[upper_bone] = end + "Upper" + S
            for lb in lower[upper_bone]:
                bone_part[lb] = end + "Lower" + S
            joints_w[end + "Upper" + S] = hw(upper_bone)
            joints_w[end + "Lower" + S] = hw(lower[upper_bone][0])

    def part_of(b):
        if b in bone_part:
            return bone_part[b]
        return "Head" if b.startswith("Bone.0") else None

    mats = {"Material.002": "HoundSkin", "Material": "HoundHair", "Fur Material": "HoundHair", "Material.001": "HoundFace",
            "Material.003": "HoundMouth", "Material.004": "GlowAmberEye", "Material.007": "EyeDark", "Material.005": "HoundTongue",
            "Material.006": "HoundTeeth"}
    save_texture_file(os.path.join(SRC, "hound_Material.png"), "T_Hound")
    feet = min(hw(b).z for b in ("Bone.028", "Bone.033", "Bone.020", "Bone.024"))
    body = rigged_body(arm, part_of, HOUND_PARTS, "Body", mats, "HoundSkin", skip=("Cube.007",), budget=None, drop_below=feet - 0.6,
                       parent=HOUND_PARENT)
    joints, _ = to_game_frame(body, joints_w, forward, 1.15, center_parts=("FrontUpperL", "FrontUpperR", "BackUpperL", "BackUpperR"))
    full = mesh_object("SM_HoundET", body.data.copy())
    preview([full], "SM_HoundET", Vector((0.7, -1.0, 0.35)))
    full.hide_render = True
    export_parts(body, joints, HOUND_PARTS, HOUND_PARENT, "SM_HoundET")
    body.hide_render = True
    JOINTS["Hound"] = {k: ue(v) for k, v in joints.items()}
    print("  articulations (cm, Unreal) :", JOINTS["Hound"])


# ---------------------------------------------------------------------------
# Smiler et Clump refaits d'apres les images fournies (pas de modele source : construction procedurale)
# ---------------------------------------------------------------------------
def mat_slot(obj, name):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    if name not in [x.name for x in obj.data.materials]:
        obj.data.materials.append(m)
    return [x.name for x in obj.data.materials].index(name)


def add_geometry(bm, verts, faces, mat):
    vs = [bm.verts.new(v) for v in verts]
    for f in faces:
        try:
            face = bm.faces.new([vs[i] for i in f])
            face.material_index = mat
        except ValueError:
            pass


def spike(bm, base, tip, width, depth, mat, twist=0.0):
    """Dent / pointe : pyramide a base losange, de base vers tip"""
    axis = (tip - base).normalized()
    side = axis.cross(Vector((1, 0, 0)))
    if side.length < 1e-3:
        side = axis.cross(Vector((0, 1, 0)))
    side.normalize()
    side = Matrix.Rotation(twist, 4, axis) @ side
    out = axis.cross(side).normalized()
    v = [base + side * width, base + out * depth, base - side * width, base - out * depth, tip]
    add_geometry(bm, v, [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4), (3, 2, 1, 0)], mat)


def lumpy_sphere(bm, radius, subdiv, amp, mat, seed, squash=(1.0, 1.0, 1.0)):
    rng = np.random.default_rng(seed)
    ret = bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=radius)
    centers = [Vector(rng.normal(size=3)).normalized() for _ in range(14)]
    for v in ret["verts"]:
        n = v.co.normalized()
        bump = sum(math.exp(-((n - c).length ** 2) / 0.12) for c in centers) * amp
        v.co = Vector((n.x * squash[0], n.y * squash[1], n.z * squash[2])) * (radius + bump + rng.uniform(-amp, amp) * 0.25)
    for f in bm.faces:
        if f.verts[0] in set(ret["verts"]):
            f.material_index = mat
    return ret["verts"]


def process_smiler():
    """Smiler (image fournie) : deux grands yeux ovales lumineux et un sourire de dents fines et irregulieres,
    flottant dans une masse noire a peine visible. Origine au centre, regard vers +X."""
    print("== Smiler")
    reset()
    me = bpy.data.meshes.new("SmilerET")
    obj = mesh_object("SM_SmilerET", me)
    dark = mat_slot(obj, "SmilerDark")
    glow = mat_slot(obj, "Glow")
    rng = np.random.default_rng(7)
    bm = bmesh.new()
    # Masse sombre (on la devine a la lampe)
    head = lumpy_sphere(bm, 0.40, 4, 0.03, dark, 3, squash=(0.8, 1.0, 1.0))
    # visage lisse (pas de bosses devant) : les yeux et le sourire posent sur l'ellipsoide
    for v in head:
        n = Vector((v.co.x / 0.8, v.co.y, v.co.z)).normalized()
        w = min(1.0, max(0.0, (n.x - 0.15) / 0.35))
        v.co = v.co.lerp(Vector((n.x * 0.8, n.y, n.z)) * 0.40, w)
    # Yeux : grands ovales legerement inclines vers l'exterieur
    for sgn in (-1.0, 1.0):
        r = bmesh.ops.create_uvsphere(bm, u_segments=14, v_segments=8, radius=1.0)
        M = (Matrix.Translation((0.30, sgn * 0.165, 0.12)) @ Matrix.Rotation(sgn * math.radians(-14), 4, "X")
             @ Matrix.Diagonal((0.035, 0.075, 0.058, 1.0)))
        bmesh.ops.transform(bm, matrix=M, verts=r["verts"])
        for f in {f for v in r["verts"] for f in v.link_faces}:
            f.material_index = glow

    # Sourire : levres en croissant (coins hauts, centre bas), sur la face bombee
    W = 0.30

    def front(y, z):
        # sur la surface de la masse (ellipsoide 0,32 x 0,40 x 0,40) : le sourire l'enveloppe sans en sortir
        return 0.32 * math.sqrt(max(0.0, 1.0 - (y / 0.40) ** 2 - (z / 0.40) ** 2)) + 0.012

    def upper(y):
        # coins releves jusque sous les yeux, centre bas : un croissant
        return 0.05 - 0.20 * max(0.0, 1.0 - (y / W) ** 2) ** 1.2

    def lower(y):
        t = max(0.0, 1.0 - (y / W) ** 2)
        return upper(y) - 0.12 * t ** 0.7

    # fond de bouche noir (cache l'interieur)
    n = 24
    strip = []
    for i in range(n + 1):
        y = -W + 2 * W * i / n
        zu, zl = upper(y) + 0.01, lower(y) - 0.01
        strip += [Vector((front(y, zu) - 0.008, y, zu)), Vector((front(y, zl) - 0.008, y, zl))]
    add_geometry(bm, strip, [(2 * i, 2 * i + 2, 2 * i + 3, 2 * i + 1) for i in range(n)], dark)
    # dents du haut (vers le bas) et du bas (vers le haut), intercalees, longueurs irregulieres
    count = 38
    for row in (0, 1):
        for i in range(count):
            u = (i + 0.5 + row * 0.5) / (count + 0.5) * 2.0 - 1.0
            y = u * W * 0.97
            t = max(0.0, 1.0 - u * u)
            gap = upper(y) - lower(y)
            length = gap * rng.uniform(0.75, 1.15) + 0.01
            z0 = upper(y) if row == 0 else lower(y)
            z1 = z0 - length if row == 0 else z0 + length
            y1 = y + rng.uniform(-0.01, 0.01) - u * 0.012
            base = Vector((front(y, z0), y, z0))
            tip = Vector((front(y1, z1) + 0.004, y1, z1))
            width = (0.62 * W / count) * (0.8 + 0.4 * t)
            spike(bm, base, tip, width, 0.012, glow, twist=rng.uniform(-0.3, 0.3))
    # longues pointes aux coins, qui remontent vers les yeux (comme sur l'image), en restant sur le visage
    for sgn in (-1.0, 1.0):
        for k in range(5):
            y = sgn * (W - 0.012 * k)
            z0 = upper(y) - 0.01 * k
            y1 = y - sgn * rng.uniform(0.0, 0.025)
            z1 = z0 + 0.04 + 0.02 * k
            spike(bm, Vector((front(y, z0), y, z0)), Vector((front(y1, z1) + 0.004, y1, z1)), 0.010, 0.008, glow)
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    for poly in me.polygons:
        poly.use_smooth = True
    export_fbx(obj, "SM_SmilerET")
    preview([obj], "SM_SmilerET", Vector((1.0, -0.35, 0.1)))


def skin_limb(nodes, edges, radii, root=0, subdiv=1):
    """Membre organique : graphe de points + rayons -> maillage (modificateur Peau + subdivision)"""
    me = bpy.data.meshes.new("limb")
    me.from_pydata([tuple(p) for p in nodes], edges, [])
    obj = mesh_object("limb", me)
    mod = obj.modifiers.new("skin", "SKIN")
    for i, r in enumerate(radii):
        sv = me.skin_vertices[0].data[i]
        sv.radius = r if isinstance(r, tuple) else (r, r)
        sv.use_root = (i == root)
    if subdiv:
        s = obj.modifiers.new("sub", "SUBSURF")
        s.levels = subdiv
    return obj


def make_arm(root, d, rng, down):
    """Bras humain (epaule -> coude -> poignet -> main a cinq doigts) qui sort de la masse dans la direction d"""
    d = d.normalized()
    side = d.cross(Vector((0, 0, 1)))
    if side.length < 1e-3:
        side = Vector((0, 1, 0))
    side.normalize()
    upper_len = rng.uniform(0.26, 0.36)
    fore_len = rng.uniform(0.26, 0.34)
    elbow = root + d * upper_len + Vector(rng.normal(scale=0.05, size=3))
    if down:
        # bras qui prend appui au sol (la masse se deplace sur ses mains)
        wrist = Vector((elbow.x + d.x * 0.12, elbow.y + d.y * 0.12, -0.58))
    else:
        bend = (d + Vector(rng.normal(scale=0.6, size=3))).normalized()
        wrist = elbow + bend * fore_len
    hand_dir = (wrist - elbow).normalized()
    if down:
        hand_dir = Vector((d.x, d.y, 0.0)).normalized() if Vector((d.x, d.y, 0)).length > 1e-3 else Vector((1, 0, 0))
    palm = wrist + hand_dir * 0.07
    nodes = [root, elbow, wrist, palm]
    edges = [(0, 1), (1, 2), (2, 3)]
    radii = [(0.055, 0.055), (0.042, 0.042), (0.03, 0.026), (0.036, 0.016)]
    fan = hand_dir.cross(side).normalized() if not down else side
    for k, spread in enumerate((-0.6, -0.2, 0.2, 0.6)):
        fd = (hand_dir + fan * spread * 0.5).normalized()
        if down:
            fd = (fd + Vector((0, 0, -0.25))).normalized()
        a = palm + fan * spread * 0.035
        b = a + fd * rng.uniform(0.045, 0.06)
        c = b + (fd + Vector((0, 0, -0.3 if down else 0.0)) + Vector(rng.normal(scale=0.25, size=3))).normalized() * 0.04
        i0 = len(nodes)
        nodes += [a, b, c]
        edges += [(3, i0), (i0, i0 + 1), (i0 + 1, i0 + 2)]
        radii += [(0.011, 0.011), (0.0095, 0.0095), (0.008, 0.008)]
    tb = wrist + hand_dir * 0.03 - fan * 0.04
    tc = tb + (hand_dir - fan * 0.7).normalized() * 0.05
    i0 = len(nodes)
    nodes += [tb, tc]
    edges += [(2, i0), (i0, i0 + 1)]
    radii += [(0.012, 0.012), (0.009, 0.009)]
    return skin_limb(nodes, edges, radii)


def process_clump():
    """Clump (image fournie) : un amas de bras humains autour d'une bouche ronde bordee de dents.
    Origine au centre de la masse (0,60 cm au-dessus du sol en jeu), bouche vers +X. Les bras sont regroupes en
    huit faisceaux exportes a part pour se tordre en jeu."""
    print("== Clump")
    reset()
    rng = np.random.default_rng(11)
    core_me = bpy.data.meshes.new("ClumpCore")
    core = mesh_object("SM_ClumpET_Core", core_me)
    flesh = mat_slot(core, "ClumpFlesh")
    mouth = mat_slot(core, "ClumpMouth")
    teeth = mat_slot(core, "ClumpTeeth")
    bm = bmesh.new()
    lumpy_sphere(bm, 0.42, 4, 0.05, flesh, 5, squash=(0.95, 1.0, 0.92))
    # Bouche ronde (facon lamproie) : gorge sombre, levre epaisse, trois couronnes de dents tournees vers le centre
    R = 0.17
    fx = 0.40
    lip = bmesh.ops.create_circle(bm, cap_ends=False, radius=1.0, segments=24)
    ring_out = lip["verts"]
    for v in ring_out:
        y, z = v.co.x, v.co.y
        v.co = Vector((fx - 0.02, y * (R + 0.06), z * (R + 0.06)))
    throat = []
    for k, (rr, xx) in enumerate(((R, fx), (R * 0.7, fx - 0.12), (R * 0.25, fx - 0.26))):
        ring = bmesh.ops.create_circle(bm, cap_ends=(k == 2), radius=1.0, segments=24)["verts"]
        for v in ring:
            y, z = v.co.x, v.co.y
            v.co = Vector((xx, y * rr, z * rr))
        throat.append(ring)
    def bridge(a, b, mat):
        sa, sb = set(a), set(b)
        edges = {e for v in a for e in v.link_edges if e.other_vert(v) in sa} | {e for v in b for e in v.link_edges if e.other_vert(v) in sb}
        r = bmesh.ops.bridge_loops(bm, edges=list(edges))
        for f in r["faces"]:
            f.material_index = mat
    bridge(ring_out, throat[0], flesh)
    bridge(throat[0], throat[1], mouth)
    bridge(throat[1], throat[2], mouth)
    for f in bm.faces:
        if all(v in set(throat[2]) for v in f.verts):
            f.material_index = mouth
    for ring_i, (rr, xx, ln) in enumerate(((R * 0.98, fx - 0.01, 0.075), (R * 0.82, fx - 0.06, 0.06), (R * 0.62, fx - 0.11, 0.05))):
        cnt = 22 - ring_i * 4
        for i in range(cnt):
            a = 2 * math.pi * (i + 0.5 * ring_i) / cnt
            base = Vector((xx, math.cos(a) * rr, math.sin(a) * rr))
            inward = Vector((-0.35, -math.cos(a), -math.sin(a))).normalized()
            spike(bm, base, base + inward * ln * rng.uniform(0.8, 1.25), 0.012, 0.01, teeth, twist=rng.uniform(-0.2, 0.2))
    bm.normal_update()
    bm.to_mesh(core_me)
    bm.free()
    for poly in core_me.polygons:
        poly.use_smooth = True

    # Bras : une trentaine, partout sauf devant la bouche ; ceux du bas prennent appui au sol
    dirs = []
    while len(dirs) < 30:
        d = Vector(rng.normal(size=3)).normalized()
        if d.x > 0.55 or any((d - o).length < 0.42 for o in dirs):
            continue
        dirs.append(d)
    bundles = [Vector(c).normalized() for c in ((1, 1, 1), (1, -1, 1), (-1, 1, 1), (-1, -1, 1), (1, 1, -1), (1, -1, -1), (-1, 1, -1), (-1, -1, -1))]
    groups = {k: [] for k in range(8)}
    for d in dirs:
        root = d * 0.36
        arm = make_arm(root, d, rng, down=d.z < -0.35)
        k = max(range(8), key=lambda i: d.dot(bundles[i]))
        groups[k].append((arm, root))
    joints = {}
    objs = [core]
    for k, items in groups.items():
        if not items:
            continue
        meshes = []
        for arm, root in items:
            me = bake_world(arm)
            o = mesh_object("A", me)
            o.data.materials.append(bpy.data.materials.get("ClumpFlesh"))
            meshes.append(o)
            bpy.data.objects.remove(arm)
        bpy.ops.object.select_all(action="DESELECT")
        for o in meshes:
            o.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        bpy.ops.object.join()
        bundle = meshes[0]
        # faisceaux complets (v3.9, Nanite dans Unreal)
        for poly in bundle.data.polygons:
            poly.use_smooth = True
        pivot = sum((r for _, r in items), Vector()) / len(items)
        set_origin(bundle, pivot)
        bundle.name = "SM_ClumpET_Arm%d" % k
        export_fbx(bundle, "SM_ClumpET_Arm%d" % k)
        bundle.location = pivot
        joints["Arm%d" % k] = ue(pivot)
        objs.append(bundle)
    export_fbx(core, "SM_ClumpET_Core")
    preview(objs, "SM_ClumpET", Vector((1.0, -0.6, 0.25)))
    JOINTS["Clump"] = joints
    print("  faisceaux de bras (cm, Unreal) :", joints)


# ---------------------------------------------------------------------------
# v3.8 : niveaux d'apres les scenes fournies (Poolrooms, Niveau 4 "Abandoned Office")
# ---------------------------------------------------------------------------
def _flip_green(img):
    """Normal map OpenGL (glTF, vert vers le haut) -> convention du jeu et d'Unreal (vert vers le bas)"""
    a = np.asarray(img.convert("RGB")).copy()
    a[..., 1] = 255 - a[..., 1]
    from PIL import Image
    return Image.fromarray(a)


def _save_jpg(im, name, size, quality):
    from PIL import Image
    im = im.convert("RGB")
    if im.size[0] != size:
        im = im.resize((size, size), Image.LANCZOS)
    os.makedirs(OUT_TEX, exist_ok=True)
    path = os.path.join(OUT_TEX, name + ".jpg")
    im.save(path, quality=quality, subsampling=0, optimize=True)
    print("  texture", path, im.size)


def _save_png(im, name):
    """Sans perte (normal maps : le JPEG laisse des blocs dans les reflets) ; retire l'ancienne version JPEG"""
    os.makedirs(OUT_TEX, exist_ok=True)
    path = os.path.join(OUT_TEX, name + ".png")
    im.convert("RGB").save(path, optimize=True)
    old = os.path.join(OUT_TEX, name + ".jpg")
    if os.path.exists(old):
        os.remove(old)
    print("  texture", path, im.size)


def process_pool_textures():
    """Carrelage vert d'eau a joints gris et platre des plafonds, repris de la scene Poolrooms fournie"""
    from PIL import Image
    d = os.path.join(SRC, "poolrooms")
    names = ("pooltile_1.png", "pooltile_n_0.png", "plaster_4.png", "plaster_n_3.png")
    if not all(os.path.isfile(os.path.join(d, n)) for n in names):
        print("!! textures des Poolrooms introuvables dans", d)
        return
    print("== Textures des Poolrooms")
    _save_jpg(Image.open(os.path.join(d, "pooltile_1.png")), "T_PoolTile37", 512, 95)
    _save_png(_flip_green(Image.open(os.path.join(d, "pooltile_n_0.png"))), "T_PoolTile37_N")
    _save_jpg(Image.open(os.path.join(d, "plaster_4.png")), "T_Plaster", 2048, 95)
    _save_png(_flip_green(Image.open(os.path.join(d, "plaster_n_3.png"))), "T_Plaster_N")


def _pnoise(size, beta, seed):
    """Bruit periodique 1/f^beta (raccord parfait), moyenne 0, ecart-type 1"""
    r = np.random.default_rng(seed)
    F = np.fft.rfft2(r.standard_normal((size, size)))
    fy = np.fft.fftfreq(size)[:, None] * size
    fx = np.fft.rfftfreq(size)[None, :] * size
    f = np.sqrt(fx ** 2 + fy ** 2)
    f[0, 0] = 1.0
    F *= 1.0 / f ** (beta / 2.0)
    F[0, 0] = 0
    n = np.fft.irfft2(F, s=(size, size))
    return (n - n.mean()) / (n.std() + 1e-9)


def _save_normal_from_height(name, h, size):
    """Normal map (x vers la droite, y vers le bas : meme convention que Tools/generate_textures.py)"""
    from PIL import Image
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    nx, ny, nz = -dx, -dy, np.ones_like(h)
    ln = np.sqrt(nx ** 2 + ny ** 2 + nz ** 2)
    nrm = np.stack([nx / ln, ny / ln, nz / ln], -1) * 0.5 + 0.5
    im = Image.fromarray((np.clip(nrm, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB")
    if im.size[0] != size:
        im = im.resize((size, size), Image.LANCZOS)
    _save_png(im, name + "_N")


def make_office_textures():
    """Moquette bleu marine et dalles de faux plafond blanches, d'apres la scene du Niveau 4 fournie
    (ses propres images, 150 px, sont trop petites pour le jeu : on les refait en 1024 px, raccordables)"""
    from PIL import Image
    print("== Textures du bureau")
    S = 1024
    # Moquette : couleur moyenne de la moquette de la scene (sRGB 0,06 / 0,11 / 0,24), fibres et boucles serrees
    fib = _pnoise(S, 0.2, 801)
    loops = _pnoise(S, 1.4, 802)
    mott = _pnoise(S, 2.8, 803)
    lum = np.clip(1.0 + 0.32 * fib + 0.1 * loops + 0.07 * mott, 0.4, 1.8)
    base = np.array([0.062, 0.115, 0.245])
    img = np.clip(lum[..., None] * base[None, None, :], 0, 1)
    _save_jpg(Image.fromarray((img * 255 + 0.5).astype(np.uint8), "RGB"), "T_OfficeCarpetNavy", S, 95)
    _save_normal_from_height("T_OfficeCarpetNavy", (0.8 * fib + 0.6 * loops) * 1.2, S)

    # Faux plafond : 2 x 2 dalles de 60 cm (la texture couvre 120 cm), ossature en T, fibre minerale fissuree
    y, x = np.mgrid[0:S, 0:S].astype(np.float32) / S
    u, v = (x * 2) % 1.0, (y * 2) % 1.0
    edge = np.minimum(np.minimum(u, 1 - u), np.minimum(v, 1 - v))
    grid = (edge < 0.018).astype(np.float32)               # ossature (~2 cm)
    bevel = np.clip((edge - 0.018) / 0.03, 0, 1)            # bord de dalle legerement en retrait
    worm = np.abs(_pnoise(S, 1.6, 811))
    fiss = np.clip(1.0 - worm / 0.07, 0, 1) * np.clip(_pnoise(S, 2.0, 812) * 0.8 + 0.6, 0, 1)
    pits = (np.random.default_rng(813).random((S, S)) < 0.004).astype(np.float32)
    tile = 0.93 - 0.16 * fiss - 0.2 * pits + 0.012 * _pnoise(S, 2.5, 814)
    tile *= 0.94 + 0.06 * bevel
    col = np.where(grid > 0, 0.88, tile)
    img = np.stack([col * 0.995, col * 0.99, col], -1)
    _save_jpg(Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB"), "T_OfficeCeiling", S, 95)
    hgt = 3.0 * grid + 1.2 * bevel - 1.5 * fiss - 2.0 * pits
    _save_normal_from_height("T_OfficeCeiling", hgt, S)


OFFICE_SCALE = 0.26  # la scene fournie est a l'echelle ~1/0,26 (plateau du bureau a 2,98 unites -> 77 cm)


def _office_bounds(objs):
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    return (Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts))),
            Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))))


def _office_copy(src, slot_for, target=None):
    """Copie independante (transformation monde appliquee), materiaux renommes, decimee a ~target sommets"""
    me = bake_world(src)
    obj = mesh_object(src.name + "_copy", me)
    for i, m in enumerate(me.materials):
        new = slot_for(src, m.name if m else "")
        me.materials[i] = bpy.data.materials.get(new) or bpy.data.materials.new(new)
    # Le glTF separe les sommets a chaque arete vive : on les soude, sinon la decimation ne peut rien fusionner
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2e-4)
    bm.to_mesh(me)
    bm.free()
    if target and len(me.vertices) > target:
        decimate(obj, target / len(me.vertices))
    for poly in me.polygons:
        poly.use_smooth = True
    try:
        me.set_sharp_from_angle(angle=math.radians(40))  # Blender 4.1+ : aretes vives au-dela de 40 degres
    except Exception:
        pass
    return obj


def _office_join(objs, name, pivot):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    o.name = name
    o.data.name = name
    o.data.transform(Matrix.Scale(OFFICE_SCALE, 4) @ Matrix.Translation(-pivot))
    o.data.update()
    return o


def process_office():
    """Niveau 4 : un poste de travail de la scene fournie (bureau + ecran cathodique + tour + clavier),
    sa chaise de bureau et sa fontaine a eau, a l'echelle du jeu. Avant du bureau (cote utilisateur) vers +X."""
    path = os.path.join(SRC, "backrooms_lvl4_office.glb")
    if not os.path.isfile(path):
        print("!! introuvable :", path)
        return
    print("== Bureau (Niveau 4)")
    reset()
    bpy.ops.import_scene.gltf(filepath=path)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]

    def center(o):
        mn, mx = _office_bounds([o])
        return (mn + mx) / 2

    def near(o, c, r):
        return (center(o) - c).xy.length < r

    tables = [o for o in meshes if o.name.startswith("Table")]
    table = min(tables, key=lambda o: center(o).xy.length)
    tc = center(table)

    # Poste de travail : bureau, ecran, et les "Cube" poses sur le plateau (clavier, tour)
    desk_mats = {"Material.004": "DeskDark", "Material.005": "DeskChrome", "Material.006": "DeskTop",
                 "Material.007": "PCBeige", "Material.009": "PCBeige", "Material.012": "PCBeige",
                 "Material.008": "PCScreen", "Material.010": "PCDark", "Material.011": "PCDark", "Material.015": "PCDark",
                 "Material.013": "PCGrey", "Material.014": "PCLight"}
    on_desk = [o for o in meshes if o.name.startswith("Cube.") and "_" not in o.name and near(o, tc, 5.0)
               and _office_bounds([o])[0].z > 2.5 and _office_bounds([o])[1].z < 7.0]
    pcs = [o for o in meshes if o.name.startswith("Computer") and near(o, tc, 5.0)]
    desk_src = [table] + pcs + on_desk
    desk_parts = [_office_copy(o, lambda src, m: desk_mats.get(m, "PCBeige")) for o in desk_src]
    mn, mx = _office_bounds(desk_src)
    desk_pivot = Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, 0.0))

    # Chaise : toutes les pieces autour de l'assise la plus proche
    seat = min((o for o in meshes if o.name.startswith("ChairSeat")), key=lambda o: (center(o) - tc).xy.length)
    sc_ = center(seat)
    keys = ("ChairBack", "ChairSeat", "Leg_LP", "Wheel_LP", "Underside_LP", "pCube37", "pCube38", "pCylinder2")
    chair_src = [o for o in meshes if o.name.startswith(keys) and near(o, sc_, 2.4)]
    leather = ("ChairBack", "ChairSeat", "pCube37", "pCube38")
    chair_parts = [_office_copy(o, lambda src, m: "ChairLeather" if src.name.startswith(leather) else "ChairBase") for o in chair_src]
    cmn, cmx = _office_bounds(chair_src)
    chair_pivot = Vector(((cmn.x + cmx.x) / 2, (cmn.y + cmx.y) / 2, 0.0))

    # Fontaine a eau : carrosserie, bonbonne, deux robinets (bleu a gauche, rouge a droite) ; sans l'etiquette
    cool_src = [o for o in meshes if o.name.startswith("Cube.002_") and "Material.008" not in o.name and near(o, tc, 7.5)]
    body = [o for o in cool_src if "Mainmetal" in o.name][0]
    bc = center(body)

    def cooler_slot(src, m):
        if "Mainmetal" in src.name:
            return "CoolerBody"
        if m == "Material.021":
            return "CoolerBottle"
        return "TapBlue" if center(src).y < bc.y else "TapRed"
    cool_parts = [_office_copy(o, cooler_slot) for o in cool_src]
    cool_pivot = Vector((bc.x, bc.y, 0.0))

    desk = _office_join(desk_parts, "SM_OfficeDeskET", desk_pivot)
    export_fbx(desk, "SM_OfficeDeskET")
    chair = _office_join(chair_parts, "SM_OfficeChairET", chair_pivot)
    export_fbx(chair, "SM_OfficeChairET")
    cooler = _office_join(cool_parts, "SM_WaterCoolerET", cool_pivot)
    export_fbx(cooler, "SM_WaterCoolerET")

    # Disposition du poste dans la scene (cm, repere Unreal) : le jeu reproduit la meme
    def ue_off(p):
        return ue((p - desk_pivot) * OFFICE_SCALE)
    dmn, dmx = _office_bounds([desk])
    JOINTS["Office"] = {
        "DeskMin": [round(dmn.x * 100, 1), round(-dmx.y * 100, 1), round(dmn.z * 100, 1)],
        "DeskMax": [round(dmx.x * 100, 1), round(-dmn.y * 100, 1), round(dmx.z * 100, 1)],
        "Chair": ue_off(chair_pivot), "Cooler": ue_off(cool_pivot),
    }
    print("  disposition :", JOINTS["Office"])
    chair.location = (chair_pivot - desk_pivot) * OFFICE_SCALE
    cooler.location = (cool_pivot - desk_pivot) * OFFICE_SCALE
    for o in [o for o in bpy.data.objects if o not in (desk, chair, cooler)]:
        bpy.data.objects.remove(o)
    bpy.context.view_layer.update()
    print("  apercu, bornes :", [tuple(round(c, 2) for c in b) for b in _office_bounds([desk, chair, cooler])])
    preview([desk, chair, cooler], "SM_OfficeDeskET", Vector((1.0, 0.8, 0.6)))


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    todo = args or ["hazmat", "bacteria", "moth", "skinstealer", "faceling", "partygoer", "hound", "smiler", "clump",
                    "pooltex", "officetex", "office"]
    if "hazmat" in todo:
        process_hazmat()
    if "bacteria" in todo:
        process_bacteria()
    if "moth" in todo:
        process_moth()
    if "skinstealer" in todo:
        process_skinstealer()
    if "faceling" in todo:
        process_faceling()
    if "partygoer" in todo:
        process_partygoer()
    if "hound" in todo:
        process_hound()
    if "smiler" in todo:
        process_smiler()
    if "clump" in todo:
        process_clump()
    if "pooltex" in todo:
        process_pool_textures()
    if "officetex" in todo:
        make_office_textures()
    if "office" in todo:
        process_office()
    out = os.path.join(OUT_MESH, "user_models.json")
    old = {}
    if os.path.isfile(out):
        with open(out) as fh:
            old = json.load(fh)
    old.update(JOINTS)
    with open(out, "w") as fh:
        json.dump(old, fh, indent=2)
    print("articulations ->", out)
