# Rapport v4.9 : HUD sans vie, affichage confirmé, Linux et macOS, Steam

Travail fait le 5 octobre 2026 sur la branche `backrooms`, à partir de `d37782e` (v4.8, 6/6).
L'état des lieux, avec les constats vérifiés dans les sources, est dans [`AUDIT_v4.9.md`](AUDIT_v4.9.md).

Commits :

- `25a8315` (1/4) : HUD, inventaire, affichage, reflets, fluidité ;
- `e63805b` (2/4) : Linux, macOS, scripts de construction, SteamPipe, traductions ;
- `aafc46a` (3/4) : tests automatiques ;
- 4/4 : deux tests de plus, ce rapport et le README.

## 1. Ce qui n'a pas pu être fait ici

Le conteneur de travail est un Linux x86_64 **sans carte graphique, sans Unreal Engine, sans Xcode et sans accès à
Steamworks**. De plus, la documentation d'Epic, de Valve et de NVIDIA est refusée par la politique réseau de la
session.

Sont donc **préparés, non exécutés** :

- la compilation (Windows, Linux, macOS), la cuisson, les paquets et leur installation ;
- le jeu lui-même : PIE, Standalone, paquet, coopération entre systèmes ;
- **toutes les captures** du HUD, de l'inventaire et des modes d'affichage. Le test automatique les produit (§ 5.2),
  mais aucune n'est jointe ici ;
- **toutes les mesures** : images par seconde, temps CPU et GPU, mémoire, objectif de 60 images/s en RTX FLUIDE à
  1440p ;
- la signature et la notarisation Apple, l'envoi SteamPipe, l'Overlay Steam.

Ce qui a été vérifié ici est listé au § 4. Un contrôle de syntaxe avec des en-têtes simplifiés ne prouve pas que le
jeu compile avec Unreal.

## 2. Corrections

Pour chaque correction, la colonne « Problème » décrit le défaut tel qu'il a été reproduit **dans les sources** (lecture
du code de `d37782e`, voir l'audit). Aucun ne l'a été en jeu.

Colonne « Test » :

- **test** : étape du test automatique qui le vérifie, à lancer (§ 5.2) ;
- **réseau** : étape de `-BRNetTest` ;
- **à l'œil** : à regarder en jeu ;
- **hors moteur** : contrôle fait ici.

Colonne « Résultat » : « préparé, non exécuté » tant que le test n'a pas tourné dans Unreal.

### 2.1 HUD et inventaire

