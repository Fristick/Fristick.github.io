// v4.16 (lidar) : test du jeu des releves du scanner sur un battant mobile (prompt v4.16, section 4).
//   -BRAutoTest -BRAutoTestV416Lidar [-BRAutoTestStrict]   : Level 6 (graine 4136), porte du passage (le bon battant) :
//      scan porte fermee, ouverture PENDANT le scan, nouveau scan juste apres (avant le rafraichissement des inscriptions),
//      ouverture vide (pas de faux mur de points), expiration des anciens points, nouveaux scans sous trois angles,
//      rechargement du chunk de la porte, reprise de la partie, autre plan (graine 4126) ; points bornes et locaux.
//   -BRNetTest -BRAutoTestV416Lidar (hote et client) : la porte ouverte par l'hote, chaque machine scanne la sienne : ses
//      points sont locaux (un seul scanner par machine, aucun chez le coequipier) et suivent le battant.
// Positions et distances relevees dans le rapport (cm). Une condition absente (porte de secours, chunk non construit) est
// NON VERIFIEE, jamais une reussite.
#include "BRAutoTest.h"
#include "BRAutoTestNet.h"
#include "BRWorld.h"
#include "BRCharacter.h"
#include "BRPlayerController.h"
#include "BRMission.h"
#include "BRMissionLogic.h"
#include "BRLidar.h"
#include "BRLidarLogic.h"
#include "BRSave.h"
#include "Backrooms.h"

#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/PlatformTime.h"

namespace BRM = BRMission;

namespace BRTestV416Lidar
{
	/** Graines du test (-BRSeed equivalentes) : aile valide avec ses quatre portes (journal final4, v4.15) */
	constexpr uint32 SeedA = 4136;
	constexpr uint32 SeedB = 4126;
	/** Barrieres du test reseau (client -> hote) */
	constexpr int32 MarkClosed = 416601;
	constexpr int32 MarkDone = 416602;
	/** Duree d'un scan focalise (s) */
	constexpr float ScanSeconds = 1.5f;

