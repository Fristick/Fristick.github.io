"""
Polices des ecritures non latines (v4.8) : Noto Sans Arabic, Noto Sans SC / TC / JP / KR, en Regular et Bold.

Usage :  python Tools/Localization/build_fonts.py [dossier_des_polices_completes]
 - telecharge les polices completes depuis Google Fonts (API CSS2, fichiers TTF statiques) si le dossier ne les contient pas ;
 - les reduit (fontTools) aux caracteres utiles : tous les textes des catalogues Content/Localization/Game/*/Game.po,
   les noms natifs des langues (BRLoc.cpp), plus un jeu courant par ecriture pour les noms saisis par les joueurs
   (GB 2312 pour le chinois simplifie, Big5 courant pour le traditionnel, JIS X 0208 pour le japonais,
   KS X 1001 pour les syllabes coreennes, blocs arabes complets) ;
 - garde toutes les fonctions OpenType (liaisons et formes contextuelles de l'arabe et du persan) ;
 - ecrit Content/Fonts/*.ttf, OFL.txt (licence) et FONTS.md (sources, tailles, empreintes).
A relancer apres chaque nouvelle traduction (nouveaux ideogrammes) : python Tools/Localization/build_fonts.py
Prerequis : pip install fonttools brotli
"""
import glob
import hashlib
import io
import os
import re
import sys
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import loc_common as lc  # noqa: E402

try:
    from fontTools import subset
    from fontTools.ttLib import TTFont
except ImportError:
    sys.exit("fontTools manquant : pip install fonttools brotli")

OUT = os.path.join(lc.ROOT, "Content", "Fonts")
CACHE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(lc.HERE, ".fonts_cache")
CSS = "https://fonts.googleapis.com/css2?family=Noto+Sans+SC:wght@400;700&family=Noto+Sans+TC:wght@400;700&family=Noto+Sans+JP:wght@400;700&family=Noto+Sans+KR:wght@400;700&family=Noto+Sans+Arabic:wght@400;700"
FAMILIES = ["NotoSansSC", "NotoSansTC", "NotoSansJP", "NotoSansKR", "NotoSansArabic"]


def download():
    os.makedirs(CACHE, exist_ok=True)
    if all(os.path.exists(os.path.join(CACHE, "%s-%s.ttf" % (f, w))) for f in FAMILIES for w in ("Regular", "Bold")):
        return
    print("Telechargement des polices completes (Google Fonts) ->", CACHE)
    css = urllib.request.urlopen(urllib.request.Request(CSS, headers={"User-Agent": "curl/8"}), timeout=60).read().decode()
    for fam, weight, url in re.findall(r"font-family: '([^']+)';\s*font-style: normal;\s*font-weight: (\d+);(?:\s*font-stretch: normal;)?\s*src: url\(([^)]+)\)", css):
        name = "%s-%s.ttf" % (fam.replace(" ", ""), "Regular" if weight == "400" else "Bold")
        with urllib.request.urlopen(url, timeout=300) as r, open(os.path.join(CACHE, name), "wb") as f:
            f.write(r.read())
        print("  ", name)


def codec_chars(enc, lead, trail):
    out = set()
    for a in lead:
        for b in trail:
            try:
                c = bytes([a, b]).decode(enc)
            except UnicodeDecodeError:
                continue
            if len(c) == 1:
                out.add(ord(c))
    return out


def catalog_chars(cultures):
    chars = set()
    for c in cultures:
        _, entries = lc.read_po(os.path.join(lc.LOC, c, "Game.po"))
        for e in entries.values():
            chars.update(ord(ch) for ch in e.translation)
    return chars


def native_names():
    with open(os.path.join(lc.SOURCE, "Private", "BRLoc.cpp"), encoding="utf-8") as f:
        src = f.read()
    block = src[src.index("Languages()"):src.index("return List;")]
    return {ord(ch) for lit in re.findall(r'TEXT\("((?:[^"\\]|\\.)*)"\)', block) for ch in lc.unescape_cpp(lit)}


