# Passation : passe « appuis » de la v4.17, et la suite (marcheurs)

Document pour reprendre le travail sur un autre PC. Écrit le 26 octobre 2026, à la fin de la passe « appuis ».

## 1. Où en est le code

- **Branche** : `claude/agitated-ramanujan-628db2`, poussée sur `origin` (`github.com/Fristick/Fristick.github.io`, dépôt
  **public**). Elle part de `backrooms-v4.17` au commit `132eba6` (code final de la v4.17 : `f5231c5`).
  - `backrooms-v4.17` n'avait jamais été poussée : cette branche contient donc toute la v4.17, plus la passe.
  - Rien n'est fusionné dans `backrooms-v4.17`. La branche en descend directement : une fusion en avance rapide suffit
    si `backrooms-v4.17` n'a pas bougé (`git checkout backrooms-v4.17 && git merge --ff-only claude/agitated-ramanujan-628db2`).
- **Commits de la passe** : `94cb3c3` à `d8386b2` (code et outils), `7fd8d63` (documents et preuves), puis celui de cette
  passation (ce document, `Tools/Tests/run_autotests.ps1`).
- **Absents de ce push** : les branches des deux sessions menées en parallèle sur le premier PC, restées locales.
  - `claude/eloquent-brattain-385b53` : inventaire au clavier seul ;
  - `claude/infallible-elbakyan-91f2a3` : taches sombres du Niveau 1.
  - Elles touchent d'autres fichiers ; `BRCharacter.cpp` peut toutefois se recouper avec l'inventaire au clavier.

**Résultat de la passe** (banc figé, Qualité, avant `f5231c5` contre après `d8386b2`) :

| Mesure | Critère | Avant | Après |
|---|---|---|---|
| Joueur : glissement par appui (appuis déclarés par le jeu) | ≤ 3 cm | 16 cm (os lent) | 0,0 à 0,1 cm |
| Joueur : cheville pendant l'appui | au sol | 6,6 à 19 cm de l'os au bas de la capsule | 11,7 cm au-dessus du sol réel = hauteur de repos |
| Joueur : pied en l'air sous le pied posé | jamais | 26 images sur 68 | 0 sur 88 |
| Bacteria : appuis du corps et griffes, par image / dérive | ≤ 0,5 (griffe 1) / ≤ 2 cm | jusqu'à 27 / 29 cm | 0 / 0 |
| Bacteria : lacet par bond | ≤ 25° | 59,7° | 23,2° |
| Bacteria : lacet en course, par image | ≤ 15° | 15,3° | 14,0° |

Tests sur `d8386b2` :

- `smoke`, `v415`, `chapitre`, `v48` réussis ; `v416vis` sans problème (8 non vérifiés, comme avant) ;
- test réseau à un client : hôte sans problème (4 non vérifiés, comme avant), client réussi.

Détails : `Docs/RAPPORT_v4.17.md` § 3.5, § 4.3, § 7.3 et § 8.2 ; `Docs/VISUELS_v4.17.md` § 5.1. Preuves :
`Docs/v417/mesures/animation/`, `Docs/v417/mesures/tests/*d8386b2*`, `Docs/v417/visuels/videos/*_appuis.webp`,
`Docs/v417/visuels/details/*appuis*`.

Ce que la passe a changé, en bref :

- **Pose après le déplacement** : la pose du corps du joueur (`FBRBodyLateTick`, `BRCharacter.h/.cpp`) et celle de la
  Bacteria (`FBREntityLateTick`, `BREntity.h/.cpp`) se font dans une tick fonction qui passe après le composant de
  déplacement. Avant, le `Tick` de l'acteur passait avant le déplacement, et un pied tenu au monde suivait la capsule.
  - Ces tick fonctions sont des **membres** de l'acteur, seulement désinscrits dans `EndPlay`. Les libérer dans `EndPlay`
    faisait planter le jeu : elles peuvent encore être en file pour l'image en cours.
- **Joueur** (`ABRCharacter::AnimateBody`) : chaque cheville vise un point au monde ; IK à deux segments ; bassin descendu
  pour la jambe d'appui, remontée bornée à 80 cm/s ; pied à plat pendant l'appui ; départ d'un arrêt recalé sur les pieds.
- **Bacteria** (`ABREntity::AnimateBacteriaGait`, `AimClaw`) : bond d'élan vers la course ; 25° au plus par bond ;
  griffes visées exactement (épaule relevée de 8 cm au plus).
