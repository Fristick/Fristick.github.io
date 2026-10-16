// v4.15 (realisme) : banc hors moteur de la couleur des lampes vue par la camera (BRColorLogic.h, copie des formules
// d'Unreal 5.8 : temperature de couleur des lampes, balance des blancs du post-process).
//
// Il verifie le modele (D65 neutre, une lampe vue avec une balance a sa temperature reste blanche sous 4000 K, le virage a la
// peche d'une balance a 4000 K ou plus que le moteur prend sur le lieu du jour), puis LIT les reglages des niveaux dans
// Source/Backrooms/Private/BRLevels.cpp (LightTemperatureK, WhiteTempK, WhiteTint, LightColor : aucune table recopiee) et
// controle ce que la v4.15 annonce :
//   - aucune lampe a temperature ne vire au rose ou a la peche (teinte entre 25 et 75 degres si elle est chaude, entre 180
//     et 250 si elle est froide) ;
//   - Level 0 : lampe blanc chaud moins saturee qu'avant, papier peint d'un jaune moins sature qu'avant (decision : aspect
//     de "Backrooms: Escape Together", fluorescents d'environ 3700 K, jaune naturel) ;
//   - Level 2 : la couleur des ampoules d'avant (1, 0,7, 0,45) ; Level 37 : blanc chaud, peu sature ;
//   - une lampe a temperature n'existe que dans un niveau du banc visuel (-BRAutoTestVisual : 0, 1, 2, 37, 6) ; un niveau
//     sans temperature garde la balance neutre (6500 K, sans teinte) : la couleur RGB d'avant.
// Le rendu reel (tonemapper, eclairage global) se juge sur les captures du banc visuel.
//
//   clang-cl /nologo /std:c++17 /O2 /EHsc /utf-8 /W4 /WX -Wshadow -Wno-c++98-compat /I Source\Backrooms\Public Tools\Light\test_color.cpp
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Light/test_color.cpp -o test_color
//   test_color [chemin de Source/Backrooms/Private]   (depuis Backrooms/ par defaut)
#include "BRColorLogic.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>

namespace
{
	int GChecks = 0, GFailures = 0;
	void Line(const char* What, bool bOk)
	{
		++GChecks;
		GFailures += bOk ? 0 : 1;
		std::printf("  %-104s %s\n", What, bOk ? "OK" : "ECHEC");
	}

	struct FLevel
	{
		float TemperatureK = 0.f;
		float WhiteTempK = 6500.f;
		float WhiteTint = 0.f;
		float Color[3] = { 1.f, 1.f, 1.f };
	};
	std::map<int, FLevel> GLevels;

	std::string ReadFile(const std::string& Path)
	{
		std::ifstream In(Path);
		std::stringstream S;
		S << In.rdbuf();
		return S.str();
	}

	float ParseFloat(const std::string& Text)
	{
		return static_cast<float>(std::atof(Text.c_str()));
	}

