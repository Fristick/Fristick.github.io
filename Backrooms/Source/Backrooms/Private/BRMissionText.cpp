// v4.11 : textes des missions, composes dans la langue de chaque joueur. Les symboles, lettres, chiffres et angles
// gardent leur sens dans toutes les langues ; la longueur d'une traduction ne change jamais un code.
#include "BRMission.h"
#include "BRLoc.h"
#include "BRKeys.h"

namespace BRM = BRMission;

namespace
{
	FString L(uint8 V) { return BRMissionText::Letter(V); }
	FString N1(uint8 V) { return FString::FromInt(V + 1); }
	FString Room(uint8 V) { return FString::FromInt(100 + V); }
	FString Deg(uint8 V) { return FString::FromInt(V * 45); }
	FString Key() { return BRKeys::Tag(EBRAction::Interact); }
}

FString BRMissionText::Symbol(uint8 Sym)
{
	switch (Sym)
	{
	case 0: return BR_STR(NSLOCTEXT("BR", "Mission.Sym.Triangle", "triangle"));
	case 1: return BR_STR(NSLOCTEXT("BR", "Mission.Sym.Circle", "cercle"));
	case 2: return BR_STR(NSLOCTEXT("BR", "Mission.Sym.Square", "carr\u00e9"));
	case 3: return BR_STR(NSLOCTEXT("BR", "Mission.Sym.Diamond", "losange"));
	case 4: return BR_STR(NSLOCTEXT("BR", "Mission.Sym.Cross", "croix"));
	default: return BR_STR(NSLOCTEXT("BR", "Mission.Sym.Star", "\u00e9toile"));
	}
}

FString BRMissionText::Letter(uint8 V)
{
	return FString::Chr(TEXT('A') + FMath::Min<int32>(V, 25));
}

FString BRMissionText::DeviceName(const BRM::FPlan& P, int32 Dev)
{
	if (Dev < 0 || Dev >= P.NumDevices)
	{
		return FString();
	}
	const BRM::FDevice& D = P.Devices[Dev];
	switch (D.Role)
	{
	case BRM::R_Anomaly: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.Anomaly", "N\u00e9on anormal (motif {Sym})"), { { TEXT("Sym"), BRLoc::Arg(Symbol(D.Label)) } });
	case BRM::R_MaintNote: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.MaintNote", "Note de maintenance"));
	case BRM::R_Dial: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.Dial", "Cadran {Sym}"), { { TEXT("Sym"), BRLoc::Arg(Symbol(D.Label)) } });
	case BRM::R_Stabilize: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.Stabilize", "Bouton STABILISER"));
	case BRM::R_RouteLever: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.RouteLever", "Levier d'aiguillage"));
	case BRM::R_Passage: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.Passage", "Passage instable"));
	case BRM::R_Schematic: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.Schematic", "Sch\u00e9ma de distribution"));
	case BRM::R_Fuse: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.Fuse", "Fusible {L}"), { { TEXT("L"), BRLoc::Arg(L(D.Label)) } });
	case BRM::R_FuseSocket: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.FuseSocket", "Porte-fusible {L}"), { { TEXT("L"), BRLoc::Arg(L(D.Label)) } });
	case BRM::R_Breaker: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.Breaker", "Disjoncteur {L}"), { { TEXT("L"), BRLoc::Arg(L(D.Label)) } });
	case BRM::R_ElevatorCall: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.ElevatorCall", "Appel de l'ascenseur"));
	case BRM::R_ReserveLights: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.ReserveLights", "\u00c9clairage de la r\u00e9serve"));
	case BRM::R_ReserveNote: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.ReserveNote", "Carnet du gardien"));
	case BRM::R_PressurePlate: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.PressurePlate", "Plaque des pressions"));
	case BRM::R_Gauge: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.Gauge", "Manom\u00e8tre M{N}"), { { TEXT("N"), BRLoc::Arg(N1(D.Label)) } });
	case BRM::R_Valve: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.Valve", "Vanne V{N}"), { { TEXT("N"), BRLoc::Arg(N1(D.Label)) } });
	case BRM::R_SteamDoor: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.SteamDoor", "Porte bloqu\u00e9e par la vapeur"));
	case BRM::R_LoadBoard: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.LoadBoard", "Tableau de charge"));
	case BRM::R_JunctionBox: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.JunctionBox", "Bo\u00eete de jonction, secteur {L}"), { { TEXT("L"), BRLoc::Arg(L(D.Label)) } });
	case BRM::R_Relay: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.Relay", "Relais {N}"), { { TEXT("N"), BRLoc::Arg(N1(D.Label)) } });
	case BRM::R_ElevatorPower: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.ElevatorPower", "Alimentation de l'ascenseur"));
	case BRM::R_Planning: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.Planning", "Planning de garde"));
	case BRM::R_Directory: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.Directory", "Archives : annuaire des bureaux"));
	case BRM::R_Archive: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.Archive", "Dossier du personnel"));
	case BRM::R_CodeDial: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.CodeDial", "Molette {N} du code"), { { TEXT("N"), BRLoc::Arg(N1(D.Label)) } });
	case BRM::R_CodeEnter: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.CodeEnter", "Bouton VALIDER"));
	case BRM::R_HotelAccess: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.HotelAccess", "Grille de l'acc\u00e8s \u00e0 l'h\u00f4tel"));
	case BRM::R_Register: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.Register", "Registre de la r\u00e9ception"));
	case BRM::R_Key: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.Key", "Cl\u00e9 de la chambre {Room}"), { { TEXT("Room"), BRLoc::Arg(Room(D.Label)) } });
	case BRM::R_Lock: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.Lock", "Serrure {Sym}"), { { TEXT("Sym"), BRLoc::Arg(Symbol(D.Label)) } });
	case BRM::R_BoilerNote: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.BoilerNote", "Consigne de la chaufferie"));
	case BRM::R_BoilerDial: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.BoilerDial", "Robinet de pression de la chaudi\u00e8re"));
	case BRM::R_BoilerPassage: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.BoilerPassage", "Passage de la chaufferie"));
	case BRM::R_StaffNote: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.StaffNote", "Note du personnel"));
	case BRM::R_StartPlate: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.StartPlate", "Plaque de d\u00e9part"));
	case BRM::R_Beacon: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.Beacon", "Balise {Sym}"), { { TEXT("Sym"), BRLoc::Arg(Symbol(D.Label)) } });
	case BRM::R_EmergencyPower: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.EmergencyPower", "Alimentation de secours"));
	case BRM::R_LightsExit: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.LightsExit", "Sortie de secours"));
	case BRM::R_PassageMarks: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.PassageMarks", "Marques de passage"));
	case BRM::R_Winch: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.Winch", "Treuil {L}"), { { TEXT("L"), BRLoc::Arg(L(D.Label)) } });
	case BRM::R_Bridge: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.Bridge", "Passerelle"));
	case BRM::R_HousePlan: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.HousePlan", "Plan de maison"));
	case BRM::R_StreetBox: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.StreetBox", "Bo\u00eetier \u00e9lectrique de la rue"));
	case BRM::R_HouseMarker: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.HouseMarker", "Maison : porche {Sym}, {N} fen\u00eatres"), { { TEXT("Sym"), BRLoc::Arg(Symbol(D.Label)) }, { TEXT("N"), BRLoc::Int(D.Info[0]) } });
	case BRM::R_HouseDoor: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.HouseDoor", "Porte de la maison"));
	case BRM::R_FenceMark: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.FenceMark", "Marque sur la cl\u00f4ture"));
	case BRM::R_BarnBoard: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.BarnBoard", "Tableau des granges"));
	case BRM::R_MillDial: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.MillDial", "Orientation du moulin"));
	case BRM::R_MillBrake: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.MillBrake", "Frein du moulin"));
	case BRM::R_BarnDoor: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.BarnDoor", "Porte de la grange"));
	case BRM::R_Generator: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.Generator", "G\u00e9n\u00e9rateur de la station"));
	case BRM::R_CityBoard: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.CityBoard", "Panneau de route {N}"), { { TEXT("N"), BRLoc::Arg(N1(D.Label)) } });
	case BRM::R_DestDial: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.DestDial", "Destination, chiffre {N}"), { { TEXT("N"), BRLoc::Arg(N1(D.Label)) } });
	case BRM::R_DestConfirm: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.DestConfirm", "Confirmer la destination"));
	case BRM::R_StationGate: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.StationGate", "Portique de la station"));
	case BRM::R_LevelMarks: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.LevelMarks", "Marques de niveau d'eau"));
	case BRM::R_Current: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.Current", "Courant de l'eau"));
	case BRM::R_Sluice: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Dev.Sluice", "Vanne du bassin {L}"), { { TEXT("L"), BRLoc::Arg(L(D.Label)) } });
	case BRM::R_DryPassage: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.DryPassage", "Passage sec"));
	case BRM::R_DiveLog: return BR_STR(NSLOCTEXT("BR", "Mission.Dev.DiveLog", "Carnet de plong\u00e9e"));
	default: return FString();
	}
}

