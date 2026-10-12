// v4.13 : banc hors moteur du fil narratif (BRStoryLogic) : ordre stable de la campagne, regle d'acces de l'hote (aucun
// raccourci, Poolrooms obligatoires), progression complete dans les trois canaux de contenu, retours sans etape perdue,
// fin de chapitre puis reprise a la mise a jour suivante, migration des anciennes parties (formats 1 a 4).
// Le banc compile le code meme du jeu (BRStoryLogic.cpp, BRContentLogic.cpp) ; il ne remplace pas un essai dans Unreal.
//
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Story/test_story.cpp
//       Source/Backrooms/Private/BRStoryLogic.cpp Source/Backrooms/Private/BRContentLogic.cpp -o test_story && ./test_story
#include "BRContentLogic.h"
#include "BRStoryLogic.h"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
	int Checks = 0;
	int Failures = 0;

	void Line(const std::string& What, bool bOk)
	{
		++Checks;
		Failures += bOk ? 0 : 1;
		std::printf("  %-100s %s\n", What.c_str(), bOk ? "OK" : "ECHEC");
	}

	std::string Path(const std::vector<int>& P)
	{
		std::string S;
		for (size_t I = 0; I < P.size(); ++I)
		{
			S += (I ? " -> " : "") + (P[I] == BRStory::Ending ? std::string("dernier quai") : P[I] == -2 ? std::string("fin du chapitre") : std::to_string(P[I]));
		}
		return S;
	}

	bool AvailIn(int Level, void* Ctx)
	{
		return BRContent::IsAvailable(Level, *static_cast<BRContent::EChannel*>(Ctx));
	}

	bool AvailAll(int, void*)
	{
		return true;
	}

	/** Une campagne neuve jouee selon les regles de l'hote : depuis le Level 0, la sortie de progression de chaque niveau
	 *  (etape accomplie avant le depart), jusqu'a la fin du contenu disponible ou au dernier quai */
	std::vector<int> PlayCampaign(BRContent::EChannel Ch, BRStory::FState& S)
	{
		std::vector<int> P = { 0 };
		int Cur = 0;
		for (int Guard = 0; Guard < 20; ++Guard)
		{
			const int Next = BRStory::ProgressTarget(S, Cur, AvailIn, &Ch);
			if (Next == BRStory::Ending)
			{
				BRStory::SetDone(S, Cur);
				P.push_back(Next);
				break;
			}
			if (!BRContent::IsAvailable(Next, Ch))
			{
				BRStory::OnChapterEnd(S, Cur, BRContent::CurrentLot(Ch));
				P.push_back(-2);
				break;
			}
			// Avant que l'etape soit accomplie, l'hote refuse la suite
			if (BRStory::CanEnter(S, Next) == BRStory::EAccess::Allowed && !BRStory::IsDone(S, Cur) && BRStory::IndexOf(Next) > S.Reached)
			{
				P.push_back(-99);
				break;
			}
			BRStory::OnProgress(S, Cur, Next);
			if (BRStory::CanEnter(S, Next) != BRStory::EAccess::Allowed)
			{
				P.push_back(-98);
				break;
			}
			P.push_back(Next);
			Cur = Next;
		}
		return P;
	}

	BRStory::FMigration Migrate(std::vector<int> Explored, int Current, uint8_t Endings = 0)
	{
		BRStory::FLegacy L;
		L.Explored = Explored.data();
		L.NumExplored = static_cast<int>(Explored.size());
		L.Current = Current;
		L.Endings = Endings;
		return BRStory::MigrateLegacy(L);
	}
}

