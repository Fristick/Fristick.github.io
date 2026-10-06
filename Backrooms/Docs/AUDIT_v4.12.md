# Audit v4.11 → v4.12

Audit fait le 6 octobre 2026 sur la branche `backrooms`, commit `d9c47f9` (v4.11, 7/7), avant toute modification. La
branche n'avait pas évolué depuis ce commit (`git fetch origin backrooms` : même tête).

## Environnement

| Élément | Constat |
|---|---|
| Machine | Conteneur Linux x86_64, **aucun GPU**. |
| Unreal Engine | **Absent** (ni éditeur, ni UHT, ni UAT, ni Xcode). Le P0 du prompt (compiler Editor et Shipping, cuire, lancer `-BRAutoTest`, `-BRNetTest`) **ne peut pas être exécuté ici**. |
| Documentation | `dev.epicgames.com`, `partner.steamgames.com` et les guides NVIDIA sont refusés par la politique réseau (403). Rien n'a été contourné. |
| Possible ici | Python 3.11, g++ 13, clang 18 (logique pure compilée et exécutée ; syntaxe du code Unreal avec des en-têtes simplifiés), fontTools. |

**Vérifications de départ, relancées sur `d9c47f9`** : outils de livraison 59 tests, 58 réussis, 1 sauté ; banc des
missions `3000` : RESULTAT OK (mêmes chiffres que le rapport v4.11).

## 1. Bugs du § 4, vérifiés sur `d9c47f9`

Catégories : *reproduit* (exécuté ici sur une copie fidèle du code de la base, `Docs/v412/repro_base_v411.cpp`,
journal `Docs/v412/reproduction_base.txt`) ; *constat de source* (lu dans le code ; reproduction en moteur impossible ici).

| Point | Où (base) | Constat | Catégorie |
|---|---|---|---|
| 4.1 Départ par une échelle | `BRInteractables.cpp:576`, `BRMissionWorld.cpp:1210, 1287`, `BRCharacter.cpp:3560` | Sommet de la montée à `plafond + conduit / 2` : 440 cm au Niveau 0 ; rassemblement à 150 cm ; tolérance 260 cm sans supplément vers le haut. Le grimpeur n'est jamais « prêt » sur les **cinq** échelles à conduit (Niveaux 0, 2, 6, 8, 37 : écarts de 280 à 450 cm). Les sorties garanties sans conduit (6, 8, 37) passent. `UpdateClimb` rappelle `FinishClimb` à chaque image tant que le joueur est en haut : en ligne, une demande de départ par image. | **reproduit** (calcul) ; trajet réseau à reproduire dans Unreal |
| 4.2 Objet accepté puis perdu | `BRCharacter.cpp:1442, 1475` ; `BRWorld.cpp:790` ; `BRCharacter.cpp:1552, 1555` | Capacité calculée par le client, l'hôte ne vérifie que `Room != 0`, puis retire l'objet du monde et crédite ses effets. L'inventaire peut changer pendant l'attente. Scénario : une case libre, demande de bandage (capacité 4), lampe rangée dans la dernière case : `ReceivePickup` renvoie `false`, ignoré ; branche « ancien niveau » : reste d'`AddItem` ignoré. | **reproduit** (fonctions copiées) |
| 4.2 Époque de vie | `BRCharacter.cpp:1513` | Les réponses sont liées au niveau, pas à la vie : une mort puis un réveil (inventaire de départ) dans le même niveau ne changent pas le numéro de niveau. | constat de source |
| 4.3 Faux « plein » | `BRCharacter.cpp:338, 1442` | `RoomFor` ne compte que poches et sac ; `AddItem` équipe aussi un emplacement libre. Poches et sac pleins, lampe à la ceinture, main libre : `RoomFor(Lampe) = 0`, `AddItem` réussit (main). | **reproduit** |
| 4.4 Lumières de mission | `BRWorld.cpp:4254` ; `BRMissionDevice.cpp` (balises, 2200 lm, 9 m) ; `BREntity.cpp:2049` | `LightLevelAt` renvoie 0 dès que le niveau n'a pas de plafonniers (Niveau 6) : les balises allumées n'existent pas pour les Smilers (disparition au-delà de 0,5), leurs apparitions ni la santé mentale. Le calcul ignore aussi les murs (distance 2D). | constat de source |
| 4.5 Animation masquée | `BRMissionDevice.cpp:619, 739, 835` ; `BRMissionWorld.cpp:800` | `SetShown(false)` coupe rendu et collisions, pas le Tick ; le Tick d'un mécanisme animé se garde (`bKeep = bAnimated`) ; le contrôle de distance ne voit que les mécanismes affichés ; `PlayFeedback` relance le Tick même masqué. | constat de source ; coût non mesuré |
| 4.6 Soin périmé | `BRCharacter.cpp:1282-1335` | Réponse d'un ancien niveau acceptée et « plus récente » (comparée à `AckHealSerial`, propre aux soins) : la santé courante est écrasée. **100 → 35** sur la copie. Coups et soins ont deux compteurs séparés ; le réveil (santé 100) n'en a aucun. | **reproduit** |
| 4.7 Placement, restauration | `BRMissionWorld.cpp:220, 241` | Placement normal et de secours impossibles : `MissionGen = 0`, `CanUseExit` ouvre toutes les sorties. État sauvegardé qui ne correspond plus au plan : mission refaite, seulement un message dans le journal technique. Le client recalcule lui-même son placement (seul `bFallback` est répliqué). | constat de source ; aucune graine défaillante trouvée |
| 4.8 Carnet | `BRHUD.cpp:5398, 5499` | Colonne de droite vide : son message part de `Y`, la hauteur atteinte par la colonne de gauche. L'aide et le rappel s'arrêtent au bas du panneau (`break`), sans défilement. | constat de source ; rendu à vérifier |

