"""v4.8 : planche HORS MOTEUR des 22 langues, rendue avec les polices livrees (Content/Fonts).

Ce n'est PAS une capture du jeu : elle montre que chaque texte trouve ses glyphes dans les polices du paquet, que
l'arabe et le persan sont mis en forme (lettres liees, droite a gauche) et que le chinois et le japonais se coupent
entre les caracteres. Les captures du jeu viennent du test automatique (-BRAutoTest -BRAutoTestV48, Saved/AutoTest).

La police du moteur (Roboto : latin, grec, cyrillique) n'est pas dans le depot ; DejaVu Sans la remplace ici.
La mise en forme utilise Pillow avec libraqm (HarfBuzz + FriBiDi, comme le moteur pour l'arabe).

Usage :  python Tools/Localization/render_language_sheet.py [sortie.png]
         (defaut : Docs/v48/planche_langues_hors_moteur.png)
"""
import os
import sys
import unicodedata

from PIL import Image, ImageDraw, ImageFont, features

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import loc_common as lc  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FONTS = os.path.join(ROOT, "Content", "Fonts")
LATIN = ["/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"]

# Noms natifs (les memes que BRLoc::Languages(), affiches tels quels)
NATIVE = {
    "fr": "Français", "en": "English", "de": "Deutsch", "es-ES": "Español (España)", "pt-BR": "Português (Brasil)",
    "ru": "Русский", "it": "Italiano", "tr": "Türkçe",
    "es-419": "Español (Latinoamérica)", "pl": "Polski", "zh-Hans": "简体中文",
    "uk": "Українська", "ar": "العربية",
    "ko": "한국어", "fa": "فارسی", "ja": "日本語", "hu": "Magyar",
    "cs": "Čeština", "pt-PT": "Português (Portugal)", "sv": "Svenska", "zh-Hant": "繁體中文",
    "id": "Bahasa Indonesia",
}
RTL = {"ar", "fa"}
NO_SPACES = {"zh-Hans", "zh-Hant", "ja"}
# Ne commencent jamais une ligne (regles de coupure du chinois et du japonais)
CLOSING = set("、。，．！？）：；」』】〉》)]!?.,:;")


def script_of(ch):
    cp = ord(ch)
    if 0x0600 <= cp <= 0x06FF or 0x0750 <= cp <= 0x077F or 0xFB50 <= cp <= 0xFEFF:
        return "Arabic"
    if 0xAC00 <= cp <= 0xD7AF or 0x1100 <= cp <= 0x11FF or 0x3130 <= cp <= 0x318F:
        return "KR"
    if 0x3040 <= cp <= 0x30FF:
        return "Kana"
    if 0x3400 <= cp <= 0x9FFF or 0x3000 <= cp <= 0x303F or 0xFF00 <= cp <= 0xFFEF or 0x3100 <= cp <= 0x312F:
        return "Han"
    return "Latin"


def font_file(script, culture, bold):
    """Meme choix que la police composite du jeu (BRFonts) : ideogrammes a la forme de la langue courante"""
    weight = "Bold" if bold else "Regular"
    if script == "Arabic":
        return os.path.join(FONTS, "NotoSansArabic-%s.ttf" % weight)
    if script == "KR":
        return os.path.join(FONTS, "NotoSansKR-%s.ttf" % weight)
    if script == "Kana":
        return os.path.join(FONTS, "NotoSansJP-%s.ttf" % weight)
    if script == "Han":
        family = {"ja": "JP", "zh-Hant": "TC"}.get(culture, "SC")
        return os.path.join(FONTS, "NotoSans%s-%s.ttf" % (family, weight))
    return LATIN[1 if bold else 0]


_cache = {}


def font(path, size):
    key = (path, size)
    if key not in _cache:
        _cache[key] = ImageFont.truetype(path, size, layout_engine=ImageFont.Layout.RAQM)
    return _cache[key]


def runs(text, culture):
    """Decoupe en suites de caracteres d'une meme police (espaces et chiffres suivent la suite en cours)"""
    out = []
    for ch in text:
        s = script_of(ch)
        neutral = ch.isspace() or unicodedata.category(ch)[0] in "PNZ"
        if out and (neutral or out[-1][0] == s or (culture in RTL and s == "Latin" and out[-1][0] == "Arabic")):
            out[-1][1] += ch
        else:
            out.append([s, ch])
    return out


