# Gameplay v4.13 : Level 0 sans énigme, progression stable, histoire, Level 6 sans lumière

Ce document décrit les règles **telles qu'elles sont codées** dans la v4.13. Il complète
[`GAMEPLAY_v4.12.md`](GAMEPLAY_v4.12.md), qui reste valable pour ce que la v4.13 ne change pas : ramassage, santé,
départ de groupe, lumière de gameplay, effets physiques des missions, entités. Le fil narratif et l'état de la partie
sont détaillés dans [`HISTOIRE_PROGRESSION_v4.13.md`](HISTOIRE_PROGRESSION_v4.13.md).

Les cotes et les règles sont vérifiées hors moteur (journaux dans [`v413/`](v413/)). **Rien de ce qui suit n'a été joué
dans Unreal** : rendu, collisions, déplacements et réseau sont à vérifier avec `-BRAutoTestV413` et `-BRNetTest`
(préparés, non exécutés ; voir le [rapport](RAPPORT_v4.13.md), § 1).

## 1. Ce qui change pour le joueur

| Avant (v4.12) | Maintenant (v4.13) |
|---|---|
| Level 0 : trois anomalies, trois cadrans, bouton, levier de route ; ou, pour une ancienne partie, six cassettes et une coupure filmée | Level 0 : **aucune énigme, aucune liste**. On se réveille, on suit l'air froid et les marques de craie, on ouvre une porte de service |
| Sortie du Level 0 : mur glitché → 1 ou échelle → 37, tirés dans les chunks, selon le levier | **Une seule sortie** : la porte de service, posée une fois par le monde, vers le Niveau 1 |
| Poolrooms facultatives ; échelle → 4 (redirigée vers 1 au lot 1) | Poolrooms **obligatoires** ; échelle du passage sec → Niveau 2 |
| Niveau 1 : porte → 2 et ascenseur → 4 (redirigé vers 2) | Niveau 1 : ascenseur → Poolrooms |
| L'hôte ne vérifie que la disponibilité d'un niveau | L'hôte vérifie l'**histoire** : niveau atteint, ou suivant d'une étape accomplie |
| Une mission se refait à chaque retour | Une étape accomplie le reste : au retour, la mission est déjà résolue, la sortie ouverte |
| Menu : tout niveau exploré comme point de départ | **CONTINUER** (l'histoire) ou **REJOUER** (un niveau découvert, sans toucher à l'histoire) |
| Destinations au hasard qui peuvent faire progresser | Exploration au hasard seulement après une fin, parmi les niveaux déjà atteints |
| Porte de service au même endroit pour une même graine de niveau | **Tirée à chaque nouvelle partie**, puis gardée pour toute la partie (mort, reprise, rechargement, client tardif) |
| Level 6 : chaîne de balises éclairées dans le noir | Level 6 **sans lumière** : scanner LiDAR (impulsion, scan focalisé), mire, postes, cages, portes du passage, sol percé, parasites (§ 6) |

## 2. Level 0

### 2.1 La porte de service

**Graine de partie.** Le Level 0 d'une partie a sa propre graine (`IntroSeed`), tirée à la **nouvelle partie** puis
sauvegardée dans l'histoire avec la porte retenue : cellule, face, palier du choix et version de placement
(`BRStory::DoorPlaceVersion` = 2). Deux nouvelles parties ont donc, en général, deux portes différentes ; une même
partie garde la sienne :

| Situation | Graine | Porte |
|---|---|---|
| Nouvelle partie (même lancée depuis un menu déjà au Level 0 : rechargement forcé) | nouvelle, tirée | nouveau tirage |
| Mort, retour au Level 0, CONTINUER, rechargement, hôte qui relance | celle de la partie | **relue** dans la sauvegarde, pas retirée |
| Client qui rejoint (même tardivement) | celle de l'hôte (état répliqué) | celle de l'hôte, ouverte si elle l'est déjà |
| REJOUER le Level 0 | celle de la partie | celle de la partie |
| Session libre (développement), `-BRSeed` | imposée | tirée pour cette graine ; ne remplace pas celle d'une partie déjà commencée |

Si la porte sauvegardée n'est plus un emplacement valide pour la graine (génération changée), un nouveau choix est fait
et journalisé (`[Story] Porte sauvegardee … plus valide`) : jamais de partie sans sortie.

**Placement** (`ABRWorld::PlaceServiceDoor`, à chaque chargement, sur l'hôte et sur chaque client avec la même
graine) :

