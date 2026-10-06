// v4.11 : missions de niveau dans le monde : placement deterministe des mecanismes dans une zone finie, etat tenu par
// l'hote et replique, validation des actions, sorties conditionnees par la mission, sortie de groupe, campagne et fin.
#include "BRWorld.h"
#include "BRMission.h"
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

	/** Sortie de progression de la base (v4.11) */
	int32 BaseForwardTarget(int32 Level)
	{
		switch (Level)
		{
		case 1: return 4;
		case 2: return 3;
		case 3: return 4;
		case 4: return 5;
		case 5: return 6;
		case 6: return 8;
		case 8: return 9;
		case 9: return 10;
		case 10: return 11;
		case 11: return BRM::EndingTarget;
		case 37: return 4;
		default: return -1;
		}
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
						// v4.12 : placement d'une autre version : la progression est gardee, les mecanismes peuvent avoir bouge
						if (ResumePlaceVersion != BRM::PlaceVersion && MissionNotice == 0)
						{
							MissionNotice = 3;
						}
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
			UE_LOG(LogBackrooms, Log, TEXT("Mission du Niveau %d : %d mecanismes, %d sortie(s) garantie(s)%s"), Level, MissionPlan.NumDevices, MissionExitSpots.Num(),
				MissionPlan.bFallback ? TEXT(", variante de secours") : TEXT(""));
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
	const int32 MaxCells = FMath::CeilToInt(9500.f / S);

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
	Dist.Add(Start, 0);
	Order.Add(Start);
	for (int32 Head = 0; Head < Order.Num() && Order.Num() < 6000; ++Head)
	{
		const FIntPoint Cur = Order[Head];
		const int32 CD = Dist[Cur];
		if (CD >= MaxCells)
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
	if (Order.Num() < 24)
	{
		return false;
	}

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
				if (M < L || M > H || IsSpawnArea(C.X, C.Y) || OutCells.Contains(C) || !FarFromUsed(C, Zone == 4 ? 3 : 2))
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

	// Places libres d'une piece (cellule centrale et voisines) : faces de mur, sinon poteaux
	auto RoomSlots = [&](const FIntPoint& Room, TArray<FBRMissionSpot>& Slots, const FIntPoint* SkipFace)
	{
		Slots.Reset();
		TArray<FIntPoint> RoomCells;
		RoomCells.Add(Room);
		for (const FIntPoint& Dir : GMissionDirs)
		{
			const FIntPoint N = Room + Dir;
			if (Dist.Contains(N) && CanStep(Room, N) && !IsSpawnArea(N.X, N.Y))
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
	if (!PickCell(4, 0x5A11u, !bOutdoor, bOutdoor ? 0 : 2, Room) && !PickCell(4, 0x5A12u, false, 0, Room))
	{
		return false;
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
	if (!bBounded)
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
					if (Dist.Contains(N) && CanStep(Room, N) && !OutCells.Contains(N) && !IsSpawnArea(N.X, N.Y))
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
				const FIntPoint Dir = Dirs[CellHash(Cell, 0xE517u + T) % static_cast<uint32>(Dirs.Num())];
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
			const FMissionExitSpot& E = OutExits[0];
			FBRMissionSpot G;
			const FVector Fwd = FRotator(0.f, E.Yaw, 0.f).Vector();
			const float Ahead = E.Style == EBRExitStyle::Barn ? 525.f : (E.Style == EBRExitStyle::NoclipFloor ? 140.f : 48.f);
			G.Pos = E.Pos + Fwd * Ahead;
			G.Pos.Z = FloorZAt(G.Pos);
			G.Yaw = E.Yaw;
			G.Cell = WorldToCell(G.Pos);
			G.bWall = E.Style != EBRExitStyle::NoclipFloor && E.Style != EBRExitStyle::Barn;
			G.bValid = true;
			OutSpots[GateDevice] = G;
		}
		else
		{
			GateDevice = -1; // niveau fini (Niveau 0) : l'element visible prend une place de la salle
		}
	}

	// 4. Groupes (meme piece) puis mecanismes seuls, chacun dans sa bande de distance
	TMap<int32, TArray<int32>> Groups;
	for (int32 I = 0; I < Plan.NumDevices; ++I)
	{
		if (I == GateDevice)
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
		const TArray<FIntPoint> RoomCells = RoomSlots(GroupRoom, Slots, (GroupRoom == Room && bHasExitFace) ? &ExitFace : nullptr);
		// Les places voisines de la sortie restent libres (la porte s'ouvre devant)
		int32 SlotIdx = 0;
		for (const int32 Dev : Members)
		{
			while (SlotIdx < Slots.Num() && GroupRoom == Room && OutExits.Num() > 0
				&& FVector::Dist2D(Slots[SlotIdx].Pos, OutExits[0].Pos) < 140.f)
			{
				++SlotIdx;
			}
			if (SlotIdx >= Slots.Num())
			{
				return false;
			}
			OutSpots[Dev] = Slots[SlotIdx++];
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
	for (const FBRMissionSpot& Spot : OutSpots)
	{
		if (!Spot.bValid)
		{
			return false;
		}
		OutCells.Add(Spot.Cell);
	}
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
		// La porte s'ouvre devant la sortie : rien a moins de 1,4 m
		while (Next < Slots.Num() - 1 && OutExits.Num() > 0 && FVector::Dist2D(Slots[Next].Pos, OutExits[0].Pos) < 140.f)
		{
			++Next;
		}
		OutSpots[I] = Slots[FMath::Min(Next, Slots.Num() - 1)];
		++Next;
		OutCells.Add(OutSpots[I].Cell);
	}
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
				Exit->InitLadder(Def().WallHeight, Spot.Shaft);
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
			}
		}
	}
}

