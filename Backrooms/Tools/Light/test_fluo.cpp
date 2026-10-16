// v4.15 (realisme) : banc hors moteur des neons fluorescents (BRFluoLogic.h, les regles du jeu).
//
// Il fait vivre des tubes qui clignotent (pas de 1 ms, plusieurs heures de tube) et des rallumages apres une coupure, avec
// un generateur fixe, et verifie ce que la v4.15 annonce : sequences d'amorcage de 2 a 4 clignements de 30 a 80 ms,
// extinctions bornees, ondulation de 3 a 5 % du tube allume, tube en fin de vie plus faible, un bruit a chaque extinction,
// rallumage decale (au plus 0,9 s) de 1 a 3 clignements ; reglage FLASHS : creux moins profonds et plus lents (attenues),
// eclat constant et aucune extinction (aucun). Le rendu (intensite, emissif, bourdonnement) se verifie en jeu.
//
//   clang-cl /nologo /std:c++17 /O2 /EHsc /utf-8 /W4 /WX -Wshadow -Wno-c++98-compat /I Source\Backrooms\Public Tools\Light\test_fluo.cpp
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Light/test_fluo.cpp -o test_fluo
#include "BRFluoLogic.h"

#include <cmath>
#include <cstdio>

namespace
{
	int GChecks = 0, GFailures = 0;
	void Line(const char* What, bool bOk)
	{
		++GChecks;
		GFailures += bOk ? 0 : 1;
		std::printf("  %-96s %s\n", What, bOk ? "OK" : "ECHEC");
	}

	/** Generateur fixe (meme suite a chaque execution, quel que soit le compilateur) */
	struct FLcg
	{
		unsigned State = 12345u;
		float operator()()
		{
			State = State * 1664525u + 1013904223u;
			return static_cast<float>(State >> 8) / 16777216.f;
		}
	};

	struct FTubeStats
	{
		int Sequences = 0;          // defaillances (amorcage direct ou extinction)
		int OffEvents = 0;          // extinctions signalees (bruit)
		int Blinks = 0;             // phases eteintes
		int MinBlinksPerSeq = 1000, MaxBlinksPerSeq = 0;
		float MinOff = 1e9f, MaxOff = 0.f;          // durees des phases eteintes courtes (amorcage)
		float MinOn = 1e9f, MaxOn = 0.f;            // durees des phases allumees courtes (amorcage)
		float MinSteady = 1e9f, MaxSteady = 0.f;    // durees allumees entre deux defaillances
		float MaxOut = 0.f;                         // plus longue phase eteinte
		float MinLit = 2.f, MaxLit = 0.f;           // eclat du tube allume (hors amorcage)
		float MinDark = 2.f, MaxDark = 0.f;         // eclat du tube eteint
		float MinPhase = 1e9f;                      // v4.15 : plus courte phase (allumee ou eteinte), hors debut de vie
	};

