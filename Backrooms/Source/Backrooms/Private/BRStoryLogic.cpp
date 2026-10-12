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
		if (From == Poolrooms)
		{
			S.Parked = -1; // l'etape manquante est faite : la progression d'avant reprend
		}
		const int I = IndexOf(To);
		if (I >= 0)
		{
			S.Reached = I > S.Reached ? I : S.Reached;
			S.Resume = To;
		}
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

	FMigration MigrateLegacy(const FLegacy& L)
	{
		FMigration M;
		FState& S = M.State;
		const int Pool = IndexOf(Poolrooms);
		int MaxIdx = 0;
		int PoolPos = -1;
		for (int K = 0; K < L.NumExplored; ++K)
		{
			const int I = IndexOf(L.Explored[K]);
			MaxIdx = I > MaxIdx ? I : MaxIdx;
			if (L.Explored[K] == Poolrooms && PoolPos < 0)
			{
				PoolPos = K;
			}
		}
		const int CurIdx = IndexOf(L.Current);
		MaxIdx = CurIdx > MaxIdx ? CurIdx : MaxIdx;
		// Preuve de traversee : le niveau decouvert juste apres les Poolrooms est le Niveau 4, la destination de leur
		// echelle dans les versions anterieures (seul acces au Niveau 4 sans passer par les Niveaux 1 ou 3, pas encore
		// decouverts). Une simple visite, ou la redirection vers le Niveau 1 de la v4.12 (le Niveau 1 s'atteint aussi
		// depuis le Level 0), ne prouve rien
		M.bPoolroomsCredited = PoolPos >= 0 && PoolPos + 1 < L.NumExplored && L.Explored[PoolPos + 1] == 4;
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
			// Allee au-dela des Poolrooms sans les traverser : reprise dans les Poolrooms, la progression d'avant est garee
			// et reprendra a la sortie du sas
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
