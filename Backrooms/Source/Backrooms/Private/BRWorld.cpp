#include "BRWorld.h"
#include "Backrooms.h"
#include "BRLevels.h"
#include "BRAssets.h"
#include "BRChunk.h"
#include "BREntity.h"
#include "BRCharacter.h"
#include "BRHUD.h"
#include "BRPlayerController.h"

#include "Algo/Reverse.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundBase.h"

namespace
{
	FORCEINLINE int32 Floor32(double V)
	{
		return static_cast<int32>(FMath::FloorToDouble(V));
	}

	TWeakObjectPtr<ABRWorld> GWorldInstance;
}

ABRWorld::ABRWorld()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(Root);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;

	AmbientAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbientAudio"));
	AmbientAudio->SetupAttachment(Root);
	AmbientAudio->bAutoActivate = false;

	HumAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("HumAudio"));
	HumAudio->SetupAttachment(Root);
	HumAudio->bAutoActivate = false;
}

ABRWorld* ABRWorld::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (GWorldInstance.IsValid() && GWorldInstance->GetWorld() == World)
	{
		return GWorldInstance.Get();
	}
	if (World)
	{
		for (TActorIterator<ABRWorld> It(World); It; ++It)
		{
			GWorldInstance = *It;
			return *It;
		}
	}
	return nullptr;
}

void ABRWorld::BeginPlay()
{
	Super::BeginPlay();
	GWorldInstance = this;

	int32 CmdLevel = StartLevel;
	if (FParse::Value(FCommandLine::Get(), TEXT("BRLevel="), CmdLevel) && BRLevels::Exists(CmdLevel))
	{
		StartLevel = CmdLevel;
	}

	LoadLevelNow(StartLevel);
	TransState = ETrans::FadingIn;
	TransTimer = 0.f;
	Fade = 1.f;
}

const FBRLevelDef& ABRWorld::Def() const
{
	return Current ? *Current : BRLevels::Get(0);
}

int32 ABRWorld::GetLevelNumber() const
{
	return Def().Number;
}

ABRCharacter* ABRWorld::GetPlayer() const
{
	return Cast<ABRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

// =====================================================================================
// Niveaux & transitions
// =====================================================================================

void ABRWorld::RequestTransition(int32 TargetLevel, bool bFromDeath)
{
	if (TransState == ETrans::FadingOut)
	{
		return;
	}
	if (TargetLevel < 0)
	{
		const TArray<FBRLevelDef>& All = BRLevels::All();
		TArray<int32> Choices;
		for (const FBRLevelDef& L : All)
		{
			if (L.Number != GetLevelNumber())
			{
				Choices.Add(L.Number);
			}
		}
		TargetLevel = Choices.Num() > 0 ? Choices[FMath::RandRange(0, Choices.Num() - 1)] : 0;
	}
	if (!BRLevels::Exists(TargetLevel))
	{
		TargetLevel = 0;
	}

	PendingLevel = TargetLevel;
	bPendingDeath = bFromDeath;
	TransState = ETrans::FadingOut;
	TransTimer = 0.f;
	DeathTimer = -1.f;

	if (UBRAssets* A = UBRAssets::Get(this))
	{
		if (USoundBase* S = A->Sound(bFromDeath ? FName(TEXT("S_Whisper")) : FName(TEXT("S_Noclip"))))
		{
			UGameplayStatics::PlaySound2D(this, S, 0.9f);
		}
	}
	if (ABRCharacter* P = GetPlayer())
	{
		P->SetInputLocked(true);
	}
}

void ABRWorld::HandlePlayerDeath()
{
	DeathTimer = 4.5f;
}

void ABRWorld::ClearLevel()
{
	for (TPair<FIntPoint, TObjectPtr<ABRChunk>>& Pair : Chunks)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}
	Chunks.Empty();

	TArray<TObjectPtr<ABREntity>> Copy = Entities;
	Entities.Empty();
	for (ABREntity* E : Copy)
	{
		if (IsValid(E))
		{
			E->Destroy();
		}
	}
}

void ABRWorld::LoadLevelNow(int32 LevelNumber)
{
	ClearLevel();
	Current = &BRLevels::Get(LevelNumber);
	Seed = static_cast<uint32>(FMath::Rand()) * 2654435761u ^ static_cast<uint32>(LevelNumber * 7919 + 17);
	Collected.Empty();
	LevelTime = 0.f;
	TitleTime = 7.f;
	SpawnTimer = 25.f;
	PhenomenaTimer = FMath::FRandRange(30.f, 60.f);
	Visited.AddUnique(Current->Number);

	UE_LOG(LogBackrooms, Log, TEXT("Chargement du Niveau %d - %s (graine %u)"), Current->Number, *Current->Title, Seed);

	ApplyEnvironment();
	UpdateStreaming(true);
	PlacePlayer();
}

