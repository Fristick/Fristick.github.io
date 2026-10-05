# Rapport v4.5 : ce qui a été fait, vérifié, et ce qui reste à vérifier

## 1. Environnement de travail

| Élément | Valeur |
|---|---|
| Machine | conteneur Linux, 4 cœurs, 15 Go de RAM, **pas de GPU**, **pas d'Unreal Engine** |
| Moteur visé | Unreal Engine **5.8** (`Backrooms.uproject`, inchangé : aucune migration) |
| Outils disponibles | Blender 5.0.1 (module Python `bpy`, rendu Cycles sur processeur), clang (vérification de syntaxe), numpy / Pillow |

Conséquence : **aucune capture du jeu, aucune mesure d'images par seconde, aucune compilation Unreal** n'a été faite
pour cette version. Toutes les images de `Docs/v45/` et `RawAssets/Previews/SK_*_Pose.jpg` sont des **rendus Blender**
des fichiers livrés (ou, pour les flaques, une simulation numpy du shader), et sont marquées comme telles.

## 2. Ce qui a été vérifié, et comment

| Vérification | Méthode | Résultat |
|---|---|---|
| Syntaxe C++ de tous les fichiers modifiés | clang `-fsyntax-only` avec des en-têtes Unreal simplifiés (stubs), `-Wshadow-all` | aucune erreur dans le code v4.5 ; 5 fichiers gardent des erreurs **dues aux stubs** (API absentes du stub : `BRAutoTest`, `BRConfig`, `BRGameMode`, `BRMaterialBuilder`, `BRWaterSim`), comme avant la v4.5 |
| Masquage de membres (C4458, erreur sous Unreal) | script `shadowcheck` | 0 (un cas trouvé et corrigé : `Mesh` dans `BuildClumpSkin`) |
| C++ en ASCII seulement | `grep -P '[^\x00-\x7F]'` | OK |
| Scripts Python | `py_compile` | OK |
| HLSL des flaques identique en Python et en C++ | comparaison du texte | identique (2 605 caractères) |
| Maillages skinnés = original | positions des os principaux comparées aux articulations des pièces rigides (`user_models.json`) | **identiques au centième de cm** pour les 4 modèles |
| Déformation des maillages skinnés | pose de test (marche, bras tendu) rendue dans Blender | articulations continues, pas de déchirure (`Docs/v45/apercu_skinnes_originaux.jpg`) |
| Wretch / Clump : poids de peau | poses rendues ; un bug trouvé et corrigé (doigts du Clump restés liés à la masse) | OK sur les rendus |
| Protection des modèles fournis | relance de `import_user_models.py -- faceling` sans `--force` | 11 fichiers « PROTEGE, non remplacé », aucun fichier modifié ; `protect_assets.py check` : OK |
| Forme des flaques | simulation numpy du HLSL avant / après sur `T_NoiseLF` et `T_Grime` | 23,1 % → 24,1 % du sol en flaque, contours cohérents (`Docs/v45/flaques_avant_apres.png`) |

## 3. Non vérifié (à faire sur un PC Windows avec Unreal 5.8)

Chaque point indique la commande ou la manipulation à faire.

1. **Compilation C++** :
   `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" BackroomsEditor Win64 Development -Project="C:\...\Backrooms.uproject" -WaitMutex`.
   Points d'API à surveiller (non vérifiables sans le moteur) :
   - `FAnimationRuntime::GetComponentSpaceTransformRefPose` ;
   - `ULightComponentBase::CastRaytracedShadow` / `ECastRayTracedShadow` ;
   - `RHIGetTextureMemoryStats` et les champs de `FTextureMemoryStats` ;
   - `IsRayTracingEnabled()` ;
   - `GMaxRHIFeatureLevel` / `ERHIFeatureLevel::SM6`.
2. **Import sur une copie propre** : supprimer `Content/Backrooms/` et `Saved/BackroomsSetup.txt`, ouvrir le projet.
   Le message « Import terminé » doit annoncer 73 textures, 127 modèles et 7 maillages à squelette. À contrôler dans le
   journal :
   - « niveaux de détail » pour `SK_Hound` / `SK_SkinStealer` / `SK_Wretch` / `SK_Clump` / `SK_Hazmat` ;
   - aucun « Maillage a squelette introuvable ».
3. **Compilation des matériaux** (`M_BR_World` v5 : `WaterLine`, `PuddleNoise`) : aucune erreur dans *Window → Output Log*.
   Le jeu, lui, signale « M_BR_World date d'une ancienne version » si l'import n'a pas été refait.
