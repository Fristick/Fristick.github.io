# Kit modulaire du Level 0 (Blender)

Kit texturé du Level 0 de THE BACKROOMS, avec une scène de démonstration de la graine 9 et la porte de service de la
v4.13. Tout est produit par deux scripts, sans retouche à la main :

- `Tools/Blender/build_level0_kit.py` : modules, disposition, éclairage, repères, caméras, contrôles, exports, rendus ;
- `Tools/Blender/level0_kit_textures.py` : textures PBR (numpy et Pillow, sans Blender).

**Exécuté ici** avec **Blender 5.0.1** (module Python `bpy` 5.0.1, sans interface, Cycles sur processeur, 4 cœurs, sans
GPU) : construction, contrôles, exports FBX, relecture de chaque FBX dans Blender, fichier `.blend`, rendus.

**Non fait ici : l'import dans Unreal.** Unreal Engine n'est pas disponible dans cet environnement. Les réglages
d'import du § 7 sont **préparés, non exécutés** : aucun résultat d'import n'est annoncé.

**v4.14 : import scripté et branchement en jeu (préparés, non exécutés).** `Content/Python/backrooms_setup.py`,
`import_level0_kit(force=True)`, importe le kit dans `/Game/Backrooms/Level0Kit/{Meshes,Textures,Materials}` avec les
réglages du § 8 (UCX, sans collision générée, normales et tangentes du FBX, UV1 fourni), crée les matériaux par nom de
slot et écrit `Saved/BackroomsSetup_Level0Kit.txt`. Le générateur du Level 0 (`ABRChunk`) pose alors les murs,
passages, poteaux, piliers et le mur percé de la porte de service avec ces modules, **si le kit est complet**
(`UBRAssets::UseLevel0Kit`), sinon ses murs habituels. La pose est vérifiée hors moteur par
`Tools/verify_level0_kit.py` (200 graines ; graine 9 comparée à `L0_Placement.json` : 2173 modules identiques). Les
modules archivés (`FBX/Archive`) ne sont pas importés.

## 1. Contenu

```
RawAssets/Level0Kit/
  BR_Level0.blend            scene complete, images emballees (fichier autonome)
  FBX/SM_L0_*.fbx            un FBX par module, avec ses collisions UCX_
  FBX/Archive/*.fbx          anciennes sorties du Level 0 (echelle, trappe, mur instable), gardees comme assets
  Textures/T_L0K_*           copies exportables des images (les memes que dans le .blend)
  L0_Placement.json          chaque instance et chaque repere, en metres (Blender) et en cm (Unreal)
  Renders/L0_*.jpg           six rendus Cycles
  build_report.json          controles et mesures de la construction
  README.md
```

## 2. Régénérer

Depuis `Backrooms/` :

```sh
python Tools/Blender/build_level0_kit.py --seed 9 --render              # module bpy 5.0.1
blender -b -P Tools/Blender/build_level0_kit.py -- --seed 9 --render    # Blender 5.0.1 en ligne de commande
```

| Option | Défaut | Effet |
|---|---|---|
| `--seed` | 9 | graine du jeu (`-BRSeed`), 9 = graine de démonstration |
| `--out` | `RawAssets/Level0Kit` | dossier de sortie |
| `--render` | non | rendus Cycles des six vues |
| `--samples` | 48 | échantillons par pixel (débruitage OpenImageDenoise) |
| `--res` | 1280x720 | taille des rendus |
| `--skip-textures` | non | réutilise les textures déjà générées |
| `--only` | (toutes) | rendus : seulement ces vues, séparées par des virgules |

Durées mesurées ici (4 cœurs, sans GPU) : textures, construction, contrôles et exports ≈ 1 min 20 s ; rendus 232 à
268 s par vue à hauteur d'œil, 75 s pour la vue d'ensemble ; 24 min en tout (journal :
`Docs/v413/blender_kit.txt` ; `build_report.json`, `render_seconds`). La même graine donne les mêmes fichiers.

`--only L0_VueEnsemble` (avec `--render`) ne refait que les vues nommées et garde les temps des autres.

## 3. Méthode

