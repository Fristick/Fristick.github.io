"""v4.17 : compare deux sorties du kit du Level 0 (build_level0_kit.py) module par module, et rend des vues rapprochees.

Usage (Blender 5.x, en ligne de commande) :
    blender -b --factory-startup -P Tools/Blender/compare_level0_kit.py -- --a <sortie de reference> --b <nouvelle sortie>
            --out <dossier du rapport> [--same <sortie a comparer a la reference, geometrie identique attendue>]
            [--render] [--samples 96] [--res 960x600]

Ce qu'il fait (lecture seule : les dossiers compares ne sont jamais modifies) :
 1. relit chaque FBX de <a>/FBX et <b>/FBX (et Archive/) dans une scene vide, comme l'import d'Unreal le lirait :
    noms des objets, pivot (transformation de l'objet), boite englobante locale, triangles, n-gones, emplacements de
    materiaux dans l'ordre, canaux UV, couleurs de sommet, normales personnalisees, collisions UCX_ (noms, nombre de
    sommets, boites) ;
 2. compare <a> et <b> : memes modules, memes pivots, memes boites (1e-4 m), memes materiaux, memes UV, collisions
    identiques (1e-5 m) ; triangles avant / apres ;
 3. plans de contact : pour chaque plan de la boite d'un module, l'enveloppe des sommets qui y sont et l'aire des faces
    qui y sont, avant / apres (un chanfrein ne doit pas reduire un plan de contact : mur, sol, joint) ; et les sections
    des plinthes aux joints (plinthe de mur contre plinthe de poteau, plinthe de jambage contre chambranle) ;
 4. --same : signature de la geometrie (sommets, UV) identique a <a> (ex. sortie --no-bevels contre la v4.16) ;
 5. --render : vues rapprochees avant / apres sous lumiere rasante (plinthe, chambranle, porte de service, luminaire),
    PNG dans <out>/Renders ; avant a gauche, apres a droite dans *_avant_apres.png.
Ecrit <out>/compare_level0_kit.json et <out>/compare_level0_kit.md.
"""
import argparse
import hashlib
import json
import math
import os
import sys
import time

import bpy
import bmesh
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_level0_kit as K  # noqa: E402  (constantes et materiaux du kit ; main() n'est pas appele)

EPS_BOX = 1e-4
EPS_UCX = 1e-5


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", required=True, help="sortie de reference (avant)")
    ap.add_argument("--b", required=True, help="nouvelle sortie (apres)")
    ap.add_argument("--same", default="", help="sortie dont la geometrie doit etre identique a --a")
    ap.add_argument("--out", required=True)
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--samples", type=int, default=96)
    ap.add_argument("--res", default="960x600")
    return ap.parse_args(argv)


# ----------------------------------------------------------------------------------------------- lecture des FBX
def fbx_files(root):
    out = {}
    for sub in ("FBX", os.path.join("FBX", "Archive")):
        d = os.path.join(root, sub)
        if os.path.isdir(d):
            for f in sorted(os.listdir(d)):
                if f.lower().endswith(".fbx"):
                    out[os.path.splitext(f)[0]] = os.path.join(d, f)
    return out


def clear_scene():
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    for me in list(bpy.data.meshes):
        if me.users == 0:
            bpy.data.meshes.remove(me)


def base(name):
    return name.split(".")[0] if len(name) > 4 and name[-4] == "." and name[-3:].isdigit() else name


def mesh_info(ob):
    me = ob.data
    xs = [v.co for v in me.vertices]
    lo = [min(c[i] for c in xs) for i in range(3)] if xs else [0, 0, 0]
    hi = [max(c[i] for c in xs) for i in range(3)] if xs else [0, 0, 0]
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    ngons = sum(1 for p in me.polygons if len(p.vertices) > 4)
    smooth = sum(1 for p in me.polygons if p.use_smooth)
    # Coins dont la normale s'ecarte de celle de leur face de plus de 1 degre (normales lissees des chanfreins)
    bent = 0
    for p in me.polygons:
        for li in p.loop_indices:
            if me.corner_normals[li].vector.dot(p.normal) < math.cos(math.radians(1.0)):
                bent += 1
    return {"lo": [round(v, 6) for v in lo], "hi": [round(v, 6) for v in hi], "verts": len(me.vertices), "polys": len(me.polygons),
            "tris": tris, "ngons": ngons, "smooth_faces": smooth, "bent_corners": bent, "custom_normals": bool(me.has_custom_normals),
            "materials": [base(m.name) if m else None for m in me.materials], "uv": [u.name for u in me.uv_layers],
            "colors": [c.name for c in me.color_attributes], "uv_bevels": uv_bevels(me)}


