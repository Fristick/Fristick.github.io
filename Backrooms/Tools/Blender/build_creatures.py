"""
v4.5 : entites reconstruites (pas de modele source) : Wretch et Clump, en maillages a squelette.

Wretch : humanoide degenere, decharne, voute (Backrooms Wiki : un ancien humain ronge par la "Deterioration").
    Silhouette d'un seul tenant (graphe d'os habille par le modificateur Skin, puis remaillage voxel) : cotes, vertebres,
    omoplates, cretes iliaques, genoux et coudes noueux sous la peau ; crane allonge, orbites creuses, pommettes
    saillantes, joues creuses, machoire pendante ouverte sur des dents jaunies. Version detaillee (~400 000 sommets)
    cuite en normal map et en couleur (2048 px) sur une version de jeu decimee, skinnee sur un squelette de 31 os.
Clump : une masse de chair faite de 20 bras humains complets (epaule, coude, poignet, main a cinq doigts) fondus dans la
    masse, autour d'une bouche de lamproie a trois couronnes de dents. Chaque bras a ses os (bras, avant-bras, main) :
    le jeu les anime un par un (ils agrippent le sol et tirent la masse).

Usage : python Tools/Blender/build_creatures.py [-- wretch clump] [--preview]
Sorties : RawAssets/Skeletal/SK_Wretch.fbx, SK_Clump.fbx ; RawAssets/Textures/T_Wretch.jpg, T_Wretch_N.png,
          T_Clump.jpg, T_Clump_N.png ; RawAssets/Skeletal/skeletal_models.json (os principaux, cm, repere Unreal)
Ces modeles sont des creations du projet (non proteges) : on peut les regenerer librement.
"""
import json
import math
import os
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Matrix, Vector, kdtree

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT_SK = os.path.join(ROOT, "RawAssets", "Skeletal")
OUT_TEX = os.path.join(ROOT, "RawAssets", "Textures")
OUT_PREV = os.path.join(ROOT, "RawAssets", "Previews")
REPORT = {}


# ---------------------------------------------------------------------------
# Outils
# ---------------------------------------------------------------------------
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def link(o):
    bpy.context.scene.collection.objects.link(o)
    return o


def select_only(objs, active=None):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = active or objs[0]


def apply_all(o):
    select_only([o])
    for m in list(o.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


def skin_graph(name, nodes, edges, root=0, subsurf=2):
    """nodes : [(position, rayon)], edges : [(i, j)]. Tube organique (modificateur Skin) puis subdivision."""
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(p) for p, _ in nodes], edges, [])
    o = link(bpy.data.objects.new(name, me))
    md = o.modifiers.new("Skin", "SKIN")
    md.use_smooth_shade = True
    md.branch_smoothing = 0.6
    for i, (_, r) in enumerate(nodes):
        rx, ry = (r, r) if not isinstance(r, tuple) else r
        me.skin_vertices[0].data[i].radius = (rx, ry)
    # une racine par ilot (sinon le modificateur Skin ignore l'ilot)
    parent = list(range(len(nodes)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for a, b in edges:
        parent[find(a)] = find(b)
    seen = set()
    for i in [root] + list(range(len(nodes))):
        r = find(i)
        if r not in seen:
            seen.add(r)
            me.skin_vertices[0].data[i].use_root = True
    if subsurf:
        s = o.modifiers.new("Sub", "SUBSURF")
        s.levels = subsurf
    apply_all(o)
    return o


def chain(nodes, edges, pts, radii, start=None):
    """Ajoute une suite de noeuds relies (start : indice du noeud auquel la rattacher). Retourne les indices."""
    idx = []
    for p, r in zip(pts, radii):
        nodes.append((Vector(p), r))
        idx.append(len(nodes) - 1)
    prev = start
    for i in idx:
        if prev is not None:
            edges.append((prev, i))
        prev = i
    return idx


def lerp_pts(a, b, n):
    a, b = Vector(a), Vector(b)
    return [a.lerp(b, (k + 1) / n) for k in range(n)]


def voxel_remesh(o, size):
    select_only([o])
    md = o.modifiers.new("Remesh", "REMESH")
    md.mode = "VOXEL"
    md.voxel_size = size
    md.use_smooth_shade = True
    apply_all(o)


def smooth(o, factor=0.5, repeat=4):
    md = o.modifiers.new("Smooth", "SMOOTH")
    md.factor = factor
    md.iterations = repeat
    apply_all(o)


def join(objs, name):
    select_only(objs)
    bpy.ops.object.join()
    objs[0].name = name
    return objs[0]


def noise3(p, freq, seed=0):
    """Bruit de valeur lisse (numpy), p : (N, 3)"""
    rng = np.random.default_rng(seed)
    perm = rng.random((32, 32, 32))
    q = p * freq
    i = np.floor(q).astype(int)
    f = q - i
    f = f * f * (3 - 2 * f)
    out = np.zeros(len(p))
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (f[:, 0] if dx else 1 - f[:, 0]) * (f[:, 1] if dy else 1 - f[:, 1]) * (f[:, 2] if dz else 1 - f[:, 2])
                out += w * perm[(i[:, 0] + dx) % 32, (i[:, 1] + dy) % 32, (i[:, 2] + dz) % 32]
    return out


def fbm(p, freq, octaves=4, seed=0):
    out = np.zeros(len(p))
    amp = 1.0
    tot = 0.0
    for k in range(octaves):
        out += amp * noise3(p, freq * (2 ** k), seed + k)
        tot += amp
        amp *= 0.5
    return out / tot


def coords(o):
    co = np.empty(len(o.data.vertices) * 3)
    o.data.vertices.foreach_get("co", co)
    return co.reshape(-1, 3)


def normals(o):
    n = np.empty(len(o.data.vertices) * 3)
    o.data.vertices.foreach_get("normal", n)
    return n.reshape(-1, 3)


def set_coords(o, co):
    o.data.vertices.foreach_set("co", co.reshape(-1))
    o.data.update()


def decimate_to(o, faces):
    cur = len(o.data.polygons)
    if cur <= faces:
        return
    md = o.modifiers.new("Dec", "DECIMATE")
    md.ratio = faces / cur
    md.use_collapse_triangulate = True
    apply_all(o)


def smart_uv(o, margin=0.004):
    select_only([o])
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=margin, area_weight=0.6)
    bpy.ops.object.mode_set(mode="OBJECT")


def mat(name, color=(0.8, 0.8, 0.8)):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1.0)
    return m


def vertex_color_material(name):
    """Materiau de cuisson : couleur des sommets x grain de peau procedural"""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    attr = nt.nodes.new("ShaderNodeVertexColor")
    attr.layer_name = "Col"
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 180.0
    noise.inputs["Detail"].default_value = 6.0
    ramp = nt.nodes.new("ShaderNodeMapRange")
    ramp.inputs["To Min"].default_value = 0.86
    ramp.inputs["To Max"].default_value = 1.08
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = 1.0
    nt.links.new(noise.outputs["Fac"], ramp.inputs["Value"])
    nt.links.new(attr.outputs["Color"], mix.inputs[6])
    nt.links.new(ramp.outputs["Result"], mix.inputs[7])
    nt.links.new(mix.outputs[2], b.inputs["Base Color"])
    return m


