// v4.11 : logique des missions de niveau, sans dependance au moteur.
//
// Ce module ne connait ni Unreal, ni la geometrie : il decrit pour chaque niveau les mecanismes d'une mission (indices,
// observations, interrupteurs, prises, objets d'equipe, manivelles, portes), tire leurs parametres d'apres la graine,
// valide chaque action (dependances, puissance limitee, ordre) et calcule l'etat des etapes et des sorties.
// Le jeu l'utilise tel quel (ABRWorld, cote hote) ; Tools/Missions/test_mission_logic.cpp le compile seul et verifie,
// sur des milliers de graines, que chaque mission se resout avec les seules informations donnees au joueur, qu'une
// erreur se comprend et se corrige, et qu'aucun etat ne bloque la partie.
//
// Regles communes :
//   - Tout ce qui est indispensable existe toujours (aucun tirage independant qui pourrait echouer).
//   - Une erreur ne detruit rien : elle donne un retour (Feedback) et reste reversible.
//   - Aucune action ne demande deux joueurs a la fois : les manivelles gardent leur progression, les interrupteurs leur
//     position. Les objets de mission (fusibles, cles) appartiennent a l'equipe : un depart ne les emporte pas.
//   - Une mission resolue le reste (verrou leve une fois pour toutes), sauf le choix de route du Niveau 0.
//   - Les symboles (triangle, cercle, carre, losange, croix, etoile), lettres A-D, chiffres et angles gardent leur sens
//     dans toutes les langues ; les textes sont composes par le jeu dans la langue de chaque joueur.
#pragma once

#include <cstdint>

namespace BRMission
{
	/** Version de generation des missions (sauvegardes : une session d'une autre version garde son ancien mode) */
	constexpr int GenVersion = 2;
	constexpr int MaxDevices = 24;
	constexpr int MaxSteps = 4;
	constexpr int MaxParams = 24;
	/** Symboles des indices (meme ordre dans le jeu et dans les textes) */
	constexpr int NumSymbols = 6;
	/** Sortie speciale du Niveau 11 : la fin de la campagne (positive : les cibles negatives tirent un niveau au hasard) */
	constexpr int EndingTarget = 999;
	/** Trousseau du Niveau 5 : cles portees en meme temps par l'equipe */
	constexpr int KeyRingSize = 3;
	/** Taille maximale d'un etat serialise */
	constexpr int MaxBlob = 16 + MaxDevices;

	enum class EKind : uint8_t
	{
		/** Panneau, schema, registre : se lit (son contenu : GetClue) */
		Clue,
		/** Anomalie, boite de jonction, marque : se documente en la regardant (maintenir), contenu : GetClue */
		Observe,
		/** Cadran, vanne, disjoncteur, levier : Positions positions */
		Switch,
		/** Prise, serrure : accepte un objet d'equipe (Need) */
		Socket,
		/** Objet d'equipe (fusible, cle) : Need = sorte d'objet donnee */
		Item,
		/** Manivelle, treuil, generateur : progression gardee (Positions = unites a atteindre) */
		Crank,
		/** Bouton : valide une configuration (ascenseur, code, alimentation) */
		Button,
		/** Element visible qui suit l'etat (porte, vapeur, passerelle, eau) : pas d'interaction */
		Gate
	};

	enum class EAction : uint8_t
	{
		/** Lire, prendre, inserer, basculer (position suivante), appuyer */
		Use,
		/** Regler un interrupteur sur une position (Value) */
		Set,
		/** Manivelle ou observation : Value unites de progression (duree verifiee par l'hote) */
		Hold
	};

