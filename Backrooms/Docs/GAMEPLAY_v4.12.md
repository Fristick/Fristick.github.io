# Gameplay v4.12 : règles finales, effets physiques des missions et contenu publié

Ce document décrit les règles **telles qu'elles sont codées** dans la v4.12 :

- missions : `BRMissionLogic.cpp` ;
- effets physiques : `BRMechLogic.cpp`, `BRMissionDevice.cpp` ;
- lumière de gameplay : `BRLightLogic.h`, `BRWorld.cpp` ;
- départs de groupe : `BRGatherLogic.cpp` ;
- transactions : `BRTxnLogic.cpp` ;
- disponibilité des niveaux : `BRContentLogic.cpp`.

Il remplace [`GAMEPLAY_v4.11.md`](GAMEPLAY_v4.11.md), qui reste valable pour ce que la v4.12 ne change pas : règles des
missions, variantes, entités.

Les cotes et les règles sont vérifiées par les bancs hors moteur (journaux dans [`v412/`](v412/)). **Rien de ce qui suit
n'a été joué dans Unreal** : rendu, collisions, déplacements et réseau sont à vérifier avec `-BRAutoTestV412` et
`-BRNetTest` (préparés, non exécutés ; voir le [rapport](RAPPORT_v4.12.md), § 1).

## 1. Ce qui est disponible

Les douze niveaux restent dans le projet ; la version publique n'ouvre que le **premier chapitre, « Le Seuil »** : Niveaux
0, 1, 2 et 37. Les autres arrivent par mises à jour de contenu ([plan de publication](PLAN_PUBLICATION.md)).

| Niveau | Chapitre (lot) | Version publiée | Test interne | Développement |
|---|---|---|---|---|
| 0, 1, 2, 37 | 1 — Le Seuil | ouvert | ouvert | ouvert |
| 3, 4 | 2 — La Station et les Bureaux | fermé | ouvert | ouvert |
| 5, 6 | 3 — L'Hôtel et le Noir | fermé | fermé | ouvert |
| 8, 9, 10 | 4 — Les Grottes, la Banlieue et le Champ | fermé | fermé | ouvert |
| 11 | 5 — La Ville | fermé | fermé | ouvert |

**Progression du chapitre 1.**

- Niveau 0, levier de route : mur vers le Niveau 1, ou échelle vers les Poolrooms.
- Poolrooms : l'échelle du passage sec **remonte au Niveau 1** (elle mène au Niveau 4 dans la campagne complète).
- Niveau 1 : l'ascenseur alimenté par la mission **descend au Niveau 2** (il mène au Niveau 4 dans la campagne complète) ;
  la porte vers le Niveau 2 s'ouvre aussi.
- Niveau 2 : la porte des vannes (vers le Niveau 3) mène à la **fin du contenu disponible**. Le départ de groupe se fait
  normalement, puis un écran annonce la fin du chapitre « Le Seuil ». La fin est enregistrée dans la partie ; on choisit
  de continuer à explorer (niveau disponible au hasard) ou de revenir au menu.

**Passage condamné.** Une sortie vers un niveau d'un chapitre suivant, sans redirection prévue :

- planches en croix, lueur rouge ;
- invite « Passage condamné (…) : pas encore accessible dans cette version » ;
- aucun départ possible.

La mission du niveau garde alors une autre sortie : aucune mission ne demande d'atteindre un niveau non publié. Le
chapitre 1 n'a aucun passage condamné.

**Parties d'une autre version.** Une partie dont le dernier niveau n'est pas disponible ici (partie v4.11 au Niveau 5,
partie d'une version de test) reprend au dernier niveau disponible exploré, sinon au Niveau 0. Le fichier est copié
avant tout, un avis l'explique, la carte de sauvegarde l'annonce, et rien n'est retiré de la partie. Le niveau redevient
sélectionnable quand il est publié.

**Coopération entre versions.** Un joueur ne rejoint que s'il a la même version (protocole 412) et le même contenu
(niveaux disponibles) que l'hôte ; sinon il lit un refus clair dans sa langue :

- autre version : « l'hôte a la version 4.12 du jeu, vous avez la version … » ;
- autre contenu : « l'hôte joue avec la version publiée (contenu jusqu'au chapitre 1), vous avez … ».

## 2. Règles communes (changements v4.12)

### 2.1 Ramassage et inventaire

**Une seule règle de place.** Vérifier la place, la réserver et ranger l'objet suivent le même calcul, dans cet ordre :

1. piles partielles du même objet ;
2. emplacement d'équipement libre et compatible (lampe en main ou à la ceinture, lampe frontale, gilet) ;
3. cases vides des poches, puis du sac.

