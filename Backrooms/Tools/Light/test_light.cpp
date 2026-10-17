// v4.12 : banc hors moteur de la lumiere de gameplay (constat 4.4).
//
// Il lit les constantes et la loi d'apport de BRLightLogic.h (celles du jeu) et verifie ce qui est annonce aux joueurs :
// la portee a laquelle une balise allumee dissipe un Smiler hors poursuite (indice du carnet, Niveau 6), celle ou plus
// aucune entite de l'ombre n'apparait, la sortie eclairee, deux balises proches, l'independance vis-a-vis des
// coupures (les sources de mission ne sont pas multipliees par la puissance des neons). L'occlusion par les murs et la
// replication sont verifiees en jeu (-BRAutoTestV412, prepare).
//
// v4.16 : lumiere de gameplay alignee sur la lumiere visible (prompt v4.16, 3.1) :
//   A. alimentation d'une coupure vue par les regles (BRLight::PhasePower) ;
//   B. scenes de controle de l'occlusion des plafonniers (BRLight::Occlusion, le code du jeu) : meme point avec et sans
//      mur, passage (ouverture, jambage, linteau selon la hauteur, mur epais traverse en biais), arche des Poolrooms,
//      bout de mur au sommet, pilier, cellule pleine, applique de l'hotel, plafond, sol perce d'une fosse, point colle a un
//      mur, loi d'apport inchangee quand rien ne cache la lampe ;
//   C. equilibrage : la grille des niveaux 0, 1, 2, 3, 4, 5, 8 et 37 rejouee hors moteur (regles d'ABRWorld : aretes,
//      zones, bureaux, labyrinthe, hotel, grottes, piliers, plafonniers, salles de fosses et portes percees du Level 0 ;
//      parametres lus dans Source/Backrooms/Private/BRLevels.cpp), sur des centaines de graines : part des positions dont la
//      lumiere change, apparitions d'entites de l'ombre, dissipation d'un Smiler, Deathmoth, Bacteria, sante mentale ;
//   D. empreintes de la grille de quelques graines, que -BRAutoTestV416Light recalcule dans Unreal (meme portage ?).
// Le banc s'execute depuis Backrooms/ (il lit BRLevels.cpp).
//
//   g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wshadow -I Source/Backrooms/Public Tools/Light/test_light.cpp -o test_light
//   clang-cl /nologo /std:c++17 /O2 /EHsc /utf-8 /W4 /WX -Wshadow -Wno-c++98-compat /I Source\Backrooms\Public Tools\Light\test_light.cpp
//   (ou python Tools/run_benches.py --bench light : gcc, clang-cl et MSVC, sorties identiques exigees)
#include "BRLightLogic.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
	int GChecks = 0, GFailures = 0;
	void Line(const char* What, bool bOk)
	{
		++GChecks;
		GFailures += bOk ? 0 : 1;
		std::printf("  %-100s %s\n", What, bOk ? "OK" : "ECHEC");
	}

	// Portee (cm) au-dela de laquelle l'apport d'une source passe sous un seuil
	float Reach(float Radius, float Intensity, float Threshold)
	{
		return Intensity <= Threshold ? 0.f : Radius * (1.f - std::sqrt(Threshold / Intensity));
	}

	// Recherche numerique de la meme portee avec la loi du jeu (aucune formule recopiee)
	float ReachByScan(float Radius, float Intensity, float Threshold)
	{
		float Last = 0.f;
		for (float D = 0.f; D <= Radius; D += 0.5f)
		{
			if (BRLight::Contribution(D, Radius, Intensity) > Threshold)
			{
				Last = D;
			}
		}
		return Last;
	}

	using BRLight::Occlusion::FGeom;
	using BRLight::Occlusion::FLamp;
	using BRLight::Occlusion::FVec;
	typedef std::pair<int, int> FCellKey;

	FVec V3(double X, double Y, double Z)
	{
		FVec V;
		V.X = X;
		V.Y = Y;
		V.Z = Z;
		return V;
	}

	// ------------------------------------------------------------------------------------------------ grille de controle
	/** Grille ecrite a la main pour les scenes de controle (meme interface que la grille du monde) */
	struct FTestGrid
	{
		double S = 350.0;
		std::map<FCellKey, unsigned char> E;
		std::map<FCellKey, unsigned char> N;
		std::set<FCellKey> Solid;
		std::set<FCellKey> Pillars;
		std::map<FCellKey, FLamp> Lamps;
		std::set<FCellKey> Holes;
		double HoleHalf = 100.0;

		unsigned char EdgeE(int X, int Y) const
		{
			const auto It = E.find(FCellKey(X, Y));
			return It == E.end() ? BRLight::Occlusion::EdgeOpen : It->second;
		}
		unsigned char EdgeN(int X, int Y) const
		{
			const auto It = N.find(FCellKey(X, Y));
			return It == N.end() ? BRLight::Occlusion::EdgeOpen : It->second;
		}
		bool SolidBox(int X, int Y, double Lo[3], double Hi[3]) const
		{
			if (!Solid.count(FCellKey(X, Y)))
			{
				return false;
			}
			Lo[0] = X * S;
			Lo[1] = Y * S;
			Lo[2] = -BRLight::Occlusion::Huge;
			Hi[0] = (X + 1) * S;
			Hi[1] = (Y + 1) * S;
			Hi[2] = BRLight::Occlusion::Huge;
			return true;
		}
		bool Pillar(int X, int Y) const { return Pillars.count(FCellKey(X, Y)) != 0; }
		bool FloorOpen(double X, double Y) const
		{
			const int GX = static_cast<int>(std::floor(X / S + 0.5));
			const int GY = static_cast<int>(std::floor(Y / S + 0.5));
			return std::fabs(X - GX * S) < HoleHalf && std::fabs(Y - GY * S) < HoleHalf && Holes.count(FCellKey(GX - 1, GY - 1)) != 0;
		}
		bool Lamp(int X, int Y, FLamp& Out) const
		{
			const auto It = Lamps.find(FCellKey(X, Y));
			if (It == Lamps.end())
			{
				return false;
			}
			Out = It->second;
			return Out.bOn;
		}
		/** Plafonnier au centre de la cellule (+ decalage), point d'emission selon le luminaire */
		void AddLamp(int X, int Y, int Fixture, double Height, double OffX = 0.0, double OffY = 0.0, bool bFlicker = false)
		{
			FLamp L;
			L.bOn = true;
			L.bFlicker = bFlicker;
			L.AX = (X + 0.5) * S + OffX;
			L.AY = (Y + 0.5) * S + OffY;
			BRLight::Occlusion::Emitter(Fixture, Height, L.AX, L.AY, OffX, OffY, L.Emitter);
			Lamps[FCellKey(X, Y)] = L;
		}
	};

	bool Sees(const FTestGrid& G, const FGeom& Geo, const FVec& From, const FVec& To)
	{
		return BRLight::Occlusion::Visible(G, Geo, From, To);
	}

	FGeom Level0Geom()
	{
		// Level 0 : cellule 350, mur 60 x 290, passage 150, linteau 225, piliers 45
		return BRLight::Occlusion::MakeGeom(350.0, 60.0, 290.0, 150.0, true, false, 0.0, 45.0, true, true);
	}

	void SectionPower()
	{
		using namespace BRLight;
		std::printf("\n## A. Alimentation d'une coupure vue par les regles (BRLight::PhasePower)\n\n");
		char Buf[200];
		const float F0 = PhasePower(PhaseFailing, FailSeconds);
		const float FH = PhasePower(PhaseFailing, FailSeconds * 0.5f);
		const float F1 = PhasePower(PhaseFailing, 0.f);
		std::snprintf(Buf, sizeof(Buf), "defaillance : %.2f au debut, %.2f a mi-chemin, %.2f a la fin (fondu regulier de %.1f s)", F0, FH, F1, FailSeconds);
		Line(Buf, F0 == 1.f && std::fabs(FH - 0.5f) < 1e-6f && F1 == 0.f);
		const float R0 = PhasePower(PhaseRestoring, RestoreSeconds);
		const float RH = PhasePower(PhaseRestoring, RestoreSeconds * 0.5f);
		const float R1 = PhasePower(PhaseRestoring, 0.f);
		std::snprintf(Buf, sizeof(Buf), "retour du courant : %.2f au debut, %.2f a mi-chemin, %.2f a la fin (%.1f s)", R0, RH, R1, RestoreSeconds);
		Line(Buf, R0 == 0.f && std::fabs(RH - 0.5f) < 1e-6f && R1 == 1.f);
		Line("noir : 0 pendant toute la phase ; hors coupure : 1 (quel que soit le minuteur)",
			PhasePower(PhaseDark, 30.f) == 0.f && PhasePower(PhaseDark, 0.f) == 0.f && PhasePower(PhaseNone, 120.f) == 1.f && PhasePower(PhaseNone, -5.f) == 1.f);
		// Monotone : la lumiere ne peut que baisser pendant la defaillance et que monter pendant le retour
		bool bMono = true;
		float PrevF = 2.f;
		float PrevR = -1.f;
		for (int I = 0; I <= 36; ++I)
		{
			const float Left = FailSeconds - I * 0.05f;
			const float F = PhasePower(PhaseFailing, Left);
			const float R = PhasePower(PhaseRestoring, RestoreSeconds - I * 0.05f);
			bMono = bMono && F <= PrevF && R >= PrevR;
			PrevF = F;
			PrevR = R;
		}
		Line("monotone : baisse pendant la defaillance, hausse pendant le retour (pas de 50 ms)", bMono);
		Line("la loi ne lit ni reglage FLASHS, ni profil graphique, ni tirage : deux appels identiques donnent le meme resultat",
			PhasePower(PhaseFailing, 0.7f) == PhasePower(PhaseFailing, 0.7f) && PhasePower(PhaseRestoring, 0.3f) == PhasePower(PhaseRestoring, 0.3f));
		Line("etat lisible (inventaire) : noir sous 0,12, penombre, eclaire au-dessus de 0,50",
			Band(0.11f) == 0 && Band(0.12f) == 1 && Band(0.5f) == 1 && Band(0.51f) == 2);
	}

	void SectionScenes()
	{
		using namespace BRLight::Occlusion;
		std::printf("\n## B. Scenes de controle de l'occlusion des plafonniers (BRLight::Occlusion)\n\n");
		const FGeom G0 = Level0Geom();
		char Buf[220];
		{
			std::snprintf(Buf, sizeof(Buf), "cotes du Level 0 : passage %.0f cm, linteau a %.0f cm, mur %.0f cm, pilier %.0f cm, arche %s", G0.DoorHalf * 2.0, G0.DoorTop,
				G0.Thick, G0.PillarHalf * 2.0, G0.bArch ? "oui" : "non");
			Line(Buf, G0.DoorHalf == 75.0 && G0.DoorTop == 225.0 && G0.PillarHalf == 22.5 && !G0.bArch);
		}
		// 1. Meme point, avec et sans mur : lampe au centre de la cellule (1, 0), point a 40 cm de la face du mur dans (0, 0)
		{
			FTestGrid T;
			T.AddLamp(1, 0, 1, G0.Height);
			const FVec Lamp = T.Lamps[FCellKey(1, 0)].Emitter;
			const FVec Smiler = V3(280.0, 175.0, 165.0);
			const FVec Player = V3(280.0, 175.0, 92.0);
			float AllOpen = 0.f, VisOpen = 0.f, AllWall = 0.f, VisWall = 0.f;
			CeilingSum(T, G0, 620.f, Smiler, AllOpen, VisOpen);
			T.E[FCellKey(0, 0)] = EdgeWall;
			CeilingSum(T, G0, 620.f, Smiler, AllWall, VisWall);
			std::snprintf(Buf, sizeof(Buf), "meme point sans mur : %.3f compte (regle d'avant %.3f) ; avec un mur : %.3f (regle d'avant %.3f)", VisOpen, AllOpen, VisWall, AllWall);
			Line(Buf, VisOpen == AllOpen && AllOpen > 0.3f && VisWall == 0.f && AllWall == AllOpen);
			Line("avec le mur : ni le Smiler (1,65 m) ni le joueur (0,92 m) ne voient la lampe", !Sees(T, G0, Lamp, Smiler) && !Sees(T, G0, Lamp, Player));
			// Point colle au mur ou pris dans son epaisseur : cote lampe, il est eclaire comme la face qu'il touche ; de l'autre cote, non
			Line("point pris a 5 cm dans le mur, cote lampe : eclaire (petite sphere) ; a 55 cm (cote oppose) : non",
				Sees(T, G0, Lamp, V3(375.0, 175.0, 120.0)) && !Sees(T, G0, Lamp, V3(325.0, 175.0, 120.0)));
		}
		// 2. Passage (arete Door) : ouverture de 150 cm centree, linteau a 225 cm
		{
			FTestGrid T;
			T.AddLamp(1, 0, 1, G0.Height);
			T.E[FCellKey(0, 0)] = EdgeDoor;
			const FVec Lamp = T.Lamps[FCellKey(1, 0)].Emitter;
			Line("passage : point dans l'axe a 40 cm du mur (joueur, Smiler) : eclaire", Sees(T, G0, Lamp, V3(280.0, 175.0, 92.0)) && Sees(T, G0, Lamp, V3(280.0, 175.0, 165.0)));
			Line("passage : point derriere le jambage (150 cm a cote de l'axe) : cache", !Sees(T, G0, Lamp, V3(280.0, 325.0, 92.0)));
			// Hauteur : loin du passage, le rayon vers un Smiler (1,65 m) passe au-dessus du linteau, celui vers le joueur dessous
			Line("passage : a 3 m du mur, le joueur (0,92 m) voit la lampe, le Smiler (1,65 m) non : le linteau la cache",
				Sees(T, G0, Lamp, V3(50.0, 175.0, 92.0)) && !Sees(T, G0, Lamp, V3(50.0, 175.0, 165.0)));
			// Mur epais : le rayon coupe la ligne de l'arete dans l'ouverture (y = 245 < 250) mais touche le jambage dans
			// l'epaisseur du mur (y = 257 sur la face opposee) : un modele de mur sans epaisseur le croirait eclaire
			Line("passage : rayon en biais dans l'ouverture sur la ligne de l'arete mais dans le jambage sur la face : cache",
				!Sees(T, G0, Lamp, V3(280.0, 273.0, 92.0)) && Sees(T, G0, Lamp, V3(280.0, 240.0, 92.0)));
			// Sans linteau (Levels 2, 3, 8) : le Smiler voit la lampe a 3 m
			const FGeom NoLintel = BRLight::Occlusion::MakeGeom(350.0, 60.0, 290.0, 150.0, false, false, 0.0, 0.0, false, true);
			Line("passage sans linteau : le Smiler a 3 m voit la lampe", Sees(T, NoLintel, Lamp, V3(50.0, 175.0, 165.0)));
		}
		// 3. Arche des Poolrooms (cellule 500, mur 120, hauteur 450, passage 220 : naissance a 280, sommet a 390)
		{
			const FGeom G37 = BRLight::Occlusion::MakeGeom(500.0, 120.0, 450.0, 220.0, true, true, 0.0, 90.0, true, true);
			std::snprintf(Buf, sizeof(Buf), "cotes des Poolrooms : arche %s, naissance a %.0f cm, rayon %.0f cm", G37.bArch ? "oui" : "non", G37.ArchSpring, G37.DoorHalf);
			Line(Buf, G37.bArch && G37.ArchSpring == 280.0 && G37.DoorHalf == 110.0);
			FTestGrid T;
			T.S = 500.0;
			T.AddLamp(1, 0, 5, G37.Height);
			T.E[FCellKey(0, 0)] = EdgeDoor;
			const FVec Lamp = T.Lamps[FCellKey(1, 0)].Emitter;
			// Cible au-dela d'un point vise sur la ligne de l'arete (U, Z)
			auto Through = [&](double U, double Z)
			{
				const FVec P0 = V3(500.0, 250.0 + U, Z);
				return V3(Lamp.X + (P0.X - Lamp.X) * 1.4, Lamp.Y + (P0.Y - Lamp.Y) * 1.4, Lamp.Z + (P0.Z - Lamp.Z) * 1.4);
			};
			const FGeom Flat = BRLight::Occlusion::MakeGeom(500.0, 120.0, 450.0, 220.0, true, false, 0.0, 90.0, true, true);
			Line("arche : rayon sous la voute au milieu (2,5 m) : eclaire ; meme rayon sous un linteau droit (2,25 m) : cache",
				Sees(T, G37, Lamp, Through(0.0, 250.0)) && !Sees(T, Flat, Lamp, Through(0.0, 250.0)));
			Line("arche : rayon pres du bord de la voute (75 cm de l'axe, 3,7 m) : cache", !Sees(T, G37, Lamp, Through(75.0, 370.0)));
			Line("arche : rayon dans l'axe a 3,4 m (sous le sommet a 3,9 m) : eclaire", Sees(T, G37, Lamp, Through(0.0, 340.0)));
		}
		// 4. Bout de mur au sommet : arete ouverte, mais le mur voisin (perpendiculaire) depasse d'un demi-mur dans le passage
		{
			FTestGrid T;
			const FVec Lamp = V3(430.0, 300.0, 265.0);
			const FVec P = V3(250.0, 345.0, 92.0);
			const bool bOpen = Sees(T, G0, Lamp, P);
			T.N[FCellKey(1, 0)] = EdgeWall;
			const bool bCapped = Sees(T, G0, Lamp, P);
			Line("sommet : rayon a moins d'un demi-mur du sommet ou arrive un mur : cache ; sans ce mur : eclaire", bOpen && !bCapped);
			FTestGrid C;
			C.N[FCellKey(1, 0)] = EdgeWall;
			C.E[FCellKey(0, 1)] = EdgeWall;
			Line("coin : rayon en diagonale par le sommet ou deux murs se rejoignent : cache",
				!Sees(C, G0, V3(175.0, 175.0, 265.0), V3(525.0, 525.0, 92.0)));
		}
		// 5. Pilier (45 cm) au sommet entre la lampe et le point
		{
			FTestGrid T;
			T.Pillars.insert(FCellKey(0, 0));
			const FVec Lamp = V3(175.0, 175.0, 265.0);
			Line("pilier : rayon par le sommet du pilier : cache ; a 42 cm du pilier : eclaire",
				!Sees(T, G0, Lamp, V3(525.0, 525.0, 92.0)) && Sees(T, G0, Lamp, V3(525.0, 440.0, 92.0)));
		}
		// 6. Cellule pleine (hotel, grottes) et applique murale
		{
			FTestGrid T;
			T.Solid.insert(FCellKey(1, 0));
			const FGeom G5 = BRLight::Occlusion::MakeGeom(350.0, 60.0, 320.0, 140.0, true, false, 0.0, 0.0, false, true);
			Line("cellule pleine entre la lampe et le point : cache ; sans elle : eclaire",
				!Sees(T, G5, V3(175.0, 175.0, 295.0), V3(875.0, 175.0, 92.0)) && Sees(FTestGrid(), G5, V3(175.0, 175.0, 295.0), V3(875.0, 175.0, 92.0)));
			T.AddLamp(0, 0, 4, G5.Height, 175.0, 0.0);
			const FVec Sconce = T.Lamps[FCellKey(0, 0)].Emitter;
			std::snprintf(Buf, sizeof(Buf), "applique : point d'emission a 30 cm devant le mur (%.0f, %.0f, %.0f)", Sconce.X, Sconce.Y, Sconce.Z);
			Line(Buf, Sconce.X == 320.0 && Sconce.Y == 175.0 && Sconce.Z == 210.0);
			Line("applique : eclaire son couloir, pas la cellule derriere son mur", Sees(T, G5, Sconce, V3(60.0, 175.0, 92.0)) && !Sees(T, G5, Sconce, V3(525.0, 175.0, 92.0)));
			FVec Street;
			BRLight::Occlusion::Emitter(6, 0.0, 460.0, 250.0, 210.0, 0.0, Street);
			Line("lampadaire : 1,3 m vers la rue, a 5,6 m", Street.X == 330.0 && Street.Y == 250.0 && Street.Z == 560.0);
		}
		// 7. Plafond et sol
		{
			FTestGrid T;
			const FVec Lamp = V3(175.0, 175.0, 265.0);
			Line("point au-dessus du plafond : cache", !Sees(T, G0, Lamp, V3(300.0, 175.0, 300.0)));
			T.Holes.insert(FCellKey(0, 0)); // fosse au sommet (350, 350)
			Line("fosse : point sous le sol vu par l'ouverture : eclaire ; meme point vu a travers la dalle : cache",
				Sees(T, G0, Lamp, V3(350.0, 350.0, -200.0)) && !Sees(T, G0, V3(-175.0, -175.0, 265.0), V3(350.0, 350.0, -200.0)));
		}
		// 8. Loi d'apport inchangee quand rien ne cache : somme des (1 - d/R)^2, x 0,6 pour un neon qui clignote, rien au-dela
		{
			FTestGrid T;
			T.AddLamp(0, 0, 1, G0.Height, 20.0, -10.0);
			T.AddLamp(1, 0, 1, G0.Height, 0.0, 0.0, true);
			T.AddLamp(3, 0, 1, G0.Height);
			const FVec P = V3(300.0, 200.0, 92.0);
			float All = 0.f, Vis = 0.f;
			CeilingSum(T, G0, 620.f, P, All, Vis);
			const float D0 = static_cast<float>(std::sqrt((195.0 - 300.0) * (195.0 - 300.0) + (165.0 - 200.0) * (165.0 - 200.0)));
			const float D1 = static_cast<float>(std::sqrt((525.0 - 300.0) * (525.0 - 300.0) + (175.0 - 200.0) * (175.0 - 200.0)));
			const float K0 = 1.f - D0 / 620.f;
			const float K1 = 1.f - D1 / 620.f;
			// Une operation par instruction, comme la somme du jeu (clang fusionnerait "a * b + c" en une seule operation)
			const float W0 = K0 * K0;
			const float W1 = K1 * K1 * 0.6f;
			float Expected = 0.f; // la lampe (3, 0) est a 9 m : hors de portee
			Expected += W0;
			Expected += W1;
			std::snprintf(Buf, sizeof(Buf), "rien ne cache : somme %.4f = loi d'avant %.4f (neon qui clignote x 0,6, lampe a 9 m ignoree)", Vis, Expected);
			Line(Buf, Vis == All && Vis == Expected);
		}
	}

	// ------------------------------------------------------------------------------------------------ portage de la grille
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
	int FloorDiv(int A, int B)
	{
		return (A >= 0) ? (A / B) : -((-A + B - 1) / B);
	}
	int PosMod(int A, int B)
	{
		const int M = A % B;
		return M < 0 ? M + B : M;
	}
	int RoundToInt(float F)
	{
		return static_cast<int>(std::floor(F + 0.5f));
	}

	/** Definition d'un niveau (les champs de FBRLevelDef dont la grille et la lumiere dependent), valeurs par defaut de
	 *  BRTypes.h, puis celles de BRLevels.cpp */
	struct FLevelDef
	{
		int Number = 0;
		std::string Layout = "Rooms";
		std::string Fixture = "Panel";
		float CellSize = 350.f, WallHeight = 290.f, WallThickness = 60.f;
		int ChunkCells = 8;
		float WallLineChance = 0.55f;
		int SegmentLength = 4;
		float DoorChance = 0.3f, DoorWidth = 140.f, PillarChance = 0.04f, PillarSize = 50.f, SolidChance = 0.f, LoopChance = 0.12f;
		float OpenZoneChance = 0.15f;
		int Spacing = 6;
		bool bCeiling = true, bLintels = true, bArches = false, bPillarGrid = false, bOutdoor = false, bLidar = false;
		float ArchApex = 0.f;
		float LightChance = 0.45f, BrokenChance = 0.05f, FlickerChance = 0.06f, LightRadius = 650.f, DarkZoneChance = 0.f;
		float CubicleZoneChance = 0.f, PoolChance = 0.f, SanityDrain = 0.12f;
		int BoundsChunks = 0;
		float PitRoomChance = 0.f;
		int PitRoomsMin = 1, PitRoomCells = 5, PitDoorsPerSide = 1;
		int Assigned = 0;
	};

	bool ReadFile(const char* Path, std::string& Out)
	{
		std::ifstream F(Path, std::ios::binary);
		if (!F)
		{
			return false;
		}
		std::ostringstream SS;
		SS << F.rdbuf();
		Out = SS.str();
		return true;
	}

	/** Valeurs "D.<Nom> = <valeur>;" de la fonction LevelN() de BRLevels.cpp (champs connus seulement) */
	bool ParseLevel(const std::string& Src, int Number, FLevelDef& D)
	{
		char Head[64];
		std::snprintf(Head, sizeof(Head), "FBRLevelDef Level%d()", Number);
		const size_t Start = Src.find(Head);
		if (Start == std::string::npos)
		{
			return false;
		}
		const size_t End = Src.find("return D;", Start);
		const std::string Body = Src.substr(Start, End == std::string::npos ? std::string::npos : End - Start);
		D = FLevelDef();
		D.Number = Number;
		size_t Pos = 0;
		while ((Pos = Body.find("D.", Pos)) != std::string::npos)
		{
			Pos += 2;
			size_t K = Pos;
			while (K < Body.size() && (std::isalnum(static_cast<unsigned char>(Body[K])) || Body[K] == '_'))
			{
				++K;
			}
			const std::string Name = Body.substr(Pos, K - Pos);
			while (K < Body.size() && Body[K] == ' ')
			{
				++K;
			}
			if (K >= Body.size() || Body[K] != '=' || (K + 1 < Body.size() && Body[K + 1] == '='))
			{
				continue;
			}
			++K;
			while (K < Body.size() && Body[K] == ' ')
			{
				++K;
			}
			const size_t Semi = Body.find(';', K);
			const std::string Val = Body.substr(K, Semi == std::string::npos ? std::string::npos : Semi - K);
			const bool bTrue = Val == "true";
			const bool bBool = bTrue || Val == "false";
			const double Num = std::strtod(Val.c_str(), nullptr);
			const bool bNum = !Val.empty() && (std::isdigit(static_cast<unsigned char>(Val[0])) || Val[0] == '-' || Val[0] == '.');
			auto F = [&](const char* Field, float& Dst) { if (Name == Field && bNum) { Dst = static_cast<float>(Num); ++D.Assigned; } };
			auto I = [&](const char* Field, int& Dst) { if (Name == Field && bNum) { Dst = static_cast<int>(Num); ++D.Assigned; } };
			auto B = [&](const char* Field, bool& Dst) { if (Name == Field && bBool) { Dst = bTrue; ++D.Assigned; } };
			if (Name == "Layout" && Val.rfind("EBRLayout::", 0) == 0)
			{
				D.Layout = Val.substr(11);
			}
			if (Name == "Fixture" && Val.rfind("EBRFixture::", 0) == 0)
			{
				D.Fixture = Val.substr(12);
			}
			F("CellSize", D.CellSize); F("WallHeight", D.WallHeight); F("WallThickness", D.WallThickness); I("ChunkCells", D.ChunkCells);
			F("WallLineChance", D.WallLineChance); I("SegmentLength", D.SegmentLength); F("DoorChance", D.DoorChance); F("DoorWidth", D.DoorWidth);
			F("PillarChance", D.PillarChance); F("PillarSize", D.PillarSize); F("SolidChance", D.SolidChance); F("LoopChance", D.LoopChance);
			F("OpenZoneChance", D.OpenZoneChance); I("Spacing", D.Spacing); B("bCeiling", D.bCeiling); B("bLintels", D.bLintels); B("bArches", D.bArches);
			B("bPillarGrid", D.bPillarGrid); B("bOutdoor", D.bOutdoor); B("bLidar", D.bLidar); F("ArchApex", D.ArchApex); F("LightChance", D.LightChance);
			F("BrokenChance", D.BrokenChance); F("FlickerChance", D.FlickerChance); F("LightRadius", D.LightRadius); F("DarkZoneChance", D.DarkZoneChance);
			F("CubicleZoneChance", D.CubicleZoneChance); F("PoolChance", D.PoolChance); F("SanityDrain", D.SanityDrain); I("BoundsChunks", D.BoundsChunks); F("PitRoomChance", D.PitRoomChance);
			I("PitRoomsMin", D.PitRoomsMin); I("PitRoomCells", D.PitRoomCells); I("PitDoorsPerSide", D.PitDoorsPerSide);
		}
		return true;
	}

	int FixtureIndex(const std::string& F)
	{
		static const char* const Names[] = { "None", "Panel", "Tube", "Bulb", "Sconce", "SkyPanel", "StreetLamp" };
		for (int I = 0; I < 7; ++I)
		{
			if (F == Names[I])
			{
				return I;
			}
		}
		return 0;
	}

	struct FRect
	{
		int MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
	};

	struct FCellLight
	{
		bool bHas = false, bBroken = false, bFlicker = false;
		float OffX = 0.f, OffY = 0.f;
	};

	/** Portage des regles de grille d'ABRWorld (BRWorld.cpp) pour les dispositions de ce banc (sans l'aile du Level 6 ni les
	 *  rues des Niveaux 9 et 11) */
	class FPortWorld
	{
	public:
		const FLevelDef& D;
		uint32_t Seed;
		int N;
		std::map<FCellKey, FRect> BoundedPitRooms;
		std::set<FCellKey> ForcedDoors;
		std::set<FCellKey> SafeReach;

		FPortWorld(const FLevelDef& InD, uint32_t InSeed) : D(InD), Seed(InSeed), N(InD.ChunkCells) { PrepareLevelLayout(); }

		bool IsRooms() const { return D.Layout == "Rooms"; }
		bool IsMaze() const { return D.Layout == "Maze"; }
		bool IsSpawnArea(int X, int Y) const { return std::abs(X) <= 1 && std::abs(Y) <= 1; }
		FCellKey CellToChunk(int X, int Y) const { return FCellKey(FloorDiv(X, N), FloorDiv(Y, N)); }
		bool IsChunkInBounds(int CX, int CY) const
		{
			const int B = D.BoundsChunks;
			return B <= 0 || (CX >= -B && CX < B && CY >= -B && CY < B);
		}
		bool IsCellInBounds(int X, int Y) const
		{
			const FCellKey C = CellToChunk(X, Y);
			return D.BoundsChunks <= 0 || IsChunkInBounds(C.first, C.second);
		}
		bool IsSpawnChunk(int CX, int CY) const
		{
			const FCellKey Lo = CellToChunk(-1, -1);
			const FCellKey Hi = CellToChunk(1, 1);
			return CX >= Lo.first && CX <= Hi.first && CY >= Lo.second && CY <= Hi.second;
		}
		int BoundedChunkCount(bool bAvoidSpawn) const
		{
			int Count = 0;
			for (int X = -D.BoundsChunks; X < D.BoundsChunks; ++X)
			{
				for (int Y = -D.BoundsChunks; Y < D.BoundsChunks; ++Y)
				{
					Count += (bAvoidSpawn && IsSpawnChunk(X, Y)) ? 0 : 1;
				}
			}
			return Count;
		}
		bool IsChunkPicked(int CX, int CY, int Salt, int Count, bool bAvoidSpawn) const
		{
			const int B = D.BoundsChunks;
			if (B <= 0 || Count <= 0 || !IsChunkInBounds(CX, CY) || (bAvoidSpawn && IsSpawnChunk(CX, CY)))
			{
				return false;
			}
			const uint32_t Mine = Hash(CX, CY, Salt, Seed);
			int Before = 0;
			for (int X = -B; X < B; ++X)
			{
				for (int Y = -B; Y < B; ++Y)
				{
					if ((X == CX && Y == CY) || (bAvoidSpawn && IsSpawnChunk(X, Y)))
					{
						continue;
					}
					const uint32_t K = Hash(X, Y, Salt, Seed);
					if (K < Mine || (K == Mine && (X < CX || (X == CX && Y < CY))))
					{
						++Before;
					}
				}
			}
			return Before < Count;
		}
		bool HasPits() const { return (D.PitRoomChance > 0.f || D.bLidar) && D.PoolChance <= 0.f && (IsRooms() || IsMaze()); }
		bool ComputePitRoom(int CX, int CY, FRect& Out) const
		{
			if (!HasPits() || D.bLidar || !IsChunkInBounds(CX, CY) || IsSpawnChunk(CX, CY))
			{
				return false;
			}
			bool bPicked = false;
			if (D.BoundsChunks > 0)
			{
				const int Count0 = RoundToInt(D.PitRoomChance * static_cast<float>(BoundedChunkCount(true)));
				const int Count = D.PitRoomsMin > Count0 ? D.PitRoomsMin : Count0;
				bPicked = IsChunkPicked(CX, CY, 1950, Count, true);
			}
			else
			{
				bPicked = Rand(CX, CY, 1950, Seed) < D.PitRoomChance;
			}
			if (!bPicked)
			{
				return false;
			}
			const int K = D.PitRoomCells < 3 ? 3 : (D.PitRoomCells > N - 2 ? N - 2 : D.PitRoomCells);
			const int Free = N - K - 2;
			const int OX = 1 + (Free > 0 ? static_cast<int>(Hash(CX, CY, 1951, Seed) % static_cast<uint32_t>(Free + 1)) : 0);
			const int OY = 1 + (Free > 0 ? static_cast<int>(Hash(CX, CY, 1952, Seed) % static_cast<uint32_t>(Free + 1)) : 0);
			Out.MinX = CX * N + OX;
			Out.MinY = CY * N + OY;
			Out.MaxX = Out.MinX + K;
			Out.MaxY = Out.MinY + K;
			return true;
		}
		bool GetPitRoom(int CX, int CY, FRect& Out) const
		{
			if (!HasPits())
			{
				return false;
			}
			if (D.BoundsChunks > 0)
			{
				const auto It = BoundedPitRooms.find(FCellKey(CX, CY));
				if (It == BoundedPitRooms.end())
				{
					return false;
				}
				Out = It->second;
				return true;
			}
			return ComputePitRoom(CX, CY, Out);
		}
		bool IsPitRoomCell(int X, int Y) const
		{
			FRect R;
			const FCellKey C = CellToChunk(X, Y);
			return GetPitRoom(C.first, C.second, R) && X >= R.MinX && X < R.MaxX && Y >= R.MinY && Y < R.MaxY;
		}
		unsigned char PitEdge(int X, int Y, bool bEast, bool& bOut) const
		{
			bOut = false;
			if (!HasPits())
			{
				return 0;
			}
			const int BX = bEast ? X + 1 : X;
			const int BY = bEast ? Y : Y + 1;
			const FCellKey Chunk = CellToChunk(X, Y);
			FRect R;
			if (Chunk != CellToChunk(BX, BY) || !GetPitRoom(Chunk.first, Chunk.second, R))
			{
				return 0;
			}
			bOut = true;
			const bool bInA = X >= R.MinX && X < R.MaxX && Y >= R.MinY && Y < R.MaxY;
			const bool bInB = BX >= R.MinX && BX < R.MaxX && BY >= R.MinY && BY < R.MaxY;
			if (bInA == bInB)
			{
				return 0;
			}
			const int K = R.MaxX - R.MinX;
			const int Side = bEast ? (bInA ? 0 : 1) : (bInA ? 2 : 3);
			const int Along = bEast ? Y - R.MinY : X - R.MinX;
			const int Doors = D.PitDoorsPerSide > 1 ? D.PitDoorsPerSide : 1;
			for (int Dd = 0; Dd < Doors; ++Dd)
			{
				const int Pos = static_cast<int>(Hash(R.MinX, R.MinY, 1960 + Side * 8 + Dd, Seed) % static_cast<uint32_t>(K > 1 ? K : 1));
				if (Pos == Along)
				{
					return 2;
				}
			}
			return 1;
		}
		float ZoneDensity(int X, int Y) const
		{
			const float R = Rand(FloorDiv(X, 6), FloorDiv(Y, 6), 104, Seed);
			if (R < D.OpenZoneChance)
			{
				return 0.25f;
			}
			if (R > 0.85f)
			{
				return 1.35f;
			}
			return 1.f;
		}
		int CubicleRole(int X, int Y) const
		{
			const int Zone = 12;
			if (D.CubicleZoneChance <= 0.f || !IsRooms() || Rand(FloorDiv(X, Zone), FloorDiv(Y, Zone), 1901, Seed) >= D.CubicleZoneChance)
			{
				return -1;
			}
			const int LX = PosMod(X, Zone);
			const int LY = PosMod(Y, Zone);
			if (LX % 6 == 0 || LY % 3 == 0)
			{
				return 0;
			}
			return LY % 3 == 1 ? 1 : 2;
		}
		unsigned char CubicleEdge(int X, int Y, bool bEast, bool& bOut) const
		{
			const int A = CubicleRole(X, Y);
			const int B = bEast ? CubicleRole(X + 1, Y) : CubicleRole(X, Y + 1);
			bOut = A >= 0 || B >= 0;
			if (!bOut)
			{
				return 0;
			}
			if (bEast)
			{
				return (A == 0 || B == 0) ? 0 : 1;
			}
			if (A == 2 || B == 1)
			{
				return 2;
			}
			if (A == 1 && B == 2)
			{
				return 1;
			}
			return (A == 0 || B == 0) ? 0 : 1;
		}
		bool MazeOpen(int X, int Y, bool bEast) const
		{
			const bool bCarveEast = Rand(X, Y, 201, Seed) < 0.5f;
			if (bEast == bCarveEast)
			{
				return true;
			}
			if (Rand(X, Y, bEast ? 202 : 203, Seed) < D.LoopChance)
			{
				return true;
			}
			if (Rand(FloorDiv(X, 4), FloorDiv(Y, 4), 204, Seed) < D.OpenZoneChance)
			{
				return bEast ? (PosMod(X, 4) != 3) : (PosMod(Y, 4) != 3);
			}
			return false;
		}
		bool IsSolid(int X, int Y) const
		{
			if (D.Layout == "Hotel")
			{
				if (IsSpawnArea(X, Y))
				{
					return false;
				}
				const bool bCorrX = PosMod(X, D.Spacing) == 0 && Rand(X, 0, 501, Seed) < 0.85f;
				const bool bCorrY = PosMod(Y, D.Spacing) == 0 && Rand(0, Y, 502, Seed) < 0.85f;
				if (bCorrX || bCorrY)
				{
					return false;
				}
				return Rand(FloorDiv(X, D.Spacing), FloorDiv(Y, D.Spacing), 503, Seed) >= D.OpenZoneChance;
			}
			if (D.Layout == "Caves")
			{
				if (std::abs(X) <= 1 && std::abs(Y) <= 1)
				{
					return false;
				}
				const float R = 0.55f * Rand(X, Y, 601, Seed) + 0.45f * Rand(FloorDiv(X, 3), FloorDiv(Y, 3), 602, Seed);
				return R < D.SolidChance;
			}
			return false;
		}
		unsigned char Edge(int X, int Y, bool bEast) const
		{
			const int BX = bEast ? X + 1 : X;
			const int BY = bEast ? Y : Y + 1;
			if (D.BoundsChunks > 0)
			{
				const bool bIn = IsCellInBounds(X, Y);
				if (bIn != IsCellInBounds(BX, BY))
				{
					return 1;
				}
				if (!bIn)
				{
					return 0;
				}
			}
			if (IsSpawnArea(X, Y) && IsSpawnArea(BX, BY))
			{
				return 0;
			}
			bool bPit = false;
			const unsigned char PE = PitEdge(X, Y, bEast, bPit);
			if (bPit)
			{
				return PE;
			}
			if (!ForcedDoors.empty() && ForcedDoors.count(FCellKey(X * 2 + (bEast ? 1 : 0), Y)))
			{
				return 2;
			}
			if (IsRooms())
			{
				bool bCub = false;
				const unsigned char CE = CubicleEdge(X, Y, bEast, bCub);
				if (bCub)
				{
					return CE;
				}
				const int Seg = D.SegmentLength > 1 ? D.SegmentLength : 1;
				if (bEast)
				{
					const int Off = static_cast<int>(Hash(X, 0, 101, Seed) % static_cast<uint32_t>(Seg));
					if (Rand(X, FloorDiv(Y + Off, Seg), 102, Seed) >= D.WallLineChance * ZoneDensity(X, Y))
					{
						return 0;
					}
					return Rand(X, Y, 103, Seed) < D.DoorChance ? 2 : 1;
				}
				const int Off = static_cast<int>(Hash(0, Y, 111, Seed) % static_cast<uint32_t>(Seg));
				if (Rand(FloorDiv(X + Off, Seg), Y, 112, Seed) >= D.WallLineChance * ZoneDensity(X, Y))
				{
					return 0;
				}
				return Rand(X, Y, 113, Seed) < D.DoorChance ? 2 : 1;
			}
			if (IsMaze())
			{
				return MazeOpen(X, Y, bEast) ? 0 : 1;
			}
			return 0;
		}
		bool CanStep(int AX, int AY, int BX, int BY) const
		{
			if (IsSolid(BX, BY) || IsSolid(AX, AY))
			{
				return false;
			}
			const int DX = BX - AX;
			const int DY = BY - AY;
			if (DX == 1 && DY == 0)
			{
				return Edge(AX, AY, true) != 1;
			}
			if (DX == -1 && DY == 0)
			{
				return Edge(BX, BY, true) != 1;
			}
			if (DX == 0 && DY == 1)
			{
				return Edge(AX, AY, false) != 1;
			}
			if (DX == 0 && DY == -1)
			{
				return Edge(BX, BY, false) != 1;
			}
			return false;
		}
		bool HasPillar(int X, int Y) const
		{
			if (!IsRooms() || D.PillarChance <= 0.f)
			{
				return false;
			}
			if (HasPits() && (IsPitRoomCell(X, Y) || IsPitRoomCell(X + 1, Y) || IsPitRoomCell(X, Y + 1) || IsPitRoomCell(X + 1, Y + 1)))
			{
				return false;
			}
			const bool bOpenZone = ZoneDensity(X, Y) < 0.5f;
			if (D.bPillarGrid && bOpenZone)
			{
				return PosMod(X, 2) == 1 && PosMod(Y, 2) == 1;
			}
			const float Mult = bOpenZone ? 2.5f : 1.f;
			return Rand(X, Y, 301, Seed) < D.PillarChance * Mult;
		}
		bool IsDarkZone(int X, int Y) const
		{
			if (D.Fixture == "None")
			{
				return !D.bOutdoor;
			}
			if (std::abs(X) <= 3 && std::abs(Y) <= 3)
			{
				return false;
			}
			return Rand(FloorDiv(X, 5), FloorDiv(Y, 5), 401, Seed) < D.DarkZoneChance;
		}
		FCellLight CellLight(int X, int Y) const
		{
			FCellLight L;
			if (D.Fixture == "None" || IsSolid(X, Y))
			{
				return L;
			}
			const bool bSpawn = IsSpawnArea(X, Y);
			auto Finish = [&](FCellLight& Out)
			{
				Out.bHas = true;
				Out.bBroken = !bSpawn && Rand(X, Y, 411, Seed) < D.BrokenChance;
				Out.bFlicker = !Out.bBroken && Rand(X, Y, 412, Seed) < D.FlickerChance;
			};
			if (D.Layout == "Hotel")
			{
				if (PosMod(X + Y, 2) != 0 || (!bSpawn && Rand(X, Y, 410, Seed) >= D.LightChance))
				{
					return L;
				}
				static const int Dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
				const int Start = static_cast<int>(Hash(X, Y, 417, Seed) % 4u);
				for (int K = 0; K < 4; ++K)
				{
					const int* Dir = Dirs[(Start + K) % 4];
					if (IsSolid(X + Dir[0], Y + Dir[1]))
					{
						Finish(L);
						const float Half = D.CellSize * 0.5f;
						L.OffX = Dir[0] * Half;
						L.OffY = Dir[1] * Half;
						return L;
					}
				}
				return L;
			}
			if (D.Layout != "Rooms" && D.Layout != "Maze" && D.Layout != "Caves")
			{
				return L; // rues (Niveaux 9, 11), Niveau 10 : hors de ce banc
			}
			FRect Room;
			const FCellKey Chunk = CellToChunk(X, Y);
			if (HasPits() && GetPitRoom(Chunk.first, Chunk.second, Room) && X >= Room.MinX && X < Room.MaxX && Y >= Room.MinY && Y < Room.MaxY)
			{
				if (PosMod(X - Room.MinX, 2) != 0 || PosMod(Y - Room.MinY, 2) != 0)
				{
					return L;
				}
				L.bHas = true;
				L.bBroken = Rand(X, Y, 1955, Seed) < 0.12f;
				L.bFlicker = !L.bBroken && Rand(X, Y, 1956, Seed) < 0.18f;
				return L;
			}
			if (!bSpawn && IsDarkZone(X, Y))
			{
				return L;
			}
			if (!bSpawn && Rand(X, Y, 410, Seed) >= D.LightChance)
			{
				return L;
			}
			Finish(L);
			const float J = D.CellSize * 0.12f;
			L.OffX = (Rand(X, Y, 414, Seed) - 0.5f) * 2.f * J;
			L.OffY = (Rand(X, Y, 415, Seed) - 0.5f) * 2.f * J;
			return L;
		}
		/** ABRWorld::PrepareLevelLayout : salles de fosses et portes percees du niveau fini */
		void PrepareLevelLayout()
		{
			if (D.BoundsChunks <= 0)
			{
				return;
			}
			if (HasPits())
			{
				for (int X = -D.BoundsChunks; X < D.BoundsChunks; ++X)
				{
					for (int Y = -D.BoundsChunks; Y < D.BoundsChunks; ++Y)
					{
						FRect R;
						if (ComputePitRoom(X, Y, R))
						{
							BoundedPitRooms[FCellKey(X, Y)] = R;
						}
					}
				}
			}
			static const int Dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
			std::vector<FCellKey> Queue;
			auto Grow = [&]()
			{
				for (size_t Head = 0; Head < Queue.size(); ++Head)
				{
					const FCellKey Cur = Queue[Head];
					for (int K = 0; K < 4; ++K)
					{
						const FCellKey Nb(Cur.first + Dirs[K][0], Cur.second + Dirs[K][1]);
						if (SafeReach.count(Nb) || !IsCellInBounds(Nb.first, Nb.second) || !CanStep(Cur.first, Cur.second, Nb.first, Nb.second)
							|| IsPitRoomCell(Nb.first, Nb.second))
						{
							continue;
						}
						SafeReach.insert(Nb);
						Queue.push_back(Nb);
					}
				}
				Queue.clear();
			};
			SafeReach.insert(FCellKey(0, 0));
			Queue.push_back(FCellKey(0, 0));
			Grow();
			for (int Guard = 0; Guard < 512; ++Guard)
			{
				bool bFound = false;
				uint32_t BestH = 0;
				FCellKey BestKey(0, 0);
				FCellKey BestCell(0, 0);
				for (const FCellKey& C : SafeReach)
				{
					for (int K = 0; K < 4; ++K)
					{
						const FCellKey Nb(C.first + Dirs[K][0], C.second + Dirs[K][1]);
						if (!IsCellInBounds(Nb.first, Nb.second) || SafeReach.count(Nb) || IsPitRoomCell(Nb.first, Nb.second))
						{
							continue;
						}
						const bool bEast = Dirs[K][1] == 0;
						const FCellKey A = (Dirs[K][0] > 0 || Dirs[K][1] > 0) ? C : Nb;
						const FCellKey Key(A.first * 2 + (bEast ? 1 : 0), A.second);
						const uint32_t H = Hash(Key.first, Key.second, 1990, Seed);
						if (!bFound || H < BestH || (H == BestH && (Key.first < BestKey.first || (Key.first == BestKey.first && Key.second < BestKey.second))))
						{
							bFound = true;
							BestH = H;
							BestKey = Key;
							BestCell = Nb;
						}
					}
				}
				if (!bFound)
				{
					break;
				}
				ForcedDoors.insert(BestKey);
				SafeReach.insert(BestCell);
				Queue.push_back(BestCell);
				Grow();
			}
		}
	};

	/** Cellule pre-calculee (comme FBRLightCell du jeu) */
	struct FBenchCell
	{
		unsigned char E = 0, N = 0;
		bool bPillar = false, bSolid = false, bLamp = false, bFlicker = false, bWalkable = false, bInBounds = true;
		FLamp Lamp;
	};

	/** Grille d'une graine pre-calculee sur une zone (meme role que le cache par chunk du jeu) */
	struct FBenchGrid
	{
		int X0 = 0, Y0 = 0, W = 0;
		double S = 350.0;
		std::vector<FBenchCell> Cells;
		const FPortWorld* Port = nullptr;
		FBenchCell Outside(int X, int Y) const
		{
			FBenchCell C;
			C.E = Port->Edge(X, Y, true);
			C.N = Port->Edge(X, Y, false);
			return C;
		}
		const FBenchCell* At(int X, int Y) const
		{
			const int LX = X - X0;
			const int LY = Y - Y0;
			return (LX < 0 || LY < 0 || LX >= W || LY >= W) ? nullptr : &Cells[static_cast<size_t>(LY * W + LX)];
		}
		unsigned char EdgeE(int X, int Y) const { const FBenchCell* C = At(X, Y); return C ? C->E : Port->Edge(X, Y, true); }
		unsigned char EdgeN(int X, int Y) const { const FBenchCell* C = At(X, Y); return C ? C->N : Port->Edge(X, Y, false); }
		bool Pillar(int X, int Y) const { const FBenchCell* C = At(X, Y); return C ? C->bPillar : Port->HasPillar(X, Y); }
		bool SolidBox(int X, int Y, double Lo[3], double Hi[3]) const
		{
			const FBenchCell* C = At(X, Y);
			if (!(C ? C->bSolid : Port->IsSolid(X, Y)))
			{
				return false;
			}
			Lo[0] = X * S;
			Lo[1] = Y * S;
			Lo[2] = -BRLight::Occlusion::Huge;
			Hi[0] = (X + 1) * S;
			Hi[1] = (Y + 1) * S;
			Hi[2] = BRLight::Occlusion::Huge;
			return true;
		}
		bool FloorOpen(double, double) const { return false; }
		bool Lamp(int X, int Y, FLamp& Out) const
		{
			const FBenchCell* C = At(X, Y);
			if (!C)
			{
				return false;
			}
			Out = C->Lamp;
			return C->bLamp;
		}
	};

	void BuildBenchGrid(const FPortWorld& P, int Half, int Margin, FBenchGrid& G)
	{
		G.Port = &P;
		G.X0 = -Half - Margin;
		G.Y0 = -Half - Margin;
		G.W = 2 * (Half + Margin);
		G.S = P.D.CellSize;
		G.Cells.assign(static_cast<size_t>(G.W * G.W), FBenchCell());
		const int Fixture = FixtureIndex(P.D.Fixture);
		const float S = P.D.CellSize;
		for (int LY = 0; LY < G.W; ++LY)
		{
			for (int LX = 0; LX < G.W; ++LX)
			{
				const int X = G.X0 + LX;
				const int Y = G.Y0 + LY;
				FBenchCell& C = G.Cells[static_cast<size_t>(LY * G.W + LX)];
				C.E = P.Edge(X, Y, true);
				C.N = P.Edge(X, Y, false);
				C.bPillar = P.HasPillar(X, Y);
				C.bSolid = P.IsSolid(X, Y);
				C.bInBounds = P.IsCellInBounds(X, Y);
				C.bWalkable = !C.bSolid && C.bInBounds;
				const FCellLight L = P.CellLight(X, Y);
				if (L.bHas && !L.bBroken && C.bInBounds)
				{
					// Meme ancre que le jeu : centre (calcul en float, comme ABRWorld::CellCenter) + decalage
					C.Lamp.AX = static_cast<double>((X + 0.5f) * S) + static_cast<double>(L.OffX);
					C.Lamp.AY = static_cast<double>((Y + 0.5f) * S) + static_cast<double>(L.OffY);
					C.Lamp.bFlicker = L.bFlicker;
					C.bLamp = BRLight::Occlusion::Emitter(Fixture, P.D.WallHeight, C.Lamp.AX, C.Lamp.AY, L.OffX, L.OffY, C.Lamp.Emitter);
					C.Lamp.bOn = C.bLamp;
					C.bFlicker = L.bFlicker;
				}
			}
		}
	}

	FGeom GeomOf(const FLevelDef& D)
	{
		return BRLight::Occlusion::MakeGeom(D.CellSize, D.WallThickness, D.WallHeight, D.DoorWidth, D.bLintels, D.bArches, D.ArchApex, D.PillarSize,
			D.Layout == "Rooms" && D.PillarChance > 0.f, D.bCeiling);
	}

	uint32_t SeedFromUser(uint32_t UserSeed, int Level)
	{
		const uint32_t S = Mix(UserSeed ^ static_cast<uint32_t>(Level * 7919 + 17));
		return S != 0u ? S : 1u;
	}

	/** Empreinte de la grille de lumiere sur [-Half, Half[ (FNV-1a) : aretes, pilier, bloc plein, plafonnier, clignotement,
	 *  ancre au cm. -BRAutoTestV416Light calcule la meme avec ABRWorld::GetLightCell */
	uint32_t Fingerprint(const FBenchGrid& G, int Half)
	{
		uint32_t H = 2166136261u;
		auto Feed = [&H](int V)
		{
			const uint32_t U = static_cast<uint32_t>(V);
			for (int B = 0; B < 4; ++B)
			{
				H ^= (U >> (8 * B)) & 0xFFu;
				H *= 16777619u;
			}
		};
		for (int Y = -Half; Y < Half; ++Y)
		{
			for (int X = -Half; X < Half; ++X)
			{
				const FBenchCell* C = G.At(X, Y);
				Feed(C->E | (C->N << 2) | (C->bPillar ? 16 : 0) | (C->bSolid ? 32 : 0) | (C->bLamp ? 64 : 0) | (C->bFlicker ? 128 : 0));
				if (C->bLamp)
				{
					Feed(static_cast<int>(std::floor(C->Lamp.AX + 0.5)));
					Feed(static_cast<int>(std::floor(C->Lamp.AY + 0.5)));
				}
			}
		}
		return H;
	}

	struct FBalance
	{
		long long Cells = 0;
		long long Changed = 0;           // joueur (0,92 m) : lumiere differente
		double SumOld = 0.0, SumNew = 0.0;
		long long SpawnOld = 0, SpawnNew = 0;       // centre de cellule a 1,65 m sous NeedsDark : une entite de l'ombre peut y apparaitre
		long long VanishOld = 0, VanishNew = 0;     // Smiler (1,65 m) au-dessus de SmilerVanish : il se dissipe hors poursuite
		long long DarkOld = 0, DarkNew = 0;         // joueur sous 0,12 : sante mentale plus vite (+0,22 point/s sans lampe)
		long long MothOld = 0, MothNew = 0;         // Deathmoth (2,10 m) au-dessus de 0,55 : il n'est pas attire par la lampe
		long long ThereOld = 0, ThereNew = 0;       // joueur au-dessus de 0,55 : Deathmoth pas attire vers lui
		long long DimOld = 0, DimNew = 0;           // joueur sous 0,15 : la Bacteria le repere deux fois moins vite
		long long Lamps = 0, Hidden = 0, Boxes = 0;
		int Seeds = 0;
	};

	double Pct(long long A, long long B)
	{
		return B > 0 ? 100.0 * static_cast<double>(A) / static_cast<double>(B) : 0.0;
	}

	void MeasureSeed(const FLevelDef& D, uint32_t LevelSeed, FBalance& B)
	{
		const FPortWorld P(D, LevelSeed);
		const int Half = D.BoundsChunks > 0 ? D.BoundsChunks * D.ChunkCells : 16;
		FBenchGrid G;
		BuildBenchGrid(P, Half, 6, G);
		const FGeom Geo = GeomOf(D);
		const double S = D.CellSize;
		const double Free = S - D.WallThickness - 2.0 * 40.0; // 40 cm des faces des murs (rayon d'une capsule)
		BRLight::Occlusion::FStats St;
		auto Light = [&](double X, double Y, double Z, float& Old, float& New)
		{
			BRLight::Occlusion::CeilingSum(G, Geo, D.LightRadius, V3(X, Y, Z), Old, New, &St);
		};
		for (int Y = -Half; Y < Half; ++Y)
		{
			for (int X = -Half; X < Half; ++X)
			{
				const FBenchCell* C = G.At(X, Y);
				if (!C->bWalkable)
				{
					continue;
				}
				++B.Cells;
				const double CX = (X + 0.5) * S;
				const double CY = (Y + 0.5) * S;
				// Point au hasard de la cellule (fixe : graine et cellule), a 40 cm au moins des faces des murs
				const uint32_t R = Hash(X, Y, 9001, LevelSeed);
				const double PX = CX + ((R & 0xFFFu) / 4095.0 - 0.5) * Free;
				const double PY = CY + (((R >> 12) & 0xFFFu) / 4095.0 - 0.5) * Free;
				float O = 0.f, Nw = 0.f;
				Light(PX, PY, 92.0, O, Nw);
				B.Changed += std::fabs(O - Nw) > 0.001f ? 1 : 0;
				B.SumOld += O;
				B.SumNew += Nw;
				B.DarkOld += O < 0.12f ? 1 : 0;
				B.DarkNew += Nw < 0.12f ? 1 : 0;
				B.ThereOld += O > 0.55f ? 1 : 0;
				B.ThereNew += Nw > 0.55f ? 1 : 0;
				B.DimOld += O < 0.15f ? 1 : 0;
				B.DimNew += Nw < 0.15f ? 1 : 0;
				Light(PX, PY, 165.0, O, Nw);
				B.VanishOld += O > BRLight::SmilerVanish ? 1 : 0;
				B.VanishNew += Nw > BRLight::SmilerVanish ? 1 : 0;
				Light(CX, CY, 165.0, O, Nw);
				B.SpawnOld += O <= BRLight::NeedsDark ? 1 : 0;
				B.SpawnNew += Nw <= BRLight::NeedsDark ? 1 : 0;
				Light(PX, PY, 210.0, O, Nw);
				B.MothOld += O > 0.55f ? 1 : 0;
				B.MothNew += Nw > 0.55f ? 1 : 0;
			}
		}
		B.Lamps += St.Lamps;
		B.Hidden += St.Hidden;
		B.Boxes += St.Boxes;
		++B.Seeds;
	}

	void SectionBalance(const std::string& Src)
	{
		std::printf("\n## C. Equilibrage : plafonniers d'avant (murs ignores) et de la v4.16, sur la grille du jeu rejouee hors moteur\n\n");
		std::printf("  Chaque cellule praticable : un point fixe au hasard (40 cm des murs) pour le joueur (0,92 m), le Smiler (1,65 m) et le\n");
		std::printf("  Deathmoth (2,10 m) ; le centre de la cellule a 1,65 m pour les apparitions (comme ABRWorld::FindSpawnSpot). Courant\n");
		std::printf("  plein, plafonniers seuls (les sources de mission sont rares : porche du Niveau 9). Seuils : apparition sous %.2f,\n", BRLight::NeedsDark);
		std::printf("  dissipation du Smiler au-dessus de %.2f, Deathmoth 0,55, Bacteria 0,15, sante mentale 0,12.\n\n", BRLight::SmilerVanish);
		struct FPlan
		{
			int Level;
			int Seeds;
		};
		const FPlan Plans[] = { { 0, 300 }, { 1, 60 }, { 2, 60 }, { 3, 60 }, { 4, 60 }, { 5, 60 }, { 8, 60 }, { 37, 60 } };
		std::printf("  %-6s %6s %8s %8s %13s %15s %15s %15s %15s %15s\n", "niveau", "graines", "cellules", "changees", "moyenne", "apparition", "dissipation",
			"noir (sante)", "Deathmoth", "Bacteria");
		bool bAllParsed = true;
		bool bMonotone = true;
		for (const FPlan& Plan : Plans)
		{
			FLevelDef D;
			if (!ParseLevel(Src, Plan.Level, D) || D.Assigned < 5)
			{
				bAllParsed = false;
				continue;
			}
			FBalance B;
			for (int K = 1; K <= Plan.Seeds; ++K)
			{
				MeasureSeed(D, SeedFromUser(static_cast<uint32_t>(K), Plan.Level), B);
			}
			char MeanBuf[32], SpawnBuf[32], VanishBuf[32], DarkBuf[32], MothBuf[32], DimBuf[32];
			std::snprintf(MeanBuf, sizeof(MeanBuf), "%.2f -> %.2f", B.SumOld / static_cast<double>(B.Cells > 0 ? B.Cells : 1),
				B.SumNew / static_cast<double>(B.Cells > 0 ? B.Cells : 1));
			std::snprintf(SpawnBuf, sizeof(SpawnBuf), "%.1f -> %.1f %%", Pct(B.SpawnOld, B.Cells), Pct(B.SpawnNew, B.Cells));
			std::snprintf(VanishBuf, sizeof(VanishBuf), "%.1f -> %.1f %%", Pct(B.VanishOld, B.Cells), Pct(B.VanishNew, B.Cells));
			std::snprintf(DarkBuf, sizeof(DarkBuf), "%.1f -> %.1f %%", Pct(B.DarkOld, B.Cells), Pct(B.DarkNew, B.Cells));
			std::snprintf(MothBuf, sizeof(MothBuf), "%.1f -> %.1f %%", Pct(B.MothOld, B.Cells), Pct(B.MothNew, B.Cells));
			std::snprintf(DimBuf, sizeof(DimBuf), "%.1f -> %.1f %%", Pct(B.DimOld, B.Cells), Pct(B.DimNew, B.Cells));
			std::printf("  %-6d %6d %8lld %7.1f%% %13s %15s %15s %15s %15s %15s\n", Plan.Level, B.Seeds, B.Cells, Pct(B.Changed, B.Cells), MeanBuf, SpawnBuf, VanishBuf,
				DarkBuf, MothBuf, DimBuf);
			std::printf("  %-6s plafonniers a portee examines : %lld, caches par la geometrie : %.1f %%, obstacles testes par plafonnier : %.1f\n", "",
				B.Lamps, Pct(B.Hidden, B.Lamps), B.Lamps > 0 ? static_cast<double>(B.Boxes) / static_cast<double>(B.Lamps) : 0.0);
			// Sante mentale d'un joueur qui va partout (courant plein, hors coupures et poursuites) : base du niveau + 0,22 point/s
			// dans le noir sans lampe (0,06 avec la lampe ou la vision nocturne) ; exterieur : la lumiere ne compte pas
			if (!D.bOutdoor)
			{
				const double DarkOld = Pct(B.DarkOld, B.Cells) / 100.0;
				const double DarkNew = Pct(B.DarkNew, B.Cells) / 100.0;
				const double NoLampOld = D.SanityDrain + 0.22 * DarkOld;
				const double NoLampNew = D.SanityDrain + 0.22 * DarkNew;
				const double LampOld = D.SanityDrain + 0.06 * DarkOld;
				const double LampNew = D.SanityDrain + 0.06 * DarkNew;
				char Minutes[64] = "la sante mentale remonte";
				if (NoLampOld > 0.0 && NoLampNew > 0.0)
				{
					std::snprintf(Minutes, sizeof(Minutes), "100 points en %.1f -> %.1f min", 100.0 / NoLampOld / 60.0, 100.0 / NoLampNew / 60.0);
				}
				std::printf("  %-6s sante mentale (moyenne des positions, base %.2f) : sans lampe %.3f -> %.3f point/s (%s) ; avec la lampe %.3f -> %.3f\n", "",
					D.SanityDrain, NoLampOld, NoLampNew, Minutes, LampOld, LampNew);
			}
			// Un mur ne peut qu'enlever de la lumiere : plus de noir, jamais moins
			bMonotone = bMonotone && B.SumNew <= B.SumOld && B.SpawnNew >= B.SpawnOld && B.VanishNew <= B.VanishOld && B.DarkNew >= B.DarkOld && B.MothNew <= B.MothOld;
		}
		Line("parametres des 8 niveaux lus dans BRLevels.cpp", bAllParsed);
		Line("la v4.16 ne fait qu'enlever la lumiere qui traverse la geometrie (jamais plus de lumiere qu'avant)", bMonotone);
		std::printf("\n  Lecture : \"changees\" = part des positions du joueur dont la lumiere differe ; \"apparition\" = cellules ou une entite\n");
		std::printf("  de l'ombre (Smiler) peut apparaitre ; \"dissipation\" = positions ou un Smiler hors poursuite se dissipe ; \"noir\" =\n");
		std::printf("  positions ou la sante mentale baisse plus vite ; \"Deathmoth\" = positions eclairees ou il n'est pas attire par la lampe\n");
		std::printf("  (sa propre hauteur) ; \"Bacteria\" = positions ou elle repere deux fois moins vite un joueur sans lampe.\n");
	}

	void SectionFingerprints(const std::string& Src)
	{
		std::printf("\n## D. Empreintes de la grille de lumiere (cellules -12..11), a comparer avec -BRAutoTestV416Light\n\n");
		struct FPrint
		{
			int Level;
			uint32_t UserSeed;
		};
		const FPrint Prints[] = { { 0, 9u }, { 0, 4242u }, { 1, 4242u }, { 2, 4242u }, { 4, 4242u }, { 5, 4242u }, { 8, 4242u }, { 37, 4242u } };
		bool bOk = true;
		for (const FPrint& Pr : Prints)
		{
			FLevelDef D;
			if (!ParseLevel(Src, Pr.Level, D))
			{
				bOk = false;
				continue;
			}
			const uint32_t LevelSeed = SeedFromUser(Pr.UserSeed, Pr.Level);
			const FPortWorld P(D, LevelSeed);
			FBenchGrid G;
			BuildBenchGrid(P, 12, 0, G);
			std::string Rooms;
			for (const auto& Room : P.BoundedPitRooms)
			{
				char RB[64];
				std::snprintf(RB, sizeof(RB), " [cellules %d,%d a %d,%d]", Room.second.MinX, Room.second.MinY, Room.second.MaxX - 1, Room.second.MaxY - 1);
				Rooms += RB;
			}
			// Comme le journal du jeu (ABRWorld::PrepareLevelLayout) : "<n> porte(s) percee(s) ... <n> salle(s) de fosses [cellules ...]"
			std::printf("  Niveau %-2d -BRSeed=%-5u (graine du niveau %10u) : %08x (%zu porte(s) percee(s), %zu salle(s) de fosses%s)\n", Pr.Level, Pr.UserSeed, LevelSeed,
				Fingerprint(G, 12), P.ForcedDoors.size(), P.BoundedPitRooms.size(), Rooms.c_str());
		}
		Line("empreintes calculees (8 graines)", bOk);
	}
}

