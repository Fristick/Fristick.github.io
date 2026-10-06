// v4.11 : logique des missions de niveau (voir BRMissionLogic.h). Aucune dependance au moteur : ce fichier est aussi
// compile seul par Tools/Missions/test_mission_logic.cpp.
#include "BRMissionLogic.h"

namespace BRMission
{
namespace Impl
{
	inline uint32_t Mix(uint32_t X)
	{
		X ^= X >> 16;
		X *= 0x7feb352dU;
		X ^= X >> 15;
		X *= 0x846ca68bU;
		X ^= X >> 16;
		return X;
	}

	struct FRng
	{
		uint32_t S;
		FRng(uint32_t Seed, uint32_t Salt) : S(Hash(Seed, Salt)) {}
		uint32_t Next()
		{
			S = Mix(S + 0x9e3779b9U);
			return S;
		}
		int Range(int N) { return N <= 1 ? 0 : (int)(Next() % (uint32_t)N); }
		void Shuffle(uint8_t* A, int N)
		{
			for (int I = N - 1; I > 0; --I)
			{
				const int J = Range(I + 1);
				const uint8_t T = A[I];
				A[I] = A[J];
				A[J] = T;
			}
		}
	};

	inline uint8_t U8(int V) { return (uint8_t)(V < 0 ? 0 : (V > 255 ? 255 : V)); }

	int Add(FPlan& P, EKind Kind, uint8_t Role, int Label, int Positions, int Zone, int Group, bool bOptional = false)
	{
		if (P.NumDevices >= MaxDevices)
		{
			return MaxDevices - 1;
		}
		FDevice& D = P.Devices[P.NumDevices];
		D = FDevice();
		D.Kind = Kind;
		D.Role = Role;
		D.Label = U8(Label);
		D.Positions = U8(Positions < 1 ? 1 : Positions);
		D.Zone = U8(Zone);
		D.Group = U8(Group);
		D.bOptional = bOptional;
		return P.NumDevices++;
	}

	// Indices des appareils par niveau (l'ordre de BuildLevel)
	enum : int
	{
		// Niveau 0 : trois anomalies, note, trois cadrans, bouton, levier de route, passage
		L0_Anom = 0, L0_Note = 3, L0_Dial = 4, L0_Stab = 7, L0_Lever = 8, L0_Pass = 9,
		// Niveau 1 : schema, trois fusibles, prises A-E, disjoncteurs A-E, appel, reserve, note de reserve
		L1_Schem = 0, L1_Fuse = 1, L1_Sock = 4, L1_Brk = 9, L1_Call = 14, L1_ResLights = 15, L1_ResNote = 16,
		// Niveau 2 : plaque, trois manometres, trois vannes, porte de vapeur
		L2_Plate = 0, L2_Gauge = 1, L2_Valve = 4, L2_Door = 7,
		// Niveau 3 : tableau, quatre boites (secteurs A-D), quatre relais (1-4), alimentation
		L3_Board = 0, L3_Box = 1, L3_Relay = 5, L3_Power = 9,
		// Niveau 4 : planning, annuaire, archive, trois cadrans, validation, acces
		L4_Plan = 0, L4_Dir = 1, L4_Arch = 2, L4_Dial = 3, L4_Enter = 6, L4_Access = 7,
		// Niveau 5 : registre, cinq cles, trois serrures, consigne, cadran, passage, note du personnel
		L5_Reg = 0, L5_Key = 1, L5_Lock = 6, L5_Note = 9, L5_Dial = 10, L5_Pass = 11, L5_Staff = 12,
		// Niveau 6 : plaque, quatre balises, alimentation, sortie
		L6_Start = 0, L6_Beacon = 1, L6_Power = 5, L6_Exit = 6,
		// Niveau 8 : marques, cinq treuils (dont deux grippes), passerelle
		L8_Marks = 0, L8_Winch = 1, L8_Bridge = 6,
		// Niveau 9 : trois plans, boitier de rue, trois maisons, porte
		L9_Plan = 0, L9_Box = 3, L9_House = 4, L9_Door = 7,
		// Niveau 10 : marque de cloture, tableau des granges, moulin, frein du moulin, porte
		L10_Fence = 0, L10_Board = 1, L10_Mill = 2, L10_Brake = 3, L10_Door = 4,
		// Niveau 11 : generateur, trois panneaux, trois cadrans, confirmation, portique
		L11_Gen = 0, L11_Board = 1, L11_Dial = 4, L11_Confirm = 7, L11_Gate = 8,
		// Niveau 37 : marques, courant, vannes A et B, passage, carnet de plongee
		L37_Marks = 0, L37_Current = 1, L37_SluiceA = 2, L37_SluiceB = 3, L37_Pass = 4, L37_Log = 5
	};

	// Parametres (Params) par niveau :
	//   0 : [0..2] clignotements des anomalies, [3..5] valeur voulue de chaque cadran
	//   1 : [0] [1] circuits de l'ascenseur, [2] circuit de la reserve, [3] [4] circuits sans fusible
	//   2 : [0..2] pression voulue par manometre, [3..5] vanne de chaque manometre, [6..8] cible de chaque vanne
	//   3 : [0] secteur en defaut, [1..4] secteur de chaque relais, [5] relais en defaut
	//   4 : [0..2] symboles du planning, [3..8] numero de bureau par symbole
	//   5 : [0..2] symbole de chaque serrure, [3..5] cle de chaque serrure, [6] pression, [7..11] chambre de chaque cle
	//   6 : [0..2] balises de la chaine (indices d'appareil), [3] leurre, [4..7] symbole de chaque balise
	//   8 : [0..2] ordre des treuils (lettres), [3] [4] treuils grippes
	//   9 : [0] circuit (1-4), [1] porche, [2] fenetres, [3] maison correcte (indice d'appareil)
	//  10 : [0] grange cible, [1..3] symbole des granges, [4..6] direction des granges
	//  11 : [0..2] chiffres de la destination
	//  37 : [0] niveau voulu du bassin A, [1] du bassin B, [2] niveau de depart du bassin B

