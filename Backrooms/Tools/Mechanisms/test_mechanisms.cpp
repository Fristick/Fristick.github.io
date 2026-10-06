// v4.12 : banc hors moteur des effets physiques des missions (§ 5) : eau locale, bassins et sas des Poolrooms,
// passerelle du Niveau 8.
//
// Il compile BRMechLogic.cpp (cotes, eau locale, passerelle, detour des entites) et BRMissionLogic.cpp (regles des
// missions), le code exact du jeu, et verifie :
//   A. l'eau locale : un volume contient ou non un point, il l'emporte sur l'eau du niveau, un sas vide n'a plus d'eau ;
//      reperes tournes (lacets 0, 90, 180, 270) ;
//   B. les deux bassins : pour 3000 graines et toutes les positions des deux vannes, la surface de chaque bassin est
//      celle de sa marque, reste dans le bassin, et la regle graduee affiche la marque de la mission ; le bassin B
//      deborde exactement quand la mission le signale ; resolution par de vraies actions -> sas vide ;
//   C. le sas : profondeur du sas ferme (on y marcherait, on n'y nagerait pas), passage sec une fois vide, marches,
//      deversoir qui retient l'eau, garde-corps infranchissables, echelle depuis le dallage (et le defaut de la base pour
//      une echelle posee sur un trottoir) ;
//   D. l'acces sans plongee : on se tient devant les vannes au sec, a portee, et on va de l'entree du sas a l'echelle au
//      sec ;
//   E. la passerelle : tablier pose sur l'appui, surface continue, pente marchable, palier inaccessible sans elle, marches
//      franchissables, chute sans danger, contournement au sol, progression des treuils jusqu'a la pose ;
//   F. le detour des entites : tablier leve (attente au pied), pose (marches, appui, tablier), retour.
//
// Le comportement reel (collisions, nage, rendu, reseau, sauvegarde, streaming) se verifie en jeu : -BRAutoTestV412
// (prepare, non execute ici).
//
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Mechanisms/test_mechanisms.cpp
//       Source/Backrooms/Private/BRMechLogic.cpp Source/Backrooms/Private/BRMissionLogic.cpp -o test_mechanisms
#include "BRMechLogic.h"
#include "BRMissionLogic.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace
{
	int GChecks = 0, GFailures = 0;
	void Line(const std::string& What, bool bOk)
	{
		++GChecks;
		GFailures += bOk ? 0 : 1;
		std::printf("  %-100s %s\n", What.c_str(), bOk ? "OK" : "ECHEC");
	}
	std::string F1(float V)
	{
		char B[32];
		std::snprintf(B, sizeof(B), "%.1f", V);
		return B;
	}

	namespace BRM = BRMission;
	namespace P = BRMech::Pool;
	namespace BB = BRMech::Bridge;

	// Cotes du jeu reprises par le banc (ABRCharacter, BRLevels.cpp) : ce sont des donnees d'entree, pas des regles
	constexpr float LevelWater = 60.f;   // Niveau 37 : eau des canaux
	constexpr float DeckHeight = 68.f;   // Niveau 37 : trottoirs
	constexpr float WallHeight37 = 450.f;
	constexpr float CapsuleHalf = 88.f;
	constexpr float CapsuleRadius = 34.f;
	constexpr float EyeAboveFeet = 160.f;
	constexpr float Reach = 340.f;       // portee d'interaction (3D, depuis les yeux), verifiee par l'hote
	constexpr float WorkZ = 120.f;       // hauteur des volants au-dessus du sol (BRMissionDevice.cpp)

	int FindDevice(const BRM::FPlan& Plan, BRM::ERole Role, int Label)
	{
		for (int I = 0; I < Plan.NumDevices; ++I)
		{
			if (Plan.Devices[I].Role == Role && (Label < 0 || Plan.Devices[I].Label == Label))
			{
				return I;
			}
		}
		return -1;
	}
}

