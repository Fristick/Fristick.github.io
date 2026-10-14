# Rapport v4.14 : parties protégées, scanner LiDAR fiable, kit du Level 0 branché

Travail fait le 14 octobre 2026 sur la branche `backrooms`.

| | |
|---|---|
| **Commit de base** | `b8f9449` (v4.13 suite 6), vérifié avant toute modification ([audit](AUDIT_v4.14.md)) |
| **Commits v4.14** | `4a53b64` (1/n) : audit, rejeu isolé, migration prouvée, scanner fiabilisé ; `3ef8ee3` (2/n) : kit du Level 0 importé et branché, HUD du scanner contextuel ; `0b8a60c` (3/n) : tests du jeu v4.14, versions, journaux, documents ; (4/4) : ce tableau |
| **Documents** | [Audit](AUDIT_v4.14.md) · [Gameplay v4.14](GAMEPLAY_v4.14.md) · [Gameplay v4.13](GAMEPLAY_v4.13.md) (règles inchangées) · [Kit Blender du Level 0](../RawAssets/Level0Kit/README.md) · journaux réels dans [`Docs/v414/`](v414/) |

**Ordre suivi** (celui du prompt) : protéger les parties, fiabiliser le LiDAR, préparer la vérification dans Unreal ;
ensuite seulement le kit, le HUD et le reste.

## 1. Où, comment, et ce qui n'a pas pu être fait

**Machine.** Conteneur Linux x86_64, 4 cœurs, **sans GPU**. **Unreal Engine absent** : ni éditeur, ni UHT/UBT/UAT, ni
cook. Disponibles : g++ 13.3, clang 18.1, Python 3.11, module `bpy` 5.0.1. Les documentations `dev.epicgames.com`,
`partner.steamgames.com` et NVIDIA sont refusées par la politique réseau (**403**) ; **rien n'a été contourné**.

**Exécuté ici** (sorties réelles dans [`Docs/v414/`](v414/), commandes au § 9) :

| Quoi | Résultat | Journal |
|---|---|---|
| Sondes d'audit sur le code de la base (scanner, migration) | 2.2 et 2.3 reproduits | [`audit_sondes.txt`](v414/audit_sondes.txt) |
| Banc de l'histoire (fil, accès, migration, **preuve des Poolrooms**, **session garée**, **REJOUER**) | 71 vérifications, 0 échec | [`banc_histoire.txt`](v414/banc_histoire.txt) |
| Banc du scanner LiDAR (**débit par seconde, réserve, expiration, inscriptions, chemin réel `RunFrame`**) | 18 228 vérifications, 0 échec | [`banc_lidar.txt`](v414/banc_lidar.txt) |
| Banc de contenu (lots, canaux, **protocole 414**) | 69 vérifications, 0 échec | [`banc_contenu.txt`](v414/banc_contenu.txt) |
| Bancs des missions (3000 graines × 12 niveaux), mécanismes (49), départ (30), lumière (6), transactions (51) | 0 échec | `banc_*.txt` |
| **Pose du kit du Level 0** rejouée sur la grille simulée : 200 graines + comparaison à la scène du kit (graine 9) | OK (après une correction, § 4.3) | [`kit_pose.txt`](v414/kit_pose.txt) |
| Outils : paquets (62 tests, 1 sauté), langues (1187 clés × 22), relecture ciblée, modèles protégés, syntaxe Python | OK | [`outils.txt`](v414/outils.txt) |
| Contrôle syntaxique C++ (clang, en-têtes Unreal **simulés**) de 13 fichiers modifiés ; masquages de noms ; compilation regroupée | 0 erreur, 0 avertissement ; 1 conflit regroupé **déjà présent dans la base** | [`syntaxe.txt`](v414/syntaxe.txt) |

