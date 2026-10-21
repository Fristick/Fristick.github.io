"""v4.17 (combinaison) : synchronisation des materiaux maitres entre le Python d'import, le C++ et le paquet (controle
statique, rapide : aucun moteur, aucun import de module Unreal).

  python -m unittest Tools/Tests/test_material_sync.py        (depuis Backrooms/)

Les materiaux maitres sont declares a cinq endroits qui doivent rester d'accord :
  - Content/Python/backrooms_setup.py : MATERIALS, USAGES, build_materials() (graphe Python de chaque maitre) ;
  - Source/Backrooms/Private/BRAssets.cpp : AssetNames, V2Params (parametre repere de version), UsageKinds (Parent()) ;
  - Source/Backrooms/Public/BRAssets.h : EParent ;
  - Source/Backrooms/Public/BRMaterialBuilder.h et Private/BRMaterialBuilder.cpp : EBRMasterMaterial, Names[] des
    materiaux construits a la volee (<nom>_Runtime), switch de Build() (graphe C++ de chaque maitre) ;
  - Tools/Build/essential_packages.txt : contenu obligatoire du paquet (check_package.py).
Un parametre repere absent d'un des deux graphes fait prendre le materiau importe pour une ancienne version (ou l'inverse) ;
un nom oublie dans essential_packages.txt laisse partir un paquet sans le materiau. Aucun test ne gardait cela avant la
v4.17 (Docs/v417/agents/materiaux_combinaison.md, § 4.4).

La classe TestGear verifie en plus l'integration de M_BR_Gear (equipement du joueur) et de ses deux cartes derivees
(RawAssets/Derived/Textures, Tools/derive_hazmat_maps.py) : elle echoue tant que cette integration n'est pas faite.
"""
import ast
import hashlib
import json
import os
import re
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

GEAR = "M_BR_Gear"
GEAR_MARKER = "RoughMapWeight"
GEAR_MAPS = ("T_Hazmat_Suit_N", "T_Hazmat_Suit_ORM")


def path(*parts):
    return os.path.join(ROOT, *parts)


def read(*parts):
    with open(path(*parts), encoding="utf-8") as handle:
        return handle.read()


def strip_comments(code):
    code = re.sub(r"/\*.*?\*/", "", code, flags=re.S)
    return re.sub(r"//[^\n]*", "", code)


def cpp_array(code, name):
    """Chaines TEXT("...") du tableau `name[] = { ... };`"""
    m = re.search(r"\b%s\[\]\s*=\s*\{(.*?)\};" % re.escape(name), strip_comments(code), re.S)
    if not m:
        raise AssertionError("tableau %s introuvable" % name)
    return re.findall(r'TEXT\("([^"]*)"\)', m.group(1))


def cpp_enum_array(code, name, enum):
    m = re.search(r"\b%s\[\]\s*=\s*\{(.*?)\};" % re.escape(name), strip_comments(code), re.S)
    if not m:
        raise AssertionError("tableau %s introuvable" % name)
    return re.findall(r"\b%s::(\w+)" % re.escape(enum), m.group(1))


def cpp_enum(code, enum):
    m = re.search(r"enum\s+class\s+%s\s*:\s*\w+\s*\{(.*?)\};" % re.escape(enum), strip_comments(code), re.S)
    if not m:
        raise AssertionError("enum %s introuvable" % enum)
    return [e.split("=")[0].strip() for e in m.group(1).split(",") if e.strip()]


def cpp_function_body(code, name):
    """Corps de la fonction `name(` (accolades equilibrees), ou None"""
    m = re.search(r"\b(?:void|UMaterial\*|static\s+void)\s+%s\s*\(" % re.escape(name), code)
    if not m:
        return None
    start = code.index("{", m.end())
    depth = 0
    for i in range(start, len(code)):
        if code[i] == "{":
            depth += 1
        elif code[i] == "}":
            depth -= 1
            if depth == 0:
                return code[start:i + 1]
    return None


