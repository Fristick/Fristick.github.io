// v4.15 (realisme) : comportement d'un tube fluorescent defaillant et rallumage apres une coupure de courant.
// Cosmetique et local a chaque machine (tirages FMath::FRand dans le jeu) : rien ici ne decide de la lumiere de gameplay
// (ABRWorld::LightLevelAt) ni de la disposition des lampes. C++ pur : le banc Tools/Light/test_fluo.cpp verifie les
// durees, le nombre de clignements et le reglage FLASHS sans le moteur.
//
// Un tube qui clignote reste allume la plupart du temps, avec une legere ondulation (3 a 5 %) ; de temps en temps il
// rate : soit une sequence d'amorcage de 2 a 4 clignements de 30 a 80 ms, soit une extinction de 80 a 600 ms suivie de la
// meme sequence. Un tube en fin de vie est plus faible, pulse lentement et rate plus souvent. Apres une coupure, chaque
// lampe se rallume avec son propre retard (au plus 0,9 s) et 1 a 3 clignements : pas toutes dans la meme image.
// Reglage FLASHS (FS : 1 normaux, 0,35 attenues, 0 aucun) : clignements moins profonds et plus lents, aucun a 0.
// v4.15 : flashs attenues : chaque demi-clignement dure au moins 0,17 s (3 clignements par seconde au plus, la limite
// usuelle pour les personnes sensibles aux flashs) et le rallumage apres une coupure se fait sans clignement (retard
// propre a chaque lampe, puis allumee).
// Reporte a la v4.16 : l'extremite sombre d'un tube en fin de vie (degrade emissif le long du tube) : il faut un parametre
// dans M_BR_Mesh, partage par tous les modeles (squelettes compris), donc une reconstruction de tous les materiaux ; ici,
// un tube en fin de vie est seulement plus faible et pulse lentement.
#pragma once

#include <cmath>

namespace BRFluo
{
	/** Durees (s) d'un clignement de la sequence d'amorcage (allume ou eteint) et nombre de clignements */
	constexpr float BlinkMin = 0.03f;
	constexpr float BlinkMax = 0.08f;
	constexpr int StrikeBlinksMin = 2;
	constexpr int StrikeBlinksMax = 4;
	/** Extinction avant un nouvel amorcage */
	constexpr float OutMin = 0.08f;
	constexpr float OutMax = 0.6f;
	/** Duree allumee entre deux defaillances (tube ordinaire, tube en fin de vie) */
	constexpr float SteadyMin = 0.6f;
	constexpr float SteadyMax = 4.5f;
	constexpr float DyingSteadyMin = 0.25f;
	constexpr float DyingSteadyMax = 1.6f;
	/** Ondulation relative du tube allume (a FS = 1) et eclat d'un tube en fin de vie */
	constexpr float Wobble = 0.04f;
	constexpr float DyingLevel = 0.55f;
	/** Part des tubes qui clignotent en fin de vie */
	constexpr float DyingShare = 0.25f;
	/** Rallumage apres une coupure : retard maximal, clignements, durees d'un clignement */
	constexpr float RestrikeDelayMax = 0.9f;
	constexpr int RestrikeBlinksMin = 1;
	constexpr int RestrikeBlinksMax = 3;
	constexpr float RestrikeOnMin = 0.04f;
	constexpr float RestrikeOnMax = 0.09f;
	constexpr float RestrikeOffMin = 0.03f;
	constexpr float RestrikeOffMax = 0.07f;

	/** Eclat d'un tube "eteint" pendant un clignement, selon le reglage FLASHS (comme avant la v4.15) */
	inline float OffLevel(float FS)
	{
		return 0.85f + (0.03f - 0.85f) * FS;
	}

	/** Ralentissement des clignements quand les flashs sont attenues */
	inline float Slow(float FS)
	{
		return 1.f / (FS > 0.25f ? FS : 0.25f);
	}

