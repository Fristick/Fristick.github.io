"""v4.13 : kit modulaire du Level 0 de THE BACKROOMS, construit, exporte et rendu par Blender (bpy).

Usage (Blender 5.0.1 ; module Python bpy, ou Blender en ligne de commande) :
    python Tools/Blender/build_level0_kit.py [--seed 9] [--out RawAssets/Level0Kit] [--render] [--samples 48] [--res 1280x720]
    blender -b -P Tools/Blender/build_level0_kit.py -- --seed 9 --render

Ce qu'il fait, dans l'ordre (tout est reproductible : meme graine, memes fichiers) :
 1. Disposition : la grille du Level 0 pour la graine demandee (graine -BRSeed du jeu ; 9 = graine de demonstration),
    rejouee par la simulation Python du generateur du jeu (Tools/verify_pitfalls.py : murs, portes, portes percees,
    piliers, salles de fosses et leurs trous, neons, cachettes, porte de service et chemin de L.). La simulation est
    verifiee contre les regles du C++ par ses propres controles ; l'assemblage Blender n'a PAS ete compare a une
    capture du jeu dans Unreal (pas d'Unreal ici).
 2. Textures : Tools/Blender/level0_kit_textures.py (images, aucun noeud procedural a reconstruire dans Unreal).
 3. Kit : modules separes (collection L0_Modules, a l'origine, pivots sur la grille de 3,5 m), collisions UCX_<maillage>_NN
    (L0_Collisions), UV0 a l'echelle du monde (tuilage), UV1 de lightmap sans chevauchement.
 4. Scene de demonstration : instances liees des modules (L0_Layout, L0_Pitfalls, L0_Mission), eclairage (L0_Lighting),
    reperes nommes (L0_GameplayMarkers), anciens elements archives (L0_Archive : echelle, trappe, mur instable).
 5. Controles : etalon de 1 m, normales, trous ouverts (lancers de rayons), passages des portes, chemin du depart a la
    porte de service (lancers de rayons a 1 m), relecture de chaque FBX exporte.
 6. Exports : un FBX par module (avec ses collisions), BR_Level0.blend (images emballees), L0_Placement.json,
    rendus Cycles (avec --render), rapport build_report.json.
Repere : Blender en metres, Z vers le haut. Unreal (cm, repere main gauche) : x_ue = 100 x, y_ue = -100 y, z_ue = 100 z,
lacet_ue = -lacet_blender. Avec cette conversion, la cellule (X, Y) du jeu a son centre en ((X + 0,5) 350, (Y + 0,5) 350) cm.
"""
import argparse
import importlib.util
import json
import math
import os
import sys
import time

import warnings

import bpy
import bmesh
from mathutils import Vector, Matrix

# Material.use_nodes reste necessaire en 5.0 pour les materiaux crees par script (retrait annonce pour Blender 6.0)
warnings.filterwarnings("ignore", message=".*use_nodes.*", category=DeprecationWarning)
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import level0_kit_textures as KT  # noqa: E402

# ----------------------------------------------------------------------------------------------- dimensions (jeu)
S = 3.5           # cellule
H = 2.9           # hauteur sous plafond
T = 0.6           # epaisseur des murs
DOOR_W = 1.5      # passage libre des portes
DOOR_H = 2.25     # hauteur sous linteau (BRChunk::AddDoorway)
SPAN = S - T      # mur entre deux poteaux
PIT_HOLE = 2.0
PIT_DEPTH = 14.0
PIT_KILL = 4.5
SLAB = 0.2        # dalle du sol (tranche visible dans les fosses)
CARPET = 0.02     # couche de moquette
BASE_H = 0.12     # plinthe
BASE_D = 0.015
SVC_HOLE_W = 1.30  # ouverture de la porte de service (ABRExit::ServiceHoleWidth)
SVC_HOLE_H = 2.25
TEX_WORLD = {"Wallpaper": 1.2, "Carpet": 2.2, "CeilingTile": 1.2, "Concrete": 3.0, "PitConcrete": 3.0, "Baseboard": 1.0, "FixtureMetal": 1.0,
             "DoorSteel": 1.0, "Hardware": 0.5}


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=9)
    ap.add_argument("--out", default=os.path.join(ROOT, "RawAssets", "Level0Kit"))
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--samples", type=int, default=48)
    ap.add_argument("--res", default="1280x720")
    ap.add_argument("--skip-textures", action="store_true", help="reutilise les textures deja generees")
    ap.add_argument("--only", default="", help="rendus : seulement ces vues (ex. L0_VueEnsemble), les autres temps sont gardes")
    return ap.parse_args(argv)


# ----------------------------------------------------------------------------------------------- disposition (jeu)
def load_layout(seed):
    spec = importlib.util.spec_from_file_location("verify_pitfalls", os.path.join(ROOT, "Tools", "verify_pitfalls.py"))
    vp = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(vp)
    P = vp.read_level0()
    L = vp.Level0(P, seed)
    return vp, P, L


def g2b(x_cm, y_cm, z=0.0):
    """Coordonnees du jeu (cm, Unreal) -> Blender (m)"""
    return Vector((x_cm / 100.0, -y_cm / 100.0, z))


def cell_center(x, y, z=0.0):
    return g2b((x + 0.5) * S * 100, (y + 0.5) * S * 100, z)


def vertex(x, y, z=0.0):
    """Coin (x, y) de la grille (le coin (X + 1, Y + 1) d'une cellule)"""
    return g2b(x * S * 100, y * S * 100, z)


# ----------------------------------------------------------------------------------------------- maillages
class MeshBuilder:
    """Boites assemblees en un maillage, UV0 a l'echelle du monde par face (projection selon la normale)"""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.col = None
        self.mats = []
        self.inward = 0  # faces de boite tournees vers l'interieur de leur boite (doit rester a 0)

    def mat_index(self, mat):
        if mat not in self.mats:
            self.mats.append(mat)
        return self.mats.index(mat)

    def colors(self):
        if self.col is None:
            self.col = self.bm.loops.layers.color.new("Col")
        return self.col

    def quad(self, pts, mat, world=1.0, shade=None):
        col = self.colors() if shade is not None else None  # avant les faces : ajouter un calque invalide les boucles
        verts = [self.bm.verts.new(p) for p in pts]
        f = self.bm.faces.new(verts)
        f.material_index = self.mat_index(mat)
        n = (Vector(pts[1]) - Vector(pts[0])).cross(Vector(pts[2]) - Vector(pts[0]))
        n.normalize()
        ax = max(range(3), key=lambda i: abs(n[i]))
        for loop in f.loops:
            p = loop.vert.co
            if ax == 0:
                u, v = (p.y if n.x > 0 else -p.y), p.z
            elif ax == 1:
                u, v = (-p.x if n.y > 0 else p.x), p.z
            else:
                u, v = p.x, (p.y if n.z > 0 else -p.y)
            loop[self.uv].uv = (u / world, v / world)
            if shade is not None:
                loop[col] = shade(p)
        return f

    def _check(self, f, center):
        if f.normal.dot(f.calc_center_median() - Vector(center)) <= 0:
            self.inward += 1

    def box(self, lo, hi, mat, faces="xXyYzZ", world=None, shade=None):
        world = TEX_WORLD.get(mat, 1.0) if world is None else world
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        c = ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
        n0 = len(self.bm.faces)
        self._faces(lo, hi, mat, faces, world, shade)
        self.bm.faces.ensure_lookup_table()
        for i in range(n0, len(self.bm.faces)):
            f = self.bm.faces[i]
            f.normal_update()
            self._check(f, c)

    def _faces(self, lo, hi, mat, faces, world, shade):
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        if "x" in faces:
            self.quad([(x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)], mat, world, shade)
        if "X" in faces:
            self.quad([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], mat, world, shade)
        if "y" in faces:
            self.quad([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], mat, world, shade)
        if "Y" in faces:
            self.quad([(x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)], mat, world, shade)
        if "z" in faces:
            self.quad([(x0, y1, z0), (x1, y1, z0), (x1, y0, z0), (x0, y0, z0)], mat, world, shade)
        if "Z" in faces:
            self.quad([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], mat, world, shade)

    def finish(self, materials, coll):
        BUILD_ISSUES["inward"][self.name] = self.inward
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=1e-5)
        me = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(me)
        self.bm.free()
        for m in self.mats:
            me.materials.append(materials[m])
        ob = bpy.data.objects.new(self.name, me)
        coll.objects.link(ob)
        return ob


BUILD_ISSUES = {"inward": {}}
RENDER = {}  # objets que certains rendus modifient le temps d'une image (battant entrouvert)


def lightmap_uv(ob):
    """UV1 (lightmap) sans chevauchement : projection intelligente sur un canal distinct"""
    me = ob.data
    if "UVLightmap" not in me.uv_layers:
        me.uv_layers.new(name="UVLightmap")
    me.uv_layers.active = me.uv_layers["UVLightmap"]
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.lightmap_pack(PREF_CONTEXT="ALL_FACES", PREF_PACK_IN_ONE=True, PREF_NEW_UVLAYER=False, PREF_MARGIN_DIV=0.2)
    bpy.ops.object.mode_set(mode="OBJECT")
    me.uv_layers.active = me.uv_layers["UVMap"]
    me.uv_layers["UVMap"].active_render = True
    ob.select_set(False)


def collision(name, boxes, coll):
    """Collisions Unreal : un volume convexe par boite, UCX_<maillage>_NN"""
    out = []
    for i, (lo, hi) in enumerate(boxes):
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        c = (Vector(lo) + Vector(hi)) * 0.5
        d = Vector(hi) - Vector(lo)
        bmesh.ops.scale(bm, vec=d, verts=bm.verts)
        bmesh.ops.translate(bm, vec=c, verts=bm.verts)
        me = bpy.data.meshes.new("UCX_%s_%02d" % (name, i))
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new(me.name, me)
        ob.display_type = "WIRE"
        coll.objects.link(ob)
        out.append(ob)
    return out