	FTubeStats Simulate(bool bDying, float FS, float Seconds)
	{
		FLcg Rand;
		BRFluo::FTube T;
		BRFluo::Init(T, bDying, Rand);
		FTubeStats S;
		const float Dt = 0.001f;
		bool bWasLit = true;
		float PhaseTime = 0.f;
		int SeqBlinks = 0;
		BRFluo::FTube::EState PrevState = T.State;
		for (float Time = 0.f; Time < Seconds; Time += Dt)
		{
			bool bOff = false;
			const float Level = BRFluo::Step(T, Dt, FS, Rand, bOff);
			S.OffEvents += bOff ? 1 : 0;
			if (PrevState == BRFluo::FTube::EState::Steady && T.State != BRFluo::FTube::EState::Steady)
			{
				++S.Sequences;
				SeqBlinks = 0;
			}
			if (T.State == BRFluo::FTube::EState::Steady && PrevState != BRFluo::FTube::EState::Steady && S.Sequences > 0)
			{
				S.MinBlinksPerSeq = SeqBlinks < S.MinBlinksPerSeq ? SeqBlinks : S.MinBlinksPerSeq;
				S.MaxBlinksPerSeq = SeqBlinks > S.MaxBlinksPerSeq ? SeqBlinks : S.MaxBlinksPerSeq;
			}
			if (T.bLit != bWasLit)
			{
				// Fin d'une phase : sa duree
				if (S.Sequences > 0 && !(bWasLit && S.Sequences == 1 && PrevState == BRFluo::FTube::EState::Steady))
				{
					S.MinPhase = PhaseTime < S.MinPhase ? PhaseTime : S.MinPhase;
				}
				const bool bStrikePhase = PrevState == BRFluo::FTube::EState::Strike || T.State == BRFluo::FTube::EState::Strike;
				if (bWasLit)
				{
					if (PrevState == BRFluo::FTube::EState::Steady)
					{
						// La premiere phase allumee vient du debut de vie (instant tire de 0,1 a 2 s) : pas comptee
						if (S.Sequences > 1)
						{
							S.MinSteady = PhaseTime < S.MinSteady ? PhaseTime : S.MinSteady;
							S.MaxSteady = PhaseTime > S.MaxSteady ? PhaseTime : S.MaxSteady;
						}
					}
					else if (bStrikePhase)
					{
						S.MinOn = PhaseTime < S.MinOn ? PhaseTime : S.MinOn;
						S.MaxOn = PhaseTime > S.MaxOn ? PhaseTime : S.MaxOn;
					}
				}
				else
				{
					++S.Blinks;
					++SeqBlinks;
					S.MaxOut = PhaseTime > S.MaxOut ? PhaseTime : S.MaxOut;
					if (PrevState == BRFluo::FTube::EState::Strike)
					{
						S.MinOff = PhaseTime < S.MinOff ? PhaseTime : S.MinOff;
						S.MaxOff = PhaseTime > S.MaxOff ? PhaseTime : S.MaxOff;
					}
				}
				PhaseTime = 0.f;
				bWasLit = T.bLit;
			}
			PhaseTime += Dt;
			if (T.bLit && T.State == BRFluo::FTube::EState::Steady)
			{
				S.MinLit = Level < S.MinLit ? Level : S.MinLit;
				S.MaxLit = Level > S.MaxLit ? Level : S.MaxLit;
			}
			else if (!T.bLit)
			{
				S.MinDark = Level < S.MinDark ? Level : S.MinDark;
				S.MaxDark = Level > S.MaxDark ? Level : S.MaxDark;
			}
			PrevState = T.State;
		}
		return S;
	}
}

