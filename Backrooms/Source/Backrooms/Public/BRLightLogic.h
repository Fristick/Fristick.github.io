// v4.12 : lumiere de gameplay partagee : ce que les regles du jeu (Smilers, apparitions dans l'ombre, sante mentale)
// considerent comme eclaire. Avant la v4.12, seuls les plafonniers comptaient (ABRWorld::LightLevelAt renvoyait 0 dans
// un niveau sans plafonnier) : les balises du Niveau 6, pourtant de vraies lumieres, etaient du noir pour les regles.
//
// Les sources de mission actives (balises allumees, sortie de secours eclairee, porche allume) sont enregistrees aupres
// du monde sur chaque machine, d'apres l'etat replique des mecanismes ; elles sont autonomes (une coupure des neons ne
// les eteint pas). L'apport d'une source suit la meme loi que les plafonniers ; un mur entre la source et le point
// l'annule (trace par ABRWorld). C++ pur : le banc Tools/Light/test_light.cpp verifie les portees annoncees aux joueurs.
//
// v4.16 : la lumiere de gameplay suit la lumiere visible (prompt v4.16, 3.1) :
//   - un plafonnier ne compte plus a travers un mur : BRLight::Occlusion teste le segment de son point d'emission au point
//     vise contre la geometrie que les chunks batissent d'apres la grille partagee (murs epais, jambages et linteau des
//     passages, arches des Poolrooms, poteaux et piliers des sommets, cellules pleines, plafond, sol perce) ; memes donnees
//     chez l'hote et chez les clients (graine, grille), aucune trace physique, independant du rendu (Lumen, MegaLights) ;
//   - pendant une coupure, l'alimentation vue par les regles est le fondu regulier de la phase repliquee (PhasePower), pas
//     le vacillement tire au hasard a chaque image ni le reglage FLASHS du joueur ;
//   - la loi d'apport et les seuils ne changent pas (Contribution, SmilerVanish, NeedsDark ; plafonniers : distance a plat
//     depuis l'ancre de la cellule, x 0,6 pour un neon qui clignote).
// Le banc Tools/Light/test_light.cpp verifie ces regles sur des scenes de controle et mesure l'equilibrage sur des
// centaines de graines ; -BRAutoTestV416Light les rejoue dans Unreal (BRAutoTestV416Light.cpp).
#pragma once

#include <cmath>

namespace BRLight
{
	/** Balise allumee du Niveau 6 (2200 lm, 9 m de portee visuelle) */
	constexpr float BeaconRadius = 900.f;
	constexpr float BeaconIntensity = 2.f;
	/** Sortie de secours eclairee (Niveau 6), porche allume (Niveau 9) */
	constexpr float LitGateRadius = 700.f;
	constexpr float LitGateIntensity = 1.6f;
	/** Seuils lus par les regles : au-dessus, un Smiler hors poursuite se dissipe (BREntity, ThinkSmiler) ; en dessous,
	 *  une entite de l'ombre peut apparaitre (BRWorld, apparitions) */
	constexpr float SmilerVanish = 0.5f;
	constexpr float NeedsDark = 0.12f;

	/** Apport d'une source a une distance (cm) : (1 - d/R)^2 x intensite, nul au-dela du rayon */
	inline float Contribution(float Dist, float Radius, float Intensity)
	{
		if (Radius <= 0.f || Dist >= Radius)
		{
			return 0.f;
		}
		const float T = 1.f - Dist / Radius;
		return T * T * Intensity;
	}

	/** v4.16 : un plafonnier qui clignote compte pour 60 % (regle d'avant, inchangee) */
	constexpr float FlickerWeight = 0.6f;

	/** v4.16 : phases d'une coupure de courant (ABRWorld, EBlackout) et durees des vacillements (ABRWorld::EnterBlackoutPhase) */
	constexpr int PhaseNone = 0;
	constexpr int PhaseFailing = 1;
	constexpr int PhaseDark = 2;
	constexpr int PhaseRestoring = 3;
	constexpr float FailSeconds = 1.8f;
	constexpr float RestoreSeconds = 1.4f;

