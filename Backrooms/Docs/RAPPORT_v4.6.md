# Rapport v4.6 : fosses du Niveau 0, ce qui a été fait, vérifié, et ce qui reste à vérifier

État des lieux : [`AUDIT_v4.6.md`](AUDIT_v4.6.md). Images et données : `Docs/v46/`.

## 1. Environnement

| Élément | Valeur |
|---|---|
| Machine | conteneur Linux, 4 cœurs, 15 Go de RAM, **pas de GPU**, **pas d'Unreal Engine** |
| Moteur visé | Unreal Engine 5.8 (`Backrooms.uproject` inchangé) |
| Outils | Blender 5.0.1 (module Python `bpy`, Cycles sur processeur), clang (syntaxe C++ avec des en-têtes Unreal simplifiés), Python 3, numpy, Pillow |
| Réseau | page du wiki bloquée par le proxy du conteneur (la description de la demande a servi de référence) |

**Aucune capture du jeu, aucune compilation Unreal, aucune mesure d'images par seconde** n'a pu être faite.

- Les images de `Docs/v46/` sont des **rendus Blender** ou des **cartes calculées en Python**, et chaque image le dit.
- Les contrôles C++ avec en-têtes simplifiés et la simulation Python de la génération vérifient la logique.
- **Ils ne valident pas le jeu.**

## 2. Ce qui a changé

### Salles de fosses (« Hole Variation »)

- **Forme.**
  - Une salle jaune carrée de 5 × 5 cellules (17,5 m), inscrite dans un chunk.
  - Une **galerie** d'au moins une cellule la contourne.
  - Ses murs sont percés d'une porte par côté.
  - Dedans, une fosse carrée de 2 m à chaque coin intérieur de cellule, soit une grille de 4 × 4 (88 % des coins
    percés : les grilles ont de 11 à 16 fosses).
  - Les **passages** de 1,5 m passent par le centre des cellules, donc là où l'A* fait déjà marcher les entités.
- **Ambiance.** Papier peint, moquette, plafond, plinthes et néons du Niveau 0.
  - Les néons sont alignés une cellule sur deux, au-dessus des croisements de passages, jamais au-dessus du vide.
  - Ils projettent des ombres : les bords découpent la lumière, et rien n'éclaire les parois à travers la dalle.
