"""
Generation procedurale de tous les modeles 3D du jeu Backrooms avec Blender.

Utilisation (au choix) :
    blender -b -P generate_models.py                  # avec Blender installe
    blender -b -P generate_models.py -- SM_Smiler     # un seul modele
    python generate_models.py                         # avec le module "bpy" (pip install bpy)
Options : --no-preview  (ne pas faire de rendu d'apercu)

Conventions (importantes pour le code C++) :
  * unites : metres dans Blender (1 m = 100 uu dans Unreal)
  * l'avant de chaque objet / entite regarde vers +X
  * le pivot (origine) est defini pour chaque modele (voir les commentaires)
  * les NOMS DE MATERIAUX servent de "slots" : le jeu les reconnait par leur nom
    (ex: tout slot contenant "Glow" devient emissif)

Sortie : ../../RawAssets/Meshes/<Nom>.fbx   et   ../../RawAssets/Previews/<Nom>.png
"""
import math
import os
import random
import sys

import bpy  # doit etre importe avant bmesh / mathutils (module pip)
import bmesh
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_MESH = os.path.normpath(os.path.join(HERE, "..", "..", "RawAssets", "Meshes"))
OUT_PREV = os.path.normpath(os.path.join(HERE, "..", "..", "RawAssets", "Previews"))

# ---------------------------------------------------------------------------
# Palette de materiaux (nom de slot -> couleur d'apercu). Le jeu applique ses
# propres materiaux a partir du NOM, ces couleurs ne servent qu'a l'apercu.
# ---------------------------------------------------------------------------
PALETTE = {
    "Frame": (0.85, 0.85, 0.82, 0), "Glow": (1.0, 0.97, 0.85, 8), "GlowWarm": (1.0, 0.75, 0.4, 8),
    "GlowGreen": (0.2, 1.0, 0.3, 8), "GlowRed": (1.0, 0.1, 0.05, 8), "GlowWindow": (1.0, 0.8, 0.45, 4),
    "Metal": (0.45, 0.45, 0.47, 0), "DarkMetal": (0.12, 0.12, 0.13, 0), "Chrome": (0.8, 0.8, 0.82, 0),
    "Rust": (0.35, 0.2, 0.12, 0), "Wood": (0.35, 0.2, 0.1, 0), "Laminate": (0.72, 0.68, 0.6, 0),
    "Plastic": (0.85, 0.85, 0.83, 0), "BlackPlastic": (0.05, 0.05, 0.05, 0), "Fabric": (0.2, 0.22, 0.28, 0),
    "Cardboard": (0.55, 0.4, 0.24, 0), "Tape": (0.7, 0.62, 0.45, 0), "Paper": (0.9, 0.88, 0.8, 0),
    "Bottle": (0.85, 0.88, 0.9, 0), "Label": (0.8, 0.65, 0.4, 0), "Cap": (0.2, 0.35, 0.7, 0),
    "Battery": (0.08, 0.08, 0.08, 0), "Copper": (0.75, 0.45, 0.2, 0), "Rubber": (0.06, 0.06, 0.06, 0),
    "Sign": (0.9, 0.75, 0.1, 0), "Glass": (0.45, 0.6, 0.75, 0), "Brass": (0.8, 0.62, 0.25, 0),
    "Barn": (0.5, 0.12, 0.08, 0), "Roof": (0.18, 0.18, 0.2, 0), "Trim": (0.9, 0.9, 0.88, 0),
    "Siding": (0.7, 0.75, 0.78, 0), "Door": (0.3, 0.18, 0.1, 0), "Window": (0.08, 0.1, 0.14, 0),
    "Brick": (0.4, 0.2, 0.14, 0), "Concrete": (0.5, 0.5, 0.48, 0), "Ceramic": (0.85, 0.85, 0.8, 0),
    "Stalk": (0.75, 0.62, 0.3, 0), "Grain": (0.82, 0.65, 0.25, 0), "Rock": (0.35, 0.31, 0.27, 0),
    "Body": (0.02, 0.02, 0.02, 0), "Skin": (0.75, 0.68, 0.62, 0), "Hair": (0.02, 0.02, 0.02, 0),
    "Cloth": (0.3, 0.32, 0.35, 0), "Pants": (0.15, 0.17, 0.22, 0), "Dark": (0.01, 0.01, 0.01, 0),
    "Flesh": (0.6, 0.35, 0.33, 0), "Fur": (0.35, 0.28, 0.2, 0), "Wing": (0.42, 0.36, 0.3, 0),
    "Pattern": (0.15, 0.1, 0.08, 0), "Eye": (0.02, 0.02, 0.02, 0), "Party": (0.95, 0.85, 0.1, 0),
    "Face": (0.02, 0.02, 0.02, 0), "Shade": (0.85, 0.7, 0.5, 0), "Teeth": (0.9, 0.88, 0.8, 0),
}

_mats = {}


def mat(name):
    if name in _mats:
        return _mats[name]
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    r, g, b, e = PALETTE.get(name, (0.6, 0.6, 0.6, 0))
    m.diffuse_color = (r, g, b, 1)
    try:
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs["Base Color"].default_value = (r, g, b, 1)
            bsdf.inputs["Roughness"].default_value = 0.55
            if e > 0:
                key = "Emission Color" if "Emission Color" in bsdf.inputs else "Emission"
                bsdf.inputs[key].default_value = (r, g, b, 1)
                bsdf.inputs["Emission Strength"].default_value = e
            if name in ("Metal", "Chrome", "Brass", "Copper", "DarkMetal"):
                bsdf.inputs["Metallic"].default_value = 0.9
    except Exception:
        pass
    _mats[name] = m
    return m


# ---------------------------------------------------------------------------
# Primitives
# ---------------------------------------------------------------------------
def _finish(obj, material, rot=None):
    if rot:
        obj.rotation_euler = [math.radians(a) for a in rot]
    if material:
        obj.data.materials.clear()
        obj.data.materials.append(mat(material))
    return obj


def box(size, loc=(0, 0, 0), material=None, rot=None, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.active_object
    o.scale = size
    if bevel > 0:
        apply_transform(o)
        m = o.modifiers.new("Bevel", "BEVEL")
        m.width = bevel
        m.segments = 2
        apply_mods(o)
    return _finish(o, material, rot)


def cyl(r, depth, loc=(0, 0, 0), material=None, rot=None, verts=16, r2=None):
    if r2 is None:
        bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=depth, location=loc)
    else:
        bpy.ops.mesh.primitive_cone_add(vertices=verts, radius1=r, radius2=r2, depth=depth, location=loc)
    return _finish(bpy.context.active_object, material, rot)