	/** v4.16 : alimentation des plafonniers pour les regles : fondu regulier de la phase (celui du reglage FLASHS "aucun"),
	 *  d'apres la phase et le temps qui lui reste (Remaining). Le vacillement visible est tire au hasard chez chaque joueur
	 *  et depend de son reglage FLASHS : il ne decide plus rien. Failing : 1 -> 0 ; Dark : 0 ; Restoring : 0 -> 1 ; sinon 1 */
	inline float PhasePower(int Phase, float Remaining)
	{
		if (Phase == PhaseDark)
		{
			return 0.f;
		}
		if (Phase != PhaseFailing && Phase != PhaseRestoring)
		{
			return 1.f;
		}
		float T = Remaining / (Phase == PhaseFailing ? FailSeconds : RestoreSeconds);
		T = T < 0.f ? 0.f : (T > 1.f ? 1.f : T);
		return Phase == PhaseFailing ? T : 1.f - T;
	}

	/** v4.16 : etat lisible de la lumiere qui compte (inventaire) : 0 dans le noir (sous NeedsDark), 1 penombre, 2 eclaire
	 *  (au-dessus de SmilerVanish) */
	inline int Band(float Light)
	{
		return Light < NeedsDark ? 0 : (Light > SmilerVanish ? 2 : 1);
	}

	/** v4.16 : occlusion des plafonniers par la grille partagee. Le monde (ou le banc) fournit la grille par un objet
	 *  GridT qui repond a :
	 *    unsigned char EdgeE(int X, int Y) const   arete entre (X, Y) et (X + 1, Y) : EdgeOpen, EdgeWall ou EdgeDoor
	 *    unsigned char EdgeN(int X, int Y) const   arete entre (X, Y) et (X, Y + 1)
	 *    bool SolidBox(int X, int Y, double Lo[3], double Hi[3]) const   bloc d'une cellule pleine (false : cellule libre)
	 *    bool Pillar(int X, int Y) const           pilier au sommet (+X, +Y) de la cellule
	 *    bool FloorOpen(double X, double Y) const  sol ouvert en ce point (fosse, bassin)
	 *    bool Lamp(int X, int Y, FLamp& Out) const plafonnier de la cellule
	 *  Coordonnees en cm, sol a Z = 0. Les murs sont centres sur les aretes, epais de Thick, de toute la hauteur ; chaque mur
	 *  ou passage couvre aussi le carre du sommet a ses deux bouts (les chunks prolongent les pans d'un demi-mur) ; un passage
	 *  (EdgeDoor) a deux jambages, une ouverture de 2 x DoorHalf centree sur l'arete et un linteau a DoorTop (ou une arche). */
	namespace Occlusion
	{
		constexpr unsigned char EdgeOpen = 0;
		constexpr unsigned char EdgeWall = 1;
		constexpr unsigned char EdgeDoor = 2;

		struct FVec
		{
			double X = 0.0;
			double Y = 0.0;
			double Z = 0.0;
		};

		/** Cotes du niveau, celles que batissent les chunks (ABRChunk::AddWallSegment, AddDoorway, piliers, cellules pleines) */
		struct FGeom
		{
			double Cell = 350.0;
			double Thick = 60.0;
			/** Hauteur sous plafond (murs) */
			double Height = 290.0;
			/** Demi-largeur de l'ouverture d'un passage : min(DoorWidth, Cell - 2 Thick - 20) / 2 */
			double DoorHalf = 75.0;
			/** Dessous du linteau ; tres grand : passage sans linteau */
			double DoorTop = 225.0;
			/** Arche en plein cintre (Poolrooms) : ouverture droite jusqu'a ArchSpring, puis demi-cercle de rayon DoorHalf */
			bool bArch = false;
			double ArchSpring = 0.0;
			/** Demi-cote des piliers (0 : aucun) */
			double PillarHalf = 0.0;
			/** Le niveau a un plafond (un point au-dessus ne voit aucun plafonnier) */
			bool bCeiling = true;
			/** Le point vise est une petite sphere : un obstacle qui ne commence que dans ses derniers EndSlack cm ne le cache pas
			 *  (point colle a un mur ou pris dans son epaisseur cote lampe : eclaire comme la face qu'il touche) */
			double EndSlack = 12.0;
		};

