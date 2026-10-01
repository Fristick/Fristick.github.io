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
OUT_ICON = os.path.normpath(os.path.join(HERE, "..", "..", "RawAssets", "Icons"))

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
    "Hazmat": (0.95, 0.75, 0.06, 0), "Visor": (0.02, 0.03, 0.04, 0), "Glove": (0.9, 0.72, 0.08, 0), "Wire": (0.012, 0.012, 0.014, 0),
    "Claw": (0.06, 0.05, 0.04, 0), "Balloon": (0.75, 0.03, 0.03, 0), "String": (0.9, 0.9, 0.9, 0), "Sucker": (0.98, 0.9, 0.45, 0),
    "Gauze": (0.95, 0.94, 0.9, 0), "Wrapper": (0.8, 0.2, 0.08, 0), "Reel": (0.9, 0.9, 0.9, 0), "Reflective": (0.85, 0.85, 0.8, 0),
    "Vest": (0.16, 0.2, 0.13, 0), "Mouth": (0.12, 0.02, 0.02, 0), "FleshGlass": (0.85, 0.55, 0.5, 0), "Vein": (0.45, 0.05, 0.08, 0),
    "Lens": (0.04, 0.07, 0.1, 0), "Strap": (0.05, 0.05, 0.05, 0), "Shoe": (0.06, 0.05, 0.05, 0), "Belt": (0.1, 0.07, 0.05, 0),
    "Water": (0.2, 0.5, 0.55, 0),
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
# OBJETS D'INVENTAIRE & DETAILS (v2)
# ---------------------------------------------------------------------------
def m_camcorder():
    # Camescope des annees 90. Pivot : poignee ; objectif vers +X.
    p = [box((0.22, 0.085, 0.11), (0.0, 0, 0.0), "BlackPlastic", bevel=0.012),
         box((0.14, 0.087, 0.03), (-0.02, 0, 0.055), "DarkMetal", bevel=0.006),
         cyl(0.038, 0.07, (0.14, 0, 0.005), "BlackPlastic", rot=(0, 90, 0), verts=24),
         cyl(0.041, 0.02, (0.18, 0, 0.005), "Rubber", rot=(0, 90, 0), verts=24),
         cyl(0.03, 0.004, (0.19, 0, 0.005), "Lens", rot=(0, 90, 0), verts=24),
         cyl(0.016, 0.075, (-0.12, -0.02, 0.07), "BlackPlastic", rot=(0, 90, 0), verts=12),
         cyl(0.02, 0.02, (-0.165, -0.02, 0.07), "Rubber", rot=(0, 90, 0), verts=12),
         box((0.006, 0.006, 0.006), (0.08, -0.04, 0.05), "GlowRed"),
         box((0.12, 0.012, 0.05), (-0.01, 0.05, -0.01), "Strap", bevel=0.004),
         box((0.05, 0.002, 0.014), (0.03, -0.044, 0.02), "Label")]
    for i in range(4):
        p.append(box((0.012, 0.004, 0.008), (-0.06 + i * 0.018, -0.044, -0.03), "DarkMetal"))
    return join(p, "SM_Camcorder")


def m_vhs():
    # Cassette VHS. Pivot : centre, posee a plat.
    p = [box((0.187, 0.103, 0.025), (0, 0, 0.0125), "BlackPlastic", bevel=0.002),
         box((0.12, 0.06, 0.001), (0.0, 0.012, 0.0255), "Label"),
         box((0.18, 0.001, 0.018), (0, -0.0517, 0.0125), "Label")]
    for x in (-0.042, 0.042):
        p.append(cyl(0.021, 0.002, (x, -0.022, 0.0255), "Lens", verts=20))
        p.append(cyl(0.012, 0.003, (x, -0.022, 0.025), "Reel", verts=12))
    return join(p, "SM_VHSTape")


def m_bandage():
    # Rouleau de bande de gaze avec une bande deroulee. Pivot : sol.
    p = [cyl(0.032, 0.055, (0, 0, 0.032), "Gauze", rot=(90, 0, 0), verts=24),
         cyl(0.011, 0.056, (0, 0, 0.032), "Tape", rot=(90, 0, 0), verts=12)]
    strip = box((0.16, 0.054, 0.002), (0.09, 0, 0.001), "Gauze")
    p.append(strip)
    return join(p, "SM_Bandage")


def m_energy_bar():
    p = [box((0.13, 0.04, 0.016), (0, 0, 0.008), "Wrapper", bevel=0.004),
         box((0.014, 0.042, 0.004), (0.068, 0, 0.008), "Wrapper"),
         box((0.014, 0.042, 0.004), (-0.068, 0, 0.008), "Wrapper"),
         box((0.06, 0.041, 0.017), (0.0, 0, 0.008), "Label")]
    return join(p, "SM_EnergyBar")


def m_headlamp():
    p = [torus(0.085, 0.012, (0, 0, 0.012), "Strap", seg=32, mseg=4),
         box((0.035, 0.06, 0.04), (0.09, 0, 0.03), "BlackPlastic", bevel=0.006),
         cyl(0.016, 0.004, (0.108, 0, 0.03), "Glow", rot=(0, 90, 0), verts=16)]
    p[0].scale = (1.0, 1.0, 0.4)
    return join(p, "SM_Headlamp")


def m_vest():
    # Gilet de protection. Pivot : bas, face avant vers +X.
    p = [box((0.07, 0.4, 0.5), (0.06, 0, 0.25), "Vest", bevel=0.03),
         box((0.07, 0.4, 0.5), (-0.06, 0, 0.25), "Vest", bevel=0.03)]
    for y in (-0.13, 0.13):
        p.append(box((0.19, 0.08, 0.04), (0.0, y, 0.5), "Vest", bevel=0.015))
    for z in (0.12, 0.3):
        p.append(box((0.075, 0.41, 0.03), (0.065, 0, z), "Reflective"))
    for y in (-0.1, 0.1):
        p.append(box((0.03, 0.12, 0.1), (0.105, y, 0.2), "Vest", bevel=0.01))
    p.append(box((0.08, 0.16, 0.12), (0.0, 0, 0.47), "Dark"))
    return join(p, "SM_Vest")


