// v4.11 : etapes du test automatique propres a la v4.11 (-BRAutoTest -BRAutoTestV411 ; reseau : -BRNetTest).
// Chaque etape verifie une regle de la v4.11 dans le jeu reel :
//   - missions : pour chacun des douze niveaux (graine fixe), sortie de progression verrouillee tant que la mission n'est
//     pas resolue ; solution calculee par le solveur qui n'utilise que ce que le joueur voit (BRMissionSolver, le meme que
//     le banc hors moteur), puis rejouee action par action par de vraies demandes (joueur place devant le mecanisme,
//     distance, ligne de vue, rythme des manivelles et retour verifies par l'hote) ; sortie ouverte ensuite ; depart
//     (Niveau 2) et fin de campagne (Niveau 11) ;
//   - ancienne partie (format 3) : migree, reprise en ancien mode (cassettes VHS) jusqu'a la sortie du niveau, mission
//     au niveau suivant, version de generation ecrite dans la sauvegarde ;
//   - ramassage : deux demandes du meme objet dans la meme image, un seul objet recu ; objet trop loin refuse ;
//   - creatures : choix de cible (vu, entendu, cache, memoire, hysteresis), budget de menace, bruit qui attire ;
//   - carnet : onglet, aide progressive (trois niveaux) ; sous-titres (direction) ; volumes des effets et des voix.
// Reseau : etat de mission identique a la connexion, action d'un client validee par l'hote, ramassage dispute (un seul
// gagnant), soin perime refuse sans rien consommer, depart de groupe (attente du coequipier, puis depart commun).
// Une verification qui ne peut pas etre faite ici est notee "NON VERIFIE", jamais comptee comme reussie.
#include "BRAutoTest.h"
#include "BRAutoTestNet.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BREntity.h"
#include "BRHUD.h"
#include "BRInteractables.h"
#include "BRLevels.h"
#include "BRLidarLogic.h"
#include "BRLoc.h"
#include "BRMission.h"
#include "BRMissionSolver.h"
#include "BRPlayerController.h"
#include "BRSave.h"
#include "BRWorld.h"
#include "AudioDevice.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Sound/SoundWave.h"

namespace BRM = BRMission;

namespace BRTestV411
{
	const TCHAR* const SavePrefix = TEXT("BR_AutoTestV411_");
	constexpr int32 LegacySlot = 0;
	/** Reglage des sous-titres du joueur, retabli a la fin des etapes de sous-titres */
	bool bPlayerSubtitles = true;
	/** v4.15 : niveau d'aide en cours de verification dans le carnet (compteur propre a ces etapes : TestCountA venait de
	 *  l'etape de ramassage, l'aide commencait au niveau 1) ; capture prise avant le changement suivant */
	int32 NotebookStep = 0;
	bool bNotebookShot = false;

	const TCHAR* YesNo(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	void DeleteSaveFiles()
	{
		for (int32 Slot = 0; Slot < BRSaves::MaxSlots; ++Slot)
		{
			for (const FString& Name : { BRSaves::SlotName(Slot), BRSaves::BackupSlotName(Slot), BRSaves::UnreadableSlotName(Slot),
				BRSaves::LegacySlotName(Slot, 1), BRSaves::LegacySlotName(Slot, 2), BRSaves::LegacySlotName(Slot, 3) })
			{
				IFileManager::Get().Delete(*BRSaves::FilePath(Name), false, true, true);
			}
		}
	}

	/** Une place debout pres d'un point vise : sol trouve sous la place, capsule libre, point visible des yeux.
	 *  Prefer : direction essayee d'abord (devant un mecanisme : son +X local) */
	bool StandNear(UWorld* World, const FVector& Target, const FVector& Prefer, float HalfHeight, const AActor* Ignore, const AActor* Self, FVector& Out)
	{
		if (!World)
		{
			return false;
		}
		const FVector Dir0 = Prefer.GetSafeNormal2D().IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Prefer.GetSafeNormal2D();
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestStand), false, Self);
		if (Ignore)
		{
			Q.AddIgnoredActor(Ignore);
		}
		for (const float Dist : { 120.f, 170.f, 230.f, 300.f })
		{
			for (int32 Turn = 0; Turn < 8; ++Turn)
			{
				const FVector Dir = Dir0.RotateAngleAxis(Turn * 45.f, FVector::UpVector);
				const FVector P = Target + Dir * Dist;
				FHitResult Floor;
				if (!World->LineTraceSingleByChannel(Floor, FVector(P.X, P.Y, Target.Z + 60.f), FVector(P.X, P.Y, Target.Z - 450.f), ECC_WorldStatic, Q))
				{
					continue;
				}
				const FVector Stand(P.X, P.Y, Floor.ImpactPoint.Z + HalfHeight + 5.f);
				if (World->OverlapAnyTestByChannel(Stand, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(36.f, HalfHeight - 4.f), Q))
				{
					continue;
				}
				const FVector Eye = Stand + FVector(0.f, 0.f, HalfHeight * 0.75f);
				if (World->LineTraceTestByChannel(Eye, Target, ECC_WorldStatic, Q))
				{
					continue;
				}
				Out = Stand;
				return true;
			}
		}
		return false;
	}

	/** Place le joueur (sans vitesse), le regard vers Target */
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

	/** Mode de deplacement pendant l'attente d'un chunk (pas de chute si le sol n'est pas encore construit) */
	void SetHover(ABRCharacter* C, bool bHover)
	{
		if (UCharacterMovementComponent* M = C ? C->GetCharacterMovement() : nullptr)
		{
			M->StopMovementImmediately();
			M->SetMovementMode(bHover ? MOVE_Flying : MOVE_Walking);
		}
	}

	FString FeedbackName(uint8 F)
	{
		switch (F)
		{
		case 0: return TEXT("aucun");
		case 1: return TEXT("Verrouille");
		case 2: return TEXT("ObjetManquant");
		case 3: return TEXT("MauvaisObjet");
		case 4: return TEXT("Surcharge");
		case 5: return TEXT("SansFusible");
		case 6: return TEXT("Faux");
		case 7: return TEXT("Avertissement");
		case 8: return TEXT("Disjonction");
		case 9: return TEXT("DejaFait");
		case 10: return TEXT("Fait");
		case 11: return TEXT("Progression");
		case 12: return TEXT("TrousseauPlein");
		case 13: return TEXT("Rendu");
		case 14: return TEXT("Ordre");
		case 15: return TEXT("HorsService");
		case BRMissionText::Cooldown: return TEXT("Rearmement");
		case BRMissionText::TooFar: return TEXT("TropLoin");
		case BRMissionText::NotNow: return TEXT("PasMaintenant");
		case BRMissionText::Taken: return TEXT("DejaPris");
		default: return FString::Printf(TEXT("%d"), F);
		}
	}

	/** Portee d'un mecanisme telle que la juge l'hote (ABRWorld::ServerMissionAct) : distance des yeux, ligne de vue */
	FString ReachInfo(UWorld* World, const ABRCharacter* C, const ABRMissionDevice* Dev)
	{
		if (!World || !C || !Dev)
		{
			return TEXT("mecanisme ou joueur absent");
		}
		const FVector Eye = C->GetEyeLocation();
		const FVector Target = Dev->GetInteractPoint();
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestReach), false, C);
		Q.AddIgnoredActor(Dev);
		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, Eye, Target, ECC_WorldStatic, Q);
		return FString::Printf(TEXT("yeux (%.0f, %.0f, %.0f), point (%.0f, %.0f, %.0f), %.0f cm (portee %.0f), vue %s"), Eye.X, Eye.Y, Eye.Z, Target.X, Target.Y, Target.Z,
			FVector::Dist(Eye, Target), ABRWorld::MissionReachServer, bBlocked ? *FString::Printf(TEXT("BLOQUEE par %s (%s) a %.0f cm"), *GetNameSafe(Hit.GetActor()),
			*GetNameSafe(Hit.GetComponent()), Hit.Distance) : TEXT("libre"));
	}

	/** Premiere sortie de cette cible (visible) */
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

	int32 CountExits(UWorld* World, int32 Target)
	{
		int32 N = 0;
		for (TActorIterator<ABRExit> It(World); It; ++It)
		{
			N += It->Target == Target ? 1 : 0;
		}
		return N;
	}

	TArray<ABRCharacter*> Mates(ABRWorld* W)
	{
		TArray<ABRCharacter*> All;
		TArray<ABRCharacter*> Out;
		if (W)
		{
			W->GetPlayers(All);
		}
		for (ABRCharacter* C : All)
		{
			if (C && !C->IsLocallyControlled())
			{
				Out.Add(C);
			}
		}
		return Out;
	}

	/** Objet commun aux deux machines : le plus petit identifiant parmi les objets charges (meme graine, meme liste) */
	ABRPickup* SharedPickup(UWorld* World)
	{
		ABRPickup* Best = nullptr;
		for (TActorIterator<ABRPickup> It(World); It; ++It)
		{
			if (It->Item != EBRItem::VHSTape && It->Item != EBRItem::Note && (!Best || It->Id < Best->Id))
			{
				Best = *It;
			}
		}
		return Best;
	}
}