	/** Retour d'une action (le jeu en fait un message court et un son) */
	enum class EFeedback : uint8_t
	{
		None,
		/** Une etape precedente manque (Related : appareil qui debloque) */
		Locked,
		/** L'objet d'equipe requis manque */
		NeedItem,
		/** Aucun objet porte ne convient a cette prise */
		WrongItem,
		/** Puissance limitee : trop de circuits a la fois (Related : circuit deja allume) */
		Overload,
		/** Pas de fusible dans la prise de ce circuit (Related : la prise) */
		NoFuse,
		/** Code ou reglage incorrect, rien n'est perdu (Count : elements faux, si le niveau le dit) */
		Wrong,
		/** Avertissement : fuite de vapeur, bassin qui deborde, pression trop haute (reversible) */
		Warning,
		/** Defaut : tout a disjoncte (relais remis a zero, a refaire) */
		Tripped,
		/** Deja fait, deja pris */
		AlreadyDone,
		/** Reussi */
		Done,
		/** Progression (manivelle, observation) ; reglage change */
		Progress,
		/** Trousseau plein */
		Full,
		/** Objet remis a sa place */
		Returned,
		/** Ordre non respecte (balise, treuil) : rien ne bouge */
		Order,
		/** Hors service (balise leurre) */
		Dead
	};

	struct FDevice
	{
		EKind Kind = EKind::Gate;
		/** Role dans le niveau (textes, aspect) : voir ERole */
		uint8_t Role = 0;
		/** Ce qui est inscrit sur l'appareil, visible de tous : symbole, lettre (0-3 = A-D), numero, rang */
		uint8_t Label = 0;
		/** Interrupteur : nombre de positions ; manivelle et observation : unites a atteindre */
		uint8_t Positions = 1;
		/** Prise : sorte d'objet acceptee ; objet : sorte donnee */
		uint8_t Need = 0;
		/** Contenu cache (indice, observation) : le jeu ne le montre que par GetClue */
		uint8_t Info[2] = { 0, 0 };
		/** Placement : 0 pres du depart ... 3 loin ; 4 : salle de la mission (pres des sorties gardees) */
		uint8_t Zone = 1;
		/** Appareils places ensemble (meme mur, meme piece) : meme groupe ; 0 = seul */
		uint8_t Group = 0;
		/** Objectif facultatif */
		bool bOptional = false;
	};

	/** Roles (textes et aspect dans le jeu) */
	enum ERole : uint8_t
	{
		R_None,
		// Niveau 0
		R_Anomaly, R_MaintNote, R_Dial, R_Stabilize, R_RouteLever, R_Passage,
		// Niveau 1
		R_Schematic, R_Fuse, R_FuseSocket, R_Breaker, R_ElevatorCall, R_ReserveLights, R_ReserveNote,
		// Niveau 2
		R_PressurePlate, R_Gauge, R_Valve, R_SteamDoor,
		// Niveau 3
		R_LoadBoard, R_JunctionBox, R_Relay, R_ElevatorPower,
		// Niveau 4
		R_Planning, R_Directory, R_Archive, R_CodeDial, R_CodeEnter, R_HotelAccess,
		// Niveau 5
		R_Register, R_Key, R_Lock, R_BoilerNote, R_BoilerDial, R_BoilerPassage, R_StaffNote,
		// Niveau 6
		R_StartPlate, R_Beacon, R_EmergencyPower, R_LightsExit,
		// Niveau 8
		R_PassageMarks, R_Winch, R_Bridge,
		// Niveau 9
		R_HousePlan, R_StreetBox, R_HouseMarker, R_HouseDoor,
		// Niveau 10
		R_FenceMark, R_BarnBoard, R_MillDial, R_MillBrake, R_BarnDoor,
		// Niveau 11
		R_Generator, R_CityBoard, R_DestDial, R_DestConfirm, R_StationGate,
		// Niveau 37
		R_LevelMarks, R_Current, R_Sluice, R_DryPassage, R_DiveLog,
		R_Count
	};

	/** Donnees de campagne utiles a une mission */
	struct FCampaign
	{
		/** Bits 0..2 : fragment de route connu (Niveau 11 : chiffre de destination). Voir RouteBitFor */
		uint8_t RouteBits = 0;
		/** Objectifs facultatifs remplis pendant la campagne (bits, voir OptionalBitFor) : variante de la fin */
		uint16_t OptionalFound = 0;
	};

