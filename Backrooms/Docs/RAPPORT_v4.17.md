# THE BACKROOMS v4.17 — passe graphique (Level 0, Poolrooms, Niveaux 1-2), agents installés

Branche **`backrooms-v4.17`**, partie de `c07eddb` (tête de `backrooms-v4.16` auditée le 20 octobre 2026). Rien n'est
poussé, rien n'est publié sur Steam. Machine de tous les essais : portable Windows 11, RTX 5050 Laptop, fenêtre de
1366 × 768 (écran 1920 × 1200 à 125 %), Unreal 5.8.3. La machine de référence (RTX 4080 + Ryzen 7 7800X3D) n'est pas
disponible : sa cible est **non vérifiée**.

Étiquettes employées :

- **[défaut reproduit]** : vu sur une capture ou mesuré dans le jeu avant correction ;
- **[constat de source]** : lu dans le code ou les ressources, effet en jeu non mesuré ;
- **[jugé à l'image]** : amélioration artistique regardée sur les captures avant/après ;
- **[correction validée]** : mesurée par un banc ou un test après correction ;
- **[hypothèse]** : à vérifier.

Documents liés : [`AGENTS_v4.17.md`](AGENTS_v4.17.md) (agents, délégations, revues),
[`VISUELS_v4.17.md`](VISUELS_v4.17.md) (index des preuves, `Docs/v417/visuels/`), rapports des agents dans
[`v417/agents/`](v417/agents/), [`GAMEPLAY_v4.17.md`](GAMEPLAY_v4.17.md) (ce qui change pour le joueur).

## 1. Résumé

**Ce qui change à l'image** (banc figé, avant/après, trois profils ; preuves dans `VISUELS_v4.17.md`) :

- **Image nette** : sans VHS, plus d'aberration (0,2-0,8 px → 0), de saleté ni de reflets d'objectif ; sans grain, plus
  de grain. Le mode caméscope ne change pas.
- **Level 0** :
  - diffuseurs non écrêtés dans les trois profils (≥ 250/255 : 71-83 % → 0-1,4 %) ;
  - haut des murs et coins plus sombres ; papier peint moins clair et moins vert, lés visibles ; cadres en tôle peinte ;
    chanfreins des plinthes ;
  - en Performance, plus de plafond délavé : tiers haut 0,40 → 0,18, Qualité 0,24.
- **Poolrooms** : caustiques seulement sous l'eau et près d'elle ; plafond un peu moins clair.
- **Combinaison** : matériau dédié `M_BR_Gear` (normale et ORM dérivées), visière en verre. Plis brillants sous une
  lumière froide, gain modeste sous le néon chaud. Vérifiée chez l'hôte et chez un client.
- **Animation** :
  - pieds du joueur posés au sol (cheville à 6,6 cm au lieu de 11,6) ; glissement par appui divisé par 2,5 ;
  - Bacteria posée, avec bonds de rattrapage.
- **Inventaire** :
  - textes plus grands ; icônes sombres cernées ; touches de la barre rapide en pastille ;
  - utilisable à la manette (réglages, déplacement d'objet) et au **clavier seul** ;
  - 22 langues, 1222 clés sur 1222.

**Ce qui reste** (§ 8) :

- murs du Level 0 encore plus clairs que la moquette (1,17 à 1,84 pour ≤ 1) ;
- voile des Poolrooms (p1 ≈ 0,05 pour ≤ 0,01) ;
- taches sombres des flaques du Niveau 1 ;
- pied d'appui qui glisse encore (16 cm par appui) ; bond de 58° de la Bacteria au demi-tour ;
- arabe affiché de gauche à droite ;
- aucun carnet réel assez long pour défiler ;
- **avertissement de photosensibilité** absent : décision du propriétaire.

**Contrat de jeu** (12 niveaux, ordre, chapitre public, Level 0 sans objectifs, Level 6, solo et coopération, 22 langues,
règle de lumière) : inchangé, contrôlé par les tests du jeu (§ 7).

**Performances** sur la machine de test (RTX 5050 Laptop) : aucune régression mesurable sur 7 scènes × 4 profils × 2 graines, en deux tours alternés (médianes de −3,6 % à +1,3 %) ; Performance de −2 à −4 % au Level 0. La cible RTX 4080 est **non vérifiée**.

**Paquet** : Shipping Windows du dernier commit de code (`2074046`) construit, vérifié et lancé ; premier chapitre traversé
(0 → 1 → 37 → 2, deux redémarrages). Modes d'affichage non vérifiés dans le paquet. Linux et macOS : rien construit.

**Agents** : 11 agents installés employés par émulation de rôle (sous-agents génériques), pour 14 délégations, dont 3
revues croisées et une revue finale (`AGENTS_v4.17.md`).

## 2. Méthode : banc visuel figé, mesures et revues

- **Banc** `-BRAutoTestV417Visual` (`Source/Backrooms/Private/BRAutoTestV417Visual.cpp`, lanceur
  `Tools/Tests/run_v417_visual.ps1`) :
  - 41 poses fixes et 6 vidéos à pas fixe de 1/30 s (Level 0, Poolrooms, Niveaux 1 et 2), dans les profils Qualité,
    Cinématique et Performance ;
  - une fiche `.json` par capture et `Captures.csv` : commit, niveau, graines, caméra, champ, résolutions de sortie et
    interne, profil, temps depuis l'arrivée, alimentation, coupure, lampe, MegaLights, 3e personne, VHS, grain, interface,
    tension, frayeur, nombre d'entités, mesures propres à l'image (appuis de la Bacteria, pose du corps du joueur).
- **Banc figé** (`ABRWorld::bTestFreeze`, `de3645c`) [correction validée] :
  - la QA visuelle avait montré que tension, Smilers, sursauts et néons changeaient d'une exécution à l'autre, ce qui
    rendait 9 poses non comparables ;
  - le banc est maintenant figé : ni rencontre, ni phénomène, ni coupure tirée, néons tenus allumés ;
  - résultat sur les deux arbres : 41 captures sur 41 au calme, frayeur nulle, aucune entité hors de celle posée par le
    banc.
- **Avant / après** : deux arbres compilés et lancés avec le même banc.
  - « Avant » : copie de `a9ccf1c` (jeu v4.16 et outillage de test v4.17 ; le code du jeu ne diffère de `c07eddb` que
    par le mode de capture du HUD) dans `C:\Users\Fristick\Desktop\Backrooms_v417_avant`. Les corrections du banc y sont
    reportées à l'identique : gel, vidéos, crochet de marche imposée.
  - « Après » : cette branche.
  - Mêmes graines, poses, réglages enregistrés (copiés d'un arbre à l'autre), fenêtre et pilote.
- **Critères mesurables** : `Tools/Visual/v417_criteria.py`. Les zones et les cibles sont celles de la QA visuelle et de la
  direction artistique ; la taille de capture est contrôlée.
- **Revues** : trois agents distincts ont relu les lots importants (Code Reviewer, Accessibility Auditor, Evidence
  Collector). Leurs constats et leurs suites sont dans `AGENTS_v4.17.md` § 5.

## 3. Changements, point par point du prompt

### 3.1 Combinaison (prompt § 4.1)

- **[constat de source]** Les slots `HazmatSuit`, `HazmatMask` et `HazmatGlass` passaient par le parent générique `Mesh`,
  sans normale tangente (seule la variante `bSkin` en avait une). La visière utilisait `T_Grime`, avec une rugosité de 0,04
  et un métal de 0,7.
- **Fait** :
  - nouveau parent **`M_BR_Gear`** (`5e3d710`) : normal map tangente, carte ORM, poids de la rugosité, réflectance,
    saturation, `RTSwitch`. Graphe identique en C++ (`BRMaterialBuilder.cpp`) et en Python (`backrooms_setup.py`) ;
    contrôlé par `Tools/Tests/test_material_sync.py` (9 tests) ;
  - visière en verre diélectrique : teinte presque noire (0,016 à 0,02), rugosité 0,06, métal 0 (0,7 avant), réflectance 0,6 ;
  - cartes **dérivées** de l'atlas protégé et des masques UV du maillage, dans un périmètre séparé
    (`RawAssets/Derived/`) : `T_Hazmat_Suit_N`, `T_Hazmat_Suit_ORM` (`d2049ec`, `ebe559a`, agent Unreal Technical
    Artist). Outils `export_hazmat_uv.py` et `derive_hazmat_maps.py --check` ;
  - valeurs neutres quand une carte manque. Un ancien matériau dans un paquet est signalé comme problème, et un équipement
    retombé sur `Mesh` est signalé ;
  - matériaux v10, puis v11 pour les icônes. `M_BR_Gear` et ses cartes figurent dans `essential_packages.txt`.
    Test v4.8 strict : `M_BR_Gear` exigé sur les trois slots (`347adac`).
- **[jugé à l'image]** (QA visuelle § 2.8 et passage figé) :
  - sous une lumière froide, les hautes lumières suivent les plis et la visière a un reflet net ;
  - sous le néon chaud du Level 0, le gain est modeste : les creux sont plus sombres, les hautes lumières inchangées ;
  - gants et bottes distincts ; aucune couture visible entre îlots à 1,5-3 m.
- **Coéquipier vu par l'hôte et par un client** : test réseau à deux instances (§ 7).
- **Non vérifié** : lampe allumée sur la combinaison, accroupissement, scintillement des reflets en mouvement, distance de
  8 à 10 m.

### 3.2 Modules et surfaces du Level 0 (prompt § 4.2)

- **Chanfreins** (agent Blender Add-on Engineer, `e359095`, `kit_blender.md`) : chanfreins ciblés et normales sur
  plinthes, chambranles, poteaux, ossature et luminaires du kit. Collisions séparées, dimensions et pivots inchangés,
  joints fermés (`verify_level0_kit`, kit réimporté : 34 modules).
  - **[jugé à l'image]** Un liseré clair suit l'arête de la plinthe à 80 cm ; le chanfrein de 6 mm ne se distingue pas
    au-delà de 2 m.
  - **[constat de source]** La porte de service et les luminaires du kit ne sont pas posés en jeu : la porte est
    inchangée (QA § 2.7).
- **Papier peint** (`0d87534`) : bandes environ 21 % moins claires, teinte ocre-olive, contraste −30 %, relief deux fois
  plus doux, joints des lés de 1,8 mm, ±2,5 % par lé. La carte de rugosité a été refaite d'après lui (`b79c8e7`).
  - **[jugé à l'image]** 3 à 5 joints visibles, qui suivent la perspective. Les chevrons restent identifiables à 3 m.
  - La ligne de marée est à la même hauteur sur tous les murs : non traité.
- **Cadre des plafonniers** en tôle peinte, sans `T_Grime` (`1eebfc2`) : écart-type relatif de la barre haute
  0,256 → 0,082 en Qualité (QA).
- **Variantes déterministes** (réparation, défaut de plafond, zone sèche/humide) : **non faites**. Les décalques de la
  v4.16 et leurs bornes sont inchangés.

### 3.3 Lumière du Level 0 après le mode 2 de Lumen (prompt § 4.3)

- Le **mode 2** du Cinématique est gardé. Agent Unreal Systems Engineer, `eclairage.md` :
  - analyse des 13 à 23 % de plus ;
  - 9 réglages proposés (B.3), qui ne touchent que l'image.
- **Pris** :
  - rebond des plafonniers 0,8 et portée visible de 560 cm (`e881217`) ; la règle garde 620 cm et sa loi ;
  - après la QA (diffuseurs brûlés, murs plus clairs que la moquette, halo et point chaud en Performance) (`3bb039b`) :
    - volets des dalles 70° sur 8 cm (au lieu de 88° sur 4) ;
    - diffuseurs × 0,72 ;
    - exposition locale du Level 0 : hautes lumières 0,7, ombres 1,0 ;
    - en Performance, projecteur vers le bas au lieu d'une lumière ponctuelle, à intensité égale au nadir. Le cône plein
      passe de 35° à 50° après le premier passage figé (`ee3365e`), sinon le haut des murs était trop sombre. Le
      projecteur ne reste qu'au Level 0 et aux Poolrooms (`2074046`) : au Niveau 1, il rendait le garage trop sombre
      (l1_vue L* 48,2 → 29,9).
- **Règle de jeu inchangée** (`LightRadius`, loi (1 − d/R)², seuils, grille) : test `v416light` (§ 7) et banc
  `test_light`.
- **152 paires occultées** : analysées (`eclairage.md` partie C), **non traitées**. Une correction toucherait la trace de
  la règle de jeu, hors de cette passe « image seulement ».
- **En mouvement** : entrée dans le couloir sombre de `l0_marche` à −11 à −14,5 % par image, sans pompage (QA § 2.4).
  - Pas de mesure dédiée de l'adaptation néon → mur sombre, de la porte, de la lampe ni de la fosse : instrumentation
    `eclairage.md` D.0 non faite ;
  - coupure et retour : poses figées au même instant du banc (§ 4).

### 3.4 Poolrooms (prompt § 4.4)

- **Caustiques attachées à l'eau** (`768a2f0`) :
  - entrée `NZ` du matériau ; atténuation murale sur 90 cm ; rien au-dessus de l'eau sur les dessus horizontaux ;
  - plafond flou (biais de mip 3) ; même HLSL en C++ et en Python ;
  - **[jugé à l'image]** plus de filaments sur les trottoirs secs ni au plafond ; caustiques gardées au fond et sur les
    parois immergées ; mouvement sans scintillement ;
  - le liseré mural manque sur `p_contrejour`.
- **Plafond, brume et voile** :
  - plâtre à 0,9 (plafond moins clair que le carrelage), brume 0,009, diffusion volumétrique 0,4 (`17f2e9b`) ;
  - bloom 1,4 → 1,0 et ombres de l'exposition locale non relevées (`3bb039b`) ;
  - mesure : § 4.
- **Non traité** :
  - les reflets des plafonniers sur l'eau restent des taches blanches à bord net ;
  - aucune autre technique d'eau essayée ; pas de mesure séparée eau / translucide / reflets / brouillard (§ 5).
- Rien n'a été ajouté aux Poolrooms (ni entité, ni changement de passage).

### 3.5 Entités (prompt § 4.5)

Agent Technical Artist : analyse (`animation.md` § 0-D), puis réalisation (`v417-anim`). Poses calculées seulement :
rien de répliqué, ni IA, capsules, vitesses ou attaques, ni ressources protégées touchées.

- **Bacteria** (`f14dbf7`) : pied tenu au monde, bonds de 0,16 à 0,26 s pour rattraper la capsule, griffes plantées,
  bras-béquilles, bras repliés près des murs.
- **Marcheurs à squelette** (`f662494`) : pied d'appui tenu au monde, piétinement en tournant, foulée du Hound.
- **Corps du joueur** (`14b2a4c`) : cadence tirée de la jambe, bassin et pieds posés au sol, bas du corps en retard.
- **Preuves** : la QA avait trouvé les animations « non démontrées » (Bacteria cachée, pieds hors cadre). Le banc a donc
  été refait :
  - Bacteria filmée par une caméra qui la suit, sur un **trajet imposé**, identique d'une version à l'autre : marche vers
    le joueur, demi-tour, course (`ABREntity::SetTestWalk`, tests seulement, `465ad85`) ;
  - marche du joueur **vue de côté**, pieds dans le cadre ;
  - appuis de la Bacteria et pose du corps du joueur relevés dans les fiches.
  - Résultats : § 4.
- **Non vérifié** : escalier, main dans une porte, objet tenu, coude, continuité attente → poursuite → attaque, Hound et
  Skin-Stealer en mouvement, entités dans les reflets en ray tracing hors champ.

### 3.6 Image nette, HUD et inventaire (prompt § 4.6)

- **Image nette** (`768a2f0`) :
  - VHS coupé : frange de base, saleté d'objectif et reflets d'objectif à 0 ;
  - grain coupé : plus aucun grain d'image, même ajouté par la santé mentale, les parasites ou la vision nocturne ;
  - nouvelle ligne GRAIN DU CAPTEUR ; aides mises à jour et traduites (`eb3cc8f`) ;
  - **[correction validée]** QA § 2.1 : aberration aux coins de 0,2-0,8 px à 0 dans les trois profils, reflets
    d'objectif disparus ; mode caméscope inchangé à ±1 %.
  - L'aide aux parasites du Level 6 reste un signal de jeu, hors du grain.
- **HUD** : toujours aucune barre de vie ; endurance et santé mentale seulement dans PERSONNAGE (Tab, touche
  modifiable).
- **Inventaire** (agent UI Designer, `v417-ui`) :
  - C1 échelle typographique (corps 12 pt, plancher 7 pt, 8 pt en CJK, arabe et persan) ;
  - C2 traits d'un pixel stables ; C3 cases lisibles ; C4 silhouette rendue dans Blender ;
  - C5 INSPECTER et infobulle opaques ; C8 défilement visible ; C10 casse correcte ; C12 en partie ;
  - C6 et C7 : réglages et déplacement d'objet à la manette.
- **Accessibilité** (agent Accessibility Auditor, `88b9ff0`, `3fa7ef5`) :
  - inventaire au **clavier seul** (flèches, Entrée, touches INTERAGIR et LAMPE ; pied propre au clavier) ;
  - textes atténués opaques ;
  - pastille des touches de la barre rapide ;
  - contour clair des cinq icônes sombres (environ 10:1 contre le fond de case) ;
  - pied d'INSPECTER, colonne du carnet annoncée à la manette, capitales Unicode des noms de touches.
- **Carnet long** : le test rend le carnet vraiment long (tous les indices lus, allemand) et mesure le défilement de la
  colonne qui déborde (`2f9e1b8`) : § 7.
- **Langues** : 22 langues, 1222 clés sur 1222. Relecture ciblée : 0 problème. Les nouvelles traductions sont marquées
  à relire (« fuzzy »).
- **Version** : jeu v4.17 (`2a13965`) ; protocole réseau 416 inchangé (rien de nouveau n'est répliqué : vérifié sur le
  diff).

## 4. Mesures avant / après (banc figé)

Passage 7 du banc figé (`avant7` = copie de `a9ccf1c`, `apres7` = `ee3365e`), fenêtre de 1366 × 768. Outil :
`python Tools/Visual/v417_criteria.py --before avant7 --after apres7`. Le diffuseur est mesuré par la part des pixels
lumineux (luminance > 0,5) dont le canal le plus fort atteint 250/255. Ce critère est un peu plus sévère que celui de la
QA visuelle (en Performance, il retrouve exactement ses 83,2 % et 22,4 %).

**Stabilité du banc** (même code, `avant6` contre `avant7`) :

- les critères ci-dessous sont égaux à 0,2 point près pour les parts et à 0,02 près pour les rapports, sauf le pixel
  maximal de la coupure avec lampe (128 contre 148) ;
- PSNR médian de 42,7 dB sur 41 poses.

### 4.1 Level 0

| Mesure | Cible | Qualité avant → après | Cinématique | Performance |
|---|---|---|---|---|
| `l0_salle` : diffuseur ≥ 250/255 | < 2 % | 71,5 → **0 %** | 81,5 → **0 %** | 83,1 → **1,4 %** |
| `l0_salle` : diffuseur à 255 | ≈ 0 | 0 → 0 % | 0 → 0 % | 22,6 → **0 %** |
| `l0_dos` : diffuseur ≥ 250/255 | < 2 % | 0,2 → 0 % | 1,6 → 0 % | 51,2 → 4,8 % |
| `l0_salle` : mur gauche / moquette | ≤ 1,0 | 1,36 → 1,17 | 1,37 → 1,17 | 1,53 → **0,85** |
| `l0_salle` : mur du fond / moquette | ≤ 1,0 | 1,54 → 1,38 | 1,58 → 1,42 | 1,68 → **0,81** |
| `l0_salle` : milieu du pilier / moquette | ≤ 1,0 | 2,07 → 1,82 | 2,10 → 1,84 | 2,43 → 1,65 |
| `l0_dos` : mur de face / moquette | ≤ 1,0 | 1,77 → 1,53 | 1,79 → 1,54 | 2,19 → 1,45 |
| `l0_salle` : tiers haut (luminance) | — | 0,263 → 0,241 | 0,263 → 0,242 | **0,403 → 0,179** |
| `l0_salle` : très sombre (< 0,01) | 4 à 10 % | 1,6 → 2,4 % | 1,7 → 2,5 % | 1,0 → 3,8 % |
| `l0_coupure_lampe` : pixel maximal | lampe visible | 148 → 121 | 125 → 97 | 126 → 99 |

En Performance, le passage `apres9` sur le code final (`2074046`) donne les mêmes valeurs au Level 0 et aux Poolrooms, à
1,2 point près : diffuseur de `l0_salle` à 0,8 %, de `l0_dos` à 6,0 %.

Lecture :

- **[correction validée]** Les diffuseurs ne sont plus écrêtés dans aucun profil.
- **[correction validée]** En Performance, le plafond délavé et le point chaud ont disparu.
  - Le tiers haut est passé de 1,53 fois celui de la Qualité à 0,74 fois.
  - Le premier réglage (cône plein de 35°) le donnait à 0,66 fois (`apres6`). Le cône de 50° de `ee3365e` réduit l'écart.
- **[jugé à l'image]** Coins et plafonds plus profonds, papier peint moins vert, cadres propres.
- **Non tenu** : murs plus clairs que la moquette en Qualité et en Cinématique (1,17 à 1,84 pour ≤ 1,0). La part de très
  sombre (2,4 %) reste sous la fourchette de 4 à 10 % de la direction artistique.
  - [hypothèse] L'exposition automatique compense la baisse d'albédo.
  - Il faudrait une courbe de compensation d'exposition (réglage 5 d'`eclairage.md`, non pris) ou une moquette plus claire.
- La pose `l0_combinaison_neutre_dos` est devenue très sombre (L* 29 → 11,8). La lumière du banc est devant le joueur, et
  l'exposition locale ne relève plus les ombres : la pose ne sert plus à juger la matière de dos (`VISUELS_v4.17.md` § 2.2).

### 4.2 Poolrooms

| Mesure | Cible | Qualité avant → après | Cinématique | Performance |
|---|---|---|---|---|
| `p_vue` : p1 de l'image | ≤ 0,01 | 0,060 → 0,051 | 0,064 → 0,056 | 0,033 → 0,038 |
| `p_vue` : p99 de l'image | ≥ 0,75 | 0,662 → 0,669 | 0,668 → 0,676 | 0,767 → 0,781 |
| `p_dos` : plafond / murs | ≤ 1 (plafond pas plus clair) | 1,86 → 1,73 | 1,91 → 1,79 | 2,10 → 2,07 |

- **[jugé à l'image]** Caustiques seulement sous et près de l'eau.
- **Non tenu** : voile laiteux (p1 ≈ 0,05) et plafond plus clair que les murs. Le bloom réduit et les ombres non relevées
  ne gagnent que 12 à 15 % sur p1 en Qualité et en Cinématique. En Performance, p1 monte même de 15 %.

### 4.3 Animation (fiches des vidéos)

Détail : `VISUELS_v4.17.md` § 5. Outil : `Tools/Visual/v417_motion.py`.

- **Bacteria** (après) :
  - **[correction validée]** à l'allure à appuis, pieds et griffes immobiles entre des bonds de rattrapage ;
  - en course, corps porté et lacet ≤ 15,3° par image ; écart du modèle ≤ 38,6 cm ;
  - **non tenu** : glissement de 3,5 cm par image au démarrage depuis l'arrêt ; au demi-tour, saut de lacet de 18° avec
    11 cm de glissement, puis un bond de 58° (critère ≤ 25°).
- **Joueur** (os des pieds, avant / après, même trajet) :
  - **[correction validée]** le pied d'appui touche le sol : os à 6,6 cm au plus bas, contre 11,6 cm en v4.16 ;
  - glissement par appui divisé par 2,5 environ (34-41 → 15,7 cm) ;
  - **non tenu** : critère de 3 cm par appui ; le pied en l'air descend plus bas que le pied d'appui au milieu du pas
    (3,6 cm), à vérifier.

## 5. Performances

**Machine** : portable, RTX 5050 Laptop (pilote 610.88), Core i7-13620H. Fenêtre de 1366 × 768. Résolutions internes :

- Performance et Qualité : 1093 × 614 ;
- RTX fluide : 915 × 515 ;
- Cinématique : 1366 × 768, déjà 100 %.

**La machine de référence RTX 4080 + Ryzen 7 7800X3D est absente : la cible de 60 images/s à 2560 × 1440 est non
vérifiée.** Le protocole pour cette machine est dans `v417/agents/performances.md` § 3.

**Protocole** (agent Performance Benchmarker, `performances.md` § 2) :

- `Tools/Tests/run_perf.ps1`, un profil par lancement (Performance, Qualité, RTX fluide, Cinématique), graines 0 et 4242 ;
- 7 scènes : `fosses`, `bassins`, `marche0`, `marche1`, `lidar6`, et les deux scènes ajoutées `entites0` (témoin) et
  `entites` (Hound, Bacteria, Smiler dans le noir) ;
- préchauffage jeté ; 0 chargement synchrone, 0 saccade de PSO dans les fenêtres mesurées ;
- **avant** : copie de `a9ccf1c` ; **après** : `e612818`. Lancés alternés, sur la machine au repos :
  - avant 1 (08:11), après 1 (08:44), avant 2 (09:18), après 2 (09:51) ;
  - soit 4 mesures par case, de 16 lancements de 4 minutes chacun par côté.
- Résultats : `Saved/TestRuns/perf/v417f_{avant,avant2,apres,apres2}/`. Comparaisons faites avec `perf_summary.py` :
  `v417f_tour1.md`, `v417f_tour2.md`, `v417f_deux_tours.md`.

**Temps d'image médian, deux tours réunis (ms, avant → après)** :

| Scène | Performance | Qualité | RTX fluide | Cinématique |
|---|---|---|---|---|
| fosses | 11,85 → 11,42 (−3,6 %) | 11,78 → 11,88 (+0,8 %) | 11,31 → 11,43 (+1,1 %) | 25,14 → 24,63 (−2,0 %) |
| bassins | 12,53 → 12,27 (−2,1 %) | 13,17 → 13,12 (−0,4 %) | 12,49 → 12,33 (−1,2 %) | 31,74 → 32,14 (+1,3 %) |
| marche0 | 12,16 → 11,82 (−2,8 %) | 12,08 → 12,01 (−0,6 %) | 11,52 → 11,59 (+0,6 %) | 25,16 → 24,81 (−1,4 %) |
| marche1 | 11,49 → 11,18 (−2,7 %) | 11,80 → 11,74 (−0,5 %) | 11,40 → 11,37 (−0,2 %) | 25,52 → 25,46 (−0,2 %) |
| lidar6 | 10,13 → 10,13 (0 %) | 10,62 → 10,74 (+1,1 %) | 10,38 → 10,33 (−0,5 %) | 19,59 → 19,27 (−1,6 %) |
| entites | 12,72 → 12,81 (+0,7 %) | 13,23 → 13,01 (−1,7 %) | 12,85 → 12,43 (−3,3 %) | 26,20 → 25,65 (−2,1 %) |
| entites0 | 11,83 → 11,76 (−0,6 %) | 12,37 → 12,25 (−0,9 %) | 11,99 → 11,80 (−1,5 %) | 25,20 → 24,80 (−1,6 %) |

Lecture :

- **Aucune régression mesurable.** Aucune médiane ne monte de plus de 1,3 %. D'un tour à l'autre, une même case varie
  jusqu'à 10 % : en Performance, les fosses donnent −9,8 % au tour 1 et +1,1 % au tour 2.
  - 95e centile : de −8,7 % à +2,9 %.
  - Images de plus de 50 ms : au plus 1,3 pour 1000 images (`entites0` en Cinématique, 1,0 avant), 0 ailleurs, avant
    comme après.
  - Bassins en Cinématique : 30 à 34 % des images au-dessus de 33 ms, avant comme après (scène la plus lourde, inchangée).
- **Performance** : −2 à −4 % sur les scènes du Level 0, mais seulement sur la somme des deux tours.
  - Le projecteur qui remplace la lumière ponctuelle réduit le poste « ombres » du GPU de 19 à 25 % dans les fosses, les
    bassins et `marche0` (0,20 à 0,23 ms), et le poste « éclairage » de 12 à 32 %.
  - Dans ce profil, l'image est limitée par le processeur (GPU ≈ 4,4 ms pour 11 à 12 ms d'image) : ce gain se voit peu
    dans le temps d'image.
- **VRAM** : −18 à −31 Mo en Performance ; ±4 Mo ailleurs. Les cartes de la combinaison (`T_Hazmat_Suit_N`, `_ORM`, 2048²)
  ne se voient pas à cette précision.
- **Maximum et 100 % de rendu** :
  - le Cinématique rend déjà à 100 % (1366 × 768 en interne) ;
  - l'essai forcé `r.ScreenPercentage 100` (`v417f_*_cine100`) donne les mêmes temps, de 19,4 ms (`lidar6`) à 32,3 ms
    (`bassins`), à ±1,2 % entre avant et après. Sur ce portable, ce profil ne tient pas 60 images/s.
- **Postes isolés** :
  - ombres et éclairage (Performance), ci-dessus ;
  - les autres postes (eau, brouillard, mise à jour RT, scanner) varient de moins de 0,1 ms, sauf MegaLights en
    Cinématique : de −0,59 ms (`entites`) à +0,21 ms (`marche1`) ;
  - pas d'essai A/B par variable de console dans cette version (protocole d'isolation : `performances.md` § 5).

**Correction après ces mesures** (`2074046`) :

- les planches de Performance montraient le Niveau 1 trop sombre, ses tubes étant devenus des projecteurs ;
- le projecteur ne reste qu'au Level 0 et aux Poolrooms ; les autres niveaux reviennent à la lumière ponctuelle de la
  v4.16 en Performance ;
- le Performance de l'après a été remesuré sur ce commit : **invalide**. À 12:0x, toutes les scènes donnaient +45 %, y compris celles dont le code n'avait pas changé (fil de
  rendu 11,9 → 17,1 ms, RHI 4,8 → 7,4 ms, GPU −3 %).
  - Cause trouvée ensuite : le jeu empaqueté du banc d'interface (§ 6) tournait encore depuis 10:40. Son lanceur avait été
    arrêté, pas lui.
  - Après l'arrêt de ce processus, mesure de contrôle alternée (avant / après, Performance, graine 0, 2 exécutions de
    chaque côté, 13:33 à 13:50, `v417f_controle.md`) :

| Scène | Médiane avant → après (ms) | Écart | GPU |
|---|---|---|---|
| fosses | 11,81 → 10,94 | −7,4 % | −6,8 % |
| bassins | 12,01 → 11,99 | −0,1 % | −11,2 % |
| marche0 | 10,88 → 10,99 | +1,0 % | −6,1 % |
| marche1 | 10,57 → 10,74 | +1,7 % | −1,2 % |
| lidar6 | 9,78 → 9,68 | −1,1 % | −0,8 % |
| entites | 12,21 → 12,23 | +0,2 % | −8,4 % |
| entites0 | 11,42 → 11,36 | −0,5 % | −8,2 % |

  Même conclusion que les deux tours : pas de régression ; le GPU gagne au Level 0 et aux Poolrooms, mais l'image reste
  limitée par le processeur.

## 6. Paquet Shipping et plateformes

**Windows 64 bits, paquet Shipping du commit final du code, `2074046`.** Les commits qui suivent ne changent que des
documents, sans aucun fichier sous `Source/`, `Config/` ou `Content/` (§ 7).

- **Construction** : `Tools/Build/build_windows.ps1 -Chapter` le 21 octobre, de 12:33 à 13:32 ; « BUILD SUCCESSFUL » ;
  compilation Shipping sans avertissement du compilateur ; contenu public, dans `Build/Windows/Shipping`.
  - BuildCookRun a pris 2824 s, contre 308 s pour le paquet précédent de `e612818`, construit de 07:58 à 08:11 et vérifié
    de même.
  - Pendant cette construction, un processus de test oublié chargeait la machine (§ 5).
  - Journaux et rapports des phases : `Saved/TestRuns/v417/paquet_2074046/` (et `paquet_e612818/`).
- **Manifeste** : commit `2074046`, « arbre : modifié ». Comme en v4.16, la cuisson a ajouté à `Config/DefaultEngine.ini`
  la section du greffon `AndroidFileServer` de l'éditeur, avec un jeton. Elle a été annulée après chaque construction
  (`git checkout`, rien de commité).
  - **Le `DefaultEngine.ini` empaqueté** (extrait du `.pak` par UnrealPak) n'a **aucune** ligne `AndroidFileServer` ni
    `SecurityToken`, dans les deux paquets.

| Vérification (`check_package.py`) | Résultat |
|---|---|
| structure | OK : `Backrooms-Win64-Shipping.exe` (161 Mo), 1 `.pak`, 2 `.utoc` |
| contenu | OK : 4604 fichiers dans les `.pak`, 1698 entrées IoStore. Les paquets essentiels, dont `M_BR_Gear`, `T_Hazmat_Suit_N` et `_ORM`, sont présents |
| architecture, dépendances | OK : x86_64 ; 41 bibliothèques importées, 13 chargées à la demande |
| lancement | OK : code 0, réussi, 0 problème, 0 non vérifié |
| chapitre, phase 1 | code 0 (314 s) : `0 → 1 → 37`, fermeture aux Poolrooms ; le jeu annonce v4.17, D3D12 SM6, RTX 5050 Laptop |
| chapitre, phase 2 | code 0 (237 s) : reprise après redémarrage, Niveau 2, fin du chapitre atteinte et enregistrée |
| chapitre, phase 3 | code 0 : partie après un nouveau redémarrage, CONTINUER au Niveau 2, nettoyage |
| **résultat** | **« RESULTAT : PAQUET VERIFIE ET LANCE, CHAPITRE TRAVERSE »**, code 0 |

**Modes d'affichage dans le paquet : non vérifiés.**

- Le banc d'interface v4.16 (`-BRAutoTestV416Visual`), qui bascule les vrais modes d'affichage, a été lancé dans le
  paquet de `e612818`. Il n'a pas fini en 60 minutes, et un jeu en Shipping n'écrit pas de journal.
  - [hypothèse] Ce banc charge aussi les Niveaux 4 et 6, absents du contenu public.
  - Le processus du jeu est resté en marche après l'arrêt de son lanceur (§ 5).
- Dans l'éditeur en mode jeu, `v416vis` a obtenu le fenêtré et le plein écran fenêtré avec leurs libellés. Le plein
  écran exclusif n'a pas été essayé : il change la définition de l'écran.
- La perte de focus reste non vérifiée (« à la main »), dans l'éditeur comme dans le paquet.

| Plateforme | État |
|---|---|
| Windows 64 bits | paquet Shipping construit, contrôlé, lancé, premier chapitre traversé (ci-dessus) |
| Linux | **non construit** : le moteur installé n'a pas `Engine/Platforms/Linux` et `LINUX_MULTIARCH_ROOT` est vide. Le propriétaire doit ajouter la plateforme Linux dans l'Epic Games Launcher |
| macOS | **impossible sur cette machine** (pas de Mac) : rien construit |
| Steam | rien envoyé, rien publié |

## 7. Tests exécutés après la dernière correction

Dernière modification du code du jeu : `ee3365e` (cône du projecteur en Performance). Les commits suivants ne touchent que
les bancs, les outils et les documents. Ils ne sont actifs qu'avec `-BRAutoTest`, sauf `ABREntity::SetTestWalk`, qui n'est
jamais appelé en jeu. Les tests d'Unreal ont tourné sur `516ad17` (code du jeu de `ee3365e`) ; le banc du carnet long et
le paquet sur `e612818`.

### 7.1 Dans Unreal (éditeur en mode jeu, une instance)

Résultats : `Saved/TestRuns/autotest/v417_final_516ad17/`.

| Test | Résultat | Remarque |
|---|---|---|
| `smoke` | réussi | |
| `v48` (combinaison, profils, RTX fluide) | **échec** au premier passage, puis réussi deux fois | voir ci-dessous |
| `v416vis` (interface, 22 langues, manette, modes d'affichage) | 0 problème, 8 non vérifiés | les 8 du passage précédent, dont le carnet long |
| `v416light` (lumière de jeu) | 0 problème, 2 non vérifiés | rues des Niveaux 9 et 11 hors banc ; aucune porte de mission sous la grille |
| `v416lidar`, `v416ladder`, `v416ship` | réussis | scanner, échelles, coopération et paquet |
| `v415` (protocole) | réussi | protocole 416, jeu v4.17 |
| `v415fix` | 0 problème, 1 non vérifié | marche avec un Smiler : aucun couloir droit et sombre |
| `v414` | 0 problème, 3 non vérifiés | perte de focus à la main ; 144 et 240 images/s non atteintes |
| `chapitre` (0 → 1 → 37 → 2, sauvegardes) | réussi (7,1 min) | |
| test réseau `run_nettest.ps1 -Clients 1` | 0 problème ; hôte : 4 non vérifiés, client : 1 | concurrence à 2 clients, matériel absent, perte de focus ; coéquipier vu par chacun : 3 slots sur 3 sur `M_BR_Gear` |
| `-BRAutoTestV417Carnet` (1280 × 720) | 0 problème, 1 non vérifié | aucun carnet des 7 niveaux ne déborde |
| banc visuel, passages 7 et 8 | 0 problème | en Performance après 7 : poses de retour du courant sans valeur |

**v48** :

- Au premier passage, les Poolrooms en RTX fluide ont échoué au seuil « 95 % des images sous 16,7 ms » : 18,4 ms.
  - Le GPU est inchangé (8,6 ms) ; le fil de rendu passe de 11,3-12,0 ms à 14,1 ms.
  - Ce passage tournait pendant que l'orchestrateur traitait des images sur la même machine.
- Deux relances sur la machine au repos : 13,8 et 13,7 ms, réussies. La copie « avant » relancée au même moment : 13,5 ms.
- Conclusion : bruit de mesure dû à une charge parasite, pas une régression. Le seuil de ce test est sensible : de 12,0 à
  17,8 ms d'une exécution à l'autre en v4.16.

### 7.2 Hors moteur

Lancés le 21 octobre sur `57f210b`, puis relancés à 11:42 sur `e612818` : 15 sur 15 réussis les deux fois
(`Saved/TestRuns/offline_v417/summary.txt`). Bancs C++ sur le commit final : <!-- HORSMOTEUR -->

| Contrôle | Résultat |
|---|---|
| `Tools/run_benches.py` (12 bancs stricts × gcc, clang-cl, msvc) | réussis |
| `unittest` : `test_perf_summary`, `test_packaging`, `test_material_sync` | réussis (1 ignoré) |
| `protect_assets.py check` | OK |
| `derive_hazmat_maps.py --check` | OK |
| `outline_icons.py --check` | OK (5 icônes cernées) |
| `loc_extract.py --check`, `loc_review.py` | à jour ; 0 problème (1222 clés sur 1222) |
| `verify_pitfalls.py`, `verify_lidar.py` (500 graines), `verify_device_clearance.py`, `verify_walk_block.py` | OK |
| `check_paths.py`, `generate_roughness.py --check`, `generate_ies.py --check`, `check_leg_ik.py` | OK |
| réimportation sans surveillance (`run_editor_setup.ps1`) | RESULT=OK, matériaux v11, 0 problème |

## 8. Non vérifié, défauts restants, décisions du propriétaire

### 8.1 Non vérifié

- **Machine de référence** (RTX 4080 + Ryzen 7 7800X3D, 2560 × 1440) : absente. La cible de 60 images/s est **non
  vérifiée**. Le protocole est dans `v417/agents/performances.md` § 3.
- **Linux** : rien construit. Le moteur installé n'a pas `Engine/Platforms/Linux` (seulement Android, IOS, VisionOS,
  Windows) et `LINUX_MULTIARCH_ROOT` est vide : c'est au propriétaire d'ajouter la plateforme dans l'Epic Games Launcher. **macOS** : pas de Mac. Aucune de ces plateformes n'est annoncée comme testée.
- **Vraie manette et clavier seul joués à la main** : non. Les appuis ont été injectés par les tests.
- **Animation** : escalier, main dans une porte, objet tenu, coude, enchaînement attente → poursuite → attaque, Hound et
  Skin-Stealer en mouvement, demi-tour sur place du joueur.
- **Reflets en ray tracing** des entités hors champ, selon le profil.
- **Eau** : caméra qui traverse la surface ; coût séparé de l'eau, du translucide, des reflets et du brouillard.
- **Lumière en mouvement** : néon puis mur sombre, porte, fosse, lampe (essais D.1 d'`eclairage.md`), sauf l'entrée dans
  un couloir sombre.
- **Traductions** : celles de la v4.17 sont marquées « fuzzy » : à relire par des personnes qui parlent chaque langue.

### 8.2 Défauts restants (constatés)

- **Level 0** :
  - murs plus clairs que la moquette en Qualité et en Cinématique (1,17 à 1,84) ;
  - ligne de marée à la même hauteur partout ;
  - porte de service inchangée : filets bleus au-dessus du battant, matière identique à l'embrasure ;
  - pas de variantes locales déterministes ; damier des fosses.
- **Poolrooms** : voile laiteux (p1 ≈ 0,05) ; plafond plus clair que les murs ; reflets des plafonniers en taches
  blanches à bord net ; liseré mural absent sur `p_contrejour`.
- **Niveaux 1 et 2** : taches sombres à bord dur sur 20 à 28 % du sol du Niveau 1. Elles viennent du masque des flaques
  du matériau partagé, pas de la texture : il faut le reprendre en C++ et en Python, avec une nouvelle version des
  matériaux. Niveau 2 quasi inchangé.
- **Animation** :
  - pied d'appui du joueur qui glisse encore d'environ 16 cm par appui. Le corps du joueur n'a pas de verrou du pied au
    monde ; les marcheurs l'ont ;
  - Bacteria : glissement au démarrage, bond de 58° au demi-tour.
- **Interface** :
  - arabe et persan affichés de gauche à droite (C9) ;
  - libellés de groupe empilés lettre par lettre (C11) ;
  - 42 textes raccourcis dans TOUCHES (M4) ; aide des réglages loin de la ligne et à défilement automatique (M6) ;
  - pas de répétition de la croix (m3) ;
  - pile de cinq notifications au centre du HUD en jeu ; notifications restées en français après un changement de langue.
- **152 paires** où la physique arrête un rayon que la grille des plafonniers laisse passer : non traitées (analyse dans
  `eclairage.md` partie C).

### 8.3 Décision du propriétaire : clignotement des néons et photosensibilité

- **Constat** (QA visuelle § 1.2) : au Niveau 1, avec FLASHS sur « normaux » (le réglage par défaut), le sol varie de
  × 2 à × 3,6 d'une image à l'autre pendant une défaillance de néon.
  - Ce sont des clignements de 30 à 80 ms, dans la plage de fréquences sensible pour les personnes photosensibles.
  - Le phénomène est **antérieur à la v4.17** (v4.15).
- **Ce qui existe** :
  - « atténués » : 3 clignements par seconde au plus, moins profonds ;
  - « aucun » : plus de clignement.
- **Ce qui manque** : aucun avertissement au premier lancement ne signale les lumières clignotantes ni ce réglage.
- **Options** : un avertissement au premier lancement, ou « atténués » par défaut. Ni l'un ni l'autre n'a été fait :
  c'est un choix de conception qui revient au propriétaire.

### 8.4 Hypothèses à tester

- Murs et moquette : une courbe de compensation d'exposition au Level 0 (`eclairage.md` B.3, réglage 5) tiendrait le
  rapport ≤ 1 sans assombrir les pièces éclairées.
- Poolrooms : les noirs viendraient de l'occlusion dans les fonds d'arches et de l'exposition, plus que de la brume.
- Le pied en l'air, plus bas que le pied d'appui au milieu du pas (3,6 cm contre 6,6 cm pour l'os), frôlerait le sol de
  la pointe.
