# Plan de publication progressive (v4.12)

Les douze niveaux restent dans le projet. La version publique n'en ouvre qu'un premier lot ; les suivants arrivent par
mises à jour de contenu, chacune après ses propres validations. **Aucune date n'est fixée** : l'ordre des lots est prévu,
la cadence reste indicative et dépend des validations ci-dessous.

Ce plan décrit ce qui est **codé** dans la v4.12 (configuration, règles d'accès, réseau, sauvegardes, scripts). Ce qui
n'a pas pu être lancé ici (Unreal, paquets, Steam) est marqué « préparé, non exécuté ».

## 1. Les lots

| Lot | Nom | Niveaux | Étape v4.12 | Entités présentes | Objectif de gameplay |
|---|---|---|---|---|---|
| 1 — lancement | Le Seuil | 0 Threshold, 1 Habitable Zone, 2 Utility Halls, 37 Poolrooms | **publié** | Smiler, Bacteria, Faceling, Hound, Wretch, Clump (Poolrooms sans entités) | Apprendre les règles communes : observer, noter, actionner, se rassembler. Deux routes depuis le Niveau 0 (mur glitché → Niveau 1, échelle → Poolrooms) qui se rejoignent au Niveau 1, puis le Niveau 2 et la fin du lot. |
| 2 | La Station et les Bureaux | 3 Electrical Station, 4 Abandoned Office | test interne | Hound, Skin-Stealer, Smiler, Faceling, Partygoer | Missions à états multiples (secteurs en défaut, relais, code d'accès) ; premier lieu calme et lisible (bureaux) après la station. |
| 3 | L'Hôtel et le Noir | 5 Terror Hotel, 6 Lights Out | développé | Skin-Stealer, Partygoer, Faceling, Smiler | Pression et lumière : serrures, consigne de pression, balises qui dissipent les Smilers (règle de lumière de la v4.12). |
| 4 | Les Grottes, la Banlieue et le Champ | 8 Cave System, 9 The Suburbs, 10 Field of Wheat | développé | Deathmoth, Clump, Hound, Skin-Stealer, Faceling | Traversées physiques (passerelle portante du Niveau 8, treuils), enquête en extérieur, orientation du moulin. |
| 5 | La Ville | 11 The Endless City (et le dernier quai) | développé | Faceling | Fin de la campagne : station à manivelle, données de route, dernier quai (deux fins). |
| Extensions | Level ! (1001), Level Fun (1002) | — | proposées (`Docs/PROPOSITIONS_NIVEAUX_v4.11.md`) | — | Routes facultatives depuis les Niveaux 6 et 8, sans changer la progression principale. Identifiants réservés, absents de la table tant qu'ils ne sont pas développés. |

Les neuf entités restent dans le projet. Au lot 1, six apparaissent ; Skin-Stealer, Partygoer et Deathmoth arrivent avec
les lots 2 à 4.

### Fin du premier lot

- Niveau 0 → (levier) Niveau 1 ou Poolrooms.
- Poolrooms : l'échelle de sortie (Niveau 4 dans la campagne complète) **remonte au Niveau 1** tant que le Niveau 4 n'est
  pas publié. Le retour par le sol glitché vers le Niveau 0 reste ouvert.
- Niveau 1 : l'ascenseur que la mission alimente et appelle **descend au Niveau 2** tant que le Niveau 4 n'est pas
  publié ; la porte vers le Niveau 2 s'ouvre aussi. La mission ne change pas et garde son sens.
- Niveau 2 : la porte vers le Niveau 3 (sortie de progression, ouverte par la mission des vannes) devient **la fin du
  contenu disponible** : le départ de groupe se fait normalement, puis un écran « Fin du contenu disponible » annonce la
  fin du lot 1 (« Le Seuil »), enregistre cette fin dans la partie, et propose de continuer à explorer (niveau disponible
  au hasard) ou de revenir au menu.
- Aucun passage du lot 1 n'est condamné (vérifié par le banc, § 6). La règle des passages condamnés existe pour les lots
  suivants : planches en croix, lumière rouge, invite « pas encore accessible », aucun départ possible ; la mission du
  niveau garde alors une autre sortie.

Les deux routes, vérifiées par le banc : `0 → 1 → 2 → fin du contenu` et `0 → 37 → 1 → 2 → fin du contenu`.

## 2. Configuration centrale

Une seule table, dans `Source/Backrooms/Private/BRContentLogic.cpp` (C++ pur, aussi compilée par le banc) :

```cpp
{ 0, EStage::Published, 1 },  { 1, EStage::Published, 1 }, { 2, EStage::Published, 1 }, { 37, EStage::Published, 1 },
{ 3, EStage::Internal, 2 },   { 4, EStage::Internal, 2 },
{ 5, EStage::Developed, 3 },  { 6, EStage::Developed, 3 },
{ 8, EStage::Developed, 4 },  { 9, EStage::Developed, 4 }, { 10, EStage::Developed, 4 },
{ 11, EStage::Developed, 5 },
```

et les redirections du lot en cours (`{ 37, 4 → 1 }`, `{ 1, 4 → 2 }`).

| Étape | Signification | Version publique | Version de test interne | Développement |
|---|---|---|---|---|
| `Published` | publié, annoncé | ouvert | ouvert | ouvert |
| `Internal` | complet, en essais internes | fermé | ouvert | ouvert |
| `Developed` | développé, en attente de son lot | fermé | fermé | ouvert |

**Canal d'une version.**

- Version publiée (Shipping) : canal **fixé à la compilation** (`BR_CONTENT_CHANNEL`, lu par `Backrooms.Build.cs`). Ni la
  ligne de commande ni un fichier de configuration ne l'ouvrent.
- Développement et éditeur : `-BRContent=public|internal|all`, tout ouvert par défaut (le contenu réservé se teste sans
  rien changer).

**Constructions** (préparé, non exécuté).

- `Tools/Build/build_windows.ps1 -Content internal` ;
- `BR_CONTENT_CHANNEL=internal Tools/Build/build_linux.sh` ;
- `BR_CONTENT_CHANNEL=internal Tools/Build/build_mac.sh`.

Par défaut le canal est `public`. Une version interne va dans `Build/<Système>/<Config>-internal/`, et son manifeste
porte `contenu : internal`. Un changement de canal depuis la construction précédente force une compilation propre :
UBT ne suit pas les variables d'environnement.

**Ce qui passe par ces règles** : la liste du menu et le choix des niveaux d'une partie, les cartes de sauvegarde, les
invites et l'usage des sorties (portes, échelles, ascenseurs, noclip), les sorties par noclip générées dans les chunks, la
sortie gardée par chaque mission, les destinations au hasard, les départs de groupe, et toute transition demandée à
l'hôte. L'hôte refuse un niveau indisponible : demande d'un client, console de test, sortie mal résolue (journal
`Transition refusee`).

## 3. Sauvegardes et progression

- Les identifiants de niveaux sont stables (numéros du wiki ; 1001 et 1002 réservés). Une partie garde ses niveaux
  explorés, découvertes, objets, fins et sa route, quelle que soit la version qui l'ouvre.
- Le format de sauvegarde ne change pas pour la disponibilité : rien n'est retiré d'une partie.
- Une partie dont le dernier niveau n'est pas disponible dans cette version (partie v4.11 ou d'une version de test) :
  - le fichier est d'abord copié tel quel (`<emplacement>_AvantRecuperation`) ;
  - la partie reprend au dernier niveau disponible exploré (ordre de découverte), sinon au Niveau 0 ;
  - un avis l'explique ; la carte de sauvegarde l'annonce déjà dans le menu.

  Quand le niveau est publié, il redevient sélectionnable : il est toujours dans la liste des niveaux explorés.
