# Audit technique v4.4 → v4.5

Audit fait le 5 octobre 2026 sur la branche `backrooms` (commit `cac153a`), à partir des sources, de la configuration et des
fichiers de `RawAssets/` et `Tools/SourceModels/`. **Unreal Engine n'est pas disponible dans l'environnement de travail**
(conteneur Linux sans GPU) : rien n'a été compilé, importé ni lancé dans le moteur. Les constats viennent de la lecture du
code et de l'analyse des fichiers sources avec Blender 5.0 (module Python `bpy`).

## Version et configuration réelles

| Point | Constat |
|---|---|
| Moteur | `Backrooms.uproject` : `"EngineAssociation": "5.8"`. Aucune migration nécessaire. |
| RHI | `DefaultGraphicsRHI_DX12`, shaders `PCD3D_SM6` uniquement (SM5 retiré). |
| Ray tracing | `r.RayTracing=True`, `r.Lumen.HardwareRayTracing=True`, `r.SkinCache.CompileShaders=True`. Ombres ray tracées désactivées (`r.RayTracing.Shadows=False`) : ombres virtuelles partout. |
| Lumen | GI et reflets Lumen, `TraceMeshSDFs=1`, reflets de premier plan de la translucidité et réfraction ray tracée activés pour le projet. |
| Réglages en jeu | Qualité 0–4 (`scalability`), RT matériel oui/non, hit lighting forcé dès « Épique » (coûteux), échelle de rendu TSR 50–100 %. |
| Contenu Unreal | `Content/Backrooms/` n'est pas versionné : tout est importé depuis `RawAssets/` par `backrooms_setup.py` à l'ouverture de l'éditeur. |

## Principales limites

### Rendu
1. **Mode de rendu invisible pour le joueur** : si la carte ou le RHI ne permet pas le ray tracing (DX11, GPU sans RT),
   Lumen repasse en logiciel sans le dire. Le menu affiche « RAY TRACING ACTIVÉ » quoi qu'il arrive.
2. **Hit lighting** forcé en qualité Épique, alors que c'est le réglage le plus coûteux des reflets.
3. **Pas de profils** cohérents (performance / qualité / cinématique) ; les options qui demandent un redémarrage
   (`r.RayTracing`, RHI) ne sont pas distinguées de celles qui s'appliquent tout de suite.
4. **Matériaux** : rugosité et métal sont des scalaires par surface, modulés par un bruit. Pas de texture de rugosité.
   Les flaques sont un seuil net sur un bruit flou : des taches aux bords lisses, sans liseré mouillé.
5. **Poolrooms** : le carrelage et sa normal map (512 px) viennent de la scène fournie ; la normal map est fine (5 Ko car le
   motif se compresse bien, mais les joints ont leur biseau). Ce qui manque : aucun contact avec l'eau (pas de ligne
   mouillée sur les murs des canaux, les piliers et les rebords).

### Modèles
1. **Entités en pièces rigides** (Faceling, Partygoer, Skin-Stealer, Hound) : chaque membre est un maillage séparé avec
   une « couverture » rentrée de 6 mm aux articulations. Aux grands angles (course, bras tendus), les coudes, genoux et
   épaules se cassent. Or **les quatre originaux ont un squelette et des poids de peau** :

   | Original | Os | Maillages | Poids |
   |---|---|---|---|
   | `faceling.glb` | 16 | 1 (1 387 sommets) | oui |
   | `partygoer.fbx` | 68 (CC3) | 2 (corps + ballon non riggé) | oui |
   | `skin_stealer.usdz` | 53 | 8 | oui |
   | `hound.blend` | 34 | 9 | oui |
   | `bacteria_recreation.blend` | 0 | 2 | non |
   | `peppered moth.obj` | 0 | 1 (scan) | non |

   → Un maillage skinné dérivé, avec la géométrie et les poids d'origine, est possible sans rien remodeler.
2. **Wretch** : six primitives procédurales (`SM_Wretch_*`, `generate_models.py`) : l'assemblage se voit.
3. **Clump** : huit faisceaux de bras rigides, sans vraies mains ; les jonctions avec la masse se voient.
4. **Smiler** (v4.4) : un seul maillage statique ; l'émission (60 × le facteur de clignement) n'est pas calibrée sur
   l'exposition des niveaux.

### Animations
1. Marche sinusoïdale : la phase avance avec la vitesse mais l'amplitude du pas n'est pas liée à la distance parcourue,
   d'où un glissement des pieds, surtout au pas.
2. Pas de pose propre aux états **détection**, **attaque** (armé, frappe), **récupération** : une attaque n'enlève que des
   points de vie, sans geste.
3. Pas d'ajustement des pieds au sol (marches de 34 cm des Poolrooms, rampes).
4. Toutes les entités sont animées à chaque image, même à 40 m derrière un mur.

### Imports et protection des modèles fournis
1. Lancer `import_user_models.py` sans argument régénère **tous** les dérivés (c'est arrivé pendant la v4.4 et a dû être
   annulé avec git). Aucun garde-fou, aucune empreinte des fichiers.
2. Si un dérivé manque, le C++ remplace silencieusement le modèle fourni par une forme procédurale (boîtes) : rien ne le
   signale.
3. `import_skeletal` retrouve un maillage mal nommé par sous-chaîne : avec plusieurs `SK_*`, il pourrait en renommer un
   autre.
4. Les originaux (`Tools/SourceModels/`) ne sont pas versionnés (`.gitignore`) : il faut les conserver à part.

### Performances
1. Streaming : jusqu'à 2 chunks construits par image, chacun d'un bloc. Un chunk riche (Poolrooms, bureaux) peut
   provoquer une saccade.
2. Le test automatique mesure les FPS moyens, la pire image et les temps GPU / jeu / rendu, mais ni la mémoire, ni la
   résolution interne, ni le mode de rendu réellement actif, ni les percentiles.

## Ce que la v4.5 traite

| Domaine | Changement |
|---|---|
| Modèles fournis | Manifeste d'empreintes SHA-256, scripts non destructifs (`--force` pour écraser un dérivé protégé), avertissement en jeu quand un repli remplace un modèle fourni |
| Rig | Maillages skinnés `SK_*` dérivés des originaux (géométrie et poids d'origine) pour le Faceling, le Partygoer, le Skin-Stealer et le Hound, pilotés par le même moteur d'animation procédurale ; repli automatique sur les pièces rigides |
| Reconstruction | Wretch (humanoïde décharné skinné, peau cuite), Clump (bras et mains complets fusionnés à la masse, bouche), Smiler (profondeur, émission calibrée) |
| Animation | Poses de détection, d'armé, de frappe et de récupération ; pas liés à la distance parcourue ; pieds posés au sol par traces ; animation moins fréquente au loin |
| Rendu | Profils Performance / Qualité / Cinématique, mode de rendu réel affiché (RHI, SM, RT matériel, résolution interne), hit lighting réservé au profil Cinématique, ombres ray tracées seulement pour la lampe en Cinématique |
| Décors | Plinthes moulurées, flaques à contours naturels avec liseré humide, ligne d'eau sur le carrelage des Poolrooms |
| Mesures | Rapport du test automatique enrichi (mémoire, résolution interne, mode de rendu, percentiles) ; construction des chunks limitée dans le temps |