FString BRMissionText::DeviceState(const BRM::FPlan& P, const BRM::FState& S, int32 Dev)
{
	if (Dev < 0 || Dev >= P.NumDevices)
	{
		return FString();
	}
	const BRM::FDevice& D = P.Devices[Dev];
	const uint8 V = S.Dev[Dev];
	const FString On = BR_STR(NSLOCTEXT("BR", "Mission.State.On", "MARCHE"));
	const FString Off = BR_STR(NSLOCTEXT("BR", "Mission.State.Off", "ARR\u00caT"));
	switch (D.Kind)
	{
	case BRM::EKind::Clue:
		return V ? BR_STR(NSLOCTEXT("BR", "Mission.State.Read", "lu")) : FString();
	case BRM::EKind::Observe:
	case BRM::EKind::Crank:
		if (V >= D.Positions)
		{
			return D.Kind == BRM::EKind::Observe ? BR_STR(NSLOCTEXT("BR", "Mission.State.Documented", "document\u00e9"))
				: BR_STR(NSLOCTEXT("BR", "Mission.State.InPlace", "en place"));
		}
		return V ? BRLoc::Fmt(NSLOCTEXT("BR", "Mission.State.Percent", "{Pct} %"), { { TEXT("Pct"), BRLoc::Int(V * 100 / FMath::Max<int32>(1, D.Positions)) } }) : FString();
	case BRM::EKind::Switch:
		switch (D.Role)
		{
		case BRM::R_Breaker:
		case BRM::R_Relay:
			return V ? On : Off;
		case BRM::R_RouteLever:
			return V ? BR_STR(NSLOCTEXT("BR", "Mission.State.RoutePool", "route : Poolrooms"))
				: BR_STR(NSLOCTEXT("BR", "Mission.State.RouteL1", "route : Niveau 1"));
		case BRM::R_StreetBox:
			return V ? BRLoc::Fmt(NSLOCTEXT("BR", "Mission.State.Circuit", "circuit {N}"), { { TEXT("N"), BRLoc::Int(V) } })
				: BR_STR(NSLOCTEXT("BR", "Mission.State.Disconnected", "d\u00e9connect\u00e9"));
		case BRM::R_MillDial:
			return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.State.Degrees", "{Deg}\u00b0"), { { TEXT("Deg"), BRLoc::Arg(Deg(V)) } });
		case BRM::R_Valve:
		case BRM::R_Sluice:
			return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.State.Opening", "ouverture {N}/{Max}"), { { TEXT("N"), BRLoc::Int(V) }, { TEXT("Max"), BRLoc::Int(D.Positions - 1) } });
		case BRM::R_BoilerDial:
			return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.State.Pressure", "pression {N}"), { { TEXT("N"), BRLoc::Int(V) } });
		default:
			return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.State.Position", "position {N}"), { { TEXT("N"), BRLoc::Int(V) } });
		}
	case BRM::EKind::Socket:
		if (D.Role == BRM::R_Lock)
		{
			return V ? BR_STR(NSLOCTEXT("BR", "Mission.State.Unlocked", "ouverte")) : BR_STR(NSLOCTEXT("BR", "Mission.State.Locked", "ferm\u00e9e"));
		}
		return V ? BR_STR(NSLOCTEXT("BR", "Mission.State.FusePlaced", "fusible pos\u00e9")) : BR_STR(NSLOCTEXT("BR", "Mission.State.Empty", "vide"));
	case BRM::EKind::Item:
		if (!V)
		{
			return FString();
		}
		return BRM::Held(P, S, D.Need) > 0 ? BR_STR(NSLOCTEXT("BR", "Mission.State.Carried", "port\u00e9e par l'\u00e9quipe"))
			: BR_STR(NSLOCTEXT("BR", "Mission.State.Used", "utilis\u00e9"));
	case BRM::EKind::Button:
		return (S.Solved & 1) ? BR_STR(NSLOCTEXT("BR", "Mission.State.Validated", "valid\u00e9")) : FString();
	default:
		return FString();
	}
}