def rng(a, b):
    return set(range(a, b + 1))


def main():
    download()
    os.makedirs(OUT, exist_ok=True)
    common = rng(0x20, 0x7E) | rng(0x3000, 0x303F) | rng(0xFF00, 0xFFEF) | {0x00B7, 0x2026, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x00D7, 0x2192}
    names = native_names()
    han_all = catalog_chars(["zh-Hans", "zh-Hant", "ja", "ko"])
    sets = {
        "NotoSansSC": codec_chars("gb2312", range(0xA1, 0xF8), range(0xA1, 0xFF)) | catalog_chars(["zh-Hans"]) | han_all | names | common,
        "NotoSansTC": codec_chars("big5", range(0xA4, 0xC7), list(range(0x40, 0x7F)) + list(range(0xA1, 0xFF))) | catalog_chars(["zh-Hant"]) | names | common | rng(0x3100, 0x312F),
        "NotoSansJP": codec_chars("euc_jp", range(0xA1, 0xF5), range(0xA1, 0xFF)) | catalog_chars(["ja"]) | names | common | rng(0x3040, 0x30FF) | rng(0x31F0, 0x31FF) | rng(0xFF65, 0xFF9F),
        "NotoSansKR": codec_chars("euc_kr", range(0xB0, 0xC9), range(0xA1, 0xFF)) | catalog_chars(["ko"]) | names | common | rng(0x1100, 0x11FF) | rng(0x3130, 0x318F),
        "NotoSansArabic": rng(0x0600, 0x06FF) | rng(0x0750, 0x077F) | rng(0x08A0, 0x08FF) | rng(0xFB50, 0xFDFF) | rng(0xFE70, 0xFEFF) | rng(0x200C, 0x200F) | catalog_chars(["ar", "fa"]) | names | rng(0x20, 0x7E),
    }
    report = []
    for fam in FAMILIES:
        for weight in ("Regular", "Bold"):
            src = os.path.join(CACHE, "%s-%s.ttf" % (fam, weight))
            font = TTFont(src)
            cmap = font.getBestCmap()
            wanted = sets[fam]
            missing = sorted(c for c in wanted if c not in cmap and c > 0x7E and not (0x3000 <= c <= 0x303F or 0xFF00 <= c <= 0xFFEF))
            opt = subset.Options()
            opt.layout_features = ["*"]
            opt.name_IDs = ["*"]
            opt.name_languages = ["*"]
            opt.notdef_outline = True
            opt.hinting = True
            opt.glyph_names = False
            sub = subset.Subsetter(opt)
            sub.populate(unicodes=sorted(c for c in wanted if c in cmap))
            sub.subset(font)
            dst = os.path.join(OUT, "%s-%s.ttf" % (fam, weight))
            font.save(dst)
            data = open(dst, "rb").read()
            version = font["name"].getDebugName(5) or "?"
            copyright_ = font["name"].getDebugName(0) or ""
            if fam in ("NotoSansTC", "NotoSansJP"):
                # Polices propres a une langue : un ideogramme des noms natifs absent doit etre exclu de leurs plages (BRFonts.cpp)
                holes = [c for c in names if 0x3400 <= c <= 0x9FFF and c not in font.getBestCmap() and c != 0x7B80]
                if holes:
                    print("  ATTENTION %s : %s absents, a exclure des plages dans BRFonts.cpp (Without)" % (fam, " ".join("U+%04X" % c for c in holes)))
            report.append((os.path.basename(dst), len(font.getBestCmap()), len(data), hashlib.sha256(data).hexdigest()[:16], version, copyright_, len(missing)))
            print("%-30s %6d caracteres  %7.2f Mo  (absents de la police source : %d)" % (os.path.basename(dst), len(font.getBestCmap()), len(data) / 1e6, len(missing)))
    write_license(report)


