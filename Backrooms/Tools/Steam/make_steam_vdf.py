"""v4.9 : fichiers SteamPipe (app_build + un depot par systeme) prepares pour steamcmd.

Les identifiants ne sont pas dans le depot : ils sont donnes en arguments ou par l'environnement (STEAM_APP_ID,
STEAM_DEPOT_WINDOWS, STEAM_DEPOT_LINUX, STEAM_DEPOT_MAC). Les fichiers sont ecrits dans Build/SteamPipe/ (hors du depot).

- Un depot par systeme : les donnees cuisinees (shaders DirectX, Vulkan, Metal) ne sont pas les memes ; rien n'est
  partage entre systemes.
- Seuls les systemes dont le paquet existe et a passe check_package.py sont inclus (--platforms pour choisir).
- "Preview" vaut 1 par defaut : steamcmd verifie et journalise sans rien envoyer ; --upload pour envoyer.
- "SetLive" reste vide : aucune branche n'est publiee automatiquement (publication a faire dans Steamworks).

Usage :
  python Tools/Steam/make_steam_vdf.py --app 1234560 --depot-windows 1234561 --depot-linux 1234562 --depot-mac 1234563 \
      [--config Shipping] [--platforms Windows,Linux,Mac] [--desc "v4.9"] [--upload]
"""
import argparse
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "Build", "SteamPipe")

# Dossier du jeu dans l'archive de BuildCookRun, et fichiers a ne pas livrer
PLATFORMS = {
    "Windows": {"dir": "Windows", "exclude": ["*.pdb", "build_manifest.txt", "Manifest_*.txt"]},
    "Linux": {"dir": "Linux", "exclude": ["*.debug", "*.sym", "build_manifest.txt", "Manifest_*.txt"]},
    "Mac": {"dir": "Mac", "exclude": ["*.dSYM", "build_manifest.txt", "Manifest_*.txt"]},
}


def q(s):
    return '"%s"' % str(s).replace("\\", "/").replace('"', '\\"')


def depot_vdf(depot, content_root, excludes):
    lines = ['"DepotBuild"', "{", "\t\"DepotID\" %s" % q(depot), "\t\"ContentRoot\" %s" % q(content_root), "\t\"FileMapping\"", "\t{",
             "\t\t\"LocalPath\" \"*\"", "\t\t\"DepotPath\" \".\"", "\t\t\"Recursive\" \"1\"", "\t}"]
    lines += ["\t\"FileExclusion\" %s" % q(e) for e in excludes]
    lines.append("}")
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--app", default=os.environ.get("STEAM_APP_ID"))
    ap.add_argument("--depot-windows", default=os.environ.get("STEAM_DEPOT_WINDOWS"))
    ap.add_argument("--depot-linux", default=os.environ.get("STEAM_DEPOT_LINUX"))
    ap.add_argument("--depot-mac", default=os.environ.get("STEAM_DEPOT_MAC"))
    ap.add_argument("--config", default="Shipping")
    ap.add_argument("--platforms", default="Windows,Linux,Mac")
    ap.add_argument("--desc", default="Backrooms")
    ap.add_argument("--upload", action="store_true")
    a = ap.parse_args()
    if not a.app or not a.app.isdigit():
        sys.exit("AppID manquant (--app ou STEAM_APP_ID) : il se trouve dans Steamworks ; aucun AppID de demonstration n'est utilise.")
    depots = {"Windows": a.depot_windows, "Linux": a.depot_linux, "Mac": a.depot_mac}
    os.makedirs(OUT, exist_ok=True)
    entries = []
    for plat in [p.strip() for p in a.platforms.split(",") if p.strip()]:
        depot = depots.get(plat)
        if not depot or not depot.isdigit():
            sys.exit("DepotID manquant pour %s" % plat)
        archive = os.path.join(ROOT, "Build", plat, a.config)
        content = os.path.join(archive, PLATFORMS[plat]["dir"])
        if not os.path.isdir(content):
            sys.exit("Paquet %s absent (%s) : construire puis verifier avec Tools/Build/check_package.py" % (plat, content))
        name = "depot_%s.vdf" % plat.lower()
        open(os.path.join(OUT, name), "w").write(depot_vdf(depot, content, PLATFORMS[plat]["exclude"]))
        entries.append((depot, name))
    app = ['"AppBuild"', "{", "\t\"AppID\" %s" % q(a.app), "\t\"Desc\" %s" % q(a.desc), "\t\"BuildOutput\" %s" % q(os.path.join(OUT, "output")),
           "\t\"ContentRoot\" %s" % q(os.path.join(ROOT, "Build")), "\t\"SetLive\" \"\"", "\t\"Preview\" %s" % q("0" if a.upload else "1"), "\t\"Depots\"", "\t{"]
    app += ["\t\t%s %s" % (q(d), q(n)) for d, n in entries]
    app += ["\t}", "}"]
    open(os.path.join(OUT, "app_build.vdf"), "w").write("\n".join(app) + "\n")
    print("SteamPipe : %s (%s ; %s)" % (os.path.join(OUT, "app_build.vdf"), ", ".join(n for _, n in entries),
                                        "envoi" if a.upload else "apercu, rien n'est envoye"))


if __name__ == "__main__":
    main()
