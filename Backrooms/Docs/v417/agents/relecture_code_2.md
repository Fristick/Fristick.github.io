# Deuxième relecture du code v4.17 (lots après `93c7fb6`)

| | |
|---|---|
| **Rôle** | « Code Reviewer », définition `~/.claude/agents/engineering-code-reviewer.md` lue en entier. Sous-agent lancé par l'orchestrateur. |
| **Périmètre** | Lots non relus depuis la première relecture (`relecture_code_jeu.md`, tête `93c7fb6`) : animation (`f14dbf7`, `14b2a4c`, `f662494`, `1f3bc1e`, fusion `c8004c6`), combinaison (`d2049ec`, `ebe559a`, `bc79f2c`, `347adac`), corrections de l'orchestrateur (`3bb039b`, `ee3365e`, `2074046`, `88b9ff0`, `3fa7ef5`), crochets de test (`de3645c`, `465ad85`, `57f210b`, `2f9e1b8`, `c1c744e`, `e612818`, `516ad17`, `711b02d`, `a632a89`). **Tête relue : `21bf02b`** (branche `backrooms-v4.17`). Les numéros de ligne renvoient à cette tête. |
| **Méthode** | Diffs lus commit par commit, puis fonctions touchées lues en entier dans la tête. API vérifiées dans `UE_5.8/Engine/Source`. Scripts de contrôle sans compilation (dossier temporaire, hors dépôt) : noms des espaces anonymes de tout le module (unity build), variables locales ajoutées portant le nom d'un membre (C4458), clés `NSLOCTEXT` contre les 22 `Game.po`. `Tools/Tests/test_material_sync.py` exécuté (contrôle statique, lecture seule). Journaux de compilation déjà présents relus (UBT, AutomationTool). |
| **Non fait** | Aucune compilation, aucun lancement d'Unreal, de Blender ni des bancs, à la demande de l'orchestrateur. `derive_hazmat_maps.py --check` non relancé (calcul lourd, machine occupée). Les points « à vérifier en jeu » portent sur l'image ou le ressenti. |

## 1. Résumé

Ces lots sont soignés. Le code d'animation garde toutes ses divisions, et l'état local du corps est rangé à part avec un pointeur faible, ce qui protège contre la réutilisation d'adresse. Les crochets de test sont en lecture seule ou neutres hors des bancs. Le code reste compilable : le journal UBT de 11:44 (BackroomsEditor Win64 Development, module complet) et le BuildCookRun Shipping de 14:18 se terminent tous deux par un succès.

Points sains, vérifiés dans le code :
- **Réseau** : rien de nouveau n'est répliqué et aucune décision de l'hôte ne change.
- **Lumière de jeu** : intacte. Les nouveaux réglages ne touchent que les composants et le post-traitement.
- **Traductions** : complètes. Les 1 222 clés sont dans les 22 langues, avec un msgid identique au texte source.
- **Matériaux** : les graphes C++ et Python n'ont pas bougé, et `test_material_sync.py` passe (9/9).
- **Première relecture** : B1, C1 et C2 sont corrigés.

