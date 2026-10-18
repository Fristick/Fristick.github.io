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
		TEXT("temps : rayons et decisions %.3f ms/image, inscriptions %.3f ms/image, envoi aux instances %.3f ms/image, total %.3f ms/image ; ")
		TEXT("v4.16 : rayons arretes par un battant %lld, passes recommencees (support deplace) %lld, rayons sur une inscription en mouvement %lld"),
		Frames, Rays, Rays / F, PeakFrameRays, AimTraces, Requested, Accepted, Replaced, Expired, Rejected, PeakFramePoints, GlyphPoints, GlyphTouches, GlyphDistinct,
		MidUpdates, MidUpdates / F, TraceSeconds * 1000.0 / F, GlyphSeconds * 1000.0 / F, UploadSeconds * 1000.0 / F, TotalSeconds * 1000.0 / F, LeafHits, GlyphPoseRestarts,
		GlyphMovingSkips);
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

	/** v4.16 : repere d'une transformation du moteur (sans echelle) pour la logique pure (BRLidarLogic.h) */
	BRLidar::FFrame3 LidarFrameOf(const FTransform& T)
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

	void LidarToFloats(const FVector& V, float Out[3])
	{
		Out[0] = static_cast<float>(V.X);
		Out[1] = static_cast<float>(V.Y);
		Out[2] = static_cast<float>(V.Z);
	}
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
	if (Cfg.GetString(LidarSection, TEXT("StaticAid"), V)) S.StaticAid = FCString::Atoi(*V);
	Settings() = BRLidar::Clamp(S);
}

void ABRLidar::SaveSettings()
{
	Settings() = BRLidar::Clamp(Settings());
	FConfigFile& Cfg = BRConfig::Get();
	Cfg.SetString(LidarSection, TEXT("PointSize"), *FString::SanitizeFloat(Settings().PointSize));
	Cfg.SetString(LidarSection, TEXT("Contrast"), *FString::SanitizeFloat(Settings().Contrast));
	Cfg.SetString(LidarSection, TEXT("Persistence"), *FString::SanitizeFloat(Settings().Persistence));
	Cfg.SetString(LidarSection, TEXT("StaticAid"), *FString::FromInt(Settings().StaticAid));
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
	ScanLeaves.Reset();
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
	if (bTestRecord && TestPoints.Num() < MaxTestPoints)
	{
		FTestPoint& T = TestPoints.AddDefaulted_GetRef();
		T.At = At + N * 0.6f;
		T.Category = Category;
		T.Device = CtxDevice;
		T.bLeaf = bCtxLeaf;
		T.Component = CtxComponent;
		T.Frame = GFrameCounter;
	}
}