# Echelle des UV0 par materiau (build_level0_kit.TEX_WORLD, par nom de materiau)
MAT_WORLD = {"M_L0_Baseboard": 1.0, "M_L0_DoorSteel": 1.0, "M_L0_DoorHardware": 0.5, "M_L0_FixtureMetal": 1.0}


def uv_bevels(me):
    """Faces lisses (chanfreins) : UV0 continues avec au moins une voisine (arete commune aux memes UV) et densite de texels
    (aire UV / aire 3D, ramenee a l'echelle du materiau : 1,0 = celle des faces planes)"""
    if "UVMap" not in me.uv_layers:
        return None
    bm = bmesh.new()
    bm.from_mesh(me)
    uvl = bm.loops.layers.uv["UVMap"]

    def uv_at(face, v):
        for lp in face.loops:
            if lp.vert == v:
                return lp[uvl].uv
        return None

    n = cont = 0
    ratios = []
    for f in bm.faces:
        if not f.smooth:
            continue
        n += 1
        ok = False
        for e in f.edges:
            for g in e.link_faces:
                if g is f:
                    continue
                if all((uv_at(f, v) - uv_at(g, v)).length < 1e-5 for v in e.verts):
                    ok = True
        cont += 1 if ok else 0
        uvs = [lp[uvl].uv for lp in f.loops]
        ua = abs(sum(uvs[i].x * uvs[(i + 1) % len(uvs)].y - uvs[(i + 1) % len(uvs)].x * uvs[i].y for i in range(len(uvs)))) / 2
        m = me.materials[f.material_index]
        w = MAT_WORLD.get(base(m.name) if m else "", 1.0)
        if f.calc_area() > 1e-12:
            ratios.append(ua / f.calc_area() * w * w)
    bm.free()
    if not n:
        return None
    return {"faces": n, "uv_continuous": cont, "density_min": round(min(ratios), 4), "density_max": round(max(ratios), 4)}


def geometry_signature(ob):
    me = ob.data
    pts = sorted(tuple(round(c, 5) for c in v.co) for v in me.vertices)
    uvs = []
    for layer in me.uv_layers:
        uvs.append(sorted(tuple(round(c, 5) for c in d.uv) for d in layer.data))
    h = hashlib.sha1(json.dumps([pts, uvs, sorted(len(p.vertices) for p in me.polygons)]).encode()).hexdigest()
    return h


def read_fbx(path, name):
    clear_scene()
    bpy.ops.import_scene.fbx(filepath=path)
    objs = list(bpy.context.scene.objects)
    main = [o for o in objs if o.type == "MESH" and base(o.name) == name]
    ucx = sorted([o for o in objs if o.type == "MESH" and o.name.startswith("UCX_")], key=lambda o: o.name)
    rec = {"objects": sorted(base(o.name) for o in objs)}
    if main:
        ob = main[0]
        rec["pivot"] = {"location": [round(v, 6) for v in ob.location], "rotation_deg": [round(math.degrees(v), 4) for v in ob.rotation_euler],
                        "scale": [round(v, 6) for v in ob.scale]}
        rec.update(mesh_info(ob))
        rec["signature"] = geometry_signature(ob)
        rec["planes"] = contact_planes(ob)
    rec["ucx"] = []
    for u in ucx:
        mi = mesh_info(u)
        rec["ucx"].append({"name": base(u.name), "verts": mi["verts"], "lo": mi["lo"], "hi": mi["hi"],
                           "pivot": [round(v, 6) for v in u.location], "box": mi["verts"] == 8 and mi["polys"] == 6})
    return rec


# ----------------------------------------------------------------------------------------------- plans de contact
def hull_area(pts2):
    pts = sorted(set(pts2))
    if len(pts) < 3:
        return 0.0

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    h = lower[:-1] + upper[:-1]
    return abs(sum(h[i][0] * h[(i + 1) % len(h)][1] - h[(i + 1) % len(h)][0] * h[i][1] for i in range(len(h)))) / 2