# ----------------------------------------------------------------------------------------------- materiaux
def make_materials(tex_dir):
    def img(name, data=False):
        path = None
        for ext in (".jpg", ".png"):
            p = os.path.join(tex_dir, name + ext)
            if os.path.isfile(p):
                path = p
        if path is None:
            return None
        im = bpy.data.images.load(path, check_existing=True)
        im.colorspace_settings.name = "Non-Color" if data else "sRGB"
        return im

    def pbr(name, tex, tint=(1, 1, 1, 1), vertex_dark=False, emissive=0.0, decal=None, uv="UVMap"):
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        nt = m.node_tree
        nodes, links = nt.nodes, nt.links
        bsdf = nodes.get("Principled BSDF")
        uvn = nodes.new("ShaderNodeUVMap")
        uvn.uv_map = uv
        uvn.location = (-1100, 0)
        if decal:
            t = nodes.new("ShaderNodeTexImage")
            t.image = img(decal)
            t.location = (-700, 200)
            links.new(uvn.outputs["UV"], t.inputs["Vector"])
            links.new(t.outputs["Color"], bsdf.inputs["Base Color"])
            links.new(t.outputs["Alpha"], bsdf.inputs["Alpha"])
            bsdf.inputs["Roughness"].default_value = 0.9
            m.surface_render_method = "DITHERED"
            return m
        base = nodes.new("ShaderNodeTexImage")
        base.image = img(tex + "_BaseColor")
        base.location = (-700, 300)
        links.new(uvn.outputs["UV"], base.inputs["Vector"])
        col = base.outputs["Color"]
        if tint != (1, 1, 1, 1):
            mix = nodes.new("ShaderNodeMix")
            mix.data_type = "RGBA"
            mix.blend_type = "MULTIPLY"
            mix.inputs["Factor"].default_value = 1.0
            mix.inputs[7].default_value = tint
            links.new(col, mix.inputs[6])
            col = mix.outputs[2]
        if vertex_dark:
            # Profondeur des fosses : couleur de sommet (blanc en haut, noir au fond), exportee avec le FBX
            vc = nodes.new("ShaderNodeVertexColor")
            vc.layer_name = "Col"
            vc.location = (-700, 520)
            mix2 = nodes.new("ShaderNodeMix")
            mix2.data_type = "RGBA"
            mix2.blend_type = "MULTIPLY"
            mix2.inputs["Factor"].default_value = 1.0
            links.new(col, mix2.inputs[6])
            links.new(vc.outputs["Color"], mix2.inputs[7])
            col = mix2.outputs[2]
        links.new(col, bsdf.inputs["Base Color"])
        orm = nodes.new("ShaderNodeTexImage")
        orm.image = img(tex + "_ORM", True)
        orm.location = (-700, 0)
        links.new(uvn.outputs["UV"], orm.inputs["Vector"])
        sep = nodes.new("ShaderNodeSeparateColor")
        sep.location = (-420, 0)
        links.new(orm.outputs["Color"], sep.inputs["Color"])
        links.new(sep.outputs["Green"], bsdf.inputs["Roughness"])
        links.new(sep.outputs["Blue"], bsdf.inputs["Metallic"])
        nrm = nodes.new("ShaderNodeTexImage")
        nrm.image = img(tex + "_Normal", True)
        nrm.location = (-1000, -300)
        links.new(uvn.outputs["UV"], nrm.inputs["Vector"])
        # Carte DirectX (convention du projet et d'Unreal) : Blender attend OpenGL, le vert est inverse ici seulement
        sep2 = nodes.new("ShaderNodeSeparateColor")
        sep2.location = (-760, -300)
        links.new(nrm.outputs["Color"], sep2.inputs["Color"])
        inv = nodes.new("ShaderNodeMath")
        inv.operation = "SUBTRACT"
        inv.inputs[0].default_value = 1.0
        links.new(sep2.outputs["Green"], inv.inputs[1])
        comb = nodes.new("ShaderNodeCombineColor")
        comb.location = (-520, -300)
        links.new(sep2.outputs["Red"], comb.inputs["Red"])
        links.new(inv.outputs[0], comb.inputs["Green"])
        links.new(sep2.outputs["Blue"], comb.inputs["Blue"])
        nm = nodes.new("ShaderNodeNormalMap")
        nm.location = (-300, -300)
        nm.uv_map = uv
        links.new(comb.outputs["Color"], nm.inputs["Color"])
        links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
        if emissive > 0:
            em = nodes.new("ShaderNodeTexImage")
            em.image = img("T_L0K_Diffuser_Emissive")
            em.location = (-700, -620)
            links.new(uvn.outputs["UV"], em.inputs["Vector"])
            links.new(em.outputs["Color"], bsdf.inputs["Emission Color"])
            bsdf.inputs["Emission Strength"].default_value = emissive
        return m

    M = {}
    M["Wallpaper"] = pbr("M_L0_Wallpaper", "T_L0K_Wallpaper")
    M["WallpaperAged"] = pbr("M_L0_Wallpaper_Aged", "T_L0K_Wallpaper", tint=(0.93, 0.9, 0.82, 1))
    M["Carpet"] = pbr("M_L0_Carpet", "T_L0K_Carpet")
    M["CeilingTile"] = pbr("M_L0_CeilingTile", "T_L0K_CeilingTile")
    M["Concrete"] = pbr("M_L0_Concrete", "T_L0K_Concrete")
    M["PitConcrete"] = pbr("M_L0_PitConcrete", "T_L0K_Concrete", vertex_dark=True)  # puits : couleur de sommet (profondeur)
    M["Baseboard"] = pbr("M_L0_Baseboard", "T_L0K_Baseboard")
    M["FixtureMetal"] = pbr("M_L0_FixtureMetal", "T_L0K_FixtureMetal")
    M["Diffuser"] = pbr("M_L0_LightDiffuser", "T_L0K_Diffuser", emissive=6.0) if img("T_L0K_Diffuser_ORM", True) else None
    if M["Diffuser"] is None:
        # Diffuseur : pas de carte ORM ni normale (surface lisse), couleur et emission seulement
        m = bpy.data.materials.new("M_L0_LightDiffuser")
        m.use_nodes = True
        b = m.node_tree.nodes.get("Principled BSDF")
        t = m.node_tree.nodes.new("ShaderNodeTexImage")
        t.image = img("T_L0K_Diffuser_Emissive")
        m.node_tree.links.new(t.outputs["Color"], b.inputs["Base Color"])
        m.node_tree.links.new(t.outputs["Color"], b.inputs["Emission Color"])
        b.inputs["Emission Strength"].default_value = 6.0
        b.inputs["Roughness"].default_value = 0.35
        M["Diffuser"] = m
        off = bpy.data.materials.new("M_L0_LightDiffuser_Off")
        off.use_nodes = True
        b2 = off.node_tree.nodes.get("Principled BSDF")
        b2.inputs["Base Color"].default_value = (0.55, 0.56, 0.52, 1)
        b2.inputs["Roughness"].default_value = 0.4
        M["DiffuserOff"] = off
    M["DoorSteel"] = pbr("M_L0_DoorSteel", "T_L0K_DoorSteel")
    M["Hardware"] = pbr("M_L0_DoorHardware", "T_L0K_Hardware")
    dark = bpy.data.materials.new("M_L0_PitDark")
    dark.use_nodes = True
    dark.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = (0.008, 0.008, 0.007, 1)
    dark.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = 1.0
    M["PitDark"] = dark
    chalk = bpy.data.materials.new("M_L0_Chalk")
    chalk.use_nodes = True
    chalk.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = (0.82, 0.81, 0.76, 1)
    chalk.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = 0.98
    M["Chalk"] = chalk
    glitch = bpy.data.materials.new("M_L0_Archive_Glitch")
    glitch.use_nodes = True
    gb = glitch.node_tree.nodes.get("Principled BSDF")
    gb.inputs["Base Color"].default_value = (0.5, 0.4, 0.8, 1)
    gb.inputs["Emission Color"].default_value = (0.6, 0.5, 0.9, 1)
    gb.inputs["Emission Strength"].default_value = 2.0
    M["Glitch"] = glitch
    draft = bpy.data.materials.new("M_L0_DraftGlow")
    draft.use_nodes = True
    db = draft.node_tree.nodes.get("Principled BSDF")
    db.inputs["Base Color"].default_value = (0.7, 0.85, 1.0, 1)
    db.inputs["Emission Color"].default_value = (0.7, 0.85, 1.0, 1)
    db.inputs["Emission Strength"].default_value = 6.0  # ABRExit::BuildServiceDoor : NewGlow (0,7 ; 0,85 ; 1), 1,6
    M["DraftGlow"] = draft
    for key, dec in (("DecalWall", "T_L0K_Decal_WaterStain"), ("DecalCeiling", "T_L0K_Decal_CeilingStain"), ("DecalCarpet", "T_L0K_Decal_CarpetDamp")):
        M[key] = pbr("M_L0_" + key, None, decal=dec)
    return M