def sphere(r, loc=(0, 0, 0), material=None, scale=(1, 1, 1), seg=24, rings=12, rot=None):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=rings, radius=r, location=loc)
    o = bpy.context.active_object
    o.scale = scale
    return _finish(o, material, rot)


def ico(r, loc=(0, 0, 0), material=None, sub=3, scale=(1, 1, 1)):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=sub, radius=r, location=loc)
    o = bpy.context.active_object
    o.scale = scale
    return _finish(o, material)


def torus(R, r, loc=(0, 0, 0), material=None, rot=None, seg=24, mseg=8):
    bpy.ops.mesh.primitive_torus_add(major_radius=R, minor_radius=r, major_segments=seg, minor_segments=mseg, location=loc)
    return _finish(bpy.context.active_object, material, rot)


def tube(points, radii, material=None, subdiv=1, name="tube"):
    """Membre organique : squelette de sommets + modificateur Skin + subdivision"""
    me = bpy.data.meshes.new(name)
    me.from_pydata([Vector(p) for p in points], [(i, i + 1) for i in range(len(points) - 1)], [])
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    sk = o.modifiers.new("Skin", "SKIN")
    sk.use_smooth_shade = True
    for i, rr in enumerate(radii):
        if not isinstance(rr, (tuple, list)):
            rr = (rr, rr)
        o.data.skin_vertices[0].data[i].radius = rr
    o.data.skin_vertices[0].data[0].use_root = True
    if subdiv:
        sb = o.modifiers.new("Sub", "SUBSURF")
        sb.levels = subdiv
        sb.render_levels = subdiv
    apply_mods(o)
    return _finish(o, material)


def lathe(profile, material=None, steps=24, loc=(0, 0, 0)):
    """Solide de revolution autour de Z. profile = [(rayon, z), ...]"""
    bm = bmesh.new()
    verts = [bm.verts.new((r, 0, z)) for r, z in profile]
    edges = [bm.edges.new((verts[i], verts[i + 1])) for i in range(len(verts) - 1)]
    bmesh.ops.spin(bm, geom=verts + edges, axis=(0, 0, 1), cent=(0, 0, 0), steps=steps, angle=2 * math.pi)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    me = bpy.data.meshes.new("lathe")
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new("lathe", me)
    o.location = loc
    bpy.context.collection.objects.link(o)
    return _finish(o, material)


def poly_plate(outline, thickness, material=None):
    """Plaque plate (contour dans le plan XY) extrudee en Z"""
    bm = bmesh.new()
    vs = [bm.verts.new((x, y, 0)) for x, y in outline]
    f = bm.faces.new(vs)
    res = bmesh.ops.extrude_face_region(bm, geom=[f])
    moved = [e for e in res["geom"] if isinstance(e, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, vec=(0, 0, thickness), verts=moved)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    me = bpy.data.meshes.new("plate")
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new("plate", me)
    bpy.context.collection.objects.link(o)
    return _finish(o, material)


# ---------------------------------------------------------------------------
# Operations
# ---------------------------------------------------------------------------
def apply_mods(o):
    if not o.modifiers:
        return o
    dg = bpy.context.evaluated_depsgraph_get()
    ev = o.evaluated_get(dg)
    me = bpy.data.meshes.new_from_object(ev)
    o.modifiers.clear()
    old = o.data
    o.data = me
    if old.users == 0:
        bpy.data.meshes.remove(old)
    return o


def apply_transform(o):
    bpy.context.view_layer.update()  # matrix_world n'est a jour qu'apres une mise a jour du depsgraph
    mw = o.matrix_world.copy()
    o.data.transform(mw)
    o.matrix_world = Matrix.Identity(4)
    return o


def _hash_noise(p, seed=0, freq=4.0):
    """Petit bruit de valeur 3D (python pur)"""
    def h(ix, iy, iz):
        n = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ (seed * 2654435761)
        n = (n << 13) ^ n
        return 1.0 - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7FFFFFFF) / 1073741824.0

    x, y, z = p[0] * freq, p[1] * freq, p[2] * freq
    ix, iy, iz = math.floor(x), math.floor(y), math.floor(z)
    fx, fy, fz = x - ix, y - iy, z - iz
    sx, sy, sz = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy), fz * fz * (3 - 2 * fz)
    acc = 0.0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (sx if dx else 1 - sx) * (sy if dy else 1 - sy) * (sz if dz else 1 - sz)
                acc += w * h(ix + dx, iy + dy, iz + dz)
    return acc


def displace(o, strength=0.05, freq=4.0, seed=0, octaves=2):
    apply_transform(o)
    me = o.data
    me.update()
    for v in me.vertices:
        n = 0.0
        a, f = 1.0, freq
        for k in range(octaves):
            n += a * _hash_noise(v.co, seed + k, f)
            a *= 0.5
            f *= 2.0
        v.co += v.normal * n * strength
    me.update()
    return o