void ABRWorld::ApplyEnvironment()
{
	const FBRLevelDef& D = Def();
	UBRAssets* A = UBRAssets::Get(this);

	// --- Brouillard ---
	Fog->SetFogDensity(D.FogDensity);
	Fog->SetFogHeightFalloff(D.FogFalloff);
	Fog->SetFogInscatteringColor(D.FogColor);
	Fog->SetStartDistance(D.FogStart);
	Fog->SetFogMaxOpacity(1.f);
	Fog->SetVolumetricFog(D.bVolumetricFog);

	// --- Ciel ---
	const bool bSky = D.bOutdoor && D.Sky != EBRSky::None;
	SkyAtmosphere->SetVisibility(bSky);
	Sun->SetVisibility(bSky);
	SkyLight->SetVisibility(bSky);
	if (bSky)
	{
		Sun->SetIntensity(D.SunLux);
		Sun->SetLightColor(D.SunColor);
		Sun->SetAtmosphereSunLight(true);
		Sun->SetCastShadows(true);
		Sun->SetWorldRotation(FRotator(D.SunPitch, 35.f, 0.f));
		SkyLight->SetIntensity(D.Sky == EBRSky::Night ? 0.6f : 1.f);
		SkyLight->RecaptureSky();
	}

	// --- Post-process du niveau ---
	FPostProcessSettings& S = PostProcess->Settings;
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
	S.bOverride_AutoExposureMinBrightness = true;
	S.AutoExposureMinBrightness = D.MinEV;
	S.bOverride_AutoExposureMaxBrightness = true;
	S.AutoExposureMaxBrightness = D.MaxEV;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = D.ExposureBias;
	S.bOverride_AutoExposureSpeedUp = true;
	S.AutoExposureSpeedUp = 2.f;
	S.bOverride_AutoExposureSpeedDown = true;
	S.AutoExposureSpeedDown = 1.f;
	S.bOverride_SceneColorTint = true;
	S.SceneColorTint = D.SceneTint;
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(D.Saturation, D.Saturation, D.Saturation, 1.f);
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(D.Contrast, D.Contrast, D.Contrast, 1.f);
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = D.Vignette;
	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = D.Grain;
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = D.Bloom;
	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.f;

	// --- Audio ---
	AmbientAudio->Stop();
	HumAudio->Stop();
	if (A)
	{
		AmbientAudio->SetSound(A->Sound(D.AmbientSound));
		AmbientAudio->SetVolumeMultiplier(D.AmbientVolume);
		if (AmbientAudio->Sound)
		{
			AmbientAudio->Play();
		}
		HumAudio->SetSound(D.HumVolume > 0.f ? A->Sound(D.HumSound) : nullptr);
		HumAudio->SetVolumeMultiplier(0.01f);
		if (HumAudio->Sound)
		{
			HumAudio->Play();
		}
	}
}

void ABRWorld::PlacePlayer()
{
	ABRCharacter* P = GetPlayer();
	if (!P)
	{
		bPlayerPlaced = false;
		return;
	}

	// Choisit une direction de depart degagee
	float Yaw = 0.f;
	const FIntPoint Start(0, 0);
	const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1) };
	const float Yaws[4] = { 0.f, 90.f, 180.f, 270.f };
	for (int32 i = 0; i < 4; ++i)
	{
		if (CanStep(Start, Start + Dirs[i]) && CanStep(Start + Dirs[i], Start + Dirs[i] * 2))
		{
			Yaw = Yaws[i];
			break;
		}
	}

	const float Half = P->GetCapsuleComponent() ? P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Loc = CellCenter(Start, Half + 5.f);
	P->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
	if (AController* C = P->GetController())
	{
		C->SetControlRotation(FRotator(0.f, Yaw, 0.f));
	}
	P->OnEnteredLevel(Def());
	bPlayerPlaced = true;
}

