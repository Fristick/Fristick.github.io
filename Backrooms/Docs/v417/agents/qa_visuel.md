# v4.17 — QA visuelle des changements (agent « Evidence Collector »)

| | |
|---|---|
| **Rôle** | Agent installé « Evidence Collector » (`C:\Users\Fristick\.claude\agents\testing-evidence-collector.md`, lu en entier), lancé comme sous-agent générique et appliqué à la QA visuelle d'un jeu Unreal. Les commandes Playwright de ce fichier ne s'appliquent pas ici et n'ont pas été exécutées. |
| **Mission** | Demande du propriétaire, § 3, 4 et 7 : vérifier à l'image, profil par profil, chaque changement annoncé entre `711b02d` et `eb3cc8f`. Pour chacun, dire s'il est amélioré, inchangé ou en régression, et chercher ce qui invalide une conclusion trop optimiste. |
| **Base** | Captures « avant » : code v4.16 avec le banc v4.17 (`711b02d`, `modifie=0`). Captures « après » : `eb3cc8f`, ressources réimportées (`modifie=1`, voir § 1.7). Branche `backrooms-v4.17`. |
| **Écriture** | Ce fichier seulement. Unreal et Blender n'ont pas été lancés. Les mesures et recadrages ont été faits en Python (Pillow, numpy) dans le dossier temporaire de la session (`…\scratchpad\qa\`), hors du dépôt. |
| **Date** | 21 octobre 2026 |

