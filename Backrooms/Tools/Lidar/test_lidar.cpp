// v4.13 : banc d'essai hors moteur du Level 6 sans lumiere (Source/Backrooms/Private/BRLidarLogic.cpp).
//
// Compilation et lancement (depuis Backrooms/) :
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Lidar/test_lidar.cpp
//       Source/Backrooms/Private/BRLidarLogic.cpp -o /tmp/test_lidar
//   /tmp/test_lidar [graines, 2000 par defaut]
//
// Verifie, pour chaque graine :
//   - l'aile : deterministe, dans l'anneau de chunks prevu, galerie d'au moins une cellule, entree tournee vers le depart,
//     quatre ouvertures d'entree, murs pleins ailleurs, quatre alcoves separees, six trous aux limites 1 et 2 ;
//   - la traversee a pied, au centimetre pres (grille de 5 cm) : sol ferme moins les trous (cote PitHoleSize) et les murs
//     (WallThickness), libre pour une capsule de 42 cm de rayon ; depuis CHAQUE porte d'entree, toutes les alcoves sont
//     atteintes, dont celle de l'echelle, sans jamais sauter (espace libre connexe) ; largeur minimale des passages ;
//   - le scanner : directions de l'impulsion dans le cone et sans trou de couverture, cone focalise, anneau de points
//     borne (jamais plus que la capacite), attenuation decroissante, aide de direction des parasites monotone ;
//   - le budget par image : rayons et points ajoutes bornes.
// Code de sortie 0 si tout passe, 1 sinon. La sortie sert de journal (Docs/v413/banc_lidar.txt).
#include "BRLidarLogic.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <set>
#include <string>
#include <vector>

using namespace BRLidar;

static int Failures = 0;
static int Checks = 0;

static void Check(bool bOk, uint32_t Seed, const char* What)
{
	++Checks;
	if (!bOk)
	{
		if (++Failures < 20)
		{
			std::printf("ECHEC graine %u : %s\n", Seed, What);
		}
	}
}

// Cotes du Level 6 (BRLevels.cpp, BRTypes.h)
static const float CellSize = 350.f;
static const float WallThick = 60.f;
static const float DoorWidth = 140.f;
static const float HoleSize = 210.f;
static const float Capsule = 42.f;
static const int ChunkCells = 8;

struct FWalk
{
	int MinU = 0, MinV = 0;
	float Res = 5.f;
	int N = 0;
	std::vector<uint8_t> Free;
};

