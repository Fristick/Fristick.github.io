#include "BRWaterSim.h"
#include "BRWorld.h"

#include "Async/ParallelFor.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "Math/Float16.h"
#include "RHITypes.h"

namespace
{
	/** Pas fixe de la simulation (s) */
	constexpr float StepDt = 1.f / 60.f;
	/** Nombre de Courant (vitesse x pas / case) : 0,27 -> vagues a ~1 m/s, bien en dessous de la limite de stabilite */
	constexpr float Courant = 0.27f;
	/** Amortissement par pas : une vague s'eteint en 4 a 5 secondes */
	constexpr float Damping = 0.988f;
	/** Cases du bord de la zone qui absorbent les vagues (pas de rebond sur un bord invisible) */
	constexpr int32 Border = 20;

	constexpr uint8 BitSolid = 1;
	constexpr uint8 BitPillar = 2;
	uint8 EdgeE(uint8 Bits) { return (Bits >> 2) & 3; }
	uint8 EdgeN(uint8 Bits) { return (Bits >> 4) & 3; }
}

void UBRWaterSim::Reset()
{
	bValid = false;
	Accum = 0.f;
	Movers.Reset();
	Impulses.Reset();
	CellCache.Reset();
}

void UBRWaterSim::AddImpulse(const FVector2D& Pos, float Radius, float Depth)
{
	if (Impulses.Num() < 64)
	{
		Impulses.Add(FStamp{ FVector2f(Pos), FMath::Max(Radius, Texel * 1.5f), -Depth });
	}
}

void UBRWaterSim::AddMover(const FVector2D& Pos, const FVector2D& Dir, float Radius, float Push)
{
	if (Movers.Num() > 60)
	{
		return;
	}
	Radius = FMath::Max(Radius, Texel * 2.f);
	if (Dir.IsNearlyZero())
	{
		Movers.Add(FStamp{ FVector2f(Pos), Radius, Push });
		return;
	}
	// Vague d'etrave devant, creux derriere : un sillage sans faire monter ni baisser le niveau de l'eau
	const FVector2D Off = Dir * Radius * 0.6;
	Movers.Add(FStamp{ FVector2f(Pos + Off), Radius * 0.8f, Push });
	Movers.Add(FStamp{ FVector2f(Pos - Off), Radius * 0.8f, -Push });
}

FLinearColor UBRWaterSim::GetWindow() const
{
	const float Size = GridRes * Texel;
	if (!bValid)
	{
		return FLinearColor(0.f, 0.f, Size, 0.f);
	}
	// Intensite 2 : les pentes simulees sont doublees a l'affichage, pour que le sillage se lise bien
	return FLinearColor((Origin.X + GridRes * 0.5f) * Texel, (Origin.Y + GridRes * 0.5f) * Texel, Size, 2.f);
}

void UBRWaterSim::Tick(float Dt, const FVector& Focus, const ABRWorld* World)
{
	if (!World)
	{
		return;
	}
	const double Start = FPlatformTime::Seconds();
	if (!Texture)
	{
		Texture = UTexture2D::CreateTransient(GridRes, GridRes, PF_G16R16F,
			MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("T_WaterSim")));
		if (!Texture)
		{
			return;
		}
		Texture->SRGB = false;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Wrap;
		Texture->AddressY = TA_Wrap;
		Texture->NeverStream = true;
		Texture->UpdateResource();
	}
	if (H.Num() != GridRes * GridRes)
	{
		H.Init(0.f, GridRes * GridRes);
		HPrev.Init(0.f, GridRes * GridRes);
		Open.Init(1, GridRes * GridRes);
		bValid = false;
	}

	const FIntPoint NewOrigin(FMath::FloorToInt(Focus.X / Texel) - GridRes / 2, FMath::FloorToInt(Focus.Y / Texel) - GridRes / 2);
	if (!bValid || NewOrigin != Origin)
	{
		Recenter(NewOrigin, World);
	}

	Accum = FMath::Min(Accum + Dt, StepDt * 4.f);
	int32 Steps = 0;
	while (Accum >= StepDt)
	{
		Accum -= StepDt;
		Step(StepDt);
		++Steps;
	}
	// Les corps sont redonnes a chaque image par le monde
	Movers.Reset();
	if (Steps > 0)
	{
		Upload();
	}
	LastTickMs = static_cast<float>((FPlatformTime::Seconds() - Start) * 1000.0);
}

