"""
Import automatique des ressources du jeu Backrooms dans Unreal Engine.

Ce script est lance automatiquement au demarrage de l'editeur par init_unreal.py
si les ressources ne sont pas encore presentes (ou si elles datent d'une version precedente).
On peut aussi le relancer a la main depuis l'Output Log (onglet "Python") :

    import backrooms_setup; backrooms_setup.run(force=True)

Il importe :
  RawAssets/Textures/*.jpg|png  -> /Game/Backrooms/Textures   (les *_N sont des normal maps)
  RawAssets/Icons/*.png         -> /Game/Backrooms/UI         (icones de l'inventaire)
  RawAssets/Sounds/*.wav        -> /Game/Backrooms/Sounds     (boucles d'apres loops.txt)
  RawAssets/Meshes/*.fbx        -> /Game/Backrooms/Meshes     (Tools/Blender/generate_models.py + import_user_models.py)
puis cree les materiaux maitres (M_BR_World, M_BR_Mesh, M_BR_Skin, M_BR_Water) et la carte L_Backrooms.

Remarque : si cet import echoue, le jeu se debrouille quand meme dans l'editeur (il charge les textures
directement depuis RawAssets et construit des materiaux equivalents en C++ : voir BRMaterialBuilder.cpp).
"""
import os

import unreal

VERSION = 4

ROOT = "/Game/Backrooms"
TEX = ROOT + "/Textures"
UI = ROOT + "/UI"
SND = ROOT + "/Sounds"
MESH = ROOT + "/Meshes"
MAT = ROOT + "/Materials"
MAP_PATH = ROOT + "/Maps/L_Backrooms"
MATERIALS = ("M_BR_World", "M_BR_Mesh", "M_BR_Skin", "M_BR_Water")

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


# ---------------------------------------------------------------------------
# Utilitaires
# ---------------------------------------------------------------------------
def log(msg):
    unreal.log("[Backrooms] " + msg)


def warn(msg):
    unreal.log_warning("[Backrooms] " + msg)


def project_dir():
    return os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))


def raw(*parts):
    return os.path.join(project_dir(), "RawAssets", *parts)


def marker_path():
    return os.path.join(project_dir(), "Saved", "BackroomsSetup.txt")


def installed_version():
    try:
        with open(marker_path()) as fh:
            return int(fh.read().strip() or 0)
    except Exception:
        return 0


def write_marker():
    try:
        os.makedirs(os.path.dirname(marker_path()), exist_ok=True)
        with open(marker_path(), "w") as fh:
            fh.write(str(VERSION))
    except Exception as e:
        warn("Impossible d'ecrire %s : %s" % (marker_path(), e))


def exists(path):
    try:
        return EAL.does_asset_exist(path)
    except Exception:
        return False


def tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def list_raw(sub, exts):
    folder = raw(sub)
    if not os.path.isdir(folder):
        warn("Dossier introuvable : " + folder)
        return []
    return [f for f in sorted(os.listdir(folder)) if os.path.splitext(f)[1].lower() in exts]