void ABRWorld::Discover(EBREntityKind Kind)
{
	const int32 K = static_cast<int32>(Kind);
	if (Discovered.Contains(K))
	{
		return;
	}
	Discovered.Add(K);
	const FBREntityInfo& Info = ABREntity::Info(Kind);
	ABRHUD::Notify(this, FString::Printf(TEXT("Nouvelle entr\u00e9e du journal : %s - %s  [TAB]"), *Info.Number, *Info.Name),
		6.f, FLinearColor(1.f, 0.4f, 0.35f));
}

// =====================================================================================
// Tick
// =====================================================================================

void ABRWorld::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	LevelTime += Dt;
	TitleTime = FMath::Max(0.f, TitleTime - Dt);

	if (!bPlayerPlaced)
	{
		PlacePlayer();
	}

	switch (TransState)
	{
	case ETrans::FadingOut:
		TransTimer += Dt;
		Fade = FMath::Clamp(TransTimer / 1.3f, 0.f, 1.f);
		Glitch = Fade;
		if (TransTimer >= 1.45f)
		{
			LoadLevelNow(PendingLevel);
			if (bPendingDeath)
			{
				if (ABRCharacter* P = GetPlayer())
				{
					P->ResetStats();
				}
			}
			TransState = ETrans::FadingIn;
			TransTimer = 0.f;
		}
		break;
	case ETrans::FadingIn:
		TransTimer += Dt;
		Fade = 1.f - FMath::Clamp(TransTimer / 1.6f, 0.f, 1.f);
		Glitch = Fade * 0.6f;
		if (TransTimer >= 1.6f)
		{
			TransState = ETrans::None;
			Fade = 0.f;
			Glitch = 0.f;
			if (ABRCharacter* P = GetPlayer())
			{
				P->SetInputLocked(false);
			}
		}
		break;
	default:
		break;
	}

	if (DeathTimer > 0.f)
	{
		DeathTimer -= Dt;
		if (DeathTimer <= 0.f)
		{
			DeathTimer = -1.f;
			RequestTransition(0, true);
		}
	}

	StreamTimer -= Dt;
	if (StreamTimer <= 0.f)
	{
		StreamTimer = 0.1f;
		UpdateStreaming(false);
	}

	if (TransState == ETrans::None)
	{
		UpdatePopulation(Dt);
		UpdatePhenomena(Dt);
	}
	UpdateAudio(Dt);
}

void ABRWorld::UpdateStreaming(bool bSynchronous)
{
	const FBRLevelDef& D = Def();
	const ABRCharacter* P = GetPlayer();
	const FVector Center = (P && bPlayerPlaced) ? P->GetActorLocation() : CellCenter(FIntPoint(0, 0));
	const float ChunkWorld = D.ChunkCells * D.CellSize;
	const int32 R = FMath::CeilToInt(D.ViewDistance / ChunkWorld);
	const FIntPoint PC = CellToChunk(WorldToCell(Center));

	auto ChunkDist = [&](const FIntPoint& C)
	{
		const FVector CC((C.X + 0.5f) * ChunkWorld, (C.Y + 0.5f) * ChunkWorld, Center.Z);
		return static_cast<float>(FVector::Dist2D(CC, Center));
	};

	TArray<FIntPoint> Wanted;
	for (int32 DX = -R; DX <= R; ++DX)
	{
		for (int32 DY = -R; DY <= R; ++DY)
		{
			const FIntPoint C(PC.X + DX, PC.Y + DY);
			if (!Chunks.Contains(C) && ChunkDist(C) <= D.ViewDistance + ChunkWorld * 0.75f)
			{
				Wanted.Add(C);
			}
		}
	}
	Wanted.Sort([&](const FIntPoint& A, const FIntPoint& B) { return ChunkDist(A) < ChunkDist(B); });

	int32 Budget = bSynchronous ? MAX_int32 : 2;
	for (const FIntPoint& C : Wanted)
	{
		if (Budget-- <= 0)
		{
			break;
		}
		SpawnChunk(C);
	}

	TArray<FIntPoint> ToRemove;
	for (const TPair<FIntPoint, TObjectPtr<ABRChunk>>& Pair : Chunks)
	{
		if (ChunkDist(Pair.Key) > D.ViewDistance + ChunkWorld * 1.6f)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FIntPoint& K : ToRemove)
	{
		if (ABRChunk* C = Chunks.FindRef(K))
		{
			C->Destroy();
		}
		Chunks.Remove(K);
	}
}