def bake(high, low, size, name, ao_strength=0.5):
    """Cuisson high -> low : normal map (convention DirectX pour Unreal) et couleur (x occlusion ambiante)"""
    from PIL import Image
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 8
    low.data.materials.clear()
    bm = bpy.data.materials.new("bake_" + name)
    bm.use_nodes = True
    low.data.materials.append(bm)
    nt = bm.node_tree
    out = {}
    for kind in ("NORMAL", "DIFFUSE", "AO"):
        # images flottantes : valeurs lineaires (une image 8 bits sRGB recevrait une couleur deja convertie)
        img = bpy.data.images.new("%s_%s" % (name, kind), size, size, alpha=False, float_buffer=True)
        if kind == "NORMAL":
            img.colorspace_settings.name = "Non-Color"
        node = nt.nodes.new("ShaderNodeTexImage")
        node.image = img
        for n in nt.nodes:
            n.select = False
        node.select = True
        nt.nodes.active = node
        select_only([high, low], low)
        kw = dict(type=kind, use_selected_to_active=True, cage_extrusion=0.012, max_ray_distance=0.03, margin=8)
        if kind == "DIFFUSE":
            kw["pass_filter"] = {"COLOR"}
        if kind == "AO":
            sc.cycles.samples = 64
        bpy.ops.object.bake(**kw)
        sc.cycles.samples = 8
        px = np.empty(size * size * 4, dtype=np.float32)
        img.pixels.foreach_get(px)
        out[kind] = px.reshape(size, size, 4)[::-1, :, :3]
    n = out["NORMAL"].copy()
    n[..., 1] = 1.0 - n[..., 1]  # vert inverse : Unreal attend des normal maps DirectX
    Image.fromarray((np.clip(n, 0, 1) * 255 + 0.5).astype(np.uint8)).save(os.path.join(OUT_TEX, name + "_N.png"), optimize=True)
    ao = out["AO"][..., :1]
    col = out["DIFFUSE"] * ((1.0 - ao_strength) + ao_strength * ao)
    # sRGB : la cuisson est lineaire
    srgb = np.where(col <= 0.0031308, col * 12.92, 1.055 * np.power(np.clip(col, 0, 1), 1 / 2.4) - 0.055)
    Image.fromarray((np.clip(srgb, 0, 1) * 255 + 0.5).astype(np.uint8)).save(os.path.join(OUT_TEX, name + ".jpg"), quality=94,
                                                                               subsampling=0)
    print("  textures", name, size)