	// FBRLevelDef LevelN() { ... D.Number = N; ... return D; } : un bloc par niveau
	bool ParseLevels(const std::string& Src)
	{
		const std::regex FnRe(R"(FBRLevelDef\s+Level\w+\(\)\s*\{)");
		const std::regex NumberRe(R"(D\.Number\s*=\s*(\d+)\s*;)");
		const std::regex TempRe(R"(D\.LightTemperatureK\s*=\s*(-?[0-9.]+)f?\s*;)");
		const std::regex WhiteRe(R"(D\.WhiteTempK\s*=\s*(-?[0-9.]+)f?\s*;)");
		const std::regex TintRe(R"(D\.WhiteTint\s*=\s*(-?[0-9.]+)f?\s*;)");
		const std::regex ColorRe(R"(D\.LightColor\s*=\s*C\(\s*(-?[0-9.]+)f?\s*,\s*(-?[0-9.]+)f?\s*,\s*(-?[0-9.]+)f?\s*\))");
		std::smatch M;
		for (auto It = std::sregex_iterator(Src.begin(), Src.end(), FnRe); It != std::sregex_iterator(); ++It)
		{
			const size_t Start = static_cast<size_t>(It->position(0));
			const size_t End = Src.find("return D;", Start);
			if (End == std::string::npos)
			{
				return false;
			}
			const std::string Body = Src.substr(Start, End - Start);
			if (!std::regex_search(Body, M, NumberRe))
			{
				continue;
			}
			FLevel L;
			const int Number = std::atoi(M[1].str().c_str());
			if (std::regex_search(Body, M, TempRe))
			{
				L.TemperatureK = ParseFloat(M[1].str());
			}
			if (std::regex_search(Body, M, WhiteRe))
			{
				L.WhiteTempK = ParseFloat(M[1].str());
			}
			if (std::regex_search(Body, M, TintRe))
			{
				L.WhiteTint = ParseFloat(M[1].str());
			}
			if (std::regex_search(Body, M, ColorRe))
			{
				for (int I = 0; I < 3; ++I)
				{
					L.Color[I] = ParseFloat(M[I + 1].str());
				}
			}
			GLevels[Number] = L;
		}
		return GLevels.size() >= 10;
	}

	/** Couleur d'une surface d'albedo Albedo eclairee par la lampe du niveau, vue par la camera, maximum ramene a 1 */
	void Seen(const FLevel& L, const float Albedo[3], float Out[3])
	{
		const float Tint[3] = { L.Color[0] * Albedo[0], L.Color[1] * Albedo[1], L.Color[2] * Albedo[2] };
		BRColor::ApparentLamp(L.TemperatureK, Tint, L.WhiteTempK, L.WhiteTint, Out);
	}

	bool Near(const float A[3], const float B[3], float Tol)
	{
		for (int I = 0; I < 3; ++I)
		{
			if (std::fabs(A[I] - B[I]) > Tol)
			{
				return false;
			}
		}
		return true;
	}
}

