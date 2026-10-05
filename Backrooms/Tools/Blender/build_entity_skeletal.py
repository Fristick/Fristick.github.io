"""
v4.5 : entites fournies en maillages a squelette (SK_*), derives des originaux SANS modifier leur geometrie.

Les quatre modeles fournis qui ont un squelette et des poids de peau (Faceling, Partygoer, Skin-Stealer, Hound) sont
exportes tels quels, en un seul maillage skinne : memes sommets, memes UV, memes poids que l'original. Le jeu les
anime os par os (UPoseableMeshComponent) avec la meme animation procedurale que les pieces rigides : les coudes, genoux
et epaules se plient au lieu de se casser. Les pieces rigides (SM_*ET_*) restent en place et servent de repli.

Traitements (identiques a ceux des pieces rigides de import_user_models.py, pour que tailles et poses correspondent) :
- poids parasites retires (un sommet de l'epaule lie a la main, sommets sans os), modificateurs appliques ;
- pose de repos "bras le long du corps" (bras baisses de 80 degres, avant-bras plies de 12 degres) appliquee au maillage ;
- repere du jeu : face vers +X, pieds a 0, taille du jeu, centre entre les hanches, fichier en centimetres ;
- os principaux renommes (Spine, Head, LeftArm, LeftForeArm, LeftUpLeg, LeftLeg..., Hound : FrontUpperL...), les autres
  gardent leur nom d'origine ; une racine unique "Root" si l'original en a plusieurs (Unreal n'en accepte qu'une) ;
- emplacements de materiaux du jeu (FacelingTex, PartygoerTex, SkinStealerFlesh, HoundSkin...).

Les fichiers de Tools/SourceModels/ sont seulement lus. Un SK_ deja exporte n'est remplace qu'avec --force (voir
Tools/protect_assets.py) : les fichiers proteges de RawAssets/ ont leur empreinte dans RawAssets/protected_assets.json.

Usage (module "bpy" ou Blender) :
    python Tools/Blender/build_entity_skeletal.py [-- faceling partygoer skinstealer hound] [--force] [--preview]
Sorties : RawAssets/Skeletal/SK_<Nom>.fbx, RawAssets/Skeletal/skeletal_models.json (os principaux en cm, repere Unreal),
          RawAssets/Previews/SK_<Nom>_Pose.jpg avec --preview (rendu Blender d'une pose de test, pas une capture du jeu)
"""
import json
import math
import os
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.normpath(os.path.join(HERE, "..")))
import import_user_models as ium  # noqa: E402  (fonctions communes : chargement, nettoyage des poids, pose)
import protect_assets  # noqa: E402

ROOT = ium.ROOT
SRC = ium.SRC
OUT = os.path.join(ROOT, "RawAssets", "Skeletal")
OUT_PREV = os.path.join(ROOT, "RawAssets", "Previews")
REPORT = {}

HUMAN_BONES = {"Torso": "Spine", "Head": "Head", "UpperArmL": "LeftArm", "LowerArmL": "LeftForeArm", "UpperArmR": "RightArm",
               "LowerArmR": "RightForeArm", "ThighL": "LeftUpLeg", "ShinL": "LeftLeg", "ThighR": "RightUpLeg", "ShinR": "RightLeg"}


# ---------------------------------------------------------------------------
# Preparation commune
# ---------------------------------------------------------------------------
def select_only(objs, active=None):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.hide_set(False)
        o.hide_viewport = False
        o.select_set(True)
    bpy.context.view_layer.objects.active = active or objs[0]


def remove_shape_keys(o):
    """Les modificateurs ne s'appliquent pas a un maillage a shape keys : on garde le melange courant"""
    if not o.data.shape_keys:
        return
    if o.data.users > 1:
        o.data = o.data.copy()
    select_only([o])
    bpy.ops.object.shape_key_remove(all=True, apply_mix=True)


def freeze_action(arm):
    """Pose courante de l'original (action comprise, comme pour les pieces rigides) figee dans les os, action retiree"""
    if arm.animation_data and arm.animation_data.action:
        basis = {pb.name: pb.matrix_basis.copy() for pb in arm.pose.bones}
        arm.animation_data.action = None
        for pb in arm.pose.bones:
            pb.matrix_basis = basis[pb.name]
        bpy.context.view_layer.update()


