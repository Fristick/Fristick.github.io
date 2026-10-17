// v4.11 : missions de niveau dans le monde : placement deterministe des mecanismes dans une zone finie, etat tenu par
// l'hote et replique, validation des actions, sorties conditionnees par la mission, sortie de groupe, campagne et fin.
#include "BRWorld.h"
#include "BRMission.h"
#include "BRMissionSolver.h"
#include "BRStoryLogic.h"
#include "BRGatherLogic.h"
#include "BRCharacter.h"
#include "BREntity.h"
#include "BRInteractables.h"
#include "BRPlayerController.h"
#include "BRHUD.h"
#include "BRAssets.h"
#include "BRLevels.h"
#include "BRLoc.h"
#include "BRKeys.h"
#include "Backrooms.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace BRM = BRMission;

namespace
{
	float YawOf(int32 DX, int32 DY)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(DY), static_cast<float>(DX)));
	}

	const FIntPoint GMissionDirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	/** Zone de placement (metres de trajet depuis le depart) : 0 pres du depart ... 3 loin ; 4 : salle de la mission */
	void ZoneBand(int32 Zone, float& OutMin, float& OutMax)
	{
		switch (Zone)
		{
		case 0: OutMin = 5.f; OutMax = 25.f; break;
		case 1: OutMin = 15.f; OutMax = 40.f; break;
		case 2: OutMin = 28.f; OutMax = 60.f; break;
		case 3: OutMin = 45.f; OutMax = 85.f; break;
		default: OutMin = 32.f; OutMax = 70.f; break;
		}
	}

	/** Sortie de progression d'un niveau. v4.13 : l'ordre de la campagne (BRStoryLogic : 0 -> 1 -> 37 -> 2 -> 3 -> ... ->
	 *  11 -> dernier quai), le meme dans toutes les versions */
	int32 BaseForwardTarget(int32 Level)
	{
		const int Next = BRStory::NextOf(Level);
		return Next == BRStory::Ending ? BRM::EndingTarget : Next;
	}

	/** Sortie que la porte de la mission garde (la porte est posee devant). v4.12 : adaptee a cette version : si la
	 *  sortie de la base est condamnee (niveau pas encore disponible), la mission garde une autre sortie disponible */
	int32 ForwardTarget(int32 Level)
	{
		return BRLevels::AdaptedForward(Level, BaseForwardTarget(Level));
	}

	BRGather::FVec ToGather(const FVector& V)
	{
		BRGather::FVec Out;
		Out.X = static_cast<float>(V.X);
		Out.Y = static_cast<float>(V.Y);
		Out.Z = static_cast<float>(V.Z);
		return Out;
	}

	FVector FromGather(const BRGather::FVec& V)
	{
		return FVector(V.X, V.Y, V.Z);
	}

	/** v4.12 : forme d'une sortie pour le depart de groupe (pied, direction vers la piece, conduit d'une echelle) */
	BRGather::FExitShape GatherShapeOf(const ABRExit* E)
	{
		BRGather::FExitShape S;
		S.Style = E->IsClimbable() ? BRGather::EStyle::Ladder : (E->Style == EBRExitStyle::Barn ? BRGather::EStyle::Barn : BRGather::EStyle::Door);
		S.Foot = ToGather(E->GetActorLocation());
		const FVector Fwd = FRotator(0.f, E->GetActorRotation().Yaw, 0.f).Vector();
		S.Forward = ToGather(Fwd);
		S.Anchor = ToGather(E->IsClimbable() ? E->GetClimbAnchor() : E->GetActorLocation());
		S.TopZ = E->IsClimbable() ? E->GetClimbTopZ() : static_cast<float>(E->GetActorLocation().Z);
		return S;
	}

	BRGather::FExitShape GatherShapeOf(const FBRNetDeparture& D)
	{
		BRGather::FExitShape S;
		S.Style = static_cast<BRGather::EStyle>(FMath::Min<uint8>(D.Style, 2));
		S.Foot = ToGather(D.Foot);
		S.Forward = ToGather(D.Forward);
		S.Anchor = ToGather(D.Anchor);
		S.TopZ = D.TopZ;
		return S;
	}

	uint64 HoldKey(const ABRCharacter* By, int32 Device)
	{
		return (static_cast<uint64>(reinterpret_cast<UPTRINT>(By)) << 8) ^ static_cast<uint64>(Device & 0xFF);
	}

	/** v4.15 : mecanisme pose (repere de son emplacement, emprise de sa sorte, de son role et de son module) */
	BRMech::Clearance::FPlaced PlacedMissionDevice(const BRM::FPlan& Plan, const TArray<FBRMissionSpot>& Spots, int32 I, float WallThickness)
	{
		BRMech::Clearance::FPlaced P;
		P.Frame.X = static_cast<float>(Spots[I].Pos.X);
		P.Frame.Y = static_cast<float>(Spots[I].Pos.Y);
		P.Frame.Z = static_cast<float>(Spots[I].Pos.Z);
		P.Frame.Yaw = Spots[I].Yaw;
		P.Print = BRMech::Clearance::Of(Plan.Devices[I].Kind, Plan.Devices[I].Role, Spots[I].Module, WallThickness);
		return P;
	}

	/** v4.15 : invariant du placement (journal ; les tests du jeu le verifient aussi dans le decor construit) : aucun point
	 *  d'interaction dans le volume bloquant d'un autre mecanisme. v4.15 (correctifs) : aussi pour le module de secours */
	void LogMissionVolumeInvariant(const BRM::FPlan& Plan, const TArray<FBRMissionSpot>& Spots, float WallThickness, const TCHAR* Tier)
	{
		for (int32 I = 0; I < Plan.NumDevices && I < Spots.Num(); ++I)
		{
			if (Plan.Devices[I].Kind == BRM::EKind::Gate)
			{
				continue;
			}
			const BRMech::FVec3 Aim = BRMech::Clearance::AimPoint(PlacedMissionDevice(Plan, Spots, I, WallThickness));
			for (int32 J = 0; J < Plan.NumDevices && J < Spots.Num(); ++J)
			{
				if (J != I && BRMech::Clearance::InSolid(PlacedMissionDevice(Plan, Spots, J, WallThickness), Aim.X, Aim.Y, Aim.Z, BRMech::Clearance::Margin))
				{
					UE_LOG(LogBackrooms, Warning, TEXT("Mission du Niveau %d%s : point d'interaction du mecanisme %d dans le volume du mecanisme %d (graine %u)"), Plan.Level, Tier, I, J,
						Plan.Seed);
				}
			}
		}
	}
}

// =====================================================================================================================
// Etat
// =====================================================================================================================

bool ABRWorld::IsMissionActive() const
{
	return MissionGen >= 2 && MissionPlan.NumDevices > 0;
}

bool ABRWorld::IsLegacyObjectives() const
{
	return MissionGen == 1;
}

BRM::FCampaign ABRWorld::GetMissionCampaign() const
{
	BRM::FCampaign C;
	C.RouteBits = NetCampaign.RouteBits;
	C.OptionalFound = NetCampaign.OptionalFound;
	return C;
}

void ABRWorld::GetMissionEval(BRM::FEval& Out) const
{
	BRM::Evaluate(MissionPlan, MissionState, GetMissionCampaign(), Out);
}

ABRMissionDevice* ABRWorld::FindMissionDevice(int32 Index) const
{
	for (ABRMissionDevice* D : MissionDevices)
	{
		if (IsValid(D) && D->GetIndex() == Index)
		{
			return D;
		}
	}
	return nullptr;
}

TArray<uint8> ABRWorld::GetMissionBlob() const
{
	TArray<uint8> Out;
	if (IsMissionActive())
	{
		uint8 Buf[BRM::MaxBlob];
		const int32 N = BRM::Serialize(MissionPlan, MissionState, Buf, BRM::MaxBlob);
		Out.Append(Buf, N);
	}
	return Out;
}

void ABRWorld::SetCampaign(uint8 RouteBits, uint16 OptionalFound, uint8 Endings)
{
	if (!HasAuthority())
	{
		return;
	}
	NetCampaign.RouteBits = RouteBits & 7;
	NetCampaign.OptionalFound = OptionalFound;
	NetCampaign.Endings = Endings; // v4.12 : bit 1 fin, bit 2 variante, bits suivants : fins du contenu disponible (lots)
	ForceNetUpdate();
}

void ABRWorld::ClearMission()
{
	for (ABRMissionDevice* D : MissionDevices)
	{
		if (IsValid(D))
		{
			D->Destroy();
		}
	}
	MissionDevices.Reset();
	GameplayLights.Reset(); // v4.12 : les sources des mecanismes partent avec eux
	LocalWaters.Reset(); // v4.12 : l'eau locale des bassins et du sas aussi
	BridgeNav = FBridgeNav();
	for (ABRExit* E : MissionExits)
	{
		if (IsValid(E))
		{
			E->Destroy();
		}
	}
	MissionExits.Reset();
	MissionSpots.Reset();
	MissionExitSpots.Reset();
	MissionCells.Reset();
	MissionKeepOuts.Reset();
	MissionHoldTimes.Reset();
	MissionCooldowns.Reset();
	MissionPlan = BRM::FPlan();
	MissionState = BRM::FState();
	MissionGen = 0;
	bMissionSolvedSeen = false;
}

// =====================================================================================================================
// Mise en place (avant la construction des chunks : les cellules des mecanismes sont reservees)
// =====================================================================================================================

