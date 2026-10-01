"""
Import automatique des ressources du jeu Backrooms dans Unreal Engine.

Ce script est lance automatiquement au demarrage de l'editeur par init_unreal.py
si les ressources ne sont pas encore presentes. On peut aussi le relancer a la main
depuis l'Output Log (onglet "Python") :

    import backrooms_setup; backrooms_setup.run(force=True)

Il importe :
  RawAssets/Textures/*.jpg|png  -> /Game/Backrooms/Textures
  RawAssets/Sounds/*.wav        -> /Game/Backrooms/Sounds   (boucles d'apres loops.txt)
  RawAssets/Meshes/*.fbx        -> /Game/Backrooms/Meshes   (generes par Tools/Blender/generate_models.py)
puis cree les materiaux maitres (M_BR_World, M_BR_Mesh, M_BR_Water) et la carte L_Backrooms.
"""
import os

import unreal

ROOT = "/Game/Backrooms"
TEX = ROOT + "/Textures"
SND = ROOT + "/Sounds"
MESH = ROOT + "/Meshes"
MAT = ROOT + "/Materials"
MAP_PATH = ROOT + "/Maps/L_Backrooms"
MATERIALS = ("M_BR_World", "M_BR_Mesh", "M_BR_Water")

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


# ---------------------------------------------------------------------------
# Imports
# ---------------------------------------------------------------------------
def missing_textures():
    return [f for f in list_raw("Textures", (".png", ".jpg", ".jpeg", ".tga")) if not exists(TEX + "/" + os.path.splitext(f)[0])]


def missing_sounds():
    return [f for f in list_raw("Sounds", (".wav",)) if not exists(SND + "/" + os.path.splitext(f)[0])]


def missing_meshes():
    return [f for f in list_raw("Meshes", (".fbx",)) if not exists(MESH + "/" + os.path.splitext(f)[0])]


def import_textures(files, task=None):
    if not files:
        return
    tasks = [make_task(raw("Textures", f), TEX, os.path.splitext(f)[0]) for f in files]
    tools().import_asset_tasks(tasks)
    for f in files:
        name = os.path.splitext(f)[0]
        path = TEX + "/" + name
        tex = unreal.load_asset(path)
        if not tex:
            warn("Texture non importee : " + name)
            continue
        if "Normal" in name:
            safe_set(tex, "srgb", False)
            safe_set(tex, "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        EAL.save_asset(path, only_if_is_dirty=False)
    log("%d textures importees" % len(files))


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


# ---------------------------------------------------------------------------
# Materiaux
# ---------------------------------------------------------------------------
def new_material(name):
    path = MAT + "/" + name
    if exists(path):
        EAL.delete_asset(path)
    return tools().create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())


def node(m, cls, x, y):
    return MEL.create_material_expression(m, cls, x, y)


def link(a, out, b, inp):
    outs = [out, ""] if out else [""]
    for o in outs:
        try:
            if MEL.connect_material_expressions(a, o, b, inp):
                return True
        except Exception:
            pass
    warn("Liaison impossible : %s.%s -> %s.%s" % (a.get_name(), out, b.get_name(), inp))
    return False


def to_prop(a, out, prop):
    for o in ([out, ""] if out else [""]):
        try:
            if MEL.connect_material_property(a, o, prop):
                return True
        except Exception:
            pass
    warn("Liaison impossible vers %s" % prop)
    return False


