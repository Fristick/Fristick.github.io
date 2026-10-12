// v4.13 : Level 6 sans lumiere, cote monde : aile d'essai (BRLidarLogic), reperes au sol de l'equipe de L., emetteur de
// parasites du dernier passage, regle des Smilers (une impulsion large s'entend). Le scanner lui-meme est local
// (ABRLidar, BRLidar.cpp) ; la mission est tenue par l'hote comme les autres (BRMissionWorld.cpp).
#include "BRWorld.h"
#include "BRMission.h"
#include "BRLevels.h"
#include "Backrooms.h"

#include "Algo/Reverse.h"

namespace BRM = BRMission;

bool ABRWorld::IsLidarLevel() const
{
	return Current && Def().bLidar;
}

FIntPoint ABRWorld::LidarCell(int32 U, int32 V) const
{
	int X = 0;
	int Y = 0;
	BRLidar::ToCell(LidarWing, U, V, X, Y);
	return FIntPoint(X, Y);
}

void ABRWorld::ComputeLidarMarks()
{
	LidarMarks.Reset();
	LidarMarkDirs.Reset();
	if (!IsLidarLevel() || !IsMissionActive() || MissionPlan.Level != 6 || MissionSpots.Num() != MissionPlan.NumDevices)
	{
		return;
	}
	// Depart : la mire de calibrage (zone sure) ; arrivee : la galerie devant l'entree de l'aile (ou la salle de la mission)
	int32 Calib = -1;
	int32 Door = -1;
	for (int32 I = 0; I < MissionPlan.NumDevices; ++I)
	{
		if (MissionPlan.Devices[I].Role == BRM::R_LidarTarget && Calib < 0)
		{
			Calib = I;
		}
		if (MissionPlan.Devices[I].Role == BRM::R_LidarDoor && Door < 0)
		{
			Door = I;
		}
	}
	if (Calib < 0 || Door < 0)
	{
		return;
	}
	const FIntPoint From = MissionSpots[Calib].Cell;
	const FIntPoint To = MissionSpots[Door].Cell;
	// Parcours en largeur sur les cellules praticables, hors de l'aile (la zone de danger se traverse au scanner)
	TMap<FIntPoint, FIntPoint> Parent;
	TArray<FIntPoint> Queue;
	Parent.Add(From, From);
	Queue.Add(From);
	static const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	bool bFound = false;
	for (int32 Head = 0; Head < Queue.Num() && Queue.Num() < 8000 && !bFound; ++Head)
	{
		const FIntPoint Cur = Queue[Head];
		for (const FIntPoint& D : Dirs)
		{
			const FIntPoint N = Cur + D;
			if (Parent.Contains(N) || !IsWalkable(N) || IsPitRoomCell(N.X, N.Y) || !CanStep(Cur, N))
			{
				continue;
			}
			Parent.Add(N, Cur);
			Queue.Add(N);
			if (N == To)
			{
				bFound = true;
				break;
			}
		}
	}
	if (!bFound)
	{
		UE_LOG(LogBackrooms, Warning, TEXT("[Lidar] Aucun chemin de la mire (%d, %d) a l'entree de l'aile (%d, %d) : pas de reperes au sol"), From.X, From.Y, To.X, To.Y);
		return;
	}
	TArray<FIntPoint> Path;
	for (FIntPoint C = To; C != From; C = Parent[C])
	{
		Path.Add(C);
	}
	Path.Add(From);
	Algo::Reverse(Path);
	// Un repere toutes les deux cellules, a partir de la deuxieme, oriente vers la cellule suivante (chevron au sol)
	for (int32 I = 1; I + 1 < Path.Num(); I += 2)
	{
		const FVector P = CellCenter(Path[I], 0.f);
		const FVector Next = CellCenter(Path[I + 1], 0.f);
		FVector Pos = P;
		Pos.Z = FloorZAt(P);
		LidarMarks.Add(Pos);
		LidarMarkDirs.Add((Next - P).GetSafeNormal2D());
	}
	UE_LOG(LogBackrooms, Log, TEXT("[Lidar] Reperes de l'equipe de L. : %d au sol, chemin de %d cellules de la mire a l'aile"), LidarMarks.Num(), Path.Num());
}

bool ABRWorld::GetLidarStatic(FVector& OutLocation) const
{
	if (!IsLidarLevel() || !IsMissionActive() || MissionPlan.Level != 6)
	{
		return false;
	}
	BRM::FEval E;
	GetMissionEval(E);
	if (!E.bSolved)
	{
		return false;
	}
	for (int32 I = 0; I < MissionPlan.NumDevices && I < MissionSpots.Num(); ++I)
	{
		if (MissionPlan.Devices[I].Role == BRM::R_LidarStatic)
		{
			OutLocation = MissionSpots[I].Pos;
			return true;
		}
	}
	return false;
}

bool ABRWorld::IsLidarCalibrated() const
{
	if (!IsLidarLevel())
	{
		return true;
	}
	if (!IsMissionActive() || MissionPlan.Level != 6)
	{
		return true; // mission absente (placement impossible signale) : rien a proteger
	}
	BRM::FEval E;
	GetMissionEval(E);
	return E.bSolved || E.Steps[0] == BRM::EStep::Done;
}

void ABRWorld::ReportLidarPulse(const FVector& Location)
{
	if (!HasAuthority() || !IsLidarLevel())
	{
		return;
	}
	ReportNoise(Location, BRLidar::PulseNoiseRadius);
}
