// v4.13 : logique pure de l'aile LiDAR et du scanner (voir BRLidarLogic.h). Aucune dependance au moteur : ce fichier est
// aussi compile seul par Tools/Lidar/test_lidar.cpp.
#include "BRLidarLogic.h"

#include <cmath>

namespace BRLidar
{
namespace
{
	inline uint32_t Mix(uint32_t X)
	{
		X ^= X >> 16;
		X *= 0x7feb352dU;
		X ^= X >> 15;
		X *= 0x846ca68bU;
		X ^= X >> 16;
		return X;
	}

	inline float ClampF(float V, float Lo, float Hi)
	{
		return V < Lo ? Lo : (V > Hi ? Hi : V);
	}

	constexpr float Pi = 3.14159265358979f;
	constexpr float GoldenAngle = 2.39996322972865f;
}

uint32_t Hash3(int32_t A, int32_t B, int32_t C, uint32_t Seed)
{
	uint32_t H = Mix(static_cast<uint32_t>(A) * 0x9E3779B1u ^ Seed);
	H = Mix(H ^ (static_cast<uint32_t>(B) * 0x85EBCA77u));
	H = Mix(H ^ (static_cast<uint32_t>(C) * 0xC2B2AE3Du));
	return H;
}

FWing PickWing(uint32_t LevelSeed, int ChunkCells)
{
	FWing W;
	const int N = ChunkCells;
	if (N < WingCells + 2)
	{
		return W;
	}
	// Chunks dont le centre est a WingRing chunks du depart (cellule 0, 0), dans un ordre fixe
	int Cands[64][2];
	int NC = 0;
	const int Lo = -WingRing - 1;
	const int Hi = WingRing;
	for (int CX = Lo; CX <= Hi; ++CX)
	{
		for (int CY = Lo; CY <= Hi; ++CY)
		{
			if (CX == Lo || CX == Hi || CY == Lo || CY == Hi)
			{
				Cands[NC][0] = CX;
				Cands[NC][1] = CY;
				++NC;
			}
		}
	}
	const int Pick = static_cast<int>(Hash3(0, 0, 6600, LevelSeed) % static_cast<uint32_t>(NC));
	W.ChunkX = Cands[Pick][0];
	W.ChunkY = Cands[Pick][1];
	// Une galerie d'au moins une cellule autour de l'aile, comme les salles de fosses
	const int Free = N - WingCells - 2;
	W.MinX = W.ChunkX * N + 1 + static_cast<int>(Hash3(W.ChunkX, W.ChunkY, 6601, LevelSeed) % static_cast<uint32_t>(Free + 1));
	W.MinY = W.ChunkY * N + 1 + static_cast<int>(Hash3(W.ChunkX, W.ChunkY, 6602, LevelSeed) % static_cast<uint32_t>(Free + 1));
	// Entree tournee vers le depart
	const int CX = W.ChunkX * N + N / 2;
	const int CY = W.ChunkY * N + N / 2;
	if ((CX < 0 ? -CX : CX) >= (CY < 0 ? -CY : CY))
	{
		W.OutX = CX > 0 ? -1 : 1;
		W.OutY = 0;
	}
	else
	{
		W.OutX = 0;
		W.OutY = CY > 0 ? -1 : 1;
	}
	W.LadderSlot = static_cast<int>(Hash3(W.ChunkX, W.ChunkY, 6603, LevelSeed) % static_cast<uint32_t>(WingCells));
	W.bValid = true;
	return W;
}

bool Contains(const FWing& W, int X, int Y)
{
	return W.bValid && X >= W.MinX && X < W.MinX + WingCells && Y >= W.MinY && Y < W.MinY + WingCells;
}

bool ToLocal(const FWing& W, int X, int Y, int& OutU, int& OutV)
{
	OutU = OutV = -1;
	if (!Contains(W, X, Y))
	{
		return false;
	}
	const int LX = X - W.MinX;
	const int LY = Y - W.MinY;
	if (W.OutX == -1)
	{
		OutU = LX;
		OutV = LY;
	}
	else if (W.OutX == 1)
	{
		OutU = WingCells - 1 - LX;
		OutV = LY;
	}
	else if (W.OutY == -1)
	{
		OutU = LY;
		OutV = LX;
	}
	else
	{
		OutU = WingCells - 1 - LY;
		OutV = LX;
	}
	return true;
}

void ToCell(const FWing& W, int U, int V, int& OutX, int& OutY)
{
	if (W.OutX == -1)
	{
		OutX = W.MinX + U;
		OutY = W.MinY + V;
	}
	else if (W.OutX == 1)
	{
		OutX = W.MinX + WingCells - 1 - U;
		OutY = W.MinY + V;
	}
	else if (W.OutY == -1)
	{
		OutX = W.MinX + V;
		OutY = W.MinY + U;
	}
	else
	{
		OutX = W.MinX + V;
		OutY = W.MinY + WingCells - 1 - U;
	}
}

EEdge EdgeBetween(const FWing& W, int AX, int AY, int BX, int BY)
{
	const bool bInA = Contains(W, AX, AY);
	const bool bInB = Contains(W, BX, BY);
	if (!bInA && !bInB)
	{
		return Open; // galerie
	}
	if (bInA != bInB)
	{
		// Pourtour : une ouverture par cellule du cote d'entree (portes du passage), des murs ailleurs
		const int DX = bInA ? BX - AX : AX - BX;
		const int DY = bInA ? BY - AY : AY - BY;
		return (DX == W.OutX && DY == W.OutY) ? Door : Wall;
	}
	int UA, VA, UB, VB;
	ToLocal(W, AX, AY, UA, VA);
	ToLocal(W, BX, BY, UB, VB);
	if (UA != UB)
	{
		// Rangee 2 -> alcoves : une ouverture par alcove
		return (UA > UB ? UA : UB) == WingCells - 1 ? Door : Open;
	}
	// Alcoves separees par des murs
	return UA == WingCells - 1 ? Wall : Open;
}

bool HoleAtCorner(const FWing& W, int X, int Y)
{
	// Coin interieur : les quatre cellules qui le touchent sont dans l'aile
	if (!Contains(W, X, Y) || !Contains(W, X + 1, Y + 1))
	{
		return false;
	}
	int U0, V0, U1, V1;
	ToLocal(W, X, Y, U0, V0);
	ToLocal(W, X + 1, Y + 1, U1, V1);
	const int UMin = U0 < U1 ? U0 : U1;
	// Limites de profondeur 1 et 2 (entre les rangees 0-1 et 1-2) : six trous ; jamais devant les alcoves
	return UMin == 0 || UMin == 1;
}

void CorridorEnd(const FWing& W, int& OutX, int& OutY)
{
	ToCell(W, 0, 1, OutX, OutY);
	OutX += W.OutX;
	OutY += W.OutY;
}

bool CorridorEdge(const FWing& W, int AX, int AY, int BX, int BY)
{
	if (!W.bValid || (AX - BX) * (AX - BX) + (AY - BY) * (AY - BY) != 1)
	{
		return false;
	}
	int GX, GY;
	CorridorEnd(W, GX, GY);
	auto Between = [](int V, int A, int B) { return (A <= B) ? (V >= A && V <= B) : (V >= B && V <= A); };
	// Entree le long de X : d'abord le long de Y (X = 0), puis le long de X (Y = GY) ; sinon l'inverse
	auto Seg1 = [&](int X, int Y) { return W.OutX != 0 ? (X == 0 && Between(Y, 0, GY)) : (Y == 0 && Between(X, 0, GX)); };
	auto Seg2 = [&](int X, int Y) { return W.OutX != 0 ? (Y == GY && Between(X, 0, GX)) : (X == GX && Between(Y, 0, GY)); };
	return (Seg1(AX, AY) && Seg1(BX, BY)) || (Seg2(AX, AY) && Seg2(BX, BY));
}

FSettings Clamp(const FSettings& In)
{
	FSettings S;
	S.PointSize = ClampF(In.PointSize, 0.5f, 2.f);
	S.Contrast = ClampF(In.Contrast, 0.5f, 2.f);
	S.Persistence = ClampF(In.Persistence, 4.f, 30.f);
	return S;
}

void PulseDir(int I, int N, float Phase, float& OutX, float& OutY, float& OutZ)
{
	// Point I de la spirale de Vogel dans le disque unite, etire a l'ellipse du cone
	const float R = std::sqrt((static_cast<float>(I) + 0.5f) / static_cast<float>(N < 1 ? 1 : N));
	const float A = static_cast<float>(I) * GoldenAngle + Phase;
	const float Yaw = R * std::cos(A) * PulseHalfYaw * Pi / 180.f;
	const float Pitch = R * std::sin(A) * PulseHalfPitch * Pi / 180.f;
	OutX = std::cos(Pitch) * std::cos(Yaw);
	OutY = std::cos(Pitch) * std::sin(Yaw);
	OutZ = std::sin(Pitch);
}

void FocusDir(int I, uint32_t Frame, float& OutX, float& OutY, float& OutZ)
{
	// Disque uniforme dans le cone, decale a chaque image (les points comblent les vides en restant appuye)
	const uint32_t H = Hash3(I, static_cast<int32_t>(Frame), 6610, 0x5CA11u);
	const float U1 = static_cast<float>(H & 0xFFFFu) / 65536.f;
	const float U2 = static_cast<float>((H >> 16) & 0xFFFFu) / 65536.f;
	const float R = std::sqrt(U1) * FocusHalfAngle * Pi / 180.f;
	const float A = U2 * 2.f * Pi;
	const float Yaw = R * std::cos(A);
	const float Pitch = R * std::sin(A);
	OutX = std::cos(Pitch) * std::cos(Yaw);
	OutY = std::cos(Pitch) * std::sin(Yaw);
	OutZ = std::sin(Pitch);
}

bool FRing::Advance(float Dt, int& OutCleared)
{
	OutCleared = -1;
	CurrentAge += Dt > 0.f ? Dt : 0.f;
	if (CurrentAge < BucketSeconds)
	{
		return false;
	}
	CurrentAge = 0.f;
	Current = (Current + 1) % AgeBuckets;
	OutCleared = Current;
	Count[Current] = 0;
	return true;
}

int FRing::Reserve(int N)
{
	const int PerBucket = Capacity / AgeBuckets;
	const int Room = PerBucket - Count[Current];
	const int Got = N < Room ? N : (Room > 0 ? Room : 0);
	Count[Current] += Got;
	return Got;
}

int FRing::Total() const
{
	int T = 0;
	for (int B = 0; B < AgeBuckets; ++B)
	{
		T += Count[B];
	}
	return T;
}

float FRing::AgeOf(int B) const
{
	const int Steps = (Current - B + AgeBuckets) % AgeBuckets;
	return static_cast<float>(Steps) * BucketSeconds + CurrentAge;
}

float FRing::FadeOf(int B, float Persistence) const
{
	const float A = AgeOf(B);
	if (Persistence <= 0.f || A >= Persistence)
	{
		return 0.f;
	}
	// Plein eclat pendant le premier quart, puis extinction lineaire
	const float T = A / Persistence;
	return T < 0.25f ? 1.f : ClampF(1.f - (T - 0.25f) / 0.75f, 0.f, 1.f);
}

float StaticLevel(float Distance)
{
	if (Distance >= StaticRange)
	{
		return 0.f;
	}
	const float T = 1.f - (Distance < 0.f ? 0.f : Distance) / StaticRange;
	return T * T;
}
}