# ----------------------------------------------------------------------------------------------- modules
def build_modules(M, coll, ccoll):
    mods = {}
    cols = {}

    def done(mb, boxes=None):
        ob = mb.finish(M, coll)
        mods[mb.name] = ob
        cols[mb.name] = collision(mb.name, boxes, ccoll) if boxes else []
        return ob

    # Etalon : 1 m (100 cm dans Unreal)
    mb = MeshBuilder("SM_L0_Reference_1m")
    mb.box((-0.5, -0.5, 0), (0.5, 0.5, 1.0), "Concrete")
    done(mb)

    def wall_span(name, x0, x1, z0=0.0, z1=H, mat="Wallpaper", ends=True):
        mb = MeshBuilder(name)
        mb.box((x0, -T / 2, z0), (x1, T / 2, z1), mat, "yYZ" + ("xX" if ends else ""))
        return mb

    # Mur entre deux poteaux (la cellule fait 3,5 m : 2,9 m de mur + un demi-poteau de chaque cote)
    mb = wall_span("SM_L0_Wall_350", -SPAN / 2, SPAN / 2)
    done(mb, [((-SPAN / 2, -T / 2, 0), (SPAN / 2, T / 2, H))])
    mb = wall_span("SM_L0_Wall_Aged_350", -SPAN / 2, SPAN / 2, mat="WallpaperAged")
    done(mb, [((-SPAN / 2, -T / 2, 0), (SPAN / 2, T / 2, H))])
    # Ouverture de porte : deux jambages de mur et un linteau (BRChunk::AddDoorway), passage libre 1,5 m
    mb = MeshBuilder("SM_L0_Doorway")
    side = (SPAN - DOOR_W) / 2
    mb.box((-SPAN / 2, -T / 2, 0), (-DOOR_W / 2, T / 2, H), "Wallpaper", "xXyYZ")
    mb.box((DOOR_W / 2, -T / 2, 0), (SPAN / 2, T / 2, H), "Wallpaper", "xXyYZ")
    mb.box((-DOOR_W / 2, -T / 2, DOOR_H), (DOOR_W / 2, T / 2, H), "Wallpaper", "yYzZ")
    done(mb, [((-SPAN / 2, -T / 2, 0), (-DOOR_W / 2, T / 2, H)), ((DOOR_W / 2, -T / 2, 0), (SPAN / 2, T / 2, H)),
              ((-DOOR_W / 2, -T / 2, DOOR_H), (DOOR_W / 2, T / 2, H))])
    # Chambranle (deux faces) : jambages et traverse en legere saillie, sans collision
    mb = MeshBuilder("SM_L0_DoorCasing")
    cw, cd = 0.09, 0.025
    for sgn in (-1, 1):
        y0, y1 = (T / 2, T / 2 + cd) if sgn > 0 else (-T / 2 - cd, -T / 2)
        mb.box((-DOOR_W / 2 - cw, y0, 0), (-DOOR_W / 2, y1, DOOR_H), "Baseboard")
        mb.box((DOOR_W / 2, y0, 0), (DOOR_W / 2 + cw, y1, DOOR_H), "Baseboard")
        mb.box((-DOOR_W / 2 - cw, y0, DOOR_H), (DOOR_W / 2 + cw, y1, DOOR_H + cw), "Baseboard")
    done(mb)
    # Plinthes (une face de mur ; une longueur de jambage de porte)
    for name, ln in (("SM_L0_Baseboard_290", SPAN), ("SM_L0_Baseboard_070", side)):
        mb = MeshBuilder(name)
        mb.box((-ln / 2, T / 2, 0), (ln / 2, T / 2 + BASE_D, BASE_H), "Baseboard", "xXYZ")
        done(mb)
    # Poteaux aux croisements : angle, T, croix, extremite ; plinthes sur les faces visibles seulement. Repere : les murs
    # du poteau arrivent par +X et +Y (angle), +X, +Y et -X (T), les quatre (croix), +X (extremite), +X et -X (droit)
    posts = {"SM_L0_Wall_Corner": "xy", "SM_L0_Wall_JunctionT": "y", "SM_L0_Wall_JunctionX": "", "SM_L0_Wall_End": "xyY", "SM_L0_Wall_Straight": "yY"}
    for name, exposed in posts.items():
        mb = MeshBuilder(name)
        mb.box((-T / 2, -T / 2, 0), (T / 2, T / 2, H), "Wallpaper", "xXyYZ")
        for f in exposed:
            if f == "x":
                mb.box((-T / 2 - BASE_D, -T / 2 - (BASE_D if "y" in exposed else 0), 0), (-T / 2, T / 2 + (BASE_D if "Y" in exposed else 0), BASE_H), "Baseboard", "xyYZ")
            elif f == "y":
                mb.box((-T / 2, -T / 2 - BASE_D, 0), (T / 2, -T / 2, BASE_H), "Baseboard", "yZ")
            elif f == "Y":
                mb.box((-T / 2, T / 2, 0), (T / 2, T / 2 + BASE_D, BASE_H), "Baseboard", "YZ")
        done(mb, [((-T / 2, -T / 2, 0), (T / 2, T / 2, H))])
    # Pilier carre isole (45 cm) avec sa plinthe
    mb = MeshBuilder("SM_L0_Pillar")
    p = 0.225
    mb.box((-p, -p, 0), (p, p, H), "Wallpaper", "xXyYZ")
    mb.box((-p - BASE_D, -p - BASE_D, 0), (p + BASE_D, p + BASE_D, BASE_H), "Baseboard", "xXyYZ")
    done(mb, [((-p, -p, 0), (p, p, H))])
    # Sol (moquette sur dalle) et plafond suspendu, une cellule
    mb = MeshBuilder("SM_L0_Floor_350")
    mb.box((-S / 2, -S / 2, -SLAB), (S / 2, S / 2, 0), "Carpet", "Z")
    mb.box((-S / 2, -S / 2, -SLAB), (S / 2, S / 2, 0), "Concrete", "xXyYz")
    done(mb, [((-S / 2, -S / 2, -SLAB), (S / 2, S / 2, 0))])
    mb = MeshBuilder("SM_L0_Ceiling_350")
    mb.box((-S / 2, -S / 2, H), (S / 2, S / 2, H + 0.05), "CeilingTile", "z")
    mb.box((-S / 2, -S / 2, H), (S / 2, S / 2, H + 0.05), "CeilingTile", "xXyYZ", world=1.2)
    done(mb, [((-S / 2, -S / 2, H), (S / 2, S / 2, H + 0.05))])
    # Neon encastre 1,2 x 0,6 m : cadre metallique, diffuseur emissif (allume / eteint)
    for name, dif in (("SM_L0_Light_Panel", "Diffuser"), ("SM_L0_Light_Panel_Off", "DiffuserOff")):
        mb = MeshBuilder(name)
        lx, ly = 0.6, 0.3
        fr = 0.035
        mb.box((-lx, -ly, H - 0.012), (lx, ly, H), "FixtureMetal", "xXyY")
        mb.box((-lx, -ly, H - 0.012), (-lx + fr, ly, H), "FixtureMetal", "z")
        mb.box((lx - fr, -ly, H - 0.012), (lx, ly, H), "FixtureMetal", "z")
        mb.box((-lx + fr, -ly, H - 0.012), (lx - fr, -ly + fr, H), "FixtureMetal", "z")
        mb.box((-lx + fr, ly - fr, H - 0.012), (lx - fr, ly, H), "FixtureMetal", "z")
        mb.box((-lx + fr, -ly + fr, H - 0.006), (lx - fr, ly - fr, H), dif, "z", world=1.2)
        ob = done(mb)
        # UV du diffuseur : l'image entiere sur le panneau
        me = ob.data
        uvl = me.uv_layers["UVMap"]
        for poly in me.polygons:
            if me.materials[poly.material_index] == M[dif]:
                for li in poly.loop_indices:
                    co = me.vertices[me.loops[li].vertex_index].co
                    uvl.data[li].uv = ((co.x + lx) / (2 * lx), (co.y + ly) / (2 * ly))
    # ---- Salle de fosses : tuile a trou (centree sur un coin de la grille, trou de 2 x 2 m), bords, puits, fond
    hole = PIT_HOLE / 2
    mb = MeshBuilder("SM_L0_Pit_HoleTile")
    rim = [((-S / 2, -S / 2), (S / 2, -hole)), ((-S / 2, hole), (S / 2, S / 2)), ((-S / 2, -hole), (-hole, hole)), ((hole, -hole), (S / 2, hole))]
    for (a, b) in rim:
        mb.box((a[0], a[1], -SLAB), (b[0], b[1], 0), "Carpet", "Z")
        mb.box((a[0], a[1], -SLAB), (b[0], b[1], -CARPET), "Concrete", "z")
    # Tranches du trou : moquette coupee (2 cm) puis dalle de beton
    # (boites de 10 cm d'epaisseur cote dalle : une seule face emise, son orientation est controlee)
    for f, lo, hi in (("Y", (-hole, -hole - 0.1, -SLAB), (hole, -hole, 0)), ("y", (-hole, hole, -SLAB), (hole, hole + 0.1, 0)),
                      ("X", (-hole - 0.1, -hole, -SLAB), (-hole, hole, 0)), ("x", (hole, -hole, -SLAB), (hole + 0.1, hole, 0))):
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        mb.box((x0, y0, -CARPET), (x1, y1, 0), "Carpet", f, world=0.5)
        mb.box((x0, y0, -SLAB), (x1, y1, -CARPET), "Concrete", f)
    for f, lo, hi in (("y", (-S / 2, -S / 2, -SLAB), (S / 2, -S / 2 + 0.1, 0)), ("Y", (-S / 2, S / 2 - 0.1, -SLAB), (S / 2, S / 2, 0)),
                      ("x", (-S / 2, -S / 2, -SLAB), (-S / 2 + 0.1, S / 2, 0)), ("X", (S / 2 - 0.1, -S / 2, -SLAB), (S / 2, S / 2, 0))):
        mb.box(lo, hi, "Concrete", f)
    done(mb, [((a[0], a[1], -SLAB), (b[0], b[1], 0)) for a, b in rim])
    for name, w, d in (("SM_L0_Pit_Walkway_350", S, S / 2), ("SM_L0_Pit_WalkwayCorner", S / 2, S / 2)):
        mb = MeshBuilder(name)
        mb.box((-w / 2, -d / 2, -SLAB), (w / 2, d / 2, 0), "Carpet", "Z")
        mb.box((-w / 2, -d / 2, -SLAB), (w / 2, d / 2, 0), "Concrete", "xXyYz")
        done(mb, [((-w / 2, -d / 2, -SLAB), (w / 2, d / 2, 0))])
    # Puits : beton de plus en plus sombre (couleur de sommet), 14 m de profondeur sous la dalle
    def depth_shade(pnt):
        k = min(1.0, max(0.0, (-pnt.z - SLAB) / 6.0))
        v = (1.0 - k) ** 1.6 * 0.9 + 0.02
        return (v, v, v, 1.0)

    mb = MeshBuilder("SM_L0_Pit_Shaft")
    wt = 0.3
    steps = 7
    for i in range(steps):
        z1 = -SLAB - PIT_DEPTH * i / steps
        z0 = -SLAB - PIT_DEPTH * (i + 1) / steps
        mb.box((-hole - wt, -hole - wt, z0), (hole + wt, -hole, z1), "PitConcrete", "Y", shade=depth_shade)
        mb.box((-hole - wt, hole, z0), (hole + wt, hole + wt, z1), "PitConcrete", "y", shade=depth_shade)
        mb.box((-hole - wt, -hole, z0), (-hole, hole, z1), "PitConcrete", "X", shade=depth_shade)
        mb.box((hole, -hole, z0), (hole + wt, hole, z1), "PitConcrete", "x", shade=depth_shade)
    done(mb, [((-hole - wt, -hole - wt, -SLAB - PIT_DEPTH), (hole + wt, -hole, -SLAB)), ((-hole - wt, hole, -SLAB - PIT_DEPTH), (hole + wt, hole + wt, -SLAB)),
              ((-hole - wt, -hole, -SLAB - PIT_DEPTH), (-hole, hole, -SLAB)), ((hole, -hole, -SLAB - PIT_DEPTH), (hole + wt, hole, -SLAB))])
    mb = MeshBuilder("SM_L0_Pit_Bottom")
    mb.box((-hole, -hole, -SLAB - PIT_DEPTH - 0.3), (hole, hole, -SLAB - PIT_DEPTH), "PitDark", "Z")
    done(mb, [((-hole, -hole, -SLAB - PIT_DEPTH - 0.3), (hole, hole, -SLAB - PIT_DEPTH))])
    # ---- Porte de service (v4.13) : mur d'enceinte perce, cadre, battant sur pivot, poignees, barre, charnieres, sas
    # Repere : X vers la piece, le mur va de X = 0 (face interieure) a X = -T ; la charniere est du cote +Y (cote -Y dans
    # Unreal, comme ABRExit::BuildServiceDoor)
    clear = SVC_HOLE_W - 0.2
    mb = MeshBuilder("SM_L0_ServiceDoor_Wall")
    mb.box((-T, -SPAN / 2, 0), (0, -SVC_HOLE_W / 2, H), "Wallpaper", "xXyYZ")
    mb.box((-T, SVC_HOLE_W / 2, 0), (0, SPAN / 2, H), "Wallpaper", "xXyYZ")
    mb.box((-T, -SVC_HOLE_W / 2, SVC_HOLE_H), (0, SVC_HOLE_W / 2, H), "Wallpaper", "xXzZ")
    done(mb, [((-T, -SPAN / 2, 0), (0, -SVC_HOLE_W / 2, H)), ((-T, SVC_HOLE_W / 2, 0), (0, SPAN / 2, H)), ((-T, -SVC_HOLE_W / 2, SVC_HOLE_H), (0, SVC_HOLE_W / 2, H))])
    mb = MeshBuilder("SM_L0_ServiceDoor_Frame")
    mb.box((-T - 0.01, -clear / 2 - 0.1, 0), (0.02, -clear / 2, SVC_HOLE_H), "DoorSteel")
    mb.box((-T - 0.01, clear / 2, 0), (0.02, clear / 2 + 0.1, SVC_HOLE_H), "DoorSteel")
    mb.box((-T - 0.01, -SVC_HOLE_W / 2, SVC_HOLE_H - 0.1), (0.02, SVC_HOLE_W / 2, SVC_HOLE_H), "DoorSteel")
    mb.box((-T + 0.0, -clear / 2, 0), (-T + 0.6, clear / 2, 0.016), "Hardware", "xXyYZ")  # seuil
    done(mb, [((-T - 0.01, -clear / 2 - 0.1, 0), (0.02, -clear / 2, SVC_HOLE_H)), ((-T - 0.01, clear / 2, 0), (0.02, clear / 2 + 0.1, SVC_HOLE_H)),
              ((-T - 0.01, -SVC_HOLE_W / 2, SVC_HOLE_H - 0.1), (0.02, SVC_HOLE_W / 2, SVC_HOLE_H))])
    leaf_w, leaf_h, leaf_t = clear - 0.03, SVC_HOLE_H - 0.14, 0.045
    # Battant : pivot = axe des charnieres (origine du maillage), il s'etend vers -Y (fermee), s'ouvre vers le sas
    mb = MeshBuilder("SM_L0_ServiceDoor_Leaf")
    mb.box((-leaf_t / 2, -leaf_w, 0.015), (leaf_t / 2, 0, 0.015 + leaf_h), "DoorSteel")
    mb.box((leaf_t / 2, -leaf_w * 0.5 - 0.18, 1.6), (leaf_t / 2 + 0.008, -leaf_w * 0.5 + 0.18, 1.7), "Hardware", "XxyYzZ")  # plaque
    mb.box((-leaf_t / 2 - 0.04, -leaf_w + 0.12, 1.0), (-leaf_t / 2, -0.12, 1.04), "Hardware")  # barre de poussee (cote sas)
    done(mb, [((-leaf_t / 2, -leaf_w, 0.015), (leaf_t / 2, 0, 0.015 + leaf_h))])
    mb = MeshBuilder("SM_L0_ServiceDoor_Handle")
    # Poignee bequille : pivot sur l'axe du carre (origine), des deux cotes du battant
    for sgn in (-1, 1):
        mb.box((sgn * (leaf_t / 2) - (0.024 if sgn < 0 else 0), -0.03, -0.11), (sgn * (leaf_t / 2) + (0.024 if sgn > 0 else 0), 0.025, 0.11), "Hardware")
        mb.box((sgn * (leaf_t / 2 + 0.03) - 0.012, 0.0, -0.013), (sgn * (leaf_t / 2 + 0.03) + 0.012, 0.14, 0.013), "Hardware")
    done(mb)
    # Filet de lumiere froide sous le battant (le courant d'air de la porte, son repere visuel)
    mb = MeshBuilder("SM_L0_ServiceDoor_DraftGlow")
    mb.box((-T + 0.005, -(clear - 0.08) / 2, 0.0), (-T + 0.035, (clear - 0.08) / 2, 0.008), "DraftGlow", "xXZ", world=1.0)
    done(mb)
    mb = MeshBuilder("SM_L0_ServiceDoor_Hinge")
    mb.box((-0.02, -0.02, -0.065), (0.02, 0.02, 0.065), "Hardware")
    done(mb)
    # Sas : palier, escalier de beton qui descend dans le noir, murs, plafond bas (BRChunk::AddServiceDoorway)
    mb = MeshBuilder("SM_L0_ServiceVestibule")
    vw, landing, steps_n, tread, riser = 2.4, 1.4, 8, 0.3, 0.2
    bottom = -steps_n * riser - 0.3
    depth = landing + steps_n * tread + 0.9
    o0 = 0.0
    mb.box((-o0 - landing, -vw / 2, -0.2), (-o0, vw / 2, 0), "Concrete")
    for k in range(steps_n):
        top = -riser * (k + 1)
        x1 = -o0 - landing - tread * k
        mb.box((x1 - tread, -vw / 2, bottom), (x1, vw / 2, top), "Concrete")
    mb.box((-o0 - depth, -vw / 2, bottom - 0.2), (-o0 - landing - steps_n * tread, vw / 2, bottom), "Concrete")
    for sgn in (-1, 1):
        mb.box((-o0 - depth, sgn * vw / 2 + (0 if sgn > 0 else -0.2), bottom), (-o0, sgn * vw / 2 + (0.2 if sgn > 0 else 0), 2.5), "Concrete")
    mb.box((-o0 - depth - 0.2, -vw / 2 - 0.2, bottom), (-o0 - depth, vw / 2 + 0.2, 2.5), "PitDark")
    mb.box((-o0 - depth, -vw / 2 - 0.2, 2.5), (-o0, vw / 2 + 0.2, 2.7), "Concrete")
    done(mb, [((-o0 - landing, -vw / 2, -0.2), (-o0, vw / 2, 0))] + [((-o0 - landing - tread * (k + 1), -vw / 2, bottom), (-o0 - landing - tread * k, vw / 2, -riser * (k + 1))) for k in range(steps_n)])
    # ---- Marque de craie de L. (cercle barre, "L.") et fleche (vers +Y local, a droite vu de face) : traits fins
    def strokes(name, segs):
        mb = MeshBuilder(name)
        w = 0.022
        for (a, b) in segs:
            a = Vector((0, a[0], a[1]))
            b = Vector((0, b[0], b[1]))
            d = (b - a)
            ln = d.length
            if ln < 1e-4:
                continue
            d /= ln
            nrm = Vector((0, -d.z, d.y)) * (w / 2)
            a2, b2 = a - d * (w * 0.3), b + d * (w * 0.3)
            mb.quad([(0.004, *(a2 - nrm)[1:]), (0.004, *(b2 - nrm)[1:]), (0.004, *(b2 + nrm)[1:]), (0.004, *(a2 + nrm)[1:])], "Chalk", 0.5)
        return done(mb)

    circle = []
    for k in range(12):
        a0, a1 = 2 * math.pi * k / 12, 2 * math.pi * (k + 1) / 12
        circle.append(((-0.12 + 0.1 * math.cos(a0), 0.1 * math.sin(a0)), (-0.12 + 0.1 * math.cos(a1), 0.1 * math.sin(a1))))
    strokes("SM_L0_ChalkMark", circle + [((-0.19, -0.07), (-0.05, 0.07)), ((0.06, -0.1), (0.06, 0.09)), ((0.06, -0.1), (0.15, -0.1)), ((0.19, -0.1), (0.205, -0.1))])
    strokes("SM_L0_ChalkArrow", [((-0.16, 0.0), (0.16, 0.0)), ((0.16, 0.0), (0.09, 0.05)), ((0.16, 0.0), (0.09, -0.05))])
    # ---- Decals (plans avec masque alpha)
    for name, mat, (w, h), axis in (("SM_L0_Decal_WaterStain", "DecalWall", (1.0, 1.3), "x"), ("SM_L0_Decal_CeilingStain", "DecalCeiling", (1.1, 1.1), "z-"),
                                    ("SM_L0_Decal_CarpetDamp", "DecalCarpet", (2.0, 1.5), "z")):
        mb = MeshBuilder(name)
        if axis == "x":
            pts = [(0.003, -w / 2, 0), (0.003, w / 2, 0), (0.003, w / 2, h), (0.003, -w / 2, h)]
        elif axis == "z":
            pts = [(-w / 2, -h / 2, 0.004), (w / 2, -h / 2, 0.004), (w / 2, h / 2, 0.004), (-w / 2, h / 2, 0.004)]
        else:
            pts = [(-w / 2, h / 2, -0.004), (w / 2, h / 2, -0.004), (w / 2, -h / 2, -0.004), (-w / 2, -h / 2, -0.004)]
        mb.quad(pts, mat)
        ob = done(mb)
        uvl = ob.data.uv_layers["UVMap"]
        for i, l in enumerate(ob.data.loops):
            uvl.data[i].uv = [(0, 0), (1, 0), (1, 1), (0, 1)][i % 4]
    return mods, cols


