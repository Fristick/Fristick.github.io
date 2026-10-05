"""v4.9 : controle hors moteur des noms references par le code, pour Linux et macOS.

Sous Linux (et sur un volume macOS sensible a la casse), "SK_hound" et "SK_Hound" sont deux fichiers differents.
Ce script verifie que chaque nom de ressource ecrit dans le code C++ (SK_, SM_, T_, S_, UI_, I_) correspond exactement,
casse comprise, a un fichier de RawAssets (source de l'import), que les polices de BRFonts.cpp existent dans
Content/Fonts, et que chaque #include "..." du module designe un en-tete du module avec la meme casse.

Un nom introuvable n'est pas une erreur (noms composes a l'execution, ressources facultatives avec repli) : il est liste.
Une difference de casse seule est une erreur (code de sortie 1).

Usage :  python Tools/check_paths.py
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC = os.path.join(ROOT, "Source", "Backrooms")
RAW = os.path.join(ROOT, "RawAssets")
FONTS = os.path.join(ROOT, "Content", "Fonts")

PREFIX_DIRS = {"SK_": ["Skeletal"], "SM_": ["Meshes"], "T_": ["Textures"], "S_": ["Sounds"], "UI_": ["Icons"], "I_": ["Icons"]}


def sources():
    for sub in ("Public", "Private"):
        d = os.path.join(SRC, sub)
        for f in sorted(os.listdir(d)):
            if f.endswith((".h", ".cpp", ".inl")):
                yield os.path.join(d, f)


def raw_names(folder):
    names = set()
    d = os.path.join(RAW, folder)
    if os.path.isdir(d):
        for f in os.listdir(d):
            names.add(os.path.splitext(f)[0])
    return names


def main():
    errors = []
    missing = []
    checked = 0
    available = {p: set().union(*(raw_names(d) for d in dirs)) for p, dirs in PREFIX_DIRS.items()}
    # Les textures importees gardent leur nom ; les cartes derivees (_N, _R) sont des fichiers a part
    name_re = re.compile(r'TEXT\("((?:SK|SM|T|S|UI|I)_[A-Za-z0-9_]+)"\)')
    for path in sources():
        text = open(path, encoding="ascii", errors="replace").read()
        for m in name_re.finditer(text):
            name = m.group(1)
            prefix = next(p for p in sorted(PREFIX_DIRS, key=len, reverse=True) if name.startswith(p))
            pool = available[prefix]
            checked += 1
            if name in pool:
                continue
            lower = {n.lower(): n for n in pool}
            if name.lower() in lower:
                errors.append("%s : %s ecrit avec une autre casse que le fichier %s" % (os.path.basename(path), name, lower[name.lower()]))
            else:
                missing.append("%s : %s" % (os.path.basename(path), name))
    # Polices
    fonts_cpp = open(os.path.join(SRC, "Private", "BRFonts.cpp"), encoding="ascii").read()
    have_fonts = set(os.listdir(FONTS)) if os.path.isdir(FONTS) else set()
    for f in sorted(set(re.findall(r'"(NotoSans[A-Za-z]+-(?:Regular|Bold)\.ttf)"', fonts_cpp)) | set(re.findall(r'TEXT\("(NotoSans[A-Za-z]+)"\)', fonts_cpp))):
        checked += 1
        candidates = [f] if f.endswith(".ttf") else [f + "-Regular.ttf", f + "-Bold.ttf"]
        for c in candidates:
            if c not in have_fonts:
                low = {x.lower(): x for x in have_fonts}
                errors.append("BRFonts.cpp : police %s %s" % (c, "ecrite avec une autre casse que " + low[c.lower()] if c.lower() in low else "absente de Content/Fonts"))
    # Inclusions du module
    headers = {}
    for sub in ("Public", "Private"):
        for f in os.listdir(os.path.join(SRC, sub)):
            headers.setdefault(f.lower(), f)
    inc_re = re.compile(r'#include\s+"([^"/]+\.(?:h|inl))"')
    for path in sources():
        for inc in inc_re.findall(open(path, encoding="ascii", errors="replace").read()):
            if not (inc.startswith("BR") or inc.startswith("Backrooms")):
                continue
            checked += 1
            real = headers.get(inc.lower())
            if real is None:
                if not inc.endswith(".generated.h"):
                    errors.append("%s : #include \"%s\" introuvable" % (os.path.basename(path), inc))
            elif real != inc:
                errors.append("%s : #include \"%s\" mais le fichier s'appelle %s" % (os.path.basename(path), inc, real))
    print("references verifiees : %d" % checked)
    print("differences de casse : %d" % len(errors))
    for e in errors:
        print("  ERREUR " + e)
    print("noms sans fichier source (composes a l'execution ou facultatifs) : %d" % len(missing))
    for m in sorted(set(missing))[:60]:
        print("  " + m)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
