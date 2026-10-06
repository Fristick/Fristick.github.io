#include "BRChunk.h"
#include "HAL/PlatformTime.h"
#include "Backrooms.h"
#include "BRWorld.h"
#include "BRAssets.h"
#include "BRLevels.h"
#include "BRInteractables.h"
#include "BRItems.h"
#include "BRRig.h"

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

	// Poste de travail du Niveau 4 (RawAssets/Meshes/user_models.json, "Office") : origine du bureau au centre de son
	// emprise au sol, +X vers l'utilisateur ; la chaise et la fontaine sont placees comme dans la scene fournie
	constexpr float GDeskBack = 48.7f;
	constexpr float GDeskHalfLen = 93.1f;
	constexpr float GCoolerHalf = 23.f;
	const FVector GChairOffset(71.23f, -22.24f, 0.f);
	const FVector GCoolerOffset(-24.46f, -136.11f, 0.f);

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

bool ABRChunk::AddSurfaceMesh(FName MeshName, const FBRSurface& S, const FTransform& T, bool bCollision)
{
	UBRAssets* A = UBRAssets::Get(this);
	UStaticMesh* M = A ? A->Mesh(MeshName) : nullptr;
	if (!M)
	{
		return false;
	}
	const FString Key = FString::Printf(TEXT("SURF|%s|%d|%s"), *MeshName.ToString(), bCollision ? 1 : 0, *S.Key());
	FBatch& B = GetBatch(Key, M, A->Surface(S), bCollision, true, 0.f);
	B.Transforms.Add(FTransform(T.GetRotation(), T.GetLocation() - GetActorLocation(), T.GetScale3D()));
	return true;
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

void ABRChunk::AddWaterPlane(const FVector& Center, const FVector2D& Size, bool bCalm)
{
	ABRWorld* W = World.Get();
	UBRAssets* A = UBRAssets::Get(this);
	if (!W || !A || !A->Plane())
	{
		return;
	}
	const FBRSurface WaterS = W->GetWaterSurface();
	// Grille subdivisee (vagues par World Position Offset) si le modele Blender est importe
	UStaticMesh* Grid = A->Mesh(TEXT("SM_WaterGrid"));
	const FBRLevelDef& D = W->Def();
	UMaterialInterface* Mat = bCalm ? A->WaterMaterial(WaterS, D.WaterAbsorption, D.WaterScattering, 0.2f, 0.4f)
		: A->WaterMaterial(WaterS, D.WaterAbsorption, D.WaterScattering, D.WaterWaves, D.WaterChop);
	FBatch& B = GetBatch(bCalm ? TEXT("WATER|CALM") : TEXT("WATER"), Grid ? Grid : A->Plane(), Mat, false, false, 0.f);
	// v4.8 : l'eau translucide n'entre pas dans la scene ray tracee : les reflets des autres surfaces voient le fond du
	// bassin au lieu d'une surface noire (l'eau garde ses propres reflets, Lumen "front layer")
	B.bNoRayTracing = true;
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
					PL->SetVolumetricScatteringIntensity(0.2f); // projecteur sous l'eau : presque rien dans l'air
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

void ABRChunk::BuildDecks()
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const int32 N = D.ChunkCells;
	const float S = D.CellSize;
	const float DH = D.DeckHeight;
	const float StepW = D.DeckStep > 0.f ? D.DeckStepWidth : 0.f;
	// Le trottoir part de la ligne de la grille (cache dans le mur, et fait pont sous une porte)
	const float Depth = D.WallThickness * 0.5f + D.DeckWidth;
	for (int32 X = Coord.X * N; X < (Coord.X + 1) * N; ++X)
	{
		for (int32 Y = Coord.Y * N; Y < (Coord.Y + 1) * N; ++Y)
		{
			const FVector C = W->CellCenter(FIntPoint(X, Y));
			if (X == 0 && Y == 0)
			{
				// Estrade seche du point de depart, bordee d'une marche
				AddBox(D.Floor, FVector(C.X, C.Y, DH * 0.5f), FVector(S - 2.f * StepW, S - 2.f * StepW, DH));
				if (StepW > 0.f)
				{
					AddBox(D.Floor, FVector(C.X, C.Y, D.DeckStep * 0.5f), FVector(S, S, D.DeckStep));
				}
				continue;
			}
			for (int32 k = 0; k < 4; ++k)
			{
				if (!W->HasDeck(X, Y, k))
				{
					continue;
				}
				const FIntPoint Dir = GDirs[k];
				const bool bAlongY = Dir.X != 0;
				const FVector Out(Dir.X, Dir.Y, 0.f);
				const FVector Line = C + Out * (S * 0.5f);
				const FVector DeckC = Line - Out * (Depth * 0.5f) + FVector(0.f, 0.f, DH * 0.5f);
				AddBox(D.Floor, DeckC, bAlongY ? FVector(Depth, S, DH) : FVector(S, Depth, DH));
				if (StepW > 0.f)
				{
					const FVector StepC = Line - Out * (Depth + StepW * 0.5f) + FVector(0.f, 0.f, D.DeckStep * 0.5f);
					AddBox(D.Floor, StepC, bAlongY ? FVector(StepW, S, D.DeckStep) : FVector(S, StepW, D.DeckStep));
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// v4.6 : salle de fosses
// ---------------------------------------------------------------------------------------------------------------------

void ABRChunk::BuildPitRoom(const FIntRect& Room)
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const int32 N = D.ChunkCells;
	const float S = D.CellSize;
	const float WX0 = Coord.X * N * S;
	const float WY0 = Coord.Y * N * S;
	const float WX1 = WX0 + N * S;
	const float WY1 = WY0 + N * S;
	const float Half = W->GetPitHoleSize() * 0.5f;
	const float Lip = FMath::Max(10.f, D.PitLipThickness);
	const float Depth = FMath::Max(D.PitDepth, D.PitKillDepth + 300.f);
	constexpr float Wall = 30.f;  // parois du puits : assez epaisses pour les champs de distance de Lumen logiciel
	constexpr float Cut = 1.f;    // la dalle s'arrete 1 cm derriere les parois : aucune face confondue (scintillement)

	// Fosses aux coins interieurs des cellules de la salle
	TArray<FVector2D> Holes;
	for (int32 X = Room.Min.X; X < Room.Max.X - 1; ++X)
	{
		for (int32 Y = Room.Min.Y; Y < Room.Max.Y - 1; ++Y)
		{
			if (W->HasPitAtCorner(X, Y))
			{
				Holes.Add(FVector2D((X + 1) * S, (Y + 1) * S));
			}
		}
	}

	// ---- Dalle du chunk, epaisse (le bord des fosses montre son epaisseur), sans aucune collision au-dessus du vide :
	// grille des bords des ouvertures, cases pleines fusionnees en grands rectangles (bandes, puis bandes empilees)
	TArray<float> XS = { WX0, WX1 };
	TArray<float> YS = { WY0, WY1 };
	for (const FVector2D& H : Holes)
	{
		XS.Add(static_cast<float>(H.X) - Half - Cut);
		XS.Add(static_cast<float>(H.X) + Half + Cut);
		YS.Add(static_cast<float>(H.Y) - Half - Cut);
		YS.Add(static_cast<float>(H.Y) + Half + Cut);
	}
	auto SortUnique = [](TArray<float>& V)
	{
		V.Sort();
		TArray<float> Out;
		for (const float F : V)
		{
			if (Out.Num() == 0 || F - Out.Last() > 0.01f)
			{
				Out.Add(F);
			}
		}
		V = MoveTemp(Out);
	};
	SortUnique(XS);
	SortUnique(YS);
	const int32 NX = XS.Num() - 1;
	const int32 NY = YS.Num() - 1;
	TArray<uint8> Solid;
	Solid.SetNumZeroed(NX * NY);
	for (int32 j = 0; j < NY; ++j)
	{
		for (int32 i = 0; i < NX; ++i)
		{
			const float CX = (XS[i] + XS[i + 1]) * 0.5f;
			const float CY = (YS[j] + YS[j + 1]) * 0.5f;
			bool bHole = false;
			for (const FVector2D& H : Holes)
			{
				bHole |= FMath::Abs(CX - static_cast<float>(H.X)) < Half + Cut && FMath::Abs(CY - static_cast<float>(H.Y)) < Half + Cut;
			}
			Solid[j * NX + i] = bHole ? 0 : 1;
		}
	}
	TArray<uint8> Used;
	Used.SetNumZeroed(NX * NY);
	auto Free = [&](int32 i, int32 j) { return Solid[j * NX + i] != 0 && Used[j * NX + i] == 0; };
	for (int32 j = 0; j < NY; ++j)
	{
		for (int32 i = 0; i < NX; ++i)
		{
			if (!Free(i, j))
			{
				continue;
			}
			int32 I1 = i;
			while (I1 + 1 < NX && Free(I1 + 1, j))
			{
				++I1;
			}
			int32 J1 = j;
			bool bGrow = true;
			while (bGrow && J1 + 1 < NY)
			{
				for (int32 k = i; k <= I1 && bGrow; ++k)
				{
					bGrow = Free(k, J1 + 1);
				}
				J1 += bGrow ? 1 : 0;
			}
			for (int32 jj = j; jj <= J1; ++jj)
			{
				for (int32 ii = i; ii <= I1; ++ii)
				{
					Used[jj * NX + ii] = 1;
				}
			}
			const float AX = XS[i];
			const float BX = XS[I1 + 1];
			const float AY = YS[j];
			const float BY = YS[J1 + 1];
			AddBox(D.Floor, FVector((AX + BX) * 0.5f, (AY + BY) * 0.5f, -Lip * 0.5f), FVector(BX - AX, BY - AY, Lip));
		}
	}

	// ---- Parois des puits : moquette coupee, tranche de la dalle, puis beton de plus en plus sombre (la lumiere des
	// neons et de la lampe s'y accroche en haut, la profondeur se perd dans le noir), et un fond qui arrete la chute
	FBRSurface Fiber = D.Floor;
	Fiber.Tint = D.Floor.Tint * 0.42f;
	Fiber.Roughness = 1.f;
	Fiber.Stains = 0.f;
	Fiber.AntiTile = 0.f;
	const FBRSurface SlabEdge(TEXT("T_Concrete"), FLinearColor(0.58f, 0.56f, 0.5f), 120.f, 0.92f, 0.45f);
	const FBRSurface Upper(TEXT("T_Concrete"), FLinearColor(0.34f, 0.32f, 0.27f), 200.f, 0.9f, 0.7f);
	const FBRSurface MidS(TEXT("T_Concrete"), FLinearColor(0.15f, 0.14f, 0.12f), 260.f, 0.92f, 0.8f);
	const FBRSurface Deep(TEXT("T_Concrete"), FLinearColor(0.045f, 0.042f, 0.038f), 300.f, 0.95f, 0.8f);
	const FBRSurface Bottom(TEXT("T_Grime"), FLinearColor(0.012f, 0.011f, 0.01f), 200.f, 1.f, 0.f);
	struct FBand
	{
		float Top;
		float Bot;
		const FBRSurface* Surf;
	};
	const float Z1 = -Lip - 150.f;
	const float Z2 = FMath::Max(-650.f, -Depth * 0.5f);
	const FBand Bands[] = {
		{ -0.4f, -4.f, &Fiber },
		{ -4.f, -Lip, &SlabEdge },
		{ -Lip, Z1, &Upper },
		{ Z1, Z2, &MidS },
		{ Z2, -Depth, &Deep },
	};
	for (const FVector2D& H : Holes)
	{
		const float HX = static_cast<float>(H.X);
		const float HY = static_cast<float>(H.Y);
		for (const FBand& B : Bands)
		{
			if (B.Top - B.Bot < 0.5f)
			{
				continue;
			}
			const float ZC = (B.Top + B.Bot) * 0.5f;
			const float ZS = B.Top - B.Bot;
			// Parois +Y / -Y sur toute la largeur (elles couvrent les angles), +X / -X entre elles
			AddBox(*B.Surf, FVector(HX, HY + Half + Wall * 0.5f, ZC), FVector(2.f * (Half + Wall), Wall, ZS));
			AddBox(*B.Surf, FVector(HX, HY - Half - Wall * 0.5f, ZC), FVector(2.f * (Half + Wall), Wall, ZS));
			AddBox(*B.Surf, FVector(HX + Half + Wall * 0.5f, HY, ZC), FVector(Wall, 2.f * Half, ZS));
			AddBox(*B.Surf, FVector(HX - Half - Wall * 0.5f, HY, ZC), FVector(Wall, 2.f * Half, ZS));
		}
		AddBox(Bottom, FVector(HX, HY, -Depth - 10.f), FVector(2.f * (Half + Wall), 2.f * (Half + Wall), 20.f));
	}
}

bool ABRChunk::PickFreeCell(int32 Salt, FIntPoint& Out, bool bFullScan) const
{
	const ABRWorld* W = World.Get();
	if (!W)
	{
		return false;
	}
	const int32 N = W->Def().ChunkCells;
	const uint32 Seed = W->GetSeed();
	const int32 X0 = Coord.X * N;
	const int32 Y0 = Coord.Y * N;
	auto IsFree = [&](const FIntPoint& Cell)
	{
		// v4.11 : jamais sur une cellule reservee a un mecanisme ou a une sortie de mission
		return W->IsWalkable(Cell) && !W->IsPoolCell(Cell.X, Cell.Y) && !HidingCells.Contains(Cell) && !W->IsPitRoomCell(Cell.X, Cell.Y)
			&& W->IsSafelyReachable(Cell) && !W->IsMissionCell(Cell.X, Cell.Y);
	};
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const uint32 Hh = BRHash::Hash(Coord.X, Coord.Y, Salt * 31 + Try, Seed);
		const FIntPoint Cell(X0 + static_cast<int32>(Hh % static_cast<uint32>(N)), Y0 + static_cast<int32>((Hh >> 8) % static_cast<uint32>(N)));
		if (IsFree(Cell))
		{
			Out = Cell;
			return true;
		}
	}
	if (!bFullScan)
	{
		return false;
	}
	const int32 Count = N * N;
	const int32 Start = static_cast<int32>(BRHash::Hash(Coord.X, Coord.Y, Salt * 31 + 97, Seed) % static_cast<uint32>(Count));
	for (int32 k = 0; k < Count; ++k)
	{
		const int32 Idx = (Start + k) % Count;
		const FIntPoint Cell(X0 + Idx % N, Y0 + Idx / N);
		if (IsFree(Cell))
		{
			Out = Cell;
			return true;
		}
	}
	return false;
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

	// Corniche a 45 degres sous le plafond, des deux cotes (Poolrooms) : le modele a la piece vers son -Y
	if (D.CoveSize > 0.f && ZHi >= D.WallHeight - 1.f)
	{
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const float Face = Fixed + Side * T * 0.5f;
			const FVector P = bAlongY ? FVector(Face, Mid, D.WallHeight) : FVector(Mid, Face, D.WallHeight);
			const float Yaw = bAlongY ? Side * 90.f : (Side > 0 ? 180.f : 0.f);
			AddSurfaceMesh(TEXT("SM_Cove"), D.Ceiling, FTransform(FRotator(0.f, Yaw, 0.f), P, FVector(Len / 100.f, D.CoveSize / 50.f, D.CoveSize / 50.f)),
				false);
		}
	}

	if (bWithTrim && D.bGarage && ZLo < 1.f && ZHi > 150.f)
	{
		// Parking : bande de couleur a hauteur de pare-chocs, soubassement plus clair
		// v4.7 : peinture : plus lisse que le beton nu, le grain du beton ne transparait qu'un peu (RoughDetail)
		FBRSurface Stripe(TEXT("T_Concrete"), D.GarageStripe * 2.2f, 300.f, 0.42f, 0.3f);
		Stripe.RoughDetail = 0.3f;
		FBRSurface Lower(TEXT("T_Concrete"), FLinearColor(1.35f, 1.35f, 1.32f), 300.f, 0.6f, 0.45f);
		Lower.RoughDetail = 0.45f;
		AddBox(Lower, bAlongY ? FVector(Fixed, Mid, 50.f) : FVector(Mid, Fixed, 50.f), bAlongY ? FVector(T + 1.f, Len, 100.f) : FVector(Len, T + 1.f, 100.f), false);
		AddBox(Stripe, bAlongY ? FVector(Fixed, Mid, 112.f) : FVector(Mid, Fixed, 112.f), bAlongY ? FVector(T + 1.6f, Len, 24.f) : FVector(Len, T + 1.6f, 24.f),
			false);
	}

	if (bWithTrim && D.bTrim && ZLo < 1.f)
	{
		// v4.5 : plinthe mouluree des deux cotes (sabot, gorge, quart-de-rond : SM_Baseboard) ; a defaut, une boite
		bool bMolded = true;
		for (int32 Side = -1; Side <= 1 && bMolded; Side += 2)
		{
			const float Face = Fixed + Side * T * 0.5f;
			const FVector P = bAlongY ? FVector(Face, Mid, 0.f) : FVector(Mid, Face, 0.f);
			const float Yaw = bAlongY ? Side * 90.f : (Side > 0 ? 180.f : 0.f);
			bMolded = AddSurfaceMesh(TEXT("SM_Baseboard"), D.Trim, FTransform(FRotator(0.f, Yaw, 0.f), P, FVector(Len / 100.f, 1.f, 1.f)), false);
		}
		if (!bMolded)
		{
			const FVector TC = bAlongY ? FVector(Fixed, Mid, 6.f) : FVector(Mid, Fixed, 6.f);
			const FVector TS = bAlongY ? FVector(T + 3.f, Len, 12.f) : FVector(Len, T + 3.f, 12.f);
			AddBox(D.Trim, TC, TS, false);
		}
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
	if (D.bLintels && D.bArches)
	{
		// Arche en plein cintre (Poolrooms) : ecoincons etires a la largeur de la porte, linteau au-dessus
		const float Apex = D.ArchApex > 0.f ? FMath::Min(D.ArchApex, H - 20.f) : H - 60.f;
		const float Spring = Apex - DW * 0.5f;
		const float Top = Spring + DW * 0.52f;
		const FVector P = bAlongY ? FVector(Fixed, Mid, Spring) : FVector(Mid, Fixed, Spring);
		if (Spring > 150.f && Top < H
			&& AddSurfaceMesh(TEXT("SM_ArchSpandrel"), D.Wall, FTransform(FRotator(0.f, bAlongY ? 90.f : 0.f, 0.f), P, FVector(DW / 100.f, T / 100.f, DW / 100.f)),
				false))
		{
			AddWallSegment(bAlongY, Fixed, Mid - DW * 0.5f, Mid + DW * 0.5f, Top, H, false, false);
			return;
		}
	}
	if (D.bLintels)
	{
		const float DoorTop = FMath::Min(225.f, H - 25.f);
		if (H - DoorTop >= 15.f)
		{
			AddWallSegment(bAlongY, Fixed, Mid - DW * 0.5f, Mid + DW * 0.5f, DoorTop, H, false, false);
		}
		if (D.bDoorCasings)
		{
			// v4.6 : chambranle sur les deux faces : deux jambages et une traverse en legere saillie (sans collision)
			constexpr float CW = 9.f;
			constexpr float CD = 2.5f;
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				const float Face = Fixed + Side * (T * 0.5f + CD * 0.5f);
				for (int32 J = -1; J <= 1; J += 2)
				{
					const float Along = Mid + J * (DW * 0.5f + CW * 0.5f);
					AddBox(D.Trim, bAlongY ? FVector(Face, Along, DoorTop * 0.5f) : FVector(Along, Face, DoorTop * 0.5f),
						bAlongY ? FVector(CD, CW, DoorTop) : FVector(CW, CD, DoorTop), false);
				}
				AddBox(D.Trim, bAlongY ? FVector(Face, Mid, DoorTop + CW * 0.5f) : FVector(Mid, Face, DoorTop + CW * 0.5f),
					bAlongY ? FVector(CD, DW + 2.f * CW, CW) : FVector(DW + 2.f * CW, CD, CW), false);
			}
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
	if (bDeferLights)
	{
		// v4.7 : construction etalee : la lumiere (et son luminaire) sera creee a l'etape des lumieres
		FPendingLight P;
		P.X = X;
		P.Y = Y;
		P.L = L;
		PendingLights.Add(P);
		return;
	}
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
		// Plafonnier ovale des Poolrooms
		MeshName = TEXT("SM_SkyPanel");
		MeshPos.Z = H;
		LightPos.Z = H - 40.f;
		FallbackSize = FVector(183.f, 68.f, 3.f);
		SourceLength = 110.f;
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
	// Un luminaire ne projette pas d'ombre : sa monture, collee a la source, dessinait un grand disque noir au plafond.
	// Seuls les lampadaires (poteau de 6 m, eclaires par les autres lampes) gardent la leur.
	const bool bFixtureShadow = D.Fixture == EBRFixture::StreetLamp;

	// Composant individuel pour pouvoir animer l'emissif : neon qui clignote, ou lampe qui peut virer au rouge
	const bool bIndividual = (L.bFlicker || D.RedLightRadius > 0.f) && !L.bBroken;
	if (bIndividual)
	{
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
		Comp->SetupAttachment(Root);
		Comp->SetMobility(EComponentMobility::Static);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCastShadow(bFixtureShadow);
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
		F.bFlickers = L.bFlicker;
		F.LightColor = D.LightColor;
		Flickers.Add(F);
	}
	else if (Mesh)
	{
		const FString Key = FString::Printf(TEXT("FIXTURE|%s|%d"), *MeshName.ToString(), L.bBroken ? 0 : 1);
		FBatch& B = GetBatch(Key, Mesh, nullptr, false, bFixtureShadow, 0.f);
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

	// v4.8 : la lumiere est decrite (FLightRecord) puis creee : elle peut etre recreee d'un autre type quand le reglage
	// change, et son ombre coupee au loin
	FLightRecord R;
	R.Pos = LightPos;
	R.Yaw = L.Yaw;
	R.Fixture = D.Fixture;
	R.SourceLength = SourceLength;
	R.bShadow = L.bShadow;
	R.bShadowOn = L.bShadow && W->ShouldCastLocalShadow(LightPos);
	if (bIndividual && Flickers.Num() > 0)
	{
		R.FlickerIndex = Flickers.Num() - 1;
	}
	// Neons et dalles lumineuses : lumieres surfaciques (ombres douces, reflets realistes en ray tracing)
	ULocalLightComponent* LC = CreateLightComponent(R, FBRSettings::Get().bAreaLights && IsAreaFixture(D.Fixture));
	++LightCount;
	if (!bIndividual)
	{
		R.PoweredIndex = PoweredLights.Num();
		PoweredLights.Add(LC);
		PoweredBase.Add(R.Lumens);
	}
	else if (R.FlickerIndex != INDEX_NONE)
	{
		FBRFlicker& F = Flickers[R.FlickerIndex];
		F.Light = LC;
		F.BaseIntensity = R.Lumens;
		F.Timer = FMath::FRandRange(0.1f, 2.f);
	}
	LightRecords.Add(R);
}

bool ABRChunk::IsAreaFixture(EBRFixture Fixture)
{
	return Fixture == EBRFixture::Panel || Fixture == EBRFixture::SkyPanel || Fixture == EBRFixture::Tube;
}

ULocalLightComponent* ABRChunk::CreateLightComponent(FLightRecord& R, bool bArea)
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return nullptr;
	}
	const FBRLevelDef& D = W->Def();
	const float H = D.WallHeight;
	ULocalLightComponent* LC = nullptr;
	float Lumens = D.LightLumens;
	if (bArea)
	{
		URectLightComponent* RL = NewObject<URectLightComponent>(this);
		RL->SetupAttachment(Root);
		RL->SetMobility(EComponentMobility::Movable);
		FVector RectPos = R.Pos;
		RectPos.Z = H - (R.Fixture == EBRFixture::Tube ? 12.f : 5.f);
		RL->SetRelativeLocation(RectPos - GetActorLocation());
		RL->SetRelativeRotation(FRotator(-90.f, R.Yaw, 0.f)); // eclaire vers le bas
		float Width = 55.f;
		float Length = 115.f;
		if (R.Fixture == EBRFixture::SkyPanel)
		{
			Width = 55.f;
			Length = 150.f;
		}
		else if (R.Fixture == EBRFixture::Tube)
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
		PL->SetRelativeLocation(R.Pos - GetActorLocation());
		PL->SetSourceRadius(R.Fixture == EBRFixture::SkyPanel ? 40.f : 8.f);
		PL->SetSourceLength(R.SourceLength);
		LC = PL;
	}
	LC->SetIntensityUnits(ELightUnits::Lumens);
	LC->SetIntensity(Lumens * Power);
	LC->SetLightColor(D.LightColor);
	LC->SetAttenuationRadius(D.LightRadius);
	LC->SetCastShadows(R.bShadow && R.bShadowOn);
	LC->SetVolumetricScatteringIntensity(D.bVolumetricFog ? D.VolumetricScatter : 0.f);
	LC->MaxDrawDistance = D.ViewDistance * 0.75f;
	LC->MaxDistanceFadeRange = 800.f;
	LC->SetVisibility(Power > 0.01f);
	LC->RegisterComponent();
	Extra.Add(LC);
	R.Comp = LC;
	R.bArea = bArea;
	R.Lumens = Lumens;
	return LC;
}

bool ABRChunk::LightTypesMatch(bool bArea) const
{
	for (const FLightRecord& R : LightRecords)
	{
		if (IsAreaFixture(R.Fixture) && R.bArea != bArea)
		{
			return false;
		}
	}
	return true;
}

int32 ABRChunk::RefreshLightTypes(bool bArea, double BudgetMs)
{
	// v4.9 : etale : les lumieres deja du bon type sont sautees (curseur implicite), et l'appel s'arrete quand le budget
	// est depense (au moins une lumiere par appel) ; le chunk reste dans la file tant que LightTypesMatch est faux
	const double Start = FPlatformTime::Seconds();
	int32 Count = 0;
	for (FLightRecord& R : LightRecords)
	{
		if (!IsAreaFixture(R.Fixture) || R.bArea == bArea)
		{
			continue;
		}
		if (Count > 0 && (FPlatformTime::Seconds() - Start) * 1000.0 >= BudgetMs)
		{
			break;
		}
		ULocalLightComponent* Old = R.Comp.Get();
		ULocalLightComponent* New = CreateLightComponent(R, bArea);
		if (R.PoweredIndex != INDEX_NONE && PoweredLights.IsValidIndex(R.PoweredIndex))
		{
			PoweredLights[R.PoweredIndex] = New;
			PoweredBase[R.PoweredIndex] = R.Lumens;
		}
		else if (Flickers.IsValidIndex(R.FlickerIndex))
		{
			Flickers[R.FlickerIndex].Light = New;
			Flickers[R.FlickerIndex].BaseIntensity = R.Lumens;
			Flickers[R.FlickerIndex].AppliedMod = -1.f; // intensite et couleur reappliquees a la prochaine image
		}
		if (Old)
		{
			Extra.Remove(Old);
			Old->DestroyComponent();
		}
		++Count;
	}
	return Count;
}

int32 ABRChunk::UpdateShadowLOD(const TArray<FVector>& Viewers, float ShadowDistance)
{
	int32 Shadowed = 0;
	for (FLightRecord& R : LightRecords)
	{
		if (!R.bShadow)
		{
			continue;
		}
		float Best = 1e20f;
		for (const FVector& V : Viewers)
		{
			Best = FMath::Min(Best, static_cast<float>(FVector::Dist(V, R.Pos)));
		}
		// Marge de 10 % : une lumiere a la limite ne bascule pas a chaque pas
		const bool bWant = Best < ShadowDistance * (R.bShadowOn ? 1.1f : 1.f);
		if (bWant != R.bShadowOn)
		{
			R.bShadowOn = bWant;
			if (ULocalLightComponent* LC = R.Comp.Get())
			{
				LC->SetCastShadows(bWant);
			}
		}
		Shadowed += R.bShadowOn ? 1 : 0;
	}
	return Shadowed;
}

// ---------------------------------------------------------------------------------------------------------------------
// Construction du chunk
// ---------------------------------------------------------------------------------------------------------------------

void ABRChunk::Build(ABRWorld* InWorld, const FIntPoint& InCoord)
{
	BeginBuild(InWorld, InCoord);
	StepBuild(1.0e9);
}

void ABRChunk::BeginBuild(ABRWorld* InWorld, const FIntPoint& InCoord)
{
	World = InWorld;
	Coord = InCoord;
	bReady = false;
	bCollisionReady = false;
	bPlanned = false;
	BuildPhase = 0;
	PlanStage = 0;
	PlanCursor = 0;
	ActorCursor = 0;
	Stats = FBuildStats();
	if (!InWorld)
	{
		bReady = bCollisionReady = bPlanned = true;
		return;
	}
	// v4.8 : rien n'est planifie ici (la planification d'un chunk riche prenait plusieurs millisecondes d'un bloc) :
	// StepBuild la fait par colonnes de cellules, dans le budget de l'image. Les lumieres sont mises en attente.
	bDeferLights = true;
	// v4.6 : salle de fosses reservee avant tout le reste (murs, cachettes, sorties, objets et accessoires l'evitent)
	bHasPitRoom = InWorld->GetPitRoom(Coord, PitRoom);
}

bool ABRChunk::StepPlan()
{
	ABRWorld* InWorld = World.Get();
	if (!InWorld)
	{
		bPlanned = true;
		bDeferLights = false;
		return true;
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
	const bool bWalls = D.Layout == EBRLayout::Rooms || D.Layout == EBRLayout::Maze;
	// Une unite par appel : une etape courte, ou une colonne de cellules d'une etape par colonnes (meme ordre qu'en v4.7 :
	// le resultat ne depend pas du decoupage)
	auto NextColumn = [this, N]()
	{
		if (++PlanCursor >= N)
		{
			PlanCursor = 0;
			++PlanStage;
		}
	};
	switch (PlanStage)
	{
	case 0:
	{
		// ---- Sol & plafond ----
		if (D.PoolChance > 0.f)
		{
			BuildPools();
		}
		else if (bHasPitRoom)
		{
			BuildPitRoom(PitRoom);
		}
		else
		{
			AddBox(D.Floor, FVector(Mid.X, Mid.Y, -10.f), FVector(ChunkW, ChunkW, 20.f));
		}
		if (D.DeckHeight > 0.f)
		{
			BuildDecks();
		}
		if (D.bWater)
		{
			AddWaterPlane(FVector(Mid.X, Mid.Y, D.WaterHeight), FVector2D(ChunkW, ChunkW));
		}
		++PlanStage;
		break;
	}
	case 1:
	{
		// ---- Murs (labyrinthes / salles) ----
		if (!bWalls)
		{
			PlanStage = 4;
			break;
		}
		const bool bPipes = D.Props == EBRProps::Pipes;
		// Lignes verticales (X fixe) : aretes Est des cellules
		const int32 X = X0 + PlanCursor;
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
		NextColumn();
		break;
	}
	case 2:
	{
		const bool bPipes = D.Props == EBRProps::Pipes;
		// Lignes horizontales (Y fixe) : aretes Nord
		const int32 Y = Y0 + PlanCursor;
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
		NextColumn();
		break;
	}
	case 3:
	{
		if (PlanCursor == 0)
		{
			// Niveau fini (v4.3) : murs d'enceinte a l'ouest et au sud (a l'est et au nord, ce sont des aretes de la grille)
			if (D.BoundsChunks > 0)
			{
				if (Coord.X == -D.BoundsChunks)
				{
					AddWallSegment(true, X0 * S, Y0 * S - T * 0.5f, (Y0 + N) * S + T * 0.5f, 0.f, H, true, false);
				}
				if (Coord.Y == -D.BoundsChunks)
				{
					AddWallSegment(false, Y0 * S, X0 * S - T * 0.5f, (X0 + N) * S + T * 0.5f, 0.f, H, true, false);
				}
			}
		}
		// Piliers aux coins
		const int32 X = X0 + PlanCursor;
		for (int32 Y = Y0; Y < Y0 + N; ++Y)
		{
			if (InWorld->HasPillar(X, Y))
			{
				AddBox(D.Pillar.Texture.IsNone() ? D.Wall : D.Pillar, FVector((X + 1) * S, (Y + 1) * S, H * 0.5f),
					FVector(D.PillarSize, D.PillarSize, H));
				if (D.bGarage)
				{
					// Bande jaune et noire au pied du pilier, lisere blanc au-dessus (pare-chocs)
					static const FBRSurface Hazard(TEXT("T_Hazard"), FLinearColor::White, 80.f, 0.55f, 0.25f);
					AddBox(Hazard, FVector((X + 1) * S, (Y + 1) * S, 55.f), FVector(D.PillarSize + 3.f, D.PillarSize + 3.f, 110.f), false);
					FBRSurface Band(TEXT("T_Concrete"), FLinearColor(1.65f, 1.65f, 1.6f), 300.f, 0.45f, 0.2f);
					Band.RoughDetail = 0.3f;
					AddBox(Band, FVector((X + 1) * S, (Y + 1) * S, 128.f), FVector(D.PillarSize + 2.f, D.PillarSize + 2.f, 22.f), false);
				}
			}
		}
		NextColumn();
		break;
	}
	case 4:
		if (D.bGarage)
		{
			BuildGarage();
		}
		PlanStage = (D.Layout == EBRLayout::Hotel || D.Layout == EBRLayout::Caves) ? 5 : 6;
		break;
	case 5:
	{
		// ---- Cellules pleines (hotel, grottes) ----
		if (D.Layout == EBRLayout::Hotel || D.Layout == EBRLayout::Caves)
		{
			const FBRSurface& SolidS = D.Solid.Texture.IsNone() ? D.Wall : D.Solid;
			const int32 X = X0 + PlanCursor;
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
		NextColumn();
		break;
	}
	case 6:
	{
		if (D.Layout != EBRLayout::Suburbs)
		{
			PlanStage = 7;
			break;
		}
		// ---- Banlieue : routes et maisons ----
		if (D.Layout == EBRLayout::Suburbs)
		{
			const int32 X = X0 + PlanCursor;
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
		NextColumn();
		break;
	}
	case 7:
	{
		if (D.Layout != EBRLayout::City)
		{
			PlanStage = 8;
			break;
		}
		// ---- Ville : immeubles ----
		if (D.Layout == EBRLayout::City)
		{
			const int32 Sp = D.Spacing;
			const int32 X = X0 + PlanCursor;
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
		NextColumn();
		break;
	}
	case 8:
		// ---- Cachettes (avant les accessoires : on ne pose rien dans un placard) ----
		if (D.HidingSpotChance > 0.f && (D.Layout == EBRLayout::Rooms || D.Layout == EBRLayout::Maze))
		{
			BuildHidingSpots();
		}
		++PlanStage;
		break;
	case 9:
		// ---- Sorties (decidees avant le plafond : une echelle le perce d'une trappe) et plafond ----
		PlanExits();
		if (D.bCeiling)
		{
			BuildCeiling();
		}
		++PlanStage;
		break;
	case 10:
	{
		// ---- Lumieres & accessoires par cellule ----
		const int32 X = X0 + PlanCursor;
		for (int32 Y = Y0; Y < Y0 + N; ++Y)
		{
			const FBRLightInfo L = InWorld->CellLight(X, Y);
			const FVector LightPos = InWorld->CellCenter(FIntPoint(X, Y)) + L.Offset;
			const bool bOverHatch = bShaftHole && LightPos.X > ShaftMin.X - 70.f && LightPos.X < ShaftMax.X + 70.f
				&& LightPos.Y > ShaftMin.Y - 70.f && LightPos.Y < ShaftMax.Y + 70.f;
			if (L.bHas && !bOverHatch)
			{
				AddLight(X, Y, L);
			}
			if (InWorld->IsWalkable(FIntPoint(X, Y)) && !HidingCells.Contains(FIntPoint(X, Y)) && !ExitCells.Contains(FIntPoint(X, Y))
				&& !InWorld->IsPitRoomCell(X, Y) && !InWorld->IsMissionCell(X, Y))
			{
				BuildCellProps(X, Y);
			}
		}
		NextColumn();
		break;
	}
	default:
		if (D.SkylightChance > 0.f)
		{
			BuildSkylight();
		}
		// Rien n'est encore cree (sauf les quelques lumieres des bassins et verrieres des Poolrooms) : StepBuild s'en charge
		bDeferLights = false;
		bPlanned = true;
		return true;
	}
	return false;
}

FString ABRChunk::NextBatchKey(bool bCollision) const
{
	for (const TPair<FString, FBatch>& Pair : Batches)
	{
		if (Pair.Value.bCollision == bCollision)
		{
			return Pair.Key;
		}
	}
	return FString();
}

float ABRChunk::CostPerInstanceMs[2] = { 0.02f, 0.004f };

int32 ABRChunk::SubBatchSize(bool bCollision, double RemainingMs)
{
	// Autant d'instances que le budget restant en permet d'apres le cout moyen mesure (collisions : un corps physique par
	// instance), au moins 16 pour avancer, au plus 1024 par composant
	const double Cost = FMath::Max(0.0005, static_cast<double>(CostPerInstanceMs[bCollision ? 0 : 1]));
	return FMath::Clamp(static_cast<int32>(FMath::Max(0.0, RemainingMs) / Cost), 16, 1024);
}

bool ABRChunk::StepBuild(double BudgetMs, bool bStopAtCollision)
{
	if (bReady)
	{
		return true;
	}
	if (bTearingDown)
	{
		return false;
	}
	const double Start = FPlatformTime::Seconds();
	auto Elapsed = [Start]() { return (FPlatformTime::Seconds() - Start) * 1000.0; };
	++Stats.Steps;
	bool bDidWork = false;
	auto Spent = [&]() { return bDidWork && Elapsed() > BudgetMs; };
	while (!bReady && !Spent() && !(bStopAtCollision && bCollisionReady))
	{
		const double T0 = FPlatformTime::Seconds();
		float* Bucket = nullptr;
		if (!bPlanned)
		{
			// v4.8 : planification par petites unites (colonnes de cellules)
			StepPlan();
			Stats.PlanMs += static_cast<float>((FPlatformTime::Seconds() - T0) * 1000.0);
			bDidWork = true;
			continue;
		}
		switch (BuildPhase)
		{
		case 0:
		case 1:
		case 3:
		{
			// 0 : collisions d'abord (sol, murs, boites des meubles : on peut y marcher des la fin de l'etape) ;
			// 1 : visuels sans collision (details, accessoires, eau, luminaires groupes) ;
			// 3 : luminaires groupes ajoutes par les lumieres.
			// v4.8 : par sous-lots, dimensionnes d'apres le cout mesure et le budget restant
			const bool bCollisionPhase = BuildPhase == 0;
			FString Key = NextBatchKey(bCollisionPhase);
			if (Key.IsEmpty() && BuildPhase == 3)
			{
				Key = NextBatchKey(true);
			}
			if (Key.IsEmpty())
			{
				if (BuildPhase == 0)
				{
					bCollisionReady = true;
				}
				BuildPhase = BuildPhase == 3 ? 4 : BuildPhase + 1;
				break;
			}
			const FBatch* B = Batches.Find(Key);
			const bool bCollisionBatch = B && B->bCollision;
			const int32 Count = CreateBatchStep(Key, SubBatchSize(bCollisionBatch, BudgetMs - Elapsed()));
			if (Count > 0)
			{
				const float Ms = static_cast<float>((FPlatformTime::Seconds() - T0) * 1000.0);
				float& Cost = CostPerInstanceMs[bCollisionBatch ? 0 : 1];
				Cost = FMath::Lerp(Cost, Ms / Count, 0.2f);
			}
			Bucket = BuildPhase == 0 ? &Stats.CollisionMs : (BuildPhase == 1 ? &Stats.VisualMs : &Stats.LightMs);
			break;
		}
		case 2:
			// Lumieres, une a une (une lumiere et son luminaire)
			if (PendingLights.Num() == 0)
			{
				BuildPhase = 3;
				break;
			}
			{
				const FPendingLight P = PendingLights[0];
				PendingLights.RemoveAt(0);
				AddLight(P.X, P.Y, P.L);
			}
			Bucket = &Stats.LightMs;
			break;
		default:
		{
			// v4.8 : objets un par un (tirages d'objets, puis sorties) ; puis animation des neons qui clignotent
			const int32 Rolls = NumPickupRolls();
			if (ActorCursor < Rolls)
			{
				SpawnPickupRoll(ActorCursor++);
			}
			else if (ActorCursor < Rolls + PlannedExits.Num())
			{
				SpawnPlannedExit(ActorCursor++ - Rolls);
			}
			else
			{
				SetActorTickEnabled(Flickers.Num() > 0);
				bReady = true;
			}
			Bucket = &Stats.ActorMs;
			break;
		}
		}
		if (Bucket)
		{
			*Bucket += static_cast<float>((FPlatformTime::Seconds() - T0) * 1000.0);
			bDidWork = true;
		}
	}
	return bReady;
}

bool ABRChunk::StepTeardown(double BudgetMs)
{
	// v4.8 : demontage etale : un chunk qui sort de la vue n'est plus detruit d'un bloc (composants, corps physiques,
	// lumieres et objets dans la meme image) ; quelques elements par image, dans le budget
	bTearingDown = true;
	const double Start = FPlatformTime::Seconds();
	bool bDidWork = false;
	while (!bDidWork || (FPlatformTime::Seconds() - Start) * 1000.0 < BudgetMs)
	{
		bDidWork = true;
		++TeardownStep;
		if (Spawned.Num() > 0)
		{
			AActor* A = Spawned.Pop(EAllowShrinking::No);
			if (IsValid(A))
			{
				A->Destroy();
			}
			continue;
		}
		if (Instances.Num() > 0)
		{
			UInstancedStaticMeshComponent* ISM = Instances.Pop(EAllowShrinking::No);
			if (IsValid(ISM))
			{
				ISM->DestroyComponent();
			}
			continue;
		}
		if (Extra.Num() > 0)
		{
			UActorComponent* C = Extra.Pop(EAllowShrinking::No);
			if (IsValid(C))
			{
				C->DestroyComponent();
			}
			continue;
		}
		PoweredLights.Reset();
		Flickers.Reset();
		LightRecords.Reset();
		return true;
	}
	return false;
}

void ABRChunk::BuildHidingSpots()
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const uint32 Seed = W->GetSeed();
	const int32 N = D.ChunkCells;
	const float S = D.CellSize;
	const float H = D.WallHeight;
	const FBRSurface Wood(TEXT("T_Wood"), FLinearColor(0.82f, 0.7f, 0.5f), 100.f, 0.55f, 0.35f);
	const FBRSurface Dark(TEXT("T_Grime"), FLinearColor(0.02f, 0.02f, 0.018f), 100.f, 0.95f, 0.f);
	const int32 Count = FMath::FloorToInt(D.HidingSpotChance * 2.f + BRHash::Rand(Coord.X, Coord.Y, 1740, Seed));

	for (int32 k = 0; k < Count; ++k)
	{
		for (int32 Try = 0; Try < 20; ++Try)
		{
			const uint32 Hh = BRHash::Hash(Coord.X, Coord.Y, 1742 + k * 37 + Try, Seed);
			const FIntPoint Cell(Coord.X * N + static_cast<int32>(Hh % static_cast<uint32>(N)), Coord.Y * N + static_cast<int32>((Hh >> 8) % static_cast<uint32>(N)));
			const FIntPoint Dir = GDirs[(Hh >> 16) % 4u];
			if (!W->IsWalkable(Cell) || W->IsSpawnArea(Cell.X, Cell.Y) || W->IsPoolCell(Cell.X, Cell.Y) || HidingCells.Contains(Cell)
				|| W->IsPitRoomCell(Cell.X, Cell.Y) || W->IsMissionCell(Cell.X, Cell.Y))
			{
				continue;
			}
			// Contre un vrai mur (ni porte ni passage)
			const FIntPoint Next(Cell.X + Dir.X, Cell.Y + Dir.Y);
			const EBREdge E = Dir.X != 0 ? W->EdgeE(Dir.X > 0 ? Cell.X : Cell.X - 1, Cell.Y) : W->EdgeN(Cell.X, Dir.Y > 0 ? Cell.Y : Cell.Y - 1);
			const bool bSolidNext = !W->IsWalkable(Next);
			if (E != EBREdge::Wall && !bSolidNext)
			{
				continue;
			}
			const float Inset = bSolidNext ? 0.f : D.WallThickness * 0.5f;
			const FVector Face = W->CellCenter(Cell, 0.f) + FVector(Dir.X, Dir.Y, 0.f) * (S * 0.5f - Inset);
			const FVector In(-Dir.X, -Dir.Y, 0.f);         // vers la piece
			const FVector Tg(-Dir.Y, Dir.X, 0.f);          // le long du mur
			auto Box = [&](float Along, float Out, float Z, float SizeAlong, float SizeOut, float SizeZ, const FBRSurface& Surf, bool bCollision)
			{
				const FVector C = Face + Tg * Along + In * Out + FVector(0.f, 0.f, Z);
				AddBox(Surf, C, Dir.X != 0 ? FVector(SizeOut, SizeAlong, SizeZ) : FVector(SizeAlong, SizeOut, SizeZ), bCollision);
			};
			FHidingSpot Spot;
			const bool bCloset = BRHash::Rand(Cell.X, Cell.Y, 1743, Seed) < 0.55f;
			float HalfW = 0.f;
			float Depth = 0.f;
			float Top = 0.f;
			if (bCloset)
			{
				// Placard de bureau, une porte entrouverte : on entre et on attend que ca passe
				const float Wd = 160.f;
				const float Dp = 80.f;
				const float Ht = 220.f;
				const float Th = 4.f;
				Box(-(Wd - Th) * 0.5f, Dp * 0.5f, Ht * 0.5f, Th, Dp, Ht, Wood, true);
				Box((Wd - Th) * 0.5f, Dp * 0.5f, Ht * 0.5f, Th, Dp, Ht, Wood, true);
				Box(0.f, Dp * 0.5f, Ht - Th * 0.5f, Wd, Dp, Th, Wood, true);
				Box(0.f, Dp * 0.5f, 3.f, Wd, Dp, 6.f, Wood, true);
				Box(0.f, 1.5f, Ht * 0.5f, Wd - 2.f * Th, 3.f, Ht - 8.f, Dark, false);
				Box(-Wd * 0.25f, Dp - 1.5f, Ht * 0.5f, Wd * 0.5f - 2.f, 3.f, Ht - 12.f, Wood, true);
				// Porte ouverte a ~105 degres autour de sa charniere
				const FVector Hinge = Face + Tg * (Wd * 0.5f) + In * Dp;
				const FVector DoorDir = (-Tg * FMath::Cos(FMath::DegreesToRadians(105.f)) + In * FMath::Sin(FMath::DegreesToRadians(105.f))).GetSafeNormal();
				const float DoorYaw = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(DoorDir.Y), static_cast<float>(DoorDir.X)));
				AddBox(Wood, Hinge + DoorDir * (Wd * 0.25f) + FVector(0.f, 0.f, Ht * 0.5f), FVector(Wd * 0.5f - 2.f, 3.f, Ht - 12.f), true, DoorYaw);
				HalfW = Wd * 0.5f - Th;
				Depth = Dp - 4.f;
				Top = Ht;
			}
			else
			{
				// Trou dans le mur : un pan de mur epais avec une ouverture basse et noire, ou l'on se glisse accroupi
				const float Wd = 130.f;
				const float Dp = 95.f;
				const float HoleH = 105.f;
				const float Side = 18.f;
				Box(-(Wd - Side) * 0.5f, Dp * 0.5f, HoleH * 0.5f, Side, Dp, HoleH, D.Wall, true);
				Box((Wd - Side) * 0.5f, Dp * 0.5f, HoleH * 0.5f, Side, Dp, HoleH, D.Wall, true);
				Box(0.f, Dp * 0.5f, (HoleH + H) * 0.5f, Wd, Dp, H - HoleH, D.Wall, true);
				Box(0.f, 2.f, HoleH * 0.5f, Wd - 2.f * Side, 4.f, HoleH, Dark, false);
				Box(0.f, Dp * 0.5f, HoleH - 1.5f, Wd - 2.f * Side, Dp - 2.f, 3.f, Dark, false);
				Box(-(Wd * 0.5f - Side - 1.f), Dp * 0.5f, HoleH * 0.5f, 2.f, Dp - 2.f, HoleH - 3.f, Dark, false);
				Box(Wd * 0.5f - Side - 1.f, Dp * 0.5f, HoleH * 0.5f, 2.f, Dp - 2.f, HoleH - 3.f, Dark, false);
				HalfW = Wd * 0.5f - Side;
				Depth = Dp;
				Top = HoleH + 120.f;
				Spot.bCrouch = true;
			}
			// Volume de la cachette (le centre de la capsule du joueur doit s'y trouver)
			const FVector A0 = Face + Tg * HalfW + In * 2.f;
			const FVector A1 = Face - Tg * HalfW + In * Depth;
			Spot.Box = FBox(FVector(FMath::Min(A0.X, A1.X), FMath::Min(A0.Y, A1.Y), -50.f), FVector(FMath::Max(A0.X, A1.X), FMath::Max(A0.Y, A1.Y), Top));
			HidingSpots.Add(Spot);
			HidingCells.Add(Cell);
			break;
		}
	}
}

bool ABRChunk::IsInHidingSpot(const FVector& Location, bool bCrouched) const
{
	for (const FHidingSpot& Spot : HidingSpots)
	{
		if (Spot.Box.IsInside(Location) && (bCrouched || !Spot.bCrouch))
		{
			return true;
		}
	}
	return false;
}

bool ABRChunk::FindHidingSpotNear(const FVector& Location, float Radius, bool& bOutNeedsCrouch) const
{
	for (const FHidingSpot& Spot : HidingSpots)
	{
		if (Spot.Box.ComputeSquaredDistanceToPoint(Location) < Radius * Radius)
		{
			bOutNeedsCrouch = Spot.bCrouch;
			return true;
		}
	}
	return false;
}

void ABRChunk::BuildSkylight()
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const uint32 Seed = W->GetSeed();
	if (BRHash::Rand(Coord.X, Coord.Y, 1730, Seed) >= D.SkylightChance)
	{
		return;
	}
	const int32 N = D.ChunkCells;
	for (int32 Try = 0; Try < 24; ++Try)
	{
		const uint32 Hh = BRHash::Hash(Coord.X, Coord.Y, 1731 + Try, Seed);
		const FIntPoint Cell(Coord.X * N + static_cast<int32>(Hh % static_cast<uint32>(N)), Coord.Y * N + static_cast<int32>((Hh >> 8) % static_cast<uint32>(N)));
		const FIntPoint Dir = GDirs[(Hh >> 16) % 4u];
		if (!W->IsWalkable(Cell) || W->IsSpawnArea(Cell.X, Cell.Y))
		{
			continue;
		}
		// Il faut un vrai mur (ni porte, ni passage) sur ce cote de la cellule
		const EBREdge E = Dir.X != 0 ? W->EdgeE(Dir.X > 0 ? Cell.X : Cell.X - 1, Cell.Y) : W->EdgeN(Cell.X, Dir.Y > 0 ? Cell.Y : Cell.Y - 1);
		if (E != EBREdge::Wall && W->IsWalkable(FIntPoint(Cell.X + Dir.X, Cell.Y + Dir.Y)))
		{
			continue;
		}
		const float H = D.WallHeight;
		AddFaceProp(Cell.X, Cell.Y, Dir, TEXT("SM_PoolSkylight"), 0.f, H, FVector::ZeroVector, false);

		// La lumiere du jour entre par les vitrages inclines (vers la piece et vers le bas)
		const float Inset = D.WallThickness * 0.5f;
		const FVector Into(-Dir.X, -Dir.Y, 0.f);
		const FVector Wall = W->CellCenter(Cell, H) + FVector(Dir.X, Dir.Y, 0.f) * (D.CellSize * 0.5f - Inset);
		URectLightComponent* RL = NewObject<URectLightComponent>(this);
		RL->SetupAttachment(Root);
		RL->SetMobility(EComponentMobility::Movable);
		RL->SetRelativeLocation(Wall + Into * 62.f + FVector(0.f, 0.f, -95.f) - GetActorLocation());
		RL->SetRelativeRotation(FRotator(-31.f, YawFromDir(Into.X, Into.Y), 0.f));
		RL->SetSourceWidth(280.f);
		RL->SetSourceHeight(200.f);
		RL->SetBarnDoorAngle(80.f);
		RL->SetBarnDoorLength(10.f);
		RL->SetIntensityUnits(ELightUnits::Lumens);
		RL->SetIntensity(26000.f);
		RL->SetLightColor(FLinearColor(0.93f, 0.97f, 1.f));
		RL->SetAttenuationRadius(2600.f);
		RL->SetCastShadows(true);
		RL->SetVolumetricScatteringIntensity(1.6f); // rayons de lumiere du jour dans l'air humide
		RL->MaxDrawDistance = D.ViewDistance;
		RL->MaxDistanceFadeRange = 1000.f;
		RL->RegisterComponent();
		Extra.Add(RL);
		return;
	}
}

void ABRChunk::AddFloorPaint(const FVector& Center, float Length, float Width, float Yaw, bool bYellow)
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	// Peinture sur le beton : meme texture (le grain du sol transparait), teintee ; les flaques la recouvrent aussi
	const FBRSurface& Floor = W->Def().Floor;
	// v4.7 : peinture de sol : satinee, l'huile du beton ne la rend pas plus brillante (RoughDetail faible)
	FBRSurface Paint(TEXT("T_ConcreteFloor"), bYellow ? FLinearColor(2.1f, 1.62f, 0.32f) : FLinearColor(1.9f, 1.9f, 1.85f), Floor.Scale, 0.42f, 0.5f);
	Paint.RoughDetail = 0.25f;
	Paint.Puddles = Floor.Puddles;
	Paint.Wetness = Floor.Wetness;
	AddBox(Paint, FVector(Center.X, Center.Y, 0.3f), FVector(Length, Width, 0.6f), false, Yaw);
}

void ABRChunk::BuildGarage()
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const int32 N = D.ChunkCells;
	const float S = D.CellSize;
	const float H = D.WallHeight;
	const int32 X0 = Coord.X * N;
	const int32 Y0 = Coord.Y * N;
	const uint32 Seed = W->GetSeed();
	const float ChunkW = N * S;

	// Poutres de beton sous la dalle, dans un seul sens (a chaque ligne de la grille), et gaines le long de certaines
	for (int32 X = X0; X < X0 + N; ++X)
	{
		const float Fixed = (X + 1) * S;
		AddBox(D.Ceiling, FVector(Fixed, (Y0 + N * 0.5f) * S, H - 26.f), FVector(46.f, ChunkW, 52.f), false);
		if (BRHash::Rand(X, Coord.Y, 1210, Seed) < 0.35f)
		{
			AddProp(TEXT("SM_Pipe"), FTransform(FRotator(0.f, 90.f, 0.f), FVector(Fixed + 70.f, (Y0 + N * 0.5f) * S, H - 30.f), FVector(ChunkW / 100.f, 0.9f, 0.9f)),
				false, FVector::ZeroVector);
		}
	}

	// Marquages : places de stationnement contre les murs, fleches et ligne jaune dans les allees
	for (int32 X = X0; X < X0 + N; ++X)
	{
		for (int32 Y = Y0; Y < Y0 + N; ++Y)
		{
			if (!W->IsWalkable(FIntPoint(X, Y)) || W->IsSpawnArea(X, Y))
			{
				continue;
			}
			const FVector C = W->CellCenter(FIntPoint(X, Y));
			auto R = [&](int32 Salt) { return BRHash::Rand(X, Y, Salt, Seed); };
			// Cote adosse a un mur (0 : +X, 1 : -X, 2 : +Y, 3 : -Y), sinon allee
			int32 Back = INDEX_NONE;
			const EBREdge Sides[4] = { W->EdgeE(X, Y), W->EdgeE(X - 1, Y), W->EdgeN(X, Y), W->EdgeN(X, Y - 1) };
			const int32 Start = static_cast<int32>(R(1220) * 4.f);
			for (int32 k = 0; k < 4; ++k)
			{
				const int32 Side = (Start + k) % 4;
				if (Sides[Side] == EBREdge::Wall)
				{
					Back = Side;
					break;
				}
			}
			if (Back == INDEX_NONE && R(1221) < 0.45f)
			{
				Back = static_cast<int32>(R(1222) * 4.f) % 4; // places en epi au milieu d'une grande salle
			}
			if (Back != INDEX_NONE)
			{
				// Deux places de 2,5 m : trois traits perpendiculaires au mur, longs de 4,8 m
				const FVector Out = Back == 0 ? FVector(-1.f, 0.f, 0.f) : Back == 1 ? FVector(1.f, 0.f, 0.f) : Back == 2 ? FVector(0.f, -1.f, 0.f) : FVector(0.f, 1.f, 0.f);
				const FVector Along(-Out.Y, Out.X, 0.f);
				const float Yaw = Back <= 1 ? 0.f : 90.f; // trait oriente le long de Out
				const FVector Wall = C - Out * (S * 0.5f - D.WallThickness * 0.5f - 12.f);
				for (int32 i = -1; i <= 1; ++i)
				{
					AddFloorPaint(Wall + Out * 240.f + Along * (i * 250.f), 480.f, 12.f, Yaw, false);
				}
				// Arret de roue (petit bloc de beton) au fond de chaque place
				if (R(1223) < 0.6f)
				{
					static const FBRSurface Stop(TEXT("T_Concrete"), FLinearColor(1.2f, 1.2f, 1.15f), 120.f, 0.8f, 0.5f);
					for (int32 i = 0; i < 2; ++i)
					{
						AddBox(Stop, Wall + Out * 70.f + Along * ((i - 0.5f) * 250.f) + FVector(0.f, 0.f, 6.f), Back <= 1 ? FVector(18.f, 150.f, 12.f)
							: FVector(150.f, 18.f, 12.f), true);
					}
				}
			}
			else
			{
				// Allee : pointilles jaunes au milieu et parfois une fleche de sens de circulation
				const bool bAlongX = R(1224) < 0.5f;
				for (int32 i = -1; i <= 1; ++i)
				{
					const FVector P = C + (bAlongX ? FVector(i * 200.f, 0.f, 0.f) : FVector(0.f, i * 200.f, 0.f));
					AddFloorPaint(P, 110.f, 12.f, bAlongX ? 0.f : 90.f, true);
				}
				if (R(1225) < 0.35f)
				{
					const float Dir = R(1226) < 0.5f ? 1.f : -1.f;
					const float BaseYaw = (bAlongX ? 0.f : 90.f) + (Dir > 0.f ? 0.f : 180.f);
					const FVector Fwd = FRotator(0.f, BaseYaw, 0.f).Vector();
					const FVector Side = FVector(-Fwd.Y, Fwd.X, 0.f) * 140.f;
					AddFloorPaint(C + Side, 170.f, 18.f, BaseYaw, false);
					AddFloorPaint(C + Side + Fwd * 70.f + FRotator(0.f, BaseYaw + 140.f, 0.f).Vector() * 30.f, 70.f, 18.f, BaseYaw + 140.f, false);
					AddFloorPaint(C + Side + Fwd * 70.f + FRotator(0.f, BaseYaw - 140.f, 0.f).Vector() * 30.f, 70.f, 18.f, BaseYaw - 140.f, false);
				}
			}
		}
	}
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
		if (R(910) < 0.08f && D.Floor.Puddles <= 0.f)
		{
			const FVector P = C + FVector((R(911) - 0.5f) * S * 0.6f, (R(912) - 0.5f) * S * 0.6f, 0.6f);
			AddWaterPlane(P, FVector2D(150.f + R(913) * 200.f, 120.f + R(914) * 150.f), true);
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
		const bool bFullDesk = BRRig::HasMesh(this, TEXT("SM_OfficeDeskET"));
		int32 DoorSide = 0;
		if (bFullDesk && W->IsCubicle(X, Y, &DoorSide))
		{
			// Bureau cloisonne : le poste de travail contre une cloison laterale, la fontaine au fond
			const FIntPoint DoorDir = GDirs[DoorSide];
			const FVector Back(-DoorDir.X, -DoorDir.Y, 0.f);
			const FVector Left(Back.Y, -Back.X, 0.f);
			auto Walled = [&](const FVector& V)
			{
				return !W->CanStep(FIntPoint(X, Y), FIntPoint(X + FMath::RoundToInt(V.X), Y + FMath::RoundToInt(V.Y)));
			};
			const bool bL = Walled(Left);
			const bool bR = Walled(-Left);
			if (bL || bR)
			{
				const FVector ToWall = (bL && (!bR || R(906) < 0.5f)) ? Left : -Left;
				const FVector Face = C + ToWall * (S * 0.5f - D.WallThickness * 0.5f);
				AddWorkstation(X, Y, Face, ToWall, Back, R(907) < 0.4f);
			}
			break;
		}
		const float Roll = R(901);
		if (Roll < D.PropDensity && bFullDesk)
		{
			// Open space : un poste au milieu de la piece, dos a une cloison basse
			const float Yaw = 90.f * FMath::FloorToFloat(R(902) * 4.f);
			const FRotator Rot(0.f, Yaw, 0.f);
			const FVector ToWall = -Rot.Vector();
			const FVector Along = Rot.RotateVector(FVector(0.f, -1.f, 0.f));
			AddWorkstation(X, Y, C + ToWall * (GDeskBack + 6.f), ToWall, Along, false);
			AddProp(TEXT("SM_Partition"), FTransform(Rot, C + ToWall * (GDeskBack + 9.f)), true, FVector(5.f, 120.f, 140.f));
		}
		else if (Roll < D.PropDensity)
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
			const FName Cooler = BRRig::HasMesh(this, TEXT("SM_WaterCoolerET")) ? FName(TEXT("SM_WaterCoolerET")) : FName(TEXT("SM_WaterCooler"));
			AddProp(Cooler, FTransform(FRotator(0.f, 225.f, 0.f), P), true, FVector(32.f, 32.f, 140.f));
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

void ABRChunk::AddWorkstation(int32 X, int32 Y, const FVector& WallFace, const FVector& ToWall, const FVector& Back, bool bCooler)
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	const uint32 Seed = W->GetSeed();
	auto R = [&](int32 Salt) { return BRHash::Rand(X, Y, Salt, Seed); };
	// Le bureau tourne le dos au mur (+X local vers la piece) ; son -Y local doit aller vers Back, sinon on le retourne
	const FRotator Rot(0.f, FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-ToWall.Y), static_cast<float>(-ToWall.X))), 0.f);
	const bool bMirror = FVector::DotProduct(Rot.RotateVector(FVector(0.f, -1.f, 0.f)), Back) < 0.f;
	const float MY = bMirror ? -1.f : 1.f;
	// Le groupe (bureau + fontaine) est centre le long du mur
	const float Shift = bCooler ? -(GCoolerHalf - GCoolerOffset.Y - GDeskHalfLen) * 0.5f : 0.f;
	const FVector DeskPos = WallFace - ToWall * (GDeskBack + 2.f) + Back * Shift;
	AddProp(TEXT("SM_OfficeDeskET"), FTransform(Rot, DeskPos, FVector(1.f, MY, 1.f)), true, FVector(98.f, 186.f, 76.f));
	const FVector ChairLocal(GChairOffset.X + (R(921) - 0.5f) * 30.f, (GChairOffset.Y + (R(922) - 0.5f) * 40.f) * MY, 0.f);
	AddProp(TEXT("SM_OfficeChairET"), FTransform(FRotator(0.f, Rot.Yaw + (R(923) - 0.5f) * 70.f, 0.f), DeskPos + Rot.RotateVector(ChairLocal)), true,
		FVector(60.f, 60.f, 110.f));
	if (bCooler)
	{
		const FVector CoolerLocal(GCoolerOffset.X, GCoolerOffset.Y * MY, 0.f);
		AddProp(TEXT("SM_WaterCoolerET"), FTransform(Rot, DeskPos + Rot.RotateVector(CoolerLocal)), true, FVector(45.f, 45.f, 160.f));
	}
}