def heat_weights(arm, body):
    """Poids automatiques (diffusion de chaleur de Blender) ; repli : poids par distance aux os"""
    select_only([body, arm], arm)
    try:
        bpy.ops.object.parent_set(type="ARMATURE_AUTO")
        verts = body.data.vertices
        step = max(1, len(verts) // 500)
        ok = all(any(g.weight > 0 for g in verts[i].groups) for i in range(0, len(verts), step))
    except Exception as e:
        print("  poids automatiques impossibles :", e)
        ok = False
    if not ok:
        print("  poids par distance aux os (repli)")
        distance_weights(arm, body)
    return body


def distance_weights(arm, body, deform=None):
    """Poids par distance aux segments d'os ; un sommet ne se partage qu'entre deux os voisins dans la hierarchie
    (parent / enfant / freres) : un bras qui pend contre la cuisse ne suit jamais la jambe"""
    for g in list(body.vertex_groups):
        body.vertex_groups.remove(g)
    for m in [m for m in body.modifiers if m.type == "ARMATURE"]:
        body.modifiers.remove(m)
    bones = [b for b in arm.data.bones if b.use_deform and b.name != "Root" and (deform is None or b.name in deform)]
    names = [b.name for b in bones]
    segs = [(np.array(b.head_local), np.array(b.tail_local)) for b in bones]
    adj = np.zeros((len(bones), len(bones)), dtype=bool)
    for i, b in enumerate(bones):
        for j, c in enumerate(bones):
            adj[i, j] = (b.parent == c or c.parent == b or (b.parent is not None and b.parent == c.parent))
    co = coords(body)
    D = []
    for h, t in segs:
        ab = t - h
        tt = np.clip(((co - h) @ ab) / max(ab @ ab, 1e-9), 0, 1)
        D.append(np.linalg.norm(co - (h + tt[:, None] * ab), axis=1))
    D = np.stack(D, 1)
    near = np.argmin(D, 1)
    groups = {n: body.vertex_groups.new(name=n) for n in names}
    for vi in range(len(co)):
        a = near[vi]
        cand = np.where(adj[a])[0]
        da = D[vi, a] + 1e-4
        if len(cand) == 0:
            groups[names[a]].add([vi], 1.0, "REPLACE")
            continue
        b = cand[np.argmin(D[vi, cand])]
        db = D[vi, b] + 1e-4
        wa = (1 / da ** 4) / (1 / da ** 4 + 1 / db ** 4)
        groups[names[a]].add([vi], float(wa), "REPLACE")
        if 1 - wa > 0.02:
            groups[names[b]].add([vi], float(1 - wa), "REPLACE")
    md = body.modifiers.new("Armature", "ARMATURE")
    md.object = arm
    body.parent = arm


def make_armature(name, bones):
    """bones : [(nom, tete, queue, parent)] en metres"""
    ad = bpy.data.armatures.new(name)
    arm = link(bpy.data.objects.new(name, ad))
    select_only([arm])
    bpy.ops.object.mode_set(mode="EDIT")
    for n, h, t, p in bones:
        eb = ad.edit_bones.new(n)
        eb.head = Vector(h)
        eb.tail = Vector(t)
        if p:
            eb.parent = ad.edit_bones[p]
            eb.use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    return arm


def export_skeletal(arm, body, name):
    """Meme convention que les autres SK_ : fichier en centimetres, face vers +X, Y inverse a l'import"""
    G = Matrix.Scale(100.0, 4)
    for o in (arm, body):
        o.matrix_world = G @ o.matrix_world
    select_only([arm, body], arm)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    sc = bpy.context.scene
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 0.01
    arm.name = "Armature"
    arm.data.name = "Armature"
    os.makedirs(OUT_SK, exist_ok=True)
    path = os.path.join(OUT_SK, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, use_mesh_modifiers=False,
                             mesh_smooth_type="FACE", add_leaf_bones=False, use_armature_deform_only=False, bake_anim=False,
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", axis_forward="-Z", axis_up="Y",
                             path_mode="STRIP")
    tris = sum(len(p.vertices) - 2 for p in body.data.polygons)
    print("  export", path, len(body.data.vertices), "sommets,", tris, "triangles,", len(arm.data.bones), "os")
    bones = {b.name: [round(b.head_local.x, 2), round(-b.head_local.y, 2), round(b.head_local.z, 2)] for b in arm.data.bones
             if not any(k in b.name for k in ("Finger", "Thumb", "Root"))}
    REPORT[name] = {"Vertices": len(body.data.vertices), "Triangles": tris, "BoneCount": len(arm.data.bones), "Bones": bones,
                    "Materials": [m.name for m in body.data.materials]}
    # retour en metres pour l'apercu
    Gi = Matrix.Scale(0.01, 4)
    for o in (arm, body):
        o.matrix_world = Gi @ o.matrix_world
    sc.unit_settings.scale_length = 1.0
    return path


# ---------------------------------------------------------------------------
# Wretch
# ---------------------------------------------------------------------------
# Pose de liaison (metres, face vers +X, gauche = +Y) : voute, tete basse en avant, genoux flechis, bras pendants
W_PELVIS = (0.0, 0.0, 0.84)
W_WAIST = (0.03, 0.0, 0.95)
W_CHEST = (0.10, 0.0, 1.06)
W_UPPER = (0.17, 0.0, 1.13)
W_NECK = (0.24, 0.0, 1.17)
W_HEAD = (0.33, 0.0, 1.19)


def w_side(s):
    """Points d'un cote (s = +1 gauche, -1 droite)"""
    return {
        "clav": (0.19, s * 0.05, 1.16), "shoulder": (0.17, s * 0.165, 1.14), "elbow": (0.18, s * 0.205, 0.84),
        "wrist": (0.23, s * 0.195, 0.56), "palm": (0.25, s * 0.195, 0.475), "hip": (0.0, s * 0.085, 0.82),
        "knee": (0.08, s * 0.10, 0.47), "ankle": (0.0, s * 0.105, 0.075), "toe": (0.15, s * 0.115, 0.02), "heel": (-0.045, s * 0.105, 0.03),
    }


def wretch_body():
    nodes, edges = [], []
    spine = chain(nodes, edges, [W_PELVIS, (0.015, 0, 0.9), W_WAIST, (0.065, 0, 1.01), W_CHEST, (0.14, 0, 1.10), W_UPPER, W_NECK],
                  [(0.105, 0.095), 0.085, 0.072, 0.085, 0.112, 0.118, 0.1, 0.038])
    for s in (1, -1):
        p = w_side(s)
        clav = chain(nodes, edges, [p["clav"], p["shoulder"]], [0.03, 0.044], start=spine[6])
        arm = chain(nodes, edges, lerp_pts(p["shoulder"], p["elbow"], 3), [0.034, 0.029, 0.031], start=clav[-1])
        fore = chain(nodes, edges, lerp_pts(p["elbow"], p["wrist"], 3), [0.027, 0.023, 0.017], start=arm[-1])
        palm = chain(nodes, edges, [p["palm"]], [0.021], start=fore[-1])
        # cinq doigts longs et osseux (trois phalanges)
        base = Vector(p["palm"])
        for k, (dy, length, dx) in enumerate(((-0.022, 0.085, 0.004), (-0.007, 0.095, 0.006), (0.008, 0.092, 0.005), (0.021, 0.078, 0.002))):
            knuckle = base + Vector((0.004 + dx, s * dy, -0.008))
            tip = knuckle + Vector((0.018, s * dy * 0.25, -length))
            mid = knuckle.lerp(tip, 0.5) + Vector((0.012, 0, 0))  # doigts legerement recourbes
            chain(nodes, edges, [knuckle, mid, tip], [0.0075, 0.0062, 0.0045], start=palm[0])
        th0 = Vector(p["wrist"]) + Vector((0.02, s * -0.018, -0.04))
        chain(nodes, edges, [th0, th0 + Vector((0.035, s * -0.012, -0.03)), th0 + Vector((0.06, s * -0.01, -0.055))], [0.009, 0.007, 0.005],
              start=fore[-1])
        leg = chain(nodes, edges, [p["hip"]] + lerp_pts(p["hip"], p["knee"], 3) + lerp_pts(p["knee"], p["ankle"], 3),
                    [0.062, 0.052, 0.044, 0.043, 0.036, 0.031, 0.022], start=spine[0])
        chain(nodes, edges, [p["heel"]], [0.022], start=leg[-1])
        foot = chain(nodes, edges, [Vector(p["ankle"]).lerp(Vector(p["toe"]), 0.55) + Vector((0, 0, -0.03)), p["toe"]], [0.026, 0.02],
                     start=leg[-1])
    body = skin_graph("WretchBody", nodes, edges, root=0, subsurf=2)
    return body


def wretch_head():
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=6, radius=1.0)
    co = np.array([tuple(v.co) for v in bm.verts])
    n = co / np.linalg.norm(co, axis=1)[:, None]
    x, y, z = n[:, 0], n[:, 1], n[:, 2]

    def bump(cx, cy, cz, sigma, amt):
        c = np.array([cx, cy, cz])
        c = c / np.linalg.norm(c)
        d2 = np.sum((n - c) ** 2, 1)
        return amt * np.exp(-d2 / (sigma * sigma))

    r = np.ones(len(n))
    for sy in (1, -1):
        r -= bump(0.8, sy * 0.38, 0.14, 0.2, 0.3)     # orbites profondes
        r += bump(0.74, sy * 0.38, 0.4, 0.13, 0.07)   # arcade sourciliere saillante
        r += bump(0.58, sy * 0.7, -0.08, 0.15, 0.09)  # pommettes
        r -= bump(0.6, sy * 0.6, -0.4, 0.2, 0.14)     # joues creusees
        r -= bump(0.05, sy * 1.0, 0.25, 0.28, 0.1)    # tempes
        r += bump(0.25, sy * 0.85, -0.55, 0.18, 0.07)  # angle de la machoire
    r -= bump(0.97, 0.0, -0.1, 0.09, 0.2)             # cavite nasale (pas de nez)
    r += bump(-0.55, 0.0, 0.75, 0.5, 0.09)            # occiput allonge
    co = n * r[:, None]
    co[:, 0] *= 0.098
    co[:, 1] *= 0.072
    co[:, 2] *= 0.108
    # machoire pendante, bouche ouverte (cavite sombre et profonde)
    front = np.clip((x - 0.2) / 0.5, 0, 1)
    lowz = np.clip((-z - 0.15) / 0.35, 0, 1)
    co[:, 2] -= 0.06 * front * lowz
    co[:, 0] += 0.012 * front * lowz
    mouth = np.exp(-(((z + 0.4) / 0.17) ** 2) - ((y / 0.4) ** 2)) * np.clip((x - 0.45) / 0.35, 0, 1)
    co[:, 0] -= 0.085 * mouth
    for v, c in zip(bm.verts, co):
        v.co = Vector(c)
    me = bpy.data.meshes.new("WretchHead")
    bm.to_mesh(me)
    bm.free()
    o = link(bpy.data.objects.new("WretchHead", me))
    o.location = Vector(W_HEAD) + Vector((0, 0, -0.01))
    o.rotation_euler = (0, math.radians(14), 0)  # tete penchee en avant
    select_only([o])
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return o


def wretch_bones_under_skin():
    """Omoplates saillantes : petits volumes fondus sous la peau par le remaillage"""
    bm = bmesh.new()
    for s in (1, -1):
        ret = bmesh.ops.create_icosphere(bm, subdivisions=3, radius=1.0)
        for v in ret["verts"]:
            v.co = Vector((v.co.x * 0.016, v.co.y * 0.048, v.co.z * 0.062)) + Vector((0.045, s * 0.08, 1.115))
    me = bpy.data.meshes.new("Knobs")
    bm.to_mesh(me)
    bm.free()
    return [link(bpy.data.objects.new("Knobs", me))]


def seg_param(co, a, b):
    a, b = np.array(a), np.array(b)
    ab = b - a
    t = np.clip(((co - a) @ ab) / (ab @ ab), 0, 1)
    p = a + t[:, None] * ab
    return t, co - p, np.linalg.norm(ab)