def m_outlet():
    # Prise murale. Pivot : contre le mur, face vers +X.
    p = [box((0.008, 0.07, 0.115), (0.004, 0, 0), "Plastic", bevel=0.002)]
    for z in (-0.025, 0.025):
        p.append(cyl(0.017, 0.004, (0.009, 0, z), "Plastic", rot=(0, 90, 0), verts=16))
        for y in (-0.006, 0.006):
            p.append(box((0.003, 0.003, 0.009), (0.011, y, z), "Dark"))
    return join(p, "SM_Outlet")


def m_vent():
    # Grille d'aeration murale. Pivot : contre le mur, face vers +X.
    p = [box((0.02, 0.45, 0.3), (0.01, 0, 0), "Metal", bevel=0.004),
         box((0.005, 0.4, 0.25), (0.021, 0, 0), "Dark")]
    for i in range(8):
        z = -0.105 + i * 0.03
        s = box((0.03, 0.41, 0.006), (0.022, 0, z), "Metal")
        s.rotation_euler = (0, math.radians(35), 0)
        p.append(s)
    return join(p, "SM_Vent")


def m_water_grid():
    # Plan d'eau 1 x 1 m subdivise (vagues par World Position Offset). Pivot : centre.
    bpy.ops.mesh.primitive_grid_add(x_subdivisions=40, y_subdivisions=40, size=1.0, location=(0, 0, 0))
    o = bpy.context.active_object
    o.data.materials.append(mat("Water"))
    return join([o], "SM_WaterGrid")


def m_flashlight_fp():
    # Lampe torche tenue par un gant (vue a la premiere personne). Pivot : poignee.
    fl = m_flashlight()
    fl.name = "fl"
    glove = make_glove(grip_radius=0.022)
    return join([fl, glove], "SM_Flashlight_FP")


def m_camcorder_fp():
    cam = m_camcorder()
    cam.name = "cam"
    glove = make_glove(grip_radius=0.05, z=-0.005)
    return join([cam, glove], "SM_Camcorder_FP")


def make_glove(grip_radius=0.03, z=0.0):
    """Main gantee (combinaison hazmat) qui empoigne un objet le long de l'axe X."""
    p = [tube([(-0.11, 0.0, z - 0.02), (-0.04, 0.0, z - grip_radius * 0.6)], [(0.045, 0.04), (0.042, 0.032)], "Glove", subdiv=2, name="palm"),
         tube([(-0.2, 0.0, z - 0.06), (-0.11, 0.0, z - 0.03)], [0.048, 0.044], "Glove", subdiv=1, name="cuff")]
    for i in range(4):
        x = -0.07 + i * 0.022
        pts = [(x, 0.03, z - grip_radius * 0.5), (x, 0.03 + grip_radius * 0.6, z - grip_radius * 1.2),
               (x + 0.005, 0.0, z - grip_radius * 1.45), (x + 0.008, -grip_radius * 0.8, z - grip_radius * 1.1)]
        p.append(tube(pts, [0.011, 0.01, 0.0095, 0.009], "Glove", subdiv=1, name="finger"))
    p.append(tube([(-0.05, -0.03, z - 0.01), (-0.02, -0.045, z + grip_radius * 0.6), (0.0, -0.035, z + grip_radius * 1.1)],
                  [0.013, 0.011, 0.01], "Glove", subdiv=1, name="thumb"))
    return join(p, "glove")


# ---------------------------------------------------------------------------
# ENTITES (v2) : membres en deux segments, tete separee (elle suit le joueur)
# Les decalages des articulations sont repris a l'identique dans BREntity.cpp
# ---------------------------------------------------------------------------
HUMANOIDS = {
    "Faceling":    dict(hip=0.92, sh=1.45, sw=0.19, hw=0.10, hunch=0.00, ua=0.30, la=0.42, th=0.46, head=0.115, neck=0.06,
                        r_arm=0.042, r_leg=0.065, torso="Cloth", arm_u="Cloth", arm_l="Skin", leg_u="Pants", leg_l="Pants",
                        foot="Shoe", hand="Skin"),
    "SkinStealer": dict(hip=1.15, sh=1.85, sw=0.22, hw=0.11, hunch=0.10, ua=0.45, la=0.62, th=0.58, head=0.135, neck=0.10,
                        r_arm=0.05, r_leg=0.072, torso="Hazmat", arm_u="Hazmat", arm_l="Flesh", leg_u="Hazmat", leg_l="Hazmat",
                        foot="Rubber", hand="Flesh"),
    "Wretch":      dict(hip=0.82, sh=1.22, sw=0.17, hw=0.09, hunch=0.30, ua=0.33, la=0.47, th=0.41, head=0.10, neck=0.05,
                        r_arm=0.03, r_leg=0.045, torso="Skin", arm_u="Skin", arm_l="Skin", leg_u="Skin", leg_l="Skin",
                        foot="Skin", hand="Skin"),
    "Partygoer":   dict(hip=0.85, sh=1.38, sw=0.22, hw=0.11, hunch=0.00, ua=0.28, la=0.40, th=0.43, head=0.17, neck=0.04,
                        r_arm=0.05, r_leg=0.075, torso="Party", arm_u="Party", arm_l="Party", leg_u="Party", leg_l="Party",
                        foot="Party", hand="Party"),
    "Bacteria":    dict(hip=1.40, sh=2.15, sw=0.19, hw=0.08, hunch=0.10, ua=0.62, la=0.82, th=0.70, head=0.10, neck=0.16,
                        r_arm=0.022, r_leg=0.03, torso="Wire", arm_u="Wire", arm_l="Wire", leg_u="Wire", leg_l="Wire",
                        foot="Wire", hand="Wire"),
}

HOUND = dict(body_z=0.70, front=(0.34, 0.15), back=(-0.32, 0.14), head=(0.42, 0.0, 0.09), upper=0.36, lower=0.40)