void ABRWorld::SetupMission()
{
	ClearMission();
	const int32 Level = GetLevelNumber();
	if (!BRM::HasMission(Level))
	{
		if (HasAuthority())
		{
			NetMission = FBRNetMission();
			NetMission.Serial = NetLevel.Serial;
		}
		return;
	}
	bool bWantFallback = false;
	uint8 WantTier = 0;
	MissionTier = 0;
	MissionNotice = 0;
	if (HasAuthority())
	{
		// Session reprise d'avant la v4.11 (version 1) : elle garde son ancien mode jusqu'a la sortie du niveau
		MissionGen = (bResumed && ResumeMissionGen > 0 && ResumeMissionGen < BRM::GenVersion) ? 1 : static_cast<uint8>(BRM::GenVersion);
	}
	else
	{
		const bool bMatch = NetMission.Serial == NetLevel.Serial && NetMission.Gen != 0;
		MissionGen = bMatch ? NetMission.Gen : static_cast<uint8>(BRM::GenVersion);
		bWantFallback = bMatch && NetMission.bFallback;
		// v4.12 : le client place exactement comme l'hote (normal, variante de secours ou module de secours)
		WantTier = bMatch ? NetMission.Tier : 0;
	}
	if (MissionGen >= 2)
	{
		BRM::FPlan Plan;
		if (bWantFallback)
		{
			BRM::BuildFallbackPlan(Level, Seed, Plan);
		}
		else
		{
			BRM::BuildPlan(Level, Seed, Plan);
		}
		TArray<FBRMissionSpot> Spots;
		TArray<FMissionExitSpot> Exits;
		TSet<FIntPoint> Cells;
		bool bPlaced = false;
		if (WantTier >= 2)
		{
			bPlaced = PlaceMissionModule(Plan, Spots, Exits, Cells);
			MissionTier = 2;
		}
		else
		{
			bPlaced = PlaceMission(Plan, Spots, Exits, Cells);
			MissionTier = Plan.bFallback ? 1 : 0;
			if (!bPlaced && !Plan.bFallback)
			{
				// Placement impossible avec cette graine : variante de secours deterministe, avant de placer les joueurs
				UE_LOG(LogBackrooms, Warning, TEXT("Mission du Niveau %d : placement impossible (graine %u), variante de secours"), Level, Seed);
				BRM::BuildFallbackPlan(Level, Seed, Plan);
				bPlaced = PlaceMission(Plan, Spots, Exits, Cells);
				MissionTier = 1;
			}
			if (!bPlaced)
			{
				// v4.12 : module de secours pres du depart (toujours possible) : la mission n'est jamais abandonnee, les
				// sorties ne s'ouvrent pas en silence
				UE_LOG(LogBackrooms, Warning, TEXT("Mission du Niveau %d : placement impossible (graine %u) : module de secours pres du depart"), Level, Seed);
				bPlaced = PlaceMissionModule(Plan, Spots, Exits, Cells);
				MissionTier = 2;
				MissionNotice = 1;
			}
		}
		if (!bPlaced)
		{
			// Ne devrait jamais arriver (le module de secours se pose toujours) : signale aux joueurs, sortie ouverte
			// explicitement plutot qu'une partie bloquee
			UE_LOG(LogBackrooms, Error, TEXT("Mission du Niveau %d : aucun placement possible (graine %u) : sortie ouverte, signalee aux joueurs"), Level, Seed);
			MissionGen = 0;
			MissionNotice = 4;
		}
		else
		{
			MissionPlan = Plan;
			MissionSpots = MoveTemp(Spots);
			MissionExitSpots = MoveTemp(Exits);
			MissionCells = MoveTemp(Cells);
			// v4.15 : zones libres des mecanismes et des sorties de la mission, avant la construction des chunks (rochers)
			for (int32 I = 0; I < MissionPlan.NumDevices && I < MissionSpots.Num(); ++I)
			{
				BRMech::Clearance::FPlaced P;
				P.Frame.X = static_cast<float>(MissionSpots[I].Pos.X);
				P.Frame.Y = static_cast<float>(MissionSpots[I].Pos.Y);
				P.Frame.Z = static_cast<float>(MissionSpots[I].Pos.Z);
				P.Frame.Yaw = MissionSpots[I].Yaw;
				P.Print = BRMech::Clearance::Of(MissionPlan.Devices[I].Kind, MissionPlan.Devices[I].Role, MissionSpots[I].Module, Def().WallThickness);
				MissionKeepOuts.Add(BRMech::Clearance::KeepOut(P));
			}
			for (const FMissionExitSpot& E : MissionExitSpots)
			{
				BRMech::FFrame F;
				F.X = static_cast<float>(E.Pos.X);
				F.Y = static_cast<float>(E.Pos.Y);
				F.Z = static_cast<float>(E.Pos.Z);
				F.Yaw = E.Yaw;
				MissionKeepOuts.Add(BRMech::Clearance::ExitKeepOut(F));
			}
			BRM::InitState(MissionPlan, MissionState);
			if (HasAuthority())
			{
				if (bResumed && ResumeMissionBlob.Num() > 0)
				{
					BRM::FState Restored;
					if (BRM::Deserialize(MissionPlan, ResumeMissionBlob.GetData(), ResumeMissionBlob.Num(), Restored))
					{
						MissionState = Restored;
						UE_LOG(LogBackrooms, Log, TEXT("Reprise : mission du Niveau %d restauree (%s)"), Level, (MissionState.Solved & 1) ? TEXT("resolue") : TEXT("en cours"));
						// v4.12 : placement d'une autre version : la progression est gardee, les mecanismes peuvent avoir bouge.
						// v4.15 : avis seulement dans les niveaux dont le placement a change (version 3 : le Level 6)
						if (ResumePlaceVersion != BRM::PlaceVersion && MissionNotice == 0 && BRM::PlacementMoved(Level, ResumePlaceVersion))
						{
							MissionNotice = 3;
						}
					}
					else if (BRM::MigrateState(MissionPlan, ResumeMissionBlob.GetData(), ResumeMissionBlob.Num(), Restored))
					{
						// v4.13 : ancienne mission du meme niveau (Niveau 6 : chaine de balises v4.12) convertie quand elle a un
						// equivalent (plaque lue -> calibrage, balises allumees -> postes, terminee -> terminee). La sauvegarde
						// d'origine est copiee avant d'etre reecrite ; les joueurs sont prevenus que la progression est gardee
						MissionState = Restored;
						BRM::FEval Conv;
						BRM::Evaluate(MissionPlan, MissionState, GetMissionCampaign(), Conv);
						UE_LOG(LogBackrooms, Log, TEXT("Reprise : mission du Niveau %d d'une version precedente convertie (graine %u) : %s, etapes %d/%d %d/%d %d/%d, sauvegarde copiee"),
							Level, Seed, Conv.bSolved ? TEXT("terminee") : TEXT("en cours"), Conv.Progress[0], Conv.Goal[0], Conv.Progress[1], Conv.Goal[1], Conv.Progress[2], Conv.Goal[2]);
						MissionNotice = 3;
						bSaveBackupRequested = true;
					}
					else
					{
						// v4.12 : jamais en silence : la sauvegarde d'origine est copiee avant d'etre reecrite, et les joueurs
						// sont prevenus que la mission de ce niveau recommence
						UE_LOG(LogBackrooms, Warning, TEXT("Reprise : etat de mission du Niveau %d ne correspond pas au plan (graine %u) : mission reprise du debut, sauvegarde copiee"), Level, Seed);
						MissionNotice = 2;
						bSaveBackupRequested = true;
					}
				}
			}
			else if (NetMission.Serial == NetLevel.Serial && NetMission.State.Num() > 0)
			{
				BRM::FState Received;
				if (BRM::Deserialize(MissionPlan, NetMission.State.GetData(), NetMission.State.Num(), Received))
				{
					MissionState = Received;
				}
			}
			SpawnMissionActors();
			// v4.13 : Level 6 : reperes au sol de l'equipe de L. (chemin de la mire a l'aile), calcules sur chaque machine
			if (IsLidarLevel())
			{
				ComputeLidarMarks();
			}
			UE_LOG(LogBackrooms, Log, TEXT("Mission du Niveau %d : %d mecanismes, %d sortie(s) garantie(s)%s"), Level, MissionPlan.NumDevices, MissionExitSpots.Num(),
				MissionPlan.bFallback ? TEXT(", variante de secours") : TEXT(""));
		}
	}
	// v4.13 : etape deja accomplie dans l'histoire (retour dans un niveau quitte par sa sortie de progression) : la
	// mission reste accomplie. Le solveur (celui des bancs, qui n'utilise que ce que le joueur voit) la resout sur le plan
	// de cette graine ; les effets (eau, passerelle, sorties) suivent l'etat comme a une reprise
	if (HasAuthority() && IsMissionActive() && !(bResumed && ResumeMissionBlob.Num() > 0))
	{
		EnsureStoryLoaded();
		if (HostSessionMode() == EBRSessionMode::Story && BRStory::IsDone(GetStory(), Level) && (MissionState.Solved & 1) == 0)
		{
			BRM::FSolver Solver(MissionPlan, MissionState, GetMissionCampaign());
			const bool bSolved = Solver.Solve(0) && (MissionState.Solved & 1) != 0;
			UE_LOG(LogBackrooms, Log, TEXT("[Story] Mission du Niveau %d deja accomplie dans l'histoire : %s (%d actions)"), Level,
				bSolved ? TEXT("resolue a l'arrivee") : TEXT("ECHEC du solveur, mission a refaire"), Solver.Actions);
			if (!bSolved)
			{
				BRM::InitState(MissionPlan, MissionState);
			}
		}
	}
	ResumeMissionGen = 0;
	ResumeMissionBlob.Reset();
	ResumePlaceVersion = 0;
	// Mission deja resolue a la reprise : pas d'annonce ni de bruit de porte a l'arrivee
	BRM::FEval E;
	GetMissionEval(E);
	bMissionSolvedSeen = E.bSolved;
	if (HasAuthority())
	{
		NetMission = FBRNetMission();
		NetMission.Gen = MissionGen;
		NetMission.bFallback = MissionPlan.bFallback;
		NetMission.Serial = NetLevel.Serial;
		PublishMission(255, 0, false);
	}
	else
	{
		OnMissionStateChanged(255, 0, false);
	}
	if (MissionNotice != 0)
	{
		ShowMissionNotice(MissionNotice);
	}
}

void ABRWorld::ShowMissionNotice(uint8 Notice)
{
	// Une fois par niveau et par machine, dans la langue de chacun
	if (NoticeShownSerial == LoadedSerial || Notice == 0)
	{
		return;
	}
	NoticeShownSerial = LoadedSerial;
	FText Msg;
	switch (Notice)
	{
	case 1:
		Msg = NSLOCTEXT("BR", "Mission.Notice.Module", "Mission de ce niveau : les m\u00e9canismes sont regroup\u00e9s pr\u00e8s du point de d\u00e9part (le d\u00e9cor ne permettait pas leur placement habituel).");
		break;
	case 2:
		Msg = NSLOCTEXT("BR", "Mission.Notice.NotRestored", "La mission de ce niveau n'a pas pu \u00eatre reprise telle quelle (sauvegarde d'une autre version) : elle recommence. Une copie de la sauvegarde d'origine est conserv\u00e9e.");
		break;
	case 3:
		Msg = NSLOCTEXT("BR", "Mission.Notice.Moved", "Les m\u00e9canismes de ce niveau ont \u00e9t\u00e9 r\u00e9am\u00e9nag\u00e9s par la mise \u00e0 jour : votre progression dans la mission est conserv\u00e9e.");
		break;
	default:
		Msg = NSLOCTEXT("BR", "Mission.Notice.Unavailable", "Mission indisponible dans ce niveau (erreur de g\u00e9n\u00e9ration signal\u00e9e) : la sortie reste ouverte.");
		break;
	}
	ABRHUD::Notify(this, Msg.ToString(), 9.f, FLinearColor(1.f, 0.85f, 0.5f));
}

