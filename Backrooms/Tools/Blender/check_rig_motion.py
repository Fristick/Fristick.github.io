"""
v4.6 : controle des maillages a squelette livres (RawAssets/Skeletal/SK_*.fbx) EN MOUVEMENT, avant toute reconstruction.

Pour chaque modele : import du FBX livre (pas du fichier de travail), poses d'un cycle de marche (4 phases), de la
poursuite (buste penche, foulee 1,5 x) et de la frappe, avec les memes amplitudes que l'animation procedurale du jeu
(BREntity.cpp, AnimateLimbs : cuisse +/- 26 deg x allure, genou flechi jusqu'a 26 deg x allure, bras +/- 20 deg, frappe :
bras lances de 105 deg). Les rotations sont faites autour de l'axe lateral du modele (de la hanche gauche a la droite),
comme le pilote FBRSkinDriver qui recopie les pivots sur les os.

Mesures (sur le maillage deforme par Blender, poids d'origine) :
- aretes etirees ou ecrasees : longueur hors de [0,6 ; 1,6] x celle du repos ;
- triangles effondres : aire < 25 % de celle du repos.
Un maillage rigide par membre donnerait 0 % ; un pli de peau normal reste sous ~1 %.

Ce sont des RENDUS BLENDER de poses fixes (Cycles, processeur), pas des captures d'Unreal.

Usage : python Tools/Blender/check_rig_motion.py -- <sortie.jpg> [modele ...]
  modeles : faceling partygoer skinstealer hound wretch clump (defaut : tous)
"""
import json
import math
import os
import sys

import bpy  # (avant bmesh / mathutils : le module bpy les enregistre)
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SKEL = os.path.join(ROOT, "RawAssets", "Skeletal")
TEX = os.path.join(ROOT, "RawAssets", "Textures")

MODELS = {
    "faceling": ("SK_Faceling", "humain", "T_Faceling"),
    "partygoer": ("SK_Partygoer", "humain", "T_Partygoer"),
    "skinstealer": ("SK_SkinStealer", "humain", None),
    "hound": ("SK_Hound", "quadrupede", "T_Hound"),
    "wretch": ("SK_Wretch", "humain", "T_Wretch"),
    "clump": ("SK_Clump", "clump", "T_Clump"),
}
LEG_AMP, ARM_AMP = 26.0, 20.0


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def load(name):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=os.path.join(SKEL, name + ".fbx"))
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == "ARMATURE")
    meshes = [o for o in new if o.type == "MESH"]
    return arm, meshes


def bone(arm, *names):
    for n in names:
        pb = arm.pose.bones.get(n)
        if pb:
            return pb
    for pb in arm.pose.bones:  # prefixes ajoutes a l'import (Armature|..., mixamorig:...)
        for n in names:
            if pb.name.endswith(n):
                return pb
    return None


def rotate(arm, pb, axis, deg):
    if pb is None or abs(deg) < 1e-3:
        return
    mw = arm.matrix_world
    head = mw @ pb.head
    rot = Matrix.Translation(head) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-head)
    pb.matrix = mw.inverted() @ rot @ mw @ pb.matrix
    bpy.context.view_layer.update()


def rest_pose(arm):
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()


def lateral_axis(arm, kind):
    mw = arm.matrix_world
    if kind == "quadrupede":
        l, r = bone(arm, "FrontUpperL", "BackUpperL"), bone(arm, "FrontUpperR", "BackUpperR")
    else:
        l, r = bone(arm, "LeftUpLeg"), bone(arm, "RightUpLeg")
    if l is None or r is None:
        return Vector((0, 1, 0))
    return ((mw @ r.head) - (mw @ l.head)).normalized()