1. parcours en largeur depuis la zone de départ, à travers les passages ouverts, **sans entrer dans une salle de
   fosses** ;
2. candidats : les cellules contre le mur d'enceinte (ni coin, ni zone de départ, ni cellule de mission) ; la porte est
   percée dans ce mur ;
3. la porte déjà retenue pour cette partie, si elle est encore candidate ;
4. sinon, **bande visée 24 à 40 cellules** de marche (84 à 140 m) : parmi ses candidats, celui que désigne la graine (plus
   petit hachage) ; bande vide : **bande élargie 16 à 56** ; vide aussi : **secours** déterministe, le candidat le plus
   proche de 32 cellules ;
5. le chemin (cellules du parcours) est gardé : il porte les marques de craie, et **rien n'y est posé** (ni cachette, ni
   meuble mural) : une capsule de joueur y passe toujours.

Vérifié hors moteur ([`v413/fosses_et_porte.txt`](v413/fosses_et_porte.txt)) :

- 500 graines : distance de marche min 24, moyenne 31, max 40 cellules ; bande visée retenue 500 fois, bande élargie et
  secours 0 fois ; de 9 à 114 candidats dans la bande par graine (moyenne 77,7) ;
- **diversité** : 118 emplacements différents (cellule et face) pour 500 graines, sur les quatre murs (ouest 124, sud 122,
  nord 121, est 133) ; sur **5000 graines** : 120 emplacements différents, c'est-à-dire tous les emplacements de bord
  possibles, et les quatre murs à parts presque égales (1240 à 1263), 0 problème, toujours dans la bande visée ;
- **reproductibilité** : même graine → même porte ; porte sauvegardée → reprise telle quelle : vérifié pour chaque
  graine ;
- 0 problème (chemin continu, sans fosse, porte dans un mur d'enceinte plein, capsule de 42 cm de rayon libre le long
  du chemin) ;
- planche de 12 graines : [`v413/cartes_porte_12_graines.png`](v413/cartes_porte_12_graines.png) (simulation, pas une
  capture du jeu).

**Outil de développement** : la commande console `BRDoor` (mode développeur) affiche la graine du niveau et celle de la
partie, la porte retenue (palier, reprise ou non), la longueur du chemin et le nombre de candidats par bande ; elle
liste chaque candidat dans le journal et les dessine 30 s dans le monde (rouge : retenu ; vert : bande visée ; jaune :
bande élargie ; gris : secours).

**Construction** (`ABRChunk::AddServiceDoorway`, `ABRExit::BuildServiceDoor`) :

| Élément | Cote |
|---|---|
| Ouverture dans le mur d'enceinte (0,6 m d'épaisseur) | 1,30 × 2,25 m |
| Passage libre (entre les montants du cadre) | 1,10 m |
| Battant | pivote sur ses charnières de 0 à **95°**, vers le sas ; collision qui suit le battant |
| Poignées, barre de poussée, plaque, trois charnières | quincaillerie métallique |
| Sas | palier, huit marches de béton qui descendent dans le noir, plafond bas |
| Lumière | filet froid sous le battant ; lampe froide du sas (260 lm, rayon 5,2 m) |
| Son | courant d'air (`S_Draft`, boucle) ; ouverture : pêne, charnières, air froid (`S_ServiceDoor`) |

**Usage.** Invite d'interaction ordinaire. En solo : la porte s'ouvre (animation et son), puis le départ 1,3 s plus
tard. En coopération : l'hôte ouvre la porte pour tout le groupe (état répliqué, chaque machine anime son battant), puis
le rassemblement habituel (8 m, même étage, sans mur), et **un seul départ**. Un client qui arrive après l'ouverture
reçoit l'état par la première réplication, et la porte de son chunk le prend à sa construction.

### 2.2 Mise en scène

- **Réveil** : une fois par partie, au premier Level 0 d'une session d'histoire : paupières (deux battements, ≈ 3 s),
  sous-titre, puis l'avis du courant d'air. Ni rejoué au chargement, ni pour une ancienne partie déjà commencée.
- **Repères** : air froid (son), filet de lumière (vue), marques de craie (une toutes les trois cellules du chemin, à
  partir de la troisième, jamais dans la zone de départ ni juste devant la porte ; avec une flèche quand un mur
  parallèle au chemin la porte). Une cellule du chemin sans mur pour la porter n'a pas de marque.