bool ABRWorld::PlaceMission(const BRM::FPlan& Plan, TArray<FBRMissionSpot>& OutSpots, TArray<FMissionExitSpot>& OutExits, TSet<FIntPoint>& OutCells) const
{
	OutSpots.Reset();
	OutExits.Reset();
	OutCells.Reset();
	if (Plan.NumDevices <= 0)
	{
		return false;
	}
	const FBRLevelDef& D = Def();
	const float S = D.CellSize;
	const bool bBounded = D.BoundsChunks > 0;
	// v4.15 : plafond de la zone de mission (95 m de trajet). Seule l'entree de l'aile LiDAR du Level 6 peut le depasser
	// (voir 3.) ; tout le reste du placement reste dans ce plafond
	const int32 BaseMaxCells = FMath::CeilToInt(9500.f / S);

	// 1. Zone de mission finie : parcours en largeur depuis le depart, au sec, hors des salles de fosses
	auto Allowed = [&](const FIntPoint& C)
	{
		return IsCellInBounds(C.X, C.Y) && IsWalkable(C) && !IsPoolCell(C.X, C.Y) && !IsPitRoomCell(C.X, C.Y) && IsSafelyReachable(C);
	};
	FIntPoint Start(0, 0);
	if (!Allowed(Start))
	{
		bool bFound = false;
		for (int32 R = 1; R <= 3 && !bFound; ++R)
		{
			for (int32 X = -R; X <= R && !bFound; ++X)
			{
				for (int32 Y = -R; Y <= R && !bFound; ++Y)
				{
					if (Allowed(FIntPoint(X, Y)))
					{
						Start = FIntPoint(X, Y);
						bFound = true;
					}
				}
			}
		}
		if (!bFound)
		{
			return false;
		}
	}
	TMap<FIntPoint, int32> Dist;
	TArray<FIntPoint> Order;
	// v4.15 : parcours pour un plafond donne. Un plafond plus haut garde le meme debut d'ordre (cellules a BaseMaxCells
	// ou moins, dans le meme ordre) : filtrees au plafond de base, les cellules donnent les memes choix
	auto RunBfs = [&](int32 Cap)
	{
		Dist.Reset();
		Order.Reset();
		Dist.Add(Start, 0);
		Order.Add(Start);
		for (int32 Head = 0; Head < Order.Num() && Order.Num() < 6000; ++Head)
		{
			const FIntPoint Cur = Order[Head];
			const int32 CD = Dist[Cur];
			if (CD >= Cap)
			{
				continue;
			}
			for (const FIntPoint& Dir : GMissionDirs)
			{
				const FIntPoint N = Cur + Dir;
				if (Dist.Contains(N) || !Allowed(N) || !CanStep(Cur, N))
				{
					continue;
				}
				Dist.Add(N, CD + 1);
				Order.Add(N);
			}
		}
	};
	RunBfs(BaseMaxCells);
	if (Order.Num() < 24)
	{
		return false;
	}
	// Cellule atteinte dans le plafond de base (zone ordinaire des mecanismes)
	auto InBaseZone = [&](const FIntPoint& C)
	{
		const int32* DC = Dist.Find(C);
		return DC && *DC <= BaseMaxCells;
	};

	// 2. Emplacements d'une cellule : faces de mur (decalees le long du mur), ou poteau au centre (exterieur)
	auto Faces = [&](const FIntPoint& C, TArray<FIntPoint>& OutDirs)
	{
		OutDirs.Reset();
		for (const FIntPoint& Dir : GMissionDirs)
		{
			const FIntPoint Next = C + Dir;
			bool bFace = IsSolid(Next.X, Next.Y);
			if (!bFace)
			{
				if (Dir.X == 1) bFace = EdgeE(C.X, C.Y) == EBREdge::Wall;
				else if (Dir.X == -1) bFace = EdgeE(Next.X, Next.Y) == EBREdge::Wall;
				else if (Dir.Y == 1) bFace = EdgeN(C.X, C.Y) == EBREdge::Wall;
				else bFace = EdgeN(Next.X, Next.Y) == EBREdge::Wall;
			}
			if (bFace)
			{
				OutDirs.Add(Dir);
			}
		}
	};
	auto FaceSpot = [&](const FIntPoint& C, const FIntPoint& Dir, float Lateral)
	{
		FBRMissionSpot Spot;
		const FIntPoint Next = C + Dir;
		const float Inset = IsSolid(Next.X, Next.Y) ? 0.f : D.WallThickness * 0.5f;
		const FVector Side(-Dir.Y, Dir.X, 0.f);
		Spot.Pos = CellCenter(C, 0.f) + FVector(Dir.X, Dir.Y, 0.f) * (S * 0.5f - Inset) + Side * Lateral;
		Spot.Pos.Z = FloorZAt(Spot.Pos - FVector(Dir.X, Dir.Y, 0.f) * 40.f);
		Spot.Yaw = YawOf(-Dir.X, -Dir.Y);
		Spot.Cell = C;
		Spot.bWall = true;
		Spot.bValid = true;
		return Spot;
	};
	auto PostSpot = [&](const FIntPoint& C, float Lateral, uint32 Salt)
	{
		FBRMissionSpot Spot;
		const float Yaw = 90.f * static_cast<float>(BRM::Hash(Plan.Seed ^ static_cast<uint32>(C.X * 73856093) ^ static_cast<uint32>(C.Y * 19349663), Salt) % 4u);
		const FVector Fwd = FRotator(0.f, Yaw, 0.f).Vector();
		const FVector Side(-Fwd.Y, Fwd.X, 0.f);
		Spot.Pos = CellCenter(C, 0.f) + Side * Lateral - Fwd * (S * 0.2f);
		Spot.Pos.Z = FloorZAt(Spot.Pos);
		Spot.Yaw = Yaw;
		Spot.Cell = C;
		Spot.bWall = false;
		Spot.bValid = true;
		return Spot;
	};
	const float Spacing = 58.f;
	const int32 PerFace = FMath::Clamp(FMath::FloorToInt((S - 110.f) / Spacing) + 1, 1, 7);
	auto Lateral = [&](int32 K)
	{
		// 0, +1, -1, +2, -2...
		const int32 Step = (K + 1) / 2;
		return (K % 2 == 1 ? 1.f : -1.f) * Step * Spacing;
	};

	// Candidats par bande, dans un ordre tire (le meme chez tous)
	auto CellHash = [&](const FIntPoint& C, uint32 Salt)
	{
		return BRM::Hash(Plan.Seed ^ static_cast<uint32>(C.X * 73856093) ^ static_cast<uint32>(C.Y * 19349663), Salt);
	};
	TArray<FIntPoint> Used;
	auto FarFromUsed = [&](const FIntPoint& C, int32 MinGap)
	{
		for (const FIntPoint& U : Used)
		{
			if (FMath::Max(FMath::Abs(U.X - C.X), FMath::Abs(U.Y - C.Y)) < MinGap)
			{
				return false;
			}
		}
		return true;
	};
	auto PickCell = [&](int32 Zone, uint32 Salt, bool bNeedFaces, int32 MinFaces, FIntPoint& Out)
	{
		float Lo = 0.f, Hi = 0.f;
		ZoneBand(Zone, Lo, Hi);
		TArray<TPair<uint32, FIntPoint>> Cands;
		TArray<FIntPoint> Dirs;
		for (int32 Pass = 0; Pass < 3 && Cands.Num() == 0; ++Pass)
		{
			// Bande elargie si elle est vide (zone etroite), puis tout ce qui est atteint
			const float L = Pass == 0 ? Lo : (Pass == 1 ? Lo * 0.5f : 3.f);
			const float H = Pass == 0 ? Hi : (Pass == 1 ? Hi * 1.6f : 1e9f);
			for (const FIntPoint& C : Order)
			{
				const float M = Dist[C] * S / 100.f;
				// v4.15 : hors de la salle de l'aile, rien au-dela du plafond de base (parcours etendu du Level 6)
				if (Dist[C] > BaseMaxCells || M < L || M > H || IsSpawnArea(C.X, C.Y) || OutCells.Contains(C) || !FarFromUsed(C, Zone == 4 ? 3 : 2))
				{
					continue;
				}
				if (bNeedFaces)
				{
					Faces(C, Dirs);
					if (Dirs.Num() < MinFaces)
					{
						continue;
					}
				}
				Cands.Add(TPair<uint32, FIntPoint>(CellHash(C, Salt), C));
			}
		}
		if (Cands.Num() == 0)
		{
			return false;
		}
		Cands.Sort([](const TPair<uint32, FIntPoint>& A, const TPair<uint32, FIntPoint>& B) { return A.Key < B.Key; });
		Out = Cands[0].Value;
		return true;
	};
	// Les niveaux exterieurs (rues, champs) ont peu de murs : poteaux quand une cellule n'a pas de face
	const bool bOutdoor = D.bOutdoor;

	// Places libres d'une piece (cellule centrale et voisines) : faces de mur, sinon poteaux. v4.15 : voisines dans le
	// plafond de base, sauf pour la salle de l'aile du Level 6 (bBeyondCap : au-dela, par le parcours etendu)
	auto RoomSlots = [&](const FIntPoint& Room, TArray<FBRMissionSpot>& Slots, const FIntPoint* SkipFace, bool bBeyondCap)
	{
		Slots.Reset();
		TArray<FIntPoint> RoomCells;
		RoomCells.Add(Room);
		for (const FIntPoint& Dir : GMissionDirs)
		{
			const FIntPoint N = Room + Dir;
			if (Dist.Contains(N) && (bBeyondCap || InBaseZone(N)) && CanStep(Room, N) && !IsSpawnArea(N.X, N.Y))
			{
				RoomCells.Add(N);
			}
		}
		TArray<FIntPoint> Dirs;
		for (const FIntPoint& C : RoomCells)
		{
			Faces(C, Dirs);
			for (const FIntPoint& Dir : Dirs)
			{
				if (SkipFace && C == Room && Dir == *SkipFace)
				{
					continue;
				}
				for (int32 K = 0; K < PerFace; ++K)
				{
					Slots.Add(FaceSpot(C, Dir, Lateral(K)));
				}
			}
		}
		if (Slots.Num() < 12)
		{
			for (const FIntPoint& C : RoomCells)
			{
				for (int32 K = 0; K < 3; ++K)
				{
					Slots.Add(PostSpot(C, Lateral(K) * 1.2f, 77u + K));
				}
			}
		}
		return RoomCells;
	};

	OutSpots.SetNum(Plan.NumDevices);

	// 3. Salle de la mission et sortie garantie (niveaux infinis : on ne marche pas sans fin en attendant une sortie)
	FIntPoint Room(0, 0);
	// v4.13 : Level 6 : la salle de la mission est la galerie devant l'entree de l'aile LiDAR (portes du passage dans
	// ses quatre ouvertures, echelle au fond d'une alcove)
	const FIntPoint WingOut(LidarWing.OutX, LidarWing.OutY);
	bool bWingRoom = false;
	if (Plan.Level == 6 && IsLidarLevel() && LidarWing.bValid)
	{
		auto FindGallery = [&]()
		{
			for (const int32 V : { 1, 2, 0, 3 })
			{
				const FIntPoint G = LidarCell(0, V) + WingOut;
				if (Dist.Contains(G))
				{
					Room = G;
					return true;
				}
			}
			return false;
		};
		bWingRoom = FindGallery();
		if (!bWingRoom)
		{
			// v4.15 : galerie au-dela du plafond de 95 m (pres d'un chargement sur deux : le couloir garanti mesure de 18 a 41
			// cellules) : nouveau parcours, plafond releve a la longueur du couloir (diamant de rayon 43 au plus, sous la
			// limite de 6000 cellules), pour la seule salle de l'aile
			const int32 WingCap = FMath::Max(BaseMaxCells, BRLidar::CorridorLength(LidarWing) + FMath::Abs(Start.X) + FMath::Abs(Start.Y) + 2);
			if (WingCap > BaseMaxCells)
			{
				RunBfs(WingCap);
				bWingRoom = FindGallery();
				if (!bWingRoom)
				{
					RunBfs(BaseMaxCells);
				}
			}
		}
		if (!bWingRoom)
		{
			// Ne devrait jamais arriver (le couloir garanti y mene toujours) : placement ordinaire
			UE_LOG(LogBackrooms, Error, TEXT("[Lidar] Entree de l'aile non atteinte depuis le depart, meme par le couloir garanti (graine %u) : mecanismes du passage places dans une salle ordinaire"), Plan.Seed);
		}
	}
	if (!bWingRoom && !PickCell(4, 0x5A11u, !bOutdoor, bOutdoor ? 0 : 2, Room) && !PickCell(4, 0x5A12u, false, 0, Room))
	{
		return false;
	}
	// v4.12 : passerelle du Niveau 8 : l'echelle gardee s'adosse a une face dont la cellule d'en face est libre (le palier,
	// l'interruption, l'appui et ses marches s'y etendent sur BRMech::Bridge::TotalLength)
	const bool bBridgeModule = Plan.Level == 8 && !bBounded;
	FIntPoint BridgeDir(0, 0);
	bool bBridgeFace = false;
	auto ModuleFace = [&](const FIntPoint& C, FIntPoint& OutDir)
	{
		TArray<FIntPoint> Ds;
		Faces(C, Ds);
		const int32 N = Ds.Num();
		for (int32 K = 0; K < N; ++K)
		{
			const FIntPoint Dir = Ds[(CellHash(C, 0xB41Du) + static_cast<uint32>(K)) % static_cast<uint32>(N)];
			const FIntPoint Front = C - Dir;
			if (InBaseZone(Front) && CanStep(C, Front) && !IsSpawnArea(Front.X, Front.Y) && !OutCells.Contains(Front))
			{
				OutDir = Dir;
				return true;
			}
		}
		return false;
	};
	if (bBridgeModule)
	{
		bBridgeFace = ModuleFace(Room, BridgeDir);
		for (uint32 Salt = 1; Salt < 16 && !bBridgeFace; ++Salt)
		{
			FIntPoint Other(0, 0);
			if (PickCell(4, 0x5A20u + Salt, true, 2, Other) && ModuleFace(Other, BridgeDir))
			{
				Room = Other;
				bBridgeFace = true;
			}
		}
	}
	Used.Add(Room);
	OutCells.Add(Room);
	FIntPoint ExitFace(0, 0);
	bool bHasExitFace = false;
	int32 GateDevice = -1;
	for (int32 I = 0; I < Plan.NumDevices; ++I)
	{
		if (Plan.Devices[I].Kind == BRM::EKind::Gate && Plan.Devices[I].Zone == 4 && Plan.Devices[I].Role != BRM::R_Passage)
		{
			GateDevice = I;
			break;
		}
	}
	if (bWingRoom)
	{
		// Echelle de sortie contre le mur du fond de l'alcove tiree ; l'emetteur de parasites (element visible) devant elle
		const FIntPoint Alcove = LidarCell(BRLidar::WingCells - 1, LidarWing.LadderSlot);
		const FBRMissionSpot F = FaceSpot(Alcove, FIntPoint(-WingOut.X, -WingOut.Y), 0.f);
		FMissionExitSpot Spot;
		Spot.Target = ForwardTarget(Plan.Level);
		Spot.Style = EBRExitStyle::Ladder;
		Spot.Pos = F.Pos;
		Spot.Yaw = F.Yaw;
		Spot.Shaft = 0.f;
		OutExits.Add(Spot);
		// Rien d'autre dans l'aile ni devant ses portes
		for (int32 U = 0; U < BRLidar::WingCells; ++U)
		{
			for (int32 V = 0; V < BRLidar::WingCells; ++V)
			{
				OutCells.Add(LidarCell(U, V));
			}
		}
		for (int32 V = 0; V < BRLidar::WingCells; ++V)
		{
			OutCells.Add(LidarCell(0, V) + WingOut);
		}
	}
	else if (!bBounded)
	{
		TArray<int32> Targets;
		for (const FBRExitDef& Ex : D.Exits)
		{
			if (BRM::IsExitGuarded(Plan.Level, Ex.Target))
			{
				Targets.AddUnique(Ex.Target);
			}
		}
		if (Plan.Level == 11)
		{
			Targets.Insert(BRM::EndingTarget, 0);
		}
		// La sortie que garde la porte de la mission en premier
		const int32 Fwd = ForwardTarget(Plan.Level);
		Targets.Sort([Fwd](int32 A, int32 B) { return (A == Fwd) > (B == Fwd); });
		TArray<FIntPoint> Dirs;
		for (int32 T = 0; T < Targets.Num(); ++T)
		{
			EBRExitStyle Style = EBRExitStyle::Door;
			for (const FBRExitDef& Ex : D.Exits)
			{
				if (Ex.Target == Targets[T])
				{
					Style = Ex.Style;
				}
			}
			if (Targets[T] == BRM::EndingTarget || Style == EBRExitStyle::HouseDoor || Style == EBRExitStyle::BuildingDoor)
			{
				Style = EBRExitStyle::Door; // porte complete (la facade d'origine n'existe pas a cet endroit)
			}
			// Premiere sortie dans la salle, la seconde dans une cellule voisine
			FIntPoint Cell = Room;
			if (T > 0)
			{
				bool bFound = false;
				for (const FIntPoint& Dir : GMissionDirs)
				{
					const FIntPoint N = Room + Dir;
					if (InBaseZone(N) && CanStep(Room, N) && !OutCells.Contains(N) && !IsSpawnArea(N.X, N.Y))
					{
						Cell = N;
						bFound = true;
						break;
					}
				}
				if (!bFound)
				{
					continue;
				}
			}
			FMissionExitSpot Spot;
			Spot.Target = Targets[T];
			Spot.Style = Style;
			const bool bFloorStyle = Style == EBRExitStyle::NoclipFloor || Style == EBRExitStyle::Barn;
			Faces(Cell, Dirs);
			if (!bFloorStyle && Dirs.Num() > 0)
			{
				FIntPoint Dir = Dirs[CellHash(Cell, 0xE517u + T) % static_cast<uint32>(Dirs.Num())];
				if (T == 0 && bBridgeFace && Style == EBRExitStyle::Ladder)
				{
					Dir = BridgeDir;
				}
				const FBRMissionSpot F = FaceSpot(Cell, Dir, 0.f);
				Spot.Pos = F.Pos;
				Spot.Yaw = F.Yaw;
				if (T == 0)
				{
					ExitFace = Dir;
					bHasExitFace = true;
				}
			}
			else
			{
				// Sortie au sol ou batiment entier : au centre de la cellule, porte vers le chemin parcouru
				Spot.Pos = CellCenter(Cell, 0.f);
				Spot.Pos.Z = FloorZAt(Spot.Pos);
				Spot.Yaw = 90.f * static_cast<float>(CellHash(Cell, 0xE518u) % 4u);
				if (Style == EBRExitStyle::Barn)
				{
					// La grange (10 x 8 m) recule d'une demi-longueur : son entree donne sur la cellule
					Spot.Pos -= FRotator(0.f, Spot.Yaw, 0.f).Vector() * 520.f;
				}
			}
			if (Style == EBRExitStyle::Ladder)
			{
				Spot.Shaft = 0.f;
			}
			OutExits.Add(Spot);
			OutCells.Add(Cell);
			// Rien ne s'installe devant la sortie, ni sur l'emprise d'une grange
			const int32 Margin = Style == EBRExitStyle::Barn ? FMath::CeilToInt(700.f / S) : 0;
			const FIntPoint Base = WorldToCell(Spot.Pos);
			for (int32 X = -Margin; X <= Margin; ++X)
			{
				for (int32 Y = -Margin; Y <= Margin; ++Y)
				{
					OutCells.Add(Base + FIntPoint(X, Y));
				}
			}
		}
	}

	// Porte de la mission devant la premiere sortie garantie
	if (GateDevice >= 0)
	{
		if (OutExits.Num() > 0)
		{
			FMissionExitSpot& E = OutExits[0];
			FBRMissionSpot G;
			const FVector Fwd = FRotator(0.f, E.Yaw, 0.f).Vector();
			float Ahead = E.Style == EBRExitStyle::Barn ? 525.f : (E.Style == EBRExitStyle::NoclipFloor ? 140.f : 48.f);
			// v4.12 : modules physiques devant l'echelle gardee : sas du passage sec (Poolrooms), passerelle (Niveau 8)
			const bool bLadderFace = E.Style == EBRExitStyle::Ladder && bHasExitFace;
			if (bLadderFace && Plan.Devices[GateDevice].Role == BRM::R_DryPassage)
			{
				Ahead = BRMech::Pool::ChannelLength;
				G.Module = BRMech::Module::PoolLock;
			}
			else if (bLadderFace && bBridgeFace && Plan.Devices[GateDevice].Role == BRM::R_Bridge)
			{
				Ahead = BRMech::Bridge::FarDepth;
				G.Module = BRMech::Module::Bridge;
			}
			G.Pos = E.Pos + Fwd * Ahead;
			// Le sas mesure son sol juste devant son entree (marche) ; les autres a leur pied
			G.Pos.Z = FloorZAt(G.Module == BRMech::Module::PoolLock ? G.Pos + Fwd * 30.f : G.Pos);
			G.Yaw = E.Yaw;
			G.Cell = WorldToCell(G.Pos);
			G.bWall = E.Style != EBRExitStyle::NoclipFloor && E.Style != EBRExitStyle::Barn;
			G.bValid = true;
			OutSpots[GateDevice] = G;
			if (G.Module == BRMech::Module::PoolLock)
			{
				// L'echelle part du dallage du sas (au-dessus de l'eau des canaux)
				E.Pos.Z = BRMech::Pool::SlabTop(D.WaterHeight, D.DeckHeight);
			}
			else if (G.Module == BRMech::Module::Bridge)
			{
				// L'echelle part du palier ; la cellule d'en face porte l'appui et ses marches
				E.Pos.Z = G.Pos.Z + BRMech::Bridge::FarTop;
				OutCells.Add(Room - BridgeDir);
			}
		}
		else
		{
			GateDevice = -1; // niveau fini (Niveau 0) : l'element visible prend une place de la salle
		}
	}

	// v4.13 : portes du passage du Level 6 dans les quatre ouvertures d'entree de l'aile (une par cellule, de V = 0 a 3),
	// tournees vers la galerie (module : battant, cadre et plaque lisible au scanner)
	TArray<FVector> WingDoors;
	if (bWingRoom)
	{
		int32 V = 0;
		for (int32 I = 0; I < Plan.NumDevices && V < BRLidar::WingCells; ++I)
		{
			if (Plan.Devices[I].Role != BRM::R_LidarDoor)
			{
				continue;
			}
			const FIntPoint Inside = LidarCell(0, V);
			FBRMissionSpot Spot;
			Spot.Pos = CellCenter(Inside, 0.f) + FVector(WingOut.X, WingOut.Y, 0.f) * (S * 0.5f);
			Spot.Pos.Z = FloorZAt(Spot.Pos + FVector(WingOut.X, WingOut.Y, 0.f) * 60.f);
			Spot.Yaw = YawOf(WingOut.X, WingOut.Y);
			Spot.Cell = Inside + WingOut;
			Spot.bWall = false;
			Spot.Module = BRMech::Module::LidarDoor;
			Spot.bValid = true;
			OutSpots[I] = Spot;
			WingDoors.Add(Spot.Pos);
			++V;
		}
	}

	// 4. Groupes (meme piece) puis mecanismes seuls, chacun dans sa bande de distance
	TMap<int32, TArray<int32>> Groups;
	for (int32 I = 0; I < Plan.NumDevices; ++I)
	{
		if (I == GateDevice || OutSpots[I].bValid)
		{
			continue;
		}
		const BRM::FDevice& Dev = Plan.Devices[I];
		if (Dev.Group > 0 || Dev.Zone == 4)
		{
			Groups.FindOrAdd(Dev.Zone == 4 ? 1000 + Dev.Group : Dev.Group).Add(I);
		}
	}
	TArray<int32> GroupKeys;
	Groups.GetKeys(GroupKeys);
	GroupKeys.Sort();
	TArray<FBRMissionSpot> Slots;
	// v4.15 : places prises dans chaque piece (deux groupes de la meme salle ne prennent plus la meme place) et
	// mecanismes deja poses dans chaque piece : aucun point d'interaction dans la cage d'un voisin (BRMech::Clearance)
	TMap<FIntPoint, TArray<bool>> TakenByRoom;
	TMap<FIntPoint, TArray<int32>> PlacedByRoom;
	const float FaceHalf = S * 0.5f - D.WallThickness * 0.5f;
	auto FrameOf = [](const FBRMissionSpot& Sp)
	{
		BRMech::FFrame F;
		F.X = static_cast<float>(Sp.Pos.X);
		F.Y = static_cast<float>(Sp.Pos.Y);
		F.Z = static_cast<float>(Sp.Pos.Z);
		F.Yaw = Sp.Yaw;
		return F;
	};
	auto LateralOf = [this](const FBRMissionSpot& Sp)
	{
		const FVector Fwd = FRotator(0.f, Sp.Yaw, 0.f).Vector();
		const FVector Right(-Fwd.Y, Fwd.X, 0.f);
		return static_cast<float>(FVector::DotProduct(Sp.Pos - CellCenter(Sp.Cell, 0.f), Right));
	};
	auto NearWingDoor = [&WingDoors](const FVector& P)
	{
		for (const FVector& Dw : WingDoors)
		{
			if (FVector::Dist2D(P, Dw) < 150.f)
			{
				return true;
			}
		}
		return false;
	};
	for (const int32 Key : GroupKeys)
	{
		const TArray<int32>& Members = Groups[Key];
		FIntPoint GroupRoom = Room;
		if (Key < 1000)
		{
			if (!PickCell(Plan.Devices[Members[0]].Zone, 0x6000u + Key, !bOutdoor, bOutdoor ? 0 : 1, GroupRoom)
				&& !PickCell(Plan.Devices[Members[0]].Zone, 0x6100u + Key, false, 0, GroupRoom))
			{
				return false;
			}
			Used.Add(GroupRoom);
		}
		const TArray<FIntPoint> RoomCells = RoomSlots(GroupRoom, Slots, (GroupRoom == Room && bHasExitFace) ? &ExitFace : nullptr, bWingRoom && GroupRoom == Room);
		TArray<bool>& Taken = TakenByRoom.FindOrAdd(GroupRoom);
		Taken.SetNumZeroed(Slots.Num());
		TArray<int32>& InRoom = PlacedByRoom.FindOrAdd(GroupRoom);
		// v4.15 : les mecanismes qui occupent de la place (cage du Level 6) d'abord, les autres autour d'eux, sans entrer
		// dans leur volume (le poste d'une cage n'est plus dans sa cage) ; sans volume : premiere place libre, comme avant
		auto HasVolume = [&Plan, &D](int32 I)
		{
			return BRMech::Clearance::Of(Plan.Devices[I].Kind, Plan.Devices[I].Role, BRMech::Module::None, D.WallThickness).bSolid;
		};
		TArray<int32> Ordered = Members;
		Ordered.StableSort([&HasVolume](int32 A, int32 B) { return HasVolume(A) && !HasVolume(B); });
		TArray<BRMech::Clearance::FSlot> Cand;
		Cand.SetNum(Slots.Num());
		for (int32 J = 0; J < Slots.Num(); ++J)
		{
			Cand[J].Frame = FrameOf(Slots[J]);
			Cand[J].Lateral = LateralOf(Slots[J]);
			Cand[J].bWall = Slots[J].bWall;
		}
		for (const int32 Dev : Ordered)
		{
			// Les places voisines de la sortie restent libres (la porte s'ouvre devant), ainsi que celles devant les portes
			// de l'aile
			for (int32 J = 0; J < Slots.Num(); ++J)
			{
				Cand[J].bOpen = !Taken[J] && !(GroupRoom == Room && OutExits.Num() > 0
					&& (FVector::Dist2D(Slots[J].Pos, OutExits[0].Pos) < 140.f || NearWingDoor(Slots[J].Pos)));
			}
			TArray<BRMech::Clearance::FPlaced> Near;
			for (const int32 Other : InRoom)
			{
				BRMech::Clearance::FPlaced& P = Near.AddDefaulted_GetRef();
				P.Frame = FrameOf(OutSpots[Other]);
				P.Print = BRMech::Clearance::Of(Plan.Devices[Other].Kind, Plan.Devices[Other].Role, OutSpots[Other].Module, D.WallThickness);
			}
			const BRMech::Clearance::FFootprint Print = BRMech::Clearance::Of(Plan.Devices[Dev].Kind, Plan.Devices[Dev].Role, BRMech::Module::None, D.WallThickness);
			const int32 J = BRMech::Clearance::PickSlot(Print, Cand.GetData(), Cand.Num(), Near.GetData(), Near.Num(), FaceHalf);
			if (J < 0)
			{
				return false;
			}
			Taken[J] = true;
			OutSpots[Dev] = Slots[J];
			InRoom.Add(Dev);
		}
		for (const FIntPoint& C : RoomCells)
		{
			OutCells.Add(C);
		}
	}
	TArray<FIntPoint> Dirs;
	for (int32 I = 0; I < Plan.NumDevices; ++I)
	{
		if (OutSpots[I].bValid)
		{
			continue;
		}
		const BRM::FDevice& Dev = Plan.Devices[I];
		FIntPoint C(0, 0);
		if (!PickCell(Dev.Zone, 0x7000u + I, !bOutdoor, bOutdoor ? 0 : 1, C) && !PickCell(Dev.Zone, 0x7100u + I, false, 0, C))
		{
			return false;
		}
		Used.Add(C);
		OutCells.Add(C);
		Faces(C, Dirs);
		// Neon du Niveau 0, caisses et poteaux : sur une face si possible, sinon au centre
		if (Dirs.Num() > 0)
		{
			OutSpots[I] = FaceSpot(C, Dirs[CellHash(C, 0x7200u + I) % static_cast<uint32>(Dirs.Num())], 0.f);
		}
		else
		{
			OutSpots[I] = PostSpot(C, 0.f, 0x7300u + I);
		}
	}
	// v4.12 : Poolrooms : les deux bassins sont adosses au mur de la vanne A ; la vanne B est fixee a cote d'elle, sur
	// l'avant du bassin B (meme face). Sans place sur cette face, les vannes restent murales (sans bassins visibles)
	if (Plan.Level == 37)
	{
		int32 SA = -1, SB = -1;
		for (int32 I = 0; I < Plan.NumDevices; ++I)
		{
			if (Plan.Devices[I].Role == BRM::R_Sluice)
			{
				(Plan.Devices[I].Label == 0 ? SA : SB) = I;
			}
		}
		if (SA >= 0 && SB >= 0 && OutSpots[SA].bValid && OutSpots[SA].bWall)
		{
			FBRMissionSpot& A = OutSpots[SA];
			const FVector Fwd = FRotator(0.f, A.Yaw, 0.f).Vector();
			const FVector Left(-Fwd.Y, Fwd.X, 0.f);
			const float Lat = static_cast<float>(FVector::DotProduct(A.Pos - CellCenter(A.Cell, 0.f), Left));
			const float Half = S * 0.5f - D.WallThickness * 0.5f;
			if (Lat + BRMech::Pool::AY0 - BRMech::Pool::Wall >= -Half && Lat + BRMech::Pool::BY1 + BRMech::Pool::Wall <= Half)
			{
				FBRMissionSpot B = A;
				B.Pos = A.Pos + Left * BRMech::Pool::WheelLateralB + Fwd * BRMech::Pool::TankDepth;
				A.Pos += Fwd * BRMech::Pool::TankDepth;
				A.Pos.Z = FloorZAt(A.Pos + Fwd * 40.f);
				B.Pos.Z = FloorZAt(B.Pos + Fwd * 40.f);
				A.Cell = WorldToCell(A.Pos);
				B.Cell = WorldToCell(B.Pos);
				A.Module = BRMech::Module::PoolTanks;
				B.Module = BRMech::Module::PoolWheelB;
				OutSpots[SB] = B;
			}
		}
	}
	for (const FBRMissionSpot& Spot : OutSpots)
	{
		if (!Spot.bValid)
		{
			return false;
		}
		OutCells.Add(Spot.Cell);
	}
	// v4.15 : invariant du placement : aucun point d'interaction dans le volume bloquant d'un autre mecanisme (journal)
	LogMissionVolumeInvariant(Plan, OutSpots, D.WallThickness, TEXT(""));
	return true;
}

