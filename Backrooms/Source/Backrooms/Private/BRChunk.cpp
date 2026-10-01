#include "BRChunk.h"
#include "Backrooms.h"
#include "BRWorld.h"
#include "BRAssets.h"
#include "BRLevels.h"
#include "BRInteractables.h"
#include "BRItems.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

namespace
{
	const FIntPoint GDirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	float YawFromDir(float DX, float DY)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(DY, DX));
	}
}

ABRChunk::ABRChunk()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
}

void ABRChunk::EndPlay(const EEndPlayReason::Type Reason)
{
	for (AActor* A : Spawned)
	{
		if (IsValid(A))
		{
			A->Destroy();
		}
	}
	Spawned.Empty();
	Super::EndPlay(Reason);
}

ABRChunk::FBatch& ABRChunk::GetBatch(const FString& Key, UStaticMesh* Mesh, UMaterialInterface* Mat, bool bCollision, bool bShadow, float Cull)
{
	FBatch* Found = Batches.Find(Key);
	if (!Found)
	{
		FBatch B;
		B.Mesh = Mesh;
		B.Material = Mat;
		B.bCollision = bCollision;
		B.bShadow = bShadow;
		B.CullDistance = Cull;
		Found = &Batches.Add(Key, B);
	}
	return *Found;
}

void ABRChunk::AddBox(const FBRSurface& S, const FVector& Center, const FVector& Size, bool bCollision, float Yaw)
{
	UBRAssets* A = UBRAssets::Get(this);
	if (!A || !A->Cube())
	{
		return;
	}
	const FString Key = FString::Printf(TEXT("BOX|%d|%s"), bCollision ? 1 : 0, *S.Key());
	FBatch& B = GetBatch(Key, A->Cube(), A->Surface(S), bCollision, true, 0.f);
	B.Transforms.Add(FTransform(FRotator(0.f, Yaw, 0.f), Center - GetActorLocation(), Size / 100.f));
}

void ABRChunk::AddProp(FName MeshName, const FTransform& T, bool bCollision, const FVector& FallbackSize,
	const FBRSurface* FallbackSurface, float CullDistance, bool bShadow)
{
	UBRAssets* A = UBRAssets::Get(this);
	if (!A)
	{
		return;
	}
	const FTransform Local(T.GetRotation(), T.GetLocation() - GetActorLocation(), T.GetScale3D());
	if (UStaticMesh* M = A->Mesh(MeshName))
	{
		GetBatch(TEXT("PROP|") + MeshName.ToString(), M, nullptr, false, bShadow, CullDistance).Transforms.Add(Local);
		if (bCollision && A->Cube())
		{
			const FBox Bounds = M->GetBoundingBox();
			const FTransform ColT = FTransform(FQuat::Identity, Bounds.GetCenter(), Bounds.GetExtent() * 0.02f) * Local;
			FBatch& CB = GetBatch(TEXT("COLLISION"), A->Cube(), nullptr, true, false, 0.f);
			CB.bHidden = true;
			CB.Transforms.Add(ColT);
		}
	}
	else if (FallbackSize.X > 0.f && A->Cube())
	{
		const FBRSurface Surf = FallbackSurface ? *FallbackSurface : FBRSurface(TEXT("T_Grime"), FLinearColor(0.45f, 0.45f, 0.45f), 100.f);
		const FTransform BoxT = FTransform(FQuat::Identity, FVector(0.f, 0.f, FallbackSize.Z * 0.5f), FallbackSize / 100.f) * Local;
		GetBatch(FString::Printf(TEXT("FALLBACK|%d|%s"), bCollision ? 1 : 0, *Surf.Key()), A->Cube(), A->Surface(Surf), bCollision, bShadow, CullDistance)
			.Transforms.Add(BoxT);
	}
}

void ABRChunk::AddWaterPlane(const FVector& Center, const FVector2D& Size)
{
	ABRWorld* W = World.Get();
	UBRAssets* A = UBRAssets::Get(this);
	if (!W || !A || !A->Plane())
	{
		return;
	}
	FBRSurface WaterS = W->Def().Water;
	if (WaterS.Texture.IsNone())
	{
		WaterS = FBRSurface(TEXT("T_WaterNormal"), FLinearColor(0.3f, 0.45f, 0.5f), 200.f, 0.05f, 0.f);
	}
	// Grille subdivisee (vagues par World Position Offset) si le modele Blender est importe
	UStaticMesh* Grid = A->Mesh(TEXT("SM_WaterGrid"));
	FBatch& B = GetBatch(TEXT("WATER"), Grid ? Grid : A->Plane(), A->WaterMaterial(WaterS, W->Def().WaterAbsorption, W->Def().WaterScattering), false, false, 0.f);
	B.Transforms.Add(FTransform(FRotator::ZeroRotator, Center - GetActorLocation(), FVector(Size.X / 100.f, Size.Y / 100.f, 1.f)));
}

