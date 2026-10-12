# Histoire et progression (v4.13)

Ce document décrit le fil narratif de la campagne et les règles qui le protègent. Tout ce qui est décrit ici est
**codé** ; la logique pure (`BRStoryLogic`) est vérifiée hors moteur par un banc (44 vérifications,
[`v413/banc_histoire.txt`](v413/banc_histoire.txt)). Le jeu lui-même n'a pas pu être lancé ici (pas d'Unreal) : les
étapes du test du jeu `-BRAutoTestV413` sont **préparées, non exécutées**.

## 1. L'équipe de L.

Avant les joueurs, une petite équipe est passée par les Backrooms. Elle signe « L. ». Elle avance vers une station que
les notes appellent **le dernier quai**, d'où un train sans nom part vers la sortie, ou vers un autre niveau.

L'équipe ne parle pas aux joueurs : elle laisse des **traces**.

- Une marque à la craie : un cercle barré et un « L. ». Là où elle est, l'équipe est passée.
- Des notes signées, qui disent ce qu'il faut faire, jamais où cliquer.
- Des inscriptions là où l'information est indispensable (les Poolrooms).

Une seule perte est racontée : M., restée au bord de l'eau des Poolrooms (`Note.L37.3`). Le reste de l'équipe a continué
par l'échelle.

Les joueurs ne « collectent » pas l'histoire : elle se lit sur le chemin qu'il faut prendre de toute façon. Aucune trace
n'est nécessaire pour avancer, sauf l'inscription des Poolrooms, qui est placée sur la route et toujours relisible dans
le carnet.

## 2. Le fil

Un seul ordre, le même dans toutes les versions et tous les canaux (`BRStory::Order`) :

```
Level 0 → Niveau 1 → Poolrooms (37) → Niveau 2 → 3 → 4 → 5 → 6 → 8 → 9 → 10 → 11 → dernier quai
```

- **Chapitre 1, « Le Seuil » (lot 1, publié)** : `0 → 1 → 37 → 2`, puis la fin du chapitre.
- **Campagne complète** (développement) : le fil entier jusqu'au dernier quai.

La disponibilité d'un niveau (lot publié ou non) ne change jamais l'ordre : elle dit seulement où le chapitre publié
s'arrête. Les redirections de la v4.12 (`37 → 4` devenu `→ 1`, `1 → 4` devenu `→ 2`) sont retirées : chaque sortie de
progression mène au niveau suivant du fil, et seulement à lui.

**Les Poolrooms sont une étape obligatoire.** Rien au-delà ne s'ouvre tant que leur sortie n'a pas été prise.

