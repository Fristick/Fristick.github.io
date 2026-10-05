"""
v4.5 : rendus Blender de comparaison avant / apres des entites (Docs/v45/blender_*.jpg).

Ce sont des RENDUS BLENDER (Cycles, CPU) des fichiers de RawAssets/, avec des materiaux approchants : ils montrent la
geometrie, les silhouettes et les textures, pas l'eclairage ni les materiaux d'Unreal. Les captures du jeu se font avec
le test automatique (-BRAutoTest, voir README).

Usage : python Tools/Blender/render_compare.py -- <cle> <sortie.jpg>
  cles : wretch_old (pieces SM_Wretch_*), wretch (SK_Wretch), clump_old (SM_ClumpET_*), clump (SK_Clump),
         smiler (SM_SmilerET), toutes avec la meme camera et la meme lumiere pour une cle et sa version _old
"""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
MESH = os.path.join(ROOT, "RawAssets", "Meshes")
SKEL = os.path.join(ROOT, "RawAssets", "Skeletal")
TEX = os.path.join(ROOT, "RawAssets", "Textures")

# Couleurs / textures approchant les emplacements du jeu (BRAssets.cpp, SlotStyles)
SLOTS = {
    "WretchSkin": ("T_Wretch", (0.42, 0.45, 0.36), 0.55, 0.0),
    "WretchMouth": (None, (0.05, 0.012, 0.01), 0.35, 0.0),
    "WretchTeeth": (None, (0.55, 0.5, 0.36), 0.4, 0.0),
    "WretchEye": (None, (0.01, 0.01, 0.01), 0.2, 0.0),
    "ClumpSkin": ("T_Clump", (0.86, 0.7, 0.6), 0.45, 0.0),
    "ClumpFlesh": (None, (0.86, 0.7, 0.6), 0.45, 0.0),
    "ClumpMouth": (None, (0.12, 0.015, 0.015), 0.25, 0.0),
    "ClumpTeeth": (None, (0.72, 0.58, 0.32), 0.4, 0.0),
    "ClumpNail": (None, (0.75, 0.62, 0.5), 0.35, 0.0),
    "Skin": (None, (0.42, 0.45, 0.36), 0.6, 0.0),
    "Mouth": (None, (0.08, 0.01, 0.01), 0.3, 0.0),
    "Teeth": (None, (0.9, 0.88, 0.8), 0.4, 0.0),
    "Eye": (None, (0.01, 0.01, 0.01), 0.1, 0.0),
    "SmilerDark": (None, (0.006, 0.006, 0.007), 0.9, 0.0),
    "SmilerMouth": (None, (0.02, 0.004, 0.004), 0.4, 0.0),
}
GLOW = {"Glow": ((1.0, 0.96, 0.88), 9.0), "GlowSoft": ((1.0, 0.93, 0.82), 1.2)}


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def import_fbx(path, offset=Vector((0, 0, 0))):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        if o.parent is None:
            o.location = o.location + offset
    return new


def materials():
    for o in bpy.data.objects:
        if o.type != "MESH":
            continue
        for slot in o.material_slots:
            name = slot.material.name.split(".")[0] if slot.material else "Skin"
            m = bpy.data.materials.new(name + "_r")
            nt = m.node_tree if m.node_tree else None
            if nt is None:
                m.use_nodes = True
                nt = m.node_tree
            b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
            key = next((k for k in GLOW if name == k), None) or next((k for k in GLOW if name.startswith(k)), None)
            if key:
                col, strength = GLOW[key]
                b.inputs["Emission Color"].default_value = (*col, 1)
                b.inputs["Emission Strength"].default_value = strength
                b.inputs["Base Color"].default_value = (0.8, 0.8, 0.8, 1)
            else:
                tex, col, rough, metal = SLOTS.get(name, SLOTS.get(next((k for k in SLOTS if k in name), "Skin")))
                p = os.path.join(TEX, (tex or "") + ".jpg")
                if tex and os.path.isfile(p):
                    t = nt.nodes.new("ShaderNodeTexImage")
                    t.image = bpy.data.images.load(p)
                    nt.links.new(t.outputs["Color"], b.inputs["Base Color"])
                    pn = os.path.join(TEX, tex + "_N.png")
                    if os.path.isfile(pn):
                        tn = nt.nodes.new("ShaderNodeTexImage")
                        tn.image = bpy.data.images.load(pn)
                        tn.image.colorspace_settings.name = "Non-Color"
                        nm = nt.nodes.new("ShaderNodeNormalMap")
                        nt.links.new(tn.outputs["Color"], nm.inputs["Color"])
                        nt.links.new(nm.outputs["Normal"], b.inputs["Normal"])
                else:
                    b.inputs["Base Color"].default_value = (*col, 1)
                b.inputs["Roughness"].default_value = rough
                b.inputs["Metallic"].default_value = metal
                if "Skin" in name or "Flesh" in name:
                    b.inputs["Subsurface Weight"].default_value = 0.15
            slot.material = m


