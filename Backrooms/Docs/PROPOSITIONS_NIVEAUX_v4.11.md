# Propositions de niveaux après la v4.11 : « Level ! » et « Level Fun »

Deux propositions de conception, **non implémentées** : aucun niveau vide n'a été ajouté au jeu. Elles restent distinctes
des douze missions livrées. Les nombres ci-dessous viennent du code actuel (`BRCharacter.cpp`, `BREntity.cpp`) ; le
calcul est reproductible (script en fin de document).

## Identifiants

| Niveau | Numéro proposé | Clé | Pourquoi ce numéro |
|---|---|---|---|
| Level ! — Run for Your Life | **1001** | `level_run` | Positif (une destination négative signifie aujourd'hui « niveau tiré au hasard »), hors de 0 à 37, différent de 999 (`BRMission::EndingTarget`, la fin de campagne). |
| Level Fun | **1002** | `level_fun` | Mêmes règles. |

Les sauvegardes stockent le niveau en `int32` : ces numéros y tiennent sans changement de format. Les tirages au hasard
du Niveau 11 doivent les exclure (ce sont des branches choisies, pas des destinations aléatoires).

## 1. Level ! — Run for Your Life (1001)

**Place dans la progression.** Branche facultative depuis le Niveau 6 : une fois l'alimentation de secours rétablie, une
porte rouge s'allume à côté de la sortie de secours. Elle mène à Level !, qui ressort au Niveau 8 (le même point que la
route principale). Aucune étape obligatoire n'en dépend.

**Ce que dit le code actuel du sprint** (constantes de `BRCharacter.cpp`) :

| Grandeur | Valeur |
|---|---|
| Marche / sprint | 260 / 470 cm/s |
| Endurance | 100 ; sprint : −17/s (−10/s avec une barre énergétique) |
| Récupération | +16/s à l'arrêt, +10/s en marchant ; épuisement levé à 35 |
| Sprint depuis le plein | 5,9 s, soit 27,6 m (10 s et 47 m avec une barre) |
| Sortir de l'épuisement | 2,2 s à l'arrêt, 3,5 s en marchant |

| Poursuivant | Vitesse de poursuite | Écart gagné sur un sprint plein | En marchant |
|---|---|---|---|
| Partygoer | 520 cm/s | −2,9 m | −260 cm/s |
| Hound | 465 cm/s | +0,3 m | −205 cm/s |
| **Bacteria** | **440 cm/s** | **+1,8 m** | −180 cm/s |
| Smiler | 700 cm/s | −13,5 m | −440 cm/s |