void BRMissionText::ClueLines(const BRM::FClue& C, TArray<FString>& Out)
{
	switch (C.Kind)
	{
	case BRM::EClue::AnomalyBlinks:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.Blinks", "N\u00e9on {Sym} : {Count} {Count}|plural(one=clignotement,other=clignotements) par s\u00e9rie"),
			{ { TEXT("Sym"), BRLoc::Arg(Symbol(C.A[0])) }, { TEXT("Count"), BRLoc::Int(C.B[0]) } }));
		break;
	case BRM::EClue::MaintRule:
		Out.Add(BR_STR(NSLOCTEXT("BR", "Mission.Clue.MaintRule", "Chaque cadran du panneau porte un motif : r\u00e9glez-le sur le nombre de clignotements du n\u00e9on qui porte le m\u00eame motif, puis appuyez sur STABILISER.")));
		break;
	case BRM::EClue::Schematic:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.Schematic", "Ascenseur : circuits {A} et {B}. R\u00e9serve : circuit {C}. Puissance limit\u00e9e : deux circuits allum\u00e9s \u00e0 la fois."),
			{ { TEXT("A"), BRLoc::Arg(L(C.A[0])) }, { TEXT("B"), BRLoc::Arg(L(C.A[1])) }, { TEXT("C"), BRLoc::Arg(L(C.A[2])) } }));
		break;
	case BRM::EClue::ReserveNote:
		Out.Add(BR_STR(NSLOCTEXT("BR", "Mission.Clue.ReserveNote", "\u00ab L'ascenseur tire sur deux lignes. Tant que la r\u00e9serve est \u00e9clair\u00e9e, il ne viendra pas. Les fusibles sont marqu\u00e9s : chacun va dans son porte-fusible. \u00bb Le gardien")));
		break;
	case BRM::EClue::PressureTargets:
		for (int32 I = 0; I < C.N; ++I)
		{
			Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.PressureTarget", "Manom\u00e8tre M{G} : {P} bars"), { { TEXT("G"), BRLoc::Arg(N1(C.A[I])) }, { TEXT("P"), BRLoc::Int(C.B[I]) } }));
		}
		break;
	case BRM::EClue::GaugeFeed:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.GaugeFeed", "La conduite du manom\u00e8tre M{G} vient de la vanne V{V}"), { { TEXT("G"), BRLoc::Arg(N1(C.A[0])) }, { TEXT("V"), BRLoc::Arg(N1(C.B[0])) } }));
		break;
	case BRM::EClue::LoadBoard:
		Out.Add(BR_STR(NSLOCTEXT("BR", "Mission.Clue.LoadRule", "L'ascenseur demande les trois secteurs sains. Un secteur en d\u00e9faut fait disjoncter tous les relais.")));
		for (int32 I = 0; I < C.N; ++I)
		{
			Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.RelaySector", "Relais {R} : secteur {S}"), { { TEXT("R"), BRLoc::Arg(N1(C.A[I])) }, { TEXT("S"), BRLoc::Arg(L(C.B[I])) } }));
		}
		break;
	case BRM::EClue::JunctionState:
		Out.Add(C.B[0] ? BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.SectorFaulty", "Secteur {S} : c\u00e2bles br\u00fbl\u00e9s, en d\u00e9faut"), { { TEXT("S"), BRLoc::Arg(L(C.A[0])) } })
			: BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.SectorOk", "Secteur {S} : normal"), { { TEXT("S"), BRLoc::Arg(L(C.A[0])) } }));
		break;
	case BRM::EClue::Planning:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.Planning", "Garde de nuit, dans l'ordre : badge {A}, badge {B}, badge {C}. Le code de l'h\u00f4tel : leurs num\u00e9ros de bureau."),
			{ { TEXT("A"), BRLoc::Arg(Symbol(C.A[0])) }, { TEXT("B"), BRLoc::Arg(Symbol(C.A[1])) }, { TEXT("C"), BRLoc::Arg(Symbol(C.A[2])) } }));
		break;
	case BRM::EClue::Directory:
		for (int32 I = 0; I < C.N; ++I)
		{
			Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.Office", "Badge {Sym} : bureau {D}"), { { TEXT("Sym"), BRLoc::Arg(Symbol(C.A[I])) }, { TEXT("D"), BRLoc::Arg(FString::FromInt(C.B[I])) } }));
		}
		break;
	case BRM::EClue::Archive:
		Out.Add(BR_STR(NSLOCTEXT("BR", "Mission.Clue.Archive", "Dossier : \u00ab \u00c0 l'h\u00f4tel, celui qui revient sans lampe, marche trop droit et ne r\u00e9pond pas quand on lui fait signe n'est pas l'un des n\u00f4tres. Deux signes suffisent : \u00e9loignez-vous. \u00bb")));
		break;
	case BRM::EClue::Register:
		Out.Add(BR_STR(NSLOCTEXT("BR", "Mission.Clue.RegisterHead", "Cl\u00e9s de la chaufferie, rang\u00e9es dans les chambres :")));
		for (int32 I = 0; I < C.N; ++I)
		{
			Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.RegisterLine", "Serrure {Sym} : cl\u00e9 de la chambre {Room}"), { { TEXT("Sym"), BRLoc::Arg(Symbol(C.A[I])) }, { TEXT("Room"), BRLoc::Arg(Room(C.B[I])) } }));
		}
		break;
	case BRM::EClue::KeyTag:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.KeyTag", "\u00c9tiquette : chambre {Room}"), { { TEXT("Room"), BRLoc::Arg(Room(C.A[0])) } }));
		break;
	case BRM::EClue::BoilerPressure:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.BoilerPressure", "Consigne : pression {P}. Au-dessus, la chaudi\u00e8re siffle ; en dessous, le passage reste ferm\u00e9."), { { TEXT("P"), BRLoc::Int(C.A[0]) } }));
		break;
	case BRM::EClue::StaffNote:
		Out.Add(BR_STR(NSLOCTEXT("BR", "Mission.Clue.StaffNote", "\u00ab Un faux client n'a pas de lampe allum\u00e9e, ne cligne pas des yeux et s'arr\u00eate net quand on lui fait signe. Attendez deux signes avant de fuir, mais ne lui tournez jamais le dos. \u00bb")));
		break;
	case BRM::EClue::StartBeacon:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.StartBeacon", "Premi\u00e8re balise : {Sym}. Chaque balise activ\u00e9e montre la suivante."), { { TEXT("Sym"), BRLoc::Arg(Symbol(C.A[0])) } }));
		break;
	case BRM::EClue::BeaconNext:
		if (C.B[0] == 255)
		{
			Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.BeaconDead", "Balise {Sym} : hors service"), { { TEXT("Sym"), BRLoc::Arg(Symbol(C.A[0])) } }));
		}
		else if (C.B[0] == 254)
		{
			Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.BeaconLast", "Balise {Sym} : derni\u00e8re, alimentation de secours ensuite"), { { TEXT("Sym"), BRLoc::Arg(Symbol(C.A[0])) } }));
		}
		else
		{
			Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.BeaconNext", "Balise {Sym} : suivante {Next}"), { { TEXT("Sym"), BRLoc::Arg(Symbol(C.A[0])) }, { TEXT("Next"), BRLoc::Arg(Symbol(C.B[0])) } }));
		}
		break;
	case BRM::EClue::WinchOrder:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.WinchOrder", "Treuils, dans l'ordre : {A}, puis {B}, puis {C}. Les autres sont gripp\u00e9s."),
			{ { TEXT("A"), BRLoc::Arg(L(C.A[0])) }, { TEXT("B"), BRLoc::Arg(L(C.A[1])) }, { TEXT("C"), BRLoc::Arg(L(C.A[2])) } }));
		break;
	case BRM::EClue::CircuitPlan:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.CircuitPlan", "Plan : le passage est aliment\u00e9 par le circuit de rue {N}"), { { TEXT("N"), BRLoc::Int(C.A[0]) } }));
		break;
	case BRM::EClue::PorchPlan:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.PorchPlan", "Plan : la maison du passage a un porche {Sym}"), { { TEXT("Sym"), BRLoc::Arg(Symbol(C.A[0])) } }));
		break;
	case BRM::EClue::WindowsPlan:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.WindowsPlan", "Plan : la maison du passage a {N} fen\u00eatres en fa\u00e7ade"), { { TEXT("N"), BRLoc::Int(C.A[0]) } }));
		break;
	case BRM::EClue::HouseFacade:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.HouseFacade", "Maison au porche {Sym}, {N} fen\u00eatres"), { { TEXT("Sym"), BRLoc::Arg(Symbol(C.A[0])) }, { TEXT("N"), BRLoc::Int(C.B[0]) } }));
		break;
	case BRM::EClue::TargetBarn:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.TargetBarn", "La marque d\u00e9signe la grange au signe {Sym}"), { { TEXT("Sym"), BRLoc::Arg(Symbol(C.A[0])) } }));
		break;
	case BRM::EClue::BarnDirections:
		for (int32 I = 0; I < C.N; ++I)
		{
			Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.BarnDirection", "Grange {Sym} : {Deg}\u00b0"), { { TEXT("Sym"), BRLoc::Arg(Symbol(C.A[I])) }, { TEXT("Deg"), BRLoc::Arg(Deg(C.B[I])) } }));
		}
		break;
	case BRM::EClue::RouteDigit:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.RouteDigit", "Destination, chiffre {I} : {D}"), { { TEXT("I"), BRLoc::Arg(N1(C.A[0])) }, { TEXT("D"), BRLoc::Arg(FString::FromInt(C.B[0])) } }));
		break;
	case BRM::EClue::LevelMarks:
		Out.Add(BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Clue.LevelMarks", "Niveaux voulus : bassin A \u00e0 la marque {A}, bassin B \u00e0 la marque {B}."), { { TEXT("A"), BRLoc::Int(C.B[0]) }, { TEXT("B"), BRLoc::Int(C.B[1]) } }));
		break;
	case BRM::EClue::CurrentDir:
		Out.Add(BR_STR(NSLOCTEXT("BR", "Mission.Clue.Current", "Le courant va du bassin A vers le bassin B : chaque cran de la vanne A fait baisser A et monter B d'une marque. La vanne B vide le bassin B.")));
		break;
	case BRM::EClue::DiveLog:
		Out.Add(BR_STR(NSLOCTEXT("BR", "Mission.Clue.DiveLog", "\u00ab Le chemin le long des bassins est plus long mais s\u00fbr. Pr\u00e8s de l'eau profonde, on gagne du temps ; on n'y plonge jamais sans souffle. \u00bb")));
		break;
	default:
		break;
	}
}