def build_archive(M, coll):
    """Anciennes sorties du Level 0, gardees comme assets reutilisables (elles ne sont plus des sorties)"""
    out = {}
    mb = MeshBuilder("SM_L0_Archive_Ladder_320")
    for sgn in (-1, 1):
        mb.box((0.0, sgn * 0.21 - 0.02, 0), (0.04, sgn * 0.21 + 0.02, 3.2), "Hardware")
    for k in range(10):
        z = 0.25 + k * 0.3
        mb.box((0.005, -0.19, z - 0.012), (0.035, 0.19, z + 0.012), "Hardware")
    out["ladder"] = mb.finish(M, coll)
    mb = MeshBuilder("SM_L0_Archive_HatchFrame")
    for (lo, hi) in (((-0.45, -0.6, -0.05), (0.45, -0.55, 0)), ((-0.45, 0.55, -0.05), (0.45, 0.6, 0)), ((-0.45, -0.55, -0.05), (-0.4, 0.55, 0)), ((0.4, -0.55, -0.05), (0.45, 0.55, 0))):
        mb.box(lo, hi, "FixtureMetal")
    out["hatch"] = mb.finish(M, coll)
    mb = MeshBuilder("SM_L0_Archive_HatchLid")
    mb.box((0.0, -0.55, -0.03), (0.8, 0.55, 0.0), "FixtureMetal")
    out["lid"] = mb.finish(M, coll)
    mb = MeshBuilder("SM_L0_Archive_UnstableWall")
    mb.box((0.0, -1.1, 0.05), (0.03, 1.1, 2.45), "Glitch")
    out["unstable"] = mb.finish(M, coll)
    for ob in out.values():
        lightmap_uv(ob)
    return out


# ----------------------------------------------------------------------------------------------- disposition
def place(mods, name, coll, loc, yaw=0.0, inst_name=None, records=None, kind="decor"):
    src = mods[name]
    ob = bpy.data.objects.new(inst_name or name, src.data)
    ob.location = loc
    ob.rotation_euler = (0.0, 0.0, math.radians(yaw))
    coll.objects.link(ob)
    if records is not None:
        records.append({"name": ob.name, "asset": name, "collection": coll.name, "kind": kind, "location_m": [round(v, 4) for v in loc],
                        "rotation_deg": [0.0, 0.0, round(yaw, 3)], "scale": [1.0, 1.0, 1.0]})
    return ob