def flatten(arm, meshes):
    """Plus de parents ni de transformations d'objet : tout est dans le repere monde"""
    select_only([arm] + meshes, arm)
    bpy.ops.object.make_single_user(object=True, obdata=True)
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in list(bpy.data.objects):
        if o.type in ("EMPTY", "CAMERA", "LIGHT"):
            bpy.data.objects.remove(o)
    bpy.context.view_layer.update()


def apply_pose_as_rest(arm, meshes):
    """Le maillage prend la pose courante (modificateur Armature applique), puis cette pose devient la pose de repos"""
    for o in meshes:
        select_only([o])
        for m in list(o.modifiers):
            if m.type == "ARMATURE":
                bpy.ops.object.modifier_apply(modifier=m.name)
    select_only([arm])
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.select_all(action="SELECT")
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode="OBJECT")


def single_root(arm):
    roots = [b.name for b in arm.data.bones if b.parent is None]
    if len(roots) <= 1:
        return roots[0] if roots else None
    select_only([arm])
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm.data.edit_bones
    root = eb.new("Root")
    root.head = (0.0, 0.0, 0.0)
    root.tail = (0.0, 0.0, 0.1)
    for n in roots:
        eb[n].parent = root
        eb[n].use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    print("  racine unique ajoutee au-dessus de", roots)
    return "Root"


def rename_bones(arm, body, mapping):
    """mapping : os d'origine -> nom du jeu (les groupes de sommets suivent)"""
    taken = {b.name.lower() for b in arm.data.bones}
    for old, new in mapping.items():
        if new.lower() in taken and new != old:
            raise RuntimeError("nom d'os deja pris : " + new)
    for old, new in mapping.items():
        b = arm.data.bones[old]
        vg = body.vertex_groups.get(old)
        if vg:
            vg.name = new
        b.name = new


def join_meshes(meshes, mat_map, default_slot, name):
    for o in meshes:
        ium.single_uv(o)
        for slot in o.material_slots:
            src = slot.material.name if slot.material else ""
            new = mat_map.get(src, default_slot)
            slot.material = bpy.data.materials.get(new) or bpy.data.materials.new(new)
        if not o.material_slots:
            o.data.materials.append(bpy.data.materials.get(default_slot) or bpy.data.materials.new(default_slot))
    select_only(meshes)
    bpy.ops.object.join()
    body = meshes[0]
    body.name = name
    # Emplacements fusionnes par nom (un seul "FacelingTex"...) : moins d'appels de rendu dans Unreal
    names = []
    for m in body.data.materials:
        if m.name not in names:
            names.append(m.name)
    remap = [names.index(m.name) for m in body.data.materials]
    idx = [remap[p.material_index] for p in body.data.polygons]
    body.data.materials.clear()
    for n in names:
        body.data.materials.append(bpy.data.materials[n])
    for p, i in zip(body.data.polygons, idx):
        p.material_index = i
    return body


def drop_faces_below(body, z):
    bm = bmesh.new()
    bm.from_mesh(body.data)
    dead = [f for f in bm.faces if f.calc_center_median().z < z]
    bmesh.ops.delete(bm, geom=dead, context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(body.data)
    bm.free()


def to_game_frame(arm, body, forward, height, center_bones):
    """Face vers +X, pieds a 0, taille du jeu (m), centre entre center_bones ; fichier en centimetres"""
    yaw = math.atan2(forward.y, forward.x)
    R = Matrix.Rotation(-yaw, 4, "Z")
    co = np.array([tuple(R @ v.co) for v in body.data.vertices])
    minz, maxz = co[:, 2].min(), co[:, 2].max()
    s = height / (maxz - minz)
    c = sum((R @ arm.data.bones[b].head_local for b in center_bones), Vector()) / len(center_bones)
    G = Matrix.Scale(100.0, 4) @ Matrix.Diagonal((s, s, s, 1.0)) @ Matrix.Translation((-c.x, -c.y, -minz)) @ R
    for o in (arm, body):
        o.matrix_world = G @ o.matrix_world
    select_only([arm, body], arm)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return s


def bind(arm, body):
    body.parent = arm
    mods = [m for m in body.modifiers if m.type == "ARMATURE"]
    md = mods[0] if mods else body.modifiers.new("Armature", "ARMATURE")
    md.object = arm


def export(arm, body, name, force):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".fbx")
    if not protect_assets.may_write(path, force):
        return None
    sc = bpy.context.scene
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 0.01
    arm.name = "Armature"
    arm.data.name = "Armature"
    select_only([arm, body], arm)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, use_mesh_modifiers=False,
                             mesh_smooth_type="FACE", add_leaf_bones=False, use_armature_deform_only=False, bake_anim=False,
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", axis_forward="-Z", axis_up="Y",
                             path_mode="STRIP")
    protect_assets.record(path, "derive de Tools/SourceModels (build_entity_skeletal.py)")
    print("  export", path, len(body.data.vertices), "sommets,", len(arm.data.bones), "os,",
          len(body.data.materials), "materiaux :", [m.name for m in body.data.materials])
    return path