## 2. Effets physiques des missions (§ 5)

| Point | Où (base) | Constat |
|---|---|---|
| Passerelle du Niveau 8 | `BRMissionDevice.cpp:70, 550` | Pièces en `NoCollision` (`AddPart`) ; le bloqueur vertical est coupé une fois la passerelle « baissée ». C'est un panneau qui pivote devant la sortie : aucune rupture de terrain, aucune surface portante. |
| Bassins des Poolrooms | `BRMissionLogic.cpp:1072` (`PoolLevels`) ; `BRCharacter.cpp:2460, 3684` | Niveaux A et B purement logiques ; nage, profondeur et immersion utilisent `Def().WaterHeight` (60 cm) partout ; aucune liaison avec l'état de mission. |
| Vapeur du Niveau 2 | `BRMissionDevice.cpp` (`R_SteamDoor`) | Quatre sphères opaques émissives. |
| Autres niveaux | `BRMissionDevice.cpp` | Mécanismes faits de primitives partagées (boîtes, cylindres) ; les liaisons (câbles, tuyaux) entre commandes et effets ne sont pas représentées. |

## 3. Publication progressive (§ 10)

| Point | Où (base) | Constat |
|---|---|---|
| Disponibilité | `BRLevels::All()` | Une seule liste : tout niveau défini est jouable. Aucune notion de niveau développé, testé ou publié. |
| Entrées | `BRWorld.cpp:144, 266-279` ; `BRGameMode.cpp:28` ; `BRHUD.cpp` (cartes de niveaux) ; `BRPlayerController.cpp` (niveaux explorés, mode développeur) | Ligne de commande, option `BRLevel`, destinations au hasard (tous les niveaux sauf le courant), menus et transitions acceptent n'importe quel niveau existant. |
| Compatibilité réseau | `Config/DefaultGame.ini` (`ProjectVersion=0.1.0.0`) | Version de projet jamais changée : le moteur ne distingue pas un client v4.11 d'un client v4.12 ; aucun contrôle du contenu à la connexion. |

## 4. Ordre de travail retenu

1. Transactions (capacité unique, réservation, récupération, époque de vie, révision de santé commune) et départ par
   échelle.
2. Cohérence : lumière de gameplay, Tick des mécanismes masqués, placement garanti et restauration expliquée.
3. Disponibilité des niveaux par version et premier lot (0, 1, 2, 37), fin du contenu disponible.
4. Passerelle du Niveau 8, bassins des Poolrooms, mécanismes du premier lot, carnet, retours d'interaction.
5. Tests (bancs exécutés ici ; tests du jeu préparés), rapport, document de gameplay, plan de publication.
