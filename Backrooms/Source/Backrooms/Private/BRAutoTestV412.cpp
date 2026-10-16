// v4.12 : etapes du test automatique propres a la v4.12 (-BRAutoTest -BRAutoTestV412 ; reseau : -BRNetTest).
// Chaque etape verifie une regle de la v4.12 dans le jeu reel :
//   - contenu de la version publiee (lot 1 : Niveaux 0, 1, 2, 37) : menu, sorties redirigees, fin du contenu disponible,
//     compatibilite des connexions, reprise d'une partie v4.11 arretee a un niveau pas encore publie ;
//   - inventaire : une seule regle de place (equipement libre compris), objet accepte sans place "mis de cote" puis range,
//     garde dans la sauvegarde ; sante : un envoi parti avant un changement officiel est ignore ;
//   - ACCES PAR DEPLACEMENT REEL (Niveaux 0, 1, 2, 37 dans la version publiee ; Niveaux 6 et 8) : le personnage n'est
//     jamais deplace directement. Un chemin est calcule sur la grille (A* du monde, celui des entites) et suivi avec les
//     seules entrees du joueur (avancer, sauter, pas de cote) ; chaque mecanisme doit etre vise par le trace
//     d'interaction depuis la place atteinte ; la sortie est prise par la touche d'interaction (echelle : on grimpe en
//     avancant) ; l'arrivee (niveau suivant, redirection ou fin du contenu disponible) est verifiee. v4.15 : chemin des
//     tests sans bassin profond (le chemin du monde faute de mieux), places jugees comme le jeu (nage au-dela de 120 cm,
//     visee du client, regle de l'hote), niveau attendu verifie, mort signalee, sante mentale gardee a 100 en marchant ;
//   - Poolrooms : surface des bassins = graduation de l'etat de mission (rendu, regle et eau du personnage lisent la
//     meme valeur) ; sas plein avant, vide apres ; traversee du sas a pied, sans nager ;
//   - Niveau 8 : tablier leve, le palier est hors d'atteinte meme en sautant ; treuils, tablier pose ; traversee a pied ;
//   - Niveau 6 : balises = vraies sources de lumiere de gameplay (une coupure ne les eteint pas) ; mecanismes masques
//     sans Tick ;
//   - reprise : etat de mission d'une autre version (copie de la sauvegarde avant reecriture, avis) ; placement d'une
//     autre version (progression gardee, avis) ; carnet a colonnes defilantes.
// Reseau : compatibilite, retour "en attente / accepte / refuse" d'une demande d'un client, depart de groupe par une
// echelle (une seule demande au sommet, grimpeur compte comme rassemble, coequipier qui arrive a pied).
// Une verification qui ne peut pas etre faite ici est notee "NON VERIFIE", jamais comptee comme reussie.
#include "BRAutoTest.h"
#include "BRAutoTestWalk.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BRHUD.h"
#include "BRInteractables.h"
#include "BRItems.h"
#include "BRLevels.h"
#include "BRLoc.h"
#include "BRMission.h"
#include "BRMissionSolver.h"
#include "BRPlayerController.h"
#include "BRSave.h"
#include "BRStoryLogic.h"
#include "BRWorld.h"
#include "Algo/Reverse.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"

namespace BRM = BRMission;

namespace BRTestV412
{
	const TCHAR* const SavePrefix = TEXT("BR_AutoTestV412_");
	constexpr int32 TestSlot = 0;
	/** Placement retenu pour chaque mission chargee (rapport) */
	TArray<FString> PlacementLines;
	int32 ModulePlacements = 0;
	/** Niveaux traverses par deplacement reel (rapport) */
	TArray<FString> AccessLines;
	/** Inventaire du joueur avant les verifications d'inventaire (retabli) */
	TArray<FBRItemSlot> SavedPockets, SavedStorage, SavedEquipment, SavedRecovered;
	/** Mesures du Niveau 6 */
	int32 LightsBefore = 0;
	float BeaconLightBefore = 0.f;
	/** Fichier de la sauvegarde ecrit avant une reprise (copie attendue a l'identique) */
	TArray<uint8> OriginalBytes;
	/** Mecanisme lu avant la reprise d'un placement d'une autre version */
	int32 RestoredDevice = -1;

	const TCHAR* YesNo(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	FString JoinInts(const TArray<int32>& Values)
	{
		FString Out;
		for (const int32 V : Values)
		{
			Out += (Out.IsEmpty() ? TEXT("") : TEXT(", ")) + FString::FromInt(V);
		}
		return Out;
	}

	void DeleteSaveFiles()
	{
		for (int32 Slot = 0; Slot < BRSaves::MaxSlots; ++Slot)
		{
			for (const FString& Name : { BRSaves::SlotName(Slot), BRSaves::BackupSlotName(Slot), BRSaves::UnreadableSlotName(Slot), BRSaves::RecoverySlotName(Slot) })
			{
				IFileManager::Get().Delete(*BRSaves::FilePath(Name), false, true, true);
			}
		}
	}

	/** Place le joueur (sans vitesse), le regard vers Target : reserve aux verifications qui ne portent pas sur l'acces */
	void PlaceAndFace(ABRCharacter* C, const FVector& Stand, const FVector& Target)
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

	void Face(ABRCharacter* C, const FVector& Target)
	{
		if (AController* Ctrl = C->GetController())
		{
			Ctrl->SetControlRotation((Target - C->GetEyeLocation()).Rotation());
		}
	}

	ABRExit* FindExit(UWorld* World, int32 Target)
	{
		for (TActorIterator<ABRExit> It(World); It; ++It)
		{
			if (It->Target == Target && !It->IsHidden())
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Mecanisme portant un module physique (bassins, sas, passerelle) */
	ABRMissionDevice* FindModule(const ABRWorld* W, uint8 Module)
	{
		for (ABRMissionDevice* D : W->GetMissionDevices())
		{
			if (D && D->GetModule() == Module)
			{
				return D;
			}
		}
		return nullptr;
	}

	/** Sortie de progression ouverte apres la mission (Niveau 0 : celle de la route choisie) ; -1 sinon */
	int32 OpenForwardTarget(const ABRWorld* W, int32 Level)
	{
		for (int32 Route = 0; Route < 2; ++Route)
		{
			const int32 T = BRLevels::AdaptedForward(Level, BRM::ForwardTarget(Level, Route));
			FString Why;
			if (T >= 0 && W->CanUseExit(T, Why))
			{
				return T;
			}
		}
		return -1;
	}

	// =================================================================================================================
	// Marche reelle : chemin sur la grille, puis seulement les entrees du joueur
	// =================================================================================================================
	FWalk Walk;

	/** v4.15 : marche en cours d'une mission a pied : ordinaire, approche (chunk du mecanisme pas encore construit), reprise */
	enum class EWalkKind : uint8 { Normal, Approach, Retry };

	/** v4.15 : etat de la derniere mission a pied (notes, etapes de non-regression) */
	struct FWalkMission
	{
		int32 Level = -1;
		uint32 LevelSeed = 0;
		/** Niveau attendu absent au depart (cascade) : mission et sortie non testees */
		bool bNotReached = false;
		/** Toutes les actions du solveur faites */
		bool bComplete = false;
		int32 Planned = 0;
		int32 Actions = 0;
		int32 Walks = 0;
		int32 Approaches = 0;
		int32 Retries = 0;
		/** Images a la nage et chemins de grille passant par un bassin, toutes marches de la mission comprises */
		int32 SwimFrames = 0;
		int32 PoolFallbacks = 0;
		EWalkKind Kind = EWalkKind::Normal;
		/** Mecanisme deja approche : sa place est choisie dans le decor construit */
		int32 Approached = -1;
		/** But de la premiere marche vers le mecanisme en cours (repli de la marche de reprise) */
		FVector FirstGoal = FVector::ZeroVector;
		/** Par mecanisme : action faite, eau sous le joueur a ce moment (surface - fond, comme ABRCharacter::UpdateWater), nage */
		bool ActDone[BRM::MaxDevices] = {};
		float ActDepth[BRM::MaxDevices] = {};
		bool ActSwim[BRM::MaxDevices] = {};
	};
	FWalkMission WalkMission;

	/** v4.15 : derniere sortie prise a pied (etapes de non-regression) */
	struct FWalkExit
	{
		int32 Level = -1;
		bool bReached = false;
		bool bArrived = false;
		int32 SwimFrames = 0;
		int32 LocalSwimFrames = 0;
		float LocalMaxDepth = 0.f;
	};
	FWalkExit LastExit;

	/** v4.15 : graine des Poolrooms dont la vanne B ne s'actionne que depuis l'eau des canaux (niveau 125584919, diag_v413_3) */
	constexpr uint32 ValveSeed = 1217121623u;

	bool FindPoolFreePath(const ABRWorld* W, const FIntPoint& From, const FIntPoint& To, TArray<FIntPoint>& OutPath, int32 MaxNodes)
	{
		OutPath.Reset();
		if (From == To)
		{
			OutPath.Add(To);
			return true;
		}
		if (!W->IsWalkable(To))
		{
			return false;
		}
		// Copie de l'A* du monde (ABRWorld::FindPath) : memes noeuds, couts et ordre ; seul le filtre des bassins s'y ajoute
		struct FNode
		{
			FIntPoint Cell;
			float F;
		};
		auto ByCost = [](const FNode& A, const FNode& B) { return A.F < B.F; };
		auto Estimate = [&To](const FIntPoint& Q) { return static_cast<float>(FMath::Abs(Q.X - To.X) + FMath::Abs(Q.Y - To.Y)); };
		TArray<FNode> OpenSet;
		TMap<FIntPoint, FIntPoint> CameFrom;
		TMap<FIntPoint, float> PathCost;
		OpenSet.HeapPush(FNode{ From, Estimate(From) }, ByCost);
		PathCost.Add(From, 0.f);
		const FIntPoint Steps4[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		const bool bPits = W->HasPits();
		int32 NumExpanded = 0;
		while (OpenSet.Num() > 0 && NumExpanded < MaxNodes)
		{
			FNode Best{ FIntPoint(0, 0), 0.f };
			OpenSet.HeapPop(Best, ByCost);
			if (Best.Cell == To)
			{
				FIntPoint Walker = To;
				OutPath.Add(Walker);
				while (Walker != From)
				{
					Walker = CameFrom.FindChecked(Walker);
					OutPath.Add(Walker);
				}
				Algo::Reverse(OutPath);
				return true;
			}
			++NumExpanded;
			const float CurCost = PathCost.FindRef(Best.Cell);
			for (const FIntPoint& Dir : Steps4)
			{
				const FIntPoint N = Best.Cell + Dir;
				// Un bassin profond (Niveau 37) n'est traverse que s'il est l'arrivee
				if (!W->CanStep(Best.Cell, N) || (N != To && W->IsPoolCell(N.X, N.Y)))
				{
					continue;
				}
				const float NewCost = CurCost + ((bPits && W->IsPitRoomCell(N.X, N.Y)) ? 3.f : 1.f);
				if (const float* Known = PathCost.Find(N))
				{
					if (*Known <= NewCost)
					{
						continue;
					}
				}
				PathCost.Add(N, NewCost);
				CameFrom.Add(N, Best.Cell);
				OpenSet.HeapPush(FNode{ N, NewCost + Estimate(N) }, ByCost);
			}
		}
		return false;
	}

	bool Replan(const ABRWorld* W, const ABRCharacter* C)
	{
		Walk.Points.Reset();
		Walk.Next = 0;
		TArray<FIntPoint> Path;
		const FIntPoint From = W->WorldToCell(C->GetActorLocation());
		const FIntPoint To = W->WorldToCell(Walk.Goal);
		// v4.15 : sans bassin profond d'abord (le chemin du monde peut en traverser un : le marcheur y nageait sans fin) ; le
		// chemin du monde (ABRWorld::FindPath, inchange) seulement faute de mieux
		Walk.bPoolFallback = false;
		if (!FindPoolFreePath(W, From, To, Path, 20000))
		{
			if (!W->FindPath(From, To, Path, 20000))
			{
				Walk.Points.Add(Walk.Goal);
				return false;
			}
			Walk.bPoolFallback = true;
		}
		for (int32 I = 1; I < Path.Num(); ++I)
		{
			Walk.Points.Add(W->CellCenter(Path[I], static_cast<float>(Walk.Goal.Z)));
		}
		Walk.Points.Add(Walk.Goal);
		return true;
	}

	bool BeginWalk(const ABRWorld* W, const ABRCharacter* C, const FVector& Goal, float Radius, bool bDetour, bool bCheckZ, float GoalFeetZ)
	{
		Walk = FWalk();
		Walk.Goal = Goal;
		Walk.Radius = Radius;
		Walk.bDetour = bDetour;
		Walk.bCheckZ = bCheckZ;
		Walk.GoalFeetZ = GoalFeetZ;
		Walk.Start = Walk.CheckAt = FPlatformTime::Seconds();
		Walk.CheckPos = Walk.LastPos = C->GetActorLocation();
		Walk.bPath = Replan(W, C);
		return Walk.bPath;
	}

	void ReleaseJump(ABRCharacter* C)
	{
		if (Walk.bJumpHeld)
		{
			C->InputJump(false);
			Walk.bJumpHeld = false;
		}
	}

	FString WalkStats()
	{
		return FString::Printf(TEXT("%.1f m en %.0f s, %d saut(s), %d nouveau(x) chemin(s), %d image(s) a la nage, %d deplacement(s) direct(s)%s%s"),
			Walk.Travelled / 100.f, FPlatformTime::Seconds() - Walk.Start, Walk.Jumps, Walk.Replans, Walk.SwimFrames, Walk.Teleports,
			Walk.bPath ? TEXT("") : TEXT(", sans chemin de grille (tout droit)"), Walk.bPoolFallback ? TEXT(", chemin du monde par un bassin (aucun chemin sans bassin)") : TEXT(""));
	}

	/** Une image de marche. bBusy : frayeur, entree bloquee, chargement (le temps ne compte pas pour le blocage) */
	EWalk StepWalk(const ABRWorld* W, ABRCharacter* C, float Dt, double Timeout, bool bBusy)
	{
		const double Now = FPlatformTime::Seconds();
		// v4.15 : la folie ignore le mode invincible (regle du jeu, inchangee) : dans le noir, lampe eteinte, elle tuait le marcheur
		// apres 260 s environ (Level 6, Niveau 8). Le test garde sa sante mentale a 100 pendant la marche
		C->Sanity = 100.f;
		const FVector Pos = C->GetActorLocation();
		const float FeetZ = static_cast<float>(Pos.Z) - C->GetSimpleCollisionHalfHeight();
		const float Moved = static_cast<float>(FVector::Dist(Pos, Walk.LastPos));
		Walk.Teleports += Moved > 300.f ? 1 : 0;
		Walk.Travelled += Moved;
		Walk.LastPos = Pos;
		++Walk.Frames;
		Walk.SwimFrames += C->IsSwimming() ? 1 : 0;
		Walk.MaxFeetZ = FMath::Max(Walk.MaxFeetZ, FeetZ);
		const BRMech::FWaterQuery Q = W->WaterAt(Pos);
		if (Q.bLocal)
		{
			++Walk.LocalFrames;
			Walk.LocalSwimFrames += C->IsSwimming() ? 1 : 0;
			if (Q.bWater)
			{
				Walk.LocalMaxDepth = FMath::Max(Walk.LocalMaxDepth, Q.Surface - FeetZ);
			}
		}
		if (FVector::Dist2D(Pos, Walk.Goal) <= Walk.Radius && (!Walk.bCheckZ || FMath::Abs(FeetZ - Walk.GoalFeetZ) < 40.f))
		{
			ReleaseJump(C);
			return EWalk::Arrived;
		}
		if (bBusy)
		{
			Walk.CheckAt = Now;
			Walk.CheckPos = Pos;
			return Now - Walk.Start > Timeout ? EWalk::Stuck : EWalk::Moving;
		}
		while (Walk.Next < Walk.Points.Num() - 1 && FVector::Dist2D(Pos, Walk.Points[Walk.Next]) < 70.f)
		{
			++Walk.Next;
		}
		FVector Target = Walk.Points.IsValidIndex(Walk.Next) ? Walk.Points[Walk.Next] : Walk.Goal;
		FVector Detour;
		if (Walk.bDetour && W->MissionNavDetour(Pos, Walk.Goal, Detour))
		{
			Target = Detour;
		}
		// Progression : moins de 40 cm (ni 30 cm de montee) en 1,2 s -> saut, puis pas de cote, puis nouveau chemin
		if (Now - Walk.CheckAt > 1.2)
		{
			const bool bProgress = FVector::Dist2D(Pos, Walk.CheckPos) >= 40.f || FMath::Abs(Pos.Z - Walk.CheckPos.Z) >= 30.f;
			Walk.CheckAt = Now;
			Walk.CheckPos = Pos;
			if (bProgress)
			{
				Walk.StuckChecks = 0;
			}
			else
			{
				++Walk.StuckChecks;
				if (Walk.StuckChecks % 2 == 1)
				{
					Walk.bWantJump = true;
				}
				else
				{
					Walk.SideLeft = 0.6f;
					Walk.SideSign = -Walk.SideSign;
				}
				if (Walk.StuckChecks == 4)
				{
					Replan(W, C);
					++Walk.Replans;
				}
			}
		}
		if (Walk.StuckChecks >= 8 || Now - Walk.Start > Timeout)
		{
			ReleaseJump(C);
			return EWalk::Stuck;
		}
		// Regard vers le point suivant (a la nage : vers sa hauteur), puis les entrees du joueur
		if (AController* Ctrl = C->GetController())
		{
			const FRotator Look = (Target - Pos).Rotation();
			FRotator R = Ctrl->GetControlRotation();
			R.Yaw = Look.Yaw;
			R.Pitch = C->IsSwimming() ? FMath::Clamp(static_cast<float>(Look.Pitch), -35.f, 35.f) : 0.f;
			R.Roll = 0.f;
			Ctrl->SetControlRotation(R);
		}
		if (Walk.bJumpHeld)
		{
			ReleaseJump(C);
		}
		else if (Walk.bWantJump)
		{
			C->InputJump(true);
			Walk.bJumpHeld = true;
			Walk.bWantJump = false;
			++Walk.Jumps;
		}
		if (Walk.SideLeft > 0.f)
		{
			Walk.SideLeft -= Dt;
			C->InputMove(FVector2D(Walk.SideSign, 0.3f));
		}
		else
		{
			C->InputMove(FVector2D(0.f, 1.f));
		}
		return EWalk::Moving;
	}

	float EyeHeight(const ABRCharacter* C)
	{
		return C ? static_cast<float>(C->GetEyeLocation().Z - C->GetActorLocation().Z) : 74.f;
	}

	const TCHAR* StandName(EStand S)
	{
		switch (S)
		{
		case EStand::Ok: return TEXT("libre");
		case EStand::NoFloor: return TEXT("sans sol");
		case EStand::Swim: return TEXT("a la nage");
		case EStand::Blocked: return TEXT("capsule genee");
		case EStand::TooFar: return TEXT("hors de portee de la visee");
		case EStand::NoSight: return TEXT("refusee par l'hote (vue coupee)");
		}
		return TEXT("?");
	}

	bool FloorUnder(const ABRWorld* W, const FVector& P, float FromZ, const AActor* Self, float& OutFloorZ)
	{
		// v4.15 : n'ignore que le joueur (avant : aussi le mecanisme vise, dont les volumes, cuves des vannes ou cage, etaient
		// traverses par la place choisie)
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestFloor), false, Self);
		FHitResult Floor;
		if (!W->GetWorld()->LineTraceSingleByChannel(Floor, FVector(P.X, P.Y, FromZ + 60.f), FVector(P.X, P.Y, FromZ - 450.f), ECC_WorldStatic, Q))
		{
			return false;
		}
		OutFloorZ = static_cast<float>(Floor.ImpactPoint.Z);
		return true;
	}

	bool IsSwimSpot(const ABRWorld* W, const FVector& P, float FloorZ)
	{
		// v4.15 : la regle de nage du jeu (avant : plus de 10 cm d'eau, alors qu'on marche dans les canaux de 60 cm) ; l'eau d'un
		// bassin ou du sas (volume local) n'est jamais une place
		const BRMech::FWaterQuery WQ = W->WaterAt(FVector(P.X, P.Y, FloorZ + 5.f));
		return WQ.bWater && (WQ.bLocal || WQ.Surface - WQ.Floor > BRMech::SwimDepth);
	}

	EStand CheckMissionStand(const ABRWorld* W, const ABRCharacter* C, const ABRMissionDevice* Dev, const FVector& P, bool bNoSwim, FVector& OutStand)
	{
		UWorld* World = W ? W->GetWorld() : nullptr;
		if (!World || !C || !Dev)
		{
			return EStand::NoFloor;
		}
		const FVector Aim = Dev->GetInteractPoint();
		float FloorZ = 0.f;
		if (!FloorUnder(W, P, static_cast<float>(Aim.Z), C, FloorZ))
		{
			return EStand::NoFloor;
		}
		if (bNoSwim && IsSwimSpot(W, P, FloorZ))
		{
			return EStand::Swim;
		}
		// Capsule du joueur (rayon 34 cm, marge 2 cm) posee 5 cm au-dessus du sol ; seul le joueur est ignore
		const float Half = C->GetSimpleCollisionHalfHeight();
		const FVector Stand(P.X, P.Y, FloorZ + Half + 5.f);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestStand), false, C);
		if (World->OverlapAnyTestByChannel(Stand, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(36.f, Half - 4.f), Q))
		{
			return EStand::Blocked;
		}
		// Yeux du joueur debout sur ce sol ; la visee du client d'abord, puis la regle de l'hote (la seule, partagee)
		const FVector Eye(P.X, P.Y, FloorZ + Half + EyeHeight(C));
		if (FVector::Dist(Eye, Aim) > ClientFocusReach)
		{
			return EStand::TooFar;
		}
		if (!W->CanReachMissionDevice(Eye, Dev, C))
		{
			return EStand::NoSight;
		}
		OutStand = Stand;
		return EStand::Ok;
	}