- **Aide facultative** : après 7 minutes au Level 0 sans que la porte soit ouverte (minuterie de l'hôte, pas de Tick),
  un sous-titre donne la direction relative de l'air (« devant vous », « derrière vous », « sur votre droite / gauche »)
  et rappelle les marques. Donnée une fois par visite ; relisible dans le carnet.
- **Rencontres dosées** :

  | Réglage | v4.12 | v4.13 |
  |---|---|---|
  | Première apparition, puis intervalle | 100 s | 150 s |
  | Ronde de la Bacteria | après 110 s | après 200 s |
  | Première coupure | 75 s | 150 s |
  | Smilers pendant une coupure | 3 | 2 |

  Une seule apparition ordinaire à la fois, en plus de la ronde de la Bacteria et des Smilers d'une coupure. Aucune
  rencontre n'est nécessaire pour sortir.
- **Notes** corrigées : plus de mur qui s'efface, plus d'échelle, plus de six cassettes ; trois notes de L. disent ce
  qu'il faut chercher (une vraie porte, là où l'air devient froid ; rien à fouiller ; les marques à la craie).
- **Carnet** : pas de mission ni de « 0/1 » ; ce que l'on sait de l'équipe de L., et l'aide si elle a été donnée.

## 3. Transitions validées par l'hôte

Toute demande de changement de niveau (sortie, client, console, fin) passe par `ABRWorld::CanEnterLevel`, sur l'hôte :

| Depuis | Demande | Histoire | Rejouer | Libre |
|---|---|---|---|---|
| Level 0, porte fermée | Niveau 1 | refusé | refusé (sauf déjà découvert) | permis |
| Level 0 | Poolrooms, Niveau 2 | refusé | idem | permis |
| Niveau 1 (atteint) | Poolrooms | refusé tant que le Niveau 1 n'est pas accompli | idem | permis |
| n'importe où | un niveau déjà atteint | permis (retour) | permis s'il est découvert | permis |
| avant la traversée des Poolrooms | Niveau 2 et au-delà | refusé (« L'équipe de L. est passée par les Poolrooms… ») | idem | permis |

Une sortie de progression accomplit l'étape du niveau quitté. Elle ne mène qu'au niveau suivant du fil ; une sortie de
retour mène à un niveau déjà atteint. Aucun raccourci n'existe dans les définitions des niveaux (vérifié par le banc de
contenu et par `-BRAutoTestV412`, étape « contenu »).

## 4. Le chapitre 1

| Niveau | But | Sortie de progression | Notes |
|---|---|---|---|
| 0 | Trouver la porte de service | porte de service → 1 | § 2 |
| 1 | Rétablir la liaison de service | ascenseur → 37 | la mission alimente et appelle l'ascenseur ; événement « liaison de service » |
| 37 | Découvrir le passage sec | échelle → 2 | sans entités ; inscription de L. 9 s après l'arrivée ; traversée à pied (règles v4.12) |
| 2 | Sécuriser le passage vers la station | porte de la station → fin du chapitre 1 | la porte est marquée par L. ; fin enregistrée |

Le banc de contenu vérifie, dans chaque canal : aucune sortie vers un niveau indisponible, le fil complet jusqu'à la fin
du chapitre (publié : `0 → 1 → 37 → 2 → fin du contenu`), aucun passage condamné au lot 1.

## 5. Menu et reprise

- **Nouvelle partie** : Level 0, histoire vide, réveil.
- **CONTINUER** : point de reprise de l'histoire. Après la fin du chapitre 1, il reste au Niveau 2 jusqu'à la
  publication du Niveau 3.
- **REJOUER** : un niveau découvert, choisi dans le carrousel. Ni l'histoire ni la session sauvegardée ne changent ; la
  partie ne se déplace pas.
- **Après une fin** : « Continuer l'exploration » tire un niveau déjà atteint (et disponible).

## 6. Level 6 « Lights Out » : sans lumière, au scanner LiDAR

But affiché : **« Retrouver dans le noir le passage de l'équipe »**. Sortie : l'échelle au fond de l'aile d'essai, vers le
**Niveau 8**. Logique pure : `BRMissionLogic` (plan, règles), `BRLidarLogic` (aile, scanner) ; monde :
`BRLidarWorld.cpp`, `BRMissionWorld.cpp` ; scanner : `ABRLidar` (`BRLidar.h/.cpp`).

### 6.1 Référence et adaptation

La demande cite une vidéo de référence : « Level 6: Lights Out (LiDAR) Teaching The Homie | Backrooms: Escape
Together ». **Elle n'a pas été visionnée ici** (aucune lecture de vidéo dans cet environnement). L'adaptation suit la
description écrite de la demande ; rien n'est repris du jeu de la vidéo (modèles, sons, textes, plans et règles sont
ceux du projet).