- **Coéquipier vu par un client** : son personnage simulé reste en mode de déplacement « aucun », d'où la pose en l'air et
  les bras en A. La pose cherche maintenant le sol sous la capsule (`cec3f0b`).
- **Outils** : `Tools/Visual/v417_motion.py` (appuis déclarés par le jeu, os à sa hauteur d'appui, bassin) ;
  `Tools/check_leg_ik.py` (IK du joueur hors moteur) ; `v416vis` attend que la Bacteria soit posée.

## 2. Reprendre sur un autre PC

1. **Logiciels** : Unreal Engine 5.8 (5.8.3) par le lanceur Epic ; Visual Studio avec le C++ pour le bureau (MSVC 14.50
   sur le premier PC) ; Git ; Python 3 avec Pillow et numpy (outils de mesure) ; PowerShell 5.1.
2. **Récupérer la branche** :

   ```bash
   git clone https://github.com/Fristick/Fristick.github.io.git
   ```

   ```bash
   git -C Fristick.github.io checkout claude/agitated-ramanujan-628db2
   ```

   Le projet est dans le sous-dossier `Backrooms/` (`main` ne contient que la page web).
3. **Compiler l'éditeur** (environ 4 min la première fois) :

   ```bash
   "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" BackroomsEditor Win64 Development -Project="<clone>\Backrooms\Backrooms.uproject" -WaitMutex -NoHotReload
   ```

4. **Ressources importées** (`Backrooms/Content/Backrooms/`, ignoré par git, environ 475 Mo) : elles se régénèrent
   depuis `RawAssets/`, de l'une de deux façons :
   - sans surveillance : `powershell -ExecutionPolicy Bypass -File Backrooms\Tools\Build\run_editor_setup.ps1 -Force`.
     Le résultat attendu est `RESULT=OK` dans `Saved\BackroomsSetup_Result.txt` ;
   - ou ouvrir l'éditeur une fois : l'import se lance seul au premier démarrage (README, installation).
   - Sans cet import, le jeu tourne en mode secours : combinaison en pièces rigides jaunes, pas de sons. Les bancs ne
     sont alors **pas comparables**.
5. **Fenêtre** : les bancs passent `-windowed -ResX -ResY`. Si l'écran est à 125 %, lancer le jeu une fois et choisir le
   mode fenêtré dans les réglages, sinon le jeu démarre sans bordure en 1536 × 960. Vérifier le champ `sortie` d'une fiche
   `.json` du banc : il doit valoir 1366 × 768.
6. **Pièges** :
   - après chaque lancement d'Unreal (jeu, banc, tests, éditeur), `git checkout -- Backrooms/Config/DefaultEngine.ini` :
     l'éditeur y écrit une section `AndroidFileServer` avec un jeton, à ne **jamais** commiter ;
   - un seul Unreal à la fois. Unreal lance au démarrage un `Build.bat -Mode=ValidatePlatforms`, qui attend la fin de tout
     autre `Build.bat` ;
   - `v48` (seuil de 16,7 ms au 95e centile en RTX fluide) réagit à toute charge du processeur : machine au repos, sans
     script Python pendant la mesure. Un échec se juge contre une référence relancée en alternance ;
   - la vidéo `l0_bacteria` a les mêmes buts d'une version à l'autre, mais pas le même horaire : les arrêts de la Bacteria
     et le bruit de ses réceptions tirent au hasard, et un bond de plus décale la suite. Relancer le banc après tout
     changement des bonds.

## 3. Mesurer

- Banc visuel figé, profil Qualité, environ 6 min. Résultats dans `Backrooms/Saved/TestRuns/v417/<tag>/Qualite/` :

  ```bash
  powershell -ExecutionPolicy Bypass -File Backrooms\Tools\Tests\run_v417_visual.ps1 -Tag essai -Profiles 1
  ```

- Mesures d'animation (depuis `Backrooms/`). Lire les lignes « appuis du jeu » et « os à moins de 2 cm de sa hauteur
  d'appui » ; la détection d'origine (os lent) compte aussi les images du décollement :

  ```bash
  python Tools/Visual/v417_motion.py --src Saved/TestRuns/v417/essai/Qualite
  ```

- IK du joueur hors moteur (300 cas, écart attendu ≤ 0,1 cm) :

  ```bash
  python Tools/check_leg_ik.py
  ```

- Tests du jeu (lanceur versionné, `Tools/Tests/run_autotests.ps1`), environ 20 min :

  ```bash
  powershell -ExecutionPolicy Bypass -File Backrooms\Tools\Tests\run_autotests.ps1 -Only "smoke,v415,v416vis,chapitre,v48" -Tag essai
  ```

- Test réseau à un client (hôte et client sur le même PC), environ 4 min :

  ```bash
  powershell -ExecutionPolicy Bypass -File Backrooms\Tools\Tests\run_nettest.ps1 -Clients 1 -Tag essai_reseau
  ```

- Références de `d8386b2` : `Docs/v417/mesures/animation/motion_apres_appuis_d8386b2.txt` et
  `Docs/v417/mesures/tests/unreal_d8386b2_resume.txt`. Les mesures d'animation (cm, degrés, pas fixe de 1/30 s) ne
  dépendent pas de la machine ; les temps d'image, si.

## 4. La suite : poser les marcheurs après le déplacement de l'image

**Constat.** `ABREntity::Animate` (`BREntity.cpp`) est encore appelé dans le `Tick` de l'acteur, donc avant le composant
de déplacement, pour les marcheurs à jambes articulées : Hound, Skin-Stealer, Faceling, Wretch, Partygoer.
`UpdateFootPlanting` tient leurs pieds au monde dans le repère de la capsule d'avant son déplacement. À chaque changement
d'allure, le pied tenu suit donc la capsule d'une image de déplacement. C'est le défaut que la Bacteria avait avant la
passe (3,5 cm par image au démarrage). Le rapport v4.17 § 8.2 le liste.

**À faire.**

1. Étendre `FBREntityLateTick` aux espèces à jambes. Le modèle à suivre est la Bacteria : `Animate` pose
   `bGaitPending` et `GaitDt`, la tick tardive appelle `PoseBacteria`.
   - Séparer ce qui dépend de la position de la capsule (`UpdateFootPlanting`, placement du `Visual`,
     `KeepRigidPartsAboveGround`) de ce qui peut rester dans le `Tick` (`AnimateLimbs`, tête, morph).
   - Garder le jumpscare (`bScareOverride` : pose immédiate, puis modèle tenu devant la caméra) et la disparition.
2. Ajouter au banc `-BRAutoTestV417Visual` une vidéo d'un marcheur sur un trajet imposé (`ABREntity::SetTestWalk`, comme
   `l0_bacteria`, en commençant par le Hound) et faire mesurer ses pieds par `v417_motion.py`. `GetSupportPoints` renvoie
   déjà les pieds des jambes articulées. Mesurer **avant** le changement, puis après.
