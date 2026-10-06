// v4.12 : banc hors moteur de la disponibilite des niveaux (publication progressive, Docs/PLAN_PUBLICATION.md).
//
// Il compile BRContentLogic.cpp et BRMissionLogic.cpp (le code exact du jeu) et LIT les sorties des niveaux dans
// Source/Backrooms/Private/BRLevels.cpp (D.Number, D.Exits) et la sortie de progression de la base dans
// BRMissionWorld.cpp (BaseForwardTarget) : aucune table recopiee. Pour chaque canal (version publiee, test interne,
// developpement) :
//   - la table couvre exactement les 12 niveaux du projet, chacun avec une etape et un lot ;
//   - depuis une nouvelle partie et depuis chaque niveau du menu, en suivant TOUTES les sorties (portes, echelles,
//     noclip, ascenseurs, tirages au hasard), aucun niveau indisponible n'est atteint ;
//   - la sortie que garde chaque mission mene quelque part (niveau disponible, fin du contenu ou fin de la campagne),
//     jamais vers un passage condamne : aucune mission ne demande d'atteindre un niveau non publie ;
//   - les deux routes du Niveau 0 (mur glitche -> Niveau 1, echelle -> Poolrooms) menent a la fin du contenu disponible ;
//   - redirections du lot 1 (echelle des Poolrooms -> Niveau 1, ascenseur du Niveau 1 -> Niveau 2) ;
//   - fin du contenu : dans la version publiee au Niveau 2 (lot 1), en test interne au Niveau 4 (lot 2), jamais en
//     developpement (la campagne finit au dernier quai) ;
//   - reprise d'une partie dont le dernier niveau n'est pas disponible (partie v4.11 ou de version de test) ;
//   - signatures distinctes par canal ; un client d'une autre version ou d'un autre contenu est refuse.
//
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Content/test_content.cpp
//       Source/Backrooms/Private/BRContentLogic.cpp Source/Backrooms/Private/BRMissionLogic.cpp -o test_content
//   ./test_content [chemin de Source/Backrooms/Private]   (depuis Backrooms/ par defaut)
#include "BRContentLogic.h"
#include "BRMissionLogic.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	using namespace BRContent;

	int GChecks = 0, GFailures = 0;
	void Line(const std::string& What, bool bOk)
	{
		++GChecks;
		GFailures += bOk ? 0 : 1;
		std::printf("  %-96s %s\n", What.c_str(), bOk ? "OK" : "ECHEC");
	}

	struct FExit
	{
		int Target = 0;
		std::string Style;
	};
	std::map<int, std::vector<FExit>> GExits;
	std::map<int, int> GBaseForward;

	std::string ReadFile(const std::string& Path)
	{
		std::ifstream In(Path);
		std::stringstream S;
		S << In.rdbuf();
		return S.str();
	}

	// D.Number = N; ... D.Exits = { X(T, EBRExitStyle::S, ...), ... };
	bool ParseLevels(const std::string& Src)
	{
		const std::regex NumberRe(R"(D\.Number\s*=\s*(\d+)\s*;)");
		const std::regex ExitsRe(R"(D\.Exits\s*=\s*\{([^;]*)\}\s*;)");
		const std::regex OneRe(R"(X\(\s*(-?\d+)\s*,\s*EBRExitStyle::(\w+))");
		std::vector<std::pair<size_t, int>> Numbers;
		for (auto It = std::sregex_iterator(Src.begin(), Src.end(), NumberRe); It != std::sregex_iterator(); ++It)
		{
			Numbers.push_back({ static_cast<size_t>(It->position()), std::stoi((*It)[1]) });
		}
		for (auto It = std::sregex_iterator(Src.begin(), Src.end(), ExitsRe); It != std::sregex_iterator(); ++It)
		{
			const size_t At = static_cast<size_t>(It->position());
			int Level = -1;
			for (const auto& N : Numbers)
			{
				Level = N.first < At ? N.second : Level;
			}
			if (Level < 0)
			{
				return false;
			}
			const std::string Body = (*It)[1];
			for (auto E = std::sregex_iterator(Body.begin(), Body.end(), OneRe); E != std::sregex_iterator(); ++E)
			{
				GExits[Level].push_back({ std::stoi((*E)[1]), (*E)[2] });
			}
		}
		for (const auto& N : Numbers)
		{
			GExits[N.second]; // un niveau sans sortie existe quand meme
		}
		return !Numbers.empty();
	}

	// int32 BaseForwardTarget(int32 Level) { switch (Level) { case N: return T; ... } }
	bool ParseForward(const std::string& Src)
	{
		const size_t At = Src.find("int32 BaseForwardTarget(int32 Level)");
		if (At == std::string::npos)
		{
			return false;
		}
		const size_t End = Src.find("default:", At);
		const std::string Body = Src.substr(At, End - At);
		const std::regex CaseRe(R"(case\s+(\d+)\s*:\s*return\s+([\w:]+)\s*;)");
		for (auto It = std::sregex_iterator(Body.begin(), Body.end(), CaseRe); It != std::sregex_iterator(); ++It)
		{
			const std::string T = (*It)[2];
			GBaseForward[std::stoi((*It)[1])] = T.find("EndingTarget") != std::string::npos ? BRMission::EndingTarget : std::stoi(T);
		}
		return !GBaseForward.empty();
	}

	// Meme construction que BRLevels::ExitListOf
	FExitList ExitListOf(int From)
	{
		FExitList L;
		for (const FExit& E : GExits[From])
		{
			L.Add(E.Target, BRMission::IsExitGuarded(From, E.Target));
		}
		if (From == 11)
		{
			L.Add(BRMission::EndingTarget, true);
		}
		return L;
	}

	// Meme regle que BRLevels::ResolveExit
	FExitResolution Resolve(int From, int Target, EChannel C)
	{
		return ResolveExit(From, Target, BRMission::IsExitGuarded(From, Target) || Target == BRMission::EndingTarget, ExitListOf(From), C);
	}

	int BaseForward(int Level)
	{
		const auto It = GBaseForward.find(Level);
		return It == GBaseForward.end() ? -1 : It->second;
	}

	const char* ChannelName(EChannel C)
	{
		return C == EChannel::Public ? "publiee" : (C == EChannel::Internal ? "test interne" : "developpement");
	}

	std::string KindName(const FExitResolution& R)
	{
		switch (R.Kind)
		{
		case EExit::Go: return R.Target < 0 ? "hasard" : "Niveau " + std::to_string(R.Target) + (R.bRedirected ? " (redirigee)" : "");
		case EExit::ChapterEnd: return "fin du contenu";
		case EExit::Ending: return "fin de la campagne";
		default: return "condamnee";
		}
	}

	// Tous les niveaux atteignables : nouvelle partie (Niveau 0) et menu (niveaux disponibles), puis toutes les sorties
	std::set<int> Reachable(EChannel C, std::vector<std::string>& OutBad)
	{
		std::set<int> Seen;
		std::vector<int> Todo = { 0 };
		int N = 0;
		const FLevelEntry* T = Levels(N);
		for (int I = 0; I < N; ++I)
		{
			if (IsAvailable(T[I].Level, C))
			{
				Todo.push_back(T[I].Level);
			}
		}
		while (!Todo.empty())
		{
			const int L = Todo.back();
			Todo.pop_back();
			if (!Seen.insert(L).second)
			{
				continue;
			}
			if (!IsAvailable(L, C))
			{
				OutBad.push_back("Niveau " + std::to_string(L) + " atteint");
				continue;
			}
			for (int I = 0; I < ExitListOf(L).Num; ++I)
			{
				const int Target = ExitListOf(L).Targets[I];
				const FExitResolution R = Resolve(L, Target, C);
				if (R.Kind != EExit::Go)
				{
					continue;
				}
				if (R.Target < 0)
				{
					int Out[32];
					const int Num = RandomChoices(C, L, Out, 32);
					for (int K = 0; K < Num; ++K)
					{
						Todo.push_back(Out[K]);
					}
				}
				else
				{
					Todo.push_back(R.Target);
				}
			}
		}
		return Seen;
	}

	// Progression : on suit la sortie gardee par la mission jusqu'a une fin ; renvoie le niveau de la fin et sa nature
	bool FollowForward(int Start, EChannel C, int& OutLastLevel, EExit& OutEnd, std::string& OutPath)
	{
		int L = Start;
		OutPath = std::to_string(L);
		for (int Step = 0; Step < 20; ++Step)
		{
			const int Fwd = AdaptedForward(L, BaseForward(L), ExitListOf(L), C);
			if (Fwd < 0)
			{
				return false;
			}
			const FExitResolution R = Resolve(L, Fwd, C);
			if (R.Kind == EExit::ChapterEnd || R.Kind == EExit::Ending)
			{
				OutLastLevel = L;
				OutEnd = R.Kind;
				OutPath += R.Kind == EExit::ChapterEnd ? " -> fin du contenu" : " -> dernier quai";
				return true;
			}
			if (R.Kind != EExit::Go || R.Target < 0 || !IsAvailable(R.Target, C))
			{
				return false;
			}
			L = R.Target;
			OutPath += " -> " + std::to_string(L) + (R.bRedirected ? " (redirection)" : "");
		}
		return false;
	}
}