	bool FindMissionStand(const ABRWorld* W, const ABRCharacter* C, const ABRMissionDevice* Dev, bool bNoSwim, FVector& OutStand, FString* OutWhy)
	{
		if (!W || !C || !Dev)
		{
			return false;
		}
		const FVector Aim = Dev->GetInteractPoint();
		const FVector Ahead = Dev->GetActorForwardVector().GetSafeNormal2D();
		const FVector Fwd = Ahead.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Ahead;
		// Cercles : 110 cm (place ordinaire), plus loin, puis tout pres (place etroite) ; 16 directions depuis l'avant
		int32 Refused[6] = { 0, 0, 0, 0, 0, 0 };
		int32 Tried = 0;
		for (const float Dist : { 110.f, 160.f, 210.f, 75.f, 240.f })
		{
			for (int32 Turn = 0; Turn < 16; ++Turn)
			{
				const FVector P = Aim + Fwd.RotateAngleAxis(Turn * 22.5f, FVector::UpVector) * Dist;
				const EStand S = CheckMissionStand(W, C, Dev, P, bNoSwim, OutStand);
				if (S == EStand::Ok)
				{
					return true;
				}
				++Refused[static_cast<int32>(S)];
				++Tried;
			}
		}
		if (OutWhy)
		{
			*OutWhy = FString::Printf(TEXT("%d places essayees : %d sans sol, %d a la nage, %d capsule genee, %d hors de portee de la visee (%.0f cm), %d refusees par l'hote"), Tried,
				Refused[static_cast<int32>(EStand::NoFloor)], Refused[static_cast<int32>(EStand::Swim)], Refused[static_cast<int32>(EStand::Blocked)],
				Refused[static_cast<int32>(EStand::TooFar)], ClientFocusReach, Refused[static_cast<int32>(EStand::NoSight)]);
		}
		return false;
	}

	/** v4.15 : yeux du joueur au point d'interaction, et ce que la regle de l'hote en dit depuis la place actuelle */
	FString ReachFromHere(const ABRWorld* W, const ABRCharacter* C, const ABRMissionDevice* Dev)
	{
		FHitResult Block;
		const bool bServer = W->CanReachMissionDevice(C->GetEyeLocation(), Dev, C, &Block);
		return FString::Printf(TEXT("yeux a %.0f cm du point, regle de l'hote : %s"), FVector::Dist(C->GetEyeLocation(), Dev->GetInteractPoint()),
			bServer ? TEXT("action acceptee") : *FString::Printf(TEXT("REFUSEE (%s, %s)"), *GetNameSafe(Block.GetActor()), *GetNameSafe(Block.GetComponent())));
	}

	/** Point au centre d'un volume local (repere d'un module) */
	FVector BoxCenter(const BRMech::FFrame& F, float LX0, float LY0, float LX1, float LY1, float Z)
	{
		float X = 0.f, Y = 0.f;
		F.ToWorld((LX0 + LX1) * 0.5f, (LY0 + LY1) * 0.5f, X, Y);
		return FVector(X, Y, Z);
	}
}

// =====================================================================================================================
// Acces par deplacement reel : mission resolue en marchant, puis sortie prise a pied
// =====================================================================================================================