	/** v4.15 : flashs attenues (FS < 1) : duree minimale d'un demi-clignement (allume ou eteint) : 3 clignements par seconde
	 *  au plus */
	constexpr float SoftHalfBlinkMin = 0.17f;

	template <typename RandT>
	inline float Range(RandT& Rand, float A, float B)
	{
		return A + (B - A) * Rand();
	}

	/** v4.15 : duree d'un demi-clignement tire dans [A, B], ralentie selon FLASHS ; au moins SoftHalfBlinkMin si attenues */
	template <typename RandT>
	inline float Blink(RandT& Rand, float A, float B, float FS)
	{
		const float Time = Range(Rand, A, B) * Slow(FS);
		return (FS < 1.f && Time < SoftHalfBlinkMin) ? SoftHalfBlinkMin : Time;
	}

	template <typename RandT>
	inline int RangeInt(RandT& Rand, int A, int B)
	{
		const int N = A + static_cast<int>(Rand() * static_cast<float>(B - A + 1));
		return N > B ? B : N;
	}

	struct FTube
	{
		enum class EState : unsigned char { Steady, Strike, Out };
		EState State = EState::Steady;
		/** Reste de la phase courante (s) */
		float Timer = 0.f;
		/** Changements d'etat restants de la sequence d'amorcage (deux par clignement : eteint puis allume) */
		int StrikeSteps = 0;
		bool bLit = true;
		/** Tube en fin de vie : plus faible, pulse lentement, rate plus souvent */
		bool bDying = false;
		/** Temps ecoule (ondulation, pulsation) */
		float Clock = 0.f;
	};

	/** Debut de vie d'un tube qui clignote : premiere defaillance a un instant different pour chaque tube */
	template <typename RandT>
	inline void Init(FTube& T, bool bDying, RandT& Rand)
	{
		T = FTube();
		T.bDying = bDying;
		T.Timer = Range(Rand, 0.1f, 2.f);
		T.Clock = Range(Rand, 0.f, 10.f);
	}

	/** Avance le tube de Dt (s). Retourne son eclat relatif (0..1). bOffEvent : le tube vient de s'eteindre (bruit
	 *  S_Flicker). FS : reglage FLASHS ; a 0, eclat constant et aucune extinction */
	template <typename RandT>
	inline float Step(FTube& T, float Dt, float FS, RandT& Rand, bool& bOffEvent)
	{
		bOffEvent = false;
		T.Clock += Dt;
		const float Base = T.bDying ? DyingLevel : 1.f;
		if (FS <= 0.f)
		{
			T.State = FTube::EState::Steady;
			T.bLit = true;
			return Base * 0.92f;
		}
		T.Timer -= Dt;
		int Guard = 0;
		while (T.Timer <= 0.f && Guard++ < 16)
		{
			switch (T.State)
			{
			case FTube::EState::Steady:
				// Defaillance : amorcage direct, ou extinction puis amorcage (plus souvent pour un tube en fin de vie)
				T.bLit = false;
				bOffEvent = true;
				if (Rand() < (T.bDying ? 0.55f : 0.4f))
				{
					T.State = FTube::EState::Out;
					T.Timer += Blink(Rand, OutMin, T.bDying ? OutMax * 2.f : OutMax, FS);
				}
				else
				{
					// Eteint, puis (2 x clignements - 1) changements jusqu'au dernier allumage
					T.State = FTube::EState::Strike;
					T.StrikeSteps = 2 * RangeInt(Rand, StrikeBlinksMin, StrikeBlinksMax) - 1;
					T.Timer += Blink(Rand, BlinkMin, BlinkMax, FS);
				}
				break;
			case FTube::EState::Out:
				// Nouvel amorcage : le tube se rallume en clignotant
				T.State = FTube::EState::Strike;
				T.StrikeSteps = 2 * RangeInt(Rand, StrikeBlinksMin, StrikeBlinksMax) - 2;
				T.bLit = true;
				T.Timer += Blink(Rand, BlinkMin, BlinkMax, FS);
				break;
			default: // Strike
				if (T.StrikeSteps <= 0)
				{
					T.State = FTube::EState::Steady;
					T.bLit = true;
					T.Timer += T.bDying ? Range(Rand, DyingSteadyMin, DyingSteadyMax) : Range(Rand, SteadyMin, SteadyMax);
					break;
				}
				--T.StrikeSteps;
				T.bLit = !T.bLit;
				bOffEvent = bOffEvent || !T.bLit;
				T.Timer += Blink(Rand, BlinkMin, BlinkMax, FS);
				break;
			}
		}
		if (!T.bLit)
		{
			return Base * OffLevel(FS);
		}
		// Ondulation de 3 a 5 % (deux frequences : pas de battement regulier) ; tube en fin de vie : pulsation lente
		const float Ripple = Wobble * FS * (0.6f * std::sin(T.Clock * 46.f) + 0.4f * std::sin(T.Clock * 83.f));
		const float Pulse = T.bDying ? 0.88f + 0.12f * std::sin(T.Clock * 4.4f) : 1.f;
		return Base * Pulse * (1.f + Ripple);
	}