FString BRMissionText::StepTitle(int32 Level, int32 Step)
{
	switch (Level * 10 + Step)
	{
	case 0: return BR_STR(NSLOCTEXT("BR", "Mission.Step.0.0", "Documenter les n\u00e9ons anormaux"));
	case 1: return BR_STR(NSLOCTEXT("BR", "Mission.Step.0.1", "R\u00e9gler le panneau et stabiliser le passage"));
	case 2: return BR_STR(NSLOCTEXT("BR", "Mission.Step.0.2", "Choisir la route au levier, puis partir"));
	case 10: return BR_STR(NSLOCTEXT("BR", "Mission.Step.1.0", "Lire le sch\u00e9ma de distribution"));
	case 11: return BR_STR(NSLOCTEXT("BR", "Mission.Step.1.1", "Poser les fusibles de l'ascenseur"));
	case 12: return BR_STR(NSLOCTEXT("BR", "Mission.Step.1.2", "Alimenter et appeler l'ascenseur"));
	case 20: return BR_STR(NSLOCTEXT("BR", "Mission.Step.2.0", "Lire la plaque des pressions"));
	case 21: return BR_STR(NSLOCTEXT("BR", "Mission.Step.2.1", "Suivre les conduites jusqu'aux manom\u00e8tres"));
	case 22: return BR_STR(NSLOCTEXT("BR", "Mission.Step.2.2", "R\u00e9gler les vannes pour d\u00e9gager la porte"));
	case 30: return BR_STR(NSLOCTEXT("BR", "Mission.Step.3.0", "Lire le tableau de charge"));
	case 31: return BR_STR(NSLOCTEXT("BR", "Mission.Step.3.1", "Trouver le secteur en d\u00e9faut"));
	case 32: return BR_STR(NSLOCTEXT("BR", "Mission.Step.3.2", "R\u00e9tablir les relais des secteurs sains"));
	case 33: return BR_STR(NSLOCTEXT("BR", "Mission.Step.3.3", "Alimenter l'ascenseur"));
	case 40: return BR_STR(NSLOCTEXT("BR", "Mission.Step.4.0", "Lire le planning de garde"));
	case 41: return BR_STR(NSLOCTEXT("BR", "Mission.Step.4.1", "Trouver les bureaux dans les archives"));
	case 42: return BR_STR(NSLOCTEXT("BR", "Mission.Step.4.2", "Composer le code de l'acc\u00e8s \u00e0 l'h\u00f4tel"));
	case 50: return BR_STR(NSLOCTEXT("BR", "Mission.Step.5.0", "Lire le registre de la r\u00e9ception"));
	case 51: return BR_STR(NSLOCTEXT("BR", "Mission.Step.5.1", "Ouvrir les serrures de la chaufferie"));
	case 52: return BR_STR(NSLOCTEXT("BR", "Mission.Step.5.2", "Appliquer la consigne de pression"));
	case 60: return BR_STR(NSLOCTEXT("BR", "Mission.Step.6.0", "Lire la plaque de d\u00e9part"));
	case 61: return BR_STR(NSLOCTEXT("BR", "Mission.Step.6.1", "Activer les balises dans l'ordre"));
	case 62: return BR_STR(NSLOCTEXT("BR", "Mission.Step.6.2", "R\u00e9tablir l'alimentation de secours"));
	case 80: return BR_STR(NSLOCTEXT("BR", "Mission.Step.8.0", "Lire les marques de passage"));
	case 81: return BR_STR(NSLOCTEXT("BR", "Mission.Step.8.1", "Actionner les treuils dans l'ordre"));
	case 90: return BR_STR(NSLOCTEXT("BR", "Mission.Step.9.0", "Rassembler les plans des maisons"));
	case 91: return BR_STR(NSLOCTEXT("BR", "Mission.Step.9.1", "Reconnecter le circuit de la rue"));
	case 92: return BR_STR(NSLOCTEXT("BR", "Mission.Step.9.2", "Identifier la maison du passage"));
	case 100: return BR_STR(NSLOCTEXT("BR", "Mission.Step.10.0", "Trouver la marque d'orientation"));
	case 101: return BR_STR(NSLOCTEXT("BR", "Mission.Step.10.1", "Lire le tableau des granges"));
	case 102: return BR_STR(NSLOCTEXT("BR", "Mission.Step.10.2", "Orienter le moulin, puis l\u00e2cher le frein"));
	case 110: return BR_STR(NSLOCTEXT("BR", "Mission.Step.11.0", "Alimenter la station"));
	case 111: return BR_STR(NSLOCTEXT("BR", "Mission.Step.11.1", "R\u00e9unir les donn\u00e9es de route"));
	case 112: return BR_STR(NSLOCTEXT("BR", "Mission.Step.11.2", "Configurer la destination finale"));
	case 370: return BR_STR(NSLOCTEXT("BR", "Mission.Step.37.0", "Lire les marques de niveau d'eau"));
	case 371: return BR_STR(NSLOCTEXT("BR", "Mission.Step.37.1", "Observer le sens du courant"));
	case 372: return BR_STR(NSLOCTEXT("BR", "Mission.Step.37.2", "\u00c9quilibrer les deux bassins"));
	default: return FString();
	}
}

