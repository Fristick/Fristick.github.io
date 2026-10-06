"""v4.10 : controle d'un jeu empaquete (Windows, Linux ou macOS) avant livraison.

Cinq verifications separees. Chacune donne un etat : OK, ECHEC ou NON VERIFIE (avec la raison). Une extension de fichier
ne prouve rien : les formats sont lus.

  structure    : lanceur et binaire reel du jeu (format, droits d'execution), conteneurs attendus par la configuration
                 (Config/DefaultGame.ini : bUseIoStore), en-tetes des .pak et .utoc, .ucas associe a chaque .utoc,
                 fichiers qui ne doivent pas etre livres (RawAssets, scripts Python) ;
  contenu      : avec UnrealPak (moteur : --ue, ou outil : --unrealpak) : liste des .pak (polices, Game.locres de chaque
                 langue, Game.locmeta) et des conteneurs IoStore (-ListContainer=<.utoc> -csv=...) : carte L_Backrooms et ressources
                 essentielles (essential_packages.txt). Le code de sortie de l'outil est verifie ;
  architecture : machine des binaires (PE x64, ELF x86_64, Mach-O : architectures de toutes les bibliotheques du .app) ;
  dependances  : bibliotheques importees par l'executable presentes dans le paquet, ou connues du systeme cible ;
  lancement    : (--launch) le paquet est lance sur cet hote avec -BRAutoTest -BRSmokeTest ; code de sortie et rapport lus.

Modes :
  --mode release (defaut) : structure, contenu, architecture et dependances sont obligatoires ; le lancement aussi avec
      --require-launch. Une verification impossible (outil absent, format illisible, hote different) donne
      « INSPECTION INCOMPLETE » (code 2) : une livraison doit la traiter comme un blocage.
  --mode structure : developpement. Seule la structure est obligatoire et le resultat le dit : « CONTROLE STRUCTUREL
      SEULEMENT ». Le contenu n'est pas presente comme verifie.

Codes de sortie : 0 complet pour le mode choisi ; 1 probleme ; 2 inspection incomplete ; 3 erreur d'utilisation.
--json <fichier> : rapport structure (lu par make_steam_vdf.py et par les tests).

Usage : python Tools/Build/check_package.py --platform Windows|Linux|Mac --dir Build/<Plateforme>/<Config> [--ue <moteur>]
"""
import argparse
import csv
import glob
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(HERE, "..", ".."))

PLATFORMS = ("Windows", "Linux", "Mac")
CULTURES = ["fr", "en", "de", "es-ES", "pt-BR", "ru", "it", "tr", "es-419", "pl", "zh-Hans", "uk", "ar", "ko", "fa", "ja", "hu", "cs",
            "pt-PT", "sv", "zh-Hant", "id"]
FONTS = ["NotoSans%s-%s.ttf" % (s, w) for s in ("Arabic", "SC", "TC", "JP", "KR") for w in ("Regular", "Bold")]
MAP_PACKAGE = "/Game/Backrooms/Maps/L_Backrooms"

OK, FAIL, SKIP = "OK", "ECHEC", "NON VERIFIE"
CHECKS = ("structure", "contenu", "architecture", "dependances", "lancement")

PAK_MAGIC = struct.pack("<I", 0x5A6F12E1)
UTOC_MAGIC = b"-==--==--==--==-"
PE_MACHINES = {0x8664: "x86_64", 0xAA64: "arm64", 0x14C: "x86"}
ELF_MACHINES = {62: "x86_64", 183: "arm64", 3: "x86"}
MACHO_CPUS = {0x01000007: "x86_64", 0x0100000C: "arm64", 7: "x86", 12: "arm"}

