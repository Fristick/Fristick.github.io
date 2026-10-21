# THE BACKROOMS v4.17 — registre des agents

Ce registre dit, pour chaque agent employé dans la v4.17 : son nom exact, le fichier qui le définit, ce qui lui a été
demandé, ce qu'il a réellement fait (lu, mesuré, écrit, commité), ce qu'il a vu, ce qui a été repris et par qui. Aucune
délégation n'est inventée : la liste vient du journal de la session (appels de l'outil Agent et messages de suite).

## 1. Provenance des agents

- **Dépôt** : `https://github.com/Hotragn/agency-agents` (fourche de `msitarzewski/agency-agents`), commit `35548a5`.
- **Installation** : 243 définitions copiées dans `~/.claude/agents/` (un fichier `.md` par agent, en-tête
  `name:` / `description:` puis le rôle en texte).
- **Audit avant usage** : aucun crochet (`hooks`), aucune permission, aucun outil imposé, aucun caractère Unicode caché dans
  les 243 fichiers. Une nouvelle session de la CLI 2.1.299 les liste (249 agents avec ceux d'origine).
- **Empreintes** (SHA-256, 12 premiers caractères) des définitions employées, relevées le 21 octobre 2026 :

| Agent (`name:`) | Fichier | Empreinte |
|---|---|---|
| Level Designer | `level-designer.md` | `79c1d41b7dfc` |
| Unreal Technical Artist | `unreal-technical-artist.md` | `54bdd46bc872` |
| Unreal Systems Engineer | `unreal-systems-engineer.md` | `ca34c6be64ee` |
| UI Designer | `design-ui-designer.md` | `3130768958d3` |
| Blender Add-on Engineer | `blender-addon-engineer.md` | `f6bf42625647` |
| Performance Benchmarker | `testing-performance-benchmarker.md` | `24dbd3a55cf6` |
| Technical Artist | `technical-artist.md` | `8a669de1fa34` |
| Code Reviewer | `engineering-code-reviewer.md` | `7509fcc3ea1d` |
| Evidence Collector | `testing-evidence-collector.md` | `d7e95b640332` |
| Accessibility Auditor | `testing-accessibility-auditor.md` | `982a14c0d0a9` |
| Reality Checker | `testing-reality-checker.md` | `748334839b21` |

## 2. Comment ils ont été employés (et la limite)

- **Émulation de rôle, pas agents natifs.** La session qui a mené la v4.17 avait été ouverte avant l'installation : son outil
  Agent ne proposait que les types intégrés. Chaque agent a donc été lancé comme **sous-agent générique** dont la consigne
  commençait par « lis en entier `~/.claude/agents/<fichier>.md` et adopte ce rôle ». Chaque rapport le
  dit dans son en-tête. Le rôle oriente la méthode et le livrable. Il ne donne aucune compétence ni aucun accès en plus :
  mêmes outils, même modèle que l'orchestrateur.
- **Ce que les agents n'ont pas fait.** Aucun n'a compilé ni lancé Unreal. La compilation, la réimportation, les bancs et le
  paquet reviennent à l'orchestrateur : un seul Unreal à la fois sur ce poste, et des mesures de performance sans charge
  parasite. Seuls le Blender Add-on Engineer (kit), l'Unreal Technical Artist (export des UV) et le UI Designer (rendu de la
  silhouette) ont lancé Blender en arrière-plan.
- **Images.** Un agent ne juge à l'image que ce qu'il a vu. Chaque rapport liste les images regardées. Quand un agent n'avait
  que du code ou des mesures, son rapport l'étiquette `[CODE]`, `[MESURÉ]` ou `[HYPOTHÈSE]`, sans verdict visuel.
- **Fichiers partagés, un seul propriétaire.** `BRAssets.*`, `BRMaterialBuilder.*`, `Content/Python/backrooms_setup.py`,
  `Tools/Build/essential_packages.txt`, `BRPlayerController.*`, le banc `BRAutoTestV417Visual.cpp` et, après la fusion du
  lot interface, `BRHUD.*` sont tenus par l'orchestrateur. Les agents qui écrivaient du code avaient leur propre arbre
  (`git worktree`) et leur propre branche. Ils ont livré des commits sur cette branche ou une liste de changements pour
  les fichiers partagés, que l'orchestrateur a intégrée.
