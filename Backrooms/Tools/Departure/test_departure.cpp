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
// v4.15 : une porte posee sur la face d'un mur vise un point dans la piece (30 cm au moins devant le plan du mur), et non
// plus la surface du mur (depart refuse devant l'ascenseur du Niveau 1 dans le test reseau).
// v4.15 (correctifs 4) : echelles du Niveau 8 tirees par les chunks (partie classique) : avec les tirages du jeu
// (BRMech::Grid::PickWallFace, BRMech::Cave::WallRock et CellRock, BRMechLogic.cpp) et son point vise (BRGather::ViewPoint),
// l'echelle BRExit_42 de la graine 233637237 (journal final2/v413) est rejouee : le rocher de sa paroi enterre son point vise,
// son accroche et la colonne du grimpeur (regle d'avant), la regle du jeu (BRMech::Cave::InExitKeepOut, ABRWorld::IsChunkExitKeepOut)
// ne pose pas ce rocher ; sur 200 graines, aucun rocher garde ne touche le point vise, la colonne ni l'approche.
//
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Departure/test_departure.cpp
//       Source/Backrooms/Private/BRGatherLogic.cpp Source/Backrooms/Private/BRMechLogic.cpp Source/Backrooms/Private/BRMissionLogic.cpp
//       -o test_departure && ./test_departure
#include "BRGatherLogic.h"
#include "BRMechLogic.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
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
		// v4.15 : porte posee sur la face d'un mur (ascenseur, porte d'hotel, porte de la mission : le pied est sur le plan du
		// mur, Forward vers la piece). Le point vise par la vue doit etre dans la piece, a 30 cm au moins du plan du mur (avant :
		// sur le plan, a distance nulle de la boite de collision) ; le point de rassemblement et la portee ne changent pas
		{
			bool bAll = true;
			const FVec Fwds[4] = { { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { -1.f, 0.f, 0.f }, { 0.f, -1.f, 0.f } };
			for (const FVec& F : Fwds)
			{
				FExitShape Wall;
				Wall.Style = EStyle::Door;
				Wall.Foot = { -900.f, -1860.f, 0.f };
				Wall.Forward = F;
				const FVec WallView = ViewPoint(Wall);
				const FVec WallGather = GatherPoint(Wall);
				const float Front = (WallView.X - Wall.Foot.X) * F.X + (WallView.Y - Wall.Foot.Y) * F.Y;
				const float Side = std::fabs((WallView.X - Wall.Foot.X) * F.Y - (WallView.Y - Wall.Foot.Y) * F.X);
				bAll &= Front >= 30.f && Side < 0.01f && std::fabs(WallView.Z - (Wall.Foot.Z + ViewHeight)) < 0.01f;
				bAll &= std::fabs(WallGather.X - Wall.Foot.X) < 0.01f && std::fabs(WallGather.Y - Wall.Foot.Y) < 0.01f && std::fabs(WallGather.Z - Wall.Foot.Z) < 0.01f;
				const FVec Near = At(Wall.Foot.X + F.X * 300.f, Wall.Foot.Y + F.Y * 300.f, CapsuleHalf);
				const FVec Far = At(Wall.Foot.X + F.X * 400.f, Wall.Foot.Y + F.Y * 400.f, CapsuleHalf);
				bAll &= CanRequest(Wall, Near) == EGather::FloorNeedsView && CanRequest(Wall, Far) == EGather::No;
			}
			Line("porte sur la face d'un mur (4 sens) : vue visee 30 cm au moins dans la piece, rassemblement au pied", bAll);
			// Niveau 1 du test reseau v4.15 : ascenseur sur la face y = -1860 (piece vers -Y), hote en (-815, -1945, 105) :
			// toute la ligne de vue reste devant le mur, a 50 cm de lui au point vise
			FExitShape Lift;
			Lift.Style = EStyle::Door;
			Lift.Foot = { -900.f, -1860.f, 0.f };
			Lift.Forward = { 0.f, -1.f, 0.f };
			const FVec LiftView = ViewPoint(Lift);
			const FVec Eye = At(-815.f, -1945.f, 105.f + 66.f);
			const float Clear = std::fmin(Lift.Foot.Y - LiftView.Y, Lift.Foot.Y - Eye.Y);
			char Buf[128];
			std::snprintf(Buf, sizeof(Buf), "Niveau 1, ascenseur du test reseau : demande a portee, vue a %.0f cm du mur au point vise (avant : 0)", Clear);
			Line(Buf, CanRequest(Lift, At(-815.f, -1945.f, 105.f)) == EGather::FloorNeedsView && Clear >= 30.f);
		}
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

namespace
{
	// =================================================================================================================
	// 4. v4.15 (correctifs 4) : echelles des chunks du Niveau 8 et rochers de leur paroi
	// =================================================================================================================

	// Donnees du jeu reprises par le banc (entrees, pas des regles) : Niveau 8 (BRLevels.cpp : cellules de 400 cm, 8 par chunk,
	// 38 % de cellules pleines, densite des accessoires 0,3, echelle vers le Niveau 9 a 12 % par chunk), bornes de SM_Rock
	// (RawAssets/Meshes/SM_Rock.fbx, comme Tools/verify_device_clearance.py), accroche du grimpeur (ABRExit::GetClimbAnchor)
	constexpr float CaveCell = 400.f;
	constexpr int CaveChunk = 8;
	constexpr float CaveSolid = 0.38f;
	constexpr float CaveProps = 0.3f;
	constexpr float LadderChance = 0.12f;
	constexpr float ClimbAnchor = 52.f;
	constexpr float CaveWall = 60.f;

	int FloorDiv(int A, int B)
	{
		return (A >= 0) ? (A / B) : -((-A + B - 1) / B);
	}

	/** ABRWorld::IsSolid, grottes (grille de la graine) */
	bool CaveIsSolid(int X, int Y, uint32_t Seed)
	{
		if ((X < 0 ? -X : X) <= 1 && (Y < 0 ? -Y : Y) <= 1)
		{
			return false;
		}
		const float R = 0.55f * BRMech::Grid::Rand(X, Y, 601, Seed) + 0.45f * BRMech::Grid::Rand(FloorDiv(X, 3), FloorDiv(Y, 3), 602, Seed);
		return R < CaveSolid;
	}

	bool IsSpawnCell(int X, int Y)
	{
		return (X < 0 ? -X : X) <= 1 && (Y < 0 ? -Y : Y) <= 1;
	}

	/** Le monde des grottes vu par le tirage d'une sortie murale (ABRWorld : niveau ouvert, sans bassin, fosse, cachette ni mur
	 *  fin ; les cellules de la mission ne sont pas rejouees ici) */
	struct FCaveQuery final : BRMech::Grid::FFaceQuery
	{
		uint32_t Seed = 0;
		virtual bool CellOk(int32_t X, int32_t Y) const override
		{
			return !CaveIsSolid(X, Y, Seed) && !IsSpawnCell(X, Y);
		}
		virtual bool IsFace(int32_t X, int32_t Y, int32_t DX, int32_t DY, bool& bOutSolid) const override
		{
			bOutSolid = CaveIsSolid(X + DX, Y + DY, Seed);
			return bOutSolid;
		}
	};

	/** Echelle tiree par le chunk (ABRChunk::PlanExits, sortie 0 du Niveau 8) : repere de son pied */
	bool ChunkLadder(int CX, int CY, uint32_t Seed, BRMech::FFrame& Out, BRMech::Grid::FWallFace& OutFace)
	{
		if ((CX == 0 && CY == 0) || !(BRMech::Grid::Rand(CX, CY, 1100, Seed) < LadderChance))
		{
			return false;
		}
		FCaveQuery Q;
		Q.Seed = Seed;
		if (!BRMech::Grid::PickWallFace(CX, CY, 0, Seed, CaveChunk, false, Q, OutFace))
		{
			return false;
		}
		Out = BRMech::Grid::WallFaceFrame(OutFace, CaveCell, CaveWall, 0.f);
		return true;
	}

	/** Boite de collision d'un rocher (rectangle oriente et hauteur) */
	struct FRockBox
	{
		BRMech::Clearance::FRect R;
		float Z0 = 0.f, Z1 = 0.f;
		bool bWall = true;
		float Dist2D(float X, float Y) const
		{
			const float Rad = R.Yaw * 3.14159265f / 180.f, C = std::cos(Rad), S = std::sin(Rad);
			const float DX = X - R.CX, DY = Y - R.CY;
			const float LX = DX * C + DY * S, LY = -DX * S + DY * C;
			const float EX = std::fmax(std::fabs(LX) - R.HX, 0.f), EY = std::fmax(std::fabs(LY) - R.HY, 0.f);
			return std::sqrt(EX * EX + EY * EY);
		}
		float Dist(float X, float Y, float Z) const
		{
			const float D = Dist2D(X, Y), EZ = std::fmax(std::fmax(Z0 - Z, 0.f), Z - Z1);
			return std::sqrt(D * D + EZ * EZ);
		}
	};

	BRMech::Cave::FMeshBounds RockBounds()
	{
		BRMech::Cave::FMeshBounds B;
		B.Min.X = -112.93f;
		B.Min.Y = -80.78f;
		B.Min.Z = -36.39f;
		B.Max.X = 100.91f;
		B.Max.Y = 79.54f;
		B.Max.Z = 115.91f;
		return B;
	}

	FRockBox BoxOf(const BRMech::Cave::FRock& Rock, bool bWall)
	{
		FRockBox B;
		B.R = BRMech::Cave::RockRect(Rock, RockBounds());
		BRMech::Cave::RockHeight(Rock, RockBounds(), B.Z0, B.Z1);
		B.bWall = bWall;
		return B;
	}

	/** Rochers qu'ABRChunk pose autour de la cellule C : parois des cellules pleines (3 cellules autour, etape 5) et cellules libres
	 *  (2 cellules autour ; ni au depart, ni dans la cellule devant la sortie, BuildCellProps) */
	void RocksAround(int CX, int CY, int FrontX, int FrontY, uint32_t Seed, std::vector<FRockBox>& Out)
	{
		Out.clear();
		for (int X = CX - 3; X <= CX + 3; ++X)
		{
			for (int Y = CY - 3; Y <= CY + 3; ++Y)
			{
				if (CaveIsSolid(X, Y, Seed))
				{
					for (int K = 0; K < 4; ++K)
					{
						BRMech::Cave::FRock Rock;
						if (!CaveIsSolid(X + BRMech::Grid::DirX[K], Y + BRMech::Grid::DirY[K], Seed) && BRMech::Cave::WallRock(X, Y, K, Seed, CaveCell, Rock))
						{
							Out.push_back(BoxOf(Rock, true));
						}
					}
				}
				else if (!IsSpawnCell(X, Y) && !(X == FrontX && Y == FrontY) && std::abs(X - CX) <= 2 && std::abs(Y - CY) <= 2)
				{
					BRMech::Cave::FRock Rock;
					if (BRMech::Cave::CellRock(X, Y, Seed, CaveCell, CaveProps, Rock))
					{
						Out.push_back(BoxOf(Rock, false));
					}
				}
			}
		}
	}

	/** Ce qu'un rocher fait a une echelle : point vise (sonde de 10 cm, ABRExit::CheckDepartureViewPoint), colonne du grimpeur
	 *  (capsule de 34 cm a l'accroche, rocher plus haut qu'une marche), approche (place debout de l'accroche a 90 cm devant elle) */
	struct FLadderHit
	{
		bool bView = false, bColumn = false, bApproach = false;
	};
	FLadderHit HitOf(const FRockBox& B, const BRMech::FFrame& F, const BRGather::FExitShape& Sh)
	{
		FLadderHit H;
		const BRGather::FVec V = BRGather::ViewPoint(Sh);
		H.bView = B.Dist(V.X, V.Y, V.Z) <= 10.f;
		const bool bTall = B.Z1 > F.Z + 35.f;
		H.bColumn = bTall && B.Dist2D(Sh.Anchor.X, Sh.Anchor.Y) < 34.f;
		for (float X = ClimbAnchor; X <= ClimbAnchor + 90.f + 0.5f && bTall; X += 10.f)
		{
			H.bApproach |= B.Dist2D(Sh.Foot.X + Sh.Forward.X * X, Sh.Foot.Y + Sh.Forward.Y * X) < 34.f;
		}
		return H;
	}

	BRGather::FExitShape ShapeOf(const BRMech::FFrame& F)
	{
		BRGather::FExitShape Sh;
		Sh.Style = EStyle::Ladder;
		Sh.Foot = { F.X, F.Y, F.Z };
		const float R = F.Yaw * 3.14159265f / 180.f;
		Sh.Forward = { std::round(std::cos(R)), std::round(std::sin(R)), 0.f };
		Sh.Anchor = { F.X + Sh.Forward.X * ClimbAnchor, F.Y + Sh.Forward.Y * ClimbAnchor, F.Z };
		Sh.TopZ = F.Z + 450.f + 150.f;
		return Sh;
	}

	uint32_t SeedMix(uint32_t X)
	{
		X ^= X >> 16;
		X *= 0x7feb352dU;
		X ^= X >> 15;
		X *= 0x846ca68bU;
		X ^= X >> 16;
		return X;
	}

	void TestChunkLadderView()
	{
		std::printf("4. Echelles des chunks du Niveau 8 et rochers de leur paroi (v4.15 correctifs 4)\n");
		char Buf[256];
		// Grille rejouee : la cellule (0, 0) libre, une valeur de hachage du journal de diagnostic
		const uint32_t Seed = 233637237u;
		const float Roll = BRMech::Grid::Rand(1, -2, 1100, Seed);
		std::snprintf(Buf, sizeof(Buf), "hachage de la grille (BRMech::Grid::Rand, comme BRHash::Rand) : tirage du chunk (1, -2) %.4f (diag4 : 0.0243)", Roll);
		Line(Buf, std::fabs(Roll - 0.0243f) < 0.00005f);
		BRMech::FFrame F;
		BRMech::Grid::FWallFace Face;
		const bool bLadder = ChunkLadder(1, -2, Seed, F, Face);
		std::snprintf(Buf, sizeof(Buf), "echelle du chunk (1, -2) : cellule (%d, %d), pied en (%.0f, %.0f, %.0f), lacet %.0f (journal : BRExit_42 en (4000, -5400, 0), vers +X)", Face.CellX,
			Face.CellY, F.X, F.Y, F.Z, F.Yaw);
		Line(Buf, bLadder && Face.CellX == 10 && Face.CellY == -14 && Face.bSolid && F.X == 4000.f && F.Y == -5400.f && F.Yaw == 0.f);
		const BRGather::FExitShape Sh = ShapeOf(F);
		// Rocher de la face +X de la cellule pleine (9, -14), contre l'echelle
		BRMech::Cave::FRock Rock;
		const bool bRock = BRMech::Cave::WallRock(9, -14, 0, Seed, CaveCell, Rock);
		const FRockBox Box = BoxOf(Rock, true);
		const BRGather::FVec V = ViewPoint(Sh);
		std::snprintf(Buf, sizeof(Buf), "rocher de la paroi (9, -14), face +X : echelle %.2f, boite de %.0f x %.0f cm, Z %.0f..%.0f ; point vise (%.0f, %.0f, %.0f) a %.0f cm de sa boite",
			Rock.Scale, Box.R.HX * 2.f, Box.R.HY * 2.f, Box.Z0, Box.Z1, V.X, V.Y, V.Z, Box.Dist(V.X, V.Y, V.Z));
		Line(Buf, bRock && Box.Dist(V.X, V.Y, V.Z) <= 0.f && std::fabs(Rock.Scale - 1.49f) < 0.01f);
		const FLadderHit Old = HitOf(Box, F, Sh);
		Line("regle d'avant : ce rocher prend aussi la colonne du grimpeur (accroche a 52 cm) et l'approche : echelle ni visee ni montee", Old.bColumn && Old.bApproach);
		Line("regle du jeu (BRMech::Cave::InExitKeepOut, zone libre de la sortie) : ce rocher n'est pas pose", BRMech::Cave::InExitKeepOut(Box.R, &F, 1));
		// 200 graines, chunks -4..4 (graines du diagnostic : diag4/level8_departure_view, probe_l8_exit.py --stats et --rules)
		int Ladders = 0, OldView = 0, OldColumn = 0, OldApproach = 0, OldViewOnly = 0, NewView = 0, NewColumn = 0, NewApproach = 0, Near = 0, Removed = 0;
		std::vector<FRockBox> Rocks;
		for (uint32_t Si = 0; Si < 200; ++Si)
		{
			uint32_t S = SeedMix((Si + 1u) * 2654435761u);
			S = S != 0 ? S : 1u;
			std::vector<BRMech::FFrame> Frames;
			std::vector<BRMech::Grid::FWallFace> Faces;
			for (int CX = -5; CX <= 5; ++CX)
			{
				for (int CY = -5; CY <= 5; ++CY)
				{
					BRMech::FFrame G;
					BRMech::Grid::FWallFace Fc;
					if (ChunkLadder(CX, CY, S, G, Fc))
					{
						Frames.push_back(G);
						Faces.push_back(Fc);
					}
				}
			}
			for (size_t L = 0; L < Frames.size(); ++L)
			{
				const BRMech::FFrame& G = Frames[L];
				const int ChX = FloorDiv(Faces[L].CellX, CaveChunk), ChY = FloorDiv(Faces[L].CellY, CaveChunk);
				if (ChX < -4 || ChX > 4 || ChY < -4 || ChY > 4)
				{
					continue;
				}
				++Ladders;
				const BRGather::FExitShape GS = ShapeOf(G);
				// Cellule devant la sortie (ABRChunk::PlanExits, ExitCells : pied + 30 cm) : pas de rocher de cellule
				const int FrontX = static_cast<int>(std::floor((G.X + GS.Forward.X * 30.f) / CaveCell)), FrontY = static_cast<int>(std::floor((G.Y + GS.Forward.Y * 30.f) / CaveCell));
				RocksAround(Faces[L].CellX, Faces[L].CellY, FrontX, FrontY, S, Rocks);
				FLadderHit O, N;
				for (const FRockBox& B : Rocks)
				{
					const FLadderHit H = HitOf(B, G, GS);
					O.bView |= H.bView;
					O.bColumn |= H.bColumn;
					O.bApproach |= H.bApproach;
					++Near;
					if (BRMech::Cave::InExitKeepOut(B.R, Frames.data(), static_cast<int>(Frames.size())))
					{
						++Removed;
						continue;
					}
					N.bView |= H.bView;
					N.bColumn |= H.bColumn;
					N.bApproach |= H.bApproach;
				}
				OldView += O.bView ? 1 : 0;
				OldColumn += O.bColumn ? 1 : 0;
				OldApproach += O.bApproach ? 1 : 0;
				OldViewOnly += (O.bView && !O.bColumn) ? 1 : 0;
				NewView += N.bView ? 1 : 0;
				NewColumn += N.bColumn ? 1 : 0;
				NewApproach += N.bApproach ? 1 : 0;
			}
		}
		std::snprintf(Buf, sizeof(Buf), "regle d'avant, %d echelles (200 graines) : point vise dans un rocher %d, colonne prise %d, approche prise %d, point vise seul (colonne libre) %d",
			Ladders, OldView, OldColumn, OldApproach, OldViewOnly);
		Line(Buf, Ladders > 1000 && OldView > 0 && OldColumn > 0 && OldApproach > 0);
		std::snprintf(Buf, sizeof(Buf), "regle du jeu : %d rochers retires sur %d voisins ; point vise %d, colonne %d, approche %d (attendu 0)", Removed, Near, NewView, NewColumn, NewApproach);
		Line(Buf, Removed > 0 && Removed * 10 < Near && NewView == 0 && NewColumn == 0 && NewApproach == 0);
		// Le point vise par le depart (inchange : 1 m devant le pied, 90 cm) est dans la zone libre d'une echelle, dans les 4 sens ;
		// porte : 50 cm devant le pied
		bool bAll = true;
		const int Fw[4][2] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };
		for (const auto& D : Fw)
		{
			BRMech::FFrame G;
			G.X = -1200.f;
			G.Y = 3600.f;
			G.Yaw = std::atan2(static_cast<float>(D[1]), static_cast<float>(D[0])) * 180.f / 3.14159265f;
			const BRGather::FExitShape GS = ShapeOf(G);
			const BRGather::FVec P = ViewPoint(GS);
			BRMech::Clearance::FRect Probe;
			Probe.CX = P.X;
			Probe.CY = P.Y;
			Probe.HX = Probe.HY = 10.f;
			float LX = 0.f, LY = 0.f;
			G.ToLocal(P.X, P.Y, LX, LY);
			const BRMech::Clearance::FRect K = BRMech::Clearance::ExitKeepOut(G);
			bAll &= LX + 10.f <= BRMech::Clearance::StandDepth + 40.f && std::fabs(LY) + 10.f <= K.HY && BRMech::Clearance::Overlap(K, Probe);
			G.ToLocal(GS.Anchor.X, GS.Anchor.Y, LX, LY);
			bAll &= LX - BRMech::Clearance::PlayerRadius >= -12.f && LX + BRMech::Clearance::PlayerRadius <= BRMech::Clearance::StandDepth + 40.f;
		}
		FExitShape Door;
		Door.Foot = { 0.f, 0.f, 0.f };
		const FVec DV = ViewPoint(Door);
		Line("point vise d'une echelle (1 m, 90 cm) et colonne du grimpeur dans la zone libre (4 sens) ; porte : 50 cm devant le pied", bAll
			&& std::fabs(DV.X - DoorViewInset) < 0.01f && std::fabs(DV.Z - ViewHeight) < 0.01f);
	}
}

int main()
{
	std::printf("# Banc du depart de groupe v4.12 (BRGatherLogic.cpp)\n\n");
	TestGather();
	TestClimbWait();
	TestGroup();
	TestChunkLadderView();
	std::printf("\nVerifications : %d, echecs : %d\nRESULTAT : %s\n", GChecks, GFailures, GFailures == 0 ? "OK" : "ECHEC");
	return GFailures == 0 ? 0 : 1;
}
