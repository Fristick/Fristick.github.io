// v4.16 (perf) : logique des mesures de performance du test automatique (-BRAutoTest -BRAutoTestPerf, BRAutoTestV416Perf.cpp),
// en C++ pur (aucun type du moteur) : le banc Tools/Perf/test_perf.cpp la verifie avec GCC, Clang et MSVC.
//   - resume d'une serie de temps d'image : moyenne, mediane, centiles 95 et 99 (meme regle que le rapport du test : indice
//     plancher de P x (N - 1)), pire image, moyenne du 1 % le plus lent, longues images (au-dela de 33,3, 50 et 100 ms) ;
//   - prechauffage : temps d'image stabilises (medianes de trois fenetres successives proches, dispersion faible, aucune
//     saccade recente) ;
//   - releve de ProfileGPU en lignes compactes (r.ProfileGPU.TableFormatting 0) : le format exact du moteur
//     (Engine/Source/Runtime/RHI/Private/GPUProfiler.cpp, LogTree) est
//         "%s%4.1f%% %5.2fms%s   %s" : retrait de 3 espaces par niveau, pourcentage, temps inclusif, " + %2.2f Wait"
//         facultatif, nom de l'evenement ;
//     en-tetes "GPU Profile for Frame %d - Graphics pipeline %d - GPU %d" (file graphique), "%s pipeline %d - GPU %d" (autres
//     files) et "    - Frame Time                    : %.2fms" ;
//     v4.16 (perf, relecture) : la ligne "%d Other Children" (enfants sous le seuil) est ecrite par le moteur AU RETRAIT DE
//     SON PARENT (LeftPad(Level * 3), pas Level + 1 comme dans le tableau) : elle est rattachee a ce parent (profondeur + 1) ;
//     lue telle quelle, celle de la racine s'ajoutait a la racine (temps de l'image compte deux fois) ;
//     v4.16 (perf, corrections de relecture) : sur la file graphique, son temps est occupation + attente des enfants caches
//     (les autres lignes : occupation seule) : l'attente en est retiree ;
//   - postes du processeur graphique (Lumen, MegaLights, ombres, brouillard, scene ray tracee, translucidite et eau, post,
//     passe de base, eclairage direct, effets d'ecran, scene GPU, interface) d'apres le nom des evenements, et attribution
//     sans double compte : chaque evenement apporte son temps PROPRE (inclusif - enfants affiches) a son poste effectif.
//     v4.16 (perf, relecture) : le poste effectif est celui du plus proche ancetre reconnu, sauf sous un poste "contenant"
//     (eclairage, effets d'ecran, aucun) ou le poste de l'evenement l'emporte : RenderDeferredLighting contient
//     DiffuseIndirectAndAO (Lumen), Lights (ombres projetees) et MegaLights ; avec la regle du plus haut ancetre, tout cela
//     comptait en "eclairage". Les autres postes s'imposent a leurs descendants (une ombre dans Lumen reste a Lumen).
#pragma once

#include <algorithm>
#include <cstdint>

namespace BRPerf
{
	// ------------------------------------------------------------------------------------------------ Temps d'image

	constexpr int NumLongThresholds = 3;
	/** Longues images (ms) : sous 30, 20 et 10 images/s (strictement au-dela) */
	constexpr float LongFrameMs[NumLongThresholds] = { 1000.f / 30.f, 50.f, 100.f };

	/** Centile d'une serie triee (croissante) : indice plancher de P x (N - 1), comme ABRAutoTest::EndMeasure */
	inline float PercentileSorted(const float* Sorted, int Num, float P)
	{
		if (!Sorted || Num <= 0)
		{
			return 0.f;
		}
		int Index = static_cast<int>(P * static_cast<float>(Num - 1));
		Index = Index < 0 ? 0 : (Index >= Num ? Num - 1 : Index);
		return Sorted[Index];
	}

	struct FSummary
	{
		int Frames = 0;
		double Sum = 0.0;
		float Mean = 0.f;
		float Median = 0.f;
		float P95 = 0.f;
		float P99 = 0.f;
		float Worst = 0.f;
		/** Moyenne du 1 % le plus lent (au moins une image) : le "1 % bas" du rapport vaut 1000 / Low1Mean */
		float Low1Mean = 0.f;
		int Long[NumLongThresholds] = { 0, 0, 0 };
	};

