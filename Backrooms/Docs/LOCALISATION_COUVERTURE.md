# Couverture des traductions

Fichier ecrit par `Tools/Localization/loc_build.py` le 2026-10-05 : ne pas modifier a la main.

Le jeu compte **676 textes** (31676 caracteres en francais, la langue source).

Deux mesures sont distinctes :

- **Traduits** : textes qui ont une traduction valide, compilee dans `Game.locres`. Le jeu les affiche.
- **Relus** : traductions verifiees par une personne qui parle la langue (entrees sans le marqueur `fuzzy` dans le catalogue `.po`).

Les traductions de la v4.8 ont ete **produites automatiquement** : aucune n'a encore ete relue par une personne qui parle la langue.
Le jeu le dit sur la page Langue. Une langue passe en « relue » dans le jeu quand son code est ajoute a `+ReviewedCultures=` dans `Config/DefaultGame.ini`.

| Langue | Code | Traduits | Relus | Manquants (francais affiche) | Refuses par les controles | A revoir (source changee) |
|---|---|---|---|---|---|---|
| Français | `fr` | 676 / 676 (100 %) | 676 | 0 | 0 | 0 |
| Anglais | `en` | 676 / 676 (100 %) | 0 | 0 | 0 | 0 |
| Allemand | `de` | 676 / 676 (100 %) | 0 | 0 | 0 | 0 |
| Espagnol (Espagne) | `es-ES` | 676 / 676 (100 %) | 0 | 0 | 0 | 0 |
| Portugais (Brésil) | `pt-BR` | 676 / 676 (100 %) | 0 | 0 | 0 | 0 |
| Russe | `ru` | 676 / 676 (100 %) | 0 | 0 | 0 | 0 |
| Italien | `it` | 676 / 676 (100 %) | 0 | 0 | 0 | 0 |
| Turc | `tr` | 0 / 676 (0 %) | 0 | 676 | 0 | 0 |
| Espagnol (Amérique latine) | `es-419` | 676 / 676 (100 %) | 0 | 0 | 0 | 0 |
| Polonais | `pl` | 0 / 676 (0 %) | 0 | 676 | 0 | 0 |
| Chinois simplifié | `zh-Hans` | 0 / 676 (0 %) | 0 | 676 | 0 | 0 |
| Ukrainien | `uk` | 0 / 676 (0 %) | 0 | 676 | 0 | 0 |
| Arabe | `ar` | 0 / 676 (0 %) | 0 | 676 | 0 | 0 |
| Coréen | `ko` | 0 / 676 (0 %) | 0 | 676 | 0 | 0 |
| Persan | `fa` | 0 / 676 (0 %) | 0 | 676 | 0 | 0 |
| Japonais | `ja` | 0 / 676 (0 %) | 0 | 676 | 0 | 0 |
| Hongrois | `hu` | 0 / 676 (0 %) | 0 | 676 | 0 | 0 |
| Tchèque | `cs` | 0 / 676 (0 %) | 0 | 676 | 0 | 0 |
| Portugais (Portugal) | `pt-PT` | 676 / 676 (100 %) | 0 | 0 | 0 | 0 |
| Suédois | `sv` | 0 / 676 (0 %) | 0 | 676 | 0 | 0 |
| Chinois traditionnel | `zh-Hant` | 0 / 676 (0 %) | 0 | 676 | 0 | 0 |
| Indonésien | `id` | 0 / 676 (0 %) | 0 | 676 | 0 | 0 |

## Ce qui n'est pas traduit

- **Voix et sons** : pas de doublage. Les sons du jeu n'ont pas de paroles a traduire ; le chat vocal transmet la voix des joueurs.
- **Noms propres et mentions** : noms des entites (Smiler, Hound...), niveaux nommes (Poolrooms), sigles techniques (VHS, RTX, DLSS, TSR), noms des langues (toujours dans leur propre ecriture), nom de la partie et nom des joueurs (texte saisi).
- **Journal du jeu** (`LogBackrooms`) : reste en francais, pour les rapports de bogues.
