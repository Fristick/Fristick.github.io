"""Rendu d apercu de toutes les entites assemblees (memes decalages d articulations que le C++).
Usage : blender -b -P preview_entities.py   (ou python preview_entities.py avec le module bpy)"""
import sys, math
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import generate_models as G
import bpy
from mathutils import Vector

def place(obj, loc, sy=1, pitch=0):
    obj.location = loc
    obj.scale = (1, sy, 1)
    obj.rotation_euler = (0, math.radians(pitch), 0)

def humanoid(kind, x0, pose=20):
    s = G.HUMANOIDS[kind]
    t = G.m_humanoid_torso(kind); place(t, (x0, 0, s['hip']))
    for side in (-1, 1):
        a = G.m_humanoid_arm(kind); place(a, (x0 + s['hunch'], side * s['sw'], s['sh']), pitch=side * pose)
        l = G.m_humanoid_leg(kind); place(l, (x0, side * s['hw'], s['hip']), pitch=-side * pose)

G.reset()
humanoid('Faceling', 0); humanoid('SkinStealer', 1.2); humanoid('Wretch', 2.4); humanoid('Partygoer', 3.6)
# hound
H = G.HOUND
b = G.m_hound_body(); place(b, (5.2, 0, H['body_z']))
for (fx, fy) in (H['front'], H['back']):
    for side in (-1, 1):
        l = G.m_hound_leg(); place(l, (5.2 + fx, side * fy, H['body_z']))
# moth
mb = G.m_moth_body(); place(mb, (6.8, 0, 1.2))
w1 = G.m_moth_wing(); place(w1, (6.8, 0.08, 1.24))
w2 = G.m_moth_wing(); place(w2, (6.8, -0.08, 1.24), sy=-1)
sm = G.m_smiler(); place(sm, (8.2, 0, 1.4))
cl = G.m_clump(); place(cl, (9.6, 0, 0.6))
bpy.ops.mesh.primitive_plane_add(size=30, location=(5,0,0))
# render from front-ish (+X side)
sc = bpy.context.scene
sc.render.engine='CYCLES'; sc.cycles.samples=24; sc.render.resolution_x=1400; sc.render.resolution_y=420
w = bpy.data.worlds.new('W'); w.use_nodes=True; w.node_tree.nodes['Background'].inputs[0].default_value=(0.4,0.4,0.42,1); sc.world=w
cd = bpy.data.cameras.new('C'); cd.type='ORTHO'; cd.ortho_scale=11.5
cam = bpy.data.objects.new('C', cd); sc.collection.objects.link(cam)
cam.location=(4.9+6, -9, 1.6); cam.rotation_euler=(math.radians(88), 0, math.radians(35))
sc.camera=cam
ld=bpy.data.lights.new('L','SUN'); ld.energy=3; lo=bpy.data.objects.new('L',ld); lo.rotation_euler=(math.radians(45),0,math.radians(60)); sc.collection.objects.link(lo)
sc.render.filepath=__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), '..', '..', 'Docs', 'apercu_entites.png')
bpy.ops.render.render(write_still=True)
