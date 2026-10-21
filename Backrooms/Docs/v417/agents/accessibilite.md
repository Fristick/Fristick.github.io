# v4.17 : audit d'accessibilité de l'interface (agent « Accessibility Auditor »)

| | |
|---|---|
| **Rôle** | « Accessibility Auditor », définition installée `C:\Users\Fristick\.claude\agents\testing-accessibility-auditor.md`, lue en entier. Elle est appliquée à une interface de jeu dessinée par le Canvas d'Unreal (`ABRHUD`) : il n'y a ni DOM, ni arbre d'accessibilité, ni lecteur d'écran possible. Les critères WCAG 2.2 servent de repère (contraste 1.4.3 et 1.4.11, focus 2.4.7 et 2.4.13, clavier 2.1.1, mouvement 2.2.2, couleur 1.4.1). Lancé comme sous-agent générique par l'orchestrateur. |
| **Mission** | Vérifier les critères d'acceptation C1 à C5, C8, C10 et C12 (partiel), ainsi que C6 et C7 à la manette, de `Docs/v417/agents/interface.md` (§ 6 et § 9), dans au moins fr, de, ru, ar et ja. Signaler les régressions et ce qui reste non conforme ou non vérifiable. Unreal et Blender n'ont pas été lancés. Ce rapport est le seul fichier écrit dans le dépôt. |
| **Base** | Branche `backrooms-v4.17`, tête `eb3cc8f`. Banc après : `Saved/TestRuns/autotest/v417_apres_eb3cc8f/v416vis/` et son `Rapport.txt`, du 2026.10.21, état « incomplet », 0 problème, 8 NON VÉRIFIÉ. Banc avant : `Saved/TestRuns/autotest/final416/v416vis/`. En jeu : `Saved/TestRuns/v417/{avant,apres}/Qualite/`, de `711b02d` à `eb3cc8f`, en 1366 × 768. |
| **Images vues** (outil de lecture, puis recadrages ×2 à ×5) | `v416_hud_1280x720_{fr,de,ru,ar,ja}_personnage`, `…_{fr,ja,ar,ru}_carnet`, `…_{fr,de}_touches`, `…_de_{parametres,journal}`, `…_fr_pause`, `v416_hud_1920x1080_ar_parametres`, `v416_hud_2560x1080_de_personnage`, `v416_manette_{equipe,inspecter,carnet_aide,1920x1080_fr_personnage}`, `v417_manette_{port,parametres}`, `v416_fenetre_{petite_1280x720,ultralarge_max}_carnet`, `v417_l0_inventaire_personnage_{fr,ar}` (avant et après), planches `v417_l0_hud_jeu_avant_apres.jpg` et `v417_l0_inventaire_personnage_de_avant_apres.jpg`. Avant et après sont comparés aux mêmes noms. |
| **Mesures** | Scripts Python/Pillow jetables dans le dossier temporaire de la session (`scratchpad/a11y/`), hors dépôt. Méthode au § 2. |

Conventions : **[mes.]** = mesuré sur la capture citée (zone en pixels `x0,y0–x1,y1` de cette capture) ; **[banc]** = relevé de `Rapport.txt` ; **[code]** = constat dans le code (fichier:ligne à `eb3cc8f`) ; **[hyp.]** = hypothèse. Sévérité : Critique, Sérieux, Moyen, Mineur.

---

## 1. Synthèse

**Verdict global : PARTIELLEMENT CONFORME.** Aucun blocage. 1 constat sérieux, 6 moyens, 7 mineurs (§ 4).

1. **Le défaut n° 1 de v4.16 est corrigé.**
   - À 1280 × 720, les capitales passent de 5 à 9 px dans les onglets, de 5 à 8 px dans les objectifs, de 5 à 7 px dans les noms en case et le pied. Les glyphes japonais passent de 8 à 11 px.
   - Banc : 0 texte au plancher de 6 points (5 163 avant), plus petit cadratin 9,3 px (7 pt). Aucun texte hors de l'image, hors du panneau ou au-delà de sa largeur, aucun chevauchement, sur 17 749 textes et 22 langues.