def contact_planes(ob, tol=5e-5):
    """Pour les six plans de la boite : aire de l'enveloppe des sommets du plan, aire des faces dans le plan"""
    me = ob.data
    co = [v.co for v in me.vertices]
    out = {}
    for ax in range(3):
        for side, val in (("min", min(c[ax] for c in co)), ("max", max(c[ax] for c in co))):
            o = [i for i in range(3) if i != ax]
            on = [(round(c[o[0]], 6), round(c[o[1]], 6)) for c in co if abs(c[ax] - val) < tol]
            fa = sum(p.area for p in me.polygons if all(abs(me.vertices[v].co[ax] - val) < tol for v in p.vertices))
            out["xyz"[ax] + side] = {"at": round(val, 5), "hull": round(hull_area(on), 7), "faces": round(fa, 7)}
    return out


def sections(name, path):
    """Points de la section d'un module dans un plan (bouts des plinthes) : renvoie {plan : [(u, v)]}"""
    clear_scene()
    bpy.ops.import_scene.fbx(filepath=path)
    ob = [o for o in bpy.context.scene.objects if o.type == "MESH" and base(o.name) == name][0]
    me = ob.data
    return me, ob


def cap_points(me, axis, value, sign, cond=lambda c: True, tol=5e-5):
    """Sommets des faces dont la normale suit sign * axe et qui sont dans le plan axis = value"""
    out = set()
    for p in me.polygons:
        if p.normal[axis] * sign < 0.999:
            continue
        vs = [me.vertices[v].co for v in p.vertices]
        if all(abs(c[axis] - value) < tol for c in vs) and all(cond(c) for c in vs):
            o = [i for i in range(3) if i != axis]
            for c in vs:
                out.add((round(abs(c[o[0]]), 5), round(c[o[1]], 5)))
    return sorted(out)


def joint_checks(root):
    """Sections aux joints de plinthes (meme profil des deux cotes) et pied du chambranle"""
    files = fbx_files(root)
    T, S, BH, BD = K.T, K.S, K.BASE_H, K.BASE_D
    res = {}
    me, ob = sections("SM_L0_Baseboard_290", files["SM_L0_Baseboard_290"])
    wall_end = cap_points(me, 0, -K.SPAN / 2, -1.0)
    res["Baseboard_290_bout"] = wall_end
    for post, axis_v, sgn in (("SM_L0_Wall_Straight", T / 2, 1.0), ("SM_L0_Wall_JunctionT", T / 2, 1.0), ("SM_L0_Wall_Corner", T / 2, 1.0),
                              ("SM_L0_Wall_End", T / 2, 1.0)):
        me, ob = sections(post, files[post])
        # Bouts des plinthes du poteau sur le plan x = +T/2 (la ou arrive un mur du bras +X), de chaque cote
        pts = cap_points(me, 0, axis_v, sgn)
        res[post + "_bout_x+"] = pts
    me, ob = sections("SM_L0_Baseboard_070", files["SM_L0_Baseboard_070"])
    res["Baseboard_070_bout"] = cap_points(me, 0, -(K.SPAN - K.DOOR_W) / 4, -1.0)
    me, ob = sections("SM_L0_DoorCasing", files["SM_L0_DoorCasing"])
    # Flanc exterieur du jambage gauche (normale -X, x = -DOOR_W/2 - 9 cm), cote +Y : saillie du flanc avant son chanfrein
    flank = [me.vertices[v].co for p in me.polygons if p.normal.x < -0.999 for v in p.vertices
             if abs(me.vertices[v].co.x - (-K.DOOR_W / 2 - 0.09)) < 5e-5 and me.vertices[v].co.y > 0]
    res["DoorCasing_flanc_saillie_m"] = round(max(c.y for c in flank) - T / 2, 5) if flank else None
    # Dos du chambranle contre le mur (plan y = +T/2, normale -Y) : aire des faces (doit rester celle de la v4.16)
    res["DoorCasing_dos_contre_mur_m2"] = round(sum(p.area for p in me.polygons if p.normal.y < -0.999
                                                    and all(abs(me.vertices[v].co.y - T / 2) < 5e-5 for v in p.vertices)), 6)
    # Cadre de la porte de service : chant exterieur dans l'epaisseur du mur (y = -0,65, x <= 0) : aire des faces
    me, ob = sections("SM_L0_ServiceDoor_Frame", files["SM_L0_ServiceDoor_Frame"])
    yo = -K.SVC_HOLE_W / 2
    def clip(poly2, xmax):
        """Partie x <= xmax d'un polygone convexe (x, z)"""
        out = []
        for i in range(len(poly2)):
            a, b = poly2[i], poly2[(i + 1) % len(poly2)]
            ina, inb = a[0] <= xmax, b[0] <= xmax
            if ina:
                out.append(a)
            if ina != inb:
                t = (xmax - a[0]) / (b[0] - a[0])
                out.append((xmax, a[1] + t * (b[1] - a[1])))
        return out

    # Enveloppe des faces du chant coupees a x = 0 (les faces de la v4.16 se recouvrent : l'enveloppe, pas la somme)
    pts = [(round(x, 6), round(z, 6)) for p in me.polygons if p.normal.y < -0.999 and all(abs(me.vertices[v].co.y - yo) < 5e-5 for v in p.vertices)
           for x, z in clip([(me.vertices[v].co.x, me.vertices[v].co.z) for v in p.vertices], 0.0)]
    res["ServiceFrame_chant_dans_mur_m2"] = round(hull_area(pts), 6)
    res["ServiceFrame_chant_xmax_m"] = round(max((me.vertices[v].co.x for p in me.polygons if p.normal.y < -0.999 for v in p.vertices
                                                  if abs(me.vertices[v].co.y - yo) < 5e-5), default=0.0), 5)
    res["Baseboard_saillie_m"] = BD
    res["Baseboard_hauteur_m"] = BH
    return res


