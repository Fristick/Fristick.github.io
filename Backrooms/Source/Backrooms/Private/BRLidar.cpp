// v4.13 : scanner LiDAR du Level 6 (voir BRLidar.h et BRLidarLogic.h). v4.14 : chaque image passe par BRLidar::RunFrame.
#include "BRLidar.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BRConfig.h"
#include "BREntity.h"
#include "BRMission.h"
#include "BRPlayerController.h"
#include "BRWorld.h"
#include "Backrooms.h"
#include "BRLidarShapes.inl"

#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"

namespace BRM = BRMission;

FBRLidarStats ABRLidar::Stats;

FString FBRLidarStats::ToString() const
{
	const double F = Frames > 0 ? static_cast<double>(Frames) : 1.0;
	return FString::Printf(TEXT("%lld images ; rayons %lld (%.1f/image, pic %d) ; visees %lld ; points demandes %lld, acceptes %lld, remplaces %lld, expires %lld, refuses %lld (pic %d/image) ; ")
		TEXT("inscriptions %lld points, %lld rayons sur des cibles -> %lld cibles distinctes ; materiaux mis a jour %lld (%.1f/image) ; ")
		TEXT("temps : rayons et decisions %.3f ms/image, inscriptions %.3f ms/image, envoi aux instances %.3f ms/image, total %.3f ms/image"),
		Frames, Rays, Rays / F, PeakFrameRays, AimTraces, Requested, Accepted, Replaced, Expired, Rejected, PeakFramePoints, GlyphPoints, GlyphTouches, GlyphDistinct,
		MidUpdates, MidUpdates / F, TraceSeconds * 1000.0 / F, GlyphSeconds * 1000.0 / F, UploadSeconds * 1000.0 / F, TotalSeconds * 1000.0 / F);
}

namespace
{
	const TCHAR* LidarSection = TEXT("/Script/Backrooms.BRLidar");

	/** Glyphe de chaque categorie (BRLidarLogic.h : couleur ET forme) */
	const TCHAR* const GlyphMesh[BRLidar::NumCategories] = { TEXT("SM_LidarSquare"), TEXT("SM_LidarTri"), TEXT("SM_LidarDiamond"), TEXT("SM_LidarCross"),
		TEXT("SM_LidarChevron"), TEXT("SM_LidarDot") };
	const FLinearColor GlyphColor[BRLidar::NumCategories] = { FLinearColor(0.55f, 0.75f, 1.f), FLinearColor(1.f, 0.42f, 0.04f), FLinearColor(0.2f, 1.f, 0.35f),
		FLinearColor(1.f, 0.06f, 0.06f), FLinearColor(0.15f, 0.42f, 1.f), FLinearColor(1.f, 0.72f, 0.22f) };
	/** Taille de base (cm) avant le reglage de taille */
	const float GlyphSize[BRLidar::NumCategories] = { 2.2f, 4.5f, 4.f, 5.5f, 12.f, 2.6f };
	/** Intensite emissive de base (multipliee par le contraste et l'attenuation du groupe) */
	constexpr float GlowBase = 7.f;
	/** Variation d'attenuation sous laquelle le materiau d'un groupe n'est pas reecrit */
	constexpr float FadeStep = 1.f / 64.f;
}