bool ABRWorld::PlaceMissionModule(const BRM::FPlan& Plan, TArray<FBRMissionSpot>& OutSpots, TArray<FMissionExitSpot>& OutExits, TSet<FIntPoint>& OutCells) const
{
	// v4.12 : module de secours. Le placement normal (zones de distance, salles reservees) peut echouer sur un decor
	// tres ferme ; ici, tout se pose pres du depart, dans les premieres cellules atteintes a pied (parcours en largeur,
	// au sec, hors des salles de fosses), sur leurs murs puis sur des poteaux. Toujours possible : la mission n'est plus
	// abandonnee et les sorties restent gardees. Deterministe (memes cellules, memes emplacements chez tous)
	OutSpots.Reset();
	OutExits.Reset();
	OutCells.Reset();
	if (Plan.NumDevices <= 0)
	{
		return false;
	}
	const FBRLevelDef& D = Def();
	const float S = D.CellSize;
	auto Allowed = [&](const FIntPoint& C)
	{
		return IsCellInBounds(C.X, C.Y) && IsWalkable(C) && !IsPoolCell(C.X, C.Y) && !IsPitRoomCell(C.X, C.Y) && IsSafelyReachable(C);
	};
	TArray<FIntPoint> Order;
	TSet<FIntPoint> Seen;
	Order.Add(FIntPoint(0, 0));
	Seen.Add(FIntPoint(0, 0));
	for (int32 Head = 0; Head < Order.Num() && Order.Num() < 400; ++Head)
	{
		for (const FIntPoint& Dir : GMissionDirs)
		{
			const FIntPoint N = Order[Head] + Dir;
			if (!Seen.Contains(N) && Allowed(N) && CanStep(Order[Head], N))
			{
				Seen.Add(N);
				Order.Add(N);
			}
		}
	}
	auto Faces = [&](const FIntPoint& C, TArray<FIntPoint>& OutDirs)
	{
		OutDirs.Reset();
		for (const FIntPoint& Dir : GMissionDirs)
		{
			const FIntPoint Next = C + Dir;
			bool bFace = IsSolid(Next.X, Next.Y);
			if (!bFace)
			{
				if (Dir.X == 1) bFace = EdgeE(C.X, C.Y) == EBREdge::Wall;
				else if (Dir.X == -1) bFace = EdgeE(Next.X, Next.Y) == EBREdge::Wall;
				else if (Dir.Y == 1) bFace = EdgeN(C.X, C.Y) == EBREdge::Wall;
				else bFace = EdgeN(Next.X, Next.Y) == EBREdge::Wall;
			}
			if (bFace)
			{
				OutDirs.Add(Dir);
			}
		}
	};
	auto FaceSpot = [&](const FIntPoint& C, const FIntPoint& Dir, float Lateral)
	{
		FBRMissionSpot Spot;
		const FIntPoint Next = C + Dir;
		const float Inset = IsSolid(Next.X, Next.Y) ? 0.f : D.WallThickness * 0.5f;
		const FVector Side(-Dir.Y, Dir.X, 0.f);
		Spot.Pos = CellCenter(C, 0.f) + FVector(Dir.X, Dir.Y, 0.f) * (S * 0.5f - Inset) + Side * Lateral;
		Spot.Pos.Z = FloorZAt(Spot.Pos - FVector(Dir.X, Dir.Y, 0.f) * 40.f);
		Spot.Yaw = YawOf(-Dir.X, -Dir.Y);
		Spot.Cell = C;
		Spot.bWall = true;
		Spot.bValid = true;
		return Spot;
	};
	auto PostSpot = [&](const FIntPoint& C, float Lateral, uint32 Salt)
	{
		FBRMissionSpot Spot;
		const float Yaw = 90.f * static_cast<float>(BRM::Hash(Plan.Seed ^ static_cast<uint32>(C.X * 73856093) ^ static_cast<uint32>(C.Y * 19349663), Salt) % 4u);
		const FVector Fwd = FRotator(0.f, Yaw, 0.f).Vector();
		const FVector Side(-Fwd.Y, Fwd.X, 0.f);
		Spot.Pos = CellCenter(C, 0.f) + Side * Lateral - Fwd * (S * 0.2f);
		Spot.Pos.Z = FloorZAt(Spot.Pos);
		Spot.Yaw = Yaw;
		Spot.Cell = C;
		Spot.bWall = false;
		Spot.bValid = true;
		return Spot;
	};
	TArray<FIntPoint> Dirs;
	// Sorties gardees (niveaux infinis) : a la cellule atteinte la plus lointaine, la suivante juste avant
	int32 GateDevice = -1;
	for (int32 I = 0; I < Plan.NumDevices; ++I)
	{
		if (Plan.Devices[I].Kind == BRM::EKind::Gate && Plan.Devices[I].Zone == 4 && Plan.Devices[I].Role != BRM::R_Passage)
		{
			GateDevice = I;
			break;
		}
	}
	if (D.BoundsChunks <= 0)
	{
		TArray<int32> Targets;
		for (const FBRExitDef& Ex : D.Exits)
		{
			if (BRM::IsExitGuarded(Plan.Level, Ex.Target))
			{
				Targets.AddUnique(Ex.Target);
			}
		}
		if (Plan.Level == 11)
		{
			Targets.Insert(BRM::EndingTarget, 0);
		}
		const int32 Fwd = ForwardTarget(Plan.Level);
		Targets.Sort([Fwd](int32 A, int32 B) { return (A == Fwd) > (B == Fwd); });
		for (int32 T = 0; T < Targets.Num(); ++T)
		{
			const FIntPoint Cell = Order[FMath::Max(0, Order.Num() - 1 - T)];
			EBRExitStyle Style = EBRExitStyle::Door;
			for (const FBRExitDef& Ex : D.Exits)
			{
				if (Ex.Target == Targets[T])
				{
					Style = Ex.Style;
				}
			}
			if (Targets[T] == BRM::EndingTarget || Style == EBRExitStyle::HouseDoor || Style == EBRExitStyle::BuildingDoor || Style == EBRExitStyle::Barn)
			{
				Style = EBRExitStyle::Door; // une porte complete tient dans n'importe quelle cellule
			}
			FMissionExitSpot Spot;
			Spot.Target = Targets[T];
			Spot.Style = Style;
			Faces(Cell, Dirs);
			if (Style != EBRExitStyle::NoclipFloor && Dirs.Num() > 0)
			{
				const FBRMissionSpot F = FaceSpot(Cell, Dirs[0], 0.f);
				Spot.Pos = F.Pos;
				Spot.Yaw = F.Yaw;
			}
			else
			{
				Spot.Pos = CellCenter(Cell, 0.f);
				Spot.Pos.Z = FloorZAt(Spot.Pos);
			}
			Spot.Shaft = 0.f;
			OutExits.Add(Spot);
			OutCells.Add(Cell);
		}
	}
	// Emplacements des mecanismes : murs des cellules les plus proches (trois par face, 70 cm d'ecart), puis poteaux
	TArray<FBRMissionSpot> Slots;
	for (const FIntPoint& C : Order)
	{
		if (OutCells.Contains(C))
		{
			continue;
		}
		Faces(C, Dirs);
		for (const FIntPoint& Dir : Dirs)
		{
			for (int32 K = 0; K < 3; ++K)
			{
				Slots.Add(FaceSpot(C, Dir, (K - 1) * 70.f));
			}
		}
		if (Slots.Num() >= Plan.NumDevices * 2 + 6)
		{
			break;
		}
	}
	for (int32 K = 0; Slots.Num() < Plan.NumDevices + 4; ++K)
	{
		const FIntPoint C = Order[K % Order.Num()];
		Slots.Add(PostSpot(C, static_cast<float>((K / Order.Num()) % 5 - 2) * 70.f, 0x9100u + static_cast<uint32>(K)));
	}
	OutSpots.SetNum(Plan.NumDevices);
	int32 Next = 0;
	// v4.15 : meme regle que le placement normal (BRMech::Clearance) des qu'un mecanisme occupe de la place (cage du
	// Level 6) : aucun point d'interaction dans son volume. Sans volume, l'ordre d'avant (curseur) est garde
	TArray<bool> Taken;
	Taken.SetNumZeroed(Slots.Num());
	TArray<BRMech::Clearance::FSlot> Cand;
	Cand.SetNum(Slots.Num());
	for (int32 J = 0; J < Slots.Num(); ++J)
	{
		const FVector Fwd = FRotator(0.f, Slots[J].Yaw, 0.f).Vector();
		Cand[J].Frame.X = static_cast<float>(Slots[J].Pos.X);
		Cand[J].Frame.Y = static_cast<float>(Slots[J].Pos.Y);
		Cand[J].Frame.Z = static_cast<float>(Slots[J].Pos.Z);
		Cand[J].Frame.Yaw = Slots[J].Yaw;
		Cand[J].Lateral = static_cast<float>(FVector::DotProduct(Slots[J].Pos - CellCenter(Slots[J].Cell, 0.f), FVector(-Fwd.Y, Fwd.X, 0.f)));
		Cand[J].bWall = Slots[J].bWall;
	}
	TArray<BRMech::Clearance::FPlaced> Placed;
	bool bAnyVolume = false;
	for (int32 I = 0; I < Plan.NumDevices; ++I)
	{
		if (I == GateDevice && OutExits.Num() > 0)
		{
			// L'element visible de la mission devant la sortie qu'il garde
			const FMissionExitSpot& E = OutExits[0];
			FBRMissionSpot G;
			G.Pos = E.Pos + FRotator(0.f, E.Yaw, 0.f).Vector() * 48.f;
			G.Pos.Z = FloorZAt(G.Pos);
			G.Yaw = E.Yaw;
			G.Cell = WorldToCell(G.Pos);
			G.bWall = true;
			G.bValid = true;
			OutSpots[I] = G;
			continue;
		}
		const BRMech::Clearance::FFootprint Print = BRMech::Clearance::Of(Plan.Devices[I].Kind, Plan.Devices[I].Role, BRMech::Module::None, D.WallThickness);
		int32 Idx = -1;
		if (Print.bSolid || bAnyVolume)
		{
			for (int32 J = 0; J < Slots.Num(); ++J)
			{
				Cand[J].bOpen = !Taken[J] && !(OutExits.Num() > 0 && FVector::Dist2D(Slots[J].Pos, OutExits[0].Pos) < 140.f);
			}
			Idx = BRMech::Clearance::PickSlot(Print, Cand.GetData(), Cand.Num(), Placed.GetData(), Placed.Num(), S * 0.5f - D.WallThickness * 0.5f);
		}
		if (Idx < 0)
		{
			// La porte s'ouvre devant la sortie : rien a moins de 1,4 m. v4.15 (correctifs) : ni sur une place deja prise par
			// PickSlot (le curseur n'avance pas pour elle) ; sans volume, rien n'est pris devant le curseur : ordre d'avant
			while (Next < Slots.Num() - 1 && (Taken[Next] || (OutExits.Num() > 0 && FVector::Dist2D(Slots[Next].Pos, OutExits[0].Pos) < 140.f)))
			{
				++Next;
			}
			Idx = FMath::Min(Next, Slots.Num() - 1);
			++Next;
		}
		Taken[Idx] = true;
		OutSpots[I] = Slots[Idx];
		OutCells.Add(OutSpots[I].Cell);
		BRMech::Clearance::FPlaced& P = Placed.AddDefaulted_GetRef();
		P.Frame = Cand[Idx].Frame;
		P.Print = Print;
		bAnyVolume |= Print.bSolid;
	}
	LogMissionVolumeInvariant(Plan, OutSpots, D.WallThickness, TEXT(" (module de secours)"));
	return true;
}

