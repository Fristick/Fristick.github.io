// v4.11 : banc d'essai hors moteur de la logique des missions (Source/Backrooms/Private/BRMissionLogic.cpp).
//
// Compilation et lancement (depuis Backrooms/) :
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -I Source/Backrooms/Public
//       Tools/Missions/test_mission_logic.cpp Source/Backrooms/Private/BRMissionLogic.cpp -o /tmp/test_missions
//   /tmp/test_missions [graines par niveau, 3000 par defaut]
//
// Pour chaque niveau et chaque graine :
//   - plan deterministe, appareils et zones valides, aucun objet requis dans la salle qu'il ouvre ;
//   - un solveur qui n'utilise QUE ce que le joueur voit (inscriptions, indices lus, observations, jauges, retours)
//     resout la mission par de vraies actions et ouvre la sortie ;
//   - apres une suite d'actions au hasard (erreurs comprises), l'etat reste valide, se serialise a l'identique, et le
//     solveur resout encore la mission : aucune impasse ;
//   - les erreurs typiques donnent un retour comprehensible et se corrigent ;
//   - la variante de secours est valide ; un blob d'une autre graine ou tronque est refuse.
// Code de sortie 0 si tout passe, 1 sinon. La sortie sert de journal (Docs/logs).
#include "BRMissionLogic.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace BRMission;

static const int Levels[] = { 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 37 };
static int Failures = 0;
static std::map<std::string, int> FailureKinds;

static void Fail(int Level, uint32_t Seed, const std::string& Why)
{
	++Failures;
	if (FailureKinds[Why]++ < 3)
	{
		std::printf("ECHEC niveau %d graine %u : %s\n", Level, Seed, Why.c_str());
	}
}

struct FCtx
{
	FCtx(const FPlan& InP, FState& InS, const FCampaign& InC) : P(InP), S(InS), C(InC) {}
	const FPlan& P;
	FState& S;
	FCampaign C;
	int Actions = 0;
	bool bBad = false;
	std::string Why;
};

static bool SameState(const FState& A, const FState& B)
{
	return std::memcmp(A.Dev, B.Dev, sizeof(A.Dev)) == 0 && A.Solved == B.Solved && A.Mistakes == B.Mistakes;
}

static FResult Do(FCtx& X, int Dev, EAction A, int V)
{
	++X.Actions;
	const FState Before = X.S;
	const FResult R = Act(X.P, X.S, X.C, Dev, A, V);
	// Un refus ne change rien (sauf le compteur d'erreurs)
	if (!R.bChanged)
	{
		FState Cmp = X.S;
		Cmp.Mistakes = Before.Mistakes;
		if (!SameState(Cmp, Before))
		{
			X.bBad = true;
			X.Why = "etat modifie sans bChanged";
		}
	}
	// Une mission resolue le reste
	if ((Before.Solved & 1) && !(X.S.Solved & 1))
	{
		X.bBad = true;
		X.Why = "mission deresolue";
	}
	return R;
}

static std::vector<int> FindRole(const FPlan& P, int Role)
{
	std::vector<int> Out;
	for (int D = 0; D < P.NumDevices; ++D)
	{
		if (P.Devices[D].Role == Role)
		{
			Out.push_back(D);
		}
	}
	return Out;
}

static int FindRoleLabel(const FPlan& P, int Role, int Label)
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

static bool ReadDev(FCtx& X, int D, FClue& C)
{
	const FResult R = Do(X, D, EAction::Use, 0);
	if (R.Feedback != EFeedback::Done && R.Feedback != EFeedback::AlreadyDone)
	{
		return false;
	}
	return GetClue(X.P, X.S, D, C);
}

