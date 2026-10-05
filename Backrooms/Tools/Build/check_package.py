"""v4.9 : controle d'un jeu empaquete (Windows, Linux ou macOS) : rien ne doit manquer chez le joueur.

Verifie, dans le dossier produit par BuildCookRun (-archivedirectory) :
  - l'executable de lancement (Backrooms.exe, Backrooms.sh, Backrooms.app) ;
  - les paquets de contenu (.pak, et .utoc/.ucas si IoStore) ;
  - avec UnrealPak (--ue) : les 10 polices de Content/Fonts, les 22 Game.locres et Game.locmeta, la carte ;
  - qu'aucun fichier de RawAssets (sources de l'import) ni script Python n'est livre.
Le contenu cuisine (.uasset dans IoStore) n'est pas liste ici : le test automatique du jeu empaquete le verifie.

Usage :  python Tools/Build/check_package.py --platform Windows|Linux|Mac --dir Build/<Plateforme>/<Config> [--ue <moteur>]
Code de sortie : 0 si complet, 1 sinon.
"""
import argparse
import glob
import os
import re
import subprocess
import sys

CULTURES = ["fr", "en", "de", "es-ES", "pt-BR", "ru", "it", "tr", "es-419", "pl", "zh-Hans", "uk", "ar", "ko", "fa", "ja", "hu", "cs",
            "pt-PT", "sv", "zh-Hant", "id"]
FONTS = ["NotoSans%s-%s.ttf" % (s, w) for s in ("Arabic", "SC", "TC", "JP", "KR") for w in ("Regular", "Bold")]


def find_root(platform, base):
    """Dossier du jeu dans l'archive (BuildCookRun ajoute un sous-dossier par plateforme)"""
    if platform == "Mac":
        apps = glob.glob(os.path.join(base, "**", "*.app"), recursive=True)
        return apps[0] if apps else None
    for cand in (os.path.join(base, platform), os.path.join(base, platform + "NoEditor"), base):
        exe = "Backrooms.exe" if platform == "Windows" else "Backrooms.sh"
        if os.path.isfile(os.path.join(cand, exe)):
            return cand
    return None


def unreal_pak(ue, platform_host):
    sub = {"win32": "Win64", "linux": "Linux", "darwin": "Mac"}.get(platform_host, "Linux")
    exe = os.path.join(ue, "Engine", "Binaries", sub, "UnrealPak" + (".exe" if sub == "Win64" else ""))
    return exe if os.path.isfile(exe) else None


def list_pak(tool, pak):
    out = subprocess.run([tool, pak, "-List"], capture_output=True, text=True, errors="replace").stdout
    return set(m.group(1).replace("\\", "/") for m in re.finditer(r'"([^"]+)"', out))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--platform", required=True, choices=["Windows", "Linux", "Mac"])
    ap.add_argument("--dir", required=True)
    ap.add_argument("--ue")
    a = ap.parse_args()
    problems = []
    root = find_root(a.platform, a.dir)
    if not root:
        print("PROBLEME : executable de lancement introuvable dans %s" % a.dir)
        return 1
    print("jeu : %s" % root)
    paks = glob.glob(os.path.join(root, "**", "*.pak"), recursive=True)
    utocs = glob.glob(os.path.join(root, "**", "*.utoc"), recursive=True)
    print("paquets : %d .pak, %d .utoc" % (len(paks), len(utocs)))
    if not paks:
        problems.append("aucun .pak")
    # Fichiers livres en clair qui ne devraient pas l'etre
    for f in glob.glob(os.path.join(root, "**", "*"), recursive=True):
        rel = os.path.relpath(f, root).replace("\\", "/")
        if "/RawAssets/" in "/" + rel or rel.endswith(".py") or "/Content/Python/" in "/" + rel:
            problems.append("livre a tort : %s" % rel)
    if a.ue:
        tool = unreal_pak(a.ue, sys.platform)
        if not tool:
            print("UnrealPak introuvable dans %s : contenu des paquets non verifie" % a.ue)
        else:
            entries = set()
            for p in paks:
                entries |= list_pak(tool, p)
            joined = "\n".join(sorted(entries))
            for f in FONTS:
                if not re.search(r"Content/Fonts/" + re.escape(f) + r"$", joined, re.M):
                    problems.append("police absente du paquet : Content/Fonts/%s" % f)
            for c in CULTURES:
                if not re.search(r"Localization/Game/" + re.escape(c) + r"/Game\.locres$", joined, re.M):
                    problems.append("traduction absente du paquet : %s/Game.locres" % c)
            if not re.search(r"Localization/Game/Game\.locmeta$", joined, re.M):
                problems.append("Game.locmeta absent du paquet")
            if not re.search(r"L_Backrooms\.(umap|utoc)", joined) and not utocs:
                problems.append("carte L_Backrooms introuvable (ni dans les .pak, ni IoStore)")
            if "RawAssets/" in joined:
                problems.append("RawAssets present dans un paquet")
            print("entrees listees : %d" % len(entries))
    if problems:
        print("RESULTAT : %d probleme(s)" % len(problems))
        for p in problems:
            print("  PROBLEME : " + p)
        return 1
    print("RESULTAT : paquet complet pour les controles disponibles")
    return 0


if __name__ == "__main__":
    sys.exit(main())
