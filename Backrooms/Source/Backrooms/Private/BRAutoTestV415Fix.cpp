// v4.15 (correctifs) : -BRAutoTest -BRAutoTestV415Fix : les seules etapes de non-regression des correctifs v4.15 des places
// des mecanismes et des tests de marche (Saved/TestRuns/analysis : diag_v413_3, diag_v413_4, diag_interchange_verified),
// pour une verification courte :
//   - Niveaux 8 et 9, graine 4111 (niveaux 643828852 et 774538619) : rochers et maison hors des places des mecanismes ;
//     chaque mecanisme a une place debout a portee de la visee et selon la regle de l'hote, essai a blanc depuis elle ;
//   - Level 6, graines 4136, 4126, 6 et 208 : salle de l'aile, aucun avertissement, postes et cages atteignables, poste 1
//     tourne depuis sa place ;
//   - Poolrooms, graine 1217121623 (niveau 125584919) : mission a pied, vanne B depuis l'eau des canaux, sortie par le sas,
//     sans nager ;
//   - v4.15 (correctifs 4) : Niveau 2, graine 4120 (niveau 554144564, vannes 4 a 6 face a la porte de la vapeur) : places des
//     mecanismes (la place trouvee est celle d'ou la visee du client prend le mecanisme, BRTestV412::CheckMissionStand) ;
//     classement d'un blocage de la marche (une creature seule devant le marcheur, puis la meme devant un mur :
//     BRTestV412::ClassifyBlock) et marche vers le mecanisme 4 avec un Smiler immobile pose sur le chemin, dans un couloir
//     sombre (comme final2/full) : ecarte par le test, mecanisme atteint, aucun PROBLEME ;
//   - v4.15 (correctifs 4, relecture) : Niveau 8, graine du niveau 233637237 (final2/v413), partie classique : l'echelle des
//     chunks BRExit_42 en (4000, -5400, 0), enterree dans le rocher de sa paroi avant 55974b4 : point vise par le depart de
//     groupe et capsule du grimpeur libres, approche a pied, visee, montee jusqu'au sommet avec une seule demande, arrivee au
//     Niveau 9. En reseau (-BRNetTest, AddNetV415FixSteps) : hote au sommet, client a 3 m devant, rassemblement 2/2 puis depart.
// Les memes etapes font partie de -BRAutoTestV411 (AddMissionSteps : places des mecanismes de chaque niveau), de
// -BRAutoTestV412 (Poolrooms) et de -BRAutoTestV413 (Level 6), donc du test complet : cette suite n'y est pas ajoutee une
// seconde fois (les etapes des correctifs 4, Niveau 2 et Niveau 8, ne sont que dans cette suite et dans -BRNetTest). Une
// verification qui ne peut pas etre faite est notee "NON VERIFIE", jamais comptee comme reussie.
#include "BRAutoTest.h"
#include "BRAutoTestNet.h"
#include "BRAutoTestWalk.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BREntity.h"
#include "BRGatherLogic.h"
#include "BRInteractables.h"
#include "BRLevels.h"
#include "BRLightLogic.h"
#include "BRMechLogic.h"
#include "BRMission.h"
#include "BRPlayerController.h"
#include "BRWorld.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"

namespace BRTestV415Fix
{
	/** v4.15 (correctifs 4) : creature posee par les etapes d'un blocage, etat de la marche forcee */
	TWeakObjectPtr<ABREntity> BlockEntity;
	/** v4.15 (correctifs 4, relecture) : cle du Smiler pose, comparee aux entites que la marche a ecartees (BRTestV412::FWalk::
	 *  Cleared) meme apres sa destruction : un chasseur ecarte a sa place ne compte pas */
	FObjectKey BlockKey;
	bool bWalking = false;
	bool bLightWas = false;
	int32 BlockWaypoint = -1;