	/** Rallumage d'une lampe apres une coupure */
	struct FRestrike
	{
		bool bActive = false;
		/** Retard restant avant le premier allumage (s) */
		float Delay = 0.f;
		/** Changements d'etat restants (allume, eteint, ... allume) */
		int Steps = 0;
		float Timer = 0.f;
		bool bLit = false;
	};

	/** Le courant revient : retard et clignements propres a cette lampe (FS = 0 : rallumage immediat, sans clignement ;
	 *  v4.15 : flashs attenues : retard propre, puis allumee, sans clignement) */
	template <typename RandT>
	inline void BeginRestrike(FRestrike& R, float FS, RandT& Rand)
	{
		R = FRestrike();
		if (FS <= 0.f)
		{
			return;
		}
		R.bActive = true;
		R.Delay = Range(Rand, 0.f, RestrikeDelayMax);
		// v4.15 : flashs attenues : aucun clignement (retard, puis allumee) ; normaux : 1 a 3 clignements
		R.Steps = FS >= 1.f ? 2 * RangeInt(Rand, RestrikeBlinksMin, RestrikeBlinksMax) - 1 : 0;
		R.bLit = false;
		R.Timer = 0.f;
	}

	/** Facteur d'eclat de la lampe pendant son rallumage (1 - FS eteinte, 1 allumee et une fois termine). bOnEvent : elle
	 *  vient de s'allumer */
	template <typename RandT>
	inline float StepRestrike(FRestrike& R, float Dt, float FS, RandT& Rand, bool& bOnEvent)
	{
		bOnEvent = false;
		if (!R.bActive)
		{
			return 1.f;
		}
		if (R.Delay > 0.f)
		{
			R.Delay -= Dt;
			if (R.Delay > 0.f)
			{
				return 1.f - FS;
			}
			R.bLit = true;
			bOnEvent = true;
			R.Timer = Blink(Rand, RestrikeOnMin, RestrikeOnMax, FS);
			return 1.f;
		}
		R.Timer -= Dt;
		int Guard = 0;
		while (R.Timer <= 0.f && Guard++ < 16)
		{
			if (R.Steps <= 0)
			{
				R.bActive = false;
				R.bLit = true;
				return 1.f;
			}
			--R.Steps;
			R.bLit = !R.bLit;
			bOnEvent = bOnEvent || R.bLit;
			R.Timer += R.bLit ? Blink(Rand, RestrikeOnMin, RestrikeOnMax, FS) : Blink(Rand, RestrikeOffMin, RestrikeOffMax, FS);
		}
		// Eteinte : sans courant (FS = 1) ; flashs attenues : creux moins profond
		return R.bLit ? 1.f : 1.f - FS;
	}
}