def measure(text, culture, size, bold=False):
    if culture in RTL:
        f = font(font_file("Arabic", culture, bold), size)
        return f.getlength(text, direction="rtl")
    return sum(font(font_file(s, culture, bold), size).getlength(t) for s, t in runs(text, culture))


def draw_line(d, text, x, y, w, culture, size, fill, bold=False):
    if culture in RTL:
        # Ligne entiere mise en forme de droite a gauche, alignee a droite (comme DrawParagraph en arabe et en persan)
        f = font(font_file("Arabic", culture, bold), size)
        width = f.getlength(text, direction="rtl")
        d.text((x + w - width, y), text, font=f, fill=fill, direction="rtl")
        return
    for s, t in runs(text, culture):
        f = font(font_file(s, culture, bold), size)
        d.text((x, y), t, font=f, fill=fill)
        x += f.getlength(t)


def wrap(text, culture, size, max_w):
    """Coupure : entre les caracteres (chinois, japonais ; jamais devant une ponctuation fermante), sinon aux espaces"""
    if culture in NO_SPACES:
        units = []
        for ch in text:
            if units and (ch in CLOSING or ch.isspace()):
                units[-1] += ch
            else:
                units.append(ch)
        sep = ""
    else:
        units = text.split(" ")
        sep = " "
    lines, cur = [], ""
    for u in units:
        cand = cur + sep + u if cur else u
        if cur and measure(cand, culture, size) > max_w:
            lines.append(cur)
            cur = u
        else:
            cur = cand
    if cur:
        lines.append(cur)
    return lines


def main():
    if not features.check("raqm"):
        sys.exit("Pillow sans libraqm : la mise en forme de l'arabe et du persan ne peut pas etre rendue")
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Docs", "v48", "planche_langues_hors_moteur.png")
    cols, col_w, row_h, pad = 2, 860, 190, 28
    rows = (len(lc.CULTURES) + cols - 1) // cols
    img = Image.new("RGB", (cols * col_w + pad, 120 + rows * row_h + pad), (18, 17, 14))
    d = ImageDraw.Draw(img)
    draw_line(d, "22 langues : polices livrées (Content/Fonts), rendu HORS MOTEUR — pas une capture du jeu", pad, 24, 1600, "fr", 30,
              (240, 228, 170), True)
    draw_line(d, "Nom natif · titre de la page Langue · note « Note.Common.1 » coupée à 640 px. "
              "Latin et cyrillique : DejaVu Sans à la place de Roboto (police du moteur).", pad, 68, 1700, "fr", 20, (170, 165, 150))
    missing_total = 0
    for i, c in enumerate(lc.CULTURES):
        code = c[0]
        _, entries = lc.read_po(os.path.join(ROOT, "Content", "Localization", "Game", code, "Game.po"))

        def tr(key):
            e = entries[key]
            return e.translation or e.source

        x = pad + (i % cols) * col_w
        y = 120 + (i // cols) * row_h
        w = col_w - 2 * pad
        d.rounded_rectangle((x - 12, y - 8, x + w + 12, y + row_h - 20), 12, fill=(30, 28, 23))
        head = "%s  ·  %s" % (NATIVE[code], tr("HUD.LanguageTitle"))
        if code in RTL:
            draw_line(d, code, x, y, w, "fr", 18, (120, 115, 100))
        else:
            draw_line(d, code, x + w - measure(code, "fr", 18), y, w, "fr", 18, (120, 115, 100))
        draw_line(d, head, x, y, w, code, 30, (250, 240, 200), True)
        text = tr("Note.Common.1")
        for k, line in enumerate(wrap(text, code, 24, 640)[:4]):
            draw_line(d, line, x, y + 50 + k * 32, w if code in RTL else 640, code, 24, (215, 208, 185))
        # Glyphes absents des polices choisies (tofu) : comptes et signales
        for s, t in runs(head + text, code):
            path = font_file("Arabic" if code in RTL and s != "Han" else s, code, False)
            f = font(path, 24)
            for ch in set(t):
                if not ch.isspace() and f.getmask(ch).getbbox() is None and unicodedata.category(ch)[0] not in "ZC":
                    missing_total += 1
                    print("glyphe absent : %s U+%04X (%s)" % (code, ord(ch), os.path.basename(path)))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    img.save(out, optimize=True)
    print("planche ecrite : %s (%d glyphe(s) absent(s))" % (out, missing_total))


if __name__ == "__main__":
    main()
