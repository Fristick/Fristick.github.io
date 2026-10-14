// v4.13 : fil narratif de la campagne (voir BRStoryLogic.h). C++ pur.
#include "BRStoryLogic.h"

namespace BRStory
{
	const int Order[NumSteps] = { 0, 1, 37, 2, 3, 4, 5, 6, 8, 9, 10, 11 };

	int IndexOf(int Level)
	{
		for (int I = 0; I < NumSteps; ++I)
		{
			if (Order[I] == Level)
			{
				return I;
			}
		}
		return -1;
	}

	int NextOf(int Level)
	{
		const int I = IndexOf(Level);
		if (I < 0)
		{
			return -1;
		}
		return I + 1 < NumSteps ? Order[I + 1] : Ending;
	}

	int PrevOf(int Level)
	{
		const int I = IndexOf(Level);
		return I > 0 ? Order[I - 1] : -1;
	}

	bool IsDone(const FState& S, int Level)
	{
		const int I = IndexOf(Level);
		return I >= 0 && (S.Done & (1u << I)) != 0;
	}

	void SetDone(FState& S, int Level)
	{
		const int I = IndexOf(Level);
		if (I >= 0)
		{
			S.Done |= 1u << I;
		}
	}

	EAccess CanEnter(const FState& S, int To)
	{
		const int I = IndexOf(To);
		if (I < 0)
		{
			return EAccess::Unknown;
		}
		if (I > IndexOf(Poolrooms) && !IsDone(S, Poolrooms))
		{
			return EAccess::PoolroomsRequired;
		}
		if (I <= S.Reached)
		{
			return EAccess::Allowed;
		}
		const int Prev = PrevOf(To);
		return Prev >= 0 && IsDone(S, Prev) ? EAccess::Allowed : EAccess::NotReached;
	}

	int ProgressTarget(const FState& S, int From, FAvailable Available, void* Ctx)
	{
		if (From == Poolrooms && S.Parked >= 0 && IndexOf(S.Parked) > IndexOf(Poolrooms) && (!Available || Available(S.Parked, Ctx)))
		{
			return S.Parked;
		}
		return NextOf(From);
	}

	void OnProgress(FState& S, int From, int To)
	{
		SetDone(S, From);
		// v4.14 : le niveau gare reste note jusqu'a la reprise reussie de sa session (OnParkedRestored) : un niveau gare
		// indisponible dans cette version (canal public) attend le chapitre qui le publiera
		const int I = IndexOf(To);
		if (I >= 0)
		{
			S.Reached = I > S.Reached ? I : S.Reached;
			S.Resume = To;
		}
	}

	void OnParkedRestored(FState& S)
	{
		S.Parked = -1;
	}

	bool WantsParked(const FState& S, int Level)
	{
		return S.Parked >= 0 && Level == S.Parked && IsDone(S, Poolrooms);
	}

	void OnArrive(FState& S, int Level)
	{
		if (IndexOf(Level) >= 0 && CanEnter(S, Level) == EAccess::Allowed)
		{
			S.Resume = Level;
		}
	}

	void OnChapterEnd(FState& S, int Level, int Lot)
	{
		SetDone(S, Level);
		if (Lot > 0 && Lot < 32)
		{
			S.ChapterEnds |= 1u << Lot;
		}
		// L'histoire reprendra au niveau suivant du fil, des qu'il sera disponible (mise a jour suivante) ; d'ici la,
		// ResumePoint redescend au dernier niveau disponible (celui de la fin du chapitre)
		const int Next = NextOf(Level);
		S.Resume = (Next >= 0 && Next != Ending) ? Next : Level;
		const int I = IndexOf(S.Resume);
		S.Reached = I > S.Reached ? I : S.Reached;
	}

	int ResumePoint(const FState& S, FAvailable Available, void* Ctx)
	{
		const auto Avail = [&](int Level) { return Level >= 0 && Level != Ending && (!Available || Available(Level, Ctx)); };
		// v4.14 : ancienne partie : apres la traversee des Poolrooms, sa progression garee reprend des que son niveau est
		// disponible (ajout d'un chapitre)
		if (S.Parked >= 0 && IsDone(S, Poolrooms) && Avail(S.Parked) && CanEnter(S, S.Parked) == EAccess::Allowed)
		{
			return S.Parked;
		}
		const int R = IndexOf(S.Resume) >= 0 ? S.Resume : 0;
		if (Avail(R) && CanEnter(S, R) == EAccess::Allowed)
		{
			return R;
		}
		for (int I = IndexOf(R); I >= 0; --I)
		{
			if (Avail(Order[I]) && CanEnter(S, Order[I]) == EAccess::Allowed)
			{
				return Order[I];
			}
		}
		return 0;
	}

