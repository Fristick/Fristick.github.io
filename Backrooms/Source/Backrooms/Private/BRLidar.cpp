// v4.13 : scanner LiDAR du Level 6 (voir BRLidar.h et BRLidarLogic.h).
#include "BRLidar.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BRConfig.h"
#include "BREntity.h"
#include "BRMission.h"
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

int64 ABRLidar::StatRays = 0;
int64 ABRLidar::StatPoints = 0;
double ABRLidar::StatSeconds = 0.0;

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

	/** Symboles des codes (meme ordre que BRMissionText::Symbol et ABRMissionDevice::AddSymbol) en grilles 7 x 7, ligne
	 *  du haut d'abord : triangle, cercle, carre, losange, croix, etoile a six branches */
	const char* const SymbolMask[BRM::NumSymbols] = {
		"...#.....#.#....#.#...#...#..#...#.#.....########",
		"..###...#...#.#.....##.....##.....#.#...#...###..",
		"########.....##.....##.....##.....##.....########",
		"...#.....#.#...#...#.#.....#.#...#...#.#.....#...",
		"...#......#......#...#######...#......#......#...",
		".#...#...#.#....#.#..#######..#.#....#.#...#...#." };

	bool SymbolCell(int32 Sym, int32 Row, int32 Col)
	{
		const char* M = SymbolMask[FMath::Clamp(Sym, 0, BRM::NumSymbols - 1)];
		const int32 I = Row * 7 + Col;
		return I >= 0 && I < 49 && M[I] == '#';
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
	if (const ABRWorld* W = ABRWorld::Get(this))
	{
		MarkShownAt.Init(-1, W->GetLidarMarks().Num());
	}
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
	Ring = BRLidar::FRing();
}

void ABRLidar::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HandMesh)
	{
		HandMesh->DestroyComponent();
		HandMesh = nullptr;
	}
	Super::EndPlay(Reason);
}

