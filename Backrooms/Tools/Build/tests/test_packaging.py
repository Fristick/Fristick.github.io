"""v4.10 : tests de check_package.py et de make_steam_vdf.py sur des paquets fabriques (aucun moteur, aucun envoi).
v4.11 : droits d'execution poses par la fixture sur POSIX (et cas sans droits), lancement simule (echec, incomplet,
rapport absent, vide, d'un autre essai, incoherent), binaires PE / ELF / Mach-O tronques ou a offsets hors limites.

  python -m unittest discover -s Tools/Build/tests -v

Les paquets sont construits dans un dossier temporaire : vrais en-tetes PE / ELF minimaux, .pak avec la signature de fin,
.utoc avec sa signature, .ucas. L'outil de liste (UnrealPak) est remplace par un script qui lit des listes preparees par le
test (hors du paquet) : listes completes, incompletes, ou outil en echec.
"""
import contextlib
import io
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS_BUILD = os.path.abspath(os.path.join(HERE, ".."))
ROOT = os.path.abspath(os.path.join(TOOLS_BUILD, "..", ".."))
sys.path.insert(0, TOOLS_BUILD)
sys.path.insert(0, os.path.join(ROOT, "Tools", "Steam"))

import check_package  # noqa: E402
import make_steam_vdf  # noqa: E402

FAKE_TOOL = r'''
import csv, os, sys
lists = os.environ["BR_FAKE_LISTS"]
args = sys.argv[1:]
if args and args[0].startswith("-ListContainer="):
    target = args[0][len("-ListContainer="):]
    out = [a for a in args if a.startswith("-csv=")][0][len("-csv="):]
else:
    target = args[0]
    out = None
name = os.path.basename(target)
if os.path.exists(os.path.join(lists, name + ".fail")):
    print("LogPakFile: Error: cannot open", target)
    sys.exit(1)
entries = open(os.path.join(lists, name + ".list"), encoding="utf-8").read().split()
if out:
    with open(out, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["PackageName", "Size"])
        for e in entries:
            w.writerow([e, 100])
else:
    for e in entries:
        print('LogPakFile: Display: "%s" offset: 0, size: 10 bytes, sha1: 00' % e)
'''


def pe_bytes():
    """En-tete PE32+ x64 minimal (aucune section, aucune importation)"""
    dos = bytearray(64)
    dos[0:2] = b"MZ"
    struct.pack_into("<I", dos, 0x3C, 0x40)
    coff = b"PE\0\0" + struct.pack("<HHIIIHH", 0x8664, 0, 0, 0, 0, 240, 0x22)
    opt = bytearray(240)
    struct.pack_into("<H", opt, 0, 0x20B)
    struct.pack_into("<I", opt, 108, 16)
    return bytes(dos) + coff + bytes(opt)


def elf_bytes():
    """En-tete ELF 64 bits x86_64 minimal (aucun segment)"""
    h = bytearray(64)
    h[0:4] = b"\x7fELF"
    h[4] = 2
    h[5] = 1
    struct.pack_into("<H", h, 18, 62)
    return bytes(h)


def write(path, data=b"x"):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as fh:
        fh.write(data if isinstance(data, bytes) else data.encode("utf-8"))


