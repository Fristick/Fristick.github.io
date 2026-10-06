"""v4.10 : fichiers SteamPipe (app_build + un depot par systeme) prepares pour steamcmd. Rien n'est envoye.

Les identifiants ne sont pas dans le depot : ils sont donnes en arguments ou par l'environnement (STEAM_APP_ID,
STEAM_DEPOT_WINDOWS, STEAM_DEPOT_LINUX, STEAM_DEPOT_MAC). Les fichiers sont ecrits dans Build/SteamPipe/ (hors du depot).

Refus (code 1, aucun fichier ecrit ni remplace) :
- un systeme inconnu, un AppID ou un DepotID absent, nul ou non numerique, deux depots identiques ;
- un paquet absent ou vide ;
- un paquet que Tools/Build/check_package.py ne declare pas complet en mode release (code 0 exige : un echec, une
  inspection incomplete ou un outil absent sont des refus) ;
- un paquet construit depuis un autre commit que l'arbre courant (build_manifest.txt), ou sans manifeste, ou avec des
  modifications non validees (« arbre : modifie » ; --allow-dirty pour un essai local) ;
- des residus d'une construction precedente dans le dossier archive (fichiers absents des listes de BuildCookRun,
  conteneurs plus anciens que les autres) ;
- v4.12 : un paquet dont le canal de contenu (build_manifest.txt : « contenu : public|internal ») n'est pas celui
  demande (--content, public par defaut), ou inconnu (scripts anterieurs a la v4.12). Une version de test interne
  (Build/<Systeme>/<Config>-internal) n'est preparee qu'avec --content internal : elle ne peut pas partir par erreur
  avec les fichiers de la version publiee.
Un seul paquet refuse suffit : aucun fichier n'est ecrit pour les autres (pas de livraison partielle).

Les fichiers sont d'abord ecrits dans un dossier temporaire, puis mis en place d'un bloc : un echec en cours de route
laisse les fichiers precedents intacts.

- Un depot par systeme : les donnees cuisinees (shaders DirectX, Vulkan, Metal) ne sont pas les memes.
- "Preview" vaut 1 par defaut : steamcmd verifie et journalise sans rien envoyer. --no-preview ecrit Preview 0 ; l'envoi
  reste une commande manuelle (Tools/Steam/upload_steam.sh), jamais lancee par ce script.
- "SetLive" reste vide : aucune branche n'est publiee automatiquement (publication a faire dans Steamworks).

Usage :
  python Tools/Steam/make_steam_vdf.py --app 1234560 --depot-windows 1234561 [--depot-linux ...] [--depot-mac ...] \
      --platforms Windows[,Linux,Mac] --ue <moteur> [--config Shipping] [--desc "v4.10"]
Codes : 0 fichiers ecrits ; 1 refus ; 3 erreur d'utilisation.
"""
import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
CHECK = os.path.join(ROOT, "Tools", "Build", "check_package.py")

# Dossier du jeu dans l'archive de BuildCookRun, et fichiers a ne pas livrer
PLATFORMS = {
    "Windows": {"dir": "Windows", "exclude": ["*.pdb", "build_manifest.txt", "Manifest_*.txt"]},
    "Linux": {"dir": "Linux", "exclude": ["*.debug", "*.sym", "build_manifest.txt", "Manifest_*.txt"]},
    "Mac": {"dir": "Mac", "exclude": ["*.dSYM", "build_manifest.txt", "Manifest_*.txt"]},
}
CONTAINERS = (".pak", ".utoc", ".ucas", ".sig")
# Ecart tolere entre les conteneurs d'une meme construction (BuildCookRun les ecrit a la suite)
STALE_SECONDS = 3600


class Refused(Exception):
    pass


def q(s):
    return '"%s"' % str(s).replace("\\", "/").replace('"', '\\"')


def depot_vdf(depot, content_root, excludes):
    lines = ['"DepotBuild"', "{", "\t\"DepotID\" %s" % q(depot), "\t\"ContentRoot\" %s" % q(content_root), "\t\"FileMapping\"", "\t{",
             "\t\t\"LocalPath\" \"*\"", "\t\t\"DepotPath\" \".\"", "\t\t\"Recursive\" \"1\"", "\t}"]
    lines += ["\t\"FileExclusion\" %s" % q(e) for e in excludes]
    lines.append("}")
    return "\n".join(lines) + "\n"


