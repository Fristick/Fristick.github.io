// v4.13 : marche reelle des tests automatiques (chemin de grille, puis seulement les entrees du joueur), definie dans
// BRAutoTestV412.cpp et partagee avec BRAutoTestV413.cpp (porte de service du Level 0, depart de groupe).
#pragma once

#include "CoreMinimal.h"

class ABRWorld;
class ABRCharacter;

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

	/** Nouveau chemin de grille (A* du monde) de la place du personnage au but */
	bool Replan(const ABRWorld* W, const ABRCharacter* C);
	/** Debut d'une marche vers Goal (rayon d'arrivee Radius) ; false : pas de chemin de grille (tout droit) */
	bool BeginWalk(const ABRWorld* W, const ABRCharacter* C, const FVector& Goal, float Radius, bool bDetour, bool bCheckZ = false, float GoalFeetZ = 0.f);
	void ReleaseJump(ABRCharacter* C);
	/** Distance, duree, sauts, nouveaux chemins, nage, deplacements directs */
	FString WalkStats();
	/** Une image de marche. bBusy : frayeur, entree bloquee, chargement (le temps ne compte pas pour le blocage) */
	EWalk StepWalk(const ABRWorld* W, ABRCharacter* C, float Dt, double Timeout, bool bBusy);
}