	/** Resume d'une serie de temps (ms). Values est triee sur place */
	inline FSummary Summarize(float* Values, int Num)
	{
		FSummary S;
		if (!Values || Num <= 0)
		{
			return S;
		}
		for (int I = 0; I < Num; ++I)
		{
			S.Sum += static_cast<double>(Values[I]);
			for (int K = 0; K < NumLongThresholds; ++K)
			{
				S.Long[K] += Values[I] > LongFrameMs[K] ? 1 : 0;
			}
		}
		std::sort(Values, Values + Num);
		S.Frames = Num;
		S.Mean = static_cast<float>(S.Sum / static_cast<double>(Num));
		S.Median = PercentileSorted(Values, Num, 0.5f);
		S.P95 = PercentileSorted(Values, Num, 0.95f);
		S.P99 = PercentileSorted(Values, Num, 0.99f);
		S.Worst = Values[Num - 1];
		const int NumLow = Num / 100 > 1 ? Num / 100 : 1;
		double SumLow = 0.0;
		for (int I = Num - NumLow; I < Num; ++I)
		{
			SumLow += static_cast<double>(Values[I]);
		}
		S.Low1Mean = static_cast<float>(SumLow / static_cast<double>(NumLow));
		return S;
	}

	/** Mediane de Num valeurs (copiees dans Scratch, au moins Num places, puis triees) */
	inline float MedianOf(const float* Values, int Num, float* Scratch)
	{
		if (!Values || !Scratch || Num <= 0)
		{
			return 0.f;
		}
		std::copy(Values, Values + Num, Scratch);
		std::sort(Scratch, Scratch + Num);
		return PercentileSorted(Scratch, Num, 0.5f);
	}

	/** Prechauffage : les temps d'image sont-ils stabilises ? Trois fenetres successives de Window images */
	struct FSettleRule
	{
		int Window = 30;
		/** Ecart admis entre la mediane de la derniere fenetre et celles des deux precedentes : relatif OU absolu (ms) ; la
		 *  premiere fenetre est a deux fenetres de la derniere (une derive lente se voit) */
		float RelTol = 0.03f;
		float AbsTolMs = 0.3f;
		/** Dispersion : ecart interquartile des 3 x Window images au plus max(IqrRel x mediane, IqrAbsMs) */
		float IqrRel = 0.3f;
		float IqrAbsMs = 1.f;
		/** Saccade : une image des 3 x Window au-dela de max(SpikeRatio x mediane, mediane + SpikeAbsMs) (mediane de la
		 *  derniere fenetre) */
		float SpikeRatio = 2.5f;
		float SpikeAbsMs = 8.f;
	};

	/** Nombre d'images que IsSettled examine (et place requise dans Scratch) */
	inline int SettleSpan(const FSettleRule& Rule)
	{
		return Rule.Window > 0 ? 3 * Rule.Window : 0;
	}

	/** Recent : temps des dernieres images (ms, ordre chronologique), au moins SettleSpan ; Scratch : au moins SettleSpan
	 *  places. Stables : medianes des trois dernieres fenetres proches de la derniere, dispersion faible, aucune saccade */
	inline bool IsSettled(const float* Recent, int Num, const FSettleRule& Rule, float* Scratch)
	{
		const int W = Rule.Window;
		const int Span = SettleSpan(Rule);
		if (!Recent || !Scratch || W <= 0 || Num < Span)
		{
			return false;
		}
		const float* First = Recent + (Num - Span);
		const float* Mid = First + W;
		const float* Last = Mid + W;
		const float MedLast = MedianOf(Last, W, Scratch);
		const float MedMid = MedianOf(Mid, W, Scratch);
		const float MedFirst = MedianOf(First, W, Scratch);
		const float Refs[2] = { MedMid, MedFirst };
		for (const float Ref : Refs)
		{
			const float Diff = MedLast > Ref ? MedLast - Ref : Ref - MedLast;
			if (Diff > Rule.AbsTolMs && Diff > Rule.RelTol * Ref)
			{
				return false;
			}
		}
		std::copy(First, First + Span, Scratch);
		std::sort(Scratch, Scratch + Span);
		const float Iqr = PercentileSorted(Scratch, Span, 0.75f) - PercentileSorted(Scratch, Span, 0.25f);
		const float Median = PercentileSorted(Scratch, Span, 0.5f);
		const float IqrLimit = Rule.IqrRel * Median > Rule.IqrAbsMs ? Rule.IqrRel * Median : Rule.IqrAbsMs;
		if (Iqr > IqrLimit)
		{
			return false;
		}
		const float RatioLimit = Rule.SpikeRatio * MedLast;
		const float AbsLimit = MedLast + Rule.SpikeAbsMs;
		const float Limit = RatioLimit > AbsLimit ? RatioLimit : AbsLimit;
		for (int I = 0; I < Span; ++I)
		{
			if (First[I] > Limit)
			{
				return false;
			}
		}
		return true;
	}

