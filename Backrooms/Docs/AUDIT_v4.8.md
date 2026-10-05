# Audit v4.7 → v4.8

Audit fait le 5 octobre 2026 sur la branche `backrooms`, commit `863c4db` (« Compilation sous Unreal 5.8.3 »).

Ce commit corrige trois erreurs de compilation de la v4.7 sous 5.8.3 :

- `GetFontMeasure()` renvoie un `TSharedPtr` ;
- la locale `Ink` masquait une constante de `BRHUD.cpp` en build unity ;
- `FConfigFile::SetInt` n'existe pas.

Mes vérifications hors moteur couvrent désormais ces cas :

- le stub suit l'API 5.8 sur ces deux points ;
- un nouveau contrôle signale toute locale qui masquerait une globale d'un autre fichier du module en build unity.

Ce contrôle trouve un cas latent du même type : `ProfileNames`, présent dans `BRAutoTest.cpp` et `BRPlayerController.cpp`. Il est corrigé.

## Environnement : Unreal toujours indisponible

| Élément | Constat |
|---|---|
| Machine | Conteneur Linux, 4 cœurs, 15 Go de RAM, **aucun GPU**. |
| Unreal Engine | **Absent.** |
| Possible ici | Blender 5.0 (module Python), clang (syntaxe avec stubs), Python. |
| Non vérifiable ici | Compilation, import, shaders, PIE et Standalone, paquet, captures moteur et mesures d'images par seconde. Ces points sont marqués **non vérifiés** dans le rapport, avec les commandes à lancer. |

## 1. Combinaison du joueur sombre et sans texture (capture du joueur, Poolrooms, 3e personne)

| Constat | Où |
|---|---|
| **Cause principale.** Les matériaux maîtres ne déclarent que l'usage « instances » :<br>• `new_material()` ne règle que `used_with_instanced_static_meshes` ;<br>• `BRMaterialBuilder::Build` ne règle que `MATUSAGE_InstancedStaticMeshes`.<br>Or `M_BR_Mesh` et `M_BR_Skin` habillent aussi des maillages à squelette : `SK_Hazmat` depuis la v4.4, les entités `SK_*` depuis la v4.5. Ils habillent aussi des maillages Nanite (modèles fournis).<br>Selon le mode de lancement :<br>• éditeur (PIE) : le moteur ajoute l'usage tout seul, recompile, et affiche le matériau par défaut en attendant ;<br>• « Standalone Game » (`-game`) et paquet : rien ne peut être recompilé, donc **matériau par défaut (gris, sans texture), sombre sous l'éclairage des Poolrooms**. | `backrooms_setup.py:617`, `BRMaterialBuilder.cpp:731` |
| Aucun contrôle de version ne remarque l'absence de ces usages. `UBRAssets::Parent()` vérifie un paramètre (`RoughContrast`), pas les usages. Le marqueur d'installation est écrit sans rien valider. | `BRAssets.cpp:537`, `backrooms_setup.py:1129` |
| `ApplySlots` nomme `Body` un slot sans nom. Le style `Body` (presque noir, 0,008) cache alors l'erreur au lieu de la montrer. Aucune trace ne dit quel matériau a reçu chaque section. | `BRAssets.cpp:1056` |
| Les noms de slots de `SK_Hazmat.fbx` sont propres (lus dans le FBX avec Blender) :<br>• `HazmatMask` : 47 930 faces, UV `UVMap` ;<br>• `HazmatGlass` : 580 faces ;<br>• `HazmatSuit` : 35 922 faces.<br>Les textures `T_Hazmat_Suit.jpg` et `T_Hazmat_Mask.jpg` (4096², sRGB, moyenne jaune 190/184/124) sont saines.<br>Le nom réel après l'import Interchange en 5.8 n'est **pas vérifiable ici** : le diagnostic par section le donnera. | `RawAssets/Skeletal/SK_Hazmat.fbx` |

## 2. Fluidité (RTX 4080 + 7800X3D, qualité maximale)

