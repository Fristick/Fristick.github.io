"""
v4.4 : combinaison du joueur en maillage a squelette (au lieu de pieces rigides), animee os par os par le C++
(UPoseableMeshComponent : marche, course, accroupi, nage, echelle, mort), avec une peau qui se deforme sans coutures.

Usage : python build_hazmat_skeletal.py <dossier contenant asyc_hazmat.glb>   (module bpy de Blender)
Sortie : ../../RawAssets/Skeletal/SK_Hazmat.fbx

- pose de repos "bras le long du corps" (le modele est livre en T) appliquee comme nouvelle pose de repos ;
- os renommes sans prefixe Mixamo ni numero ("mixamorig:LeftUpLeg_055" -> "LeftUpLeg") ;
- repere du jeu : face vers +X, pieds a 0, 1,80 m ; fichier en centimetres (unite 1 cm, comme les autres modeles) ;
- emplacements de materiaux HazmatSuit / HazmatMask / HazmatGlass (memes textures que les pieces).
"""
import math
import os
import re
import sys

import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "RawAssets", "Skeletal")


def clean_name(n):
    n = n.replace("mixamorig:", "")
    n = re.sub(r"_end_\d+$", "_end", n)
    n = re.sub(r"_\d+$", "", n)
    return n


def apply_modifiers(obj):
    bpy.context.view_layer.objects.active = obj
    for m in list(obj.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


def rotate_pose_bone(arm, pb, axis_world, angle):
    mw = arm.matrix_world
    head_w = mw @ pb.head
    rot = Matrix.Translation(head_w) @ Matrix.Rotation(angle, 4, axis_world) @ Matrix.Translation(-head_w)
    pb.matrix = mw.inverted() @ rot @ mw @ pb.matrix
    bpy.context.view_layer.update()


def build(src_dir):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(src_dir, "asyc_hazmat.glb"))
    for o in list(bpy.data.objects):
        if o.type == "MESH" and o.name.startswith("Icosphere"):
            bpy.data.objects.remove(o)
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    meshes = [o for o in bpy.data.objects if o.type == "MESH" and o.find_armature()]

    # 1) Tout a plat dans le repere monde (plus de parents ni de transformations)
    bpy.ops.object.select_all(action="DESELECT")
    for o in [arm] + meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in list(bpy.data.objects):
        if o.type == "EMPTY":
            bpy.data.objects.remove(o)

    # 2) Bras le long du corps (le modele regarde vers -Y), avant-bras a peine plies : nouvelle pose de repos
    pbs = arm.pose.bones

    def bone(key):
        return [b for b in pbs if key in b.name][0]

    rotate_pose_bone(arm, bone("LeftArm_"), Vector((0, 1, 0)), math.radians(78))
    rotate_pose_bone(arm, bone("RightArm_"), Vector((0, 1, 0)), math.radians(-78))
    rotate_pose_bone(arm, bone("LeftForeArm"), Vector((1, 0, 0)), math.radians(-12))
    rotate_pose_bone(arm, bone("RightForeArm"), Vector((1, 0, 0)), math.radians(-12))
    for o in meshes:
        apply_modifiers(o)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.select_all(action="SELECT")
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode="OBJECT")

    # 3) Un seul maillage, emplacements de materiaux du jeu
    rename = {"Suit": "HazmatSuit", "Mask": "HazmatMask", "Glass": "HazmatGlass"}
    for m in bpy.data.materials:
        if m.name in rename:
            m.name = rename[m.name]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.join()
    body = meshes[0]
    body.name = "SK_Hazmat_Mesh"

    # 4) Os renommes (les groupes de sommets suivent)
    for b in arm.data.bones:
        new = clean_name(b.name)
        for vg in body.vertex_groups:
            if vg.name == b.name:
                vg.name = new
        b.name = new
    arm.name = "Armature"
    arm.data.name = "Armature"

    # 5) Repere du jeu : face vers +X, pieds a 0, 1,80 m, centre entre les hanches
    pts = [v.co for v in body.data.vertices]
    minz = min(p.z for p in pts)
    maxz = max(p.z for p in pts)
    s = 1.80 / (maxz - minz)
    hl = arm.data.bones["LeftUpLeg"].head_local
    hr = arm.data.bones["RightUpLeg"].head_local
    cx, cy = (hl.x + hr.x) * 0.5, (hl.y + hr.y) * 0.5
    G = Matrix.Diagonal((s, s, s, 1.0)) @ Matrix.Rotation(math.radians(90), 4, "Z") @ Matrix.Translation((-cx, -cy, -minz))
    # en centimetres (unite du fichier : 1 cm)
    G = Matrix.Scale(100.0, 4) @ G
    for o in (arm, body):
        o.matrix_world = G @ o.matrix_world
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    body.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # 6) Le maillage suit l'armature (modificateur + parent)
    body.parent = arm
    if not any(m.type == "ARMATURE" for m in body.modifiers):
        md = body.modifiers.new("Armature", "ARMATURE")
        md.object = arm
    else:
        for m in body.modifiers:
            if m.type == "ARMATURE":
                m.object = arm

    sc = bpy.context.scene
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 0.01
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, "SK_Hazmat.fbx")
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    body.select_set(True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, use_mesh_modifiers=False,
                             mesh_smooth_type="FACE", add_leaf_bones=False, use_armature_deform_only=True, bake_anim=False,
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", axis_forward="-Z", axis_up="Y",
                             path_mode="STRIP")
    print("export", path, len(body.data.vertices), "sommets,", len(arm.data.bones), "os")
    for n in ("Hips", "Spine", "Spine1", "Spine2", "Neck", "Head", "LeftArm", "LeftForeArm", "RightArm", "RightForeArm",
              "LeftUpLeg", "LeftLeg", "RightUpLeg", "RightLeg", "RightHand"):
        b = arm.data.bones.get(n)
        print("  os", n, tuple(round(c, 1) for c in b.head_local) if b else "ABSENT")
    return arm, body


if __name__ == "__main__":
    src = sys.argv[-1] if len(sys.argv) > 1 and os.path.isdir(sys.argv[-1]) else HERE
    build(src)