# Bibliotheques fournies par le systeme cible (non livrees avec le jeu)
WINDOWS_SYSTEM_DLLS = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll", "ole32.dll", "oleaut32.dll", "ws2_32.dll",
    "winmm.dll", "version.dll", "dbghelp.dll", "shlwapi.dll", "setupapi.dll", "imm32.dll", "rpcrt4.dll", "crypt32.dll",
    "bcrypt.dll", "ncrypt.dll", "secur32.dll", "iphlpapi.dll", "psapi.dll", "dwmapi.dll", "uxtheme.dll", "winhttp.dll",
    "wininet.dll", "comdlg32.dll", "comctl32.dll", "hid.dll", "xinput1_3.dll", "xinput1_4.dll", "dxgi.dll", "d3d11.dll",
    "d3d12.dll", "d3dcompiler_47.dll", "dinput8.dll", "dsound.dll", "opengl32.dll", "mpr.dll", "netapi32.dll", "userenv.dll",
    "wintrust.dll", "wldap32.dll", "normaliz.dll", "dnsapi.dll", "powrprof.dll", "propsys.dll", "mfplat.dll",
    "mfreadwrite.dll", "mf.dll", "mfuuid.dll", "avrt.dll", "ksuser.dll", "mmdevapi.dll", "xaudio2_9.dll", "x3daudio1_7.dll",
    "ntdll.dll", "msvcrt.dll", "cfgmgr32.dll", "wtsapi32.dll", "dwrite.dll", "d2d1.dll", "windowscodecs.dll", "shcore.dll",
    "nvapi64.dll", "amd_ags_x64.dll", "dxcore.dll", "dbgeng.dll", "msimg32.dll", "usp10.dll", "winspool.drv", "credui.dll",
    "wevtapi.dll", "pdh.dll", "tdh.dll", "esent.dll", "wer.dll", "d3d9.dll", "xinput9_1_0.dll", "msacm32.dll",
    "vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll", "concrt140.dll",
    # v4.10 : relevees sur le paquet Shipping reel (accessibilite de Slate, TPM)
    "uiautomationcore.dll", "tbs.dll",
}


def windows_system_dll(name):
    """Bibliotheque fournie par Windows : liste connue, ou presente dans System32 quand l'hote est un Windows"""
    low = name.lower()
    if low.startswith(("api-ms-", "ext-ms-")) or low in WINDOWS_SYSTEM_DLLS:
        return True
    if os.name == "nt":
        system32 = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32")
        return os.path.isfile(os.path.join(system32, name))
    return False


LINUX_SYSTEM_LIBS = re.compile(r"^(libc|libm|libpthread|libdl|librt|ld-linux-x86-64|libgcc_s|libresolv|libutil)\.so(\.\d+)*$")


class Check(object):
    def __init__(self, name):
        self.name = name
        self.state = None
        self.details = []
        self.problems = []
        self.reason = ""

    def fail(self, msg):
        self.problems.append(msg)

    def info(self, msg):
        self.details.append(msg)

    def skip(self, reason):
        if self.state is None:
            self.reason = reason
            self.state = SKIP

    def close(self):
        if self.problems:
            self.state = FAIL
        elif self.state is None:
            self.state = OK
        return self

    def as_dict(self):
        return {"etat": self.state, "raison": self.reason, "details": self.details, "problemes": self.problems}


# ---------------------------------------------------------------------------------------------------------------------
# Lecture des formats
# ---------------------------------------------------------------------------------------------------------------------
def read_head(path, n):
    try:
        with open(path, "rb") as fh:
            return fh.read(n)
    except OSError:
        return b""


def read_tail(path, n):
    try:
        with open(path, "rb") as fh:
            fh.seek(0, os.SEEK_END)
            size = fh.tell()
            fh.seek(max(0, size - n))
            return fh.read()
    except OSError:
        return b""


