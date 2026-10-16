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
//   - le budget par image : rayons et points ajoutes bornes ;
//   - v4.15 : la longueur du couloir garanti (BRLidar::CorridorLength, plafond du parcours de l'aile dans
//     ABRWorld::PlaceMission) egale au couloir parcouru, au plus 41 cellules sur les 180 positions possibles de l'aile.
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

// ============================================================================================================
// v4.14 : scanner par BRLidar::RunFrame, la logique meme de ABRLidar::Tick (rayons, reserve, inscriptions)
// ============================================================================================================
namespace Scan
{
	/** Scene du banc : ce que touchent les rayons. Un rayon = un point de sa categorie (comme ABRLidar::TraceRay) ;
	 *  une porte ou une cage touchee par le focus est une cible a inscriptions */
	struct FSimHost : public IScanHost
	{
		int Pending[NumCategories] = {};
		/** Focus : categorie touchee, ou cibles a inscriptions (alternance entre les cibles) */
		int FocusCat = Env;
		int NumFocusTargets = 0;
		int FocusTarget[8] = {};
		int FocusTotal[8] = {};
		int PulseCat = Env;
		long long Rays = 0;
		long long TargetRays = 0;
		virtual FHit TraceRay(bool bPulse, bool bCenter, float, float, float) override
		{
			(void)bCenter;
			++Rays;
			FHit H;
			if (bPulse)
			{
				++Pending[PulseCat];
				return H;
			}
			if (NumFocusTargets > 0)
			{
				const int T = static_cast<int>(Rays % NumFocusTargets);
				++Pending[Device];
				H.GlyphTarget = FocusTarget[T];
				H.GlyphTotal = FocusTotal[T];
				++TargetRays;
				return H;
			}
			++Pending[FocusCat];
			return H;
		}
		virtual int EmitGlyphs(int, int, int Count) override
		{
			Pending[Glyph] += Count;
			return Count;
		}
		virtual int PendingPoints(int Cat) const override { return Pending[Cat]; }
	};

	struct FRun
	{
		long long Requested = 0;
		long long Accepted = 0;
		long long Frames = 0;
		long long EmptyFrames = 0;
		long long Rays = 0;
		long long GlyphPoints = 0;
		int PeakPoints = 0;
		int PeakRays = 0;
		int PeakGlyph = 0;
		double Seconds = 0.0;
	};

	/** Une image : RunFrame, puis ce que fait ABRLidar::ApplyFrame (les points acceptes partent au rendu) */
	void Step(FScanState& S, FSimHost& H, float Dt, float Persistence, bool bFocus, FRun& R, FFrameResult* OutRes = nullptr)
	{
		FFrameResult Res;
		RunFrame(S, Dt, Persistence, bFocus, H, Res);
		int Points = 0;
		for (int Cat = 0; Cat < NumCategories; ++Cat)
		{
			R.Requested += H.Pending[Cat];
			R.Accepted += Res.Accepted[Cat];
			Points += Res.Accepted[Cat];
			H.Pending[Cat] = 0;
		}
		R.Frames += 1;
		R.EmptyFrames += (bFocus && Points == 0) ? 1 : 0;
		R.Rays += Res.PulseRays + Res.FocusRays;
		R.GlyphPoints += Res.GlyphPoints;
		R.PeakPoints = Points > R.PeakPoints ? Points : R.PeakPoints;
		R.PeakRays = Res.PulseRays + Res.FocusRays > R.PeakRays ? Res.PulseRays + Res.FocusRays : R.PeakRays;
		R.PeakGlyph = Res.GlyphPoints > R.PeakGlyph ? Res.GlyphPoints : R.PeakGlyph;
		R.Seconds += Dt;
		if (OutRes)
		{
			*OutRes = Res;
		}
	}

	uint32_t Rng = 12345u;
	float Rand01()
	{
		Rng = Rng * 1664525u + 1013904223u;
		return static_cast<float>((Rng >> 8) & 0xFFFF) / 65535.f;
	}
}