int main()
{
	using namespace BRLight;
	std::printf("# Banc de la lumiere de gameplay v4.12 / v4.16 (BRLightLogic.h)\n\n");
	const float Vanish = ReachByScan(BeaconRadius, BeaconIntensity, SmilerVanish);
	const float NoSpawn = ReachByScan(BeaconRadius, BeaconIntensity, NeedsDark);
	const float Gate = ReachByScan(LitGateRadius, LitGateIntensity, SmilerVanish);
	std::printf("  balise : Smiler hors poursuite dissipe a moins de %.1f m ; aucune entite de l'ombre a moins de %.1f m\n", Vanish / 100.f, NoSpawn / 100.f);
	std::printf("  sortie de secours eclairee / porche : Smiler dissipe a moins de %.1f m\n", Gate / 100.f);
	Line("formule de l'indice (R x (1 - racine(seuil / I))) = recherche avec la loi du jeu", std::fabs(Reach(BeaconRadius, BeaconIntensity, SmilerVanish) - Vanish) < 1.f);
	const int Announced = static_cast<int>(std::floor(Reach(BeaconRadius, BeaconIntensity, SmilerVanish) / 100.f));
	char Buf[160];
	std::snprintf(Buf, sizeof(Buf), "indice du carnet : \"a moins de %d m\" ; a %d m l'effet a toujours lieu (apport %.2f > %.2f)", Announced, Announced,
		Contribution(Announced * 100.f, BeaconRadius, BeaconIntensity), SmilerVanish);
	Line(Buf, Contribution(Announced * 100.f, BeaconRadius, BeaconIntensity) > SmilerVanish);
	Line("au-dela de la portee visuelle (9 m), aucun apport", Contribution(BeaconRadius, BeaconRadius, BeaconIntensity) == 0.f && Contribution(1200.f, BeaconRadius, BeaconIntensity) == 0.f);
	Line("apport decroissant avec la distance", Contribution(100.f, BeaconRadius, BeaconIntensity) > Contribution(300.f, BeaconRadius, BeaconIntensity));
	// Deux balises a 8 m l'une de l'autre : le milieu (4 m de chacune) est eclaire au-dessus du seuil
	const float Mid = 2.f * Contribution(400.f, BeaconRadius, BeaconIntensity);
	std::snprintf(Buf, sizeof(Buf), "deux balises a 8 m : milieu a %.2f (somme des apports)", Mid);
	Line(Buf, Mid > SmilerVanish);
	// Coupure : les plafonniers sont multiplies par la puissance (0 en coupure noire), pas les sources de mission
	const float Fixtures = 0.8f, Power = 0.f;
	const float DuringBlackout = Fixtures * Power + Contribution(200.f, BeaconRadius, BeaconIntensity);
	Line("coupure noire : la balise eclaire toujours (source autonome)", DuringBlackout > SmilerVanish);

	SectionPower();
	SectionScenes();
	std::string Src;
	const bool bSrc = ReadFile("Source/Backrooms/Private/BRLevels.cpp", Src);
	Line("Source/Backrooms/Private/BRLevels.cpp lu (le banc s'execute depuis Backrooms/)", bSrc);
	if (bSrc)
	{
		SectionBalance(Src);
		SectionFingerprints(Src);
	}
	std::printf("\nVerifications : %d, echecs : %d\nRESULTAT : %s\n", GChecks, GFailures, GFailures == 0 ? "OK" : "ECHEC");
	return GFailures == 0 ? 0 : 1;
}
