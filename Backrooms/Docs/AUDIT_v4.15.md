# Audit v4.15 : la base v4.14 jouée pour la première fois dans Unreal

Base : branche `backrooms`, commit `3f65cc9` (v4.14). Les versions 4.12 à 4.14 avaient été écrites dans un conteneur
Linux **sans Unreal ni GPU** ([`RAPPORT_v4.14.md`](RAPPORT_v4.14.md) § 1) : leurs tests du jeu étaient préparés, jamais
lancés. Cet audit est le premier passage complet de ces tests dans le moteur.

## 1. Machine

| | |
|---|---|
| Système | Windows 11 Famille 10.0.26200 |
| Processeur, mémoire | Intel Core i7-13620H, 16 Go |
| Carte graphique | NVIDIA GeForce RTX 5050 Laptop GPU (rendu) ; écran sur Intel UHD Graphics |
| Moteur | Unreal Engine 5.8.3 (CL 58210709), D3D12 SM6, ray tracing matériel |
| Compilateurs | Visual Studio 2026 Community, MSVC 14.50.35739 ; clang-cl 22.1.3 (bancs) |
| Fenêtre des tests | 1536 × 960 (rendu interne 80 %, TSR) |

## 2. Mise en route sur Unreal 5.8

- Deux API dépréciées en 5.8 (`FCoreDelegates::OnPostEngineInit`, `UMaterial::SetMaterialUsage`) : remplacées ; la
  compilation éditeur et jeu passe sans avertissement du projet.
- Installation du contenu (`backrooms_setup.py`) : deux faux échecs de validation sous Windows, corrigés dans le script :
  l'import FBX d'Unreal 5.8 (Interchange) ne crée plus un emplacement de matériau sans polygone (dents du Hound), et la
  taille d'une texture se lit dans le registre des ressources (la compilation des textures, asynchrone au démarrage,
  donnait 32 px).
- Rapport des tests : la carte graphique nommée était celle de l'écran (Intel), pas celle du rendu ; corrigé
  (`GRHIAdapterName`).

## 3. Résultats de la base (avant toute correction de jeu)

Suites lancées une par une (`Saved/TestRuns/run_autotests.ps1`, journaux `autotest/base`) :

| Suite | État | Problèmes | Non vérifiés |
|---|---|---|---|
| démarrage (`-BRSmokeTest`) | réussi | 0 | 0 |
| V414 | incomplet | 0 | 3 |
| V413 | échec | 11 | 0 |
| V412 | échec | 8 | 0 |
| V411 | échec | 6 | 1 |
| V410 | incomplet | 0 | 2 |
| V49, V47 | réussi | 0 | 0 |
| V48 | échec | 2 (60 img/s en RTX fluide : course au Level 0, Poolrooms) | 0 |
| fosses | échec | 1 (img/s en Cinématique) | 0 |
| complet (`-BRSeed=4242`) | échec | 29 | 6 |
| réseau (`-BRNetTest`, lancé après la fusion du parcours linéaire) | échec | hôte 5, client 6 | 3 et 2 |

## 4. Constats

Chaque échec a été diagnostiqué (code, journaux, captures, sondes hors moteur), puis vérifié par une seconde analyse
chargée de le réfuter. Deux de mes hypothèses ont été réfutées ainsi et abandonnées. Diagnostics : `Saved/TestRuns/
analysis/` (non versionné) ; corrections et bancs : [`Docs/v415/`](v415/).

**Bugs du jeu** (un joueur pouvait être bloqué) :

| # | Constat | Portée |
|---|---|---|
| 1 | Level 6 : la cage de confinement enferme le point d'action de son poste | toutes les graines : mission impossible |
| 2 | Level 6 : recherche de l'aile du scanner limitée | ~47 % des graines en solution de repli |
| 3 | Graines de niveau tirées par `rand()` | 32 768 dispositions possibles sous Windows |
| 4 | Niveau 8 : mécanismes pris dans la collision des rochers | ~51 % des graines |
| 5 | Niveau 9 : « Plan de maison » dans le volume de la maison (porche) | selon la graine |
| 6 | Bruit : la ligne de vue du bruit s'arrête sur les capsules des personnages | Wretch : ~55 % de la portée ; LiDAR 9,9 m au lieu de 18 |
| 7 | Coopération : un joueur qui rejoint tombe pendant son chargement et l'hôte le tue | à chaque arrivée au Level 0 |
| 8 | Coopération : départ de groupe refusé devant une porte posée sur un mur (point visé sur la face du mur) | ascenseurs, portes |

Trouvés ensuite, une fois ces blocages levés (le test allait plus loin) :

| # | Constat | Portée |
|---|---|---|
| 9 | Niveau 10 : poteaux du moulin orientés au hasard, un mécanisme cache son voisin à la visée | 36 combinaisons sur 64 |
| 10 | Niveau 8 : échelle de sortie impossible à grimper depuis le tablier incliné | depuis la v4.12 |
| 11 | Niveau 8 : échelles des chunks enfouies dans la roche de leur paroi (départ de groupe refusé) | selon la graine |
| 12 | Créatures : chemin calculé par une porte de l'aile du Level 6 restée fermée | Level 6 |
| 13 | Poolrooms : échelles des chunks contre la marche immergée d'un trottoir voisin (le grimpeur n'y tient pas) ; montée arrêtée au bord d'une marche ou d'un trottoir quand on prend l'échelle depuis plus bas | 22 % des échelles des chunks, parcours classique ; échelles sur un trottoir prises depuis le canal |

**Défauts des tests** (le jeu était juste, le test se trompait) : seuil des zones humides ; chemin par un bassin
profond ; mort par folie malgré l'invincibilité des tests ; étapes en cascade après un premier échec ; compteur partagé
de l'aide du carnet ; carnet contrôlé au Level 0 ; « mise de côté » attendue à tort ; vision nocturne demandée sur un
niveau LiDAR ; course entre les deux machines du test réseau ; barrière de fin ; téléportation du client ; attente du
rassemblement ; sonde de 5 cm tangente aux murs ; marche par une porte fermée ; Smiler immobile dans un couloir ;
attentes écrites avant que le test atteigne la fin du Niveau 2 (canal de test : le fil continue au Niveau 3).

**Limites de cette machine** (non corrigées, notées) : 60 img/s en RTX fluide dans les Poolrooms (à la limite :
95 % des images autour de 17 ms) ; images par seconde des salles de fosses en Cinématique (21 à 26) ; débits du scanner
à 144 et 240 img/s non jugeables.