2. **Les bords d'un pixel sont stables** : 24 cases sur 24, 4 cases d'équipement sur 4, 69 cases de TOUCHES sur 69 et 6 séparateurs sur 6, en fr, de, ru, ar et ja. Avant : 6 cases fermées sur 10 détectées, 16 sur 57 dans TOUCHES, 4 séparateurs sur 6.
3. **Silhouette** : contraste moyen avec le panneau de 2,13 à 2,16:1 (avant 1,12 à 1,15:1) et étendue p5–p95 de 63 niveaux (avant 5 à 6), en jeu au Level 0 comme au Level 1. CEINTURE ne touche plus TORSE.
4. **INSPECTER et l'infobulle sont opaques** : 0 niveau d'écart au-dessus des cadres cachés (avant jusqu'à 11,5 niveaux). Le focus se repère sur une capture réduite de moitié : bord focalisé contre bord normal à 3,07:1 (avant 2,39:1).
5. **Les contrastes du banc sont optimistes.**
   - Le banc dessine hors écran sur un fond uni. Les vraies fenêtres ont la VHS et le grain (`Captures.csv`) et des panneaux translucides.
   - Les textes atténués y perdent 15 à 40 % : « VIDE » 2,8:1, « Étape encore inconnue » 3,1 à 3,4:1, « AIDE · étape en cours » 3,3:1, ligne technique des réglages 3,0:1. Le seuil est de 4,5:1.
6. **Reste non conforme** :
   - icônes sombres (caméscope, frontale, lampe) à 1,6–1,8:1 : le halo de 10 % n'éclaircit le fond que de 2 niveaux ;
   - numéros de la barre rapide en jeu à 2,2–2,5:1, sans pastille, en partie cachés par l'icône ;
   - noms de touches mis en capitales en ASCII seulement (« BOTóN », « BOTãO ») ;
   - aide des réglages qui défile seule, sans pause à la manette ;
   - arabe non inversé (C9 non fait).
7. **Clavier seul (constat sérieux, hors C1 à C12)** : sans souris, on ne peut ni choisir une case, ni régler une valeur, ni déplacer un objet. Les flèches sont liées aux mêmes actions que la croix, mais l'inventaire ne réagit qu'aux touches de la manette.
8. **Régressions** :
   - **42 textes raccourcis** par des points de suspension (0 en v4.16), surtout dans TOUCHES : es-ES, pt-BR, ru, it, pl, es-419, fa, zh-Hant ;
   - le pied de la manette pendant INSPECTER annonce « [Y] DÉPLACER », qui n'agit pas pendant une inspection ;
   - dans CARNET et JOURNAL, la croix gauche ou droite choisit maintenant une colonne au lieu de changer d'onglet, et le pied ne le dit pas.

   Aucun texte coupé, aucun recouvrement, aucun focus perdu et aucune langue cassée sur les captures vues.
9. **Contraintes du propriétaire tenues** [banc] :
   - aucune barre de vie : 0 barre sur 66 rendus du HUD du jeu ;
   - endurance et santé mentale seulement dans PERSONNAGE : 66 rendus avec les deux jauges, 0 ailleurs sur 396 rendus ;
   - 22 langues sans aucun problème ;
   - aucun texte sous 7 pt (8 pt en CJK et en arabe).

---

## 2. Méthode

- **Hauteur des capitales** : masque des pixels d'au moins 40 niveaux au-dessus de la médiane locale (canal max), puis découpage en glyphes par colonnes.
  - Une ligne a pour ligne de base le bas le plus fréquent de ses glyphes.
  - Hauteur d'x : valeur fréquente basse des glyphes posés sur cette ligne. Capitales : valeur fréquente haute. L'anticrénelage compte comme pixel de texte, comme dans `interface.md`.
  - Les accents (É, Ä) ajoutent 1 à 2 px quand ils tombent dans la zone. Ils sont signalés quand c'est le cas.
  - Japonais et arabe : hauteur totale de la ligne et hauteur du glyphe le plus haut. Pour l'arabe, aussi la « bande principale » : rangées qui portent au moins 35 % de l'encre de la ligne, équivalent de la hauteur d'x.
- **Contraste WCAG** : luminance relative sRGB et rapport (L1 + 0,05)/(L2 + 0,05).
  - Texte : médiane des 15 % de pixels les plus clairs du masque. Fond : médiane hors masque.
  - Silhouette : moyenne linéaire des pixels gris (écart entre canaux ≤ 12) contre la médiane du panneau voisin.
  - Icônes : médiane des pixels de l'icône contre un anneau de 5 px autour d'elle.
- **Bords** :
  - Un bord est présent si, sur au moins 80 % de sa longueur, les pixels sont jaunâtres (R − B > 35) et à au moins 30 niveaux au-dessus du fond. Contrôle refait à +10 et +4 niveaux : les bords absents en v4.16 le restent, ils manquent vraiment.
  - Bords haut et bas : détectés au milieu de chaque colonne de cases. Bords gauche et droit : vérifiés à ±2 px.
- **Focus à demi-taille** : capture réduite de 50 % (Lanczos), puis contraste du bord focalisé contre un bord normal et contre l'intérieur de la case.
- **Opacité** : écart maximal, en niveaux du canal max, entre la ligne d'un cadre de case caché et ses voisines à ±3 px, dans une zone vide de la fenêtre.
- **Limites** :
  - PNG 8 bits : l'écran réel dépend du profil et de la luminosité choisie.
  - Les rendus `v416_hud_*` sont faits sur fond uni, sans VHS.
  - Les captures `fenetre_*` et `manette_*` (Level 1) ont la VHS et le grain. Les captures `v417_l0_*` (Level 0, le niveau le plus clair) ne les ont pas.

---

## 3. Verdict par critère

| Critère | Verdict | Mesures (avant → après) | Captures et zones |
|---|---|---|---|
| **C1** Typographie, plancher | **Conforme** | Capitales à 720p : onglets 5 → **9 px** ; objectifs 5 → **8** (fr, de, ru) ; noms en case 5 → **7** ; pied 5 → **7** ; légendes 5 → **7** ; « THRESHOLD » 5 → 7 ; « ÉCHAP » 5 → 7 (accent compris). À 1080p : onglets 9 → **12** ; objectifs 8 → **12–13** ; noms 5 → **9** ; pied 8 → **11**. Japonais à 720p : 8 → **11 px** (kanji ; kana 9–10). Arabe à 720p : hauteur des lignes 8 → 11 px. Banc : 0 texte au plancher de 6 points (5 163 avant), plus petit cadratin 9,3 px, 0 débordement. | `v416_hud_1280x720_{fr,de,ru}_personnage` : onglets `130,66–520,90`, objectifs `70,145–365,310`, case `518,155–595,170`, fin du pied `1200,668–1228,692` ; `…_ja_carnet` `60,145–600,340` ; `…_fr_touches` « ÉCHAP » `806,528–914,543` ; 1080p : zones × 1,5 |
| **C2** Lignes d'un pixel | **Conforme** | Cases à 720p : 24/24 et équipement 4/4 en fr, de, ru, ar, ja (avant : 6 fermées sur 10 détectées). En jeu en 1366 × 768 : 24/24 en fr, de, ar (avant 6/15). TOUCHES : 69/69 en fr, de, ar (avant 16/57). PARAMÈTRES > VIDÉO en de, fr, ar à 720p : 6 séparateurs sur 6 (avant 4) et 6 cases de flèche fermées sur 6 (avant 4 détectées). | `v416_hud_1280x720_*_personnage` `428,148–858,610` ; `v417_l0_inventaire_personnage_*` `457,155–915,650` ; `…_touches` `681,155–1037,605` ; `…_de_parametres` `80,170–540,420` |
| **C3** Cases | **Partiel** | Nom « МИНДАЛЬНАЯ ВОДА » sur 2 lignes, marges de 9 et 9 px à 720p ; « MANDELWASSER » 3 et 3 px. Capitales du nom à 1080p : 9 px (≥ 7). Pastille de touche 14,6 à 15,7:1. Focus à demi-taille : bord focalisé contre bord normal **3,07:1** (avant 2,39:1) ; contre l'intérieur 5,0:1. « x2 » vu seulement sur une source estompée (2,5:1, état voulu). **Icônes sombres sur leur halo : caméscope 1,71:1, frontale 1,78:1, lampe 1,64:1 (attendu ≥ 3:1 ; avant 1,5 à 2,0)**. POCHE 1 réaffectée, cadre rouge du refus, case d'équipement éclairée : non capturés. | `v416_manette_equipe` : caméscope `460,297–540,360`, frontale `985,303–1053,355`, lampe `1226,450–1294,502`, focus case 2 du sac `550,277–636,363` (réduite) ; `v416_hud_1280x720_ru_personnage` poche 1 `428,155–508,182` ; `v417_manette_port` `520,345–542,361` |
| **C4** Silhouette, équipement | **Conforme** (5 langues mesurées sur 22) | Contraste moyen avec le panneau **2,13 à 2,16:1** (avant 1,12 à 1,15) ; gris p5, p50 et p95 de 50, 102 et 114, soit une étendue de **63 niveaux** (avant 5 à 6). CEINTURE entre TORSE et LAMPE : 8 et 6 px libres en jeu (1366 × 768), 11 et 9 px à 1080p, 7 et 6 px en russe à 720p, 9 et 9 px en allemand à 2560 × 1080. Avant : la légende chevauchait le bas de TORSE à 1080p. | `v417_l0_inventaire_personnage_{fr,de,ar}` `1062,290–1218,565` ; `v416_fenetre_169_max_personnage` `1181,314–1356,629` ; `v416_manette_equipe` (Level 1) ; `v416_hud_1920x1080_fr_personnage` `1493,398–1713,795` ; légendes : colonne `1146–1216` (720p) |
| **C5** INSPECTER, infobulle | **Conforme en fr**, allemand non vérifiable | Écart sur les cadres cachés : **0 niveau** sous INSPECTER (avant jusqu'à 11,5) et **0** sous l'infobulle. Boutons : 8 et 9 rangées libres au-dessus et au-dessous des capitales, 7 et 7 crochets compris (centrés à ≤ 1 px). Hiérarchie en 1366 × 768 : nom 7 → **11 px**, description 6 → **10 px**, type 5 → 7 px. Contrastes de 5,7 à 12,5:1. | `v416_manette_inspecter` : zone vide `800,300–945,455`, boutons `591,462–795,489`, texte `589,305–945,410` ; `v416_manette_1920x1080_fr_personnage` `1080,540–1150,556` |
| **C6** PARAMÈTRES à la manette | **Partiel** | [banc] La ligne LUMINOSITÉ est atteinte, puis réglée de 0,0 à 0,1 (affiché « +0.0 » puis « +0.1 ») et revient à 0,0. Son aide est affichée. **Mais le banc appelle `PadSettingsMove` et `PadSettingsAdjust` directement**, sans passer par une vraie touche. Le branchement de la croix, de A, de LT et de RT existe ([code] BRPlayerController.cpp:811-812, 1519, 1534 et 1547-1555, 1743) mais n'est pas exercé dans PARAMÈTRES. [mes.] Focus : barre jaune de 3 px, à 8,2:1 sur une capture réduite de moitié ; fond de ligne à 1,35:1 contre les autres lignes ; libellé jaune à 8,0:1. Capture seulement en 1366 × 768 (720p et 1080p attendues). TOUCHES à la manette : non fait. | `v417_manette_parametres` : ligne `64,188–832,225`, barre `62–70` |
| **C7** Déplacer à la manette | **Partiel** | [banc] Prise, port vers la 4e poche, pose, puis retour vers le sac : contenus relus et justes. **B, vraie touche injectée, annule le port sans fermer l'inventaire.** Y passe par `PadCarry` appelé directement : la vraie touche Y n'est pas essayée dans PERSONNAGE (branchée, [code] BRPlayerController.cpp:1320-1328). L'échange poche 4 ↔ poche 1 n'est pas fait. [mes.] Pendant le port, la case visée a le focus et la source est estompée (« x2 » 2,5:1). L'icône portée déborde de 10 px au-dessus du cadre. | `v417_manette_port` : cible `730,150–830,252`, source `455,275–545,365` |
| **C8** Carnet | **Partiel** | Les trois `v416_fenetre_*_carnet` montrent le carnet (K14 corrigé ; avant, le parking sans interface). L'invite « FAIRE DÉFILER » est absente quand rien ne défile. Bouton d'aide : 7 et 8 px libres (centré). **Carnet long : NON VÉRIFIÉ** [banc] (« rien à faire défiler à 1280 × 720 ») ; croix bas NON VÉRIFIÉ ; rail et cadre de colonne jamais visibles. **Répétition de la croix absente** : [code] liaisons en `ETriggerEvent::Started`, BRPlayerController.cpp:744-748. Stick droit branché (`OnLookPad`, 1237-1250), non essayé. | `v416_fenetre_{petite_1280x720,169_max,ultralarge_max}_carnet` ; `v416_manette_carnet_aide` bouton `70,651–273,674` |
| **C10** Casse | **Partiel** | Objectifs et étapes en casse de phrase en fr, de et ru ; « FILMER UNE ENTITÉ », « EINE ENTITÄT FILMEN » et « СНЯТЬ СУЩНОСТЬ » restent en capitales de façon cohérente ; plus de « RéTABLIR » ni de « SCHéMA ». **Noms de touches** : [banc] « BOTóN CENTRAL DEL R… » (es-ES) et « BOTãO DO MEIO DO MO… » (pt-BR), qui correspondent à `\p{Lu}\p{Ll}\p{Lu}`. Cause : [code] `BRKeys::KeyName`, `FString::ToUpper`, BRKeys.cpp:208. tr, pl, cs, hu : aucune capture (même défaut attendu [hyp.]). | `v416_hud_1280x720_{fr,de,ru}_personnage` objectifs ; `Rapport.txt` lignes « raccourci : … touches » |
| **C12** (partiel) Réglages | **Partiel** | Chevrons tracés et centrés : écart **0 à 0,5 px** (avant −3,5 px à 720p et −5,5 à 1080p) en fr, de et ar. Valeurs centrées à ≤ 1 px, ou −1,5 à −2 px quand un accent (É, Ê) monte la ligne. Flèches arabes tournées vers l'extérieur. **Colonne d'aide à droite non faite** (480 px vides à 1366 × 768) ; **défilement automatique gardé** ([code] BRHUD.cpp:5906-5916). | `v416_hud_{1280x720,1920x1080}_{fr,de,ar}_parametres` cases de flèche `556–579` (720p) ; `v417_manette_parametres` |
| C9 Arabe et persan inversés | Non conforme (non fait, hors mission) | Puce à **185 px** du texte à 720p et **277 px** à 1080p (attendu ≤ 12) ; compteur « 0/2 » en tête de phrase, à 6 px du texte (avant : collé) ; titres et onglets dans l'ordre de gauche à droite. | `v416_hud_{1280x720,1920x1080}_ar_personnage` 3e objectif `60,186–365,205` (× 1,5) |
| C11 Raccourcis et libellés de groupe | Non conforme (non fait) | Pied toujours entre crochets ; « ХРАНИЛИЩЕ », « STOCKAGE » et « LAGER » toujours empilés lettre par lettre. | `v416_hud_1280x720_ru_personnage` `405,260–420,335` |

---

## 4. Constats

### S1 (Sérieux). Le clavier seul ne permet pas d'utiliser l'inventaire
- **Repère WCAG** : 2.1.1 Clavier.
- **Impact** : un joueur qui ne peut pas utiliser la souris (handicap moteur, pavé tactile, bras immobilisé) peut ouvrir l'inventaire, mais ne peut ni choisir une case, ni équiper, ni inspecter, ni déplacer un objet, ni régler un paramètre. Le pied du clavier ne propose que des gestes de souris (« [GLISSER] DÉPLACER », « [DOUBLE-CLIC] », « [CLIC DROIT] »).
- **Preuve** [code] :
  - les flèches, Z, Q, S, D et Entrée sont liées à `IA_MenuUp`, `IA_MenuDown`, `IA_MenuPrev`, `IA_MenuNext` et `IA_MenuConfirm` (BRPlayerController.cpp:650-664) ;
  - mais `BRPadInput::Direction` (802-818) ne répond que si `Gamepad_DPad_*` est enfoncée (1665, 1685, 1704, 1719) ;
  - de même, `PadSettingsActivate` attend `Gamepad_FaceButton_Bottom` (1743) ;
  - le focus n'est montré qu'avec les invites de la manette (`UsesPadCursor`, BRHUD.cpp:4551-4554).
- **Correction** :
  - accepter aussi `EKeys::Up/Down/Left/Right` dans `Direction`, inventaire ouvert ;
  - Entrée = A, Suppr ou E = X (inspecter), F ou Espace = Y (prendre / poser), Échap = B ;
  - afficher le jeton focus dès qu'une touche de navigation du clavier a servi en dernier ;
  - pied du clavier en conséquence (« [FLÈCHES] CHOISIR  [ENTRÉE] UTILISER… »), clé de texte nouvelle.
- **Vérification** : banc au clavier seul (touches injectées) : curseur, équiper, inspecter, port et LUMINOSITÉ ±1.

### M1 (Moyen). Textes atténués sous 4,5:1 dans les vraies fenêtres
- **Repère WCAG** : 1.4.3 Contraste (minimum).
- **Preuve** [mes.] :

  | Texte | Banc (fond uni) | Vraie fenêtre | Capture et zone |
  |---|---|---|---|
  | VIDE (case vide) | 4,41:1 | **2,78:1** (Level 0 : 2,76) | `v416_fenetre_petite_1280x720_personnage` `940,295–972,306` ; `v417_l0_…_fr` `1000,312–1040,330` |
  | Étape encore inconnue | 4,80 | **3,12** (720p) ; **3,39** (1366) | `v416_fenetre_petite_1280x720_carnet` `78,210–200,224` ; `v416_manette_carnet_aide` `82,225–210,239` |
  | AIDE · étape en cours | 5,15 | **3,26** ; **3,27** | mêmes captures, `66,258–160,269` ; `70,276–165,287` |
  | Ligne technique « Immédiat : … » (InkDim × 0,7) | 4,75 | **3,03** | `v417_manette_parametres` `75,668–790,680` |
  | « reposé », phrase sous les objectifs | 6,3 | 4,55 ; 4,81 (à la limite) | `v416_fenetre_petite_1280x720_personnage` `86,529–118,540` et `62,274–360,302` |

- **Cause** : la VHS et le grain, actifs par défaut dans ces captures, plus l'alpha 0,7 de la ligne technique, retirent 15 à 40 % au contraste des teintes atténuées. Ink et le jaune restent au-dessus de 10:1.
- **Correction** :
  - un jeton `InkFaint` à au moins 4,5:1 mesuré **dans la vraie fenêtre** ; par exemple InkDim à (0,80 ; 0,79 ; 0,73) au lieu de l'alpha 0,7 ;
  - « VIDE » en InkDim plein, ou en pictogramme ;
  - banc : mesurer aussi le contraste dans les `fenetre_*`, avec la VHS.

### M2 (Moyen). Icônes sombres toujours à 1,6–1,8:1 (C3 non atteint)
- **Repère WCAG** : 1.4.11 Contraste du contenu non textuel. Le nom de l'objet compense en partie.
- **Preuve** [mes.] `v416_manette_equipe` (zones au § 3) : icônes (65, 65, 67) à (69, 69, 69) sur un halo de (27, 25, 15) à (30, 28, 17). Le halo `UI_Radial` à 10 % ne gagne que 1 à 3 niveaux, avant (27, 25, 15) et après (28, 26, 16).
- **Correction** :
  - halo à 25–30 % en espace linéaire, ou fond de case éclairci à au moins (60, 55, 35) sous l'icône ;
  - ou contour clair de 1 px autour des icônes sombres (caméscope, frontale, lampe).

  Cible : ≥ 3:1 sur la même mesure.

### M3 (Moyen). Barre rapide en jeu : numéros de poche peu lisibles
- **Repère WCAG** : 1.4.3.
- **Preuve** [mes.] :
  - `v417_l0_hud_jeu` (Level 0, moquette claire) : numéros « 1 » et « 4 » à **2,16 et 2,47:1** sur la case (noir α 0,38) ;
  - fond de la case contre la moquette 1,83:1 ;
  - le « 2 » est en partie caché par l'icône des bandages.

  Le § 9.1 annonçait une pastille dans la barre rapide. [code] BRHUD.cpp:3716 dessine la touche en InkDim, sans pastille, **avant** l'icône (3720).
- **Correction** : la même pastille noire que dans l'inventaire (contraste 14,6–15,7:1), dessinée après l'icône ; fond de case α 0,55.

### M4 (Moyen, régression). 42 textes raccourcis par des points de suspension
- **Preuve** [banc] : 42 raccourcis contre 0 en v4.16. Langues : es-ES 3, pt-BR 3, ru 12, it 6, es-419 1, pl 15, fa 1, zh-Hant 1. Seuls 10 sont listés, tous dans TOUCHES :
  - « BOTóN CENTRAL DEL R… » ;
  - « BOTãO DO MEIO DO MO… » ;
  - « "1" на цифровой клави… ».
- **Impact** : l'information est tronquée (le nom de touche entier n'est lisible nulle part). Le choix est volontaire (plancher de 7 pt plutôt que réduction), mais 32 cas ne sont pas montrés et n'ont été vus sur aucune capture.
- **Correction** :
  - banc : lister les 42 ;
  - TOUCHES : nom de touche sur 2 lignes dans la case ;
  - ou abréviations propres : `GetDisplayName(true)` court, « Pavé 1 », « Пр. кн. мыши » ;
  - nom complet dans l'aide au survol ou au focus.

### M5 (Moyen). Arabe : mise en page non inversée, corps visuellement plus petit, sans gras
- **Repères WCAG** : 1.3.2 Ordre significatif ; 1.4.1 Utilisation de la couleur.
- **Preuve** [mes.] :
  - C9 non fait (§ 3) ;
  - à 720p, la bande principale de l'arabe mesure **4 px**, contre une hauteur d'x de **6 px** en latin au même jeton ; à 1080p, 5–6 px contre 9 ;
  - la police de secours n'a pas de gras : le but du niveau se distingue seulement par le jaune, alors qu'en français il est en jaune et en gras.

  Zones : `v416_hud_1280x720_ar_personnage` `73,145–362,300` ; recadrage ×3 de `50,60–380,290`.
- **Correction** :
  - C9 tel que proposé ;
  - facteur d'échelle de 1,15 à 1,2 pour l'arabe et le persan (police plus grande), en plus du plancher de 8 pt ;
  - but du niveau marqué aussi par une puce pleine ou un préfixe.

### M6 (Moyen). L'aide des réglages défile seule, sans pause à la manette
- **Repère WCAG** : 2.2.2 Mettre en pause, arrêter, masquer.
- **Preuve** [code] BRHUD.cpp:5906-5916 : au-delà de 3 lignes, défilement d'une ligne toutes les 2 s en boucle. Seul le survol de la souris (`bInHelp`) l'arrête. À la manette, `PadScrollPixels` sur la zone 0 est écrasé à chaque image par `HintScroll`. Sujet : aides longues comme PROFIL (environ 600 caractères).
- **Correction** : la colonne d'aide de C12, sans défilement automatique. À défaut, arrêter le défilement dès qu'une ligne est choisie à la manette, et laisser le stick droit faire défiler.

### m1 (Mineur, régression). Pied pendant INSPECTER
- **Repère WCAG** : 3.2.4 Identification cohérente.
- **Preuve** : [image] `v416_manette_inspecter`, pied `840,716–1310,730`, non voilé, qui affiche « [Y] DÉPLACER … [X] INSPECTER [B] FERMER » ; [code] BRHUD.cpp:4450-4464 ne tient pas compte de `Inspecting`. Pendant une inspection, `PadCarry` renvoie false (4697) et Y ouvre l'aide du carnet. B ferme l'inspection, pas l'inventaire.
- **Correction** : pendant `Inspecting`, pied « [A] ÉQUIPER/UTILISER  [B] FERMER L'INSPECTION », ou pied voilé comme le reste.

### m2 (Mineur, changement non annoncé). Colonne du carnet à la croix
La croix gauche ou droite choisit une colonne dans CARNET et JOURNAL ([code] BRPlayerController.cpp:811). Le pied de la manette n'en dit rien : « [Y] HILFE  [LB][RB] REITER  [B] SCHLIESSEN », `v416_manette_1280x720_de_carnet`. La clé `HUD.PadColumn` n'existe pas. Le cadre jaune de la colonne choisie n'apparaît sur aucune capture : 2.4.7 non vérifiable.

### m3 (Mineur). Croix sans répétition
Liaisons en `ETriggerEvent::Started` (BRPlayerController.cpp:744-748) : parcourir 23 lignes de réglages ou un long carnet demande un appui par pas. C'est pénible pour les troubles moteurs. Correction : répétition à 8 pas par seconde après 0,35 s (§ 6 de `interface.md`).

### m4 (Mineur). Aide et indices du carnet à 7 pt à 720p
- **Constat** : `BodySmall` (11 pt) donne 7 pt à 720p : capitales de 7 px, hauteur d'x de 5 px. Il sert pour l'aide, les observations et le « Facultatif ». C'est le contenu qui fait avancer le joueur.
- **Preuve** : `v416_fenetre_petite_1280x720_carnet`, lignes 237 à 289 et `640,145–1220,190`. Lisible, mais au plancher.
- **Correction** : aide et observations en `Body` (8 pt à 720p). Les zones défilent déjà.

### m5 (Mineur). Objet porté
L'icône portée déborde de 10 px au-dessus du cadre de la case visée, et la quantité portée (×2) n'est pas montrée. Capture `v417_manette_port`, `730,150–830,252`. Correction : décaler l'icône vers l'intérieur et ajouter la pastille ×N à l'icône portée.

### m6 (Mineur, déjà présent en v4.16). Messages dans la langue précédente
Après un changement de langue, les messages déjà affichés restent dans l'ancienne langue. Exemple : « TÂCHE ACCOMPLIE : FILMER UNE ENTITÉ » en français sur l'inventaire allemand et arabe (`v417_l0_inventaire_personnage_{de,ar}`, `480,20–885,47`). C'est surtout un effet du banc, qui change de langue en cours de partie. Correction : garder le `FText` et le refaire au dessin.

### m7 (Mineur). Casse des noms de touches
Voir C10 : « BOTóN », « BOTãO ». Correction : `Key.GetDisplayName(false).ToUpper()` sur le `FText`, donc par ICU, dans `BRKeys::KeyName` (BRKeys.cpp:208).

---

## 5. Ce qui marche

- L'échelle typographique tient dans les 22 langues sans aucun débordement [banc]. Elle préserve les acquis : coupure ICU, `TextFit` borné à 7 pt, mise en forme arabe.
- La hiérarchie de PERSONNAGE est rétablie : onglets (9 px) au-dessus des objectifs (8 px) et des noms (7 px) à 720p, au lieu de 5 px partout.
- Le jeton focus est le même pour la souris, la manette et PARAMÈTRES : cadre, fond jaune et coins à la manette ; barre de 3 px dans les réglages. Il se repère réduit de moitié.
- Les fenêtres modales sont opaques et leurs boutons centrés. Les chevrons sont tracés, donc insensibles au miroir bidirectionnel. Les bords tombent sur des pixels entiers.
- La silhouette a du modelé et un contraste de 2,1:1. Les légendes gardent au moins 6 px d'écart avec les cadres.
- B annule le port sans fermer l'inventaire, avec une vraie touche injectée. Le pied de PARAMÈTRES correspond à LT et RT, branchés.

---

## 6. Non vérifié ou non vérifiable

- **C9** (droite à gauche) : non fait. Persan : aucune capture.
- **Répétition de la croix** : absente, donc rien à essayer. **Stick droit** : branché, non essayé.
- **Vraie manette** : jamais essayée. Le banc injecte des touches, et pour Y, la croix, A, LT et RT dans PARAMÈTRES et PERSONNAGE, il appelle le HUD directement. Seuls B, LB, RB, et la croix dans PERSONNAGE et le carnet passent par de vraies touches.
- **Langues sans capture** : zh-Hans, zh-Hant, ko, fa, pt-BR, it, tr, pl, cs, hu.
  - Leurs 32 textes raccourcis non listés ne sont vus nulle part.
  - Les noms en case de C3 en pt-BR et it, la casse de C10 en tr, pl, cs et hu, et les légendes de C4 dans 17 langues reposent seulement sur le relevé du banc (0 débordement).
- **États non capturés** :
  - INSPECTER en allemand (description contre boutons) ;
  - POCHE 1 réaffectée à F ;
  - cadre rouge et son du refus ;
  - case d'équipement éclairée ;
  - pile non estompée « ×2 » ;
  - rail et fondu d'un carnet long ;
  - cadre de colonne du carnet ;
  - PARAMÈTRES à la manette en 1280 × 720 et en 1920 × 1080 ;
  - pastilles de code de la page Langue.
- **Port en multijoueur client** : seulement essayé en solo par le banc.
- **Hors Canvas** : aucune synthèse vocale ni aucun texte exposé à un lecteur d'écran. C'est structurel, aucun critère n'en parle. À noter pour une future option « lecture des menus ».
- Contrastes sur PNG 8 bits, profil d'écran inconnu ; réglage LUMINOSITÉ à +0,0.

---

## 7. Priorités

**Avant diffusion (Sérieux et Moyens)** :
1. S1 : navigation de l'inventaire au clavier seul.
2. M1 : jeton InkFaint à au moins 4,5:1 mesuré en vraie fenêtre, et contrôle du contraste dans le banc des fenêtres.
3. M3 : pastille des touches dans la barre rapide, dessinée après l'icône.
4. M2 : halo ou contour des icônes sombres.
5. M4 : lister et réduire les 42 textes raccourcis dans TOUCHES.
6. M6 : colonne d'aide des réglages sans défilement automatique (reste de C12).
7. M5 : C9 et facteur d'échelle pour l'arabe et le persan.

**Ensuite (Mineurs)** : m1 pied pendant INSPECTER ; m2 clé `HUD.PadColumn` ; m3 répétition de la croix ; m4 aide et observations en `Body` ; m7 `FText::ToUpper` dans `BRKeys::KeyName` ; m5 ; m6.

**Banc** :
- captures 720p et 1080p de PARAMÈTRES à la manette ;
- carnet long réellement plus haut que sa zone : Level 6 ou 11, ou plusieurs observations ;
- INSPECTER en allemand ;
- POCHE 1 réaffectée ;
- passage des vraies touches Y, croix, A, LT et RT ;
- une langue au moins dans chacun des groupes absents (zh, ko, fa, pl, tr).

---

## 8. Résumé (12 lignes)

1. Verdict global : partiellement conforme, aucun blocage. 1 constat sérieux, 6 moyens, 7 mineurs.
2. C1 conforme : à 720p, onglets 5 → 9 px, objectifs 5 → 8 px, noms en case 5 → 7 px, japonais 8 → 11 px. Banc : 0 texte sous 7 pt, 0 débordement dans 22 langues.
3. C2 conforme : 24/24 cases, 69/69 cases de TOUCHES, 6/6 séparateurs, en fr, de, ru, ar et ja (avant : bords manquants).
4. C4 conforme : silhouette à 2,13–2,16:1 (avant 1,12) avec 63 niveaux de modelé ; légendes à au moins 6 px des cadres.
5. C5 conforme en fr : INSPECTER et infobulle opaques (0 niveau d'écart), boutons centrés ; allemand non capturé.
6. C3 partiel : focus net à demi-taille (3,07:1) et noms tenus en russe, mais icônes sombres à 1,6–1,8:1 (attendu 3:1).
7. C6 et C7 partiels : le banc réussit en appelant le HUD directement ; seuls B, LB et RB passent par de vraies touches. Vraie manette jamais essayée, échange 4 ↔ 1 non fait.
8. C8 partiel : captures du carnet réparées, mais carnet long NON VÉRIFIÉ, rail jamais vu, croix sans répétition.
9. C10 partiel : objectifs en casse de phrase, mais noms de touches « BOTóN » et « BOTãO » (`FString::ToUpper`) ; C12 partiel : chevrons centrés, aide toujours défilante.
10. Sérieux : l'inventaire est inutilisable au clavier seul (flèches ignorées, `Direction` exige la croix de la manette).
11. En vraie fenêtre (VHS et grain), VIDE, « Étape encore inconnue », « AIDE · étape en cours » et la ligne technique tombent à 2,8–3,4:1 ; barre rapide à 2,2–2,5:1.
12. Régressions : 42 textes raccourcis (0 avant), pied d'INSPECTER trompeur, colonne du carnet à la croix non annoncée ; C9 (arabe) et C11 non faits.