1. **Disposition** : la grille du Level 0 pour la graine demandée est rejouée par la simulation Python du générateur du
   jeu (`Tools/verify_pitfalls.py`, classe `Level0`) : murs, portes, piliers, salles de fosses et leurs trous, néons
   (dont ceux en panne), cachettes, objets, porte de service et chemin de L. Cette simulation est vérifiée contre les
   règles du C++ par ses propres contrôles (500 graines pour la porte, 24 pour les fosses : `Docs/v413/`).
   **L'assemblage n'a pas été comparé à une capture du jeu dans Unreal.**
2. **Textures** : chaque carte est un fichier image ; rien ne dépend d'un nœud procédural de Blender. Les textures du
   projet `T_L0_Wallpaper`, `T_L0_Carpet`, `T_L0_Ceiling` et `T_Concrete` (non protégées) servent de base ; elles ne sont
   pas modifiées : chaque carte du kit est un nouveau fichier `T_L0K_*`. Aucun modèle protégé
   (`RawAssets/protected_assets.json`) n'est lu.
3. **Modules** : maillages fermés construits par boîtes, origine sur la grille, à l'échelle réelle (1 unité = 1 m).
4. **Scène** : instances liées des modules (une seule copie de chaque maillage), placées comme le jeu les placerait.
5. **Contrôles** (§ 9), **exports**, **rendus**.

## 4. Dimensions (celles du jeu)

| Cote | Valeur |
|---|---|
| Zone | 32 × 32 cellules = **112 m** de côté |
| Cellule | **3,5 m** |
| Hauteur sous plafond | **2,9 m** |
| Épaisseur des murs | **0,6 m** (mur de 2,9 m entre deux poteaux) |
| Passage des portes ordinaires | **1,5 m** de large, linteau à 2,25 m |
| Salle de fosses | 5 × 5 cellules = **17,5 m** |
| Trous | **2 × 2 m**, centrés sur les coins intérieurs ; passerelles de **1,5 m** entre eux |
| Profondeur des puits | **14 m** sous la dalle (fond sombre) |
| Seuil de chute mortelle | **4,5 m** sous le sol (repère `MK_PitKill_*`) |
| Porte de service | ouverture 1,30 × 2,25 m, passage libre 1,10 m, battant 1,07 × 2,11 m |

L'étalon `SM_L0_Reference_1m` mesure 1,000 × 1,000 × 1,000 m (contrôlé).

## 5. Modules

Origine de chaque module : au **niveau du sol** (z = 0), sur la grille ; tout se place donc à z = 0. Les murs sont
centrés sur la ligne de la grille, le long de X local.

