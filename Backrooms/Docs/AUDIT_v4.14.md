# Audit v4.14 : constats vérifiés sur la base

| | |
|---|---|
| **Base** | `b8f9449` (v4.13 suite 6), tête de `backrooms` au 14 octobre 2026 ; la branche n'a pas avancé depuis l'audit du prompt |
| **Méthode** | lecture des chemins de code cités ; sondes C++ liées aux fichiers du jeu (`BRLidarLogic.cpp`, `BRStoryLogic.cpp`) ; historique git des sorties |
| **Journal** | [`v414/audit_sondes.txt`](v414/audit_sondes.txt) (sortie réelle des sondes) |
| **Limite** | pas d'Unreal ni de GPU dans le conteneur : les effets réseau, visuels et de collision restent à constater dans le moteur |

## 2.1 REJOUER et mode développeur écrivent dans la partie de campagne — **confirmé**

`ABRPlayerController::WriteActiveSave` (`BRPlayerController.cpp`) :

- `C->WriteToSave(ActiveSave)` (équipement, santé, statistiques) puis journal, `RouteBits`, `OptionalFound`, `Endings` sont
  recopiés **avant** le test du mode ; seuls l'histoire et la session sont protégés en REJOUER ;
- personnage mort : `bPendingDeath = true`, `Session.bValid = false` sans test du mode ; au chargement suivant,
  `ResolvePendingDeath` vide les objets et la session de la partie ;
- `NotifyPlayerDeath` incrémente `Deaths` et écrit, quel que soit le mode ;
- `PlayTime` augmente en REJOUER ; `OnEndingShown`, la pause, le retour au menu et la fermeture écrivent tous par ce
  même chemin ;
- mode développeur sur un niveau **déjà découvert** : `(!bDevSession || IsExplored(...))` laisse passer l'écriture du
  niveau courant et l'équipement est déjà écrit plus haut ; `OnLevelLoaded` déplace aussi `CurrentLevel` en mémoire.

Mort en REJOUER, en solo : `ABRWorld` renvoie au Level 0 (`RequestTransition(0, true)`), règle de la campagne.

## 2.2 La migration crédite des Poolrooms non traversées — **confirmé** (sonde)

`BRStory::MigrateLegacy` lit l'adjacence `37 → 4` dans `Explored`, liste remplie par `AddUnique` (premières
découvertes). Sonde sur le code du jeu, découvertes `[0, 1, 37, 4]`, partie au Niveau 4 :

```
Poolrooms creditees OUI, etape 37 accomplie OUI, partie garee NON, detour NON
```

Historique des sorties (`git show <commit>:BRLevels.cpp`, de `b01814c` à `90bd60c`) : le Niveau 4 a toujours été
atteignable depuis **1** (ascenseur), **3** (ascenseur), **5** (retour) et **37** (échelle), plus les destinations au
hasard (porte d'immeuble du Niveau 11, exploration après une fin) ; la console ajoutait les niveaux visités aux
découvertes jusqu'à la v4.3 (format 1). L'ordre des découvertes ne prouve donc une traversée que si 37 est le seul
prédécesseur possible du Niveau 4 déjà découvert.

Autres points du même chemin :

- `TakeParkedSession` vide la session garée **avant** que sa reprise ait réussi, et la vide aussi quand le niveau ne
  correspond pas ;
- `OnProgress` efface `Parked` à la sortie des Poolrooms même quand le niveau garé n'est pas disponible (canal public) :
  la progression garée est alors perdue.

## 2.3 Réserve du scanner : refus à haut débit d'images, expiration tronquée — **confirmé** (sonde)

`BRLidar::FRing` : 3 000 points par groupe d'âge (24 000 / 8), plein = refus jusqu'au groupe suivant ; 37 rayons de focus
par image. Sonde sur la logique du jeu, 30 s de focus continu, un point par rayon :

| Images/s | Rejetés | Images sans point ajouté |
|---:|---:|---:|
| 30 | 0 % | 0 |
| 60 | 10,4 % | 170 |
| 144 | 62,5 % | 2 680 |
| 240 | 77,5 % | 5 559 |

`Advance(13 s)` avec 12 s de persistance : un seul groupe vidé, le reliquat perdu ; les 100 points restent comptés,
âge 1,5 s, atténuation 1.

## 2.4 Inscriptions : budget annoncé ≠ code — **confirmé** (calcul des branches)

`ABRLidar::TraceRay` appelle `AddPlateGlyphs(D, 4)` pour **chaque** rayon focalisé qui touche une porte : 4 points de
silhouette + le symbole 7 × 7 entier (12 à 24 cases), et `ScannedPlates.Add` à chaque rayon. Avec 37 rayons : 629 à
1 073 points pour le focus seul, jusqu'à 1 293 avec 220 impacts d'impulsion. Le banc `test_lidar.cpp` réservait
`257 + 120 = 377` points sans passer par ce chemin. Les cages (`AddCageGlyphs`, 6 points par rayon) suivent le même
schéma. `UpdateFades` réécrit les 48 matériaux dynamiques à chaque image.

## 2.5 Le scanner survit à un rechargement du Level 6 — **confirmé** (cycle de vie)

`ABRCharacter::UpdateLidar` ne détruit le scanner que hors du Level 6 ; `ABRWorld::ClearLevel` et `LoadLevelNow` ne le
touchent pas. Un rechargement `6 → 6` (autre graine, reprise) garde points, `ScannedPlates`, `MarkShownAt` et le focus.

## 2.6 Porte du Level 0 chez le client — **confirmé** (code ; effet à constater)

`ABRWorld::OnRep_Story` constate un écart entre la porte du client et celle de l'hôte et n'écrit qu'un avertissement.
`PlaceServiceDoor` reprend bien le candidat de l'hôte si l'état est arrivé avant la construction ; sinon le client garde
son propre choix (mur percé, sas, chemin réservé, battant).

## 2.7 Actions maintenues — **confirmé** (code)

- `IA_LidarFocus` : seuls `Started` et `Completed` sont liés ; ni `Canceled`, ni l'ouverture de l'inventaire
  (`SetInventoryOpen` purge les touches et le sprint), ni la pause, ni la perte de focus de fenêtre, ni une transition
  n'arrêtent le focus ; `ABRLidar::Tick` ne vérifie pas le verrouillage des entrées.
- `EntityHold` s'additionne quel que soit le Smiler visé (la cible n'est pas comparée), décroît lentement et repart à
  −3 s après un recul.
- Recul : `ABREntity` recalcule une destination 7 m « plus loin » depuis sa position **à chaque décision** pendant 6 s.

## Ce qui n'a pas pu être vérifié ici

Compilation Editor/Shipping, UHT, cook, réseau réel, collisions, rendu (Lumen logiciel, RT matériel), mesures sur la
RTX 4080 + Ryzen 7 7800X3D citée : **aucune** dans ce conteneur (pas d'Unreal, pas de GPU). Les documentations
`dev.epicgames.com`, `partner.steamgames.com` et NVIDIA restent refusées (403).