# ----------------------------------------------------------------------------------------------- comparaison
def close(a, b, eps):
    return all(abs(x - y) <= eps for x, y in zip(a, b))


def compare(A, B):
    rows, problems = [], []
    for name in sorted(set(A) | set(B)):
        a, b = A.get(name), B.get(name)
        if a is None or b is None:
            problems.append("%s : present seulement dans %s" % (name, "b" if a is None else "a"))
            continue
        r = {"module": name, "tris_a": a.get("tris"), "tris_b": b.get("tris"), "verts_a": a.get("verts"), "verts_b": b.get("verts"),
             "bent_b": b.get("bent_corners"), "smooth_b": b.get("smooth_faces"), "custom_b": b.get("custom_normals"), "ngons_b": b.get("ngons"),
             "uv_bevels_b": b.get("uv_bevels")}
        r["objects_same"] = a["objects"] == b["objects"]
        r["pivot_same"] = a.get("pivot") == b.get("pivot")
        r["box_same"] = close(a["lo"], b["lo"], EPS_BOX) and close(a["hi"], b["hi"], EPS_BOX)
        r["dims_m"] = [round(b["hi"][i] - b["lo"][i], 4) for i in range(3)]
        r["materials_same"] = a["materials"] == b["materials"]
        r["uv_same"] = a["uv"] == b["uv"] and a["colors"] == b["colors"]
        ua, ub = a["ucx"], b["ucx"]
        r["ucx"] = len(ub)
        r["ucx_same"] = len(ua) == len(ub) and all(x["name"] == y["name"] and x["verts"] == y["verts"] and x["box"] == y["box"]
                                                   and close(x["lo"], y["lo"], EPS_UCX) and close(x["hi"], y["hi"], EPS_UCX)
                                                   and x["pivot"] == y["pivot"] for x, y in zip(ua, ub))
        r["geometry_same"] = a.get("signature") == b.get("signature")
        planes = {}
        for k, pa in a.get("planes", {}).items():
            pb = b["planes"][k]
            if abs(pa["hull"] - pb["hull"]) > 1e-7 or abs(pa["faces"] - pb["faces"]) > 1e-7:
                planes[k] = {"at": pa["at"], "hull_a": pa["hull"], "hull_b": pb["hull"], "faces_a": pa["faces"], "faces_b": pb["faces"]}
        r["planes_changed"] = planes
        for key in ("objects_same", "pivot_same", "box_same", "materials_same", "uv_same", "ucx_same"):
            if not r[key]:
                problems.append("%s : %s" % (name, key))
        if b.get("ngons"):
            problems.append("%s : %d n-gone(s) (tangentes non exportees)" % (name, b["ngons"]))
        rows.append(r)
    return rows, problems


