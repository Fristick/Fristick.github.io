"""
v4.6 : RENDU BLENDER (Cycles, processeur) de la geometrie d'une salle de fosses telle que le jeu la genere.

Ce n'est PAS une capture d'Unreal : les boites viennent de Tools/verify_pitfalls.py (portage Python des regles de
ABRChunk::BuildPitRoom, des murs, portes, piliers, plafond et neons), habillees des textures du Niveau 0 projetees dans
l'espace monde comme le fait le materiau M_BR_World. Eclairage approche (lampes surfaciques des neons, lampe torche),
sans Lumen, sans brouillard, sans etalonnage du jeu. Sert a juger la geometrie : bords epais, parois, profondeur,
raccords, grille de fosses et passages.

Usage :
    python Tools/verify_pitfalls.py --seed 9 --json pit.json
    python Tools/Blender/render_pitroom.py -- pit.json <sortie_prefixe>
      -> <prefixe>_vue.jpg (depuis le point de vue de BRPits), <prefixe>_lampe.jpg (lampe torche au bord d'une fosse),
         <prefixe>_coupe.jpg (coupe verticale : profondeur et bandes sombres)
"""
import json
import math
import os
import sys

import bpy  # (avant bmesh : le module bpy l'enregistre)
import bmesh
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
TEX = os.path.join(ROOT, "RawAssets", "Textures")

# Surfaces : (texture, teinte, echelle en m, rugosite) d'apres BRLevels.cpp (Niveau 0) et ABRChunk::BuildPitRoom
SURF = {
    "floor": ("T_L0_Carpet", (1, 1, 1), 2.2, 0.95),
    "wall": ("T_L0_Wallpaper", (1, 1, 1), 1.2, 0.8),
    "trim": ("T_L0_Wallpaper", (0.55, 0.48, 0.32), 1.0, 0.6),
    "ceiling": ("T_L0_Ceiling", (1, 1, 1), 1.2, 0.9),
    "fiber": ("T_L0_Carpet", (0.42, 0.42, 0.42), 2.2, 1.0),
    "slabedge": ("T_Concrete", (0.58, 0.56, 0.5), 1.2, 0.92),
    "upper": ("T_Concrete", (0.34, 0.32, 0.27), 2.0, 0.9),
    "mid": ("T_Concrete", (0.15, 0.14, 0.12), 2.6, 0.92),
    "deep": ("T_Concrete", (0.045, 0.042, 0.038), 3.0, 0.95),
    "bottom": ("T_Grime", (0.012, 0.011, 0.01), 2.0, 1.0),
}
LIGHT_COL = (1.0, 0.96, 0.84)


def ue(p):
    """cm UE (main gauche) -> m Blender (main droite) : Y inverse"""
    return Vector((p[0] / 100.0, -p[1] / 100.0, p[2] / 100.0))


