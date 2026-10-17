// v4.16 (vis) : banc hors moteur des decalques d'usure (BRDecalLogic.h, regles appliquees par ABRChunk::BuildDecals).
//
// Avec les dimensions des trois niveaux qui en ont (Levels 0, 1 et 2) et les tirages BRHash du jeu (fonction recopiee
// de BRTypes.h, memes sels), il verifie pour chaque sorte de decalque :
//   - qu'il reste sur sa face de mur (poteaux exclus), sous le plafond (seule la marge transparente du haut de l'aureole
//     le depasse), au-dessus du sol (eraflures), hors du chambranle et du poteau (jambages) ;
//   - qu'une tache au sol passee sous un mur n'en ressort pas de l'autre cote, meme tournee ;
//   - qu'une tache de plafond ne passe jamais au-dessus de la piece voisine ;
//   - les bornes v4.16 : nombre par chunk (pire cas : toutes les faces sont des murs), distance utile (formule du moteur) ;
//   - la variete : frequences tirees, independance de deux faces voisines (pas de rangees), signatures distinctes, eraflures
//     retournees une fois sur deux, aucun chunk qui en repete un autre.
// Les surfaces reelles (kit du Level 0, objets contre les murs, portes) sont mesurees en jeu : -BRAutoTestV416Visual.
//
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Visual/test_decals.cpp -o test_decals
#include "BRDecalLogic.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
	int GChecks = 0, GFailures = 0;
	void Line(const char* What, bool bOk)
	{
		++GChecks;
		GFailures += bOk ? 0 : 1;
		std::printf("  %-104s %s\n", What, bOk ? "OK" : "ECHEC");
	}

	// ---- BRHash (BRTypes.h), recopie a l'identique
	uint32_t Mix(uint32_t X)
	{
		X ^= X >> 16;
		X *= 0x7feb352dU;
		X ^= X >> 15;
		X *= 0x846ca68bU;
		X ^= X >> 16;
		return X;
	}
	uint32_t Hash(int32_t A, int32_t B, int32_t C, uint32_t Seed)
	{
		uint32_t H = Mix(static_cast<uint32_t>(A) * 0x9E3779B1u ^ Seed);
		H = Mix(H ^ (static_cast<uint32_t>(B) * 0x85EBCA77u));
		H = Mix(H ^ (static_cast<uint32_t>(C) * 0xC2B2AE3Du));
		return H;
	}
	float Rand(int32_t A, int32_t B, int32_t C, uint32_t Seed)
	{
		return static_cast<float>(Hash(A, B, C, Seed) & 0xFFFFFFu) / 16777216.f;
	}

	/** Dimensions d'un niveau (BRLevels.cpp) */
	struct FLevel
	{
		int Number;
		float CellSize, WallHeight, WallThickness, DoorWidthDef, FixtureHalf, Density, LightChance, BrokenChance;
	};
	const FLevel Levels[] = {
		{ 0, 350.f, 290.f, 60.f, 150.f, 61.f, 1.0f, 0.38f, 0.06f },  // panneau 61 cm
		{ 1, 600.f, 360.f, 120.f, 260.f, 65.f, 0.6f, 0.35f, 0.12f }, // tube 65 cm
		{ 2, 260.f, 280.f, 90.f, 140.f, 8.f, 0.8f, 0.22f, 0.2f },    // ampoule
	};

	float DoorWidthOf(const FLevel& L)
	{
		// ABRChunk::BuildDecals : min(DoorWidth, S - 2T - 20)
		return std::fmin(L.DoorWidthDef, L.CellSize - 2.f * L.WallThickness - 20.f);
	}

	constexpr float Eps = 0.01f;
}