int32 ABRChunk::NumPickupRolls()
{
	return 11;
}

bool ABRChunk::SpawnPickupRoll(int32 Index)
{
	ABRWorld* W = World.Get();
	if (!W || !GetWorld())
	{
		return false;
	}
	const FBRLevelDef& D = W->Def();
	const uint32 Seed = W->GetSeed();
	const float S = D.CellSize;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

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
	static_assert(UE_ARRAY_COUNT(Rolls) == 11, "NumPickupRolls doit suivre la table");
	if (Index < 0 || Index >= static_cast<int32>(UE_ARRAY_COUNT(Rolls)))
	{
		return false;
	}
	const bool bBounded = D.BoundsChunks > 0;
	const FPickupRoll& Roll = Rolls[Index];
	bool bRoll = Roll.Chance > 0.f && BRHash::Rand(Coord.X, Coord.Y, Roll.Salt, Seed) < Roll.Chance;
	if (bBounded && Roll.Item == EBRItem::VHSTape)
	{
		// Niveau fini : deux cassettes de plus que necessaire, une par chunk tire (loin du point de depart)
		bRoll = VHS > 0.f && Roll.Salt == 1008 && W->IsChunkPicked(Coord, 1008, D.VHSRequired + 2, true);
	}
	if (!bRoll)
	{
		return true;
	}
	FIntPoint Cell(0, 0);
	// v4.6 : niveau fini, cassette garantie : on balaie tout le chunk si les essais tires tombent mal
	if (!PickFreeCell(Roll.Salt, Cell, bBounded && Roll.Item == EBRItem::VHSTape))
	{
		return true;
	}
	if (Roll.Item == EBRItem::VHSTape && W->IsSpawnArea(Cell.X, Cell.Y))
	{
		return true;
	}
	// La graine entre dans l'identifiant : un objet ramasse au niveau precedent ne cache rien dans le suivant
	const uint64 Id = (static_cast<uint64>(static_cast<uint32>(Coord.X)) << 40) ^ (static_cast<uint64>(static_cast<uint32>(Coord.Y)) << 16)
		^ static_cast<uint64>(Roll.Salt) ^ (static_cast<uint64>(Seed) * 0x9E3779B97F4A7C15ull);
	if (W->IsCollected(Id))
	{
		return true;
	}
	const float JX = (BRHash::Rand(Cell.X, Cell.Y, Roll.Salt + 7, Seed) - 0.5f) * S * 0.5f;
	const float JY = (BRHash::Rand(Cell.X, Cell.Y, Roll.Salt + 8, Seed) - 0.5f) * S * 0.5f;
	FVector Pos = W->CellCenter(Cell, 1.f) + FVector(JX, JY, 0.f);
	Pos.Z += W->FloorZAt(Pos); // sur un trottoir (Niveau 37)
	const FRotator Rot(0.f, BRHash::Rand(Cell.X, Cell.Y, Roll.Salt + 9, Seed) * 360.f, 0.f);
	ABRPickup* P = GetWorld()->SpawnActor<ABRPickup>(ABRPickup::StaticClass(), FTransform(Rot, Pos), Params);
	if (P)
	{
		FString Note;
		if (Roll.Item == EBRItem::Note)
		{
			// v4.8 : la note est designee par son identifiant (meme tirage qu'avant) ; son texte suit la langue de chacun
			const TArray<FBRNote>& Common = BRLevels::CommonNotes();
			const int32 Total = D.Notes.Num() + Common.Num();
			const int32 Idx = Total > 0 ? static_cast<int32>(BRHash::Hash(Coord.X, Coord.Y, 1005, Seed) % static_cast<uint32>(Total)) : 0;
			Note = Idx < D.Notes.Num() ? D.Notes[Idx].Id.ToString()
				: (Common.IsValidIndex(Idx - D.Notes.Num()) ? Common[Idx - D.Notes.Num()].Id.ToString() : FString());
		}
		P->Init(Roll.Item, Id, Note);
		Spawned.Add(P);
	}
	return true;
}