FString BRMissionText::StepHint(int32 Level, int32 Step)
{
	const FString K = Key();
	switch (Level * 10 + Step)
	{
	case 0: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Hint.0.0", "Trois n\u00e9ons portent un motif peint et clignotent par s\u00e9ries, puis s'arr\u00eatent. Regardez-en un en maintenant {Key} : le cam\u00e9scope du sac enregistre le rythme."), { { TEXT("Key"), BRLoc::Arg(K) } });
	case 1: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.0.1", "Le panneau de stabilisation porte trois cadrans marqu\u00e9s d'un motif. Chacun attend le rythme du n\u00e9on au m\u00eame motif. Une erreur ne co\u00fbte rien : le panneau dit combien de cadrans sont faux."));
	case 2: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.0.2", "Le levier d'aiguillage ouvre soit le mur vers le Niveau 1, soit la trappe vers les Poolrooms. Vous pouvez changer d'avis tant que personne n'est parti."));
	case 10: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.1.0", "Le sch\u00e9ma est affich\u00e9 pr\u00e8s du d\u00e9part, sur un panneau m\u00e9tallique. Il dit quels circuits alimentent l'ascenseur."));
	case 11: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.1.1", "Les fusibles sont marqu\u00e9s d'une lettre et dispers\u00e9s dans le parking. Chacun ne va que dans le porte-fusible de sa lettre, dans la salle \u00e9lectrique."));
	case 12: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.1.2", "Deux circuits au plus peuvent \u00eatre allum\u00e9s. \u00c9teignez ce qui ne sert pas l'ascenseur avant de l'appeler."));
	case 20: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.2.0", "La plaque des pressions donne la valeur voulue sur chaque manom\u00e8tre, pas sur chaque vanne."));
	case 21: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Hint.2.1", "Chaque manom\u00e8tre est au bout d'une conduite. Examinez-le en maintenant {Key} pour savoir de quelle vanne il d\u00e9pend."), { { TEXT("Key"), BRLoc::Arg(K) } });
	case 22: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.2.2", "Une vanne trop ouverte fait fuir la vapeur : refermez-la d'un cran. La porte se d\u00e9gage quand les trois manom\u00e8tres sont justes."));
	case 30: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.3.0", "Le tableau de charge dit quel relais alimente quel secteur."));
	case 31: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Hint.3.1", "Les bo\u00eetes de jonction des secteurs sont dans les couloirs. Une bo\u00eete en d\u00e9faut sent le br\u00fbl\u00e9 et cr\u00e9pite ; examinez-les en maintenant {Key}."), { { TEXT("Key"), BRLoc::Arg(K) } });
	case 32: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.3.2", "N'enclenchez jamais le relais du secteur en d\u00e9faut : tout disjoncte. Les trois autres doivent \u00eatre en marche."));
	case 33: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.3.3", "Avec les trois secteurs sains aliment\u00e9s, appuyez sur l'alimentation de l'ascenseur."));
	case 40: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.4.0", "Le planning est punais\u00e9 dans un bureau. Il donne l'ordre des badges de garde, pas le code."));
	case 41: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.4.1", "Les archives donnent le num\u00e9ro de bureau de chaque badge. Le code se lit dans l'ordre du planning."));
	case 42: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.4.2", "R\u00e9glez les trois molettes, puis VALIDER. Un code refus\u00e9 ne bloque rien : le m\u00e9canisme se r\u00e9arme en quelques secondes."));
	case 50: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.5.0", "Le registre de la r\u00e9ception dit dans quelle chambre se trouve la cl\u00e9 de chaque serrure."));
	case 51: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.5.1", "Le trousseau ne tient que trois cl\u00e9s. Une cl\u00e9 inutile se remet \u00e0 son crochet. Chaque serrure n'accepte que sa cl\u00e9."));
	case 52: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.5.2", "La consigne est affich\u00e9e pr\u00e8s de la chaudi\u00e8re. Trop de pression la fait siffler : redescendez."));
	case 60: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.6.0", "La plaque de d\u00e9part est en relief : on la lit m\u00eame dans le noir. Elle donne la premi\u00e8re balise."));
	case 61: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Hint.6.1", "Maintenez {Key} pour remonter une balise : elle fait du bruit pendant ce temps. Activ\u00e9e, elle \u00e9claire et montre la suivante. Une balise hors ordre ne s'enclenche pas."), { { TEXT("Key"), BRLoc::Arg(K) } });
	case 62: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.6.2", "La derni\u00e8re balise m\u00e8ne \u00e0 l'alimentation de secours, pr\u00e8s de la sortie."));
	case 80: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.8.0", "Les marques de passage sont grav\u00e9es dans la roche, pr\u00e8s du d\u00e9part. Elles donnent l'ordre des treuils."));
	case 81: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Hint.8.1", "Maintenez {Key} sur un treuil : il garde sa progression si vous le l\u00e2chez. Hors ordre, le c\u00e2ble reste mou."), { { TEXT("Key"), BRLoc::Arg(K) } });
	case 90: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.9.0", "Trois plans sont pos\u00e9s dans trois maisons du quartier. Chacun donne un seul indice."));
	case 91: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.9.1", "Le bo\u00eetier de la rue choisit un circuit. Sans courant, les porches restent \u00e9teints et aucune maison ne s'ouvre."));
	case 92: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.9.2", "Deux maisons ressemblent \u00e0 la bonne : il faut le bon porche ET le bon nombre de fen\u00eatres."));
	case 100: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Hint.10.0", "Une marque peinte sur une cl\u00f4ture d\u00e9signe une grange par son signe. Examinez-la en maintenant {Key}."), { { TEXT("Key"), BRLoc::Arg(K) } });
	case 101: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.10.1", "Le tableau pr\u00e8s du moulin donne l'angle de chaque grange."));
	case 102: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.10.2", "Tournez l'orientation du moulin sur l'angle de la grange d\u00e9sign\u00e9e, puis l\u00e2chez le frein. Un mauvais angle fait tourner le moulin \u00e0 vide un moment."));
	case 110: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Hint.11.0", "Le g\u00e9n\u00e9rateur de la station se remonte en maintenant {Key}. Il garde sa charge."), { { TEXT("Key"), BRLoc::Arg(K) } });
	case 111: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.11.1", "Les fragments de route rapport\u00e9s des niveaux pr\u00e9c\u00e9dents sont dans ce carnet. Les chiffres manquants sont sur les panneaux de la ville."));
	case 112: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.11.2", "R\u00e9glez les trois chiffres de la destination, puis confirmez \u00e0 la station."));
	case 370: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.37.0", "Les marques de niveau sont peintes sur un pilier, au sec."));
	case 371: return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Hint.37.1", "Le courant se voit \u00e0 la surface, depuis le trottoir. Observez-le en maintenant {Key} : inutile de plonger."), { { TEXT("Key"), BRLoc::Arg(K) } });
	case 372: return BR_STR(NSLOCTEXT("BR", "Mission.Hint.37.2", "Les r\u00e8gles gradu\u00e9es montrent la hauteur de chaque bassin. Si le bassin B d\u00e9borde, refermez la vanne A ou ouvrez la vanne B."));
	default: return FString();
	}
}

