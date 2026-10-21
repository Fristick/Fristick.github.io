# v4.17 — Matériaux : combinaison, visière et surfaces du Level 0 (conception)

- **Agent** : « Unreal Technical Artist » (définition lue : `~/.claude/agents/unreal-technical-artist.md`), lancé comme sous-agent générique ayant adopté ce rôle.
- **Base** : branche `backrooms-v4.17`, tête `c07eddb` (arbre propre au moment de la lecture).
- **Périmètre** : lecture et conception. Aucun fichier du dépôt n'a été modifié ; ce rapport est le seul fichier créé dans le dépôt. Les essais (lecture du FBX dans Blender 5.2 en arrière-plan, prototype de dérivation des cartes) ont été faits dans le scratchpad de la session, hors dépôt, sur des copies en mémoire : ils n'écrivent ni dans `RawAssets/` ni dans `Content/`.
- **Images regardées par cet agent** : `v416_entite_SkinStealers_3p_face.png`, `v416_entite_SkinStealers_3p_cote.png`, `v416_fenetre_169_max_personnage.png`, `RawAssets/Icons/I_Silhouette.png`, `ref_01.jpg`, `ref_03.jpg`, l'atlas `T_Hazmat_Suit.jpg` (réduit et extrait 1:1), `T_Hazmat_Mask.jpg` réduit, `vis_0_Qualite_{mur_x,sol,plafond,vue,lampe}.png`, `V48_combinaison_L00.png` et `_L37.png` (final416 et final416c), ainsi que les aperçus du prototype.

Légende des constats :
- **[CODE]** constat dans le code, avec `fichier:ligne` ;
- **[IMAGE]** observé sur une image nommée ;
- **[MESURE]** calcul fait hors moteur sur les fichiers du dépôt (scripts du scratchpad) ;
- **[HYPOTHÈSE]** à vérifier dans Unreal ou auprès du propriétaire.

---

## 0. Décisions proposées (résumé)