def wretch_relief(o):
    """Os sous la peau, en deplacement le long des normales : cotes qui descendent vers le sternum, espaces
    intercostaux creuses, ventre rentre, vertebres, clavicules"""
    co = coords(o)
    nrm = normals(o)
    disp = np.zeros(len(co))
    # thorax : axe du bas des cotes au haut de la poitrine
    t, rel, L = seg_param(co, (0.055, 0, 0.99), W_UPPER)
    axis = np.array(W_UPPER) - np.array((0.055, 0, 0.99))
    axis /= np.linalg.norm(axis)
    back = np.cross(axis, np.array([0.0, 1.0, 0.0]))
    back /= np.linalg.norm(back)  # vers l'arriere (-X environ)
    if back[0] > 0:
        back = -back
    side = np.cross(axis, back)
    d = np.linalg.norm(rel, axis=1)
    theta = np.arctan2(rel @ side, rel @ back)  # 0 = dos, +-pi = sternum
    torso = np.clip((0.14 - d) / 0.02, 0, 1) * (np.abs(co[:, 1]) < 0.135)
    front = 0.5 - 0.5 * np.cos(theta)  # 0 dos, 1 devant
    phase = (t * L + 0.05 * front) / 0.026
    ridge = (0.5 + 0.5 * np.cos(2 * np.pi * phase)) ** 6
    ribzone = torso * np.clip((t - 0.05) / 0.1, 0, 1) * np.clip((0.97 - t) / 0.1, 0, 1)
    sternum = np.exp(-((np.abs(theta) - np.pi) / 0.22) ** 2)
    spinez = np.exp(-(theta / 0.3) ** 2)
    disp += ribzone * (1 - 0.8 * sternum) * (1 - 0.7 * spinez) * (ridge * 0.0055 - 0.0018)
    # ventre creuse sous les cotes
    tb, relb, _ = seg_param(co, W_PELVIS, (0.055, 0, 0.99))
    db = np.linalg.norm(relb, axis=1)
    belly = np.clip((0.12 - db) / 0.02, 0, 1) * np.clip((co[:, 0] - 0.0) / 0.06, 0, 1) * np.clip((tb - 0.2) / 0.3, 0, 1)
    disp -= belly * 0.012
    # vertebres le long du dos (bassin -> nuque)
    for a, b in ((W_PELVIS, W_WAIST), (W_WAIST, W_CHEST), (W_CHEST, W_UPPER), (W_UPPER, W_NECK)):
        ts, rs, Ls = seg_param(co, a, b)
        ds = np.linalg.norm(rs, axis=1)
        ax = (np.array(b) - np.array(a)) / Ls
        bk = np.cross(ax, np.array([0.0, 1.0, 0.0]))
        bk /= np.linalg.norm(bk)
        if bk[0] > 0:
            bk = -bk
        cosb = (rs @ bk) / np.maximum(ds, 1e-6)
        lateral = np.abs(rs[:, 1]) / np.maximum(ds, 1e-6)
        mask = (cosb > 0.6) * np.exp(-(lateral / 0.22) ** 2) * (ds < 0.15) * (ts > 0.0) * (ts < 1.0)
        knob = (0.5 + 0.5 * np.cos(2 * np.pi * ts * Ls / 0.027)) ** 4
        disp += mask * (knob * 0.007 + 0.002)
    # clavicules
    for s in (1, -1):
        tc, rc, _ = seg_param(co, (0.2, s * 0.02, 1.155), (0.185, s * 0.16, 1.15))
        dc = np.linalg.norm(rc, axis=1)
        disp += np.exp(-((dc - 0.035) / 0.012) ** 2) * (co[:, 0] > 0.17) * 0.004
    set_coords(o, co + nrm * disp[:, None])


def wretch_colors(o):
    """Couleur des sommets : peau grise et verdatre, marbrures, veines sombres, crasse dans les creux, bouche et orbites"""
    co = coords(o)
    nrm = normals(o)
    x, y, z = co[:, 0], co[:, 1], co[:, 2]
    base = np.array([0.36, 0.38, 0.30])
    mott = fbm(co, 9.0, 4, 3)[:, None]
    col = base * (0.75 + 0.5 * mott)
    # marbrures violacees et jaunatres
    blot = np.clip((fbm(co, 4.0, 3, 8) - 0.55) / 0.15, 0, 1)[:, None]
    col = col * (1 - 0.35 * blot) + np.array([0.30, 0.22, 0.26]) * 0.35 * blot
    # veines : creux d'un bruit (lignes fines)
    vein = np.abs(fbm(co, 22.0, 3, 11) - 0.5)
    vmask = np.clip(1 - vein / 0.035, 0, 1)[:, None] * np.clip((fbm(co, 3.0, 2, 13) - 0.4) / 0.2, 0, 1)[:, None]
    col = col * (1 - 0.45 * vmask) + np.array([0.12, 0.13, 0.2]) * 0.45 * vmask
    # crasse vers le bas (pieds, genoux) et sur les mains
    dirt = np.clip((0.35 - z) / 0.3, 0, 1)[:, None]
    col = col * (1 - 0.5 * dirt) + np.array([0.12, 0.1, 0.07]) * 0.5 * dirt
    # tete : orbites noires, interieur de la bouche rouge sombre, levres plus sombres
    hc = Vector(W_HEAD) + Vector((0, 0, -0.01))
    hd = co - np.array(hc)
    head = np.linalg.norm(hd, axis=1) < 0.15
    rot = Matrix.Rotation(math.radians(-14), 3, "Y")
    loc = hd @ np.array(rot).T
    for sy in (1, -1):
        eye = np.exp(-(((loc[:, 0] - 0.07) / 0.03) ** 2) - (((loc[:, 1] - sy * 0.027) / 0.024) ** 2) - (((loc[:, 2] - 0.015) / 0.024) ** 2))
        col = col * (1 - 0.95 * eye[:, None] * head[:, None])
    mouthc = np.exp(-(((loc[:, 2] + 0.06) / 0.04) ** 2) - ((loc[:, 1] / 0.036) ** 2)) * np.clip((0.1 - loc[:, 0]) / 0.04, 0, 1) * head
    col = col * (1 - 0.9 * mouthc[:, None]) + np.array([0.09, 0.02, 0.02]) * 0.9 * mouthc[:, None]
    col = np.clip(col, 0, 1)
    if "Col" not in o.data.color_attributes:
        o.data.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
    a = o.data.color_attributes["Col"]
    rgba = np.concatenate([col, np.ones((len(col), 1))], 1)
    a.data.foreach_set("color", rgba.reshape(-1))


def wretch_teeth():
    """Dents jaunies, irregulieres, en deux rangees dans la bouche ouverte (hors cuisson : materiau a part)"""
    bm = bmesh.new()
    rng = np.random.default_rng(4)
    rot = Matrix.Rotation(math.radians(14), 4, "Y")
    hc = Vector(W_HEAD) + Vector((0, 0, -0.01))
    for row, (z0, down) in enumerate(((-0.032, -1), (-0.082, 1))):
        n = 11
        for k in range(n):
            t = (k + 0.5) / n
            ang = (t - 0.5) * math.radians(140)
            base = Vector((0.062 * math.cos(ang) + 0.012 + row * 0.006, 0.036 * math.sin(ang), z0))
            ln = 0.012 + rng.uniform(0, 0.01)
            if rng.random() < 0.15:
                continue  # dent manquante
            ret = bmesh.ops.create_cone(bm, cap_ends=True, segments=5, radius1=0.0045, radius2=0.0012, depth=ln)
            tilt = Matrix.Rotation(rng.uniform(-0.25, 0.25), 4, "X") @ Matrix.Rotation(rng.uniform(-0.2, 0.2), 4, "Y")
            flip = Matrix.Rotation(math.pi if down < 0 else 0.0, 4, "X")
            M = Matrix.Translation(hc) @ rot @ Matrix.Translation(base + Vector((0, 0, down * -ln * 0.5))) @ tilt @ flip
            for v in ret["verts"]:
                v.co = M @ v.co
    me = bpy.data.meshes.new("Teeth")
    bm.to_mesh(me)
    bm.free()
    return link(bpy.data.objects.new("WretchTeeth", me))