def wiry(points, radius, material, strands=3, twist=1.5, name="wire"):
    """Faisceau de cables torsades (aspect 'fil de fer' de la Bacteria)"""
    out = []
    pts = [Vector(pt) for pt in points]
    for k in range(strands):
        ph = k * 2 * math.pi / strands
        res = []
        n = len(pts)
        for i in range(n):
            a = ph + twist * i
            off = Vector((0, math.cos(a), math.sin(a))) * radius * 0.9
            res.append(tuple(pts[i] + off))
        out.append(tube(res, [radius * 0.75] * n, material, subdiv=1, name=name))
    return out


def m_torso(kind):
    s = HUMANOIDS[kind]
    top = s["sh"] - s["hip"]
    hx = s["hunch"]
    neck_top = Vector((hx * 1.05, 0, top + s["neck"]))
    p = []
    if kind == "Bacteria":
        pts = [(0, 0, 0), (hx * 0.3, 0, top * 0.35), (hx * 0.7, 0, top * 0.7), (hx, 0, top), tuple(neck_top)]
        p += wiry(pts, 0.03, "Wire", strands=4, twist=1.2)
        for k in range(5):  # cotes en fil de fer
            z = top * (0.45 + k * 0.1)
            p.append(torus(0.075, 0.006, (hx * 0.6, 0, z), "Wire", rot=(0, 75, 0), mseg=4, seg=12))
        p.append(tube([(hx, -s["sw"], top - 0.02), (hx, 0, top + 0.02), (hx, s["sw"], top - 0.02)], [0.022, 0.03, 0.022], "Wire", subdiv=1, name="sh"))
        p.append(tube([(0, -s["hw"] - 0.02, 0), (0, s["hw"] + 0.02, 0)], [0.03, 0.03], "Wire", subdiv=1, name="hip"))
        return join(p, f"SM_{kind}_Torso")

    chest = {"Wretch": (0.12, 0.085), "Partygoer": (0.19, 0.14), "SkinStealer": (0.2, 0.15)}.get(kind, (0.16, 0.11))
    belly = {"Wretch": (0.09, 0.07), "Partygoer": (0.2, 0.16), "SkinStealer": (0.17, 0.13)}.get(kind, (0.13, 0.09))
    pelvis = {"Partygoer": (0.17, 0.13), "Wretch": (0.1, 0.08), "SkinStealer": (0.16, 0.12)}.get(kind, (0.13, 0.1))
    torso = tube([(0, 0, 0), (hx * 0.3, 0, top * 0.4), (hx * 0.8, 0, top - 0.06), tuple(neck_top)],
                 [pelvis, belly, chest, (0.05, 0.05)], s["torso"], subdiv=2, name="torso")
    p.append(torso)
    p.append(tube([(hx * 0.85, -s["sw"] + 0.03, top - 0.02), (hx * 0.85, 0, top + 0.01), (hx * 0.85, s["sw"] - 0.03, top - 0.02)],
                  [s["r_arm"] * 1.35, s["r_arm"] * 1.8, s["r_arm"] * 1.35], s["torso"], subdiv=2, name="shoulders"))
    if kind == "Faceling":
        p.append(torus(0.06, 0.012, (neck_top.x, 0, neck_top.z - 0.04), "Cloth", mseg=6))
        p.append(torus(0.135, 0.015, (0, 0, 0.02), "Belt", mseg=4, seg=24))
        p[-1].scale = (1.0, 0.75, 1.0)
    elif kind == "Wretch":
        displace(torso, 0.01, 14.0, seed=21)
        for i in range(6):
            z = top * 0.38 + i * 0.045
            r = torus(0.085, 0.007, (hx * 0.55 + 0.03, 0, z), "Skin", rot=(0, 72, 0), mseg=4, seg=16)
            r.scale = (1.0, 1.15, 0.6)
            p.append(r)
        for i in range(8):
            t = i / 7
            p.append(sphere(0.016, (hx * t * 0.9 - 0.07, 0, top * t), "Skin", seg=8, rings=4))
    elif kind == "SkinStealer":
        displace(torso, 0.02, 6.0, seed=12)
        p.append(box((0.01, 0.02, top * 0.9), (chest[0] * 0.95 + hx * 0.4, 0, top * 0.45), "Rubber"))
        for k in range(4):  # chair qui deborde des dechirures de la combinaison
            pos = (hx * 0.4 + 0.12, (k - 1.5) * 0.08, top * (0.3 + 0.12 * k))
            blob = ico(0.05, pos, "Flesh", sub=2)
            displace(blob, 0.015, 15.0, seed=40 + k)
            p.append(blob)
        p.append(cyl(0.05, 0.12, (hx * 0.6 - 0.16, 0.0, top * 0.6), "Rubber", verts=12))
    elif kind == "Partygoer":
        pass
    return join(p, f"SM_{kind}_Torso")