int main(int Argc, char** Argv)
{
	const std::string Dir = Argc > 1 ? Argv[1] : "Source/Backrooms/Private";
	char Buf[512];
	std::printf("Banc de la couleur des lampes v4.15 (BRColorLogic.h ; niveaux lus dans %s/BRLevels.cpp)\n", Dir.c_str());

	std::printf("Modele (formules du moteur)\n");
	{
		const float White[3] = { 1.f, 1.f, 1.f };
		float Out[3];
		BRColor::WhiteBalance(White, 6500.f, 0.f, Out);
		std::snprintf(Buf, sizeof(Buf), "balance des blancs a 6500 K : neutre (%.3f, %.3f, %.3f)", Out[0], Out[1], Out[2]);
		Line(Buf, Near(Out, White, 0.005f));
		float Lamp[3];
		BRColor::LampRgb(3700.f, Lamp);
		BRColor::WhiteBalance(Lamp, 3700.f, 0.f, Out);
		const float Peak = Out[0] > Out[1] ? (Out[0] > Out[2] ? Out[0] : Out[2]) : (Out[1] > Out[2] ? Out[1] : Out[2]);
		const float Norm[3] = { Out[0] / Peak, Out[1] / Peak, Out[2] / Peak };
		std::snprintf(Buf, sizeof(Buf), "lampe de 3700 K vue avec une balance a 3700 K (lieu de Planck) : blanche (%.3f, %.3f, %.3f)", Norm[0], Norm[1], Norm[2]);
		Line(Buf, Near(Norm, White, 0.01f));
		const float One[3] = { 1.f, 1.f, 1.f };
		float Peach[3];
		BRColor::ApparentLamp(3700.f, One, 4300.f, 0.f, Peach);
		float Hue = 0.f;
		float Sat = 0.f;
		BRColor::HueSat(Peach, Hue, Sat);
		const float Expected[3] = { 1.f, 0.81f, 0.70f };
		std::snprintf(Buf, sizeof(Buf), "balance a 4300 K (lieu du jour) : la lampe de 3700 K vire a la peche (%.2f, %.2f, %.2f), teinte %.0f", Peach[0], Peach[1], Peach[2], Hue);
		Line(Buf, Near(Peach, Expected, 0.02f) && Hue < 25.f);
	}

	const bool bParsed = ParseLevels(ReadFile(Dir + "/BRLevels.cpp"));
	std::snprintf(Buf, sizeof(Buf), "BRLevels.cpp : %d niveaux lus", static_cast<int>(GLevels.size()));
	Line(Buf, bParsed);
	if (!bParsed)
	{
		std::printf("RESULTAT : %d/%d verification(s) en echec\n", GFailures, GChecks);
		return 1;
	}

	std::printf("Niveaux\n");
	const int Bench[] = { 0, 1, 2, 37, 6 };
	const float One[3] = { 1.f, 1.f, 1.f };
	for (const auto& Pair : GLevels)
	{
		const FLevel& L = Pair.second;
		float Lamp[3];
		Seen(L, One, Lamp);
		float Hue = 0.f;
		float Sat = 0.f;
		BRColor::HueSat(Lamp, Hue, Sat);
		if (L.TemperatureK <= 0.f)
		{
			std::snprintf(Buf, sizeof(Buf), "Level %d : couleur RGB (%.2f, %.2f, %.2f), balance neutre (%.0f K, teinte %.2f)", Pair.first, L.Color[0], L.Color[1],
				L.Color[2], L.WhiteTempK, L.WhiteTint);
			Line(Buf, std::fabs(L.WhiteTempK - 6500.f) < 1.f && std::fabs(L.WhiteTint) < 1e-4f);
			continue;
		}
		bool bInBench = false;
		for (const int B : Bench)
		{
			bInBench = bInBench || B == Pair.first;
		}
		const bool bWarm = Hue >= 25.f && Hue <= 75.f;
		const bool bCool = Hue >= 180.f && Hue <= 250.f;
		std::snprintf(Buf, sizeof(Buf), "Level %d : lampe %.0f K, balance %.0f K teinte %.2f : vue (%.2f, %.2f, %.2f), teinte %.0f, saturation %.2f%s", Pair.first,
			L.TemperatureK, L.WhiteTempK, L.WhiteTint, Lamp[0], Lamp[1], Lamp[2], Hue, Sat, bInBench ? "" : " (HORS DU BANC VISUEL)");
		Line(Buf, bInBench && (Sat < 0.03f || bWarm || bCool));
	}

	// Decisions de la v4.15 (couleurs d'avant : LightColor de la v4.14, balance neutre)
	if (GLevels.count(0))
	{
		const FLevel& L = GLevels[0];
		const float Before[3] = { 1.f, 0.96f, 0.84f };
		float HueBefore = 0.f;
		float SatBefore = 0.f;
		BRColor::HueSat(Before, HueBefore, SatBefore);
		float Lamp[3];
		Seen(L, One, Lamp);
		float Hue = 0.f;
		float Sat = 0.f;
		BRColor::HueSat(Lamp, Hue, Sat);
		std::snprintf(Buf, sizeof(Buf), "Level 0 : lampe blanc chaud (teinte %.0f, saturation %.2f ; avant %.0f / %.2f)", Hue, Sat, HueBefore, SatBefore);
		Line(Buf, Hue >= 25.f && Hue <= 50.f && Sat < SatBefore);
		// Papier peint du Level 0 (moyenne de T_L0_Wallpaper)
		const float Paper[3] = { 0.67f, 0.65f, 0.40f };
		const float PaperBefore[3] = { Paper[0] * Before[0], Paper[1] * Before[1], Paper[2] * Before[2] };
		float WallBefore[3];
		BRColor::WhiteBalance(PaperBefore, 6500.f, 0.f, WallBefore);
		float HueWallBefore = 0.f;
		float SatWallBefore = 0.f;
		BRColor::HueSat(WallBefore, HueWallBefore, SatWallBefore);
		float Wall[3];
		Seen(L, Paper, Wall);
		float HueWall = 0.f;
		float SatWall = 0.f;
		BRColor::HueSat(Wall, HueWall, SatWall);
		std::snprintf(Buf, sizeof(Buf), "Level 0 : papier peint jaune moins sature (teinte %.0f, saturation %.2f ; avant %.0f / %.2f)", HueWall, SatWall, HueWallBefore,
			SatWallBefore);
		Line(Buf, HueWall >= 42.f && HueWall <= 58.f && SatWall < SatWallBefore);
	}
	if (GLevels.count(2))
	{
		const float Before[3] = { 1.f, 0.7f, 0.45f };
		float Lamp[3];
		Seen(GLevels[2], One, Lamp);
		std::snprintf(Buf, sizeof(Buf), "Level 2 : ampoules de la meme couleur qu'avant (%.2f, %.2f, %.2f ; avant 1, 0,70, 0,45)", Lamp[0], Lamp[1], Lamp[2]);
		Line(Buf, Near(Lamp, Before, 0.03f));
	}
	if (GLevels.count(37))
	{
		float Lamp[3];
		Seen(GLevels[37], One, Lamp);
		float Hue = 0.f;
		float Sat = 0.f;
		BRColor::HueSat(Lamp, Hue, Sat);
		std::snprintf(Buf, sizeof(Buf), "Level 37 : plafonniers blanc chaud peu satures (%.2f, %.2f, %.2f), teinte %.0f, saturation %.2f", Lamp[0], Lamp[1], Lamp[2], Hue, Sat);
		Line(Buf, Hue >= 30.f && Hue <= 60.f && Sat <= 0.16f);
	}

	// v4.15 : lampe portee compensee par la balance des blancs du niveau (BRCharacter::UpdateLampColor) : a l'ecran, le blanc
	// chaud d'avant (1, 0,93, 0,8), sans virer au bleu-vert ; Gain : luminosite gardee
	std::printf("Lampe portee\n");
	for (const auto& Pair : GLevels)
	{
		const FLevel& L = Pair.second;
		if (std::fabs(L.WhiteTempK - 6500.f) < 1.f && std::fabs(L.WhiteTint) < 1e-4f)
		{
			continue;
		}
		const float Torch[3] = { 1.f, 0.93f, 0.8f };
		float Lamp[3];
		float Gain = 1.f;
		BRColor::UnbalancedLamp(Torch, L.WhiteTempK, L.WhiteTint, Lamp, &Gain);
		float Raw[3];
		BRColor::WhiteBalance(Lamp, L.WhiteTempK, L.WhiteTint, Raw);
		const float Peak = Raw[0] > Raw[1] ? (Raw[0] > Raw[2] ? Raw[0] : Raw[2]) : (Raw[1] > Raw[2] ? Raw[1] : Raw[2]);
		const float Back[3] = { Raw[0] / Peak, Raw[1] / Peak, Raw[2] / Peak };
		float Plain[3];
		BRColor::WhiteBalance(Torch, L.WhiteTempK, L.WhiteTint, Plain);
		std::snprintf(Buf, sizeof(Buf), "Level %d : lampe (%.2f, %.2f, %.2f) vue (%.2f, %.2f, %.2f), luminosite %.0f %% (sans compensation : %.2f, %.2f, %.2f)", Pair.first,
			Lamp[0], Lamp[1], Lamp[2], Back[0], Back[1], Back[2], Gain * 100.f, Plain[0] / Plain[0], Plain[1] / Plain[0], Plain[2] / Plain[0]);
		Line(Buf, Near(Back, Torch, 0.01f) && Gain > 0.6f);
	}

	std::printf("RESULTAT : %s (%d verification(s))\n", GFailures == 0 ? "OK" : "ECHEC", GChecks);
	return GFailures == 0 ? 0 : 1;
}