def material(kind):
    tex, tint, scale, rough = SURF[kind]
    m = bpy.data.materials.new(kind)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    div = nt.nodes.new("ShaderNodeVectorMath")
    div.operation = "DIVIDE"
    div.inputs[1].default_value = (scale, scale, scale)
    nt.links.new(geo.outputs["Position"], div.inputs[0])
    img = nt.nodes.new("ShaderNodeTexImage")
    img.image = bpy.data.images.load(os.path.join(TEX, tex + ".jpg"), check_existing=True)
    img.projection = "BOX"
    img.projection_blend = 0.15
    nt.links.new(div.outputs["Vector"], img.inputs["Vector"])
    mul = nt.nodes.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    nt.links.new(img.outputs["Color"], mul.inputs[6])
    mul.inputs[7].default_value = (*tint, 1)
    nt.links.new(mul.outputs[2], bsdf.inputs["Base Color"])
    npath = os.path.join(TEX, tex + "_N.png")
    if os.path.isfile(npath):
        nimg = nt.nodes.new("ShaderNodeTexImage")
        nimg.image = bpy.data.images.load(npath, check_existing=True)
        nimg.image.colorspace_settings.name = "Non-Color"
        nimg.projection = "BOX"
        nimg.projection_blend = 0.15
        nt.links.new(div.outputs["Vector"], nimg.inputs["Vector"])
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nm.inputs["Strength"].default_value = 0.6
        nt.links.new(nimg.outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    bsdf.inputs["Roughness"].default_value = rough
    return m


def build(data):
    groups = {}
    for b in data["boxes"]:
        groups.setdefault(b["kind"], []).append(b)
    lights = groups.pop("light", [])
    for kind, boxes in groups.items():
        bm = bmesh.new()
        for i, b in enumerate(boxes):
            c, s = ue(b["c"]), Vector((b["s"][0], b["s"][1], b["s"][2])) / 100.0
            if kind in ("wall", "trim"):
                # Murs qui se chevauchent dans le meme plan (aux portes) : sans effet visible dans le jeu (meme materiau
                # projete), mais Cycles les rend en noir. Epaisseur reduite d'une fraction de millimetre par boite.
                k = 1.0 - 0.00004 * (1 + i % 7)
                if s.x < s.y:
                    s.x *= k
                else:
                    s.y *= k
            bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Translation(c) @ Matrix.Diagonal((s.x, s.y, s.z, 1.0)))
        me = bpy.data.meshes.new(kind)
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new(kind, me)
        ob.data.materials.append(material(kind))
        bpy.context.scene.collection.objects.link(ob)
    # Neons : dalle emissive au plafond + lampe surfacique dessous (comme ABRChunk::AddLight, lampes surfaciques)
    glow = bpy.data.materials.new("glow")
    glow.use_nodes = True
    gb = next(n for n in glow.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    gb.inputs["Emission Color"].default_value = (*LIGHT_COL, 1)
    gb.inputs["Emission Strength"].default_value = 6.0
    for i, l in enumerate(lights):
        c = ue(l["c"])
        bpy.ops.mesh.primitive_plane_add(size=1, location=c)
        p = bpy.context.active_object
        p.scale = (1.2, 0.6, 1)
        p.data.materials.append(glow)
        ld = bpy.data.lights.new("neon%d" % i, "AREA")
        ld.shape = "RECTANGLE"
        ld.size, ld.size_y = 1.15, 0.55
        ld.energy = 55.0
        ld.color = LIGHT_COL
        lo = bpy.data.objects.new("neon%d" % i, ld)
        lo.location = c - Vector((0, 0, 0.04))
        bpy.context.scene.collection.objects.link(lo)


def scene_setup(res=(1280, 720)):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 96
    sc.cycles.max_bounces = 6
    try:
        sc.cycles.use_denoising = True
    except Exception:
        pass
    sc.render.resolution_x, sc.render.resolution_y = res
    w = bpy.data.worlds.new("W")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0, 0, 0, 1)
    sc.world = w
    try:
        sc.view_settings.view_transform = "AgX"
    except Exception:
        pass
    sc.view_settings.exposure = 0.6
    try:
        sc.render.image_settings.media_type = "IMAGE"
    except Exception:
        pass
    sc.render.image_settings.file_format = "JPEG"
    sc.render.image_settings.quality = 92


def camera(loc_cm, yaw, pitch, fov=88.0, ortho=None):
    sc = bpy.context.scene
    cd = bpy.data.cameras.new("C")
    if ortho:
        cd.type = "ORTHO"
        cd.ortho_scale = ortho
    else:
        cd.sensor_fit = "HORIZONTAL"
        cd.angle = math.radians(fov)
    cd.clip_start = 0.05
    cd.clip_end = 200
    cam = bpy.data.objects.new("C", cd)
    sc.collection.objects.link(cam)
    cam.location = ue(loc_cm)
    # UE : lacet autour de Z (main gauche) -> Blender : lacet inverse ; la camera Blender regarde vers -Z local
    fwd = Vector((math.cos(math.radians(pitch)) * math.cos(math.radians(-yaw)), math.cos(math.radians(pitch)) * math.sin(math.radians(-yaw)),
                  math.sin(math.radians(pitch))))
    cam.rotation_euler = fwd.to_track_quat("-Z", "Y").to_euler()
    sc.camera = cam
    return cam, fwd