void ABRAutoTest::AddWalkMissionSteps(int32 Level, uint32 UserSeed, bool bLoad)
{
	using namespace BRTestV412;
	if (bLoad)
	{
		AddSeedLoad(Level, UserSeed, FString::Printf(TEXT("v4.12 acces par deplacement reel : Niveau %d (graine %u)"), Level, UserSeed));
	}
	Add(FString::Printf(TEXT("v4.12 acces %d : depart"), Level), 0.5f, [this, Level, UserSeed]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if ((W->IsTransitioning() || !W->IsLevelReady()) && StepTime < 30.f)
		{
			return false;
		}
		TestMission.Reset();
		TestMissionStep = 0;
		TestMissionPhase = 0;
		TestMissionRetries = 0;
		TestMissionDevice = -1;
		TestMissionLastHold = 0.0;
		TestMissionClock = 0.0;
		WalkMission = FWalkMission();
		WalkMission.Level = Level;
		WalkMission.LevelSeed = W->GetSeed();
		// v4.15 : en cascade (sortie du niveau precedent pas prise, mort), le niveau attendu n'est pas la : rien n'est teste (avant :
		// la mission du niveau courant etait rejouee sous le nom de celui-ci)
		if (W->GetLevelNumber() != Level)
		{
			WalkMission.bNotReached = true;
			Skip(FString::Printf(TEXT("Niveau %d non atteint (niveau actuel %d) : mission non testee"), Level, W->GetLevelNumber()));
			return true;
		}
		if (!W->IsMissionActive() && StepTime < 10.f)
		{
			return false;
		}
		const uint8 Tier = W->GetMissionTier();
		ModulePlacements += Tier == 2 ? 1 : 0;
		PlacementLines.Add(FString::Printf(TEXT("Niveau %2d, graine %u (niveau %u) : placement %s, avis %d, mission %s"), Level, UserSeed, W->GetSeed(),
			Tier == 0 ? TEXT("normal") : (Tier == 1 ? TEXT("variante de secours") : TEXT("MODULE DE SECOURS")), W->GetMissionNotice(),
			W->IsMissionActive() ? TEXT("active") : TEXT("ABSENTE")));
		// Sorties de ce niveau dans cette version : passages condamnes, voyants des sorties gardees (rouge = 0 avant la mission)
		int32 Sealed = 0, Red = 0, Green = 0;
		for (TActorIterator<ABRExit> It(GetWorld()); It; ++It)
		{
			Sealed += It->IsSealed() ? 1 : 0;
			Red += It->GetMissionOpen() == 0 ? 1 : 0;
			Green += It->GetMissionOpen() == 1 ? 1 : 0;
		}
		// v4.13 : le Level 0 n'a pas de mission (sa porte de service est libre) ; les autres niveaux en ont une
		const bool bWantMission = Level != 0;
		Note(FString::Printf(TEXT("Niveau %d (canal %d, graine du niveau %u) : mission %s, placement %d, passages condamnes %d, voyants rouges %d, verts %d"), Level,
			static_cast<int32>(BRLevels::Channel()), W->GetSeed(), W->IsMissionActive() ? TEXT("active") : (bWantMission ? TEXT("ABSENTE") : TEXT("aucune (Level 0, v4.13)")), Tier,
			Sealed, Red, Green), W->IsMissionActive() != bWantMission);
		Shot(FString::Printf(TEXT("v412_acces_%d_depart"), Level));
		return true;
	});

	Add(FString::Printf(TEXT("v4.12 acces %d : mission resolue en marchant"), Level), 0.f, [this, Level]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (WalkMission.Level == Level && WalkMission.bNotReached)
		{
			return true; // niveau non atteint : NON VERIFIE au depart
		}
		if (!W || !C)
		{
			if (StepTime > 30.f)
			{
				Note(FString::Printf(TEXT("Niveau %d : monde ou joueur absent pendant la mission a pied"), Level), true);
				return true;
			}
			return false;
		}
		// v4.15 : mort ou niveau quitte pendant la mission : signale ici (avant : fin silencieuse, "deja note", et la sortie etait
		// cherchee au Level 0)
		if (C->IsDead() || W->GetLevelNumber() != Level || W->IsTransitioning())
		{
			ReleaseJump(C);
			const FString What = C->IsDead() ? FString::Printf(TEXT("MORT (cause %s, sante mentale %.0f, sante %.0f)"), BRDeath::CauseId(C->GetDeathCause()), C->Sanity, C->Health)
				: FString::Printf(TEXT("NIVEAU QUITTE (niveau %d%s)"), W->GetLevelNumber(), W->IsTransitioning() ? TEXT(", changement de niveau en cours") : TEXT(""));
			const FIntPoint Cell = W->WorldToCell(C->GetActorLocation());
			Note(FString::Printf(TEXT("Niveau %d (graine du niveau %u) : %s pendant la mission a pied, action %d/%d, mecanisme %d, cellule (%d, %d)%s"), Level, WalkMission.LevelSeed,
				*What, TestMissionStep + 1, TestMission.Num() / 3, TestMissionDevice, Cell.X, Cell.Y, TestMissionPhase == 2 ? *(FString(TEXT(" ; marche en cours")) + Walk.Tag + TEXT(" : ")
				+ WalkStats()) : TEXT("")), true);
			Shot(FString::Printf(TEXT("v412_acces_%d_interrompu"), Level));
			return true;
		}
		if (!W->IsMissionActive())
		{
			return true; // Level 0 : aucune mission (notee au depart)
		}
		const BRM::FPlan& P = W->GetMissionPlan();
		const double Now = FPlatformTime::Seconds();
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		const bool bBusy = C->GetScareKind() >= 0 || C->IsInputLocked();
		if (StepTime > 900.f)
		{
			ReleaseJump(C);
			Note(FString::Printf(TEXT("Niveau %d : mission interrompue apres 900 s (action %d/%d, graine du niveau %u)"), Level, TestMissionStep, TestMission.Num() / 3,
				WalkMission.LevelSeed), true);
			return true;
		}
		switch (TestMissionPhase)
		{
		case 0:
		{
			BRM::FState S = W->GetMissionState();
			BRM::FSolver Solver(P, S, W->GetMissionCampaign());
			Solver.bPlayerActions = true;
			Solver.bRecord = true;
			if (!Solver.Solve(0) || Solver.bBad || Solver.bOverflow || !(S.Solved & 1))
			{
				const FString WhyText = Solver.bBad ? FString(ANSI_TO_TCHAR(Solver.Why)) : FString(TEXT("bloque"));
				Note(FString::Printf(TEXT("Niveau %d : le solveur n'a pas trouve de solution (%s)"), Level, *WhyText), true);
				return true;
			}
			for (int32 I = 0; I < Solver.NumSteps; ++I)
			{
				TestMission.Add(Solver.Steps[I].Device);
				TestMission.Add(Solver.Steps[I].Action);
				TestMission.Add(Solver.Steps[I].Feedback);
			}
			WalkMission.Planned = Solver.NumSteps;
			TestCountA = 0; // metres parcourus pour la mission
			TestCountB = 0; // nouvelles marches apres un refus "trop loin" pour l'action en cours
			TestTimer = Now;
			TestMissionPhase = 1;
			return false;
		}
		case 1:
		{
			// Mecanisme suivant : deja vise depuis la place actuelle (meme mecanisme), sinon on y marche
			if (TestMissionStep * 3 >= TestMission.Num())
			{
				WalkMission.bComplete = true;
				Note(FString::Printf(TEXT("Niveau %d : %d actions faites en marchant (%d m, %.0f s ; %d marche(s) dont %d approche(s) et %d reprise(s), %d image(s) a la nage, "
					"%d chemin(s) par un bassin ; graine du niveau %u)"), Level, TestMission.Num() / 3, TestCountA, Now - TestTimer, WalkMission.Walks, WalkMission.Approaches,
					WalkMission.Retries, WalkMission.SwimFrames, WalkMission.PoolFallbacks, WalkMission.LevelSeed));
				return true;
			}
			const int32 D = TestMission[TestMissionStep * 3];
			ABRMissionDevice* Dev = W->FindMissionDevice(D);
			if (!Dev)
			{
				Note(FString::Printf(TEXT("Niveau %d : mecanisme %d absent du monde"), Level, D), true);
				return true;
			}
			if (D == TestMissionDevice && C->FocusActor.Get() == Dev)
			{
				TestMissionPhase = 4;
				return false;
			}
			TestMissionDevice = D;
			TestMissionRetries = 0;
			// v4.15 : chunk du mecanisme pas encore construit (loin du joueur) : marche vers sa cellule d'abord ; sa place est
			// choisie a l'arrivee, dans le decor construit (avant : sans sol sous la place, le mecanisme etait juge sans place)
			if (!Dev->IsShown() && WalkMission.Approached != D)
			{
				BeginWalk(W, C, W->CellCenter(Dev->GetCell(), static_cast<float>(C->GetActorLocation().Z)), 200.f, true);
				Walk.Tag = TEXT(" (approche : chunk du mecanisme pas encore construit)");
				WalkMission.Kind = EWalkKind::Approach;
				TestMissionPhase = 2;
				return false;
			}
			if ((!Dev->IsShown() || W->GetPreparingChunkCount() > 0) && Now - TestMissionClock < 10.0)
			{
				return false; // approche faite : chunks en construction
			}
			// v4.15 : la place devant le mecanisme (110 cm) jugee comme le jeu (sol, capsule, pas a la nage, visee du client, regle de
			// l'hote), sinon la premiere place valable autour de lui
			const FVector Aim = Dev->GetInteractPoint();
			const FVector Ahead = Dev->GetActorForwardVector().GetSafeNormal2D();
			const FVector Fwd = Ahead.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Ahead;
			FVector Goal = FVector::ZeroVector;
			const EStand Front = CheckMissionStand(W, C, Dev, Aim + Fwd * 110.f, true, Goal);
			if (Front != EStand::Ok)
			{
				FString Why;
				if (!FindMissionStand(W, C, Dev, true, Goal, &Why))
				{
					Note(FString::Printf(TEXT("Niveau %d : mecanisme %d (%s) : aucune place sans nager a portee et a vue (graine du niveau %u ; devant lui a 110 cm : %s ; "
						"autour : %s)"), Level, D, *BRMissionText::DeviceName(P, D), WalkMission.LevelSeed, StandName(Front), *Why), true);
					Shot(FString::Printf(TEXT("v412_acces_%d_sans_place"), Level));
					return true;
				}
			}
			WalkMission.FirstGoal = Goal;
			BeginWalk(W, C, FVector(Goal.X, Goal.Y, C->GetActorLocation().Z), 70.f, true);
			Walk.Tag = Front == EStand::Ok ? FString() : FString::Printf(TEXT(" (place autour du mecanisme : devant lui, %s)"), StandName(Front));
			WalkMission.Kind = EWalkKind::Normal;
			TestMissionPhase = 2;
			return false;
		}
		case 2:
		{
			// v4.15 : une approche s'arrete des que le mecanisme est affiche (son chunk est construit)
			const ABRMissionDevice* Near = W->FindMissionDevice(TestMissionDevice);
			const bool bShownNow = WalkMission.Kind == EWalkKind::Approach && Near && Near->IsShown() && Now - Walk.Start > 0.5;
			const EWalk R = bShownNow ? EWalk::Arrived : StepWalk(W, C, Dt, 240.0, bBusy);
			if (R == EWalk::Moving)
			{
				return false;
			}
			ReleaseJump(C);
			TestCountA += FMath::RoundToInt(Walk.Travelled / 100.f);
			++WalkMission.Walks;
			WalkMission.SwimFrames += Walk.SwimFrames;
			WalkMission.PoolFallbacks += Walk.bPoolFallback ? 1 : 0;
			if (R == EWalk::Stuck)
			{
				Note(FString::Printf(TEXT("Niveau %d : mecanisme %d (%s) NON ATTEINT a pied%s : bloque a la cellule (%d, %d), %s (graine du niveau %u)"), Level, TestMissionDevice,
					*BRMissionText::DeviceName(P, TestMissionDevice), *Walk.Tag, W->WorldToCell(C->GetActorLocation()).X, W->WorldToCell(C->GetActorLocation()).Y, *WalkStats(),
					WalkMission.LevelSeed), true);
				Shot(FString::Printf(TEXT("v412_acces_%d_bloque"), Level));
				return true;
			}
			// v4.15 : chaque marche dans le rapport (nage comprise)
			Note(FString::Printf(TEXT("Niveau %d : marche vers le mecanisme %d (%s)%s : %s"), Level, TestMissionDevice, *BRMissionText::DeviceName(P, TestMissionDevice), *Walk.Tag,
				*WalkStats()));
			TestMissionClock = Now;
			if (WalkMission.Kind == EWalkKind::Approach)
			{
				WalkMission.Approached = TestMissionDevice;
				++WalkMission.Approaches;
				TestMissionPhase = 1;
				return false;
			}
			TestMissionPhase = 3;
			return false;
		}
		case 3:
		{
			// Le trace d'interaction (le meme que la touche) doit trouver le mecanisme depuis la place atteinte
			ABRMissionDevice* Dev = W->FindMissionDevice(TestMissionDevice);
			if (!Dev)
			{
				Note(FString::Printf(TEXT("Niveau %d : mecanisme %d absent du monde"), Level, TestMissionDevice), true);
				return true;
			}
			Face(C, Dev->GetInteractPoint());
			if (Now - TestMissionClock < 0.15)
			{
				return false;
			}
			if (C->FocusActor.Get() == Dev)
			{
				TestMissionClock = Now;
				TestMissionPhase = 4;
				return false;
			}
			if (Now - TestMissionClock < 1.5)
			{
				return false;
			}
			if (TestMissionRetries++ == 0)
			{
				// Un pas plus pres, puis on vise a nouveau. v4.15 : le but a 60 cm est verifie comme une place (sol, capsule libre, pas a
				// la nage, portee et vue) ; sinon le but de la premiere marche, atteint de plus pres (avant : un but a 60 cm dans la
				// cage du Level 6 n'etait jamais atteint, 240 s de marche). La marche de reprise est nommee dans les notes
				const FVector Aim = Dev->GetInteractPoint();
				const FVector Ahead = Dev->GetActorForwardVector().GetSafeNormal2D();
				const FVector Fwd = Ahead.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Ahead;
				FVector Closer = FVector::ZeroVector;
				const EStand Near = CheckMissionStand(W, C, Dev, Aim + Fwd * 60.f, true, Closer);
				const FVector Goal = Near == EStand::Ok ? Closer : WalkMission.FirstGoal;
				BeginWalk(W, C, FVector(Goal.X, Goal.Y, C->GetActorLocation().Z), 35.f, true);
				Walk.Tag = Near == EStand::Ok ? FString(TEXT(" (marche de reprise : but a 60 cm)"))
					: FString::Printf(TEXT(" (marche de reprise : but a 60 cm refuse, %s : but de la premiere marche)"), StandName(Near));
				WalkMission.Kind = EWalkKind::Retry;
				++WalkMission.Retries;
				TestMissionPhase = 2;
				return false;
			}
			const AActor* Seen = C->FocusActor.Get();
			Note(FString::Printf(TEXT("Niveau %d : mecanisme %d (%s) atteint mais PAS VISE par l'interaction (vise : %s ; %s ; apres la marche de reprise ; graine du niveau %u)"),
				Level, TestMissionDevice, *BRMissionText::DeviceName(P, TestMissionDevice), Seen ? *Seen->GetName() : TEXT("rien"), *ReachFromHere(W, C, Dev), WalkMission.LevelSeed),
				true);
			Shot(FString::Printf(TEXT("v412_acces_%d_pas_vise"), Level));
			return true;
		}
		case 4:
		{
			// Action : la meme demande que la touche d'interaction, mecanisme vise
			const int32 D = TestMission[TestMissionStep * 3];
			const uint8 Action = TestMission[TestMissionStep * 3 + 1];
			const uint8 Expected = TestMission[TestMissionStep * 3 + 2];
			const bool bHold = P.Devices[D].Kind == BRM::EKind::Observe || P.Devices[D].Kind == BRM::EKind::Crank;
			ABRMissionDevice* Dev = W->FindMissionDevice(D);
			if (Dev)
			{
				Face(C, Dev->GetInteractPoint());
			}
			if ((bHold && Now - TestMissionLastHold < 0.45) || Now - TestMissionClock < 0.05)
			{
				return false;
			}
			const int32 Before = C->MissionResultsReceived;
			C->RequestMissionAction(D, Action);
			if (bHold)
			{
				TestMissionLastHold = Now;
			}
			TestMissionClock = Now;
			if (C->MissionResultsReceived == Before)
			{
				Note(FString::Printf(TEXT("Niveau %d : action %d sans reponse"), Level, TestMissionStep), true);
				return true;
			}
			const uint8 Got = C->LastMissionFeedback;
			const bool bItemTaken = P.Devices[D].Kind == BRM::EKind::Item && Expected == static_cast<uint8>(BRM::EFeedback::AlreadyDone) && Got == BRMissionText::Taken;
			if (Got == BRMissionText::Cooldown || Got == BRMissionText::NotNow || Got == BRMissionText::TooFar)
			{
				// v4.15 : un refus "trop loin" relance la marche ; au-dela de trois, la place n'est pas bonne : signale (avant : le
				// compteur des refus etait remis a zero a chaque marche)
				TestCountB += Got == BRMissionText::TooFar ? 1 : 0;
				if (++TestMissionRetries > 8 || TestCountB > 3)
				{
					Note(FString::Printf(TEXT("Niveau %d : action %d refusee %d fois (retour %d, %d nouvelle(s) marche(s) apres \"trop loin\" ; %s ; graine du niveau %u)"), Level,
						TestMissionStep, TestMissionRetries, Got, TestCountB, Dev ? *ReachFromHere(W, C, Dev) : TEXT("mecanisme absent"), WalkMission.LevelSeed), true);
					return true;
				}
				TestMissionClock = Now + (Got == BRMissionText::Cooldown ? 1.0 : 0.3);
				TestMissionPhase = Got == BRMissionText::TooFar ? 1 : 4;
				if (Got == BRMissionText::TooFar)
				{
					TestMissionDevice = -1; // nouvelle marche vers le mecanisme
				}
				return false;
			}
			if (Got != Expected && !bItemTaken)
			{
				Note(FString::Printf(TEXT("Niveau %d : action %d/%d sur %s : retour %d, attendu %d"), Level, TestMissionStep + 1, TestMission.Num() / 3,
					*BRMissionText::DeviceName(P, D), Got, Expected), true);
				return true;
			}
			// v4.15 : eau sous le joueur au moment de l'action (surface - fond, la profondeur qui fait nager) et nage
			if (D >= 0 && D < BRM::MaxDevices)
			{
				const BRMech::FWaterQuery Q = W->WaterAt(C->GetActorLocation() - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight()));
				WalkMission.ActDone[D] = true;
				WalkMission.ActDepth[D] = FMath::Max(WalkMission.ActDepth[D], Q.bWater ? Q.Surface - Q.Floor : 0.f);
				WalkMission.ActSwim[D] = WalkMission.ActSwim[D] || C->IsSwimming();
			}
			TestMissionRetries = 0;
			TestCountB = 0;
			++TestMissionStep;
			WalkMission.Actions = TestMissionStep;
			TestMissionPhase = 1;
			return false;
		}
		default:
			return true;
		}
	});
	LimitLastStep(920.f);
}

