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
  lancement    : (--launch) le paquet est lance sur cet hote avec -BRAutoTest -BRSmokeTest -BRAutoTestStrict. v4.11 : le
                 rapport doit exister, venir de cet essai (son chemin figure dans la ligne de commande du jeu) et suivre
                 le schema (etat, problemes, non_verifies) ; code de sortie et etat doivent concorder. Etat INCOMPLET si
                 le jeu a demarre mais n'a pas tout verifie.

Modes :
  --mode release (defaut) : structure, contenu, architecture et dependances sont obligatoires ; le lancement aussi avec
      --require-launch. v4.11 : avec --launch, un lancement qui a eu lieu compte toujours (echec -> ECHEC, incomplet ->
      INSPECTION INCOMPLETE) ; seul un lancement impossible sur cet hote reste facultatif. Une verification impossible (outil absent, format illisible, hote different) donne
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
import time

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(HERE, "..", ".."))

PLATFORMS = ("Windows", "Linux", "Mac")
CULTURES = ["fr", "en", "de", "es-ES", "pt-BR", "ru", "it", "tr", "es-419", "pl", "zh-Hans", "uk", "ar", "ko", "fa", "ja", "hu", "cs",
            "pt-PT", "sv", "zh-Hant", "id"]
FONTS = ["NotoSans%s-%s.ttf" % (s, w) for s in ("Arabic", "SC", "TC", "JP", "KR") for w in ("Regular", "Bold")]
MAP_PACKAGE = "/Game/Backrooms/Maps/L_Backrooms"

OK, FAIL, SKIP, PARTIAL = "OK", "ECHEC", "NON VERIFIE", "INCOMPLET"
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

    def partial(self, reason):
        """v4.11 : verification faite mais qui ne prouve pas tout (lancement avec des verifications non faites)"""
        if self.state is None:
            self.reason = reason
            self.state = PARTIAL

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


class BadFormat(Exception):
    """Fichier qui porte la signature d'un format mais dont la structure est invalide (tronque, offset hors limites)"""


class Blob(object):
    """Lectures bornees : tout offset, toute longueur venant du fichier est verifie avant lecture (v4.11)"""

    def __init__(self, data, what):
        self.data = data
        self.what = what

    def need(self, off, size):
        if off < 0 or size < 0 or off + size > len(self.data):
            raise BadFormat("%s : lecture hors du fichier (offset %d, %d octets, taille %d)" % (self.what, off, size, len(self.data)))

    def unpack(self, fmt, off):
        self.need(off, struct.calcsize(fmt))
        return struct.unpack_from(fmt, self.data, off)

    def cstr(self, off, limit=4096):
        self.need(off, 1)
        end = self.data.find(b"\0", off, min(len(self.data), off + limit))
        if end < 0:
            raise BadFormat("%s : chaine sans fin a l'offset %d" % (self.what, off))
        return self.data[off:end].decode("utf-8", "replace")


def read_all(path):
    try:
        with open(path, "rb") as fh:
            return fh.read()
    except OSError:
        return None


