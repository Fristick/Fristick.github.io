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
//     avancant) ; l'arrivee (niveau suivant, redirection ou fin du contenu disponible) est verifiee ;
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
#include "BRAutoTestNet.h"
#include "BRAutoTestWalk.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BRGatherLogic.h"
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
	/** v4.15 : la reprise a change la session de test (partie active, mode, histoire) : retablie par l'etape "retour a la
	 *  session de test", quelle que soit l'issue des etapes de reprise ; une reprise en echec saute la suivante */
	bool bRepriseSession = false;
	bool bRepriseFailed = false;
	BRStory::FState StoryMark;

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


	bool Replan(const ABRWorld* W, const ABRCharacter* C)
	{
		Walk.Points.Reset();
		Walk.Next = 0;
		TArray<FIntPoint> Path;
		if (!W->FindPath(W->WorldToCell(C->GetActorLocation()), W->WorldToCell(Walk.Goal), Path, 20000))
		{
			Walk.Points.Add(Walk.Goal);
			return false;
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
		return FString::Printf(TEXT("%.1f m en %.0f s, %d saut(s), %d nouveau(x) chemin(s), %d image(s) a la nage, %d deplacement(s) direct(s)%s"),
			Walk.Travelled / 100.f, FPlatformTime::Seconds() - Walk.Start, Walk.Jumps, Walk.Replans, Walk.SwimFrames, Walk.Teleports,
			Walk.bPath ? TEXT("") : TEXT(", sans chemin de grille (tout droit)"));
	}

	/** Une image de marche. bBusy : frayeur, entree bloquee, chargement (le temps ne compte pas pour le blocage) */
	EWalk StepWalk(const ABRWorld* W, ABRCharacter* C, float Dt, double Timeout, bool bBusy)
	{
		const double Now = FPlatformTime::Seconds();
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

	/** Sol sous (X, Y) dans l'eau (fond sous la surface) : le joueur y nagerait */
	bool IsWetSpot(const ABRWorld* W, const FVector& P, float FromZ, const AActor* Ignore, const AActor* Self, float* OutFloorZ = nullptr)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestWet), false, Self);
		Q.AddIgnoredActor(Ignore);
		FHitResult Floor;
		if (!W->GetWorld()->LineTraceSingleByChannel(Floor, FVector(P.X, P.Y, FromZ + 150.f), FVector(P.X, P.Y, FromZ - 450.f), ECC_WorldStatic, Q))
		{
			return true; // pas de sol : pas une place
		}
		const float FloorZ = static_cast<float>(Floor.ImpactPoint.Z);
		if (OutFloorZ)
		{
			*OutFloorZ = FloorZ;
		}
		const BRMech::FWaterQuery WQ = W->WaterAt(FVector(P.X, P.Y, FloorZ + 5.f));
		return WQ.bWater && WQ.Surface > FloorZ + 10.f;
	}

	/** Place au sec pres d'un mecanisme pose au bord de l'eau (fleche du courant des Poolrooms) : un joueur l'observe depuis
	 *  le trottoir, pas a la nage. A portee de l'hote (ABRWorld::MissionReachServer depuis les yeux) et a vue, la plus proche */
	bool DryStandNear(const ABRWorld* W, const ABRCharacter* C, const ABRMissionDevice* Dev, FVector& Out)
	{
		UWorld* World = W->GetWorld();
		const FVector Aim = Dev->GetInteractPoint();
		const FVector Fwd = Dev->GetActorForwardVector().GetSafeNormal2D().IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Dev->GetActorForwardVector().GetSafeNormal2D();
		const float Half = C->GetSimpleCollisionHalfHeight();
		const float EyeUp = static_cast<float>(C->GetEyeLocation().Z - C->GetActorLocation().Z);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestDryStand), false, C);
		Q.AddIgnoredActor(Dev);
		for (const float Dist : { 110.f, 160.f, 210.f, 260.f })
		{
			for (int32 Turn = 0; Turn < 16; ++Turn)
			{
				const FVector P = Aim + Fwd.RotateAngleAxis(Turn * 22.5f, FVector::UpVector) * Dist;
				float FloorZ = 0.f;
				if (IsWetSpot(W, P, static_cast<float>(Aim.Z), Dev, C, &FloorZ))
				{
					continue;
				}
				const FVector Stand(P.X, P.Y, FloorZ + Half + 5.f);
				if (World->OverlapAnyTestByChannel(Stand, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(36.f, Half - 4.f), Q))
				{
					continue;
				}
				const FVector Eye = Stand + FVector(0.f, 0.f, EyeUp);
				if (FVector::Dist(Eye, Aim) > ABRWorld::MissionReachServer - 40.f || World->LineTraceTestByChannel(Eye, Aim, ECC_WorldStatic, Q))
				{
					continue;
				}
				Out = Stand;
				return true;
			}
		}
		return false;
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
		if (!W->IsMissionActive() && StepTime < 10.f)
		{
			return false;
		}
		const uint8 Tier = W->GetMissionTier();
		ModulePlacements += Tier == 2 ? 1 : 0;
		PlacementLines.Add(FString::Printf(TEXT("Niveau %2d, graine %u : placement %s, avis %d, mission %s"), Level, UserSeed,
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
		Note(FString::Printf(TEXT("Niveau %d (canal %d) : mission %s, placement %d, passages condamnes %d, voyants rouges %d, verts %d"), Level,
			static_cast<int32>(BRLevels::Channel()), W->IsMissionActive() ? TEXT("active") : (bWantMission ? TEXT("ABSENTE") : TEXT("aucune (Level 0, v4.13)")), Tier, Sealed,
			Red, Green), W->IsMissionActive() != bWantMission);
		TestMission.Reset();
		TestMissionStep = 0;
		TestMissionPhase = 0;
		TestMissionRetries = 0;
		TestMissionDevice = -1;
		TestMissionLastHold = 0.0;
		TestMissionClock = 0.0;
		Shot(FString::Printf(TEXT("v412_acces_%d_depart"), Level));
		return true;
	});

	Add(FString::Printf(TEXT("v4.12 acces %d : mission resolue en marchant"), Level), 0.f, [this, Level]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || !W->IsMissionActive())
		{
			return true; // deja note
		}
		const BRM::FPlan& P = W->GetMissionPlan();
		const double Now = FPlatformTime::Seconds();
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		const bool bBusy = C->GetScareKind() >= 0 || C->IsInputLocked() || C->IsDead() || W->IsTransitioning();
		if (StepTime > 900.f)
		{
			ReleaseJump(C);
			Note(FString::Printf(TEXT("Niveau %d : mission interrompue apres 900 s (action %d/%d)"), Level, TestMissionStep, TestMission.Num() / 3), true);
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
			TestCountA = 0; // metres parcourus pour la mission
			TestTimer = Now;
			TestMissionPhase = 1;
			return false;
		}
		case 1:
		{
			// Mecanisme suivant : deja vise depuis la place actuelle (meme mecanisme), sinon on y marche
			if (TestMissionStep * 3 >= TestMission.Num())
			{
				Note(FString::Printf(TEXT("Niveau %d : %d actions faites en marchant (%d m, %.0f s)"), Level, TestMission.Num() / 3, TestCountA,
					Now - TestTimer));
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
			const FVector Aim = Dev->GetInteractPoint();
			FVector Goal = Aim + Dev->GetActorForwardVector().GetSafeNormal2D() * 110.f;
			// Devant le mecanisme, c'est l'eau (fleche du courant au bord du trottoir) : place au sec a portee
			FVector Dry;
			if (IsWetSpot(W, Goal, static_cast<float>(Aim.Z), Dev, C))
			{
				if (!DryStandNear(W, C, Dev, Dry))
				{
					Note(FString::Printf(TEXT("Niveau %d : mecanisme %d (%s) au bord de l'eau : aucune place au sec a portee et a vue"), Level, D,
						*BRMissionText::DeviceName(P, D)), true);
					return true;
				}
				Goal = Dry;
			}
			BeginWalk(W, C, FVector(Goal.X, Goal.Y, C->GetActorLocation().Z), 70.f, true);
			TestMissionPhase = 2;
			return false;
		}
		case 2:
		{
			const EWalk R = StepWalk(W, C, Dt, 240.0, bBusy);
			if (R == EWalk::Moving)
			{
				return false;
			}
			TestCountA += FMath::RoundToInt(Walk.Travelled / 100.f);
			if (R == EWalk::Stuck)
			{
				Note(FString::Printf(TEXT("Niveau %d : mecanisme %d (%s) NON ATTEINT a pied : bloque a la cellule (%d, %d), %s"), Level, TestMissionDevice,
					*BRMissionText::DeviceName(P, TestMissionDevice), W->WorldToCell(C->GetActorLocation()).X, W->WorldToCell(C->GetActorLocation()).Y,
					*WalkStats()), true);
				Shot(FString::Printf(TEXT("v412_acces_%d_bloque"), Level));
				return true;
			}
			TestMissionClock = Now;
			TestMissionPhase = 3;
			return false;
		}
		case 3:
		{
			// Le trace d'interaction (le meme que la touche) doit trouver le mecanisme depuis la place atteinte
			ABRMissionDevice* Dev = W->FindMissionDevice(TestMissionDevice);
			if (!Dev)
			{
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
				// Un pas plus pres (pas dans l'eau), puis on vise a nouveau
				const FVector Goal = Dev->GetInteractPoint() + Dev->GetActorForwardVector().GetSafeNormal2D() * 60.f;
				if (!IsWetSpot(W, Goal, static_cast<float>(Dev->GetInteractPoint().Z), Dev, C))
				{
					BeginWalk(W, C, FVector(Goal.X, Goal.Y, C->GetActorLocation().Z), 35.f, true);
					TestMissionPhase = 2;
					return false;
				}
			}
			const AActor* Seen = C->FocusActor.Get();
			Note(FString::Printf(TEXT("Niveau %d : mecanisme %d (%s) atteint mais PAS VISE par l'interaction (vise : %s)"), Level, TestMissionDevice,
				*BRMissionText::DeviceName(P, TestMissionDevice), Seen ? *Seen->GetName() : TEXT("rien")), true);
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
			if (ABRMissionDevice* Dev = W->FindMissionDevice(D))
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
				if (++TestMissionRetries > 8)
				{
					Note(FString::Printf(TEXT("Niveau %d : action %d refusee %d fois (retour %d)"), Level, TestMissionStep, TestMissionRetries, Got), true);
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
			TestMissionRetries = 0;
			++TestMissionStep;
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
			// Mission resolue, sortie ouverte (voyant vert), place devant la sortie
			BRM::FEval E;
			W->GetMissionEval(E);
			// v4.13 : sans mission (Level 0), la sortie est libre : la porte de service
			const bool bSolved = E.bSolved || !W->IsMissionActive();
			TestLevel = OpenForwardTarget(W, Level);
			ABRExit* Exit = TestLevel >= 0 ? FindExit(GetWorld(), TestLevel) : nullptr;
			if (!bSolved || !Exit)
			{
				Note(FString::Printf(TEXT("Niveau %d : mission resolue %s, sortie ouverte trouvee %s : sortie non testee"), Level, YesNo(bSolved), YesNo(Exit != nullptr)), true);
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
			const EWalk R = StepWalk(W, C, Dt, 300.0, bBusy);
			if (R == EWalk::Moving)
			{
				return false;
			}
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
		const int32 AlmondInv = BRTxn::Count(C->ToTxn(), static_cast<uint8>(EBRItem::AlmondWater));
		C->StorePickup(EBRItem::AlmondWater, false);
		// v4.15 : l'objet mis de cote est au joueur et compte (CountItem compte la reserve, comme le stock declare a l'hote),
		// mais il n'est pas range (pas de place) : l'inventaire seul ne change pas. Avant : CountItem attendu inchange (faux)
		const int32 AlmondAfter = C->CountItem(EBRItem::AlmondWater);
		const int32 AlmondInvAfter = BRTxn::Count(C->ToTxn(), static_cast<uint8>(EBRItem::AlmondWater));
		const bool bAside = C->CountRecovered() == AsideBefore + 1 && C->RecoveredPickups == Before + 1 && AlmondAfter == Almond + 1 && AlmondInvAfter == AlmondInv
			&& C->Recovered.Num() > 0 && C->Recovered.Last().Item == EBRItem::AlmondWater;
		const FString AsideInfo = FString::Printf(TEXT("place avant %d, mis de cote avant %d apres %d, compteur +%d, eaux %d -> %d, inventaire %d -> %d"), RoomBefore, AsideBefore,
			C->CountRecovered(), C->RecoveredPickups - Before, Almond, AlmondAfter, AlmondInv, AlmondInvAfter);
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
		if (!W || !C)
		{
			return true;
		}
		TestCountA = 0;
		if (W->GetLevelNumber() != 6 || W->GetMissionDevices().Num() == 0)
		{
			// v4.15 : pas au Niveau 6 (mort pendant la marche, retour au Level 0) : le controle suivant passait sans rien verifier
			Note(FString::Printf(TEXT("Tick des mecanismes masques : controle impossible, Niveau %d avec %d mecanisme(s) (attendu : Niveau 6)"), W->GetLevelNumber(),
				W->GetMissionDevices().Num()), true);
			TestCountA = -1;
			return true;
		}
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
		if (!W || TestCountA < 0)
		{
			TestCountA = 0;
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
			if (StepTime <= 10.f)
			{
				return false;
			}
			// v4.15 : un delai depasse est un probleme (avant : etape terminee sans rien dire, et toutes les verifications de
			// reprise disparaissaient du rapport)
			Note(TEXT("reprise : changement de niveau en cours depuis 10 s, reprise non preparee"), true);
			return true;
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
		// v4.15 : la reprise est une session d'histoire : son fil va jusqu'au Niveau 2 (Level 0, Niveau 1 et Poolrooms faits).
		// Sans cela, l'hote refusait le Niveau 2 (Poolrooms requis) et les verifications de reprise ne se faisaient pas
		{
			BRStory::FState St;
			BRStory::SetDone(St, 0);
			BRStory::SetDone(St, 1);
			BRStory::SetDone(St, BRStory::Poolrooms);
			St.Reached = BRStory::IndexOf(2);
			St.Resume = 2;
			S->SetStory(St);
		}
		OriginalBytes.Reset();
		UGameplayStatics::SaveGameToMemory(S, OriginalBytes);
		FFileHelper::SaveArrayToFile(OriginalBytes, *BRSaves::FilePath(BRSaves::SlotName(TestSlot)));
		TestOrigSlot = BRSaves::ActiveSlot();
		TestOrigSave = PC->ActiveSave;
		bTestDevSession = PC->bDevSession;
		StoryMark = W->GetStory();
		bRepriseSession = true;
		bRepriseFailed = false;
		BRSaves::ActiveSlot() = TestSlot;
		PC->ActiveSave = S;
		PC->bDevSession = false;
		W->SetStory(S->GetStory(), EBRSessionMode::Story, true);
		BRSaves::PendingResume() = S->Session;
		TestCountC = W->GetLevelSerial();
		W->RequestTransition(2, false, Seed);
		return true;
	});
	Add(TEXT("v4.12 reprise : mission non restauree"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!bRepriseSession)
		{
			return true; // reprise non preparee : deja signale par l'etape precedente
		}
		if (!W || !PC || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 2 || W->GetLevelSerial() == TestCountC)
		{
			if (StepTime <= 30.f)
			{
				return false;
			}
			// v4.15 : un delai depasse est un probleme (avant : etape terminee sans rien dire)
			FString Why;
			const bool bAllowed = W && W->CanEnterLevel(2, &Why);
			Note(FString::Printf(TEXT("reprise : Niveau 2 non charge en 30 s (niveau %d ; entree %s%s)"), W ? W->GetLevelNumber() : -1, bAllowed ? TEXT("permise") : TEXT("refusee : "),
				*Why), true);
			bRepriseFailed = true;
			return true;
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
		if (!bRepriseSession || bRepriseFailed)
		{
			return true; // la reprise precedente n'a pas eu lieu : deja signale
		}
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
		if (!bRepriseSession || bRepriseFailed)
		{
			return true;
		}
		if (!W || !PC || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 2 || W->GetLevelSerial() == TestCountC)
		{
			if (StepTime <= 30.f)
			{
				return false;
			}
			Note(FString::Printf(TEXT("reprise d'un placement d'une autre version : Niveau 2 non recharge en 30 s (niveau %d)"), W ? W->GetLevelNumber() : -1), true);
			bRepriseFailed = true; // v4.15 : une etape de reprise qui suivrait celle-ci est sautee (deja signale)
			return true;
		}
		const uint8 Notice = W->GetMissionNotice();
		const bool bKept = RestoredDevice >= 0 && W->IsMissionActive() && W->GetMissionState().Dev[RestoredDevice] != 0;
		Note(FString::Printf(TEXT("placement d'une autre version : avis %d (attendu 3 : mecanismes replaces), indice lu garde %s"), Notice, YesNo(bKept)), Notice != 3 || !bKept);
		Shot(TEXT("v412_reprise_mecanismes_replaces"));
		return true;
	});
	// v4.15 : la session de test (partie active, emplacement, mode, histoire, prefixe des fichiers) est retablie quelle que
	// soit l'issue des etapes de reprise (avant : seulement apres une reprise reussie ; un refus laissait le test en session
	// d'histoire avec la partie de test, et les chargements suivants etaient refuses ou faits sans leur graine). Seul endroit
	// qui la retablit : une etape de reprise ajoutee avant celle-ci commence par "if (!bRepriseSession || bRepriseFailed)
	// return true;", met bRepriseFailed a true sur un delai depasse (note PROBLEME) et ne retablit rien elle-meme
	Add(TEXT("v4.12 reprise : retour a la session de test"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (bRepriseSession)
		{
			BRSaves::Flush();
			BRSaves::ActiveSlot() = TestOrigSlot;
			if (PC)
			{
				PC->ActiveSave = TestOrigSave;
				PC->bDevSession = bTestDevSession;
			}
			TestOrigSave = nullptr;
			if (W && PC)
			{
				W->SetStory(StoryMark, PC->GetSessionMode());
			}
			bRepriseSession = false;
		}
		bRepriseFailed = false;
		DeleteSaveFiles();
		BRSaves::SetTestPrefix(FString());
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Carnet
	// v4.15 : le carnet est verifie sur un niveau a mission charge avec une graine fixe (avant : le niveau ou le test se
	// trouvait, souvent le Level 0 apres une mort au Niveau 6 : pas de colonne d'aide, controles impossibles)
	AddSeedLoad(4, 4113, TEXT("v4.12 carnet"));
	Add(TEXT("v4.12 carnet : ouverture"), 0.5f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		const ABRWorld* W = GetBRWorld();
		if (!PC || !W)
		{
			return false;
		}
		if (!W->IsMissionActive() || W->HasServiceDoor() || W->GetLevelNumber() != 4)
		{
			if (StepTime < 10.f)
			{
				return false;
			}
			Note(FString::Printf(TEXT("carnet : controle impossible, Niveau %d sans mission (attendu : Niveau 4, graine 4113)"), W->GetLevelNumber()), true);
			TestCountA = -1;
			return true;
		}
		TestCountA = 0;
		PC->SetInventoryOpen(true, static_cast<int32>(ABRHUD::ETab::Notebook));
		return true;
	});
	Add(TEXT("v4.12 carnet : colonne de gauche defilante"), 0.3f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr;
		const ABRWorld* W = GetBRWorld();
		if (!H || !W)
		{
			return false;
		}
		if (TestCountA < 0)
		{
			TestCountA = 0;
			TestLanguage = BRLoc::Current().Code;
			return true;
		}
		if (TestCountA == 0)
		{
			// Le carnet de ce niveau doit etre dessine (cle de l'aide : graine du niveau et niveau du plan), sinon l'aide
			// demandee irait a une cle perimee
			const uint32 HintSeed = W->GetSeed() ^ static_cast<uint32>(W->GetMissionPlan().Level * 7919);
			if (H->NotebookHintSeed != HintSeed || H->NotebookHintStep < 0)
			{
				if (StepTime < 5.f)
				{
					return false;
				}
				Note(FString::Printf(TEXT("carnet : controle impossible, Niveau %d sans etape de mission dessinee (etape %d)"), W->GetLevelNumber(), H->NotebookHintStep), true);
				TestLanguage = BRLoc::Current().Code;
				return true;
			}
			Note(FString::Printf(TEXT("carnet controle au Niveau %d (mission active, etape %d)"), W->GetLevelNumber(), H->NotebookHintStep));
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
			// v4.15 : la capture se fait a la fin de cette image : l'aide change a l'image suivante (sinon la capture montrait
			// deja l'aide ouverte)
			TestCountA = 2;
			return false;
		}
		if (TestCountA == 2)
		{
			// Aide de niveau 2 : la colonne defile d'elle-meme jusqu'a l'aide
			H->NotebookLeftScroll = 0.f;
			H->NotebookHint.FindOrAdd(H->NotebookHintStep) = 2;
			TestCountA = 3;
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
		// v4.15 : section propre au bilan : ses problemes ne sont plus ranges sous le dernier niveau charge (le Niveau 4 du
		// carnet, avant lui le Niveau 6)
		{
			FLevelReport R;
			R.Title = TEXT("v4.12 : bilan (placements des missions, acces par deplacement reel)");
			R.Scene = TEXT("v4.12");
			R.FirstLogLine = LogLineCount();
			Reports.Add(R);
		}
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

namespace BRTestV412Net
{
	/** v4.15 : deplacement instantane du client suivi jusqu'a ce que le serveur l'ait recu (BRTestNet) */
	BRTestNet::FPlacement Place;
	/** v4.15 : demande a 9 m : distance des yeux du client a l'indice au moment de la demande */
	float RequestDist = 0.f;
	/** v4.15 : hote : demandes du client deja servies (numeros vus dans l'historique du serveur) */
	TSet<uint16> SeenRequests;
	bool bHostWatch = false;
	/** v4.15 : coequipier a pied : debut de la marche et arrivee (temps de l'etape ; -1 : pas encore), place a l'arrivee,
	 *  budget de marche (sous le temps restant du depart) */
	float WalkT0 = 0.f;
	float ArrivedAt = -1.f;
	FVector ArrivedPos = FVector(0.0, 0.0, 0.0);
	float WalkBudget = 0.f;
	/** v4.15 : hote : rassemblement du client vu par le serveur (etat, changements notes, temps compte) */
	FString GatherState;
	int32 GatherChanges = 0;
	float CountedSeconds = 0.f;

	/** Portee d'un mecanisme telle que la juge l'hote (ABRWorld::ServerMissionAct) : distance des yeux, ligne de vue */
	FString ReachInfo(UWorld* World, const ABRCharacter* C, const ABRMissionDevice* Dev)
	{
		if (!World || !C || !Dev)
		{
			return TEXT("mecanisme ou joueur absent");
		}
		const FVector Eye = C->GetEyeLocation();
		const FVector Target = Dev->GetInteractPoint();
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestReach412), false, C);
		Q.AddIgnoredActor(Dev);
		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, Eye, Target, ECC_WorldStatic, Q);
		return FString::Printf(TEXT("yeux (%.0f, %.0f, %.0f), %.0f cm du point (portee %.0f), vue %s"), Eye.X, Eye.Y, Eye.Z, FVector::Dist(Eye, Target), ABRWorld::MissionReachServer,
			bBlocked ? *FString::Printf(TEXT("BLOQUEE par %s a %.0f cm"), *GetNameSafe(Hit.GetActor()), Hit.Distance) : TEXT("libre"));
	}

	/** Hote : le client est-il compte comme rassemble, et sinon quelle condition manque (zone de 8 m, etage, vue). Category :
	 *  la condition seule (sans les distances), pour ne noter que les changements */
	FString GatherInfo(UWorld* World, const ABRWorld* W, const ABRCharacter* M, FString& Category, bool& bCounted)
	{
		const FBRNetDeparture& Dep = W->GetDeparture();
		bCounted = W->IsGatheredForDeparture(M);
		if (Dep.Phase != 1)
		{
			Category = FString::Printf(TEXT("plus de depart en cours (phase %d)"), static_cast<int32>(Dep.Phase));
			return Category;
		}
		BRGather::FExitShape S;
		S.Style = static_cast<BRGather::EStyle>(FMath::Min<uint8>(Dep.Style, 2));
		S.Foot = { static_cast<float>(Dep.Foot.X), static_cast<float>(Dep.Foot.Y), static_cast<float>(Dep.Foot.Z) };
		S.Forward = { static_cast<float>(Dep.Forward.X), static_cast<float>(Dep.Forward.Y), static_cast<float>(Dep.Forward.Z) };
		S.Anchor = { static_cast<float>(Dep.Anchor.X), static_cast<float>(Dep.Anchor.Y), static_cast<float>(Dep.Anchor.Z) };
		S.TopZ = Dep.TopZ;
		const FVector L = M->GetActorLocation();
		const BRGather::FVec Pos = { static_cast<float>(L.X), static_cast<float>(L.Y), static_cast<float>(L.Z) };
		const BRGather::FVec VP = BRGather::ViewPoint(S);
		const float Dist = static_cast<float>(FVector::Dist2D(L, Dep.Location));
		const float DZ = static_cast<float>(L.Z) - (S.Foot.Z + BRGather::CapsuleHalf);
		const BRGather::EGather G = BRGather::Classify(S, Pos);
		if (bCounted)
		{
			Category = TEXT("compte comme rassemble");
		}
		else if (G == BRGather::EGather::No)
		{
			Category = Dist >= BRGather::Radius ? TEXT("hors de la zone (plus de 8 m)") : TEXT("autre etage");
		}
		else
		{
			// Meme vue que l'hote (ABRWorld::IsGatheredForDeparture) : la sortie elle-meme ne compte pas
			FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestGatherView), false, M);
			if (const ABRExit* Exit = BRTestV412::FindExit(World, Dep.Target))
			{
				Q.AddIgnoredActor(Exit);
			}
			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(Hit, M->GetEyeLocation(), FVector(VP.X, VP.Y, VP.Z), ECC_WorldStatic, Q);
			Category = bBlocked ? FString::Printf(TEXT("dans la zone, vue coupee par %s (%s)"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()))
				: FString(TEXT("dans la zone, vue libre, mais non compte"));
		}
		return FString::Printf(TEXT("%s (en (%.0f, %.0f, %.0f), %.1f m du point, ecart vertical %.0f cm ; %d/%d rassembles, %.0f s restantes)"), *Category, L.X, L.Y, L.Z,
			Dist / 100.f, DZ, Dep.Ready, Dep.Needed, W->GetDepartureRemaining());
	}

	/** Client : une place libre autour de Around (rayons R0, R0 + 100, R0 + 200, R0 - 50 cm ; au moins MinDist a
	 *  l'horizontale) : cellule praticable, sol a FootZ (60 cm pres), capsule libre (murs seuls), Aim visible a hauteur des
	 *  yeux. Ignore (sortie, mecanisme) ne compte pas */
	bool FarStandAround(UWorld* World, const ABRWorld* W, const ABRCharacter* C, const AActor* Ignore, const FVector& Around, const FVector& Aim, float FootZ, float R0,
		float MinDist, FVector& Out)
	{
		const float Half = C->GetSimpleCollisionHalfHeight();
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestFarStand), false, C);
		Q.AddIgnoredActor(Ignore);
		FCollisionResponseParams WallsOnly;
		WallsOnly.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		for (const float R : { R0, R0 + 100.f, R0 + 200.f, R0 - 50.f })
		{
			for (int32 K = 0; K < 16; ++K)
			{
				const FVector P = Around + FVector(1.f, 0.f, 0.f).RotateAngleAxis(K * 22.5f, FVector::UpVector) * R;
				const FIntPoint Cell = W->WorldToCell(P);
				if (!W->IsWalkable(Cell))
				{
					continue;
				}
				const FVector Center = W->CellCenter(Cell, FootZ);
				if (FVector::Dist2D(Center, Around) < MinDist)
				{
					continue;
				}
				FHitResult Floor;
				if (!World->LineTraceSingleByChannel(Floor, Center + FVector(0.f, 0.f, 120.f), Center - FVector(0.f, 0.f, 120.f), ECC_WorldStatic, Q, WallsOnly)
					|| FMath::Abs(Floor.ImpactPoint.Z - FootZ) > 60.f)
				{
					continue;
				}
				const FVector Stand(Center.X, Center.Y, Floor.ImpactPoint.Z + Half + 5.f);
				if (World->OverlapAnyTestByChannel(Stand, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(36.f, Half - 4.f), Q, WallsOnly))
				{
					continue;
				}
				if (World->LineTraceTestByChannel(Stand + FVector(0.f, 0.f, Half * 0.75f), Aim, ECC_WorldStatic, Q, WallsOnly))
				{
					continue;
				}
				Out = Stand;
				return true;
			}
		}
		return false;
	}

	/** Client : une place a 9-12 m du point de rassemblement d'une echelle (hors de la zone de 8 m), au meme etage, d'ou le
	 *  point se voit : la marche vers le point est courte et sans detour */
	bool FarStandInSight(UWorld* World, const ABRWorld* W, const ABRCharacter* C, const ABRExit* Exit, const FVector& Gather, FVector& Out)
	{
		return FarStandAround(World, W, C, Exit, Gather, Gather + FVector(0.f, 0.f, BRGather::ViewHeight), static_cast<float>(Exit->GetActorLocation().Z), 1000.f, 900.f, Out);
	}
}