void ABRAutoTest::AddWalkExitSteps(int32 Level)
{
	using namespace BRTestV412;
	Add(FString::Printf(TEXT("v4.12 acces %d : sortie prise a pied"), Level), 0.f, [this, Level]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		const float Half = C->GetSimpleCollisionHalfHeight();
		const bool bBusy = C->GetScareKind() >= 0 || C->IsInputLocked() || C->IsDead();
		switch (TestMissionPhase)
		{
		default:
		{
			if ((W->IsTransitioning() || !W->IsLevelReady()) && StepTime < 30.f)
			{
				return false;
			}
			LastExit = FWalkExit();
			LastExit.Level = Level;
			// v4.15 : niveau attendu absent (cascade, mort pendant la mission) : la sortie n'est pas cherchee dans un autre niveau
			// (avant : "mission resolue oui" au Level 0, ou la sortie du niveau courant prise sous le nom de celui-ci)
			if (W->GetLevelNumber() != Level || C->IsDead())
			{
				Skip(FString::Printf(TEXT("Niveau %d non atteint (niveau actuel %d%s) : sortie non testee"), Level, W->GetLevelNumber(), C->IsDead() ? TEXT(", joueur mort") : TEXT("")));
				return true;
			}
			// Mission resolue, sortie ouverte (voyant vert), place devant la sortie
			BRM::FEval E;
			W->GetMissionEval(E);
			// v4.13 : sans mission (Level 0), la sortie est libre : la porte de service. v4.15 : seulement au Level 0 (une mission
			// absente ailleurs n'est pas une mission resolue)
			const bool bSolved = E.bSolved || (Level == 0 && !W->IsMissionActive());
			TestLevel = OpenForwardTarget(W, Level);
			ABRExit* Exit = TestLevel >= 0 ? FindExit(GetWorld(), TestLevel) : nullptr;
			if (!bSolved || !Exit)
			{
				Note(FString::Printf(TEXT("Niveau %d : mission resolue %s, sortie ouverte trouvee %s : sortie non testee (graine du niveau %u)"), Level, YesNo(bSolved),
					YesNo(Exit != nullptr), W->GetSeed()), true);
				return true;
			}
			const FVector Fwd = FRotator(0.f, Exit->GetActorRotation().Yaw, 0.f).Vector();
			const bool bLadder = Exit->IsClimbable();
			const FVector Foot = bLadder ? Exit->GetClimbAnchor() : Exit->GetActorLocation();
			const FVector Goal = Foot + Fwd * (bLadder ? 90.f : 130.f);
			const float FootZ = static_cast<float>(Exit->GetActorLocation().Z);
			Note(FString::Printf(TEXT("Niveau %d : sortie vers %d (%s, %s), voyant %s"), Level, TestLevel, bLadder ? TEXT("echelle") : TEXT("porte"),
				*W->DestinationLabel(TestLevel), Exit->GetMissionOpen() < 0 ? TEXT("aucun (sortie libre)") : (Exit->GetMissionOpen() == 1 ? TEXT("vert") : TEXT("ROUGE"))),
				Exit->GetMissionOpen() == 0);
			BeginWalk(W, C, FVector(Goal.X, Goal.Y, FootZ + Half), bLadder ? 60.f : 80.f, true, true, FootZ);
			TestCountB = C->ClimbRequestsSent;
			TestCountC = bLadder ? 1 : 0;
			TestExitSpot = Exit->GetActorLocation();
			TestMissionPhase = 11;
			return false;
		}
		case 11:
		{
			// v4.15 : mort ou niveau quitte pendant la marche vers la sortie : signale
			if (C->IsDead() || W->GetLevelNumber() != Level || W->IsTransitioning())
			{
				ReleaseJump(C);
				Note(FString::Printf(TEXT("Niveau %d : %s pendant la marche vers la sortie vers %d (graine du niveau %u) : %s"), Level, C->IsDead()
					? *FString::Printf(TEXT("MORT (cause %s)"), BRDeath::CauseId(C->GetDeathCause())) : *FString::Printf(TEXT("NIVEAU QUITTE (niveau %d)"), W->GetLevelNumber()),
					TestLevel, W->GetSeed(), *WalkStats()), true);
				return true;
			}
			const EWalk R = StepWalk(W, C, Dt, 300.0, bBusy);
			if (R == EWalk::Moving)
			{
				return false;
			}
			LastExit.SwimFrames = Walk.SwimFrames;
			LastExit.LocalSwimFrames = Walk.LocalSwimFrames;
			LastExit.LocalMaxDepth = Walk.LocalMaxDepth;
			LastExit.bReached = R == EWalk::Arrived;
			const FString Stats = WalkStats();
			FString Water;
			if (Walk.LocalFrames > 0)
			{
				Water = FString::Printf(TEXT(" ; volume d'eau local traverse : %d image(s), %d a la nage, profondeur maximale %.0f cm"), Walk.LocalFrames,
					Walk.LocalSwimFrames, Walk.LocalMaxDepth);
			}
			if (R == EWalk::Stuck)
			{
				Note(FString::Printf(TEXT("Niveau %d : sortie vers %d NON ATTEINTE a pied (cellule %d, %d ; pieds a %.0f cm, attendu %.0f) : %s%s"), Level, TestLevel,
					W->WorldToCell(C->GetActorLocation()).X, W->WorldToCell(C->GetActorLocation()).Y, C->GetActorLocation().Z - Half, Walk.GoalFeetZ, *Stats, *Water), true);
				Shot(FString::Printf(TEXT("v412_acces_%d_sortie_bloquee"), Level));
				return true;
			}
			// Poolrooms : le passage sec se traverse a pied (pas de nage, au plus une flaque)
			const bool bDry = Level != 37 || (Walk.LocalFrames > 0 && Walk.LocalSwimFrames == 0 && Walk.LocalMaxDepth < 30.f);
			Note(FString::Printf(TEXT("Niveau %d : sortie atteinte a pied : %s%s"), Level, *Stats, *Water), Walk.Teleports > 0 || !bDry);
			Shot(FString::Printf(TEXT("v412_acces_%d_sortie"), Level));
			TestMissionClock = Now;
			TestMissionPhase = 12;
			return false;
		}
		case 12:
		{
			// La touche d'interaction sur la sortie visee
			ABRExit* Exit = FindExit(GetWorld(), TestLevel);
			if (!Exit)
			{
				return true;
			}
			Face(C, Exit->IsClimbable() ? Exit->GetClimbAnchor() + FVector(0.f, 0.f, 60.f) : Exit->GetActorLocation() + FVector(0.f, 0.f, 110.f));
			if (Now - TestMissionClock < 0.15)
			{
				return false;
			}
			if (C->FocusActor.Get() != Exit)
			{
				if (Now - TestMissionClock < 2.0)
				{
					return false;
				}
				const AActor* Seen = C->FocusActor.Get();
				Note(FString::Printf(TEXT("Niveau %d : sortie vers %d PAS VISEE par l'interaction (vise : %s)"), Level, TestLevel, Seen ? *Seen->GetName() : TEXT("rien")), true);
				return true;
			}
			C->Interact();
			TestTimer = Now;
			TestMissionPhase = Exit->IsClimbable() ? 13 : 14;
			return false;
		}
		case 13:
		{
			// Echelle : on grimpe en avancant, jusqu'au depart (une seule demande au sommet)
			if (W->IsTransitioning() || W->IsChapterEnd() || W->IsEndingShown())
			{
				TestMissionPhase = 14;
				return false;
			}
			if (!C->IsClimbing())
			{
				if (Now - TestTimer > 1.0)
				{
					Note(FString::Printf(TEXT("Niveau %d : l'echelle vers %d n'a pas ete prise"), Level, TestLevel), true);
					return true;
				}
				return false;
			}
			C->InputMove(FVector2D(0.f, 1.f));
			if (Now - TestTimer > 30.0)
			{
				Note(FString::Printf(TEXT("Niveau %d : pas de depart apres 30 s sur l'echelle (attente %d, demandes %d)"), Level,
					static_cast<int32>(C->GetClimbWaitState()), C->ClimbRequestsSent - TestCountB), true);
				return true;
			}
			return false;
		}
		case 14:
		{
			// Arrivee : niveau suivant (ou redirige), ou fin du contenu disponible
			const BRContent::FExitResolution Res = BRLevels::ResolveExit(Level, TestLevel);
			const double Elapsed = Now - TestTimer;
			const int32 Requests = C->ClimbRequestsSent - TestCountB;
			const bool bLadder = TestCountC == 1;
			if (Res.Kind == BRContent::EExit::ChapterEnd)
			{
				if (!W->IsChapterEnd() && Elapsed < 15.0)
				{
					return false;
				}
				const bool bOk = W->IsChapterEnd() && W->GetChapterEndLot() == BRContent::CurrentLot(BRLevels::Channel());
				const FString Line = FString::Printf(TEXT("Niveau %2d -> fin du contenu disponible (chapitre %d) : %s"), Level, W->GetChapterEndLot(), YesNo(bOk));
				LastExit.bArrived = bOk;
				AccessLines.Add(Line);
				Note(Line, !bOk);
				Shot(FString::Printf(TEXT("v412_fin_du_contenu_%d"), Level));
				W->CloseEnding(false);
				return true;
			}
			if ((W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != Res.Target) && Elapsed < 40.0)
			{
				return false;
			}
			const bool bArrived = W->GetLevelNumber() == Res.Target && W->IsLevelReady();
			const bool bOneRequest = !bLadder || Requests == 1;
			LastExit.bArrived = bArrived && bOneRequest;
			const FString Line = FString::Printf(TEXT("Niveau %2d -> Niveau %d%s : arrivee %s%s"), Level, Res.Target, Res.bRedirected ? TEXT(" (redirige)") : TEXT(""),
				YesNo(bArrived), bLadder ? *FString::Printf(TEXT(", demandes de depart au sommet de l'echelle : %d (attendu 1)"), Requests) : TEXT(""));
			AccessLines.Add(Line);
			Note(Line, !bArrived || !bOneRequest);
			return true;
		}
		}
	});
	LimitLastStep(330.f);
}

// =====================================================================================================================
// v4.15 : Poolrooms de la graine 1217121623 (niveau 125584919, diag_v413_3) : devant la vanne B il n'y a que la marche
// immergee ou le canal (60 cm d'eau) ; la regle "au sec" des tests (plus de 10 cm d'eau) la declarait sans place alors que
// le jeu la laisse actionner en marchant dans le canal. Mission a pied, vanne B actionnee sans nager, sortie par le sas
// =====================================================================================================================