bool ABRLidar::Pulse()
{
	ABRCharacter* P = Holder.Get();
	if (!P || Cooldown > 0.f || P->IsDead())
	{
		return false;
	}
	Cooldown = BRLidar::PulseCooldown;
	PulseNext = 0;
	PulseOrigin = P->GetEyeLocation();
	PulseRot = P->GetAimRotation().Quaternion();
	PulsePhase = FMath::FRandRange(0.f, 6.2831f);
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
	if (bFocus == bOn)
	{
		return;
	}
	bFocus = bOn;
	FocusHold = 0.f;
	EntityHold = 0.f;
	FocusDevice.Reset();
	FocusEntity.Reset();
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

void ABRLidar::AddCageGlyphs(ABRMissionDevice* Dev, const FVector& Toward, int32 Count)
{
	FTransform F;
	if (!Dev || !Dev->GetCageFrame(F))
	{
		return;
	}
	const int32 Fig = FMath::Clamp<int32>(Dev->GetDeviceDef().Info[0], 0, BRLidarShapes::NumFigures - 1);
	for (int32 I = 0; I < Count; ++I)
	{
		const int32 K = (GlyphCursor + I * 7) % BRLidarShapes::PointsPerFigure;
		const short* Pt = BRLidarShapes::Points[Fig][K];
		// Silhouette debout dans la cage (1,8 m au plus : la cage mesure 2,1 m), face a l'avant de la cage
		const FVector Local(Pt[0] * 0.1f * 0.9f, Pt[1] * 0.1f * 0.9f, Pt[2] * 0.1f * 0.9f);
		AddPoint(BRLidar::Glyph, F.TransformPosition(Local), Toward, GlyphSize[BRLidar::Glyph]);
	}
	GlyphCursor = (GlyphCursor + Count * 7 + 1) % BRLidarShapes::PointsPerFigure;
}

void ABRLidar::AddPlateGlyphs(ABRMissionDevice* Dev, int32 Count)
{
	FVector C, R, U, N;
	if (!Dev || !Dev->GetLidarPlate(C, R, U, N))
	{
		return;
	}
	const BRM::FDevice& D = Dev->GetDeviceDef();
	// A gauche : la silhouette (vue de face, 40 cm de haut) ; a droite : le code (grille 7 x 7, 18 cm)
	const int32 Fig = FMath::Clamp<int32>(D.Info[0], 0, BRLidarShapes::NumFigures - 1);
	for (int32 I = 0; I < Count; ++I)
	{
		const int32 K = (GlyphCursor + I * 11) % BRLidarShapes::PointsPerFigure;
		const short* Pt = BRLidarShapes::Points[Fig][K];
		const float Y = Pt[1] * 0.1f * 0.22f;
		const float Z = Pt[2] * 0.1f * 0.22f;
		AddPoint(BRLidar::Glyph, C + R * (-12.f + Y) + U * (Z - 18.f), N, GlyphSize[BRLidar::Glyph] * 0.7f);
	}
	for (int32 Row = 0; Row < 7; ++Row)
	{
		for (int32 Col = 0; Col < 7; ++Col)
		{
			if (SymbolCell(D.Label, Row, Col))
			{
				AddPoint(BRLidar::Glyph, C + R * (12.f + (Col - 3) * 2.8f) + U * ((3 - Row) * 2.8f), N, GlyphSize[BRLidar::Glyph] * 0.8f);
			}
		}
	}
	GlyphCursor = (GlyphCursor + Count * 11 + 1) % BRLidarShapes::PointsPerFigure;
	// Releve local de la plaque (carnet) : rang de la porte -> silhouette, code
	if (const ABRWorld* W = ABRWorld::Get(this))
	{
		if (W->IsMissionActive())
		{
			const BRM::FPlan& Plan = W->GetMissionPlan();
			int32 Rank = 0;
			for (int32 I = 0; I <= Dev->GetIndex() && I < Plan.NumDevices; ++I)
			{
				Rank += Plan.Devices[I].Role == BRM::R_LidarDoor ? 1 : 0;
			}
			ScannedPlates.Add(Rank, FIntPoint(D.Info[0], D.Label));
		}
	}
}

void ABRLidar::TraceRay(const FVector& From, const FVector& Dir, bool bFocused, bool bCenter)
{
	UWorld* World = GetWorld();
	ABRWorld* W = ABRWorld::Get(this);
	ABRCharacter* P = Holder.Get();
	if (!World || !W || !P)
	{
		return;
	}
	++StatRays;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BRLidar), false, P);
	Q.AddIgnoredActor(this);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, From, From + Dir * BRLidar::Range, ECC_Visibility, Q))
	{
		return;
	}
	AActor* A = Hit.GetActor();
	const FVector At = Hit.ImpactPoint;
	const FVector N = Hit.ImpactNormal;
	if (ABREntity* E = Cast<ABREntity>(A))
	{
		// Menace : croix rouges, tournees vers le scanner
		AddPoint(BRLidar::Entity, At, -Dir, GlyphSize[BRLidar::Entity]);
		if (bFocused && bCenter)
		{
			FocusEntity = E;
		}
		return;
	}
	if (ABRMissionDevice* D = Cast<ABRMissionDevice>(A))
	{
		AddPoint(BRLidar::Device, At, N, GlyphSize[BRLidar::Device]);
		if (bFocused)
		{
			const uint8 DevRole = D->GetDeviceDef().Role;
			if (DevRole == BRM::R_LidarCage && D->GetGateValue() >= 128)
			{
				AddCageGlyphs(D, -Dir, 6);
			}
			else if (DevRole == BRM::R_LidarDoor)
			{
				AddPlateGlyphs(D, 4);
			}
			if (bCenter)
			{
				FocusDevice = D;
			}
		}
		return;
	}
	// Decor : danger (au fond ou au bord d'une fosse), repere au sol, ou surface ordinaire
	if (W->HasPits() && (At.Z < -40.f || (N.Z > 0.7f && W->IsOverPit(At, 35.f))))
	{
		AddPoint(BRLidar::Danger, At, N, GlyphSize[BRLidar::Danger]);
		return;
	}
	if (N.Z > 0.7f)
	{
		const TArray<FVector>& Marks = W->GetLidarMarks();
		const TArray<FVector>& Dirs = W->GetLidarMarkDirs();
		for (int32 I = 0; I < Marks.Num(); ++I)
		{
			if (FVector::DistSquared2D(Marks[I], At) < FMath::Square(70.f) && FMath::Abs(Marks[I].Z - At.Z) < 40.f)
			{
				// Un chevron par repere et par groupe d'age, pose sur le repere (pas a l'endroit touche)
				if (MarkShownAt.IsValidIndex(I) && MarkShownAt[I] != BucketSerial)
				{
					MarkShownAt[I] = BucketSerial;
					AddPoint(BRLidar::Marker, Marks[I] + FVector(0.f, 0.f, 1.f), FVector::UpVector, GlyphSize[BRLidar::Marker], &Dirs[I]);
				}
				break;
			}
		}
	}
	AddPoint(BRLidar::Env, At, N, GlyphSize[BRLidar::Env]);
}

void ABRLidar::Flush()
{
	if (Ism.Num() != BRLidar::NumCategories * BRLidar::AgeBuckets)
	{
		for (TArray<FTransform>& P : Pending)
		{
			P.Reset();
		}
		return;
	}
	for (int32 Cat = 0; Cat < BRLidar::NumCategories; ++Cat)
	{
		TArray<FTransform>& P = Pending[Cat];
		if (P.Num() == 0)
		{
			continue;
		}
		const int32 Got = Ring.Reserve(P.Num());
		if (Got < P.Num())
		{
			P.SetNum(Got);
		}
		if (Got > 0)
		{
			Ism[Cat * BRLidar::AgeBuckets + Ring.Current]->AddInstances(P, false, true);
			StatPoints += Got;
		}
		P.Reset();
	}
}