def wretch_eyes():
    """Yeux laiteux tout au fond des orbites"""
    bm = bmesh.new()
    rot = Matrix.Rotation(math.radians(14), 4, "Y")
    hc = Vector(W_HEAD) + Vector((0, 0, -0.01))
    for sy in (1, -1):
        ret = bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=0.0115)
        c = hc + rot @ Vector((0.058, sy * 0.026, 0.014))
        for v in ret["verts"]:
            v.co = v.co + c
    me = bpy.data.meshes.new("Eyes")
    bm.to_mesh(me)
    bm.free()
    return link(bpy.data.objects.new("WretchEyes", me))


WRETCH_BONES = None


def wretch_armature():
    L, R = w_side(1), w_side(-1)
    b = [("Root", (0, 0, 0), (0, 0, 0.1), None),
         ("Hips", W_PELVIS, (0.015, 0, 0.9), "Root"),
         ("Spine", W_WAIST, W_CHEST, "Hips"),
         ("Chest", W_CHEST, W_UPPER, "Spine"),
         ("Neck", W_UPPER, W_NECK, "Chest"),
         ("Head", W_NECK, (0.36, 0, 1.24), "Neck")]
    for side, p in (("Left", L), ("Right", R)):
        s = 1 if side == "Left" else -1
        b += [(side + "Shoulder", p["clav"], p["shoulder"], "Chest"),
              (side + "Arm", p["shoulder"], p["elbow"], side + "Shoulder"),
              (side + "ForeArm", p["elbow"], p["wrist"], side + "Arm"),
              (side + "Hand", p["wrist"], p["palm"], side + "ForeArm"),
              (side + "Thumb", tuple(Vector(p["wrist"]) + Vector((0.02, s * -0.018, -0.04))),
               tuple(Vector(p["wrist"]) + Vector((0.08, s * -0.03, -0.095))), side + "Hand")]
        for k, dy in enumerate((-0.022, -0.007, 0.008, 0.021)):
            kn = Vector(p["palm"]) + Vector((0.006, s * dy, -0.008))
            b.append((side + "Finger%d" % (k + 1), tuple(kn), tuple(kn + Vector((0.03, 0, -0.088))), side + "Hand"))
        b += [(side + "UpLeg", p["hip"], p["knee"], "Hips"),
              (side + "Leg", p["knee"], p["ankle"], side + "UpLeg"),
              (side + "Foot", p["ankle"], p["toe"], side + "Leg")]
    return make_armature("WretchRig", b)


def build_wretch(preview=False):
    print("== Wretch")
    reset()
    body = wretch_body()
    head = wretch_head()
    extra = wretch_bones_under_skin()
    high = join([body, head] + extra, "WretchHigh")
    voxel_remesh(high, 0.0035)
    smooth(high, 0.5, 3)
    wretch_relief(high)
    smooth(high, 0.3, 1)
    # rides et plis de peau tendue sur les os (deplacement le long des normales)
    co = coords(high)
    nrm = normals(high)
    wr = (fbm(co * np.array([1.0, 1.0, 2.2]), 60.0, 3, 21) - 0.5) * 0.0016 + (fbm(co, 14.0, 3, 5) - 0.5) * 0.004
    set_coords(high, co + nrm * wr[:, None])
    high.data.update()
    wretch_colors(high)
    high.data.materials.clear()
    high.data.materials.append(vertex_color_material("WretchBake"))
    print("  detaille :", len(high.data.vertices), "sommets")

    low = high.copy()
    low.data = high.data.copy()
    low.name = "SK_Wretch_Mesh"
    link(low)
    decimate_to(low, 12000)
    smooth(low, 0.2, 1)
    smart_uv(low)
    bake(high, low, 2048, "T_Wretch", ao_strength=0.55)
    bpy.data.objects.remove(high)

    arm = wretch_armature()
    heat_weights(arm, low)
    teeth = wretch_teeth()
    eyes = wretch_eyes()
    for o in (teeth, eyes):
        g = o.vertex_groups.new(name="Head")
        g.add(list(range(len(o.data.vertices))), 1.0, "REPLACE")
    low.data.materials.clear()
    low.data.materials.append(mat("WretchSkin", (0.42, 0.45, 0.36)))
    teeth.data.materials.append(mat("WretchTeeth", (0.55, 0.5, 0.36)))
    eyes.data.materials.append(mat("WretchEye", (0.35, 0.36, 0.33)))
    for o in (teeth, eyes):
        select_only([o])
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project()
        bpy.ops.object.mode_set(mode="OBJECT")
    low = join([low, teeth, eyes], "SK_Wretch_Mesh")
    for p in low.data.polygons:
        p.use_smooth = True
    REPORT_KEYS = ("Spine", "Head", "LeftArm", "LeftForeArm", "RightArm", "RightForeArm", "LeftUpLeg", "LeftLeg", "RightUpLeg", "RightLeg")
    export_skeletal(arm, low, "SK_Wretch")
    print("  os principaux :", {k: REPORT["SK_Wretch"]["Bones"][k] for k in REPORT_KEYS})
    if preview:
        # poursuite : bras droit tendu vers l'avant, foulee, tete relevee
        pose_preview(arm, low, "SK_Wretch", poses=(("RightArm", (0, 1, 0), -65), ("RightForeArm", (0, 1, 0), -20), ("LeftArm", (0, 1, 0), 25),
                                                  ("LeftUpLeg", (0, 1, 0), -30), ("LeftLeg", (0, 1, 0), 35), ("RightUpLeg", (0, 1, 0), 15),
                                                  ("Head", (0, 1, 0), -20)))
    return arm, low


# ---------------------------------------------------------------------------
# Apercu (rendu Blender, pas une capture du jeu)
# ---------------------------------------------------------------------------
def preview_materials(body):
    for m in body.data.materials:
        m.use_nodes = True
        nt = m.node_tree
        b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
        tex = {"WretchSkin": "T_Wretch", "ClumpSkin": "T_Clump"}.get(m.name)
        if tex:
            t = nt.nodes.new("ShaderNodeTexImage")
            t.image = bpy.data.images.load(os.path.join(OUT_TEX, tex + ".jpg"))
            nt.links.new(t.outputs["Color"], b.inputs["Base Color"])
            tn = nt.nodes.new("ShaderNodeTexImage")
            tn.image = bpy.data.images.load(os.path.join(OUT_TEX, tex + "_N.png"))
            tn.image.colorspace_settings.name = "Non-Color"
            # la normal map est en convention DirectX : on reinverse le vert pour Blender
            sep = nt.nodes.new("ShaderNodeSeparateColor")
            inv = nt.nodes.new("ShaderNodeMath")
            inv.operation = "SUBTRACT"
            inv.inputs[0].default_value = 1.0
            comb = nt.nodes.new("ShaderNodeCombineColor")
            nm = nt.nodes.new("ShaderNodeNormalMap")
            nt.links.new(tn.outputs["Color"], sep.inputs["Color"])
            nt.links.new(sep.outputs[0], comb.inputs[0])
            nt.links.new(sep.outputs[1], inv.inputs[1])
            nt.links.new(inv.outputs[0], comb.inputs[1])
            nt.links.new(sep.outputs[2], comb.inputs[2])
            nt.links.new(comb.outputs["Color"], nm.inputs["Color"])
            nt.links.new(nm.outputs["Normal"], b.inputs["Normal"])
            b.inputs["Subsurface Weight"].default_value = 0.12
        else:
            b.inputs["Base Color"].default_value = m.diffuse_color
        b.inputs["Roughness"].default_value = 0.5