def pe_info(path):
    """(architecture, imports, imports differes) d'un executable Windows ; None si ce n'est pas un PE ;
    BadFormat si la signature est la mais la structure est invalide"""
    data = read_all(path)
    if data is None or data[:2] != b"MZ":
        return None
    b = Blob(data, "PE " + os.path.basename(path))
    pe = b.unpack("<I", 0x3C)[0]
    b.need(pe, 24)
    if data[pe:pe + 4] != b"PE\0\0":
        return None
    machine, nsec = b.unpack("<HH", pe + 4)
    opt_size = b.unpack("<H", pe + 20)[0]
    opt = pe + 24
    b.need(opt, opt_size)  # en-tete optionnel complet
    b.need(opt + opt_size, nsec * 40)  # table des sections complete
    if nsec > 96:
        raise BadFormat("%s : %d sections (96 au plus)" % (b.what, nsec))
    sections = []
    sec = opt + opt_size
    for i in range(nsec):
        # En-tete de section : nom (8), VirtualSize, VirtualAddress, SizeOfRawData, PointerToRawData
        vsize, va, raw_size, raw_ptr = b.unpack("<IIII", sec + i * 40 + 8)
        sections.append((va, max(vsize, raw_size), raw_ptr))
    arch = PE_MACHINES.get(machine, "machine 0x%x" % machine)
    if opt_size == 0:
        return arch, [], []
    magic = b.unpack("<H", opt)[0]
    dirs = opt + (112 if magic == 0x20B else 96)
    ndirs = b.unpack("<I", dirs - 4)[0]

    def rva(r):
        for va, size, ptr in sections:
            if va <= r < va + size:
                return r - va + ptr
        return None

    def names(dir_index, entry_size, name_field):
        out = []
        if dir_index >= min(ndirs, 16):
            return out
        r, _ = b.unpack("<II", dirs + dir_index * 8)
        off = rva(r) if r else None
        for _ in range(4096):  # au plus 4096 bibliotheques : une table sans fin est une erreur
            if off is None:
                return out
            b.need(off, entry_size)
            fields = data[off:off + entry_size]
            if not any(fields):
                return out
            name_rva = b.unpack("<I", off + name_field)[0]
            if name_rva == 0:
                return out
            name_off = rva(name_rva)
            if name_off is None:
                raise BadFormat("%s : nom de bibliotheque hors des sections" % b.what)
            out.append(b.cstr(name_off))
            off += entry_size
        raise BadFormat("%s : table des importations sans fin" % b.what)

    return arch, names(1, 20, 12), names(13, 32, 4)


def elf_info(path):
    """(architecture, DT_NEEDED) d'un binaire ELF 64 bits ; None si ce n'est pas un ELF ; BadFormat si invalide"""
    data = read_all(path)
    if data is None or data[:4] != b"\x7fELF":
        return None
    b = Blob(data, "ELF " + os.path.basename(path))
    b.need(0, 64)
    if data[4] != 2:
        return None
    machine = b.unpack("<H", 18)[0]
    phoff = b.unpack("<Q", 32)[0]
    phentsize, phnum = b.unpack("<HH", 54)
    if phnum and phentsize < 56:
        raise BadFormat("%s : entree de programme trop courte (%d)" % (b.what, phentsize))
    loads = []
    dynamic = None
    for i in range(phnum):
        p_type, _, p_offset, p_vaddr, _, p_filesz = b.unpack("<IIQQQQ", phoff + i * phentsize)
        if p_type == 1:
            loads.append((p_vaddr, p_filesz, p_offset))
        elif p_type == 2:
            b.need(p_offset, p_filesz)
            dynamic = (p_offset, p_filesz)
    needed = []
    if dynamic:
        strtab = None
        entries = []
        for off in range(dynamic[0], dynamic[0] + dynamic[1] - 15, 16):
            tag, val = b.unpack("<qQ", off)
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
        if entries and base is None:
            raise BadFormat("%s : table des chaines introuvable" % b.what)
        for e in entries:
            needed.append(b.cstr(base + e))
    return ELF_MACHINES.get(machine, "machine %d" % machine), needed