- **Géométrie** (`ABRChunk::BuildPitRoom`).
  - **Dalle** : la dalle du chunk (30 cm d'épaisseur) est découpée en rectangles autour des ouvertures, entre 18 et
    22 boîtes par chunk. **Aucune collision au-dessus du vide.**
  - **Parois des puits** (30 cm), par bandes :
    1. tranche de moquette (4 cm) ;
    2. tranche de dalle en béton clair ;
    3. béton de plus en plus sombre (albédo 0,34, puis 0,15, puis 0,045) ;
    4. un fond noir à 14 m, qui arrête la chute.
  - La profondeur disparaît dans le noir sans grain, VHS ni bloom. Aucune lumière dans les puits : seuls les néons et
    la lampe les éclairent.
  - La dalle s'arrête 1 cm derrière les parois, pour qu'aucune face ne se confonde avec une autre.
- **Paramètres** (`FBRLevelDef`, réglés dans `BRLevels.cpp`, `Level0()`) :

  | Paramètre | Rôle | Niveau 0 |
  |---|---|---|
  | `PitRoomChance` | fréquence par chunk | 0,08 |
  | `PitRoomsMin` | nombre minimum garanti par graine | 1 |
  | `PitRoomCells` | taille de la salle (cellules) | 5 |
  | `PitHoleSize` | côté d'une fosse | 200 cm |
  | `PitPassage` | largeur minimale des passages (reste ≥ 120 cm) | 150 cm |
  | `PitHoleChance` | part des coins percés | 0,88 |
  | `PitDepth` | profondeur jusqu'au fond | 1 400 cm |
  | `PitKillDepth` | profondeur mortelle | 450 cm |
  | `PitLipThickness` | épaisseur de la dalle | 30 cm |
  | `PitDoorsPerSide` | portes par côté | 1 |

  Le Niveau 0 garde ses **112 m** (4 × 4 chunks) ; les 12 chunks hors départ sont candidats. Les autres niveaux n'ont
  pas de fosses (`PitRoomChance = 0`).

### Génération (déterminisme, réservations)

- **Déterminisme.**
  - Tout se déduit de la graine et des coordonnées.
  - Niveau fini : la liste des salles est calculée au chargement (`ABRWorld::PrepareLevelLayout`), avant tout chunk.
    Elle est la même chez chaque joueur, quel que soit l'ordre de chargement.
  - Niveau infini : un tirage par chunk.
- **Réservation.**
  - Les règles d'arêtes (`EdgeE` / `EdgeN` → `PitEdge`) ouvrent la salle et sa galerie, et murent son pourtour avant
    tout le reste.
  - Pas de pilier dans la salle ni à son pourtour.
  - Les cachettes, les objets, les cassettes, les sorties et les accessoires évitent les cellules de la salle
    (`PickFreeCell`, `BuildHidingSpots`, `PlanExits`).
  - Les entités terrestres n'y apparaissent pas.
- **Défaut v4.5 corrigé : niveau mal relié** (voir l'état des lieux).
  - Au chargement, une **porte est percée** dans le mur qui sépare la zone atteinte d'une zone isolée, jusqu'à ce que
    toute cellule hors salle soit atteignable depuis le départ **sans traverser de salle de fosses**.
  - Le mur choisi est celui de plus petit hachage : le même chez tous.
  - Cela fait en moyenne 58 portes de plus par Niveau 0 (de 36 à 84 sur 24 graines).
  - Cassettes et sorties ne sont placées que dans ces cellules. Si les essais tirés tombent mal, tout le chunk est
    balayé : le nombre de cassettes et de sorties reste garanti.

### Chute (serveur) et réapparition

- **Physique.** Rien ne retient le joueur au-dessus d'une fosse : il tombe dès que le centre de son corps dépasse le
  bord d'environ 18 cm (`PerchRadiusThreshold`).
- **Validation par le serveur.**
  - `ABRWorld::UpdatePitFalls` (serveur seulement) examine la position de chaque joueur, telle que le serveur la
    connaît.
  - Sous 4,50 m, au-dessus d'une fosse, il appelle `ABRCharacter::NotifyFellIntoPit`.
  - Pour un client, cela passe par la RPC `ClientFellIntoPit`, puis par `Die`, le **système de mort existant**.
- **Corps et réanimation.**
  - Le corps continue de tomber jusqu'au fond.
  - **Pas de réanimation** pour une mort par chute : un corps sous le sol est ignoré par la recherche de coéquipier,
    et le serveur refuse la réanimation.
  - Les coéquipiers lisent « X est tombé dans une fosse ».
- **Pas de chute infinie.**
  - Le fond a une collision.
  - Un garde-fou tue aussi tout joueur sous 20 m.
  - Une entité tombée est retirée.
- **Pas une sortie.**
  - Seul : retour à un Niveau 0 neuf, avec les objectifs remis à zéro (comportement existant).
  - En équipe : réveil au point de départ du niveau.
  - Le point de départ n'est jamais dans un chunk à fosses (garde-fou en plus dans `SpawnSpot`).
- **Mode développeur invincible.** Une chute ramène au croisement de passages le plus proche.

### IA

- **A*** : traverser une salle de fosses coûte 3 au lieu de 1. Les entités prennent la galerie quand elle n'est pas
  beaucoup plus longue ; dans la salle, elles suivent les centres de cellule, qui sont des passages.
- **Raccourcis** : `IsDirectPathClear` refuse toute ligne droite qui passe à moins de (rayon + 10 cm) d'une fosse
  (`SegmentCrossesPit`, test segment contre carré).
- **Navigation locale.**
  - Les points de passage sont validés à 28 cm au lieu de 70 dans une salle : l'entité tourne au croisement.
  - Dans une même cellule, elle passe par le croisement plutôt que de couper un coin.
  - Si le pas suivant mène au-dessus du vide, `MoveTowards` garde la composante X ou Y qui reste sur le passage,
    sinon elle s'arrête.
- **Dernier garde-fou.** Dans un niveau à fosses, une entité terrestre ne peut pas quitter le sol
  (`bCanWalkOffLedges = false`).
- **Clump** : une main d'appui sans sol dessous (bord de fosse) reste en l'air au lieu de se poser dans le vide.
- **Entités volantes** (Smiler, Deathmoth) : elles survolent les fosses.

### Mode développeur et graine de démonstration

- **Graine de démonstration : 9** (`-BRSeed=9` ou console `BRSeed 9`).
  - Salle de 16 fosses aux cellules (-14, -7) à (-10, -3), à 14 cellules du départ.
  - En v4.5, cette graine enfermait le départ dans 294 cellules.
- **Touche Suppr** (mode développeur), ou console **`BRPits`** : placement au coin de la salle de fosses la plus proche,
  regard en diagonale sur la grille. Hors du Niveau 0, charge d'abord le Niveau 0 avec la graine 9.
- Console **`BRSeed <n>`** : recharge le niveau avec la même graine que `-BRSeed=<n>` (hôte seulement).

### Qualité

- **Appuis au sol** (`ABREntity::MeasureFeet` / `LowestFootHeight`).
  - Le corps de chaque entité à jambes se cale à chaque image pour que le pied le plus bas touche le sol : c'est la
    jambe d'appui, genou tendu.
  - Plus de pieds qui flottent quand les jambes s'écartent (jusqu'à 7,9 % de la taille du Wretch en poursuite en
    v4.5), ni de pied sous le sol quand les genoux se plient pour l'armé.
  - Cela remplace le rebond et le tassement fixes de la v4.5.
- **Hound allégé** : `SK_HoundLite.fbx`, un nouveau dérivé (`Tools/Blender/build_hound_lite.py`). `SK_Hound.fbx`
  reste intact et protégé.

  | Fichier | Sommets | Triangles | Cheveux (triangles) |
  |---|---|---|---|
  | `SK_Hound` (fourni, Cinématique) | 175 201 | 319 000 | 281 248 |
  | `SK_HoundLite` (Performance, Qualité) | 89 381 (−49 %) | 164 524 (−48 %) | 126 772 |

  - Le dérivé garde 45 % des mèches (choix déterministe), deux fois plus larges.
  - Corps (37 752 triangles), squelette (écart des os : 0,0000 m), groupes de sommets et matériaux sont identiques.
  - Comparaison : `Docs/v46/blender_hound_original_vs_lite.jpg`.
  - Le jeu revient à `SK_Hound` si le dérivé manque. Les LOD sont générés à l'import (3).
- **Chambranles** : jambages et traverse en saillie autour des portes du Niveau 0, sur les deux faces (`bDoorCasings`).

### Test automatique

- **Scénario « fosses »** : inclus quand le Niveau 0 est testé, ou seul avec `-BRAutoTestPits`. Il :
  - charge le Niveau 0 avec `-BRSeed` (sinon la graine 9) ;
  - note les salles (même présentation que `verify_pitfalls.py`, pour comparer) ;
  - vérifie les points d'apparition ;
  - capture le point de vue ;
  - **mesure la salle dans chaque profil** (Performance, Qualité, Cinématique), puis capture en Lumen logiciel ;
  - capture la lampe plongée dans une fosse ;
  - fait traverser la salle à une Bacteria, en suivant à chaque image son point le plus bas, les images passées
    au-dessus du vide et sa distance au joueur ;
  - fait **marcher le joueur** d'un croisement vers une fosse (chute physique), vérifie la mort par chute, le corps au
    fond et le réveil hors des fosses ;
  - rétablit les réglages du joueur.
- `Mesures.csv` a une colonne `scene` en plus : `vue`, `fosses_Performance`, `fosses_Qualite`, `fosses_Cinematique`.
- Test multijoueur (`-BRNetTest`) : chaque machine note ses salles de fosses avec le motif des trous. L'hôte et le
  client doivent avoir exactement les mêmes.

## 3. Vérifié hors moteur, et comment

| Vérification | Méthode | Résultat |
|---|---|---|
| Présence, placement, accessibilité, raccords, sur **24 graines** (`-BRSeed=1` à `24`) | `python Tools/verify_pitfalls.py --seeds 1-24` : portage Python fidèle des règles C++ (même hachage, mêmes arêtes, mêmes tirages de cachettes, cassettes et sorties, même découpage de la dalle) ; paramètres lus dans `BRLevels.cpp` | **24 / 24 OK** (`Docs/v46/verification_24_graines.txt`, cartes : `Docs/v46/cartes_24_graines.png`). Les contrôles sont détaillés sous ce tableau. |
| Mêmes contrôles sur 300 graines | même script, `--seeds 1-300` | 300 / 300 OK, 12 000 chemins d'entités (128 984 segments), aucun au-dessus du vide (`Docs/v46/verification_300_graines.txt`) |
| Navigation dans et autour des salles | A* portée en Python (coût 3 dans la salle), 40 trajets par graine depuis, vers et autour de la salle | 0 segment de chemin à moins de (60 + 10) cm du vide (Clump, la plus large entité terrestre) ; marge de 15 cm ; 0 traversée de salle pour les trajets qui la contournent |
| Géométrie | rendu Blender des boîtes exportées par le script (`Tools/Blender/render_pitroom.py`), graine 9 | `Docs/v46/blender_fosses_vue.jpg`, `_lampe.jpg`, `_coupe.jpg` : bords épais, parois, profondeur sombre, raccords. **Éclairage approché : pas Lumen** |
| Rigs v4.5 en mouvement | `Tools/Blender/check_rig_motion.py`, FBX livrés | aucun défaut justifiant une reconstruction (état des lieux) |
| Pieds qui flottent (v4.5) | même pose, point le plus bas du maillage | `Docs/v46/pieds_v45_sans_appui.json` |
| Hound allégé | `build_hound_lite.py`, comparaison automatique | voir le tableau du § 2 |
| Syntaxe C++ | clang `-fsyntax-only -Wshadow-all`, en-têtes Unreal simplifiés | aucune erreur dans le code v4.6. Les fichiers déjà touchés par des manques des en-têtes simplifiés en v4.5 le restent (`BRAutoTest`, `BRConfig`, `BRGameMode`, `BRMaterialBuilder`, `BRWaterSim`) : les erreurs restantes n'y concernent que des API absentes des en-têtes simplifiés |
| Masquage de membres (erreur sous Unreal) | script `shadowcheck` | 0 |
| C++ en ASCII | `grep -P '[^\x00-\x7F]'` | OK |
| Modèles fournis | `python Tools/protect_assets.py check` | OK (`SK_HoundLite` ajouté au manifeste) |

Contrôles faits pour chaque graine par `verify_pitfalls.py` :

- au moins une salle, jamais dans les chunks du départ, avec des fosses et des portes ;
- une galerie continue autour ;
- la salle est accessible ;
- aucun mur, pilier ni néon au-dessus d'une fosse ;
- la dalle couvre exactement le chunk moins les ouvertures : surface exacte, aucun recouvrement, aucune dalle
  au-dessus d'une fosse, bords du chunk couverts sur toute leur longueur, donc raccord avec les chunks voisins ;
- les 4 points d'apparition sont hors des fosses ;
- 8 cassettes pour 6 exigées ;
- 2 murs glitchés et 2 échelles ;
- cassettes, sorties et cachettes hors des salles ;
- **toutes les cellules hors salle sont atteignables depuis le départ sans traverser de salle** (999 sur 1 024 : les 25
  autres sont la salle).

## 4. Non vérifié (à faire sur un PC Windows avec Unreal 5.8)

1. **Compilation** :
   `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" BackroomsEditor Win64 Development -Project="C:\...\Backrooms.uproject" -WaitMutex`
   Les nouvelles API à surveiller :
   - `UCharacterMovementComponent::PerchRadiusThreshold` et `bCanWalkOffLedges` ;
   - `FIntRect` ;
   - `TSet` parcouru avec `for` ;
   - `AActor::GetActorTransform`.
2. **Import** : à l'ouverture de l'éditeur, `SK_HoundLite` est importé avec 3 niveaux de détail. Le journal doit dire
   « 8 maillages à squelette importés ».
3. **Salle de fosses en jeu** :
   `UnrealEditor.exe Backrooms.uproject -game -windowed -ResX=1920 -ResY=1080 -BRSeed=9`. En mode développeur,
   **Suppr** place le joueur au bord de la salle. Vérifier :
   - **ombres** des bords (néons) ;
   - **exposition** : la salle ne doit pas être plus sombre ou plus claire que le reste ;
   - **fuites de lumière** sur les parois sous la dalle ;
   - profondeur noire ;
   - lampe (**F**) qui accroche les parois proches ;
   - **Lumen matériel**, puis **logiciel** : paramètre « RT matériel » ; vue *Lit → Lumen → Surface Cache* pour la
     couverture des parois.
4. **Test automatique de la salle** (captures, chute, IA, mesures par profil) :
   ```bat
   UnrealEditor.exe Backrooms.uproject -game -windowed -ResX=1920 -ResY=1080 -BRAutoTest -BRAutoTestPits -BRSeed=9 -BRAutoTestGPU
   ```
   Résultats dans `Saved/AutoTest/` :
   - `Rapport.txt` : section « Salle de fosses » ;
   - `Mesures.csv` : lignes `fosses_*` ;
   - captures `L00_fosses_vue / _Performance / _Qualite / _Cinematique / _Lumen_logiciel / _lampe / _IA / _chute.png`.

   Le rapport doit montrer :
   - la même salle que `python Tools/verify_pitfalls.py --seed 9` : `-14,-7->-10,-3`, 16 fosses ;
   - une mort par « une chute dans une fosse » en moins de 3 s ;
   - un corps au fond (Z ≈ −1 400) ;
   - un réveil hors des fosses ;
   - une Bacteria jamais au-dessus du vide.
5. **Coopération** : `-BRNetTest` (README § 9) avec `-BRSeed=9` sur l'hôte. Les lignes « salles de fosses » des deux
   rapports (`Saved/NetTest_Hote`, `Saved/NetTest_Client`) doivent être identiques. À 2 puis 4 joueurs, à la main :
   - un client tombe : mort constatée par l'hôte ;
   - « X est tombé dans une fosse » chez les autres ;
   - pas de réanimation possible ;
   - réveil au départ.
6. **Anciennes sauvegardes** : charger une partie v4.5. Le format est inchangé (pas de position ni de graine
   enregistrées) : la partie reprend au point de départ d'un Niveau 0 neuf.
7. **Appuis au sol** : regarder marcher et courir le Wretch, le Skin-Stealer et le Hound (**F10** fait défiler les
   jumpscares ; `BRSpawn 5`, `BRSpawn 3`, `BRSpawn 1`). Les pieds doivent rester au sol sans tressauter.
8. **Empaquetage** :
   `RunUAT.bat BuildCookRun -project="C:\...\Backrooms.uproject" -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory="C:\Builds\Backrooms"`

## 5. Mesures

**Aucune mesure en jeu n'a pu être faite.** À remplir avec `Saved/AutoTest/Mesures.csv` (étape 4 ci-dessus) :

| Scène | Profil | img/s | 1 % bas | médiane (ms) | GPU (ms) | lumières (ombres) |
|---|---|---|---|---|---|---|
| Salle de fosses (graine 9) | Performance | non mesuré | | | | |
| Salle de fosses (graine 9) | Qualité | non mesuré | | | | |
| Salle de fosses (graine 9) | Cinématique | non mesuré | | | | |

### Coût attendu de la salle, compté sur la géométrie générée (graines 1 à 24)

| Élément | Nombre |
|---|---|
| Dalles du chunk | 18 à 22 boîtes (au lieu d'1) |
| Parois et fonds des puits | 21 boîtes par fosse, 336 au plus, en 6 lots d'instances (une par surface) |
| Néons allumés dans la salle | 5 à 9, **tous avec ombres** (contre 15 % des néons ailleurs au Niveau 0) |
| Portes percées pour relier le niveau | 36 à 84 (moyenne 58) |

Le principal coût à surveiller, ce sont ces **néons à ombres** : jusqu'à 9 lumières surfaciques à ombres virtuelles dans
la même pièce. Si `stat gpu` montre des *ShadowDepths* trop chers, deux réglages sont possibles dans `CellLight`,
cas `Room` :

- `L.bShadow` une lampe sur deux ;
- `Broken` plus fréquent.

## 6. Fichiers modifiés ou ajoutés

**C++**

- `BRTypes.h` :
  - paramètres `Pit*` ;
  - `bDoorCasings`.
- `BRLevels.cpp` : Niveau 0, fosses et chambranles.
- `BRWorld.h/.cpp` :
  - salles de fosses (`HasPits`, `ComputePitRoom`, `GetPitRoom`, `IsPitRoomCell`, `HasPitAtCorner`, `IsOverPit`,
    `SegmentCrossesPit`, `PitEdge`, `GetPitRooms`, `FindPitRoomView`) ;
  - `PrepareLevelLayout` : salles, portes de raccord, accessibilité ;
  - `UpdatePitFalls` ;
  - `FloorZAt` ;
  - coût A* ;
  - néons de la salle ;
  - piliers ;
  - apparitions ;
  - `SeedFromUser`, `DemoSeed` ;
  - `RequestTransition` avec graine ;
  - `HandlePlayerDeath(bNoRevive)`.
- `BRChunk.h/.cpp` :
  - `BuildPitRoom` ;
  - `PickFreeCell` ;
  - réservations (cachettes, objets, sorties, accessoires) ;
  - balayage complet des sorties dans un niveau fini ;
  - chambranles.
- `BRCharacter.h/.cpp` :
  - `NotifyFellIntoPit` / `ClientFellIntoPit` / `FallDeath` ;
  - corps qui continue de tomber ;
  - pas de réanimation au fond ;
  - message aux coéquipiers ;
  - `PerchRadiusThreshold`.
- `BREntity.h/.cpp` :
  - navigation (raccourcis, points de passage, pilotage au bord, `bCanWalkOffLedges`) ;
  - entité tombée retirée ;
  - mains du Clump ;
  - appuis au sol ;
  - `SK_HoundLite`.
- `BRPlayerController.h/.cpp` : `BRPits`, `BRSeed`, touche Suppr.
- `BRHUD.cpp` : aide du mode développeur.
- `BRAutoTest.h/.cpp` :
  - scénario « fosses » ;
  - `-BRAutoTestPits` ;
  - mesure factorisée (`StartMeasure` / `EndMeasure`) ;
  - colonne `scene` ;
  - salles notées dans le test multijoueur.

**Python et outils**

- `Content/Python/backrooms_setup.py` : niveaux de détail de `SK_HoundLite`.
- Nouveaux :
  - `Tools/verify_pitfalls.py` ;
  - `Tools/Blender/render_pitroom.py` ;
  - `Tools/Blender/check_rig_motion.py` ;
  - `Tools/Blender/build_hound_lite.py`.
- `Tools/protect_assets.py` : `SK_HoundLite` protégé.

**Ressources**

- `RawAssets/Skeletal/SK_HoundLite.fbx` (3,9 Mo).
- `skeletal_models.json`.
- Manifeste de protection.

**Docs** : `AUDIT_v4.6.md`, ce rapport, `Docs/v46/*`.

## 7. Limites restantes

- **Rien n'a été vu dans le moteur.** Exposition, ombres, Lumen et fuites dans les puits ne sont jugés que sur des
  rendus Blender à l'éclairage approché.
- **Brouillard.** Si le brouillard volumétrique est désactivé dans les paramètres, le brouillard ordinaire du Niveau 0
  (couleur jaune) peut éclaircir le fond des puits. Le volumétrique, actif par défaut, n'éclaire que là où il y a de la
  lumière.
- **Portes de raccord.** Elles changent un peu le dessin du Niveau 0 (en moyenne 58 portes de plus). Elles suppriment
  les poches fermées : la graine 1 enfermait le départ.
- **Navigation locale minimale.** Une entité poussée hors d'un passage (collision avec une autre entité) ne peut pas
  tomber (`bCanWalkOffLedges`), mais elle peut rester bloquée au bord jusqu'au calcul suivant de son chemin (0,7 s).
- **Appuis au sol** : le corps descend sur la jambe d'appui, mais il n'y a pas d'IK des pieds sur les marches.
  Les niveaux à marches (Poolrooms) n'ont pas d'entités.
- **Hound allégé** : chevelure un peu moins fournie de près (voir la comparaison). Le profil Cinématique garde
  l'original.
- **Bacteria et Deathmoth** : toujours en pièces rigides, comme en v4.5.