def pe_info(path):
    """(architecture, imports, imports differes) d'un executable Windows, ou None si ce n'est pas un PE"""
    try:
        with open(path, "rb") as fh:
            data = fh.read()
    except OSError:
        return None
    if data[:2] != b"MZ" or len(data) < 0x40:
        return None
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        return None
    machine, nsec = struct.unpack_from("<HH", data, pe + 4)
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    dirs = opt + (112 if magic == 0x20B else 96)
    sections = []
    sec = opt + opt_size
    for i in range(nsec):
        # En-tete de section : nom (8), VirtualSize, VirtualAddress, SizeOfRawData, PointerToRawData
        vsize, va, raw_size, raw_ptr = struct.unpack_from("<IIII", data, sec + i * 40 + 8)
        sections.append((va, max(vsize, raw_size), raw_ptr))

    def rva(r):
        for va, size, ptr in sections:
            if va <= r < va + size:
                return r - va + ptr
        return None

    def cstr(off):
        if off is None:
            return ""
        end = data.find(b"\0", off)
        return data[off:end].decode("ascii", "replace")

    def names(dir_index, entry_size, name_field):
        out = []
        ndirs = struct.unpack_from("<I", data, dirs - 4)[0]
        if dir_index >= ndirs:
            return out
        r, size = struct.unpack_from("<II", data, dirs + dir_index * 8)
        off = rva(r) if r else None
        while off is not None and off + entry_size <= len(data):
            fields = data[off:off + entry_size]
            if not any(fields):
                break
            name_rva = struct.unpack_from("<I", data, off + name_field)[0]
            if name_rva == 0:
                break
            out.append(cstr(rva(name_rva)))
            off += entry_size
        return out

    imports = names(1, 20, 12)
    delayed = names(13, 32, 4)
    return PE_MACHINES.get(machine, "machine 0x%x" % machine), imports, delayed


def elf_info(path):
    """(architecture, DT_NEEDED) d'un binaire ELF 64 bits, ou None"""
    try:
        with open(path, "rb") as fh:
            data = fh.read()
    except OSError:
        return None
    if data[:4] != b"\x7fELF" or data[4] != 2:
        return None
    machine = struct.unpack_from("<H", data, 18)[0]
    phoff = struct.unpack_from("<Q", data, 32)[0]
    phentsize, phnum = struct.unpack_from("<HH", data, 54)
    loads = []
    dynamic = None
    for i in range(phnum):
        p_type, _, p_offset, p_vaddr, _, p_filesz = struct.unpack_from("<IIQQQQ", data, phoff + i * phentsize)
        if p_type == 1:
            loads.append((p_vaddr, p_filesz, p_offset))
        elif p_type == 2:
            dynamic = (p_offset, p_filesz)
    needed = []
    if dynamic:
        strtab = None
        entries = []
        for off in range(dynamic[0], dynamic[0] + dynamic[1], 16):
            tag, val = struct.unpack_from("<qQ", data, off)
            if tag == 0:
                break
            if tag == 5:
                strtab = val
            elif tag == 1:
                entries.append(val)
        base = None
        for vaddr, size, offset in loads:
            if strtab is not None and vaddr <= strtab < vaddr + size:
                base = strtab - vaddr + offset
        if base is not None:
            for e in entries:
                end = data.find(b"\0", base + e)
                needed.append(data[base + e:end].decode("ascii", "replace"))
    return ELF_MACHINES.get(machine, "machine %d" % machine), needed


def macho_info(path):
    """(architectures, bibliotheques chargees) d'un binaire Mach-O (fin ou universel), ou None"""
    try:
        with open(path, "rb") as fh:
            data = fh.read()
    except OSError:
        return None
    if len(data) < 8:
        return None
    magic = struct.unpack_from(">I", data, 0)[0]
    slices = []
    if magic in (0xCAFEBABE, 0xCAFEBABF):
        n = struct.unpack_from(">I", data, 4)[0]
        wide = magic == 0xCAFEBABF
        for i in range(n):
            if wide:
                cpu, _, off, size = struct.unpack_from(">iiQQ", data, 8 + i * 32)[0:4]
            else:
                cpu, _, off, size = struct.unpack_from(">iiII", data, 8 + i * 20)[0:4]
            slices.append((cpu, off))
    else:
        le = struct.unpack_from("<I", data, 0)[0]
        if le not in (0xFEEDFACF, 0xFEEDFACE):
            return None
        slices.append((struct.unpack_from("<i", data, 4)[0], 0))
    archs = []
    libs = set()
    for cpu, off in slices:
        archs.append(MACHO_CPUS.get(cpu & 0xFFFFFFFF, "cpu 0x%x" % (cpu & 0xFFFFFFFF)))
        mh = struct.unpack_from("<I", data, off)[0]
        if mh not in (0xFEEDFACF, 0xFEEDFACE):
            continue
        ncmds = struct.unpack_from("<I", data, off + 16)[0]
        cmd_off = off + (32 if mh == 0xFEEDFACF else 28)
        for _ in range(ncmds):
            cmd, size = struct.unpack_from("<II", data, cmd_off)
            if cmd in (0xC, 0x18 | 0x80000000, 0x1F | 0x80000000):  # LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, LC_REEXPORT_DYLIB
                name_off = struct.unpack_from("<I", data, cmd_off + 8)[0]
                end = data.find(b"\0", cmd_off + name_off)
                libs.add(data[cmd_off + name_off:end].decode("utf-8", "replace"))
            cmd_off += size
    return archs, sorted(libs)