void ABRChunk::BuildPools()
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const int32 N = D.ChunkCells;
	const float S = D.CellSize;
	const float T = D.WallThickness;
	const float PD = D.PoolDepth;
	const int32 X0 = Coord.X * N;
	const int32 Y0 = Coord.Y * N;
	const uint32 Seed = W->GetSeed();
	const FBRSurface& PillarS = D.Pillar.Texture.IsNone() ? D.Wall : D.Pillar;
	constexpr float LampDepth = 3.f;
	int32 PoolLights = 0;

	auto EdgeTo = [W](int32 X, int32 Y, const FIntPoint& Dir) -> EBREdge
	{
		if (Dir.X != 0)
		{
			return W->EdgeE(Dir.X > 0 ? X : X - 1, Y);
		}
		return W->EdgeN(X, Dir.Y > 0 ? Y : Y - 1);
	};

	for (int32 X = X0; X < X0 + N; ++X)
	{
		for (int32 Y = Y0; Y < Y0 + N; ++Y)
		{
			const FVector C = W->CellCenter(FIntPoint(X, Y));
			if (!W->IsPoolCell(X, Y))
			{
				AddBox(D.Floor, FVector(C.X, C.Y, -10.f), FVector(S, S, 20.f));
				continue;
			}
			// Fond du bassin
			AddBox(D.Floor, FVector(C.X, C.Y, -PD - 10.f), FVector(S, S, 20.f));
			for (int32 k = 0; k < 4; ++k)
			{
				const FIntPoint Dir = GDirs[k];
				const bool bPoolNext = W->IsPoolCell(X + Dir.X, Y + Dir.Y);
				const EBREdge E = EdgeTo(X, Y, Dir);
				const FVector Face = C + FVector(Dir.X, Dir.Y, 0.f) * (S * 0.5f);
				const bool bAlongY = Dir.X != 0; // la paroi s'etend le long de Y
				if (E != EBREdge::Open)
				{
					// Le mur du dessus descend jusqu'au fond (pas de mur suspendu au-dessus de l'eau) ;
					// sous une porte, le seuil reste 1 cm sous le sol voisin
					const float Top = E == EBREdge::Door ? -1.f : 0.f;
					AddBox(D.Wall, FVector(Face.X, Face.Y, (Top - PD) * 0.5f), bAlongY ? FVector(T, S + T, Top + PD) : FVector(S + T, T, Top + PD));
				}
				else if (!bPoolNext)
				{
					// Paroi carrelee sous le sol voisin
					const FVector WallC = Face + FVector(Dir.X, Dir.Y, 0.f) * 10.f;
					AddBox(D.Wall, FVector(WallC.X, WallC.Y, (-PD - 20.f) * 0.5f), bAlongY ? FVector(20.f, S + 40.f, PD - 20.f) : FVector(S + 40.f, 20.f, PD - 20.f));
				}
			}
			// Projecteur immerge (lumiere turquoise qui fait vivre le fond), sur une paroi
			if (BRHash::Rand(X, Y, 1720, Seed) < 0.35f && PoolLights < 6)
			{
				const int32 Start = static_cast<int32>(BRHash::Hash(X, Y, 1721, Seed) % 4u);
				for (int32 k = 0; k < 4; ++k)
				{
					const FIntPoint Dir = GDirs[(Start + k) % 4];
					const EBREdge E = EdgeTo(X, Y, Dir);
					if (E == EBREdge::Open && W->IsPoolCell(X + Dir.X, Y + Dir.Y))
					{
						continue;
					}
					const float Inset = E != EBREdge::Open ? T * 0.5f : 0.f;
					const FVector Wall = C + FVector(Dir.X, Dir.Y, 0.f) * (S * 0.5f - Inset) + FVector(0.f, 0.f, -PD + 60.f);
					UPointLightComponent* PL = NewObject<UPointLightComponent>(this);
					PL->SetupAttachment(Root);
					PL->SetMobility(EComponentMobility::Movable);
					PL->SetRelativeLocation(Wall - FVector(Dir.X, Dir.Y, 0.f) * 30.f - GetActorLocation());
					PL->SetIntensityUnits(ELightUnits::Lumens);
					PL->SetIntensity(1400.f);
					PL->SetLightColor(FLinearColor(0.55f, 0.95f, 1.f));
					PL->SetAttenuationRadius(650.f);
					PL->SetSourceRadius(12.f);
					PL->SetCastShadows(false);
					PL->MaxDrawDistance = D.ViewDistance * 0.6f;
					PL->MaxDistanceFadeRange = 600.f;
					PL->RegisterComponent();
					Extra.Add(PL);
					++PoolLights;
					// Hublot lumineux encastre dans la paroi
					FBRSurface Lamp(TEXT("T_Grime"), FLinearColor(0.9f, 1.f, 1.f), 100.f, 0.2f, 0.f);
					Lamp.Emissive = FLinearColor(0.55f, 0.95f, 1.f) * 25.f;
					const FVector LampC = Wall - FVector(Dir.X, Dir.Y, 0.f) * (LampDepth * 0.5f - 1.f);
					AddBox(Lamp, LampC, Dir.X != 0 ? FVector(LampDepth, 46.f, 26.f) : FVector(46.f, LampDepth, 26.f), false);
					break;
				}
			}
		}
	}

	// Piliers prolonges jusqu'au fond des bassins
	for (int32 X = X0; X < X0 + N; ++X)
	{
		for (int32 Y = Y0; Y < Y0 + N; ++Y)
		{
			if (D.Layout != EBRLayout::Rooms || !W->HasPillar(X, Y))
			{
				continue;
			}
			if (W->IsPoolCell(X, Y) || W->IsPoolCell(X + 1, Y) || W->IsPoolCell(X, Y + 1) || W->IsPoolCell(X + 1, Y + 1))
			{
				AddBox(PillarS, FVector((X + 1) * S, (Y + 1) * S, -PD * 0.5f), FVector(D.PillarSize, D.PillarSize, PD));
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Murs
// ---------------------------------------------------------------------------------------------------------------------

void ABRChunk::AddWallSegment(bool bAlongY, float Fixed, float A, float B, float ZLo, float ZHi, bool bWithTrim, bool bWithPipes)
{
	ABRWorld* W = World.Get();
	if (!W || B - A < 1.f || ZHi - ZLo < 1.f)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const float T = D.WallThickness;
	const float Len = B - A;
	const float Mid = (A + B) * 0.5f;
	const FVector Center = bAlongY ? FVector(Fixed, Mid, (ZLo + ZHi) * 0.5f) : FVector(Mid, Fixed, (ZLo + ZHi) * 0.5f);
	const FVector Size = bAlongY ? FVector(T, Len, ZHi - ZLo) : FVector(Len, T, ZHi - ZLo);
	AddBox(D.Wall, Center, Size);

	if (bWithTrim && D.bTrim && ZLo < 1.f)
	{
		const FVector TC = bAlongY ? FVector(Fixed, Mid, 6.f) : FVector(Mid, Fixed, 6.f);
		const FVector TS = bAlongY ? FVector(T + 3.f, Len, 12.f) : FVector(Len, T + 3.f, 12.f);
		AddBox(D.Trim, TC, TS, false);
	}

	if (bWithTrim && ZLo < 1.f && D.WallDetailChance > 0.f)
	{
		AddWallDetails(bAlongY, Fixed, A, B);
	}

	if (bWithPipes && Len > 60.f)
	{
		const float Yaw = bAlongY ? 90.f : 0.f;
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const float Off1 = Side * (T * 0.5f + 14.f);
			const float Off2 = Side * (T * 0.5f + 9.f);
			const FVector P1 = bAlongY ? FVector(Fixed + Off1, Mid, D.WallHeight - 40.f) : FVector(Mid, Fixed + Off1, D.WallHeight - 40.f);
			const FVector P2 = bAlongY ? FVector(Fixed + Off2, Mid, D.WallHeight - 75.f) : FVector(Mid, Fixed + Off2, D.WallHeight - 75.f);
			AddProp(TEXT("SM_Pipe"), FTransform(FRotator(0.f, Yaw, 0.f), P1, FVector(Len / 100.f, 0.8f, 0.8f)), false, FVector::ZeroVector);
			AddProp(TEXT("SM_Pipe"), FTransform(FRotator(0.f, Yaw, 0.f), P2, FVector(Len / 100.f, 0.45f, 0.45f)), false, FVector::ZeroVector);
		}
	}
}

void ABRChunk::AddWallDetails(bool bAlongY, float Fixed, float A, float B)
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const float Len = B - A;
	if (Len < 90.f)
	{
		return;
	}
	const uint32 Seed = W->GetSeed();
	const int32 KF = FMath::RoundToInt(Fixed);
	const int32 KM = FMath::RoundToInt((A + B) * 0.5f);
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const int32 Salt = 990 + (bAlongY ? 0 : 4) + (Side > 0 ? 1 : 0);
		if (BRHash::Rand(KF, KM, Salt, Seed) >= D.WallDetailChance)
		{
			continue;
		}
		const bool bVent = BRHash::Rand(KF, KM, Salt + 20, Seed) < 0.3f;
		const float Along = A + 40.f + BRHash::Rand(KF, KM, Salt + 40, Seed) * (Len - 80.f);
		const float Off = Fixed + Side * (D.WallThickness * 0.5f);
		const float Z = bVent ? D.WallHeight - 55.f : 32.f;
		const FVector Pos = bAlongY ? FVector(Off, Along, Z) : FVector(Along, Off, Z);
		const float Yaw = bAlongY ? (Side > 0 ? 0.f : 180.f) : (Side > 0 ? 90.f : -90.f);
		AddProp(bVent ? FName(TEXT("SM_Vent")) : FName(TEXT("SM_Outlet")), FTransform(FRotator(0.f, Yaw, 0.f), Pos), false,
			FVector::ZeroVector, nullptr, 2500.f, false);
	}
}

