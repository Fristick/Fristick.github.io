# Gameplay v4.16 : la lumière de jeu suit l'image

Les règles de la v4.15 ([`GAMEPLAY_v4.15.md`](GAMEPLAY_v4.15.md)) restent valables. Ce document décrit seulement ce que
la v4.16 change pour le joueur ; elle n'ajoute aucun système de jeu. Sauf mention contraire, tout a été joué dans Unreal
5.8 par les tests du jeu (`final416`), le test réseau (`fnet_*`, 2 à 4 joueurs sur un seul PC) et la traversée du premier
chapitre dans le paquet Shipping : résultats et limites dans [`RAPPORT_v4.16.md`](RAPPORT_v4.16.md). Les appuis de
manette ont été injectés par les tests : aucune vraie manette n'a été essayée.

## 1. Lumière de jeu (créatures, santé mentale)

- **Plafonniers** : une lampe ne compte plus quand sa lumière traverse la géométrie bâtie (murs, jambages, linteau ou
  arche des passages, piliers, cellules pleines, plafond, sol sauf par une fosse, un bassin ou une cuve ouverte, porte de
  mission fermée). Avant, elle comptait par la seule distance : au Level 0 (graine 9), un coin que l'image montre noir
  valait 0,72, dont 0,35 d'une lampe derrière le mur, et un Smiler s'y dissipait ; il vaut 0,22 et le Smiler reste. Le
  calcul ne lit que la graine et le plan : même valeur chez l'hôte et les clients, dans tous les profils graphiques, quel
  que soit FLASHS.
- **Coupures** : pendant la défaillance (1,8 s) et le retour (1,4 s), les règles suivent le fondu régulier de
  l'alimentation du niveau, commun à tous ; le vacillement de l'écran, tiré chez chacun, ne décide plus rien (avant, une
  zone pouvait être éclairée chez un joueur et noire chez l'autre). Chez un client, le fondu part avec la latence.
- **Échelles de sortie** (parcours classique, Niveaux 2, 8, 37) : la lampe que le chunk ne pose pas sous la trappe ne
  compte plus.
- **Lumières de mission** : une capsule (joueur, coéquipier, créature, ou la cible elle-même) ne les arrête plus ; murs et
  battants fermés, toujours. Jusqu'à la v4.15, elles ne comptaient donc jamais pour un joueur ou une créature. Seule
  source actuelle : le porche allumé du Niveau 9 ; à moins de 5,1 m, le joueur n'est plus « dans le noir ». Le Level 6
  n'en a plus depuis la v4.13.
- **Inventaire** : sous la jauge de santé mentale (PERSONNAGE), l'état qui compte : « dans le noir » (sous 0,12),
  « pénombre », « éclairé » (au-dessus de 0,50), sans couleur. Traductions (21 langues) à relire.
- **Pas compté** : le rebond (Lumen) ; trottoirs des Poolrooms, objets posés et battants des mécanismes ne cachent pas les
  lampes.

## 2. Ce que cela change par niveau

La v4.16 n'ajoute jamais de lumière. Seuils inchangés (apparition d'une entité de l'ombre sous 0,12, Smiler dissipé
au-dessus de 0,50, Deathmoth 0,55, Bacteria 0,15, santé mentale 0,12), comme les minuteries et le budget de menace. Banc
hors moteur (300 graines du Level 0, 60 par autre niveau, courant plein, plafonniers seuls), v4.15 → v4.16 :

