// v4.17 (visuel) : banc des captures comparables de la passe graphique (-BRAutoTest -BRAutoTestV417Visual).
// Memes graines, memes poses et meme apparence a chaque version : l'avant (code de la v4.16 et ce banc) et l'apres se
// comparent image par image. Chaque capture a sa fiche (WriteShotMeta : Name.json et une ligne de Captures.csv : commit
// -BRCommit=, niveau, graines, pose et champ de la camera, definitions de sortie et de rendu, profil, temps depuis
// l'arrivee, alimentation de l'image et des regles, coupure, lampe, VHS, grain). L'interface est cachee pendant les
// captures de scene (ABRHUD::TestCaptureMode = 2 : ni bandeau du mode developpeur, ni sous-titres) ; les captures de
// l'inventaire gardent l'interface sans le bandeau (mode 1).
//   - Level 0 (graine 9, celle du banc v4.15) : salle du depart (image nette, puis VHS et grain), demi-tour, lumiere rasante
//     le long du mur le plus proche, plinthe, raccord de deux chunks (mur et plafond), porte de service fermee puis ouverte,
//     salle de fosses, combinaison a la 3e personne sous les neons (dos, face, cote) puis sous une lumiere neutre (6500 K,
//     posee par le banc dans le noir d'une coupure), lampe dans le noir, retour du courant (1,5 s et 4 s), entite de
//     controle (Bacteria du Level 0) figee devant le joueur, inventaire (PERSONNAGE et CARNET en francais, PERSONNAGE en
//     allemand et en arabe) ;
//   - Poolrooms (Niveau 37, graine 4242) : depart (net, VHS), demi-tour, trottoir le long d'un canal, bord d'un bassin
//     profond, regard rasant au ras de l'eau, camera sous l'eau, contre-jour vers un plafonnier ;
//   - Niveaux 1 et 2 (graine 4242) : depart, demi-tour, mur ; Niveau 2 : plafond ;
//   - sequences pour les videos (pas de temps fixe de 1/30 s pendant l'enregistrement : une image par pas, quelle que soit
//     la duree d'une capture) : marche au Level 0 a la 1re puis a la 3e personne (elle tourne devant un mur), Bacteria
//     lachee au bout du couloir et suivie par la camera, marche dans l'eau des Poolrooms a la 3e personne, marche au Niveau 1
//     (raccords du streaming) ; tirages locaux fixes (FMath::RandInit) avant chaque chargement : tubes qui clignotent au
//     meme instant d'un lancement a l'autre. Porte de service capturee lampe allumee (chemin hors des plafonniers). Images
//     v417_video_<scene>_<nnn>.png, assemblees hors du jeu (Tools/Visual/make_v417_media.py).
// Profil : Qualite, ou -BRProfile=<0|1|2|4> (un profil par lancement). Apparence fixee pendant le banc (VHS et grain coupes
// sauf captures _vhs, luminosite 0, flashs normaux, flou de mouvement coupe) puis retablie au bilan, comme la langue.
// Aucune verification de reussite : ce banc produit des images a juger et leurs fiches ; une pose introuvable est notee
// NON VERIFIE (Skip), jamais remplacee en silence.
#include "BRAutoTest.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BREntity.h"
#include "BRRig.h"
#include "BRHUD.h"
#include "BRInteractables.h"
#include "BRLevels.h"
#include "BRLoc.h"
#include "BRPlayerController.h"
#include "BRWorld.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace BRTestV417
{
	/** Attente de l'exposition et de Lumen apres un changement de pose (s) */
	constexpr float Settle = 3.f;
	constexpr float CloseSettle = 2.f;
	/** Noir d'une coupure : l'exposition descend lentement (banc v4.15 : 9 s la posent a moins de 0,1 IL) */
	constexpr float DarkSettle = 9.f;
	/** Images par sequence de video (pas de 1/30 s) */
	constexpr int32 VideoFrames = 90;
	constexpr int32 LongVideoFrames = 150;

	FVector SpawnLoc = FVector::ZeroVector;
	FRotator SpawnRot = FRotator::ZeroRotator;
	TWeakObjectPtr<ABREntity> Control;
	TWeakObjectPtr<ACameraActor> Cam;
	TWeakObjectPtr<UPointLightComponent> Neutral;
	int32 Frame = 0;
	int32 Shots = 0;
	double SuiteStart = 0.0;
	double RestoreStart = 0.0;
	FVector WalkDir = FVector(1.f, 0.f, 0.f);
	FString SavedLanguage;
	/** v4.17 (correction) : cadence fixe du moteur pendant une video (UEngine::bUseFixedFrameRate : le reglage de FApp seul
	 *  restait sans effet : 0,1 s de jeu par image, agent Technical Artist), etat d'avant, pas de jeu releves par image */
	bool bPrevFixedRate = false;
	float PrevFixedRate = 30.f;
	double LastVideoTime = -1.0;
	double MinVideoStep = 1.0e9;
	double MaxVideoStep = 0.0;
	/** v4.17 (correction) : images de la video de la Bacteria ou elle est rendue */
	int32 SeenFrames = 0;
	/** v4.17 (correction, QA visuelle) : camera qui suit (Bacteria, marche vue de cote) et cote de la marche le plus libre */
	FVector TrackCam = FVector::ZeroVector;
	float FeetSide = 1.f;
	/** v4.17 (correction, QA visuelle) : mesures de la prochaine image de video (ABRAutoTest::ShotExtra) */
	FString Extra;

	/** Point ou poser une camera qui suit Target : Want, ramene devant le premier obstacle (mur, pilier) */
	FVector ClearCamera(UWorld* InWorld, const FVector& Target, const FVector& Want, const AActor* IgnoreA, const AActor* IgnoreB)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRV417TrackCam), false, IgnoreA);
		if (IgnoreB)
		{
			Q.AddIgnoredActor(IgnoreB);
		}
		if (InWorld && InWorld->LineTraceSingleByChannel(Hit, Target, Want, ECC_Visibility, Q))
		{
			return Hit.Location + (Target - Want).GetSafeNormal() * 20.f;
		}
		return Want;
	}
	/** v4.17 (correction) : marche des videos : images sans avancer (coin, obstacle bas) */
	int32 Stuck = 0;
	FVector LastWalkLoc = FVector::ZeroVector;

	FString Scene(const TCHAR* Name)
	{
		return FString::Printf(TEXT("v417_%s"), Name);
	}

	/** Cellules autour de Start, anneau par anneau (ordre fixe : la meme cellule a chaque version pour la meme graine) */
	template <typename FnT>
	bool ForRings(const FIntPoint& Start, int32 MaxRing, FnT&& Fn)
	{
		for (int32 Ring = 0; Ring <= MaxRing; ++Ring)
		{
			for (int32 DX = -Ring; DX <= Ring; ++DX)
			{
				for (int32 DY = -Ring; DY <= Ring; ++DY)
				{
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) == Ring && Fn(FIntPoint(Start.X + DX, Start.Y + DY)))
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	void Place(ABRCharacter* C, ABRPlayerController* PC, const FVector& Loc, const FRotator& Rot)
	{
		C->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* Move = C->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}
		PC->SetControlRotation(Rot);
		C->Sanity = 100.f;
		C->Health = 100.f;
	}

	/** Mur le plus proche du point de depart le long de X ou de Y (+ d'abord a egalite) : point d'impact et normale */
	bool NearestWall(const UWorld* InWorld, const AActor* Ignored, FVector& OutImpact, FVector& OutFace)
	{
		if (!InWorld)
		{
			return false;
		}
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(BRV417Wall), false, Ignored);
		const FVector From = SpawnLoc + FVector(0.f, 0.f, 60.f);
		float Best = 1.0e9f;
		bool bFound = false;
		static const FVector Dirs[4] = { FVector(1.f, 0.f, 0.f), FVector(-1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector(0.f, -1.f, 0.f) };
		for (const FVector& Dir : Dirs)
		{
			FHitResult Hit;
			if (!InWorld->LineTraceSingleByChannel(Hit, From, From + Dir * 1500.f, ECC_Visibility, Query) || Hit.Distance + 1.f >= Best)
			{
				continue;
			}
			Best = Hit.Distance;
			OutImpact = Hit.ImpactPoint;
			OutFace = Hit.ImpactNormal.GetSafeNormal2D();
			if (OutFace.IsNearlyZero())
			{
				OutFace = -Dir;
			}
			bFound = true;
		}
		return bFound;
	}

	/** Direction (parmi 8, ordre fixe) ou la marche est libre le plus loin depuis le point de depart */
	FVector FreestDirection(const UWorld* InWorld, const AActor* Ignored, float& OutFree)
	{
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(BRV417Walk), false, Ignored);
		const FVector From = SpawnLoc + FVector(0.f, 0.f, 20.f);
		FVector Best = FVector(1.f, 0.f, 0.f);
		OutFree = 0.f;
		for (int32 K = 0; K < 8 && InWorld; ++K)
		{
			const float A = FMath::DegreesToRadians(45.f * K);
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
			FHitResult Hit;
			const float Free = InWorld->SweepSingleByChannel(Hit, From, From + Dir * 2500.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(40.f), Query)
				? Hit.Distance : 2500.f;
			if (Free > OutFree + 1.f)
			{
				OutFree = Free;
				Best = Dir;
			}
		}
		return Best;
	}

	/** Place libre devant le personnage dans la direction Dir (sphere de 40 cm, hauteur de la taille) */
	float FreeAhead(const ABRCharacter* C, const FVector& Dir, float Max)
	{
		const UWorld* InWorld = C ? C->GetWorld() : nullptr;
		if (!InWorld)
		{
			return 0.f;
		}
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(BRV417Steer), false, C);
		// v4.17 (correction) : sonde a la hauteur de la taille, un peu plus fine que la capsule (les plinthes et les rebords
		// arretaient la sphere de 40 cm au centre de la capsule)
		const FVector From = C->GetActorLocation() + FVector(0.f, 0.f, 30.f);
		FHitResult Hit;
		return InWorld->SweepSingleByChannel(Hit, From, From + Dir * Max, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(32.f), Query) ? Hit.Distance : Max;
	}

	/** Marche des videos : tout droit ; a moins de 1,6 m d'un obstacle, nouveau cap vers le cote le plus libre (gauche,
	 *  droite, puis demi-tour, ordre fixe a egalite), atteint en tournant de 9 degres par pas (pas de saut de camera) */
	void SteerWalk(ABRCharacter* C)
	{
		ABRPlayerController* PC = C ? Cast<ABRPlayerController>(C->GetController()) : nullptr;
		if (!PC)
		{
			return;
		}
		// v4.17 (correction) : bloque 8 images de suite (coin, obstacle que la sonde ne voit pas) : nouveau cap comme devant un mur
		const FVector Here = C->GetActorLocation();
		Stuck = FVector::Dist2D(Here, LastWalkLoc) < 1.f ? Stuck + 1 : 0;
		LastWalkLoc = Here;
		if (FreeAhead(C, WalkDir, 160.f) < 159.f || Stuck >= 8)
		{
			Stuck = 0;
			const FVector Left(-WalkDir.Y, WalkDir.X, 0.f);
			const FVector Right(WalkDir.Y, -WalkDir.X, 0.f);
			const float FL = FreeAhead(C, Left, 1500.f);
			const float FR = FreeAhead(C, Right, 1500.f);
			WalkDir = FL >= FR ? Left : Right;
			if (FMath::Max(FL, FR) < 160.f)
			{
				WalkDir = -WalkDir;
			}
		}
		FRotator Rot = PC->GetControlRotation();
		const float Delta = FMath::FindDeltaAngleDegrees(Rot.Yaw, WalkDir.Rotation().Yaw);
		Rot.Yaw += FMath::Clamp(Delta, -9.f, 9.f);
		PC->SetControlRotation(Rot);
		C->AddMovementInput(FRotator(0.f, Rot.Yaw, 0.f).Vector(), 1.f);
	}

	/** Camera independante (face de la combinaison) : memes reglages d'image que la camera du joueur */
	ACameraActor* ShowCamera(UWorld* InWorld, const UCameraComponent* Theirs, ABRPlayerController* PC, const FVector& Loc, const FRotator& Rot)
	{
		ACameraActor* A = Cam.Get();
		if (!A && InWorld)
		{
			A = InWorld->SpawnActor<ACameraActor>(Loc, Rot);
			Cam = A;
		}
		if (!A)
		{
			return nullptr;
		}
		A->SetActorLocationAndRotation(Loc, Rot);
		if (UCameraComponent* Mine = A->GetCameraComponent())
		{
			if (Theirs)
			{
				Mine->SetFieldOfView(Theirs->FieldOfView);
				Mine->PostProcessSettings = Theirs->PostProcessSettings;
				Mine->PostProcessBlendWeight = Theirs->PostProcessBlendWeight;
			}
			Mine->bConstrainAspectRatio = false;
		}
		PC->SetViewTarget(A);
		return A;
	}

	void HideCamera(ABRCharacter* C, ABRPlayerController* PC)
	{
		if (PC && C)
		{
			PC->SetViewTarget(C);
		}
	}

	void SetThird(ABRCharacter* C, bool bThird)
	{
		if (C && C->IsThirdPerson() != bThird)
		{
			C->ToggleThirdPerson();
		}
	}

	void SetLamp(ABRCharacter* C, bool bOn)
	{
		if (C && C->IsFlashlightOn() != bOn)
		{
			C->ToggleFlashlight();
		}
	}

	void KillControl()
	{
		if (ABREntity* E = Control.Get())
		{
			E->Destroy();
		}
		Control.Reset();
	}
}