def m_head(kind):
    s = HUMANOIDS[kind]
    r = s["head"]
    c = Vector((0.01, 0, r * 0.95))
    p = []
    if kind == "Faceling":
        head = sphere(r, c, "Skin", scale=(0.92, 0.85, 1.12), seg=32, rings=16)
        p.append(head)
        hair = sphere(r * 1.04, c + Vector((-0.015, 0, 0.02)), "Hair", scale=(0.95, 0.9, 1.05), seg=24, rings=12)
        p.append(hair)
        hair_front = box((0.05, r * 1.7, r * 1.0), c + Vector((r * 0.85, 0, r * 0.35)), "Hair")
        hair_front.rotation_euler = (0, math.radians(-35), 0)
        p.append(hair_front)
        for sgn in (-1, 1):
            p.append(sphere(0.022, c + Vector((0.0, sgn * r * 0.86, -0.01)), "Skin", scale=(0.6, 0.4, 1.0), seg=10, rings=6))
        p.append(tube([(0, 0, 0), (0, 0, 0.06)], [0.045, 0.045], "Skin", subdiv=1, name="neck"))
    elif kind == "SkinStealer":
        # capuche de combinaison hazmat deformee, visiere fendue d'ou deborde la chair
        hood = sphere(r, c + Vector((0, 0, 0.03)), "Hazmat", scale=(1.0, 0.95, 1.35), seg=32, rings=16)
        displace(hood, 0.012, 7.0, seed=31)
        visor = sphere(r * 0.86, c + Vector((r * 0.32, 0, 0.06)), "Visor", scale=(0.55, 0.85, 0.75), seg=24, rings=12)
        flesh = ico(r * 0.4, c + Vector((r * 0.75, -0.02, 0.0)), "Flesh", sub=2)
        displace(flesh, 0.02, 12.0, seed=33)
        p += [hood, visor, flesh]
        # machoire beante sous la visiere
        p.append(sphere(r * 0.45, c + Vector((r * 0.65, 0, -r * 0.6)), "Mouth", scale=(0.5, 0.9, 0.9), seg=16, rings=8))
        for i in range(9):
            t = i / 8 * 2 - 1
            for zz, h in ((-r * 0.35, -0.03), (-r * 0.85, 0.03)):
                tooth = cyl(0.008, 0.035, c + Vector((r * 0.82, t * r * 0.35, zz)), "Teeth", verts=6, r2=0.0)
                tooth.rotation_euler = (0, math.radians(180 if h < 0 else 0), 0)
                p.append(tooth)
        p.append(cyl(0.035, 0.07, c + Vector((r * 0.75, 0, -r * 1.05)), "Rubber", rot=(0, 70, 0), verts=12))
    elif kind == "Wretch":
        head = sphere(r, c, "Skin", scale=(0.95, 0.8, 1.1), seg=28, rings=14)
        displace(head, 0.008, 12.0, seed=22)
        p.append(head)
        for sgn in (-1, 1):
            p.append(sphere(0.024, c + Vector((r * 0.78, sgn * 0.034, 0.02)), "Dark", scale=(0.5, 1.0, 1.2), seg=10, rings=6))
        p.append(sphere(0.03, c + Vector((r * 0.8, 0, -0.05)), "Mouth", scale=(0.5, 1.3, 1.1), seg=12, rings=6))
        for i in range(6):
            p.append(cyl(0.004, 0.016, c + Vector((r * 0.86, (i - 2.5) * 0.011, -0.035)), "Teeth", verts=5, r2=0.0, rot=(180, 0, 0)))
        rnd = random.Random(23)
        for i in range(12):
            a = rnd.uniform(-1.2, 1.2)
            top = c + Vector((-0.02, math.sin(a) * r * 0.7, r * 0.9))
            p.append(tube([tuple(top), tuple(top + Vector((-0.04, math.sin(a) * 0.05, -0.22 - rnd.random() * 0.1)))],
                          [0.004, 0.002], "Hair", subdiv=0, name="hair"))
        p.append(tube([(0, 0, 0), (0, 0, 0.05)], [0.03, 0.03], "Skin", subdiv=1, name="neck"))
    elif kind == "Partygoer":
        head = sphere(r, c + Vector((0, 0, 0.02)), "Party", scale=(0.95, 0.95, 1.0), seg=32, rings=16)
        p.append(head)
        fx = r * 0.93
        for sgn in (-1, 1):
            p.append(sphere(0.02, c + Vector((fx, sgn * 0.055, 0.06)), "Face", scale=(0.4, 0.7, 1.9), seg=12, rings=6))
        pts = []
        for i in range(13):
            t = i / 12 * 2 - 1
            yy = t * 0.11
            zz = c.z - 0.03 - 0.06 * (1 - t * t)
            xx = math.sqrt(max(0.0, (r * 0.97) ** 2 - yy ** 2 - (zz - c.z - 0.02) ** 2 * 0.9)) + 0.005
            pts.append((xx, yy, zz))
        p.append(tube(pts, [0.009] * len(pts), "Face", subdiv=1, name="smile"))
        p.append(tube([(0, 0, 0), (0, 0, 0.05)], [0.06, 0.06], "Party", subdiv=1, name="neck"))
    elif kind == "Bacteria":
        head = sphere(r, c + Vector((0.02, 0, 0.04)), "Wire", scale=(1.3, 0.75, 1.6), seg=24, rings=12)
        displace(head, 0.01, 10.0, seed=55)
        p.append(head)
        p.append(box((0.01, 0.02, 0.12), c + Vector((r * 1.25, 0, 0.0)), "Mouth"))
        p += wiry([(0, 0, 0), (0.0, 0, 0.08), (0.02, 0, 0.16)], 0.018, "Wire", strands=3)
        rnd = random.Random(56)
        for i in range(8):  # fils qui depassent du crane
            a = rnd.uniform(0, 2 * math.pi)
            base = c + Vector((math.cos(a) * r * 0.6, math.sin(a) * r * 0.4, r * 1.2))
            p.append(tube([tuple(base), tuple(base + Vector((math.cos(a) * 0.08, math.sin(a) * 0.08, 0.12)))],
                          [0.005, 0.002], "Wire", subdiv=0, name="w"))
    return join(p, f"SM_{kind}_Head")


def make_hand(kind, length):
    s = HUMANOIDS[kind]
    mat_h = s["hand"]
    z0 = -length
    p = [sphere(0.03 if kind != "Partygoer" else 0.045, (0.01, 0, z0 - 0.02), mat_h, scale=(0.6, 1.0, 1.25), seg=12, rings=6)]
    fl = {"Bacteria": 0.16, "Wretch": 0.11, "SkinStealer": 0.14}.get(kind, 0.075)
    for i in range(4):
        y = (i - 1.5) * 0.016
        tip = (0.025, y * 1.3, z0 - 0.04 - fl)
        if kind == "Partygoer":
            # doigts en tentacules avec ventouses
            pts = [(0.01, y * 1.5, z0 - 0.05), (0.03, y * 1.9, z0 - 0.1), (0.02, y * 2.2, z0 - 0.15)]
            p.append(tube(pts, [0.013, 0.01, 0.006], "Party", subdiv=1, name="tentacle"))
            for k in range(3):
                pp = Vector(pts[k]) + Vector((0.012, 0, 0))
                p.append(cyl(0.006, 0.003, tuple(pp), "Sucker", rot=(0, 90, 0), verts=10))
        else:
            p.append(tube([(0.012, y, z0 - 0.04), (0.02, y * 1.2, z0 - 0.04 - fl * 0.55), tip], [0.008, 0.007, 0.005], mat_h, subdiv=1, name="f"))
            if kind in ("Wretch", "SkinStealer", "Bacteria"):
                cl = cyl(0.005, 0.03, (tip[0] + 0.004, tip[1], tip[2] - 0.012), "Claw", verts=6, r2=0.0, rot=(180, 0, 0))
                p.append(cl)
    if kind != "Partygoer":
        p.append(tube([(0.01, -0.03, z0 - 0.02), (0.03, -0.04, z0 - 0.06)], [0.008, 0.006], mat_h, subdiv=1, name="thumb"))
    return p