static bool HoldUntilDone(FCtx& X, int D)
{
	for (int I = 0; I < 40; ++I)
	{
		const FResult R = Do(X, D, EAction::Hold, 1);
		if (R.Feedback == EFeedback::Done || R.Feedback == EFeedback::AlreadyDone)
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

static bool SetSwitch(FCtx& X, int D, int V)
{
	const FResult R = Do(X, D, EAction::Set, V);
	return R.bChanged || R.Feedback == EFeedback::AlreadyDone;
}

/** Solveur : seules les informations visibles du joueur sont utilisees (jamais Params ni Info) */
static bool Solve(FCtx& X, int Route)
{
	const FPlan& P = X.P;
	FClue C;
	switch (P.Level)
	{
	case 0:
	{
		int Blinks[NumSymbols];
		for (int I = 0; I < NumSymbols; ++I) Blinks[I] = -1;
		for (int D : FindRole(P, R_Anomaly))
		{
			if (!HoldUntilDone(X, D) || !GetClue(P, X.S, D, C) || C.Kind != EClue::AnomalyBlinks) return false;
			Blinks[C.A[0]] = C.B[0];
		}
		ReadDev(X, FindRole(P, R_MaintNote)[0], C);
		for (int D : FindRole(P, R_Dial))
		{
			if (Blinks[P.Devices[D].Label] < 0 || !SetSwitch(X, D, Blinks[P.Devices[D].Label])) return false;
		}
		const FResult R = Do(X, FindRole(P, R_Stabilize)[0], EAction::Use, 0);
		if (R.Feedback != EFeedback::Done && R.Feedback != EFeedback::AlreadyDone) return false;
		SetSwitch(X, FindRole(P, R_RouteLever)[0], Route);
		return true;
	}
	case 1:
	{
		if (!ReadDev(X, FindRole(P, R_Schematic)[0], C)) return false;
		const int E1 = C.A[0], E2 = C.A[1];
		for (int D : FindRole(P, R_Fuse))
		{
			Do(X, D, EAction::Use, 0);
		}
		for (int L : { E1, E2 })
		{
			const FResult R = Do(X, FindRoleLabel(P, R_FuseSocket, L), EAction::Use, 0);
			if (R.Feedback != EFeedback::Done && R.Feedback != EFeedback::AlreadyDone) return false;
		}
		for (int D : FindRole(P, R_Breaker))
		{
			SetSwitch(X, D, 0);
		}
		if (!SetSwitch(X, FindRoleLabel(P, R_Breaker, E1), 1) || !SetSwitch(X, FindRoleLabel(P, R_Breaker, E2), 1)) return false;
		const FResult R = Do(X, FindRole(P, R_ElevatorCall)[0], EAction::Use, 0);
		return R.Feedback == EFeedback::Done || R.Feedback == EFeedback::AlreadyDone;
	}
	case 2:
	{
		if (!ReadDev(X, FindRole(P, R_PressurePlate)[0], C)) return false;
		int Target[3];
		for (int G = 0; G < 3; ++G) Target[C.A[G]] = C.B[G];
		for (int D : FindRole(P, R_Gauge))
		{
			FClue G;
			if (!HoldUntilDone(X, D) || !GetClue(P, X.S, D, G)) return false;
			if (!SetSwitch(X, FindRoleLabel(P, R_Valve, G.B[0]), Target[G.A[0]])) return false;
			// Le manometre affiche la pression voulue
			if (GaugeReading(P, X.S, D) != Target[G.A[0]]) return false;
		}
		return true;
	}
	case 3:
	{
		FClue Board;
		if (!ReadDev(X, FindRole(P, R_LoadBoard)[0], Board)) return false;
		int Faulty = -1;
		for (int D : FindRole(P, R_JunctionBox))
		{
			if (!HoldUntilDone(X, D) || !GetClue(P, X.S, D, C)) return false;
			if (C.B[0]) Faulty = C.A[0];
		}
		if (Faulty < 0) return false;
		// Relais dont le secteur (tableau de charge) n'est pas en defaut
		for (int I = 0; I < Board.N; ++I)
		{
			if (Board.B[I] != Faulty && !SetSwitch(X, FindRoleLabel(P, R_Relay, Board.A[I]), 1)) return false;
		}
		const FResult R = Do(X, FindRole(P, R_ElevatorPower)[0], EAction::Use, 0);
		return R.Feedback == EFeedback::Done || R.Feedback == EFeedback::AlreadyDone;
	}
	case 4:
	{
		FClue Plan, Dir;
		if (!ReadDev(X, FindRole(P, R_Planning)[0], Plan) || !ReadDev(X, FindRole(P, R_Directory)[0], Dir)) return false;
		int Office[NumSymbols];
		for (int I = 0; I < Dir.N; ++I) Office[Dir.A[I]] = Dir.B[I];
		for (int I = 0; I < 3; ++I)
		{
			if (!SetSwitch(X, FindRoleLabel(P, R_CodeDial, I), Office[Plan.A[I]])) return false;
		}
		const FResult R = Do(X, FindRole(P, R_CodeEnter)[0], EAction::Use, 0);
		return R.Feedback == EFeedback::Done || R.Feedback == EFeedback::AlreadyDone;
	}
	case 5:
	{
		FClue Reg, Boil;
		if (!ReadDev(X, FindRole(P, R_Register)[0], Reg)) return false;
		const std::vector<int> Keys = FindRole(P, R_Key);
		const std::vector<int> Locks = FindRole(P, R_Lock);
		// Chambres encore utiles : celles des serrures fermees (symbole de la serrure -> chambre du registre)
		std::set<int> Wanted;
		for (int L : Locks)
		{
			if (X.S.Dev[L]) continue;
			for (int I = 0; I < Reg.N; ++I)
			{
				if (Reg.A[I] == P.Devices[L].Label) Wanted.insert(Reg.B[I]);
			}
		}
		// Rendre les cles inutiles du trousseau, prendre les bonnes
		for (int K : Keys)
		{
			GetClue(P, X.S, K, C);
			if (X.S.Dev[K] && Held(P, X.S, P.Devices[K].Need) > 0 && !Wanted.count(C.A[0]))
			{
				if (Do(X, K, EAction::Use, 0).Feedback != EFeedback::Returned) return false;
			}
		}
		for (int K : Keys)
		{
			GetClue(P, X.S, K, C);
			if (!X.S.Dev[K] && Wanted.count(C.A[0]))
			{
				if (Do(X, K, EAction::Use, 0).Feedback != EFeedback::Done) return false;
			}
		}
		for (int L : Locks)
		{
			const FResult R = Do(X, L, EAction::Use, 0);
			if (R.Feedback != EFeedback::Done && R.Feedback != EFeedback::AlreadyDone) return false;
		}
		if (!ReadDev(X, FindRole(P, R_BoilerNote)[0], Boil)) return false;
		return SetSwitch(X, FindRole(P, R_BoilerDial)[0], Boil.A[0]);
	}
	case 6:
	{
		if (!ReadDev(X, FindRole(P, R_StartPlate)[0], C)) return false;
		int Sym = C.A[0];
		for (int Step = 0; Step < 4; ++Step)
		{
			const int B = FindRoleLabel(P, R_Beacon, Sym);
			if (B < 0 || !HoldUntilDone(X, B) || !GetClue(P, X.S, B, C)) return false;
			if (C.B[0] == 254) break;
			if (C.B[0] == 255) return false;
			Sym = C.B[0];
		}
		const FResult R = Do(X, FindRole(P, R_EmergencyPower)[0], EAction::Use, 0);
		return R.Feedback == EFeedback::Done || R.Feedback == EFeedback::AlreadyDone;
	}
	case 8:
	{
		if (!ReadDev(X, FindRole(P, R_PassageMarks)[0], C)) return false;
		for (int I = 0; I < 3; ++I)
		{
			if (!HoldUntilDone(X, FindRoleLabel(P, R_Winch, C.A[I]))) return false;
		}
		return true;
	}
	case 9:
	{
		int Circuit = -1, Porch = -1, Windows = -1;
		for (int D : FindRole(P, R_HousePlan))
		{
			if (!ReadDev(X, D, C)) return false;
			if (C.Kind == EClue::CircuitPlan) Circuit = C.A[0];
			if (C.Kind == EClue::PorchPlan) Porch = C.A[0];
			if (C.Kind == EClue::WindowsPlan) Windows = C.A[0];
		}
		if (Circuit < 0 || Porch < 0 || Windows < 0 || !SetSwitch(X, FindRole(P, R_StreetBox)[0], Circuit)) return false;
		int Found = 0;
		for (int D : FindRole(P, R_HouseMarker))
		{
			GetClue(P, X.S, D, C);
			if (C.A[0] == Porch && C.B[0] == Windows)
			{
				++Found;
				const FResult R = Do(X, D, EAction::Use, 0);
				if (R.Feedback != EFeedback::Done && R.Feedback != EFeedback::AlreadyDone) return false;
			}
		}
		return Found == 1;
	}
	case 10:
	{
		FClue Fence, Board;
		const int F = FindRole(P, R_FenceMark)[0];
		if (!HoldUntilDone(X, F) || !GetClue(P, X.S, F, Fence) || !ReadDev(X, FindRole(P, R_BarnBoard)[0], Board)) return false;
		for (int I = 0; I < Board.N; ++I)
		{
			if (Board.A[I] == Fence.A[0])
			{
				if (!SetSwitch(X, FindRole(P, R_MillDial)[0], Board.B[I])) return false;
				const FResult R = Do(X, FindRole(P, R_MillBrake)[0], EAction::Use, 0);
				return R.Feedback == EFeedback::Done || R.Feedback == EFeedback::AlreadyDone;
			}
		}
		return false;
	}
	case 11:
	{
		if (!HoldUntilDone(X, FindRole(P, R_Generator)[0])) return false;
		for (int I = 0; I < 3; ++I)
		{
			int Digit = KnownDigit(P, X.S, X.C, I);
			if (Digit < 0)
			{
				if (!ReadDev(X, FindRoleLabel(P, R_CityBoard, I), C)) return false;
				Digit = C.B[0];
			}
			if (!SetSwitch(X, FindRoleLabel(P, R_DestDial, I), Digit)) return false;
		}
		const FResult R = Do(X, FindRole(P, R_DestConfirm)[0], EAction::Use, 0);
		return R.Feedback == EFeedback::Done || R.Feedback == EFeedback::AlreadyDone;
	}
	case 37:
	{
		FClue Marks;
		if (!ReadDev(X, FindRole(P, R_LevelMarks)[0], Marks) || !HoldUntilDone(X, FindRole(P, R_Current)[0])) return false;
		// Regle du courant : A se deverse dans B (vanne A), B se vide (vanne B). Vannes fermees, on lit le depart de B.
		const std::vector<int> Sl = FindRole(P, R_Sluice);
		if (!SetSwitch(X, Sl[0], 0) || !SetSwitch(X, Sl[1], 0)) return false;
		int A = 0, B0 = 0;
		PoolLevels(P, X.S, A, B0);
		const int X0 = A - Marks.B[0];
		const int Y0 = B0 + X0 - Marks.B[1];
		return SetSwitch(X, Sl[0], X0) && SetSwitch(X, Sl[1], Y0);
	}
	default:
		return false;
	}
}

static int ForwardTarget(int Level, int Route)
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

static bool StateValid(const FPlan& P, const FState& S, std::string& Why)
{
	for (int D = 0; D < P.NumDevices; ++D)
	{
		const FDevice& Dev = P.Devices[D];
		const int V = S.Dev[D];
		bool bOk = true;
		switch (Dev.Kind)
		{
		case EKind::Switch: bOk = V < Dev.Positions; break;
		case EKind::Observe:
		case EKind::Crank: bOk = V <= Dev.Positions; break;
		case EKind::Clue:
		case EKind::Socket:
		case EKind::Item: bOk = V <= 1; break;
		case EKind::Gate:
		case EKind::Button: bOk = V == 0; break;
		}
		if (!bOk)
		{
			Why = "valeur hors bornes";
			return false;
		}
	}
	return true;
}

static bool CheckPlanShape(const FPlan& P, std::string& Why)
{
	if (P.NumDevices <= 0 || P.NumDevices > MaxDevices || P.NumSteps < 2 || P.NumSteps > MaxSteps)
	{
		Why = "plan vide ou etapes hors bornes";
		return false;
	}
	for (int D = 0; D < P.NumDevices; ++D)
	{
		const FDevice& Dev = P.Devices[D];
		if (Dev.Zone > 4 || Dev.Role == R_None || Dev.Role >= R_Count)
		{
			Why = "zone ou role invalide";
			return false;
		}
		// Un objet requis n'est jamais dans la salle de la mission (derriere ce qu'il ouvre)
		if (Dev.Kind == EKind::Item && Dev.Zone == 4)
		{
			Why = "objet requis dans la salle de mission";
			return false;
		}
		if (Dev.Kind == EKind::Switch && Dev.Positions < 2)
		{
			Why = "interrupteur a une position";
			return false;
		}
	}
	return true;
}

static bool Roundtrip(const FPlan& P, const FState& S)
{
	uint8_t Blob[MaxBlob];
	const int N = Serialize(P, S, Blob, sizeof(Blob));
	FState Back;
	return N > 0 && Deserialize(P, Blob, N, Back) && SameState(Back, S);
}

/** Erreurs typiques : retour attendu, puis correction (le solveur termine) */
static void MistakeScenarios(int Level, uint32_t Seed, int& Checked)
{
	FPlan P;
	BuildPlan(Level, Seed, P);
	FState S;
	InitState(P, S);
	FCtx X{ P, S, FCampaign() };
	FClue C;
	auto Expect = [&](const FResult& R, EFeedback F, const char* What)
	{
		++Checked;
		if (R.Feedback != F)
		{
			Fail(Level, Seed, std::string("retour attendu absent : ") + What);
		}
	};
	switch (Level)
	{
	case 0:
	{
		Expect(Do(X, FindRole(P, R_Stabilize)[0], EAction::Use, 0), EFeedback::Locked, "stabiliser sans observations");
		for (int D : FindRole(P, R_Anomaly)) HoldUntilDone(X, D);
		const FResult R = Do(X, FindRole(P, R_Stabilize)[0], EAction::Use, 0);
		Expect(R, EFeedback::Wrong, "cadrans au repos");
		if (R.Count == 0) Fail(Level, Seed, "nombre de cadrans faux absent");
		break;
	}
	case 1:
	{
		ReadDev(X, FindRole(P, R_Schematic)[0], C);
		Expect(Do(X, FindRoleLabel(P, R_Breaker, C.A[0]), EAction::Set, 1), EFeedback::NoFuse, "disjoncteur sans fusible");
		for (int D : FindRole(P, R_Fuse)) Do(X, D, EAction::Use, 0);
		for (int L = 0; L < 3; ++L) Do(X, FindRoleLabel(P, R_FuseSocket, C.A[L]), EAction::Use, 0);
		Do(X, FindRoleLabel(P, R_Breaker, C.A[2]), EAction::Set, 1);
		Do(X, FindRoleLabel(P, R_Breaker, C.A[0]), EAction::Set, 1);
		Expect(Do(X, FindRoleLabel(P, R_Breaker, C.A[1]), EAction::Set, 1), EFeedback::Overload, "troisieme circuit");
		Expect(Do(X, FindRole(P, R_ElevatorCall)[0], EAction::Use, 0), EFeedback::Wrong, "ascenseur sans ses deux circuits");
		// La reserve eclairee permet de lire sa note (objectif facultatif)
		Expect(Do(X, FindRole(P, R_ReserveNote)[0], EAction::Use, 0), EFeedback::Done, "note de la reserve eclairee");
		break;
	}
	case 2:
	{
		ReadDev(X, FindRole(P, R_PressurePlate)[0], C);
		bool bAllMax = true;
		for (int G = 0; G < 3; ++G) bAllMax &= C.B[G] == 4;
		for (int D : FindRole(P, R_Valve)) Do(X, D, EAction::Set, 4);
		FEval E;
		Evaluate(P, S, X.C, E);
		++Checked;
		if (!bAllMax && E.WarningDevice == 255) Fail(Level, Seed, "fuite non signalee");
		break;
	}
	case 3:
	{
		const std::vector<int> Relays = FindRole(P, R_Relay);
		for (int D : Relays) Do(X, D, EAction::Set, 1);
		bool bTrip = false;
		for (int D : Relays) bTrip |= S.Dev[D] == 0;
		if (!bTrip) Fail(Level, Seed, "relais en defaut sans disjonction");
		++Checked;
		break;
	}
	case 4:
		Expect(Do(X, FindRole(P, R_CodeEnter)[0], EAction::Use, 0), EFeedback::Locked, "code sans planning");
		ReadDev(X, FindRole(P, R_Planning)[0], C);
		ReadDev(X, FindRole(P, R_Directory)[0], C);
		Do(X, FindRoleLabel(P, R_CodeDial, 0), EAction::Set, 0);
		{
			const FResult R = Do(X, FindRole(P, R_CodeEnter)[0], EAction::Use, 0);
			++Checked;
			if (R.Feedback != EFeedback::Wrong && R.Feedback != EFeedback::Done) Fail(Level, Seed, "code faux sans retour");
		}
		break;
	case 5:
	{
		const std::vector<int> Keys = FindRole(P, R_Key);
		for (int I = 0; I < 3; ++I) Do(X, Keys[I], EAction::Use, 0);
		Expect(Do(X, Keys[3], EAction::Use, 0), EFeedback::Full, "quatrieme cle");
		Expect(Do(X, Keys[0], EAction::Use, 0), EFeedback::Returned, "cle remise");
		Expect(Do(X, FindRole(P, R_BoilerDial)[0], EAction::Set, 5), EFeedback::Locked, "chaufferie fermee");
		{
			// Deux cles portees pour trois serrures : au moins une serrure refuse (mauvaise cle ou plus de cle)
			int Refused = 0;
			for (int L : FindRole(P, R_Lock))
			{
				const EFeedback F = Do(X, L, EAction::Use, 0).Feedback;
				Refused += (F == EFeedback::WrongItem || F == EFeedback::NeedItem) ? 1 : 0;
			}
			++Checked;
			if (Refused == 0) Fail(Level, Seed, "serrure sans la bonne cle acceptee");
		}
		break;
	}
	case 6:
	{
		int Decoy = -1;
		for (int D : FindRole(P, R_Beacon))
		{
			if (GetClue(P, S, D, C) && C.B[0] == 255) Decoy = D;
		}
		if (Decoy < 0) Fail(Level, Seed, "balise leurre non signalee");
		else Expect(Do(X, Decoy, EAction::Hold, 1), EFeedback::Dead, "balise leurre");
		ReadDev(X, FindRole(P, R_StartPlate)[0], C);
		for (int D : FindRole(P, R_Beacon))
		{
			if (D != Decoy && P.Devices[D].Label != C.A[0])
			{
				Expect(Do(X, D, EAction::Hold, 1), EFeedback::Order, "balise hors ordre");
				break;
			}
		}
		break;
	}
	case 8:
	{
		ReadDev(X, FindRole(P, R_PassageMarks)[0], C);
		Expect(Do(X, FindRoleLabel(P, R_Winch, C.A[2]), EAction::Hold, 1), EFeedback::Order, "treuil hors ordre");
		for (int W = 0; W < 5; ++W)
		{
			if (W != C.A[0] && W != C.A[1] && W != C.A[2])
			{
				Expect(Do(X, FindRoleLabel(P, R_Winch, W), EAction::Hold, 1), EFeedback::Dead, "treuil grippe");
			}
		}
		break;
	}
	case 9:
		Expect(Do(X, FindRole(P, R_HouseMarker)[0], EAction::Use, 0), EFeedback::Locked, "maison sans courant");
		break;
	case 10:
	{
		const int Mill = FindRole(P, R_MillDial)[0];
		Expect(Do(X, FindRole(P, R_MillBrake)[0], EAction::Use, 0), EFeedback::Wrong, "moulin au repos");
		(void)Mill;
		break;
	}
	case 11:
		Expect(Do(X, FindRole(P, R_DestConfirm)[0], EAction::Use, 0), EFeedback::Locked, "station sans courant");
		break;
	case 37:
	{
		int A = 0, B = 0;
		PoolLevels(P, S, A, B);
		const FResult R = Do(X, FindRole(P, R_Sluice)[0], EAction::Set, 5);
		if (B + 5 >= 7) Expect(R, EFeedback::Warning, "bassin B qui deborde");
		break;
	}
	default:
		break;
	}
	if (X.bBad) Fail(Level, Seed, X.Why);
	// Toute erreur se corrige
	FCtx Y{ P, S, FCampaign() };
	if (!Solve(Y, 0) || !(S.Solved & 1)) Fail(Level, Seed, "correction impossible apres l'erreur");
}

int main(int argc, char** argv)
{
	const int Seeds = argc > 1 ? std::atoi(argv[1]) : 3000;
	std::printf("Banc d'essai des missions v4.11 : %d graines par niveau, generation %d\n", Seeds, GenVersion);
	long TotalActions = 0, TotalFuzz = 0;
	for (int Level : Levels)
	{
		int Solved = 0, Fallbacks = 0, Mistakes = 0, FuzzSolved = 0, Checked = 0;
		long Actions = 0;
		std::set<std::vector<uint8_t>> Variants;
		for (int I = 0; I < Seeds; ++I)
		{
			const uint32_t Seed = 0x1000u + (uint32_t)I * 2654435761u;
			FPlan P, P2;
			BuildPlan(Level, Seed, P);
			BuildPlan(Level, Seed, P2);
			std::string Why;
			if (Fingerprint(P) != Fingerprint(P2) || std::memcmp(P.Params, P2.Params, sizeof(P.Params)) != 0)
			{
				Fail(Level, Seed, "plan non deterministe");
			}
			if (!CheckPlanShape(P, Why))
			{
				Fail(Level, Seed, Why);
				continue;
			}
			Fallbacks += P.bFallback ? 1 : 0;
			Variants.insert(std::vector<uint8_t>(P.Params, P.Params + MaxParams));

			// 1. Resolution directe par les seules informations visibles
			for (int Route = 0; Route < (Level == 0 ? 2 : 1); ++Route)
			{
				FState S;
				InitState(P, S);
				FCtx X{ P, S, FCampaign() };
				FEval E;
				Evaluate(P, S, X.C, E);
				if (E.Steps[0] == EStep::Hidden) Fail(Level, Seed, "premiere etape cachee");
				const int Target = ForwardTarget(Level, Route);
				if (IsExitOpen(P, S, Target)) Fail(Level, Seed, "sortie ouverte avant la mission");
				if (!Solve(X, Route) || X.bBad)
				{
					Fail(Level, Seed, X.bBad ? X.Why : "solveur bloque");
					continue;
				}
				Evaluate(P, S, X.C, E);
				if (!E.bSolved || !IsExitOpen(P, S, Target)) Fail(Level, Seed, "sortie fermee apres resolution");
				if (Level == 0 && E.Route != Route) Fail(Level, Seed, "route du levier ignoree");
				if (Level == 0 && IsExitOpen(P, S, ForwardTarget(0, 1 - Route))) Fail(Level, Seed, "les deux routes ouvertes");
				for (int St = 0; St < E.NumSteps; ++St)
				{
					if (E.Steps[St] == EStep::Hidden) Fail(Level, Seed, "etape cachee apres resolution");
				}
				if (!Roundtrip(P, S)) Fail(Level, Seed, "serialisation de l'etat resolu");
				if (!StateValid(P, S, Why)) Fail(Level, Seed, Why);
				Actions += X.Actions;
				if (Route == 0) ++Solved;
			}

			// 2. Actions au hasard (erreurs, retours, ordre quelconque), puis resolution : aucune impasse
			{
				FState S;
				InitState(P, S);
				FCtx X{ P, S, FCampaign() };
				uint32_t R = Hash(Seed, 0xF022u + (uint32_t)Level);
				const int Steps = (int)(R % 80u);
				for (int K = 0; K < Steps; ++K)
				{
					R = Hash(R, (uint32_t)K);
					const int D = (int)(R % (uint32_t)P.NumDevices);
					const int Mode = (int)((R >> 8) % 10u);
					const EAction A = Mode < 6 ? EAction::Use : (Mode < 8 ? EAction::Set : EAction::Hold);
					const int V = (int)((R >> 16) % 11u) - (Mode == 7 ? 1 : 0);
					Do(X, D, A, V);
					if (!StateValid(P, S, Why)) { Fail(Level, Seed, "aleatoire : " + Why); break; }
					if (!Roundtrip(P, S)) { Fail(Level, Seed, "aleatoire : serialisation"); break; }
				}
				Mistakes += S.Mistakes;
				FCtx Y{ P, S, FCampaign() };
				if (!Solve(Y, 0) || Y.bBad || !(S.Solved & 1))
				{
					Fail(Level, Seed, Y.bBad ? "apres aleatoire : " + Y.Why : "impasse apres actions au hasard");
				}
				else
				{
					++FuzzSolved;
				}
				if (X.bBad) Fail(Level, Seed, "aleatoire : " + X.Why);
				TotalFuzz += Steps;
			}

			// 3. Serialisation : autre graine, autre version, blob tronque
			{
				FPlan Other;
				BuildPlan(Level, Seed ^ 0x5A5A5A5Au, Other);
				FState S;
				InitState(P, S);
				uint8_t Blob[MaxBlob];
				const int N = Serialize(P, S, Blob, sizeof(Blob));
				FState Back;
				if (Fingerprint(Other) != Fingerprint(P) && Deserialize(Other, Blob, N, Back)) Fail(Level, Seed, "blob d'une autre graine accepte");
				if (Deserialize(P, Blob, N - 1, Back)) Fail(Level, Seed, "blob tronque accepte");
				Blob[1] = (uint8_t)(GenVersion + 1);
				if (Deserialize(P, Blob, N, Back)) Fail(Level, Seed, "blob d'une autre version accepte");
			}

			if (I < 200)
			{
				MistakeScenarios(Level, Seed, Checked);
			}
		}

		// 4. Variante de secours
		{
			FPlan P;
			BuildFallbackPlan(Level, 0, P);
			FState S;
			InitState(P, S);
			FCtx X{ P, S, FCampaign() };
			std::string Why;
			if (!P.bFallback || !CheckPlanShape(P, Why) || !Solve(X, 0) || !(S.Solved & 1)) Fail(Level, 0, "variante de secours");
		}

		// 5. Niveau 11 : fragments de route de la campagne (moins de panneaux a lire), variante de fin
		if (Level == 11)
		{
			FPlan P;
			BuildPlan(11, 77, P);
			FState S;
			InitState(P, S);
			FCampaign Camp;
			Camp.RouteBits = 7;
			FCtx X{ P, S, Camp };
			if (!Solve(X, 0) || !(S.Solved & 1)) Fail(11, 77, "fragments de campagne");
			for (int D : FindRole(P, R_CityBoard))
			{
				if (S.Dev[D]) Fail(11, 77, "panneau lu alors que le fragment etait connu");
			}
			FCampaign F;
			F.OptionalFound = 0x7;
			if (!EndingVariant(F)) Fail(11, 77, "variante de fin");
			F.OptionalFound = 0x3;
			if (EndingVariant(F)) Fail(11, 77, "variante de fin trop facile");
		}

		std::printf("Niveau %2d : %d/%d resolues, %d/%d resolues apres actions au hasard, %zu variantes distinctes, "
			"%d variantes de secours, %.1f actions en moyenne, %d erreurs commises au hasard, %d erreurs typiques verifiees\n",
			Level, Solved, Seeds, FuzzSolved, Seeds, Variants.size(), Fallbacks, Seeds ? (double)Actions / Seeds : 0.0,
			Mistakes, Checked);
		TotalActions += Actions;
	}
	// Une prise a un seul gagnant : deux demandes successives du meme fusible
	{
		FPlan P;
		BuildPlan(1, 5, P);
		FState S;
		InitState(P, S);
		FCtx X{ P, S, FCampaign() };
		const int Fuse = FindRole(P, R_Fuse)[0];
		const FResult A = Do(X, Fuse, EAction::Use, 0);
		const FResult B = Do(X, Fuse, EAction::Use, 0);
		if (A.Feedback != EFeedback::Done || B.Feedback != EFeedback::AlreadyDone || Held(P, S, P.Devices[Fuse].Need) != 1)
		{
			Fail(1, 5, "fusible attribue deux fois");
		}
	}
	std::printf("Actions jouees : %ld (solveur) + %ld (au hasard)\n", TotalActions, TotalFuzz);
	if (Failures)
	{
		std::printf("RESULTAT : %d ECHEC(S)\n", Failures);
		for (const auto& K : FailureKinds)
		{
			std::printf("  %5d x %s\n", K.second, K.first.c_str());
		}
		return 1;
	}
	std::printf("RESULTAT : OK\n");
	return 0;
}