void ABRChunk::AddDoorway(bool bAlongY, float Fixed, float Mid)
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const float S = D.CellSize;
	const float T = D.WallThickness;
	const float H = D.WallHeight;
	const float DW = FMath::Min(D.DoorWidth, S - 2.f * T - 20.f);
	const float Start = Mid - S * 0.5f - T * 0.5f;
	const float End = Mid + S * 0.5f + T * 0.5f;
	AddWallSegment(bAlongY, Fixed, Start, Mid - DW * 0.5f, 0.f, H, true, false);
	AddWallSegment(bAlongY, Fixed, Mid + DW * 0.5f, End, 0.f, H, true, false);
	if (D.bLintels)
	{
		const float DoorTop = FMath::Min(225.f, H - 25.f);
		if (H - DoorTop >= 15.f)
		{
			AddWallSegment(bAlongY, Fixed, Mid - DW * 0.5f, Mid + DW * 0.5f, DoorTop, H, false, false);
		}
	}
}

void ABRChunk::AddFaceProp(int32 X, int32 Y, const FIntPoint& Dir, FName Mesh, float Along, float Z, const FVector& FallbackSize, bool bCollision)
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const float Half = D.CellSize * 0.5f;
	const bool bWall = !W->IsSolid(X + Dir.X, Y + Dir.Y);
	const float Inset = bWall ? D.WallThickness * 0.5f : 0.f;
	const FVector Perp(-Dir.Y, Dir.X, 0.f);
	const FVector Pos = W->CellCenter(FIntPoint(X, Y), Z) + FVector(Dir.X, Dir.Y, 0.f) * (Half - Inset) + Perp * Along;
	const float Yaw = YawFromDir(-Dir.X, -Dir.Y);
	AddProp(Mesh, FTransform(FRotator(0.f, Yaw, 0.f), Pos), bCollision, FallbackSize);
}

// ---------------------------------------------------------------------------------------------------------------------
// Lumieres
// ---------------------------------------------------------------------------------------------------------------------