def shot(target, dist, view, out, res=600, lens=50):
    sc = bpy.context.scene
    cam = link(bpy.data.objects.new("C", bpy.data.cameras.new("C")))
    cam.data.lens = lens
    cam.location = target + view.normalized() * dist
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc.camera = cam
    sc.render.resolution_x = sc.render.resolution_y = res
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)


def pose_preview(arm, body, name, poses=None, shots=None):
    """Planche de 3 vues (rendus Blender, pas des captures du jeu) : pose de liaison, pose de poursuite, gros plan"""
    sys.path.insert(0, HERE)
    import import_user_models as ium
    from PIL import Image
    preview_materials(body)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = 40
    w = bpy.data.worlds.new("W")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.22, 0.22, 0.23, 1)
    sc.world = w
    for e, rot in ((3.0, (55, 0, 35)), (0.9, (70, 0, -140))):
        l = link(bpy.data.objects.new("L", bpy.data.lights.new("L", "SUN")))
        l.data.energy = e
        l.rotation_euler = [math.radians(a) for a in rot]
    sc.render.image_settings.file_format = "PNG"
    tmp = os.path.join(OUT_PREV, "_tmp_%d.png")
    shots = shots or [(Vector((0.1, 0, 0.7)), 2.6, Vector((1.0, -0.6, 0.15))), None, (Vector((0.33, 0, 1.16)), 0.75, Vector((1.0, -0.45, 0.1)))]
    files = []
    for k, sh in enumerate(shots):
        if sh is None:
            for bone, axis, deg in (poses or ()):
                pb = arm.pose.bones.get(bone)
                if pb:
                    ium.rotate_pose_bone(arm, pb, Vector(axis), math.radians(deg))
            sh = shots[0]
        shot(sh[0], sh[1], sh[2], tmp % k)
        files.append(tmp % k)
    ims = [Image.open(f).convert("RGB") for f in files]
    sheet = Image.new("RGB", (sum(i.size[0] for i in ims), ims[0].size[1]))
    x = 0
    for im in ims:
        sheet.paste(im, (x, 0))
        x += im.size[0]
    out = os.path.join(OUT_PREV, name + "_Pose.jpg")
    sheet.save(out, quality=90)
    for f in files:
        os.remove(f)
    print("  apercu (Blender)", out)


# ---------------------------------------------------------------------------
# Clump
# ---------------------------------------------------------------------------
C_CENTER = Vector((0.0, 0.0, 0.52))
C_RADIUS = 0.36


def fib_dirs(n, seed=0):
    rng = np.random.default_rng(seed)
    out = []
    ga = math.pi * (3 - math.sqrt(5))
    for i in range(n):
        z = 1 - 2 * (i + 0.5) / n
        r = math.sqrt(1 - z * z)
        a = ga * i + rng.uniform(-0.2, 0.2)
        out.append(Vector((math.cos(a) * r, math.sin(a) * r, z)))
    return out


def clump_arm_specs():
    """20 bras : 6 d'appui (main a plat au sol, autour de la masse) et 14 qui se tordent (vers le haut, les cotes, l'arriere).
    Aucun bras devant la bouche (+X)."""
    rng = np.random.default_rng(31)
    specs = []
    # bras d'appui : epaule sur le flanc bas, coude haut et vers l'exterieur (comme une pompe), main a plat au sol
    for k, ang in enumerate((-150, -100, -40, 40, 100, 150)):
        a = math.radians(ang + rng.uniform(-10, 10))
        d = Vector((math.cos(a), math.sin(a), 0.0))
        root = C_CENTER + d * (C_RADIUS * 0.5) + Vector((0, 0, -0.08))
        shoulder = C_CENTER + d * (C_RADIUS * 0.92) + Vector((0, 0, -0.06))
        elbow = shoulder + d * 0.2 + Vector((0, 0, 0.12))
        wrist = Vector((elbow.x, elbow.y, 0.0)) + d * 0.1 + Vector((0, 0, 0.045))
        hand = wrist + d * 0.09 + Vector((0, 0, -0.03))
        specs.append(dict(root=root, shoulder=shoulder, elbow=elbow, wrist=wrist, hand=hand, ground=True, curl=0.1, flat=True))
    # bras qui se tordent : directions reparties sur le haut et les cotes, sauf devant
    dirs = [v for v in fib_dirs(44, 7) if v.z > -0.2 and not (v.x > 0.5 and abs(v.y) < 0.62 and v.z < 0.75)][:14]
    for k, d in enumerate(dirs):
        d = d.normalized()
        side = d.cross(Vector((0, 0, 1)))
        if side.length < 1e-3:
            side = Vector((0, 1, 0))
        side.normalize()
        bend = rng.uniform(0.6, 1.3)
        root = C_CENTER + d * (C_RADIUS * 0.55)
        shoulder = C_CENTER + d * (C_RADIUS * 0.95)
        elbow = shoulder + d * 0.27 + side * rng.uniform(-0.05, 0.05)
        wrist = elbow + (d * math.cos(bend) + Vector((0, 0, -1)) * math.sin(bend) * 0.8 + side * rng.uniform(-0.3, 0.3)).normalized() * 0.24
        hand = wrist + (wrist - elbow).normalized() * 0.09
        specs.append(dict(root=root, shoulder=shoulder, elbow=elbow, wrist=wrist, hand=hand, ground=False, curl=rng.uniform(0.3, 0.9), flat=False))
    return specs


def clump_arm_graph(nodes, edges, sp, rng, k):
    """Bras complet : deltoide, biceps, coude, ventre de l'avant-bras, poignet, paume et cinq doigts (trois phalanges).
    Retourne ses segments (p0, p1, r0, r1, os) pour le calcul des poids."""
    segs = []

    def add(pts, radii, bone, start=None):
        idx = chain(nodes, edges, pts, radii, start)
        prev_p = nodes[start][0] if start is not None else None
        prev_r = nodes[start][1] if start is not None else None
        for i in idx:
            p, r = nodes[i]
            r = r if not isinstance(r, tuple) else max(r)
            if prev_p is not None:
                segs.append((prev_p, p, prev_r if not isinstance(prev_r, tuple) else max(prev_r), r, bone))
            prev_p, prev_r = p, r
        return idx

    sh, el, wr, hd = Vector(sp["shoulder"]), Vector(sp["elbow"]), Vector(sp["wrist"]), Vector(sp["hand"])
    up = [sh.lerp(el, t) for t in (0.25, 0.5, 0.75, 1.0)]
    root_i = add([sp["root"], sh], [0.058, 0.05], "Core")
    upper = add(up, [0.047, 0.044, 0.038, 0.033], "Arm%d_Upper" % k, start=root_i[-1])
    fore = add([el.lerp(wr, t) for t in (0.2, 0.45, 0.75, 1.0)], [0.037, 0.035, 0.028, 0.021], "Arm%d_Fore" % k, start=upper[-1])
    fwd = (hd - wr).normalized()
    if sp["flat"]:
        across = fwd.cross(Vector((0, 0, 1))).normalized()
        palm_n = Vector((0, 0, -1))
    else:
        across = fwd.cross(Vector((0, 0, 1)))
        if across.length < 1e-3:
            across = Vector((0, 1, 0))
        across.normalize()
        palm_n = fwd.cross(across).normalized()
    palm = add([wr.lerp(hd, 0.55)], [(0.03, 0.016)], "Arm%d_Hand" % k, start=fore[-1])
    curl = sp["curl"]
    for off, length in ((-0.024, 0.075), (-0.008, 0.085), (0.008, 0.082), (0.022, 0.07)):
        kn = hd + across * off
        d1 = fwd
        pts = [kn]
        cur = kn
        for j in range(3):
            d1 = (d1 * math.cos(curl * 0.6) + palm_n * math.sin(curl * 0.6)).normalized()
            cur = cur + d1 * (length / 3)
            pts.append(cur)
        add(pts, [0.0085, 0.008, 0.007, 0.0055], "Arm%d_Hand" % k, start=palm[0])
    th = wr.lerp(hd, 0.3) - across * 0.03
    tdir = (fwd * 0.6 - across * 0.6 + palm_n * 0.3).normalized()
    add([th, th + tdir * 0.035, th + tdir * 0.065], [0.011, 0.009, 0.0065], "Arm%d_Hand" % k, start=palm[0])
    return segs


