# Audit v4.6 → v4.7

Audit fait le 5 octobre 2026 sur la branche `backrooms`, commit `be26444` (v4.6). La branche n'a pas évolué depuis.

## Environnement : Unreal toujours indisponible

| Élément | Constat |
|---|---|
| Machine | conteneur Linux, 4 cœurs, 15 Go de RAM, **aucun GPU** (`nvidia-smi` absent) |
| Unreal Engine | **absent**. L'installer demande un compte Epic, plus de 100 Go, et un GPU pour compiler les shaders, lancer le PIE et capturer. |
| Conséquence | aucune compilation, aucun import, aucun PIE, Standalone, empaquetage, capture ni mesure de FPS n'est possible ici. Les validations restantes et leurs commandes sont listées dans le rapport. |

## Défauts trouvés dans les sources

### Mort et coopération (`BRCharacter.cpp`, `BRPlayerController.cpp`)

1. **Cause de mort devinée à partir de la hauteur.**
   - `OnRep_Dead` annonce « X est tombé dans une fosse » dès que `Z < -100`.
   - `ServerRevive` et `FindDownedTeammate` refusent toute réanimation sous cette hauteur.
   - Or les bassins des Poolrooms font 2,60 m : **un joueur blessé ou noyé dans un bassin est annoncé tombé dans une
     fosse, et ne peut pas être relevé.**
2. **État de mort tenu par le client.**
   - `bDead` est répliqué sauf au propriétaire (`COND_SkipOwner`). Le client le fixe lui-même par `ServerSetDead`, que
     le serveur accepte sans contrôle.
   - La chute constatée par le serveur passe par une RPC qui demande au client de mourir. Si elle est perdue ou
     ignorée, le serveur et les autres clients ne voient jamais la mort.
3. **Réanimation déclenchée par le client** (`ClientRevived`), réveil annoncé par le client (`ServerSetDead(false)`).
   Aucun état commun ne dit si un joueur peut être relevé.
4. **Commandes de test ouvertes à tous.**
   - `ServerCheat` (coupure, objectifs, apparition d'entité) s'exécute pour n'importe quel client.
   - `ServerRequestTransition` change de niveau pour n'importe quelle cible.
   - Le mode développeur s'active aussi dans une version Shipping (réglage `DevMode` du fichier ini).

### Sauvegarde (`BRSave.*`, `BRPlayerController.cpp`)

1. Ni graine, ni position, ni objectifs, ni objets ramassés : « Reprendre » recrée une disposition neuve.
2. Le champ `Version` n'est jamais utilisé, et il n'y a pas de copie de secours.
3. **Fichier illisible** : la partie disparaît du menu, mais l'emplacement reste occupé, sans explication.
4. Écriture synchrone sur le thread du jeu à chaque autosauvegarde, toutes les minutes et à chaque pause : une saccade
   possible.
5. **Mort contournable.** Fermer le jeu pendant qu'on est à terre garde l'inventaire d'avant la mort : `WriteToSave`
   est sauté, et rien ne marque la session comme perdue.

### Attaques et mouvement (`BREntity.cpp`)

1. **Dégâts avant le geste.** `TryAttack` appelle `MulticastStrike()` puis `ReceiveAttack()` dans la même image. La
   frappe visible atteint son maximum 0,12 s plus tard, et rien ne permet de l'esquiver.
2. Mouvement identique pour toutes les créatures, à la vitesse près : mêmes accélérations, mêmes virages, aucun arrêt.
3. Appui au sol v4.6 : le corps est décalé sur le pied le plus bas, en supposant un sol plat. Sur un trottoir ou une
   marche (villes, Poolrooms), le pied du dessus s'enfonce.

### Performances (`BRWorld::UpdateStreaming`, `ABRChunk::Build`)

1. Le budget de 5 ms n'est vérifié qu'entre deux chunks. Un chunk est construit d'un bloc : géométrie, puis tous les
   composants d'instances, toutes les lumières et tous les objets. Un chunk riche peut dépasser le budget à lui seul.
2. Salles de fosses : de 5 à 9 néons à ombres par salle (rapport v4.6), à mesurer.

### Rendu (`BRMaterialBuilder.cpp`, `backrooms_setup.py`)

1. La rugosité est un **scalaire par surface**, modulé par un bruit de saleté commun à toutes les matières. Les joints
   des carreaux, le béton poli par les pneus et la peinture des places de parking répondent à la lumière comme leur
   support.
2. Fosses : avec le brouillard volumétrique désactivé, le brouillard ordinaire du Niveau 0 (densité 0,08, couleur
   jaune) ajoute environ 10 % de voile au fond d'un puits de 14 m (estimation d'après les paramètres du brouillard,
   pas une mesure).

### Peur et confort

1. Apparitions à intervalle fixe (`SpawnInterval`), sans répit après une poursuite.
2. Sons des entités sans occlusion (seul le chat vocal en a), aucune réverbération propre aux lieux.
3. Réglages : balancement de la tête seulement. Pas d'option pour les flashs ni pour le flou de mouvement.
4. Manette : déplacements, regard et actions principales sont branchés. L'interface se pilote aussi au clavier.

## Ordre de travail

1. Mort et coopération.
2. Attaques.
3. Sauvegarde.
4. Streaming.
5. Rythme et sons.
6. Matériaux.
7. Tests et rapport.

Chaque point est vérifié avec les moyens disponibles ici : syntaxe C++ avec en-têtes Unreal simplifiés, simulations
Python et rendus Blender. Ce qui reste à valider dans Unreal est noté à chaque fois.