def build_layout(vp, L, mods, M, colls, records):
    N = L.N
    B = L.B
    lo, hi = -B * N, B * N
    walls = {}

    def edge_kind(x, y, east):
        return L.edge(x, y, east)

    # Aretes : murs, portes ; la porte de service remplace une arete d'enceinte
    door_cell, door_dir = L.door if L.door else (None, None)
    svc_edge = None
    if door_cell:
        if door_dir[0] != 0:
            svc_edge = (door_cell[0] if door_dir[0] > 0 else door_cell[0] - 1, door_cell[1], True)
        else:
            svc_edge = (door_cell[0], door_cell[1] if door_dir[1] > 0 else door_cell[1] - 1, False)
    aged = lambda x, y, e: vp.rand(x * 2 + (1 if e else 0), y, 7301, L.seed) < 0.18
    counts = {"wall": 0, "doorway": 0, "post": 0, "pillar": 0, "floor": 0, "ceiling": 0, "light": 0, "light_off": 0, "pit_tile": 0, "shaft": 0}
    for x in range(lo - 1, hi):
        for y in range(lo - 1, hi):
            for east in (True, False):
                nx, ny = (x + 1, y) if east else (x, y + 1)
                if not (L.cell_in_bounds(x, y) or L.cell_in_bounds(nx, ny)):
                    continue
                k = edge_kind(x, y, east)
                if k == vp.OPEN:
                    continue
                if east:
                    c = vertex(x + 1, y + 0.5)
                    yaw = 90.0
                else:
                    c = vertex(x + 0.5, y + 1)
                    yaw = 0.0
                if svc_edge == (x, y, east):
                    continue
                walls[(x, y, east)] = k
                tag = "%s_%d_%d" % ("E" if east else "N", x, y)
                if k == vp.DOOR:
                    place(mods, "SM_L0_Doorway", colls["L0_Layout"], c, yaw, "L0_Doorway_" + tag, records)
                    place(mods, "SM_L0_DoorCasing", colls["L0_Layout"], c, yaw, "L0_DoorCasing_" + tag, records)
                    side = (SPAN - DOOR_W) / 2
                    for s in (-1, 1):
                        off = Vector((s * (DOOR_W / 2 + side / 2), 0, 0))
                        for face in (0, 180):
                            rot = Matrix.Rotation(math.radians(yaw), 3, "Z")
                            place(mods, "SM_L0_Baseboard_070", colls["L0_Layout"], c + rot @ off, yaw + face, None, None)
                    counts["doorway"] += 1
                else:
                    name = "SM_L0_Wall_Aged_350" if aged(x, y, east) else "SM_L0_Wall_350"
                    place(mods, name, colls["L0_Layout"], c, yaw, "L0_Wall_" + tag, records)
                    for face in (0, 180):
                        place(mods, "SM_L0_Baseboard_290", colls["L0_Layout"], c, yaw + face, None, None)
                    counts["wall"] += 1
    # Poteaux aux sommets touches par un mur (forme selon les murs qui y arrivent)
    for vx in range(lo, hi + 1):
        for vy in range(lo, hi + 1):
            arms = {"+X": (vx, vy - 1, False) in walls or False, "-X": (vx - 1, vy - 1, False) in walls,
                    "+Y": (vx - 1, vy, True) in walls, "-Y": (vx - 1, vy - 1, True) in walls}
            # Directions du jeu, dans l'ordre de ses lacets : E (+X), S (+Y du jeu), O, N. Le module au lacet 0 a ses bras vers
            # +X et +Y Blender, soit E et N du jeu ; place au lacet Blender -lacet_jeu, ses bras tournent de +lacet_jeu
            gx = [arms["+X"], arms["+Y"], arms["-X"], arms["-Y"]]
            nb = sum(gx)
            if nb == 0:
                continue
            if nb == 4:
                name, yaw_g = "SM_L0_Wall_JunctionX", 0
            elif nb == 3:
                name, yaw_g = "SM_L0_Wall_JunctionT", ((gx.index(False) + 3) % 4) * 90  # au lacet 0, il manque le bras S
            elif nb == 2 and gx[0] == gx[2]:
                name, yaw_g = "SM_L0_Wall_Straight", 0 if gx[0] else 90
            elif nb == 2:
                first = [i for i in range(4) if gx[i] and gx[(i + 1) % 4]][0]
                name, yaw_g = "SM_L0_Wall_Corner", ((first + 1) % 4) * 90  # au lacet 0 : bras N et E (first = 3)
            else:
                name, yaw_g = "SM_L0_Wall_End", gx.index(True) * 90
            place(mods, name, colls["L0_Layout"], vertex(vx, vy), -yaw_g, "L0_Post_%d_%d" % (vx, vy), records)
            counts["post"] += 1
    # Piliers isoles (coins de cellule sans mur)
    for x in range(lo, hi):
        for y in range(lo, hi):
            if L.has_pillar(x, y):
                c = vertex(x + 1, y + 1)
                place(mods, "SM_L0_Pillar", colls["L0_Layout"], c, 0.0, "L0_Pillar_%d_%d" % (x, y), records)
                counts["pillar"] += 1
    # Sols, plafonds, neons ; salles de fosses a part
    lights = []
    for x in range(lo, hi):
        for y in range(lo, hi):
            c = cell_center(x, y)
            if not L.is_pit_cell(x, y):
                place(mods, "SM_L0_Floor_350", colls["L0_Layout"], c, 0.0, "L0_Floor_%d_%d" % (x, y), records)
                counts["floor"] += 1
            place(mods, "SM_L0_Ceiling_350", colls["L0_Layout"], c, 0.0, "L0_Ceiling_%d_%d" % (x, y), records)
            counts["ceiling"] += 1
            info = light_info(vp, L, x, y)
            if info:
                off, yaw_g, broken = info
                pos = g2b((x + 0.5) * S * 100 + off[0], (y + 0.5) * S * 100 + off[1], 0.0)
                name = "SM_L0_Light_Panel_Off" if broken else "SM_L0_Light_Panel"
                place(mods, name, colls["L0_Layout"], pos, -yaw_g, "L0_Light_%d_%d" % (x, y), records)
                counts["light_off" if broken else "light"] += 1
                if not broken:
                    lights.append((pos, -yaw_g, (x, y)))
    # Fosses : tuiles a trou centrees sur les coins interieurs, bordures, puits, fond, seuil de chute
    pit_info = []
    for ch, r in sorted(L.pit_rooms.items()):
        (x0, y0), (x1, y1) = r
        holes = set((round(hx), round(hy)) for hx, hy in L.holes(r))
        for vx in range(x0 + 1, x1):
            for vy in range(y0 + 1, y1):
                c = vertex(vx, vy)
                key = (round(vx * S * 100), round(vy * S * 100))
                if key in holes:
                    place(mods, "SM_L0_Pit_HoleTile", colls["L0_Pitfalls"], c, 0.0, "L0_PitTile_%d_%d" % (vx, vy), records, "pitfall")
                    place(mods, "SM_L0_Pit_Shaft", colls["L0_Pitfalls"], c, 0.0, "L0_PitShaft_%d_%d" % (vx, vy), records, "pitfall")
                    place(mods, "SM_L0_Pit_Bottom", colls["L0_Pitfalls"], c, 0.0, "L0_PitBottom_%d_%d" % (vx, vy), records, "pitfall")
                    counts["shaft"] += 1
                else:
                    place(mods, "SM_L0_Floor_350", colls["L0_Pitfalls"], c, 0.0, "L0_PitSolid_%d_%d" % (vx, vy), records, "pitfall")
                counts["pit_tile"] += 1
        # Bordure : 1,75 m le long des murs de la salle
        for vx in range(x0 + 1, x1):
            for (vy, yaw) in ((y0 + 0.25, 0.0), (y1 - 0.25, 0.0)):
                place(mods, "SM_L0_Pit_Walkway_350", colls["L0_Pitfalls"], vertex(vx, vy), yaw, None, records, "pitfall")
        for vy in range(y0 + 1, y1):
            for vx in (x0 + 0.25, x1 - 0.25):
                place(mods, "SM_L0_Pit_Walkway_350", colls["L0_Pitfalls"], vertex(vx, vy), 90.0, None, records, "pitfall")
        for vx in (x0 + 0.25, x1 - 0.25):
            for vy in (y0 + 0.25, y1 - 0.25):
                place(mods, "SM_L0_Pit_WalkwayCorner", colls["L0_Pitfalls"], vertex(vx, vy), 0.0, None, records, "pitfall")
        pit_info.append({"chunk": list(ch), "cells_min": [x0, y0], "cells_max": [x1 - 1, y1 - 1], "holes": len(holes),
                         "center_m": list(vertex((x0 + x1) / 2, (y0 + y1) / 2)), "hole_centers_m": [list(g2b(hx, hy)) for hx, hy in sorted(L.holes(r))]})
    return counts, lights, pit_info, svc_edge


def light_info(vp, L, x, y):
    """ABRWorld::CellLight (agencement en salles) : (decalage cm, lacet du jeu, en panne) ou None"""
    P = L.P
    r = L.room_of(x, y)
    if r and r[0][0] <= x < r[1][0] and r[0][1] <= y < r[1][1]:
        if vp.posmod(x - r[0][0], 2) != 0 or vp.posmod(y - r[0][1], 2) != 0:
            return None
        broken = vp.rand(x, y, 1955, L.seed) < vp.F32(0.12)
        return ((0.0, 0.0), 90.0 if vp.posmod(x + y, 4) == 0 else 0.0, bool(broken))
    off = L.cell_light(x, y)
    if off is None:
        return None
    spawn = L.spawn_area(x, y)
    broken = (not spawn) and vp.rand(x, y, 411, L.seed) < vp.F32(P["BrokenChance"] if "BrokenChance" in P else 0.06)
    yaw = 0.0 if vp.rand(x, y, 416, L.seed) < vp.F32(0.5) else 90.0
    return (off, yaw, bool(broken))


def build_mission(vp, L, mods, colls, records, markers):
    """Porte de service, sas, marques de craie de L. (reperes narratifs), d'apres ABRWorld::PlaceServiceDoor"""
    if not L.door:
        return None
    (cx, cy), d = L.door
    S_cm = S * 100
    # Pied de la porte (face interieure du mur d'enceinte) et orientation vers la piece, comme le jeu
    foot = g2b((cx + 0.5) * S_cm + d[0] * (S_cm / 2 - T * 50), (cy + 0.5) * S_cm + d[1] * (S_cm / 2 - T * 50))
    yaw_ue = math.degrees(math.atan2(-d[1], -d[0]))
    yaw_b = -yaw_ue
    col = colls["L0_Mission"]
    rot = Matrix.Rotation(math.radians(yaw_b), 3, "Z")
    place(mods, "SM_L0_ServiceDoor_Wall", col, foot, yaw_b, "L0_ServiceDoor_Wall", records, "narrative")
    place(mods, "SM_L0_ServiceDoor_Frame", col, foot, yaw_b, "L0_ServiceDoor_Frame", records, "narrative")
    clear = SVC_HOLE_W - 0.2
    hinge = foot + rot @ Vector((-T + 0.08, clear / 2 - 0.015, 0))
    leaf = place(mods, "SM_L0_ServiceDoor_Leaf", col, hinge, yaw_b, "L0_ServiceDoor_Leaf", records, "narrative")
    handle = place(mods, "SM_L0_ServiceDoor_Handle", col, hinge + rot @ Vector((0, -(clear - 0.03) + 0.09, 1.04)), yaw_b, "L0_ServiceDoor_Handle", records, "narrative")
    for i, z in enumerate((0.28, 1.1, 1.92)):
        place(mods, "SM_L0_ServiceDoor_Hinge", col, hinge + Vector((0, 0, z)), yaw_b, "L0_ServiceDoor_Hinge_%d" % i, records, "narrative")
    place(mods, "SM_L0_ServiceVestibule", col, foot + rot @ Vector((-T, 0, 0)), yaw_b, "L0_ServiceVestibule", records, "narrative")
    place(mods, "SM_L0_ServiceDoor_DraftGlow", col, foot, yaw_b, "L0_ServiceDoor_DraftGlow", records, "narrative")
    # Lampe froide du sas (ABRExit::BuildServiceDoor : 260 lm, (0,72 ; 0,84 ; 1), rayon 5,2 m, a 1,6 m derriere le mur)
    ld = bpy.data.lights.new("LT_L0_ServiceVestibule", "POINT")
    ld.energy = 18.0
    ld.color = (0.72, 0.84, 1.0)
    ld.shadow_soft_size = 0.08
    lo_ = bpy.data.objects.new(ld.name, ld)
    lo_.location = foot + rot @ Vector((-T - 1.6, 0, 2.15))
    colls["L0_Lighting"].objects.link(lo_)
    RENDER["leaf"] = leaf
    # Parentage : poignee sur le battant (elle suit son ouverture)
    handle.parent = leaf
    handle.matrix_parent_inverse = leaf.matrix_world.inverted()
    markers.append(("MK_ServiceDoor", foot + rot @ Vector((1.2, 0, 0)), yaw_b, "narrative", "Porte de service du Level 0 (vers le Niveau 1), entree cote piece"))
    markers.append(("MK_ServiceDoor_Pivot", hinge, yaw_b, "gameplay", "Axe des charnieres (ouverture : +95 degres de lacet dans Unreal, vers le sas)"))
    markers.append(("MK_ServiceDoor_Exit", foot + rot @ Vector((-T - 1.5, 0, 0)), yaw_b, "gameplay", "Sas : point de depart du groupe"))
    # Marques de craie le long du chemin (ABRWorld::IsChalkMarkCell, ABRChunk::AddChalkMark)
    marks = 0
    for (c, nxt) in L.chalk_marks():
        travel = (nxt[0] - c[0], nxt[1] - c[1])
        face, arrow = None, False
        for dd in vp.DIRS:
            if dd[0] * travel[0] + dd[1] * travel[1] == 0 and wall_face(vp, L, c, dd):
                face, arrow = dd, True
                break
        if face is None:
            back = (-travel[0], -travel[1])
            if not wall_face(vp, L, c, back):
                continue
            face = back
        shift = (float(vp.rand(c[0], c[1], 2140, L.seed)) - 0.5) * 60.0
        n_g = (-face[0], -face[1])  # vers la piece
        right_g = (-face[1], face[0])  # a droite de celui qui regarde le mur (repere du jeu)
        base = ((c[0] + 0.5) * S_cm + face[0] * (S_cm / 2 - T * 50 - 0.35) + right_g[0] * shift,
                (c[1] + 0.5) * S_cm + face[1] * (S_cm / 2 - T * 50 - 0.35) + right_g[1] * shift)
        pos = g2b(base[0], base[1], 1.38)
        yaw_mark = -math.degrees(math.atan2(n_g[1], n_g[0]))
        place(mods, "SM_L0_ChalkMark", col, pos, yaw_mark, "L0_Chalk_%02d" % marks, records, "narrative")
        if arrow:
            dot = travel[0] * right_g[0] + travel[1] * right_g[1]
            ob = place(mods, "SM_L0_ChalkArrow", col, pos + Vector((0, 0, -0.24)), yaw_mark, "L0_ChalkArrow_%02d" % marks, records, "narrative")
            # Fleche du modele vers +Y local (a droite vu de face dans Blender) : demi-tour autour de X pour l'autre sens.
            # Le repere du jeu est en miroir : a droite (jeu) = a gauche (Blender) vu de face
            if dot > 0:
                ob.rotation_euler = (math.pi, 0.0, math.radians(yaw_mark))
                records[-1]["rotation_deg"] = [180.0, 0.0, round(yaw_mark, 3)]
        markers.append(("MK_ChalkMark_%02d" % marks, pos, yaw_mark, "narrative", "Marque de L. sur le chemin de la porte (cellule %d,%d)" % c))
        marks += 1
    return {"cell": [cx, cy], "dir_game": list(d), "foot_m": list(foot), "yaw_blender": yaw_b, "path_cells": len(L.door_path) - 1, "chalk_marks": marks,
            "tier": L.door_info.get("tier", 0)}