		/** Compteurs (cout, journal des tests) */
		struct FStats
		{
			int Lamps = 0;
			int Hidden = 0;
			int Cells = 0;
			int Boxes = 0;
		};

		/** Plafonnier d'une cellule : allume (present, pas casse), clignotant, ancre de la loi d'apport (centre + decalage, la
		 *  distance a plat d'avant la v4.16) et point d'emission (ou est la lumiere : ABRChunk::AddLight) */
		struct FLamp
		{
			bool bOn = false;
			bool bFlicker = false;
			double AX = 0.0;
			double AY = 0.0;
			FVec Emitter;
		};

		constexpr double Huge = 1.0e9;

		/** Cotes du niveau d'apres sa definition (FBRLevelDef), avec les regles de construction des chunks : passage de
		 *  min(DoorWidth, Cell - 2 Thick - 20) ; linteau a min(225, H - 25) s'il reste au moins 15 cm au-dessus (sinon aucun) ;
		 *  arche des Poolrooms (sommet ArchApex, ou H - 60 ; naissance sommet - demi-largeur) si elle tient (naissance au-dessus
		 *  de 1,5 m, haut sous le plafond), sinon le linteau droit : la regle seule, que le modele de l'arche soit charge ou non ;
		 *  piliers de PillarSize (bPillars : le niveau en a). Meme calcul dans le jeu et dans le banc */
		inline FGeom MakeGeom(double Cell, double Thick, double Height, double DoorWidth, bool bLintels, bool bArches, double ArchApex, double PillarSize,
			bool bPillars, bool bCeiling)
		{
			FGeom G;
			G.Cell = Cell;
			G.Thick = Thick;
			G.Height = Height;
			const double DW = DoorWidth < Cell - 2.0 * Thick - 20.0 ? DoorWidth : Cell - 2.0 * Thick - 20.0;
			G.DoorHalf = DW * 0.5;
			G.DoorTop = Huge;
			if (bLintels)
			{
				const double Top = 225.0 < Height - 25.0 ? 225.0 : Height - 25.0;
				if (Height - Top >= 15.0)
				{
					G.DoorTop = Top;
				}
				if (bArches)
				{
					const double Apex = ArchApex > 0.0 ? (ArchApex < Height - 20.0 ? ArchApex : Height - 20.0) : Height - 60.0;
					const double Spring = Apex - DW * 0.5;
					if (Spring > 150.0 && Spring + DW * 0.52 < Height)
					{
						G.bArch = true;
						G.ArchSpring = Spring;
					}
				}
			}
			G.PillarHalf = bPillars ? PillarSize * 0.5 : 0.0;
			G.bCeiling = bCeiling;
			return G;
		}

