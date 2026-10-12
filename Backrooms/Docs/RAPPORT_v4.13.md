# Rapport v4.13 : Level 0 sans énigme, progression stable, histoire de l'équipe de L., Level 6 sans lumière

Travail fait le 12 octobre 2026 sur la branche `backrooms`.

| | |
|---|---|
| **Commit de base** | `90bd60c` (v4.12, 9/9), vérifié avant toute modification ([audit](AUDIT_v4.13.md)) |
| **Commits v4.13** | huit commits « v4.13 (n/8) », listés ci-dessous ; puis six commits « v4.13 suite (n) » : porte tirée à chaque partie, Level 6 sans lumière ([§ 14](#14-suite-v413--porte-tirée-à-chaque-partie-level-6-sans-lumière)) |
| **Documents** | [Audit](AUDIT_v4.13.md) · [Histoire et progression](HISTOIRE_PROGRESSION_v4.13.md) · [Gameplay](GAMEPLAY_v4.13.md) · [Plan de publication](PLAN_PUBLICATION.md) · [Kit Blender du Level 0](../RawAssets/Level0Kit/README.md) · journaux réels dans [`Docs/v413/`](v413/) |

Commits v4.13 :

- `7c4426a` (1/8) : audit, ordre narratif unique (`BRStoryLogic`), Level 0 hors des missions, sorties du chapitre ;
- `7673f7c` (2/8) : histoire tenue par l'hôte, porte de service, sauvegarde format 5, CONTINUER / REJOUER ;
- `42e7d0b` (3/8) : fil narratif de L., objectifs formulés comme des résultats, traductions ;
- `2694ced` (4/8) : tests du jeu v4.13 (`-BRAutoTestV413`, `-BRNetTest`), version du projet 4.13 ;
- `ade878c` (5/8) : kit Blender du Level 0 (scripts, modules, textures, FBX, `.blend`, placement, rendus) ;
- `77b69c7` (6/8) : journaux réels (`Docs/v413/`), planche des portes ;
- `9f01682` (7/8) : histoire et progression, gameplay, plan de publication ;
- `abbf28c` (8/8) : rapport, README.

La suite (porte tirée à chaque partie, Level 6 sans lumière) est décrite aux §§ 14 à 19 ; les §§ 1 à 13 décrivent la
première passe et restent valables pour ce que la suite ne change pas.

## 1. Où, comment, et ce qui n'a pas pu être fait

**Machine.** Conteneur Linux x86_64, 4 cœurs, **sans GPU**.

- **Unreal Engine absent** : ni éditeur, ni UHT/UBT/UAT.
- Disponibles : g++ 13.3, clang 18.1, Python 3.11, **module `bpy` 5.0.1** (Blender 5.0.1 sans interface).
- La documentation de `dev.epicgames.com`, `partner.steamgames.com` et des guides NVIDIA est refusée par la politique
  réseau (**403**). **Rien n'a été contourné.**

**Exécuté ici** (journaux réels dans [`Docs/v413/`](v413/), commandes au § 9) :

- sept bancs C++ hors moteur, sur le code même du jeu :
  - histoire : 44 vérifications ;
  - contenu par version : 68 ;
  - missions : 3000 graines × 12 niveaux ;
  - mécanismes : 49 ; départ de groupe : 30 ; lumière : 6 ; transactions : 51 ;
  - tout est OK ;
- simulation du Level 0 : fosses sur 24 graines, porte de service sur **500 graines**, 0 problème ; planche de 12 graines ;
- **kit Blender du Level 0 complet** (Blender 5.0.1) : textures, 38 modules, scène de la graine 9, contrôles, exports
  FBX relus, `.blend` autonome, placement, six rendus Cycles (§ 8) ;
- langues : 1142 clés, 22 langues à 1142/1142, 0 refusée ; relecture ciblée : 0 problème ;
- tests des outils de paquet : 62 tests, 61 OK, 1 sauté (propre à un hôte non Linux) ;
- empreintes des modèles protégés : OK ;
- contrôle syntaxique (clang, en-têtes Unreal simulés) des 13 fichiers C++ modifiés vérifiables : 0 avertissement,
  0 erreur ; aucun masquage de nom ; aucun nouveau conflit de compilation regroupée.

**Préparé, non exécuté** (aucune de ces étapes n'est comptée comme réussie) :

- compilation Editor et Shipping, cook, paquets Windows, Linux et macOS ;
- tous les tests du jeu : `-BRAutoTestV413` (nouveau, § 7), V47 à V412, fosses, `-BRNetTest`, `-BRSmokeTest` ;
- captures du jeu, mesures d'images ;
- **import du kit Blender dans Unreal** (réglages écrits, aucun import fait) ;
- essais coop à quatre processus ; un client qui rejoint après l'ouverture de la porte ;
- Steam : aucun envoi ; `SetLive` vide.

**Catégories.** *Constat de source* : lu dans le code de la base ([audit](AUDIT_v4.13.md)). *Corrigé, vérifié hors
moteur* : banc exécuté ici sur le code du jeu. *Corrigé, test préparé* : code et test du jeu écrits, rien de lancé
dans Unreal.

## 2. Level 0 sans énigme

| Demande | Fait | Catégorie |
|---|---|---|
| Une vraie porte de service vers le Niveau 1 (cadre, poignée, charnières, battant, pivot, animation, collision, son) | `ABRExit::BuildServiceDoor` : cadre d'acier, battant sur pivot (0 → 95°, Tick seulement pendant l'animation), poignées, barre, trois charnières, collision du battant, filet de lumière, lampe du sas ; `S_ServiceDoor`, `S_Draft` (sons générés) ; mur d'enceinte percé et sas bâtis par le chunk | corrigé, test préparé |
| Posée une fois, chemin garanti pour une capsule, sans fosse, repli déterministe | `ABRWorld::PlaceServiceDoor` : parcours en largeur sans salle de fosses, fenêtre 24–40 cellules, repli au plus proche de 32 ; rien posé sur le chemin | vérifié hors moteur (500 graines, 0 problème, 0 repli) ; marche réelle préparée (`-BRAutoTestV413`, 5 graines) |
| Coop : état ouvert tenu par l'hôte, rassemblement, un seul départ | état répliqué (`FBRNetStory::DoorSerial`), chaque machine anime son battant ; `Leave` → rassemblement v4.11 → un départ | test préparé (`-BRNetTest`) |
| Mise en scène : réveil, repères, aide facultative, rencontres dosées, notes corrigées, plus de « 0/1 » | réveil une fois par partie ; air froid, lumière, marques de craie ; aide après 7 min (minuterie) ; apparitions, coupures et ronde retardées, deux Smilers par coupure ; notes `Note.L0.3`, `.7`, `.8` réécrites ; plus de liste d'objectifs au Level 0 | test préparé |
| Plus de mission ni d'ancien objectif au Level 0 | `HasMission(0)` faux ; `bRequireObjectives = false`, plus de cassettes tirées ; identifiants de rôles inchangés (empreintes des autres plans) | banc des missions (introduction sans mission, 3000 graines) |

## 3. Progression stable, transitions validées par l'hôte

| Demande | Fait | Vérification |
|---|---|---|
| `0 → 1 → 37 → 2 → fin du chapitre` ; campagne `… → 11 → dernier quai` | `BRStory::Order` ; sorties des niveaux réécrites (1 : ascenseur → 37 ; 37 : échelle → 2 ; 4 : retour → 3 ; 11 : plus de porte au hasard) ; redirections retirées | bancs histoire et contenu |
| Poolrooms obligatoires | `CanEnter` : rien au-delà des Poolrooms avant leur traversée | banc d'histoire ; test préparé |
| Validation des transitions par l'hôte, sans raccourci | `ABRWorld::CanEnterLevel` dans `RequestTransition` (histoire, rejouer, libre) ; raison dans la langue du joueur | banc ; test préparé (raccourcis refusés depuis le Level 0) |
| Traversée des Poolrooms enregistrée | `OnProgress` : étape 37 accomplie à la prise de l'échelle | banc ; test préparé |
| Missions accomplies restent accomplies | au retour dans un niveau accompli, la mission est résolue par le solveur (même solution que les bancs) | test préparé (retour au Niveau 1) |
| Destinations au hasard seulement après une fin | `GetExplorationChoices` : niveaux atteints et disponibles ; porte au hasard du Niveau 11 retirée | banc de contenu ; test préparé |

## 4. Histoire et état narratif

- Fil de l'équipe « L. » vers le dernier quai : buts concrets par niveau, lignes de réussite, notes signées,
  inscription des Poolrooms, porte de la station marquée à la fin du chapitre 1. Détail :
  [`HISTOIRE_PROGRESSION_v4.13.md`](HISTOIRE_PROGRESSION_v4.13.md).
- État (`BRStory::FState`) tenu par l'hôte, répliqué (`FBRNetStory`, réplication à la connexion), sauvegardé (format 5).
  Aucun Tick : minuteries (aide, inscription) et événements. Un événement déjà vu n'est jamais rejoué au chargement.
- Textes : 59 textes nouveaux ou modifiés, traduits dans les 21 autres langues (traduction faite ici, **sans relecture
  humaine** ; relecture ciblée outillée sans problème).

## 5. Migration des anciennes parties

| Demande | Fait |
|---|---|
| Sessions du Level 0 : la porte sans condition, objets gardés | `MigrateStory` : session gardée (graine, objets, documents), état de mission retiré |
| Copie de secours | `<emplacement>_Format<N>` avant toute réécriture (inchangé depuis la v4.7), supprimée avec la partie |
| Traversée des Poolrooms organisée si elle manque | session au-delà **garée intacte**, reprise aux Poolrooms, retour à la progression d'avant à leur sortie (`TakeParkedSession`, avis) |
| Traversée créditée seulement sur preuve | preuve = Niveau 4 découvert juste après les Poolrooms (leur échelle d'avant la v4.13) |
| `GenVersion` pas monté à l'aveugle | inchangé ; seule la session du Level 0 change de mode |

Vérifié : banc d'histoire (logique) ; test préparé sur de vrais fichiers du format 4 (`-BRAutoTestV413`, § 7).

## 6. Menu

NOUVELLE PARTIE (Level 0, histoire vide) ; **CONTINUER** (point de reprise : `ResumePoint`) ; **REJOUER** (carrousel des
niveaux découverts, sans toucher à l'histoire ni à la session sauvegardée). Même choix pour héberger.

## 7. Tests du jeu ajoutés (préparés, non exécutés)

`-BRAutoTestV413` (seul) ou dans `-BRAutoTest` ; fichier `Source/Backrooms/Private/BRAutoTestV413.cpp`. La marche réelle
de la v4.12 (chemin de grille suivi avec les seules entrées du joueur) est partagée par un en-tête
(`BRAutoTestWalk.h`). Sauvegardes du test : `BR_AutoTestV413_*` ; la partie active et le mode sont rétablis.

```bat
UnrealEditor.exe Backrooms.uproject -game -windowed -ResX=1920 -ResY=1080 -BRAutoTest -BRAutoTestV413 -BRAutoTestStrict
```

| Étape | Vérifie |
|---|---|
| porte, graines 9, 1, 77, 4605, 31337 | porte construite, chemin continu depuis la zone de départ jusqu'à la cellule de la porte, aucune fosse, marques, aucune autre sortie, aucune mission ; **marche réelle** jusqu'à la porte (0 déplacement direct) ; porte visée par l'interaction ; captures `v413_graine_*` |
| histoire : réveil | session d'histoire, réveil joué, porte fermée, aide programmée (≤ 420 s), aucune mission |
| histoire : raccourcis | Niveaux 1, 37 et 2 refusés depuis le Level 0 (raisons notées) ; demande directe des Poolrooms sans effet |
| histoire : porte | marche réelle, touche d'interaction, battant ouvert, un seul changement de niveau, arrivée au Niveau 1 ; Level 0 accompli et sauvegardé ; Poolrooms et Niveau 2 encore refusés, retour au Level 0 permis |
| histoire : 1 → 37 → 2 → fin | missions résolues en marchant, sorties prises à pied (v4.12) ; inscription de L. ; retour au Niveau 1 : mission déjà résolue, sortie ouverte ; traversée des Poolrooms enregistrée ; fin du chapitre enregistrée et sauvegardée, marque de la station, CONTINUER au Niveau 2, exploration limitée aux niveaux atteints |
| migration (format 4) | 0, 1, 2, 3 : copie identique, reprise aux Poolrooms, session du Niveau 3 garée intacte, Niveau 2 fermé ; 0, 37, 4, 1 : traversée créditée, session inchangée ; 0, 37, 1, 2 : non créditée ; session du Level 0 : graine et documents gardés, mission retirée |

**`-BRNetTest`, v4.13** (Level 0, graine 9, après les étapes v4.12) :

- porte fermée chez l'hôte et chez le client (cellule notée dans les deux rapports) ;
- l'hôte l'ouvre (touche d'interaction) ; le client reçoit l'état, voit son battant s'ouvrir et le rassemblement ;
- le client arrive à pied ; un seul départ ; arrivée commune au Niveau 1 ;
- un client qui rejoint **après** l'ouverture : non reproductible dans ce banc (le client se connecte au début), noté
  « NON VÉRIFIÉ ».

Les tests V411 et V412 ont été adaptés au commit 2/8 (Level 0 sans mission, sorties du fil, migration).

## 8. Kit Blender du Level 0 (exécuté)

`Tools/Blender/build_level0_kit.py` et `Tools/Blender/level0_kit_textures.py`, exécutés avec Blender 5.0.1 ; sortie dans
[`RawAssets/Level0Kit/`](../RawAssets/Level0Kit/README.md).

- 34 modules `SM_L0_*` + 4 modules archivés (échelle, trappe, couvercle, mur instable) : murs, angles, jonctions, poteaux,
  passages et chambranles, piliers, sols, plafonds, plinthes, néons (allumés, en panne), dalles percées, bordures,
  puits de 14 m, fonds, porte de service (mur, cadre, battant sur pivot, poignée, charnières, filet de lumière, sas),
  marques de craie, décals ;
- collisions `UCX_<module>_NN` ; UV0 à l'échelle du monde, UV1 de lightmap sans chevauchement ;
- textures PBR cuites en images (2K pour les grandes surfaces ; BaseColor, ORM, Normal DirectX, émissif, décals RGBA),
  emballées dans le `.blend` et copiées dans `Textures/` (30 Mo) ;
- scène de la graine 9 rejouée depuis la simulation du générateur : 5205 instances, vrais trous (16), 313 néons allumés,
  porte de service et chemin de L., repères nommés, caméras à hauteur d'œil ;
- contrôles : étalon de 1 m, normales (0 défaut), 16 trous ouverts sur 16 (rayons), chemin de 28 pas libre pour une
  capsule (rayons), **38 FBX relus sur 38** ;
- six rendus Cycles 1280 × 720 : couloir, salle répétitive, intersection, fosses, porte de service, vue d'ensemble ;
- `L0_Placement.json` : chaque instance et chaque repère, en mètres et en centimètres Unreal.

**Non fait** : l'import dans Unreal (réglages préparés dans le README du kit) ; la comparaison avec une capture du jeu.

## 9. Commandes et résultats (exécutés ici)

Depuis `Backrooms/`. Chaque journal donne la commande exacte, l'outil, la date et le code de sortie.

| Commande | Résultat | Journal |
|---|---|---|
| `g++ … Tools/Story/test_story.cpp BRStoryLogic.cpp BRContentLogic.cpp` | 44 vérifications, 0 échec | `v413/banc_histoire.txt` |
| `g++ … Tools/Content/test_content.cpp BRContentLogic.cpp BRMissionLogic.cpp BRStoryLogic.cpp` | 68, 0 échec | `v413/banc_contenu.txt` |
| `g++ … Tools/Missions/test_mission_logic.cpp …` puis `./test_missions` (3000) | RÉSULTAT OK | `v413/banc_missions.txt` |
| `g++ … Tools/Mechanisms/test_mechanisms.cpp …` | 49, 0 échec | `v413/banc_mecanismes.txt` |
| `g++ … Tools/Departure/test_departure.cpp BRGatherLogic.cpp` | 30, 0 échec | `v413/banc_depart.txt` |
| `g++ … Tools/Light/test_light.cpp` | 6, 0 échec | `v413/banc_lumiere.txt` |
| `g++ … Tools/Transactions/test_transactions.cpp BRTxnLogic.cpp` | 51, 0 échec | `v413/banc_transactions.txt` |
| `python3 Tools/verify_pitfalls.py` | 24 graines, 0 problème | `v413/fosses_et_porte.txt` |
| `python3 Tools/verify_pitfalls.py --doors 1-500` | 500 graines, 0 problème, 0 repli, 24–40 cellules | `v413/fosses_et_porte.txt` |
| `python3 Tools/verify_pitfalls.py --seeds 1-12 --maps …` | planche (simulation, pas une capture du jeu) | `v413/cartes_porte_12_graines.png` |
| `python3 -m unittest Tools/Build/tests/test_packaging.py -v` | 62 tests, 61 OK, 1 sauté | `v413/outils.txt` |
| `python3 Tools/Localization/loc_extract.py --check` ; `loc_build.py --check` ; `loc_review.py` | 1142 clés ; 22 langues à 1142/1142 ; 0 problème | `v413/outils.txt` |
| `python3 Tools/protect_assets.py check` | OK | `v413/outils.txt` |
| clang 18, contrôle syntaxique (13 fichiers) | 0 avertissement, 0 erreur ; 0 masquage ; 1 conflit regroupé, déjà dans la base | `v413/syntaxe.txt` |
| `python3 Tools/Blender/build_level0_kit.py --seed 9 --render` | voir § 8 | `RawAssets/Level0Kit/build_report.json`, `v413/blender_kit.txt` |

Non couverts par le contrôle syntaxique : `BRSave.cpp` (archives du moteur absentes des en-têtes simulés : les erreurs
portent sur le code d'archive v4.12, pas sur la migration v4.13) et `BRAutoTest.cpp` (`Dom/JsonObject.h`).

## 10. Contraintes

| Contrainte | État |
|---|---|
| Pas de jauge de santé | inchangé (deux jauges dans l'inventaire) |
| 9 entités, 12 niveaux | inchangés ; les Poolrooms restent sans entités |
| Modèles protégés | `protect_assets.py check` OK ; le kit lit seulement des textures non protégées |
| Caméscope dans le sac | inchangé |
| Solo, coop à 4, 22 langues, Windows / Linux / macOS | inchangés ; nouveaux textes dans les 22 langues |
| Aucun Tick permanent pour la narration | minuteries et événements ; Tick du battant seulement pendant son mouvement |
| Aucun déploiement Steam automatique | inchangé (`SetLive` vide) |
| Version | `ProjectVersion=4.13.0.0`, protocole 413 |

## 11. Limitations restantes

1. **Rien n'a été compilé ni joué dans Unreal.** Le premier essai est la compilation, puis `-BRAutoTestV413`.
2. La porte, le sas et les marques ont été vérifiés par la simulation et par la reconstruction Blender, pas dans le
   rendu du jeu : alignement du battant, lumière froide et lisibilité des marques restent à voir.
3. Le suiveur de chemin du test peut échouer pour une raison étrangère à la porte (entité qui gêne, coupure) : le
   rapport du test donne la cellule et une capture.
4. Client arrivé après l'ouverture de la porte : non automatisé.
5. Traductions v4.13 : relecture outillée seulement, aucune relecture humaine.
6. Kit Blender : import Unreal non validé ; disposition non comparée à une capture du jeu ; modules à arêtes vives,
   sans LOD.
7. Chapitres 2 à 5 : buts écrits, traces de L. à écrire avec leurs lots.
8. Plateformes, signature Apple, Steam : préparés, non exécutés ; documentation Epic, Steamworks et NVIDIA inaccessible
   (403).

## 12. Fichiers modifiés depuis `90bd60c`

**Nouveaux** :

- `Public/BRStoryLogic.h`, `Private/BRStoryLogic.cpp` (logique pure), `Private/BRStoryWorld.cpp` ;
- `Private/BRAutoTestV413.cpp`, `Private/BRAutoTestWalk.h` ;
- `Tools/Story/test_story.cpp`, `Tools/generate_story_sounds.py` ;
- `Tools/Blender/build_level0_kit.py`, `Tools/Blender/level0_kit_textures.py`, `RawAssets/Level0Kit/` ;
- `RawAssets/Sounds/S_Draft.wav`, `S_ServiceDoor.wav` ;
- `Docs/AUDIT_v4.13.md`, `RAPPORT_v4.13.md`, `HISTOIRE_PROGRESSION_v4.13.md`, `GAMEPLAY_v4.13.md`, `Docs/v413/`.

**Jeu** : `BRWorld` (porte, chemin, histoire, transitions), `BRMissionWorld` (sorties du fil, mission accomplie au
retour, fin du chapitre), `BRChunk` (mur percé, sas, marques, rien sur le chemin), `BRInteractables` (porte de service),
`BRLevels` (sorties, notes, rencontres du Level 0), `BRContentLogic` (plus de redirections, protocole 413),
`BRMissionLogic`, `BRMissionSolver`, `BRMission.h`, `BRMissionText` (buts, lignes, inscription), `BRHUD` (réveil,
carnet, menu, fin du chapitre), `BRPlayerController` (CONTINUER / REJOUER, histoire sauvegardée), `BRSave` (format 5,
migration), `BRTypes.h`, `Config/DefaultGame.ini`.

**Tests et bancs** : `BRAutoTest.cpp/.h`, `BRAutoTestV411.cpp`, `BRAutoTestV412.cpp`, `Tools/Content/test_content.cpp`,
`Tools/Missions/test_mission_logic.cpp`, `Tools/verify_pitfalls.py`.

**Langues** : `Content/Localization/Game/*/Game.po` et `.locres` (22 langues), `BRLocKeys.inl`,
`Docs/LOCALISATION_COUVERTURE.md`.

**Documents** : `Docs/PLAN_PUBLICATION.md`, `README.md`.

**Non modifiés** : les modèles protégés et leurs originaux.

## 13. Essais restant à exécuter, dans l'ordre

1. Compiler `BackroomsEditor Win64 Development`, puis `Backrooms Shipping` (Windows, Linux, Mac), unités regroupées
   comprises ; corriger les erreurs réelles.
2. `-BRAutoTest -BRAutoTestV413 -BRAutoTestStrict` ; lire `Saved/AutoTest/Rapport.txt` et les captures `v413_*`.
3. `-BRAutoTest -BRAutoTestStrict` complet (V47 à V412, fosses).
4. `-BRNetTest` (hôte avec `?listen`, client sur la même machine), puis à la main : quatre joueurs ; un client qui
   rejoint après l'ouverture de la porte ; latence et pertes.
5. Reprendre de vraies parties v4.12 (une au Level 0, une au-delà des Poolrooms) et lire les avis.
6. Importer le kit Blender dans Unreal avec les réglages de son README ; comparer à une capture du Level 0 de la
   graine 9.
7. Paquets natifs, signature et notarisation ; préparation Steam, sans publication.

## 14. Suite v4.13 : porte tirée à chaque partie, Level 6 sans lumière

Demande traitée : « amélioration Backrooms 3 » (porte du Level 0 aléatoire par partie, douze niveaux obligatoires,
Level 6 LiDAR, migration, kit Blender, tests et documents). Même machine, mêmes limites (§ 1) : **pas d'Unreal, pas de
GPU** ; `dev.epicgames.com`, `partner.steamgames.com` et la documentation NVIDIA refusées (**403**), rien de contourné.

Commits de la suite :

- `d82729d` suite (1) : porte du Level 0 tirée à chaque nouvelle partie ;
- `90fcdfd` suite (2) : Level 6, logique de la mission LiDAR (plan, solveur, banc, conversion des balises) ;
- `49c2887` suite (3) : Level 6 sans lumière : aile d'essai, scanner, décor, sons, bancs ;
- `e312a3c` suite (4) : textes du Level 6 en 22 langues, polices ;
- `fe49feb` suite (5) : autotests du Level 6, repères des candidats de porte dans le kit Blender ;
- **suite (6)** (ce rapport) : documents, journaux, liste des clés de langue, `loc_extract.py --check` en lecture seule,
  étapes de sauvegarde et reprise du Level 6, planche des ailes (`verify_lidar.py --sheet`), `verify_pitfalls.py --jobs` ;
  le dernier de la branche (`git log -1`).

### 14.1 Porte du Level 0 tirée à chaque partie (demande A)

| Demande | Fait | Catégorie |
|---|---|---|
| Une porte différente à chaque nouvelle partie | graine du Level 0 propre à la partie (`IntroSeed`), tirée à la nouvelle partie (aussi depuis un menu déjà au Level 0 : rechargement forcé) | vérifié hors moteur : 118 emplacements différents pour 500 graines, 120 (tous les emplacements de bord possibles) pour 5000 graines ; test préparé |
| Sauvegardée et stable : graine, emplacement, version, état ouvert | histoire (format 5, champs ajoutés) : `IntroSeed`, `DoorX/Y/Dir`, `DoorTier`, `DoorVersion` (= 2) ; événement `DoorOpened` ; à chaque chargement, la porte sauvegardée est **relue** si elle est encore candidate, sinon nouveau choix journalisé | vérifié hors moteur (même graine → même porte, porte sauvegardée reprise : chaque graine) ; test préparé : mort, sauvegarde et reprise, rechargement, après ouverture |
| Coop, client tardif | graine et porte de l'hôte répliquées (`FBRNetStory`) | test préparé (client connecté au début) ; client tardif non automatisé |
| Placement sûr, varié | candidats contre le mur d'enceinte (ni coin, ni départ, ni mission), atteints sans fosse ; bandes 24–40, 16–56, secours le plus proche de 32 | vérifié hors moteur : 0 problème, bande visée retenue 500/500 |
| Outils de développement | commande `BRDoor` : graines, porte, palier, candidats par bande (journal et boîtes de couleur 30 s) | code ; non exécuté |
| Tests de diversité et de reproductibilité | `Tools/verify_pitfalls.py --doors` (reproductibilité, reprise de la porte sauvegardée, diversité, murs) | **exécuté** (§ 16) |

### 14.2 Douze niveaux obligatoires (demande B)

Déjà en place depuis la première passe et vérifié à nouveau : le fil `0 → 1 → 37 → 2 → 3 → 4 → 5 → 6 → 8 → 9 → 10 → 11
→ dernier quai` ne saute aucun niveau (banc de contenu, 68 vérifications : aucune sortie vers un niveau indisponible,
aucun raccourci, fin atteinte dans chaque canal) ; les douze missions sont résolues sur 3000 graines chacune (banc des
missions). Le Level 6 garde sa place (`5 → 6 → 8`) ; sa mission change (§ 14.3).

### 14.3 Level 6 « Lights Out » au scanner LiDAR (demande C)

Règles détaillées : [`GAMEPLAY_v4.13.md`](GAMEPLAY_v4.13.md), § 6.

**Référence et adaptation.** La vidéo de référence (« Level 6: Lights Out (LiDAR) Teaching The Homie | Backrooms: Escape
Together ») **n'a pas été visionnée ici** : l'adaptation suit la description écrite de la demande. Rien n'est repris du
jeu de la vidéo ; modèles, sons, textes, plans et règles sont ceux du projet (tableau de correspondance dans le document
de gameplay, § 6.1).

| Demande | Fait | Catégorie |
|---|---|---|
| Obscurité réelle | aucune lumière de niveau ; lampe réduite à une lueur, vision nocturne brouillée (refusée), expliquées au sas d'entrée et par une note ; aucun réglage graphique changé | test préparé (vision nocturne refusée) |
| Scanner : impulsion et scan focalisé | `ABRLidar` local : impulsion de 1400 rayons sur 7 images, cône 100 × 64°, portée 26 m, recharge 1,6 s ; scan focalisé de 36 rayons par image dans 7° | banc LiDAR (couverture : plus grand vide 1,36°, cône focalisé) ; test préparé |
| Points de couleurs ET de formes | six catégories (carré, triangle, losange, croix, chevron, point) ; maillages `SM_Lidar*` (Blender) | banc (catégories) ; rendu non vu |
| Réglages, touches | taille 0,5–2, contraste 0,5–2, persistance 4–30 s ; `LidarPulse` (clic droit, G, gâchette gauche), `LidarFocus` (X, clic molette, gâchette droite), reconfigurables | banc (bornes) ; code |
| Mission avec tutoriel et repères bleus | mire de calibrage dans une zone sûre (aucune entité avant le calibrage) ; flèches bleues de L. au sol, du calibrage à l'aile | banc des missions ; test préparé |
| Postes, cages, portes à solution unique | 4 postes (anciennes balises, ordre libre), 4 cages (silhouette, code ; ouvertes par leur poste), journal de L., 4 portes ; une seule solution ; sans journal ou sans cages, au moins deux portes possibles | **banc : 3000 graines, unicité, solveur sur les seules informations visibles** |
| Erreur réversible | porte fausse : refus, nombre d'éléments faux, réarmement, rien de perdu | banc (erreurs typiques) |
| Zone dangereuse aux vrais trous, sans saut à l'aveugle | aile de 4 × 4 cellules : six trous de 210 cm (machinerie des salles de fosses : sol ouvert, chute mortelle, évitement), passages ≥ 140 cm, bords en triangles orange | **banc : 2000 graines, marche d'une capsule de 42 cm au pas de 5 cm (200 graines), toutes les alcôves atteintes depuis chaque porte** |
| Dernier passage aux parasites, aide sans le son | émetteur de parasites à l'échelle (son `S_LidarStatic`) ; HUD « PARASITES » avec direction | banc (niveau monotone en distance) ; test préparé (émetteur actif) |
| Silhouettes dérivées des modèles protégés | points tirés des maillages `RawAssets/Skeletal/SK_*` de six entités, lus seulement (`build_lidar_assets.py --silhouettes`) ; planche `v413/lidar_silhouettes.png` | exécuté (Blender) ; `protect_assets.py check` OK |
| Menace lisible et apprise | Smilers : l'impulsion s'entend (18 m) et attire ; faisceau maintenu 0,6 s → recul de 7 m, pas de retour avant 6 s (vérifié par l'hôte : portée, vue, axe) ; approche annoncée ; notes de L. | code ; test non automatisé |
| Ancienne chaîne de balises réutilisée | les balises de la v4.12 deviennent les postes (même rôle de manivelle, même progression) | banc (conversion) |
| Coop tenue par l'hôte | plan, relevés, postes, portes, recul : hôte ; état répliqué ; points de scanner locaux | test préparé (`-BRNetTest`) |
| Performances | instances groupées par catégorie et âge, aucune lumière, ombre, éclairage indirect ni ray tracing par point ; au plus 257 rayons et 377 points par image ; 24 000 points au plus | banc (budget) ; mesure en jeu préparée (`StatRays`, `StatSeconds`) |
| Sortie vers le Niveau 8 | échelle dans une alcôve de l'aile | test préparé (arrivée au Niveau 8 sans chute) |
| Livrables | maillages (`RawAssets/Meshes/SM_Lidar*.fbx`, scanner tenu en main), sons (`S_LidarPulse`, `Focus`, `Static`, `Deny`, `Post`, `Calib`, générés), textes (51 nouveaux, 22 langues), documents | exécuté (générés ici) ; captures du jeu préparées, non exécutées |

### 14.4 Migration (demande D)

| Demande | Fait | Vérification |
|---|---|---|
| Pas de `GenVersion` monté à l'aveugle | inchangé (2) ; rôles du Level 6 ajoutés en fin d'énumération | **empreintes des plans des onze autres niveaux identiques avant et après** (`v413/empreintes_missions.txt`) |
| Anciennes sessions du Level 6 (balises) converties | `BRMission::MigrateState` : plan v4.12 reconstruit à l'identique (`BuildLegacyPlan6`, vérifié contre la v4.12) ; plaque lue → calibrage, balises → postes (progression partielle gardée), terminée → terminée | banc : 15 000 converties et conformes, 15 000 d'une autre graine refusées ; test préparé (en cours, terminée) |
| Copie avant réécriture | copie de la sauvegarde demandée à la conversion ; avis « progression gardée » | test préparé (avis 3) |
| Un Level 6 fini ne bloque jamais | mission terminée → toutes étapes accomplies, résolue | banc ; test préparé |
| Level 0 : parties sans graine de partie | la session du Level 0 en cours donne sa graine ; sinon tirage à la prochaine visite, puis gardée | code (`MigrateStory`) ; test préparé |

### 14.5 Kit Blender (demande E)

Le module de porte se pose sur **n'importe quel candidat** (quatre orientations) ; le kit exporte un repère
`MK_DoorCandidate_NNN` par emplacement (cellule, face, distance de marche, bande), et la porte de la scène n'est qu'un
exemple : l'hôte choisit à chaque partie. Graine 9 : 120 candidats (85 en bande visée, 35 en bande élargie). Relance
exécutée (Blender 5.0.1, 25 s) : `L0_Placement.json` identique à celui du dépôt, 38 FBX relus sur 38
(`v413/blender_kit.txt`). README du kit mis à jour.

## 15. Tests du jeu ajoutés par la suite (préparés, non exécutés)

Dans `-BRAutoTestV413` (et `-BRAutoTest`) ; fichiers `BRAutoTestV413.cpp` (porte) et `BRAutoTestV413Lidar.cpp` (Level 6).

| Étape | Vérifie |
|---|---|
| porte stable : mort, sauvegarde et reprise, rechargement | même graine du Level 0, même cellule, même face ; porte relue (`WasServiceDoorRestored`) ; histoire sauvegardée conforme |
| porte stable : après ouverture | graine et porte gardées, événement « porte ouverte » |
| Level 6 : aile, mission, scanner | aile valide, 15 mécanismes, 4 portes dans l'aile, repères au sol, zone sûre, scanner local ; vision nocturne refusée ; capture `v413_lidar_arrivee` |
| Level 6 : impulsion | rayons tracés ≥ 1400, points affichés, seconde impulsion refusée pendant la recharge ; temps passé dans le scanner ; capture |
| Level 6 : calibrage au scan focalisé | mire à 3 m en ligne de vue, calibrage validé par l'hôte ; capture |
| mission en marchant (v4.12), plaques et cages relevées | mission résolue par de vraies interactions, parasites actifs, erreurs ; capture `v413_lidar_passage_ouvert` |
| sortie à pied, arrivée au Niveau 8 | traversée de la zone percée sans chute ; scanner retiré hors du Level 6 |
| ancienne session convertie | plaque lue + 2 balises → avis 3, calibrage fait, 2 postes sur 4 |
| sauvegarde et reprise | mission LiDAR en cours : même aile, même progression, pas de reprise du début |
| ancienne session terminée | mission terminée, avis 3 : le Level 6 fini ne bloque pas |
| `-BRNetTest` : Level 6 | même aile et même empreinte de plan chez l'hôte et le client ; points locaux ; calibrage de l'hôte reçu par le client |

**Non automatisés** : quatre joueurs et client arrivé en cours de partie (notés « SAUTÉ » dans le rapport du test) ;
recul d'un Smiler au faisceau (aucune étape). À faire à la main (§ 19).

## 16. Commandes et résultats de la suite (exécutés ici)

| Commande | Résultat | Journal |
|---|---|---|
| huit bancs C++ (`run_benches`, commandes en tête de chaque journal) | histoire 44, contenu 68, missions 3000 graines × 12 niveaux, mécanismes 49, départ 30, lumière 6, transactions 51, **LiDAR 18 207** : 0 échec | `v413/banc_*.txt` |
| `python3 Tools/verify_pitfalls.py --doors 1-500` | 0 problème ; 24–40 cellules (moyenne 31) ; 118 emplacements différents ; reproductibilité et reprise : chaque graine | `v413/fosses_et_porte.txt` |
| `python3 Tools/verify_pitfalls.py --doors 1-5000 --jobs 4` | 0 problème ; bande visée 5000 fois sur 5000 ; 120 emplacements différents (tous les emplacements de bord) ; murs 1240 à 1263 ; 729 s sur 4 processus (option `--jobs` ajoutée : sortie identique à un seul processus, comparée sur 40 graines) | `v413/fosses_et_porte.txt` |
| `python3 Tools/verify_lidar.py --seeds 1-500` | entrée de l'aile atteinte 500/500 (18 à 41 cellules, médiane 28) ; 4 portes, aucun autre passage | `v413/lidar_aile.txt` |
| `python3 Tools/verify_lidar.py --seeds 1-6 --sheet Docs/v413/lidar_ailes_6_graines.png` | planche vue de dessus (simulation, pas une capture du jeu) | `v413/lidar_ailes_6_graines.png` |
| empreintes des plans avant (`d82729d`) et après | onze niveaux identiques ; plan v4.12 du Level 6 reconstruit à l'identique | `v413/empreintes_missions.txt` |
| `python3 Tools/Blender/build_level0_kit.py --seed 9 --skip-textures --out <temporaire>` | 174 repères dont 120 candidats ; placement identique au dépôt ; 38 FBX relus | `v413/blender_kit.txt` |
| `loc_extract.py --check`, `loc_build.py --check`, `loc_review.py` | 1181 clés, 22 langues à 1181/1181, 0 problème | `v413/outils_suite.txt` |
| `protect_assets.py check` ; tests des outils de paquet | OK ; 62 tests, 61 OK, 1 sauté | `v413/outils_suite.txt` |
| clang 18, contrôle syntaxique (18 fichiers de la suite) | 0 avertissement, 0 erreur ; 0 masquage ; 1 conflit regroupé déjà dans la base | `v413/syntaxe_suite.txt` |

**Constat corrigé pendant cette passe** : `Tools/Localization/loc_extract.py` ignorait `--check` et réécrivait les
catalogues. Sa première exécution a mis à jour `BRLocKeys.inl` (1142 → 1181 clés : la liste de recensement des
traductions en jeu) et `fr/Game.po`, restés en retard au commit (4) ; les `.locres` étaient déjà justes. `--check` est
maintenant en lecture seule (code 1 si un fichier n'est pas à jour).

## 17. Fichiers de la suite

- **Nouveaux** : `Public/BRLidarLogic.h`, `Private/BRLidarLogic.cpp`, `Private/BRLidarWorld.cpp`, `Public/BRLidar.h`,
  `Private/BRLidar.cpp`, `Private/BRLidarShapes.inl`, `Private/BRAutoTestV413Lidar.cpp` ; `Tools/Lidar/test_lidar.cpp`,
  `Tools/verify_lidar.py`, `Tools/Blender/build_lidar_assets.py`, `Tools/generate_lidar_sounds.py` ;
  `RawAssets/Meshes/SM_Lidar*.fbx`, `RawAssets/Sounds/S_Lidar*.wav` ; `Docs/v413/banc_lidar.txt`, `lidar_aile.txt`,
  `empreintes_missions.txt`, `lidar_silhouettes.png`, `lidar_ailes_6_graines.png`, `outils_suite.txt`, `syntaxe_suite.txt`.
- **Jeu** : `BRWorld`, `BRStoryWorld` (porte par partie), `BRMissionWorld`, `BRMissionLogic`, `BRMissionSolver`,
  `BRMission.h`, `BRMissionDevice`, `BRMissionText`, `BRMechLogic.h`, `BRTypes.h`, `BRLevels`, `BRKeys`,
  `BRPlayerController`, `BRCharacter`, `BREntity`, `BRHUD`, `BRSave` (champs de la porte), `BRStoryLogic.h`.
- **Tests et bancs** : `BRAutoTest.cpp/.h`, `BRAutoTestV413.cpp`, `Tools/Missions/test_mission_logic.cpp`,
  `Tools/verify_pitfalls.py`.
- **Langues** : `Content/Localization/Game/*/Game.po` et `.locres`, `BRLocKeys.inl`, polices CJK et arabe,
  `Docs/LOCALISATION_COUVERTURE.md` ; `Tools/Localization/loc_extract.py`.
- **Kit Blender** : `Tools/Blender/build_level0_kit.py`, `RawAssets/Level0Kit/` (`.blend`, placement, rapport, README).
- **Documents** : ce rapport, `GAMEPLAY_v4.13.md`, `HISTOIRE_PROGRESSION_v4.13.md`, `PLAN_PUBLICATION.md`, `README.md`.

## 18. Limitations de la suite

1. **Rien n'a été compilé ni joué dans Unreal** : rendu des points, lisibilité des formes, budget réel par image,
   collisions de l'aile, chute dans les trous et recul des Smilers restent à voir en jeu.
2. **Référence non visionnée** : l'adaptation suit la description écrite ; à comparer à la vidéo par quelqu'un qui peut
   la voir.
3. Captures du jeu (`v413_lidar_*`, `v413_graine_*`) : préparées, non exécutées. Les planches `lidar_silhouettes.png`,
   `lidar_ailes_6_graines.png` et les cartes de portes sont des rendus hors jeu (Blender, simulation), pas des captures.
4. Quatre joueurs, client arrivé en cours de partie, recul d'un Smiler : non automatisés.
5. Textes du Level 6 : traduits ici, marqués « fuzzy » (à relire par des locuteurs), polices régénérées.
6. Silhouettes : nuages de points tirés des maillages protégés, réduits ; leur lisibilité à 9 m reste à juger en jeu.
7. Documentation Epic, Steamworks et NVIDIA inaccessible (403) : rien contourné, rien vérifié sur ces sites.

## 19. Essais restant à exécuter (suite), dans l'ordre

1. Compiler (Editor puis Shipping, unités regroupées comprises) ; corriger les erreurs réelles.
2. `-BRAutoTest -BRAutoTestV413 -BRAutoTestStrict` : lire le rapport et les captures `v413_lidar_*` ; relever
   `StatRays` et `StatSeconds` (coût du scanner par image).
3. `-BRNetTest`, puis à la main : quatre joueurs au Level 6 ; un client qui rejoint pendant la mission LiDAR ; un client
   qui rejoint au Level 0 après l'ouverture de la porte.
4. À la main : deux nouvelles parties → deux portes ; mort et reprise → même porte ; `BRDoor`.
5. À la main au Level 6 : lisibilité des formes et des couleurs (y compris en simulation de daltonisme), réglages,
   touches et manette ; recul d'un Smiler au faisceau ; aide « PARASITES » sans le son.
6. Reprendre une vraie partie v4.12 arrêtée au Level 6 (en cours, puis terminée) : lire l'avis et la copie.
7. Comparer l'adaptation à la vidéo de référence.