	/** Mediane d'une petite serie de doubles (triee sur place) ; moyenne des deux valeurs centrales si Num est pair */
	inline double MedianInPlace(double* Values, int Num)
	{
		if (!Values || Num <= 0)
		{
			return 0.0;
		}
		std::sort(Values, Values + Num);
		return (Num % 2) == 1 ? Values[Num / 2] : 0.5 * (Values[Num / 2 - 1] + Values[Num / 2]);
	}

	// ------------------------------------------------------------------------------------------------ Texte (ASCII)

	template <typename CharT>
	inline int LowerAscii(CharT C)
	{
		const int V = static_cast<int>(C);
		return (V >= 'A' && V <= 'Z') ? V + ('a' - 'A') : V;
	}

	template <typename CharT>
	inline bool IsBlank(CharT C)
	{
		const int V = static_cast<int>(C);
		return V == ' ' || V == '\t' || V == '\r' || V == '\n';
	}

	template <typename CharT>
	inline bool IsDigit(CharT C)
	{
		const int V = static_cast<int>(C);
		return V >= '0' && V <= '9';
	}

	/** Position de Needle (ASCII, casse exacte ou non) dans S[0, Len), -1 sinon */
	template <typename CharT>
	inline int FindAscii(const CharT* S, int Len, const char* Needle, bool bIgnoreCase = false)
	{
		if (!S || !Needle)
		{
			return -1;
		}
		int NeedleLen = 0;
		while (Needle[NeedleLen] != '\0')
		{
			++NeedleLen;
		}
		if (NeedleLen == 0 || NeedleLen > Len)
		{
			return NeedleLen == 0 ? 0 : -1;
		}
		for (int At = 0; At + NeedleLen <= Len; ++At)
		{
			int K = 0;
			while (K < NeedleLen)
			{
				const int A = bIgnoreCase ? LowerAscii(S[At + K]) : static_cast<int>(S[At + K]);
				const int B = bIgnoreCase ? LowerAscii(Needle[K]) : static_cast<int>(Needle[K]);
				if (A != B)
				{
					break;
				}
				++K;
			}
			if (K == NeedleLen)
			{
				return At;
			}
		}
		return -1;
	}

	/** Nombre decimal positif "123", "12.5", ".5" a partir de Pos (avance Pos) */
	template <typename CharT>
	inline bool ParseNumber(const CharT* S, int Len, int& Pos, double& Out)
	{
		int P = Pos;
		double IntPart = 0.0;
		bool bDigits = false;
		while (P < Len && IsDigit(S[P]))
		{
			IntPart = IntPart * 10.0 + static_cast<double>(static_cast<int>(S[P]) - '0');
			++P;
			bDigits = true;
		}
		double Frac = 0.0;
		double Div = 1.0;
		if (P < Len && static_cast<int>(S[P]) == '.')
		{
			++P;
			while (P < Len && IsDigit(S[P]))
			{
				Frac = Frac * 10.0 + static_cast<double>(static_cast<int>(S[P]) - '0');
				Div *= 10.0;
				++P;
				bDigits = true;
			}
		}
		if (!bDigits)
		{
			return false;
		}
		Out = IntPart + Frac / Div;
		Pos = P;
		return true;
	}

	/** Copie de Name ou chaque suite de chiffres devient '#' (numeros d'image, de vue, de lot : chemins stables d'un releve a
	 *  l'autre), espaces de fin retires. Rend la longueur ecrite (au plus OutCap - 1, Out termine par 0) */
	template <typename CharT>
	inline int NormalizeName(const CharT* Name, int Len, CharT* Out, int OutCap)
	{
		if (!Out || OutCap <= 0)
		{
			return 0;
		}
		int N = 0;
		bool bInDigits = false;
		for (int I = 0; Name && I < Len && N < OutCap - 1; ++I)
		{
			if (IsDigit(Name[I]))
			{
				if (!bInDigits)
				{
					Out[N++] = static_cast<CharT>('#');
				}
				bInDigits = true;
				continue;
			}
			bInDigits = false;
			Out[N++] = Name[I];
		}
		while (N > 0 && IsBlank(Out[N - 1]))
		{
			--N;
		}
		Out[N] = static_cast<CharT>(0);
		return N;
	}