void ABRWorld::SpawnChunk(const FIntPoint& Coord)
{
	const FBRLevelDef& D = Def();
	const float ChunkWorld = D.ChunkCells * D.CellSize;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform T(FVector(Coord.X * ChunkWorld, Coord.Y * ChunkWorld, 0.f));
	ABRChunk* Chunk = GetWorld()->SpawnActor<ABRChunk>(ABRChunk::StaticClass(), T, Params);
	if (Chunk)
	{
		Chunk->Build(this, Coord);
		Chunks.Add(Coord, Chunk);
	}
}

// =====================================================================================
// Entites
// =====================================================================================

ABREntity* ABRWorld::SpawnEntity(EBREntityKind Kind, const FVector& Location)
{
	const FTransform T(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Location);
	ABREntity* E = GetWorld()->SpawnActorDeferred<ABREntity>(ABREntity::StaticClass(), T, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!E)
	{
		return nullptr;
	}
	E->Kind = Kind;
	E->FinishSpawning(T);
	Entities.Add(E);
	return E;
}

void ABRWorld::UnregisterEntity(ABREntity* Entity)
{
	Entities.Remove(Entity);
}

void ABRWorld::UpdatePopulation(float Dt)
{
	const FBRLevelDef& D = Def();
	ABRCharacter* P = GetPlayer();
	if (!P)
	{
		return;
	}
	const FVector PL = P->GetActorLocation();
	const float ChunkWorld = D.ChunkCells * D.CellSize;

	// Disparition des entites trop eloignees (ou dont le sol n'est plus charge)
	for (int32 i = Entities.Num() - 1; i >= 0; --i)
	{
		ABREntity* E = Entities[i];
		if (!IsValid(E))
		{
			Entities.RemoveAt(i);
			continue;
		}
		const bool bFar = FVector::Dist2D(E->GetActorLocation(), PL) > D.ViewDistance * 0.95f;
		const bool bNoFloor = !IsChunkLoaded(CellToChunk(WorldToCell(E->GetActorLocation())));
		if (bFar || bNoFloor)
		{
			Entities.RemoveAt(i);
			E->Destroy();
		}
	}

	if (D.MaxEntities <= 0 || D.Entities.Num() == 0 || Entities.Num() >= D.MaxEntities || P->IsDead())
	{
		return;
	}
	if (const ABRPlayerController* PC = Cast<ABRPlayerController>(P->GetController()))
	{
		if (PC->IsInMenu())
		{
			return;
		}
	}
	SpawnTimer -= Dt;
	if (SpawnTimer > 0.f)
	{
		return;
	}
	SpawnTimer = D.SpawnInterval * FMath::FRandRange(0.6f, 1.4f);

	// Choix pondere de l'espece
	float Total = 0.f;
	for (const FBREntitySpawn& S : D.Entities)
	{
		Total += S.Weight;
	}
	float Pick = FMath::FRandRange(0.f, Total);
	EBREntityKind Kind = D.Entities[0].Kind;
	for (const FBREntitySpawn& S : D.Entities)
	{
		Pick -= S.Weight;
		if (Pick <= 0.f)
		{
			Kind = S.Kind;
			break;
		}
	}

	const FBREntityInfo& Info = ABREntity::Info(Kind);
	const float MaxDist = FMath::Min(4200.f, D.ViewDistance - ChunkWorld * 0.5f);
	const FVector Eye = P->GetEyeLocation();
	for (int32 Attempt = 0; Attempt < 30; ++Attempt)
	{
		const float Ang = FMath::FRandRange(0.f, 2.f * PI);
		const float Dist = FMath::FRandRange(1600.f, FMath::Max(1700.f, MaxDist));
		const FVector Cand = PL + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * Dist;
		const FIntPoint Cell = WorldToCell(Cand);
		if (!IsWalkable(Cell) || IsSpawnArea(Cell.X, Cell.Y) || !IsChunkLoaded(CellToChunk(Cell)))
		{
			continue;
		}
		const float Z = Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 5.f;
		const FVector Loc = CellCenter(Cell, Z);
		if (Info.bNeedsDark && LightLevelAt(Loc) > 0.12f)
		{
			continue;
		}
		// Pas d'apparition sous les yeux du joueur
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRSpawnLOS), false, P);
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Loc, ECC_Visibility, Q);
		if (!bBlocked && Dist < 3000.f)
		{
			continue;
		}
		SpawnEntity(Kind, Loc);
		return;
	}
}