**Conclusion** : une poursuite longue sans pause est impossible avec ces réglages (un Bacteria rattrape le joueur dès
qu'il marche). Le niveau ne change donc pas le sprint : il découpe la course.

**Structure.** Une suite de six à huit **séquences** : un couloir de course de 15 à 22 m, puis un **sas**. Un couloir de
22 m coûte 4,7 s de sprint et 80 d'endurance ; un couloir de 15 m, 3,2 s et 54. Le sas se ferme derrière le joueur et
reste fermé 6 s (vérins hydrauliques, bruit et lampe d'état) : le poursuivant attend ou fait un détour. Debout dans le
sas, le joueur regagne 64 à 96 d'endurance (4 à 6 s). La sortie du sas s'ouvre ensuite ; le poursuivant passe 6 s
après. Chaque sas est un point de reprise : en cas de chute, retour au dernier sas, poursuivant replacé 15 m en arrière.

**Embranchements lisibles.** Deux couloirs par séquence : un court avec un obstacle (poutre basse à passer accroupi,
140 cm/s ; trou à sauter), un plus long et dégagé. Le bon choix se lit avant d'y entrer : bandes peintes au sol, lampe
verte au-dessus du sas atteignable, obstacle visible depuis l'entrée. Aucun choix n'est mortel à coup sûr : le couloir
long coûte du temps, pas la vie.

**Menace.** Un seul Bacteria (modèle conservé), annoncé par ses cris ; il ne traverse pas un sas fermé. Il garde ses
règles existantes jusqu'au bout de la poursuite (pas de disparition pour respecter un quota). À la sortie, il abandonne
au dernier sas.

**Coopération.** Le sas se ferme quand tous les joueurs debout sont dedans, ou 8 s après le premier entré ; un joueur à
terre peut être relevé dans le sas. Le choix de la cible suit la règle de la v4.11 (perception d'abord, hystérésis) :
un joueur qui reste en arrière attire la poursuite, sans changement de cible incessant.

**Ce qu'il faudra construire et tester.** Acteur « sas » répliqué (état tenu par l'hôte, comme les portes de mission),
couloirs générés à partir d'une graine (longueurs bornées à 22 m), obstacles existants (poutre, trou), point de reprise.
Tests : parcours complet par un joueur (aucune séquence où l'endurance passe sous 10 avec le couloir long), parcours
avec erreur (couloir court raté), deux et quatre joueurs, joueur à terre dans un sas.

## 2. Level Fun (1002)

**Place dans la progression.** Branche facultative depuis le Niveau 5 : la porte de la salle de bal de l'hôtel, ouverte
après la mission, mène à Level Fun, qui ressort au Niveau 6.

**Ambiance.** Salles de fête sans fin : guirlandes lumineuses, ballons, confettis au sol, tables de gâteaux, musique
étouffée. Inquiétant parce que tout est prêt et que personne ne fête rien.

**Menace.** Partygoers avec leurs modèles conservés (ballon rouge). Leur vitesse de poursuite (520 cm/s) dépasse le
sprint : on ne leur échappe pas en courant. Le niveau est donc un niveau d'**infiltration**.

**Règle claire, annoncée dès l'entrée** (affiche « Règlement de la fête ») :

- quand la musique joue, les Partygoers se rassemblent sur la piste de danse de leur salle ; la musique couvre les pas ;
- quand elle s'arrête, ils fouillent la salle ; ils voient ce qui bouge sous les guirlandes allumées, pas dans l'ombre
  entre deux guirlandes si l'on marche.

**Mission.** Trois cartes d'invitation ouvrent la porte de sortie (« salle des cadeaux »). Chaque carte est derrière un
petit mécanisme bruyant :

1. une boîte à cadeau à manivelle (cliquetis : s'entend à 10 m) ;
2. une pompe à ballons (sifflement : 8 m) ;
3. une boîte à musique (joue 5 s : relance la musique de la salle, donc rassemble les Partygoers ailleurs).

Le joueur choisit le moment : actionner un mécanisme bruyant pendant que la musique joue, ou se servir de la boîte à
musique comme diversion. Les cartes sont des objets d'équipe (transaction de l'hôte, un seul preneur).

**Espaces de repli.** Un placard ou une réserve au plus tous les 20 m (les cachettes existantes), et des rideaux qui
cassent la ligne de vue. Un Partygoer qui fouille abandonne après une recherche courte s'il ne voit plus personne.

**Ce qu'il faudra construire et tester.** Cycle de musique par salle (répliqué), règle de visibilité sous les guirlandes
(éclairage existant), trois mécanismes (variantes des mécanismes de mission), cachettes. Tests : parcours solo sans
détection en suivant la règle, détection volontaire puis repli dans un placard, deux et quatre joueurs, micro coupé.

## Calcul (reproductible)

```python
sprint, walk, drain, regen_stand = 470.0, 260.0, 17.0, 16.0
t_full = 100 / drain                     # 5.9 s
print(t_full * sprint / 100)             # 27.6 m de sprint depuis le plein
for name, v in {"Partygoer": 520, "Hound": 465, "Bacteria": 440, "Smiler": 700}.items():
    print(name, (sprint - v) * t_full / 100, walk - v)   # ecart gagne (m), en marchant (cm/s)
for L in (15, 22):
    print(L, L * 100 / sprint * drain)   # endurance depensee par couloir : 54, 80
```
