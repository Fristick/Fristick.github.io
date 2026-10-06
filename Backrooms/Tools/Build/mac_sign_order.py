"""v4.11 : ordre de signature d'un bundle macOS, de l'interieur vers l'exterieur.

Apple exige que tout code imbrique soit signe avant ce qui le contient : bibliotheques et executables auxiliaires,
puis les bundles imbriques (.app, .framework, .bundle, .plugin, .xpc), puis le bundle principal. La v4.10 ne signait que
les .dylib, .so et .framework : un executable auxiliaire (CrashReportClient, outils de shaders) ou une bibliotheque sans
extension restait non signe, et la notarisation le refusait.

Chaque fichier est reconnu par son contenu (en-tete Mach-O, fin ou universel), pas par son extension. L'executable
principal d'un bundle est signe avec son bundle (il n'apparait pas seul). Le bundle principal vient en dernier.

Usage : python Tools/Build/mac_sign_order.py <Backrooms.app>       (un chemin par ligne, dans l'ordre de signature)
        python Tools/Build/mac_sign_order.py --print0 <Backrooms.app>  (separes par NUL, pour une boucle shell)
"""
import os
import struct
import sys

BUNDLE_EXTS = (".app", ".framework", ".bundle", ".plugin", ".xpc", ".appex")
MACHO_MAGICS = {0xFEEDFACE, 0xFEEDFACF, 0xCEFAEDFE, 0xCFFAEDFE}
FAT_MAGICS = {0xCAFEBABE, 0xCAFEBABF}


def is_macho(path):
    try:
        with open(path, "rb") as fh:
            head = fh.read(8)
    except OSError:
        return False
    if len(head) < 8:
        return False
    magic_be = struct.unpack(">I", head[:4])[0]
    if magic_be in MACHO_MAGICS:
        return True
    if magic_be in FAT_MAGICS:
        # Les classes Java portent aussi 0xCAFEBABE : un binaire universel annonce peu d'architectures
        return 0 < struct.unpack(">I", head[4:8])[0] <= 32
    return False


def bundle_main_executable(bundle):
    """Executable principal declare par Info.plist (CFBundleExecutable), ou None"""
    for plist in (os.path.join(bundle, "Contents", "Info.plist"), os.path.join(bundle, "Resources", "Info.plist"),
                  os.path.join(bundle, "Info.plist")):
        try:
            with open(plist, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            continue
        key = text.find("<key>CFBundleExecutable</key>")
        if key < 0:
            continue
        start = text.find("<string>", key)
        end = text.find("</string>", start)
        if start < 0 or end < 0:
            continue
        name = text[start + len("<string>"):end].strip()
        for cand in (os.path.join(bundle, "Contents", "MacOS", name), os.path.join(bundle, name)):
            if os.path.isfile(cand):
                return os.path.normpath(cand)
    return None


def signing_order(app):
    """Liste des chemins a signer : fichiers Mach-O et bundles imbriques, du plus profond au moins profond, puis app"""
    app = os.path.normpath(app)
    bundles = []
    files = []
    for dirpath, dirnames, filenames in os.walk(app):
        for d in dirnames:
            full = os.path.join(dirpath, d)
            if d.endswith(BUNDLE_EXTS) and not os.path.islink(full):
                bundles.append(os.path.normpath(full))
        for f in filenames:
            full = os.path.join(dirpath, f)
            if not os.path.islink(full) and is_macho(full):
                files.append(os.path.normpath(full))
    mains = {m for m in (bundle_main_executable(b) for b in bundles + [app]) if m}
    items = [f for f in files if f not in mains] + bundles

    def depth(path):
        return os.path.relpath(path, app).count(os.sep)

    # Plus profond d'abord ; a profondeur egale, les fichiers avant les bundles (un bundle scelle son contenu)
    items.sort(key=lambda p: (-depth(p), p.endswith(BUNDLE_EXTS), p))
    return items + [app]


def main(argv):
    args = [a for a in argv if a != "--print0"]
    if len(args) != 1 or not os.path.isdir(args[0]):
        print(__doc__)
        return 3
    order = signing_order(args[0])
    if "--print0" in argv:
        sys.stdout.write("\0".join(order) + "\0")
    else:
        print("\n".join(order))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