def render(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("rendu Blender :", path)


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    data = json.load(open(args[0]))
    prefix = os.path.abspath(args[1])
    only = args[2:] or ["vue", "lampe", "coupe"]
    S = data["cell"]
    (mnx, mny), (mxx, mxy) = data["room"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene_setup()
    build(data)

    # 1. Point de vue de BRPits : fond de la cellule d'angle la plus proche du depart, regard en diagonale vers la salle
    corner = (mxx - 1, mxy - 1)
    out = (1, 1)
    loc = ((corner[0] + 0.5) * S + out[0] * (S * 0.5 - 95), (corner[1] + 0.5) * S + out[1] * (S * 0.5 - 95), 93 + 74)
    yaw = math.degrees(math.atan2(-out[1], -out[0]))
    camera(loc, yaw, -24.0)
    if "vue" in only:
        render(prefix + "_vue.jpg")

    # 2. Lampe torche au bord d'une fosse (passage entre deux fosses), regard plonge dans le puits
    for o in [o for o in bpy.data.objects if o.type == "CAMERA"]:
        bpy.data.objects.remove(o)
    hx, hy = (mnx + 2) * S, (mny + 2) * S          # un coin interieur (fosse si percee)
    holes = [b for b in data["boxes"] if b["kind"] == "bottom"]
    if holes:
        best = min(holes, key=lambda b: (b["c"][0] - (mnx + 2) * S) ** 2 + (b["c"][1] - (mny + 2) * S) ** 2)
        hx, hy = best["c"][0], best["c"][1]
    eye = (hx - S * 0.5, hy - 40, 93 + 74)          # centre de la cellule voisine (croisement des passages)
    yaw2 = math.degrees(math.atan2(hy - eye[1], hx - eye[0]))
    cam, fwd = camera(eye, yaw2, -52.0)
    sd = bpy.data.lights.new("lampe", "SPOT")
    sd.energy = 90.0
    sd.spot_size = math.radians(60)
    sd.spot_blend = 0.5
    sd.color = (1.0, 0.93, 0.8)
    sd.shadow_soft_size = 0.02
    so = bpy.data.objects.new("lampe", sd)
    # lampe a la ceinture (BeltLightPos), faisceau sur la paroi du fond de la fosse, 1 m sous le bord
    so.location = cam.location + Vector((0.0, 0.0, -0.52))
    aim = ue((hx, hy, -100.0)) + Vector((fwd.x, fwd.y, 0.0)).normalized() * (data["hole"] / 200.0 * 0.6)
    so.rotation_euler = (aim - so.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.collection.objects.link(so)
    for o in bpy.data.objects:
        if o.type == "LIGHT" and o.name.startswith("neon"):
            o.data.energy = 3.0  # neons presque eteints : la lampe fait le travail
    if "lampe" in only:
        render(prefix + "_lampe.jpg")
    if "coupe" not in only:
        return

    # 3. Coupe verticale (orthographique) a travers une rangee de fosses : epaisseur du bord, parois, profondeur
    for o in [o for o in bpy.data.objects if o.type == "CAMERA"]:
        bpy.data.objects.remove(o)
    for o in bpy.data.objects:
        if o.type == "LIGHT" and o.name.startswith("neon"):
            o.data.energy = 55.0
    so.data.energy = 0.0
    cy = hy
    # on retire tout ce qui est devant le plan de coupe (cote camera)
    cut_y = -(cy - 2) / 100.0
    for o in list(bpy.data.objects):
        if o.type != "MESH":
            continue
        bm = bmesh.new()
        bm.from_mesh(o.data)
        doomed = [f for f in bm.faces if f.calc_center_median().y > cut_y]
        bmesh.ops.delete(bm, geom=doomed, context="FACES")
        bm.to_mesh(o.data)
        bm.free()
    cx = (mnx + mxx) * 0.5 * S
    cam, _ = camera((cx, cy - 2000, -500), 90.0, 0.0, ortho=21.0)
    bpy.context.scene.render.resolution_x, bpy.context.scene.render.resolution_y = 1280, 1280
    render(prefix + "_coupe.jpg")


if __name__ == "__main__":
    main()