	/** Joueur pose (sans vitesse) en P (au sol), regard vers Dir : mise en place d'une verification, pas une marche */
	void PlaceAt(const ABRWorld* W, ABRCharacter* C, const FVector& P, const FVector& Dir)
	{
		const float Half = C->GetSimpleCollisionHalfHeight();
		C->SetActorLocation(FVector(P.X, P.Y, W->FloorZAt(P) + Half + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* M = C->GetCharacterMovement())
		{
			M->StopMovementImmediately();
		}
		if (AController* Ctrl = C->GetController())
		{
			Ctrl->SetControlRotation(Dir.Rotation());
		}
	}

	/** Creature figee (temps ralenti 50 fois, comme les captures du banc visuel) au sol ou a sa hauteur de vol en P */
	ABREntity* SpawnFrozen(ABRWorld* W, EBREntityKind Kind, const FVector& P)
	{
		const FBREntityInfo& Info = ABREntity::Info(Kind);
		ABREntity* E = W->SpawnEntity(Kind, FVector(P.X, P.Y, W->FloorZAt(P) + (Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 5.f)));
		if (E)
		{
			E->CustomTimeDilation = 0.02f;
		}
		return E;
	}

	// v4.15 (correctifs 4, relecture) : echelle des chunks BRExit_42 du Niveau 8 (final2/v413 : graine du niveau 233637237,
	// partie classique, sortie vers le Niveau 9 en (4000, -5400, 0), avertissement "point vise par le depart de groupe
	// (4100, -5400, 90) pris dans BRChunk_431" avant 55974b4)
	constexpr uint32 Level8Seed = 233637237u;
	constexpr double LadderX = 4000.0;
	constexpr double LadderY = -5400.0;
	constexpr int32 LadderTarget = 9;
	/** Cellules de grille entre le depart de l'approche a pied et la cellule devant l'echelle */
	constexpr int32 ApproachCells = 4;
	/** Barrieres du test reseau (le client signale l'etape atteinte, BRTestNet) */
	constexpr int32 MarkNetStart = 4154;
	constexpr int32 MarkNetPlaced = 4155;
	constexpr int32 MarkNetNoPlace = 4156;

	/** Etat des etapes de l'echelle (solo et reseau) */
	TWeakObjectPtr<ABRExit> Ladder;
	bool bLadderOk = false;
	int32 Phase = 0;
	int32 SerialMark = 0;
	int32 RequestsMark = 0;
	double Clock = 0.0;
	float FootZ = 0.f;
	float TopZ = 0.f;
	float MaxZ = -1.0e9f;
	float ApproachFrom = 0.f;
	/** Reseau : depart vu (rassembles au plus, phase 2, annulation et sa raison) */
	bool bNetOk = false;
	bool bDepStarted = false;
	bool bDepPhase2 = false;
	bool bDepCancelled = false;
	uint8 DepReason = 0;
	uint8 MaxReady = 0;
	uint8 MaxNeeded = 0;
	BRTestNet::FPlacement Place;

	FVector LadderSpot()
	{
		return FVector(LadderX, LadderY, 0.0);
	}

	const TCHAR* Oui(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	/** La sortie vers le Niveau 9 a moins de 50 cm (a l'horizontale) de la place de BRExit_42 (son nom depend de l'ordre des
	 *  creations : elle est retrouvee par sa place) */
	ABRExit* FindLadder(UWorld* World)
	{
		ABRExit* Best = nullptr;
		double BestD = 50.0;
		for (TActorIterator<ABRExit> It(World); It; ++It)
		{
			const double D = FVector::Dist2D(It->GetActorLocation(), LadderSpot());
			if (It->Target == LadderTarget && It->IsClimbable() && D < BestD)
			{
				Best = *It;
				BestD = D;
			}
		}
		return Best;
	}

	/** Les deux requetes de la verification du developpeur (ABRExit::CheckDepartureViewPoint) : sphere de 10 cm au point vise par
	 *  le depart de groupe (ABRWorld::DepartureViewPoint), capsule du grimpeur a l'accroche (GetClimbAnchor, posee 5 cm au-dessus
	 *  du pied) ; canal WorldStatic, l'echelle et les pions ignores. Ce qui les prend, "libre" sinon */
	FString LadderBlocker(UWorld* World, const ABRExit* Exit, bool bClimber)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestLadderClear), false, Exit);
		FCollisionResponseParams R;
		R.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		constexpr float Radius = BRMech::Clearance::PlayerRadius - BRMech::Clearance::Margin;
		constexpr float Half = 88.f - 2.f * BRMech::Clearance::Margin;
		const FVector Anchor = Exit->GetClimbAnchor();
		const FVector At = bClimber ? FVector(Anchor.X, Anchor.Y, Exit->GetActorLocation().Z + 5.f + 2.f * BRMech::Clearance::Margin + Half)
			: ABRWorld::DepartureViewPoint(Exit);
		const FCollisionShape Shape = bClimber ? FCollisionShape::MakeCapsule(Radius, Half) : FCollisionShape::MakeSphere(10.f);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, At, FQuat::Identity, ECC_WorldStatic, Shape, Q, R);
		for (const FOverlapResult& O : Overlaps)
		{
			if (O.bBlockingHit)
			{
				return FString::Printf(TEXT("PRIS dans %s (%s)"), *GetNameSafe(O.GetActor()), *GetNameSafe(O.GetComponent()));
			}
		}
		return FString(TEXT("libre"));
	}

	/** Place debout (centre de la capsule) sur le sol de cette cellule ou pres de son centre, au niveau du pied de l'echelle
	 *  (a 40 cm pres), chunk construit, capsule libre ; false sinon */
	bool StandIn(const ABRWorld* W, const ABRCharacter* C, const FVector& P, FVector& Out)
	{
		const float Half = C->GetSimpleCollisionHalfHeight();
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestLadderStand), false, C);
		for (const FVector& Off : { FVector(0.0, 0.0, 0.0), FVector(80.0, 0.0, 0.0), FVector(-80.0, 0.0, 0.0), FVector(0.0, 80.0, 0.0), FVector(0.0, -80.0, 0.0) })
		{
			const FVector At = P + Off;
			float FloorZ = 0.f;
			if (!W->IsChunkLoaded(W->CellToChunk(W->WorldToCell(At))) || !BRTestV412::FloorUnder(W, At, FootZ + 100.f, C, FloorZ) || FMath::Abs(FloorZ - FootZ) > 40.f)
			{
				continue;
			}
			const FVector Stand(At.X, At.Y, FloorZ + Half + 5.f);
			if (!W->GetWorld()->OverlapAnyTestByChannel(Stand, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(36.f, Half - 4.f), Q))
			{
				Out = Stand;
				return true;
			}
		}
		return false;
	}

	/** Depart de l'approche a pied : une cellule a ApproachCells pas de grille (ABRWorld::CanStep, parcours en largeur dans l'ordre
	 *  des voisins) de la cellule devant l'echelle, sinon la plus lointaine trouvee ; false : aucune (on marche d'ou l'on est) */
	bool ApproachStart(const ABRWorld* W, const ABRCharacter* C, const FIntPoint& Front, FVector& Out)
	{
		const FIntPoint Steps4[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		TArray<TPair<FIntPoint, int32>> Queue;
		TSet<FIntPoint> Seen;
		Queue.Add(TPair<FIntPoint, int32>(Front, 0));
		Seen.Add(Front);
		for (int32 I = 0; I < Queue.Num(); ++I)
		{
			const FIntPoint Cell = Queue[I].Key;
			const int32 Depth = Queue[I].Value;
			if (Depth >= ApproachCells)
			{
				continue;
			}
			for (const FIntPoint& D : Steps4)
			{
				const FIntPoint N = Cell + D;
				if (!Seen.Contains(N) && W->CanStep(Cell, N) && !(W->HasPits() && W->IsPitRoomCell(N.X, N.Y)))
				{
					Seen.Add(N);
					Queue.Add(TPair<FIntPoint, int32>(N, Depth + 1));
				}
			}
		}
		// Les plus lointaines d'abord (dans l'ordre du parcours), sans revenir a la cellule devant l'echelle
		for (int32 Depth = ApproachCells; Depth >= 1; --Depth)
		{
			for (const TPair<FIntPoint, int32>& Q : Queue)
			{
				if (Q.Value == Depth && StandIn(W, C, W->CellCenter(Q.Key, FootZ), Out))
				{
					ApproachFrom = static_cast<float>(Depth);
					return true;
				}
			}
		}
		return false;
	}

	/** Joueur pose (sans vitesse) en Stand, regard vers Target : mise en place d'une verification, pas une marche */
	void PlaceStand(ABRCharacter* C, const FVector& Stand, const FVector& Target)
	{
		C->SetActorLocation(Stand, false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* M = C->GetCharacterMovement())
		{
			M->StopMovementImmediately();
		}
		if (AController* Ctrl = C->GetController())
		{
			Ctrl->SetControlRotation((Target - C->GetEyeLocation()).Rotation());
		}
	}

	void FaceTo(ABRCharacter* C, const FVector& Target)
	{
		if (AController* Ctrl = C ? C->GetController() : nullptr)
		{
			Ctrl->SetControlRotation((Target - C->GetEyeLocation()).Rotation());
		}
	}

	FVector LadderFront(const ABRExit* Exit)
	{
		return FRotator(0.f, Exit->GetActorRotation().Yaw, 0.f).Vector();
	}

	/** Le point que vise la touche d'interaction sur l'echelle (comme AddWalkExitSteps) */
	FVector LadderAim(const ABRExit* Exit)
	{
		return Exit->GetClimbAnchor() + FVector(0.f, 0.f, 60.f);
	}

	/** Echelle trouvee (chunk construit) : sa description et les deux requetes ; bOutFree : les deux libres */
	FString LadderReport(UWorld* World, const ABRWorld* W, const ABRExit* Exit, bool& bOutFree)
	{
		const FString View = LadderBlocker(World, Exit, false);
		const FString Climb = LadderBlocker(World, Exit, true);
		bOutFree = View == TEXT("libre") && Climb == TEXT("libre");
		const FVector L = Exit->GetActorLocation();
		const FVector Aim = ABRWorld::DepartureViewPoint(Exit);
		return FString::Printf(TEXT("echelle vers le Niveau %d (%s) en (%.0f, %.0f, %.0f), paroi de la cellule (%d, %d), sommet de la montee a %.0f cm du pied ; point vise par le depart de "
			"groupe (%.0f, %.0f, %.0f) %s ; capsule du grimpeur a l'accroche %s"), Exit->Target, *Exit->GetName(), L.X, L.Y, L.Z, W->WorldToCell(L - LadderFront(Exit) * 30.f).X,
			W->WorldToCell(L - LadderFront(Exit) * 30.f).Y, Exit->GetClimbTopZ() - L.Z, Aim.X, Aim.Y, Aim.Z, *View, *Climb);
	}
}