ABRLidar::ABRLidar()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetReplicates(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

BRLidar::FSettings& ABRLidar::Settings()
{
	static BRLidar::FSettings S;
	return S;
}

void ABRLidar::LoadSettings()
{
	const FConfigFile& Cfg = BRConfig::Get();
	BRLidar::FSettings S = Settings();
	FString V;
	if (Cfg.GetString(LidarSection, TEXT("PointSize"), V)) S.PointSize = FCString::Atof(*V);
	if (Cfg.GetString(LidarSection, TEXT("Contrast"), V)) S.Contrast = FCString::Atof(*V);
	if (Cfg.GetString(LidarSection, TEXT("Persistence"), V)) S.Persistence = FCString::Atof(*V);
	Settings() = BRLidar::Clamp(S);
}

void ABRLidar::SaveSettings()
{
	Settings() = BRLidar::Clamp(Settings());
	FConfigFile& Cfg = BRConfig::Get();
	Cfg.SetString(LidarSection, TEXT("PointSize"), *FString::SanitizeFloat(Settings().PointSize));
	Cfg.SetString(LidarSection, TEXT("Contrast"), *FString::SanitizeFloat(Settings().Contrast));
	Cfg.SetString(LidarSection, TEXT("Persistence"), *FString::SanitizeFloat(Settings().Persistence));
	BRConfig::Save();
}

void ABRLidar::Init(ABRCharacter* InOwner)
{
	Holder = InOwner;
	SetOwner(InOwner);
	LoadSettings();
	BuildComponents();
	State.Reset();
	State.Store.Persistence = Settings().Persistence;
	LoadKey = ComputeLoadKey();
	if (const ABRWorld* W = ABRWorld::Get(this))
	{
		MarkShownAt.Init(-1, W->GetLidarMarks().Num());
	}
}

uint64 ABRLidar::ComputeLoadKey() const
{
	const ABRWorld* W = ABRWorld::Get(this);
	if (!W)
	{
		return 0;
	}
	// Identite du chargement : niveau, graine, numero de chargement et empreinte du plan de mission (un autre plan sur la
	// meme graine change aussi la cle)
	const uint32 Plan = W->IsMissionActive() ? BRM::Fingerprint(W->GetMissionPlan()) : 0u;
	uint64 K = (static_cast<uint64>(W->GetSeed()) << 32) | Plan;
	K ^= static_cast<uint64>(static_cast<uint32>(W->GetLevelNumber())) * 0x9E3779B97F4A7C15ull;
	K ^= static_cast<uint64>(static_cast<uint32>(W->GetLevelSerial())) * 0xC2B2AE3D27D4EB4Full;
	return K;
}

void ABRLidar::ResetForLoad()
{
	SetFocus(false);
	for (UInstancedStaticMeshComponent* C : Ism)
	{
		if (C)
		{
			C->ClearInstances();
		}
	}
	State.Reset();
	State.Store.Persistence = Settings().Persistence;
	for (TArray<FTransform>& P : Pending)
	{
		P.Reset();
	}
	ScannedPlates.Empty();
	GlyphCache.Empty();
	FocusDevice.Reset();
	HoldEntity.Reset();
	FocusHold = FocusSend = EntityHold = 0.f;
	const ABRWorld* W = ABRWorld::Get(this);
	MarkShownAt.Init(-1, W ? W->GetLidarMarks().Num() : 0);
	LoadKey = ComputeLoadKey();
	UpdateFades(true);
	UE_LOG(LogBackrooms, Log, TEXT("[Lidar] Nouveau chargement (Niveau %d, graine %u, chargement %d) : points, releves et cibles effaces"),
		W ? W->GetLevelNumber() : -1, W ? W->GetSeed() : 0u, W ? W->GetLevelSerial() : -1);
}

void ABRLidar::BuildComponents()
{
	UBRAssets* A = UBRAssets::Get(this);
	if (!A)
	{
		return;
	}
	Ism.Reset();
	Mids.Reset();
	for (int32 Cat = 0; Cat < BRLidar::NumCategories; ++Cat)
	{
		UStaticMesh* M = A->Mesh(GlyphMesh[Cat]);
		bPlaneFallback[Cat] = M == nullptr;
		if (!M)
		{
			M = A->Plane();
		}
		for (int32 B = 0; B < BRLidar::AgeBuckets; ++B)
		{
			UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
			C->SetupAttachment(RootComponent);
			C->SetStaticMesh(M);
			C->SetMobility(EComponentMobility::Movable);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCanEverAffectNavigation(false);
			// Aucun cout d'eclairage : pas d'ombre, pas d'eclairage indirect (Lumen), pas de champ de distance, pas de lancer
			// de rayons materiel, pas de reflets captes
			C->SetCastShadow(false);
			C->bAffectDynamicIndirectLighting = false;
			C->bAffectDistanceFieldLighting = false;
			C->bVisibleInRayTracing = false;
			C->bVisibleInReflectionCaptures = false;
			C->bVisibleInRealTimeSkyCaptures = false;
			C->SetReceivesDecals(false);
			C->SetCullDistances(0, static_cast<int32>(BRLidar::Range * 1.6f));
			UMaterialInstanceDynamic* Mid = A->NewGlow(this, GlyphColor[Cat], 0.f);
			C->SetMaterial(0, Mid);
			C->RegisterComponent();
			Ism.Add(C);
			Mids.Add(Mid);
		}
	}
	SentFade.Init(-1.f, BRLidar::NumCategories * BRLidar::AgeBuckets);
	// Scanner tenu en main, devant la camera (vue a la premiere personne seulement)
	if (ABRCharacter* P = Holder.Get())
	{
		if (UStaticMesh* Hand = A->Mesh(TEXT("SM_LidarScanner")))
		{
			HandMesh = NewObject<UStaticMeshComponent>(this);
			HandMesh->SetStaticMesh(Hand);
			HandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			HandMesh->SetCastShadow(false);
			HandMesh->RegisterComponent();
			if (USceneComponent* View = P->GetViewComponent())
			{
				HandMesh->AttachToComponent(View, FAttachmentTransformRules::KeepRelativeTransform);
			}
			HandMesh->SetRelativeLocation(FVector(34.f, 17.f, -19.f));
			HandMesh->SetRelativeRotation(FRotator(4.f, -6.f, 0.f));
			A->ApplySlots(HandMesh);
		}
		FocusAudio = NewObject<UAudioComponent>(this);
		FocusAudio->SetupAttachment(RootComponent);
		FocusAudio->bAutoActivate = false;
		FocusAudio->bIsUISound = true;
		if (USoundBase* Hum = A->Sound(TEXT("S_LidarFocus")))
		{
			FocusAudio->SetSound(Hum);
		}
		FocusAudio->RegisterComponent();
	}
}

void ABRLidar::EndPlay(const EEndPlayReason::Type Reason)
{
	SetFocus(false);
	if (HandMesh)
	{
		HandMesh->DestroyComponent();
		HandMesh = nullptr;
	}
	Super::EndPlay(Reason);
}

bool ABRLidar::CanScan() const
{
	const ABRCharacter* P = Holder.Get();
	const ABRWorld* W = ABRWorld::Get(this);
	if (!P || !W || P->IsDead() || P->IsInputLocked() || !W->IsLevelReady() || W->IsTransitioning())
	{
		return false;
	}
	// Inventaire, pause, menu, dialogue, saisie d'une touche : pas de scan (un nouvel appui sera necessaire)
	const ABRPlayerController* PC = Cast<ABRPlayerController>(P->GetController());
	return !PC || PC->CanPlayActions();
}

bool ABRLidar::Pulse()
{
	ABRCharacter* P = Holder.Get();
	if (!P || !CanScan() || !State.StartPulse(FMath::FRandRange(0.f, 6.2831f)))
	{
		return false;
	}
	PulseOrigin = P->GetEyeLocation();
	PulseRot = P->GetAimRotation().Quaternion();
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(TEXT("S_LidarPulse")))
		{
			UGameplayStatics::PlaySound2D(this, S, 0.6f);
		}
	}
	// L'impulsion s'entend : les Smilers proches viennent voir (regle lisible, verifiee par l'hote)
	P->NotifyLidarPulse(PulseOrigin);
	return true;
}