1. **Nouveau matériau maître `M_BR_Gear`** (« équipement » : combinaison, masque, visière). Il utilise le modèle d'éclairage standard (Default Lit) : ni diffusion sous-cutanée ni parent `Skin`. Il prend une normal map tangente, une carte ORM (R occlusion, G rugosité absolue, B métal), un réglage de saturation et une réflectance (`Specular`). Sans carte, tout reste neutre : `NormalStrength = 0`, `RoughMapWeight = CavityWeight = MetalMapWeight = 0`.
2. **Deux cartes dérivées de l'atlas protégé, 2048²** : `T_Hazmat_Suit_N.png` et `T_Hazmat_Suit_ORM.png`. Leurs sources vont dans un dossier de dérivés distinct, `RawAssets/Derived/Textures/` ; elles sont importées dans `/Game/Backrooms/Textures`, où le jeu cherche les textures par leur nom. Le relief vient de trois sources : les coutures (bords d'îlots UV qui coïncident en 3D), les ourlets (bords libres) et les plis peints de l'albedo, filtrés. Les salissures sont plus rugueuses. Aucun bruit procédural n'est ajouté. Le prototype est validé hors moteur (§ 2.5).
3. **Valeurs** : PVC propre à rugosité 0,36 (0,30 sur les crêtes, 0,46 dans les creux) et `Specular` 0,5. Gants 0,50, bandes noires 0,55, bottes 0,55, semelles 0,78, salissure 0,78. Masque respiratoire en caoutchouc : rugosité 0,55 et **metallic 0**.
4. **Visière** : en opaque, **metallic 0** (au lieu de 0,7), `Specular` 0,6, rugosité 0,06 au centre et jusqu'à 0,15 au bord, albedo fumé d'environ 0,015 en linéaire. Aucune translucidité.
5. **Paquet** : le matériau et les cartes sont ajoutés à `essential_packages.txt`. Un matériau maître ancien utilisé dans un jeu cuisiné doit être **signalé comme problème** (aujourd'hui, il n'est qu'écrit dans le journal). Le test « combinaison : matériaux » doit vérifier le parent et les deux cartes.
6. **Level 0** : le relief du papier peint est **trop fort de près et en lumière rasante** (pente médiane 7,7°, p90 19°, p99 34°). La hauteur proposée dans `Tools/generate_textures.py:244` donne une médiane de 2,6°, un p90 de 7,1° et un p99 de 13° [MESURE]. Il faut régénérer à la fois la carte du jeu et celle du kit (en PNG), et ramener `Cavity` des murs de 0,5 à 0,2. Sur la moquette : `Sheen` 0,8 → 0,5–0,6 et `Stains` 0,65 → 0,4.
7. **Hors matériau** : la vue « personnage » de l'inventaire est une **icône précalculée sombre** (`I_Silhouette.png`). Aucune modification du matériau ne l'éclaircira (§ 1.3). Les captures de test `V48_combinaison_*` **ne montrent pas la combinaison** (§ 1.6).

---

## 1. État actuel

### 1.1 Chemin des matériaux de la combinaison

- [CODE] Le joueur porte `SK_Hazmat` dans un `UPoseableMeshComponent`. Ses trois slots sont affectés par leur nom : `BRCharacter.cpp:3680-3695` (`ApplySlotMap`, table `HazmatSlots` à la ligne 3694). Le Skin-Stealer déguisé emploie le même modèle et passe par `ApplySlots` (`BREntity.cpp:1015-1017` puis `BRRig.cpp:205`). La préparation des shaders aussi (`BRWorld.cpp:2306-2350`, `PrewarmModels`). Ces trois chemins aboutissent à `UBRAssets::SlotMaterial` (`BRAssets.cpp:1969-2041`), qui partage une seule MID par slot (clé `S|HazmatSuit|-`, `BRAssets.cpp:1971`).
- [CODE] Styles des slots : `BRAssets.cpp:157-159`. Combinaison : `T_Hazmat_Suit`, teinte 1, rugosité 0,62, metallic 0. Masque : `T_Hazmat_Mask`, 0,45, **0,1**. Visière : `T_Grime`, teinte (0,015 ; 0,02 ; 0,025), **0,04**, **0,7**.
- [CODE] Choix du parent : `BRAssets.cpp:1988-1989`. Il prend `Skin` si `IsSkinSlot`, sinon `Mesh`. Aucun des trois noms ne contient Skin, Flesh, Vein ou Party (`BRAssets.cpp:1932-1936`) : les trois slots utilisent donc `M_BR_Mesh`.
- [CODE] `M_BR_Mesh` est construit par `BuildMesh(bSkin=false)` (`BRMaterialBuilder.cpp:506-538`) et `build_mesh_material` (`backrooms_setup.py:1028-1055`). Il prend `BaseTex × Tint`, une rugosité constante (avec `RoughTex` seulement si `RoughContrast > 0`), `Metallic` et `Emissive`. **Il n'a ni normale ni `Specular` relié** (réflectance par défaut 0,5) **et pas d'occlusion**. La normale n'est reliée que pour `bSkin` (`BRMaterialBuilder.cpp:531-537`, `backrooms_setup.py:1049-1053`).
- [CODE] `ApplyRoughMap(MID, "T_Hazmat_Suit", 1)` (`BRAssets.cpp:2009`, corps aux lignes 1646-1664) cherche `T_Hazmat_Suit_R`. Ce fichier n'existe pas (`RawAssets/Textures/` ne contient que les deux JPG de la combinaison). Le code met alors `RoughContrast = 0` : **la rugosité est la constante 0,62 sur toute la combinaison**.

### 1.2 Ce que montrent les images

- [IMAGE] `v416_entite_SkinStealers_3p_face.png` et `_3p_cote.png` montrent le joueur de dos, à la 3e personne, sous des néons blancs (Level 4). Les gros plis de la géométrie se lisent dans l'ombrage (dos, capuche). En revanche, **aucun reflet spéculaire n'apparaît** sur les épaules, le haut de la capuche ou les crêtes des plis. La matière évoque du feutre ou de l'argile jaune. Les gants paraissent noirs.
- [MESURE] Sur la zone jaune de cette capture : luminance sRGB p50 0,565 et p99 0,858. Seuls 1,6 % des pixels dépassent 0,85. Teinte médiane 58°, saturation 0,70.
- [IMAGE] Dans `ref_01.jpg` et `ref_03.jpg` (même modèle : bande noire de la capuche, deux pastilles, tuyau), le PVC est brillant à semi-brillant. Les reflets suivent les plis ; `ref_01` montre de fines rides, `ref_03` un aspect presque mouillé. Les bottes et gants sont sombres et satinés, avec des salissures en bas des jambes.
- [MESURE] Teinte médiane des références : 46–49°, plus dorée que la nôtre, mais ces images sont prises sous une lumière jaune du Level 0. Teinte de l'albedo de l'atlas : 56° ; médiane sRGB (220, 208, 70) ; linéaire (0,72 ; 0,63 ; 0,06) ; luminance 0,61. [HYPOTHÈSE] L'écart de teinte tient surtout à la lumière. C'est pourquoi la désaturation n'est proposée qu'en option (`Saturation` 0,92 à juger à l'image).
- Conclusion : la combinaison est mate parce que sa rugosité est constante et élevée (0,62), qu'elle n'a pas de relief fin, et que son seul ombrage vient de la géométrie et des plis peints dans l'albedo.

### 1.3 Vue « personnage » de l'inventaire : une icône, pas le matériau

- [IMAGE] Dans `v416_fenetre_169_max_personnage.png`, le panneau ÉQUIPEMENT montre une silhouette presque noire (luminance sRGB p50 environ 0,18).
- [CODE] Cette silhouette est l'icône `I_Silhouette` dessinée par `BRHUD.cpp:4697`. Elle provient d'un rendu Cycles préparé à l'avance, avec un matériau sombre, par `Tools/Blender/import_user_models.py:408-445` (`render_silhouette`, sortie `RawAssets/Icons/I_Silhouette.png`). [IMAGE] L'icône est effectivement une silhouette gris foncé uniforme.
- Conséquence : **les changements de matériaux proposés ici ne changeront pas cette vue.** Pour qu'elle montre une combinaison jaune lisible, il faut soit refaire l'icône (rendu Blender avec l'albedo et les cartes dérivées, sur fond transparent), soit afficher une capture en direct (coût d'une capture de scène). Cette décision relève du lot UI/inventaire (§ 4.6 du prompt) ; elle est signalée ici sans être conçue.

### 1.4 Maillage, UV et tangentes ([MESURE] : `SK_Hazmat.fbx` lu dans Blender 5.2 en arrière-plan, scratchpad)

- 48 450 sommets et 84 432 triangles. **Une seule couche UV** (`UVMap`), dans [0,006 ; 0,994]. Trois matériaux : `HazmatMask`, `HazmatGlass`, `HazmatSuit`. Normales personnalisées présentes.
- **303 îlots** : 83 pour `HazmatSuit`, 219 pour `HazmatMask` et 1 pour `HazmatGlass`. L'atlas de la combinaison suit un vrai patron de couture : 2 demi-capuches, 2 flancs du torse (avec les pastilles), 2 manches avec poignet noir, 4 panneaux de jambes, 4 bandes de tuyau, 2 gants (sarcelle sombre), 2 tiges de botte (orange avec salissure) et 2 semelles (brun foncé). L'ovale blanc de l'atlas est **l'emplacement exact des UV de la visière** (u 0,887–0,983 ; v 0,175–0,293).
- **Le maillage est séparé le long des coutures UV** : 0 arête intérieure entre deux îlots, 12 218 arêtes de bord. Il y a 3 703 arêtes de bord de la combinaison dont les deux extrémités coïncident en 3D avec un autre bord (**coutures**), et 487 bords libres (**ourlets** : poignets, chevilles, ouverture de la capuche). Cette classification sert directement à placer les bandes de couture.
- Densité de texels de la combinaison : 14 à 14,5 px/cm à 4096, et 11,5 sur les manches. En sortie 2048, cela donne **0,58 à 0,73 px/mm**. Le masque atteint 60 px/cm.
- Îlots en miroir (aire UV signée positive) : pied gauche, main gauche et tige de botte gauche. MikkTSpace prend en charge le signe de la bitangente.
- [CODE] Export : `Tools/Blender/build_hazmat_skeletal.py:160-163` appelle `export_scene.fbx(..., mesh_smooth_type="FACE", ...)` sans `use_tspace`. **Le FBX ne contient donc pas de tangentes.** Import : `fbx_skeletal_options()` (`backrooms_setup.py:319-334`) ne règle ni `normal_import_method` ni `normal_generation_method`. Les valeurs par défaut de l'importeur s'appliquent : normales importées, tangentes calculées par MikkTSpace à partir des normales et de l'UV0. Avec un cache de skinning actif (`DefaultEngine.ini:19-24`), le repère tangent suit la déformation.
- **Verdict** : une normal map en espace tangent est utilisable. Il n'existe qu'une seule couche UV, sans chevauchement visible entre îlots, et MikkTSpace calcule les tangentes à l'import. La normal map dérivée n'est pas cuite contre un modèle haute définition ; il suffit donc d'un repère tangent cohérent avec les UV, ce que MikkTSpace fournit. Risque restant : les UV en demi-précision des maillages à squelette (§ 4.6, point 9).

### 1.5 Textures source et protection

- [CODE] `T_Hazmat_Suit.jpg` et `T_Hazmat_Mask.jpg` font 4096² en sRGB. Ils sont enregistrés dans `RawAssets/protected_assets.json` (catégorie `derived`, sha256 `7c3a9ca5…` et `cb7dd12b…`) et protégés par le motif `RawAssets/Textures/T_Hazmat_*.jpg` (`Tools/protect_assets.py:50`). L'original `Tools/SourceModels/asyc_hazmat.glb` (23,5 Mo) est absent de l'arbre et du disque examiné.
- [MESURE] À 1:1, l'albedo contient une **trame tissée fine**, d'une période d'environ 3–4 px à 4096 (écart-type haute fréquence 0,021). On y voit aussi des plis peints doux (écart-type 0,007–0,008 aux échelles moyennes), des salissures en bas des jambes et des manches, des bandes et pastilles noires (max RGB ≈ 0,09) et un fond gris (185, 187, 184).
- [HYPOTHÈSE] **Le GLB d'origine contient peut-être des cartes normales ou de rugosité.** Dans `import_user_models.py:283-290`, une boucle enregistre *chaque* image d'un matériau `Suit` sous le même nom `T_Hazmat_Suit`. Or `save_texture` (`import_user_models.py:80-96`) refuse d'écraser un fichier déjà enregistré (`may_write` puis `record`) : seule la première image a donc été gardée. Les textures en JPG (≈ 9 Mo) et le maillage (≈ 4 Mo estimés) n'expliquent pas les 23,5 Mo du fichier. **Avant de dériver**, le propriétaire peut lire le bloc JSON du `.glb` (`normalTexture`, `metallicRoughnessTexture` et `occlusionTexture` des matériaux `Suit` et `Mask`). Des cartes d'origine seraient préférables à des cartes dérivées.

### 1.6 Les captures de test actuelles ne montrent pas la combinaison

- [IMAGE] `final416/full/V48_combinaison_L00.png` et `final416c/full/V48_combinaison_L00.png` affichent le carton « NIVEAU 0 » et le panneau développeur, sans personnage. `V48_combinaison_L37.png` est prise sous la surface de l'eau des Poolrooms, sans personnage. Le test `BRAutoTestV48.cpp:249-310` (vérification aux lignes 250-296, capture à la ligne 310) contrôle les références de matériaux, mais **sa capture ne valide rien à l'image**.
- [CODE] À la 1re personne, le corps est entièrement masqué et ne projette pas d'ombre (`BRCharacter.cpp:3745-3756`). Le joueur local ne voit donc sa combinaison qu'à la 3e personne (V).

---

## 2. A — Matière de la combinaison

### 2.1 Parent dédié `M_BR_Gear` (Default Lit, opaque)

Pourquoi un nouveau parent plutôt qu'enrichir `M_BR_Mesh` : `M_BR_Mesh` habille des centaines d'accessoires en instances et des modèles Nanite. Lui ajouter une normale et une carte ORM coûterait deux échantillons sur tous ces objets et changerait leur rendu. `M_BR_Skin`, de son côté, utilise la diffusion sous-cutanée, que le prompt exclut. Un parent « équipement » sans interrupteur statique n'ajoute qu'**une** carte de shaders et pourra servir à d'autres équipements fournis.

Graphe (identique en C++ et en Python ; noms exacts des paramètres) :

| Sortie | Calcul | Valeur neutre (sans carte) |
|---|---|---|
| BaseColor | `lerp(dot(Albedo, Rec709), Albedo, Saturation) × Tint`, où `Albedo = BaseTex(UV0 × TexScale)` | `Saturation` 1, `Tint` blanc |
| Normal (tangent) | `lerp((0,0,1), NormalTex, NormalStrength)` | `NormalStrength` **0** ; défaut de `NormalTex` = normale plate (`T_FlatNormal` en Python, `DefaultNormal` en C++) |
| Roughness | `lerp(Roughness, MaskTex.g, RoughMapWeight)` | `RoughMapWeight` **0** |
| Metallic | `lerp(Metallic, MaskTex.b, MetalMapWeight)` | `MetalMapWeight` **0** |
| AmbientOcclusion | `lerp(1, MaskTex.r, CavityWeight)` | `CavityWeight` **0** |
| Specular | `Specular × lerp(1, AO, 0,5)` (occlusion du reflet dans les creux) | `Specular` 0,5 |
| Emissive | `Base × SelfIllum + Emissive` (même interface que `M_BR_Mesh`) | 0 |
| Scène ray tracée | `RayTracingQualitySwitch` : `Albedo × Tint`, `Roughness` constante, normale plate | — |

- `MaskTex` a pour défaut `T_NoiseLF`, une texture **linéaire**, avec un échantillonneur *Linear Color*. Ce défaut suit la règle de `RoughTex`, qui existe pour éviter les échecs de compilation (`BRMaterialBuilder.cpp:518-524` et `183-185`). Si `T_NoiseLF` est absent ou en sRGB, la construction Python doit **échouer** : `build_materials` supprime alors le matériau à moitié construit (`backrooms_setup.py:1300-1310`) et la validation le signale. Mieux vaut cela qu'un graphe privé du paramètre repère de version.
- Paramètre repère de version (`V2Params`) : **`RoughMapWeight`**. Il doit être relié dans le graphe Python comme dans le graphe C++.
- Usages : les mêmes que `M_BR_Mesh`, c'est-à-dire instances, maillages à squelette et Nanite. Les pièces rigides de secours `SM_Hazmat_*` sont en Nanite (`backrooms_setup.py:428`) et passent aussi par `SlotMaterial`.
- Aucun interrupteur statique : le nombre de permutations se limite au nombre de fabriques de sommets déclarées. Coût estimé de la passe de base : 3 échantillons (base, normale, ORM) et environ 120–150 instructions, à confirmer dans la fenêtre *Stats* de l'éditeur.
- ClearCoat (double lobe « mouillé » de `ref_03`) : **non retenu pour l'instant**. Il demanderait un second maître et un modèle d'éclairage de plus, pour un gain incertain. Il ne vaudrait la peine d'être essayé que si l'image montre, après le Lot A, qu'il manque un reflet net par-dessus le lustre.

### 2.2 Valeurs recommandées

| Zone (région de l'atlas) | Rugosité (ORM.g, absolue) | Specular | Metallic | Remarque |
|---|---|---|---|---|
| PVC jaune propre | **0,36** (plage d'essai 0,34–0,40) | 0,5 (F0 ≈ 4 %, PVC n ≈ 1,54) | 0 | le reflet suit les plis |
| Crête de pli (relief > 0) | 0,30 | 0,5 | 0 | frottée, plus lisse |
| Creux de pli | 0,46 | 0,5 × occlusion | 0 | poussière dans les creux |
| Bande de couture (thermosoudée) | 0,46 | 0,5 | 0 | ruban satiné |
| Bandes et pastilles noires | 0,55 | 0,5 | 0 | caoutchouc appliqué |
| Gants (sarcelle sombre) | 0,50 | 0,5 | 0 | nitrile ou butyle satiné |
| Tiges de botte (orange) | 0,55 | 0,5 | 0 | la couleur d'origine est gardée (ressource protégée) |
| Semelles | 0,78 | 0,5 | 0 | caoutchouc sale |
| Salissure (boue sèche) | vers **0,78** selon le masque | 0,5 | 0 | plus rugueuse que le PVC |
| Tuyau | 0,36 (PVC) | 0,5 | 0 | — |
| Visière (ovale) | 0,05 au centre, jusqu'à 0,15 au bord | **0,6** | **0** | voir § 3 |

Réglages des slots (MID) :
- `HazmatSuit` : `Roughness` 0,38 (utilisé seulement sans carte), `RoughMapWeight` 1, `CavityWeight` 0,6, `NormalStrength` 1, `Specular` 0,5, `Saturation` **1,0** par défaut (essai à 0,92 si la combinaison « éclaire » trop à côté des murs jaunes), `Tint` blanc.
- `HazmatMask` : `T_Hazmat_Mask`, `Roughness` **0,55**, `Metallic` **0** (au lieu de 0,1 : le metallic d'un caoutchouc assombrit le diffus et teinte le reflet), sans normale ni ORM dans un premier temps. Une phase 2 pourrait ajouter un ORM du masque où les pièces gris clair et peu saturées (boucles, bagues des filtres) deviendraient du métal (B = 1, rugosité 0,35).
- `HazmatGlass` : voir § 3.

### 2.3 Cartes dérivées : noms, dossier, format, import

| Fichier source (dépôt) | Asset | Taille | Contenu | Import |
|---|---|---|---|---|
| `RawAssets/Derived/Textures/T_Hazmat_Suit_N.png` | `/Game/Backrooms/Textures/T_Hazmat_Suit_N` | 2048², PNG 8 bits RGB | normale en espace tangent, convention Unreal (DirectX : x à droite, y vers le bas de l'image, `nx = −dh/dx`, `ny = −dh/dy`, comme `Tools/generate_textures.py:615-625`) ; hors îlots (128, 128, 255) | `srgb = False`, **`TC_Normalmap`** (BC5), groupe `WorldNormalMap` : règle existante du suffixe `_N`, `backrooms_setup.py:241-244`. Pas d'inversion du vert. |
| `RawAssets/Derived/Textures/T_Hazmat_Suit_ORM.png` | `/Game/Backrooms/Textures/T_Hazmat_Suit_ORM` | 2048², PNG 8 bits RGB | R occlusion, G rugosité absolue, B métal (0) | `srgb = False`, compression par défaut (BC1) lue par un échantillonneur *Linear Color* (comme les `_ORM` du kit, `backrooms_setup.py:1812-1815`). **`composite_texture = T_Hazmat_Suit_N`, `composite_texture_mode = CTM_NormalRoughnessToGreen`, `composite_power = 1`** : la variance de la normale augmente la rugosité dans les mips, ce qui limite le scintillement à distance. |
| `RawAssets/Derived/Hazmat/hazmat_uv_islands.png`, `hazmat_uv_edges.png`, `hazmat_uv_islands.json` | — (non importés) | 2048² | identifiants d'îlots (16 bits), lignes de couture (255) et de bord libre (128), densité en px/mm et os dominant de chaque îlot | données d'entrée du script, produites par Blender une seule fois |
| `RawAssets/Derived/Textures/T_Hazmat_Suit_maps.json` | — | — | provenance : sha256 de l'atlas source et des masques, version du script, paramètres, statistiques | — |

- Pourquoi 2048 et non 4096 : la densité utile atteint 0,58–0,73 px/mm en 2048, alors que la combinaison couvre au plus 3–4 px/cm à l'écran en 3e personne. Une normal map BC5 en 2048 occupe 5,3 Mo avec ses mips, contre 21 Mo en 4096. La trame tissée de l'albedo (2 px en 2048) est volontairement **écartée** : un PVC enduit la cache, et elle provoquerait du crénelage et un scintillement des reflets.
- Pourquoi un dossier distinct dans `RawAssets` mais le même dossier d'assets : `UBRAssets::Texture` ne cherche que dans `/Game/Backrooms/Textures/<Nom>` (`BRAssets.cpp:1213-1222`, `LoadAsset` aux lignes 844-905). Les noms `T_Hazmat_Suit_*` tombent dans l'ensemble préchargé « common » (`BRAssets.cpp:517-523`). Le motif protégé `T_Hazmat_*.jpg` ne couvre pas ces PNG, qui restent régénérables.
- Les cartes dérivées n'entrent pas dans le registre protégé : leur provenance figure dans le JSON. Le script de dérivation vérifie le sha256 de l'atlas avant de commencer et refuse de s'exécuter si l'atlas diffère du registre. Il appelle aussi `protect_assets.may_write` avant chaque écriture et n'écrit jamais dans `RawAssets/Textures/`.

### 2.4 Algorithme de dérivation (PIL + numpy, sans scipy)

**Étape 0 — Masques UV (Blender, une seule fois).** Script `Tools/Blender/export_hazmat_uv.py` (bpy). Il lit `RawAssets/Skeletal/SK_Hazmat.fbx` sans le modifier, puis :
1. regroupe les faces en îlots : faces voisines par une arête et ayant les mêmes UV aux deux extrémités, même matériau ;
2. pour chaque arête de bord, la classe en **couture** si ses deux sommets 3D (arrondis à 0,01 cm) coïncident avec ceux d'une autre arête de bord, et en **bord libre** sinon ;
3. pour chaque îlot, enregistre le matériau, l'os dominant (poids maximal par sommet) et la densité `px_per_mm = sqrt(aire_UV × 2048² / aire_3D_mm²)` ;
4. rasterise en 2048² (u, v) → (u·W, (1 − v)·W) : identifiant d'îlot (`HazmatSuit` : 1…N ; `HazmatGlass` : 65535), lignes de couture (255) et de bord libre (128), d'un pixel de large.
Sans ces masques, le script retombe sur une segmentation de l'albedo : tout pixel à plus de 0,07 du gris de fond (médiane d'un cadre de 8 px) est pris pour un îlot, et tous les bords comptent comme des coutures. Le script le signale dans le JSON.

**Étape 1 — Préparation.** L'atlas passe de 4096 à 2048 par Lanczos, puis en linéaire (`srgb → lin`). On calcule `L = lin · (0,2126 ; 0,7152 ; 0,0722)`, `mx = max(rgb)`, `sat = (mx − min)/mx` et `hue = atan2(√3(G − B), 2R − G − B)`, toutes ces grandeurs étant prises sur les valeurs sRGB. Le flou est un quasi-gaussien obtenu par trois flous en boîte à sommes cumulées. Le « flou masqué » vaut `flou(a·m)/flou(m)`.

**Étape 2 — Régions** (dans les îlots) :
- jaune PVC : `sat > 0,35` et `38° < hue < 75°` ;
- orange (bottes) : `sat > 0,45` et `5° < hue ≤ 38°` ;
- gants : `sat > 0,15`, `160° < hue < 240°` et `mx < 0,5` ;
- bandes et pastilles noires (`trim`) : `mx < 0,16` et `sat < 0,35`, puis `flou_σ2 > 0,85` et écart-type local (σ = 3) `< 0,02`, puis `flou_σ1 > 0,5`. Le seuil de 0,16 est nécessaire : avec 0,28, la boue sombre passait pour du caoutchouc et prenait un relief de plaque (corrigé dans le prototype) ;
- semelles : îlots dont l'os dominant est `LeftFoot` ou `RightFoot` et dont la hauteur relative est inférieure à 0,03 ;
- visière : identifiant 65535.

**Étape 3 — Salissure.** On calcule `L_loc` et `S_loc` : flous masqués (σ = 24 px) de `L` et de `sat` sur jaune ∪ orange. Puis :
`dirt = smoothstep(0,08 ; 0,30 ; S_loc − sat) × smoothstep(0,15 ; 0,45 ; (L_loc − L)/L_loc)`, hors bandes et gants, flou σ1.
On en déduit une zone de salissure dilatée : `dirt_zone = clip(3 · flou_σ6(dirt > 0,03), 0, 1)`.

**Étape 4 — Distances.** `d_couture` et `d_libre` sont les distances en px aux lignes de l'étape 0, obtenues par dilatations alternées en carré et en croix (métrique octogonale), bornées à 24 px. On les convertit en mm avec le `px_per_mm` de l'îlot.

**Étape 5 — Hauteurs (mm)** :
- **bande de couture** (sauf gants) : `0,30·(1 − smoothstep(9 ; 12 ; d_c)) + 0,12·exp(−(d_c/3)²)`. La pente est **nulle au bord de l'îlot** : la hauteur est la même des deux côtés de la couture 3D, donc **aucune marche de normale au raccord UV** ;
- **ourlet** : `0,35·exp(−((d_l − 5)/3)²)` ;
- **bandes et pastilles** : `0,30 · flou_σ1,5(trim)` (bord adouci sur environ 2 mm) ;
- **plis peints** : on pose `fold_m = jaune ∧ ¬trim × (1 − dirt_zone)`, puis `rel = clip(flou_masqué_σ1,5(L)/flou_masqué_σ18(L) − 1 ; −0,2 ; 0,2)`, et
  `h_pli = rel/0,10 × 0,45 × flou_σ2,5(fold_m) × smoothstep(4 ; 14 ; min(d_c, d_l))`.
  Le passe-bande retire l'éclairage cuit à basse fréquence. Le masque adouci et la zone de salissure retirent les marches aux bords de la boue. Le fondu près des coutures garde la bande propre ;
- **rides fines** (option, **désactivée par défaut**, à juger à l'image) : `rel2 = clip(flou_σ0,8/flou_σ5 − 1 ; ±0,1)` et `act = smoothstep(0,03 ; 0,08 ; √flou_σ30(rel²))`. On ajoute `h_ride = rel2/0,05 × 0,04 × act × flou(fold_m) × fondu × (1 − dirt)`. Les rides n'apparaissent que là où le peintre a mis des plis : ce n'est pas un bruit uniforme ;
- **salissure** : `0,025 × dirt × clip(rel2/0,05 ; ±1)` (grumeaux à peine en relief).

On convertit ensuite `h_px = h_mm × px_per_mm`.

**Étape 6 — Normale.** Différences centrales `dx` et `dy`, mises à zéro quand un voisin est hors de l'îlot (pas de falaise au bord UV). Puis `n = normalize(−dx, −dy, 1)`. Hors des îlots, la normale est plate.

**Étape 7 — Rugosité absolue (G).** On part de 0,36, puis : `+0,10·smoothstep(0 ; −0,4 ; h_pli)` et `−0,06·smoothstep(0 ; 0,4 ; h_pli)` ; 0,46 sur la bande de couture (où `h_bande > 0,15`) ; 0,55 sur l'orange ; 0,50 sur les gants ; 0,55 sur les bandes ; 0,78 sur les semelles ; mélange vers 0,78 selon `dirt`. Dans l'ovale de la visière : `0,05 + 0,10·smoothstep(0,55 ; 1 ; r)`, où `r` est la distance normalisée au centre de l'ovale (le bord est plus manipulé et retient la buée ; c'est la seule forme dessinée, et elle est radiale, pas un bruit). On termine par un flou σ0,7.

**Étape 8 — Occlusion (R)** : `clip(1 − 0,35·smoothstep(0 ; −0,45 ; h_pli) − 0,15·dirt ; 0,55 ; 1)`, et 1 hors des îlots. **Métal (B)** : 0.

**Étape 9 — Bourrage.** On étend chaque carte d'au moins 16 px autour des îlots, par moyenne masquée itérative. Le gris de fond ne se mélange ainsi plus aux mips. La normale hors îlots reste plate (le bord est déjà à pente nulle).

**Étape 10 — Écriture et contrôles automatiques.** Écriture en PNG 8 bits RGB, accompagnée du JSON de provenance. Le script échoue si l'un des critères suivants n'est pas tenu : pente médiane dans les îlots ≤ 1,5° ; p99 ≤ 15° ; aucune pente > 45° ; rugosité dans [0,04 ; 0,85] ; fond plat ; dimensions 2048 ; sortie identique d'une exécution à l'autre (le script ne tire aucun hasard).

### 2.5 Prototype (scratchpad, hors dépôt) — résultats mesurés

- Exécuté sur l'atlas du dépôt et des masques UV extraits du FBX par Blender en arrière-plan : **43 s** en 2048 (numpy pur).
- [MESURE] Pente dans les îlots : p50 0,82°, p90 2,6°, p99 7,3°, max 26,7°. Rugosité : p5 0,33, p50 0,37, p95 0,64. Salissure > 0,5 sur 5,8 % des pixels des îlots ; bandes noires sur 1,2 %. Fond exactement plat.
- [IMAGE] (aperçus du prototype, éclairage rasant appliqué à la normale) :
  - les bandes de couture se lisent comme un ruban légèrement en relief le long des panneaux ;
  - les pastilles ont un rebord ;
  - les plis peints du torse et de la capuche donnent un relief doux ;
  - après les deux corrections décrites à l'étape 2 et à l'étape 5, la boue ne prend plus de relief de plaque, mais un léger relief reste au front de la salissure. Cela peut se lire comme de la boue séchée ; à juger dans le jeu.
- Défaut restant : les rides fines (option) captent un résidu de trame et de blocs JPEG. Elles restent donc désactivées par défaut.

### 2.6 Import du maillage : réglages à rendre explicites

- `fbx_skeletal_options()` (`backrooms_setup.py:330-333`) : ajouter `normal_import_method = FBXNIM_IMPORT_NORMALS` et `normal_generation_method = MIKK_T_SPACE` dans `skeletal_mesh_import_data`. Ce ne sont pas des changements de résultat attendus, mais aujourd'hui ces valeurs dépendent des défauts de l'importeur (Interchange en 5.8).
- **Ne pas** augmenter `VERSION` pour cela : la combinaison n'a pas besoin d'être réimportée. Ces réglages vaudront pour le prochain import.
- Vérification visuelle dans l'éditeur, une seule fois : *Skeletal Mesh* > *Show* > *Tangents*, et une lumière rasante sur les deux gants (l'un est en miroir).

---

## 3. B — Visière (`HazmatGlass`)

- [CODE] Réglages actuels : parent `M_BR_Mesh`, `BaseTex = T_Grime` (bruit sRGB, environ 0,59 en linéaire), `Tint` (0,015 ; 0,02 ; 0,025), rugosité 0,04, **metallic 0,7**, `Specular` non relié (0,5) (`BRAssets.cpp:159`).
- [MESURE, calcul] Avec metallic 0,7, on obtient `F0 = lerp(0,04 ; base ≈ 0,012 ; 0,7) ≈ 0,020`. La visière **reflète deux fois moins qu'un vrai verre** (4 %), et son diffus est réduit à 30 % : elle ressemble à un miroir sombre et terne. Le bruit de `T_Grime` donne en plus des taches de luminosité sans cause. Enfin, une rugosité de 0,04 sur une petite surface courbe scintille à distance.
- [IMAGE] Les captures disponibles montrent la combinaison de dos : **la visière n'y est pas visible**. Le jugement à l'image reste à faire (§ 6).
- Proposition (opaque, sans translucidité) :
  - parent `M_BR_Gear` ;
  - `BaseTex = T_Hazmat_Suit` : les UV de la visière tombent dans l'ovale blanc, uniforme, de l'atlas, environ 0,87 en linéaire ;
  - `Tint` (0,016 ; 0,018 ; 0,020), soit un albedo d'environ 0,014–0,017 (verre fumé, intérieur sombre) ;
  - **`Metallic` 0**, **`Specular` 0,6** (polycarbonate, n ≈ 1,58, F0 ≈ 5 %) ;
  - `Roughness` 0,06, avec `MaskTex = T_Hazmat_Suit_ORM` et `RoughMapWeight` 1 (0,05 au centre, 0,15 au bord) ;
  - `NormalStrength` 0, `CavityWeight` 0, pas d'émission.
- Réponse attendue :
  - avec Schlick, le reflet passe de 5 % de face à environ 17 % à 70° d'incidence : le bord de la visière s'éclaircit en lumière rasante, comme un vrai verre ;
  - une lampe de coéquipier y fait un reflet net ;
  - pendant une coupure, la visière reste noire.
- Pourquoi pas de translucidité : le visage et l'intérieur du casque ne sont pas modélisés, il n'y aurait donc rien à montrer. La translucidité ajouterait une passe triée, le problème de la première couche Lumen, des effets sur le TSR et des ombres ray tracées moins fiables, pour aucun gain. Le coût de la proposition est celui de la combinaison : même parent, même MID partagée.
- Réflexions de la visière : sa rugosité est sous `r.Lumen.Reflections.MaxRoughnessToTrace`, qui vaut 0,4, ou 0,5 en qualité ≥ 3 (`BRPlayerController.cpp:4311`). La visière reçoit donc des réflexions Lumen tracées, matérielles dans les profils RT et en traces d'écran ou SDF sinon. Comme elle est petite, le coût est négligeable.

---

## 4. C — Changements fichier par fichier

### 4.1 `Source/Backrooms/Public/BRMaterialBuilder.h` et `Private/BRMaterialBuilder.cpp`

- `.h:12-21` : ajouter `Gear` à la fin de `EBRMasterMaterial`, avec le commentaire « v4.17 : équipement (combinaison, masque, visière) : éclairage standard, normal map tangente, ORM ».
- `.cpp`, après `BuildMesh` (fin vers la ligne 538) : nouvelle fonction `BuildGear(FGraph&, UMaterialEditorOnlyData*, TFunctionRef<UTexture*(FName)>)`, qui suit le graphe du § 2.1. Défauts : `BaseTex` = `T_Hazmat_Suit` ou `EngineTexture(false)` ; `NormalTex` = `EngineTexture(true)` avec `bNormal = true` ; `MaskTex` = `T_NoiseLF`, à condition qu'elle soit linéaire (même test qu'aux lignes 518-519). Toutes les sorties passent par `G.RTSwitch` pour la scène ray tracée. Les sorties `Specular` et `AmbientOcclusion` sont **reliées**, ce que `BuildMesh` ne fait pas.
- `.cpp:843-859` (`RequiredUsages`) : ajouter `case EBRMasterMaterial::Gear:` au groupe `Mesh/Skin` (instances, maillages à squelette, Nanite).
- `.cpp:888-889` (`Names[]`) : ajouter `TEXT("M_BR_Gear_Runtime")`, puis **`static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(EBRMasterMaterial::Gear) + 1)`**. Aujourd'hui, rien ne protège cette table contre un décalage.
- `.cpp:909-944` (`switch`) : `case EBRMasterMaterial::Gear: BuildGear(G, Out, LoadTexture); break;`. La normale reste en espace tangent : la ligne 901 ne concerne que World et WorldCloth.

### 4.2 `Source/Backrooms/Public/BRAssets.h` et `Private/BRAssets.cpp`

- `.h:199` : `enum class EParent : uint8 { World, Mesh, Skin, WaterSurface, PitShade, WorldCloth, Decal, Gear, Count };`.
- `.cpp:69-79` (`FSlotStyle`) : ajouter à la fin, avec des valeurs par défaut (l'initialisation agrégée des 100 lignes existantes reste valide) :
  `const TCHAR* Orm = nullptr; bool bGear = false; float Specular = 0.5f; float Saturation = 1.f; float NormalStrength = 1.f; float Cavity = 0.f;`
- `.cpp:157-159` : remplacer les trois lignes de l'équipement par :
  - `{ TEXT("HazmatSuit"), TEXT("T_Hazmat_Suit"), FLinearColor(1.f, 1.f, 1.f), 0.38f, 0.f, 1.f, TEXT("T_Hazmat_Suit_N"), TEXT("T_Hazmat_Suit_ORM"), true, 0.5f, 1.f, 1.f, 0.6f },`
  - `{ TEXT("HazmatMask"), TEXT("T_Hazmat_Mask"), FLinearColor(1.f, 1.f, 1.f), 0.55f, 0.f, 1.f, nullptr, nullptr, true, 0.5f, 1.f, 0.f, 0.f },`
  - `{ TEXT("HazmatGlass"), TEXT("T_Hazmat_Suit"), FLinearColor(0.016f, 0.018f, 0.02f), 0.06f, 0.f, 1.f, nullptr, TEXT("T_Hazmat_Suit_ORM"), true, 0.6f, 1.f, 0.f, 0.f },`
- `.cpp`, espace anonyme après `FindSlotStyle` (vers la ligne 220) : fonction libre `ApplyGearMaps(UBRAssets& A, UMaterialInstanceDynamic* MID, const FSlotStyle& S, const FString& Slot)`. Elle doit être libre parce que `FSlotStyle` est privé au fichier. Elle :
  1. lie `NormalTex` si `A.Texture(S.Normal)` existe, et met `NormalStrength` à `S.NormalStrength`, ou à 0 sinon ;
  2. lie `MaskTex` seulement si la carte n'est **pas** en sRGB et si la valeur par défaut du paramètre est linéaire (même garde que `ApplyRoughMap`, lignes 1653-1658), et met `RoughMapWeight` à 1 ou 0 et `CavityWeight` à `S.Cavity` ou 0 ;
  3. met `MetalMapWeight` à 0, `Specular` et `Saturation` ;
  4. appelle **`UBRAssets::ReportMaterialProblem`** si une carte déclarée manque ou est en sRGB (« valeurs neutres »). Ce n'est jamais silencieux.
- `.cpp:1986-2021` (`SlotMaterial`) : `bGear = Style && Style->bGear` ; `bSkin = !bGear && IsSkinSlot(SlotName)` ; parent `Gear`, `Skin` ou `Mesh`. Dans la branche `bCustom`, appeler `ApplyGearMaps` à la place d'`ApplyRoughMap` pour l'équipement. Le reste est inchangé (`BaseTex`, `Tint`, `TexScale`, `Roughness`, `Metallic`, `Emissive`).
- `.cpp:1407-1413` (`Parent`) : ajouter `TEXT("M_BR_Gear")` à `AssetNames`, `TEXT("RoughMapWeight")` à `V2Params` et `EBRMasterMaterial::Gear` à `UsageKinds` (lignes 1439-1440). Les `static_assert` existants vérifient les tailles.
- `.cpp:1425-1434` : **changement de comportement voulu.** Quand `!bV2` et `!BRMaterialBuilder::IsAvailable()` (jeu cuisiné), le code actuel garde l'ancien matériau et n'émet qu'un `UE_LOG Warning`. Il faut y ajouter `ReportMaterialProblem("<nom> date d'une ancienne version dans le paquet : rendu ancien")`, pour **tous** les maîtres. C'est ce qui interdit un ancien matériau importé utilisé en silence dans le paquet.
- `.cpp:1455-1462` : ajouter un repli pour `Gear`, sur le modèle de `Skin` : `M = Parent(EParent::Mesh)` avec `ReportMaterialProblem("M_BR_Gear absent : équipement sur M_BR_Mesh, sans relief")`.
- `.cpp:1236` (repli de l'éditeur sur les fichiers bruts) : ajouter `|| N.EndsWith(TEXT("_ORM"))` à `bLinear`. Dans `LoadRawTexture`, essayer aussi le sous-dossier `Derived/Textures` (éditeur seulement ; jamais dans un paquet, ligne 912).
- `.cpp:2206-2265` (`DescribeSections`) : pour un parent `M_BR_Gear`, afficher `NormalTex`, `MaskTex`, `NormalStrength` et `RoughMapWeight`. Ce diagnostic est déjà écrit par `BRCharacter.cpp:3697-3704` et par le test v4.8.

### 4.3 `Content/Python/backrooms_setup.py`

- l. 42-52 : **`MATERIAL_VERSION = 10`**, commentaire « v4.17 : M_BR_Gear (équipement du joueur : normal map tangente, ORM, réflectance) ; cartes dérivées de la combinaison ». `VERSION` ne change pas : la réimportation complète n'est pas nécessaire.
- l. 73 : ajouter `"M_BR_Gear"` à `MATERIALS`.
- l. 76-85 : `USAGES["M_BR_Gear"] = _MODEL_USAGES` (instances, maillage à squelette, Nanite).
- l. 101 : ajouter `GEAR_TEXTURES = ("T_Hazmat_Suit_N", "T_Hazmat_Suit_ORM")` et `DERIVED_SUB = os.path.join("Derived", "Textures")`.
- l. 200-206 : ajouter `is_orm_map(name)`, qui renvoie vrai si le nom finit par `_ORM`.
- l. 219-256 (`import_textures`) :
  - ajouter un paramètre `sub=None` (par défaut `"Icons"` ou `"Textures"`) ;
  - ajouter une branche `elif is_orm_map(name):` qui règle `srgb = False` et `lod_group = TEXTUREGROUP_WORLD` ;
  - ajouter une passe finale : pour chaque `X_ORM` dont `X_N` existe, régler `composite_texture`, `composite_texture_mode = CTM_NORMAL_ROUGHNESS_TO_GREEN` et `composite_power = 1.0` ;
  - enregistrer le sha256 du fichier source avec `EAL.set_metadata_tag(tex, "BRSourceSha256", …)`.
- l. 319-334 (`fbx_skeletal_options`) : réglages de normales et tangentes explicites (§ 2.6).
- Après `build_mesh_material` (l. 1055) : nouvelle fonction `build_gear_material()`, identique à `BuildGear`. Elle utilise `g.tex("MaskTex", noise_tex, uv)[0]` puis `(orm, "R")`, `(orm, "G")` et `(orm, "B")`, comme le kit (l. 1626-1629), et `g.tex("NormalTex", load_tex("T_FlatNormal"), uv, normal=True)`. Elle lève `RuntimeError` si `T_NoiseLF` est absent ou en sRGB.
- l. 1296-1299 (`build_materials`) : ajouter `("M_BR_Gear", build_gear_material)`.
- l. 1397-1452 (`validate`) :
  - ajouter `M_BR_Gear` aux matériaux vérifiés, ce qui est automatique par `MATERIALS` ;
  - ajouter une boucle `GEAR_TEXTURES` : présente, côté ≥ 2048, **pas** en sRGB, `_N` en `TC_NORMALMAP`, et étiquette `BRSourceSha256` égale au sha256 du fichier de `RawAssets/Derived/Textures`. Toute différence est un problème « texture » que `repair()` réimporte ;
  - pour l'option rides : s'assurer que les cartes contrôlées sont bien celles qui sont livrées.
- l. 1518-1523 (`needs_setup`) et l. 1864-1906 (`run`) : ajouter `missing_in(DERIVED_SUB, TEX, IMG_EXT)` et l'import `import_textures(der, sub=DERIVED_SUB)` **avant** `build_materials`. `run(force=True)` réimporte aussi le dossier des dérivés.

### 4.4 Outils (nouveaux fichiers, aucun fichier protégé touché)

- `Tools/Blender/export_hazmat_uv.py` : étape 0 du § 2.4. Il lit le FBX sans le modifier et écrit seulement dans `RawAssets/Derived/Hazmat/`.
- `Tools/derive_hazmat_maps.py` : étapes 1 à 10. Ses entrées sont l'atlas (dont le sha256 est vérifié dans `protected_assets.json`) et les masques. Il écrit dans `RawAssets/Derived/Textures/` et passe par `protect_assets.may_write`. Options : `--size 2048`, `--crinkle 0|1`, `--check` (contrôles seuls).
- `Tools/Tests/test_material_sync.py` (contrôle statique, rapide). Il vérifie que les maîtres listés sont les mêmes dans `backrooms_setup.MATERIALS`, `BRAssets.cpp` (`AssetNames`), `BRMaterialBuilder.cpp` (`Names[]`) et `essential_packages.txt`. Il vérifie aussi que chaque paramètre de `V2Params` apparaît dans le graphe Python **et** dans le graphe C++ du même maître. Aujourd'hui, aucun test ne garde cette synchronisation.

### 4.5 Garantir que le paquet Shipping cuisine et utilise le nouveau matériau

1. Cuisson : `Config/DefaultGame.ini:9` cuit tout `/Game/Backrooms`. `M_BR_Gear` et les deux textures y sont, à condition que **l'import ait été rejoué dans l'éditeur avant la cuisson**. Le marqueur `MATERIAL_VERSION = 10` déclenche cet import à l'ouverture de l'éditeur (`init_unreal.py:44-45`).
2. `Tools/Build/essential_packages.txt`, après la ligne 13 : ajouter `/Game/Backrooms/Materials/M_BR_Gear`, `/Game/Backrooms/Textures/T_Hazmat_Suit_N` et `/Game/Backrooms/Textures/T_Hazmat_Suit_ORM`. Le contrôle « contenu » de `check_package.py` échoue alors si l'un manque au paquet.
3. Au lancement, `Parent()` signale un maître absent ou ancien (§ 4.2), et `ApplyGearMaps` signale une carte absente. Les deux alimentent `GetMaterialProblems()`.
4. Le test `BRAutoTestV48.cpp:250-296` (« combinaison : matériaux ») doit marquer **ÉCHEC**, et non une simple note, si pour le slot `HazmatSuit` :
   - le parent n'est pas `M_BR_Gear` ;
   - `NormalTex` n'est pas `T_Hazmat_Suit_N` ;
   - `MaskTex` n'est pas `T_Hazmat_Suit_ORM` ;
   - `NormalStrength` ≤ 0 ou `RoughMapWeight` ≠ 1.
   Pour `HazmatGlass`, le test doit aussi vérifier metallic 0. Avec `-BRRuntimeMaterials` dans l'éditeur, le parent attendu devient `M_BR_Gear_Runtime`.
5. Lancer le paquet avec `-BRAutoTest -BRAutoTestStrict`, puis `check_package.py --mode release --launch` sur le commit final.

### 4.6 Risques

1. **Permutations** : un maître de plus, avec trois usages (instances, squelette, Nanite) et sans interrupteur statique. Cela ajoute une carte de shaders d'une taille comparable à celle de `M_BR_Mesh` : cuisson plus longue de quelques minutes et paquet un peu plus lourd. À mesurer dans le journal de cuisson.
2. **Usage `SkeletalMesh`** : s'il manque dans l'asset importé, le jeu cuisiné affiche le matériau par défaut. `USAGES` et `EnsureUsages` (`BRAssets.cpp:1492-1533`) couvrent ce cas ; la validation Python le signale.
3. **Ancien matériau importé** : traité au § 4.2 (signalement dans le paquet) et au § 4.5.
4. **MID en cache partagée** : une seule MID par slot, partagée par le joueur, les coéquipiers et le Skin-Stealer déguisé. Elle n'est recréée que dans un nouveau monde, parce que `UBRAssets` est un sous-système de monde. Aucun code ne modifie ces MID en jeu aujourd'hui : un futur effet par joueur (mouillé, sang) devra utiliser des MID propres.
5. **Profils sans RT** (Performance, Linux, macOS, DX11) : les maillages à squelette n'ont pas de champ de distance dans Lumen logiciel. La combinaison reçoit la lumière indirecte par les sondes d'écran et ses reflets par les traces d'écran. La normale et la rugosité fonctionnent dans tous les profils ; elles n'ont pas besoin de RT. À vérifier en Performance.
6. **Coût des reflets tracés** : avec une rugosité de 0,30–0,40, sous `MaxRoughnessToTrace` (0,4, ou 0,5 en qualité ≥ 3, `BRPlayerController.cpp:4311`), les pixels de la combinaison reçoivent des réflexions Lumen tracées. À quatre joueurs proches, c'est à mesurer avec ProfileGPU, en comparant Lumen Reflections avant et après. Si le coût est notable, passer la rugosité nominale à 0,41 : les reflets directs des néons restent, seuls les reflets tracés disparaissent.
7. **Scintillement** : faible rugosité plus normales fines donne des étincelles de reflet en mouvement. Les parades sont le mode composite (rugosité augmentée par la variance de la normale), la rugosité minimale de 0,30 et le TSR. Vérifier en vidéo.
8. **PSO** : avec `r.PSOPrecache.ProxyCreationWhenPSOReady = 1` (`DefaultEngine.ini:44-46`), un composant dont les PSO ne sont pas prêts n'est pas dessiné. `PrewarmModels` affiche `SK_Hazmat` avec le nouveau matériau derrière l'écran noir. Il reste à vérifier un **client qui rejoint en cours de niveau**, pour qu'un coéquipier n'apparaisse pas en retard. Le cache de PSO du paquet (`Docs/RAPPORT_v4.8.md` § 7.4), s'il est utilisé, devra être réenregistré.
9. **Précision des UV** : les UV des maillages à squelette sont stockés en demi-précision par défaut. Près de u ≈ 0,98, l'erreur atteint environ 1 texel en 2048. Si les bandes de couture paraissent tremblées sur les bords d'atlas, activer *Use Full Precision UVs* sur `SK_Hazmat` (+4 octets par sommet, environ 0,2 Mo).
10. **Double comptage des plis** : les plis sont peints dans l'albedo et repris dans la normale. L'amplitude est volontairement faible (pente p99 de 7° dans le prototype). C'est à juger en lumière rasante, ou à réduire par `NormalStrength`.
11. **Substrate** : il n'est pas activé dans `DefaultEngine.ini`. S'il l'était un jour, la conversion automatique du Default Lit est sans risque particulier, mais devrait être revérifiée.

---

## 5. D — Surfaces du Level 0

### 5.1 Deux chemins pour les mêmes matières ([CODE])

- Murs pleins : modules du kit (`SM_L0_Wall_350`, etc.) avec **leurs propres matériaux** `M_L0_Wallpaper` → `M_L0K_PbrWorld` (`BRChunk.cpp:139-151`, `bMeshMaterials`, et lignes 783-825). Les textures sont `T_L0K_Wallpaper_*` (`backrooms_setup.py:1539-1540`, graphe aux lignes 1592-1632).
- Jambages, linteaux et pans coupés : des boîtes avec `D.Wall` → `M_BR_World` et `T_L0_Wallpaper` (+ `_N`, `_R`) (`BRChunk.cpp:826-830`).
- Sols et plafonds : dalles du kit habillées de `A->Surface(D.Floor / D.Ceiling)`, c'est-à-dire `M_BR_WorldCloth` pour la moquette et `M_BR_World` (`BRChunk.cpp:153-199`).
- [MESURE] `T_L0K_Wallpaper_Normal.jpg` est une copie de `T_L0_Wallpaper_N.png` (statistiques de pente identiques), **enregistrée en JPEG de qualité 90** (`Tools/Blender/level0_kit_textures.py:121-133`, `lossless=False`). C'est en contradiction avec la règle de `generate_textures.py:102-110`, qui écrit les normal maps en PNG justement à cause des blocs JPEG.
- **Toute retouche du papier peint doit donc toucher les deux chemins**. Sinon, un linteau en boîte ne se raccordera plus au module voisin.

### 5.2 Paramètres et endroits où ils sont fixés

| Effet | Paramètre exact | Valeur actuelle | Où |
|---|---|---|---|
| Relief du papier peint (carte) | hauteur `0.35*motif - 0.3*border + 0.12*weave/3.0 - 0.9*gap + 0.6*lift`, force `save_normal(..., 2.0, S)` (k = 8) | — | `Tools/generate_textures.py:244-245` |
| Relief, chemin des boîtes | `NormalStrength` de `M_BR_World` | **forcé à 1,0** dès qu'une `_N` existe (non réglable par surface) | `BRAssets.cpp:1718` ; graphe `BRMaterialBuilder.cpp:493`, `backrooms_setup.py:963` |
| Relief, chemin du kit | aucun paramètre (`BRAxisNormal` à pleine force) | — | `backrooms_setup.py:1631-1632` |
| Rugosité du papier | `Roughness` 0,8 et `RoughDetail` 1 (`T_L0_Wallpaper_R`, moyenne relative 0,467, soit environ 0,73) ; `Specular` 0,4 | — | `BRLevels.cpp:57`, `:63` |
| Occlusion de cavité des murs | `Cavity` 0,5 (dérivée de la luminance : les liserés imprimés comptent comme des creux) ; kit : `ao_from_height(h, 6, 1.6)` | — | `BRLevels.cpp:63` ; `level0_kit_textures.py:157` |
| Moquette, modèle tissu | `Sheen` 0,8 (Cloth = Custom Data 0) ; `FuzzTint` (1,3 ; 1,25 ; 1,15) par défaut dans le graphe (non réglé par `CreateSurface`) ; `Specular` 0,3 ; `Cavity` 1 ; `Roughness` 0,95 ; `Grime` 0,3 | — | `BRLevels.cpp:54`, `:62` ; `BRMaterialBuilder.cpp:497-503` ; `backrooms_setup.py:967-971` |
| Relief de la moquette | `save_height_normal("T_L0_Carpet", height * 0.9)` : pente p50 15°, p90 24° [MESURE] | — | `generate_textures.py:299` |
| Taches au sol | `Stains` 0,65 ; `StainScale` 1600 ; seuil 0,7/0,12 ; couleur (0,7 ; 0,7 ; 0,56) | — | `BRLevels.cpp:56` ; `BRMaterialBuilder.cpp:375-378` ; `backrooms_setup.py:861-865` |
| Saleté générale | `Grime` (5e argument de `S()`) : sol 0,3, murs 0,3, plafond 0,15 ; `GrimeScale` 900 | — | `BRLevels.cpp:54-58` ; `BRMaterialBuilder.cpp:392-394` |
| Murs : teinte et auréoles | `WallVariation` 1 ; seuil 0,68/0,1 ; couleur (0,8 ; 0,76 ; 0,62) | — | `BRLevels.cpp:115` ; `BRMaterialBuilder.cpp:383-389` |
| Pied de mur, ligne de marée | `FloorGrime` 0,6 ; `TideLine` 1 | — | `BRLevels.cpp:113`, `:63` ; kit `backrooms_setup.py:1539-1540` |
| Décalques | `DecalDensity` 1 ; opacité par sorte : `WaterStain` 0,85, `CeilingStain` 0,9, `Damp` 0,75, `Scuff` 0,8 | — | `BRLevels.cpp:80` ; `BRAssets.cpp:1626-1631` |

Remarque : `D.Pillar = D.Wall` est copié à `BRLevels.cpp:65`, avant les réglages des lignes 113-117. Un nouveau réglage du mur devra aussi être recopié sur `D.Pillar`.

### 5.3 Observé

- [IMAGE] `vis_0_Qualite_mur_x.png` est un gros plan d'environ 76 cm de largeur de mur. Le motif se lit comme **un cuir ou un plastique embossé** : chevrons à arêtes biseautées éclairées d'un côté et ombrées de l'autre, liserés en gouttière, trame verticale « brossée » très marquée. On ne lit pas un papier peint imprimé.
- [IMAGE] `vis_0_Qualite_vue.png` est vu à distance normale : le relief y est discret et acceptable. Les murs paraissent plutôt jaune-vert pâle que dorés. Ce point relève de l'étalonnage et de la lumière (lot éclairage), pas de ce lot.
- [IMAGE] `vis_0_Qualite_sol.png` et `vue.png` : la moquette est mate, ce qui convient. Elle porte **des taches sombres de 20 à 50 cm réparties assez régulièrement** sur tout le sol, et de petits points sombres de 5 à 10 cm. Le sol paraît « daim sale » plus que moquette humide par endroits. [HYPOTHÈSE] Sources probables, à séparer : `Stains` 0,65, les zones humides et la saleté peintes dans `T_L0_Carpet` (répétées tous les 160 cm et seulement en partie cassées par `AntiTile`) et les décalques `Damp`.
- [IMAGE] `vis_0_Qualite_plafond.png` : les dalles et l'ossature sont crédibles. L'auréole du décalque autour du plafonnier est plausible, mais ses lignes de contour sont très graphiques.

### 5.4 Verdict et propositions

**Le relief du papier peint est trop fort** de près et le sera encore plus en lumière rasante (lampe tenue près d'un mur). Les valeurs mesurées sont une pente médiane de 7,7°, un p90 de 19,3° et un p99 de 33,9°. Les pentes en x (écart-type 0,18) dominent celles en y (0,095) : ce sont les liserés verticaux, qui sont de l'encre imprimée et n'ont aucune raison d'être en creux.

1. **Carte** (`generate_textures.py:244`), variante B mesurée en mémoire sur le code du dépôt :
   `height = 0.15 * motif - 0.08 * border + 0.04 * weave / 3.0 - 0.9 * gap + 0.6 * lift` (force 2,0 inchangée)
   → pente p50 **2,6°**, p90 **7,1°**, p99 **13,0°** [MESURE]. Les termes qui ont une cause sont gardés : le joint des lés (`gap`) et le bord soulevé (`lift`). Pour comparaison, une force globale de 1,0 donnerait 3,9°, 9,9° et 18,6°, mais garderait le liseré trop fort par rapport au motif.
2. **Régénérer le kit** à partir de la nouvelle carte (`level0_kit_textures.py:157`) **en PNG** (`lossless=True` pour le papier peint, la moquette et les dalles, lignes 121-133), puis rejouer `import_level0_kit(force=True)`.
3. **Paramètre de réglage** :
   - ajouter `float NormalStrength = 1.f` à `FBRSurface` (`BRTypes.h`, à côté de `RoughDetail`) et le transmettre dans `CreateSurface` (`BRAssets.cpp:1718`, `Nrm ? S.NormalStrength : 0.f`) ;
   - ajouter à `M_L0K_PbrWorld` un scalaire `NormalStrength` (`lerp(VN, n, s)` après `BRAxisNormal`, `backrooms_setup.py:1632`), avec une clé `"nstr"` dans `KIT_MATERIALS`, réglée à la même valeur ;
   - valeur : 1,0 après la régénération ; 0,5 en attendant (même effet sur les deux chemins).
4. `D.Wall.Cavity` : **0,5 → 0,2** (`BRLevels.cpp:63`, à recopier sur `D.Pillar`). Dans le kit, passer `ao_power` de 1,6 à 1,0 (`level0_kit_textures.py:157`).
5. Moquette :
   - `D.Floor.Sheen` **0,8 → 0,55** (`BRLevels.cpp:62`) ;
   - `FuzzTint` : défaut du graphe (1,3 ; 1,25 ; 1,15) → **(1,1 ; 1,08 ; 1,0)** (`BRMaterialBuilder.cpp:500`, `backrooms_setup.py:969`), ce qui demande `MATERIAL_VERSION` 10, de toute façon nécessaire ;
   - `Cavity` 1 inchangé (les creux entre les boucles sont réels) ;
   - relief inchangé (rugosité 0,95), à revoir seulement si la lampe rasante le montre trop.
6. Taches :
   - `D.Floor.Stains` **0,65 → 0,4** (`BRLevels.cpp:56`) ;
   - couleur de la tache (0,7 ; 0,7 ; 0,56) → **(0,82 ; 0,8 ; 0,68)** (`BRMaterialBuilder.cpp:378`, `backrooms_setup.py:865`) ;
   - `Grime` du sol **0,3 → 0,2** (`BRLevels.cpp:54`, 5e argument).
   Pour séparer les causes avant de choisir, faire trois captures avec la même graine et la même pose que `vis_0_Qualite_sol` : (a) `Stains = 0` ; (b) `Grime = 0` ; (c) `DecalDensity = 0`.
7. Décalque `CeilingStain` : opacité **0,9 → 0,75** (`BRAssets.cpp:1628`), à juger sur la capture « plafond ».

---

## 6. E — Protocole de validation à l'image

Les captures suivent le modèle existant de `BRAutoTestV416Visual.cpp:2621-2672` : `PlaceLooking`, `SetControlRotation`, `ToggleThirdPerson`, `ShotNamed`. Elles s'enregistrent dans un dossier versionné et sont indexées dans `Docs/VISUELS_v4.17.md`. Chaque image porte ses métadonnées hors HUD : commit, niveau et graine, pose de caméra, champ de vision, résolution interne et de sortie, profil, temps depuis l'arrivée, état de la lumière. Chaque capture est faite **avant et après** le changement, avec la même graine, la même pose et le même profil.

1. **Poses de lumière** (joueur à la 3e personne, caméra derrière, puis de 3/4 en déplaçant le joueur de 90° comme pour la vue « côté » de la v4.16) :
   - L1 **lumière neutre** : Level 4 (néons blancs), sous un plafonnier, à 1,5 m de la caméra ;
   - L2 **néon chaud** : Level 0, graine 4605, sous un panneau allumé, à 1,5 m puis à 4 m ;
   - L3 **lampe** : zone sombre du Level 0, lampe allumée, faisceau rasant sur le flanc ;
   - L4 **coupure** : même pose que L2, au même instant de phase de la coupure, lampe éteinte puis allumée ;
   - L5 **distance** : 12 m et 25 m, pour la transition des LOD 1 et 2 et le scintillement.
2. **Vue de face et visière** : la caméra de 3e personne ne montre que le dos.
   - **solo** : un Skin-Stealer déguisé (même `SK_Hazmat`, même MID) face au joueur, sous L2 et sous la lampe. C'est un indicateur seulement, pas un remplacement de l'essai réseau ;
   - **réseau** (2 instances sur le même PC, banc v4.16) : l'hôte regarde le client placé face à lui à 2 m, puis le client regarde l'hôte. Sur chacun : capture, `DescribeSections` (parent `M_BR_Gear`, `NormalTex` et `MaskTex` attendus) et `GetMaterialProblems()` vide. Également un **client qui rejoint en cours de niveau** : le coéquipier doit être visible dès son apparition (PSO).
3. **Mouvement** : courte vidéo (5 s) de marche, course, accroupissement et demi-tour sous L2 et L3. On y cherche le scintillement des reflets, le glissement du relief et la continuité aux coutures quand les coudes et genoux plient.
4. **Profils** : Performance, Qualité, RTX fluide et Cinématique (scène ray tracée simplifiée). En Cinématique, regarder la combinaison dans un reflet (carrelage des Poolrooms ou flaque). Mesurer le coût de Lumen Reflections avec ProfileGPU, 4 joueurs dans le champ, avant et après.
5. **Parité des deux graphes** : même pose, une fois avec le matériau importé et une fois avec `-BRRuntimeMaterials` (graphe C++). Écart visuel nul attendu, avec un PSNR de la région de la combinaison ≥ 35 dB comme indication.
6. **Critères chiffrés indicatifs** (le jugement reste artistique) :
   - sur la région de la combinaison sous L2, le rapport `(p99 − p50)/p50` de la luminance doit monter (0,52 aujourd'hui sous néon blanc) sans écrêtage (< 1 % des pixels ≥ 0,98) ;
   - aucune ligne visible aux raccords d'îlots à 1,5 m ;
   - la visière est plus sombre que le PVC de face, et son bord s'éclaircit en vue rasante.
7. **Paquet Shipping** : `check_package.py --mode release` (contenu : `M_BR_Gear` et les deux textures), lancement avec `-BRAutoTest -BRAutoTestStrict`, test « combinaison : matériaux » réussi. Puis les mêmes captures L2, L4 et la vue réseau **dans le paquet**.
8. **Level 0** : mêmes poses que `vis_0_Qualite_{mur_x,mur_y,sol,plafond,vue}`, plus une lampe rasante à 50 cm d'un mur. Il faut une paire avant/après où un linteau en boîte touche un module de mur, pour vérifier que les deux chemins se raccordent.
9. **Corriger le test v4.8** : sa capture `V48_combinaison_*` doit montrer le personnage, c'est-à-dire être prise après le carton de niveau et hors de l'eau.

---

## 7. Limites de ce travail

- Unreal n'a pas été lancé. Aucune image du nouveau matériau n'existe, et l'effet réel de la normale et de l'ORM sur la combinaison n'a pas été vu dans le jeu. Les valeurs du § 2.2 sont des points de départ physiques, à régler à l'image.
- Les statistiques de pente et de rugosité viennent du prototype et du code du dépôt rejoué en mémoire. Elles décrivent les textures, pas l'image rendue.
- La visière n'apparaît dans aucune capture disponible.
- L'hypothèse du GLB d'origine (§ 1.5) n'a pas pu être vérifiée : le fichier n'est ni dans l'arbre ni sur le disque examiné.
- Les sources des taches au sol (§ 5.3) sont des hypothèses, à séparer par les trois captures proposées.

---

## 8. Réalisation (v417-suit)

Branche `v417-suit` (arbre `wt/suit`, base `1e67bb1`), même agent. Unreal n'a pas été lancé et aucun fichier partagé n'a été touché (`BRAssets.*`, `BRMaterialBuilder.*`, `backrooms_setup.py`, `essential_packages.txt`) : l'orchestrateur les a intégrés dans `v417-jeu`, lus ici en lecture seule pour aligner les noms. Les aperçus regardés sont dans le scratchpad de la session, hors dépôt.

### 8.1 Fichiers livrés

| Fichier | Rôle |
|---|---|
| `Tools/Blender/export_hazmat_uv.py` | étape 0 : lit `SK_Hazmat.fbx` (empreinte comparée au registre avant, revérifiée après), écrit les masques UV |
| `RawAssets/Derived/Hazmat/hazmat_uv_islands.png` | identifiant d'îlot, gris 16 bits (combinaison 1…83, visière 65535) |
| `RawAssets/Derived/Hazmat/hazmat_uv_edges.png` | coutures (255) et bords libres (128), 1 px |
| `RawAssets/Derived/Hazmat/hazmat_uv_islands.json` | par îlot : matériau, faces, os dominant, px/mm, hauteur relative, miroir, boîte UV ; **chaque côté de couture avec l'îlot d'en face** (`seams`, 3 699 lignes) ; empreintes des deux PNG et du FBX |
| `Tools/derive_hazmat_maps.py` | étapes 1 à 10 (numpy + Pillow, aucun hasard), contrôles qui échouent, `--size`, `--crinkle`, `--check`, `--preview` |
| `RawAssets/Derived/Textures/T_Hazmat_Suit_N.png` | normale tangente 2048², DirectX, plate hors îlots |
| `RawAssets/Derived/Textures/T_Hazmat_Suit_ORM.png` | R occlusion, G rugosité absolue, B métal (0), 2048², linéaire |
| `RawAssets/Derived/Textures/T_Hazmat_Suit_maps.json` | provenance : empreintes de l'atlas, des masques et du FBX, paramètres, mesures, empreintes des pixels et des fichiers |
| `RawAssets/Derived/.gitattributes` | `* -text` : aucune conversion de fin de ligne sur ces données, dont les empreintes sont écrites dans les JSON |
| `Tools/Tests/test_material_sync.py` | contrôle statique du § 4.4 (9 tests) |
| `Source/Backrooms/Private/BRAutoTestV48.cpp` | test « combinaison : matériaux » du § 4.5, point 4 |

Commits : `d2049ec` (étape 0), `ebe559a` (cartes), `bc79f2c` (test statique), `347adac` (test v4.8), `69fa579` (copie de ce document, identique au blob de `a9ccf1c`), puis cette section. **À la fusion dans `backrooms-v4.17`**, ce fichier arrive des deux côtés (ajout/ajout) : garder la version de `v417-suit`, qui est celle de `a9ccf1c` plus la section 8.

### 8.2 Commandes exécutées

```
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --factory-startup --python Tools/Blender/export_hazmat_uv.py -- --size 2048
python Tools/derive_hazmat_maps.py --preview <scratchpad>      (écrit les cartes : 65 s)
python Tools/derive_hazmat_maps.py --check                      (recalcule et compare : 110 s, « controles : OK »)
python -m unittest Tools/Tests/test_material_sync.py            (depuis Backrooms/)
```

- Export Blender : 20 s ; deux exécutions donnent des fichiers identiques octet pour octet (sha256). Hauteur lue 180,0 cm, unité 1 cm.
- `--check` après la dernière écriture : pixels identiques aux PNG livrés, provenance à jour. Le script échoue (code 1) **avant toute écriture** si un contrôle échoue.
- `test_material_sync.py` : sur `v417-suit` seul, 6 réussis et 3 échecs attendus (classe `TestGear` : `M_BR_Gear` absent de `MATERIALS`, `AssetNames`, `GEAR_TEXTURES`, etc.). Sur une copie fusionnée dans le scratchpad (sources de `v417-jeu` + `RawAssets/Derived` de `v417-suit`) : **9/9**. Sur l'arbre principal (avant intégration) : `TestMasterSync` réussit, `TestGear` échoue. Deux mutations ont été vérifiées : renommer `RoughMapWeight` dans le graphe Python, ou permuter deux entrées de `Names[]`, fait échouer le test.
- Pas de compilation possible ici : le C++ a été relu à la main contre les en-têtes d'Unreal 5.8 installés (`GetScalarParameterValue` et `GetTextureParameterValue` sont `const` sur `UMaterialInterface`, `FHashedMaterialParameterInfo` vient de `Materials/MaterialParameters.h`, inclus par `MaterialInterface.h`) ; aucun nom local ne masque un membre d'`ABRAutoTest` ni d'`AActor`, et les nouveaux symboles de l'espace anonyme (`V48Scalar`, `V48TextureName`, `FV48GearSlot`, `V48GearSlots`) n'existent nulle part ailleurs (build unifié).

### 8.3 Mesures (cartes livrées, lues comme le jeu : 8 bits, z reconstruit comme en BC5)

| Mesure | Prototype (§ 2.5) | Livré |
|---|---|---|
| Pente dans les îlots p50 / p90 / p99 / max | 0,82 / 2,6 / 7,3 / 26,7° | **0,32 / 1,31 / 5,56 / 10,9°** |
| Pente du PVC propre p99 | — | 2,31° |
| Pente dans la salissure p99 | — | 4,77° |
| Pli aux raccords p50 / p90 / p99 / max (4 249 points, 108 non mesurables) | non mesuré | **0,62 / 1,19 / 3,31 / 6,9°** (plancher de quantification 0,45°) |
| Écart de hauteur aux raccords p50 / p99 | — | 0,000 / 0,29 mm (bande peinte d'un seul côté, sans pente : invisible) |
| Fentes (203 points, coupures de 2 à 3 px dans un même panneau) | — | pli p99 8,0°, max 8,9° (compté à part) |
| Rugosité dans les îlots p5 / p50 / p95 | 0,33 / 0,37 / 0,64 | **0,337 / 0,365 / 0,690** |
| Rugosité médiane : PVC, bottes, gants, bandes, semelles | — | 0,361 ; 0,561 ; 0,502 ; 0,549 ; 0,78 |
| Visière min / médiane / max | — | 0,051 / 0,067 / 0,149 |
| Occlusion min / p5 / p50 | — | 0,68 / 0,86 / 1,0 |
| Salissure > 0,5 ; bandes noires ; semelles | 5,8 % ; 1,2 % ; — | 3,4 % ; 2,4 % ; 2,5 % |
| Îlots : PVC, gants, bottes, semelles | — | 77, 2, 2, 2 |

Le « pli aux raccords » est le contrôle des marches : pour chaque arête de couture entre deux grands îlots, aux quarts de l'arête, la pente de la normal map livrée est lue de chaque côté (le long de l'arête et vers l'intérieur, à 1,5 et 3 px puis extrapolée au bord). La même surface vue des deux côtés a la même pente le long de l'arête et des pentes opposées en travers ; l'écart est l'angle du pli que dessinerait le raccord. Seuil : p99 ≤ 4°. Vérifications du contrôle : une normale plate donne 0,45° (quantification) ; l'ancienne crête de couture du § 2.4 donnait 3,1° en médiane.

### 8.4 Ce que j'ai vu sur les cartes, et corrigé

Aperçus regardés : normale ombrée en lumière rasante (haut et gauche, contraste ×3 et ×4), atlas éclairé en rasant (GGX avec la rugosité de la carte), rugosité, occlusion, masques, gros plans du torse et des pastilles, de la capuche, du bas des jambes et des bottes, avant et après chaque correction.

1. **Bord UV en dents de scie.** Les bords d'îlots du modèle suivent les arêtes des triangles (1 à 5 px d'amplitude, période 10 à 20 px) : la bande de couture, mesurée depuis ces lignes, zigzaguait (déjà dans le prototype). Les bandes et les ourlets suivent maintenant le **contour lissé** de l'îlot : distance signée tirée du logit d'un flou de l'îlot (σ = 8 px). Le flou ne voit que l'îlot, comme l'autre côté de la couture en 3D : la distance est opposée des deux côtés, et le relief se raccorde. Les lignes sont maintenant lisses et régulières.
2. **Crête au raccord.** La crête de 0,12 mm et de 3 mm de large (§ 2.4) posée sur la couture est plus fine que la résolution : elle dessinait un pli de 2 à 4° exactement sur le bord UV, de chaque côté. Supprimée : la bande est un plateau de 0,30 mm sur 9 mm, qui retombe à 0 à 12 mm de chaque côté. Ses deux bords se lisent comme un ruban d'environ 2 cm à cheval sur la couture.
3. **Anneaux de bande au milieu des panneaux.** Le modèle a 61 fragments d'îlots (1 à 29 faces, 2 835 px) et des trous de 1 à 2 px : chacun faisait un anneau de bande. L'îlot est fermé (3 px) avant le flou ; les fragments prennent la densité du panneau qui les entoure. Les grands îlots sont à plus de 16 px les uns des autres (mesuré) : la fermeture ne les relie jamais.
4. **Grumeaux de bande à la jonction du masque.** Les arêtes « ourlet » (vers le masque) et « couture » alternent sur quelques millimètres : le choix tranché du plus proche faisait des grumeaux. La nature du bord est maintenant une part pondérée (flou de 6 px), et le pli p99 de la seule bande aux raccords est passé de 5,1° à 1,0°.
5. **Pastilles en puzzle, plaques sur la boue.** Le seuil uniforme sur l'écart type local ne prenait qu'une partie des pastilles (motif gris imprimé) et des bandes de capuche, et quelques taches aux chevilles. Les bandes noires sont désormais des **composantes entières**, retenues si elles sont noires et neutres en médiane et assez unies. Les pastilles, les poignets de manche et les bandes de capuche sont complets. Aucune composante de boue n'est retenue : il n'y a plus de plaque sur la salissure. Les poignets de cheville (bruns et striés dans l'atlas) ne sont levés sur aucune jambe : ils restent en rugosité de salissure, sans relief.
6. **Grumeaux au front de la salissure.** Au-dessus de la boue, des taches olive restent aussi saturées que le PVC et échappent au masque de salissure. Le passe-bande en faisait des creux allant jusqu'à −0,8 mm. Corrections : aucun pli à moins d'environ 5 cm d'une salissure, et hauteur des plis écrêtée en douceur à ±0,35 mm. Les plis peints restent entre −0,21 et +0,20 mm (p5 / p95). Il reste un léger relief de plis plus haut sur les tibias, qui se lit comme des plis du tissu.
7. **Bruit.** Avec σ = 1,5 ou 2 px, la trame horizontale de l'albedo passait dans le relief, en stries fines sur tout le PVC. Le passe-bande vaut maintenant **σ 3 / 18 px** : les plis peints dominent, et il ne reste qu'un léger moutonnement de faible amplitude. L'option des rides (`--crinkle 1`) a été revue et ajoute encore le grain du JPEG et de la trame : elle reste désactivée.
8. **Restes, à juger dans le jeu.** Une encoche de 2 à 3 mm au bord de deux pastilles, là où le modèle a une fente. Un bord de pastille légèrement polygonal, qui suit l'albedo. La semelle est entourée d'un ourlet : ses arêtes ne sont pas soudées à la botte dans le FBX, et ce bord se lit comme une arête arrondie.
9. **Rugosité et occlusion.** Le PVC est à 0,36 ; les creux des plis sont plus rugueux et les crêtes plus lisses. Les bandes de couture font des rubans à 0,46 le long des coutures, les gants sont à 0,50, les bottes à 0,55 avec la boue à 0,78, les semelles à 0,78. La visière va de 0,05 au centre à 0,15 au bord, et le fond est bourré. L'occlusion n'assombrit que les creux de plis et la salissure (minimum 0,68).

### 8.5 Écarts au cahier des charges

- **Bandes et ourlets** : profil calculé sur le contour lissé et non sur la distance brute aux lignes, sans crête (points 1 et 2). Le caoutchouc moulé ne reçoit aucune bande (gant contre gant, botte contre botte, botte contre semelle). La bande couvre les deux côtés d'une jonction PVC / caoutchouc (le § 2.4 l'ôtait côté gant, ce qui laissait une demi-bande). La jonction avec le masque est un ourlet.
- **Normale** : différences d'un seul côté au bord de l'îlot au lieu de pentes mises à zéro. La pente au raccord est ainsi la vraie pente, ce que mesure le contrôle des raccords.
- **Bandes noires** : détection par composantes (point 5) ; poignets de cheville non retenus.
- **Plis** : σ 3 au lieu de 1,5, écrêtage à ±0,35 mm, poids de saturation, exclusion d'environ 5 cm autour de la salissure.
- **Semelles** : hauteur relative maximale < 0,05 au lieu de 0,03 (valeur mesurée : 0,032).
- **Contrôles** : un contrôle des raccords est ajouté (pli p99 ≤ 4°). Celui de la « sortie identique d'une exécution à l'autre » passe par `--check`, qui compare les pixels recalculés à ceux livrés et à leur empreinte.
- **Étape 0** : le JSON décrit aussi chaque côté de couture et l'îlot d'en face (`seams`). Il compte 3 699 arêtes de couture et 491 bords libres, contre 3 703 et 487 dans le prototype : quatre côtés en double sur la même arête sont désormais comptés comme bords libres.
- **Test v4.8** : en plus du § 4.5, il vérifie sur la visière `MaskTex = T_Hazmat_Suit_ORM` et `RoughMapWeight = 1`, et l'absence de métal sur le masque.

### 8.6 Registre des ressources protégées : non enregistrées, et pourquoi

Les cartes ne sont **pas** ajoutées à la section « derived » de `RawAssets/protected_assets.json`. Dans `Tools/protect_assets.py`, `record()` n'enregistre qu'un fichier qui correspond à `PROTECTED_PATTERNS`. Ces motifs ne couvrent que les dérivés de ressources fournies, que l'on ne peut pas refaire sans les originaux de `Tools/SourceModels` (non versionnés) : les 83 entrées actuelles y correspondent toutes. Les textures générées par les scripts du dépôt (`generate_textures.py`, `_R.png` de `generate_roughness.py`) n'y sont pas. Nos cartes se régénèrent à partir de l'atlas enregistré et de scripts versionnés. Les enregistrer aurait demandé un motif de plus dans `protect_assets.py`, `may_write` aurait refusé chaque régénération sans `--force`, et `check` aurait signalé chaque régénération comme « MODIFIE ». La provenance est donc dans `T_Hazmat_Suit_maps.json`. Le script vérifie l'empreinte de l'atlas contre le registre avant de commencer, et `test_gear_maps_derive_from_the_protected_atlas` vérifie que la provenance cite l'atlas protégé actuel et les PNG livrés.

### 8.7 Ce que l'orchestrateur doit faire dans les fichiers partagés (rappel du § 4.1-4.3, corrigé)

Lu dans `v417-jeu` (`17585a1`) : `EBRMasterMaterial::Gear`, `BuildGear`, `Names[]` avec `static_assert`, `EParent::Gear`, `AssetNames`/`V2Params` (`RoughMapWeight`), `FSlotStyle` et les trois slots, le branchement `NormalTex`/`MaskTex` avec signalement, le repli `M_BR_Gear` → `M_BR_Mesh` signalé, l'ancien maître signalé dans le paquet, le repli de l'éditeur sur `Derived/Textures` et `_ORM` linéaire, `MATERIAL_VERSION = 10`, `MATERIALS`/`USAGES`, `DERIVED_SUB`/`GEAR_TEXTURES`, l'import `_ORM` avec texture composite, `build_gear_material`, l'import des dérivés avant `build_materials`, `validate` et `essential_packages.txt` sont **faits**. Le graphe C++ et le graphe Python sont conformes au § 2.1. Reste :

1. **Fusionner `v417-suit`** : `RawAssets/Derived/**` (avec son `.gitattributes`), les deux outils, `test_material_sync.py`, `BRAutoTestV48.cpp` et ce document (voir 8.1 pour l'ajout/ajout). Puis lancer `python -m unittest Tools/Tests/test_material_sync.py` (9/9 attendu) et `python Tools/derive_hazmat_maps.py --check`.
2. **Rejouer l'import dans l'éditeur** avant toute cuisson (`MATERIAL_VERSION 10` le déclenche) et contrôler `backrooms_setup.validate()`. L'import des cartes repose sur deux préalables : `_N` doit être importée avant `_ORM` pour la texture composite (`run()` trie déjà ainsi), et `T_NoiseLF` doit être linéaire (sinon `M_BR_Gear` n'est pas construit, ce qui est voulu).
3. **Facultatif, recommandé** (`backrooms_setup.py`, `import_textures`/`validate`) : étiquette `BRSourceSha256` posée à l'import et comparée au sha256 du PNG de `RawAssets/Derived/Textures`. Aujourd'hui, une carte régénérée sans changement de `MATERIAL_VERSION` reste l'ancienne dans l'éditeur sans que la validation le dise. Il faudrait aussi vérifier une taille d'au moins 2048 (§ 4.3).
4. **Facultatif** (`fbx_skeletal_options`, § 2.6) : `normal_import_method = FBXNIM_IMPORT_NORMALS` et `normal_generation_method = MIKK_T_SPACE` explicites pour le prochain import de `SK_Hazmat`. Il ne faut **pas** `IMPORT_NORMALS_AND_TANGENTS` : le FBX de la combinaison n'a pas de tangentes. Pas de changement de `VERSION`.
5. **Facultatif** (`BRAssets.cpp`, `DescribeSections`) : afficher `NormalTex`, `MaskTex`, `NormalStrength` et `RoughMapWeight` pour un parent `M_BR_Gear`. Le test v4.8 écrit désormais lui-même une ligne par slot avec ces valeurs.
6. **À l'image** (§ 6, banc v4.17) : poses L1 à L5, vue réseau de face pour la visière, profils Performance et Cinématique, paire `-BRRuntimeMaterials`. À regarder en particulier : les bords des bandes de couture, les pastilles, le bas des jambes et le scintillement. Si le relief paraît trop fort, baisser `NormalStrength` dans le style `HazmatSuit` (`BRAssets.cpp`), sans régénérer les cartes.

### 8.8 Limites

- Le rendu dans le jeu n'a pas été vu (Unreal n'a pas été lancé) : les aperçus sont un éclairage rasant appliqué à l'atlas à plat, pas la combinaison déformée ; la continuité aux raccords est mesurée, non vue en 3D.
- Le test v4.8 et le test statique n'ont pas été exécutés dans le moteur ; le C++ n'a pas été compilé.
- Le contrôle des raccords suppose des UV localement conformes (même échelle dans toutes les directions) ; les fentes et les fragments de moins de 30 faces en sont exclus et comptés à part.