void ABRWorld::UpdatePhenomena(float Dt)
{
	const FBRLevelDef& D = Def();
	ABRCharacter* P = GetPlayer();
	UBRAssets* A = UBRAssets::Get(this);
	if (!P || !A || P->IsDead())
	{
		return;
	}

	PhenomenaTimer -= Dt;
	const bool bInsane = P->Sanity < 35.f;
	if (PhenomenaTimer > 0.f || (!D.bPhenomena && !bInsane))
	{
		return;
	}
	PhenomenaTimer = FMath::FRandRange(35.f, 90.f) * (bInsane ? 0.5f : 1.f);

	const float Ang = FMath::FRandRange(0.f, 2.f * PI);
	const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
	const int32 Roll = FMath::RandRange(0, 2);
	FName SoundName = TEXT("S_DistantSteps");
	float Dist = 1400.f;
	if (bInsane && Roll == 0)
	{
		SoundName = TEXT("S_Whisper");
		Dist = 300.f;
	}
	else if (Roll == 1)
	{
		SoundName = TEXT("S_Flicker");
		Dist = 700.f;
	}
	if (USoundBase* S = A->Sound(SoundName))
	{
		UGameplayStatics::PlaySoundAtLocation(this, S, P->GetActorLocation() + Dir * Dist + FVector(0, 0, 100.f), 1.f, 1.f, 0.f,
			A->Attenuation(4000.f));
	}
}

void ABRWorld::UpdateAudio(float Dt)
{
	const FBRLevelDef& D = Def();
	if (AmbientAudio->Sound && !AmbientAudio->IsPlaying())
	{
		AmbientAudio->Play();
	}
	if (HumAudio->Sound)
	{
		if (!HumAudio->IsPlaying())
		{
			HumAudio->Play();
		}
		const ABRCharacter* P = GetPlayer();
		const float Light = P ? LightLevelAt(P->GetActorLocation()) : 0.f;
		HumAudio->SetVolumeMultiplier(FMath::Max(0.01f, D.HumVolume * FMath::Clamp(Light, 0.f, 1.f) * (1.f - Fade)));
	}
	AmbientAudio->SetVolumeMultiplier(FMath::Max(0.01f, D.AmbientVolume * (1.f - Fade * 0.8f)));
}

// =====================================================================================
// Grille procedurale
// =====================================================================================

float ABRWorld::CellSize() const
{
	return Def().CellSize;
}

FIntPoint ABRWorld::WorldToCell(const FVector& P) const
{
	const double S = CellSize();
	return FIntPoint(Floor32(P.X / S), Floor32(P.Y / S));
}

FVector ABRWorld::CellCenter(const FIntPoint& C, float Z) const
{
	const float S = CellSize();
	return FVector((C.X + 0.5f) * S, (C.Y + 0.5f) * S, Z);
}

FIntPoint ABRWorld::CellToChunk(const FIntPoint& C) const
{
	const int32 N = Def().ChunkCells;
	return FIntPoint(BRHash::FloorDiv(C.X, N), BRHash::FloorDiv(C.Y, N));
}

bool ABRWorld::IsSpawnArea(int32 X, int32 Y) const
{
	return FMath::Abs(X) <= 1 && FMath::Abs(Y) <= 1;
}

float ABRWorld::ZoneDensity(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	const float R = BRHash::Rand(BRHash::FloorDiv(X, 6), BRHash::FloorDiv(Y, 6), 104, Seed);
	if (R < D.OpenZoneChance)
	{
		return 0.25f;
	}
	if (R > 0.85f)
	{
		return 1.35f;
	}
	return 1.f;
}

bool ABRWorld::MazeOpen(int32 X, int32 Y, bool bEast) const
{
	const FBRLevelDef& D = Def();
	const bool bCarveEast = BRHash::Rand(X, Y, 201, Seed) < 0.5f;
	if (bEast == bCarveEast)
	{
		return true;
	}
	if (BRHash::Rand(X, Y, bEast ? 202 : 203, Seed) < D.LoopChance)
	{
		return true;
	}
	// Petites salles ouvertes dans le labyrinthe
	const int32 ZX = BRHash::FloorDiv(X, 4);
	const int32 ZY = BRHash::FloorDiv(Y, 4);
	if (BRHash::Rand(ZX, ZY, 204, Seed) < D.OpenZoneChance)
	{
		return bEast ? (BRHash::PosMod(X, 4) != 3) : (BRHash::PosMod(Y, 4) != 3);
	}
	return false;
}

bool ABRWorld::HasHouse(int32 LotX, int32 LotY) const
{
	return BRHash::Rand(LotX, LotY, 801, Seed) < 0.85f;
}

