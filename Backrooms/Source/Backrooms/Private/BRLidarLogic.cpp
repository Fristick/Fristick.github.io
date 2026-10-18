// v4.13 : logique pure de l'aile LiDAR et du scanner (voir BRLidarLogic.h). Aucune dependance au moteur : ce fichier est
// aussi compile seul par Tools/Lidar/test_lidar.cpp.
#include "BRLidarLogic.h"
// v4.16 : cotes de la porte du passage (constantes seulement : rien a lier pour le banc)
#include "BRMechLogic.h"

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

int CorridorLength(const FWing& W)
{
	int GX = 0, GY = 0;
	CorridorEnd(W, GX, GY);
	return (GX < 0 ? -GX : GX) + (GY < 0 ? -GY : GY);
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
	S.StaticAid = In.StaticAid < 0 ? 0 : (In.StaticAid > 2 ? 2 : In.StaticAid);
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

// ------------------------------------------------------------------------------- v4.14 : reserve de points
int CategoryCapacity(int Cat)
{
	switch (Cat)
	{
	case Env: return 15000;
	case Danger: return 3000;
	case Device: return 1500;
	case Entity: return 600;
	case Marker: return 400;
	case Glyph: return 3500;
	default: return 0;
	}
}

void FPointStore::Reset()
{
	const float P = Persistence;
	*this = FPointStore();
	Persistence = P;
}

float FPointStore::BucketSeconds() const
{
	// Avec AgeBuckets - 1 periodes par persistance, le groupe reutilise par le temps a deja expire : aucun point n'est
	// remplace avant sa persistance tant que la categorie n'est pas saturee
	const float P = Persistence > 0.1f ? Persistence : 0.1f;
	return P / static_cast<float>(AgeBuckets - 1);
}

namespace
{
	void Rotate(FPointStore& S, int Cat, uint8_t& Mask)
	{
		int& Cur = S.Current[Cat];
		Cur = (Cur + 1) % AgeBuckets;
		if (S.Count[Cat][Cur] > 0)
		{
			S.Replaced += S.Count[Cat][Cur];
			S.Count[Cat][Cur] = 0;
			Mask |= static_cast<uint8_t>(1u << Cur);
		}
		S.Start[Cat][Cur] = S.Now;
		S.Last[Cat][Cur] = S.Now;
		++S.Serial[Cat];
	}
}

void FPointStore::Advance(float Dt, uint8_t OutCleared[NumCategories])
{
	Now += Dt > 0.f ? static_cast<double>(Dt) : 0.0;
	const double P = Persistence > 0.1f ? Persistence : 0.1f;
	const double BS = BucketSeconds();
	for (int Cat = 0; Cat < NumCategories; ++Cat)
	{
		uint8_t Mask = 0;
		for (int B = 0; B < AgeBuckets; ++B)
		{
			if (Count[Cat][B] > 0 && Now - Last[Cat][B] >= P)
			{
				Expired += Count[Cat][B];
				Count[Cat][B] = 0;
				Mask |= static_cast<uint8_t>(1u << B);
			}
		}
		const int Cur = Current[Cat];
		if (Count[Cat][Cur] > 0 && Now - Start[Cat][Cur] >= BS)
		{
			Rotate(*this, Cat, Mask);
		}
		OutCleared[Cat] = Mask;
	}
}

int FPointStore::Insert(int Cat, int N, int& OutBucket, uint8_t& OutCleared)
{
	OutCleared = 0;
	OutBucket = 0;
	if (Cat < 0 || Cat >= NumCategories || N <= 0)
	{
		return 0;
	}
	Requested += N;
	const int PerBucket = CategoryCapacity(Cat) / AgeBuckets;
	if (Count[Cat][Current[Cat]] > 0 && (Now - Start[Cat][Current[Cat]] >= BucketSeconds() || Count[Cat][Current[Cat]] + N > PerBucket))
	{
		// Duree ecoulee ou part du groupe atteinte : le groupe suivant (le plus ancien) est remplace, la decouverte continue
		Rotate(*this, Cat, OutCleared);
	}
	const int Cur = Current[Cat];
	const int Room = PerBucket - Count[Cat][Cur];
	const int Got = N < Room ? N : (Room > 0 ? Room : 0);
	Rejected += N - Got;
	if (Got > 0)
	{
		if (Count[Cat][Cur] == 0)
		{
			Start[Cat][Cur] = Now;
		}
		Count[Cat][Cur] += Got;
		Last[Cat][Cur] = Now;
		Accepted += Got;
	}
	OutBucket = Cur;
	return Got;
}

int FPointStore::Total() const
{
	int T = 0;
	for (int Cat = 0; Cat < NumCategories; ++Cat)
	{
		T += Total(Cat);
	}
	return T;
}

int FPointStore::Total(int Cat) const
{
	int T = 0;
	for (int B = 0; B < AgeBuckets; ++B)
	{
		T += Count[Cat][B];
	}
	return T;
}

float FPointStore::FadeOf(int Cat, int B) const
{
	if (Cat < 0 || Cat >= NumCategories || B < 0 || B >= AgeBuckets || Count[Cat][B] <= 0 || Persistence <= 0.f)
	{
		return 0.f;
	}
	const double A = Now - Last[Cat][B];
	if (A >= Persistence)
	{
		return 0.f;
	}
	// Plein eclat pendant le premier quart, puis extinction lineaire
	const float T = static_cast<float>(A / Persistence);
	return T < 0.25f ? 1.f : ClampF(1.f - (T - 0.25f) / 0.75f, 0.f, 1.f);
}

int FRateBudget::Take(float Dt)
{
	Carry += Rate * (Dt > 0.f ? Dt : 0.f);
	int N = static_cast<int>(Carry);
	if (N > MaxPerFrame)
	{
		N = MaxPerFrame;
	}
	Carry -= static_cast<float>(N);
	if (Carry > 1.f)
	{
		Carry = 1.f; // pas d'arriere : une image lente ne declenche pas de rattrapage
	}
	return N;
}

// ------------------------------------------------------------------------------------------- Inscriptions
namespace
{
	/** Grilles 7 x 7, ligne du haut d'abord : triangle, cercle, carre, losange, croix, etoile a six branches */
	const char* const SymbolMask[NumSymbols] = {
		"...#.....#.#....#.#...#...#..#...#.#.....########",
		"..###...#...#.#.....##.....##.....#.#...#...###..",
		"########.....##.....##.....##.....##.....########",
		"...#.....#.#...#...#.#.....#.#...#...#.#.....#...",
		"...#......#......#...#######...#......#......#...",
		".#...#...#.#....#.#..#######..#.#....#.#...#...#." };

	int ClampSymbol(int S)
	{
		return S < 0 ? 0 : (S >= NumSymbols ? NumSymbols - 1 : S);
	}
}

bool SymbolCell(int Symbol, int Row, int Col)
{
	if (Row < 0 || Row >= 7 || Col < 0 || Col >= 7)
	{
		return false;
	}
	return SymbolMask[ClampSymbol(Symbol)][Row * 7 + Col] == '#';
}

int SymbolCellCount(int Symbol)
{
	int N = 0;
	for (int I = 0; I < 49; ++I)
	{
		N += SymbolMask[ClampSymbol(Symbol)][I] == '#' ? 1 : 0;
	}
	return N;
}

int PlateGlyphPoints(int Symbol)
{
	return PlateFigurePoints + SymbolCellCount(Symbol);
}

bool PlateGlyphPoint(int Symbol, int K, int FigurePoints, int& OutFigure, int& OutRow, int& OutCol)
{
	OutFigure = OutRow = OutCol = 0;
	if (K < PlateFigurePoints)
	{
		OutFigure = (K * (FigurePoints > 0 ? FigurePoints : 1)) / PlateFigurePoints;
		return false;
	}
	int Cell = K - PlateFigurePoints;
	const char* M = SymbolMask[ClampSymbol(Symbol)];
	for (int I = 0; I < 49; ++I)
	{
		if (M[I] == '#' && Cell-- == 0)
		{
			OutRow = I / 7;
			OutCol = I % 7;
			return true;
		}
	}
	return true;
}

int CageGlyphFigure(int K, int FigurePoints)
{
	return (K * (FigurePoints > 0 ? FigurePoints : 1)) / CageFigurePoints;
}

float GlyphRefreshSeconds(float Persistence)
{
	const float R = Persistence * 0.6f;
	return R > 2.f ? R : 2.f;
}

void FGlyphPlanner::Reset()
{
	*this = FGlyphPlanner();
}

void FGlyphPlanner::Touch(int Id, int Total, double Now, float Persistence, unsigned Pose)
{
	++Touches;
	if (Id < 0 || Total <= 0)
	{
		return;
	}
	FTarget* T = nullptr;
	for (FTarget& X : Targets)
	{
		if (X.Id == Id)
		{
			T = &X;
			break;
		}
	}
	if (!T)
	{
		// Nouvelle cible : une place libre, sinon la moins recemment touchee
		T = &Targets[0];
		for (FTarget& X : Targets)
		{
			if (X.Id < 0)
			{
				T = &X;
				break;
			}
			if (X.TouchedAt < T->TouchedAt)
			{
				T = &X;
			}
		}
		*T = FTarget();
		T->Id = Id;
		T->Pose = Pose;
	}
	if (!T->bTouched)
	{
		++DistinctTouches;
	}
	T->Total = Total;
	if (T->Pose != Pose)
	{
		// v4.16 : le support a bouge depuis cette passe (porte ouverte) : nouvelle passe tout de suite, a la pose actuelle
		T->Pose = Pose;
		T->Cursor = 0;
		++PoseRestarts;
	}
	else if (T->Cursor >= T->Total && Now - T->DoneAt >= GlyphRefreshSeconds(Persistence))
	{
		T->Cursor = 0; // nouvelle passe : l'inscription est redessinee avant de s'eteindre
	}
	T->bTouched = true;
	T->TouchedAt = Now;
}

int FGlyphPlanner::Plan(float Dt, double Now, FGlyphEmit* Out, int MaxOut, int* OutCompleted, int& NumCompleted)
{
	NumCompleted = 0;
	int Left = Budget.Take(Dt);
	int NumOut = 0;
	int Active = 0;
	for (const FTarget& T : Targets)
	{
		Active += (T.bTouched && T.Cursor < T.Total) ? 1 : 0;
	}
	// Partage equitable du budget entre les cibles en cours, puis le reste a qui en a encore besoin
	for (int Pass = 0; Pass < 2 && Left > 0 && Active > 0; ++Pass)
	{
		const int Share = Pass == 0 ? (Left / Active > 0 ? Left / Active : 1) : Left;
		for (FTarget& T : Targets)
		{
			if (!T.bTouched || T.Cursor >= T.Total || Left <= 0 || NumOut >= MaxOut)
			{
				continue;
			}
			int N = T.Total - T.Cursor;
			N = N < Share ? N : Share;
			N = N < Left ? N : Left;
			if (N <= 0)
			{
				continue;
			}
			Out[NumOut].Target = T.Id;
			Out[NumOut].First = T.Cursor;
			Out[NumOut].Count = N;
			++NumOut;
			T.Cursor += N;
			Left -= N;
			if (T.Cursor >= T.Total)
			{
				T.DoneAt = Now;
				if (OutCompleted && NumCompleted < MaxGlyphTargets)
				{
					OutCompleted[NumCompleted++] = T.Id;
				}
			}
		}
	}
	for (FTarget& T : Targets)
	{
		T.bTouched = false;
	}
	return NumOut;
}

// ----------------------------------------------------------------------------------------------- Image
void FScanState::Reset()
{
	const float P = Store.Persistence;
	*this = FScanState();
	Store.Persistence = P;
}

bool FScanState::StartPulse(float Phase)
{
	if (Cooldown > 0.f)
	{
		return false;
	}
	Cooldown = PulseCooldown;
	PulseNext = 0;
	PulsePhase = Phase;
	PulseBudget.Reset();
	return true;
}

void RunFrame(FScanState& S, float Dt, float Persistence, bool bFocus, IScanHost& Host, FFrameResult& Out)
{
	Out = FFrameResult();
	++S.Frame;
	S.Cooldown = S.Cooldown - Dt > 0.f ? S.Cooldown - Dt : 0.f;
	S.Store.Persistence = Persistence;
	S.Store.Advance(Dt, Out.Cleared);
	// Impulsion : PulseRays rayons en PulseSeconds, plafond par image
	if (S.PulseNext >= 0)
	{
		const int N = S.PulseBudget.Take(Dt);
		const int End = S.PulseNext + N < PulseRays ? S.PulseNext + N : PulseRays;
		for (int I = S.PulseNext; I < End; ++I)
		{
			float X, Y, Z;
			PulseDir(I, PulseRays, S.PulsePhase, X, Y, Z);
			Host.TraceRay(true, false, X, Y, Z);
		}
		Out.PulseRays = End - S.PulseNext;
		S.PulseNext = End >= PulseRays ? -1 : End;
	}
	// Scan focalise : FocusRaysPerSecond rayons, un sur FocusCenterEvery dans l'axe ; les cibles a inscriptions touchees
	// par plusieurs rayons ne comptent qu'une fois
	if (bFocus)
	{
		const int N = S.FocusBudget.Take(Dt);
		for (int I = 0; I < N; ++I)
		{
			const bool bCenter = (S.FocusCount++ % FocusCenterEvery) == 0;
			float X = 1.f, Y = 0.f, Z = 0.f;
			if (!bCenter)
			{
				FocusDir(I, S.Frame, X, Y, Z);
			}
			const FHit H = Host.TraceRay(false, bCenter, X, Y, Z);
			if (H.GlyphTarget >= 0)
			{
				S.Glyphs.Touch(H.GlyphTarget, H.GlyphTotal, S.Store.Now, Persistence, H.GlyphPose);
			}
		}
		Out.FocusRays = N;
	}
	else
	{
		S.FocusBudget.Reset();
	}
	FGlyphEmit Emits[MaxGlyphTargets * 2];
	const int NumEmits = S.Glyphs.Plan(Dt, S.Store.Now, Emits, MaxGlyphTargets * 2, Out.Completed, Out.NumCompleted);
	for (int E = 0; E < NumEmits; ++E)
	{
		Out.GlyphPoints += Host.EmitGlyphs(Emits[E].Target, Emits[E].First, Emits[E].Count);
	}
	for (int Cat = 0; Cat < NumCategories; ++Cat)
	{
		const int N = Host.PendingPoints(Cat);
		Out.Requested[Cat] = N;
		if (N > 0)
		{
			uint8_t Mask = 0;
			Out.Accepted[Cat] = S.Store.Insert(Cat, N, Out.Bucket[Cat], Mask);
			Out.Cleared[Cat] |= Mask;
		}
		else
		{
			Out.Bucket[Cat] = S.Store.Current[Cat];
		}
	}
}

// ------------------------------------------------- v4.16 : inscriptions sur un support mobile, surfaces du scanner
FFrame3 YawFrame(float OX, float OY, float OZ, float YawDeg)
{
	const float A = YawDeg * (Pi / 180.f);
	const float C = std::cos(A);
	const float S = std::sin(A);
	FFrame3 F;
	F.O[0] = OX;
	F.O[1] = OY;
	F.O[2] = OZ;
	F.X[0] = C;
	F.X[1] = S;
	F.X[2] = 0.f;
	F.Y[0] = -S;
	F.Y[1] = C;
	F.Y[2] = 0.f;
	F.Z[0] = 0.f;
	F.Z[1] = 0.f;
	F.Z[2] = 1.f;
	return F;
}

void PointToParent(const FFrame3& F, const float L[3], float Out[3])
{
	for (int I = 0; I < 3; ++I)
	{
		Out[I] = F.O[I] + F.X[I] * L[0] + F.Y[I] * L[1] + F.Z[I] * L[2];
	}
}

void DirToParent(const FFrame3& F, const float L[3], float Out[3])
{
	for (int I = 0; I < 3; ++I)
	{
		Out[I] = F.X[I] * L[0] + F.Y[I] * L[1] + F.Z[I] * L[2];
	}
}

void DirToLocal(const FFrame3& F, const float P[3], float Out[3])
{
	// Axes orthonormes : l'inverse de la rotation est sa transposee
	Out[0] = F.X[0] * P[0] + F.X[1] * P[1] + F.X[2] * P[2];
	Out[1] = F.Y[0] * P[0] + F.Y[1] * P[1] + F.Y[2] * P[2];
	Out[2] = F.Z[0] * P[0] + F.Z[1] * P[1] + F.Z[2] * P[2];
}

void PointToLocal(const FFrame3& F, const float P[3], float Out[3])
{
	const float D[3] = { P[0] - F.O[0], P[1] - F.O[1], P[2] - F.O[2] };
	DirToLocal(F, D, Out);
}

FFrame3 Compose(const FFrame3& Parent, const FFrame3& Child)
{
	FFrame3 F;
	PointToParent(Parent, Child.O, F.O);
	DirToParent(Parent, Child.X, F.X);
	DirToParent(Parent, Child.Y, F.Y);
	DirToParent(Parent, Child.Z, F.Z);
	return F;
}

bool FrameMoved(const FFrame3& A, const FFrame3& B, float PosTol, float AxisTol)
{
	float DO = 0.f, DX = 0.f, DY = 0.f, DZ = 0.f;
	for (int I = 0; I < 3; ++I)
	{
		DO += (A.O[I] - B.O[I]) * (A.O[I] - B.O[I]);
		DX += (A.X[I] - B.X[I]) * (A.X[I] - B.X[I]);
		DY += (A.Y[I] - B.Y[I]) * (A.Y[I] - B.Y[I]);
		DZ += (A.Z[I] - B.Z[I]) * (A.Z[I] - B.Z[I]);
	}
	const float AT = AxisTol * AxisTol;
	return DO > PosTol * PosTol || DX > AT || DY > AT || DZ > AT;
}

void FSupportTrack::Update(const FFrame3& Now)
{
	if (!bInit)
	{
		bInit = true;
		Last = Now;
		RevPose = Now;
		bMoving = false;
		return;
	}
	bMoving = FrameMoved(Last, Now, SupportStillPos, SupportStillAxis);
	if (FrameMoved(RevPose, Now, SupportRevPos, SupportRevAxis))
	{
		++Rev;
		RevPose = Now;
	}
	Last = Now;
}

FFrame3 DoorLeafFrame(float Progress, float ShakeDeg)
{
	// Charnieres a -Y : le battant commence a 1 cm du bord de l'ouverture (BuildLidarDoor), pivote de OpenAngle vers l'aile
	namespace LD = BRMech::LidarDoor;
	return YawFrame(0.f, -LD::Width * 0.5f + 1.f, 0.f, LD::OpenAngle * Progress + ShakeDeg);
}

FFrame3 DoorPlateFrame()
{
	// Milieu du battant, 2 cm devant sa face cote galerie (0,45 cm devant la plaque en relief), a PlateZ
	namespace LD = BRMech::LidarDoor;
	FFrame3 F;
	F.O[0] = LD::LeafThick * 0.5f + 2.f;
	F.O[1] = (LD::Width - 4.f) * 0.5f;
	F.O[2] = LD::PlateZ;
	return F;
}

void DoorScanBox(int I, float OutMin[3], float OutMax[3])
{
	namespace LD = BRMech::LidarDoor;
	const float LW = LD::Width - 4.f;
	if (I == 1)
	{
		// Plaque en relief : 1,5 cm d'epaisseur, posee 0,8 cm devant le centre de la face du battant
		const float CX = LD::LeafThick * 0.5f + 0.8f;
		OutMin[0] = CX - 0.75f;
		OutMax[0] = CX + 0.75f;
		OutMin[1] = LW * 0.5f - LD::PlateSize * 0.5f;
		OutMax[1] = LW * 0.5f + LD::PlateSize * 0.5f;
		OutMin[2] = LD::PlateZ - LD::PlateSize * 0.5f;
		OutMax[2] = LD::PlateZ + LD::PlateSize * 0.5f;
		return;
	}
	// Battant (LW x Height - 4, de 1 a Height - 3 cm), elargi au jeu de la porte : de l'axe des charnieres (1 cm du bord de
	// l'ouverture) au bord oppose, du sol au linteau. Porte fermee, il couvre exactement l'ouverture du mur
	OutMin[0] = -LD::LeafThick * 0.5f;
	OutMax[0] = LD::LeafThick * 0.5f;
	OutMin[1] = -1.f;
	OutMax[1] = LD::Width - 1.f;
	OutMin[2] = 0.f;
	OutMax[2] = LD::Height;
}

bool RayBox(const FFrame3& F, const float Min[3], const float Max[3], const float From[3], const float Dir[3], float MaxT, float& OutT, float OutN[3])
{
	float O[3], D[3];
	PointToLocal(F, From, O);
	DirToLocal(F, Dir, D);
	float TNear = -1e30f, TFar = 1e30f;
	int Axis = -1;
	float Sign = 0.f;
	for (int I = 0; I < 3; ++I)
	{
		if (std::fabs(D[I]) < 1e-8f)
		{
			if (O[I] < Min[I] || O[I] > Max[I])
			{
				return false;
			}
			continue;
		}
		const float Inv = 1.f / D[I];
		float T0 = (Min[I] - O[I]) * Inv;
		float T1 = (Max[I] - O[I]) * Inv;
		// Entree par la face Min si le rayon va vers +I (normale -I), par la face Max sinon (normale +I)
		float S = -1.f;
		if (T0 > T1)
		{
			const float Tmp = T0;
			T0 = T1;
			T1 = Tmp;
			S = 1.f;
		}
		if (T0 > TNear)
		{
			TNear = T0;
			Axis = I;
			Sign = S;
		}
		TFar = T1 < TFar ? T1 : TFar;
		if (TNear > TFar)
		{
			return false;
		}
	}
	if (Axis < 0 || TNear <= 0.f || TNear > MaxT)
	{
		return false; // parti de l'interieur, derriere le point de depart ou au-dela de la portee
	}
	float NL[3] = { 0.f, 0.f, 0.f };
	NL[Axis] = Sign;
	DirToParent(F, NL, OutN);
	OutT = TNear;
	return true;
}

float BoxDistance(const FFrame3& F, const float Min[3], const float Max[3], const float P[3])
{
	float L[3];
	PointToLocal(F, P, L);
	float D2 = 0.f;
	for (int I = 0; I < 3; ++I)
	{
		const float E = L[I] < Min[I] ? Min[I] - L[I] : (L[I] > Max[I] ? L[I] - Max[I] : 0.f);
		D2 += E * E;
	}
	return std::sqrt(D2);
}

void PlateShapePoint(int Symbol, int K, const short (*Figure)[3], int FigurePoints, float& OutRight, float& OutUp)
{
	// A gauche la silhouette (40 cm de haut), a droite le code (grille 7 x 7, 18 cm) : memes cotes qu'en v4.14
	int FigIdx = 0, Row = 0, Col = 0;
	if (PlateGlyphPoint(Symbol, K, FigurePoints, FigIdx, Row, Col))
	{
		OutRight = 12.f + (Col - 3) * 2.8f;
		OutUp = (3 - Row) * 2.8f;
		return;
	}
	const short* Pt = Figure[FigIdx < FigurePoints ? FigIdx : FigurePoints - 1];
	OutRight = -12.f + Pt[1] * 0.1f * 0.22f;
	OutUp = Pt[2] * 0.1f * 0.22f - 18.f;
}

void CageShapePoint(int K, const short (*Figure)[3], int FigurePoints, float Out[3])
{
	// Silhouette debout dans la cage (1,8 m au plus : la cage mesure 2,1 m), face a l'avant de la cage
	const int Idx = CageGlyphFigure(K, FigurePoints);
	const short* Pt = Figure[Idx < FigurePoints ? Idx : FigurePoints - 1];
	Out[0] = Pt[0] * 0.09f;
	Out[1] = Pt[1] * 0.09f;
	Out[2] = Pt[2] * 0.09f;
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
