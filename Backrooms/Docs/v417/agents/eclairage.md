# v4.17 — Éclairage : Cinématique en mode 2, équilibre du Level 0, 152 paires, essais en mouvement

Agent : rôle « Unreal Systems Engineer » (définition `~/.claude/agents/unreal-systems-engineer.md`), lancé comme sous-agent
générique par l'orchestrateur. Analyse seule, le 21 octobre 2026 : **aucun fichier du dépôt modifié** (ce rapport mis à
part), **Unreal non lancé**. Demande traitée : prompt v4.17, § 4.3 et § 5 ; rapport v4.16, § 2.1 et § 6.6.

Code relu : branche `backrooms-v4.17`, base `c07eddb`. La tête a avancé pendant l'analyse (`1e67bb1`, banc
`-BRAutoTestV417Visual`, plus des modifications non validées d'autres agents) ; aucun des fichiers cités ci-dessous
(BRChunk, BRWorld, BRWorldLight, BRLightLogic.h, BRCharacter, BRPlayerController, BRLevels, BRTypes.h, BRAssets,
BRAutoTestVisual) n'a changé entre `c07eddb` et `1e67bb1` (`git diff --stat`), donc les numéros de ligne valent pour les deux.

Étiquettes :

- **[MESURÉ]** : calculé sur des captures existantes (Python, PIL et numpy) ;
- **[CODE]** : lu dans le dépôt (`fichier:ligne`) ;
- **[MOTEUR]** : lu dans les sources d'Unreal 5.8 installées (`C:\Program Files\Epic Games\UE_5.8\Engine\…`) ;
- **[HYPOTHÈSE]** : à vérifier dans Unreal ; aucune n'est validée ici.

---

## A. Cinématique : mode 2 et mode 1 sur les mêmes poses

### A.1 Données et méthode

- Mode 2 : `Saved/TestRuns/autotest/hlcmpc/visualc` (20 octobre, 01:08). Ligne de commande sans `-BRHitLightingMode` ;
  journal : `r.Lumen.HardwareRayTracing.LightingMode = "2"`.
- Mode 1 : `hlcmpc/visualchl1` (01:12), `-BRHitLightingMode=1` ; journal : `= "1"`.
- **Témoin du bruit** : `hlcmp/visual` et `hlcmp/visualhl1` (00:51 et 01:01). Profils Performance, Qualité et RTX fluide,
  sans hit lighting : `-BRHitLightingMode=1` n'y change rien ([CODE] `BRPlayerController.cpp:4300-4301`). Les deux
  lancements ont donc les mêmes réglages, et leur écart mesure le bruit d'un lancement à l'autre. Comparaison aussi avec
  `final416/visual` (trois profils).
- Captures en 1366 × 768, rendu à 100 % en Cinématique, VHS et grain actifs (sauf `vue_sansvhs`). Caméra alignée
  (corrélation de phase : 0 px de décalage), sauf `coupure` (35 px) et `lampe` (70 px), écartées.
- Masque : bandeau de développement (lignes 0 à 31), bande d'aide (lignes 74 à 105, colonnes 380 à 989), bulle
  d'information (lignes 30 à 59). On passe de sRGB à linéaire, puis on calcule Y = 0,2126 R + 0,7152 G + 0,0722 B.
  **C'est la luminance de l'image affichée, après exposition et tonemapper, pas celle de la scène.**
- Zones : gros plans (`mur_x` et `mur_y` pour les murs, `sol`, `plafond`) ; tiers de l'image pour les vues de couloir
  (haut pour le plafond, milieu pour les murs, bas pour le sol) ; bandes de luminance de l'image du mode 1 floutée en
  9 × 9 (0 à 10 % pour les coins et recoins, 95 à 100 % pour les hautes lumières).

### A.2 Bruit d'un lancement à l'autre [MESURÉ] et sa cause [CODE]

- Poses **stables** : `vue`, `vue_sansvhs`, `dos`, `sol`, `mur_x`, `mur_y`, `fosses`. Entre deux lancements identiques en
  Qualité et en RTX fluide, la moyenne varie de ±0,5 % (PSNR de 39 à 44 dB). En Performance, l'écart reste sous 1,2 %.
