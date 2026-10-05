# Couverture des traductions

Fichier ecrit par `Tools/Localization/loc_build.py` le 2026-10-05 : ne pas modifier a la main.

Le jeu compte **724 textes** (34596 caracteres en francais, la langue source).

Deux mesures sont distinctes :

- **Traduits** : textes qui ont une traduction valide, compilee dans `Game.locres`. Le jeu les affiche.
- **Relus** : traductions verifiees par une personne qui parle la langue (entrees sans le marqueur `fuzzy` dans le catalogue `.po`).

Les traductions de la v4.8 ont ete **produites automatiquement** : aucune n'a encore ete relue par une personne qui parle la langue.
Le jeu le dit sur la page Langue. Une langue passe en « relue » dans le jeu quand son code est ajoute a `+ReviewedCultures=` dans `Config/DefaultGame.ini`.

| Langue | Code | Traduits | Relus | Manquants (francais affiche) | Refuses par les controles | A revoir (source changee) |
|---|---|---|---|---|---|---|
| Français | `fr` | 724 / 724 (100 %) | 724 | 0 | 0 | 0 |
| Anglais | `en` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Allemand | `de` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Espagnol (Espagne) | `es-ES` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Portugais (Brésil) | `pt-BR` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Russe | `ru` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Italien | `it` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Turc | `tr` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Espagnol (Amérique latine) | `es-419` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Polonais | `pl` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Chinois simplifié | `zh-Hans` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Ukrainien | `uk` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Arabe | `ar` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Coréen | `ko` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Persan | `fa` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Japonais | `ja` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Hongrois | `hu` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Tchèque | `cs` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Portugais (Portugal) | `pt-PT` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Suédois | `sv` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Chinois traditionnel | `zh-Hant` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |
| Indonésien | `id` | 724 / 724 (100 %) | 0 | 0 | 0 | 0 |

## Ce qui n'est pas traduit

- **Voix et sons** : pas de doublage. Les sons du jeu n'ont pas de paroles a traduire ; le chat vocal transmet la voix des joueurs.
- **Noms propres et mentions** : noms des entites (Smiler, Hound...), niveaux nommes (Poolrooms), sigles techniques (VHS, RTX, DLSS, TSR), noms des langues (toujours dans leur propre ecriture), nom de la partie et nom des joueurs (texte saisi).
- **Journal du jeu** (`LogBackrooms`) : reste en francais, pour les rapports de bogues.