float ABRWorld::BuildingHeight(int32 BlockX, int32 BlockY) const
{
	if (BRHash::Rand(BlockX, BlockY, 701, Seed) < 0.12f)
	{
		return 0.f; // place / parc
	}
	const float R = BRHash::Rand(BlockX, BlockY, 702, Seed);
	return 1500.f + R * R * 7000.f;
}

bool ABRWorld::IsSolid(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	switch (D.Layout)
	{
	case EBRLayout::Hotel:
	{
		if (IsSpawnArea(X, Y))
		{
			return false;
		}
		const bool bCorrX = BRHash::PosMod(X, D.Spacing) == 0 && BRHash::Rand(X, 0, 501, Seed) < 0.85f;
		const bool bCorrY = BRHash::PosMod(Y, D.Spacing) == 0 && BRHash::Rand(0, Y, 502, Seed) < 0.85f;
		if (bCorrX || bCorrY)
		{
			return false;
		}
		const int32 ZX = BRHash::FloorDiv(X, D.Spacing);
		const int32 ZY = BRHash::FloorDiv(Y, D.Spacing);
		return BRHash::Rand(ZX, ZY, 503, Seed) >= D.OpenZoneChance;
	}
	case EBRLayout::Caves:
	{
		if (FMath::Abs(X) <= 1 && FMath::Abs(Y) <= 1)
		{
			return false;
		}
		const float R = 0.55f * BRHash::Rand(X, Y, 601, Seed)
			+ 0.45f * BRHash::Rand(BRHash::FloorDiv(X, 3), BRHash::FloorDiv(Y, 3), 602, Seed);
		return R < D.SolidChance;
	}
	case EBRLayout::Suburbs:
	{
		const int32 Sp = D.Spacing;
		if (BRHash::PosMod(X, Sp) == 0 || BRHash::PosMod(Y, Sp) == 0)
		{
			return false;
		}
		const int32 LotX = BRHash::FloorDiv(X, Sp) * 2 + (BRHash::PosMod(X, Sp) - 1) / 2;
		const int32 LotY = BRHash::FloorDiv(Y, Sp) * 2 + (BRHash::PosMod(Y, Sp) - 1) / 2;
		return HasHouse(LotX, LotY);
	}
	case EBRLayout::City:
	{
		const int32 Sp = D.Spacing;
		if (BRHash::PosMod(X, Sp) == 0 || BRHash::PosMod(Y, Sp) == 0)
		{
			return false;
		}
		return BuildingHeight(BRHash::FloorDiv(X, Sp), BRHash::FloorDiv(Y, Sp)) > 0.f;
	}
	default:
		return false;
	}
}

EBREdge ABRWorld::EdgeE(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	if (IsSpawnArea(X, Y) && IsSpawnArea(X + 1, Y))
	{
		return EBREdge::Open;
	}
	if (D.Layout == EBRLayout::Rooms)
	{
		const int32 Seg = FMath::Max(1, D.SegmentLength);
		const int32 Off = static_cast<int32>(BRHash::Hash(X, 0, 101, Seed) % static_cast<uint32>(Seg));
		const int32 SegIdx = BRHash::FloorDiv(Y + Off, Seg);
		if (BRHash::Rand(X, SegIdx, 102, Seed) >= D.WallLineChance * ZoneDensity(X, Y))
		{
			return EBREdge::Open;
		}
		return BRHash::Rand(X, Y, 103, Seed) < D.DoorChance ? EBREdge::Door : EBREdge::Wall;
	}
	if (D.Layout == EBRLayout::Maze)
	{
		return MazeOpen(X, Y, true) ? EBREdge::Open : EBREdge::Wall;
	}
	return EBREdge::Open;
}

EBREdge ABRWorld::EdgeN(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	if (IsSpawnArea(X, Y) && IsSpawnArea(X, Y + 1))
	{
		return EBREdge::Open;
	}
	if (D.Layout == EBRLayout::Rooms)
	{
		const int32 Seg = FMath::Max(1, D.SegmentLength);
		const int32 Off = static_cast<int32>(BRHash::Hash(0, Y, 111, Seed) % static_cast<uint32>(Seg));
		const int32 SegIdx = BRHash::FloorDiv(X + Off, Seg);
		if (BRHash::Rand(SegIdx, Y, 112, Seed) >= D.WallLineChance * ZoneDensity(X, Y))
		{
			return EBREdge::Open;
		}
		return BRHash::Rand(X, Y, 113, Seed) < D.DoorChance ? EBREdge::Door : EBREdge::Wall;
	}
	if (D.Layout == EBRLayout::Maze)
	{
		return MazeOpen(X, Y, false) ? EBREdge::Open : EBREdge::Wall;
	}
	return EBREdge::Open;
}