void ABRAutoTest::AddV415FixSteps()
{
	Add(TEXT("v4.15 correctifs : preparation"), 0.f, [this]()
	{
		FLevelReport R;
		R.Title = TEXT("v4.15 (correctifs) : places des mecanismes (Niveaux 6, 8, 9), Poolrooms a pied sans nager");
		R.Scene = TEXT("v4.15");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		return true;
	});
	// Niveaux 8 et 9 : la graine de -BRAutoTestV411 (rochers du Niveau 8, perron de la maison du Niveau 9)
	for (const int32 PlaceLevel : { 8, 9 })
	{
		AddSeedLoad(PlaceLevel, 4111, FString::Printf(TEXT("v4.15 correctifs : Niveau %d (graine 4111)"), PlaceLevel));
		AddMissionPlaceSweep(PlaceLevel, false);
	}
	// Level 6 : aile, postes et cages (BRAutoTestV413Lidar.cpp)
	AddV415LidarPlaceSteps();
	// Poolrooms : vanne B et sas a pied (BRAutoTestV412.cpp)
	AddV415PoolroomsSteps();

	// v4.15 (correctifs 4) : Niveau 2, graine 4120 (niveau 554144564) : vannes face a la porte de la vapeur (la graine du Niveau 2
	// de -BRAutoTestV411, 4111, n'a pas cette disposition) ; puis un blocage de la marche par une creature
	AddSeedLoad(2, 4120, TEXT("v4.15 correctifs 4 : Niveau 2 (graine 4120)"));
	AddMissionPlaceSweep(2, false);
	Add(TEXT("v4.15 correctifs 4 : classement d'un blocage (creature seule, creature devant un mur)"), 0.5f, [this]()
	{
		using namespace BRTestV415Fix;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->GetLevelNumber() != 2)
		{
			Skip(TEXT("Niveau 2 non atteint : classement d'un blocage non verifie"));
			return true;
		}
		// Une cellule du couloir avec une face fermee (mur) et une face ouverte sur un couloir qui continue
		const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		const FIntPoint Here = W->WorldToCell(C->GetActorLocation());
		FIntPoint Cell(0, 0), Closed(0, 0), Open(0, 0);
		bool bFound = false;
		for (int32 R = 1; R <= 6 && !bFound; ++R)
		{
			for (int32 X = Here.X - R; X <= Here.X + R && !bFound; ++X)
			{
				for (int32 Y = Here.Y - R; Y <= Here.Y + R && !bFound; ++Y)
				{
					const FIntPoint Ce(X, Y);
					if (!W->IsWalkable(Ce) || W->IsSpawnArea(X, Y) || W->IsMissionCell(X, Y))
					{
						continue;
					}
					for (const FIntPoint& A : Dirs)
					{
						for (const FIntPoint& B : Dirs)
						{
							if (!bFound && A != B && !W->CanStep(Ce, Ce + A) && W->CanStep(Ce - A, Ce) && W->CanStep(Ce, Ce + B) && W->CanStep(Ce + B, Ce + B + B)
								&& W->CanStep(Ce - B, Ce))
							{
								Cell = Ce;
								Closed = A;
								Open = B;
								bFound = true;
							}
						}
					}
				}
			}
		}
		if (!bFound)
		{
			Skip(TEXT("classement d'un blocage : aucune cellule de couloir avec un mur et une suite a moins de 6 cellules"));
			return true;
		}
		// Le marcheur 55 cm avant le centre, une creature 90 cm devant lui (contre lui, sans le toucher), puis la suite du chemin
		const FVector Center = W->CellCenter(Cell, 0.f);
		auto Check = [&](const FIntPoint& D, BRTestV412::FBlock& Out)
		{
			const FVector Dir(D.X, D.Y, 0.f);
			const FVector P0 = Center - Dir * 55.f;
			PlaceAt(W, C, P0, Dir);
			ABREntity* E = SpawnFrozen(W, EBREntityKind::Hound, P0 + Dir * 90.f);
			if (!E)
			{
				return false;
			}
			Out = BRTestV412::ClassifyBlock(W, C, P0 + Dir * 300.f, P0 + Dir * 400.f);
			const bool bFirst = Out.First == E;
			E->Destroy();
			return bFirst;
		};
		BRTestV412::FBlock Wall, Free;
		const bool bWallFirst = Check(Closed, Wall);
		const bool bFreeFirst = Check(Open, Free);
		if (!bWallFirst && !bFreeFirst && !Wall.bBlocked && !Free.bBlocked)
		{
			Skip(FString::Printf(TEXT("classement d'un blocage : creature non creee ou pas touchee (cellule %d, %d)"), Cell.X, Cell.Y));
			return true;
		}
		const bool bOk = bWallFirst && !Wall.bEntity && Wall.Static != nullptr && bFreeFirst && Free.bEntity && Free.Static == nullptr;
		Note(FString::Printf(TEXT("classement d'un blocage, cellule (%d, %d) : creature devant le marcheur dans le couloir libre : %s (%s) ; la meme devant un mur a 140 cm : %s "
			"(%s) (attendu : creature seule, puis decor : le mur reste un blocage du decor malgre la creature)"), Cell.X, Cell.Y, Free.bEntity ? TEXT("creature seule") : TEXT("DECOR"),
			*Free.What, Wall.bEntity ? TEXT("CREATURE SEULE") : TEXT("decor"), *Wall.What), !bOk);
		return true;
	});
	Add(TEXT("v4.15 correctifs 4 : marche vers le mecanisme 4 avec un Smiler sur le chemin"), 0.5f, [this]()
	{
		using namespace BRTestV415Fix;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->GetLevelNumber() != 2 || !W->IsMissionActive())
		{
			Skip(TEXT("Niveau 2 non atteint ou sans mission : marche avec un Smiler sur le chemin non verifiee"));
			return true;
		}
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		auto End = [&]()
		{
			BRTestV412::ReleaseJump(C);
			if (ABREntity* E = BlockEntity.Get())
			{
				E->Destroy();
			}
			BlockEntity.Reset();
			BlockKey = FObjectKey();
			if (C->IsFlashlightOn() != bLightWas)
			{
				C->ToggleFlashlight();
			}
			bWalking = false;
			return true;
		};
		if (!bWalking)
		{
			ABRMissionDevice* Dev = W->FindMissionDevice(4);
			if (!Dev)
			{
				Skip(TEXT("marche avec un Smiler : mecanisme 4 absent"));
				return true;
			}
			// Lampe eteinte (un faisceau sur un Smiler le fait charger) ; marche vers la cellule du mecanisme
			bLightWas = C->IsFlashlightOn();
			if (bLightWas)
			{
				C->ToggleFlashlight();
			}
			BRTestV412::BeginWalk(W, C, W->CellCenter(Dev->GetCell(), static_cast<float>(C->GetActorLocation().Z)), 200.f, true);
			// Un point de passage en ligne droite dans un couloir (murs des deux cotes), sombre (la lumiere dissiperait le Smiler),
			// a plus de 3 points du depart
			const TArray<FVector>& Pts = BRTestV412::Walk.Points;
			BlockWaypoint = -1;
			for (int32 K = 3; K + 1 < Pts.Num() && K <= 14 && BlockWaypoint < 0; ++K)
			{
				const FIntPoint A = W->WorldToCell(Pts[K - 1]), B = W->WorldToCell(Pts[K]), N = W->WorldToCell(Pts[K + 1]);
				const FIntPoint D = B - A;
				const FIntPoint Side(-D.Y, D.X);
				const FBREntityInfo& Info = ABREntity::Info(EBREntityKind::Smiler);
				const FVector At(Pts[K].X, Pts[K].Y, W->FloorZAt(Pts[K]) + Info.HoverHeight);
				if (N - B == D && !W->CanStep(B, B + Side) && !W->CanStep(B, B - Side) && W->LightLevelAt(At) < BRLight::SmilerVanish * 0.8f)
				{
					BlockWaypoint = K;
				}
			}
			if (BlockWaypoint < 0)
			{
				Skip(FString::Printf(TEXT("marche avec un Smiler : aucun point de passage en couloir droit et sombre parmi %d"), Pts.Num()));
				return End();
			}
			BlockEntity = SpawnFrozen(W, EBREntityKind::Smiler, Pts[BlockWaypoint]);
			if (!BlockEntity.IsValid())
			{
				Skip(TEXT("marche avec un Smiler : Smiler non cree"));
				return End();
			}
			BlockKey = FObjectKey(BlockEntity.Get());
			bWalking = true;
			return false;
		}
		const BRTestV412::EWalk R = BRTestV412::StepWalk(W, C, Dt, 150.0, C->GetScareKind() >= 0 || C->IsInputLocked());
		if (R == BRTestV412::EWalk::Moving)
		{
			return false;
		}
		const FIntPoint Cell = W->WorldToCell(BRTestV412::Walk.Points.IsValidIndex(BlockWaypoint) ? BRTestV412::Walk.Points[BlockWaypoint] : C->GetActorLocation());
		// v4.15 (correctifs 4, relecture) : le Smiler pose lui-meme a ete ecarte (avant : toute entite ecartee comptait ; au Niveau 2,
		// un chasseur, Hound ou Wretch, pouvait l'etre a sa place pendant que le Smiler etait contourne ou dissipe)
		const bool bSmilerCleared = BRTestV412::Walk.Cleared.Contains(BlockKey);
		const TCHAR* const SmilerState = BlockEntity.IsValid() ? (BlockEntity->IsVanishing() ? TEXT("dissipe") : TEXT("contourne")) : TEXT("disparu");
		const FString Others = BRTestV412::Walk.EntityClears > 0 ? FString::Printf(TEXT("entite(s) ecartee(s) par le test : %s"), *BRTestV412::Walk.EntityClearText)
			: FString(TEXT("aucune entite ecartee"));
		if (R == BRTestV412::EWalk::Arrived && !bSmilerCleared)
		{
			Skip(FString::Printf(TEXT("marche avec un Smiler pose a la cellule (%d, %d) : arrivee sans que le Smiler pose barre le passage (Smiler %s ; %s) : %s"), Cell.X, Cell.Y,
				SmilerState, *Others, *BRTestV412::WalkStats()));
			return End();
		}
		if (R == BRTestV412::EWalk::Stuck && BRTestV412::Walk.bEntityBlock)
		{
			// Plafond des entites ecartees atteint par d'autres creatures : rien n'est appris du decor
			Skip(FString::Printf(TEXT("marche avec un Smiler pose a la cellule (%d, %d) : NON VERIFIE, une entite seule barre encore le passage (Smiler pose ecarte %s ; %s) : %s"),
				Cell.X, Cell.Y, Oui(bSmilerCleared), *Others, *BRTestV412::WalkStats()));
			return End();
		}
		// Arrivee apres avoir ecarte le Smiler pose : attendu. Bloque par le decor : PROBLEME
		Note(FString::Printf(TEXT("marche vers le mecanisme 4 (graine du niveau %u), Smiler immobile pose a la cellule (%d, %d) : %s ; %s"), W->GetSeed(), Cell.X, Cell.Y,
			R == BRTestV412::EWalk::Arrived ? TEXT("ecarte par le test, mecanisme atteint") : TEXT("NON ATTEINT"), *BRTestV412::WalkStats()), R != BRTestV412::EWalk::Arrived);
		return End();
	});
	LimitLastStep(170.f);

	// v4.15 (correctifs 4, relecture) : Niveau 8, graine du niveau 233637237 (celle de final2/v413), partie classique imposee
	// (ABRWorld::SetLinearTestOverride, avant le chargement : le parcours lineaire ne tire aucune sortie des chunks au Niveau 8).
	// L'echelle BRExit_42, enterree dans le rocher de sa paroi avant 55974b4, doit etre libre, s'approcher a pied, se viser et se
	// monter avec une seule demande au sommet. Echelle introuvable : NON VERIFIE, jamais OK
	Add(TEXT("v4.15 correctifs 4 : partie classique imposee (Niveau 8)"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		W->SetLinearTestOverride(0);
		return true;
	});
	AddLevelSeedLoad(8, BRTestV415Fix::Level8Seed, TEXT("v4.15 correctifs 4 : Niveau 8 (graine du niveau 233637237, partie classique)"));
	Add(TEXT("v4.15 correctifs 4 : echelle BRExit_42 du Niveau 8, place libre"), 0.5f, [this]()
	{
		using namespace BRTestV415Fix;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		Ladder.Reset();
		bLadderOk = false;
		Phase = 0;
		if (!W || !C)
		{
			return false;
		}
		if (W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 8 || W->GetSeed() != Level8Seed)
		{
			Skip(FString::Printf(TEXT("Niveau 8 (graine du niveau %u) non charge (Niveau %d, graine %u) : echelle BRExit_42 non verifiee"), Level8Seed, W->GetLevelNumber(),
				W->GetSeed()));
			return true;
		}
		if (W->IsLinearLevel())
		{
			Skip(TEXT("Niveau 8 charge en parcours lineaire malgre la partie classique imposee : aucune sortie des chunks, echelle BRExit_42 non verifiee"));
			return true;
		}
		const FIntPoint Chunk = W->CellToChunk(W->WorldToCell(LadderSpot()));
		ABRExit* Exit = FindLadder(GetWorld());
		if ((!Exit || !W->IsChunkLoaded(Chunk)) && StepTime < 30.f)
		{
			return false;
		}
		if (!Exit || !W->IsChunkLoaded(Chunk))
		{
			Skip(FString::Printf(TEXT("Niveau 8 (graine du niveau %u, partie classique) : NON VERIFIE, aucune echelle vers le Niveau %d a 50 cm de (%.0f, %.0f) en 30 s (chunk (%d, %d) "
				"construit %s)"), Level8Seed, LadderTarget, LadderX, LadderY, Chunk.X, Chunk.Y, Oui(W->IsChunkLoaded(Chunk))));
			return true;
		}
		if (BRLevels::ResolveExit(8, LadderTarget).Kind == BRContent::EExit::Sealed)
		{
			Skip(FString::Printf(TEXT("Niveau 8 : passage vers le Niveau %d condamne dans ce canal : echelle BRExit_42 non verifiee"), LadderTarget));
			return true;
		}
		bool bFree = false;
		const FString Report = LadderReport(GetWorld(), W, Exit, bFree);
		Note(FString::Printf(TEXT("Niveau 8 (graine du niveau %u, partie classique), chunk (%d, %d) : %s (attendu : libres, verification du developpeur ABRExit::"
			"CheckDepartureViewPoint)"), Level8Seed, Chunk.X, Chunk.Y, *Report), !bFree);
		Shot(TEXT("v415fix_niveau8_echelle"));
		Ladder = Exit;
		bLadderOk = bFree;
		FootZ = static_cast<float>(Exit->GetActorLocation().Z);
		TopZ = Exit->GetClimbTopZ();
		return true;
	});
	LimitLastStep(40.f);
	Add(TEXT("v4.15 correctifs 4 : echelle BRExit_42 du Niveau 8, approche, visee et montee"), 0.f, [this]()
	{
		using namespace BRTestV415Fix;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		ABRExit* Exit = Ladder.Get();
		if (Phase == 0 && (!Exit || !bLadderOk))
		{
			if (Exit)
			{
				Skip(TEXT("echelle BRExit_42 : place prise (notee plus haut) : approche et montee non tentees"));
			}
			return true; // echelle introuvable : NON VERIFIE plus haut
		}
		const double Now = FPlatformTime::Seconds();
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		const float Half = C->GetSimpleCollisionHalfHeight();
		auto Stop = [&]()
		{
			BRTestV412::ReleaseJump(C);
			Phase = 0;
			return true;
		};
		if (!Exit && Phase <= 3)
		{
			Note(TEXT("echelle BRExit_42 disparue pendant l'approche (chunk retire ?)"), true);
			return Stop();
		}
		switch (Phase)
		{
		case 0:
		{
			// La sortie vers le Niveau 9 est gardee par la mission du niveau : resolue par l'hote (seule l'echelle est jugee ici).
			// Depart de l'approche a ApproachCells cellules de la grille devant l'echelle (mise en place, pas une marche)
			W->DebugCompleteMission();
			const FVector Goal = Exit->GetClimbAnchor() + LadderFront(Exit) * 90.f;
			FVector Start;
			ApproachFrom = 0.f;
			if (ApproachStart(W, C, W->WorldToCell(Goal), Start))
			{
				PlaceStand(C, Start, Goal);
			}
			RequestsMark = C->ClimbRequestsSent;
			MaxZ = -1.0e9f;
			Clock = Now;
			Phase = 1;
			return false;
		}
		case 1:
		{
			if (Now - Clock < 0.5)
			{
				return false;
			}
			const FVector Goal = Exit->GetClimbAnchor() + LadderFront(Exit) * 90.f;
			BRTestV412::BeginWalk(W, C, FVector(Goal.X, Goal.Y, FootZ + Half), 60.f, true, true, FootZ);
			Phase = 2;
			return false;
		}
		case 2:
		{
			const BRTestV412::EWalk R = BRTestV412::StepWalk(W, C, Dt, 120.0, C->GetScareKind() >= 0 || C->IsInputLocked());
			if (R == BRTestV412::EWalk::Moving)
			{
				return false;
			}
			BRTestV412::ReleaseJump(C);
			const FIntPoint Cell = W->WorldToCell(C->GetActorLocation());
			if (R == BRTestV412::EWalk::Stuck && BRTestV412::Walk.bEntityBlock)
			{
				Skip(FString::Printf(TEXT("echelle BRExit_42 : approche NON VERIFIEE a pied (cellule %d, %d) : une entite seule barre le passage : %s"), Cell.X, Cell.Y,
					*BRTestV412::WalkStats()));
				return Stop();
			}
			if (R == BRTestV412::EWalk::Stuck)
			{
				Note(FString::Printf(TEXT("echelle BRExit_42 : approche a pied BLOQUEE a la cellule (%d, %d), pieds a %.0f cm (attendu %.0f), depuis %.0f cellule(s) : %s"), Cell.X, Cell.Y,
					C->GetActorLocation().Z - Half, FootZ, ApproachFrom, *BRTestV412::WalkStats()), true);
				Shot(TEXT("v415fix_niveau8_approche_bloquee"));
				return Stop();
			}
			Note(FString::Printf(TEXT("echelle BRExit_42 : pied de l'echelle atteint a pied depuis %.0f cellule(s) de la grille : %s"), ApproachFrom, *BRTestV412::WalkStats()),
				BRTestV412::Walk.Teleports > 0);
			Clock = Now;
			Phase = 3;
			return false;
		}
		case 3:
		{
			// La touche d'interaction sur l'echelle visee (comme AddWalkExitSteps)
			FaceTo(C, LadderAim(Exit));
			if (Now - Clock < 0.15)
			{
				return false;
			}
			if (C->FocusActor.Get() != Exit)
			{
				if (Now - Clock < 2.0)
				{
					return false;
				}
				Note(FString::Printf(TEXT("echelle BRExit_42 PAS VISEE par l'interaction depuis son pied (vise : %s)"), *BRTestV412::FocusName(W, C->FocusActor.Get())), true);
				Shot(TEXT("v415fix_niveau8_pas_visee"));
				return Stop();
			}
			C->Interact();
			Clock = Now;
			Phase = 4;
			return false;
		}
		case 4:
		{
			// On grimpe en avancant jusqu'au depart (une seule demande au sommet ; seul, le depart est immediat)
			if (W->IsTransitioning() || W->GetLevelNumber() != 8 || W->IsChapterEnd())
			{
				Clock = Now;
				Phase = 5;
				return false;
			}
			MaxZ = FMath::Max(MaxZ, static_cast<float>(C->GetActorLocation().Z));
			if (!C->IsClimbing())
			{
				if (Now - Clock > 1.0)
				{
					Note(TEXT("echelle BRExit_42 visee mais PAS PRISE par l'interaction"), true);
					return Stop();
				}
				return false;
			}
			C->InputMove(FVector2D(0.f, 1.f));
			if (Now - Clock > 30.0)
			{
				const UPrimitiveComponent* Block = C->ClimbBlocker.Get();
				Note(FString::Printf(TEXT("echelle BRExit_42 : pas de depart apres 30 s sur l'echelle (attente %d, demandes %d ; centre au plus haut a %.0f cm du pied, sommet a %.0f ; "
					"dernier obstacle : %s)"), static_cast<int32>(C->GetClimbWaitState()), C->ClimbRequestsSent - RequestsMark, MaxZ - FootZ, TopZ - FootZ,
					Block ? *FString::Printf(TEXT("%s (%s)"), *GetNameSafe(Block->GetOwner()), *Block->GetName()) : TEXT("aucun")), true);
				Shot(TEXT("v415fix_niveau8_montee"));
				return Stop();
			}
			return false;
		}
		default:
		{
			// Arrivee (niveau suivant, redirige, ou fin du contenu disponible)
			const BRContent::FExitResolution Res = BRLevels::ResolveExit(8, LadderTarget);
			const bool bEnd = Res.Kind == BRContent::EExit::ChapterEnd;
			const bool bThere = bEnd ? W->IsChapterEnd() : (!W->IsTransitioning() && W->IsLevelReady() && W->GetLevelNumber() == Res.Target);
			if (!bThere && Now - Clock < 40.0)
			{
				return false;
			}
			const int32 Requests = C->ClimbRequestsSent - RequestsMark;
			const bool bTop = MaxZ >= TopZ - 2.f;
			const bool bOk = bThere && Requests == 1 && bTop;
			Note(FString::Printf(TEXT("echelle BRExit_42 visee et montee : centre du grimpeur au plus haut a %.0f cm du pied (sommet de la montee a %.0f cm) %s, demandes de depart au "
				"sommet %d (attendu 1), %s %s"), MaxZ - FootZ, TopZ - FootZ, bTop ? TEXT("atteint") : TEXT("NON ATTEINT"), Requests,
				bEnd ? TEXT("fin du contenu disponible") : *FString::Printf(TEXT("arrivee au Niveau %d"), Res.Target), Oui(bThere)), !bOk);
			if (bEnd && W->IsChapterEnd())
			{
				W->CloseEnding(false);
			}
			return Stop();
		}
		}
	});
	LimitLastStep(240.f);
	Add(TEXT("v4.15 correctifs 4 : retour a la regle normale du parcours"), 0.f, [this]()
	{
		if (ABRWorld* W = GetBRWorld())
		{
			W->SetLinearTestOverride(-1);
		}
		BRTestV415Fix::Ladder.Reset();
		return true;
	});
}