- **Rien n'est poussé.** Toutes les branches sont locales.

## 3. Délégations (dans l'ordre)

| # | Date (UTC) | Agent | Mission | Où | Livrable | Images vues |
|---|---|---|---|---|---|---|
| 1 | 20/10 21:38 | Level Designer | Direction artistique et environnement : Level 0, Pitfalls, Poolrooms, Niveaux 1-2, joueur, interface ; 12 changements priorisés, critères mesurables | lecture seule | `Docs/v417/agents/direction_artistique.md` | oui (captures v4.16 `final416`, références Steam hors dépôt) |
| 2 | 20/10 21:38 | Unreal Technical Artist | Matériau de la combinaison et de la visière (§ 4.1), surfaces du Level 0 | lecture seule, puis prototype dans le dossier de travail | `materiaux_combinaison.md` § 0-7 | oui (§ en-tête du rapport) |
| 3 | 20/10 21:39 | Unreal Systems Engineer | Éclairage : Cinématique en mode 2, équilibre du Level 0 dans tous les profils, 152 paires occultées, essais en mouvement | lecture seule, mesures sur captures existantes | `eclairage.md` | mesures `[MESURÉ]` sur captures v4.16 |
| 4 | 20/10 21:39 | UI Designer | Inventaire, réglages, carnet : analyse | lecture seule | `interface.md` § 1-8 | oui (`v416vis`, `final416`) |
| 5 | 20/10 21:39 | Blender Add-on Engineer | Kit du Level 0 : chanfreins ciblés et normales (§ 4.2) | arbre propre, Blender en arrière-plan | `kit_blender.md`, `Tools/Blender/build_level0_kit.py` (`--no-bevels`, `--skip-textures`), `compare_level0_kit.py` | oui (captures du kit, 16 rendus avant/après) |
| 6 | 20/10 21:40 | Performance Benchmarker | Protocole avant/après (§ 5), scène « joueur et entités » | écrit `BRAutoTestV416Perf.cpp` (commité par l'orchestrateur, `4a9996a`) | `performances.md`, scènes `entites0` / `entites` | aucune (pas de verdict visuel) |
| 7 | 20/10 22:04 | UI Designer (suite) | Implémentation C1-C5, C8 (dessin), C10, C12 en partie ; puis C6, C7, C8 manette | `wt/ui`, branche `v417-ui` | commits `9d57ed8` … `93c7fb6`, `interface.md` § 9 | oui |
| 8 | 20/10 22:20 | Unreal Technical Artist (suite) | Cartes dérivées de la combinaison (normale, ORM), outils, tests | `wt/suit`, branche `v417-suit` | `d2049ec`, `ebe559a`, `bc79f2c`, `347adac`, `69fa579`, `30e728a` ; `materiaux_combinaison.md` § 8 | oui (cartes, aperçus) |
| 9 | 20/10 22:39 | Technical Artist | Animation procédurale et intégration des créatures : analyse | lecture seule | `animation.md` § 0-D | oui (vidéos du banc en planches) |
| 10 | 20/10 22:54 | Code Reviewer | Relecture de la branche `v417-jeu` | lecture seule | `relecture_code_jeu.md` (`17585a1`) | sans objet |
| 11 | 20/10 23:05 | Technical Artist (suite) | Implémentation : Bacteria posée au sol, corps du joueur, marcheurs, crochet de test | `wt/anim`, branche `v417-anim` | `f14dbf7`, `14b2a4c`, `f662494`, `1f3bc1e`, `animation.md` « Réalisation » | — |
| 12 | 21/10 02:27 | Evidence Collector | QA visuelle avant/après de tous les changements, sur les captures du banc | lecture seule | `qa_visuel.md` (`ba8c120`) | oui : 40 poses × 3 profils, 5 vidéos, plus de 60 recadrages |
| 13 | 21/10 02:32 | Accessibility Auditor | Audit du lot interface (C1-C12) dans fr, de, ru, ar, ja | lecture seule | `accessibilite.md` (`88b9ff0`) | oui (captures `v416vis`, `v417`) |
| 14 | 21/10 13:56 | Reality Checker | Revue finale de la livraison : affirmations, étiquettes, exigences du prompt, secrets | lecture seule, mesures refaites sur des images choisies | `Docs/v417/agents/revue_finale.md` | oui (planches, gros plans, images brutes des vidéos) |
| 15 | 21/10 14:25 | Code Reviewer (seconde relecture) | Relecture des lots que personne n'avait relus : animation, outils de la combinaison, corrections d'éclairage et d'accessibilité, crochets de test | lecture seule | `Docs/v417/agents/relecture_code_2.md` | sans objet |

Deux sous-agents ont été interrompus par une limite d'usage puis relancés par un message de suite (Unreal Technical
Artist, export des coutures ; Technical Artist, section « Réalisation »). Ils ont repris là où ils s'étaient arrêtés.