def macho_info(path):
    """(architectures, bibliotheques chargees) d'un binaire Mach-O (fin ou universel) ; None si ce n'en est pas un ;
    BadFormat si invalide"""
    data = read_all(path)
    if data is None or len(data) < 8:
        return None
    b = Blob(data, "Mach-O " + os.path.basename(path))
    magic = b.unpack(">I", 0)[0]
    slices = []
    if magic in (0xCAFEBABE, 0xCAFEBABF):
        n = b.unpack(">I", 4)[0]
        if n == 0 or n > 32:
            # 0xCAFEBABE est aussi la signature des classes Java : un nombre aberrant d'architectures n'est pas un Mach-O
            raise BadFormat("%s : %d architectures annoncees" % (b.what, n))
        wide = magic == 0xCAFEBABF
        for i in range(n):
            if wide:
                cpu, _, off, size = b.unpack(">iiQQ", 8 + i * 32)
            else:
                cpu, _, off, size = b.unpack(">iiII", 8 + i * 20)
            b.need(off, max(size, 28))
            slices.append((cpu, off, size))
    else:
        le = b.unpack("<I", 0)[0]
        if le not in (0xFEEDFACF, 0xFEEDFACE):
            return None
        slices.append((b.unpack("<i", 4)[0], 0, len(data)))
    archs = []
    libs = set()
    for cpu, off, size in slices:
        archs.append(MACHO_CPUS.get(cpu & 0xFFFFFFFF, "cpu 0x%x" % (cpu & 0xFFFFFFFF)))
        mh = b.unpack("<I", off)[0]
        if mh not in (0xFEEDFACF, 0xFEEDFACE):
            raise BadFormat("%s : tranche %s sans en-tete Mach-O" % (b.what, archs[-1]))
        ncmds, sizeofcmds = b.unpack("<II", off + 16)
        cmd_off = off + (32 if mh == 0xFEEDFACF else 28)
        end = cmd_off + sizeofcmds
        b.need(cmd_off, sizeofcmds)
        if ncmds > 4096:
            raise BadFormat("%s : %d commandes de chargement" % (b.what, ncmds))
        for _ in range(ncmds):
            cmd, csize = b.unpack("<II", cmd_off)
            if csize < 8 or cmd_off + csize > end:
                raise BadFormat("%s : commande de chargement de taille %d hors limites" % (b.what, csize))
            if cmd in (0xC, 0x18 | 0x80000000, 0x1F | 0x80000000):  # LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, LC_REEXPORT_DYLIB
                name_off = b.unpack("<I", cmd_off + 8)[0]
                if name_off >= csize:
                    raise BadFormat("%s : nom de bibliotheque hors de sa commande" % b.what)
                libs.add(b.cstr(cmd_off + name_off, csize - name_off))
            cmd_off += csize
    return archs, sorted(libs)


def binary_info(reader, path):
    """(informations, erreur) : informations None et erreur None si le fichier n'est pas de ce format"""
    try:
        return reader(path), None
    except BadFormat as e:
        return None, str(e)
    except (struct.error, ValueError, IndexError, OverflowError) as e:  # filet : jamais d'exception non geree
        return None, "%s : structure illisible (%s)" % (os.path.basename(path), e)


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
            for f in (launcher, binary):
                info, err = binary_info(pe_info, f)
                if err:
                    c.fail(err)
                elif not info:
                    c.fail("%s n'est pas un executable Windows" % os.path.basename(f))
        elif platform == "Linux":
            info, err = binary_info(elf_info, binary)
            if err:
                c.fail(err)
            elif not info:
                c.fail("binaire qui n'est pas un ELF 64 bits")
            if not read_head(launcher, 2) == b"#!":
                c.fail("Backrooms.sh n'est pas un script (#! absent)")
            if os.name == "nt":
                c.info("droits d'execution : non lisibles depuis Windows (verifies sur l'hote Linux)")
            elif not (os.access(binary, os.X_OK) and os.access(launcher, os.X_OK)):
                c.fail("droit d'execution absent sur le binaire ou Backrooms.sh")
        else:
            info, err = binary_info(macho_info, binary)
            if err:
                c.fail(err)
            elif not info:
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
    reader = {"Windows": pe_info, "Linux": elf_info}.get(platform, macho_info)
    info, err = binary_info(reader, binary)
    if err or not info:
        c.fail(err or "binaire illisible")
        return
    if platform in ("Windows", "Linux"):
        c.info("binaire : %s" % info[0])
        if info[0] != "x86_64":
            c.fail("architecture %s, x86_64 attendu" % info[0])
        pattern = "*.dll" if platform == "Windows" else "*.so*"
        for lib in glob.glob(os.path.join(root, "**", pattern), recursive=True):
            d, derr = binary_info(reader, lib)
            if derr:
                c.fail(derr)
            elif d and d[0] != "x86_64":
                c.fail("bibliotheque %s en %s" % (os.path.relpath(lib, root), d[0]))
    else:
        want = set(expected_archs)
        c.info("binaire : %s" % "+".join(info[0]))
        for f in [binary] + glob.glob(os.path.join(root, "Contents", "**", "*"), recursive=True):
            if not os.path.isfile(f) or os.path.islink(f):
                continue
            d, derr = binary_info(macho_info, f)
            if derr:
                c.fail(derr)
                continue
            if d and not want.issubset(set(d[0])):
                c.fail("%s : %s (attendu : %s)" % (os.path.relpath(f, root), "+".join(d[0]), "+".join(sorted(want))))