# ----------------------------------------------------------------------------------------------- rendus
def setup_cycles(args):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    device = "CPU"
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        for kind in ("OPTIX", "CUDA", "HIP", "ONEAPI"):
            try:
                prefs.compute_device_type = kind
                prefs.get_devices()
                gpus = [d for d in prefs.devices if d.type == kind]
                if gpus:
                    for d in prefs.devices:
                        d.use = d.type == kind
                    device = kind
                    break
            except Exception:
                continue
    except Exception:
        pass
    sc.cycles.device = "GPU" if device != "CPU" else "CPU"
    sc.cycles.samples = args.samples
    sc.cycles.use_denoising = True
    sc.cycles.max_bounces = 6
    sc.cycles.diffuse_bounces = 3
    sc.cycles.glossy_bounces = 3
    w, h = (int(v) for v in args.res.split("x"))
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.render.image_settings.file_format = "PNG"
    sc.view_settings.view_transform = "AgX"
    sc.view_settings.exposure = 0.0
    return device


class Stage:
    """Scene de rendu : modules d'un dossier de sortie (FBX relus), materiaux du kit (textures du dossier)"""

    def __init__(self, root):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        self.root = root
        self.files = fbx_files(root)
        self.M = K.make_materials(os.path.join(root, "Textures"))
        self.by_name = {m.name: m for m in self.M.values() if m is not None}
        self.lib = {}
        self.coll = bpy.context.scene.collection
        w = bpy.data.worlds.new("W")
        w.use_nodes = True
        w.node_tree.nodes["Background"].inputs[0].default_value = (0.012, 0.012, 0.010, 1)
        bpy.context.scene.world = w

    def mesh(self, name):
        if name not in self.lib:
            before = set(bpy.data.objects)
            bpy.ops.import_scene.fbx(filepath=self.files[name])
            new = [o for o in bpy.data.objects if o not in before]
            main = [o for o in new if o.type == "MESH" and base(o.name) == name][0]
            me = main.data
            for i, m in enumerate(me.materials):
                if m and base(m.name) in self.by_name:
                    me.materials[i] = self.by_name[base(m.name)]
            for o in new:
                bpy.data.objects.remove(o, do_unlink=True)
            self.lib[name] = me
        return self.lib[name]

    def put(self, name, loc, yaw=0.0):
        ob = bpy.data.objects.new(name, self.mesh(name))
        ob.location = loc
        ob.rotation_euler = (0, 0, math.radians(yaw))
        self.coll.objects.link(ob)
        return ob

    def area(self, loc, target, energy, size=0.4, color=(1.0, 0.985, 0.86)):
        ld = bpy.data.lights.new("L", "AREA")
        ld.energy = energy
        ld.size = size
        ld.color = color
        ob = bpy.data.objects.new("L", ld)
        ob.location = loc
        ob.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        self.coll.objects.link(ob)

    def camera(self, loc, target, lens=35.0):
        cd = bpy.data.cameras.new("C")
        cd.lens = lens
        cd.clip_start = 0.01
        ob = bpy.data.objects.new("C", cd)
        ob.location = loc
        ob.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        self.coll.objects.link(ob)
        bpy.context.scene.camera = ob


