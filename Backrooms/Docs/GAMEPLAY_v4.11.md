# Gameplay v4.11 : règles des douze niveaux et des neuf entités

Ce document décrit les règles **telles qu'elles sont codées** (`Source/Backrooms/Private/BRMissionLogic.cpp` pour les
missions, `BREntity.cpp` pour les créatures). Les nombres de variantes viennent du banc hors moteur
(`Docs/v411/banc_missions_3000.txt`, 3000 graines par niveau) ; ce sont des jeux de paramètres distincts observés, pas
une borne théorique. Rien de ce qui suit n'a encore été joué dans Unreal (voir le rapport, § 1).

## 1. Règles communes

**Une mission par niveau.** Deux à quatre étapes liées, affichées dans l'onglet CARNET de l'inventaire (Tab). Une étape
reste « encore inconnue » tant que la précédente n'est pas avancée. La sortie de progression du niveau est verrouillée
jusqu'à la résolution (l'invite l'indique : « Sortie verrouillée… ») ; les sorties de retour restent ouvertes.

**Paramètres tirés par graine.** L'hôte tire la solution (symboles, nombres, ordre, code) et place les mécanismes dans une
zone finie atteignable, avant de construire le décor. Les règles et les indices ne changent jamais ; seules les valeurs
changent. Une graine sans plan valide donne une variante de secours déterministe, choisie avant de placer les joueurs
(aucune sur les 36 000 graines du banc ; le placement dans le décor réel n'a pas pu être essayé ici).

**Interactions.** Touche d'interaction : lire, prendre, insérer, basculer (position suivante), appuyer. Maintenue :
observer (le caméscope reste dans le sac) ou actionner une manivelle. L'hôte vérifie chaque action : distance 3D (3,4 m
des yeux), ligne de vue, joueur vivant et chargé, une unité de manivelle toutes les 0,4 s au plus. Un bouton refusé se
réarme en 4 s (le moulin du Niveau 10 : 8 s).