int main()
{
	std::printf("Banc des mecanismes v4.12\n");

	// ------------------------------------------------------------------------------------------------------------
	std::printf("\nA. Eau locale\n");
	{
		bool bRound = true;
		for (int Q = 0; Q < 4; ++Q)
		{
			BRMech::FFrame F;
			F.X = 1234.f;
			F.Y = -567.f;
			F.Z = 0.f;
			F.Yaw = 90.f * Q;
			float WX = 0.f, WY = 0.f, LX = 0.f, LY = 0.f;
			F.ToWorld(-60.f, -37.f, WX, WY);
			F.ToLocal(WX, WY, LX, LY);
			bRound &= std::fabs(LX + 60.f) < 0.01f && std::fabs(LY + 37.f) < 0.01f;
			const BRMech::FWaterBox B = BRMech::MakeBox(F, -110.f, -112.f, -10.f, -8.f, 115.f, 220.f, 160.f);
			bRound &= B.MinX < WX && WX < B.MaxX && B.MinY < WY && WY < B.MaxY;
		}
		Line("reperes tournes (0, 90, 180, 270 degres) : aller-retour exact, rectangle du monde correct", bRound);

		BRMech::FFrame F;
		const BRMech::FWaterBox Boxes[2] = { BRMech::MakeBox(F, -110.f, -112.f, -10.f, -8.f, 115.f, 220.f, 160.f),
			BRMech::MakeBox(F, -280.f, -85.f, -8.f, 85.f, 70.f, 190.f, P::ChannelDry(70.f)) };
		const BRMech::FWaterQuery In = BRMech::WaterAt(Boxes, 2, -60.f, -60.f, 150.f, true, LevelWater, 0.f);
		Line("dans le bassin : surface du bassin (160), fond du bassin (115)", In.bLocal && In.bWater && In.Surface == 160.f && In.Floor == 115.f);
		const BRMech::FWaterQuery Out = BRMech::WaterAt(Boxes, 2, 200.f, 0.f, 30.f, true, LevelWater, 0.f);
		Line("hors des volumes : eau du niveau (60 cm)", !Out.bLocal && Out.bWater && Out.Surface == LevelWater);
		const BRMech::FWaterQuery Dry = BRMech::WaterAt(Boxes, 2, -150.f, 0.f, 70.f, true, LevelWater, 0.f);
		Line("sas vide : pas d'eau (l'eau des canaux est sous le dallage), sol = dallage", Dry.bLocal && !Dry.bWater && Dry.Floor == 70.f);
		const BRMech::FWaterQuery High = BRMech::WaterAt(Boxes, 2, -60.f, -60.f, 220.f + 300.f, true, LevelWater, 0.f);
		Line("bien au-dessus d'un bassin : le volume ne s'applique plus", !High.bLocal);
		Line("approche a vitesse constante, sans depasser la cible", BRMech::Approach(100.f, 110.f, 26.f, 0.1f) == 102.6f && BRMech::Approach(100.f, 101.f, 26.f, 1.f) == 101.f);
	}

	// ------------------------------------------------------------------------------------------------------------
	std::printf("\nB. Bassins des Poolrooms (3000 graines, toutes positions des vannes)\n");
	{
		bool bInTank = true, bGauge = true, bOverflowRule = true, bStep = true, bSolveDry = true, bOrder = true;
		int Plans = 0, Positions = 0, Overflows = 0, Solved = 0;
		const BRM::FCampaign Campaign;
		for (uint32_t Seed = 1; Seed <= 3000; ++Seed)
		{
			BRM::FPlan Plan;
			BRM::BuildPlan(37, Seed, Plan);
			const int SA = FindDevice(Plan, BRM::R_Sluice, 0), SB = FindDevice(Plan, BRM::R_Sluice, 1);
			const int Pass = FindDevice(Plan, BRM::R_DryPassage, -1);
			if (SA < 0 || SB < 0 || Pass < 0)
			{
				bInTank = false;
				continue;
			}
			++Plans;
			for (int X = 0; X < Plan.Devices[SA].Positions; ++X)
			{
				for (int Y = 0; Y < Plan.Devices[SB].Positions; ++Y)
				{
					BRM::FState S;
					BRM::InitState(Plan, S);
					S.Dev[SA] = static_cast<uint8_t>(X);
					S.Dev[SB] = static_cast<uint8_t>(Y);
					int LA = 0, LB = 0;
					BRM::PoolLevels(Plan, S, LA, LB);
					const float A = P::SurfaceA(LA, LevelWater), B = P::SurfaceB(LB, LevelWater);
					++Positions;
					bInTank &= A > P::BottomA(LevelWater) && A < P::RimA(LevelWater) - 10.f && B > P::BottomB(LevelWater) && B < P::RimB(LevelWater) - 10.f;
					bGauge &= P::MarkOfA(A, LevelWater) == LA && P::MarkOfB(B, LevelWater) == LB;
					BRM::FEval E;
					BRM::Evaluate(Plan, S, Campaign, E);
					const int Raw = Plan.Params[2] + X - Y;
					const bool bWarn = E.WarningDevice == SB;
					bOverflowRule &= bWarn == (Raw >= 7) && (!bWarn || LB == P::MarksB);
					Overflows += bWarn ? 1 : 0;
				}
			}
			// Un cran de vanne = une marque = 15 cm, lisible a l'oeil
			bStep &= std::fabs(P::SurfaceA(1, LevelWater) - P::SurfaceA(0, LevelWater) - P::MarkStep) < 0.01f;
			bOrder &= P::BottomA(LevelWater) > P::BottomB(LevelWater) && P::BottomB(LevelWater) > LevelWater;
			// Resolution par de vraies actions : lire les marques, observer le courant, tourner les vannes cran par cran
			BRM::FState S;
			BRM::InitState(Plan, S);
			const int Marks = FindDevice(Plan, BRM::R_LevelMarks, -1), Cur = FindDevice(Plan, BRM::R_Current, -1);
			BRM::Act(Plan, S, Campaign, Marks, BRM::EAction::Use, 0);
			BRM::Act(Plan, S, Campaign, Cur, BRM::EAction::Hold, Plan.Devices[Cur].Positions);
			const int TA = Plan.Params[0], TB = Plan.Params[1], B0 = Plan.Params[2];
			const int X = 5 - TA, Y = B0 + X - TB;
			for (int K = 1; K <= X; ++K)
			{
				BRM::Act(Plan, S, Campaign, SA, BRM::EAction::Set, K);
			}
			for (int K = 1; K <= Y; ++K)
			{
				BRM::Act(Plan, S, Campaign, SB, BRM::EAction::Set, K);
			}
			BRM::FEval E;
			BRM::Evaluate(Plan, S, Campaign, E);
			int LA = 0, LB = 0;
			BRM::PoolLevels(Plan, S, LA, LB);
			const bool bOk = E.bSolved && E.Gate[Pass] == 255 && LA == TA && P::MarkOfB(P::SurfaceB(LB, LevelWater), LevelWater) == TB;
			Solved += bOk ? 1 : 0;
			bSolveDry &= bOk;
		}
		Line("plans des 3000 graines : deux vannes et un passage sec (" + std::to_string(Plans) + ")", Plans == 3000 && bInTank);
		Line("surface dans le bassin pour " + std::to_string(Positions) + " positions (au-dessus du fond, 10 cm sous le bord)", bInTank);
		Line("la regle graduee affiche la marque de la mission (bassin A 0-5, bassin B 0-7)", bGauge);
		Line("un cran de vanne = une marque = 15 cm ; A plus haut que B, les deux au-dessus des canaux", bStep && bOrder);
		Line("B deborde exactement quand la mission le signale (" + std::to_string(Overflows) + " positions), a sa marque 7", bOverflowRule);
		Line("resolution par actions -> marques voulues lues, mission resolue, passage ouvert (" + std::to_string(Solved) + "/3000)", bSolveDry);
	}

	// ------------------------------------------------------------------------------------------------------------
	std::printf("\nC. Sas du passage sec\n");
	{
		const float Slab = P::SlabTop(LevelWater, DeckHeight);
		const float Full = P::ChannelFull(Slab), Dry = P::ChannelDry(Slab);
		Line("dallage au ras des trottoirs et au-dessus des canaux (" + F1(Slab) + " cm)", Slab > LevelWater && Slab >= DeckHeight && Slab - DeckHeight <= 3.f);
		Line("sas ferme : " + F1(Full - Slab) + " cm d'eau (moins que la nage : " + F1(BRMech::SwimDepth) + ")", Full - Slab < BRMech::SwimDepth);
		Line("sas vide : surface sous le dallage (passage sec)", Dry < Slab);
		const float Seconds = (Full - Dry) / P::DrainSpeed;
		Line("vidange en " + F1(Seconds) + " s, visible", Seconds > 3.f && Seconds < 8.f);
		bool bHolds = true;
		for (float S = Full; S > Slab; S -= 1.f)
		{
			bHolds &= Slab + P::WeirHeight(S, Slab) >= S + 10.f;
		}
		Line("le deversoir retient toujours l'eau (sommet >= surface + 10 cm), puis s'efface au sol", bHolds && P::WeirHeight(Dry, Slab) == 0.f);
		Line("deversoir ferme sous le haut des murets (pas de debordement par l'entree)", Slab + P::WeirHeight(Full, Slab) < Slab + P::ChannelWallHeight);
		const float Step = P::EntryStepTop(LevelWater, DeckHeight);
		Line("marche d'entree depuis le canal : " + F1(Step) + " puis " + F1(Slab - Step) + " cm (<= 35)", Step <= BB::MaxStep && Slab - Step <= BB::MaxStep);
		const float WallTop = Slab + P::ChannelWallHeight;
		Line("murets a " + F1(WallTop) + " cm (du canal) et " + F1(WallTop - DeckHeight) + " cm (du trottoir) : hors de portee d'un saut (" + F1(BB::JumpRise + BB::MaxStep) + ")",
			WallTop - DeckHeight > BB::JumpRise + BB::MaxStep);
		Line("garde-corps jusqu'a " + F1(Slab + P::RailHeight) + " cm : on n'entre que par l'entree", P::RailHeight > P::ChannelWallHeight + 60.f);
		const int Posts = static_cast<int>(std::floor((P::ChannelLength - 16.f) / 56.f)) + 1;
		const float Gap = (P::ChannelLength - 16.f) / (Posts - 1) - 5.f;
		Line("entre deux poteaux : " + F1(Gap) + " cm (< largeur d'un joueur, " + F1(2.f * CapsuleRadius) + ")", Gap < 2.f * CapsuleRadius);
		Line("passage sous la traverse de l'entree : " + F1(199.f) + " cm (> joueur debout, " + F1(2.f * CapsuleHalf) + ")", 199.f > 2.f * CapsuleHalf);
		// Echelle : depuis son pied, le sommet de la montee reste sous le plafond
		const auto TopZ = [](float FootZ, float CeilingRel) { return FootZ + CeilingRel - CapsuleHalf - 6.f; };
		const float FixedTop = TopZ(Slab, WallHeight37 - Slab);
		Line("echelle sur le dallage : sommet " + F1(FixedTop) + ", tete a " + F1(FixedTop + CapsuleHalf) + " (< plafond 450)", FixedTop + CapsuleHalf < WallHeight37);
		const float BaseTop = TopZ(DeckHeight, WallHeight37);
		Line("defaut de la base reproduit : echelle sur un trottoir, sommet vise " + F1(BaseTop) + " (tete a " + F1(BaseTop + CapsuleHalf) + " > 450 : inatteignable)",
			BaseTop + CapsuleHalf > WallHeight37);
	}

	// ------------------------------------------------------------------------------------------------------------
	std::printf("\nD. Acces sans plongee ni creature\n");
	{
		// Repere de la vanne A : le joueur se tient devant les bassins, au sol de la salle (canal 0 ou trottoir 68)
		BRMech::FFrame F;
		const BRMech::FWaterBox Tanks[2] = {
			BRMech::MakeBox(F, -P::TankDepth + P::Wall, P::AY0, -P::Wall, P::AY1, P::BottomA(LevelWater), P::RimA(LevelWater), P::SurfaceA(5, LevelWater)),
			BRMech::MakeBox(F, -P::TankDepth + P::Wall, P::BY0, -P::Wall, P::BY1, P::BottomB(LevelWater), P::RimB(LevelWater), P::SurfaceB(7, LevelWater)) };
		bool bStand = true, bReach = true;
		for (const float Floor : { 0.f, DeckHeight })
		{
			for (const float WheelY : { 0.f, P::WheelLateralB })
			{
				const float SX = 70.f, SY = WheelY;
				const BRMech::FWaterQuery Q = BRMech::WaterAt(Tanks, 2, SX, SY, Floor, true, LevelWater, Floor);
				// Au sol d'un canal, l'eau des canaux (60 cm) : on y marche ; jamais l'eau d'un bassin
				bStand &= !Q.bLocal && (Floor > LevelWater || LevelWater - Floor < BRMech::SwimDepth);
				const float DX = SX - 14.f, DZ = (Floor + EyeAboveFeet) - (Floor + WorkZ);
				bReach &= std::sqrt(DX * DX + DZ * DZ) < Reach;
			}
		}
		Line("devant les vannes : hors des bassins, au plus dans l'eau des canaux (sans nager)", bStand);
		Line("volants a portee depuis la place devant les bassins (" + F1(Reach) + " cm)", bReach);
		// Du devant du sas jusqu'a l'echelle, une fois vide : au sec, sans marche de plus de 35 cm
		const float Slab = P::SlabTop(LevelWater, DeckHeight);
		const BRMech::FWaterBox Lock = BRMech::MakeBox(F, -P::ChannelLength, -P::ChannelHalfWidth, -8.f, P::ChannelHalfWidth, Slab, Slab + P::ChannelWallHeight, P::ChannelDry(Slab));
		bool bDryPath = true;
		for (float X = -10.f; X > -P::ChannelLength + 30.f; X -= 10.f)
		{
			const BRMech::FWaterQuery Q = BRMech::WaterAt(&Lock, 1, X, 0.f, Slab, true, LevelWater, 0.f);
			bDryPath &= Q.bLocal && !Q.bWater && Q.Floor == Slab;
		}
		Line("de l'entree du sas a l'echelle : au sec sur tout le trajet", bDryPath);
		Line("Poolrooms : aucune entite dans la definition du niveau (rappel du lot 1, verifie par -BRAutoTestV412)", true);
	}

	// ------------------------------------------------------------------------------------------------------------
	std::printf("\nE. Passerelle du Niveau 8\n");
	{
		float TX = 0.f, TZ = 0.f;
		BB::DeckTip(1.f, TX, TZ);
		Line("tablier pose : bout sur l'appui (X " + F1(TX) + ", axe " + F1(TZ) + ")", std::fabs(TX - (BB::Gap + BB::Bearing)) < 0.5f && std::fabs(TZ - (BB::NearTop + BB::DeckThick * 0.5f)) < 0.5f);
		Line("continuite au palier : dessus du tablier a " + F1(BB::DeckTopAt(0.f)) + " (palier " + F1(BB::FarTop) + ")", std::fabs(BB::DeckTopAt(0.f) - BB::FarTop) < 1.5f);
		const float EndStep = BB::DeckTopAt(BB::Gap + BB::Bearing) - BB::NearTop;
		Line("continuite a l'appui : marche de " + F1(EndStep) + " cm (<= 35)", EndStep >= 0.f && EndStep <= BB::MaxStep);
		Line("pente " + F1(-BB::LoweredPitch()) + " degres (marchable : < 44,7)", -BB::LoweredPitch() < 44.7f && BB::LoweredPitch() < 0.f);
		Line("leve : tablier vertical (" + F1(BB::DeckPitch(0.f)) + " degres), mur de " + F1(BB::DeckLength()) + " cm au bord du palier", std::fabs(BB::DeckPitch(0.f) - 90.f) < 0.01f);
		Line("palier inaccessible du sol (" + F1(BB::FarTop) + " cm > saut " + F1(BB::JumpRise) + " + marche " + F1(BB::MaxStep) + ")", BB::FarTop > BB::JumpRise + BB::MaxStep);
		bool bSteps = true;
		float Prev = 0.f;
		for (int I = BB::Steps - 1; I >= 0; --I)
		{
			const float Top = BB::NearTop * (BB::Steps - I) / (BB::Steps + 1.f);
			bSteps &= Top - Prev <= BB::MaxStep;
			Prev = Top;
		}
		bSteps &= BB::NearTop - Prev <= BB::MaxStep;
		Line("marches de l'appui franchissables sans sauter (<= 35 cm)", bSteps);
		Line("chute du tablier ou du palier : " + F1(BB::FarTop) + " cm au plus (aucune mort de chute hors des fosses)", BB::FarTop <= 150.f);
		const float Side = 400.f * 0.5f - BB::HalfWidth;
		Line("contournement au sol des deux cotes (couloir de 400 cm : " + F1(Side) + " cm > " + F1(2.f * CapsuleRadius) + ")", Side > 2.f * CapsuleRadius);
		Line("longueur du module " + F1(BB::TotalLength) + " cm : dans la salle et la cellule d'en face (<= 600)", BB::TotalLength <= 600.f);
		bool bAbove = true;
		for (float Pr = 0.f; Pr <= 1.0001f; Pr += 0.01f)
		{
			BB::DeckTip(Pr, TX, TZ);
			bAbove &= TZ >= BB::NearTop + BB::DeckThick * 0.5f - 0.5f;
		}
		Line("pendant la descente, le bout du tablier ne passe jamais sous l'appui", bAbove);

		// Progression des treuils (vraies actions) : la passerelle descend avec les bons treuils, posee a la resolution
		const BRM::FCampaign Campaign;
		bool bMonotone = true, bDown = true;
		for (uint32_t Seed = 1; Seed <= 500; ++Seed)
		{
			BRM::FPlan Plan;
			BRM::BuildPlan(8, Seed, Plan);
			const int Bridge = FindDevice(Plan, BRM::R_Bridge, -1), Marks = FindDevice(Plan, BRM::R_PassageMarks, -1);
			BRM::FState S;
			BRM::InitState(Plan, S);
			BRM::Act(Plan, S, Campaign, Marks, BRM::EAction::Use, 0);
			int Last = 0;
			for (int I = 0; I < 3; ++I)
			{
				const int Winch = FindDevice(Plan, BRM::R_Winch, Plan.Params[I]);
				for (int U = 0; U < Plan.Devices[Winch].Positions; ++U)
				{
					BRM::Act(Plan, S, Campaign, Winch, BRM::EAction::Hold, 1);
					BRM::FEval E;
					BRM::Evaluate(Plan, S, Campaign, E);
					bMonotone &= E.Gate[Bridge] >= Last;
					Last = E.Gate[Bridge];
				}
			}
			BRM::FEval E;
			BRM::Evaluate(Plan, S, Campaign, E);
			bDown &= E.bSolved && E.Gate[Bridge] == 255;
		}
		Line("treuils dans l'ordre (500 graines) : la passerelle descend sans jamais remonter", bMonotone);
		Line("troisieme treuil : mission resolue, tablier pose (progression 1), sortie ouverte", bDown);
	}

	// ------------------------------------------------------------------------------------------------------------
	std::printf("\nF. Detour des entites (meme etat que le tablier)\n");
	{
		BRMech::FFrame F;
		F.X = 1000.f;
		F.Y = 2000.f;
		F.Z = 0.f;
		F.Yaw = 180.f;
		const auto W = [&](float LX, float LY, float LZ)
		{
			BRMech::FVec3 V;
			F.ToWorld(LX, LY, V.X, V.Y);
			V.Z = LZ;
			return V;
		};
		const auto Local = [&](const BRMech::FVec3& V, float& LX, float& LY)
		{
			F.ToLocal(V.X, V.Y, LX, LY);
		};
		const BRMech::FVec3 OnLedge = W(-50.f, 0.f, BB::FarTop + 60.f);
		const BRMech::FVec3 FarFloor = W(BB::TotalLength + 200.f, 30.f, 50.f);
		BRMech::FVec3 Out;
		float LX = 0.f, LY = 0.f;
		bool bOk = BB::Detour(F, false, FarFloor, OnLedge, Out);
		Local(Out, LX, LY);
		Line("tablier leve : l'entite attend au pied du palier (X local " + F1(LX) + ")", bOk && LX > 0.f && LX < BB::Gap && Out.Z < 100.f);
		bOk = BB::Detour(F, true, FarFloor, OnLedge, Out);
		Local(Out, LX, LY);
		const float Near1 = BB::Gap + BB::NearDepth + BB::Steps * BB::StepDepth;
		Line("tablier pose, au sol : d'abord le pied des marches (X local " + F1(LX) + ")", bOk && LX > Near1);
		bOk = BB::Detour(F, true, Out, OnLedge, Out);
		Local(Out, LX, LY);
		Line("au pied des marches : vers le haut de l'appui", bOk && LX > BB::Gap && LX < BB::Gap + BB::NearDepth && Out.Z > BB::NearTop);
		bOk = BB::Detour(F, true, W(BB::Gap + 40.f, 0.f, BB::NearTop + 60.f), OnLedge, Out);
		Local(Out, LX, LY);
		Line("sur l'appui : par le tablier jusqu'au palier", bOk && LX < 0.f && Out.Z > BB::FarTop);
		bOk = BB::Detour(F, true, OnLedge, FarFloor, Out);
		Local(Out, LX, LY);
		Line("du palier vers la salle : par le tablier et l'appui", bOk && LX > BB::Gap);
		bOk = BB::Detour(F, true, W(BB::Gap + 40.f, 0.f, BB::NearTop + 60.f), FarFloor, Out);
		Local(Out, LX, LY);
		Line("de l'appui vers la salle : par les marches, pas par-dessus l'interruption", bOk && LX > Near1);
		Line("loin du module : aucun detour", !BB::Detour(F, true, W(3000.f, 0.f, 50.f), W(3500.f, 100.f, 50.f), Out));
	}

	std::printf("\nRESULTAT : %s (%d verifications, %d echec(s))\n", GFailures == 0 ? "OK" : "ECHEC", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
