# Relecture du code de la branche `v417-jeu` (v4.17)

| | |
|---|---|
| **Rôle** | « Code Reviewer », définition installée `C:\Users\Fristick\.claude\agents\engineering-code-reviewer.md`, lue en entier. Lancé comme sous-agent générique par l'orchestrateur. |
| **Périmètre** | `git diff 1e67bb1..v417-jeu -- Backrooms/Source Backrooms/Content/Python Backrooms/Tools`, commits lus un par un. La branche a avancé pendant la relecture (`7c50c7a`, puis `d8fa05d`, puis `93c7fb6`). **Tête relue : `93c7fb6`.** Les numéros de ligne renvoient à `93c7fb6`. |
| **Méthode** | Lecture des diffs et des fonctions touchées en entier. Contrôle des API dans le moteur installé (`UE_5.8/Engine/Source`, `Engine/Shaders/Private/Common.ush`, `PythonScriptPlugin`). Scripts de contrôle sans compilation : comparaison des chaînes HLSL C++ / Python, recherche heuristique des noms masqués (C4456 à C4459), relevé des clés de traduction ajoutées ou modifiées. |
| **Non fait** | Aucune compilation, aucun lancement d'Unreal, de Blender ni des bancs, à la demande de l'orchestrateur. Les points « certain » le sont à la lecture du code. Ceux qui portent sur l'image ou le ressenti sont marqués « à vérifier en jeu ». |

## 1. Résumé

L'ensemble est soigné. Les points suivants sont sains :
- les tables indexées par enum sont maintenant protégées par des `static_assert` ;
- les replis de matériau sont signalés au lieu d'être silencieux ;
- les échantillonneurs sont gardés comme dans `ApplyRoughMap` ;
- le HLSL est strictement identique entre C++ et Python ;
- la manette dans PARAMÈTRES passe par le même `AdjustSetting` qu'un clic.

Je n'ai trouvé **aucune erreur de compilation certaine**. Toutes les API vérifiées existent en 5.8, et la recherche de noms masqués ne donne rien. Elle reste heuristique : seule une vraie compilation MSVC la confirme.

Il y a **trois problèmes à traiter avant la cuisson et la traversée du chapitre** :

1. **Bloquant.** Les cartes dérivées de la combinaison (`T_Hazmat_Suit_N`, `T_Hazmat_Suit_ORM`) et leur outil ne sont pas dans la branche. Elles n'existent que non suivies dans `wt/suit`. Conséquences :
   - le jeu signale trois problèmes de matériau à chaque lancement ;
   - `check_package.py` échouera (ressources essentielles absentes) ;
   - la combinaison reste sans relief.
2. **À corriger.** Six textes nouveaux ou modifiés ne sont pas dans les catalogues de traduction. Ils s'afficheront en français dans les 21 autres langues.
3. **À corriger.** Le stick droit et LT / RT ne répondent pas quand l'inventaire est ouvert depuis la pause en solo, car le monde est arrêté. Le pied de PARAMÈTRES annonce pourtant LT / RT.

## 2. Constats

Gravités : **bloquant** (empêche une livraison correcte), **à corriger** (défaut réel, contournable), **remarque** (amélioration ou risque à surveiller).

### 2.1 Bloquant

**B1. Cartes dérivées de la combinaison absentes de la branche** (certain)

- **Où** :
  - `Source/Backrooms/Private/BRAssets.cpp:171`, `:173` ;
  - `Tools/Build/essential_packages.txt:18-19` ;
  - `Content/Python/backrooms_setup.py:113-114`, `:1505-1508`, `:1964`.
- **Constat** :
  - Les styles `HazmatSuit` et `HazmatGlass` déclarent `T_Hazmat_Suit_N` et `T_Hazmat_Suit_ORM`.
  - `RawAssets/Derived/Textures` n'existe pas dans `v417-jeu`. `Tools/derive_hazmat_maps.py` et `Tools/Blender/export_hazmat_uv.py` non plus.
  - Ces fichiers existent seulement **non suivis** dans `wt/suit` (branche `v417-suit`, jamais validée). `git status` y montre `?? Backrooms/RawAssets/Derived/`.
