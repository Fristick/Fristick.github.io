// v4.13 : banc hors moteur du fil narratif (BRStoryLogic) : ordre stable de la campagne, regle d'acces de l'hote (aucun
// raccourci, Poolrooms obligatoires), progression complete dans les trois canaux de contenu, retours sans etape perdue,
// fin de chapitre puis reprise a la mise a jour suivante, migration des anciennes parties (formats 1 a 4).
// v4.14 : preuve de traversee des Poolrooms (prouvee, absente, ambigue), session garee conservee jusqu'a sa reprise, regle
// d'ecriture de la partie (REJOUER et mode developpeur n'ecrivent pas la campagne).
// v4.15 : parcours lineaire (section J) : graine fixe par niveau (LevelSeedFor), regle de la mort (DeathRuleFor), sorties
// des chunks (ChunkExitAllowed), fil, acces, reprise et migration inchanges, rien de nouveau ecrit en REJOUER.
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

	BRStory::FMigration Migrate(std::vector<int> Explored, int Current, uint8_t Endings = 0, int Format = 4)
	{
		BRStory::FLegacy L;
		L.Explored = Explored.data();
		L.NumExplored = static_cast<int>(Explored.size());
		L.Current = Current;
		L.Endings = Endings;
		L.Format = Format;
		return BRStory::MigrateLegacy(L);
	}

	/** v4.14 : champs durables d'une partie de campagne (ceux que WriteActiveSave peut toucher) */
	struct FRecord
	{
		int Items = 0;
		int Deaths = 0;
		bool bPendingDeath = false;
		bool bSessionValid = false;
		int SessionLevel = -1;
		unsigned SessionSeed = 0;
		int CurrentLevel = 0;
		int RouteBits = 0;
		int OptionalFound = 0;
		int Endings = 0;
		int StoryDone = 0;
		/** v4.15 : parcours (BRStory::Flag) et graine du parcours lineaire */
		unsigned StoryFlags = 0;
		unsigned RunSeed = 0;
		double PlayTime = 0.0;
		std::vector<int> Journal;
		bool operator==(const FRecord& O) const
		{
			return Items == O.Items && Deaths == O.Deaths && bPendingDeath == O.bPendingDeath && bSessionValid == O.bSessionValid && SessionLevel == O.SessionLevel
				&& SessionSeed == O.SessionSeed && CurrentLevel == O.CurrentLevel && RouteBits == O.RouteBits && OptionalFound == O.OptionalFound
				&& Endings == O.Endings && StoryDone == O.StoryDone && StoryFlags == O.StoryFlags && RunSeed == O.RunSeed && PlayTime == O.PlayTime
				&& Journal == O.Journal;
		}
	};

	/** Etat de la session en cours (personnage et monde), tel que WriteActiveSave le recopierait */
	struct FPlay
	{
		int Items = 0;
		bool bDead = false;
		int Level = 0;
		unsigned Seed = 0;
		int RouteBits = 0;
		int OptionalFound = 0;
		int Endings = 0;
		int StoryDone = 0;
		unsigned StoryFlags = 0;
		unsigned RunSeed = 0;
		std::vector<int> Journal;
	};

	/** Meme decoupage que ABRPlayerController::WriteActiveSave, NotifyPlayerDeath et le compteur de temps, gouverne par
	 *  BRStory::WritePolicyFor */
	void Write(const BRStory::FWritePolicy& P, FRecord& R, const FPlay& Play)
	{
		if (!P.bSession)
		{
			if (P.bJournalUnion)
			{
				for (int Id : Play.Journal)
				{
					bool bHas = false;
					for (int J : R.Journal)
					{
						bHas |= J == Id;
					}
					if (!bHas)
					{
						R.Journal.push_back(Id);
					}
				}
			}
			return;
		}
		if (Play.bDead)
		{
			R.bPendingDeath = true;
			R.bSessionValid = false;
		}
		else
		{
			R.Items = Play.Items;
			R.bPendingDeath = false;
			R.bSessionValid = true;
			R.SessionLevel = Play.Level;
			R.SessionSeed = Play.Seed;
		}
		R.Journal = Play.Journal;
		R.RouteBits = Play.RouteBits;
		R.OptionalFound = Play.OptionalFound;
		R.Endings = Play.Endings;
		R.StoryDone = Play.StoryDone;
		R.StoryFlags = Play.StoryFlags;
		R.RunSeed = Play.RunSeed;
		R.CurrentLevel = Play.Level;
	}

	void Death(const BRStory::FWritePolicy& P, FRecord& R, FPlay& Play)
	{
		Play.bDead = true;
		if (P.bDeath)
		{
			++R.Deaths;
		}
		Write(P, R, Play);
	}

	void Tick(const BRStory::FWritePolicy& P, FRecord& R, double Dt)
	{
		if (P.bPlayTime)
		{
			R.PlayTime += Dt;
		}
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
			// v4.14 : l'ordre des premieres decouvertes n'est pas un journal des trajets
			{ "[0, 1, 37, 4] : 1, retour a 0, 37, retour par 0, ascenseur 1 -> 4", { 0, 1, 37, 4 }, 4, 0, 37, 4, false },
			{ "[0, 1, 4, 37] : vraie traversee vers un Niveau 4 deja connu (non prouvable)", { 0, 1, 4, 37 }, 4, 0, 37, 4, false },
			{ "[0, 37, 1, 4, 5] : revisite, 1 decouvert avant 4", { 0, 37, 1, 4, 5 }, 5, 0, 37, 5, false },
			{ "[0, 37, 2, 3, 4] : 3 (ascenseur -> 4) decouvert avant 4", { 0, 37, 2, 3, 4 }, 4, 0, 37, 4, false },
			{ "[0, 37, 4, 1, 2, 3, 5] : retours et revisites apres une vraie traversee", { 0, 37, 4, 1, 2, 3, 5 }, 5, 0, 5, -1, true },
			{ "[0, 37] arrete dans les Poolrooms (visitees, pas de preuve, rien a garer)", { 0, 37 }, 37, 0, 37, -1, false },
			{ "[0, 1, 2] jamais visitees (absence prouvee), arrete au Niveau 2", { 0, 1, 2 }, 2, 0, 37, 2, false },
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
			Line("sortie du sas : retour au Niveau 5 (progression d'avant), etape enregistree, gare jusqu'a la reprise (v4.14)",
				Target == 5 && IsDone(S, 37) && CanEnter(S, 5) == EAccess::Allowed && S.Parked == 5 && WantsParked(S, 5));
			OnParkedRestored(S);
			Line("reprise de la session garee confirmee : etape de migration terminee", S.Parked == -1);
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

	std::printf("\nG. v4.14 : preuve de traversee des Poolrooms\n");
	{
		Line("[0, 1, 37, 4] (contre-exemple de l'audit) : ambigu, pas de credit, partie garee, detour organise",
			[] { const FMigration M = Migrate({ 0, 1, 37, 4 }, 4); return M.PoolProof == EPoolProof::Ambiguous && !M.bPoolroomsCredited && M.bParkSession
				&& (M.State.Events & Event::MigratedDetour) && !IsDone(M.State, 37); }());
		Line("[0, 37, 4, 5] format 4 : prouvee (seul acces au Niveau 4 connu : l'echelle des Poolrooms)",
			Migrate({ 0, 37, 4, 5 }, 5).PoolProof == EPoolProof::Proven);
		Line("[0, 37, 4, 5] format 1 (console v4.1 a v4.3) : ambigu", Migrate({ 0, 37, 4, 5 }, 5, 0, 1).PoolProof == EPoolProof::Ambiguous);
		Line("[0, 37, 4, 5] format inconnu (0) : ambigu", Migrate({ 0, 37, 4, 5 }, 5, 0, 0).PoolProof == EPoolProof::Ambiguous);
		Line("[0, 11, 37, 4] : Niveau 11 (destinations au hasard) connu avant 4 : ambigu", Migrate({ 0, 11, 37, 4 }, 4).PoolProof == EPoolProof::Ambiguous);
		Line("[0, 1] : jamais visitees : absence prouvee", Migrate({ 0, 1 }, 1).PoolProof == EPoolProof::Absent);
		Line("courant 37 sans decouverte enregistree : ambigu (visitees)", Migrate({ 0 }, 37).PoolProof == EPoolProof::Ambiguous);
		// Rien n'est efface par la migration : seule la session est garee (le jeu la copie dans ParkedSession)
		const FMigration M = Migrate({ 0, 1, 37, 4 }, 4);
		Line("ancienne progression conservee : Niveaux 0 et 1 accomplis, plus loin atteint = Niveau 4", IsDone(M.State, 0) && IsDone(M.State, 1) && M.State.Reached == IndexOf(4));
	}

	std::printf("\nH. v4.14 : session garee conservee jusqu'a sa reprise (canaux, ajout d'un chapitre)\n");
	{
		BRContent::EChannel Pub = BRContent::EChannel::Public;
		BRContent::EChannel Int = BRContent::EChannel::Internal;
		BRContent::EChannel Dev = BRContent::EChannel::All;
		FMigration M = Migrate({ 0, 1, 4, 5 }, 5);
		FState& S = M.State;
		// Version publique : le niveau gare (5) est indisponible : la sortie du sas mene au Niveau 2, la progression garee reste
		const int T = ProgressTarget(S, 37, AvailIn, &Pub);
		OnProgress(S, 37, T);
		Line("version publique : sortie des Poolrooms vers le Niveau 2, Poolrooms accomplies, Niveau 5 toujours gare",
			T == 2 && IsDone(S, 37) && S.Parked == 5 && !WantsParked(S, 2));
		PlayCampaign(Pub, S);
		Line("fin du chapitre 1 : la progression garee n'est pas videe", S.Parked == 5);
		Line("chapitre 2 publie (Niveau 5 encore indisponible) : Continuer au Niveau 3, gare conserve", ResumePoint(S, AvailIn, &Int) == 3 && S.Parked == 5);
		Line("Niveau 5 publie : Continuer reprend la progression garee au Niveau 5", ResumePoint(S, AvailIn, &Dev) == 5 && WantsParked(S, 5));
		OnParkedRestored(S);
		Line("reprise reussie : l'etape de migration est terminee", S.Parked == -1 && !WantsParked(S, 5));
		// Canal de developpement : la sortie du sas mene directement au niveau gare ; la reprise n'est confirmee qu'a l'arrivee
		FMigration M2 = Migrate({ 0, 1, 4, 5 }, 5);
		const int T2 = ProgressTarget(M2.State, 37, AvailAll, nullptr);
		OnProgress(M2.State, 37, T2);
		Line("niveau gare disponible : sortie du sas vers le Niveau 5, gare jusqu'a la confirmation de la reprise", T2 == 5 && WantsParked(M2.State, 5) && M2.State.Parked == 5);
	}

	std::printf("\nI. v4.14 : REJOUER et mode developpeur n'ecrivent pas la partie de campagne\n");
	{
		const FWritePolicy Story = WritePolicyFor(ESession::Story);
		const FWritePolicy Replay = WritePolicyFor(ESession::Replay);
		const FWritePolicy Free = WritePolicyFor(ESession::Free);
		// Partie de campagne arretee au Niveau 2 : inventaire de 5 objets, mission en cours, journal { 3, 7 }
		FRecord R;
		FPlay Camp;
		Camp.Items = 5;
		Camp.Level = 2;
		Camp.Seed = 4242;
		Camp.RouteBits = 1;
		Camp.StoryDone = 0x0B;
		// v4.15 : partie au parcours lineaire : son parcours et sa graine font partie de ce que le rejeu ne touche pas
		Camp.StoryFlags = Flag::Linear;
		Camp.RunSeed = 41500u;
		Camp.Journal = { 3, 7 };
		Write(Story, R, Camp);
		Tick(Story, R, 600.0);
		const FRecord Before = R;
		// Rejeu du Level 0 : consommation (4 objets), ramassage (6), degats, document lu (9), fin vue, mort, menu, fermeture
		FPlay Rep;
		Rep.Level = 0;
		Rep.Seed = 77;
		Rep.Items = 4;
		Rep.Journal = { 3, 7, 9 };
		Rep.Endings = 2;
		Rep.StoryDone = 0;
		Write(Replay, R, Rep);
		Rep.Items = 6;
		Tick(Replay, R, 120.0);
		Write(Replay, R, Rep);
		Death(Replay, R, Rep);
		Write(Replay, R, Rep);
		FRecord Expected = Before;
		Expected.Journal = { 3, 7, 9 };
		Line("rejeu (consommation, ramassage, mort, fin, fermeture) : equipement, morts, session, niveau, campagne, histoire, temps inchanges",
			R == Expected);
		Line("transfert explicite : le document lu en rejeu complete le journal (union, rien retire)", R.Journal.size() == 3 && R.Journal[2] == 9);
		// Mode developpeur sur un niveau deja decouvert : rien du tout
		FRecord R2 = Before;
		FPlay DevPlay = Rep;
		DevPlay.bDead = false;
		DevPlay.Items = 1;
		DevPlay.Level = 2;
		Write(Free, R2, DevPlay);
		Death(Free, R2, DevPlay);
		Tick(Free, R2, 60.0);
		Line("mode developpeur (niveau deja decouvert, mort comprise) : partie identique", R2 == Before);
		// CONTINUER retrouve exactement la session d'avant le rejeu
		Line("CONTINUER apres le rejeu : session du Niveau 2, graine 4242, 5 objets", R.bSessionValid && R.SessionLevel == 2 && R.SessionSeed == 4242 && R.Items == 5 && !R.bPendingDeath);
		// L'histoire, elle, ecrit tout (regle inchangee)
		FRecord R3 = Before;
		FPlay Next = Camp;
		Next.Items = 3;
		Write(Story, R3, Next);
		Line("histoire : l'ecriture suit la session (3 objets)", R3.Items == 3);
	}

	std::printf("\nJ. v4.15 : parcours lineaire\n");
	{
		FState Lin;
		Lin.Flags = Flag::Linear;
		Lin.RunSeed = 41500u;
		// --------------------------------------------------------------------------------------------- Graines des niveaux
		Line("graine d'un niveau : deterministe (meme parcours, meme niveau, meme graine)", LevelSeedFor(Lin, 2) == LevelSeedFor(Lin, 2) && LevelSeedFor(Lin, 2) != 0);
		int Zero = 0;
		int Dup = 0;
		int SameAcross = 0;
		int EvenSeeds = 0;
		for (unsigned Run = 1; Run <= 1000; ++Run)
		{
			FState A;
			A.Flags = Flag::Linear;
			A.RunSeed = Run * 2654435761u + 7u;
			FState B = A;
			B.RunSeed = A.RunSeed + 1u;
			std::vector<uint32_t> Seen;
			for (int I = 1; I < NumSteps; ++I)
			{
				const uint32_t Sa = LevelSeedFor(A, Order[I]);
				Zero += Sa == 0 ? 1 : 0;
				EvenSeeds += (Sa & 1u) == 0 ? 1 : 0;
				for (const uint32_t Prev : Seen)
				{
					Dup += Prev == Sa ? 1 : 0;
				}
				Seen.push_back(Sa);
				SameAcross += Sa == LevelSeedFor(B, Order[I]) ? 1 : 0;
			}
		}
		Line("1000 parcours x 11 niveaux : aucune graine nulle (" + std::to_string(Zero) + ")", Zero == 0 && EvenSeeds == 0);
		Line("1000 parcours : 11 graines distinctes par parcours (" + std::to_string(Dup) + " doublon)", Dup == 0);
		Line("deux parcours voisins : graines differentes a chaque niveau (" + std::to_string(SameAcross) + " egalite)", SameAcross == 0);
		Line("Level 0 : aucune graine imposee (il garde IntroSeed)", LevelSeedFor(Lin, 0) == 0);
		{
			FState Classic = Lin;
			Classic.Flags = 0;
			FState NoRun = Lin;
			NoRun.RunSeed = 0;
			int Any = 0;
			for (int I = 0; I < NumSteps; ++I)
			{
				Any += LevelSeedFor(Classic, Order[I]) != 0 ? 1 : 0;
				Any += LevelSeedFor(NoRun, Order[I]) != 0 ? 1 : 0;
			}
			Line("parcours classique (Flags 0) ou graine du parcours nulle : aucune graine imposee", Any == 0);
		}
		Line("niveaux hors du fil (7, 1001, 1002, dernier quai) : aucune graine imposee",
			LevelSeedFor(Lin, 7) == 0 && LevelSeedFor(Lin, 1001) == 0 && LevelSeedFor(Lin, 1002) == 0 && LevelSeedFor(Lin, Ending) == 0);

		// --------------------------------------------------------------------------------------------- Regle de la mort
		const ESession Modes[3] = { ESession::Free, ESession::Story, ESession::Replay };
		int BadDeath = 0;
		for (const ESession Mode : Modes)
		{
			for (int Li = 0; Li < 2; ++Li)
			{
				for (int Ne = 0; Ne < 2; ++Ne)
				{
					const bool bRestart = Mode == ESession::Replay || (Mode == ESession::Story && Li != 0);
					const EDeathRule Want = Ne != 0 ? EDeathRule::RespawnInPlace : (bRestart ? EDeathRule::RestartLevel : EDeathRule::BackToStart);
					BadDeath += DeathRuleFor(Mode, Li != 0, Ne != 0) == Want ? 0 : 1;
				}
			}
		}
		Line("regle de la mort : table complete (3 sessions x lineaire x reseau, " + std::to_string(BadDeath) + " ecart)", BadDeath == 0);
		Line("histoire lineaire en solo : le niveau recommence (meme graine)", DeathRuleFor(ESession::Story, true, false) == EDeathRule::RestartLevel);
		Line("histoire classique en solo : retour au Level 0", DeathRuleFor(ESession::Story, false, false) == EDeathRule::BackToStart);
		Line("REJOUER en solo : le niveau rejoue recommence (lineaire ou non)",
			DeathRuleFor(ESession::Replay, false, false) == EDeathRule::RestartLevel && DeathRuleFor(ESession::Replay, true, false) == EDeathRule::RestartLevel);
		Line("partie en reseau : reveil dans le niveau, quel que soit le parcours ou la session",
			DeathRuleFor(ESession::Story, true, true) == EDeathRule::RespawnInPlace && DeathRuleFor(ESession::Story, false, true) == EDeathRule::RespawnInPlace
			&& DeathRuleFor(ESession::Replay, false, true) == EDeathRule::RespawnInPlace && DeathRuleFor(ESession::Free, false, true) == EDeathRule::RespawnInPlace);
		Line("mode libre en solo : retour au Level 0 (le parcours lineaire ne compte pas hors histoire)",
			DeathRuleFor(ESession::Free, false, false) == EDeathRule::BackToStart && DeathRuleFor(ESession::Free, true, false) == EDeathRule::BackToStart);

		// --------------------------------------------------------------------------------------------- Sorties des chunks
		struct FExitRow { bool bLinear; bool bFwd; bool bGuarded; bool bWant; };
		const FExitRow Rows[8] = {
			{ false, false, false, true }, { false, false, true, true }, { false, true, false, true }, { false, true, true, true },
			{ true, false, false, false }, { true, false, true, true }, { true, true, false, false }, { true, true, true, false },
		};
		int BadExit = 0;
		for (const FExitRow& Row : Rows)
		{
			BadExit += ChunkExitAllowed(Row.bLinear, Row.bFwd, Row.bGuarded) == Row.bWant ? 0 : 1;
		}
		Line("sorties des chunks : table des 8 cas (classique : toutes ; lineaire : aucune avec la sortie garantie, sinon la gardee seule) : "
			+ std::to_string(BadExit) + " ecart", BadExit == 0);

		// --------------------------------------------------------------------------------------------- Fil inchange
		{
			FState C0;
			FState L0;
			L0.Flags = Flag::Linear;
			L0.RunSeed = 41500u;
			const int Chapter[4] = { 0, 1, 37, 2 };
			bool bSame = true;
			for (int K = 0; K + 1 < 4; ++K)
			{
				OnProgress(C0, Chapter[K], Chapter[K + 1]);
				OnProgress(L0, Chapter[K], Chapter[K + 1]);
				OnArrive(C0, Chapter[K + 1]);
				OnArrive(L0, Chapter[K + 1]);
				bSame = bSame && C0.Reached == L0.Reached && C0.Done == L0.Done && C0.Resume == L0.Resume;
			}
			Line("0 -> 1 -> 37 -> 2 : meme plus loin atteint, memes etapes, meme reprise, parcours et graine gardes",
				bSame && L0.Flags == Flag::Linear && L0.RunSeed == 41500u && L0.Resume == 2);
			int BadAccess = 0;
			FState Ca;
			FState La;
			La.Flags = Flag::Linear;
			La.RunSeed = 77u;
			for (int I = 0; I + 1 < NumSteps; ++I)
			{
				for (int J = 0; J < NumSteps; ++J)
				{
					BadAccess += CanEnter(Ca, Order[J]) == CanEnter(La, Order[J]) ? 0 : 1;
				}
				BadAccess += ProgressTarget(Ca, Order[I], AvailAll, nullptr) == ProgressTarget(La, Order[I], AvailAll, nullptr) ? 0 : 1;
				OnProgress(Ca, Order[I], Order[I + 1]);
				OnProgress(La, Order[I], Order[I + 1]);
			}
			Line("acces de l'hote et destination de progression identiques en lineaire (12 x 12 paires, " + std::to_string(BadAccess) + " ecart)", BadAccess == 0);
			BRContent::EChannel Pub = BRContent::EChannel::Public;
			BRContent::EChannel Int = BRContent::EChannel::Internal;
			FState Cp;
			FState Lp;
			Lp.Flags = Flag::Linear;
			Lp.RunSeed = 41500u;
			PlayCampaign(Pub, Cp);
			PlayCampaign(Pub, Lp);
			Line("reprise (CONTINUER) identique en lineaire : fin du chapitre 1, puis chapitre 2 publie",
				ResumePoint(Cp, AvailIn, &Pub) == ResumePoint(Lp, AvailIn, &Pub) && ResumePoint(Cp, AvailIn, &Int) == ResumePoint(Lp, AvailIn, &Int)
				&& ResumePoint(Lp, AvailIn, &Int) == 3 && Cp.ChapterEnds == Lp.ChapterEnds);
			// Mort en solo dans un niveau lineaire : le niveau recommence ; l'arrivee dans le meme niveau ne change pas l'histoire
			FState Before = L0;
			OnArrive(L0, 2);
			Line("niveau lineaire recommence apres une mort : reprise, etapes et plus loin atteint inchanges, meme graine",
				L0.Resume == Before.Resume && L0.Done == Before.Done && L0.Reached == Before.Reached && LevelSeedFor(L0, 2) == LevelSeedFor(Before, 2));
		}

		// --------------------------------------------------------------------------------------------- Ecriture et migration
		{
			const FWritePolicy Pst = WritePolicyFor(ESession::Story);
			const FWritePolicy Prp = WritePolicyFor(ESession::Replay);
			const FWritePolicy Pfr = WritePolicyFor(ESession::Free);
			const bool bStory = Pst.bCharacter && Pst.bDeath && Pst.bSession && Pst.bCampaign && Pst.bStory && Pst.bPlayTime && Pst.bJournal && !Pst.bJournalUnion;
			const bool bReplay = !Prp.bCharacter && !Prp.bDeath && !Prp.bSession && !Prp.bCampaign && !Prp.bStory && !Prp.bPlayTime && !Prp.bJournal && Prp.bJournalUnion;
			const bool bFree = !Pfr.bCharacter && !Pfr.bDeath && !Pfr.bSession && !Pfr.bCampaign && !Pfr.bStory && !Pfr.bPlayTime && !Pfr.bJournal && !Pfr.bJournalUnion;
			Line("regles d'ecriture inchangees (histoire : tout ; REJOUER : journal par union ; libre : rien)", bStory && bReplay && bFree);
			// Partie lineaire : REJOUER et le mode developpeur n'ecrivent ni parcours ni graine (ni rien d'autre)
			FRecord Rec;
			FPlay Camp;
			Camp.Items = 5;
			Camp.Level = 2;
			Camp.Seed = LevelSeedFor(Lin, 2);
			Camp.StoryFlags = Flag::Linear;
			Camp.RunSeed = 41500u;
			Write(Pst, Rec, Camp);
			const FRecord Kept = Rec;
			Line("nouvelle partie (histoire) : parcours lineaire et graine du parcours enregistres", Rec.StoryFlags == Flag::Linear && Rec.RunSeed == 41500u);
			FPlay Rep;
			Rep.Level = 1;
			Rep.Seed = 99u;
			Write(Prp, Rec, Rep);
			Death(Prp, Rec, Rep);
			FPlay Dev = Rep;
			Dev.bDead = false;
			Write(Pfr, Rec, Dev);
			Death(Pfr, Rec, Dev);
			Line("REJOUER et mode developpeur dans une partie lineaire : parcours, graine et partie inchanges", Rec == Kept && Rec.StoryFlags == Flag::Linear && Rec.RunSeed == 41500u);
			int NumLinear = 0;
			const std::vector<std::vector<int>> Histories = { { 0 }, { 0, 1 }, { 0, 37 }, { 0, 1, 37, 2 }, { 0, 1, 4, 5 }, { 0, 37, 4, 5, 6, 8, 9, 10, 11 }, { 0, 1, 37, 4 } };
			for (const std::vector<int>& H : Histories)
			{
				for (int Fmt = 0; Fmt <= 4; ++Fmt)
				{
					const FMigration M = Migrate(H, H.back(), 0, Fmt);
					NumLinear += (IsLinear(M.State) || M.State.Flags != 0 || M.State.RunSeed != 0) ? 1 : 0;
				}
			}
			Line("migration des anciennes parties (7 historiques x formats 0 a 4) : toujours le parcours classique (" + std::to_string(NumLinear) + " lineaire)",
				NumLinear == 0);
		}
	}

	std::printf("\nVerifications : %d, echecs : %d\nRESULTAT : %s\n", Checks, Failures, Failures == 0 ? "OK" : "ECHEC");
	return Failures == 0 ? 0 : 1;
}