void ABRAutoTest::AddNetV412Steps(bool bClient)
{
	using namespace BRTestV412;
	using namespace BRTestV412Net;

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
			// la demande attend que l'hote ait recu la nouvelle position (sinon il juge la distance d'ou etait le client).
			// v4.15 : mouvements en attente envoyes avant le deplacement, place verifiee sur plusieurs images (BRTestNet)
			Place.Start(C, FVector(Aim.X, Aim.Y, C->GetActorLocation().Z) + Dev->GetActorForwardVector().GetSafeNormal2D() * 120.f, &Aim, FPlatformTime::Seconds());
			TestMissionPhase = 412;
			TestTimer = FPlatformTime::Seconds();
			return false;
		}
		if (!Place.Update(C, FPlatformTime::Seconds()) && FPlatformTime::Seconds() - TestTimer < 10.0)
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
	// Une demande refusee par l'hote (trop loin) : "refuse", avec la raison. v4.15 : le client reste vraiment a 9 m (place
	// verifiee, deplacement recu par le serveur) ; l'hote note chaque demande du client qu'il sert et d'ou il voit le client
	Add(TEXT("v4.12 reseau : demande refusee"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!bClient)
		{
			ABRCharacter* M = W ? BRTestNet::RemoteMate(W) : nullptr;
			if (!W || !M || !W->IsMissionActive())
			{
				bHostWatch = false;
				return true;
			}
			const BRM::FPlan& P = W->GetMissionPlan();
			int32 D = INDEX_NONE;
			for (int32 I = 0; I < P.NumDevices && D == INDEX_NONE; ++I)
			{
				D = P.Devices[I].Kind == BRM::EKind::Clue ? I : INDEX_NONE;
			}
			if (!bHostWatch)
			{
				SeenRequests.Reset();
				for (const ABRCharacter::FBRTxnRecord& R : M->ServerMissionHistory)
				{
					SeenRequests.Add(R.RequestId);
				}
				bHostWatch = true;
			}
			for (const ABRCharacter::FBRTxnRecord& R : M->ServerMissionHistory)
			{
				if (R.RequestId != 0 && !SeenRequests.Contains(R.RequestId))
				{
					SeenRequests.Add(R.RequestId);
					Note(FString::Printf(TEXT("hote : demande %d du client servie, retour %d%s ; client vu par l'hote : %s"), static_cast<int32>(R.RequestId), static_cast<int32>(R.Item),
						R.Item == BRMissionText::TooFar ? TEXT(" (trop loin)") : TEXT(""), *ReachInfo(GetWorld(), M, D != INDEX_NONE ? W->FindMissionDevice(D) : nullptr)));
				}
			}
			if (BRTestNet::RemoteMark(W) < BRTestNet::MarkV412Refused && StepTime < 40.f)
			{
				return false;
			}
			bHostWatch = false;
			return true;
		}
		if (!W || !C || TestMissionDevice == INDEX_NONE)
		{
			BRTestNet::SendMark(GetPC(), BRTestNet::MarkV412Refused);
			return true;
		}
		ABRMissionDevice* Dev = W->FindMissionDevice(TestMissionDevice);
		if (!Dev)
		{
			BRTestNet::SendMark(GetPC(), BRTestNet::MarkV412Refused);
			return true;
		}
		const double Now = FPlatformTime::Seconds();
		if (TestCountB == 0)
		{
			// v4.15 : jamais dans l'image qui suit une capture (saccade : le deplacement pouvait etre defait par la fusion des
			// mouvements du client)
			if (StepTime < 0.2f)
			{
				return false;
			}
			// v4.15 : une vraie place a 9-10 m de l'indice (cellule praticable, sol au meme etage, capsule libre, indice en vue,
			// bien au-dela de la portee de l'hote) ; a defaut, 9 m devant lui (la tenue de la place est verifiee ensuite)
			const FVector Aim = Dev->GetInteractPoint();
			const float Half = C->GetSimpleCollisionHalfHeight();
			FVector Stand;
			if (!FarStandAround(GetWorld(), W, C, Dev, Aim, Aim, static_cast<float>(C->GetActorLocation().Z) - Half, 900.f, 850.f, Stand))
			{
				const FVector Far = Aim + Dev->GetActorForwardVector().GetSafeNormal2D() * 900.f;
				Stand = FVector(Far.X, Far.Y, C->GetActorLocation().Z);
			}
			Place.Start(C, Stand, &Aim, Now);
			TestTimer = Now;
			TestCountB = 2;
			return false;
		}
		if (TestCountB == 2)
		{
			// Demande une fois la nouvelle position recue par l'hote : le personnage est reste a sa place 0,6 s
			const bool bHeld = Place.Update(C, Now);
			if (!bHeld && Now - TestTimer < 10.0)
			{
				return false;
			}
			RequestDist = static_cast<float>(FVector::Dist(C->GetEyeLocation(), Dev->GetInteractPoint()));
			if (!bHeld || RequestDist < 800.f)
			{
				// v4.15 : faute du test, pas de l'hote : sans place tenue loin de l'indice, la demande ne juge pas sa
				// verification de distance (avant : demande faite quand meme, et le refus attendu manquait)
				TestCountB = 0;
				Note(FString::Printf(TEXT("client : demande a 9 m non faite : client pas tenu a 9 m (place %s en 10 s, %.1f m de l'indice, %d replacement(s))"), bHeld ? TEXT("tenue") : TEXT("NON tenue"),
					RequestDist / 100.f, Place.Reapplied), true);
				BRTestNet::SendMark(GetPC(), BRTestNet::MarkV412Refused);
				return true;
			}
			TestCountA = C->MissionResultsReceived;
			C->RequestMissionAction(TestMissionDevice, static_cast<uint8>(BRM::EAction::Use));
			// v4.15 : la reponse est attendue 10 s a partir de la demande (avant : depuis le debut de l'etape, placement
			// compris : il pouvait ne rester aucune image pour la reponse)
			TestTimer = Now;
			TestCountB = 1;
			return false;
		}
		if (C->MissionResultsReceived == TestCountA && Now - TestTimer < 10.0)
		{
			return false;
		}
		TestCountB = 0;
		FString Detail;
		float Age = 0.f;
		const EBRActionStatus S = C->GetActionStatus(Detail, Age);
		const bool bOk = S == EBRActionStatus::Refused && !Detail.IsEmpty();
		// Le client etait verifie loin : une demande acceptee serait une vraie faute de l'hote (distance non verifiee)
		Note(FString::Printf(TEXT("client : demande a 9 m : etat affiche %d (attendu : refuse), raison \"%s\" ; client a %.1f m de l'indice a la demande (%d replacement(s)), %.1f m a la reponse"),
			static_cast<int32>(S), *Detail, RequestDist / 100.f, Place.Reapplied, FVector::Dist(C->GetEyeLocation(), Dev->GetInteractPoint()) / 100.f), !bOk);
		Shot(TEXT("v412_reseau_refusee"));
		BRTestNet::SendMark(GetPC(), BRTestNet::MarkV412Refused);
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
			// Le client s'eloigne (plus de 8 m du point de rassemblement). v4.15 : a 9-12 m, au meme etage, a vue du point
			// (la marche qui suit est courte), place verifiee et recue par l'hote ; jamais dans l'image qui suit une capture
			const double Now = FPlatformTime::Seconds();
			const FVector ExitLoc = Exit->GetActorLocation();
			const FVector Gather = ExitLoc + Fwd * 100.f; // point de rassemblement d'une echelle (BRGather::GatherPoint)
			if (TestMissionPhase != 4120)
			{
				if (StepTime < 0.2f)
				{
					return false;
				}
				FVector Stand;
				if (!FarStandInSight(GetWorld(), W, C, Exit, Gather, Stand))
				{
					const FIntPoint Base = W->WorldToCell(ExitLoc + Fwd * 1400.f);
					Stand = W->CellCenter(Base, static_cast<float>(ExitLoc.Z) + Half + 40.f);
				}
				Place.Start(C, Stand, &ExitLoc, Now);
				TestMissionPhase = 4120;
				return false;
			}
			if (!Place.Update(C, Now, 0.3) && StepTime < 5.f)
			{
				return false;
			}
			TestMissionPhase = 0;
			Note(FString::Printf(TEXT("client : place a %.1f m du point de rassemblement de l'echelle (attendu : plus de 8 m), %d replacement(s)"),
				FVector::Dist2D(C->GetActorLocation(), Gather) / 100.f, Place.Reapplied));
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
			if (Dep.Phase != 1)
			{
				TestLevel = -1; // v4.15 : un seul probleme, les etapes qui en dependent sont sautees
			}
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
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		const FBRNetDeparture& Dep = W->GetDeparture();
		if (!bClient)
		{
			// v4.15 : l'hote note si (et pourquoi pas) il compte le client comme rassemble, a chaque changement
			const bool bDone = W->IsTransitioning() || W->GetLevelSerial() != TestCountA || StepTime > 90.f;
			if (ABRCharacter* M = BRTestNet::RemoteMate(W))
			{
				FString Category;
				bool bCounted = false;
				const FString Info = GatherInfo(GetWorld(), W, M, Category, bCounted);
				CountedSeconds += bCounted ? Dt : 0.f;
				if (Category != GatherState)
				{
					if (GatherChanges < 12)
					{
						Note(FString::Printf(TEXT("hote : client %s, a %.1f s"), *Info, StepTime));
					}
					++GatherChanges;
					GatherState = Category;
				}
			}
			if (bDone)
			{
				const bool bLaunched = W->IsTransitioning() || W->GetLevelSerial() != TestCountA;
				// v4.15 : depart non lance : le seul probleme de l'hote (l'etape du depart par l'echelle, qui en depend, est
				// sautee au lieu d'ajouter un ECHEC 40 s plus tard)
				Note(FString::Printf(TEXT("hote : client compte comme rassemble pendant %.1f s, %d changement(s) d'etat ; depart %s"), CountedSeconds, GatherChanges,
					bLaunched ? TEXT("lance") : TEXT("NON lance en 90 s")), !bLaunched);
				if (!bLaunched)
				{
					TestLevel = -1;
				}
				GatherState.Reset();
				GatherChanges = 0;
				CountedSeconds = 0.f;
			}
			return bDone;
		}
		if (TestMissionPhase != 31)
		{
			// Les deux machines avancent a leur rythme : l'hote mesure son attente 4 s apres etre arrive au sommet (le depart
			// est demande a son arrivee). v4.15 : le client part a pied d'apres l'age du depart vu par l'hote (replique), sans
			// reference prise a l'entree de l'etape : depart en rassemblement depuis 6 s au moins (84 s restantes sur 90),
			// grimpeur compte. Avant : "hote monte et immobile depuis 6 s", jamais vrai si l'hote etait deja en haut (45 s
			// perdues sur les 90 du depart)
			if (Dep.Phase == 2)
			{
				return true; // le groupe part deja : l'etape suivante verifie l'arrivee
			}
			if (Dep.Phase != 1)
			{
				if (StepTime < 10.f)
				{
					return false;
				}
				Note(FString::Printf(TEXT("client : plus de depart en cours avant la marche (phase %d, raison %d)"), static_cast<int32>(Dep.Phase), static_cast<int32>(Dep.Reason)), true);
				TestLevel = -1;
				return true;
			}
			if (Dep.Ready < 1 || W->GetDepartureRemaining() > 84.f)
			{
				if (StepTime < 45.f)
				{
					return false;
				}
				Note(FString::Printf(TEXT("client : depart pas pret pour la marche en 45 s (%d/%d rassembles, %.0f s restantes)"), Dep.Ready, Dep.Needed, W->GetDepartureRemaining()), true);
				TestLevel = -1;
				return true;
			}
			const FVector Loc = Dep.Location;
			BeginWalk(W, C, FVector(Loc.X, Loc.Y, C->GetActorLocation().Z), 250.f, true);
			// La marche tient dans le temps restant du depart
			WalkBudget = FMath::Max(10.f, W->GetDepartureRemaining() - 3.f);
			WalkT0 = StepTime;
			ArrivedAt = -1.f;
			Note(FString::Printf(TEXT("client : marche vers le point de rassemblement depuis (%.0f, %.0f, %.0f), a %.1f m, depart : %d/%d rassembles, %.0f s restantes"),
				C->GetActorLocation().X, C->GetActorLocation().Y, C->GetActorLocation().Z, FVector::Dist2D(C->GetActorLocation(), Loc) / 100.f, Dep.Ready, Dep.Needed,
				W->GetDepartureRemaining()));
			TestMissionPhase = 31;
			return false;
		}
		if (W->IsTransitioning() || W->GetLevelSerial() != TestCountA)
		{
			TestMissionPhase = 0;
			Note(FString::Printf(TEXT("client : depart lance %.1f s apres le debut de la marche (arrive au point en %s)"), StepTime - WalkT0,
				ArrivedAt >= 0.f ? *FString::Printf(TEXT("%.1f s"), ArrivedAt) : TEXT("cours de marche")));
			return true;
		}
		const EWalk R = StepWalk(W, C, Dt, WalkBudget, C->GetScareKind() >= 0 || C->IsInputLocked());
		if (R == EWalk::Arrived && ArrivedAt < 0.f)
		{
			ArrivedAt = StepTime - WalkT0;
			ArrivedPos = C->GetActorLocation();
		}
		// Depart lance (phase 2) : on attend la transition ; annule, expire ou disparu : fin de la marche
		if (Dep.Phase == 2 && StepTime - WalkT0 < WalkBudget + 15.f)
		{
			return false;
		}
		const bool bExpired = Dep.Phase != 1 || W->GetDepartureRemaining() <= 0.f;
		if ((R == EWalk::Moving || R == EWalk::Arrived) && !bExpired && StepTime - WalkT0 < WalkBudget + 2.f)
		{
			return false; // une fois rassemble, l'hote lance le depart
		}
		TestMissionPhase = 0;
		const FString DepInfo = FString::Printf(TEXT("depart : phase %d, %d/%d rassembles, %.0f s restantes"), static_cast<int32>(Dep.Phase), Dep.Ready, Dep.Needed,
			W->GetDepartureRemaining());
		if (ArrivedAt >= 0.f)
		{
			// Arrive mais le groupe n'est pas parti : l'hote ne l'a pas compte (voir ses notes "hote : client ...")
			Note(FString::Printf(TEXT("client : arrive au point de rassemblement a pied en %.1f s (en (%.0f, %.0f, %.0f), a %.1f m du point) mais non compte par l'hote (%s ; %s)"),
				ArrivedAt, ArrivedPos.X, ArrivedPos.Y, ArrivedPos.Z, FVector::Dist2D(ArrivedPos, Dep.Location) / 100.f, *DepInfo, *WalkStats()), true);
		}
		else
		{
			Note(FString::Printf(TEXT("client : point de rassemblement non atteint a pied (%s ; %s ; en (%.0f, %.0f, %.0f))"), *WalkStats(), *DepInfo, C->GetActorLocation().X,
				C->GetActorLocation().Y, C->GetActorLocation().Z), true);
		}
		// v4.15 : un seul probleme : l'etape du depart par l'echelle, qui en depend, est sautee (avant : ECHEC 40 s plus tard)
		TestLevel = -1;
		return true;
	});
	LimitLastStep(120.f);
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
		const FBRNetDeparture& Dep = W->GetDeparture();
		Note(FString::Printf(TEXT("%s : depart de groupe par l'echelle vers le Niveau %d %s%s"), bClient ? TEXT("client") : TEXT("hote"), Res.Target, bOk ? TEXT("effectue") : TEXT("ECHEC"),
			bOk ? TEXT("") : *FString::Printf(TEXT(" (depart : phase %d, raison %d, %d/%d rassembles)"), static_cast<int32>(Dep.Phase), static_cast<int32>(Dep.Reason), Dep.Ready,
			Dep.Needed)), !bOk);
		return true;
	});
}