void ABRWorld::SpawnMissionActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 I = 0; I < MissionPlan.NumDevices && I < MissionSpots.Num(); ++I)
	{
		const FBRMissionSpot& Spot = MissionSpots[I];
		ABRMissionDevice* Dev = World->SpawnActor<ABRMissionDevice>(ABRMissionDevice::StaticClass(), FTransform(FRotator(0.f, Spot.Yaw, 0.f), Spot.Pos), Params);
		if (Dev)
		{
			Dev->Init(I, MissionPlan, Spot);
			Dev->SetShown(false);
			MissionDevices.Add(Dev);
		}
	}
	for (const FMissionExitSpot& Spot : MissionExitSpots)
	{
		ABRExit* Exit = World->SpawnActor<ABRExit>(ABRExit::StaticClass(), FTransform(FRotator(0.f, Spot.Yaw, 0.f), Spot.Pos), Params);
		if (Exit)
		{
			Exit->Init(Spot.Target, Spot.Style);
			if (Spot.Style == EBRExitStyle::Ladder)
			{
				// v4.12 : hauteur jusqu'au plafond depuis le pied de l'echelle (trottoir, dallage du sas, palier) : avant, une
				// echelle posee a 68 cm visait un sommet au-dessus du plafond et la montee s'y bloquait
				Exit->InitLadder(Def().WallHeight - static_cast<float>(Spot.Pos.Z), Spot.Shaft);
			}
			Exit->SetActorHiddenInGame(true);
			Exit->SetActorEnableCollision(false);
			MissionExits.Add(Exit);
		}
	}
	MissionDeviceTimer = 0.f;
}

void ABRWorld::UpdateMissionDevices(float Dt)
{
	MissionDeviceTimer -= Dt;
	if (MissionDeviceTimer > 0.f)
	{
		return;
	}
	MissionDeviceTimer = 0.5f;
	TArray<FVector> Near;
	if (const ABRCharacter* P = GetPlayer())
	{
		Near.Add(P->GetActorLocation());
	}
	for (ABRMissionDevice* D : MissionDevices)
	{
		if (!IsValid(D))
		{
			continue;
		}
		// Visible quand le sol et les murs de sa cellule existent (un chunk decharge ne remet rien a zero : l'etat est
		// tenu par le monde)
		D->SetShown(HasChunkFloor(CellToChunk(D->GetCell())));
		if (D->WantsTick() && D->IsShown())
		{
			bool bClose = false;
			for (const FVector& L : Near)
			{
				bClose |= FVector::DistSquared(L, D->GetActorLocation()) < FMath::Square(3500.f);
			}
			if (bClose != D->IsActorTickEnabled())
			{
				D->SetActorTickEnabled(bClose);
			}
		}
	}
	for (ABRExit* E : MissionExits)
	{
		if (IsValid(E))
		{
			const bool bShow = HasChunkFloor(CellToChunk(WorldToCell(E->GetActorLocation())));
			if (E->IsHidden() == bShow)
			{
				E->SetActorHiddenInGame(!bShow);
				E->SetActorEnableCollision(bShow);
				// v4.12 : une sortie masquee (zone dechargee) ne s'anime plus (lueur du conduit d'une echelle)
				E->SetActorTickEnabled(bShow && E->IsClimbable());
				if (bShow)
				{
					E->ScheduleDepartureViewCheck(); // v4.15 (developpement) : point vise par le depart, decor construit
				}
			}
		}
	}
}

// =====================================================================================================================
// Actions (serveur)
// =====================================================================================================================