def pose(arm, kind, phase_deg, gait=1.0, strike=0.0, lean=0.0):
    """Meme logique que ABREntity::AnimateLimbs (Swing = sin, Lift = max(0, cos), cote droit dephase de 180 deg)"""
    rest_pose(arm)
    ax = lateral_axis(arm, kind)
    p = math.radians(phase_deg)
    if kind == "clump":
        # bras libres tordus, bras d'appui au sol : ondulation de chaque bras (comme les vrilles)
        for pb in arm.pose.bones:
            if pb.name.endswith("_Upper") or pb.name.endswith("_Fore"):
                k = sum(ord(ch) for ch in pb.name) % 97 / 97.0 * 6.28
                rotate(arm, pb, ax, 25.0 * math.sin(p + k))
        return
    if kind == "humain":
        spine = bone(arm, "Spine", "Chest")
        rotate(arm, spine, ax, -16.0 * lean - 14.0 * strike)
        for side, ph in (("Left", 0.0), ("Right", math.pi)):
            swing, lift = math.sin(p + ph), max(0.0, math.cos(p + ph))
            rotate(arm, bone(arm, side + "UpLeg"), ax, LEG_AMP * swing * gait + 14.0 * strike)
            rotate(arm, bone(arm, side + "Leg"), ax, -(LEG_AMP * lift * gait + 26.0 * strike))
            # bras : en opposition avec la jambe du meme cote ; frappe : lances vers l'avant et le haut
            rotate(arm, bone(arm, side + "Arm"), ax, -ARM_AMP * swing * gait * (1 - strike) + 105.0 * strike)
            rotate(arm, bone(arm, side + "ForeArm"), ax, 10.0 * max(0.0, -swing) * gait + 10.0 * strike)
    else:
        # quadrupede : pattes en diagonale (avant gauche avec arriere droite)
        for name, ph in (("FrontUpperL", 0.0), ("BackUpperR", 0.0), ("FrontUpperR", math.pi), ("BackUpperL", math.pi)):
            swing, lift = math.sin(p + ph), max(0.0, math.cos(p + ph))
            rotate(arm, bone(arm, name), ax, 24.0 * swing * gait - 30.0 * strike * (1 if "Front" in name else 0))
            lower = name.replace("Upper", "Lower")
            sign = 1.0 if "Front" in name else -1.0
            rotate(arm, bone(arm, lower), ax, sign * 30.0 * lift * gait)


def mesh_arrays(ob, dg):
    ev = ob.evaluated_get(dg)
    me = ev.to_mesh()
    co = np.empty(len(me.vertices) * 3, dtype=np.float64)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    me.calc_loop_triangles()
    tris = np.empty(len(me.loop_triangles) * 3, dtype=np.int64)
    me.loop_triangles.foreach_get("vertices", tris)
    tris = tris.reshape(-1, 3)
    edges = np.empty(len(me.edges) * 2, dtype=np.int64)
    me.edges.foreach_get("vertices", edges)
    edges = edges.reshape(-1, 2)
    ev.to_mesh_clear()
    return co, tris, edges


def deformation(rest, posed):
    (c0, t, e), (c1, _, _) = rest, posed
    l0 = np.linalg.norm(c0[e[:, 0]] - c0[e[:, 1]], axis=1)
    l1 = np.linalg.norm(c1[e[:, 0]] - c1[e[:, 1]], axis=1)
    ok = l0 > 1e-6
    ratio = l1[ok] / l0[ok]
    bad_e = float(np.mean((ratio < 0.6) | (ratio > 1.6))) * 100.0
    a0 = 0.5 * np.linalg.norm(np.cross(c0[t[:, 1]] - c0[t[:, 0]], c0[t[:, 2]] - c0[t[:, 0]]), axis=1)
    a1 = 0.5 * np.linalg.norm(np.cross(c1[t[:, 1]] - c1[t[:, 0]], c1[t[:, 2]] - c1[t[:, 0]]), axis=1)
    ok = a0 > 1e-10
    bad_t = float(np.mean(a1[ok] / a0[ok] < 0.25)) * 100.0
    return bad_e, bad_t


def materials(meshes, tex):
    for ob in meshes:
        for slot in ob.material_slots:
            m = bpy.data.materials.new("m")
            m.use_nodes = True
            b = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
            p = os.path.join(TEX, (tex or "") + ".jpg")
            name = slot.material.name if slot.material else ""
            if tex and os.path.isfile(p) and not any(k in name for k in ("Teeth", "Eye", "Glow", "Claw", "Mouth", "Tongue")):
                t = m.node_tree.nodes.new("ShaderNodeTexImage")
                t.image = bpy.data.images.load(p, check_existing=True)
                m.node_tree.links.new(t.outputs["Color"], b.inputs["Base Color"])
            else:
                b.inputs["Base Color"].default_value = (0.55, 0.5, 0.45, 1) if "Teeth" not in name else (0.85, 0.82, 0.7, 1)
            b.inputs["Roughness"].default_value = 0.6
            slot.material = m


