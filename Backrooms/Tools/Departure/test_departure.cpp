// v4.12 : banc hors moteur du depart de groupe par une sortie (constat 4.1).
//
// Il compile BRGatherLogic.cpp, le code exact du jeu (ABRWorld::ServerStartDeparture, ::UpdateDeparture,
// ABRCharacter::UpdateClimb) et verifie, avec les hauteurs reelles des niveaux (BRLevels.cpp, BRChunk.cpp) :
//   - le grimpeur au sommet est compte sur toutes les echelles, avec ou sans conduit (avant : jamais avec un conduit) ;
//   - deux puis quatre joueurs : un au sommet, les autres au pied ; joueurs a differentes hauteurs sur l'echelle ;
//   - un joueur a l'etage au-dessus (derriere un plafond), ou parti trop loin, n'est jamais compte ;
//   - l'attente au sommet : une demande par arrivee, aucune demande repetee a chaque image, pas de montee au-dela du
//     sommet, annulation par l'initiateur qui redescend, refus, expiration, rearmement apres une descente d'un metre.
// Les lignes de vue (murs) sont du ressort du moteur : ici, elles sont supposees libres au meme etage.
//
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Departure/test_departure.cpp
//       Source/Backrooms/Private/BRGatherLogic.cpp -o test_departure && ./test_departure
#include "BRGatherLogic.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace BRGather;

namespace
{
	int GChecks = 0;
	int GFailures = 0;

	void Check(bool bOk, const char* What)
	{
		++GChecks;
		if (!bOk)
		{
			++GFailures;
			std::printf("  ECHEC : %s\n", What);
		}
	}

	void Line(const char* What, bool bOk)
	{
		std::printf("  %-78s %s\n", What, bOk ? "OK" : "ECHEC");
		Check(bOk, What);
	}

	// Echelle posee contre un mur a l'origine, la piece vers +X (ABRExit::InitLadder, ::GetClimbAnchor)
	FExitShape Ladder(float Ceiling, float Shaft)
	{
		FExitShape S;
		S.Style = EStyle::Ladder;
		S.Foot = { 0.f, 0.f, 0.f };
		S.Forward = { 1.f, 0.f, 0.f };
		S.Anchor = { 52.f, 0.f, 0.f };
		S.TopZ = Shaft > 0.f ? Ceiling + Shaft * 0.5f : Ceiling - CapsuleHalf - 6.f;
		return S;
	}

	FVec At(float X, float Y, float Z) { return { X, Y, Z }; }

	bool Counted(const FExitShape& S, const FVec& P)
	{
		return Classify(S, P) != EGather::No; // meme etage : ligne de vue supposee libre
	}

	// Critere de la base v4.11 (BRMissionWorld.cpp:1210, 1287) pour comparaison
	bool BaseCounted(const FExitShape& S, const FVec& P)
	{
		const FVec Loc = { S.Foot.X, S.Foot.Y, S.Foot.Z + 150.f };
		const float D2 = (P.X - Loc.X) * (P.X - Loc.X) + (P.Y - Loc.Y) * (P.Y - Loc.Y);
		return D2 < 800.f * 800.f && std::fabs(P.Z - Loc.Z) < 260.f + (Loc.Z > P.Z + 100.f ? 250.f : 0.f);
	}