// =====================================================================================================================
// Actions (serveur)
// =====================================================================================================================

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
	// Distance 3D depuis les yeux et ligne de vue (pas a travers un mur, pas depuis un autre etage)
	const FVector Eye = By->GetEyeLocation();
	const FVector Target = Dev->GetInteractPoint();
	if (FVector::DistSquared(Eye, Target) > FMath::Square(MissionReachServer))
	{
		return BRMissionText::TooFar;
	}
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BRMissionLos), false, By);
	Q.AddIgnoredActor(Dev);
	if (GetWorld()->LineTraceTestByChannel(Eye, Target, ECC_WorldStatic, Q))
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
		case BRM::EKind::Crank: Radius = D.Role == BRM::R_Beacon ? 1800.f : 1000.f; break;
		case BRM::EKind::Switch: Radius = D.Role == BRM::R_Relay ? 1200.f : 600.f; break;
		case BRM::EKind::Button: Radius = 800.f; break;
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
		MissionCooldowns.Add(Device, Now + (D.Role == BRM::R_MillBrake ? 8.0 : 4.0));
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
	if (E.bSolved && !bMissionSolvedSeen)
	{
		bMissionSolvedSeen = true;
		if (bAnimate)
		{
			ABRHUD::Notify(this, BRMissionText::SolvedLine(GetLevelNumber()), 7.f, FLinearColor(0.55f, 1.f, 0.55f));
			if (UBRAssets* A = UBRAssets::Get(this))
			{
				if (USoundBase* S = A->Sound(TEXT("S_Objective")))
				{
					UGameplayStatics::PlaySound2D(this, S, 0.8f);
				}
			}
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
	if ((MissionState.Solved & 1) && MissionPlan.Level == 0)
	{
		OutReason = BR_STR(NSLOCTEXT("BR", "Mission.Exit.OtherRoute", "Ce passage est ferm\u00e9 : le levier d'aiguillage ouvre l'autre route."));
		return false;
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
			if (GetWorld()->LineTraceTestByChannel(By->GetEyeLocation(), FromGather(BRGather::ViewPoint(Shape)), ECC_WorldStatic, Q))
			{
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
		return false;
	}
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
		const bool bWall = GetWorld() && GetWorld()->LineTraceTestByChannel(From + FVector(0.f, 0.f, 80.f), E.Location, ECC_WorldStatic, Q);
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