def check_dependencies(c, platform, root, binary):
    if not binary:
        c.skip("binaire introuvable")
        return
    shipped = {os.path.basename(f).lower() for f in glob.glob(os.path.join(root, "**", "*"), recursive=True) if os.path.isfile(f)}
    reader = {"Windows": pe_info, "Linux": elf_info}.get(platform, macho_info)
    info, err = binary_info(reader, binary)
    if err or not info:
        c.fail(err or "binaire illisible")
        return
    if platform == "Windows":
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
        for lib in info[1]:
            if LINUX_SYSTEM_LIBS.match(lib) or lib.lower() in shipped:
                continue
            c.fail("bibliotheque requise absente du paquet : %s" % lib)
        c.info("requises : %s" % ", ".join(info[1]))
    else:
        for lib in info[1]:
            if lib.startswith(("/usr/lib/", "/System/")):
                continue
            if os.path.basename(lib).lower() not in shipped:
                c.fail("bibliotheque requise absente du bundle : %s" % lib)


def host_platform():
    return {"win32": "Windows", "linux": "Linux", "darwin": "Mac"}.get(sys.platform)


def launch_command(platform, root, launcher):
    """Commande qui lance le paquet (remplacee par les tests unitaires)"""
    if platform == "Mac":
        # open -W attend la fin de l'application ; son code de sortie n'est pas celui du jeu (le rapport fait foi)
        return ["open", "-W", "-n", root, "--args"]
    return [launcher]


LAUNCH_STATES = ("reussi", "echec", "incomplet")


def read_launch_report(path, out_dir, started):
    """(rapport, erreur) : le rapport doit exister, etre ecrit par CET essai et suivre le schema du jeu (v4.11)"""
    if not os.path.isfile(path):
        return None, "aucun rapport de lancement"
    if os.path.getmtime(path) + 2 < started:
        return None, "rapport anterieur a l'essai (%s)" % path
    try:
        with open(path, encoding="utf-8-sig") as fh:
            rep = json.load(fh)
    except (ValueError, OSError) as e:
        return None, "rapport de lancement illisible (%s)" % e
    if not isinstance(rep, dict):
        return None, "rapport de lancement mal forme (objet JSON attendu)"
    missing = [k for k in ("etat", "jeu", "ligne_de_commande", "problemes", "non_verifies") if k not in rep]
    if missing:
        return None, "rapport de lancement incomplet : champ(s) absent(s) %s" % ", ".join(missing)
    if rep["etat"] not in LAUNCH_STATES:
        return None, "etat inconnu dans le rapport : %r" % (rep["etat"],)
    for k in ("problemes", "non_verifies"):
        if not isinstance(rep[k], list) or not all(isinstance(x, str) for x in rep[k]):
            return None, "champ %s mal forme (liste de textes attendue)" % k
    cmdline = rep["ligne_de_commande"] if isinstance(rep["ligne_de_commande"], str) else ""
    # Le chemin du rapport est un dossier temporaire neuf : un rapport qui ne le cite pas vient d'un autre essai
    if "-BRSmokeTest" not in cmdline or os.path.basename(out_dir) not in cmdline:
        return None, "le rapport ne correspond pas a cet essai (ligne de commande : %s)" % cmdline[:200]
    return rep, None