def m_upper_arm(kind):
    s = HUMANOIDS[kind]
    r = s["r_arm"]
    if kind == "Bacteria":
        return join(wiry([(0, 0, 0), (0, 0, -s["ua"] * 0.5), (0, 0, -s["ua"])], r, "Wire"), f"SM_{kind}_UpperArm")
    a = tube([(0, 0, 0), (0.005, 0, -s["ua"] * 0.5), (0, 0, -s["ua"])], [r, r * 0.92, r * 0.8], s["arm_u"], subdiv=2, name="ua")
    if kind == "SkinStealer":
        displace(a, 0.01, 9.0, seed=61)
    return join([a], f"SM_{kind}_UpperArm")


def m_lower_arm(kind):
    s = HUMANOIDS[kind]
    r = s["r_arm"] * 0.85
    L = s["la"] - 0.06
    if kind == "Bacteria":
        p = wiry([(0, 0, 0), (0.01, 0, -L * 0.5), (0.02, 0, -L)], r, "Wire")
    else:
        p = [tube([(0, 0, 0), (0.01, 0, -L * 0.5), (0.02, 0, -L)], [r, r * 0.85, r * 0.65], s["arm_l"], subdiv=2, name="la")]
    if kind == "Faceling":
        p.append(torus(r * 1.05, 0.006, (0, 0, -0.02), "Cloth", mseg=4))
    p += make_hand(kind, L)
    return join(p, f"SM_{kind}_LowerArm")


def m_thigh(kind):
    s = HUMANOIDS[kind]
    r = s["r_leg"]
    if kind == "Bacteria":
        return join(wiry([(0, 0, 0), (0.01, 0, -s["th"] * 0.5), (0, 0, -s["th"])], r, "Wire"), f"SM_{kind}_Thigh")
    t = tube([(0, 0, 0), (0.015, 0, -s["th"] * 0.5), (0, 0, -s["th"])], [r, r * 0.88, r * 0.72], s["leg_u"], subdiv=2, name="th")
    return join([t], f"SM_{kind}_Thigh")


def m_shin(kind):
    s = HUMANOIDS[kind]
    r = s["r_leg"] * 0.72
    sn = s["hip"] - s["th"]
    L = sn - 0.05
    if kind == "Bacteria":
        p = wiry([(0, 0, 0), (-0.01, 0, -L * 0.5), (0, 0, -L)], r, "Wire")
        for k in (-1, 0, 1):
            p.append(tube([(0, 0, -L), (0.1, k * 0.03, -sn + 0.01), (0.16, k * 0.04, -sn + 0.005)], [0.012, 0.008, 0.003], "Wire", subdiv=1, name="toe"))
        return join(p, f"SM_{kind}_Shin")
    p = [tube([(0, 0, 0), (-0.01, 0, -L * 0.5), (0, 0, -L)], [r, r * 0.85, r * 0.6], s["leg_l"], subdiv=2, name="sh")]
    foot_mat = s["foot"]
    if kind in ("Wretch",):
        for k in (-1, 0, 1):
            p.append(tube([(0, 0, -L), (0.08, k * 0.025, -sn + 0.012), (0.12, k * 0.03, -sn + 0.005)], [0.014, 0.01, 0.005], "Skin", subdiv=1, name="toe"))
    else:
        p.append(box((0.24 if kind != "Partygoer" else 0.2, 0.095, 0.07), (0.06, 0, -sn + 0.035), foot_mat, bevel=0.025))
    return join(p, f"SM_{kind}_Shin")


def m_smiler():
    # Entite 3 : Smilers. Masse de fumee noire, yeux et sourire demesure lumineux.
    rnd = random.Random(9)
    p = []
    for i in range(5):
        c = (rnd.uniform(-0.12, 0.05), rnd.uniform(-0.25, 0.25), rnd.uniform(-0.25, 0.3))
        b = ico(rnd.uniform(0.25, 0.42), c, "Body", sub=3, scale=(0.6, 1.0, 1.0))
        displace(b, 0.08, 3.0, seed=10 + i, octaves=3)
        p.append(b)
    fx = 0.3
    for sgn in (-1, 1):
        p.append(sphere(0.085, (fx, sgn * 0.17, 0.2), "Glow", scale=(0.3, 1.0, 0.42), seg=16, rings=8, rot=(sgn * 24, 0, 0)))
    # bouche : interieur sombre + deux rangees de crocs coniques
    p.append(sphere(0.22, (fx - 0.06, 0, -0.1), "Dark", scale=(0.3, 1.7, 0.5), seg=20, rings=10))
    n = 25
    for i in range(n):
        t = (i / (n - 1)) * 2 - 1
        y = t * 0.36
        zu = -0.05 + 0.14 * t * t
        gap = 0.1 * (1 - t * t) + 0.012
        x = fx + 0.02 - 0.09 * t * t
        h = 0.07 * (1 - 0.5 * abs(t)) + 0.02
        up = cyl(0.016, h, (x, y, zu - h * 0.5), "Glow", verts=6, r2=0.0, rot=(180, 0, 0))
        lo = cyl(0.016, h * 0.9, (x, y, zu - gap + h * 0.45), "Glow", verts=6, r2=0.0)
        p += [up, lo]
    return join(p, "SM_Smiler")