void ABRAutoTest::AddV415PoolroomsSteps()
{
	using namespace BRTestV412;
	AddSeedLoad(37, ValveSeed, FString::Printf(TEXT("v4.15 Poolrooms : vanne B depuis l'eau des canaux (graine %u)"), ValveSeed));
	AddWalkMissionSteps(37, ValveSeed, false);
	Add(TEXT("v4.15 Poolrooms : vanne B actionnee sans nager"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (W->GetLevelNumber() != 37 || !W->IsMissionActive() || WalkMission.Level != 37 || WalkMission.bNotReached)
		{
			Skip(FString::Printf(TEXT("Poolrooms (graine %u) non atteintes ou sans mission : vanne B non verifiee"), ValveSeed));
			return true;
		}
		const BRM::FPlan& P = W->GetMissionPlan();
		int32 ValveB = -1;
		for (int32 I = 0; I < P.NumDevices; ++I)
		{
			ValveB = (P.Devices[I].Role == BRM::R_Sluice && P.Devices[I].Label == 1) ? I : ValveB;
		}
		const bool bAct = ValveB >= 0 && WalkMission.ActDone[ValveB];
		const float Depth = ValveB >= 0 ? WalkMission.ActDepth[ValveB] : 0.f;
		const bool bSwam = ValveB >= 0 && WalkMission.ActSwim[ValveB];
		const bool bOk = WalkMission.bComplete && bAct && Depth <= BRMech::SwimDepth && !bSwam && WalkMission.SwimFrames == 0 && WalkMission.PoolFallbacks == 0;
		Note(FString::Printf(TEXT("Poolrooms, graine %u (niveau %u) : %d/%d actions faites a pied ; vanne B (mecanisme %d) actionnee %s, dans %.0f cm d'eau (on nage au-dela de "
			"%.0f cm), a la nage %s ; mission : %d image(s) a la nage, %d chemin(s) par un bassin"), ValveSeed, WalkMission.LevelSeed, WalkMission.Actions, WalkMission.Planned,
			ValveB, YesNo(bAct), Depth, BRMech::SwimDepth, bSwam ? TEXT("OUI") : TEXT("non"), WalkMission.SwimFrames, WalkMission.PoolFallbacks), !bOk);
		return true;
	});
	AddWalkExitSteps(37);
	Add(TEXT("v4.15 Poolrooms : sortie par le sas sans nager"), 0.f, [this]()
	{
		if (LastExit.Level != 37 || WalkMission.bNotReached)
		{
			Skip(FString::Printf(TEXT("Poolrooms (graine %u) : sortie non testee"), ValveSeed));
			return true;
		}
		const bool bOk = LastExit.bReached && LastExit.bArrived && LastExit.SwimFrames == 0 && LastExit.LocalSwimFrames == 0;
		Note(FString::Printf(TEXT("Poolrooms, graine %u : sortie atteinte a pied %s, %d image(s) a la nage (dans le sas : %d, eau au plus %.0f cm), arrivee au niveau suivant %s"),
			ValveSeed, YesNo(LastExit.bReached), LastExit.SwimFrames, LastExit.LocalSwimFrames, LastExit.LocalMaxDepth, YesNo(LastExit.bArrived)), !bOk);
		return true;
	});
}

// =====================================================================================================================
// Etapes v4.12 (solo)
// =====================================================================================================================

