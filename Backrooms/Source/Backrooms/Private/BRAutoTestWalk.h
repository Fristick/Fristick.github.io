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
	};
	extern FWalk Walk;

	enum class EWalk : uint8 { Moving, Arrived, Stuck };

	/** Nouveau chemin de grille de la place du personnage au but. v4.15 : sans entrer dans un bassin profond (FindPoolFreePath),
	 *  sinon le chemin du monde (ABRWorld::FindPath, celui des entites) */
	bool Replan(const ABRWorld* W, const ABRCharacter* C);
	/** Debut d'une marche vers Goal (rayon d'arrivee Radius) ; false : pas de chemin de grille (tout droit) */
	bool BeginWalk(const ABRWorld* W, const ABRCharacter* C, const FVector& Goal, float Radius, bool bDetour, bool bCheckZ = false, float GoalFeetZ = 0.f);
	void ReleaseJump(ABRCharacter* C);
	/** Distance, duree, sauts, nouveaux chemins, nage, deplacements directs */
	FString WalkStats();
	/** Une image de marche. bBusy : frayeur, entree bloquee, chargement (le temps ne compte pas pour le blocage).
	 *  v4.15 : la sante mentale du marcheur reste a 100 (la folie ignore le mode invincible : regle du jeu inchangee) */
	EWalk StepWalk(const ABRWorld* W, ABRCharacter* C, float Dt, double Timeout, bool bBusy);

	/** v4.15 : A* de la grille des tests, le meme qu'ABRWorld::FindPath (couts, heuristique, ordre des voisins) sans entrer dans
	 *  un bassin profond (ABRWorld::IsPoolCell ; depart et arrivee permis). Le placement de la mission garantit un tel chemin
	 *  vers chaque mecanisme et la salle de la sortie (parcours "au sec" d'ABRWorld::PlaceMission) */
	bool FindPoolFreePath(const ABRWorld* W, const FIntPoint& From, const FIntPoint& To, TArray<FIntPoint>& OutPath, int32 MaxNodes);

	/** v4.15 : portee de la visee du client (ABRCharacter::UpdateFocus : balayage de 260 cm, impact a moins de 290 cm des yeux) :
	 *  un point d'interaction a 250 cm des yeux au plus est toujours vise (la boite visee est devant son centre). L'hote accepte
	 *  jusqu'a ABRWorld::MissionReachServer (340 cm) */
	constexpr float ClientFocusReach = 250.f;
	/** v4.15 : hauteur reelle des yeux au-dessus du centre de la capsule (camera du personnage, 74 cm debout) */
	float EyeHeight(const ABRCharacter* C);

	/** v4.15 : place debout devant un mecanisme de la mission */
	enum class EStand : uint8 { Ok, NoFloor, Swim, Blocked, TooFar, NoSight };
	const TCHAR* StandName(EStand S);
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
	 *  OutStand : centre de la capsule a poser (5 cm au-dessus du sol) */
	EStand CheckMissionStand(const ABRWorld* W, const ABRCharacter* C, const ABRMissionDevice* Dev, const FVector& P, bool bNoSwim, FVector& OutStand);
	/** v4.15 : la premiere place debout valable (CheckMissionStand) sur des cercles de 110, 160 et 210 cm, puis 75 cm (place
	 *  etroite) et 240 cm autour du point d'interaction, 16 directions a partir de l'avant du mecanisme (miroir :
	 *  Tools/verify_device_clearance.py, qui lit les cercles dans le code). OutWhy : causes des refus (nombre par cause) */
	bool FindMissionStand(const ABRWorld* W, const ABRCharacter* C, const ABRMissionDevice* Dev, bool bNoSwim, FVector& OutStand, FString* OutWhy = nullptr);
}