def check_launch(c, platform, root, launcher, timeout, required):
    host = host_platform()
    if host != platform:
        c.skip("lancement possible seulement sur un hote %s (hote actuel : %s)" % (platform, host))
        return
    out_dir = tempfile.mkdtemp(prefix="br_smoke_")
    result = os.path.join(out_dir, "smoke.json")
    log = os.path.join(out_dir, "smoke.log")
    # -BRAutoTestStrict : une verification non faite rend le code 2 (jamais un succes silencieux)
    cmd = launch_command(platform, root, launcher) + ["-BRAutoTest", "-BRSmokeTest", "-BRAutoTestStrict", "-BRSmokeOut=%s" % result,
                                                       "-unattended", "-windowed", "-ResX=960", "-ResY=540", "-abslog=%s" % log]
    started = time.time()
    try:
        p = subprocess.run(cmd, timeout=timeout)
        code = p.returncode if platform != "Mac" else None
    except subprocess.TimeoutExpired:
        c.fail("le jeu n'a pas fini le test de lancement en %d s" % timeout)
        return
    except OSError as e:
        c.fail("lancement impossible : %s" % e)
        return
    rep, err = read_launch_report(result, out_dir, started)
    if err:
        c.fail("%s (code de sortie %s ; journal : %s)" % (err, code, log))
        return
    c.info("lancement : code %s, etat %s, %d probleme(s), %d verification(s) non faite(s)" % (
        "inconnu (open)" if code is None else code, rep["etat"], len(rep["problemes"]), len(rep["non_verifies"])))
    for prob in rep["problemes"]:
        c.fail(prob)
    if rep["etat"] == "echec" and not rep["problemes"]:
        c.fail("le jeu indique un echec sans le detailler")
    if rep["etat"] == "reussi" and (rep["problemes"] or rep["non_verifies"]):
        c.fail("rapport incoherent : etat reussi avec des problemes ou des verifications non faites")
    if code not in (None, 0, 2):
        c.fail("code de sortie %s" % code)
    if code == 2 and rep["etat"] != "incomplet":
        c.fail("code de sortie 2 (verifications non faites) mais etat %s" % rep["etat"])
    if code == 0 and rep["etat"] == "incomplet":
        c.info("code de sortie 0 malgre -BRAutoTestStrict : le rapport fait foi (etat incomplet)")
    if c.problems:
        return
    if rep["etat"] == "incomplet" or rep["non_verifies"]:
        for item in rep["non_verifies"]:
            c.info("non verifie : " + item)
        c.partial("le jeu a demarre mais %d verification(s) n'ont pas ete faites" % len(rep["non_verifies"]))


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

    def guarded(check, fn, *args):
        # v4.11 : une erreur imprevue dans une verification la met en ECHEC, avec son rapport (jamais de trace Python)
        try:
            return fn(check, *args)
        except Exception as e:  # noqa: BLE001
            check.fail("erreur interne de la verification : %s: %s" % (type(e).__name__, e))
            return None

    st = guarded(checks["structure"], check_structure, a.platform, a.dir, a.project)
    if st:
        root, launcher, binary, paks, utocs = st
        tool = find_unrealpak(a)
        if a.mode == "structure":
            checks["contenu"].skip("mode structure : contenu non lu")
        else:
            guarded(checks["contenu"], check_content, tool, paks, utocs)
        guarded(checks["architecture"], check_architecture, a.platform, root, binary, [x for x in a.archs.split(",") if x])
        guarded(checks["dependances"], check_dependencies, a.platform, root, binary)
        if a.launch or a.require_launch:
            if checks["structure"].close().state == OK:
                guarded(checks["lancement"], check_launch, a.platform, root, launcher, a.launch_timeout, a.require_launch)
            else:
                checks["lancement"].fail("structure en echec : paquet non lance")
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
    # v4.11 : un lancement demande (--launch) qui a eu lieu compte toujours : un echec fait echouer la commande, un essai
    # incomplet la rend incomplete. Seul un lancement impossible sur cet hote reste facultatif avec --launch.
    counted = list(required)
    if a.launch and "lancement" not in counted and checks["lancement"].state in (FAIL, PARTIAL):
        counted.append("lancement")
    failed = [n for n in counted if checks[n].state == FAIL]
    missing = [n for n in counted if checks[n].state in (SKIP, PARTIAL)]
    if failed:
        status, code = "ECHEC", 1
    elif missing:
        status, code = "INSPECTION INCOMPLETE" + (" (lancement incomplet)" if checks["lancement"].state == PARTIAL else ""), 2
    elif a.mode == "structure":
        status, code = "CONTROLE STRUCTUREL SEULEMENT (contenu non verifie)", 0
    else:
        status, code = "PAQUET VERIFIE" + (" ET LANCE" if checks["lancement"].state == OK else ""), 0

    print("Paquet %s : %s" % (a.platform, a.dir))
    for n in CHECKS:
        c = checks[n]
        flag = "" if n in counted else " (facultatif)"
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
                       "obligatoires": counted, "verifications": {n: checks[n].as_dict() for n in CHECKS}}, fh, ensure_ascii=False, indent=2)
    return code


if __name__ == "__main__":
    sys.exit(main())
