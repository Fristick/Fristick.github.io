"""
Import automatique des ressources du jeu Backrooms dans Unreal Engine.

Ce script est lance automatiquement au demarrage de l'editeur par init_unreal.py
si les ressources ne sont pas encore presentes (ou si elles datent d'une version precedente).
On peut aussi le relancer a la main depuis l'Output Log (onglet "Python") :

    import backrooms_setup; backrooms_setup.run(force=True)

v4.8 : validation et reparation ciblee (rejouables, sans tout reimporter) :

    import backrooms_setup; backrooms_setup.validate(verbose=True)   # Saved/BackroomsSetup_Validation.txt
    import backrooms_setup; backrooms_setup.repair()                 # ne refait que ce qui manque

Il importe :
  RawAssets/Textures/*.jpg|png  -> /Game/Backrooms/Textures   (les *_N sont des normal maps)
  RawAssets/Icons/*.png         -> /Game/Backrooms/UI         (icones de l'inventaire)
  RawAssets/Sounds/*.wav        -> /Game/Backrooms/Sounds     (boucles d'apres loops.txt)
  RawAssets/Meshes/*.fbx        -> /Game/Backrooms/Meshes     (Tools/Blender/generate_models.py + import_user_models.py)
puis cree les materiaux maitres (M_BR_World, M_BR_Mesh, M_BR_Skin, M_BR_WaterSurface) et la carte L_Backrooms.

Remarque : si cet import echoue, le jeu se debrouille quand meme dans l'editeur (il charge les textures
directement depuis RawAssets et construit des materiaux equivalents en C++ : voir BRMaterialBuilder.cpp).
"""
import os

import unreal

VERSION = 10
# Version des materiaux maitres : quand elle change, seuls les materiaux sont reconstruits (v4.1 : flaques,
# v4.3 : anti-repetition des sols, v4.4 : echantillonneur lineaire du bruit, le materiau du monde compile de nouveau,
# v4.7 : cartes de rugosite RoughTex/RoughContrast dans le monde et les modeles,
# v4.8 : usages "maillage a squelette" et "Nanite" sur M_BR_Mesh et M_BR_Skin : sans eux, la combinaison du joueur et les
# entites SK_* recevaient le materiau par defaut, gris et sans texture, en jeu autonome et en paquet)
MATERIAL_VERSION = 7
# Textures refaites depuis une version des materiaux : reimportees avec elle, sans tout reimporter
RETEXTURED = {3: ["T_L0_Carpet.jpg", "T_L0_Carpet_N.png"]}
# Sons remplaces depuis une version des materiaux (v4.4 : cris de la Bacteria fournis, en boucle)
RESOUNDED = {4: ["S_Bacteria.wav"]}
# Modeles refaits (v4.4 : nouveau Smiler)
RESHAPED = {4: ["SM_SmilerET.fbx"], 5: ["SM_SmilerET.fbx"]}

ROOT = "/Game/Backrooms"
TEX = ROOT + "/Textures"
UI = ROOT + "/UI"
SND = ROOT + "/Sounds"
MESH = ROOT + "/Meshes"
MAT = ROOT + "/Materials"
MAP_PATH = ROOT + "/Maps/L_Backrooms"
MATERIALS = ("M_BR_World", "M_BR_Mesh", "M_BR_Skin", "M_BR_WaterSurface", "M_BR_PitShade")
# v4.8 : usages que chaque materiau maitre doit declarer (meme liste que BRMaterialBuilder::RequiredUsages en C++)
_MODEL_USAGES = ("used_with_instanced_static_meshes", "used_with_skeletal_mesh", "used_with_nanite")
USAGES = {
    "M_BR_World": ("used_with_instanced_static_meshes",),
    "M_BR_Mesh": _MODEL_USAGES,
    "M_BR_Skin": _MODEL_USAGES,
    "M_BR_WaterSurface": ("used_with_instanced_static_meshes",),
    "M_BR_PitShade": (),
}
# v4.8 : slots attendus apres l'import de chaque maillage a squelette (noms des materiaux du FBX) : la validation
# signale un slot renomme par l'importeur (le jeu affecte les materiaux par nom, jamais par indice)
EXPECTED_SLOTS = {
    "SK_Hazmat": ("HazmatMask", "HazmatGlass", "HazmatSuit"),
    "SK_Faceling": ("FacelingTex",),
    "SK_Partygoer": ("PartygoerTex",),
    "SK_SkinStealer": ("SkinStealerFlesh", "EyeDark", "StealerEye", "StealerClaw"),
    "SK_Hound": ("HoundSkin", "HoundMouth", "HoundTeeth", "HoundHair", "HoundFace", "GlowAmberEye", "EyeDark", "HoundTongue"),
    "SK_HoundLite": ("HoundSkin", "HoundMouth", "HoundTeeth", "HoundHair", "HoundFace", "GlowAmberEye", "EyeDark", "HoundTongue"),
    "SK_Wretch": ("WretchSkin", "WretchTeeth", "WretchEye"),
    "SK_Clump": ("ClumpSkin", "ClumpTeeth"),
}
# v4.8 : textures dont la combinaison du joueur depend (taille minimale attendue apres import)
HAZMAT_TEXTURES = ("T_Hazmat_Suit", "T_Hazmat_Mask")
# Materiaux des versions precedentes, supprimes a la mise a jour (l'eau "Single Layer Water" a disparu en v7)
OBSOLETE_MATERIALS = ("M_BR_Water",)

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


def _marker_values():
    try:
        with open(marker_path()) as fh:
            return [int(v) for v in fh.read().split()]
    except Exception:
        return []


def installed_version():
    v = _marker_values()
    return v[0] if v else 0


def installed_material_version():
    v = _marker_values()
    return v[1] if len(v) > 1 else (1 if v else 0)


def write_marker():
    try:
        os.makedirs(os.path.dirname(marker_path()), exist_ok=True)
        with open(marker_path(), "w") as fh:
            fh.write("%d %d" % (VERSION, MATERIAL_VERSION))
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