	// ------------------------------------------------------------------------------------------------ Releve ProfileGPU

	/** Une ligne d'evenement du releve compact */
	struct FGpuLine
	{
		int Depth = 0;
		double Percent = 0.0;
		/** Temps inclusif occupe (ms). v4.16 (perf, corrections de relecture) : occupation seule, y compris pour "%d Other
		 *  Children" (le moteur y ajoute l'attente : retiree ici) */
		double Ms = 0.0;
		/** Attente (file graphique : racine, feuilles et "Other Children" seulement), 0 sinon */
		double WaitMs = 0.0;
		int NameStart = 0;
		int NameLen = 0;
		/** v4.16 (perf, relecture) : ligne "%d Other Children" (enfants caches sous le seuil) ; Depth est deja celle d'un
		 *  enfant de l'evenement precedent de meme retrait (son parent) */
		bool bOtherChildren = false;
	};

	/** "17 Other Children" (ou "1 Other Child", forme du tableau) */
	template <typename CharT>
	inline bool IsOtherChildrenName(const CharT* Name, int Len)
	{
		int P = 0;
		while (P < Len && IsDigit(Name[P]))
		{
			++P;
		}
		if (P == 0 || P >= Len || static_cast<int>(Name[P]) != ' ')
		{
			return false;
		}
		const int Rest = Len - (P + 1);
		const CharT* Tail = Name + P + 1;
		return (Rest == 14 && FindAscii(Tail, Rest, "Other Children") == 0) || (Rest == 11 && FindAscii(Tail, Rest, "Other Child") == 0);
	}

	/** "%s%4.1f%% %5.2fms%s   %s" (voir l'en-tete) ; faux pour toute autre ligne (en-tetes, lignes vides, journal) */
	template <typename CharT>
	inline bool ParseGpuLine(const CharT* S, int Len, FGpuLine& Out)
	{
		if (!S)
		{
			return false;
		}
		while (Len > 0 && IsBlank(S[Len - 1]))
		{
			--Len;
		}
		int P = 0;
		while (P < Len && static_cast<int>(S[P]) == ' ')
		{
			++P;
		}
		const int Lead = P;
		double Percent = 0.0;
		double Ms = 0.0;
		double Wait = 0.0;
		if (!ParseNumber(S, Len, P, Percent) || P >= Len || static_cast<int>(S[P]) != '%')
		{
			return false;
		}
		++P;
		while (P < Len && static_cast<int>(S[P]) == ' ')
		{
			++P;
		}
		if (!ParseNumber(S, Len, P, Ms) || P + 1 >= Len || static_cast<int>(S[P]) != 'm' || static_cast<int>(S[P + 1]) != 's')
		{
			return false;
		}
		P += 2;
		// " + 0.12 Wait" facultatif ; un nom qui commencerait par '+' reste un nom
		int Q = P;
		while (Q < Len && static_cast<int>(S[Q]) == ' ')
		{
			++Q;
		}
		if (Q < Len && static_cast<int>(S[Q]) == '+')
		{
			int R = Q + 1;
			while (R < Len && static_cast<int>(S[R]) == ' ')
			{
				++R;
			}
			double WaitValue = 0.0;
			if (ParseNumber(S, Len, R, WaitValue))
			{
				while (R < Len && static_cast<int>(S[R]) == ' ')
				{
					++R;
				}
				if (R + 3 < Len && static_cast<int>(S[R]) == 'W' && static_cast<int>(S[R + 1]) == 'a' && static_cast<int>(S[R + 2]) == 'i'
					&& static_cast<int>(S[R + 3]) == 't')
				{
					Wait = WaitValue;
					P = R + 4;
				}
			}
		}
		const int BeforeGap = P;
		while (P < Len && static_cast<int>(S[P]) == ' ')
		{
			++P;
		}
		if (P == BeforeGap || P >= Len)
		{
			return false;
		}
		// Le champ "%4.1f" ajoute au plus une espace devant le pourcentage ("_5.0") : Lead / 3 reste le niveau
		Out.Depth = Lead / 3;
		Out.Percent = Percent;
		Out.Ms = Ms;
		Out.WaitMs = Wait;
		Out.NameStart = P;
		Out.NameLen = Len - P;
		// v4.16 (perf, relecture) : "%d Other Children" est ecrit au retrait de son parent : enfant de ce parent
		Out.bOtherChildren = IsOtherChildrenName(S + P, Len - P);
		if (Out.bOtherChildren)
		{
			++Out.Depth;
			// v4.16 (perf, corrections de relecture) : sur la file graphique, le moteur ecrit pour cette ligne occupation +
			// attente des enfants caches (GPUProfiler.cpp : OtherChildrenInclusiveMs = Busy + Wait), alors que toutes les
			// autres lignes portent l'occupation seule. Ramene a l'occupation (attente gardee dans WaitMs) : sinon le parent
			// recevait l'attente dans ChildMs, son temps propre tombait a 0 et la somme des postes depassait la racine
			if (Wait > 0.0)
			{
				Out.Ms = Ms > Wait ? Ms - Wait : 0.0;
			}
		}
		return true;
	}

