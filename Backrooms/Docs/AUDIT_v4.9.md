# Audit v4.8 → v4.9

Audit fait le 5 octobre 2026 sur la branche `backrooms`, commit `d37782e` (v4.8, 6/6). Aucun push plus récent.

## Environnement : ce qui peut être vérifié ici

| Élément | Constat |
|---|---|
| Machine | Conteneur Linux x86_64, 4 cœurs, 15 Go de RAM, **aucun GPU**. |
| Unreal Engine | **Absent.** Ni éditeur, ni UAT, ni toolchain Linux d'Epic, ni Xcode. La version exacte ne peut pas être lue ici. Le projet indique `EngineAssociation 5.8`, et le commit `863c4db` a été compilé par l'utilisateur sous 5.8.3. |
| Documentation | `dev.epicgames.com`, `partner.steamgames.com` et les guides NVIDIA sont **refusés par la politique réseau** de cette session. Les points qui en dépendent sont marqués « à vérifier dans la documentation ». Pour les variables du moteur, le jeu vérifie à l'exécution qu'elles existent. |
| Possible ici | clang (syntaxe avec en-têtes simplifiés), Python, Pillow et libraqm, scripts. |
| Non vérifiable ici | Compilation réelle (Windows, Linux, macOS), cuisinage, paquets, rendu, captures, images par seconde, Steam, signature Apple. Ces points seront marqués **préparé, non exécuté**. |

## 1. Constats du prompt, vérifiés dans les sources