	const TCHAR* Oui(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	BRLidar::FFrame3 FrameOf(const FTransform& T)
	{
		BRLidar::FFrame3 F;
		const FVector O = T.GetLocation();
		const FVector AX = T.GetUnitAxis(EAxis::X);
		const FVector AY = T.GetUnitAxis(EAxis::Y);
		const FVector AZ = T.GetUnitAxis(EAxis::Z);
		for (int32 I = 0; I < 3; ++I)
		{
			F.O[I] = static_cast<float>(O[I]);
			F.X[I] = static_cast<float>(AX[I]);
			F.Y[I] = static_cast<float>(AY[I]);
			F.Z[I] = static_cast<float>(AZ[I]);
		}
		return F;
	}

	/** Distance (cm) d'un point du monde aux boites du battant (BRLidar::DoorScanBox) dans le repere LF */
	float LeafDistance(const BRLidar::FFrame3& LF, const FVector& P)
	{
		const float P3[3] = { static_cast<float>(P.X), static_cast<float>(P.Y), static_cast<float>(P.Z) };
		float Best = 1e9f;
		for (int32 B = 0; B < BRLidar::DoorScanBoxes; ++B)
		{
			float Lo[3], Hi[3];
			BRLidar::DoorScanBox(B, Lo, Hi);
			Best = FMath::Min(Best, BRLidar::BoxDistance(LF, Lo, Hi, P3));
		}
		return Best;
	}

	/** Plaque (centre, droite, haut, normale) a un instant */
	struct FPlate
	{
		FVector C = FVector(0.0, 0.0, 0.0);
		FVector R = FVector(0.0, 1.0, 0.0);
		FVector U = FVector(0.0, 0.0, 1.0);
		FVector N = FVector(1.0, 0.0, 0.0);
		BRLidar::FFrame3 Leaf;
		bool bLeaf = false;
		float Progress = 0.f;
	};

	FPlate PlateOf(const ABRMissionDevice* D)
	{
		FPlate P;
		D->GetLidarPlate(P.C, P.R, P.U, P.N);
		FTransform LT;
		P.bLeaf = D->GetLidarLeaf(LT);
		if (P.bLeaf)
		{
			P.Leaf = FrameOf(LT);
		}
		P.Progress = D->GetModuleProgress();
		return P;
	}

	/** Point d'une inscription sur la plaque P (ecart au plan au plus PlaneTol, dans le cadre de la plaque) ; ecart au plan */
	bool OnPlate(const FVector& At, const FPlate& P, float PlaneTol, float& OutPlane)
	{
		const FVector D = At - P.C;
		OutPlane = static_cast<float>(FMath::Abs(FVector::DotProduct(D, P.N)));
		return OutPlane <= PlaneTol && FMath::Abs(FVector::DotProduct(D, P.R)) <= 32.0 && FMath::Abs(FVector::DotProduct(D, P.U)) <= 32.0;
	}

	/** Point (monde) dans l'ouverture du mur de la porte, hors de ses bords (repere du mecanisme : X vers la galerie) */
	bool InDoorway(const ABRMissionDevice* D, const FVector& At)
	{
		const FVector L = D->GetActorTransform().InverseTransformPosition(At);
		return L.X > -27.0 && L.X < 27.0 && FMath::Abs(L.Y) <= 62.0 && L.Z >= 8.0 && L.Z <= 215.0;
	}

	/** Etat du test (une instance, creee au premier usage) */
	struct FRun
	{
		int32 Door = -1;
		bool bReady = false;
		int32 Phase = 0;
		double Clock = 0.0;
		/** Plaque relevee porte fermee (ancienne place) */
		FPlate Old;
		bool bOld = false;
		double ClosedPassAt = -1.0;
		/** Plaque a chaque image du mouvement (numero d'image) */
		TMap<uint64, FPlate> Samples;
		uint64 LoadKey = 0;
		int32 Serial = -1;
		int32 Count = 0;
		int32 Shots = 0;
		BRTestNet::FPlacement Place;
	};
	FRun& Run()
	{
		static FRun R;
		return R;
	}

	/** Mesures d'une fenetre de scan */
	struct FScan
	{
		int32 Total = 0;
		int32 Glyphs = 0;
		int32 GlyphsOnPlate = 0;
		int32 GlyphsAtOld = 0;
		float GlyphPlaneMax = 0.f;
		float MinFromOld = 1e9f;
		float MaxFromOld = 0.f;
		int32 DoorImpacts = 0;
		int32 DoorOnLeaf = 0;
		float LeafDistMax = 0.f;
		int32 Ignored = 0;
		int32 InDoorway = 0;
		int32 Beyond = 0;
		/** Inscriptions posees pendant que le battant bougeait (images du mouvement) */
		int32 GlyphsMoving = 0;
	};

	/** Releve de l'image F ou de ses voisines (l'ordre des Ticks du test, du mecanisme et du scanner n'est pas fixe) qui
	 *  explique le mieux le point : une inscription, par sa plaque (ecart au plan) ; un impact, par les boites du battant.
	 *  bOutMoving : le battant bougeait autour de cette image. False si aucune image relevee */
	bool BestSample(const FRun& S, uint64 Frame, const FVector& At, bool bGlyph, FPlate& Out, bool& bOutMoving)
	{
		bool bAny = false;
		float Best = 1e9f;
		bOutMoving = false;
		const FPlate* Prev = S.Samples.Find(Frame - 2);
		for (uint64 F = Frame - 1; F <= Frame + 1; ++F)
		{
			const FPlate* P = S.Samples.Find(F);
			if (!P)
			{
				continue;
			}
			if (Prev && !FMath::IsNearlyEqual(Prev->Progress, P->Progress))
			{
				bOutMoving = true;
			}
			Prev = P;
			float Score = 0.f;
			if (bGlyph)
			{
				float Plane = 0.f;
				Score = OnPlate(At, *P, 1e9f, Plane) ? Plane : 1e6f + static_cast<float>(FVector::Dist(At, P->C));
			}
			else
			{
				Score = P->bLeaf ? LeafDistance(P->Leaf, At) : 1e6f;
			}
			if (Score < Best)
			{
				Best = Score;
				Out = *P;
				bAny = true;
			}
		}
		return bAny;
	}

	/** Releve du scanner L sur la porte D : inscriptions sur la plaque (actuelle, ou de l'image si bSamples), ancienne place,
	 *  impacts sur le battant, composants d'interaction touches, points dans l'ouverture vide, points au-dela */
	FScan Analyze(const ABRLidar* L, const ABRMissionDevice* D, const FRun& S, bool bSamples)
	{
		FScan R;
		TArray<const UPrimitiveComponent*> Ignored;
		D->GetLidarIgnored(Ignored);
		const FPlate Now = PlateOf(D);
		for (const ABRLidar::FTestPoint& T : L->GetTestPoints())
		{
			++R.Total;
			const UPrimitiveComponent* Comp = T.Component.Get();
			if (Comp && Ignored.Contains(Comp))
			{
				++R.Ignored;
			}
			FPlate P = Now;
			bool bMoving = false;
			const bool bGlyph = T.Category == BRLidar::Glyph && T.Device == S.Door;
			if (bSamples)
			{
				BestSample(S, T.Frame, T.At, bGlyph, P, bMoving);
			}
			if (bGlyph)
			{
				++R.Glyphs;
				R.GlyphsMoving += bMoving ? 1 : 0;
				float Plane = 0.f;
				if (OnPlate(T.At, P, 1.5f, Plane))
				{
					++R.GlyphsOnPlate;
				}
				R.GlyphPlaneMax = FMath::Max(R.GlyphPlaneMax, Plane);
				if (S.bOld)
				{
					float OldPlane = 0.f;
					// Ancienne place : seulement une fois le battant parti de sa pose fermee
					if (P.Progress > 0.05f && OnPlate(T.At, S.Old, 3.f, OldPlane))
					{
						++R.GlyphsAtOld;
					}
					const float Dc = static_cast<float>(FVector::Dist(T.At, S.Old.C));
					R.MinFromOld = FMath::Min(R.MinFromOld, Dc);
					R.MaxFromOld = FMath::Max(R.MaxFromOld, Dc);
				}
				continue;
			}
			if (T.Category == BRLidar::Device && T.Device == S.Door)
			{
				++R.DoorImpacts;
				if (P.bLeaf)
				{
					const float Ld = LeafDistance(P.Leaf, T.At);
					R.LeafDistMax = FMath::Max(R.LeafDistMax, Ld);
					R.DoorOnLeaf += (T.bLeaf && Ld <= 1.5f) ? 1 : 0;
				}
			}
			// Ouverture vide (porte partie de sa pose fermee) : rien sauf le battant lui-meme (une entite qui passe dans
			// l'ouverture y est vraiment : ses croix ne sont pas un faux mur)
			if (T.Category != BRLidar::Entity && P.Progress > 0.05f && InDoorway(D, T.At) && (!P.bLeaf || LeafDistance(P.Leaf, T.At) > 4.f))
			{
				++R.InDoorway;
			}
			if (D->GetActorTransform().InverseTransformPosition(T.At).X < -35.0)
			{
				++R.Beyond;
			}
		}
		return R;
	}

	/** Place debout (ou en vol, sans chute pendant la construction d'un chunk) a Local (repere du mecanisme, X vers la
	 *  galerie ; Z : hauteur du centre de la capsule au-dessus du sol), regard vers Target */
	void PlaceNear(ABRCharacter* C, const ABRMissionDevice* D, const FVector& Local, const FVector& Target, bool bHover)
	{
		const float HH = C->GetCapsuleComponent() ? C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
		const FVector Stand = D->GetActorTransform().TransformPosition(FVector(Local.X, Local.Y, HH + 2.f));
		C->SetActorLocation(Stand, false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* M = C->GetCharacterMovement())
		{
			M->StopMovementImmediately();
			M->SetMovementMode(bHover ? MOVE_Flying : MOVE_Walking);
		}
		if (AController* Ctrl = C->GetController())
		{
			Ctrl->SetControlRotation((Target - C->GetEyeLocation()).Rotation());
		}
	}

	void Face(ABRCharacter* C, const FVector& Target)
	{
		if (AController* Ctrl = C ? C->GetController() : nullptr)
		{
			Ctrl->SetControlRotation((Target - C->GetEyeLocation()).Rotation());
		}
	}

	/** Bonne porte du passage (celle qui s'ouvre) si c'est un vrai battant (module) ; -1 sinon */
	int32 RightDoor(const ABRWorld* W)
	{
		if (!W || !W->IsMissionActive())
		{
			return -1;
		}
		const BRM::FPlan& P = W->GetMissionPlan();
		const int32 Idx = P.Params[0];
		const ABRMissionDevice* D = (Idx >= 0 && Idx < P.NumDevices && P.Devices[Idx].Role == BRM::R_LidarDoor) ? W->FindMissionDevice(Idx) : nullptr;
		return (D && D->GetModule() == BRMech::Module::LidarDoor) ? Idx : -1;
	}

	FBRSessionState MakeSession(uint32 LevelSeed, const BRM::FPlan& P, const BRM::FState& St)
	{
		uint8 Buffer[11 + BRM::MaxDevices];
		const int32 Size = BRM::Serialize(P, St, Buffer, sizeof(Buffer));
		FBRSessionState Session;
		Session.bValid = Size > 0;
		Session.Level = 6;
		Session.Seed = LevelSeed;
		Session.GenVersion = BRM::GenVersion;
		Session.PlaceVersion = BRM::PlaceVersion;
		Session.Mission = TArray<uint8>(Buffer, Size);
		return Session;
	}

	/** Scanners presents dans ce monde (un seul attendu : celui du joueur local) */
	int32 CountScanners(UWorld* World, const ABRCharacter* Local, int32& OutOwned)
	{
		int32 N = 0;
		OutOwned = 0;
		for (TActorIterator<ABRLidar> It(World); It; ++It)
		{
			++N;
			OutOwned += It->GetOwner() == Local ? 1 : 0;
		}
		return N;
	}

	/** Lieux du scan, repere du mecanisme (X vers la galerie, Y le long du mur ; charnieres a -Y), par ordre de preference :
	 *  galerie (la plaque ouverte, dans l'aile contre le cote des charnieres, se voit a travers l'ouverture depuis +Y), aile en
	 *  face de la plaque ouverte, aile de biais */
	TArray<FVector> StandsGallery()
	{
		return { FVector(200.0, 40.0, 0.0), FVector(160.0, 10.0, 0.0), FVector(240.0, 90.0, 0.0), FVector(140.0, 60.0, 0.0), FVector(260.0, 20.0, 0.0) };
	}
	TArray<FVector> StandsWing()
	{
		return { FVector(-75.0, 115.0, 0.0), FVector(-90.0, 150.0, 0.0), FVector(-70.0, 85.0, 0.0) };
	}
	TArray<FVector> StandsWingSide()
	{
		return { FVector(-190.0, 70.0, 0.0), FVector(-160.0, 40.0, 0.0), FVector(-220.0, 110.0, 0.0) };
	}

	/** Premiere place (repere du mecanisme) ou la capsule du joueur tient et d'ou Target se voit (sans compter la porte : ses
	 *  collisions d'interaction sont ignorees comme par le scanner) ; false si aucune */
	bool PickStand(ABRCharacter* C, const ABRMissionDevice* D, const TArray<FVector>& Cands, const FVector& Target, FVector& Out)
	{
		UWorld* World = C ? C->GetWorld() : nullptr;
		if (!World || Cands.Num() == 0)
		{
			return false;
		}
		const UCapsuleComponent* Cap = C->GetCapsuleComponent();
		const float HH = Cap ? Cap->GetScaledCapsuleHalfHeight() : 90.f;
		const float Rad = Cap ? Cap->GetScaledCapsuleRadius() : 34.f;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestV416Stand), false, C);
		TArray<const UPrimitiveComponent*> Ignored;
		D->GetLidarIgnored(Ignored);
		for (const UPrimitiveComponent* P : Ignored)
		{
			if (P)
			{
				Q.AddIgnoredComponent(P);
			}
		}
		for (const FVector& L : Cands)
		{
			const FVector Stand = D->GetActorTransform().TransformPosition(FVector(L.X, L.Y, HH + 2.f));
			const bool bFree = !World->OverlapAnyTestByChannel(Stand, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Rad, HH), Q);
			const FVector Eye = Stand + FVector(0.0, 0.0, HH * 0.75f);
			FHitResult Hit;
			const bool bSeen = !World->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Q) || FVector::Dist(Hit.ImpactPoint, Target) < 3.0;
			if (bFree && bSeen)
			{
				Out = L;
				return true;
			}
		}
		return false;
	}
}