**Aucun bloquant.** Il y a **trois points à corriger**, tous dans le lot accessibilité (`88b9ff0`) :
1. Entrée agit sur un curseur que le joueur à la souris ne voit pas. Elle peut utiliser un objet ou changer un réglage sans qu'il le voie.
2. L'inventaire au clavier seul reste incomplet : impossible de changer d'onglet ou de catégorie de PARAMÈTRES (LUMINOSITÉ est hors d'atteinte), et Échap ne fait pas « retour ».
3. Au curseur clavier, l'infobulle de PERSONNAGE nomme les boutons de la manette (« [A] UTILISER », « [Y] DÉPLACER »).

## 2. Constats

Gravités : **bloquant** (empêche une livraison correcte), **à corriger** (défaut réel, contournable), **remarque** (amélioration ou risque à surveiller).

### 2.1 Bloquant

Aucun.

### 2.2 À corriger

**C1. Entrée agit sur un curseur caché (joueur à la souris)** (certain à la lecture du code)

- **Où** :
  - `BRPlayerController.cpp:1763` : `IsInputKeyDown(EKeys::Enter)` lance `PadSettingsActivate` ;
  - `BRPlayerController.cpp:1774-1775` : `SlotAction(..., EKeys::Enter, false)` ;
  - `BRHUD.cpp:4646-4703` (`PadSlotAction`) et `:4850-4872` (`PadSettingsActivate`).
- **Constat** :
  - Avant `88b9ff0`, seul A de la manette menait là. Or appuyer sur A met `bPadActive` à vrai (`BRPlayerController.cpp:906`), donc le curseur était toujours affiché quand A agissait.
  - Entrée, elle, ne montre rien : `UsesPadCursor` (`BRHUD.cpp:4580`) et `bPadRows` (`:5828`) restent faux tant que les flèches n'ont pas servi. Aucune des deux fonctions ne vérifie que le curseur est visible.
  - **PERSONNAGE** : un premier appui choisit la première case du sac sans la montrer (`PadDirection(0, 0)`, `:4675`). Le second l'utilise ou l'équipe (`C->UseSlot`, `:4700`). Si une inspection est ouverte au clic droit, un seul appui utilise l'objet (`:4659-4670`).
  - **PARAMÈTRES** : un premier appui choisit la première ligne de la catégorie sans la montrer, le second la règle (+1). `SettingsPadRow` n'est jamais remis à zéro à la fermeture (`BRHUD.h:414`). Après un usage antérieur des flèches ou de la manette, un seul appui suffit donc à changer la ligne retenue.
  - OnFlash (`:1331`) et OnInteract (`:1364`), eux, exigent bien `BRKeys::UsesKeyboardCursor()`.
- **Pourquoi c'est important** : une ressource de jeu (eau, bandage) peut être consommée, ou un réglage changé, sans que le joueur le voie.
- **Correction proposée** : traiter Entrée comme les flèches. Un appui sans curseur clavier le montre seulement ; il ne fait rien d'autre.

  ```cpp
  // OnMenuConfirm, avant les deux branches de l'inventaire
  const bool bInvInput = bInventory && !IsModalDialogOpen() && CaptureAction == INDEX_NONE;
  const bool bEnter = bInvInput && IsInputKeyDown(EKeys::Enter);
  if (bEnter && !BRKeys::UsesKeyboardCursor() && !IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom))
  {
      BRKeys::SetKeyboardCursor(true); // premier appui : le curseur se montre, rien n'est utilise ni regle
      return;
  }
  ```

  Garder ensuite `IsInputKeyDown(EKeys::Enter)` aux lignes 1763 et 1775.

**C2. Inventaire au clavier seul : onglets, catégories de PARAMÈTRES et « retour » inaccessibles** (certain à la lecture du code)

- **Où** :
  - `BRPadInput::Inventory` (`BRPlayerController.cpp:774-795`) n'accepte que des touches de manette : B (`:1312`), Y (`:1342`), LB (`:1395`), RB (`:1411`) ;
  - `PadSettingsTrigger` (`:1570-1579`) : LT / RT seulement ;
  - dans PARAMÈTRES, `PadSettingsAdjust` / `PadSettingsMove` répondent toujours vrai (`BRHUD.cpp:4803-4848`) ;
  - dans PERSONNAGE, `PadDirection` déplace le curseur des cases (`:4583-4644`) ;
  - `OnPause` : Échap ferme tout l'inventaire (`:1652-1655`) et n'appelle pas `PadBack`.
- **Constat** :
  - En jeu, l'inventaire s'ouvre sur PERSONNAGE (`:1524`), et les flèches n'en sortent jamais.
  - Depuis la pause, on arrive sur PARAMÈTRES ou TOUCHES (`:1825`, `:1828`). Depuis TOUCHES, gauche / droite changent d'onglet, mais seulement vers PARAMÈTRES ou PERSONNAGE, où l'on reste bloqué.
  - CARNET et JOURNAL sont donc hors d'atteinte au clavier.
  - Dans PARAMÈTRES, la catégorie reste JEU (`SettingsCategory = 0`, `BRHUD.h:488`). VIDÉO (dont `Row_Brightness`), INTERFACE et GRAPHISMES sont inaccessibles.
  - Dans TOUCHES, Entrée ne lance pas de réaffectation.
  - Une inspection ou un objet porté ne s'annulent pas seuls : Échap ferme tout, alors que l'audit demandait « Échap = B » (`accessibilite.md`, S1, correction).
  - La vérification prévue par l'audit (« LUMINOSITÉ ±1 » au clavier seul) ne peut pas réussir.
  - Pendant une inspection, le pied du clavier (`BRHUD.cpp:4515-4519`) annonce encore des touches sans effet : flèches et LAMPE.
- **Correction proposée** :
  - **Onglets** : `PageUp` / `PageDown` mappées sur les actions de LB / RB (`DrinkAction`, `ReloadAction`), ou sur deux nouvelles actions avec `bTriggerWhenPaused`. Les accepter dans `BRPadInput::Inventory`.
  - **Catégories** : `Maj + PageUp/PageDown` dans PARAMÈTRES, via `PadSettingsCategory`. `Fin` est déjà prise par `QuitAction`.
  - **Échap** : `if (bInventory) { if (H && H->PadBack()) return; SetInventoryOpen(false); }`.
  - **Pieds du clavier** : en ajouter pour PARAMÈTRES et CARNET / JOURNAL, sans les touches inactives pendant une inspection. Ce sont de nouvelles clés, à faire passer par `loc_extract.py`, `loc_fill.py` puis `loc_build.py`.

**C3. Au curseur clavier, l'infobulle de PERSONNAGE nomme les boutons de la manette** (certain à la lecture du code)

- **Où** : `BRHUD.cpp:5457-5479`.
- **Constat** :
  - `PadBox = UsesPadCursor(C) && HoverSlot == PadCursor`. Or `UsesPadCursor` couvre maintenant le clavier (`:4580`).
  - Les aides sont pourtant construites avec `GamepadName(Gamepad_FaceButton_Bottom)` (« [A] UTILISER / ÉQUIPER / RETIRER ») et `GamepadName(Gamepad_FaceButton_Top)` (« [Y] DÉPLACER »).
  - Un joueur au clavier lit donc des boutons de manette à côté de la case, alors que le pied dit « [ENTRÉE] » et la touche LAMPE.
- **Correction proposée** : `const bool bKeys = BRKeys::UsesKeyboardCursor();`, puis :
  - `PadUse = TEXT("[") + (bKeys ? BRKeys::KeyName(EKeys::Enter) : BRKeys::GamepadName(EKeys::Gamepad_FaceButton_Bottom)) + TEXT("]  ")` ;
  - pour `HUD.PadCarryHint`, l'argument `Carry = bKeys ? BRKeys::Primary(EBRAction::Flashlight) : BRKeys::GamepadName(EKeys::Gamepad_FaceButton_Top)`.

  Aucun nouveau texte à traduire.

### 2.3 Remarques

**R1. La souris n'efface pas le curseur clavier (ni celui de la manette) dans l'inventaire** (certain à la lecture du code ; à vérifier en jeu)

- **Où** :
  - `BRPlayerController.cpp:909-919` : `GetInputMouseDelta` remet `SetKeyboardCursor(false)` ;
  - `:1205-1208` : `FInputModeGameAndUI` ;
  - moteur : `SceneViewport.cpp:923-933`, `PlayerController.cpp:6412`.
- **Constat** :
  - En mode jeu et interface, la capture n'a lieu que bouton enfoncé (`CaptureDuringMouseDown`). Le moteur n'accumule le déplacement de la souris que pendant une capture. Survoler ou cliquer sans glisser ne remet donc rien à zéro. Seul un glisser-déposer le fait.
  - Après un appui sur une flèche, le curseur jaune et le pied du clavier restent affichés. Le survol est aussi remplacé par le curseur (`BRHUD.cpp:5994`), et l'aide de PARAMÈTRES suit la ligne du curseur, pas celle survolée (`:5912`). Cela vaut aussi au menu titre.
  - Le même mécanisme existait pour la manette depuis la v4.16.
  - Autre cas : une flèche pressée après la manette laisse les invites de la manette (`UsesKeyboardCursor` est faux tant que `GPadPrompts` est vrai). INTERAGIR et LAMPE ne répondent alors pas au clavier.
- **Proposition** :
  - dans `ABRHUD`, comparer la position du curseur (`GetMousePosition`) à celle de l'image précédente. Au-delà de 2 px : `SetKeyboardCursor(false)` et `bPadActive = false` ;
  - dans `BRPadInput::Direction`, mettre `bPadActive = false` quand c'est une flèche qui a servi.

**R2. Piétinement des marcheurs chez un client** (à vérifier en jeu, client)

- **Où** : `BREntity.cpp:3560-3565` (`YawRate` d'une image à l'autre) et `:3590-3598`.
- **Constat** :
  - Le Visual est attaché à la capsule (`:208-209`), sans lissage réseau. Chez un client, la rotation répliquée arrive par paliers, à la fréquence réseau.
  - `YawRate` vaut alors des pics sur les images de mise à jour et 0 entre elles. Le seuil de 25°/s et `OmegaTurn` donnent un piétinement irrégulier.
  - L'hôte et le solo ne sont pas touchés.
- **Proposition** : lisser `YawRate`, par exemple `FInterpTo(YawRateSmoothed, YawRate, Dt, 6)`, ou le mesurer sur 0,2 s. Le lissage réseau du Visual reste à faire (`animation.md` § B.6, R.5).

**R3. Pas sonores du joueur décalés de la nouvelle cadence** (connu, `animation.md` R.5)

- **Constat** : le corps fait environ 2,8 pas/s à la marche. Les sons restent sur `BobTime` (2,55 pas/s, `UpdateCamera`) et sur une minuterie de 0,46 s pour les coéquipiers (`TickRemote`).
- **Proposition** :
  - dans `UpdateCamera`, prendre la même cadence `Ω` que `AnimateBody` ;
  - dans `TickRemote`, jouer le pas aux demi-cycles de `BodyAnim`.

**R4. `ABRWorld::bTestFreeze` : statique de processus, remis à faux seulement au bilan du banc** (certain ; sans effet sur le paquet)

- **Où** :
  - `BRWorld.h:814` ;
  - `BRAutoTestV417Visual.cpp:462` (vrai) et `:1501` (faux).
- **Constat** :
  - Le plan est refusé par une version publiée (`BRAutoTest.cpp:2105-2117`), et les étapes continuent après un échec : le bilan passe normalement.
  - En revanche, un banc interrompu avec `-BRAutoTestStay`, ou lancé dans un processus d'éditeur réutilisé, laisse la suite du processus sans rencontres, phénomènes, rondes ni coupures tirées.
- **Proposition** : remettre le drapeau à faux aussi dans `ABRAutoTest::EndPlay` / `Finish`. On peut en plus écrire un avertissement dans le journal si `ABRWorld` démarre avec le drapeau vrai sans `-BRAutoTest`.

**R5. Icônes : version des matériaux 11 et contour non gardé** (certain)

- **Où** :
  - `backrooms_setup.py:57-58`, `:1988` ;
  - `Tools/outline_icons.py` (docstring : « à relancer après generate_models.py »).
- **Constat** :
  - Passer `MATERIAL_VERSION` à 11 pour réimporter cinq icônes reconstruit aussi tous les matériaux maîtres. C'est correct, mais plus lourd que nécessaire.
  - Rien n'appelle `outline_icons.py` ni son `--check`. Un nouveau rendu des icônes (`generate_models.py`) enlèverait le contour sans aucun signal. Les métadonnées d'origine des PNG (eXIf, pHYs, tEXt de Blender) sont aussi perdues.
- **Proposition** : ajouter `python Tools/outline_icons.py --check` aux contrôles (`check_package.py`, ou un test de `Tools/Tests`). Dire dans le commentaire de version 11 que tous les maîtres sont reconstruits.

**R6. Outils de la combinaison : robustesse de détail** (certain)

- **`derive_hazmat_maps.py --check`** (`:770-787`) compare les pixels exacts. Le résultat dépend des versions de numpy et Pillow (décodage JPEG, réduction LANCZOS) ; celles du calcul livré, numpy 2.5.3 et Pillow 12.3.0, sont écrites dans le JSON (`:739`). Après une simple mise à jour d'une bibliothèque, le message « relancer le script » pousserait à changer des cartes livrées. **Proposition** : comparer d'abord les versions et le dire.
- **`write_previews`** (`:681`) : `os.path.commonpath` lève `ValueError` si le dossier des aperçus est sur un autre lecteur.
- **`export_hazmat_uv.py:219-222`** continue sans entrée au registre protégé, alors que `derive_hazmat_maps.py:300-302` s'arrête. Il faut aligner les deux, par un arrêt.
- **`validate()`** (`backrooms_setup.py:1508-1510`) saute encore une carte dérivée dont la source manque (proposition B1 de la première relecture). Le risque est faible depuis que les cartes sont validées et que `TestGear` les contrôle.

**R7. Noms masqués dans des lambdas des bancs** (certain ; compilé sans erreur)

- **Où** :
  - `BRAutoTestV416Visual.cpp:1827` : locale `Plan`, comme le membre `ABRAutoTest::Plan` (`BRAutoTest.h:364`) ; c'est le motif corrigé par `e612818` dans le carnet long ;
  - `BRAutoTestV417Visual.cpp:991` : paramètre `Foot` de `BoneAt`, alors qu'une locale `Foot` existe en `:968`.
- **Constat** : le compilateur les accepte (journal de 11:44).
- **Proposition** : les renommer (`MPlan`, `FootBone`), pour la lisibilité seulement.

## 3. Vérifications positives

**Compilation**
- Journal UBT du 21/10 à 11:44 (BackroomsEditor Win64 Development, état de `2074046`) :
  - compilés : Module.Backrooms.1 à 8, plus BRChunk.cpp et BRLevels.cpp (sortis du lot unity par la compilation adaptative) ;
  - « Result: Succeeded ».
- AutomationTool du 21/10 à 14:18 : `BuildCookRun -clientconfig=Shipping`, cible Backrooms Win64 Shipping à jour, « BUILD SUCCESSFUL ».
- API vérifiées dans le moteur 5.8 :
  - `USpotLightComponent::SetInner/OuterConeAngle` (`SpotLightComponent.h:30-33`) ;
  - `SetSourceRadius/Length` ; `SetBarnDoorAngle/Length` ;
  - `APlayerController::IsInputKeyDown` (`PlayerController.h:1594`) ;
  - `FText::ToUpper` (`Text.h:640`).
- Contrôle des noms en unity build : aucun nouveau nom d'espace anonyme en double dans le module (`FBRBodyPoseState`, `BRBodyPoseStates`, `BRBodyPoseStateOf`, `InkFaint`, `YellowFaint`, `GKeyCursor`, `V48Scalar`, `V48TextureName` sont uniques).
- Contrôle C4458 : aucune variable locale ajoutée dans une méthode ne porte le nom d'un membre de sa classe ou de ses bases du moteur.

**Réseau**
- Aucun `Replicated`, aucun `DOREPLIFETIME` ni aucune RPC ajoutés depuis `93c7fb6`. Le protocole 416 est inchangé (`2a13965`).
- L'animation est locale. Le modèle et ses pièces sont sans collision (`BRRig.cpp:197`, `:256`) : son décalage, borné à 40 ou 60 cm et remis à zéro au-delà de 200 cm, ne touche ni les traces ni le jeu.
- Le jumpscare part de la transformation du Visual (`BRJumpscare.cpp:105`) et la remplace ensuite (`BREntity.cpp:1146`).
- `SetTestWalk` n'agit que dans `Think` (côté hôte). Il n'est appelé que par le banc. `bTestWalk` vaut faux par défaut.
- La caméra du coéquipier (`57f210b`) est un `ACameraActor` non répliqué.

**Lumière de jeu**
- `BRWorldLight.cpp` et `BRLightLogic.h` sont inchangés.
- `LightBarnAngle/Length`, `DiffuserGlow`, `LocalHighlight/ShadowContrast` et `bPerfSpotLights` ne sont lus que par `CreateLightComponent`, `AddLight` et `ApplyEnvironment` : composants et post-traitement seulement.
- Le choix projecteur ou ponctuelle suit `bAreaLights`, comme le choix rectangulaire ou ponctuelle avant.
- Les valeurs par défaut reproduisent les anciennes constantes (88° / 4 cm, 1, 0,8 / 0,9) : les autres niveaux ne changent pas.
- Calcul du projecteur vérifié contre `SpotLightComponent.cpp:95-98` (cd = lm / 2π(1 − cos θ)) : 420 cd au nadir, comme l'annonce le commentaire.
- Le bourdonnement (`GatherHumSources`) travaille en rapport d'intensité : le nouveau `R.Lumens` ne le change pas.

**Crochets de test**
- `bTestFreeze` vaut faux par défaut. Il n'est mis à vrai que par le banc v4.17, et seulement dans les plans autonomes (`BRAutoTest.cpp:416`).
- `GetSupportPoints`, `GetVisualLag`, `GetVisualYaw`, `GetSupportRun`, `IsHopping` et `BRRig::GetBodyPoseInfo` sont en lecture seule (`const`).
- `-BRCommit=` vide est maintenant écarté (`465ad85`).

**Traductions**
- 1 222 clés `BR` dans les sources, et `BRLocKeys.inl` est à jour (1 222).
- Les 22 cultures contiennent toutes les clés, avec un msgid identique au texte source et aucun msgstr vide.
- `HUD.KeyCharacterFooter` et `HUD.PadColumn` sont présents dans les 22 `.po` et les 22 `.locres`.

**Matériaux**
- `BRMaterialBuilder.cpp` et les graphes de `backrooms_setup.py` sont inchangés depuis `93c7fb6` (seuls `MATERIAL_VERSION` et `REICONED` bougent).
- `test_material_sync.py` : 9/9. Repères et paramètres de `M_BR_Gear` présents dans les deux graphes ; cartes livrées conformes à leur provenance.
- Le test v4.8 (`347adac`) contrôle, slot par slot, le parent, `NormalTex`, `MaskTex`, `RoughMapWeight` et l'absence de métal.

**Animation (qualité)**
- Toutes les divisions sont gardées : `HopDuration` ≥ 0,05 ; `LegH` ≥ 20 ; `LegL` ≥ 30 ; `SinA` ≥ 0,05 ; argument d'`Asin` ≤ 0,766.
- `PoseSkin` pose les os du parent vers l'enfant. La décomposition du buste (lacet × tangage × roulis) est cohérente.
- Le corps est placé avant la pose : l'objet tenu n'a plus d'image de retard.

**Première relecture**
- B1 : cartes validées (`ebe559a`) et gardées par `TestGear`.
- C1 : catalogues complets (`eb3cc8f`, contrôle ci-dessus).
- C2 : `IA_LookPad`, `IA_LidarPulse` et `IA_LidarFocus` actives pendant la pause (`BRPlayerController.cpp:478`, `:506-507`).

## 4. À vérifier en jeu

1. **Souris** : Entrée dans PERSONNAGE et dans PARAMÈTRES (C1, avant et après correction). Survol après un appui sur une flèche (R1).
2. **Clavier seul**, le banc prévu par l'audit : onglets, catégories, LUMINOSITÉ ±1, inspection et objet porté (C2), infobulle (C3).
3. **Animation** :
   - sens de l'IK horizontale des marcheurs (pattes arrière du Hound) ;
   - bras de la Bacteria repliés dans une porte ; hauteur des bonds ;
   - piétinement vu par un client (R2) ;
   - pas sonores du joueur (R3).
4. **Éclairage en Performance** : Level 0 et Poolrooms en projecteur, contre la Qualité ; Niveau 1 revenu à la lumière ponctuelle de la v4.16.

## 5. Résumé (12 lignes au plus)

1. Tête relue : `21bf02b`. Je n'ai rien compilé ni lancé ; les journaux montrent que l'éditeur (11:44) et le paquet Shipping (14:18) compilent.
2. Aucun bloquant, et aucune erreur de compilation : pas de nom masqué nouveau, pas de collision en unity build.
3. Réseau : rien de nouveau n'est répliqué. Animation locale, modèle sans collision ; marche imposée seulement côté hôte et par le banc.
4. Lumière de jeu intacte : les nouveaux réglages (projecteur, volets, diffuseurs, exposition) ne touchent que l'image.
5. Traductions complètes : 1 222 clés dans les 22 langues ; les 2 nouvelles clés sont dans les `.po` et les `.locres`.
6. Matériaux : graphes C++ et Python inchangés, `test_material_sync.py` 9/9 ; B1, C1 et C2 de la première relecture sont corrigés.
7. **C1** : Entrée agit sur un curseur caché (souris) ; elle peut utiliser un objet ou changer un réglage sans qu'on le voie.
8. **C2** : au clavier seul, ni onglet ni catégorie de PARAMÈTRES (LUMINOSITÉ hors d'atteinte), et Échap ne fait pas « retour ».
9. **C3** : au curseur clavier, l'infobulle de PERSONNAGE affiche les boutons de la manette.
10. Remarques : la souris n'efface pas le curseur clavier ; piétinement irrégulier chez un client ; pas sonores du joueur décalés.
11. Remarques : `bTestFreeze` n'est remis à faux qu'au bilan ; contour des icônes non contrôlé ; `--check` des cartes sensible aux versions.
12. Après C1 à C3 : banc clavier seul de l'audit, puis contrôle de l'animation (client compris) et de l'éclairage en Performance.