	void TestGather()
	{
		std::printf("1. Rassemblement autour de l'echelle choisie\n");
		struct FL { int Level; float Ceiling; float Shaft; };
		const FL Ls[] = { { 0, 290.f, 300.f }, { 2, 280.f, 300.f }, { 6, 290.f, 300.f }, { 8, 450.f, 300.f }, { 37, 450.f, 300.f },
			{ 6, 290.f, 0.f }, { 8, 450.f, 0.f }, { 37, 450.f, 0.f } };
		for (const FL& L : Ls)
		{
			const FExitShape S = Ladder(L.Ceiling, L.Shaft);
			const FVec Top = At(52.f, 0.f, S.TopZ);
			const FVec FootMate = At(250.f, 120.f, CapsuleHalf);
			const bool bNew = Counted(S, Top) && Counted(S, FootMate) && CanRequest(S, Top) == EGather::Ladder;
			char Buf[128];
			std::snprintf(Buf, sizeof(Buf), "Niveau %2d, plafond %3.0f, conduit %3.0f : grimpeur a %3.0f cm compte (base : %s)", L.Level, L.Ceiling, L.Shaft, S.TopZ,
				BaseCounted(S, Top) ? "compte" : "non compte");
			Line(Buf, bNew);
		}
		// Niveau 0 -> 37 : deux puis quatre joueurs
		const FExitShape S0 = Ladder(290.f, 300.f);
		{
			const std::vector<FVec> Two = { At(52.f, 0.f, S0.TopZ), At(180.f, -60.f, CapsuleHalf) };
			const std::vector<FVec> Four = { At(52.f, 0.f, S0.TopZ), At(180.f, -60.f, CapsuleHalf), At(420.f, 300.f, CapsuleHalf), At(52.f, 0.f, 180.f) };
			int A = 0, B = 0;
			for (const FVec& P : Two) A += Counted(S0, P) ? 1 : 0;
			for (const FVec& P : Four) B += Counted(S0, P) ? 1 : 0;
			Line("Niveau 0 -> 37, 2 joueurs : un au sommet, un au pied : 2/2", A == 2);
			Line("Niveau 0 -> 37, 4 joueurs : sommet, pied, 5 m, a mi-hauteur : 4/4", B == 4);
		}
		// Hauteurs le long de l'echelle (tous les 10 cm)
		{
			bool bAll = true;
			for (float Z = CapsuleHalf; Z <= S0.TopZ; Z += 10.f)
			{
				bAll &= Counted(S0, At(52.f, 0.f, Z));
			}
			Line("grimpeur compte a toute hauteur de la montee (88 a 440 cm)", bAll);
		}
		// Un etage au-dessus, derriere le plafond, hors de l'emprise du conduit (124 x 114 cm contre le mur, ferme sur ses
		// quatre cotes : un autre etage n'y donne pas acces) : a l'aplomb du point de rassemblement et a 4 m
		{
			const bool bAbove = Counted(S0, At(200.f, 90.f, 360.f + CapsuleHalf)) || Counted(S0, At(400.f, 200.f, 300.f + CapsuleHalf));
			Line("joueur a l'etage au-dessus (derriere le plafond) : non compte", !bAbove);
			const bool bBelow = Counted(S0, At(100.f, 0.f, -330.f + CapsuleHalf));
			Line("joueur a l'etage en dessous : non compte", !bBelow);
		}
		// Retour en arriere : a 9 m du point de rassemblement
		Line("joueur reparti a 9 m : non compte", !Counted(S0, At(100.f + 900.f, 0.f, CapsuleHalf)));
		// Hors de la colonne mais a la hauteur du sommet (a cote du conduit, dans le mur) : non compte
		Line("joueur a cote du conduit, a la hauteur du sommet : non compte", !Counted(S0, At(52.f, 150.f, S0.TopZ)));
		// Demande : depuis l'echelle sans test de vue ; depuis le sol, a portee de l'echelle avec test de vue
		Line("demande depuis le sommet : acceptee sans ligne de vue (conduit ferme)", CanRequest(S0, At(52.f, 0.f, S0.TopZ)) == EGather::Ladder);
		Line("demande depuis un autre etage : refusee", CanRequest(S0, At(60.f, 0.f, 400.f)) != EGather::FloorNeedsView || OnLadder(S0, At(60.f, 0.f, 400.f)));
		// Points de vue alignes : demande et rassemblement visent le meme point
		const FVec V = ViewPoint(S0);
		Line("meme point de vue pour la demande et le rassemblement (1 m devant, 90 cm)", std::fabs(V.X - 100.f) < 0.01f && std::fabs(V.Z - ViewHeight) < 0.01f);
		// Porte et grange : inchanges dans l'esprit
		FExitShape Door;
		Door.Foot = { 0.f, 0.f, 0.f };
		Line("porte : demande a 3 m, refus a 4 m", CanRequest(Door, At(300.f, 0.f, CapsuleHalf)) == EGather::FloorNeedsView && CanRequest(Door, At(400.f, 0.f, CapsuleHalf)) == EGather::No);
		FExitShape Barn;
		Barn.Style = EStyle::Barn;
		Barn.Foot = { 0.f, 0.f, 0.f };
		Line("grange : rassemblement devant l'entree (5,2 m)", Counted(Barn, At(520.f + 600.f, 0.f, CapsuleHalf)) && !Counted(Barn, At(-400.f, 0.f, CapsuleHalf)));
	}