void ABRAutoTest::AddMissionSteps(int32 Level, uint32 UserSeed, bool bDepart)
{
	using namespace BRTestV411;
	const FString Title = FString::Printf(TEXT("v4.11 mission du Niveau %d (graine %u)"), Level, UserSeed);
	AddSeedLoad(Level, UserSeed, Title);

	// Mission en place, mecanismes crees, sortie de progression verrouillee
	Add(FString::Printf(TEXT("v4.11 mission %d : sortie verrouillee"), Level), 0.5f, [this, Level]()
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
		if (!W->IsMissionActive())
		{
			Note(FString::Printf(TEXT("Niveau %d : aucune mission active (generation %d)"), Level, W->GetMissionGen()), true);
			return true;
		}
		const BRM::FPlan& P = W->GetMissionPlan();
		const int32 Target = BRM::ForwardTarget(Level, 0);
		FString Why;
		const bool bOpen = W->CanUseExit(Target, Why);
		const int32 Devices = W->GetMissionDevices().Num();
		const int32 Exits = CountExits(GetWorld(), Target);
		const bool bOk = !bOpen && !Why.IsEmpty() && Devices == P.NumDevices && Exits > 0 && !P.bFallback;
		Note(FString::Printf(TEXT("Niveau %d : generation %d, %d mecanismes (plan : %d), %d sortie(s) vers %d, verrouillee avant la mission : %s (\"%s\"), variante de secours : %s"),
			Level, W->GetMissionGen(), Devices, P.NumDevices, Exits, Target, YesNo(!bOpen), *Why, P.bFallback ? TEXT("OUI") : TEXT("non")), !bOk);
		TestMission.Reset();
		TestMissionStep = 0;
		TestMissionPhase = 0;
		TestMissionRetries = 0;
		TestMissionDevice = -1;
		TestMissionLastHold = 0.0;
		TestMissionClock = 0.0;
		Shot(FString::Printf(TEXT("v411_mission_%d_avant"), Level));
		return true;
	});

	// Solution par le solveur aux seules informations visibles, rejouee par de vraies demandes
	Add(FString::Printf(TEXT("v4.11 mission %d : resolution par interactions"), Level), 0.f, [this, Level]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || !W->IsMissionActive())
		{
			return true; // deja note
		}
		const BRM::FPlan& P = W->GetMissionPlan();
		const double Now = FPlatformTime::Seconds();
		if (StepTime > 240.f)
		{
			SetHover(C, false);
			Note(FString::Printf(TEXT("Niveau %d : resolution interrompue apres 240 s (action %d/%d)"), Level, TestMissionStep, TestMission.Num() / 3), true);
			return true;
		}
		switch (TestMissionPhase)
		{
		case 0:
		{
			// Le solveur part de l'etat courant de l'hote, avec la campagne de la partie
			BRM::FState S = W->GetMissionState();
			BRM::FSolver Solver(P, S, W->GetMissionCampaign());
			Solver.bPlayerActions = true;
			Solver.bRecord = true;
			const bool bSolved = Solver.Solve(0) && !Solver.bBad && !Solver.bOverflow && (S.Solved & 1);
			if (!bSolved)
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
			Note(FString::Printf(TEXT("Niveau %d : solution de %d actions (informations visibles seulement)"), Level, Solver.NumSteps));
			TestMissionPhase = 1;
			return false;
		}
		case 1:
		{
			// Placement devant le mecanisme de l'action suivante (meme mecanisme : on reste)
			if (TestMissionStep * 3 >= TestMission.Num())
			{
				TestMissionPhase = 4;
				return false;
			}
			const int32 D = TestMission[TestMissionStep * 3];
			ABRMissionDevice* Dev = W->FindMissionDevice(D);
			if (!Dev)
			{
				Note(FString::Printf(TEXT("Niveau %d : mecanisme %d absent du monde"), Level, D), true);
				return true;
			}
			if (D == TestMissionDevice && TestMissionRetries == 0)
			{
				TestMissionPhase = 3;
				return false;
			}
			TestMissionDevice = D;
			SetHover(C, true);
			FVector Stand;
			const FVector Aim = Dev->GetInteractPoint();
			if (!StandNear(GetWorld(), Aim, Dev->GetActorForwardVector(), C->GetSimpleCollisionHalfHeight(), Dev, C, Stand))
			{
				// Chunk pas encore construit : on s'approche par la cellule, puis on cherche a nouveau
				Stand = W->CellCenter(Dev->GetCell(), Aim.Z);
			}
			PlaceAndFace(C, Stand, Aim);
			TestMissionClock = Now;
			TestMissionPhase = 2;
			return false;
		}
		case 2:
		{
			// Attente du chunk (mecanisme affiche), puis place definitive et regard vers le mecanisme
			ABRMissionDevice* Dev = W->FindMissionDevice(TestMissionDevice);
			if (!Dev)
			{
				return true;
			}
			if (!Dev->IsShown() || Now - TestMissionClock < 0.4)
			{
				if (Now - TestMissionClock > 20.0)
				{
					SetHover(C, false);
					Note(FString::Printf(TEXT("Niveau %d : le chunk du mecanisme %d n'est pas construit apres 20 s"), Level, TestMissionDevice), true);
					return true;
				}
				return false;
			}
			FVector Stand;
			const FVector Aim = Dev->GetInteractPoint();
			if (!StandNear(GetWorld(), Aim, Dev->GetActorForwardVector(), C->GetSimpleCollisionHalfHeight(), Dev, C, Stand))
			{
				SetHover(C, false);
				Note(FString::Printf(TEXT("Niveau %d : aucune place libre devant le mecanisme %d (%s)"), Level, TestMissionDevice,
					*BRMissionText::DeviceName(P, TestMissionDevice)), true);
				return true;
			}
			PlaceAndFace(C, Stand, Aim);
			SetHover(C, false);
			TestMissionPhase = 3;
			return false;
		}
		case 3:
		{
			// Action : la meme demande que la touche d'interaction (hote : validation et application immediates)
			const int32 D = TestMission[TestMissionStep * 3];
			const uint8 Action = TestMission[TestMissionStep * 3 + 1];
			const uint8 Expected = TestMission[TestMissionStep * 3 + 2];
			const bool bHold = P.Devices[D].Kind == BRM::EKind::Observe || P.Devices[D].Kind == BRM::EKind::Crank;
			if (bHold && Now - TestMissionLastHold < 0.45)
			{
				return false; // l'hote compte une unite par 0,4 s au plus
			}
			if (Now - TestMissionClock < 0.05)
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
				Note(FString::Printf(TEXT("Niveau %d : action %d sans reponse de l'hote"), Level, TestMissionStep), true);
				return true;
			}
			const uint8 Got = C->LastMissionFeedback;
			const bool bItemTaken = P.Devices[D].Kind == BRM::EKind::Item && Expected == static_cast<uint8>(BRM::EFeedback::AlreadyDone) && Got == BRMissionText::Taken;
			if (Got == BRMissionText::Cooldown || Got == BRMissionText::NotNow || Got == BRMissionText::TooFar)
			{
				if (++TestMissionRetries > 8)
				{
					Note(FString::Printf(TEXT("Niveau %d : action %d refusee %d fois (%s)"), Level, TestMissionStep, TestMissionRetries, *FeedbackName(Got)), true);
					return true;
				}
				// Trop loin : nouvelle place ; rearmement ou niveau pas pret : attente
				TestMissionClock = Now + (Got == BRMissionText::Cooldown ? 1.0 : 0.3);
				TestMissionPhase = Got == BRMissionText::TooFar ? 1 : 3;
				return false;
			}
			if (Got != Expected && !bItemTaken)
			{
				Note(FString::Printf(TEXT("Niveau %d : action %d/%d sur %s : retour %s, attendu %s"), Level, TestMissionStep + 1, TestMission.Num() / 3,
					*BRMissionText::DeviceName(P, D), *FeedbackName(Got), *FeedbackName(Expected)), true);
				return true;
			}
			TestMissionRetries = 0;
			++TestMissionStep;
			TestMissionPhase = 1;
			return false;
		}
		default:
		{
			// Mission resolue, sortie ouverte, etat publie
			const int32 Target = BRM::ForwardTarget(Level, 0);
			FString Why;
			const bool bSolved = (W->GetMissionState().Solved & 1) != 0;
			const bool bOpen = W->CanUseExit(Target, Why);
			BRM::FEval E;
			W->GetMissionEval(E);
			bool bAllDone = true;
			for (int32 I = 0; I < E.NumSteps; ++I)
			{
				bAllDone &= E.Steps[I] == BRM::EStep::Done;
			}
			const bool bOk = bSolved && bOpen && bAllDone && E.bSolved;
			const FString Line = FString::Printf(TEXT("Niveau %2d : %d actions rejouees, mission resolue %s, sortie vers %d ouverte %s, etapes terminees %s, erreurs %d"),
				Level, TestMission.Num() / 3, YesNo(bSolved), Target, YesNo(bOpen), YesNo(bAllDone), W->GetMissionState().Mistakes);
			TestMissionLines.Add(Line);
			Note(Line, !bOk);
			Shot(FString::Printf(TEXT("v411_mission_%d_apres"), Level));
			return true;
		}
		}
	});
	LimitLastStep(260.f);

	if (bDepart)
	{
		// Seul : la sortie ouverte fait partir tout de suite (Niveau 11 : la fin de la campagne)
		Add(FString::Printf(TEXT("v4.11 mission %d : depart"), Level), 0.f, [this, Level]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			TestLevel = BRM::ForwardTarget(Level, 0);
			TestCountA = W->GetCampaign().Endings;
			ABRExit* Exit = FindExit(GetWorld(), TestLevel);
			W->RequestDeparture(C, TestLevel, Exit ? Exit->GetActorLocation() : C->GetActorLocation());
			TestTimer = FPlatformTime::Seconds();
			return true;
		});
		Add(FString::Printf(TEXT("v4.11 mission %d : arrivee"), Level), 1.f, [this, Level]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W)
			{
				return false;
			}
			const double Elapsed = FPlatformTime::Seconds() - TestTimer;
			if (TestLevel == BRM::EndingTarget)
			{
				if (!W->IsEndingShown() && Elapsed < 15.0)
				{
					return false;
				}
				const bool bEnding = W->IsEndingShown();
				const bool bRecorded = (W->GetCampaign().Endings & ~TestCountA) != 0;
				Note(FString::Printf(TEXT("fin de campagne : ecran de fin %s (variante %s), fin notee dans la campagne %s"), YesNo(bEnding),
					W->IsEndingVariant() ? TEXT("oui") : TEXT("non"), YesNo(bRecorded)), !bEnding || !bRecorded);
				Shot(TEXT("v411_fin"));
				if (bEnding)
				{
					W->CloseEnding(true); // continuer l'exploration
				}
				return true;
			}
			if ((W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != TestLevel) && Elapsed < 30.0)
			{
				return false;
			}
			const bool bOk = W->GetLevelNumber() == TestLevel && W->IsLevelReady();
			Note(FString::Printf(TEXT("depart du Niveau %d par la sortie ouverte : arrivee au Niveau %d %s"), Level, TestLevel, YesNo(bOk)), !bOk);
			return true;
		});
	}
}