	enum class EQueueLine : uint8_t
	{
		None,
		Graphics,
		Other
	};

	/** En-tete d'une file : "GPU Profile for Frame N - Graphics pipeline 0 - GPU 0" ou "Compute pipeline 0 - GPU 0" */
	template <typename CharT>
	inline EQueueLine ParseQueueLine(const CharT* S, int Len)
	{
		const int At = FindAscii(S, Len, " pipeline ");
		if (At <= 0 || FindAscii(S, Len, " - GPU ") < 0)
		{
			return EQueueLine::None;
		}
		int Begin = At;
		while (Begin > 0 && static_cast<int>(S[Begin - 1]) != ' ')
		{
			--Begin;
		}
		const int WordLen = At - Begin;
		const char* const Graphics = "Graphics";
		bool bGraphics = WordLen == 8;
		for (int I = 0; bGraphics && I < WordLen; ++I)
		{
			bGraphics = static_cast<int>(S[Begin + I]) == Graphics[I];
		}
		return bGraphics ? EQueueLine::Graphics : EQueueLine::Other;
	}

	/** "    - Frame Time                    : 12.34ms" */
	template <typename CharT>
	inline bool ParseFrameTimeLine(const CharT* S, int Len, double& OutMs)
	{
		const int At = FindAscii(S, Len, "- Frame Time");
		if (At < 0)
		{
			return false;
		}
		int P = At;
		while (P < Len && static_cast<int>(S[P]) != ':')
		{
			++P;
		}
		if (P >= Len)
		{
			return false;
		}
		++P;
		while (P < Len && static_cast<int>(S[P]) == ' ')
		{
			++P;
		}
		double Value = 0.0;
		if (!ParseNumber(S, Len, P, Value) || P + 1 >= Len || static_cast<int>(S[P]) != 'm' || static_cast<int>(S[P + 1]) != 's')
		{
			return false;
		}
		OutMs = Value;
		return true;
	}

	// ------------------------------------------------------------------------------------------------ Postes

	enum ECategory : int
	{
		CatOther = 0,
		CatMegaLights,
		CatLumen,
		CatRtScene,
		CatShadows,
		CatFog,
		CatTranslucency,
		CatPost,
		CatBase,
		CatLighting,
		CatScreen,
		CatGpuScene,
		CatUi,
		NumCategories
	};

	/** Nom du poste (colonne "categorie" de PerfPasses.csv) */
	inline const char* CategoryName(int Cat)
	{
		static const char* const Names[NumCategories] = { "autre", "megalights", "lumen", "scene_rt", "ombres", "brouillard", "translucidite_eau", "post",
			"base", "eclairage", "effets_ecran", "scene_gpu", "interface" };
		return (Cat >= 0 && Cat < NumCategories) ? Names[Cat] : "autre";
	}

	/** Comparaison d'un motif : sous-chaine, debut du nom, nom entier (casse ignoree dans les trois cas) */
	enum EMatch : int
	{
		MatchAnywhere = 0,
		MatchPrefix,
		MatchWhole
	};

	struct FPattern
	{
		int Cat;
		const char* Text;
		int Mode;
	};

