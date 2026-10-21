# THE BACKROOMS v4.17 — index des preuves visuelles

Toutes les images et vidéos citées ici sont dans le dépôt, sous [`Docs/v417/visuels/`](v417/visuels/). Ce sont des
captures du jeu telles qu'il les a écrites : rien n'est retouché. Les planches ne font que juxtaposer « avant » et
« après », avec la luminance mesurée sous chaque image.

## 1. Conditions

- **Banc** `-BRAutoTestV417Visual`, passage **7** du 21 octobre 2026 (06:23 à 06:59), lancé par
  `Tools/Tests/run_v417_visual.ps1 -Tag <avant7|apres7> -Profiles 1,2,0`.
  - **Avant** : copie de `a9ccf1c` (jeu v4.16, outillage de test v4.17).
  - **Après** : `ee3365e`. Le dernier changement du rendu, `2074046`, ne touche que le Performance hors Level 0 et
    Poolrooms : il est vérifié au passage `apres9` (Performance).
  - Le banc est le même des deux côtés : gel, caméras, trajet imposé de la Bacteria.
  - Les captures brutes et leurs fiches restent hors du dépôt : `Saved/TestRuns/v417/{avant7,apres7}/<profil>/`.
- **Image** :
  - fenêtre de 1366 × 768 ; rendu interne selon le profil, indiqué dans chaque fiche ;
  - portable RTX 5050 Laptop, Unreal 5.8.3, éditeur en mode jeu (`-game`), pas le paquet ;
  - VHS et grain coupés, sauf pour les poses `_vhs` ; luminosité 0 ; flashs normaux ; interface cachée, sauf pour le HUD
    et l'inventaire.
- **Banc figé** : ni rencontre, ni phénomène, ni coupure tirée, néons tenus allumés. Les coupures sont demandées au même
  moment du banc.
  - Fiches : 41 poses sur 41 au calme, frayeur 0, aucune entité hors de celle posée par le banc.
  - **Stabilité** (même code, `avant6` contre `avant7`) : PSNR médian de 42,7 dB ; 26 poses sur 41 au-dessus de 40 dB.
  - Les moins stables suivent le temps : Bacteria 27,7 dB, eau 28 à 33 dB, coupure et retour 33 à 35 dB.
  - Une différence plus petite que ces écarts ne prouve rien.
- **Fiche de chaque capture** (`<capture>.json`) : commit, niveau, graines, caméra, champ, résolutions, profil, temps
  depuis l'arrivée, alimentation, coupure, lampe, MegaLights, 3e personne, VHS, grain, interface, tension, frayeur,
  entités. Pour les vidéos d'après : appuis de la Bacteria, pose et os des pieds du joueur.
- **Fabrication des fichiers** :
  - `Tools/Visual/make_v417_media.py pairs` (planches JPEG) ;
  - `... video` (WebP animé à 30 images/s, la cadence du pas fixe) ;
  - gros plans recadrés et agrandis, sans autre traitement.

## 2. Planches avant / après

`v417/visuels/paires/<profil>/<pose>_avant_apres.jpg`. Les 41 poses sont en **Qualité**. En **Cinématique** et en
**Performance**, 8 poses : `l0_salle`, `l0_dos`, `l0_fosses`, `l0_combinaison_neutre_face`, `p_vue`, `p_dos`,
`l1_vue`, `l2_vue`. Le tableau dit ce qui a été regardé et ce qui reste.

Les planches de **Performance** viennent du passage `apres9` (`2074046`, Performance seulement). Au passage 7, le Niveau 1
y était trop sombre (`l1_vue` L* 48,2 → 29,9), ses tubes étant devenus des projecteurs. Le projecteur ne reste qu'au
Level 0 et aux Poolrooms, et le Niveau 1 revient à 48,8 (Qualité : 49,3). Au Level 0 et aux Poolrooms, `apres9` est
identique au passage 7.

### 2.1 Level 0