float UBRWaterSim::GetPeak() const
{
	float Peak = 0.f;
	for (const float V : H)
	{
		Peak = FMath::Max(Peak, FMath::Abs(V));
	}
	return Peak;
}

void UBRWaterSim::Recenter(const FIntPoint& NewOrigin, const ABRWorld* World)
{
	const FIntPoint Delta = NewOrigin - Origin;
	if (!bValid || FMath::Abs(Delta.X) >= GridRes || FMath::Abs(Delta.Y) >= GridRes)
	{
		// Toute la zone est nouvelle (debut du niveau, teleportation)
		Origin = NewOrigin;
		for (int32 GY = Origin.Y; GY < Origin.Y + GridRes; ++GY)
		{
			for (int32 GX = Origin.X; GX < Origin.X + GridRes; ++GX)
			{
				ResetCell(GX, GY, World);
			}
		}
		bValid = true;
		return;
	}
	// Colonnes puis lignes qui entrent dans la zone : elles reprennent les cases de celles qui en sortent
	for (int32 GX = NewOrigin.X; GX < NewOrigin.X + GridRes; ++GX)
	{
		if (GX < Origin.X || GX >= Origin.X + GridRes)
		{
			for (int32 GY = NewOrigin.Y; GY < NewOrigin.Y + GridRes; ++GY)
			{
				ResetCell(GX, GY, World);
			}
		}
	}
	for (int32 GY = NewOrigin.Y; GY < NewOrigin.Y + GridRes; ++GY)
	{
		if (GY < Origin.Y || GY >= Origin.Y + GridRes)
		{
			for (int32 GX = FMath::Max(NewOrigin.X, Origin.X); GX < FMath::Min(NewOrigin.X, Origin.X) + GridRes; ++GX)
			{
				ResetCell(GX, GY, World);
			}
		}
	}
	Origin = NewOrigin;
}

void UBRWaterSim::ResetCell(int32 GX, int32 GY, const ABRWorld* World)
{
	const int32 I = Wrap(GY) * GridRes + Wrap(GX);
	H[I] = 0.f;
	HPrev[I] = 0.f;
	Open[I] = IsBlocked((GX + 0.5f) * Texel, (GY + 0.5f) * Texel, World) ? 0 : 1;
}

uint8 UBRWaterSim::CellBits(int32 X, int32 Y, const ABRWorld* World)
{
	const FIntPoint Key(X, Y);
	if (const uint8* Found = CellCache.Find(Key))
	{
		return *Found;
	}
	if (CellCache.Num() > 8192)
	{
		CellCache.Reset();
	}
	uint8 Bits = World->IsSolid(X, Y) ? BitSolid : 0;
	Bits |= World->HasPillar(X, Y) ? BitPillar : 0;
	Bits |= static_cast<uint8>(World->EdgeE(X, Y)) << 2;
	Bits |= static_cast<uint8>(World->EdgeN(X, Y)) << 4;
	CellCache.Add(Key, Bits);
	return Bits;
}