class Fixture(object):
    """Arbre de construction : <build>/<Plateforme>/<Config>/<Dossier du jeu>, listes de l'outil, projet"""

    def __init__(self, tmp):
        self.tmp = tmp
        self.build = os.path.join(tmp, "Build")
        self.lists = os.path.join(tmp, "lists")
        self.project = os.path.join(tmp, "Project")
        os.makedirs(self.lists)
        write(os.path.join(self.project, "Config", "DefaultGame.ini"), "[/Script/UnrealEd.ProjectPackagingSettings]\nbUseIoStore=True\n")
        self.tool = os.path.join(tmp, "fake_unrealpak.py")
        write(self.tool, FAKE_TOOL)
        os.environ["BR_FAKE_LISTS"] = self.lists

    def archive(self, platform, config="Shipping"):
        return os.path.join(self.build, platform, config)

    def make(self, platform="Windows", config="Shipping", commit="c0ffee", skip_files=(), pak_entries=None, packages=None, tree="propre",
             exec_bits=True):
        archive = self.archive(platform, config)
        game = os.path.join(archive, platform)
        staged = []

        def put(rel, data=b"x", list_it=True):
            if rel in skip_files:
                return
            write(os.path.join(game, rel), data)
            if list_it:
                staged.append(rel)

        sub = "Win64" if platform == "Windows" else "Linux"
        if platform == "Windows":
            put("Backrooms.exe", pe_bytes())
            put("Backrooms/Binaries/Win64/Backrooms-Win64-Shipping.exe", pe_bytes())
        else:
            put("Backrooms.sh", b"#!/bin/sh\nexec ./Backrooms/Binaries/Linux/Backrooms-Linux-Shipping \"$@\"\n")
            put("Backrooms/Binaries/Linux/Backrooms-Linux-Shipping", elf_bytes())
        paks = "Backrooms/Content/Paks/"
        put(paks + "pakchunk0-%s.pak" % platform, b"\0" * 200 + struct.pack("<I", 0x5A6F12E1) + b"\0" * 40, list_it=False)
        put(paks + "global.utoc", check_package.UTOC_MAGIC + b"\0" * 32, list_it=False)
        put(paks + "global.ucas", b"\0" * 32, list_it=False)
        put(paks + "pakchunk0-%s.utoc" % platform, check_package.UTOC_MAGIC + b"\0" * 32, list_it=False)
        put(paks + "pakchunk0-%s.ucas" % platform, b"\0" * 64, list_it=False)
        if pak_entries is None:
            pak_entries = ["Backrooms/Content/Fonts/%s" % f for f in check_package.FONTS]
            pak_entries += ["Backrooms/Content/Localization/Game/%s/Game.locres" % c for c in check_package.CULTURES]
            pak_entries += ["Backrooms/Content/Localization/Game/Game.locmeta"]
        if packages is None:
            packages = [check_package.MAP_PACKAGE] + check_package.essential_packages()
        write(os.path.join(self.lists, "pakchunk0-%s.pak.list" % platform), "\n".join(pak_entries))
        write(os.path.join(self.lists, "pakchunk0-%s.utoc.list" % platform), "\n".join(packages))
        write(os.path.join(game, "Manifest_NonUFSFiles_%s.txt" % sub), "".join("%s\t2026-10-06T00:00:00\n" % s for s in staged))
        write(os.path.join(archive, "build_manifest.txt"), "projet : Backrooms\ncommit : %s\narbre : %s\nconfiguration : %s\n" % (commit, tree, config))
        # v4.11 : sur POSIX, BuildCookRun livre le lanceur et le binaire executables ; la fixture fait de meme (sans quoi le
        # controleur refuse a juste titre le paquet). exec_bits=False fabrique le paquet fautif.
        if platform != "Windows" and os.name != "nt":
            for rel in ("Backrooms.sh", "Backrooms/Binaries/Linux/Backrooms-Linux-Shipping"):
                path = os.path.join(game, rel)
                if os.path.exists(path):
                    os.chmod(path, 0o755 if exec_bits else 0o644)
        return archive


FAKE_GAME = r'''
import json, os, sys
scenario = json.loads(os.environ["BR_FAKE_GAME"])
args = sys.argv[1:]
out = [a for a in args if a.startswith("-BRSmokeOut=")][0][len("-BRSmokeOut="):]
with open(os.environ["BR_FAKE_ARGS"], "w") as fh:
    fh.write("\n".join(args))
rep = scenario.get("rapport")
if rep is not None:
    if rep == "VIDE":
        rep = {}
    elif rep == "TEXTE":
        rep = None
    else:
        rep = dict(rep)
        rep.setdefault("ligne_de_commande", " ".join(args) if scenario.get("meme_essai", True) else "Backrooms -BRSmokeTest -BRSmokeOut=/ailleurs/smoke.json")
    with open(out, "w", encoding="utf-8") as fh:
        if rep is None:
            fh.write("pas du json")
        else:
            json.dump(rep, fh)
sys.exit(scenario.get("code", 0))
'''


def run_check(argv):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        code = check_package.main(argv)
    return code, buf.getvalue()


def run_vdf(argv):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        code = make_steam_vdf.main(argv)
    return code, buf.getvalue()


class CheckPackageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="br_pkgtest_")
        self.f = Fixture(self.tmp)

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def check(self, archive, *extra, platform="Windows", tool=True):
        argv = ["--platform", platform, "--dir", archive, "--project", self.f.project, "--json", os.path.join(self.tmp, "r.json")]
        if tool:
            argv += ["--unrealpak", self.f.tool]
        code, out = run_check(argv + list(extra))
        rep = {}
        if os.path.exists(os.path.join(self.tmp, "r.json")):
            with open(os.path.join(self.tmp, "r.json"), encoding="utf-8") as fh:
                rep = json.load(fh)
        return code, out, rep

    def test_paquet_complet(self):
        code, out, rep = self.check(self.f.make())
        self.assertEqual(code, 0, out)
        self.assertEqual(rep["verifications"]["contenu"]["etat"], "OK")
        self.assertIn("PAQUET VERIFIE", out)

    def test_paquet_linux_complet(self):
        code, out, _ = self.check(self.f.make("Linux"), platform="Linux")
        self.assertEqual(code, 0, out)

    def test_dossier_vide(self):
        empty = os.path.join(self.tmp, "Build", "Windows", "Shipping")
        os.makedirs(empty)
        code, out, rep = self.check(empty)
        self.assertEqual(code, 1, out)
        self.assertEqual(rep["verifications"]["structure"]["etat"], "ECHEC")

    def test_dossier_absent(self):
        code, out, _ = self.check(os.path.join(self.tmp, "nulle_part"))
        self.assertEqual(code, 1, out)

    def test_ucas_absent(self):
        code, out, _ = self.check(self.f.make(skip_files=("Backrooms/Content/Paks/pakchunk0-Windows.ucas",)))
        self.assertEqual(code, 1, out)
        self.assertIn(".ucas", out)

    def test_outil_absent(self):
        code, out, rep = self.check(self.f.make(), tool=False)
        self.assertEqual(code, 2, out)
        self.assertEqual(rep["verifications"]["contenu"]["etat"], "NON VERIFIE")
        self.assertIn("INSPECTION INCOMPLETE", out)

    def test_outil_en_echec(self):
        archive = self.f.make()
        write(os.path.join(self.f.lists, "pakchunk0-Windows.pak.fail"), "")
        code, out, rep = self.check(archive)
        self.assertEqual(code, 1, out)
        self.assertEqual(rep["verifications"]["contenu"]["etat"], "ECHEC")

    def test_carte_absente(self):
        code, out, _ = self.check(self.f.make(packages=[p for p in check_package.essential_packages() if p != check_package.MAP_PACKAGE]))
        self.assertEqual(code, 1, out)
        self.assertIn("carte absente", out)

    def test_langue_absente(self):
        entries = ["Backrooms/Content/Fonts/%s" % f for f in check_package.FONTS]
        entries += ["Backrooms/Content/Localization/Game/%s/Game.locres" % c for c in check_package.CULTURES if c != "ja"]
        entries += ["Backrooms/Content/Localization/Game/Game.locmeta"]
        code, out, _ = self.check(self.f.make(pak_entries=entries))
        self.assertEqual(code, 1, out)
        self.assertIn("langue absente : ja", out)

    def test_ressource_essentielle_absente(self):
        packages = [check_package.MAP_PACKAGE] + [p for p in check_package.essential_packages() if not p.endswith("SK_Hazmat")]
        code, out, _ = self.check(self.f.make(packages=packages))
        self.assertEqual(code, 1, out)
        self.assertIn("SK_Hazmat", out)

    def test_extension_sans_format(self):
        archive = self.f.make()
        write(os.path.join(archive, "Windows", "Backrooms", "Binaries", "Win64", "Backrooms-Win64-Shipping.exe"), b"pas un executable")
        code, out, _ = self.check(archive)
        self.assertEqual(code, 1, out)

    def test_pak_sans_signature(self):
        archive = self.f.make()
        write(os.path.join(archive, "Windows", "Backrooms", "Content", "Paks", "pakchunk0-Windows.pak"), b"\0" * 300)
        code, out, _ = self.check(archive)
        self.assertEqual(code, 1, out)

    def test_systeme_inconnu(self):
        code, out = run_check(["--platform", "Amiga", "--dir", self.tmp])
        self.assertEqual(code, 3, out)

    def test_mode_structure_le_dit(self):
        code, out, rep = self.check(self.f.make(), "--mode", "structure", tool=False)
        self.assertEqual(code, 0, out)
        self.assertIn("STRUCTUREL", rep["resultat"])
        self.assertEqual(rep["verifications"]["contenu"]["etat"], "NON VERIFIE")

    # ----------------------------------------------------------------------------------------------------- v4.11 : droits
    def test_paquet_linux_sans_droits(self):
        if os.name == "nt":
            self.skipTest("droits POSIX illisibles sous Windows")
        code, out, rep = self.check(self.f.make("Linux", exec_bits=False), platform="Linux")
        self.assertEqual(code, 1, out)
        self.assertEqual(rep["verifications"]["structure"]["etat"], "ECHEC")
        self.assertIn("droit d'execution", out)

    # ----------------------------------------------------------------------------------------------------- v4.11 : lancement
    def launch(self, scenario, *flags, platform="Linux"):
        """Lance le controle avec un jeu simule (script Python) a la place du paquet, comme si l'hote etait du bon systeme"""
        game = os.path.join(self.tmp, "fake_game.py")
        write(game, FAKE_GAME)
        args_file = os.path.join(self.tmp, "args.txt")
        os.environ["BR_FAKE_GAME"] = json.dumps(scenario)
        os.environ["BR_FAKE_ARGS"] = args_file
        saved = check_package.launch_command, check_package.host_platform
        check_package.launch_command = lambda plat, root, launcher: [sys.executable, game]
        check_package.host_platform = lambda: platform
        try:
            code, out, rep = self.check(self.f.make(platform), *flags, platform=platform)
        finally:
            check_package.launch_command, check_package.host_platform = saved
        args = []
        if os.path.exists(args_file):
            with open(args_file) as fh:
                args = fh.read().split("\n")
        return code, out, rep, args

    OK_REPORT = {"etat": "reussi", "jeu": "v4.11", "problemes": [], "non_verifies": []}

    def test_lancement_reussi(self):
        code, out, rep, args = self.launch({"code": 0, "rapport": self.OK_REPORT}, "--require-launch")
        self.assertEqual(code, 0, out)
        self.assertIn("ET LANCE", out)
        self.assertIn("-BRAutoTestStrict", args)

    def test_lancement_demande_echoue_fait_echouer(self):
        # Constat 2.4 : --launch, code 1, etat echec -> la commande doit echouer (avant : PAQUET VERIFIE, code 0)
        rapport = dict(self.OK_REPORT, etat="echec", problemes=["Niveau 0 : carte absente"])
        code, out, rep, _ = self.launch({"code": 1, "rapport": rapport}, "--launch")
        self.assertEqual(code, 1, out)
        self.assertEqual(rep["verifications"]["lancement"]["etat"], "ECHEC")
        self.assertIn("ECHEC", rep["resultat"])

    def test_lancement_obligatoire_incomplet(self):
        # Constat 2.4 : --require-launch, code 0, etat incomplet, non_verifies -> statut incomplet (avant : VERIFIE ET LANCE)
        rapport = dict(self.OK_REPORT, etat="incomplet", non_verifies=["polices : aucun texte mesure"])
        code, out, rep, _ = self.launch({"code": 0, "rapport": rapport}, "--require-launch")
        self.assertEqual(code, 2, out)
        self.assertEqual(rep["verifications"]["lancement"]["etat"], "INCOMPLET")
        self.assertNotIn("ET LANCE", out)

    def test_lancement_incomplet_code_2(self):
        rapport = dict(self.OK_REPORT, etat="incomplet", non_verifies=["polices : aucun texte mesure"])
        code, out, rep, _ = self.launch({"code": 2, "rapport": rapport}, "--require-launch")
        self.assertEqual(code, 2, out)
        self.assertEqual(rep["verifications"]["lancement"]["etat"], "INCOMPLET")
        self.assertIn("INSPECTION INCOMPLETE", rep["resultat"])

    def test_lancement_demande_incomplet(self):
        rapport = dict(self.OK_REPORT, etat="incomplet", non_verifies=["sol : non mesure"])
        code, out, rep, _ = self.launch({"code": 2, "rapport": rapport}, "--launch")
        self.assertEqual(code, 2, out)

    def test_lancement_json_vide(self):
        code, out, rep, _ = self.launch({"code": 0, "rapport": "VIDE"}, "--require-launch")
        self.assertEqual(code, 1, out)
        self.assertIn("champ(s) absent(s)", out)

    def test_lancement_json_illisible(self):
        code, out, rep, _ = self.launch({"code": 0, "rapport": "TEXTE"}, "--require-launch")
        self.assertEqual(code, 1, out)
        self.assertIn("illisible", out)

    def test_lancement_sans_rapport(self):
        code, out, rep, _ = self.launch({"code": 0}, "--require-launch")
        self.assertEqual(code, 1, out)
        self.assertIn("aucun rapport", out)

    def test_lancement_rapport_d_un_autre_essai(self):
        code, out, rep, _ = self.launch({"code": 0, "rapport": self.OK_REPORT, "meme_essai": False}, "--require-launch")
        self.assertEqual(code, 1, out)
        self.assertIn("ne correspond pas", out)

    def test_lancement_code_non_nul_avec_rapport_reussi(self):
        code, out, rep, _ = self.launch({"code": 3, "rapport": self.OK_REPORT}, "--require-launch")
        self.assertEqual(code, 1, out)
        self.assertIn("code de sortie 3", out)

    def test_lancement_rapport_incoherent(self):
        rapport = dict(self.OK_REPORT, problemes=["un probleme"])
        code, out, rep, _ = self.launch({"code": 0, "rapport": rapport}, "--require-launch")
        self.assertEqual(code, 1, out)

    def test_lancement_etat_inconnu(self):
        rapport = dict(self.OK_REPORT, etat="ok")
        code, out, rep, _ = self.launch({"code": 0, "rapport": rapport}, "--require-launch")
        self.assertEqual(code, 1, out)
        self.assertIn("etat inconnu", out)

    def test_lancement_windows_simule(self):
        rapport = dict(self.OK_REPORT, etat="echec", problemes=["plantage"])
        code, out, rep, _ = self.launch({"code": 1, "rapport": rapport}, "--launch", platform="Windows")
        self.assertEqual(code, 1, out)

    def test_lancement_exige_hors_hote(self):
        # Paquet Linux controle depuis un autre systeme : lancement impossible -> inspection incomplete, jamais un succes
        if sys.platform.startswith("linux"):
            self.skipTest("hote Linux")
        code, out, rep = self.check(self.f.make("Linux"), "--require-launch", platform="Linux")
        self.assertEqual(code, 2, out)
        self.assertEqual(rep["verifications"]["lancement"]["etat"], "NON VERIFIE")


class SteamVdfTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="br_vdftest_")
        self.f = Fixture(self.tmp)
        self.out = os.path.join(self.f.build, "SteamPipe")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def vdf(self, *extra, platforms="Windows", app="1000", commit="c0ffee"):
        argv = ["--app", app, "--depot-windows", "1001", "--depot-linux", "1002", "--depot-mac", "1003", "--platforms", platforms,
                "--build-root", self.f.build, "--project", self.f.project, "--commit", commit, "--unrealpak", self.f.tool]
        return run_vdf(argv + list(extra))

    def test_ecrit_apercu_sans_publication(self):
        self.f.make()
        code, out = self.vdf()
        self.assertEqual(code, 0, out)
        with open(os.path.join(self.out, "app_build.vdf"), encoding="utf-8") as fh:
            app = fh.read()
        self.assertIn('"Preview" "1"', app)
        self.assertIn('"SetLive" ""', app)
        self.assertTrue(os.path.isfile(os.path.join(self.out, "depot_windows.vdf")))

    def test_dossier_vide_refuse(self):
        os.makedirs(os.path.join(self.f.archive("Windows"), "Windows"))
        write(os.path.join(self.f.archive("Windows"), "build_manifest.txt"), "commit : c0ffee\n")
        code, out = self.vdf()
        self.assertEqual(code, 1, out)
        self.assertIn("vide", out)
        self.assertFalse(os.path.exists(self.out))

    def test_paquet_absent_refuse(self):
        code, out = self.vdf()
        self.assertEqual(code, 1, out)
        self.assertFalse(os.path.exists(self.out))

    def test_un_paquet_invalide_parmi_deux(self):
        self.f.make("Windows")
        self.f.make("Linux", skip_files=("Backrooms/Content/Paks/pakchunk0-Linux.ucas",))
        # Fichiers d'une preparation precedente : gardes tels quels
        write(os.path.join(self.out, "app_build.vdf"), "ancien")
        code, out = self.vdf(platforms="Windows,Linux")
        self.assertEqual(code, 1, out)
        self.assertIn("Linux", out)
        with open(os.path.join(self.out, "app_build.vdf"), encoding="utf-8") as fh:
            self.assertEqual(fh.read(), "ancien")
        self.assertFalse(os.path.exists(os.path.join(self.out, "depot_windows.vdf")))

    def test_contenu_non_verifie_refuse(self):
        self.f.make()
        argv = ["--app", "1000", "--depot-windows", "1001", "--platforms", "Windows", "--build-root", self.f.build, "--project", self.f.project,
                "--commit", "c0ffee"]
        code, out = run_vdf(argv)  # aucun outil de liste : inspection incomplete -> refus
        self.assertEqual(code, 1, out)
        self.assertIn("INSPECTION INCOMPLETE", out)

    def test_systeme_inconnu(self):
        code, out = self.vdf(platforms="Windows,Amiga")
        self.assertEqual(code, 3, out)

    def test_identifiants_invalides(self):
        self.f.make()
        self.assertEqual(self.vdf(app="0")[0], 3)
        self.assertEqual(self.vdf(app="abc")[0], 3)
        code, out = run_vdf(["--app", "1000", "--depot-windows", "1001", "--depot-linux", "1001", "--platforms", "Windows,Linux",
                             "--build-root", self.f.build, "--commit", "c0ffee"])
        self.assertEqual(code, 3, out)

    def test_autre_commit_refuse(self):
        self.f.make(commit="deadbeef")
        code, out = self.vdf()
        self.assertEqual(code, 1, out)
        self.assertIn("deadbeef", out)

    def test_residu_refuse(self):
        archive = self.f.make()
        write(os.path.join(archive, "Windows", "Backrooms", "Binaries", "Win64", "Ancien-Plugin.dll"), pe_bytes())
        code, out = self.vdf()
        self.assertEqual(code, 1, out)
        self.assertIn("Ancien-Plugin.dll", out)

    def test_conteneur_ancien_refuse(self):
        archive = self.f.make()
        old = os.path.join(archive, "Windows", "Backrooms", "Content", "Paks", "pakchunk1-Windows.pak")
        write(old, b"\0" * 200 + struct.pack("<I", 0x5A6F12E1) + b"\0" * 40)
        t = time.time() - 3 * 86400
        os.utime(old, (t, t))
        write(os.path.join(self.f.lists, "pakchunk1-Windows.pak.list"), "Backrooms/Content/Old.txt")
        code, out = self.vdf()
        self.assertEqual(code, 1, out)
        self.assertIn("pakchunk1-Windows.pak", out)

    def test_arbre_modifie_refuse(self):
        self.f.make(tree="modifie")
        code, out = self.vdf()
        self.assertEqual(code, 1, out)
        self.assertIn("non validees", out)
        code, out = self.vdf("--allow-dirty")
        self.assertEqual(code, 0, out)

    def test_manifeste_absent_refuse(self):
        archive = self.f.make()
        os.remove(os.path.join(archive, "build_manifest.txt"))
        code, out = self.vdf()
        self.assertEqual(code, 1, out)

    def test_aucun_envoi(self):
        # Le script ne lance jamais steamcmd (ni aucun programme autre que python et git)
        with open(os.path.join(ROOT, "Tools", "Steam", "make_steam_vdf.py"), encoding="utf-8") as fh:
            src = fh.read()
        self.assertNotIn("steamcmd\"", src.replace("'", "\""))
        self.assertNotIn("run_app_build", src)