def make_task(src, dest, name, options=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", src)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    if options is not None:
        t.set_editor_property("options", options)
    return t


def safe_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as e:  # propriete absente selon la version du moteur
        warn("Propriete %s ignoree (%s)" % (prop, e))
        return False


def is_normal_map(name):
    return name.endswith("_N") or "Normal" in name


# ---------------------------------------------------------------------------
# Imports
# ---------------------------------------------------------------------------
IMG_EXT = (".png", ".jpg", ".jpeg", ".tga")


def missing_in(sub, folder, exts):
    return [f for f in list_raw(sub, exts) if not exists(folder + "/" + os.path.splitext(f)[0])]


def import_textures(files, dest=TEX, ui=False):
    if not files:
        return
    sub = "Icons" if ui else "Textures"
    tasks = [make_task(raw(sub, f), dest, os.path.splitext(f)[0]) for f in files]
    tools().import_asset_tasks(tasks)
    for f in files:
        name = os.path.splitext(f)[0]
        path = dest + "/" + name
        tex = unreal.load_asset(path)
        if not tex:
            warn("Texture non importee : " + name)
            continue
        if ui:
            safe_set(tex, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            safe_set(tex, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            safe_set(tex, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            safe_set(tex, "never_stream", True)
        elif is_normal_map(name):
            safe_set(tex, "srgb", False)
            safe_set(tex, "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            safe_set(tex, "lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
        elif name == "T_LensDirt":
            safe_set(tex, "lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
        EAL.save_asset(path, only_if_is_dirty=False)
    log("%d textures importees dans %s" % (len(files), dest))


def import_sounds(files):
    if not files:
        return
    loops_file = raw("Sounds", "loops.txt")
    loops = set()
    if os.path.isfile(loops_file):
        with open(loops_file) as fh:
            loops = set(l.strip() for l in fh if l.strip())
    tasks = [make_task(raw("Sounds", f), SND, os.path.splitext(f)[0]) for f in files]
    tools().import_asset_tasks(tasks)
    for f in files:
        name = os.path.splitext(f)[0]
        path = SND + "/" + name
        snd = unreal.load_asset(path)
        if not snd:
            warn("Son non importe : " + name)
            continue
        safe_set(snd, "looping", name in loops)
        EAL.save_asset(path, only_if_is_dirty=False)
    log("%d sons importes" % len(files))


def fbx_options():
    o = unreal.FbxImportUI()
    safe_set(o, "import_mesh", True)
    safe_set(o, "import_textures", False)
    safe_set(o, "import_materials", False)
    safe_set(o, "import_as_skeletal", False)
    safe_set(o, "import_animations", False)
    safe_set(o, "automated_import_should_detect_type", False)
    safe_set(o, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = o.get_editor_property("static_mesh_import_data")
    safe_set(sm, "combine_meshes", True)
    safe_set(sm, "auto_generate_collision", True)
    safe_set(sm, "generate_lightmap_u_vs", False)
    safe_set(sm, "remove_degenerates", True)
    safe_set(sm, "compute_weighted_normals", True)
    return o


def fix_mesh_name(name):
    """Interchange peut nommer l'asset differemment : on le retrouve et on le renomme."""
    target = MESH + "/" + name
    if exists(target):
        return True
    for path in EAL.list_assets(MESH, recursive=True, include_folder=False):
        asset_name = path.split("/")[-1].split(".")[0]
        if name.lower() in asset_name.lower():
            obj = unreal.load_asset(path)
            if isinstance(obj, unreal.StaticMesh) and EAL.rename_asset(path, target):
                return True
    warn("Maillage introuvable apres import : " + name)
    return False


def import_meshes(files):
    if not files:
        return
    tasks = [make_task(raw("Meshes", f), MESH, os.path.splitext(f)[0], fbx_options()) for f in files]
    tools().import_asset_tasks(tasks)
    for f in files:
        fix_mesh_name(os.path.splitext(f)[0])
    EAL.save_directory(MESH, only_if_is_dirty=True, recursive=True)
    log("%d modeles importes" % len(files))


def delete_obsolete_meshes():
    """Pieces de la v1 remplacees par le squelette articule de la v2 ; combinaison d'un seul bloc remplacee en v3
    par les pieces articulees du modele fourni (SM_Hazmat_*)"""
    for name in ("SM_Faceling_Arm", "SM_Faceling_Leg", "SM_Hound_Leg", "SM_Partygoer_Arm", "SM_Partygoer_Leg",
                 "SM_SkinStealer_Arm", "SM_SkinStealer_Leg", "SM_Wretch_Arm", "SM_Wretch_Leg", "SM_Hazmat"):
        if name == "SM_Hazmat" and not os.path.exists(raw("Meshes", "SM_Hazmat_Torso.fbx")):
            continue
        path = MESH + "/" + name
        if exists(path):
            try:
                EAL.delete_asset(path)
            except Exception as e:
                warn("Suppression impossible %s : %s" % (path, e))


# ---------------------------------------------------------------------------
# Materiaux : petit "langage" pour construire des graphes de noeuds
# (le meme graphe est construit en C++ par BRMaterialBuilder.cpp si ce script echoue)
# ---------------------------------------------------------------------------
class Graph(object):
    def __init__(self, material):
        self.m = material
        self.n = 0

    def _pos(self):
        self.n += 1
        return (-2600 + (self.n // 24) * 260, -1200 + (self.n % 24) * 110)

    def node(self, cls):
        x, y = self._pos()
        return MEL.create_material_expression(self.m, cls, x, y)

    @staticmethod
    def pin(p):
        return p if isinstance(p, tuple) else (p, "")

    def link(self, src, dst, inp):
        e, out = self.pin(src)
        for o in ([out, ""] if out else [""]):
            try:
                if MEL.connect_material_expressions(e, o, dst, inp):
                    return True
            except Exception:
                pass
        warn("Liaison impossible : %s.%s -> %s.%s" % (e.get_name(), out, dst.get_name(), inp))
        return False

    def output(self, src, prop):
        e, out = self.pin(src)
        for o in ([out, ""] if out else [""]):
            try:
                if MEL.connect_material_property(e, o, prop):
                    return True
            except Exception:
                pass
        warn("Liaison impossible vers %s" % prop)
        return False

    # --- feuilles
    def scalar(self, name, value):
        e = self.node(unreal.MaterialExpressionScalarParameter)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", value)
        return e

    def vector(self, name, rgba):
        e = self.node(unreal.MaterialExpressionVectorParameter)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", unreal.LinearColor(*rgba))
        return e

    def const(self, v):
        e = self.node(unreal.MaterialExpressionConstant)
        e.set_editor_property("r", v)
        return e

    def c2(self, r, g):
        e = self.node(unreal.MaterialExpressionConstant2Vector)
        e.set_editor_property("r", r)
        e.set_editor_property("g", g)
        return e

    def c3(self, r, g, b):
        e = self.node(unreal.MaterialExpressionConstant3Vector)
        e.set_editor_property("constant", unreal.LinearColor(r, g, b, 1.0))
        return e

    def world_pos(self):
        return self.node(unreal.MaterialExpressionWorldPosition)

    def custom(self, description, code, inputs, output="CMOT_FLOAT3"):
        """Noeud HLSL "Custom" ; inputs = [(nom, noeud), ...]"""
        e = self.node(unreal.MaterialExpressionCustom)
        safe_set(e, "description", description)
        e.set_editor_property("code", code)
        safe_set(e, "output_type", getattr(unreal.CustomMaterialOutputType, output))
        ins = []
        for name, _src in inputs:
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", name)
            ins.append(ci)
        e.set_editor_property("inputs", ins)
        for name, src in inputs:
            self.link(src, e, name)
        return e

    def vertex_normal(self):
        return self.node(unreal.MaterialExpressionVertexNormalWS)

    def time(self):
        return self.node(unreal.MaterialExpressionTime)

    def uv0(self):
        return self.node(unreal.MaterialExpressionTextureCoordinate)

    def tex(self, name, texture, uv, normal=False, out="RGB"):
        e = self.node(unreal.MaterialExpressionTextureSampleParameter2D)
        e.set_editor_property("parameter_name", name)
        if texture:
            e.set_editor_property("texture", texture)
        e.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal
                              else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        self.link(uv, e, "UVs")
        return (e, out)

    # --- operations
    def mask(self, src, channels):
        e = self.node(unreal.MaterialExpressionComponentMask)
        for key in "rgba":
            e.set_editor_property(key, key in channels)
        self.link(src, e, "")
        return e

    def _bin(self, cls, a, b):
        e = self.node(cls)
        self.link(a, e, "A")
        self.link(b, e, "B")
        return e

    def mul(self, a, b):
        return self._bin(unreal.MaterialExpressionMultiply, a, b)

    def div(self, a, b):
        return self._bin(unreal.MaterialExpressionDivide, a, b)

    def add(self, a, b):
        return self._bin(unreal.MaterialExpressionAdd, a, b)

    def sub(self, a, b):
        return self._bin(unreal.MaterialExpressionSubtract, a, b)

    def min(self, a, b):
        return self._bin(unreal.MaterialExpressionMin, a, b)

    def dot(self, a, b):
        return self._bin(unreal.MaterialExpressionDotProduct, a, b)

    def append(self, a, b):
        return self._bin(unreal.MaterialExpressionAppendVector, a, b)

    def _un(self, cls, a):
        e = self.node(cls)
        self.link(a, e, "")
        return e

    def abs(self, a):
        return self._un(unreal.MaterialExpressionAbs, a)

    def sat(self, a):
        return self._un(unreal.MaterialExpressionSaturate, a)

    def sine(self, a):
        e = self._un(unreal.MaterialExpressionSine, a)
        e.set_editor_property("period", 6.283185)  # sin(x) et non sin(2 pi x)
        return e

    def lerp(self, a, b, alpha):
        e = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, e, "A")
        self.link(b, e, "B")
        self.link(alpha, e, "Alpha")
        return e


def new_material(name):
    path = MAT + "/" + name
    if exists(path):
        EAL.delete_asset(path)
    m = tools().create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())
    # Le monde est construit en instances (murs, sols, accessoires) : sans ce drapeau, materiau par defaut !
    safe_set(m, "used_with_instanced_static_meshes", True)
    return m


def load_tex(name, folder=TEX):
    path = folder + "/" + name
    return unreal.load_asset(path) if exists(path) else None


def finish_material(m):
    try:
        MEL.layout_material_expressions(m)
    except Exception:
        pass
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m, only_if_is_dirty=False)


def build_world_material():
    """Murs, sols, plafonds : projection triplanaire dans l'espace monde (jamais de texture etiree),
    normal maps, salete a grande echelle, salete au pied des murs, caustiques animees (Poolrooms)."""
    m = new_material("M_BR_World")
    g = Graph(m)
    P = unreal.MaterialProperty

    wp = g.world_pos()
    d = g.div(wp, g.scalar("TexScale", 200.0))
    flip = g.c2(1.0, -1.0)
    uvx = g.mul(g.mask(d, "gb"), flip)
    uvy = g.mul(g.mask(d, "rb"), flip)
    uvz = g.mask(d, "rg")

    a = g.abs(g.vertex_normal())
    a2 = g.mul(a, a)
    w4 = g.mul(a2, a2)
    wn = g.div(w4, g.dot(w4, g.c3(1.0, 1.0, 1.0)))
    wx, wy, wz = g.mask(wn, "r"), g.mask(wn, "g"), g.mask(wn, "b")

    def tri(name, texture, normal=False):
        sx = g.tex(name, texture, uvx, normal)
        sy = g.tex(name, texture, uvy, normal)
        sz = g.tex(name, texture, uvz, normal)
        return g.add(g.add(g.mul(sx, wx), g.mul(sy, wy)), g.mul(sz, wz))

    col = tri("BaseTex", load_tex("T_L0_Wallpaper"))

    # Salete a grande echelle (casse la repetition)
    grime_tex = load_tex("T_Grime")
    guv = g.div(g.append(g.dot(wp, g.c3(0.7, 0.3, 0.0)), g.dot(wp, g.c3(0.0, 0.5, 1.0))), g.scalar("GrimeScale", 900.0))
    gs = g.tex("GrimeTex", grime_tex, guv, out="R")
    col = g.mul(col, g.lerp(g.const(1.0), gs, g.scalar("Grime", 0.35)))

    # Salete au pied des murs
    z = g.mask(wp, "b")
    wall = g.sat(g.sub(g.const(1.0), g.mul(g.mask(a, "b"), g.const(1.5))))
    h = g.sat(g.sub(g.const(1.0), g.div(z, g.const(70.0))))
    fuv = g.div(g.append(g.dot(wp, g.c3(1.0, 1.0, 0.0)), g.mul(z, g.const(2.0))), g.const(180.0))
    fn = g.tex("GrimeTex", grime_tex, fuv, out="G")
    fg = g.mul(g.mul(g.scalar("FloorGrime", 0.0), wall), g.mul(h, h))
    fmod = g.sub(g.const(1.0), g.mul(fg, g.add(g.const(0.45), g.mul(fn, g.const(0.55)))))
    col = g.mul(col, fmod)

    # Caustiques (reflets de l'eau sur le carrelage)
    caus_tex = load_tex("T_Caustics")
    t = g.time()
    cuv = g.lerp(g.append(g.dot(wp, g.c3(1.0, 1.0, 0.0)), z), g.mask(wp, "rg"), wz)
    k1 = g.tex("CausticsTex", caus_tex, g.add(g.div(cuv, g.const(260.0)), g.mul(t, g.c2(0.031, 0.017))), out="R")
    k2 = g.tex("CausticsTex", caus_tex, g.add(g.mul(cuv, g.c2(-1.0 / 330.0, 1.0 / 330.0)), g.mul(t, g.c2(-0.022, 0.026))), out="R")
    k = g.min(k1, k2)
    fade = g.lerp(g.sat(g.sub(g.const(1.0), g.div(g.abs(g.sub(z, g.const(45.0))), g.const(260.0)))), g.const(1.0), wz)
    cm = g.add(g.const(1.0), g.mul(g.mul(g.scalar("Caustics", 0.0), fade), g.sub(g.mul(k, g.const(2.2)), g.const(0.35))))
    col = g.mul(col, cm)

    base = g.mul(col, g.vector("Tint", (1, 1, 1, 1)))
    g.output(base, P.MP_BASE_COLOR)

    # Rugosite variable (zones plus lisses / plus mates)
    rv = g.tex("GrimeTex", grime_tex, g.mul(guv, g.const(3.1)), out="B")
    rough = g.sat(g.add(g.scalar("Roughness", 0.85), g.mul(g.sub(rv, g.const(0.5)), g.const(0.3))))
    g.output(rough, P.MP_ROUGHNESS)
    g.output(g.scalar("Metallic", 0.0), P.MP_METALLIC)
    g.output(g.add(g.mul(base, g.scalar("SelfIllum", 0.0)), g.vector("Emissive", (0, 0, 0, 1))), P.MP_EMISSIVE_COLOR)

    # Normal maps (espace tangent, melange triplanaire)
    nrm = tri("NormalTex", load_tex("T_FlatNormal"), normal=True)
    g.output(g.lerp(g.c3(0.0, 0.0, 1.0), nrm, g.scalar("NormalStrength", 1.0)), P.MP_NORMAL)
    finish_material(m)
    return m


def build_mesh_material(name="M_BR_Mesh", skin=False):
    """Modeles Blender (UV en metres). La variante "Skin" utilise le modele d'eclairage Subsurface (peau, chair)."""
    m = new_material(name)
    g = Graph(m)
    P = unreal.MaterialProperty
    if skin:
        safe_set(m, "shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE)
    uv = g.mul(g.uv0(), g.scalar("TexScale", 1.0))
    t = g.tex("BaseTex", load_tex("T_Skin" if skin else "T_Grime"), uv)
    base = g.mul(t, g.vector("Tint", (1, 1, 1, 1)))
    g.output(base, P.MP_BASE_COLOR)
    g.output(g.scalar("Roughness", 0.6 if skin else 0.85), P.MP_ROUGHNESS)
    g.output(g.scalar("Metallic", 0.0), P.MP_METALLIC)
    g.output(g.add(g.mul(base, g.scalar("SelfIllum", 0.0)), g.vector("Emissive", (0, 0, 0, 1))), P.MP_EMISSIVE_COLOR)
    if skin:
        n = g.tex("NormalTex", load_tex("T_Skin_N"), uv, normal=True)
        g.output(g.lerp(g.c3(0.0, 0.0, 1.0), n, g.scalar("NormalStrength", 0.8)), P.MP_NORMAL)
        g.output(g.mul(base, g.vector("SubsurfaceColor", (1.0, 0.35, 0.25, 1))), P.MP_SUBSURFACE_COLOR)
        g.output(g.scalar("Subsurface", 0.6), P.MP_OPACITY)
    finish_material(m)
    return m


# Surface de l'eau (meme code que BRMaterialBuilder::WaterSurfaceHLSL en C++).
# Entrees : P (XY monde, cm), T (temps, s), Amp (houle), Chop (clapot), R0..R7 (ondes : x, y, rayon, amplitude).
# Sortie : float3(pente X, pente Y, hauteur de la houle).
NUM_RIPPLES = 8
BR_WATER_SURFACE_HLSL = (
    "float3 acc = float3(0.0, 0.0, 0.0);\n"
    "float2 dir; float k; float ph;\n"
    "// Houle lente : deplace la surface (la grille d'eau a un sommet par metre)\n"
    "dir = float2(0.8, 0.6); k = 6.2831853 / 620.0; ph = dot(P, dir) * k + T * 0.8;\n"
    "acc += float3(cos(ph) * 1.3 * k * dir, sin(ph) * 1.3) * Amp;\n"
    "dir = float2(-0.6, 0.8); k = 6.2831853 / 470.0; ph = dot(P, dir) * k + T * 1.05;\n"
    "acc += float3(cos(ph) * 0.8 * k * dir, sin(ph) * 0.8) * Amp;\n"
    "// Clapot : vagues courtes qui ne font que plier les reflets\n"
    "float2 sl = float2(0.0, 0.0);\n"
    "dir = float2(0.8, 0.6); k = 6.2831853 / 340.0; sl += cos(dot(P, dir) * k + T * 1.1) * 0.9 * k * dir;\n"
    "dir = float2(-0.5, 0.866); k = 6.2831853 / 210.0; sl += cos(dot(P, dir) * k + T * 1.6) * 0.55 * k * dir;\n"
    "dir = float2(0.2, -0.98); k = 6.2831853 / 130.0; sl += cos(dot(P, dir) * k + T * 2.3) * 0.3 * k * dir;\n"
    "dir = float2(-0.94, -0.34); k = 6.2831853 / 75.0; sl += cos(dot(P, dir) * k + T * 3.1) * 0.16 * k * dir;\n"
    "dir = float2(0.57, -0.82); k = 6.2831853 / 46.0; sl += cos(dot(P, dir) * k + T * 4.2) * 0.08 * k * dir;\n"
    "acc.xy += sl * Amp * Chop;\n"
    "// Ondes circulaires (pas du joueur, nage, gouttes) : front gaussien qui s'eloigne en s'elargissant\n"
    "float4 R[8] = { R0, R1, R2, R3, R4, R5, R6, R7 };\n"
    "[unroll] for (int i = 0; i < 8; i++)\n"
    "{\n"
    "  float4 r = R[i];\n"
    "  if (r.w > 0.0005)\n"
    "  {\n"
    "    float2 d2 = P - r.xy;\n"
    "    float d = max(length(d2), 0.5);\n"
    "    float x = d - r.z;\n"
    "    float w = 18.0 + 0.15 * r.z;\n"
    "    float kk = 6.2831853 / 22.0;\n"
    "    float env = exp(-(x * x) / (w * w));\n"
    "    float dh = r.w * env * (kk * cos(kk * x) - 2.0 * x / (w * w) * sin(kk * x));\n"
    "    acc.xy += dh * d2 / d;\n"
    "  }\n"
    "}\n"
    "return acc;\n")


def build_water_material():
    """Eau "Single Layer Water" : vagues par World Position Offset, petites rides (normal maps qui defilent),
    absorption et diffusion de la lumiere dans l'eau, reflets Lumen / ray tracing."""
    m = new_material("M_BR_Water")
    g = Graph(m)
    P = unreal.MaterialProperty
    safe_set(m, "shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    safe_set(m, "blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    wp = g.world_pos()
    xy = g.mask(wp, "rg")
    t = g.time()

    # Surface : houle (deplace la surface), clapot (normales) et ondes circulaires autour du joueur
    inputs = [("P", xy), ("T", t), ("Amp", g.scalar("WaveAmplitude", 1.0)), ("Chop", g.scalar("WaveChop", 1.0))]
    for i in range(NUM_RIPPLES):
        inputs.append(("R%d" % i, g.vector("Ripple%d" % i, (0.0, 0.0, 0.0, 0.0))))
    surface = g.custom("BRWaterSurface", BR_WATER_SURFACE_HLSL, inputs)
    h = g.mask(surface, "b")
    slope = g.mask(surface, "rg")
    g.output(g.append(g.c2(0.0, 0.0), h), P.MP_WORLD_POSITION_OFFSET)

    # Rides de detail
    ntex = load_tex("T_WaterNormal")
    scale = g.scalar("TexScale", 300.0)
    uva = g.add(g.div(xy, scale), g.mul(t, g.c2(0.012, 0.008)))
    uvb = g.add(g.mul(g.div(xy, scale), g.c2(-1.6, 1.6)), g.mul(t, g.c2(-0.01, 0.014)))
    na = g.tex("NormalTex", ntex, uva, normal=True)
    nb = g.tex("NormalTex", ntex, uvb, normal=True)
    detail = g.mul(g.mask(g.add(na, nb), "rg"), g.scalar("NormalStrength", 0.35))
    nxy = g.sub(detail, slope)
    g.output(g.append(nxy, g.const(1.0)), P.MP_NORMAL)

    g.output(g.c3(0.0, 0.0, 0.0), P.MP_BASE_COLOR)
    g.output(g.scalar("Roughness", 0.04), P.MP_ROUGHNESS)
    g.output(g.const(0.5), P.MP_SPECULAR)

    # Proprietes optiques (coefficients exprimes par metre, convertis en 1/cm)
    tint = g.vector("Tint", (0.35, 0.75, 0.8, 1))
    absorb = g.mul(g.add(g.mul(g.sub(g.c3(1.0, 1.0, 1.0), tint), g.scalar("Absorption", 1.2)), g.c3(0.02, 0.02, 0.02)), g.const(0.01))
    scatter = g.mul(g.mul(tint, g.scalar("Scattering", 0.15)), g.const(0.01))
    out = g.node(unreal.MaterialExpressionSingleLayerWaterMaterialOutput)
    for src, names in ((scatter, ("Scattering Coefficients", "ScatteringCoefficients")),
                       (absorb, ("Absorption Coefficients", "AbsorptionCoefficients")),
                       (g.const(0.1), ("Phase G", "PhaseG")),
                       (g.const(1.0), ("Color Scale Behind Water", "ColorScaleBehindWater"))):
        ok = False
        for nm in names:
            try:
                if MEL.connect_material_expressions(src, "", out, nm):
                    ok = True
                    break
            except Exception:
                pass
        if not ok:
            warn("Sortie Single Layer Water non reliee : " + names[0])
    finish_material(m)
    return m


def materials_missing():
    return [n for n in MATERIALS if not exists(MAT + "/" + n)]


def build_materials():
    for fn in (build_world_material, build_mesh_material, lambda: build_mesh_material("M_BR_Skin", skin=True),
               build_water_material):
        try:
            fn()
        except Exception as e:
            unreal.log_error("[Backrooms] Echec de creation d'un materiau : %s" % e)


# ---------------------------------------------------------------------------
# Carte
# ---------------------------------------------------------------------------
def create_map():
    if exists(MAP_PATH):
        return
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    ok = False
    try:
        ok = les.new_level(MAP_PATH)
    except Exception as e:
        warn("new_level : %s" % e)
    if not ok:
        try:
            ok = unreal.EditorLevelLibrary.new_level(MAP_PATH)
        except Exception as e:
            warn("EditorLevelLibrary.new_level : %s" % e)
    if ok:
        les.save_current_level()
        log("Carte creee : " + MAP_PATH)


# ---------------------------------------------------------------------------
# Point d'entree
# ---------------------------------------------------------------------------
def needs_setup():
    if installed_version() < VERSION:
        return True
    return bool(materials_missing() or missing_in("Textures", TEX, IMG_EXT) or missing_in("Icons", UI, (".png",))
                or missing_in("Sounds", SND, (".wav",)) or missing_in("Meshes", MESH, (".fbx",)) or not exists(MAP_PATH))


def run(force=False):
    # Changement de version : tout est reimporte (les modeles et textures ont change)
    if installed_version() < VERSION:
        force = True
    tex = list_raw("Textures", IMG_EXT) if force else missing_in("Textures", TEX, IMG_EXT)
    ico = list_raw("Icons", (".png",)) if force else missing_in("Icons", UI, (".png",))
    snd = list_raw("Sounds", (".wav",)) if force else missing_in("Sounds", SND, (".wav",))
    msh = list_raw("Meshes", (".fbx",)) if force else missing_in("Meshes", MESH, (".fbx",))
    mats = list(MATERIALS) if force else materials_missing()
    log("Installation v%d : %d textures, %d icones, %d sons, %d modeles, %d materiaux"
        % (VERSION, len(tex), len(ico), len(snd), len(msh), len(mats)))
    with unreal.ScopedSlowTask(6, "The Backrooms : import des ressources...") as task:
        task.make_dialog(True)
        task.enter_progress_frame(1, "Textures (%d)" % len(tex))
        import_textures(tex)
        task.enter_progress_frame(1, "Icones (%d)" % len(ico))
        import_textures(ico, UI, ui=True)
        task.enter_progress_frame(1, "Sons (%d)" % len(snd))
        import_sounds(snd)
        task.enter_progress_frame(1, "Modeles Blender (%d)" % len(msh))
        delete_obsolete_meshes()
        import_meshes(msh)
        task.enter_progress_frame(1, "Materiaux")
        if mats or tex:
            build_materials()
        task.enter_progress_frame(1, "Carte")
        create_map()
    if not materials_missing():
        write_marker()
    log("Installation terminee. Appuyez sur Play !")
    try:
        unreal.EditorDialog.show_message(
            "The Backrooms",
            "Les ressources du jeu ont ete importees (v%d).\n\nAppuyez sur Play (Alt+P) pour noclipper dans le Niveau 0.\n"
            "TAB : inventaire (onglet TOUCHES : toutes les touches se reconfigurent)  -  V : 3e personne  -  "
            "N : vision nocturne  -  F : lampe" % VERSION,
            unreal.AppMsgType.OK)
    except Exception:
        pass


if __name__ == "__main__":
    run(force=False)