| Module | Rôle | Dimensions (m) | Collisions |
|---|---|---|---|
| `SM_L0_Reference_1m` | étalon | 1 × 1 × 1 | — |
| `SM_L0_Wall_350`, `SM_L0_Wall_Aged_350` | mur entre deux poteaux (papier peint neuf ou jauni) | 2,9 × 0,6 × 2,9 | 1 |
| `SM_L0_Doorway` | mur percé d'un passage de 1,5 m (jambages, linteau) | 2,9 × 0,6 × 2,9 | 3 |
| `SM_L0_DoorCasing` | chambranle des deux faces | 1,68 × 0,65 × 2,34 | — |
| `SM_L0_Baseboard_290`, `_070` | plinthes (face de mur, jambage) | 2,9 / 0,7 × 0,015 × 0,12 | — |
| `SM_L0_Wall_Corner`, `_JunctionT`, `_JunctionX`, `_End`, `_Straight` | poteaux aux croisements, plinthes sur les faces visibles seulement | 0,6 × 0,6 × 2,9 | 1 |
| `SM_L0_Pillar` | pilier isolé de 45 cm | 0,48 × 0,48 × 2,9 | 1 |
| `SM_L0_Floor_350` | moquette sur dalle de béton de 20 cm | 3,5 × 3,5 × 0,2 | 1 |
| `SM_L0_Ceiling_350` | plafond suspendu (dalles de 0,6 m) | 3,5 × 3,5 × 0,05 | 1 |
| `SM_L0_Light_Panel`, `_Off` | néon encastré 1,2 × 0,6 m, allumé ou en panne | 1,2 × 0,6 × 0,012 | — |
| `SM_L0_Pit_HoleTile` | dalle percée d'un trou de 2 × 2 m, tranches de moquette et de béton | 3,5 × 3,5 × 0,2 | 4 |
| `SM_L0_Pit_Walkway_350`, `_WalkwayCorner` | bordures de la salle de fosses | 3,5 / 1,75 × 1,75 × 0,2 | 1 |
| `SM_L0_Pit_Shaft` | puits de 14 m, béton assombri avec la profondeur (couleur de sommet) | 2,6 × 2 × 14 | 4 |
| `SM_L0_Pit_Bottom` | fond sombre | 2 × 2 | 1 |
| `SM_L0_ServiceDoor_Wall` | mur d'enceinte percé (1,30 × 2,25 m) | 0,6 × 2,9 × 2,9 | 3 |
| `SM_L0_ServiceDoor_Frame` | cadre d'acier et seuil | 0,63 × 1,3 × 2,25 | 3 |
| `SM_L0_ServiceDoor_Leaf` | battant ; **origine = axe des charnières** ; plaque et barre de poussée | 0,093 × 1,07 × 2,11 | 1 |
| `SM_L0_ServiceDoor_Handle` | poignée béquille des deux faces ; origine = axe du carré | 0,13 × 0,17 × 0,22 | — |
| `SM_L0_ServiceDoor_Hinge` | charnière (trois par porte) | 0,04 × 0,04 × 0,13 | — |
| `SM_L0_ServiceDoor_DraftGlow` | filet de lumière froide sous le battant (émissif) | 0,03 × 1,02 × 0,008 | — |
| `SM_L0_ServiceVestibule` | sas : palier, huit marches qui descendent, murs, plafond bas | 4,9 × 2,8 × 4,8 | 9 |
| `SM_L0_ChalkMark`, `SM_L0_ChalkArrow` | marque de L. (cercle barré, « L. ») et flèche, traits de craie | 0,44 × 0,22 / 0,34 × 0,13 | — |
| `SM_L0_Decal_WaterStain`, `_CeilingStain`, `_CarpetDamp` | plans à masque alpha (auréoles, taches, moquette humide) | 1 × 1,3 / 1,1 × 1,1 / 2 × 1,5 | — |
| `SM_L0_Archive_Ladder_320`, `_HatchFrame`, `_HatchLid`, `_UnstableWall` | **archivés** : anciennes sorties du Level 0 (échelle, trappe, mur instable), gardées comme assets ; plus utilisés par le jeu | | — |

La porte de service suit le repère du jeu (`ABRExit::BuildServiceDoor`) : X local vers la pièce, le mur va de X = 0 (face
intérieure) à X = -0,6 ; la charnière est du côté +Y local (−Y dans Unreal) ; le battant s'ouvre vers le sas (+95° de
lacet dans Unreal).