- Une fin du contenu disponible s'enregistre dans les fins de la partie (bit propre à chaque lot, distinct des deux fins
  de la campagne) : elle n'efface rien et n'impose pas de recommencer quand le lot suivant arrive.
- Une mise à jour de contenu qui change la progression d'un niveau existant doit ajouter une migration explicite (comme
  `PlaceVersion` pour le placement des missions) : ancien état → nouvel état, avec copie avant récupération.

## 4. Réseau

- Le client envoie `?BRNet=412?BRContent=<signature>` en rejoignant.
- L'hôte (PreLogin) refuse un joueur d'une autre version ou d'un autre contenu, avec une raison lue dans la langue du
  joueur :
  - « l'hôte a la version 4.12 du jeu, vous avez la version … » ;
  - « l'hôte joue avec la version publiée (contenu jusqu'au lot 1), vous avez la version de test interne (lot 2) ».
- `ProjectVersion=4.12.0.0` (`Config/DefaultGame.ini`) : le moteur refuse déjà, avant la demande d'entrée, une version
  du projet différente (message « l'hôte a une autre version du jeu »).
- Partie complète (4 joueurs) : message dédié.

La signature dépend du protocole, du lot courant et des niveaux disponibles. Deux versions publiées du même lot jouent
ensemble ; une version publiée et une version de test, non. Vérifié hors moteur (§ 6) ; l'essai réel en réseau est
préparé, non exécuté (`-BRNetTest`, étape « client incompatible » du rapport).

## 5. Ajouter un lot (modules)

Un niveau est un ensemble de modules indépendants ; en ajouter un ne touche pas aux systèmes communs.

| Module | Où |
|---|---|
| Définition (titre, décor, entités, sorties) | `BRLevels.cpp` (une fonction par niveau) |
| Mission (plan, règles, solveur) | `BRMissionLogic.cpp`, `BRMissionSolver.cpp` |
| Textes de mission et carnet | `BRMissionText.cpp` (+ traductions, `Tools/Localization`) |
| Ressources indispensables | `Tools/Build/essential_packages.txt`, préchargement par ensembles (`BRAssets`) |
| Disponibilité | une ligne de la table (`BRContentLogic.cpp`) et, si besoin, les redirections du lot |

Les ressources des niveaux réservés restent dans le paquet du lot 1 : rien n'est exclu, donc aucune dépendance ne peut
manquer. Leur accès est fermé par le canal compilé. Les originaux et le manifeste des modèles protégés ne changent pas.
Exclure plus tard des ressources par lot (chunks de pak) demandera de vérifier les dépendances et les chargements de
chaque niveau publié.

### Passer un niveau d'une étape à la suivante

**Developed → Internal** :

- mission résolue par le solveur sur 3000 graines (`Tools/Missions`) ;
- placement réel essayé (`-BRAutoTestV411`/`V412`, préparé) ;
- textes traduits dans les 22 langues (`loc_build.py --check`) ;
- ressources essentielles présentes (`check_package.py`) ;
- banc de contenu OK.

**Internal → Published** :

- la version de test interne est jouée de bout en bout, en solo et en coop à 2-4 (départs de groupe, transactions,
  sauvegarde et reprise) ;
- performances mesurées sur la machine de référence ;
- aucune impasse ;
- reprise d'une partie du lot précédent ;
- refus réseau entre versions vérifié ;
- notes de version rédigées ;
- banc de contenu OK, avec les nouvelles routes et la nouvelle fin du lot.

**Mise à jour de contenu** : au moins un niveau jouable complet (ou l'ensemble annoncé), avec sa mission, sa place dans
la progression, ses validations solo et coop. Les correctifs peuvent sortir indépendamment. Un niveau incomplet reste
`Internal` plutôt que d'être publié pour tenir une cadence.

## 6. Vérifications

**Exécuté ici.** `Tools/Content/test_content.cpp`, 67 vérifications OK (`Docs/v412/banc_contenu.txt`). Il lit les sorties
réelles des niveaux dans `BRLevels.cpp` et la progression de la base dans `BRMissionWorld.cpp`, puis vérifie :

- dans chaque canal :
  - aucune sortie ne mène à un niveau indisponible ;
  - tous les niveaux disponibles sont atteignables ;
  - chaque mission garde une sortie qui mène quelque part ;
  - les deux routes du Niveau 0 se terminent : fin du contenu au Niveau 2 (publiée), au Niveau 4 (test interne), dernier
    quai en développement ;
- les redirections, les tirages au hasard et la reprise des sauvegardes ;
- les refus réseau.

Les tests des outils de paquet (`Tools/Build/tests`) vérifient en plus qu'une version interne n'est jamais préparée
comme version publique pour Steam.

**Préparé, non exécuté.** Constructions Shipping par canal, `-BRAutoTestV412` (étape « contenu » : menu, sorties
condamnées ou redirigées, écran de fin du contenu, reprise d'une partie v4.11), `-BRNetTest` avec un client incompatible.

## 7. Steam (préparé, non exécuté)

- `Tools/Steam/make_steam_vdf.py --content public` (par défaut) ne prend que `Build/<Système>/<Config>/`, dont le
  manifeste doit dire `contenu : public`. `--content internal` prend `<Config>-internal/` pour une branche de test privée.
- Un manifeste sans canal (scripts antérieurs) ou d'un autre canal est refusé.
- `SetLive` reste vide et la prévisualisation reste active : aucune publication automatique. Le choix de la branche
  (publique ou de test) se fait à la main dans Steamworks.

  Les pages de partner.steamgames.com n'étaient pas accessibles depuis cet environnement (403) : la procédure exacte des
  branches n'a pas été vérifiée ici.

## 8. Notes de version du lancement (brouillon)

> **The Backrooms — Le Seuil (lot 1)**
>
> - Quatre niveaux : Threshold (Niveau 0), Habitable Zone (Niveau 1), Utility Halls (Niveau 2) et les Poolrooms
>   (Niveau 37), chacun avec sa mission.
> - Deux routes depuis le Niveau 0, qui se rejoignent avant la fin du lot.
> - Solo ou coopération jusqu'à 4 joueurs par IP directe, chat vocal de proximité facultatif.
> - 22 langues ; Windows, Linux et macOS selon les paquets validés.
> - À la fin du Niveau 2 : fin du contenu disponible. Vos parties seront conservées lors des prochaines mises à jour.
>
> *Prévu (sans date)* : la Station et les Bureaux (Niveaux 3 et 4), puis l'Hôtel et le Noir, les Grottes, la Banlieue et
> le Champ, et la Ville.

La communication sépare toujours ce qui est disponible (liste ci-dessus) de ce qui est prévu (en italique, sans date).