	struct FPlan
	{
		int Level = -1;
		uint32_t Seed = 0;
		int NumDevices = 0;
		FDevice Devices[MaxDevices];
		int NumSteps = 0;
		/** Parametres tires (solution) : lus par Evaluate et Act, jamais montres directement au joueur */
		uint8_t Params[MaxParams] = {};
		/** Variante de secours deterministe utilisee (graine invalide ou placement impossible) */
		bool bFallback = false;
		/** Niveau 0 : la mission ouvre l'une ou l'autre sortie selon le levier de route */
		bool bHasRoute = false;
	};

	struct FState
	{
		/** Etat de chaque appareil : position, lu/observe, rempli, pris, progression */
		uint8_t Dev[MaxDevices] = {};
		/** Bit 0 : mission resolue (sorties ouvertes, verrou leve pour toujours) */
		uint8_t Solved = 0;
		/** Nombre d'erreurs (statistique, retour au joueur) */
		uint8_t Mistakes = 0;
	};

	enum class EStep : uint8_t { Hidden, Available, InProgress, Done };

	struct FEval
	{
		int NumSteps = 0;
		EStep Steps[MaxSteps] = {};
		/** Progression de chaque etape (affichage k/n) */
		uint8_t Progress[MaxSteps] = {};
		uint8_t Goal[MaxSteps] = {};
		/** Mission resolue : sorties gardees ouvertes */
		bool bSolved = false;
		/** Niveau 0 : route ouverte (0 vers le Niveau 1, 1 vers les Poolrooms) ; 255 : aucune */
		uint8_t Route = 255;
		/** Ouverture de chaque element visible (0..255) : porte, vapeur, passerelle, eau, eclairage */
		uint8_t Gate[MaxDevices] = {};
		/** Avertissement en cours (vapeur qui fuit, bassin qui deborde) : appareil concerne, 255 sinon */
		uint8_t WarningDevice = 255;
		/** Objectif facultatif du niveau rempli */
		bool bOptionalDone = false;
	};

	struct FResult
	{
		bool bChanged = false;
		EFeedback Feedback = EFeedback::None;
		/** Appareil concerne par le retour (Locked : celui qui debloque ; Overload : circuit allume ; NoFuse : prise) */
		uint8_t Related = 255;
		/** Wrong : nombre d'elements faux quand le niveau l'indique (Niveau 0, 1, 3), 0 sinon */
		uint8_t Count = 0;
	};

	/** Contenu d'un indice, d'une observation ou d'une inscription visible (le jeu compose le texte) */
	enum class EClue : uint8_t
	{
		None,
		AnomalyBlinks,  // A0 symbole, B0 clignotements
		MaintRule,      // regle du panneau (aucune donnee)
		Schematic,      // A0, A1 circuits de l'ascenseur, A2 circuit de la reserve (lettres)
		ReserveNote,    // texte
		PressureTargets,// Ai manometre, Bi pression voulue
		GaugeFeed,      // A0 manometre, B0 vanne qui l'alimente
		LoadBoard,      // Ai relais (1-4), Bi secteur qu'il alimente (A-D)
		JunctionState,  // A0 secteur, B0 1 si en defaut
		Planning,       // A0..A2 symboles des badges de garde, dans l'ordre
		Directory,      // Ai symbole, Bi numero de bureau (0-9)
		Archive,        // texte (Skin-Stealer)
		Register,       // Ai symbole de serrure, Bi chambre (100 + Bi)
		KeyTag,         // A0 chambre (100 + A0)
		BoilerPressure, // A0 pression
		StaffNote,      // texte (imposteur)
		StartBeacon,    // A0 symbole de la premiere balise
		BeaconNext,     // A0 symbole de cette balise, B0 suivante (254 : alimentation de secours, 255 : hors service)
		WinchOrder,     // A0..A2 lettres des treuils, dans l'ordre
		CircuitPlan,    // A0 circuit de rue (1-4)
		PorchPlan,      // A0 symbole du porche
		WindowsPlan,    // A0 nombre de fenetres
		HouseFacade,    // A0 symbole du porche, B0 fenetres (visible sur la maison)
		TargetBarn,     // A0 symbole de la grange
		BarnDirections, // Ai symbole, Bi direction (0-7, fois 45 degres)
		RouteDigit,     // A0 rang (0-2), B0 chiffre
		LevelMarks,     // A0 = 0 bassin A, B0 niveau voulu ; A1 = 1 bassin B, B1 niveau voulu (graduations 0-7)
		CurrentDir,     // regle du courant (A se deverse dans B, B se vide dans l'evacuation)
		DiveLog         // texte
	};

