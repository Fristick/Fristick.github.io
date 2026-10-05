"""
Localisation de Backrooms (v4.8) : fonctions communes aux outils de ce dossier.

Les textes du jeu sont ecrits dans le code C++ : NSLOCTEXT("BR", "<Cle>", "<Texte francais>"). Le francais est la langue source.
Les traductions vivent dans Content/Localization/Game/<culture>/Game.po (catalogues PO au format d'Unreal : msgctxt
"BR,<Cle>"), modifiables avec Poedit, Weblate, Crowdin ou un editeur de texte. Une entree marquee "fuzzy" est une traduction
produite automatiquement et pas encore relue par une personne qui parle la langue : elle est utilisee par le jeu, et le
rapport de couverture la compte a part des traductions relues.
"""
import io
import os
import re
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SOURCE = os.path.join(ROOT, "Source", "Backrooms")
LOC = os.path.join(ROOT, "Content", "Localization", "Game")
KEYS_INL = os.path.join(SOURCE, "Private", "BRLocKeys.inl")
NAMESPACE = "BR"
NATIVE = "fr"

# Les 22 langues, dans l'ordre du menu (BRLoc.cpp) : code Unreal, nom francais, formes du pluriel (en-tete PO, indicatif :
# le jeu utilise les categories ICU ecrites dans les textes, {Count}|plural(one=...,few=...,other=...))
CULTURES = [
    ("fr", "Fran\u00e7ais", "nplurals=2; plural=(n > 1);"),
    ("en", "Anglais", "nplurals=2; plural=(n != 1);"),
    ("de", "Allemand", "nplurals=2; plural=(n != 1);"),
    ("es-ES", "Espagnol (Espagne)", "nplurals=2; plural=(n != 1);"),
    ("pt-BR", "Portugais (Br\u00e9sil)", "nplurals=2; plural=(n > 1);"),
    ("ru", "Russe", "nplurals=3; plural=(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);"),
    ("it", "Italien", "nplurals=2; plural=(n != 1);"),
    ("tr", "Turc", "nplurals=2; plural=(n != 1);"),
    ("es-419", "Espagnol (Am\u00e9rique latine)", "nplurals=2; plural=(n != 1);"),
    ("pl", "Polonais", "nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);"),
    ("zh-Hans", "Chinois simplifi\u00e9", "nplurals=1; plural=0;"),
    ("uk", "Ukrainien", "nplurals=3; plural=(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);"),
    ("ar", "Arabe", "nplurals=6; plural=(n==0 ? 0 : n==1 ? 1 : n==2 ? 2 : n%100>=3 && n%100<=10 ? 3 : n%100>=11 ? 4 : 5);"),
    ("ko", "Cor\u00e9en", "nplurals=1; plural=0;"),
    ("fa", "Persan", "nplurals=2; plural=(n > 1);"),
    ("ja", "Japonais", "nplurals=1; plural=0;"),
    ("hu", "Hongrois", "nplurals=2; plural=(n != 1);"),
    ("cs", "Tch\u00e8que", "nplurals=3; plural=(n==1) ? 0 : (n>=2 && n<=4) ? 1 : 2;"),
    ("pt-PT", "Portugais (Portugal)", "nplurals=2; plural=(n != 1);"),
    ("sv", "Su\u00e9dois", "nplurals=2; plural=(n != 1);"),
    ("zh-Hant", "Chinois traditionnel", "nplurals=1; plural=0;"),
    ("id", "Indon\u00e9sien", "nplurals=1; plural=0;"),
]
CULTURE_CODES = [c[0] for c in CULTURES]

# Categories de pluriel ICU (CLDR) de chaque langue : un motif {Count}|plural(...) doit les couvrir (other suffit au minimum)
PLURAL_CATEGORIES = {
    "fr": ["one", "many", "other"], "en": ["one", "other"], "de": ["one", "other"], "es-ES": ["one", "many", "other"],
    "pt-BR": ["one", "many", "other"], "ru": ["one", "few", "many", "other"], "it": ["one", "many", "other"], "tr": ["one", "other"],
    "es-419": ["one", "many", "other"], "pl": ["one", "few", "many", "other"], "zh-Hans": ["other"], "uk": ["one", "few", "many", "other"],
    "ar": ["zero", "one", "two", "few", "many", "other"], "ko": ["other"], "fa": ["one", "other"], "ja": ["other"], "hu": ["one", "other"],
    "cs": ["one", "few", "many", "other"], "pt-PT": ["one", "many", "other"], "sv": ["one", "other"], "zh-Hant": ["other"], "id": ["other"],
}

_NSLOC = re.compile(r'NSLOCTEXT\(\s*"BR"\s*,\s*"([^"]+)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\)')