| # | Problème | Modification | Fichiers | Test | Résultat |
|---|---|---|---|---|---|
| H1 | En exploration : barres permanentes de **santé**, santé mentale et endurance ; oxygène sous l'eau ; piles avec une lampe (`DrawStats`). | `DrawStats` supprimé. **Aucune jauge de statut en exploration**, et aucun réglage ne les y remet. Il reste un signe contextuel : un message d'air sous l'eau (§ H7). | `BRHUD.cpp`, `BRHUD.h` | test `aucune jauge en exploration`, et jauges à 0 dans chaque réglage d'interface (`toujours`, `masques`, tailles) | préparé, non exécuté |
| H2 | Inventaire : bloc BIOMÉTRIE à **quatre** jauges (santé mentale, santé, endurance, piles). | Panneau **ÉTAT** à exactement deux jauges, distinctes par la forme et l'icône, pas seulement par la couleur. **ENDURANCE** : 10 segments et une icône en chevrons. **SANTÉ MENTALE** : barre continue et une icône en œil. Chacune donne un pourcentage, un état en mots (reposé / essoufflé / à bout de souffle ; stable / nerveux / au bord de la rupture) et une tendance. La disposition est inversée en arabe et en persan. | `BRHUD.cpp` (`DrawStatusPanel`, `DrawStatusGauge`) | test `onglet Personnage et deux jauges` ; captures `V49_inventaire_personnage`, `V49_jauges_basses` | préparé, non exécuté |
| H3 | L'inventaire rouvrait sur l'onglet de la visite précédente, par exemple PARAMÈTRES en pleine partie. | En partie, Tab ouvre toujours sur **PERSONNAGE**. Depuis le menu titre ou la pause, il ouvre sur PARAMÈTRES. | `BRPlayerController.cpp` (`OnInventory`) | test `Tab : ouverture` (onglet forcé à Paramètres avant) | préparé, non exécuté |
| H4 | Au menu titre, l'inventaire recevait le **personnage présent derrière le menu** : ses onglets PERSONNAGE et JOURNAL et ses jauges étaient accessibles. | Au menu titre, l'inventaire est dessiné sans personnage : PARAMÈTRES et TOUCHES seulement. | `BRHUD.cpp` (`DrawInventory`) | test `sans personnage fictif` ; capture `V49_menu_parametres` | préparé, non exécuté |
| H5 | À la fermeture, une touche encore enfoncée (Maj, ZQSD) reprenait son effet aussitôt. | Touches enfoncées purgées (`FlushPressedKeys`) à l'ouverture et à la fermeture ; il faut appuyer de nouveau. | `BRPlayerController.cpp` (`SetInventoryOpen`) | test `sprint purge` et `pas de reprise toute seule` ; **l'appui physique tenu se vérifie à la main** | préparé, non exécuté |
| H6 | L'inventaire restait ouvert pendant un changement de niveau, avec un glisser-déposer possible sur un état qui change. | Fermé au début de la transition, comme à la mort (déjà le cas). | `BRHUD.cpp` | test `transition : inventaire ouvert` | préparé, non exécuté |
| H7 | Piles et oxygène n'étaient montrés que par des barres. | **Piles** : la charge est donnée dans l'infobulle et l'inspection des objets lumineux et des piles de rechange (« Charge des piles : 72 % »). Un seul avertissement apparaît quand elle passe sous 15 % (« Piles faibles : la lampe va s'éteindre ») ; la lampe grésille, comme avant. **Air** : message sous l'eau quand le souffle passe sous 45 % (« Manque d'air : remontez respirer »), puis plus pressant sous 20 % ; voile et vignette d'asphyxie déjà présents. | `BRHUD.cpp` (`ChargeLine`, `DrawContextCues`), `BRCharacter.*` | test `piles faibles` ; air : à l'œil (Poolrooms) | préparé, non exécuté |
| H8 | Sans barre, une santé basse n'avait plus de signe visuel durable. | Signe de blessure grave sous 35 de santé : vignette (au plus +0,3) et légère désaturation, réglées par FLASHS ET CLIGNOTEMENTS. Ce n'est pas un effet plein écran permanent. **Mécanique de santé inchangée** : dégâts, bandages, soins, gilet, mort, réanimation, sauvegarde, autorité du serveur. | `BRCharacter.cpp` | à l'œil | préparé, non exécuté |
| H9 | Tab pendant la capture d'une nouvelle touche. | Comportement déjà correct, vérifié dans le code : l'action est ignorée pendant la capture, qui lit la touche après les actions de la même image. Tests ajoutés. | — | tests `pendant la reaffectation`, `touche reaffectee` (commandes reconstruites, ouverture et fermeture) | préparé, non exécuté |
| H10 | Exigence : l'inventaire n'est ni une pause ni une protection. | Inchangé. | — | test `coup recu inventaire ouvert` (la santé baisse, l'écran reste ouvert) ; mort : fermeture vérifiée dans `BRHUD.cpp` ; déconnexion : à la main | préparé, non exécuté |

### 2.2 Présentation

| # | Problème | Modification | Fichiers | Test | Résultat |
|---|---|---|---|---|---|
| I1 | Informations permanentes en exploration, sans réglage. | Nouvelle catégorie **INTERFACE** dans les paramètres. **TAILLE DE L'INTERFACE** : de 0,8 à 1,25. **OPACITÉ** : de 40 à 100 % ; les consignes d'interaction et la réanimation restent au moins à 75 %. **RÉTICULE** : point, sur les objets, ou aucun. **OBJETS RAPIDES** : brièvement (3 s après un changement), toujours, ou masqués. **OBJECTIFS** : brièvement (8 s à l'arrivée et à chaque progrès), toujours, ou masqués ; ils restent toujours dans l'onglet Personnage. | `BRTypes.h`, `BRPlayerController.cpp`, `BRHUD.cpp` | tests `interface : toujours / masques / brievement`, `objets rapides montres`, `caches ensuite` | préparé, non exécuté |
| I2 | Une seule longue liste de paramètres. | Quatre catégories : JEU, VIDÉO, INTERFACE, GRAPHISMES. | `BRHUD.cpp`, `BRPlayerController.cpp` (`GetSettingCategory`) | test `parametres : lignes` (5 lignes dans INTERFACE, libellés, aides) | préparé, non exécuté |
| I3 | Textes longs : la police était réduite jusqu'à 72 %. | Objectifs et journal (notes, entités) passent à la ligne et défilent à la molette, avec des repères quand du texte est caché. La réduction reste réservée aux libellés courts. | `BRHUD.cpp` (`ScrollArea`) | à l'œil (allemand, russe, arabe) | préparé, non exécuté |
| I4 | Noms des coéquipiers projetés jusqu'à 50 m **sans contrôle d'occlusion**, visibles à travers les murs et les étages. | Ligne de vue depuis la caméra. Un coéquipier à terre et relevable, caché par un mur, garde un repère discret (« + » et distance, sans nom). L'indicateur de voix dit qui parle, sans position. | `BRHUD.cpp` (`DrawTeammates`) | réseau `nom a vue`, `nom cache` | préparé, non exécuté |
| I5 | Échelle de l'interface et zones cliquables selon la résolution. | `Ui()` = hauteur / 1080 × taille choisie, plafonnée à hauteur / 820 pour que tout tienne. Dessin et zones cliquables utilisent la même échelle. | `BRHUD.cpp` | test `taille` (inventaire et boutons dans l'écran à 0,8, à 1,25 en allemand, à 1 en arabe). 1280×720, 1440p, 4K, 21:9 et Retina : à lancer avec `-ResX/-ResY` (§ 5.2) | préparé, non exécuté |
| I6 | Textes ajoutés ou retirés par la v4.9. | 54 nouveaux textes et 6 retirés (santé, oxygène, piles, biométrie, deux anciennes aides). Les 22 langues sont à 724/724. Traductions **automatiques, aucune relue**. En arabe et en persan, « demandé → obtenu » devient « demandé (réel : obtenu) » : la police arabe n'a pas de flèches, et une flèche se lit à l'envers de droite à gauche. | `BRLocKeys.inl`, `Content/Localization/Game/*` | hors moteur : `loc_build.py` (0 refusé), glyphes des polices CJK et arabe | **fait hors moteur** ; relecture humaine à faire |

### 2.3 Modes d'affichage

| # | Problème | Modification | Fichiers | Test | Résultat |
|---|---|---|---|---|---|
| D1 | Deux sources pour le mode : `BackroomsPlayer.ini` (`WindowMode`) et `GameUserSettings`. | `UGameUserSettings` est la seule source du mode et de la résolution. L'ancienne valeur est reprise une fois au lancement. | `BRDisplay.*`, `BRPlayerController.cpp` | test `affichage : etat` | préparé, non exécuté |
| D2 | Le mode était réappliqué à chaque `ApplySettings`, donc à chaque changement de profil graphique. | `ApplySettings` ne touche plus la fenêtre. | `BRPlayerController.cpp` | test `profil graphique` (profil PERFORMANCE appliqué, affichage inchangé) | préparé, non exécuté |
| D3 | Aucun choix de résolution. | Ligne **RÉSOLUTION** adaptée au mode : en plein écran, les modes de l'écran ; en plein écran fenêtré, la définition de l'écran (fixe, avec un message) ; en fenêtre, les tailles qui tiennent sur le bureau, plus la dernière taille fenêtrée. L'échelle de rendu (RÉSOLUTION DE RENDU) reste un réglage à part. | `BRDisplay.cpp`, `BRPlayerController.cpp` | test `affichage : etat` (nombre de résolutions par mode) | préparé, non exécuté |
| D4 | Aucune confirmation. | Après un changement de mode ou de résolution, confirmation de **15 s** : CONSERVER [ENTRÉE] ou RÉTABLIR [ÉCHAP], avec un compte à rebours. Sans réponse, retour à la dernière configuration confirmée. Rien n'est écrit avant la confirmation. Le volume et les autres réglages sans risque ne passent pas par ce dialogue. | `BRDisplay.*`, `BRHUD.cpp` (`DrawVideoConfirm`), `BRPlayerController.cpp` | tests `essai`, `RETABLIR`, `confirmation`, `confirme`, `expiration` ; capture `V49_affichage_confirmation` | préparé, non exécuté |
| D5 | Arrêt du jeu avant la confirmation (plantage, fermeture). | Changement en attente noté (`Pending`) dans `BackroomsPlayer.ini`. Au lancement suivant, retour à la configuration confirmée et message « Affichage rétabli ». | `BRDisplay.cpp` | **à la main** : fermer le jeu pendant le compte à rebours, relancer | préparé, non exécuté |
| D6 | Changements faits hors du jeu : Alt+Entrée, redimensionnement à la souris, mode imposé par le système, écran retiré. | Suivi toutes les 0,5 s (2 s de grâce après un changement du jeu). Une taille de fenêtre est retenue après 1 s de stabilité. Au lancement, une fenêtre qui ne tient plus sur le bureau y est ramenée. | `BRDisplay.cpp` | à la main | préparé, non exécuté |
| D7 | Linux et macOS : pas de plein écran exclusif. | Le plein écran exclusif n'est annoncé que sous Windows. Ailleurs, le libellé indique « demandé → obtenu » (mode réel lu sur la fenêtre), avec une explication. | `BRDisplay.cpp`, `BRPlayerController.cpp` | test `affichage : etat` (demandé, obtenu) | préparé, non exécuté |
| D8 | Dans l'éditeur, la fenêtre est celle de l'éditeur. | Aucun effet, avec un message. Les tests d'affichage demandent le jeu lancé seul. | `BRDisplay.cpp` | test (sauté avec une note en PIE) | préparé, non exécuté |

### 2.4 Rendu et fluidité

| # | Problème | Modification | Fichiers | Test | Résultat |
|---|---|---|---|---|---|
| R1 | Sans *hit lighting* ni ombres RT, `r.RayTracing.Geometry.SkeletalMeshes 0` : une créature n'est visible dans un reflet **que si elle est à l'écran** (traces d'écran). | Réglage **CRÉATURES DANS LES REFLETS** : **ÉCRAN SEULEMENT**, dont la limite est dite dans l'aide, ou **RAYONS**. RAYONS met les créatures dans la scène ray tracée et active `r.Lumen.Reflections.HardwareRayTracing.Retrace.HitLighting`, qui éclaire par les rayons les points que le cache de surfaces ne couvre pas. Le jeu vérifie que cette variable existe ; si le moteur ne l'a pas, il affiche **ÉCRAN (MOTEUR)**. Avec *hit lighting*, la valeur affichée est **COMPLETS (RAYONS)**. `r.RayTracing.Geometry.SkeletalMeshes` vaut 1 dès que l'un des trois est actif. | `BRPlayerController.cpp`, `BRTypes.h` | test `reflets : creatures` (variables du moteur) ; rendu à l'œil : créature devant puis hors champ près d'une flaque, *hit lighting* activé puis désactivé | préparé, non exécuté |
| R2 | Hors de la vue, la pose d'une créature passait à 5 images/s, même tout près : saccadée dans un reflet proche. | Hors de la vue : pleine cadence à moins de 15 m, 30 images/s jusqu'à 30 m, 5 au-delà. À l'écran : inchangé (pleine cadence à moins de 25 m, 20 images/s au-delà). Seule la pose affichée est concernée ; les **décisions** (poursuite, attaques) restent prises à chaque image. | `BREntity.*` | test `creatures : de face / de dos` (cadence attendue, Tick à chaque image) | préparé, non exécuté |
| R3 | `RefreshLightTypes` recréait toutes les lumières d'un chunk d'un coup. | Tranche par image de max(0,25 ms ; 25 % du budget de génération), avec au moins une lumière. Le chunk reste en tête de file jusqu'à ce qu'il soit à jour. Mesures : lumières recréées, pire tranche. | `BRChunk.*`, `BRWorld.*` | test `lumieres : changement de type` (mesure de 3 s, chunks à jour, pire tranche) | préparé, non exécuté |
| R4 | Tout `/Game/Backrooms` était chargé dès le menu et gardé toute la session. | Préchargement **par ensembles** : `common` au menu, puis par niveau les textures de ses surfaces et ses créatures, ainsi que celles des niveaux où mènent ses sorties. Les ensembles devenus inutiles sont relâchés. Pour chaque ensemble, le rapport donne le nombre de ressources, la durée, la RAM et les textures. `-BRPreloadAll` rétablit l'ancien comportement pour comparer. | `BRAssets.*`, `BRWorld.cpp` | test `prechargement` (Niveau 0, 37, retour au 0 : mêmes ensembles, pas d'accumulation) ; mesures comparées : § 5.3 | préparé, non exécuté |
| R5 | Une préférence RT copiée d'un PC RTX, sans RT disponible, laissait un profil « RT » affiché. | Ray tracing effectif = préférence **et** disponibilité au démarrage. Sans RT, les lignes RT ne changent plus et donnent la raison, selon la plateforme et le RHI (Windows, Linux, macOS). | `BRPlayerController.cpp` | test `ray tracing indisponible` (sur une machine sans RT) | préparé, non exécuté |
| R6 | Objectif : 60 images/s stables en RTX FLUIDE à 1440p (RTX 4080, Ryzen 7 7800X3D). | Aucun réglage de plus : c'est un objectif **à mesurer**. Le profil Cinématique reste disponible. | — | § 5.3 | **non mesuré** |

Matériaux, rugosité, visières, appuis, transitions d'animation et LOD des modèles importés : **inchangés en v4.9**. La
demande dit de ne pas refaire le pipeline sans cause reproduite, et rien n'a pu être reproduit ici. Les diagnostics de
la v4.8 (slots, usages, sections) restent le point de départ (§ 5.4).

### 2.5 Plateformes et construction

| # | Problème | Modification | Fichiers | Test | Résultat |
|---|---|---|---|---|---|
| B1 | `BRAssets.cpp` utilise l'Asset Registry sans déclarer le module. | `AssetRegistry` et `ApplicationCore` (fenêtres et écrans de `BRDisplay`) en dépendances directes. Construction sans unity proposée par le script Windows (`-NoUnityCheck`). | `Backrooms.Build.cs`, `Tools/Build/build_windows.ps1` | compilation | préparé, non exécuté |
| B2 | Plugins d'import (Python, Editor Scripting) activés pour toutes les cibles. | Limités à l'éditeur (`TargetAllowList`). Plateformes cibles déclarées : Windows, Linux, Mac. | `Backrooms.uproject` | paquet | préparé, non exécuté |
| B3 | Seul Windows était configuré ; `r.RayTracing=True` partout. | Linux : Vulkan SM6, avec SM5 en repli. macOS : Metal SM5 et SM6. RT matériel coupé dans `Config/Linux` et `Config/Mac` (Lumen logiciel). Les noms des RHI sont **à confirmer dans l'éditeur 5.8**. | `DefaultEngine.ini`, `Config/Linux/LinuxEngine.ini`, `Config/Mac/MacEngine.ini` | cuisson | préparé, non exécuté |
| B4 | Casse des chemins sous Linux et macOS. | `Tools/check_paths.py` : chaque nom de ressource, police et inclusion est comparé, casse comprise, aux fichiers. | `Tools/check_paths.py` | hors moteur : 739 références (dont les inclusions du nouveau test), **0 différence de casse**, 30 noms composés à l'exécution | **fait hors moteur** |
| B5 | Aucun script de construction reproductible. | `Tools/Build/versions.env` fixe UE 5.8.3 ; toolchain Linux et Xcode à renseigner après la première construction validée. Scripts `build_windows.ps1` (Win64, ou Linux en croisé), `build_linux.sh` et `build_mac.sh`, avec le chemin du moteur en paramètre. Chaque script refuse un moteur d'une autre version, écrit un manifeste (commit, outils, hôte) et sépare les paquets par plateforme (`Build/<Plateforme>/<Config>`). `check_package.py` vérifie qu'il ne manque rien. | `Tools/Build/*` | hors moteur : `bash -n`, `check_package.py` sur des paquets factices | scripts **préparés, non exécutés** |
| B6 | Mac : micro, binaire universel, signature. | `build_mac.sh` produit un binaire universel arm64+x86_64, vérifié par `lipo`. Il ajoute la description d'usage du micro (`NSMicrophoneUsageDescription`), puis signe si `MAC_SIGN_IDENTITY` est défini, et notarise et agrafe si `NOTARY_PROFILE` est défini. Droits : micro ; bibliothèques injectées pour l'Overlay Steam (**à confirmer**) ; pas de bac à sable. | `Tools/Build/build_mac.sh`, `Tools/Build/mac/Backrooms.entitlements` | hors moteur : plist valide | préparé, non exécuté |
| B7 | Micro refusé ou absent. | Le chat vocal reste facultatif : le jeu ne dépend pas du micro. | — | à la main (refuser puis réautoriser dans Réglages Système) | préparé, non exécuté |

### 2.6 Steam

| # | Besoin | Livré | Fichiers | Résultat |
|---|---|---|---|---|
| S1 | SteamPipe avec AppID et DepotID à renseigner, un depot par OS. | `make_steam_vdf.py` lit les identifiants en arguments ou dans l'environnement ; aucun identifiant n'est dans le dépôt ni par défaut. Il écrit `Build/SteamPipe/app_build.vdf` et un `depot_<os>.vdf` par système (aucune donnée partagée : shaders DirectX, Vulkan et Metal différents), en n'incluant que les paquets présents. `Preview` vaut 1 par défaut (rien n'est envoyé) ; `SetLive` est vide (aucune publication). | `Tools/Steam/make_steam_vdf.py` | hors moteur : testé avec de faux identifiants et paquets |
| S2 | Envoi. | `upload_steam.sh` : `steamcmd +login $STEAM_BUILD_USER +run_app_build`. Le mot de passe et Steam Guard sont demandés par steamcmd. | `Tools/Steam/upload_steam.sh` | préparé, non exécuté |
| S3 | Lancement par OS. | À déclarer dans Steamworks (Installation → General) : Windows `Backrooms.exe`, Linux `Backrooms.sh`, macOS `Backrooms.app`. Le binaire universel choisit l'architecture native. | — | préparé, non exécuté |
| S4 | Fonctions Steam. | **Steamworks n'est pas activé** (`OnlineSubsystemSteam` absent). Le jeu n'a ni succès, ni lobby, ni invitation ; la coop passe par l'adresse IP et le chat vocal par le moteur. Activer Steamworks changerait le transport et le chat vocal, et romprait la connexion directe. L'Overlay est injecté par le client Steam, **à vérifier sur chaque OS**. | `Config/DefaultEngine.ini` (inchangé) | décision documentée |