void ABRLidar::UpdateFades()
{
	const BRLidar::FSettings& S = Settings();
	for (int32 Cat = 0; Cat < BRLidar::NumCategories; ++Cat)
	{
		for (int32 B = 0; B < BRLidar::AgeBuckets; ++B)
		{
			const int32 I = Cat * BRLidar::AgeBuckets + B;
			if (!Mids.IsValidIndex(I) || !Mids[I])
			{
				continue;
			}
			const float Fade = Ring.FadeOf(B, S.Persistence);
			// Les dangers et les menaces restent plus vifs (lisibles meme en fin de vie)
			const float Keep = (Cat == BRLidar::Danger || Cat == BRLidar::Entity) ? 1.4f : 1.f;
			Mids[I]->SetVectorParameterValue(TEXT("Emissive"), GlyphColor[Cat] * GlowBase * S.Contrast * Keep * Fade);
		}
	}
}

void ABRLidar::UpdateFocusTargets(float Dt)
{
	ABRCharacter* P = Holder.Get();
	if (!P || !bFocus)
	{
		return;
	}
	// Mire et cages : le scan focalise maintenu releve le mecanisme (meme action que la main, verifiee par l'hote)
	ABRMissionDevice* D = FocusDevice.Get();
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
	// Smiler pris dans le faisceau : il recule (l'hote verifie distance, ligne de vue et visee)
	ABREntity* E = FocusEntity.Get();
	if (E && E->Kind == EBREntityKind::Smiler)
	{
		EntityHold += Dt;
		if (EntityHold >= BRLidar::RepelHold)
		{
			EntityHold = -BRLidar::RepelLock * 0.5f;
			P->RequestLidarRepel(E);
		}
	}
	else
	{
		EntityHold = FMath::Max(0.f, EntityHold - Dt);
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
	++Frame;
	Cooldown = FMath::Max(0.f, Cooldown - DeltaSeconds);
	if (HandMesh)
	{
		HandMesh->SetVisibility(!P->IsThirdPerson() && !P->IsDead());
	}
	// Groupes d'age : le plus ancien est vide puis reutilise
	Ring.BucketSeconds = Settings().Persistence / BRLidar::AgeBuckets;
	int32 Cleared = -1;
	if (Ring.Advance(DeltaSeconds, Cleared) && Cleared >= 0)
	{
		++BucketSerial;
		for (int32 Cat = 0; Cat < BRLidar::NumCategories; ++Cat)
		{
			const int32 I = Cat * BRLidar::AgeBuckets + Cleared;
			if (Ism.IsValidIndex(I) && Ism[I])
			{
				Ism[I]->ClearInstances();
			}
		}
	}
	if (MarkShownAt.Num() != W->GetLidarMarks().Num())
	{
		MarkShownAt.Init(-1, W->GetLidarMarks().Num());
	}
	// Impulsion : rayons repartis sur plusieurs images, depuis le repere fige au tir
	if (PulseNext >= 0)
	{
		const int32 End = FMath::Min(PulseNext + BRLidar::PulseRaysPerFrame, BRLidar::PulseRays);
		for (int32 I = PulseNext; I < End; ++I)
		{
			float X, Y, Z;
			BRLidar::PulseDir(I, BRLidar::PulseRays, PulsePhase, X, Y, Z);
			TraceRay(PulseOrigin, PulseRot.RotateVector(FVector(X, Y, Z)), false, false);
		}
		PulseNext = End >= BRLidar::PulseRays ? -1 : End;
	}
	// Scan focalise : cone etroit dans l'axe de la vue, a chaque image tant qu'il est maintenu
	if (bFocus && !P->IsDead())
	{
		FocusDevice.Reset();
		FocusEntity.Reset();
		const FVector Eye = P->GetEyeLocation();
		const FQuat Rot = P->GetAimRotation().Quaternion();
		TraceRay(Eye, Rot.GetForwardVector(), true, true);
		for (int32 I = 0; I < BRLidar::FocusRaysPerFrame; ++I)
		{
			float X, Y, Z;
			BRLidar::FocusDir(I, Frame, X, Y, Z);
			TraceRay(Eye, Rot.RotateVector(FVector(X, Y, Z)), true, false);
		}
		UpdateFocusTargets(DeltaSeconds);
	}
	Flush();
	UpdateFades();
	StatSeconds += FPlatformTime::Seconds() - T0;
}