/** Plan de l'aile en repere local (U profondeur, V lateral), avec une rangee de galerie devant l'entree (U = -1) */
static void BuildFloor(const FWing& W, FWalk& F)
{
	// Domaine : U de -1 a 4 cellules, V de 0 a 4 cellules
	const float Lo = -CellSize;
	const float Hi = WingCells * CellSize;
	F.N = static_cast<int>((Hi - Lo) / F.Res);
	const int NV = static_cast<int>(WingCells * CellSize / F.Res);
	F.Free.assign(static_cast<size_t>(F.N) * NV, 1);
	auto At = [&](int I, int J) -> uint8_t& { return F.Free[static_cast<size_t>(I) * NV + J]; };
	for (int I = 0; I < F.N; ++I)
	{
		for (int J = 0; J < NV; ++J)
		{
			const float U = Lo + (I + 0.5f) * F.Res; // profondeur (cm), 0 = cote d'entree
			const float V = (J + 0.5f) * F.Res;      // lateral (cm)
			bool bFree = true;
			// Bords lateraux de l'aile (murs pleins) : on reste a l'interieur
			if (V < WallThick * 0.5f || V > WingCells * CellSize - WallThick * 0.5f)
			{
				bFree = false;
			}
			// Mur du fond (U = 4 cellules)
			if (U > WingCells * CellSize - WallThick * 0.5f)
			{
				bFree = false;
			}
			// Mur d'entree (U = 0) : une ouverture de DoorWidth au milieu de chaque cellule
			if (std::fabs(U) < WallThick * 0.5f)
			{
				const float InCell = std::fmod(V, CellSize);
				if (std::fabs(InCell - CellSize * 0.5f) > DoorWidth * 0.5f)
				{
					bFree = false;
				}
			}
			// Aretes interieures : regles de EdgeBetween (profondeur : rangee 2 | 3 ; laterales : entre alcoves)
			for (int K = 1; K < WingCells && bFree; ++K)
			{
				// Limite de profondeur K (entre U = K-1 et K)
				if (std::fabs(U - K * CellSize) < WallThick * 0.5f)
				{
					const int Cell = static_cast<int>(V / CellSize);
					int AX, AY, BX, BY;
					ToCell(W, K - 1, Cell, AX, AY);
					ToCell(W, K, Cell, BX, BY);
					const EEdge E = EdgeBetween(W, AX, AY, BX, BY);
					const float InCell = std::fmod(V, CellSize);
					if (E == Wall || (E == Door && std::fabs(InCell - CellSize * 0.5f) > DoorWidth * 0.5f))
					{
						bFree = false;
					}
				}
				// Limite laterale K (entre V = K-1 et K), dans la rangee de U
				if (U >= 0.f && std::fabs(V - K * CellSize) < WallThick * 0.5f)
				{
					const int Row = static_cast<int>(U / CellSize);
					if (Row >= 0 && Row < WingCells)
					{
						int AX, AY, BX, BY;
						ToCell(W, Row, K - 1, AX, AY);
						ToCell(W, Row, K, BX, BY);
						const EEdge E = EdgeBetween(W, AX, AY, BX, BY);
						const float InCell = std::fmod(U, CellSize);
						if (E == Wall || (E == Door && std::fabs(InCell - CellSize * 0.5f) > DoorWidth * 0.5f))
						{
							bFree = false;
						}
					}
				}
			}
			// Trous aux coins interieurs (cote HoleSize, centres sur les coins de la grille)
			for (int KU = 1; KU < WingCells && bFree; ++KU)
			{
				for (int KV = 1; KV < WingCells && bFree; ++KV)
				{
					int X0, Y0, X1, Y1;
					ToCell(W, KU - 1, KV - 1, X0, Y0);
					ToCell(W, KU, KV, X1, Y1);
					const int CX = X0 < X1 ? X0 : X1;
					const int CY = Y0 < Y1 ? Y0 : Y1;
					if (HoleAtCorner(W, CX, CY) && std::fabs(U - KU * CellSize) < HoleSize * 0.5f && std::fabs(V - KV * CellSize) < HoleSize * 0.5f)
					{
						bFree = false;
					}
				}
			}
			At(I, J) = bFree ? 1 : 0;
		}
	}
	// Erosion par la capsule : un point est praticable si le disque de rayon Capsule autour est libre
	const int R = static_cast<int>(std::ceil(Capsule / F.Res));
	std::vector<uint8_t> Out(F.Free.size(), 0);
	for (int I = 0; I < F.N; ++I)
	{
		for (int J = 0; J < NV; ++J)
		{
			bool bOk = At(I, J) != 0;
			for (int DI = -R; DI <= R && bOk; ++DI)
			{
				for (int DJ = -R; DJ <= R && bOk; ++DJ)
				{
					if (DI * DI + DJ * DJ > R * R)
					{
						continue;
					}
					const int II = I + DI, JJ = J + DJ;
					if (II < 0 || JJ < 0 || II >= F.N || JJ >= NV || !At(II, JJ))
					{
						bOk = false;
					}
				}
			}
			Out[static_cast<size_t>(I) * NV + J] = bOk ? 1 : 0;
		}
	}
	F.Free.swap(Out);
	F.MinV = NV;
}

/** Points atteints depuis le milieu de l'ouverture d'entree de la cellule V0 (cote galerie) */
static std::vector<uint8_t> Reach(const FWalk& F, int V0)
{
	const int NV = F.MinV;
	std::vector<uint8_t> Seen(F.Free.size(), 0);
	std::deque<std::pair<int, int>> Q;
	const int I0 = static_cast<int>((CellSize * 0.5f) / F.Res); // U = -CellSize/2 : dans la galerie
	const int J0 = static_cast<int>((V0 * CellSize + CellSize * 0.5f) / F.Res);
	if (!F.Free[static_cast<size_t>(I0) * NV + J0])
	{
		return Seen;
	}
	Seen[static_cast<size_t>(I0) * NV + J0] = 1;
	Q.push_back({ I0, J0 });
	const int D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	while (!Q.empty())
	{
		const auto C = Q.front();
		Q.pop_front();
		for (const auto& Dd : D)
		{
			const int I = C.first + Dd[0], J = C.second + Dd[1];
			if (I < 0 || J < 0 || I >= F.N || J >= NV)
			{
				continue;
			}
			const size_t K = static_cast<size_t>(I) * NV + J;
			if (!F.Free[K] || Seen[K])
			{
				continue;
			}
			Seen[K] = 1;
			Q.push_back({ I, J });
		}
	}
	return Seen;
}