def project_uses_iostore(project):
    ini = os.path.join(project, "Config", "DefaultGame.ini")
    try:
        with open(ini, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError:
        return True
    m = re.search(r"^\s*bUseIoStore\s*=\s*(\w+)", text, re.M)
    return not m or m.group(1).lower() in ("true", "1")


def essential_packages():
    path = os.path.join(HERE, "essential_packages.txt")
    out = []
    try:
        with open(path, encoding="utf-8") as fh:
            for line in fh:
                line = line.split("#", 1)[0].strip()
                if line:
                    out.append(line)
    except OSError:
        pass
    return out


# ---------------------------------------------------------------------------------------------------------------------
# Paquet
# ---------------------------------------------------------------------------------------------------------------------
def find_game(platform, base):
    """(dossier du jeu, lanceur, binaire reel) ; None si introuvable"""
    if platform == "Mac":
        apps = []
        for app in glob.glob(os.path.join(base, "**", "*.app"), recursive=True):
            # Bundle principal : celui du jeu, pas un .app imbrique (CrashReportClient.app, ...)
            if ".app" + os.sep in os.path.relpath(app, base).replace("/", os.sep)[:-4]:
                continue
            plist = os.path.join(app, "Contents", "Info.plist")
            exe = plist_value(plist, "CFBundleExecutable")
            if exe and exe.lower().startswith("backrooms"):
                apps.append((app, os.path.join(app, "Contents", "MacOS", exe)))
        if len(apps) != 1:
            return None
        return apps[0][0], apps[0][0], apps[0][1]
    launcher_name = "Backrooms.exe" if platform == "Windows" else "Backrooms.sh"
    sub = "Win64" if platform == "Windows" else "Linux"
    for cand in (os.path.join(base, platform), os.path.join(base, platform + "NoEditor"), os.path.join(base, "Windows" if platform == "Windows" else "Linux"), base):
        launcher = os.path.join(cand, launcher_name)
        if os.path.isfile(launcher):
            bins = sorted(glob.glob(os.path.join(cand, "Backrooms", "Binaries", sub, "Backrooms*")))
            bins = [b for b in bins if os.path.isfile(b) and not re.search(r"\.(pdb|debug|sym|txt|target|modules|version)$", b, re.I)]
            return cand, launcher, (bins[0] if bins else None)
    return None


def plist_value(path, key):
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError:
        return None
    m = re.search(r"<key>%s</key>\s*<string>([^<]*)</string>" % re.escape(key), text)
    return m.group(1) if m else None


def run_tool(cmd, timeout=600):
    """(code de sortie, sortie) ; code None si l'outil ne peut pas etre lance"""
    if cmd[0].endswith(".py"):
        cmd = [sys.executable] + cmd
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=timeout)
        return p.returncode, p.stdout + p.stderr
    except (OSError, subprocess.TimeoutExpired) as e:
        return None, str(e)


