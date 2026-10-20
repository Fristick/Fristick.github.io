"""
v4.17 : remplit les traductions de cles donnees dans les catalogues Content/Localization/Game/<culture>/Game.po.

Usage :  python Tools/Localization/loc_fill.py <fichier.json> [<fichier.json> ...]
 - chaque fichier : {"<cle>": {"fr": "...", "<culture>": "<traduction>", ...}, ...} (le francais est ignore : la source
   est le texte du code, recopie par loc_extract.py) ;
 - a lancer APRES loc_extract.py (les cles doivent exister dans les catalogues) et AVANT loc_build.py ;
 - chaque traduction ecrite est marquee "fuzzy" (a relire par une personne qui parle la langue), comme les traductions
   produites automatiquement des versions precedentes ; l'ancien texte source garde son commentaire (#| msgid) ;
 - une cle absente d'un catalogue, ou une culture inconnue, est signalee (code de sortie 1), rien n'est ecrit pour elle.
Reprend l'outil hors depot de la v4.16 (Saved/TestRuns/po_fill.py), avec ces controles.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import loc_common as lc  # noqa: E402


def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    data = {}
    for path in sys.argv[1:]:
        with open(path, encoding="utf-8") as f:
            data.update(json.load(f))
    codes = [c for c, _, _ in lc.CULTURES]
    errors = 0
    for key, langs in data.items():
        for code in langs:
            if code not in codes:
                print("ERREUR : %s : culture inconnue %s" % (key, code))
                errors += 1
    filled = 0
    for code in codes:
        if code == "fr":
            continue
        path = os.path.join(lc.LOC, code, "Game.po")
        with open(path, encoding="utf-8") as f:
            lines = f.read().split("\n")
        seen = set()
        out = []
        i = 0
        while i < len(lines):
            line = lines[i]
            key = line[len('msgctxt "BR,'):-1] if line.startswith('msgctxt "BR,') else None
            if key in data and code in data[key] and i + 2 < len(lines) and lines[i + 2].startswith("msgstr "):
                if not (out and out[-1].startswith("#,") and "fuzzy" in out[-1]):
                    out.append("#, fuzzy")
                out.append(line)
                out.append(lines[i + 1])
                out.append('msgstr "%s"' % esc(data[key][code]))
                seen.add(key)
                filled += 1
                i += 3
                continue
            out.append(line)
            i += 1
        for key in data:
            if code in data[key] and key not in seen:
                print("ERREUR : %s : cle absente du catalogue %s (lancer loc_extract.py d'abord)" % (key, code))
                errors += 1
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(out))
    print("traductions ecrites : %d (%d cle(s)) ; erreurs : %d" % (filled, len(data), errors))
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