	FWritePolicy WritePolicyFor(ESession Mode)
	{
		FWritePolicy P;
		if (Mode == ESession::Story)
		{
			P.bCharacter = P.bDeath = P.bSession = P.bCampaign = P.bStory = P.bPlayTime = P.bJournal = true;
		}
		else if (Mode == ESession::Replay)
		{
			P.bJournalUnion = true;
		}
		return P;
	}

	EPoolProof PoolroomsProof(const FLegacy& L)
	{
		int PoolPos = -1;
		int FourPos = -1;
		for (int K = 0; K < L.NumExplored; ++K)
		{
			if (L.Explored[K] == Poolrooms && PoolPos < 0)
			{
				PoolPos = K;
			}
			if (L.Explored[K] == 4 && FourPos < 0)
			{
				FourPos = K;
			}
		}
		if (PoolPos < 0 && L.Current != Poolrooms)
		{
			return EPoolProof::Absent;
		}
		// La liste ne garde que les PREMIERES decouvertes : elle ne dit pas d'ou l'on est entre dans un niveau. Le Niveau 4
		// a toujours ete atteignable depuis 1 (ascenseur), 3 (ascenseur), 5 (retour) et 37 (echelle), et au hasard depuis
		// le Niveau 11 (porte d'immeuble, exploration apres une fin). Son entree prouve l'echelle des Poolrooms seulement
		// si aucun de ces autres acces n'etait deja decouvert, et si la console n'a pas pu ajouter de niveau (format 1)
		if (L.Format < 2 || PoolPos < 0 || FourPos < PoolPos)
		{
			return EPoolProof::Ambiguous;
		}
		for (int K = 0; K < FourPos; ++K)
		{
			const int Level = L.Explored[K];
			if (Level == 1 || Level == 3 || Level == 5 || Level == 11)
			{
				return EPoolProof::Ambiguous;
			}
		}
		return EPoolProof::Proven;
	}

	FMigration MigrateLegacy(const FLegacy& L)
	{
		FMigration M;
		FState& S = M.State;
		const int Pool = IndexOf(Poolrooms);
		int MaxIdx = 0;
		for (int K = 0; K < L.NumExplored; ++K)
		{
			const int I = IndexOf(L.Explored[K]);
			MaxIdx = I > MaxIdx ? I : MaxIdx;
		}
		const int CurIdx = IndexOf(L.Current);
		MaxIdx = CurIdx > MaxIdx ? CurIdx : MaxIdx;
		// v4.14 : traversee creditee seulement sur preuve (PoolroomsProof) ; absente ou ambigue, l'etape est organisee
		M.PoolProof = PoolroomsProof(L);
		M.bPoolroomsCredited = M.PoolProof == EPoolProof::Proven;
		// Etapes depassees : un niveau explore avant le plus loin atteint a ete quitte vers l'avant
		for (int K = 0; K < L.NumExplored; ++K)
		{
			const int Level = L.Explored[K];
			const int I = IndexOf(Level);
			if (I >= 0 && I < MaxIdx && Level != Poolrooms)
			{
				SetDone(S, Level);
			}
		}
		if (MaxIdx > 0)
		{
			SetDone(S, 0); // le Level 0 a ete quitte
		}
		if (M.bPoolroomsCredited)
		{
			SetDone(S, Poolrooms);
		}
		S.Reached = MaxIdx;
		S.Resume = CurIdx >= 0 ? L.Current : 0;
		if (MaxIdx > Pool && !M.bPoolroomsCredited && CurIdx > Pool)
		{
			// Allee au-dela des Poolrooms sans traversee prouvee : reprise dans les Poolrooms, la progression d'avant est
			// garee (graine, objets, mission, documents) et reprendra apres la traversee
			S.Resume = Poolrooms;
			S.Parked = L.Current;
			S.Events |= Event::MigratedDetour;
			M.bParkSession = true;
		}
		for (int Lot = 1; Lot <= 5; ++Lot)
		{
			if (L.Endings & (1u << (1 + Lot)))
			{
				S.ChapterEnds |= 1u << Lot;
			}
		}
		if (S.Done & 1u)
		{
			S.Events |= Event::IntroWake | Event::DoorOpened;
		}
		return M;
	}
}