static void RunScannerChecks()
{
	using namespace Scan;
	std::printf("Scanner (v4.14, BRLidar::RunFrame : meme logique que ABRLidar::Tick) :\n");
	// 1. Focus continu 30 s sur une surface (un point par rayon), cadence fixe et irreguliere
	const int Fps[] = { 30, 60, 144, 240 };
	for (int Jitter = 0; Jitter < 2; ++Jitter)
	{
		for (int F : Fps)
		{
			FScanState S;
			FSimHost H;
			FRun R;
			Rng = 777u + F;
			while (R.Seconds < 30.0)
			{
				// Irreguliere : de 0,4 a 1,6 fois la duree moyenne, avec une saccade de 120 ms toutes les 2 s environ
				float Dt = 1.f / F;
				if (Jitter)
				{
					Dt *= 0.4f + 1.2f * Rand01();
					if (Rand01() < 0.5f / F)
					{
						Dt = 0.12f;
					}
				}
				Step(S, H, Dt, 12.f, true, R);
			}
			const double RaysPerSec = R.Rays / R.Seconds;
			// Cadence irreguliere : une image plus longue que FocusMaxPerFrame / FocusRaysPerSecond (43 ms) est plafonnee, sans
			// rattrapage ensuite (travail borne) : la cadence moyenne peut baisser un peu, jamais monter
			const double MinRate = FocusRaysPerSecond * (Jitter ? 0.90 : 0.99);
			const bool bOk = R.Requested == R.Accepted && S.Store.Rejected == 0 && S.Store.Total() <= MaxPoints && RaysPerSec > MinRate
				&& RaysPerSec < FocusRaysPerSecond * 1.01 && R.PeakRays <= FocusMaxPerFrame && R.EmptyFrames == 0;
			Check(bOk, F, Jitter ? "focus a cadence irreguliere : points refuses ou cadence non tenue" : "focus continu : points refuses ou images vides");
			std::printf("  focus 30 s, %3d images/s%s : %lld points demandes, %.1f %% acceptes, %lld images sans point, %.0f rayons/s, %d rayons au plus par image, %d points en reserve\n",
				F, Jitter ? " (irreguliere, saccades de 120 ms)" : "", R.Requested, 100.0 * R.Accepted / (R.Requested ? R.Requested : 1), R.EmptyFrames, RaysPerSec,
				R.PeakRays, S.Store.Total());
		}
	}
	// 2. Saturation : impulsions a chaque recharge et focus, 90 s, persistance maximale (30 s) ; puis un trou decouvert
	{
		FScanState S;
		FSimHost H;
		FRun R;
		for (int I = 0; I < 90 * 144; ++I)
		{
			if (S.Cooldown <= 0.f)
			{
				S.StartPulse(0.3f * I);
			}
			Step(S, H, 1.f / 144.f, 30.f, true, R);
		}
		const int EnvTotal = S.Store.Total(Env);
		const long long Replaced = S.Store.Replaced;
		Check(EnvTotal <= CategoryCapacity(Env) && Replaced > 0 && S.Store.Rejected == 0, 0, "saturation : capacite depassee ou points refuses");
		// La camera se tourne vers un trou : les bords dangereux sont releves tout de suite, a plein eclat
		H.FocusCat = Danger;
		FRun D;
		for (int I = 0; I < 72; ++I)
		{
			Step(S, H, 1.f / 144.f, 30.f, true, D);
		}
		const int Cur = S.Store.Current[Danger];
		const bool bDanger = S.Store.Total(Danger) > 0 && D.Requested == D.Accepted && S.Store.FadeOf(Danger, Cur) == 1.f;
		Check(bDanger, 0, "danger decouvert apres saturation : non releve");
		std::printf("  saturation (impulsion a chaque recharge + focus, 90 s, persistance 30 s, 144 images/s) : decor %d / %d points, %lld remplaces (les plus anciens), %lld refuses ; "
			"trou vise ensuite : %d bords releves en 0,5 s, %.0f %% acceptes, eclat %.0f %%\n",
			EnvTotal, CategoryCapacity(Env), Replaced, S.Store.Rejected, S.Store.Total(Danger), 100.0 * D.Accepted / (D.Requested ? D.Requested : 1), 100.f * S.Store.FadeOf(Danger, Cur));
	}
	// 3. Expiration : longue image de 13 s pour 12 s de persistance ; persistances minimale et maximale ; persistance reduite
	{
		FScanState S;
		FSimHost H;
		FRun R;
		H.Pending[Env] = 100;
		Step(S, H, 0.f, 12.f, false, R);
		Step(S, H, 13.f, 12.f, false, R);
		const bool bLong = S.Store.Total() == 0 && S.Store.Expired == 100;
		Check(bLong, 0, "longue image : points non expires");
		std::printf("  longue image de 13 s, persistance 12 s : %d points restent, %lld expires\n", S.Store.Total(), S.Store.Expired);
		for (float P : { 4.f, 30.f })
		{
			FScanState T;
			FSimHost G;
			FRun Q;
			for (int I = 0; I < 120; ++I)
			{
				Step(T, G, 1.f / 60.f, P, true, Q); // 2 s de focus
			}
			FRun Wait;
			bool bBefore = true;
			for (double Tm = 0.0; Tm < P - 0.05; Tm += 1.0 / 60.0)
			{
				Step(T, G, 1.f / 60.f, P, false, Wait);
			}
			bBefore = T.Store.Total() > 0; // les derniers points (age < P) sont encore la
			for (int I = 0; I < 12; ++I)
			{
				Step(T, G, 1.f / 60.f, P, false, Wait);
			}
			const bool bAfter = T.Store.Total() == 0;
			Check(bBefore && bAfter, static_cast<uint32_t>(P), "persistance : expiration avant l'heure ou trop tard");
			std::printf("  persistance %.0f s : points encore visibles a %.2f s, tous expires a %.2f s apres l'arret\n", P, P - 0.05f, P + 0.15f);
		}
		FScanState U;
		FSimHost G;
		FRun Q;
		for (int I = 0; I < 120; ++I)
		{
			Step(U, G, 1.f / 60.f, 30.f, true, Q);
		}
		for (int I = 0; I < 5 * 60; ++I)
		{
			Step(U, G, 1.f / 60.f, 4.f, false, Q); // reglage ramene de 30 a 4 s
		}
		Check(U.Store.Total() == 0, 0, "persistance reduite : anciens points non expires");
		std::printf("  persistance ramenee de 30 a 4 s : %d points restent apres 5 s\n", U.Store.Total());
	}
	// 4. Impulsion et focus simultanes : l'impulsion dure PulseSeconds a toute cadence (plafond par image), bornes par image
	{
		for (int F : { 10, 30, 60, 144, 240 })
		{
			FScanState S;
			FSimHost H;
			FRun R;
			S.StartPulse(0.f);
			int Frames = 0;
			while (S.PulseNext >= 0 || Frames == 0)
			{
				Step(S, H, 1.f / F, 12.f, true, R);
				++Frames;
			}
			const double Dur = Frames / static_cast<double>(F);
			const bool bOk = R.PeakRays <= PulseMaxPerFrame + FocusMaxPerFrame && (F < 30 || std::fabs(Dur - PulseSeconds) < 2.5 / F + 1e-6);
			Check(bOk, F, "impulsion + focus : duree ou plafond par image non tenu");
			std::printf("  impulsion + focus a %3d images/s : %d images (%.3f s), %d rayons au plus par image\n", F, Frames, Dur, R.PeakRays);
		}
	}
	// 5. Inscriptions : une plaque cadree par TOUS les rayons focalises ; quatre portes et quatre cages ensemble
	{
		const int Sym = 2; // le carre : 24 cases, le plus gros symbole
		for (int F : { 30, 240 })
		{
			FScanState S;
			FSimHost H;
			FRun R;
			H.NumFocusTargets = 1;
			H.FocusTarget[0] = 11;
			H.FocusTotal[0] = PlateGlyphPoints(Sym);
			int Completions = 0;
			FFrameResult Res;
			for (int I = 0; I < 3 * F; ++I)
			{
				Step(S, H, 1.f / F, 12.f, true, R, &Res);
				Completions += Res.NumCompleted;
			}
			// Sur 3 s, avec un rafraichissement a GlyphRefreshSeconds (7,2 s pour 12 s) : une seule passe complete
			const bool bOk = R.GlyphPoints == PlateGlyphPoints(Sym) && Completions == 1 && R.PeakGlyph <= GlyphMaxPerFrame
				&& S.Glyphs.DistinctTouches <= R.Frames && R.PeakPoints <= FocusMaxPerFrame + GlyphMaxPerFrame;
			Check(bOk, F, "plaque cadree : inscription repetee, hors budget ou carnet note plusieurs fois");
			std::printf("  plaque (code a %d cases) cadree par tous les rayons, 3 s a %3d images/s : %lld rayons sur la porte -> %lld cibles distinctes, %lld points d'inscription "
				"(une passe de %d), releve au carnet %d fois, au plus %d points d'inscription et %d points par image (v4.13 : jusqu'a %d par image)\n",
				SymbolCellCount(Sym), F, H.TargetRays, S.Glyphs.DistinctTouches, R.GlyphPoints, PlateGlyphPoints(Sym), Completions, R.PeakGlyph, R.PeakPoints, 37 * (5 + SymbolCellCount(Sym)));
		}
		FScanState S;
		FSimHost H;
		FRun R;
		H.NumFocusTargets = 8;
		for (int K = 0; K < 8; ++K)
		{
			H.FocusTarget[K] = 20 + K;
			H.FocusTotal[K] = K < 4 ? PlateGlyphPoints(K) : CageFigurePoints;
		}
		int Done = 0;
		FFrameResult Res;
		double At = 0.0;
		for (int I = 0; I < 6 * 60 && Done < 8; ++I)
		{
			Step(S, H, 1.f / 60.f, 12.f, true, R, &Res);
			Done += Res.NumCompleted;
			At = R.Seconds;
		}
		int Expected = 0;
		for (int K = 0; K < 8; ++K)
		{
			Expected += H.FocusTotal[K];
		}
		Check(Done == 8 && R.GlyphPoints == Expected && R.PeakGlyph <= GlyphMaxPerFrame, 0, "huit cibles : inscriptions incompletes ou hors budget");
		std::printf("  quatre portes et quatre cages cadrees ensemble (60 images/s) : %d inscriptions completes en %.2f s, %lld points (%d attendus), %d au plus par image\n",
			Done, At, R.GlyphPoints, Expected, R.PeakGlyph);
		std::printf("  borne par image (constantes du code) : %d rayons d'impulsion + %d rayons focalises + %d points d'inscription, plus un chevron par repere et par groupe\n",
			PulseMaxPerFrame, FocusMaxPerFrame, GlyphMaxPerFrame);
	}
	// 6. Capacites par categorie et memoire bornee
	{
		int Sum = 0;
		for (int Cat = 0; Cat < NumCategories; ++Cat)
		{
			Sum += CategoryCapacity(Cat);
		}
		Check(Sum == MaxPoints, 0, "capacites par categorie : somme differente de MaxPoints");
		std::printf("  reserve : %d points (decor %d, dangers %d, mecanismes %d, menaces %d, reperes %d, inscriptions %d), %d groupes d'age par categorie\n",
			MaxPoints, CategoryCapacity(Env), CategoryCapacity(Danger), CategoryCapacity(Device), CategoryCapacity(Entity), CategoryCapacity(Marker), CategoryCapacity(Glyph), AgeBuckets);
	}
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
			// v4.15 : la longueur que le placement de la mission prend pour plafond de son parcours (ABRWorld::PlaceMission)
			Check(CorridorLength(W) == Steps, Seed, "CorridorLength differente du couloir parcouru");
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
			for (int I = 0; I < FocusMaxPerFrame; ++I)
			{
				float X, Y, Z;
				FocusDir(I, Fr, X, Y, Z);
				const float Ang = std::acos(X < 1.f ? X : 1.f) * 180.f / 3.14159265f;
				bFocusIn = bFocusIn && Ang <= FocusHalfAngle + 0.05f;
			}
		}
		Check(bFocusIn, 0, "rayon focalise hors du cone");
		std::printf("Impulsion : %d rayons en %.2f s (%d par image au plus), cone %.0f x %.0f degres, plus grand vide %.2f degres ; scan focalise : %.0f rayons/s (%d par image au plus), cone de %.0f degres\n",
			PulseRays, PulseSeconds, PulseMaxPerFrame, PulseHalfYaw * 2.f, PulseHalfPitch * 2.f, WorstGap, FocusRaysPerSecond, FocusMaxPerFrame, FocusHalfAngle * 2.f);
	}
	RunScannerChecks();
	// v4.15 : toutes les positions possibles de l'aile (chunks de l'anneau x places dans le chunk, comme PickWing) : le
	// couloir garanti mesure au plus 41 cellules ; le plafond du parcours de l'aile (couloir + 2) reste sous la limite de
	// 6000 cellules du placement (diamant de rayon 43 : 3785 cellules)
	int WingPositions = 0, CorridorMin = 1 << 30, CorridorMax = 0;
	{
		const int RingLo = -WingRing - 1, RingHi = WingRing, Slack = ChunkCells - WingCells - 2;
		bool bAll = true;
		for (int CX = RingLo; CX <= RingHi; ++CX)
		{
			for (int CY = RingLo; CY <= RingHi; ++CY)
			{
				if (CX != RingLo && CX != RingHi && CY != RingLo && CY != RingHi)
				{
					continue;
				}
				for (int OX = 0; OX <= Slack; ++OX)
				{
					for (int OY = 0; OY <= Slack; ++OY)
					{
						FWing W;
						W.bValid = true;
						W.ChunkX = CX;
						W.ChunkY = CY;
						W.MinX = CX * ChunkCells + 1 + OX;
						W.MinY = CY * ChunkCells + 1 + OY;
						const int MX = CX * ChunkCells + ChunkCells / 2;
						const int MY = CY * ChunkCells + ChunkCells / 2;
						if (std::abs(MX) >= std::abs(MY))
						{
							W.OutX = MX > 0 ? -1 : 1;
							W.OutY = 0;
						}
						else
						{
							W.OutX = 0;
							W.OutY = MY > 0 ? -1 : 1;
						}
						int GX, GY;
						CorridorEnd(W, GX, GY);
						const int Len = CorridorLength(W);
						bAll = bAll && Len == std::abs(GX) + std::abs(GY) && Len <= 41;
						CorridorMin = Len < CorridorMin ? Len : CorridorMin;
						CorridorMax = Len > CorridorMax ? Len : CorridorMax;
						++WingPositions;
					}
				}
			}
		}
		const int Diamond = 2 * (CorridorMax + 2) * (CorridorMax + 2) + 2 * (CorridorMax + 2) + 1;
		Check(bAll && WingPositions == 180 && Diamond < 6000, 0, "couloir garanti de plus de 41 cellules, ou plafond de l'aile au-dela de la limite du placement");
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
	std::printf("       v4.15 : %d positions possibles de l'aile : couloir de %d a %d cellules (au plus 41), plafond du parcours de l'aile %d\n", WingPositions,
		CorridorMin, CorridorMax, CorridorMax + 2);
	std::printf("Verifications : %d, echecs : %d\n", Checks, Failures);
	std::printf("RESULTAT : %s\n", Failures ? "ECHEC" : "OK");
	return Failures ? 1 : 0;
}
