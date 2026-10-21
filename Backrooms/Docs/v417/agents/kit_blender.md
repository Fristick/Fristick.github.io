# v4.17 — Kit Blender du Level 0 : chanfreins ciblés et normales (§ 4.2)

- **Agent** : « Blender Add-on Engineer » (définition lue : `~/.claude/agents/blender-addon-engineer.md`). Il a été lancé comme sous-agent générique et a adopté ce rôle.
- **Base** : branche `backrooms-v4.17`, tête `c07eddb`. L'arbre était propre au départ.
- **Fichiers du dépôt** : un seul fichier modifié, `Tools/Blender/build_level0_kit.py`. Deux fichiers créés : `Tools/Blender/compare_level0_kit.py` (comparaison et rendus) et ce rapport. **Rien n'est commité.** `RawAssets/Level0Kit` n'a pas été touché.
- **Sorties de travail** (scratchpad de la session, `…\scratchpad\`, abrégé `SP`) :
  - `SP\kit_v416_ref\` : script d'origine (c07eddb) ;
  - `SP\kit_v417\` : nouveau script ;
  - `SP\kit_v417_nobevel\` : nouveau script avec `--no-bevels` ;
  - `SP\kit_v417\compare\` : rapport de comparaison (`compare_level0_kit.md` et `.json`) et rendus (`Renders\`).
- **Outils** : Blender 5.2.2 LTS (`--background`), Cycles sur OptiX pour les rendus, Python 3.14 système pour `verify_level0_kit.py`.
- **Images regardées par cet agent** : `v416_kit_angle`, `v416_kit_jambage`, `v416_kit_poteau_droit` et `v416_kit_plafond_mur` (final416/v416vis), `vis_0_Qualite_vue` et `vis_0_Qualite_mur_x` (final416/visual), les 16 rendus avant/après décrits au § 5, et des agrandissements de certains d'entre eux.

Légende : **[CODE]** constat dans les sources ; **[IMAGE]** observé sur une image nommée ; **[MESURE]** calcul hors moteur sur les FBX produits ; **[NON VÉRIFIÉ]** demande Unreal ou le paquet.

---

## 1. Résumé

1. Les moulures sont désormais des **sections balayées le long d'un chemin**, avec des **onglets** aux angles (classes `Profile` et `sweep`). Elles ont de petits chanfreins sur leurs seules arêtes libres : plinthes (murs, jambages, poteaux, pilier), chambranles, cadre, seuil, battant et poignée de la porte de service, cadre des luminaires.
2. Une arête de contact (mur, sol, plafond, joint entre modules) ne peut pas recevoir de chanfrein : `Profile` le **refuse à la construction** (AssertionError).
3. **Normales pondérées** : les grandes faces gardent leur normale plane. Le chanfrein est lisse : sa normale passe de celle d'une face voisine à celle de l'autre. Ces normales personnalisées sont exportées dans le FBX, avec les tangentes, et le maillage n'a aucun n-gone.
4. Les **UV0** du chanfrein sont dépliées depuis la face principale : continuité de ce côté-là, densité de texels 1,00 ± 2,4 %.
5. **Inchangés**, vérifiés sur les FBX relus : noms, pivots, boîtes englobantes, emplacements de matériaux et leur ordre, canaux UV, collisions UCX. `L0_Placement.json` est identique **octet pour octet** à celui de `RawAssets`. La sortie `--no-bevels` est **géométriquement identique** à la v4.16 pour les 38 modules.
6. `verify_level0_kit.py` passe sur la nouvelle sortie : **RESULTAT : OK**, graine 9 et 40 graines sans défaut.
7. **Portée en jeu** [CODE] : le C++ ne pose que les plinthes, les chambranles, les poteaux et le pilier du kit. La porte de service et les luminaires du jeu sont construits ailleurs (§ 6.1). Leurs versions du kit sont améliorées mais **ne se voient pas en jeu**.

## 2. Constat de départ

- **[CODE]** `MeshBuilder` assemblait des boîtes, puis `remove_doubles`. Toutes les faces étaient plates, exportées avec `mesh_smooth_type="FACE"`. L'import Unreal lit les normales et les tangentes du FBX (`backrooms_setup._kit_fbx_options` : `FBXNIM_IMPORT_NORMALS_AND_TANGENTS`). Il n'existait aucune normale lissée.
- **[CODE]** Les collisions sont des volumes `UCX_<module>_NN`, des boîtes à 8 sommets. Elles sont créées par `collision()` à partir de **listes de boîtes séparées** de la géométrie visible. Le travail n'a pas touché ces listes.
- **[CODE]** Modules posés par le jeu (`BRAssets.cpp` KitModules, `BRChunk.cpp` `AddKitModule` et `AddKitCellSlabs`) : murs, `Doorway`, `DoorCasing`, `Baseboard_290`, `Baseboard_070`, les cinq poteaux, `Pillar`, `ServiceDoor_Wall`, `Floor_350` et `Ceiling_350`. Le sol et le plafond sont rhabillés du matériau du niveau, projeté dans l'espace monde.
- **[IMAGE]** Sur `v416_kit_angle` et `v416_kit_poteau_droit`, la plinthe est une bande sombre à arêtes vives, sans liseré de lumière. Sur `v416_kit_jambage` et `vis_0_Qualite_vue`, le chambranle a des arêtes de boîte. Sur `v416_kit_plafond_mur`, l'ossature du plafond est **peinte dans la texture**.
- **[IMAGE]** Défauts de la v4.16 visibles sur les rendus « avant » :
  - cadre de la porte de service (`porte_service_cadre_avant`) : à l'angle, le jambage et la traverse se recouvrent et scintillent (z-fighting) ;
  - luminaire du kit (`luminaire_eteint_avant`) : une **fente de 6 mm** reste ouverte entre la face basse du cadre et le diffuseur, car il manquait la feuillure.

## 3. Ce qui a été fait (technique)

Toutes les modifications sont dans `Tools/Blender/build_level0_kit.py`.

- **`BEVEL_SPEC`** : table des largeurs et des segments (§ 4). Option **`--no-bevels`** : elle refait exactement les modules de la v4.16 (ancien code gardé dans les branches `else`).
- **`Profile`** décrit une section 2D convexe :
  - arêtes cachées (non émises) et arêtes de contact ;
  - chanfreins aux coins à 90° : 1 segment = chanfrein à 45°, 2 segments = arrondi à deux facettes ;
  - normales des points de l'arc ;
  - dépli des UV depuis une face d'ancrage.

  Elle refuse quatre cas : un chanfrein sur un coin dont une arête est de contact, un chanfrein plus large que l'arête, un coin qui n'est pas à 90°, une arête de longueur nulle.
- **`sweep`** balaye la section le long d'un chemin :
  - onglets aux angles : vecteur `(s0 + s1) / (1 + s0·s1)` ;
  - bouts fermés par la section entière, découpée en quads et triangles ;
  - orientation de chaque face forcée vers l'extérieur, puis contrôlée (`mb.inward`, resté à 0) ;
  - un groupe de lissage par segment de chemin : les onglets restent vifs.
- **`MeshBuilder`** a deux calques temporaires, la normale par coin (`cn`) et le groupe par face (`sg`). Dans `finish()` :
  - normales personnalisées, coin par coin ;
  - arête vive entre deux groupes, ou à côté d'une face plane ;
  - `normals_split_custom_set`, puis les calques sont retirés.

  La méthode `poly()` découpe les polygones de plus de 4 côtés : l'export FBX ne calcule les tangentes que sans n-gone.
- **Plinthes de poteau** : une plinthe continue par chaîne de faces exposées (`post_runs`). Le pilier reçoit un anneau fermé. Les bouts sont fermés par la même section que les plinthes de mur, donc le joint n'a pas de marche.
- **Robustesse** : Pillow n'est pas installé dans le Python de Blender 5.2. Son import n'est donc plus obligatoire. `--skip-textures` reprend les textures du dossier de sortie. Sans textures et sans Pillow, le script s'arrête avec un message qui dit quoi faire.
- **`build_report.json`** contient deux nouvelles clés : `bevels` (largeurs et segments) et `triangles` (par module).

## 4. Modules touchés : chanfreins et triangles

Largeur = retrait sur chacune des deux faces de l'arête. Les triangles ont été comptés sur les FBX relus [MESURE].

| Module | Arêtes chanfreinées | Largeur | Segments | Tri. avant | Tri. après | Posé en jeu |
|---|---|---:|---:|---:|---:|---|
| SM_L0_Baseboard_290 | arête supérieure avant | 0,5 cm | 1 | 8 | 12 | oui (2 par mur) |
| SM_L0_Baseboard_070 | arête supérieure avant | 0,5 cm | 1 | 8 | 12 | oui (4 par passage) |
| SM_L0_Wall_Corner | plinthes : arête sup. avant, onglet saillant | 0,5 cm | 1 | 22 | 28 | oui |
| SM_L0_Wall_End | idem, deux onglets | 0,5 cm | 1 | 26 | 34 | oui |
| SM_L0_Wall_JunctionT | idem | 0,5 cm | 1 | 14 | 22 | oui |
| SM_L0_Wall_Straight | idem, deux plinthes | 0,5 cm | 1 | 18 | 34 | oui |
| SM_L0_Pillar | anneau de plinthe, quatre onglets | 0,5 cm | 1 | 20 | 34 | oui |
| SM_L0_DoorCasing | deux arêtes avant (côté passage et côté mur), onglets à 45° aux angles hauts, deux faces | 0,6 cm | 1 | 72 | 88 | oui |
| SM_L0_Light_Panel / _Off | arête basse extérieure du cadre, arête côté diffuseur, feuillure de 6 mm ajoutée | 0,4 / 0,3 cm | 1 | 18 | 42 | **non** (kit seulement) |
| SM_L0_ServiceDoor_Frame | cadre : deux arêtes côté pièce, onglets ; seuil : deux arêtes supérieures | 0,4 cm ; 0,3 cm | 2 ; 1 | 46 | 84 | **non** (kit seulement) |
| SM_L0_ServiceDoor_Leaf | pourtour des deux grandes faces (tôle pliée) | 0,4 cm | 2 | 36 | 68 | **non** |
| SM_L0_ServiceDoor_Handle | rosace : pourtour de la face avant ; béquille : quatre arêtes longues | 0,3 cm ; 0,4 cm | 1 ; 2 | 48 | 128 | **non** |

Aucune arête chanfreinée ne touche le mur, le sol, le plafond ou un module voisin :

- le dos et le dessous des plinthes ne sont pas émis, comme en v4.16 ;
- le dos du chambranle, plaqué contre le mur, reste vif ;
- le dessus du cadre de luminaire, contre le plafond, n'est pas émis ;
- le chant du cadre de service reste vif dans l'épaisseur du mur.

Modules **inchangés** (la signature des sommets et des UV est identique à la v4.16) : murs, `Doorway`, `Wall_JunctionX`, sol, plafond, fosses, `ServiceDoor_Wall`, sas, charnière, filet lumineux, décalques, craies, étalon et archives.

**Budget [MESURE]** : la pose moyenne du jeu sur 40 graines a été lue dans `verify_level0_kit`. Elle donne 1 520 `Baseboard_290`, 1 392 `Baseboard_070`, 348 chambranles, 26, 101, 171 et 375 poteaux d'angle, de bout, en T et droits, et 12 piliers.

- Toute l'enceinte du Level 0 passe de **104 124 à 129 840 triangles**, soit +25 716 (+24,7 %).
- Ce chiffre compte tous les chunks de l'enceinte, pas ce qui est affiché à un instant donné : les plinthes et les chambranles sont coupés à 60 m.
- Les modules sont instanciés (un ISM par module et par chunk) : le nombre d'appels de dessin ne change pas.
- La plus grosse part vient des bouts fermés des plinthes de poteaux droits : 375 × 16, soit 6 000 triangles. On peut les retirer si le budget l'exige (§ 6.4).

## 5. Vérifications

### 5.1 Comparaison module par module

Script : `Tools/Blender/compare_level0_kit.py`. Il relit les FBX comme Unreal les lirait. Rapport complet : `SP\kit_v417\compare\compare_level0_kit.md`.

- **38 modules, 0 problème.**
  - Mêmes objets et noms, y compris `UCX_*`.
  - Pivots identiques : position, rotation et échelle de l'objet à l'origine.
  - Boîtes identiques à 1e-4 m près.
  - Mêmes matériaux, dans le même ordre.
  - Mêmes canaux `UVMap` et `UVLightmap`, et mêmes couleurs de sommet.
  - **UCX identiques** à 1e-5 m près : même nombre, mêmes noms, 8 sommets, mêmes boîtes, mêmes pivots.
- Le `build_report.json` de la nouvelle sortie est identique à celui de la référence pour :
  - `checks` : étalon, normales, trous ouverts, chemin de la porte ;
  - `counts` ;
  - `fbx` : dimensions, collisions, matériaux.

  Les dimensions des FBX de `RawAssets` (Blender 5.0.1) sont aussi identiques à celles de la référence reconstruite sous Blender 5.2.
- **`--same`** : la sortie `--no-bevels` a la même signature (sommets, UV et taille des faces) que la v4.16 pour les 38 modules. Le remaniement ne change rien quand les chanfreins sont désactivés.
- **Plans de contact** :
  - Plinthes : les plans du mur et du sol sont inchangés (même enveloppe de sommets). Seuls les plans libres perdent de l'aire, c'est-à-dire la face avant et le dessus ; les bouts perdent le petit triangle du chanfrein (§ 5.2).
  - Chambranle : le dos contre le mur garde **0,5562 m²** avant comme après. Le flanc extérieur fait encore **1,9 cm** de saillie avant son chanfrein, contre 2,5 cm avant ; la plinthe, avec 1,5 cm de saillie, reste donc entièrement couverte à son arrêt.
  - Pied du chambranle : il perd les coins du chanfrein sur le sol (8 × 0,18 cm²). C'est le chanfrein qui descend jusqu'au sol ; le sol reste plein dessous, rien ne s'ouvre.
  - Cadre de service : le chant dans l'épaisseur du mur garde **1,3725 m²** avant comme après. Seule la saillie de 2 cm côté pièce est arrondie (le chant s'arrête à x = 1,6 cm au lieu de 2 cm).
- **Normales** : dans chaque module, seuls les coins des faces de chanfrein ont une normale écartée de leur face de plus de 1°. Leur nombre vaut exactement 4 × le nombre de faces de chanfrein (4 pour une plinthe, 48 pour un chambranle, 96 pour la poignée). Les grandes faces restent planes, sans ombrage baveux.
- **UV** : chaque face de chanfrein a des UV continues avec au moins une voisine (13 modules sur 13). Densité de texels rapportée à celle des faces planes :
  - 1,0000 sur les plinthes droites ;
  - de 0,998 à 1,003 sur le chambranle, le cadre et le luminaire ;
  - 1,004 à 1,011 sur les onglets des poteaux et du pilier ;
  - jusqu'à 1,024 sur la poignée.

  Ces écarts de 1 à 2 % se trouvent dans les 5 mm de l'onglet ; ils ne se voient pas.

### 5.2 Joints des plinthes (sections relevées sur les FBX)

| Bout | Points de la section (\|y\|, z en m) |
|---|---|
| `Baseboard_290` et `Baseboard_070` (après) | (0,30 ; 0), (0,30 ; 0,12), (0,31 ; 0,12), (0,315 ; 0,115), (0,315 ; 0) |
| Plinthes des poteaux `Wall_Straight`, `JunctionT`, `Corner`, `End`, au plan x = +T/2 (après) | **mêmes points**, plus l'arête du poteau (0,30 ; 2,9) |
| Avant (v4.16) | plinthes de mur : rectangle de 1,5 × 12 cm ; plinthes de poteau : **bout ouvert**, aucune face |

Le profil est donc identique des deux côtés de chaque joint, sans marche. Les angles rentrants restent faits comme en v4.16, par l'interpénétration des deux plinthes. Cette union donne exactement l'onglet rentrant (rendu `plinthe_rentrant`).

### 5.3 `Tools/verify_level0_kit.py`

Le chemin `PLACEMENT` est écrit en dur dans le script. Il a été remplacé en mémoire par un appel Python, sans modifier le fichier :

```python
v.PLACEMENT = r"…\kit_v417\L0_Placement.json"
sys.exit(v.main())
```

| Sortie | Résultat | Journal |
|---|---|---|
| `kit_v417` | **RESULTAT : OK**, graine 9 et 40 graines sans défaut | `SP\kit_v417_verify.txt` |
| `kit_v416_ref` | même résultat | `SP\kit_v416_ref_verify.txt` |

Limite : ce script contrôle la logique de pose, pas la géométrie. Or `L0_Placement.json` est identique, ce qui était attendu.

### 5.4 Rendus rapprochés avant / après (Cycles, 1280 × 800, 128 échantillons)

Dossier : `SP\kit_v417\compare\Renders\`. Pour chaque vue, trois fichiers : `<vue>_avant.png`, `<vue>_apres.png` et `<vue>_avant_apres.png` (avant à gauche, après à droite).

Les scènes sont construites à partir des **FBX relus**, ce qui vérifie que les normales survivent à l'export. Elles utilisent les matériaux du kit, éclairés par un néon de plafond et une lumière rasante. Toutes les images ont été regardées, certaines agrandies.

| Vue | Ce qui est examiné | Observé [IMAGE] |
|---|---|---|
| `plinthe_angle` | angle saillant au bout de mur (`Wall_End`), à environ 45 cm | Après : onglet chanfreiné propre, liseré de lumière continu sur l'arête autour de l'angle. Avant : coin en boîte. |
| `plinthe_raccord` | joint plinthe de mur / plinthe de poteau droit, lumière rasante | Après : liseré continu à travers le joint, aucune marche visible. Avant : arête vive, sans reflet. |
| `plinthe_rentrant` | angle rentrant (`Wall_Corner` et deux murs) | Après : les deux chanfreins se rejoignent en onglet rentrant propre, sans défaut visible au croisement. |
| `chambranle_angle` | onglet haut du chambranle | Après : le chanfrein côté passage accroche la lumière jusqu'à l'onglet. Avant : arêtes de boîte. |
| `chambranle_pied` | pied du chambranle et arrêt de la plinthe de jambage | Après : la plinthe s'arrête contre le flanc, sous le chanfrein du chambranle ; le chanfrein descend sur la moquette. |
| `porte_service_cadre` | angle haut du cadre de service | Avant : **z-fighting** des boîtes superposées à l'angle. Après : onglet propre, arêtes arrondies avec reflet. |
| `porte_service_poignee` | béquille et rosace, côté pièce | Après : béquille arrondie et rosace chanfreinée. Les stries brossées continuent sur les arrondis, sans étirement. Léger dentelé le long de l'arête basse, dû à la normal map de la quincaillerie. |
| `luminaire_allume` | coin du cadre, diffuseur allumé | Après : arête intérieure plus nette, avec un liseré. Écart faible : à cette distance, le diffuseur domine l'image. |
| `luminaire_eteint` | cadre et feuillure sous lumière rasante, diffuseur éteint | Avant : **fente ouverte** de 6 mm le long du diffuseur. Après : feuillure fermée, liseré sur les deux arêtes. La normal map bosselée du métal charge un peu la feuillure sous lumière rasante. |

## 6. Limites et risques

1. **Effet en jeu limité** [CODE] : l'image du jeu ne change que pour les plinthes, les chambranles, les poteaux et le pilier.
   - La porte de service du jeu est faite de cubes mis à l'échelle : `ABRExit::BuildServiceDoor`, `BRInteractables.cpp:772`.
   - Les plafonniers utilisent `SM_LightPanel` : `BRChunk.cpp:1638`.
   - Pour voir les nouveaux modules, il faut soit les poser depuis le C++ (`BRInteractables.cpp` / `BRChunk.cpp`, hors périmètre de cet agent), soit reprendre la même section dans le générateur de `SM_LightPanel`.
2. **Ossature du plafond : rien n'a été fait.** Le kit n'en a aucune géométrie : la grille est peinte dans `T_L0K_CeilingTile` et, en jeu, dans le matériau du plafond projeté dans l'espace monde. Une ossature en relief dans un module de 3,50 m ne peut pas s'aligner sur une grille de 61 cm projetée dans le monde (3,50 / 0,61 n'est pas entier). Décision à prendre : une géométrie posée par le C++ et calée sur la grille du monde, ou un relief porté par le matériau.
3. **Import Unreal et paquet [NON VÉRIFIÉ].** Rien n'a été importé ni cuisiné ici. Pour intégrer :
   1. Régénérer le kit : `blender -b --factory-startup -P Tools/Blender/build_level0_kit.py -- --out RawAssets/Level0Kit --skip-textures`, en ajoutant `--render` pour refaire les rendus de la scène. Cette commande récrit `BR_Level0.blend` (environ 26 Mo) et `build_report.json` (Blender 5.2.2).
   2. Lancer `import_level0_kit(force=True)`, ou changer de `VERSION`. Sinon, les anciens maillages restent dans le paquet.
   3. Vérifier en jeu que Interchange garde les normales importées : le liseré doit apparaître en haut des plinthes. Vérifier aussi que `BackroomsSetup_Level0Kit.txt` donne toujours le même nombre d'UCX par module.
4. **Joints et budget.**
   - Angles rentrants : les dessus des deux plinthes sont coplanaires sur environ 1 × 1 cm, contre 1,5 × 1,5 cm en v4.16. Leurs UV diffèrent d'un module à l'autre, ce qui peut provoquer un scintillement minuscule vu de dessus. Ce défaut existait avant ; il est inchangé et n'est pas visible sur le rendu Cycles.
   - Bouts des plinthes de poteaux : ils sont maintenant **fermés**. À côté du mur percé de la porte de service, qui n'a pas de plinthe, la v4.16 laissait un bout creux. C'est un constat de source, non rendu ici.
   - Ces bouts coûtent environ 6 000 triangles sur toute l'enceinte. On peut les supprimer là où un mur arrive toujours, si le budget le demande.
5. **Collisions** : elles sont identiques. Les chanfreins rentrent de 3 à 6 mm sous les boîtes UCX (battant, cadre). L'écart est invisible et n'a aucun effet sur le jeu.
6. **Matières** : les chanfreins prennent la normal map de leur matériau. Sous lumière rasante, la feuillure du luminaire et l'arête basse de la béquille paraissent un peu chargées. Le réglage relève des cartes (agent matériaux), pas de la géométrie.
7. **Outil** : le Python de Blender 5.2 n'a pas Pillow. La référence a été construite avec le script d'origine, non modifié, et une fausse bibliothèque PIL limitée à l'import (`SP\pilstub`, passée par `--python-use-system-env`). Les textures ont été copiées depuis `RawAssets/Level0Kit/Textures` dans les deux sorties.

## 7. Commandes exécutées (reproductibles)

```text
# Référence (script à c07eddb, avant modification)
PYTHONPATH=SP\pilstub blender -b --factory-startup --python-use-system-env -P Tools/Blender/build_level0_kit.py -- --out SP\kit_v416_ref --skip-textures
# Nouvelle sortie, puis la même sans chanfreins
blender -b --factory-startup -P Tools/Blender/build_level0_kit.py -- --out SP\kit_v417 --skip-textures
blender -b --factory-startup -P Tools/Blender/build_level0_kit.py -- --out SP\kit_v417_nobevel --skip-textures --no-bevels
# Comparaison et rendus
blender -b --factory-startup -P Tools/Blender/compare_level0_kit.py -- --a SP\kit_v416_ref --b SP\kit_v417 --same SP\kit_v417_nobevel --out SP\kit_v417\compare --render --samples 128 --res 1280x800
# Pose (verify_level0_kit, PLACEMENT remplacé en mémoire), sur les deux sorties
python -c "import sys,os; sys.path.insert(0,'Tools'); import verify_level0_kit as v; v.PLACEMENT=r'SP\kit_v417\L0_Placement.json'; sys.argv=['v','--out',r'SP\kit_v417_verify.txt']; sys.exit(v.main())"
```

Contrôles du build de la nouvelle sortie :

- normales : `{}` (aucune face inversée, aucune arête incohérente) ;
- trous ouverts : 16/16 ;
- chemin de la porte : 28 pas, aucun bloqué ;
- relecture des FBX : tous `ok`.