| Constat | Où |
|---|---|
| `ApplyGraphicsProfile(2)` (Cinématique) cumule :<br>• `scalability 4` ;<br>• Lumen matériel avec *hit lighting* ;<br>• ombres ray tracées de la lampe ;<br>• brouillard volumétrique, lumières rectangulaires, rendu à 100 %.<br>Il n'existe aucun profil « RTX » intermédiaire, et le TSR n'est pas réglable. | `BRPlayerController.cpp:2709` |
| `UpdateStreaming` appelle `SpawnChunk` → `BeginBuild` : **toute la planification d'un chunk se fait d'un bloc**. Le budget (3 ms) n'est vérifié qu'entre deux chunks. | `BRWorld.cpp:1265`, `BRChunk.cpp:968` |
| `StepBuild` → `CreateBatch` crée **un lot entier** (toutes les instances, toutes les collisions) d'un coup. La dernière étape crée **tous les acteurs** (objets, sorties) d'un coup. | `BRChunk.cpp:1276`, `2198` |
| `StepChunkBuilds` termine sans budget les collisions des 3×3 chunks autour de **chaque** joueur. À quatre joueurs éloignés, cela fait jusqu'à 36 chunks forcés dans une même image.<br>Le tri des chunks en attente ne regarde que le premier joueur. | `BRWorld.cpp:1339` |
| Les chunks trop loin sont **tous détruits dans la même image** (composants, collisions, lumières, acteurs). | `BRWorld.cpp:1290` |
| `UBRAssets::LoadAsset` charge en synchrone (`StaticLoadObject`) au premier besoin, pendant le jeu. Aucun préchargement n'est fait : entités du niveau, sons, textures des modèles. | `BRAssets.cpp:186` |
| `LoadRawTexture` est un secours sans import : mips calculés sur le CPU, `NeverStream`. **Rien ne l'empêche dans un jeu empaqueté.** | `BRAssets.cpp:226` |
| Le type de lumière (rectangle ou point) est décidé une fois, dans `AddLight`. Changer de profil ne touche pas les chunks déjà construits. | `BRChunk.cpp:737` |
| Hound complet ou allégé : décidé à la création, avec `GraphicsProfile == 2`. Le profil Personnalisé n'a donc jamais le modèle complet, et un changement de profil ne touche pas les Hounds présents. | `BREntity.cpp:557` |
| Néons mobiles à ombres : il n'existe ni distance d'ombre ni limite. | `BRChunk.cpp:941` |
| Les maillages à squelette sont toujours dans la scène ray tracée (`r.RayTracing.Geometry.SkeletalMeshes=1`). Sans *hit lighting*, le cache de surfaces de Lumen ne les éclaire pas : **entités noires dans les reflets** (Qualité). | `DefaultEngine.ini` |
| L'eau translucide est dans la scène ray tracée : noire dans les reflets d'autres surfaces. | `BRChunk.cpp:135` |
| Aucun réglage de précompilation des PSO, et aucune préparation des shaders au chargement. | `DefaultEngine.ini` |

## 3. Langues

| Constat | Où |
|---|---|
| **Toute l'interface est en français codé en dur** : environ 620 chaînes visibles, 32 000 caractères, en `TEXT()`. Elles sont assemblées par `FString::Printf`, sans `FText`, `LOCTEXT` ni table de chaînes. | `BRHUD.cpp` (208), `BRPlayerController.cpp` (140), `BRLevels.cpp` (101), `BREntity.cpp` (43)… |
| Les notes trouvées sont enregistrées **par leur texte français** (`TArray<FString> Notes`). Le texte d'une note est porté par `ABRPickup::NoteText`. | `BRSave.h`, `BRCharacter.cpp:1447`, `BRChunk.cpp:1940` |
| En coop :<br>• le nom de l'entité qui frappe circule en texte (`ClientReceiveAttack(..., FString SourceName)`) ;<br>• le nom du sauveteur circule dans `FBRDeathState::By`. | `BRCharacter.h:291` |
| `TextSpaced` dessine **caractère par caractère**, ce qui casse les liaisons de l'arabe et du persan et les graphèmes composés.<br>`Wrap` et `WrapF` coupent seulement aux espaces, ce qui ne coupe jamais une ligne de chinois ou de japonais.<br>`VLabel` empile les caractères un à un. | `BRHUD.cpp:494`, `313`, `540`, `383` |
| Police : Roboto du moteur, qui couvre le latin et le cyrillique. Elle n'a ni CJK ni arabe, et ne fait aucune composition par culture. | `BRHUD.cpp:115` |
| Paquet : aucune culture n'est déclarée, il n'y a aucun `.locres`, et les données ICU sont celles par défaut. | `DefaultGame.ini` |

## 4. Bogues

| # | Constat | Où |
|---|---|---|
| B1 | `Migrate` lit un fichier d'un **format futur** « tel quel ». Le `Snapshot` suivant force `Version = 2` : un jeu ancien réécrit et abîme la sauvegarde d'un jeu plus récent. | `BRSave.cpp:139`, `116` |
| B2 | `ProcessQueue` écrase `LastOk` à chaque tâche. Un échec suivi d'une réussite (autre emplacement) disparaît : **aucun message au joueur**, et ni l'emplacement ni la demande ne sont connus. | `BRSave.cpp:135` |
| B3 | `ServerReportRespawn` avertit sous 2 s, mais accepte tout, quelle que soit la cause. Un client modifié peut se relever aussitôt d'une mort relevable, alors que le délai de réanimation n'est pas écoulé. | `BRCharacter.cpp:1112` |
| B4 | `ReceiveAttack` (serveur) envoie le coup au client **sans toucher la santé**, que seul le client tient. Le serveur ne sait pas si le coup a tué ; un client modifié ignore les coups. | `BRCharacter.cpp:928` |
| B5 | Marqueur d'installation écrit si les matériaux existent, **sans valider** :<br>• textures et maillages à squelette ;<br>• slots et usages ;<br>• compilation.<br>Aucune réparation ciblée n'existe : seul `run(force=True)` répare, et il réimporte tout. | `backrooms_setup.py:1129` |
| B6 | Build unity : `ProfileNames` (locale de `BRAutoTest.cpp`) est homonyme d'une globale de `BRPlayerController.cpp`. C'est la même famille d'erreur que `Ink` (C4459). | `BRAutoTest.cpp:988` |
| B7 | `ApplySlots` : un slot sans nom reçoit le style `Body`, presque noir (voir §1). | `BRAssets.cpp:1056` |

La suite de l'audit (déclencheur, avant, après, fichiers, vérification) est tenue dans le tableau des bogues de
`RAPPORT_v4.8.md`.