FString BRMissionText::Feedback(const BRM::FPlan& P, int32 Dev, uint8 Fb, uint8 Related, uint8 Count)
{
	const FString Rel = Related != 255 ? DeviceName(P, Related) : FString();
	switch (Fb)
	{
	case static_cast<uint8>(BRM::EFeedback::Locked):
		return Rel.IsEmpty() ? BR_STR(NSLOCTEXT("BR", "Mission.Fb.Locked", "Pas encore : une \u00e9tape manque."))
			: BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Fb.LockedBy", "Pas encore : il faut d'abord {Device}."), { { TEXT("Device"), BRLoc::Arg(Rel) } });
	case static_cast<uint8>(BRM::EFeedback::NeedItem):
		return Dev >= 0 && Dev < P.NumDevices && P.Devices[Dev].Role == BRM::R_Lock ? BR_STR(NSLOCTEXT("BR", "Mission.Fb.NeedKey", "Il faut la cl\u00e9 de cette serrure."))
			: BR_STR(NSLOCTEXT("BR", "Mission.Fb.NeedFuse", "Il faut le fusible de cette lettre."));
	case static_cast<uint8>(BRM::EFeedback::WrongItem):
		return BR_STR(NSLOCTEXT("BR", "Mission.Fb.WrongKey", "Aucune cl\u00e9 du trousseau n'entre dans cette serrure."));
	case static_cast<uint8>(BRM::EFeedback::Overload):
		return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Fb.Overload", "Surcharge : deux circuits \u00e0 la fois au maximum. D\u00e9j\u00e0 allum\u00e9 : {Device}."), { { TEXT("Device"), BRLoc::Arg(Rel) } });
	case static_cast<uint8>(BRM::EFeedback::NoFuse):
		return BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Fb.NoFuse", "Rien ne s'allume : {Device} est vide."), { { TEXT("Device"), BRLoc::Arg(Rel) } });
	case static_cast<uint8>(BRM::EFeedback::Wrong):
		return Count > 0 ? BRLoc::Fmt(NSLOCTEXT("BR", "Mission.Fb.WrongCount", "Refus\u00e9 : {Count} {Count}|plural(one=\u00e9l\u00e9ment faux,other=\u00e9l\u00e9ments faux). Rien n'est perdu."), { { TEXT("Count"), BRLoc::Int(Count) } })
			: BR_STR(NSLOCTEXT("BR", "Mission.Fb.Wrong", "Refus\u00e9 : le r\u00e9glage n'est pas le bon. Rien n'est perdu."));
	case static_cast<uint8>(BRM::EFeedback::Warning):
		if (P.Level == 37)
		{
			return BR_STR(NSLOCTEXT("BR", "Mission.Fb.Overflow", "Le bassin B d\u00e9borde sur le trottoir : refermez la vanne A ou ouvrez la vanne B."));
		}
		if (P.Level == 5)
		{
			return BR_STR(NSLOCTEXT("BR", "Mission.Fb.BoilerHigh", "La chaudi\u00e8re siffle : pression trop haute."));
		}
		return BR_STR(NSLOCTEXT("BR", "Mission.Fb.SteamLeak", "Fuite de vapeur : cette vanne est trop ouverte."));
	case static_cast<uint8>(BRM::EFeedback::Tripped):
		return BR_STR(NSLOCTEXT("BR", "Mission.Fb.Tripped", "D\u00e9faut ! Ce relais alimente le secteur ab\u00eem\u00e9 : tous les relais ont disjonct\u00e9."));
	case static_cast<uint8>(BRM::EFeedback::AlreadyDone):
		return BR_STR(NSLOCTEXT("BR", "Mission.Fb.AlreadyDone", "C'est d\u00e9j\u00e0 fait."));
	case static_cast<uint8>(BRM::EFeedback::Done):
		return BR_STR(NSLOCTEXT("BR", "Mission.Fb.Done", "Fait."));
	case static_cast<uint8>(BRM::EFeedback::Full):
		return BR_STR(NSLOCTEXT("BR", "Mission.Fb.Full", "Le trousseau est plein (trois cl\u00e9s). Remettez une cl\u00e9 \u00e0 son crochet."));
	case static_cast<uint8>(BRM::EFeedback::Returned):
		return BR_STR(NSLOCTEXT("BR", "Mission.Fb.Returned", "Cl\u00e9 remise \u00e0 son crochet."));
	case static_cast<uint8>(BRM::EFeedback::Order):
		return P.Level == 8 ? BR_STR(NSLOCTEXT("BR", "Mission.Fb.WinchOrder", "Le c\u00e2ble reste mou : un autre treuil d'abord."))
			: BR_STR(NSLOCTEXT("BR", "Mission.Fb.BeaconOrder", "La balise ne s'enclenche pas : suivez l'ordre."));
	case static_cast<uint8>(BRM::EFeedback::Dead):
		return P.Level == 8 ? BR_STR(NSLOCTEXT("BR", "Mission.Fb.Jammed", "Ce treuil est gripp\u00e9 : il tourne \u00e0 vide."))
			: BR_STR(NSLOCTEXT("BR", "Mission.Fb.Dead", "Hors service."));
	case Cooldown:
		return BR_STR(NSLOCTEXT("BR", "Mission.Fb.Cooldown", "Le m\u00e9canisme se r\u00e9arme..."));
	case TooFar:
		return BR_STR(NSLOCTEXT("BR", "Mission.Fb.TooFar", "Trop loin."));
	case NotNow:
		return BR_STR(NSLOCTEXT("BR", "Mission.Fb.NotNow", "Pas maintenant."));
	case Taken:
		return BR_STR(NSLOCTEXT("BR", "Mission.Fb.Taken", "Un co\u00e9quipier l'a d\u00e9j\u00e0 pris : il est dans les objets de l'\u00e9quipe."));
	default:
		return FString();
	}
}