OFL = """Copyright notices of the fonts in this folder:
{notices}

This Font Software is licensed under the SIL Open Font License, Version 1.1.
This license is copied below, and is also available with a FAQ at: https://openfontlicense.org

-----------------------------------------------------------
SIL OPEN FONT LICENSE Version 1.1 - 26 February 2007
-----------------------------------------------------------

PREAMBLE
The goals of the Open Font License (OFL) are to stimulate worldwide development of collaborative font projects, to support the font creation efforts of academic and linguistic communities, and to provide a free and open framework in which fonts may be shared and improved in partnership with others.

The OFL allows the licensed fonts to be used, studied, modified and redistributed freely as long as they are not sold by themselves. The fonts, including any derivative works, can be bundled, embedded, redistributed and/or sold with any software provided that any reserved names are not used by derivative works. The fonts and derivatives, however, cannot be released under any other type of license. The requirement for fonts to remain under this license does not apply to any document created using the fonts or their derivatives.

DEFINITIONS
"Font Software" refers to the set of files released by the Copyright Holder(s) under this license and clearly marked as such. This may include source files, build scripts and documentation.

"Reserved Font Name" refers to any names specified as such after the copyright statement(s).

"Original Version" refers to the collection of Font Software components as distributed by the Copyright Holder(s).

"Modified Version" refers to any derivative made by adding to, deleting, or substituting -- in part or in whole -- any of the components of the Original Version, by changing formats or by porting the Font Software to a new environment.

"Author" refers to any designer, engineer, programmer, technical writer or other person who contributed to the Font Software.

PERMISSION & CONDITIONS
Permission is hereby granted, free of charge, to any person obtaining a copy of the Font Software, to use, study, copy, merge, embed, modify, redistribute, and sell modified and unmodified copies of the Font Software, subject to the following conditions:

1) Neither the Font Software nor any of its individual components, in Original or Modified Versions, may be sold by itself.

2) Original or Modified Versions of the Font Software may be bundled, redistributed and/or sold with any software, provided that each copy contains the above copyright notice and this license. These can be included either as stand-alone text files, human-readable headers or in the appropriate machine-readable metadata fields within text or binary files as long as those fields can be easily viewed by the user.

3) No Modified Version of the Font Software may use the Reserved Font Name(s) unless explicit written permission is granted by the corresponding Copyright Holder. This restriction only applies to the primary font name as presented to the users.

4) The name(s) of the Copyright Holder(s) and the Author(s) of the Font Software shall not be used to promote, endorse or advertise any Modified Version, except to acknowledge the contribution(s) of the Copyright Holder(s) and the Author(s) or with their explicit written permission.

5) The Font Software, modified or unmodified, in part or in whole, must be distributed entirely under this license, and must not be distributed under any other license. The requirement for fonts to remain under this license does not apply to any document created using the Font Software.

TERMINATION
This license becomes null and void if any of the above conditions are not met.

DISCLAIMER
THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM OTHER DEALINGS IN THE FONT SOFTWARE.
"""


def write_license(report):
    notices = sorted({r[5] for r in report if r[5]})
    with open(os.path.join(OUT, "OFL.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write(OFL.format(notices="\n".join("- " + n for n in notices)))
    with open(os.path.join(OUT, "FONTS.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# Polices des ecritures non latines\n\n")
        f.write("Fichiers ecrits par `Tools/Localization/build_fonts.py` : sous-ensembles (Modified Versions au sens de l'OFL) des polices\n")
        f.write("Noto de Google Fonts, licence SIL Open Font License 1.1 (`OFL.txt`). Le jeu les ajoute comme sous-polices a la police\n")
        f.write("de l'interface (`BRFonts.cpp`). Pour ajouter des caracteres, relancer l'outil.\n\n")
        f.write("| Fichier | Caracteres | Taille | SHA-256 (16) | Version source |\n|---|---|---|---|---|\n")
        for name, chars, size, sha, version, _, _ in report:
            f.write("| %s | %d | %.2f Mo | `%s` | %s |\n" % (name, chars, size / 1e6, sha, version))


if __name__ == "__main__":
    main()