| Niveau | But (résultat concret) | Sortie de progression | Trace de L. |
|---|---|---|---|
| Level 0 | Trouver la porte de service (aucune liste d'objectifs) | porte de service → Niveau 1 | marques de craie le long du chemin, notes `Note.L0.3`, `.7`, `.8` |
| Niveau 1 | Rétablir la liaison de service | ascenseur → Poolrooms | « Les bassins, puis la vapeur. On tient bon. - L. » sur la cabine |
| Poolrooms | Découvrir le passage sec | échelle du passage sec → Niveau 2 | inscription peinte au carrelage ; marque et flèche au pied de l'échelle |
| Niveau 2 | Sécuriser le passage vers la station | porte de la station → fin du chapitre 1 (Niveau 3 plus tard) | « Station. On continue. » sur la porte |
| 3 | Rétablir l'alimentation centrale | → 4 | (chapitre 2, textes à venir) |
| 4 | Retrouver la destination dans les archives | → 5 | |
| 5 | Rouvrir la route de la chaufferie | → 6 | |
| 6 | Retrouver dans le noir le passage de l'équipe | échelle de l'aile d'essai → 8 | flèches bleues au sol jusqu'à l'aile ; journal de l'équipe (silhouette à suivre) ; « En bas, la terre. On descend. » gratté sur le montant de la porte du passage (lu à son ouverture) ; notes `Note.L6.4` à `.6` (scanner, Smilers, sol percé) |
| 8 | Franchir le passage que l'équipe n'a pas pu tenir | → 9 | |
| 9 | Rallumer le bon porche | → 10 | |
| 10 | Ouvrir la grange qui indique la route | → 11 | |
| 11 | Rejoindre le dernier quai | portique → dernier quai (deux fins) | |

Les buts sont affichés en tête des objectifs et du carnet (`BRMissionText::GoalTitle`) ; les étapes utiles suivent. À la
réussite, une ligne courte annonce la suite (`Mission.Solved.*`, `Mission.Story.*`).

## 3. Le chapitre 1, pas à pas

### Level 0 : le réveil

- **Réveil** (une fois par partie, au premier Level 0) : les paupières s'ouvrent en deux battements (environ 3 s), un
  sous-titre « [Le bourdonnement des néons. La moquette est humide sous vos mains.] », puis l'avis « Quelque part, un
  courant d'air froid passe. L'air ne devrait pas bouger ici. »
- **Aucune énigme, aucune liste.** La seule sortie est une **porte de service** réelle (cadre, charnières, battant sur
  pivot, poignées, collision), dans le mur d'enceinte, à 24 à 40 cellules de marche du départ (84 à 140 m), sans
  traverser de salle de fosses.
- **Une porte par partie.** Le Level 0 d'une partie a sa graine, tirée à la nouvelle partie : la disposition et la porte
  changent d'une partie à l'autre. Dans une même partie, elles ne changent plus : une mort, un retour, CONTINUER, un
  rechargement ou un client qui rejoint retrouvent la porte au même endroit (relue dans la sauvegarde, pas retirée).
- **Repères d'orientation**, tous redondants :
  - à l'oreille : le courant d'air sous la porte (`S_Draft`, boucle), distinct du bourdonnement ;
  - à l'œil : un filet de lumière froide sous le battant ; les marques de craie de L. toutes les trois cellules du
    chemin, avec une flèche quand un mur suit le chemin ;
  - par écrit : les notes de L. (« La seule sortie que nous ayons trouvée est une porte de service, une vraie, avec une
    poignée. Là où l'air devient froid. »).
- **Aide facultative** : après 7 minutes d'errance (minuterie, pas de Tick), si la porte n'est pas ouverte, un sous-titre
  donne la direction relative de l'air froid (« devant vous », « sur votre droite »…) et rappelle les marques de craie.
  Elle reste lisible dans le carnet.
- **Rencontres dosées** : une seule apparition ordinaire à la fois, première apparition et première coupure plus tard
  (150 s), ronde de la Bacteria après 200 s, deux Smilers pendant une coupure. Aucune rencontre n'est nécessaire pour
  sortir.
- **Sortie** : la porte s'ouvre à la main (poignée, charnières, air froid). En solo, le départ suit l'ouverture
  (1,3 s) ; en coopération, l'hôte ouvre la porte pour tout le groupe, puis le rassemblement habituel (8 m) et **un seul
  départ**.

### Niveau 1 : la liaison de service

La mission remet sous tension la liaison de service : les néons du parking se rallument un à un et l'ascenseur arrive.
Cette première preuve que « les installations répondent encore » est un événement narratif (`ServiceLink`). L'ascenseur
**descend aux Poolrooms**.

### Poolrooms : le passage sec

Aucune entité. Neuf secondes après chaque arrivée (sur chaque machine), l'inscription de L. s'affiche en sous-titre :

> Peint au carrelage, à la craie bleue : « Ne nagez pas. Les vannes font monter et baisser l'eau : quand les bassins sont
> justes, un chemin sec mène à l'échelle. Elle monte vers la vapeur. - L. »

Elle reste dans le carnet. Les bassins équilibrés découvrent le passage sec ; l'échelle monte vers la vapeur du
Niveau 2. La traversée est **enregistrée** comme une étape accomplie.

### Niveau 2 : la station

La mission des pressions dégage la porte de la station. Dans le chapitre 1, cette porte mène à la **fin du chapitre** :
la porte est marquée par L. (« Station. On continue. »), l'escalier descend vers un bourdonnement électrique, la suite
arrivera avec le chapitre 2. La partie enregistre la fin du chapitre ; rien n'est perdu quand le lot suivant arrive.

## 4. L'état narratif

Tenu **par l'hôte**, répliqué à chaque client (y compris un client qui arrive en cours de partie, par la première
réplication), sauvegardé dans la partie (format 5). Aucun Tick : il change aux transitions, à la porte et aux
événements.

| Champ | Sens |
|---|---|
| `Reached` | rang le plus loin atteint dans le fil |
| `Done` | étapes accomplies (une par niveau du fil) : niveau quitté par sa sortie de progression. **Une étape accomplie le reste** : au retour, la mission du niveau est déjà résolue (même solution que les bancs) et sa sortie est ouverte |
| `Events` | réveil, porte ouverte, aide d'orientation, inscription des Poolrooms, marque de la station, détour de migration, liaison de service : chacun une fois par partie, jamais rejoué au chargement |
| `ChapterEnds` | fins de chapitre vues (une par lot) |
| `Resume` | point de reprise de « Continuer » |
| `Parked` | ancienne partie : niveau où reprendre après la traversée des Poolrooms |
| `IntroSeed` | graine du Level 0 de la partie, tirée à la nouvelle partie (0 : pas encore tirée) |
| `DoorX`, `DoorY`, `DoorDir`, `DoorTier`, `DoorVersion` | porte de service retenue pour `IntroSeed` : cellule, face, palier du choix (bande visée, bande élargie, secours), version de placement ; relue à chaque chargement du Level 0 de la partie |

### Règle d'accès (hôte)

Chaque transition demandée à l'hôte (sortie, client, console, fin) passe par `ABRWorld::CanEnterLevel` :

| Session | Règle |
|---|---|
| **Histoire** (partie choisie, CONTINUER) | entrer dans un niveau **déjà atteint**, ou dans le suivant si l'étape précédente est accomplie ; jamais au-delà des Poolrooms avant leur traversée |
| **Rejouer** | un niveau déjà découvert dans la partie, ou un niveau que l'histoire permet déjà ; l'histoire n'avance pas |
| **Libre** (développement, tests) | disponibilité seule |

Un refus donne la raison dans la langue du joueur (« Ce niveau n'est pas encore atteint dans l'histoire. », « L'équipe
de L. est passée par les Poolrooms : la suite du chemin n'est pas encore ouverte. »).

### Menu

- **Nouvelle partie** : toujours au Level 0, histoire vide.
- **CONTINUER** : reprend l'histoire à son point de reprise (`ResumePoint` : la reprise si elle est disponible et
  permise, sinon le dernier niveau atteint disponible en remontant le fil, sinon le Level 0).
- **REJOUER** : le carrousel des niveaux découverts. On y revisite un niveau sans toucher à l'histoire ni à la session
  sauvegardée.

### Après une fin

« Continuer l'exploration » tire un niveau **déjà atteint** dans l'histoire (et disponible) : l'exploration ne fait
jamais progresser. Aucune destination au hasard ne fait avancer le fil ; la porte d'immeuble au hasard du Niveau 11 est
retirée.

## 5. Anciennes parties (formats 1 à 4)

Au premier chargement, la partie est copiée telle quelle (`<emplacement>_Format<N>`), puis migrée en mémoire
(`BRSaves::MigrateStory`, logique `BRStory::MigrateLegacy`) :

| Cas | Résultat |
|---|---|
| Session en cours au Level 0 (cassettes, cadrans, mission) | la session garde sa graine, ses objets ramassés et ses documents ; plus d'objectif : la porte de service s'ouvre sans condition |
| Niveaux dépassés | un niveau exploré avant le plus loin atteint est compté comme accompli (sauf les Poolrooms) |
| Traversée des Poolrooms | **créditée seulement sur preuve** : le niveau découvert juste après les Poolrooms est le Niveau 4, destination de leur échelle avant la v4.13. Une simple visite, ou la redirection vers le Niveau 1 de la v4.12, ne prouve rien |
| Partie allée au-delà des Poolrooms sans preuve | la session en cours est **garée intacte** (graine, objets, mission) ; la partie reprend aux Poolrooms ; à leur sortie, la progression d'avant reprend là où elle était (`TakeParkedSession`), avec un avis |
| Partie déjà commencée (plus d'une minute) | le réveil n'est pas rejoué |
| Génération de mission | **inchangée** pour les autres niveaux : monter `GenVersion` aurait fait passer toutes les missions v4.12 sauvegardées en mode historique ; seule la session du Level 0 change de mode |
| Partie sans graine de Level 0 (avant la porte tirée par partie) | la graine de la session du Level 0 en cours devient celle de la partie ; sinon, la prochaine visite du Level 0 la tire puis la garde |
| Session du Level 6 de la v4.12 (chaîne de balises) | l'ancien plan est reconstruit à l'identique et l'état converti : plaque de départ lue → scanner calibré ; balises allumées → postes réactivés ; mission terminée → terminée (**un Level 6 fini ne bloque jamais**). Sauvegarde copiée avant réécriture, avis « progression gardée ». Un état qui ne correspond à aucun ancien plan de cette graine n'est pas converti (mission reprise du début, copie et avis, comme en v4.12) |

Vérifié hors moteur (banc d'histoire ; banc des missions pour le Level 6 : 15 000 anciennes sessions converties et
conformes, 15 000 d'une autre graine refusées) et préparé dans le jeu (`-BRAutoTestV413`, étapes « migration » sur de
vrais fichiers du format 4, et « ancienne session convertie » pour le Level 6).

## 6. Fichiers

- Logique pure : `Source/Backrooms/Public/BRStoryLogic.h`, `Private/BRStoryLogic.cpp` ; banc `Tools/Story/test_story.cpp`.
- Monde : `Private/BRStoryWorld.cpp` (état répliqué, règle d'accès, porte de service, réveil, aide, inscription).
- Textes : `Private/BRMissionText.cpp` (buts, lignes de réussite, inscription), `Private/BRLevels.cpp` (notes),
  `Private/BRHUD.cpp` (carnet, fin du chapitre) ; 22 langues.
- Sauvegarde : `Public/BRSave.h` (format 5), `Private/BRSave.cpp` (migration).
- Level 6 : `Public/BRLidarLogic.h`, `Private/BRLidarLogic.cpp` (aile, scanner, logique pure ; banc
  `Tools/Lidar/test_lidar.cpp`), `Private/BRLidarWorld.cpp`, `Public/BRLidar.h`, `Private/BRLidar.cpp` (scanner) ;
  mission dans `BRMissionLogic` (banc `Tools/Missions/test_mission_logic.cpp`).
- Test du jeu : `Private/BRAutoTestV413.cpp`, `Private/BRAutoTestV413Lidar.cpp` (préparés, non exécutés).
