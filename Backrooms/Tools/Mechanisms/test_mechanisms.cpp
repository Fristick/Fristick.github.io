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
//   F. le detour des entites : tablier leve (attente au pied), pose (marches, appui, tablier), retour ;
//   G. v4.15 : l'emprise des mecanismes (BRMech::Clearance, regle du placement, des rochers et des tests du jeu) : cage et
//      poste du Level 6 (point d'interaction du poste dans la cage avec la regle v4.14, a cote d'elle avec PickSlot ; cage
//      au bout de sa face, place ordinaire du poste libre du mur voisin), cage tenue dans sa face, premiere place libre
//      sans volume (placement des autres niveaux inchange), rectangles orientes, zone libre devant un treuil et devant une
//      sortie (echelle), boites visees de toutes les sortes et de tous les roles, volumes des modules ; v4.15 (correctifs 3) :
//      sonde du point d'interaction des tests plus fine que le plus petit ecart point vise - mur ;
//   H. v4.15 (correctifs 3) : couloirs de visee (BRMech::Clearance::CutsFocusLane, regle de PickSlot) : le groupe du moulin
//      du Niveau 10 (graine 1697132605 : tableau des granges entre le moulin et le frein), poteaux d'une cellule en rang,
//      PickSlot (poteau au couloir degage, choix d'avant faute de mieux), voisins muraux et Level 6 inchanges.
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

	// v4.15 : emprise des mecanismes (BRMech::Clearance, la regle du placement, des rochers et des tests du jeu)
	std::printf("\nG. Emprise des mecanismes et places libres (v4.15)\n");
	{
		namespace CL = BRMech::Clearance;
		namespace LC = BRMech::LidarCage;
		const CL::FFootprint Cage = CL::Of(BRM::EKind::Observe, BRM::R_LidarCage, BRMech::Module::None, 60.f);
		const CL::FFootprint Post = CL::Of(BRM::EKind::Crank, BRM::R_LidarPost, BRMech::Module::None, 60.f);
		Line("cage du Level 6 : volume de " + F1(LC::Gap) + " a " + F1(LC::Gap + LC::Depth) + " cm du mur, +-" + F1(LC::HalfWidth) + ", " + F1(LC::Height) + " cm de haut",
			Cage.bSolid && Cage.Solid.X0 == LC::Gap && Cage.Solid.X1 == LC::Gap + LC::Depth && Cage.Solid.Y1 == LC::HalfWidth && Cage.Solid.Z1 == LC::Height);
		Line("poste de confinement : point d'interaction (" + F1(Post.Aim.X) + ", " + F1(Post.Aim.Y) + ", " + F1(Post.Aim.Z) + "), sans volume",
			!Post.bSolid && Post.Aim.X == 18.f && Post.Aim.Z == 110.f);
		// Face d'une cellule de 350 cm (Level 6, murs de 60 cm) : places laterales 0, +58, -58, +116, -116 (BRMissionWorld.cpp)
		const float S = 350.f, Wall = 60.f;
		const float FaceHalf = S * 0.5f - Wall * 0.5f;
		const float Laterals[5] = { 0.f, 58.f, -58.f, 116.f, -116.f };
		CL::FSlot Slots[5];
		for (int K = 0; K < 5; ++K)
		{
			// Mur au nord (Y = 145 depuis le centre), mecanisme tourne vers le sud (lacet -90 : +X local vers -Y, +Y local
			// vers +X). v4.15 (correctifs) : decalage lateral le long du Y local, comme LateralOf du placement
			Slots[K].Frame.X = Laterals[K];
			Slots[K].Frame.Y = FaceHalf;
			Slots[K].Frame.Z = 0.f;
			Slots[K].Frame.Yaw = -90.f;
			Slots[K].Lateral = Laterals[K];
		}
		auto At = [&](int K, const CL::FFootprint& Print)
		{
			CL::FPlaced P0;
			P0.Frame = Slots[K].Frame;
			P0.Print = Print;
			return P0;
		};
		const CL::FPlaced Cage0 = At(0, Cage);
		Line("v4.14 : poste a 0, cage a +58 : point d'interaction du poste dans la cage (actions toujours refusees)",
			CL::Clash(At(0, Post), At(1, Cage)) && CL::InSolid(At(1, Cage), CL::AimPoint(At(0, Post)).X, CL::AimPoint(At(0, Post)).Y, CL::AimPoint(At(0, Post)).Z, 0.f));
		Line("poste a +-58 d'une cage : refuse ; a +-116 : libre (" + F1(116.f - LC::HalfWidth) + " cm entre le poste et la cage)",
			CL::Clash(At(1, Post), Cage0) && CL::Clash(At(2, Post), Cage0) && !CL::Clash(At(3, Post), Cage0) && !CL::Clash(At(4, Post), Cage0));
		// PickSlot : la cage d'abord, au bout de sa face ou elle tient (v4.15 correctifs), puis le poste a cote d'elle, hors de
		// son volume, a sa place ordinaire
		const int CageSlot = CL::PickSlot(Cage, Slots, 5, nullptr, 0, FaceHalf, true);
		int PostSlot = -1;
		if (CageSlot >= 0)
		{
			Slots[CageSlot].bOpen = false;
			const CL::FPlaced Placed[1] = { At(CageSlot, Cage) };
			PostSlot = CL::PickSlot(Post, Slots, 5, Placed, 1, FaceHalf, true);
			Slots[CageSlot].bOpen = true;
		}
		Line("placement v4.15 : cage a " + F1(CageSlot >= 0 ? Laterals[CageSlot] : -1.f) + ", poste a " + F1(PostSlot >= 0 ? Laterals[PostSlot] : -1.f) +
			" (meme face, la cage au bout de sa face, comme diag_v413_4)", CageSlot == 1 && PostSlot == 2);
		if (CageSlot >= 0 && PostSlot >= 0)
		{
			// Place ordinaire du poste (110 cm devant lui) : capsule libre du mur voisin et de la cage
			const float PostLat = Laterals[PostSlot];
			const float CageLat = Laterals[CageSlot];
			const float ToWall = FaceHalf - std::fabs(PostLat);
			const float ToCage = std::fabs(PostLat - CageLat) - LC::HalfWidth;
			Line("place ordinaire du poste : " + F1(ToWall) + " cm du mur voisin, " + F1(ToCage) + " cm de la cage (capsule de " + F1(CL::PlayerRadius) + " cm libre ; poste a +-116 : " +
				F1(FaceHalf - 116.f) + " cm du mur)", ToWall >= CL::StandGap && ToCage >= CL::StandGap);
		}
		// Bout de face pris : l'autre bout ; aucun bout libre : le centre
		Slots[1].bOpen = false;
		const int CageOther = CL::PickSlot(Cage, Slots, 5, nullptr, 0, FaceHalf, true);
		Slots[2].bOpen = false;
		const int CageMid = CL::PickSlot(Cage, Slots, 5, nullptr, 0, FaceHalf, true);
		Slots[1].bOpen = Slots[2].bOpen = true;
		Line("cage, place +58 prise : a " + F1(CageOther >= 0 ? Laterals[CageOther] : -1.f) + " ; +58 et -58 prises : au centre", CageOther == 2 && CageMid == 0);
		CL::FSlot Edge[2] = { Slots[3], Slots[4] };
		Edge[0].bOpen = Edge[1].bOpen = true;
		Line("cage a +-116 : refusee (elle entrerait de " + F1(116.f + LC::HalfWidth - FaceHalf) + " cm dans le mur voisin)", CL::PickSlot(Cage, Edge, 2, nullptr, 0, FaceHalf, true) == -1);
		// Sans volume : premiere place libre (placement des autres niveaux inchange)
		const CL::FFootprint Winch = CL::Of(BRM::EKind::Crank, BRM::R_Winch, BRMech::Module::None, 60.f);
		CL::FSlot Free[5];
		for (int K = 0; K < 5; ++K)
		{
			Free[K] = Slots[K];
			Free[K].bOpen = K != 1;
		}
		// v4.15 (correctifs 3) : voisins loin du couloir de visee (bouts de la face) ; un voisin pose a la meme place prendrait
		// la visee (couloirs, plus bas)
		const CL::FPlaced Two[2] = { At(3, Winch), At(4, Post) };
		Line("sans volume ni voisin a volume : premiere place libre (ordre d'avant la v4.15)", CL::PickSlot(Winch, Free, 5, Two, 2, FaceHalf, true) == 0
			&& CL::PickSlot(Winch, Free, 5, Two, 2, FaceHalf, false) == 0);
		// Separation des axes : rectangles tournes
		CL::FRect A;
		A.HX = 100.f;
		A.HY = 20.f;
		CL::FRect B = A;
		B.CX = 0.f;
		B.CY = 90.f;
		B.Yaw = 90.f;
		CL::FRect Cc = A;
		Cc.CY = 70.f;
		Cc.Yaw = 45.f;
		CL::FRect Far = A;
		Far.CY = 160.f;
		Far.Yaw = 30.f;
		Line("rectangles orientes : croisement, rotation de 45 degres, ecart", CL::Overlap(A, B) && CL::Overlap(A, Cc) && !CL::Overlap(A, Far));
		// Zone libre devant un treuil : le point vise, la place debout devant lui, la largeur de la capsule
		const CL::FPlaced WinchAt = At(0, Winch);
		const CL::FRect K0 = CL::KeepOut(WinchAt);
		const BRMech::FVec3 Aim = CL::AimPoint(WinchAt);
		CL::FRect Probe;
		// Un point 1,15 m devant le point vise (vers la salle, -Y ici) est dans la zone libre
		Probe.CX = Aim.X;
		Probe.CY = Aim.Y - (CL::StandDepth - 5.f);
		Probe.HX = Probe.HY = 1.f;
		// Rectangle tourne d'un quart de tour (mur au nord) : la profondeur est sur Y
		const float Depth = K0.HX > K0.HY ? K0.HX * 2.f : K0.HY * 2.f;
		Line("zone libre d'un treuil : " + F1(Depth) + " cm depuis 12 cm derriere le mur, +-" + F1(K0.HX < K0.HY ? K0.HX : K0.HY) + " cm (capsule debout a " +
			F1(CL::StandDepth) + " cm devant le point vise comprise)",
			CL::Overlap(K0, Probe) && Depth >= 12.f + Winch.Aim.X + CL::StandDepth + CL::StandGap - 0.01f);
		// v4.15 (correctifs) : zone libre d'une sortie de la mission (rochers du Level 8) : la capsule qui monte a l'echelle
		// (ABRExit::GetClimbAnchor : 52 cm devant le pied) et la place devant elle (StandDepth) y sont entieres
		{
			BRMech::FFrame ExitF;
			ExitF.Y = FaceHalf;
			ExitF.Yaw = -90.f;
			const CL::FRect KE = CL::ExitKeepOut(ExitF);
			constexpr float ClimbAnchor = 52.f;
			// HX : demi-profondeur (X local, depuis 12 cm dans le mur), HY : demi-largeur (RectOf)
			const float HalfWide = KE.HY;
			const float Front = 2.f * KE.HX - 12.f;
			CL::FRect Climb;
			Climb.CX = 0.f;
			Climb.CY = FaceHalf - ClimbAnchor;
			Climb.HX = Climb.HY = CL::PlayerRadius;
			Line("zone libre d'une sortie : de 12 cm dans le mur a " + F1(Front) + " cm devant le pied, +-" + F1(HalfWide) + " cm (capsule de l'echelle a " + F1(ClimbAnchor) +
				" cm et place debout a " + F1(CL::StandDepth) + " cm comprises)",
				CL::Overlap(KE, Climb) && Front >= ClimbAnchor + CL::PlayerRadius && Front >= CL::StandDepth + CL::StandGap && HalfWide >= CL::StandGap);
		}
		// Toutes les sortes et tous les roles : point vise devant le mur, a hauteur de main ou de plaque
		bool bAll = true;
		for (int Kind = 0; Kind <= static_cast<int>(BRM::EKind::Gate); ++Kind)
		{
			for (int Role = 0; Role < BRM::R_Count; ++Role)
			{
				const CL::FFootprint Fp = CL::Of(static_cast<BRM::EKind>(Kind), static_cast<uint8_t>(Role), BRMech::Module::None, 60.f);
				bAll = bAll && Fp.Aim.X > 0.f && Fp.Aim.X <= 130.f && Fp.Aim.Z >= 40.f && Fp.Aim.Z <= 170.f && Fp.AimExtent.X > 0.f;
			}
		}
		Line("emprise de chaque sorte et role : point vise devant l'origine, entre 40 et 170 cm de haut", bAll);
		// v4.15 (correctifs 3) : la sonde des tests du jeu (ABRWorld::FindMissionDeviceObstruction, rayon AimProbe) ne touche
		// pas le mur qui porte un mecanisme : son point vise est a au moins AimProbe + 2 cm de l'origine murale (avant : sonde
		// de 5 cm, tangente au mur des indices, cles et mire a 5 cm, comptee comme un contact par Chaos)
		float MinAimX = 1e9f;
		for (int Kind = 0; Kind <= static_cast<int>(BRM::EKind::Gate); ++Kind)
		{
			for (int Role = 0; Role < BRM::R_Count; ++Role)
			{
				for (const uint8_t ModuleId : { BRMech::Module::None, BRMech::Module::LidarDoor, BRMech::Module::PoolTanks })
				{
					const CL::FFootprint Fp = CL::Of(static_cast<BRM::EKind>(Kind), static_cast<uint8_t>(Role), ModuleId, 60.f);
					MinAimX = Fp.Aim.X < MinAimX ? Fp.Aim.X : MinAimX;
				}
			}
		}
		Line("sonde du point d'interaction (" + F1(CL::AimProbe) + " cm) : plus petit ecart point vise - mur " + F1(MinAimX) + " cm, au moins sonde + 2 cm (sonde de 5 cm "
			"d'avant : jusqu'au mur)", MinAimX - CL::AimProbe >= 2.f && MinAimX <= 5.f);
		const CL::FFootprint Door = CL::Of(BRM::EKind::Button, BRM::R_LidarDoor, BRMech::Module::LidarDoor, 60.f);
		const CL::FFootprint Lock = CL::Of(BRM::EKind::Gate, BRM::R_DryPassage, BRMech::Module::PoolLock, 120.f);
		const CL::FFootprint BridgeF = CL::Of(BRM::EKind::Gate, BRM::R_Bridge, BRMech::Module::Bridge, 60.f);
		Line("modules : battant de l'aile (" + F1(Door.Solid.X1 - Door.Solid.X0) + " cm, mur de 60), sas (" + F1(Lock.Solid.X1 - Lock.Solid.X0) + " cm), passerelle (" +
			F1(BridgeF.Solid.X1 - BridgeF.Solid.X0) + " cm)", Door.bSolid && Lock.bSolid && BridgeF.bSolid && BridgeF.Solid.X1 - BridgeF.Solid.X0 == BB::TotalLength);
	}

	// v4.15 (correctifs 3) : couloirs de visee (BRMech::Clearance::CutsFocusLane, regle de PickSlot) : le groupe du moulin du
	// Niveau 10 (graine 1697132605, diag3/level10_focus), les poteaux en rang, les voisins muraux et le Level 6 inchanges
	std::printf("\nH. Couloirs de visee des mecanismes (v4.15 correctifs 3)\n");
	{
		namespace CL = BRMech::Clearance;
		const CL::FFootprint Board = CL::Of(BRM::EKind::Clue, BRM::R_BarnBoard, BRMech::Module::None, 60.f);
		const CL::FFootprint Dial = CL::Of(BRM::EKind::Switch, BRM::R_MillDial, BRMech::Module::None, 60.f);
		const CL::FFootprint Brake = CL::Of(BRM::EKind::Button, BRM::R_MillBrake, BRMech::Module::None, 60.f);
		// ABRWorld::PlaceMission, PostSpot : poteau K d'une cellule (decalage Lateral(K) x 1,2, recule de 0,2 S), lacet donne
		constexpr float S10 = 400.f;
		auto Post = [](int CX, int CY, int K, float Yaw, const CL::FFootprint& Print)
		{
			const float R = Yaw * 3.14159265f / 180.f;
			const float FX = std::round(std::cos(R)), FY = std::round(std::sin(R));
			const int Step = (K + 1) / 2;
			const float Lat = (K % 2 == 1 ? 1.f : -1.f) * static_cast<float>(Step) * 58.f * 1.2f;
			CL::FPlaced P0;
			P0.Frame.X = (static_cast<float>(CX) + 0.5f) * S10 - FY * Lat - FX * S10 * 0.2f;
			P0.Frame.Y = (static_cast<float>(CY) + 0.5f) * S10 + FX * Lat - FY * S10 * 0.2f;
			P0.Frame.Yaw = Yaw;
			P0.Print = Print;
			return P0;
		};
		// Graine 1697132605, cellule (-1, -14) : lacets tires par poteau (sels 77, 78, 79) : 270, 0, 180 (journal du jeu :
		// moulin en (-274, -5330, 140), frein en (-126, -5330, 140))
		const CL::FPlaced OldBoard = Post(-1, -14, 0, 270.f, Board), OldDial = Post(-1, -14, 1, 0.f, Dial), OldBrake = Post(-1, -14, 2, 180.f, Brake);
		const BRMech::FVec3 DialAim = CL::AimPoint(OldDial), BrakeAim = CL::AimPoint(OldBrake);
		Line("Niveau 10, placement d'avant : moulin en (" + F1(DialAim.X) + ", " + F1(DialAim.Y) + ", " + F1(DialAim.Z) + "), frein en (" + F1(BrakeAim.X) + ", " + F1(BrakeAim.Y) +
			") (points du journal du jeu a 0,4 cm pres)", std::fabs(DialAim.X + 274.f) < 1.f && std::fabs(DialAim.Y + 5330.f) < 1.f && std::fabs(BrakeAim.X + 126.f) < 1.f
			&& std::fabs(BrakeAim.Y + 5330.f) < 1.f && DialAim.Z == 140.f);
		Line("Niveau 10, placement d'avant : le tableau des granges coupe le couloir du moulin et celui du frein (visee prise, PAS VISE)",
			CL::CutsFocusLane(OldDial, OldBoard) && CL::CutsFocusLane(OldBrake, OldBoard) && !CL::CutsFocusLane(OldBoard, OldDial) && !CL::CutsFocusLane(OldBoard, OldBrake));
		// Poteaux en rang (un lacet par cellule) : aucun couloir coupe, quel que soit le lacet et l'ordre des roles
		const CL::FFootprint Roles[3] = { Board, Dial, Brake };
		const int Orders[6][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
		int RowCuts = 0;
		for (const float Yaw : { 0.f, 90.f, 180.f, 270.f })
		{
			for (const auto& O : Orders)
			{
				CL::FPlaced Row[3];
				for (int K = 0; K < 3; ++K)
				{
					Row[K] = Post(-1, -14, K, Yaw, Roles[O[K]]);
				}
				for (int A = 0; A < 3; ++A)
				{
					for (int B = A + 1; B < 3; ++B)
					{
						RowCuts += CL::FocusClash(Row[A], Row[B]) ? 1 : 0;
					}
				}
			}
		}
		Line("poteaux d'une cellule en rang (4 lacets x 6 ordres des roles) : aucun couloir de visee coupe (" + std::to_string(RowCuts) + ")", RowCuts == 0);
		// Lacets tires par poteau (placement d'avant) : combinaisons ou un couloir est coupe
		int Combos = 0;
		for (int Y0 = 0; Y0 < 4; ++Y0)
		{
			for (int Y1 = 0; Y1 < 4; ++Y1)
			{
				for (int Y2 = 0; Y2 < 4; ++Y2)
				{
					const CL::FPlaced Pb = Post(-1, -14, 0, 90.f * static_cast<float>(Y0), Board), Pd = Post(-1, -14, 1, 90.f * static_cast<float>(Y1), Dial),
						Pk = Post(-1, -14, 2, 90.f * static_cast<float>(Y2), Brake);
					Combos += (CL::FocusClash(Pb, Pd) || CL::FocusClash(Pb, Pk) || CL::FocusClash(Pd, Pk)) ? 1 : 0;
				}
			}
		}
		Line("lacets tires par poteau (placement d'avant) : " + std::to_string(Combos) + " combinaisons sur 64 avec un couloir coupe (rang : aucune)", Combos > 0);
		// PickSlot : le tableau pose, le moulin ne prend plus le poteau dont le couloir passe par lui ; sans place degagee, le
		// choix d'avant (le placement n'echoue pas plus qu'avant)
		CL::FSlot Posts[4];
		const CL::FPlaced Ref[4] = { OldBoard, Post(-1, -14, 1, 0.f, Dial), Post(-1, -14, 2, 180.f, Dial), Post(0, -14, 0, 90.f, Dial) };
		for (int K = 0; K < 4; ++K)
		{
			Posts[K].Frame = Ref[K].Frame;
			Posts[K].bWall = false;
			Posts[K].bOpen = K != 0;
		}
		const CL::FPlaced Placed1[1] = { OldBoard };
		const int DialNew = CL::PickSlot(Dial, Posts, 4, Placed1, 1, 200.f, true);
		const int DialOld = CL::PickSlot(Dial, Posts, 4, Placed1, 1, 200.f, false);
		Posts[3].bOpen = false;
		const int DialOnly = CL::PickSlot(Dial, Posts, 3, Placed1, 1, 200.f, true);
		Line("PickSlot (moulin apres le tableau) : poteau " + std::to_string(DialNew) + " au couloir degage (avant : " + std::to_string(DialOld) + ") ; sans poteau degage : " +
			std::to_string(DialOnly) + " (le choix d'avant, pas d'echec)", DialNew == 3 && DialOld == 1 && DialOnly == 1);
		// Voisins muraux a 58 cm (meme face) : couloirs degages pour les boites les plus larges des groupes ; Level 6 : poste a
		// -58 et cage a +58 (correctifs) ; porte ou passage de la mission : jamais vise
		auto Wall = [](float Lat, const CL::FFootprint& Print)
		{
			CL::FPlaced P0;
			P0.Frame.X = Lat;
			P0.Frame.Y = 145.f;
			P0.Frame.Yaw = -90.f;
			P0.Print = Print;
			return P0;
		};
		const CL::FFootprint Wide[4] = { Dial, Board, CL::Of(BRM::EKind::Switch, BRM::R_Sluice, BRMech::Module::None, 60.f), CL::Of(BRM::EKind::Switch, BRM::R_CodeDial, BRMech::Module::None, 60.f) };
		bool bSide = true;
		for (const CL::FFootprint& A0 : Wide)
		{
			for (const CL::FFootprint& B0 : Wide)
			{
				bSide = bSide && !CL::FocusClash(Wall(0.f, A0), Wall(58.f, B0));
			}
		}
		const CL::FFootprint Cage = CL::Of(BRM::EKind::Observe, BRM::R_LidarCage, BRMech::Module::None, 60.f);
		const CL::FFootprint LPost = CL::Of(BRM::EKind::Crank, BRM::R_LidarPost, BRMech::Module::None, 60.f);
		const CL::FFootprint Gate = CL::Of(BRM::EKind::Gate, BRM::R_ReserveLights, BRMech::Module::None, 60.f);
		Line("voisins muraux a 58 cm (moulin, tableau, vanne, cadran) : couloirs degages ; Level 6 : poste a -58, cage a +58 : degages", bSide
			&& !CL::FocusClash(Wall(-58.f, LPost), Wall(58.f, Cage)));
		Line("porte ou passage de la mission (boite sans collision) : ne prend pas la visee, n'a pas de couloir", !Gate.bAimable && !CL::FocusClash(Wall(0.f, Gate), Wall(0.f, Board))
			&& CL::FocusClash(Wall(0.f, Board), Wall(0.f, Dial)));
		// v4.15 (correctifs 3, relecture) : Poolrooms : vanne B fixee sur l'avant des bassins de la vanne A (ABRWorld::PlaceMission,
		// module pose apres PickSlot : vanne A avancee de TankDepth, vanne B a WheelLateralB sur le cote, meme lacet). Son point
		// vise est a 8 cm du volume des bassins (CutsFocusLane le compte), sa boite visee 20 cm devant eux (la visee la prend
		// d'abord) : l'invariant du placement ne journalise rien (LaneCutLogged) ; il journalise toujours le Niveau 10
		int PoolCounted = 0, PoolLogged = 0, PoolInside = 0;
		float PoolFront = 1.0e6f;
		for (const float Yaw : { 0.f, 90.f, 180.f, 270.f })
		{
			const float R = Yaw * 3.14159265f / 180.f;
			const float FX = std::round(std::cos(R)), FY = std::round(std::sin(R));
			CL::FPlaced TanksA;
			TanksA.Frame.X = 500.f + FX * P::TankDepth;
			TanksA.Frame.Y = -300.f + FY * P::TankDepth;
			TanksA.Frame.Yaw = Yaw;
			TanksA.Print = CL::Of(BRM::EKind::Switch, BRM::R_Sluice, BRMech::Module::PoolTanks, 60.f);
			CL::FPlaced WheelB;
			WheelB.Frame = TanksA.Frame;
			TanksA.Frame.ToWorld(0.f, P::WheelLateralB, WheelB.Frame.X, WheelB.Frame.Y);
			WheelB.Print = CL::Of(BRM::EKind::Switch, BRM::R_Sluice, BRMech::Module::PoolWheelB, 60.f);
			const BRMech::FVec3 BAim = CL::AimPoint(WheelB);
			PoolCounted += CL::CutsFocusLane(WheelB, TanksA) ? 1 : 0;
			PoolLogged += (CL::LaneCutLogged(WheelB, BRMech::Module::PoolWheelB, TanksA, BRMech::Module::PoolTanks)
				|| CL::LaneCutLogged(TanksA, BRMech::Module::PoolTanks, WheelB, BRMech::Module::PoolWheelB)) ? 1 : 0;
			PoolInside += (CL::InSolid(TanksA, BAim.X, BAim.Y, BAim.Z, CL::Margin) || CL::Clash(TanksA, WheelB)) ? 1 : 0;
			// Avant de la boite visee de la vanne B, dans le repere des bassins (meme lacet), devant leur face avant
			float LX = 0.f, LY = 0.f;
			TanksA.Frame.ToLocal(BAim.X, BAim.Y, LX, LY);
			const float Front = LX + WheelB.Print.AimExtent.X - TanksA.Print.Solid.X1;
			PoolFront = Front < PoolFront ? Front : PoolFront;
		}
		Line("Poolrooms, vanne B sur l'avant des bassins (4 lacets) : couloir compte " + std::to_string(PoolCounted) + "/4 par CutsFocusLane, journalise " +
			std::to_string(PoolLogged) + " (attendu 0), boite visee " + F1(PoolFront) + " cm devant les bassins ; Niveau 10 toujours journalise",
			PoolLogged == 0 && PoolInside == 0 && PoolFront > CL::FocusSweep + CL::Margin
			&& CL::LaneCutLogged(OldDial, BRMech::Module::None, OldBoard, BRMech::Module::None) && CL::LaneCutLogged(OldBrake, BRMech::Module::None, OldBoard, BRMech::Module::None));
	}

	std::printf("\nRESULTAT : %s (%d verifications, %d echec(s))\n", GFailures == 0 ? "OK" : "ECHEC", GChecks, GFailures);
	return GFailures == 0 ? 0 : 1;
}
