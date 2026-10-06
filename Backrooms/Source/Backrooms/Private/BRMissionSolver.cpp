// v4.11 : solveur des missions par les seules informations visibles (voir BRMissionSolver.h).
#include "BRMissionSolver.h"

namespace BRMission
{
	namespace SolverImpl
	{
		bool SameState(const FState& A, const FState& B)
		{
			for (int I = 0; I < MaxDevices; ++I)
			{
				if (A.Dev[I] != B.Dev[I])
				{
					return false;
				}
			}
			return A.Solved == B.Solved && A.Mistakes == B.Mistakes;
		}

		bool DoneOrAlready(const FResult& R)
		{
			return R.Feedback == EFeedback::Done || R.Feedback == EFeedback::AlreadyDone;
		}
	}

	int FindRole(const FPlan& P, int Role, int* Out, int Max)
	{
		int N = 0;
		for (int D = 0; D < P.NumDevices && N < Max; ++D)
		{
			if (P.Devices[D].Role == Role)
			{
				Out[N++] = D;
			}
		}
		return N;
	}

	int FirstRole(const FPlan& P, int Role)
	{
		int D = -1;
		return FindRole(P, Role, &D, 1) == 1 ? D : -1;
	}

	int FindRoleLabel(const FPlan& P, int Role, int Label)
	{
		for (int D = 0; D < P.NumDevices; ++D)
		{
			if (P.Devices[D].Role == Role && P.Devices[D].Label == Label)
			{
				return D;
			}
		}
		return -1;
	}

	int ForwardTarget(int Level, int Route)
	{
		switch (Level)
		{
		case 0: return Route == 0 ? 1 : 37;
		case 1: return 4;
		case 2: return 3;
		case 3: return 4;
		case 4: return 5;
		case 5: return 6;
		case 6: return 8;
		case 8: return 9;
		case 9: return 10;
		case 10: return 11;
		case 11: return EndingTarget;
		case 37: return 4;
		default: return -1;
		}
	}

	FResult FSolver::Do(int Dev, EAction Action, int Value)
	{
		if (Dev < 0 || Dev >= P.NumDevices)
		{
			bBad = true;
			Why = "mecanisme absent du plan";
			return FResult();
		}
		++Actions;
		const FState Before = S;
		const FResult R = Act(P, S, C, Dev, Action, Value);
		// Un refus ne change rien (sauf le compteur d'erreurs)
		if (!R.bChanged)
		{
			FState Cmp = S;
			Cmp.Mistakes = Before.Mistakes;
			if (!SolverImpl::SameState(Cmp, Before))
			{
				bBad = true;
				Why = "etat modifie sans bChanged";
			}
		}
		// Une mission resolue le reste
		if ((Before.Solved & 1) && !(S.Solved & 1))
		{
			bBad = true;
			Why = "mission deresolue";
		}
		if (bRecord)
		{
			if (NumSteps < MaxSolveSteps)
			{
				FSolveStep& St = Steps[NumSteps++];
				St.Device = static_cast<uint8_t>(Dev);
				St.Action = static_cast<uint8_t>(Action);
				St.Feedback = static_cast<uint8_t>(R.Feedback);
			}
			else
			{
				bOverflow = true;
			}
		}
		return R;
	}

	bool FSolver::Read(int Dev, FClue& Out)
	{
		const FResult R = Do(Dev, EAction::Use, 0);
		if (!SolverImpl::DoneOrAlready(R))
		{
			return false;
		}
		return GetClue(P, S, Dev, Out);
	}

	bool FSolver::HoldUntilDone(int Dev)
	{
		for (int I = 0; I < 40; ++I)
		{
			const FResult R = Do(Dev, EAction::Hold, 1);
			if (SolverImpl::DoneOrAlready(R))
			{
				return true;
			}
			if (R.Feedback != EFeedback::Progress)
			{
				return false;
			}
		}
		return false;
	}

	bool FSolver::SetSwitch(int Dev, int Value)
	{
		if (Dev < 0 || Dev >= P.NumDevices)
		{
			return false;
		}
		if (!bPlayerActions)
		{
			const FResult R = Do(Dev, EAction::Set, Value);
			return R.bChanged || R.Feedback == EFeedback::AlreadyDone;
		}
		// Comme dans le jeu : chaque appui passe a la position suivante
		const FDevice& D = P.Devices[Dev];
		if (D.Kind != EKind::Switch || Value < 0 || Value >= D.Positions)
		{
			return false;
		}
		for (int I = 0; I < D.Positions && S.Dev[Dev] != Value; ++I)
		{
			if (!Do(Dev, EAction::Use, 0).bChanged)
			{
				return false;
			}
		}
		return S.Dev[Dev] == Value;
	}