def studio(out, target, dist, view=Vector((1.0, -0.8, 0.25)), dark=False, res=720):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 48
    try:
        sc.cycles.use_denoising = True
    except Exception:
        pass
    sc.render.resolution_x = sc.render.resolution_y = res
    w = bpy.data.worlds.new("W")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.004, 0.004, 0.005, 1) if dark else (0.22, 0.22, 0.23, 1)
    sc.world = w
    cam = bpy.data.objects.new("C", bpy.data.cameras.new("C"))
    sc.collection.objects.link(cam)
    cam.data.lens = 50
    cam.location = target + view.normalized() * dist
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc.camera = cam
    if not dark:
        for e, rot in ((3.0, (55, 0, 35)), (0.8, (70, 0, -140))):
            l = bpy.data.objects.new("L", bpy.data.lights.new("L", "SUN"))
            l.data.energy = e
            l.rotation_euler = [math.radians(a) for a in rot]
            sc.collection.objects.link(l)
    else:
        l = bpy.data.objects.new("L", bpy.data.lights.new("L", "POINT"))
        l.data.energy = 25.0
        l.location = target + Vector((1.2, 1.0, 1.4))
        sc.collection.objects.link(l)
    try:
        sc.render.image_settings.media_type = "IMAGE"
    except Exception:
        pass
    sc.render.image_settings.file_format = "JPEG"
    sc.render.image_settings.quality = 92
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)
    print("rendu (Blender)", out)


def ue(p):
    return Vector((p[0] / 100.0, -p[1] / 100.0, p[2] / 100.0))


def wretch_old(out):
    # Pieces procedurales de la v4.4 (generate_models.py), posees comme FBRHumanoidSpec::Simple pour le Wretch
    hip, sh, sw, hw, hunch, ua, th, neck = 82, 122, 17, 9, 30, 33, 41, 5
    import_fbx(os.path.join(MESH, "SM_Wretch_Torso.fbx"), ue((0, 0, hip)))
    import_fbx(os.path.join(MESH, "SM_Wretch_Head.fbx"), ue((hunch * 1.05, 0, sh + neck)))
    for sgn in (-1, 1):
        import_fbx(os.path.join(MESH, "SM_Wretch_UpperArm.fbx"), ue((hunch, sgn * sw, sh)))
        import_fbx(os.path.join(MESH, "SM_Wretch_LowerArm.fbx"), ue((hunch, sgn * sw, sh - ua)))
        import_fbx(os.path.join(MESH, "SM_Wretch_Thigh.fbx"), ue((0, sgn * hw, hip)))
        import_fbx(os.path.join(MESH, "SM_Wretch_Shin.fbx"), ue((0, sgn * hw, hip - th)))
    materials()
    studio(out, Vector((0.12, 0, 0.75)), 3.2)


def wretch(out):
    import_fbx(os.path.join(SKEL, "SK_Wretch.fbx"))
    materials()
    studio(out, Vector((0.12, 0, 0.75)), 3.2)


def clump_old(out):
    with open(os.path.join(MESH, "user_models.json")) as fh:
        roots = json.load(fh)["Clump"]
    center = Vector((0, 0, 0.6))
    import_fbx(os.path.join(MESH, "SM_ClumpET_Core.fbx"), center)
    for k in range(8):
        import_fbx(os.path.join(MESH, "SM_ClumpET_Arm%d.fbx" % k), center + ue(roots["Arm%d" % k]))
    materials()
    studio(out, Vector((0, 0, 0.6)), 3.6, view=Vector((1.0, -0.55, 0.35)))


def clump(out):
    import_fbx(os.path.join(SKEL, "SK_Clump.fbx"))
    materials()
    studio(out, Vector((0, 0, 0.6)), 3.6, view=Vector((1.0, -0.55, 0.35)))


def smiler(out):
    import_fbx(os.path.join(MESH, "SM_SmilerET.fbx"))
    materials()
    studio(out, Vector((0, 0, 0.0)), 2.6, view=Vector((1.0, -0.35, 0.05)), dark=True)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    reset()
    globals()[args[0]](os.path.abspath(args[1]))