def render_tile(meshes, out, res=360):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 12
    try:
        sc.cycles.use_denoising = True
    except Exception:
        pass
    sc.render.resolution_x = sc.render.resolution_y = res
    if not sc.world:
        w = bpy.data.worlds.new("W")
        w.use_nodes = True
        w.node_tree.nodes["Background"].inputs[0].default_value = (0.32, 0.32, 0.34, 1)
        sc.world = w
    dg = bpy.context.evaluated_depsgraph_get()
    pts = []
    for ob in meshes:
        ev = ob.evaluated_get(dg)
        pts += [tuple(ev.matrix_world @ Vector(c)) for c in [b for b in ob.bound_box]]
    pts = np.array(pts)
    mn, mx = Vector(pts.min(0)), Vector(pts.max(0))
    ctr, rad = (mn + mx) / 2, (mx - mn).length / 2
    cam = sc.camera
    if cam is None:
        cam = bpy.data.objects.new("C", bpy.data.cameras.new("C"))
        sc.collection.objects.link(cam)
        sc.camera = cam
        sun = bpy.data.objects.new("L", bpy.data.lights.new("L", "SUN"))
        sun.data.energy = 3.5
        sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(30))
        sc.collection.objects.link(sun)
    cam.data.lens = 50
    cam.data.clip_end = 100000
    cam.location = ctr + Vector((0.35, -1.0, 0.25)).normalized() * rad * 3.2
    cam.rotation_euler = (ctr - cam.location).to_track_quat("-Z", "Y").to_euler()
    try:
        sc.render.image_settings.media_type = "IMAGE"
    except Exception:
        pass
    sc.render.image_settings.file_format = "JPEG"
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    out = os.path.abspath(args[0])
    todo = args[1:] or list(MODELS)
    tmp = os.path.join(os.path.dirname(out), "_tiles")
    os.makedirs(tmp, exist_ok=True)
    poses = [("marche 0", 0, 1.0, 0, 0), ("marche 90", 90, 1.0, 0, 0), ("marche 180", 180, 1.0, 0, 0), ("marche 270", 270, 1.0, 0, 0),
             ("poursuite", 90, 1.5, 0, 1.0), ("frappe", 0, 0.0, 1.0, 0.5)]
    report = {}
    rows = []
    for key in todo:
        name, kind, tex = MODELS[key]
        reset()
        arm, meshes = load(name)
        materials(meshes, tex)
        rest_pose(arm)
        dg = bpy.context.evaluated_depsgraph_get()
        rest = [mesh_arrays(o, dg) for o in meshes]
        row = []
        report[name] = {}
        for label, ph, gait, strike, lean in poses:
            if kind == "clump" and label in ("poursuite", "frappe"):
                continue
            pose(arm, kind, ph, gait, strike, lean)
            dg = bpy.context.evaluated_depsgraph_get()
            be = bt = 0.0
            nv = 0
            for o, r in zip(meshes, rest):
                p = mesh_arrays(o, dg)
                e, t = deformation(r, p)
                w = len(r[0])
                be += e * w
                bt += t * w
                nv += w
            be, bt = be / max(nv, 1), bt / max(nv, 1)
            report[name][label] = {"aretes_hors_bornes_pct": round(be, 3), "triangles_effondres_pct": round(bt, 3)}
            tile = os.path.join(tmp, "%s_%s.jpg" % (name, label.replace(" ", "_")))
            render_tile(meshes, tile)
            row.append((label, tile, be, bt))
            print("%-15s %-11s aretes hors bornes %.2f %%  triangles effondres %.2f %%" % (name, label, be, bt))
        rows.append((name, row))
    # Planche
    from PIL import Image, ImageDraw
    tw = 360
    cols = max(len(r) for _, r in rows)
    sheet = Image.new("RGB", (cols * tw + 140, len(rows) * (tw + 34) + 40), (14, 14, 14))
    d = ImageDraw.Draw(sheet)
    d.text((8, 8), "RENDUS BLENDER des SK_*.fbx livres, poses calculees comme AnimateLimbs (pas des captures Unreal). "
           "Sous chaque vue : aretes hors [0,6 ; 1,6] x repos / triangles < 25 % de leur aire.", fill=(230, 230, 220))
    for j, (name, row) in enumerate(rows):
        y = 40 + j * (tw + 34)
        d.text((8, y + tw // 2), name, fill=(240, 220, 120))
        for i, (label, tile, be, bt) in enumerate(row):
            im = Image.open(tile)
            sheet.paste(im, (140 + i * tw, y))
            col = (140, 230, 140) if be < 1.0 and bt < 0.5 else (240, 160, 80)
            d.text((140 + i * tw + 6, y + tw + 4), "%s : %.2f %% / %.2f %%" % (label, be, bt), fill=col)
    sheet.save(out, quality=90)
    with open(os.path.splitext(out)[0] + ".json", "w") as fh:
        json.dump(report, fh, indent=1)
    print("planche :", out)


if __name__ == "__main__":
    main()