int main()
{
	using namespace BRDecal;
	std::printf("# Banc des decalques d'usure v4.16 (BRDecalLogic.h)\n");
	char Buf[256];
	const uint32_t Seeds[] = { 9u, 4242u, 77u, 115083815u };

	for (const FLevel& L : Levels)
	{
		const float S = L.CellSize, H = L.WallHeight, T = L.WallThickness;
		const float DW = DoorWidthOf(L);
		const float Jamb = JambLength(S, T, DW);
		std::printf("\n## Level %d : cellule %.0f cm, plafond %.0f cm, murs %.0f cm, passage %.0f cm, jambage %.0f cm\n", L.Number, S, H, T, DW, Jamb);
		int Faces = 0, StainOut = 0, StainAbove = 0, ScuffOut = 0, ScuffLow = 0, JambBad = 0, JambCount = 0, PatchThrough = 0, PatchOut = 0, CeilBad = 0;
		int CeilCross = 0;
		float StainTopShare = 0.f, ScuffMinZ = 1e9f, JambMinGap = 1e9f, JambMinPost = 1e9f, PatchMaxUnder = 0.f, CeilMaxReach = 0.f;
		for (const uint32_t Seed : Seeds)
		{
			for (int32_t X = -40; X < 40; ++X)
			{
				for (int32_t Y = -40; Y < 40; ++Y)
				{
					for (int32_t Side = 0; Side < 4; ++Side)
					{
						auto R = [X, Y, Side, Seed](int32_t Salt) { return Rand(X * 4 + Side, Y, Salt, Seed); };
						++Faces;
						for (int Solid = 0; Solid < 2; ++Solid)
						{
							const float Inset = Solid ? 0.f : T * 0.5f;
							const float Span = FaceSpan(S, Inset);
							// Aureole : sur la face, sous le plafond (marge transparente du haut seulement)
							const FWallDecal St = WaterStain(R(7411), R(7412), R(7413), Span, H);
							StainOut += std::fabs(St.Along) + St.HalfWidth > Span * 0.5f + Eps ? 1 : 0;
							const float Above = std::fmax(0.f, St.Z + St.HalfHeight - H) / (2.f * St.HalfHeight);
							StainTopShare = std::fmax(StainTopShare, Above);
							StainAbove += Above > WaterStainTopMargin + 0.004f ? 1 : 0;
							// Eraflure basse : sur la face, jamais sur le sol, sous le plafond
							const FWallDecal Sc = WallScuff(R(7414), R(7415), R(7416), R(7417), R(7418), Span);
							ScuffOut += std::fabs(Sc.Along) + Sc.HalfWidth > Span * 0.5f + Eps ? 1 : 0;
							ScuffLow += (Sc.Z - Sc.HalfHeight < ScuffFloorClear - Eps || Sc.Z + Sc.HalfHeight > H) ? 1 : 0;
							ScuffMinZ = std::fmin(ScuffMinZ, Sc.Z - Sc.HalfHeight);
							// Tache au pied du mur : la part sous le mur, tournee du lacet maximal, n'en ressort pas
							const float Behind = Solid ? S : T;
							const FFloorPatch P = WallFootPatch(R(7421), R(7422), R(7423), R(7424), Span, Behind);
							const float Reach = P.Under + 0.5f * P.Length * std::sin(PatchMaxJitterDeg * 3.14159265f / 180.f);
							PatchThrough += Reach > Behind - PatchUnderWallMargin + Eps ? 1 : 0;
							PatchMaxUnder = std::fmax(PatchMaxUnder, Solid ? 0.f : Reach);
							PatchOut += (P.Out < -Eps || std::fabs(P.Along) + P.Length * 0.5f > (Span + 20.f) * 0.5f + Eps) ? 1 : 0;
						}
						// Jambage : hors du chambranle (9 cm) + 3 cm, et a 4 cm du poteau au moins
						if (JambTakesScuff(Jamb))
						{
							++JambCount;
							const FWallDecal J = JambScuff(R(7431), R(7432), R(7433), R(7434), R(7435), R(7436), DW, Jamb);
							const float Inner = std::fabs(J.Along) - J.HalfWidth - DW * 0.5f;
							const float Outer = (S - T) * 0.5f - (std::fabs(J.Along) + J.HalfWidth);
							JambMinGap = std::fmin(JambMinGap, Inner);
							JambMinPost = std::fmin(JambMinPost, Outer);
							JambBad += (Inner < CasingWidth + 3.f - Eps || Outer < 4.f - Eps || J.Z - J.HalfHeight < 5.f - Eps) ? 1 : 0;
						}
						// Tache de plafond : dans sa cellule, au plus Radius / 2 sous le mur, jamais au-dessus de la piece voisine
						const float LX = (Rand(X, Y, 11, Seed) - 0.5f) * 60.f;
						const float LY = (Rand(X, Y, 12, Seed) - 0.5f) * 60.f;
						const FCeilingStain C = CeilingStain(R(7451), R(7452), R(7453), LX, LY, L.FixtureHalf, S, T);
						const float Lim = S * 0.5f - T * 0.5f - C.Radius * 0.5f;
						CeilBad += (std::fabs(C.X) > Lim + Eps || std::fabs(C.Y) > Lim + Eps) ? 1 : 0;
						const float ReachC = std::fmax(std::fabs(C.X), std::fabs(C.Y)) + C.Radius;
						CeilMaxReach = std::fmax(CeilMaxReach, ReachC);
						CeilCross += ReachC > S * 0.5f + T * 0.5f - Eps ? 1 : 0;
					}
				}
			}
		}
		std::snprintf(Buf, sizeof(Buf), "aureoles sur leur face (%d faces x 2 x %d graines), aucune ne depasse un poteau", Faces / 4, 4);
		Line(Buf, StainOut == 0);
		std::snprintf(Buf, sizeof(Buf), "aureoles : part au-dessus du plafond <= marge transparente de l'atlas (pire %.1f %% ; marge %.1f %%)", StainTopShare * 100.f,
			WaterStainTopMargin * 100.f + 0.4f);
		Line(Buf, StainAbove == 0);
		std::snprintf(Buf, sizeof(Buf), "eraflures basses sur leur face, jamais sur le sol (bas le plus bas %.0f cm, minimum %.0f)", ScuffMinZ, ScuffFloorClear);
		Line(Buf, ScuffOut == 0 && ScuffLow == 0);
		if (JambTakesScuff(Jamb))
		{
			std::snprintf(Buf, sizeof(Buf), "eraflures des jambages (%d) : a %.0f cm de l'ouverture au moins (chambranle %.0f + 3), a %.0f cm du poteau au moins", JambCount,
				JambMinGap, CasingWidth, JambMinPost);
			Line(Buf, JambBad == 0);
		}
		std::snprintf(Buf, sizeof(Buf), "taches au pied des murs : sous le mur jusqu'a %.0f cm (mur de %.0f), jamais de l'autre cote ; sur la face", PatchMaxUnder, T);
		Line(Buf, PatchThrough == 0 && PatchOut == 0);
		std::snprintf(Buf, sizeof(Buf), "taches de plafond dans leur cellule (pire portee %.0f cm du centre ; mur voisin a %.0f cm)", CeilMaxReach, S * 0.5f + T * 0.5f);
		Line(Buf, CeilBad == 0 && CeilCross == 0);
		const FFloorPatch Th0 = ThresholdPatch(0.f, 0.f, 0.f, DW, T);
		const FFloorPatch Th1 = ThresholdPatch(1.f, 1.f, 1.f, DW, T);
		std::snprintf(Buf, sizeof(Buf), "seuils humides : %.0f a %.0f cm le long du passage de %.0f, %.0f a %.0f cm de chaque cote du mur", Th0.Length, Th1.Length, DW,
			Th0.Width * 0.5f - T * 0.5f, Th1.Width * 0.5f - T * 0.5f);
		Line(Buf, Th1.Length <= DW * 1.2f + Eps && Th0.Width * 0.5f - T * 0.5f >= 40.f - Eps && std::fabs(Th1.YawJitterDeg) <= 10.f + Eps);

		// ---- Bornes par chunk (8 x 8 cellules praticables). Cas dense : une face sur deux est un mur, plafonniers du niveau
		// (presence, part des lampes mortes). Cas extreme : quatre murs a chaque cellule, chaque cellule eclairee d'une lampe
		// morte (seule la borne limite alors le nombre)
		for (int Extreme = 0; Extreme < 2; ++Extreme)
		{
			int Worst = 0, WorstPatches = 0, OverCap = 0, OverPatchCap = 0;
			double Sum = 0.0;
			const int Chunks = 2000;
			std::set<std::string> Layouts;
			int Duplicates = 0;
			for (int Ch = 0; Ch < Chunks; ++Ch)
			{
				const int32_t X0 = (Ch % 50) * 8 - 200, Y0 = (Ch / 50) * 8 - 160;
				int Proj = 0, Patches = 0;
				std::string Sig;
				for (int32_t X = X0; X < X0 + 8; ++X)
				{
					for (int32_t Y = Y0; Y < Y0 + 8; ++Y)
					{
						for (int32_t Side = 0; Side < 4; ++Side)
						{
							if (!Extreme && Rand(X * 4 + Side, Y, 31, 9u) >= 0.5f)
							{
								continue; // pas un mur
							}
							const float Roll = Rand(X * 4 + Side, Y, 7410, 9u);
							if (Roll < 0.1f * L.Density)
							{
								++Proj;
								Sig += static_cast<char>('a' + Side + (Roll < 0.06f * L.Density ? 0 : 4));
								Sig += static_cast<char>('0' + (X - X0) * 8 + (Y - Y0));
							}
							Patches += Rand(X * 4 + Side, Y, 7420, 9u) < 0.05f * L.Density ? 1 : 0;
						}
						const bool bLit = Extreme || Rand(X, Y, 32, 9u) < L.LightChance;
						const bool bBroken = Extreme || Rand(X, Y, 33, 9u) < L.BrokenChance;
						Proj += (bLit && Rand(X, Y, 7450, 9u) < (bBroken ? 0.35f : 0.22f) * L.Density) ? 1 : 0;
					}
				}
				Worst = Proj > Worst ? Proj : Worst;
				WorstPatches = Patches > WorstPatches ? Patches : WorstPatches;
				OverCap += Proj > MaxProjectedPerChunk ? 1 : 0;
				OverPatchCap += Patches > MaxFloorPatchesPerChunk ? 1 : 0;
				Sum += Proj;
				Duplicates += Layouts.insert(Sig).second ? 0 : 1;
			}
			if (Extreme)
			{
				std::printf("  cas extreme (%d chunks) : %.1f decalques projetes par chunk en moyenne, %d au plus : borne %d atteinte dans %.1f %% des chunks\n", Chunks,
					Sum / Chunks, Worst, MaxProjectedPerChunk, 100.0 * OverCap / Chunks);
				continue;
			}
			std::snprintf(Buf, sizeof(Buf), "cas dense (%d chunks) : %.1f decalques projetes par chunk, %d au plus (borne %d, %d fois) ; taches au sol %d au plus (borne %d)",
				Chunks, Sum / Chunks, Worst, MaxProjectedPerChunk, OverCap, WorstPatches, MaxFloorPatchesPerChunk);
			Line(Buf, OverCap * 100 <= Chunks && OverPatchCap * 100 <= Chunks);
			std::snprintf(Buf, sizeof(Buf), "aucun chunk ne repete la disposition d'un autre (%d dispositions comparees, %d doublon(s))", Chunks, Duplicates);
			Line(Buf, Duplicates == 0);
		}
	}

	// ---- Variete (Level 0, 4 graines, 160 x 160 cellules) : frequences, faces voisines independantes, signatures, retournement
	{
		std::printf("\n## Variete (Level 0)\n");
		const float K = 1.f;
		const float Span = FaceSpan(350.f, 30.f);
		long long Faces = 0, Stains = 0, Pairs = 0, PairBoth = 0, Scuffs = 0, Flipped = 0;
		std::map<int, int> Signatures;
		for (const uint32_t Seed : Seeds)
		{
			for (int32_t X = -80; X < 80; ++X)
			{
				for (int32_t Y = -80; Y < 80; ++Y)
				{
					for (int32_t Side = 0; Side < 4; ++Side)
					{
						++Faces;
						const float Roll = Rand(X * 4 + Side, Y, 7410, Seed);
						const bool bStain = Roll < 0.06f * K;
						if (bStain)
						{
							++Stains;
							const FWallDecal St = WaterStain(Rand(X * 4 + Side, Y, 7411, Seed), Rand(X * 4 + Side, Y, 7412, Seed), Rand(X * 4 + Side, Y, 7413, Seed), Span, 290.f);
							const int Sig = static_cast<int>(std::floor((St.Along + 200.f) / 10.f)) * 10000 + static_cast<int>(St.HalfWidth / 5.f) * 100 +
								static_cast<int>(St.HalfHeight / 5.f);
							++Signatures[Sig];
						}
						else if (Roll < 0.1f * K)
						{
							++Scuffs;
							Flipped += WallScuff(0.5f, 0.5f, 0.5f, 0.5f, Rand(X * 4 + Side, Y, 7418, Seed), Span).bFlipped ? 1 : 0;
						}
						// Face voisine sur la meme ligne de mur : la cellule suivante le long du mur (meme cote)
						const int32_t NX = (Side >= 2) ? X + 1 : X, NY = (Side >= 2) ? Y : Y + 1;
						const bool bNext = Rand(NX * 4 + Side, NY, 7410, Seed) < 0.06f * K;
						++Pairs;
						PairBoth += (bStain && bNext) ? 1 : 0;
					}
				}
			}
		}
		const double P = static_cast<double>(Stains) / static_cast<double>(Faces);
		const double Ratio = static_cast<double>(PairBoth) / (static_cast<double>(Pairs) * P * P);
		int Top = 0;
		for (const auto& KV : Signatures)
		{
			Top = KV.second > Top ? KV.second : Top;
		}
		std::snprintf(Buf, sizeof(Buf), "aureoles sur %.2f %% des faces (attendu 6 %%)", P * 100.0);
		Line(Buf, std::fabs(P - 0.06) < 0.003);
		std::snprintf(Buf, sizeof(Buf), "deux faces voisines d'une meme ligne de mur : %.2f fois le hasard (independantes : pas de rangee repetee)", Ratio);
		Line(Buf, Ratio > 0.85 && Ratio < 1.15);
		std::snprintf(Buf, sizeof(Buf), "aureoles : %d signatures (place a 10 cm, taille a 5 cm) pour %lld ; la plus frequente : %.2f %%", static_cast<int>(Signatures.size()), Stains,
			100.0 * Top / static_cast<double>(Stains));
		Line(Buf, static_cast<double>(Top) / static_cast<double>(Stains) < 0.01);
		std::snprintf(Buf, sizeof(Buf), "eraflures retournees (v4.16) : %.1f %% (attendu 50 %%)", 100.0 * static_cast<double>(Flipped) / static_cast<double>(Scuffs));
		Line(Buf, std::fabs(static_cast<double>(Flipped) / static_cast<double>(Scuffs) - 0.5) < 0.02);
	}

	// ---- Distance utile (formule du moteur, DecalRenderingShared.cpp)
	{
		std::printf("\n## Distance utile\n");
		bool bSame = true;
		for (const float Half : { 5.f, 15.f, 35.f, 70.f })
		{
			const float Fss = FadeScreenSizeFor(Half);
			const float Ref = FadeOutDistance(Half, Fss, ReferenceWidth, ReferenceHFovDeg);
			const float Low = FadeOutDistance(Half, Fss, 1229.f, ReferenceHFovDeg);
			const float Old = FadeOutDistance(Half, 0.01f, ReferenceWidth, ReferenceHFovDeg);
			std::printf("  demi-taille %3.0f cm : efface a %4.1f m en 1920 px (%4.1f m en 1229 px, fenetre de test a 80 %%) ; avant (0,01) : %5.1f m\n", Half, Ref / 100.f,
				Low / 100.f, Old / 100.f);
			// Au plus tard a la distance utile ; jamais plus loin qu'avant ; a la distance utile des que la regle d'avant allait au-dela
			bSame = bSame && Ref <= UsefulDistance + 1.f && Ref <= Old + 1.f && (Old <= UsefulDistance + 1.f || std::fabs(Ref - UsefulDistance) < 1.f) && Low < Ref;
		}
		std::snprintf(Buf, sizeof(Buf), "chaque decalque s'efface au plus tard a %.0f m en 1920 px, jamais plus loin qu'avant (plus tot dans une image plus petite)",
			UsefulDistance / 100.f);
		Line(Buf, bSame);
		Line("le lot des taches au sol s'arrete a la meme distance", std::fabs(FloorPatchCull - UsefulDistance) < 1.f);
	}

	std::printf("\nVerifications : %d, echecs : %d\nRESULTAT : %s\n", GChecks, GFailures, GFailures == 0 ? "OK" : "ECHEC");
	return GFailures == 0 ? 0 : 1;
}
