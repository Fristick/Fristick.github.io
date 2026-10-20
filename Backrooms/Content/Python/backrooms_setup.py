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
  RawAssets/Lights/*.ies        -> /Game/Backrooms/Lights     (v4.15 : profils IES de la lampe, Tools/generate_ies.py)
  RawAssets/Level0Kit/FBX/*.fbx -> /Game/Backrooms/Level0Kit/Meshes      (v4.14 : kit Blender du Level 0, dossier separe)
  RawAssets/Level0Kit/Textures/* -> /Game/Backrooms/Level0Kit/Textures   (BaseColor sRGB, ORM lineaire, Normal DirectX)
puis cree les materiaux maitres (M_BR_World, M_BR_Mesh, M_BR_Skin, M_BR_WaterSurface, M_BR_PitShade ; v4.15 : M_BR_WorldCloth,
MD_BR_Decal)
et la carte L_Backrooms.

v4.14 : le kit du Level 0 a son propre import, rejouable seul (les originaux de RawAssets ne sont jamais modifies) :

    import backrooms_setup; backrooms_setup.import_level0_kit(force=True)   # Saved/BackroomsSetup_Level0Kit.txt

Remarque : si cet import echoue, le jeu se debrouille quand meme dans l'editeur (il charge les textures
directement depuis RawAssets et construit des materiaux equivalents en C++ : voir BRMaterialBuilder.cpp).
"""
import os

import unreal

# v4.15 : 11 : profils photometriques IES de la lampe torche et de la frontale (RawAssets/Lights, /Game/Backrooms/Lights) ;
# 12 : kit du Level 0 : papier peint, moquette et dalles projetes dans l'espace monde (M_L0K_PbrWorld), diffuseur texture
# (M_L0K_ColorTex), textures du kit refaites ; atlas des decalques (tout est reimporte une fois) ;
# 13 : profils IES refaits (Tools/generate_ies.py : faisceau plus large, au moins 70 % de la lumiere du cone d'avant)
VERSION = 13
# Version des materiaux maitres : quand elle change, seuls les materiaux sont reconstruits (v4.1 : flaques,
# v4.3 : anti-repetition des sols, v4.4 : echantillonneur lineaire du bruit, le materiau du monde compile de nouveau,
# v4.7 : cartes de rugosite RoughTex/RoughContrast dans le monde et les modeles,
# v4.8 : usages "maillage a squelette" et "Nanite" sur M_BR_Mesh et M_BR_Skin : sans eux, la combinaison du joueur et les
# entites SK_* recevaient le materiau par defaut, gris et sans texture, en jeu autonome et en paquet ;
# v4.15 : 8 : monde : normales triplanaires reorientees dans l'espace monde, occlusion de cavite, reflectance par surface,
# ligne de maree ; nouveau M_BR_WorldCloth (moquette en modele d'eclairage "tissu") ;
# 9 : M_BR_WorldCloth par un noeud d'attributs : sa creation echouait en 5.8 (Custom Data 0 cache a Python), la moquette
# retombait sur M_BR_World et le paquet n'avait pas le materiau)
# v4.17 : 10 : M_BR_Gear (equipement du joueur : normal map tangente, carte ORM, reflectance ; cartes derivees de la
# combinaison dans RawAssets/Derived/Textures) ; caustiques des Poolrooms attachees a l'eau (entree NZ) ; papier peint du
# Level 0 refait (moins clair, relief adouci, joints des les)
MATERIAL_VERSION = 10
# Textures refaites depuis une version des materiaux : reimportees avec elle, sans tout reimporter
# (v4.15 : papier peint en les de 53 cm, moquette bouclee, dalles fissurees de 61 cm, et leurs cartes de rugosite)
RETEXTURED = {3: ["T_L0_Carpet.jpg", "T_L0_Carpet_N.png"],
              8: ["T_L0_Wallpaper.jpg", "T_L0_Wallpaper_N.png", "T_L0_Wallpaper_R.png", "T_L0_Carpet.jpg", "T_L0_Carpet_N.png",
                  "T_L0_Carpet_R.png", "T_L0_Ceiling.jpg", "T_L0_Ceiling_N.png", "T_L0_Ceiling_R.png"],
              10: ["T_L0_Wallpaper.jpg", "T_L0_Wallpaper_N.png"]}
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
# v4.15 : profils IES (Tools/generate_ies.py) : faisceau de la lampe torche et de la frontale (UTextureLightProfile)
LIGHTS = ROOT + "/Lights"
IES_EXT = (".ies",)
MAP_PATH = ROOT + "/Maps/L_Backrooms"
MATERIALS = ("M_BR_World", "M_BR_Mesh", "M_BR_Skin", "M_BR_WaterSurface", "M_BR_PitShade", "M_BR_WorldCloth", "MD_BR_Decal",
             "M_BR_Gear")
# v4.8 : usages que chaque materiau maitre doit declarer (meme liste que BRMaterialBuilder::RequiredUsages en C++)
_MODEL_USAGES = ("used_with_instanced_static_meshes", "used_with_skeletal_mesh", "used_with_nanite")
USAGES = {
    "M_BR_World": ("used_with_instanced_static_meshes",),
    "M_BR_Mesh": _MODEL_USAGES,
    "M_BR_Skin": _MODEL_USAGES,
    "M_BR_WaterSurface": ("used_with_instanced_static_meshes",),
    "M_BR_PitShade": (),
    "M_BR_WorldCloth": ("used_with_instanced_static_meshes",),
    # v4.15 : decalques projetes (UDecalComponent) et taches au sol en instances (decalques sur maillage)
    "MD_BR_Decal": ("used_with_instanced_static_meshes",),
    # v4.17 : combinaison (maillage a squelette), pieces rigides de secours (Nanite), objets en instances
    "M_BR_Gear": _MODEL_USAGES,
}
# v4.8 : slots attendus apres l'import de chaque maillage a squelette (noms des materiaux du FBX) : la validation
# signale un slot renomme par l'importeur (le jeu affecte les materiaux par nom, jamais par indice)
EXPECTED_SLOTS = {
    "SK_Hazmat": ("HazmatMask", "HazmatGlass", "HazmatSuit"),
    "SK_Faceling": ("FacelingTex",),
    "SK_Partygoer": ("PartygoerTex",),
    "SK_SkinStealer": ("SkinStealerFlesh", "EyeDark", "StealerEye", "StealerClaw"),
    # v4.15 : pas de HoundTeeth. Ce slot a 0 polygone dans les FBX (verifie dans Blender) : l'import Interchange d'Unreal
    # 5.8 ne garde pas les slots vides, l'ancien importeur FBX les gardait. Le jeu ne l'affecte pas (BRAssets.cpp).
    "SK_Hound": ("HoundSkin", "HoundMouth", "HoundHair", "HoundFace", "GlowAmberEye", "EyeDark", "HoundTongue"),
    "SK_HoundLite": ("HoundSkin", "HoundMouth", "HoundHair", "HoundFace", "GlowAmberEye", "EyeDark", "HoundTongue"),
    "SK_Wretch": ("WretchSkin", "WretchTeeth", "WretchEye"),
    "SK_Clump": ("ClumpSkin", "ClumpTeeth"),
}
# v4.8 : textures dont la combinaison du joueur depend (taille minimale attendue apres import)
HAZMAT_TEXTURES = ("T_Hazmat_Suit", "T_Hazmat_Mask")
# v4.17 : cartes derivees de la combinaison (Tools/derive_hazmat_maps.py), sources dans RawAssets/Derived/Textures, importees
# avec les autres textures (/Game/Backrooms/Textures : UBRAssets::Texture ne cherche que la)
DERIVED_SUB = os.path.join("Derived", "Textures")
GEAR_TEXTURES = ("T_Hazmat_Suit_N", "T_Hazmat_Suit_ORM")
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


def is_orm_map(name):
    """v4.17 : cartes ORM (R occlusion, G rugosite, B metal) : donnees lineaires"""
    return name.endswith("_ORM")


# ---------------------------------------------------------------------------
# Imports
# ---------------------------------------------------------------------------
IMG_EXT = (".png", ".jpg", ".jpeg", ".tga")


def missing_in(sub, folder, exts):
    return [f for f in list_raw(sub, exts) if not exists(folder + "/" + os.path.splitext(f)[0])]


def import_textures(files, dest=TEX, ui=False, sub=None):
    if not files:
        return
    # v4.17 : sub : dossier de RawAssets (cartes derivees : DERIVED_SUB)
    if sub is None:
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
        elif is_orm_map(name):
            # v4.17 : occlusion, rugosite absolue, metal : lineaire, compression par defaut lue par un echantillonneur
            # "Linear Color" (comme les _ORM du kit) ; la variance de la normale du meme nom releve la rugosite dans les
            # mips (moins d'etincelles de reflet a distance)
            safe_set(tex, "srgb", False)
            safe_set(tex, "lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
            nrm = unreal.load_asset(dest + "/" + name[:-len("_ORM")] + "_N")
            if nrm:
                safe_set(tex, "composite_texture", nrm)
                safe_set(tex, "composite_texture_mode", unreal.CompositeTextureMode.CTM_NORMAL_ROUGHNESS_TO_GREEN)
                safe_set(tex, "composite_power", 1.0)
        elif is_roughness_map(name):
            # v4.7 : rugosite relative (0,5 = valeur nominale de la surface) : lineaire, compression par defaut
            # (echantillonneur "Linear Color", comme T_NoiseLF qui sert de valeur par defaut au parametre RoughTex)
            safe_set(tex, "srgb", False)
            safe_set(tex, "lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        elif name == "T_LensDirt":
            safe_set(tex, "lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
        EAL.save_asset(path, only_if_is_dirty=False)
    log("%d textures importees dans %s" % (len(files), dest))


def import_light_profiles(files):
    """v4.15 : profils photometriques IES (LM-63) -> UTextureLightProfile (faisceau de la lampe torche et de la frontale).
    Le jeu les lit par leur nom (UBRAssets::LightProfile) ; sans eux, la lampe garde son cone simple."""
    if not files:
        return
    tasks = [make_task(raw("Lights", f), LIGHTS, os.path.splitext(f)[0]) for f in files]
    tools().import_asset_tasks(tasks)
    done = 0
    for f in files:
        name = os.path.splitext(f)[0]
        path = LIGHTS + "/" + name
        profile = unreal.load_asset(path) if exists(path) else None
        if not isinstance(profile, unreal.TextureLightProfile):
            warn("Profil IES non importe : %s (%s)" % (name, type(profile).__name__ if profile else "absent"))
            continue
        EAL.save_asset(path, only_if_is_dirty=False)
        done += 1
    log("%d profil(s) IES importe(s) dans %s" % (done, LIGHTS))


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

    # v4.15 : entrees du noeud MakeMaterialAttributes par propriete du materiau (mode attributs, use_attributes)
    ATTRIBUTE_INPUTS = (("MP_BASE_COLOR", "BaseColor"), ("MP_METALLIC", "Metallic"), ("MP_SPECULAR", "Specular"),
                        ("MP_ROUGHNESS", "Roughness"), ("MP_ANISOTROPY", "Anisotropy"), ("MP_EMISSIVE_COLOR", "EmissiveColor"),
                        ("MP_OPACITY", "Opacity"), ("MP_OPACITY_MASK", "OpacityMask"), ("MP_NORMAL", "Normal"),
                        ("MP_TANGENT", "Tangent"), ("MP_WORLD_POSITION_OFFSET", "WorldPositionOffset"),
                        ("MP_SUBSURFACE_COLOR", "SubsurfaceColor"), ("MP_AMBIENT_OCCLUSION", "AmbientOcclusion"),
                        ("MP_REFRACTION", "Refraction"))
    attrs = None

    def use_attributes(self):
        """v4.15 : les sorties du materiau passent par un noeud MakeMaterialAttributes (attributs de materiau). Seule
        facon, depuis Python en 5.8, de relier Custom Data 0 (« Cloth » du modele tissu, entree ClearCoat du noeud) :
        MP_CustomData0 est cache dans l'enumeration (UMETA(Hidden)) et connect_material_property le refuse. A appeler
        avant toute sortie ; output() accepte alors aussi le nom d'une entree du noeud ("ClearCoat")."""
        safe_set(self.m, "use_material_attributes", True)
        self.attrs = self.node(unreal.MaterialExpressionMakeMaterialAttributes)
        if not MEL.connect_material_property(self.attrs, "", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES):
            raise RuntimeError("attributs de materiau : liaison du noeud MakeMaterialAttributes impossible")

    def output(self, src, prop):
        e, out = self.pin(src)
        if self.attrs is not None:
            inp = prop if isinstance(prop, str) else None
            for name, pin_name in self.ATTRIBUTE_INPUTS:
                if inp is None and hasattr(unreal.MaterialProperty, name) and prop == getattr(unreal.MaterialProperty, name):
                    inp = pin_name
            if inp is None:
                warn("Liaison impossible vers %s : pas d'entree d'attributs" % prop)
                return False
            return self.link(src, self.attrs, inp)
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

    def tex(self, name, texture, uv, normal=False, out="RGB", mip_bias=0):
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
        if mip_bias > 0:
            # v4.15 : echantillon flou (mip_bias niveaux de mip au-dessus) : moyenne locale de la texture
            mode = getattr(getattr(unreal, "TextureMipValueMode", None), "TMVM_MIP_BIAS", None)
            if mode is None:
                warn("TextureMipValueMode.TMVM_MIP_BIAS absent : echantillon %s non floute (occlusion de cavite nulle)" % name)
            else:
                safe_set(e, "mip_value_mode", mode)
                safe_set(e, "const_mip_value", int(mip_bias))
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


def build_world_material(cloth=False):
    """Murs, sols, plafonds : projection triplanaire dans l'espace monde (jamais de texture etiree),
    normal maps, salete a grande echelle, salete au pied des murs, caustiques animees (Poolrooms).
    v4.15 : normales reorientees dans l'espace monde, occlusion de cavite (Cavity), reflectance (Specular), ligne de maree
    des murs (TideLine) ; cloth=True : M_BR_WorldCloth, meme graphe en modele d'eclairage "tissu" (moquettes, Sheen)."""
    m = new_material("M_BR_WorldCloth" if cloth else "M_BR_World")
    # v4.15 : normales calculees dans l'espace monde (meme reglage que BRMaterialBuilder::Build)
    safe_set(m, "tangent_space_normal", False)
    if cloth:
        cloth_model = getattr(unreal.MaterialShadingModel, "MSM_CLOTH", None)
        if cloth_model is None:
            raise RuntimeError("MaterialShadingModel.MSM_CLOTH absent")
        safe_set(m, "shading_model", cloth_model)
    g = Graph(m)
    if cloth:
        # v4.15 : sorties par un noeud d'attributs : « Cloth » (Custom Data 0) n'est plus reliable directement en 5.8
        g.use_attributes()
    P = unreal.MaterialProperty

    wp = g.world_pos()
    d = g.div(wp, g.scalar("TexScale", 200.0))
    flip = g.c2(1.0, -1.0)
    uvx = g.mul(g.mask(d, "gb"), flip)
    uvy = g.mul(g.mask(d, "rb"), flip)
    uvz = g.mask(d, "rg")

    vn = g.vertex_normal()
    a = g.abs(vn)
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

    def tri3(name, texture, normal=False):
        """Trois projections (X, Y, Z ; Z melange a son 2e echantillon tourne). Normales : pente du 2e echantillon ramenee
        dans le repere des UV d'origine (rotation inverse)"""
        sx = g.tex(name, texture, uvx, normal)
        sy = g.tex(name, texture, uvy, normal)
        sz1 = g.tex(name, texture, uvz, normal)
        sz2 = g.tex(name, texture, uvz2, normal)
        if normal:
            nxy = g.mask(sz2, "rg")
            sz2 = g.append(g.append(g.dot(nxy, g.c2(0.8, 0.6)), g.dot(nxy, g.c2(-0.6, 0.8))), g.mask(sz2, "b"))
        return sx, sy, g.lerp(sz1, sz2, mixz)

    cx, cy, cz = tri3("BaseTex", load_tex("T_L0_Wallpaper"))
    col = g.add(g.add(g.mul(cx, wx), g.mul(cy, wy)), g.mul(cz, wz))

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

    # v4.15 : ligne de maree au-dessus de la plinthe (eau remontee puis sechee) et bande plus sombre sous le plafond, a la
    # hauteur du monde (la texture, elle, se repete en hauteur) ; TideLine = 0 : rien
    tide = g.custom("BRTideLine", BR_TIDE_LINE_HLSL, [
        ("WP", wp), ("NZ", g.mask(vn, "b")), ("Amount", g.scalar("TideLine", 0.0)), ("Top", g.scalar("WallTop", 290.0)),
        ("Noise", g.texobj("TideNoise", noise_tex))], output="CMOT_FLOAT3")
    col = g.mul(col, tide)

    # Caustiques (reflets de l'eau sur le carrelage), deformees par les vagues simulees autour du joueur
    cm = g.custom("BRCaustics", BR_CAUSTICS_HLSL, [
        ("WP", wp), ("Wz", wz), ("NZ", g.mask(vn, "b")), ("T", g.time()), ("Amount", g.scalar("Caustics", 0.0)),
        ("CausTex", g.texobj("CausticsTex", load_tex("T_Caustics"))), ("SimTex", g.texobj("WaterSim", black_tex())),
        ("SimWin", g.vector4("WaterSimWindow", (0.0, 0.0, 1000.0, 0.0)))], output="CMOT_FLOAT1")
    col = g.mul(col, cm)

    base = g.mul(col, g.vector("Tint", (1, 1, 1, 1)))

    # v4.1 : flaques et sol mouille (reflets ray traces : rugosite quasi nulle, surface plane, ronds de gouttes)
    pud = g.custom("BRPuddles", BR_PUDDLES_HLSL, [
        ("WP", wp), ("N", vn), ("T", g.time()), ("Amount", g.scalar("Puddles", 0.0)),
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
    dom_sharp = g.tex("BaseTex", load_tex("T_L0_Wallpaper"), uv_dom)
    rt_col = g.mul(g.mul(dom_sharp, g.lerp(g.const(1.0), gs, g.scalar("Grime", 0.35))), g.vector("Tint", (1, 1, 1, 1)))
    g.output(g.rt_switch(shaded, rt_col), P.MP_BASE_COLOR)

    # v4.15 : occlusion de cavite : la ou la texture est plus sombre que sa moyenne locale (meme echantillon 4 mips plus haut,
    # ~16 texels) : creux entre les boucles de la moquette, fissures des dalles, joints. La lumiere indirecte (Lumen) et le
    # reflet y sont attenues. Cavity = 0 : aucune occlusion (rendu d'avant)
    dom_blur = g.tex("BaseTex", load_tex("T_L0_Wallpaper"), uv_dom, mip_bias=4)
    lum_w = g.c3(0.3, 0.59, 0.11)
    lum_sharp = g.dot(dom_sharp, lum_w)
    lum_blur = g.dot(dom_blur, lum_w)
    cav = g.mul(g.sat(g.mul(g.div(g.sub(lum_blur, lum_sharp), g.add(lum_blur, g.const(0.03))), g.const(3.0))), g.scalar("Cavity", 0.0))
    g.output(g.sub(g.const(1.0), g.mul(cav, g.const(0.85))), P.MP_AMBIENT_OCCLUSION)

    # Rugosite variable (zones plus lisses / plus mates), puis mouillee, puis miroir dans les flaques
    rv = g.tex("GrimeTex", grime_tex, g.mul(guv, g.const(3.1)), out="B")
    # v4.7 : carte de rugosite de la matiere (<Texture>_R, lineaire, relative a Roughness : 0,5 = valeur nominale),
    # un seul echantillon projete sur l'axe dominant de la face ; RoughContrast = 0 : rugosite v4.6
    rmap = g.tex("RoughTex", noise_tex, uv_dom, out="R")
    rcon = g.scalar("RoughContrast", 0.0)
    grime_var = g.mul(g.sub(rv, g.const(0.5)), g.sub(g.const(0.3), g.mul(rcon, g.const(0.18))))
    rough = g.sat(g.add(g.add(g.scalar("Roughness", 0.85), grime_var), g.mul(g.sub(rmap, g.const(0.5)), g.mul(rcon, g.const(2.0)))))
    rough = g.sat(g.sub(rough, g.mul(wall_damp, g.const(0.15))))
    rough = g.lerp(rough, g.mul(rough, g.const(0.35)), wet)
    rough = g.lerp(rough, g.const(0.02), puddle)
    g.output(g.rt_switch(rough, g.scalar("Roughness", 0.85)), P.MP_ROUGHNESS)
    g.output(g.mul(g.scalar("Metallic", 0.0), g.sub(g.const(1.0), puddle)), P.MP_METALLIC)
    # v4.15 : reflectance propre a la surface (Specular : 0,5 = 4 %, moquette 0,3, papier peint 0,4), attenuee dans les
    # creux ; l'eau d'une flaque reflete comme l'eau (0,35, comme M_BR_WaterSurface)
    spec = g.mul(g.scalar("Specular", 0.5), g.sub(g.const(1.0), g.mul(cav, g.const(0.5))))
    g.output(g.lerp(spec, g.const(0.35), puddle), P.MP_SPECULAR)
    g.output(g.add(g.mul(base, g.scalar("SelfIllum", 0.0)), g.vector("Emissive", (0, 0, 0, 1))), P.MP_EMISSIVE_COLOR)

    # v4.15 : normales dans l'espace monde : chaque projection a ses propres axes (X -> (Y, -Z), Y -> (X, -Z), Z -> (X, Y)) ;
    # avant, les trois normales etaient melangees telles quelles dans le repere tangent du cube (relief faux sur certaines
    # faces). L'eau d'une flaque est plane (normale monde du sol), seules les gouttes la rident
    nx, ny, nz = tri3("NormalTex", load_tex("T_FlatNormal"), normal=True)
    nrm = g.custom("BRTriNormal", BR_TRI_NORMAL_HLSL, [
        ("TX", nx), ("TY", ny), ("TZ", nz), ("VN", vn), ("W", wn), ("Strength", g.scalar("NormalStrength", 1.0))],
        output="CMOT_FLOAT3")
    g.output(g.rt_switch(g.lerp(nrm, g.append(ripple, g.const(1.0)), puddle), vn), P.MP_NORMAL)

    if cloth:
        # v4.15 : moquette : lustre des fibres (duvet un peu plus clair que la matiere), perdu quand elle est mouillee
        g.output(g.mul(shaded, g.vector("FuzzTint", (1.3, 1.25, 1.15, 1))), P.MP_SUBSURFACE_COLOR)
        # « Cloth » du modele tissu = Custom Data 0 = entree ClearCoat du noeud d'attributs (BRMaterialBuilder : ClearCoat)
        g.output(g.mul(g.mul(g.scalar("Sheen", 0.8), g.sub(g.const(1.0), wet)), g.sub(g.const(1.0), puddle)), "ClearCoat")
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


def build_decal_material():
    """v4.15 : decalques d'usure (meme graphe que BuildDecal en C++). Domaine "decalque differe", melange translucide :
    couleur et rugosite ecrites dans le DBuffer (la normale n'est pas remplacee : le relief du papier peint et de la moquette
    reste visible sous une tache). Une case de l'atlas T_Decal_Atlas (AtlasCell : decalage u, v, echelle u, v) ; les UV sont
    ceux de la projection (UDecalComponent) ou du maillage (plans des taches au sol, en instances : usage ISM)."""
    m = new_material("MD_BR_Decal")
    domain = getattr(unreal.MaterialDomain, "MD_DEFERRED_DECAL", None)
    if domain is None:
        raise RuntimeError("MaterialDomain.MD_DEFERRED_DECAL absent")
    # Melange avant le domaine : chaque reglage recompile le materiau, et un decalque opaque (melange par defaut) ne compile
    # pas ("Failed to compile Material ... DeferredDecal domain can only use ... Translucent" dans le journal, sans suite)
    safe_set(m, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    safe_set(m, "material_domain", domain)
    g = Graph(m)
    P = unreal.MaterialProperty
    cell = g.vector4("AtlasCell", (0.0, 0.0, 1.0, 1.0))
    uv = g.add(g.mul(g.uv0(), g.mask(cell, "ba")), g.mask(cell, "rg"))
    t = g.tex("DecalTex", load_tex("T_Decal_Atlas"), uv, out="RGBA")[0]
    g.output(g.mul((t, "RGB"), g.vector("DecalTint", (1, 1, 1, 1))), P.MP_BASE_COLOR)
    g.output(g.mul((t, "A"), g.scalar("DecalOpacity", 1.0)), P.MP_OPACITY)
    g.output(g.scalar("DecalRough", 0.8), P.MP_ROUGHNESS)
    g.output(g.scalar("DecalSpecular", 0.4), P.MP_SPECULAR)
    g.output(g.const(0.0), P.MP_METALLIC)
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


def build_gear_material():
    """v4.17 : equipement du joueur (combinaison, masque, visiere) : eclairage standard (ni chair ni diffusion
    sous-cutanee), normal map tangente (NormalTex) et carte ORM (MaskTex : R occlusion, G rugosite absolue, B metal,
    lineaire). Sans carte, tout reste neutre (NormalStrength, RoughMapWeight, CavityWeight, MetalMapWeight a 0). Meme
    graphe que BuildGear (BRMaterialBuilder.cpp)."""
    m = new_material("M_BR_Gear")
    g = Graph(m)
    P = unreal.MaterialProperty
    uv = g.mul(g.uv0(), g.scalar("TexScale", 1.0))
    albedo = g.tex("BaseTex", load_tex("T_Hazmat_Suit"), uv)
    tint = g.vector("Tint", (1, 1, 1, 1))
    gray = g.dot(albedo, g.c3(0.2126, 0.7152, 0.0722))
    base = g.mul(g.lerp(gray, albedo, g.scalar("Saturation", 1.0)), tint)
    g.output(g.rt_switch(base, g.mul(albedo, tint)), P.MP_BASE_COLOR)
    rough_c = g.scalar("Roughness", 0.38)
    metal_c = g.scalar("Metallic", 0.0)
    noise_tex = load_tex("T_NoiseLF")
    if not noise_tex or is_srgb(noise_tex):
        # Defaut lineaire indispensable (echantillonneur Linear Color) : un graphe sans carte ni parametre repere serait
        # pris pour un ancien materiau ; mieux vaut l'echec (le jeu reconstruit alors le materiau en C++)
        raise RuntimeError("T_NoiseLF absente ou en sRGB : M_BR_Gear non construit")
    orm = g.tex("MaskTex", noise_tex, uv)
    rough = g.lerp(rough_c, g.mask(orm, "g"), g.scalar("RoughMapWeight", 0.0))
    metal = g.lerp(metal_c, g.mask(orm, "b"), g.scalar("MetalMapWeight", 0.0))
    ao = g.lerp(g.const(1.0), g.mask(orm, "r"), g.scalar("CavityWeight", 0.0))
    g.output(g.rt_switch(rough, rough_c), P.MP_ROUGHNESS)
    g.output(metal, P.MP_METALLIC)
    g.output(ao, P.MP_AMBIENT_OCCLUSION)
    # Reflet attenue dans les creux (la moitie de l'occlusion)
    g.output(g.mul(g.scalar("Specular", 0.5), g.lerp(g.const(1.0), ao, g.const(0.5))), P.MP_SPECULAR)
    n = g.tex("NormalTex", load_tex("T_FlatNormal"), uv, normal=True)
    g.output(g.rt_switch(g.lerp(g.c3(0.0, 0.0, 1.0), n, g.scalar("NormalStrength", 0.0)), g.c3(0.0, 0.0, 1.0)), P.MP_NORMAL)
    g.output(g.add(g.mul(base, g.scalar("SelfIllum", 0.0)), g.vector("Emissive", (0, 0, 0, 1))), P.MP_EMISSIVE_COLOR)
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

# Caustiques du carrelage. Entrees : WP (position monde), Wz (poids des faces horizontales), NZ (normale du sommet,
# z ; v4.17), T, Amount,
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
    "// v4.17 : sous l'eau : partout (fond et parois) ; au-dessus : le lisere des murs pres de la surface (reflets qui\n"
    "// s'eteignent en 90 cm) et un reflet doux et flou au plafond ; un sol ou le dessus d'un trottoir hors de l'eau est sec :\n"
    "// aucune caustique (avant : faces horizontales a plein poids a toute hauteur, murs jusqu'a 2 m au-dessus de l'eau)\n"
    "float above = WP.z - 45.0;\n"
    "float down = saturate(-NZ * 4.0);\n"
    "float wallFade = above > 0.0 ? 0.55 * saturate(1.0 - above / 90.0) : saturate(1.0 + above / 300.0);\n"
    "float horizFade = above > 0.0 ? down * 0.6 : 1.0;\n"
    "float fade = lerp(wallFade, horizFade, Wz);\n"
    "// Plafond : motif plus flou (niveau de detail plus grossier de la texture) et contraste reduit\n"
    "float kc = saturate(Texture2DSampleBias(CausTex, CausTexSampler, uv / 260.0 + T * float2(0.011, 0.006) + wob * 0.018, 3.0).r + 0.45 * k2);\n"
    "float kk = lerp(k, kc * 0.7 + 0.12, down * saturate(above / 60.0));\n"
    "return 1.0 + Amount * fade * (kk * 1.2 - 0.22);\n")


BR_TRI_NORMAL_HLSL = (
    "// v4.15 : normales de la projection triplanaire dans l'espace monde. Entrees : TX, TY, TZ (normales de la texture,\n"
    "// espace tangent de chaque projection), VN (normale du sommet), W (poids des projections), Strength (0 : normale du\n"
    "// sommet). Projection X : U = +Y, V = -Z ; projection Y : U = +X, V = -Z ; projection Z : U = +X, V = +Y.\n"
    "float sx = VN.x >= 0.0 ? 1.0 : -1.0;\n"
    "float sy = VN.y >= 0.0 ? 1.0 : -1.0;\n"
    "float sz = VN.z >= 0.0 ? 1.0 : -1.0;\n"
    "float3 nx = float3(TX.z * sx, TX.x, -TX.y);\n"
    "float3 ny = float3(TY.x, TY.z * sy, -TY.y);\n"
    "float3 nz = float3(TZ.x, TZ.y, TZ.z * sz);\n"
    "float3 n = nx * W.x + ny * W.y + nz * W.z;\n"
    "return normalize(lerp(VN, n, saturate(Strength)));\n")


BR_TIDE_LINE_HLSL = (
    "// v4.15 : ligne de maree au pied des murs (eau remontee dans le papier puis sechee : lisere brun, papier plus sombre\n"
    "// dessous) et bande plus sombre sous le plafond. Entrees : WP (position monde, cm), NZ (normale du sommet, z), Amount\n"
    "// (0..1), Top (hauteur du plafond, cm), Noise (T_NoiseLF). Sortie : multiplicateur de la couleur (1 hors des murs).\n"
    "if (Amount <= 0.0) return float3(1.0, 1.0, 1.0);\n"
    "float wall = saturate(1.0 - abs(NZ) * 1.5);\n"
    "float u = WP.x + WP.y;\n"
    "float n1 = Texture2DSample(Noise, NoiseSampler, float2(u / 410.0, 0.37)).r;\n"
    "float n2 = Texture2DSample(Noise, NoiseSampler, float2(u / 97.0, 0.71)).g;\n"
    "float h = 13.0 + 15.0 * n1 + 4.0 * (n2 - 0.5);\n"
    "float z = WP.z;\n"
    "float below = saturate((h - z) / 4.0 + 0.5) * saturate(z / 2.0 + 1.0);\n"
    "float edge = exp(-pow((z - h) / 1.2, 2.0));\n"
    "float3 m = lerp(float3(1.0, 1.0, 1.0), float3(0.9, 0.86, 0.74), below * 0.75);\n"
    "m *= lerp(float3(1.0, 1.0, 1.0), float3(0.74, 0.65, 0.47), edge * 0.8);\n"
    "m *= 1.0 - 0.08 * smoothstep(Top - 45.0, Top - 2.0, z);\n"
    "return lerp(float3(1.0, 1.0, 1.0), m, saturate(Amount) * wall);\n")


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
            ("M_BR_WaterSurface", build_water_surface_material), ("M_BR_PitShade", build_pit_shade_material),
            ("M_BR_WorldCloth", lambda: build_world_material(cloth=True)), ("MD_BR_Decal", build_decal_material),
            ("M_BR_Gear", build_gear_material))
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
    # v4.15 : un materiau dont la creation a echoue peut rester en memoire (suppression refusee) sans fichier sur le
    # disque : la validation le voyait, le paquet ne l'avait pas (M_BR_WorldCloth, check_package.py)
    if path.startswith("/Game/"):
        disk = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), path[len("/Game/"):] + ".uasset")
        if not os.path.isfile(disk):
            return [("material", name, "absent du disque (creation echouee ?)")]
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


def _imported_size(tex, path):
    """v4.15 : plus petit cote de la texture importee, lu dans le registre (tag Dimensions, taille de l'image source).
    Au demarrage de l'editeur, Unreal 5.8 compile les textures en tache de fond : blueprint_get_size_x() rend alors 32
    (texture de remplacement) pendant quelques centaines d'images. Une limite d'import (max_texture_size) compte aussi."""
    size = 0
    try:
        tag = EAL.find_asset_data(path).get_tag_value("Dimensions")
        if tag:
            size = min(int(v) for v in str(tag).lower().split("x")[:2])
    except Exception:
        size = 0
    if size <= 0:
        size = min(tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
    try:
        cap = int(tex.get_editor_property("max_texture_size"))
        if cap > 0:
            size = min(size, cap)
    except Exception:
        pass
    return size


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
    for f in missing_in("Lights", LIGHTS, IES_EXT):
        problems.append(("light", os.path.splitext(f)[0], "absent"))
    for name in HAZMAT_TEXTURES:
        tex = load_tex(name)
        if tex is None:
            continue  # deja signalee absente
        try:
            size = _imported_size(tex, TEX + "/" + name)
            if size < 1024:
                problems.append(("texture", name, "taille %d (attendu 4096)" % size))
            if not is_srgb(tex):
                problems.append(("texture", name, "importee en lineaire (sRGB attendu)"))
        except Exception:
            pass
    # v4.17 : cartes derivees de la combinaison : presentes si leur source existe, lineaires, normale en TC_NORMALMAP
    for name in GEAR_TEXTURES:
        if not os.path.isfile(raw(DERIVED_SUB, name + ".png")):
            continue
        tex = load_tex(name)
        if tex is None:
            problems.append(("texture", name, "carte derivee de la combinaison non importee (combinaison sans relief)"))
            continue
        try:
            if is_srgb(tex):
                problems.append(("texture", name, "importee en sRGB (lineaire attendu)"))
            if name.endswith("_N") and tex.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_NORMALMAP:
                problems.append(("texture", name, "compression autre que TC_NORMALMAP"))
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
    if "light" in cats:
        import_light_profiles([f for f in list_raw("Lights", IES_EXT) if os.path.splitext(f)[0] in cats["light"]])
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
                or missing_in("Skeletal", MESH, (".fbx",)) or missing_in("Lights", LIGHTS, IES_EXT) or not exists(MAP_PATH)
                or missing_in(DERIVED_SUB, TEX, IMG_EXT))


# ---------------------------------------------------------------------------
# v4.14 : kit Blender du Level 0 (RawAssets/Level0Kit, Tools/Blender/build_level0_kit.py)
# ---------------------------------------------------------------------------
KIT = ROOT + "/Level0Kit"
KIT_MESH = KIT + "/Meshes"
KIT_TEX = KIT + "/Textures"
KIT_MAT = KIT + "/Materials"
# Materiaux du kit (noms des slots des FBX) : textures T_L0K_<Nom>_BaseColor / _ORM / _Normal, teinte, couleur de sommet
# (profondeur des puits), emission ; couleur unie ; decal (masque alpha). Meme table que le README du kit (section 7)
KIT_MATERIALS = {
    # v4.15 : "world" : texture projetee dans l'espace monde (M_L0K_PbrWorld, cm par repetition, comme les surfaces du jeu) :
    # le motif ne recommence plus a chaque module ; "tide" : ligne de maree et bande sous le plafond ; "aged" : la teinte du
    # papier jauni s'estompe vers les bouts du module ; "spec" : reflectance
    "M_L0_Wallpaper": {"tex": "T_L0K_Wallpaper", "world": 106.0, "tide": 1.0, "spec": 0.4},
    "M_L0_Wallpaper_Aged": {"tex": "T_L0K_Wallpaper", "tint": (0.93, 0.9, 0.82), "world": 106.0, "tide": 1.0, "spec": 0.4, "aged": 1.0},
    "M_L0_Carpet": {"tex": "T_L0K_Carpet", "world": 160.0, "spec": 0.3},
    "M_L0_CeilingTile": {"tex": "T_L0K_CeilingTile", "world": 122.0},
    "M_L0_Concrete": {"tex": "T_L0K_Concrete"},
    "M_L0_PitConcrete": {"tex": "T_L0K_Concrete", "vertex": 1.0},
    "M_L0_Baseboard": {"tex": "T_L0K_Baseboard"},
    "M_L0_FixtureMetal": {"tex": "T_L0K_FixtureMetal"},
    "M_L0_DoorSteel": {"tex": "T_L0K_DoorSteel"},
    "M_L0_DoorHardware": {"tex": "T_L0K_Hardware"},
    "M_L0_LightDiffuser": {"color_tex": "T_L0K_Diffuser_Emissive", "base_tex": "T_L0K_Diffuser_BaseColor", "emissive": 6.0, "rough": 0.35},
    "M_L0_LightDiffuser_Off": {"color": (0.55, 0.56, 0.52), "rough": 0.4},
    "M_L0_DraftGlow": {"color": (0.7, 0.85, 1.0), "emissive": 6.0, "rough": 0.5},
    "M_L0_PitDark": {"color": (0.008, 0.008, 0.007), "rough": 1.0},
    "M_L0_Chalk": {"color": (0.82, 0.81, 0.76), "rough": 0.98},
    "M_L0_Archive_Glitch": {"color": (0.5, 0.4, 0.8), "emissive": 2.0, "rough": 0.5},
    "M_L0_DecalWall": {"decal": "T_L0K_Decal_WaterStain"},
    "M_L0_DecalCeiling": {"decal": "T_L0K_Decal_CeilingStain"},
    "M_L0_DecalCarpet": {"decal": "T_L0K_Decal_CarpetDamp"},
}
# Materiaux maitres du kit (sorte -> nom). v4.15 : PbrWorld (textures projetees dans l'espace monde), ColorTex (couleur et
# emission d'une image : diffuseur des neons)
KIT_MASTERS = {"pbr": "M_L0K_Pbr", "pbrworld": "M_L0K_PbrWorld", "color": "M_L0K_Color", "colortex": "M_L0K_ColorTex", "decal": "M_L0K_Decal"}
# Modules que le jeu pose dans le Level 0 genere (ABRChunk, v4.14) : le kit n'est utilise que s'ils sont tous la
# (meme liste que KitModules dans BRAssets.cpp)
KIT_REQUIRED = ("SM_L0_Wall_350", "SM_L0_Wall_Aged_350", "SM_L0_Doorway", "SM_L0_DoorCasing", "SM_L0_Baseboard_290", "SM_L0_Baseboard_070",
                "SM_L0_Wall_Corner", "SM_L0_Wall_JunctionT", "SM_L0_Wall_JunctionX", "SM_L0_Wall_End", "SM_L0_Wall_Straight", "SM_L0_Pillar",
                "SM_L0_ServiceDoor_Wall")

# v4.15 : normale de la texture projetee sur l'axe dominant de la face, dans l'espace monde (memes axes que la projection
# triplanaire de M_BR_World : X -> (Y, -Z), Y -> (X, -Z), Z -> (X, Y)) ; materiau maitre M_L0K_PbrWorld
BR_AXIS_NORMAL_HLSL = (
    "// v4.15 (kit du Level 0) : normale de la texture projetee sur l'axe dominant de la face, dans l'espace monde (memes axes\n"
    "// que la projection triplanaire de M_BR_World). Entrees : T (normale de la texture, espace tangent), VN (normale du sommet).\n"
    "float3 a = abs(VN);\n"
    "float3 n;\n"
    "if (a.z >= 0.5) n = float3(T.x, T.y, T.z * (VN.z >= 0.0 ? 1.0 : -1.0));\n"
    "else if (a.x >= a.y) n = float3(T.z * (VN.x >= 0.0 ? 1.0 : -1.0), T.x, -T.y);\n"
    "else n = float3(T.x, T.z * (VN.y >= 0.0 ? 1.0 : -1.0), -T.y);\n"
    "return normalize(n);\n")


def kit_raw(*parts):
    return os.path.join(project_dir(), "RawAssets", "Level0Kit", *parts)


def _kit_files(sub, exts):
    folder = kit_raw(sub)
    if not os.path.isdir(folder):
        return []
    return [f for f in sorted(os.listdir(folder)) if os.path.splitext(f)[1].lower() in exts]


def _kit_world_pbr(g, m, P):
    """v4.15 : M_L0K_PbrWorld : papier peint, moquette, dalles projetes dans l'espace monde sur l'axe dominant de la face
    (meme projection et meme echelle, TexScale en cm, que les surfaces du jeu : les motifs des modules et des boites du
    generateur se raccordent), normale reorientee dans l'espace monde. Ligne de maree (TideLine, meme code que M_BR_World).
    Papier jauni : la teinte s'estompe vers les bouts du module (position locale) et par plaques (AgedFade) : plus de marche
    de teinte d'un module a l'autre."""
    safe_set(m, "tangent_space_normal", False)
    wp = g.world_pos()
    d = g.div(wp, g.scalar("TexScale", 120.0))
    flip = g.c2(1.0, -1.0)
    uvx = g.mul(g.mask(d, "gb"), flip)
    uvy = g.mul(g.mask(d, "rb"), flip)
    uvz = g.mask(d, "rg")
    vn = g.vertex_normal()
    a = g.abs(vn)
    step_x = g.sat(g.mul(g.sub(g.mask(a, "r"), g.mask(a, "g")), g.const(1000.0)))
    step_z = g.sat(g.mul(g.sub(g.mask(a, "b"), g.const(0.5)), g.const(1000.0)))
    uv = g.lerp(g.lerp(uvy, uvx, step_x), uvz, step_z)
    noise_tex = load_tex("T_NoiseLF")
    lp_cls = getattr(unreal, "MaterialExpressionLocalPosition", None)
    if lp_cls is not None:
        # module de mur : 290 cm le long de son X local ; teinte entiere sur les 150 cm du milieu, nulle aux bouts
        ends = g.sat(g.div(g.sub(g.const(145.0), g.abs(g.mask(g.node(lp_cls), "r"))), g.const(70.0)))
    else:
        ends = g.const(1.0)
    blot_uv = g.div(g.append(g.dot(g.mask(wp, "rg"), g.c2(0.707, 0.707)), g.mask(wp, "b")), g.const(260.0))
    blot = g.sat(g.sub(g.mul(g.tex("NoiseTex", noise_tex, blot_uv, out="G"), g.const(1.6)), g.const(0.1)))
    aged = g.lerp(g.const(1.0), g.mul(ends, blot), g.scalar("AgedFade", 0.0))
    tint = g.lerp(g.c3(1.0, 1.0, 1.0), g.vector("Tint", (1, 1, 1, 1)), aged)
    base = g.mul(g.tex("BaseColor", load_tex("T_L0K_Wallpaper_BaseColor", KIT_TEX), uv), tint)
    tide = g.custom("BRTideLine", BR_TIDE_LINE_HLSL, [
        ("WP", wp), ("NZ", g.mask(vn, "b")), ("Amount", g.scalar("TideLine", 0.0)), ("Top", g.scalar("WallTop", 290.0)),
        ("Noise", g.texobj("TideNoise", noise_tex))], output="CMOT_FLOAT3")
    g.output(g.mul(base, tide), P.MP_BASE_COLOR)
    orm = g.tex("ORM", load_tex("T_L0K_Wallpaper_ORM", KIT_TEX), uv)[0]
    g.output((orm, "R"), P.MP_AMBIENT_OCCLUSION)
    g.output((orm, "G"), P.MP_ROUGHNESS)
    g.output((orm, "B"), P.MP_METALLIC)
    g.output(g.scalar("Specular", 0.5), P.MP_SPECULAR)
    nt = g.tex("Normal", load_tex("T_L0K_Wallpaper_Normal", KIT_TEX), uv, normal=True)
    g.output(g.custom("BRAxisNormal", BR_AXIS_NORMAL_HLSL, [("T", nt), ("VN", vn)], output="CMOT_FLOAT3"), P.MP_NORMAL)


def _kit_master(name, kind):
    """Materiaux maitres du kit : 'pbr' (cartes BaseColor / ORM / Normal), 'pbrworld' (v4.15 : memes cartes projetees dans
    l'espace monde), 'color' (couleur unie), 'colortex' (v4.15 : couleur et emission d'images), 'decal' (masque)"""
    path = KIT_MAT + "/" + name
    if exists(path):
        EAL.delete_asset(path)
    m = tools().create_asset(name, KIT_MAT, unreal.Material, unreal.MaterialFactoryNew())
    safe_set(m, "used_with_instanced_static_meshes", True)
    g = Graph(m)
    P = unreal.MaterialProperty
    uv = g.uv0()
    if kind == "pbr":
        base = g.mul(g.mul(g.tex("BaseColor", load_tex("T_L0K_Wallpaper_BaseColor", KIT_TEX), uv), g.vector("Tint", (1, 1, 1, 1))),
                     g.lerp(g.const(1.0), g.node(unreal.MaterialExpressionVertexColor), g.scalar("VertexDark", 0.0)))
        # ORM : R = occlusion, G = rugosite, B = metal (un seul echantillonnage, lineaire)
        orm = g.tex("ORM", load_tex("T_L0K_Wallpaper_ORM", KIT_TEX), uv)[0]
        g.output(base, P.MP_BASE_COLOR)
        g.output((orm, "R"), P.MP_AMBIENT_OCCLUSION)
        g.output((orm, "G"), P.MP_ROUGHNESS)
        g.output((orm, "B"), P.MP_METALLIC)
        g.output(g.tex("Normal", load_tex("T_L0K_Wallpaper_Normal", KIT_TEX), uv, normal=True), P.MP_NORMAL)
        g.output(g.mul(g.tex("EmissiveTex", black_tex(), uv), g.scalar("Emissive", 0.0)), P.MP_EMISSIVE_COLOR)
    elif kind == "pbrworld":
        _kit_world_pbr(g, m, P)
    elif kind == "color":
        col = g.vector("Color", (0.8, 0.8, 0.8, 1))
        g.output(col, P.MP_BASE_COLOR)
        g.output(g.scalar("Roughness", 0.8), P.MP_ROUGHNESS)
        g.output(g.mul(col, g.scalar("Emissive", 0.0)), P.MP_EMISSIVE_COLOR)
    elif kind == "colortex":
        # v4.15 : diffuseur des neons : prismes et bords de l'image (T_L0K_Diffuser_BaseColor / _Emissive) sur toute la face
        col = g.vector("Color", (1.0, 0.97, 0.88, 1))
        diffuser = load_tex("T_L0K_Diffuser_Emissive", KIT_TEX)
        g.output(g.mul(col, g.tex("ColorTex", load_tex("T_L0K_Diffuser_BaseColor", KIT_TEX) or diffuser, uv)), P.MP_BASE_COLOR)
        g.output(g.scalar("Roughness", 0.35), P.MP_ROUGHNESS)
        g.output(g.mul(g.mul(col, g.tex("EmissiveTex", diffuser, uv)), g.scalar("Emissive", 0.0)), P.MP_EMISSIVE_COLOR)
    else:
        safe_set(m, "blend_mode", unreal.BlendMode.BLEND_MASKED)
        safe_set(m, "two_sided", True)
        t = g.tex("Decal", None, uv, out="RGBA")
        g.output((t[0], "RGB"), P.MP_BASE_COLOR)
        g.output((t[0], "A"), P.MP_OPACITY_MASK)
        g.output(g.scalar("Roughness", 0.9), P.MP_ROUGHNESS)
    finish_material(m)
    return m


def _kit_kind(spec):
    """Sorte de materiau maitre d'un materiau du kit (KIT_MATERIALS)"""
    if "tex" in spec:
        return "pbrworld" if "world" in spec else "pbr"
    if "decal" in spec:
        return "decal"
    return "colortex" if "color_tex" in spec else "color"


def _kit_masters():
    """Construit les materiaux maitres du kit. v4.15 : un maitre nouveau qui echoue retombe sur son equivalent d'avant
    (PbrWorld -> Pbr : UV des modules ; ColorTex -> Color : couleur unie) : le kit reste complet"""
    masters = {}
    for kind in ("pbr", "color", "decal", "pbrworld", "colortex"):
        try:
            masters[kind] = _kit_master(KIT_MASTERS[kind], kind)
        except Exception as e:
            warn("Kit : materiau maitre %s non construit (%s)" % (KIT_MASTERS[kind], e))
            if kind in ("pbr", "color", "decal"):
                raise
            path = KIT_MAT + "/" + KIT_MASTERS[kind]
            if exists(path):
                try:
                    EAL.delete_asset(path)
                except Exception:
                    pass
    masters.setdefault("pbrworld", masters["pbr"])
    masters.setdefault("colortex", masters["color"])
    return masters


def _kit_instance(name, parent, spec):
    path = KIT_MAT + "/" + name
    if exists(path):
        EAL.delete_asset(path)
    mi = tools().create_asset(name, KIT_MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    if "tex" in spec:
        for param, suffix in (("BaseColor", "_BaseColor"), ("ORM", "_ORM"), ("Normal", "_Normal")):
            t = load_tex(spec["tex"] + suffix, KIT_TEX)
            if t:
                MEL.set_material_instance_texture_parameter_value(mi, param, t)
            else:
                warn("Kit : texture %s%s absente pour %s" % (spec["tex"], suffix, name))
        if "tint" in spec:
            MEL.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*(tuple(spec["tint"]) + (1.0,))))
        if "vertex" in spec:
            MEL.set_material_instance_scalar_parameter_value(mi, "VertexDark", spec["vertex"])
        if "world" in spec:
            # v4.15 : echelle (cm par repetition), ligne de maree, teinte estompee du papier jauni, reflectance
            MEL.set_material_instance_scalar_parameter_value(mi, "TexScale", spec["world"])
            MEL.set_material_instance_scalar_parameter_value(mi, "TideLine", spec.get("tide", 0.0))
            MEL.set_material_instance_scalar_parameter_value(mi, "AgedFade", spec.get("aged", 0.0))
            MEL.set_material_instance_scalar_parameter_value(mi, "Specular", spec.get("spec", 0.5))
    elif "decal" in spec:
        t = load_tex(spec["decal"], KIT_TEX)
        if t:
            MEL.set_material_instance_texture_parameter_value(mi, "Decal", t)
    else:
        if "color_tex" in spec:
            # v4.15 : diffuseur texture : couleur de l'image de base, emission de l'image emissive (prismes, bords)
            MEL.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(1.0, 0.97, 0.88, 1.0))
            for param, tex_name in (("EmissiveTex", spec["color_tex"]), ("ColorTex", spec.get("base_tex", spec["color_tex"]))):
                t = load_tex(tex_name, KIT_TEX)
                if t:
                    MEL.set_material_instance_texture_parameter_value(mi, param, t)
                else:
                    warn("Kit : texture %s absente pour %s" % (tex_name, name))
        else:
            MEL.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*(tuple(spec["color"]) + (1.0,))))
        MEL.set_material_instance_scalar_parameter_value(mi, "Roughness", spec.get("rough", 0.8))
        MEL.set_material_instance_scalar_parameter_value(mi, "Emissive", spec.get("emissive", 0.0))
    EAL.save_loaded_asset(mi, only_if_is_dirty=False)
    return mi


def _fbx_has_ucx(path):
    """Le FBX contient des volumes de collision UCX_ (noms des noeuds lus dans le fichier)"""
    try:
        with open(path, "rb") as fh:
            return b"UCX_" in fh.read()
    except OSError:
        return False


def _kit_fbx_options(with_ucx=False):
    o = fbx_options()
    sm = o.get_editor_property("static_mesh_import_data")
    # Collisions : les volumes UCX_<module>_NN du FBX (convexes), jamais une boite generee a la place.
    # Unreal 5.8 importe les FBX par Interchange : l'option "auto_generate_collision" y devient bCollision, qui commande
    # AUSSI les UCX. A False, aucun UCX n'etait importe (0 collision partout, kit refuse par le jeu). Elle est donc activee
    # pour les modules qui ont des UCX (Interchange ne genere rien quand un UCX est importe) et coupee pour les autres
    # (decals, marques : aucune collision)
    safe_set(sm, "auto_generate_collision", bool(with_ucx))
    safe_set(sm, "one_convex_hull_per_ucx", True)
    # UV1 du kit : canal de lightmap sans chevauchement (lightmap_pack) ; normales et tangentes du FBX
    safe_set(sm, "generate_lightmap_u_vs", False)
    safe_set(sm, "normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    safe_set(sm, "import_uniform_scale", 1.0)
    safe_set(sm, "convert_scene", True)
    safe_set(sm, "force_front_x_axis", False)
    safe_set(sm, "vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    return o


def import_level0_kit(force=False):
    """Importe le kit du Level 0 dans /Game/Backrooms/Level0Kit (maillages, textures, materiaux), affecte les materiaux
    par nom de slot, puis verifie : rapport Saved/BackroomsSetup_Level0Kit.txt. Rejouable ; ne touche ni aux originaux
    (RawAssets) ni aux autres dossiers"""
    report = []
    fbx = _kit_files("FBX", (".fbx",))
    tex = _kit_files("Textures", IMG_EXT)
    if not fbx:
        warn("Kit du Level 0 absent (RawAssets/Level0Kit/FBX) : rien a importer, le jeu garde ses murs generes")
        return False
    if not force and all(exists(KIT_MESH + "/" + os.path.splitext(f)[0]) for f in fbx):
        log("Kit du Level 0 deja importe (%d modules) ; force=True pour le refaire" % len(fbx))
    else:
        tasks = [make_task(kit_raw("Textures", f), KIT_TEX, os.path.splitext(f)[0]) for f in tex]
        tools().import_asset_tasks(tasks)
        for f in tex:
            name = os.path.splitext(f)[0]
            t = load_tex(name, KIT_TEX)
            if not t:
                report.append("texture non importee : " + name)
                continue
            if name.endswith("_Normal"):
                safe_set(t, "srgb", False)
                safe_set(t, "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                safe_set(t, "lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
            elif name.endswith("_ORM"):
                # Donnees lineaires (occlusion, rugosite, metal) : jamais en sRGB ; compression par defaut, lue par un
                # echantillonneur "Linear Color" (une compression Masks exigerait un echantillonneur Masks)
                safe_set(t, "srgb", False)
            EAL.save_asset(KIT_TEX + "/" + name, only_if_is_dirty=False)
        masters = _kit_masters()
        for name, spec in KIT_MATERIALS.items():
            _kit_instance(name, masters[_kit_kind(spec)], spec)
        tasks = [make_task(kit_raw("FBX", f), KIT_MESH, os.path.splitext(f)[0], _kit_fbx_options(_fbx_has_ucx(kit_raw("FBX", f)))) for f in fbx]
        tools().import_asset_tasks(tasks)
    # Materiaux par nom de slot (le jeu garde les materiaux du maillage pour le kit)
    missing = []
    for f in fbx:
        name = os.path.splitext(f)[0]
        sm = unreal.load_asset(KIT_MESH + "/" + name)
        if not sm:
            missing.append(name)
            continue
        slots = list(sm.get_editor_property("static_materials"))
        for i, slot in enumerate(slots):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            mi = unreal.load_asset(KIT_MAT + "/" + slot_name) if exists(KIT_MAT + "/" + slot_name) else None
            if mi:
                slot.set_editor_property("material_interface", mi)
                slots[i] = slot
            else:
                report.append("%s : slot %s sans materiau du kit" % (name, slot_name))
        sm.set_editor_property("static_materials", slots)
        try:
            bs = sm.get_editor_property("body_setup")
            ucx = len(bs.get_editor_property("agg_geom").get_editor_property("convex_elems")) if bs else 0
        except Exception:
            ucx = -1
        report.append("%s : %d slot(s), %d collision(s) UCX" % (name, len(slots), ucx))
        EAL.save_loaded_asset(sm, only_if_is_dirty=False)
    missing_required = [n for n in KIT_REQUIRED if not exists(KIT_MESH + "/" + n)]
    status = "OK" if not missing and not missing_required else "INCOMPLET"
    lines = ["Kit du Level 0 : %s (%d modules, %d textures)" % (status, len(fbx), len(tex))]
    lines += ["  module non importe : " + n for n in missing]
    lines += ["  module requis par le jeu absent : " + n for n in missing_required]
    lines += ["  " + r for r in report]
    try:
        out = os.path.join(unreal.Paths.project_saved_dir(), "BackroomsSetup_Level0Kit.txt")
        with open(out, "w", encoding="utf-8") as fh:
            fh.write("\n".join(lines) + "\n")
    except Exception as e:
        warn("Rapport du kit non ecrit : %s" % e)
    for l in lines[:12]:
        log(l)
    return status == "OK"


def run(force=False):
    # Changement de version : tout est reimporte (les modeles et textures ont change)
    if installed_version() < VERSION:
        force = True
    tex = list_raw("Textures", IMG_EXT) if force else missing_in("Textures", TEX, IMG_EXT)
    # v4.17 : cartes derivees (combinaison) : reimportees avec la version des materiaux 10, ou si elles manquent
    der = list_raw(DERIVED_SUB, IMG_EXT) if (force or installed_material_version() < 10) else missing_in(DERIVED_SUB, TEX, IMG_EXT)
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
    ies = list_raw("Lights", IES_EXT) if force else missing_in("Lights", LIGHTS, IES_EXT)
    log("Installation v%d : %d textures, %d icones, %d sons, %d modeles, %d materiaux, %d profils IES"
        % (VERSION, len(tex), len(ico), len(snd), len(msh), len(mats), len(ies)))
    with unreal.ScopedSlowTask(7, "The Backrooms : import des ressources...") as task:
        task.make_dialog(True)
        task.enter_progress_frame(1, "Textures (%d)" % len(tex))
        import_textures(tex)
        # v4.17 : normales d'abord (la carte ORM reference la normale du meme nom pour ses mips)
        import_textures(sorted(der, key=lambda f: 0 if os.path.splitext(f)[0].endswith("_N") else 1), sub=DERIVED_SUB)
        task.enter_progress_frame(1, "Profils IES (%d)" % len(ies))
        import_light_profiles(ies)
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
        if mats or tex or der:
            build_materials()
        task.enter_progress_frame(1, "Carte")
        create_map()
    # v4.14 : kit Blender du Level 0, dans son dossier (le jeu garde ses murs generes tant qu'il manque)
    import_level0_kit(force=force)
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