| Ce que la demande décrit (référence) | Ce qui est fait ici (adaptation) |
|---|---|
| Obscurité réelle, la lumière ne sert pas | aucune lumière de niveau ; la lampe ne donne qu'une lueur, la vision nocturne ne voit que de la neige, expliqué au sas d'entrée et dans une note ; **aucun réglage graphique n'est changé** |
| Scanner : impulsion large et scan focalisé | deux commandes configurables (§ 6.2) ; points qui s'estompent |
| Points de couleurs ET de formes | six catégories, chacune une couleur et une forme (lisible sans distinguer les couleurs) |
| Apprendre à un coéquipier (« teaching the homie ») | zone sûre au départ, sans entité tant que le scanner n'est pas calibré ; aides par étape ; flèches bleues de L. au sol |
| Postes, cages, portes, une seule bonne | quatre postes (les anciennes balises), quatre cages (silhouette et code), quatre portes ; une seule solution, déduite des informations vues |
| Zone dangereuse percée | six **vrais trous** (on tombe), passages d'au moins 140 cm, tous visibles au scanner : **aucun saut à l'aveugle** |
| Dernier passage aux parasites | émetteur de parasites à l'échelle ; aide de direction **sans le son** (HUD) |
| Silhouettes | points dérivés des modèles protégés (lus seulement, jamais modifiés) |
| Menace | Smilers, règle lisible et annoncée (§ 6.5) |

### 6.2 Le scanner

Chaque joueur porte son scanner (`ABRLidar`, local : les points ne sont pas répliqués ; ce que le scanner **valide**
— mire, cages, recul d'un Smiler — est vérifié par l'hôte).

| Commande | Touches par défaut | Effet |
|---|---|---|
| **Impulsion** (`LidarPulse`) | clic droit, `G` ; manette : gâchette gauche | 1400 rayons répartis sur 7 images (220 par image), cône de 100 × 64°, portée 26 m, recharge 1,6 s. **S'entend** (bruit de 18 m de rayon) |
| **Scan focalisé** (`LidarFocus`, maintenir) | `X`, clic molette ; manette : gâchette droite | 36 rayons par image dans un cône de 7° (demi-angle) ; relève les détails (mire, cages, plaques des portes, silhouettes) |

Les deux actions se reconfigurent dans le menu des touches, comme les autres.

**Points** (plus grand vide de l'impulsion : 1,36° ; capacité 24 000 points en 8 groupes d'âge, le plus ancien est vidé
et réutilisé) :

| Catégorie | Couleur | Forme |
|---|---|---|
| Décor (murs, sol, plafond) | blanc bleuté | petit carré |
| Danger (bord d'un trou, fond d'une fosse) | orange | triangle |
| Mécanisme de mission, objet | vert | losange |
| Entité | rouge | croix |
| Repères de L. au sol | bleu | chevron, dans le sens du chemin |
| Silhouettes et inscriptions (cages, plaques) | ambre | point |

**Réglages** (menu Paramètres) : taille des points 0,5 à 2 ; contraste 0,5 à 2 ; persistance 4 à 30 s (12 s par défaut).
Un groupe reste à plein éclat pendant le premier quart de la persistance, puis s'éteint linéairement.

**HUD** : cône de visée, recharge, « RELEVÉ AU SCANNER » pendant un relevé, et l'aide **PARASITES** (direction de
l'émetteur, « derrière vous ») qui remplace le son pour qui ne l'entend pas.

**Budget** : une instance groupée par catégorie et par groupe d'âge ; aucune lumière, ombre, éclairage indirect ni ray
tracing par point. Le banc mesure au plus 257 rayons et 377 points ajoutés par image
([`v413/banc_lidar.txt`](v413/banc_lidar.txt)).

### 6.3 La mission