void ABRAutoTest::AddV411Steps()
{
	using namespace BRTestV411;
	Add(TEXT("v4.11 : preparation"), 0.f, [this]()
	{
		FLevelReport R;
		R.Title = TEXT("v4.11 : missions, transactions, creatures, carnet, sous-titres, volumes");
		R.Scene = TEXT("v4.11");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		TestMissionLines.Reset();
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Choix de cible
	Add(TEXT("v4.11 creatures : choix de cible"), 0.f, [this]()
	{
		using FCand = ABREntity::FTargetCandidate;
		auto Make = [](float Dist, bool bSeen, bool bHeard, bool bHid, bool bCurrent, float Since)
		{
			FCand C;
			C.Dist = Dist;
			C.bSeen = bSeen;
			C.bHeard = bHeard;
			C.bHidden = bHid;
			C.bCurrent = bCurrent;
			C.SincePerceived = Since;
			return C;
		};
		struct FCase
		{
			const TCHAR* Name;
			TArray<FCand> Cands;
			float Held;
			int32 Expected;
		};
		const TArray<FCase> Cases = {
			{ TEXT("vu a 6 m avant entendu a 4 m"), { Make(600.f, true, false, false, false, 100.f), Make(400.f, false, true, false, false, 100.f) }, 0.f, 0 },
			{ TEXT("entendu a 5 m avant ni vu ni entendu a 3 m"), { Make(300.f, false, false, false, false, 100.f), Make(500.f, false, true, false, false, 100.f) }, 0.f, 1 },
			{ TEXT("cache a 5 m apres vu a 6 m"), { Make(500.f, true, false, true, false, 100.f), Make(600.f, true, false, false, false, 100.f) }, 0.f, 1 },
			{ TEXT("cible gardee moins de 2 s"), { Make(700.f, true, false, false, true, 1.f), Make(300.f, true, false, false, false, 100.f) }, 1.f, 0 },
			{ TEXT("cible quittee pour une nettement plus proche"), { Make(700.f, true, false, false, true, 1.f), Make(300.f, true, false, false, false, 100.f) }, 3.f, 1 },
			{ TEXT("cible gardee si l'autre n'est pas nettement mieux"), { Make(700.f, true, false, false, true, 1.f), Make(400.f, true, false, false, false, 100.f) }, 3.f, 0 },
			{ TEXT("aucun candidat"), {}, 0.f, INDEX_NONE },
		};
		int32 Bad = 0;
		for (const FCase& K : Cases)
		{
			const int32 Got = ABREntity::ChooseTarget(K.Cands, K.Held);
			Bad += Got == K.Expected ? 0 : 1;
			Note(FString::Printf(TEXT("choix de cible, %s : %d (attendu %d)"), K.Name, Got, K.Expected), Got != K.Expected);
		}
		Note(FString::Printf(TEXT("choix de cible : %d/%d cas conformes"), Cases.Num() - Bad, Cases.Num()), Bad > 0);
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Missions
	// Ordre de la campagne ; chaque niveau est charge avec sa graine (les fragments de route s'accumulent pour le Niveau 11)
	// v4.13 : le Level 0 n'a plus de mission (introduction, porte de service) : il est couvert par BRAutoTestV413
	AddMissionSteps(1, 4111, false);
	AddMissionSteps(2, 4111, true);   // depart vers le Niveau 3 par la sortie ouverte
	AddMissionSteps(3, 4111, false);
	AddMissionSteps(4, 4111, false);
	AddMissionSteps(5, 4111, false);
	AddMissionSteps(6, 4111, false);
	AddMissionSteps(8, 4111, false);
	AddMissionSteps(9, 4111, false);
	AddMissionSteps(10, 4111, false);
	AddMissionSteps(37, 4111, false);
	AddMissionSteps(11, 4111, true);  // fin de la campagne
	Add(TEXT("v4.11 missions : bilan"), 0.f, [this]()
	{
		Note(FString::Printf(TEXT("missions resolues par interactions : %d niveau(x) rapportes sur 11 (Level 0 sans mission depuis la v4.13)"), TestMissionLines.Num()),
			TestMissionLines.Num() != 11);
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Trop loin, ligne de vue
	AddSeedLoad(2, 4112, TEXT("v4.11 mecanisme hors de portee"));
	Add(TEXT("v4.11 mecanisme : trop loin"), 0.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || !W->IsMissionActive())
		{
			return StepTime > 10.f;
		}
		const BRM::FPlan& P = W->GetMissionPlan();
		const int32 D = BRM::FirstRole(P, BRM::R_PressurePlate);
		ABRMissionDevice* Dev = W->FindMissionDevice(D);
		if (!Dev)
		{
			Skip(TEXT("mecanisme hors de portee : plaque des pressions absente"));
			return true;
		}
		const BRM::FState Before = W->GetMissionState();
		// 8 m plus loin que le mecanisme : refus, rien ne change
		const FVector Far = Dev->GetInteractPoint() + Dev->GetActorForwardVector() * 800.f;
		C->SetActorLocation(FVector(Far.X, Far.Y, C->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		C->RequestMissionAction(D, static_cast<uint8>(BRM::EAction::Use));
		const bool bRefused = C->LastMissionFeedback == BRMissionText::TooFar && W->GetMissionState().Dev[D] == Before.Dev[D];
		Note(FString::Printf(TEXT("plaque lue a plus de 8 m : %s (retour %s)"), bRefused ? TEXT("refusee, rien ne change") : TEXT("ACCEPTEE"),
			*FeedbackName(C->LastMissionFeedback)), !bRefused);
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Ramassage
	Add(TEXT("v4.11 ramassage : deux demandes dans la meme image"), 0.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		ABRPickup* Best = nullptr;
		float BestD = TNumericLimits<float>::Max();
		for (TActorIterator<ABRPickup> It(GetWorld()); It; ++It)
		{
			const float D = static_cast<float>(FVector::DistSquared(It->GetActorLocation(), C->GetActorLocation()));
			if (It->Item != EBRItem::Note && It->Item != EBRItem::VHSTape && D < BestD)
			{
				Best = *It;
				BestD = D;
			}
		}
		if (!Best)
		{
			Skip(TEXT("ramassage : aucun objet charge pres du joueur"));
			return true;
		}
		TestPickup = Best->Id;
		const EBRItem Item = Best->Item;
		TestCountA = C->CountItem(Item);
		TestCountB = C->ServerPickupsAccepted;
		TestCountC = C->ServerPickupsRefused;
		C->SetActorLocation(Best->GetActorLocation() + FVector(60.f, 0.f, C->GetSimpleCollisionHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		C->RequestPickup(Best);
		C->RequestPickup(Best); // meme image : la seconde ne donne rien
		const int32 Got = C->CountItem(Item) - TestCountA;
		const int32 Accepted = C->ServerPickupsAccepted - TestCountB;
		const bool bOk = Got <= 1 && Accepted == 1;
		Note(FString::Printf(TEXT("deux demandes du meme objet : %d objet(s) recu(s), %d acceptation(s), %d refus (attendu : 1, 1)"), Got, Accepted,
			C->ServerPickupsRefused - TestCountC), !bOk);
		return true;
	});
	Add(TEXT("v4.11 ramassage : trop loin"), 0.5f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		if (!C)
		{
			return false;
		}
		ABRPickup* Far = nullptr;
		for (TActorIterator<ABRPickup> It(GetWorld()); It; ++It)
		{
			if (It->Item != EBRItem::Note && It->Item != EBRItem::VHSTape && It->Id != TestPickup)
			{
				Far = *It;
				break;
			}
		}
		if (!Far)
		{
			Skip(TEXT("ramassage hors de portee : un seul objet charge"));
			return true;
		}
		const EBRItem Item = Far->Item;
		const int32 Before = C->CountItem(Item);
		C->SetActorLocation(Far->GetActorLocation() + FVector(1200.f, 0.f, C->GetSimpleCollisionHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		C->RequestPickup(Far);
		const bool bOk = C->CountItem(Item) == Before && IsValid(Far) && !Far->IsActorBeingDestroyed();
		Note(FString::Printf(TEXT("objet demande a 12 m : %s"), bOk ? TEXT("refuse, l'objet reste") : TEXT("RAMASSE")), !bOk);
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Ancienne partie (format 3)
	Add(TEXT("v4.11 ancienne partie : fichier du format 3"), 0.f, [this]()
	{
		BRSaves::SetTestPrefix(SavePrefix);
		DeleteSaveFiles();
		BRSaves::TakeLoadMessages();
		// Une partie v4.10 en cours au Niveau 0 (cassettes VHS) : ecrite sans passer par BRSaves (format garde a 3)
		UBRSaveGame* Old = NewObject<UBRSaveGame>();
		Old->Version = 3;
		Old->SaveName = TEXT("Partie v4.10");
		Old->Created = FDateTime::Now();
		Old->MarkExplored(0);
		Old->Session.bValid = true;
		Old->Session.Level = 0;
		Old->Session.Seed = ABRWorld::SeedFromUser(4605, 0);
		Old->Session.VHSFound = 1;
		TArray<uint8> Bytes;
		UGameplayStatics::SaveGameToMemory(Old, Bytes);
		FFileHelper::SaveArrayToFile(Bytes, *BRSaves::FilePath(BRSaves::SlotName(LegacySlot)));
		UBRSaveGame* L = BRSaves::Load(LegacySlot);
		// v4.13 : une session du Level 0 (ancien mode VHS) n'a plus d'objectif : elle passe au mode courant, garde sa graine
		// et ses cassettes (documents) ; l'histoire commence au Level 0 (rien d'accompli, rien de gare)
		const bool bOk = L && L->LoadedVersion == 3 && L->Version == UBRSaveGame::CurrentVersion && L->Session.bValid && L->Session.GenVersion == BRM::GenVersion
			&& L->Session.Mission.Num() == 0 && L->Session.VHSFound == 1 && L->RouteBits == 0 && L->Endings == 0 && L->StoryResume == 0 && L->StoryDone == 0
			&& L->StoryParked == -1;
		Note(FString::Printf(TEXT("format 3 migre : %s (format lu %d, generation de la session %d, cassettes gardees %d, histoire : reprise %d, etapes 0x%x, gare %d)"),
			bOk ? TEXT("OK") : TEXT("ECHEC"), L ? L->LoadedVersion : -1, L ? L->Session.GenVersion : -1, L ? L->Session.VHSFound : -1, L ? L->StoryResume : -1,
			L ? L->StoryDone : 0u, L ? L->StoryParked : -2), !bOk);
		return true;
	});
	Add(TEXT("v4.11 ancienne partie : reprise"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		UBRSaveGame* L = BRSaves::Load(LegacySlot);
		if (!W || !PC || !L || W->IsTransitioning())
		{
			return StepTime > 10.f;
		}
		TestOrigSlot = BRSaves::ActiveSlot();
		TestOrigSave = PC->ActiveSave;
		bTestDevSession = PC->bDevSession;
		BRSaves::ActiveSlot() = LegacySlot;
		PC->ActiveSave = L;
		PC->bDevSession = false;
		BRSaves::PendingResume() = L->Session;
		W->RequestTransition(L->Session.Level, false, L->Session.Seed);
		return true;
	});
	Add(TEXT("v4.11 ancienne partie : Level 0 sans condition (v4.13)"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning() || !W->IsLevelReady())
		{
			return StepTime > 30.f;
		}
		// v4.13 : la session reprend (meme graine, cassettes gardees), sans mission ni ancien objectif ; la porte de service
		// s'ouvre sans condition
		FString Why;
		const bool bDoor = W->HasServiceDoor() && W->CanUseExit(1, Why);
		const bool bOk = W->IsResumedSession() && !W->IsMissionActive() && !W->IsLegacyObjectives() && W->GetVHSFound() == 1 && bDoor
			&& W->GetSessionMode() == EBRSessionMode::Story;
		Note(FString::Printf(TEXT("ancienne session reprise : generation %d, mission %s, objectifs anciens %s, cassettes gardees %d, porte de service %s, session d'histoire %s"),
			W->GetMissionGen(), W->IsMissionActive() ? TEXT("ACTIVE") : TEXT("aucune"), YesNo(W->IsLegacyObjectives()), W->GetVHSFound(),
			bDoor ? TEXT("ouverte a tous") : *FString::Printf(TEXT("FERMEE (%s)"), *Why), YesNo(W->GetSessionMode() == EBRSessionMode::Story)), !bOk);
		PC->SetInventoryOpen(true, static_cast<int32>(ABRHUD::ETab::Notebook));
		return true;
	});
	Add(TEXT("v4.11 ancienne partie : carnet"), 0.5f, [this]()
	{
		Shot(TEXT("v411_carnet_ancienne_partie"));
		if (ABRPlayerController* PC = GetPC())
		{
			PC->SetInventoryOpen(false);
		}
		return true;
	});
	Add(TEXT("v4.11 ancienne partie : saut refuse par l'histoire (v4.13)"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		// v4.13 : en session d'histoire, l'hote refuse une transition que le fil n'a pas preparee (ici : Niveau 1 depuis
		// un Level 0 dont la porte n'a pas ete prise) ; on passe donc par la porte de service, comme un joueur
		FString Why;
		const bool bRefused = !W->CanEnterLevel(1, &Why);
		W->RequestTransition(1);
		Note(FString::Printf(TEXT("Niveau 1 demande sans prendre la porte : %s (%s)"), bRefused ? TEXT("refuse") : TEXT("ACCEPTE"), *Why), !bRefused);
		return true;
	});
	Add(TEXT("v4.11 ancienne partie : devant la porte de service"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->IsTransitioning() || !W->HasServiceDoor())
		{
			return StepTime > 10.f;
		}
		const FVector Fwd = FRotator(0.f, W->GetServiceDoorYaw(), 0.f).Vector();
		C->SetActorLocation(W->GetServiceDoorLocation() + Fwd * 120.f + FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight() + 5.f), false, nullptr,
			ETeleportType::TeleportPhysics);
		return true;
	});
	Add(TEXT("v4.11 ancienne partie : niveau suivant"), 1.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->IsTransitioning())
		{
			return false;
		}
		ABRExit* Door = nullptr;
		for (TActorIterator<ABRExit> It(GetWorld()); It; ++It)
		{
			Door = It->Style == EBRExitStyle::ServiceDoor ? *It : Door;
		}
		if (!Door)
		{
			return StepTime > 15.f; // chunk de la porte en construction
		}
		Door->Use(C);
		return true;
	});
	Add(TEXT("v4.11 ancienne partie : mission au niveau suivant"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 1)
		{
			return StepTime > 30.f;
		}
		PC->WriteActiveSave(true);
		const UBRSaveGame* L = BRSaves::Load(LegacySlot);
		// v4.13 : l'etape du Level 0 est accomplie (porte prise), la reprise est au Niveau 1
		const bool bStory = L && BRStory::IsDone(L->GetStory(), 0) && L->StoryResume == 1;
		const bool bOk = W->GetMissionGen() == BRM::GenVersion && W->IsMissionActive() && L && L->Session.GenVersion == BRM::GenVersion && L->Session.Mission.Num() > 0 && bStory;
		Note(FString::Printf(TEXT("niveau suivant : generation %d, mission %s, sauvegarde : generation %d, etat de mission %d octets, Level 0 accompli %s, reprise %d"),
			W->GetMissionGen(), W->IsMissionActive() ? TEXT("active") : TEXT("ABSENTE"), L ? L->Session.GenVersion : -1, L ? L->Session.Mission.Num() : -1,
			YesNo(L && BRStory::IsDone(L->GetStory(), 0)), L ? L->StoryResume : -1), !bOk);
		// Retour a la session de test d'avant (partie active, mode)
		BRSaves::ActiveSlot() = TestOrigSlot;
		PC->ActiveSave = TestOrigSave;
		PC->bDevSession = bTestDevSession;
		TestOrigSave = nullptr;
		DeleteSaveFiles();
		BRSaves::SetTestPrefix(FString());
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Carnet et aide
	AddSeedLoad(4, 4113, TEXT("v4.11 carnet"));
	Add(TEXT("v4.11 carnet : onglet"), 0.5f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRWorld* W = GetBRWorld();
		if (!PC || !W || !W->IsMissionActive())
		{
			return StepTime > 10.f;
		}
		PC->SetInventoryOpen(true, static_cast<int32>(ABRHUD::ETab::Notebook));
		NotebookStep = 0;
		bNotebookShot = false;
		return true;
	});
	Add(TEXT("v4.11 carnet : aide progressive"), 0.3f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr;
		if (!H)
		{
			return false;
		}
		// Le bouton AIDE fait passer l'etape en cours de 0 a 1 puis 2 (indice, puis rappel des indices trouves) ; ici, le
		// niveau d'aide est regle comme le ferait un clic, et l'image suivante doit l'afficher
		const int32 Step = NotebookStep;
		// L'aide est rangee par etape du carnet de ce niveau (cle et graine fixees au dessin du carnet) : on attend que le
		// carnet de ce niveau ait ete dessine, sinon l'aide demandee irait a une cle perimee et serait effacee
		const ABRWorld* W = GetBRWorld();
		const uint32 HintSeed = W ? (W->GetSeed() ^ static_cast<uint32>(W->GetMissionPlan().Level * 7919)) : 0u;
		if (Step == 0 && (H->NotebookHintSeed != HintSeed || H->NotebookHintStep < 0) && StepTime < 5.f)
		{
			return false;
		}
		if (!bNotebookShot)
		{
			if (Step == 0)
			{
				const bool bTab = H->Tab == ABRHUD::ETab::Notebook && H->LastNotebookHintLevel == 0;
				Note(FString::Printf(TEXT("carnet ouvert : onglet %s, aide au depart %d"), YesNo(H->Tab == ABRHUD::ETab::Notebook), H->LastNotebookHintLevel), !bTab);
				Shot(TEXT("v411_carnet_aide0"));
			}
			else
			{
				const bool bLevel = H->LastNotebookHintLevel == Step;
				Note(FString::Printf(TEXT("aide de niveau %d affichee : %s"), Step, YesNo(bLevel)), !bLevel);
				Shot(FString::Printf(TEXT("v411_carnet_aide%d"), Step));
			}
			// v4.15 : la capture se fait a la fin de cette image : l'aide (ou la fermeture) change a l'image suivante, sinon
			// la capture montrait deja l'etat suivant
			bNotebookShot = true;
			return false;
		}
		bNotebookShot = false;
		if (Step >= 2)
		{
			H->NotebookHint.Reset();
			NotebookStep = 0;
			PC->SetInventoryOpen(false);
			return true;
		}
		H->NotebookHint.FindOrAdd(H->NotebookHintStep) = Step + 1;
		++NotebookStep;
		return false;
	});

	// ---------------------------------------------------------------------------------------------- Sous-titres
	Add(TEXT("v4.11 sous-titres : affiches avec la direction"), 0.2f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr;
		if (!C || !H)
		{
			return false;
		}
		BRTestV411::bPlayerSubtitles = FBRSettings::Get().bSubtitles;
		FBRSettings::Get().bSubtitles = true;
		const FVector Right = C->GetActorLocation() + PC->GetControlRotation().RotateVector(FVector(0.f, 400.f, 0.f));
		const FString Dir = ABRHUD::DirectionWord(this, Right);
		const FString Expected = BR_STR(NSLOCTEXT("BR", "Caption.Right", "\u00e0 droite"));
		ABRHUD::Caption(this, BR_STR(NSLOCTEXT("BR", "Caption.Hound", "[Grondement d'un Hound]")), Right, 3.f);
		Note(FString::Printf(TEXT("direction d'un son a droite : \"%s\" (attendu \"%s\")"), *Dir, *Expected), Dir != Expected);
		return true;
	});
	Add(TEXT("v4.11 sous-titres : image suivante"), 0.2f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr;
		if (!H)
		{
			return false;
		}
		const int32 Shown = H->LastFrameCaptions;
		Note(FString::Printf(TEXT("sous-titres actives : %d ligne(s) dessinee(s)"), Shown), Shown < 1);
		Shot(TEXT("v411_sous_titres"));
		FBRSettings::Get().bSubtitles = false;
		return true;
	});
	Add(TEXT("v4.11 sous-titres : desactives"), 0.2f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr;
		if (!H)
		{
			return false;
		}
		ABRHUD::Caption(this, TEXT("[test]"), FVector::ZeroVector, 3.f);
		const int32 Shown = H->LastFrameCaptions;
		Note(FString::Printf(TEXT("sous-titres desactives : %d ligne(s) dessinee(s) (attendu 0)"), Shown), Shown != 0);
		FBRSettings::Get().bSubtitles = BRTestV411::bPlayerSubtitles;
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Volumes separes
	Add(TEXT("v4.11 volumes : effets et voix"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		UBRAssets* A = UBRAssets::Get(this);
		if (!PC || !A)
		{
			return false;
		}
		USoundWave* Wave = Cast<USoundWave>(A->Sound(TEXT("S_M_Switch")));
		FAudioDeviceHandle Audio = GetWorld()->GetAudioDevice();
		if (!Wave || !Audio.IsValid())
		{
			Skip(TEXT("volumes : son S_M_Switch ou appareil audio absent (jeu sans son)"));
			return true;
		}
		FBRSettings& S = FBRSettings::Get();
		const FBRSettings Saved = S;
		S.MasterVolume = 1.f;
		S.EffectsVolume = 1.f;
		S.VoiceVolume = 1.f;
		PC->ApplySettings();
		const float Base = A->Sound(TEXT("S_M_Switch")) ? Wave->Volume : 0.f;
		// Voix a 50 %, effets a 100 % : appareil a 50 %, sons du jeu doubles (chacun obtient general x son volume)
		S.VoiceVolume = 0.5f;
		PC->ApplySettings();
		A->Sound(TEXT("S_M_Switch"));
		// Unreal 5.8 : GetTransientPrimaryVolume ne se lit que sur le fil audio (assertion, plantage du test) : non lu ici
		Skip(TEXT("volumes : volume general de l'appareil lisible seulement sur le fil audio (Unreal 5.8) : non verifie"));
		const float EffectsHalfVoice = Wave->Volume;
		// Effets a 25 %, voix a 100 %
		S.VoiceVolume = 1.f;
		S.EffectsVolume = 0.25f;
		PC->ApplySettings();
		A->Sound(TEXT("S_M_Switch"));
		const float EffectsQuarter = Wave->Volume;
		const bool bOk = Base > 0.f && FMath::IsNearlyEqual(EffectsHalfVoice, Base * 2.f, Base * 0.02f)
			&& FMath::IsNearlyEqual(EffectsQuarter, Base * 0.25f, Base * 0.02f);
		Note(FString::Printf(TEXT("volumes : voix 50 %% -> sons du jeu x%.2f (attendu 2.00, l'appareil passe a 50 %%) ; effets 25 %% -> x%.2f (attendu 0.25)"),
			Base > 0.f ? EffectsHalfVoice / Base : 0.f, Base > 0.f ? EffectsQuarter / Base : 0.f), !bOk);
		S = Saved;
		PC->ApplySettings();
		return true;
	});

	// ---------------------------------------------------------------------------------------------- Bruit et menace
	Add(TEXT("v4.11 creatures : bruit"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		const FVector Here = C->GetActorLocation();
		// A 5 m en vue directe (un mur entre les deux reduit la portee a 55 % : 4,95 m pour un bruit de 9 m, regle voulue) :
		// une direction degagee autour du joueur. v4.15 : degagee au sens de la regle du jeu (ABRWorld::FindRecentNoise : seuls
		// les murs comptent, pas les capsules)
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestNoise), false, C);
		FCollisionResponseParams WallsOnly;
		WallsOnly.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		FVector Dir = FVector::ZeroVector;
		for (int32 Turn = 0; Turn < 8 && Dir.IsZero(); ++Turn)
		{
			const FVector Try = FVector(1.f, 0.f, 0.f).RotateAngleAxis(Turn * 45.f, FVector::UpVector);
			if (!GetWorld()->LineTraceTestByChannel(Here + Try * 500.f + FVector(0.f, 0.f, 80.f), Here, ECC_WorldStatic, Q, WallsOnly))
			{
				Dir = Try;
			}
		}
		if (Dir.IsZero())
		{
			Skip(TEXT("bruit de mecanisme : aucune direction degagee sur 5 m autour du joueur"));
			return true;
		}
		// v4.15 : diagnostic : ce qui coupe le segment du test, capsules comprises (avant la v4.15 : la capsule du joueur qui
		// fait le bruit, toujours), puis avec la regle du jeu (seuls les murs absorbent le bruit : rien attendu)
		{
			const FCollisionQueryParams Raw(SCENE_QUERY_STAT(BRTestNoiseRaw), false);
			FHitResult Hit;
			const bool bRaw = GetWorld()->LineTraceSingleByChannel(Hit, Here + Dir * 500.f + FVector(0.f, 0.f, 80.f), Here, ECC_WorldStatic, Raw);
			FHitResult WallHit;
			const bool bWallHit = GetWorld()->LineTraceSingleByChannel(WallHit, Here + Dir * 500.f + FVector(0.f, 0.f, 80.f), Here, ECC_WorldStatic, Raw, WallsOnly);
			Note(FString::Printf(TEXT("bruit : segment du test (5 m) : capsules comprises %s ; murs seulement (regle du jeu) %s"),
				bRaw ? *FString::Printf(TEXT("coupe par %s (%s) a %.0f cm"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.Distance) : TEXT("libre"),
				bWallHit ? *FString::Printf(TEXT("coupe par %s (%s)"), *GetNameSafe(WallHit.GetActor()), *GetNameSafe(WallHit.GetComponent())) : TEXT("libre")));
		}
		W->ReportNoise(Here, 900.f);
		FVector Heard;
		const bool bNear = W->FindRecentNoise(Here + Dir * 500.f, Heard) && FVector::Dist(Heard, Here) < 50.f;
		FVector Unused;
		const bool bFar = W->FindRecentNoise(Here + Dir * 5000.f, Unused);
		Note(FString::Printf(TEXT("bruit de mecanisme : entendu a 5 m en vue %s, a 50 m %s (attendu oui, non)"), YesNo(bNear), bFar ? TEXT("oui") : TEXT("non")), !bNear || bFar);

		// v4.15 : derriere un mur, le meme bruit (9 m) n'est plus entendu a 5-8,8 m (portee 55 % : 4,95 m) : les murs
		// absorbent toujours. Verifie avant les bruits suivants (plus forts, ils seraient entendus de la)
		{
			FCollisionQueryParams QW(SCENE_QUERY_STAT(BRTestNoiseWall), false, C);
			FVector Behind = FVector::ZeroVector;
			for (const float Dist : { 500.f, 600.f, 700.f, 800.f })
			{
				for (int32 Turn = 0; Turn < 16 && Behind.IsZero(); ++Turn)
				{
					const FVector Try = Here + FVector(1.f, 0.f, 0.f).RotateAngleAxis(Turn * 22.5f, FVector::UpVector) * Dist;
					if (GetWorld()->LineTraceTestByChannel(Try + FVector(0.f, 0.f, 80.f), Here, ECC_WorldStatic, QW, WallsOnly))
					{
						Behind = Try;
					}
				}
			}
			if (Behind.IsZero())
			{
				Skip(TEXT("bruit derriere un mur : aucun mur entre 5 et 8 m autour du joueur"));
			}
			else
			{
				FVector WallHeard;
				const bool bHeardThrough = W->FindRecentNoise(Behind, WallHeard) && FVector::Dist(WallHeard, Here) < 50.f;
				Note(FString::Printf(TEXT("bruit de mecanisme derriere un mur a %.1f m : entendu %s (attendu non : portee 55 %%, 4,95 m)"), FVector::Dist2D(Behind, Here) / 100.f,
					bHeardThrough ? TEXT("OUI") : TEXT("non")), bHeardThrough);
			}
		}

		// v4.15 : ecouteur grand (Wretch, demi-hauteur 80 : son trace partait du haut de sa propre capsule, toujours coupe) :
		// un bruit de 6 m a 4 m de lui, en vue, au niveau du centre du joueur (sous le depart du trace du Wretch), est entendu
		{
			const FBREntityInfo& Info = ABREntity::Info(EBREntityKind::Wretch);
			const float FloorZ = static_cast<float>(Here.Z) - C->GetSimpleCollisionHalfHeight();
			const FVector At = Here + Dir * 500.f;
			ABREntity* E = W->SpawnEntity(EBREntityKind::Wretch, FVector(At.X, At.Y, FloorZ + Info.HalfHeight + 5.f));
			if (!E)
			{
				Skip(TEXT("bruit entendu par un Wretch : creature non creee"));
			}
			else
			{
				const FVector Source = Here + Dir * 100.f;
				W->ReportNoise(Source, 600.f);
				FVector WretchHeard;
				const bool bWretch = W->FindRecentNoise(E->GetActorLocation(), WretchHeard) && FVector::Dist(WretchHeard, Source) < 50.f;
				Note(FString::Printf(TEXT("bruit de 6 m a %.1f m d'un Wretch, en vue : entendu %s (attendu oui ; avant la v4.15 sa propre capsule coupait la ligne : 3,3 m)"),
					FVector::Dist(E->GetActorLocation(), Source) / 100.f, YesNo(bWretch)), !bWretch);
				E->Destroy();
			}
		}

		// v4.15 : impulsion LiDAR (18 m, emise aux yeux du joueur) entendue a 12 m sans mur ; avant, la capsule du joueur qui
		// l'emettait coupait toujours la ligne (9,9 m)
		{
			const FVector Eye = C->GetEyeLocation();
			FVector Far = FVector::ZeroVector;
			for (int32 Turn = 0; Turn < 8 && Far.IsZero(); ++Turn)
			{
				const FVector Try = Here + FVector(1.f, 0.f, 0.f).RotateAngleAxis(Turn * 45.f, FVector::UpVector) * 1200.f;
				if (!GetWorld()->LineTraceTestByChannel(Try + FVector(0.f, 0.f, 80.f), Eye, ECC_WorldStatic, Q, WallsOnly))
				{
					Far = Try;
				}
			}
			if (Far.IsZero())
			{
				Skip(TEXT("impulsion LiDAR entendue a 12 m : aucune direction degagee sur 12 m autour du joueur"));
			}
			else
			{
				W->ReportNoise(Eye, BRLidar::PulseNoiseRadius);
				FVector PulseHeard;
				const bool bPulse = W->FindRecentNoise(Far, PulseHeard) && FVector::Dist(PulseHeard, Eye) < 50.f;
				Note(FString::Printf(TEXT("impulsion LiDAR (%.0f m) entendue a %.1f m en vue : %s (attendu oui)"), BRLidar::PulseNoiseRadius / 100.f, FVector::Dist(Far, Eye) / 100.f,
					YesNo(bPulse)), !bPulse);
			}
		}
		return true;
	});
	Add(TEXT("v4.11 creatures : budget de menace"), 0.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		const int32 Budget = W->ThreatBudget();
		TestSpawned.Reset();
		FVector Spot;
		for (int32 I = 0; I < 8 && W->ThreatScore() < Budget; ++I)
		{
			if (!FindSpotInFront(1400.f + I * 150.f, 0.f, Spot))
			{
				break;
			}
			if (ABREntity* E = W->SpawnEntity(I % 2 ? EBREntityKind::Hound : EBREntityKind::Wretch, Spot))
			{
				TestSpawned.Add(E);
			}
		}
		const int32 Score = W->ThreatScore();
		const bool bBlocked = Score >= Budget && !W->AllowsNewEncounter();
		Note(FString::Printf(TEXT("budget de menace : score %d / budget %d, nouvelle rencontre refusee %s"), Score, Budget, YesNo(bBlocked)), !bBlocked);
		for (const TWeakObjectPtr<ABREntity>& E : TestSpawned)
		{
			if (E.IsValid())
			{
				E->Destroy();
			}
		}
		TestSpawned.Reset();
		return true;
	});
}

namespace BRTestV411Net
{
	/** v4.15 : deplacement instantane du client suivi jusqu'a ce que le serveur l'ait recu (BRTestNet) */
	BRTestNet::FPlacement Place;

	/** v4.15 : hote juste devant la sortie (1,2 m sur son axe, sol trouve, capsule libre) : la vue vers la sortie est celle
	 *  d'un joueur qui s'y presente de face ; StandNear (premiere place libre autour) reste le repli */
	bool StandInFront(UWorld* World, const ABRExit* Exit, const ABRCharacter* C, FVector& Out)
	{
		if (!World || !Exit || !C)
		{
			return false;
		}
		const float Half = C->GetSimpleCollisionHalfHeight();
		const FVector Foot = Exit->GetActorLocation();
		const FVector P = Foot + FRotator(0.f, Exit->GetActorRotation().Yaw, 0.f).Vector() * 120.f;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestFront), false, C);
		Q.AddIgnoredActor(Exit);
		FHitResult Floor;
		if (!World->LineTraceSingleByChannel(Floor, FVector(P.X, P.Y, Foot.Z + 100.f), FVector(P.X, P.Y, Foot.Z - 200.f), ECC_WorldStatic, Q))
		{
			return false;
		}
		const FVector Stand(P.X, P.Y, Floor.ImpactPoint.Z + Half + 5.f);
		if (World->OverlapAnyTestByChannel(Stand, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(36.f, Half - 4.f), Q))
		{
			return false;
		}
		Out = Stand;
		return true;
	}

	/** v4.15 : vue vers le point vise par le depart de groupe (ABRWorld::DepartureViewPoint), comme ReachInfo : libre ou
	 *  coupee par quoi, a quelle distance */
	FString DepartureViewInfo(UWorld* World, const ABRCharacter* C, const ABRExit* Exit)
	{
		if (!World || !C || !Exit)
		{
			return TEXT("sortie ou joueur absent");
		}
		const FVector Eye = C->GetEyeLocation();
		const FVector Aim = ABRWorld::DepartureViewPoint(Exit);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestDepartView), false, C);
		Q.AddIgnoredActor(Exit);
		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, Eye, Aim, ECC_WorldStatic, Q);
		return FString::Printf(TEXT("yeux (%.0f, %.0f, %.0f) vers (%.0f, %.0f, %.0f), %.0f cm, vue %s"), Eye.X, Eye.Y, Eye.Z, Aim.X, Aim.Y, Aim.Z, FVector::Dist(Eye, Aim),
			bBlocked ? *FString::Printf(TEXT("BLOQUEE par %s (%s) a %.0f cm"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.Distance) : TEXT("libre"));
	}
}