def cpp_build_functions(builder_cpp):
    """EBRMasterMaterial -> fonction Build* appelee dans le switch de Build()"""
    out = {}
    code = strip_comments(builder_cpp)
    cases = list(re.finditer(r"case\s+EBRMasterMaterial::(\w+)\s*:", code))
    for i, m in enumerate(cases):
        end = cases[i + 1].start() if i + 1 < len(cases) else len(code)
        body = re.split(r"\bbreak\s*;|\breturn\b", code[m.end():end])[0]
        call = re.search(r"\b(Build\w+)\s*\(\s*G\b", body)
        if call:
            out[m.group(1)] = call.group(1)
    return out


def cpp_params(builder_cpp, function):
    """Noms TEXT("...") du corps de la fonction et des fonctions du fichier qu'elle appelle (un niveau)"""
    body = cpp_function_body(builder_cpp, function) or ""
    names = set(re.findall(r'TEXT\("([^"]*)"\)', body))
    for callee in set(re.findall(r"\b(Build\w+|\w+HLSL)\s*\(", body)) - {function}:
        names |= set(re.findall(r'TEXT\("([^"]*)"\)', cpp_function_body(builder_cpp, callee) or ""))
    return names


class Setup:
    """backrooms_setup.py lu par ast (le module importe `unreal`, absent hors de l'editeur)"""

    def __init__(self):
        self.tree = ast.parse(read("Content", "Python", "backrooms_setup.py"))
        self.funcs = {n.name: n for n in self.tree.body if isinstance(n, ast.FunctionDef)}
        self.values = {}
        for node in self.tree.body:
            if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
                self.values[node.targets[0].id] = node.value

    def constant(self, name):
        node = self.values.get(name)
        if node is None:
            return None
        try:
            return ast.literal_eval(node)
        except ValueError:
            return node

    def dict_keys(self, name):
        node = self.values.get(name)
        if not isinstance(node, ast.Dict):
            return None
        return [ast.literal_eval(k) for k in node.keys]

    def jobs(self):
        """Materiau -> fonction Python qui le construit (tuple `jobs` de build_materials)"""
        fn = self.funcs.get("build_materials")
        if fn is None:
            raise AssertionError("build_materials() introuvable")
        out = {}
        for node in ast.walk(fn):
            if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "jobs" for t in node.targets):
                for item in node.value.elts:
                    name, target = item.elts
                    if isinstance(target, ast.Lambda):
                        target = target.body.func
                    out[ast.literal_eval(name)] = target.id
        return out

    def strings(self, function):
        """Premiers arguments textuels des appels (g.scalar("Nom", ...), g.tex("Nom", ...)) dans la fonction et dans les
        fonctions du module qu'elle appelle (un niveau) : les noms de parametres du graphe, sans les docstrings"""
        fn = self.funcs.get(function)
        if fn is None:
            return set()

        def args_of(tree):
            out, called = set(), set()
            for node in ast.walk(tree):
                if isinstance(node, ast.Call):
                    if node.args and isinstance(node.args[0], ast.Constant) and isinstance(node.args[0].value, str):
                        out.add(node.args[0].value)
                    if isinstance(node.func, ast.Name) and node.func.id in self.funcs:
                        called.add(node.func.id)
            return out, called

        found, callees = args_of(fn)
        for callee in callees - {function}:
            found |= args_of(self.funcs[callee])[0]
        return found


def essential_packages():
    lines = read("Tools", "Build", "essential_packages.txt").splitlines()
    return {ln.strip() for ln in lines if ln.strip() and not ln.strip().startswith("#")}


class Sources:
    def __init__(self):
        self.assets_cpp = read("Source", "Backrooms", "Private", "BRAssets.cpp")
        self.assets_h = read("Source", "Backrooms", "Public", "BRAssets.h")
        self.builder_cpp = read("Source", "Backrooms", "Private", "BRMaterialBuilder.cpp")
        self.builder_h = read("Source", "Backrooms", "Public", "BRMaterialBuilder.h")
        self.asset_names = cpp_array(self.assets_cpp, "AssetNames")
        self.v2_params = cpp_array(self.assets_cpp, "V2Params")
        self.usage_kinds = cpp_enum_array(self.assets_cpp, "UsageKinds", "EBRMasterMaterial")
        self.runtime_names = cpp_array(self.builder_cpp, "Names")
        self.masters = cpp_enum(self.builder_h, "EBRMasterMaterial")
        self.parents = [p for p in cpp_enum(self.assets_h, "EParent") if p != "Count"]
        self.builds = cpp_build_functions(self.builder_cpp)
        self.setup = Setup()