int main(int Argc, char** Argv)
{
	const std::string Dir = Argc > 1 ? Argv[1] : "Source/Backrooms/Private";
	const bool bLevels = ParseLevels(ReadFile(Dir + "/BRLevels.cpp"));
	const bool bForward = ParseForward(ReadFile(Dir + "/BRMissionWorld.cpp"));
	std::printf("Banc de contenu v4.12 (sources lues dans %s)\n", Dir.c_str());
	Line("BRLevels.cpp : niveaux et sorties lus", bLevels);
	Line("BRMissionWorld.cpp : sorties de progression de la base lues", bForward);
	if (!bLevels || !bForward)
	{
		std::printf("RESULTAT : ECHEC (sources illisibles)\n");
		return 1;
	}

	std::printf("\nA. Configuration centrale\n");
	int N = 0;
	const FLevelEntry* T = Levels(N);
	std::set<int> InTable, Defined;
	for (int I = 0; I < N; ++I)
	{
		InTable.insert(T[I].Level);
	}
	for (const auto& E : GExits)
	{
		Defined.insert(E.first);
	}
	Line("12 niveaux definis dans le projet (" + std::to_string(Defined.size()) + ")", Defined.size() == 12);
	Line("la table couvre exactement les niveaux definis", InTable == Defined);
	bool bLots = true;
	for (int I = 0; I < N; ++I)
	{
		bLots &= T[I].Lot >= 1 && T[I].Lot <= 5;
	}
	Line("chaque niveau a un lot (1 a 5)", bLots);
	std::string Lot1;
	for (int I = 0; I < N; ++I)
	{
		if (T[I].Stage == EStage::Published)
		{
			Lot1 += (Lot1.empty() ? "" : ", ") + std::to_string(T[I].Level);
		}
	}
	Line("lot 1 publie : Niveaux " + Lot1, Lot1 == "0, 1, 2, 37");
	Line("Niveaux 3 et 4 en test interne (lot 2)", StageOf(3) == EStage::Internal && StageOf(4) == EStage::Internal && LotOf(3) == 2);
	Line("lots courants : publiee 1, test interne 2, developpement 5",
		CurrentLot(EChannel::Public) == 1 && CurrentLot(EChannel::Internal) == 2 && CurrentLot(EChannel::All) == 5);
	Line("Niveau 7 (inexistant) et 1001/1002 (reserves) inconnus et indisponibles", !IsKnown(7) && !IsKnown(1001) && !IsAvailable(1002, EChannel::All));

	// Regles de resolution sur des listes de sorties construites (cas absents de la configuration actuelle)
	{
		FExitList Two;
		Two.Add(5, true);
		Two.Add(1, true);
		Line("sortie gardee indisponible, autre sortie gardee disponible : condamnee",
			ResolveExit(2, 5, true, Two, EChannel::Public).Kind == EExit::Sealed);
		Line("... et la mission garde l'autre sortie", AdaptedForward(2, 5, Two, EChannel::Public) == 1);
		FExitList One;
		One.Add(5, true);
		One.Add(1, false);
		Line("seule sortie gardee indisponible : fin du contenu disponible", ResolveExit(2, 5, true, One, EChannel::Public).Kind == EExit::ChapterEnd);
		Line("sortie libre (non gardee) indisponible : condamnee", ResolveExit(2, 5, false, One, EChannel::Public).Kind == EExit::Sealed);
		Line("sortie vers un niveau disponible : inchangee", ResolveExit(2, 1, false, One, EChannel::Public).Target == 1);
		Line("redirection seulement depuis le niveau prevu (Niveau 3 -> 4 : pas redirigee)",
			!ResolveExit(3, 4, true, One, EChannel::Public).bRedirected);
		Line("destination au hasard : jamais condamnee", ResolveExit(11, -1, false, One, EChannel::Public).Kind == EExit::Go);
		Line("dernier quai demande hors du Niveau 11 : fin du contenu", ResolveExit(2, BRMission::EndingTarget, true, One, EChannel::All).Kind == EExit::ChapterEnd);
	}

	const EChannel Channels[] = { EChannel::Public, EChannel::Internal, EChannel::All };
	for (const EChannel C : Channels)
	{
		std::printf("\nB. Version %s\n", ChannelName(C));
		std::vector<std::string> Bad;
		const std::set<int> Seen = Reachable(C, Bad);
		std::string SeenList;
		for (const int L : Seen)
		{
			SeenList += (SeenList.empty() ? "" : ", ") + std::to_string(L);
		}
		Line("atteignables par les sorties : " + SeenList, Bad.empty());
		for (const std::string& B : Bad)
		{
			std::printf("    ! %s\n", B.c_str());
		}
		int Avail = 0;
		for (int I = 0; I < N; ++I)
		{
			Avail += IsAvailable(T[I].Level, C) ? 1 : 0;
		}
		Line("tous les niveaux disponibles sont atteignables (" + std::to_string(Avail) + ")", static_cast<int>(Seen.size()) == Avail);

		// Chaque sortie de chaque niveau disponible : jamais un niveau indisponible ; mission jamais condamnee
		bool bExitsOk = true, bForwardOk = true;
		std::string Sealed;
		for (int I = 0; I < N; ++I)
		{
			const int L = T[I].Level;
			if (!IsAvailable(L, C))
			{
				continue;
			}
			const FExitList List = ExitListOf(L);
			for (int K = 0; K < List.Num; ++K)
			{
				const FExitResolution R = Resolve(L, List.Targets[K], C);
				bExitsOk &= R.Kind != EExit::Go || R.Target < 0 || IsAvailable(R.Target, C);
				if (R.Kind == EExit::Sealed)
				{
					std::string Style;
					for (const FExit& E : GExits[L])
					{
						Style = E.Target == List.Targets[K] ? E.Style : Style;
					}
					Sealed += (Sealed.empty() ? "" : ", ") + std::to_string(L) + "->" + std::to_string(List.Targets[K]) + " (" + Style + ")";
				}
			}
			const int Base = BaseForward(L);
			if (Base >= 0)
			{
				const int Fwd = AdaptedForward(L, Base, List, C);
				const FExitResolution R = Resolve(L, Fwd, C);
				bForwardOk &= R.Kind != EExit::Sealed && (R.Kind != EExit::Go || IsAvailable(R.Target, C));
				bForwardOk &= Fwd == Base || BRMission::IsExitGuarded(L, Fwd);
			}
		}
		Line("aucune sortie ne mene a un niveau indisponible", bExitsOk);
		Line("la sortie gardee par chaque mission mene quelque part (jamais condamnee)", bForwardOk);
		std::printf("    passages condamnes : %s\n", Sealed.empty() ? "aucun" : Sealed.c_str());

		// Les deux routes du Niveau 0
		for (const int Route : { 1, 37 })
		{
			const FExitResolution R0 = Resolve(0, Route, C);
			int Last = -1;
			EExit End = EExit::Sealed;
			std::string Path = "0";
			const bool bFollow = R0.Kind == EExit::Go && FollowForward(R0.Target, C, Last, End, Path);
			const bool bExpect = C == EChannel::All ? (End == EExit::Ending && Last == 11)
				: (End == EExit::ChapterEnd && Last == (C == EChannel::Public ? 2 : 4));
			Line(std::string("route du Niveau 0 par le Niveau ") + std::to_string(Route) + " : 0 -> " + Path, bFollow && bExpect);
		}
		// Redirections du lot 1 : echelle des Poolrooms, ascenseur du Niveau 1
		const FExitResolution P = Resolve(37, 4, C);
		Line("Poolrooms -> echelle : " + KindName(P),
			C == EChannel::Public ? (P.Kind == EExit::Go && P.Target == 1 && P.bRedirected) : (P.Kind == EExit::Go && P.Target == 4 && !P.bRedirected));
		const FExitResolution L1 = Resolve(1, 4, C);
		Line("Niveau 1 -> ascenseur (sortie gardee par la mission) : " + KindName(L1),
			C == EChannel::Public ? (L1.Kind == EExit::Go && L1.Target == 2 && L1.bRedirected) : (L1.Kind == EExit::Go && L1.Target == 4));
		Line("la mission du Niveau 1 garde toujours l'ascenseur", AdaptedForward(1, BaseForward(1), ExitListOf(1), C) == 4);
		if (C == EChannel::Public)
		{
			Line("lot 1 : aucun passage condamne", Sealed.empty());
		}
		// Fin de la campagne : seulement si le Niveau 11 est disponible
		const FExitResolution E = Resolve(11, BRMission::EndingTarget, C);
		Line("dernier quai (Niveau 11) : " + std::string(IsAvailable(11, C) ? "fin de la campagne" : "hors de cette version"),
			!IsAvailable(11, C) || E.Kind == EExit::Ending);
		// Tirages au hasard
		int Out[32];
		const int Num = RandomChoices(C, -1, Out, 32);
		bool bRandomOk = Num == Avail;
		for (int K = 0; K < Num; ++K)
		{
			bRandomOk &= IsAvailable(Out[K], C);
		}
		Line("tirages au hasard : " + std::to_string(Num) + " niveaux, tous disponibles", bRandomOk);
	}

	std::printf("\nC. Reprise des sauvegardes (BRContent::ResumeLevel, utilise par le menu et les cartes de sauvegarde)\n");
	{
		struct FCase
		{
			const char* What;
			int Current;
			std::vector<int> Explored;
			EChannel Channel;
			int Expect;
			bool bMoved;
		};
		const FCase Cases[] = {
			{ "partie v4.11 au Niveau 5 (0, 1, 4, 5 explores), version publiee : Niveau 1", 5, { 0, 1, 4, 5 }, EChannel::Public, 1, true },
			{ "partie v4.11 au Niveau 5, version de test interne : Niveau 4", 5, { 0, 1, 4, 5 }, EChannel::Internal, 4, true },
			{ "partie v4.11 au Niveau 5, version de developpement : Niveau 5", 5, { 0, 1, 4, 5 }, EChannel::All, 5, false },
			{ "partie au Niveau 2, version publiee : Niveau 2 (inchangee)", 2, { 0, 37, 1, 2 }, EChannel::Public, 2, false },
			{ "partie de test au Niveau 4 (route des Poolrooms), version publiee : Niveau 2", 4, { 0, 37, 1, 2, 3, 4 }, EChannel::Public, 2, true },
			{ "dernier niveau non explore (format 1) : Niveau 0, comme avant", 37, { 0 }, EChannel::Public, 0, false },
			{ "aucun niveau disponible explore : Niveau 0", 9, { 9 }, EChannel::Public, 0, true },
		};
		for (const FCase& K : Cases)
		{
			bool bMoved = false;
			const int Got = ResumeLevel(K.Current, K.Explored.data(), static_cast<int>(K.Explored.size()), K.Channel, &bMoved);
			Line(K.What, Got == K.Expect && bMoved == K.bMoved);
		}
		// La regle ne modifie pas la partie : a la version suivante, le niveau explore redevient disponible
		const std::vector<int> Explored = { 0, 1, 4, 5 };
		bool bMoved = false;
		ResumeLevel(5, Explored.data(), 4, EChannel::Public, &bMoved);
		Line("la liste des niveaux explores n'est pas modifiee (Niveau 4 de nouveau disponible au lot 2)",
			Explored.size() == 4 && Explored[2] == 4 && IsAvailable(Explored[2], EChannel::Internal));
	}

	std::printf("\nD. Compatibilite reseau\n");
	const uint32_t SP = Signature(EChannel::Public), SI = Signature(EChannel::Internal), SA = Signature(EChannel::All);
	Line("signatures distinctes par canal", SP != SI && SI != SA && SP != SA);
	Line("signature stable (deux calculs identiques)", SP == Signature(EChannel::Public));
	Line("meme version, meme contenu : accepte", CheckJoin(NetVersion, true, SP, EChannel::Public) == EJoin::Ok);
	Line("autre version (v4.11 : 411) : refus version", CheckJoin(411, true, SP, EChannel::Public) == EJoin::Version);
	Line("client sans controle (avant v4.12) : refus version", CheckJoin(-1, false, 0, EChannel::Public) == EJoin::Version);
	Line("version de test contre version publiee : refus contenu", CheckJoin(NetVersion, true, SI, EChannel::Public) == EJoin::Content);
	Line("version publiee contre hote de developpement : refus contenu", CheckJoin(NetVersion, true, SP, EChannel::All) == EJoin::Content);
	Line("signature absente : refus contenu", CheckJoin(NetVersion, false, SP, EChannel::Public) == EJoin::Content);

	std::printf("\nRESULTAT : %s (%d verifications, %d echec(s))\n", GFailures == 0 ? "OK" : "ECHEC", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