def wall_face(vp, L, c, d):
    if d[0] != 0:
        return L.edge_e(c[0] if d[0] > 0 else c[0] - 1, c[1]) == vp.WALL
    return L.edge_n(c[0], c[1] if d[1] > 0 else c[1] - 1) == vp.WALL


def pickups(vp, L):
    """ABRChunk::SpawnPickupRoll pour le Level 0 (sans cassettes : plus d'objectif VHS)"""
    P = L.P
    S_cm = S * 100
    rolls = [("AlmondWater", P.get("AlmondWaterChance", 0.3), 1001), ("AlmondWater", P.get("AlmondWaterChance", 0.3) * 0.4, 1002),
             ("Battery", P.get("BatteryChance", 0.15), 1003), ("Note", P.get("NoteChance", 0.25), 1004), ("Bandage", P.get("BandageChance", 0.12), 1006),
             ("EnergyBar", P.get("EnergyBarChance", 0.08), 1007), ("Flashlight", P.get("GearChance", 0.03), 1010), ("Headlamp", P.get("GearChance", 0.03), 1011),
             ("Vest", P.get("GearChance", 0.03) * 0.7, 1012)]
    out = []
    for cx in range(-L.B, L.B):
        for cy in range(-L.B, L.B):
            hid = L.hiding_cells((cx, cy))
            for item, chance, salt in rolls:
                if chance <= 0 or not (vp.rand(cx, cy, salt, L.seed) < vp.F32(chance)):
                    continue
                cell = L.pick_free_cell((cx, cy), salt, hid, False)
                if cell is None:
                    continue
                jx = (float(vp.rand(cell[0], cell[1], salt + 7, L.seed)) - 0.5) * S_cm * 0.5
                jy = (float(vp.rand(cell[0], cell[1], salt + 8, L.seed)) - 0.5) * S_cm * 0.5
                out.append((item, cell, g2b((cell[0] + 0.5) * S_cm + jx, (cell[1] + 0.5) * S_cm + jy, 0.01)))
    return out


# ----------------------------------------------------------------------------------------------- eclairage, cameras
def build_lighting(lights, colls):
    col = colls["L0_Lighting"]
    for i, (pos, yaw, cell) in enumerate(lights):
        ld = bpy.data.lights.new("LT_L0_Panel_%d_%d" % cell, "AREA")
        ld.shape = "RECTANGLE"
        ld.size = 1.12
        ld.size_y = 0.52
        ld.energy = 55.0
        ld.color = (1.0, 0.985, 0.86)  # blanc chaud a peine verdatre ; le jaune vient des materiaux
        ob = bpy.data.objects.new(ld.name, ld)
        ob.location = pos + Vector((0, 0, H - 0.02))
        ob.rotation_euler = (0.0, 0.0, math.radians(yaw))
        col.objects.link(ob)
    w = bpy.data.worlds.new("W_L0")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.002, 0.002, 0.0015, 1)
    bpy.context.scene.world = w


def add_camera(name, loc, target, lens=18.0, colls=None, ortho=None):
    cd = bpy.data.cameras.new(name)
    cd.lens = lens
    cd.clip_start = 0.05
    cd.clip_end = 400.0
    if ortho:
        cd.type = "ORTHO"
        cd.ortho_scale = ortho
    ob = bpy.data.objects.new(name, cd)
    ob.location = loc
    d = Vector(target) - Vector(loc)
    ob.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    colls["L0_GameplayMarkers"].objects.link(ob)
    return ob


def pick_views(vp, L):
    """Points de vue : couloir eclaire le plus long pres du depart ; intersection en T vue depuis son pied, eclairee"""
    lo, hi = -L.B * L.N, L.B * L.N
    lit = lambda c: light_info(vp, L, c[0], c[1]) is not None
    best = None
    for x in range(lo, hi):
        for y in range(lo, hi):
            if L.is_pit_cell(x, y) or abs(x) + abs(y) > 14:
                continue
            for d in vp.DIRS:
                n, c, lights = 0, (x, y), 0
                while True:
                    nx = (c[0] + d[0], c[1] + d[1])
                    if not L.cell_in_bounds(*nx) or not L.can_step(c, nx) or L.is_pit_cell(*nx):
                        break
                    n += 1
                    c = nx
                    lights += 1 if lit(c) else 0
                    if n > 12:
                        break
                walls = sum(1 for k in range(1, n) if not L.can_step((x + d[0] * k, y + d[1] * k), (x + d[0] * k - d[1], y + d[1] * k + d[0])))
                score = n * 2 + walls + lights * 2
                if n >= 5 and lit((x, y)) and (best is None or score > best[0]):
                    best = (score, (x, y), d, n)
    inter = None
    for r in range(2, 16):
        for x in range(-r, r + 1):
            for y in range(-r, r + 1):
                if max(abs(x), abs(y)) != r or L.is_pit_cell(x, y) or L.spawn_area(x, y) or not lit((x, y)):
                    continue
                ways = [d for d in vp.DIRS if L.cell_in_bounds(x + d[0], y + d[1]) and L.can_step((x, y), (x + d[0], y + d[1]))]
                if len(ways) != 3:
                    continue
                stem = [d for d in ways if (-d[0], -d[1]) not in ways]
                if not stem:
                    continue
                a = stem[0]
                c1, c2 = (x + a[0], y + a[1]), (x + 2 * a[0], y + 2 * a[1])
                if L.cell_in_bounds(*c2) and L.can_step(c1, c2) and not L.is_pit_cell(*c1) and not L.is_pit_cell(*c2) and (lit(c1) or lit(c2)):
                    inter = ((x, y), a)
                    break
            if inter:
                break
        if inter:
            break
    return best, inter


def plan_overlay(L, colls, M):
    """Vue d'ensemble seulement : ruban du chemin de L. (depart -> porte), disque du depart, pastille de la porte"""
    out = []

    def emissive(name, color, strength):
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        b = m.node_tree.nodes.get("Principled BSDF")
        b.inputs["Base Color"].default_value = color
        b.inputs["Emission Color"].default_value = color
        b.inputs["Emission Strength"].default_value = strength
        return m

    path_mat = emissive("M_Render_Path", (0.35, 0.8, 1.0, 1), 3.0)
    mb = MeshBuilder("RO_L0_DoorPath")
    w = 0.45
    for a, b in zip(L.door_path, L.door_path[1:]):
        pa, pb = cell_center(a[0], a[1], 3.05), cell_center(b[0], b[1], 3.05)
        lo = Vector((min(pa.x, pb.x) - w, min(pa.y, pb.y) - w, 3.05))
        hi = Vector((max(pa.x, pb.x) + w, max(pa.y, pb.y) + w, 3.1))
        mb.box(tuple(lo), tuple(hi), "Path", "Z")
    out.append(mb.finish({"Path": path_mat}, colls["L0_RenderOnly"]))
    for name, cell, color, r in (("RO_L0_Start", (0, 0), (0.4, 1.0, 0.45, 1), 1.4), ("RO_L0_Door", L.door[0] if L.door else None, (1.0, 0.45, 0.2, 1), 1.4)):
        if cell is None:
            continue
        mat = emissive("M_" + name, color, 4.0)
        mb = MeshBuilder(name)
        c = cell_center(cell[0], cell[1], 3.12)
        mb.box((c.x - r, c.y - r, 3.12), (c.x + r, c.y + r, 3.16), "Mark", "Z")
        out.append(mb.finish({"Mark": mat}, colls["L0_RenderOnly"]))
    return out


def cam_at_cell(c, d, height=1.65, back=0.0):
    p = cell_center(c[0], c[1], height)
    v = g2b(d[0] * 100, d[1] * 100)
    v.z = 0
    v.normalize()
    return p - v * back, v


# ----------------------------------------------------------------------------------------------- controles
def check_normals(mods):
    """Normales : (1) chaque boite a ses faces tournees vers l'exterieur (controle a la construction) ; (2) enroulement
    coherent : deux faces voisines parcourent leur arete commune en sens opposes ; (3) aucune face d'aire nulle"""
    bad = {}
    for name, ob in mods.items():
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        mixed = 0
        for e in bm.edges:
            if len(e.link_loops) == 2 and e.link_loops[0].vert == e.link_loops[1].vert:
                mixed += 1
        zero = sum(1 for f in bm.faces if f.calc_area() < 1e-8)
        bm.free()
        inward = BUILD_ISSUES["inward"].get(name, 0)
        if mixed or zero or inward:
            bad[name] = {"inconsistent_edges": mixed, "zero_area": zero, "inward_faces": inward}
    return bad


def raycast(scene, depsgraph, origin, direction, dist):
    hit, loc, nrm, idx, ob, mat = scene.ray_cast(depsgraph, Vector(origin), Vector(direction).normalized(), distance=dist)
    return (hit, loc, ob.name if ob else None)