Conventions :
- **Coordonnées** : pixels de l'image d'origine (1366 × 768), origine en haut à gauche, zone notée (x0–x1, y0–y1).
- **Luminance** : sRGB linéarisé, puis Y = 0,2126 R + 0,7152 G + 0,0722 B. C'est la luminance de l'image affichée, après exposition. L* est calculé par CIE.
- **Profils** : « Q » pour Qualité, « C » pour Cinématique, « P » pour Performance.
- **Verdicts** : **AMÉLIORÉ**, **INCHANGÉ**, **RÉGRESSION**, **PARTIEL** (amélioré, mais critère d'acceptation non tenu), **NON VÉRIFIABLE** (les captures ne permettent pas de juger).

---

## 0. Synthèse

| Changement (commit) | Q | C | P | Verdict |
|---|---|---|---|---|
| Image nette sans VHS (`768a2f0`) | aberration 0,6 px → 0 ; reflets d'objectif disparus | idem | idem | **AMÉLIORÉ**. Grain : **NON VÉRIFIABLE** en temporel |
| Caustiques des Poolrooms (`768a2f0`) | plafond et trottoirs secs propres, fond conservé | idem | idem | **AMÉLIORÉ**. Liseré mural de 90 cm vu sur 2 poses sur 3 |
| Plafond et brume des Poolrooms (`17f2e9b`) | plafond −8 à −12 % ; noirs inchangés | idem | idem | **PARTIEL**. « Brume moins laiteuse » non mesurable |
| Lumière du Level 0 (`e881217`) | recoins −28 à −40 % ; diffuseurs plus brûlés | idem | plafond +5 %, pixels écrêtés +28 % | **PARTIEL**, et **RÉGRESSION** légère en Performance |
| Papier peint du Level 0 (`0d87534`) | lés lisibles, relief adouci ; mur/moquette 1,2 à 1,8 | idem | 1,2 à 2,1 | **PARTIEL** : critère ≤ 1,0 non tenu |
| Cadre des plafonniers (`1eebfc2`) | taches −68 % | −62 % | −79 % | **AMÉLIORÉ** |
| Chanfreins du kit (`e359095`) | liseré sur la plinthe à 80 cm ; rien de visible au-delà de 2 m | idem | idem | **AMÉLIORÉ** (plinthes seulement). Porte de service et luminaires : **INCHANGÉ** à l'image |
| Combinaison et visière (`5e3d710`, cartes de `v417-suit`) | plis brillants et bord de visière sous lumière froide ; faible gain sous néon chaud | idem | idem | **AMÉLIORÉ** (modéré sous néon) ; mouvement et réseau **NON VÉRIFIABLES** |
| Flaques des Niveaux 1 et 2 (`4f2a947`) | N1 : reflets −75 % mais taches sombres présentes ; N2 quasi inchangé | idem | N1 : reflets −14 % | **PARTIEL** |
| Inventaire (lot interface) | texte lisible, silhouette à 2,3:1 ; icônes à 1,7:1 ; capture « carnet » erronée | idem | idem | **AMÉLIORÉ**, avec des critères non tenus |
| Animation de la Bacteria (`f14dbf7`) | entité cachée par un pilier après l'image 008 | entité invisible | entité à 12 m, presque immobile | **NON VÉRIFIABLE** |
| Animation du joueur (`14b2a4c`) | pieds hors cadre dans toutes les vidéos | idem | idem | **NON VÉRIFIABLE** |

**État** : il reste du travail (« NEEDS WORK »). Les améliorations nettes concernent l'image nette, les caustiques, le cadre des plafonniers, les lés et la plinthe. Les critères chiffrés des agents ne sont pas tenus sur la hiérarchie des valeurs du Level 0, les diffuseurs, les noirs des Poolrooms, les icônes de l'inventaire et les flaques. Les deux lots d'animation ne sont pas démontrés : le banc ne montre ni les appuis de la Bacteria ni les pieds du joueur. **Neuf poses ne sont pas comparables** (§ 1), parce que l'état du directeur de tension, les entités et les néons qui clignotent diffèrent entre les deux exécutions.

---

## 1. Fiabilité du banc : ce qui invalide une comparaison

Ces points passent avant les verdicts : ils disent quelles différences ne viennent pas du code.

### 1.1 Directeur de tension non figé : Smilers, effets de peur, image noire

- **[Mesuré, journaux]** `game.log` des 6 exécutions, lignes « Tension » : les états diffèrent au même moment du banc.
  - Capture `l0_coupure_lampe` pendant une **poursuite** : avant Q, avant C, après C, après P.
  - Même capture pendant une **détection** : avant P, après Q.
- **[Observé]** Les Smilers apparaissent en des endroits différents :
  - `v417_l0_combinaison_neutre_face`, Q : Smiler à gauche avant (x ≈ 35–200, y ≈ 215–335), à droite après (x ≈ 1130–1280, y ≈ 210–330) ;
  - `neutre_dos` : déplacement de 260 px mesuré par corrélation de phase ;
  - `l0_coupure_lampe`, Q avant : les dents d'un Smiler remplissent le haut de l'image (sursaut). Q après : trois Smilers visibles.
- **[Mesuré, fiches .json]** Sur `l0_coupure`, `l0_coupure_lampe`, `l0_retour_15` et `l0_retour_40`, la pose de caméra n'est pas la même : roulis de −3,9° à +3,9° et champ de 82° à 91° au lieu de 88° (secousse de peur). Les images ne sont pas alignables (corrélation ≤ 0,15).
- **[Mesuré]** `apres/Performance/v417_l0_coupure_lampe.png` est **entièrement noire** : pixel maximal 2/255, avec `lampe: true`.
  - Elle est prise pendant une poursuite (journal : « détection → poursuite » 3 s avant).
  - [Hypothèse] Cause probable : l'exposition du personnage baisse de −3 × `ScareDark` (`BRCharacter.cpp`, cité dans `eclairage.md` § B.1).
  - La lampe allumée ne donne alors aucune image, et les yeux du Smiler ne se voient pas non plus. À traiter comme un cas de lisibilité, même si le changement v4.17 n'en est pas la cause démontrée.
- **Conséquence** : `coupure`, `coupure_lampe`, `retour_15`, `retour_40`, `combinaison_neutre_face`, `combinaison_neutre_dos`, `entite_bacteria` et la vidéo `l0_bacteria` (§ 2.11) **ne servent à aucun avant/après**. Les écarts trouvés là (par exemple `retour_15` en P : Y 0,225 → 0,124) ne sont pas attribuables au code.
- **À faire** : dans le banc, figer le directeur (pas d'apparition, `ScareDark` à 0) pour toutes les poses, sauf celles qui testent une entité. Écrire l'état de tension dans la fiche .json.

### 1.2 Néons qui clignotent : stroboscope à environ 15 Hz au Niveau 1

- **[Mesuré]** `apres/Qualite/v417_video_l1_marche_115…120`. La caméra avance régulièrement (8,7 cm par image, fiches). La moyenne de l'image vaut 0,127 / 0,062 / 0,061 / 0,112 / 0,062 / 0,111. Le sol (0–1366, 600–768) passe de **0,048 à 0,173 entre 117 et 118** (× 3,6).
- Avant (images 060–073) : alternance d'amplitude × 1,4 au plus. Après en C : images 123–131, × 2.
- [Constat] Le phénomène est préexistant et tiré au hasard (`eclairage.md` § A.2, `FMath::FRand`) ; ce n'est pas une régression du code v4.17. Pourtant :
  - (a) le banc ne fige pas les néons, alors que c'était recommandé (`eclairage.md` § D.0) ;
  - (b) une alternance plein écran de × 2 à × 3,6, image par image, en « flashs normaux », est dans la plage photosensible (3 à 30 Hz). Il faut vérifier que le réglage FLASHS la limite.

### 1.3 Vidéo de la Bacteria non reproductible

La caméra suit la capsule (`BRAutoTestV417Visual.cpp`, `AddVideo("l0_bacteria")`), mais le trajet de l'entité change d'une exécution à l'autre. Détails au § 2.11.

### 1.4 La capture « carnet » montre le JOURNAL

- **[Observé]** `v417_l0_inventaire_carnet_fr.png`, avant et après, dans les 3 profils : l'onglet actif est « > JOURNAL ».
- [Source] Le banc demande l'onglet `1` (`InvShots`). Le rapport `v416vis` de la même tête confirme « onglet 1 (attendu 1) » pour JOURNAL et « onglet 4 » pour CARNET.
- Aucune capture v4.17 ne montre donc le carnet. Le cas « carnet long » est **NON VÉRIFIÉ** dans `autotest/v417_apres_eb3cc8f/v416vis/Rapport.txt` (« rien à faire défiler à 1280 x 720 »).

### 1.5 Pose VHS des Poolrooms

`p_vue_vhs`, Q : le centre (500–870, 300–470) passe de 0,160 à 0,206 (+29 %), contre +4 % pour `p_vue`. En C et en P, l'écart n'est que de +9 %. La pose est prise 1,2 s après le basculement du VHS : [Hypothèse] l'exposition est encore en train de s'adapter. Cette pose ne sert pas à juger le mode caméscope.

### 1.6 Pas fixe : corrigé

Les fiches des vidéos avancent de 1/30 s (0,03 à 0,04 s, arrondi) et le rapport indique « pas de jeu de 0.0333 à 0.0333 s ». Le défaut relevé par `animation.md` § 0 est corrigé. `l0_3p` et `p_3p_eau` ont la même trajectoire avant et après (fiches identiques aux images 010, 014 et 020) : le blocage de `SteerWalk` n'apparaît plus.

### 1.7 Conditions de la capture « après »

- **Éditeur, pas le paquet.** `game.log` charge les DLL `UnrealEditor-*` : aucune capture ne valide le paquet Shipping (prompt § 7).
- **Arbre modifié.** `commit.txt` donne `modifie=1`. Le seul fichier modifié, `Config/DefaultEngine.ini`, contient une section `AndroidFileServer` ajoutée par l'éditeur, avec un jeton : aucun effet de rendu, **à ne pas commiter**.
- **Version.** Le rapport annonce « Jeu : v4.16 », car `Config/DefaultGame.ini` garde `ProjectVersion=4.16.0.0`.
- **Arbre de travail.** `BRAutoTestV417Visual.cpp` est en cours de modification par un autre intervenant (compteur `SeenFrames` ajouté). Mes verdicts portent sur les captures de `eb3cc8f`.

---

## 2. Vérification par changement

### 2.1 Image nette sans VHS (`768a2f0`)

**Captures** : `l0_salle`, `l0_dos`, `p_vue`, `l1_vue`, `l2_vue` (fiches `vhs: false`, `grain: false` avant et après), et `*_vhs` pour le mode caméscope.

**Aberration chromatique.** Mesure : décalage R → B par corrélation de phase sous-pixel, sur des carrés de 160 px aux 4 coins.

| Pose | Q avant → après | C | P |
|---|---|---|---|
| `l0_salle`, bas gauche / bas droite | −0,58 / 0,49 px → 0,00 / 0,00 | −0,59 / 0,41 → 0,00 / 0,00 | −0,59 / 0,59 → 0,00 / 0,00 |
| `l1_vue`, haut gauche / haut droite | −0,81 / 0,81 → 0,00 / 0,00 | −0,84 / 0,84 → 0,00 | −0,77 / 0,76 → 0,00 |
| `p_vue`, `l0_dos`, `l2_vue` | de 0,2 à 0,75 px → ≤ 0,01 | idem | idem |

Centre : 0 avant comme après, ce qui contrôle la méthode. **AMÉLIORÉ**, dans les 3 profils.

**Reflets d'objectif.** `l2_vue` (560–800, 260–560) : avant, des orbes à x ≈ 675–695, y ≈ 325–480 ; après, aucun. Sur l'image de différence amplifiée × 6, ils ressortent en taches claires au centre. **AMÉLIORÉ**.

**Grain : NON VÉRIFIABLE.** Aucune séquence à caméra fixe n'est comparable entre l'avant et l'après (vidéo de la Bacteria : caméra qui tourne en Q après). L'énergie haute fréquence d'une zone plate monte un peu après (plafond de `l0_salle` (700–900, 60–120) : 3,74 → 4,13 en Q). C'est cohérent avec la suppression de l'aberration (image plus piquée), pas avec un grain ajouté.

**Mode caméscope.** `l0_salle_vhs` reste distinct : franges cyan et rouge au bord du chambranle (x ≈ 455–465, y ≈ 500–560) et vignette (coins / centre 0,60–0,65). Les modes VHS d'avant et d'après sont identiques à ±1 % de luminance (L* 48,4 → 48,3). **INCHANGÉ**, comme voulu.

**Effet secondaire à surveiller.** Sans aberration ni saleté d'objectif, la hachure diagonale de la moquette se lit plus nettement (`l0_plinthe` (900–1366, 580–768), recadrage × 2). Le risque de moiré en mouvement est déjà signalé par `direction_artistique.md` § 1.1 point 10. Rien à corriger sur image fixe.

### 2.2 Caustiques des Poolrooms (`768a2f0`)

| Critère (`direction_artistique.md` § 2, point 2) | Observation après, Q | C | P | Tenu ? |
|---|---|---|---|---|
| Pas de filament sur les dessus secs | `p_vue` banc (20–400, 560–640), `p_trottoir` trottoir bas (330–670, 700–768) : aucun filament (avant : filaments nets) | idem | idem | **oui** |
| Plafond : rien au-dessus d'un trottoir | `p_vue`, `p_dos`, `p_contrejour`, `p_rasant` : plafond uni. Énergie haute fréquence de `p_contrejour` (300–1000, 40–250) 0,031 → 0,014 | 0,033 → 0,013 | 0,031 → 0,020 | **oui** |
| Au-dessus des canaux : reflet plus doux | plafond au-dessus du canal de `p_vue` (1050–1300, 90–200) : uniforme avant et après (0,030 → 0,029). Le « reflet doux et flou » annoncé ne se distingue pas | idem | idem | non discernable |
| Fond conservé, sous l'eau | `p_sous_eau`, `p_trottoir`, `p_bassin` : caustiques présentes au fond et sur les parois immergées | idem | idem | **oui** |
| Liseré mural de 90 cm | visible sur `p_rasant` (mur de droite, x ≈ 1040–1320, y ≈ 265–410) et sur `p_bassin` (x ≈ 1040–1360, y ≈ 80–340) ; **absent** sur `p_contrejour` (600–1200, 450–700) | idem | idem | en partie |
| En mouvement, sans scintillement | `p_3p_eau` : saut maximal d'une image à l'autre de 1,4 % (avant 1,4 %), aucune inversion | 1,4 % | 3,1 % | **oui** (moyenne d'image seulement) |

**AMÉLIORÉ**. Reste :
- vérifier pourquoi le liseré manque sur le mur de `p_contrejour` (distance à l'eau, ou mur hors du masque) ;
- les reflets des plafonniers sur l'eau restent des **taches blanches à bord net** (`p_bassin` x ≈ 590–720, y ≈ 400–470 ; `p_trottoir` x ≈ 690–800, y ≈ 560–640). Elles sont inchangées et loin des « reflets contrôlés » demandés (prompt § 3).

### 2.3 Plafond et brume des Poolrooms (`17f2e9b`)

| Mesure | Q avant → après | C | P | Critère |
|---|---|---|---|---|
| `p_dos`, plafond (300–1000, 40–150) | 0,319 → 0,292 | 0,311 → 0,286 | 0,349 → 0,325 | ≤ murs |
| `p_dos`, mur gauche (0–210, 300–470) / mur droit (1100–1366, 280–500) | 0,159 → 0,164 / 0,186 → 0,195 | 0,152 → 0,155 / 0,178 → 0,186 | 0,130 → 0,133 / 0,193 → 0,200 | **non tenu** (plafond 1,5 à 2,4 fois les murs) |
| `p_vue`, plafond du canal / murs gauches (0–330, 250–420) | 0,337 → 0,306 / 0,316 → 0,323 | 0,332 → 0,301 / 0,312 → 0,319 | 0,356 → 0,332 / 0,304 → 0,313 | tenu après en Q et en C (non tenu avant), pas en P |
| `p_vue`, image p1 / p99 | 0,060 → 0,061 / 0,662 → 0,681 | 0,064 → 0,066 / 0,668 → 0,687 | 0,033 → 0,034 / 0,768 → 0,782 | p1 ≤ 0,01 **non tenu** ; p99 ≥ 0,75 non tenu en Q et en C |
| `p_vue`, salle de droite (980–1366, 180–420) | halo laiteux identique à l'œil | — | — | « brume moins laiteuse » **non visible** |

- La teinte du plafond baisse de 17 % (1,08 → 0,9), mais le plafond ne baisse que de 8 à 12 % à l'image : l'exposition remonte les murs de 2 à 6 %.
- La baisse de densité de la brume ne se voit pas dans les noirs : p1 et p5 montent même un peu (p5 de `p_vue` 0,088 → 0,093).
- **PARTIEL**. Pour obtenir des noirs, il faut agir sur l'exposition ou l'occlusion (fonds d'arches, `direction_artistique.md` point 7), pas seulement sur la teinte.

### 2.4 Lumière du Level 0 : rebond 0,8 et rayon visuel de 560 cm (`e881217`)

**Recoins.** [Mesuré] Q, avant → après :
- `l0_dos`, dessous du linteau (590–780, 330–360) : 0,050 → 0,036 ;
- `l0_dos`, vue à travers la baie (600–770, 380–520) : 0,059 → 0,041 ;
- `l0_raccord_plafond`, tiers haut : 0,155 → 0,093 ;
- `l0_fosses`, tiers du milieu : 0,087 → 0,063.

C donne les mêmes rapports à ±1 %. **Recoins plus sombres : AMÉLIORÉ.** Le critère « angles au moins deux fois plus sombres que le mur voisin » est tenu : 0,036 contre 0,418 pour le mur de face de `l0_dos`. Il l'était déjà avant (0,050 contre 0,444).

**Critères non tenus** :
- **Part de très sombre** (< 0,01, hors bandeau) : `l0_salle` Q 1,6 → 1,8 %, C 1,7 → 1,8 %, P 1,0 → 1,2 % ; `l0_dos` 0,2 % partout. L'objectif est de 4 à 10 %.
- **Diffuseurs, `l0_salle`** (pixels Y > 0,5 dans (560–660, 100–160)) :
  - part ≥ 250/255 : Q **64,6 → 79,1 %**, C 73,7 → 79,7 %, P 83,2 → 88,1 % ;
  - part à 255 (écrêtée) : P **22,4 → 28,7 %** ;
  - p95 / p5 : 1,14 → 1,14, donc aucune structure.
  - Avec la zone de l'agent pour `l0_dos` (600–820, 100–135) : ≥ 250 passe de 0,5 à 62,5 % en Q.
  - L'objectif est moins de 2 %. **Les diffuseurs sont plus blancs qu'avant**, parce que l'exposition remonte (moquette +9 à +10 %).
- **Performance** : le tiers haut de `l0_salle` passe de 0,399 à **0,417 (+5 %)**, alors qu'il baisse de 5 % en Q (0,263 → 0,250). L'écart de plafond entre P et Q passe de 1,52 à **1,67**. La planche indique 0,44 → 0,65 % de pixels brûlés.
  - Le réglage 2 de l'agent éclairage (projecteur au lieu de la lumière ponctuelle en P) n'a pas été intégré. Le halo au plafond et le point chaud du diffuseur restent, un peu aggravés.
  - **RÉGRESSION légère en Performance.**

**En mouvement** (`l0_marche`, entrée du couloir sombre, images 039–042) : −11 à −14,5 % par image (avant : −8 à −11 %), avec une seule inversion avant comme après. L'entrée dans le noir est plus franche, sans pompage. Aucun bord d'atténuation à 560 cm n'est visible sur les images 036–044.

### 2.5 Papier peint du Level 0 (`0d87534`)

- **Teinte du mur à l'image** : Q 49,4° → 45,8° (`l0_dos`, 800–1050, 330–570), saturation 0,355 → 0,347. C et P donnent le même écart (−3,5 à −4°). Le mur est moins vert, sans filtre global. Il reste dans la plage du banc des couleurs (42–58°).
- **Hiérarchie mur / moquette** (critère ≤ 1,0, médianes ; zones : piliers et murs de `l0_salle` d'après `direction_artistique.md`, moquette (400–1366, 620–768)) :

| Rapport | Q avant → après | C | P |
|---|---|---|---|
| `l0_salle`, milieu du pilier / moquette | 2,07 → 1,82 | 2,09 → 1,83 | 2,42 → 2,04 |
| `l0_salle`, mur gauche / moquette | 1,36 → 1,18 | 1,37 → 1,18 | 1,53 → 1,23 |
| `l0_dos`, mur de face / moquette (0–1366, 650–768) | 1,77 → 1,52 | 1,79 → 1,53 | 2,19 → 1,78 |

  **Non tenu.** Le mur ne baisse que de 4 à 6 % (milieu du pilier en Q : 0,583 → 0,558), pendant que la moquette monte de 9 % (0,282 → 0,307). L'exposition automatique a absorbé l'essentiel de la baisse d'albédo d'environ 21 %. Pour tenir le critère, il faudra baisser encore l'albédo, ou fixer la cible d'exposition, ou les deux.
- **Lés de 53 cm : AMÉLIORÉ, critère tenu.**
  - `l0_plinthe` : 3 joints successifs visibles (x ≈ 55 → 225 en diagonale, x ≈ 700, x ≈ 1035).
  - `l0_raccord_mur` : 5 joints (x ≈ 150–250, 420–480, 680–690, 890–940, 1130–1210).
  - J'ai vérifié qu'un joint suit bien la perspective des rayures et ne tranche pas le motif : pente du joint 0,139 contre 0,16 pour la rayure voisine, et 0,14 attendu pour une verticale à x ≈ 445 avec un tangage de −25°.
- **Relief** : les chevrons se lisent comme imprimés, sans biseau marqué (`l0_plinthe` à environ 80 cm). Contraste relatif du motif (écart-type de Y rapporté à son flou de 31 px) :
  - `l0_rasante` à 1 m : 0,197 → 0,154 ; à 5 m (830–1100, 200–420) : 0,244 → 0,211 ;
  - `l0_dos` à 3 m : 0,172 → 0,137 ;
  - même baisse de 12 à 24 % en C et en P.
  
  Les chevrons restent identifiables à 3 m ; au fond de `l0_rasante` (5 à 6 m), on ne lit plus guère que des rayures.
- **Inchangé, non traité** : la ligne de marée à la même hauteur sur tous les murs (`l0_raccord_mur` y ≈ 585 ; `l0_dos` ; `l0_plinthe` y ≈ 500–540 côté gauche).

### 2.6 Cadre des plafonniers (`1eebfc2`)

- `l0_salle`, barre haute du cadre central (540–672, 82–87), écart-type relatif de Y :
  - Q **0,256 → 0,082** ; C 0,276 → 0,105 ; P 0,180 → 0,037.
  - Recadrage × 2 de (500–900, 40–260) : la marbrure de `T_Grime` a disparu et le cadre se lit comme de la tôle peinte beige.
  - **AMÉLIORÉ**, critère « cadre sans taches de `T_Grime` » tenu.
- **Inchangé, non traité** :
  - diffuseur sans prismes (§ 2.4) ;
  - ossature du plafond identique avant et après (aucun changement prévu dans ce lot). Je n'ai pas mesuré le critère « aucune traverse terminée au milieu d'un côté » sur les poses v4.17 : il reste ouvert.

### 2.7 Chanfreins des plinthes et chambranles (`e359095`)

- **Plinthe** : `l0_plinthe` (880–1080, 560–660), recadrage × 4. Après, un liseré clair continu suit l'arête haute de la plinthe sur toute la longueur visible ; avant, l'arête était vive et sombre. Critère tenu à environ 80 cm. Le liseré fait 1 à 2 px : mes profils verticaux ne le captent pas, et le constat reste visuel.
- **Chambranle** : `l0_dos` (540–640, 290–420), à environ 3 m, recadrage × 4 : aucune différence discernable. C'est attendu pour un chanfrein de 6 mm.
- **Joints** : aucune fente lumineuse aux raccords dans `l0_raccord_mur` ni `l0_plinthe`.
- **Porte de service : INCHANGÉ** (`l0_porte_fermee`, rmse 4,2/255).
  - Battant, cadre et embrasure ont toujours la même matière tachée.
  - Les filets bleus du jeu du battant sont toujours là (x ≈ 548 et ≈ 811, y ≈ 158–208).
  - C'est conforme à `kit_blender.md` § 6.1 : la porte et les luminaires du kit ne sont pas posés en jeu. Le message de commit « chanfreins … porte de service » ne doit pas être lu comme un changement visible.
- **Verdict** : **AMÉLIORÉ** pour les plinthes. Le critère sur la porte (`direction_artistique.md` point 9 : battant, cadre et mur distincts) n'est pas traité.

### 2.8 Combinaison et visière (`5e3d710`, cartes dérivées)

- **Branchement** : `apres/Qualite/game.log`, l. 1824–1832. Les slots `HazmatMask`, `HazmatGlass` et `HazmatSuit` ont le parent `M_BR_Gear`, aux LOD 0, 1 et 2, avec « usage squelette OK, secours non ». Le matériau est réellement utilisé dans l'éditeur.
- **Lumière froide** (`combinaison_neutre_face`, (530–840, 170–560), recadrage × 1,4) :
  - après, les hautes lumières suivent les plis du bras gauche et du torse : PVC brillant ;
  - la visière garde un reflet net et un bord qui s'éclaircit (Fresnel) ;
  - p95 du jaune 0,174 → 0,202 en Q.
  - **AMÉLIORÉ, à l'image.** Attention : cette pose n'est ni « neutre » ni comparable (§ 1.1). Elle est prise dans une zone du Level 0 sans lumière, avec un Smiler dans le champ, et non au Level 4 sous néons blancs comme le prévoit `materiaux_combinaison.md` § 6.
- **Néon chaud**. Masque des pixels jaunes, (p99 − p50) / p50 ; écrêtage ≥ 0,98 à 0 % partout :

| Pose | Q | C | P |
|---|---|---|---|
| `neon_face` | 0,58 → 0,68 | 0,57 → 0,69 | 0,63 → 0,74 |
| `neon_dos` | 0,54 → 0,57 | 0,48 → 0,55 | 0,61 → 0,68 |
| `neon_cote` | 0,62 → **0,59** | 0,63 → 0,61 | 0,80 → 0,90 |

  Le p99 ne bouge pas (environ 0,63). L'écart vient d'un p50 plus bas (creux plus sombres, occlusion), pas de hautes lumières nouvelles. Sous le néon chaud du Level 0, le gain est **modeste**, et nul de côté en Q et en C.
- **Visière** : plus sombre que le PVC de face (pixels sombres de la visière, p50 0,008 en Q). Les trois reflets des plafonniers sont un peu plus nets après (`neon_face`, 625–735, 200–290). Critère tenu.
- **Gants et bottes** : lisibles et distincts (bottes orange, gants sarcelle). Aucune ligne visible aux raccords d'îlots à cette distance (1,5 à 3 m).
- **NON VÉRIFIABLE** avec ces captures :
  - scintillement en mouvement (`l0_3p` : silhouette de dos à environ 2,4 m, pas de mesure temporelle des reflets) ;
  - lampe allumée ;
  - distance de 8 à 10 m ;
  - coéquipier vu par l'hôte et par un client (« NON VÉRIFIÉ » dans `v416vis`) ;
  - paquet Shipping.

### 2.9 Flaques des Niveaux 1 et 2 (`4f2a947`)

Sol : Niveau 1 (150–1210, 480–768) ; Niveau 2 `l2_vue` (0–320 et 520–820, en bas) et `l2_dos` (0–1366, 600–768).

| Mesure | Q avant → après | C | P |
|---|---|---|---|
| `l1_vue`, part brillante (> 1,6 × médiane) | 11,5 → **2,9 %** | 12,4 → 3,2 % | 26,2 → 22,5 % |
| `l1_vue`, part sombre (< 0,6 × médiane) | 30,9 → **27,6 %** | 31,5 → 28,0 % | 28,4 → 27,1 % |
| `l1_dos`, brillante / sombre | 6,7 → 1,7 % / 26,2 → 19,6 % | 10,0 → 1,9 % / 27,2 → 20,2 % | 21,4 → 16,3 % / 24,7 → 21,3 % |
| `l2_vue`, sombre (< 0,5 ×) / brillante (> 1,8 ×) | 21,2 → 17,6 % / 10,0 → 8,8 % | 22,0 → 18,3 % / 9,8 → 8,6 % | 19,6 → 16,4 % / 8,7 → 7,4 % |
| `l2_dos`, sombre / brillante | 18,5 → 17,9 % / 14,0 → 13,7 % | 18,4 → 18,1 % / 14,0 → 14,2 % | 16,5 → 16,4 % / 15,3 → 15,2 % |

- Niveau 1 : les reflets brillants reculent fortement en Q et en C. Mais **les taches sombres à bord dur restent sur 20 à 28 % du sol** (`l1_vue`, recadrage (140–1220, 480–768)). L'effet « camouflage » relevé par `direction_artistique.md` § 1.4 n'a pas disparu.
- Niveau 2 : l'image de différence ne montre qu'un retrait des contours des flaques (liserés au sol). L'aspect est **quasi inchangé** (rmse 4,1/255).
- En Performance, l'effet est faible (part brillante −14 %).
- **PARTIEL**. Reste : séparer, dans le shader, l'assombrissement humide de la flaque proprement dite, ou réduire la part sombre, et revoir le Niveau 2.

### 2.10 Inventaire (lot interface)

Captures : `v417_l0_inventaire_personnage_{fr,de,ar}` (vraie fenêtre 1366 × 768, sur la scène du Level 0), `v416vis/v417_manette_port` et `v417_manette_parametres` (Niveau 1).

- **AMÉLIORÉ** :
  - onglets, objectifs et noms en case nettement plus grands et lisibles sans zoom (recadrage × 2 de (440–840, 150–370)) ;
  - touches de poche en pastilles ;
  - curseur de la manette visible (cadre jaune à coins) ;
  - légendes TÊTE, TORSE, MAIN et CEINTURE sans chevauchement, OBERKÖRPER compris en allemand.
- **Silhouette** (1080–1200, 300–540) contre le panneau (990–1290, 160–260) : contraste **1,03:1 → 2,30:1**, étendue p5–p95 34–45 → 44–113 niveaux. Critères C4 tenus (≥ 2:1, ≥ 30 niveaux). Remarque de direction : la vue est un mannequin **gris neutre**, alors que la combinaison du joueur est jaune.
- **Manette** : `v417_manette_port` montre l'objet porté sur la 4e poche, la source estompée avec « x2 » et le pied « [Y] POSER [CROIX] CHOISIR [B] ANNULER ». `v417_manette_parametres` montre la ligne LUMINOSITÉ en focus, avec son aide. Le branchement existe dans `BRPlayerController.cpp` (l. 811–812, 1247, 1324, 1555).
- **Critères non tenus** :
  - icônes sur leur halo (critère ≥ 3:1) : caméscope (458–542, 296–362) **1,70 → 1,71:1** ; lampe en CEINTURE (1225–1295, 450–503) 1,49 → 1,67:1 ;
  - POCHES et STOCKAGE toujours empilés lettre par lettre (x ≈ 437–443) : C11 non fait ;
  - pied entre crochets en petits caractères ;
  - aide des PARAMÈTRES toujours en bas, loin de la ligne (C12 non fait) ;
  - arabe : mise en page de gauche à droite (C9 non fait).
- **Défauts préexistants, inchangés** :
  - notifications en français dans l'interface allemande et arabe (« TÂCHE ACCOMPLIE : FILMER UNE ENTITÉ », « Nouvelle entrée du journal… »), probablement créées avant le changement de langue du banc ;
  - mélange de casses dans OBJECTIFS (casse de phrase, mais « FILMER UNE ENTITÉ » en capitales).
- **HUD en jeu** (`l0_hud_jeu`) : 5 notifications empilées au centre haut de l'écran (y ≈ 80–245), avant comme après. La pastille estompée « Vue à la 1re personne » (x ≈ 600–765, y ≈ 114–137) passe sur le diffuseur central et se lit mal. Ce n'est pas discret ; c'est à confier au lot interface. Aucune barre de vie : acquis conservé.
- **Carnet : NON VÉRIFIÉ** (§ 1.4).

### 2.11 Animations

**Bacteria (`f14dbf7`) : NON VÉRIFIABLE.**
- Q après (`v417_video_l0_bacteria_000…089`, toutes les images vues en planche, puis recadrage de (450–950, 180–520) aux images 005, 040 et 085) :
  - la Bacteria n'est visible qu'au début, en partie, au bord d'un pilier (image 005, x ≈ 665–700, y ≈ 295–440) ;
  - elle est ensuite masquée : la caméra tourne de 0° à 15,5° de lacet pour suivre une capsule cachée.
- C après : elle n'est visible sur aucune des 15 images échantillonnées.
- P après (recadrage (560–820, 250–480)) : elle reste à environ 12 m, presque immobile, puis pivote d'environ 90° entre 082 et 089 (environ 13° par image).
  - Le critère « ≤ 25° par bond » serait tenu, mais cette mesure est faite à l'œil.
  - Les pattes fines se dissolvent pendant le pivot (085–089), ce qui fait penser au TSR en Performance à cette distance.
- Avant Q : elle reste dans le couloir et s'approche lentement. Le trajet n'est donc pas le même : il dépend de l'état de tension (§ 1.1).
- Les fiches des vidéos ne contiennent **aucun point d'appui**. Les crochets `GetSupportPoints`, `GetVisualYaw` et `GetVisualLag` (`animation.md` § R.4) ne sont pas relevés. Aucun critère d'acceptation (appui ≤ 0,5 cm par image, griffe ≤ 1 cm par image, lacet ≤ 15° par image en course) n'est mesurable.
- L'image fixe `l0_entite_bacteria` n'est pas comparable : l'entité est plus loin après (pieds à y ≈ 574 contre 600–630 avant). Aucune ombre de contact ne se voit sur la moquette rouge, ni avant ni après : critère « appuis au sol ombrés » non tenu.

**Corps du joueur (`14b2a4c`) : NON VÉRIFIABLE.**
- Dans `l0_3p` et `p_3p_eau`, avant et après, le cadre coupe le personnage à mi-cuisse : **les pieds ne sont jamais visibles**. Glissement, double appui, bassin et marche d'escalier ne sont pas mesurables.
- Observation à confirmer, sans mesure : en Q après, images 010–014 (380–880, 300–768), le buste paraît tourné vers la gauche par rapport à la caméra, alors que la trajectoire est droite (+x, lacet 0 dans les fiches). Avant, le corps est dans l'axe.
  - [Hypothèse] Le bas du corps est encore en retard après le replacement instantané du personnage par `Place()`.
  - À vérifier avec `BRRig::GetBodyPoseInfo` (`YawOffset`) dans la fiche.

---

## 3. Problèmes trouvés (par priorité)

1. **Animations non démontrées** (critique pour la clôture du lot). Aucune image ne montre un appui de la Bacteria en marche, ni un pied du joueur (§ 2.11). Faute de fiches d'appui, les critères des deux lots ne peuvent pas être vérifiés.
2. **Banc non figé** (critique pour toute comparaison). Tension, Smilers, effets de peur et néons qui clignotent changent d'une exécution à l'autre. Neuf poses et la vidéo de la Bacteria ne sont pas comparables. `coupure_lampe` en P après est entièrement noire avec la lampe allumée (§ 1.1, 1.3).
3. **Diffuseurs du Level 0 plus brûlés** (moyen). Pixels ≥ 250 : 64,6 → 79,1 % en Q ; pixels à 255 : 22,4 → 28,7 % en P. En Performance, le plafond est plus clair qu'avant et l'écart entre profils augmente (1,52 → 1,67). Le réglage « projecteur en Performance » n'est pas intégré (§ 2.4).
4. **Hiérarchie mur / moquette toujours inversée** (moyen). Rapport de 1,18 à 1,82 en Q et en C, jusqu'à 2,04 en P, pour une cible ≤ 1,0. L'exposition compense la baisse d'albédo (§ 2.5).
5. **Poolrooms toujours laiteuses** (moyen). p1 = 0,061 contre ≤ 0,01 visé ; plafond de `p_dos` 1,5 à 2,4 fois les murs. La baisse de brume ne se voit pas (§ 2.3).
6. **Flaques du Niveau 1** (moyen). Les reflets ont reculé, mais les taches sombres à bord dur couvrent encore 20 à 28 % du sol ; le Niveau 2 est quasi inchangé (§ 2.9).
7. **Stroboscope du Niveau 1** (moyen, accessibilité). Le sol varie de × 3,6 d'une image à l'autre, sur 5 alternances en 0,2 s (`l1_marche` 115–120), en flashs normaux (§ 1.2).
8. **Capture « carnet » erronée et carnet long non vérifié** (faible pour l'image, bloquant pour le § 4.6 du prompt) (§ 1.4).
9. **Icônes de l'inventaire** sous 3:1 (caméscope 1,7:1, lampe 1,7:1) ; POCHES et STOCKAGE empilés ; aide des réglages loin de la ligne (§ 2.10).
10. **Version et conditions de capture** (faible). Le paquet annonce « v4.16 ». Les captures viennent de l'éditeur, sur un arbre modifié : section `AndroidFileServer` avec jeton dans `DefaultEngine.ini`, à ne pas commiter (§ 1.7).
11. **Reflets des plafonniers sur l'eau** : taches blanches à bord dur, inchangées (§ 2.2).
12. **HUD en jeu** : pile de 5 notifications au centre ; une pastille estompée posée sur un diffuseur se lit mal (§ 2.10).

## 4. Critères d'acceptation des agents

| Source, critère | Résultat |
|---|---|
| DA 1 : médiane du mur ≤ médiane de la moquette (`l0_salle`, `l0_dos`) | **non tenu** (1,18 à 1,82 en Q) |
| DA 1 : chevrons identifiables à 3 m | tenu |
| DA 1 : Smiler trois fois plus sombre que le mur | non vérifiable (pas de pose `mur_smiler` en v4.17) |
| DA 2 : pas de caustiques sur les trottoirs secs ni au plafond au-dessus d'un trottoir | **tenu** (Q, C, P) |
| DA 2 : caustiques du fond qui bougent sans scintiller (`p_3p_eau`) | tenu (moyenne d'image) |
| DA 3 : 4 à 10 % de pixels sous 0,01 | **non tenu** (1,8 %) |
| DA 3 : angles deux fois plus sombres que le mur | tenu (déjà avant) |
| DA 5 et Matériaux § 6 : hautes lumières qui suivent les plis | tenu sous lumière froide ; faible sous néon |
| DA 5 et Matériaux § 6 : visière plus sombre que le PVC, bord plus clair | tenu |
| DA 5 et Matériaux § 6 : 8–10 m, réseau, paquet | non vérifiable |
| DA 5 : appuis de la Bacteria ombrés | **non tenu** ou non vérifiable |
| DA 7 : plafond ≤ murs (`p_dos`) | **non tenu** |
| DA 7 : p1 ≤ 0,01 | **non tenu** |
| DA 7 : p99 ≥ 0,75 | tenu sur `p_dos`, non tenu sur `p_vue` |
| DA 8 : cadre sans `T_Grime` | **tenu** |
| DA 8 : diffuseur avec structure, < 2 % à 250 | **non tenu** (aggravé) |
| DA 8 : ossature du plafond | non traitée, non mesurée |
| DA 9 : liseré de plinthe | **tenu** à 80 cm |
| DA 9 : pas de fente lumineuse | tenu |
| DA 9 : porte de service | non traité |
| DA 11 : trois raccords de lés successifs | **tenu** (`l0_plinthe`, `l0_raccord_mur`) |
| DA 11 : chevrons sans biseau marqué | tenu |
| Éclairage D.1 : pas de pompage en marche | pas d'inversion au Level 0 ; les essais D.1 (néon puis mur, fosse, coupure figée) ne sont pas dans le banc |
| Interface C1, C3 (noms), C4, C7, C10 | tenus à l'image |
| Interface C5 (INSPECTER) | non regardé ici |
| Interface C3 : icônes ≥ 3:1 | **non tenu** |
| Interface C8 : carnet long | **NON VÉRIFIÉ** (banc v4.16) |
| Interface C9, C11, C12 | non faits |
| Animation, priorités 1 à 3 | **non vérifiables** (pas de fiche d'appui, pieds hors cadre) |

## 5. Non vérifié dans cette QA

- Niveau 6 et LiDAR (« aucune lumière nouvelle ») : aucune capture v4.17 ; `v416lidar` non examiné.
- Reflets RT des entités et de la combinaison selon le profil.
- Plein écran et réglages persistants.
- Paquet Shipping.
- Linux et macOS.
- Combinaison en mouvement (scintillement).
- Joueur à 4 et coéquipier client.
- Grain en temporel.
- Fosses : bord et damier inchangés (pose `l0_fosses`, rmse 13/255, essentiellement de l'exposition). C'est attendu : le point 4 de la DA n'a pas été traité.

## 6. Reproduction

Scripts du dossier temporaire de la session (`…\scratchpad\qa\`, non versionnés) :
- `qalib.py` : luminance, L*, zones, planches ;
- `global_cmp.py` : 40 poses × 3 profils, rapport des moyennes, corrélation de phase, rmse, écarts des fiches ;
- `nette.py` : aberration chromatique ;
- `zones.py` : mur / moquette et diffuseur ;
- `contraste.py` : contraste du motif ;
- `pool.py` : Poolrooms ;
- `suit.py` : combinaison ;
- `expo.py` : courbes des vidéos ;
- `strips.py` : bandes de vidéo, une image sur 6, puis images successives.

Toutes les zones citées ci-dessus suffisent à refaire chaque mesure sur `Saved/TestRuns/v417/{avant,apres}/<profil>/`.

## 7. Évaluation honnête

- **Qualité réelle** : B−. L'image est plus propre (netteté, caustiques, cadre, lés, plinthe). La passe n'atteint pas encore la hiérarchie de lumière visée par la direction artistique (murs, diffuseurs, noirs des Poolrooms), et ses deux lots d'animation ne sont pas prouvés.
- **Prêt** : non (« NEEDS WORK »).
- **Prochaine boucle conseillée** :
  1. figer le banc (tension, néons, apparitions) et ajouter les fiches d'appui ;
  2. ajouter une caméra latérale basse pour les pieds ;
  3. recalibrer l'exposition et l'albédo du Level 0 contre les diffuseurs ;
  4. intégrer le projecteur en Performance ;
  5. corriger l'onglet de la capture « carnet » ;
  6. refaire ensuite la même QA sur les mêmes poses.

---

**QA** : agent « Evidence Collector », 21 octobre 2026. Images examinées : 40 paires × 3 profils (planches et pleine résolution), plus de 60 recadrages, 5 vidéos × 3 profils en bandes (Q en entier pour `l0_bacteria`), 2 captures de la manette (`v416vis`) et les journaux des 6 exécutions.