def scene_plinth(st, view):
    S, T = K.S, K.T
    if view == "rentrant":
        # Angle rentrant : poteau d'angle (bras +X et +Y), plinthes des deux murs qui se croisent dans l'angle (comme en jeu)
        st.put("SM_L0_Wall_Corner", (0, 0, 0))
        st.put("SM_L0_Wall_350", (S / 2, 0, 0), 0)
        st.put("SM_L0_Baseboard_290", (S / 2, 0, 0), 0)
        st.put("SM_L0_Wall_350", (0, S / 2, 0), 90)
        st.put("SM_L0_Baseboard_290", (0, S / 2, 0), 270)
        st.put("SM_L0_Floor_350", (S / 2, S / 2, 0))
        st.camera((T / 2 + 0.36, T / 2 + 0.30, 0.30), (T / 2 + 0.005, T / 2 + 0.005, 0.10), 35)
        st.area((T / 2 + 1.2, T / 2 + 0.9, K.H - 0.05), (T / 2, T / 2, 0.1), 60, 0.6)
        st.area((T / 2 + 1.6, T / 2 + 0.08, 0.30), (T / 2, T / 2 + 0.02, 0.1), 12, 0.2)  # rasante le long du mur +X
        return
    st.put("SM_L0_Wall_350", (0, 0, 0))
    st.put("SM_L0_Baseboard_290", (0, 0, 0), 0)
    st.put("SM_L0_Baseboard_290", (0, 0, 0), 180)
    st.put("SM_L0_Wall_End", (S / 2, 0, 0), 180)  # bout de mur : la plinthe fait le tour (deux angles saillants)
    st.put("SM_L0_Wall_Straight", (-S / 2, 0, 0), 0)
    st.put("SM_L0_Wall_350", (-S, 0, 0))
    st.put("SM_L0_Baseboard_290", (-S, 0, 0), 0)
    for x in (-S, 0, S):
        for y in (-S / 2, S / 2):
            st.put("SM_L0_Floor_350", (x, y, 0))
    if view == "angle":
        # Angle saillant de la plinthe au bout du mur (onglet), a 45 cm
        st.camera((S / 2 + T / 2 + 0.33, T / 2 + 0.33, 0.33), (S / 2 + T / 2 - 0.02, T / 2 - 0.02, 0.08), 35)
        st.area((S / 2 - 0.8, T / 2 + 0.9, K.H - 0.05), (S / 2 + 0.2, T / 2, 0.1), 60, 0.6)  # neon du plafond, en avant du mur
        st.area((-0.5, T / 2 + 0.10, 0.30), (S / 2, T / 2 + 0.01, 0.1), 15, 0.2)  # rasante, le long de la face du mur
    else:
        # Raccord plinthe de mur / plinthe de poteau (x = -S/2 + T/2), lumiere rasante le long du mur
        st.camera((-S / 2 + T / 2 + 0.30, T / 2 + 0.26, 0.26), (-S / 2 + T / 2 - 0.05, T / 2, 0.09), 35)
        st.area((-S / 2 - 1.5, T / 2 + 0.10, 0.35), (-S / 2 + 0.6, T / 2 + 0.01, 0.1), 25, 0.3)
        st.area((-S / 2 + 0.6, T / 2 + 1.2, K.H - 0.05), (-S / 2, T / 2, 0.1), 30, 0.6)


def scene_casing(st, view):
    T, DW, DH = K.T, K.DOOR_W, K.DOOR_H
    side = (K.SPAN - DW) / 2
    st.put("SM_L0_Doorway", (0, 0, 0))
    st.put("SM_L0_DoorCasing", (0, 0, 0))
    for s in (-1, 1):
        for face in (0, 180):
            st.put("SM_L0_Baseboard_070", (s * (DW / 2 + side / 2), 0, 0), face)
    for y in (-K.S / 2, K.S / 2):
        st.put("SM_L0_Floor_350", (0, y, 0))
    if view == "angle":
        st.camera((-DW / 2 + 0.23, T / 2 + 0.50, DH - 0.27), (-DW / 2 - 0.05, T / 2 + 0.01, DH + 0.04), 45)
        st.area((0.6, T / 2 + 1.0, K.H - 0.05), (-DW / 2 - 0.05, T / 2, DH), 45, 0.5)
        st.area((-DW / 2 - 1.4, T / 2 + 0.12, DH + 0.15), (-DW / 2, T / 2 + 0.02, DH + 0.04), 10, 0.2)  # rasante, dans le plan du mur
    else:
        st.camera((-DW / 2 + 0.22, T / 2 + 0.30, 0.28), (-DW / 2 - 0.06, T / 2 + 0.01, 0.07), 40)
        st.area((0.6, T / 2 + 0.9, K.H - 0.05), (-DW / 2, T / 2, 0.1), 45, 0.5)
        st.area((-DW / 2 - 1.5, T / 2 + 0.2, 0.3), (-DW / 2, T / 2 + 0.02, 0.1), 10, 0.2)