// =====================================================================================================================
// v4.15 (correctifs 4, relecture) : reseau (-BRNetTest) : Niveau 8 en partie classique (graine du niveau 233637237), l'hote
// au sommet de l'echelle BRExit_42, le client a 3 m devant elle, a vue du point vise par le depart de groupe : rassemblement
// 2/2 puis depart (phase 2) avant la fin du delai (raison 1), arrivee au Niveau 9 chez chacun. Avant 55974b4, le point vise
// etait dans le rocher de la paroi : le client n'etait jamais compte et le depart finissait par la fin du delai
// =====================================================================================================================

void ABRAutoTest::AddNetV415FixSteps(bool bClient)
{
	// Barriere (comme AddNetV415Steps) : l'hote ne change de niveau qu'une fois le client arrive a cette etape
	Add(TEXT("v4.15 correctifs 4 reseau : Niveau 8 en partie classique"), 0.f, [this, bClient]()
	{
		using namespace BRTestV415Fix;
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		SerialMark = W->GetLevelSerial();
		Ladder.Reset();
		bNetOk = true;
		Phase = 0;
		bDepStarted = bDepPhase2 = bDepCancelled = false;
		DepReason = MaxReady = MaxNeeded = 0;
		if (bClient)
		{
			BRTestNet::SendMark(GetPC(), MarkNetStart);
			return true;
		}
		const int32 Mark = BRTestNet::RemoteMark(W);
		if (Mark != MarkNetStart && Mark != BRTestNet::MarkFinished && BRTestNet::RemoteMate(W))
		{
			if (StepTime < 90.f)
			{
				return false;
			}
			Note(FString::Printf(TEXT("hote : le client n'est pas arrive a l'etape de l'echelle BRExit_42 en 90 s (derniere etape signalee %d) : niveau change quand meme"), Mark),
				true);
		}
		SerialMark = W->GetLevelSerial();
		W->SetLinearTestOverride(0);
		W->RequestTransition(8, false, Level8Seed);
		return true;
	});
	LimitLastStep(100.f);
	Add(TEXT("v4.15 correctifs 4 reseau : Niveau 8 chez chacun"), 0.f, [this, bClient]()
	{
		using namespace BRTestV415Fix;
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		// Le client attend aussi que l'hote finisse ses etapes v4.15 et passe la barriere (120 s)
		const bool bThere = W->IsLevelReady() && !W->IsTransitioning() && W->GetLevelNumber() == 8 && W->GetLevelSerial() != SerialMark;
		if ((!bThere || StepTime < 3.f) && StepTime < 120.f)
		{
			return false;
		}
		const TCHAR* Who = bClient ? TEXT("client") : TEXT("hote");
		if (!bThere)
		{
			Note(FString::Printf(TEXT("%s : Niveau 8 (graine du niveau %u) non charge en 120 s"), Who, Level8Seed), true);
			bNetOk = false;
			return true;
		}
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = true;
		}
		const bool bOk = W->GetSeed() == Level8Seed && !W->IsLinearLevel();
		Note(FString::Printf(TEXT("%s : Niveau 8, graine %u (attendue %u), partie classique %s"), Who, W->GetSeed(), Level8Seed, Oui(!W->IsLinearLevel())), !bOk);
		bNetOk = bOk;
		SerialMark = W->GetLevelSerial();
		return true;
	});
	LimitLastStep(130.f);
	Add(TEXT("v4.15 correctifs 4 reseau : echelle BRExit_42 chez chacun"), 0.5f, [this, bClient]()
	{
		using namespace BRTestV415Fix;
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (!bNetOk)
		{
			return true;
		}
		const TCHAR* Who = bClient ? TEXT("client") : TEXT("hote");
		const FIntPoint Chunk = W->CellToChunk(W->WorldToCell(LadderSpot()));
		ABRExit* Exit = FindLadder(GetWorld());
		if ((!Exit || !W->IsChunkLoaded(Chunk)) && StepTime < 30.f)
		{
			return false;
		}
		if (!Exit || !W->IsChunkLoaded(Chunk) || BRLevels::ResolveExit(8, LadderTarget).Kind == BRContent::EExit::Sealed)
		{
			Skip(FString::Printf(TEXT("%s : NON VERIFIE, aucune echelle utilisable vers le Niveau %d a 50 cm de (%.0f, %.0f) en 30 s (chunk (%d, %d) construit %s)"), Who, LadderTarget,
				LadderX, LadderY, Chunk.X, Chunk.Y, Oui(W->IsChunkLoaded(Chunk))));
			bNetOk = false;
			return true;
		}
		// Chaque machine construit ses chunks : memes rochers retires chez l'hote et le client (meme graine, meme parcours recu)
		bool bFree = false;
		const FString Report = LadderReport(GetWorld(), W, Exit, bFree);
		Note(FString::Printf(TEXT("%s : %s"), Who, *Report), !bFree);
		Ladder = Exit;
		FootZ = static_cast<float>(Exit->GetActorLocation().Z);
		TopZ = Exit->GetClimbTopZ();
		bNetOk = bFree;
		return true;
	});
	LimitLastStep(40.f);
	Add(TEXT("v4.15 correctifs 4 reseau : client a 3 m devant l'echelle"), 0.f, [this, bClient]()
	{
		using namespace BRTestV415Fix;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		ABRExit* Exit = Ladder.Get();
		if (bClient)
		{
			if (!bNetOk || !Exit)
			{
				// Rien a faire ici (deja note) : l'hote n'attend pas la place du client
				bNetOk = false;
				BRTestNet::SendMark(GetPC(), MarkNetNoPlace);
				return true;
			}
			if (Phase != 1)
			{
				// 3 m devant le pied de l'echelle (sinon 2 m), au meme etage, regard vers le point vise par le depart de groupe
				FVector Stand = FVector::ZeroVector;
				bool bStand = false;
				for (const float Ahead : { 300.f, 200.f })
				{
					bStand = bStand || StandIn(W, C, Exit->GetActorLocation() + LadderFront(Exit) * Ahead, Stand);
				}
				if (!bStand)
				{
					Note(TEXT("client : aucune place libre a 2 ou 3 m devant l'echelle BRExit_42"), true);
					bNetOk = false;
					BRTestNet::SendMark(GetPC(), MarkNetNoPlace);
					return true;
				}
				const FVector Look = ABRWorld::DepartureViewPoint(Exit);
				Place.Start(C, Stand, &Look, Now);
				Phase = 1;
				return false;
			}
			if (!Place.Update(C, Now, 0.6) && StepTime < 10.f)
			{
				return false;
			}
			Phase = 0;
			Note(FString::Printf(TEXT("client : place a %.1f m devant le pied de l'echelle, en (%.0f, %.0f, %.0f), %d replacement(s)"),
				FVector::Dist2D(C->GetActorLocation(), Exit->GetActorLocation()) / 100.f, C->GetActorLocation().X, C->GetActorLocation().Y, C->GetActorLocation().Z, Place.Reapplied));
			BRTestNet::SendMark(GetPC(), MarkNetPlaced);
			return true;
		}
		if (!bNetOk || !Exit)
		{
			return true;
		}
		// Hote : la sortie vers le Niveau 9 est gardee par la mission du niveau (resolue ici, une fois : seul le depart est juge),
		// puis il attend que le client soit a sa place
		if (Phase != 1)
		{
			W->DebugCompleteMission();
			Phase = 1;
		}
		const int32 Mark = BRTestNet::RemoteMark(W);
		if (!BRTestNet::RemoteMate(W))
		{
			Note(TEXT("hote : aucun client pour le depart de groupe par l'echelle BRExit_42"), true);
			bNetOk = false;
			Phase = 0;
			return true;
		}
		if (Mark == MarkNetNoPlace)
		{
			Skip(TEXT("hote : le client n'a pas pu se placer devant l'echelle BRExit_42 (voir son rapport) : depart de groupe non verifie"));
			bNetOk = false;
			Phase = 0;
			return true;
		}
		if (Mark != MarkNetPlaced)
		{
			if (StepTime < 60.f)
			{
				return false;
			}
			Note(FString::Printf(TEXT("hote : le client n'a pas signale sa place devant l'echelle BRExit_42 en 60 s (derniere etape signalee %d)"), Mark), true);
			bNetOk = false;
			Phase = 0;
			return true;
		}
		Phase = 0;
		return true;
	});
	LimitLastStep(70.f);
	Add(TEXT("v4.15 correctifs 4 reseau : hote au sommet, rassemblement 2/2 puis depart"), 0.f, [this, bClient]()
	{
		using namespace BRTestV415Fix;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if (!bNetOk)
		{
			return true;
		}
		const double Now = FPlatformTime::Seconds();
		// Releves communs : le depart vers le Niveau 9 (replique chez le client)
		const FBRNetDeparture& Dep = W->GetDeparture();
		const bool bLeft = W->IsTransitioning() || W->GetLevelSerial() != SerialMark;
		if (Dep.Target == LadderTarget && (Dep.Phase == 1 || Dep.Phase == 2))
		{
			bDepStarted = true;
			MaxReady = FMath::Max(MaxReady, Dep.Ready);
			MaxNeeded = FMath::Max(MaxNeeded, Dep.Needed);
			bDepPhase2 = bDepPhase2 || Dep.Phase == 2;
		}
		if (bDepStarted && Dep.Phase == 3 && !bDepCancelled)
		{
			bDepCancelled = true;
			DepReason = Dep.Reason;
		}
		if (bClient)
		{
			if (bLeft)
			{
				// La phase 2 peut ne pas parvenir au client avant le changement de niveau (l'hote part dans la meme image)
				Note(FString::Printf(TEXT("client : depart de groupe par l'echelle BRExit_42 lance (depart vu %s, rassembles au plus %d/%d, phase 2 recue %s)"), Oui(bDepStarted),
					MaxReady, MaxNeeded, Oui(bDepPhase2)), !bDepStarted);
				return true;
			}
			ABRExit* Exit = Ladder.Get();
			if (Exit)
			{
				Place.Update(C, Now, 0.6); // reste a sa place (replace s'il a ete pousse)
			}
			if (bDepCancelled)
			{
				Note(FString::Printf(TEXT("client : depart de groupe par l'echelle BRExit_42 ANNULE (raison %d ; 1 : fin du delai), rassembles au plus %d/%d"), DepReason, MaxReady,
					MaxNeeded), true);
				bNetOk = false;
				return true;
			}
			if (!bDepStarted && StepTime > 60.f)
			{
				Skip(TEXT("client : aucun depart lance par l'hote en 60 s (voir le rapport de l'hote) : depart de groupe par l'echelle BRExit_42 non verifie"));
				bNetOk = false;
				return true;
			}
			if (StepTime > 130.f)
			{
				Note(FString::Printf(TEXT("client : depart de groupe par l'echelle BRExit_42 pas parti en 130 s (phase %d, %d/%d rassembles, %.0f s restantes)"),
					static_cast<int32>(Dep.Phase), Dep.Ready, Dep.Needed, W->GetDepartureRemaining()), true);
				bNetOk = false;
				return true;
			}
			return false;
		}
		ABRExit* Exit = Ladder.Get();
		const float Half = C->GetSimpleCollisionHalfHeight();
		if (!Exit && Phase <= 1)
		{
			Note(TEXT("hote : echelle BRExit_42 disparue avant la montee"), true);
			Phase = 0;
			bNetOk = false;
			return true;
		}
		switch (Phase)
		{
		case 0:
		{
			// Placement direct au pied : cette etape porte sur le depart de groupe (l'approche a pied est jugee en solo)
			PlaceStand(C, Exit->GetClimbAnchor() + LadderFront(Exit) * 90.f + FVector(0.f, 0.f, Half + 5.f), LadderAim(Exit));
			RequestsMark = C->ClimbRequestsSent;
			MaxZ = -1.0e9f;
			Clock = Now;
			Phase = 1;
			return false;
		}
		case 1:
		{
			FaceTo(C, LadderAim(Exit));
			if (Now - Clock < 0.15)
			{
				return false;
			}
			if (C->FocusActor.Get() != Exit)
			{
				if (Now - Clock < 3.0)
				{
					return false;
				}
				Note(FString::Printf(TEXT("hote : echelle BRExit_42 PAS VISEE par l'interaction depuis son pied (vise : %s)"), *BRTestV412::FocusName(W, C->FocusActor.Get())), true);
				Phase = 0;
				bNetOk = false;
				return true;
			}
			C->Interact();
			Clock = Now;
			Phase = 2;
			return false;
		}
		case 2:
		{
			if (bLeft)
			{
				Phase = 3;
				return false;
			}
			MaxZ = FMath::Max(MaxZ, static_cast<float>(C->GetActorLocation().Z));
			if (!C->IsClimbing())
			{
				if (Now - Clock < 1.0)
				{
					return false;
				}
				Note(TEXT("hote : echelle BRExit_42 visee mais PAS PRISE par l'interaction"), true);
				Phase = 0;
				bNetOk = false;
				return true;
			}
			if (C->GetClimbWaitState() == BRGather::FClimbWait::EState::Climbing)
			{
				C->InputMove(FVector2D(0.f, 1.f));
				if (Now - Clock > 20.0)
				{
					Note(FString::Printf(TEXT("hote : sommet de l'echelle BRExit_42 non atteint en 20 s (centre au plus haut a %.0f cm du pied, sommet a %.0f)"), MaxZ - FootZ,
						TopZ - FootZ), true);
					Phase = 0;
					bNetOk = false;
					return true;
				}
				return false;
			}
			// Au sommet : une demande, puis l'attente du groupe
			Clock = Now;
			Phase = 3;
			return false;
		}
		default:
		{
			if (!bLeft && !bDepCancelled && Now - Clock < 100.0)
			{
				return false;
			}
			const int32 Requests = C->ClimbRequestsSent - RequestsMark;
			const bool bOk = bLeft && !bDepCancelled && Requests == 1 && bDepPhase2 && MaxNeeded == 2 && MaxReady == MaxNeeded;
			Note(FString::Printf(TEXT("hote au sommet de l'echelle BRExit_42 : %d demande(s) (attendu 1), centre au plus haut a %.0f cm du pied (sommet a %.0f) ; rassembles au plus "
				"%d/%d (attendu 2/2), depart (phase 2) %s%s"), Requests, MaxZ - FootZ, TopZ - FootZ, MaxReady, MaxNeeded, Oui(bDepPhase2 && bLeft),
				bDepCancelled ? *FString::Printf(TEXT(", ANNULE (raison %d ; 1 : fin du delai)"), DepReason) : (bLeft ? TEXT("") : TEXT(", NON lance en 100 s"))), !bOk);
			Shot(TEXT("v415fix_reseau_sommet_echelle"));
			Phase = 0;
			bNetOk = bLeft;
			return true;
		}
		}
	});
	LimitLastStep(160.f);
	Add(TEXT("v4.15 correctifs 4 reseau : arrivee au Niveau 9 chez chacun"), 1.f, [this, bClient]()
	{
		using namespace BRTestV415Fix;
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (!bNetOk)
		{
			return true; // un seul probleme (note plus haut)
		}
		const BRContent::FExitResolution Res = BRLevels::ResolveExit(8, LadderTarget);
		const bool bThere = !W->IsTransitioning() && W->IsLevelReady() && W->GetLevelNumber() == Res.Target && W->GetLevelSerial() != SerialMark;
		if (!bThere && StepTime < 40.f)
		{
			return false;
		}
		Note(FString::Printf(TEXT("%s : depart de groupe par l'echelle BRExit_42 vers le Niveau %d %s (Niveau %d)"), bClient ? TEXT("client") : TEXT("hote"), Res.Target,
			bThere ? TEXT("effectue") : TEXT("ECHEC"), W->GetLevelNumber()), !bThere);
		return true;
	});
	LimitLastStep(50.f);
	Add(TEXT("v4.15 correctifs 4 reseau : retour a la regle normale"), 0.f, [this, bClient]()
	{
		if (!bClient)
		{
			if (ABRWorld* W = GetBRWorld())
			{
				W->SetLinearTestOverride(-1);
			}
		}
		BRTestV415Fix::Ladder.Reset();
		return true;
	});
}
