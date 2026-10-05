# État des lieux v4.5 → v4.6 : fosses du Niveau 0

État des lieux fait le 5 octobre 2026 sur la branche `backrooms` (commit `ca02e76`, v4.5). Il part de la lecture des
sources, puis de la génération rejouée hors moteur (`Tools/verify_pitfalls.py`) et des modèles livrés, ouverts dans
Blender. **Unreal Engine n'est toujours pas disponible ici** (conteneur Linux sans GPU) : rien n'a été compilé ni lancé.

La page du wiki (`backrooms-wiki.wikidot.com/level-0`, section « Hole Variation ») est **bloquée par le proxy réseau** du
conteneur : je me suis fondé sur la description de la demande. Salles jaunes, plusieurs fosses carrées profondes en
grille, passages praticables entre elles, même papier peint, même moquette, même plafond et mêmes néons.

## Ce que fait le code v4.5

| Point | Constat |
|---|---|
| Sol | `ABRChunk::Build` pose **une seule dalle** de 20 cm par chunk hors bassins (`AddBox(D.Floor, ..., (ChunkW, ChunkW, 20))`) : aucune ouverture possible. |
| Requêtes de sol | `IsWalkable` = `!IsSolid` (cellule entière). `FloorZAt` ne connaît que les bassins (Niveau 37) et les trottoirs. |
| Navigation | A* à 4 directions sur les **centres de cellule** (3,5 m au Niveau 0). Les entités vont en ligne droite vers le joueur si un balayage de sphère ne touche rien. **Un trou ne bloque pas un balayage** : sans changement, elles traverseraient le vide. |
| Mort | Santé gérée chez le joueur (`ABRCharacter::Die`, puis `ServerSetDead`). Les positions sont envoyées par les clients et reprises telles quelles par le serveur (`bServerAcceptClientAuthoritativePosition`). Le serveur connaît donc la position de chacun et peut constater une chute. |
| Après la mort | Seul : retour au Niveau 0 avec une **nouvelle graine** (les objectifs repartent de zéro). En équipe : à terre 30 s (réanimation possible), puis réveil au point de départ, cellule (0, 0). |
| Sauvegardes | Elles ne stockent **ni la position ni la graine** : une sauvegarde v4.5 reprend au point de départ d'un niveau neuf. Aucun risque de réapparaître au-dessus d'une fosse. |
| Réservations | Cachettes, puis sorties, puis cassettes : chaque chunk tire des cellules au hasard, sans connaître l'accessibilité. |

## Défaut découvert : Niveau 0 mal relié

En rejouant la génération sur 200 graines (sans fosses, règles v4.5), **toutes** ont des cellules inaccessibles depuis
le départ. Pour certaines, le départ est enfermé :

| `-BRSeed` | Cellules atteignables (sur 1 024) |
|---|---|
| 1 | **21** |
| 132 | 34 |
| 191 | 63 |
| 9 | 294 |

Les murs tirés au hasard, par tronçons, ferment des poches. En v4.5, des cassettes VHS et des sorties pouvaient donc être
**inatteignables**, et la partie impossible à finir. Comme la demande exige un chemin de contournement vers les
objectifs et les sorties, ce défaut est corrigé en v4.6 (voir le rapport).

## Modèles et animations v4.5, vérifiés en mouvement

Les six maillages à squelette livrés ont été importés tels quels dans Blender et posés comme le fait `AnimateLimbs` :
4 phases de marche, poursuite, frappe. Détails et planche : `Docs/v46/blender_rigs_en_mouvement.jpg` et `.json`.

- Aucune déchirure.
- Au pire, 1,07 % d'arêtes hors de [0,6 ; 1,6] fois leur longueur au repos : frappe du Partygoer, à l'épaule.
- Moins de 1 % de triangles écrasés (Faceling, très peu de polygones).
- **Aucune reconstruction n'est justifiée.**

Deux limites signalées dans le rapport v4.5 se confirment :

1. **Pieds qui flottent.** Le bassin garde sa hauteur pendant la foulée, alors que la jambe d'appui, inclinée,
   raccourcit. Mesuré sur les modèles livrés (`Docs/v46/pieds_v45_sans_appui.json`) : le point le plus bas monte de
   7,9 % de la taille du Wretch en poursuite, et d'environ 4 % pour le Skin-Stealer et le Partygoer.
2. **Hound trop cher.** Ses 175 201 sommets et 319 000 triangles viennent pour **88 % des cheveux** : 7 801 mèches de
   20 sommets. Le corps ne fait que 37 752 triangles.

## Conséquences pour la v4.6

1. Réserver les salles de fosses dans les règles de grille avant toute construction.
2. Découper la dalle du chunk autour des ouvertures.
3. Faire constater la chute par le serveur, avec le système de mort existant.
4. Rendre le vide visible à l'A* et aux raccourcis des entités.
5. Relier tout le Niveau 0 au départ.
6. Poser la jambe d'appui au sol.
7. Alléger le Hound par un dérivé, sans toucher à l'original.