int main()
{
	using namespace BRStory;
	std::printf("Banc du fil narratif v4.13\n\n");

	std::printf("A. Ordre de la campagne\n");
	{
		std::vector<int> Chain = { 0 };
		int L = 0;
		for (int I = 0; I < 15 && L != Ending; ++I)
		{
			L = NextOf(L);
			Chain.push_back(L);
		}
		Line("fil : " + Path(Chain), Path(Chain) == "0 -> 1 -> 37 -> 2 -> 3 -> 4 -> 5 -> 6 -> 8 -> 9 -> 10 -> 11 -> dernier quai");
		bool bPrev = PrevOf(0) == -1;
		for (int I = 1; I < NumSteps; ++I)
		{
			bPrev = bPrev && PrevOf(Order[I]) == Order[I - 1] && IndexOf(Order[I]) == I;
		}
		Line("precedent et position coherents pour les 12 niveaux", bPrev);
		Line("Poolrooms entre le Niveau 1 et le Niveau 2", NextOf(1) == 37 && NextOf(37) == 2 && PrevOf(2) == 37);
		Line("niveaux hors du fil (7, 1001, 1002) : aucun suivant, acces inconnu", NextOf(7) == -1 && CanEnter(FState(), 1001) == EAccess::Unknown);
	}

	std::printf("\nB. Campagne neuve dans chaque canal (regles de l'hote)\n");
	const BRContent::EChannel Channels[3] = { BRContent::EChannel::Public, BRContent::EChannel::Internal, BRContent::EChannel::All };
	const char* Names[3] = { "version publiee", "test interne", "developpement" };
	const char* Expect[3] = { "0 -> 1 -> 37 -> 2 -> fin du chapitre", "0 -> 1 -> 37 -> 2 -> 3 -> 4 -> fin du chapitre",
		"0 -> 1 -> 37 -> 2 -> 3 -> 4 -> 5 -> 6 -> 8 -> 9 -> 10 -> 11 -> dernier quai" };
	for (int C = 0; C < 3; ++C)
	{
		FState S;
		const std::vector<int> P = PlayCampaign(Channels[C], S);
		Line(std::string(Names[C]) + " : " + Path(P), Path(P) == Expect[C]);
	}
	{
		// Le debut du chapitre ne depend pas du canal : 0 -> 1 -> 37 -> 2 partout
		bool bSame = true;
		for (int C = 0; C < 3; ++C)
		{
			FState S;
			const std::vector<int> P = PlayCampaign(Channels[C], S);
			bSame = bSame && P.size() >= 4 && P[0] == 0 && P[1] == 1 && P[2] == 37 && P[3] == 2;
		}
		Line("le chapitre 1 (0 -> 1 -> 37 -> 2) est identique dans les trois canaux", bSame);
	}

	std::printf("\nC. Aucun raccourci (validation de l'hote)\n");
	{
		FState S; // nouvelle partie, au Level 0
		Line("Level 0, porte pas ouverte : Niveau 1 refuse", CanEnter(S, 1) == EAccess::NotReached);
		Line("Level 0 : Poolrooms refusees", CanEnter(S, 37) == EAccess::NotReached);
		Line("Level 0 : Niveau 2 refuse (Poolrooms obligatoires)", CanEnter(S, 2) == EAccess::PoolroomsRequired);
		OnProgress(S, 0, 1);
		Line("porte ouverte : Niveau 1 accepte, Level 0 accompli", CanEnter(S, 1) == EAccess::Allowed && IsDone(S, 0));
		Line("Niveau 1, mission pas faite : Poolrooms refusees", CanEnter(S, 37) == EAccess::NotReached);
		Line("Niveau 1 : Niveaux 2 et 4 refuses (Poolrooms obligatoires)", CanEnter(S, 2) == EAccess::PoolroomsRequired && CanEnter(S, 4) == EAccess::PoolroomsRequired);
		OnProgress(S, 1, 37);
		Line("Niveau 1 accompli : Poolrooms acceptees ; Niveau 2 toujours refuse", CanEnter(S, 37) == EAccess::Allowed && CanEnter(S, 2) == EAccess::PoolroomsRequired);
		const FState Visit = S; // arrivee dans les Poolrooms : simple visite
		Line("simple visite des Poolrooms : pas d'etape accomplie", !IsDone(Visit, 37));
		OnProgress(S, 37, 2);
		Line("sortie des Poolrooms prise : Niveau 2 accepte, etape enregistree", CanEnter(S, 2) == EAccess::Allowed && IsDone(S, 37));
		Line("Niveau 2 : Niveau 3 refuse avant la mission, Niveau 4 refuse", CanEnter(S, 3) == EAccess::NotReached && CanEnter(S, 4) == EAccess::NotReached);
		// Toutes les paires : a chaque etape, seuls les niveaux atteints et la suite (etape faite) s'ouvrent
		int Bad = 0;
		FState T;
		for (int I = 0; I + 1 < NumSteps; ++I)
		{
			for (int J = 0; J < NumSteps; ++J)
			{
				const bool bAllowed = CanEnter(T, Order[J]) == EAccess::Allowed;
				Bad += (bAllowed != (J <= I)) ? 1 : 0;
			}
			OnProgress(T, Order[I], Order[I + 1]);
		}
		Line("12 x 12 paires etape/destination : seules les etapes atteintes s'ouvrent (" + std::to_string(Bad) + " ecart)", Bad == 0);
	}

	std::printf("\nD. Retours et detours\n");
	{
		FState S;
		OnProgress(S, 0, 1);
		OnProgress(S, 1, 37);
		OnProgress(S, 37, 2);
		Line("retour du Niveau 2 vers les Poolrooms puis le Niveau 1 : acceptes", CanEnter(S, 37) == EAccess::Allowed && CanEnter(S, 1) == EAccess::Allowed);
		OnArrive(S, 1);
		Line("au retour, les etapes deja accomplies le restent (mission du Niveau 1 non refaite)", IsDone(S, 1) && IsDone(S, 37) && IsDone(S, 0));
		Line("depuis le Niveau 1, la suite de l'histoire reste les Poolrooms", ProgressTarget(S, 1, AvailAll, nullptr) == 37);
		Line("le plus loin atteint ne recule pas (Niveau 2)", S.Reached == IndexOf(2));
		Line("point de reprise : le niveau ou l'on est revenu (Niveau 1)", S.Resume == 1);
	}

	std::printf("\nE. Fin du chapitre 1 et mise a jour suivante\n");
	{
		BRContent::EChannel Pub = BRContent::EChannel::Public;
		BRContent::EChannel Int = BRContent::EChannel::Internal;
		FState S;
		PlayCampaign(Pub, S);
		Line("fin du chapitre 1 enregistree, Niveau 2 accompli", (S.ChapterEnds & (1u << 1)) != 0 && IsDone(S, 2));
		Line("version publiee : Continuer reprend au Niveau 2 (rien de plus a ouvrir)", ResumePoint(S, AvailIn, &Pub) == 2);
		Line("chapitre 2 publie : Continuer ouvre directement le Niveau 3, sans refaire le Level 0", ResumePoint(S, AvailIn, &Int) == 3);
		Line("et l'hote accepte le Niveau 3", CanEnter(S, 3) == EAccess::Allowed);
		FState Fresh;
		Line("nouvelle partie : Continuer = Level 0", ResumePoint(Fresh, AvailIn, &Pub) == 0);
	}

	std::printf("\nF. Migration des anciennes parties\n");
	{
		struct FCase
		{
			const char* What;
			std::vector<int> Explored;
			int Current;
			uint8_t Endings;
			int Resume;
			int Parked;
			bool bPoolDone;
		};
		const FCase Cases[] = {
			{ "format 1 a 3, au Level 0 (cassettes, coupure)", { 0 }, 0, 0, 0, -1, false },
			{ "v4.11/v4.12, au Level 0 (cadrans, mission resolue ou non)", { 0 }, 0, 0, 0, -1, false },
			{ "au Niveau 1 (0, 1 explores)", { 0, 1 }, 1, 0, 1, -1, false },
			{ "dans les Poolrooms par l'ancienne echelle (0, 37)", { 0, 37 }, 37, 0, 37, -1, false },
			{ "au Niveau 5 par 0 -> 37 -> 4 -> 5 (Poolrooms traversees)", { 0, 37, 4, 5 }, 5, 0, 5, -1, true },
			{ "au Niveau 5 par 0 -> 1 -> 4 -> 5 (sans les Poolrooms)", { 0, 1, 4, 5 }, 5, 0, 37, 5, false },
			{ "v4.12 publiee, fin du chapitre au Niveau 2 (0 -> 1 -> 2)", { 0, 1, 2 }, 2, 4, 37, 2, false },
			{ "v4.12 publiee, 0 -> 37 -> 1 -> 2 (redirection : pas une preuve)", { 0, 37, 1, 2 }, 2, 0, 37, 2, false },
			{ "revenu au Niveau 1 apres le Niveau 4 (sans les Poolrooms)", { 0, 1, 4 }, 1, 0, 1, -1, false },
			{ "campagne finie par 0 -> 37 -> 4 -> ... -> 11", { 0, 37, 4, 5, 6, 8, 9, 10, 11 }, 11, 1, 11, -1, true },
		};
		for (const FCase& K : Cases)
		{
			const FMigration M = Migrate(K.Explored, K.Current, K.Endings);
			const FState& S = M.State;
			const bool bOk = S.Resume == K.Resume && S.Parked == K.Parked && IsDone(S, 37) == K.bPoolDone && M.bParkSession == (K.Parked >= 0);
			char Buf[160];
			std::snprintf(Buf, sizeof(Buf), " : reprise %d, gare %d, Poolrooms %s", S.Resume, S.Parked, IsDone(S, 37) ? "creditees" : "a traverser");
			Line(std::string(K.What) + Buf, bOk);
		}
		// Ancienne partie au Niveau 5 sans les Poolrooms : rien au-dela avant la traversee, puis la progression reprend
		{
			FMigration M = Migrate({ 0, 1, 4, 5 }, 5);
			FState& S = M.State;
			const bool bLocked = CanEnter(S, 5) == EAccess::PoolroomsRequired && CanEnter(S, 2) == EAccess::PoolroomsRequired && CanEnter(S, 37) == EAccess::Allowed;
			Line("au Niveau 5 sans les Poolrooms : Poolrooms ouvertes, Niveaux 2 et 5 en attente de la traversee", bLocked);
			const int Target = ProgressTarget(S, 37, AvailAll, nullptr);
			OnProgress(S, 37, Target);
			Line("sortie du sas : retour au Niveau 5 (progression d'avant), etape enregistree", Target == 5 && IsDone(S, 37) && CanEnter(S, 5) == EAccess::Allowed && S.Parked == -1);
			Line("les Niveaux 1 et 4 deja depasses restent accomplis", IsDone(S, 1) && IsDone(S, 4) && IsDone(S, 0));
			BRContent::EChannel Pub = BRContent::EChannel::Public;
			FMigration M2 = Migrate({ 0, 1, 4, 5 }, 5);
			Line("meme partie dans la version publiee (Niveau 5 indisponible) : la sortie du sas mene au Niveau 2",
				ProgressTarget(M2.State, 37, AvailIn, &Pub) == 2);
		}
		{
			const FMigration M = Migrate({ 0, 1, 2 }, 2, 4);
			Line("fin du lot 1 de la v4.12 conservee comme fin de chapitre", (M.State.ChapterEnds & (1u << 1)) != 0);
		}
	}

	std::printf("\nVerifications : %d, echecs : %d\nRESULTAT : %s\n", Checks, Failures, Failures == 0 ? "OK" : "ECHEC");
	return Failures == 0 ? 0 : 1;
}