def report(key, arm, body, bones):
    """Os principaux (cm, repere Unreal : Y inverse) : a comparer aux articulations des pieces rigides (user_models.json)"""
    out = {}
    for part, b in bones.items():
        h = arm.data.bones[b].head_local
        out[part] = [round(h.x, 2), round(-h.y, 2), round(h.z, 2)]
    co = np.array([tuple(v.co) for v in body.data.vertices])
    REPORT[key] = {"Bones": out, "Vertices": len(body.data.vertices), "Triangles": int(sum(len(p.vertices) - 2 for p in body.data.polygons)),
                   "BoneCount": len(arm.data.bones), "Height": round(float(co[:, 2].max() - co[:, 2].min()), 1),
                   "Materials": [m.name for m in body.data.materials]}
    print("  os principaux (cm, Unreal) :", out)


# ---------------------------------------------------------------------------
# Humanoides
# ---------------------------------------------------------------------------
def humanoid(key, path, part_of, joint_bones, mat_map, default_slot, height, arm_drop=80.0, elbow=12.0, skip=(), extra_names=None,
             force=False):
    print("==", key)
    ium.load_model(path)
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    meshes = [o for o in bpy.data.objects if o.type == "MESH" and o.find_armature() == arm and o.name not in skip
              and len(o.data.vertices) > 0]
    for o in list(bpy.data.objects):
        if o.type == "MESH" and o not in meshes:
            bpy.data.objects.remove(o)
    freeze_action(arm)
    flatten(arm, meshes)
    for o in meshes:
        remove_shape_keys(o)
        ium.apply_shape_modifiers(o)
        ium.drop_orphans(o)
        ium.clean_weights(o, part_of, ium.HAZMAT_PARENT)

    pbs = arm.pose.bones

    def hw(b):
        return arm.matrix_world @ pbs[b].head

    def tw(b):
        return arm.matrix_world @ pbs[b].tail

    up = Vector((0, 0, 1))
    left = hw(joint_bones["UpperArmL"]) - hw(joint_bones["UpperArmR"])
    left.z = 0
    left.normalize()
    forward = left.cross(up).normalized()
    # Meme pose de repos que les pieces rigides (import_user_models.humanoid)
    for side, sgn in (("L", -1.0), ("R", 1.0)):
        b = joint_bones["UpperArm" + side]
        d = tw(b) - hw(b)
        cur = math.degrees(math.atan2(-d.z, Vector((d.x, d.y, 0)).length))
        ium.rotate_pose_bone(arm, pbs[b], forward, math.radians(sgn * (arm_drop - cur)))
        ium.rotate_pose_bone(arm, pbs[joint_bones["LowerArm" + side]], left, math.radians(-elbow))
    bpy.context.view_layer.update()
    apply_pose_as_rest(arm, meshes)

    body = join_meshes(meshes, mat_map, default_slot, "SK_" + key + "_Mesh")
    names = {joint_bones[p]: HUMAN_BONES[p] for p in HUMAN_BONES}
    names.update(extra_names or {})
    # Cote du jeu : gauche = +Y Blender une fois tourne vers +X (= -Y dans Unreal, index 0)
    rename_bones(arm, body, names)
    single_root(arm)
    to_game_frame(arm, body, forward, height, ("LeftUpLeg", "RightUpLeg"))
    la, ra = arm.data.bones["LeftArm"].head_local, arm.data.bones["RightArm"].head_local
    if la.y < ra.y:
        print("  !! cotes inverses (LeftArm a droite) : verifier le modele")
    bind(arm, body)
    report(key, arm, body, {p: HUMAN_BONES[p] for p in HUMAN_BONES})
    return arm, body, export(arm, body, "SK_" + key, force)