| Étape | Ce qu'on fait | Règle |
|---|---|---|
| 1. Calibrer | dans la zone sûre près du départ, viser la **mire** et maintenir le scan focalisé | relevé validé par l'hôte (portée 9 m, ligne de vue) ; avant lui, aucune entité n'apparaît |
| 2. Postes | suivre les flèches bleues ; maintenir Interagir sur chacun des **quatre postes de confinement** (ordre libre) | la manivelle garde sa progression si on la lâche ; elle fait du bruit ; un poste allume **sa** cage |
| 3. Cages | maintenir le scan focalisé sur chaque **cage** sous tension | la silhouette apparaît en points ambre, le code est sur la plaque ; le carnet garde chaque relevé |
| 4. Journal | lire le **journal de l'équipe de L.** devant l'aile | il dit quelle silhouette suivre |
| 5. Porte | ouvrir la **porte du passage** qui porte cette silhouette **et** le code de sa cage | une seule porte convient ; une porte fausse refuse (« {N} éléments ne correspondent pas »), **rien n'est perdu**, elle se réarme |
| 6. Traverser | scanner avant de marcher, contourner les trous, suivre les parasites jusqu'à l'échelle | l'échelle mène au Niveau 8 |

Les portes restent verrouillées tant que les postes, les cages et le journal n'ont pas été vus. Sans le journal, ou sans
les cages, au moins deux portes restent possibles : la solution ne se devine pas (vérifié sur 3000 graines,
[`v413/banc_missions.txt`](v413/banc_missions.txt)). Les silhouettes sont celles du Hound, du Faceling, du Partygoer, du
Skin-Stealer, du Wretch et du Clump.

### 6.4 L'aile d'essai

Salle fermée de **4 × 4 cellules** (14 × 14 m), posée d'après la graine seule (la même chez tous les joueurs) dans un
chunk à deux chunks du départ, l'entrée tournée vers lui.

- **Entrée** : quatre ouvertures, une par cellule, fermées par les quatre portes du passage ; les trois autres côtés sont
  des murs pleins. Un **couloir garanti** relie le départ à l'entrée (18 à 41 cellules de marche sur 500 graines,
  [`v413/lidar_aile.txt`](v413/lidar_aile.txt)).
- **Zone percée** (trois premières rangées) : six trous de 210 cm aux angles intérieurs des dalles ; entre deux trous, au
  moins **140 cm** de passage. Les trous sont ceux des salles de fosses (sol réellement ouvert, chute mortelle, évitement
  par les entités).
- **Alcôves** (dernière rangée) : quatre alcôves séparées par des murs ; l'échelle et l'émetteur de parasites sont dans
  l'une d'elles.

Vérifié (banc LiDAR, 2000 graines) : six trous partout ; marche réelle d'une capsule de 42 cm sur une grille de 5 cm
(200 graines) : depuis chaque porte, toutes les alcôves sont atteintes sans passer au-dessus d'un trou. Planche de six
graines, vue de dessus : [`v413/lidar_ailes_6_graines.png`](v413/lidar_ailes_6_graines.png) (simulation de la grille,
pas une capture du jeu).

### 6.5 Menace lisible : les Smilers

- Seule espèce du niveau (trois au plus) ; **aucune** apparition tant que le scanner n'est pas calibré.
- **L'impulsion s'entend** et les attire : un Smiler qui l'entend vient voir ; son approche s'annonce une fois (souffle).
- **Le faisceau focalisé maintenu 0,6 s** sur un Smiler le fait **reculer de 7 m** ; il ne revient pas avant 6 s.
  L'hôte vérifie la portée (26 m), la ligne de vue et l'axe de visée.
- La règle est écrite par L. (notes `Note.L6.4` à `.6`) avant d'être nécessaire.

### 6.6 Coopération, sauvegarde, anciennes parties

- **Hôte** : plan de la mission, relevés, postes, portes, recul des Smilers ; l'état est répliqué, y compris à un client
  qui arrive en cours de partie. Chaque joueur scanne pour lui-même.
- **Sauvegarde** : l'état de la mission est sauvegardé avec la partie ; la reprise retrouve l'aile et
  le plan (même graine).
- **Parties de la v4.12** (chaîne de balises) : l'ancien plan est reconstruit à l'identique, puis converti — plaque de
  départ lue → scanner calibré ; balises allumées → postes réactivés ; mission terminée → **terminée** (un Level 6 fini
  ne bloque jamais). La sauvegarde d'origine est **copiée** avant d'être réécrite, et un avis dit que la progression est
  gardée. `GenVersion` n'est pas monté : les autres niveaux gardent leurs plans (empreintes identiques avant et après,
  [`v413/empreintes_missions.txt`](v413/empreintes_missions.txt)).

## 7. Tests du jeu (préparés, non exécutés)

`-BRAutoTestV413` (seul) ou dans `-BRAutoTest` ; fichiers `Source/Backrooms/Private/BRAutoTestV413.cpp` et
`BRAutoTestV413Lidar.cpp`. Le détail est dans le [rapport](RAPPORT_v4.13.md), § 7 et § 15.
