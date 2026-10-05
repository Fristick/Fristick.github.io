"""
Compile les catalogues de traduction en ressources du jeu et ecrit le rapport de couverture.

Usage :  python Tools/Localization/loc_build.py [--check]
 - lit Content/Localization/Game/<culture>/Game.po ;
 - verifie chaque traduction (memes {Arguments} que le francais, pluriels avec 'other', accolades) : une traduction fautive
   n'est pas compilee, le jeu affiche alors le texte francais et le signale (journal LogBackrooms) ;
 - ecrit <culture>/Game.locres (format Unreal, version 2) et Game.locmeta ;
 - ecrit Docs/LOCALISATION_COUVERTURE.md : textes traduits, textes relus par une personne (entrees sans "fuzzy"),
   textes manquants ou refuses, par langue.
--check : verifie sans rien ecrire (code de sortie 1 en cas d'erreur).
Equivalent Unreal (dans l'editeur) : Localization Dashboard > Import Text (PO) puis Compile Text, ou
  UnrealEditor-Cmd Backrooms.uproject -run=GatherText -config=Config/Localization/Game_Import.ini;Config/Localization/Game_Compile.ini
"""
import datetime
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import loc_common as lc  # noqa: E402

REPORT = os.path.join(lc.ROOT, "Docs", "LOCALISATION_COUVERTURE.md")


def main():
    check_only = "--check" in sys.argv
    gathered, errors = lc.gather()
    if errors:
        for e in errors:
            print("ERREUR :", e)
        sys.exit(1)
    total = len(gathered)
    rows = []
    problems_all = {}
    for code, name, _ in lc.CULTURES:
        path = os.path.join(lc.LOC, code, "Game.po")
        header, entries = lc.read_po(path)
        items = []
        translated = reviewed = refused = stale = 0
        problems = []
        for key, (source, _) in gathered.items():
            e = entries.get(key)
            if code == lc.NATIVE:
                items.append((lc.NAMESPACE, key, source, source))
                continue
            if not e or not e.translation:
                continue
            if e.source != source:
                stale += 1
                continue
            issues = lc.check_translation(code, source, e.translation)
            if issues:
                refused += 1
                problems.append("%s : %s" % (key, "; ".join(issues)))
                continue
            items.append((lc.NAMESPACE, key, source, e.translation))
            translated += 1
            if not e.fuzzy:
                reviewed += 1
        if code == lc.NATIVE:
            translated = reviewed = total
        problems_all[code] = problems
        if not check_only:
            lc.write_locres(os.path.join(lc.LOC, code, "Game.locres"), items)
            back = lc.read_locres(os.path.join(lc.LOC, code, "Game.locres"))
            assert len(back) == len(items), code
        missing = total - translated - refused - stale
        rows.append((code, name, translated, reviewed, missing, refused, stale))
        print("%-8s traduits %4d/%d  relus %4d  manquants %4d  refuses %3d  a revoir %3d" % (code, translated, total, reviewed, missing, refused, stale))
        for p in problems[:10]:
            print("    refuse :", p)
    if check_only:
        sys.exit(1 if any(problems_all.values()) else 0)
    lc.write_locmeta(os.path.join(lc.LOC, "Game.locmeta"), lc.NATIVE, lc.CULTURE_CODES)
    write_report(rows, total, problems_all)
    with open(os.path.join(lc.LOC, "coverage.json"), "w", encoding="utf-8") as f:
        json.dump({"total": total, "cultures": {r[0]: {"translated": r[2], "reviewed": r[3], "missing": r[4], "refused": r[5], "stale": r[6]} for r in rows}},
                  f, indent=1, ensure_ascii=False)
    print("Rapport :", os.path.relpath(REPORT, lc.ROOT))


def write_report(rows, total, problems):
    chars = {}
    gathered, _ = lc.gather()
    source_chars = sum(len(s) for s, _ in gathered.values())
    with open(REPORT, "w", encoding="utf-8", newline="\n") as f:
        f.write("# Couverture des traductions\n\n")
        f.write("Fichier ecrit par `Tools/Localization/loc_build.py` le %s : ne pas modifier a la main.\n\n" % datetime.date.today().isoformat())
        f.write("Le jeu compte **%d textes** (%d caracteres en francais, la langue source).\n\n" % (total, source_chars))
        f.write("Deux mesures sont distinctes :\n\n")
        f.write("- **Traduits** : textes qui ont une traduction valide, compilee dans `Game.locres`. Le jeu les affiche.\n")
        f.write("- **Relus** : traductions verifiees par une personne qui parle la langue (entrees sans le marqueur `fuzzy` dans le catalogue `.po`).\n\n")
        f.write("Les traductions de la v4.8 ont ete **produites automatiquement** : aucune n'a encore ete relue par une personne qui parle la langue.\n")
        f.write("Le jeu le dit sur la page Langue. Une langue passe en « relue » dans le jeu quand son code est ajoute a `+ReviewedCultures=` dans `Config/DefaultGame.ini`.\n\n")
        f.write("| Langue | Code | Traduits | Relus | Manquants (francais affiche) | Refuses par les controles | A revoir (source changee) |\n")
        f.write("|---|---|---|---|---|---|---|\n")
        for code, name, tr, rv, miss, ref, stale in rows:
            pct = 100.0 * tr / total if total else 0
            f.write("| %s | `%s` | %d / %d (%.0f %%) | %d | %d | %d | %d |\n" % (name, code, tr, total, pct, rv, miss, ref, stale))
        bad = {c: p for c, p in problems.items() if p}
        if bad:
            f.write("\n## Traductions refusees\n\n")
            for c, p in bad.items():
                f.write("- `%s` :\n" % c)
                for line in p:
                    f.write("  - %s\n" % line)
        f.write("\n## Ce qui n'est pas traduit\n\n")
        f.write("- **Voix et sons** : pas de doublage. Les sons du jeu n'ont pas de paroles a traduire ; le chat vocal transmet la voix des joueurs.\n")
        f.write("- **Noms propres et mentions** : noms des entites (Smiler, Hound...), niveaux nommes (Poolrooms), sigles techniques (VHS, RTX, DLSS, TSR), ")
        f.write("noms des langues (toujours dans leur propre ecriture), nom de la partie et nom des joueurs (texte saisi).\n")
        f.write("- **Journal du jeu** (`LogBackrooms`) : reste en francais, pour les rapports de bogues.\n")


if __name__ == "__main__":
    main()