SRC = None


def sources():
    global SRC
    if SRC is None:
        SRC = Sources()
    return SRC


class TestMasterSync(unittest.TestCase):
    """Les memes materiaux maitres partout, dans le meme ordre, avec leur parametre repere dans les deux graphes"""

    def test_tables_have_one_entry_per_master(self):
        s = sources()
        n = len(s.asset_names)
        self.assertEqual(len(s.v2_params), n, "V2Params : un parametre repere par materiau de AssetNames")
        self.assertEqual(len(s.usage_kinds), n, "UsageKinds : un type par materiau de AssetNames")
        self.assertEqual(len(s.parents), n, "EParent : une entree par materiau de AssetNames")
        self.assertEqual(len(s.runtime_names), len(s.masters), "Names[] : un nom par EBRMasterMaterial")

    def test_python_materials_match_cpp(self):
        s = sources()
        materials = list(s.setup.constant("MATERIALS"))
        self.assertEqual(materials, s.asset_names, "MATERIALS (Python) et AssetNames (BRAssets.cpp) different")
        usages = s.setup.dict_keys("USAGES")
        self.assertIsNotNone(usages, "USAGES introuvable")
        self.assertEqual(sorted(usages), sorted(materials), "USAGES : un jeu d'usages par materiau de MATERIALS")
        self.assertEqual(sorted(s.setup.jobs()), sorted(materials), "build_materials() : une fonction par materiau de MATERIALS")

    def test_runtime_names_follow_the_enum(self):
        """Names[i] est le nom du materiau de EBRMasterMaterial i : chaque materiau importe a son jumeau construit en C++"""
        s = sources()
        for name, kind in zip(s.asset_names, s.usage_kinds):
            self.assertIn(kind, s.masters, "%s : EBRMasterMaterial::%s inconnu" % (name, kind))
            idx = s.masters.index(kind)
            self.assertLess(idx, len(s.runtime_names), "Names[] trop court pour %s" % kind)
            self.assertEqual(s.runtime_names[idx], name + "_Runtime",
                             "Names[%d] (EBRMasterMaterial::%s) devrait etre %s_Runtime" % (idx, kind, name))

    def test_marker_parameter_in_both_graphs(self):
        s = sources()
        jobs = s.setup.jobs()
        for name, kind, marker in zip(s.asset_names, s.usage_kinds, s.v2_params):
            with self.subTest(materiau=name, repere=marker):
                fn = s.builds.get(kind)
                self.assertIsNotNone(fn, "switch de Build() : aucune fonction pour EBRMasterMaterial::%s" % kind)
                self.assertIn(marker, cpp_params(s.builder_cpp, fn), "%s : %s absent du graphe C++ (%s)" % (name, marker, fn))
                self.assertIn(marker, s.setup.strings(jobs[name]), "%s : %s absent du graphe Python (%s)" % (name, marker, jobs[name]))

    def test_essential_packages_list_every_master(self):
        s = sources()
        pkgs = essential_packages()
        for name in s.asset_names:
            self.assertIn("/Game/Backrooms/Materials/" + name, pkgs, "%s absent de Tools/Build/essential_packages.txt" % name)