		/** Point d'emission d'un plafonnier (les cotes d'ABRChunk::AddLight) : Fixture = EBRFixture (1 dalle, 2 tube, 3 ampoule,
		 *  4 applique, 5 dalle ovale des Poolrooms, 6 lampadaire) ; Base : centre de la cellule + decalage (sol a 0) ; l'applique
		 *  et le lampadaire regardent a l'oppose de leur decalage (le mur, le bord de la rue). False : pas de plafonnier */
		inline bool Emitter(int Fixture, double Height, double BaseX, double BaseY, double OffX, double OffY, FVec& Out)
		{
			const double OffLen = std::sqrt(OffX * OffX + OffY * OffY);
			const double FX = OffLen > 1.0e-6 ? -OffX / OffLen : 0.0;
			const double FY = OffLen > 1.0e-6 ? -OffY / OffLen : 0.0;
			Out.X = BaseX;
			Out.Y = BaseY;
			switch (Fixture)
			{
			case 1: // dalle
			case 3: // ampoule
				Out.Z = Height - 25.0;
				return true;
			case 2: // tube
				Out.Z = Height - 70.0;
				return true;
			case 5: // dalle ovale (Poolrooms)
				Out.Z = Height - 40.0;
				return true;
			case 4: // applique : 30 cm devant le mur, a 2,1 m
				Out.X = BaseX + FX * 30.0;
				Out.Y = BaseY + FY * 30.0;
				Out.Z = 210.0;
				return true;
			case 6: // lampadaire : 1,3 m vers la rue, a 5,6 m
				Out.X = BaseX + FX * 130.0;
				Out.Y = BaseY + FY * 130.0;
				Out.Z = 560.0;
				return true;
			default:
				return false;
			}
		}

		/** Intervalle [T0, T1] inclus dans [0, TMax] ou le point A + t D est dans la boite [Lo, Hi] ; false s'il est vide */
		inline bool Clip(const FVec& A, const FVec& D, const double Lo[3], const double Hi[3], double TMax, double& T0, double& T1)
		{
			T0 = 0.0;
			T1 = TMax;
			const double O[3] = { A.X, A.Y, A.Z };
			const double V[3] = { D.X, D.Y, D.Z };
			for (int I = 0; I < 3; ++I)
			{
				if (std::fabs(V[I]) < 1.0e-12)
				{
					if (O[I] < Lo[I] || O[I] > Hi[I])
					{
						return false;
					}
					continue;
				}
				double TA = (Lo[I] - O[I]) / V[I];
				double TB = (Hi[I] - O[I]) / V[I];
				if (TA > TB)
				{
					const double Swap = TA;
					TA = TB;
					TB = Swap;
				}
				T0 = TA > T0 ? TA : T0;
				T1 = TB < T1 ? TB : T1;
				if (T0 > T1)
				{
					return false;
				}
			}
			return true;
		}

		/** Le segment traverse la boite sur une longueur non nulle (un contact ne cache rien) */
		inline bool Hits(const FVec& A, const FVec& D, const double Lo[3], const double Hi[3], double TMax, FStats* Stats)
		{
			if (Stats)
			{
				++Stats->Boxes;
			}
			double T0 = 0.0;
			double T1 = 0.0;
			return Clip(A, D, Lo, Hi, TMax, T0, T1) && T1 - T0 > 1.0e-9;
		}

		/** Point (U : ecart le long de l'arete depuis le milieu du passage, Z : hauteur) dans l'ouverture d'un passage */
		inline bool InOpening(const FGeom& G, double U, double Z)
		{
			const double R = G.DoorHalf + 0.01;
			if (U < -R || U > R)
			{
				return false;
			}
			if (G.bArch)
			{
				if (Z <= G.ArchSpring)
				{
					return true;
				}
				const double DZ = Z - G.ArchSpring;
				return U * U + DZ * DZ <= R * R;
			}
			return Z <= G.DoorTop + 0.01;
		}