	bool BuildLevel(int Level, FRng& R, FPlan& P)
	{
		P.NumDevices = 0;
		P.NumSteps = 0;
		P.bHasRoute = false;
		for (int I = 0; I < MaxParams; ++I)
		{
			P.Params[I] = 0;
		}
		uint8_t Sym[NumSymbols] = { 0, 1, 2, 3, 4, 5 };
		R.Shuffle(Sym, NumSymbols);
		switch (Level)
		{
		case 0:
		{
			uint8_t Zones[3] = { 1, 2, 3 };
			R.Shuffle(Zones, 3);
			for (int I = 0; I < 3; ++I)
			{
				const int D = Add(P, EKind::Observe, R_Anomaly, Sym[I], 4, Zones[I], 0);
				P.Params[I] = (uint8_t)(1 + R.Range(7));
				P.Devices[D].Info[0] = P.Params[I];
			}
			Add(P, EKind::Clue, R_MaintNote, 0, 1, 0, 0);
			uint8_t Ord[3] = { 0, 1, 2 };
			R.Shuffle(Ord, 3);
			for (int I = 0; I < 3; ++I)
			{
				Add(P, EKind::Switch, R_Dial, Sym[Ord[I]], 8, 4, 1);
				P.Params[3 + I] = P.Params[Ord[I]];
			}
			Add(P, EKind::Button, R_Stabilize, 0, 1, 4, 1);
			Add(P, EKind::Switch, R_RouteLever, 0, 2, 4, 1);
			Add(P, EKind::Gate, R_Passage, 0, 1, 4, 1);
			P.NumSteps = 3;
			P.bHasRoute = true;
			// Les trois anomalies identiques rendraient le panneau trivial
			return !(P.Params[0] == P.Params[1] && P.Params[1] == P.Params[2]);
		}
		case 1:
		{
			uint8_t Letters[5] = { 0, 1, 2, 3, 4 };
			R.Shuffle(Letters, 5);
			for (int I = 0; I < 5; ++I)
			{
				P.Params[I] = Letters[I];
			}
			Add(P, EKind::Clue, R_Schematic, 0, 1, 0, 0);
			uint8_t Zones[3] = { 1, 2, 3 };
			R.Shuffle(Zones, 3);
			for (int I = 0; I < 3; ++I)
			{
				const int D = Add(P, EKind::Item, R_Fuse, Letters[I], 1, Zones[I], 0);
				P.Devices[D].Need = Letters[I];
			}
			for (int I = 0; I < 5; ++I)
			{
				const int D = Add(P, EKind::Socket, R_FuseSocket, I, 1, 4, 1);
				P.Devices[D].Need = (uint8_t)I;
			}
			for (int I = 0; I < 5; ++I)
			{
				Add(P, EKind::Switch, R_Breaker, I, 2, 4, 1);
			}
			Add(P, EKind::Button, R_ElevatorCall, 0, 1, 4, 1);
			Add(P, EKind::Gate, R_ReserveLights, Letters[2], 1, 2, 2);
			Add(P, EKind::Clue, R_ReserveNote, 0, 1, 2, 2, true);
			P.NumSteps = 3;
			return true;
		}
		case 2:
		{
			uint8_t Feed[3] = { 0, 1, 2 };
			R.Shuffle(Feed, 3);
			Add(P, EKind::Clue, R_PressurePlate, 0, 1, 0, 0);
			uint8_t Zones[3] = { 1, 2, 3 };
			R.Shuffle(Zones, 3);
			for (int G = 0; G < 3; ++G)
			{
				P.Params[G] = (uint8_t)(1 + R.Range(4));
				P.Params[3 + G] = Feed[G];
				P.Params[6 + Feed[G]] = P.Params[G];
				const int D = Add(P, EKind::Observe, R_Gauge, G, 2, Zones[G], 0);
				P.Devices[D].Info[0] = Feed[G];
			}
			for (int V = 0; V < 3; ++V)
			{
				Add(P, EKind::Switch, R_Valve, V, 5, 4, 1);
			}
			Add(P, EKind::Gate, R_SteamDoor, 0, 1, 4, 1);
			P.NumSteps = 3;
			return true;
		}
		case 3:
		{
			// Le tableau de charge dit quel relais alimente quel secteur ; les boites de jonction montrent le defaut
			uint8_t Map[4] = { 0, 1, 2, 3 };
			R.Shuffle(Map, 4);
			P.Params[0] = (uint8_t)R.Range(4);
			for (int Relay = 0; Relay < 4; ++Relay)
			{
				P.Params[1 + Relay] = Map[Relay];
				if (Map[Relay] == P.Params[0])
				{
					P.Params[5] = (uint8_t)Relay;
				}
			}
			Add(P, EKind::Clue, R_LoadBoard, 0, 1, 0, 0);
			uint8_t Zones[4] = { 1, 2, 3, 3 };
			R.Shuffle(Zones, 4);
			for (int S = 0; S < 4; ++S)
			{
				const int D = Add(P, EKind::Observe, R_JunctionBox, S, 2, Zones[S], 0);
				P.Devices[D].Info[0] = (uint8_t)(S == P.Params[0] ? 1 : 0);
			}
			for (int S = 0; S < 4; ++S)
			{
				Add(P, EKind::Switch, R_Relay, S, 2, 4, 1);
			}
			Add(P, EKind::Button, R_ElevatorPower, 0, 1, 4, 1);
			P.NumSteps = 4;
			return true;
		}
		case 4:
		{
			for (int I = 0; I < 3; ++I)
			{
				P.Params[I] = Sym[I];
			}
			for (int S = 0; S < NumSymbols; ++S)
			{
				P.Params[3 + S] = (uint8_t)R.Range(10);
			}
			Add(P, EKind::Clue, R_Planning, 0, 1, 1, 0);
			Add(P, EKind::Clue, R_Directory, 0, 1, 2, 0);
			Add(P, EKind::Clue, R_Archive, 0, 1, 3, 0, true);
			for (int I = 0; I < 3; ++I)
			{
				Add(P, EKind::Switch, R_CodeDial, I, 10, 4, 1);
			}
			Add(P, EKind::Button, R_CodeEnter, 0, 1, 4, 1);
			Add(P, EKind::Gate, R_HotelAccess, 0, 1, 4, 1);
			P.NumSteps = 3;
			// Un code 000 se trouverait sans rien lire (cadrans au repos)
			return !(P.Params[3 + Sym[0]] == 0 && P.Params[3 + Sym[1]] == 0 && P.Params[3 + Sym[2]] == 0);
		}
		case 5:
		{
			// Cinq chambres distinctes (101..130), trois cles utiles parmi cinq
			uint8_t Rooms[30];
			for (int I = 0; I < 30; ++I)
			{
				Rooms[I] = (uint8_t)(1 + I);
			}
			R.Shuffle(Rooms, 30);
			uint8_t KeyOrd[5] = { 0, 1, 2, 3, 4 };
			R.Shuffle(KeyOrd, 5);
			Add(P, EKind::Clue, R_Register, 0, 1, 0, 0);
			uint8_t Zones[5] = { 1, 1, 2, 3, 3 };
			R.Shuffle(Zones, 5);
			for (int K = 0; K < 5; ++K)
			{
				const int D = Add(P, EKind::Item, R_Key, Rooms[K], 1, Zones[K], 0);
				P.Devices[D].Need = (uint8_t)K;
				P.Params[7 + K] = Rooms[K];
			}
			for (int L = 0; L < 3; ++L)
			{
				P.Params[L] = Sym[L];
				P.Params[3 + L] = KeyOrd[L];
				const int D = Add(P, EKind::Socket, R_Lock, Sym[L], 1, 4, 1);
				P.Devices[D].Need = KeyOrd[L];
			}
			P.Params[6] = (uint8_t)(1 + R.Range(5));
			Add(P, EKind::Clue, R_BoilerNote, 0, 1, 4, 1);
			Add(P, EKind::Switch, R_BoilerDial, 0, 6, 4, 1);
			Add(P, EKind::Gate, R_BoilerPassage, 0, 1, 4, 1);
			Add(P, EKind::Clue, R_StaffNote, 0, 1, 2, 0, true);
			P.NumSteps = 3;
			return true;
		}
		case 6:
		{
			uint8_t Chain[4] = { 0, 1, 2, 3 };
			R.Shuffle(Chain, 4);
			Add(P, EKind::Clue, R_StartPlate, 0, 1, 0, 0);
			uint8_t Zones[4] = { 1, 2, 3, 2 };
			R.Shuffle(Zones, 4);
			for (int B = 0; B < 4; ++B)
			{
				Add(P, EKind::Crank, R_Beacon, Sym[B], 3, Zones[B], 0);
				P.Params[4 + B] = Sym[B];
			}
			for (int I = 0; I < 3; ++I)
			{
				P.Params[I] = (uint8_t)(L6_Beacon + Chain[I]);
				P.Devices[L6_Beacon + Chain[I]].Info[0] = I < 2 ? Sym[Chain[I + 1]] : (uint8_t)254;
			}
			P.Params[3] = (uint8_t)(L6_Beacon + Chain[3]);
			P.Devices[L6_Beacon + Chain[3]].Info[0] = 255;
			Add(P, EKind::Button, R_EmergencyPower, 0, 1, 4, 1);
			Add(P, EKind::Gate, R_LightsExit, 0, 1, 4, 1);
			P.NumSteps = 3;
			return true;
		}
		case 8:
		{
			uint8_t Ord[5] = { 0, 1, 2, 3, 4 };
			R.Shuffle(Ord, 5);
			for (int I = 0; I < 5; ++I)
			{
				P.Params[I] = Ord[I];
			}
			Add(P, EKind::Clue, R_PassageMarks, 0, 1, 1, 0);
			uint8_t Zones[5] = { 2, 3, 4, 3, 2 };
			R.Shuffle(Zones, 5);
			for (int W = 0; W < 5; ++W)
			{
				Add(P, EKind::Crank, R_Winch, W, 4, Zones[W], 0);
			}
			Add(P, EKind::Gate, R_Bridge, 0, 1, 4, 1);
			P.NumSteps = 2;
			return true;
		}
		case 9:
		{
			P.Params[0] = (uint8_t)(1 + R.Range(4));
			P.Params[1] = Sym[0];
			P.Params[2] = (uint8_t)(2 + R.Range(4));
			uint8_t Zones[3] = { 1, 2, 3 };
			R.Shuffle(Zones, 3);
			for (int I = 0; I < 3; ++I)
			{
				Add(P, EKind::Clue, R_HousePlan, I, 1, Zones[I], 0);
			}
			// Position 0 : boitier deconnecte
			Add(P, EKind::Switch, R_StreetBox, 0, 5, 2, 0);
			// Maison correcte : porche et fenetres ; leurres : un seul des deux traits
			const uint8_t OtherWindows = (uint8_t)(2 + (P.Params[2] - 2 + 1 + R.Range(3)) % 4);
			uint8_t Ord[3] = { 0, 1, 2 };
			R.Shuffle(Ord, 3);
			const uint8_t HouseZones[3] = { 4, 2, 3 };
			for (int I = 0; I < 3; ++I)
			{
				const int Kind = Ord[I];
				const uint8_t Porch = Kind == 2 ? Sym[1] : Sym[0];
				const uint8_t Windows = Kind == 1 ? OtherWindows : P.Params[2];
				const int D = Add(P, EKind::Button, R_HouseMarker, Porch, 1, HouseZones[Kind], 0);
				P.Devices[D].Info[0] = Windows;
				if (Kind == 0)
				{
					P.Params[3] = (uint8_t)D;
				}
			}
			Add(P, EKind::Gate, R_HouseDoor, 0, 1, 4, 0);
			P.NumSteps = 3;
			return true;
		}
		case 10:
		{
			uint8_t Dirs[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
			R.Shuffle(Dirs, 8);
			P.Params[0] = (uint8_t)R.Range(3);
			for (int B = 0; B < 3; ++B)
			{
				P.Params[1 + B] = Sym[B];
				P.Params[4 + B] = Dirs[B];
			}
			const int Fence = Add(P, EKind::Observe, R_FenceMark, 0, 3, 1, 0);
			P.Devices[Fence].Info[0] = Sym[P.Params[0]];
			Add(P, EKind::Clue, R_BarnBoard, 0, 1, 2, 1);
			Add(P, EKind::Switch, R_MillDial, 0, 8, 2, 1);
			Add(P, EKind::Button, R_MillBrake, 0, 1, 2, 1);
			Add(P, EKind::Gate, R_BarnDoor, 0, 1, 4, 0);
			P.NumSteps = 3;
			// Le moulin au repos (0) ne doit pas deja indiquer la bonne grange
			return P.Params[4 + P.Params[0]] != 0;
		}
		case 11:
		{
			for (int I = 0; I < 3; ++I)
			{
				P.Params[I] = (uint8_t)R.Range(10);
			}
			Add(P, EKind::Crank, R_Generator, 0, 8, 2, 0);
			uint8_t Zones[3] = { 1, 2, 3 };
			R.Shuffle(Zones, 3);
			for (int I = 0; I < 3; ++I)
			{
				const int D = Add(P, EKind::Clue, R_CityBoard, I, 1, Zones[I], 0);
				P.Devices[D].Info[0] = P.Params[I];
			}
			for (int I = 0; I < 3; ++I)
			{
				Add(P, EKind::Switch, R_DestDial, I, 10, 4, 1);
			}
			Add(P, EKind::Button, R_DestConfirm, 0, 1, 4, 1);
			Add(P, EKind::Gate, R_StationGate, 0, 1, 4, 1);
			P.NumSteps = 3;
			return !(P.Params[0] == 0 && P.Params[1] == 0 && P.Params[2] == 0);
		}
		case 37:
		{
			// A se deverse dans B par la vanne A (x crans), B se vide par la vanne B (y crans) :
			// A = 5 - x, B = B0 + x - y. Solution x = 5 - tA, y = B0 + x - tB, toutes deux dans 0..5.
			// Tirage direct dans les bornes : aucune variante impossible, la vanne B doit toujours bouger (y >= 1)
			const int B0 = 1 + R.Range(3);
			const int TA = 1 + R.Range(4);
			const int X = 5 - TA;
			const int Lo = B0 + X - 5 > 1 ? B0 + X - 5 : 1;
			const int Hi = B0 + X - 1 < 5 ? B0 + X - 1 : 5;
			const int TB = Lo + R.Range(Hi - Lo + 1);
			const int Y = B0 + X - TB;
			P.Params[0] = (uint8_t)TA;
			P.Params[1] = (uint8_t)TB;
			P.Params[2] = (uint8_t)B0;
			Add(P, EKind::Clue, R_LevelMarks, 0, 1, 1, 0);
			Add(P, EKind::Observe, R_Current, 0, 3, 2, 0);
			Add(P, EKind::Switch, R_Sluice, 0, 6, 3, 1);
			Add(P, EKind::Switch, R_Sluice, 1, 6, 3, 1);
			Add(P, EKind::Gate, R_DryPassage, 0, 1, 4, 0);
			Add(P, EKind::Clue, R_DiveLog, 0, 1, 3, 0, true);
			P.NumSteps = 3;
			return Y >= 0 && Y <= 5 && TB != B0 + X;
		}
		default:
			return false;
		}
	}

	inline bool Read(const FState& S, int D) { return S.Dev[D] != 0; }
	inline bool Observed(const FPlan& P, const FState& S, int D) { return S.Dev[D] >= P.Devices[D].Positions; }
	inline bool Solved(const FState& S) { return (S.Solved & 1) != 0; }

	int Pos37A(const FState& S) { return S.Dev[L37_SluiceA]; }
	int Pos37B(const FState& S) { return S.Dev[L37_SluiceB]; }
	int RawLevelB(const FPlan& P, const FState& S) { return P.Params[2] + Pos37A(S) - Pos37B(S); }

	int WrongDials0(const FPlan& P, const FState& S)
	{
		int N = 0;
		for (int I = 0; I < 3; ++I)
		{
			N += S.Dev[L0_Dial + I] != P.Params[3 + I] ? 1 : 0;
		}
		return N;
	}

	int CorrectValves2(const FPlan& P, const FState& S)
	{
		int N = 0;
		for (int V = 0; V < 3; ++V)
		{
			N += S.Dev[L2_Valve + V] == P.Params[6 + V] ? 1 : 0;
		}
		return N;
	}

	int GoodRelaysOn3(const FPlan& P, const FState& S)
	{
		int N = 0;
		for (int R = 0; R < 4; ++R)
		{
			N += (R != P.Params[5] && S.Dev[L3_Relay + R] == 1) ? 1 : 0;
		}
		return N;
	}

	int CodeDigit4(const FPlan& P, int I) { return P.Params[3 + P.Params[I]]; }

	int OpenLocks5(const FState& S)
	{
		int N = 0;
		for (int L = 0; L < 3; ++L)
		{
			N += S.Dev[L5_Lock + L] ? 1 : 0;
		}
		return N;
	}

	int HeldTotal(const FPlan& P, const FState& S)
	{
		int N = 0;
		for (int K = 0; K < 8; ++K)
		{
			N += Held(P, S, K);
		}
		return N;
	}

	int BeaconsDone6(const FPlan& P, const FState& S)
	{
		int N = 0;
		for (int I = 0; I < 3; ++I)
		{
			N += Observed(P, S, P.Params[I]) ? 1 : 0;
		}
		return N;
	}

	int WinchesDone8(const FPlan& P, const FState& S)
	{
		int N = 0;
		for (int I = 0; I < 3; ++I)
		{
			N += Observed(P, S, L8_Winch + P.Params[I]) ? 1 : 0;
		}
		return N;
	}

	int DigitsRight11(const FPlan& P, const FState& S)
	{
		int N = 0;
		for (int I = 0; I < 3; ++I)
		{
			N += S.Dev[L11_Dial + I] == P.Params[I] ? 1 : 0;
		}
		return N;
	}

	bool Pools37Right(const FPlan& P, const FState& S)
	{
		int A = 0, B = 0;
		PoolLevels(P, S, A, B);
		return A == P.Params[0] && RawLevelB(P, S) == P.Params[1];
	}

	/** Conditions continues : la mission se resout des que le reglage est bon, puis le reste */
	void Latch(const FPlan& P, FState& S)
	{
		if (Solved(S))
		{
			return;
		}
		bool bNow = false;
		switch (P.Level)
		{
		case 2: bNow = CorrectValves2(P, S) == 3; break;
		case 5: bNow = OpenLocks5(S) == 3 && S.Dev[L5_Dial] == P.Params[6]; break;
		case 8: bNow = WinchesDone8(P, S) == 3; break;
		case 37: bNow = Pools37Right(P, S); break;
		default: break;
		}
		if (bNow)
		{
			S.Solved |= 1;
		}
	}

	FResult SwitchTo(const FPlan& P, FState& S, int Dev, int To)
	{
		FResult R;
		const FDevice& D = P.Devices[Dev];
		uint8_t& V = S.Dev[Dev];
		if (To == V)
		{
			R.Feedback = EFeedback::AlreadyDone;
			return R;
		}
		switch (P.Level)
		{
		case 1:
			if (D.Role == R_Breaker && To == 1)
			{
				if (!S.Dev[L1_Sock + D.Label])
				{
					R.Feedback = EFeedback::NoFuse;
					R.Related = (uint8_t)(L1_Sock + D.Label);
					return R;
				}
				// Puissance limitee : deux circuits a la fois
				int On = 0, FirstOn = -1;
				for (int B = 0; B < 5; ++B)
				{
					if (S.Dev[L1_Brk + B] == 1)
					{
						++On;
						FirstOn = FirstOn < 0 ? B : FirstOn;
					}
				}
				if (On >= 2)
				{
					R.Feedback = EFeedback::Overload;
					R.Related = (uint8_t)(L1_Brk + FirstOn);
					return R;
				}
			}
			break;
		case 3:
			if (D.Role == R_Relay && To == 1 && D.Label == P.Params[5])
			{
				// Le secteur en defaut fait tout disjoncter : relais remis a zero, la commande reste possible
				for (int I = 0; I < 4; ++I)
				{
					S.Dev[L3_Relay + I] = 0;
				}
				R.bChanged = true;
				R.Feedback = EFeedback::Tripped;
				R.Related = (uint8_t)Dev;
				return R;
			}
			break;
		default:
			break;
		}
		V = (uint8_t)To;
		R.bChanged = true;
		R.Feedback = EFeedback::Progress;
		switch (P.Level)
		{
		case 2:
			if (To > P.Params[6 + D.Label])
			{
				R.Feedback = EFeedback::Warning;
				R.Related = (uint8_t)Dev;
			}
			break;
		case 5:
			if (D.Role == R_BoilerDial && To > P.Params[6])
			{
				R.Feedback = EFeedback::Warning;
				R.Related = (uint8_t)Dev;
			}
			break;
		case 37:
			if (RawLevelB(P, S) >= 7)
			{
				R.Feedback = EFeedback::Warning;
				R.Related = (uint8_t)L37_SluiceB;
			}
			break;
		default:
			break;
		}
		return R;
	}

	FResult UseSocket(const FPlan& P, FState& S, int Dev)
	{
		FResult R;
		const FDevice& D = P.Devices[Dev];
		if (S.Dev[Dev])
		{
			R.Feedback = EFeedback::AlreadyDone;
			return R;
		}
		if (Held(P, S, D.Need) > 0)
		{
			S.Dev[Dev] = 1;
			R.bChanged = true;
			R.Feedback = EFeedback::Done;
			return R;
		}
		R.Feedback = HeldTotal(P, S) > 0 ? EFeedback::WrongItem : EFeedback::NeedItem;
		return R;
	}

	FResult UseItem(const FPlan& P, FState& S, int Dev)
	{
		FResult R;
		const FDevice& D = P.Devices[Dev];
		if (!S.Dev[Dev])
		{
			if (P.Level == 5 && HeldTotal(P, S) >= KeyRingSize)
			{
				R.Feedback = EFeedback::Full;
				return R;
			}
			S.Dev[Dev] = 1;
			R.bChanged = true;
			R.Feedback = EFeedback::Done;
			return R;
		}
		// Cle portee, pas encore inseree : elle retourne a son crochet (trousseau limite)
		if (P.Level == 5 && Held(P, S, D.Need) > 0)
		{
			S.Dev[Dev] = 0;
			R.bChanged = true;
			R.Feedback = EFeedback::Returned;
			return R;
		}
		R.Feedback = EFeedback::AlreadyDone;
		return R;
	}

	FResult UseCrank(const FPlan& P, FState& S, int Dev, int Units)
	{
		FResult R;
		const FDevice& D = P.Devices[Dev];
		if (Observed(P, S, Dev))
		{
			R.Feedback = EFeedback::AlreadyDone;
			return R;
		}
		if (P.Level == 6)
		{
			if (Dev == P.Params[3])
			{
				R.Feedback = EFeedback::Dead;
				return R;
			}
			for (int I = 0; I < 3; ++I)
			{
				if (P.Params[I] == Dev)
				{
					if (I > 0 && !Observed(P, S, P.Params[I - 1]))
					{
						R.Feedback = EFeedback::Order;
						return R;
					}
					break;
				}
			}
		}
		else if (P.Level == 8)
		{
			if (D.Label == P.Params[3] || D.Label == P.Params[4])
			{
				R.Feedback = EFeedback::Dead;
				return R;
			}
			for (int I = 0; I < 3; ++I)
			{
				if (P.Params[I] == D.Label)
				{
					if (I > 0 && !Observed(P, S, L8_Winch + P.Params[I - 1]))
					{
						R.Feedback = EFeedback::Order;
						return R;
					}
					break;
				}
			}
		}
		if (Units <= 0)
		{
			R.Feedback = EFeedback::Progress;
			return R;
		}
		const int Next = S.Dev[Dev] + (Units > 4 ? 4 : Units);
		S.Dev[Dev] = U8(Next > D.Positions ? D.Positions : Next);
		R.bChanged = true;
		R.Feedback = Observed(P, S, Dev) ? EFeedback::Done : EFeedback::Progress;
		return R;
	}

	FResult Press(const FPlan& P, FState& S, int Dev)
	{
		FResult R;
		if (Solved(S))
		{
			R.Feedback = EFeedback::AlreadyDone;
			return R;
		}
		int Wrong = 0;
		bool bCount = false;
		switch (P.Level)
		{
		case 0:
			Wrong = WrongDials0(P, S);
			bCount = true;
			break;
		case 1:
			Wrong = (S.Dev[L1_Brk + P.Params[0]] == 1 ? 0 : 1) + (S.Dev[L1_Brk + P.Params[1]] == 1 ? 0 : 1);
			bCount = true;
			break;
		case 3:
			Wrong = 3 - GoodRelaysOn3(P, S);
			bCount = true;
			break;
		case 4:
			for (int I = 0; I < 3; ++I)
			{
				Wrong += S.Dev[L4_Dial + I] != CodeDigit4(P, I) ? 1 : 0;
			}
			break;
		case 6:
			Wrong = 3 - BeaconsDone6(P, S);
			bCount = true;
			break;
		case 9:
			Wrong = Dev == P.Params[3] ? 0 : 1;
			break;
		case 10:
			Wrong = S.Dev[L10_Mill] == P.Params[4 + P.Params[0]] ? 0 : 1;
			break;
		case 11:
			Wrong = 3 - DigitsRight11(P, S);
			break;
		default:
			return R;
		}
		if (Wrong == 0)
		{
			S.Solved |= 1;
			R.bChanged = true;
			R.Feedback = EFeedback::Done;
			return R;
		}
		R.Feedback = EFeedback::Wrong;
		R.Count = (uint8_t)(bCount ? Wrong : 0);
		return R;
	}

	uint8_t FirstUnobserved(const FPlan& P, const FState& S, int From, int N)
	{
		for (int I = 0; I < N; ++I)
		{
			if (!Observed(P, S, From + I))
			{
				return (uint8_t)(From + I);
			}
		}
		return 255;
	}

	void SetStep(FEval& E, int I, int Progress, int Goal, bool bPrereq)
	{
		E.Progress[I] = U8(Progress > Goal ? Goal : Progress);
		E.Goal[I] = U8(Goal);
		if (Progress >= Goal)
		{
			E.Steps[I] = EStep::Done;
		}
		else if (Progress > 0)
		{
			E.Steps[I] = EStep::InProgress;
		}
		else
		{
			E.Steps[I] = bPrereq ? EStep::Available : EStep::Hidden;
		}
	}

	inline bool StepDone(const FEval& E, int I) { return E.Steps[I] == EStep::Done; }
}

using namespace Impl;

uint32_t Hash(uint32_t Seed, uint32_t Salt)
{
	return Mix(Seed * 0x9E3779B1U ^ Mix(Salt + 0x632BE59BU));
}

bool HasMission(int Level)
{
	switch (Level)
	{
	case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 8: case 9: case 10: case 11: case 37:
		return true;
	default:
		return false;
	}
}

void BuildFallbackPlan(int Level, uint32_t Seed, FPlan& Out)
{
	Out = FPlan();
	Out.Level = Level;
	Out.Seed = Seed;
	Out.bFallback = true;
	if (!HasMission(Level))
	{
		return;
	}
	// Graine fixe, verifiee par le banc d'essai : meme variante chez tous les joueurs
	for (uint32_t Try = 0; Try < 64; ++Try)
	{
		FRng R(0xB0A7F00DU, 0x5EC0 + Try * 131U + (uint32_t)Level);
		if (BuildLevel(Level, R, Out))
		{
			return;
		}
	}
}

void BuildPlan(int Level, uint32_t Seed, FPlan& Out)
{
	Out = FPlan();
	Out.Level = Level;
	Out.Seed = Seed;
	if (!HasMission(Level))
	{
		return;
	}
	// Quelques tirages (rejet des variantes triviales ou hors bornes), puis la variante de secours
	for (uint32_t Try = 0; Try < 8; ++Try)
	{
		FRng R(Seed, 0x4D490000U + Try * 7919U + (uint32_t)Level);
		if (BuildLevel(Level, R, Out))
		{
			return;
		}
	}
	BuildFallbackPlan(Level, Seed, Out);
}

void InitState(const FPlan& Plan, FState& Out)
{
	Out = FState();
	(void)Plan;
}

int Held(const FPlan& Plan, const FState& State, int Kind)
{
	int N = 0;
	for (int D = 0; D < Plan.NumDevices; ++D)
	{
		const FDevice& Dev = Plan.Devices[D];
		if (Dev.Need != Kind || !State.Dev[D])
		{
			continue;
		}
		if (Dev.Kind == EKind::Item)
		{
			++N;
		}
		else if (Dev.Kind == EKind::Socket)
		{
			--N;
		}
	}
	return N < 0 ? 0 : N;
}

FResult CanUse(const FPlan& P, const FState& S, const FCampaign& Campaign, int Dev)
{
	(void)Campaign;
	FResult R;
	if (Dev < 0 || Dev >= P.NumDevices)
	{
		return R;
	}
	switch (P.Level)
	{
	case 0:
		if (Dev == L0_Stab && !Solved(S))
		{
			R.Related = FirstUnobserved(P, S, L0_Anom, 3);
		}
		break;
	case 1:
		if (Dev == L1_ResNote && !S.Dev[Dev] && S.Dev[L1_Brk + P.Params[2]] != 1)
		{
			R.Related = (uint8_t)(L1_Brk + P.Params[2]);
		}
		break;
	case 4:
		if (Dev == L4_Enter && !Solved(S))
		{
			R.Related = !S.Dev[L4_Plan] ? (uint8_t)L4_Plan : (!S.Dev[L4_Dir] ? (uint8_t)L4_Dir : (uint8_t)255);
		}
		break;
	case 5:
		if (Dev == L5_Dial)
		{
			if (!S.Dev[L5_Note])
			{
				R.Related = (uint8_t)L5_Note;
			}
			for (int L = 0; L < 3 && R.Related == 255; ++L)
			{
				if (!S.Dev[L5_Lock + L])
				{
					R.Related = (uint8_t)(L5_Lock + L);
					break;
				}
			}
		}
		break;
	case 9:
		if (P.Devices[Dev].Role == R_HouseMarker && !Solved(S) && S.Dev[L9_Box] != P.Params[0])
		{
			R.Related = (uint8_t)L9_Box;
		}
		break;
	case 11:
		if (Dev == L11_Confirm && !Solved(S) && !Observed(P, S, L11_Gen))
		{
			R.Related = (uint8_t)L11_Gen;
		}
		break;
	default:
		break;
	}
	if (R.Related != 255)
	{
		R.Feedback = EFeedback::Locked;
	}
	return R;
}

FResult Act(const FPlan& P, FState& S, const FCampaign& Campaign, int Dev, EAction Action, int Value)
{
	FResult R;
	if (Dev < 0 || Dev >= P.NumDevices || !HasMission(P.Level))
	{
		return R;
	}
	const FResult Lock = CanUse(P, S, Campaign, Dev);
	if (Lock.Feedback == EFeedback::Locked)
	{
		return Lock;
	}
	const FDevice& D = P.Devices[Dev];
	switch (D.Kind)
	{
	case EKind::Clue:
		if (S.Dev[Dev])
		{
			R.Feedback = EFeedback::AlreadyDone;
			return R;
		}
		S.Dev[Dev] = 1;
		R.bChanged = true;
		R.Feedback = EFeedback::Done;
		break;
	case EKind::Observe:
		if (Observed(P, S, Dev))
		{
			R.Feedback = EFeedback::AlreadyDone;
			return R;
		}
		R.Feedback = EFeedback::Progress;
		if (Action == EAction::Hold && Value > 0)
		{
			const int Next = S.Dev[Dev] + (Value > 4 ? 4 : Value);
			S.Dev[Dev] = U8(Next > D.Positions ? D.Positions : Next);
			R.bChanged = true;
			R.Feedback = Observed(P, S, Dev) ? EFeedback::Done : EFeedback::Progress;
		}
		break;
	case EKind::Switch:
	{
		const int To = Action == EAction::Set ? Value : (S.Dev[Dev] + 1) % D.Positions;
		if (To < 0 || To >= D.Positions)
		{
			return R;
		}
		R = SwitchTo(P, S, Dev, To);
		break;
	}
	case EKind::Socket:
		R = UseSocket(P, S, Dev);
		break;
	case EKind::Item:
		R = UseItem(P, S, Dev);
		break;
	case EKind::Crank:
		R = UseCrank(P, S, Dev, Action == EAction::Hold ? Value : 0);
		break;
	case EKind::Button:
		R = Press(P, S, Dev);
		break;
	case EKind::Gate:
		return R;
	}
	if (R.bChanged)
	{
		Latch(P, S);
	}
	if ((R.Feedback == EFeedback::Wrong || R.Feedback == EFeedback::Tripped) && S.Mistakes < 255)
	{
		++S.Mistakes;
	}
	return R;
}

void PoolLevels(const FPlan& Plan, const FState& State, int& OutA, int& OutB)
{
	OutA = 5;
	OutB = 0;
	if (Plan.Level != 37)
	{
		return;
	}
	OutA = 5 - Pos37A(State);
	const int B = RawLevelB(Plan, State);
	OutB = B < 0 ? 0 : (B > 7 ? 7 : B);
}

int GaugeReading(const FPlan& Plan, const FState& State, int Device)
{
	if (Plan.Level != 2 || Device < L2_Gauge || Device >= L2_Gauge + 3)
	{
		return 0;
	}
	return State.Dev[L2_Valve + Plan.Params[3 + (Device - L2_Gauge)]];
}

int KnownDigit(const FPlan& Plan, const FState& State, const FCampaign& Campaign, int Index)
{
	if (Plan.Level != 11 || Index < 0 || Index > 2)
	{
		return -1;
	}
	if (State.Dev[L11_Board + Index] || (Campaign.RouteBits & (1u << Index)))
	{
		return Plan.Params[Index];
	}
	return -1;
}

void Evaluate(const FPlan& P, const FState& S, const FCampaign& Campaign, FEval& E)
{
	E = FEval();
	E.NumSteps = P.NumSteps;
	E.bSolved = Solved(S);
	const bool bSolved = E.bSolved;
	switch (P.Level)
	{
	case 0:
	{
		int Docs = 0;
		for (int I = 0; I < 3; ++I)
		{
			Docs += Observed(P, S, L0_Anom + I) ? 1 : 0;
		}
		SetStep(E, 0, Docs, 3, true);
		SetStep(E, 1, bSolved ? 1 : 0, 1, StepDone(E, 0));
		SetStep(E, 2, 0, 1, bSolved);
		if (bSolved)
		{
			E.Route = S.Dev[L0_Lever];
			E.Gate[L0_Pass] = 255;
		}
		break;
	}
	case 1:
	{
		const int Fused = (S.Dev[L1_Sock + P.Params[0]] ? 1 : 0) + (S.Dev[L1_Sock + P.Params[1]] ? 1 : 0);
		SetStep(E, 0, S.Dev[L1_Schem] ? 1 : 0, 1, true);
		SetStep(E, 1, bSolved ? 2 : Fused, 2, StepDone(E, 0));
		SetStep(E, 2, bSolved ? 1 : 0, 1, StepDone(E, 1));
		E.Gate[L1_ResLights] = S.Dev[L1_Brk + P.Params[2]] == 1 ? 255 : 0;
		E.bOptionalDone = S.Dev[L1_ResNote] != 0;
		break;
	}
	case 2:
	{
		int Traced = 0;
		for (int G = 0; G < 3; ++G)
		{
			Traced += Observed(P, S, L2_Gauge + G) ? 1 : 0;
		}
		const int Correct = bSolved ? 3 : CorrectValves2(P, S);
		SetStep(E, 0, S.Dev[L2_Plate] ? 1 : 0, 1, true);
		SetStep(E, 1, bSolved ? 3 : Traced, 3, StepDone(E, 0));
		SetStep(E, 2, Correct, 3, StepDone(E, 1));
		E.Gate[L2_Door] = U8(bSolved ? 255 : Correct * 70);
		if (!bSolved)
		{
			for (int V = 0; V < 3; ++V)
			{
				if (S.Dev[L2_Valve + V] > P.Params[6 + V])
				{
					E.WarningDevice = (uint8_t)(L2_Valve + V);
					break;
				}
			}
		}
		break;
	}
	case 3:
	{
		int OkSeen = 0;
		bool bFault = false;
		for (int B = 0; B < 4; ++B)
		{
			if (Observed(P, S, L3_Box + B))
			{
				if (B == P.Params[0])
				{
					bFault = true;
				}
				else
				{
					++OkSeen;
				}
			}
		}
		SetStep(E, 0, S.Dev[L3_Board] ? 1 : 0, 1, true);
		SetStep(E, 1, (bSolved || bFault || OkSeen >= 3) ? 1 : 0, 1, StepDone(E, 0));
		SetStep(E, 2, bSolved ? 3 : GoodRelaysOn3(P, S), 3, StepDone(E, 1));
		SetStep(E, 3, bSolved ? 1 : 0, 1, StepDone(E, 2));
		break;
	}
	case 4:
		SetStep(E, 0, S.Dev[L4_Plan] ? 1 : 0, 1, true);
		SetStep(E, 1, S.Dev[L4_Dir] ? 1 : 0, 1, StepDone(E, 0));
		SetStep(E, 2, bSolved ? 1 : 0, 1, StepDone(E, 1));
		E.Gate[L4_Access] = bSolved ? 255 : 0;
		E.bOptionalDone = S.Dev[L4_Arch] != 0;
		break;
	case 5:
		SetStep(E, 0, S.Dev[L5_Reg] ? 1 : 0, 1, true);
		SetStep(E, 1, OpenLocks5(S), 3, StepDone(E, 0));
		SetStep(E, 2, bSolved ? 1 : 0, 1, StepDone(E, 1));
		E.Gate[L5_Pass] = bSolved ? 255 : 0;
		if (!bSolved && S.Dev[L5_Dial] > P.Params[6])
		{
			E.WarningDevice = (uint8_t)L5_Dial;
		}
		E.bOptionalDone = S.Dev[L5_Staff] != 0;
		break;
	case 6:
		SetStep(E, 0, S.Dev[L6_Start] ? 1 : 0, 1, true);
		SetStep(E, 1, BeaconsDone6(P, S), 3, StepDone(E, 0));
		SetStep(E, 2, bSolved ? 1 : 0, 1, StepDone(E, 1));
		for (int B = 0; B < 4; ++B)
		{
			E.Gate[L6_Beacon + B] = Observed(P, S, L6_Beacon + B) ? 255 : 0;
		}
		E.Gate[L6_Exit] = bSolved ? 255 : 0;
		break;
	case 8:
	{
		int Units = 0;
		for (int I = 0; I < 3; ++I)
		{
			Units += S.Dev[L8_Winch + P.Params[I]];
		}
		SetStep(E, 0, S.Dev[L8_Marks] ? 1 : 0, 1, true);
		SetStep(E, 1, WinchesDone8(P, S), 3, StepDone(E, 0));
		E.Gate[L8_Bridge] = U8(bSolved ? 255 : Units * 255 / 12);
		break;
	}
	case 9:
	{
		int Plans = 0;
		for (int I = 0; I < 3; ++I)
		{
			Plans += S.Dev[L9_Plan + I] ? 1 : 0;
		}
		const bool bPower = S.Dev[L9_Box] == P.Params[0];
		SetStep(E, 0, Plans, 3, true);
		SetStep(E, 1, (bSolved || bPower) ? 1 : 0, 1, S.Dev[L9_Plan] != 0);
		SetStep(E, 2, bSolved ? 1 : 0, 1, StepDone(E, 1));
		for (int H = 0; H < 3; ++H)
		{
			E.Gate[L9_House + H] = bPower ? 255 : 0;
		}
		E.Gate[L9_Door] = bSolved ? 255 : 0;
		break;
	}
	case 10:
		SetStep(E, 0, Observed(P, S, L10_Fence) ? 1 : 0, 1, true);
		SetStep(E, 1, S.Dev[L10_Board] ? 1 : 0, 1, true);
		SetStep(E, 2, bSolved ? 1 : 0, 1, StepDone(E, 0) && StepDone(E, 1));
		E.Gate[L10_Door] = bSolved ? 255 : 0;
		break;
	case 11:
	{
		int Known = 0;
		for (int I = 0; I < 3; ++I)
		{
			Known += KnownDigit(P, S, Campaign, I) >= 0 ? 1 : 0;
		}
		SetStep(E, 0, S.Dev[L11_Gen], P.Devices[L11_Gen].Positions, true);
		SetStep(E, 1, bSolved ? 3 : Known, 3, true);
		SetStep(E, 2, bSolved ? 1 : 0, 1, StepDone(E, 0) && StepDone(E, 1));
		E.Gate[L11_Gate] = bSolved ? 255 : 0;
		break;
	}
	case 37:
	{
		int A = 0, B = 0;
		PoolLevels(P, S, A, B);
		const int Right = bSolved ? 2 : ((A == P.Params[0] ? 1 : 0) + (RawLevelB(P, S) == P.Params[1] ? 1 : 0));
		SetStep(E, 0, S.Dev[L37_Marks] ? 1 : 0, 1, true);
		SetStep(E, 1, Observed(P, S, L37_Current) ? 1 : 0, 1, true);
		SetStep(E, 2, Right, 2, StepDone(E, 0));
		E.Gate[L37_Pass] = bSolved ? 255 : 0;
		if (!bSolved && RawLevelB(P, S) >= 7)
		{
			E.WarningDevice = (uint8_t)L37_SluiceB;
		}
		E.bOptionalDone = S.Dev[L37_Log] != 0;
		break;
	}
	default:
		break;
	}
}

bool GetClue(const FPlan& P, const FState& S, int Dev, FClue& C)
{
	C = FClue();
	if (Dev < 0 || Dev >= P.NumDevices)
	{
		return false;
	}
	const FDevice& D = P.Devices[Dev];
	const bool bKnown = (D.Kind == EKind::Clue && S.Dev[Dev]) || (D.Kind == EKind::Observe && Observed(P, S, Dev));
	switch (D.Role)
	{
	case R_Anomaly:
		if (!bKnown) return false;
		C.Kind = EClue::AnomalyBlinks;
		C.N = 1;
		C.A[0] = D.Label;
		C.B[0] = D.Info[0];
		return true;
	case R_MaintNote:
		if (!bKnown) return false;
		C.Kind = EClue::MaintRule;
		return true;
	case R_Schematic:
		if (!bKnown) return false;
		C.Kind = EClue::Schematic;
		C.N = 3;
		C.A[0] = P.Params[0];
		C.A[1] = P.Params[1];
		C.A[2] = P.Params[2];
		return true;
	case R_ReserveNote:
		if (!bKnown) return false;
		C.Kind = EClue::ReserveNote;
		return true;
	case R_PressurePlate:
		if (!bKnown) return false;
		C.Kind = EClue::PressureTargets;
		C.N = 3;
		for (int G = 0; G < 3; ++G)
		{
			C.A[G] = (uint8_t)G;
			C.B[G] = P.Params[G];
		}
		return true;
	case R_Gauge:
		if (!bKnown) return false;
		C.Kind = EClue::GaugeFeed;
		C.N = 1;
		C.A[0] = D.Label;
		C.B[0] = D.Info[0];
		return true;
	case R_LoadBoard:
		if (!bKnown) return false;
		C.Kind = EClue::LoadBoard;
		C.N = 4;
		for (int Relay = 0; Relay < 4; ++Relay)
		{
			C.A[Relay] = (uint8_t)Relay;
			C.B[Relay] = P.Params[1 + Relay];
		}
		return true;
	case R_JunctionBox:
		if (!bKnown) return false;
		C.Kind = EClue::JunctionState;
		C.N = 1;
		C.A[0] = D.Label;
		C.B[0] = D.Info[0];
		return true;
	case R_Planning:
		if (!bKnown) return false;
		C.Kind = EClue::Planning;
		C.N = 3;
		for (int I = 0; I < 3; ++I)
		{
			C.A[I] = P.Params[I];
		}
		return true;
	case R_Directory:
		if (!bKnown) return false;
		C.Kind = EClue::Directory;
		C.N = NumSymbols;
		for (int I = 0; I < NumSymbols; ++I)
		{
			C.A[I] = (uint8_t)I;
			C.B[I] = P.Params[3 + I];
		}
		return true;
	case R_Archive:
		if (!bKnown) return false;
		C.Kind = EClue::Archive;
		return true;
	case R_Register:
		if (!bKnown) return false;
		C.Kind = EClue::Register;
		C.N = 3;
		for (int L = 0; L < 3; ++L)
		{
			C.A[L] = P.Params[L];
			C.B[L] = P.Params[7 + P.Params[3 + L]];
		}
		return true;
	case R_Key:
		// L'etiquette se lit sur la cle elle-meme
		C.Kind = EClue::KeyTag;
		C.N = 1;
		C.A[0] = D.Label;
		return true;
	case R_BoilerNote:
		if (!bKnown) return false;
		C.Kind = EClue::BoilerPressure;
		C.N = 1;
		C.A[0] = P.Params[6];
		return true;
	case R_StaffNote:
		if (!bKnown) return false;
		C.Kind = EClue::StaffNote;
		return true;
	case R_StartPlate:
		if (!bKnown) return false;
		C.Kind = EClue::StartBeacon;
		C.N = 1;
		C.A[0] = P.Devices[P.Params[0]].Label;
		return true;
	case R_Beacon:
		// La balise leurre porte une plaque "hors service" ; les autres montrent la suivante une fois activees
		if (D.Info[0] != 255 && !Observed(P, S, Dev)) return false;
		C.Kind = EClue::BeaconNext;
		C.N = 1;
		C.A[0] = D.Label;
		C.B[0] = D.Info[0];
		return true;
	case R_PassageMarks:
		if (!bKnown) return false;
		C.Kind = EClue::WinchOrder;
		C.N = 3;
		for (int I = 0; I < 3; ++I)
		{
			C.A[I] = P.Params[I];
		}
		return true;
	case R_HousePlan:
		if (!bKnown) return false;
		C.N = 1;
		if (D.Label == 0)
		{
			C.Kind = EClue::CircuitPlan;
			C.A[0] = P.Params[0];
		}
		else if (D.Label == 1)
		{
			C.Kind = EClue::PorchPlan;
			C.A[0] = P.Params[1];
		}
		else
		{
			C.Kind = EClue::WindowsPlan;
			C.A[0] = P.Params[2];
		}
		return true;
	case R_HouseMarker:
		// Porche et fenetres se voient sur la facade
		C.Kind = EClue::HouseFacade;
		C.N = 1;
		C.A[0] = D.Label;
		C.B[0] = D.Info[0];
		return true;
	case R_FenceMark:
		if (!bKnown) return false;
		C.Kind = EClue::TargetBarn;
		C.N = 1;
		C.A[0] = D.Info[0];
		return true;
	case R_BarnBoard:
		if (!bKnown) return false;
		C.Kind = EClue::BarnDirections;
		C.N = 3;
		for (int B = 0; B < 3; ++B)
		{
			C.A[B] = P.Params[1 + B];
			C.B[B] = P.Params[4 + B];
		}
		return true;
	case R_CityBoard:
		if (!bKnown) return false;
		C.Kind = EClue::RouteDigit;
		C.N = 1;
		C.A[0] = D.Label;
		C.B[0] = D.Info[0];
		return true;
	case R_LevelMarks:
		if (!bKnown) return false;
		C.Kind = EClue::LevelMarks;
		C.N = 2;
		C.A[0] = 0;
		C.B[0] = P.Params[0];
		C.A[1] = 1;
		C.B[1] = P.Params[1];
		return true;
	case R_Current:
		if (!bKnown) return false;
		C.Kind = EClue::CurrentDir;
		return true;
	case R_DiveLog:
		if (!bKnown) return false;
		C.Kind = EClue::DiveLog;
		return true;
	default:
		return false;
	}
}

bool IsExitGuarded(int Level, int Target)
{
	switch (Level)
	{
	case 0: return Target == 1 || Target == 37;
	case 1: return Target == 2 || Target == 4;
	case 2: return Target == 3;
	case 3: return Target == 4;
	case 4: return Target == 5;
	case 5: return Target == 6;
	case 6: return Target == 8;
	case 8: return Target == 9;
	case 9: return Target == 10;
	case 10: return Target == 11;
	case 11: return Target == EndingTarget;
	case 37: return Target == 0 || Target == 4;
	default: return false;
	}
}

bool IsExitOpen(const FPlan& Plan, const FState& State, int Target)
{
	if (!HasMission(Plan.Level) || !IsExitGuarded(Plan.Level, Target))
	{
		return true;
	}
	if (!Solved(State))
	{
		return false;
	}
	if (Plan.Level == 0)
	{
		return State.Dev[L0_Lever] == (Target == 1 ? 0 : 1);
	}
	return true;
}

uint8_t RouteBitFor(int Level)
{
	switch (Level)
	{
	case 2: case 3: case 37: return 1;
	case 5: case 6: return 2;
	case 8: case 9: case 10: return 4;
	default: return 0;
	}
}

uint16_t OptionalBitFor(int Level)
{
	switch (Level)
	{
	case 0: return 1u << 0;
	case 1: return 1u << 1;
	case 4: return 1u << 2;
	case 5: return 1u << 3;
	case 37: return 1u << 4;
	default: return 0;
	}
}

bool EndingVariant(const FCampaign& Campaign)
{
	int N = 0;
	for (int B = 0; B < 16; ++B)
	{
		N += (Campaign.OptionalFound >> B) & 1;
	}
	return N >= 3;
}

uint32_t Fingerprint(const FPlan& Plan)
{
	uint32_t H = Hash((uint32_t)Plan.Level, Plan.Seed ^ (Plan.bFallback ? 0xFA11BAC0U : 0U));
	for (int I = 0; I < MaxParams; ++I)
	{
		H = Hash(H, Plan.Params[I] + 0x100U * (uint32_t)I);
	}
	for (int D = 0; D < Plan.NumDevices; ++D)
	{
		const FDevice& Dev = Plan.Devices[D];
		H = Hash(H, (uint32_t)Dev.Kind | ((uint32_t)Dev.Role << 8) | ((uint32_t)Dev.Label << 16) | ((uint32_t)Dev.Positions << 24));
	}
	return H;
}

int Serialize(const FPlan& Plan, const FState& State, uint8_t* Out, int Capacity)
{
	const int Size = 11 + Plan.NumDevices;
	if (!Out || Capacity < Size || Plan.NumDevices > MaxDevices)
	{
		return 0;
	}
	const uint32_t F = Fingerprint(Plan);
	Out[0] = 0x4D;
	Out[1] = (uint8_t)GenVersion;
	Out[2] = U8(Plan.Level);
	Out[3] = (uint8_t)Plan.NumDevices;
	Out[4] = (uint8_t)(F & 0xFF);
	Out[5] = (uint8_t)((F >> 8) & 0xFF);
	Out[6] = (uint8_t)((F >> 16) & 0xFF);
	Out[7] = (uint8_t)((F >> 24) & 0xFF);
	for (int D = 0; D < Plan.NumDevices; ++D)
	{
		Out[8 + D] = State.Dev[D];
	}
	Out[8 + Plan.NumDevices] = State.Solved;
	Out[9 + Plan.NumDevices] = State.Mistakes;
	Out[10 + Plan.NumDevices] = 0;
	return Size;
}

bool Deserialize(const FPlan& Plan, const uint8_t* In, int Size, FState& Out)
{
	InitState(Plan, Out);
	if (!In || Size != 11 + Plan.NumDevices || In[0] != 0x4D || In[1] != GenVersion || In[2] != U8(Plan.Level)
		|| In[3] != Plan.NumDevices)
	{
		return false;
	}
	const uint32_t F = (uint32_t)In[4] | ((uint32_t)In[5] << 8) | ((uint32_t)In[6] << 16) | ((uint32_t)In[7] << 24);
	if (F != Fingerprint(Plan))
	{
		return false;
	}
	// Valeurs bornees appareil par appareil (un blob abime ne cree ni position impossible ni objet)
	for (int D = 0; D < Plan.NumDevices; ++D)
	{
		const FDevice& Dev = Plan.Devices[D];
		const uint8_t V = In[8 + D];
		switch (Dev.Kind)
		{
		case EKind::Switch: Out.Dev[D] = V < Dev.Positions ? V : 0; break;
		case EKind::Observe:
		case EKind::Crank: Out.Dev[D] = V > Dev.Positions ? Dev.Positions : V; break;
		case EKind::Clue:
		case EKind::Socket:
		case EKind::Item: Out.Dev[D] = V ? 1 : 0; break;
		default: Out.Dev[D] = 0; break;
		}
	}
	// Une prise remplie suppose son objet pris
	for (int D = 0; D < Plan.NumDevices; ++D)
	{
		const FDevice& Dev = Plan.Devices[D];
		if (Dev.Kind != EKind::Socket || !Out.Dev[D])
		{
			continue;
		}
		int Taken = 0, Filled = 0;
		for (int E = 0; E < Plan.NumDevices; ++E)
		{
			if (Plan.Devices[E].Need == Dev.Need && Out.Dev[E])
			{
				Taken += Plan.Devices[E].Kind == EKind::Item ? 1 : 0;
				Filled += Plan.Devices[E].Kind == EKind::Socket ? 1 : 0;
			}
		}
		for (int E = 0; E < Plan.NumDevices && Taken < Filled; ++E)
		{
			if (Plan.Devices[E].Kind == EKind::Item && Plan.Devices[E].Need == Dev.Need && !Out.Dev[E])
			{
				Out.Dev[E] = 1;
				++Taken;
			}
		}
		if (Taken < Filled)
		{
			Out.Dev[D] = 0;
		}
	}
	// Trousseau limite
	if (Plan.Level == 5)
	{
		for (int D = Plan.NumDevices - 1; D >= 0 && HeldTotal(Plan, Out) > KeyRingSize; --D)
		{
			if (Plan.Devices[D].Kind == EKind::Item && Out.Dev[D] && Held(Plan, Out, Plan.Devices[D].Need) > 0)
			{
				Out.Dev[D] = 0;
			}
		}
	}
	// Niveau 3 : le relais en defaut ne peut pas etre enclenche
	if (Plan.Level == 3)
	{
		Out.Dev[L3_Relay + Plan.Params[5]] = 0;
	}
	// Niveau 1 : deux circuits au plus, chacun avec son fusible
	if (Plan.Level == 1)
	{
		int On = 0;
		for (int B = 0; B < 5; ++B)
		{
			uint8_t& V = Out.Dev[L1_Brk + B];
			if (V && (!Out.Dev[L1_Sock + B] || On >= 2))
			{
				V = 0;
			}
			On += V ? 1 : 0;
		}
	}
	Out.Solved = (uint8_t)(In[8 + Plan.NumDevices] & 1);
	Out.Mistakes = In[9 + Plan.NumDevices];
	return true;
}
}
