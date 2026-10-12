# Rapport v4.13 : Level 0 sans énigme, progression stable, histoire de l'équipe de L.

Travail fait le 12 octobre 2026 sur la branche `backrooms`.

| | |
|---|---|
| **Commit de base** | `90bd60c` (v4.12, 9/9), vérifié avant toute modification ([audit](AUDIT_v4.13.md)) |
| **Commits v4.13** | huit commits « v4.13 (n/8) », listés ci-dessous |
| **Documents** | [Audit](AUDIT_v4.13.md) · [Histoire et progression](HISTOIRE_PROGRESSION_v4.13.md) · [Gameplay](GAMEPLAY_v4.13.md) · [Plan de publication](PLAN_PUBLICATION.md) · [Kit Blender du Level 0](../RawAssets/Level0Kit/README.md) · journaux réels dans [`Docs/v413/`](v413/) |

Commits v4.13 :

- `7c4426a` (1/8) : audit, ordre narratif unique (`BRStoryLogic`), Level 0 hors des missions, sorties du chapitre ;
- `7673f7c` (2/8) : histoire tenue par l'hôte, porte de service, sauvegarde format 5, CONTINUER / REJOUER ;
- `42e7d0b` (3/8) : fil narratif de L., objectifs formulés comme des résultats, traductions ;
- `2694ced` (4/8) : tests du jeu v4.13 (`-BRAutoTestV413`, `-BRNetTest`), version du projet 4.13 ;
- `ade878c` (5/8) : kit Blender du Level 0 (scripts, modules, textures, FBX, `.blend`, placement, rendus) ;
- `77b69c7` (6/8) : journaux réels (`Docs/v413/`), planche des portes ;
- `9f01682` (7/8) : histoire et progression, gameplay, plan de publication ;
- **8/8 (ce rapport, README)** : le dernier de la branche (`git log -1`).

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
