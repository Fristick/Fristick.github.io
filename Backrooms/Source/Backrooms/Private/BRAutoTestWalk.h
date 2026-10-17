// v4.13 : marche reelle des tests automatiques (chemin de grille, puis seulement les entrees du joueur), definie dans
// BRAutoTestV412.cpp et partagee avec BRAutoTestV413.cpp (porte de service du Level 0, depart de groupe).
// v4.15 : places debout devant un mecanisme de la mission jugees comme le jeu (partagees avec BRAutoTestV411.cpp et
// BRAutoTestV413Lidar.cpp) et chemin de grille des tests sans bassin profond.
#pragma once

#include "CoreMinimal.h"

class AActor;
class ABRWorld;
class ABRCharacter;
class ABRMissionDevice;

namespace BRTestV412
{
	struct FWalk
	{
		TArray<FVector> Points;
		int32 Next = 0;
		FVector Goal = FVector::ZeroVector;
		float Radius = 80.f;
		/** Hauteur des pieds attendue a l'arrivee (palier, appui) ; ignoree si bCheckZ est faux */
		float GoalFeetZ = 0.f;
		bool bCheckZ = false;
		/** Pres de la passerelle du Niveau 8 : l'etape suivante vient de la regle des entites (ABRWorld::MissionNavDetour) */
		bool bDetour = true;
		/** Chemin de grille trouve (sinon : tout droit vers le but) */
		bool bPath = false;
		/** v4.15 : chemin de grille du monde (ABRWorld::FindPath) pris faute de chemin sans bassin profond (Niveau 37), au
		 *  depart ou a un nouveau chemin de la marche (v4.15 correctifs : reste vrai jusqu'a la marche suivante) */
		bool bPoolFallback = false;
		/** v4.15 : nature de la marche dans les notes (approche, reprise) ; vide pour une marche ordinaire */
		FString Tag;
		double Start = 0.0;
		double CheckAt = 0.0;
		FVector CheckPos = FVector::ZeroVector;
		int32 StuckChecks = 0;
		int32 Jumps = 0;
		int32 Replans = 0;
		bool bWantJump = false;
		bool bJumpHeld = false;
		float SideLeft = 0.f;
		float SideSign = 1.f;
		FVector LastPos = FVector::ZeroVector;
		float Travelled = 0.f;
		int32 Frames = 0;
		int32 SwimFrames = 0;
		/** Dans un volume d'eau local (bassins, sas) : images, images a la nage, profondeur maximale (surface - pieds) */
		int32 LocalFrames = 0;
		int32 LocalSwimFrames = 0;
		float LocalMaxDepth = 0.f;
		float MaxFeetZ = -1.0e6f;
		/** Deplacements de plus de 3 m en une image (le personnage ne doit jamais etre deplace pendant la marche) */
		int32 Teleports = 0;
		/** v4.15 (correctifs 3) : progression mesuree sur le chemin restant (plus court atteint), les points de passage atteints,
		 *  les etapes du detour et la montee (plus haut atteint) : un pas de cote contre un obstacle ne compte plus (avant : 40 cm
		 *  de deplacement suffisaient, et le marcheur poussait une porte fermee 300 s sans nouveau chemin) */
		float BestRemaining = 1.0e9f;
		float BestZ = -1.0e9f;
		int32 BestNext = -1;
		bool bCheckDetour = false;
		FVector CheckTarget = FVector::ZeroVector;
		/** v4.15 (correctifs 3) : images a moins de 1,5 m d'une entite ; cause d'un blocage (ce qui est devant le marcheur,
		 *  l'entite la plus proche, le point de passage), ajoutee a WalkStats */
		int32 EntityFrames = 0;
		FString StuckWhy;
		/** v4.15 (correctifs 4) : entites ecartees par le test pendant cette marche (ClassifyBlock : elles seules barraient le
		 *  passage ; ABREntity::Dismiss, chez l'hote seulement, au plus MaxEntityClears) et leur description pour WalkStats */
		int32 EntityClears = 0;
		FString EntityClearText;
		/** v4.15 (correctifs 4) : blocage final (EWalk::Stuck) cause par une entite seule : la marche n'a rien appris du decor
		 *  (NON VERIFIE chez l'appelant, jamais PROBLEME) ; BlockerText : l'entite, son espece, la cellule et pourquoi elle reste */
		bool bEntityBlock = false;
		FString BlockerText;
	};
	extern FWalk Walk;

	enum class EWalk : uint8 { Moving, Arrived, Stuck };