def clump_mass():
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=6, radius=C_RADIUS)
    co = np.array([tuple(v.co) for v in bm.verts])
    n = co / np.linalg.norm(co, axis=1)[:, None]
    # masse affaissee par son poids : plus large en bas, aplatie, bosselee
    lump = (fbm(n * 1.0, 2.2, 3, 41) - 0.5) * 0.14 + (fbm(n, 6.0, 3, 43) - 0.5) * 0.04
    r = C_RADIUS * (1 + lump)
    co = n * r[:, None]
    co[:, 2] *= 0.86
    co[:, 0] *= 1.05
    sag = np.clip(-n[:, 2], 0, 1)
    co[:, 0] *= 1 + 0.12 * sag
    co[:, 1] *= 1 + 0.12 * sag
    # bouche de lamproie vers +X : entonnoir profond
    mouth = np.clip((n[:, 0] - 0.8) / 0.18, 0, 1)
    co[:, 0] -= mouth * 0.13
    for v, c in zip(bm.verts, co):
        v.co = Vector(c) + C_CENTER
    me = bpy.data.meshes.new("ClumpMass")
    bm.to_mesh(me)
    bm.free()
    return link(bpy.data.objects.new("ClumpMass", me))


def clump_lip():
    """Levre epaisse autour de la bouche (anneau) : fondue dans la masse par le remaillage"""
    nodes, edges = [], []
    R = 0.15
    ring = []
    for k in range(20):
        a = 2 * math.pi * k / 20
        ring.append(Vector((C_CENTER.x + C_RADIUS * 1.05 - 0.05, math.cos(a) * R, C_CENTER.z + math.sin(a) * R * 0.92)))
    idx = chain(nodes, edges, ring, [0.042] * 20)
    edges.append((idx[-1], idx[0]))
    return skin_graph("ClumpLip", nodes, edges, subsurf=1)


def clump_teeth():
    """Trois couronnes de dents crochues tournees vers la gorge"""
    bm = bmesh.new()
    rng = np.random.default_rng(9)
    for ring, (R, x, n, ln) in enumerate(((0.13, 0.30, 22, 0.035), (0.095, 0.24, 18, 0.03), (0.06, 0.18, 14, 0.024))):
        for k in range(n):
            a = 2 * math.pi * (k + 0.5 * ring) / n
            base = Vector((C_CENTER.x + x, math.cos(a) * R, C_CENTER.z + math.sin(a) * R * 0.92))
            inward = Vector((-0.55, -math.cos(a), -math.sin(a))).normalized()
            L = ln * rng.uniform(0.7, 1.2)
            ret = bmesh.ops.create_cone(bm, cap_ends=True, segments=6, radius1=0.006, radius2=0.0008, depth=L)
            M = Matrix.Translation(base + inward * L * 0.5) @ inward.to_track_quat("Z", "Y").to_matrix().to_4x4()
            for v in ret["verts"]:
                v.co = M @ v.co
    me = bpy.data.meshes.new("ClumpTeeth")
    bm.to_mesh(me)
    bm.free()
    return link(bpy.data.objects.new("ClumpTeeth", me))


def clump_colors(o, specs):
    co = coords(o)
    x, y, z = co[:, 0], co[:, 1], co[:, 2]
    base = np.array([0.62, 0.42, 0.36])
    mott = fbm(co, 8.0, 4, 51)[:, None]
    col = base * (0.7 + 0.55 * mott)
    # rougeurs, ecchymoses, peau plus pale aux mains
    bruise = np.clip((fbm(co, 3.5, 3, 53) - 0.58) / 0.12, 0, 1)[:, None]
    col = col * (1 - 0.5 * bruise) + np.array([0.32, 0.12, 0.16]) * 0.5 * bruise
    vein = np.abs(fbm(co, 18.0, 3, 57) - 0.5)
    vmask = np.clip(1 - vein / 0.03, 0, 1)[:, None] * 0.6
    col = col * (1 - 0.4 * vmask) + np.array([0.25, 0.1, 0.18]) * 0.4 * vmask
    # gorge : rouge sombre puis noire
    rel = co - np.array(C_CENTER)
    radial = np.sqrt(rel[:, 1] ** 2 + rel[:, 2] ** 2)
    throat = np.clip((0.2 - radial) / 0.06, 0, 1) * np.clip((rel[:, 0] - 0.05) / 0.1, 0, 1) * np.clip((C_RADIUS + 0.05 - rel[:, 0]) / 0.1, 0, 1)
    deep = np.clip((0.3 - rel[:, 0]) / 0.15, 0, 1)
    col = col * (1 - throat[:, None]) + (np.array([0.18, 0.02, 0.025]) * (1 - deep[:, None]) + np.array([0.01, 0.0, 0.0]) * deep[:, None]) * throat[:, None]
    # crasse au sol (mains d'appui, bas de la masse)
    dirt = np.clip((0.12 - z) / 0.12, 0, 1)[:, None]
    col = col * (1 - 0.55 * dirt) + np.array([0.1, 0.08, 0.06]) * 0.55 * dirt
    col = np.clip(col, 0, 1)
    if "Col" not in o.data.color_attributes:
        o.data.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
    rgba = np.concatenate([col, np.ones((len(col), 1))], 1)
    o.data.color_attributes["Col"].data.foreach_set("color", rgba.reshape(-1))


def clump_armature(specs):
    b = [("Root", (0, 0, 0), (0, 0, 0.1), None), ("Core", tuple(C_CENTER), tuple(C_CENTER + Vector((0, 0, 0.25))), "Root")]
    for k, sp in enumerate(specs):
        b += [("Arm%d_Upper" % k, tuple(sp["shoulder"]), tuple(sp["elbow"]), "Core"),
              ("Arm%d_Fore" % k, tuple(sp["elbow"]), tuple(sp["wrist"]), "Arm%d_Upper" % k),
              ("Arm%d_Hand" % k, tuple(sp["wrist"]), tuple(Vector(sp["hand"]) + (Vector(sp["hand"]) - Vector(sp["wrist"])).normalized() * 0.08),
               "Arm%d_Fore" % k)]
    return make_armature("ClumpRig", b)