bool ABRWorld::CanStep(const FIntPoint& From, const FIntPoint& To) const
{
	if (IsSolid(To.X, To.Y) || IsSolid(From.X, From.Y))
	{
		return false;
	}
	const int32 DX = To.X - From.X;
	const int32 DY = To.Y - From.Y;
	if (DX == 1 && DY == 0)
	{
		return EdgeE(From.X, From.Y) != EBREdge::Wall;
	}
	if (DX == -1 && DY == 0)
	{
		return EdgeE(To.X, To.Y) != EBREdge::Wall;
	}
	if (DX == 0 && DY == 1)
	{
		return EdgeN(From.X, From.Y) != EBREdge::Wall;
	}
	if (DX == 0 && DY == -1)
	{
		return EdgeN(To.X, To.Y) != EBREdge::Wall;
	}
	return false;
}

bool ABRWorld::HasPillar(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	if (D.Layout != EBRLayout::Rooms || D.PillarChance <= 0.f)
	{
		return false;
	}
	const float Mult = ZoneDensity(X, Y) < 0.5f ? 2.5f : 1.f;
	return BRHash::Rand(X, Y, 301, Seed) < D.PillarChance * Mult;
}

bool ABRWorld::IsDarkZone(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	if (D.Fixture == EBRFixture::None)
	{
		return !D.bOutdoor;
	}
	if (FMath::Abs(X) <= 3 && FMath::Abs(Y) <= 3)
	{
		return false;
	}
	return BRHash::Rand(BRHash::FloorDiv(X, 5), BRHash::FloorDiv(Y, 5), 401, Seed) < D.DarkZoneChance;
}

FBRLightInfo ABRWorld::CellLight(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	FBRLightInfo L;
	if (D.Fixture == EBRFixture::None || IsSolid(X, Y))
	{
		return L;
	}
	const bool bSpawn = IsSpawnArea(X, Y);

	auto Finish = [&](FBRLightInfo& Out)
	{
		Out.bHas = true;
		Out.bBroken = !bSpawn && BRHash::Rand(X, Y, 411, Seed) < D.BrokenChance;
		Out.bFlicker = !Out.bBroken && BRHash::Rand(X, Y, 412, Seed) < D.FlickerChance;
		Out.bShadow = BRHash::Rand(X, Y, 413, Seed) < D.ShadowChance;
	};

	switch (D.Layout)
	{
	case EBRLayout::Hotel:
	{
		// Appliques murales : uniquement contre un mur de chambre
		if (BRHash::PosMod(X + Y, 2) != 0 || (!bSpawn && BRHash::Rand(X, Y, 410, Seed) >= D.LightChance))
		{
			return L;
		}
		const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		const int32 Start = static_cast<int32>(BRHash::Hash(X, Y, 417, Seed) % 4u);
		for (int32 k = 0; k < 4; ++k)
		{
			const FIntPoint Dir = Dirs[(Start + k) % 4];
			if (IsSolid(X + Dir.X, Y + Dir.Y))
			{
				Finish(L);
				const float Half = D.CellSize * 0.5f;
				L.Offset = FVector(Dir.X * Half, Dir.Y * Half, 0.f);
				L.Yaw = FMath::RadiansToDegrees(FMath::Atan2(-static_cast<float>(Dir.Y), -static_cast<float>(Dir.X)));
				return L;
			}
		}
		return L;
	}
	case EBRLayout::Suburbs:
	case EBRLayout::City:
	{
		// Lampadaires le long des rues, sur le bas-cote
		const int32 Sp = D.Spacing;
		const bool bRoadX = BRHash::PosMod(X, Sp) == 0;
		const bool bRoadY = BRHash::PosMod(Y, Sp) == 0;
		if (bRoadX == bRoadY)
		{
			return L; // pas sur une rue, ou intersection
		}
		const int32 Along = bRoadX ? Y : X;
		if (BRHash::PosMod(Along, 2) != 0 || (!bSpawn && BRHash::Rand(X, Y, 410, Seed) >= D.LightChance))
		{
			return L;
		}
		Finish(L);
		const float Side = (BRHash::PosMod(Along, 4) == 0 ? 1.f : -1.f) * D.CellSize * 0.42f;
		L.Offset = bRoadX ? FVector(Side, 0.f, 0.f) : FVector(0.f, Side, 0.f);
		L.Yaw = FMath::RadiansToDegrees(FMath::Atan2(-static_cast<float>(L.Offset.Y), -static_cast<float>(L.Offset.X)));
		return L;
	}
	case EBRLayout::Open:
		return L;
	default:
	{
		if (!bSpawn && IsDarkZone(X, Y))
		{
			return L;
		}
		if (!bSpawn && BRHash::Rand(X, Y, 410, Seed) >= D.LightChance)
		{
			return L;
		}
		Finish(L);
		const float J = D.CellSize * 0.12f;
		L.Offset = FVector((BRHash::Rand(X, Y, 414, Seed) - 0.5f) * 2.f * J, (BRHash::Rand(X, Y, 415, Seed) - 0.5f) * 2.f * J, 0.f);
		L.Yaw = BRHash::Rand(X, Y, 416, Seed) < 0.5f ? 0.f : 90.f;
		return L;
	}
	}
}