**Préparé, non exécuté** (rien de cette liste n'est compté comme réussi) :

- compilation **Development Editor** et **Shipping**, UHT, compilation regroupée et séparée ; cook ; paquets Windows,
  Linux, macOS ;
- tests du jeu : **`-BRAutoTestV414`** (nouveau, § 7), `-BRAutoTestV413` et les précédents, `-BRNetTest` ;
- **import du kit dans Unreal** (`backrooms_setup.import_level0_kit`, écrit et relu, jamais lancé) ;
- **captures dans Unreal** (Level 0, porte, joueur, carnet, inventaire, Poolrooms, Level 6) : aucune ; les rendus Blender
  du kit restent des rendus Blender ;
- **profilage sur RTX 4080 + Ryzen 7 7800X3D** : aucune mesure ; aucun chiffre d'images par seconde n'est annoncé ;
- essais à quatre joueurs réels, client tardif sous latence, reconnexion ;
- Steam : aucun envoi.

> Le jeu **n'est pas déclaré terminé** : les bancs hors moteur sont verts, mais rien n'a encore été compilé ni joué dans
> Unreal pour la v4.14.

## 2. Corrections prioritaires (cause, correction, comportement obtenu)

### 2.1 REJOUER et le mode développeur n'écrivent plus la partie de campagne

**Cause** ([audit § 2.1](AUDIT_v4.14.md)) : `WriteActiveSave` recopiait équipement, santé, journal, campagne et la mort
dans la partie **avant** de tester le mode ; `NotifyPlayerDeath`, le temps de jeu et `OnLevelLoaded` écrivaient dans tous
les modes ; une mort en REJOUER renvoyait au Level 0 de la campagne.

**Correction** :

- une règle unique, testable hors moteur : `BRStory::WritePolicyFor(ESession)` (`BRStoryLogic.h`). En session
  d'histoire, tout est écrit ; en **REJOUER**, seul le **journal** est complété (union : un document lu en rejeu reste
  lu, rien n'est retiré) ; en session **libre** (mode développeur), rien ;
- `ABRPlayerController::WriteActiveSave` applique cette règle **avant** toute copie ; morts, temps de jeu, niveau courant
  et découvertes passent par `WritesCampaign()` ;
- mort en REJOUER, en solo : le niveau rejoué est relancé (`ABRWorld`), plus le Level 0 de la campagne.

**Obtenu** : consommation, ramassage, blessure, mort, fin, pause, fermeture et minuterie en REJOUER laissent la partie
**identique** (équipement, santé, morts, session, niveau, campagne, histoire, temps). Banc de l'histoire, section I
(6 vérifications) ; `-BRAutoTestV414` relit le **fichier** avec `BRSaves::Load` après chaque étape (préparé).

### 2.2 La migration ne crédite plus de Poolrooms imaginaires

**Cause** : `MigrateLegacy` concluait à une traversée dès que `37` précédait `4` dans `Explored`, alors que le Niveau 4
était aussi atteignable depuis 1, 3, 5 et 11 (historique des sorties, [audit § 2.2](AUDIT_v4.14.md)) ; la session
garée était vidée avant que sa reprise ait réussi, et effacée à la sortie des Poolrooms même si son niveau n'était pas
disponible.

**Correction** (`BRStoryLogic.cpp`, `BRSave.cpp`, `BRPlayerController.cpp`, `BRStoryWorld.cpp`) :

- `PoolroomsProof` rend **Prouvée**, **Absente** ou **Ambiguë**. Prouvée seulement si : format connu (≥ 2, la console
  des v4.1–4.3 ajoutait les niveaux visités), 37 et 4 découverts dans cet ordre, et **aucun autre prédécesseur possible
  du Niveau 4** (1, 3, 5, 11) découvert avant lui. Seule une preuve crédite l'étape ;
- la session garée reste dans la sauvegarde jusqu'à la **confirmation** de sa reprise (`OnParkedRestored` au niveau
  chargé) ; `OnProgress` ne l'efface plus ; `WantsParked` et `ResumePoint` la reprennent dès que son niveau est
  disponible (canal public, puis ajout d'un chapitre) ;
- deux messages du menu expliquent l'attente (`Menu.ParkedWaiting`, `Menu.ParkedSaveV414`, 22 langues).

**Obtenu** : `[0, 1, 37, 4]` → traversée non créditée, Niveau 4 garé intact, reprise aux Poolrooms ; `[0, 37, 4, 5]`
au format 4 → prouvée ; formats 0 et 1 → ambigus. Banc G (8 vérifications ; la première est le contre-exemple de la
sonde d'audit), H (6), migration des anciennes parties (F, 23).

### 2.3 Scanner : débit par seconde, réserve par catégorie, expiration complète

**Cause** : 37 rayons de focus **par image** et 3000 points par groupe d'âge refusés une fois pleins : 10 % de refus à
60 images/s, 77,5 % à 240 ; une image de 13 s ne vidait qu'un groupe.

**Correction** (`BRLidarLogic.h/.cpp`, logique pure partagée par le jeu et le banc) :

- **rayons par seconde** : focus 1110 rayons/s (la cadence de la v4.13 à 30 images/s), 48 au plus par image ;
  impulsion de 1400 rayons étalée sur 0,2 s, 260 au plus par image (`FRateBudget`) ;
- **réserve par catégorie** (`FPointStore`) : décor 15 000, dangers 3000, mécanismes 1500, menaces 600, repères 400,
  inscriptions 3500 (24 000 au total, 8 groupes d'âge). Pleine : le **plus ancien** de la même catégorie est
  remplacé ; le décor ne peut jamais chasser un danger ;
- **expiration** : `Advance` vide **tous** les groupes échus, quelle que soit la durée de l'image ; l'âge n'est jamais
  raccourci.

**Obtenu** (banc, chemin réel `RunFrame`) :

| Focus 30 s | 30 img/s | 60 img/s | 144 img/s | 240 img/s |
|---|---:|---:|---:|---:|
| Points acceptés | 100 % | 100 % | 100 % | 100 % |
| Images sans point | 0 | 0 | 0 | 0 |
| Rayons par seconde | 1110 | 1110 | 1110 | 1110 |
| Avec saccades de 120 ms | 1027/s | 1068/s | 1054/s | 1065/s |

Saturation (impulsion à chaque recharge + focus, 90 s, persistance 30 s, 144 img/s) : décor 13 737 / 15 000,
165 729 remplacés, **0 refusé** ; un trou visé ensuite : 555 bords relevés en 0,5 s, 100 % acceptés. Longue image de
13 s pour 12 s de persistance : **0 point restant, 100 expirés**. Persistances 4 s et 30 s exactes à 0,2 s près.

### 2.4 Inscriptions : dédupliquées, budget commun, coordonnées en cache, mesurées

**Cause** : chaque rayon focalisé sur une porte dessinait toute la plaque (629 à 1073 points par image) et relevait la
plaque à chaque rayon ; le banc réservait 377 points sans passer par ce chemin ; 48 matériaux réécrits à chaque image.

**Correction** : `FGlyphPlanner` (une inscription par cible et par période, budget commun de 120 points par image),
coordonnées calculées **une fois par cible et par chargement** (`ABRLidar::GetGlyphCache`), plaque relevée au carnet
quand son inscription est **complète** ; matériaux mis à jour seulement quand l'atténuation change visiblement ;
compteurs `FBRLidarStats` (rayons, demandés, acceptés, remplacés, expirés, refusés, inscriptions, temps) et commande
**`BRLidarStats`**. Le banc pilote `RunFrame` à travers `IScanHost`, l'interface que `ABRLidar` implémente : c'est le
même code que le jeu.

**Obtenu** : plaque cadrée par tous les rayons pendant 3 s : **88 points d'inscription** (une passe), relevée **une
fois**, au plus 30 points d'inscription par image à 30 img/s (v4.13 : jusqu'à 1073) ; quatre portes et quatre cages :
8 inscriptions complètes en 1,15 s.

### 2.5 Scanner lié au chargement

`ABRLidar::ComputeLoadKey` (niveau, graine, numéro de chargement, empreinte du plan) ; un autre chargement appelle
`ResetForLoad` : points, plaques relevées, cibles, repères montrés, focus et temporisations effacés. Test du jeu
`6 → 6` avec une autre graine préparé (§ 7).

### 2.6 Porte du Level 0 chez le client

`ABRWorld::ReconcileServiceDoor` (client seulement, à chaque `OnRep_Story`) : si la porte construite diffère de celle
de l'hôte, le client reprend l'emplacement de l'hôte (`PlaceServiceDoor`) et reconstruit le Level 0
(`RebuildAllChunks`) ; un écart persistant est une **erreur** dans le journal, plus un simple avertissement.

### 2.7 Actions maintenues

- `StopHeldActions` (focus du scanner, son, sprint) à l'ouverture de l'inventaire, à la pause et à tout passage au
  curseur, à la capture d'une touche, à la reconstruction des touches, à la **perte du focus de la fenêtre** (avec
  `FlushPressedKeys`), et sur `Canceled` de l'action ; `ABRLidar::CanScan` refuse aussi menu, dialogue, mort, transition ;
- maintien du faisceau sur **un** Smiler (`HoldEntity`) : remis à zéro si la cible change ; l'hôte vérifie que la cible
  annoncée (`ServerLidarAim`) est la même depuis assez longtemps avant d'accepter le recul ;
- recul : destination calculée **une fois** (tracé, arrêt 80 cm avant un mur, 50 cm en retrait d'une fosse), puis
  attente ; plus de nouvelle destination à chaque décision.

## 3. Direction préservée

Aucun changement à l'ordre de la campagne (`0 → 1 → 37 → 2 → …`), au Level 0 sans énigme, aux règles de tirage et de
sauvegarde de la porte, aux dimensions des fosses, au Level 6 d'après la vidéo de référence, ni à la sortie vers le
Niveau 8. Le kit ne remplace pas la génération : le Level 0 reste généré, sa porte reste tirée par partie.

## 4. Kit Blender du Level 0 : importé (préparé) et branché

### 4.1 Import explicite et reproductible

`Content/Python/backrooms_setup.py`, fonction **`import_level0_kit(force=False)`** (appelée aussi par `run()`), dans un
dossier séparé `/Game/Backrooms/Level0Kit/{Meshes,Textures,Materials}`. Les originaux (`RawAssets/Level0Kit`) ne sont
jamais modifiés.

| Point vérifié | Réglage |
|---|---|
| Unités | 1 unité Blender = 1 m, `Import Uniform Scale` 1,0, `Convert Scene` actif → centimètres |
| Axes | `Force Front XAxis` désactivé ; Unreal retourne Y (main gauche) ; **le jeu lit ce sens sur le maillage** (§ 4.2) |
| Pivots | au sol, sur la grille (murs centrés sur la ligne, porte de service au pied de la face intérieure) |
| Collisions | `UCX_<module>_NN` importés, **collision automatique désactivée** ; le jeu refuse un module requis sans collision |
| Slots | un matériau par nom de slot (`M_L0_Wallpaper`…), instances de trois maîtres `M_L0K_Pbr/Color/Decal` |
| PBR | BaseColor sRGB × teinte × couleur de sommet (puits) ; **ORM linéaire (sRGB décoché), un seul échantillonnage** : R occlusion, G rugosité, B métal |
| Normales | compression *Normalmap*, convention DirectX (vert non retourné) |
| Alpha | décals et marques : maître masqué deux faces, masque = canal A |
| Rapport | `Saved/BackroomsSetup_Level0Kit.txt` : modules importés, slots, nombre de UCX, modules requis manquants |

### 4.2 Branchement dans le générateur, avec repli

`FBRLevelDef::bKitModules` (Level 0 seulement) et `UBRAssets::UseLevel0Kit()` : le kit est utilisé si **chaque module
requis** est importé, avec ses UCX quand il en faut, habillé **uniquement** de matériaux du kit, et si le joueur ne l'a
pas écarté (`-BRNoLevel0Kit`, `BRLevel0Kit 0`). Sinon : les boîtes habituelles, et la raison est écrite au journal
(**pas de repli silencieux**). Le sens de l'axe Y est lu sur la boîte de `SM_L0_Wall_Corner` (plinthes côté +Y
attendues) ; une boîte inattendue (échelle, axe X retourné) refuse le kit.

| Élément | Module | Règle |
|---|---|---|
| Mur plein | `SM_L0_Wall_350` / `_Aged_350` | un par cellule ; papier jauni : même tirage que la scène du kit (`Rand(2x+est, y, 7301) < 0,18`) |
| Plinthes | `SM_L0_Baseboard_290` ×2, `_070` ×4 par passage | cachées à 60 m |
| Passage | `SM_L0_Doorway` + `SM_L0_DoorCasing` | passage 150 cm, linteau 225 cm (comme avant) |
| Poteaux | `Corner`, `JunctionT`, `JunctionX`, `End`, `Straight` | un par sommet touché par un mur ou une porte ; forme et lacet selon les bras ; chaque sommet a un seul propriétaire (cellules du chunk, plus les lignes ouest et sud de l'enceinte) |
| Piliers | `SM_L0_Pillar` | sauf là où un poteau occupe le sommet |
| Porte de service | `SM_L0_ServiceDoor_Wall` | mur percé 130 × 225 cm, au pied de la face intérieure, X local vers la pièce ; le cadre, le battant et le sas restent ceux d'`ABRExit` et du générateur |

Lots : un lot d'instances par module et par chunk (`GetBatch`, matériaux du maillage gardés : `bMeshMaterials`) ;
préchargement dans l'ensemble `kit:Level0`, demandé pour le Level 0 seulement.

### 4.3 Vérifié hors moteur

`Tools/verify_level0_kit.py` rejoue les règles de pose chunk par chunk sur la grille simulée (`verify_pitfalls.py`) :

- **graine 9** comparée à la scène du kit (`L0_Placement.json`, convertie dans le repère d'Unreal) : 2213 modules dans
  la scène, 2175 posés par le jeu, **2173 identiques** (position et lacet) ; écarts voulus : 38 piliers de la scène
  cachés dans un poteau, et les deux poteaux au bout du mur percé de la porte (la scène ne comptait pas ce mur comme un
  bras) ;
- **200 graines** : chaque arête mur ou porte reçoit exactement un module, chaque sommet touché exactement un poteau,
  aucun mur plein ne retombe sur une boîte ;
- premier passage (40 graines) : **5 défauts trouvés** — quand la porte de service est dans la première ou la dernière
  cellule d'un chunk, un bout de mur de 30 cm, entièrement dans un poteau, retombait sur une boîte (doublon visible) ;
  corrigé dans `AddWallSegment`, 0 défaut ensuite.

### 4.4 Non fait (limites)

- import, collisions, rendu et captures **non constatés dans Unreal** ; comparaison avant/après de la même graine
  préparée (`BRLevel0Kit 0|1`, captures `v414_kit_graine9_avec/_sans` de `-BRAutoTestV414`) ;
- sols, plafonds, néons et salle de fosses restent ceux du générateur (variations de moquette, salissures, coupures,
  trous) : les modules correspondants du kit sont importés mais pas posés ;
- **chanfreins** : les modules sont des boîtes à arêtes vives ; un biseau demande de regénérer le kit (Blender) et de
  refaire ses rendus : non fait dans cette passe ;
- **LOD** : modules de quelques dizaines de faces, sans Nanite ni LOD ; la stratégie retenue est l'instanciation par
  module et la distance de disparition des petits éléments (plinthes, chambranles). À mesurer dans Unreal ;
- UV par module : le motif du papier peint repart à chaque module (raccord visible aux poteaux possible) ; le mur
  percé de la porte de service n'a pas de plinthe (comme dans le kit).

## 5. HUD, inventaire et confort

- **Scanner contextuel** : cône seulement pendant le focus (+0,35 s) ; barre de recharge seulement pendant la recharge,
  puis 0,6 s « prête » ; relevé seulement pendant un relevé ; rappel des touches pendant le tutoriel (jusqu'au
  calibrage), 12 s à chaque arrivée, puis effacé en 3 s. Les invites suivent les touches reconfigurées.
- **Aide visuelle des parasites**, nouveau réglage (Paramètres › Interface) : *standard*, *grande* (1,5 ×, fond opaque,
  opacité du HUD ignorée), *grande + bords* (chevrons au bord de l'écran du côté des parasites, un à quatre selon
  l'intensité). Formes et couleurs gardées ; jamais masquée. 5 textes nouveaux, 22 langues.
- Inchangé et conservé : aucune barre de vie ; stamina et santé mentale dans l'inventaire seulement ; Level 0 sans
  compteur. **Non fait** dans cette passe : la nouvelle hiérarchie de l'inventaire et sa vérification en 16:9,
  ultrawide, petite fenêtre, arabe et CJK (aucune capture possible ici).

## 6. Coopération et plateformes

Protocole réseau **414** (visée du scanner validée par l'hôte) : une partie 4.13 et une 4.14 refusent de se mélanger.
Version du projet 4.14.0.0. Points LiDAR toujours locaux. Rien de testé à quatre joueurs, ni sur Linux ou macOS, ni en
paquet cuisiné.

## 7. Tests du jeu v4.14 (préparés, non exécutés)

`-BRAutoTestV414` (aussi inclus dans `-BRAutoTest`), fichier `BRAutoTestV414.cpp` ; parties de test sous un préfixe
dédié, jamais une partie du joueur.

| Scénario du prompt | Couverture |
|---|---|
| Campagne puis rejeu : consommation, ramassage, mort, fin, fermeture | banc I (exécuté) ; jeu : fichier relu après chaque étape, réveil dans le niveau rejoué (préparé) |
| Migration `[0, 1, 37, 4]` et retour par 0 | banc G, H (exécuté) ; jeu : vrai fichier format 4, session garée conservée (préparé) |
| Focus à 30/60/144/240 img/s, réserve pleine, trou visé | banc (exécuté) ; jeu : `t.MaxFPS`, débit et refus mesurés (préparé ; jugé seulement si la cadence est atteinte) |
| Longue image de 13 s pour 12 s | banc (exécuté) |
| Porte/cage cadrée, impulsion + focus | banc (exécuté) ; jeu : compteurs `BRLidarStats` (préparé) |
| Rechargement `6 → 6` | jeu (préparé) |
| Client tardif, état de porte reçu après le niveau | jeu : porte du client = porte de l'hôte (préparé) ; réception tardive à provoquer à la main (latence) |
| Inventaire / pause / perte de focus pendant le scan | jeu : inventaire et pause (préparé) ; perte de focus : à la main |
| Nouvelle partie, mort, reprise, streaming au Level 0 | tests v4.13 (préparés) |
| Chapitre `0 → 1 → 37 → 2` | banc de l'histoire et de contenu (exécuté) ; tests v4.13 (préparés) |
| Quatre joueurs, hôte distant, reconnexion | non automatisé ; à jouer |
| Kit et joueur dans un paquet cuisiné | jeu : état du kit, modules dans les chunks, passages franchissables par la capsule, murs bloquants (préparé) ; paquet : à faire |

## 8. Langues

7 clés nouvelles (`Menu.ParkedWaiting`, `Menu.ParkedSaveV414`, `Menu.LidarStaticAid`, `Menu.StaticAidStandard`,
`Menu.StaticAidLarge`, `Menu.StaticAidEdges`, `Menu.LidarStaticAidHint`), 1 retirée (`Menu.ParkedSave`), 1187 clés,
22 langues à 1187/1187. Les 21 traductions nouvelles sont marquées **à relire** (faites ici, pas par des traducteurs).
Police chinoise simplifiée : +1 caractère (遺) ; les autres polices sont inchangées (jeu de caractères identique).

## 9. Commandes

```sh
# Depuis Backrooms/
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Story/test_story.cpp \
    Source/Backrooms/Private/BRStoryLogic.cpp Source/Backrooms/Private/BRContentLogic.cpp -o test_story && ./test_story
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Lidar/test_lidar.cpp \
    Source/Backrooms/Private/BRLidarLogic.cpp -o test_lidar && ./test_lidar
python3 Tools/verify_level0_kit.py --seeds 200
python3 Tools/Localization/loc_extract.py --check && python3 Tools/Localization/loc_build.py --check

# Dans l'éditeur (préparé) : import du kit, puis rapport Saved/BackroomsSetup_Level0Kit.txt
#   import backrooms_setup; backrooms_setup.import_level0_kit(force=True)
# Jeu (préparé) :
#   Backrooms.exe -game -BRAutoTest -BRAutoTestV414
#   Backrooms.exe -game -BRNetTest            (hôte et client)
# Console (mode développeur) : BRLevel0Kit [0|1], BRLidarStats [1], BRDoor
```

Les commandes complètes des autres bancs sont en tête de chaque journal de [`Docs/v414/`](v414/).

## 10. Limites restantes et prochaines étapes

1. **Compiler** Development Editor et Shipping (UHT, regroupé et séparé) et corriger les erreurs du moteur réel ;
   `BRAssets.cpp`, `BRSave.cpp` et `BRAutoTest.cpp` n'ont été relus qu'à la main.
2. Lancer `import_level0_kit(force=True)`, lire le rapport, puis `-BRAutoTestV414` : kit complet, passages, murs,
   captures avec et sans le kit à la même graine.
3. Lancer `-BRAutoTestV414`, `-BRAutoTestV413`, les précédents et `-BRNetTest` ; jouer les cas non automatisés (perte
   de focus, client tardif sous latence, quatre joueurs, reconnexion pendant une mission).
4. Profiler sur la RTX 4080 + 7800X3D (qualité maximale, RT) : référence avant tout gain annoncé ; Poolrooms, porte de
   service, transitions, Level 6 en scan continu (compteurs `BRLidarStats`, 48 matériaux du scanner).
5. Captures dans Unreal (Level 0, porte, joueur, carnet, inventaire, Poolrooms, Level 6) ; inventaire en 16:9,
   ultrawide, petite fenêtre, arabe, CJK.
6. Chanfreins et LOD du kit, sols et plafonds du kit si le rendu le justifie ; relecture humaine des 21 traductions.
7. Cook et paquets Windows, Linux, macOS ; aucune dépendance aux fichiers bruts.