void ABRChunk::AddLight(int32 X, int32 Y, const FBRLightInfo& L)
{
	ABRWorld* W = World.Get();
	UBRAssets* A = UBRAssets::Get(this);
	if (!W || !A)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const float H = D.WallHeight;
	const FVector Base = W->CellCenter(FIntPoint(X, Y)) + L.Offset;
	const FRotator Rot(0.f, L.Yaw, 0.f);
	const FVector Fwd = Rot.Vector();

	FName MeshName = NAME_None;
	FVector MeshPos = Base;
	FVector LightPos = Base;
	FVector FallbackSize = FVector::ZeroVector;
	bool bWarm = false;
	float SourceLength = 0.f;

	switch (D.Fixture)
	{
	case EBRFixture::Panel:
		MeshName = TEXT("SM_LightPanel");
		MeshPos.Z = H;
		LightPos.Z = H - 25.f;
		FallbackSize = FVector(120.f, 60.f, 3.f);
		SourceLength = 100.f;
		break;
	case EBRFixture::SkyPanel:
		MeshName = TEXT("SM_SkyPanel");
		MeshPos.Z = H;
		LightPos.Z = H - 40.f;
		FallbackSize = FVector(100.f, 100.f, 3.f);
		break;
	case EBRFixture::Tube:
		MeshName = TEXT("SM_LightTube");
		MeshPos.Z = H;
		LightPos.Z = H - 70.f;
		FallbackSize = FVector(130.f, 16.f, 5.f);
		SourceLength = 110.f;
		break;
	case EBRFixture::Bulb:
		MeshName = TEXT("SM_LightBulb");
		MeshPos.Z = H;
		LightPos.Z = H - 25.f;
		FallbackSize = FVector(15.f, 15.f, 15.f);
		bWarm = true;
		break;
	case EBRFixture::Sconce:
		MeshName = TEXT("SM_Sconce");
		MeshPos.Z = 200.f;
		LightPos = Base + Fwd * 30.f;
		LightPos.Z = 210.f;
		FallbackSize = FVector(15.f, 15.f, 20.f);
		bWarm = true;
		break;
	case EBRFixture::StreetLamp:
		MeshName = TEXT("SM_StreetLamp");
		MeshPos.Z = 0.f;
		LightPos = Base + Fwd * 130.f;
		LightPos.Z = 560.f;
		FallbackSize = FVector(20.f, 20.f, 600.f);
		bWarm = true;
		break;
	default:
		return;
	}

	const FTransform Local(Rot, MeshPos - GetActorLocation());
	const float FallbackZ = (D.Fixture == EBRFixture::StreetLamp || D.Fixture == EBRFixture::Sconce) ? 0.f : -FallbackSize.Z;
	UStaticMesh* Mesh = A->Mesh(MeshName);

	if (L.bFlicker)
	{
		// Composant individuel pour pouvoir animer l'emissif
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		Comp->SetupAttachment(Root);
		Comp->SetMobility(EComponentMobility::Static);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FBRFlicker F;
		F.GlowColor = UBRAssets::GlowColorForSlot(bWarm ? TEXT("GlowWarm") : TEXT("Glow"));
		if (Mesh)
		{
			Comp->SetStaticMesh(Mesh);
			Comp->SetRelativeTransform(Local);
			TArray<UMaterialInstanceDynamic*> Glows;
			A->ApplySlots(Comp, nullptr, true, &Glows);
			for (UMaterialInstanceDynamic* G : Glows)
			{
				F.Glow.Add(G);
			}
		}
		else
		{
			Comp->SetStaticMesh(A->Cube());
			Comp->SetRelativeTransform(FTransform(FQuat::Identity, FVector(0.f, 0.f, FallbackZ + FallbackSize.Z * 0.5f), FallbackSize / 100.f) * Local);
			if (UMaterialInstanceDynamic* G = A->NewGlow(Comp, FLinearColor::White, 1.f))
			{
				Comp->SetMaterial(0, G);
				F.Glow.Add(G);
			}
		}
		Comp->RegisterComponent();
		Extra.Add(Comp);
		Flickers.Add(F);
	}
	else if (Mesh)
	{
		const FString Key = FString::Printf(TEXT("FIXTURE|%s|%d"), *MeshName.ToString(), L.bBroken ? 0 : 1);
		FBatch& B = GetBatch(Key, Mesh, nullptr, false, true, 0.f);
		B.GlowScale = L.bBroken ? 0.f : 1.f;
		B.bPowered = !L.bBroken;
		B.Transforms.Add(Local);
	}
	else
	{
		FBRSurface Fb(TEXT("T_Grime"), L.bBroken ? FLinearColor(0.3f, 0.3f, 0.3f) : FLinearColor::White, 100.f);
		if (!L.bBroken)
		{
			Fb.Emissive = UBRAssets::GlowColorForSlot(bWarm ? TEXT("GlowWarm") : TEXT("Glow"));
		}
		FBatch& B = GetBatch(TEXT("FIXTUREBOX|") + Fb.Key(), A->Cube(), A->Surface(Fb), false, false, 0.f);
		B.Transforms.Add(FTransform(FQuat::Identity, FVector(0.f, 0.f, FallbackZ + FallbackSize.Z * 0.5f), FallbackSize / 100.f) * Local);
	}

	if (L.bBroken)
	{
		return;
	}

	// Neons et dalles lumineuses : lumieres surfaciques (ombres douces, reflets realistes en ray tracing)
	const bool bArea = FBRSettings::Get().bAreaLights
		&& (D.Fixture == EBRFixture::Panel || D.Fixture == EBRFixture::SkyPanel || D.Fixture == EBRFixture::Tube);
	ULocalLightComponent* LC = nullptr;
	float Lumens = D.LightLumens;
	if (bArea)
	{
		URectLightComponent* RL = NewObject<URectLightComponent>(this);
		RL->SetupAttachment(Root);
		RL->SetMobility(EComponentMobility::Movable);
		FVector RectPos = LightPos;
		RectPos.Z = H - (D.Fixture == EBRFixture::Tube ? 12.f : 5.f);
		RL->SetRelativeLocation(RectPos - GetActorLocation());
		RL->SetRelativeRotation(FRotator(-90.f, L.Yaw, 0.f)); // eclaire vers le bas
		float Width = 55.f;
		float Length = 115.f;
		if (D.Fixture == EBRFixture::SkyPanel)
		{
			Width = Length = 92.f;
		}
		else if (D.Fixture == EBRFixture::Tube)
		{
			Width = 8.f;
			Length = 120.f;
		}
		RL->SetSourceWidth(Width);
		RL->SetSourceHeight(Length);
		RL->SetBarnDoorAngle(88.f);
		RL->SetBarnDoorLength(4.f);
		Lumens *= 0.55f; // tout le flux part vers le bas
		LC = RL;
	}
	else
	{
		UPointLightComponent* PL = NewObject<UPointLightComponent>(this);
		PL->SetupAttachment(Root);
		PL->SetMobility(EComponentMobility::Movable);
		PL->SetRelativeLocation(LightPos - GetActorLocation());
		PL->SetSourceRadius(D.Fixture == EBRFixture::SkyPanel ? 40.f : 8.f);
		PL->SetSourceLength(SourceLength);
		LC = PL;
	}
	LC->SetIntensityUnits(ELightUnits::Lumens);
	LC->SetIntensity(Lumens * Power);
	LC->SetLightColor(D.LightColor);
	LC->SetAttenuationRadius(D.LightRadius);
	LC->SetCastShadows(L.bShadow);
	LC->SetVolumetricScatteringIntensity(D.bVolumetricFog ? 1.f : 0.f);
	LC->MaxDrawDistance = D.ViewDistance * 0.75f;
	LC->MaxDistanceFadeRange = 800.f;
	LC->SetVisibility(Power > 0.01f);
	LC->RegisterComponent();
	Extra.Add(LC);
	++LightCount;

	if (!L.bFlicker)
	{
		PoweredLights.Add(LC);
		PoweredBase.Add(Lumens);
	}

	if (L.bFlicker && Flickers.Num() > 0)
	{
		Flickers.Last().Light = LC;
		Flickers.Last().BaseIntensity = Lumens;
		Flickers.Last().Timer = FMath::FRandRange(0.1f, 2.f);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Construction du chunk
// ---------------------------------------------------------------------------------------------------------------------

void ABRChunk::Build(ABRWorld* InWorld, const FIntPoint& InCoord)
{
	World = InWorld;
	Coord = InCoord;
	if (!InWorld)
	{
		return;
	}
	const FBRLevelDef& D = InWorld->Def();
	const int32 N = D.ChunkCells;
	const float S = D.CellSize;
	const float H = D.WallHeight;
	const float T = D.WallThickness;
	const int32 X0 = Coord.X * N;
	const int32 Y0 = Coord.Y * N;
	const float WX0 = X0 * S;
	const float WY0 = Y0 * S;
	const float ChunkW = N * S;
	const FVector Mid(WX0 + ChunkW * 0.5f, WY0 + ChunkW * 0.5f, 0.f);
	const uint32 Seed = InWorld->GetSeed();

	// ---- Sol & plafond ----
	if (D.PoolChance > 0.f)
	{
		BuildPools();
	}
	else
	{
		AddBox(D.Floor, FVector(Mid.X, Mid.Y, -10.f), FVector(ChunkW, ChunkW, 20.f));
	}
	if (D.bCeiling)
	{
		AddBox(D.Ceiling, FVector(Mid.X, Mid.Y, H + 10.f), FVector(ChunkW, ChunkW, 20.f));
	}
	if (D.bWater)
	{
		AddWaterPlane(FVector(Mid.X, Mid.Y, D.WaterHeight), FVector2D(ChunkW, ChunkW));
	}

	// ---- Murs (labyrinthes / salles) ----
	if (D.Layout == EBRLayout::Rooms || D.Layout == EBRLayout::Maze)
	{
		const bool bPipes = D.Props == EBRProps::Pipes;
		// Lignes verticales (X fixe) : aretes Est des cellules
		for (int32 X = X0; X < X0 + N; ++X)
		{
			const float Fixed = (X + 1) * S;
			int32 RunStart = MIN_int32;
			for (int32 Y = Y0; Y <= Y0 + N; ++Y)
			{
				const EBREdge E = (Y < Y0 + N) ? InWorld->EdgeE(X, Y) : EBREdge::Open;
				if (E == EBREdge::Wall)
				{
					if (RunStart == MIN_int32)
					{
						RunStart = Y;
					}
					continue;
				}
				if (RunStart != MIN_int32)
				{
					const bool bRunPipes = bPipes || (D.Props == EBRProps::Electrical && BRHash::Rand(X, RunStart, 960, Seed) < 0.35f);
					AddWallSegment(true, Fixed, RunStart * S - T * 0.5f, Y * S + T * 0.5f, 0.f, H, true, bRunPipes);
					RunStart = MIN_int32;
				}
				if (E == EBREdge::Door)
				{
					AddDoorway(true, Fixed, (Y + 0.5f) * S);
				}
			}
		}
		// Lignes horizontales (Y fixe) : aretes Nord
		for (int32 Y = Y0; Y < Y0 + N; ++Y)
		{
			const float Fixed = (Y + 1) * S;
			int32 RunStart = MIN_int32;
			for (int32 X = X0; X <= X0 + N; ++X)
			{
				const EBREdge E = (X < X0 + N) ? InWorld->EdgeN(X, Y) : EBREdge::Open;
				if (E == EBREdge::Wall)
				{
					if (RunStart == MIN_int32)
					{
						RunStart = X;
					}
					continue;
				}
				if (RunStart != MIN_int32)
				{
					const bool bRunPipes = bPipes || (D.Props == EBRProps::Electrical && BRHash::Rand(RunStart, Y, 961, Seed) < 0.35f);
					AddWallSegment(false, Fixed, RunStart * S - T * 0.5f, X * S + T * 0.5f, 0.f, H, true, bRunPipes);
					RunStart = MIN_int32;
				}
				if (E == EBREdge::Door)
				{
					AddDoorway(false, Fixed, (X + 0.5f) * S);
				}
			}
		}
		// Piliers aux coins
		for (int32 X = X0; X < X0 + N; ++X)
		{
			for (int32 Y = Y0; Y < Y0 + N; ++Y)
			{
				if (InWorld->HasPillar(X, Y))
				{
					AddBox(D.Pillar.Texture.IsNone() ? D.Wall : D.Pillar, FVector((X + 1) * S, (Y + 1) * S, H * 0.5f),
						FVector(D.PillarSize, D.PillarSize, H));
				}
			}
		}
	}

	// ---- Cellules pleines (hotel, grottes) ----
	if (D.Layout == EBRLayout::Hotel || D.Layout == EBRLayout::Caves)
	{
		const FBRSurface& SolidS = D.Solid.Texture.IsNone() ? D.Wall : D.Solid;
		for (int32 X = X0; X < X0 + N; ++X)
		{
			for (int32 Y = Y0; Y < Y0 + N; ++Y)
			{
				if (!InWorld->IsSolid(X, Y))
				{
					continue;
				}
				AddBox(SolidS, InWorld->CellCenter(FIntPoint(X, Y), H * 0.5f), FVector(S, S, H));
				for (int32 k = 0; k < 4; ++k)
				{
					const FIntPoint Dir = GDirs[k];
					if (InWorld->IsSolid(X + Dir.X, Y + Dir.Y))
					{
						continue;
					}
					const FVector Face = InWorld->CellCenter(FIntPoint(X, Y)) + FVector(Dir.X, Dir.Y, 0.f) * (S * 0.5f);
					if (D.bTrim && D.Layout == EBRLayout::Hotel)
					{
						const FVector TS = Dir.X != 0 ? FVector(6.f, S, 14.f) : FVector(S, 6.f, 14.f);
						AddBox(D.Trim, FVector(Face.X, Face.Y, 7.f), TS, false);
					}
					if (D.Layout == EBRLayout::Caves && BRHash::Rand(X * 4 + k, Y, 931, Seed) < 0.55f)
					{
						const float Sc = 1.f + 0.9f * BRHash::Rand(X, Y * 4 + k, 932, Seed);
						const FVector Perp(-Dir.Y, Dir.X, 0.f);
						const FVector P = Face + Perp * ((BRHash::Rand(X, Y, 933 + k, Seed) - 0.5f) * S * 0.6f) - FVector(Dir.X, Dir.Y, 0.f) * 30.f;
						AddProp(TEXT("SM_Rock"), FTransform(FRotator(0.f, BRHash::Rand(X, Y, 937 + k, Seed) * 360.f, 0.f), P, FVector(Sc, Sc, Sc * 1.4f)),
							true, FVector::ZeroVector);
					}
				}
			}
		}
	}

	// ---- Banlieue : routes et maisons ----
	if (D.Layout == EBRLayout::Suburbs)
	{
		for (int32 X = X0; X < X0 + N; ++X)
		{
			for (int32 Y = Y0; Y < Y0 + N; ++Y)
			{
				const bool bRoad = BRHash::PosMod(X, D.Spacing) == 0 || BRHash::PosMod(Y, D.Spacing) == 0;
				if (bRoad)
				{
					AddBox(D.Road, InWorld->CellCenter(FIntPoint(X, Y), 1.f), FVector(S, S, 2.f), false);
					continue;
				}
				const int32 LX = BRHash::PosMod(X, D.Spacing);
				const int32 LY = BRHash::PosMod(Y, D.Spacing);
				if ((LX == 1 || LX == 3) && (LY == 1 || LY == 3))
				{
					const int32 LotX = BRHash::FloorDiv(X, D.Spacing) * 2 + (LX - 1) / 2;
					const int32 LotY = BRHash::FloorDiv(Y, D.Spacing) * 2 + (LY - 1) / 2;
					if (InWorld->HasHouse(LotX, LotY))
					{
						const float Yaw = (LX == 1) ? 180.f : 0.f;
						AddProp(TEXT("SM_House"), FTransform(FRotator(0.f, Yaw, 0.f), FVector((X + 1) * S, (Y + 1) * S, 0.f)), true,
							FVector(800.f, 700.f, 450.f), &D.Wall);
					}
				}
			}
		}
	}

	// ---- Ville : immeubles ----
	if (D.Layout == EBRLayout::City)
	{
		const int32 Sp = D.Spacing;
		for (int32 X = X0; X < X0 + N; ++X)
		{
			for (int32 Y = Y0; Y < Y0 + N; ++Y)
			{
				if (BRHash::PosMod(X, Sp) != 1 || BRHash::PosMod(Y, Sp) != 1)
				{
					continue;
				}
				const int32 BX = BRHash::FloorDiv(X, Sp);
				const int32 BY = BRHash::FloorDiv(Y, Sp);
				const float BH = InWorld->BuildingHeight(BX, BY);
				const float Span = (Sp - 1) * S;
				const FVector C(X * S + Span * 0.5f, Y * S + Span * 0.5f, 0.f);
				if (BH > 0.f)
				{
					AddBox(D.Solid, FVector(C.X, C.Y, BH * 0.5f), FVector(Span - 10.f, Span - 10.f, BH));
					AddBox(D.Road, FVector(C.X, C.Y, 8.f), FVector(Span + 120.f, Span + 120.f, 16.f));
				}
				else
				{
					AddBox(D.Road, FVector(C.X, C.Y, 8.f), FVector(Span, Span, 16.f));
				}
			}
		}
	}

	// ---- Lumieres & accessoires par cellule ----
	for (int32 X = X0; X < X0 + N; ++X)
	{
		for (int32 Y = Y0; Y < Y0 + N; ++Y)
		{
			const FBRLightInfo L = InWorld->CellLight(X, Y);
			if (L.bHas)
			{
				AddLight(X, Y, L);
			}
			if (InWorld->IsWalkable(FIntPoint(X, Y)))
			{
				BuildCellProps(X, Y);
			}
		}
	}

	FinishBatches();
	BuildPickupsAndExits();
	SetActorTickEnabled(Flickers.Num() > 0);
}

void ABRChunk::BuildCellProps(int32 X, int32 Y)
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const uint32 Seed = W->GetSeed();
	const float S = D.CellSize;
	const bool bSpawn = W->IsSpawnArea(X, Y);
	const FVector C = W->CellCenter(FIntPoint(X, Y));
	auto R = [&](int32 Salt) { return BRHash::Rand(X, Y, Salt, Seed); };

	switch (D.Props)
	{
	case EBRProps::Warehouse:
	{
		if (!bSpawn && R(901) < D.PropDensity)
		{
			const int32 Count = 1 + static_cast<int32>(R(902) * 3.f);
			const FVector P = C + FVector((R(903) - 0.5f) * S * 0.5f, (R(904) - 0.5f) * S * 0.5f, 0.f);
			for (int32 i = 0; i < Count; ++i)
			{
				const float Yaw = R(905 + i) * 40.f - 20.f;
				const FVector Off = (i == 2) ? FVector(65.f, 0.f, 0.f) : FVector(0.f, 0.f, i * 40.f);
				AddProp(TEXT("SM_Crate"), FTransform(FRotator(0.f, Yaw, 0.f), P + Off), true, FVector(60.f, 45.f, 40.f));
			}
		}
		if (R(910) < 0.08f)
		{
			const FVector P = C + FVector((R(911) - 0.5f) * S * 0.6f, (R(912) - 0.5f) * S * 0.6f, 0.6f);
			AddWaterPlane(P, FVector2D(150.f + R(913) * 200.f, 120.f + R(914) * 150.f));
		}
		break;
	}
	case EBRProps::Electrical:
	{
		if (bSpawn || R(940) >= D.PropDensity)
		{
			break;
		}
		const int32 Start = static_cast<int32>(R(941) * 4.f);
		for (int32 k = 0; k < 4; ++k)
		{
			const FIntPoint Dir = GDirs[(Start + k) % 4];
			if (!W->CanStep(FIntPoint(X, Y), FIntPoint(X + Dir.X, Y + Dir.Y)))
			{
				AddFaceProp(X, Y, Dir, TEXT("SM_ElectricBox"), (R(942) - 0.5f) * S * 0.4f, 30.f, FVector(22.f, 55.f, 75.f), true);
				break;
			}
		}
		break;
	}
	case EBRProps::Office:
	{
		if (bSpawn)
		{
			break;
		}
		const float Roll = R(901);
		if (Roll < D.PropDensity)
		{
			const float Yaw = 90.f * FMath::FloorToFloat(R(902) * 4.f);
			const FRotator Rot(0.f, Yaw, 0.f);
			const FTransform DeskT(Rot, C);
			AddProp(TEXT("SM_Desk"), DeskT, true, FVector(160.f, 80.f, 75.f));
			const FVector ChairLocal(-20.f + R(903) * 30.f, -70.f, 0.f);
			AddProp(TEXT("SM_OfficeChair"), FTransform(FRotator(0.f, Yaw + 90.f + (R(904) - 0.5f) * 50.f, 0.f), C + Rot.RotateVector(ChairLocal)), true,
				FVector(50.f, 50.f, 100.f));
			AddProp(TEXT("SM_Partition"), FTransform(FRotator(0.f, Yaw + 90.f, 0.f), C + Rot.RotateVector(FVector(0.f, 55.f, 0.f))), true,
				FVector(5.f, 120.f, 140.f));
		}
		else if (Roll < D.PropDensity + 0.05f)
		{
			const FVector P = C + FVector(S * 0.5f - 45.f, S * 0.5f - 45.f, 0.f);
			AddProp(TEXT("SM_WaterCooler"), FTransform(FRotator(0.f, 225.f, 0.f), P), true, FVector(32.f, 32.f, 140.f));
		}
		break;
	}
	case EBRProps::Hotel:
	{
		for (int32 k = 0; k < 4; ++k)
		{
			const FIntPoint Dir = GDirs[k];
			if (W->IsSolid(X + Dir.X, Y + Dir.Y) && R(920 + k) < D.PropDensity)
			{
				const float Along = (R(925 + k) < 0.5f ? -1.f : 1.f) * S * 0.25f;
				AddFaceProp(X, Y, Dir, TEXT("SM_HotelDoor"), Along, 0.f, FVector(6.f, 92.f, 215.f), true);
			}
		}
		break;
	}
	case EBRProps::Caves:
	{
		if (!bSpawn && R(930) < D.PropDensity * 0.4f)
		{
			const float Sc = 0.35f + R(931) * 0.6f;
			const FVector P = C + FVector((R(932) - 0.5f) * S * 0.6f, (R(933) - 0.5f) * S * 0.6f, -10.f);
			AddProp(TEXT("SM_Rock"), FTransform(FRotator(0.f, R(934) * 360.f, 0.f), P, FVector(Sc)), true, FVector::ZeroVector);
		}
		break;
	}
	case EBRProps::Field:
	{
		if (Y != 0)
		{
			for (int32 i = 0; i < 6; ++i)
			{
				const FVector P(X * S + R(950 + i) * S, Y * S + R(960 + i) * S, 0.f);
				const float Sc = 0.8f + R(970 + i) * 0.45f;
				AddProp(TEXT("SM_Wheat"), FTransform(FRotator(0.f, R(980 + i) * 360.f, 0.f), P, FVector(Sc)), false, FVector::ZeroVector, nullptr,
					7000.f, false);
			}
		}
		if (Y == 1 && BRHash::PosMod(X, 6) == 0)
		{
			AddProp(TEXT("SM_PowerPole"), FTransform(FRotator(0.f, 90.f, 0.f), FVector(C.X, Y * S + S * 0.15f, 0.f)), true,
				FVector(26.f, 26.f, 900.f));
		}
		break;
	}
	default:
		break;
	}
}

void ABRChunk::BuildPickupsAndExits()
{
	ABRWorld* W = World.Get();
	if (!W || !GetWorld())
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const uint32 Seed = W->GetSeed();
	const int32 N = D.ChunkCells;
	const float S = D.CellSize;
	const int32 X0 = Coord.X * N;
	const int32 Y0 = Coord.Y * N;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	auto PickCell = [&](int32 Salt, FIntPoint& Out) -> bool
	{
		for (int32 Try = 0; Try < 8; ++Try)
		{
			const uint32 Hh = BRHash::Hash(Coord.X, Coord.Y, Salt * 31 + Try, Seed);
			const FIntPoint Cell(X0 + static_cast<int32>(Hh % static_cast<uint32>(N)), Y0 + static_cast<int32>((Hh >> 8) % static_cast<uint32>(N)));
			if (W->IsWalkable(Cell) && !W->IsPoolCell(Cell.X, Cell.Y))
			{
				Out = Cell;
				return true;
			}
		}
		return false;
	};

	// ---- Objets a ramasser ----
	struct FPickupRoll
	{
		EBRItem Item;
		float Chance;
		int32 Salt;
	};
	const float VHS = D.bRequireObjectives ? D.VHSChance : 0.f;
	const FPickupRoll Rolls[] = {
		{ EBRItem::AlmondWater, D.AlmondWaterChance, 1001 },
		{ EBRItem::AlmondWater, D.AlmondWaterChance * 0.4f, 1002 },
		{ EBRItem::Battery, D.BatteryChance, 1003 },
		{ EBRItem::Note, D.NoteChance, 1004 },
		{ EBRItem::Bandage, D.BandageChance, 1006 },
		{ EBRItem::EnergyBar, D.EnergyBarChance, 1007 },
		{ EBRItem::VHSTape, VHS, 1008 },
		{ EBRItem::VHSTape, VHS * 0.45f, 1009 },
		{ EBRItem::Flashlight, D.GearChance, 1010 },
		{ EBRItem::Headlamp, D.GearChance, 1011 },
		{ EBRItem::Vest, D.GearChance * 0.7f, 1012 },
	};
	for (const FPickupRoll& Roll : Rolls)
	{
		if (Roll.Chance <= 0.f || BRHash::Rand(Coord.X, Coord.Y, Roll.Salt, Seed) >= Roll.Chance)
		{
			continue;
		}
		FIntPoint Cell(0, 0);
		if (!PickCell(Roll.Salt, Cell))
		{
			continue;
		}
		if (Roll.Item == EBRItem::VHSTape && W->IsSpawnArea(Cell.X, Cell.Y))
		{
			continue;
		}
		const uint64 Id = (static_cast<uint64>(static_cast<uint32>(Coord.X)) << 40) ^ (static_cast<uint64>(static_cast<uint32>(Coord.Y)) << 16)
			^ static_cast<uint64>(Roll.Salt);
		if (W->IsCollected(Id))
		{
			continue;
		}
		const float JX = (BRHash::Rand(Cell.X, Cell.Y, Roll.Salt + 7, Seed) - 0.5f) * S * 0.5f;
		const float JY = (BRHash::Rand(Cell.X, Cell.Y, Roll.Salt + 8, Seed) - 0.5f) * S * 0.5f;
		const FVector Pos = W->CellCenter(Cell, 1.f) + FVector(JX, JY, 0.f);
		const FRotator Rot(0.f, BRHash::Rand(Cell.X, Cell.Y, Roll.Salt + 9, Seed) * 360.f, 0.f);
		ABRPickup* P = GetWorld()->SpawnActor<ABRPickup>(ABRPickup::StaticClass(), FTransform(Rot, Pos), Params);
		if (P)
		{
			FString Note;
			if (Roll.Item == EBRItem::Note)
			{
				const TArray<FString>& Common = BRLevels::CommonNotes();
				const int32 Total = D.Notes.Num() + Common.Num();
				const int32 Idx = Total > 0 ? static_cast<int32>(BRHash::Hash(Coord.X, Coord.Y, 1005, Seed) % static_cast<uint32>(Total)) : 0;
				Note = Idx < D.Notes.Num() ? D.Notes[Idx] : (Common.IsValidIndex(Idx - D.Notes.Num()) ? Common[Idx - D.Notes.Num()] : FString());
			}
			P->Init(Roll.Item, Id, Note);
			Spawned.Add(P);
		}
	}

	// ---- Sorties vers d'autres niveaux ----
	if (Coord == FIntPoint(0, 0))
	{
		return; // jamais de sortie juste a cote du point d'apparition
	}
	for (int32 i = 0; i < D.Exits.Num(); ++i)
	{
		const FBRExitDef& Ex = D.Exits[i];
		if (BRHash::Rand(Coord.X, Coord.Y, 1100 + i, Seed) >= Ex.ChancePerChunk)
		{
			continue;
		}

		FVector Pos = FVector::ZeroVector;
		float Yaw = 0.f;
		bool bFound = false;

		switch (Ex.Style)
		{
		case EBRExitStyle::NoclipFloor:
		case EBRExitStyle::Barn:
		{
			FIntPoint Cell(0, 0);
			if (PickCell(1200 + i, Cell) && !W->IsSpawnArea(Cell.X, Cell.Y))
			{
				Pos = W->CellCenter(Cell, 0.f);
				Yaw = 90.f * FMath::FloorToFloat(BRHash::Rand(Cell.X, Cell.Y, 1201, Seed) * 4.f);
				bFound = Ex.Style != EBRExitStyle::Barn || FMath::Abs(Cell.Y) > 2;
			}
			break;
		}
		case EBRExitStyle::HouseDoor:
		{
			for (int32 Try = 0; Try < 16 && !bFound; ++Try)
			{
				const uint32 Hh = BRHash::Hash(Coord.X, Coord.Y, 1300 + Try, Seed);
				const int32 X = X0 + static_cast<int32>(Hh % static_cast<uint32>(N));
				const int32 Y = Y0 + static_cast<int32>((Hh >> 8) % static_cast<uint32>(N));
				const int32 LX = BRHash::PosMod(X, D.Spacing);
				const int32 LY = BRHash::PosMod(Y, D.Spacing);
				if ((LX == 1 || LX == 3) && (LY == 1 || LY == 3))
				{
					const int32 LotX = BRHash::FloorDiv(X, D.Spacing) * 2 + (LX - 1) / 2;
					const int32 LotY = BRHash::FloorDiv(Y, D.Spacing) * 2 + (LY - 1) / 2;
					if (W->HasHouse(LotX, LotY))
					{
						const float Dir = (LX == 1) ? -1.f : 1.f;
						Pos = FVector((X + 1) * S + Dir * 405.f, (Y + 1) * S, 0.f);
						Yaw = (LX == 1) ? 180.f : 0.f;
						bFound = true;
					}
				}
			}
			break;
		}
		case EBRExitStyle::BuildingDoor:
		{
			const int32 Sp = D.Spacing;
			for (int32 X = X0; X < X0 + N && !bFound; ++X)
			{
				for (int32 Y = Y0; Y < Y0 + N && !bFound; ++Y)
				{
					if (BRHash::PosMod(X, Sp) == 1 && BRHash::PosMod(Y, Sp) == 1
						&& W->BuildingHeight(BRHash::FloorDiv(X, Sp), BRHash::FloorDiv(Y, Sp)) > 0.f)
					{
						const float Span = (Sp - 1) * S;
						const int32 Side = static_cast<int32>(BRHash::Hash(X, Y, 1400, Seed) % 4u);
						const FIntPoint Dir = GDirs[Side];
						const FVector C(X * S + Span * 0.5f, Y * S + Span * 0.5f, 16.f);
						Pos = C + FVector(Dir.X, Dir.Y, 0.f) * (Span * 0.5f - 5.f);
						Yaw = YawFromDir(Dir.X, Dir.Y);
						bFound = true;
					}
				}
			}
			break;
		}
		default:
		{
			// Sur une face de mur (mur fin ou cellule pleine)
			for (int32 Try = 0; Try < 24 && !bFound; ++Try)
			{
				const uint32 Hh = BRHash::Hash(Coord.X, Coord.Y, 1500 + i * 64 + Try, Seed);
				const FIntPoint Cell(X0 + static_cast<int32>(Hh % static_cast<uint32>(N)), Y0 + static_cast<int32>((Hh >> 8) % static_cast<uint32>(N)));
				if (!W->IsWalkable(Cell) || W->IsSpawnArea(Cell.X, Cell.Y) || W->IsPoolCell(Cell.X, Cell.Y))
				{
					continue;
				}
				const FIntPoint Dir = GDirs[(Hh >> 16) % 4u];
				const FIntPoint Next(Cell.X + Dir.X, Cell.Y + Dir.Y);
				bool bFace = false;
				if (W->IsSolid(Next.X, Next.Y))
				{
					bFace = true;
				}
				else if (Dir.X == 1)
				{
					bFace = W->EdgeE(Cell.X, Cell.Y) == EBREdge::Wall;
				}
				else if (Dir.X == -1)
				{
					bFace = W->EdgeE(Next.X, Next.Y) == EBREdge::Wall;
				}
				else if (Dir.Y == 1)
				{
					bFace = W->EdgeN(Cell.X, Cell.Y) == EBREdge::Wall;
				}
				else
				{
					bFace = W->EdgeN(Next.X, Next.Y) == EBREdge::Wall;
				}
				if (!bFace)
				{
					continue;
				}
				const float Inset = W->IsSolid(Next.X, Next.Y) ? 0.f : D.WallThickness * 0.5f;
				Pos = W->CellCenter(Cell, 0.f) + FVector(Dir.X, Dir.Y, 0.f) * (S * 0.5f - Inset);
				Yaw = YawFromDir(-Dir.X, -Dir.Y);
				bFound = true;
			}
			break;
		}
		}

		if (!bFound)
		{
			continue;
		}
		ABRExit* Exit = GetWorld()->SpawnActor<ABRExit>(ABRExit::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Pos), Params);
		if (Exit)
		{
			Exit->Init(Ex.Target, Ex.Style);
			Spawned.Add(Exit);
		}
	}
}