| Pose | Examiné | Constat (après) | Défaut restant |
|---|---|---|---|
| `l0_salle` | diffuseurs, murs, piliers, moquette, plafond | diffuseurs non écrêtés (≥ 250/255 : 71,5 → 0 % en Qualité) ; haut des murs et plafond plus sombres (volets 70°) ; papier peint moins clair et moins vert | murs encore plus clairs que la moquette (1,17 à 1,82, cible ≤ 1,0) ; pas de motif dans le diffuseur |
| `l0_dos` | mur de face, embrasure, diffuseur | mur moins clair, embrasure plus profonde | mur / moquette 1,53 |
| `l0_rasante` | papier peint à 1 et 5 m | chevrons adoucis, lés visibles | au-delà de 5 m, des rayures seulement |
| `l0_plinthe` | arête de plinthe, joints | liseré clair sur l'arête (chanfrein du kit), joints fermés | ligne de marée à la même hauteur partout |
| `l0_raccord_mur`, `l0_raccord_plafond` | joints entre chunks, plafond | aucune fente de lumière ; plafond plus sombre près du mur | — |
| `l0_porte_fermee`, `l0_porte_ouverte` | porte de service à la lampe | inchangées (le kit ne pose pas la porte) | filets bleus au-dessus du battant, matière tachée identique à l'embrasure |
| `l0_fosses` | bord, fond, parois | fonds plus noirs (p5 3,9 → 1,3 L*) ; en Performance, proche de la Qualité | damier des trous inchangé (direction artistique n° 4 non faite) |
| `l0_coupure`, `l0_coupure_lampe` | noir, halo de la lampe | noir tenu ; lampe lisible (pixel maximal 148 → 121) | — |
| `l0_retour_15`, `l0_retour_40` | rallumage à 15 et 40 s | comparable en Qualité et en Cinématique | en Performance « après », le courant était déjà revenu : pose sans valeur |
| `l0_salle_vhs` | mode caméscope | inchangé (franges, vignette) | — |
| `l0_entite_bacteria` | entité de contrôle, lumière rouge | plus sombre autour (L* 40,3 → 34,6) | aucune ombre de contact nette sous les pattes |

### 2.2 Combinaison (Level 0)

| Pose | Examiné | Constat (après) | Défaut restant |
|---|---|---|---|
| `l0_combinaison_neon_face`, `_dos`, `_cote` | PVC sous le néon chaud, visière, gants, bottes | creux plus sombres, plis lisibles, visière à reflets nets | sous le néon chaud, les hautes lumières n'augmentent pas (gain modeste) |
| `l0_combinaison_neutre_face` | lumière neutre de 6500 K posée par le banc | plis brillants sur les épaules et le torse, reflet net de la visière | fond plus sombre |
| `l0_combinaison_neutre_dos` | dos, lumière de l'autre côté | dos presque noir (L* 29 → 11,8) : la lumière est devant le joueur ; l'exposition locale ne relève plus les ombres | pose devenue sans valeur pour juger la matière de dos ; à refaire avec une lumière derrière |

### 2.3 Poolrooms

| Pose | Examiné | Constat (après) | Défaut restant |
|---|---|---|---|
| `p_vue` | caustiques, plafond, trottoirs secs, brume | plus de caustiques au plafond ni sur les dessus secs ; plafond un peu moins clair | voile laiteux : p1 0,060 → 0,051 (cible ≤ 0,01) ; p99 0,669 (cible ≥ 0,75) |
| `p_dos` | plafond contre murs | plafond / murs 1,86 → 1,73 | plafond toujours plus clair que les murs |
| `p_trottoir`, `p_bassin` | eau, fond, reflets | fond et parois immergées gardent leurs caustiques | reflets des plafonniers en taches blanches à bord net |
| `p_rasant`, `p_contrejour` | angle rasant, liseré mural | reflets rasants corrects ; liseré mural visible sur `p_rasant` | liseré absent sur `p_contrejour` |
| `p_sous_eau` | sous la surface | inchangé | — |
| `p_vue_vhs` | mode caméscope | prise 2,5 s après le basculement : exposition reposée | — |

### 2.4 Niveaux 1 et 2

| Pose | Examiné | Constat (après) | Défaut restant |
|---|---|---|---|
| `l1_vue`, `l1_dos`, `l1_mur` | sol du garage, béton, néons | moins de reflets brillants au sol | **taches sombres à bord dur sur 20 à 28 % du sol** : masque des flaques du matériau partagé (assombri × 0,55 puis × 0,7, bord très dur), pas la texture |
| `l2_vue`, `l2_dos`, `l2_mur`, `l2_plafond` | couloirs techniques | quasi inchangés | flaques du Niveau 2 à revoir |

### 2.5 HUD et inventaire

