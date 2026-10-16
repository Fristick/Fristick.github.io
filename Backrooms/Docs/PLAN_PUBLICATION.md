# Plan de publication progressive (v4.13)

Les douze niveaux restent dans le projet. La version publique n'en ouvre qu'un premier lot ; les suivants arrivent par
mises à jour de contenu, chacune après ses propres validations. **Aucune date n'est fixée** : l'ordre des lots est prévu,
la cadence reste indicative et dépend des validations ci-dessous.

Ce plan décrit ce qui est **codé** dans la v4.13 (configuration, règles d'accès, histoire, réseau, sauvegardes, scripts).
Ce qui n'a pas pu être lancé ici (Unreal, paquets, Steam) est marqué « préparé, non exécuté ».

**v4.13.** Les lots suivent désormais un fil narratif unique, le même dans tous les canaux
([`HISTOIRE_PROGRESSION_v4.13.md`](HISTOIRE_PROGRESSION_v4.13.md)) : `0 → 1 → 37 → 2 → 3 → 4 → 5 → 6 → 8 → 9 → 10 →
11 → dernier quai`. Un lot publie la suite du fil ; il ne change jamais l'ordre. Les redirections de la v4.12 sont
retirées.

## 1. Les lots

| Lot | Nom | Niveaux | Étape v4.13 | Entités présentes | Objectif de gameplay |
|---|---|---|---|---|---|
| 1 — lancement | Le Seuil | 0 Threshold, 1 Habitable Zone, 37 Poolrooms, 2 Utility Halls | **publié** | Smiler, Bacteria, Faceling, Hound, Wretch, Clump (Poolrooms sans entités) | Apprendre les règles communes : se réveiller et s'orienter (Level 0 sans énigme, porte de service), observer, actionner, se rassembler. Un seul fil : `0 → 1 → 37 → 2`, Poolrooms obligatoires, puis la fin du chapitre. |
| 2 | La Station et les Bureaux | 3 Electrical Station, 4 Abandoned Office | test interne | Hound, Skin-Stealer, Smiler, Faceling, Partygoer | Missions à états multiples (secteurs en défaut, relais, code d'accès) ; premier lieu calme et lisible (bureaux) après la station. |
| 3 | L'Hôtel et le Noir | 5 Terror Hotel, 6 Lights Out | développé | Skin-Stealer, Partygoer, Faceling, Smiler | Pression, puis le noir : serrures et consigne de pression (5) ; Level 6 **sans lumière** au scanner LiDAR (v4.13 suite : impulsion et scan focalisé, mire, postes, cages, portes du passage, sol percé, parasites ; Smilers attirés par l'impulsion, repoussés par le faisceau maintenu). |
| 4 | Les Grottes, la Banlieue et le Champ | 8 Cave System, 9 The Suburbs, 10 Field of Wheat | développé | Deathmoth, Clump, Hound, Skin-Stealer, Faceling | Traversées physiques (passerelle portante du Niveau 8, treuils), enquête en extérieur, orientation du moulin. |
| 5 | La Ville | 11 The Endless City (et le dernier quai) | développé | Faceling | Fin de la campagne : station à manivelle, données de route, dernier quai (deux fins). |
| Extensions | Level ! (1001), Level Fun (1002) | — | proposées (`Docs/PROPOSITIONS_NIVEAUX_v4.11.md`) | — | Routes facultatives depuis les Niveaux 6 et 8, sans changer la progression principale. Identifiants réservés, absents de la table tant qu'ils ne sont pas développés. |

Les neuf entités restent dans le projet. Au lot 1, six apparaissent ; Skin-Stealer, Partygoer et Deathmoth arrivent avec
les lots 2 à 4.

### Fin du premier lot

- Level 0 → **porte de service** → Niveau 1 (aucune condition, aucune énigme).
- Niveau 1 → l'ascenseur que la mission remet sous tension → **Poolrooms**.
- Poolrooms → l'échelle du passage sec → **Niveau 2**. Étape obligatoire : rien au-delà ne s'ouvre avant sa traversée.
- Niveau 2 → la porte de la station (sortie de progression, ouverte par la mission des pressions) mène à **la fin du
  chapitre 1** : départ de groupe normal, porte marquée par L., écran « Fin du chapitre 1 : Le Seuil ». La fin est
  enregistrée dans la partie (bit du lot) ; l'histoire reprendra au Niveau 3 quand il sera publié (CONTINUER reste au
  Niveau 2 d'ici là). « Continuer l'exploration » tire un niveau déjà atteint.
- Les sorties de retour (Niveau 2 → Poolrooms par l'échelle, Poolrooms → Niveau 1 par le sol qui glitche) restent ouvertes : un niveau atteint se
  revisite, sa mission accomplie reste accomplie. **Parcours classique seulement** (v4.15) : dans une partie au
  parcours linéaire (toute nouvelle partie), chaque niveau n'a qu'une sortie, celle de sa mission, sans retour
  ([`HISTOIRE_PROGRESSION_v4.15.md`](HISTOIRE_PROGRESSION_v4.15.md), R-L3).
- Aucun passage du lot 1 n'est condamné (vérifié par le banc, § 6). La règle des passages condamnés existe pour les lots
  suivants : planches en croix, lumière rouge, invite « pas encore accessible », aucun départ possible.

Le fil, vérifié par le banc : `0 → 1 → 37 → 2 → fin du contenu`.

## 2. Configuration centrale

Une seule table, dans `Source/Backrooms/Private/BRContentLogic.cpp` (C++ pur, aussi compilée par le banc) :

```cpp
{ 0, EStage::Published, 1 },  { 1, EStage::Published, 1 }, { 2, EStage::Published, 1 }, { 37, EStage::Published, 1 },
{ 3, EStage::Internal, 2 },   { 4, EStage::Internal, 2 },
{ 5, EStage::Developed, 3 },  { 6, EStage::Developed, 3 },
{ 8, EStage::Developed, 4 },  { 9, EStage::Developed, 4 }, { 10, EStage::Developed, 4 },
{ 11, EStage::Developed, 5 },
```

Plus de redirections (v4.13) : la sortie de progression d'un niveau mène au niveau suivant du fil ; si celui-ci n'est pas
disponible dans le canal, elle mène à la fin du contenu disponible.

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
- **v4.13, format 5** : la partie porte son état narratif (atteint, étapes accomplies, événements, fins de chapitre,
  reprise, niveau garé). Les parties des formats 1 à 4 sont copiées (`<emplacement>_Format<N>`) puis migrées
  (`BRSaves::MigrateStory`) : étapes dépassées créditées, traversée des Poolrooms créditée seulement sur preuve, sinon
  organisée (la session au-delà est garée intacte et reprend à la sortie des Poolrooms), Level 0 sans condition. La
  génération des missions des autres niveaux n'est pas modifiée.
- **v4.13 suite (même format 5, champs ajoutés)** : la partie garde la graine de son Level 0 (`StoryIntroSeed`) et la
  porte de service retenue (cellule, face, palier, version de placement) ; un fichier qui ne les a pas encore les reçoit
  à la prochaine visite du Level 0 (ou de la session du Level 0 en cours, à la migration). Les sessions du Level 6 de
  la v4.12 (chaîne de balises) sont converties vers la mission LiDAR quand elles ont un équivalent (plaque lue →
  calibrage, balises → postes, terminée → terminée), après copie de la sauvegarde ; `GenVersion` n'est pas monté.
- **Arrivée d'un lot** : rien à migrer. Le point de reprise d'une partie arrêtée à une fin de chapitre est déjà le niveau
  suivant du fil ; dès qu'il est disponible, CONTINUER y mène.

## 4. Réseau

- Le client envoie `?BRNet=413?BRContent=<signature>` en rejoignant.
- L'hôte (PreLogin) refuse un joueur d'une autre version ou d'un autre contenu, avec une raison lue dans la langue du
  joueur :
  - « Impossible de rejoindre : l'hôte a la version 4.13 du jeu, vous avez la version … » ;
  - « Impossible de rejoindre : l'hôte joue avec la version publiée (contenu jusqu'au chapitre 1), vous avez la version
    de test interne (chapitre 2). Les deux joueurs doivent avoir le même contenu. »

  Le joueur lit « chapitre » : un lot de contenu est présenté comme un chapitre (« Le Seuil » pour le lot 1).
- `ProjectVersion=4.13.0.0` (`Config/DefaultGame.ini`) : le moteur refuse déjà, avant la demande d'entrée, une version
  du projet différente (message « l'hôte a une autre version du jeu »). Le protocole passe à 413 : l'état narratif
  répliqué et la porte de service n'existent pas chez un client v4.12.
- Partie complète (4 joueurs) : message dédié.

La signature dépend du protocole, du lot courant et des niveaux disponibles. Deux versions publiées du même lot jouent
ensemble ; une version publiée et une version de test, non. Vérifié hors moteur (§ 6). Dans le jeu, `-BRAutoTestV412`
vérifie la règle de l'hôte et la lecture du refus (étape « contenu : version publiée », préparé, non exécuté) et
`-BRNetTest` compare protocole et signature entre l'hôte et le client (étape « compatibilité »). **L'essai d'un vrai
client incompatible (deux paquets de canaux différents) n'est pas automatisé** : il reste à faire à la main.

## 5. Ajouter un lot (modules)

Un niveau est un ensemble de modules indépendants ; en ajouter un ne touche pas aux systèmes communs.

| Module | Où |
|---|---|
| Définition (titre, décor, entités, sorties) | `BRLevels.cpp` (une fonction par niveau) ; la sortie de progression mène au niveau suivant du fil (`BRStory::Order`) |
| Place dans l'histoire, but, traces de L. | `BRStoryLogic.cpp` (ordre, déjà complet), `BRMissionText.cpp` (`GoalTitle`, `StoryLine`), notes du niveau |
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

**Exécuté ici.** `Tools/Content/test_content.cpp`, 68 vérifications OK (`Docs/v413/banc_contenu.txt`) ; banc de
l'histoire `Tools/Story/test_story.cpp`, 44 vérifications OK (`Docs/v413/banc_histoire.txt`). Le banc de contenu lit les sorties
réelles des niveaux dans `BRLevels.cpp` et la progression de la base dans `BRMissionWorld.cpp`, puis vérifie :

- dans chaque canal :
  - aucune sortie ne mène à un niveau indisponible ;
  - tous les niveaux disponibles sont atteignables ;
  - chaque mission garde une sortie qui mène quelque part ;
  - le fil se termine : fin du contenu au Niveau 2 (publiée), au Niveau 4 (test interne), dernier quai en développement ;
  - aucune sortie ne saute une étape du fil (aucun raccourci) ;
- les tirages au hasard (seulement après une fin, parmi les niveaux atteints) et la reprise des sauvegardes ;
- les refus réseau.

Les tests des outils de paquet (`Tools/Build/tests`) vérifient en plus qu'une version interne n'est jamais préparée
comme version publique pour Steam.

**Préparé, non exécuté.**

- Constructions Shipping par canal.
- `-BRAutoTestV413` : porte de service à pied sur cinq graines, histoire `0 → 1 → 37 → 2 → fin du chapitre` en marchant,
  raccourcis refusés par l'hôte, migration de vrais fichiers du format 4 ; porte stable après une mort, une sauvegarde
  et reprise, un rechargement ; Level 6 LiDAR (scanner, calibrage, mission en marchant, traversée jusqu'au Niveau 8,
  sauvegarde et reprise, anciennes sessions converties) ; `-BRNetTest` : porte ouverte par l'hôte pour le groupe, un seul
  départ ; Level 6 : même aile et même plan chez l'hôte et le client, calibrage de l'hôte reçu par le client.
- `-BRAutoTestV412`, dans la version publiée simulée (`BRLevels::SetChannelOverride`, hors Shipping) :
  - liste du menu ;
  - sorties du fil, sans raccourci ;
  - règle de connexion ;
  - reprise d'une partie v4.11 arrêtée au Niveau 5 (v4.13 : reprise aux Poolrooms, progression garée) ;
  - missions des Niveaux 1, 2 et 37 résolues en marchant, sorties prises à pied, arrivée ou écran de fin du contenu
    au Niveau 2.
- Essai à la main d'un client incompatible (voir § 4).

## 7. Steam (préparé, non exécuté)

- `Tools/Steam/make_steam_vdf.py --content public` (par défaut) ne prend que `Build/<Système>/<Config>/`, dont le
  manifeste doit dire `contenu : public`. `--content internal` prend `<Config>-internal/` pour une branche de test privée.
- Un manifeste sans canal (scripts antérieurs) ou d'un autre canal est refusé.
- `SetLive` reste vide et la prévisualisation reste active : aucune publication automatique. Le choix de la branche
  (publique ou de test) se fait à la main dans Steamworks.

  Les pages de partner.steamgames.com n'étaient pas accessibles depuis cet environnement (403) : la procédure exacte des
  branches n'a pas été vérifiée ici.

## 8. Notes de version du lancement (brouillon)

> **The Backrooms — Le Seuil (chapitre 1)**
>
> - Vous vous réveillez au Level 0. Une équipe est passée avant vous, elle signe « L. » et marque son chemin à la craie :
>   suivez-la, du Level 0 aux couloirs techniques, par l'Habitable Zone (Niveau 1) et les Poolrooms (Niveau 37).
> - Quatre niveaux, un seul chemin ; chaque niveau a un but concret (rétablir une liaison, découvrir un passage, sécuriser
>   une porte).
> - Solo ou coopération jusqu'à 4 joueurs par IP directe, chat vocal de proximité facultatif.
> - 22 langues ; Windows, Linux et macOS selon les paquets validés.
> - À la fin du Niveau 2 : fin du chapitre 1. Vos parties seront conservées et l'histoire reprendra là où elle s'est
>   arrêtée lors des prochaines mises à jour. REJOUER permet de revisiter les niveaux découverts.
>
> *Prévu (sans date)* : la Station et les Bureaux (Niveaux 3 et 4), puis l'Hôtel et le Noir, les Grottes, la Banlieue et
> le Champ, et la Ville.

La communication sépare toujours ce qui est disponible (liste ci-dessus) de ce qui est prévu (en italique, sans date).