	/** Regles dans l'ordre (la premiere qui convient l'emporte) : MegaLights et Lumen avant la scene ray tracee (leurs passes
	 *  materielles parlent de "RayTracing"), l'eau des Poolrooms (SLW::) avant Lumen (SLW::LumenReflections), le brouillard
	 *  avant l'eau ("FogBehindWater"). v4.16 (perf, relecture) : "Lights" seulement en nom entier (sous-chaine de
	 *  "LightScattering" du brouillard volumetrique et de "LightShafts") ; noms releves dans les RDG_EVENT_SCOPE du moteur
	 *  5.8 (Engine/Source/Runtime/Renderer/Private) */
	inline const FPattern* Patterns(int& OutNum)
	{
		static const FPattern List[] = {
			{ CatMegaLights, "megalights", MatchAnywhere },
			{ CatTranslucency, "slw::", MatchPrefix },
			{ CatLumen, "lumen", MatchAnywhere },
			{ CatRtScene, "raytracingscene", MatchAnywhere },
			{ CatRtScene, "raytracingbuild", MatchAnywhere },
			{ CatRtScene, "buildraytracing", MatchAnywhere },
			{ CatRtScene, "raytracingdynamic", MatchAnywhere },
			{ CatRtScene, "raytracinginstance", MatchAnywhere },
			{ CatRtScene, "raytracingserialize", MatchAnywhere },
			{ CatRtScene, "setraytracingbindings", MatchAnywhere },
			{ CatRtScene, "naniteraytracing", MatchAnywhere },
			{ CatRtScene, "acceleration structure", MatchAnywhere },
			{ CatRtScene, "rayscene", MatchAnywhere },
			{ CatShadows, "shadow", MatchAnywhere },
			{ CatFog, "fog", MatchAnywhere },
			{ CatTranslucency, "translucen", MatchAnywhere },
			{ CatTranslucency, "water", MatchAnywhere },
			{ CatTranslucency, "distortion", MatchAnywhere },
			{ CatPost, "postprocess", MatchAnywhere },
			{ CatPost, "temporalsuperresolution", MatchAnywhere },
			{ CatPost, "tsr", MatchPrefix },
			{ CatPost, "tonemap", MatchAnywhere },
			{ CatPost, "bloom", MatchAnywhere },
			{ CatPost, "lightshaft", MatchAnywhere },
			{ CatPost, "motionblur", MatchAnywhere },
			{ CatPost, "depthoffield", MatchAnywhere },
			{ CatPost, "dof ", MatchPrefix },
			{ CatPost, "lensflare", MatchAnywhere },
			{ CatPost, "histogram", MatchAnywhere },
			{ CatPost, "eyeadaptation", MatchAnywhere },
			{ CatPost, "localexposure", MatchAnywhere },
			{ CatPost, "fxaa", MatchAnywhere },
			{ CatPost, "taa", MatchPrefix },
			{ CatBase, "basepass", MatchAnywhere },
			{ CatBase, "prepass", MatchAnywhere },
			{ CatBase, "depthpass", MatchAnywhere },
			{ CatBase, "nanite", MatchAnywhere },
			{ CatBase, "velocity", MatchAnywhere },
			{ CatBase, "gbuffer", MatchAnywhere },
			{ CatBase, "visbuffer", MatchAnywhere },
			{ CatLighting, "directlighting", MatchAnywhere },
			{ CatLighting, "deferredlighting", MatchAnywhere },
			{ CatLighting, "clusteredshading", MatchAnywhere },
			{ CatLighting, "lights", MatchWhole },
			{ CatLighting, "batchedlights", MatchAnywhere },
			{ CatLighting, "simplelights", MatchAnywhere },
			{ CatLighting, "lightgrid", MatchAnywhere },
			{ CatLighting, "light grid", MatchAnywhere },
			{ CatLighting, "reflectionenvironment", MatchAnywhere },
			{ CatLighting, "skylight", MatchAnywhere },
			{ CatScreen, "ambientocclusion", MatchAnywhere },
			{ CatScreen, "screenspacereflection", MatchAnywhere },
			{ CatScreen, "gtao", MatchAnywhere },
			{ CatScreen, "ssao", MatchAnywhere },
			{ CatScreen, "hzb", MatchAnywhere },
			{ CatScreen, "diffuseindirect", MatchAnywhere },
			{ CatGpuScene, "gpuscene", MatchAnywhere },
			{ CatGpuScene, "instancecull", MatchAnywhere },
			{ CatUi, "slate", MatchAnywhere },
			{ CatUi, "canvas", MatchAnywhere },
		};
		OutNum = static_cast<int>(sizeof(List) / sizeof(List[0]));
		return List;
	}

