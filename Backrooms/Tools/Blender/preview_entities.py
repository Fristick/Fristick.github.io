"""Rendu d'apercu des entites assemblees, avec les MEMES decalages d'articulations que BREntity.cpp.
Usage : blender -b -P preview_entities.py   (ou python preview_entities.py avec le module bpy)"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: E402
import generate_models as G  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Docs", "apercu_entites.png")


def place(o, loc, parent=None, pitch=0.0, roll=0.0, sy=1.0, scale=1.0):
    o.location = loc
    o.rotation_euler = (math.radians(roll), math.radians(pitch), 0)
    o.scale = (scale, sy * scale, scale)
    if parent is not None:
        o.parent = parent
    return o


def empty(loc, parent=None, pitch=0.0, roll=0.0):
    e = bpy.data.objects.new("pivot", None)
    bpy.context.collection.objects.link(e)
    return place(e, loc, parent, pitch, roll)


def humanoid(kind, x0, swing=22.0):
    s = G.HUMANOIDS[kind]
    root = empty((x0, 0, 0))
    place(G.m_torso(kind), (0, 0, s["hip"]), root)
    place(G.m_head(kind), (s["hunch"] * 1.05, 0, s["sh"] + s["neck"]), root, pitch=-10 if kind != "Partygoer" else 0)
    for side in (-1, 1):
        ua = empty((s["hunch"], side * s["sw"], s["sh"]), root, pitch=side * swing * 0.8)
        G.m_upper_arm(kind).parent = ua
        la = empty((0, 0, -s["ua"]), ua, pitch=-20)
        G.m_lower_arm(kind).parent = la
        if kind == "Partygoer" and side > 0:
            place(G.m_balloon(), (0.02, 0, -s["la"] + 0.05), la)
        th = empty((0, side * s["hw"], s["hip"]), root, pitch=-side * swing)
        G.m_thigh(kind).parent = th
        sh = empty((0, 0, -s["th"]), th, pitch=25 if side > 0 else 5)
        G.m_shin(kind).parent = sh
    return root


def hound(x0):
    H = G.HOUND
    root = empty((x0, 0, 0))
    body = place(G.m_hound_body(), (0, 0, H["body_z"]), root)
    place(G.m_hound_head(), H["head"], body, pitch=15)
    poses = [(H["front"], 1, 25, -45), (H["back"], -1, -30, 40)]
    for (fx, fy), _, up, lo in poses:
        for side in (-1, 1):
            u = empty((fx, side * fy, 0), body, pitch=up, roll=side * -12)
            G.m_hound_upper().parent = u
            low = empty((0, 0, -H["upper"]), u, pitch=lo)
            G.m_hound_lower().parent = low
    return root


G.reset()
xs = iter([0.0, 1.3, 2.6, 3.9, 5.3, 6.9])
humanoid("Faceling", next(xs))
humanoid("SkinStealer", next(xs))
humanoid("Wretch", next(xs))
humanoid("Partygoer", next(xs))
humanoid("Bacteria", next(xs))
hound(next(xs))
mass = place(G.m_skinstealer_mass(), (8.4, 0, 0))
moth = place(G.m_moth_body(), (9.9, 0, 1.3))
place(G.m_moth_wing(), (9.9, 0.08, 1.34))
place(G.m_moth_wing(), (9.9, -0.08, 1.34), sy=-1.0)
place(G.m_smiler(), (11.3, 0, 1.5))
place(G.m_clump(), (12.8, 0, 0.6))
bpy.ops.mesh.primitive_plane_add(size=40, location=(6.5, 0, 0))

sc = bpy.context.scene
sc.render.engine = "CYCLES"
sc.cycles.samples = 32
sc.render.resolution_x = 1800
sc.render.resolution_y = 520
w = bpy.data.worlds.new("W")
w.use_nodes = True
w.node_tree.nodes["Background"].inputs[0].default_value = (0.42, 0.42, 0.44, 1)
sc.world = w
cd = bpy.data.cameras.new("C")
cd.type = "ORTHO"
cd.ortho_scale = 14.5
cam = bpy.data.objects.new("C", cd)
sc.collection.objects.link(cam)
cam.location = (6.4 + 6.5, -9.5, 1.5)
cam.rotation_euler = (math.radians(87), 0, math.radians(34))
sc.camera = cam
ld = bpy.data.lights.new("L", "SUN")
ld.energy = 3
lo = bpy.data.objects.new("L", ld)
lo.rotation_euler = (math.radians(45), 0, math.radians(60))
sc.collection.objects.link(lo)
sc.render.filepath = os.path.normpath(OUT)
bpy.ops.render.render(write_still=True)
print("Apercu :", OUT)