class TestGear(unittest.TestCase):
    """v4.17 : M_BR_Gear (combinaison, masque, visiere) et ses cartes derivees, integres partout"""

    def test_gear_declared_everywhere(self):
        s = sources()
        self.assertIn(GEAR, s.setup.constant("MATERIALS"), "MATERIALS (backrooms_setup.py)")
        self.assertIn(GEAR, s.asset_names, "AssetNames (BRAssets.cpp)")
        self.assertIn(GEAR + "_Runtime", s.runtime_names, "Names[] (BRMaterialBuilder.cpp)")
        self.assertIn("Gear", s.masters, "EBRMasterMaterial (BRMaterialBuilder.h)")
        self.assertIn("Gear", s.parents, "EParent (BRAssets.h)")
        self.assertIn("/Game/Backrooms/Materials/" + GEAR, essential_packages(), "essential_packages.txt")

    def test_gear_marker_and_parameters(self):
        s = sources()
        self.assertIn(GEAR, s.asset_names, "AssetNames (BRAssets.cpp)")
        self.assertEqual(s.v2_params[s.asset_names.index(GEAR)], GEAR_MARKER, "parametre repere de M_BR_Gear dans V2Params")
        cpp = cpp_params(s.builder_cpp, s.builds.get("Gear", "BuildGear"))
        py = s.setup.strings(s.setup.jobs().get(GEAR, "build_gear_material"))
        # noms du § 2.1, lus par le jeu (BRAssets.cpp, SlotMaterial) et par le test v4.8 (BRAutoTestV48.cpp)
        for param in ("BaseTex", "Tint", "TexScale", "Roughness", "Metallic", "NormalTex", "MaskTex", "NormalStrength",
                      "RoughMapWeight", "CavityWeight", "MetalMapWeight", "Specular", "Saturation", "Emissive"):
            with self.subTest(parametre=param):
                self.assertIn(param, cpp, "%s absent du graphe C++ de M_BR_Gear" % param)
                self.assertIn(param, py, "%s absent du graphe Python de M_BR_Gear" % param)
        usages = s.setup.values.get("USAGES")
        keys = s.setup.dict_keys("USAGES") or []
        self.assertIn(GEAR, keys, "USAGES (backrooms_setup.py)")
        gear_usage = usages.values[keys.index(GEAR)]
        self.assertTrue(isinstance(gear_usage, ast.Name) and gear_usage.id == "_MODEL_USAGES",
                        "USAGES[M_BR_Gear] : instances, maillage a squelette et Nanite (_MODEL_USAGES)")

    def test_gear_maps_shipped(self):
        pkgs = essential_packages()
        s = sources()
        listed = s.setup.constant("GEAR_TEXTURES")
        self.assertEqual(tuple(listed or ()), GEAR_MAPS, "GEAR_TEXTURES (backrooms_setup.py)")
        for name in GEAR_MAPS:
            with self.subTest(carte=name):
                self.assertIn("/Game/Backrooms/Textures/" + name, pkgs, "%s absent de essential_packages.txt" % name)
                self.assertTrue(os.path.isfile(path("RawAssets", "Derived", "Textures", name + ".png")),
                                "RawAssets/Derived/Textures/%s.png absent (Tools/derive_hazmat_maps.py)" % name)
                self.assertIn('TEXT("%s")' % name, s.assets_cpp, "%s non affectee a un slot (BRAssets.cpp, SlotStyles)" % name)

    def test_gear_maps_derive_from_the_protected_atlas(self):
        """La provenance des cartes livrees cite l'atlas protege actuel (sinon : relancer Tools/derive_hazmat_maps.py)"""
        with open(path("RawAssets", "Derived", "Textures", "T_Hazmat_Suit_maps.json"), encoding="utf-8") as handle:
            prov = json.load(handle)
        with open(path("RawAssets", "protected_assets.json"), encoding="utf-8") as handle:
            reg = json.load(handle)
        self.assertEqual(prov["atlas_sha256"], reg["derived"]["RawAssets/Textures/T_Hazmat_Suit.jpg"]["sha256"])
        for name in GEAR_MAPS:
            with open(path("RawAssets", "Derived", "Textures", name + ".png"), "rb") as handle:
                self.assertEqual(hashlib.sha256(handle.read()).hexdigest(), prov["outputs"][name + ".png"]["file_sha256"],
                                 "%s.png ne correspond pas a sa provenance" % name)


if __name__ == "__main__":
    unittest.main()