		/** Arete (mur ou passage) le long de Y (bAlongY : arete Est, ligne X = Line) ou de X (arete Nord, ligne Y = Line), de
		 *  Start a Start + Cell : coupe-t-elle le segment ? Le carre des sommets est teste a part */
		inline bool EdgeBlocks(const FGeom& G, unsigned char Kind, bool bAlongY, double Line, double Start, const FVec& A, const FVec& D,
			double TMax, FStats* Stats)
		{
			if (Kind == EdgeOpen)
			{
				return false;
			}
			const double HT = G.Thick * 0.5;
			auto Box = [&](double From, double To, double ZLo, double ZHi)
			{
				if (To - From <= 0.0)
				{
					return false;
				}
				const double Lo[3] = { bAlongY ? Line - HT : From, bAlongY ? From : Line - HT, ZLo };
				const double Hi[3] = { bAlongY ? Line + HT : To, bAlongY ? To : Line + HT, ZHi };
				return Hits(A, D, Lo, Hi, TMax, Stats);
			};
			const double CoreLo = Start + HT;
			const double CoreHi = Start + G.Cell - HT;
			if (Kind != EdgeDoor)
			{
				return Box(CoreLo, CoreHi, -Huge, Huge);
			}
			const double Mid = Start + G.Cell * 0.5;
			if (Box(CoreLo, Mid - G.DoorHalf, -Huge, Huge) || Box(Mid + G.DoorHalf, CoreHi, -Huge, Huge))
			{
				return true; // jambage
			}
			// Ouverture : la partie du segment dans le passage (epaisseur du mur x largeur de l'ouverture) doit rester sous le
			// linteau ou l'arche. L'ouverture est convexe : ses deux extremites suffisent
			const double Lo[3] = { bAlongY ? Line - HT : Mid - G.DoorHalf, bAlongY ? Mid - G.DoorHalf : Line - HT, -Huge };
			const double Hi[3] = { bAlongY ? Line + HT : Mid + G.DoorHalf, bAlongY ? Mid + G.DoorHalf : Line + HT, Huge };
			double T0 = 0.0;
			double T1 = 0.0;
			if (Stats)
			{
				++Stats->Boxes;
			}
			if (!Clip(A, D, Lo, Hi, TMax, T0, T1) || T1 - T0 <= 1.0e-9)
			{
				return false;
			}
			const double Ends[2] = { T0, T1 };
			for (int I = 0; I < 2; ++I)
			{
				const double U = (bAlongY ? A.Y + D.Y * Ends[I] : A.X + D.X * Ends[I]) - Mid;
				if (!InOpening(G, U, A.Z + D.Z * Ends[I]))
				{
					return true; // linteau ou arche
				}
			}
			return false;
		}

		/** La lumiere emise en A atteint-elle B ? (voir l'en-tete du namespace) */
		template <typename GridT>
		bool Visible(const GridT& Grid, const FGeom& G, const FVec& A, const FVec& B, FStats* Stats = nullptr)
		{
			const FVec D{ B.X - A.X, B.Y - A.Y, B.Z - A.Z };
			const double Len = std::sqrt(D.X * D.X + D.Y * D.Y + D.Z * D.Z);
			if (Len <= G.EndSlack)
			{
				return true;
			}
			const double TMax = 1.0 - G.EndSlack / Len;
			// Plafond : un point au-dessus ne voit aucun plafonnier
			if (G.bCeiling && B.Z > G.Height + 1.0)
			{
				return false;
			}
			// Sol : un point sous le sol (fosse, bassin) ne voit la lampe qu'a travers l'ouverture du sol
			if (B.Z < -1.0 && A.Z > 0.0)
			{
				const double T = A.Z / (A.Z - B.Z);
				if (!Grid.FloorOpen(A.X + D.X * T, A.Y + D.Y * T))
				{
					return false;
				}
			}
			const double S = G.Cell;
			const double HT = G.Thick * 0.5;
			const double M = (HT > G.PillarHalf ? HT : G.PillarHalf) + 1.0;
			const int X0 = static_cast<int>(std::floor(((A.X < B.X ? A.X : B.X) - M) / S));
			const int X1 = static_cast<int>(std::floor(((A.X > B.X ? A.X : B.X) + M) / S));
			const int Y0 = static_cast<int>(std::floor(((A.Y < B.Y ? A.Y : B.Y) - M) / S));
			const int Y1 = static_cast<int>(std::floor(((A.Y > B.Y ? A.Y : B.Y) + M) / S));
			for (int X = X0; X <= X1; ++X)
			{
				for (int Y = Y0; Y <= Y1; ++Y)
				{
					// Cellule elargie (ses aretes Est et Nord, son sommet (+X, +Y)) traversee par le segment ?
					const double CLo[3] = { X * S - M, Y * S - M, -Huge };
					const double CHi[3] = { (X + 1) * S + M, (Y + 1) * S + M, Huge };
					double T0 = 0.0;
					double T1 = 0.0;
					if (!Clip(A, D, CLo, CHi, TMax, T0, T1))
					{
						continue;
					}
					if (Stats)
					{
						++Stats->Cells;
					}
					double Lo[3] = { 0.0, 0.0, 0.0 };
					double Hi[3] = { 0.0, 0.0, 0.0 };
					if (Grid.SolidBox(X, Y, Lo, Hi) && Hits(A, D, Lo, Hi, TMax, Stats))
					{
						return false;
					}
					const unsigned char E = Grid.EdgeE(X, Y);
					const unsigned char N = Grid.EdgeN(X, Y);
					if (EdgeBlocks(G, E, true, (X + 1) * S, Y * S, A, D, TMax, Stats) || EdgeBlocks(G, N, false, (Y + 1) * S, X * S, A, D, TMax, Stats))
					{
						return false;
					}
					// Sommet (+X, +Y) : carre d'un demi-mur si un mur ou un passage y arrive, pilier
					double H = 0.0;
					if (E != EdgeOpen || N != EdgeOpen || Grid.EdgeE(X, Y + 1) != EdgeOpen || Grid.EdgeN(X + 1, Y) != EdgeOpen)
					{
						H = HT;
					}
					if (G.PillarHalf > H && Grid.Pillar(X, Y))
					{
						H = G.PillarHalf;
					}
					if (H > 0.0)
					{
						const double VX = (X + 1) * S;
						const double VY = (Y + 1) * S;
						const double VLo[3] = { VX - H, VY - H, -Huge };
						const double VHi[3] = { VX + H, VY + H, Huge };
						if (Hits(A, D, VLo, VHi, TMax, Stats))
						{
							return false;
						}
					}
				}
			}
			return true;
		}