void ABRAutoTest::AddNetV411Steps(bool bClient)
{
	using namespace BRTestV411;
	using namespace BRTestV411Net;

	// v4.13 : le Level 0 n'a plus de mission (sa porte de service mene au Niveau 1) : l'hote emmene le groupe au Niveau 1
	Add(TEXT("v4.11 reseau : niveau a mission"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (!bClient && TestMissionPhase != 1411)
		{
			if (W->IsTransitioning())
			{
				return false;
			}
			TestMissionPhase = 1411;
			if (W->GetLevelNumber() != 1)
			{
				if (ABRPlayerController* PC = GetPC())
				{
					PC->BRLevel(1);
				}
				return false;
			}
		}
		if ((W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 1 || !W->IsMissionActive()) && StepTime < 45.f)
		{
			return false;
		}
		TestMissionPhase = 0;
		if (W->GetLevelNumber() != 1 || !W->IsMissionActive())
		{
			Note(FString::Printf(TEXT("%s : Niveau 1 avec mission pas atteint en 45 s (niveau %d)"), bClient ? TEXT("client") : TEXT("hote"), W->GetLevelNumber()), true);
		}
		return true;
	});

	// Etat de mission : meme generation, meme plan, meme etat (sinon le client verrait d'autres mecanismes)
	Add(TEXT("v4.11 reseau : mission a la connexion"), 1.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (bClient && !W->IsMissionActive() && StepTime < 15.f)
		{
			return false;
		}
		const TArray<uint8> Blob = W->GetMissionBlob();
		const uint32 Crc = FCrc::MemCrc32(Blob.GetData(), Blob.Num());
		const BRM::FPlan& P = W->GetMissionPlan();
		const bool bOk = W->IsMissionActive() && W->GetMissionDevices().Num() == P.NumDevices;
		Note(FString::Printf(TEXT("%s : generation %d, plan %08x, %d mecanismes, revision %d, etat %08x (a comparer entre les deux rapports)"),
			bClient ? TEXT("client") : TEXT("hote"), W->GetMissionGen(), BRM::Fingerprint(P), W->GetMissionDevices().Num(), W->GetMissionRev(), Crc), !bOk);
		return true;
	});

	// Le client lit un indice par une vraie interaction : demande numerotee, validee par l'hote, etat replique
	Add(TEXT("v4.11 reseau : action d'un client"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || !W->IsMissionActive())
		{
			return StepTime > 15.f;
		}
		const BRM::FPlan& P = W->GetMissionPlan();
		int32 D = INDEX_NONE;
		for (int32 I = 0; I < P.NumDevices && D == INDEX_NONE; ++I)
		{
			D = P.Devices[I].Kind == BRM::EKind::Clue ? I : INDEX_NONE;
		}
		TestMissionDevice = D;
		if (D == INDEX_NONE)
		{
			Skip(TEXT("action d'un client : aucun indice dans le plan"));
			return true;
		}
		if (!bClient)
		{
			TestTimer = FPlatformTime::Seconds();
			return true;
		}
		ABRMissionDevice* Dev = W->FindMissionDevice(D);
		FVector Stand;
		if (!Dev || !Dev->IsShown())
		{
			if (Dev && StepTime < 1.f)
			{
				// Le chunk se construit pres du mecanisme
				PlaceAndFace(C, W->CellCenter(Dev->GetCell(), Dev->GetInteractPoint().Z), Dev->GetInteractPoint());
			}
			return StepTime > 20.f;
		}
		if (TestMissionPhase != 4111)
		{
			if (!StandNear(GetWorld(), Dev->GetInteractPoint(), Dev->GetActorForwardVector(), C->GetSimpleCollisionHalfHeight(), Dev, C, Stand))
			{
				Note(TEXT("action d'un client : aucune place devant l'indice"), true);
				return true;
			}
			// La position du client part a l'hote avec ses mouvements, pas avec la demande : la demande attend que l'hote
			// l'ait recue. v4.15 : mouvements en attente envoyes avant le deplacement, place verifiee sur plusieurs images
			const FVector Aim = Dev->GetInteractPoint();
			Place.Start(C, Stand, &Aim, FPlatformTime::Seconds());
			TestMissionPhase = 4111;
			TestTimer = FPlatformTime::Seconds();
			return false;
		}
		if (!Place.Update(C, FPlatformTime::Seconds()) && FPlatformTime::Seconds() - TestTimer < 10.0)
		{
			return false;
		}
		TestMissionPhase = 0;
		TestCountA = C->MissionResultsReceived;
		TestTimer = FPlatformTime::Seconds();
		C->RequestMissionAction(D, static_cast<uint8>(BRM::EAction::Use));
		return true;
	});
	Add(TEXT("v4.11 reseau : reponse et etat replique"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		const int32 D = TestMissionDevice;
		if (!W || !C || D == INDEX_NONE)
		{
			return true;
		}
		const bool bRead = W->GetMissionState().Dev[D] != 0;
		if (!bRead && StepTime < 15.f)
		{
			return false;
		}
		if (bClient)
		{
			const bool bAnswered = C->MissionResultsReceived > TestCountA;
			const bool bOk = bAnswered && bRead && (C->LastMissionFeedback == static_cast<uint8>(BRM::EFeedback::Done)
				|| C->LastMissionFeedback == static_cast<uint8>(BRM::EFeedback::AlreadyDone));
			Note(FString::Printf(TEXT("client : reponse de l'hote %s (%s), indice lu dans l'etat replique %s, en %.0f ms"), YesNo(bAnswered),
				*FeedbackName(C->LastMissionFeedback), YesNo(bRead), (FPlatformTime::Seconds() - TestTimer) * 1000.0), !bOk);
		}
		else
		{
			FString Seen;
			for (const ABRCharacter* M : Mates(W))
			{
				Seen = ReachInfo(GetWorld(), M, W->FindMissionDevice(D));
			}
			Note(FString::Printf(TEXT("hote : indice %d lu par le client %s ; client vu par l'hote : %s"), D, YesNo(bRead), *Seen), !bRead);
		}
		return true;
	});

	// Ramassage dispute : les deux joueurs demandent le meme objet ; un seul le recoit
	Add(TEXT("v4.11 reseau : ramassage dispute"), 0.f, [this, bClient]()
	{
		ABRCharacter* C = GetPlayer();
		ABRWorld* W = GetBRWorld();
		if (!C || !W)
		{
			return false;
		}
		ABRPickup* Shared = SharedPickup(GetWorld());
		if (!Shared)
		{
			Skip(TEXT("ramassage dispute : aucun objet charge"));
			TestPickup = 0;
			return true;
		}
		TestPickup = Shared->Id;
		TestCountA = C->CountItem(Shared->Item);
		TestCountB = C->ServerPickupsAccepted;
		const TArray<ABRCharacter*> Others = Mates(W);
		TestCountC = 0;
		for (const ABRCharacter* M : Others)
		{
			TestCountC += M->ServerPickupsAccepted;
		}
		// Chacun d'un cote de l'objet, a portee
		C->SetActorLocation(Shared->GetActorLocation() + FVector(bClient ? -70.f : 70.f, 0.f, C->GetSimpleCollisionHalfHeight() + 5.f), false, nullptr,
			ETeleportType::TeleportPhysics);
		C->RequestPickup(Shared);
		TestTimer = FPlatformTime::Seconds();
		return true;
	});
	Add(TEXT("v4.11 reseau : un seul gagnant"), 3.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || TestPickup == 0)
		{
			return true;
		}
		if (C->IsPickupPending() && StepTime < 10.f)
		{
			return false;
		}
		if (bClient)
		{
			Note(FString::Printf(TEXT("client : ramassage dispute, %d acceptation(s), %d refus"), C->ServerPickupsAccepted - TestCountB, C->ServerPickupsRefused));
			return true;
		}
		// L'hote voit les deux demandes : la sienne et celle du client (compteurs tenus par l'hote)
		int32 Accepted = C->ServerPickupsAccepted - TestCountB;
		for (const ABRCharacter* M : Mates(W))
		{
			Accepted += M->ServerPickupsAccepted;
		}
		Accepted -= TestCountC;
		Note(FString::Printf(TEXT("ramassage dispute : %d acceptation(s) en tout (attendu 1)"), Accepted), Accepted != 1);
		return true;
	});

	// Soin d'un autre niveau (demande perimee) : refuse, rien n'est consomme
	Add(TEXT("v4.11 reseau : soin perime"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if (!bClient)
		{
			for (ABRCharacter* M : Mates(W))
			{
				TestCountA = M->ServerHealsRefused;
				TestCountB = M->GetServerHealStock(EBRItem::Bandage);
			}
			return true;
		}
		if (C->CountItem(EBRItem::Bandage) < 1)
		{
			// Le refus d'une demande perimee passe avant le controle du stock connu de l'hote
			C->AddItem(EBRItem::Bandage, 1);
		}
		TestCountA = C->CountItem(EBRItem::Bandage);
		// La demande porte le numero du niveau precedent : comme un soin parti juste avant un changement de niveau
		C->ServerRequestHeal(static_cast<uint8>(EBRItem::Bandage), 60001, W->GetLevelSerial() - 1, C->GetInvEpoch());
		return true;
	});
	Add(TEXT("v4.11 reseau : soin perime refuse"), 2.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if (bClient)
		{
			const bool bOk = C->CountItem(EBRItem::Bandage) == TestCountA;
			Note(FString::Printf(TEXT("client : soin perime, bandages %d -> %d (rien consomme %s)"), TestCountA, C->CountItem(EBRItem::Bandage), YesNo(bOk)), !bOk);
			return true;
		}
		const TArray<ABRCharacter*> Others = Mates(W);
		if (Others.Num() == 0)
		{
			Skip(TEXT("soin perime : aucun client"));
			return true;
		}
		ABRCharacter* M = Others[0];
		// Les deux machines avancent a leur rythme : la demande peut arriver avant que l'hote releve ses compteurs. La reponse
		// donnee a cette demande (numero 60001) est cherchee dans l'historique du serveur
		int32 Result = -1;
		for (const ABRCharacter::FBRTxnRecord& R : M->ServerHealHistory)
		{
			if (R.RequestId == 60001)
			{
				Result = R.Result;
			}
		}
		if (Result < 0 && StepTime < 10.f)
		{
			return false;
		}
		const bool bOk = Result == 7 && M->GetServerHealStock(EBRItem::Bandage) >= TestCountB;
		Note(FString::Printf(TEXT("hote : soin perime du client refuse %s (raison %d, attendu 7 : autre niveau), bandages connus de l'hote %d -> %d"), YesNo(Result == 7), Result,
			TestCountB, M->GetServerHealStock(EBRItem::Bandage)), !bOk);
		return true;
	});

	// Depart de groupe : l'hote resout la mission et demande le depart ; le client, loin, est attendu, puis rejoint la
	// sortie ; le groupe part ensemble (meme niveau, meme graine)
	Add(TEXT("v4.11 reseau : depart demande"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || !W->IsMissionActive())
		{
			if (StepTime > 15.f)
			{
				// Pas de depart a verifier : les etapes suivantes ne doivent pas reussir sur un niveau inchange
				TestLevel = -1;
				Note(TEXT("depart de groupe : aucune mission active sur ce niveau"), true);
				return true;
			}
			return false;
		}
		TestLevel = BRM::ForwardTarget(W->GetLevelNumber(), 0);
		TestCountA = W->GetLevelSerial();
		if (bClient)
		{
			return true;
		}
		W->DebugCompleteMission();
		ABRExit* Exit = FindExit(GetWorld(), TestLevel);
		FVector Stand;
		if (!Exit || !StandNear(GetWorld(), Exit->GetActorLocation() + FVector(0.f, 0.f, 100.f), Exit->GetActorForwardVector(), C->GetSimpleCollisionHalfHeight(), Exit, C, Stand))
		{
			Note(FString::Printf(TEXT("depart de groupe : sortie vers %d introuvable ou sans place"), TestLevel), true);
			return true;
		}
		// v4.15 : de face d'abord (1,2 m devant la sortie, sur son axe), la premiere place libre autour sinon
		FVector Front;
		if (StandInFront(GetWorld(), Exit, C, Front))
		{
			Stand = Front;
		}
		TestExitSpot = Stand;
		PlaceAndFace(C, Stand, Exit->GetActorLocation());
		// La demande de l'hote en partie en ligne (ce que fait RequestDeparture), avec la raison d'un refus
		uint8 Reason = 0;
		if (!W->ServerStartDeparture(C, TestLevel, W->GetLevelSerial(), Reason))
		{
			FString Why;
			W->CanUseExit(TestLevel, Why);
			Note(FString::Printf(TEXT("depart de groupe vers %d refuse a l'hote : raison %d (1 sortie fermee, 2 hors de portee ou sans vue, 3 autre depart, 4 niveau, 5 a terre) ; "
				"hote en (%.0f, %.0f, %.0f), sortie en (%.0f, %.0f, %.0f), %.0f cm ; %s ; vue : %s%s"), TestLevel, Reason, Stand.X, Stand.Y, Stand.Z, Exit->GetActorLocation().X,
				Exit->GetActorLocation().Y, Exit->GetActorLocation().Z, FVector::Dist(Stand, Exit->GetActorLocation()), Why.IsEmpty() ? TEXT("sortie utilisable") : *Why,
				*DepartureViewInfo(GetWorld(), C, Exit), W->GetLastDepartureBlock().IsEmpty() ? TEXT("") : *(TEXT(" ; jeu : ") + W->GetLastDepartureBlock())), true);
			// v4.15 : un seul probleme : les etapes qui dependent du depart sont sautees (avant : trois PROBLEMES de plus)
			TestLevel = -1;
		}
		TestTimer = FPlatformTime::Seconds();
		return true;
	});
	Add(TEXT("v4.11 reseau : coequipier attendu"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || TestLevel < 0)
		{
			return TestLevel < 0;
		}
		const FBRNetDeparture& Dep = W->GetDeparture();
		if (Dep.Phase != 1 && StepTime < 15.f)
		{
			return false; // client : depart pas encore recu
		}
		if (bClient && Dep.Phase != 1)
		{
			// v4.15 : un seul probleme (le client ne distingue pas un refus de l'hote d'une replication perdue) ; les etapes
			// qui dependent du depart sont sautees
			Note(FString::Printf(TEXT("client : depart de groupe non recu en 15 s (phase %d)"), static_cast<int32>(Dep.Phase)), true);
			TestLevel = -1;
			return true;
		}
		if (StepTime < 4.f)
		{
			return false; // le client reste loin 4 s
		}
		const bool bWaiting = Dep.Phase == 1 && Dep.Ready < Dep.Needed && W->GetLevelSerial() == TestCountA;
		Note(FString::Printf(TEXT("%s : depart en attente du coequipier %s (%d/%d rassembles, %.0f s restantes)"), bClient ? TEXT("client") : TEXT("hote"),
			YesNo(bWaiting), Dep.Ready, Dep.Needed, W->GetDepartureRemaining()), !bWaiting);
		Shot(bClient ? TEXT("v411_depart_client") : TEXT("v411_depart_hote"));
		return true;
	});
	Add(TEXT("v4.11 reseau : rassemblement"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || TestLevel < 0)
		{
			return TestLevel < 0;
		}
		if (bClient)
		{
			// Le client rejoint le point de depart (moins de 8 m, meme etage, a vue)
			const FVector Loc = W->GetDeparture().Location;
			if (TestMissionPhase != 4112)
			{
				// v4.15 : jamais vers un point de depart par defaut : seulement vers celui d'un depart recu
				if (W->GetDeparture().Phase != 1)
				{
					Note(FString::Printf(TEXT("client : rassemblement impossible, plus de depart en rassemblement (phase %d)"), static_cast<int32>(W->GetDeparture().Phase)), true);
					TestLevel = -1;
					return true;
				}
				FVector Stand;
				if (!StandNear(GetWorld(), Loc + FVector(0.f, 0.f, 90.f), FVector(-1.f, 0.f, 0.f), C->GetSimpleCollisionHalfHeight(), nullptr, C, Stand))
				{
					Stand = Loc + FVector(150.f, 0.f, C->GetSimpleCollisionHalfHeight() + 5.f);
				}
				Place.Start(C, Stand, &Loc, FPlatformTime::Seconds());
				TestMissionPhase = 4112;
				return false;
			}
			// La place doit etre recue par l'hote (sinon il compte le client ou il etait) ; le depart peut deja partir
			if (W->GetDeparture().Phase == 1 && !Place.Update(C, FPlatformTime::Seconds(), 0.3) && StepTime < 5.f)
			{
				return false;
			}
			TestMissionPhase = 0;
		}
		TestTimer = FPlatformTime::Seconds();
		return true;
	});
	Add(TEXT("v4.11 reseau : depart commun"), 1.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || TestLevel < 0)
		{
			return TestLevel < 0;
		}
		const double Elapsed = FPlatformTime::Seconds() - TestTimer;
		if ((W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != TestLevel) && Elapsed < 40.0)
		{
			return false;
		}
		const bool bOk = W->GetLevelNumber() == TestLevel && W->IsLevelReady();
		const FBRNetDeparture& Dep = W->GetDeparture();
		Note(FString::Printf(TEXT("%s : depart de groupe vers le Niveau %d %s (%.1f s apres le rassemblement)%s"), bClient ? TEXT("client") : TEXT("hote"), TestLevel,
			bOk ? TEXT("effectue") : TEXT("ECHEC"), Elapsed, bOk ? TEXT("") : *FString::Printf(TEXT(" ; depart : phase %d, raison %d, %d/%d rassembles"), static_cast<int32>(Dep.Phase),
			static_cast<int32>(Dep.Reason), Dep.Ready, Dep.Needed)), !bOk);
		return true;
	});
}