	struct FClue
	{
		EClue Kind = EClue::None;
		uint8_t N = 0;
		uint8_t A[6] = {};
		uint8_t B[6] = {};
	};

	/** Le niveau a une mission */
	bool HasMission(int Level);
	/** Plan deterministe d'un niveau (meme plan chez tous les joueurs pour la meme graine) */
	void BuildPlan(int Level, uint32_t Seed, FPlan& Out);
	/** Variante de secours deterministe (ne depend pas de la graine), toujours valide */
	void BuildFallbackPlan(int Level, uint32_t Seed, FPlan& Out);
	/** Etat de depart d'une mission */
	void InitState(const FPlan& Plan, FState& Out);
	/** Applique une action si elle est permise ; l'etat ne change que si Result.bChanged */
	FResult Act(const FPlan& Plan, FState& State, const FCampaign& Campaign, int Device, EAction Action, int Value);
	/** Etapes, sorties et elements visibles */
	void Evaluate(const FPlan& Plan, const FState& State, const FCampaign& Campaign, FEval& Out);
	/** Un appareil peut etre utilise maintenant (sinon : Locked, Related = appareil qui debloque) */
	FResult CanUse(const FPlan& Plan, const FState& State, const FCampaign& Campaign, int Device);
	/** Contenu connu d'un appareil : false tant qu'il n'a pas ete lu ou observe (ou s'il n'a rien a dire) */
	bool GetClue(const FPlan& Plan, const FState& State, int Device, FClue& Out);
	/** Objets d'equipe de cette sorte portes (pris et pas encore inseres) */
	int Held(const FPlan& Plan, const FState& State, int Kind);
	/** Niveau 11 : chiffre Index de la destination connu (panneau lu ou fragment de campagne) ; -1 sinon */
	int KnownDigit(const FPlan& Plan, const FState& State, const FCampaign& Campaign, int Index);
	/** Niveau 37 : hauteur d'eau des bassins A et B (graduations 0..7) */
	void PoolLevels(const FPlan& Plan, const FState& State, int& OutA, int& OutB);
	/** Niveau 2 : pression lue sur un manometre (position de la vanne qui l'alimente) */
	int GaugeReading(const FPlan& Plan, const FState& State, int Device);
	/** Une sortie vers Target est ouverte (sorties de retour toujours ouvertes ; EndingTarget : fin du Niveau 11) */
	bool IsExitOpen(const FPlan& Plan, const FState& State, int Target);
	/** Une sortie vers Target attend la mission (pour le message "sortie verrouillee") */
	bool IsExitGuarded(int Level, int Target);
	/** Fragment de route donne par la mission d'un niveau (bit de FCampaign::RouteBits), 0 si aucun */
	uint8_t RouteBitFor(int Level);
	/** Objectif facultatif d'un niveau (bit de FCampaign::OptionalFound), 0 si aucun */
	uint16_t OptionalBitFor(int Level);
	/** Fin du Niveau 11 : variante si au moins trois objectifs facultatifs ont ete remplis pendant la campagne */
	bool EndingVariant(const FCampaign& Campaign);

	/** Serialisation compacte (sauvegarde, instantane reseau) : version, empreinte du plan, etat. Taille ou 0 */
	int Serialize(const FPlan& Plan, const FState& State, uint8_t* Out, int Capacity);
	/** false si le blob ne correspond pas a ce plan (autre graine, autre version) ; l'etat est assaini */
	bool Deserialize(const FPlan& Plan, const uint8_t* In, int Size, FState& Out);
	/** Empreinte du plan (graine, parametres, appareils) */
	uint32_t Fingerprint(const FPlan& Plan);

	/** Generateur deterministe (graine, sel) */
	uint32_t Hash(uint32_t Seed, uint32_t Salt);
}