		/** Plafonniers autour de P : meme parcours et meme loi qu'avant la v4.16 (cellules a +-R, R = Radius / Cell arrondi au-
		 *  dessus et borne a 1..4, distance a plat a l'ancre, apport (1 - d / Radius)^2, x 0,6 s'il clignote). OutAll : somme
		 *  de tous (la regle d'avant, murs ignores) ; OutVisible : des seuls plafonniers dont la lumiere atteint P. Sans
		 *  l'alimentation (multipliee par l'appelant) */
		template <typename GridT>
		void CeilingSum(const GridT& Grid, const FGeom& G, float Radius, const FVec& P, float& OutAll, float& OutVisible, FStats* Stats = nullptr)
		{
			OutAll = 0.f;
			OutVisible = 0.f;
			const float S = static_cast<float>(G.Cell);
			const int CX = static_cast<int>(std::floor(P.X / G.Cell));
			const int CY = static_cast<int>(std::floor(P.Y / G.Cell));
			int R = static_cast<int>(std::ceil(Radius / S));
			R = R < 1 ? 1 : (R > 4 ? 4 : R);
			for (int DX = -R; DX <= R; ++DX)
			{
				for (int DY = -R; DY <= R; ++DY)
				{
					FLamp L;
					if (!Grid.Lamp(CX + DX, CY + DY, L) || !L.bOn)
					{
						continue;
					}
					const double QX = L.AX - P.X;
					const double QY = L.AY - P.Y;
					const float Dist = static_cast<float>(std::sqrt(QX * QX + QY * QY));
					if (!(Dist < Radius))
					{
						continue;
					}
					const float K = 1.f - Dist / Radius;
					const float W = K * K * (L.bFlicker ? FlickerWeight : 1.f);
					OutAll += W;
					if (Stats)
					{
						++Stats->Lamps;
					}
					if (Visible(Grid, G, L.Emitter, P, Stats))
					{
						OutVisible += W;
					}
					else if (Stats)
					{
						++Stats->Hidden;
					}
				}
			}
		}
	}
}