def run_checks(vp, L, mods, pit_info, svc, scene):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    rep = {}
    ref = mods["SM_L0_Reference_1m"]
    rep["reference_1m_dims"] = [round(v, 6) for v in ref.dimensions]
    rep["normals"] = check_normals(mods)
    # Trous reellement ouverts : un rayon vertical au centre de chaque trou ne touche que le fond, 14 m plus bas
    holes_ok, holes_bad = 0, []
    for room in pit_info:
        for hc in room["hole_centers_m"]:
            hit, loc, name = raycast(scene, depsgraph, (hc[0], hc[1], 1.0), (0, 0, -1), 30.0)
            if hit and name and name.startswith("L0_PitBottom") and loc.z < -PIT_DEPTH:
                holes_ok += 1
            else:
                holes_bad.append({"hole": hc, "hit": name, "z": round(loc.z, 3) if hit else None})
    rep["holes_open"] = holes_ok
    rep["holes_blocked"] = holes_bad
    # Chemin du depart a la porte de service : un rayon a 1 m de haut entre chaque centre de cellule ne touche rien
    blocked = []
    path = L.door_path
    for a, b in zip(path, path[1:]):
        pa, pb = cell_center(a[0], a[1], 1.0), cell_center(b[0], b[1], 1.0)
        for off in (-0.42, 0.0, 0.42):  # largeur d'une capsule de 42 cm de rayon
            d = pb - pa
            side = Vector((-d.y, d.x, 0)).normalized() * off
            hit, loc, name = raycast(scene, depsgraph, pa + side, d, d.length)
            if hit:
                blocked.append({"from": list(a), "to": list(b), "offset_m": off, "hit": name})
    rep["door_path_steps"] = len(path) - 1
    rep["door_path_blocked"] = blocked
    # Passage de la porte de service : rayon a 1 m au centre de l'ouverture, battant ferme : il touche le battant ;
    # rayon a travers le cadre ouvert (battant ignore) : rien jusqu'au sas
    rep["service_door"] = svc
    # Portes ordinaires : largeur libre du module
    rep["doorway_clear_width_m"] = DOOR_W
    rep["service_door_clear_width_m"] = round(SVC_HOLE_W - 0.2, 3)
    return rep


# ----------------------------------------------------------------------------------------------- exports
def export_fbx(mods, cols, out_dir, kit_root):
    os.makedirs(out_dir, exist_ok=True)
    done = {}
    for name, ob in mods.items():
        for o in bpy.context.view_layer.objects:
            o.select_set(False)
        sel = [ob] + cols.get(name, [])
        for o in sel:
            o.hide_set(False)
            o.select_set(True)
        bpy.context.view_layer.objects.active = ob
        path = os.path.join(out_dir, name + ".fbx")
        bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                                 apply_scale_options="FBX_SCALE_UNITS", global_scale=1.0, axis_forward="-Z", axis_up="Y",
                                 mesh_smooth_type="FACE", use_tspace=True, use_mesh_modifiers=True, add_leaf_bones=False,
                                 bake_anim=False, path_mode="STRIP", embed_textures=False, colors_type="LINEAR")
        done[name] = {"path": path, "file": os.path.relpath(path, kit_root).replace(os.sep, "/"), "collisions": len(cols.get(name, [])), "dims_m": [round(v, 4) for v in ob.dimensions],
                      "materials": [m.name for m in ob.data.materials]}
        for o in sel:
            o.select_set(False)
    return done


def verify_fbx(exported):
    """Relit chaque FBX dans une scene vide : memes dimensions, collisions presentes, materiaux nommes"""
    results = {}
    for name, info in exported.items():
        tmp = bpy.data.scenes.new("FBXCheck")
        win_scene = bpy.context.window.scene if bpy.context.window else None
        if bpy.context.window:
            bpy.context.window.scene = tmp
        before = set(bpy.data.objects)
        before_mats, before_imgs = set(bpy.data.materials), set(bpy.data.images)
        bpy.ops.import_scene.fbx(filepath=info["path"])
        new = [o for o in bpy.data.objects if o not in before]
        main = [o for o in new if o.name.split(".")[0] == name]
        ucx = [o for o in new if o.name.startswith("UCX_")]
        dims = [round(v, 4) for v in main[0].dimensions] if main else None
        ok = bool(main) and all(abs(a - b) < 1e-3 for a, b in zip(dims, info["dims_m"])) and len(ucx) == info["collisions"]
        results[name] = {"ok": ok, "dims_m": dims, "collisions": len(ucx), "materials": [m.name.split(".")[0] for m in main[0].data.materials] if main else []}
        for o in new:
            me = o.data
            bpy.data.objects.remove(o, do_unlink=True)
            if me and me.users == 0:
                bpy.data.meshes.remove(me)
        for m in [m for m in bpy.data.materials if m not in before_mats]:
            bpy.data.materials.remove(m)
        for im in [im for im in bpy.data.images if im not in before_imgs]:
            bpy.data.images.remove(im)
        if win_scene:
            bpy.context.window.scene = win_scene
        bpy.data.scenes.remove(tmp)
    return results


def write_placement(path, args, L, records, markers, svc, pit_info, counts):
    def ue(loc, rot):
        return {"location_cm": [round(loc[0] * 100, 2), round(-loc[1] * 100, 2), round(loc[2] * 100, 2)],
                "rotation_deg_pitch_yaw_roll": [round(-rot[1], 3), round(-rot[2], 3), round(rot[0], 3)]}

    for r in records:
        r["unreal"] = ue(r["location_m"], r["rotation_deg"])
    mk = []
    for name, loc, yaw, kind, desc in markers:
        rec = {"name": name, "kind": kind, "description": desc, "location_m": [round(v, 4) for v in loc], "rotation_deg": [0.0, 0.0, round(yaw, 3)]}
        rec["unreal"] = ue(rec["location_m"], rec["rotation_deg"])
        mk.append(rec)
    data = {
        "format": "THE BACKROOMS - L0_Placement v4.13",
        "units": "metres (Blender) ; Unreal : centimetres",
        "axes": "Blender : Z haut, main droite. Unreal : x_ue = 100 x, y_ue = -100 y, z_ue = 100 z, lacet_ue = -lacet_blender "
                "(import FBX par defaut, Convert Scene actif). La grille du jeu : centre de la cellule (X, Y) = ((X + 0,5) 350, (Y + 0,5) 350) cm",
        "seed_user": args.seed,
        "seed_level": L.seed,
        "source": "Tools/verify_pitfalls.py (simulation Python du generateur du Level 0) ; non compare a une capture du jeu dans Unreal",
        "grid": {"cell_m": S, "cells": 2 * L.B * L.N, "zone_m": 2 * L.B * L.N * S, "ceiling_m": H, "wall_m": T, "door_clear_m": DOOR_W},
        "counts": counts,
        "service_door": svc,
        "pit_rooms": pit_info,
        "kinds": {"decor": "elements du decor (murs, sols, plafonds, neons)", "pitfall": "salle de fosses (sols perces, puits, fonds)",
                  "narrative": "reperes et elements narratifs (porte de service, marques de L.)", "gameplay": "reperes de jeu (depart, objets, chute, cachettes)"},
        "instances": records,
        "markers": mk,
    }
    # Une instance (ou un repere) par ligne : fichier compact et diffs lisibles
    big = {"instances": data.pop("instances"), "markers": data.pop("markers")}
    head = json.dumps(data, ensure_ascii=False, indent=1)
    parts = [head[:-2]]
    for key, rows in big.items():
        parts.append(',\n "%s": [\n' % key + ",\n".join("  " + json.dumps(r, ensure_ascii=False, separators=(",", ":")) for r in rows) + "\n ]")
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("".join(parts) + "\n}\n")


# ----------------------------------------------------------------------------------------------- rendus
def setup_render(args):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = args.samples
    sc.cycles.use_denoising = True
    try:
        sc.cycles.denoiser = "OPENIMAGEDENOISE"
    except Exception:
        pass
    sc.cycles.max_bounces = 6
    sc.cycles.diffuse_bounces = 3
    sc.cycles.glossy_bounces = 2
    sc.cycles.transparent_max_bounces = 4
    w, h = (int(v) for v in args.res.split("x"))
    sc.render.resolution_x = w
    sc.render.resolution_y = h
    sc.render.image_settings.file_format = "JPEG"
    sc.render.image_settings.quality = 90
    sc.view_settings.view_transform = "AgX"
    sc.view_settings.exposure = 0.9


def render_views(cams, out_dir, colls, args):
    os.makedirs(out_dir, exist_ok=True)
    sc = bpy.context.scene
    times = {}
    leaf = RENDER.get("leaf")
    only = [n for n in args.only.split(",") if n]
    w, h = sc.render.resolution_x, sc.render.resolution_y
    for v in cams:
        if only and v["name"] not in only:
            continue
        sc.camera = v["cam"]
        # Vue d'ensemble orthographique : image carree (la zone est carree)
        sc.render.resolution_x = h if v["cam"].data.type == "ORTHO" else w
        colls["L0_Ceiling"].hide_render = v["hide_ceiling"]
        for o in v["show"]:
            o.hide_render = False
        keep = leaf.rotation_euler.z if leaf else 0.0
        if leaf and v["leaf_open"]:
            leaf.rotation_euler.z = keep - math.radians(v["leaf_open"])  # vers le sas (+ dans Unreal)
        t0 = time.time()
        sc.render.filepath = os.path.join(out_dir, v["name"] + ".jpg")
        bpy.ops.render.render(write_still=True)
        times[v["name"]] = round(time.time() - t0, 1)
        print("  rendu %s : %.0f s" % (v["name"], times[v["name"]]))
        for o in v["show"]:
            o.hide_render = True
        if leaf:
            leaf.rotation_euler.z = keep
    sc.render.resolution_x = w
    colls["L0_Ceiling"].hide_render = False
    return times