void ABRChunk::FinishBatches()
{
	UBRAssets* A = UBRAssets::Get(this);
	if (!A)
	{
		return;
	}
	for (TPair<FString, FBatch>& Pair : Batches)
	{
		FBatch& B = Pair.Value;
		if (!B.Mesh || B.Transforms.Num() == 0)
		{
			continue;
		}
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
		ISM->SetupAttachment(Root);
		ISM->SetMobility(EComponentMobility::Static);
		ISM->SetStaticMesh(B.Mesh);
		ISM->SetCastShadow(B.bShadow && !B.bHidden);
		if (B.bCollision)
		{
			ISM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			ISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		else
		{
			ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		if (B.bHidden)
		{
			ISM->SetVisibility(false);
			ISM->SetHiddenInGame(true);
		}
		if (B.CullDistance > 0.f)
		{
			ISM->SetCullDistances(FMath::RoundToInt(B.CullDistance * 0.8f), FMath::RoundToInt(B.CullDistance));
		}
		ISM->AddInstances(B.Transforms, false);
		if (!B.bHidden)
		{
			if (B.Material)
			{
				for (int32 i = 0; i < ISM->GetNumMaterials(); ++i)
				{
					ISM->SetMaterial(i, B.Material);
				}
			}
			else
			{
				A->ApplySlots(ISM, nullptr, false, nullptr, B.GlowScale, B.bPowered);
			}
		}
		ISM->RegisterComponent();
		Instances.Add(ISM);
	}
	Batches.Empty();
}

void ABRChunk::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UBRAssets* A = UBRAssets::Get(this);
	for (FBRFlicker& F : Flickers)
	{
		F.Timer -= DeltaSeconds;
		if (F.Timer <= 0.f)
		{
			F.bOn = !F.bOn;
			F.Timer = F.bOn ? FMath::FRandRange(0.05f, 2.5f) : FMath::FRandRange(0.03f, 0.4f);
			if (!F.bOn && F.Light && A && FMath::FRand() < 0.2f)
			{
				if (USoundBase* Snd = A->Sound(TEXT("S_Flicker")))
				{
					UGameplayStatics::PlaySoundAtLocation(this, Snd, F.Light->GetComponentLocation(), 0.5f, FMath::FRandRange(0.9f, 1.1f), 0.f,
						A->Attenuation(1500.f));
				}
			}
		}
		F.Phase += DeltaSeconds * 37.f;
		const float Mod = (F.bOn ? (0.85f + 0.15f * FMath::Sin(F.Phase)) : 0.03f) * Power;
		if (F.Light)
		{
			F.Light->SetIntensity(F.BaseIntensity * Mod);
		}
		for (UMaterialInstanceDynamic* G : F.Glow)
		{
			if (G)
			{
				G->SetVectorParameterValue(TEXT("Emissive"), F.GlowColor * Mod);
			}
		}
	}
}

void ABRChunk::SetPower(float InPower)
{
	Power = FMath::Clamp(InPower, 0.f, 1.f);
	for (int32 i = 0; i < PoweredLights.Num(); ++i)
	{
		if (ULocalLightComponent* L = PoweredLights[i])
		{
			L->SetIntensity(PoweredBase.IsValidIndex(i) ? PoweredBase[i] * Power : 0.f);
			L->SetVisibility(Power > 0.01f);
		}
	}
	for (FBRFlicker& F : Flickers)
	{
		if (F.Light)
		{
			F.Light->SetVisibility(Power > 0.01f);
		}
	}
}
