# v4.17 : animation procédurale et intégration des créatures (état initial v4.16)

Rôle : agent installé **Technical Artist** (`~/.claude/agents/technical-artist.md`), lancé comme
sous-agent générique pour l'animation procédurale et l'intégration des créatures (aucun agent « animateur » n'est
installé). Mission : § 4.5 de la demande v4.17. Analyse seule : aucun fichier du dépôt modifié hors de ce rapport ;
Unreal et Blender non lancés (mesures de performances en cours).

Images examinées une par une ou en planches (Python/Pillow, dossier de travail de la session, script `sheet.py`) :

- `Saved/TestRuns/v417/avant/Qualite/` (commit `a632a89`, profil Qualité, rendu 1093 × 614 → 1366 × 768) :
  `v417_video_l0_bacteria_000..089`, `v417_video_l0_3p_000..089`, `v417_video_p_3p_eau_000..089`,
  `v417_l0_entite_bacteria`, `v417_l0_combinaison_{neutre,neon}_*`, et leurs fiches `.json`, `Captures.csv`, `game.log` ;
- `Saved/TestRuns/autotest/final416/v416vis/v416_entite_{Bacteria,Hounds,SkinStealers}_*` (Level 4).

Conventions : coordonnées en pixels de l'image pleine (1366 × 768, origine en haut à gauche). Conversion pixel → sol :
œil à 164,2 cm, champ horizontal de 88° (focale 707 px), sol à z = 0 ; à 7 m, 1 px vertical ≈ 4 cm ; à 1,2 m sous un
regard de −30°, 1 px latéral ≈ 0,26 cm.

Étiquettes : **[Observé]** vu ou mesuré sur les images ou leurs fiches ; **[Code]** constat dans les sources
(fichier:ligne) ; **[Hypothèse]** cause ou effet à vérifier.

---

## 0. Préalable : les séquences « à pas fixe de 1/30 s » ne le sont pas

- **[Observé]** `game.log` l. 2047-2072 (l0_marche) et 2232-2324 (l0_bacteria) : une capture par image moteur
  (`[317]`, `[318]`, `[319]`…), espacées de **0,19 à 0,29 s réelles**. Dans chaque fiche, `temps_depuis_arrivee_s`
  avance de **0,100 s exactement** par image. Pour `l0_marche`, `Captures.csv` donne **55,0 cm par image** en
  moyenne, avec la pleine vitesse dès la 2e image. Or 260 cm/s × 0,21 s = 55 cm, alors que 260 cm/s × 1/30 s = 8,7 cm.
- **[Code]** `BRAutoTestV417Visual.cpp:320-321` appelle `FApp::SetFixedDeltaTime(1/30)` puis
  `FApp::SetUseFixedTimeStep(true)` depuis l'étape de test (Tick d'un acteur). Le monde borne son pas à 0,1 s
  (`BRWorld.cpp:1591`), comme les entités (`BREntity.cpp:1135`) et le joueur (`BRCharacter.cpp:2684`). Le déplacement
  du `CharacterMovementComponent`, lui, utilise le pas réel.
- **[Hypothèse]** En 5.8, `FAppTime` est transmis par contexte (`Engine/Source/Runtime/Core/Private/Misc/AppTime.cpp:25-67`).
  La valeur posée depuis le Tick d'un acteur serait écrasée par un `FApp::Set*` ultérieur du contexte de la boucle
  moteur. Les options de ligne de commande `-UseFixedTimeStep -FPS=30` sont lues à l'initialisation
  (`LaunchEngineLoop.cpp:2460`, `4727-4730`).
- **Conséquence pour les mesures** : sur ces vidéos, la phase de marche (`AnimTime`, `BodyAnim`, `BobTime`) avance de
  0,1 s pendant que le corps parcourt 0,19 à 0,29 s de trajet. La cadence vue relativement au déplacement est donc 2 à
  3 fois trop basse, et un glissement du pied y paraît 2 à 3 fois trop fort. La période de balancement de la tête de
  `l0_marche` le confirme : 8 images, soit 432 cm de trajet, alors que le jeu donne 204 cm à 260 cm/s
  (`BRCharacter.cpp:3057`). **Exception** : un pied rigide (Bacteria) glisse de 100 % du déplacement quel que soit le
  pas ; ce constat-là ne dépend pas du défaut du banc.
- **[Observé] autre défaut du banc** : `SteerWalk` (`BRAutoTestV417Visual.cpp:180-204`) oscille dans les coins. En
  `l0_3p`, le pion est immobile (≤ 0,2 cm par image) de **028 à 089**, avec un lacet qui alterne −135 / −144 / −153°.
  En `p_3p_eau`, il est bloqué de **056 à 089**, avec un lacet qui alterne −18 / −36°. 62 + 34 images ne montrent
  aucune marche.

---

## A. Défauts observés et causes

### A.1 Bacteria (entité de contrôle du Level 0)

Le système existe et fonctionne sur les images fixes. Modèle protégé en pièces rigides : corps avec ses jambes, tête,
deux bras-béquilles (`BuildBacteriaModel`, `BREntity.cpp:515-558`). Appui des pièces rigides
(`KeepRigidPartsAboveGround`, `3056-3113`) : `v417_l0_entite_bacteria.png` (griffes et pied, x 585-835, y 480-625)
montre les appuis posés sur la moquette, sans enfoncement. Le correctif v4.16 tient.

| Images | Zone | [Observé] | Mesure |
|---|---|---|---|
| 000-017 | x 610-760, y 215-560 | En attente à ~12 m, face à la caméra : corps et pattes immobiles ; seule la tête se tord (010-014). | Appuis au sol fixes (y ≈ 518 constant). |
| 016-018 | pieds, y 540-560 | Points scintillants sur les fils fins au démarrage. | Probable reconstruction TSR sur des fils de 1-2 px. Ce n'est pas un défaut d'animation. |
| 019 | — | Départ de la poursuite sans préparation : la silhouette grandit dès l'image suivante. | — |
| 020 → 021 | appuis bas, x 615-765 | Caméra identique (même position, tangage −4,36°). Les points bas de la silhouette (pied et griffes) passent de y ≈ 490 à y ≈ 518. | **+28 px**, soit de ≈ 718 à ≈ 609 cm : **≈ 100 cm de glissement des appuis en une image** (0,27 s réelle). Les pattes ne bougent pas : le corps est translaté d'un bloc. |
| 022-023 | bras | Bras ouverts vers l'avant (geste « Reach ») ; la griffe droite est à hauteur de taille en 022, puis au sol en 023. | — |
| 023 → 024 → 025 | silhouette entière | 023 de face, 024 **de profil** (bras tendu vers la gauche), 025 de nouveau de face, bras grands ouverts (armé). | Hauteur apparente > 720 px (≤ 2,4 m), puis ≈ 500 px (tête y ≈ 80, pieds y ≈ 580, ≈ 3,4 m), puis > 720 px. Rotation d'environ **90° en une image** ; écart latéral ≈ 1 m. |
| 028-089 | arche du pied, x 430-1040, y 600-768 (caméra figée, −30°) | Debout au contact du joueur, pied rigide. L'arche saute de **30 à 60 px** (≈ 8 à 15 cm) d'une image à l'autre en 029→030, 045→046, 061→062, 077→078 ; elle est stable entre ces sauts. | Période de **16 images**. Le pied d'appui glisse alors que la créature est « à l'arrêt ». |
| 040-042, 056-058, 072-074, 088 | plein cadre | À chaque frappe, un bras de 2,3 m balaie la caméra. | Part de l'écran couverte par le bras : **28 % (040)**, 17 % (042), 13 % (058), 21 % (072), contre 6,5 % hors frappe. |

**[Code] Causes**

- **Glissement en marche (020→021)** : le corps et les jambes forment une seule pièce rigide
  (`BREntity.cpp:523`, appuis `Feet` 542-543). Aucune jambe n'est animée ; le corps suit la capsule (Visual attaché à la
  capsule, 208-209). Pendant la marche, le Visual monte et descend de 0 à 3 cm (`Bob`, 3238 et 3245). Comme
  `KeepRigidPartsAboveGround` ne fait que relever, le pied flotte de 0 à 3 cm en glissant : c'est exactement le cas
  « pied proche du sol » qui ne valide pas une marche.