	/** v4.15 (correctifs 4) : entites ecartees au plus par marche */
	constexpr int32 MaxEntityClears = 3;
	/** v4.15 (correctifs 4) : ce qui barre le passage du marcheur vers Target puis After : capsule du marcheur (raccourcie de
	 *  30 cm par le bas : une marche franchie sans sauter ne compte pas, rayon moins 2 cm : le mur qu'il longe non plus)
	 *  balayee sur 150 cm de son chemin, telle quelle puis en ignorant toutes les entites. Une entite barre le passage
	 *  (bEntity) seulement si la premiere chose touchee est une entite ET si le meme balayage sans entite est libre : un mur,
	 *  une porte fermee ou une cage reste un blocage du decor, meme avec une creature collee au marcheur */
	struct FBlock
	{
		bool bBlocked = false;
		bool bEntity = false;
		AActor* First = nullptr;
		/** Ce qui bloque encore sans les entites (decor) */
		AActor* Static = nullptr;
		FString What;
	};
	FBlock ClassifyBlock(const ABRWorld* W, const ABRCharacter* C, const FVector& Target, const FVector& After);

	/** Nouveau chemin de grille de la place du personnage au but. v4.15 : sans entrer dans un bassin profond (FindPoolFreePath),
	 *  sinon le chemin du monde (ABRWorld::FindPath, celui des entites) */
	bool Replan(const ABRWorld* W, const ABRCharacter* C);
	/** Debut d'une marche vers Goal (rayon d'arrivee Radius) ; false : pas de chemin de grille (tout droit) */
	bool BeginWalk(const ABRWorld* W, const ABRCharacter* C, const FVector& Goal, float Radius, bool bDetour, bool bCheckZ = false, float GoalFeetZ = 0.f);
	void ReleaseJump(ABRCharacter* C);
	/** Distance, duree, sauts, nouveaux chemins, nage, deplacements directs ; v4.15 (correctifs 3) : images pres d'une entite et
	 *  cause d'un blocage ; v4.15 (correctifs 4) : entites ecartees par le test */
	FString WalkStats();
	/** Une image de marche. bBusy : frayeur, entree bloquee, chargement (le temps ne compte pas pour le blocage).
	 *  v4.15 : la sante mentale du marcheur reste a 100 (la folie ignore le mode invincible : regle du jeu inchangee).
	 *  v4.15 (correctifs 3) : progression sur le chemin restant (pas sur le deplacement) : nouveau chemin a 4 controles sans
	 *  progres, blocage a 8 (environ 10 s, cause dans Walk.StuckWhy) ; points de passage a 35 cm dans une salle de fosses.
	 *  v4.15 (correctifs 4) : a 4 et a 8 controles sans progres, une entite qui barre seule le passage (ClassifyBlock) est
	 *  ecartee chez l'hote (ABREntity::Dismiss : la dissipation du jeu, collision coupee) et la marche reprend ; un Smiler a
	 *  moins de 3 m ne bouge jamais et un chasseur suit le marcheur invincible : attendre ne sert a rien. Au-dela de
	 *  MaxEntityClears, ou chez un client (l'entite de l'hote bloque toujours sa simulation), le blocage est rendu avec
	 *  Walk.bEntityBlock */
	EWalk StepWalk(const ABRWorld* W, ABRCharacter* C, float Dt, double Timeout, bool bBusy);

	/** v4.15 : A* de la grille des tests, le meme qu'ABRWorld::FindPath (couts, heuristique, ordre des voisins) sans entrer dans
	 *  un bassin profond (ABRWorld::IsPoolCell ; depart et arrivee permis). Le placement de la mission garantit un tel chemin
	 *  vers chaque mecanisme et la salle de la sortie (parcours "au sec" d'ABRWorld::PlaceMission). v4.15 (correctifs 3) : ni
	 *  par une ouverture de l'aile du Level 6 dont la porte reste fermee (ABRWorld::GetClosedMissionEdges, comme FindPath) */
	bool FindPoolFreePath(const ABRWorld* W, const FIntPoint& From, const FIntPoint& To, TArray<FIntPoint>& OutPath, int32 MaxNodes);

	/** v4.15 : portee de la visee du client (ABRCharacter::UpdateFocus : balayage de 260 cm, impact a moins de 290 cm des yeux) :
	 *  un point d'interaction a 250 cm des yeux au plus est toujours vise (la boite visee est devant son centre). L'hote accepte
	 *  jusqu'a ABRWorld::MissionReachServer (340 cm) */
	constexpr float ClientFocusReach = 250.f;
	/** v4.15 : hauteur reelle des yeux au-dessus du centre de la capsule (camera du personnage, 74 cm debout) */
	float EyeHeight(const ABRCharacter* C);