	bool FSolver::Solve(int Route)
	{
		FClue Cl;
		int List[MaxDevices];
		int N = 0;
		switch (P.Level)
		{
		case 0:
		{
			int Blinks[NumSymbols];
			for (int I = 0; I < NumSymbols; ++I)
			{
				Blinks[I] = -1;
			}
			N = FindRole(P, R_Anomaly, List, MaxDevices);
			for (int I = 0; I < N; ++I)
			{
				if (!HoldUntilDone(List[I]) || !GetClue(P, S, List[I], Cl) || Cl.Kind != EClue::AnomalyBlinks || Cl.A[0] >= NumSymbols)
				{
					return false;
				}
				Blinks[Cl.A[0]] = Cl.B[0];
			}
			Read(FirstRole(P, R_MaintNote), Cl);
			N = FindRole(P, R_Dial, List, MaxDevices);
			for (int I = 0; I < N; ++I)
			{
				const int Sym = P.Devices[List[I]].Label;
				if (Sym >= NumSymbols || Blinks[Sym] < 0 || !SetSwitch(List[I], Blinks[Sym]))
				{
					return false;
				}
			}
			if (!SolverImpl::DoneOrAlready(Do(FirstRole(P, R_Stabilize), EAction::Use, 0)))
			{
				return false;
			}
			SetSwitch(FirstRole(P, R_RouteLever), Route);
			return true;
		}
		case 1:
		{
			if (!Read(FirstRole(P, R_Schematic), Cl))
			{
				return false;
			}
			const int E1 = Cl.A[0];
			const int E2 = Cl.A[1];
			N = FindRole(P, R_Fuse, List, MaxDevices);
			for (int I = 0; I < N; ++I)
			{
				Do(List[I], EAction::Use, 0);
			}
			if (!SolverImpl::DoneOrAlready(Do(FindRoleLabel(P, R_FuseSocket, E1), EAction::Use, 0))
				|| !SolverImpl::DoneOrAlready(Do(FindRoleLabel(P, R_FuseSocket, E2), EAction::Use, 0)))
			{
				return false;
			}
			N = FindRole(P, R_Breaker, List, MaxDevices);
			for (int I = 0; I < N; ++I)
			{
				SetSwitch(List[I], 0);
			}
			if (!SetSwitch(FindRoleLabel(P, R_Breaker, E1), 1) || !SetSwitch(FindRoleLabel(P, R_Breaker, E2), 1))
			{
				return false;
			}
			return SolverImpl::DoneOrAlready(Do(FirstRole(P, R_ElevatorCall), EAction::Use, 0));
		}
		case 2:
		{
			if (!Read(FirstRole(P, R_PressurePlate), Cl))
			{
				return false;
			}
			int Target[3] = { -1, -1, -1 };
			for (int G = 0; G < 3; ++G)
			{
				if (Cl.A[G] < 3)
				{
					Target[Cl.A[G]] = Cl.B[G];
				}
			}
			N = FindRole(P, R_Gauge, List, MaxDevices);
			for (int I = 0; I < N; ++I)
			{
				FClue G;
				if (!HoldUntilDone(List[I]) || !GetClue(P, S, List[I], G) || G.A[0] >= 3 || Target[G.A[0]] < 0)
				{
					return false;
				}
				if (!SetSwitch(FindRoleLabel(P, R_Valve, G.B[0]), Target[G.A[0]]))
				{
					return false;
				}
				// Le manometre affiche la pression voulue
				if (GaugeReading(P, S, List[I]) != Target[G.A[0]])
				{
					return false;
				}
			}
			return true;
		}
		case 3:
		{
			FClue Board;
			if (!Read(FirstRole(P, R_LoadBoard), Board))
			{
				return false;
			}
			int Faulty = -1;
			N = FindRole(P, R_JunctionBox, List, MaxDevices);
			for (int I = 0; I < N; ++I)
			{
				if (!HoldUntilDone(List[I]) || !GetClue(P, S, List[I], Cl))
				{
					return false;
				}
				if (Cl.B[0])
				{
					Faulty = Cl.A[0];
				}
			}
			if (Faulty < 0)
			{
				return false;
			}
			// Relais dont le secteur (tableau de charge) n'est pas en defaut
			for (int I = 0; I < Board.N; ++I)
			{
				if (Board.B[I] != Faulty && !SetSwitch(FindRoleLabel(P, R_Relay, Board.A[I]), 1))
				{
					return false;
				}
			}
			return SolverImpl::DoneOrAlready(Do(FirstRole(P, R_ElevatorPower), EAction::Use, 0));
		}
		case 4:
		{
			FClue Plan;
			FClue Dir;
			if (!Read(FirstRole(P, R_Planning), Plan) || !Read(FirstRole(P, R_Directory), Dir))
			{
				return false;
			}
			int Office[NumSymbols];
			for (int I = 0; I < NumSymbols; ++I)
			{
				Office[I] = -1;
			}
			for (int I = 0; I < Dir.N; ++I)
			{
				if (Dir.A[I] < NumSymbols)
				{
					Office[Dir.A[I]] = Dir.B[I];
				}
			}
			for (int I = 0; I < 3; ++I)
			{
				if (Plan.A[I] >= NumSymbols || Office[Plan.A[I]] < 0 || !SetSwitch(FindRoleLabel(P, R_CodeDial, I), Office[Plan.A[I]]))
				{
					return false;
				}
			}
			return SolverImpl::DoneOrAlready(Do(FirstRole(P, R_CodeEnter), EAction::Use, 0));
		}
		case 5:
		{
			FClue Reg;
			FClue Boil;
			if (!Read(FirstRole(P, R_Register), Reg))
			{
				return false;
			}
			int Keys[MaxDevices];
			const int NumKeys = FindRole(P, R_Key, Keys, MaxDevices);
			int Locks[MaxDevices];
			const int NumLocks = FindRole(P, R_Lock, Locks, MaxDevices);
			// Chambres encore utiles : celles des serrures fermees (symbole de la serrure -> chambre du registre)
			int Wanted[8];
			int NumWanted = 0;
			for (int L = 0; L < NumLocks; ++L)
			{
				if (S.Dev[Locks[L]])
				{
					continue;
				}
				for (int I = 0; I < Reg.N && NumWanted < 8; ++I)
				{
					if (Reg.A[I] == P.Devices[Locks[L]].Label)
					{
						Wanted[NumWanted++] = Reg.B[I];
					}
				}
			}
			auto IsWanted = [&](int Room)
			{
				for (int I = 0; I < NumWanted; ++I)
				{
					if (Wanted[I] == Room)
					{
						return true;
					}
				}
				return false;
			};
			// Rendre les cles inutiles du trousseau, prendre les bonnes
			for (int K = 0; K < NumKeys; ++K)
			{
				GetClue(P, S, Keys[K], Cl);
				if (S.Dev[Keys[K]] && Held(P, S, P.Devices[Keys[K]].Need) > 0 && !IsWanted(Cl.A[0]))
				{
					if (Do(Keys[K], EAction::Use, 0).Feedback != EFeedback::Returned)
					{
						return false;
					}
				}
			}
			for (int K = 0; K < NumKeys; ++K)
			{
				GetClue(P, S, Keys[K], Cl);
				if (!S.Dev[Keys[K]] && IsWanted(Cl.A[0]))
				{
					if (Do(Keys[K], EAction::Use, 0).Feedback != EFeedback::Done)
					{
						return false;
					}
				}
			}
			for (int L = 0; L < NumLocks; ++L)
			{
				if (!SolverImpl::DoneOrAlready(Do(Locks[L], EAction::Use, 0)))
				{
					return false;
				}
			}
			if (!Read(FirstRole(P, R_BoilerNote), Boil))
			{
				return false;
			}
			return SetSwitch(FirstRole(P, R_BoilerDial), Boil.A[0]);
		}
		case 6:
		{
			if (!Read(FirstRole(P, R_StartPlate), Cl))
			{
				return false;
			}
			int Sym = Cl.A[0];
			for (int Step = 0; Step < 4; ++Step)
			{
				const int B = FindRoleLabel(P, R_Beacon, Sym);
				if (B < 0 || !HoldUntilDone(B) || !GetClue(P, S, B, Cl))
				{
					return false;
				}
				if (Cl.B[0] == 254)
				{
					break;
				}
				if (Cl.B[0] == 255)
				{
					return false;
				}
				Sym = Cl.B[0];
			}
			return SolverImpl::DoneOrAlready(Do(FirstRole(P, R_EmergencyPower), EAction::Use, 0));
		}
		case 8:
		{
			if (!Read(FirstRole(P, R_PassageMarks), Cl))
			{
				return false;
			}
			for (int I = 0; I < 3; ++I)
			{
				if (!HoldUntilDone(FindRoleLabel(P, R_Winch, Cl.A[I])))
				{
					return false;
				}
			}
			return true;
		}
		case 9:
		{
			int Circuit = -1;
			int Porch = -1;
			int Windows = -1;
			N = FindRole(P, R_HousePlan, List, MaxDevices);
			for (int I = 0; I < N; ++I)
			{
				if (!Read(List[I], Cl))
				{
					return false;
				}
				if (Cl.Kind == EClue::CircuitPlan) Circuit = Cl.A[0];
				if (Cl.Kind == EClue::PorchPlan) Porch = Cl.A[0];
				if (Cl.Kind == EClue::WindowsPlan) Windows = Cl.A[0];
			}
			if (Circuit < 0 || Porch < 0 || Windows < 0 || !SetSwitch(FirstRole(P, R_StreetBox), Circuit))
			{
				return false;
			}
			int Found = 0;
			N = FindRole(P, R_HouseMarker, List, MaxDevices);
			for (int I = 0; I < N; ++I)
			{
				GetClue(P, S, List[I], Cl);
				if (Cl.A[0] == Porch && Cl.B[0] == Windows)
				{
					++Found;
					if (!SolverImpl::DoneOrAlready(Do(List[I], EAction::Use, 0)))
					{
						return false;
					}
				}
			}
			return Found == 1;
		}
		case 10:
		{
			FClue Fence;
			FClue Board;
			const int F = FirstRole(P, R_FenceMark);
			if (!HoldUntilDone(F) || !GetClue(P, S, F, Fence) || !Read(FirstRole(P, R_BarnBoard), Board))
			{
				return false;
			}
			for (int I = 0; I < Board.N; ++I)
			{
				if (Board.A[I] == Fence.A[0])
				{
					if (!SetSwitch(FirstRole(P, R_MillDial), Board.B[I]))
					{
						return false;
					}
					return SolverImpl::DoneOrAlready(Do(FirstRole(P, R_MillBrake), EAction::Use, 0));
				}
			}
			return false;
		}
		case 11:
		{
			if (!HoldUntilDone(FirstRole(P, R_Generator)))
			{
				return false;
			}
			for (int I = 0; I < 3; ++I)
			{
				int Digit = KnownDigit(P, S, C, I);
				if (Digit < 0)
				{
					if (!Read(FindRoleLabel(P, R_CityBoard, I), Cl))
					{
						return false;
					}
					Digit = Cl.B[0];
				}
				if (!SetSwitch(FindRoleLabel(P, R_DestDial, I), Digit))
				{
					return false;
				}
			}
			return SolverImpl::DoneOrAlready(Do(FirstRole(P, R_DestConfirm), EAction::Use, 0));
		}
		case 37:
		{
			FClue Marks;
			if (!Read(FirstRole(P, R_LevelMarks), Marks) || !HoldUntilDone(FirstRole(P, R_Current)))
			{
				return false;
			}
			// Regle du courant : A se deverse dans B (vanne A), B se vide (vanne B). Vannes fermees, on lit le depart de B.
			int Sl[2] = { -1, -1 };
			if (FindRole(P, R_Sluice, Sl, 2) != 2 || !SetSwitch(Sl[0], 0) || !SetSwitch(Sl[1], 0))
			{
				return false;
			}
			int A = 0;
			int B0 = 0;
			PoolLevels(P, S, A, B0);
			const int X0 = A - Marks.B[0];
			const int Y0 = B0 + X0 - Marks.B[1];
			return SetSwitch(Sl[0], X0) && SetSwitch(Sl[1], Y0);
		}
		default:
			return false;
		}
	}

	bool Replay(const FPlan& P, FState& S, const FCampaign& C, const FSolveStep* Steps, int NumSteps)
	{
		for (int I = 0; I < NumSteps; ++I)
		{
			const FResult R = Act(P, S, C, Steps[I].Device, static_cast<EAction>(Steps[I].Action), Steps[I].Action == static_cast<uint8_t>(EAction::Hold) ? 1 : 0);
			if (static_cast<uint8_t>(R.Feedback) != Steps[I].Feedback)
			{
				return false;
			}
		}
		return true;
	}
}