void ABRLidar::SetFocus(bool bOn)
{
	if (bOn && !CanScan())
	{
		bOn = false;
	}
	if (bFocus == bOn)
	{
		return;
	}
	bFocus = bOn;
	// v4.14 : tout ce qui dependait de l'appui s'arrete : releve en cours, maintien sur un Smiler, son
	FocusHold = 0.f;
	FocusSend = 0.f;
	EntityHold = 0.f;
	FocusDevice.Reset();
	if (HoldEntity.IsValid())
	{
		HoldEntity.Reset();
		if (ABRCharacter* P = Holder.Get())
		{
			P->SetLidarAimTarget(nullptr);
		}
	}
	if (FocusAudio)
	{
		if (bOn)
		{
			FocusAudio->Play();
		}
		else
		{
			FocusAudio->Stop();
		}
	}
}

void ABRLidar::AddPoint(uint8 Category, const FVector& At, const FVector& Normal, float Size, const FVector* Along)
{
	if (Category >= BRLidar::NumCategories)
	{
		return;
	}
	const FVector N = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
	FMatrix M;
	if (bPlaneFallback[Category])
	{
		// Plan du moteur : il regarde +Z
		M = (Along && !Along->IsNearlyZero()) ? FRotationMatrix::MakeFromZY(N, *Along) : FRotationMatrix::MakeFromZ(N);
	}
	else
	{
		// Glyphes du kit : forme dans le plan YZ, face vers +X ; le chevron pointe vers +Y
		M = (Along && !Along->IsNearlyZero()) ? FRotationMatrix::MakeFromXY(N, *Along) : FRotationMatrix::MakeFromX(N);
	}
	const float S = Size * Settings().PointSize / 100.f;
	Pending[Category].Add(FTransform(M.ToQuat(), At + N * 0.6f, FVector(S, S, S)));
}