bool ABRWorld::CanReachMissionDevice(const FVector& Eye, const ABRMissionDevice* Dev, const AActor* By, FHitResult* OutBlock) const
{
	UWorld* World = GetWorld();
	if (!World || !Dev || !IsMissionActive())
	{
		return false;
	}
	const int32 Device = Dev->GetIndex();
	if (Device < 0 || Device >= MissionPlan.NumDevices)
	{
		return false;
	}
	const FVector Target = Dev->GetInteractPoint();
	// v4.13 : la mire et les cages du Level 6 se relevent au scan focalise, de plus loin (toujours en ligne de vue)
	const BRM::EKind KindAt = MissionPlan.Devices[Device].Kind;
	const uint8 RoleAt = MissionPlan.Devices[Device].Role;
	const bool bScanned = KindAt == BRM::EKind::Observe && (RoleAt == BRM::R_LidarTarget || RoleAt == BRM::R_LidarCage);
	const float Reach = bScanned ? MissionScanReachServer : MissionReachServer;
	if (FVector::DistSquared(Eye, Target) > FMath::Square(Reach))
	{
		return false;
	}
	// Ligne de vue : seuls le joueur et le mecanisme vise sont ignores (pas les portes fermees, cages, bassins voisins)
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BRMissionLos), false, By);
	Q.AddIgnoredActor(Dev);
	if (OutBlock)
	{
		return !World->LineTraceSingleByChannel(*OutBlock, Eye, Target, ECC_WorldStatic, Q);
	}
	return !World->LineTraceTestByChannel(Eye, Target, ECC_WorldStatic, Q);
}

bool ABRWorld::FindMissionDeviceObstruction(const ABRMissionDevice* Dev, const AActor* IgnoreAlso, FString& OutWhat) const
{
	UWorld* World = GetWorld();
	if (!World || !Dev)
	{
		return false;
	}
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BRMissionPoint), false, Dev);
	if (IgnoreAlso)
	{
		Q.AddIgnoredActor(IgnoreAlso);
	}
	TArray<FOverlapResult> Hits;
	// v4.15 (correctifs 3) : sonde de 1 cm (BRMech::Clearance::AimProbe). Avant : 5 cm, exactement tangente au mur d'un
	// mecanisme dont le point vise est a 5 cm de son origine murale (indices, cles, mire du Level 6) : Chaos compte ce contact,
	// et 14 mecanismes atteignables etaient notes "point d'interaction dans BRChunk_..." (le mur qui les porte)
	World->OverlapMultiByChannel(Hits, Dev->GetInteractPoint(), FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(BRMech::Clearance::AimProbe), Q);
	for (const FOverlapResult& H : Hits)
	{
		if (H.bBlockingHit)
		{
			// v4.15 (correctifs 3) : modele du composant touche et visibilite (lot de collision invisible ou decor rendu)
			const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(H.GetComponent());
			OutWhat = FString::Printf(TEXT("%s (%s%s)"), *GetNameSafe(H.GetActor()), *GetNameSafe(H.GetComponent()),
				Mesh ? *FString::Printf(TEXT(", %s, %s"), *GetNameSafe(Mesh->GetStaticMesh()), Mesh->IsVisible() ? TEXT("visible") : TEXT("invisible")) : TEXT(""));
			return true;
		}
	}
	return false;
}

bool ABRWorld::IsMissionKeepOut(const BRMech::Clearance::FRect& R) const
{
	for (const BRMech::Clearance::FRect& K : MissionKeepOuts)
	{
		if (BRMech::Clearance::Overlap(K, R))
		{
			return true;
		}
	}
	return false;
}

uint8 ABRWorld::ServerMissionAct(ABRCharacter* By, int32 Device, uint8 Action, int32 LevelSerial, uint8& OutRelated, uint8& OutCount)
{
	OutRelated = 255;
	OutCount = 0;
	if (!HasAuthority() || !IsMissionActive() || !By || Device < 0 || Device >= MissionPlan.NumDevices || Action > static_cast<uint8>(BRM::EAction::Hold))
	{
		return BRMissionText::NotNow;
	}
	// Demande d'un autre niveau, transition en cours, joueur a terre ou en chargement : refus sans effet
	if (LevelSerial != LoadedSerial || !bLevelReady || TransState != ETrans::None || By->IsDead() || By->IsLevelLoading())
	{
		return BRMissionText::NotNow;
	}
	ABRMissionDevice* Dev = FindMissionDevice(Device);
	if (!Dev)
	{
		return BRMissionText::NotNow;
	}
	// Distance 3D depuis les yeux et ligne de vue (pas a travers un mur, pas depuis un autre etage). v4.15 : la regle
	// partagee avec les tests (CanReachMissionDevice)
	if (!CanReachMissionDevice(By->GetEyeLocation(), Dev, By))
	{
		return BRMissionText::TooFar;
	}
	const double Now = FPlatformTime::Seconds();
	if (const double* Until = MissionCooldowns.Find(Device))
	{
		if (Now < *Until)
		{
			return BRMissionText::Cooldown;
		}
	}
	const BRM::FDevice& D = MissionPlan.Devices[Device];
	BRM::EAction ActKind = static_cast<BRM::EAction>(Action);
	int32 Value = 0;
	if (D.Kind == BRM::EKind::Observe || D.Kind == BRM::EKind::Crank)
	{
		// Duree verifiee ici : une unite par 0,4 s au plus, quel que soit le rythme des demandes du client
		ActKind = BRM::EAction::Hold;
		Value = 1;
		const uint64 Key = HoldKey(By, Device);
		const double* Last = MissionHoldTimes.Find(Key);
		if (Last && Now - *Last < 0.4)
		{
			return static_cast<uint8>(BRM::EFeedback::Progress);
		}
		MissionHoldTimes.Add(Key, Now);
	}
	else
	{
		ActKind = BRM::EAction::Use;
	}
	const BRM::FResult R = BRM::Act(MissionPlan, MissionState, GetMissionCampaign(), Device, ActKind, Value);
	OutRelated = R.Related;
	OutCount = R.Count;
	// Les mecanismes font du bruit : une balise qu'on remonte, un relais qui claque, une disjonction s'entendent de loin
	if (R.bChanged || R.Feedback == BRM::EFeedback::Wrong)
	{
		float Radius = 500.f;
		switch (D.Kind)
		{
		case BRM::EKind::Crank: Radius = (D.Role == BRM::R_Beacon || D.Role == BRM::R_LidarPost) ? 1800.f : 1000.f; break;
		case BRM::EKind::Switch: Radius = D.Role == BRM::R_Relay ? 1200.f : 600.f; break;
		case BRM::EKind::Button: Radius = D.Role == BRM::R_LidarDoor ? 1400.f : 800.f; break;
		default: break;
		}
		if (R.Feedback == BRM::EFeedback::Tripped)
		{
			Radius = 1600.f;
		}
		ReportNoise(Dev->GetInteractPoint(), Radius);
	}
	if (R.Feedback == BRM::EFeedback::Wrong && D.Kind == BRM::EKind::Button)
	{
		// Une erreur se comprend et se corrige, mais le mecanisme se rearme (pas d'essais en rafale) ; le moulin tourne
		// un tour a vide
		// v4.13 : une porte du passage du Level 6 refuse dans un crepitement de parasites et se rearme
		MissionCooldowns.Add(Device, Now + (D.Role == BRM::R_MillBrake ? 8.0 : (D.Role == BRM::R_LidarDoor ? static_cast<double>(BRMech::LidarDoor::Rearm) : 4.0)));
	}
	uint8 Fb = static_cast<uint8>(R.Feedback);
	if (D.Kind == BRM::EKind::Item && R.Feedback == BRM::EFeedback::AlreadyDone)
	{
		Fb = BRMissionText::Taken;
	}
	if (R.bChanged || R.Feedback == BRM::EFeedback::Wrong || R.Feedback == BRM::EFeedback::Dead || R.Feedback == BRM::EFeedback::Order)
	{
		PublishMission(Device, static_cast<uint8>(R.Feedback));
	}
	UE_LOG(LogBackrooms, Verbose, TEXT("Mission : %s -> mecanisme %d action %d : retour %d"), *GetNameSafe(By), Device, Action, Fb);
	return Fb;
}

void ABRWorld::PublishMission(int32 Device, uint8 Feedback, bool bAnimate)
{
	if (!HasAuthority())
	{
		return;
	}
	NetMission.Gen = MissionGen;
	NetMission.bFallback = MissionPlan.bFallback;
	NetMission.Serial = NetLevel.Serial;
	NetMission.Tier = MissionTier;
	NetMission.PlaceVersion = static_cast<uint8>(BRM::PlaceVersion);
	NetMission.Notice = MissionNotice;
	NetMission.State.Reset();
	if (IsMissionActive())
	{
		uint8 Buf[BRM::MaxBlob];
		const int32 N = BRM::Serialize(MissionPlan, MissionState, Buf, BRM::MaxBlob);
		NetMission.State.Append(Buf, N);
	}
	++NetMission.Rev;
	NetMission.LastDevice = static_cast<uint8>(Device < 0 || Device > 254 ? 255 : Device);
	NetMission.LastFeedback = Feedback;
	// Campagne : fragment de route et objectif facultatif
	if (IsMissionActive())
	{
		BRM::FEval E;
		GetMissionEval(E);
		if (E.bSolved)
		{
			NetCampaign.RouteBits |= BRM::RouteBitFor(GetLevelNumber());
		}
		if (E.bOptionalDone)
		{
			NetCampaign.OptionalFound |= BRM::OptionalBitFor(GetLevelNumber());
		}
	}
	ForceNetUpdate();
	OnMissionStateChanged(Device, Feedback, bAnimate);
}

void ABRWorld::OnRep_Mission()
{
	if (!bLevelReady || NetMission.Serial != LoadedSerial || NetMission.Serial == 0)
	{
		return; // etat d'un autre niveau, ou niveau pas encore construit (SetupMission le lira)
	}
	const bool bConfig = NetMission.Gen != MissionGen || (MissionGen >= 2 && (NetMission.bFallback != MissionPlan.bFallback || NetMission.Tier != MissionTier));
	if (bConfig)
	{
		// La version ou la variante annoncees par l'hote different de la supposition faite au chargement : on suit l'hote
		SetupMission();
		return;
	}
	if (NetMission.Notice != 0)
	{
		ShowMissionNotice(NetMission.Notice);
	}
	if (MissionGen < 2)
	{
		return;
	}
	BRM::FState Received;
	if (!BRM::Deserialize(MissionPlan, NetMission.State.GetData(), NetMission.State.Num(), Received))
	{
		UE_LOG(LogBackrooms, Warning, TEXT("Etat de mission recu illisible (revision %d)"), NetMission.Rev);
		return;
	}
	MissionState = Received;
	OnMissionStateChanged(NetMission.LastDevice == 255 ? -1 : NetMission.LastDevice, NetMission.LastFeedback, true);
}

void ABRWorld::OnMissionStateChanged(int32 Device, uint8 Feedback, bool bAnimate)
{
	if (!IsMissionActive())
	{
		return;
	}
	BRM::FEval E;
	GetMissionEval(E);
	for (ABRMissionDevice* D : MissionDevices)
	{
		if (IsValid(D))
		{
			D->ApplyState(MissionPlan, MissionState, E, bAnimate);
		}
	}
	if (ABRMissionDevice* D = FindMissionDevice(Device))
	{
		if (Feedback != 0)
		{
			D->PlayFeedback(Feedback);
			// Avertissement ou defaut : tout joueur proche le lit aussi (le son a son equivalent ecrit)
			const ABRCharacter* P = GetPlayer();
			const BRM::EFeedback F = static_cast<BRM::EFeedback>(Feedback);
			if (P && (F == BRM::EFeedback::Warning || F == BRM::EFeedback::Tripped) && FVector::Dist(P->GetActorLocation(), D->GetActorLocation()) < 2000.f)
			{
				ABRHUD::Notify(this, BRMissionText::Feedback(MissionPlan, Device, Feedback, 255, 0), 4.f, FLinearColor(1.f, 0.6f, 0.3f));
			}
		}
	}
	for (int32 G = 0; G < MissionPlan.NumDevices; ++G)
	{
		// Element visible qui vient de s'ouvrir : bruit de la porte ou de la passerelle
		if (MissionPlan.Devices[G].Kind == BRM::EKind::Gate && E.Gate[G] == 255 && bAnimate && !bMissionSolvedSeen && E.bSolved)
		{
			if (ABRMissionDevice* D = FindMissionDevice(G))
			{
				if (UBRAssets* A = UBRAssets::Get(this))
				{
					ABRHUD::Caption(this, BR_STR(NSLOCTEXT("BR", "Caption.Gate", "[Une porte lourde se d\u00e9place]")), D->GetActorLocation(), 4.f);
					if (USoundBase* S = A->Sound(TEXT("S_M_Gate")))
					{
						UGameplayStatics::PlaySoundAtLocation(this, S, D->GetActorLocation(), 1.f, 1.f, 0.f, A->Attenuation(2600.f, false));
					}
				}
			}
		}
	}
	// v4.12 : les sorties gardees montrent l'etat qui les valide (voyant ; ascenseur alimente du Niveau 1)
	for (ABRExit* X : MissionExits)
	{
		if (IsValid(X) && BRM::IsExitGuarded(GetLevelNumber(), X->Target))
		{
			X->SetMissionOpen(BRM::IsExitOpen(MissionPlan, MissionState, X->Target), bAnimate);
		}
	}
	if (E.bSolved && !bMissionSolvedSeen)
	{
		bMissionSolvedSeen = true;
		if (bAnimate)
		{
			ABRHUD::Notify(this, BRMissionText::SolvedLine(GetLevelNumber()), 7.f, FLinearColor(0.55f, 1.f, 0.55f));
			// v4.13 : un court element narratif annonce la suite (trace de l'equipe de L.), en sous-titre
			const FString Story = BRMissionText::StoryLine(GetLevelNumber());
			if (!Story.IsEmpty())
			{
				ABRHUD::Caption(this, Story, FVector::ZeroVector, 7.f);
			}
			if (UBRAssets* A = UBRAssets::Get(this))
			{
				if (USoundBase* S = A->Sound(TEXT("S_Objective")))
				{
					UGameplayStatics::PlaySound2D(this, S, 0.8f);
				}
			}
		}
		if (GetLevelNumber() == 1)
		{
			MarkStoryEvent(BRStory::Event::ServiceLink); // v4.13 : premiere preuve que les installations repondent encore
		}
	}
}