def app_vdf(app, desc, out, content_root, entries, preview):
    lines = ['"AppBuild"', "{", "\t\"AppID\" %s" % q(app), "\t\"Desc\" %s" % q(desc), "\t\"BuildOutput\" %s" % q(os.path.join(out, "output")),
             "\t\"ContentRoot\" %s" % q(content_root), "\t\"SetLive\" \"\"", "\t\"Preview\" %s" % q("1" if preview else "0"), "\t\"Depots\"", "\t{"]
    lines += ["\t\t%s %s" % (q(d), q(n)) for d, n in entries]
    lines += ["\t}", "}"]
    return "\n".join(lines) + "\n"


def valid_id(value):
    return bool(value) and str(value).isdigit() and int(value) > 0


def git_head(project):
    try:
        p = subprocess.run(["git", "-C", project, "rev-parse", "HEAD"], capture_output=True, text=True, timeout=30)
    except (OSError, subprocess.TimeoutExpired):
        return None
    return p.stdout.strip() if p.returncode == 0 and p.stdout.strip() else None


def read_manifest(archive):
    path = os.path.join(archive, "build_manifest.txt")
    try:
        with open(path, encoding="utf-8-sig", errors="replace") as fh:
            text = fh.read()
    except OSError:
        return None
    out = {}
    for line in text.splitlines():
        if ":" in line:
            k, v = line.split(":", 1)
            out[k.strip()] = v.strip()
    return out


def staged_files(archive):
    """Fichiers listes par BuildCookRun (Manifest_*.txt : chemin<TAB>date) ; None si aucune liste"""
    lists = glob.glob(os.path.join(archive, "**", "Manifest_*.txt"), recursive=True)
    if not lists:
        return None
    files = set()
    for path in lists:
        with open(path, encoding="utf-8-sig", errors="replace") as fh:
            for line in fh:
                name = line.split("\t", 1)[0].strip().replace("\\", "/")
                if name:
                    files.add(name.lower())
    return files


def residues(archive, content):
    """Fichiers laisses par une construction precedente (le dossier archive n'est pas vide par BuildCookRun)"""
    found = []
    listed = staged_files(archive)
    if listed is None:
        return ["aucune liste Manifest_*.txt de BuildCookRun : residus impossibles a verifier"]
    containers = []
    for f in glob.glob(os.path.join(content, "**", "*"), recursive=True):
        if not os.path.isfile(f):
            continue
        rel = os.path.relpath(f, content).replace("\\", "/")
        base = os.path.basename(f)
        if base == "build_manifest.txt" or re.match(r"Manifest_.*\.txt$", base):
            continue
        if base.lower().endswith(CONTAINERS):
            containers.append(f)
            continue  # les conteneurs sont ecrits apres les listes : compares entre eux ci-dessous
        if rel.lower() not in listed and not any(l.endswith("/" + rel.lower()) for l in listed):
            found.append("fichier absent des listes de la construction : %s" % rel)
    if containers:
        newest = max(os.path.getmtime(f) for f in containers)
        for f in containers:
            if newest - os.path.getmtime(f) > STALE_SECONDS:
                found.append("conteneur plus ancien que la construction : %s" % os.path.relpath(f, content).replace("\\", "/"))
    return found


