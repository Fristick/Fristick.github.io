# v4.17 : inventaire, réglages, carnet : analyse de l'interface (agent « UI Designer »)

| | |
|---|---|
| **Rôle** | « UI Designer », définition installée `C:\Users\Fristick\.claude\agents\design-ui-designer.md`, lue en entier et appliquée à une interface de jeu dessinée par le Canvas d'Unreal (`ABRHUD`). Lancé comme sous-agent générique par l'orchestrateur. |
| **Mission** | Analyse seule (prompt v4.17, § 3 « Interface » et § 4.6). Aucun fichier du dépôt modifié : ce rapport est le seul fichier écrit. |
| **Base lue** | Branche `backrooms-v4.17`, tête `c07eddb`. **Les numéros de ligne renvoient à `c07eddb`.** Pendant l'analyse, l'orchestrateur a publié `1e67bb1` (banc visuel v4.17), qui ajoute 5 lignes nettes à `BRHUD.cpp` après la ligne 1322 (`TestCaptureMode`) : **à partir de `1e67bb1`, ajoutez 5 aux numéros de `BRHUD.cpp` supérieurs à 1322**. Les autres fichiers cités n'ont pas changé. |
| **Images vues** (outil de lecture d'images, puis recadrages ×2 à ×5) | `Saved/TestRuns/autotest/final416/v416vis/` : `v416_fenetre_{169_max,ultralarge_max,petite_1280x720}_{personnage,carnet}.png` ; `v416_hud_1280x720_{fr,de,ar,ja,ru}_{personnage,carnet,parametres,touches,journal}` (selon les fichiers présents) ; `v416_hud_1920x1080_{fr,ar}_{personnage,parametres}` ; `v416_hud_2560x1080_de_personnage` ; `v416_hud_1920x1080_fr_jeu` ; `v416_manette_1920x1080_fr_{personnage,carnet}` ; `v416_manette_{inspecter,equipe,carnet_aide,pause_fenetre}` ; référence `scratchpad/ref/ref_09.jpg` (inventaire de Backrooms: Escape Together, analyse seulement). **Absentes** : aucune capture en chinois, coréen ni persan dans ce dossier. |
| **Mesures** | Hauteur des glyphes : projection des lignes de pixels clairs d'une zone, à partir de la PNG d'origine. Contraste : rapport WCAG calculé sur des pixels médians. Scripts jetables dans le dossier temporaire de la session, hors dépôt. Rapport du banc : `v416vis/Rapport.txt`. |

Conventions : **[image]** = observé sur l'image citée ; **[code]** = constat dans le code (fichier:ligne) ; **[hyp.]** = hypothèse à vérifier. Les tailles « pt » sont celles que demande le code (`TextF`, points Slate, 96 ppp : cadratin en px = pt × 4/3 ; hauteur des capitales de Roboto ≈ 0,71 cadratin). « U » = `ABRHUD::Ui()` = hauteur / 1080 (0,667 à 720 lignes, 0,711 à 768, 1 à 1080).

---

## 1. Synthèse

1. **La taille du texte est le défaut principal, avant le contraste.** À 1280×720, presque tout le corps de l'inventaire est rendu au plancher de 6 pt (cadratin de 8 px, capitales de 5 à 6 px). À cette taille, Roboto déforme les lettres : « TOUCIIES », « CIIOISIR », « TIIRESHOLD », « ÉCIIAP », « DANDAGES » pour BANDAGES. Le banc v4.16 le mesure sans le juger : **5 163 textes sur 18 345 au plancher de 6 points** dans les rendus hors écran, **170 textes sur 237** dans les vraies fenêtres. Même à 1080p, le corps ne fait que 7 à 8,6 pt et les noms d'objets 6 pt. Les couleurs sont bonnes : Ink 10,9:1, InkDim 8,0:1, jaune 10,8:1, cadres 3,5:1.
2. **La vue du personnage n'est pas une capture de scène.** C'est une image 2D pré-rendue dans Blender, convertie en gris, **multipliée par 0,28** puis teintée à 0,85. Elle est donc plus sombre que le panneau (contraste 1,1 à 1,2:1) et n'a aucun modelé (luminance p5 à p95 : 43 à 45 sur 255). Aucun éclairage de capture, aucune exposition ni aucun matériau n'entre en jeu.
3. **Manette** : on ne peut **régler aucune valeur de PARAMÈTRES** (pas de curseur de ligne ; `AdjustSetting` n'est appelé que par les clics). On ne peut **déplacer aucun objet** (Y ne fait rien dans PERSONNAGE). Dans le carnet, la croix fait défiler **les deux colonnes en même temps**, un pas par appui, sans répétition.
4. **Carnet** : le cas du banc (Level 1, deux indices) occupe environ 15 % de la colonne : rien ne défile, d'où le « NON VÉRIFIÉ » du banc. Les captures `v416_fenetre_*_carnet` montrent **le jeu sans inventaire** : la fermeture est demandée dans la même étape que la capture, qui est asynchrone.
5. **Superpositions** : la fenêtre INSPECTER est translucide (fond α 0,72) et laisse lire les cases du dessous ; l'infobulle (α 0,95) laisse voir leurs cadres. Le mélange se fait en espace linéaire, si bien que même 5 % restent visibles.
6. **Arabe** : la mise en page n'est pas inversée : puces à environ 400 px du texte, titres à gauche, compteurs collés en tête de phrase, flèches « < » et « > » inversées par l'algorithme bidirectionnel. **Casse** : `FString::ToUpper()` ne touche que l'ASCII, d'où « RéTABLIR » et « SCHéMA » en français, et des objectifs russes restés en minuscules.
7. **Lignes de 0,67 px** à 720 lignes : bords de cases et séparateurs de réglages disparaissent par endroits.
8. Ce qui est **acquis** : seulement deux jauges, et seulement dans PERSONNAGE (banc : 66 rendus avec les deux jauges, 0 ailleurs, aucune barre de vie) ; mot d'état sans couleur (« reposé », « stable · éclairé ») ; libellés d'état bornés (`TextFit`) ; coupure des lignes ICU ; mise en forme arabe ; ultra-large centré à 1 760U. Les 12 corrections ci-dessous gardent ces acquis.

---

## 2. Observé sur les images

### 2.1 Tailles mesurées

| Élément | 1920×1080 (U = 1) | 1280×720 (U = 0,667) | Code (taille demandée) |
|---|---|---|---|
| « MENU > » | capitales 12 px | — | `Txt(..., 1.25U, Large)` ≈ 12,5 pt (BRHUD.cpp:4152) |
| Onglets (PERSONNAGE…) | capitales **9 px** | **5 px** | `LegacySize(Medium, 0.95U)` ≈ 9,5 pt → 6 pt à 720p (4180-4182) |
| Titre de panneau | 12 px | 8 px | 11,5 pt gras, espacement 2,5U (775) |
| Ligne d'objectif | capitales **8 px** | **6 px** | Small × 0,78 ≈ 7,8 pt → plancher 6 pt (4598, 4608) |
| Phrase sous les objectifs | ≈ 7 px (avec hampes 9) | ≈ 5 px | Small × 0,72 (4626-4630) |
| **Nom dans une case** | capitales **5 px** | **5 px** | Small × 0,62 ≈ 6,2 pt, réduit en largeur, **bloqué au plancher** (4541-4547) |
| Numéro de poche, « VIDE » | 7 px | ≈ 5 px | 0,7 (4524, 4531) |
| ENDURANCE / SANTÉ MENTALE | 12 px | 8 px | 12,5 pt gras (4810) |
| Raccourcis du pied | crochets 12 px, capitales ≈ 8 | capitales ≈ 5 | Small × 0,8 (4275) |
| Japonais, objectifs | — | glyphes **8 px** (kanji pâteux) | plancher 6 pt |
| Arabe, objectifs | hauteur totale 11 px | — | ≈ 8 pt |

[code, déduction] Les polices Small, Medium et Large du moteur ont toutes un `LegacyFontSize` d'environ 10 : Small × 0,78 donne des capitales de 8 px, Large × 1,25 des capitales de 12 px. Les « facteurs » anciens valent donc directement des points de référence (0,62 ≈ 6,2 pt).

### 2.2 PERSONNAGE (inventaire)

- **Hiérarchie inversée** [image `v416_hud_1920x1080_fr_personnage`]. Les jauges d'ÉTAT (libellé 12,5 pt gras, pourcentage 13 pt) dominent. Les objectifs (8 pt) et les onglets (9,5 pt), qui sont la navigation principale, sont plus petits que les titres de panneau. Dans la référence `ref_09`, les onglets font environ 30 px de capitales et les objectifs environ 18 px.
- **Cases** : une bande claire couvre les 45 % du haut de **chaque** case, vide ou pleine. La grille de 20 cases vides se lit comme « à deux tons » et fait du bruit visuel. La référence montre une grille unie à traits fins.
- **Nom d'objet** : 5 px de capitales à toutes les définitions. En russe à 720p, « МИНДАЛЬНАЯ ВОДА » touche les deux bords de la case [image `v416_hud_1280x720_ru_personnage`, recadrage ×4].
- **Icônes sombres** : le caméscope, la lampe et la frontale ressortent mal sur les cases presque noires (luminance médiane des icônes 0,052 à 0,068 ; contraste mesuré en jeu de 1,5 à 2:1, `v416_manette_equipe`, `v416_fenetre_169_max_personnage`). La frontale ne couvre que 8 % de son icône.
- **Curseur de la manette** : cadre jaune de 1,4 px à 1366×768 [image `v416_manette_equipe`]. Il est visible mais discret et ressemble au survol de la souris.
- **Équipement** : la légende « CEINTURE » **chevauche de 4 px le cadre bas de la case TORSE** à 1080p [recadrage ×5] ; même chose pour « GÜRTEL » en allemand [image 2560×1080].
- **Fonds** : les rendus `v416_hud_*` sont dessinés sur un fond uni (0,05 ; 0,045 ; 0,03), qui n'est pas un « fond crédible ». Les vraies fenêtres (`fenetre_*`, `manette_*`) montrent le Level 1 assombri derrière le voile (0,84) : la lecture y reste bonne.

### 2.3 Inspection et infobulle

- [image `v416_manette_inspecter`] La fenêtre INSPECTER laisse voir, à travers son fond, l'étiquette « FRONTALE » et les cadres de la grille. Nom de l'objet ≈ 10,5 pt, description 8 pt. Les boutons [A] ÉQUIPER et [B] FERMER sont collés au cadre de l'aperçu.
- [image `v416_manette_1920x1080_fr_personnage`, recadrage ×2] À travers l'infobulle, les cadres des cases restent visibles : pixel 57 sur un trait contre 39 à côté. Le fond de l'infobulle (39, 36, 22) est presque celui des cases (36, 32, 17) : rien ne la détache.

### 2.4 Vue du personnage

- [image] Silhouette (40, 39, 36) sur un panneau (52, 47, 28) : **contraste 1,12:1** à 1080p, 1,21:1 dans `fenetre_169_max`, 1,18:1 dans `manette_equipe`. Elle paraît « presque noire » et plate.
- [référence] La silhouette d'Escape Together est elle aussi noire, mais **beaucoup plus sombre qu'un fond plus clair** (la scène floue derrière), avec un contour net. La nôtre est d'une luminance voisine de son fond.

### 2.5 Carnet

- [images `v416_manette_1920x1080_fr_carnet`, `v416_hud_1280x720_fr_carnet`] Huit lignes occupent environ 15 % de la colonne : rien ne défile. L'invite « [CROIX HAUT] [CROIX BAS] FAIRE DÉFILER » est pourtant affichée. Le texte du bouton « AIDE : masquer » est collé au haut du bouton.
- [images `v416_fenetre_{169_max,ultralarge_max,petite_1280x720}_carnet`] **Ces trois captures montrent le parking du Level 1 sans aucune interface** (voir § 3, C8).
- [image `v416_hud_1280x720_ja_carnet`] Les kanji en 6 pt sont illisibles (配電図を読む, 業務用連絡線を復旧する).

### 2.6 Paramètres et touches

- [image `v416_hud_1920x1080_fr_parametres`] Les pastilles JEU, VIDÉO… (12 pt gras) sont plus grosses que les lignes (8,5 pt). « < » et « > » sont collés en haut de leur case de 34 px. Les lignes n'occupent que 1 100 px sur 1 760 : la droite reste vide, alors que l'aide n'apparaît qu'au survol, en bas, sur 3 lignes qui défilent seules toutes les 2 s. Les lignes techniques du bas sont en 6,2 et 6,6 pt.
- [image `v416_hud_1280x720_de_parametres`] Les séparateurs sous ANZEIGEMODUS et VERTIKALE SYNCHRONISATION manquent, comme une partie des cadres des flèches.
- [image `v416_hud_1920x1080_ar_parametres`] Les libellés arabes sont alignés à gauche mais l'aide à droite. La case « moins » montre « > » et la case « plus » « < » : les deux flèches pointent vers la valeur.
- [image `v416_hud_1280x720_fr_touches`] Les cases de touches perdent des bords. Tous les textes sont au plancher.

### 2.7 Langues

- **Arabe** [`v416_hud_1920x1080_ar_personnage`, recadrage ×3] : puces à x ≈ 100 et texte aligné à droite vers x ≈ 513 ; « 0/2 » collé au début de la phrase ; titres de panneau (الأهداف, المخزون) à gauche ; onglets dans l'ordre gauche-droite, avec « > » devant le nom choisi.
- **Japonais** : 8 px à 720p, illisible. Les titres espacés lettre par lettre (« イ ン ベ ン ト リ », « 装 備 ») ne sont pas l'usage japonais.
- **Russe** : casse incohérente (objectifs en minuscules, « СНЯТЬ СУЩНОСТЬ » en capitales). « ХРАНИЛИЩЕ » s'empile sur 9 lettres.
- **Allemand** : le pied tient ; « GEISTIGE GESUNDHEIT » tient grâce à `TextFit`.

---

## 3. Constats dans le code

| # | Constat | Endroit |
|---|---|---|
| K1 | Plancher typographique de 6 pt, pour toutes les écritures | `UiFontInfo`, BRHUD.cpp:206 `FMath::Max(6.f, ...)` |
| K2 | L'échelle suit seulement la hauteur ; le réglage TAILLE est plafonné à 820 lignes virtuelles. À 720 lignes, U vaut au plus 0,833 même avec la taille d'interface à 1,25 | `Ui()`, 491-502 |
| K3 | Les anciennes primitives (`Txt`, `TxtLine`, `Wrap`) gardent des tailles de 6,2 à 8,6 pt pour le corps | 4541, 4598, 4626, 4869, 4913, 4918, 4958, 4964-4973, 5233, 5245, 5278, 5310, 5317-5318, 5877-6154, 4042, 4054, 4067 |
| K4 | `Frame` sans arrondi, épaisseur `1.f * U` (0,667 px à 720 lignes) | 759-765 ; appels 4519, 5241-5242, 5248, 4063, 4076 |
| K5 | Nom en case réduit à la largeur mais bloqué au plancher ; le banc ne connaît pas la largeur de la case (`TestTextMaxW` non fixé) | `DrawSlot`, 4541-4547 |
| K6 | Numéros de poche codés en dur (« 1 » à « 4 ») même si POCHE 1 est réassignée | `LayoutCharacterTab` 4455 ; `DrawQuickBar` 3539 |
| K7 | Silhouette : `point(lambda v: int(v * 0.28))` après conversion en gris ; teinte au dessin | `Tools/Blender/import_user_models.py`:451 (lumières 434-437, fond 421-422) ; BRHUD.cpp:4697. Ancienne variante : `generate_models.py`:1601 (× 0,25) |
| K8 | Légende d'équipement à `Y − 22U` ; TORSE à `SilH × 0,2` et CEINTURE à `SilH × 0,5` : 18 px entre les cases à 1080p pour une légende de 22 px | 4485-4490, 4528 |
| K9 | INSPECTER : voile 0,55 puis panneau `PanelBg` α 0,72 ; infobulle α 0,95 ; hauteur d'INSPECTER fixe (340U) | 4936-4942, 46, 4906 |
| K10 | Manette : hors PERSONNAGE, la croix change d'onglet (gauche, droite) et fait défiler (haut, bas). `AdjustSetting` n'est appelé que par les clics | `PadDirection` 4316-4323 ; `HandleInventoryMouse` 5417, 5422 ; BRPlayerController.cpp:1609, 1629, 1648, 1663 |
| K11 | Y dans l'inventaire ne sert qu'à l'aide du carnet ; aucun déplacement d'objet à la manette | BRPlayerController.cpp:1296 → `PadInventory(..., bHint)` 685-690 ; `PadSlotAction` 4379-4424 |
| K12 | `PadScroll` ajouté à **toutes** les zones de défilement de l'image (les deux colonnes du carnet, l'aide des réglages) ; 60 unités par appui, sans répétition (`ETriggerEvent::Started`) | `ScrollArea` 526 ; `PadInventory` 684 |
| K13 | Observations du carnet : `ContentH += 8U` par appareil, jamais dessiné : la fin de liste est trop basse de 8U × N | 6093 contre 6157 |
| K14 | Banc : `ShotNamed()` (demande de capture asynchrone, `FScreenshotRequest`) puis `CloseScreens()` dans la même étape | BRAutoTestV416Visual.cpp:1530-1534 ; `ABRAutoTest::Shot` BRAutoTest.cpp:547 (561 dans `1e67bb1`) |
| K15 | Mise en page de droite à gauche non inversée : titre aligné à gauche, puce à gauche, compteur à droite, libellé de réglage aligné à gauche | `Panel` 775 ; 4605-4608 ; 6033, 6038 ; 5233 ; onglets 4171-4189 |
| K16 | Mise en capitales ASCII seulement : `FString::ToUpper()` sur des textes traduits | BRWorld.cpp:3226, 3239 ; BRHUD.cpp:5877 (aussi 6420, 6463) |
| K17 | Flèches de réglage dessinées comme glyphes, à `BY + 4U` (non centrées) | 5243-5244 |
| K18 | Aide d'une ligne de réglage seulement au survol de la souris, zone de 3 lignes en bas, défilement automatique | 5257-5312 |
| K19 | Le pied de l'inventaire est un texte entre crochets ; la pause utilise les pastilles `KeyCap`/`KeyHints` (10,5 et 11,5 pt) | 4235-4276 contre 1245-1288 |
| K20 | Libellés de groupe empilés lettre par lettre (latin, cyrillique) | `VLabel` 780-804 ; 4660, 4664 |

## 4. Hypothèses

- **H1** (K14) : la capture d'écran est prise à la fin de l'image suivante ; l'inventaire est alors déjà fermé. Cela explique les trois `v416_fenetre_*_carnet` vides ; la capture `personnage` n'est pas touchée parce que la fermeture vient plus tard.
- **H2** (K4) : les rectangles de 0,667 px à position fractionnaire ne couvrent parfois aucun centre de pixel : arêtes sautées selon la position.
- **H3** (§ 2.6) : en arabe, « < » et « > » passent par la mise en forme de droite à gauche et reçoivent leur glyphe miroir (caractères `Bidi_Mirrored`).
- **H4** : un lissage « Light » (vertical seulement) préserverait mieux H, B et R vers 9 à 10 px que le lissage par défaut de Roboto. À essayer sur une capture, non nécessaire si C1 est appliquée.
- **H5** : la silhouette sombre voulait imiter Escape Together ; la référence obtient la lisibilité par un **fond plus clair**, que notre panneau n'a pas.

---

## 5. Fondations proposées (jetons)

**Échelle typographique** (points de référence à 1080p, passés à `TextF` ; taille finale = max(plancher, round(Taille × U))) :

| Jeton | Taille | Usage | à 720 lignes |
|---|---|---|---|
| `title.menu` | 15 gras | MENU > | 10 |
| `tab` | 13 (choisi : gras) | onglets | 9 |
| `panel.title` | 12,5 gras, espacement 2U (0 pour CJK, arabe, persan) | titres de panneau | 8 |
| `body` | 12 | objectifs, carnet, infobulle, description, lignes de réglage, aide | 8 |
| `body.small` | 11 | phrases secondaires, observations, états | 7 |
| `label` | 10 gras | nom en case, légendes d'équipement, groupes, en-têtes de colonnes | 7 (plancher) |
| `value` | 12 gras, chiffres de largeur fixe si possible | %, 0/2, valeurs des réglages | 8 |
| `key` | pastille 10,5 gras + libellé 11,5 (`KeyCap` existant) | raccourcis | 7 / 8 |

**Planchers** : 7 pt (cadratin 9,3 px) en latin, cyrillique et grec ; **8 pt (10,7 px) pour zh-Hans, zh-Hant, ja, ko, ar, fa**. Hauteur de ligne = max(ancienne valeur × U, ⌈hauteur mesurée × 1,2⌉).
**Espacements** : base 4U ; marge de panneau 20U ; écart entre une légende et un cadre ≥ 6U ; écart entre cases 10U (gardé).
**Couleurs** (palette gardée, contrastes conformes) ; ajouts : `ModalBg` (0,03 ; 0,027 ; 0,012 ; **1,0**) ; `TooltipBg` (0,045 ; 0,04 ; 0,02 ; 1,0) ; `Focus` = cadre jaune α 1,0 de max(2 px, 2U) + fond jaune α 0,10 ; `Refus` = Danger α 0,9 pendant 150 ms ; `Halo d'icône` = (1 ; 0,85 ; 0,3) α 0,10 en dégradé radial.
**Cohérence avec l'univers** (secondaire, sans réduire le texte) : rayon des panneaux de 14U ramené à 6U (aspect « terminal Threshold Systems » plus proche de la référence), lignes de balayage gardées, pastilles de touches partout.

---

## 6. Corrections priorisées (12)

Pour chaque correction : **Où**, **Actuel**, **Proposé**, **Risque dans les 22 langues**, **Acceptation sur capture**.

### C1. Échelle typographique et plancher : texte lisible à 1280×720 (priorité 1)
- **Où** : `UiFontInfo` BRHUD.cpp:206 ; appels cités en K3 ; onglets 4180-4182 ; MENU 4152 ; hauteurs de ligne 4582 (20U), carnet 24U et 19U (5879-6008, 6083-6089, 6145), infobulle 19U (4895, 4914), aide des réglages 20U (5257).
- **Actuel** : plancher 6 pt ; corps de 7,2 à 8,6 pt à 1080p et 6 pt à 720p ; onglets 9,5 pt, soit 5 px de capitales à 720p ; noms en case 6 pt partout.
- **Proposé** : jetons du § 5 ; plancher 7 pt (8 pt pour CJK et arabe, choisi selon la culture courante ou une plage du texte, comme `NeedsShaping`) ; hauteurs de ligne dérivées de la mesure (objectifs 20U → 23U, carnet 24/19U → 26/22U, infobulle 19U → 22U, aide 20U → 22U). Infobulle élargie de 380U à 420U.
- **Risque** : jusqu'à +46 % de longueur (« GEISTIGE GESUNDHEIT », « LANTERNA DE CABEÇA »). Les zones à risque sont seulement les zones de hauteur fixe : INSPECTER (C5), pied (C11) et cases (C3). Les objectifs, le carnet et l'infobulle sont déjà coupés et défilants.
- **Acceptation** : captures 1280×720 en fr, de, ru, ja et ar (PERSONNAGE, CARNET, PARAMÈTRES) : capitales des objectifs ≥ 7 px (aujourd'hui 6), onglets ≥ 7 px (5), glyphes japonais ≥ 11 px (8) ; « TOUCHES », « THRESHOLD » et « ÉCHAP » lisibles sans zoom. Banc : 0 texte sous 7 pt (8 pt en CJK et arabe) aux trois définitions, corps ≥ 8 pt à 720p et ≥ 12 pt à 1080p ; toujours 0 hors largeur, 0 hors panneau, 0 chevauchement.

### C2. Lignes d'un pixel stables (priorité 2)
- **Où** : `Frame` 759-765 et appels de K4 ; séparateurs `DrawRect(..., 1.f * U)` 5248, 4076.
- **Actuel** : épaisseur 0,667 px à position fractionnaire : arêtes manquantes.
- **Proposé** : dans `Frame` (et un petit utilitaire de ligne), arrondir X, Y, W et H, épaisseur = max(1, round(T)). Séparateurs : `Y = RoundToFloat(Y)`, hauteur `Max(1, Round(U))`.
- **Risque** : aucun texte touché.
- **Acceptation** : à 1280×720, chaque case montre ses 4 bords (aucun bord de luminance égale au fond) ; 6 séparateurs sur 6 dans PARAMÈTRES > VIDÉO en allemand ; les 75 cases de TOUCHES sont fermées.

### C3. Cases : nom lisible, quantité, vrai raccourci, états lisibles (priorité 3)
- **Où** : `DrawSlot` 4505-4560 ; `LayoutCharacterTab` 4455 ; `DrawQuickBar` 3539.
- **Actuel** : nom 6,2 pt plafonné, qui déborde en russe ; bande claire de 45 % sur toutes les cases ; quantité « x{N} » en 7,5 pt sur la ligne du numéro de poche ; numéros 1 à 4 fixes ; curseur = cadre 2U α 0,85 ; dépôt refusé sans retour visuel sur la case.
- **Proposé** :
  - nom en jeton `label` (10 pt gras), **2 lignes au plus** (`WrapF` sur S − 10U) puis points de suspension ; si 2 lignes, icône déplacée à Y + 0,30·S et réduite à 0,62·S ; `TestTextMaxW = S − 10U` pour que le banc contrôle la case ;
  - bande claire seulement sur une case pleine (α 0,05, hauteur du nom + 6U) ;
  - quantité en pastille « ×N » en bas à droite (10 pt gras, fond noir α 0,6, rayon 4U) ;
  - numéro de poche = `BRKeys::Primary(EBRAction::Pocket1 + i)` en petite pastille, dans l'inventaire et dans la barre rapide ;
  - curseur et survol en jeton `Focus` (cadre de 2 px au moins, fond α 0,10, quatre coins de 8U) ; quand le curseur est sur un objet équipable du sac, la case d'équipement visée s'éclaire (α 0,25) ;
  - cible de dépôt incompatible : cadre Danger ; action refusée à la manette (A sur une case vide) : flash Danger de 150 ms et son `S_UIDeny`, qui existe déjà ;
  - halo radial `UI_Radial` derrière l'icône (rayon 0,42·S, α 0,10).
- **Risque** : les noms courts vont jusqu'à 18 caractères (pt-BR) ; à 720p, en 7 pt, environ 12 capitales tiennent sur 75 px. Avec 2 lignes, les 22 langues tiennent : le mot le plus long, « МИНДАЛЬНАЯ », fait environ 62 px. Un nom de touche long passe en points de suspension sur 0,45·S.
- **Acceptation** : 1280×720 en ru, pt-BR et it : « eau d'amande » entièrement dans la case, au moins 2 px de marge de chaque côté ; 1080p : capitales du nom ≥ 7 px (aujourd'hui 5) ; une pile de 2 bandages montre « ×2 » ; POCHE 1 réassignée à F affiche F ; le curseur se repère sur la capture réduite de moitié ; caméscope, frontale et lampe ≥ 3:1 sur leur halo (aujourd'hui 1,5 à 2:1).

### C4. Vue du personnage et disposition de l'équipement (priorité 4)
- **Où** : `Tools/Blender/import_user_models.py`:408-453 (`render_silhouette`) ; BRHUD.cpp:4697 ; `LayoutCharacterTab` 4471-4490 ; `DrawSlot` 4528.
- **Actuel** : sortie × 0,28 en gris, teinte (0,85 ; 0,82 ; 0,72 ; 0,95) ; contraste 1,1 à 1,2:1 ; aplat (p5 à p95 : 43 à 45) ; légende CEINTURE sur la case TORSE.
- **Proposé** :
  1. **Rendu** : garder la lumière principale et ajouter un contre-jour (SUN 2,0, rotation (100, 0, 80)) ; remplacer `v * 0.28` par `20 + v * 0.42` (plage d'environ 20 à 127), alpha gardé. Relancer le script dans Blender, réimporter `I_Silhouette` et vérifier la texture cuite dans le paquet.
  2. **Dessin** : teinte (1 ; 0,97 ; 0,9 ; 1) ; halo `UI_Radial` derrière, centré à (SilX + SilW/2, SilY + 0,45·SilH), rayons (0,9·SilW, 0,6·SilH), couleur (1 ; 0,85 ; 0,3 ; 0,07).
  3. **Disposition** : `Belt.Y = max(SilY + 0,5·SilH, Chest.Y + EqS + 30U)`, légendes à Y − 20U, écart ≥ 8U ; même règle pour TÊTE et MAIN.
  
  L'image est un rendu dérivé, absent de `RawAssets/protected_assets.json` (vérifié : aucune entrée Silhouette ni Icons).
- **Risque** : légendes jusqu'à « OBERKÖRPER » (10 lettres, environ 90 px en 10 pt gras, pour une case de 104U) ; `TextFit` en repli.
- **Acceptation** : captures en vraie fenêtre (1366×768 et 1080p, Level 0 et Level 1) : contraste moyen silhouette/panneau ≥ 2:1, écart de luminance interne p5 à p95 ≥ 30 niveaux (les plis se voient), aucune légende ne coupe un cadre (≥ 6 px), dans les 22 langues (relevé du banc).

### C5. INSPECTER et infobulle opaques, hiérarchie interne (priorité 5)
- **Où** : `DrawInspect` 4936-5013 ; `DrawTooltip` 4895-4919.
- **Actuel** : voile 0,55 + fond α 0,72 ; infobulle α 0,95 ; hauteur fixe de 340U ; nom ≈ 10,5 pt, description 8 pt, boutons 8,5 pt dont le texte est posé à BY + 6U.
- **Proposé** : `ModalBg` α 1 pour INSPECTER, voile 0,70 ; infobulle en `TooltipBg` α 1, un ton au-dessus des cases, cadre jaune α 0,7, ombre portée de 6U (noir α 0,5). INSPECTER : nom 16 pt gras, type et quantité 10,5 pt, charge 11,5 pt jaune, description 12,5 pt (lignes de 26U), boutons en pastilles 12 pt gras centrés verticalement ; **hauteur = max(340U, contenu + 2 × 20U + hauteur des boutons)** ; aperçu ramené de 220U à 200U.
- **Risque** : descriptions jusqu'à 177 caractères (caméscope) : environ 4 lignes de 55 caractères ; la hauteur dynamique couvre les langues longues et les deux lignes de charge des piles.
- **Acceptation** : nouvelle `manette_inspecter` : aucun libellé ni cadre discernable sous la fenêtre (différence ≤ 2 niveaux entre une zone au-dessus d'un cadre et sa voisine) ; même mesure sous l'infobulle ; boutons centrés (écart haut/bas ≤ 1 px) ; en allemand, la description ne touche pas les boutons.

### C6. PARAMÈTRES utilisables à la manette (priorité 6)
- **Où** : `PadDirection` 4316-4323 ; `HandleInventoryMouse` 5417, 5422 ; choix de l'aide 5268-5275 ; BRPlayerController.cpp:1609-1673 (croix), 1337 et 1353 (LB, RB).
- **Actuel** : à la manette on change d'onglet et on fait défiler, sans pouvoir régler une valeur ; l'aide d'une ligne n'apparaît jamais.
- **Proposé** : curseur de ligne `SettingsPadRow`.
  - Croix haut/bas : ligne (bouclée dans la catégorie) ; croix gauche/droite : `AdjustSetting(i, ±1)` ; A : même effet qu'un clic sur la ligne.
  - LT/RT : catégorie ; LB/RB : onglets (inchangés).
  - `HintSetting` = ligne du curseur ; focus en barre de 4U à gauche + fond jaune α 0,10.
  - Pied en pastilles : Onglets · Catégorie · Choisir · Régler · Fermer.
  - TOUCHES : la croix choisit la case et A lance la saisie (le clavier reste le moyen de réaffecter ; l'aide doit le dire).
  - MODE D'AFFICHAGE : A = conserver, pendant la confirmation existante.
- **Risque** : environ 5 nouvelles clés courtes (`loc_extract.py`, entrées « fuzzy » jusqu'à relecture) ; pied plus long (voir C11) ; vérifier que LT et RT ne déclenchent aucune action de jeu inventaire ouvert (scanner du Level 6).
- **Acceptation** : banc à la manette seule : LUMINOSITÉ +1 puis −1 (la valeur change puis revient), l'aide de la ligne s'affiche ; captures 1080p et 720p avec la ligne focalisée identifiable ; aucun réglage ne change pendant les changements d'onglet.

### C7. Déplacer un objet à la manette (priorité 7)
- **Où** : `PadSlotAction` 4379-4424 ; BRPlayerController.cpp:1296 ; pied `HUD.PadCharacterFooter` 4245 ; dessin de l'objet porté 4283-4293.
- **Actuel** : la souris propose « [GLISSER] DÉPLACER » ; la manette ne peut pas mettre un objet du sac dans une poche ni réorganiser.
- **Proposé** : dans PERSONNAGE, **Y = prendre / poser**.
  - Premier appui : `Dragging = PadCursor` ; icône portée sur la case du curseur (84U, α 0,85), source estompée à α 0,3 comme à la souris.
  - La croix déplace le curseur ; second appui : `C->MoveItem(...)`, qui applique déjà les règles et prévient par un message si l'objet ne peut pas aller dans cet emplacement.
  - B annule le port avant de fermer l'inventaire.
  - Pied et infobulle : « [Y] Déplacer ».
- **Risque** : une clé courte `HUD.PadMove` (≤ 12 caractères dans toutes les langues, par exemple « VERSCHIEBEN »).
- **Acceptation** : banc à la manette seule : eau d'amande du sac vers la poche 4, puis échange poche 4 ↔ poche 1 ; capture pendant le port (icône sur la case visée, source estompée) ; B pendant le port rend l'objet sans fermer l'inventaire.

### C8. Carnet : vrai défilement, une colonne à la fois, preuves du banc (priorité 8)
- **Où** : `ScrollArea` 509-541 (526 ; repères 529-539) ; `PadInventory` 684 ; `DrawNotebookTab` 6024, 6142, 6093 et 6157 ; bouton d'aide 6051 ; BRAutoTestV416Visual.cpp:1530-1534.
- **Actuel** : les deux colonnes défilent ensemble, un pas de 60U par appui, sans répétition ; repères de 6U à α 0,45 ; décalage de 8U × N à la fin ; texte du bouton non centré ; aucun cas du banc ne dépasse sa zone ; captures `fenetre_*_carnet` vides.
- **Proposé** :
  - colonne focalisée (croix gauche/droite dans CARNET ; LB/RB restent les onglets), cadre de la colonne focalisée en jaune α 0,6 ; `PadScroll` appliqué seulement à la zone focalisée (paramètre de `ScrollArea`) ;
  - croix haut/bas : une ligne (26U), avec répétition à 8 pas par seconde après 0,35 s ; stick droit vertical : défilement continu ;
  - rail de 3U (jaune α 0,15) et curseur proportionnel d'au moins 28U (jaune α 0,6) ; fondu de 24U en bas quand du texte reste caché (`Gradient` existe déjà, 1189) ;
  - invite « FAIRE DÉFILER » seulement s'il reste quelque chose à faire défiler ; corriger 6093 ; texte du bouton centré à BY + (BtnH − hauteur)/2 ;
  - **banc** : cas long (Level 6 ou 11, tous indices lus, aide de niveau 2, à 1280×720 et taille d'interface 1,25), vérifier `LastNotebookLeftRoom > 0` puis une baisse de 26U par appui jusqu'à la dernière ligne ; fermer l'inventaire à l'étape **suivante** de la capture (K14).
- **Risque** : une clé courte (« Colonne »).
- **Acceptation** : capture 720p d'un carnet long : rail visible, dernière ligne atteinte après ⌈(contenu − zone)/26U⌉ appuis ; les trois `v416_fenetre_*_carnet` montrent le carnet ; le banc passe de « NON VÉRIFIÉ » à « vérifié ».

### C9. Arabe et persan : mise en page inversée (priorité 9)
- **Où** : `Panel` 775 ; objectifs 4605-4608 ; carnet 6033, 6038 ; onglets 4171-4189 ; MENU 4152 ; réglages 5233 et 5243-5244 ; `VLabel` 4660, 4664 ; espacement des titres `TextSpaced` 982.
- **Actuel** : voir § 2.7.
- **Proposé** : utilitaire `MirrorX` actif quand `BRLoc::IsRightToLeft()`.
  - Titres alignés à droite (X + W − 20U) ; puce à X + W − 24U, texte qui finit à 8U de la puce, compteur à l'extrémité gauche de la ligne.
  - Onglets disposés de droite à gauche ; repère du choix par le soulignement seulement.
  - Réglages : libellé à droite, groupe [◄ valeur ►] à gauche ; **flèches dessinées en traits** (comme les repères de `ScrollArea`), donc insensibles au miroir des glyphes.
  - Espacement des titres à 0 aussi pour le chinois, le japonais et le coréen.
- **Risque** : nul pour les langues de gauche à droite (code isolé derrière `IsRightToLeft`) ; le persan en profite aussi.
- **Acceptation** : captures ar et fa à 1080p et 720p : puce à ≤ 12 px du texte (aujourd'hui environ 400 px) ; titres alignés à droite ; compteur en fin de ligne ; flèches de réglage tournées vers l'extérieur ; aucune différence de pixels en français hors des zones modifiées.

### C10. Casse correcte et cohérente (priorité 10)
- **Où** : BRWorld.cpp:3226 et 3239 ; BRHUD.cpp:5877 (et 6420, 6463).
- **Actuel** : « RéTABLIR LA LIAISON DE SERVICE », « LIRE LE SCHéMA » ; objectifs russes non mis en capitales.
- **Proposé** : objectifs et étapes en **casse de phrase** : retirer `ToUpper`, distinguer le but du niveau par la graisse et la couleur. Réserver les capitales aux libellés de trois mots au plus, par `FText::ToUpper()` (ICU, sensible à la culture : i → İ en turc).
- **Risque** : textes 10 à 15 % plus étroits, donc favorable.
- **Acceptation** : relevé du banc : aucun mot avec une minuscule entre deux capitales (expression `\p{Lu}\p{Ll}\p{Lu}`) en fr, de, tr, pl, cs, hu et pt ; même casse pour un même rôle en fr, ru et uk ; capture fr 720p.

### C11. Raccourcis et libellés de groupe : un seul système (priorité 11)
- **Où** : pied 4235-4276 ; `KeyCap`/`KeyHints` 1245-1288 ; `VLabel` 780-804 ; raccourci de l'infobulle 4918.
- **Actuel** : pied entre crochets, en 8 pt (6 pt à 720p) ; libellés empilés lettre par lettre ; la pause utilise déjà des pastilles lisibles.
- **Proposé** : pied = `KeyHints` aligné à droite, construit depuis des paires (touche, libellé) en casse de phrase. Si la place manque : (1) masquer « © 1992 THRESHOLD SYSTEMS », (2) écart de 28U à 16U, (3) libellés à 10,5 pt, (4) points de suspension sur le dernier libellé. Libellés **horizontaux** en 10,5 pt gras jaune au-dessus des poches (poches descendues de 18U) et du sac (dans l'écart de 40U existant), comme dans la référence.
- **Risque** : pied ru, uk et hu (123 à 125 caractères) : environ 1 370 px à 1080p pour 1 570 disponibles, environ 915 px à 720p pour environ 1 050 ; tient sans le copyright. Les phrases composées actuelles doivent être découpées en libellés séparés (nouvelles clés ; les anciennes sont retirées par `loc_extract.py`). Libellé de groupe le plus long : « PENYIMPANAN », environ 95 px.
- **Acceptation** : 720p en ru, hu et de : tous les raccourcis visibles sans points de suspension, sur une même ligne de base, capitales des libellés ≥ 7 px ; libellés de groupe sur une ligne dans les 22 langues.

### C12. PARAMÈTRES : aide à côté de la ligne, flèches centrées, hiérarchie (priorité 12)
- **Où** : `DrawSettingsTab` 5189-5318.
- **Actuel** : pastilles 12 pt gras au-dessus de lignes en 8,5 pt ; flèches à BY + 4U ; un panneau vide à 40 % ; aide en bas, sur 3 lignes défilantes ; survol α 0,06 ; lignes techniques en 6,2 et 6,6 pt.
- **Proposé** : lignes en `body` (libellé Ink, valeur en `value` jaune), catégories 11,5 pt gras ; chevrons en traits centrés (8U, épaisseur 2U).
  - Quand toutes les lignes tiennent sur une colonne (`PerColumn == Count`) : **colonne d'aide à droite** (X = PanelX + W + 30U, largeur PanelW − W − 58U, du haut des lignes jusqu'au mode de rendu), avec le libellé de la ligne en titre (12 pt gras) puis l'aide en 11,5 pt, **sans défilement automatique**.
  - Focus et survol en jeton `Focus` (le même qu'en C6) ; lignes techniques en 10 pt.
- **Risque** : l'aide la plus longue (PROFIL, environ 600 caractères en français, plus en allemand) fait environ 14 lignes de 70 caractères dans 600 px à 1080p et environ 12 lignes dans 400 px à 720p : tient. Avec deux colonnes de lignes, garder la zone du bas mais sur 4 lignes.
- **Acceptation** : 1080p et 720p en fr, de et ar : valeurs et flèches centrées verticalement (écart ≤ 1 px) ; aide de chaque ligne de JEU, VIDÉO et INTERFACE lisible en entier sans défilement automatique ; hiérarchie visible (titre du panneau > ligne > aide).

**Ordre d'intégration conseillé** : C2 et C1 ensemble (nouvelle base de mesures du banc) ; puis C3, C5, C4, C10 (dessin et code pur) ; puis C6, C7, C8 (manette ; `BRPlayerController.cpp` est un fichier partagé, sous un seul responsable) ; puis C9, C11, C12 (mises en page et nouvelles clés de texte, en un seul passage de `Tools/Localization/loc_extract.py`).

---

## 7. Non vérifié par cet agent

- Unreal n'a pas été lancé ; aucune correction n'est essayée. Le comportement de la manette est déduit du code et du rapport du banc.
- Aucune capture en zh-Hans, zh-Hant, ko ni fa : leurs constats viennent du japonais, de l'arabe et du code.
- Le `LegacyFontSize` d'environ 10 est déduit des mesures. La mise en capitales ASCII seulement de `FString::ToUpper` est déduite des images (« RéTABLIR ») et de la connaissance du moteur. H1 à H5 restent des hypothèses.
- Les contrastes sont mesurés sur des PNG 8 bits ; les couleurs à l'écran dépendent du profil et de la luminosité choisie.

## 8. Résumé (12 lignes)

1. Défaut n° 1 : texte trop petit (plancher 6 pt, corps de 7 à 8,6 pt ; 5 163 textes sur 18 345 au plancher) ; les couleurs sont conformes (8:1 à 11:1).
2. C1 : jetons typographiques (corps 12 pt, libellés 10 pt), plancher 7 pt (8 pt en CJK et arabe), lignes dérivées de la mesure.
3. C2 : lignes de 0,67 px à 720 lignes, arêtes perdues ; arrondir à au moins 1 px.
4. C3 : noms en case de 5 px, débordement en russe, numéros de poche figés, curseur faible, icônes sombres à 1,5–2:1.
5. C4 : la silhouette est une PNG × 0,28 puis teintée (contraste 1,1:1, sans modelé) ; nouveau rendu + halo ; CEINTURE chevauche TORSE.
6. C5 : INSPECTER (α 0,72) et infobulle (α 0,95) laissent voir les cases ; fonds opaques, hauteur dynamique.
7. C6 : à la manette, aucun réglage modifiable dans PARAMÈTRES ; curseur de ligne + aide.
8. C7 : aucun déplacement d'objet à la manette ; Y = prendre / poser avec `MoveItem`.
9. C8 : le carnet ne défile pas (cas trop court), les colonnes défilent ensemble ; focus, rail, cas long, captures du banc à refaire.
10. C9 : arabe non inversé (puces à environ 400 px, titres à gauche, flèches inversées).
11. C10 : `FString::ToUpper` ASCII (« RéTABLIR ») ; casse de phrase ou `FText::ToUpper`.
12. C11 et C12 : pied en pastilles comme la pause, libellés de groupe horizontaux, aide des réglages à droite et flèches centrées.

---

## 9. Implémentation (v417-ui)

| | |
|---|---|
| **Branche** | `v417-ui`, partie de `1e67bb1`, arbre `wt/ui`. Rien n'est poussé. |
| **Fichiers modifiés** | `Source/Backrooms/Private/BRHUD.cpp`, `Source/Backrooms/Public/BRHUD.h`, `Source/Backrooms/Private/BRWorld.cpp` (les deux lignes `ToUpper` de C10, rien d'autre), `Tools/Blender/import_user_models.py`, `RawAssets/Icons/I_Silhouette.png` (régénérée). Plus ce rapport et `Docs/v417/agents/interface_cles.json`. |
| **Non vérifié** | Unreal n'a été ni compilé ni lancé dans cet arbre. Le C++ a été relu à la main et par trois contrôles par script : équilibre des accolades et parenthèses, locales qui masqueraient un membre d'`ABRHUD` ou un symbole de l'espace anonyme (C4458, C4459), polices et constantes locales déclarées mais inutilisées. Aucun constat. Les largeurs de texte ont été mesurées en Roboto, avec les polices du moteur (`UE_5.8/Engine/Content/Slate/Fonts`), pour 16 langues. Le chinois, le japonais, le coréen, l'arabe et le persan passent par les polices de secours et n'ont pas été mesurés. |

### 9.1 Par correction

| Correction | Commit | Contenu |
|---|---|---|
| **C2 + C1** | `9d57ed8` | Plancher de `UiFontInfo` à **7 pt**, **8 pt** en zh, ja, ko, ar et fa (`DenseScript`, culture relue seulement quand elle change). Jetons `UiType` (§ 9.2). En-tête, onglets, pied, OBJECTIFS, JOURNAL, PARAMÈTRES, TOUCHES et CARNET passent des anciens facteurs de police aux jetons. Les hauteurs de ligne viennent de `LineStep` : max(valeur × U, hauteur mesurée × 1,2). `TextLineF` et `ParagraphF` (lignes au plus, puis points de suspension) alimentent `TestTextMaxW` et `bTestTextCut`. Titres de panneau sans espacement des lettres en CJK. `Frame` et `HLine` tombent sur des pixels entiers, d'au moins 1 px. Textes désormais bornés : nom du niveau (place laissée par les onglets), aide de TOUCHES (2 lignes), objets mis de côté (2 lignes), plaques du Level 6 et fragments de route (coupés à la colonne). Le bouton d'aide du carnet est centré verticalement. La largeur de « MENU > » est relue dans `DrawMessages`. |
| **C3** | `ddd2aec` | Nom en case : 10 pt gras, 2 lignes au plus puis « … », `TestTextMaxW = S − 10U`. Icône sous le nom (réduite si le nom prend 2 lignes), sur un halo `UI_Radial` de 0,42·S à 10 %. Bande claire seulement sur une case pleine. Quantité « x{N} » en pastille en bas à droite. **Touche de poche** = `BRKeys::Primary(Pocket1 + i)` (l'API existait), en pastille en bas à gauche, dans l'inventaire et dans la barre rapide. Jeton focus : cadre max(2 px, 2U), fond jaune 10 %, quatre coins épais. La case d'équipement qui accepte l'objet survolé ou porté s'éclaire à 25 % (`BRItems::CanEquipIn`). Dépôt impossible : cadre rouge. A ou X sur une case sans action : cadre rouge 150 ms et `S_UIDeny`. |
| **C5** | `2301718` | INSPECTER : voile 0,70, panneau opaque `ModalBg` (`Panel(..., bOpaque)`). Nom 16 pt gras, type 10,5, charge 11,5, description 12,5 (lignes de 26U), boutons en pastilles 12 pt gras centrés. Hauteur = max(340U, contenu + 20U + boutons + 20U, aperçu), bornée à l'écran. Aperçu de 200U sur un halo. Les deux boutons se partagent la largeur si les libellés sont longs. Infobulle : 420U, `TooltipBg` opaque, cadre jaune 70 %, ombre portée de 6U, nom 12,5 gras, texte 12 (22U), raccourci 10,5, hauteur mesurée. |
| **C4** | `ba6a8b6` | **Rendu régénéré dans Blender 5.2.2** (§ 9.3). Dessin : teinte (1 ; 0,97 ; 0,9 ; 1), halo (1 ; 0,85 ; 0,3) à **5 %** (proposé : 7 %, voir § 9.3). Équipement : entre deux cases d'une même colonne, la légende du dessous (hauteur mesurée + 6U) et 8U jusqu'au cadre du dessus. Il en résulte CEINTURE = max(0,5·SilH, TORSE + EqS + écart) et MAIN de même sous TÊTE, sans descendre sous le panneau. Légendes en 10 pt gras, à 6U de leur cadre, bornées à S + 12U. |
| **C10** | `af337ed` | `BRWorld.cpp` : `ToUpper` retiré du but et des étapes (casse de phrase, comme le carnet). Carnet : but du niveau en casse de phrase, en gras. OBJECTIFS et suivi d'exploration : le but se distingue par le gras (et le jaune dans OBJECTIFS). Destination du départ de groupe et nom du lot de fin de chapitre : `FText::ToUpper` (ICU). |
| **C8, dessin** | `cbf4548`, `ec8f2df` | `ScrollArea` : rail de 3U à 15 % et curseur proportionnel d'au moins 28U à 60 %, à 10U du bord droit de la colonne. Fondu de 24U (20 % de la zone au plus) dessiné **après** le contenu, tant que du texte reste caché. Paramètre `Zone` avec `INDEX_NONE` par défaut, qui garde le comportement v4.16. `PadScrollZone` : seule la zone choisie reçoit le défilement de la manette, avec un cadre jaune à 60 %. Zones : OBJECTIFS 0 ; JOURNAL 0 et 1 ; CARNET 0 (étapes) et 1 (observations) ; aide des PARAMÈTRES 0. Les zones des colonnes reçoivent la colonne entière : le rail passe dans la marge, hors des compteurs alignés à droite. Pied de la manette : « FAIRE DÉFILER » seulement s'il reste à défiler (`bScrollRoom`). **K13 corrigé** : l'écart de 8U entre deux appareils est dessiné. API pour le contrôleur : § 9.6. |
| **C12, en partie** | `a74fd4f` | Chevrons moins et plus **dessinés en traits** et centrés dans leur case, donc insensibles au miroir bidirectionnel en arabe. Pastilles de catégorie en 11,5 pt gras centrées. Ligne survolée : fond 10 % et barre de 4U. Les libellés à 12 pt, les valeurs à 12 pt gras et les lignes techniques à 10,5 et 10 pt sont faits dans C1. |

### 9.2 Valeurs choisies

| Jeton (`UiType`) | Points à 1080 lignes | À 720 lignes (U = 0,667) | Usage |
|---|---|---|---|
| `MenuTitle` | 15 gras | 10 | MENU > |
| `Tab` | 13 gras | 9 | onglets |
| `PanelTitle` | 12,5 gras | 8 | titres de panneau, en-têtes d'entités, nom de l'infobulle |
| `Body` | 12 | 8 | objectifs, étapes, descriptions, lignes de réglage, actions de TOUCHES |
| `BodySmall` | 11 | 7 | phrases secondaires, observations, notes, aide des réglages |
| `Hint` | 10,5 | 7 | pied, raccourci de l'infobulle, mode de rendu |
| `Label` | 10 gras | 7 (8 en CJK, arabe, persan) | nom en case, légendes, pastilles, en-têtes de colonnes |
| `Value` | 12 gras | 8 | compteurs, valeurs des réglages, boutons |

Hauteurs de ligne (base × U, ou plus si la mesure × 1,2 l'exige) : objectifs 23U (avant 20), phrase du bas 21U (18), carnet 26U et 22U (24 et 19), observations 26U et 22U (24 et 19), infobulle 22U (19), aide des réglages 22U (20), journal 24U et 22U (avant : 23U, et 18 à 20U). Couleurs ajoutées : `ModalBg` (0,03 ; 0,027 ; 0,012 ; 1), `TooltipBg` (0,045 ; 0,04 ; 0,02 ; 1). Le jeton focus suit le § 5.

Mesures en Roboto, 16 langues (fr, en, de, es-ES, es-419, pt-BR, pt-PT, ru, uk, it, tr, pl, hu, cs, sv, id) :
- **noms en case** : tous tiennent en 2 lignes au plus, aussi bien à 1080 lignes (10 pt dans 111 px) qu'à 720 (7 pt dans 74 px). Aucun mot n'est plus large que la case.
- **légendes d'équipement** : toutes tiennent sans réduction.
- **onglets** : de 580 à 743 px à 1080 lignes, pour 1 760 px disponibles ; de 404 à 521 px à 720 lignes, pour 1 173. Le nom du niveau (≤ 331 px) tient à côté.
- **pieds** : tous tiennent, PERSONNAGE à la manette compris.
- **libellés de réglage** : le plus long fait 404 px pour 686 permis (« REFLEJOS CON TRAZADO DE RAYOS DE ALTA CALIDAD »).

### 9.3 Vue du personnage (C4), rendu

- `import_user_models.py` dépend de `Tools/SourceModels/asyc_hazmat.glb`, absent du dépôt. Le nouveau mode `blender -b -P Tools/Blender/import_user_models.py -- silhouette` (`process_silhouette_from_parts`) s'en passe. Il recharge les dix pièces dérivées `RawAssets/Meshes/SM_Hazmat_*.fbx`, dont le pivot est sur leur articulation, et les replace sur les articulations de `user_models.json` (hauteur reconstituée : 1,80 m, 55 521 sommets). Il les réunit, puis appelle le même `render_silhouette` que `process_hazmat`. `user_models.json` n'est pas réécrit. Les pièces protégées sont seulement lues.
- `render_silhouette` :
  - lumière principale de trois quarts, en haut à gauche (soleil 3,0) ; appoint à droite (0,8) ; contre-jour d'avant gardé (4,0) ;
  - matières neutres sur l'objet rendu seulement : combinaison 0,62, masque 0,2, visière 0,06 ;
  - vue « Standard » et 64 échantillons ;
  - gris = **20 + 0,42 × rendu** (avant : × 0,28) ;
  - post-traitement par numpy dans Blender, car PIL est absent du Python de Blender.
- Résultat, `RawAssets/Icons/I_Silhouette.png` : même cadrage (couverture 49 %), gris p5, p50 et p95 à **52, 104 et 117** (avant : 38, 43 et 45). Le relief est visible (plis, visière, tuyau, poches). Image vue avec l'outil de lecture.
- Contraste calculé par composition linéaire, sur la couleur du panneau mesurée en jeu (52, 47, 28) avec la teinte du HUD. La même méthode retrouve 1,13:1 pour l'ancienne image, soit la mesure en jeu de 1,12:1. Nouvelle image : **2,23:1** en moyenne contre le panneau, étendue p5–p95 de **63 niveaux** (avant : 6). Contre le fond local, halo compris : 1,85:1 avec un halo à 7 % et 1,94:1 à 5 %, d'où le choix de **5 %**.
- **À faire dans Unreal** : réimporter `/Game/Backrooms/UI/I_Silhouette`. `backrooms_setup.py` ne réimporte une icône existante qu'au changement de version ; ce fichier m'est interdit.

### 9.4 Nouvelle clé de texte (une seule)

`HUD.PadInventoryFooterNoScroll` vaut « [{Prev}] [{Next}]  ONGLETS     [{Close}]  FERMER ». C'est le pied de la manette hors PERSONNAGE quand rien ne défile. Ses 21 traductions sont dans `Docs/v417/agents/interface_cles.json`. Elles reprennent **mot pour mot** les termes de `HUD.PadInventoryFooter` de chaque `Game.po`, lus sans les modifier, sans le segment de défilement. À reporter dans les catalogues (`loc_extract.py`). Tant que ce n'est pas fait, les autres langues affichent le français pour ce seul pied.

### 9.5 Ce qui reste

- **Non fait, comme demandé** : C6 (PARAMÈTRES à la manette), C7 (déplacer un objet à la manette), partie manette de C8 (§ 9.6), C9 (mise en page de droite à gauche). Le mode RTL est inchangé, sauf les chevrons des réglages, désormais tracés et orientés vers l'extérieur dans toutes les langues.
- **C11** : pied en pastilles `KeyHints`, libellés de groupe horizontaux. `VLabel` (POCHES, STOCKAGE) reste empilé ; il passe seulement au plancher de 7 pt.
- **C12** : colonne d'aide à droite sans défilement automatique. Elle n'est pas faite, car le banc v4.10 vérifie que l'aide est sous les lignes (`LastRowsBottom <= LastHintZoneTop`). Il faudrait adapter ce test en même temps.
- **K2** : `Ui()` reste plafonnée à 820 lignes virtuelles.
- Écrans hors inventaire encore sur les anciennes primitives (menu, pause, mort, note, document de mission, LiDAR) : seul le plancher de 7 et 8 pt s'y applique.
- `BRKeys::KeyName` (`BRKeys.cpp`, hors périmètre) met encore les noms de touches en capitales par `FString::ToUpper`, ASCII seulement.
- Banc : le relevé compte toujours les textes « au plancher de 6 points » (`<= 6,01`). Il en trouvera 0 ; un compteur à 7 et 8 pt serait plus utile.

### 9.6 Points à brancher côté contrôleur (`BRPlayerController.cpp`, `BRPadInput`)

Fonctions publiques ajoutées à `BRHUD.h` :

```cpp
bool  PadScrollFocus(int32 Step);   // croix gauche / droite : colonne suivante (+1) ou precedente (-1) ; 0 : toutes les zones.
                                    // false dans PERSONNAGE ou un onglet a moins de 2 zones : garder PadDirection
bool  PadScrollLines(int32 Lines);  // croix haut / bas : Lines x PadScrollLine (26 px a 1080 lignes) ; false dans PERSONNAGE
void  PadScrollPixels(float Pixels);// stick droit : defilement continu (pixels a 1080 lignes), tous onglets
static constexpr float PadScrollLine = 26.f;
int32 GetPadScrollZone() const;  int32 GetScrollZoneCount() const;  bool HadScrollRoom() const;   // banc
```

1. **Croix gauche et droite** (lignes 1609 et 1629, `BRPadInput::Direction(..., ±1, 0)`) : appeler d'abord `H->PadScrollFocus(DX)`, et `H->PadDirection(DX, 0)` seulement si elle renvoie false. Dans CARNET et JOURNAL, la croix choisit alors la colonne ; LB et RB restent les onglets. **Banc à adapter** : `BRAutoTestV416Visual`, « croix droite : onglet suivant (JOURNAL) » depuis CARNET (passer par RB).
2. **Croix haut et bas** (lignes 1648 et 1663) : `if (!H->PadScrollLines(DY)) H->PadDirection(0, DY);`. Répétition : 8 pas par seconde après 0,35 s d'appui ; la liaison actuelle est en `ETriggerEvent::Started`, il faudrait un minuteur ou `Triggered`. Avant, un appui faisait défiler de 60 px toutes les zones.
3. **Stick droit vertical** (`OnLookPad`, inventaire ouvert) : `H->PadScrollPixels(V.Y * 900.f * Dt)`, environ 900 px/s à 1080 lignes, au lieu de tourner la vue.
4. **Pied** : une fois la colonne branchée, ajouter « [◄] [►] COLONNE » au pied de CARNET et de JOURNAL. Cela demande une clé courte, `HUD.PadColumn`, non créée ici.
5. **Banc** : dans un cas long (carnet du Level 6 ou 11, aide de niveau 2, 1280 × 720, taille 1,25), vérifier que `LastNotebookLeftRoom` baisse de 26 par appui de `PadScrollLines(1)`. Fermer l'inventaire à l'étape **suivante** de la capture (K14).

### 9.7 Risques à vérifier en jeu

- **Débordements** : relancer le banc visuel (`-BRAutoTestV416Visual`, `-BRAutoTestV417Visual`) aux trois définitions. Attendu : 0 texte hors de l'image, 0 hors du panneau, 0 au-delà de la largeur permise et 0 chevauchement. Zones à regarder en premier :
  - TOUCHES à 720 lignes avec la taille d'interface à 1,25 : lignes de 16 px ; le nom de touche est réduit pour tenir dans sa case ;
  - colonne gauche du JOURNAL (texte fixe plus haut avant la zone des notes) ;
  - INSPECTER en allemand.
- **Langues** : chinois, japonais, coréen, arabe et persan au plancher de 8 pt, non mesurés (polices de secours). Vérifier :
  - les noms en case en japonais (2 lignes) ;
  - les pastilles de code de la page Langue (« ZH-HANT » à 7 pt gras dans 70U).
- **Arabe et persan** : mise en page toujours de gauche à droite (C9) ; seuls les chevrons des réglages changent.
- **Casse** :
  - objectifs et étapes en casse de phrase à côté de « FILMER UNE ENTITÉ » (libellé de 3 mots, laissé en capitales) ;
  - `FText::ToUpper` suit-il la culture courante, par exemple le I pointé turc ?
- **Silhouette** : contraste de 2,2:1 calculé, non mesuré en jeu. À mesurer sur une vraie fenêtre (1366 × 768 et 1080p, Level 0 et Level 1) après réimport de la texture.
- **Coût** : `DenseScript` lit la culture à chaque police demandée, soit une copie de `TSharedRef` ; environ 3 000 appels par image d'inventaire, négligeable mais non mesuré.