- **Bras-béquilles qui frottent** : les bras sont des `UpperArm` en pendule (528-529, amplitude 16° × allure,
  3380-3390). La cadence vient de valeurs par défaut sans rapport avec les bras : `LegLength` 90 cm et amplitude 26°,
  car il n'y a pas de cuisse (3147-3161). Les bras, eux, mesurent environ 230 cm. Par demi-cycle, la griffe recule
  de 2·230·sin(16°·allure) quand le corps avance de 2·90·sin(26°·allure). Cela donne **81 contre 51 cm en marche**
  (140 cm/s) et **187 contre 113 cm en poursuite** : la griffe recule de 30 à 74 cm de plus que le corps n'avance. Rien
  ne fixe la griffe au sol : elle rase la moquette sur tout l'arc, à 0-3,5 cm en marche et jusqu'à 20 cm aux extrémités
  en poursuite. Les pas sonores (1245-1250, toutes les π/Ω = 0,26 s en poursuite) n'ont aucun pas visible.
- **Rotation en une image (023→024)** : `bOrientRotationToMovement` (202) avec une vitesse de rotation de **540°/s**
  (151, 273). `FollowPathTo` vise les **centres de cellule** (1750-1758) : un point de passage latéral fait pivoter le
  corps de profil, d'un bloc, sans pas. Même chose à l'arrêt « elle m'a vu ? » (`FacePlayer`, 1815-1828, appelé en
  2852-2853) et à l'armé (`RInterpConstantTo` à 1,5 × 540°/s, 1885-1893) : le pied rigide pivote autour du centre
  de la capsule.
- **Arrêts et départs secs** : `UpdateGait` (1965-1974) fait passer `GaitScale` de 1 (1,25 en poursuite) à 0,05 sans
  transition. L'amplitude des bras suit l'allure à chaque image (3381).
- **Pied qui saute pendant les attaques (028-089)** : **[Hypothèse]** cela vient surtout de la capsule elle-même. Les
  sauts arrivent 6 images après chaque frappe (040/056/072/088 → 046/062/078), soit vers la fin de la récupération
  (0,7 s, 1942-1949). La vitesse demandée revient alors à 1 (`AttackMoveScale` 1858-1872) et la capsule pousse contre
  celle du joueur (`MoveTowards`, 1647-1700). Seconde cause possible : la fente rigide `Strike·16 − WindupAnim·4` cm et
  le tangage du Visual entier (3246, 3251-3255) déplacent le pied avec le corps.
- **Bras dans la caméra** : la frappe lève le bras de **105°** (3384) avec une allonge de 230 cm. À 1 m du joueur, la
  griffe traverse forcément la tête et le plan proche. Rien ne limite le geste à la surface de la cible.
- **[Hypothèse, non observée] Griffes dans les portes** : au repos, l'envergure des griffes vaut environ 151 cm
  (griffes à −82 et +69 cm d'après 530-541, roulis de 6° compris), pour une porte du Level 0 de **150 cm**
  (`BRLevels.cpp:50`). Le roulis de « Reach » (+8°, 3382) et celui de l'armé (+22°, 3385) l'élargissent encore. Les
  griffes n'ont aucun test contre les murs. Il faut une capture (§ C).

### A.2 Combinaison du joueur (3e personne, coéquipiers)

Le système existe : squelette `SK_Hazmat` piloté os par os (`PoseSkin`, `BRCharacter.cpp:3938-3989`), poses de marche,
course, accroupi, nage, échelle et mort (`AnimateBody`, 3787-3932), objet tenu (3735-3741, 3898-3908), lissage réseau
par le maillage du Character (3669-3672).

| Images | Zone | [Observé] | Mesure |
|---|---|---|---|
| `l0_3p` 000-026, `p_3p_eau` 000-049 | bas du cadre | **Les pieds ne sont jamais dans le cadre** : perche de 240 cm, regard horizontal, personnage coupé aux cuisses (y 768). Le glissement du pied ne peut pas être mesuré sur ces séquences. | — |
| `l0_3p` 010-014 | personnage entier | Le corps longe le mur de côté avec un cycle de marche vers l'avant : démarche « en crabe ». | Pion reconstruit depuis la caméra (perche −240/45/18 cm) : il se déplace vers ≈ 180° alors que le corps regarde vers −108..−126°. **Écart de 43 à 72°** pendant 5 images. |
| `l0_3p` 015-016, 023 | cadre | La perche heurte le mur : la caméra saute contre le dos (capuche plein cadre en 015) puis revient (z 163 → 173 cm). | Écart de 204 cm en une image (reconstruction faussée par la collision). |
| `l0_3p` 027-028 | gant droit, x ≈ 663-670, y ≈ 655-660 | Pion collé à l'angle d'une porte : le gant est coupé net par l'arête du chambranle, puis réapparaît en 029-030 quand le lacet change de 9°. | Compatible avec une main **entrée dans le chambranle** (à confirmer, § C). |
| `l0_3p` 028-089 | corps | Pion immobile, lacet de ±9° par image : le corps entier pivote sans un pas. | — |
| `p_3p_eau` 044-049 | bras | Marche dans l'eau : bras écartés (roulis « Wade »), cohérent. | — |
| `p_3p_eau` 050-055 | corps | Passage de la marche à la nage en 2 à 3 images : chute (z 170 → 61 cm), corps vertical, jambes repliées, un bras tendu. | La transition se lit ; sa durée réelle est à revoir à pas fixe. |
| `combinaison_neutre_face`, `neon_cote` | bottes, x ≈ 560-760, y ≈ 640-768 | Pose de repos : bottes à plat sur la moquette. | **Une image fixe ne valide pas la marche.** |

**[Code] Causes et effets chiffrés**

- **Cadence sans lien avec la géométrie de la jambe** : `BodyAnim += Dt·Speed2D/45` (3816-3819) fixe un pas de
  π·45 = **141 cm** à toute vitesse. Or la hanche est à 93,9 cm et l'amplitude vaut 30° × allure (`BRRig.cpp:39-48` ;
  3808, 3829). En appui, le pied recule de 2·93,9·sin(30°·allure) :
  - **marche (260 cm/s)** : 94 cm pour un pas de 141 cm, soit **≈ 47 cm de glissement par pas (34 %)** ;
  - **course (470 cm/s, allure plafonnée à 1,6)** : 140 cm, ≈ 2 cm (1 %) ;
  - **accroupi (140 cm/s)** : amplitude ≈ 10°, glissement de l'ordre de 85 % ;
  - **dans l'eau (marche à ×0,6)** : ≈ 59 %.

  Ce sont des **[Hypothèses chiffrées]** à vérifier sur une capture où les pieds sont visibles.
- **Pas de contact au sol** : `BodyFeet` est à hauteur fixe (3931), sans abaissement du bassin ni rayon sous les pieds.
  Jambe tendue à ±30°, les deux pieds sont **12,6 cm au-dessus du sol** à chaque double appui (93,9·(1−cos 30°)),
  et 31 cm en course (±48°). Sur une marche ou un trottoir, `MaxStepHeight` vaut 35 cm (86) : un pied flotte ou entre
  dans la marche d'autant.
- **Rotation sur place** : `bUseControllerRotationYaw = true` (75). Le corps entier suit la souris : les pieds pivotent
  autour de la capsule, et aucun bassin ne tarde ni ne fait de pas.
- **Démarche en crabe** : la cadence vient de `Speed2D`. Seul le signe de la vitesse avant inverse le cycle (3818) ; les
  jambes ne se balancent que dans le plan du corps. Un déplacement latéral (glissement contre un mur, pas de côté)
  joue donc une marche avant.
- **Sons de pas désynchronisés** :
  - joueur local : `BobTime`, un pas toutes les π/8 s ≈ **0,39 s** à la marche (3057-3066) ;
  - corps visible : un pas toutes les **0,54 s** ;
  - coéquipier entendu : minuterie fixe de **0,46 s** (2912-2916).