**Erreurs.** Une erreur ne détruit rien : elle donne un retour lisible (texte, son, lampe d'état) et se corrige.
Exemples : « 2 éléments faux », « Surcharge : deux circuits à la fois au maximum », « Fuite de vapeur », « Défaut ! tous
les relais ont disjoncté » (à refaire), « Le câble reste mou : un autre treuil d'abord ».

**Seul ou à plusieurs.** Aucune étape n'exige deux joueurs en même temps. Une manivelle garde sa progression quand on la
lâche ; un interrupteur reste où on le laisse. Les objets de mission (fusibles, clés) sont des **objets d'équipe** : un
seul joueur peut les prendre (transaction de l'hôte), toute l'équipe peut s'en servir, et ils ne partent pas avec un
joueur qui quitte la partie.

**Départ de groupe** (en ligne). Utiliser une sortie ouverte lance un départ annoncé à tous. Le groupe part quand chaque
joueur debout est à moins de 8 m du point de départ, au même étage (2,6 m) et à vue, pendant 1,2 s. Un joueur à terre
est emmené ; un joueur encore en chargement n'est pas attendu. Annulation : 90 s sans rassemblement, initiateur à plus de
25 m, plus personne debout. Seul : départ immédiat.

**Carnet et aide.** Le carnet liste les étapes, les objets de l'équipe, les observations enregistrées et l'objectif
facultatif. Le bouton AIDE passe, pour l'étape en cours, de rien à un indice (où chercher, quoi faire, jamais la
solution), puis au rappel des indices déjà trouvés.

**Accessibilité.** Tout son utile a un signe visible ou un sous-titre (option SOUS-TITRES DES SONS, avec la direction :
devant, derrière, à gauche, à droite). Volumes séparés : effets et ambiances, voix. Aucune énigme ne demande le micro.
Les codes et symboles sont universels (lettres A à E, chiffres, six symboles dessinés en relief : triangle, cercle,
carré, losange, croix, étoile) : ils ne dépendent pas de la langue.

**Objectifs facultatifs** (5 dans la campagne : Niveaux 0, 1, 4, 5 et 37). Trois au moins donnent la variante de la fin.

## 2. Les douze niveaux

### Niveau 0 — Threshold

| | |
|---|---|
| Étapes | 1. Documenter les néons anormaux. 2. Régler le panneau et stabiliser le passage. 3. Choisir la route au levier, puis partir. |
| Indices | Trois néons portent un symbole peint et clignotent par séries (1 à 7 éclats, puis pause). Les observer (touche maintenue) les enregistre dans le carnet. Une note de maintenance donne la règle. |
| Mécanismes | Panneau de trois cadrans (0 à 7), chacun marqué du symbole d'un néon ; bouton STABILISER (refusé tant que les trois observations manquent ; sinon « n éléments faux ») ; levier d'aiguillage à deux positions (Niveau 1 / Poolrooms). |
| Sortie | Le levier ouvre soit le mur vers le Niveau 1, soit l'échelle des Poolrooms ; on peut changer d'avis tant que personne n'est parti. |
| Variantes | Symboles et nombres d'éclats tirés (jamais trois nombres égaux), néons dans trois zones différentes, ordre des cadrans mélangé : 1359 jeux distincts sur 3000 graines. |
| Facultatif | Trouver au moins trois cassettes VHS (documents de lore). |
| Ancienne partie | Une session commencée avant la v4.11 garde son objectif (six cassettes et enregistrement pendant une coupure) jusqu'à la sortie du niveau ; le niveau suivant a sa mission. |
| Menace | Bacteria (ronde), Smilers des coupures ; fosses contournables (chemins sûrs inchangés). |

### Niveau 1 — Habitable Zone

| | |
|---|---|
| Étapes | 1. Lire le schéma de distribution. 2. Poser les fusibles de l'ascenseur. 3. Alimenter et appeler l'ascenseur. |
| Indices | Le schéma (près du départ) : l'ascenseur tire sur deux circuits, la réserve sur un troisième (lettres A à E) ; puissance limitée à deux circuits. |
| Mécanismes | Trois fusibles marqués d'une lettre, dispersés dans le parking (objets d'équipe) ; cinq porte-fusibles A à E ; cinq disjoncteurs ; bouton d'appel. |
| Règles | Un disjoncteur sans fusible : « Rien ne s'allume ». Un troisième circuit : « Surcharge », avec le circuit déjà allumé. Appel avec d'autres circuits que ceux du schéma : refusé (nombre de circuits faux). |
| Choix | Allumer la réserve éclaire une salle et permet de lire le carnet du gardien (facultatif), mais l'ascenseur ne vient pas tant qu'elle est allumée. |
| Sortie | Porte vers le Niveau 2 et ascenseur vers le Niveau 4, ouverts par la mission. |
| Variantes | 120 permutations des lettres (circuits de l'ascenseur, de la réserve, fusibles leurres). |
| Menace | Hound et Smilers selon les règles existantes ; garage et zones éclairées. |

### Niveau 2 — Utility Halls

| | |
|---|---|
| Étapes | 1. Lire la plaque des pressions. 2. Suivre les conduites jusqu'aux manomètres. 3. Régler les vannes pour dégager la porte. |
| Indices | La plaque donne la pression voulue sur chaque manomètre M1 à M3 (1 à 4 bars). Examiner un manomètre (maintenu) révèle la vanne V1 à V3 qui l'alimente. |
| Mécanismes | Trois vannes à cinq crans ; porte bloquée par la vapeur. |
| Règles | Une vanne trop ouverte siffle et fuit (avertissement, réversible). La porte se dégage quand les trois manomètres sont justes, et le reste. |
| Sortie | Porte vers le Niveau 3 ; échelle de retour vers le Niveau 1 ouverte. |
| Variantes | 384 (pressions et branchement des conduites). |
| Menace | Wretch principal (chasse au bruit : les vannes et la vapeur s'entendent). |

### Niveau 3 — Electrical Station

| | |
|---|---|
| Étapes | 1. Lire le tableau de charge. 2. Trouver le secteur en défaut. 3. Rétablir les relais des secteurs sains. 4. Alimenter l'ascenseur. |
| Indices | Le tableau dit quel relais (1 à 4) alimente quel secteur (A à D). Les boîtes de jonction des secteurs, dans les couloirs : la boîte en défaut sent le brûlé et crépite (à examiner). |
| Règles | Enclencher le relais du secteur en défaut fait disjoncter tous les relais (à refaire). L'alimentation de l'ascenseur exige les trois autres. |
| Sortie | Ascenseur vers le Niveau 4 ; porte de retour vers le Niveau 2. |
| Variantes | 96 (24 correspondances relais-secteur × 4 secteurs en défaut). |
| Menace | Clump (garde un passage, s'écarte des machines alimentées : les relais en marche déplacent les zones sûres), Skin-Stealer. |

### Niveau 4 — Abandoned Office

| | |
|---|---|
| Étapes | 1. Lire le planning de garde. 2. Trouver les bureaux dans les archives. 3. Composer le code de l'accès à l'hôtel. |
| Indices | Le planning (dans un bureau) donne l'ordre de trois badges (symboles). L'annuaire des archives donne le numéro de bureau (0 à 9) de chaque badge. Le code se lit dans l'ordre du planning. |
| Mécanismes | Trois molettes 0 à 9, bouton VALIDER (refusé tant que planning et annuaire ne sont pas lus ; code faux : réarmement 4 s). |
| Sortie | Porte vers le Niveau 5, derrière la grille de l'accès à l'hôtel (levée par la mission) ; ascenseur de retour vers le Niveau 1. |
| Variantes | 3000 sur 3000 (code jamais 000 : il se trouverait sans rien lire). |
| Facultatif | Le dossier du personnel : comment reconnaître un faux explorateur au niveau suivant. |
| Menace | Facelings (neutres sauf provocation). |

### Niveau 5 — Terror Hotel

| | |
|---|---|
| Étapes | 1. Lire le registre de la réception. 2. Ouvrir les serrures de la chaufferie. 3. Appliquer la consigne de pression. |
| Indices | Le registre associe chaque serrure (symbole) à la chambre qui garde sa clé (101 à 130). Chaque clé porte l'étiquette de sa chambre. |
| Règles | Cinq clés, trois utiles ; le trousseau en tient trois (« plein » : remettre une clé à son crochet). Une serrure n'accepte que sa clé. Le robinet de la chaudière est verrouillé tant que les trois serrures sont fermées et la consigne non lue ; trop de pression fait siffler (avertissement). |
| Sortie | Passage de la chaufferie vers le Niveau 6 ; porte de retour vers le Niveau 4. |
| Variantes | 3000 sur 3000 (chambres, clés, symboles, pression 1 à 5). |
| Facultatif | La note du personnel : un faux client n'a pas de lampe allumée, ne cligne pas des yeux et s'arrête net quand on lui fait signe ; attendre deux signes, ne jamais tourner le dos. |
| Menace | Skin-Stealer déguisé, Partygoer rare. |

### Niveau 6 — Lights Out

| | |
|---|---|
| Étapes | 1. Lire la plaque de départ. 2. Activer les balises dans l'ordre. 3. Rétablir l'alimentation de secours. |
| Indices | Plaque en relief (lisible dans le noir) : symbole de la première balise. Chaque balise remontée s'allume et affiche le symbole de la suivante ; la dernière renvoie à l'alimentation de secours. |
| Règles | Remonter une balise (maintenu) fait du bruit pendant ce temps (les créatures l'entendent jusqu'à 18 m). Une balise hors ordre ne s'enclenche pas ; une balise leurre est hors service. |
| Sortie | Échelle vers le Niveau 8, derrière la sortie de secours (éclairée par la mission). |
| Variantes | 2536 (chaîne de trois balises parmi quatre symboles, leurre, zones). |
| Menace | Smilers dans le noir : se déplacer lentement, lampe baissée. |

### Niveau 8 — Cave System

| | |
|---|---|
| Étapes | 1. Lire les marques de passage. 2. Actionner les treuils dans l'ordre. |
| Indices | Marques gravées près du départ : trois lettres de treuils (parmi A à E), dans l'ordre. |
| Règles | Un treuil garde sa progression quand on le lâche (seul, on peut faire une pause). Hors ordre, le câble reste mou ; deux treuils sont grippés. La passerelle descend après le troisième. |
| Sortie | Échelle vers le Niveau 9. |
| Variantes | 120 (ordre et choix des treuils). |
| Menace | Deathmoths attirés par la lumière (l'éclairage du décor les détourne), Clump dans une zone limitée. |

### Niveau 9 — The Suburbs

| | |
|---|---|
| Étapes | 1. Rassembler les plans des maisons. 2. Reconnecter le circuit de la rue. 3. Identifier la maison du passage. |
| Indices | Trois plans dans trois maisons, un indice chacun : circuit de rue (1 à 4), symbole du porche, nombre de fenêtres en façade (2 à 5). |
| Règles | Le boîtier de la rue choisit un circuit (position 0 : déconnecté). Sans le bon circuit, les porches restent éteints et aucune maison ne s'ouvre. Deux maisons leurres ont un seul des deux traits. |
| Sortie | Porte de la maison, vers le Niveau 10. |
| Variantes | 288. |
| Menace | Skin-Stealer et Hound, brouillard. |

### Niveau 10 — Field of Wheat

| | |
|---|---|
| Étapes | 1. Trouver la marque d'orientation. 2. Lire le tableau des granges. 3. Orienter le moulin, puis lâcher le frein. |
| Indices | Une marque peinte sur une clôture (à examiner) désigne une grange par son symbole ; le tableau près du moulin donne l'angle de chaque grange (multiples de 45°). |
| Règles | Mauvais angle : le moulin tourne à vide 8 s (réarmement), rien n'est perdu. Au repos, le moulin n'indique jamais la bonne grange. |
| Sortie | Porte de la grange désignée, vers le Niveau 11. |
| Variantes | 2969. |
| Finition | Soleil bas et chaud, nuages volumétriques (profil Qualité et au-delà), ciel plus lumineux. |
| Menace | Faceling rare ; respiration après les niveaux hostiles. |

### Niveau 11 — The Endless City

| | |
|---|---|
| Étapes | 1. Alimenter la station (générateur à manivelle, garde sa charge). 2. Réunir les données de route. 3. Configurer la destination finale. |
| Indices | Trois chiffres. Un chiffre est déjà connu s'il a été rapporté d'un niveau précédent (fragment de route : Niveaux 2, 3 ou 37 ; 5 ou 6 ; 8, 9 ou 10), sinon il se lit sur un panneau de la ville. Les fragments sont dans le carnet. |
| Règles | Confirmer est refusé sans courant ; une destination fausse est refusée sans rien perdre. |
| Fin | Le portique s'ouvre sur le dernier quai : écran de fin (« Le dernier quai »), ou sa variante avec au moins trois objectifs facultatifs. Ensuite : continuer l'exploration ou revenir au menu. |
| Route annexe | La porte d'immeuble vers un niveau tiré au hasard reste ouverte, distincte de la fin. |
| Finition | Lampadaires plus nombreux et plus forts, soleil plus haut, exposition relevée. |
| Variantes | 952. |
| Menace | Facelings. |

### Niveau 37 — Poolrooms

| | |
|---|---|
| Étapes | 1. Lire les marques de niveau d'eau. 2. Observer le sens du courant. 3. Équilibrer les deux bassins. |
| Indices | Les marques (sur un pilier, au sec) donnent le niveau voulu des bassins A et B. Le courant s'observe depuis le trottoir : la vanne A déverse A dans B, la vanne B vide B. |
| Règles | Niveaux visibles sur des règles graduées : A = 5 − crans de A ; B = départ + crans de A − crans de B. Si B dépasse 7, il déborde (avertissement) : refermer A ou ouvrir B. Toutes les solutions sont atteignables sans plonger ni délai. |
| Sortie | Échelle vers le Niveau 4, au bout du passage sec (découvert par la mission) ; sol qui glitche, retour vers le Niveau 0. |
| Variantes | 40 (cibles et départ de B). |
| Facultatif | Le carnet de plongée. |
| Menace | **Aucune entité.** Eau profonde et souffle : le trajet le long des bassins est plus long mais sûr. |

## 3. Les neuf entités

| Entité | Signe annonciateur | Déclencheur | Poursuite | Contre-mesure | Retour au calme |
|---|---|---|---|---|---|
| **Bacteria** | Cris, lumières rouges autour d'elle, tête qui fouille | Suspicion qui monte avec le bruit et la lumière | Vers la dernière position connue, pas un suivi magique | Casser la ligne de vue, se cacher (placards, trous) | Fouille de la dernière position, puis reprise de la ronde |
| **Smiler** | Faisceau braqué sur lui : il tremble, son sourire s'embrase, sifflement (0,25 s de faisceau) | Faisceau maintenu (charge après 1,15 s) | Charge courte | Détourner la lampe dès le signe ; la lumière ambiante le fait disparaître | Disparition ou retour dans l'ombre |
| **Hound** | Grondement tête basse quand il traque | Courir près de lui | Pas de morsure avant 1,5 s de poursuite | Le regarder sans courir (intimidation) | Recul, retour à la ronde |
| **Skin-Stealer** | Masse immobile qui prend forme humaine quand on approche ; faux explorateur qui marche calmement vers vous, sans lampe allumée, trop droit | À moins de 5 m, ou regard soutenu 2,5 s : il se démasque | Poursuite rapide | Lui faire signe à la lampe : il s'arrête net, sans un geste (un vrai explorateur répondrait) ; garder ses distances et ne pas le fixer ; deux signes suffisent pour s'éloigner, sans lui tourner le dos | Abandonne après 8 à 10 s hors de vue |
| **Faceling** | La variante hostile se fige, tête inclinée, et crie (1,2 s) avant de charger ; dans les blés, il est tapi | Provocation : lampe braquée sur lui 2 s, ou course à moins de 4 m ; dans les blés, passer à moins de 2,6 m | Courte, seulement si l'on reste à moins de 3 m | Ne pas le provoquer ; s'éloigner dès l'avertissement | Neutre |
| **Partygoer** | Ballon rouge, rire, avertissement avant la charge | Ligne de vue, coupures | Charge | Rompre la ligne de vue derrière un angle | Recherche puis retrait |
| **Deathmoth** | Bourdonnement d'ailes | Lumière (lampe du joueur) | Vol vers la source | Éteindre sa lampe près d'un éclairage du décor : il s'y détourne | Se pose |
| **Wretch** | Halètement | Bruits (mécanismes, pas, chutes) | Chasse nerveuse, recherche courte ; après 9 s de course, pause haletante de 3,5 s | Un mécanisme bruyant ailleurs peut détourner sa recherche (sans garantie) | Recherche brève puis abandon |
| **Clump** | Craquements | Joueur dans le passage qu'il garde | Charge courte, ne s'éloigne pas de son poste (11 m) | Détour ; il s'écarte des machines alimentées (relais, générateur) | Retour au poste |

**Choix de la cible** (toutes les créatures) : la perception passe avant la proximité. Un joueur vu compte pour sa
distance, un joueur seulement entendu pour trois fois plus, un joueur ni vu ni entendu pour dix fois plus ; un joueur
caché double encore. La cible perçue depuis peu est gardée, et n'est quittée qu'après 2 s pour une autre nettement mieux
placée. Les bruits sont atténués par les murs et localisés approximativement ; la voix des joueurs n'est pas entendue.

**Budget de menace** : créatures (1, ou 2 si elles poursuivent), coupure de courant (2), joueur dans une salle de
fosses (1). Au-delà de 4 (5 à plus de deux joueurs), aucune nouvelle rencontre. Une créature n'apparaît jamais dans le
demi-espace que le joueur regarde à moins de 25 m, ni sur la route d'une sortie ouverte.