- **Effets** :
  1. `SlotMaterial` rapporte trois problèmes à chaque lancement (`BRAssets.cpp:2057`, `:2072`) :
     - `HazmatSuit : T_Hazmat_Suit_N absente` ;
     - `HazmatSuit : T_Hazmat_Suit_ORM absente` ;
     - `HazmatGlass : T_Hazmat_Suit_ORM absente`.

     Ils apparaissent dans le bandeau magenta du mode développeur (`BRHUD.cpp:3417`) et en problèmes dans l'étape « combinaison » du banc v4.8 (`BRAutoTestV48.cpp:288-291`), donc aussi dans le test de fumée strict du paquet.
  2. `check_package.py:613-615` échoue : « ressource essentielle absente : /Game/Backrooms/Textures/T_Hazmat_Suit_N » (et `_ORM`).
  3. À l'image, la combinaison garde `NormalStrength = 0` et une rugosité constante de 0,38 (valeurs neutres). Le gain annoncé par `5e3d710` (relief, coutures, ORM) n'existe pas dans ce paquet.
  4. `validate()` saute ces cartes en silence quand leur source manque (`backrooms_setup.py:1506-1507`). L'import se déclare donc « OK » alors que le C++ les attend.
- **Correction proposée** :
  - Valider dans `v417-jeu`, depuis `wt/suit`, les fichiers suivants (après la relecture visuelle prévue) :
    - `RawAssets/Derived/Textures/T_Hazmat_Suit_N.png`, `T_Hazmat_Suit_ORM.png`, `T_Hazmat_Suit_maps.json` ;
    - `RawAssets/Derived/Hazmat/*` ;
    - `Tools/derive_hazmat_maps.py`, `Tools/Blender/export_hazmat_uv.py` ;
    - l'inscription des dérivés dans `RawAssets/protected_assets.json`, si le registre doit les suivre.
  - Si l'intégration est repoussée : retirer `Normal` et `Orm` des deux styles (`BRAssets.cpp:171`, `:173`) et les lignes 18-19 de `essential_packages.txt`.
  - Dans `validate()`, une source absente pour un nom de `GEAR_TEXTURES` doit devenir un problème (« source de la carte dérivée absente »), puisque `BRAssets.cpp` la déclare sans condition.

### 2.2 À corriger

**C1. Textes nouveaux ou modifiés absents des catalogues : invariant des 22 langues** (certain)

- **Où** :
  - `BRPlayerController.cpp:3803` : `Menu.LignesBalayageLegereAberrationSalete`, texte source modifié ;
  - `BRPlayerController.cpp:3806` : `Menu.GrainCapteurOmbres`, nouvelle clé ;
  - `BRHUD.cpp:4460` : `HUD.PadCarryFooter`, nouvelle clé ;
  - `BRHUD.cpp:4463` et `:5452` : `HUD.PadCarryHint`, nouvelle clé (même texte aux deux endroits) ;
  - `BRHUD.cpp:4469` : `HUD.PadSettingsFooter`, nouvelle clé ;
  - `BRHUD.cpp:4484` : `HUD.PadInventoryFooterNoScroll`, nouvelle clé.
- **Constat** :
  - Aucun `Content/Localization/Game/*/Game.po` ni `Game.locres` n'est modifié entre `1e67bb1` et `93c7fb6`. Le `de/Game.po` contient encore l'ancien msgid de l'aide VHS.
  - À l'exécution, Unreal compare l'empreinte du texte source : un texte modifié perd sa traduction et une clé absente garde le français. Les 21 autres langues afficheront donc ces six textes en français.
- **Matériau prêt mais non appliqué** :
  - `Tools/Localization/v417/image_nette.json` (22 cultures, 2 clés) ;
  - `Docs/v417/agents/interface_cles.json` (4 clés).