Hors v4.17 : un test « Game Designer » (`game-designer.md`) a servi à vérifier l'installation, sans rien produire dans le
dépôt.

## 4. Ce qui a été repris, et par qui

### 4.1 Réalisé par les agents (sur leur branche), fusionné par l'orchestrateur

- **Interface** (UI Designer, `v417-ui` fusionnée dans `0a5859d` puis `d8fa05d`) :
  - C1 échelle typographique, C2 traits d'un pixel, C3 cases lisibles, C4 silhouette rendue dans Blender ;
  - C5 INSPECTER et infobulle opaques, C8 défilement visible et zones à la manette, C10 casse correcte ;
  - C12 en partie, C6 et C7 côté HUD.
- **Combinaison** (Unreal Technical Artist, `v417-suit` fusionnée dans `42de87e`) : masques UV, cartes `T_Hazmat_Suit_N` et
  `_ORM`, `derive_hazmat_maps.py --check`, `test_material_sync.py` (9 tests), test v4.8 strict.
- **Animation** (Technical Artist, `v417-anim` fusionnée dans `c8004c6`) : Bacteria, corps du joueur, marcheurs, crochets
  `GetSupportPoints` et `GetBodyPoseInfo`.
- **Kit** (Blender Add-on Engineer) : script et comparaison. L'orchestrateur a régénéré les FBX (`e359095`) après la
  relecture, qui les avait trouvés absents.
- **Performances** (Performance Benchmarker) : scènes `entites0` et `entites`, hors liste par défaut (`4a9996a`).

### 4.2 Proposé par les agents, implémenté par l'orchestrateur (fichiers partagés)