- Poses **instables** :
  - `plafond` : ×5,66 en Performance (la dalle visée est éteinte dans un lancement, allumée dans l'autre) et ×0,65 en
    RTX fluide ;
  - `x10` : ×1,11 en Performance ;
  - `mur_smiler` : ×1,18 en RTX fluide ;
  - `coupure`, `lampe` et `retour_*` ne sont pas comparables.
- Causes :
  - `BRAutoTestVisual::CeilingPose` (`BRAutoTestVisual.cpp:179`) écarte les plafonniers cassés, mais pas ceux qui
    clignotent ; la pose `mur_smiler` les écarte (`:242`) ;
  - l'état cosmétique d'un néon qui clignote est tiré au hasard à chaque lancement (`BRChunk.cpp:1797`,
    `FMath::FRand`) ;
  - le nouveau banc a le même défaut : pose `p_contrejour` de `BRAutoTestV417Visual.cpp`, test `!L.bHas || L.bBroken`.
- Exemple d'état transitoire [MESURÉ] : dans `vue` en mode 2, la flaque de lumière du couloir central manque ; on la
  retrouve dans `vue_sansvhs`, même pose prise plus tard. Ce n'est pas un effet du mode.
- **Conséquence** : `plafond`, `x10` et `mur_smiler` servent seulement d'appoint ; les conclusions reposent sur les poses
  stables.

### A.3 Écart du mode 2 par rapport au mode 1 [MESURÉ]

Le tableau donne la luminance linéaire moyenne ; les rapports se lisent mode 2 / mode 1.

| Pose | Mode 1 | Mode 2 | Qualité | m2 / m1 linéaire (sRGB) | m2 / Qualité | Coins (bande 0-10 %) | Hautes lumières (95-100 %) | Haut / bas de l'image |
|---|---|---|---|---|---|---|---|---|
| vue | 0,2246 | 0,2066 | 0,2086 | **0,920** (0,967) | 0,991 | 0,948 | 0,934 | 1,149 / 0,824 |
| vue_sansvhs | 0,2702 | 0,2540 | 0,2539 | **0,940** (0,977) | 1,000 | 0,942 | 0,943 | 1,161 / 0,850 |
| dos | 0,2212 | 0,2062 | 0,2064 | **0,932** (0,972) | 0,999 | 1,108 | 0,944 | 1,130 / 0,848 |
| sol | 0,1910 | 0,1935 | 0,1938 | 1,013 (1,007) | 0,999 | 1,015 | 1,010 | — |
| mur_x | 0,0728 | 0,0931 | 0,0936 | **1,279** (1,132) | 0,994 | 1,315 | 1,255 | — |
| mur_y | 0,2141 | 0,2119 | 0,2120 | 0,990 (0,995) | 1,000 | 1,001 | 0,984 | — |
| fosses | 0,1420 | 0,1472 | 0,1479 | 1,036 (1,019) | 0,995 | 1,054 | 1,025 | 1,054 / 1,027 |
| x10 (instable) | 0,0489 | 0,0597 | 0,0598 | 1,220 (1,215) | 0,997 | 1,503 | 1,019 | 1,948 / 1,155 |
| mur_smiler (instable) | 0,0452 | 0,0548 | 0,0556 | 1,213 (1,131) | 0,987 | 1,294 | 1,063 | 1,258 / 1,278 |
| plafond (instable) | 0,1803 | 0,2177 | 0,2473 | 1,208 (1,144) | 0,880 | 1,457 | 1,014 | — |

Percentiles linéaires (p5 / p50 / p95) :

| Pose | Mode 1 | Mode 2 |
|---|---|---|
| vue | 0,0236 / 0,1861 / 0,5587 | 0,0204 / 0,1856 / 0,5102 |
| mur_x | 0,0169 / 0,0653 / 0,1455 | 0,0225 / 0,0848 / 0,1824 |
| x10 | 0,0022 / 0,0148 / 0,3315 | 0,0030 / 0,0224 / 0,3526 |

Lecture :

1. **Le mode 2 rend la même image que Qualité et RTX fluide** : de 0,987 à 1,000 sur les neuf poses comparables, zones
   comprises (par exemple, haut / milieu / bas de `vue` : 0,210 / 0,220 / 0,195 en mode 2, contre 0,210 / 0,222 / 0,199
   en Qualité). Le Cinématique d'avant (mode 1) était le profil à part.
2. **L'écart se loge dans l'indirect diffus** :
   - mur à l'ombre (`mur_x`) : +28 % en linéaire, sur toutes les bandes ;
   - coins et zones sombres des vues sombres : +30 à +50 % ;
   - plafond des vues claires : +13 à +16 % à l'image, soit environ **+33 à +39 % dans la scène** une fois l'exposition
     retirée (rapport haut / bas : 1,149 / 0,824 dans `vue`, 1,161 / 0,850 dans `vue_sansvhs`) ;
   - surfaces éclairées en direct : quasi inchangées (`sol` +1,3 %, `mur_y` −1,0 %, `fosses` +3,6 %).
3. **Hautes lumières** :
   - elles ne montent pas ; elles baissent légèrement dans les vues claires (bande 95-100 % : 0,93 à 0,94) ;
   - pixels au-dessus de 0,95 : 0,65 % → 0 % (`vue`), 0,88 % → 0,07 % (`vue_sansvhs`), 0,12 % → 0 % (`dos`). Les
     diffuseurs ne brûlent plus ;
   - diffuseur central de `vue` : p50 de 0,957 en mode 1, 0,939 en mode 2 ; aucun pixel à 254/255 dans les deux cas ;
   - rapport diffuseur / dalle voisine : 5,1 en mode 1, 3,4 en mode 2 (comme en Qualité) : moins de contraste autour des
     plafonniers.
4. **Exposition** :
   - vues claires : l'auto-exposition compense le surcroît d'indirect. L'image du mode 2 est 6 à 8 % plus sombre en
     moyenne : sol −15 à −18 % à l'image, soit environ −0,2 IL (approximation par le sol, éclairé surtout en direct) ;
   - vues sombres (`mur_x`, `x10`, `mur_smiler`, à 0,05 à 0,09 en linéaire) : la luminance est bien sous la cible ;
     l'exposition y est vraisemblablement tenue au plancher `MinEV 4.5` dans les deux modes, et l'écart de l'indirect
     passe tel quel à l'image ([HYPOTHÈSE] tant que l'exposition n'est pas journalisée, voir D.0).
5. **Chiffres du rapport v4.16** : leurs « +13 à 23 % » correspondent aux moyennes en sRGB (`mur_x` 1,132,
   `mur_smiler` 1,131, `plafond` 1,144, `x10` 1,215). En linéaire, c'est +21 à +28 % sur ces poses.
6. **Teinte** : dans les vues claires, la part du bleu baisse de 5 % en relatif (0,112 → 0,106 dans `vue`) ; le rebond
   sur le papier peint jaunit un peu l'image. Dans les gros plans, elle ne change pas (±1 %). Ce jaune vient du rebond,
   pas d'un filtre.

### A.4 Ce que change `LightingMode` dans Unreal 5.8

- **[MOTEUR]** `Renderer/Private/Lumen/LumenHardwareRayTracingCommon.cpp:32-40` (texte d'aide paraphrasé) et
  énumération `EHitLightingMode` (`LumenHardwareRayTracingCommon.h:32-39`) :
  - **0** : l'éclairage aux impacts des rayons de Lumen est lu dans le **cache de surfaces**, pour l'indirect comme pour
    les reflets ;
  - **1** : l'éclairage est **calculé à l'impact** (matériau complet et lumières directes, ombres tracées), pour
    l'indirect et les reflets ; le cache ne sert qu'aux rebonds suivants ;
  - **2** : calculé à l'impact **pour les reflets seulement** ; l'indirect et les rebonds, y compris l'indirect vu dans
    les reflets, reviennent au cache de surfaces.
- Avec Lumen en ray tracing matériel, Qualité et RTX fluide sont en mode 0 ([CODE] `BRPlayerController.cpp:4301`). En
  mode 2, l'indirect du Cinématique emprunte donc exactement le chemin de ces profils, ce que A.3 mesure.
- **Rôle des sondes d'écran** : les rayons des sondes d'écran commencent par des traces d'écran, qui reprennent l'image
  précédente et ne dépendent pas du mode. Seuls les rayons qui sortent de l'écran ou passent derrière un objet atteignent
  la géométrie et lisent soit le cache (modes 0 et 2), soit l'éclairage calculé à l'impact (mode 1).
- C'est là que se loge l'écart : plafond (rayons vers le haut, peu couverts par l'écran), revers des piliers, coins et
  murs à l'ombre, c'est-à-dire tout ce qui n'est éclairé que par rebond.
- Le coût mesuré par le rapport v4.16 (`DiffuseIndirectAndAO` 25 ms sur 34 dans les fosses en mode 1) est celui de
  l'évaluation des matériaux et des ombres à chaque impact de l'indirect.

**[HYPOTHÈSE] Pourquoi le cache rend plus de lumière que le calcul à l'impact au Level 0.** L'effet est nul ailleurs :
Niveaux 1, 37 et 6 à 1,00 ± 0,01 ; Niveau 2 à 1,00, sauf `mur_y` (+10,7 %). Causes possibles, par ordre de vraisemblance :

1. **Fuites et filtrage du cache** dans un décor fermé, très clair et dense en murs :
   - résolution des cartes (`r.LumenScene.SurfaceCache.CardTexelDensityScale`) ;
   - sondes de radiosité qui fuient (`r.LumenScene.Radiosity.ProbeOcclusionStrength`, 0,5 par défaut d'après l'aide :
     compromis entre fuite et auto-occlusion) ;
   - projection des texels de carte sur les impacts (`r.Lumen.HardwareRayTracing.SurfaceCacheSampling.DepthBias` de
     10 cm, plus que l'épaisseur du plafond du kit, 5 cm) ;
   - modules du kit Blender, propres au Level 0 ([CODE] `BRChunk.cpp:122-138`, `UsesKit`).
2. **Diffuseurs émissifs** :
   - chaque dalle émet (1 ; 0,96 ; 0,86) × 120 ([CODE] `BRAssets.cpp:1915`), une émission captée dans les cartes à
     basse résolution ;
   - dans le cache, cette émission peut déborder sur les cadres et les dalles voisines ; en mode 1, elle est évaluée au
     pixel.
3. **Albédo capté dans les cartes** différent de celui des matériaux évalués à l'impact (décalques d'usure, `Cavity`,
   `TideLine`) : faible probabilité.

**Expérience décisive, en Cinématique mode 2 et en Qualité** :

- comparer `r.Lumen.Visualize 13` (indirect du cache), `12` (direct du cache), `10` (émissif) et `5` (couverture du
  cache) sur `mur_x` et `x10` ([MOTEUR] `LumenVisualize.cpp:29-56`) ;
- refaire `mur_x` avec `-BRPerfCmds="r.LumenScene.Radiosity.ProbeOcclusionStrength 1"`, puis
  `"r.LumenScene.SurfaceCache.CardTexelDensityScale 2"` ;
- si l'une des deux ramène `mur_x` vers 0,073 (valeur du mode 1), la cause est trouvée et le réglage se règle au cache,
  pour tous les profils.

### A.5 Compensation limitée au Cinématique

**Recommandation (A0) : ne pas compenser le seul Cinématique.**

- Le mode 2 a remis le Cinématique au même niveau que Qualité et RTX fluide (à 1,5 % près).
- Assombrir le seul Cinématique recréerait une différence d'image entre profils, avec des coins et des créatures plus
  ou moins visibles selon la carte graphique.
- La profondeur des coins se règle pour tous les profils au Level 0 (B.3, réglage 1). Le mode 1 sert de **référence de
  niveau pour l'indirect** : c'est le calcul le plus exact (`mur_x` 0,073, haut de `vue` 0,183, p5 de `x10` 0,0022).

**Option A1, si le propriétaire veut retrouver l'aspect du mode 1 en Cinématique seulement :**

| Élément | Valeur | Endroit |
|---|---|---|
| Intensité indirecte de chaque plafonnier | `LC->SetIndirectLightingIntensity(K)`, K = 0,78 (1 / 1,28, rapport de `mur_x`), essais 0,70 / 0,78 / 0,85 | `ABRChunk::CreateLightComponent`, `BRChunk.cpp:1857-1860`, juste après `SetIntensity` ; K = 1 hors Cinématique, ou si `r.Lumen.HardwareRayTracing.LightingMode` ≠ 2 |
| Lumières déjà construites | même valeur au changement de profil | boucle sur `LightRecords`, comme `ABRChunk::ApplyMegaLights` (`BRChunk.cpp:1988-2013`), appelée depuis `ABRWorld::OnGraphicsSettingsChanged` |
| Essai sans recompiler | `-BRCineIndirect=<k>`, hors version publiée, sur le modèle de `BRPerfHitLightingMode` (`BRPlayerController.cpp:93-105`) | `BRPlayerController.cpp` ; exposer la valeur par une fonction statique |
| Biais d'exposition | aucun d'abord ; −0,1 IL seulement si plus de 0,1 % des pixels de `vue` dépassent 0,95 (le mode 1 en avait 0,65 %) | `ABRCharacter::UpdatePostProcess`, `BRCharacter.cpp:3597` |

- Constat [MOTEUR] : l'intensité indirecte de chaque lumière (`GetIndirectLightingScale`) multiplie son apport au cache
  de surfaces (`LumenSceneDirectLighting.cpp:272` et `:2186`) et aux données de lumière du ray tracing
  (`RayTracingLighting.cpp:423`).
- **Effet de bord** : les reflets éclairés à l'impact du Cinématique baissent aussi de 22 % pour les surfaces éclairées
  par ces lampes. Au Level 0, les reflets sont rares : moquette et papier peint ont une rugosité de 0,8 à 0,95, au-delà
  de `MaxRoughnessToTrace` 0,5 ([CODE] `BRPlayerController.cpp:4311`).
- **Ce qui ne marche pas, ou n'est pas à faire** :
  - `FPostProcessSettings::IndirectLightingIntensity` : [MOTEUR] il n'alimente que `View.IndirectLightingColorScale`
    (`SceneRendering.cpp:2048-2061`), lu par l'éclairage précalculé (`BasePassPixelShader.usf:736`), pas par la
    composition de Lumen (`DiffuseIndirectComposite.usf` ne le lit pas). **Sans effet sur Lumen en 5.8** ;
  - revenir au mode 1 : +7 à +9 ms dans les fosses ;
  - un filtre jaune global, ou toucher `SceneTint` et la balance des blancs : exclus par le prompt.
- **Attendu, à mesurer** :
  - `mur_x` de 0,093 à environ 0,075 ;
  - haut de `vue` de 0,21 à environ 0,185 ;
  - sol de `vue` +5 à +10 %, parce que l'exposition remonte comme en mode 1 ;
  - diffuseurs : pas plus de 0,1 % au-dessus de 0,95 ;
  - temps d'image inchangé, puisqu'il s'agit d'un multiplicateur.

---

## B. Équilibre de l'éclairage du Level 0 dans tous les profils

### B.1 Valeurs actuelles [CODE]

| Réglage | Valeur | Où |
|---|---|---|
| Luminaire, tirage | `Panel`, `LightChance` 0,38, `BrokenChance` 0,06, `FlickerChance` 0,07 ; salle de fosses : un néon sur deux cellules, cassé à 12 %, clignotant à 18 % (**règle de jeu, ne pas toucher**) | `BRLevels.cpp:66-67` ; `ABRWorld::CellLight`, `BRWorld.cpp:4789-4897` (fosses `:4859-4883`, tirage `:4889`) |
| Flux, couleur, rayon | `LightLumens` 2400, `LightColor` (1, 1, 1), `LightRadius` 620 cm (**rayon partagé avec la règle de jeu**) | `BRLevels.cpp:68` ; règle : `BRWorldLight.cpp:415` et `:705` |
| Températures | lampes 3700 K, balance des blancs 3900 K (gardées) | `BRLevels.cpp:74` ; `BRChunk.cpp:1862-1866` ; `BRWorld.cpp:1244-1249` |
| Dalle, profils en ray tracing (`bAreaLights`) | lumière rectangulaire de 55 × 115 cm, volets 88° sur 4 cm, flux × 0,55 = 1320 lm, à 5 cm sous le plafond | `BRChunk.cpp:1819-1846` |
| Dalle, Performance (`bAreaLights` faux) | **lumière ponctuelle** de 2400 lm à 25 cm sous le plafond, rayon de source 8, longueur 100 | `BRChunk.cpp:1847-1856` ; profil : `BRPlayerController.cpp:4089` |
| Ombres, distance | MegaLights : toutes les lampes ombrées ; sinon `ShadowChance` 0,15 ; `MaxDrawDistance` 5000 / 3750, fondu 800 | `BRChunk.cpp:1871-1877` |
| Brouillard des lampes | `VolumetricScatter` 0,45 (0 avec MegaLights pendant un vacillement) | `BRLevels.cpp:81` ; `BRChunk.cpp:1939-1952` |
| IES, intensité indirecte, échelle du spéculaire | aucun profil IES ; valeurs par défaut (1) | `BRChunk.cpp:1808-1886` |
| Diffuseur | émissif (1 ; 0,96 ; 0,86) × 120, ramené à la luminance de 3700 K | `BRAssets.cpp:1915` et `:1918-1930` |
| Exposition (post-traitement du niveau) | histogramme, `MinEV` 4,5, `MaxEV` 8, biais 0,3, montée 1,5 IL/s, descente 0,7 IL/s ; percentiles bas et haut par défaut du moteur, 10 / 90 ([MOTEUR] `Scene.cpp:495-496`) | `BRLevels.cpp:84-85` ; `BRWorld.cpp:1230-1243` |
| Exposition (caméra, prioritaire) | biais = `ExposureBias` + luminosité du joueur (+1 en vision nocturne, −3 × `ScareDark`) ; `MinEV` − 6 et vitesse 6 en vision nocturne | `BRCharacter.cpp:3596-3603` |
| Exposition locale (tous niveaux) | contraste des hautes lumières 0,8, des ombres 0,9, détail 1,12 | `BRWorld.cpp:1271-1276` |
| Bloom, vignette, grain | bloom 0,6, `Vignette` 0,5, `Grain` 0,3 ; saleté d'objectif 1 (3 en VHS) | `BRTypes.h:344` ; `BRLevels.cpp:84` ; `BRCharacter.cpp:3604-3616` |
| Lumen (caméra) | qualité des reflets 2, détail de la scène, collecte et éclairage de la scène 1,5 à partir de la qualité Épique, vitesses de mise à jour 2, `DiffuseColorBoost` 1 | `BRCharacter.cpp:3628-3649` |
| Occlusion ambiante | aucun réglage : l'AO à courte portée de Lumen tourne par défaut, en traces d'écran ([MOTEUR] `LumenShortRangeAOHardwareRayTracing.cpp:24-30`, valeur 0) ; `LumenAmbientOcclusionIntensity` à 1, déjà au maximum ([MOTEUR] `Scene.h:1817-1819`) | — |
| Émissif du Smiler compensé par `MinEV` | `EvComp = 2^clamp(MinEV − 3, 0, 3)` | `BREntity.cpp:3186` |

### B.2 Écarts entre profils [MESURÉ]

`final416/visual` et `hlcmp`, poses stables, luminance linéaire.

| Pose | Performance | Qualité | RTX fluide | Cinématique mode 2 |
|---|---|---|---|---|
| vue : haut / milieu / bas | **0,335** / 0,196 / **0,142** | 0,210 / 0,221 / 0,198 | 0,210 / 0,222 / 0,199 | 0,210 / 0,220 / 0,195 |
| dos : haut / bas | **0,323 / 0,120** | 0,195 / 0,177 | 0,194 / 0,177 | 0,195 / 0,176 |
| fosses (moyenne) | **0,087** | 0,148 | 0,148 | 0,147 |
| mur_x (moyenne) | 0,081 | 0,094 | 0,094 | 0,093 |
| Diffuseur central de `vue` : pixels à 254/255 | **13,4 %** (point chaud) | 0 % | 0 % | 0 % |

- **En Performance, la dalle devient une lumière ponctuelle** :
  - [MOTEUR] conversion des lumens : ponctuelle ÷ 4π (`PointLightComponent.cpp:145`), rectangulaire ÷ π
    (`RectLightComponent.cpp:196`) ;
  - au nadir : 2400 lm → 191 cd en ponctuelle, contre 1320 lm → 420 cd pour la dalle ; **le sol reçoit 2,2 fois moins de
    lumière directe** ;
  - la lumière ponctuelle éclaire aussi le plafond, à 25 cm (halo clair au plafond, point chaud dans le diffuseur) ;
  - résultat : plafond de `vue` +60 %, sol −28 %, salle des fosses −41 %, sans doute tenue au plancher `MinEV`. C'est
    l'écart d'image le plus fort entre profils au Level 0.
- **Les trois profils en ray tracing** sont identiques à 1 % près sur ces poses.
- Le Cinématique d'avant (mode 1) avait des coins plus profonds (−22 % sur `mur_x`, −18 % sur `x10`, jusqu'à −33 % dans
  les zones les plus sombres de `x10`), mais des diffuseurs et un sol plus brûlés (exposition +0,2 IL).

### B.3 Réglages proposés (rendu seulement, règle de jeu intacte)

Ordre conseillé : réglages 1 et 2, mesure, puis 3 à 7 un par un. Sauf mention contraire, chaque nouveau champ du niveau
reprend par défaut la valeur actuelle, donc aucun autre niveau ne change.

| # | Réglage | Valeur actuelle → proposée | Fichier, fonction | Effet attendu, critère mesurable |
|---|---|---|---|---|
| 1 | Indirect des plafonniers (tous profils) | 1 → **0,80**, nouveau champ `FBRLevelDef::LightIndirectScale` (Level 0 seulement) | `BRTypes.h` (bloc Éclairage, l. 305-315), `BRLevels.cpp:68`, `ABRChunk::CreateLightComponent` (`BRChunk.cpp:1857`) : `SetIndirectLightingIntensity`. Recréation des types de lumière : passe déjà par cette fonction (`RefreshLightTypes`, `:1900-1937`) | Coins et plafonds vers la référence du mode 1, sans toucher au direct ni aux diffuseurs : `mur_x` (Qualité) 0,094 → 0,076-0,080 ; haut de `vue` 0,21 → 0,185-0,19 ; p5 de `x10` −15 à −20 %. Remplace A1 si le propriétaire le retient pour tous les profils |
| 2 | Dalle en Performance | lumière ponctuelle 2400 lm → **projecteur vers le bas**, cône extérieur 80°, intérieur 35°, mêmes rayon et longueur de source, flux = `Lumens × 0,55 × 2 (1 − cos 80°)` = **2182 lm** (même intensité au nadir que la dalle : 420 cd ; conversion [MOTEUR] `SpotLightComponent.cpp:97`) | `ABRChunk::CreateLightComponent`, branche `!bArea` (`BRChunk.cpp:1847-1856`), pour `IsAreaFixture` (dalle, tube, dalle ovale) | Plus de halo au plafond ni de point chaud ; sol de `vue` 0,142 → ~0,19 ; fosses 0,087 → ~0,14 ; diffuseur sous 1 % à 254/255. Les ombres d'un projecteur coûtent moins que celles d'une ponctuelle (une vue au lieu de six) : à mesurer dans `fosses` et `marche0` en Performance |
| 3 | Rayon d'atténuation **visuel** | 620 → **560 cm** (× 0,9), nouveau champ `LightRadiusVisualScale` ; `D.LightRadius` reste à 620 pour la règle | `BRChunk.cpp:1867` seulement ; **ne pas toucher** `BRWorldLight.cpp:415`, `:705`, `BRAutoTestV416Light.cpp:152`, `:673`, `BRAutoTestVisual.cpp:250` | Recoins à deux cellules d'une lampe plus sombres, moins de lampes par pixel pour MegaLights (bruit et coût en baisse). Alignement avec la règle préservé : à 560 cm, l'apport de la règle (1 − d/R)² vaut 0,009 |
| 4 | Volets de la dalle | 88° / 4 cm → essais 70° / 8 cm et 60° / 8 cm, nouveaux champs du niveau | `BRChunk.cpp:1842-1843` | Haut des murs plus sombre près des lampes, flaque au sol mieux dessinée. Vérifier avec `r.Lumen.Visualize 12` que le cache suit les volets ; sinon le direct et le rebond divergent |
| 5 | Exposition | `MinEV` 4,5 et `MaxEV` 8 **gardés** (noir des coupures, compensation du Smiler `BREntity.cpp:3186`) ; vitesses 1,5 / 0,7 gardées ; biais 0,3 → 0,2 **seulement si** le réglage 1 remonte le sol de `vue` de plus de 5 % ou les diffuseurs au-dessus de 0,95 ; **ajout** d'une courbe de compensation `AutoExposureBiasCurve` au Level 0 : −0,5 IL si la luminance moyenne de la scène est sous EV100 3, −0,3 à 4, −0,1 à 5, 0 à 6 et au-delà | `ABRWorld::ApplyEnvironment` (`BRWorld.cpp:1229-1243`) ; courbe `UCurveFloat` créée à l'exécution, tenue par une `UPROPERTY` du monde. La caméra ne remplace pas ce champ (`BRCharacter.cpp:3596-3603`). [MOTEUR] la courbe est lue sur la luminance moyenne de la scène de l'image en cours, avant exposition et sans historique (`PostProcessEyeAdaptation.cpp:463-490`) : pas de boucle ; l'exposition qui en résulte reste lissée par les vitesses | Vues sombres et coins 0,3 à 0,5 IL plus profonds, pièces éclairées inchangées. Le biais s'ajoute aussi au plancher : `coupure` plus noire, donc juger la lisibilité de `lampe` et des yeux du Smiler |
| 6 | Exposition locale du Level 0 | contraste des hautes lumières 0,8 → **0,7**, des ombres 0,9 → **1,0** ; nouveaux champs du niveau | `BRWorld.cpp:1271-1276` | Barres et cadres des dalles mieux lisibles ; les ombres ne sont plus relevées par l'exposition locale (0,9 les éclaircit un peu), donc plus profondes. Critère : diffuseurs p50 ≤ 0,93 et bande 0-10 % de `vue` −5 % |
| 7 | AO à courte portée de Lumen | traces d'écran → **ray tracing matériel** en Qualité et Cinématique : `r.Lumen.ScreenProbeGather.ShortRangeAO.HardwareRayTracing 1` ; essai `r.Lumen.ScreenProbeGather.ShortRangeAO.MaxMultibounceAlbedo 0.4` (0,5 par défaut : sur un albédo clair comme le papier peint jaune, l'occlusion s'efface) | `ABRPlayerController::ApplySettings`, à côté de `LightingMode` (`BRPlayerController.cpp:4301`) ; cvars [MOTEUR] `LumenShortRangeAOHardwareRayTracing.cpp:24`, `LumenScreenSpaceBentNormal.cpp:56-59` | Angles mur / plafond et mur / plinthe plus marqués, y compris hors écran. Coût à mesurer (ProfileGPU, poste `ShortRangeAO`), avec un plafond de +0,5 ms en Qualité |
| 8 | Diffuseur (lisibilité) | émissif 120 uniforme → **80 à 90**, avec un motif prismatique discret (±8 %) | `BRAssets.cpp:1915` et matériau du diffuseur. **Fichiers partagés** (`BRAssets.cpp`, `BRMaterialBuilder.cpp`) : passer par le responsable désigné | Motif visible ; un peu moins de rebond émissif, ce qui va dans le sens du réglage 1. Critère : p50 du diffuseur ≤ 0,90, sans paraître éteint à côté de la flaque au sol |
| 9 | Bloom du Level 0 | 0,6 → 0,5 (optionnel) | `BRLevels.cpp:84` (`D.Bloom`) | Halo des dalles plus fin, avec le brouillard volumétrique |

**Ne changent pas** :

- le nombre et la place des plafonniers (`LightChance`, `BrokenChance`, `FlickerChance`, `CellLight`) ;
- `LightRadius` pour la règle ;
- la loi (1 − d/R)² et les seuils `NeedsDark` 0,12 et `SmilerVanish` 0,5 (`BRLightLogic.h:36-53`) ;
- la grille et `PhasePower` ;
- `SceneTint`, la balance des blancs et les températures.

Contrôles :

- **Empreintes de la grille égales au banc** (`[Lumiere] … empreinte` dans le journal) ;
- `-BRAutoTestV416Light` : 0 / 28 points différents entre profils ;
- `Tools/Light/test_light.cpp` inchangé et réussi.

### B.4 Risques

- **Pompage de l'exposition** :
  - les cellules éclairées et sombres alternent tous les 3,5 m : à 4,7 m/s, l'exposition (montée 1,5 IL/s) les poursuit ;
  - le réglage 5 peut aider (moins d'écart d'exposition entre une vue sombre et une vue claire), mais une courbe trop
    raide crée des à-coups ;
  - [HYPOTHÈSE] remonter `AutoExposureLowPercent` (10 → 20 à 25) ferait moins peser les coins noirs sur la mesure : à
    essayer après la courbe, pas en même temps.
- **Bruit de MegaLights** :
  - 4 échantillons en Qualité et en Cinématique, 2 en RTX fluide (`BRPlayerController.cpp:4346`) ;
  - des coins plus sombres (réglages 1, 5, 6) rendent le bruit résiduel plus visible quand l'exposition remonte ;
  - le rayon visuel réduit (réglage 3) le diminue ;
  - mesurer le bruit image par image sur caméra fixe (D.1).
- **Coupures** :
  - `MinEV` reste à 4,5 ;
  - le réglage 5 assombrit davantage le noir : la lampe torche (non soumise à MegaLights, `BRCharacter.cpp:124-126`) et
    les yeux du Smiler (émissif 28 / 20) doivent rester lisibles ;
  - pendant le rallumage, l'indirect suit avec `LumenSceneLightingUpdateSpeed` 2 : vérifier que le réglage 1 n'allonge
    pas le temps de retour perçu.
- **Performance** (réglage 2) :
  - change l'aspect du profil le moins cher : plafond plus sombre, comme les autres profils ;
  - le cache de Lumen logiciel suit le nouveau type de lumière ;
  - l'ombre d'un projecteur coûte moins (gain probable sur le poste « ombres virtuelles », 1,05 à 1,34 ms).
- **Réglage 1** : le Cinématique perd aussi un peu de reflets éclairés (A.5), faible au Level 0.
- **Comparaison** : stabiliser d'abord le banc (A.2), sinon les écarts recherchés (−15 à −20 %) se perdent dans le bruit
  des poses instables (×0,65 à ×5,7).

---

## C. Les 152 paires où la physique arrête un rayon que la grille laisse passer

### C.1 Rappel des mesures du rapport v4.16

`final416/v416light`, journal ; « hors grille » désigne les paires que la physique seule arrête.

| Niveau (graine) | Paires | Accord | Hors grille | Composants nommés |
|---|---|---|---|---|
| 0 (9) | 910 | 100 % | 0 | — |
| 0 (4242) | 902 | 99,6 % | 4 | `BRChunk_34.ISM_17` ×4 |
| 1, 2, 5, 37 | 902 / 906 / 200 / 904 | 100 % | 0 | — |
| 4 | 900 | 90,2 % | **88** | `BRChunk_95.ISM_7` ×18, `_6` ×18, `BRChunk_94.ISM_13` ×16, `BRChunk_95.ISM_8` ×14 |
| 8 | 486 | 96,3 % | 18 | `ISM_2` ×6 dans les chunks 136, 138 et 139 |
| 9 | 536 | 92,2 % | 42 | `ISM_1` dans les chunks 164, 165, 166 et 171 |

### C.2 Ce que touche la trace [CODE]

- La trace du test : canal `ECC_WorldStatic`, capsules ignorées (`BRAutoTestV416Light.cpp:163-180`). Le nom regroupé
  est celui du composant, sans numéro d'instance (`:739-741`).
- Les composants de chunk sont créés sans nom, dans l'ordre des lots (`ABRChunk::CreateBatchStep`, `BRChunk.cpp:3588`) :
  - lots à collision d'abord (`NextBatchKey`, `:2531-2541`) ;
  - lots découpés en sous-lots de 16 à 1024 instances selon le budget (`SubBatchSize`, `:2546-2552`) ;
  - le suffixe `_N` est donc l'ordre de création dans le chunk, pas un type.
- Murs, jambages, linteaux, piliers, sol et plafond sont bâtis aux cotes exactes de la grille :
  - murs : `AddWallSegment` (`:768-831`) et `AddDoorway` (`:932-1020`) ;
  - grille : `BRLight::Occlusion::MakeGeom` et `EdgeBlocks`, `BRLightLogic.h:214-245` et `:353-406` ;
  - Niveau 4 : passage de 110 cm, linteau à 225 cm, piliers de 50 ; les points sont à 40 cm au moins des faces des murs
    (`Free = S − T − 80`).
  Ces éléments ne peuvent pas produire de paires « hors grille ».
- **Ce que la grille ne porte pas** : le lot caché `COLLISION`, qui reçoit **la boîte englobante de chaque objet posé
  avec collision** (`ABRChunk::AddProp`, `BRChunk.cpp:321-344`), et quelques boîtes ou modules à collision propre.
  Par niveau :

| Niveau | Composant le plus probable (confiance) | Objets en cause | Lu dans le code |
|---|---|---|---|
| 4 | lot `COLLISION`, sous-lots `_6` à `_8` et `_13` : il vient après le sol, les murs et le plafond (**forte**) | `SM_OfficeDeskET` (boîte de repli 98 × 186 × 76), `SM_OfficeChairET` (60 × 60 × 110), `SM_WaterCoolerET` (45 × 45 × 160), **`SM_Partition` (5 × 120 × 140)**, `SM_Desk`, `SM_OfficeChair`, `SM_WaterCooler` (32 × 32 × 140) | `BuildCellProps` Office, `BRChunk.cpp:3075-3132` ; `AddWorkstation`, `:3186-3210` |
| 9 | lot `COLLISION`, `_1` : premier lot à collision après le sol (**forte**) | `SM_House` : corps de la boîte englobante jusqu'à `GHouseBodyFront` 440 cm et **jusqu'au sommet du modèle** (toit à environ 5,5 m, cheminée à 5,95 m d'après `generate_models.py:658-684`), plus le perron ; la grille ne connaît qu'une boîte de 8 × 7 × **4,5 m** (`BRWorldLight.cpp:326-337`) ; lampadaires à 5,6 m | `BRChunk.cpp:2422`, `:272-274`, `:333-339` |
| 8 | lot `COLLISION`, `_2` : après le sol et les cellules pleines (**forte**) | `SM_Rock` des parois et des cellules : boîte orientée du modèle, bien plus grosse que le rocher visible | `BRChunk.cpp:2385`, `:3157` ; `RockTransform`, `:277-280` |
| 0 (4242) | un module du kit à collision maillée, poteau de sommet ou passage, un peu plus large que les boîtes de la grille (**faible**, non identifié) | 4 paires sur 902 | `AddKitPost`, `BRChunk.cpp:200-264` ; `AddKitModule`, `:140-152` |
| 1 | caisses (`SM_Crate`, jusqu'à 120 cm empilées) et butées de roue (12 cm) : **0 paire mesurée** | — | `BRChunk.cpp:3047`, `:2992` |

### C.3 Ce qui gêne vraiment

1. **Niveau 9, les maisons** :
   - la règle compte comme éclairé un jardin que le toit plonge dans l'ombre (image MegaLights), dans un niveau de nuit
     où les entités (Skin-Stealer, Hound) dépendent de l'ombre ;
   - c'est le défaut de la v4.15 (lumière derrière un mur), en plus petit : 7,8 % des paires du niveau ;
   - c'est le cas le plus gênant, quand le Niveau 9 sortira.
2. **Niveau 4, cloisons d'open space (1,4 m) et fontaines (1,4 à 1,6 m)** :
   - vraies ombres à hauteur du joueur (92 cm) ;
   - au-dessus d'elles, à 165 cm (Smiler), la lumière passe ;
   - le niveau est très éclairé (70 % des cellules ont un plafonnier, le Smiler se dissipe sur 85 % des positions), donc
     l'effet sur les seuils est faible.
   - **Bureaux et chaises** : leur boîte englobante est surtout de l'air (pieds, dossier) ; la boîte de collision
     surestime l'ombre réelle ; pas d'occultant important.
3. **Niveau 8, rochers** : surtout un artefact de la boîte de collision, qui déborde du rocher visible. Le niveau est
   déjà sombre (69 % des positions dans le noir) : effet minime.
4. **Level 0** : 4 paires, aucun effet attendu. Les niveaux du premier chapitre (0, 1, 37, 2) ne sont pas concernés.

### C.4 Traitement ciblé, déterministe, à coût borné

**Étape 0, diagnostic : avant tout changement de règle.**

- Dans le test, garder `FHitResult::Item` (l'indice d'instance de l'ISM touché) et retrouver l'objet par la clé et les
  transformations du lot (par exemple `BRChunk_95 COLLISION#37 = SM_Partition`).
- En dehors de la version publiée, nommer les composants d'après la clé du lot, ou tenir une table composant → clé dans
  le chunk.
- Coût nul en version publiée. Ce relevé tranche les « (forte) » de C.2.

**Étape 1, Niveau 9 : une représentation partagée de la maison.**

- Remplacer la boîte de 4,5 m par deux ou trois boîtes calées sur le modèle :
  - corps de 8 × 7 m jusqu'à 3,3 m ;
  - pignon ou toit en marches : 8,8 × 7,4 m de 3,3 à 4,4 m, puis 8,8 × 3,7 m de 4,4 à 5,5 m.
- Constantes communes dans `BRLightLogic.h`, à côté de `HatchRect`, lues par `ABRWorld::FLightImpl::MakeCell`
  (`BRWorldLight.cpp:326-337`), par le banc `Tools/Light/test_light.cpp` et par `AddProp` pour la collision, au lieu de
  `Bounds.Max.Z` : une seule définition.
- Coût : `SolidBox` renvoie aujourd'hui une boîte par cellule ; il en renverrait deux ou trois (`Hits` par boîte,
  cellules voisines seulement, résultat en cache par nœud de 10 cm). Coût par appel inchangé à l'échelle mesurée
  (0,122 µs en cache, 3,99 µs à vide).
- Déterminisme : la boîte dépend du lot et de la graine (`HasHouse`), comme aujourd'hui.

**Étape 2, Niveau 4 : les grands objets seulement (hauteur ≥ 120 cm, largeur ≥ 30 cm), c'est-à-dire `SM_Partition` et
les fontaines.**

- Extraire les tirages de `BuildCellProps` Office (`R(901)`, `R(902)`, `R(906)`, `R(907)`, `IsCubicle`) dans une fonction
  pure partagée, sur le modèle de `BRMech::Cave::CellRock`, déjà commune au chunk et au banc (`BRChunk.cpp:3149-3151`).
  Elle rend, par cellule, au plus deux boîtes aux **cotes visibles fixées en constantes** (cloison 5 × 120 × 140,
  fontaine 45 × 45 × 160), pas aux bornes du modèle importé.
- `FBRLightCell` reçoit au plus deux boîtes basses :
  - `Visible` les teste dans la cellule parcourue, avec le bloc plein ;
  - mémoire : +48 octets par cellule, soit 64 cellules × 512 chunks au plus ≈ 1,5 Mo dans le pire cas ;
  - chunk en cache inchangé.
- Hôte et clients : même graine, même grille, aucune trace physique.
- L'empreinte (`GetLightGridFingerprint`) intègre ces boîtes, donc le test réseau `fnet_classic` compare l'hôte et le
  client.
- Le banc gagne une scène témoin : point à 92 cm derrière une cloison, caché ; point à 165 cm, visible.

**Hors champ** :

- chaises et bureaux, à garder hors de la grille ;
- rochers du Niveau 8 : pas de traitement. Si le niveau sort, utiliser un noyau du rocher (60 % de la boîte orientée,
  test dans son repère), car `BRMech::Cave::RockRect` dépend des bornes du modèle ;
- Level 0 : rien à faire.

**Contrôles** :

- Level 0 et Niveaux 1, 2 et 37 : **empreintes inchangées** (aucun objet de cette taille) ;
- Niveaux 4 et 9 : nouvelles empreintes, à reporter dans `BRLightPrints.h` ;
- accord grille / physique attendu : Niveau 4 vers 95 %, Niveau 9 vers 97 %. Il ne doit pas atteindre 100 % : le reste
  vient des bureaux, des chaises et des marges de boîtes ;
- le budget « la grille seule cache au plus 3 % » doit rester respecté.

---

## D. Essais en mouvement et mesures

### D.0 Instrumentation commune (hors version publiée, à ajouter au banc)

- **Exposition réelle, image par image** :
  - un `ISceneViewExtension` de test qui lit, dans `SetupView`, `FSceneView::GetLastEyeAdaptationExposure()` et
    `GetLastAverageSceneLuminance()` ([MOTEUR] `SceneView.h:2147-2150`) ;
  - `ULocalPlayer::ViewStates` est privé (`LocalPlayer.h:248`) : passer par la vue ;
  - journal CSV : temps, lacet / tangage, exposition, luminance moyenne de la scène, alimentation des règles
    (`GetGameplayPower`) et de l'image (`Power`), `LightLevelAt` au joueur, profil.
- **Images** : une capture tous les 3 pas fixes de 1/30 s, comme les séquences vidéo du banc v4.17, puis analyse hors du
  jeu avec le script de l'annexe (moyenne, p5 / p50 / p95, bandes, zones) et une vidéo par essai.
- **Stabilité** :
  - néons qui clignotent figés ou tirés avec la graine pendant les bancs (`BRChunk.cpp:1797`) ;
  - poses qui écartent `bFlicker` (`BRAutoTestVisual.cpp:179`, pose `p_contrejour` du banc v4.17) ;
  - coupures au même instant de phase.
- **Coût** : `-BRAutoTestPerf` avec ses relevés ProfileGPU (`-BRPerfGpu=<k>`), `-BRPerfCmds` pour les essais A/B,
  `Tools/Tests/run_perf.ps1 -Runs 3` et `perf_summary.py --by-seed`, deux graines (9 et 4242). Postes à lire :
  `MegaLights`, `Lumen` (sondes d'écran, éclairage du cache : direct et radiosité), `ShortRangeAO`, `VolumetricFog`,
  ombres virtuelles, `TSR`, post-traitement (histogramme de l'exposition).
  - **Ne pas additionner la file asynchrone de Lumen à la file graphique.**
  - Unreal Insights : `-trace=default,gpu` et `trace.bookmark` à chaque étape.
- **À l'œil** : `ShowFlag.VisualizeHDR 1` (histogramme et exposition) ; `r.Lumen.Visualize 5`, `12` et `13` ;
  `r.MegaLights.Debug 1` si besoin.

### D.1 Essais

Chacun en Performance, Qualité, RTX fluide et Cinématique, un profil par lancement. Level 0, graine 9, sauf mention.

| # | Essai | Déroulé | Mesures | Critères proposés |
|---|---|---|---|---|
| 1 | Néon puis mur sombre | Pose `plafond` sur un plafonnier **qui ne clignote pas**, 3 s, puis rotation de 90°/s et de 180°/s vers un mur à l'ombre (type `mur_x`) ; 6 s de maintien ; retour | Exposition ; temps pour atteindre 90 % de l'écart (t90) ; inversions de sens ; dépassement ; diffuseur (p99,5) face au néon ; bruit de MegaLights sur le mur une fois posé (écart-type temporel sur 30 images fixes, par blocs de 8 px) ; `stat unit`, `stat gpu` | Montée : t90 ≤ 2,5 s ; au plus une inversion de plus de 0,05 IL ; dépassement ≤ 0,1 IL ; diffuseur p99,5 < 0,95 ; bruit après 1 s ≤ 1,5 fois celui de la même pose en Qualité |
| 2 | Tourner près d'une porte | Passage du kit : contourner un jambage à 1 m, à 90°/s, lampe d'un seul côté ; puis porte de service fermée, puis ouverte (banc v4.17) | Fuite au jambage et au linteau (luminance du côté sombre contre le reste du mur) ; traînées de MegaLights (images pour que l'ombre se repose) ; essais A/B `r.MegaLights.Temporal.MaxFramesAccumulated 12` / `6` et `r.Lumen.ScreenProbeGather.ShortRangeAO.HardwareRayTracing 0` / `1` ; ProfileGPU | Fuite < 5 % ; ombre reposée en 6 images au plus (0,2 s à 30 img/s) ; poste `ShortRangeAO` : ≤ +0,5 ms en Qualité |
| 3 | Entrer dans une fosse | Salle de fosses de la graine 9 : marche jusqu'au bord, regard vers le bas, chute contrôlée (ou caméra libre) jusqu'à 4 m sous le sol. Ses néons clignotent à 18 % (`BRWorld.cpp:4870`) : les figer pendant l'essai | Exposition (plancher `MinEV` 4,5) ; p50 du fond de la fosse ; dégradé de `PitShade` ; temps d'image (fosses : 25 ms en Cinématique) | Le fond reste noir (p50 < 0,01) ; pas de remontée de l'exposition au-delà de 0,3 IL quand la fosse remplit la moitié de l'image ; haut des parois lisible |
| 4 | Coupure, puis rallumage | Coupure imposée au même instant de phase, marche à 2 m/s pendant la défaillance (1,8 s), le noir et le retour (1,4 s) | `GetGameplayPower` et alimentation de l'image ; exposition ; luminance du sol ; retard de l'indirect (temps pour retrouver 90 % du plafond après une alimentation de 1) ; traînées ; « chunks en rallumage » ; `LightLevelAt` au joueur dans les quatre profils | Règle identique dans tous les profils (0 image différente) et monotone ; sol < 0,005 au bout de 9 s de noir ; indirect à 90 % en 1,5 s au plus après le retour ; pas de traînée au-delà de 0,3 s |
| 5 | Allumer la lampe | Dans le noir de l'essai 4, au bout de 9 s, lampe allumée face à un mur à 1 m, puis à 4 m ; vision nocturne coupée | Exposition (montée de 1,5 IL/s, 4 s) ; tache centrale sur le mur proche (p99) ; bruit (la lampe est hors MegaLights) ; en Cinématique, coût des ombres ray tracées de la lampe | Tache p99 < 0,97 ; reposée en 4 s ; détails du papier peint lisibles dans le halo |
| 6 | Combinaison jaune proche | Troisième personne, ou coéquipier en réseau (`run_nettest.ps1`, deux instances) : combinaison à 1 m sous une dalle, puis à l'ombre, puis en contre-jour ; le joueur tourne autour | Saut d'exposition quand la combinaison remplit 30 % de l'image ; hautes lumières de la combinaison et de la visière (rugosité 0,04, métal 0,7) ; ombre de la combinaison sur le mur (MegaLights, maillage à squelette dans la scène ray tracée : `r.RayTracing.Geometry.SkeletalMeshes`, `BRPlayerController.cpp:4338`) ; reflets selon le profil | Saut < 0,3 IL ; p99 de la combinaison < 0,95 ; ombre présente dans les trois profils en ray tracing ; pas de combinaison noire dans un reflet |
| 7 | Ombres au loin (appoint) | Marche dans le plus long couloir, aller et retour (comme `marche0`) | Distance de disparition des ombres : `MaxDrawDistance` 5000 / 3750 et fondu 800 (`BRChunk.cpp:1876-1877`) ; en Performance, `UpdateShadowLOD` | Pas d'apparition brutale à moins de 25 m ; en Performance, une coupure visible est notée comme limite assumée |

Chaque essai est fait **avant puis après** chaque réglage de B.3, avec le même binaire et le même pilote, deux graines et
trois exécutions. La règle de jeu est contrôlée à la fin : empreintes, `-BRAutoTestV416Light`, banc `test_light`.

---

## Annexe : calcul des mesures (reproductible)

Scripts hors du dépôt (dossier de travail de l'agent) : `analyse.py`, `profils.py`, `planche.py`. Le cœur du calcul :

```python
a = np.asarray(Image.open(p).convert('RGB'), np.float32) / 255
l = np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)        # sRGB -> lineaire
y = 0.2126 * l[..., 0] + 0.7152 * l[..., 1] + 0.0722 * l[..., 2]           # luminance affichee
m = np.ones(y.shape, bool); m[:32] = False; m[74:106, 380:990] = False; m[30:60, 180:1190] = False  # interface
# bandes : percentiles 10 / 95 de l'image du mode 1 floutee 9 x 9 ; rapport des moyennes du mode 2 et du mode 1 sur chaque bande
# zones : tiers 0-35 % / 35-62 % / 62-100 % de la hauteur ; temoin : hlcmp/visual contre hlcmp/visualhl1 (memes reglages)
```