- **Objet tenu** : il est placé à `RightHand + 6 cm` dans l'axe de l'**avant-bras** et orienté sur cet axe (3901-3906) :
  pas de point de prise dans la paume ni de décalage par objet. Seule la lampe a un maillage (770-789) : un caméscope
  ou un scanner tenu est invisible pour les coéquipiers. En 3e personne, le faisceau part de l'œil, avancé de 32 cm
  (3100-3105, 2896-2906), et non de la lampe visible.
- **Bras sans collision** : capsule de rayon 34 cm (74), épaules à ±27,5 cm et coudes à ±32,8 cm. Le balancement de
  ±26° × allure (3831) porte la main jusqu'à ≈ 41 cm devant l'épaule en course. Tenant un objet, l'avant-bras est
  à l'horizontale et suit la visée (3881-3889). Rien n'arrête la main dans un mur ou un chambranle (027-028).
- **Coude** : en tenue, le coude est plié à **62°** (3888) et ne bouge pas ; il va jusqu'à 47° en marche accroupie
  (3833). `SK_Hazmat` est protégé, sans os de torsion ajouté. Le pincement à 60° et plus est probable, mais **aucune
  capture ne le montre** (§ C).
- **Pas de temps** : `AnimateBody` reçoit le pas borné à 0,1 s (2684). Aucun effet à 60 i/s ; c'est la cause du biais
  du banc (§ 0).

### A.3 Hound (marcheur à squelette, Hound allégé compris)

- **[Observé]** Uniquement des images fixes v4.16 au Level 4 :
  - `1p_repos` : pattes avant à plat sur la moquette (x ≈ 630-735, y ≈ 520-537), sans enfoncement visible ;
  - `1p_menace` : tête plus basse ;
  - `3p_cote` : patte avant en appui.

  Aucune séquence en mouvement n'existe.
- **[Code]** Le système existe : `SK_Hound` ou `SK_HoundLite` selon le réglage (612-619, 703-722), quatre pattes de deux
  segments (632-646), pli des bas de pattes selon le sol (`UpdateFootPlanting`, 3002-3006), animation des os espacée
  par la distance (`ApplySkins`, 1338-1358).

  Limites mesurées sur le code :
  - **Cadence plafonnée** : `Omega` est borné à 16 rad/s (3161). En poursuite (465 × 1,12 ≈ 520 cm/s, amplitude
    24 × 1,5 = 36°, `LegLength` 67 cm), il faudrait 20,8 rad/s. Le pied d'appui glisse alors d'environ
    **23 cm par appui**.
  - **Pattes arrière plus courtes** : la cadence vient des hanches avant (67 cm, 646), alors que les hanches arrière
    sont à 58,5-59,3 cm (676-681). À même angle, les pattes arrière glissent de **≈ 12 %** en marche et
    **≈ 33 cm par appui** en poursuite.
  - **Demi-tour sur place** : sous 5 cm/s, `Omega` vaut 0 (3160) et les pattes ne bougent plus. Le corps tourne à
    240°/s (140, 273) : des pattes à ±37 cm du centre balaient **≈ 116 cm d'arc par demi-tour**.
  - **Pas de verrou horizontal** : le pli d'appui sert seulement à monter sur un sol plus haut (3000-3006). Rayons à
    20 Hz, bornés à −25..+35 cm (2959-2981), et rien hors de la vue à plus de 25 m (2945).

### A.4 Skin-Stealer (marcheur à squelette)

- **[Observé]** Les quatre captures v4.16 montrent seulement la **masse** de repos (état inactif, `Morph` = 0). Sa
  marche, son déguisement et sa déchirure n'ont aucune image.
- **[Code]**
  - Hanche à 100,8 cm, amplitude 26° (`BRRig.cpp:74-82`). La cadence reste sous le plafond même en poussée :
    616 cm/s donnent 15,3 rad/s. Le glissement en ligne droite est faible en théorie.
  - Il reste exposé au **demi-tour sur place** (300°/s, 143), à l'absence de verrou horizontal et aux escaliers.
  - **Continuité** : le corps « pousse » par mise à l'échelle (0,5 + 0,5·M, puis M ; 3496-3501). La fin du déguisement
    remet `Morph` à 0,3 d'un coup (3485-3490). La poussée finale passe de 0,85 à 1,4 × la vitesse, avec une
    accélération de 3200 (1993-2012), sans geste qui l'annonce.
  - Les cuisses sont rattachées au Visual et non au buste (`BRRig.cpp:233`) : la penche de poursuite (3256-3264) ne
    déplace pas le bassin. C'est correct pour l'appui.

### A.5 Systèmes déjà présents, à réutiliser

Squelettes à pivots (`FBRSkinDriver`, `BRRig.h`), cadence liée à la distance parcourue (`Animate`), appui vertical par
IK à deux segments (`UpdateFootPlanting`, vérifiée hors moteur par `Tools/check_leg_ik.py`), appui des pièces rigides
(`KeepRigidPartsAboveGround`), **mains plantées au sol avec pas déclenchés** du Clump (`AnimateClumpSkin`, 786-921 :
`Planted`/`Swing`/seuil de pas, deux mains au plus en l'air), poses d'état (`UpdateStatePose`), Hound allégé, animation
des os espacée par la distance (`ApplySkins`). Aucun n'est à déclarer absent ; les propositions ci-dessous les prolongent.

---

## B. Améliorations prioritaires (autour des silhouettes existantes, sans remodelage)

Ressources protégées (`RawAssets/protected_assets.json`, groupe `derived` : `SM_BacteriaET_*`, `SK_Hazmat`, `SK_Hound`,
`SK_HoundLite`, `SK_SkinStealer`…) : aucune géométrie ni aucun poids n'est touché, seulement des poses calculées.

| # | Amélioration | Où | Réseau / coût | Acceptation (banc v4.17, après le § C.0) |
|---|---|---|---|---|
| 1 | Bacteria : appuis verrouillés et allure de béquilles | `BREntity.cpp` : `Animate` 3164-3221 (nouveau cas Bacteria) et 3235-3255 ; bras 3380-3390 ; `BuildBacteriaModel` 525-556 | Visuel local seulement | `l0_bacteria` avec fiches d'appui |
| 2 | Combinaison : cadence géométrique, bassin, pivot sur place, pas sonores | `BRCharacter.cpp` : 3808-3833, 3925-3931, 75, 3057-3066, 2912-2916 | Local et coéquipiers, rien de nouveau répliqué | Nouvelle séquence pieds visibles, `l0_3p` corrigée |
| 3 | Marcheurs : verrou horizontal du pied, pas de rotation, cadence du Hound | `BREntity.cpp` : `UpdateFootPlanting` 2939-3023 ; `Animate` 3147-3161 ; `AnimateLimbs` 3395-3408 | Visuel local | Nouvelle séquence Hound et Skin-Stealer |
| 4 | Continuité attente → marche → poursuite → attaque | `BREntity.cpp` : 1750-1758, 1815-1828, 1885-1893, 1965-1974, 3381-3385 | En partie serveur (IA) | `l0_bacteria` |
| 5 | Objet tenu, faisceau, bras contre les murs, coude | `BRCharacter.cpp` : 3735-3741, 3881-3908, 3100-3105, 2896-2906 | Local ; `NetHand` déjà répliqué | Gros plans de main, porte |
| 6 | Lissage réseau du modèle des entités | `BREntity.cpp` : 208-209, 2956-2957, 3039-3041, 3068-3069, 3117 | Client seulement, coût nul | Test réseau à 2 instances |

### 1. Bacteria : appuis verrouillés et allure de béquilles (priorité 1, défaut observé à 100 %)

**Principe.** Le modèle a un pied d'appui rigide (le gauche, `Feet` 542) et deux griffes. On le fait avancer comme
un corps rigide **sur béquilles**, au lieu de le faire glisser.

- **Verrou du pied.** On garde le point d'appui en coordonnées monde. Tant qu'il est verrouillé, le Visual reçoit le
  décalage qui le maintient en place, en position et en lacet : le corps pivote autour de son pied et non du centre de
  la capsule. Ce décalage est borné : environ 45 cm en marche, et le verrou est forcé à moins de
  `AttackRange` + 60 cm de la proie pour frapper depuis la vraie place.
- **Bond.** Quand la borne est atteinte, ou quand le lacet dépasse 25°, un bond de 0,15 à 0,25 s ramène le Visual sur
  la capsule en arc : montée de 4 à 6 cm, tangage avant de 4°, pied décollé. Le corps retombe ensuite (léger
  tassement, saccade de la tête). En marche, c'est exactement le caractère « saccadé, arrêts et départs secs » déjà
  voulu par `UpdateGait`.
- **Griffes.** On reprend l'algorithme du Clump (786-900) : une griffe plantée reste au monde, et la rotation du bras
  rigide vient de l'axe de repos épaule → griffe aligné sur l'épaule → point planté (`FindBetweenNormals`, comme en
  893). Un seul bras à la fois en l'air ; la griffe se repose devant au moment du bond.
- **Poursuite (> 250 cm/s).** Le corps est porté par les bras : Visual relevé de 5 à 8 cm, donc pied hors du sol, et
  griffes alternées verrouillées. Elle « court sur ses béquilles », les jambes rigides pendantes, au lieu de patiner.
  Le passage d'un mode à l'autre suit une interpolation douce de la vitesse, entre 150 et 250 cm/s.
- **Frappe.** On remplace la fente en translation (3246) par un tangage autour du point d'appui : le pied reste en
  place.
- **Portes et murs.** Deux rayons latéraux à hauteur d'épaule (10 Hz) replient les bras, avec un roulis vers le corps
  de 12° au plus et sans « Reach », quand un mur ou un chambranle est à moins de 60 cm.
- `KeepRigidPartsAboveGround` reste la garde finale.

**Risques.** Tout est visuel et calculé sur chaque machine à partir de la vitesse et de la rotation répliquées, et de
`State`, `Target` et des multicasts d'attaque : aucune nouvelle propriété répliquée, aucun effet sur les coups (la
capsule décide). Les clients peuvent différer de quelques centimètres entre eux. Les rayons des griffes sont lancés au
moment de poser (2 à 4 par seconde) : environ 0,01 ms par Bacteria, rien au-delà de 25 m. À éviter : poser au-dessus
d'une fosse (même repli que le Clump, 849-853), et le verrou pendant un jumpscare (`bScareOverride`).