def join(objs, name):
    for o in objs:
        apply_mods(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    apply_transform(o)
    o.name = name
    o.data.name = name
    return o


def finalize(o, smooth_angle=35.0, uv_scale=1.0, all_smooth=False):
    """UV en projection cubique, normales lissees par angle"""
    me = o.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bm.normal_update()
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for l in f.loops:
            c = l.vert.co
            if ax == 0:
                u, v = c.y, c.z
            elif ax == 1:
                u, v = c.x, c.z
            else:
                u, v = c.x, c.y
            l[uv].uv = (u * uv_scale, v * uv_scale)
    th = math.radians(smooth_angle)
    for f in bm.faces:
        f.smooth = True
    for e in bm.edges:
        if all_smooth:
            e.smooth = True
        else:
            ang = e.calc_face_angle(math.pi)
            e.smooth = ang < th
    bm.to_mesh(me)
    bm.free()
    return o


# ---------------------------------------------------------------------------
# PROPS
# ---------------------------------------------------------------------------
def m_light_panel():
    # Dalle lumineuse 120x60 cm. Pivot : centre de la face superieure (plafond).
    p = []
    L, W, b, h = 1.22, 0.62, 0.035, 0.035
    p.append(box((L, b, h), (0, W / 2 - b / 2, -h / 2), "Frame"))
    p.append(box((L, b, h), (0, -W / 2 + b / 2, -h / 2), "Frame"))
    p.append(box((b, W, h), (L / 2 - b / 2, 0, -h / 2), "Frame"))
    p.append(box((b, W, h), (-L / 2 + b / 2, 0, -h / 2), "Frame"))
    p.append(box((L - 2 * b, W - 2 * b, 0.01), (0, 0, -0.02), "Glow"))
    p.append(box((L - 2 * b, 0.02, 0.015), (0, 0, -0.026), "Frame"))
    for x in (-0.3, 0.3):
        p.append(box((0.02, W - 2 * b, 0.012), (x, 0, -0.026), "Frame"))
    return join(p, "SM_LightPanel")


def m_sky_panel():
    # Puits de lumiere carre (Niveau 37). Pivot : face superieure.
    p = []
    S, b, h = 1.0, 0.06, 0.05
    for s in (-1, 1):
        p.append(box((S, b, h), (0, s * (S / 2 - b / 2), -h / 2), "Trim"))
        p.append(box((b, S, h), (s * (S / 2 - b / 2), 0, -h / 2), "Trim"))
    p.append(box((S - 2 * b, S - 2 * b, 0.01), (0, 0, -0.01), "Glow"))
    return join(p, "SM_SkyPanel")


def m_light_tube():
    # Reglette industrielle suspendue. Pivot : point d'accroche au plafond.
    p = []
    drop = 0.5
    for x in (-0.5, 0.5):
        p.append(cyl(0.006, drop, (x, 0, -drop / 2), "DarkMetal", verts=6))
    p.append(box((1.3, 0.16, 0.05), (0, 0, -drop - 0.025), "Metal", bevel=0.005))
    for y in (-0.04, 0.04):
        p.append(cyl(0.017, 1.22, (0, y, -drop - 0.07), "Glow", rot=(0, 90, 0), verts=12))
    return join(p, "SM_LightTube")


def m_light_bulb():
    # Ampoule grillagee. Pivot : plafond.
    p = [cyl(0.06, 0.02, (0, 0, -0.01), "DarkMetal", verts=20),
         cyl(0.025, 0.08, (0, 0, -0.06), "DarkMetal", verts=12),
         sphere(0.05, (0, 0, -0.14), "GlowWarm", scale=(1, 1, 1.25), seg=16, rings=8)]
    for a in range(4):
        ang = a * math.pi / 2
        x, y = math.cos(ang) * 0.075, math.sin(ang) * 0.075
        p.append(cyl(0.004, 0.16, (x, y, -0.13), "DarkMetal", verts=5))
    for z in (-0.07, -0.13, -0.2):
        p.append(torus(0.075, 0.004, (0, 0, z), "DarkMetal", mseg=4))
    p.append(cyl(0.075, 0.006, (0, 0, -0.21), "DarkMetal", verts=16))
    return join(p, "SM_LightBulb")


def m_sconce():
    # Applique murale (hotel). Pivot : contre le mur, l'avant vers +X.
    p = [box((0.03, 0.12, 0.2), (0.015, 0, 0), "Brass", bevel=0.005),
         cyl(0.012, 0.12, (0.08, 0, 0), "Brass", rot=(0, 90, 0), verts=8),
         lathe([(0.0, -0.02), (0.06, -0.02), (0.085, 0.12), (0.0, 0.12)], "Shade", steps=20, loc=(0.15, 0, 0.0)),
         sphere(0.035, (0.15, 0, 0.03), "GlowWarm", seg=12, rings=6)]
    return join(p, "SM_Sconce")


def m_street_lamp():
    # Lampadaire 6 m. Pivot : au sol.
    p = [cyl(0.16, 0.4, (0, 0, 0.2), "DarkMetal", verts=12),
         cyl(0.08, 6.0, (0, 0, 3.0), "DarkMetal", verts=12, r2=0.06),
         cyl(0.045, 1.3, (0.62, 0, 5.95), "DarkMetal", rot=(0, 90, 0), verts=8),
         box((0.55, 0.28, 0.12), (1.3, 0, 5.92), "DarkMetal", bevel=0.02),
         box((0.45, 0.2, 0.02), (1.3, 0, 5.855), "GlowWarm")]
    return join(p, "SM_StreetLamp")


def m_almond_water():
    # Bouteille d'eau d'amande. Pivot : base.
    prof = [(0.0, 0.0), (0.03, 0.0), (0.034, 0.006), (0.035, 0.02), (0.035, 0.155), (0.03, 0.185),
            (0.016, 0.21), (0.014, 0.222), (0.0, 0.222)]
    p = [lathe(prof, "Bottle", 24),
         lathe([(0.0, 0.05), (0.0358, 0.05), (0.0358, 0.135), (0.0, 0.135)], "Label", 24),
         cyl(0.0155, 0.022, (0, 0, 0.232), "Cap", verts=16)]
    return join(p, "SM_AlmondWater")


def m_battery():
    # Deux grosses piles. Pivot : sol.
    p = []
    for y in (-0.026, 0.026):
        p.append(cyl(0.024, 0.085, (0, y, 0.024), "Battery", rot=(0, 90, 0), verts=16))
        p.append(cyl(0.0245, 0.025, (0.03, y, 0.024), "Copper", rot=(0, 90, 0), verts=16))
        p.append(cyl(0.008, 0.008, (0.046, y, 0.024), "Metal", rot=(0, 90, 0), verts=8))
    return join(p, "SM_Battery")


def m_note():
    p = [box((0.21, 0.297, 0.002), (0, 0, 0.001), "Paper", rot=(0, 0, 12))]
    return join(p, "SM_Note")


def m_flashlight():
    # Lampe torche (vue a la premiere personne). Pivot : poignee, faisceau vers +X.
    p = [cyl(0.019, 0.15, (-0.03, 0, 0), "DarkMetal", rot=(0, 90, 0), verts=16),
         cyl(0.02, 0.05, (0.07, 0, 0), "DarkMetal", rot=(0, 90, 0), verts=16, r2=0.029),
         cyl(0.03, 0.02, (0.105, 0, 0), "DarkMetal", rot=(0, 90, 0), verts=20),
         cyl(0.025, 0.004, (0.116, 0, 0), "Glow", rot=(0, 90, 0), verts=20),
         box((0.02, 0.012, 0.01), (0.0, 0, 0.02), "Rubber")]
    for x in (-0.08, -0.06, -0.04):
        p.append(torus(0.0195, 0.0025, (x, 0, 0), "Rubber", rot=(0, 90, 0), mseg=4, seg=16))
    return join(p, "SM_Flashlight")


def m_crate():
    p = [box((0.6, 0.45, 0.4), (0, 0, 0.2), "Cardboard", bevel=0.006),
         box((0.62, 0.07, 0.004), (0, 0, 0.401), "Tape"),
         box((0.004, 0.452, 0.4), (0.0, 0, 0.2), "Cardboard")]
    return join(p, "SM_Crate")


def m_pipe():
    # Troncon de 1 m le long de X, centre a l'origine (mis a l'echelle en X par le jeu).
    p = [cyl(0.08, 1.0, (0, 0, 0), "Rust", rot=(0, 90, 0), verts=16)]
    for x in (-0.47, 0.47):
        p.append(cyl(0.1, 0.04, (x, 0, 0), "Metal", rot=(0, 90, 0), verts=16))
    return join(p, "SM_Pipe")


def m_electric_box():
    # Armoire electrique murale. Pivot : bas, contre le mur, avant vers +X.
    p = [box((0.22, 0.55, 0.75), (0.11, 0, 0.375), "Metal", bevel=0.01),
         box((0.005, 0.5, 0.7), (0.222, 0, 0.375), "DarkMetal"),
         box((0.02, 0.03, 0.12), (0.235, 0.18, 0.4), "DarkMetal"),
         poly_plate([(0, -0.07), (0.12, 0), (0, 0.07)], 0.004, "Sign"),
         cyl(0.03, 1.5, (0.08, -0.2, 1.5), "Metal", verts=10)]
    # triangle d'avertissement pose sur la porte
    tri = p[3]
    tri.rotation_euler = (math.radians(90), 0, math.radians(90))
    tri.location = (0.226, 0, 0.55)
    return join(p, "SM_ElectricBox")


def m_desk():
    p = [box((1.6, 0.8, 0.035), (0, 0, 0.735), "Laminate", bevel=0.004),
         box((0.03, 0.75, 0.72), (-0.77, 0, 0.36), "Laminate"),
         box((0.03, 0.75, 0.72), (0.77, 0, 0.36), "Laminate"),
         box((1.5, 0.02, 0.45), (0, 0.35, 0.45), "Laminate"),
         box((0.42, 0.7, 0.62), (0.52, -0.02, 0.37), "Metal", bevel=0.005)]
    for z in (0.22, 0.42, 0.6):
        p.append(box((0.3, 0.012, 0.012), (0.52, -0.375, z), "Chrome"))
    # ecran cathodique de bureau
    p.append(box((0.38, 0.36, 0.33), (-0.2, 0.12, 0.93), "Plastic", bevel=0.02))
    p.append(box((0.3, 0.005, 0.24), (-0.2, -0.062, 0.95), "Window"))
    p.append(box((0.42, 0.15, 0.025), (-0.2, -0.2, 0.765), "Plastic"))
    return join(p, "SM_Desk")


def m_office_chair():
    p = [box((0.48, 0.48, 0.08), (0, 0, 0.47), "Fabric", bevel=0.02),
         box((0.08, 0.46, 0.55), (-0.25, 0, 0.8), "Fabric", bevel=0.03),
         cyl(0.025, 0.36, (0, 0, 0.26), "Chrome", verts=10)]
    for a in range(5):
        ang = a * 2 * math.pi / 5
        x, y = math.cos(ang) * 0.17, math.sin(ang) * 0.17
        leg = box((0.34, 0.04, 0.03), (x, y, 0.08), "BlackPlastic")
        leg.rotation_euler = (0, 0, ang)
        p.append(leg)
        p.append(sphere(0.03, (x * 2, y * 2, 0.03), "BlackPlastic", seg=8, rings=4))
    return join(p, "SM_OfficeChair")


def m_water_cooler():
    p = [box((0.32, 0.32, 0.95), (0, 0, 0.475), "Plastic", bevel=0.015),
         cyl(0.135, 0.45, (0, 0, 1.2), "Glass", verts=20),
         cyl(0.05, 0.06, (0, 0, 0.98), "Plastic", verts=12),
         box((0.04, 0.04, 0.04), (0.17, 0.06, 0.75), "Cap"),
         box((0.04, 0.04, 0.04), (0.17, -0.06, 0.75), "GlowRed"),
         box((0.14, 0.24, 0.02), (0.12, 0, 0.6), "DarkMetal")]
    return join(p, "SM_WaterCooler")


def m_partition():
    p = [box((0.05, 1.2, 1.4), (0, 0, 0.7), "Fabric"),
         box((0.06, 1.22, 0.03), (0, 0, 1.415), "Metal"),
         box((0.06, 0.03, 1.4), (0, 0.6, 0.7), "Metal"),
         box((0.06, 0.03, 1.4), (0, -0.6, 0.7), "Metal")]
    return join(p, "SM_Partition")


def door_frame(p, w, h, depth, material):
    p.append(box((depth, 0.08, h + 0.08), (depth / 2, w / 2 + 0.04, (h + 0.08) / 2), material))
    p.append(box((depth, 0.08, h + 0.08), (depth / 2, -w / 2 - 0.04, (h + 0.08) / 2), material))
    p.append(box((depth, w + 0.16, 0.08), (depth / 2, 0, h + 0.04), material))


def m_exit_door():
    # Porte de secours metallique + panneau EXIT. Pivot : bas, face du mur, avant +X.
    p = []
    door_frame(p, 0.95, 2.1, 0.07, "DarkMetal")
    p.append(box((0.045, 0.94, 2.09), (0.03, 0, 1.045), "Metal", bevel=0.004))
    p.append(box((0.05, 0.7, 0.05), (0.08, 0.02, 1.0), "Chrome", bevel=0.01))
    p.append(box((0.3, 0.2, 0.004), (0.054, 0, 1.6), "Window"))
    p.append(box((0.06, 0.36, 0.16), (0.04, 0, 2.35), "BlackPlastic", bevel=0.01))
    p.append(box((0.005, 0.3, 0.1), (0.072, 0, 2.35), "GlowGreen"))
    return join(p, "SM_ExitDoor")


def m_hotel_door():
    p = []
    door_frame(p, 0.92, 2.15, 0.06, "Trim")
    p.append(box((0.045, 0.9, 2.13), (0.025, 0, 1.065), "Door", bevel=0.004))
    for z in (0.45, 1.35):
        p.append(box((0.01, 0.6, 0.65), (0.05, 0, z + 0.2), "Wood", bevel=0.01))
    p.append(sphere(0.03, (0.08, 0.36, 1.0), "Brass", seg=12, rings=6))
    p.append(box((0.01, 0.12, 0.08), (0.05, 0, 1.75), "Brass"))
    return join(p, "SM_HotelDoor")


def m_elevator():
    p = []
    door_frame(p, 1.3, 2.25, 0.1, "Chrome")
    p.append(box((0.04, 0.645, 2.24), (0.03, 0.33, 1.12), "Chrome"))
    p.append(box((0.04, 0.645, 2.24), (0.03, -0.33, 1.12), "Chrome"))
    p.append(box((0.03, 0.12, 0.25), (0.02, -0.95, 1.15), "Metal"))
    p.append(cyl(0.025, 0.02, (0.04, -0.95, 1.2), "GlowWarm", rot=(0, 90, 0), verts=12))
    p.append(box((0.04, 0.5, 0.14), (0.04, 0, 2.55), "BlackPlastic"))
    p.append(box((0.005, 0.12, 0.08), (0.062, 0, 2.55), "GlowRed"))
    return join(p, "SM_ElevatorDoor")


def m_ladder():
    # Echelle murale 3,2 m. Pivot : bas, contre le mur.
    p = []
    for y in (-0.22, 0.22):
        p.append(cyl(0.022, 3.2, (0.14, y, 1.6), "Metal", verts=8))
        for z in (0.4, 1.6, 2.8):
            p.append(cyl(0.012, 0.14, (0.07, y, z), "Metal", rot=(0, 90, 0), verts=6))
    for i in range(10):
        p.append(cyl(0.015, 0.44, (0.14, 0, 0.3 + i * 0.3), "Metal", rot=(90, 0, 0), verts=6))
    return join(p, "SM_Ladder")


def m_barn():
    # Grange 10 x 8 m (Niveau 10). Pivot : sol, centre ; grande porte vers +X.
    p = [box((10, 8, 4.2), (0, 0, 2.1), "Barn")]
    # pignons
    for x in (-5.0, 5.0):
        g = poly_plate([(-4, 0), (4, 0), (0, 2.8)], 0.2, "Barn")
        g.rotation_euler = (math.radians(90), 0, math.radians(90))
        g.location = (x - 0.1 if x > 0 else x - 0.1, 0, 4.2)
        p.append(g)
    for s in (-1, 1):
        r = box((10.6, 4.9, 0.15), (0, s * 2.05, 5.6), "Roof")
        r.rotation_euler = (s * -math.radians(34.99), 0, 0)
        p.append(r)
    p.append(box((0.12, 3.2, 3.4), (5.05, 0, 1.7), "Door"))
    for s in (-1, 1):
        d = box((0.06, 0.2, 4.6), (5.12, 0, 1.7), "Trim")
        d.rotation_euler = (s * math.radians(43), 0, 0)
        p.append(d)
    p.append(box((0.1, 3.4, 0.2), (5.12, 0, 3.4), "Trim"))
    return join(p, "SM_Barn")


def m_house():
    # Maison de banlieue 8 x 7 m (Niveau 9). Pivot : sol, centre ; porte vers +X.
    p = [box((8, 7, 3.0), (0, 0, 1.5 + 0.3), "Siding"),
         box((8.2, 7.2, 0.3), (0, 0, 0.15), "Concrete")]
    for x in (-4.0, 4.0):
        g = poly_plate([(-3.5, 0), (3.5, 0), (0, 2.2)], 0.15, "Siding")
        g.rotation_euler = (math.radians(90), 0, math.radians(90))
        g.location = (x - 0.075, 0, 3.3)
        p.append(g)
    for s in (-1, 1):
        r = box((8.8, 4.35, 0.15), (0, s * 1.85, 4.4), "Roof")
        r.rotation_euler = (s * -math.radians(32.2), 0, 0)
        p.append(r)
    p.append(box((0.6, 0.6, 1.5), (-1.5, 2.0, 5.2), "Brick"))
    # porte + porche
    p.append(box((0.08, 1.0, 2.1), (4.02, 0, 1.35), "Door"))
    p.append(box((0.1, 1.2, 0.1), (4.04, 0, 2.45), "Trim"))
    p.append(box((1.2, 2.0, 0.15), (4.6, 0, 0.075), "Concrete"))
    rnd = random.Random(7)
    for (x, y) in [(4.02, 2.2), (4.02, -2.2), (-4.02, 1.6), (-4.02, -1.6)]:
        lit = rnd.random() < 0.4
        p.append(box((0.06, 1.2, 1.1), (x, y, 1.8), "GlowWindow" if lit else "Window"))
        p.append(box((0.1, 1.35, 0.08), (x, y, 2.38), "Trim"))
        p.append(box((0.1, 1.35, 0.08), (x, y, 1.22), "Trim"))
    for (x, y) in [(2.0, 3.52), (-2.0, 3.52), (2.0, -3.52), (-2.0, -3.52)]:
        p.append(box((1.2, 0.06, 1.1), (x, y, 1.8), "Window"))
    return join(p, "SM_House")


def m_power_pole():
    p = [cyl(0.13, 9.0, (0, 0, 4.5), "Wood", verts=10, r2=0.11),
         box((0.12, 2.4, 0.12), (0, 0, 8.4), "Wood")]
    for y in (-1.0, -0.35, 0.35, 1.0):
        p.append(cyl(0.04, 0.16, (0, y, 8.54), "Ceramic", verts=8))
    return join(p, "SM_PowerPole")


def m_wheat():
    # Touffe de ble ~1,1 m. Pivot : sol.
    rnd = random.Random(42)
    p = []
    for i in range(26):
        a = rnd.random() * 2 * math.pi
        d = rnd.random() ** 0.5 * 0.28
        x, y = math.cos(a) * d, math.sin(a) * d
        h = rnd.uniform(0.85, 1.2)
        tilt_x, tilt_y = rnd.uniform(-8, 8), rnd.uniform(-8, 8)
        st = cyl(0.006, h, (x, y, h / 2), "Stalk", verts=4, rot=(tilt_x, tilt_y, 0))
        p.append(st)
        top = Matrix.Translation((x, y, 0)) @ Matrix.Rotation(math.radians(tilt_y), 4, "Y") @ \
            Matrix.Rotation(math.radians(tilt_x), 4, "X") @ Vector((0, 0, h))
        p.append(sphere(0.018, top + Vector((0, 0, 0.05)), "Grain", scale=(1, 1, 4.5), seg=6, rings=4,
                        rot=(tilt_x, tilt_y, 0)))
    return join(p, "SM_Wheat")


def m_rock():
    o = ico(0.8, (0, 0, 0.45), "Rock", sub=4, scale=(1.25, 1.0, 0.75))
    displace(o, 0.22, 1.6, seed=3, octaves=3)
    return join([o], "SM_Rock")


# ---------------------------------------------------------------------------
# ENTITES
# ---------------------------------------------------------------------------
def m_smiler():
    # Entite 3 : Smilers. Corps sombre presque invisible, yeux et sourire lumineux.
    # Pivot : centre ; regarde vers +X.
    body = ico(0.5, (0, 0, 0), "Body", sub=4, scale=(0.55, 1.0, 1.0))
    displace(body, 0.1, 2.2, seed=9, octaves=3)
    p = [body]
    fx = 0.36
    for s in (-1, 1):
        e = sphere(0.09, (fx, s * 0.17, 0.17), "Glow", scale=(0.35, 1.0, 0.5), seg=16, rings=8,
                   rot=(s * 20, 0, 0))
        p.append(e)
    # sourire demesure : deux rangees de dents le long d'une courbe en U
    n = 21
    for i in range(n):
        t = (i / (n - 1)) * 2 - 1  # -1..1
        y = t * 0.34
        zu = -0.08 + 0.12 * t * t
        gap = 0.09 * (1 - t * t) + 0.015
        x = fx + 0.02 - 0.07 * t * t
        ang = math.degrees(math.atan(t * 0.7))
        p.append(box((0.03, 0.028, 0.055), (x, y, zu - 0.025), "Glow", rot=(0, 0, -ang)))
        p.append(box((0.03, 0.028, 0.05), (x, y, zu - gap - 0.025), "Glow", rot=(0, 0, -ang)))
    # interieur de la bouche
    p.append(sphere(0.2, (fx - 0.08, 0, -0.1), "Dark", scale=(0.3, 1.6, 0.4), seg=16, rings=8))
    return join(p, "SM_Smiler")


HOUND = dict(body_z=0.62, leg_len=0.62, front=(0.33, 0.16), back=(-0.30, 0.14))


def m_hound_body():
    # Entite 8 : Hounds. Humanoide a quatre pattes, longs cheveux noirs.
    # Pivot : centre du torse (place a 62 cm du sol par le jeu).
    torso = tube([(-0.36, 0, 0), (-0.1, 0, 0.03), (0.22, 0, 0.05), (0.42, 0, 0.1)],
                 [(0.12, 0.1), (0.11, 0.09), (0.15, 0.12), (0.05, 0.05)], "Skin", subdiv=2, name="torso")
    displace(torso, 0.012, 9.0, seed=4)
    head = sphere(0.11, (0.55, 0, 0.1), "Skin", scale=(1.2, 0.9, 0.95), seg=20, rings=10)
    p = [torso, head]
    rnd = random.Random(8)
    for i in range(36):
        a = rnd.uniform(-1.4, 1.4)
        ox = 0.5 + rnd.uniform(-0.08, 0.1)
        y = math.sin(a) * 0.1
        top = (ox, y, 0.19 + rnd.uniform(-0.02, 0.02))
        ln = rnd.uniform(0.28, 0.48)
        bot = (ox + 0.12 + rnd.uniform(-0.03, 0.05), y * 1.4, top[2] - ln)
        p.append(tube([top, ((top[0] + bot[0]) / 2 + 0.03, (top[1] + bot[1]) / 2, (top[2] + bot[2]) / 2), bot],
                      [0.012, 0.009, 0.003], "Hair", subdiv=0, name="hair"))
    # colonne vertebrale saillante
    for i in range(7):
        x = -0.3 + i * 0.1
        p.append(sphere(0.022, (x, 0, 0.11 + 0.02 * math.sin(i)), "Skin", seg=8, rings=4))
    return join(p, "SM_Hound_Body")


def m_hound_leg():
    # Patte : pivot a l'articulation, vers le bas sur 62 cm, "main" vers +X.
    L = HOUND["leg_len"]
    leg = tube([(0, 0, 0), (0.07, 0, -0.28), (0.0, 0, -L + 0.05), (0.1, 0, -L + 0.015)],
               [0.055, 0.042, 0.03, (0.04, 0.03)], "Skin", subdiv=2, name="leg")
    fingers = []
    for s in (-1, 0, 1):
        fingers.append(tube([(0.08, s * 0.02, -L + 0.02), (0.16, s * 0.035, -L + 0.008)], [0.01, 0.006],
                            "Skin", subdiv=1, name="finger"))
    return join([leg] + fingers, "SM_Hound_Leg")


# Gabarits humanoides (en metres) : utilises a l'identique par le C++ (BackroomsEntity.cpp)
HUMANOIDS = {
    "Faceling":    dict(hip=0.92, sh=1.45, sw=0.20, hw=0.10, arm=0.68, head=0.115, hunch=0.00,
                        torso="Cloth", arms="Skin", legs="Pants", skin="Skin"),
    "SkinStealer": dict(hip=1.12, sh=1.80, sw=0.23, hw=0.11, arm=1.00, head=0.125, hunch=0.12,
                        torso="Flesh", arms="Flesh", legs="Flesh", skin="Flesh"),
    "Wretch":      dict(hip=0.80, sh=1.22, sw=0.18, hw=0.09, arm=0.78, head=0.10, hunch=0.28,
                        torso="Skin", arms="Skin", legs="Skin", skin="Skin"),
    "Partygoer":   dict(hip=0.88, sh=1.42, sw=0.22, hw=0.11, arm=0.66, head=0.14, hunch=0.00,
                        torso="Party", arms="Party", legs="Party", skin="Party"),
}


def m_humanoid_torso(kind):
    s = HUMANOIDS[kind]
    top = s["sh"] - s["hip"]
    hx = s["hunch"]
    chest_r = (0.16, 0.11) if kind != "Wretch" else (0.13, 0.09)
    torso = tube([(0, 0, 0), (hx * 0.3, 0, top * 0.4), (hx * 0.8, 0, top - 0.06), (hx * 1.05, 0, top + 0.08)],
                 [(0.13, 0.1), (0.12, 0.085), chest_r, (0.05, 0.05)], s["torso"], subdiv=2, name="torso")
    shoulders = tube([(hx * 0.85, -s["sw"] + 0.03, top - 0.02), (hx * 0.85, 0, top + 0.01),
                      (hx * 0.85, s["sw"] - 0.03, top - 0.02)], [0.06, 0.08, 0.06], s["torso"], subdiv=2,
                     name="shoulders")
    hc = Vector((hx * 1.15 + 0.01, 0, top + 0.05 + s["head"] * 0.95))
    head_scale = (0.92, 0.85, 1.12) if kind != "SkinStealer" else (0.85, 0.75, 1.35)
    head = sphere(s["head"], hc, s["skin"], scale=head_scale, seg=24, rings=12)
    p = [torso, shoulders, head]
    fx = hc.x + s["head"] * head_scale[0] * 0.92
    if kind == "SkinStealer":
        displace(torso, 0.025, 7.0, seed=12)
        displace(head, 0.015, 9.0, seed=13)
        for sgn in (-1, 1):
            p.append(sphere(0.026, (fx - 0.01, sgn * 0.045, hc.z + 0.03), "Dark", scale=(0.6, 1, 1.4), seg=10, rings=6))
        p.append(box((0.02, 0.08, 0.012), (fx - 0.005, 0, hc.z - 0.07), "Dark"))
    elif kind == "Wretch":
        displace(torso, 0.012, 12.0, seed=21)
        for i in range(5):  # cotes saillantes
            z = top * 0.45 + i * 0.05
            p.append(torus(0.1, 0.008, (hx * 0.6 + 0.02, 0, z), "Skin", rot=(0, 90 - 20, 0), mseg=4, seg=16))
        for sgn in (-1, 1):
            p.append(sphere(0.022, (fx - 0.012, sgn * 0.035, hc.z + 0.02), "Dark", seg=10, rings=6))
        p.append(sphere(0.025, (fx - 0.01, 0, hc.z - 0.05), "Dark", scale=(0.6, 1.2, 0.9), seg=10, rings=6))
    elif kind == "Partygoer":
        # visage souriant "=)" dessine
        for sgn in (-1, 1):
            p.append(sphere(0.022, (fx, sgn * 0.05, hc.z + 0.04), "Face", scale=(0.4, 0.8, 1.4), seg=10, rings=6))
        pts = []
        for i in range(9):
            t = i / 8 * 2 - 1
            yy = t * 0.075
            zz = hc.z - 0.045 - 0.03 * (1 - t * t)
            xx = hc.x + math.sqrt(max(0.0, (s["head"] * 0.9) ** 2 - yy ** 2 - (zz - hc.z) ** 2 * 0.6)) * 0.98 + 0.004
            pts.append((xx, yy, zz))
        p.append(tube(pts, [0.008] * len(pts), "Face", subdiv=1, name="smile"))
    # Faceling : visage parfaitement lisse (aucun trait)
    return join(p, f"SM_{kind}_Torso")


def m_humanoid_arm(kind):
    s = HUMANOIDS[kind]
    L = s["arm"]
    r = 0.045 if kind != "Wretch" else 0.032
    arm = tube([(0, 0, 0), (0.0, 0, -L * 0.47), (0.03, 0, -L * 0.88), (0.04, 0, -L)],
               [r, r * 0.85, r * 0.65, (r * 0.75, r * 0.4)], s["arms"], subdiv=2, name="arm")
    p = [arm]
    if kind in ("SkinStealer", "Wretch"):
        for k in (-1, 0, 1):  # longs doigts
            p.append(tube([(0.04, k * 0.012, -L), (0.05, k * 0.02, -L - 0.12)], [0.008, 0.004], s["arms"], subdiv=1,
                          name="finger"))
    return join(p, f"SM_{kind}_Arm")


def m_humanoid_leg(kind):
    s = HUMANOIDS[kind]
    L = s["hip"]
    r = 0.07 if kind != "Wretch" else 0.05
    leg = tube([(0, 0, 0), (0.02, 0, -L * 0.5), (0.0, 0, -L + 0.07), (0.0, 0, -L + 0.03)],
               [r, r * 0.75, r * 0.55, r * 0.5], s["legs"], subdiv=2, name="leg")
    foot = box((0.22, 0.09, 0.06), (0.06, 0, -L + 0.03), s["skin"] if kind != "Faceling" else "BlackPlastic", bevel=0.02)
    return join([leg, foot], f"SM_{kind}_Leg")


def m_moth_body():
    # Entite 4 : Deathmoths. Pivot : centre du thorax ; tete vers +X.
    body = tube([(-0.42, 0, -0.02), (-0.15, 0, 0.0), (0.05, 0, 0.02), (0.18, 0, 0.03)],
                [(0.07, 0.07), (0.11, 0.1), (0.12, 0.11), (0.07, 0.07)], "Fur", subdiv=2, name="body")
    displace(body, 0.01, 18.0, seed=31)
    p = [body]
    for s in (-1, 1):
        p.append(sphere(0.04, (0.2, s * 0.05, 0.05), "Eye", seg=12, rings=6))
        p.append(tube([(0.22, s * 0.03, 0.08), (0.35, s * 0.12, 0.25), (0.42, s * 0.2, 0.33)], [0.008, 0.012, 0.003],
                      "Fur", subdiv=1, name="antenna"))
        for k in range(3):
            x0 = 0.08 - k * 0.09
            p.append(tube([(x0, s * 0.06, -0.06), (x0 + 0.04, s * 0.22, -0.12), (x0 + 0.06, s * 0.28, -0.3)],
                          [0.012, 0.009, 0.004], "DarkMetal", subdiv=1, name="leg"))
    return join(p, "SM_Deathmoth_Body")


def m_moth_wing():
    # Aile : racine a l'origine, s'etend vers +Y. Le jeu la mire pour l'autre cote.
    outline = [(0.16, 0.0), (0.32, 0.3), (0.34, 0.62), (0.18, 0.86), (0.02, 0.9), (-0.12, 0.7),
               (-0.3, 0.6), (-0.44, 0.36), (-0.36, 0.12), (-0.18, 0.0)]
    w = poly_plate(outline, 0.012, "Wing")
    p = [w]
    spot = cyl(0.11, 0.004, (0.06, 0.55, 0.014), "Pattern", verts=20)
    p.append(spot)
    p.append(cyl(0.05, 0.004, (0.06, 0.55, 0.017), "Eye", verts=16))
    p.append(cyl(0.07, 0.004, (-0.26, 0.38, 0.014), "Pattern", verts=16))
    return join(p, "SM_Deathmoth_Wing")


def m_clump():
    # Entite 5 : Clump. Amas de membres. Pivot : centre.
    blob = ico(0.5, (0, 0, 0), "Flesh", sub=3, scale=(1.0, 0.9, 0.85))
    displace(blob, 0.12, 2.5, seed=41, octaves=3)
    p = [blob]
    rnd = random.Random(5)
    for i in range(16):
        d = Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), rnd.uniform(-0.6, 1))).normalized()
        base = d * 0.38
        mid = d * 0.75 + Vector((rnd.uniform(-0.2, 0.2), rnd.uniform(-0.2, 0.2), rnd.uniform(-0.25, 0.1)))
        tip = mid + (d + Vector((0, 0, -0.9))).normalized() * rnd.uniform(0.25, 0.45)
        p.append(tube([tuple(base), tuple(mid), tuple(tip)], [0.07, 0.05, 0.035], "Skin" if i % 3 else "Flesh",
                      subdiv=1, name="limb"))
        for k in (-1, 1):
            p.append(tube([tuple(tip), tuple(tip + Vector((k * 0.03, 0.03, -0.08)))], [0.012, 0.006], "Skin",
                          subdiv=0, name="finger"))
    return join(p, "SM_Clump")