def scene_service_door(st, view):
    T = K.T
    clear = K.SVC_HOLE_W - 0.2
    st.put("SM_L0_ServiceDoor_Wall", (0, 0, 0))
    st.put("SM_L0_ServiceDoor_Frame", (0, 0, 0))
    hinge = Vector((-T + 0.08, clear / 2 - 0.015, 0))
    leaf_w = clear - 0.03
    st.put("SM_L0_ServiceDoor_Leaf", hinge)
    st.put("SM_L0_ServiceDoor_Handle", hinge + Vector((0, -leaf_w + 0.09, 1.04)))
    for z in (0.28, 1.1, 1.92):
        st.put("SM_L0_ServiceDoor_Hinge", hinge + Vector((0, 0, z)))
    st.put("SM_L0_Floor_350", (K.S / 2, 0, 0))
    hp = hinge + Vector((0.0225 + 0.03, -leaf_w + 0.09 + 0.07, 1.04))
    if view == "poignee":
        st.camera(hp + Vector((0.36, 0.20, 0.15)), hp + Vector((0, -0.01, -0.02)), 50)
        st.area((0.9, 0.9, K.H - 0.05), hp, 30, 0.5)
        st.area(Vector((-0.40, -0.52, 1.10)), hp, 4, 0.15)  # rasante, le long du battant (dans l'embrasure)
    else:
        st.camera((0.50, 0.12, 1.88), (0.01, clear / 2 + 0.03, K.SVC_HOLE_H - 0.07), 45)
        st.area((1.1, -0.6, K.H - 0.05), (0.0, clear / 2, K.SVC_HOLE_H - 0.1), 50, 0.5)
        st.area((0.15, -1.8, 2.0), (0.02, clear / 2 + 0.05, K.SVC_HOLE_H - 0.05), 6, 0.2)


def scene_light(st, view):
    H = K.H
    st.put("SM_L0_Ceiling_350", (0, 0, 0))
    if view == "allume":
        st.put("SM_L0_Light_Panel", (0, 0, 0))
        st.camera((0.80, -0.22, 2.45), (0.60, 0.29, H - 0.005), 45)
        st.area((-1.4, 0.25, H - 0.12), (0.6, 0.3, H - 0.01), 60, 0.6)  # autre neon, lumiere rasante sous le plafond
    else:
        # Neon en panne (diffuseur eteint) : cadre, chanfreins et feuillure sous une lumiere rasante, a 40 cm
        st.put("SM_L0_Light_Panel_Off", (0, 0, 0))
        st.camera((0.86, -0.02, 2.56), (0.585, 0.16, H - 0.006), 45)
        st.area((-1.6, 0.05, H - 0.10), (0.6, 0.1, H - 0.01), 60, 0.6)
        st.area((1.2, 1.4, 1.6), (0.6, 0.3, H), 15, 0.8)


SCENES = (("plinthe_angle", scene_plinth, "angle"), ("plinthe_raccord", scene_plinth, "raccord"), ("plinthe_rentrant", scene_plinth, "rentrant"), ("chambranle_angle", scene_casing, "angle"),
          ("chambranle_pied", scene_casing, "pied"), ("porte_service_poignee", scene_service_door, "poignee"),
          ("porte_service_cadre", scene_service_door, "cadre"), ("luminaire_allume", scene_light, "allume"),
          ("luminaire_eteint", scene_light, "eteint"))


def side_by_side(pa, pb, out):
    ia, ib = bpy.data.images.load(pa), bpy.data.images.load(pb)
    w, h = ia.size
    gap = 6
    im = bpy.data.images.new("ab", w * 2 + gap, h, alpha=False)
    import numpy as np
    A = np.array(ia.pixels[:], dtype=np.float32).reshape(h, w, 4)
    B = np.array(ib.pixels[:], dtype=np.float32).reshape(h, w, 4)
    G = np.ones((h, gap, 4), dtype=np.float32)
    im.pixels[:] = np.concatenate([A, G, B], axis=1).ravel()
    im.filepath_raw = out
    im.file_format = "PNG"
    im.save()


def render_all(args, out_dir):
    rd = os.path.join(out_dir, "Renders")
    os.makedirs(rd, exist_ok=True)
    times = {}
    device = None
    for name, fn, view in SCENES:
        paths = []
        for tag, root in (("avant", args.a), ("apres", args.b)):
            st = Stage(root)
            device = setup_cycles(args)
            fn(st, view)
            p = os.path.join(rd, "%s_%s.png" % (name, tag))
            bpy.context.scene.render.filepath = p
            t0 = time.time()
            bpy.ops.render.render(write_still=True)
            times["%s_%s" % (name, tag)] = round(time.time() - t0, 1)
            paths.append(p)
        side_by_side(paths[0], paths[1], os.path.join(rd, "%s_avant_apres.png" % name))
    return times, device