void ABRWorld::OnMissionLoreFound()
{
	if (GetLevelNumber() == 0 && LoreFound >= 3)
	{
		NetCampaign.OptionalFound |= BRM::OptionalBitFor(0);
	}
	// Le message va au joueur qui l'a ramassee (ABRCharacter::ReceivePickup) : ici, seulement la campagne
	ForceNetUpdate();
}

void ABRWorld::DebugCompleteMission()
{
	if (!HasAuthority() || !IsMissionActive())
	{
		return;
	}
	MissionState.Solved |= 1;
	PublishMission(-1, 0);
}

void ABRWorld::DebugCompleteMissionDevice(int32 Device)
{
	// v4.13 (tests) : un mecanisme maintenu (observation, manivelle) mene a son terme par les regles ordinaires (hote)
	if (!HasAuthority() || !IsMissionActive() || Device < 0 || Device >= MissionPlan.NumDevices)
	{
		return;
	}
	for (int32 I = 0; I < 16; ++I)
	{
		const BRM::FResult R = BRM::Act(MissionPlan, MissionState, GetMissionCampaign(), Device, BRM::EAction::Hold, 1);
		if (R.Feedback != BRM::EFeedback::Progress)
		{
			break;
		}
	}
	PublishMission(Device, static_cast<uint8>(BRM::EFeedback::Done));
}

uint8 ABRWorld::DebugMissionAct(int32 Device, uint8 Action)
{
	// v4.15 (tests) : meme application que ServerMissionAct (mecanisme maintenu : une unite), sans portee ni ligne de vue
	if (!HasAuthority() || !IsMissionActive() || Device < 0 || Device >= MissionPlan.NumDevices || Action > static_cast<uint8>(BRM::EAction::Hold))
	{
		return BRMissionText::NotNow;
	}
	const BRM::FDevice& D = MissionPlan.Devices[Device];
	const bool bHeld = D.Kind == BRM::EKind::Observe || D.Kind == BRM::EKind::Crank;
	const BRM::FResult R = BRM::Act(MissionPlan, MissionState, GetMissionCampaign(), Device, bHeld ? BRM::EAction::Hold : BRM::EAction::Use, bHeld ? 1 : 0);
	if (R.bChanged || R.Feedback == BRM::EFeedback::Wrong || R.Feedback == BRM::EFeedback::Dead || R.Feedback == BRM::EFeedback::Order)
	{
		PublishMission(Device, static_cast<uint8>(R.Feedback));
	}
	return static_cast<uint8>(R.Feedback);
}

bool ABRWorld::HasGuaranteedForwardExit() const
{
	// v4.15 : sortie de progression posee par la mission (salle de la mission, alcove du Level 6, module de secours) ; une
	// mission absente ou abandonnee (MissionGen 0) n'en a pas : les chunks gardent alors leur sortie gardee (securite)
	if (MissionGen < 2)
	{
		return false;
	}
	const int32 Fwd = ForwardTarget(GetLevelNumber());
	for (const FMissionExitSpot& Spot : MissionExitSpots)
	{
		if (Spot.Target == Fwd)
		{
			return true;
		}
	}
	return false;
}

// =====================================================================================================================
// Sorties
// =====================================================================================================================

bool ABRWorld::CanUseExit(int32 Target, FString& OutReason) const
{
	// v4.12 : un passage condamne (niveau pas encore disponible dans cette version) ne s'utilise jamais
	if (BRLevels::ResolveExit(GetLevelNumber(), Target).Kind == BRContent::EExit::Sealed)
	{
		OutReason = BR_STR(NSLOCTEXT("BR", "Interact.ExitSealedUse", "Ce passage ne m\u00e8ne nulle part pour l'instant : le niveau suivant arrivera dans une prochaine mise \u00e0 jour."));
		return false;
	}
	if (IsLegacyObjectives())
	{
		return CanLeaveLevel(OutReason);
	}
	if (!IsMissionActive() || BRM::IsExitOpen(MissionPlan, MissionState, Target))
	{
		return true;
	}
	BRM::FEval E;
	GetMissionEval(E);
	FString Step;
	for (int32 I = 0; I < E.NumSteps; ++I)
	{
		if (E.Steps[I] != BRM::EStep::Done)
		{
			Step = BRMissionText::StepTitle(MissionPlan.Level, I);
			break;
		}
	}
	OutReason = BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Exit.Locked", "Sortie verrouill\u00e9e. Mission : {Step}  {Key}"), { { TEXT("Step"), BRLoc::Arg(Step) },
		{ TEXT("Key"), BRLoc::Arg(BRKeys::Tag(EBRAction::Inventory)) } });
	return false;
}

// =====================================================================================================================
// Sortie de groupe
// =====================================================================================================================

void ABRWorld::RequestDeparture(ABRCharacter* By, int32 Target, const FVector& ExitLocation)
{
	if (!By || IsTransitioning())
	{
		return;
	}
	if (!IsNetGame())
	{
		LeaveForTarget(Target); // seul : depart immediat
		return;
	}
	if (HasAuthority())
	{
		uint8 Reason = 0;
		if (!ServerStartDeparture(By, Target, LoadedSerial, Reason) && By->IsLocallyControlled())
		{
			By->NotifyDepartureRefused(Reason);
		}
		return;
	}
	By->RequestDepartureFromServer(Target);
	(void)ExitLocation;
}

bool ABRWorld::ServerStartDeparture(ABRCharacter* By, int32 Target, int32 LevelSerial, uint8& OutReason)
{
	OutReason = 0;
	if (!HasAuthority() || !By)
	{
		return false;
	}
	if (LevelSerial != LoadedSerial || !bLevelReady || TransState != ETrans::None)
	{
		OutReason = 4;
		return false;
	}
	if (By->IsDead() || By->IsLevelLoading())
	{
		OutReason = 5;
		return false;
	}
	FString Unused;
	if (!CanUseExit(Target, Unused))
	{
		OutReason = 1;
		return false;
	}
	// v4.12 : une vraie sortie de cette cible, demandee depuis son echelle (a n'importe quelle hauteur de la montee : le
	// conduit est ferme) ou a sa portee avec une ligne de vue vers le point de rassemblement (le meme point que le
	// rassemblement lui-meme). Regle partagee BRGather
	const ABRExit* Found = nullptr;
	BRGather::FExitShape FoundShape;
	const BRGather::FVec PL = ToGather(By->GetActorLocation());
	// v4.15 : la vue coupee est relevee (obstacle, composant, distance) : un refus devant une sortie se diagnostique
	FString Blocked;
	for (TActorIterator<ABRExit> It(GetWorld()); It; ++It)
	{
		if (It->Target != Target || It->IsHidden())
		{
			continue;
		}
		const BRGather::FExitShape Shape = GatherShapeOf(*It);
		const BRGather::EGather G = BRGather::CanRequest(Shape, PL);
		if (G == BRGather::EGather::No)
		{
			continue;
		}
		if (G == BRGather::EGather::FloorNeedsView)
		{
			FCollisionQueryParams Q(SCENE_QUERY_STAT(BRDepartLos), false, By);
			Q.AddIgnoredActor(*It);
			const FVector Eye = By->GetEyeLocation();
			const FVector Aim = FromGather(BRGather::ViewPoint(Shape));
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Aim, ECC_WorldStatic, Q))
			{
				Blocked = FString::Printf(TEXT("sortie %s, vue des yeux (%.0f, %.0f, %.0f) vers (%.0f, %.0f, %.0f) coupee par %s (%s) a %.0f cm sur %.0f cm"), *It->GetName(),
					Eye.X, Eye.Y, Eye.Z, Aim.X, Aim.Y, Aim.Z, *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.Distance, FVector::Dist(Eye, Aim));
				continue;
			}
		}
		Found = *It;
		FoundShape = Shape;
		break;
	}
	if (!Found)
	{
		OutReason = 2;
		LastDepartureBlock = Blocked;
		if (!Blocked.IsEmpty())
		{
			UE_LOG(LogBackrooms, Log, TEXT("Depart de groupe vers %d refuse a %s : %s"), Target, *GetNameSafe(By), *Blocked);
		}
		return false;
	}
	LastDepartureBlock.Reset();
	if (NetDeparture.Phase == 1 && NetDeparture.Serial == LoadedSerial)
	{
		if (NetDeparture.Target == Target)
		{
			return true; // demande repetee ou deuxieme joueur : meme depart
		}
		OutReason = 3;
		return false;
	}
	NetDeparture = FBRNetDeparture();
	NetDeparture.Phase = 1;
	NetDeparture.Id = ++NextDepartureId;
	NetDeparture.Serial = LoadedSerial;
	NetDeparture.Target = Target;
	// v4.12 : la forme de la sortie choisie (et non plus un point unique 150 cm au-dessus du pied d'une echelle)
	NetDeparture.Location = FromGather(BRGather::GatherPoint(FoundShape));
	NetDeparture.Style = static_cast<uint8>(FoundShape.Style);
	NetDeparture.Foot = FromGather(FoundShape.Foot);
	NetDeparture.Forward = FromGather(FoundShape.Forward);
	NetDeparture.Anchor = FromGather(FoundShape.Anchor);
	NetDeparture.TopZ = FoundShape.TopZ;
	DepartureExit = const_cast<ABRExit*>(Found);
	if (Found->Style == EBRExitStyle::ServiceDoor)
	{
		ServerOpenServiceDoor(By); // v4.13 : la porte s'ouvre pour tout le groupe, puis le rassemblement commence
	}
	const AGameStateBase* GS = GetWorld()->GetGameState();
	NetDeparture.Deadline = (GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) + 90.f;
	if (const APlayerState* PS = By->GetPlayerState())
	{
		NetDeparture.By = PS->GetPlayerName();
		NetDeparture.ByPlayerId = PS->GetPlayerId();
	}
	DepartureInitiator = By;
	DepartureReadyTime = 0.f;
	UpdateDeparture(0.f);
	ForceNetUpdate();
	OnRep_Departure();
	UE_LOG(LogBackrooms, Log, TEXT("Depart de groupe %d vers %d demande par %s"), NetDeparture.Id, Target, *NetDeparture.By);
	return true;
}

void ABRWorld::ServerCancelDeparture(ABRCharacter* By)
{
	// v4.12 : seul l'initiateur annule (en redescendant de l'echelle) ; un autre joueur qui s'eloigne cesse seulement
	// d'etre compte
	if (HasAuthority() && NetDeparture.Phase == 1 && By && By == DepartureInitiator.Get())
	{
		CancelDeparture(3);
	}
}

FVector ABRWorld::DepartureViewPoint(const ABRExit* Exit)
{
	return Exit ? FromGather(BRGather::ViewPoint(GatherShapeOf(Exit))) : FVector::ZeroVector;
}

bool ABRWorld::IsGatheredForDeparture(const ABRCharacter* C) const
{
	if (!C || NetDeparture.Phase != 1)
	{
		return false;
	}
	const BRGather::FExitShape Shape = GatherShapeOf(NetDeparture);
	const BRGather::EGather G = BRGather::Classify(Shape, ToGather(C->GetActorLocation()));
	if (G == BRGather::EGather::Ladder)
	{
		return true;
	}
	if (G != BRGather::EGather::FloorNeedsView)
	{
		return false;
	}
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BRGatherLos), false, C);
	if (const ABRExit* E = DepartureExit.Get())
	{
		Q.AddIgnoredActor(E);
	}
	return !GetWorld()->LineTraceTestByChannel(C->GetEyeLocation(), FromGather(BRGather::ViewPoint(Shape)), ECC_WorldStatic, Q);
}

void ABRWorld::CancelDeparture(uint8 Reason)
{
	NetDeparture.Phase = 3;
	NetDeparture.Reason = Reason;
	DepartureInitiator.Reset();
	DepartureExit.Reset();
	ForceNetUpdate();
	OnRep_Departure();
}

float ABRWorld::GetDepartureRemaining() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const float Now = GS ? static_cast<float>(GS->GetServerWorldTimeSeconds()) : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
	return FMath::Max(0.f, NetDeparture.Deadline - Now);
}

void ABRWorld::UpdateDeparture(float Dt)
{
	if (!HasAuthority() || NetDeparture.Phase != 1)
	{
		return;
	}
	if (NetDeparture.Serial != LoadedSerial || TransState != ETrans::None)
	{
		NetDeparture = FBRNetDeparture();
		return;
	}
	TArray<ABRCharacter*> All;
	GetPlayers(All);
	uint8 Ready = 0, Needed = 0, Carried = 0;
	const FVector Loc = NetDeparture.Location;
	for (ABRCharacter* C : All)
	{
		if (!C)
		{
			continue;
		}
		if (C->IsDead())
		{
			++Carried; // un joueur a terre est emmene avec le groupe
			continue;
		}
		if (C->IsLevelLoading())
		{
			continue; // en chargement : on ne l'attend pas
		}
		++Needed;
		// v4.12 : sur l'echelle (grimpeur compte a toute hauteur), ou au meme etage a vue du point de rassemblement
		Ready += IsGatheredForDeparture(C) ? 1 : 0;
	}
	if (Ready != NetDeparture.Ready || Needed != NetDeparture.Needed || Carried != NetDeparture.Carried)
	{
		NetDeparture.Ready = Ready;
		NetDeparture.Needed = Needed;
		NetDeparture.Carried = Carried;
		ForceNetUpdate();
	}
	if (Needed == 0)
	{
		CancelDeparture(4);
		return;
	}
	if (const ABRCharacter* Init = DepartureInitiator.Get())
	{
		if (!Init->IsDead() && FVector::Dist(Init->GetActorLocation(), Loc) > 2500.f)
		{
			CancelDeparture(2);
			return;
		}
	}
	if (GetDepartureRemaining() <= 0.f)
	{
		CancelDeparture(1);
		return;
	}
	DepartureReadyTime = Ready == Needed ? DepartureReadyTime + Dt : 0.f;
	if (DepartureReadyTime >= 1.2f)
	{
		NetDeparture.Phase = 2;
		ForceNetUpdate();
		OnRep_Departure();
		LeaveForTarget(NetDeparture.Target);
	}
}