- **Second défaut** : `interface_cles.json` est une liste d'objets `{cle, espace, ou, termes, fr, en, ...}`. `loc_fill.py` attend un dictionnaire `{"<cle>": {"<culture>": "..."}}` : passé tel quel, il signalerait « culture inconnue ».
- **Correction proposée** :
  1. `loc_extract.py` ;
  2. `loc_fill.py Tools/Localization/v417/image_nette.json <interface converti>` (convertir d'abord la liste en dictionnaire) ;
  3. `loc_build.py` ;
  4. valider les `.po` et `.locres` des 22 cultures et `coverage.json`.

  Les traductions restent marquées « fuzzy », à relire par des locuteurs.

**C2. Stick droit et LT / RT inactifs dans l'inventaire ouvert depuis la pause** (certain dans le code, effet à vérifier en jeu)

- **Où** :
  - `BRPlayerController.cpp:476` : `IA_LookPad` ;
  - `BRPlayerController.cpp:504-505` : `IA_LidarPulse` et `IA_LidarFocus` ;
  - `BRPlayerController.cpp:1463` : `SetPause(bPauseMenu)` ;
  - `BRHUD.cpp:4469` : pied de PARAMÈTRES.
- **Constat** :
  - Ces trois actions sont créées sans `bTriggerWhenPaused` (valeur par défaut `false`, `BRPlayerController.h:360`).
  - En solo, `TogglePause` arrête le monde, et l'inventaire peut s'ouvrir depuis la pause (`bInventory` n'est remis à faux qu'en sortant de la pause, `:1447-1450`). Enhanced Input ne déclenche alors pas ces actions.
  - Effets :
    - `OnLookPad` (`:1235-1246`) ne fait pas défiler ;
    - `PadSettingsTrigger` (`:1545-1554`), appelé depuis `OnLidarPulse` et `OnLidarFocusStarted`, ne change pas de catégorie ;
    - le pied annonce pourtant « [LT] [RT] CATÉGORIE ».
  - La croix (actions Menu*), A, Y et B sont, elles, actives en pause.
- **Correction proposée** : passer `true` comme troisième argument de `MakeAction` pour ces trois actions. Les gestionnaires de jeu sont déjà gardés :
  - `OnLookPad` : `if (!CanPlay()) return;` (`:1249`) ;
  - `OnLidarPulse` et `OnLidarFocusStarted` : `if (CanPlay())` ;
  - `OnLidarFocusCompleted` ne fait qu'arrêter le scan.

  À vérifier en jeu : pause solo, puis inventaire, puis PARAMÈTRES, puis LT / RT et stick droit.

**C3. Carte de rugosité du papier peint non régénérée** (certain ; effet à vérifier à l'image)

- **Où** :
  - `Tools/generate_textures.py` (commit `0d87534`) ;
  - `Tools/generate_roughness.py:99-110` ;
  - `backrooms_setup.py:63` ;
  - `Tools/Blender/level0_kit_textures.py:150-157`.
- **Constat** :
  - `T_L0_Wallpaper.jpg` et `_N.png` ont été refaits (joint de 1,8 px au lieu de 0,7, contraste des bandes réduit de 30 %), ainsi que les trois cartes du kit.
  - `T_L0_Wallpaper_R.png`, que `r_l0_wallpaper` dérive de la luminance de `T_L0_Wallpaper`, n'a pas été régénérée.
  - Le kit (`project_material` → `load_rough`) a donc été recalculé avec l'ancienne rugosité. L'encre et les joints de la rugosité ne coïncident plus exactement avec la couleur.
- **Correction proposée** :
  1. régénérer la rugosité du papier peint (`generate_roughness.py`), puis les textures du kit ;
  2. ajouter `"T_L0_Wallpaper_R.png"` à `RETEXTURED[10]`.

  `VERSION = 14` réimporte déjà tout une fois.

### 2.3 Remarques

**R1. Le commentaire de `VERSION = 14` annonce des FBX du kit chanfreinés absents de la branche** (certain)

- **Où** : `backrooms_setup.py:42-44`, message de `7c50c7a`.
- **Constat** : aucun fichier de `RawAssets/Level0Kit/FBX` ne change dans la plage relue.
- **Risque** : si les FBX chanfreinés arrivent **après** un premier import en version 14, ils ne seront pas réimportés. `import_level0_kit(force=False)` s'arrête dès que tous les modules existent (`backrooms_setup.py:1891`).
- **Proposition** : fusionner les FBX avant le premier import, ou passer `VERSION` à 15 avec eux. Corriger le commentaire sinon.

**R2. Références de commentaires vers des fichiers absents de la branche** (certain)

- **Fichiers absents** :
  - `Docs/v417/agents/materiaux_combinaison.md` (cité dans `BRMaterialBuilder.cpp:547` et `BRAssets.cpp:167`) ;
  - `Docs/v417/agents/direction_artistique.md` (cité dans `generate_textures.py:185` ; `BRLevels.cpp:69` renvoie au dossier `Docs/v417/agents/`) ;
  - `Tools/derive_hazmat_maps.py` (cité dans `backrooms_setup.py:111`).
- **Proposition** : les valider avec B1, ou corriger les renvois. C'est une question de traçabilité pour `AGENTS_v4.17.md`.

**R3. Image nette : il reste un vignettage et des lignes de balayage sans l'effet VHS** (certain)

- **Où** :
  - `BRCharacter.cpp:3576` : vignettage de base × 0,6 sans VHS ;
  - `BRHUD.cpp:4323` (inventaire) et `:4087` (pause) : `Scanlines` sans condition ;
  - l'écran d'accueil, lui, teste `bVHSEffect` (`BRHUD.cpp:1784`).
- **Constat** :
  - La nouvelle aide dit : « Désactivé : image nette, sans effet d'objectif. »
  - L'aberration de base, la saleté et les reflets sont bien coupés (`:3558`, `:3622`, `:3628`). Le grain est entièrement soumis à `bFilmGrain` (`:3569`), conformément à son aide.
- **Proposition** :
  - soit `(bVHS ? 1.f : 0.f)` sur la part de base du vignettage, en gardant les apports d'état (cachette, santé mentale, coup) ;
  - soit « léger vignettage conservé » dans l'aide.

  Les lignes de l'inventaire et de la pause sont du style d'interface et existaient déjà. Les soumettre au réglage, ou le documenter.

**R4. Repli du maître `M_BR_Gear` différent entre Python et C++** (certain, voulu)

- **Où** : `backrooms_setup.py:1107-1109`, `BRMaterialBuilder.cpp:573-585`.
- **Constat** :
  - Sans `T_NoiseLF` linéaire, Python refuse de construire le maître et le supprime. Le C++ construit un graphe sans `MaskTex`, où les poids restent reliés.
  - Dans le paquet, l'absence de `M_BR_Gear` retombe sur `M_BR_Mesh` avec un problème signalé (`BRAssets.cpp:1482-1489`).
  - **Dans le cas nominal, les deux graphes sont identiques, nœud par nœud** :
    - `TexScale`, `BaseTex`, `Tint` ;
    - `Dot` de luminance (0,2126 ; 0,7152 ; 0,0722), `Lerp(gris, albédo, Saturation)` puis teinte ;
    - `Roughness` 0,38, `Metallic` 0, `MaskTex` ;
    - `Lerp(const, G, RoughMapWeight)`, `Lerp(const, B, MetalMapWeight)`, `Lerp(1, R, CavityWeight)` ;
    - `Specular` 0,5 × `Lerp(1, AO, 0,5)` ;
    - `NormalTex`, `NormalStrength` 0, trois `RTSwitch`, `SelfIllum`, `Emissive`.
  - Seule différence : la normale par défaut, `T_FlatNormal` en Python contre `DefaultNormal` du moteur en C++. C'est déjà la convention du monde et de la peau.
  - Tous les noms appliqués par `SlotMaterial` existent dans les deux graphes. Les types de broches sont compatibles : `Lerp` scalaire / float3 promu, `Dot` float3 · float3, `Mask` d'une sortie RGB, `AmbientOcclusion` et `Specular` scalaires.
  - Les usages sont les mêmes des deux côtés (instances, squelette, Nanite).

**R5. Un `M_BR_World` cuit en version 9 n'est pas détectable dans le paquet** (certain ; risque faible)

- **Où** : `BRAssets.cpp:1432` (repère de version `Cavity` pour `M_BR_World`).
- **Constat** :
  - Le changement des caustiques (`NZ`, `Texture2DSampleBias`) n'ajoute aucun paramètre. Un `M_BR_World` cuit sans réimport passerait le contrôle avec les anciennes caustiques.
  - La défense actuelle est `MATERIAL_VERSION = 10`, avec la reconstruction à l'ouverture de l'éditeur et par `run_editor_setup.ps1`.
- **Proposition** : avant la cuisson du commit final, vérifier `material_version=10` dans `Saved/BackroomsSetup_Result.txt`. Facultatif : un paramètre repère dans les deux graphes, par exemple un scalaire `CausticsNZ` = 1 non utilisé, et `V2Params[0]` mis à jour.

**R6. Croix sans répétition et pied sans « COLONNE »** (à vérifier en jeu)

- **Où** : `BRPlayerController.cpp:805-815`, liaisons Menu* en `ETriggerEvent::Started` ; `interface.md` § 9.6, points 2 et 4.
- **Constat** :
  - Un appui fait défiler d'une ligne (26 px à 1080 lignes, contre 60 avant). Un long carnet demande beaucoup d'appuis quand le stick n'est pas disponible (C2).
  - Dans CARNET et JOURNAL, gauche / droite choisit désormais la colonne (cadre jaune) au lieu de changer d'onglet, mais aucun pied ne le dit (`HUD.PadColumn` non créée).
- **Proposition** : ajouter une répétition (minuteur, 8 pas par seconde après 0,35 s) et la mention de colonne au pied, avec sa traduction.

**R7. Sens du stick droit dans l'inventaire** (à vérifier avec une vraie manette)

- **Où** : `BRPlayerController.cpp:1245`.
- **Constat** : `-V.Y * 900 * Dt` fait remonter le contenu quand le stick monte, comme l'annonce le commentaire. C'est l'inverse du `V.Y` proposé dans `interface.md` § 9.6.
- **Proposition** : choisir à l'essai. Rien n'est branché sur l'inversion de l'axe Y du joueur, ce qui est correct pour un défilement.

**R8. Plancher typographique et hauteur de ligne pour les écritures denses** (à vérifier en jeu)

- **Où** : `BRHUD.cpp:231-262` (`DenseScript`, `FloorPt`, `UiFontInfo`), `:887-892` (`LineStep`).
- **Constat** :
  - Le plancher de 7 ou 8 points s'applique à tout le texte du Canvas, y compris le HUD de jeu (LiDAR, barre rapide, messages).
  - `LineStep` mesure « Hg » dans la police latine, donc pas la hauteur des polices de secours (CJK, arabe).
- **Proposition** : relancer `-BRAutoTestV416Visual` et `-BRAutoTestV417Visual` en japonais, chinois et arabe, à 1280 × 720 et taille 1,25, en surveillant les chevauchements et les textes hors du cadre.

**R9. Visière sur l'atlas de la combinaison** (à vérifier à l'image)

- **Où** : `BRAssets.cpp:173`.
- **Constat** : `HazmatGlass` prend `T_Hazmat_Suit` comme `BaseTex` (avant : `T_Grime`), teinté à environ 0,017. La couleur dépend des UV de la visière dans l'atlas, ce qui est cohérent avec les zones `glass_center` / `glass_edge` de la carte ORM dérivée.
- **Vérifier** :
  - aucun motif de l'atlas visible sous une lumière rasante ;
  - le reflet diélectrique (`Specular` 0,6, soit environ 4,8 %) ;
  - le rendu sans la carte ORM (rugosité constante 0,06) tant que B1 n'est pas réglé.

**R10. `Frame` passe en tôle peinte sur `T_Paper`** (à vérifier)

- **Où** : `BRAssets.cpp:97`, `:217-233` (`FindSlotStyle` fait une seconde passe par `Contains`).
- **Constat** : dans `generate_models.py`, seul le plafonnier à dalle (`:318-325`) utilise `Frame`. Mais tout autre slot fourni dont le nom contient « Frame » et n'a pas de style exact prendrait désormais le papier sans métal.
- **Vérifier** : la liste des slots des modèles fournis (rapport `DescribeSections`). Juger le grain du papier sur le cadre à l'image.

**R11. `run_editor_setup.ps1` affecte la variable automatique `$args`** (certain, sans effet aujourd'hui)

- **Où** : `Tools/Build/run_editor_setup.ps1:27`.
- **Proposition** : renommer en `$editorArgs`. PSScriptAnalyzer signale `PSAvoidAssignmentToAutomaticVariable`.

## 3. Vérifications positives (ce qui tient)

**Compilation**
- `ULightComponent::SetIndirectLightingIntensity` existe (`Engine/Classes/Components/LightComponent.h:289`), de même que `ULocalLightComponent::SetAttenuationRadius` (`LocalLightComponent.h:53`).
- `Texture2DSampleBias` existe dans `Engine/Shaders/Private/Common.ush:352`, avec le même repli `USE_FORCE_TEXTURE_MIP` que `Texture2DSample`. Le nœud des caustiques reste donc compilable dans les ombrages ray tracés, où il est atteint par l'émissif hors `RTSwitch`.
- Toutes les `UMaterialExpression*` utilisées par `BuildGear` sont déjà incluses.
- Les `switch` sur `EBRMasterMaterial` couvrent `Gear`. Les `static_assert` des tables de noms, des repères de version et des usages correspondent à `EParent::Count` et à l'ordre de l'enum.
- `FHashedMaterialParameterInfo{ FName(...) }` suit le même motif qu'`ApplyRoughMap`. Les ternaires de `TEXT()` passés à `%s` se réduisent bien à `const TCHAR*`.
- `ABRHUD` : les fonctions `const` (`LineStep`, `SettingsRowsOf`) n'appellent que des membres `const` (`TextSize`, `Ui`, `GetSettingsCount`, `GetSettingCategory`). `ABRAutoTest` est bien `friend` d'`ABRHUD` et du contrôleur pour les membres privés lus par le banc.
- Recherche de masquage, par script sur les lignes ajoutées de `BRHUD.cpp`, `BRAssets.cpp`, `BRMaterialBuilder.cpp`, `BRPlayerController.cpp` et `BRAutoTestV416Visual.cpp` : aucune locale ne porte le nom d'un membre (y compris `IX/IY/IW/IH`, `SilX..SilH`, `Tab`, `PadScroll*`) ni d'un nom de l'espace anonyme (`ModalBg`, `TooltipBg`, `PocketKey`, `DenseScript`, `FloorPt`). Les quelques alertes du script étaient des portées sœurs (boucles `for`, lambdas distinctes). Les constantes `UiType::*` sont toutes utilisées.

**Rendu**
- Le HLSL est **identique octet pour octet** entre `BRMaterialBuilder.cpp` et `backrooms_setup.py` pour les huit nœuds Custom : caustiques, triplanaire, ligne de marée, flaques, houle, simulation, réfraction, ombrage de l'eau.
- Les entrées des caustiques sont les mêmes, dans le même ordre : `WP`, `Wz`, `NZ`, `T`, `Amount`, `CausTex`, `SimTex`, `SimWin`.

**Lumière de jeu**
- Lumière de gameplay inchangée :
  - `LightRadius` du Level 0 reste 620 (`BRLevels.cpp:68`) ;
  - `BRWorldLight.cpp:415` et `:705` utilisent toujours `D.LightRadius` et la loi (1 − d/R)² ;
  - `LightVisualRadius` et `LightIndirect` ne touchent que les composants (`BRChunk.cpp:1868-1869`) ;
  - aucune autre lecture du rayon des composants pour le jeu.
- Écart de règle à 560-620 cm : poids inférieur à 0,01, négligeable.

**Contrôleur**
- Ordre des priorités : PARAMÈTRES (`PadSettingsAdjust` / `PadSettingsMove`), puis colonnes (`PadScrollFocus`, seulement avec au moins deux zones), puis ligne (`PadScrollLines`, sauf PERSONNAGE), puis action d'avant (`:809-815`).
- Y : `PadCarry` seulement si la touche est bien Y. A : `PadSettingsActivate`. Les gâchettes sont testées elles-mêmes (le clic droit ne change pas de catégorie).
- Le défilement de la manette est remis à zéro à chaque image (`BRHUD.cpp:4434`). Les fondus ne sont alimentés que depuis `DrawInventory`, sans accumulation.

**Import**
- `VERSION = 14` (`:44`) et `MATERIAL_VERSION = 10` (`:57`).
- Cartes dérivées : normale d'abord, puis ORM, qui référence la normale pour `CTM_NORMAL_ROUGHNESS_TO_GREEN` (`:268-278`, `:1990`). L'enum Python est généré à partir de la propriété `BlueprintReadWrite` `CompositeTextureMode`, comme `TextureGroup`.
- `needs_setup` et `validate` couvrent les dérivés. `editor_setup.py` écrit `RESULT` et quitte l'éditeur.

**Invariants**
- Aucune barre de vie ajoutée.
- Endurance et santé mentale restent dans le panneau ÉTAT de l'inventaire.
- Seules les sources visuelles changent ; les règles de lumière sont intactes.

## 4. À vérifier en jeu (après correction de B1, C1 et C2)

1. **Combinaison**, avec les cartes : joueur proche et loin, coéquipier vu par l'hôte et par un client, lumière neutre puis néons, lampe. Contrôler le relief, les zones sèches et brillantes, la visière, et l'absence de motif de l'atlas.
2. **Poolrooms** :
   - les sols et dessus de trottoir hors de l'eau sont secs ;
   - le liseré des murs s'éteint en 90 cm ;
   - le reflet flou au plafond (`Caustics` 0,35) ne contredit pas le plafond assombri de `17f2e9b`.
3. **Level 0** : rebond à 0,8 et rayon visible de 560 cm, en salle éclairée, en coins, pendant les coupures, à la lampe. Papier peint sous lumière rasante (avec C3).
4. **Image nette** : VHS désactivé et grain désactivé (R3), parasites du Level 6 avec FLASHS réduits (aberration restante × 0,35 et aide statique).
5. **Manette** :
   - pause solo, puis inventaire (C2) ;
   - carnet long, colonnes, répétition (R6) ;
   - PARAMÈTRES : ligne, valeur, catégorie ;
   - déplacement d'objet (Y, A, B), refus en rouge.
6. **Langues** : les six textes de C1 dans les 21 langues, et les écritures denses (R8).
7. **Paquet** : `check_package.py` (ressources essentielles), test de fumée strict sans problème de matériau, `material_version=10` (R5).

## 5. Résumé (12 lignes au plus)

1. Tête relue : `93c7fb6`. Aucune compilation ni aucun lancement n'ont été faits.
2. Aucune erreur de compilation certaine : les API 5.8 sont vérifiées et le contrôle de masquage ne trouve rien.
3. Le HLSL des caustiques et des sept autres nœuds Custom est identique entre C++ et Python.
4. Le graphe `M_BR_Gear` est identique en C++ et en Python dans le cas nominal (noms, types de broches, usages).
5. Lumière de jeu intacte : `LightRadius` et sa loi n'ont pas bougé ; `LightIndirect` et `LightVisualRadius` ne touchent que l'image.
6. **B1 (bloquant)** : `T_Hazmat_Suit_N` et `_ORM` (et leur outil) ne sont pas dans la branche, seulement non suivis dans `wt/suit`.
7. Effets de B1 : trois problèmes de matériau à chaque lancement, échec de `check_package.py` et une combinaison sans relief.
8. **C1** : six textes, dont les aides VHS et grain et quatre pieds de manette, absents des `.po` / `.locres`. Ils resteront en français dans 21 langues.
9. **C2** : stick droit et LT / RT inactifs dans l'inventaire ouvert depuis la pause (actions sans `bTriggerWhenPaused`).
10. **C3** : `T_L0_Wallpaper_R` n'a pas été régénérée après le nouveau papier peint ; le kit en a hérité.
11. Remarques : FBX chanfreinés annoncés mais absents ; vignettage et lignes de balayage sans VHS ; croix sans répétition ; documents cités absents.
12. Après B1, C1 et C2 : vérifier en jeu la combinaison, les Poolrooms, le Level 0, la manette, les langues, puis le paquet.