ABRLidar::FGlyphCache* ABRLidar::GetGlyphCache(int32 DeviceIndex)
{
	if (FGlyphCache* Found = GlyphCache.Find(DeviceIndex))
	{
		return Found;
	}
	const ABRWorld* W = ABRWorld::Get(this);
	ABRMissionDevice* Dev = W ? W->FindMissionDevice(DeviceIndex) : nullptr;
	FTransform Support;
	if (!Dev || !Dev->GetLidarSupport(Support))
	{
		return nullptr;
	}
	// v4.16 : forme calculee une fois par chargement a la pose actuelle, puis gardee dans le repere de son support (le battant
	// de la porte pivote ; v4.15 gardait les coordonnees du monde)
	const BRLidar::FFrame3 SF = LidarFrameOf(Support);
	const BRM::FDevice& D = Dev->GetDeviceDef();
	const int32 Fig = FMath::Clamp<int32>(D.Info[0], 0, BRLidarShapes::NumFigures - 1);
	FGlyphCache Cache;
	auto AddWorld = [&Cache, &SF](const FVector& P)
	{
		float WP[3], LP[3];
		LidarToFloats(P, WP);
		BRLidar::PointToLocal(SF, WP, LP);
		Cache.Local.Add(FVector(LP[0], LP[1], LP[2]));
	};
	FVector WorldNormal = FVector::ForwardVector;
	if (D.Role == BRM::R_LidarDoor)
	{
		// Plaque : a gauche la silhouette (40 cm de haut), a droite le code (grille 7 x 7, 18 cm)
		FVector C, R, U, N;
		if (!Dev->GetLidarPlate(C, R, U, N))
		{
			return nullptr;
		}
		Cache.bPlate = true;
		WorldNormal = N;
		const int32 Total = BRLidar::PlateGlyphPoints(D.Label);
		for (int32 K = 0; K < Total; ++K)
		{
			float PR = 0.f, PU = 0.f;
			BRLidar::PlateShapePoint(D.Label, K, BRLidarShapes::Points[Fig], BRLidarShapes::PointsPerFigure, PR, PU);
			AddWorld(C + R * PR + U * PU);
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
		WorldNormal = F.GetRotation().GetForwardVector();
		for (int32 K = 0; K < BRLidar::CageFigurePoints; ++K)
		{
			float CP[3];
			BRLidar::CageShapePoint(K, BRLidarShapes::Points[Fig], BRLidarShapes::PointsPerFigure, CP);
			AddWorld(F.TransformPosition(FVector(CP[0], CP[1], CP[2])));
		}
	}
	else
	{
		return nullptr;
	}
	float WN[3], LN[3];
	LidarToFloats(WorldNormal, WN);
	BRLidar::DirToLocal(SF, WN, LN);
	Cache.LocalNormal = FVector(LN[0], LN[1], LN[2]);
	Cache.Track.Update(SF);
	return &GlyphCache.Add(DeviceIndex, MoveTemp(Cache));
}

bool ABRLidar::GetGlyphTrack(int32 DeviceIndex, uint32& OutRev, bool& bOutMoving) const
{
	const FGlyphCache* G = GlyphCache.Find(DeviceIndex);
	if (!G)
	{
		return false;
	}
	OutRev = G->Track.Rev;
	bOutMoving = G->Track.bMoving;
	return true;
}

void ABRLidar::AddScanIgnored(const ABRWorld* W, FCollisionQueryParams& Q)
{
	if (!W)
	{
		return;
	}
	// Collisions de l'interaction et du passage : pas des surfaces a representer (un mecanisme masque n'en a plus)
	TArray<const UPrimitiveComponent*> Ignored;
	for (const ABRMissionDevice* D : W->GetMissionDevices())
	{
		if (D && D->IsShown())
		{
			D->GetLidarIgnored(Ignored);
		}
	}
	for (const UPrimitiveComponent* C : Ignored)
	{
		if (C)
		{
			Q.AddIgnoredComponent(C);
		}
	}
}

ABRMissionDevice* ABRLidar::TraceLeaves(const ABRWorld* W, const FVector& From, const FVector& Dir, float MaxDist, float& OutDist, FVector& OutNormal)
{
	ABRMissionDevice* Best = nullptr;
	if (!W)
	{
		return Best;
	}
	float F3[3], D3[3];
	LidarToFloats(From, F3);
	LidarToFloats(Dir, D3);
	float Reach = MaxDist;
	for (ABRMissionDevice* D : W->GetMissionDevices())
	{
		FTransform Leaf;
		if (!D || !D->GetLidarLeaf(Leaf))
		{
			continue;
		}
		const BRLidar::FFrame3 LF = LidarFrameOf(Leaf);
		for (int32 B = 0; B < BRLidar::DoorScanBoxes; ++B)
		{
			float Lo[3], Hi[3], T = 0.f, BN[3];
			BRLidar::DoorScanBox(B, Lo, Hi);
			if (BRLidar::RayBox(LF, Lo, Hi, F3, D3, Reach, T, BN) && T < Reach)
			{
				Reach = T;
				Best = D;
				OutDist = T;
				OutNormal = FVector(BN[0], BN[1], BN[2]);
			}
		}
	}
	return Best;
}

void ABRLidar::PrepareScan(const FVector& Eye)
{
	const ABRWorld* W = ABRWorld::Get(this);
	ScanQuery = FCollisionQueryParams(SCENE_QUERY_STAT(BRLidar), false, Holder.Get());
	ScanQuery.AddIgnoredActor(this);
	AddScanIgnored(W, ScanQuery);
	ScanLeaves.Reset();
	if (!W)
	{
		return;
	}
	for (ABRMissionDevice* D : W->GetMissionDevices())
	{
		// Battant : ses boites, a sa pose de cette image (au-dela de la portee : rien a toucher)
		FTransform Leaf;
		if (D && D->GetLidarLeaf(Leaf) && FVector::DistSquared(Leaf.GetLocation(), Eye) < FMath::Square(BRLidar::Range + 400.f))
		{
			FScanLeaf& L = ScanLeaves.AddDefaulted_GetRef();
			L.Frame = LidarFrameOf(Leaf);
			L.Device = D;
		}
	}
	// Supports des inscriptions deja relevees : immobiles ou en mouvement, revision de leur pose
	for (TPair<int32, FGlyphCache>& E : GlyphCache)
	{
		const ABRMissionDevice* D = W->FindMissionDevice(E.Key);
		FTransform Support;
		if (D && D->GetLidarSupport(Support))
		{
			E.Value.Track.Update(LidarFrameOf(Support));
		}
	}
}

BRLidar::FHit ABRLidar::TraceRay(bool bPulse, bool bCenter, float X, float Y, float Z)
{
	BRLidar::FHit Out;
	UWorld* World = GetWorld();
	const ABRWorld* W = ABRWorld::Get(this);
	ABRCharacter* P = Holder.Get();
	CtxDevice = -1;
	bCtxLeaf = false;
	CtxComponent.Reset();
	if (!World || !W || !P)
	{
		return Out;
	}
	++Stats.Rays;
	const FVector From = bPulse ? PulseOrigin : ViewOrigin;
	const FVector Dir = (bPulse ? PulseRot : ViewRot).RotateVector(FVector(X, Y, Z));
	// v4.16 : requete de l'image (PrepareScan) : boite visee et corps bloquant des portes du passage ignores
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, From, From + Dir * BRLidar::Range, ECC_Visibility, ScanQuery);
	// v4.16 : battants des portes du passage : leurs boites, a la pose de cette image, devant le decor touche
	float Reach = bHit ? static_cast<float>(FVector::Dist(From, Hit.ImpactPoint)) : BRLidar::Range;
	ABRMissionDevice* LeafDevice = nullptr;
	float LeafN[3] = { 0.f, 0.f, 0.f };
	if (ScanLeaves.Num() > 0)
	{
		float F3[3], D3[3];
		LidarToFloats(From, F3);
		LidarToFloats(Dir, D3);
		for (const FScanLeaf& L : ScanLeaves)
		{
			ABRMissionDevice* LD = L.Device.Get();
			for (int32 B = 0; LD && B < BRLidar::DoorScanBoxes; ++B)
			{
				float Lo[3], Hi[3], T = 0.f, BN[3];
				BRLidar::DoorScanBox(B, Lo, Hi);
				if (BRLidar::RayBox(L.Frame, Lo, Hi, F3, D3, Reach, T, BN) && T < Reach)
				{
					Reach = T;
					LeafDevice = LD;
					LeafN[0] = BN[0];
					LeafN[1] = BN[1];
					LeafN[2] = BN[2];
				}
			}
		}
	}
	if (LeafDevice)
	{
		++Stats.LeafHits;
		bCtxLeaf = true;
		DeviceHit(LeafDevice, From + Dir * Reach, FVector(LeafN[0], LeafN[1], LeafN[2]), bPulse, Out);
		bCtxLeaf = false;
		return Out;
	}
	if (!bHit)
	{
		return Out;
	}
	AActor* A = Hit.GetActor();
	const FVector At = Hit.ImpactPoint;
	const FVector N = Hit.ImpactNormal;
	CtxComponent = Hit.GetComponent();
	if (Cast<ABREntity>(A))
	{
		// Menace : croix rouges, tournees vers le scanner
		AddPoint(BRLidar::Entity, At, -Dir, GlyphSize[BRLidar::Entity]);
		return Out;
	}
	if (ABRMissionDevice* D = Cast<ABRMissionDevice>(A))
	{
		DeviceHit(D, At, N, bPulse, Out);
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

void ABRLidar::DeviceHit(ABRMissionDevice* D, const FVector& At, const FVector& N, bool bPulse, BRLidar::FHit& Out)
{
	CtxDevice = D->GetIndex();
	AddPoint(BRLidar::Device, At, N, GlyphSize[BRLidar::Device]);
	CtxDevice = -1;
	if (bPulse)
	{
		return;
	}
	// Inscriptions : la cible est seulement signalee ; RunFrame la deduplique et la dessine sous le budget commun
	const BRM::FDevice& Def = D->GetDeviceDef();
	const bool bCage = Def.Role == BRM::R_LidarCage && D->GetGateValue() >= 128;
	if (!bCage && Def.Role != BRM::R_LidarDoor)
	{
		return;
	}
	const FGlyphCache* G = GetGlyphCache(D->GetIndex());
	if (!G)
	{
		return;
	}
	// v4.16 : un support qui bouge (battant qui pivote, tremblement d'un refus) ne se lit pas ; une fois arrete, sa nouvelle
	// revision de pose recommence la passe a la position actuelle
	if (G->Track.bMoving)
	{
		++Stats.GlyphMovingSkips;
		return;
	}
	Out.GlyphTarget = D->GetIndex();
	Out.GlyphTotal = bCage ? BRLidar::CageFigurePoints : BRLidar::PlateGlyphPoints(Def.Label);
	Out.GlyphPose = G->Track.Rev;
}

int ABRLidar::EmitGlyphs(int Target, int First, int Count)
{
	const double T0 = FPlatformTime::Seconds();
	const FGlyphCache* C = GetGlyphCache(Target);
	const ABRWorld* W = ABRWorld::Get(this);
	const ABRMissionDevice* Dev = W ? W->FindMissionDevice(Target) : nullptr;
	FTransform Support;
	int Added = 0;
	if (C && Dev && Dev->GetLidarSupport(Support))
	{
		// v4.16 : forme du repere du support posee a sa pose ACTUELLE (position, normale et orientation des glyphes)
		const BRLidar::FFrame3 SF = LidarFrameOf(Support);
		float LN[3], WN[3];
		LidarToFloats(C->LocalNormal, LN);
		BRLidar::DirToParent(SF, LN, WN);
		const FVector Normal(WN[0], WN[1], WN[2]);
		const float Size = GlyphSize[BRLidar::Glyph] * (C->bPlate ? 0.75f : 1.f);
		const int32 End = FMath::Min(First + Count, C->Local.Num());
		CtxDevice = Target;
		bCtxLeaf = false;
		CtxComponent.Reset();
		for (int32 K = FMath::Max(0, First); K < End; ++K)
		{
			float LP[3], WP[3];
			LidarToFloats(C->Local[K], LP);
			BRLidar::PointToParent(SF, LP, WP);
			AddPoint(BRLidar::Glyph, FVector(WP[0], WP[1], WP[2]), Normal, Size);
			++Added;
		}
		CtxDevice = -1;
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
	// Visee du scan focalise (un rayon par image, sans point) : mecanisme a relever, Smiler a repousser. v4.16 : memes
	// surfaces que les points (une porte du passage ouverte n'arrete plus le faisceau ; son battant l'arrete)
	++Stats.AimTraces;
	FHitResult Hit;
	const FVector Fwd = ViewRot.GetForwardVector();
	const FVector End = ViewOrigin + Fwd * BRLidar::Range;
	const bool bAimHit = World->LineTraceSingleByChannel(Hit, ViewOrigin, End, ECC_Visibility, ScanQuery);
	AActor* A = bAimHit ? Hit.GetActor() : nullptr;
	float LeafDist = 0.f;
	FVector LeafNormal = FVector::ZeroVector;
	if (ABRMissionDevice* LeafDev = TraceLeaves(ABRWorld::Get(this), ViewOrigin, Fwd, bAimHit ? static_cast<float>(FVector::Dist(ViewOrigin, Hit.ImpactPoint)) : BRLidar::Range,
		LeafDist, LeafNormal))
	{
		A = LeafDev;
	}
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
	// v4.16 : requete, battants et supports des inscriptions de cette image
	PrepareScan(ViewOrigin);
	BRLidar::FFrameResult R;
	const double Glyph0 = Stats.GlyphSeconds;
	const long long Replaced0 = State.Store.Replaced;
	const long long Expired0 = State.Store.Expired;
	const long long Touches0 = State.Glyphs.Touches;
	const long long Distinct0 = State.Glyphs.DistinctTouches;
	const long long Restarts0 = State.Glyphs.PoseRestarts;
	BRLidar::RunFrame(State, DeltaSeconds, Settings().Persistence, bFocus && bCan, *this, R);
	Stats.TraceSeconds += (FPlatformTime::Seconds() - TTrace) - (Stats.GlyphSeconds - Glyph0);
	Stats.Replaced += State.Store.Replaced - Replaced0;
	Stats.Expired += State.Store.Expired - Expired0;
	Stats.GlyphTouches += State.Glyphs.Touches - Touches0;
	Stats.GlyphDistinct += State.Glyphs.DistinctTouches - Distinct0;
	Stats.GlyphPoseRestarts += State.Glyphs.PoseRestarts - Restarts0;
	ApplyFrame(R);
	if (bFocus && bCan)
	{
		UpdateAim(DeltaSeconds);
	}
	UpdateFades(false);
	++Stats.Frames;
	Stats.TotalSeconds += FPlatformTime::Seconds() - T0;
}