def m_hound_body():
    # Entite 8 : Hounds. Torse decharne a l'horizontale (tete et pattes separees).
    torso = tube([(-0.38, 0, 0), (-0.12, 0, 0.03), (0.2, 0, 0.05), (0.42, 0, 0.09)],
                 [(0.12, 0.1), (0.1, 0.085), (0.15, 0.12), (0.06, 0.06)], "Skin", subdiv=2, name="torso")
    displace(torso, 0.01, 11.0, seed=4)
    p = [torso]
    for i in range(9):
        x = -0.33 + i * 0.085
        p.append(sphere(0.022, (x, 0, 0.1 + 0.02 * math.sin(i * 0.7)), "Skin", seg=8, rings=4))
    for i in range(5):
        r = torus(0.12, 0.008, (0.05 + i * 0.045, 0, 0.03), "Skin", rot=(0, 90, 0), mseg=4, seg=16)
        r.scale = (1.0, 1.05, 0.85)
        p.append(r)
    return join(p, "SM_Hound_Body")


def m_hound_head():
    # Tete : pivot au cou ; machoire beante cachee sous de longs cheveux noirs.
    p = [sphere(0.11, (0.1, 0, 0.0), "Skin", scale=(1.25, 0.88, 0.95), seg=24, rings=12),
         tube([(-0.04, 0, -0.02), (0.06, 0, 0.0)], [0.05, 0.055], "Skin", subdiv=1, name="neck"),
         sphere(0.06, (0.2, 0, -0.06), "Mouth", scale=(0.9, 1.0, 0.6), seg=16, rings=8)]
    for i in range(10):
        t = i / 9 * 2 - 1
        p.append(cyl(0.006, 0.03, (0.235, t * 0.045, -0.035), "Teeth", verts=5, r2=0.0, rot=(180, 0, 0)))
        p.append(cyl(0.006, 0.026, (0.23, t * 0.04, -0.09), "Teeth", verts=5, r2=0.0))
    rnd = random.Random(8)
    for i in range(44):
        a = rnd.uniform(-1.5, 1.5)
        ox = 0.1 + rnd.uniform(-0.1, 0.1)
        y = math.sin(a) * 0.1
        top = (ox, y, 0.1 + rnd.uniform(-0.02, 0.02))
        ln = rnd.uniform(0.3, 0.55)
        bot = (ox + 0.16 + rnd.uniform(-0.03, 0.05), y * 1.3, top[2] - ln)
        p.append(tube([top, ((top[0] + bot[0]) / 2 + 0.04, (top[1] + bot[1]) / 2, (top[2] + bot[2]) / 2), bot],
                      [0.011, 0.008, 0.002], "Hair", subdiv=0, name="hair"))
    return join(p, "SM_Hound_Head")


def m_hound_upper():
    L = HOUND["upper"]
    t = tube([(0, 0, 0), (0.0, 0, -L * 0.5), (0, 0, -L)], [0.055, 0.045, 0.038], "Skin", subdiv=2, name="u")
    return join([t], "SM_Hound_UpperLeg")


def m_hound_lower():
    L = HOUND["lower"]
    p = [tube([(0, 0, 0), (0.02, 0, -L * 0.6), (0.0, 0, -L + 0.04)], [0.036, 0.028, 0.025], "Skin", subdiv=2, name="l"),
         sphere(0.03, (0.03, 0, -L + 0.02), "Skin", scale=(1.5, 1.1, 0.6), seg=12, rings=6)]
    for k in (-1, 0, 1):
        tip = (0.15, k * 0.03, -L + 0.004)
        p.append(tube([(0.04, k * 0.012, -L + 0.02), (0.1, k * 0.025, -L + 0.02), tip], [0.01, 0.008, 0.005], "Skin", subdiv=1, name="f"))
        p.append(cyl(0.006, 0.04, (tip[0] + 0.02, tip[1], tip[2]), "Claw", verts=6, r2=0.0, rot=(0, 90, 0)))
    return join(p, "SM_Hound_LowerLeg")


def m_moth_body():
    # Entite 4 : Deathmoths. Pivot : centre du thorax ; tete vers +X.
    body = tube([(-0.36, 0, -0.02), (-0.12, 0, 0.0), (0.05, 0, 0.02), (0.16, 0, 0.03)],
                [(0.06, 0.06), (0.09, 0.085), (0.1, 0.09), (0.06, 0.06)], "Fur", subdiv=2, name="body")
    displace(body, 0.014, 22.0, seed=31)
    p = [body]
    for i in range(6):  # anneaux de l'abdomen
        r = torus(0.08 - i * 0.008, 0.008, (-0.3 + i * 0.05, 0, -0.01), "Fur", rot=(0, 90, 0), mseg=4, seg=16)
        p.append(r)
    for s in (-1, 1):
        p.append(sphere(0.038, (0.18, s * 0.045, 0.045), "Eye", seg=12, rings=6))
        p.append(tube([(0.2, s * 0.03, 0.08), (0.32, s * 0.12, 0.24), (0.38, s * 0.2, 0.32)], [0.008, 0.012, 0.003],
                      "Fur", subdiv=1, name="antenna"))
        for k in range(3):
            x0 = 0.08 - k * 0.08
            p.append(tube([(x0, s * 0.05, -0.05), (x0 + 0.04, s * 0.2, -0.1), (x0 + 0.06, s * 0.25, -0.26)],
                          [0.01, 0.008, 0.003], "DarkMetal", subdiv=1, name="leg"))
    return join(p, "SM_Deathmoth_Body")