	// Simulation de l'attente au sommet (ABRCharacter::UpdateClimb) avec un hote simple
	struct FHostDep
	{
		uint8_t Phase = 0;
		uint16_t Id = 0;
		int Target = 37;
		int Requests = 0;
		int Cancels = 0;
		bool bRefuse = false;
		float Delay = 0.f;
		float Pending = -1.f;
		void Request()
		{
			++Requests;
			if (bRefuse)
			{
				return;
			}
			if (Phase != 1)
			{
				Pending = Delay; // reponse apres la latence
			}
		}
		void Tick(float Dt)
		{
			if (Pending >= 0.f)
			{
				Pending -= Dt;
				if (Pending < 0.f)
				{
					Phase = 1;
					++Id;
				}
			}
		}
	};

	void TestClimbWait()
	{
		std::printf("2. Attente au sommet de l'echelle\n");
		const FExitShape S = Ladder(290.f, 300.f);
		const float Dt = 1.f / 60.f;
		// Montee puis 10 s au sommet en tenant "avancer" : une seule demande, jamais au-dela du sommet
		{
			FClimbWait W;
			FHostDep H;
			H.Delay = 0.1f;
			float Z = CapsuleHalf;
			float MaxZ = 0.f;
			for (int F = 0; F < 60 * 14; ++F)
			{
				float Up = 1.f;
				const FClimbWait::FOut O = W.Update(Dt, Z, S.TopZ, Up, H.Phase, H.Id, true, true);
				if (O.bClampTop && Up > 0.f) Up = 0.f;
				float DZ = Up * 140.f * Dt;
				if (Z + DZ > S.TopZ) DZ = S.TopZ - Z > 0.f ? S.TopZ - Z : 0.f;
				Z += DZ;
				MaxZ = Z > MaxZ ? Z : MaxZ;
				if (O.bSendRequest) H.Request();
				H.Tick(Dt);
			}
			Line("10 s au sommet en tenant la touche : 1 demande, attente stable, pas plus haut", H.Requests == 1 && W.State == FClimbWait::EState::Waiting && MaxZ <= S.TopZ + 0.01f);
			// L'initiateur redescend : une annulation, puis l'attente s'arrete et la descente rearme
			int Cancels = 0;
			for (int F = 0; F < 60 * 2; ++F)
			{
				const float Up = -1.f;
				const FClimbWait::FOut O = W.Update(Dt, Z, S.TopZ, Up, H.Phase, H.Id, true, true);
				if (O.bSendCancel) { ++Cancels; H.Phase = 3; }
				Z += Up * 140.f * Dt;
			}
			Line("l'initiateur redescend : une annulation, demande rearmee apres 1 m", Cancels == 1 && W.State == FClimbWait::EState::Climbing);
			// Il remonte : une nouvelle demande
			H.Phase = 0;
			for (int F = 0; F < 60 * 4; ++F)
			{
				float Up = 1.f;
				const FClimbWait::FOut O = W.Update(Dt, Z, S.TopZ, Up, H.Phase, H.Id, true, true);
				if (O.bClampTop) Up = 0.f;
				float DZ = Up * 140.f * Dt;
				if (Z + DZ > S.TopZ) DZ = S.TopZ - Z > 0.f ? S.TopZ - Z : 0.f;
				Z += DZ;
				if (O.bSendRequest) H.Request();
				H.Tick(Dt);
			}
			Line("remonte au sommet : une nouvelle demande (2 en tout)", H.Requests == 2);
		}
		// Coequipier (pas l'initiateur) : redescendre n'annule rien
		{
			FClimbWait W;
			W.Update(Dt, S.TopZ, S.TopZ, 1.f, 1, 7, true, false);
			bool bCancel = false;
			for (int F = 0; F < 30; ++F)
			{
				bCancel |= W.Update(Dt, S.TopZ - F * 2.f, S.TopZ, -1.f, 1, 7, true, false).bSendCancel;
			}
			Line("un coequipier redescend : aucune annulation", !bCancel);
		}
		// Refus de l'hote (sortie verrouillee, autre depart) : on reste au sommet sans redemander
		{
			FClimbWait W;
			int Requests = 0;
			for (int F = 0; F < 600; ++F)
			{
				const FClimbWait::FOut O = W.Update(Dt, S.TopZ, S.TopZ, 1.f, 0, 0, true, true);
				if (O.bSendRequest) { ++Requests; W.OnRefused(); }
			}
			Line("refus de l'hote : une demande, puis arret au sommet (aucune demande par image)", Requests == 1 && W.State == FClimbWait::EState::Stopped);
		}
		// Aucune reponse (hote ancien, refus perdu) : arret apres 3 s
		{
			FClimbWait W;
			int Requests = 0;
			for (int F = 0; F < 60 * 5; ++F)
			{
				Requests += W.Update(Dt, S.TopZ, S.TopZ, 0.f, 0, 0, true, true).bSendRequest ? 1 : 0;
			}
			Line("aucune reponse : arret apres 3 s, 1 demande", Requests == 1 && W.State == FClimbWait::EState::Stopped);
		}
		// Un depart vers une autre destination est ignore
		{
			FClimbWait W;
			W.Update(Dt, S.TopZ, S.TopZ, 0.f, 1, 9, false, false);
			for (int F = 0; F < 60 * 4; ++F)
			{
				W.Update(Dt, S.TopZ, S.TopZ, 0.f, 1, 9, false, false);
			}
			Line("depart vers une autre destination : ignore (arret apres 3 s)", W.State == FClimbWait::EState::Stopped);
		}
		// Le groupe part : etat "Leaving", plus de blocage au sommet (on continue de monter pendant le noclip)
		{
			FClimbWait W;
			W.Update(Dt, S.TopZ, S.TopZ, 1.f, 0, 0, true, true);
			W.Update(Dt, S.TopZ, S.TopZ, 1.f, 1, 3, true, true);
			const FClimbWait::FOut O = W.Update(Dt, S.TopZ, S.TopZ, 1.f, 2, 3, true, true);
			Line("depart du groupe : etat Leaving, sommet plus bloque", W.State == FClimbWait::EState::Leaving && !O.bClampTop);
		}
	}

