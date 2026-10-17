// v4.11 : banc d'essai hors moteur de la logique des missions (Source/Backrooms/Private/BRMissionLogic.cpp).
// v4.13 : le Niveau 0 n'a plus de mission (introduction d'exploration) : il est verifie a part (aucun plan, aucune sortie
// gardee, sa porte mene au Niveau 1) ; les sorties gardees suivent l'ordre de la campagne (0 -> 1 -> 37 -> 2 -> ...).
//
// Compilation et lancement (depuis Backrooms/) :
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Missions/test_mission_logic.cpp
//       Source/Backrooms/Private/BRMissionLogic.cpp Source/Backrooms/Private/BRMissionSolver.cpp -o /tmp/test_missions
//   /tmp/test_missions [graines par niveau, 3000 par defaut]
//
// Pour chaque niveau et chaque graine :
//   - plan deterministe, appareils et zones valides, aucun objet requis dans la salle qu'il ouvre ;
//   - un solveur qui n'utilise QUE ce que le joueur voit (inscriptions, indices lus, observations, jauges, retours)
//     resout la mission par de vraies actions et ouvre la sortie ; en mode joueur (appuis du jeu seulement), ses
//     actions enregistrees se rejouent a l'identique sur un etat neuf (le test du jeu les rejoue par interactions) ;
//   - apres une suite d'actions au hasard (erreurs comprises), l'etat reste valide, se serialise a l'identique, et le
//     solveur resout encore la mission : aucune impasse ;
//   - les erreurs typiques donnent un retour comprehensible et se corrigent ;
//   - la variante de secours est valide ; un blob d'une autre graine ou tronque est refuse.
// Code de sortie 0 si tout passe, 1 sinon. La sortie sert de journal (Docs/logs).
#include "BRMissionLogic.h"
#include "BRMissionSolver.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace BRMission;

static const int Levels[] = { 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 37 };
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

/** Solveur partage avec le test du jeu (Source/Backrooms/Private/BRMissionSolver.cpp) */
using FCtx = FSolver;

static bool SameState(const FState& A, const FState& B)
{
	return std::memcmp(A.Dev, B.Dev, sizeof(A.Dev)) == 0 && A.Solved == B.Solved && A.Mistakes == B.Mistakes;
}

