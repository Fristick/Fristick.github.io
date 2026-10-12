# Audit v4.12 → v4.13

Audit fait le 12 octobre 2026 sur la branche `backrooms`, commit `90bd60c` (v4.12, 9/9), avant toute modification. La
branche n'avait pas évolué depuis ce commit (`git fetch origin backrooms` : même tête).

## Environnement

| Élément | Constat |
|---|---|
| Machine | Conteneur Linux x86_64, 4 cœurs, **aucun GPU**. |
| Unreal Engine | **Absent** (ni éditeur, ni UHT/UBT/UAT). Compilation, cook et tests du jeu impossibles ici. |
| Blender | **Module Python `bpy` 5.0.1** (Blender 5.0.1 sans interface) : le kit du Level 0 peut être réellement construit, exporté et rendu (Cycles sur processeur). |
| Documentation | `dev.epicgames.com`, `partner.steamgames.com` et les guides NVIDIA sont refusés par la politique réseau (403). Rien n'a été contourné. |

**Bancs relancés sur `90bd60c`** (chiffres identiques au prompt) :

- contenu par version : 67 vérifications OK ;
- missions : 3000 graines × 12 niveaux, RÉSULTAT OK ;
- fosses du Level 0 : 24 graines, 0 problème.

## 1. Level 0 (§ 2 du prompt)

| Constat | Où (base) | Vérifié |
|---|---|---|
| Mission du Level 0 : trois anomalies à observer, trois cadrans, bouton de stabilisation, levier de route | `BRMissionLogic.cpp:117` (`BuildPlan`, cas 0) ; rôles `R_Anomaly` à `R_Passage` (`BRMissionLogic.h:134`) ; `HasMission` inclut le Niveau 0 (`:852`) | oui |
| Sorties gardées du Level 0 : Level 1 **ou** Poolrooms selon le levier | `IsExitGuarded` (`BRMissionLogic.cpp:1504`), `ForwardTarget` (`BRMissionSolver.cpp:61`) | oui |
| Sorties du Level 0 tirées par chunk : mur glitché → 1 et échelle → 37, 17 % chacune | `BRLevels.cpp:86` | oui |
| Ancien objectif VHS : 6 cassettes + coupure filmée, cassettes tirées dans les chunks | `BRLevels.cpp:88` ; `BRChunk.cpp:2148` ; `BRWorld.cpp:2816-2879` (`AreObjectivesComplete`, `CanLeaveLevel`, annonce « OBJECTIFS ») | oui |
| La sortie dépend de ces deux modes chez l'hôte | `CanUseExit` (`BRMissionWorld.cpp:1515-1532`) : mode historique → `CanLeaveLevel` (cassettes) ; mission → `IsExitOpen` ; message « le levier d'aiguillage ouvre l'autre route » | oui |
| Reprise : une session de format 1 à 3 garde l'ancien mode VHS jusqu'à la sortie du niveau | `SetupMission` (`BRMissionWorld.cpp:238`) : génération < `GenVersion` → mode historique ; migration `BRSave.cpp:426` | oui ; monter `GenVersion` ferait passer **toutes** les missions v4.12 sauvegardées en mode historique (même comparaison) |
| Textes hérités faux | `Note.L0.7` (échelle, `BRLevels.cpp:111`), `Note.L0.8` (six cassettes, `:112`), `Note.L0.3` (murs « faux » qu'on touche pour sortir, `:107`), description du niveau (« une échelle monte dans le plafond », `:46`), page Solo (« Objectifs : 6 cassettes… », `BRHUD.cpp:2367`) | oui |
| Objectif facultatif du Level 0 (trois cassettes) | `OptionalBitFor(0)` (`BRMissionLogic.cpp:1552`) | oui |

## 2. Progression (§ 3)

| Constat | Où (base) | Vérifié |
|---|---|---|
| Sorties du Level 1 : porte → 2 et ascenseur → 4 | `BRLevels.cpp:154` ; gardées toutes deux (`IsExitGuarded`, `:1505`) | oui |
| Sorties des Poolrooms : sol glitché → 0 et échelle → 4 | `BRLevels.cpp:602` ; gardées toutes deux (`:1515`) | oui |
| Redirections du lot 1 : `37 → 4` devient `→ 1`, `1 → 4` devient `→ 2` | `BRContentLogic.cpp:42-44` | oui |
| La même sortie mène ailleurs selon le canal | banc de contenu : `0 → 1 → 2` (publiée), `0 → 1 → 4` (test interne), `0 → 1 → 4 → 5 → … → 11` (développement) | **reproduit** (banc relancé) |
| Les Poolrooms sont facultatives | le levier du Level 0 choisit 1 **ou** 37 ; aucune route ne les impose | oui |
| Sortie de retour du Level 4 vers le Level 1 | `BRLevels.cpp:299` | oui |
| Destinations au hasard qui font progresser | porte d'immeuble du Level 11 (`BRLevels.cpp:552`, cible -1) ; « continuer l'exploration » après une fin (`BRMissionWorld.cpp:1935`) | oui |
| L'hôte ne valide que la **disponibilité** d'une destination | `ABRWorld::RequestTransition` : `BRLevels::Exists` seulement | oui |
| Le menu propose tout niveau exploré comme point de départ | `IsLevelUnlocked` + `StartSolo` (`BRPlayerController.cpp:1393`, `:2416`) ; un niveau non exploré passe en « session de développement » | oui |
| Missions refaites à chaque retour | une mission n'existe que pour la session en cours ; un retour dans un niveau tire une mission neuve | oui |
| Aucun état narratif | ni chapitre atteint, ni étapes, ni événements vus : seuls `Explored`, `CurrentLevel` et la campagne (fragments, facultatifs, fins) | oui |

## 3. Conséquences retenues

1. Le Level 0 sort du système de missions. Il n'a plus de plan : `HasMission(0)` est faux. Les identifiants de rôles
   restent inchangés, pour que les empreintes des plans des autres niveaux, donc leurs sauvegardes, restent valides. Sa
   seule sortie est une porte de service posée par le monde.
2. Un ordre unique du chapitre et de la campagne, dans une logique pure partagée par le jeu et un banc : `BRStoryLogic`.
3. Un état narratif tenu par l'hôte, répliqué et sauvegardé (format 5). Une migration ciblée remplace la comparaison de
   `GenVersion` pour le Level 0 ; les missions v4.12 des autres niveaux ne changent pas de mode.
4. L'hôte valide les transitions contre l'histoire, et plus seulement contre la disponibilité.