3. Critères (`Docs/v417/agents/animation.md` R.1) : appuis ≤ 0,5 cm par image ; dérive ≤ 2 cm sur un appui ; écart
   entre le modèle et la capsule ≤ 40 cm.
4. Vérifier : BackroomsEditor compile ; banc avant/après ; `run_autotests.ps1 -Only "smoke,v415,v416vis,chapitre"` et
   `run_nettest.ps1 -Clients 1` sans problème ; `RAPPORT_v4.17.md` § 8.2 mis à jour, avec des étiquettes honnêtes
   (`[correction validée]` seulement si c'est mesuré).

Contraintes du projet : rien de répliqué en plus (protocole 416), lumière de jeu intacte, messages de commit en français.

## 5. Autres restes (rapport § 8.2)

- Joueur : le bassin descend jusqu'à 16,3 cm au changement d'appui (12,6 avant), avec la longue foulée jambe tendue de
  la cadence (94 cm par pas). Une foulée plus courte ou un genou plié à l'appui la réduirait. Le talon ne se lève pas en
  fin d'appui.
- Bacteria : au demi-tour, elle peut courir quelques images en regardant encore derrière elle (lacet rattrapé à 420°/s).
- Chez un client, le personnage simulé d'un autre joueur reste en mode de déplacement « aucun » : cause non cherchée.
  Tout autre code qui lirait « au sol » pour lui chez un client le croirait en l'air.
- R2 (rotation des marcheurs lissée chez un client) et R3 (pas sonores à la pose du pied, 3e personne et coéquipiers) :
  faits dans le code, non vérifiés en jeu.
- Non mesuré : course, accroupi, escalier, demi-tour sur place, arrêt et redépart rapprochés ; marche d'un coéquipier
  vue par un autre joueur (le test réseau ne photographie qu'un coéquipier immobile).
