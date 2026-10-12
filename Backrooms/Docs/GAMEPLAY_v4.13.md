# Gameplay v4.13 : Level 0 sans énigme, progression stable, histoire

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

## 2. Level 0

### 2.1 La porte de service

**Placement** (`ABRWorld::PlaceServiceDoor`, une fois par chargement, sur l'hôte et sur chaque client avec la même
graine) :

1. parcours en largeur depuis la zone de départ, à travers les passages ouverts, **sans entrer dans une salle de
   fosses** ;
2. candidats : les cellules du bord de la zone (pas les coins) dont le mur d'enceinte est plein ;
3. fenêtre de distance de marche : **24 à 40 cellules** (84 à 140 m). Parmi les candidats de la fenêtre, le plus petit
   tirage de la graine ;
4. repli déterministe si la fenêtre est vide : la cellule de bord atteignable la plus proche de 32 cellules (palier 1) ;
5. le chemin (cellules du parcours) est gardé : il porte les marques de craie, et **rien n'y est posé** (ni cachette, ni
   meuble mural) : une capsule de joueur y passe toujours.

Vérifié hors moteur sur 500 graines ([`v413/fosses_et_porte.txt`](v413/fosses_et_porte.txt)) :

- distance de marche : min 24, moyenne 31, max 40 cellules ;
- repli : 0 fois sur 500 ;
- portes réparties sur les quatre murs (ouest 124, sud 122, nord 121, est 133) ;
- 0 problème (chemin continu, sans fosse, porte dans un mur d'enceinte plein, capsule de 42 cm de rayon libre le long
  du chemin) ;
- planche de 12 graines : [`v413/cartes_porte_12_graines.png`](v413/cartes_porte_12_graines.png) (simulation, pas une
  capture du jeu).

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

## 6. Tests du jeu (préparés, non exécutés)

`-BRAutoTestV413` (seul) ou dans `-BRAutoTest` ; fichier `Source/Backrooms/Private/BRAutoTestV413.cpp`. Le détail est
dans le [rapport](RAPPORT_v4.13.md), § 7.