# ---------------------------------------------------------------------------
# Liste des modeles
# ---------------------------------------------------------------------------
MODELS = {
    "SM_LightPanel": m_light_panel, "SM_SkyPanel": m_sky_panel, "SM_LightTube": m_light_tube,
    "SM_LightBulb": m_light_bulb, "SM_Sconce": m_sconce, "SM_StreetLamp": m_street_lamp,
    "SM_AlmondWater": m_almond_water, "SM_Battery": m_battery, "SM_Note": m_note, "SM_Flashlight": m_flashlight,
    "SM_Crate": m_crate, "SM_Pipe": m_pipe, "SM_ElectricBox": m_electric_box, "SM_Desk": m_desk,
    "SM_OfficeChair": m_office_chair, "SM_WaterCooler": m_water_cooler, "SM_Partition": m_partition,
    "SM_ExitDoor": m_exit_door, "SM_HotelDoor": m_hotel_door, "SM_ElevatorDoor": m_elevator, "SM_Ladder": m_ladder,
    "SM_Barn": m_barn, "SM_House": m_house, "SM_PowerPole": m_power_pole, "SM_Wheat": m_wheat, "SM_Rock": m_rock,
    "SM_Smiler": m_smiler, "SM_Hound_Body": m_hound_body, "SM_Hound_Leg": m_hound_leg,
    "SM_Deathmoth_Body": m_moth_body, "SM_Deathmoth_Wing": m_moth_wing, "SM_Clump": m_clump,
}
for _k in HUMANOIDS:
    MODELS[f"SM_{_k}_Torso"] = (lambda k: lambda: m_humanoid_torso(k))(_k)
    MODELS[f"SM_{_k}_Arm"] = (lambda k: lambda: m_humanoid_arm(k))(_k)
    MODELS[f"SM_{_k}_Leg"] = (lambda k: lambda: m_humanoid_leg(k))(_k)

