// v4.16 (perf) : banc hors moteur de la logique des mesures de performance (Source/Backrooms/Public/BRPerfLogic.h), celle
// du test du jeu -BRAutoTest -BRAutoTestPerf (BRAutoTestV416Perf.cpp).
//
// Compilation et lancement (depuis Backrooms/ ; Tools/run_benches.py le fait avec GCC, Clang, clang-cl et MSVC) :
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Perf/test_perf.cpp -o test_perf
//
// Verifie :
//   - resume des temps d'image : mediane, centiles 95 / 99 (regle du rapport du test), pire image, 1 % le plus lent, longues
//     images strictement au-dela de 33,3, 50 et 100 ms ;
//   - prechauffage : saccades de compilation puis temps stables (accepte), derive lente (refusee),
//     images heurtees de medianes voisines (refusees : dispersion), saccade recente (refusee tant qu'elle est dans les
//     90 dernieres images), bruit de +/- 3 % (accepte), images tres lentes (Cinematique, 45 ms) ;
//   - releve ProfileGPU : lignes produites avec LE FORMAT DU MOTEUR ("%s%4.1f%% %5.2fms%s   %s", retrait de 3 espaces par
//     niveau, attente facultative), en caracteres etroits et larges (TCHAR) : profondeur, temps, attente, nom ; en-tetes
//     des files et temps de l'image ; lignes etrangeres refusees ;
//   - postes : noms des passes d'Unreal 5.8 ramenes au bon poste, "Build" n'est pas l'interface, prefixes ("TSR") ;
//   - attribution sans double compte (une ombre dans Lumen reste a Lumen) et "autre" = racine - postes ;
//   - chemins stables (chiffres remplaces par '#'), mediane de trois releves.
// v4.16 (perf, relecture) :
//   - "%d Other Children" est ecrit par le moteur au retrait de son PARENT : rattache a ce parent (profondeur + 1) ; celui de
//     la racine ne s'ajoute plus a la racine (avant : image comptee 30,3 ms au lieu de 30) ;
//   - postes "contenants" (eclairage, effets d'ecran) : Lumen, MegaLights et les ombres projetees sous
//     RenderDeferredLighting / DiffuseIndirectAndAO / Lights gardent leur poste (avant : tout en "eclairage") ;
//     "LightScattering" du brouillard n'est pas de l'eclairage ("Lights" en nom entier) ; eau des Poolrooms (SLW::) ;
//   - plusieurs racines (files de calcul asynchrone) et saut de profondeur rattache au dernier evenement ouvert.
// v4.16 (perf, corrections de relecture) :
//   - "k Other Children ... + w Wait" de la file graphique (occupation + attente dans le temps ecrit) : relu en occupation
//     seule ; racine sans temps propre : somme des postes = racine (avant : racine + attente cachee).
// Code de sortie 0 si tout passe, 1 sinon.
#include "BRPerfLogic.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	int GChecks = 0;
	int GFailures = 0;

	void Line(const char* What, bool bOk)
	{
		++GChecks;
		GFailures += bOk ? 0 : 1;
		std::printf("  %-96s %s\n", What, bOk ? "OK" : "ECHEC");
	}

	bool Near(double A, double B, double Tol = 1e-6)
	{
		return std::fabs(A - B) <= Tol;
	}

	/** Ligne d'evenement ecrite comme le moteur (GPUProfiler.cpp, LogTree, r.ProfileGPU.TableFormatting 0) :
	 *  FString::Printf(TEXT("%s%4.1f%% %5.2fms%s   %s\n"), *FString().LeftPad(Level * 3), Percent, Ms,
	 *      bWait ? *FString::Printf(TEXT(" + %2.2f Wait"), WaitMs) : TEXT(""), *Name) */
	std::string EngineLine(int Level, double Percent, double Ms, double WaitMs, const char* Name)
	{
		const std::string Pad(static_cast<size_t>(Level * 3), ' ');
		char Wait[64] = "";
		if (WaitMs > 0.0)
		{
			std::snprintf(Wait, sizeof(Wait), " + %2.2f Wait", WaitMs);
		}
		char Buf[512];
		std::snprintf(Buf, sizeof(Buf), "%s%4.1f%% %5.2fms%s   %s", Pad.c_str(), Percent, Ms, Wait, Name);
		return std::string(Buf);
	}

	/** En-tete du releve (meme source) : "    - %-30s: %.2fms" */
	std::string EngineFrameTime(double Ms)
	{
		char Buf[128];
		std::snprintf(Buf, sizeof(Buf), "    - %-30s: %.2fms", "Frame Time", Ms);
		return std::string(Buf);
	}

	std::wstring Widen(const std::string& S)
	{
		return std::wstring(S.begin(), S.end());
	}

	std::string NameOf(const std::string& S, const BRPerf::FGpuLine& L)
	{
		return S.substr(static_cast<size_t>(L.NameStart), static_cast<size_t>(L.NameLen));
	}

	void TestSummary()
	{
		std::printf("Resume des temps d'image\n");
		std::vector<float> V;
		for (int I = 0; I < 990; ++I)
		{
			V.push_back(10.f + 0.01f * static_cast<float>(I % 7));
		}
		for (int I = 0; I < 6; ++I)
		{
			V.push_back(40.f);
		}
		for (int I = 0; I < 3; ++I)
		{
			V.push_back(60.f);
		}
		V.push_back(150.f);
		// Ordre melange (les longues images au milieu de la serie)
		std::vector<float> Mixed;
		for (size_t I = 0; I < V.size(); ++I)
		{
			Mixed.push_back(V[(I * 37) % V.size()]);
		}
		const BRPerf::FSummary S = BRPerf::Summarize(Mixed.data(), static_cast<int>(Mixed.size()));
		char Buf[256];
		std::snprintf(Buf, sizeof(Buf), "1000 images : mediane %.2f, 95 %% %.2f, 99 %% %.2f, pire %.0f, moyenne %.3f ms", S.Median, S.P95, S.P99, S.Worst, S.Mean);
		Line(Buf, S.Frames == 1000 && S.Median >= 10.f && S.Median <= 10.07f && S.P95 <= 10.07f && S.P99 <= 10.07f && S.Worst == 150.f);
		std::snprintf(Buf, sizeof(Buf), "longues images > 33,3 / 50 / 100 ms : %d / %d / %d (attendu 10 / 4 / 1)", S.Long[0], S.Long[1], S.Long[2]);
		Line(Buf, S.Long[0] == 10 && S.Long[1] == 4 && S.Long[2] == 1);
		// 1 % le plus lent : les 10 plus longues (6 x 40, 3 x 60, 150) -> moyenne 57
		std::snprintf(Buf, sizeof(Buf), "1 %% le plus lent : moyenne %.1f ms (attendu 57,0), soit %.1f img/s", S.Low1Mean, 1000.f / S.Low1Mean);
		Line(Buf, Near(S.Low1Mean, 57.0, 1e-4));
		// Regle du rapport : indice plancher de P x (N - 1)
		std::vector<float> Ten = { 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f };
		Line("centiles d'une serie de 10 : mediane = 5 (indice 4), 95 % = 9 (indice 8), 99 % = 9", BRPerf::PercentileSorted(Ten.data(), 10, 0.5f) == 5.f
			&& BRPerf::PercentileSorted(Ten.data(), 10, 0.95f) == 9.f && BRPerf::PercentileSorted(Ten.data(), 10, 0.99f) == 9.f);
		// Seuils stricts
		std::vector<float> Edge = { 33.3f, 33.34f, 50.f, 50.01f, 100.f, 100.5f };
		const BRPerf::FSummary E = BRPerf::Summarize(Edge.data(), static_cast<int>(Edge.size()));
		std::snprintf(Buf, sizeof(Buf), "seuils stricts : 33,30 non, 33,34 oui ; 50,00 non, 50,01 oui ; 100,0 non, 100,5 oui -> %d / %d / %d", E.Long[0], E.Long[1], E.Long[2]);
		Line(Buf, E.Long[0] == 5 && E.Long[1] == 3 && E.Long[2] == 1);
		const BRPerf::FSummary Empty = BRPerf::Summarize(nullptr, 0);
		Line("serie vide : resume nul", Empty.Frames == 0 && Empty.Median == 0.f && Empty.Worst == 0.f);
	}

	/** Premiere image (indice) a laquelle le prechauffage serait accepte, -1 sinon */
	int SettleIndex(const std::vector<float>& Frames, const BRPerf::FSettleRule& Rule)
	{
		std::vector<float> Scratch(static_cast<size_t>(BRPerf::SettleSpan(Rule)));
		for (size_t N = 1; N <= Frames.size(); ++N)
		{
			if (BRPerf::IsSettled(Frames.data(), static_cast<int>(N), Rule, Scratch.data()))
			{
				return static_cast<int>(N) - 1;
			}
		}
		return -1;
	}

	void TestSettle()
	{
		std::printf("Prechauffage\n");
		const BRPerf::FSettleRule Rule;
		char Buf[256];
		// Compilation des shaders : 60 images heurtees (30 a 120 ms), puis 10 ms +/- 0,2
		std::vector<float> A;
		for (int I = 0; I < 60; ++I)
		{
			A.push_back(30.f + static_cast<float>((I * 53) % 90));
		}
		for (int I = 0; I < 200; ++I)
		{
			A.push_back(10.f + 0.2f * static_cast<float>((I % 5) - 2) / 2.f);
		}
		const int IA = SettleIndex(A, Rule);
		std::snprintf(Buf, sizeof(Buf), "saccades de compilation puis 10 ms stables : accepte a l'image %d (au plus tot 149 : trois fenetres propres)", IA);
		Line(Buf, IA == 149);
		// Les seules images heurtees (30 a 120 ms, medianes voisines) : dispersion trop forte, jamais acceptees
		std::vector<float> Noisy(A.begin(), A.begin() + 60);
		for (int I = 0; I < 60; ++I)
		{
			Noisy.push_back(30.f + static_cast<float>((I * 71) % 90));
		}
		Line("images heurtees de medianes voisines (30 a 120 ms) : jamais acceptees (dispersion)", SettleIndex(Noisy, Rule) == -1);
		// Derive lente de 10 a 14 ms en 200 images : 0,6 ms par fenetre de 30
		std::vector<float> B;
		for (int I = 0; I < 200; ++I)
		{
			B.push_back(10.f + 4.f * static_cast<float>(I) / 200.f);
		}
		Line("derive lente (10 -> 14 ms, 0,6 ms par fenetre) : jamais acceptee", SettleIndex(B, Rule) == -1);
		// Stable, puis une saccade de 40 ms : refusee tant qu'elle est dans les trois dernieres fenetres (90 images)
		std::vector<float> C(120, 12.f);
		C.push_back(40.f);
		std::vector<float> Scratch(static_cast<size_t>(BRPerf::SettleSpan(Rule)));
		const bool bWithSpike = BRPerf::IsSettled(C.data(), static_cast<int>(C.size()), Rule, Scratch.data());
		for (int I = 0; I < 89; ++I)
		{
			C.push_back(12.f);
		}
		const bool bStill = BRPerf::IsSettled(C.data(), static_cast<int>(C.size()), Rule, Scratch.data());
		C.push_back(12.f);
		const bool bAfter = BRPerf::IsSettled(C.data(), static_cast<int>(C.size()), Rule, Scratch.data());
		Line("saccade de 40 ms : refusee pendant les 89 images suivantes, acceptee a la 90e", !bWithSpike && !bStill && bAfter);
		// Bruit de +/- 3 % autour de 8 ms
		std::vector<float> D;
		for (int I = 0; I < 150; ++I)
		{
			D.push_back(8.f * (1.f + 0.03f * static_cast<float>((I * 7) % 3 - 1)));
		}
		Line("bruit de +/- 3 % autour de 8 ms : accepte des la 90e image", SettleIndex(D, Rule) == 89);
		// Cinematique : 45 ms +/- 2 ms, une image a 60 ms (sous 2,5 x la mediane) : accepte
		std::vector<float> E;
		for (int I = 0; I < 90; ++I)
		{
			E.push_back(I == 50 ? 60.f : 45.f + 2.f * static_cast<float>((I % 3) - 1));
		}
		Line("images lentes (45 ms, une a 60 ms) : acceptees (ecart relatif, seuil de saccade proportionnel)", SettleIndex(E, Rule) == 89);
		Line("trop peu d'images (moins de trois fenetres) : refuse", !BRPerf::IsSettled(D.data(), 89, Rule, Scratch.data()));
	}

	template <typename StrT>
	bool ParseAs(const StrT& S, BRPerf::FGpuLine& Out)
	{
		return BRPerf::ParseGpuLine(S.c_str(), static_cast<int>(S.size()), Out);
	}

	void TestParse()
	{
		std::printf("Releve ProfileGPU (format du moteur)\n");
		char Buf[256];
		struct FCase
		{
			int Level;
			double Percent;
			double Ms;
			double Wait;
			const char* Name;
			/** Profondeur attendue (v4.16 relecture : "Other Children" = enfant de l'evenement de meme retrait) */
			int Depth;
			bool bOther;
		};
		const FCase Cases[] = {
			{ 0, 100.0, 23.45, 0.31, "<root>", 0, false },
			{ 1, 98.7, 23.15, 0.0, "Scene", 1, false },
			{ 2, 5.0, 1.17, 0.0, "BasePass", 2, false },
			{ 2, 12.3, 2.89, 0.0, "LumenScreenProbeGather", 2, false },
			{ 3, 0.1, 0.01, 0.0, "ShadowDepths 1024x1024 (Atlas 3)", 3, false },
			{ 4, 0.0, 0.00, 0.12, "Dispatch(12, 4, 1)", 4, false },
			{ 5, 64.2, 123.45, 0.0, "VolumetricFog::LightScattering 160x90x64", 5, false },
			{ 2, 1.5, 0.35, 0.0, "+ commence par plus", 2, false },
			{ 1, 3.3, 0.77, 0.0, "17 Other Children", 2, true },
			{ 0, 0.4, 0.09, 0.02, "3 Other Children", 1, true },
			{ 2, 0.2, 0.05, 0.0, "1 Other Child", 3, true },
			{ 2, 0.2, 0.05, 0.0, "Other Children", 2, false },
			{ 2, 0.2, 0.05, 0.0, "12 Other Childrens", 2, false },
		};
		int Ok = 0;
		int OkWide = 0;
		for (const FCase& Case : Cases)
		{
			const std::string S = EngineLine(Case.Level, Case.Percent, Case.Ms, Case.Wait, Case.Name);
			BRPerf::FGpuLine L;
			const bool bParsed = ParseAs(S, L);
			// v4.16 (perf, corrections de relecture) : "Other Children" ecrit occupation + attente : relu en occupation seule
			const double PrintedMs = std::round(Case.Ms * 100.0) / 100.0;
			const double PrintedWait = std::round(Case.Wait * 100.0) / 100.0;
			const double WantMs = (Case.bOther && PrintedWait > 0.0) ? std::max(0.0, PrintedMs - PrintedWait) : PrintedMs;
			const bool bGood = bParsed && L.Depth == Case.Depth && L.bOtherChildren == Case.bOther && Near(L.Ms, WantMs, 1e-9)
				&& Near(L.WaitMs, std::round(Case.Wait * 100.0) / 100.0, 1e-9) && Near(L.Percent, std::round(Case.Percent * 10.0) / 10.0, 1e-9)
				&& NameOf(S, L) == Case.Name;
			Ok += bGood ? 1 : 0;
			const std::wstring WS = Widen(S);
			BRPerf::FGpuLine WL;
			const bool bWide = ParseAs(WS, WL) && WL.Depth == L.Depth && WL.Ms == L.Ms && WL.WaitMs == L.WaitMs && WL.NameStart == L.NameStart && WL.NameLen == L.NameLen
				&& WL.bOtherChildren == L.bOtherChildren;
			OkWide += bWide ? 1 : 0;
			if (!bGood)
			{
				std::printf("    ligne mal lue : [%s]\n", S.c_str());
			}
		}
		std::snprintf(Buf, sizeof(Buf), "%d / %d lignes lues (profondeur, \"Other Children\" rattache au parent et sans attente, temps, attente, nom), %d / %d en larges", Ok,
			static_cast<int>(sizeof(Cases) / sizeof(Cases[0])), OkWide, static_cast<int>(sizeof(Cases) / sizeof(Cases[0])));
		Line(Buf, Ok == static_cast<int>(sizeof(Cases) / sizeof(Cases[0])) && OkWide == Ok);
		// Fin de ligne Windows et espaces de fin ignores
		BRPerf::FGpuLine Crlf;
		const std::string WithCr = EngineLine(1, 10.0, 2.5, 0.0, "PostProcessing") + "  \r";
		Line("fin de ligne \\r et espaces de fin retires du nom", ParseAs(WithCr, Crlf) && NameOf(WithCr, Crlf) == "PostProcessing");
		// Lignes etrangeres
		const char* const Foreign[] = { "", "GPU Profile for Frame 1234 - Graphics pipeline 0 - GPU 0", "    - Frame Time                    : 12.34ms",
			"    - r.ProfileGPU.Root           : \"*\"", "    No recorded work for this queue.", "Compute pipeline 0 - GPU 0", "12.5% sans temps", "abc 1.00ms   x",
			"  5.0% 1.00ms", "  5.0% 1.00msNom" };
		int Refused = 0;
		for (const char* F : Foreign)
		{
			BRPerf::FGpuLine L;
			Refused += BRPerf::ParseGpuLine(F, static_cast<int>(std::strlen(F)), L) ? 0 : 1;
		}
		std::snprintf(Buf, sizeof(Buf), "lignes etrangeres refusees : %d / %d (en-tetes, vide, sans nom, sans separateur)", Refused,
			static_cast<int>(sizeof(Foreign) / sizeof(Foreign[0])));
		Line(Buf, Refused == static_cast<int>(sizeof(Foreign) / sizeof(Foreign[0])));
		// En-tetes
		const std::string G = "GPU Profile for Frame 4567 - Graphics pipeline 0 - GPU 0";
		const std::string Cmp = "Compute pipeline 0 - GPU 0";
		const std::string Cpy = "Copy pipeline 1 - GPU 0";
		const std::string NotQueue = EngineLine(1, 2.0, 0.5, 0.0, "Scene");
		Line("en-tetes des files : graphique, calcul, copie ; une ligne d'evenement n'en est pas un",
			BRPerf::ParseQueueLine(G.c_str(), static_cast<int>(G.size())) == BRPerf::EQueueLine::Graphics
			&& BRPerf::ParseQueueLine(Cmp.c_str(), static_cast<int>(Cmp.size())) == BRPerf::EQueueLine::Other
			&& BRPerf::ParseQueueLine(Cpy.c_str(), static_cast<int>(Cpy.size())) == BRPerf::EQueueLine::Other
			&& BRPerf::ParseQueueLine(NotQueue.c_str(), static_cast<int>(NotQueue.size())) == BRPerf::EQueueLine::None);
		double Frame = 0.0;
		const std::string FT = EngineFrameTime(16.78);
		const std::wstring WFT = Widen(FT);
		double WFrame = 0.0;
		Line("temps de l'image de l'en-tete : 16,78 ms (etroit et large)", BRPerf::ParseFrameTimeLine(FT.c_str(), static_cast<int>(FT.size()), Frame) && Near(Frame, 16.78, 1e-9)
			&& BRPerf::ParseFrameTimeLine(WFT.c_str(), static_cast<int>(WFT.size()), WFrame) && WFrame == Frame);
	}

	void TestCategories()
	{
		std::printf("Postes du processeur graphique\n");
		struct FCase
		{
			const char* Name;
			int Cat;
		};
		using namespace BRPerf;
		const FCase Cases[] = {
			{ "MegaLights", CatMegaLights },
			{ "MegaLights RayTracing (spp=2)", CatMegaLights },
			{ "LumenSceneUpdate: 12 card captures 0.123M texels", CatLumen },
			{ "LumenScreenProbeGather", CatLumen },
			{ "LumenReflections HardwareRayTracing", CatLumen },
			{ "RayTracingScene", CatRtScene },
			{ "RayTracingBuildScene", CatRtScene },
			{ "BuildRayTracingGeometries", CatRtScene },
			{ "RayTracingDynamicGeometry", CatRtScene },
			{ "Acceleration Structure Build", CatRtScene },
			{ "ShadowDepths", CatShadows },
			{ "FVirtualShadowMapArray::BuildPageAllocation", CatShadows },
			{ "VolumetricFog", CatFog },
			{ "LocalFogVolumeVolumes", CatFog },
			{ "FogBehindWater", CatFog },
			{ "ExponentialHeightFog", CatFog },
			{ "Translucency", CatTranslucency },
			{ "SingleLayerWater", CatTranslucency },
			{ "FrontLayerTranslucency", CatTranslucency },
			{ "Distortion", CatTranslucency },
			{ "PostProcessing", CatPost },
			{ "TemporalSuperResolution", CatPost },
			{ "TSR RejectShading(#)", CatPost },
			{ "BasePass", CatBase },
			{ "PrePass DDM_AllOpaque (Forced by Nanite)", CatBase },
			{ "Nanite::VisBuffer", CatBase },
			{ "Lights", CatLighting },
			{ "BatchedLights", CatLighting },
			{ "UnbatchedLights", CatLighting },
			{ "DirectLighting", CatLighting },
			{ "RenderDeferredLighting", CatLighting },
			{ "ShadowProjectionOnOpaque", CatShadows },
			{ "RayTracedShadow (spp=1)", CatShadows },
			{ "LightScattering 160x90x64", CatOther },
			{ "LightShafts (Bloom)", CatPost },
			{ "SLW::LumenReflections", CatTranslucency },
			{ "SLW::CopySceneWithoutWater", CatTranslucency },
			{ "LumenFrontLayerTranslucencyReflections", CatLumen },
			{ "RayTracingDynamicUpdate", CatRtScene },
			{ "RayTracingBuildInstanceBuffer", CatRtScene },
			{ "InstanceCulling", CatGpuScene },
			{ "DiffuseIndirectAndAO", CatScreen },
			{ "ScreenSpaceReflections(Quality=#)", CatScreen },
			{ "HZB", CatScreen },
			{ "GPUSceneUpdate", CatGpuScene },
			{ "SlateUI", CatUi },
			{ "Scene", CatOther },
			{ "<root>", CatOther },
			{ "BuildRenderingCommands", CatOther },
			{ "HairStrandsRasterFrontDepth", CatOther },
			{ "17 Other Children", CatOther },
			{ "STAA", CatOther },
		};
		int Ok = 0;
		const int Num = static_cast<int>(sizeof(Cases) / sizeof(Cases[0]));
		for (const FCase& Case : Cases)
		{
			const int Got = CategoryOf(Case.Name, static_cast<int>(std::strlen(Case.Name)));
			Ok += Got == Case.Cat ? 1 : 0;
			if (Got != Case.Cat)
			{
				std::printf("    \"%s\" -> %s (attendu %s)\n", Case.Name, CategoryName(Got), CategoryName(Case.Cat));
			}
		}
		char Buf[200];
		std::snprintf(Buf, sizeof(Buf), "%d / %d noms de passes ramenes au bon poste", Ok, Num);
		Line(Buf, Ok == Num);
		const std::wstring W = L"LumenSceneLighting";
		Line("noms en caracteres larges (TCHAR)", CategoryOf(W.c_str(), static_cast<int>(W.size())) == CatLumen);
		Line("13 postes nommes, \"autre\" pour un indice inconnu", std::strcmp(CategoryName(CatUi), "interface") == 0 && std::strcmp(CategoryName(99), "autre") == 0
			&& std::strcmp(CategoryName(CatOther), "autre") == 0);
	}

	struct FRow
	{
		int Level;
		double Ms;
		const char* Name;
	};

	/** Lignes ecrites comme le moteur (retrait du niveau d'affichage, "Other Children" au retrait du parent), relues par
	 *  ParseGpuLine puis rangees en evenements */
	std::vector<BRPerf::FGpuNode> NodesFromEngineLines(const FRow* Rows, int Num, double FrameMs)
	{
		std::vector<BRPerf::FGpuNode> Nodes;
		for (int I = 0; I < Num; ++I)
		{
			const std::string S = EngineLine(Rows[I].Level, Rows[I].Ms / FrameMs * 100.0, Rows[I].Ms, 0.0, Rows[I].Name);
			BRPerf::FGpuLine L;
			if (!BRPerf::ParseGpuLine(S.c_str(), static_cast<int>(S.size()), L))
			{
				continue;
			}
			BRPerf::FGpuNode N;
			N.Depth = L.Depth;
			N.Ms = L.Ms;
			N.Cat = BRPerf::CategoryOf(S.c_str() + L.NameStart, L.NameLen);
			Nodes.push_back(N);
		}
		return Nodes;
	}

	void TestAttribution()
	{
		std::printf("Attribution sans double compte\n");
		using namespace BRPerf;
		// Releve d'une image de 30 ms tel que le moteur l'ecrit (ordre chronologique, prefixe) : "N Other Children" apres
		// les enfants affiches, AU RETRAIT DU PARENT (GPUProfiler.cpp : LeftPad(Level * 3))
		const FRow Rows[] = {
			{ 0, 30.00, "<root>" },
			{ 1, 27.00, "Scene" },
			{ 2, 1.00, "PrePass DDM_AllOpaque (Forced by Nanite)" },
			{ 2, 3.00, "BasePass" },
			{ 2, 2.00, "LumenSceneUpdate: 3 card captures 0.010M texels" },
			{ 3, 0.50, "ShadowDepths" },
			{ 2, 1.50, "ShadowDepths" },
			{ 2, 10.00, "RenderDeferredLighting" },
			{ 3, 4.00, "DiffuseIndirectAndAO" },
			{ 4, 3.00, "LumenScreenProbeGather" },
			{ 4, 0.60, "AmbientOcclusion" },
			{ 3, 2.50, "Lights" },
			{ 4, 1.00, "BatchedLights" },
			{ 4, 1.20, "ShadowProjectionOnOpaque" },
			{ 3, 0.20, "2 Other Children" },
			{ 3, 3.00, "MegaLights" },
			{ 2, 1.00, "VolumetricFog" },
			{ 3, 0.70, "LightScattering 160x90x64" },
			{ 2, 1.50, "Translucency" },
			{ 2, 1.00, "SingleLayerWater" },
			{ 3, 0.40, "SLW::LumenReflections" },
			{ 2, 2.00, "PostProcessing" },
			{ 3, 1.00, "TSR" },
			{ 2, 0.80, "RayTracingScene" },
			{ 1, 0.40, "7 Other Children" },
			{ 1, 0.50, "SlateUI" },
			{ 0, 0.30, "3 Other Children" },
		};
		const int NumRows = static_cast<int>(sizeof(Rows) / sizeof(Rows[0]));
		std::vector<FGpuNode> Nodes = NodesFromEngineLines(Rows, NumRows, 30.0);
		double Ms[NumCategories];
		const double Root = Attribute(Nodes.data(), static_cast<int>(Nodes.size()), Ms);
		char Buf[400];
		std::snprintf(Buf, sizeof(Buf), "racine %.2f ms (attendu 30, pas 30,3 : \"3 Other Children\" de la racine est son enfant)", Root);
		Line(Buf, static_cast<int>(Nodes.size()) == NumRows && Near(Root, 30.0));
		std::snprintf(Buf, sizeof(Buf), "lumen %.2f (5,0), megalights %.2f (3,0), ombres %.2f (2,7), eclairage %.2f (1,8), effets d'ecran %.2f (1,0)",
			Ms[CatLumen], Ms[CatMegaLights], Ms[CatShadows], Ms[CatLighting], Ms[CatScreen]);
		Line(Buf, Near(Ms[CatLumen], 5.0) && Near(Ms[CatMegaLights], 3.0) && Near(Ms[CatShadows], 2.7) && Near(Ms[CatLighting], 1.8) && Near(Ms[CatScreen], 1.0));
		std::snprintf(Buf, sizeof(Buf), "base %.2f (4,0), brouillard %.2f (1,0), translucidite et eau %.2f (2,5), post %.2f (2,0), scene RT %.2f (0,8), interface %.2f (0,5), autre %.2f (5,7)",
			Ms[CatBase], Ms[CatFog], Ms[CatTranslucency], Ms[CatPost], Ms[CatRtScene], Ms[CatUi], Ms[CatOther]);
		Line(Buf, Near(Ms[CatBase], 4.0) && Near(Ms[CatFog], 1.0) && Near(Ms[CatTranslucency], 2.5) && Near(Ms[CatPost], 2.0) && Near(Ms[CatRtScene], 0.8)
			&& Near(Ms[CatUi], 0.5) && Near(Ms[CatOther], 5.7));
		double Sum = 0.0;
		for (int C = 0; C < NumCategories; ++C)
		{
			Sum += Ms[C];
		}
		Line("ombre dans Lumen et LightScattering dans le brouillard gardent le poste du parent ; somme des postes = racine (30 ms)", Near(Sum, Root));
		// Postes plus grands que la racine (arrondis du releve) : un temps propre negatif est borne a 0
		FGpuNode Over[2];
		Over[0].Depth = 0;
		Over[0].Ms = 1.0;
		Over[1].Depth = 1;
		Over[1].Ms = 1.01;
		Over[1].Cat = CatPost;
		const double Root2 = Attribute(Over, 2, Ms);
		Line("arrondis : \"autre\" borne a 0", Near(Root2, 1.0) && Ms[CatOther] == 0.0 && Near(Ms[CatPost], 1.01));
		// File de calcul asynchrone (deux racines dans la meme liste) et saut de profondeur (3 apres 1 : enfant du 1)
		const FRow Async[] = {
			{ 0, 2.00, "<root>" },
			{ 1, 1.50, "LumenSceneLighting" },
			{ 3, 0.40, "DirectLighting" },
			{ 1, 0.30, "HZB" },
			{ 0, 1.00, "<root>" },
			{ 1, 0.60, "RayTracingDynamicUpdate" },
		};
		std::vector<FGpuNode> AsyncNodes = NodesFromEngineLines(Async, static_cast<int>(sizeof(Async) / sizeof(Async[0])), 2.0);
		const double AsyncRoot = Attribute(AsyncNodes.data(), static_cast<int>(AsyncNodes.size()), Ms);
		std::snprintf(Buf, sizeof(Buf), "deux files : racines %.2f (3,0) ; lumen %.2f (1,5 : DirectLighting a la profondeur 3 reste dans Lumen), effets d'ecran %.2f, scene RT %.2f, autre %.2f (0,6)",
			AsyncRoot, Ms[CatLumen], Ms[CatScreen], Ms[CatRtScene], Ms[CatOther]);
		Line(Buf, Near(AsyncRoot, 3.0) && Near(Ms[CatLumen], 1.5) && Near(Ms[CatScreen], 0.3) && Near(Ms[CatRtScene], 0.6) && Near(Ms[CatOther], 0.6)
			&& Near(Ms[CatLighting], 0.0));

		// v4.16 (perf, corrections de relecture) : attente des enfants caches sur la file graphique. Le moteur ecrit la racine
		// en occupation seule (10 ms, " + 0.40 Wait" a part) et "k Other Children" en occupation + attente (1,00 + 0,40 =
		// 1,40 ms, " + 0.40 Wait"). Temps propre de la racine : 10 - 9 - 1 = 0. Avant : 10 - 9 - 1,4 borne a 0, et les postes
		// faisaient 10,4 ms pour une racine de 10
		struct FWaitRow
		{
			int Level;
			double Ms;
			double WaitMs;
			const char* Name;
		};
		const FWaitRow WaitRows[] = {
			{ 0, 10.00, 0.40, "<root>" },
			{ 1, 9.00, 0.00, "Scene" },
			{ 2, 9.00, 0.00, "BasePass" },
			{ 0, 1.40, 0.40, "4 Other Children" },
		};
		std::vector<FGpuNode> WaitNodes;
		double OtherWait = -1.0;
		double OtherMs = -1.0;
		for (const FWaitRow& Row : WaitRows)
		{
			const std::string S = EngineLine(Row.Level, (Row.Ms + Row.WaitMs) / 10.4 * 100.0, Row.Ms, Row.WaitMs, Row.Name);
			FGpuLine L;
			if (!ParseGpuLine(S.c_str(), static_cast<int>(S.size()), L))
			{
				continue;
			}
			if (L.bOtherChildren)
			{
				OtherWait = L.WaitMs;
				OtherMs = L.Ms;
			}
			FGpuNode N;
			N.Depth = L.Depth;
			N.Ms = L.Ms;
			N.Cat = CategoryOf(S.c_str() + L.NameStart, L.NameLen);
			WaitNodes.push_back(N);
		}
		const double WaitRoot = Attribute(WaitNodes.data(), static_cast<int>(WaitNodes.size()), Ms);
		double WaitSum = 0.0;
		for (int C = 0; C < NumCategories; ++C)
		{
			WaitSum += Ms[C];
		}
		std::snprintf(Buf, sizeof(Buf), "\"4 Other Children\" ecrit 1,40 ms + 0,40 Wait : relu %.2f ms (attente %.2f) ; racine %.2f, somme des postes %.2f (10), base %.2f, autre %.2f (1,0)",
			OtherMs, OtherWait, WaitRoot, WaitSum, Ms[CatBase], Ms[CatOther]);
		Line(Buf, static_cast<int>(WaitNodes.size()) == 4 && Near(OtherMs, 1.0) && Near(OtherWait, 0.4) && Near(WaitRoot, 10.0) && Near(WaitSum, WaitRoot)
			&& Near(Ms[CatBase], 9.0) && Near(Ms[CatOther], 1.0));
	}

	void TestPathsAndMedian()
	{
		std::printf("Chemins stables et mediane des releves\n");
		char Out[128];
		const char* const In = "ShadowDepths 1024x2048 (Atlas 3)   ";
		const int N = BRPerf::NormalizeName(In, static_cast<int>(std::strlen(In)), Out, static_cast<int>(sizeof(Out)));
		Line("\"ShadowDepths 1024x2048 (Atlas 3)\" -> \"ShadowDepths #x# (Atlas #)\"", std::string(Out, static_cast<size_t>(N)) == "ShadowDepths #x# (Atlas #)");
		wchar_t WOut[64];
		const std::wstring WIn = L"View0 Frame 123";
		const int WN = BRPerf::NormalizeName(WIn.c_str(), static_cast<int>(WIn.size()), WOut, 64);
		Line("caracteres larges : \"View0 Frame 123\" -> \"View# Frame #\"", std::wstring(WOut, static_cast<size_t>(WN)) == L"View# Frame #");
		char Small[6];
		const int SN = BRPerf::NormalizeName("ABCDEFGHIJ", 10, Small, 6);
		Line("tampon trop court : tronque et termine", SN == 5 && Small[5] == '\0' && std::string(Small) == "ABCDE");
		double Three[3] = { 2.5, 1.5, 9.0 };
		double Four[4] = { 4.0, 1.0, 3.0, 2.0 };
		Line("mediane de 3 releves (2,5 ; 1,5 ; 9) = 2,5 ; de 4 (1..4) = 2,5", Near(BRPerf::MedianInPlace(Three, 3), 2.5) && Near(BRPerf::MedianInPlace(Four, 4), 2.5));
	}
}

int main()
{
	std::printf("# Banc des mesures de performance v4.16 (BRPerfLogic.h)\n\n");
	TestSummary();
	TestSettle();
	TestParse();
	TestCategories();
	TestAttribution();
	TestPathsAndMedian();
	std::printf("\nVerifications : %d, echecs : %d\nRESULTAT : %s\n", GChecks, GFailures, GFailures == 0 ? "OK" : "ECHEC");
	return GFailures == 0 ? 0 : 1;
}