# ----------------------------------------------------------------------------------------------- principal
def write_md(path, rows, problems, joints_b, joints_a, same, args, times, device):
    L = ["# Comparaison du kit du Level 0 (FBX relus)", "", "- avant (a) : `%s`" % args.a, "- apres (b) : `%s`" % args.b, ""]
    L.append("| Module | Tri. avant | Tri. apres | Ecart | Coins lisses (apres) | Pivot | Boite | Mat. | UV | UCX | Plans de la boite modifies |")
    L.append("|---|---:|---:|---:|---:|---|---|---|---|---|---|")
    for r in rows:
        pc = ", ".join("%s (enveloppe %.6f -> %.6f m2, faces %.6f -> %.6f m2)" % (k, v["hull_a"], v["hull_b"], v["faces_a"], v["faces_b"])
                       for k, v in sorted(r["planes_changed"].items()))
        L.append("| %s | %s | %s | %+d | %s | %s | %s | %s | %s | %d %s | %s |" % (
            r["module"], r["tris_a"], r["tris_b"], (r["tris_b"] or 0) - (r["tris_a"] or 0), r["bent_b"], "=" if r["pivot_same"] else "DIFF",
            "=" if r["box_same"] else "DIFF", "=" if r["materials_same"] else "DIFF", "=" if r["uv_same"] else "DIFF", r["ucx"],
            "=" if r["ucx_same"] else "DIFF", pc or "-"))
    L += ["", "Problemes : %s" % ("aucun" if not problems else "; ".join(problems)), ""]
    L += ["## Chanfreins (apres) : normales et UV", "", "| Module | Faces lisses | Normales perso. | UV continues | Densite UV min / max (1 = faces planes) |",
          "|---|---:|---|---:|---|"]
    for r in rows:
        u = r.get("uv_bevels_b")
        if u:
            L.append("| %s | %d | %s | %d / %d | %.4f / %.4f |" % (r["module"], u["faces"], "oui" if r["custom_b"] else "non", u["uv_continuous"], u["faces"],
                                                               u["density_min"], u["density_max"]))
    L.append("")
    L += ["## Joints des plinthes (points de section, |y| et z en m)", ""]
    for k in sorted(joints_b):
        L.append("- %s : apres %s ; avant %s" % (k, joints_b[k], joints_a.get(k)))
    if same is not None:
        L += ["", "## Geometrie identique a la reference (--same)", "", "- modules differents : %s" % (", ".join(same) if same else "aucun")]
    if times:
        L += ["", "## Rendus (%s)" % device, ""] + ["- %s : %.1f s" % (k, v) for k, v in sorted(times.items())]
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(L) + "\n")


def main():
    args = parse_args()
    os.makedirs(args.out, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    data = {}
    for tag, root in (("a", args.a), ("b", args.b)) + ((("same", args.same),) if args.same else ()):
        data[tag] = {n: read_fbx(p, n) for n, p in fbx_files(root).items()}
    rows, problems = compare(data["a"], data["b"])
    same = None
    if args.same:
        same = sorted(n for n in data["a"] if data["same"].get(n, {}).get("signature") != data["a"][n].get("signature")
                      or data["same"].get(n, {}).get("ucx") != data["a"][n].get("ucx"))
    joints_b = joint_checks(args.b)
    joints_a = joint_checks(args.a)
    times, device = ({}, None)
    if args.render:
        times, device = render_all(args, args.out)
    res = {"a": args.a, "b": args.b, "rows": rows, "problems": problems, "joints_b": joints_b, "joints_a": joints_a, "same_diff": same,
           "render_seconds": times, "render_device": device}
    with open(os.path.join(args.out, "compare_level0_kit.json"), "w", encoding="utf-8") as fh:
        json.dump(res, fh, ensure_ascii=False, indent=1)
    write_md(os.path.join(args.out, "compare_level0_kit.md"), rows, problems, joints_b, joints_a, same, args, times, device)
    print("Comparaison : %d modules, %d probleme(s)%s" % (len(rows), len(problems), "" if same is None else ", --same : %d module(s) differents" % len(same)))


if __name__ == "__main__":
    main()