// =====================================================================================================================
// Plan (porte d'entree : ABRAutoTest::BeginPlay, -BRAutoTestV416Lidar)
// =====================================================================================================================

void ABRAutoTest::AddV416LidarPlan()
{
	if (bNetTest)
	{
		const bool bClient = GetNetMode() == NM_Client;
		FLevelReport R;
		R.Title = bClient ? TEXT("v4.16 scanner, battant mobile (client)") : TEXT("v4.16 scanner, battant mobile (hote)");
		Reports.Add(R);
		AddNetV416LidarSteps(bClient);
		UE_LOG(LogBackrooms, Display, TEXT("[AutoTest] Test multijoueur (%s) : %d etapes. Rapport : %s"), bClient ? TEXT("client") : TEXT("hote"), Plan.Num(), *OutDir);
		return;
	}
	AddV416LidarSteps();
	Add(TEXT("Fin"), 0.f, [this]()
	{
		Finish();
		return true;
	});
	UE_LOG(LogBackrooms, Display, TEXT("[AutoTest] Verifications v4.16 (scanner, battant mobile) : %d etapes. Captures et rapport : %s"), Plan.Num(), *OutDir);
}

void ABRAutoTest::AddV416LidarSteps()
{
	using namespace BRTestV416Lidar;
	const uint32 LevelSeed = ABRWorld::SeedFromUser(SeedA, 6);

	// Une fenetre de scan focalise depuis Stand (repere de la porte), visee sur la plaque actuelle (ou le milieu de
	// l'ouverture), puis l'analyse. Phases 160 a 162
	enum class EExpect : uint8 { Closed, Open, Doorway };
	auto AddScan = [this](const FString& Name, const TArray<FVector>& Stands, bool bHover, EExpect Expect, bool bPulse, bool bNeedGlyphs)
	{
		Add(Name, 0.f, [this, Name, Stands, bHover, Expect, bPulse, bNeedGlyphs]()
		{
			FRun& S = Run();
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			ABRLidar* L = C ? C->GetLidar() : nullptr;
			ABRMissionDevice* D = (W && S.Door >= 0) ? W->FindMissionDevice(S.Door) : nullptr;
			if (!S.bReady || !W || !C || !D || W->GetLevelNumber() != 6)
			{
				Skip(FString::Printf(TEXT("%s : porte du passage absente (voir plus haut)"), *Name));
				S.Phase = 0;
				return true;
			}
			if (!L)
			{
				if (StepTime < 5.f)
				{
					return false;
				}
				Note(FString::Printf(TEXT("%s : scanner absent au Level 6"), *Name), true);
				S.Phase = 0;
				return true;
			}
			const double Now = FPlatformTime::Seconds();
			const FVector Target = Expect == EExpect::Doorway ? D->GetActorTransform().TransformPosition(FVector(0.0, 0.0, 120.0)) : PlateOf(D).C;
			if (S.Phase < 160 || S.Phase > 162)
			{
				// Chunk de la porte construit (le joueur peut en etre loin), puis une place libre et a vue de la cible
				L->SetFocus(false);
				if (!D->IsShown())
				{
					PlaceNear(C, D, Stands[0], Target, true);
					if (StepTime > 30.f)
					{
						Skip(FString::Printf(TEXT("%s : chunk de la porte non construit apres 30 s"), *Name));
						return true;
					}
					return false;
				}
				FVector Stand = Stands[0];
				if (!PickStand(C, D, Stands, Target, Stand))
				{
					if (StepTime < 2.f)
					{
						return false; // collisions du decor voisin pas encore construites
					}
					Skip(FString::Printf(TEXT("%s : aucune des %d places prevues n'est libre et a vue de la cible"), *Name, Stands.Num()));
					return true;
				}
				PlaceNear(C, D, Stand, Target, bHover);
				S.Clock = Now;
				S.Phase = 160;
				return false;
			}
			Face(C, Target);
			C->Sanity = 100.f;
			if (S.Phase == 160)
			{
				// Porte attendue ouverte : son battant a fini de pivoter (une reprise peut l'ouvrir a l'arrivee)
				const bool bSettled = Expect == EExpect::Closed || D->GetModuleProgress() >= 1.f;
				if (!D->IsShown() || !bSettled || Now - S.Clock < 0.4)
				{
					if (Now - S.Clock > 30.0)
					{
						Skip(FString::Printf(TEXT("%s : chunk de la porte non construit ou battant pas arrete apres 30 s"), *Name));
						S.Phase = 0;
						return true;
					}
					return false;
				}
				L->SetTestRecord(true);
				L->SetFocus(true);
				if (bPulse)
				{
					L->Pulse();
				}
				S.Clock = Now;
				S.Phase = 161;
				return false;
			}
			if (!L->IsFocusing())
			{
				L->SetFocus(true);
			}
			if (Now - S.Clock < ScanSeconds)
			{
				return false;
			}
			L->SetFocus(false);
			S.Phase = 0;
			const FScan R = Analyze(L, D, S, false);
			const FPlate P = PlateOf(D);
			const FVector Eye = C->GetEyeLocation();
			const FString Where = FString::Printf(TEXT("yeux a %.0f cm de la plaque, battant ouvert a %.0f %%"), FVector::Dist(Eye, P.C), P.Progress * 100.f);
			if (Expect == EExpect::Doorway)
			{
				const bool bOk = R.InDoorway == 0 && R.Ignored == 0 && R.Beyond > 0;
				Note(FString::Printf(TEXT("%s : %d points ; dans l'ouverture vide (hors battant) %d (attendu 0 ; v4.15 : boite visee restee dans l'ouverture), sur la boite visee ou le "
					"corps bloquant %d (attendu 0), au-dela de l'ouverture %d (les rayons passent) ; %s"), *Name, R.Total, R.InDoorway, R.Ignored, R.Beyond, *Where), !bOk);
				Shot(FString::Printf(TEXT("v416_lidar_%02d_ouverture_vide"), ++S.Shots));
				return true;
			}
			// Une passe par cible et par periode (v4.14) : juste apres une autre passe a la meme pose, rien n'est redessine (bNeedGlyphs
			// faux) ; ce qui est dessine doit etre sur la plaque actuelle
			const int32 Need = BRLidar::PlateGlyphPoints(D->GetDeviceDef().Label);
			const bool bGlyphs = (!bNeedGlyphs || R.Glyphs >= Need) && R.GlyphsOnPlate == R.Glyphs;
			const bool bOld = Expect == EExpect::Closed || R.GlyphsAtOld == 0;
			const bool bImpacts = R.DoorImpacts > 0 && R.DoorOnLeaf == R.DoorImpacts && R.Ignored == 0;
			const FString Old = (S.bOld && Expect != EExpect::Closed)
				? FString::Printf(TEXT(", a l'ancienne place (plaque fermee) %d (attendu 0), distance a l'ancien centre %.1f a %.1f cm"), R.GlyphsAtOld, R.MinFromOld, R.MaxFromOld)
				: FString();
			Note(FString::Printf(TEXT("%s : inscriptions %d (une passe : %d%s), sur la plaque actuelle %d (ecart au plan %.2f cm au plus)%s ; impacts sur le battant %d, sur ses "
				"boites a 1,5 cm %d (au plus %.2f cm), sur la boite visee ou le corps bloquant %d (attendu 0) ; %s"), *Name, R.Glyphs, Need,
				bNeedGlyphs ? TEXT(" attendue") : TEXT(", deja dessinee a cette pose : pas de nouvelle passe avant le rafraichissement"), R.GlyphsOnPlate, R.GlyphPlaneMax, *Old,
				R.DoorImpacts, R.DoorOnLeaf, R.LeafDistMax, R.Ignored, *Where), !bGlyphs || !bOld || !bImpacts);
			Shot(FString::Printf(TEXT("v416_lidar_%02d_%s"), ++S.Shots, Expect == EExpect::Closed ? TEXT("ferme") : TEXT("ouvert")));
			return true;
		});
		LimitLastStep(40.f);
	};

	AddSeedLoad(6, SeedA, FString::Printf(TEXT("v4.16 Level 6 LiDAR, battant mobile (graine %u)"), SeedA));
	Add(TEXT("v4.16 lidar : porte du passage"), 0.f, [this]()
	{
		FRun& S = Run();
		S = FRun();
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || !W->IsLevelReady() || W->GetLevelNumber() != 6 || (!W->IsMissionActive() && StepTime < 10.f) || (!C->GetLidar() && StepTime < 5.f))
		{
			if (StepTime > 30.f)
			{
				Note(TEXT("v4.16 lidar : Level 6 non pret apres 30 s"), true);
				return true;
			}
			return false;
		}
		C->bGodMode = true;
		S.Door = RightDoor(W);
		ABRMissionDevice* D = S.Door >= 0 ? W->FindMissionDevice(S.Door) : nullptr;
		if (!D)
		{
			Skip(TEXT("v4.16 lidar : la bonne porte du passage n'est pas un battant (placement de secours) : battant mobile non verifie sur cette graine"));
			return true;
		}
		S.bReady = true;
		S.LoadKey = C->GetLidar() ? C->GetLidar()->GetLoadKey() : 0;
		ABRLidar::ResetStats();
		const FVector A = D->GetActorLocation();
		int32 Rank = 0;
		for (int32 I = 0; I <= S.Door; ++I)
		{
			Rank += W->GetMissionPlan().Devices[I].Role == BRM::R_LidarDoor ? 1 : 0;
		}
		Note(FString::Printf(TEXT("v4.16 lidar : bonne porte du passage = mecanisme %d (porte %d sur 4), battant en (%.0f, %.0f, %.0f), lacet %.0f, ferme (ouverture %.0f %%) ; "
			"scanner local %s, persistance %.0f s"), S.Door, Rank, A.X, A.Y, A.Z, D->GetActorRotation().Yaw, D->GetModuleProgress() * 100.f, Oui(C->GetLidar() != nullptr),
			ABRLidar::Settings().Persistence));
		return true;
	});
	// 1. Porte fermee : inscription sur la plaque fermee, impacts sur le battant (pas sur la boite visee ni le corps bloquant)
	AddScan(TEXT("v4.16 lidar : scan porte fermee (galerie)"), StandsGallery(), false, EExpect::Closed, false, true);
	Add(TEXT("v4.16 lidar : ancienne place relevee"), 0.f, [this]()
	{
		FRun& S = Run();
		ABRWorld* W = GetBRWorld();
		ABRMissionDevice* D = (W && S.bReady) ? W->FindMissionDevice(S.Door) : nullptr;
		ABRCharacter* C = GetPlayer();
		if (D && C && C->GetLidar())
		{
			S.Old = PlateOf(D);
			S.bOld = true;
			S.ClosedPassAt = FPlatformTime::Seconds();
			const ABRLidar* L = C->GetLidar();
			int32 Rank = 0;
			for (int32 I = 0; I <= S.Door; ++I)
			{
				Rank += W->GetMissionPlan().Devices[I].Role == BRM::R_LidarDoor ? 1 : 0;
			}
			Note(FString::Printf(TEXT("plaque fermee en (%.1f, %.1f, %.1f), normale (%.2f, %.2f, %.2f) ; releve de la porte %d au carnet %s"), S.Old.C.X, S.Old.C.Y, S.Old.C.Z,
				S.Old.N.X, S.Old.N.Y, S.Old.N.Z, Rank, Oui(L->GetScannedPlates().Contains(Rank))), !L->GetScannedPlates().Contains(Rank));
		}
		return true;
	});
	// 2. Ouverture PENDANT le scan focalise (la plaque est visee a sa position de chaque image) : les inscriptions et les
	// impacts suivent le battant ; aucune inscription a l'ancienne place. Phases 163 a 165
	Add(TEXT("v4.16 lidar : ouverture pendant le scan"), 0.f, [this]()
	{
		FRun& S = Run();
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		ABRMissionDevice* D = (W && S.bReady) ? W->FindMissionDevice(S.Door) : nullptr;
		if (!W || !C || !L || !D || !S.bOld)
		{
			Skip(TEXT("v4.16 lidar : ouverture pendant le scan non verifiee (porte, scanner ou scan ferme absent)"));
			S.Phase = 0;
			return true;
		}
		const double Now = FPlatformTime::Seconds();
		const FPlate P = PlateOf(D);
		S.Samples.Add(GFrameCounter, P);
		Face(C, P.C);
		if (S.Phase < 163 || S.Phase > 165)
		{
			S.Samples.Reset();
			S.Samples.Add(GFrameCounter, P);
			L->SetTestRecord(true);
			L->SetFocus(true);
			W->DebugCompleteMission(); // l'hote resout la mission : la bonne porte s'ouvre (Gate 255, animation de UpdateModule)
			S.Clock = Now;
			S.Phase = 163;
			return false;
		}
		if (!L->IsFocusing())
		{
			L->SetFocus(true);
		}
		// Jusqu'a l'arret du battant : le scan suivant (etape 3) est le premier apres l'arret
		if (P.Progress < 1.f && Now - S.Clock < 8.0)
		{
			return false;
		}
		S.Count = static_cast<int32>((Now - S.Clock) * 100.0);
		L->SetFocus(false);
		S.Phase = 0;
		const FScan R = Analyze(L, D, S, true);
		const bool bOpen = P.Progress >= 1.f && D->GetGateValue() >= 255;
		const bool bOk = bOpen && R.GlyphsOnPlate == R.Glyphs && R.GlyphsAtOld == 0 && R.DoorOnLeaf == R.DoorImpacts && R.Ignored == 0 && R.InDoorway == 0;
		Note(FString::Printf(TEXT("ouverture pendant le scan (%.2f s jusqu'a 95 degres) : porte ouverte %s ; inscriptions %d dont %d pendant le mouvement, sur la plaque de leur image %d, "
			"a l'ancienne place %d (attendu 0) ; impacts sur le battant %d, sur ses boites a sa position %d (au plus %.2f cm), sur la boite visee ou le corps bloquant %d, dans "
			"l'ouverture vide %d (attendu 0) ; %d images relevees"), S.Count / 100.f, Oui(bOpen), R.Glyphs, R.GlyphsMoving, R.GlyphsOnPlate, R.GlyphsAtOld, R.DoorImpacts,
			R.DoorOnLeaf, R.LeafDistMax, R.Ignored, R.InDoorway, S.Samples.Num()), !bOk);
		const FVector Moved = P.C - S.Old.C;
		Note(FString::Printf(TEXT("plaque ouverte en (%.1f, %.1f, %.1f) : centre deplace de %.1f cm (prevu par le code : 100,5 cm), normale (%.2f, %.2f, %.2f)"), P.C.X, P.C.Y, P.C.Z,
			Moved.Size(), P.N.X, P.N.Y, P.N.Z), !bOpen || FMath::Abs(Moved.Size() - 100.5) > 2.0);
		S.Samples.Reset();
		return true;
	});
	LimitLastStep(30.f);
	// 3. Nouveau scan juste apres l'ouverture (avant le rafraichissement des inscriptions, 0,6 x persistance) : la plaque est
	// redessinee a sa position actuelle, les anciens points (memoire du releve) restent jusqu'a leur expiration
	Add(TEXT("v4.16 lidar : avant le nouveau scan"), 0.f, [this]()
	{
		FRun& S = Run();
		const double Since = S.ClosedPassAt > 0.0 ? FPlatformTime::Seconds() - S.ClosedPassAt : -1.0;
		const float Refresh = BRLidar::GlyphRefreshSeconds(ABRLidar::Settings().Persistence);
		S.Count = Since >= 0.0 && Since + ScanSeconds + 1.0 < Refresh ? 1 : 0;
		if (!S.Count && S.bReady)
		{
			Skip(FString::Printf(TEXT("nouveau scan %.1f s apres la passe fermee, apres le rafraichissement (%.1f s) : le cas \"avant le rafraichissement\" n'est pas isole"), Since,
				Refresh));
		}
		return true;
	});
	AddScan(TEXT("v4.16 lidar : nouveau scan juste apres l'ouverture (galerie)"), StandsGallery(), false, EExpect::Open, false, true);
	Add(TEXT("v4.16 lidar : passe recommencee"), 0.f, [this]()
	{
		FRun& S = Run();
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		if (!S.bReady || !L)
		{
			return true;
		}
		uint32 Rev = 0;
		bool bMoving = false;
		const bool bTrack = L->GetGlyphTrack(S.Door, Rev, bMoving);
		Note(FString::Printf(TEXT("support de la plaque : revision de pose %u, en mouvement %s ; passes recommencees (support deplace) %lld, rayons sur une inscription en mouvement %lld, "
			"rayons arretes par un battant %lld ; %d points encore affiches (memoire du releve)"), Rev, Oui(bMoving), ABRLidar::Stats.GlyphPoseRestarts, ABRLidar::Stats.GlyphMovingSkips,
			ABRLidar::Stats.LeafHits, L->GetPointCount()), !bTrack || Rev == 0 || bMoving || (S.Count == 1 && ABRLidar::Stats.GlyphPoseRestarts < 1));
		return true;
	});
	// 4. Ouverture vide : visee au milieu de l'ouverture, impulsion et scan focalise : aucun faux mur
	AddScan(TEXT("v4.16 lidar : ouverture vide (galerie, impulsion et focus)"), StandsGallery(), false, EExpect::Doorway, true, false);
	// 5. Expiration des anciens points (persistance), puis nouveaux scans sous plusieurs angles
	auto AddExpiry = [this](const FString& Name)
	{
		Add(Name, 0.f, [this, Name]()
		{
			FRun& S = Run();
			ABRCharacter* C = GetPlayer();
			ABRLidar* L = C ? C->GetLidar() : nullptr;
			if (!S.bReady || !L)
			{
				return true;
			}
			L->SetFocus(false);
			const float Persist = ABRLidar::Settings().Persistence;
			if (L->GetPointCount() > 0 && StepTime < Persist + 3.f)
			{
				return false;
			}
			Note(FString::Printf(TEXT("%s : anciens points expires apres %.1f s (persistance %.0f s) : %d restent"), *Name, StepTime, Persist, L->GetPointCount()),
				L->GetPointCount() > 0);
			return true;
		});
		LimitLastStep(45.f);
	};
	AddExpiry(TEXT("v4.16 lidar : expiration des anciens points"));
	AddScan(TEXT("v4.16 lidar : rescan, galerie a travers l'ouverture"), StandsGallery(), false, EExpect::Open, false, true);
	AddScan(TEXT("v4.16 lidar : rescan, dans l'aile face a la plaque"), StandsWing(), true, EExpect::Open, false, false);
	AddScan(TEXT("v4.16 lidar : rescan, dans l'aile de biais"), StandsWingSide(), true, EExpect::Open, false, false);
	// 6. Rechargement du chunk de la porte (le joueur s'eloigne puis revient) : la porte reparait ouverte, sa plaque est
	// relevee a sa position actuelle. Phases 166 a 168
	Add(TEXT("v4.16 lidar : rechargement du chunk de la porte"), 0.f, [this]()
	{
		FRun& S = Run();
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRMissionDevice* D = (W && S.bReady) ? W->FindMissionDevice(S.Door) : nullptr;
		if (!W || !C || !D)
		{
			Skip(TEXT("v4.16 lidar : rechargement du chunk non verifie (porte absente)"));
			S.Phase = 0;
			return true;
		}
		const double Now = FPlatformTime::Seconds();
		if (S.Phase < 166 || S.Phase > 168)
		{
			// Loin cote galerie (vers le depart, au-dela) : ViewDistance + 1,6 chunk au moins
			const FVector Far = D->GetActorLocation() + D->GetActorForwardVector() * 13500.0 + FVector(0.0, 0.0, 300.0);
			C->SetActorLocation(Far, false, nullptr, ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* M = C->GetCharacterMovement())
			{
				M->StopMovementImmediately();
				M->SetMovementMode(MOVE_Flying);
			}
			S.Clock = Now;
			S.Phase = 166;
			return false;
		}
		if (S.Phase == 166)
		{
			if (D->IsShown())
			{
				if (Now - S.Clock > 40.0)
				{
					Skip(TEXT("v4.16 lidar : le chunk de la porte n'a pas ete decharge en 40 s : rechargement non verifie"));
					S.Phase = 0;
					return true;
				}
				return false;
			}
			FTransform Leaf;
			S.Count = D->GetLidarLeaf(Leaf) ? 1 : 0;
			S.Phase = 167;
			S.Clock = Now;
			return false;
		}
		Note(FString::Printf(TEXT("chunk de la porte decharge (mecanisme masque, battant hors du scanner %s)"), Oui(S.Count == 0)), S.Count != 0);
		S.Phase = 0;
		return true;
	});
	LimitLastStep(50.f);
	AddExpiry(TEXT("v4.16 lidar : expiration avant le retour"));
	AddScan(TEXT("v4.16 lidar : apres le rechargement du chunk (galerie)"), StandsGallery(), false, EExpect::Open, false, true);
	// 7. Reprise de la partie (session sauvegardee, mission resolue) : nouveau chargement, releves effaces, porte ouverte des
	// l'arrivee, plaque relevee a sa position actuelle
	Add(TEXT("v4.16 lidar : reprise de la partie"), 0.f, [this, LevelSeed]()
	{
		FRun& S = Run();
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!S.bReady || !W || !C)
		{
			Skip(TEXT("v4.16 lidar : reprise non verifiee (porte absente)"));
			return true;
		}
		if (W->IsTransitioning())
		{
			return false;
		}
		S.LoadKey = C->GetLidar() ? C->GetLidar()->GetLoadKey() : 0;
		S.Serial = W->GetLevelSerial();
		BRSaves::PendingResume() = MakeSession(LevelSeed, W->GetMissionPlan(), W->GetMissionState());
		W->RequestTransition(6, false, LevelSeed);
		return true;
	});
	Add(TEXT("v4.16 lidar : reprise de la partie, verification"), 1.f, [this]()
	{
		FRun& S = Run();
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!S.bReady)
		{
			return true;
		}
		if (!W || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 6 || W->GetLevelSerial() == S.Serial || !C->GetLidar())
		{
			if (StepTime > 40.f)
			{
				Note(TEXT("reprise du Level 6 : pas de nouveau chargement apres 40 s"), true);
				S.bReady = false;
				return true;
			}
			return false;
		}
		C->bGodMode = true;
		const ABRLidar* L = C->GetLidar();
		S.Door = RightDoor(W);
		const ABRMissionDevice* D = S.Door >= 0 ? W->FindMissionDevice(S.Door) : nullptr;
		BRM::FEval E;
		W->GetMissionEval(E);
		const bool bOk = D && E.bSolved && L->GetLoadKey() != S.LoadKey && L->GetPointCount() == 0 && L->GetScannedPlates().Num() == 0;
		Note(FString::Printf(TEXT("reprise : mission resolue %s, bonne porte %d ouverte a %.0f %%, nouvelle identite du scanner %s, points %d (attendu 0), plaques relevees %d (attendu 0)"),
			Oui(E.bSolved), S.Door, D ? D->GetModuleProgress() * 100.f : -1.f, Oui(L->GetLoadKey() != S.LoadKey), L->GetPointCount(), L->GetScannedPlates().Num()), !bOk);
		S.bReady = D != nullptr;
		return true;
	});
	LimitLastStep(45.f);
	AddScan(TEXT("v4.16 lidar : apres la reprise (galerie)"), StandsGallery(), false, EExpect::Open, false, true);
	// 8. Autre plan (autre graine) : releves effaces, la bonne porte (fermee) relevee sur sa plaque
	AddSeedLoad(6, SeedB, FString::Printf(TEXT("v4.16 Level 6 LiDAR, autre plan (graine %u)"), SeedB));
	Add(TEXT("v4.16 lidar : autre plan"), 0.f, [this]()
	{
		FRun& S = Run();
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || !W->IsLevelReady() || W->GetLevelNumber() != 6 || (!W->IsMissionActive() && StepTime < 10.f) || (!C->GetLidar() && StepTime < 5.f))
		{
			if (StepTime > 30.f)
			{
				Note(TEXT("v4.16 lidar : Level 6 (autre plan) non pret apres 30 s"), true);
				S.bReady = false;
				return true;
			}
			return false;
		}
		C->bGodMode = true;
		const uint64 Before = S.LoadKey;
		S.Door = RightDoor(W);
		S.bOld = false;
		const ABRLidar* L = C->GetLidar();
		const ABRMissionDevice* D = S.Door >= 0 ? W->FindMissionDevice(S.Door) : nullptr;
		if (!D || !L)
		{
			Skip(TEXT("v4.16 lidar : autre plan : la bonne porte n'est pas un battant ou scanner absent"));
			S.bReady = false;
			return true;
		}
		S.bReady = true;
		S.LoadKey = L->GetLoadKey();
		Note(FString::Printf(TEXT("autre plan (graine du niveau %u) : identite du scanner changee %s, points %d (attendu 0), bonne porte %d fermee (ouverture %.0f %%)"), W->GetSeed(),
			Oui(S.LoadKey != Before), L->GetPointCount(), S.Door, D->GetModuleProgress() * 100.f), S.LoadKey == Before || L->GetPointCount() != 0);
		return true;
	});
	AddScan(TEXT("v4.16 lidar : autre plan, scan porte fermee (galerie)"), StandsGallery(), false, EExpect::Closed, false, true);
	// 9. Points bornes et locaux
	Add(TEXT("v4.16 lidar : points bornes et locaux"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		if (!L || !GetWorld())
		{
			Skip(TEXT("v4.16 lidar : scanner absent : bornes non verifiees"));
			return true;
		}
		int32 Owned = 0;
		const int32 Scanners = CountScanners(GetWorld(), C, Owned);
		const FBRLidarStats& St = ABRLidar::Stats;
		const int32 FrameCap = BRLidar::PulseMaxPerFrame + BRLidar::FocusMaxPerFrame + BRLidar::GlyphMaxPerFrame + 64;
		const bool bOk = Scanners == 1 && Owned == 1 && L->GetPointCount() <= BRLidar::MaxPoints && St.PeakFramePoints <= FrameCap;
		Note(FString::Printf(TEXT("scanners dans ce monde %d (du joueur local %d), points %d sur %d au plus, pic %d points par image (borne %d), refuses %lld (part d'un groupe "
			"depassee dans une image) ; mesures : %s"), Scanners, Owned,
			L->GetPointCount(), BRLidar::MaxPoints, St.PeakFramePoints, FrameCap, St.Rejected, *St.ToString()), !bOk);
		L->SetTestRecord(false);
		if (UCharacterMovementComponent* M = C->GetCharacterMovement())
		{
			M->SetMovementMode(MOVE_Walking);
		}
		return true;
	});
}