# ----------------------------------------------------------------------------------------------- principal
def main():
    args = parse_args()
    t_start = time.time()
    out = os.path.abspath(args.out)
    tex_dir = os.path.join(out, "Textures")
    os.makedirs(out, exist_ok=True)
    if not args.skip_textures or not os.path.isdir(tex_dir):
        print("Textures...")
        KT.build_all(tex_dir)
    vp, P, L = load_layout(args.seed)
    print("Disposition : graine %d (niveau %u), %d salle(s) de fosses, porte de service %s" % (args.seed, L.seed, len(L.pit_rooms), L.door))

    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 1.0
    colls = {}
    for name in ("L0_Modules", "L0_Layout", "L0_Ceiling", "L0_Pitfalls", "L0_Mission", "L0_Lighting", "L0_Collisions", "L0_GameplayMarkers", "L0_Archive",
                 "L0_RenderOnly"):
        c = bpy.data.collections.new(name)
        sc.collection.children.link(c)
        colls[name] = c
    # Modules et collisions : a l'origine, hors rendu (la scene les instancie)
    colls["L0_Modules"].hide_render = True
    colls["L0_Collisions"].hide_render = True
    colls["L0_Archive"].hide_render = True
    M = make_materials(tex_dir)
    mods, cols = build_modules(M, colls["L0_Modules"], colls["L0_Collisions"])
    for ob in mods.values():
        lightmap_uv(ob)
    archive = build_archive(M, colls["L0_Archive"])
    # Modules ranges en ligne a cote de la zone (lisibles dans la vue 3D), pivots inchanges
    for i, ob in enumerate(mods.values()):
        ob.location = (-70.0 - (i % 8) * 5.0, 60.0 - (i // 8) * 6.0, 0.0)
        for u in cols.get(ob.name, []):
            u.location = ob.location
    for i, ob in enumerate(archive.values()):
        ob.location = (-70.0 - i * 4.0, 75.0, 0.0)

    records, markers = [], []
    counts, lights, pit_info, svc_edge = build_layout(vp, L, mods, M, colls, records)
    # Plafonds dans leur propre collection (masques pour la vue d'ensemble)
    for ob in list(colls["L0_Layout"].objects):
        if ob.name.startswith("L0_Ceiling_") or ob.name.startswith("L0_Light_"):
            colls["L0_Layout"].objects.unlink(ob)
            colls["L0_Ceiling"].objects.link(ob)
    svc = build_mission(vp, L, mods, colls, records, markers)
    # Decals : quelques aureoles, taches de plafond et zones humides (tirages de la graine, jamais sur le chemin de L.)
    nd = 0
    lo, hi = -L.B * L.N, L.B * L.N
    for x in range(lo, hi):
        for y in range(lo, hi):
            if L.is_pit_cell(x, y):
                continue
            r = float(vp.rand(x, y, 7401, L.seed))
            if r < 0.05:
                place(mods, "SM_L0_Decal_CeilingStain", colls["L0_Ceiling"], cell_center(x, y, H), float(vp.rand(x, y, 7402, L.seed)) * 360, "L0_DecalCeil_%d_%d" % (x, y), records)
                nd += 1
            elif r < 0.09 and (x, y) not in L.door_path:
                place(mods, "SM_L0_Decal_CarpetDamp", colls["L0_Layout"], cell_center(x, y), float(vp.rand(x, y, 7403, L.seed)) * 360, "L0_DecalFloor_%d_%d" % (x, y), records)
                nd += 1
            elif r < 0.14:
                for dd in vp.DIRS:
                    if wall_face(vp, L, (x, y), dd):
                        n_g = (-dd[0], -dd[1])
                        pos = g2b((x + 0.5) * 350 + dd[0] * (175 - 30 - 0.3), (y + 0.5) * 350 + dd[1] * (175 - 30 - 0.3), 0.15)
                        place(mods, "SM_L0_Decal_WaterStain", colls["L0_Layout"], pos, -math.degrees(math.atan2(n_g[1], n_g[0])), "L0_DecalWall_%d_%d" % (x, y), records)
                        nd += 1
                        break
    counts["decals"] = nd
    build_lighting(lights, colls)
    # Reperes de jeu
    S_cm = S * 100
    markers.append(("MK_PlayerStart", cell_center(0, 0, 0.0), 0.0, "gameplay", "Depart (cellule 0,0), zone sure sans fosse"))
    for slot in range(1, 4):
        a = math.radians(90 + 60 * (slot - 1))
        px, py = 0.5 * S_cm + math.cos(a) * min(85, S_cm * 0.3), 0.5 * S_cm + math.sin(a) * min(85, S_cm * 0.3)
        markers.append(("MK_PlayerStart_%d" % slot, g2b(px, py), 0.0, "gameplay", "Place de depart d'un coequipier (ABRWorld::SpawnSpot)"))
    for room in pit_info:
        c = room["center_m"]
        markers.append(("MK_PitKill_%d_%d" % tuple(room["chunk"]), Vector((c[0], c[1], -PIT_KILL)), 0.0, "gameplay", "Seuil de chute mortelle : 4,5 m sous le sol (la mort est geree par le jeu)"))
        markers.append(("MK_PitRoom_%d_%d" % tuple(room["chunk"]), Vector((c[0], c[1], 0.0)), 0.0, "gameplay", "Salle de fosses : %d trous de 2 x 2 m, profondeur 14 m" % room["holes"]))
    items = pickups(vp, L)
    for i, (item, cell, pos) in enumerate(items):
        markers.append(("MK_Item_%02d_%s" % (i, item), pos, 0.0, "gameplay", "Objet tire par le jeu (%s), cellule %d,%d" % (item, cell[0], cell[1])))
    hcount = 0
    for cx in range(-L.B, L.B):
        for cy in range(-L.B, L.B):
            for c in L.hiding_cells((cx, cy)):
                markers.append(("MK_Hiding_%02d" % hcount, cell_center(c[0], c[1]), 0.0, "gameplay", "Cachette (placard ou trou dans le mur), cellule %d,%d" % c))
                hcount += 1
    for name, loc, yaw, kind, desc in markers:
        e = bpy.data.objects.new(name, None)
        e.empty_display_type = "ARROWS" if kind == "narrative" else "PLAIN_AXES"
        e.empty_display_size = 0.5
        e.location = loc
        e.rotation_euler = (0, 0, math.radians(yaw))
        e["kind"] = kind
        e["description"] = desc
        colls["L0_GameplayMarkers"].objects.link(e)

    # Cameras : couloir, salle repetitive, intersection, fosses, porte de service, vue d'ensemble
    best, inter = pick_views(vp, L)
    cams = []

    def view(name, cam, hide_ceiling=False, show=(), leaf_open=0.0):
        cams.append({"name": name, "cam": cam, "hide_ceiling": hide_ceiling, "show": list(show), "leaf_open": leaf_open})

    if best:
        _, c0, d, n = best
        loc, v = cam_at_cell(c0, d, 1.62, 1.2)
        view("L0_Couloir", add_camera("CAM_L0_Couloir", loc, loc + v * 10 + Vector((0, 0, -0.1)), 17.0, colls))
    loc = cell_center(0, 0, 1.62) + Vector((-1.4, 1.4, 0))
    view("L0_SalleRepetitive", add_camera("CAM_L0_SalleRepetitive", loc, loc + Vector((6, -5, -0.3)), 16.0, colls))
    if inter:
        (ix, iy), a = inter
        loc0 = cell_center(ix + 2 * a[0], iy + 2 * a[1], 1.62)
        view("L0_Intersection", add_camera("CAM_L0_Intersection", loc0, cell_center(ix, iy, 1.45) - (cell_center(ix + a[0], iy + a[1]) - cell_center(ix, iy)) * 0.4, 16.0, colls))
    if pit_info:
        room = pit_info[0]
        (x0, y0), (x1, y1) = room["cells_min"], room["cells_max"]
        loc = vertex(x0 + 0.3, y0 + 0.3, 1.65)
        view("L0_Fosses", add_camera("CAM_L0_Fosses", loc, vertex(x1 + 0.4, y1 + 0.4, -0.8), 16.0, colls))
    if svc:
        foot = Vector(svc["foot_m"])
        rot = Matrix.Rotation(math.radians(svc["yaw_blender"]), 3, "Z")
        loc = foot + rot @ Vector((2.5, -0.55, 1.62))
        target = foot + rot @ Vector((0, 0.1, 1.05))
        cam = add_camera("CAM_L0_PorteDeService", loc, target, 17.0, colls)
        # Lampe du joueur (le jeu en donne une) : la cellule de la porte peut tomber dans une zone sans neon
        torch = bpy.data.lights.new("LT_Render_Torch", "SPOT")
        torch.energy = 30.0
        torch.spot_size = math.radians(55)
        torch.spot_blend = 0.7
        torch.color = (1.0, 0.94, 0.82)
        tob = bpy.data.objects.new(torch.name, torch)
        tob.location = loc + rot @ Vector((0, -0.25, -0.25))
        tob.rotation_euler = (target - tob.location).to_track_quat("-Z", "Y").to_euler()
        colls["L0_RenderOnly"].objects.link(tob)
        tob.hide_render = True
        view("L0_PorteDeService", cam, show=[tob], leaf_open=38.0)
    zone = 2 * L.B * L.N * S
    # Vue d'ensemble : plan vu de dessus, plafonds masques, chemin de L. et reperes en couleur, soleil de lecture
    sun = bpy.data.lights.new("LT_Render_PlanSun", "SUN")
    sun.energy = 2.2
    sun.angle = math.radians(8)
    sob = bpy.data.objects.new(sun.name, sun)
    sob.rotation_euler = (math.radians(22), math.radians(12), 0)
    colls["L0_RenderOnly"].objects.link(sob)
    plan = [sob] + plan_overlay(L, colls, M)
    for o in plan:
        o.hide_render = True
    view("L0_VueEnsemble", add_camera("CAM_L0_VueEnsemble", (0.0, 0.0, 120.0), (0.0, 0.0, 0.0), 50.0, colls, ortho=zone * 1.04), True, plan)

    report = {"blender": bpy.app.version_string, "seed": args.seed, "seed_level": L.seed, "counts": counts, "modules": len(mods),
              "archive": list(archive.keys()), "materials": len(bpy.data.materials), "images": len(bpy.data.images)}
    report["checks"] = run_checks(vp, L, mods, pit_info, svc, sc)
    print("Controles :", json.dumps({k: v for k, v in report["checks"].items() if k != "service_door"}, ensure_ascii=False)[:600])

    # Exports : FBX (modules a l'origine le temps de l'export), relecture, placement
    saved = {ob.name: ob.location.copy() for ob in mods.values()}
    for ob in mods.values():
        ob.location = (0, 0, 0)
        for u in cols.get(ob.name, []):
            u.location = (0, 0, 0)
    exported = export_fbx(mods, cols, os.path.join(out, "FBX"), out)
    arch_cols = {}
    for key, ob in archive.items():
        keep = ob.location.copy()
        ob.location = (0, 0, 0)
        arch_cols[ob.name] = []
        exported.update(export_fbx({ob.name: ob}, arch_cols, os.path.join(out, "FBX", "Archive"), out))
        ob.location = keep
    report["fbx_reimport"] = verify_fbx(exported)
    for ob in mods.values():
        ob.location = saved[ob.name]
        for u in cols.get(ob.name, []):
            u.location = ob.location
    report["fbx"] = {k: {kk: vv for kk, vv in v.items() if kk != "path"} for k, v in exported.items()}
    write_placement(os.path.join(out, "L0_Placement.json"), args, L, records, markers, svc, pit_info, counts)

    # Rendus
    setup_render(args)
    if args.render:
        report["render_seconds"] = {}
        prev = os.path.join(out, "build_report.json")
        if args.only and os.path.isfile(prev):
            with open(prev, encoding="utf-8") as fh:
                report["render_seconds"] = json.load(fh).get("render_seconds", {})
        report["render_seconds"].update(render_views(cams, os.path.join(out, "Renders"), colls, args))
    # Fichier .blend autonome : images emballees ; une copie exportable reste dans Textures/
    bpy.ops.file.pack_all()
    bpy.context.preferences.filepaths.save_version = 0  # pas de copie .blend1
    blend = os.path.join(out, "BR_Level0.blend")
    bpy.ops.wm.save_as_mainfile(filepath=blend, compress=True)
    report["blend_mb"] = round(os.path.getsize(blend) / 1e6, 1)
    report["seconds"] = round(time.time() - t_start, 1)
    with open(os.path.join(out, "build_report.json"), "w", encoding="utf-8") as fh:
        json.dump(report, fh, ensure_ascii=False, indent=1)
    print("Termine en %.0f s : %s" % (report["seconds"], out))


if __name__ == "__main__":
    main()