Ne pas annoncer Linux ni macOS sur la fiche Steam avant un paquet natif installé et testé. Aucune compatibilité Steam
Deck n'est revendiquée.

## 3. Plateformes

| Cible | Matériel | OS | RHI | Unreal | Résultat | Limites |
|---|---|---|---|---|---|---|
| Windows 64 bits | RTX 4080 + Ryzen 7 7800X3D (machine de l'utilisateur) | Windows 10/11 | DirectX 12 SM6 ; RT matériel si disponible, sinon Lumen logiciel | 5.8.3 | **préparé, non exécuté** pour la v4.9. Dernière compilation connue : `863c4db` (v4.7), par l'utilisateur. | 60 images/s en RTX FLUIDE à 1440p : non mesuré. |
| Linux x86_64 | non testé | distribution à choisir | Vulkan SM6 (SM5 en repli) | 5.8.3 + chaîne clang d'Epic (à fixer dans `versions.env`) | **préparé, non exécuté** | RT matériel coupé ; noms des RHI à confirmer ; Proton ne remplace pas ce paquet. |
| macOS (arm64 + x86_64) | non testé (Apple Silicon et Intel à tester séparément) | macOS pris en charge par UE 5.8 | Metal SM5 / SM6 | 5.8.3 + Xcode (à fixer dans `versions.env`) | **préparé, non exécuté** | RT matériel coupé ; plein écran = fenêtre sans bordures (mode obtenu affiché) ; signature et notarisation : compte Apple requis. |
| Steam | — | Windows, Linux, macOS | — | — | **préparé, non exécuté** | Aucun accès Steamworks ici ; Overlay à vérifier par OS. |
| Conteneur de travail | 4 cœurs, sans GPU | Linux x86_64 | — | absent | contrôles hors moteur seulement (§ 4) | ne prouve ni la compilation ni le rendu |

## 4. Vérifié ici, et comment

| Contrôle | Outil | Résultat |
|---|---|---|
| Syntaxe C++ de tous les fichiers du module | clang avec des en-têtes Unreal simplifiés (stubs) | OK, sauf `BRConfig.cpp`, `BRGameMode.cpp` et `BRWaterSim.cpp`, auxquels les stubs ne suffisent pas (même état qu'en v4.8). Fonctions ajoutées aux stubs, toutes présentes dans Unreal : `GetViewportSize`, `IsGamePaused`, `TickInterval`, `BaseEyeHeight`, `EKeys::K`… |
| Variables locales qui masquent un membre | script | 0 |
| Noms dupliqués entre fichiers (build unity) | script | 0 |
| Sources C++ en ASCII | `grep` | OK |
| Ressources protégées (modèles fournis) | `Tools/protect_assets.py check` | OK |
| Casse des chemins | `Tools/check_paths.py` | 739 références, 0 différence |
| Traductions | `loc_build.py` | 22 langues, 724/724, 0 refusée ; relues : 0 |
| Glyphes | fontTools sur `Content/Fonts` | tous les caractères des catalogues CJK sont dans les polices ; flèches absentes de Noto Sans Arabic, d'où la forme sans flèche en arabe et en persan |
| Scripts de construction | `bash -n`, faux paquets pour `check_package.py`, plist Mac | OK ; `build_windows.ps1` non exécuté (pas de PowerShell ici) |
| SteamPipe | `make_steam_vdf.py` avec de faux identifiants | fichiers écrits, identifiants absents refusés ; `Build/` retiré ensuite |

## 5. Commandes à lancer

### 5.1 Construire

Windows (éditeur sans unity, puis paquet) :

```bat
powershell -ExecutionPolicy Bypass -File Tools\Build\build_windows.ps1 -UERoot "C:\Program Files\Epic Games\UE_5.8" -NoUnityCheck
```

Linux, sur une machine Linux :

```bash
UE_ROOT=/opt/UnrealEngine-5.8 Tools/Build/build_linux.sh Shipping
```

Linux, en croisé depuis Windows :

```bat
set LINUX_MULTIARCH_ROOT=C:\UnrealToolchains\<version pour UE 5.8>\
powershell -ExecutionPolicy Bypass -File Tools\Build\build_windows.ps1 -UERoot "C:\Program Files\Epic Games\UE_5.8" -Platform Linux
```

macOS, sur un Mac ; la signature et la notarisation sont facultatives :

```bash
export MAC_SIGN_IDENTITY="Developer ID Application: <nom> (<équipe>)"
export NOTARY_PROFILE=<profil créé par xcrun notarytool store-credentials>
UE_ROOT="/Users/Shared/Epic Games/UE_5.8" Tools/Build/build_mac.sh Shipping
```

Chaque script :

- s'arrête si le moteur n'est pas en 5.8.3 (`BR_ALLOW_OTHER_UE=1` pour passer outre) ;
- écrit `Build/<Plateforme>/<Config>/build_manifest.txt` ;
- lance `check_package.py`, qui doit finir par « paquet complet ».

Après la première construction validée, renseigner `LINUX_TOOLCHAIN` et `XCODE_VERSION` dans
`Tools/Build/versions.env`.

### 5.2 Tests automatiques

Les tests d'affichage demandent le **jeu lancé seul** : en PIE, la fenêtre est celle de l'éditeur.

```bat
UnrealEditor.exe Backrooms.uproject -game -windowed -ResX=1920 -ResY=1080 -BRAutoTest -BRAutoTestV49
```

Dans un paquet : `Backrooms.exe -BRAutoTest -BRAutoTestV49` (`Backrooms.sh` sous Linux, `Backrooms.app` sous macOS).

`Saved/AutoTest/Rapport.txt` ne doit lister aucun « PROBLEME ». Captures produites :

- exploration et inventaire : `V49_exploration`, `V49_inventaire_personnage`, `V49_jauges_basses`, `V49_menu_parametres`, `V49_piles_faibles` ;
- réglages d'interface : `V49_interface_toujours`, `V49_interface_masques`, `V49_objets_rapides_brefs` ;
- taille de l'interface : `V49_taille_080`, `V49_taille_125_de`, `V49_taille_100_ar` ;
- affichage : `V49_affichage_confirmation` ;
- créatures : `V49_creature_de_face`.

Résolutions et DPI : relancer avec `-ResX=1280 -ResY=720`, `2560 1440`, `3840 2160` et `3440 1440` (21:9). Sur un Mac
Retina, lancer en plein écran. Regarder ensuite les captures `V49_taille_*` : texte lisible, rien hors de l'écran,
clics alignés.

Réseau, sur une ou deux machines (et entre systèmes) :

```bat
UnrealEditor.exe "C:\...\Backrooms.uproject" "/Game/Backrooms/Maps/L_Backrooms?listen?BRLevel=0" -game -windowed -BRNetTest -ABSLOG=C:\Temp\Hote.log
UnrealEditor.exe "C:\...\Backrooms.uproject" 127.0.0.1 -game -windowed -BRNetTest -ABSLOG=C:\Temp\Client.log
```

Entre deux systèmes : la même chose avec les paquets (`Backrooms.exe`, `Backrooms.sh`, `Backrooms.app`), le client
rejoignant l'adresse IP de l'hôte.

Les étapes v4.9 placent l'hôte à 3 m du client, puis derrière un mur. Captures : `Net_v49_nom_a_vue`, `Net_v49_nom_cache`.

Non-régression : `-BRAutoTest` (tout, v4.7 et v4.8 compris), `-BRAutoTestPits` et `python Tools/protect_assets.py check`.

### 5.3 Mesures (RTX 4080 + 7800X3D)

À réglages et résolution identiques (2560×1440, profil RTX FLUIDE), comparer :

- **avant** : `-BRPreloadAll` (préchargement complet de la v4.8) ;
- **après** : sans option.

Faire chaque mesure après un démarrage à froid, puis après préchauffage (second lancement).

```bat
Backrooms.exe -BRAutoTest -BRAutoTestLevels=0,37 -ResX=2560 -ResY=1440
Backrooms.exe -BRAutoTest -BRAutoTestLevels=0,37 -ResX=2560 -ResY=1440 -BRPreloadAll
```

Pour chaque scène, le rapport donne :

- les temps CPU et GPU, p50, p95 et p99, le 1 % le plus lent ;
- la mémoire du processus et des textures ;
- les chargements synchrones ;
- les lumières recréées et la pire tranche ;
- les ensembles préchargés : ressources, durée, RAM et textures.

Si l'objectif de 60 images/s n'est atteint qu'en baissant l'échelle de rendu, le dire avec la valeur.

### 5.4 À vérifier à l'œil

- **Reflets** : *hit lighting* activé puis désactivé, CRÉATURES DANS LES REFLETS sur ÉCRAN SEULEMENT puis RAYONS, avec
  un Hound devant une flaque, puis hors champ mais visible dans la flaque. Noter si la valeur affichée est ÉCRAN
  (MOTEUR).
- **Combinaison** : texturée en lumière neutre, au Niveau 0, dans les Poolrooms, en coop et dans les reflets
  (diagnostic v4.8 dans le rapport).
- **Fosses, murs et angles** pendant un changement de profil : ni fuite de lumière, ni scintillement, ni ombre qui
  disparaît.
- **Affichage** :
  - Alt+Entrée ;
  - redimensionnement à la souris ;
  - retrait d'un second écran ;
  - fermeture pendant le compte à rebours, puis relance (message « Affichage rétabli ») ;
  - sous Linux et macOS : mode réellement obtenu, focus, souris capturée, curseur rendu après Alt+Tab, saisie de
    l'adresse IP ;
  - Overlay Steam (Maj+Tab) dans le paquet lancé depuis Steam.
- **Micro sous macOS** : refuser l'accès, jouer, réautoriser dans Réglages Système, puis relancer.

## 6. Fichiers modifiés ou ajoutés (depuis `d37782e`)

- **HUD et inventaire** : `BRHUD.h/.cpp`
- **Commandes, paramètres, rendu** : `BRPlayerController.h/.cpp`, `BRTypes.h`
- **Affichage** : `BRDisplay.h/.cpp` (nouveaux)
- **Blessure, piles** : `BRCharacter.h/.cpp`
- **Cadence des créatures** : `BREntity.h/.cpp`
- **Lumières étalées** : `BRChunk.h/.cpp`, `BRWorld.h/.cpp`
- **Préchargement** : `BRAssets.h/.cpp`
- **Tests** : `BRAutoTest.h/.cpp`, `BRAutoTestV49.cpp` (nouveau)
- **Modules** : `Backrooms.Build.cs`, `Backrooms.uproject`
- **Configurations** : `Config/DefaultEngine.ini`, `Config/Linux/LinuxEngine.ini`, `Config/Mac/MacEngine.ini`
- **Construction** : `Tools/Build/` (`versions.env`, `common.sh`, `build_linux.sh`, `build_mac.sh`,
  `build_windows.ps1`, `check_package.py`, `mac/Backrooms.entitlements`)
- **Steam** : `Tools/Steam/make_steam_vdf.py`, `Tools/Steam/upload_steam.sh`
- **Chemins** : `Tools/check_paths.py`
- **Traductions** : `BRLocKeys.inl`, 22 `Game.po` et `Game.locres`, `coverage.json`, `Docs/LOCALISATION_COUVERTURE.md`
- **Divers** : `.gitignore` (`/Build/` à la racine seulement ; l'ancienne règle ignorait aussi `Tools/Build`)
- **Documentation** : `Docs/AUDIT_v4.9.md`, `Docs/RAPPORT_v4.9.md`, `README.md`

## 7. Limites connues

- Rien n'a été compilé ni lancé dans Unreal. Les points les plus exposés à la compilation sont :
  - `SWindow::GetWindowMode` et `GetClientSizeInScreen` (`BRDisplay.cpp`) ;
  - `UKismetSystemLibrary::GetSupportedFullscreenResolutions` et `GetConvenientWindowedResolutions` ;
  - `FlushPressedKeys` ;
  - les noms des RHI cibles dans les `.ini` ;
  - la variable `r.Lumen.Reflections.HardwareRayTracing.Retrace.HitLighting`, dont l'absence est gérée à l'exécution.
- Le test du sprint purgé ne simule pas une touche physique tenue ; à vérifier à la main.
- Les noms des coéquipiers ne sont testés que du côté de l'hôte.
- Les 54 nouvelles traductions n'ont été relues par personne.
- Les captures et mesures demandées restent à produire (§ 5) ; aucune n'est inventée ici.