bool UBRWaterSim::IsBlocked(float X, float Y, const ABRWorld* World)
{
	// Meme geometrie que ABRChunk : murs epais de WallThickness sur les aretes, portes de DoorWidth au milieu,
	// bouts de murs qui depassent de WallThickness / 2, piliers aux coins
	const FBRLevelDef& D = World->Def();
	const float S = D.CellSize;
	const int32 CX = FMath::FloorToInt(X / S);
	const int32 CY = FMath::FloorToInt(Y / S);
	if (CellBits(CX, CY, World) & BitSolid)
	{
		return true;
	}
	// Trottoirs au-dessus de l'eau (Niveau 37) : les vagues s'y brisent
	if (D.DeckHeight > D.WaterHeight && World->DeckZAt(FVector(X, Y, 0.f)) > D.WaterHeight)
	{
		return true;
	}
	if (D.Layout != EBRLayout::Rooms && D.Layout != EBRLayout::Maze)
	{
		return false;
	}
	const float LX = X - CX * S;
	const float LY = Y - CY * S;
	const float HT = D.WallThickness * 0.5f;
	const float DW = FMath::Min(D.DoorWidth, S - 2.f * D.WallThickness - 20.f);
	auto Blocks = [&](uint8 Edge, float Along)
	{
		return Edge == static_cast<uint8>(EBREdge::Wall)
			|| (Edge == static_cast<uint8>(EBREdge::Door) && FMath::Abs(Along - S * 0.5f) > DW * 0.5f);
	};
	if ((LX > S - HT && Blocks(EdgeE(CellBits(CX, CY, World)), LY)) || (LX < HT && Blocks(EdgeE(CellBits(CX - 1, CY, World)), LY))
		|| (LY > S - HT && Blocks(EdgeN(CellBits(CX, CY, World)), LX)) || (LY < HT && Blocks(EdgeN(CellBits(CX, CY - 1, World)), LX)))
	{
		return true;
	}
	// Coin le plus proche : pilier, ou bout d'un des quatre murs qui s'y rejoignent
	const int32 KX = FMath::RoundToInt(X / S);
	const int32 KY = FMath::RoundToInt(Y / S);
	const float DX = FMath::Abs(X - KX * S);
	const float DY = FMath::Abs(Y - KY * S);
	const float PH = D.PillarSize * 0.5f;
	if (DX < PH && DY < PH && (CellBits(KX - 1, KY - 1, World) & BitPillar))
	{
		return true;
	}
	if (DX < HT && DY < HT)
	{
		const uint8 Open8 = static_cast<uint8>(EBREdge::Open);
		return EdgeE(CellBits(KX - 1, KY - 1, World)) != Open8 || EdgeE(CellBits(KX - 1, KY, World)) != Open8
			|| EdgeN(CellBits(KX - 1, KY - 1, World)) != Open8 || EdgeN(CellBits(KX, KY - 1, World)) != Open8;
	}
	return false;
}

void UBRWaterSim::Stamp(const FStamp& S, float Scale)
{
	const float R = S.Radius;
	const int32 X0 = FMath::Max(FMath::FloorToInt((S.Pos.X - R) / Texel), Origin.X);
	const int32 X1 = FMath::Min(FMath::FloorToInt((S.Pos.X + R) / Texel), Origin.X + GridRes - 1);
	const int32 Y0 = FMath::Max(FMath::FloorToInt((S.Pos.Y - R) / Texel), Origin.Y);
	const int32 Y1 = FMath::Min(FMath::FloorToInt((S.Pos.Y + R) / Texel), Origin.Y + GridRes - 1);
	for (int32 GY = Y0; GY <= Y1; ++GY)
	{
		for (int32 GX = X0; GX <= X1; ++GX)
		{
			const float Dist = FMath::Sqrt(FMath::Square((GX + 0.5f) * Texel - S.Pos.X) + FMath::Square((GY + 0.5f) * Texel - S.Pos.Y));
			const int32 I = Wrap(GY) * GridRes + Wrap(GX);
			if (Dist < R && Open[I])
			{
				// Bosse en cosinus : pas d'arete vive, donc pas de bruit de grille
				H[I] += S.Amount * Scale * 0.5f * (1.f + FMath::Cos(PI * Dist / R));
			}
		}
	}
}