def is_roughness_map(name):
    """v4.7 : cartes de rugosite <Texture>_R (Tools/generate_roughness.py) : donnees lineaires"""
    return name.endswith("_R")


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
            if name.startswith("UI_"):
                # Degrades, formes arrondies, logo : pas de repetition (le filtrage ne melange pas les bords opposes)
                safe_set(tex, "address_x", unreal.TextureAddress.TA_CLAMP)
                safe_set(tex, "address_y", unreal.TextureAddress.TA_CLAMP)
        elif is_normal_map(name):
            safe_set(tex, "srgb", False)
            safe_set(tex, "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            safe_set(tex, "lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
        elif name == "T_NoiseLF":
            # Texture de donnees (bruits du materiau du monde) : valeurs lineaires
            safe_set(tex, "srgb", False)
        elif is_roughness_map(name):
            # v4.7 : rugosite relative (0,5 = valeur nominale de la surface) : lineaire, compression par defaut
            # (echantillonneur "Linear Color", comme T_NoiseLF qui sert de valeur par defaut au parametre RoughTex)
            safe_set(tex, "srgb", False)
            safe_set(tex, "lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
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


def fbx_skeletal_options():
    """v4.4 : combinaison du joueur a squelette (SK_Hazmat) ; le squelette est cree a l'import, sans animation"""
    o = unreal.FbxImportUI()
    safe_set(o, "import_mesh", True)
    safe_set(o, "import_textures", False)
    safe_set(o, "import_materials", False)
    safe_set(o, "import_as_skeletal", True)
    safe_set(o, "import_animations", False)
    safe_set(o, "create_physics_asset", False)
    safe_set(o, "automated_import_should_detect_type", False)
    safe_set(o, "mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    sk = o.get_editor_property("skeletal_mesh_import_data")
    safe_set(sk, "import_morph_targets", False)
    safe_set(sk, "convert_scene", True)
    safe_set(sk, "import_meshes_in_bone_hierarchy", True)
    return o


# v4.5 : niveaux de detail generes a l'import pour les maillages a squelette denses (le Hound garde son pelage
# d'origine, 175 000 sommets, au plus pres ; ses LOD le remplacent au loin)
# v4.6 : SK_HoundLite (45 % des meches de cheveux, -49 % de sommets) est celui des profils Performance et Qualite
SKELETAL_LODS = {"SK_Hound": 4, "SK_HoundLite": 3, "SK_SkinStealer": 3, "SK_Hazmat": 3, "SK_Wretch": 3, "SK_Clump": 3}
# v4.8 : niveau de detail utilise dans la scene ray tracee (reflets, ombres de la lampe) : les maillages a squelette sont
# reconstruits a chaque image dans les structures RT ; le pelage complet du Hound y coutait le plus cher
SKELETAL_RT_MIN_LOD = {"SK_Hound": 1, "SK_HoundLite": 1, "SK_Clump": 1, "SK_SkinStealer": 1}


def tune_skeletal():
    """Reglages des maillages a squelette deja importes (rejouable) : LOD minimal du ray tracing"""
    for name, lod in SKELETAL_RT_MIN_LOD.items():
        sk = unreal.load_asset(MESH + "/" + name) if exists(MESH + "/" + name) else None
        if not isinstance(sk, unreal.SkeletalMesh):
            continue
        try:
            if sk.get_editor_property("ray_tracing_min_lod") == lod:
                continue
        except Exception:
            pass
        if safe_set(sk, "ray_tracing_min_lod", lod):
            EAL.save_loaded_asset(sk, only_if_is_dirty=False)
            log("%s : LOD %d dans la scene ray tracee" % (name, lod))


def generate_skeletal_lods(name, count):
    sk = unreal.load_asset(MESH + "/" + name)
    if not isinstance(sk, unreal.SkeletalMesh):
        return
    done = False
    for owner, fn in ((lambda: unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem), "regenerate_lod"),
                      (lambda: unreal.EditorSkeletalMeshLibrary, "regenerate_lod")):
        try:
            done = bool(getattr(owner(), fn)(sk, count, False, False))
            if done:
                break
        except Exception:
            continue
    if done:
        EAL.save_loaded_asset(sk)
        log("%s : %d niveaux de detail" % (name, count))
    else:
        warn("%s : niveaux de detail non generes (a faire dans l'editeur : Skeletal Mesh > LOD Settings)" % name)


def import_skeletal(files):
    if not files:
        return
    tasks = [make_task(raw("Skeletal", f), MESH, os.path.splitext(f)[0], fbx_skeletal_options()) for f in files]
    tools().import_asset_tasks(tasks)
    wanted = {os.path.splitext(f)[0].lower() for f in list_raw("Skeletal", (".fbx",))}
    for f in files:
        name = os.path.splitext(f)[0]
        target = MESH + "/" + name
        if not exists(target):
            # Interchange peut nommer l'asset autrement ("SK_Hound_Armature"...) : on retrouve CE maillage a squelette
            # (nom commencant par le sien, qui n'est pas celui d'un autre fichier) et on le renomme
            for path in EAL.list_assets(MESH, recursive=True, include_folder=False):
                asset = path.split("/")[-1].split(".")[0]
                low = asset.lower()
                if low in wanted or not low.startswith(name.lower()):
                    continue
                obj = unreal.load_asset(path)
                if isinstance(obj, unreal.SkeletalMesh):
                    EAL.rename_asset(path, target)
                    break
            else:
                warn("Maillage a squelette introuvable apres import : " + name)
                continue
        if name in SKELETAL_LODS:
            generate_skeletal_lods(name, SKELETAL_LODS[name])
    EAL.save_directory(MESH, only_if_is_dirty=True, recursive=True)
    log("%d maillages a squelette importes" % len(files))


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


# Modeles fournis, gardes a pleine resolution (v3.9) : Nanite les affiche sans cout, quelle que soit leur densite
NANITE_PREFIXES = ("SM_Hazmat_", "SM_BacteriaET_", "SM_DeathmothET_", "SM_SkinStealerET_", "SM_FacelingET_", "SM_PartygoerET_",
                   "SM_HoundET_", "SM_SmilerET", "SM_ClumpET_", "SM_OfficeDeskET", "SM_OfficeChairET", "SM_WaterCoolerET")


def enable_nanite(name):
    if not name.startswith(NANITE_PREFIXES):
        return
    sm = unreal.load_asset(MESH + "/" + name)
    if not isinstance(sm, unreal.StaticMesh):
        return
    try:
        ns = sm.get_editor_property("nanite_settings")
        if ns.get_editor_property("enabled"):
            return
        ns.set_editor_property("enabled", True)
        try:
            unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(sm, ns, apply_changes=True)
        except Exception:
            sm.set_editor_property("nanite_settings", ns)
        EAL.save_loaded_asset(sm)
    except Exception as e:
        warn("Nanite non active pour %s : %s" % (name, e))


def import_meshes(files):
    if not files:
        return
    tasks = [make_task(raw("Meshes", f), MESH, os.path.splitext(f)[0], fbx_options()) for f in files]
    tools().import_asset_tasks(tasks)
    for f in files:
        fix_mesh_name(os.path.splitext(f)[0])
    for f in files:
        enable_nanite(os.path.splitext(f)[0])
    EAL.save_directory(MESH, only_if_is_dirty=True, recursive=True)
    log("%d modeles importes" % len(files))


def delete_obsolete_meshes():
    """Pieces de la v1 remplacees par le squelette articule de la v2 ; combinaison d'un seul bloc remplacee en v3
    par les pieces articulees du modele fourni (SM_Hazmat_*)"""
    # v3.5 : entites refaites a partir des modeles / images fournis (SM_*ET_*) -> anciennes pieces procedurales
    replaced = {
        "SM_HoundET_Body": ["SM_Hound_Body", "SM_Hound_Head", "SM_Hound_UpperLeg", "SM_Hound_LowerLeg"],
        "SM_FacelingET_Torso": ["SM_Faceling_" + p for p in ("Torso", "Head", "UpperArm", "LowerArm", "Thigh", "Shin")],
        "SM_PartygoerET_Torso": ["SM_Partygoer_" + p for p in ("Torso", "Head", "UpperArm", "LowerArm", "Thigh", "Shin", "Balloon")],
        "SM_SkinStealerET_Torso": ["SM_SkinStealer_" + p for p in ("Torso", "Head", "UpperArm", "LowerArm", "Thigh", "Shin")],
        "SM_SmilerET": ["SM_Smiler"],
        "SM_ClumpET_Core": ["SM_Clump"],
    }
    for marker, olds in replaced.items():
        if not os.path.exists(raw("Meshes", marker + ".fbx")):
            continue
        for name in olds:
            path = MESH + "/" + name
            if not os.path.exists(raw("Meshes", name + ".fbx")) and exists(path):
                try:
                    EAL.delete_asset(path)
                except Exception as e:
                    warn("Suppression impossible %s : %s" % (path, e))
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
                    if out and not o:
                        warn("Sortie %s introuvable sur %s : sortie par defaut utilisee" % (out, e.get_name()))
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

    def rt_switch(self, normal, raytraced):
        """v4.8 : version simplifiee pour les rayons (reflets eclaires par les rayons, ombres ray tracees) : le noeud
        RayTracingQualitySwitch garde le calcul complet a l'ecran et le cout minimal dans la scene ray tracee"""
        cls = getattr(unreal, "MaterialExpressionRayTracingQualitySwitch", None)
        if cls is None:
            return normal
        e = self.node(cls)
        if not (self.link(normal, e, "Normal") and self.link(raytraced, e, "RayTraced")):
            return normal
        return e

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

    def vector4(self, name, rgba):
        """Parametre vectoriel lu en float4 (sortie RGBA) : la sortie par defaut (RGB) perd la 4e composante"""
        return (self.vector(name, rgba), "RGBA")

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

    def texobj(self, name, texture):
        """Texture passee telle quelle a un noeud Custom (echantillonnee dans le HLSL : Nom, NomSampler)"""
        e = self.node(unreal.MaterialExpressionTextureObjectParameter)
        e.set_editor_property("parameter_name", name)
        if texture:
            e.set_editor_property("texture", texture)
        # meme regle que tex() : texture lineaire (T_NoiseLF) -> echantillonneur LinearColor, sinon le materiau ne compile pas
        linear = texture is not None and not is_srgb(texture)
        safe_set(e, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if linear else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        return e

    def _scene(self, cls, offset):
        e = self.node(cls)
        e.set_editor_property("input_mode", unreal.MaterialSceneAttributeInputMode.OFFSET_FRACTION)
        if offset is not None:
            self.link(offset, e, "")
        return e

    def scene_color(self, offset=None):
        """Image de la scene derriere un materiau translucide, decalee de offset (fraction de l'ecran)"""
        return self._scene(unreal.MaterialExpressionSceneColor, offset)

    def scene_depth(self, offset=None):
        """Profondeur de la scene opaque (cm), decalee de offset"""
        return self._scene(unreal.MaterialExpressionSceneDepth, offset)

    def world_to_view(self, a):
        """Vecteur du monde exprime dans l'espace de la camera (X a droite, Y en haut)"""
        e = self.node(unreal.MaterialExpressionTransform)
        e.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
        e.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_VIEW)
        self.link(a, e, "")
        return e

    def time(self):
        return self.node(unreal.MaterialExpressionTime)

    def uv0(self):
        return self.node(unreal.MaterialExpressionTextureCoordinate)

    def tex(self, name, texture, uv, normal=False, out="RGB"):
        e = self.node(unreal.MaterialExpressionTextureSampleParameter2D)
        e.set_editor_property("parameter_name", name)
        if texture:
            e.set_editor_property("texture", texture)
        # Le type d'echantillonneur doit suivre la texture : une texture lineaire (bruits, donnees) lue en "Color"
        # empeche le materiau de compiler (v4.3 : plus aucune texture a l'ecran)
        st = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
        if normal:
            st = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
        elif texture and not is_srgb(texture):
            st = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
        e.set_editor_property("sampler_type", st)
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
    # v4.8 : les modeles aussi sont des maillages a squelette (combinaison, entites) et des maillages Nanite
    for prop in USAGES.get(name, ("used_with_instanced_static_meshes",)):
        safe_set(m, prop, True)
    return m


def is_srgb(texture):
    try:
        return bool(texture.get_editor_property("srgb"))
    except Exception:
        return True


def load_tex(name, folder=TEX):
    path = folder + "/" + name
    return unreal.load_asset(path) if exists(path) else None


def black_tex():
    """Texture par defaut de la simulation de l'eau (remplacee en jeu par celle de UBRWaterSim)"""
    return unreal.load_asset("/Engine/EngineResources/Black") or unreal.load_asset("/Engine/EngineResources/DefaultTexture")


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

    # v4.3 : anti-repetition des sols et plafonds (AntiTile) : un 2e echantillon, tourne de 37 degres et a une autre
    # echelle, se melange au premier selon un bruit a grande echelle (13 m) ; les motifs ne s'alignent plus
    grime_tex = load_tex("T_Grime")
    noise_tex = load_tex("T_NoiseLF") or grime_tex
    anti = g.scalar("AntiTile", 0.0)
    uvz2 = g.add(g.mul(g.append(g.dot(uvz, g.c2(0.8, -0.6)), g.dot(uvz, g.c2(0.6, 0.8))), g.const(0.77)), g.c2(0.37, 0.61))
    xy = g.mask(wp, "rg")
    mix_noise = g.tex("NoiseTex", noise_tex, g.div(xy, g.const(1300.0)), out="B")
    mixz = g.mul(g.sat(g.div(g.sub(mix_noise, g.const(0.45)), g.const(0.1))), anti)

    def tri(name, texture, normal=False):
        sx = g.tex(name, texture, uvx, normal)
        sy = g.tex(name, texture, uvy, normal)
        sz1 = g.tex(name, texture, uvz, normal)
        sz2 = g.tex(name, texture, uvz2, normal)
        if normal:
            # Pente du 2e echantillon ramenee dans le repere des UV d'origine (rotation inverse)
            nxy = g.mask(sz2, "rg")
            sz2 = g.append(g.append(g.dot(nxy, g.c2(0.8, 0.6)), g.dot(nxy, g.c2(-0.6, 0.8))), g.mask(sz2, "b"))
        sz = g.lerp(sz1, sz2, mixz)
        return g.add(g.add(g.mul(sx, wx), g.mul(sy, wy)), g.mul(sz, wz))

    col = tri("BaseTex", load_tex("T_L0_Wallpaper"))

    # v4.3 : variation de teinte a grande echelle (sols), et taches d'humidite dessinees a l'echelle du monde (Stains)
    macro_uv = g.div(g.append(g.dot(xy, g.c2(0.6, 0.8)), g.dot(xy, g.c2(-0.8, 0.6))), g.const(2300.0))
    macro = g.tex("NoiseTex", noise_tex, macro_uv, out="G")
    col = g.mul(col, g.add(g.const(1.0), g.mul(g.mul(g.sub(macro, g.const(0.5)), g.const(0.45)), g.mul(anti, wz))))
    stain_uv = g.add(g.div(g.append(g.dot(xy, g.c2(0.92, -0.39)), g.dot(xy, g.c2(0.39, 0.92))), g.scalar("StainScale", 1600.0)),
                     g.c2(0.21, 0.53))
    stain_noise = g.tex("NoiseTex", noise_tex, stain_uv, out="R")
    stain = g.mul(g.sat(g.div(g.sub(stain_noise, g.const(0.7)), g.const(0.12))), g.mul(g.scalar("Stains", 0.0), wz))
    col = g.mul(col, g.lerp(g.c3(1.0, 1.0, 1.0), g.c3(0.7, 0.7, 0.56), stain))

    # v4.7 : murs : variation de teinte et aureoles d'humidite a l'echelle du monde (17 m) : le papier peint se
    # repete tous les 1,2 m, ces taches non ; les aureoles sont un peu plus lisses. WallVariation = 0 : murs v4.6
    wall_var = g.mul(g.scalar("WallVariation", 0.0), g.sub(g.const(1.0), wz))
    wall_uv = g.div(g.append(g.dot(xy, g.c2(0.707, 0.707)), g.mask(wp, "b")), g.const(1700.0))
    wall_tone = g.tex("NoiseTex", noise_tex, wall_uv, out="G")
    col = g.mul(col, g.add(g.const(1.0), g.mul(g.mul(g.sub(wall_tone, g.const(0.5)), g.const(0.3)), wall_var)))
    wall_damp_n = g.tex("NoiseTex", noise_tex, g.add(g.mul(wall_uv, g.const(1.9)), g.c2(0.31, 0.77)), out="R")
    wall_damp = g.mul(g.sat(g.div(g.sub(wall_damp_n, g.const(0.68)), g.const(0.1))), wall_var)
    col = g.mul(col, g.lerp(g.c3(1.0, 1.0, 1.0), g.c3(0.8, 0.76, 0.62), wall_damp))

    # Salete a grande echelle (casse la repetition)
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

    # Caustiques (reflets de l'eau sur le carrelage), deformees par les vagues simulees autour du joueur
    cm = g.custom("BRCaustics", BR_CAUSTICS_HLSL, [
        ("WP", wp), ("Wz", wz), ("T", g.time()), ("Amount", g.scalar("Caustics", 0.0)),
        ("CausTex", g.texobj("CausticsTex", load_tex("T_Caustics"))), ("SimTex", g.texobj("WaterSim", black_tex())),
        ("SimWin", g.vector4("WaterSimWindow", (0.0, 0.0, 1000.0, 0.0)))], output="CMOT_FLOAT1")
    col = g.mul(col, cm)

    base = g.mul(col, g.vector("Tint", (1, 1, 1, 1)))

    # v4.1 : flaques et sol mouille (reflets ray traces : rugosite quasi nulle, surface plane, ronds de gouttes)
    pud = g.custom("BRPuddles", BR_PUDDLES_HLSL, [
        ("WP", wp), ("N", g.vertex_normal()), ("T", g.time()), ("Amount", g.scalar("Puddles", 0.0)),
        ("Wet", g.scalar("Wetness", 0.0)), ("Tex", g.texobj("PuddleTex", grime_tex)), ("Noise", g.texobj("PuddleNoise", noise_tex)),
        ("WaterZ", g.scalar("WaterLine", -1.0))],
        output="CMOT_FLOAT4")
    puddle = g.mask(pud, "r")
    wet = g.mask(pud, "g")
    ripple = g.mask(pud, "ba")
    # Mouille : plus sombre ; sous l'eau d'une flaque : encore un peu plus (l'eau absorbe)
    shaded = g.mul(base, g.lerp(g.const(1.0), g.const(0.55), wet))
    shaded = g.mul(shaded, g.lerp(g.const(1.0), g.const(0.7), puddle))
    # v4.8 : dans la scene ray tracee (hit lighting du profil Cinematique), un seul echantillon de la texture projete sur
    # l'axe dominant, sans bruits, caustiques ni flaques : meme teinte moyenne, cout bien moindre a chaque rayon
    step_x0 = g.sat(g.mul(g.sub(wx, wy), g.const(1000.0)))
    step_z0 = g.sat(g.mul(g.sub(wz, g.const(0.5)), g.const(1000.0)))
    uv_dom = g.lerp(g.lerp(uvy, uvx, step_x0), uvz, step_z0)
    rt_col = g.mul(g.mul(g.tex("BaseTex", load_tex("T_L0_Wallpaper"), uv_dom), g.lerp(g.const(1.0), gs, g.scalar("Grime", 0.35))),
                   g.vector("Tint", (1, 1, 1, 1)))
    g.output(g.rt_switch(shaded, rt_col), P.MP_BASE_COLOR)

    # Rugosite variable (zones plus lisses / plus mates), puis mouillee, puis miroir dans les flaques
    rv = g.tex("GrimeTex", grime_tex, g.mul(guv, g.const(3.1)), out="B")
    # v4.7 : carte de rugosite de la matiere (<Texture>_R, lineaire, relative a Roughness : 0,5 = valeur nominale),
    # un seul echantillon projete sur l'axe dominant de la face ; RoughContrast = 0 : rugosite v4.6
    step_x = g.sat(g.mul(g.sub(wx, wy), g.const(1000.0)))
    step_z = g.sat(g.mul(g.sub(wz, g.const(0.5)), g.const(1000.0)))
    uvr = g.lerp(g.lerp(uvy, uvx, step_x), uvz, step_z)
    rmap = g.tex("RoughTex", noise_tex, uvr, out="R")
    rcon = g.scalar("RoughContrast", 0.0)
    grime_var = g.mul(g.sub(rv, g.const(0.5)), g.sub(g.const(0.3), g.mul(rcon, g.const(0.18))))
    rough = g.sat(g.add(g.add(g.scalar("Roughness", 0.85), grime_var), g.mul(g.sub(rmap, g.const(0.5)), g.mul(rcon, g.const(2.0)))))
    rough = g.sat(g.sub(rough, g.mul(wall_damp, g.const(0.15))))
    rough = g.lerp(rough, g.mul(rough, g.const(0.35)), wet)
    rough = g.lerp(rough, g.const(0.02), puddle)
    g.output(g.rt_switch(rough, g.scalar("Roughness", 0.85)), P.MP_ROUGHNESS)
    g.output(g.mul(g.scalar("Metallic", 0.0), g.sub(g.const(1.0), puddle)), P.MP_METALLIC)
    g.output(g.add(g.mul(base, g.scalar("SelfIllum", 0.0)), g.vector("Emissive", (0, 0, 0, 1))), P.MP_EMISSIVE_COLOR)

    # Normal maps (espace tangent, melange triplanaire) ; l'eau d'une flaque est plane, seules les gouttes la rident
    nrm = tri("NormalTex", load_tex("T_FlatNormal"), normal=True)
    nrm = g.lerp(g.c3(0.0, 0.0, 1.0), nrm, g.scalar("NormalStrength", 1.0))
    g.output(g.rt_switch(g.lerp(nrm, g.append(ripple, g.const(1.0)), puddle), g.c3(0.0, 0.0, 1.0)), P.MP_NORMAL)
    finish_material(m)
    return m


def build_pit_shade_material():
    """v4.7 : post-traitement des salles de fosses (meme graphe que BuildPitShade en C++). Le brouillard ordinaire ajoute
    sa couleur au fond d'un puits de 14 m (environ 11 % de voile a 15 m avec la densite du Niveau 0) : les pixels situes
    sous le sol s'assombrissent progressivement avec la profondeur ; le haut des parois reste visible."""
    m = new_material("M_BR_PitShade")
    safe_set(m, "material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    location = getattr(unreal.BlendableLocation, "BL_SCENE_COLOR_BEFORE_DOF", None)
    if location is not None:
        safe_set(m, "blendable_location", location)
    safe_set(m, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    g = Graph(m)
    P = unreal.MaterialProperty
    st = g.node(unreal.MaterialExpressionSceneTexture)
    st.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    scene = g.mask((st, "Color"), "rgb")
    z = g.mask(g.world_pos(), "b")
    below = g.sub(g.sub(g.scalar("PitFloorZ", 0.0), g.scalar("PitShadeStart", 60.0)), z)
    t = g.sat(g.div(below, g.scalar("PitShadeRange", 600.0)))
    ease = g.mul(g.mul(t, g.sub(g.const(2.0), t)), g.scalar("PitShadeAmount", 1.0))
    factor = g.lerp(g.const(1.0), g.scalar("PitShadeFloor", 0.04), ease)
    g.output(g.mul(scene, factor), P.MP_EMISSIVE_COLOR)
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
    # v4.7 : carte de rugosite des peaux (bouches luisantes, dents, grain), relative a Roughness ; defaut lineaire
    noise_tex = load_tex("T_NoiseLF")
    if noise_tex and not is_srgb(noise_tex):
        rmap = g.tex("RoughTex", noise_tex, uv, out="R")
        g.output(g.sat(g.add(g.scalar("Roughness", 0.6 if skin else 0.85),
                             g.mul(g.sub(rmap, g.const(0.5)), g.mul(g.scalar("RoughContrast", 0.0), g.const(2.0))))), P.MP_ROUGHNESS)
    else:
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


# Code HLSL des noeuds Custom (meme code que BRMaterialBuilder.cpp en C++).

# Houle et clapot de fond. Entrees : P (XY monde, cm), T (temps, s), Amp (houle), Chop (clapot).
# Sortie : float3(pente X, pente Y, hauteur de la houle).
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
    "return acc;\n")

# Vagues simulees autour du joueur. Entrees : P, Win (centre X, centre Y, cote en cm, intensite), SimTex.
# Sortie : float2(pente X, pente Y).
BR_WATER_SIM_HLSL = (
    "// Vagues simulees autour du joueur : pentes de la surface, estompees au bord de la zone simulee\n"
    "float sz = max(Win.z, 1.0);\n"
    "float2 d = abs(P - Win.xy) / (0.5 * sz);\n"
    "float fade = Win.w * saturate((1.0 - max(d.x, d.y)) / 0.15);\n"
    "return Texture2DSampleLevel(SimTex, SimTexSampler, P / sz, 0.0).rg * fade;\n")

# Refraction. Entrees : VN (inclinaison de la surface dans l'espace camera), PixD / D0 (profondeurs, cm), Strength.
# Sortie : decalage de l'image du fond, en fraction de l'ecran.
BR_WATER_REFRACT_HLSL = (
    "// Plus l'eau est epaisse sous ce point, plus l'image du fond est deplacee par la pente de la surface\n"
    "float thick = clamp(D0 - PixD, 0.0, 250.0);\n"
    "return float2(VN.x, -VN.y) * (Strength * 0.12 * thick / max(PixD, 20.0));\n")

# Lumiere qui traverse l'eau. Entrees : S (XY de la normale), V (vers la camera), Side (+1 dessus, -1 dessous),
# PixD, D0 / C0 (profondeur / couleur de la scene juste derriere), D1 / C1 (idem, image refractee), Absorb (1/cm).
# Sortie : float4(lumiere transmise, voile de l'eau 0..1).
BR_WATER_SHADE_HLSL = (
    "float3 N = normalize(float3(S, 1.0));\n"
    "float ndv = saturate(abs(dot(N, normalize(V))));\n"
    "// Image refractee, sauf si elle tombe sur un objet place devant l'eau\n"
    "bool ok = D1 > PixD + 2.0;\n"
    "float3 C = ok ? C1 : C0;\n"
    "float thick = max((ok ? D1 : D0) - PixD, 0.0);\n"
    "if (Side >= 0.0)\n"
    "{\n"
    "  // Vue de dessus : Fresnel de Schlick (eau : F0 = 0,02), absorption selon l'epaisseur traversee\n"
    "  float F = 0.02 + 0.98 * pow(1.0 - ndv, 5.0);\n"
    "  float3 Tr = exp(-thick * Absorb);\n"
    "  return float4(C * Tr * (1.0 - F), 1.0 - dot(Tr, float3(0.3333, 0.3334, 0.3333)));\n"
    "}\n"
    "// Vue de dessous : au-dela de ~49 degres, reflexion totale (on voit l'eau elle-meme)\n"
    "float s2 = 1.7689 * (1.0 - ndv * ndv);\n"
    "float F = s2 >= 1.0 ? 1.0 : 0.02 + 0.98 * pow(1.0 - sqrt(1.0 - s2), 5.0);\n"
    "return float4(C * (1.0 - F), F);\n")

# Caustiques du carrelage. Entrees : WP (position monde), Wz (poids des faces horizontales), T, Amount,
# CausTex (T_Caustics), SimTex / SimWin (vagues simulees). Sortie : multiplicateur de la couleur.
BR_CAUSTICS_HLSL = (
    "// Reseau lumineux qui derive lentement (deux echelles, legerement ondulees) ; les vagues simulees autour\n"
    "// du joueur le deforment d'autant plus que l'eau est profonde au-dessus du carrelage\n"
    "float sz = max(SimWin.z, 1.0);\n"
    "float2 sd = abs(WP.xy - SimWin.xy) / (0.5 * sz);\n"
    "float sf = SimWin.w * saturate((1.0 - max(sd.x, sd.y)) / 0.15);\n"
    "float2 slope = Texture2DSampleLevel(SimTex, SimTexSampler, WP.xy / sz, 0.0).rg * sf;\n"
    "float depth = max(45.0 - WP.z, 0.0);\n"
    "float2 uv = lerp(float2(WP.x + WP.y, WP.z), WP.xy, Wz) + slope * (20.0 + depth) * 2.0;\n"
    "float2 wob = float2(sin(uv.y * 0.019 + T * 0.7) + sin(uv.x * 0.013 - T * 0.45), cos(uv.x * 0.017 + T * 0.6) + cos(uv.y * 0.011 - T * 0.5));\n"
    "float k1 = Texture2DSample(CausTex, CausTexSampler, uv / 260.0 + T * float2(0.011, 0.006) + wob * 0.018).r;\n"
    "float k2 = Texture2DSample(CausTex, CausTexSampler, uv * float2(-1.0, 1.0) / 430.0 + T * float2(-0.007, 0.01) - wob * 0.012).r;\n"
    "float k = saturate(k1 + 0.45 * k2);\n"
    "// Sous l'eau : partout ; au-dessus : reflets plus faibles qui s'eteignent 2 m au-dessus de la surface\n"
    "float above = WP.z - 45.0;\n"
    "float fade = lerp(above > 0.0 ? 0.75 * saturate(1.0 - above / 200.0) : saturate(1.0 + above / 300.0), 1.0, Wz);\n"
    "return 1.0 + Amount * fade * (k * 1.5 - 0.27);\n")


BR_PUDDLES_HLSL = (
    "// Flaques, sol mouille et ligne d'eau (v4.5). Entrees : WP (position monde, cm), N (normale du sommet), T (temps, s),\n"
    "// Amount (part du sol couverte de flaques, 0..1), Wet (humidite generale, 0..1), Tex (T_Grime), Noise (T_NoiseLF),\n"
    "// WaterZ (hauteur de l'eau en cm, < 0 : pas d'eau). Sortie : float4(flaque 0..1, mouille 0..1, pente XY des ronds de gouttes).\n"
    "if (Amount <= 0.0 && Wet <= 0.0 && WaterZ < -0.5) return float4(0.0, 0.0, 0.0, 0.0);\n"
    "float up = saturate((N.z - 0.6) * 4.0);\n"
    "float2 p = WP.xy;\n"
    "// v4.5 : la forme vient d'un bruit doux (Noise : T_NoiseLF, lineaire) : de vraies flaques, pas un semis de taches ;\n"
    "// T_Grime (sRGB : pow pour revenir aux valeurs du fichier) decoupe seulement leurs bords (anses, presqu'iles)\n"
    "float base = Texture2DSample(Noise, NoiseSampler, p / 900.0).r * 0.7 + Texture2DSample(Noise, NoiseSampler, p / 370.0 + 0.41).g * 0.3;\n"
    "float e1 = pow(Texture2DSample(Tex, TexSampler, p / 160.0 + 0.23).b, 0.4545);\n"
    "float e2 = pow(Texture2DSample(Tex, TexSampler, p / 45.0 + 0.61).r, 0.4545);\n"
    "float n = base + (e1 - 0.5) * 0.07 + (e2 - 0.5) * 0.025;\n"
    "// Plus Amount est grand, plus le seuil baisse : 0,2 -> ~8 % du sol, 0,55 -> ~25 %, 1 -> ~57 % (calibre sur les textures)\n"
    "float th = lerp(0.755, 0.505, saturate(Amount));\n"
    "float puddle = Amount > 0.0 ? saturate((n - th) / 0.004) * up : 0.0;\n"
    "// lisere mouille irregulier autour de chaque flaque, plus large d'un cote que de l'autre\n"
    "float wet = saturate(saturate((n - th + 0.03 + (e1 - 0.5) * 0.03) / 0.03) * 0.9 * saturate(Amount * 4.0) + Wet) * up;\n"
    "wet = max(wet, puddle);\n"
    "// Ligne d'eau : bande mouillee de 4 a 9 cm au-dessus de la surface (murs, piliers, rebords), sous l'eau tout est mouille\n"
    "if (WaterZ > -0.5)\n"
    "{\n"
    "  float h = WP.z - WaterZ;\n"
    "  float band = 4.0 + 5.0 * e1 + 2.0 * e2;\n"
    "  wet = max(wet, saturate(1.0 - h / band) * step(-60.0, h));\n"
    "}\n"
    "// Gouttes qui tombent du plafond : un rond qui s'elargit par case de 70 cm, a un rythme propre a chaque case\n"
    "float2 ripple = float2(0.0, 0.0);\n"
    "if (puddle > 0.001)\n"
    "{\n"
    "  float2 cell = floor(p / 70.0);\n"
    "  for (int i = -1; i <= 1; i++)\n"
    "  {\n"
    "    for (int j = -1; j <= 1; j++)\n"
    "    {\n"
    "      float2 c = cell + float2(i, j);\n"
    "      float h = frac(sin(dot(c, float2(12.9898, 78.233))) * 43758.5453);\n"
    "      float h2 = frac(h * 91.7);\n"
    "      float2 center = (c + float2(h, h2)) * 70.0;\n"
    "      float period = 1.8 + h2 * 2.6;\n"
    "      float age = frac(T / period + h) * period;\n"
    "      float2 d = p - center;\n"
    "      float r = length(d);\n"
    "      float ring = exp(-pow((r - age * 34.0) / 3.5, 2.0)) * exp(-age * 1.7);\n"
    "      ripple += (d / max(r, 0.01)) * ring * 0.45;\n"
    "    }\n"
    "  }\n"
    "}\n"
    "return float4(puddle, wet, ripple * puddle);\n"
)


def build_water_surface_material():
    """Eau translucide : l'image de la scene derriere l'eau est refractee par la surface, puis absorbee selon
    l'epaisseur d'eau traversee (limpide en surface, turquoise puis vert-bleu en profondeur). Fresnel, reflets
    Lumen, vue de dessous (reflexion totale). La surface ondule : houle de fond, et surtout les vagues simulees
    autour du joueur (UBRWaterSim : sillage, plongeons, gouttes)."""
    m = new_material("M_BR_WaterSurface")
    g = Graph(m)
    P = unreal.MaterialProperty
    safe_set(m, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    safe_set(m, "shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    safe_set(m, "two_sided", True)
    safe_set(m, "translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)

    wp = g.world_pos()
    xy = g.mask(wp, "rg")
    t = g.time()
    surface = g.custom("BRWaterSurface", BR_WATER_SURFACE_HLSL, [
        ("P", xy), ("T", t), ("Amp", g.scalar("WaveAmplitude", 1.0)), ("Chop", g.scalar("WaveChop", 1.0))])
    g.output(g.append(g.c2(0.0, 0.0), g.mask(surface, "b")), P.MP_WORLD_POSITION_OFFSET)

    # Vagues simulees autour du joueur
    sim = g.custom("BRWaterSim", BR_WATER_SIM_HLSL, [
        ("P", xy), ("Win", g.vector4("WaterSimWindow", (0.0, 0.0, 1000.0, 0.0))), ("SimTex", g.texobj("WaterSim", black_tex()))],
        output="CMOT_FLOAT2")

    # Rides de detail
    ntex = load_tex("T_WaterNormal")
    scale = g.scalar("TexScale", 300.0)
    uva = g.add(g.div(xy, scale), g.mul(t, g.c2(0.012, 0.008)))
    uvb = g.add(g.mul(g.div(xy, scale), g.c2(-1.6, 1.6)), g.mul(t, g.c2(-0.01, 0.014)))
    na = g.tex("NormalTex", ntex, uva, normal=True)
    nb = g.tex("NormalTex", ntex, uvb, normal=True)
    detail = g.mul(g.mask(g.add(na, nb), "rg"), g.scalar("NormalStrength", 0.1))
    nxy = g.sub(g.sub(detail, g.mask(surface, "rg")), sim)
    g.output(g.append(nxy, g.const(1.0)), P.MP_NORMAL)

    # Refraction : l'image du fond (couleur et profondeur de la scene) est decalee selon la pente de la surface
    pixd = g.node(unreal.MaterialExpressionPixelDepth)
    d0 = g.scene_depth()
    offset = g.custom("BRWaterRefract", BR_WATER_REFRACT_HLSL, [
        ("VN", g.world_to_view(g.append(nxy, g.const(0.0)))), ("PixD", pixd), ("D0", d0),
        ("Strength", g.scalar("RefractionStrength", 1.0))], output="CMOT_FLOAT2")

    # Absorption par centimetre d'eau traversee (la teinte est la couleur qui passe le mieux)
    tint = g.vector("Tint", (0.24, 0.7, 0.72, 1))
    absorb = g.mul(g.add(g.mul(g.sub(g.c3(1.0, 1.0, 1.0), tint), g.scalar("Absorption", 1.0)), g.c3(0.02, 0.02, 0.02)), g.const(0.01))
    shade = g.custom("BRWaterShade", BR_WATER_SHADE_HLSL, [
        ("S", nxy), ("V", g.node(unreal.MaterialExpressionCameraVectorWS)), ("Side", g.node(unreal.MaterialExpressionTwoSidedSign)),
        ("PixD", pixd), ("D0", d0), ("D1", g.scene_depth(offset)), ("C0", g.scene_color()), ("C1", g.scene_color(offset)),
        ("Absorb", absorb)], output="CMOT_FLOAT4")

    # Lumiere transmise en emission ; voile de l'eau profonde eclaire par la scene ; reflets speculaires
    g.output(g.mask(shade, "rgb"), P.MP_EMISSIVE_COLOR)
    g.output(g.mul(g.mul(tint, g.scalar("Scattering", 0.2)), g.mask(shade, "a")), P.MP_BASE_COLOR)
    g.output(g.const(0.35), P.MP_SPECULAR)
    g.output(g.scalar("Roughness", 0.03), P.MP_ROUGHNESS)
    g.output(g.const(1.0), P.MP_OPACITY)
    finish_material(m)
    return m


def materials_missing():
    return [n for n in MATERIALS if not exists(MAT + "/" + n)]


def build_materials():
    for name in OBSOLETE_MATERIALS:
        if exists(MAT + "/" + name):
            try:
                EAL.delete_asset(MAT + "/" + name)
            except Exception as e:
                warn("Suppression impossible %s : %s" % (name, e))
    jobs = (("M_BR_World", build_world_material), ("M_BR_Mesh", build_mesh_material),
            ("M_BR_Skin", lambda: build_mesh_material("M_BR_Skin", skin=True)),
            ("M_BR_WaterSurface", build_water_surface_material), ("M_BR_PitShade", build_pit_shade_material))
    for name, fn in jobs:
        try:
            fn()
        except Exception as e:
            unreal.log_error("[Backrooms] Echec de creation de %s : %s" % (name, e))
            # Un materiau a moitie construit serait pire que rien : le jeu le reconstruit alors en C++
            if exists(MAT + "/" + name):
                try:
                    EAL.delete_asset(MAT + "/" + name)
                except Exception:
                    pass


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
# v4.8 : validation de l'installation et reparation ciblee
# ---------------------------------------------------------------------------
def validation_report_path():
    return os.path.join(project_dir(), "Saved", "BackroomsSetup_Validation.txt")


def _material_problems(name):
    path = MAT + "/" + name
    if not exists(path):
        return [("material", name, "absent")]
    m = unreal.load_asset(path)
    if not isinstance(m, unreal.Material):
        return [("material", name, "n'est pas un materiau")]
    out = []
    for prop in USAGES.get(name, ()):
        try:
            if not m.get_editor_property(prop):
                out.append(("usage", name, prop))
        except Exception as e:
            out.append(("usage", name, "%s illisible (%s)" % (prop, e)))
    return out


def _skeletal_slot_names(sk):
    names = []
    try:
        for entry in sk.get_editor_property("materials"):
            names.append(str(entry.get_editor_property("material_slot_name")))
    except Exception as e:
        warn("Slots de %s illisibles : %s" % (sk.get_name(), e))
    return names


def validate(verbose=False):
    """Verifie ce que l'installation doit produire : textures, maillages (statiques et a squelette), slots des maillages
    a squelette, usages des materiaux maitres, carte. Retourne la liste des problemes (categorie, nom, detail) et ecrit
    Saved/BackroomsSetup_Validation.txt. La compilation des shaders se verifie en jeu (diagnostic par section)."""
    problems = []
    for name in MATERIALS:
        problems += _material_problems(name)
    for f in missing_in("Textures", TEX, IMG_EXT):
        problems.append(("texture", os.path.splitext(f)[0], "absente"))
    for f in missing_in("Icons", UI, (".png",)):
        problems.append(("icon", os.path.splitext(f)[0], "absente"))
    for f in missing_in("Sounds", SND, (".wav",)):
        problems.append(("sound", os.path.splitext(f)[0], "absent"))
    for f in missing_in("Meshes", MESH, (".fbx",)):
        problems.append(("mesh", os.path.splitext(f)[0], "absent"))
    for name in HAZMAT_TEXTURES:
        tex = load_tex(name)
        if tex is None:
            continue  # deja signalee absente
        try:
            size = min(tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
            if size < 1024:
                problems.append(("texture", name, "taille %d (attendu 4096)" % size))
            if not is_srgb(tex):
                problems.append(("texture", name, "importee en lineaire (sRGB attendu)"))
        except Exception:
            pass
    for f in list_raw("Skeletal", (".fbx",)):
        name = os.path.splitext(f)[0]
        sk = unreal.load_asset(MESH + "/" + name) if exists(MESH + "/" + name) else None
        if not isinstance(sk, unreal.SkeletalMesh):
            problems.append(("skeletal", name, "absent"))
            continue
        slots = _skeletal_slot_names(sk)
        for want in EXPECTED_SLOTS.get(name, ()):
            if not any(want.lower() == s.lower() or want.lower() in s.lower() for s in slots):
                problems.append(("slot", name, "slot %s introuvable (slots importes : %s)" % (want, ", ".join(slots) or "aucun")))
    if not exists(MAP_PATH):
        problems.append(("map", MAP_PATH, "absente"))
    try:
        os.makedirs(os.path.dirname(validation_report_path()), exist_ok=True)
        with open(validation_report_path(), "w", encoding="utf-8") as fh:
            fh.write("Validation de l'installation Backrooms v%d (materiaux v%d)\n" % (VERSION, MATERIAL_VERSION))
            fh.write("%d probleme(s)\n" % len(problems))
            for cat, name, detail in problems:
                fh.write("%s\t%s\t%s\n" % (cat, name, detail))
    except Exception as e:
        warn("Rapport de validation non ecrit : %s" % e)
    if verbose:
        for cat, name, detail in problems:
            warn("Validation : [%s] %s : %s" % (cat, name, detail))
        if not problems:
            log("Validation : tout est en place (textures, maillages, slots, usages des materiaux, carte)")
    return problems


def fix_usages(name):
    """Ajoute les usages manquants a un materiau maitre existant, le recompile et l'enregistre (sans le reconstruire)"""
    m = unreal.load_asset(MAT + "/" + name)
    if not isinstance(m, unreal.Material):
        return False
    changed = False
    for prop in USAGES.get(name, ()):
        try:
            if not m.get_editor_property(prop):
                changed = safe_set(m, prop, True) or changed
        except Exception:
            pass
    if changed:
        MEL.recompile_material(m)
        EAL.save_loaded_asset(m, only_if_is_dirty=False)
        log("%s : usages ajoutes, recompile et enregistre" % name)
    return changed


def repair():
    """Reparation ciblee et rejouable : ne refait que ce que la validation signale (materiau sans usage, texture, son,
    modele ou carte manquants), puis valide de nouveau et ecrit le marqueur si tout est en ordre."""
    problems = validate(verbose=True)
    if not problems:
        write_marker()
        return []
    cats = {}
    for cat, name, detail in problems:
        cats.setdefault(cat, []).append(name)
    rebuild = sorted(set(cats.get("material", [])))
    for name in sorted(set(cats.get("usage", []))):
        if name not in rebuild and not fix_usages(name):
            rebuild.append(name)
    if rebuild or "texture" in cats:
        if "texture" in cats:
            import_textures([f for f in list_raw("Textures", IMG_EXT) if os.path.splitext(f)[0] in cats["texture"]])
        build_materials()
    if "icon" in cats:
        import_textures([f for f in list_raw("Icons", (".png",)) if os.path.splitext(f)[0] in cats["icon"]], UI, ui=True)
    if "sound" in cats:
        import_sounds([f for f in list_raw("Sounds", (".wav",)) if os.path.splitext(f)[0] in cats["sound"]])
    if "mesh" in cats:
        import_meshes([f for f in list_raw("Meshes", (".fbx",)) if os.path.splitext(f)[0] in cats["mesh"]])
    if "skeletal" in cats:
        import_skeletal([f for f in list_raw("Skeletal", (".fbx",)) if os.path.splitext(f)[0] in cats["skeletal"]])
    if "map" in cats:
        create_map()
    tune_skeletal()
    if "slot" in cats:
        warn("Slots inattendus (%s) : reimporter le maillage concerne (backrooms_setup.run(force=True)) ; le jeu affiche le "
             "materiau d'erreur sur ces sections et les liste dans le journal (LogBackrooms)." % ", ".join(sorted(set(cats["slot"]))))
    remaining = validate(verbose=True)
    if not remaining:
        write_marker()
        log("Reparation terminee : installation valide. Recuire le jeu avant d'empaqueter.")
    return remaining


# ---------------------------------------------------------------------------
# Point d'entree
# ---------------------------------------------------------------------------
def needs_setup():
    if installed_version() < VERSION or installed_material_version() < MATERIAL_VERSION:
        return True
    return bool(materials_missing() or missing_in("Textures", TEX, IMG_EXT) or missing_in("Icons", UI, (".png",))
                or missing_in("Sounds", SND, (".wav",)) or missing_in("Meshes", MESH, (".fbx",))
                or missing_in("Skeletal", MESH, (".fbx",)) or not exists(MAP_PATH))


def run(force=False):
    # Changement de version : tout est reimporte (les modeles et textures ont change)
    if installed_version() < VERSION:
        force = True
    tex = list_raw("Textures", IMG_EXT) if force else missing_in("Textures", TEX, IMG_EXT)
    if not force:
        for mv, files in sorted(RETEXTURED.items()):
            if installed_material_version() < mv:
                tex += [f for f in files if f not in tex and os.path.isfile(raw("Textures", f))]
    ico = list_raw("Icons", (".png",)) if force else missing_in("Icons", UI, (".png",))
    snd = list_raw("Sounds", (".wav",)) if force else missing_in("Sounds", SND, (".wav",))
    if not force:
        for mv, files in sorted(RESOUNDED.items()):
            if installed_material_version() < mv:
                snd += [f for f in files if f not in snd and os.path.isfile(raw("Sounds", f))]
    msh = list_raw("Meshes", (".fbx",)) if force else missing_in("Meshes", MESH, (".fbx",))
    skl = list_raw("Skeletal", (".fbx",)) if force else missing_in("Skeletal", MESH, (".fbx",))
    if not force:
        for mv, files in sorted(RESHAPED.items()):
            if installed_material_version() < mv:
                msh += [f for f in files if f not in msh and os.path.isfile(raw("Meshes", f))]
    mats = list(MATERIALS) if (force or installed_material_version() < MATERIAL_VERSION) else materials_missing()
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
        import_skeletal(skl)
        tune_skeletal()
        task.enter_progress_frame(1, "Materiaux")
        if mats or tex:
            build_materials()
        task.enter_progress_frame(1, "Carte")
        create_map()
    problems = validate(verbose=True)
    if not problems:
        write_marker()
        log("Installation terminee et verifiee. Appuyez sur Play !")
    else:
        warn("Installation incomplete (%d probleme(s)) : marqueur non ecrit. Reparer : import backrooms_setup; "
             "backrooms_setup.repair()" % len(problems))
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