const ABRLidar::FGlyphCache* ABRLidar::GetGlyphCache(int32 DeviceIndex)
{
	if (const FGlyphCache* Found = GlyphCache.Find(DeviceIndex))
	{
		return Found;
	}
	const ABRWorld* W = ABRWorld::Get(this);
	ABRMissionDevice* Dev = W ? W->FindMissionDevice(DeviceIndex) : nullptr;
	if (!Dev)
	{
		return nullptr;
	}
	const BRM::FDevice& D = Dev->GetDeviceDef();
	const int32 Fig = FMath::Clamp<int32>(D.Info[0], 0, BRLidarShapes::NumFigures - 1);
	FGlyphCache Cache;
	if (D.Role == BRM::R_LidarDoor)
	{
		// Plaque : a gauche la silhouette (40 cm de haut), a droite le code (grille 7 x 7, 18 cm). Coordonnees fixes,
		// calculees une fois par chargement
		FVector C, R, U, N;
		if (!Dev->GetLidarPlate(C, R, U, N))
		{
			return nullptr;
		}
		Cache.bPlate = true;
		Cache.Normal = N;
		const int32 Total = BRLidar::PlateGlyphPoints(D.Label);
		for (int32 K = 0; K < Total; ++K)
		{
			int32 FigIdx = 0, Row = 0, Col = 0;
			if (BRLidar::PlateGlyphPoint(D.Label, K, BRLidarShapes::PointsPerFigure, FigIdx, Row, Col))
			{
				Cache.Points.Add(C + R * (12.f + (Col - 3) * 2.8f) + U * ((3 - Row) * 2.8f));
			}
			else
			{
				const short* Pt = BRLidarShapes::Points[Fig][FigIdx];
				Cache.Points.Add(C + R * (-12.f + Pt[1] * 0.1f * 0.22f) + U * (Pt[2] * 0.1f * 0.22f - 18.f));
			}
		}
	}
	else if (D.Role == BRM::R_LidarCage)
	{
		// Silhouette debout dans la cage (1,8 m au plus : la cage mesure 2,1 m), face a l'avant de la cage
		FTransform F;
		if (!Dev->GetCageFrame(F))
		{
			return nullptr;
		}
		Cache.Normal = F.GetRotation().GetForwardVector();
		for (int32 K = 0; K < BRLidar::CageFigurePoints; ++K)
		{
			const short* Pt = BRLidarShapes::Points[Fig][BRLidar::CageGlyphFigure(K, BRLidarShapes::PointsPerFigure)];
			Cache.Points.Add(F.TransformPosition(FVector(Pt[0] * 0.09f, Pt[1] * 0.09f, Pt[2] * 0.09f)));
		}
	}
	else
	{
		return nullptr;
	}
	return &GlyphCache.Add(DeviceIndex, MoveTemp(Cache));
}