	/** Poste d'un evenement d'apres son nom (CatOther : aucun) */
	template <typename CharT>
	inline int CategoryOf(const CharT* Name, int Len)
	{
		int Num = 0;
		const FPattern* List = Patterns(Num);
		for (int I = 0; I < Num; ++I)
		{
			const int At = FindAscii(Name, Len, List[I].Text, true);
			if (At < 0)
			{
				continue;
			}
			int TextLen = 0;
			while (List[I].Text[TextLen] != '\0')
			{
				++TextLen;
			}
			const bool bOk = List[I].Mode == MatchAnywhere || (List[I].Mode == MatchPrefix && At == 0) || (List[I].Mode == MatchWhole && At == 0 && Len == TextLen);
			if (bOk)
			{
				return List[I].Cat;
			}
		}
		return CatOther;
	}

	/** v4.16 (perf, relecture) : poste "contenant" : ses descendants reconnus gardent leur propre poste (RenderDeferredLighting
	 *  et Lights contiennent Lumen, MegaLights et les ombres projetees ; DiffuseIndirectAndAO contient Lumen). Les autres
	 *  postes s'imposent a tous leurs descendants */
	inline bool IsContainerCategory(int Cat)
	{
		return Cat == CatOther || Cat == CatLighting || Cat == CatScreen;
	}

	struct FGpuNode
	{
		int Depth = 0;
		/** Temps inclusif (ms) */
		double Ms = 0.0;
		/** Poste de l'evenement d'apres son nom */
		int Cat = CatOther;
		/** Sorties d'Attribute : poste effectif, somme des temps inclusifs des enfants affiches */
		int Effective = CatOther;
		double ChildMs = 0.0;
	};

	constexpr int MaxTreeDepth = 64;

	/** Nodes : evenements d'une ou plusieurs files dans l'ordre du releve (prefixe : un parent avant ses enfants ; une racine
	 *  par file, a la profondeur 0 ; une profondeur qui saute un niveau est rattachee au dernier evenement ouvert).
	 *  Chaque evenement apporte son temps propre (inclusif - enfants, au moins 0 : arrondis du releve) a son poste effectif :
	 *  celui de son parent si le parent a un poste qui s'impose, sinon le sien s'il en a un, sinon celui du parent.
	 *  OutMs[NumCategories] : ms par poste ("autre" : temps propre des evenements sans poste, racine comprise) ; leur somme
	 *  vaut la racine aux arrondis pres. Rend le temps de la (des) racine(s) */
	inline double Attribute(FGpuNode* Nodes, int Num, double* OutMs)
	{
		for (int C = 0; C < NumCategories; ++C)
		{
			OutMs[C] = 0.0;
		}
		int Open[MaxTreeDepth];
		int NumOpen = 0;
		double Root = 0.0;
		for (int I = 0; Nodes && I < Num; ++I)
		{
			FGpuNode& Node = Nodes[I];
			Node.ChildMs = 0.0;
			int D = Node.Depth < 0 ? 0 : (Node.Depth >= MaxTreeDepth ? MaxTreeDepth - 1 : Node.Depth);
			D = D > NumOpen ? NumOpen : D;
			const int Own = (Node.Cat > CatOther && Node.Cat < NumCategories) ? Node.Cat : CatOther;
			if (D == 0)
			{
				Node.Effective = Own;
				Root += Node.Ms;
			}
			else
			{
				FGpuNode& Parent = Nodes[Open[D - 1]];
				Parent.ChildMs += Node.Ms;
				Node.Effective = !IsContainerCategory(Parent.Effective) ? Parent.Effective : (Own != CatOther ? Own : Parent.Effective);
			}
			Open[D] = I;
			NumOpen = D + 1;
		}
		for (int I = 0; Nodes && I < Num; ++I)
		{
			const double Self = Nodes[I].Ms - Nodes[I].ChildMs;
			OutMs[Nodes[I].Effective] += Self > 0.0 ? Self : 0.0;
		}
		return Root;
	}
}