int main()
{
	std::printf("# Banc des neons fluorescents v4.15 (BRFluoLogic.h)\n\n");
	char Buf[200];
	const float Slack = 0.0015f; // pas de simulation (1 ms) et arrondis
	const float Hours = 2.f * 3600.f;

	// ------------------------------------------------------------------ tube ordinaire, flashs normaux
	const FTubeStats N = Simulate(false, 1.f, Hours);
	std::printf("  tube ordinaire (2 h) : %d defaillances, %d phases eteintes, %d bruits ; amorcage %d a %d clignements, eteint %.0f-%.0f ms, allume %.0f-%.0f ms\n",
		N.Sequences, N.Blinks, N.OffEvents, N.MinBlinksPerSeq, N.MaxBlinksPerSeq, N.MinOff * 1000.f, N.MaxOff * 1000.f, N.MinOn * 1000.f, N.MaxOn * 1000.f);
	std::printf("  allume entre deux defaillances %.2f-%.2f s, plus longue extinction %.0f ms, eclat allume %.3f-%.3f, eteint %.3f-%.3f\n", N.MinSteady, N.MaxSteady,
		N.MaxOut * 1000.f, N.MinLit, N.MaxLit, N.MinDark, N.MaxDark);
	Line("des defaillances se produisent (au moins une par minute en moyenne)", N.Sequences > 120);
	std::snprintf(Buf, sizeof(Buf), "amorcage : %d a %d clignements (attendu 2 a 4, une extinction comptee comme le premier)", N.MinBlinksPerSeq, N.MaxBlinksPerSeq);
	Line(Buf, N.MinBlinksPerSeq >= BRFluo::StrikeBlinksMin && N.MaxBlinksPerSeq <= BRFluo::StrikeBlinksMax);
	std::snprintf(Buf, sizeof(Buf), "clignements d'amorcage de 30 a 80 ms (eteint %.0f-%.0f, allume %.0f-%.0f)", N.MinOff * 1000.f, N.MaxOff * 1000.f, N.MinOn * 1000.f,
		N.MaxOn * 1000.f);
	Line(Buf, N.MinOff >= BRFluo::BlinkMin - Slack && N.MaxOff <= BRFluo::BlinkMax + Slack && N.MinOn >= BRFluo::BlinkMin - Slack && N.MaxOn <= BRFluo::BlinkMax + Slack);
	Line("tube allume entre deux defaillances de 0,6 a 4,5 s", N.MinSteady >= BRFluo::SteadyMin - Slack && N.MaxSteady <= BRFluo::SteadyMax + BRFluo::BlinkMax + Slack);
	Line("extinction avant un nouvel amorcage : 600 ms au plus", N.MaxOut <= BRFluo::OutMax + Slack);
	const float Ripple = (N.MaxLit - N.MinLit) * 0.5f;
	std::snprintf(Buf, sizeof(Buf), "ondulation du tube allume de 3 a 5 %% (mesuree : +-%.1f %%)", Ripple * 100.f);
	Line(Buf, Ripple >= 0.03f && Ripple <= 0.05f);
	Line("un bruit a chaque extinction (autant de bruits que de phases eteintes)", N.OffEvents == N.Blinks);
	Line("tube eteint pendant un clignement : presque noir (3 %)", N.MaxDark <= 0.031f);

	// ------------------------------------------------------------------ tube en fin de vie
	const FTubeStats Dying = Simulate(true, 1.f, Hours);
	std::printf("  tube en fin de vie (2 h) : %d defaillances, eclat allume %.3f-%.3f\n", Dying.Sequences, Dying.MinLit, Dying.MaxLit);
	Line("fin de vie : plus faible (eclat allume <= 0,62) et defaillances au moins 1,5 fois plus frequentes", Dying.MaxLit <= 0.62f && Dying.Sequences * 2 > N.Sequences * 3);

	// ------------------------------------------------------------------ reglage FLASHS
	const FTubeStats Soft = Simulate(false, 0.35f, Hours);
	std::printf("  flashs attenues : %d defaillances, eteint %.0f-%.0f ms, eclat eteint %.3f, ondulation +-%.1f %%\n", Soft.Sequences, Soft.MinOff * 1000.f,
		Soft.MaxOff * 1000.f, Soft.MaxDark, (Soft.MaxLit - Soft.MinLit) * 50.f);
	Line("flashs attenues : creux moins profond (eclat eteint au-dessus de 0,5)", Soft.MinDark > 0.5f);
	Line("flashs attenues : clignements plus lents (au moins 2,5 fois)", Soft.MinOff >= BRFluo::BlinkMin * 2.5f);
	// v4.15 : 3 clignements par seconde au plus (chaque demi-clignement dure au moins 0,17 s)
	std::snprintf(Buf, sizeof(Buf), "flashs attenues : demi-clignements de %.0f ms au moins (eteint %.0f-%.0f, allume %.0f-%.0f) : 3 par seconde au plus",
		Soft.MinPhase * 1000.f, Soft.MinOff * 1000.f, Soft.MaxOff * 1000.f, Soft.MinOn * 1000.f, Soft.MaxOn * 1000.f);
	Line(Buf, Soft.MinPhase >= BRFluo::SoftHalfBlinkMin - Slack && Soft.MinPhase * 2.f >= 1.f / 3.f - Slack);
	{
		const FTubeStats SoftDying = Simulate(true, 0.35f, Hours);
		std::snprintf(Buf, sizeof(Buf), "flashs attenues, tube en fin de vie : demi-clignements de %.0f ms au moins", SoftDying.MinPhase * 1000.f);
		Line(Buf, SoftDying.MinPhase >= BRFluo::SoftHalfBlinkMin - Slack);
	}
	const FTubeStats None = Simulate(false, 0.f, 600.f);
	std::snprintf(Buf, sizeof(Buf), "aucun flash : eclat constant (%.3f-%.3f), aucune extinction ni bruit (%d)", None.MinLit, None.MaxLit, None.OffEvents);
	Line(Buf, None.OffEvents == 0 && None.Blinks == 0 && std::fabs(None.MaxLit - None.MinLit) < 1e-6f);

	// ------------------------------------------------------------------ rallumage apres une coupure
	{
		FLcg Rand;
		int MinBlinks = 1000, MaxBlinks = 0, Unfinished = 0;
		float MaxDelay = 0.f, MaxTotal = 0.f;
		const int Lamps = 2000;
		for (int L = 0; L < Lamps; ++L)
		{
			BRFluo::FRestrike R;
			BRFluo::BeginRestrike(R, 1.f, Rand);
			float Time = 0.f;
			float FirstOn = -1.f;
			int Ons = 0;
			while (R.bActive && Time < 5.f)
			{
				bool bOn = false;
				const float Mul = BRFluo::StepRestrike(R, 0.001f, 1.f, Rand, bOn);
				Ons += bOn ? 1 : 0;
				if (FirstOn < 0.f && Mul > 0.5f)
				{
					FirstOn = Time;
				}
				Time += 0.001f;
			}
			Unfinished += R.bActive ? 1 : 0;
			MaxDelay = FirstOn > MaxDelay ? FirstOn : MaxDelay;
			MaxTotal = Time > MaxTotal ? Time : MaxTotal;
			MinBlinks = Ons < MinBlinks ? Ons : MinBlinks;
			MaxBlinks = Ons > MaxBlinks ? Ons : MaxBlinks;
		}
		std::printf("  rallumage (%d lampes) : premier allumage au plus a %.0f ms, %d a %d allumages, termine en %.2f s au plus\n", Lamps, MaxDelay * 1000.f, MinBlinks,
			MaxBlinks, MaxTotal);
		Line("chaque lampe se rallume (rallumage termine)", Unfinished == 0);
		Line("retard propre a chaque lampe : 0,9 s au plus", MaxDelay <= BRFluo::RestrikeDelayMax + Slack);
		Line("1 a 3 clignements avant de rester allumee", MinBlinks >= BRFluo::RestrikeBlinksMin && MaxBlinks <= BRFluo::RestrikeBlinksMax);
		Line("rallumage complet en moins de 1,6 s (la phase de retour du courant dure 1,4 s)", MaxTotal < 1.6f);
		// v4.15 : flashs attenues : retard propre a chaque lampe, puis allumee, sans clignement
		int SoftMaxOns = 0, SoftUnfinished = 0;
		float SoftMaxTotal = 0.f;
		bool bSoftLevels = true;
		for (int L = 0; L < Lamps; ++L)
		{
			BRFluo::FRestrike R;
			BRFluo::BeginRestrike(R, 0.35f, Rand);
			float Time = 0.f;
			int Ons = 0;
			while (R.bActive && Time < 5.f)
			{
				bool bOn = false;
				const float Mul = BRFluo::StepRestrike(R, 0.001f, 0.35f, Rand, bOn);
				Ons += bOn ? 1 : 0;
				bSoftLevels = bSoftLevels && (std::fabs(Mul - 1.f) < 1e-6f || std::fabs(Mul - 0.65f) < 1e-6f);
				Time += 0.001f;
			}
			SoftUnfinished += R.bActive ? 1 : 0;
			SoftMaxOns = Ons > SoftMaxOns ? Ons : SoftMaxOns;
			SoftMaxTotal = Time > SoftMaxTotal ? Time : SoftMaxTotal;
		}
		std::snprintf(Buf, sizeof(Buf), "flashs attenues : rallumage sans clignement (%d allumage(s) au plus, termine en %.2f s au plus)", SoftMaxOns, SoftMaxTotal);
		Line(Buf, SoftUnfinished == 0 && SoftMaxOns <= 1 && bSoftLevels && SoftMaxTotal < 1.6f);
		BRFluo::FRestrike Off;
		BRFluo::BeginRestrike(Off, 0.f, Rand);
		bool bOn = false;
		Line("aucun flash : rallumage immediat, sans clignement", !Off.bActive && BRFluo::StepRestrike(Off, 0.016f, 0.f, Rand, bOn) == 1.f);
	}

	std::printf("\nRESULTAT : %s (%d verifications, %d echec(s))\n", GFailures == 0 ? "OK" : "ECHEC", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