ORGANIC = ("Smiler", "Hound", "Faceling", "SkinStealer", "Wretch", "Partygoer", "Deathmoth", "Clump", "Rock")


# ---------------------------------------------------------------------------
# Scene, export, apercu
# ---------------------------------------------------------------------------
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    _mats.clear()


def export_fbx(o, name):
    os.makedirs(OUT_MESH, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    path = os.path.join(OUT_MESH, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, use_mesh_modifiers=True,
                             mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False, apply_unit_scale=True,
                             axis_forward="-Z", axis_up="Y", path_mode="STRIP")
    return path


def render_preview(o, name):
    os.makedirs(OUT_PREV, exist_ok=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 24
    try:
        sc.cycles.use_denoising = False
    except Exception:
        pass
    sc.render.resolution_x = sc.render.resolution_y = 256
    sc.render.film_transparent = False
    world = bpy.data.worlds.new("W")
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    bg.inputs[0].default_value = (0.35, 0.35, 0.38, 1)
    bg.inputs[1].default_value = 0.8
    sc.world = world
    bb = [o.matrix_world @ Vector(c) for c in o.bound_box]
    mn = Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
    mx = Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
    ctr = (mn + mx) / 2
    rad = max((mx - mn).length / 2, 0.05)
    cam_data = bpy.data.cameras.new("C")
    cam_data.lens = 50
    cam = bpy.data.objects.new("C", cam_data)
    sc.collection.objects.link(cam)
    dirv = Vector((1.0, -0.75, 0.55)).normalized()
    cam.location = ctr + dirv * rad * 3.3
    cam.rotation_euler = (ctr - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc.camera = cam
    ld = bpy.data.lights.new("L", "SUN")
    ld.energy = 3.0
    lo = bpy.data.objects.new("L", ld)
    lo.rotation_euler = (math.radians(50), math.radians(10), math.radians(30))
    sc.collection.objects.link(lo)
    sc.render.filepath = os.path.join(OUT_PREV, name + ".png")
    bpy.ops.render.render(write_still=True)


def build(name, preview=True):
    reset()
    o = MODELS[name]()
    organic = any(k in name for k in ORGANIC)
    finalize(o, smooth_angle=70 if organic else 35, all_smooth=organic and "Rock" not in name)
    export_fbx(o, name)
    tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
    slots = [m.name for m in o.data.materials]
    print(f"  {name:24s} {tris:6d} tris  slots={slots}")
    if preview:
        render_preview(o, name)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    preview = "--no-preview" not in argv
    names = [a for a in argv if not a.startswith("--")] or list(MODELS.keys())
    print("Export FBX ->", OUT_MESH)
    for n in names:
        build(n, preview)
    print("Termine :", len(names), "modeles")


if __name__ == "__main__":
    main()