def find_unrealpak(args):
    if args.unrealpak:
        return args.unrealpak if os.path.isfile(args.unrealpak) else None
    if not args.ue:
        return None
    sub = {"win32": "Win64", "linux": "Linux", "darwin": "Mac"}.get(sys.platform, "Linux")
    exe = os.path.join(args.ue, "Engine", "Binaries", sub, "UnrealPak" + (".exe" if sub == "Win64" else ""))
    return exe if os.path.isfile(exe) else None


def check_structure(c, platform, base, project):
    if not os.path.isdir(base):
        c.fail("dossier introuvable : %s" % base)
        return None
    found = find_game(platform, base)
    if not found:
        c.fail("jeu introuvable dans %s (%s)" % (base, "un seul Backrooms.app attendu" if platform == "Mac" else "lanceur absent"))
        return None
    root, launcher, binary = found
    c.info("jeu : %s" % root)
    if not binary or not os.path.isfile(binary):
        c.fail("binaire du jeu introuvable (Backrooms/Binaries/%s ou Contents/MacOS)" % ("Win64" if platform == "Windows" else platform))
    else:
        c.info("binaire : %s (%d Mo)" % (os.path.relpath(binary, root), os.path.getsize(binary) // (1024 * 1024)))
        if platform == "Windows":
            if not pe_info(binary) or not pe_info(launcher):
                c.fail("lanceur ou binaire qui n'est pas un executable Windows")
        elif platform == "Linux":
            if not elf_info(binary):
                c.fail("binaire qui n'est pas un ELF 64 bits")
            if not read_head(launcher, 2) == b"#!":
                c.fail("Backrooms.sh n'est pas un script (#! absent)")
            if os.name == "nt":
                c.info("droits d'execution : non lisibles depuis Windows (verifies sur l'hote Linux)")
            elif not (os.access(binary, os.X_OK) and os.access(launcher, os.X_OK)):
                c.fail("droit d'execution absent sur le binaire ou Backrooms.sh")
        else:
            if not macho_info(binary):
                c.fail("binaire qui n'est pas un Mach-O")
            if os.name != "nt" and not os.access(binary, os.X_OK):
                c.fail("droit d'execution absent sur %s" % binary)
    # Conteneurs attendus par la configuration
    paks = glob.glob(os.path.join(root, "**", "*.pak"), recursive=True)
    utocs = glob.glob(os.path.join(root, "**", "*.utoc"), recursive=True)
    c.info("conteneurs : %d .pak, %d .utoc" % (len(paks), len(utocs)))
    if not paks:
        c.fail("aucun .pak")
    for p in paks:
        if os.path.getsize(p) < 64 or PAK_MAGIC not in read_tail(p, 1024):
            c.fail("en-tete de paquet invalide : %s" % os.path.relpath(p, root))
    iostore = project_uses_iostore(project)
    if iostore:
        names = {os.path.basename(u).lower() for u in utocs}
        if "global.utoc" not in names:
            c.fail("global.utoc absent alors que la configuration utilise IoStore (bUseIoStore)")
        if not any(n != "global.utoc" for n in names):
            c.fail("aucun conteneur de contenu .utoc alors que la configuration utilise IoStore")
    for u in utocs:
        if read_head(u, 16) != UTOC_MAGIC:
            c.fail("table de conteneur invalide : %s" % os.path.relpath(u, root))
        ucas = u[:-5] + ".ucas"
        if not os.path.isfile(ucas) or os.path.getsize(ucas) == 0:
            c.fail("donnees absentes ou vides pour %s (.ucas)" % os.path.relpath(u, root))
    # Fichiers livres en clair qui ne doivent pas l'etre
    for f in glob.glob(os.path.join(root, "**", "*"), recursive=True):
        rel = os.path.relpath(f, root).replace("\\", "/")
        if "/RawAssets/" in "/" + rel or rel.endswith(".py") or "/Content/Python/" in "/" + rel:
            c.fail("livre a tort : %s" % rel)
    return root, launcher, binary, paks, utocs


def list_pak(tool, pak):
    # Chemins absolus : UnrealPak change de dossier de travail (un chemin relatif ne designe plus rien)
    code, out = run_tool([tool, os.path.abspath(pak), "-List"])
    if code is None:
        return None, "outil impossible a lancer : %s" % out.strip()[:200]
    if code != 0:
        return None, "UnrealPak -List a echoue (code %d) sur %s" % (code, os.path.basename(pak))
    entries = set(m.group(1).replace("\\", "/") for m in re.finditer(r'"([^"]+)"', out))
    if not entries:
        return None, "UnrealPak n'a liste aucun fichier dans %s" % os.path.basename(pak)
    return entries, None


def list_container(tool, utoc):
    tmp = tempfile.mkdtemp(prefix="br_utoc_")
    try:
        csv_path = os.path.join(tmp, "list.csv")
        # Unreal 5.8 : -ListContainer=<.utoc> (la forme -List= n'existe que derriere la sous-commande IoStore)
        code, out = run_tool([tool, "-ListContainer=%s" % os.path.abspath(utoc), "-csv=%s" % os.path.abspath(csv_path)])
        if code is None:
            return None, "outil impossible a lancer : %s" % out.strip()[:200]
        if code != 0:
            return None, "liste IoStore en echec (code %d) sur %s" % (code, os.path.basename(utoc))
        if not os.path.isfile(csv_path):
            return None, "liste IoStore sans fichier CSV pour %s" % os.path.basename(utoc)
        names = set()
        with open(csv_path, encoding="utf-8", errors="replace") as fh:
            for row in csv.reader(fh):
                for cell in row:
                    cell = cell.strip()
                    if cell.startswith("/") or cell.endswith((".uasset", ".umap", ".ubulk", ".uexp")):
                        names.add(cell.replace("\\", "/"))
        return names, None
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def check_content(c, tool, paks, utocs):
    if not tool:
        c.skip("UnrealPak introuvable (--ue <moteur> ou --unrealpak <outil>) : contenu des conteneurs non lu")
        return
    entries = set()
    for p in paks:
        listed, err = list_pak(tool, p)
        if err:
            c.fail(err)
            continue
        entries |= listed
    packages = set()
    for u in utocs:
        if os.path.basename(u).lower() == "global.utoc":
            continue
        listed, err = list_container(tool, u)
        if err:
            c.fail(err)
            continue
        packages |= listed
    if c.problems:
        return
    joined = "\n".join(sorted(entries))
    c.info("fichiers des .pak : %d ; entrees IoStore : %d" % (len(entries), len(packages)))
    for f in FONTS:
        if not re.search(r"Content/Fonts/" + re.escape(f) + r"$", joined, re.M):
            c.fail("police absente : Content/Fonts/%s" % f)
    for cult in CULTURES:
        if not re.search(r"Localization/Game/" + re.escape(cult) + r"/Game\.locres$", joined, re.M):
            c.fail("langue absente : %s/Game.locres" % cult)
    if not re.search(r"Localization/Game/Game\.locmeta$", joined, re.M):
        c.fail("Game.locmeta absent")
    every = "\n".join(sorted(entries | packages))

    def present(package):
        # Nom de paquet (/Game/...) ou fichier cuisine (Backrooms/Content/...)
        tail = package.replace("/Game/", "Content/", 1)
        return (package in packages) or re.search(re.escape(tail) + r"(\.u(asset|map))?$", every, re.M) is not None

    if not present(MAP_PACKAGE):
        c.fail("carte absente : %s" % MAP_PACKAGE)
    for pkg in essential_packages():
        if not present(pkg):
            c.fail("ressource essentielle absente : %s" % pkg)
    if "RawAssets/" in every:
        c.fail("RawAssets present dans un conteneur")


def check_architecture(c, platform, root, binary, expected_archs):
    if not binary:
        c.skip("binaire introuvable")
        return
    if platform == "Windows":
        info = pe_info(binary)
        if not info:
            c.fail("binaire illisible")
            return
        c.info("binaire : %s" % info[0])
        if info[0] != "x86_64":
            c.fail("architecture %s, x86_64 attendu" % info[0])
        for dll in glob.glob(os.path.join(root, "**", "*.dll"), recursive=True):
            d = pe_info(dll)
            if d and d[0] != "x86_64":
                c.fail("bibliotheque %s en %s" % (os.path.relpath(dll, root), d[0]))
    elif platform == "Linux":
        info = elf_info(binary)
        if not info:
            c.fail("binaire illisible")
            return
        c.info("binaire : %s" % info[0])
        if info[0] != "x86_64":
            c.fail("architecture %s, x86_64 attendu" % info[0])
        for so in glob.glob(os.path.join(root, "**", "*.so*"), recursive=True):
            d = elf_info(so)
            if d and d[0] != "x86_64":
                c.fail("bibliotheque %s en %s" % (os.path.relpath(so, root), d[0]))
    else:
        want = set(expected_archs)
        for f in [binary] + glob.glob(os.path.join(root, "Contents", "**", "*"), recursive=True):
            if not os.path.isfile(f) or os.path.islink(f):
                continue
            d = macho_info(f)
            if not d:
                continue
            if not want.issubset(set(d[0])):
                c.fail("%s : %s (attendu : %s)" % (os.path.relpath(f, root), "+".join(d[0]), "+".join(sorted(want))))
        info = macho_info(binary)
        if info:
            c.info("binaire : %s" % "+".join(info[0]))


def check_dependencies(c, platform, root, binary):
    if not binary:
        c.skip("binaire introuvable")
        return
    shipped = {os.path.basename(f).lower() for f in glob.glob(os.path.join(root, "**", "*"), recursive=True) if os.path.isfile(f)}
    if platform == "Windows":
        info = pe_info(binary)
        if not info:
            c.fail("binaire illisible")
            return
        _, imports, delayed = info
        c.info("importees : %d, chargees a la demande : %d" % (len(imports), len(delayed)))
        for dll in imports:
            if windows_system_dll(dll) or dll.lower() in shipped:
                continue
            c.fail("bibliotheque requise absente du paquet : %s" % dll)
        for dll in delayed:
            if not (windows_system_dll(dll) or dll.lower() in shipped):
                c.info("chargee a la demande, absente du paquet (verifier son usage) : %s" % dll)
    elif platform == "Linux":
        info = elf_info(binary)
        if not info:
            c.fail("binaire illisible")
            return
        for lib in info[1]:
            if LINUX_SYSTEM_LIBS.match(lib) or lib.lower() in shipped:
                continue
            c.fail("bibliotheque requise absente du paquet : %s" % lib)
        c.info("requises : %s" % ", ".join(info[1]))
    else:
        info = macho_info(binary)
        if not info:
            c.fail("binaire illisible")
            return
        for lib in info[1]:
            if lib.startswith(("/usr/lib/", "/System/")):
                continue
            if os.path.basename(lib).lower() not in shipped:
                c.fail("bibliotheque requise absente du bundle : %s" % lib)


def check_launch(c, platform, root, launcher, timeout):
    host = {"win32": "Windows", "linux": "Linux", "darwin": "Mac"}.get(sys.platform)
    if host != platform:
        c.skip("lancement possible seulement sur un hote %s (hote actuel : %s)" % (platform, host))
        return
    out_dir = tempfile.mkdtemp(prefix="br_smoke_")
    result = os.path.join(out_dir, "smoke.json")
    log = os.path.join(out_dir, "smoke.log")
    if platform == "Mac":
        cmd = ["open", "-W", "-n", root, "--args"]
    else:
        cmd = [launcher]
    cmd += ["-BRAutoTest", "-BRSmokeTest", "-BRSmokeOut=%s" % result, "-unattended", "-windowed", "-ResX=960", "-ResY=540",
            "-abslog=%s" % log]
    try:
        p = subprocess.run(cmd, timeout=timeout)
        code = p.returncode
    except subprocess.TimeoutExpired:
        c.fail("le jeu n'a pas fini le test de lancement en %d s" % timeout)
        return
    except OSError as e:
        c.fail("lancement impossible : %s" % e)
        return
    if not os.path.isfile(result):
        c.fail("le jeu s'est arrete sans rapport de lancement (code %s ; journal : %s)" % (code, log))
        return
    try:
        with open(result, encoding="utf-8-sig") as fh:
            rep = json.load(fh)
    except ValueError:
        c.fail("rapport de lancement illisible : %s" % result)
        return
    c.info("lancement : code %s, %s" % (code, rep.get("resume", "")))
    for p in rep.get("problemes", []):
        c.fail(p)
    if code not in (0, None) and not c.problems:
        c.fail("code de sortie %s" % code)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--platform", required=True)
    ap.add_argument("--dir", required=True)
    ap.add_argument("--ue", help="dossier du moteur (UnrealPak)")
    ap.add_argument("--unrealpak", help="chemin de l'outil de liste (remplace --ue)")
    ap.add_argument("--project", default=PROJECT)
    ap.add_argument("--mode", choices=["release", "structure"], default="release")
    ap.add_argument("--launch", action="store_true", help="lancer le paquet (meme systeme que l'hote)")
    ap.add_argument("--require-launch", action="store_true")
    ap.add_argument("--launch-timeout", type=int, default=600)
    ap.add_argument("--archs", default="arm64,x86_64", help="Mac : architectures attendues")
    ap.add_argument("--json")
    a = ap.parse_args(argv)
    if a.platform not in PLATFORMS:
        print("ERREUR : plateforme inconnue %r (attendu : %s)" % (a.platform, ", ".join(PLATFORMS)))
        return 3

    checks = {n: Check(n) for n in CHECKS}
    st = check_structure(checks["structure"], a.platform, a.dir, a.project)
    if st:
        root, launcher, binary, paks, utocs = st
        tool = find_unrealpak(a)
        if a.mode == "structure":
            checks["contenu"].skip("mode structure : contenu non lu")
        else:
            check_content(checks["contenu"], tool, paks, utocs)
        check_architecture(checks["architecture"], a.platform, root, binary, [x for x in a.archs.split(",") if x])
        check_dependencies(checks["dependances"], a.platform, root, binary)
        if a.launch or a.require_launch:
            if checks["structure"].close().state == OK:
                check_launch(checks["lancement"], a.platform, root, launcher, a.launch_timeout)
            else:
                checks["lancement"].skip("structure en echec : paquet non lance")
        else:
            checks["lancement"].skip("non demande (--launch)")
    else:
        for n in CHECKS[1:]:
            checks[n].skip("structure introuvable")
    for c in checks.values():
        c.close()

    required = ["structure"] if a.mode == "structure" else ["structure", "contenu", "architecture", "dependances"]
    if a.require_launch:
        required.append("lancement")
    failed = [n for n in required if checks[n].state == FAIL]
    missing = [n for n in required if checks[n].state == SKIP]
    if failed:
        status, code = "ECHEC", 1
    elif missing:
        status, code = "INSPECTION INCOMPLETE", 2
    elif a.mode == "structure":
        status, code = "CONTROLE STRUCTUREL SEULEMENT (contenu non verifie)", 0
    else:
        status, code = "PAQUET VERIFIE" + (" ET LANCE" if checks["lancement"].state == OK else ""), 0

    print("Paquet %s : %s" % (a.platform, a.dir))
    for n in CHECKS:
        c = checks[n]
        flag = "" if n in required else " (facultatif)"
        print("  %-12s %s%s%s" % (n, c.state, flag, (" : " + c.reason) if c.reason else ""))
        for d in c.details:
            print("      " + d)
        for p in c.problems:
            print("      PROBLEME : " + p)
    print("RESULTAT : %s" % status)
    if a.json:
        os.makedirs(os.path.dirname(os.path.abspath(a.json)), exist_ok=True)
        with open(a.json, "w", encoding="utf-8") as fh:
            json.dump({"plateforme": a.platform, "dossier": os.path.abspath(a.dir), "mode": a.mode, "resultat": status, "code": code,
                       "obligatoires": required, "verifications": {n: checks[n].as_dict() for n in CHECKS}}, fh, ensure_ascii=False, indent=2)
    return code


if __name__ == "__main__":
    sys.exit(main())