Avant la v4.12, une lampe pouvait être refusée (« inventaire plein ») alors que la main était libre.

**Place réservée.** Pendant qu'une demande de ramassage attend la réponse de l'hôte (au plus 10 s), un déplacement dans
l'inventaire qui prendrait la place de l'objet est refusé (« Place réservée : … est en cours de ramassage »).

**Un objet accepté n'est jamais perdu.**

- S'il n'a plus de place à l'arrivée de la réponse (réponse tardive, réveil entre-temps), il est **mis de côté** : une
  réserve de 8 emplacements, visible dans l'inventaire et gardée dans la sauvegarde.
- Il se range tout seul dès qu'une place se libère.
- Le joueur accuse réception à l'hôte. S'il quitte la partie avant cet accusé, l'hôte **rend l'objet au monde** à sa
  place, sous un nouvel identifiant ; ses effets sont compensés (une cassette est décomptée).

**Vie du joueur.** Une réponse qui concerne une vie terminée (demande faite avant un réveil confirmé par l'hôte) ne touche
pas l'inventaire de la nouvelle vie.

### 2.2 Santé

Les coups, soins, morts, relevés et réveils partagent une **révision de santé** tenue par l'hôte. Une réponse ou un envoi
plus ancien que le dernier état reçu n'écrase jamais la santé courante. Un soin accepté dans un niveau précédent est
compté une fois (objet consommé dans la même vie), sans rejouer d'effet ni changer la santé du nouveau niveau.

Rien n'est affiché en permanence : endurance et santé mentale restent dans l'inventaire (Tab).

### 2.3 Retour des demandes à l'hôte

Sous l'invite d'interaction, une pastille discrète donne l'état de la dernière demande en ligne (ramassage, mécanisme,
soin, départ) :

| État | Texte | Durée |
|---|---|---|
| en attente | « En attente de l'hôte… » | jusqu'à la réponse |
| sans réponse | « Toujours sans réponse de l'hôte (connexion lente ?) » | après 5 s |
| accepté | « Accepté : … » | 1,4 s |
| refusé | « Refusé : … », avec la raison (trop loin, déjà pris, verrouillé…) | 3,5 s |

Seul, l'action s'applique tout de suite : pas de pastille d'attente. Une action maintenue (manivelle, observation) garde
sa barre de progression.

### 2.4 Départ de groupe

Le départ se fait **autour de la sortie choisie** : porte, échelle ou grange.

**Rassemblement.** Un joueur est compté comme rassemblé dans l'un de ces deux cas :

- **sur l'échelle**, à n'importe quelle hauteur de sa montée : dans la colonne de son conduit, 70 cm autour de la prise.
  Le grimpeur arrivé au sommet est donc compté sans redescendre ;