def clump_weights(arm, body, segs):
    """Chaque sommet suit le segment de tube le plus proche (doigts compris) : bras, avant-bras ou main de son bras, ou
    la masse (os Core). Aux articulations, melange avec le segment voisin du meme bras ; a l'epaule, transition douce
    vers la masse : la chair suit le bras sans se dechirer."""
    for g in list(body.vertex_groups):
        body.vertex_groups.remove(g)
    for m in [m for m in body.modifiers if m.type == "ARMATURE"]:
        body.modifiers.remove(m)
    co = coords(body)
    groups = {bn.name: body.vertex_groups.new(name=bn.name) for bn in arm.data.bones if bn.name != "Root"}
    P0 = np.array([tuple(a) for a, _, _, _, _ in segs])
    P1 = np.array([tuple(b) for _, b, _, _, _ in segs])
    R0 = np.array([r for _, _, r, _, _ in segs])
    R1 = np.array([r for _, _, _, r, _ in segs])
    bone = [b for _, _, _, _, b in segs]
    ab = P1 - P0
    L2 = np.maximum((ab * ab).sum(1), 1e-9)
    best = np.full(len(co), 1e9)
    best_i = np.zeros(len(co), dtype=int)
    second = np.full(len(co), 1e9)
    second_i = np.zeros(len(co), dtype=int)
    for c0 in range(0, len(co), 4000):
        c = co[c0:c0 + 4000]
        t = np.clip(((c[:, None, :] - P0[None]) * ab[None]).sum(2) / L2[None], 0, 1)
        q = P0[None] + t[..., None] * ab[None]
        d = np.linalg.norm(c[:, None, :] - q, axis=2) - (R0[None] + (R1 - R0)[None] * t)
        order = np.argsort(d, 1)
        best[c0:c0 + 4000] = d[np.arange(len(c)), order[:, 0]]
        best_i[c0:c0 + 4000] = order[:, 0]
        # second : le plus proche dont l'os est different mais du meme bras (ou la masse)
        for r in range(len(c)):
            b0 = bone[order[r, 0]]
            arm_id = b0.split("_")[0]
            for j in order[r, 1:12]:
                bj = bone[j]
                if bj != b0 and (bj == "Core" or bj.split("_")[0] == arm_id):
                    second[c0 + r] = d[r, j]
                    second_i[c0 + r] = j
                    break
    dc = np.linalg.norm(co - np.array(C_CENTER), axis=1)
    for vi in range(len(co)):
        b0 = bone[best_i[vi]]
        # sommet de la masse (loin de tout tube) : Core
        if best[vi] > 0.03 and dc[vi] < C_RADIUS * 1.35:
            groups["Core"].add([vi], 1.0, "REPLACE")
            continue
        da = max(best[vi], 0.0) + 0.004
        db = max(second[vi], 0.0) + 0.004
        if second[vi] < 1e8:
            wa = (1 / da ** 3) / (1 / da ** 3 + 1 / db ** 3)
            groups[b0].add([vi], float(wa), "REPLACE")
            if 1 - wa > 0.02:
                groups[bone[second_i[vi]]].add([vi], float(1 - wa), "ADD")
        else:
            groups[b0].add([vi], 1.0, "REPLACE")
    md = body.modifiers.new("Armature", "ARMATURE")
    md.object = arm
    body.parent = arm


def build_clump(preview=False):
    print("== Clump")
    reset()
    rng = np.random.default_rng(5)
    specs = clump_arm_specs()
    nodes, edges = [], []
    segs = []
    for k, sp in enumerate(specs):
        segs += clump_arm_graph(nodes, edges, sp, rng, k)
    arms = skin_graph("ClumpArms", nodes, edges, subsurf=2)
    mass = clump_mass()
    lip = clump_lip()
    high = join([mass, arms, lip], "ClumpHigh")
    voxel_remesh(high, 0.0035)
    # jonctions organiques : lissage plus fort pres de la surface de la masse (racines des bras)
    co = coords(high)
    dc = np.linalg.norm(co - np.array(C_CENTER), axis=1)
    vg = high.vertex_groups.new(name="Junction")
    near = np.where(np.abs(dc - C_RADIUS * 1.0) < 0.09)[0]
    for vi in near:
        vg.add([int(vi)], float(1 - abs(dc[vi] - C_RADIUS) / 0.09), "REPLACE")
    md = high.modifiers.new("Smooth", "SMOOTH")
    md.factor = 0.8
    md.iterations = 12
    md.vertex_group = "Junction"
    apply_all(high)
    smooth(high, 0.4, 2)
    # plis de chair et rides
    co = coords(high)
    nrm = normals(high)
    wr = (fbm(co * np.array([1.0, 1.0, 1.6]), 45.0, 3, 61) - 0.5) * 0.0018 + (fbm(co, 9.0, 3, 63) - 0.5) * 0.008
    set_coords(high, co + nrm * wr[:, None])
    clump_colors(high, specs)
    high.data.materials.clear()
    high.data.materials.append(vertex_color_material("ClumpBake"))
    print("  detaille :", len(high.data.vertices), "sommets")

    low = high.copy()
    low.data = high.data.copy()
    low.name = "SK_Clump_Mesh"
    link(low)
    decimate_to(low, 21000)
    smart_uv(low)
    bake(high, low, 2048, "T_Clump", ao_strength=0.6)
    bpy.data.objects.remove(high)
    arm = clump_armature(specs)
    clump_weights(arm, low, segs)
    teeth = clump_teeth()
    g = teeth.vertex_groups.new(name="Core")
    g.add(list(range(len(teeth.data.vertices))), 1.0, "REPLACE")
    low.data.materials.clear()
    low.data.materials.append(mat("ClumpSkin", (0.86, 0.7, 0.6)))
    teeth.data.materials.append(mat("ClumpTeeth", (0.72, 0.58, 0.32)))
    select_only([teeth])
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project()
    bpy.ops.object.mode_set(mode="OBJECT")
    low = join([low, teeth], "SK_Clump_Mesh")
    for p in low.data.polygons:
        p.use_smooth = True
    export_skeletal(arm, low, "SK_Clump")
    # bras d'appui / bras libres (le jeu les anime differemment) : indices dans le rapport
    REPORT["SK_Clump"]["GroundArms"] = [k for k, sp in enumerate(specs) if sp["ground"]]
    if preview:
        poses = []
        for k, sp in enumerate(specs):
            if not sp["ground"]:
                poses.append(("Arm%d_Upper" % k, (0, 1, 0), 25 * (1 if k % 2 else -1)))
                poses.append(("Arm%d_Fore" % k, (1, 0, 0), 35 * (1 if k % 3 else -1)))
        pose_preview(arm, low, "SK_Clump", poses=poses,
                     shots=[(Vector((0, 0, 0.5)), 3.3, Vector((1.0, -0.55, 0.35))), None, (Vector((0.3, 0, 0.6)), 1.4, Vector((1.0, -0.25, 0.1)))])
    return arm, low



def save_report():
    out = os.path.join(OUT_SK, "skeletal_models.json")
    old = {}
    if os.path.isfile(out):
        with open(out) as fh:
            old = json.load(fh)
    old.update(REPORT)
    with open(out, "w") as fh:
        json.dump(old, fh, indent=2)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    preview = "--preview" in args
    todo = [a for a in args if not a.startswith("--")] or ["wretch", "clump"]
    if "wretch" in todo:
        build_wretch(preview)
    if "clump" in todo:
        build_clump(preview)
    save_report()