| Pose | Examiné | Constat (après) |
|---|---|---|
| `l0_hud_jeu` | barre rapide, notifications | touches des poches en pastille sombre, lisibles sur la moquette (gros plan § 4) ; aucune barre de vie |
| `l0_inventaire_personnage_fr`, `_de`, `_ar` | cases, icônes, silhouette, légendes, textes | textes plus grands ; icônes sombres cernées d'un contour ; « VIDE » lisible ; silhouette modelée ; allemand sans chevauchement |
| `l0_inventaire_journal_fr` | onglet JOURNAL (1) | corps de texte lisible |
| `l0_inventaire_carnet_fr` | onglet CARNET (4), nouvelle capture | texte du Level 0 plus grand |

Défauts restants de l'interface :

- arabe affiché de gauche à droite (C9 non fait) ;
- POCHES et STOCKAGE empilés lettre par lettre (C11 non fait) ;
- notifications en français dans les autres langues (préexistant) ;
- pile de cinq notifications au centre du HUD en jeu.

## 3. Vidéos (mouvement)

`v417/visuels/videos/<scène>_{avant,apres}.webp`, Qualité, 960 px de large (720 pour `l1_marche`), 30 images/s, pas de
jeu fixe de 1/30 s vérifié par le banc.

| Vidéo | Ce qu'elle montre | Constat |
|---|---|---|
| `l0_bacteria` (150 images) | Bacteria sur un trajet imposé, identique des deux côtés : marche vers le joueur (environ 2,4 m), demi-tour vers l'image 76, course de retour, arrêt ; caméra qui la suit à 3,5 m | voir § 5 |
| `l0_3p_cote` (90 images) | marche du joueur vue de côté, pieds dans le cadre | avant : pieds en l'air, sans contact ; après : semelles posées, ombre de contact (gros plan § 4). Après l'image 55, un mur rapproche la caméra : pieds coupés des deux côtés |
| `l0_3p` (90 images) | caméra de la 3e personne, de dos | pieds hors cadre (cadrage du jeu) |
| `l0_marche` (90 images) | marche à la 1re personne, entrée dans un couloir sombre | adaptation de l'exposition sans pompage |
| `p_3p_eau` (90 images) | marche dans l'eau des Poolrooms | caustiques et rides stables |
| `l1_marche` (150 images) | marche au Niveau 1, raccords de streaming | néons fixes : banc figé, voir § 6 pour le clignotement |

## 4. Gros plans

`v417/visuels/details/` :

- `diffuseur_l0_salle_Qualite.jpg` : diffuseur central × 3. Avant, écrêté ; après, blanc sans écrêtage, cadre en tôle
  propre.
- `l0_salle_Performance_contre_Qualite.jpg`, `l0_dos_Performance_contre_Qualite.jpg`, `l1_vue_Performance_contre_Qualite.jpg` :
  Performance avant / après (`apres9`), et Qualité après comme repère.
  - Level 0 : plus de plafond délavé ; tiers haut 0,403 → 0,179 (Qualité 0,241).
  - Niveau 1 : inchangé, comme voulu.
- `barre_rapide.jpg` : barre rapide en jeu × 2.
- `icones_contour.png` : les cinq icônes sombres à 64 px sur le fond des cases, v4.16 en haut, v4.17 en bas.
- `pieds_l0_3p_cote_010-032.jpg` : pieds sur un cycle de marche, une image sur deux.

## 5. Mesures tirées des fiches (animation)

`python Tools/Visual/v417_motion.py --src Saved/TestRuns/v417/apres7/Qualite`. Les crochets n'existent qu'après :
l'avant n'a que ses images.

**Bacteria** (passage 7, après ; le passage 8 donne les mêmes valeurs à 0,3 cm près) :

| Critère (`animation.md` R.1) | Mesuré | Tenu ? |
|---|---|---|
| Appuis posés, hors bond et hors course : ≤ 0,5 cm par image | glissement médian 0 cm ; p95 3,6 à 6,8 cm ; max 10 à 27 cm | **non**, sur les démarrages et le demi-tour |
| Dérive sur un appui ≤ 2 cm | 15 à 29 cm au plus | **non** |
| Lacet affiché en course ≤ 15° par image | 15,3° au plus, sur 32 images de course | presque (0,3° de trop) |
| Lacet ≤ 25° par bond | 59,7° pour le bond du demi-tour (les autres, environ 0°) | **non** au demi-tour |
| Écart entre le modèle et la capsule ≤ 40 cm | 38,6 cm au plus, 5,1 cm médian | oui |
| Course : corps porté au-dessus du sol | 4 à 8 cm au-dessus du sol sur les images de course | oui |

Image par image :

- **Allure à appuis** : pieds et griffes immobiles entre des bonds de rattrapage de 8 images, toutes les 25 images
  environ (0,8 s).