| Point | Vérifié dans | Constat | Travail |
|---|---|---|---|
| HUD d'exploration | `BRHUD.cpp` `DrawStats` | Barres permanentes : **santé**, santé mentale, endurance ; oxygène sous l'eau ; piles avec une lampe. | Plus aucune barre de statut en exploration. |
| Inventaire | `DrawCharacterTab` | Bloc BIOMÉTRIE à **quatre** jauges : santé mentale, santé, endurance, piles. | Exactement deux jauges : ENDURANCE et SANTÉ MENTALE. |
| Onglet à l'ouverture | `DrawInventory`, `OnInventory` | L'onglet de la visite précédente est gardé : on peut rouvrir sur PARAMÈTRES en pleine partie. | Onglet PERSONNAGE à chaque ouverture en partie. |
| Menu titre | `DrawHUD` | Depuis le menu titre, l'inventaire reçoit le **personnage présent derrière le menu** : les onglets PERSONNAGE et JOURNAL et ses jauges sont accessibles. | Dans le menu titre : paramètres et touches seulement. |
| Tab et capture | `OnInventory`, `PollKeyCapture`, `PlayerTick` | La capture est protégée : l'action est ignorée pendant la capture, et la capture lit la touche après le traitement des actions de la même image. Tab se capture donc sans fermer l'écran. | Garder ; ajouter un test. |
| Touches maintenues | `SetInventoryOpen` | À la fermeture, le sprint est seulement coupé ; une touche encore enfoncée (Maj, ZQSD) reprend son effet aussitôt. | Purger les touches enfoncées à l'ouverture et à la fermeture. |
| Changement de niveau | `DrawHUD` | L'inventaire reste ouvert pendant un changement de niveau, avec un glisser-déposer possible sur un état qui change. | Le fermer au début de la transition. |
| Batterie, oxygène | `DrawStats` | Uniquement en barres. Signes existants : vignette et voile de manque d'air (`Choke`), halètement, lampe qui grésille sous 15 %. | Charge affichée dans l'inspection et l'infobulle des objets lumineux ; message contextuel de manque d'air. |
| Retours de blessure | `BRCharacter.cpp` (`UpdateAudio`, post-traitement) | Battements de cœur selon la santé, la peur et la santé mentale ; souffle selon l'endurance ; flash de dégâts. Aucun signe visuel durable d'une santé basse. | Signe discret de blessure grave, réglé par le confort ; pas d'effet plein écran permanent. |
| Noms des coéquipiers | `DrawTeammates` | Projetés jusqu'à 50 m **sans contrôle d'occlusion** : visibles à travers les murs et les étages. | Ligne de vue, avec des exceptions pour la réanimation. |
| Affichage | `ApplySettings` | Mode écrit à la fois dans `BackroomsPlayer.ini` (`WindowMode`) et dans `GameUserSettings` : **deux sources**. Le mode est réappliqué à **chaque** `ApplySettings`, donc à chaque changement de profil graphique. Ni résolution, ni confirmation, ni retour. | `UGameUserSettings` seul ; résolution ; confirmation de 15 s ; retour au lancement. |
| Préchargement | `UBRAssets::StartPreload` | Tout `/Game/Backrooms` (maillages, textures, sons, matériaux) chargé dès le menu et **gardé toute la session** par un handle statique. Coût mémoire non mesuré. | Ensembles par besoin (commun, niveau, transitions), libération, mesures. |
| Dépendances C++ | `Backrooms.Build.cs` | `BRAssets.cpp` inclut `AssetRegistry/AssetRegistryModule.h` et `IAssetRegistry.h` : le module `AssetRegistry` **n'est pas déclaré**. Les autres en-têtes appartiennent à des modules déclarés (`Engine`, `SlateCore`, `Slate`, `RenderCore`, `RHI`, `Sockets`, `Core`). | Déclarer `AssetRegistry` ; build sans unity (script). |
| Plugins | `Backrooms.uproject` | `PythonScriptPlugin` et `EditorScriptingUtilities` sont activés pour **toutes** les cibles, jeu empaqueté compris. Ils ne servent qu'à l'import dans l'éditeur. | Les limiter à l'éditeur (`TargetAllowList`). |
| Reflets | `ApplySettings` | Sans *hit lighting* ni ombres RT, `r.RayTracing.Geometry.SkeletalMeshes 0` : les créatures ne sont visibles dans un reflet **que si elles sont à l'écran** (traces d'écran). Hors champ, elles disparaissent du reflet. | Option prise en charge par le moteur, annoncée honnêtement. |
| Cadence des créatures | `ABREntity::ApplySkins` | Hors vue (`WasRecentlyRendered(0.25)` faux) : pose mise à jour **5 fois par seconde**, quelle que soit la distance. Une créature proche, visible seulement dans un reflet, est saccadée. | Cadence pleine près du joueur ; décisions de jeu inchangées. |
| Lumières | `ABRChunk::RefreshLightTypes` | Toutes les lumières concernées d'un chunk sont recréées dans **un seul appel** (un chunk par image). | Curseur et budget partagé. |
| Plateformes | `Config/` | Seul Windows est configuré (`DefaultGraphicsRHI_DX12`, `PCD3D_SM6`). Aucun `Config/Linux` ni `Config/Mac`. `r.RayTracing=True` vaut pour toutes les plateformes, y compris Vulkan et Metal, où il est expérimental. | Configurations par plateforme, profil de base sans RT matériel. |
| Message RT | `GetSettingHint` | « DirectX 12 et carte compatible requis » affiché quelle que soit la plateforme. | Message selon la plateforme et les capacités réelles. |
| Préférence RT importée | `LoadSettings` | Un `BackroomsPlayer.ini` copié d'un PC RTX garde `bHardwareRT` ; sans RT disponible, Lumen repasse en logiciel, mais le profil reste affiché comme RT. | Le profil effectif suit les capacités ; la préférence reste. |
| Micro sur Mac | — | Aucune description d'usage du micro pour le paquet Mac. | Ajout au paquet par le script Mac ; jeu jouable sans micro. |
| Steam | `DefaultEngine.ini` | `DefaultPlatformService=Null` ; aucun script SteamPipe. Le transport (connexion IP directe) et le chat vocal passent par le moteur, sans Steam. | Scripts SteamPipe ; Steamworks non activé (§ 2). |
| Chemins | sources | Aucun chemin Windows en dur. Sauvegardes et réglages sous `Saved/` (dossier utilisateur dans un paquet Linux ou Mac). Polices lues par `IFileManager` (paquet compris). Les inclusions du module ont la bonne casse : la vérification de syntaxe tourne sous Linux, dont le système de fichiers distingue les majuscules. | Contrôle automatique de la casse des références de ressources. |

## 2. Décisions

- **Santé** : mécanique inchangée (dégâts, bandages, soins, armure, mort, réanimation, sauvegarde, autorité du serveur).
  Seul l'affichage disparaît.
- **Steamworks n'est pas activé.** Vendre sur Steam ne l'exige pas. Le jeu n'a ni succès, ni lobby, ni invitation :
  la coopération passe par une adresse IP. L'Overlay est injecté par le client Steam sans SDK, ce qui reste à vérifier
  sur chaque OS. ~~Activer `OnlineSubsystemSteam` changerait le transport réseau et le chat vocal, et romprait la
  connexion directe~~ **(erreur corrigée en v4.10, voir `RAPPORT_v4.10.md` § Steam)** : activer Steamworks ne rompt
  pas forcément la connexion directe. Cela dépend des pilotes réseau déclarés (le pilote IP peut rester celui des
  adresses IP, le pilote Steam servant aux invitations) et du choix du chat vocal. Rien de cela n'est vérifié ici.
  C'est noté comme évolution possible, pas comme besoin.
- **RT matériel sur Linux et Mac** : désactivé dans le profil de base (Vulkan et Metal : support expérimental selon la
  documentation citée par le prompt, non vérifiable ici). Il peut être réactivé par la configuration de la plateforme,
  après validation sur une machine.

La suite (problème reproduit, modification, fichiers, test et résultat) est dans `RAPPORT_v4.9.md`.