| Proposition | Source | Commit |
|---|---|---|
| Image nette sans VHS (frange, saleté d'objectif, reflets, grain) | direction artistique, Code Reviewer | `768a2f0` |
| Caustiques attachées à l'eau (entrée `NZ`, murs, plafond flou) | direction artistique n° 2 | `768a2f0` |
| Plafond des Poolrooms moins clair, brume réduite | direction artistique n° 7 | `17f2e9b` |
| Rebond 0,8 et portée visible de 560 cm au Level 0 (réglages 1 et 3) | éclairage B.3 | `e881217` |
| Papier peint moins clair, relief adouci, joints des lés | direction artistique n° 1 et 11 | `0d87534` |
| Cadre des plafonniers en tôle peinte | direction artistique n° 8 | `1eebfc2` |
| Moins de flaques aux Niveaux 1 et 2 | direction artistique § 1.4 | `4f2a947` |
| `M_BR_Gear` et visière en verre diélectrique (C++ et Python) | Unreal Technical Artist § 4.1-4.3 | `5e3d710` |
| Corrections de la relecture : B1 cartes absentes, C1 six textes sans traduction, C2 actions sans `bTriggerWhenPaused`, C3 rugosité du papier peint | Code Reviewer | C2, C3 : `b79c8e7` ; B1 : fusion `42de87e` ; C1 : `eb3cc8f` |
| Banc figé, Bacteria filmée de près, marche vue de côté, captures CARNET et JOURNAL distinctes, poses VHS reposées | Evidence Collector § 1, § 2.11 | `de3645c` |
| Projecteur en Performance, volets 70°, diffuseurs × 0,72, exposition locale, bloom des Poolrooms (réglages 2, 4, 6, 8) | éclairage B.3, Evidence Collector § 3 | `3bb039b`, puis `ee3365e` (cône plein de 50°) et `2074046` (projecteur limité au Level 0 et aux Poolrooms) |
| Inventaire au clavier, jetons opaques, pastille de la barre rapide, pied d'INSPECTER, `HUD.PadColumn`, `FText::ToUpper` | Accessibility Auditor S1, M1, M3, m1, m2, m7 | `88b9ff0` |
| Contour des icônes sombres | Accessibility Auditor M2, Evidence Collector n° 9 | `3fa7ef5` |
| Version du jeu 4.17 | Evidence Collector n° 10 | `2a13965` |
| Entrée n'agit qu'avec le curseur du clavier montré (C1) ; infobulle de PERSONNAGE aux touches du clavier, Entrée et LAMPE (C3) ; gel du banc remis à zéro à la fin de tout essai (R4) ; deux noms masqués renommés dans les bancs (R7) | Code Reviewer (seconde relecture) | `f5231c5` |

### 4.3 Proposé, non repris (et pourquoi)

- **Éclairage, réglage 5** (courbe de compensation d'exposition) et **réglage 7** (AO courte portée en ray tracing matériel) :
  ils touchent l'exposition et le coût en Qualité. Ils sont reportés tant que le banc figé n'a pas mesuré les réglages
  déjà pris. Réglage 9 (bloom 0,5 au Level 0) : non pris, l'exposition locale traite déjà les diffuseurs.
- **Éclairage, partie C** (traitement ciblé des 152 paires) : non fait. Il demande un passage sur la trace de la règle de
  jeu, hors du périmètre « image seulement » de cette passe.
- **Direction artistique** :
  - n° 4 (bord des fosses) et n° 9 (porte de service distincte : le kit ne pose pas la porte en jeu) : non faits ;
  - n° 10 (usure localisée) et n° 12 (carrelage des Poolrooms) : non faits.
- **Interface** : C9 (arabe et persan de droite à gauche) et C11 (pied en pastilles, libellés de groupe horizontaux) : non
  faits. L'Accessibility Auditor les marque « non conformes ».
- **Accessibilité** :
  - M4 (42 textes raccourcis dans TOUCHES) : non fait ;
  - M5 (facteur d'échelle pour l'arabe et le persan) : non fait ;
  - M6 (aide des réglages sans défilement automatique) : non fait ;
  - m3 (répétition de la croix) : non fait.
- **Animation** : continuité des états, prise d'objet, lissage réseau du visuel des entités (priorités 4 à 6) : non faits.
- **Seconde relecture du code** :
  - C2 (onglets par Page préc. / Page suiv., catégories de PARAMÈTRES, Échap qui annule d'abord l'inspection ou le port) :
    non fait. Il faut de nouvelles touches actives pendant la pause et de nouveaux pieds à traduire dans les 22 langues ;
  - C1 est corrigé autrement que proposé : sans curseur montré, Entrée ne fait rien (comme en v4.16), au lieu de montrer
    le curseur ;
  - R1 (souris), R2 (lissage de la rotation chez un client), R3 (cadence des pas sonores), R5 (contrôle du contour des
    icônes), R6 (robustesse des outils de la combinaison) : non faits.

## 5. Revues croisées

| Revue | Portait sur | Constats principaux | Suite |
|---|---|---|---|
| Code Reviewer | `v417-jeu` (tête `93c7fb6`) | 1 bloquant (B1 cartes de la combinaison absentes de la branche), 3 à corriger (C1-C3), remarques | tout corrigé : C2 et C3 dans `b79c8e7`, B1 par la fusion `42de87e`, C1 dans `eb3cc8f` ; FBX du kit régénérés (`e359095`) |
| Accessibility Auditor | lot interface, captures `v416vis` et `v417` | 1 sérieux (clavier seul), 6 moyens, 7 mineurs ; C9 et C11 non faits | S1, M1, M2, M3, m1, m2, m7 corrigés (`88b9ff0`, `3fa7ef5`) ; M4-M6, m3 restent |
| Evidence Collector | captures avant/après des 3 profils | statut NEEDS WORK (B−) : banc non figé, animations non démontrées, diffuseurs brûlés, murs plus clairs que la moquette, Poolrooms laiteuses, icônes sous 3:1, version v4.16 | banc, éclairage, icônes et version corrigés (`de3645c` … `2a13965`) ; nouvelles captures dans `Docs/VISUELS_v4.17.md` |
| Reality Checker | livraison finale (tête `21bf02b`) | NEEDS WORK, C+ : 1 bloquant (paquet de `2074046` incomplet : exécutable de `e612818`, 13 fichiers non copiés), 2 erreurs du § 7, 25 affirmations inexactes ou trop fortes | § 6 |
| Code Reviewer (seconde relecture) | lots fusionnés après la première relecture : animation, outils de la combinaison, corrections d'éclairage et d'accessibilité, crochets de test (tête `21bf02b`) | aucun bloquant ; 3 à corriger, tous dans l'inventaire au clavier (`88b9ff0`) : **C1** Entrée agit sur un curseur caché (joueur à la souris), **C2** au clavier seul, ni onglet, ni catégorie de PARAMÈTRES, ni « retour » par Échap, **C3** infobulle de PERSONNAGE aux boutons de la manette ; 7 remarques (R1 la souris n'efface pas le curseur du clavier, R2 piétinement chez un client, R3 pas sonores, R4 gel du banc, R5 contour des icônes non contrôlé, R6 outils de la combinaison, R7 noms masqués) | C1, C3, R4 et R7 corrigés (`f5231c5`) et vérifiés par quatre appuis au clavier ajoutés à `v416vis` ; C2, R1-R3, R5 et R6 restent (`RAPPORT_v4.17.md` § 8.2) |

## 6. Revue finale (Reality Checker)

Rapport : `v417/agents/revue_finale.md`, sur la tête `21bf02b`. Statut **NEEDS WORK**, note C+ (environ 60/100) :
à peu près la moitié du prompt livrée et prouvée.

**Confirmé par ses propres mesures** :

- image nette (0,4-0,8 px → 0) ;
- diffuseurs de `l0_salle` (71-82 % → 0 en Qualité et en Cinématique) ;
- plafond délavé du Performance corrigé ; caustiques ;
- banc figé (41 sur 41) ; planches identiques aux captures brutes ;
- tableaux de performance exacts ; empreintes des définitions d'agents.

**Constats et suite donnée** :

| Constat | Suite |
|---|---|
| **Bloquant** : le « paquet de `2074046` » contenait l'exécutable de `e612818`. 13 fichiers non copiés, verrouillés par le jeu resté ouvert du banc d'interface | paquet refait sur machine propre, puis vérifié et retraversé (`RAPPORT_v4.17.md` § 6) |
| Le § 7 attribuait au code final des tests lancés sur l'avant-dernier ; un marqueur de travail était resté | tests Unreal relancés sur le code final ; § 7 réécrit |
| Diffuseur de `l0_dos` en Performance au-dessus de la cible ; résolutions internes fausses ; stabilité du banc (3,1 points, pas 0,2) ; queues p99, images de plus de 50 ms, VRAM, bassins en Cinématique | corrigé dans le rapport (§ 1, 4, 5) |
| Étiquettes trop fortes pour l'animation ([correction validée]) | ramenées à [jugé à l'image], avec les critères non tenus |
| Rallumage pris à 1,5 et 4 s, plus lent après ; bras en A du coéquipier chez le client ; aide aux parasites non vérifiée ; `protect_assets` sans les originaux | ajouté au rapport et à l'index visuel, comme défauts ou non-vérifiés |
| Non fait : 152 paires, variantes, matières des Niveaux 1-2, carnet qui défile, arabe | écrit « non fait », sans excuse de périmètre |
| Lots non relus | seconde relecture par le Code Reviewer (délégation 15, § 5) ; C1, C3, R4 et R7 corrigés dans `f5231c5`, puis paquet et tests refaits sur ce code |
| Chemins privés dans les documents | remplacés (`~/.claude/agents/`, `%USERPROFILE%`) ; aucun jeton ni adresse trouvés |
| `v417_criteria.py` plantait sur une sortie redirigée | sortie forcée en UTF-8 |
