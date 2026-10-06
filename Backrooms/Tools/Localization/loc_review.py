"""v4.12 : relecture ciblee des traductions (gauche/droite, identifiants de vannes et d'appareils, nombres, unites).

Usage :  python Tools/Localization/loc_review.py [--out FICHIER]
Ce controle ne juge pas le style : il signale les traductions ou un detail qui change le sens a disparu ou s'est
inverse :
 - gauche/droite : le mot de la langue pour le bon cote est present, celui du cote oppose absent ;
 - identifiants : lettres de vannes et d'appareils (A-E), V1-V9, chiffres et nombres du francais presents tels quels ;
 - unites : une distance en metres ({Meters} m, 8 m) garde une unite de longueur de la langue ; secondes et pourcentages
   gardent leur unite.
Code de sortie 1 si un probleme est trouve.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import loc_common as lc  # noqa: E402

LEFT = {
    'en': ['left'], 'de': ['link'], 'es-ES': ['izquierd'], 'es-419': ['izquierd'], 'it': ['sinistr'], 'pt-BR': ['esquerd'],
    'pt-PT': ['esquerd'], 'pl': ['lew'], 'cs': ['lev', 'vlevo'], 'hu': ['bal'], 'sv': ['vänster'], 'tr': ['sol'], 'ru': ['лев', 'влево', 'слева'],
    'uk': ['лів', 'вліво', 'ліворуч'], 'ar': ['يسار', 'يسرى'], 'fa': ['چپ'], 'id': ['kiri'], 'ja': ['左'], 'ko': ['왼'], 'zh-Hans': ['左'], 'zh-Hant': ['左'],
}
RIGHT = {
    'en': ['right'], 'de': ['recht'], 'es-ES': ['derech'], 'es-419': ['derech'], 'it': ['destr'], 'pt-BR': ['direit'], 'pt-PT': ['direit'],
    'pl': ['praw'], 'cs': ['prav', 'vpravo'], 'hu': ['jobb'], 'sv': ['höger'], 'tr': ['sağ'], 'ru': ['прав', 'вправо', 'справа'],
    'uk': ['прав', 'вправо', 'праворуч'], 'ar': ['يمين', 'يمنى'], 'fa': ['راست'], 'id': ['kanan'], 'ja': ['右'], 'ko': ['오른'], 'zh-Hans': ['右'], 'zh-Hant': ['右'],
}
METERS = {
    'ru': ['м'], 'uk': ['м'], 'ar': ['م', 'متر'], 'fa': ['متر', 'م'], 'zh-Hans': ['米', 'm'], 'zh-Hant': ['公尺', 'm'],
}
SECONDS = {
    'ru': ['с'], 'uk': ['с'], 'ar': ['ث', 'ثانية', 's'], 'fa': ['ثانیه', 's'], 'zh-Hans': ['秒', 's'], 'zh-Hant': ['秒', 's'],
    'ja': ['秒', 's'], 'ko': ['초', 's'], 'id': ['dtk', 's', 'detik'],
}


# Relus a la main (v4.12) : nombre ecrit en toutes lettres ou en numeration de la langue, sens conserve
ACCEPTED = {
    ('zh-Hans', 'Note.Common.1', '1'), ('zh-Hans', 'Note.Common.2', '2'), ('zh-Hans', 'Note.Common.3', '3'),
    ('zh-Hant', 'Note.Common.1', '1'), ('zh-Hant', 'Note.Common.2', '2'), ('zh-Hant', 'Note.Common.3', '3'),
    ('zh-Hans', 'Menu.HoundOrigine175000Sommets', '175'), ('zh-Hans', 'Menu.HoundOrigine175000Sommets', '000'),
    ('zh-Hant', 'Menu.HoundOrigine175000Sommets', '175'), ('zh-Hant', 'Menu.HoundOrigine175000Sommets', '000'),
    ('ko', 'Menu.HoundOrigine175000Sommets', '175'), ('ko', 'Menu.HoundOrigine175000Sommets', '000'),
    ('ja', 'Menu.HoundOrigine175000Sommets', '175'),
    ('ar', 'Item.CamescopeVhsAnnees90Accroche', '90'), ('ar', 'Note.L0.6', '3'), ('ar', 'Level.5.Description', '1920'),
    ('ar', 'Menu.6EmplacementsSontOccupesSupprimez', '6'),
    ('fa', 'HUD.AstuceFilmerEntiteCamescopePendant', '3'), ('fa', 'Note.L0.6', '3'),
    ('id', 'Menu.6EmplacementsSontOccupesSupprimez', '6'),
}


def strip_args(s):
    return re.sub(r'\{[^}]*\}', ' ', s)


def has_any(text, words):
    low = text.lower()
    return any(w.lower() in low for w in words)


def review():
    gathered, _ = lc.gather()
    problems = []
    checked = {'cote': 0, 'identifiants': 0, 'unites': 0}
    for code, _, _ in lc.CULTURES:
        if code == 'fr':
            continue
        _, entries = lc.read_po(os.path.join(lc.LOC, code, 'Game.po'))
        for key, (src, _) in gathered.items():
            e = entries.get(key)
            if not e or not e.translation:
                continue
            t = e.translation
            s = strip_args(src)
            # Gauche / droite
            bl = re.search(r'\bgauche\b', s, re.I) is not None
            br = re.search(r'\bdroite\b', s, re.I) is not None
            if bl or br:
                checked['cote'] += 1
                if bl and not has_any(t, LEFT[code]):
                    problems.append('%s %s : "gauche" absent : %s' % (code, key, t))
                if br and not has_any(t, RIGHT[code]):
                    problems.append('%s %s : "droite" absent : %s' % (code, key, t))
                if bl and not br and has_any(t, RIGHT[code]) and not has_any(t, LEFT[code]):
                    problems.append('%s %s : cote inverse : %s' % (code, key, t))
                if br and not bl and has_any(t, LEFT[code]) and not has_any(t, RIGHT[code]):
                    problems.append('%s %s : cote inverse : %s' % (code, key, t))
            # Identifiants et nombres (hors {arguments})
            # (une lettre suivie d'une apostrophe est une elision du francais : D'AMANDE, C'est)
            tokens = re.findall(r"(?<![\w'\u2019])(?:[A-E]|V[1-9]|\d+(?:[.,]\d+)?)(?![\w'\u2019])", s)
            # "A" en tete de phrase francaise n'est pas un identifiant (rare) ; les identifiants sont ailleurs en majuscule seule
            for tok in tokens:
                checked['identifiants'] += 1
                if tok not in strip_args(t):
                    # Nombres ecrits autrement (chiffres arabes-indiens, ideogrammes) : acceptes s'ils restent en chiffres
                    alt = tok.translate(str.maketrans('0123456789', '٠١٢٣٤٥٦٧٨٩'))
                    alt_fa = tok.translate(str.maketrans('0123456789', '۰۱۲۳۴۵۶۷۸۹'))
                    decade = len(tok) == 4 and tok.startswith('19') and tok[2:] in t  # "annees 1920" -> "anos 20"
                    if (code, key, tok) in ACCEPTED:
                        checked['acceptes'] = checked.get('acceptes', 0) + 1
                        continue
                    if alt not in t and alt_fa not in t and tok.replace('.', ',') not in t and not decade:
                        problems.append('%s %s : "%s" absent : %s' % (code, key, tok, t))
            # Unites
            if re.search(r'(\d|\})\s?m\b', src):
                checked['unites'] += 1
                if not has_any(t, METERS.get(code, ['m'])):
                    problems.append('%s %s : unite de longueur absente : %s' % (code, key, t))
            if re.search(r'(\d|\})\s?s\b', src):
                checked['unites'] += 1
                if not has_any(t, SECONDS.get(code, ['s', 'sec'])):
                    problems.append('%s %s : unite de temps absente : %s' % (code, key, t))
            if '%' in s and '%' not in t and '٪' not in t:
                problems.append('%s %s : pourcentage absent : %s' % (code, key, t))
    return problems, checked


def main():
    out = None
    if '--out' in sys.argv:
        out = sys.argv[sys.argv.index('--out') + 1]
    problems, checked = review()
    lines = ['Relecture ciblee des traductions (gauche/droite, identifiants, nombres, unites)',
             'verifications : cote %d, identifiants %d, unites %d ; nombres en toutes lettres relus a la main : %d'
             % (checked['cote'], checked['identifiants'], checked['unites'], checked.get('acceptes', 0)),
             'problemes : %d' % len(problems)] + ['  ' + p for p in problems]
    text = '\n'.join(lines) + '\n'
    if out:
        with open(out, 'w', encoding='utf-8') as fh:
            fh.write(text)
    print(text[:6000])
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