BRLidar::FHit ABRLidar::TraceRay(bool bPulse, bool bCenter, float X, float Y, float Z)
{
	BRLidar::FHit Out;
	UWorld* World = GetWorld();
	const ABRWorld* W = ABRWorld::Get(this);
	ABRCharacter* P = Holder.Get();
	if (!World || !W || !P)
	{
		return Out;
	}
	++Stats.Rays;
	const FVector From = bPulse ? PulseOrigin : ViewOrigin;
	const FVector Dir = (bPulse ? PulseRot : ViewRot).RotateVector(FVector(X, Y, Z));
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BRLidar), false, P);
	Q.AddIgnoredActor(this);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, From, From + Dir * BRLidar::Range, ECC_Visibility, Q))
	{
		return Out;
	}
	AActor* A = Hit.GetActor();
	const FVector At = Hit.ImpactPoint;
	const FVector N = Hit.ImpactNormal;
	if (Cast<ABREntity>(A))
	{
		// Menace : croix rouges, tournees vers le scanner
		AddPoint(BRLidar::Entity, At, -Dir, GlyphSize[BRLidar::Entity]);
		return Out;
	}
	if (ABRMissionDevice* D = Cast<ABRMissionDevice>(A))
	{
		AddPoint(BRLidar::Device, At, N, GlyphSize[BRLidar::Device]);
		if (!bPulse)
		{
			// Inscriptions : la cible est seulement signalee ; RunFrame la deduplique et la dessine sous le budget commun
			const BRM::FDevice& Def = D->GetDeviceDef();
			if (Def.Role == BRM::R_LidarCage && D->GetGateValue() >= 128)
			{
				Out.GlyphTarget = D->GetIndex();
				Out.GlyphTotal = BRLidar::CageFigurePoints;
			}
			else if (Def.Role == BRM::R_LidarDoor)
			{
				Out.GlyphTarget = D->GetIndex();
				Out.GlyphTotal = BRLidar::PlateGlyphPoints(Def.Label);
			}
		}
		return Out;
	}
	(void)bCenter;
	// Decor : danger (au fond ou au bord d'une fosse), repere au sol, ou surface ordinaire
	if (W->HasPits() && (At.Z < -40.f || (N.Z > 0.7f && W->IsOverPit(At, 35.f))))
	{
		AddPoint(BRLidar::Danger, At, N, GlyphSize[BRLidar::Danger]);
		return Out;
	}
	if (N.Z > 0.7f)
	{
		const TArray<FVector>& Marks = W->GetLidarMarks();
		const TArray<FVector>& Dirs = W->GetLidarMarkDirs();
		const int32 Serial = State.Store.Serial[BRLidar::Marker];
		for (int32 I = 0; I < Marks.Num(); ++I)
		{
			if (FVector::DistSquared2D(Marks[I], At) < FMath::Square(70.f) && FMath::Abs(Marks[I].Z - At.Z) < 40.f)
			{
				// Un chevron par repere et par groupe des reperes, pose sur le repere (pas a l'endroit touche)
				if (MarkShownAt.IsValidIndex(I) && MarkShownAt[I] != Serial && Dirs.IsValidIndex(I))
				{
					MarkShownAt[I] = Serial;
					AddPoint(BRLidar::Marker, Marks[I] + FVector(0.f, 0.f, 1.f), FVector::UpVector, GlyphSize[BRLidar::Marker], &Dirs[I]);
				}
				break;
			}
		}
	}
	AddPoint(BRLidar::Env, At, N, GlyphSize[BRLidar::Env]);
	return Out;
}

int ABRLidar::EmitGlyphs(int Target, int First, int Count)
{
	const double T0 = FPlatformTime::Seconds();
	const FGlyphCache* C = GetGlyphCache(Target);
	int Added = 0;
	if (C)
	{
		const float Size = GlyphSize[BRLidar::Glyph] * (C->bPlate ? 0.75f : 1.f);
		const int32 End = FMath::Min(First + Count, C->Points.Num());
		for (int32 K = FMath::Max(0, First); K < End; ++K)
		{
			AddPoint(BRLidar::Glyph, C->Points[K], C->Normal, Size);
			++Added;
		}
	}
	Stats.GlyphSeconds += FPlatformTime::Seconds() - T0;
	return Added;
}

