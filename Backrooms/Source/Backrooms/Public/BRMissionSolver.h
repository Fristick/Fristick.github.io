// v4.11 : solveur des missions qui n'utilise que ce que le joueur voit (inscriptions, indices lus, observations,
// jauges, retours), jamais les parametres tires. Partage par le banc hors moteur (Tools/Missions/test_mission_logic.cpp)
// et le test automatique du jeu (BRAutoTestV411.cpp), qui rejoue ses actions par de vraies interactions.
// C++ pur, sans dependance au moteur ni a la bibliotheque standard (comme BRMissionLogic).
#pragma once

#include "BRMissionLogic.h"

namespace BRMission
{
	/** Actions enregistrees au plus (une mission en demande moins de 200 en mode joueur) */
	constexpr int MaxSolveSteps = 600;

	/** Une action enregistree : mecanisme, action (EAction), retour obtenu (EFeedback) */
	struct FSolveStep
	{
		uint8_t Device = 0;
		uint8_t Action = 0;
		uint8_t Feedback = 0;
	};

	class FSolver
	{
	public:
		FSolver(const FPlan& InPlan, FState& InState, const FCampaign& InCampaign) : P(InPlan), S(InState), C(InCampaign) {}

		const FPlan& P;
		FState& S;
		FCampaign C;
		/** Mode joueur : un reglage devient des appuis successifs (Use), comme dans le jeu (pas d'action Set) */
		bool bPlayerActions = false;
		/** Enregistre chaque action (Steps) */
		bool bRecord = false;
		/** Actions jouees */
		int Actions = 0;
		/** Invariant viole : etat change sans bChanged, mission deresolue */
		bool bBad = false;
		const char* Why = "";
		FSolveStep Steps[MaxSolveSteps];
		int NumSteps = 0;
		/** Plus de MaxSolveSteps actions enregistrees */
		bool bOverflow = false;

		/** Joue une action (verifie les invariants, enregistre si demande) */
		FResult Do(int Dev, EAction Action, int Value);
		/** Lit un indice (Use), puis son contenu */
		bool Read(int Dev, FClue& Out);
		/** Manivelle ou observation jusqu'au bout */
		bool HoldUntilDone(int Dev);
		/** Interrupteur sur une position (mode joueur : appuis successifs) */
		bool SetSwitch(int Dev, int Value);
		/** Resout la mission du plan ; Route : niveau 0 (0 : Niveau 1, 1 : Poolrooms). false : bloque */
		bool Solve(int Route);
	};

	/** Mecanismes d'un role (indices dans le plan) ; retour : nombre trouve */
	int FindRole(const FPlan& P, int Role, int* Out, int Max);
	/** Premier mecanisme d'un role, -1 s'il n'y en a pas */
	int FirstRole(const FPlan& P, int Role);
	/** Mecanisme d'un role portant une inscription, -1 s'il n'y en a pas */
	int FindRoleLabel(const FPlan& P, int Role, int Label);
	/** Destination de la sortie ouverte par la mission du niveau (Route : niveau 0) ; -1 sans mission */
	int ForwardTarget(int Level, int Route);
	/** Rejoue des actions enregistrees sur un etat : true si chaque retour est identique a l'enregistrement */
	bool Replay(const FPlan& P, FState& S, const FCampaign& C, const FSolveStep* Steps, int NumSteps);
}