FACELING_JOINTS = {"Torso": "spine1_11", "Head": "neck_1", "UpperArmL": "shoulder.L_3", "LowerArmL": "foearm.L_2",
                   "UpperArmR": "shoulder.R_5", "LowerArmR": "foearm.R_4", "ThighL": "thigh.L_8", "ShinL": "calf.L_7",
                   "ThighR": "thigh.R_10", "ShinR": "calf.R_9"}


def faceling_part(b):
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


def build_faceling(force):
    # Style PS1 conserve : 1 387 sommets, texture d'origine basse resolution, aucune subdivision ni lissage ajoute
    return humanoid("Faceling", os.path.join(SRC, "faceling.glb"), faceling_part, FACELING_JOINTS, {"H_Body": "FacelingTex"},
                    "FacelingTex", 1.78, extra_names={"hand.L_18": "LeftHand", "hand.R_20": "RightHand"}, force=force)


PARTYGOER_JOINTS = {"Torso": "CC_Base_Hip", "Head": "CC_Base_NeckTwist01", "UpperArmL": "CC_Base_L_Upperarm",
                    "LowerArmL": "CC_Base_L_Forearm", "UpperArmR": "CC_Base_R_Upperarm", "LowerArmR": "CC_Base_R_Forearm",
                    "ThighL": "CC_Base_L_Thigh", "ShinL": "CC_Base_L_Calf", "ThighR": "CC_Base_R_Thigh", "ShinR": "CC_Base_R_Calf"}


def partygoer_part(b):
    fingers = ("Thumb", "Index", "Mid", "Ring", "Pinky")
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


def build_partygoer(force):
    # Le ballon rouge (non rigge dans l'original) reste SM_PartygoerET_Balloon, tenu par l'os RightHand en jeu
    return humanoid("Partygoer", os.path.join(SRC, "partygoer.fbx"), partygoer_part, PARTYGOER_JOINTS,
                    {"Partygoer_LP.003": "PartygoerTex"}, "PartygoerTex", 1.92, skip=("Baloon red",),
                    extra_names={"CC_Base_L_Hand": "LeftHand", "CC_Base_R_Hand": "RightHand"}, force=force)


SKIN_GROUPS = [((15, 17), "Torso"), ((18, 23), "Head"), ((24, 24), "UpperArmL"), ((25, 41), "LowerArmL"), ((42, 42), "UpperArmR"),
               ((43, 59), "LowerArmR"), ((60, 60), "ThighL"), ((61, 63), "ShinL"), ((64, 64), "ThighR"), ((65, 67), "ShinR")]
SKIN_JOINTS = {"Torso": "n15", "Head": "n18", "UpperArmL": "n24", "LowerArmL": "n25", "UpperArmR": "n42", "LowerArmR": "n43",
               "ThighL": "n60", "ShinL": "n61", "ThighR": "n64", "ShinR": "n65"}
SKIN_MATS = {"Material_001": "EyeDark", "Material_002": "SkinStealerFlesh", "Material_003": "StealerClaw", "Material_004": "StealerEye",
             "Material_005": "StealerEye", "Material_006": "SkinStealerFlesh", "Material_007": "SkinStealerFlesh",
             "Material_008": "SkinStealerFlesh"}


def skin_part(b):
    if not (b.startswith("n") and b[1:].isdigit()):
        return None
    n = int(b[1:])
    for (lo, hi), part in SKIN_GROUPS:
        if lo <= n <= hi:
            return part
    return None


def build_skinstealer(force):
    # n17 = poitrine (les bras et la tete en partent) ; n26 / n44 = mains aux griffes demesurees
    return humanoid("SkinStealer", os.path.join(SRC, "skin_stealer.usdz"), skin_part, SKIN_JOINTS, SKIN_MATS, "SkinStealerFlesh",
                    2.05, arm_drop=82.0, extra_names={"n17": "Chest", "n26": "LeftHand", "n44": "RightHand"}, force=force)