4. **Os des maillages importés** : ouvrir `SK_Faceling` / `SK_Wretch` / `SK_Clump` / `SK_Hound`.
   - Les os `Spine`, `Head`, `LeftArm`… ou `Core`, `Arm0_Upper`… doivent exister.
   - Si un os racine « Armature » a été ajouté par l'import FBX, ce n'est pas gênant (les os sont pilotés par leur nom).
   - Si un os manque, le jeu revient aux pièces rigides et l'écrit dans le journal.
5. **PIE et Standalone** : `UnrealEditor.exe Backrooms.uproject -game -windowed -ResX=1920 -ResY=1080`. En mode
   développeur, **F10** fait défiler les jumpscares des 9 entités. Vérifier :
   - la pose de frappe ;
   - les coudes et genoux qui se plient ;
   - le Clump qui pose ses mains au sol (Niveau 2) ;
   - le Smiler au Niveau 0 pendant une coupure (**Inser**).
6. **Test automatique et mesures**, même graine, même résolution, un passage par profil :
   ```bat
   UnrealEditor.exe Backrooms.uproject -game -windowed -ResX=1920 -ResY=1080 -BRAutoTest -BRAutoTestLevels=0,1,2,37 -BRSeed=4242 -BRAutoTestGPU
   ```
   Résultats : `Saved/AutoTest/Rapport.txt`, `Mesures.csv`, captures `*.png`. Pour comparer avec la v4.4, lancer la même
   commande sur le commit `cac153a` (sans `-BRSeed` dans cette version : les dispositions différeront).
7. **Empaquetage Windows** :
   `RunUAT.bat BuildCookRun -project="C:\...\Backrooms.uproject" -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory="C:\Builds\Backrooms"`
8. **Coopération** à 2 joueurs : `-BRNetTest` (README § 9). À 4 joueurs : 4 instances, adresse 127.0.0.1. À observer :
   - la frappe des entités se voit chez tous (`MulticastStrike`) ;
   - les poses d'état sont calculées chez chacun.
9. **Sauvegarde existante** : charger une partie v4.4. Les réglages d'avant restent en profil **PERSONNALISÉ**.
10. **Ray tracing** :
    - **mode réel** : bas de la page Paramètres et pastille du mode développeur ;
    - **couverture du Surface Cache** : vue *Lit → Lumen → Surface Cache* (zones roses = non couvertes) ;
    - **reflets** : *Lumen → Reflection View* ;
    - **entités dans les reflets** : flaques du Niveau 1 en profil Qualité puis Cinématique. En profil Qualité (reflets
      via le cache de surfaces), une entité à squelette peut apparaître sombre dans un reflet : le *hit lighting* du
      profil Cinématique la montre éclairée ;
    - **VSM ou ombres ray tracées de la lampe** : `stat gpu` en profil Qualité puis Cinématique, lampe allumée (lignes
      *ShadowDepths* / *RayTracing shadows*) ;
    - **eau** : translucidité, réfraction, reflets de premier plan, caustiques (simulées par le matériau, pas ray
      tracées) au Niveau 37 ;
    - **traînées et bruit** pendant une coupure (**Inser**) : Lumen met quelques images à se stabiliser.

## 4. Mesures

**Aucune mesure n'a pu être faite** (pas de GPU ni d'Unreal). L'objectif de **60 images/s en profil Qualité** (Lumen
matériel, TSR à 80 %) reste un objectif. Le tableau ci-dessous est à remplir avec `Saved/AutoTest/Mesures.csv`.

| Niveau | Profil | img/s | 1 % bas | médiane (ms) | GPU (ms) | jeu (ms) | rendu (ms) | VRAM textures (Mo) | chunk max (ms) |
|---|---|---|---|---|---|---|---|---|---|
| 0 | Qualité | non mesuré | | | | | | | |
| 1 | Qualité | non mesuré | | | | | | | |
| 37 | Qualité | non mesuré | | | | | | | |

### Coût des nouveaux assets (mesuré sur les fichiers)

| Asset | Sommets | Triangles | Os | Textures |
|---|---|---|---|---|
| `SK_Faceling` | 1 387 | 656 | 16 | `T_Faceling` (original) |
| `SK_Partygoer` | 3 517 | 6 758 | 68 | `T_Partygoer` (original) |
| `SK_SkinStealer` | 46 087 | 74 427 | 53 | originales |
| `SK_Hound` | 175 201 | 319 000 | 35 | `T_Hound` (original). Pelage d'origine : LOD générés à l'import, coût à mesurer |
| `SK_Wretch` | 12 396 | 24 688 | 30 | `T_Wretch` 2048 + `_N` 2048 |
| `SK_Clump` | 21 630 | 43 080 | 62 | `T_Clump` 2048 + `_N` 2048 |
| `SM_SmilerET` | 11 554 | | | aucune (émissif) |
| `SM_Baseboard` | | 60 | | matériau du mur (projection monde) |