if __name__ == "__main__":
    unittest.main()


class BinaryFormatTests(unittest.TestCase):
    """v4.11 (constat 2.5) : fichiers tronques ou a offsets hors limites : un ECHEC avec son explication, jamais une
    exception non geree"""

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="br_fmt_")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def file(self, name, data):
        path = os.path.join(self.tmp, name)
        write(path, data)
        return path

    def assert_bad(self, reader, data, name="f"):
        info, err = check_package.binary_info(reader, self.file(name, data))
        self.assertIsNone(info)
        self.assertTrue(err, "erreur attendue pour %r" % data[:16])
        return err

    def test_pe_68_octets(self):
        # Reproduction du constat : MZ, pointeur a 64, PE\0\0, rien d'autre (struct.error avant la v4.11)
        data = bytearray(68)
        data[0:2] = b"MZ"
        struct.pack_into("<I", data, 0x3C, 64)
        data[64:68] = b"PE\0\0"
        self.assert_bad(check_package.pe_info, bytes(data))

    def test_pe_tronques(self):
        full = pe_bytes()
        for n in (2, 30, 63, 64, 70, 100, len(full) - 1):
            data = full[:n]
            info, err = check_package.binary_info(check_package.pe_info, self.file("pe%d" % n, data))
            self.assertTrue(info is None, n)

    def test_pe_offset_hors_limites(self):
        data = bytearray(pe_bytes())
        struct.pack_into("<I", data, 0x3C, 10 ** 9)
        info, err = check_package.binary_info(check_package.pe_info, self.file("pe_loin", bytes(data)))
        self.assertIsNone(info)
        self.assertIn("hors du fichier", err)

    def test_pe_sections_hors_limites(self):
        data = bytearray(pe_bytes())
        struct.pack_into("<H", data, 0x40 + 6, 50)  # 50 sections annoncees, aucune presente
        self.assert_bad(check_package.pe_info, bytes(data))

    def test_pe_valide(self):
        info, err = check_package.binary_info(check_package.pe_info, self.file("ok.exe", pe_bytes()))
        self.assertIsNone(err)
        self.assertEqual(info[0], "x86_64")

    def test_elf_tronques(self):
        full = elf_bytes()
        for n in (4, 5, 20, 40, 63):
            info, err = check_package.binary_info(check_package.elf_info, self.file("elf%d" % n, full[:n]))
            self.assertIsNone(info, n)
            self.assertTrue(err, n)

    def test_elf_table_programme_hors_limites(self):
        data = bytearray(elf_bytes())
        struct.pack_into("<Q", data, 32, 10 ** 12)
        struct.pack_into("<HH", data, 54, 56, 3)
        err = self.assert_bad(check_package.elf_info, bytes(data))
        self.assertIn("hors du fichier", err)

    def test_elf_dynamique_hors_limites(self):
        data = bytearray(elf_bytes()) + bytearray(56)
        struct.pack_into("<Q", data, 32, 64)
        struct.pack_into("<HH", data, 54, 56, 1)
        struct.pack_into("<IIQQQQ", data, 64, 2, 0, 10 ** 9, 0, 0, 4096)  # PT_DYNAMIC hors du fichier
        self.assert_bad(check_package.elf_info, bytes(data))

    def test_elf_valide(self):
        info, err = check_package.binary_info(check_package.elf_info, self.file("ok.elf", elf_bytes()))
        self.assertIsNone(err)
        self.assertEqual(info[0], "x86_64")

    def macho(self, ncmds=0, sizeofcmds=0, extra=b""):
        h = struct.pack("<IiiIIII", 0xFEEDFACF, 0x01000007, 3, 2, ncmds, sizeofcmds, 0) + b"\0" * 4
        return h + extra

    def test_macho_valide(self):
        info, err = check_package.binary_info(check_package.macho_info, self.file("ok", self.macho()))
        self.assertIsNone(err)
        self.assertEqual(info[0], ["x86_64"])

    def test_macho_commandes_hors_limites(self):
        self.assert_bad(check_package.macho_info, self.macho(ncmds=3, sizeofcmds=4096))

    def test_macho_taille_de_commande_nulle(self):
        self.assert_bad(check_package.macho_info, self.macho(ncmds=1, sizeofcmds=16, extra=struct.pack("<II", 0xC, 0) + b"\0" * 8))

    def test_macho_nom_hors_commande(self):
        cmd = struct.pack("<III", 0xC, 24, 200) + b"\0" * 12
        self.assert_bad(check_package.macho_info, self.macho(ncmds=1, sizeofcmds=24, extra=cmd))

    def test_macho_universel_tranche_hors_limites(self):
        fat = struct.pack(">II", 0xCAFEBABE, 1) + struct.pack(">iiIII", 0x01000007, 3, 10 ** 6, 4096, 12)
        self.assert_bad(check_package.macho_info, fat)

    def test_macho_tronque(self):
        full = self.macho()
        for n in (8, 12, 20, 31):
            info, err = check_package.binary_info(check_package.macho_info, self.file("m%d" % n, full[:n]))
            self.assertIsNone(info, n)

    def test_paquet_avec_binaire_tronque(self):
        # Paquet complet sauf le binaire du jeu (PE tronque) : ECHEC avec explication, sans exception
        f = Fixture(self.tmp)
        archive = f.make()
        data = bytearray(68)
        data[0:2] = b"MZ"
        struct.pack_into("<I", data, 0x3C, 64)
        data[64:68] = b"PE\0\0"
        write(os.path.join(archive, "Windows", "Backrooms", "Binaries", "Win64", "Backrooms-Win64-Shipping.exe"), bytes(data))
        code, out = run_check(["--platform", "Windows", "--dir", archive, "--project", f.project, "--unrealpak", f.tool])
        self.assertEqual(code, 1, out)
        self.assertIn("hors du fichier", out)
        self.assertNotIn("Traceback", out)


