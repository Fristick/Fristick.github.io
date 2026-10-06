// v4.12 : disponibilite des niveaux par version (voir BRContentLogic.h). C++ pur.
#include "BRContentLogic.h"
#include "BRMissionLogic.h"

namespace BRContent
{
	namespace ContentImpl
	{
		// Configuration centrale : etape et lot de chaque niveau (Docs/PLAN_PUBLICATION.md)
		const FLevelEntry Table[] = {
			// Lot 1, lancement : le Seuil
			{ 0, EStage::Published, 1 },
			{ 1, EStage::Published, 1 },
			{ 2, EStage::Published, 1 },
			{ 37, EStage::Published, 1 },
			// Lot 2, en test interne : la Station et les Bureaux
			{ 3, EStage::Internal, 2 },
			{ 4, EStage::Internal, 2 },
			// Lot 3 : l'Hotel et le Noir
			{ 5, EStage::Developed, 3 },
			{ 6, EStage::Developed, 3 },
			// Lot 4 : les Grottes, la Banlieue, le Champ
			{ 8, EStage::Developed, 4 },
			{ 9, EStage::Developed, 4 },
			{ 10, EStage::Developed, 4 },
			// Lot 5 : la Ville et le dernier quai (fin de la campagne)
			{ 11, EStage::Developed, 5 },
		};
		constexpr int NumTable = static_cast<int>(sizeof(Table) / sizeof(Table[0]));

		struct FRedirect
		{
			int From;
			int Target;
			int To;
		};
		// Tant que le Niveau 4 n'est pas disponible :
		//   - l'echelle de sortie des Poolrooms remonte vers le Niveau 1 : les deux routes du Niveau 0 se rejoignent avant
		//     la fin du lot 1 (Niveau 2) ;
		//   - l'ascenseur du Niveau 1 (celui que la mission alimente et appelle) descend au Niveau 2 : la mission garde son
		//     sens, et aucun passage du lot 1 n'est condamne
		const FRedirect Redirects[] = {
			{ 37, 4, 1 },
			{ 1, 4, 2 },
		};

		const FLevelEntry* Find(int Level)
		{
			for (const FLevelEntry& E : Table)
			{
				if (E.Level == Level)
				{
					return &E;
				}
			}
			return nullptr;
		}

		bool StageOpen(EStage Stage, EChannel Channel)
		{
			switch (Channel)
			{
			case EChannel::All: return true;
			case EChannel::Internal: return Stage == EStage::Published || Stage == EStage::Internal;
			default: return Stage == EStage::Published;
			}
		}

		uint32_t Fnv(uint32_t H, uint32_t V)
		{
			for (int I = 0; I < 4; ++I)
			{
				H ^= (V >> (I * 8)) & 0xFFu;
				H *= 16777619u;
			}
			return H;
		}
	}

	const FLevelEntry* Levels(int& OutCount)
	{
		OutCount = ContentImpl::NumTable;
		return ContentImpl::Table;
	}

	bool IsKnown(int Level)
	{
		return ContentImpl::Find(Level) != nullptr;
	}

	EStage StageOf(int Level)
	{
		const FLevelEntry* E = ContentImpl::Find(Level);
		return E ? E->Stage : EStage::Developed;
	}

	int LotOf(int Level)
	{
		const FLevelEntry* E = ContentImpl::Find(Level);
		return E ? E->Lot : 0;
	}

	bool IsAvailable(int Level, EChannel Channel)
	{
		const FLevelEntry* E = ContentImpl::Find(Level);
		return E && ContentImpl::StageOpen(E->Stage, Channel);
	}

	int CurrentLot(EChannel Channel)
	{
		int Lot = 0;
		for (int L = 1; L < 100; ++L)
		{
			bool bAny = false, bAll = true;
			for (const FLevelEntry& E : ContentImpl::Table)
			{
				if (E.Lot == L)
				{
					bAny = true;
					bAll &= ContentImpl::StageOpen(E.Stage, Channel);
				}
			}
			if (!bAny || !bAll)
			{
				break;
			}
			Lot = L;
		}
		return Lot;
	}