## 5. Fichiers modifiés ou ajoutés

**C++**
- `BRRig.*` : pilote de maillage à squelette (`FBRSkinDriver`), `BuildSkinnedHumanoid`, `AddSkin`, `WretchSK()`.
- `BREntity.*` : entités skinnées avec repli rigide, Clump à IK, poses d'état, foulée liée à la distance, LOD
  d'animation, `MulticastStrike`, émission du Smiler par surface, signalement des replis.
- `BRCharacter.*` : ombres ray tracées de la lampe, effets de jumpscare réduits, signalement du repli de la combinaison.
- `BRPlayerController.*` : profils graphiques, mode de rendu réel, hit lighting réservé au profil Cinématique, ligne
  « ombres ray tracées ».
- `BRHUD.cpp` : mode réel dans les paramètres et en mode développeur, modèles de secours listés, jumpscares allégés,
  version 4.5.
- `BRJumpscare.cpp` : cadrage du Wretch et du Clump, tremblement −40 %.
- `BRAssets.*` : styles `WretchSkin` / `ClumpSkin` (normal maps propres), `GlowEye` / `GlowTooth`, `WaterLine`,
  `ReportFallback`.
- `BRMaterialBuilder.cpp` : flaques v4.5 et ligne d'eau, échantillonneur des objets texture.
- `BRChunk.cpp` : plinthe moulurée.
- `BRLevels.cpp` : ligne d'eau des Poolrooms.
- `BRTypes.h` : `FBRSurface::WaterLine`, réglages `GraphicsProfile` et `bRTShadows`.
- `BRWorld.*` : `-BRSeed`, budget de 5 ms par image pour les chunks, statistiques de construction.
- `BRAutoTest.*` : percentiles, 1 % bas, mémoire, matériel, profil, mode réel, `Mesures.csv`.

**Config** : `DefaultEngine.ini` (cache de skinning inclusif, maillages à squelette dans le ray tracing).

**Python et outils**
- `backrooms_setup.py` : matériaux v5, LOD des maillages à squelette, renommage d'import sûr, Smiler réimporté.
- Nouveaux : `Tools/protect_assets.py`, `Tools/Blender/build_entity_skeletal.py`, `build_creatures.py`,
  `render_compare.py`.
- Modifiés : `import_user_models.py` (protection, nouveau Smiler), `generate_models.py` (plinthe, protection),
  `generate_textures.py` (protection).

**Ressources**
- 6 maillages à squelette.
- 4 textures cuites (`T_Wretch`, `T_Clump` et leurs normal maps).
- `SM_SmilerET` et `SM_Baseboard`.
- Le manifeste `RawAssets/protected_assets.json`.
- Les aperçus Blender.

## 6. Limites restantes

- **Bacteria et Deathmoth** restent en pièces rigides : leurs originaux n'ont pas de squelette. Pour la Bacteria, un
  squelette automatique déformerait ses fils ; les ailes du papillon sont rigides par nature.
- **IK des pieds des humanoïdes** non faite : la foulée suit la distance (plus de glissement sur sol plat), mais les
  pieds ne s'adaptent pas aux marches. Les niveaux concernés n'ont presque pas d'entités (Poolrooms : aucune).
  Seules les mains d'appui du Clump ont une IK.
- **Entités dans les reflets en Lumen logiciel** : les maillages à squelette n'ont pas de champ de distance, donc seuls
  les reflets en espace écran les montrent.
- **Hound** : 175 000 sommets skinnés, c'est cher pour le cache de skinning et la mise à jour de la structure de ray
  tracing. Les LOD générés à l'import sont à vérifier ; sinon, en générer à la main dans l'éditeur.
- **Animations procédurales** : il n'y a pas de clips d'animation (pas de capture de mouvement). Les états (repos,
  marche, détection, poursuite, armé, frappe, récupération) sont des poses procédurales calculées en C++ ; leur rendu
  en mouvement n'a pas été vu.
- **Décors** :
  - Niveau 0 : seule la plinthe est nouvelle (papier peint, moquette, plafond, prises et aérations existaient) ;
  - Niveau 1 : les flaques changent ; béton, peinture, poutres et signalétique datent de la v4.1 ;
  - Poolrooms : seule la ligne d'eau est nouvelle ;
  - aucun nouveau matériau PBR à cartes de rugosité : la rugosité reste un paramètre par surface modulé par du bruit.
- **Sons** : `S_Bacteria`, `S_Scare_Bacteria` et `S_LightBuzz` viennent d'enregistrements non libres (voir les crédits
  du README).