def scalar(m, name, value, x, y):
    e = node(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e


def vector(m, name, rgba, x, y):
    e = node(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(*rgba))
    return e


def texparam(m, name, tex, x, y, normal=False):
    e = node(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    e.set_editor_property("parameter_name", name)
    if tex:
        e.set_editor_property("texture", tex)
    if normal:
        e.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    return e


def mask(m, src, rgba, x, y):
    e = node(m, unreal.MaterialExpressionComponentMask, x, y)
    for key, val in zip(("r", "g", "b", "a"), rgba):
        e.set_editor_property(key, val)
    link(src, "", e, "")
    return e


def binop(m, cls, a, b, x, y, a_out="", b_out=""):
    e = node(m, cls, x, y)
    link(a, a_out, e, "A")
    link(b, b_out, e, "B")
    return e


def lerp(m, a, b, alpha, x, y, a_out="", b_out="", alpha_out=""):
    e = node(m, unreal.MaterialExpressionLinearInterpolate, x, y)
    link(a, a_out, e, "A")
    link(b, b_out, e, "B")
    link(alpha, alpha_out, e, "Alpha")
    return e


def const(m, value, x, y):
    e = node(m, unreal.MaterialExpressionConstant, x, y)
    e.set_editor_property("r", value)
    return e


def const2(m, r, g, x, y):
    e = node(m, unreal.MaterialExpressionConstant2Vector, x, y)
    e.set_editor_property("r", r)
    e.set_editor_property("g", g)
    return e


def const3(m, rgb, x, y):
    e = node(m, unreal.MaterialExpressionConstant3Vector, x, y)
    e.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return e


def finish_material(m):
    try:
        MEL.layout_material_expressions(m)
    except Exception:
        pass
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m, only_if_is_dirty=False)


def common_outputs(m, base, x, y):
    """Rugosite, metal, emissif communs aux materiaux opaques"""
    to_prop(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(scalar(m, "Roughness", 0.85, x, y), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(scalar(m, "Metallic", 0.0, x, y + 120), "", unreal.MaterialProperty.MP_METALLIC)
    selfi = scalar(m, "SelfIllum", 0.0, x - 200, y + 260)
    emi = vector(m, "Emissive", (0, 0, 0, 1), x - 200, y + 380)
    e1 = binop(m, unreal.MaterialExpressionMultiply, base, selfi, x, y + 260)
    e2 = binop(m, unreal.MaterialExpressionAdd, e1, emi, x + 200, y + 320)
    to_prop(e2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def build_world_material():
    """Projection triplanaire dans l'espace monde : les murs geants n'ont jamais de textures etirees."""
    m = new_material("M_BR_World")
    base_tex = unreal.load_asset(TEX + "/T_L0_Wallpaper")
    grime_tex = unreal.load_asset(TEX + "/T_Grime")

    wp = node(m, unreal.MaterialExpressionWorldPosition, -2200, 0)
    scale = scalar(m, "TexScale", 200.0, -2200, 250)
    div = binop(m, unreal.MaterialExpressionDivide, wp, scale, -2000, 0)
    m_xy = mask(m, div, (True, True, False, False), -1800, -300)
    m_xz = mask(m, div, (True, False, True, False), -1800, 0)
    m_yz = mask(m, div, (False, True, True, False), -1800, 300)
    flip = const2(m, 1.0, -1.0, -1800, 500)
    xz = binop(m, unreal.MaterialExpressionMultiply, m_xz, flip, -1600, 0)
    yz = binop(m, unreal.MaterialExpressionMultiply, m_yz, flip, -1600, 300)

    t_xy = texparam(m, "BaseTex", base_tex, -1300, -400)
    link(m_xy, "", t_xy, "UVs")
    t_xz = texparam(m, "BaseTex", base_tex, -1300, -50)
    link(xz, "", t_xz, "UVs")
    t_yz = texparam(m, "BaseTex", base_tex, -1300, 300)
    link(yz, "", t_yz, "UVs")

    nrm = node(m, unreal.MaterialExpressionVertexNormalWS, -1800, 700)
    nabs = node(m, unreal.MaterialExpressionAbs, -1600, 700)
    link(nrm, "", nabs, "")
    nx = mask(m, nabs, (True, False, False, False), -1400, 650)
    nz = mask(m, nabs, (False, False, True, False), -1400, 780)
    l1 = lerp(m, t_xz, t_yz, nx, -1000, 100, "RGB", "RGB", "")
    l2 = lerp(m, l1, t_xy, nz, -800, 0, "", "RGB", "")

    # Salete a grande echelle (casse la repetition)
    du = binop(m, unreal.MaterialExpressionDotProduct, wp, const3(m, (0.7, 0.3, 0.0), -2000, 900), -1800, 900)
    dv = binop(m, unreal.MaterialExpressionDotProduct, wp, const3(m, (0.0, 0.5, 1.0), -2000, 1050), -1800, 1050)
    app = binop(m, unreal.MaterialExpressionAppendVector, du, dv, -1600, 950)
    gdiv = binop(m, unreal.MaterialExpressionDivide, app, scalar(m, "GrimeScale", 900.0, -1600, 1100), -1400, 950)
    g = texparam(m, "GrimeTex", grime_tex, -1200, 950)
    link(gdiv, "", g, "UVs")
    gl = lerp(m, const(m, 1.0, -1000, 850), g, scalar(m, "Grime", 0.35, -1000, 1100), -800, 900, "", "R", "")

    tint = vector(m, "Tint", (1, 1, 1, 1), -800, 300)
    c1 = binop(m, unreal.MaterialExpressionMultiply, l2, tint, -600, 100)
    base = binop(m, unreal.MaterialExpressionMultiply, c1, gl, -400, 200)
    common_outputs(m, base, -200, 400)
    finish_material(m)
    return m


def build_mesh_material():
    """Materiau des modeles Blender (UV en metres)"""
    m = new_material("M_BR_Mesh")
    grime_tex = unreal.load_asset(TEX + "/T_Grime")
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    uvs = binop(m, unreal.MaterialExpressionMultiply, uv, scalar(m, "TexScale", 1.0, -1200, 150), -1000, 0)
    t = texparam(m, "BaseTex", grime_tex, -800, 0)
    link(uvs, "", t, "UVs")
    tint = vector(m, "Tint", (1, 1, 1, 1), -800, 300)
    base = binop(m, unreal.MaterialExpressionMultiply, t, tint, -500, 100, "RGB", "")
    common_outputs(m, base, -300, 300)
    finish_material(m)
    return m


def build_water_material():
    m = new_material("M_BR_Water")
    safe_set(m, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    safe_set(m, "translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    normal_tex = unreal.load_asset(TEX + "/T_WaterNormal")
    wp = node(m, unreal.MaterialExpressionWorldPosition, -1400, 0)
    xy = mask(m, wp, (True, True, False, False), -1200, 0)
    uv = binop(m, unreal.MaterialExpressionDivide, xy, scalar(m, "TexScale", 300.0, -1200, 150), -1000, 0)
    pan = node(m, unreal.MaterialExpressionPanner, -800, 0)
    safe_set(pan, "speed_x", 0.015)
    safe_set(pan, "speed_y", 0.01)
    link(uv, "", pan, "Coordinate")
    n = texparam(m, "NormalTex", normal_tex, -600, 0, normal=True)
    link(pan, "", n, "UVs")
    to_prop(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    to_prop(vector(m, "Tint", (0.35, 0.75, 0.8, 1), -400, 250), "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_prop(scalar(m, "Opacity", 0.35, -400, 400), "", unreal.MaterialProperty.MP_OPACITY)
    to_prop(scalar(m, "Roughness", 0.05, -400, 500), "", unreal.MaterialProperty.MP_ROUGHNESS)
    to_prop(const(m, 1.0, -400, 600), "", unreal.MaterialProperty.MP_SPECULAR)
    finish_material(m)
    return m


def materials_missing():
    return [n for n in MATERIALS if not exists(MAT + "/" + n)]


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
    return bool(materials_missing() or missing_textures() or missing_sounds() or missing_meshes() or not exists(MAP_PATH))


def run(force=False):
    tex = list_raw("Textures", (".png", ".jpg", ".jpeg", ".tga")) if force else missing_textures()
    snd = list_raw("Sounds", (".wav",)) if force else missing_sounds()
    msh = list_raw("Meshes", (".fbx",)) if force else missing_meshes()
    mats = list(MATERIALS) if force else materials_missing()
    steps = 5
    log("Installation : %d textures, %d sons, %d modeles, %d materiaux" % (len(tex), len(snd), len(msh), len(mats)))
    with unreal.ScopedSlowTask(steps, "The Backrooms : import des ressources...") as task:
        task.make_dialog(True)
        task.enter_progress_frame(1, "Textures (%d)" % len(tex))
        import_textures(tex)
        task.enter_progress_frame(1, "Sons (%d)" % len(snd))
        import_sounds(snd)
        task.enter_progress_frame(1, "Modeles Blender (%d)" % len(msh))
        import_meshes(msh)
        task.enter_progress_frame(1, "Materiaux")
        if mats or tex:
            build_world_material()
            build_mesh_material()
            build_water_material()
        task.enter_progress_frame(1, "Carte")
        create_map()
    log("Installation terminee. Appuyez sur Play !")
    try:
        unreal.EditorDialog.show_message(
            "The Backrooms",
            "Les ressources du jeu ont ete importees.\n\nAppuyez sur Play (Alt+P) pour noclipper dans le Niveau 0.",
            unreal.AppMsgType.OK)
    except Exception:
        pass


if __name__ == "__main__":
    run(force=False)