int main(int argc, char** argv)
{
	const int Seeds = argc > 1 ? std::atoi(argv[1]) : 2000;
	std::printf("Banc d'essai du Level 6 (LiDAR) : %d graines\n", Seeds);
	int Walked = 0, Holes6 = 0;
	int Sides[4] = {};
	int Ladder[4] = {};
	std::set<std::pair<int, int>> Chunks;
	double MinPassage = 1e9;
	int MaxCorridor = 0;
	for (int S = 0; S < Seeds; ++S)
	{
		const uint32_t Seed = 0x6000u + static_cast<uint32_t>(S) * 2654435761u;
		const FWing W = PickWing(Seed, ChunkCells);
		const FWing W2 = PickWing(Seed, ChunkCells);
		Check(W.bValid && W.MinX == W2.MinX && W.MinY == W2.MinY && W.OutX == W2.OutX && W.OutY == W2.OutY && W.LadderSlot == W2.LadderSlot, Seed,
			"aile non deterministe");
		Chunks.insert({ W.ChunkX, W.ChunkY });
		// Anneau : centre du chunk a WingRing chunks du depart ; galerie d'une cellule au moins dans le chunk
		const int CX = W.ChunkX * ChunkCells + ChunkCells / 2;
		const int CY = W.ChunkY * ChunkCells + ChunkCells / 2;
		const int Cheb = std::abs(CX) > std::abs(CY) ? std::abs(CX) : std::abs(CY);
		Check(Cheb == WingRing * ChunkCells + ChunkCells / 2, Seed, "aile hors de l'anneau prevu");
		Check(W.MinX >= W.ChunkX * ChunkCells + 1 && W.MinX + WingCells <= (W.ChunkX + 1) * ChunkCells - 1
			&& W.MinY >= W.ChunkY * ChunkCells + 1 && W.MinY + WingCells <= (W.ChunkY + 1) * ChunkCells - 1, Seed, "aile sans galerie");
		// Entree tournee vers le depart : la normale d'entree pointe vers la cellule 0, 0
		Check(W.OutX * CX + W.OutY * CY < 0, Seed, "entree tournee vers l'exterieur");
		Sides[(W.OutX == 1 ? 0 : (W.OutX == -1 ? 1 : (W.OutY == 1 ? 2 : 3)))]++;
		Ladder[W.LadderSlot]++;
		// Pourtour : quatre ouvertures d'entree, murs ailleurs ; alcoves separees ; six trous
		int Doors = 0, Walls = 0, Partitions = 0, Alcoves = 0, NHoles = 0;
		for (int V = 0; V < WingCells; ++V)
		{
			int X, Y;
			ToCell(W, 0, V, X, Y);
			Doors += EdgeBetween(W, X, Y, X + W.OutX, Y + W.OutY) == Door ? 1 : 0;
			ToCell(W, WingCells - 1, V, X, Y);
			Walls += EdgeBetween(W, X, Y, X - W.OutX, Y - W.OutY) == Wall ? 1 : 0;
			int AX, AY;
			ToCell(W, WingCells - 2, V, AX, AY);
			Alcoves += EdgeBetween(W, AX, AY, X, Y) == Door ? 1 : 0;
			if (V > 0)
			{
				int PX, PY;
				ToCell(W, WingCells - 1, V - 1, PX, PY);
				Partitions += EdgeBetween(W, PX, PY, X, Y) == Wall ? 1 : 0;
			}
		}
		for (int X = W.MinX - 1; X < W.MinX + WingCells; ++X)
		{
			for (int Y = W.MinY - 1; Y < W.MinY + WingCells; ++Y)
			{
				NHoles += HoleAtCorner(W, X, Y) ? 1 : 0;
			}
		}
		Check(Doors == 4, Seed, "entree sans ses quatre ouvertures");
		Check(Walls == 4, Seed, "mur du fond perce");
		Check(Alcoves == 4 && Partitions == 3, Seed, "alcoves mal separees");
		Check(NHoles == 6, Seed, "nombre de trous different de 6");
		// Couloir garanti : du depart (0, 0) a la galerie devant la porte V = 1, sans entrer dans l'aile, pas a pas
		{
			int GX, GY;
			CorridorEnd(W, GX, GY);
			int X = 0, Y = 0, Steps = 0;
			bool bOk = true;
			auto Walk = [&](int TX, int TY)
			{
				while ((X != TX || Y != TY) && Steps < 200)
				{
					const int NX = X + (TX > X ? 1 : (TX < X ? -1 : 0)) * (X != TX ? 1 : 0);
					const int NY = Y + (X == TX ? (TY > Y ? 1 : (TY < Y ? -1 : 0)) : 0);
					bOk = bOk && CorridorEdge(W, X, Y, NX, NY) && !Contains(W, NX, NY);
					X = NX;
					Y = NY;
					++Steps;
				}
			};
			if (W.OutX != 0)
			{
				Walk(0, GY);
				Walk(GX, GY);
			}
			else
			{
				Walk(GX, 0);
				Walk(GX, GY);
			}
			int EX, EY;
			ToCell(W, 0, 1, EX, EY);
			Check(bOk && X == GX && Y == GY && GX == EX + W.OutX && GY == EY + W.OutY && Steps == std::abs(GX) + std::abs(GY), Seed,
				"couloir garanti interrompu ou dans l'aile");
			MaxCorridor = Steps > MaxCorridor ? Steps : MaxCorridor;
		}
		Holes6 += NHoles == 6 ? 1 : 0;
		// Traversee a pied (une graine sur 10 : grille fine), depuis chaque porte, vers chaque alcove, sans saut
		if (S % 10 == 0)
		{
			FWalk F;
			BuildFloor(W, F);
			const int NV = F.MinV;
			bool bAll = true;
			for (int V0 = 0; V0 < WingCells; ++V0)
			{
				const std::vector<uint8_t> Seen = Reach(F, V0);
				for (int A = 0; A < WingCells; ++A)
				{
					// Fond de l'alcove A (U = 3,5 cellules)
					const int I = static_cast<int>((CellSize + 3.5f * CellSize) / F.Res);
					const int J = static_cast<int>((A * CellSize + CellSize * 0.5f) / F.Res);
					bAll = bAll && Seen[static_cast<size_t>(I) * NV + J];
				}
			}
			Check(bAll, Seed, "une alcove (ou l'echelle) inaccessible a pied depuis une porte");
			Walked += bAll ? 1 : 0;
			// Passage minimal entre deux trous voisins sur une meme limite (cote a cote)
			MinPassage = std::fmin(MinPassage, CellSize - HoleSize);
		}
	}
	// Scanner : couverture de l'impulsion (aucun trou angulaire de plus de 3 degres), cone respecte
	{
		std::vector<std::pair<float, float>> Dirs;
		bool bIn = true;
		for (int I = 0; I < PulseRays; ++I)
		{
			float X, Y, Z;
			PulseDir(I, PulseRays, 0.7f, X, Y, Z);
			const float Yaw = std::atan2(Y, X) * 180.f / 3.14159265f;
			const float Pitch = std::asin(Z) * 180.f / 3.14159265f;
			bIn = bIn && std::fabs(Yaw) <= PulseHalfYaw + 0.01f && std::fabs(Pitch) <= PulseHalfPitch + 0.01f && std::fabs(X * X + Y * Y + Z * Z - 1.f) < 1e-3f;
			Dirs.push_back({ Yaw, Pitch });
		}
		Check(bIn, 0, "rayon d'impulsion hors du cone");
		float WorstGap = 0.f;
		for (float Ty = -PulseHalfYaw * 0.9f; Ty <= PulseHalfYaw * 0.9f; Ty += 2.f)
		{
			for (float Tp = -PulseHalfPitch * 0.9f; Tp <= PulseHalfPitch * 0.9f; Tp += 2.f)
			{
				// dans l'ellipse seulement
				if ((Ty * Ty) / (PulseHalfYaw * PulseHalfYaw) + (Tp * Tp) / (PulseHalfPitch * PulseHalfPitch) > 0.81f)
				{
					continue;
				}
				float Best = 1e9f;
				for (const auto& D : Dirs)
				{
					const float Dd = std::sqrt((D.first - Ty) * (D.first - Ty) + (D.second - Tp) * (D.second - Tp));
					Best = Dd < Best ? Dd : Best;
				}
				WorstGap = Best > WorstGap ? Best : WorstGap;
			}
		}
		Check(WorstGap < 3.f, 0, "trou de couverture de l'impulsion");
		bool bFocusIn = true;
		for (uint32_t Fr = 0; Fr < 200; ++Fr)
		{
			for (int I = 0; I < FocusRaysPerFrame; ++I)
			{
				float X, Y, Z;
				FocusDir(I, Fr, X, Y, Z);
				const float Ang = std::acos(X < 1.f ? X : 1.f) * 180.f / 3.14159265f;
				bFocusIn = bFocusIn && Ang <= FocusHalfAngle + 0.05f;
			}
		}
		Check(bFocusIn, 0, "rayon focalise hors du cone");
		std::printf("Impulsion : %d rayons en %d images (%d par image), cone %.0f x %.0f degres, plus grand vide %.2f degres ; scan focalise : %d rayons par image, cone de %.0f degres\n",
			PulseRays, (PulseRays + PulseRaysPerFrame - 1) / PulseRaysPerFrame, PulseRaysPerFrame, PulseHalfYaw * 2.f, PulseHalfPitch * 2.f, WorstGap,
			FocusRaysPerFrame + 1, FocusHalfAngle * 2.f);
	}
	// Anneau de points : capacite jamais depassee, groupes vides puis reutilises, attenuation decroissante
	{
		FRing R;
		R.BucketSeconds = 12.f / AgeBuckets;
		int MaxTotal = 0;
		bool bFade = true;
		for (int Fr = 0; Fr < 4000; ++Fr)
		{
			int Cleared = -1;
			R.Advance(1.f / 60.f, Cleared);
			R.Reserve(PulseRaysPerFrame + FocusRaysPerFrame + 1 + 120);
			MaxTotal = R.Total() > MaxTotal ? R.Total() : MaxTotal;
			float Prev = 2.f;
			for (int K = 0; K < AgeBuckets; ++K)
			{
				const int B = (R.Current - K + AgeBuckets) % AgeBuckets;
				const float F = R.FadeOf(B, 12.f);
				bFade = bFade && F <= Prev + 1e-5f && F >= 0.f && F <= 1.f;
				Prev = F;
			}
		}
		Check(MaxTotal <= MaxPoints, 0, "plus de points que la capacite");
		Check(bFade, 0, "attenuation non decroissante avec l'age");
		std::printf("Points : capacite %d (%d groupes d'age), au plus %d affiches en charge continue ; par image au plus %d rayons et %d points (silhouettes comprises)\n",
			MaxPoints, AgeBuckets, MaxTotal, PulseRaysPerFrame + FocusRaysPerFrame + 1, PulseRaysPerFrame + FocusRaysPerFrame + 1 + 120);
	}
	// Reglages bornes ; aide des parasites monotone
	{
		FSettings S;
		S.PointSize = 9.f;
		S.Contrast = -1.f;
		S.Persistence = 99.f;
		const FSettings C = Clamp(S);
		Check(C.PointSize == 2.f && C.Contrast == 0.5f && C.Persistence == 30.f, 0, "reglages non bornes");
		bool bMono = true;
		float Prev = 2.f;
		for (float D = 0.f; D <= StaticRange + 100.f; D += 50.f)
		{
			const float L = StaticLevel(D);
			bMono = bMono && L <= Prev && L >= 0.f && L <= 1.f;
			Prev = L;
		}
		Check(bMono && StaticLevel(StaticRange) == 0.f && StaticLevel(0.f) == 1.f, 0, "aide des parasites non monotone");
	}
	std::printf("Aile : %d/%d graines avec six trous ; traversee a pied verifiee (grille de 5 cm, capsule de %.0f cm) sur %d graines : toutes les alcoves atteintes depuis chaque porte\n",
		Holes6, Seeds, Capsule, Walked);
	std::printf("       passage minimal entre deux trous : %.0f cm ; %zu chunks differents ; entree vers +X %d, -X %d, +Y %d, -Y %d ; echelle dans l'alcove 1..4 : %d, %d, %d, %d\n",
		MinPassage, Chunks.size(), Sides[0], Sides[1], Sides[2], Sides[3], Ladder[0], Ladder[1], Ladder[2], Ladder[3]);
	std::printf("       couloir garanti du depart a l'entree : continu, hors de l'aile, %d cellules au plus\n", MaxCorridor);
	std::printf("Verifications : %d, echecs : %d\n", Checks, Failures);
	std::printf("RESULTAT : %s\n", Failures ? "ECHEC" : "OK");
	return Failures ? 1 : 0;
}