def m_moth_wing():
    # Aile : racine a l'origine, s'etend vers +Y. Le jeu la mire pour l'autre cote.
    outline = [(0.16, 0.0), (0.32, 0.3), (0.34, 0.62), (0.18, 0.86), (0.02, 0.9), (-0.12, 0.7),
               (-0.3, 0.6), (-0.44, 0.36), (-0.36, 0.12), (-0.18, 0.0)]
    w = poly_plate(outline, 0.008, "Wing")
    p = [w]
    for (x1, y1) in [(0.3, 0.55), (0.12, 0.85), (-0.15, 0.68), (-0.4, 0.38), (0.25, 0.25)]:
        p.append(tube([(0.0, 0.02, 0.009), (x1 * 0.5, y1 * 0.5, 0.01), (x1 * 0.95, y1 * 0.95, 0.009)], [0.006, 0.004, 0.002],
                      "Pattern", subdiv=0, name="vein"))
    p.append(cyl(0.11, 0.004, (0.06, 0.55, 0.011), "Pattern", verts=24))
    p.append(cyl(0.07, 0.004, (0.06, 0.55, 0.014), "Wing", verts=20))
    p.append(cyl(0.04, 0.004, (0.06, 0.55, 0.017), "Eye", verts=16))
    p.append(cyl(0.07, 0.004, (-0.26, 0.38, 0.011), "Pattern", verts=16))
    return join(p, "SM_Deathmoth_Wing")


def m_skinstealer_mass():
    # Forme "au repos" du Skin-Stealer : masse de chair translucide et palpitante.
    blob = ico(0.55, (0, 0, 0.45), "FleshGlass", sub=4, scale=(1.1, 0.9, 0.85))
    displace(blob, 0.16, 2.2, seed=71, octaves=3)
    p = [blob]
    rnd = random.Random(72)
    for i in range(14):  # veines
        a = rnd.uniform(0, 2 * math.pi)
        z = rnd.uniform(0.2, 0.75)
        pts = []
        for k in range(4):
            aa = a + k * 0.25
            pts.append((math.cos(aa) * 0.6, math.sin(aa) * 0.5, z + k * 0.04 * rnd.uniform(-1, 1)))
        p.append(tube(pts, [0.012, 0.01, 0.008, 0.005], "Vein", subdiv=0, name="vein"))
    for i in range(5):  # membres a moitie formes
        d = Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), rnd.uniform(-0.2, 0.6))).normalized()
        base = Vector((0, 0, 0.45)) + d * 0.45
        tip = base + d * 0.3 + Vector((0, 0, -0.2))
        p.append(tube([tuple(base), tuple(tip)], [0.06, 0.03], "Flesh", subdiv=1, name="limb"))
    return join(p, "SM_SkinStealer_Mass")


def m_balloon():
    # Ballon rouge du Partygoer : pivot a la main, le ballon flotte 1 m plus haut.
    p = [tube([(0, 0, 0), (0.05, 0.04, 0.7), (0.12, 0.1, 1.4)], [0.002, 0.002, 0.002], "String", subdiv=0, name="string"),
         sphere(0.16, (0.12, 0.1, 1.58), "Balloon", scale=(0.95, 0.95, 1.15), seg=24, rings=12),
         cyl(0.015, 0.03, (0.12, 0.1, 1.4), "Balloon", verts=8, r2=0.004)]
    return join(p, "SM_Partygoer_Balloon")


def m_hazmat():
    # Explorateur en combinaison hazmat (silhouette de l'inventaire). Pivot : au sol.
    p = [tube([(0, 0, 0.95), (0, 0, 1.15), (0, 0, 1.38), (0.01, 0, 1.5)], [(0.17, 0.13), (0.18, 0.14), (0.21, 0.15), (0.08, 0.08)],
              "Hazmat", subdiv=2, name="torso"),
         sphere(0.15, (0.02, 0, 1.66), "Hazmat", scale=(1.0, 0.95, 1.12), seg=32, rings=16),
         sphere(0.13, (0.07, 0, 1.67), "Visor", scale=(0.6, 0.85, 0.7), seg=24, rings=12),
         cyl(0.04, 0.08, (0.15, 0, 1.56), "Rubber", rot=(0, 70, 0), verts=16),
         cyl(0.05, 0.14, (-0.12, 0, 1.25), "Rubber", verts=16),
         torus(0.18, 0.02, (0, 0, 0.98), "Rubber", mseg=6, seg=32)]
    p[-1].scale = (1.0, 0.8, 1.0)
    for sgn in (-1, 1):
        p.append(tube([(0, sgn * 0.2, 1.42), (0.03, sgn * 0.27, 1.15), (0.06, sgn * 0.29, 0.92)], [0.065, 0.06, 0.055], "Hazmat", subdiv=2, name="arm"))
        p.append(sphere(0.055, (0.07, sgn * 0.29, 0.86), "Rubber", scale=(0.8, 0.7, 1.2), seg=12, rings=6))
        p.append(tube([(0, sgn * 0.1, 0.95), (0.02, sgn * 0.11, 0.5), (0, sgn * 0.11, 0.12)], [0.085, 0.075, 0.065], "Hazmat", subdiv=2, name="leg"))
        p.append(box((0.26, 0.11, 0.12), (0.04, sgn * 0.11, 0.06), "Rubber", bevel=0.03))
    return join(p, "SM_Hazmat")


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
    # objets v2
    "SM_Camcorder": m_camcorder, "SM_VHSTape": m_vhs, "SM_Bandage": m_bandage, "SM_EnergyBar": m_energy_bar,
    "SM_Headlamp": m_headlamp, "SM_Vest": m_vest, "SM_Outlet": m_outlet, "SM_Vent": m_vent, "SM_WaterGrid": m_water_grid,
    "SM_Flashlight_FP": m_flashlight_fp, "SM_Camcorder_FP": m_camcorder_fp, "SM_Hazmat": m_hazmat,
    # entites
    "SM_Smiler": m_smiler, "SM_Hound_Body": m_hound_body, "SM_Hound_Head": m_hound_head,
    "SM_Hound_UpperLeg": m_hound_upper, "SM_Hound_LowerLeg": m_hound_lower,
    "SM_Deathmoth_Body": m_moth_body, "SM_Deathmoth_Wing": m_moth_wing, "SM_Clump": m_clump,
    "SM_SkinStealer_Mass": m_skinstealer_mass, "SM_Partygoer_Balloon": m_balloon,
}
for _k in HUMANOIDS:
    for _part, _fn in (("Torso", m_torso), ("Head", m_head), ("UpperArm", m_upper_arm), ("LowerArm", m_lower_arm),
                       ("Thigh", m_thigh), ("Shin", m_shin)):
        MODELS[f"SM_{_k}_{_part}"] = (lambda k, fn: lambda: fn(k))(_k, _fn)