	// Depart complet : hote qui compte (ABRWorld::UpdateDeparture), grimpeur au sommet, coequipiers qui arrivent
	void TestGroup()
	{
		std::printf("3. Depart du groupe (hote)\n");
		const FExitShape S = Ladder(290.f, 300.f);
		for (int N : { 2, 4 })
		{
			std::vector<FVec> P(N);
			P[0] = At(52.f, 0.f, S.TopZ);
			for (int I = 1; I < N; ++I) P[I] = At(2500.f + I * 100.f, 300.f * I, CapsuleHalf);
			const float Dt = 1.f / 30.f;
			float ReadyTime = 0.f;
			float T = 0.f;
			bool bLeft = false;
			for (; T < 60.f && !bLeft; T += Dt)
			{
				for (int I = 1; I < N; ++I)
				{
					// Marche vers le pied de l'echelle a 2,6 m/s
					const float DX = 150.f - P[I].X, DY = 0.f - P[I].Y;
					const float D = std::sqrt(DX * DX + DY * DY);
					if (D > 5.f)
					{
						P[I].X += DX / D * 260.f * Dt;
						P[I].Y += DY / D * 260.f * Dt;
					}
				}
				int Ready = 0;
				for (const FVec& Q : P) Ready += Counted(S, Q) ? 1 : 0;
				ReadyTime = Ready == N ? ReadyTime + Dt : 0.f;
				bLeft = ReadyTime >= 1.2f;
			}
			char Buf[96];
			std::snprintf(Buf, sizeof(Buf), "%d joueurs : le groupe part ensemble (%.1f s), le grimpeur reste au sommet", N, T);
			Line(Buf, bLeft);
		}
	}
}

int main()
{
	std::printf("# Banc du depart de groupe v4.12 (BRGatherLogic.cpp)\n\n");
	TestGather();
	TestClimbWait();
	TestGroup();
	std::printf("\nVerifications : %d, echecs : %d\nRESULTAT : %s\n", GChecks, GFailures, GFailures == 0 ? "OK" : "ECHEC");
	return GFailures == 0 ? 0 : 1;
}