void ABRLidar::ApplyFrame(const BRLidar::FFrameResult& R)
{
	const double T0 = FPlatformTime::Seconds();
	const bool bReady = Ism.Num() == BRLidar::NumCategories * BRLidar::AgeBuckets;
	int32 FramePoints = 0;
	for (int32 Cat = 0; Cat < BRLidar::NumCategories; ++Cat)
	{
		// Les groupes vides par l'horloge ou remplaces d'abord, puis les points acceptes dans le groupe courant
		for (int32 B = 0; B < BRLidar::AgeBuckets && bReady; ++B)
		{
			if (R.Cleared[Cat] & (1u << B))
			{
				const int32 I = Cat * BRLidar::AgeBuckets + B;
				if (Ism[I])
				{
					Ism[I]->ClearInstances();
				}
				SentFade[I] = -1.f;
			}
		}
		TArray<FTransform>& P = Pending[Cat];
		Stats.Requested += P.Num();
		if (P.Num() > R.Accepted[Cat])
		{
			Stats.Rejected += P.Num() - R.Accepted[Cat];
			P.SetNum(R.Accepted[Cat]);
		}
		if (bReady && P.Num() > 0)
		{
			const int32 I = Cat * BRLidar::AgeBuckets + R.Bucket[Cat];
			if (Ism[I])
			{
				Ism[I]->AddInstances(P, false, true);
			}
			SentFade[I] = -1.f;
		}
		Stats.Accepted += P.Num();
		FramePoints += P.Num();
		P.Reset();
	}
	Stats.PeakFramePoints = FMath::Max(Stats.PeakFramePoints, FramePoints);
	Stats.PeakFrameRays = FMath::Max(Stats.PeakFrameRays, R.PulseRays + R.FocusRays);
	Stats.GlyphPoints += R.GlyphPoints;
	// Plaques completees : le releve entre au carnet (une fois par porte et par chargement)
	const ABRWorld* W = ABRWorld::Get(this);
	for (int32 K = 0; K < R.NumCompleted && W && W->IsMissionActive(); ++K)
	{
		const BRM::FPlan& Plan = W->GetMissionPlan();
		const int32 Idx = R.Completed[K];
		if (Idx < 0 || Idx >= Plan.NumDevices || Plan.Devices[Idx].Role != BRM::R_LidarDoor)
		{
			continue;
		}
		int32 Rank = 0;
		for (int32 I = 0; I <= Idx; ++I)
		{
			Rank += Plan.Devices[I].Role == BRM::R_LidarDoor ? 1 : 0;
		}
		if (!ScannedPlates.Contains(Rank))
		{
			ScannedPlates.Add(Rank, FIntPoint(Plan.Devices[Idx].Info[0], Plan.Devices[Idx].Label));
		}
	}
	Stats.UploadSeconds += FPlatformTime::Seconds() - T0;
}

void ABRLidar::UpdateFades(bool bForce)
{
	const BRLidar::FSettings& S = Settings();
	const bool bContrast = SentContrast != S.Contrast;
	SentContrast = S.Contrast;
	for (int32 Cat = 0; Cat < BRLidar::NumCategories; ++Cat)
	{
		for (int32 B = 0; B < BRLidar::AgeBuckets; ++B)
		{
			const int32 I = Cat * BRLidar::AgeBuckets + B;
			if (!Mids.IsValidIndex(I) || !Mids[I] || !SentFade.IsValidIndex(I))
			{
				continue;
			}
			const float Fade = State.Store.FadeOf(Cat, B);
			// v4.14 : le materiau n'est reecrit que si l'attenuation change visiblement (48 materiaux, pas a chaque image)
			if (!bForce && !bContrast && SentFade[I] >= 0.f && FMath::Abs(Fade - SentFade[I]) < FadeStep && (Fade > 0.f) == (SentFade[I] > 0.f))
			{
				continue;
			}
			SentFade[I] = Fade;
			// Les dangers et les menaces restent plus vifs (lisibles meme en fin de vie)
			const float Keep = (Cat == BRLidar::Danger || Cat == BRLidar::Entity) ? 1.4f : 1.f;
			Mids[I]->SetVectorParameterValue(TEXT("Emissive"), GlyphColor[Cat] * GlowBase * S.Contrast * Keep * Fade);
			++Stats.MidUpdates;
		}
	}
}