- **Démarrage depuis l'arrêt** : le modèle glisse de 3,5 cm par image pendant 3 à 4 images avant le premier bond.
- **Demi-tour** (image 76) : saut de lacet de 18° avec 11 cm de glissement, puis un bond qui tourne de 58°.
- Fin de course : arrêt au point de départ.

**Joueur, os des pieds** (passage 8, Qualité, `avant8` contre `apres8`, même trajet) :

| Mesure | v4.16 | v4.17 | Critère |
|---|---|---|---|
| Glissement du pied par appui (médian) | 34 à 41 cm | 15,6 à 15,8 cm | ≤ 3 cm : **non tenu** (−60 % environ) |
| Hauteur de l'os pendant l'appui (au plus bas) | 11,6 cm : le pied ne touche pas le sol | 6,6 à 6,7 cm : semelle à plat | — |
| Hauteur de l'os au plus bas du pas | 11,6 à 14,9 cm | 3,6 à 3,7 cm, plus bas que pendant l'appui | à vérifier : la pointe du pied en l'air frôle peut-être le sol |

Le point de foulée de `BRRig::GetBodyPoseInfo` (fiche « corps ») est une estimation au sol, pas l'os : il ne sert pas
à mesurer le glissement. Non mesuré : demi-tour sur place, escalier, accroupi.

## 5 bis. Réseau, interface et Level 6 (tests du jeu)

Tests du jeu sur `516ad17`, même code du jeu que `ee3365e`.

- `v417/visuels/reseau/` (test réseau `run_nettest.ps1 -Clients 1`, hôte et un client sur ce PC, 1280 × 720) :
  - `coequipier_vu_par_hote.jpg` et `coequipier_vu_par_client.jpg` : la combinaison d'un coéquipier, de face, à 2,3 m, telle
    que chaque machine la rend ;
  - le test relit les slots du personnage répliqué : 3 sur 3 sur `M_BR_Gear` chez l'hôte comme chez le client ;
  - à l'image : plis et reflet de la visière des deux côtés. Bras plus écartés dans la vue du client (pose de repos du
    coéquipier distant), à vérifier en mouvement ;
  - les fiches portent `commit : inconnu` : ce lanceur ne transmet pas `-BRCommit`.
- `v417/visuels/interface/` :
  - `carnet_niveau3_de_1280x720_tous_indices.jpg` (banc `-BRAutoTestV417Carnet`, `e612818`) : le carnet le plus rempli des
    7 niveaux à mission essayés (1, 2, 3, 4, 6, 9, 11 ; graine 4242), tous indices lus, en allemand, à la taille 1,25,
    dans une fenêtre de 1280 × 720 ;
  - **aucun ne déborde** de sa zone. Le défilement d'un carnet long n'a donc rien à faire défiler dans le jeu actuel, et
    reste **non vérifié** sur un vrai débordement ;
  - `v417_manette_port.jpg` : objet porté à la manette (4e poche), source estompée, pied « [Y] POSER [CROIX] CHOISIR
    [B] ANNULER » ;
  - `v417_manette_parametres.jpg` : ligne LUMINOSITÉ choisie à la manette, avec son aide ;
  - `v416_fenetre_ultralarge_max_personnage.jpg` : fenêtre ultra-large ;
  - `v416_hud_1280x720_ar_personnage.jpg` : arabe, mise en page encore de gauche à droite (C9 non fait) ;
  - `v416_hud_1920x1080_ja_carnet.jpg` : japonais ;
  - `v416_lidar_cone.jpg`, `v416_lidar_releve.jpg` : Level 6 dans le noir, cône de points du scanner, titre du niveau
    encore affiché par-dessus. Le test `v416lidar` est réussi (§ 7 du rapport).

## 6. Ce que ces preuves ne couvrent pas

- **Paquet** : rien de tout cela n'a été pris dans le paquet Shipping. Son contrôle est dans `RAPPORT_v4.17.md` § 6.
- **Clignotement des néons** : figé dans le banc. Le stroboscope du Niveau 1 en flashs « normaux » (QA visuelle § 1.2)
  n'est pas filmé ici.
- **Non filmé** :
  - caméra qui traverse la surface de l'eau ; marches immergées en mouvement ;
  - Hound, Skin-Stealer et autres entités en mouvement ;
  - objet tenu en main ; escalier ;
  - reflets en ray tracing des entités hors champ.
- **Machine de référence** RTX 4080 : aucune image.
