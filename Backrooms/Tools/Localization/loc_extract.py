"""
Rassemble les textes du jeu (NSLOCTEXT du code C++) et met a jour les catalogues de traduction.

Usage :  python Tools/Localization/loc_extract.py
 - Source/Backrooms/Private/BRLocKeys.inl : liste des cles (recensement des textes sans traduction, en jeu) ;
 - Content/Localization/Game/<culture>/Game.po : nouvelles cles ajoutees (msgstr vide), cles disparues retirees,
   traductions gardees. Si le texte francais d'une cle a change, sa traduction est gardee, marquee "fuzzy" (a revoir),
   avec l'ancien texte source en commentaire (#| msgid).
Le francais (fr) est la langue source : son catalogue reprend les textes du code.
Equivalent Unreal (dans l'editeur) : Outils > Localization Dashboard > Gather Text, ou
  UnrealEditor-Cmd Backrooms.uproject -run=GatherText -config=Config/Localization/Game_Gather.ini
"""
import datetime
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import loc_common as lc  # noqa: E402


def main():
    gathered, errors = lc.gather()
    for e in errors:
        print("ERREUR :", e)
    if errors:
        sys.exit(1)
    # Liste des cles pour le jeu
    with open(lc.KEYS_INL, "w", encoding="utf-8", newline="\n") as f:
        f.write("// Genere par Tools/Localization/loc_extract.py : ne pas modifier a la main (%d cles du namespace BR)\n" % len(gathered))
        for key in sorted(gathered):
            f.write('TEXT("%s"),\n' % key)
    today = datetime.date.today().isoformat()
    for code, name, plural in lc.CULTURES:
        path = os.path.join(lc.LOC, code, "Game.po")
        _, old = lc.read_po(path)
        entries = {}
        added = changed = 0
        for key, (source, _) in gathered.items():
            e = old.get(key)
            if code == lc.NATIVE:
                entries[key] = lc.PoEntry(key, source, source)
                continue
            if e is None:
                entries[key] = lc.PoEntry(key, source, "")
                added += 1
            elif e.source != source and e.translation:
                entries[key] = lc.PoEntry(key, source, e.translation, True, e.source, e.comments)
                changed += 1
            else:
                e.source = source
                entries[key] = e
        removed = len([k for k in old if k not in gathered])
        lc.write_po(path, code, name, plural, entries, gathered, today)
        print("%-8s %4d cles  (+%d nouvelles, %d a revoir, -%d retirees)" % (code, len(gathered), added, changed, removed))
    print("Cles :", os.path.relpath(lc.KEYS_INL, lc.ROOT))


if __name__ == "__main__":
    main()