static FResult Do(FCtx& X, int Dev, EAction A, int V)
{
	return X.Do(Dev, A, V);
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

static bool ReadDev(FCtx& X, int D, FClue& C)
{
	return X.Read(D, C);
}

static bool Solve(FCtx& X, int Route)
{
	return X.Solve(Route);
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
		// v4.13 : ordre du LiDAR (calibrage, poste, cage, porte), porte fausse reversible et explicite
		const std::vector<int> Posts = FindRole(P, R_LidarPost);
		const std::vector<int> Cages = FindRole(P, R_LidarCage);
		const std::vector<int> Doors = FindRole(P, R_LidarDoor);
		const int Calib = FindRole(P, R_LidarTarget)[0];
		Expect(Do(X, Posts[0], EAction::Hold, 1), EFeedback::Locked, "poste avant le calibrage");
		Expect(Do(X, Doors[0], EAction::Use, 0), EFeedback::Locked, "porte avant les postes");
		X.HoldUntilDone(Calib);
		Expect(Do(X, Cages[1], EAction::Hold, 1), EFeedback::Locked, "cage avant son poste");
		for (int D : Posts) X.HoldUntilDone(D);
		Expect(Do(X, Doors[0], EAction::Use, 0), EFeedback::Locked, "porte avant le releve des cages");
		int CodeOf[NumFigures];
		for (int F = 0; F < NumFigures; ++F) CodeOf[F] = -1;
		for (int D : Cages)
		{
			X.HoldUntilDone(D);
			if (GetClue(P, S, D, C)) CodeOf[C.A[0]] = C.B[0];
		}
		Expect(Do(X, Doors[0], EAction::Use, 0), EFeedback::Locked, "porte avant le journal");
		ReadDev(X, FindRole(P, R_TeamLog)[0], C);
		const int Fig = C.A[0];
		// Une seule porte avec toutes les informations ; deux sans le journal (paires exactes), deux sans les cages
		int Full = 0, NoLog = 0, NoCages = 0, Wrong = -1;
		for (int D : Doors)
		{
			FClue Pl;
			GetClue(P, S, D, Pl);
			const bool bPair = Pl.A[0] < NumFigures && CodeOf[Pl.A[0]] == Pl.B[0];
			Full += (Pl.A[0] == Fig && bPair) ? 1 : 0;
			NoLog += bPair ? 1 : 0;
			NoCages += Pl.A[0] == Fig ? 1 : 0;
			if (!(Pl.A[0] == Fig && bPair) && Wrong < 0) Wrong = D;
		}
		++Checked;
		if (Full != 1) Fail(Level, Seed, "porte du passage non unique");
		++Checked;
		if (NoLog < 2) Fail(Level, Seed, "porte devinable sans le journal");
		++Checked;
		if (NoCages < 2) Fail(Level, Seed, "porte devinable sans les cages");
		const FState Before = S;
		const FResult R = Do(X, Wrong, EAction::Use, 0);
		Expect(R, EFeedback::Wrong, "porte fausse");
		++Checked;
		if (R.Count < 1 || R.Count > 2 || (S.Solved & 1) || std::memcmp(S.Dev, Before.Dev, sizeof(S.Dev)) != 0 || S.Mistakes != Before.Mistakes + 1)
		{
			Fail(Level, Seed, "porte fausse : retour sans nombre d'elements faux, ou etat modifie");
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
	std::printf("Banc d'essai des missions v4.13 : %d graines par niveau, generation %d\n", Seeds, GenVersion);
	// v4.13 : le Niveau 0 est une introduction sans mission : aucun plan (normal ou de secours), aucune sortie gardee
	{
		int Empty = 0;
		for (int I = 0; I < Seeds; ++I)
		{
			FPlan P;
			BuildPlan(0, 0x1000u + (uint32_t)I * 2654435761u, P);
			Empty += (P.NumDevices == 0 && P.NumSteps == 0 && !P.bHasRoute) ? 1 : 0;
		}
		FPlan F;
		BuildFallbackPlan(0, 0, F);
		FState S;
		InitState(F, S);
		bool bFree = !HasMission(0) && F.NumDevices == 0;
		for (int T : { 1, 37, 2, 4, 0 })
		{
			bFree = bFree && !IsExitGuarded(0, T) && IsExitOpen(F, S, T);
		}
		if (Empty != Seeds || !bFree || ForwardTarget(0, 0) != 1 || ForwardTarget(0, 1) != 1)
		{
			Fail(0, 0, "Niveau 0 : une mission ou une condition de sortie subsiste");
		}
		std::printf("Niveau  0 : introduction sans mission : %d/%d graines sans plan, variante de secours vide, aucune sortie gardee, porte vers le Niveau %d\n",
			Empty, Seeds, ForwardTarget(0, 0));
	}
	// v4.13 : une seule sortie gardee par niveau, celle de l'ordre de la campagne ; les anciennes (1 -> 2, 1 -> 4, 37 -> 4,
	// 37 -> 0, 0 -> 37) ne le sont plus
	{
		const int Chain[] = { 0, 1, 37, 2, 3, 4, 5, 6, 8, 9, 10, 11 };
		bool bOrder = true;
		for (int I = 1; I < 12; ++I)
		{
			const int Next = I + 1 < 12 ? Chain[I + 1] : EndingTarget;
			bOrder = bOrder && ForwardTarget(Chain[I], 0) == Next && IsExitGuarded(Chain[I], Next);
		}
		const bool bOld = !IsExitGuarded(1, 2) && !IsExitGuarded(1, 4) && !IsExitGuarded(37, 4) && !IsExitGuarded(37, 0) && !IsExitGuarded(0, 37);
		if (!bOrder || !bOld)
		{
			Fail(0, 0, "sorties gardees hors de l'ordre de la campagne");
		}
		std::printf("Ordre : sortie gardee de chaque niveau = suivant dans 0 -> 1 -> 37 -> 2 -> ... -> 11 -> dernier quai : %s ; anciennes sorties gardees retirees : %s\n",
			bOrder ? "oui" : "NON", bOld ? "oui" : "NON");
	}
	long TotalActions = 0, TotalFuzz = 0;
	for (int Level : Levels)
	{
		int Solved = 0, Fallbacks = 0, Mistakes = 0, FuzzSolved = 0, Checked = 0, PlayerSolved = 0, MaxPlayerActions = 0;
		long Actions = 0, PlayerActions = 0;
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
			for (int Route = 0; Route < 1; ++Route)
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
				for (int St = 0; St < E.NumSteps; ++St)
				{
					if (E.Steps[St] == EStep::Hidden) Fail(Level, Seed, "etape cachee apres resolution");
				}
				if (!Roundtrip(P, S)) Fail(Level, Seed, "serialisation de l'etat resolu");
				if (!StateValid(P, S, Why)) Fail(Level, Seed, Why);
				Actions += X.Actions;
				if (Route == 0) ++Solved;

				// 1 bis. Memes informations, actions du jeu seulement (un appui passe a la position suivante) : enregistrees,
				// puis rejouees sur un etat neuf (ce que fait le test du jeu par de vraies interactions)
				{
					FState S2;
					InitState(P, S2);
					FCtx Y{ P, S2, FCampaign() };
					Y.bPlayerActions = true;
					Y.bRecord = true;
					if (!Solve(Y, Route) || Y.bBad || Y.bOverflow || !(S2.Solved & 1))
					{
						Fail(Level, Seed, Y.bOverflow ? std::string("mode joueur : trop d'actions") : (Y.bBad ? std::string("mode joueur : ") + Y.Why : std::string("mode joueur : solveur bloque")));
					}
					else
					{
						FState S3;
						InitState(P, S3);
						if (!Replay(P, S3, FCampaign(), Y.Steps, Y.NumSteps) || !SameState(S3, S2) || !IsExitOpen(P, S3, Target))
						{
							Fail(Level, Seed, "mode joueur : rejeu different");
						}
						for (int K = 0; K < Y.NumSteps; ++K)
						{
							if (Y.Steps[K].Action == static_cast<uint8_t>(EAction::Set))
							{
								Fail(Level, Seed, "mode joueur : action Set");
								break;
							}
						}
						PlayerActions += Y.NumSteps;
						MaxPlayerActions = std::max(MaxPlayerActions, Y.NumSteps);
						if (Route == 0) ++PlayerSolved;
					}
				}
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
					Fail(Level, Seed, Y.bBad ? std::string("apres aleatoire : ") + Y.Why : "impasse apres actions au hasard");
				}
				else
				{
					++FuzzSolved;
				}
				if (X.bBad) Fail(Level, Seed, std::string("aleatoire : ") + X.Why);
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

		// 4 bis. v4.13 : Niveau 6, anciennes sauvegardes (chaine de balises v4.12) converties : terminee -> terminee,
		// en cours -> calibrage et postes acquis, rien n'est perdu ni bloque ; un blob d'une autre graine n'est pas converti
		if (Level == 6)
		{
			int Converted = 0, Kept = 0, Refused = 0;
			for (int I = 0; I < Seeds; ++I)
			{
				const uint32_t Seed = 0x1000u + (uint32_t)I * 2654435761u;
				FPlan P, Old;
				BuildPlan(6, Seed, P);
				BuildLegacyPlan6(Seed, false, Old);
				for (int Done = 0; Done <= 4; ++Done)
				{
					// Ancien etat : plaque lue, Done balises de la chaine allumees (4 : alimentation de secours, resolue)
					FState OS;
					InitState(Old, OS);
					OS.Dev[0] = 1;
					for (int K = 0; K < Done && K < 3; ++K) OS.Dev[Old.Params[K]] = Old.Devices[Old.Params[K]].Positions;
					if (Done < 3) OS.Dev[Old.Params[Done]] = 1;
					OS.Solved = Done == 4 ? 1 : 0;
					OS.Mistakes = 2;
					uint8_t Blob[MaxBlob];
					const int N = Serialize(Old, OS, Blob, sizeof(Blob));
					FState Back, NS;
					if (Deserialize(P, Blob, N, Back))
					{
						Fail(6, Seed, "ancien blob accepte tel quel par le nouveau plan");
					}
					if (!MigrateState(P, Blob, N, NS))
					{
						Fail(6, Seed, "ancien blob non converti");
						continue;
					}
					++Converted;
					FEval E;
					Evaluate(P, NS, FCampaign(), E);
					const int PostsDone = E.Progress[1];
					const bool bOk = Done == 4 ? (E.bSolved && IsExitOpen(P, NS, 8)) : (!E.bSolved && E.Steps[0] == EStep::Done && PostsDone == Done && NS.Mistakes == 2);
					std::string Why;
					if (!bOk || !StateValid(P, NS, Why) || !Roundtrip(P, NS)) Fail(6, Seed, "conversion d'une ancienne sauvegarde incorrecte");
					else ++Kept;
					// La mission convertie se termine
					FCtx X{ P, NS, FCampaign() };
					if (!Solve(X, 0) || !(NS.Solved & 1)) Fail(6, Seed, "mission convertie bloquee");
					// Blob d'une autre graine : pas de conversion
					FPlan Other;
					BuildPlan(6, Seed ^ 0x5A5A5A5Au, Other);
					FState OS2;
					if (MigrateState(Other, Blob, N, OS2)) Fail(6, Seed, "ancien blob d'une autre graine converti");
					else ++Refused;
				}
			}
			std::printf("Niveau  6 : anciennes sauvegardes (balises v4.12) : %d converties, %d conformes (etapes gardees, terminee -> terminee), %d refusees (autre graine)\n",
				Converted, Kept, Refused);
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
		std::printf("           mode joueur (appuis du jeu, rejoues a l'identique) : %d/%d resolues, %.1f actions en moyenne, %d au plus\n",
			PlayerSolved, Seeds, Seeds ? (double)PlayerActions / Seeds : 0.0, MaxPlayerActions);
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
	// v4.15 : version de placement 3 : l'avis "mecanismes reamenages" a la reprise ne concerne que le Level 6 (postes et
	// cages, salle de l'aile) ; une sauvegarde d'avant la v4.12 (1) ou d'une version inconnue le donne partout
	{
		bool bOk = PlaceVersion == 3;
		int Moved = 0;
		for (const int L : { 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 37 })
		{
			bOk = bOk && PlacementMoved(L, 2) == (L == 6) && PlacementMoved(L, 1) && !PlacementMoved(L, PlaceVersion) && PlacementMoved(L, PlaceVersion + 1);
			Moved += PlacementMoved(L, 2) ? 1 : 0;
		}
		if (!bOk)
		{
			Fail(6, 0, "avis de placement : niveaux concernes par la version 3");
		}
		std::printf("Placement v%d : avis \"reamenages\" depuis la version 2 dans %d niveau(x) (Level 6)\n", PlaceVersion, Moved);
	}
	// v4.15 (correctifs 3, relecture) : avis a la reprise (ResumeMoved, ABRWorld::SetupMission) : depuis le placement 2 hors du
	// Level 6, seulement si l'hote a trouve des mecanismes de la graine a une autre place (couloirs de visee, poteaux en rang)
	{
		struct FCase
		{
			int Level, From, Moved;
			bool bWant;
		};
		const FCase Cases[] = {
			{ 2, 2, 0, false },               // Niveau 2, graine de -BRAutoTestV412 : rien de deplace
			{ 10, 2, 2, true },               // Niveau 10, graine 4111 : orientation et frein du moulin
			{ 2, 2, -1, false },              // pas compare (client, module de secours)
			{ 6, 2, -1, true },               // Level 6 : placement 3 en entier
			{ 6, 2, 0, true },
			{ 10, 1, 0, true },               // avant la v4.12 : partout
			{ 10, PlaceVersion, 2, false },   // version courante : jamais
			{ 2, PlaceVersion + 1, 0, true }, // version inconnue : partout
		};
		int Bad = 0;
		for (const FCase& K : Cases)
		{
			Bad += ResumeMoved(K.Level, K.From, K.Moved) == K.bWant ? 0 : 1;
		}
		bool bSame = true;
		for (const int L : { 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 37 })
		{
			for (const int From : { 0, 1, 2, PlaceVersion, PlaceVersion + 1 })
			{
				for (const int Count : { 3, 1, 0, -1 })
				{
					// Regle d'avant la relecture (ABRWorld::SetupMission), a l'identique
					const bool bOld = From != PlaceVersion && (PlacementMoved(L, From) || (From == 2 && Count > 0));
					bSame = bSame && ResumeMoved(L, From, Count) == bOld;
				}
			}
		}
		if (Bad > 0 || !bSame)
		{
			Fail(6, 0, "avis de placement : reprise du placement 2 selon les mecanismes deplaces");
		}
		std::printf("Reprise : avis \"reamenages\" selon les mecanismes deplaces : %d/%d cas conformes, meme decision que la regle d'avant : %s\n",
			static_cast<int>(sizeof(Cases) / sizeof(Cases[0])) - Bad, static_cast<int>(sizeof(Cases) / sizeof(Cases[0])), bSame ? "oui" : "NON");
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