void ABRLidar::UpdateAim(float Dt)
{
	ABRCharacter* P = Holder.Get();
	UWorld* World = GetWorld();
	if (!P || !World || !bFocus)
	{
		return;
	}
	// Visee du scan focalise (un rayon par image, sans point) : mecanisme a relever, Smiler a repousser
	++Stats.AimTraces;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BRLidarAim), false, P);
	Q.AddIgnoredActor(this);
	FHitResult Hit;
	const FVector End = ViewOrigin + ViewRot.GetForwardVector() * BRLidar::Range;
	AActor* A = World->LineTraceSingleByChannel(Hit, ViewOrigin, End, ECC_Visibility, Q) ? Hit.GetActor() : nullptr;
	// Mire et cages : le scan focalise maintenu releve le mecanisme (meme action que la main, verifiee par l'hote)
	ABRMissionDevice* D = Cast<ABRMissionDevice>(A);
	FocusDevice = D;
	const bool bObserve = D && D->GetDeviceDef().Kind == BRM::EKind::Observe
		&& (D->GetDeviceDef().Role == BRM::R_LidarTarget || D->GetDeviceDef().Role == BRM::R_LidarCage)
		&& FVector::DistSquared(P->GetEyeLocation(), D->GetInteractPoint()) < FMath::Square(ABRWorld::MissionScanReachServer - 40.f);
	if (bObserve)
	{
		FocusHold += Dt;
		FocusSend += Dt;
		if (FocusSend >= 0.5f)
		{
			FocusSend = 0.f;
			P->RequestMissionAction(D->GetIndex(), static_cast<uint8>(BRM::EAction::Hold));
		}
	}
	else
	{
		FocusHold = 0.f;
		FocusSend = 0.f;
	}
	// v4.14 : Smiler tenu dans le faisceau : le maintien ne compte que sur LA MEME cible, sans interruption ; l'hote
	// mesure aussi la duree depuis le debut de la visee (ServerLidarAim) avant d'accepter le recul
	ABREntity* E = Cast<ABREntity>(A);
	if (E && E->Kind != EBREntityKind::Smiler)
	{
		E = nullptr;
	}
	if (E != HoldEntity.Get())
	{
		HoldEntity = E;
		EntityHold = 0.f;
		P->SetLidarAimTarget(E);
	}
	else if (E)
	{
		EntityHold += Dt;
		if (EntityHold >= BRLidar::RepelHold)
		{
			EntityHold = 0.f;
			P->RequestLidarRepel(E);
		}
	}
}

void ABRLidar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double T0 = FPlatformTime::Seconds();
	ABRCharacter* P = Holder.Get();
	const ABRWorld* W = ABRWorld::Get(this);
	if (!P || !W)
	{
		return;
	}
	// v4.14 : un autre chargement (6 -> 6, autre graine, autre plan) : rien de l'ancien ne reste
	if (W->IsLevelReady() && !W->IsTransitioning() && ComputeLoadKey() != LoadKey)
	{
		ResetForLoad();
	}
	if (HandMesh)
	{
		HandMesh->SetVisibility(!P->IsThirdPerson() && !P->IsDead());
	}
	const bool bCan = CanScan();
	if (bFocus && !bCan)
	{
		SetFocus(false); // inventaire, pause, dialogue, entree verrouillee, mort, transition : il faudra un nouvel appui
	}
	if (!W->IsLevelReady() || W->IsTransitioning())
	{
		return;
	}
	if (MarkShownAt.Num() != W->GetLidarMarks().Num())
	{
		MarkShownAt.Init(-1, W->GetLidarMarks().Num());
	}
	ViewOrigin = P->GetEyeLocation();
	ViewRot = P->GetAimRotation().Quaternion();
	const double TTrace = FPlatformTime::Seconds();
	BRLidar::FFrameResult R;
	const double Glyph0 = Stats.GlyphSeconds;
	const long long Replaced0 = State.Store.Replaced;
	const long long Expired0 = State.Store.Expired;
	const long long Touches0 = State.Glyphs.Touches;
	const long long Distinct0 = State.Glyphs.DistinctTouches;
	BRLidar::RunFrame(State, DeltaSeconds, Settings().Persistence, bFocus && bCan, *this, R);
	Stats.TraceSeconds += (FPlatformTime::Seconds() - TTrace) - (Stats.GlyphSeconds - Glyph0);
	Stats.Replaced += State.Store.Replaced - Replaced0;
	Stats.Expired += State.Store.Expired - Expired0;
	Stats.GlyphTouches += State.Glyphs.Touches - Touches0;
	Stats.GlyphDistinct += State.Glyphs.DistinctTouches - Distinct0;
	ApplyFrame(R);
	if (bFocus && bCan)
	{
		UpdateAim(DeltaSeconds);
	}
	UpdateFades(false);
	++Stats.Frames;
	Stats.TotalSeconds += FPlatformTime::Seconds() - T0;
}