// =====================================================================================================================
// Reseau : la porte ouverte par l'hote, chaque machine scanne la sienne (points locaux, bornes, sur le battant)
// =====================================================================================================================

void ABRAutoTest::AddNetV416LidarSteps(bool bClient)
{
	using namespace BRTestV416Lidar;
	const uint32 LevelSeed = ABRWorld::SeedFromUser(SeedA, 6);
	const TCHAR* Who = bClient ? TEXT("client") : TEXT("hote");
	Add(TEXT("v4.16 reseau : attente de la partie"), 2.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		if (!W || !PC || !GetPlayer() || !W->IsLevelReady() || W->IsTransitioning() || !PC->IsNetGame())
		{
			return false;
		}
		if (!bClient)
		{
			if ((!GS || GS->PlayerArray.Num() < 2) && StepTime < 60.f)
			{
				return false;
			}
			const ABRCharacter* M = BRTestNet::RemoteMate(W);
			if (M && M->IsLevelLoading() && StepTime < 90.f)
			{
				return false;
			}
		}
		GetPlayer()->bGodMode = true;
		NoteNetState(TEXT("debut"));
		return true;
	});
	LimitLastStep(100.f);
	Add(TEXT("v4.16 reseau : Level 6"), 1.f, [this, bClient, LevelSeed, Who]()
	{
		FRun& S = Run();
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (!bClient && S.Phase != 169)
		{
			if (W->IsTransitioning())
			{
				return false;
			}
			S = FRun();
			S.Phase = 169;
			W->RequestTransition(6, false, LevelSeed);
			return false;
		}
		ABRCharacter* C = GetPlayer();
		if ((W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 6 || !W->IsMissionActive() || !C || !C->GetLidar()) && StepTime < 60.f)
		{
			return false;
		}
		S.Phase = 0;
		S.Door = RightDoor(W);
		S.bReady = S.Door >= 0 && C && C->GetLidar();
		if (C)
		{
			C->bGodMode = true;
		}
		Note(FString::Printf(TEXT("%s : Level 6 %s, plan %08x, bonne porte %d (battant %s), scanner local %s"), Who, Oui(W->GetLevelNumber() == 6), BRM::Fingerprint(W->GetMissionPlan()),
			S.Door, Oui(S.Door >= 0), Oui(C && C->GetLidar())), W->GetLevelNumber() != 6 || !C || !C->GetLidar());
		if (S.Door < 0 && W->GetLevelNumber() == 6)
		{
			Skip(FString::Printf(TEXT("%s : la bonne porte n'est pas un battant (placement de secours) : test reseau du battant non verifie"), Who));
		}
		return true;
	});
	LimitLastStep(70.f);
	// Chaque joueur se place dans la galerie (l'hote a 2,2 m, le client a 1,5 m de la porte, cote oppose aux charnieres : la plaque
	// ouverte reste visible a travers l'ouverture), regarde la plaque et la scanne. Phases 170 a 172
	auto AddNetScan = [this, bClient, Who](const FString& Name, bool bOpen)
	{
		Add(Name, 0.f, [this, bClient, Who, Name, bOpen]()
		{
			FRun& S = Run();
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			ABRLidar* L = C ? C->GetLidar() : nullptr;
			ABRMissionDevice* D = (W && S.bReady) ? W->FindMissionDevice(S.Door) : nullptr;
			if (!D || !C || !L)
			{
				Skip(FString::Printf(TEXT("%s : %s non verifie (porte ou scanner absent)"), Who, *Name));
				S.Phase = 0;
				return true;
			}
			const double Now = FPlatformTime::Seconds();
			const FPlate P = PlateOf(D);
			if (S.Phase < 170 || S.Phase > 172)
			{
				L->SetFocus(false);
				const float HH = C->GetCapsuleComponent() ? C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
				const TArray<FVector> Cands = bClient ? TArray<FVector>{ FVector(150.0, 20.0, 0.0), FVector(130.0, 60.0, 0.0), FVector(170.0, -10.0, 0.0) }
					: TArray<FVector>{ FVector(220.0, 75.0, 0.0), FVector(250.0, 105.0, 0.0), FVector(200.0, 40.0, 0.0) };
				FVector Local = Cands[0];
				if (D->IsShown() && !PickStand(C, D, Cands, P.C, Local))
				{
					Skip(FString::Printf(TEXT("%s : %s : aucune place libre et a vue de la plaque dans la galerie"), Who, *Name));
					return true;
				}
				if (bClient)
				{
					S.Place.Start(C, D->GetActorTransform().TransformPosition(FVector(Local.X, Local.Y, HH + 2.0)), &P.C, Now);
				}
				else
				{
					PlaceNear(C, D, Local, P.C, false);
				}
				S.Clock = Now;
				S.Phase = 170;
				return false;
			}
			if (S.Phase == 170)
			{
				const bool bPlaced = !bClient || S.Place.Update(C, Now);
				if (!bPlaced || !D->IsShown() || (bOpen && P.Progress < 1.f) || Now - S.Clock < 0.5)
				{
					if (Now - S.Clock > 30.0)
					{
						Skip(FString::Printf(TEXT("%s : %s : place, chunk ou ouverture de la porte non obtenus en 30 s"), Who, *Name));
						S.Phase = 0;
						return true;
					}
					return false;
				}
				Face(C, P.C);
				L->SetTestRecord(true);
				L->SetFocus(true);
				S.Clock = Now;
				S.Phase = 171;
				return false;
			}
			Face(C, P.C);
			if (!L->IsFocusing())
			{
				L->SetFocus(true);
			}
			if (Now - S.Clock < ScanSeconds)
			{
				return false;
			}
			L->SetFocus(false);
			S.Phase = 0;
			if (!bOpen)
			{
				S.Old = P;
				S.bOld = true;
			}
			const FScan R = Analyze(L, D, S, false);
			const int32 Need = BRLidar::PlateGlyphPoints(D->GetDeviceDef().Label);
			const bool bOk = R.Glyphs >= Need && R.GlyphsOnPlate == R.Glyphs && (!bOpen || R.GlyphsAtOld == 0) && R.DoorImpacts > 0 && R.DoorOnLeaf == R.DoorImpacts && R.Ignored == 0;
			Note(FString::Printf(TEXT("%s : %s : inscriptions %d (une passe : %d), sur la plaque actuelle %d, a l'ancienne place %d ; impacts sur le battant %d, sur ses boites %d, sur la "
				"boite visee ou le corps bloquant %d ; battant ouvert a %.0f %%"), Who, *Name, R.Glyphs, Need, R.GlyphsOnPlate, bOpen ? R.GlyphsAtOld : 0, R.DoorImpacts, R.DoorOnLeaf,
				R.Ignored, P.Progress * 100.f), !bOk);
			return true;
		});
		LimitLastStep(45.f);
	};
	AddNetScan(TEXT("v4.16 reseau : scan porte fermee"), false);
	// L'hote attend le scan du client, puis ouvre la porte pour le groupe (mission resolue : etat replique)
	Add(TEXT("v4.16 reseau : porte ouverte par l'hote"), 0.f, [this, bClient, Who]()
	{
		ABRWorld* W = GetBRWorld();
		if (bClient)
		{
			BRTestNet::SendMark(GetPC(), MarkClosed);
			return true;
		}
		if (W && BRTestNet::RemoteMark(W) != MarkClosed && StepTime < 60.f)
		{
			return false;
		}
		if (W)
		{
			W->DebugCompleteMission();
		}
		Note(FString::Printf(TEXT("%s : scan du client %s, mission resolue par l'hote (la bonne porte s'ouvre chez chacun)"), Who, Oui(W && BRTestNet::RemoteMark(W) == MarkClosed)),
			!W || BRTestNet::RemoteMark(W) != MarkClosed);
		return true;
	});
	LimitLastStep(70.f);
	AddNetScan(TEXT("v4.16 reseau : scan porte ouverte"), true);
	Add(TEXT("v4.16 reseau : points locaux et bornes"), 0.f, [this, bClient, Who]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		if (!W || !C || !L || !GetWorld())
		{
			Skip(FString::Printf(TEXT("%s : scanner absent : points locaux non verifies"), Who));
			return true;
		}
		int32 Owned = 0;
		const int32 Scanners = CountScanners(GetWorld(), C, Owned);
		int32 MateScanners = 0;
		TArray<ABRCharacter*> Players;
		W->GetPlayers(Players);
		for (const ABRCharacter* P : Players)
		{
			MateScanners += (P && P != C && P->GetLidar()) ? 1 : 0;
		}
		const bool bOk = Scanners == 1 && Owned == 1 && MateScanners == 0 && L->GetPointCount() <= BRLidar::MaxPoints && Players.Num() >= 2;
		Note(FString::Printf(TEXT("%s : %d joueur(s), scanners dans ce monde %d (du joueur local %d), scanner du coequipier ici %d (attendu 0 : points non repliques), points %d sur %d "
			"au plus"), Who, Players.Num(), Scanners, Owned, MateScanners, L->GetPointCount(), BRLidar::MaxPoints), !bOk);
		L->SetTestRecord(false);
		if (bClient)
		{
			BRTestNet::SendMark(GetPC(), MarkDone);
		}
		return true;
	});
	// Fin : le client part le premier ; l'hote attend son rapport (120 s au plus)
	Add(TEXT("Fin"), bClient ? 1.f : 2.f, [this, bClient]()
	{
		if (bClient)
		{
			BRTestNet::SendMark(GetPC(), BRTestNet::MarkFinished);
			return true;
		}
		const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		if (!GS || GS->PlayerArray.Num() < 2)
		{
			return true;
		}
		const int32 Mark = BRTestNet::RemoteMark(GetBRWorld());
		if (Mark == BRTestNet::MarkFinished)
		{
			return StepTime > 5.f;
		}
		if (StepTime > 120.f)
		{
			Note(TEXT("hote : le client n'a pas termine son plan 120 s apres celui de l'hote (voir son rapport)"));
			return true;
		}
		return false;
	});
	LimitLastStep(130.f);
	Add(TEXT("Fermeture"), 0.f, [this]()
	{
		Finish();
		return true;
	});
}