	/** v4.15 (correctifs 3) : ce que la visee du client a pris, pour les notes : un mecanisme de la mission par son indice et
	 *  son nom (avant : le nom de l'acteur seul, BRMissionDevice_97), sinon le nom de l'acteur, ou "rien" */
	FString FocusName(const ABRWorld* W, const AActor* A);
	/** v4.15 (correctifs 3, relecture) : mecanismes que l'hote a trouves a une autre place qu'au placement 2
	 *  (ABRWorld::GetMissionMovedDevices) : "2, 3", "aucun", "placement 2 impossible" ou "comparaison NON FAITE" */
	FString MovedFrom2Text(const ABRWorld* W);

	/** v4.15 : place debout devant un mecanisme de la mission. v4.15 (correctifs 4) : FocusTaken : depuis cette place, la
	 *  visee du client (premier temps d'ABRCharacter::UpdateFocus) touche d'abord autre chose que ce mecanisme */
	enum class EStand : uint8 { Ok, NoFloor, Swim, Blocked, TooFar, NoSight, FocusTaken };
	const TCHAR* StandName(EStand S);
	/** v4.15 (correctifs 4) : longueur du balayage de la visee du client (ABRCharacter::UpdateFocus, InteractReach) */
	constexpr float ClientFocusSweep = 260.f;
	/** v4.15 (correctifs 4) : la visee du client depuis ces yeux, dirigee sur le point d'interaction de Dev (sphere de
	 *  BRMech::Clearance::FocusSweep, canal Visibility, seul Self ignore, ClientFocusSweep de long) : ce qu'elle touche en
	 *  premier (nullptr : rien) */
	const AActor* FocusFirstHit(const ABRWorld* W, const AActor* Self, const FVector& Eye, const ABRMissionDevice* Dev);
	/** v4.15 : le sol sous (X, Y) (trace WorldStatic depuis FromZ + 60 cm jusqu'a FromZ - 450 cm, qui n'ignore que Self) ;
	 *  false : pas de sol (chunk pas construit, vide) */
	bool FloorUnder(const ABRWorld* W, const FVector& P, float FromZ, const AActor* Self, float& OutFloorZ);
	/** v4.15 : on nagerait sur ce sol : eau plus profonde que BRMech::SwimDepth (120 cm, regle d'ABRCharacter::UpdateWater),
	 *  ou eau d'un bassin ou d'un sas (volume local) ; l'eau des canaux (60 cm) se traverse a pied */
	bool IsSwimSpot(const ABRWorld* W, const FVector& P, float FloorZ);
	/** v4.15 : la place debout en (P.X, P.Y) pour agir sur Dev, jugee comme le jeu : sol sous la place et capsule libre (requetes
	 *  qui n'ignorent que le joueur : les volumes du mecanisme vise genent comme les autres), pas a la nage si bNoSwim, yeux a
	 *  leur hauteur reelle, point d'interaction a portee de la visee du client (ClientFocusReach) et selon la regle de l'hote
	 *  (ABRWorld::CanReachMissionDevice : distance des yeux, ligne de vue qui n'ignore que le joueur et le mecanisme).
	 *  v4.15 (correctifs 4) : puis la visee du client depuis ces yeux (FocusFirstHit) touche ce mecanisme en premier. Avant :
	 *  vannes du Niveau 2 face a la porte de la vapeur (graine du niveau 554144564) : place ordinaire dans la porte, premiere
	 *  place des cercles de biais (110 cm, 67,5 degres) d'ou la visee prenait la vanne voisine ; la marche ne s'en sortait que
	 *  par sa reprise a 60 cm, et le balayage des places (sans reprise) pouvait echouer.
	 *  OutStand : centre de la capsule a poser (5 cm au-dessus du sol) */
	EStand CheckMissionStand(const ABRWorld* W, const ABRCharacter* C, const ABRMissionDevice* Dev, const FVector& P, bool bNoSwim, FVector& OutStand);
	/** v4.15 : la premiere place debout valable (CheckMissionStand) sur des cercles de 110, 160 et 210 cm, puis 75 cm (place
	 *  etroite) et 240 cm autour du point d'interaction, 16 directions a partir de l'avant du mecanisme (miroir :
	 *  Tools/verify_device_clearance.py, qui lit les cercles dans le code). OutWhy : causes des refus (nombre par cause) */
	bool FindMissionStand(const ABRWorld* W, const ABRCharacter* C, const ABRMissionDevice* Dev, bool bNoSwim, FVector& OutStand, FString* OutWhy = nullptr);
}