# ---------------------------------------------------------------------------
# Hound (quadrupede)
# ---------------------------------------------------------------------------
HOUND_MATS = {"Material.002": "HoundSkin", "Material": "HoundHair", "Fur Material": "HoundHair", "Material.001": "HoundFace",
              "Material.003": "HoundMouth", "Material.004": "GlowAmberEye", "Material.007": "EyeDark", "Material.005": "HoundTongue",
              "Material.006": "HoundTeeth"}


def build_hound(force):
    print("== Hound")
    ium.load_model(os.path.join(SRC, "hound.blend"))
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    meshes = [o for o in bpy.data.objects if o.type == "MESH" and o.find_armature() == arm and o.name != "Cube.007"
              and len(o.data.vertices) > 0]
    for o in list(bpy.data.objects):
        if o.type == "MESH" and o not in meshes:
            bpy.data.objects.remove(o)
    # Meme pose que les pieces rigides (pose courante de l'original, action "t_pose" comprise)
    freeze_action(arm)
    flatten(arm, meshes)
    pbs = arm.pose.bones

    def hw(b):
        return arm.matrix_world @ pbs[b].head

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
    names = {"Bone.002": "Chest", "Bone.003": "Head", "Bone": "Hips", "Bone.001": "Spine"}
    for end, (a, b) in legs.items():
        for upper_bone in (a, b):
            S = side(upper_bone)
            bone_part[upper_bone] = end + "Upper" + S
            for lb in lower[upper_bone]:
                bone_part[lb] = end + "Lower" + S
            names[upper_bone] = end + "Upper" + S
            names[lower[upper_bone][0]] = end + "Lower" + S

    def part_of(b):
        if b in bone_part:
            return bone_part[b]
        return "Head" if b.startswith("Bone.0") else None

    feet = min(hw(b).z for b in ("Bone.028", "Bone.033", "Bone.020", "Bone.024"))
    for o in meshes:
        remove_shape_keys(o)
        ium.apply_shape_modifiers(o)
        ium.drop_orphans(o)
        ium.clean_weights(o, part_of, ium.HOUND_PARENT)
    apply_pose_as_rest(arm, meshes)
    body = join_meshes(meshes, HOUND_MATS, "HoundSkin", "SK_Hound_Mesh")
    drop_faces_below(body, feet - 0.6)  # sol de la scene d'origine
    rename_bones(arm, body, names)
    single_root(arm)
    to_game_frame(arm, body, forward, 1.15, ("FrontUpperL", "FrontUpperR", "BackUpperL", "BackUpperR"))
    bind(arm, body)
    report("Hound", arm, body, {p: p for p in ("Head", "FrontUpperL", "FrontLowerL", "FrontUpperR", "FrontLowerR", "BackUpperL",
                                               "BackLowerL", "BackUpperR", "BackLowerR")} | {"Body": "Chest"})
    return arm, body, export(arm, body, "SK_Hound", force)


# ---------------------------------------------------------------------------
# Verification : pose de test rendue dans Blender (ce n'est PAS une capture du jeu)
# ---------------------------------------------------------------------------
SLOT_TEX = {"FacelingTex": "T_Faceling", "PartygoerTex": "T_Partygoer", "SkinStealerFlesh": "T_SkinStealer_Flesh",
            "StealerClaw": "T_SkinStealer_Claw", "StealerEye": "T_SkinStealer_Eye", "HoundSkin": "T_Hound"}
SLOT_COLOR = {"HoundHair": (0.012, 0.011, 0.011), "HoundFace": (0.42, 0.32, 0.28), "HoundTongue": (0.75, 0.28, 0.35),
              "HoundMouth": (0.1, 0.02, 0.02), "HoundTeeth": (0.8, 0.78, 0.7), "GlowAmberEye": (1.0, 0.55, 0.1), "EyeDark": (0.01, 0.01, 0.01)}