| Niveau | Cellules où une entité de l'ombre peut apparaître | Positions où un Smiler se dissipe | Positions « dans le noir » | 100 points de santé mentale perdus sans lampe en |
|---|---|---|---|---|
| 0 | 14,4 → 29,0 % | 45,9 → 36,7 % | 16,0 → 31,3 % | 14,5 → 11,2 min |
| 1 | 22,1 → 32,5 % | 38,6 → 31,7 % | 23,4 → 33,5 % | 11,0 → 9,6 min |
| 2 | 34,0 → 56,8 % | 27,7 → 18,4 % | 34,1 → 57,0 % | 7,7 → 6,3 min |
| 3 | 24,7 → 43,4 % | 37,5 → 25,5 % | 24,3 → 44,2 % | 6,6 → 5,6 min |
| 4 | 0,0 → 3,5 % | 96,8 → 81,2 % | 0,1 → 6,0 % | 55,4 → 38,5 min |
| 5 | 54,8 → 58,0 % | 8,2 → 7,3 % | 64,3 → 66,1 % | 6,4 → 6,3 min |
| 8 | 73,3 → 73,7 % | 8,4 → 8,3 % | 72,7 → 73,6 % | 4,9 → 4,9 min |
| 37 | 0,2 → 3,4 % | 94,7 → 80,1 % | 0,2 → 3,9 % | remonte (pas d'entités) |

- Une part de cellules n'est pas un nombre d'apparitions. En jeu (une graine par niveau), joueur dans le noir : Level 0
  18,7 → 37,0 % (graine 9), Niveau 2 23,9 → 45,7 %, Niveau 9 30,3 → 31,9 %. Level 6 (sans plafonniers) : inchangé ;
  Niveaux 10 et 11 : non mesurés.
- **Décision du propriétaire** : seuils gardés. Si c'est trop dur : baisser le seuil d'apparition (`NeedsDark`, 0,12) ou
  compter une part des lampes cachées comme rebond. Rien n'est appliqué ; aucune partie réelle n'a jugé la difficulté.

## 3. Scanner LiDAR

- Les inscriptions restent sur leur support (battant, plaque, cage), à sa place du moment. Level 6 : le battant de la
  porte du passage pivote jusqu'à 95° et déplace la plaque de 100,5 cm ; avant, un nouveau scan la redessinait dans le
  vide, à l'ancienne place. Rien n'est relevé pendant le mouvement ; à l'arrêt, la passe recommence.
- Plus de faux mur de points dans une porte du passage ouverte (avant : 91 % des rayons arrêtés par sa boîte de visée) ;
  fermée, les points sont sur le battant (avant : jusqu'à 17 cm devant) ; cages : sur les barreaux. La porte de service
  ouverte ne coupe plus le scanner (§ 7).

## 4. Échelles

- **Près du pied** : le grimpeur n'est plus levé le long d'un mur haut, d'une caisse, d'un coéquipier ou sous un plafond
  bas (avant : jusqu'à 37 cm sur un sol plat, 105 cm depuis le canal, collé à la paroi) ; il monte seulement si la marche
  est libre (caisse de 20 cm franchie, 50 cm non ; plus strict : 36 à 45 cm sous l'accroche ne passent plus).
- **En descente** : sur un obstacle bas (caisse sous l'échelle, marche latérale), il lâche l'échelle et s'y tient ; avant,
  il restait accroché à 16–20 cm jusqu'à Saut ou Interagir.
- **Poolrooms, canal → trottoir** : prise depuis le canal, la montée passe au-dessus du trottoir (0,55 s), debout dessus ;
  en redescendant, il lâche sur le trottoir. Jugé avec des entrées injectées (non vérifié en v4.15), pas par une personne.

## 5. Coopération

- **Abandon à terre** ({Saut}) pour tous, hôte comme client ; avant, l'hôte refusait le réveil d'un client qui
  abandonnait, qui retombait à terre au point de départ. Réveil 0,3 s après l'appui.
- **Coéquipiers lointains** : chaque personnage est transmis à tous, à toute distance (avant : rien au-delà de 150 m).
  Délai à terre juste : un coéquipier debout, même à 230 m, donne la fenêtre entière (30 s pour une blessure, aide
  affichée) ; tous les autres à terre, ou le dernier debout qui tombe après : 6 s. Avant, réveil à 6 s refusé par l'hôte.
- **Mécanismes loin de l'hôte** : animés chez l'hôte à moins de 35 m de n'importe quel joueur (avant : tablier de la
  passerelle du Niveau 8 et déversoir du sas des Poolrooms restés levés chez l'hôte).
- **Arrivée tardive, reconnexion** : même porte de service ouverte, ou au Level 6 même plan et même état de mission ; rien
  ne recommence chez l'hôte. Les objets rendus au monde sont visibles pour qui rejoint ou se reconnecte.
- **Interactions simultanées** (4 joueurs) : un seul gagnant d'un ramassage, objet retiré pour tous ; un seul départ ; la
  même action du sas demandée par deux clients s'applique l'une après l'autre.
- **Actions maintenues** : la pause arrête aussi l'interaction (manivelle, treuil, poste, relever un coéquipier) ; après
  l'inventaire, la pause ou la perte du focus, une touche toujours tenue ne relance plus seule le scan, la course ou
  l'interaction.
- Protocole **416** (un client 4.15 est refusé). Deux PC, réseau réel, quatre joueurs physiques : non essayés.

## 6. Manette

- Les invites montrent les boutons dès que la manette a servi en dernier (tout bouton compte).
- **Inventaire et carnet** : LB / RB et croix gauche / droite (onglets), croix haut / bas (défilement), Y (aide du
  carnet), B (fermer). **PERSONNAGE** : la croix déplace le curseur des cases, A utilise ou équipe, X inspecte, B ferme
  l'inspection puis l'inventaire.
- **Pause** (aussi en solo, jeu arrêté) : croix pour les cartes, A valide, B revient ; plus de « [FIN] Quitter » à la
  manette. Dans l'inventaire ouvert depuis la pause, LB / RB changent d'onglet, B revient à la pause (ces commandes ne
  marchaient pas pendant la pause au premier essai : corrigé).
- Glisser-déposer : souris seulement ; défilement du carnet à la croix : non vérifié.

## 7. Visée, portes et mécanismes

- **Mécanismes voisins** (tous les niveaux) : parmi les mécanismes touchés par la visée, celui dont le point visé est le
  plus près de l'axe du regard l'emporte (un autre que le premier touché seulement si l'hôte l'accepte). Avant, de biais,
  un voisin à 58 cm volait la visée (banc : 16 paires sur 21, dont disjoncteurs du Niveau 1, vannes du Niveau 2, moulin
  du Niveau 10) ; maintenant aucune. Un mécanisme caché n'est jamais pris. Pas d'étape dédiée en jeu.
- **Poolrooms** : la vanne B du bassin est à 98 cm de la vanne A (avant 58 cm) ; de biais, viser A prenait B et la mission
  ne se résolvait pas. Même place pour tous, sauvegarde inchangée ; graine du défaut rejouée hors moteur seulement.
- **Porte de service ouverte** (Level 0) : sa boîte d'interaction coupait, chez tous, la vue des créatures, le nom des
  coéquipiers, l'occultation des voix et des sons, le scanner et les apparitions à l'abri des regards. Ouverte, elle
  laisse tout passer et se vise encore (à moins de 2,08 m, regard à 30° au plus de son centre) ; fermée, rien ne change.

## 8. Image et performances

- **Bacteria** posée sur le sol : elle s'y enfonçait jusqu'à 9 cm en se tassant ; mesurée à 2,1 cm au-dessus. Code de
  pose seulement, le modèle ne change pas.
- **Décalques d'usure** : effacés à 35 m au plus (avant : jusqu'à 116 m) ; au plus 40 décalques et 24 taches au sol par
  chunk ; aucun sur les objets ; pas de seuil humide au-dessus d'un bassin. Tirages inchangés, les mêmes chez tous.
- **Cinématique** : Lumen n'éclaire plus aux impacts que les reflets (l'indirect revient aux sondes d'écran). Sur le
  portable de test (RTX 5050 Laptop, 1366 × 768) : salles de fosses 34,7 → 25,1 ms par image (28,8 → 39,8 img/s),
  autres scènes 19,2 à 31,7 ms (avant 22,9 à 39,6) ; mémoire vidéo −6 à −8 %. Image : la plupart des poses très proches
  (35 à 54 dB de PSNR), poses éclairées du Level 0 13 à 23 % plus lumineuses. Cinématique reste le plus lourd (mêmes
  scènes : Performance 7,9 à 10,6 ms, Qualité 9,2 à 12,6, RTX fluide 8,9 à 11,5, à rendu interne plus petit) ; les autres
  profils ne changent pas. Jugement de l'image : propriétaire ; aucune mesure sur la machine de référence.

## 9. HUD, affichage, REJOUER

- Toujours aucune barre de vie ; endurance et santé mentale dans PERSONNAGE seulement, avec l'état de la lumière (§ 1).
  462 rendus (3 définitions, 22 langues) : aucun texte hors de son panneau, aucun chevauchement. Aides du pied de
  l'inventaire bornées (taille réduite, puis points de suspension). Commandes de la console sous la pause : mode
  développeur seulement.
- HUD contextuel du scanner et trois modes d'affichage (PARAMÈTRES > VIDÉO) : vérifiés, inchangés ; plein écran
  exclusif non essayé.
- **REJOUER** : un document lu en rejeu reste au carnet (avant : perdu au retour dans la campagne) ; rien d'autre n'est
  écrit.

## 10. Ce qui ne change pas

- Campagne `0 → 1 → 37 → 2 → 3 → 4 → 5 → 6 → 8 → 9 → 10 → 11 → dernier quai` ; premier chapitre
  `0 → 1 → 37 → 2 → fin du chapitre`, traversé aussi dans le paquet Shipping.
- Parcours linéaire (disposition fixe, une seule sortie) ; Level 0 sans énigme, porte tirée par partie ; Level 6 au
  scanner, sans lumière.
- Mort : en solo linéaire, le niveau recommence (équipement de départ) ; parcours classique : retour au Level 0 ;
  coopération : fenêtre de réanimation, puis réveil au départ du niveau avec l'équipement de départ.
- Sauvegardes au format 6, sans migration. Modèles : aucun modèle protégé touché, aucune créature remodelée.
- Seuils et loi de la lumière, minuteries, budget de menace ; plans des niveaux (hors vanne B des Poolrooms).