def _strip_comments(src):
    """Retire les commentaires C++ (en gardant les sauts de ligne pour les numeros de ligne) sans toucher aux chaines"""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '"':
            j = i + 1
            while j < n and src[j] != '"':
                j += 2 if src[j] == "\\" else 1
            out.append(src[i:j + 1])
            i = j + 1
        elif c == "'":
            j = i + 1
            while j < n and src[j] != "'":
                j += 2 if src[j] == "\\" else 1
            out.append(src[i:j + 1])
            i = j + 1
        elif src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in src[i:j]))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def unescape_cpp(s):
    """Litteral C++ (TEXT("...") ASCII avec \\uXXXX) -> texte"""
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            e = s[i + 1]
            if e == "u":
                out.append(chr(int(s[i + 2:i + 6], 16)))
                i += 6
                continue
            if e == "U":
                out.append(chr(int(s[i + 2:i + 10], 16)))
                i += 10
                continue
            out.append({"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\", "'": "'", "0": "\0"}.get(e, e))
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def gather():
    """Textes du jeu : {cle: (source, [emplacements])}, dans l'ordre des fichiers ; erreurs (meme cle, sources differentes)"""
    entries = {}
    errors = []
    files = []
    for base, _, names in os.walk(SOURCE):
        for name in names:
            if name.endswith((".cpp", ".h", ".inl")):
                files.append(os.path.join(base, name))
    for path in sorted(files):
        rel = os.path.relpath(path, ROOT).replace(os.sep, "/")
        with open(path, encoding="utf-8") as f:
            src = _strip_comments(f.read())
        for m in _NSLOC.finditer(src):
            key, text = m.group(1), unescape_cpp(m.group(2))
            line = src.count("\n", 0, m.start()) + 1
            where = "%s(%d)" % (rel, line)
            if key in entries:
                if entries[key][0] != text:
                    errors.append("%s : cle %s deja utilisee avec un autre texte (%s)" % (where, key, entries[key][1][0]))
                entries[key][1].append(where)
            else:
                entries[key] = (text, [where])
    return entries, errors


# ---------------------------------------------------------------------------------------------------------------- PO

def po_escape(s):
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t").replace("\r", "\\r")


def po_unescape(s):
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            out.append({"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\"}.get(s[i + 1], s[i + 1]))
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


class PoEntry:
    def __init__(self, key="", source="", translation="", fuzzy=False, previous=None, comments=None, refs=None):
        self.key = key
        self.source = source
        self.translation = translation
        self.fuzzy = fuzzy
        self.previous = previous        # ancien texte source (#| msgid) quand la source a change depuis la traduction
        self.comments = comments or []  # commentaires du traducteur (# ...)
        self.refs = refs or []


def read_po(path):
    """Catalogue PO -> (en-tete {champ: valeur}, {cle: PoEntry})"""
    header, entries = {}, {}
    if not os.path.exists(path):
        return header, entries
    with open(path, encoding="utf-8") as f:
        lines = f.read().splitlines()
    cur = None
    field = None
    block = {"comments": [], "flags": [], "refs": [], "prev": None}

    def flush():
        nonlocal cur, block
        if cur is not None:
            ctx = cur.get("msgctxt", "")
            if ctx == "" and cur.get("msgid", "") == "":
                for hl in cur.get("msgstr", "").split("\n"):
                    if ":" in hl:
                        k, v = hl.split(":", 1)
                        header[k.strip()] = v.strip()
            else:
                ns, _, key = ctx.partition(",")
                if ns == NAMESPACE and key:
                    entries[key] = PoEntry(key, cur.get("msgid", ""), cur.get("msgstr", ""), "fuzzy" in block["flags"], block["prev"],
                                           block["comments"], block["refs"])
        cur = None
        block = {"comments": [], "flags": [], "refs": [], "prev": None}

    for ln in lines + [""]:
        if not ln.strip():
            flush()
            field = None
            continue
        if ln.startswith("#,"):
            block["flags"] += [x.strip() for x in ln[2:].split(",")]
        elif ln.startswith("#|"):
            m = re.match(r'#\|\s*msgid\s+"(.*)"$', ln)
            if m:
                block["prev"] = po_unescape(m.group(1))
        elif ln.startswith("#:"):
            block["refs"] += ln[2:].split()
        elif ln.startswith("#."):
            pass
        elif ln.startswith("#"):
            block["comments"].append(ln[1:].strip())
        else:
            if cur is None:
                cur = {}
            m = re.match(r'(msgctxt|msgid|msgstr)\s+"(.*)"$', ln)
            if m:
                field = m.group(1)
                cur[field] = po_unescape(m.group(2))
            else:
                m = re.match(r'"(.*)"$', ln)
                if m and field:
                    cur[field] = cur.get(field, "") + po_unescape(m.group(1))
    return header, entries


def write_po(path, culture, native_name, plural_forms, entries, gathered, revision_date):
    """Ecrit un catalogue dans l'ordre des cles du jeu (gathered : sortie de gather())"""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    out = io.StringIO()
    out.write("# Backrooms : textes du jeu en %s (%s).\n" % (native_name, culture))
    out.write("# Source : francais (fr). Format Unreal : msgctxt \"BR,<Cle>\". Les {Arguments} et les motifs |plural(...) se gardent tels quels.\n")
    out.write("# Une entree \"fuzzy\" est une traduction produite automatiquement, pas encore relue par une personne qui parle la langue :\n")
    out.write("# le jeu l'utilise ; apres relecture, retirez \"fuzzy\" (Poedit : Besoin de travail). Puis : python Tools/Localization/loc_build.py\n")
    out.write('msgid ""\nmsgstr ""\n')
    for k, v in [("Project-Id-Version", "Backrooms"), ("PO-Revision-Date", revision_date), ("Language-Team", ""), ("Language", culture.replace("-", "_")),
                 ("MIME-Version", "1.0"), ("Content-Type", "text/plain; charset=UTF-8"), ("Content-Transfer-Encoding", "8bit"),
                 ("Plural-Forms", plural_forms)]:
        out.write('"%s: %s\\n"\n' % (k, v))
    for key, (source, refs) in gathered.items():
        e = entries.get(key)
        out.write("\n")
        if e:
            for c in e.comments:
                out.write("# %s\n" % c)
        out.write("#. Key:\t%s\n" % key)
        out.write("#. SourceLocation:\t%s\n" % refs[0])
        out.write("#: %s\n" % " ".join(r.replace(" ", "_") for r in refs[:4]))
        if e and e.fuzzy and e.translation:
            out.write("#, fuzzy\n")
        if e and e.previous is not None and e.previous != source:
            out.write('#| msgid "%s"\n' % po_escape(e.previous))
        out.write('msgctxt "%s,%s"\n' % (NAMESPACE, po_escape(key)))
        out.write('msgid "%s"\n' % po_escape(source))
        out.write('msgstr "%s"\n' % po_escape(e.translation if e else ""))
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(out.getvalue())


# -------------------------------------------------------------------------------------------- Ressources .locres

LOCRES_MAGIC = struct.pack("<4I", 0x7574140E, 0xFC034A67, 0x9D90154A, 0x1B7F37C3)
LOCMETA_MAGIC = struct.pack("<4I", 0xA14CEE4F, 0x83554868, 0xBD464C6C, 0x7C50DA70)
LOCRES_VERSION_OPTIMIZED_CRC32 = 2
LOCMETA_VERSION_COMPILED_CULTURES = 1


def str_crc32(s):
    """FCrc::StrCrc32 : CRC-32 de chaque unite UTF-16 etendue a 4 octets (FTextLocalizationResource::HashString)"""
    data = s.encode("utf-16-le")
    units = struct.unpack("<%dH" % (len(data) // 2), data)
    return zlib.crc32(struct.pack("<%dI" % len(units), *units)) & 0xFFFFFFFF


def fstring(s):
    """Serialisation d'un FString : longueur negative + UTF-16 si besoin, sinon latin-1 ; zero final compris"""
    if s == "":
        return struct.pack("<i", 0)
    if all(ord(c) < 128 for c in s):
        b = s.encode("ascii") + b"\0"
        return struct.pack("<i", len(b)) + b
    b = s.encode("utf-16-le") + b"\0\0"
    return struct.pack("<i", -(len(b) // 2)) + b


def write_locres(path, items):
    """items : [(namespace, cle, texte source, texte localise)] -> .locres (version 2, Optimized_CRC32)"""
    strings, index = [], {}
    by_ns = {}
    for ns, key, source, text in items:
        if text not in index:
            index[text] = len(strings)
            strings.append([text, 0])
        strings[index[text]][1] += 1
        by_ns.setdefault(ns, []).append((key, str_crc32(source), index[text]))
    body = io.BytesIO()
    body.write(struct.pack("<I", sum(len(v) for v in by_ns.values())))
    body.write(struct.pack("<I", len(by_ns)))
    for ns, keys in by_ns.items():
        body.write(struct.pack("<I", str_crc32(ns)))
        body.write(fstring(ns))
        body.write(struct.pack("<I", len(keys)))
        for key, src_hash, idx in keys:
            body.write(struct.pack("<I", str_crc32(key)))
            body.write(fstring(key))
            body.write(struct.pack("<Ii", src_hash, idx))
    head_len = len(LOCRES_MAGIC) + 1 + 8
    table_offset = head_len + len(body.getvalue())
    table = io.BytesIO()
    table.write(struct.pack("<i", len(strings)))
    for text, refs in strings:
        table.write(fstring(text))
        table.write(struct.pack("<i", refs))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(LOCRES_MAGIC)
        f.write(struct.pack("<B", LOCRES_VERSION_OPTIMIZED_CRC32))
        f.write(struct.pack("<q", table_offset))
        f.write(body.getvalue())
        f.write(table.getvalue())


def read_locres(path):
    """Relit un .locres ecrit par write_locres (verification) -> {(ns, cle): (hash source, texte)}"""
    with open(path, "rb") as f:
        data = f.read()
    pos = 0

    def take(fmt):
        nonlocal pos
        v = struct.unpack_from(fmt, data, pos)
        pos += struct.calcsize(fmt)
        return v

    def rstr():
        nonlocal pos
        (n,) = take("<i")
        if n == 0:
            return ""
        if n < 0:
            s = data[pos:pos + (-n) * 2].decode("utf-16-le")
            pos += (-n) * 2
            return s[:-1]
        s = data[pos:pos + n].decode("latin-1")
        pos += n
        return s[:-1]

    assert data[:16] == LOCRES_MAGIC, "magic"
    pos = 16
    (version,) = take("<B")
    assert version == LOCRES_VERSION_OPTIMIZED_CRC32
    (table_offset,) = take("<q")
    save = pos
    pos = table_offset
    (count,) = take("<i")
    strings = []
    for _ in range(count):
        s = rstr()
        take("<i")
        strings.append(s)
    pos = save
    take("<I")
    (ns_count,) = take("<I")
    out = {}
    for _ in range(ns_count):
        take("<I")
        ns = rstr()
        (key_count,) = take("<I")
        for _ in range(key_count):
            take("<I")
            key = rstr()
            src_hash, idx = take("<Ii")
            out[(ns, key)] = (src_hash, strings[idx])
    return out


def write_locmeta(path, native, cultures):
    with open(path, "wb") as f:
        f.write(LOCMETA_MAGIC)
        f.write(struct.pack("<B", LOCMETA_VERSION_COMPILED_CULTURES))
        f.write(fstring(native))
        f.write(fstring("%s/Game.locres" % native))
        f.write(struct.pack("<i", len(cultures)))
        for c in cultures:
            f.write(fstring(c))


# ------------------------------------------------------------------------------------------------ Verifications

_ARG = re.compile(r"\{([A-Za-z_][A-Za-z0-9_]*)\}")


def _strip_escapes(s):
    return re.sub(r"`.", "", s)


def arguments(s):
    """Arguments {Nom} d'un motif FText (y compris ceux des branches |plural(...)), hors accolades echappees par `"""
    return set(_ARG.findall(_strip_escapes(s))) | set(re.findall(r"\{([A-Za-z_][A-Za-z0-9_]*)\}\|(?:plural|ordinal|gender)", _strip_escapes(s)))


def plural_blocks(s):
    """[(argument, {categorie: texte})] des motifs {Arg}|plural(cat=texte,...)"""
    out = []
    t = _strip_escapes(s)
    for m in re.finditer(r"\{([A-Za-z_][A-Za-z0-9_]*)\}\|plural\(", t):
        depth, i = 1, m.end()
        while i < len(t) and depth:
            depth += {"(": 1, ")": -1}.get(t[i], 0)
            i += 1
        inner = t[m.end():i - 1]
        cats = {}
        for part in re.finditer(r"(zero|one|two|few|many|other|=\d+)=", inner):
            cats[part.group(1)] = True
        out.append((m.group(1), cats))
    return out


def check_translation(culture, source, translation):
    """Problemes d'une traduction (liste vide : bonne) ; une traduction fautive est remplacee par le francais a la compilation"""
    problems = []
    if not translation:
        return problems
    a, b = arguments(source), arguments(translation)
    if a != b:
        problems.append("arguments %s au lieu de %s" % (sorted(b), sorted(a)))
    t = _strip_escapes(translation)
    if t.count("{") != t.count("}"):
        problems.append("accolades non appariees")
    if t.count("(") < t.count(")") and "|plural(" in translation:
        problems.append("parentheses du pluriel non appariees")
    for arg, cats in plural_blocks(translation):
        if "other" not in cats:
            problems.append("pluriel {%s} sans 'other'" % arg)
    if source.startswith(" ") != translation.startswith(" ") or source.endswith(" ") != translation.endswith(" "):
        problems.append("espaces de debut ou de fin differents")
    return problems