void UBRWaterSim::Step(float InStepDt)
{
	for (const FStamp& S : Impulses)
	{
		Stamp(S, 1.f);
	}
	Impulses.Reset();
	for (const FStamp& S : Movers)
	{
		Stamp(S, InStepDt);
	}

	// Equation des ondes (schema saute-mouton) : HPrev recoit le pas suivant, puis on echange les tableaux.
	// Contre un mur, la case voisine prend la hauteur de la case courante : la vague y rebondit.
	const float C2 = Courant * Courant;
	const FIntPoint O = Origin;
	const int32 IX0 = Wrap(O.X);
	ParallelFor(GridRes, [this, C2, O, IX0](int32 Row)
	{
		const int32 GY = O.Y + Row;
		const int32 IY = Wrap(GY);
		const int32 Base = IY * GridRes;
		const int32 Down = (Row > 0 ? Wrap(GY - 1) : IY) * GridRes;
		const int32 Up = (Row < GridRes - 1 ? Wrap(GY + 1) : IY) * GridRes;
		const int32 RowEdge = FMath::Min(Row, GridRes - 1 - Row);
		int32 IX = IX0;
		for (int32 Col = 0; Col < GridRes; ++Col)
		{
			const int32 I = Base + IX;
			const int32 IXL = Col > 0 ? (IX == 0 ? GridRes - 1 : IX - 1) : IX;
			const int32 IXR = Col < GridRes - 1 ? (IX == GridRes - 1 ? 0 : IX + 1) : IX;
			IX = IX == GridRes - 1 ? 0 : IX + 1;
			if (!Open[I])
			{
				HPrev[I] = 0.f;
				continue;
			}
			const float C = H[I];
			const float L = Open[Base + IXL] ? H[Base + IXL] : C;
			const float R = Open[Base + IXR] ? H[Base + IXR] : C;
			const float B = Open[Down + (I - Base)] ? H[Down + (I - Base)] : C;
			const float T = Open[Up + (I - Base)] ? H[Up + (I - Base)] : C;
			// Bornee : aucune vague de plus de 6 cm, meme si des corps s'accumulent au meme endroit
			float Next = FMath::Clamp(C + (C - HPrev[I]) * Damping + C2 * (L + R + B + T - 4.f * C), -6.f, 6.f);
			const int32 Edge = FMath::Min(RowEdge, FMath::Min(Col, GridRes - 1 - Col));
			if (Edge < Border)
			{
				Next *= 0.85f + 0.15f * static_cast<float>(Edge) / Border;
			}
			HPrev[I] = Next;
		}
	});
	Swap(H, HPrev);
}

void UBRWaterSim::Upload()
{
	if (!Texture || !Texture->GetResource())
	{
		return;
	}
	// Pentes de la surface (differences centrees) en demi-flottants : R = dH/dX, G = dH/dY
	uint8* Buffer = new uint8[GridRes * GridRes * 4];
	FFloat16* Out = reinterpret_cast<FFloat16*>(Buffer);
	const float Inv = 1.f / (2.f * Texel);
	ParallelFor(GridRes, [this, Out, Inv](int32 IY)
	{
		const int32 Base = IY * GridRes;
		const int32 Up = ((IY + 1) % GridRes) * GridRes;
		const int32 Down = ((IY + GridRes - 1) % GridRes) * GridRes;
		for (int32 IX = 0; IX < GridRes; ++IX)
		{
			const int32 I = Base + IX;
			const float C = H[I];
			const int32 L = Base + (IX + GridRes - 1) % GridRes;
			const int32 R = Base + (IX + 1) % GridRes;
			const float HL = Open[L] ? H[L] : C;
			const float HR = Open[R] ? H[R] : C;
			const float HB = Open[Down + IX] ? H[Down + IX] : C;
			const float HT = Open[Up + IX] ? H[Up + IX] : C;
			Out[I * 2] = FFloat16((HR - HL) * Inv);
			Out[I * 2 + 1] = FFloat16((HT - HB) * Inv);
		}
	});
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, GridRes, GridRes);
	Texture->UpdateTextureRegions(0, 1, Region, GridRes * 4, 4, Buffer, [](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
	{
		delete[] SrcData;
		delete Regions;
	});
}
