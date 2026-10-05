"""
v4.5 : protection des modeles et ressources fournis par l'utilisateur.

Le manifeste RawAssets/protected_assets.json liste :
- "derived" : les fichiers de RawAssets/ tires des modeles, textures et sons fournis (pieces SM_*ET_*, combinaison
  SM_Hazmat_*, maillages a squelette SK_*, textures d'origine, meubles du Niveau 4, carrelage et platre des Poolrooms,
  sons fournis), avec leur empreinte SHA-256 ;
- "sources" : les originaux de Tools/SourceModels/ (non versionnes), avec leur empreinte.

Les scripts de generation (import_user_models.py, build_entity_skeletal.py, generate_models.py, generate_textures.py)
appellent may_write() avant d'ecrire : un fichier protege n'est jamais remplace sans --force, et avec --force l'ancienne
version est d'abord copiee dans RawAssets/_Backup/<date>/ (retour arriere possible, en plus de git).

Ligne de commande :
    python Tools/protect_assets.py check     verifie les empreintes (fichiers modifies, manquants, originaux changes)
    python Tools/protect_assets.py update    re-enregistre les empreintes des fichiers proteges presents (apres --force)
    python Tools/protect_assets.py list      liste les fichiers proteges
"""
import datetime
import fnmatch
import hashlib
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
MANIFEST = os.path.join(ROOT, "RawAssets", "protected_assets.json")
BACKUP = os.path.join(ROOT, "RawAssets", "_Backup")

# Fichiers de RawAssets/ derives de ressources fournies (motifs relatifs a la racine du projet)
PROTECTED_PATTERNS = [
    "RawAssets/Meshes/SM_Hazmat_*.fbx",
    "RawAssets/Meshes/SM_BacteriaET_*.fbx",
    "RawAssets/Meshes/SM_DeathmothET_*.fbx",
    "RawAssets/Meshes/SM_SkinStealerET_*.fbx",
    "RawAssets/Meshes/SM_FacelingET_*.fbx",
    "RawAssets/Meshes/SM_PartygoerET_*.fbx",
    "RawAssets/Meshes/SM_HoundET_*.fbx",
    "RawAssets/Meshes/SM_OfficeDeskET.fbx",
    "RawAssets/Meshes/SM_OfficeChairET.fbx",
    "RawAssets/Meshes/SM_WaterCoolerET.fbx",
    "RawAssets/Skeletal/SK_Hazmat.fbx",
    "RawAssets/Skeletal/SK_Faceling.fbx",
    "RawAssets/Skeletal/SK_Partygoer.fbx",
    "RawAssets/Skeletal/SK_SkinStealer.fbx",
    "RawAssets/Skeletal/SK_Hound.fbx",
    "RawAssets/Skeletal/SK_HoundLite.fbx",
    "RawAssets/Textures/T_Hazmat_*.jpg",
    "RawAssets/Textures/T_Deathmoth.jpg",
    "RawAssets/Textures/T_SkinStealer_*.jpg",
    "RawAssets/Textures/T_Faceling.jpg",
    "RawAssets/Textures/T_Partygoer.jpg",
    "RawAssets/Textures/T_Hound.jpg",
    "RawAssets/Textures/T_PoolTile37.jpg",
    "RawAssets/Textures/T_PoolTile37_N.png",
    "RawAssets/Textures/T_Plaster.jpg",
    "RawAssets/Textures/T_Plaster_N.png",
    "RawAssets/Sounds/S_Bacteria.wav",
    "RawAssets/Sounds/S_Scare_Bacteria.wav",
    "RawAssets/Sounds/S_LightBuzz.wav",
]
SOURCE_DIR = "Tools/SourceModels"


def rel(path):
    return os.path.relpath(os.path.abspath(path), ROOT).replace(os.sep, "/")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def is_protected(path):
    r = rel(path)
    return any(fnmatch.fnmatch(r, p) for p in PROTECTED_PATTERNS)


def load():
    if os.path.isfile(MANIFEST):
        with open(MANIFEST) as fh:
            return json.load(fh)
    return {"derived": {}, "sources": {}}


def save(data):
    os.makedirs(os.path.dirname(MANIFEST), exist_ok=True)
    with open(MANIFEST, "w") as fh:
        json.dump(data, fh, indent=1, sort_keys=True)


def may_write(path, force=False):
    """True si le script peut ecrire ce fichier. Un fichier protege existant n'est remplace qu'avec force=True,
    apres une copie de sauvegarde."""
    if not os.path.exists(path) or not is_protected(path):
        return True
    if not force:
        print("  PROTEGE, non remplace : %s (relancer avec --force pour le regenerer ; une copie sera faite)" % rel(path))
        return False
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    dst = os.path.join(BACKUP, stamp, rel(path))
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(path, dst)
    print("  sauvegarde avant remplacement :", rel(dst))
    return True


def record(path, note=""):
    """Enregistre l'empreinte d'un fichier protege qui vient d'etre ecrit"""
    if not is_protected(path) or not os.path.isfile(path):
        return
    data = load()
    data.setdefault("derived", {})[rel(path)] = {"sha256": sha256(path), "note": note}
    save(data)


def scan_sources(data):
    src = os.path.join(ROOT, SOURCE_DIR)
    if not os.path.isdir(src):
        return
    for dirpath, _, files in os.walk(src):
        for f in files:
            p = os.path.join(dirpath, f)
            data.setdefault("sources", {})[rel(p)] = {"sha256": sha256(p), "bytes": os.path.getsize(p)}


def protected_files():
    out = []
    for p in PROTECTED_PATTERNS:
        d = os.path.join(ROOT, os.path.dirname(p))
        if os.path.isdir(d):
            out += [os.path.join(d, f) for f in sorted(os.listdir(d)) if fnmatch.fnmatch(f, os.path.basename(p))]
    return out


def update():
    data = load()
    for f in protected_files():
        old = data.setdefault("derived", {}).get(rel(f), {})
        data["derived"][rel(f)] = {"sha256": sha256(f), "note": old.get("note", "ressource fournie ou derivee")}
    scan_sources(data)
    save(data)
    print("%d fichiers proteges, %d originaux enregistres -> %s" % (len(data["derived"]), len(data.get("sources", {})), rel(MANIFEST)))


def check():
    data = load()
    bad = 0
    for r, e in sorted(data.get("derived", {}).items()):
        p = os.path.join(ROOT, r)
        if not os.path.isfile(p):
            print("MANQUANT  ", r)
            bad += 1
        elif sha256(p) != e["sha256"]:
            print("MODIFIE   ", r)
            bad += 1
    for f in protected_files():
        if rel(f) not in data.get("derived", {}):
            print("NON ENREGISTRE", rel(f))
    src = os.path.join(ROOT, SOURCE_DIR)
    if os.path.isdir(src):
        for r, e in sorted(data.get("sources", {}).items()):
            p = os.path.join(ROOT, r)
            if not os.path.isfile(p):
                print("ORIGINAL ABSENT", r)
            elif sha256(p) != e["sha256"]:
                print("ORIGINAL MODIFIE", r)
                bad += 1
    else:
        print("(Tools/SourceModels absent : originaux non verifies)")
    print("OK" if bad == 0 else "%d probleme(s)" % bad)
    return bad


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    if cmd == "update":
        update()
    elif cmd == "list":
        for f in protected_files():
            print(rel(f))
    else:
        sys.exit(1 if check() else 0)