void ABRWorld::OnRep_Departure()
{
	if (NetDeparture.Id == SeenDepartureId && NetDeparture.Phase == SeenDeparturePhase)
	{
		return;
	}
	SeenDepartureId = NetDeparture.Id;
	SeenDeparturePhase = NetDeparture.Phase;
	const FString Dest = DestinationLabel(NetDeparture.Target);
	switch (NetDeparture.Phase)
	{
	case 1:
		ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Depart.Started", "{Name} veut partir vers {Dest} : rejoignez la sortie (moins de 8 m)."),
			{ { TEXT("Name"), BRLoc::Arg(NetDeparture.By) }, { TEXT("Dest"), BRLoc::Arg(Dest) } }), 6.f, FLinearColor(1.f, 0.9f, 0.5f));
		break;
	case 2:
		ABRHUD::Notify(this, BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Depart.Leaving", "Le groupe part vers {Dest}."), { { TEXT("Dest"), BRLoc::Arg(Dest) } }), 4.f, FLinearColor(0.55f, 1.f, 0.55f));
		break;
	case 3:
		switch (NetDeparture.Reason)
		{
		case 1:
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Mission.Depart.Timeout", "D\u00e9part annul\u00e9 : tout le monde n'a pas rejoint la sortie \u00e0 temps.")), 5.f, FLinearColor(1.f, 0.6f, 0.4f));
			break;
		case 2:
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Mission.Depart.Left", "D\u00e9part annul\u00e9 : la personne qui l'a demand\u00e9 s'est \u00e9loign\u00e9e.")), 5.f, FLinearColor(1.f, 0.6f, 0.4f));
			break;
		case 4:
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Mission.Depart.Nobody", "D\u00e9part annul\u00e9 : plus personne n'est debout.")), 5.f, FLinearColor(1.f, 0.6f, 0.4f));
			break;
		default:
			ABRHUD::Notify(this, BR_STR(NSLOCTEXT("BR", "Mission.Depart.Cancelled", "D\u00e9part annul\u00e9.")), 4.f, FLinearColor(1.f, 0.6f, 0.4f));
			break;
		}
		break;
	default:
		break;
	}
}

FString ABRWorld::DestinationLabel(int32 Target) const
{
	Target = StoryDestination(Target); // v4.13 : niveau gare d'une ancienne partie apres les Poolrooms
	const BRContent::FExitResolution R = BRLevels::ResolveExit(GetLevelNumber(), Target);
	if (R.Kind == BRContent::EExit::Ending || Target == BRM::EndingTarget)
	{
		return BR_STR(NSLOCTEXT("BR", "Mission.Depart.Ending", "le dernier quai"));
	}
	if (R.Kind == BRContent::EExit::ChapterEnd)
	{
		return BR_STR(NSLOCTEXT("BR", "Mission.Depart.ChapterEnd", "la fin du contenu disponible"));
	}
	return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Depart.Level", "Niveau {N}"), { { TEXT("N"), BRLoc::Int(R.Kind == BRContent::EExit::Go && R.Target >= 0 ? R.Target : Target) } });
}

void ABRWorld::LeaveForTarget(int32 Target)
{
	// v4.13 : histoire : la sortie de progression accomplit l'etape (ou termine le chapitre) avant la transition, que
	// l'hote valide ensuite contre le fil. Apres les Poolrooms, une ancienne partie retrouve sa progression gardee
	uint32 StorySeed = 0;
	const int32 Dest = StoryOnLeave(Target, StorySeed);
	if (Dest != Target)
	{
		RequestTransition(Dest, false, StorySeed);
		return;
	}
	// v4.12 : la destination suit la disponibilite des niveaux de cette version (meme regle que l'invite et l'hote)
	const BRContent::FExitResolution Res = BRLevels::ResolveExit(GetLevelNumber(), Target);
	if (Res.Kind == BRContent::EExit::ChapterEnd)
	{
		if (HasAuthority())
		{
			const int32 Lot = BRContent::CurrentLot(BRLevels::Channel());
			NetCampaign.Endings |= static_cast<uint8>(FMath::Clamp(1 << (1 + Lot), 0, 0x80));
			ForceNetUpdate();
			MulticastChapterEnd(Lot);
		}
		return;
	}
	if (Res.Kind == BRContent::EExit::Sealed)
	{
		UE_LOG(LogBackrooms, Warning, TEXT("Sortie vers %d condamnee dans cette version : aucun depart"), Target);
		return;
	}
	if (Res.Kind == BRContent::EExit::Go && Target != BRM::EndingTarget)
	{
		RequestTransition(Res.Target);
		return;
	}
	if (Target == BRM::EndingTarget)
	{
		if (HasAuthority())
		{
			const bool bVariant = BRM::EndingVariant(GetMissionCampaign());
			NetCampaign.Endings |= bVariant ? 2 : 1;
			ForceNetUpdate();
			MulticastEnding(bVariant);
		}
		return;
	}
	RequestTransition(Target);
}

void ABRWorld::MulticastChapterEnd_Implementation(int32 Lot)
{
	bEndingShown = true;
	bEndingVariant = false;
	bChapterEnd = true;
	ChapterEndLot = Lot;
	if (ABRPlayerController* PC = LocalPC())
	{
		PC->OnEndingShown();
	}
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(TEXT("S_Objective")))
		{
			UGameplayStatics::PlaySound2D(this, S, 0.9f);
		}
	}
}

void ABRWorld::MulticastEnding_Implementation(bool bVariant)
{
	bEndingShown = true;
	bEndingVariant = bVariant;
	bChapterEnd = false;
	if (ABRPlayerController* PC = LocalPC())
	{
		PC->OnEndingShown();
	}
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(TEXT("S_Objective")))
		{
			UGameplayStatics::PlaySound2D(this, S, 0.9f);
		}
	}
}

void ABRWorld::CloseEnding(bool bContinue)
{
	bEndingShown = false;
	bChapterEnd = false;
	NetDeparture = FBRNetDeparture();
	if (bContinue && HasAuthority())
	{
		// Route annexe : l'exploration continue dans un niveau au hasard (la fin est enregistree)
		RequestTransition(-1);
	}
}

// =====================================================================================================================
// v4.12 : effets physiques des missions (eau locale, passerelle)
// =====================================================================================================================

void ABRWorld::SetLocalWater(const AActor* WaterOwner, uint8 Slot, const BRMech::FWaterBox& Box, float Target, float Speed, bool bSnap)
{
	if (!WaterOwner)
	{
		return;
	}
	FLocalWater* Found = nullptr;
	for (FLocalWater& W : LocalWaters)
	{
		if (W.Owner.Get() == WaterOwner && W.Slot == Slot)
		{
			Found = &W;
			break;
		}
	}
	if (!Found)
	{
		Found = &LocalWaters.AddDefaulted_GetRef();
		Found->Owner = WaterOwner;
		Found->Slot = Slot;
		bSnap = true;
	}
	const float Prev = Found->Box.Surface;
	Found->Box = Box;
	Found->Box.Surface = bSnap ? Target : Prev;
	Found->Target = Target;
	Found->Speed = Speed;
	if (bSnap)
	{
		Found->Rate = 0.f;
	}
}

float ABRWorld::GetLocalWaterSurface(const AActor* WaterOwner, uint8 Slot, float* OutRate) const
{
	for (const FLocalWater& W : LocalWaters)
	{
		if (W.Owner.Get() == WaterOwner && W.Slot == Slot)
		{
			if (OutRate)
			{
				*OutRate = W.Rate;
			}
			return W.Box.Surface;
		}
	}
	if (OutRate)
	{
		*OutRate = 0.f;
	}
	return -1.0e6f;
}

void ABRWorld::UpdateLocalWaters(float Dt)
{
	LocalWaters.RemoveAll([](const FLocalWater& W) { return !W.Owner.IsValid(); });
	for (FLocalWater& W : LocalWaters)
	{
		const float Before = W.Box.Surface;
		W.Box.Surface = BRMech::Approach(Before, W.Target, W.Speed, Dt);
		W.Rate = Dt > 0.f ? (W.Box.Surface - Before) / Dt : 0.f;
	}
}

BRMech::FWaterQuery ABRWorld::WaterAt(const FVector& P) const
{
	const FBRLevelDef& D = Def();
	BRMech::FWaterBox Boxes[16];
	int32 Num = 0;
	for (const FLocalWater& W : LocalWaters)
	{
		if (Num < UE_ARRAY_COUNT(Boxes))
		{
			Boxes[Num++] = W.Box;
		}
	}
	return BRMech::WaterAt(Boxes, Num, static_cast<float>(P.X), static_cast<float>(P.Y), static_cast<float>(P.Z), D.bWater, D.WaterHeight, FloorZAt(P));
}

void ABRWorld::SetBridgeNav(const BRMech::FFrame& Frame, bool bDown)
{
	BridgeNav.bActive = true;
	BridgeNav.bDown = bDown;
	BridgeNav.Frame = Frame;
}

bool ABRWorld::MissionNavDetour(const FVector& From, const FVector& Goal, FVector& OutWaypoint) const
{
	if (!BridgeNav.bActive)
	{
		return false;
	}
	BRMech::FVec3 F, G, Out;
	F.X = static_cast<float>(From.X);
	F.Y = static_cast<float>(From.Y);
	F.Z = static_cast<float>(From.Z);
	G.X = static_cast<float>(Goal.X);
	G.Y = static_cast<float>(Goal.Y);
	G.Z = static_cast<float>(Goal.Z);
	if (!BRMech::Bridge::Detour(BridgeNav.Frame, BridgeNav.bDown, F, G, Out))
	{
		return false;
	}
	OutWaypoint = FVector(Out.X, Out.Y, Out.Z);
	return true;
}

// =====================================================================================================================
// v4.11 : bruits du monde, machines, budget de menace
// =====================================================================================================================

void ABRWorld::ReportNoise(const FVector& Location, float Radius)
{
	if (!HasAuthority())
	{
		return;
	}
	Noises.RemoveAll([this](const FNoiseEvent& E) { return LevelTime - E.Time > 4.f; });
	if (Noises.Num() >= 8)
	{
		Noises.RemoveAt(0);
	}
	FNoiseEvent E;
	E.Location = Location;
	E.Radius = Radius;
	E.Time = LevelTime;
	Noises.Add(E);
}

bool ABRWorld::FindRecentNoise(const FVector& From, FVector& OutLocation) const
{
	float Best = TNumericLimits<float>::Max();
	for (const FNoiseEvent& E : Noises)
	{
		if (LevelTime - E.Time > 4.f)
		{
			continue;
		}
		// Sans ligne de vue, les murs absorbent une partie du bruit
		const float D = static_cast<float>(FVector::Dist(From, E.Location));
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRNoiseLos), false);
		// v4.15 : seuls les murs absorbent le bruit : les capsules (joueur qui fait le bruit, creature qui ecoute,
		// coequipier entre les deux) ne comptent pas. Avant, la capsule du joueur qui faisait le bruit (impulsion LiDAR,
		// test) ou celle d'un ecouteur grand (Wretch) coupait toujours la ligne : portee divisee par deux en vue directe.
		FCollisionResponseParams R;
		R.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		const bool bWall = GetWorld() && GetWorld()->LineTraceTestByChannel(From + FVector(0.f, 0.f, 80.f), E.Location, ECC_WorldStatic, Q, R);
		if (D <= E.Radius * (bWall ? 0.55f : 1.f) && D < Best)
		{
			Best = D;
			OutLocation = E.Location;
		}
	}
	return Best < TNumericLimits<float>::Max();
}

bool ABRWorld::IsNearActiveMachine(const FVector& Location, float Radius) const
{
	for (const ABRMissionDevice* D : MissionDevices)
	{
		if (IsValid(D) && D->IsShown() && D->IsRunning() && FVector::DistSquared(D->GetActorLocation(), Location) < FMath::Square(Radius))
		{
			return true;
		}
	}
	return false;
}

int32 ABRWorld::ThreatScore() const
{
	int32 Score = 0;
	for (const ABREntity* E : Entities)
	{
		if (IsValid(E))
		{
			Score += 1 + (E->GetChaseIntensity() > 0.3f ? 1 : 0);
		}
	}
	if (BlackoutPhase == EBlackout::Dark)
	{
		Score += 2;
	}
	if (HasPits())
	{
		TArray<ABRCharacter*> All;
		GetPlayers(All);
		for (const ABRCharacter* C : All)
		{
			const FIntPoint Cell = WorldToCell(C->GetActorLocation());
			if (C && IsPitRoomCell(Cell.X, Cell.Y))
			{
				++Score;
				break;
			}
		}
	}
	return Score;
}

int32 ABRWorld::ThreatBudget() const
{
	TArray<ABRCharacter*> All;
	GetPlayers(All);
	return All.Num() > 2 ? 5 : 4;
}