void ABRChunk::SpawnPlannedExit(int32 Index)
{
	ABRWorld* W = World.Get();
	if (!W || !GetWorld() || !PlannedExits.IsValidIndex(Index))
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FPlannedExit& P = PlannedExits[Index];
	ABRExit* Exit = GetWorld()->SpawnActor<ABRExit>(ABRExit::StaticClass(), FTransform(FRotator(0.f, P.Yaw, 0.f), P.Pos), Params);
	if (Exit)
	{
		Exit->Init(P.Target, P.Style);
		if (P.Style == EBRExitStyle::Ladder)
		{
			Exit->InitLadder(D.WallHeight - static_cast<float>(P.Pos.Z), P.Shaft); // v4.12 : depuis le pied (trottoir)
		}
		Spawned.Add(Exit);
	}
}

void ABRChunk::BuildPickupsAndExits()
{
	for (int32 i = 0; i < NumPickupRolls(); ++i)
	{
		SpawnPickupRoll(i);
	}
	for (int32 i = 0; i < PlannedExits.Num(); ++i)
	{
		SpawnPlannedExit(i);
	}
}

void ABRChunk::PlanExits()
{
	PlannedExits.Reset();
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const uint32 Seed = W->GetSeed();
	const int32 N = D.ChunkCells;
	const float S = D.CellSize;
	const int32 X0 = Coord.X * N;
	const int32 Y0 = Coord.Y * N;


	const bool bBounded = D.BoundsChunks > 0;
	if (!bBounded && Coord == FIntPoint(0, 0))
	{
		return; // jamais de sortie juste a cote du point d'apparition
	}
	for (int32 i = 0; i < D.Exits.Num(); ++i)
	{
		const FBRExitDef& Ex = D.Exits[i];
		// Niveau fini : un nombre garanti de sorties de chaque sorte, tirees parmi les chunks (hors point de depart)
		const bool bPicked = bBounded
			? W->IsChunkPicked(Coord, 1100 + i, FMath::Max(1, FMath::RoundToInt(Ex.ChancePerChunk * W->BoundedChunkCount(true))), true)
			: BRHash::Rand(Coord.X, Coord.Y, 1100 + i, Seed) < Ex.ChancePerChunk;
		if (!bPicked)
		{
			continue;
		}
		// v4.12 : vers un niveau pas encore disponible (sans redirection), un passage par noclip n'existe pas dans cette
		// version ; une porte, un ascenseur ou une echelle reste visible et condamne (ABRExit::IsSealed). Chaque sortie a
		// ses propres tirages : les autres ne changent pas
		if ((Ex.Style == EBRExitStyle::NoclipWall || Ex.Style == EBRExitStyle::NoclipFloor)
			&& BRLevels::ResolveExit(D.Number, Ex.Target).Kind == BRContent::EExit::Sealed)
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
			if (PickFreeCell(1200 + i, Cell, bBounded) && !W->IsSpawnArea(Cell.X, Cell.Y))
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
			// Sur une face de mur (mur fin ou cellule pleine). v4.6 : niveau fini (nombre de sorties garanti) : apres les 24
			// essais tires, on balaie toutes les cellules et directions du chunk dans un ordre tire
			const int32 Tries = bBounded ? 24 + N * N * 4 : 24;
			const int32 ScanStart = static_cast<int32>(BRHash::Hash(Coord.X, Coord.Y, 1500 + i * 64 + 63, Seed) % static_cast<uint32>(N * N * 4));
			for (int32 Try = 0; Try < Tries && !bFound; ++Try)
			{
				const uint32 Hh = BRHash::Hash(Coord.X, Coord.Y, 1500 + i * 64 + Try, Seed);
				FIntPoint Cell(X0 + static_cast<int32>(Hh % static_cast<uint32>(N)), Y0 + static_cast<int32>((Hh >> 8) % static_cast<uint32>(N)));
				FIntPoint Dir = GDirs[(Hh >> 16) % 4u];
				if (Try >= 24)
				{
					const int32 Idx = (ScanStart + Try - 24) % (N * N * 4);
					Cell = FIntPoint(X0 + (Idx / 4) % N, Y0 + (Idx / 4) / N);
					Dir = GDirs[Idx % 4];
				}
				if (!W->IsWalkable(Cell) || W->IsSpawnArea(Cell.X, Cell.Y) || W->IsPoolCell(Cell.X, Cell.Y) || HidingCells.Contains(Cell)
					|| W->IsPitRoomCell(Cell.X, Cell.Y) || !W->IsSafelyReachable(Cell) || W->IsMissionCell(Cell.X, Cell.Y))
				{
					continue; // v4.6 : jamais dans une salle de fosses ; niveau fini : atteignable sans la traverser
				}
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
				Pos.Z = W->FloorZAt(Pos - FVector(Dir.X, Dir.Y, 0.f) * 30.f); // pied de la sortie sur le trottoir
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
		FPlannedExit Plan;
		Plan.Pos = Pos;
		Plan.Yaw = Yaw;
		Plan.Target = Ex.Target;
		Plan.Style = Ex.Style;
		ExitCells.Add(W->WorldToCell(Pos + FRotator(0.f, Yaw, 0.f).Vector() * 30.f)); // rien n'encombre la sortie
		if (Ex.Style == EBRExitStyle::Ladder && D.bCeiling && !bShaftHole)
		{
			// L'echelle monte par une trappe dans un conduit sombre : on y noclippe en grimpant
			const FVector Fwd = FRotator(0.f, Yaw, 0.f).Vector();
			const FVector Side(-Fwd.Y, Fwd.X, 0.f);
			const FVector C0 = Pos + Side * 62.f - Fwd * 2.f;
			const FVector C1 = Pos - Side * 62.f + Fwd * 112.f;
			ShaftMin = FVector2D(FMath::Min(C0.X, C1.X), FMath::Min(C0.Y, C1.Y));
			ShaftMax = FVector2D(FMath::Max(C0.X, C1.X), FMath::Max(C0.Y, C1.Y));
			ShaftHeight = 300.f;
			bShaftHole = true;
			Plan.Shaft = ShaftHeight;
		}
		PlannedExits.Add(Plan);
	}
}

void ABRChunk::BuildCeiling()
{
	ABRWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	const FBRLevelDef& D = W->Def();
	const float ChunkW = D.ChunkCells * D.CellSize;
	const float X0 = Coord.X * ChunkW;
	const float Y0 = Coord.Y * ChunkW;
	const float X1 = X0 + ChunkW;
	const float Y1 = Y0 + ChunkW;
	const float H = D.WallHeight;
	auto Slab = [&](float AX, float AY, float BX, float BY)
	{
		if (BX - AX > 0.5f && BY - AY > 0.5f)
		{
			AddBox(D.Ceiling, FVector((AX + BX) * 0.5f, (AY + BY) * 0.5f, H + 10.f), FVector(BX - AX, BY - AY, 20.f));
		}
	};
	if (!bShaftHole)
	{
		Slab(X0, Y0, X1, Y1);
		return;
	}
	// Plafond autour de la trappe
	const FVector2D A = ShaftMin;
	const FVector2D B = ShaftMax;
	Slab(X0, Y0, X1, A.Y);
	Slab(X0, B.Y, X1, Y1);
	Slab(X0, A.Y, A.X, B.Y);
	Slab(B.X, A.Y, X1, B.Y);
	// Conduit au-dessus : quatre parois de beton sale et un fond, sans lumiere (seule la lueur du noclip, en haut)
	const FBRSurface Shaft(TEXT("T_Concrete"), FLinearColor(0.42f, 0.4f, 0.34f), 200.f, 0.9f, 0.6f);
	const float Th = 15.f;
	const float SH = ShaftHeight;
	const float Zc = H + SH * 0.5f;
	const float CX = (A.X + B.X) * 0.5f;
	const float CY = (A.Y + B.Y) * 0.5f;
	AddBox(Shaft, FVector(CX, A.Y - Th * 0.5f, Zc), FVector(B.X - A.X + 2.f * Th, Th, SH));
	AddBox(Shaft, FVector(CX, B.Y + Th * 0.5f, Zc), FVector(B.X - A.X + 2.f * Th, Th, SH));
	AddBox(Shaft, FVector(A.X - Th * 0.5f, CY, Zc), FVector(Th, B.Y - A.Y, SH));
	AddBox(Shaft, FVector(B.X + Th * 0.5f, CY, Zc), FVector(Th, B.Y - A.Y, SH));
	AddBox(Shaft, FVector(CX, CY, H + SH + Th * 0.5f), FVector(B.X - A.X + 2.f * Th, B.Y - A.Y + 2.f * Th, Th));
	// Cadre metallique de la trappe
	const FBRSurface Frame(TEXT("T_MetalPanel"), FLinearColor(0.35f, 0.35f, 0.33f), 100.f, 0.5f, 0.4f);
	AddBox(Frame, FVector(CX, A.Y + 2.5f, H - 1.f), FVector(B.X - A.X, 5.f, 4.f), false);
	AddBox(Frame, FVector(CX, B.Y - 2.5f, H - 1.f), FVector(B.X - A.X, 5.f, 4.f), false);
	AddBox(Frame, FVector(A.X + 2.5f, CY, H - 1.f), FVector(5.f, B.Y - A.Y, 4.f), false);
	AddBox(Frame, FVector(B.X - 2.5f, CY, H - 1.f), FVector(5.f, B.Y - A.Y, 4.f), false);
}

int32 ABRChunk::CreateBatchStep(const FString& Key, int32 MaxInstances)
{
	UBRAssets* A = UBRAssets::Get(this);
	FBatch* Found = Batches.Find(Key);
	if (!Found)
	{
		return 0;
	}
	FBatch& B = *Found;
	const int32 First = B.Cursor;
	const int32 Count = FMath::Clamp(B.Transforms.Num() - First, 0, FMath::Max(1, MaxInstances));
	B.Cursor += Count;
	const bool bDone = B.Cursor >= B.Transforms.Num();
	if (!A || !B.Mesh || Count <= 0)
	{
		Batches.Remove(Key);
		return 0;
	}
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
	ISM->SetupAttachment(Root);
	ISM->SetMobility(EComponentMobility::Static);
	ISM->SetStaticMesh(B.Mesh);
	ISM->SetCastShadow(B.bShadow && !B.bHidden);
	if (B.bNoRayTracing)
	{
		ISM->bVisibleInRayTracing = false;
	}
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
	TArray<FTransform> Part;
	Part.Append(B.Transforms.GetData() + First, Count);
	ISM->AddInstances(Part, false);
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
	if (bDone)
	{
		Batches.Remove(Key);
	}
	return Count;
}

void ABRChunk::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UBRAssets* A = UBRAssets::Get(this);
	const ABRWorld* W = World.Get();
	const TArray<FVector>* RedSources = W ? &W->GetRedLightSources() : nullptr;
	const float RedRadius = W ? W->GetRedLightRadius() : 0.f;
	const FLinearColor RedLight(1.f, 0.05f, 0.03f);
	for (FBRFlicker& F : Flickers)
	{
		float Mod = Power;
		if (F.bFlickers)
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
			// v4.7 : reglage FLASHS : le neon defaillant baisse sans s'eteindre (attenues), ou ne clignote plus (aucun)
			const float FS = FBRSettings::Get().FlashScale();
			Mod *= F.bOn ? (1.f - 0.15f * FS + 0.15f * FS * FMath::Sin(F.Phase)) : FMath::Lerp(0.85f, 0.03f, FS);
		}

		// Niveau 0 : la lampe vire au rouge quand l'entite qui fait des rondes passe a moins de RedRadius
		float RedTarget = 0.f;
		if (F.Light && RedSources && RedRadius > 0.f)
		{
			const FVector LP = F.Light->GetComponentLocation();
			for (const FVector& Src : *RedSources)
			{
				const float Dist = static_cast<float>(FVector::Dist2D(LP, Src));
				RedTarget = FMath::Max(RedTarget, FMath::Clamp((RedRadius - Dist) / (RedRadius * 0.3f), 0.f, 1.f));
			}
		}
		F.Red = FMath::FInterpTo(F.Red, RedTarget, DeltaSeconds, 2.5f);

		// On ne touche aux lumieres que si quelque chose a change (lampes stables : rien a faire la plupart du temps)
		if (FMath::Abs(Mod - F.AppliedMod) < 0.004f && FMath::Abs(F.Red - F.AppliedRed) < 0.004f)
		{
			continue;
		}
		F.AppliedMod = Mod;
		F.AppliedRed = F.Red;
		if (F.Light)
		{
			F.Light->SetIntensity(F.BaseIntensity * Mod * FMath::Lerp(1.f, 0.75f, F.Red));
			F.Light->SetLightColor(FMath::Lerp(F.LightColor, RedLight, F.Red));
		}
		const FLinearColor Glow = FMath::Lerp(F.GlowColor, RedLight * 90.f, F.Red) * Mod;
		for (UMaterialInstanceDynamic* G : F.Glow)
		{
			if (G)
			{
				G->SetVectorParameterValue(TEXT("Emissive"), Glow);
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