- **au même étage** que le pied de la sortie (2,6 m d'écart vertical au plus, comme avant), à moins de 8 m du point de
  rassemblement, avec ce point en vue.

Le point de rassemblement dépend de la sortie : 1 m devant le pied d'une échelle, 5,2 m devant une grange, devant une
porte. La demande et le rassemblement visent le même point (90 cm au-dessus). Un joueur séparé par un plafond n'est
jamais compté : la tolérance verticale n'a pas été élargie.

**Au sommet d'une échelle.**

- Une seule demande de départ par arrivée au sommet (avant : une par image) ; on ne monte pas plus haut et on attend,
  l'attente est annoncée.
- Redescendre d'un mètre annule le départ (initiateur seulement) ou réarme la demande.
- Un refus (sortie verrouillée) laisse le joueur au sommet, sans redemander et sans le faire tomber.
- Sans réponse en 3 s, l'attente s'arrête.

Les autres règles du départ sont inchangées : joueur à terre emmené, joueur en chargement non attendu, annulation après
90 s, initiateur à plus de 25 m, plus personne debout.

### 2.5 Lumière de gameplay

Les règles qui dépendent de la lumière lisent une **lumière de gameplay** commune à toutes les machines :

- un Smiler hors poursuite se dissipe au-dessus de 0,5 ;
- une entité de l'ombre n'apparaît pas au-dessus de 0,12 ;
- le drain de santé mentale suit la même valeur.

**Sources.** Les plafonniers (ils suivent les coupures) et les sources de mission allumées :

| Source | Portée | Intensité |
|---|---|---|
| balise du Niveau 6 | 9 m | 2 |
| sortie de secours éclairée (Niveau 6) | 7 m | 1,6 |
| porche allumé (Niveau 9) | 7 m | 1,6 |

Ces sources suivent l'état répliqué de la mission. Elles sont **autonomes** : une coupure ne les éteint pas.

**Loi de lumière.** Apport d'une source = (1 − d/R)² × intensité, nul au-delà de la portée. Un mur entre la source et le
point annule l'apport.

Conséquence annoncée par l'aide du Niveau 6 : à moins de 4 m d'une balise allumée, un Smiler qui ne vous poursuit pas se
dissipe (sous-titre « [Un Smiler se dissipe dans la lumière] »). Le faisceau de la lampe braqué sur un Smiler reste un
danger (avertissement, puis charge).

### 2.6 Mécanismes hors de vue

Un mécanisme dont la zone n'est plus construite est **masqué** : sans Tick, sans collision ni animation. Il garde son
état logique. Une action de l'hôte reçue pendant ce temps ne relance pas d'animation ; à la réapparition, le mécanisme
reprend directement sa position, sans rejouer le mouvement. Seuls les mécanismes animés proches d'un joueur s'animent.

### 2.7 Placement et reprise des missions

**Placement.** L'hôte essaie trois placements, dans l'ordre :

1. le placement normal ;
2. la variante de secours déterministe ;
3. un **module de secours** près du départ : mécanismes sur les murs des premières cellules atteintes (ou sur des
   poteaux), sorties gardées à la cellule atteinte la plus lointaine. Ce module est toujours possible.

Une mission n'est donc plus abandonnée, et les sorties ne s'ouvrent plus en silence. Le placement retenu est répliqué :
chaque client place exactement de la même façon.

**Avis** (montré une fois par niveau) :

| Avis | Cas |
|---|---|
| module de secours | « les mécanismes sont regroupés près du point de départ » |
| mission non restaurée | état sauvegardé incompatible avec le plan (autre version). La sauvegarde d'origine est **copiée avant d'être réécrite** ; la mission recommence |
| mécanismes réaménagés | placement d'une autre version (`PlaceVersion`) ; la progression est gardée |
| mission indisponible | ne devrait jamais arriver ; la sortie reste ouverte, explicitement |

### 2.8 Carnet

- Chaque colonne a sa propre position et son propre défilement.
- **Colonne de gauche** : étapes, objets de l'équipe, objectif facultatif, aide. Le bouton AIDE est fixé en bas, toujours
  visible. Quand on demande de l'aide, la colonne défile d'elle-même jusqu'à l'indice, sous un en-tête « Aide ».
- **Colonne de droite** : observations. Son message « aucune observation » commence en haut de sa colonne, quel que soit
  le contenu de la gauche.
- Tout reste consultable à la molette, avec retour à la ligne ; la taille du texte n'est pas réduite pour faire tenir le
  contenu.

### 2.9 Voyants des sorties

Une sortie gardée par la mission porte un voyant :

- **rouge** tant qu'elle est verrouillée ;
- **vert** une fois ouverte.

Il suit le même état que la validation de la sortie. Une sortie libre n'a pas de voyant.

## 3. Effets physiques des missions

Les règles des missions ne changent pas (voir v4.11) ; leur réalisation dans le décor, si. Les surfaces d'eau, les sols
et les collisions suivent **un seul état**, celui de la mission (tenu par l'hôte, répliqué, sauvegardé). Les volumes
d'eau locaux (`ABRWorld::WaterAt`) l'emportent sur l'eau du niveau. La nage, la profondeur, l'immersion de la caméra,
l'apnée, le corps qui flotte et le contrôle de noyade de l'hôte passent tous par eux.

### 3.1 Poolrooms (Niveau 37) — deux bassins et un passage sec

**Bassins.** Les deux bassins sont adossés au mur de la vanne A ; les vannes sont fixées sur leur face avant (vanne B à
58 cm de la vanne A).

| | Bassin A (haut) | Bassin B (bas) |
|---|---|---|
| Fond | 55 cm au-dessus de l'eau des canaux | 15 cm au-dessus |
| Règle graduée | marques 0 à 5 | marques 0 à 7 |
| Profondeur | 1,20 m (des vannes au mur) | 1,20 m |
| Largeur | 1,04 m | 1,44 m |

- Une marque vaut 15 cm ; la marque 0 est à 8 cm du fond.
- Chaque règle porte une colonne d'eau et un flotteur.
- La ligne d'eau sur les parois suit la surface.

La surface d'un bassin est celle de la graduation calculée par la mission. Elle s'anime à 26 cm/s : un cran de vanne se
voit couler en environ 0,6 s.

**Circulation visible.**

- Un jet de A vers B pendant le transfert.
- Une vidange de B vers le canal.
- Un débordement de B au-delà de la marque 7, qui est aussi l'avertissement de la mission.
- Deux roues à aubes, dans un regard, tournent avec le transfert et la vidange : elles montrent le sens et la force du
  courant.
- Un bruit d'eau quand ça coule.

**Passage sec** devant l'échelle de sortie.

| | |
|---|---|
| Sas | 2,8 m de long, 1,7 m de large ; dallage au ras des trottoirs ; murets de 1,2 m, garde-corps à 2 m ; marche d'entrée |
| Fermé | le déversoir retient 1 m d'eau sur le dallage |
| Mission résolue | le déversoir descend, le sas se vide à 25 cm/s (environ 5 s) ; l'eau finit 30 cm sous le dallage |

On traverse ensuite **à pied, au sec**, sans plonger.

Toujours **aucune entité** dans les Poolrooms.

### 3.2 Niveau 8 — passerelle sur une vraie interruption

| Élément | Cotes |
|---|---|
| Palier de l'échelle de sortie | 1,1 m de profondeur, 1,4 m de haut |
| Interruption | 2,2 m |
| Appui d'arrivée | 0,9 m de profondeur, 0,9 m de haut |
| Marches derrière l'appui | deux marches de 30 cm (franchies sans sauter, marche maximale 35 cm) |
| Appuis | 2,2 m de large : on peut passer à côté, au sol, des deux côtés |
| Tablier | 1,6 m de large, 12 cm d'épaisseur ; appuyé sur 20 cm de l'appui ; pente d'environ 9° une fois posé |

**Tablier levé** : il est vertical contre le palier. Le palier est hors d'atteinte : un saut gagne 66 cm, le palier en
fait 140.

**Treuils.** Les trois treuils, faits dans l'ordre, abaissent le tablier en environ 3 s. Il est relié au portique par
deux câbles, et il porte (collision) avec des mains courantes.

**Chute.** Tomber de l'appui ou du tablier ramène au sol de la grotte (1,4 m au plus) : sans danger, on remonte par les
marches. Aucune mort pendant l'ouverture.

**Créatures** : elles suivent le même état que les collisions.

- Tablier posé : elles passent par les marches, l'appui et le tablier.
- Tablier levé : elles attendent au pied.

### 3.3 Autres niveaux

| Niveau | v4.12 |
|---|---|
| 1 | L'ascenseur alimenté allume son voyant d'appel et sonne à son arrivée (sous-titre « [Sonnerie : l'ascenseur est arrivé] »). Voyant rouge ou vert de l'ascenseur. |
| 2 | La vapeur des conduites est faite de voiles translucides animés (plus de sphères opaques). |
| 6 | Balises et sortie de secours éclairée : vraies lumières **et** lumière de gameplay (§ 2.5). |
| 9 | Porche allumé : vraie lumière et lumière de gameplay. |
| 3, 4, 5, 10 | Inchangés en v4.12 (défaut électrique, archives, chaudière, moulin) : prévus avec la finition de leur chapitre (§ 5 du rapport). |

### 3.4 Correction d'échelle

Une échelle posée sur un trottoir (68 cm, Poolrooms) visait un sommet au-dessus du plafond : la montée se bloquait. La
hauteur est désormais mesurée depuis le pied de l'échelle (missions et chunks).

## 4. Les quatre niveaux du chapitre 1

Règles, indices et variantes inchangés depuis la v4.11 ; voici ce qui change pour le joueur.

| Niveau | Mission | Sortie dans la version publiée | Changements v4.12 |
|---|---|---|---|
| **0 — Threshold** | Néons anormaux, panneau de trois cadrans, levier de route | Niveau 1 (mur) ou Poolrooms (échelle) | Départ par l'échelle : le grimpeur est compté au sommet ; voyants des sorties ; pitfalls et zone de 112 m inchangés |
| **1 — Habitable Zone** | Schéma, fusibles, disjoncteurs, appel de l'ascenseur | Ascenseur et porte → Niveau 2 | Ascenseur redirigé vers le Niveau 2 ; voyant d'appel, sonnerie |
| **2 — Utility Halls** | Plaque des pressions, manomètres, trois vannes | Fin du contenu disponible | Vapeur translucide ; écran de fin du chapitre ; échelle de retour vers le Niveau 1 |
| **37 — Poolrooms** | Marques, courant, deux bassins | Échelle → Niveau 1 | Bassins réels, passage sec qui se vide (§ 3.1) ; aucune entité |

## 5. Entités

Inchangées depuis la v4.11 (tableau complet dans [`GAMEPLAY_v4.11.md`](GAMEPLAY_v4.11.md), § 3), sauf deux points :

- **Smiler** : la lumière ambiante qui le dissipe inclut maintenant les balises et les sources de mission (§ 2.5) ;
- les **apparitions dans l'ombre** et le **drain de santé mentale** lisent la même lumière de gameplay.

Au chapitre 1, six entités apparaissent : Smiler, Bacteria, Faceling, Hound, Wretch, Clump. Les neuf restent dans le
projet pour les chapitres suivants.