class MacSignOrderTests(unittest.TestCase):
    """v4.11 : ordre de signature du bundle macOS (de l'interieur vers l'exterieur, tout code imbrique)"""

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="br_macsign_")
        import mac_sign_order
        self.m = mac_sign_order

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def macho(self):
        return struct.pack("<IiiIIII", 0xFEEDFACF, 0x0100000C, 0, 2, 0, 0, 0) + b"\0" * 4

    def plist(self, exe):
        return ('<?xml version="1.0"?><plist><dict><key>CFBundleExecutable</key><string>%s</string></dict></plist>' % exe)

    def test_ordre_interieur_vers_exterieur(self):
        app = os.path.join(self.tmp, "Backrooms.app")
        write(os.path.join(app, "Contents", "Info.plist"), self.plist("Backrooms-Mac-Shipping"))
        write(os.path.join(app, "Contents", "MacOS", "Backrooms-Mac-Shipping"), self.macho())
        # Bibliotheque sans extension (oubliee par la v4.10), dylib, executable auxiliaire, bundle imbrique
        write(os.path.join(app, "Contents", "UE", "Engine", "Binaries", "ThirdParty", "libfoo"), self.macho())
        write(os.path.join(app, "Contents", "UE", "Engine", "Binaries", "Mac", "libbar.dylib"), self.macho())
        write(os.path.join(app, "Contents", "UE", "Engine", "Binaries", "Mac", "ShaderTool"), self.macho())
        crash = os.path.join(app, "Contents", "UE", "Engine", "Binaries", "Mac", "CrashReportClient.app")
        write(os.path.join(crash, "Contents", "Info.plist"), self.plist("CrashReportClient"))
        write(os.path.join(crash, "Contents", "MacOS", "CrashReportClient"), self.macho())
        write(os.path.join(crash, "Contents", "Frameworks", "libcrash.dylib"), self.macho())
        write(os.path.join(app, "Contents", "Resources", "image.png"), b"\x89PNG\r\n\x1a\n" + b"\0" * 20)
        # 0xCAFEBABE avec 50 "architectures" : une classe Java, pas un Mach-O
        write(os.path.join(app, "Contents", "Resources", "Outil.class"), struct.pack(">II", 0xCAFEBABE, 50) + b"\0" * 8)
        order = self.m.signing_order(app)
        rel = [os.path.relpath(p, self.tmp) for p in order]
        self.assertEqual(order[-1], os.path.normpath(app))
        names = [os.path.basename(p) for p in order]
        for needed in ("libfoo", "libbar.dylib", "ShaderTool", "libcrash.dylib", "CrashReportClient.app"):
            self.assertIn(needed, names, rel)
        # Executables principaux signes avec leur bundle, pas seuls
        self.assertNotIn("Backrooms-Mac-Shipping", names)
        self.assertNotIn("CrashReportClient", names)
        self.assertNotIn("image.png", names)
        self.assertNotIn("Outil.class", names)
        # Le contenu d'un bundle imbrique est signe avant lui
        self.assertLess(names.index("libcrash.dylib"), names.index("CrashReportClient.app"))
