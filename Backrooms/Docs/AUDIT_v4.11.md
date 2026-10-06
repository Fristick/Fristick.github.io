# Audit v4.10 → v4.11

Audit fait le 6 octobre 2026 sur la branche `backrooms`, commit `3e1f55b` (v4.10, 3/3), avant toute modification. La
branche n'avait pas évolué depuis ce commit.

## Environnement : ce qui peut être vérifié ici

| Élément | Constat |
|---|---|
| Machine | Conteneur Linux x86_64, **aucun GPU**. |
| Unreal Engine | **Absent** (ni éditeur, ni UAT, ni toolchain d'Epic, ni Xcode). Aucune compilation, aucun cuisinage, aucun lancement du jeu n'est possible ici. |
| Documentation | `dev.epicgames.com`, `partner.steamgames.com` et les guides NVIDIA sont **refusés par la politique réseau** (réponse 403). Rien n'a été contourné. |
| Possible ici | Python 3.11 (tests des outils), g++ 13 et clang 18 (logique pure compilée et exécutée ; syntaxe du code Unreal avec des en-têtes simplifiés), fontTools, Pillow. |
| Non vérifiable ici | Compilation Editor et Shipping, cook, paquets, rendu, captures d'écran, images par seconde, réseau réel, Linux et macOS natifs, Steam. Ces points sont marqués **préparé, non exécuté** dans le rapport. |

## 1. Bugs du § 2, vérifiés sur `3e1f55b`

| Point | Vérifié dans (base) | Constat | Catégorie |
|---|---|---|---|
| 2.1 Ramassage | `BRInteractables.cpp:184` (`ABRPickup::Collect`) | `ReceivePickup` remplit l'inventaire **avant** toute réponse de l'hôte ; `MarkCollected` vient ensuite. Deux clients sur la même copie locale reçoivent chacun l'objet. | constat de source ; reproduction réseau **préparée** (test `-BRNetTest`, « ramassage dispute ») |
| 2.1 Cassettes | `BRPlayerController.cpp:2313` (`ServerVHSCollected`), `BRWorld.cpp:2569` | RPC sans identifiant d'objet : le compteur VHS avance à chaque appel, sans lien avec un ramassage. | constat de source |
| 2.1 Validation | `BRPlayerController.cpp:2292` (`ServerMarkCollected`) | Ni distance, ni ligne de vue, ni état du joueur, ni numéro de niveau. | constat de source |
| 2.1 Stock de soins | `BRCharacter.cpp:1281-1287` | `ServerDeclareHealStock` reprend les quantités du client (bornées à 40) à chaque arrivée ; un soin est accepté tant que le stock est inconnu. | constat de source |
| 2.2 Réponses de soin | `BRCharacter.cpp:1219` (`ClientHealResult`) | Toute réponse acceptée retire un objet et applique le soin, même déjà reçue ; aucune trace des transactions appliquées. | constat de source ; répétition **préparée** (test réseau « soin périmé ») |
| 2.3 Fixture POSIX | `Tools/Build/tests/test_packaging.py` | **Reproduit** : 28 tests, 26 réussis, 1 échec (`test_paquet_linux_complet`), 1 sauté. Journal : `Docs/v411/tests_outils_base_v410.txt`. | défaut reproduit |
| 2.4 Lancement | `Tools/Build/check_package.py` (`check_launch`) | **Reproduit** : lancement en échec → « PAQUET VÉRIFIÉ », code 0 ; lancement incomplet ou rapport `{}` → « PAQUET VÉRIFIÉ ET LANCÉ », code 0. Journal : `Docs/v411/reproduction_outils.txt`. | défaut reproduit |
| 2.5 Binaires tronqués | `check_package.py` (`pe_info`, ELF, Mach-O) | **Reproduit** : faux PE de 68 octets → `struct.error` non gérée. Même journal. | défaut reproduit |

## 2. Progression, créatures, finition (§ 4 à § 8)

| Point | Vérifié dans (base) | Constat |
|---|---|---|
| Objectifs | `BRLevels.cpp:80` | Seul le Niveau 0 a `bRequireObjectives` (6 VHS, `VHSChance` 0,22, enregistrement pendant une coupure). Les autres niveaux s'ouvrent dès qu'une sortie est trouvée. |
| Fin | `BRLevels.cpp:538` | Le Niveau 11 sort vers `-1` (niveau tiré au hasard) : pas de conclusion. |
| Sortie de groupe | `BRPlayerController.cpp` (`ServerRequestTransition`) | Une seule interaction transporte tout le monde ; la validation est une distance 2D de 6 m à une sortie (à travers un mur ou depuis un autre étage). |
| Wretch et Clump | `BREntity.cpp:1914, 1917` | Les deux appellent `ThinkSimpleHunter` (mêmes règles, seules l'intensité et la durée diffèrent). |
| Choix de cible | `BREntity.cpp:1354` (`PickTarget`) | Choix par la distance, pénalité pour un joueur caché ; ni mémoire, ni hystérésis : un joueur proche derrière un mur passe devant un joueur visible. |
| Menace | `BRWorld.cpp` (`AllowsNewEncounter`) | Directeur de tension seul : rien n'empêche d'empiler coupure, Bacteria et plusieurs chasseurs. |
| Niveau 10 | `BRLevels.cpp` (Niveau 10) | Ciel couvert uniforme, sans nuages ni soleil marqué. |
| Niveau 11 | `BRLevels.cpp` (Niveau 11) | Lampadaires rares et faibles : rues sombres. |
| Sons | `BRPlayerController.cpp` (`ApplySettings`) | Un seul volume général ; aucun sous-titre. |
| Signature Mac | `Tools/Build/build_mac.sh` | Signe `.dylib`, `.so`, frameworks et le bundle extérieur ; les exécutables imbriqués ne sont pas tous signés de l'intérieur vers l'extérieur. |

## 3. Ce qui est garanti par la base et doit rester

Préparation des niveaux, éviction des caches, confirmation vidéo modale, soins confirmés par l'hôte (v4.10), protections
des sauvegardes (CRC, copie de secours, file d'écriture, formats futurs en lecture seule), polices, contrôles de paquet,
suites de tests V47, V48, V49, V410 et fosses. Le manifeste `RawAssets/protected_assets.json` liste les modèles fournis ;
aucun fichier qu'il protège n'est modifié par la v4.11.

## 4. Ordre de travail retenu

1. Outils de livraison (2.3 à 2.5), puis transactions coop (2.1, 2.2).
2. État des missions, réseau, sauvegarde (format 4 avec version de génération), sortie de groupe.
3. Missions des Niveaux 0, 1, 2 et 37, puis des huit autres, validées hors moteur sur des milliers de graines.
4. Créatures, puis finition (carnet, aide, sous-titres, volumes, ciel du 10, rues du 11, traductions).
5. Tests du jeu v4.11 (préparés), journaux, rapport, document de gameplay, propositions de niveaux.