ORGANIC = ("Smiler", "Hound", "Faceling", "SkinStealer", "Wretch", "Partygoer", "Deathmoth", "Clump", "Rock", "Bacteria",
           "Hazmat", "_FP", "Vest")

# Objets d'inventaire : icone rendue (fond transparent) -> RawAssets/Icons/I_<Nom>.png
ICONS = {
    "AlmondWater": "SM_AlmondWater", "Bandage": "SM_Bandage", "Battery": "SM_Battery", "EnergyBar": "SM_EnergyBar",
    "VHSTape": "SM_VHSTape", "Flashlight": "SM_Flashlight", "Camcorder": "SM_Camcorder", "Headlamp": "SM_Headlamp",
    "Vest": "SM_Vest", "Note": "SM_Note",
}


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


def _setup_render(size, transparent):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 32
    try:
        sc.cycles.use_denoising = False
    except Exception:
        pass
    sc.render.resolution_x = size[0]
    sc.render.resolution_y = size[1]
    sc.render.film_transparent = transparent
    world = bpy.data.worlds.new("W")
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    bg.inputs[0].default_value = (0.5, 0.5, 0.52, 1)
    bg.inputs[1].default_value = 0.6
    sc.world = world
    return sc


def _frame_camera(sc, o, dirv, margin=1.0, ortho=False):
    bb = [o.matrix_world @ Vector(c) for c in o.bound_box]
    mn = Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
    mx = Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
    ctr = (mn + mx) / 2
    rad = max((mx - mn).length / 2, 0.02)
    cam_data = bpy.data.cameras.new("C")
    if ortho:
        cam_data.type = "ORTHO"
        cam_data.ortho_scale = max(mx.z - mn.z, mx.y - mn.y) * 1.08 * margin
    else:
        cam_data.lens = 70
    cam = bpy.data.objects.new("C", cam_data)
    sc.collection.objects.link(cam)
    cam.location = ctr + dirv.normalized() * rad * (3.6 * margin if not ortho else 6.0)
    cam.rotation_euler = (ctr - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc.camera = cam
    return ctr


def _add_light(sc, kind, energy, rot, size=1.0, loc=(0, 0, 0)):
    ld = bpy.data.lights.new("L", kind)
    ld.energy = energy
    if kind == "AREA":
        ld.size = size
    lo = bpy.data.objects.new("L", ld)
    lo.rotation_euler = [math.radians(a) for a in rot]
    lo.location = loc
    sc.collection.objects.link(lo)


def draw_note_icon(path):
    """Icone de note dessinee (feuille manuscrite), plus lisible qu'un rendu 3D d'une feuille plate."""
    from PIL import Image, ImageDraw
    im = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    sheet = Image.new("RGBA", (150, 200), (226, 218, 190, 255))
    d = ImageDraw.Draw(sheet)
    import random as _r
    rnd = _r.Random(3)
    for i in range(11):
        y = 26 + i * 15
        d.line((12, y, 140, y), fill=(150, 170, 200, 255), width=1)
        x = 16
        while x < 130:
            w = rnd.randint(8, 26)
            d.line((x, y - 4, x + w, y - 3 + rnd.randint(-1, 1)), fill=(40, 40, 70, 255), width=2)
            x += w + rnd.randint(4, 8)
    d.line((22, 8, 22, 196), fill=(200, 90, 90, 255), width=1)
    sheet = sheet.rotate(-9, expand=True, resample=Image.BICUBIC)
    im.alpha_composite(sheet, ((256 - sheet.size[0]) // 2, (256 - sheet.size[1]) // 2))
    im.save(path)


def render_icon(item, model):
    """Icone d'inventaire 256x256 sur fond transparent."""
    os.makedirs(OUT_ICON, exist_ok=True)
    if item == "Note":
        try:
            draw_note_icon(os.path.join(OUT_ICON, "I_Note.png"))
            print("  icone", item)
            return
        except Exception:
            pass
    reset()
    o = MODELS[model]()
    finalize(o)
    sc = _setup_render((256, 256), True)
    flat = item in ("Note", "VHSTape", "EnergyBar", "Bandage")
    _frame_camera(sc, o, Vector((0.35, -0.5, 1.0)) if flat else Vector((0.9, -1.0, 0.75)), margin=1.12 if not flat else 1.0)
    _add_light(sc, "SUN", 3.5, (45, 15, 35))
    _add_light(sc, "SUN", 1.2, (120, 0, -140))
    sc.render.filepath = os.path.join(OUT_ICON, "I_" + item + ".png")
    bpy.ops.render.render(write_still=True)
    print("  icone", item)


def render_silhouette():
    """Silhouette de l'explorateur en combinaison (panneau Equipement de l'inventaire)."""
    os.makedirs(OUT_ICON, exist_ok=True)
    reset()
    o = MODELS["SM_Hazmat"]()
    finalize(o, all_smooth=True)
    sc = _setup_render((384, 768), True)
    _frame_camera(sc, o, Vector((1.0, -0.12, 0.05)), margin=1.0, ortho=True)
    _add_light(sc, "SUN", 0.6, (60, 0, 90))
    _add_light(sc, "SUN", 4.0, (80, 0, -100))
    path = os.path.join(OUT_ICON, "I_Silhouette.png")
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    try:  # assombrit en silhouette en gardant un leger contour lumineux
        from PIL import Image, ImageFilter
        im = Image.open(path).convert("RGBA")
        r, g, b, a = im.split()
        lum = Image.merge("RGB", (r, g, b)).convert("L").point(lambda v: int(v * 0.25))
        out = Image.merge("RGBA", (lum, lum, lum, a))
        out.save(path)
    except Exception as e:
        print("  (silhouette brute, PIL indisponible :", e, ")")
    print("  silhouette")


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
    if "--no-icons" not in argv and not [a for a in argv if not a.startswith("--")]:
        print("Icones ->", OUT_ICON)
        for item, model in ICONS.items():
            render_icon(item, model)
        render_silhouette()
    print("Termine :", len(names), "modeles")


if __name__ == "__main__":
    main()