def preview_materials(body):
    for m in body.data.materials:
        m.use_nodes = True
        nt = m.node_tree
        bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
        tex = SLOT_TEX.get(m.name)
        path = os.path.join(ROOT, "RawAssets", "Textures", (tex or "") + ".jpg")
        if tex and os.path.isfile(path):
            node = nt.nodes.new("ShaderNodeTexImage")
            node.image = bpy.data.images.load(path)
            nt.links.new(node.outputs["Color"], bsdf.inputs["Base Color"])
        else:
            c = SLOT_COLOR.get(m.name, (0.5, 0.5, 0.5))
            bsdf.inputs["Base Color"].default_value = (*c, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.6


def pose_preview(arm, body, name, quadruped=False):
    """Pose de marche + bras tendus, comme l'animation procedurale du jeu (rotations autour des axes du repere)"""
    def rot(bone, axis, deg):
        pb = arm.pose.bones.get(bone)
        if not pb:
            return
        ium.rotate_pose_bone(arm, pb, Vector(axis), math.radians(deg))

    if quadruped:
        rot("FrontUpperL", (0, 1, 0), 28)
        rot("FrontLowerL", (0, 1, 0), -40)
        rot("FrontUpperR", (0, 1, 0), -22)
        rot("BackUpperL", (0, 1, 0), -25)
        rot("BackUpperR", (0, 1, 0), 30)
        rot("BackLowerR", (0, 1, 0), 35)
        rot("Head", (0, 1, 0), 12)
    else:
        # cuisse gauche en avant, genou droit plie, bras droit tendu vers l'avant (poursuite), tete tournee
        rot("LeftUpLeg", (0, 1, 0), -30)
        rot("LeftLeg", (0, 1, 0), 18)
        rot("RightUpLeg", (0, 1, 0), 22)
        rot("RightLeg", (0, 1, 0), 55)
        rot("RightArm", (0, 1, 0), -80)
        rot("RightForeArm", (0, 1, 0), -15)
        rot("LeftArm", (0, 1, 0), 25)
        rot("LeftForeArm", (0, 1, 0), -40)
        rot("Head", (0, 0, 1), 25)
    preview_materials(body)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 24
    sc.render.resolution_x, sc.render.resolution_y = 520, 520
    world = bpy.data.worlds.new("W")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.3, 0.3, 0.32, 1)
    sc.world = world
    dg = bpy.context.evaluated_depsgraph_get()
    ev = body.evaluated_get(dg)
    co = np.array([tuple(ev.matrix_world @ v.co) for v in ev.data.vertices])
    mn, mx = Vector(co.min(0)), Vector(co.max(0))
    ctr = (mn + mx) / 2
    rad = (mx - mn).length / 2
    cam = ium.link(bpy.data.objects.new("C", bpy.data.cameras.new("C")))
    cam.data.lens = 50
    cam.data.clip_end = 10000
    cam.location = ctr + Vector((1.0, -0.75, 0.3)).normalized() * rad * 3.0
    cam.rotation_euler = (ctr - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc.camera = cam
    sun = ium.link(bpy.data.objects.new("L", bpy.data.lights.new("L", "SUN")))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(30))
    os.makedirs(OUT_PREV, exist_ok=True)
    try:
        sc.render.image_settings.media_type = "IMAGE"  # Blender 4.5+ (la scene du Hound est reglee en video)
    except Exception:
        pass
    sc.render.image_settings.file_format = "JPEG"
    sc.render.filepath = os.path.join(OUT_PREV, name + "_Pose.jpg")
    bpy.ops.render.render(write_still=True)
    print("  apercu (Blender)", sc.render.filepath)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    force = "--force" in args
    want_preview = "--preview" in args
    todo = [a for a in args if not a.startswith("--")] or ["faceling", "partygoer", "skinstealer", "hound"]
    builders = {"faceling": build_faceling, "partygoer": build_partygoer, "skinstealer": build_skinstealer, "hound": build_hound}
    for k in todo:
        res = builders[k](force)
        if res and want_preview:
            a, b, _ = res
            pose_preview(a, b, "SK_" + {"faceling": "Faceling", "partygoer": "Partygoer", "skinstealer": "SkinStealer",
                                        "hound": "Hound"}[k], quadruped=(k == "hound"))
    out = os.path.join(OUT, "skeletal_models.json")
    old = {}
    if os.path.isfile(out):
        with open(out) as fh:
            old = json.load(fh)
    old.update(REPORT)
    os.makedirs(OUT, exist_ok=True)
    with open(out, "w") as fh:
        json.dump(old, fh, indent=2)
    print("rapport ->", out)