void ABRAutoTest::AddV417VisualSteps()
{
	using namespace BRTestV417;
	int32 Profile = 1;
	FParse::Value(FCommandLine::Get(), TEXT("BRProfile="), Profile);

	auto SetHudMode = [this](int32 Mode)
	{
		if (ABRPlayerController* PC = GetPC())
		{
			if (ABRHUD* Hud = Cast<ABRHUD>(PC->GetHUD()))
			{
				Hud->TestCaptureMode = Mode;
			}
		}
	};
	// Capture de scene : fiche + compte
	auto Snap = [this](const FString& Name)
	{
		Shot(Name);
		++BRTestV417::Shots;
	};
	// Pose fixe, attente, capture
	auto AddPose = [this, Snap](const FString& Name, float Wait, TFunction<bool(ABRWorld*, ABRCharacter*, ABRPlayerController*)> Pose)
	{
		Add(FString::Printf(TEXT("v4.17 visuel : pose %s"), *Name), Wait, [this, Name, Pose]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			ABRPlayerController* PC = GetPC();
			if (!W || !C || !PC || W->IsTransitioning())
			{
				return false;
			}
			if (!Pose(W, C, PC))
			{
				Skip(FString::Printf(TEXT("%s : pose introuvable dans ce chargement (capture prise a la pose precedente)"), *Name));
			}
			return true;
		});
		Add(FString::Printf(TEXT("v4.17 visuel : capture %s"), *Name), 0.3f, [Snap, Name]()
		{
			Snap(Name);
			return true;
		});
	};
	// Sequence de video : une capture par pas de 1/30 s ; Move : deplacement de chaque pas
	auto AddVideo = [this](const FString& Name, int32 Frames, TFunction<void(ABRCharacter*)> Move)
	{
		Add(FString::Printf(TEXT("v4.17 visuel : video %s : debut"), *Name), 0.f, [this, Name]()
		{
			BRTestV417::Frame = 0;
			FApp::SetFixedDeltaTime(1.0 / 30.0);
			FApp::SetUseFixedTimeStep(true);
			if (GEngine)
			{
				BRTestV417::bPrevFixedRate = GEngine->bUseFixedFrameRate;
				BRTestV417::PrevFixedRate = GEngine->FixedFrameRate;
				GEngine->bUseFixedFrameRate = true;
				GEngine->FixedFrameRate = 30.f;
			}
			BRTestV417::LastVideoTime = -1.0;
			BRTestV417::MinVideoStep = 1.0e9;
			BRTestV417::MaxVideoStep = 0.0;
			BRTestV417::Stuck = 0;
			if (const ABRCharacter* C0 = GetPlayer())
			{
				BRTestV417::LastWalkLoc = C0->GetActorLocation();
			}
			Note(FString::Printf(TEXT("%s : enregistrement a pas fixe de 1/30 s"), *Name));
			return true;
		});
		Add(FString::Printf(TEXT("v4.17 visuel : video %s"), *Name), 0.f, [this, Name, Frames, Move]()
		{
			ABRCharacter* C = GetPlayer();
			if (!C)
			{
				return false;
			}
			Move(C);
			ShotExtra = BRTestV417::Extra;
			BRTestV417::Extra.Reset();
			if (const ABRWorld* VW = GetBRWorld())
			{
				const double T = VW->GetLevelTime();
				if (BRTestV417::LastVideoTime >= 0.0)
				{
					BRTestV417::MinVideoStep = FMath::Min(BRTestV417::MinVideoStep, T - BRTestV417::LastVideoTime);
					BRTestV417::MaxVideoStep = FMath::Max(BRTestV417::MaxVideoStep, T - BRTestV417::LastVideoTime);
				}
				BRTestV417::LastVideoTime = T;
			}
			Shot(FString::Printf(TEXT("v417_video_%s_%03d"), *Name, BRTestV417::Frame));
			return ++BRTestV417::Frame >= Frames;
		});
		LimitLastStep(240.f);
		Add(FString::Printf(TEXT("v4.17 visuel : video %s : fin"), *Name), 0.5f, [this, Name, Frames]()
		{
			FApp::SetUseFixedTimeStep(false);
			if (GEngine)
			{
				GEngine->bUseFixedFrameRate = BRTestV417::bPrevFixedRate;
				GEngine->FixedFrameRate = BRTestV417::PrevFixedRate;
			}
			if (ABRCharacter* C = GetPlayer())
			{
				if (UCharacterMovementComponent* Move = C->GetCharacterMovement())
				{
					Move->StopMovementImmediately();
				}
			}
			// Pas de jeu reellement obtenus (temps du niveau entre deux captures) : 1/30 s attendu ; un autre pas est un
			// defaut du banc, pas une video valable
			const bool bFixed = BRTestV417::MaxVideoStep < 0.04 && BRTestV417::MinVideoStep > 0.026;
			Note(FString::Printf(TEXT("%s : %d images, pas de jeu de %.4f a %.4f s"), *Name, Frames, BRTestV417::MinVideoStep, BRTestV417::MaxVideoStep));
			if (!bFixed)
			{
				Skip(FString::Printf(TEXT("%s : pas de jeu de %.3f a %.3f s (attendu 1/30 s) : video a pas variable"), *Name, BRTestV417::MinVideoStep, BRTestV417::MaxVideoStep));
			}
			return true;
		});
	};

	Add(TEXT("v4.17 banc visuel : debut"), 0.f, [this, Profile, SetHudMode]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC || !GetBRWorld())
		{
			return false;
		}
		if (!bSettingsSaved)
		{
			SavedSettings = FBRSettings::Get();
			bSettingsSaved = true;
		}
		BRTestV417::SavedLanguage = FString(BRLoc::Current().Code);
		FBRSettings& S = FBRSettings::Get();
		S.bVHSEffect = false;
		S.bFilmGrain = false;
		S.Brightness = 0.f;
		S.Flashes = 0;
		S.bMotionBlur = false;
		ABRPlayerController::ApplyGraphicsProfile(Profile);
		PC->ApplySettings();
		SetHudMode(2);
		BRTestV417::Shots = 0;
		BRTestV417::SuiteStart = FPlatformTime::Seconds();
		// v4.17 (correction, QA visuelle) : banc fige : les poses ne dependent plus du directeur de tension (Smilers, sursauts,
		// coupures tirees) ni des neons qui clignotent ; seules les entites posees par le banc apparaissent
		ABRWorld::bTestFreeze = true;
		FLevelReport R;
		R.Title = TEXT("v4.17 banc visuel (poses fixes, fiches des captures)");
		R.Scene = TEXT("v417");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		FString Commit = TEXT("inconnu");
		FParse::Value(FCommandLine::Get(), TEXT("BRCommit="), Commit);
		Note(FString::Printf(TEXT("banc visuel v4.17 : profil %d, commit %s, ray tracing materiel %s ; apparence fixee : VHS et grain coupes (sauf captures _vhs), luminosite 0, flashs normaux, flou de mouvement coupe ; interface cachee (scene seule) ; banc fige (ni rencontre, ni phenomene, ni coupure tiree, neons tenus allumes)"),
			Profile, *Commit, ABRPlayerController::IsHardwareRayTracingAvailable() ? TEXT("actif") : TEXT("indisponible")));
		return true;
	});

	// ===================================================================================================== Level 0
	Add(TEXT("v4.17 visuel : tirages locaux fixes (Level 0, graine 9)"), 0.f, []()
	{
		FMath::RandInit(4170);
		return true;
	});
	AddSeedLoad(0, 9u, TEXT("v4.17 visuel : Level 0, graine 9"));
	Add(TEXT("v4.17 visuel : Level 0 : pose de depart"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (!C || !PC)
		{
			return false;
		}
		BRTestV417::SpawnLoc = C->GetActorLocation();
		BRTestV417::SpawnRot = FRotator(0.f, PC->GetControlRotation().Yaw, 0.f);
		return true;
	});
	AddPose(Scene(TEXT("l0_salle")), Settle, [](ABRWorld*, ABRCharacter* C, ABRPlayerController* PC)
	{
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::SpawnRot);
		return true;
	});
	Add(TEXT("v4.17 visuel : VHS et grain (Level 0)"), 1.f, [this]()
	{
		FBRSettings::Get().bVHSEffect = true;
		FBRSettings::Get().bFilmGrain = true;
		if (ABRPlayerController* PC = GetPC())
		{
			PC->ApplySettings();
		}
		return true;
	});
	// v4.17 (correction, QA visuelle) : 2,5 s apres le basculement : l'exposition s'est reposee (avant : 0,3 s)
	Add(TEXT("v4.17 visuel : capture l0_salle_vhs"), 2.5f, [Snap]()
	{
		Snap(Scene(TEXT("l0_salle_vhs")));
		return true;
	});
	Add(TEXT("v4.17 visuel : image nette (Level 0)"), 0.5f, [this]()
	{
		FBRSettings::Get().bVHSEffect = false;
		FBRSettings::Get().bFilmGrain = false;
		if (ABRPlayerController* PC = GetPC())
		{
			PC->ApplySettings();
		}
		return true;
	});
	AddPose(Scene(TEXT("l0_dos")), Settle, [](ABRWorld*, ABRCharacter* C, ABRPlayerController* PC)
	{
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::SpawnRot + FRotator(0.f, 180.f, 0.f));
		return true;
	});
	// Lumiere rasante : a 120 cm du mur le plus proche, regard presque le long du mur (70 degres de sa normale), un peu vers
	// le bas : papier peint, plinthe et moquette sous la lumiere des plafonniers
	AddPose(Scene(TEXT("l0_rasante")), CloseSettle, [](ABRWorld* W, ABRCharacter* C, ABRPlayerController* PC)
	{
		FVector Impact, Face;
		if (!BRTestV417::NearestWall(W->GetWorld(), C, Impact, Face))
		{
			return false;
		}
		const FVector Loc(Impact.X + Face.X * 120.f, Impact.Y + Face.Y * 120.f, BRTestV417::SpawnLoc.Z);
		BRTestV417::Place(C, PC, Loc, FRotator(-10.f, (-Face).Rotation().Yaw + 70.f, 0.f));
		return true;
	});
	AddPose(Scene(TEXT("l0_plinthe")), CloseSettle, [](ABRWorld* W, ABRCharacter* C, ABRPlayerController* PC)
	{
		FVector Impact, Face;
		if (!BRTestV417::NearestWall(W->GetWorld(), C, Impact, Face))
		{
			return false;
		}
		const FVector Loc(Impact.X + Face.X * 70.f, Impact.Y + Face.Y * 70.f, BRTestV417::SpawnLoc.Z);
		BRTestV417::Place(C, PC, Loc, FRotator(-38.f, (-Face).Rotation().Yaw + 25.f, 0.f));
		return true;
	});
	// Raccord de deux chunks : premier mur continu (face Nord de deux cellules voisines en X) dont les cellules sont dans
	// deux chunks differents ; regard vers le raccord, puis vers le plafond au meme endroit
	for (int32 Look = 0; Look < 2; ++Look)
	{
		AddPose(Scene(Look == 0 ? TEXT("l0_raccord_mur") : TEXT("l0_raccord_plafond")), CloseSettle, [Look](ABRWorld* W, ABRCharacter* C, ABRPlayerController* PC)
		{
			const FBRLevelDef& Def = W->Def();
			FVector Loc = FVector::ZeroVector;
			float Yaw = 0.f;
			const bool bFound = BRTestV417::ForRings(W->WorldToCell(BRTestV417::SpawnLoc), 10, [&](const FIntPoint& Cell)
			{
				const FIntPoint Next(Cell.X + 1, Cell.Y);
				if (W->CellToChunk(Cell) == W->CellToChunk(Next) || !W->IsWalkable(Cell) || !W->IsWalkable(Next)
					|| !W->IsChunkLoaded(W->CellToChunk(Cell)) || !W->IsChunkLoaded(W->CellToChunk(Next))
					|| W->EdgeE(Cell.X, Cell.Y) != EBREdge::Open || W->IsPitRoomCell(Cell.X, Cell.Y) || W->IsPitRoomCell(Next.X, Next.Y))
				{
					return false;
				}
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const int32 Y = Side == 0 ? Cell.Y : Cell.Y - 1;
					if (W->EdgeN(Cell.X, Y) == EBREdge::Wall && W->EdgeN(Next.X, Y) == EBREdge::Wall)
					{
						const FVector A = W->CellCenter(Cell);
						const FVector B = W->CellCenter(Next);
						const float Toward = Side == 0 ? 1.f : -1.f;
						Loc = FVector((A.X + B.X) * 0.5f, A.Y - Toward * (Def.CellSize * 0.5f - 160.f), 0.f);
						Yaw = Side == 0 ? 90.f : -90.f;
						return true;
					}
				}
				return false;
			});
			if (!bFound)
			{
				return false;
			}
			Loc.Z = W->FloorZAt(Loc) + C->GetSimpleCollisionHalfHeight() + 5.f;
			BRTestV417::Place(C, PC, Loc, FRotator(Look == 0 ? -25.f : 50.f, Yaw, 0.f));
			return true;
		});
	}
	// Porte de service : a 3 m devant elle, regard sur le battant ; fermee, puis ouverte (battant anime, 2 s). Elle est dans
	// le mur d'enceinte, loin du depart : on attend que les chunks autour d'elle soient construits (avant : capture noire,
	// salle pas encore construite)
	Add(TEXT("v4.17 visuel : porte de service : devant elle"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (!W || !C || !PC)
		{
			return false;
		}
		if (!W->HasServiceDoor())
		{
			Skip(TEXT("v417_l0_porte_fermee : pas de porte de service dans ce chargement"));
			return true;
		}
		const FVector Door = W->GetServiceDoorLocation();
		const FVector Out = FRotator(0.f, W->GetServiceDoorYaw(), 0.f).Vector();
		FVector Loc = Door + Out * 300.f;
		Loc.Z = W->FloorZAt(Loc) + C->GetSimpleCollisionHalfHeight() + 5.f;
		BRTestV417::Place(C, PC, Loc, ((Door + FVector(0.f, 0.f, 110.f)) - (Loc + FVector(0.f, 0.f, C->BaseEyeHeight))).Rotation());
		// Le chemin de la porte finit hors des plafonniers (premier passage : image noire, chunks prets) : lampe allumee,
		// comme le joueur qui y arrive
		BRTestV417::SetLamp(C, true);
		Note(FString::Printf(TEXT("porte de service : lumiere de jeu %.2f devant elle (lampe allumee pour les deux captures)"), W->LightLevelAt(Loc)));
		return true;
	});
	Add(TEXT("v4.17 visuel : porte de service : salles construites"), Settle + 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || !W->HasServiceDoor())
		{
			return true;
		}
		const FIntPoint Here = W->CellToChunk(W->WorldToCell(C->GetActorLocation()));
		const FIntPoint Door = W->CellToChunk(W->GetServiceDoorCell());
		bool bReady = W->IsChunkLoaded(Here) && W->IsChunkLoaded(Door);
		for (int32 DX = -1; DX <= 1 && bReady; ++DX)
		{
			for (int32 DY = -1; DY <= 1 && bReady; ++DY)
			{
				const FIntPoint Near(Here.X + DX, Here.Y + DY);
				bReady = !W->IsChunkInBounds(Near) || W->IsChunkLoaded(Near);
			}
		}
		if (!bReady && StepTime < 20.f)
		{
			return false;
		}
		if (!bReady)
		{
			Skip(TEXT("v417_l0_porte_fermee : chunks autour de la porte pas construits apres 20 s (capture prise quand meme)"));
		}
		Note(FString::Printf(TEXT("porte de service : chunks prets apres %.1f s"), StepTime));
		return true;
	});
	Add(TEXT("v4.17 visuel : capture l0_porte_fermee"), 0.3f, [Snap]()
	{
		Snap(Scene(TEXT("l0_porte_fermee")));
		return true;
	});
	Add(TEXT("v4.17 visuel : porte de service : ouverture"), 2.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		bool bDone = false;
		for (TActorIterator<ABRExit> It(W->GetWorld()); It && !bDone; ++It)
		{
			if (It->Style == EBRExitStyle::ServiceDoor)
			{
				It->SetServiceDoorOpen(true, true);
				bDone = true;
			}
		}
		if (!bDone)
		{
			Skip(TEXT("v417_l0_porte_ouverte : porte de service absente des chunks construits"));
		}
		return true;
	});
	Add(TEXT("v4.17 visuel : capture l0_porte_ouverte"), 0.3f, [this, Snap]()
	{
		Snap(Scene(TEXT("l0_porte_ouverte")));
		BRTestV417::SetLamp(GetPlayer(), false);
		return true;
	});
	// Salle de fosses (point de vue de la commande BRPits)
	Add(TEXT("v4.17 visuel : pose l0_fosses"), Settle, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!PC || !C)
		{
			return false;
		}
		PC->BRPits();
		C->Sanity = 100.f;
		return true;
	});
	Add(TEXT("v4.17 visuel : capture l0_fosses"), 0.3f, [Snap]()
	{
		Snap(Scene(TEXT("l0_fosses")));
		return true;
	});

	// ---------------------------------------------------------------------------------- combinaison sous les neons
	AddPose(Scene(TEXT("l0_combinaison_neon_dos")), Settle, [](ABRWorld*, ABRCharacter* C, ABRPlayerController* PC)
	{
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::SpawnRot);
		BRTestV417::SetThird(C, true);
		return true;
	});
	for (int32 Side = 0; Side < 2; ++Side)
	{
		AddPose(Scene(Side == 0 ? TEXT("l0_combinaison_neon_face") : TEXT("l0_combinaison_neon_cote")), CloseSettle, [Side](ABRWorld* W, ABRCharacter* C, ABRPlayerController* PC)
		{
			const FRotator Facing(0.f, C->GetActorRotation().Yaw, 0.f);
			const FVector Dir = (Side == 0 ? Facing : Facing + FRotator(0.f, 90.f, 0.f)).Vector();
			const FVector Chest = C->GetActorLocation() + FVector(0.f, 0.f, 30.f);
			const FVector Loc = Chest + Dir * 230.f + FVector(0.f, 0.f, 15.f);
			return BRTestV417::ShowCamera(W->GetWorld(), C->Camera, PC, Loc, (Chest - Loc).Rotation()) != nullptr;
		});
	}
	Add(TEXT("v4.17 visuel : camera du joueur"), 0.5f, [this]()
	{
		BRTestV417::HideCamera(GetPlayer(), GetPC());
		return true;
	});

	// ---------------------------------------------------------------------------------- coupure : lumiere neutre, lampe
	Add(TEXT("v4.17 visuel : coupure (Level 0)"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (!W || !C || !PC)
		{
			return false;
		}
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::SpawnRot);
		BRTestV417::SetLamp(C, false);
		W->ForceBlackout();
		return true;
	});
	Add(TEXT("v4.17 visuel : noir complet (Level 0)"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (!W->IsBlackout() || W->GetPower() > 0.01f)
		{
			if (StepTime > 10.f)
			{
				Skip(TEXT("coupure du Level 0 : le noir n'est pas atteint (captures de la lumiere neutre et de la lampe sans valeur)"));
				return true;
			}
			return false;
		}
		return true;
	});
	// Lumiere neutre posee par le banc : 6500 K, 1,6 m devant et a droite du joueur, a 1,9 m du sol, ombres portees
	Add(TEXT("v4.17 visuel : lumiere neutre"), DarkSettle, [this]()
	{
		ABRCharacter* C = GetPlayer();
		if (!C)
		{
			return false;
		}
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetMobility(EComponentMobility::Movable);
		L->RegisterComponent();
		const FRotator Facing(0.f, C->GetActorRotation().Yaw, 0.f);
		const FVector Foot = C->GetActorLocation() - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight());
		L->SetWorldLocation(Foot + Facing.RotateVector(FVector(160.f, 90.f, 190.f)));
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(60.f);
		L->SetAttenuationRadius(900.f);
		L->SetUseTemperature(true);
		L->SetTemperature(6500.f);
		L->SetCastShadows(true);
		BRTestV417::Neutral = L;
		BRTestV417::SetThird(C, true);
		return true;
	});
	AddPose(Scene(TEXT("l0_combinaison_neutre_dos")), 1.f, [](ABRWorld*, ABRCharacter*, ABRPlayerController*)
	{
		return BRTestV417::Neutral.IsValid();
	});
	AddPose(Scene(TEXT("l0_combinaison_neutre_face")), CloseSettle, [](ABRWorld* W, ABRCharacter* C, ABRPlayerController* PC)
	{
		const FRotator Facing(0.f, C->GetActorRotation().Yaw, 0.f);
		const FVector Chest = C->GetActorLocation() + FVector(0.f, 0.f, 30.f);
		const FVector Loc = Chest + Facing.Vector() * 230.f + FVector(0.f, 0.f, 15.f);
		return BRTestV417::Neutral.IsValid() && BRTestV417::ShowCamera(W->GetWorld(), C->Camera, PC, Loc, (Chest - Loc).Rotation()) != nullptr;
	});
	Add(TEXT("v4.17 visuel : fin de la lumiere neutre"), 0.5f, [this]()
	{
		if (UPointLightComponent* L = BRTestV417::Neutral.Get())
		{
			L->DestroyComponent();
		}
		BRTestV417::Neutral.Reset();
		ABRCharacter* C = GetPlayer();
		BRTestV417::HideCamera(C, GetPC());
		BRTestV417::SetThird(C, false);
		return true;
	});
	Add(TEXT("v4.17 visuel : adaptation au noir (Level 0)"), DarkSettle, []()
	{
		return true;
	});
	Add(TEXT("v4.17 visuel : capture l0_coupure"), 0.3f, [Snap]()
	{
		Snap(Scene(TEXT("l0_coupure")));
		return true;
	});
	Add(TEXT("v4.17 visuel : lampe dans le noir"), 4.f, [this]()
	{
		BRTestV417::SetLamp(GetPlayer(), true);
		return true;
	});
	Add(TEXT("v4.17 visuel : capture l0_coupure_lampe"), 0.3f, [Snap]()
	{
		Snap(Scene(TEXT("l0_coupure_lampe")));
		return true;
	});
	Add(TEXT("v4.17 visuel : retour du courant"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		BRTestV417::SetLamp(C, false);
		if (!W->IsBlackout())
		{
			Skip(TEXT("retour du courant du Level 0 : la coupure etait deja finie (captures de retour sans valeur)"));
		}
		W->ForceRestore();
		BRTestV417::RestoreStart = FPlatformTime::Seconds();
		return true;
	});
	static const TCHAR* const Returns[] = { TEXT("l0_retour_15"), TEXT("l0_retour_40") };
	static const float ReturnAt[] = { 1.5f, 4.f };
	for (int32 K = 0; K < 2; ++K)
	{
		const FString Name = Scene(Returns[K]);
		const float At = ReturnAt[K];
		Add(FString::Printf(TEXT("v4.17 visuel : capture %s"), *Name), 0.3f, [this, Name, At, Snap]()
		{
			// Meme instant de la phase de retour a chaque version (temps reel depuis ForceRestore)
			if (FPlatformTime::Seconds() - BRTestV417::RestoreStart < At)
			{
				return false;
			}
			const ABRWorld* W = GetBRWorld();
			Note(FString::Printf(TEXT("%s : %.2f s apres le retour, alimentation de l'image %.2f, des regles %.2f"), *Name,
				FPlatformTime::Seconds() - BRTestV417::RestoreStart, W ? W->GetPower() : -1.f, W ? W->GetGameplayPower() : -1.f));
			Snap(Name);
			return true;
		});
	}

	// ---------------------------------------------------------------------------------- entite de controle (Bacteria)
	Add(TEXT("v4.17 visuel : courant revenu (Level 0)"), 3.f, [this]()
	{
		const ABRWorld* W = GetBRWorld();
		return W && !W->IsBlackout();
	});
	AddPose(Scene(TEXT("l0_entite_bacteria")), CloseSettle, [this](ABRWorld* W, ABRCharacter* C, ABRPlayerController* PC)
	{
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::SpawnRot);
		const FBREntityInfo& Info = ABREntity::Info(EBREntityKind::Bacteria);
		FVector Spot;
		if (!FindSpotInFront(450.f, Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 5.f, Spot))
		{
			return false;
		}
		ABREntity* E = W->SpawnEntity(EBREntityKind::Bacteria, Spot);
		if (!E)
		{
			return false;
		}
		FVector ToPlayer = C->GetActorLocation() - Spot;
		ToPlayer.Z = 0.f;
		E->SetActorRotation(ToPlayer.Rotation());
		E->CustomTimeDilation = 0.02f;
		BRTestV417::Control = E;
		PC->SetControlRotation((Spot - C->GetEyeLocation()).Rotation());
		return true;
	});
	Add(TEXT("v4.17 visuel : fin de l'entite figee"), 0.f, []()
	{
		BRTestV417::KillControl();
		return true;
	});

	// ---------------------------------------------------------------------------------- videos du Level 0
	Add(TEXT("v4.17 visuel : direction de marche (Level 0)"), 1.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (!C || !PC)
		{
			return false;
		}
		float Free = 0.f;
		BRTestV417::WalkDir = BRTestV417::FreestDirection(GetWorld(), C, Free);
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::WalkDir.Rotation());
		Note(FString::Printf(TEXT("videos du Level 0 : marche vers (%.2f, %.2f), %.0f cm libres"), BRTestV417::WalkDir.X, BRTestV417::WalkDir.Y, Free));
		return true;
	});
	AddVideo(TEXT("l0_marche"), VideoFrames, [](ABRCharacter* C)
	{
		BRTestV417::SteerWalk(C);
	});
	Add(TEXT("v4.17 visuel : 3e personne"), 2.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (!W || !C || !PC)
		{
			return false;
		}
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::WalkDir.Rotation());
		BRTestV417::SetThird(C, true);
		return true;
	});
	AddVideo(TEXT("l0_3p"), VideoFrames, [](ABRCharacter* C)
	{
		BRTestV417::SteerWalk(C);
	});
	// v4.17 (correction, QA visuelle) : la camera de la 3e personne coupe le personnage a mi-cuisse : marche vue de cote, camera
	// a 2,8 m du cote le plus libre et a 80 cm du sol, pieds dans le cadre (appuis, bassin, retard du bas du corps)
	Add(TEXT("v4.17 visuel : 3e personne vue de cote"), 1.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (!C || !PC)
		{
			return false;
		}
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::WalkDir.Rotation());
		BRTestV417::SetThird(C, true);
		const FVector Loc = C->GetActorLocation();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, BRTestV417::WalkDir).GetSafeNormal();
		const float FreeR = static_cast<float>(FVector::Dist(Loc, BRTestV417::ClearCamera(GetWorld(), Loc, Loc + Right * 400.f, C, nullptr)));
		const float FreeL = static_cast<float>(FVector::Dist(Loc, BRTestV417::ClearCamera(GetWorld(), Loc, Loc - Right * 400.f, C, nullptr)));
		BRTestV417::FeetSide = FreeR >= FreeL ? 1.f : -1.f;
		Note(FString::Printf(TEXT("l0_3p_cote : camera a %s (libre : droite %.0f cm, gauche %.0f cm)"), BRTestV417::FeetSide > 0.f ? TEXT("droite") : TEXT("gauche"), FreeR, FreeL));
		return true;
	});
	AddVideo(TEXT("l0_3p_cote"), VideoFrames, [](ABRCharacter* C)
	{
		BRTestV417::SteerWalk(C);
		ABRPlayerController* PC = C ? Cast<ABRPlayerController>(C->GetController()) : nullptr;
		if (!PC)
		{
			return;
		}
		const FVector Loc = C->GetActorLocation();
		const FVector Fwd = FRotator(0.f, C->GetActorRotation().Yaw, 0.f).Vector();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Fwd) * BRTestV417::FeetSide;
		const FVector Foot = Loc - FVector(0.f, 0.f, C->GetSimpleCollisionHalfHeight());
		const FVector Want = BRTestV417::ClearCamera(C->GetWorld(), Loc, Foot + Side * 280.f + Fwd * 60.f + FVector(0.f, 0.f, 80.f), C, nullptr);
		BRTestV417::TrackCam = BRTestV417::Frame == 0 ? Want : FMath::Lerp(BRTestV417::TrackCam, Want, 0.35f);
		const FVector Look = Foot + FVector(0.f, 0.f, 85.f);
		BRTestV417::ShowCamera(C->GetWorld(), C->Camera, PC, BRTestV417::TrackCam, (Look - BRTestV417::TrackCam).Rotation());
		FBRBodyPoseInfo P;
		if (BRRig::GetBodyPoseInfo(C, P))
		{
			BRTestV417::Extra = FString::Printf(TEXT("{ \"corps\": { \"lacet_jambes\": %.2f, \"ecart_jambes\": %.2f, \"pas_de_rattrapage\": %.2f, \"bassin_cm\": %.2f, \"appui_cm\": %.2f, ")
				TEXT("\"sol_pieds_cm\": [%.2f, %.2f], \"pied_gauche\": [%.1f, %.1f, %.1f], \"pied_droit\": [%.1f, %.1f, %.1f] } }"),
				P.LowerYaw, P.YawOffset, P.TurnStep, P.PelvisDrop, P.WalkDrop, P.FootGround[0], P.FootGround[1],
				P.FootWorld[0].X, P.FootWorld[0].Y, P.FootWorld[0].Z, P.FootWorld[1].X, P.FootWorld[1].Y, P.FootWorld[1].Z);
		}
	});
	Add(TEXT("v4.17 visuel : fin de la marche vue de cote"), 0.f, [this]()
	{
		BRTestV417::HideCamera(GetPlayer(), GetPC());
		return true;
	});
	// Entite de controle en mouvement : joueur immobile a la 1re personne au point de depart, Bacteria posee au bout de la
	// direction la plus libre (jusqu'a 12 m) et lachee ; la camera la suit (marche, demi-tour, appui des pattes)
	// v4.17 (correction) : tirages fixes juste avant l'apparition et enregistrement aussitot (pas fixe) : l'IA fait les memes
	// choix d'une version a l'autre (avant : chemin different apres l'image 0, entite cachee derriere un pilier)
	Add(TEXT("v4.17 visuel : Bacteria au bout du couloir"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (!W || !C || !PC)
		{
			return false;
		}
		BRTestV417::KillControl();
		BRTestV417::SetThird(C, false);
		float Free = 0.f;
		const FVector Dir = BRTestV417::FreestDirection(GetWorld(), C, Free);
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, Dir.Rotation());
		const float Dist = FMath::Clamp(Free - 100.f, 300.f, 1200.f);
		FMath::RandInit(4171);
		BRTestV417::SeenFrames = 0;
		const FBREntityInfo& Info = ABREntity::Info(EBREntityKind::Bacteria);
		FVector Spot = BRTestV417::SpawnLoc + Dir * Dist;
		Spot.Z = W->FloorZAt(Spot) + (Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 5.f);
		BRTestV417::Control = W->SpawnEntity(EBREntityKind::Bacteria, Spot);
		if (!BRTestV417::Control.IsValid())
		{
			Skip(TEXT("v417_video_l0_bacteria : la Bacteria n'a pas pu etre posee (video sans entite)"));
		}
		Note(FString::Printf(TEXT("video de la Bacteria : posee a %.0f cm du joueur"), Dist));
		return true;
	});
	AddVideo(TEXT("l0_bacteria"), LongVideoFrames, [](ABRCharacter* C)
	{
		ABRPlayerController* PC = C ? Cast<ABRPlayerController>(C->GetController()) : nullptr;
		if (ABREntity* E = BRTestV417::Control.Get())
		{
			BRTestV417::SeenFrames += E->WasRecentlyRendered(0.05f) ? 1 : 0;
			const FVector Target = E->GetActorLocation();
			if (PC)
			{
				// v4.17 (correction, QA visuelle) : camera qui suit l'entite a 3,5 m, du cote du joueur, a 1,5 m du sol, ramenee
				// devant un mur ou un pilier (avant : camera du joueur, entite cachee par un pilier des l'image 8)
				FVector Away = C->GetActorLocation() - Target;
				Away.Z = 0.f;
				Away = Away.GetSafeNormal(UE_SMALL_NUMBER, FVector(1.f, 0.f, 0.f));
				const float FloorZ = Target.Z - ABREntity::Info(EBREntityKind::Bacteria).HalfHeight;
				FVector Want = Target + Away * 350.f;
				Want.Z = FloorZ + 150.f;
				const FVector Clear = BRTestV417::ClearCamera(C->GetWorld(), Target, Want, C, E);
				BRTestV417::TrackCam = (BRTestV417::Frame == 0 || !Clear.Equals(Want, 1.f)) ? Clear : FMath::Lerp(BRTestV417::TrackCam, Clear, 0.25f);
				const FVector Look(Target.X, Target.Y, FloorZ + 50.f);
				BRTestV417::ShowCamera(C->GetWorld(), C->Camera, PC, BRTestV417::TrackCam, (Look - BRTestV417::TrackCam).Rotation());
			}
			TArray<FVector> Pts;
			E->GetSupportPoints(Pts);
			FString Json = FString::Printf(TEXT("{ \"entite\": { \"x\": %.1f, \"y\": %.1f, \"z\": %.1f, \"lacet_affiche\": %.2f, \"ecart_affiche_cm\": %.2f, \"sol_z\": %.1f, \"course\": %.2f, \"bond\": %s, \"rendue\": %s, \"appuis\": ["),
				Target.X, Target.Y, Target.Z, E->GetVisualYaw(), E->GetVisualLag(), E->GetSupportGroundZ(), E->GetSupportRun(), E->IsHopping() ? TEXT("true") : TEXT("false"),
				E->WasRecentlyRendered(0.05f) ? TEXT("true") : TEXT("false"));
			for (int32 i = 0; i < Pts.Num(); ++i)
			{
				Json += FString::Printf(TEXT("%s[%.1f, %.1f, %.1f]"), i ? TEXT(", ") : TEXT(""), Pts[i].X, Pts[i].Y, Pts[i].Z);
			}
			BRTestV417::Extra = Json + TEXT("] } }");
		}
	});
	Add(TEXT("v4.17 visuel : fin des videos du Level 0"), 0.5f, [this]()
	{
		Note(FString::Printf(TEXT("l0_bacteria : entite rendue sur %d image(s) sur %d"), BRTestV417::SeenFrames, BRTestV417::LongVideoFrames));
		BRTestV417::HideCamera(GetPlayer(), GetPC());
		BRTestV417::KillControl();
		BRTestV417::SetThird(GetPlayer(), false);
		return true;
	});

	// ---------------------------------------------------------------------------------- inventaire (interface sans bandeau)
	struct FInvShot
	{
		const TCHAR* Name;
		const TCHAR* Lang;
		int32 Tab;
	};
	static const FInvShot InvShots[] = { { TEXT("l0_inventaire_personnage_fr"), TEXT("fr"), 0 }, { TEXT("l0_inventaire_journal_fr"), TEXT("fr"), 1 },
		{ TEXT("l0_inventaire_carnet_fr"), TEXT("fr"), 4 },
		{ TEXT("l0_inventaire_personnage_de"), TEXT("de"), 0 }, { TEXT("l0_inventaire_personnage_ar"), TEXT("ar"), 0 } };
	Add(TEXT("v4.17 visuel : inventaire : pose"), 1.f, [this, SetHudMode]()
	{
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (!C || !PC)
		{
			return false;
		}
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::SpawnRot);
		SetHudMode(1);
		return true;
	});
	Add(TEXT("v4.17 visuel : capture l0_hud_jeu"), 0.3f, [Snap]()
	{
		Snap(Scene(TEXT("l0_hud_jeu")));
		return true;
	});
	for (const FInvShot& I : InvShots)
	{
		const FString Name = Scene(I.Name);
		const FString Lang = I.Lang;
		const int32 Tab = I.Tab;
		Add(FString::Printf(TEXT("v4.17 visuel : %s : ouverture"), *Name), 1.2f, [this, Name, Lang, Tab]()
		{
			ABRPlayerController* PC = GetPC();
			if (!PC)
			{
				return false;
			}
			if (FString(BRLoc::Current().Code) != Lang && !BRLoc::SetLanguage(Lang))
			{
				Skip(FString::Printf(TEXT("%s : langue %s refusee"), *Name, *Lang));
			}
			PC->SetInventoryOpen(true, Tab);
			return true;
		});
		Add(FString::Printf(TEXT("v4.17 visuel : capture %s"), *Name), 0.3f, [Snap, Name]()
		{
			Snap(Name);
			return true;
		});
	}
	Add(TEXT("v4.17 visuel : inventaire : fermeture"), 0.5f, [this, SetHudMode]()
	{
		if (ABRPlayerController* PC = GetPC())
		{
			PC->SetInventoryOpen(false);
		}
		BRLoc::SetLanguage(BRTestV417::SavedLanguage.IsEmpty() ? FString(TEXT("fr")) : BRTestV417::SavedLanguage);
		SetHudMode(2);
		return true;
	});

	// ===================================================================================================== Poolrooms
	Add(TEXT("v4.17 visuel : tirages locaux fixes (Poolrooms, graine 4242)"), 0.f, []()
	{
		FMath::RandInit(4170);
		return true;
	});
	AddSeedLoad(37, 4242u, TEXT("v4.17 visuel : Poolrooms, graine 4242"));
	Add(TEXT("v4.17 visuel : Poolrooms : pose de depart"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (!C || !PC)
		{
			return false;
		}
		BRTestV417::SpawnLoc = C->GetActorLocation();
		BRTestV417::SpawnRot = FRotator(0.f, PC->GetControlRotation().Yaw, 0.f);
		return true;
	});
	AddPose(Scene(TEXT("p_vue")), Settle, [](ABRWorld*, ABRCharacter* C, ABRPlayerController* PC)
	{
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::SpawnRot);
		return true;
	});
	Add(TEXT("v4.17 visuel : VHS et grain (Poolrooms)"), 1.f, [this]()
	{
		FBRSettings::Get().bVHSEffect = true;
		FBRSettings::Get().bFilmGrain = true;
		if (ABRPlayerController* PC = GetPC())
		{
			PC->ApplySettings();
		}
		return true;
	});
	Add(TEXT("v4.17 visuel : capture p_vue_vhs"), 2.5f, [Snap]()
	{
		Snap(Scene(TEXT("p_vue_vhs")));
		return true;
	});
	Add(TEXT("v4.17 visuel : image nette (Poolrooms)"), 0.5f, [this]()
	{
		FBRSettings::Get().bVHSEffect = false;
		FBRSettings::Get().bFilmGrain = false;
		if (ABRPlayerController* PC = GetPC())
		{
			PC->ApplySettings();
		}
		return true;
	});
	AddPose(Scene(TEXT("p_dos")), Settle, [](ABRWorld*, ABRCharacter* C, ABRPlayerController* PC)
	{
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::SpawnRot + FRotator(0.f, 180.f, 0.f));
		return true;
	});
	// Trottoir le long d'un canal : premiere cellule en eau avec un trottoir ; debout sur le trottoir, regard le long du canal
	AddPose(Scene(TEXT("p_trottoir")), Settle, [](ABRWorld* W, ABRCharacter* C, ABRPlayerController* PC)
	{
		static const FVector SideDir[4] = { FVector(1.f, 0.f, 0.f), FVector(-1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector(0.f, -1.f, 0.f) };
		FVector Loc = FVector::ZeroVector;
		FRotator Rot = FRotator::ZeroRotator;
		const float Half = C->GetSimpleCollisionHalfHeight();
		const bool bFound = BRTestV417::ForRings(W->WorldToCell(BRTestV417::SpawnLoc), 8, [&](const FIntPoint& Cell)
		{
			if (!W->IsWalkable(Cell) || W->IsPoolCell(Cell.X, Cell.Y) || !W->IsChunkLoaded(W->CellToChunk(Cell)))
			{
				return false;
			}
			const FVector Center = W->CellCenter(Cell);
			if (!W->WaterAt(Center + FVector(0.f, 0.f, 5.f)).bWater)
			{
				return false;
			}
			for (int32 Side = 0; Side < 4; ++Side)
			{
				if (!W->HasDeck(Cell.X, Cell.Y, Side))
				{
					continue;
				}
				// Premier point du trottoir en allant du centre vers ce cote
				for (float D = 40.f; D < W->CellSize() * 0.5f; D += 10.f)
				{
					const FVector P = Center + SideDir[Side] * D;
					const float Deck = W->DeckZAt(P);
					if (Deck > 1.f)
					{
						const FVector On = Center + SideDir[Side] * FMath::Min(D + 30.f, W->CellSize() * 0.5f - 45.f);
						Loc = FVector(On.X, On.Y, W->DeckZAt(On) + Half + 5.f);
						const FVector Along = Side < 2 ? FVector(0.f, 1.f, 0.f) : FVector(1.f, 0.f, 0.f);
						Rot = FRotator(-12.f, Along.Rotation().Yaw, 0.f);
						return true;
					}
				}
			}
			return false;
		});
		if (!bFound)
		{
			return false;
		}
		BRTestV417::Place(C, PC, Loc, Rot);
		return true;
	});
	// Bord d'un bassin profond : cellule voisine praticable, a 60 cm du bord, regard vers le centre du bassin
	AddPose(Scene(TEXT("p_bassin")), Settle, [](ABRWorld* W, ABRCharacter* C, ABRPlayerController* PC)
	{
		static const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		FVector Loc = FVector::ZeroVector;
		FVector Target = FVector::ZeroVector;
		const bool bFound = BRTestV417::ForRings(W->WorldToCell(BRTestV417::SpawnLoc), 10, [&](const FIntPoint& Cell)
		{
			if (!W->IsPoolCell(Cell.X, Cell.Y) || !W->IsChunkLoaded(W->CellToChunk(Cell)))
			{
				return false;
			}
			for (const FIntPoint& D : Dirs)
			{
				const FIntPoint Next(Cell.X + D.X, Cell.Y + D.Y);
				if (W->IsWalkable(Next) && !W->IsPoolCell(Next.X, Next.Y) && W->CanStep(Next, Cell))
				{
					const FVector Edge = (W->CellCenter(Cell) + W->CellCenter(Next)) * 0.5f;
					const FVector Back(static_cast<float>(D.X), static_cast<float>(D.Y), 0.f);
					Loc = Edge + Back * 60.f;
					Target = W->CellCenter(Cell);
					return true;
				}
			}
			return false;
		});
		if (!bFound)
		{
			return false;
		}
		Loc.Z = W->FloorZAt(Loc) + C->GetSimpleCollisionHalfHeight() + 5.f;
		BRTestV417::Place(C, PC, Loc, FRotator(-30.f, (Target - Loc).GetSafeNormal2D().Rotation().Yaw, 0.f));
		return true;
	});
	// Regard rasant : oeil a 10 cm au-dessus de l'eau d'un bassin (vol du developpeur), vers le centre du bassin puis au-dela ;
	// camera sous l'eau : oeil a 45 cm sous la surface, meme regard
	for (int32 Under = 0; Under < 2; ++Under)
	{
		AddPose(Scene(Under == 0 ? TEXT("p_rasant") : TEXT("p_sous_eau")), Settle, [Under](ABRWorld* W, ABRCharacter* C, ABRPlayerController* PC)
		{
			FVector Eye = FVector::ZeroVector;
			float Yaw = 0.f;
			static const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
			const bool bFound = BRTestV417::ForRings(W->WorldToCell(BRTestV417::SpawnLoc), 10, [&](const FIntPoint& Cell)
			{
				if (!W->IsPoolCell(Cell.X, Cell.Y) || !W->IsChunkLoaded(W->CellToChunk(Cell)))
				{
					return false;
				}
				const FVector Center = W->CellCenter(Cell);
				const BRMech::FWaterQuery Q = W->WaterAt(Center + FVector(0.f, 0.f, -50.f));
				if (!Q.bWater)
				{
					return false;
				}
				for (const FIntPoint& D : Dirs)
				{
					const FIntPoint Next(Cell.X + D.X, Cell.Y + D.Y);
					if (W->IsWalkable(Next) && !W->IsPoolCell(Next.X, Next.Y))
					{
						const FVector Back(static_cast<float>(D.X), static_cast<float>(D.Y), 0.f);
						const FVector P = Center + Back * (W->CellSize() * 0.5f - 80.f);
						Eye = FVector(P.X, P.Y, Q.Surface + (Under == 0 ? 10.f : -45.f));
						Yaw = (-Back).Rotation().Yaw;
						return true;
					}
				}
				return false;
			});
			if (!bFound)
			{
				return false;
			}
			C->SetDevFly(true);
			BRTestV417::Place(C, PC, Eye - FVector(0.f, 0.f, C->BaseEyeHeight), FRotator(Under == 0 ? -2.f : 0.f, Yaw, 0.f));
			return true;
		});
	}
	Add(TEXT("v4.17 visuel : fin du vol (Poolrooms)"), 0.5f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (!C || !PC)
		{
			return false;
		}
		C->SetDevFly(false);
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::SpawnRot);
		return true;
	});
	// Contre-jour : regard vers le premier plafonnier allume a 2 cellules ou plus du depart
	AddPose(Scene(TEXT("p_contrejour")), Settle, [](ABRWorld* W, ABRCharacter* C, ABRPlayerController* PC)
	{
		const FIntPoint Start = W->WorldToCell(BRTestV417::SpawnLoc);
		FVector Lamp = FVector::ZeroVector;
		const bool bFound = BRTestV417::ForRings(Start, 6, [&](const FIntPoint& Cell)
		{
			if (FMath::Max(FMath::Abs(Cell.X - Start.X), FMath::Abs(Cell.Y - Start.Y)) < 2 || !W->IsChunkLoaded(W->CellToChunk(Cell)))
			{
				return false;
			}
			const FBRLightInfo L = W->CellLight(Cell.X, Cell.Y);
			if (!L.bHas || L.bBroken)
			{
				return false;
			}
			Lamp = W->CellCenter(Cell) + L.Offset + FVector(0.f, 0.f, W->Def().WallHeight - 10.f);
			return true;
		});
		if (!bFound)
		{
			return false;
		}
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, (Lamp - C->GetEyeLocation()).Rotation());
		return true;
	});
	// Video : marche a la 3e personne dans l'eau du depart (rides du joueur, contact avec les parois)
	Add(TEXT("v4.17 visuel : direction de marche (Poolrooms)"), 1.5f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		ABRPlayerController* PC = GetPC();
		if (!C || !PC)
		{
			return false;
		}
		float Free = 0.f;
		BRTestV417::WalkDir = BRTestV417::FreestDirection(GetWorld(), C, Free);
		BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::WalkDir.Rotation());
		BRTestV417::SetThird(C, true);
		Note(FString::Printf(TEXT("video des Poolrooms : marche vers (%.2f, %.2f), %.0f cm libres"), BRTestV417::WalkDir.X, BRTestV417::WalkDir.Y, Free));
		return true;
	});
	AddVideo(TEXT("p_3p_eau"), VideoFrames, [](ABRCharacter* C)
	{
		BRTestV417::SteerWalk(C);
	});
	Add(TEXT("v4.17 visuel : fin de la video des Poolrooms"), 0.5f, [this]()
	{
		BRTestV417::SetThird(GetPlayer(), false);
		return true;
	});

	// ===================================================================================================== Niveaux 1 et 2
	for (int32 Level : { 1, 2 })
	{
		const FString Tag = FString::Printf(TEXT("l%d"), Level);
		Add(FString::Printf(TEXT("v4.17 visuel : tirages locaux fixes (Niveau %d)"), Level), 0.f, []()
		{
			FMath::RandInit(4170);
			return true;
		});
		AddSeedLoad(Level, 4242u, FString::Printf(TEXT("v4.17 visuel : Niveau %d, graine 4242"), Level));
		Add(FString::Printf(TEXT("v4.17 visuel : Niveau %d : pose de depart"), Level), 0.f, [this]()
		{
			ABRCharacter* C = GetPlayer();
			ABRPlayerController* PC = GetPC();
			if (!C || !PC)
			{
				return false;
			}
			BRTestV417::SpawnLoc = C->GetActorLocation();
			BRTestV417::SpawnRot = FRotator(0.f, PC->GetControlRotation().Yaw, 0.f);
			return true;
		});
		AddPose(Scene(*(Tag + TEXT("_vue"))), Settle, [](ABRWorld*, ABRCharacter* C, ABRPlayerController* PC)
		{
			BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::SpawnRot);
			return true;
		});
		AddPose(Scene(*(Tag + TEXT("_dos"))), Settle, [](ABRWorld*, ABRCharacter* C, ABRPlayerController* PC)
		{
			BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::SpawnRot + FRotator(0.f, 180.f, 0.f));
			return true;
		});
		AddPose(Scene(*(Tag + TEXT("_mur"))), CloseSettle, [](ABRWorld* W, ABRCharacter* C, ABRPlayerController* PC)
		{
			FVector Impact, Face;
			if (!BRTestV417::NearestWall(W->GetWorld(), C, Impact, Face))
			{
				return false;
			}
			const FVector Loc(Impact.X + Face.X * 110.f, Impact.Y + Face.Y * 110.f, BRTestV417::SpawnLoc.Z);
			BRTestV417::Place(C, PC, Loc, FRotator(-8.f, (-Face).Rotation().Yaw + 35.f, 0.f));
			return true;
		});
		if (Level == 2)
		{
			AddPose(Scene(TEXT("l2_plafond")), CloseSettle, [](ABRWorld*, ABRCharacter* C, ABRPlayerController* PC)
			{
				BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::SpawnRot + FRotator(45.f, 0.f, 0.f));
				return true;
			});
		}
		if (Level == 1)
		{
			// Video : marche de 5 s au Niveau 1 (raccords du streaming)
			Add(TEXT("v4.17 visuel : direction de marche (Niveau 1)"), 1.f, [this]()
			{
				ABRCharacter* C = GetPlayer();
				ABRPlayerController* PC = GetPC();
				if (!C || !PC)
				{
					return false;
				}
				float Free = 0.f;
				BRTestV417::WalkDir = BRTestV417::FreestDirection(GetWorld(), C, Free);
				BRTestV417::Place(C, PC, BRTestV417::SpawnLoc, BRTestV417::WalkDir.Rotation());
				Note(FString::Printf(TEXT("video du Niveau 1 : marche vers (%.2f, %.2f), %.0f cm libres"), BRTestV417::WalkDir.X, BRTestV417::WalkDir.Y, Free));
				return true;
			});
			AddVideo(TEXT("l1_marche"), LongVideoFrames, [](ABRCharacter* C)
			{
				BRTestV417::SteerWalk(C);
			});
		}
	}

	Add(TEXT("v4.17 banc visuel : bilan"), 0.f, [this, SetHudMode]()
	{
		FApp::SetUseFixedTimeStep(false);
		if (GEngine && GEngine->bUseFixedFrameRate && !BRTestV417::bPrevFixedRate)
		{
			GEngine->bUseFixedFrameRate = false;
		}
		BRTestV417::KillControl();
		ABRWorld::bTestFreeze = false;
		if (ACameraActor* A = BRTestV417::Cam.Get())
		{
			A->Destroy();
		}
		BRTestV417::Cam.Reset();
		SetHudMode(0);
		if (bSettingsSaved)
		{
			FBRSettings::Get() = SavedSettings;
			bSettingsSaved = false;
			if (ABRPlayerController* PC = GetPC())
			{
				PC->ApplySettings();
			}
		}
		if (!BRTestV417::SavedLanguage.IsEmpty())
		{
			BRLoc::SetLanguage(BRTestV417::SavedLanguage);
		}
		FLevelReport R;
		R.Title = TEXT("v4.17 banc visuel : bilan");
		R.Scene = TEXT("v417");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		Note(FString::Printf(TEXT("banc visuel v4.17 : %d capture(s) de scene en %.0f s (fiches : <capture>.json et Captures.csv)"),
			BRTestV417::Shots, FPlatformTime::Seconds() - BRTestV417::SuiteStart));
		return true;
	});
}