def check_package(plat, archive, args, workdir):
    """Code de check_package.py en mode release (0 exige) et resume du rapport JSON"""
    report = os.path.join(workdir, "check_%s.json" % plat.lower())
    cmd = [sys.executable, args.check_tool, "--platform", plat, "--dir", archive, "--mode", "release", "--json", report, "--project", args.project]
    if args.ue:
        cmd += ["--ue", args.ue]
    if args.unrealpak:
        cmd += ["--unrealpak", args.unrealpak]
    if args.require_launch:
        cmd += ["--require-launch"]
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=args.check_timeout)
    except (OSError, subprocess.TimeoutExpired) as e:
        raise Refused("%s : check_package.py n'a pas pu s'executer (%s)" % (plat, e))
    try:
        with open(report, encoding="utf-8") as fh:
            rep = json.load(fh)
    except (OSError, ValueError):
        rep = {}
    if p.returncode != 0 or rep.get("code") != 0:
        reasons = []
        for name, chk in sorted(rep.get("verifications", {}).items()):
            if name in rep.get("obligatoires", []) and chk.get("etat") != "OK":
                detail = "; ".join(chk.get("problemes", [])[:3]) or chk.get("raison", "")
                reasons.append("%s %s%s" % (name, chk.get("etat"), (" (" + detail + ")") if detail else ""))
        raise Refused("%s : paquet refuse par check_package.py (code %s, %s)%s" % (plat, p.returncode, rep.get("resultat", "sans rapport"),
                                                                                 (" : " + " | ".join(reasons)) if reasons else ""))
    return rep


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--app", default=os.environ.get("STEAM_APP_ID"))
    ap.add_argument("--depot-windows", default=os.environ.get("STEAM_DEPOT_WINDOWS"))
    ap.add_argument("--depot-linux", default=os.environ.get("STEAM_DEPOT_LINUX"))
    ap.add_argument("--depot-mac", default=os.environ.get("STEAM_DEPOT_MAC"))
    ap.add_argument("--config", default="Shipping", choices=["Shipping", "Development"])
    ap.add_argument("--platforms", default="Windows")
    ap.add_argument("--desc", default="Backrooms")
    ap.add_argument("--no-preview", action="store_true", help="Preview 0 (l'envoi reste manuel : upload_steam.sh)")
    ap.add_argument("--ue", help="dossier du moteur (UnrealPak, pour check_package.py)")
    ap.add_argument("--unrealpak", help="outil de liste des conteneurs (remplace --ue)")
    ap.add_argument("--require-launch", action="store_true", help="exiger le test de lancement (hote du meme systeme)")
    ap.add_argument("--build-root", default=os.path.join(ROOT, "Build"))
    ap.add_argument("--out", default=None, help="dossier des .vdf (defaut : <build-root>/SteamPipe)")
    ap.add_argument("--project", default=ROOT)
    ap.add_argument("--commit", default=None, help="commit attendu dans build_manifest.txt (defaut : HEAD du projet)")
    ap.add_argument("--check-tool", default=CHECK)
    ap.add_argument("--content", default="public", choices=["public", "internal"],
                    help="canal de contenu du paquet (v4.12, Docs/PLAN_PUBLICATION.md) : public, ou internal (version de test)")
    ap.add_argument("--allow-dirty", action="store_true", help="accepter un paquet construit avec des modifications non validees (essai local seulement)")
    ap.add_argument("--check-timeout", type=int, default=3600)
    a = ap.parse_args(argv)
    out = a.out or os.path.join(a.build_root, "SteamPipe")

    # --- Arguments
    plats = [p.strip() for p in a.platforms.split(",") if p.strip()]
    unknown = [p for p in plats if p not in PLATFORMS]
    if not plats or unknown:
        print("ERREUR : systeme inconnu %s (attendu : %s)" % (", ".join(unknown) or "(aucun)", ", ".join(PLATFORMS)))
        return 3
    if len(set(plats)) != len(plats):
        print("ERREUR : systeme repete dans --platforms")
        return 3
    errors = []
    if not valid_id(a.app):
        errors.append("AppID absent ou invalide (--app ou STEAM_APP_ID, nombre > 0) : aucun AppID de demonstration n'est utilise")
    depots = {"Windows": a.depot_windows, "Linux": a.depot_linux, "Mac": a.depot_mac}
    used = {}
    for plat in plats:
        d = depots[plat]
        if not valid_id(d):
            errors.append("DepotID absent ou invalide pour %s" % plat)
        elif d in used:
            errors.append("DepotID %s utilise pour %s et %s : un depot par systeme" % (d, used[d], plat))
        elif d == a.app:
            errors.append("DepotID de %s identique a l'AppID" % plat)
        else:
            used[d] = plat
    if errors:
        for e in errors:
            print("ERREUR : " + e)
        return 3

    # --- Paquets : tous verifies avant d'ecrire quoi que ce soit
    expected_commit = a.commit or git_head(a.project)
    refusals = []
    entries = []
    work = tempfile.mkdtemp(prefix="br_steampipe_")
    try:
        for plat in plats:
            archive = os.path.join(a.build_root, plat, a.config + ("-internal" if a.content == "internal" else ""))
            # Unreal 5.8 archive le jeu directement dans le dossier demande ; d'autres versions ajoutent <Plateforme>/
            content = os.path.join(archive, PLATFORMS[plat]["dir"])
            if not os.path.isdir(content) and os.path.isdir(archive):
                content = archive
            try:
                if not os.path.isdir(content):
                    raise Refused("%s : paquet absent (%s) ; construire puis verifier avec Tools/Build/check_package.py" % (plat, content))
                if not any(os.path.isfile(f) for f in glob.glob(os.path.join(content, "**", "*"), recursive=True)):
                    raise Refused("%s : paquet vide (%s)" % (plat, content))
                manifest = read_manifest(archive)
                if manifest is None:
                    raise Refused("%s : build_manifest.txt absent (paquet d'origine inconnue)" % plat)
                if not expected_commit:
                    raise Refused("%s : commit courant inconnu (git indisponible ; --commit pour le donner)" % plat)
                if manifest.get("arbre") == "modifie" and not a.allow_dirty:
                    raise Refused("%s : paquet construit avec des modifications non validees (build_manifest.txt : arbre modifie) : valider puis reconstruire" % plat)
                if manifest.get("commit") != expected_commit:
                    raise Refused("%s : paquet construit depuis %s, arbre courant %s : reconstruire" % (plat, manifest.get("commit") or "(inconnu)", expected_commit))
                if manifest.get("configuration") not in (None, a.config):
                    raise Refused("%s : manifeste en %s, %s demande" % (plat, manifest.get("configuration"), a.config))
                if manifest.get("contenu") is None:
                    raise Refused("%s : canal de contenu inconnu (build_manifest.txt sans ligne contenu : scripts anterieurs a la v4.12) : reconstruire" % plat)
                if manifest.get("contenu") != a.content:
                    raise Refused("%s : paquet de contenu %s, %s demande (--content)" % (plat, manifest.get("contenu"), a.content))
                left = residues(archive, content)
                if left:
                    raise Refused("%s : residus d'une construction precedente : %s" % (plat, "; ".join(left[:5])))
                check_package(plat, archive, a, work)
                entries.append((plat, depots[plat], content))
            except Refused as e:
                refusals.append(str(e))
        if refusals:
            for r in refusals:
                print("REFUS : " + r)
            print("RESULTAT : aucun fichier SteamPipe ecrit (%d paquet(s) refuse(s) sur %d)" % (len(refusals), len(plats)))
            return 1

        # --- Ecriture dans un dossier temporaire, puis mise en place d'un bloc
        parent = os.path.dirname(os.path.abspath(out))
        os.makedirs(parent, exist_ok=True)
        staging = tempfile.mkdtemp(prefix=".steampipe_", dir=parent)
        names = []
        for plat, depot, content in entries:
            name = "depot_%s.vdf" % plat.lower()
            with open(os.path.join(staging, name), "w", encoding="utf-8", newline="\n") as fh:
                fh.write(depot_vdf(depot, content, PLATFORMS[plat]["exclude"]))
            names.append((depot, name))
        with open(os.path.join(staging, "app_build.vdf"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(app_vdf(a.app, a.desc, out, a.build_root, names, not a.no_preview))
        old = None
        if os.path.exists(out):
            old = out + ".old-%d" % int(time.time() * 1000)
            os.replace(out, old)
        try:
            os.replace(staging, out)
        except OSError:
            if old:
                os.replace(old, out)  # les fichiers precedents reviennent
            raise
        if old:
            shutil.rmtree(old, ignore_errors=True)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print("SteamPipe : %s (%s) ; contenu %s ; %s ; SetLive vide ; aucun envoi (upload_steam.sh, manuel)" % (
        os.path.join(out, "app_build.vdf"), ", ".join(n for _, n in names), a.content, "apercu (Preview 1)" if not a.no_preview else "Preview 0"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
