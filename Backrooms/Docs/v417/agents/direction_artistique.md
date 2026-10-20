# v4.17 : direction artistique et environnement (Level 0, Poolrooms)

**Rôle** : agent installé « Level Designer » (`C:\Users\Fristick\.claude\agents\level-designer.md`), lancé comme sous-agent générique avec ce rôle. Mission : direction artistique et environnement.
**Base** : branche `backrooms-v4.17`, tête `1e67bb1` (banc visuel v4.17, sans changement d'image). Les captures examinées sont celles du code v4.16 (`c07eddb`).
**Date** : 20 et 21 octobre 2026.
**Périmètre d'écriture** : ce fichier seulement. Aucun autre fichier du dépôt n'a été modifié.

## 0. Méthode, preuves et limites

- J'ai **vu** les images avec l'outil d'affichage, en entier puis en recadrages agrandis. Les recadrages et les mesures ont été produits par un script local dans le dossier temporaire de la session, hors du dépôt. Les références Steam n'ont pas été copiées dans le dépôt.
- **Mesure de luminance** : chaque pixel sRGB est linéarisé, puis Y = 0,2126 R + 0,7152 G + 0,0722 B. Je donne la médiane (p50), les centiles p1, p5, p95 et p99, et la part des pixels sous Y = 0,01 (« très sombre »). Les zones sont des rectangles (x0–x1, y0–y1) en pixels de l'image d'origine : 1366 × 768 pour nos captures, 1920 × 1080 pour les références. Le bandeau DEV (y < 40) est exclu.
- **Limites** :
  - Les références sont des JPEG promotionnels, peut-être étalonnés. Je ne compare donc que des **rapports** entre surfaces (mur / sol / plafond, sombre / clair), jamais des valeurs absolues.
  - Nos captures sont en profil Qualité : rendu 1093 × 614 → 1366 × 768 en TSR, Lumen matériel, MegaLights, VHS et grain actifs (sauf `vue_sansvhs`). Ce sont des images fixes.
  - Je ne conclus rien sur le mouvement, le scintillement, l'exposition dans le temps ni l'animation.
- **Étiquettes** :
  - **[Observé]** : vu sur l'image (fichier et zone).
  - **[Mesuré]** : chiffre calculé sur l'image.
  - **[Source]** : constat lu dans le code ou les scripts.
  - **[Hypothèse]** : cause ou effet supposé, à vérifier.
  - **[Non concluant]** : l'image ne permet pas de trancher.
- **Images examinées** :
  - Références : `planche_ref.jpg`, `ref_01`, `ref_03`, `ref_05`, `ref_09`, `ref_11`, `ref_15` et `ref_16`. Les autres n'ont été vues qu'en vignette sur la planche ; je n'en tire aucun constat.
  - Banc v4.16 (`final416/visual`), profil Qualité :
    - Level 0 : poses `vue`, `vue_sansvhs`, `dos`, `x10`, `mur_x`, `mur_y`, `sol`, `plafond`, `fosses`, `coupure`, `lampe`, `retour_03`, `retour_15`, `retour_40` et `mur_smiler`.
    - Niveau 37 : `vue`, `dos`, `x10`.
    - Niveau 1 : `vue`, `dos`, `x10`.
    - Niveau 2 : `vue`, `dos`, `x10`, `mur_x`, `mur_y`, `sol`, `plafond`.
    - Niveau 6 : `vue`, `x10`.
    - Profil Performance : Level 0 (`vue`, `x10`, `fosses`) et Niveau 37 (`vue`).
  - `final416/v416vis` :
    - les 5 vues `v416_kit_*` ;
    - toutes les entités en `3p_face`, `3p_cote` et `1p_menace`, en planches, plus en grand `Partygoers_3p_face`, `Partygoers_1p_menace` et `Bacteria_1p_menace` ;
    - l'interface : `v416_fenetre_169_max_personnage`, `v416_fenetre_petite_1280x720_personnage`, `v416_hud_1920x1080_fr_personnage` et `v416_hud_1920x1080_fr_jeu`.
  - `final416/full` : `L00_E_Bacteria`, `L00_E_Smilers`, `L00_fosses_lampe`, `L00_fosses_chute`, `L00_nuit`.
  - `final416/chapitre` : `v413_chapitre_porte` (porte de service, code v4.16).
  - `hlcmpc` : `cmp_lit.png`, ainsi que `visualchl1/vis_37_Cinematique_dos` et `_x10`.

## 1. Constats par environnement

### 1.1 Level 0 (salles)

**Écarts observés sur nos captures**

1. **Les murs sont la surface la plus claire de l'image.** [Observé, Mesuré] Sur `vis_0_Qualite_vue.png` :
   - face éclairée du pilier gauche (x 395–475, y 215–595) : médianes 0,41 en haut, 0,57 au milieu et 0,47 en bas ;
   - mur gauche (x 0–120, y 210–600) : 0,25 ;
   - mur du fond (x 620–730, y 345–435) : 0,44 ;
   - moquette au premier plan (x 400–1366, y 620–768) : 0,23 ;
   - plafond entre deux plafonniers (x 700–790, y 200–300) : 0,09.

   Les murs atteignent donc 1,1 à 2,5 fois la moquette. Le papier peint vert-jaune pâle, très contrasté entre bandes et liserés, donne l'impression d'une « boîte claire » uniforme (x 0–390, y 210–600). `vis_0_Qualite_dos.png` montre la même chose.
2. **Peu de noir dans les salles éclairées.** [Mesuré]
   - `vue` : 1,9 % des pixels sous 0,01 (p5 = 0,022, p50 = 0,19).
   - `dos` : 0,3 %.
   - En revanche, `vis_0_Qualite_x10.png` montre une zone sans plafonnier très juste (x 1000–1366, y 250–600 : médiane 0,007 ; 18 % de l'image sous 0,01), avec une seule lampe au fond. C'est **notre meilleure image du Level 0** et elle est proche de l'intention recherchée. Le profil Performance garde ce noir (même zone : 0,008).
3. **Plafonniers.** Au moins 8 plafonniers sont visibles dans `vue`. Leur nombre et leur place sont des règles de jeu et ne changent pas.
   - [Observé] Le diffuseur est un aplat blanc sans structure : `vis_0_Qualite_plafond.png` (x 540–700, y 120–320) donne un plateau à p50 = 0,90, et le diffuseur central de `vue` (x 545–665, y 95–165) a 11,6 % de pixels ≥ 250.
   - [Observé] Le cadre porte une texture tachée, comme marbrée, qui évoque du béton plutôt que de la tôle peinte (bords du diffuseur sur `plafond`).
   - [Source] En jeu, le luminaire est `SM_LightPanel` (BRChunk.cpp, l. 1638). Son emplacement `Frame` utilise `T_Grime` (BRAssets.cpp, l. 87).
   - [Source] Le kit Blender possède un diffuseur à prismes (`T_L0K_Diffuser_*`, level0_kit_textures.py, fonction `diffuser`) qui n'est pas celui que l'on voit en jeu.
4. **L'ossature du plafond n'est pas alignée sur les luminaires.**
   - [Observé] Sur `vis_0_Qualite_dos.png` (x 560–870, y 40–135), deux traverses arrivent au milieu du grand côté du luminaire central (x ≈ 660 et ≈ 820) : il n'occupe pas des cases entières de l'ossature.
   - [Source] Un luminaire est placé au centre de sa cellule de 350 cm, décalé au hasard de ±42 cm (BRWorld.cpp, l. 4894–4896). L'ossature de 61 cm est projetée dans l'espace monde (BRLevels.cpp, l. 58 : 122 cm par répétition). Le décalage est donc systématique.
5. **Papier peint de près.**
   - [Observé] `vis_0_Qualite_mur_x.png` (zoom x 400–700, y 250–450) : les chevrons paraissent nettement **embossés**, avec un biseau clair en haut et sombre en bas. Des stries verticales fibreuses donnent un aspect de bois ou de tissu brossé. `mur_y` est plus plat (l'éclairage vient d'un autre côté).
   - [Mesuré] Le motif se répète tous les 67 px sur `mur_x`, soit 7,6 cm à 80 cm de distance avec un champ de 88° (1 px ≈ 1,13 mm). C'est conforme à la source : 7 colonnes par lé de 53 cm.
   - [Mesuré] **Aucun raccord de lé n'est repérable** (pas de creux distinct au pas de 469 px). Le lé de 53 cm ne se lit donc pas.
   - [Source] Le joint fait 1 px de texture (≈ 0,5 mm) et le ton varie de ±1,2 % par lé (generate_textures.py, l. 180–241).
6. **Plinthes et chambranles plats.**
   - [Observé] Plinthe en bande sombre sans arête éclairée : `v416_kit_poteau_droit.png` (x 300–1000, y 355–415) et `v416_kit_angle.png`.
   - [Observé] Chambranle en planche plate : `v416_kit_jambage.png` (x 660–715, y 0–560).
   - [Source] Plinthe de 12 × 1,5 cm, chambranle de 9 × 2,5 cm, même matériau « Baseboard » (build_level0_kit.py : `BASE_H`, `BASE_D`, `SM_L0_DoorCasing` en v4.16). Le fichier est en cours de modification par un autre agent : je cite les identifiants plutôt que les numéros de ligne.
   - **Travail en cours** dans l'arbre de travail (non commité, vu le 21 octobre) : chanfreins de 3 à 6 mm (`BEVEL_SPEC`) sur les plinthes, les chambranles, la porte de service et le cadre des luminaires.
7. **Ligne de marée partout à la même hauteur.** [Observé] La même ligne ondulée, environ 15 cm au-dessus de la plinthe, apparaît sur toutes les faces de mur visibles :
   - `vis_0_Qualite_dos.png` (x 380–1366, y ≈ 555–575) ;
   - `vis_0_Qualite_vue.png` (mur gauche, y ≈ 540–560) ;
   - `v416_kit_angle.png` et `v416_kit_poteau_droit.png` (y ≈ 315–330).

   Elle se lit comme un motif, pas comme une humidité locale.
   - [Source] `TideLine` est porté par le matériau, à une hauteur monde (BRLevels.cpp, l. 63).
   - [Hypothèse] Le matériau du kit applique la même règle à tous les murs.
8. **Jonction plafond-mur sans cornière de rive.** [Observé] `v416_kit_plafond_mur.png` : les dalles touchent directement le papier peint.
9. **Taches.**
   - [Observé] La tache d'eau en anneaux autour d'un plafonnier est crédible (`vis_0_Qualite_plafond.png`, x 470–920, y 400–640).
   - [Observé] Les taches de moquette sont plausibles sur `vue` et `dos` (premiers plans).
   - [Non concluant] Leur répétition à grande échelle : une seule graine et trois vues.
10. **Moquette.**
    - [Observé] Matte. Une fine hachure diagonale apparaît à −60° (`vis_0_Qualite_sol.png`, zoom x 500–900, y 250–500).
    - [Hypothèse] Moiré des rangs de touffetage, à juger en mouvement.
11. **Porte de service** (`final416/chapitre/v413_chapitre_porte.png`).
    - [Observé] Battant (x 510–850, y 30–610), cadre et embrasure (x 420–950) partagent un même aspect taché gris-vert : trois matières attendues, une seule lue. La béquille est une forme simple (x 548–598, y 328–390).
    - [Observé] Des filets bleu froid courent le long du jeu du battant (x 505–520, y 30–130 ; x 845–860, y 30–330).
    - [Hypothèse] Lumière voulue du sas ou fuite de lumière. Le filet `SM_L0_ServiceDoor_DraftGlow` du kit est sous le battant (build_level0_kit.py), pas sur les côtés. À juger sur `v417_l0_porte_fermee`.
12. **Smiler.**
    - [Observé] Près d'un mur éclairé (`vis_0_Qualite_mur_smiler.png`, x ≈ 490–860, y ≈ 130–710), son corps est une forme noire plate à contour dur, comme une découpe.
    - [Observé] Dans le noir (`vis_0_Qualite_coupure.png`, `_lampe.png`), yeux et dents se lisent très bien : à garder.
13. **Épaisseur des murs.** [Observé] Les murs de 60 cm se lisent dans les baies (`v416_kit_jambage.png`) comme des murs porteurs plutôt que des cloisons de bureau. C'est une dimension de jeu : on la garde, et cela n'appelle aucune action.

**Ce que la référence fait mieux**

- `ref_01.jpg`, `ref_03.jpg` : **hiérarchie des valeurs**.
  - [Mesuré] ref_03 : mur gauche 0,056, mur du fond 0,079, moquette 0,126, plafond 0,136.
  - [Mesuré] ref_01 : mur du fond 0,024, moquette 0,21, plafond 0,115.
  - Les murs y sont plus sombres que le sol, et environ 10 % de l'image est sous 0,01.
- Peu de sources visibles par image, décroissance forte, ombres de contact sous les personnages.
- Une variation locale forte : mur arraché sur ref_03 (x 0–440, y 480–950).
- Un accent de couleur : panneau de sortie rouge au-dessus d'une baie noire sur ref_01 (x 1670–1760, y 330–720).
- Papier peint peu contrasté, avec des taches de ton à grande échelle (ref_03, mur gauche).
- Un décroché de plafond qui crée une bande d'ombre (ref_01, haut du mur du fond).

**À garder chez nous**

- Le papier peint à chevrons du modèle fourni et ses lés de 53 cm.
- La moquette bouclée et ses taches à l'échelle du monde.
- Les dalles fissurées de 61 cm et leurs taches d'eau en décalques.
- Les néons blanc chaud (3700 K, balance à 3900 K).
- Les zones sans lumière de type `x10`.
- Les baies sans battant.
- Le Smiler lumineux dans le noir.
- La brume légère.

### 1.2 Salle de fosses (Pitfalls)

**Écarts observés**

- [Observé] `vis_0_Qualite_fosses.png` : une quinzaine de trous carrés identiques en treillis diagonal régulier, tous avec le même bord. L'ensemble se lit comme un **damier** (toute l'image).
- [Observé] Fosse la plus proche (x 430–935, y 310–545) :
  - la moquette s'arrête sur une arête nette, sans épaisseur visible ;
  - aucune tranche de dalle ne se distingue du béton du puits ;
  - pas de bord abîmé.
- [Observé] La paroi en béton taché descend sur environ 1 m puis passe au noir. Cette **obscurité progressive** est bonne : fond (x 560–800, y 420–520) à p50 = 0,003, 84 % sous 0,01.
- [Observé] Les fosses lointaines (y 140–200) gardent une bande claire, leur paroi du fond, qui donne la profondeur.
- [Observé] `final416/full/L00_fosses_lampe.png` (regard vers le bas, lampe) : bord tranchant, sans épaisseur.
- [Source] Le kit modélise 2 cm de moquette et 20 cm de dalle (« tranche visible dans les fosses », build_level0_kit.py : `SLAB`, `CARPET`, `SM_L0_Pit_HoleTile`).
- [Non concluant] À cette distance et dans cette lumière, l'image ne permet pas de dire si la tranche est rendue et seulement indiscernable.
- [Non concluant] Le plafond au-dessus des fosses n'est pas dans le champ (caméra baissée) : ni ses luminaires ni leurs reflets ne peuvent être jugés.

**Référence** : aucune des 16 images Steam ne montre de fosses. La direction vient du prompt (« puits réels ; hauteur, bord et chemin praticable lisibles ; obscurité progressive ; aucun damier plat »).

**À garder** :
- la disposition et les dimensions : trous de 2 m, 14 m de profondeur ;
- les collisions et les passages ;
- les néons aux croisements, une cellule sur deux ;
- le béton qui s'assombrit avec la profondeur ;
- la brume locale.

### 1.3 Poolrooms (Niveau 37)

**Écarts observés**

1. **Carrelage.**
   - [Observé] Échelle constante sur les murs, les piliers, les trottoirs et les intrados d'arches. Sur `vis_37_Qualite_x10.png` (arche, x 300–640, y 40–520), les joints suivent la voûte sans étirement. C'est bon.
   - [Source] Carreaux de 25 cm (`T_PoolTile37` : 4 × 4 carreaux pour 100 cm), texture de 512 px (2 mm par pixel), sans variation par carreau.
2. **Caustiques détachées de l'eau et de la lumière.** [Observé] Des filaments blancs nets apparaissent :
   - sur le **dessus sec des trottoirs** : `x10`, x ≈ 760–1150, y ≈ 440–640 ; `vue`, banc gauche, x 0–430, y 550–640 ;
   - sur **tout le plafond en plâtre**, à densité uniforme : `vue`, x 0–700, y 40–220.

   Ils se lisent comme des veines de marbre, pas comme de la lumière. Les murs hauts n'en ont pas (`x10`, x 0–330, y 40–400).
   - [Source] `BRCaustics` multiplie l'albédo (BRMaterialBuilder.cpp, l. 424).
   - [Source] Sur les faces horizontales, l'atténuation vaut 1 à toute hauteur (`fade = lerp(..., 1.0, Wz)`, l. 838).
   - [Source] Le plafond reçoit 0,35 partout (BRLevels.cpp, l. 676).

   Ces trois points expliquent les filaments sur les dessus secs et au plafond.
3. **Image laiteuse, sans noir ni hautes lumières.** [Mesuré]
   - `vue` : p1 = 0,024, p5 = 0,063, p99 = 0,625, 0,0 % sous 0,01. ref_05 : p1 = 0,004, p99 = 0,91.
   - `dos` : plafond (x 300–1000, y 40–230) à 0,28, plus clair que les murs (x 0–210, y 290–470) à 0,11. ref_05 : plafond 0,156, murs 0,150.
   - [Observé] La salle de droite de `vue` (x 1000–1310, y 190–330) est noyée de halo.
4. **Eau.**
   - [Observé] Claire. Les joints du sol se voient sous 60 cm d'eau avec réfraction (bas de `vue` et de `dos`). Reflets rasants des piliers et des baies (`vue` x 990–1366, y 380–640 ; `dos`). C'est bon.
   - [Observé] Le **bassin profond** (260 cm) ne se lit que comme une zone plus sombre et miroitante (`dos`, x 300–700, y 430–510). Ni bord, ni rupture de pente, ni paroi immergée ne sont lisibles : la profondeur ne se perçoit pas.
   - [Non concluant] Ligne humide au-dessus de l'eau sur piliers et murs : invisible à cette résolution (`dos`, zoom x 200–480, y 380–560). [Source] `WaterLine` existe pour le sol, les murs et les piliers (BRLevels.cpp, l. 678).
5. **Trottoirs.** [Observé] Ils dépassent l'eau de 8 cm. Leur bord se lit surtout par la différence de couleur (`vue`, x 500–640, y 475–600). Il n'y a ni carreau de nez ni margelle, et les caustiques des dessus brouillent la différence entre sec et mouillé.
6. **Sources de lumière.**
   - [Observé] Plafonniers ovales bien identifiables.
   - [Non concluant] Aucune verrière (`SM_PoolSkylight`) dans les trois poses : la lumière du jour n'est pas jugeable.

**Ce que la référence fait mieux** (`ref_05.jpg`) :
- grandes taches de lumière douces sur le fond du bassin, liées à une ouverture (x 0–560, y 650–760 ; x 1200–1900, y 680–830) ;
- murs secs propres ;
- fente sombre sous la banquette, qui donne la profondeur (x 700–1360, y 520–560) ;
- halo près de l'ouverture (x 0–540, y 330–560) ;
- joints du fond réfractés ;
- reflets des piliers en vue rasante.

Les petites mosaïques de ref_05 ne sont **pas** à copier : notre carrelage vient de la scène fournie.

**À garder** :
- arches, corniches à 45°, canaux de 60 cm, trottoirs de 68 cm et bassins de 260 cm ;
- plafonniers ovales ;
- carrelage vert d'eau de 25 cm à joints gris (scène fournie) ;
- eau claire, reflets et réfraction ;
- inscriptions à la craie ;
- calme et absence d'entités.

### 1.4 Niveaux techniques 1 et 2

**Niveau 1** (`vis_1_Qualite_vue`, `dos`, `x10`)

- [Observé] Un même béton gris tacheté, très contrasté, sur les murs, le plafond et les piliers (`vue`, toute l'image). Pas de joints de coffrage, de trous de banches ni de chanfreins d'angle.
- [Observé] Les flaques couvrent une grande partie du sol en formes dures, comme un camouflage (`vue`, x 150–1220, y 470–768). Elles brillent fort sur `x10` (x 600–850, y 600–720).
- [Observé] Voile de brume sur `vue` (x 480–860, y 200–460). En revanche, `x10` a un bon contraste : 73,7 % sous 0,01, salle éclairée devant.
- [Référence] `ref_11.jpg` :
  - piliers en béton banché avec coulures verticales (x 1180–1420, y 60–580) ;
  - **une** flaque localisée avec une cause, des reflets nets (x 830–1300, y 620–900) ;
  - conduites et suspentes au plafond ;
  - périphérie noire (46 % sous 0,01).
- **À garder** :
  - la bande bleue peinte à environ 1 m ;
  - les rayures jaune et noir des piliers ;
  - les tubes suspendus ;
  - le tableau d'interrupteurs (`x10`, x 910–1045, y 395–460).

**Niveau 2** (`vis_2_Qualite_vue`, `dos`, `mur_x`, `mur_y`, `sol`, `plafond`)

- [Observé] Murs et plafond portent la même texture brune tachetée, sans structure à 80 cm (`mur_x`, `mur_y`). Les arêtes des blocs sont vives.
- [Observé] Les tuyaux de cuivre s'arrêtent net au bout des blocs, sur un manchon, sans tourner ni entrer dans le mur (`dos`, x 300–345, y 205–240 ; `vue`, x 520–620, y 215–345).
- [Observé] Les flaques ont le même masque dur qu'au Niveau 1 (`sol`).
- [Observé] La tache de plafond autour de l'ampoule se lit comme un disque orange saturé à anneaux, une lueur plutôt qu'une tache (`plafond`, x ≈ 530–940, y ≈ 170–510).
- [Observé] Reflets d'objectif en cercles (`vue`, x 650–720, y 300–480) ; le VHS est actif sur ce banc.
- [Non concluant] `x10` est presque noir (99,4 % sous 0,01).
- [Référence] `ref_16.jpg` : tuyaux continus sur supports, avec deux matières distinctes (calorifuge peint, acier rouillé), plafond en béton à panneaux de coffrage, une seule applique, noir profond (67 % sous 0,01). `ref_15.jpg` : carrelage brillant, colonnes et gaines, sources ponctuelles.
- **À garder** : les ampoules grillagées, les tuyaux de cuivre et la palette brune chaude.

### 1.5 Joueur (combinaison) et entités

- **Combinaison.** [Observé] `v416_entite_Partygoers_3p_face.png` (x 430–690, y 355–768) :
  - jaune lisse et mat, peu de plis fins, aucune brillance qui suit les plis ;
  - texture d'étoffe invisible ;
  - bras écartés du corps au repos ;
  - un peu de saleté sur les jambes.
  - [Hypothèse de cause, cohérente avec le prompt § 4.1] Pas de normal map dédiée dans le parent `Mesh`.
  - [Non concluant] La visière : les vues sont de dos.
  - [Référence] `ref_01.jpg` (x 340–660, y 260–1040) et `ref_03.jpg` : plis serrés qui accrochent la lumière, trame enduite visible, saleté aux genoux et en bas, gants et bottes avec leur propre brillance, ombres de contact nettes.
  - **À garder** : le modèle et son dessin (bande noire de capuche, deux pastilles, tuyau) et le jaune.
- **Entités.**
  - [Observé] Toutes les captures `v416_entite_*` sont au Niveau 4 (bureaux blancs, moquette bleue). Leur intégration au Level 0 ou aux Poolrooms n'est pas jugeable, sauf pour la Bacteria (`final416/full/L00_E_Bacteria.png`, lumière rouge : silhouette de filaments, bien) et le Smiler (§ 1.1).
  - [Observé] Partygoer (`v416_entite_Partygoers_1p_menace.png`, pieds x 600–715, y 585–612) : aucune ombre de contact visible sur la moquette.
  - [Observé] Bacteria sous néons blancs (`v416_entite_Bacteria_1p_menace.png`, x 660–830, y 40–700) : laque noire brillante, aspect plastique.
  - [Non concluant] Appuis et glissements : images fixes.
  - **À garder** : tous les modèles, la lueur du Smiler, le ballon du Partygoer et les filaments de la Bacteria.

### 1.6 Interface

- [Observé] L'inventaire (`v416_fenetre_169_max_personnage.png`, `v416_hud_1920x1080_fr_personnage.png`) a déjà la structure de `ref_09.jpg` : trois colonnes, silhouette à emplacements, fond flou, conseils en pied. **À garder.**
- [Observé] **Accents** : « RéTABLIR LA LIAISON DE SERVICE » et « LIRE LE SCHéMA DE DISTRIBUTION » portent un é minuscule dans des chaînes en capitales (`v416_hud_1920x1080_fr_personnage.png`, panneau OBJECTIFS, x ≈ 100–480, y ≈ 220–265), alors que « FILMER UNE ENTITÉ » est correct.
  - [Source] Le texte d'origine est « R\u00e9tablir… » (BRMissionText.cpp, l. 507).
  - [Hypothèse] Il est mis en capitales par `FString::ToUpper`, qui ne traite que l'ASCII dans UE5. `FText::ToUpper`, déjà employé ailleurs dans BRHUD.cpp, gérerait les accents. Pour l'agent UI.
- [Observé, estimation] Les objectifs ont environ 9 px de hauteur de capitale en 1920 × 1080, contre environ 20 px dans ref_09.
- [Observé] « POCHES » et « STOCKAGE » sont écrits en lettres empilées, une par ligne (`v416_fenetre_169_max_personnage.png`, x 485–495, y 180–385). ref_09 fait pivoter le mot.
- [Observé] Les lignes facultatives sont gris sur olive, peu contrastées.
- [Observé] Au Level 0, pendant une coupure, deux notifications « TÂCHE ACCOMPLIE » s'affichent en haut au centre (`vis_0_Qualite_coupure.png`, x 505–860, y 78–138).
  - [Source] Ces tâches ne sont pas obligatoires au Level 0 (`bRequired = D.bRequireObjectives`, BRWorld.cpp, l. 3287) : le contrat de jeu est respecté.
  - L'allure reste celle d'une mission. La discrétion du HUD est à juger par l'agent UI.
- [Non concluant] HUD en partie : `v416_hud_1920x1080_fr_jeu.png` montre le carton de titre du niveau, pas le HUD de jeu.

## 2. Passe Level 0 et Poolrooms : changements priorisés (12)

**Échelle de coût** :
- **faible** : au plus une demi-journée (paramètres, texture régénérée) ;
- **moyen** : 1 à 2 jours (matériau ou shader, cook, banc) ;
- **élevé** : 3 jours ou plus (modélisation Blender, import, contrôles de collision, de réseau et de sauvegarde).

**Règles communes** :
- aucune source ajoutée, déplacée ni supprimée ;
- intensités des plafonniers, seuils (0,12, 0,50, 0,55, 0,15) et grille de lumière de jeu inchangés ;
- tout critère se vérifie sur l'avant (code v4.16 avec le banc v4.17) et sur l'après, mêmes poses, profil Qualité puis Performance.

**Fichiers partagés** :
- `BRMaterialBuilder.cpp`, `BRAssets.cpp`, `BRChunk.cpp` et les scripts d'import : sous un responsable unique ;
- `build_level0_kit.py` : déjà modifié par un autre agent (chanfreins) ;
- `BRWorld.cpp` (position des luminaires) : **ne pas toucher**.

### 1. Level 0 : baisser la valeur du papier peint (murs moins clairs que la moquette)

- **Quoi** : baisser d'environ 20 % la luminance claire du papier peint et d'environ 30 % l'écart entre bande et liseré. Garder le motif et la teinte, et tirer le vert vers un ocre-olive dans le papier lui-même, sans filtre global.
  - [Source] Les murs du kit gardent leurs propres matériaux (`bMeshMaterials`, BRChunk.cpp, l. 149) : la teinte `D.Wall` ne suffit pas. Il faut passer par `T_L0_Wallpaper` (generate_textures.py), puis `T_L0K_Wallpaper` (level0_kit_textures.py, `project_material`), puis réimporter.
  - Le banc des couleurs (Tools/Light/test_color.cpp, « Papier peint du Level 0 ») sera à recalibrer.
- **Impact** : fort. Profondeur et hiérarchie des surfaces ; moins de rebond, donc des coins plus sombres. L'image se rapproche de la règle de jeu, qui ne compte pas le rebond (GAMEPLAY_v4.16, § 1).
- **Coût** : faible à moyen.
- **Risque** :
  - lumière de jeu inchangée ;
  - les zones que la règle compte « éclairé » doivent rester lues comme éclairées ;
  - la silhouette noire du Smiler se détache moins d'un mur plus sombre ;
  - Level 6 : aucun (pas de papier peint).
- **Acceptation** :
  - sur `v417_l0_salle` et `v417_l0_dos` : médiane Y des faces de mur éclairées ≤ médiane Y de la moquette au premier plan. Aujourd'hui, ce rapport va de 1,1 à 2,5 sur `vis_0_Qualite_vue` ;
  - chevrons toujours identifiables à 3 m ;
  - `vis_0_<profil>_mur_smiler` (banc v4.16 relancé) : corps du Smiler au moins trois fois plus sombre que le mur derrière lui.

### 2. Poolrooms : caustiques attachées à l'eau

- **Quoi** :
  - (a) aucune caustique sur les faces horizontales au-dessus de l'eau (dessus des trottoirs, sols secs) ;
  - (b) au plafond, seulement au-dessus d'une surface d'eau, en décroissant avec la hauteur et la distance au bord ;
  - (c) filaments plus larges et plus doux (contraste divisé par deux environ) ;
  - (d) au fond des canaux, garder des taches plus larges, l'esprit de ref_05.

  Zones de code : BRMaterialBuilder.cpp, l. 819–841 (`CausticsHLSL`), et BRLevels.cpp, l. 676.
- **Impact** : fort. Plus de marbre au plafond, et la différence entre sec et mouillé devient lisible.
- **Coût** : faible à moyen (shader, paramètres, cook).
- **Risque** :
  - améliore la lisibilité des passages secs et des vannes ;
  - coût GPU négligeable, mais à mesurer : c'est la scène la plus lourde ;
  - Level 6 : aucun.
- **Acceptation** :
  - sur `v417_p_trottoir`, `v417_p_vue` et `v417_p_dos` : aucun filament sur le dessus des trottoirs secs ni au plafond au-dessus d'un trottoir ;
  - au-dessus des canaux, filaments plus doux qu'avant ;
  - vidéo `p_3p_eau` : les caustiques du fond bougent sans scintiller.

### 3. Level 0 : recoins sombres et cohérence entre l'image et la règle

- **Quoi** :
  - occlusion de contact aux pieds des murs, dans les angles rentrants, sous les linteaux et dans les embrasures (AO de matériau ou décalques bornés, ou réglage Lumen par l'agent éclairage) ;
  - exposition de la salle de départ à arbitrer avec l'agent éclairage : médiane de `l0_salle` vers 0,13–0,15 au lieu de 0,19 ;
  - conserver les températures et les balances ;
  - aucune source ne bouge.
- **Impact** : fort. Des « recoins sombres » sans toucher aux plafonniers.
- **Coût** : moyen.
- **Risque** :
  - un écart entre l'image et la règle dans l'autre sens. Aujourd'hui l'image montre moins de noir que la règle n'en compte : 31,3 % des positions « dans le noir » au Level 0 selon GAMEPLAY_v4.16 § 2, contre 1,9 % de pixels très sombres sur `vue` ;
  - pompage d'exposition à revoir en mouvement ;
  - Level 6 : aucun.
- **Acceptation** :
  - sur `v417_l0_salle` et `v417_l0_dos` : angles rentrants et dessous de linteau au moins deux fois plus sombres que le centre du mur voisin (zones fixées sur l'avant) ;
  - 4 à 10 % de pixels sous 0,01 hors plafonniers, sans mur éclairé bouché ;
  - « carte lumière » (nouvelle capture, § 4) : au moins 90 % des points « dans le noir » (< 0,12) sous Y 0,03 à l'image, et au moins 90 % des points « éclairé » (> 0,50) au-dessus de Y 0,10. Seuils à confirmer sur l'avant.

### 4. Pitfalls : bord lisible et damier rompu, sans toucher la salle

- **Quoi** :
  - moquette coupée qui déborde de 1 à 2 cm, avec un effilochage irrégulier sans collision ;
  - tranche de dalle de 20 cm plus claire et texturée (béton coulé, armature visible sur quelques fosses) ;
  - assombrissement de contact sur la moquette autour du trou (décalque borné) ;
  - au moins trois traitements de bord (net, effiloché, cassé), tirés par la graine et la cellule.

  Disposition, trous, profondeur, collisions et néons inchangés.
- **Impact** : fort sur la salle la plus marquante du Level 0.
- **Coût** : moyen à élevé (kit, import, décalques).
- **Risque** :
  - le bord de collision et la possibilité de contourner ne changent pas ;
  - un débord ne doit pas faire croire à un appui au-delà du trou ;
  - Level 6 : aucun.
- **Acceptation** :
  - sur `v417_l0_fosses` et la nouvelle vue du bord (§ 4) : la bande de moquette et la tranche de dalle se distinguent du puits sur la fosse la plus proche ;
  - au moins trois bords différents parmi les fosses visibles ;
  - les fosses lointaines gardent la bande claire de leur paroi du fond ;
  - les tests de fosses et de chute restent inchangés.

### 5. Combinaison et entité de contrôle

- **Quoi** :
  - combinaison : parent ou variante « Suit » avec normal map dédiée (plis, coutures, fermeture, renforts) et carte de rugosité (zones mates et zones enduites brillantes) ;
  - saleté placée là où elle a une cause (genoux, bas des jambes, gants) ;
  - gants et bottes avec leurs propres matières ;
  - valeurs neutres si une carte manque ; cook synchronisé ;
  - Bacteria de contrôle : ombre de contact et matière moins « laque » sous lumière blanche.
- **Impact** : fort. Le joueur et ses coéquipiers sont à l'écran en coopération.
- **Coût** : moyen à élevé.
- **Risque** :
  - aucun sur la lumière de jeu ;
  - lisibilité d'un coéquipier dans le noir, à la lampe, à vérifier ;
  - aucun émissif ajouté (Level 6).
- **Acceptation** :
  - sur `v417_l0_combinaison_neon_{dos,face,cote}` et `_neutre_{dos,face}` : les hautes lumières suivent les plis principaux, sans bruit fin régulier ;
  - trois matières lisibles à 2 m : étoffe, gants et bottes, visière ;
  - à 8–10 m (nouvelle capture), silhouette et bande de capuche lisibles ;
  - `v417_l0_entite_bacteria` : appuis au sol ombrés.

### 6. Poolrooms : eau lisible en profondeur, bords et ligne humide

- **Quoi** :
  - une margelle ou une bande de carreaux contrastés au bord des bassins profonds et des trottoirs, lisible à 10 m ;
  - une paroi de bassin visible sous l'eau, qui s'assombrit avec la profondeur (absorption) ;
  - une ligne humide de 3 à 5 cm au-dessus de l'eau sur les piliers et les murs (`WaterLine` existe : trouver pourquoi elle ne se voit pas) ;
  - un clapot intérieur conservé.

  Attention : les vannes changent le niveau d'eau. La ligne humide doit suivre la hauteur courante ou marquer l'ancien niveau, mais jamais contredire l'eau visible.
- **Impact** : moyen à fort. On lit le danger (« N'y plonge pas ») et l'état des vannes.
- **Coût** : moyen.
- **Risque** :
  - performance de l'eau : mesurer séparément l'eau, la première couche translucide, les reflets et le brouillard ;
  - lisibilité des mécanismes améliorée ;
  - Level 6 : aucun.
- **Acceptation** :
  - `v417_p_bassin` : bord et paroi immergée lisibles, fond qui s'assombrit progressivement sur au moins 1 m ;
  - `v417_p_rasant` : joints visibles sous 60 cm près de la caméra, reflets à l'horizon (acquis à garder) ;
  - `v417_p_trottoir` : ligne humide visible sur le pilier ou le mur le plus proche ;
  - `v417_p_sous_eau` : aucun plan qui coupe l'image au passage de la surface.

### 7. Poolrooms : hiérarchie lumineuse et volumes

- **Quoi** :
  - plafond en plâtre moins clair (teinte 1,08 → environ 0,9, BRLevels.cpp, l. 643) ;
  - brume moins laiteuse dans les grandes salles (`FogColor`, `VolumetricScatter`) ;
  - fonds d'arches et de couloirs plus sombres par l'albédo et l'AO ;
  - reflets des plafonniers ovales plus francs sur l'eau et le carrelage mouillé.

  Nombre et place des plafonniers inchangés ; la sérénité reste.
- **Impact** : moyen.
- **Coût** : faible.
- **Risque** :
  - pas d'entité, et la santé mentale remonte au Niveau 37 ;
  - vannes et passages secs doivent rester lisibles ;
  - Level 6 : aucun.
- **Acceptation** :
  - sur `v417_p_vue` et `v417_p_dos` : médiane du plafond ≤ médiane des murs (aujourd'hui 0,28 contre 0,11 sur `dos`) ;
  - p1 ≤ 0,01 et p99 ≥ 0,75, avec des hautes lumières qui ont une source identifiable, sans grande zone écrêtée ;
  - `v417_p_contrejour` : contours des arches et des piliers nets devant la source.

### 8. Level 0 : plafonniers crédibles (diffuseur, cadre, ossature)

- **Quoi** :
  - diffuseur à prismes avec deux ou trois bandes de tubes à peine marquées (reprendre `T_L0K_Diffuser`), à **luminance moyenne égale** ;
  - cadre en tôle peinte blanc cassé jaunie au lieu de `T_Grime` (emplacement `Frame`), avec les chanfreins déjà en cours dans le kit ;
  - ossature, sans déplacer les luminaires : des traverses locales autour de chaque luminaire (dalles découpées, comme le ferait un poseur), pour que l'ossature du monde s'arrête sur une traverse et non au milieu d'un côté.

  Vérifier d'abord quel luminaire est rendu en jeu : `SM_LightPanel` ou le module du kit.
- **Impact** : moyen. Les luminaires sont dans toutes les vues.
- **Coût** : faible pour le diffuseur et le cadre ; moyen pour l'ossature.
- **Risque** :
  - émissif et éclairage global : garder la luminance moyenne et l'intensité de la RectLight ;
  - les diffuseurs restent lisibles ;
  - Level 6 : aucun (pas de plafonniers).
- **Acceptation** :
  - sur `vis_0_<profil>_plafond` (banc v4.16) et `v417_l0_raccord_plafond` : dans le diffuseur, rapport p95/p5 ≥ 1,15 (structure visible) ;
  - cadre sans taches de `T_Grime` ;
  - sur `v417_l0_salle` : moins de 2 % de pixels du diffuseur central ≥ 250 (aujourd'hui 11,6 %) ;
  - sur `v417_l0_dos` : aucune ligne d'ossature terminée au milieu d'un côté de luminaire.

### 9. Level 0 : plinthes, chambranles et porte de service (chanfreins en cours)

- **Quoi** :
  - valider à l'image les chanfreins en cours (`BEVEL_SPEC`) ;
  - des matières distinctes :
    - plinthe et chambranle en peinture satinée avec arête claire ;
    - battant en tôle peinte avec usure aux arêtes et autour de la poignée ;
    - cadre métallique plus sombre ;
    - embrasure en enduit ou en béton ;
    - poignée en métal patiné ;
  - collisions séparées, dimensions et pivots inchangés.
- **Impact** : moyen. La porte est le bout du chemin critique.
- **Coût** : moyen.
- **Risque** :
  - les joints restent fermés ;
  - ouverture de la porte et LiDAR (porte de service ouverte) à revérifier ;
  - la porte doit rester contrastée par rapport au mur ;
  - Level 6 : aucun.
- **Acceptation** :
  - `v417_l0_plinthe` et `v417_l0_rasante` : un liseré clair continu sur l'arête haute de la plinthe ;
  - `v417_l0_raccord_mur` : aucune fente lumineuse dans le noir ;
  - `v417_l0_porte_fermee` : battant, cadre et mur distincts en valeur ou en rugosité, poignée lisible à 2 m, filets bleus expliqués (voulus ou corrigés) ;
  - `v417_l0_porte_ouverte` : battant en place, pas de trou de lumière.

### 10. Level 0 : usure localisée et variantes déterministes

- **Quoi** :
  - ligne de marée sur 15 à 25 % des murs seulement, près d'une cause (moquette humide, salle de fosses, air froid du sas), à une hauteur variable de 6 à 20 cm ;
  - quelques variantes rares :
    - dalle de plafond remplacée (ton différent) ou manquante (vide noir) ;
    - lé décollé ;
    - moquette usée sur le chemin direct entre deux baies ;
  - des salles presque vides préservées ;
  - tirage par graine et par cellule, continu d'un chunk à l'autre, bornes de décalques de la v4.16 respectées.
- **Impact** : moyen à fort. Casse la répétition.
- **Coût** : moyen.
- **Risque** :
  - déterminisme lors de la mort, de la reprise, de la reconnexion et du streaming ;
  - aucun déplacement de mur, porte, fosse ou mécanisme ;
  - Level 6 : aucun.
- **Acceptation** :
  - sur `v417_l0_salle` et `v417_l0_dos`, graine 9 et une deuxième graine : ligne de marée sur une minorité de murs, chaque fois près d'une cause visible ;
  - `v417_l0_raccord_mur` : la variante continue d'un chunk à l'autre ;
  - même capture avant et après rechargement de la partie : écart faible, hors bruit.

### 11. Level 0 : lés de 53 cm lisibles et relief du papier adouci

- **Quoi** :
  - joint de 1,5 à 2 mm légèrement encrassé ;
  - ton par lé de ±3 %, tiré par un hachage de l'index du lé dans l'espace monde et non dans la texture (sinon on obtient l'alternance A/B de la répétition de 106 cm) ;
  - décollements rares visibles en lumière rasante ;
  - embossage des chevrons et trame verticale environ deux fois plus discrets.
- **Impact** : moyen. Une échelle crédible, demandée explicitement.
- **Coût** : faible (texture et matériau du kit).
- **Risque** : répétition visible si le ton reste dans la texture ; Level 6 : aucun.
- **Acceptation** :
  - `v417_l0_rasante` : au moins trois raccords successifs visibles, espacés de 53 cm. Contrôle : sur `mur_x` (80 cm, champ de 88°), le profil des colonnes montre un pic au pas de 469 px ;
  - `vis_0_<profil>_mur_x` : chevrons sans biseau clair et sombre marqué ;
  - `v417_l0_salle` à 3–5 m : motif calme, sans moiré.

### 12. Poolrooms : carrelage (résolution, joints, nez)

- **Quoi** :
  - un **dérivé** de `T_PoolTile37` en 2048 px pour 1 m, l'original étant une ressource fournie (déclaration dans `protected_assets.json`) ;
  - un joint simple de 4 à 5 mm ;
  - ±2 % de ton par carreau et une légère inclinaison (comme `T_PoolTile`) ;
  - des carreaux de nez arrondis aux bords des trottoirs, des marches immergées et des bassins ;
  - trois réponses distinctes : sec (rugosité environ 0,25), mouillé (environ 0,08, plus sombre), immergé ;
  - 25 cm et vert d'eau conservés.
- **Impact** : moyen.
- **Coût** : moyen.
- **Risque** :
  - ressource protégée, donc dérivé séparé ;
  - hauteurs et collisions des trottoirs, échelles du canal inchangées ;
  - Level 6 : aucun.
- **Acceptation** :
  - `v417_p_trottoir` et `v417_p_rasant` : à 1 m, joints nets de 3 à 5 px, carreaux non identiques ;
  - nez de trottoir lisible à 10 m sur `v417_p_vue` ;
  - sec et mouillé distincts sur un même plan.

**Hors liste, pour l'agent UI** :
- accents en capitales (§ 1.6) ;
- taille des textes d'objectifs ;
- « POCHES » et « STOCKAGE » en lettres empilées ;
- contraste des lignes facultatives.

**Hors passe, idée à décider plus tard** : des retombées de plafond au Level 0 (ref_01). Elles toucheraient l'occultation de la lumière de jeu, où le plafond compte : à ne faire qu'avec la logique de lumière.

## 3. Règles de réutilisation pour les niveaux suivants

1. **Hiérarchie des valeurs par niveau.** Fixer quelle surface est la plus claire (sol, mur, plafond) et la mesurer sur la pose « salle ». Une surface verticale ne doit pas dépasser le sol sous la même lampe sans raison.
2. **Échelles réelles documentées et vérifiées à 80 cm.** Noter les centimètres par répétition et mesurer le pas du motif sur la capture `mur`. Un détail d'échelle (lé, joint, dalle) doit se lire entre 1 et 3 m.
3. **Pas de lumière peinte dans l'albédo sans masque physique.** Caustiques, halos ou taches lumineuses n'apparaissent que là où leur cause existe (eau, source) ; jamais sur une face sèche ou tournée à l'opposé.
4. **L'usure a une cause et reste rare.** Fuite, passage, eau : jamais sur toutes les surfaces à la même hauteur. Tirage par graine et cellule, continu entre chunks, sous les bornes de décalques.
5. **`T_Grime` est une couche, pas une matière.** Chaque pièce lisse ou peinte (cadre, porte, plastique) a sa matière propre, avec sa valeur et sa rugosité.
6. **La lumière de jeu ne bouge pas.** On travaille l'albédo, la forme des sources, l'occlusion et l'exposition, puis on contrôle l'image et la règle ensemble (« carte lumière »).
7. **Chanfreins de 3 à 6 mm** seulement sur les arêtes vues à hauteur d'œil. Joints fermés, collisions simples séparées.
8. **Chaque niveau garde sa palette.** Le jaune vient du Level 0 et de la combinaison. Aucun filtre global.
9. **Le chemin et les mécanismes priment.** Bord praticable, sortie, vannes et inscriptions contrastés. Le Level 6 se recontrôle à chaque passe : aucun émissif ni décor lumineux nouveau.
10. **Validation** : deux graines, Qualité et Performance, une vidéo de déplacement, un avant et un après aux mêmes poses.

## 4. Captures qui manquent pour juger

**Déjà prévues par le banc v4.17** (`BRAutoTestV417Visual.cpp`) :
- Level 0 : `l0_salle`, `l0_salle_vhs`, `l0_dos`, `l0_rasante`, `l0_plinthe`, `l0_raccord_mur`, `l0_raccord_plafond`, `l0_porte_fermee`, `l0_porte_ouverte`, `l0_fosses` ;
- combinaison : `l0_combinaison_neon_*`, `l0_combinaison_neutre_*` ;
- coupure et retour : `l0_coupure`, `l0_coupure_lampe`, `l0_retour_15`, `l0_retour_40` ;
- entité, inventaire et HUD : `l0_entite_bacteria`, `l0_inventaire_*`, `l0_hud_jeu` ;
- Poolrooms : `p_vue`, `p_vue_vhs`, `p_dos`, `p_trottoir`, `p_bassin`, `p_rasant`, `p_sous_eau`, `p_contrejour`, `p_3p_eau` ;
- Niveaux 1 et 2, dont `l2_plafond` ;
- vidéos : `l0_marche`, `l0_3p`, `l1_marche`.

**À ajouter** :

1. **Carte lumière, Level 0** (`l0_salle`, `l0_dos`, une pose de type `x10`) : la valeur de la lumière de jeu à des points du sol, projetés sur l'image (CSV plus image annotée). C'est indispensable pour les items 1 et 3.
2. **Enfilade longue, Level 0** (au moins 20 m, regard horizontal), graine 9 et une deuxième graine. Elle permet de juger la répétition des luminaires, des taches et de la ligne de marée, le brouillard et un raccord de chunk lointain.
3. **Dessous d'un plafonnier, Level 0** (regard à +80°, 1 m à côté), en Qualité et en Performance. Elle montre le diffuseur, le cadre et l'ossature ; le v4.17 n'a que `raccord_plafond`.
4. **Recoin sans lumière, Level 0**, à hauteur d'œil, lampe éteinte puis allumée, avec la valeur de jeu dans la fiche.
5. **Fosses** :
   - (a) le bord à 1 m, regard à −45° ;
   - (b) une vue horizontale avec le plafond au-dessus des fosses ;
   - (c) une deuxième salle de fosses, sur une autre graine ;
   - (d) les mêmes poses en Performance.
6. **Gros plans de surfaces, Level 0** : relancer `mur_x`, `mur_y`, `sol` et `plafond` du banc v4.16, ou les reprendre dans le v4.17, à 80 cm. Ajouter un raccord de lé à 30 cm en lumière rasante.
7. **Moquette humide et passage usé, Level 0**, à 1,5 m et −45°, pour juger la cause de l'usure (item 10).
8. **Poolrooms** :
   - une verrière (`SM_PoolSkylight`) et sa tache au sol ;
   - une marche immergée de près ;
   - un pilier et la ligne d'eau à 1 m ;
   - le bassin profond vu depuis le trottoir à −50° ;
   - une vanne et le passage sec ;
   - le plafond au-dessus d'un trottoir et au-dessus d'un canal ;
   - après manœuvre d'une vanne : ligne humide et niveau.
9. **Poolrooms dans les autres profils** (Performance et Cinématique), aux poses `p_*` principales : eau et reflets diffèrent selon le profil.
10. **Combinaison** : lampe allumée, à 8–10 m, et un **coéquipier vu chez un client** (réseau). Ces deux dernières vues ne sont pas dans le banc.
11. **Entités dans nos lieux** : Bacteria au Level 0 sous néons blancs, pas seulement rouges, et appuis en mouvement (vidéo) ; Smiler près d'un mur éclairé (`mur_smiler` existe en v4.16, à reprendre).
12. **Niveaux 1 et 2** :
    - une flaque de près ;
    - un coin et un bout de tuyau au Niveau 2 ;
    - un tableau de mécanisme à 1,5 m ;
    - une pose `x10` du Niveau 2 qui ne soit pas noire, ou la même avec la lampe.
13. **Level 6** : `vis_6_*` et `v416_lidar_*` relancés après la passe. Contrôle par différence d'image : aucune lumière nouvelle.
14. **Interface** :
    - état sélectionné et équipé, focus à la manette ;
    - japonais ou chinois et arabe ;
    - ultralarge 2560 × 1080 et 1280 × 720 ;
    - HUD en partie (la capture « jeu » v4.16 montre le carton de titre).
15. **En mouvement** : regarder un néon puis un mur sombre (pompage), et une coupure puis le retour au même instant de phase. Vidéos courtes.

## Annexe : mesures de base (v4.16, Qualité) et références

| Image | Zone (x0–x1, y0–y1) | Y p50 | Autre |
|---|---|---|---|
| vis_0_Qualite_vue | image | 0,19 | p5 0,022 ; 1,9 % sous 0,01 |
| vis_0_Qualite_vue | pilier gauche (395–475, 215–595) | 0,41 / 0,57 / 0,47 | haut / milieu / bas |
| vis_0_Qualite_vue | mur du fond (620–730, 345–435) | 0,44 | |
| vis_0_Qualite_vue | moquette (400–1366, 620–768) | 0,23 | |
| vis_0_Qualite_vue | plafond entre plafonniers (700–790, 200–300) | 0,09 | |
| vis_0_Qualite_vue | diffuseur central (545–665, 95–165) | 0,94 | 11,6 % ≥ 250 |
| vis_0_Qualite_dos | image | 0,18 | 0,3 % sous 0,01 |
| vis_0_Qualite_x10 | image ; zone sombre (1000–1366, 250–600) | 0,024 ; 0,007 | 18,1 % sous 0,01 |
| vis_0_Qualite_fosses | moquette (0–400, 560–768) ; fond de fosse (560–800, 420–520) | 0,14 ; 0,003 | fond : 84 % sous 0,01 |
| ref_01 | image ; mur du fond (80–1040, 200–790) ; moquette (0–1920, 820–1080) | 0,11 ; 0,024 ; 0,21 | 10,6 % sous 0,01 |
| ref_03 | image ; mur gauche (0–690, 0–900) ; moquette (800–1920, 900–1080) ; plafond (700–1750, 0–360) | 0,12 ; 0,056 ; 0,126 ; 0,136 | 10,0 % sous 0,01 |
| vis_37_Qualite_vue | image | 0,19 | p1 0,024 ; p5 0,063 ; p99 0,625 ; 0,0 % sous 0,01 |
| vis_37_Qualite_dos | plafond (300–1000, 40–230) ; mur gauche (0–210, 290–470) | 0,28 ; 0,11 | |
| ref_05 | image ; plafond gauche (0–520, 0–250) ; mur du fond (700–1360, 120–400) | 0,134 ; 0,156 ; 0,150 | p1 0,004 ; p99 0,91 ; 2,0 % sous 0,01 |
| vis_1_Qualite_vue ; x10 | image | 0,139 ; 0,002 | x10 : 73,7 % sous 0,01 |
| vis_2_Qualite_vue ; x10 | image | 0,106 ; 0,000 | x10 : 99,4 % sous 0,01 |
| ref_11 ; ref_15 ; ref_16 | image | 0,011 ; 0,012 ; 0,004 | 46,0 / 45,6 / 67,3 % sous 0,01 |