**Acceptation** (`l0_bacteria`, à vrai pas fixe, fiches d'appui du § C.0) :

- pied verrouillé : déplacement monde ≤ 0,5 cm par image et ≤ 2 cm par appui ;
- en poursuite : pied à ≥ 3 cm du sol sur ≥ 90 % des images en mouvement ;
- griffe plantée : dérive ≤ 2 cm sur son appui ;
- variation du lacet visible ≤ 15° par 1/30 s (aujourd'hui ≈ 90° entre 023 et 024) ;
- bas des volumes jamais sous −1 cm (contrôle v4.16 conservé) ;
- `Tools/attack_timing.py` inchangé (aucun réglage d'IA modifié) ;
- à l'œil : bonds et appuis lisibles dans la vidéo assemblée (`Tools/Visual/make_v417_media.py`).

### 2. Combinaison : cadence géométrique, bassin, pivot sur place, pas sonores (priorité 2)

**Principe.**

1. **Cadence.** On reprend la formule des entités (`BREntity.cpp:3159-3160`) :
   `BodyAnim += Dt·π·v/(2·L·sin A)`, avec L = 93,9 cm et A = 30° × allure (minimum 8°). Accroupi, L vient de la
   distance hanche → cheville de la pose accroupie, mesurée une fois. Le pas de marche devient ≈ 94 cm (≈ 2,8 pas/s
   à 260 cm/s) au lieu de 141 cm.
2. **Bassin.** `BodyFeet` descend de L·(1 − cos θ) de la jambe d'appui, celle où cos(Ph) < 0. On ajoute un rayon sous
   chaque pied à 20 Hz et l'IK à deux segments déjà validée (`BREntity.cpp:3008-3021`, `Tools/check_leg_ik.py`) sur
   `LeftUpLeg/LeftLeg` : marches, trottoirs des Poolrooms, escalier du sas.
3. **Pivot sur place.** Un lacet du bas du corps, appliqué à `BodySkin` avec une contre-rotation de `Spine`, reste en
   arrière du lacet de visée. Il ne le rattrape qu'au-delà de 50° d'écart ou dès que le joueur avance, par deux petits
   pas (`BodyAnim` avancé de π, amplitude 12°).
4. **Direction du déplacement.** Le bas du corps s'oriente vers la vitesse jusqu'à ±60°. Au-delà de 110°, on garde le
   cycle inversé existant. Cela supprime la démarche en crabe.
5. **Pas sonores.** `UpdateCamera` (3057) prend le même `Omega` : un balancement et un pas sonore par demi-cycle. Les
   pas des coéquipiers (2912-2916) passent sur les demi-cycles de `BodyAnim` au lieu d'une minuterie.

**Risques.** Rien de nouveau n'est répliqué : les coéquipiers sont animés depuis la vitesse et le lacet répliqués,
avec le lissage par le maillage existant. Le balancement de la caméra à la 1re personne passe de 2,55 à environ
2,8 pas/s à la marche, ce qui est perceptible : il faut garder le réglage « balancement désactivable ». Pas d'effet
sur l'ouïe des entités : les pas locaux sont des sons 2D et le bruit vient de `GetNoiseRadius`. Coût : 2 rayons à
20 Hz par corps visible (au plus 4 joueurs).

**Acceptation** (séquence « pieds visibles » du § C.1, et `l0_3p` après correction de `SteerWalk`) :

- glissement du pied d'appui ≤ 3 cm par pas en marche, course et accroupi ;
- hauteur des deux pieds au double appui ≤ 2 cm ;
- sur une marche de 20 cm, pied posé à ±2 cm de la surface tracée ;
- demi-tour sur place de 180° : aucun pied ne bouge de plus de 5 cm hors d'un pas ;
- `l0_3p` 010-014 : écart entre le bas du corps et la direction du déplacement ≤ 20° (aujourd'hui 43-72°) ;
- coéquipier vu par l'hôte et par un client : même cadence de pas sonores et visibles (±1 image).

### 3. Marcheurs à squelette : verrou horizontal, pas de rotation, cadence du Hound (priorité 3)

**Principe.**

1. **Verrou horizontal.** Dans `UpdateFootPlanting`, à la pose d'un pied (passage de cos(phase) à une valeur
   négative), on enregistre son point monde. Pendant l'appui, on résout l'IK à deux segments dans le plan de la jambe,
   en **horizontal et en vertical** (aujourd'hui seulement l'élévation, 3008-3021). La correction est bornée à ±15 cm
   et relâchée au lever du pied. Pour le Hound, on ajoute la composante horizontale au pli existant (3002-3006).
2. **Pas de rotation.** Sous 30 cm/s, avec un lacet supérieur à 30°/s (mesuré par image sur chaque machine),
   `AnimTime` avance à `Ω = |ω|·r_pied/(2·L·sin 10°)` avec une allure fictive de 10°. La créature piétine en tournant
   au lieu de patiner.
3. **Hound.** Quand l'`Omega` demandé dépasse 16 rad/s, on allonge la foulée au lieu de plafonner la cadence :
   A = asin(min(0,85 ; π·v/(2·L·16))), transmis à `AnimateLimbs`. Amplitude par patte pour les pattes arrière :
   sin A_arr = (67 / L_arr)·sin A.

**Risques.** Visuel local ; aucun rayon supplémentaire, car on réutilise ceux à 20 Hz. Le sens des pivots, que
`check_leg_ik.py` marque « à confirmer en jeu », doit être vérifié sur une capture avant de généraliser.

**Acceptation** (nouvelle séquence Hound et Skin-Stealer du § C.1, fiches d'appui) :

- glissement par appui ≤ 3 cm en marche et ≤ 6 cm en poursuite (aujourd'hui ≈ 23 cm calculés pour le Hound) ;
- demi-tour sur place : pas de pied d'appui déplacé de plus de 5 cm ;
- escalier du sas : pieds à ±2 cm des marches ;
- `ApplySkins` toujours à 0 s de période à moins de 25 m.

### 4. Continuité attente → marche → poursuite → attaque (priorité 4)

**Principe.**

- (a) En `Chase`/`Stalk`, avec la proie vue à moins de 8 m : `SetOrientToMovement(false)` et lacet vers la proie
  (`RInterpConstantTo`, `TurnRate`). Ajouter un « tirage de ficelle » dans `FollowPathTo` (1750-1758) : on vise le
  point de passage le plus lointain dont le chemin direct est libre. Hors salles de fosses seulement : la règle
  `bPitZone` est inchangée.
- (b) Amplitude de l'animation filtrée (allure filtrée à 8/s). La cadence garde la vitesse réelle, et les saccades de
  1 à 0,05 (1965-1974) ne font plus sauter les bras.
- (c) Départ de poursuite : 0,15 à 0,2 s de ramassement visible avant le premier bond (`AlertAnim`, 3284-3296). Pour la
  Bacteria, c'est la pose des deux griffes de l'amélioration 1.
- (d) Frappe bornée : angle maximal du bras calculé pour que la griffe s'arrête à la surface de la capsule visée plus
  10 cm (3384), sauf pendant le jumpscare.

**Risques.** (a) modifie le chemin réel des entités sur l'**hôte** ; la position est répliquée. Il faut relancer les
tests de fosses (v4.6, `PitOverFrames` = 0), de marche bloquée (T9) et de distances d'attaque. (b), (c) et (d) sont
visuels et locaux.

**Acceptation** (`l0_bacteria`) :

- pas plus de 45° de lacet par 1/30 s en poursuite ;
- amplitude des bras qui varie de 30 % au plus par image ;
- aucune image de frappe où le bras couvre plus de 15 % de l'écran à moins de 30 cm de la caméra (aujourd'hui 28 %
  en 040, 21 % en 072) ;
- fosses : 0 image au-dessus du vide.

### 5. Objet tenu, faisceau, bras contre les murs, coude (priorité 5)

**Principe.**

- **Prise dans la paume** : `RightHand` + 8 cm dans l'axe de la **main** (os `RightHandMiddle1` s'il existe, sinon
  l'avant-bras), et −3 cm vers la paume. Rotation tirée de l'os de la main, avec une table de prise par objet.
  Maillages tenus pour le caméscope et le scanner si leurs modèles existent ; sinon, documenter l'absence.
- **Faisceau en 3e personne** et chez les coéquipiers : il part de l'avant de la lampe tenue quand elle est en main
  (`LampSlot()` = 0).
- **Bras contre les murs** : balayage d'une sphère de 6 cm, de l'épaule à l'extrémité de l'objet tenu, à 20 Hz et
  seulement pour un corps visible. Au contact, le coude plie et le bras recule (0,1 s) ; près d'un mur latéral, le
  balancement est réduit.
- **Coude** : en tenue, IK vers un point devant la poitrine plutôt qu'un coude fixe à 62°, pour garder la main au même
  endroit avec un coude d'environ 45°.

**Risques.** Local : `NetHand` est déjà répliqué. Coût : 1 balayage à 20 Hz par corps visible. `SK_Hazmat` est
protégé : poses seulement, aucun poids modifié.

**Acceptation** (§ C.1, « objet tenu », « porte », « coude ») :

- centre de la lampe à ≤ 3 cm du centre de la paume (fiche monde), sans vide ni pénétration en recadrage 1:1 ;
- source du faisceau à ≤ 5 cm de la lentille ;
- marche à 20 cm d'un chambranle : distance du balayage ≥ 0 sur toutes les images, gant jamais coupé par l'arête
  (aujourd'hui `l0_3p` 027-028) ;
- coude en tenue : largeur de la silhouette au coude ≥ 70 % de celle de l'avant-bras.

### 6. Lissage réseau du modèle des entités (priorité 6, constat de code)

**Principe.** Le Visual est attaché à la **capsule** (208-209). Or le moteur ne lisse les corrections de position des
pions simulés que sur le **maillage du Character** (`CharacterMovementComponent.cpp:8548-8566`), comme le fait déjà le
corps du joueur (`BRCharacter.cpp:3670-3671`). Il faut attacher le Visual à `GetMesh()` et prendre les références de
sol dans le repère du parent du Visual : rayons des pieds (2956-2957), mesure des pieds (3039-3041), appui rigide
(3068-3069), pied le plus bas (3117).

**Risques.** Client seulement : sur l'hôte et en solo, le décalage de lissage est nul. Aucun coût. À contrôler :
jumpscare (`SetScareTransform`, transformation monde : sans effet), base du Visual (−HalfHeight) inchangée si le
maillage reste à (0, 0, 0).

**Acceptation.** Test réseau à 2 instances (infrastructure `-BRAutoTestNet`) avec une Bacteria en marche, vue par le
client à vrai pas fixe : pas de saut de position du Visual au-delà de 3 cm par 1/30 s par rapport au déplacement
attendu de la vitesse (|Δx − v·dt| ≤ 3 cm), à comparer avec la version actuelle.

---

## C. Captures supplémentaires

### C.0 Préalables du banc (sinon aucune acceptation n'est mesurable)

1. **Vrai pas fixe** pour le plan visuel : lancement séparé avec `-UseFixedTimeStep -FPS=30`, sans le mélanger aux
   mesures de performances. Contrôle : `temps_depuis_arrivee_s` + 0,033 par image et 8,7 cm par image à la marche.
2. **Fiches d'appui par image** (`BRAutoTestV417Visual.cpp`) pour l'entité de contrôle, le Hound, le Skin-Stealer et
   le joueur :
   - positions monde des points d'appui (`FootOffset` transformé, `BodyContacts`, `FLimb::Contacts`, chevilles de
     `SK_Hazmat`) et leur hauteur au-dessus du sol tracé ;
   - vitesse, lacet, `AnimTime` / `BodyAnim`, état, phase d'attaque.

   Glissement = déplacement d'un appui dont la hauteur est < 2 cm. Cette mesure évite les estimations au pixel de ce
   rapport.
3. **`SteerWalk`** : quand les deux côtés sont bouchés, garder un nouveau cap au moins 1 s, ou suivre la grille. Sans
   cela, `l0_3p` 028-089 et `p_3p_eau` 056-089 restent des images sans marche.
4. **Accès de test** pour conduire une entité en ligne droite et la faire tourner sur place sans son IA (vitesse et
   cap imposés), car `MoveTowards` est privé.

### C.1 Scènes à ajouter

| Sujet | Scène et caméra | Ce qui est jugé |
|---|---|---|
| Pieds de la combinaison | Level 0, salle de départ (graine 1896875771). Caméra indépendante (`ShowCamera`) de côté à 3,5 m, à 60 cm du sol, tangage −5°. 3 s de marche, 2 s de course, 2 s accroupi, arrêt, demi-tour de 180° sur place (6° par image, sans avancer), 2 s de pas de côté, 2 s à reculons | Glissement, double appui, pivot (amélioration 2) |
| **Escalier** | Level 0, **sas derrière la porte de service** : palier de 140 cm, 8 marches de 20 × 30 cm, largeur 240 cm (`BRChunk.cpp:1046-1080`). Caméra fixe sur le palier en plongée, puis une en bas. Joueur en 3e personne qui descend puis remonte ; Bacteria, Hound et Skin-Stealer posés **pour le test** sur le palier et conduits (C.0-4) | Pied sur la marche, bassin, sens des pivots de l'IK (`check_leg_ik.py`) |
| Trottoir et marches immergées | Poolrooms, graine 4242, pose `p_trottoir` existante : joueur qui monte du canal sur le trottoir (pieds visibles hors de l'eau) | Appui sur un sol plus haut (amélioration 2) |
| **Porte** | Porte de service (130 cm) ouverte, et porte ordinaire du Level 0 (150 cm, `BRLevels.cpp:50`). Joueur qui passe à 20 cm du chambranle (caméra fixe au chambranle), lampe en main puis mains libres. Bacteria qui passe une porte du Level 0 au centre puis décentrée | Main ou griffe dans le chambranle (A.1, A.2 027-028 ; amélioration 5) |
| **Objet tenu** | Level 0, sous lumière neutre puis néon. Gros plan de côté à 80 cm de la main droite, lampe en main éteinte puis allumée, à l'arrêt puis en marche. Même scène pour un coéquipier vu par l'hôte et par un client (2 instances) | Prise dans la paume, source du faisceau, coéquipier |
| **Coude** | Même gros plan : tenue avec visée −60°, 0° et +60° ; marche accroupie ; échelle (Niveau 2 ou 8) ; brasse (Poolrooms, bassin profond) | Pincement et volume du coude (amélioration 5) |
| Hound, Skin-Stealer en mouvement | Level 4 (comme v416vis) ou Level 0 avec apparition de test. Vue de côté à 3 m et à 5 m, pendant 3 s chacun : marche, poursuite vers un joueur figé, attaque ; Skin-Stealer : masse → corps → déguisement → déchirure | Glissement en poursuite (Hound ≈ 23 cm calculés), continuité des états |
| Bacteria vue par un client | 2 instances, même vidéo `l0_bacteria`, capture du client | Amélioration 6 |
| Reflets (§ 4.5) | Hors de ce lot : la même entité hors champ, dans un reflet RT et en traces d'écran, selon le profil (à confier à l'agent éclairage) | — |

---

## D. Limites de cette analyse

- Les vidéos « 1/30 s » ne sont pas à pas fixe (§ 0) : les glissements en pixels sont convertis avec une géométrie de
  caméra approchée (sol plat à z = 0, œil à 164,2 cm), à ±10 % environ. Les chiffres du joueur, du Hound et du
  Skin-Stealer sont **calculés depuis le code**, pas observés.
- Les pieds du joueur n'apparaissent dans aucune image en mouvement. Le Hound et le Skin-Stealer n'ont aucune image en
  mouvement, et le Skin-Stealer aucune hors de sa masse.
- Les causes marquées [Hypothèse] (pas fixe écrasé, saut du pied pendant les attaques, griffes dans les portes, main
  dans le chambranle) demandent les captures du § C.
- Aucun test exécuté : ni Unreal, ni Blender, ni bancs C++.

## Résumé

1. Les vidéos « 1/30 s » ne sont pas à pas fixe : 0,19 à 0,29 s réelles par image, animation bornée à 0,1 s ; la
   cadence vue y est 2 à 3 fois trop basse. Préalable : `-UseFixedTimeStep -FPS=30`, fiches d'appui, `SteerWalk`.
2. Bacteria : appui statique correct (v4.16 tient). En marche, le corps rigide glisse : ≈ 100 cm entre les images
   020 et 021, pied flottant de 0 à 3 cm.
3. Bacteria : ses griffes frottent sans appui, elle pivote d'environ 90° en une image (023→024), son pied glisse de
   8 à 15 cm entre les frappes, et ses bras balaient la caméra (28 % de l'écran en 040).
4. Combinaison : pieds hors cadre dans les vidéos 3e personne, donc rien n'y est mesurable. Démarche en crabe de 43 à
   72° (010-014) ; gant probablement dans le chambranle (027-028).
5. Combinaison, d'après le code : pas fixe de 141 cm, soit ≈ 47 cm de glissement par pas en marche ; pieds 12,6 cm
   en l'air au double appui ; pivot sans pas ; trois cadences de pas sonores différentes.
6. Hound, d'après le code : cadence plafonnée en poursuite (≈ 23 cm de glissement par appui), pattes arrière plus
   courtes, demi-tour sans pas. Skin-Stealer : jamais vu hors de sa masse.
7. Six améliorations, par priorité : béquilles et appui verrouillé de la Bacteria ; cadence, bassin et pivot du
   joueur ; verrou horizontal et pas de rotation des marcheurs.
8. Puis : continuité des états (orientation, saccades, frappe bornée), prise de l'objet, faisceau et coude, et lissage
   réseau du Visual des entités.
9. Toutes sont des poses calculées, sans modifier les ressources protégées. Seule l'orientation et le chemin de la
   poursuite touchent l'IA sur l'hôte.
10. Captures à ajouter : escalier du sas de service (8 marches de 20 cm), portes de 130 et 150 cm, gros plans de main
    et de coude, Hound et Skin-Stealer en mouvement, vue d'un client.

---

## Réalisation (v417-anim)

Branche `v417-anim`, partie de `v417-jeu` (`7c50c7a`). Toujours dans le rôle Technical Artist. Fichiers modifiés :
`Source/Backrooms/Private/BREntity.cpp`, `Public/BREntity.h`, `Public/BRRig.h`, et dans `Private/BRCharacter.cpp`
seulement `AnimateBody` et `PoseSkin`, plus l'état local de la pose et un crochet de lecture placés juste avant
`AnimateBody`.

Je n'ai touché ni au banc (`BRAutoTestV417Visual.cpp`), ni aux fichiers partagés, ni aux ressources protégées. Rien de
ce qui est répliqué n'a changé, ni l'IA décidée par l'hôte, ni les capsules, collisions, vitesses, attaques et dégâts :
seules les poses et l'orientation visuelle des modèles changent.

**Rien n'a été compilé ni lancé** : la compilation (MSVC strict) et les essais reviennent à l'orchestrateur. J'ai relu le
code ligne à ligne, avec une attention particulière aux points suivants :
- avertissements C4458/C4459 : aucun nom local ne reprend un membre ou une déclaration globale (vérifié par recherche) ;
- conversions double → float : toujours explicites ;
- API déjà employées dans ces fichiers ;
- `UPoseableMeshComponent::SetBoneTransformByName` : relu dans les sources 5.8 (pose de l'espace du composant reconstruite
  à chaque appel).

| Commit | Contenu |
|---|---|
| `f14dbf7` | Priorité 1 : Bacteria posée au sol |
| `14b2a4c` | Priorité 2 : corps du joueur |
| `f662494` | Priorité 3 : marcheurs à squelette |
| `1f3bc1e` | Crochet de test du corps du joueur (`BRRig::GetBodyPoseInfo`) |

### R.1 Bacteria (priorité 1, `f14dbf7`)

**Où.**
- `BREntity.cpp` : `AnimateBacteriaGait` (l. 3218-3463), `AimRigidArm` (3200), `LowestContact` (3185).
- Branche dans `Animate`, juste après `UpdateFootPlanting` (≈ 3661-3668), suivie de `KeepRigidPartsAboveGround`.
- Bruit de pas dans `UpdateFootsteps` (1237-1290).
- État dans `BREntity.h`, en fin de classe (`FCrutchArm`, `SupportPos`, `HopT`…).

**Principe.** Le modèle n'est plus collé à la capsule. On mémorise son origine au sol et son lacet, en coordonnées monde.
- **Allure à appuis** (vitesse lissée sous 200 cm/s : marche, rondes, « elle m'a vu ? », approche lente, arrêts) :
  - le modèle reste posé, pied et griffes immobiles ;
  - un **bond** de 0,16 à 0,26 s et de 2 à 5 cm de haut rattrape la capsule (un peu en avant d'elle si elle avance)
    dès qu'elle est à plus de 40 cm, qu'elle a tourné de plus de 22°, ou qu'elle a changé de hauteur de plus de 8 cm ;
  - pendant une attaque, les seuils passent à 60 cm et 35°, et il n'y a aucun recalage : le pied ne saute plus quand la
    capsule pousse contre la proie (028-089 du banc) ;
  - à l'arrêt, un petit bond recale le modèle quand l'écart dépasse 12 cm ou 6° pendant 0,35 s ;
  - à chaque réception : un hochement de 2° et un bruit de pas, au lieu des pas au rythme des bras.
- **Course** (vitesse lissée au-delà de 200 à 320 cm/s ; montée rapide, descente lente, donc les arrêts secs de la
  poursuite ne la font pas retomber) :
  - portée par ses bras, pieds à 5-8 cm du sol, elle suit la capsule ;
  - son lacet affiché tourne à 420°/s au plus, jamais plus de 45° de retard : plus de demi-tour d'un bloc comme entre
    023 et 024 ;
  - en poursuite à moins de 8 m, le corps fait face à sa proie (70° au plus de la capsule).
- **Frappe** : la fente de 16 cm en translation devient un tangage de 5° autour du pied (le pied reste posé).
- **Griffes** :
  - posées au monde à l'allure à appuis ;
  - quand le corps les a dépassées de plus de 16 cm (après un bond), elles se lèvent (arc de 25 cm, 0,16 s) et se
    reposent devant, un bras à la fois, sauf si l'une est restée à plus de 45 cm ; même principe que les mains d'appui
    du Clump ;
  - le bras rigide est tourné par la plus petite rotation qui amène sa griffe sur le point posé, et l'écart de longueur
    est rattrapé par `KeepRigidPartsAboveGround` ;
  - pas d'appui au-dessus d'une fosse (`IsOverPit`) ;
  - le geste d'`AnimateLimbs` reprend la main en course, bras tendus, armée et frappe.
- **Portes et murs** : toutes les 0,1 s, un rayon latéral à 45 cm du sol, de chaque côté. Si un mur ou un chambranle est
  plus près que l'envergure de la griffe + 8 cm, le bras se replie vers le corps (roulis de 22° au plus) et la griffe se
  repose à l'intérieur.

**Risques.**
- Le modèle affiché peut être jusqu'à 40 cm (60 pendant un coup) derrière la capsule. Les coups, la vue et les tests de
  fosses utilisent toujours la capsule : rien ne change pour le jeu, mais la créature peut sembler frapper d'un peu plus
  loin.
- Près d'une fosse, la corde d'un bond (≤ 60 cm) peut couper un angle de passage d'environ 15 cm.
- Chez un client, la pose est calculée depuis la vitesse et la rotation répliquées ; le verrou masque aussi les sauts de
  correction de la capsule. Il peut y avoir quelques centimètres d'écart entre machines (pose locale).
- **Coût estimé** : environ 3 µs par Bacteria et par image (transformations, deux quaternions, deux recherches
  `IsOverPit`), plus 2 rayons toutes les 0,1 s. Aucun rayon par image.

**Critères d'acceptation** (vidéo `l0_bacteria` à vrai pas fixe de 1/30 s, avec les crochets du § R.4 relevés à chaque
image) :
1. Pied posé, `IsHopping()` faux et `GetSupportRun()` < 0,5 : déplacement monde des appuis du corps ≤ 0,5 cm par image
   et ≤ 2 cm par appui. Aujourd'hui, environ 100 cm entre deux images du banc (020 → 021).
2. Course (`GetSupportRun()` ≥ 0,5) : appuis du corps à au moins 3 cm de `GetSupportGroundZ()` sur 90 % des images en
   mouvement.
3. Griffe à moins de 2 cm du sol : ≤ 1 cm de déplacement par image.
4. `GetVisualYaw()` : ≤ 15° de variation par image en course ; ≤ 25° par bond à l'allure à appuis. Aujourd'hui
   environ 90° entre 023 et 024.
5. `GetVisualLag()` ≤ 40 cm (≤ 60 pendant un coup), et contrôle v4.16 « bas des volumes ≥ −1 cm » conservé.
6. À l'œil (vidéo assemblée par `Tools/Visual/make_v417_media.py`) : bonds et appuis lisibles, plus de glissement,
   griffes repliées dans une porte du Level 0 (capture du § C.1).

### R.2 Corps du joueur (priorité 2, `14b2a4c`, crochet `1f3bc1e`)

**Où.**
- `BRCharacter.cpp` : `AnimateBody` (l. 3873-4166), `PoseSkin` (4172-4235).
- État local `FBRBodyPoseState`, clé : le personnage (3800-3849), et `BRRig::GetBodyPoseInfo` (3851).
- `BRRig.h` : `FBRBodyPoseInfo`.

**Principe.**
- **Cadence tirée de la jambe**, comme pour les entités : `Ω = π v / (2 L sin A)`.
  - L : hanche à 93,9 cm, ou 52 cm accroupi.
  - A : amplitude de 30° × allure, au moins 8° en marche (16° accroupi).
  - Le pas de marche passe de 141 à environ 94 cm (environ 2,8 pas/s à 260 cm/s) ; course inchangée (environ 1 % de
    glissement avant comme après).
- **Bassin** :
  - il descend de L (1 − cos θ) sur la jambe d'appui : plus de pieds à 12,6 cm du sol au double appui ;
  - il descend aussi vers le sol le plus bas sous les pieds : 2 rayons à 20 Hz, sol statique ;
  - un pied posé plus haut plie le genou, avec la même IK à deux segments que les entités (vérifiée par
    `Tools/check_leg_ik.py`) : marches, trottoirs, bords de bassin.
- **Bas du corps en retard** :
  - en marche, les jambes vont vers le déplacement (35° au plus de la visée ; à reculons, elles gardent la visée) ;
  - à l'arrêt, elles restent en place jusqu'à 40° d'écart, puis de petits pas (12°) les ramènent à 220°/s ;
  - torsion bornée à 45°, répartie par tiers sur `Spine`, `Spine1` et `Spine2` (`PoseSkin`) ;
  - pour les pièces rigides de repli, le buste reçoit le lacet inverse.
- **Placement avant la pose** : le corps est placé avant d'être posé, donc la main et l'objet tenu n'ont plus une image
  de retard.

**Risques.**
- Torsion de la taille jusqu'à 45° (35° en marche latérale) : la déformation du maillage `SK_Hazmat` (protégé, poids
  inchangés) est à juger à l'image.
- Les pieds sont 2 cm plus bas : le sol est mesuré, et la capsule flotte d'environ 2 cm au-dessus.
- La 1re personne n'est pas touchée (`AnimateBody` n'y est pas appelé).
- Les coéquipiers sont animés depuis leur vitesse et leur rotation répliquées.
- L'état est gardé dans une table propre à `BRCharacter.cpp` pour ne pas toucher à `BRCharacter.h`. **À l'intégration :
  en faire un membre d'`ABRCharacter`.**
- **Coût estimé** : 2 rayons à 20 Hz et environ 5 à 10 µs par image pour chaque corps visible (4 au plus), dont deux
  appels de plus à `SetBoneTransformByName` (chacun reconstruit la pose de l'espace du composant, comme les dix
  existants).

**Critères d'acceptation** : séquence « pieds visibles » du § C.1 et `l0_3p` après correction de `SteerWalk`. Les os se
lisent sur le `UPoseableMeshComponent` du personnage (`LeftLeg`, `RightLeg`, et `LeftFoot`/`RightFoot` s'ils existent) ;
la pose sur `BRRig::GetBodyPoseInfo`.
1. Glissement du pied d'appui ≤ 3 cm par pas en marche, course et accroupi.
2. Au double appui, les deux pieds à ≤ 2 cm du sol.
3. Pendant un glissement le long d'un mur (comme `l0_3p` 010-014), écart entre la direction du déplacement et `LowerYaw`
   ≤ 37°. Aujourd'hui, le corps entier est à 43-72° du déplacement.
4. Demi-tour sur place : `YawOffset` atteint au plus 40-45° avant que `TurnStep` > 0 ne fasse des pas, et aucun pied ne
   bouge de plus de 5 cm hors d'un pas.
5. Escalier du sas : pied à ±2 cm de la marche ; `FootGround` suit les 8 marches de 20 cm.
6. À l'œil : taille sans pincement à 35-45° de torsion.

### R.3 Marcheurs à squelette (priorité 3, `f662494`)

**Où.** Dans `BREntity.cpp` :
- `UpdateFootPlanting(Dt, InGait)` (l. 2954-3078 ; signature changée, appel unique dans `Animate`) ;
- `MeasureFeet` (3080-3125) ;
- cadence et piétinement dans `Animate` (3550-3593) ;
- `AnimateLimbs`, cas `Thigh` et `HoundUpper` (3835-3848).

Champs ajoutés à `FLimb` et membres `StrideBoost`, `TurnShuffle`, `FootRadius` dans `BREntity.h`.

**Principe.**
- **Pied tenu au monde à l'horizontale.**
  - Au début de l'appui (cos(phase) ≤ 0), le pied pose son point au monde.
  - Pendant l'appui, la cuisse ou le haut de patte tourne de atan(écart / hauteur de hanche) pour l'y garder. La
    correction est sans retard (l'écart mesuré à une image est corrigé à la suivante) et bornée à 15 cm. Au-delà, le
    point d'appui suit (virage serré, arrêt net, correction du serveur).
  - Au lever du pied, la correction s'efface.
- **Piétinement en tournant** : sous 40 cm/s et au-delà de 25°/s de lacet, la phase avance au rythme de l'arc parcouru
  par les pieds, avec de petits pas de 10°. Avant, les membres restaient immobiles pendant que le corps tournait.
- **Foulée du Hound** : quand la cadence demandée dépasse 16 rad/s, la foulée s'allonge (jusqu'à 50°) au lieu de
  plafonner la cadence.
- **Foulée par jambe** : hauteur de chaque hanche mesurée sur le pivot, facteur = `LegLength` / hauteur.
  - Pattes arrière du Hound : environ 1,13.
  - Skin-Stealer : sa vraie forme reçoit 0,93, car `LegLength` vient du déguisement construit en dernier ; cela corrige
    un écart existant.

**Risques.**
- Le sens des pivots (tangage positif = pied vers l'avant, comme le balancement existant) est déduit du code et de la
  matrice de `FRotator`, pas encore vu en jeu (`check_leg_ik.py` le signalait déjà). À vérifier d'abord sur une capture.
- Le verrou est seulement dans l'axe de marche : un reste de glissement latéral subsiste dans les virages, que le
  piétinement atténue.
- Le Hound en poursuite a une foulée plus longue, donc une silhouette plus étirée.
- Le rayon des pieds du Skin-Stealer est mesuré sous l'échelle de sa masse (environ moitié) : il piétine plus lentement.
- **Coût estimé** : moins de 2 µs par marcheur et par image (une transformation et quelques fonctions
  trigonométriques par jambe), aucun rayon de plus.

**Critères d'acceptation** (séquence Hound et Skin-Stealer du § C.1, `GetSupportPoints` à chaque image) :
1. Glissement par appui ≤ 3 cm en marche, ≤ 6 cm pour le Hound en poursuite. Aujourd'hui environ 23 cm, d'après le
   calcul.
2. Demi-tour sur place : aucun pied d'appui déplacé de plus de 5 cm.
3. Escalier du sas : pieds à ±2 cm des marches.
4. `GetSkinPeriod()` = 0 à moins de 25 m.

### R.4 Crochets de test (lecture seule)

- `ABREntity::GetSupportPoints(TArray<FVector>&)` : appuis visibles en coordonnées monde, dans cet ordre :
  1. pieds des jambes articulées, dans l'ordre des membres ;
  2. appuis du corps rigide (`BodyContacts`, Bacteria : pied gauche) ;
  3. griffe la plus basse de chaque bras-béquille.
- `ABREntity::GetSupportGroundZ()` : sol de référence (bas de la capsule).
- `ABREntity::GetVisualLag()` : écart horizontal modèle/capsule, en cm.
- `ABREntity::GetVisualYaw()` : lacet du modèle affiché.
- `ABREntity::GetSupportRun()` et `IsHopping()` : allure et bond de la Bacteria.
- `BRRig::GetBodyPoseInfo(const AActor*, FBRBodyPoseInfo&)` :
  - lacet des jambes (`LowerYaw`) et écart à la visée (`YawOffset`) ;
  - pas en tournant (`TurnStep`) ;
  - descentes du bassin (`PelvisDrop`, `WalkDrop`) ;
  - sol sous chaque pied (`FootGround`) et pied estimé (`FootWorld`) ;
  - renvoie faux tant que le corps n'a pas été animé (1re personne).

Mesure de glissement proposée : déplacement monde d'un appui entre deux images consécutives tant qu'il est à moins de
2 cm du sol. Elle remplace les estimations au pixel de la partie A.

### R.5 Non fait, et pourquoi

- **Priorité 4** : seule l'orientation visuelle locale de la Bacteria est faite. Restent hors de ce lot, comme demandé :
  - le lissage du chemin et l'orientation de la capsule (IA de l'hôte) ;
  - l'anticipation au départ de la poursuite ;
  - la frappe bornée (le bras de la Bacteria balaie toujours la caméra : `AnimateLimbs`, `Strike * 105°`).
- **Priorité 5** :
  - seul le retard d'une image de l'objet tenu est corrigé ;
  - prise dans la paume et coude non faits, faute de capture. Les axes des os de la main et la pose de repos des doigts
    de `SK_Hazmat` sont inconnus ; le point de prise actuel (`RightHand` + 6 cm dans l'axe de l'avant-bras, pivot de
    `SM_Flashlight` à la poignée) tombe déjà à environ 1 cm du milieu de la paume. Le vrai défaut probable est la main
    ouverte : il faudrait fermer les doigts, os à vérifier sur un gros plan (§ C.1) ;
  - bras qui s'écartent des murs : non fait.
- **Priorité 6** (décrite seulement, car elle change l'attachement réseau) : attacher `Visual` à `GetMesh()` au lieu de la
  capsule (`BREntity.cpp:208-209`) et prendre les sols de référence dans le repère du parent du Visual
  (`UpdateFootPlanting`, `MeasureFeet`, `KeepRigidPartsAboveGround`, `LowestFootHeight`). Le verrou de la Bacteria
  masque déjà en partie ces sauts chez un client.
- **Pas sonores du joueur** : je n'ai pas touché `UpdateCamera` (`BobTime`, 2,55 pas/s à la marche) ni `TickRemote`
  (minuterie de 0,46 s), qui sortent des fonctions du corps. Le corps fait maintenant environ 2,8 pas/s à la marche. À
  aligner : dans `UpdateCamera`, prendre la même `Ω` que `AnimateBody` ; dans `TickRemote`, jouer le pas aux
  demi-cycles de `BodyAnim`.
- **À voir d'abord à l'image**, dans cet ordre :
  1. sens de l'IK horizontale des marcheurs (§ R.3) ;
  2. torsion de la taille du joueur ;
  3. repliement des bras de la Bacteria dans une porte (sens du roulis déduit de la matrice de `FRotator` et du
     commentaire de `BuildBacteriaModel`) ;
  4. hauteur des bonds.

### Résumé de la réalisation

1. Branche `v417-anim`, 4 commits sur `7c50c7a` (plus celui de ce document). Rien compilé ni lancé : compilation et
   essais par l'orchestrateur.
2. Bacteria : posée au monde (pied et griffes immobiles), elle avance par bonds de 0,16 à 0,26 s. En course, portée par
   ses bras, pieds au-dessus du sol.
3. Bacteria : lacet affiché borné (420°/s, au plus 45° de retard), face à la proie à moins de 8 m. Frappe en tangage
   autour du pied, griffes posées comme celles du Clump, bras repliés près des murs.
4. Joueur : cadence tirée de la jambe (pas de 141 à environ 94 cm), bassin sur la jambe d'appui et vers le sol le plus
   bas, IK du genou sur les marches.
5. Joueur : bas du corps en retard (35° en marche, petits pas au-delà de 40° à l'arrêt), torsion répartie sur trois
   vertèbres.
6. Marcheurs : pied tenu au monde pendant l'appui (15 cm au plus), piétinement en tournant, foulée du Hound au lieu de
   la cadence plafonnée, foulée par jambe.
7. Crochets de lecture : `GetSupportPoints`, `GetVisualLag`, `GetVisualYaw`, `GetSupportRun`, `IsHopping`,
   `BRRig::GetBodyPoseInfo`.
8. Coût estimé : environ 3 µs par Bacteria, 5 à 10 µs par corps de joueur visible, moins de 2 µs par marcheur ; rayons
   bornés (20 Hz par corps, 10 Hz par bras).
9. Rien de répliqué, ni IA, ni capsules, ni vitesses, ni coups modifiés ; ressources protégées intactes.
10. Reste : frappe bornée et chemin (priorité 4, IA), main fermée et coude (priorité 5, captures), lissage réseau
    (priorité 6, décrit), pas sonores du joueur.