	FExitResolution ResolveExit(int From, int Target, bool bGuarded, const FExitList& FromExits, EChannel Channel)
	{
		FExitResolution R;
		if (Target == BRMission::EndingTarget)
		{
			R.Kind = IsAvailable(From, Channel) && From == 11 ? EExit::Ending : EExit::ChapterEnd;
			return R;
		}
		if (Target < 0)
		{
			R.Kind = EExit::Go; // tire au hasard parmi les niveaux disponibles (RandomChoices)
			R.Target = -1;
			return R;
		}
		if (IsAvailable(Target, Channel))
		{
			R.Kind = EExit::Go;
			R.Target = Target;
			return R;
		}
		for (const ContentImpl::FRedirect& D : ContentImpl::Redirects)
		{
			if (D.From == From && D.Target == Target && IsAvailable(D.To, Channel))
			{
				R.Kind = EExit::Go;
				R.Target = D.To;
				R.bRedirected = true;
				return R;
			}
		}
		if (bGuarded)
		{
			// Sortie de progression vers un niveau pas encore publie : si une autre sortie gardee mene quelque part, la
			// mission la gardera (celle-ci est condamnee) ; sinon, c'est la fin du contenu disponible
			for (int I = 0; I < FromExits.Num; ++I)
			{
				const int T2 = FromExits.Targets[I];
				if (!FromExits.Guarded[I] || T2 == Target || T2 < 0)
				{
					continue;
				}
				bool bLeads = IsAvailable(T2, Channel);
				for (const ContentImpl::FRedirect& D : ContentImpl::Redirects)
				{
					bLeads |= D.From == From && D.Target == T2 && IsAvailable(D.To, Channel);
				}
				if (bLeads)
				{
					R.Kind = EExit::Sealed;
					return R;
				}
			}
			R.Kind = EExit::ChapterEnd;
			R.Target = ChapterEndTarget;
			return R;
		}
		R.Kind = EExit::Sealed;
		return R;
	}

	int AdaptedForward(int From, int BaseForward, const FExitList& FromExits, EChannel Channel)
	{
		if (BaseForward < 0 || BaseForward == BRMission::EndingTarget)
		{
			return BaseForward;
		}
		if (ResolveExit(From, BaseForward, true, FromExits, Channel).Kind != EExit::Sealed)
		{
			return BaseForward;
		}
		for (int I = 0; I < FromExits.Num; ++I)
		{
			const int T2 = FromExits.Targets[I];
			if (FromExits.Guarded[I] && T2 != BaseForward && ResolveExit(From, T2, true, FromExits, Channel).Kind == EExit::Go)
			{
				return T2;
			}
		}
		return BaseForward;
	}

	int RandomChoices(EChannel Channel, int Exclude, int* Out, int Max)
	{
		int N = 0;
		for (const FLevelEntry& E : ContentImpl::Table)
		{
			if (E.Level != Exclude && ContentImpl::StageOpen(E.Stage, Channel) && N < Max)
			{
				Out[N++] = E.Level;
			}
		}
		return N;
	}

	uint32_t Signature(EChannel Channel)
	{
		uint32_t H = 2166136261u;
		H = ContentImpl::Fnv(H, static_cast<uint32_t>(NetVersion));
		H = ContentImpl::Fnv(H, static_cast<uint32_t>(CurrentLot(Channel)));
		for (const FLevelEntry& E : ContentImpl::Table)
		{
			if (ContentImpl::StageOpen(E.Stage, Channel))
			{
				H = ContentImpl::Fnv(H, static_cast<uint32_t>(E.Level));
			}
		}
		return H;
	}

	int ResumeLevel(int Current, const int* Explored, int NumExplored, EChannel Channel, bool* bOutMoved)
	{
		const bool bMoved = !IsAvailable(Current, Channel);
		if (bOutMoved)
		{
			*bOutMoved = bMoved;
		}
		bool bExplored = false;
		for (int I = 0; I < NumExplored; ++I)
		{
			bExplored |= Explored[I] == Current;
		}
		if (!bMoved)
		{
			return bExplored ? Current : 0;
		}
		for (int I = NumExplored - 1; I >= 0; --I)
		{
			if (IsAvailable(Explored[I], Channel))
			{
				return Explored[I];
			}
		}
		return 0;
	}

	EJoin CheckJoin(int ClientNet, bool bHasSignature, uint32_t ClientSignature, EChannel HostChannel)
	{
		if (ClientNet != NetVersion)
		{
			return EJoin::Version;
		}
		if (!bHasSignature || ClientSignature != Signature(HostChannel))
		{
			return EJoin::Content;
		}
		return EJoin::Ok;
	}
}