void ABRAutoTest::AddV412Steps()
{
	using namespace BRTestV412;
	Add(TEXT("v4.12 : preparation"), 0.f, [this]()
	{
		FLevelReport R;
		R.Title = TEXT("v4.12 : contenu publie, acces par deplacement reel, effets physiques, transactions, lumiere, reprise, carnet");
		R.Scene = TEXT("v4.12");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		PlacementLines.Reset();
		AccessLines.Reset();
		ModulePlacements = 0;
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Version publiee
	Add(TEXT("v4.12 contenu : version publiee"), 0.f, [this]()
	{
		BRLevels::SetChannelOverride(static_cast<int32>(BRContent::EChannel::Public));
		TArray<int32> Nums;
		for (const FBRLevelDef& D : BRLevels::All())
		{
			Nums.Add(D.Number);
		}
		const bool bList = Nums.Num() == 4 && Nums.Contains(0) && Nums.Contains(1) && Nums.Contains(2) && Nums.Contains(37);
		Note(FString::Printf(TEXT("niveaux disponibles (version publiee) : %s ; niveaux definis dans le projet : %d ; Niveau 5 defini %s, disponible %s"),
			*JoinInts(Nums), BRLevels::Defined().Num(), YesNo(BRLevels::IsDefined(5)), BRLevels::Exists(5) ? TEXT("OUI") : TEXT("non")), !bList || BRLevels::Exists(5));
		struct FCase
		{
			int32 From;
			int32 Target;
			BRContent::EExit Kind;
			int32 To;
			const TCHAR* What;
		};
		// v4.13 : plus de redirection : le fil est le meme dans tous les canaux (0 -> 1 -> Poolrooms -> 2 -> fin du chapitre)
		const FCase Cases[] = {
			{ 0, 1, BRContent::EExit::Go, 1, TEXT("Level 0 -> porte de service : Niveau 1") },
			{ 1, 37, BRContent::EExit::Go, 37, TEXT("Niveau 1 -> ascenseur : Poolrooms") },
			{ 37, 2, BRContent::EExit::Go, 2, TEXT("Poolrooms -> echelle : Niveau 2") },
			{ 2, 3, BRContent::EExit::ChapterEnd, -1, TEXT("Niveau 2 -> sortie de progression : fin du contenu disponible") },
		};
		// Aucune sortie ne saute une etape du fil (les anciennes 0 -> 37, 1 -> 2, 1 -> 4, 37 -> 4 n'existent plus)
		int32 Shortcuts = 0;
		for (const FBRLevelDef& D : BRLevels::Defined())
		{
			for (const FBRExitDef& X : D.Exits)
			{
				const int32 IFrom = BRStory::IndexOf(D.Number);
				const int32 ITo = BRStory::IndexOf(X.Target);
				Shortcuts += (X.Target < 0 || (ITo > IFrom + 1 && X.Target != BRStory::Ending)) ? 1 : 0;
			}
		}
		Note(FString::Printf(TEXT("sorties qui sautent une etape du fil ou menent au hasard : %d"), Shortcuts), Shortcuts != 0);
		for (const FCase& K : Cases)
		{
			const BRContent::FExitResolution R = BRLevels::ResolveExit(K.From, K.Target);
			const bool bOk = R.Kind == K.Kind && (K.Kind != BRContent::EExit::Go || R.Target == K.To);
			Note(FString::Printf(TEXT("%s : %s (genre %d, destination %d)"), K.What, bOk ? TEXT("OK") : TEXT("ECHEC"), static_cast<int32>(R.Kind), R.Target), !bOk);
		}
		// Connexion : meme version et meme contenu acceptes ; autre version, ancien client, autre contenu refuses (raison lisible)
		const uint32 Sig = BRLevels::ContentSignature();
		const FString Same = BRLevels::CheckJoinOptions(BRLevels::JoinOptions());
		const FString Old = BRLevels::CheckJoinOptions(TEXT("?Name=Ancien"));
		const FString OtherNet = BRLevels::CheckJoinOptions(FString::Printf(TEXT("?BRNet=%d?BRContent=%u"), BRContent::NetVersion - 1, Sig));
		const FString OtherContent = BRLevels::CheckJoinOptions(FString::Printf(TEXT("?BRNet=%d?BRContent=%u"), BRContent::NetVersion, Sig ^ 0x5A5Au));
		bool bVersion = true;
		int32 HostNet = 0, HostChannel = -1, HostLot = 0;
		const bool bParsed = BRLevels::ParseJoinRefusal(OtherContent, bVersion, HostNet, HostChannel, HostLot) && !bVersion && HostNet == BRContent::NetVersion
			&& HostChannel == 0 && HostLot == BRContent::CurrentLot(BRContent::EChannel::Public);
		bool bOldVersion = false;
		int32 N2 = 0, C2 = 0, L2 = 0;
		const bool bOldParsed = BRLevels::ParseJoinRefusal(Old, bOldVersion, N2, C2, L2) && bOldVersion;
		const bool bJoin = Same.IsEmpty() && bOldParsed && !OtherNet.IsEmpty() && bParsed;
		Note(FString::Printf(TEXT("connexion : meme contenu accepte %s ; ancien client refuse (version) %s ; autre protocole refuse %s ; autre contenu refuse et lu (chapitre %d de l'hote) %s"),
			YesNo(Same.IsEmpty()), YesNo(bOldParsed), YesNo(!OtherNet.IsEmpty()), HostLot, YesNo(bParsed)), !bJoin);
		// Partie v4.11 arretee au Niveau 5 : reprise au dernier niveau disponible explore, partie intacte
		TArray<int32> Explored = { 0, 1, 4, 5 };
		bool bMoved = false;
		const int32 Resume = BRLevels::ResumeLevel(5, Explored, &bMoved);
		const bool bResume = Resume == 1 && bMoved && Explored.Num() == 4 && BRLevels::CountAvailable(Explored) == 2;
		Note(FString::Printf(TEXT("partie v4.11 au Niveau 5 (0, 1, 4, 5 explores) : reprise au Niveau %d, deplacee %s, niveaux explores gardes %d, disponibles %d"), Resume,
			YesNo(bMoved), Explored.Num(), BRLevels::CountAvailable(Explored)), !bResume);
		// v4.13 : la meme partie migree vers l'histoire : alle au-dela des Poolrooms sans les traverser, elle reprend dans les
		// Poolrooms (disponibles dans la version publiee) ; sa progression est garee, pas perdue
		BRStory::FLegacy Legacy;
		Legacy.Explored = Explored.GetData();
		Legacy.NumExplored = Explored.Num();
		Legacy.Current = 5;
		const BRStory::FMigration M = BRStory::MigrateLegacy(Legacy);
		const int32 Cont = BRStory::ResumePoint(M.State, [](int Level, void*) { return BRLevels::Exists(Level); }, nullptr);
		const bool bStory = M.bParkSession && M.State.Parked == 5 && Cont == BRStory::Poolrooms && !M.bPoolroomsCredited;
		Note(FString::Printf(TEXT("la meme partie en histoire : CONTINUER au Niveau %d, progression garee au Niveau %d, Poolrooms creditees %s"), Cont, M.State.Parked,
			YesNo(M.bPoolroomsCredited)), !bStory);
		return true;
	});
	Add(TEXT("v4.12 contenu : menu de la version publiee"), 1.2f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return false;
		}
		bWasInMenu = PC->bInMenu;
		PC->SetInventoryOpen(false);
		PC->bInMenu = true;
		PC->SetMenuPage(EBRMenuPage::Solo);
		PC->UpdateInputMode();
		return true;
	});
	Add(TEXT("v4.12 contenu : capture du menu"), 0.f, [this]()
	{
		Shot(TEXT("v412_menu_version_publiee"));
		if (ABRPlayerController* PC = GetPC())
		{
			PC->bInMenu = bWasInMenu;
			PC->SetMenuPage(EBRMenuPage::Main);
			PC->UpdateInputMode();
		}
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Inventaire, sante
	Add(TEXT("v4.12 inventaire : une seule regle de place"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		if (!C)
		{
			return false;
		}
		if (C->Equipment.Num() < static_cast<int32>(EBREquipSlot::Count))
		{
			Skip(TEXT("regle de place : equipement non initialise"));
			return true;
		}
		SavedPockets = C->Pockets;
		SavedStorage = C->Storage;
		SavedEquipment = C->Equipment;
		SavedRecovered = C->Recovered;
		// Poches et sac pleins d'objets non empilables, equipement libre : une lampe a encore sa place (main ou ceinture)
		for (FBRItemSlot& S : C->Pockets)
		{
			S = FBRItemSlot{ EBRItem::Vest, 1 };
		}
		for (FBRItemSlot& S : C->Storage)
		{
			S = FBRItemSlot{ EBRItem::Vest, 1 };
		}
		for (FBRItemSlot& S : C->Equipment)
		{
			S.Clear();
		}
		C->Recovered.Reset();
		const int32 Room = C->RoomFor(EBRItem::Flashlight);
		const int32 Left = C->AddItem(EBRItem::Flashlight, 1);
		const bool bLamp = Room >= 1 && Left == 0;
		// Plus rien de libre : la place annoncee et l'ajout disent la meme chose
		C->Equipment[static_cast<int32>(EBREquipSlot::Head)] = FBRItemSlot{ EBRItem::Headlamp, 1 };
		C->Equipment[static_cast<int32>(EBREquipSlot::Chest)] = FBRItemSlot{ EBRItem::Vest, 1 };
		C->Equipment[static_cast<int32>(EBREquipSlot::Hand)] = FBRItemSlot{ EBRItem::Flashlight, 1 };
		C->Equipment[static_cast<int32>(EBREquipSlot::Belt)] = FBRItemSlot{ EBRItem::Flashlight, 1 };
		const int32 RoomFull = C->RoomFor(EBRItem::AlmondWater);
		const int32 LeftFull = C->AddItem(EBRItem::AlmondWater, 1);
		const bool bFull = RoomFull == 0 && LeftFull == 1;
		Note(FString::Printf(TEXT("poches et sac pleins, equipement libre : place pour une lampe %d, ajout %s (avant la v4.12 : \"inventaire plein\") ; tout plein : place %d, ajout refuse %s"),
			Room, Left == 0 ? TEXT("range") : TEXT("REFUSE"), RoomFull, YesNo(LeftFull == 1)), !bLamp || !bFull);
		return true;
	});
	Add(TEXT("v4.12 inventaire : objet accepte sans place"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		if (!C)
		{
			return false;
		}
		// La reponse de l'hote arrive alors que l'inventaire s'est rempli : l'objet est mis de cote, jamais perdu
		const int32 Before = C->RecoveredPickups;
		const int32 Almond = C->CountItem(EBRItem::AlmondWater);
		const int32 RoomBefore = C->RoomFor(EBRItem::AlmondWater);
		const int32 AsideBefore = C->CountRecovered();
		C->StorePickup(EBRItem::AlmondWater, false);
		const bool bAside = C->CountRecovered() == 1 && C->RecoveredPickups == Before + 1 && C->CountItem(EBRItem::AlmondWater) == Almond;
		const FString AsideInfo = FString::Printf(TEXT("place avant %d, mis de cote avant %d apres %d, compteur +%d, eaux %d -> %d"), RoomBefore, AsideBefore, C->CountRecovered(),
			C->RecoveredPickups - Before, Almond, C->CountItem(EBRItem::AlmondWater));
		// Une place se libere : il se range
		C->RemoveItem(EBRItem::Vest, 1);
		const int32 Stowed = C->StowRecovered(false);
		const bool bStowed = Stowed == 1 && C->CountRecovered() == 0 && C->CountItem(EBRItem::AlmondWater) == Almond + 1;
		// Mis de cote, puis sauvegarde : il est dans le fichier et revient a la lecture
		C->StorePickup(EBRItem::Battery, false);
		UBRSaveGame* S = NewObject<UBRSaveGame>();
		C->WriteToSave(S);
		TArray<uint8> Bytes;
		UGameplayStatics::SaveGameToMemory(S, Bytes);
		const UBRSaveGame* Back = Cast<UBRSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		const bool bSaved = Back && Back->Recovered.Num() == 1 && Back->Recovered[0].Item == static_cast<uint8>(EBRItem::Battery);
		Note(FString::Printf(TEXT("reponse acceptee sans place : mise de cote %s%s ; place liberee : rangee %s ; dans la sauvegarde %s"), YesNo(bAside),
			bAside ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *AsideInfo), YesNo(bStowed), YesNo(bSaved)), !bAside || !bStowed || !bSaved);
		C->Pockets = SavedPockets;
		C->Storage = SavedStorage;
		C->Equipment = SavedEquipment;
		C->Recovered = SavedRecovered;
		return true;
	});
	Add(TEXT("v4.12 sante : envoi perime ignore"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		if (!C || !C->HasAuthority())
		{
			return true;
		}
		// Un envoi du proprietaire parti avant le dernier changement officiel (revision plus ancienne) ne l'efface pas
		C->Health = 80.f;
		const uint16 Rev = C->GetHealthRev();
		C->ServerSyncVitals(20.f, static_cast<uint16>(Rev - 1));
		const bool bIgnored = FMath::IsNearlyEqual(C->Health, 80.f);
		C->ServerSyncVitals(60.f, Rev);
		const bool bApplied = FMath::IsNearlyEqual(C->Health, 60.f);
		Note(FString::Printf(TEXT("sante (revision %u) : envoi de la revision precedente ignore %s, envoi a jour applique %s"), Rev, YesNo(bIgnored), YesNo(bApplied)),
			!bIgnored || !bApplied);
		C->Health = 100.f;
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Poolrooms (version publiee)
	AddSeedLoad(37, 4120, TEXT("v4.12 Poolrooms : bassins et passage sec"));
	Add(TEXT("v4.12 Poolrooms : bassins avant la mission"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || !W->IsMissionActive())
		{
			return StepTime > 10.f;
		}
		namespace P = BRMech::Pool;
		const FBRLevelDef& D = W->Def();
		ABRMissionDevice* Tanks = FindModule(W, BRMech::Module::PoolTanks);
		ABRMissionDevice* Lock = FindModule(W, BRMech::Module::PoolLock);
		if (!Tanks || !Lock)
		{
			Note(FString::Printf(TEXT("Poolrooms : bassins %s, sas %s"), YesNo(Tanks != nullptr), YesNo(Lock != nullptr)), true);
			return true;
		}
		int32 LA = 0, LB = 0;
		BRM::PoolLevels(W->GetMissionPlan(), W->GetMissionState(), LA, LB);
		const float SA = W->GetLocalWaterSurface(Tanks, 0);
		const float SB = W->GetLocalWaterSurface(Tanks, 1);
		const float SL = W->GetLocalWaterSurface(Lock, 0);
		const float Slab = P::SlabTop(D.WaterHeight, D.DeckHeight);
		const BRMech::FFrame LF = Lock->GetModuleFrame();
		const BRMech::FWaterQuery Q = W->WaterAt(BoxCenter(LF, -P::ChannelLength, -P::ChannelHalfWidth, -8.f, P::ChannelHalfWidth, Slab + 20.f));
		const bool bTanks = FMath::Abs(SA - P::SurfaceA(LA, D.WaterHeight)) < 0.5f && FMath::Abs(SB - P::SurfaceB(LB, D.WaterHeight)) < 0.5f;
		const bool bLock = FMath::Abs(SL - P::ChannelFull(Slab)) < 0.5f && Q.bLocal && Q.bWater;
		Note(FString::Printf(TEXT("avant : bassin A marque %d (surface %.1f, attendu %.1f), bassin B marque %d (%.1f, attendu %.1f) ; sas plein %s (surface %.1f, dallage %.1f, %.0f cm d'eau)"),
			LA, SA, P::SurfaceA(LA, D.WaterHeight), LB, SB, P::SurfaceB(LB, D.WaterHeight), YesNo(bLock), SL, Slab, SL - Slab), !bTanks || !bLock);
		// Toujours aucune entite dans les Poolrooms
		Note(FString::Printf(TEXT("Poolrooms : %d entite(s) dans le niveau (attendu 0)"), W->GetEntities().Num()), W->GetEntities().Num() != 0);
		Shot(TEXT("v412_poolrooms_avant"));
		return true;
	});
	AddWalkMissionSteps(37, 4120, false);
	Add(TEXT("v4.12 Poolrooms : bassins apres la mission"), 0.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		namespace P = BRMech::Pool;
		ABRMissionDevice* Tanks = FindModule(W, BRMech::Module::PoolTanks);
		ABRMissionDevice* Lock = FindModule(W, BRMech::Module::PoolLock);
		if (!Tanks || !Lock)
		{
			return true; // deja note
		}
		// Les surfaces s'animent (26 et 25 cm/s) : on attend qu'elles soient posees
		float RA = 0.f, RB = 0.f, RL = 0.f;
		const float SA = W->GetLocalWaterSurface(Tanks, 0, &RA);
		const float SB = W->GetLocalWaterSurface(Tanks, 1, &RB);
		const float SL = W->GetLocalWaterSurface(Lock, 0, &RL);
		if ((FMath::Abs(RA) > 0.01f || FMath::Abs(RB) > 0.01f || FMath::Abs(RL) > 0.01f) && StepTime < 20.f)
		{
			return false;
		}
		const FBRLevelDef& D = W->Def();
		int32 LA = 0, LB = 0;
		BRM::PoolLevels(W->GetMissionPlan(), W->GetMissionState(), LA, LB);
		const float Slab = P::SlabTop(D.WaterHeight, D.DeckHeight);
		// Ce que lit le personnage (nage, profondeur) au milieu des bassins et du sas : la meme surface que le rendu
		const BRMech::FFrame TF = Tanks->GetModuleFrame();
		const BRMech::FFrame LF = Lock->GetModuleFrame();
		const float X0 = -P::TankDepth + P::Wall, X1 = -P::Wall;
		const BRMech::FWaterQuery QA = W->WaterAt(BoxCenter(TF, X0, P::AY0, X1, P::AY1, P::BottomA(D.WaterHeight) + 10.f));
		const BRMech::FWaterQuery QB = W->WaterAt(BoxCenter(TF, X0, P::BY0, X1, P::BY1, P::BottomB(D.WaterHeight) + 10.f));
		const BRMech::FWaterQuery QL = W->WaterAt(BoxCenter(LF, -P::ChannelLength, -P::ChannelHalfWidth, -8.f, P::ChannelHalfWidth, Slab + 20.f));
		const bool bMarks = P::MarkOfA(SA, D.WaterHeight) == LA && P::MarkOfB(SB, D.WaterHeight) == LB;
		const bool bSame = QA.bLocal && QB.bLocal && FMath::Abs(QA.Surface - SA) < 0.5f && FMath::Abs(QB.Surface - SB) < 0.5f;
		const bool bDry = FMath::Abs(SL - P::ChannelDry(Slab)) < 0.5f && QL.bLocal && !QL.bWater;
		Note(FString::Printf(TEXT("apres : marques lues A %d / %d, B %d / %d (etat de mission) ; eau du personnage = surface rendue %s ; sas vide %s (surface %.1f, dallage %.1f)"),
			P::MarkOfA(SA, D.WaterHeight), LA, P::MarkOfB(SB, D.WaterHeight), LB, YesNo(bSame), YesNo(bDry), SL, Slab), !bMarks || !bSame || !bDry);
		Shot(TEXT("v412_poolrooms_apres"));
		return true;
	});
	AddWalkExitSteps(37);
	// v4.15 : la graine dont la vanne B n'a de place que dans l'eau des canaux (diag_v413_3)
	AddV415PoolroomsSteps();

	// ---------------------------------------------------------------------------------------------- Lot 1 a pied
	AddWalkMissionSteps(0, 4120);
	AddWalkExitSteps(0);
	AddWalkMissionSteps(1, 4120);
	AddWalkExitSteps(1);
	AddWalkMissionSteps(2, 4120);
	AddWalkExitSteps(2);

	// ---------------------------------------------------------------------------------------------- Niveau 8 (developpement)
	Add(TEXT("v4.12 contenu : retour a tous les niveaux"), 0.f, []()
	{
		BRLevels::SetChannelOverride(static_cast<int32>(BRContent::EChannel::All));
		return true;
	});
	AddSeedLoad(8, 4128, TEXT("v4.12 Niveau 8 : passerelle"));
	Add(TEXT("v4.12 passerelle : tablier leve, palier hors d'atteinte"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || !W->IsMissionActive())
		{
			return StepTime > 10.f;
		}
		namespace B = BRMech::Bridge;
		ABRMissionDevice* Bridge = FindModule(W, BRMech::Module::Bridge);
		if (!Bridge)
		{
			Note(TEXT("Niveau 8 : aucune passerelle construite"), true);
			return true;
		}
		const BRMech::FFrame F = Bridge->GetModuleFrame();
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		if (TestMissionPhase != 21)
		{
			// Tout droit vers le palier (sans la regle de contournement) : on doit buter contre son arete, meme en sautant
			float X = 0.f, Y = 0.f;
			F.ToWorld(-B::FarDepth * 0.5f, 0.f, X, Y);
			BeginWalk(W, C, FVector(X, Y, F.Z + B::FarTop + C->GetSimpleCollisionHalfHeight()), 40.f, false, true, F.Z + B::FarTop);
			TestMissionPhase = 21;
			return false;
		}
		const EWalk R = StepWalk(W, C, Dt, 180.0, C->GetScareKind() >= 0 || C->IsInputLocked());
		if (R == EWalk::Moving)
		{
			return false;
		}
		TestMissionPhase = 0;
		const float Reached = Walk.MaxFeetZ - F.Z;
		// Bloque contre l'arete du palier (et non en chemin) : a moins de 2,1 m du milieu du palier
		if (R == EWalk::Stuck && FVector::Dist2D(C->GetActorLocation(), Walk.Goal) > B::FarDepth * 0.5f + 160.f)
		{
			Skip(FString::Printf(TEXT("tablier leve : le pied du palier n'a pas ete atteint (bloque a %.0f cm du palier, %s)"),
				FVector::Dist2D(C->GetActorLocation(), Walk.Goal), *WalkStats()));
			return true;
		}
		const bool bOk = R == EWalk::Stuck && Reached < B::FarTop - 20.f && Bridge->GetModuleProgress() < 0.01f && Walk.Jumps > 0;
		Note(FString::Printf(TEXT("tablier leve (progression %.2f) : palier (%.0f cm) %s ; pieds au plus haut a %.0f cm apres %d saut(s)"), Bridge->GetModuleProgress(), B::FarTop,
			R == EWalk::Arrived ? TEXT("ATTEINT") : TEXT("hors d'atteinte"), Reached, Walk.Jumps), !bOk);
		Shot(TEXT("v412_passerelle_levee"));
		return true;
	});
	LimitLastStep(200.f);
	AddWalkMissionSteps(8, 4128, false);
	Add(TEXT("v4.12 passerelle : tablier pose"), 0.2f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRMissionDevice* Bridge = W ? FindModule(W, BRMech::Module::Bridge) : nullptr;
		if (!Bridge)
		{
			return true;
		}
		if (Bridge->GetModuleProgress() < 0.99f && StepTime < 8.f)
		{
			return false;
		}
		namespace B = BRMech::Bridge;
		const BRMech::FFrame F = Bridge->GetModuleFrame();
		// Les entites suivent le meme etat : du pied des marches vers le palier, l'etape suivante monte sur l'appui
		float FX = 0.f, FY = 0.f, LX = 0.f, LY = 0.f;
		F.ToWorld(B::Gap + B::NearDepth + B::Steps * B::StepDepth + 45.f, 0.f, FX, FY);
		F.ToWorld(-B::FarDepth * 0.5f, 0.f, LX, LY);
		FVector WP;
		const bool bNav = W->MissionNavDetour(FVector(FX, FY, F.Z + 88.f), FVector(LX, LY, F.Z + B::FarTop + 88.f), WP) && WP.Z > F.Z + B::NearTop;
		const bool bOk = Bridge->GetModuleProgress() >= 0.99f && bNav;
		Note(FString::Printf(TEXT("treuils faits : tablier pose %s (progression %.2f en %.1f s), navigation des entites par le tablier %s"), YesNo(Bridge->GetModuleProgress() >= 0.99f),
			Bridge->GetModuleProgress(), StepTime, YesNo(bNav)), !bOk);
		Shot(TEXT("v412_passerelle_posee"));
		return true;
	});
	AddWalkExitSteps(8);

	// ---------------------------------------------------------------------------------------------- Niveau 6 : lumiere, Tick
	AddSeedLoad(6, 4126, TEXT("v4.12 Niveau 6 : balises et mecanismes masques"));
	Add(TEXT("v4.12 lumiere : avant les balises"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || !W->IsMissionActive())
		{
			return StepTime > 10.f;
		}
		const BRM::FPlan& P = W->GetMissionPlan();
		const ABRMissionDevice* Beacon = W->FindMissionDevice(BRM::FirstRole(P, BRM::R_Beacon));
		LightsBefore = W->NumGameplayLights();
		if (!Beacon)
		{
			// v4.13 : le Level 6 se joue sans lumiere (scanner LiDAR) ; ses postes de confinement (anciennes balises) ont un
			// petit voyant qui n'eclaire pas le secteur. Les balises ne restent que pour convertir les anciennes sauvegardes
			BeaconLightBefore = -1.f;
			Note(FString::Printf(TEXT("avant (Level 6 sans lumiere depuis la v4.13) : %d source(s) de gameplay (attendu 0)"), LightsBefore), LightsBefore != 0);
			return true;
		}
		BeaconLightBefore = W->LightLevelAt(Beacon->GetActorLocation() + Beacon->GetActorForwardVector() * 150.f + FVector(0.f, 0.f, 100.f));
		Note(FString::Printf(TEXT("avant : %d source(s) de gameplay, lumiere devant la premiere balise %.2f"), LightsBefore, BeaconLightBefore));
		return true;
	});
	AddWalkMissionSteps(6, 4126, false);
	Add(TEXT("v4.12 lumiere : balises allumees"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || !W->IsMissionActive())
		{
			return true;
		}
		const BRM::FPlan& P = W->GetMissionPlan();
		const ABRMissionDevice* Beacon = W->FindMissionDevice(BRM::FirstRole(P, BRM::R_Beacon));
		if (!Beacon)
		{
			// v4.13 : postes faits, toujours aucune source de gameplay (le secteur reste a decouvrir au scanner)
			const int32 Lights = W->NumGameplayLights();
			Note(FString::Printf(TEXT("apres (Level 6 sans lumiere) : %d source(s) de gameplay (attendu 0), mission resolue %s"), Lights,
				YesNo((W->GetMissionState().Solved & 1) != 0)), Lights != 0);
			return true;
		}
		const FVector Probe = Beacon->GetActorLocation() + Beacon->GetActorForwardVector() * 150.f + FVector(0.f, 0.f, 100.f);
		const int32 Lights = W->NumGameplayLights();
		const float After = W->LightLevelAt(Probe);
		// Une coupure eteint les plafonniers, pas les sources autonomes de la mission
		W->ForceBlackout();
		const float InBlackout = W->LightLevelAt(Probe);
		const bool bOk = Lights > LightsBefore && After > BeaconLightBefore + 0.1f && InBlackout > 0.1f && W->NumGameplayLights() == Lights;
		Note(FString::Printf(TEXT("apres : %d source(s) de gameplay (avant %d), lumiere devant la balise %.2f (avant %.2f), pendant une coupure %.2f"), Lights, LightsBefore, After,
			BeaconLightBefore, InBlackout), !bOk);
		Shot(TEXT("v412_balises"));
		return true;
	});
	Add(TEXT("v4.12 Tick : eloignement"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->GetMissionDevices().Num() == 0)
		{
			return true;
		}
		TestCountA = 0;
		// Le plus loin possible des mecanismes (dans l'enceinte) : leurs chunks sont retires, ils sont masques
		FVector Center = FVector::ZeroVector;
		for (const ABRMissionDevice* D : W->GetMissionDevices())
		{
			Center += D ? D->GetActorLocation() : FVector::ZeroVector;
		}
		Center /= static_cast<double>(W->GetMissionDevices().Num());
		const FIntPoint C0 = W->WorldToCell(Center);
		for (const int32 R : { 40, 32, 24, 16 })
		{
			for (int32 K = 0; K < 16; ++K)
			{
				const float A = K * PI / 8.f;
				const FIntPoint Cell(C0.X + FMath::RoundToInt(R * FMath::Cos(A)), C0.Y + FMath::RoundToInt(R * FMath::Sin(A)));
				if (W->IsCellInBounds(Cell.X, Cell.Y) && W->IsWalkable(Cell))
				{
					PlaceAndFace(C, W->CellCenter(Cell, static_cast<float>(C->GetActorLocation().Z) + 40.f), Center);
					TestExitSpot = Center;
					return true;
				}
			}
		}
		Skip(TEXT("Tick des mecanismes masques : aucune cellule libre loin des mecanismes"));
		return true;
	});
	Add(TEXT("v4.12 Tick : mecanismes masques"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return true;
		}
		int32 Shown = 0;
		for (const ABRMissionDevice* D : W->GetMissionDevices())
		{
			Shown += D && D->IsShown() ? 1 : 0;
		}
		if (Shown > 0 && StepTime < 25.f)
		{
			return false;
		}
		if (Shown > 0)
		{
			Skip(FString::Printf(TEXT("Tick des mecanismes masques : %d mecanisme(s) encore affiche(s) a cette distance"), Shown));
			return true;
		}
		if (TestCountA != 1)
		{
			ABRMissionDevice::ResetTickStats();
			TestTimer = FPlatformTime::Seconds();
			TestCountA = 1;
			return false;
		}
		if (FPlatformTime::Seconds() - TestTimer < 3.0)
		{
			return false;
		}
		TestCountA = 0;
		Note(FString::Printf(TEXT("tous les mecanismes masques : %lld Tick(s) de mecanisme en 3 s (attendu 0)"), ABRMissionDevice::TickCount), ABRMissionDevice::TickCount != 0);
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Reprise
	Add(TEXT("v4.12 reprise : etat d'une autre version"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return StepTime > 10.f;
		}
		BRSaves::SetTestPrefix(SavePrefix);
		DeleteSaveFiles();
		const uint32 Seed = ABRWorld::SeedFromUser(4122, 2);
		UBRSaveGame* S = NewObject<UBRSaveGame>();
		S->Version = UBRSaveGame::CurrentVersion;
		S->SaveName = TEXT("Partie v4.12 (reprise)");
		S->Created = FDateTime::Now();
		S->MarkExplored(0);
		S->MarkExplored(2);
		S->CurrentLevel = 2;
		S->Session.bValid = true;
		S->Session.Level = 2;
		S->Session.Seed = Seed;
		S->Session.GenVersion = BRM::GenVersion;
		S->Session.PlaceVersion = 1;
		S->Session.Mission = { 0x02, 0x13, 0x37, 0x00 }; // ne correspond a aucun plan
		OriginalBytes.Reset();
		UGameplayStatics::SaveGameToMemory(S, OriginalBytes);
		FFileHelper::SaveArrayToFile(OriginalBytes, *BRSaves::FilePath(BRSaves::SlotName(TestSlot)));
		TestOrigSlot = BRSaves::ActiveSlot();
		TestOrigSave = PC->ActiveSave;
		bTestDevSession = PC->bDevSession;
		BRSaves::ActiveSlot() = TestSlot;
		PC->ActiveSave = S;
		PC->bDevSession = false;
		BRSaves::PendingResume() = S->Session;
		TestCountC = W->GetLevelSerial();
		W->RequestTransition(2, false, Seed);
		return true;
	});
	Add(TEXT("v4.12 reprise : mission non restauree"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 2 || W->GetLevelSerial() == TestCountC)
		{
			return StepTime > 30.f;
		}
		const uint8 Notice = W->GetMissionNotice();
		PC->WriteActiveSave(true);
		TArray<uint8> Copy;
		const bool bCopy = FFileHelper::LoadFileToArray(Copy, *BRSaves::FilePath(BRSaves::RecoverySlotName(TestSlot))) && Copy == OriginalBytes;
		const bool bOk = Notice == 2 && W->IsMissionActive() && bCopy;
		Note(FString::Printf(TEXT("etat de mission d'une autre version : avis %d (attendu 2), mission reprise du debut %s, copie intacte de la sauvegarde avant reecriture %s"),
			Notice, YesNo(W->IsMissionActive()), YesNo(bCopy)), !bOk);
		Shot(TEXT("v412_reprise_non_restauree"));
		return true;
	});
	Add(TEXT("v4.12 reprise : placement d'une autre version"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return false;
		}
		// Un etat valide pour ce plan (un indice lu), enregistre avec le placement d'avant la v4.12
		const uint32 Seed = ABRWorld::SeedFromUser(4122, 2);
		BRM::FPlan MPlan;
		BRM::BuildPlan(2, Seed, MPlan);
		BRM::FState State;
		BRM::InitState(MPlan, State);
		RestoredDevice = -1;
		for (int32 I = 0; I < MPlan.NumDevices && RestoredDevice < 0; ++I)
		{
			RestoredDevice = MPlan.Devices[I].Kind == BRM::EKind::Clue ? I : -1;
		}
		if (RestoredDevice >= 0)
		{
			State.Dev[RestoredDevice] = 1;
		}
		uint8 Buffer[512];
		const int32 Size = BRM::Serialize(MPlan, State, Buffer, sizeof(Buffer));
		FBRSessionState Session = PC->ActiveSave ? PC->ActiveSave->Session : FBRSessionState();
		Session.bValid = true;
		Session.Level = 2;
		Session.Seed = Seed;
		Session.GenVersion = BRM::GenVersion;
		Session.PlaceVersion = 1;
		Session.Mission = TArray<uint8>(Buffer, Size);
		BRSaves::PendingResume() = Session;
		TestCountC = W->GetLevelSerial();
		W->RequestTransition(2, false, Seed);
		return true;
	});
	Add(TEXT("v4.12 reprise : progression gardee"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 2 || W->GetLevelSerial() == TestCountC)
		{
			return StepTime > 30.f;
		}
		const uint8 Notice = W->GetMissionNotice();
		const bool bKept = RestoredDevice >= 0 && W->IsMissionActive() && W->GetMissionState().Dev[RestoredDevice] != 0;
		Note(FString::Printf(TEXT("placement d'une autre version : avis %d (attendu 3 : mecanismes replaces), indice lu garde %s"), Notice, YesNo(bKept)), Notice != 3 || !bKept);
		Shot(TEXT("v412_reprise_mecanismes_replaces"));
		// v4.15 : placement 3 : une session du placement 2 (v4.14) hors du Level 6 retrouve ses mecanismes a la meme place
		// (aucun avis "reamenages") ; la sauvegarde de test reste active pour cette reprise
		const uint32 Seed = ABRWorld::SeedFromUser(4122, 2);
		BRM::FPlan MPlan;
		BRM::BuildPlan(2, Seed, MPlan);
		BRM::FState State;
		BRM::InitState(MPlan, State);
		uint8 Buffer[512];
		const int32 Size = BRM::Serialize(MPlan, State, Buffer, sizeof(Buffer));
		FBRSessionState Session = PC->ActiveSave ? PC->ActiveSave->Session : FBRSessionState();
		Session.bValid = true;
		Session.Level = 2;
		Session.Seed = Seed;
		Session.GenVersion = BRM::GenVersion;
		Session.PlaceVersion = 2;
		Session.Mission = TArray<uint8>(Buffer, Size);
		BRSaves::PendingResume() = Session;
		TestCountC = W->GetLevelSerial();
		W->RequestTransition(2, false, Seed);
		return true;
	});
	Add(TEXT("v4.15 reprise : placement 2 hors du Level 6"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 2 || W->GetLevelSerial() == TestCountC)
		{
			return StepTime > 30.f;
		}
		const uint8 Notice = W->GetMissionNotice();
		Note(FString::Printf(TEXT("session du placement 2 (v4.14) au Niveau 2 : avis %d (attendu 0 : mecanismes a la meme place)"), Notice), Notice != 0 || !W->IsMissionActive());
		BRSaves::Flush();
		BRSaves::ActiveSlot() = TestOrigSlot;
		PC->ActiveSave = TestOrigSave;
		PC->bDevSession = bTestDevSession;
		TestOrigSave = nullptr;
		DeleteSaveFiles();
		BRSaves::SetTestPrefix(FString());
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Carnet
	Add(TEXT("v4.12 carnet : ouverture"), 0.5f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return false;
		}
		TestCountA = 0;
		PC->SetInventoryOpen(true, static_cast<int32>(ABRHUD::ETab::Notebook));
		return true;
	});
	Add(TEXT("v4.12 carnet : colonne de gauche defilante"), 0.3f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr;
		if (!H)
		{
			return false;
		}
		if (TestCountA == 0)
		{
			// Demande de defilement tres loin : l'image suivante la borne au contenu
			H->NotebookLeftScroll = 1.0e6f;
			TestCountA = 1;
			return false;
		}
		if (TestCountA == 1)
		{
			const bool bClamped = H->NotebookLeftScroll >= 0.f && H->NotebookLeftScroll < 1.0e6f;
			Note(FString::Printf(TEXT("colonne de gauche : defilement borne au contenu %s (%.0f)"), YesNo(bClamped), H->NotebookLeftScroll), !bClamped);
			Shot(TEXT("v412_carnet_bas"));
			// Aide de niveau 2 : la colonne defile d'elle-meme jusqu'a l'aide
			H->NotebookLeftScroll = 0.f;
			H->NotebookHint.FindOrAdd(H->NotebookHintStep) = 2;
			TestCountA = 2;
			return false;
		}
		const bool bHint = H->NotebookHintShown == 2 && H->LastNotebookHintLevel == 2;
		Note(FString::Printf(TEXT("aide de niveau 2 : affichee %s, colonne amenee a l'aide (defilement %.0f)"), YesNo(bHint), H->NotebookLeftScroll), !bHint);
		Shot(TEXT("v412_carnet_aide"));
		TestCountA = 0;
		TestLanguage = BRLoc::Current().Code;
		return true;
	});
	// Langue longue et ecriture de droite a gauche : captures du meme carnet (aide de niveau 2 ouverte), a la resolution
	// courante (les autres resolutions restent a capturer a la main)
	for (const TCHAR* Code : { TEXT("de"), TEXT("ar") })
	{
		const FString Lang = Code;
		Add(TEXT("v4.12 carnet : langue ") + Lang, 1.f, [Lang]()
		{
			BRLoc::SetLanguage(Lang);
			return true;
		});
		Add(TEXT("v4.12 carnet : capture ") + Lang, 0.f, [this, Lang]()
		{
			Shot(TEXT("v412_carnet_aide_") + Lang);
			return true;
		});
	}
	Add(TEXT("v4.12 carnet : fermeture"), 0.f, [this]()
	{
		BRLoc::SetLanguage(TestLanguage);
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr;
		if (H)
		{
			H->NotebookHint.Reset();
		}
		if (PC)
		{
			PC->SetInventoryOpen(false);
		}
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Bilan
	Add(TEXT("v4.12 : bilan"), 0.f, [this]()
	{
		BRLevels::SetChannelOverride(-1);
		for (const FString& L : PlacementLines)
		{
			Note(TEXT("placement : ") + L);
		}
		for (const FString& L : AccessLines)
		{
			Note(TEXT("acces : ") + L);
		}
		Note(FString::Printf(TEXT("acces par deplacement reel : %d sortie(s) prise(s) a pied sur 6 (Niveaux 37, 0, 1, 2, 8 ; Niveau 6 sans sortie) ; modules de secours : %d"),
			AccessLines.Num(), ModulePlacements), AccessLines.Num() != 5);
		return true;
	});
}

// =====================================================================================================================
// Etapes v4.12 (reseau) : au Niveau 37, les deux machines
// =====================================================================================================================

void ABRAutoTest::AddNetV412Steps(bool bClient)
{
	using namespace BRTestV412;

	Add(TEXT("v4.12 reseau : compatibilite"), 0.f, [this, bClient]()
	{
		const bool bSelf = BRLevels::CheckJoinOptions(BRLevels::JoinOptions()).IsEmpty();
		Note(FString::Printf(TEXT("%s : protocole %d, contenu %08x (canal %d), options acceptees par sa propre regle %s (a comparer entre les deux rapports)"),
			bClient ? TEXT("client") : TEXT("hote"), BRContent::NetVersion, BRLevels::ContentSignature(), static_cast<int32>(BRLevels::Channel()), YesNo(bSelf)), !bSelf);
		return true;
	});

	// Le client fait une demande : "en attente" tout de suite, puis "accepte" a la reponse de l'hote
	Add(TEXT("v4.12 reseau : demande en attente"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		TestCountB = 0;
		if (!W || !C || !W->IsMissionActive())
		{
			return StepTime > 15.f;
		}
		if (!bClient)
		{
			return true;
		}
		const BRM::FPlan& P = W->GetMissionPlan();
		int32 D = INDEX_NONE;
		for (int32 I = 0; I < P.NumDevices && D == INDEX_NONE; ++I)
		{
			D = P.Devices[I].Kind == BRM::EKind::Clue ? I : INDEX_NONE;
		}
		ABRMissionDevice* Dev = D != INDEX_NONE ? W->FindMissionDevice(D) : nullptr;
		if (!Dev)
		{
			Skip(TEXT("etat d'une demande : aucun indice dans le plan"));
			TestMissionDevice = INDEX_NONE;
			return true;
		}
		if (!Dev->IsShown())
		{
			if (StepTime < 1.f)
			{
				PlaceAndFace(C, W->CellCenter(Dev->GetCell(), static_cast<float>(Dev->GetInteractPoint().Z)), Dev->GetInteractPoint());
			}
			return StepTime > 20.f;
		}
		const FVector Aim = Dev->GetInteractPoint();
		if (TestMissionPhase != 412)
		{
			// La position du client part a l'hote avec ses mouvements, pas avec la demande : apres un deplacement instantane,
			// la demande attend que l'hote ait recu la nouvelle position (sinon il juge la distance d'ou etait le client)
			PlaceAndFace(C, FVector(Aim.X, Aim.Y, C->GetActorLocation().Z) + Dev->GetActorForwardVector().GetSafeNormal2D() * 120.f, Aim);
			TestMissionPhase = 412;
			TestTimer = FPlatformTime::Seconds();
			return false;
		}
		if (FPlatformTime::Seconds() - TestTimer < 0.6)
		{
			return false;
		}
		TestMissionPhase = 0;
		TestMissionDevice = D;
		TestCountA = C->MissionResultsReceived;
		C->RequestMissionAction(D, static_cast<uint8>(BRM::EAction::Use));
		FString Detail;
		float Age = 0.f;
		const bool bPending = C->GetActionStatus(Detail, Age) == EBRActionStatus::Pending;
		Note(FString::Printf(TEXT("client : demande envoyee, etat affiche \"en attente\" %s"), YesNo(bPending)), !bPending);
		Shot(TEXT("v412_reseau_en_attente"));
		TestTimer = FPlatformTime::Seconds();
		return true;
	});
	Add(TEXT("v4.12 reseau : demande acceptee"), 0.f, [this, bClient]()
	{
		ABRCharacter* C = GetPlayer();
		if (!bClient || !C || TestMissionDevice == INDEX_NONE)
		{
			return true;
		}
		if (C->MissionResultsReceived == TestCountA && StepTime < 10.f)
		{
			return false;
		}
		FString Detail;
		float Age = 0.f;
		const EBRActionStatus S = C->GetActionStatus(Detail, Age);
		const bool bOk = C->MissionResultsReceived > TestCountA && S == EBRActionStatus::Accepted;
		Note(FString::Printf(TEXT("client : reponse en %.0f ms, etat affiche %d (attendu : accepte)%s"), (FPlatformTime::Seconds() - TestTimer) * 1000.0, static_cast<int32>(S),
			Detail.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", raison \"%s\""), *Detail)), !bOk);
		Shot(TEXT("v412_reseau_acceptee"));
		return true;
	});
	// Une demande refusee par l'hote (trop loin) : "refuse", avec la raison
	Add(TEXT("v4.12 reseau : demande refusee"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!bClient || !W || !C || TestMissionDevice == INDEX_NONE)
		{
			return true;
		}
		if (TestCountB == 0)
		{
			ABRMissionDevice* Dev = W->FindMissionDevice(TestMissionDevice);
			if (!Dev)
			{
				return true;
			}
			const FVector Far = Dev->GetInteractPoint() + Dev->GetActorForwardVector().GetSafeNormal2D() * 900.f;
			C->SetActorLocation(FVector(Far.X, Far.Y, C->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			TestTimer = FPlatformTime::Seconds();
			TestCountB = 2;
			return false;
		}
		if (TestCountB == 2)
		{
			// Demande une fois la nouvelle position recue par l'hote (voir plus haut)
			if (FPlatformTime::Seconds() - TestTimer < 0.6)
			{
				return false;
			}
			TestCountA = C->MissionResultsReceived;
			C->RequestMissionAction(TestMissionDevice, static_cast<uint8>(BRM::EAction::Use));
			TestCountB = 1;
			return false;
		}
		if (C->MissionResultsReceived == TestCountA && StepTime < 10.f)
		{
			return false;
		}
		TestCountB = 0;
		FString Detail;
		float Age = 0.f;
		const EBRActionStatus S = C->GetActionStatus(Detail, Age);
		const bool bOk = S == EBRActionStatus::Refused && !Detail.IsEmpty();
		Note(FString::Printf(TEXT("client : demande a 9 m : etat affiche %d (attendu : refuse), raison \"%s\""), static_cast<int32>(S), *Detail), !bOk);
		Shot(TEXT("v412_reseau_refusee"));
		return true;
	});

	// Depart de groupe par l'echelle du sas : l'hote grimpe (une seule demande au sommet, compte comme rassemble), le
	// client, loin, est attendu, puis arrive a pied
	Add(TEXT("v4.12 reseau : echelle"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || !W->IsMissionActive() || W->GetLevelNumber() != 37)
		{
			if (StepTime > 15.f)
			{
				Skip(TEXT("depart par une echelle : groupe absent du Niveau 37"));
				TestLevel = -1;
				return true;
			}
			return false;
		}
		TestCountA = W->GetLevelSerial();
		TestLevel = BRLevels::AdaptedForward(37, BRM::ForwardTarget(37, 0));
		ABRExit* Exit = TestLevel >= 0 ? FindExit(GetWorld(), TestLevel) : nullptr;
		if (!Exit || !Exit->IsClimbable())
		{
			Skip(TEXT("depart par une echelle : echelle de sortie introuvable"));
			TestLevel = -1;
			return true;
		}
		const FVector Fwd = FRotator(0.f, Exit->GetActorRotation().Yaw, 0.f).Vector();
		const float Half = C->GetSimpleCollisionHalfHeight();
		if (bClient)
		{
			// Le client s'eloigne (plus de 8 m du point de rassemblement) sur une cellule libre
			const FIntPoint Base = W->WorldToCell(Exit->GetActorLocation() + Fwd * 1400.f);
			PlaceAndFace(C, W->CellCenter(Base, static_cast<float>(Exit->GetActorLocation().Z) + Half + 40.f), Exit->GetActorLocation());
			return true;
		}
		W->DebugCompleteMission();
		PlaceAndFace(C, Exit->GetClimbAnchor() + Fwd * 90.f + FVector(0.f, 0.f, Half + 5.f), Exit->GetClimbAnchor() + FVector(0.f, 0.f, 60.f));
		TestCountB = C->ClimbRequestsSent;
		TestTimer = FPlatformTime::Seconds();
		TestMissionPhase = 0;
		return true;
	});
	Add(TEXT("v4.12 reseau : grimpeur au sommet"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || TestLevel < 0)
		{
			return true;
		}
		const FBRNetDeparture& Dep = W->GetDeparture();
		if (bClient)
		{
			if (Dep.Phase != 1 && StepTime < 30.f)
			{
				return false;
			}
			Note(FString::Printf(TEXT("client : depart en rassemblement recu %s (%d/%d)"), YesNo(Dep.Phase == 1), Dep.Ready, Dep.Needed), Dep.Phase != 1);
			TestTimer = FPlatformTime::Seconds();
			return true;
		}
		ABRExit* Exit = FindExit(GetWorld(), TestLevel);
		if (!Exit)
		{
			return true;
		}
		const double Now = FPlatformTime::Seconds();
		if (TestMissionPhase == 0)
		{
			// Interaction sur l'echelle visee, puis on monte en avancant
			Face(C, Exit->GetClimbAnchor() + FVector(0.f, 0.f, 60.f));
			if (C->FocusActor.Get() == Exit)
			{
				C->Interact();
				TestMissionPhase = 1;
				TestTimer = Now;
			}
			else if (StepTime > 3.f)
			{
				Note(TEXT("hote : echelle du sas pas visee par l'interaction"), true);
				TestLevel = -1;
				return true;
			}
			return false;
		}
		if (C->GetClimbWaitState() == BRGather::FClimbWait::EState::Climbing)
		{
			C->InputMove(FVector2D(0.f, 1.f));
			if (Now - TestTimer > 20.0)
			{
				Note(FString::Printf(TEXT("hote : sommet de l'echelle non atteint en 20 s (en train de grimper %s)"), YesNo(C->IsClimbing())), true);
				TestLevel = -1;
				return true;
			}
			return false;
		}
		// Au sommet : on attend 4 s sans toucher a rien ; une seule demande, l'hote se compte comme rassemble
		if (TestMissionPhase == 1)
		{
			TestMissionPhase = 2;
			TestTimer = Now;
		}
		if (Now - TestTimer < 4.0)
		{
			return false;
		}
		const int32 Requests = C->ClimbRequestsSent - TestCountB;
		const bool bGathered = W->IsGatheredForDeparture(C);
		const bool bOk = Requests == 1 && C->GetClimbWaitState() == BRGather::FClimbWait::EState::Waiting && Dep.Phase == 1 && bGathered && Dep.Ready < Dep.Needed;
		Note(FString::Printf(TEXT("hote au sommet : %d demande(s) en 4 s (attendu 1), attente stable %s, grimpeur compte comme rassemble %s, %d/%d rassembles"), Requests,
			YesNo(C->GetClimbWaitState() == BRGather::FClimbWait::EState::Waiting), YesNo(bGathered), Dep.Ready, Dep.Needed), !bOk);
		Shot(TEXT("v412_reseau_sommet_echelle"));
		TestTimer = Now;
		return true;
	});
	Add(TEXT("v4.12 reseau : coequipier a pied"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || TestLevel < 0)
		{
			return true;
		}
		if (!bClient)
		{
			return W->IsTransitioning() || W->GetLevelSerial() != TestCountA || StepTime > 90.f;
		}
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		if (TestMissionPhase != 31)
		{
			// Les deux machines avancent a leur rythme : l'hote mesure son attente 4 s apres etre arrive au sommet. Le client ne
			// part a pied qu'une fois l'hote monte (plus haut qu'au depart) et immobile depuis 6 s ; sinon le groupe partirait
			// avant la mesure (2/2 rassembles, depart lance)
			TArray<ABRCharacter*> Players;
			W->GetPlayers(Players);
			const ABRCharacter* Host = nullptr;
			for (const ABRCharacter* P : Players)
			{
				Host = P && !P->IsLocallyControlled() ? P : Host;
			}
			const double Now = FPlatformTime::Seconds();
			if (Host && StepTime < 45.f)
			{
				const FVector HostLoc = Host->GetActorLocation();
				if (TestMissionPhase != 30)
				{
					TestMissionPhase = 30;
					TestExitSpot = HostLoc;
					FallStart = HostLoc.Z;
					TestTimer = Now;
					return false;
				}
				if (FVector::Dist(HostLoc, TestExitSpot) > 5.f)
				{
					TestExitSpot = HostLoc;
					TestTimer = Now;
				}
				if (HostLoc.Z < FallStart + 100.f || Now - TestTimer < 6.0)
				{
					return false;
				}
			}
			const FVector Loc = W->GetDeparture().Location;
			BeginWalk(W, C, FVector(Loc.X, Loc.Y, C->GetActorLocation().Z), 250.f, true);
			TestMissionPhase = 31;
			return false;
		}
		if (W->IsTransitioning() || W->GetLevelSerial() != TestCountA)
		{
			// v4.15 : la marche du coequipier dans le rapport (nage comprise)
			Note(FString::Printf(TEXT("client : marche vers le point de rassemblement jusqu'au depart : %s"), *WalkStats()));
			TestMissionPhase = 0;
			return true;
		}
		const EWalk R = StepWalk(W, C, Dt, 75.0, C->GetScareKind() >= 0 || C->IsInputLocked());
		if ((R == EWalk::Moving || R == EWalk::Arrived) && StepTime < 90.f)
		{
			return false; // une fois rassemble, l'hote lance le depart
		}
		TestMissionPhase = 0;
		Note(FString::Printf(TEXT("client : point de rassemblement non atteint a pied (%s)"), *WalkStats()), true);
		return true;
	});
	LimitLastStep(110.f);
	Add(TEXT("v4.12 reseau : depart par l'echelle"), 1.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || TestLevel < 0)
		{
			return true;
		}
		const BRContent::FExitResolution Res = BRLevels::ResolveExit(37, TestLevel);
		if ((W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != Res.Target) && StepTime < 40.f)
		{
			return false;
		}
		const bool bOk = W->GetLevelNumber() == Res.Target && W->IsLevelReady();
		Note(FString::Printf(TEXT("%s : depart de groupe par l'echelle vers le Niveau %d %s"), bClient ? TEXT("client") : TEXT("hote"), Res.Target, bOk ? TEXT("effectue") : TEXT("ECHEC")), !bOk);
		return true;
	});
}