FString BRMissionText::SolvedLine(int32 Level)
{
	switch (Level)
	{
	case 0: return BR_STR(NSLOCTEXT("BR", "Mission.Solved.0", "Le passage se stabilise : la route choisie au levier est ouverte."));
	case 1: return BR_STR(NSLOCTEXT("BR", "Mission.Solved.1", "Le courant revient : l'ascenseur arrive et la porte de service se d\u00e9verrouille."));
	case 2: return BR_STR(NSLOCTEXT("BR", "Mission.Solved.2", "La pression s'\u00e9quilibre : la vapeur se dissipe devant la porte."));
	case 3: return BR_STR(NSLOCTEXT("BR", "Mission.Solved.3", "Les secteurs sont aliment\u00e9s : l'ascenseur se remet en marche."));
	case 4: return BR_STR(NSLOCTEXT("BR", "Mission.Solved.4", "Code accept\u00e9 : la grille de l'acc\u00e8s \u00e0 l'h\u00f4tel se l\u00e8ve."));
	case 5: return BR_STR(NSLOCTEXT("BR", "Mission.Solved.5", "La chaudi\u00e8re tourne \u00e0 la bonne pression : le passage de la chaufferie s'ouvre."));
	case 6: return BR_STR(NSLOCTEXT("BR", "Mission.Solved.6", "L'alimentation de secours s'allume : la sortie est \u00e9clair\u00e9e."));
	case 8: return BR_STR(NSLOCTEXT("BR", "Mission.Solved.8", "La passerelle est descendue : l'\u00e9chelle est accessible."));
	case 9: return BR_STR(NSLOCTEXT("BR", "Mission.Solved.9", "Le porche s'allume et la porte de la maison s'entrouvre."));
	case 10: return BR_STR(NSLOCTEXT("BR", "Mission.Solved.10", "Le moulin s'aligne : la porte de la grange d\u00e9sign\u00e9e s'ouvre."));
	case 11: return BR_STR(NSLOCTEXT("BR", "Mission.Solved.11", "Destination accept\u00e9e : le portique de la station s'ouvre sur le dernier quai."));
	case 37: return BR_STR(NSLOCTEXT("BR", "Mission.Solved.37", "Les bassins sont \u00e9quilibr\u00e9s : le passage sec est d\u00e9couvert."));
	default: return FString();
	}
}

FString BRMissionText::OptionalLine(int32 Level)
{
	switch (Level)
	{
	case 0: return BR_STR(NSLOCTEXT("BR", "Mission.Optional.0", "Facultatif : retrouver au moins trois cassettes VHS (souvenirs)"));
	case 1: return BR_STR(NSLOCTEXT("BR", "Mission.Optional.1", "Facultatif : \u00e9clairer la r\u00e9serve et lire le carnet du gardien"));
	case 4: return BR_STR(NSLOCTEXT("BR", "Mission.Optional.4", "Facultatif : consulter le dossier du personnel"));
	case 5: return BR_STR(NSLOCTEXT("BR", "Mission.Optional.5", "Facultatif : trouver la note du personnel"));
	case 37: return BR_STR(NSLOCTEXT("BR", "Mission.Optional.37", "Facultatif : trouver le carnet de plong\u00e9e"));
	default: return FString();
	}
}