**Porte tirée à chaque nouvelle partie (v4.13).** Le jeu ne fixe plus l'emplacement de la porte : l'hôte le tire à chaque
nouvelle partie parmi les **candidats** : cellules contre le mur d'enceinte (ni coin, ni zone de départ), atteintes depuis
le départ sans traverser de fosse ; la porte est percée dans ce mur d'enceinte. Le tirage suit une graine de partie,
sauvegardée avec l'emplacement retenu (`ABRWorld::PlaceServiceDoor`). Les modules `SM_L0_ServiceDoor_*` (cadre, battant,
poignée, charnières, sas) se posent **sur n'importe quel candidat**, quelle que soit la face de l'enceinte (quatre
orientations) : il suffit de placer le repère du module au pied du mur, X local vers la pièce. La
porte posée dans la scène n'est qu'un **exemple** (celle que la graine 9 de l'export retiendrait) ; les candidats sont
tous exportés en repères `MK_DoorCandidate_NNN` (§ 6), pour que le tirage final reste celui de l'hôte.

## 6. Collections de la scène

| Collection | Contenu |
|---|---|
| `L0_Modules` | un exemplaire de chaque module, rangés hors de la zone (x < −70 m), hors rendu |
| `L0_Layout` | murs, passages, poteaux, piliers, sols, décals de murs et de moquette |
| `L0_Ceiling` | plafonds, néons, taches de plafond (masqués pour la vue d'ensemble) |
| `L0_Pitfalls` | salle de fosses : dalles percées, sols pleins, bordures, puits, fonds |
| `L0_Mission` | porte de service, cadre, battant (poignée parentée au battant), charnières, sas, filet de lumière, marques de L. |
| `L0_Lighting` | 313 lumières de surface (une par néon allumé, 55 W, blanc chaud à peine verdâtre ; 14 néons en panne sans lumière), lampe froide du sas |
| `L0_Collisions` | volumes `UCX_<module>_NN`, en fil de fer, hors rendu |
| `L0_GameplayMarkers` | repères (vides nommés, propriétés `kind` et `description`) et caméras |
| `L0_Archive` | modules archivés, hors rendu |
| `L0_RenderOnly` | aides des rendus seulement : lampe du joueur (vue de la porte), soleil et tracé du chemin (vue d'ensemble) ; masqués ailleurs |

Repères (174 pour la graine 9) : `MK_PlayerStart` et trois places de coéquipiers, `MK_ServiceDoor` (entrée côté pièce),
`MK_ServiceDoor_Pivot` (axe des charnières), `MK_ServiceDoor_Exit` (sas), `MK_ChalkMark_NN`, `MK_PitRoom_*`,
`MK_PitKill_*`, `MK_Item_NN_<objet>` (objets tirés par la graine), `MK_Hiding_NN` (cachettes), et
`MK_DoorCandidate_NNN` (120 pour la graine 9) : un repère par emplacement possible de la porte de service, au pied du mur,
tourné vers la pièce. Sa propriété `description` donne la cellule, la face, la distance de marche depuis le départ et la
**bande** du tirage : 0 = 24 à 40 cellules (85 candidats pour la graine 9), 1 = bande élargie 16 à 56 (35), 2 = repli
(aucun ici). Les repères sont triés par bande, puis par distance ; celui de la porte d'exemple porte la mention « exemple
pose dans cet export ». L'hôte tire d'abord dans la bande 0, puis 1, puis le repli.

**Nommage** : `SM_L0_*` pour les maillages ; `UCX_<maillage>_NN` pour les collisions convexes (convention d'Unreal) ;
`M_L0_*` pour les matériaux ; `T_L0K_*` pour les textures ; instances `L0_<type>_<cellule>`.

## 7. Matériaux et textures

| Matériau | Textures | Taille |
|---|---|---|
| `M_L0_Wallpaper` | `T_L0K_Wallpaper_BaseColor`, `_ORM`, `_Normal` | 2048 |
| `M_L0_Wallpaper_Aged` | mêmes cartes, teinte multipliée (0,93 ; 0,90 ; 0,82) | 2048 |
| `M_L0_Carpet` | `T_L0K_Carpet_*` | 2048 |
| `M_L0_CeilingTile` | `T_L0K_CeilingTile_*` | 2048 |
| `M_L0_Concrete` | `T_L0K_Concrete_*` (dalles, sas, étalon) | 2048 |
| `M_L0_PitConcrete` | `T_L0K_Concrete_*` × couleur de sommet `Col` (profondeur du puits) | 2048 |
| `M_L0_Baseboard` | `T_L0K_Baseboard_*` | 1024 |
| `M_L0_FixtureMetal` | `T_L0K_FixtureMetal_*` | 1024 |
| `M_L0_LightDiffuser` | `T_L0K_Diffuser_Emissive` (couleur et émission) ; `_Off` : diffuseur éteint | 512 |
| `M_L0_DoorSteel` | `T_L0K_DoorSteel_*` (porte peinte) | 1024 |
| `M_L0_DoorHardware` | `T_L0K_Hardware_*` (métal ; ORM et Normal en PNG) | 1024 |
| `M_L0_DraftGlow` | émissif (0,7 ; 0,85 ; 1) | — |
| `M_L0_PitDark`, `M_L0_Chalk`, `M_L0_Archive_Glitch` | couleurs unies | — |
| `M_L0_DecalWall`, `_DecalCeiling`, `_DecalCarpet` | `T_L0K_Decal_*` (RGBA, alpha = masque) | 1024 |

**Conventions** :

- BaseColor : sRGB.
- **ORM** (linéaire) : **R = occlusion ambiante, G = rugosité, B = métal**. Seule la quincaillerie est métallique.
- **Normal** : espace tangent, **convention DirectX** (vert vers le bas), comme les cartes du projet. Unreal l'utilise
  telle quelle ; le matériau Blender inverse le vert (nœuds `Separate Color`, `1 − G`, `Combine Color`).
- Émissif : `T_L0K_Diffuser_Emissive`.
- Formats : JPEG pour les surfaces peu brillantes (BaseColor qualité 92 ; ORM 95 et Normal 90 sans sous-échantillonnage
  des couleurs), PNG pour le métal et les décals. Total 30 Mo, 29 fichiers.

**Branchements dans Unreal** (préparé, non exécuté) :

| Texture | Réglage de la texture | Entrée du matériau |
|---|---|---|
| `*_BaseColor` | sRGB | Base Color |
| `*_ORM` | **sRGB décoché** (compression par défaut : *Masks* exigerait un échantillonneur *Masks* dans le matériau) | R → Ambient Occlusion, G → Roughness, B → Metallic, **un seul échantillonnage** |
| `*_Normal` | compression *Normalmap* (sRGB décoché), **Flip Green Channel décoché** (déjà DirectX) | Normal |
| `T_L0K_Diffuser_Emissive` | sRGB | Emissive Color × intensité |
| `T_L0K_Decal_*` | sRGB, alpha | matériau translucide ou masqué (Opacity / Opacity Mask = A) |
| couleur de sommet (`M_L0_PitConcrete`) | — | Vertex Color × Base Color |

## 8. Export FBX et import dans Unreal

**Export** (exécuté, `bpy.ops.export_scene.fbx`) : un fichier par module (le module et ses `UCX_`), module ramené à
l'origine ; `object_types = MESH` ; `apply_unit_scale` et `apply_scale_options = FBX_SCALE_UNITS` (échelle d'objet
1,0 côté Unreal, 1 m = 100 cm) ; `axis_forward = -Z`, `axis_up = Y` (réglages par défaut de Blender, convertis par
Unreal) ; lissage par face ; tangentes exportées ; couleurs de sommet linéaires ; textures non intégrées
(`path_mode = STRIP`).

**Relecture** (exécutée) : chaque FBX est réimporté dans Blender ; mêmes dimensions (à 1 mm près), même nombre de
`UCX_`, mêmes matériaux : **38 sur 38** conformes (`build_report.json`, `fbx_reimport`).

**Import dans Unreal 5** (préparé, non exécuté) :

| Réglage | Valeur |
|---|---|
| Import Uniform Scale | 1,0 |
| Convert Scene / Convert Scene Unit | activés |
| Force Front XAxis | désactivé |
| Combine Meshes | activé (un module par fichier) |
| Auto Generate Collision | **désactivé** (les `UCX_` sont importés comme collision) |
| Normal Import Method | Import Normals and Tangents |
| Generate Lightmap UVs | **désactivé** : le canal `UVLightmap` (UV1) est fourni ; Light Map Coordinate Index = 1 |
| Import Materials / Textures | au choix ; puis brancher les `T_L0K_*` comme au § 7 |
| Vertex Color Import Option | Replace (pour `SM_L0_Pit_Shaft`) |

**UV** : UV0 (`UVMap`) à l'échelle du monde (texture tuilée, tailles du § 7 : papier peint 1,2 m, moquette 2,2 m,
dalles 1,2 m, béton 3 m) ; UV1 (`UVLightmap`) sans chevauchement (`lightmap_pack`, marge 0,2), pour les lightmaps.

## 9. Placement (`L0_Placement.json`)

Une ligne par instance et par repère. Chaque entrée donne :

- `location_m`, `rotation_deg` (Blender) ;
- `unreal.location_cm`, `unreal.rotation_deg_pitch_yaw_roll` (Unreal) ;
- `asset` (module), `collection`, `kind` (`decor`, `pitfall`, `narrative`, `gameplay`).

**Conversion** (repère Blender en mètres, main droite, Z vers le haut → Unreal en centimètres, main gauche) :

```
x_ue = 100 x      y_ue = -100 y      z_ue = 100 z      lacet_ue = -lacet_blender
```

Avec cette conversion, la cellule (X, Y) du jeu a son centre en ((X + 0,5) × 350, (Y + 0,5) × 350) cm, comme dans
`ABRWorld::CellCenter`. En tête du fichier : graine, grille, comptes, porte de service (cellule, direction, pied, lacet,
longueur du chemin, marques), salles de fosses (centres des trous).

Graine 9 : 5205 instances, 174 repères (dont 120 candidats de porte) ; 824 murs, 416 passages, 929 poteaux,
43 piliers, 999 sols, 1024 plafonds, 313 néons allumés et 14 en panne, 1 salle de fosses (16 trous), 136 décals ;
porte de service d'exemple en (−16, 6), mur ouest, chemin de 28 cellules, 7 marques de craie posées (une cellule marquée
sans mur pour la porter n'en a pas, comme dans le jeu).

## 10. Contrôles (exécutés à chaque construction)

| Contrôle | Résultat (graine 9) |
|---|---|
| Étalon de 1 m | 1,000 × 1,000 × 1,000 m |
| Normales : faces de chaque boîte vers l'extérieur, enroulement cohérent entre faces voisines, aucune face nulle | 0 défaut sur 34 modules |
| Trous ouverts : un rayon vertical au centre de chaque trou ne touche que le fond, 14 m plus bas | 16 sur 16 |
| Chemin du départ à la porte : rayons à 1 m de haut entre centres de cellules, au centre et à ±42 cm (capsule) | 28 pas, 0 obstacle |
| Relecture des FBX | 38 sur 38 |

## 11. Rendus (`Renders/`)

Cycles, 1280 × 720 (vue d'ensemble : 720 × 720), 48 échantillons, débruitage OpenImageDenoise, AgX, exposition +0,9.
Caméras à hauteur d'œil (1,62 m) sauf la vue d'ensemble.

| Fichier | Vue |
|---|---|
| `L0_Couloir.jpg` | le plus long couloir éclairé près du départ |
| `L0_SalleRepetitive.jpg` | la salle de départ, piliers et plafond répétés |
| `L0_Intersection.jpg` | une intersection en T vue depuis son pied |
| `L0_Fosses.jpg` | la salle de fosses depuis son coin |
| `L0_PorteDeService.jpg` | la porte de service **entrouverte** (38°) sur le sas et sa lampe froide, éclairée par la lampe du joueur (la cellule de la porte est dans une zone sans néon pour cette graine) |
| `L0_VueEnsemble.jpg` | plan vu de dessus (toute la zone de 112 m), plafonds masqués, soleil de lecture ; tracé du chemin de L., pastilles du départ (au centre) et de la porte (mur ouest). Le nord de Blender (+Y) est le −Y du jeu : la carte est retournée haut-bas par rapport aux planches de `verify_pitfalls.py` |

## 12. Limites

- **Import Unreal non validé** (pas d'Unreal ici) : orientation, échelle, collisions `UCX_`, lightmaps et matériaux sont à
  vérifier à l'import.
- La disposition vient de la simulation Python du générateur : elle n'a pas été comparée à une capture du jeu.
- Le jeu construit le Level 0 lui-même (`ABRChunk`) : ce kit est un **asset de production** (modules, matériaux, scène de
  référence), pas un niveau chargé par le jeu.
- La porte de la scène est un exemple : dans le jeu, son emplacement change à chaque nouvelle partie (tirage de l'hôte,
  sauvegardé). Les candidats exportés sont ceux de la simulation Python pour la graine 9.
- Lumières : surfaces de Blender (55 W par néon), réglées à l'œil sur les rendus ; ce ne sont pas les intensités
  d'Unreal.
- Modules par boîtes : arêtes vives, pas de chanfreins ni de LOD. Décals : simples plans.
- Rendus sur processeur, 48 échantillons : un peu de bruit débruité dans les zones sombres.