float ABRWorld::LightLevelAt(const FVector& P) const
{
	const FBRLevelDef& D = Def();
	if (D.bOutdoor && (D.Sky == EBRSky::Day || D.Sky == EBRSky::Overcast))
	{
		return 1.f;
	}
	if (D.Fixture == EBRFixture::None)
	{
		return 0.f;
	}
	const FIntPoint C = WorldToCell(P);
	const int32 R = FMath::Clamp(FMath::CeilToInt(D.LightRadius / D.CellSize), 1, 4);
	float Acc = 0.f;
	for (int32 DX = -R; DX <= R; ++DX)
	{
		for (int32 DY = -R; DY <= R; ++DY)
		{
			const FBRLightInfo L = CellLight(C.X + DX, C.Y + DY);
			if (!L.bHas || L.bBroken)
			{
				continue;
			}
			const FVector LP = CellCenter(FIntPoint(C.X + DX, C.Y + DY)) + L.Offset;
			const float Dist = static_cast<float>(FVector::Dist2D(P, LP));
			if (Dist < D.LightRadius)
			{
				Acc += FMath::Square(1.f - Dist / D.LightRadius) * (L.bFlicker ? 0.6f : 1.f);
			}
		}
	}
	return FMath::Clamp(Acc, 0.f, 1.5f);
}

bool ABRWorld::FindPath(const FIntPoint& From, const FIntPoint& To, TArray<FIntPoint>& OutPath, int32 MaxNodes) const
{
	OutPath.Reset();
	if (From == To)
	{
		OutPath.Add(To);
		return true;
	}
	if (!IsWalkable(To))
	{
		return false;
	}

	struct FNode
	{
		FIntPoint P;
		float F;
	};
	auto Less = [](const FNode& A, const FNode& B) { return A.F < B.F; };
	auto Heur = [&To](const FIntPoint& P) { return static_cast<float>(FMath::Abs(P.X - To.X) + FMath::Abs(P.Y - To.Y)); };

	TArray<FNode> Open;
	TMap<FIntPoint, FIntPoint> Came;
	TMap<FIntPoint, float> G;
	Open.HeapPush(FNode{ From, Heur(From) }, Less);
	G.Add(From, 0.f);

	const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	int32 Expanded = 0;
	while (Open.Num() > 0 && Expanded < MaxNodes)
	{
		FNode Cur{ FIntPoint(0, 0), 0.f };
		Open.HeapPop(Cur, Less);
		if (Cur.P == To)
		{
			FIntPoint P = To;
			OutPath.Add(P);
			while (P != From)
			{
				P = Came.FindChecked(P);
				OutPath.Add(P);
			}
			Algo::Reverse(OutPath);
			return true;
		}
		++Expanded;
		const float CurG = G.FindRef(Cur.P);
		for (const FIntPoint& Dir : Dirs)
		{
			const FIntPoint N = Cur.P + Dir;
			if (!CanStep(Cur.P, N))
			{
				continue;
			}
			const float NG = CurG + 1.f;
			if (const float* Old = G.Find(N))
			{
				if (*Old <= NG)
				{
					continue;
				}
			}
			G.Add(N, NG);
			Came.Add(N, Cur.P);
			Open.HeapPush(FNode{ N, NG + Heur(N) }, Less);
		}
	}
	return false;
}
