// v4.12 : effets physiques des missions, sans moteur : eau locale (bassins et sas des Poolrooms) et passerelle du
// Niveau 8. Le jeu (ABRWorld, ABRMissionDevice, ABRCharacter, ABREntity) et le banc Tools/Mechanisms/test_mechanisms.cpp
// utilisent ces fonctions et ces cotes : la surface rendue, la profondeur, la nage, l'immersion de la camera, la ligne
// d'eau, les collisions et la validation de sortie suivent un seul etat, celui de la mission (replique, sauvegarde).
//
// Reperes : un module est construit dans le repere d'un mecanisme (FFrame : origine au sol, X vers la salle, Y lateral,
// Z vers le haut, lacet multiple de 90 degres). Les hauteurs absolues sont en cm, sol des salles a 0.
#pragma once

#include "BRMissionLogic.h"

#include <cstdint>

namespace BRMech
{
	struct FVec3
	{
		float X = 0.f;
		float Y = 0.f;
		float Z = 0.f;
	};

	/** Repere d'un mecanisme : origine (monde) et lacet (degres, multiple de 90) */
	struct FFrame
	{
		float X = 0.f;
		float Y = 0.f;
		float Z = 0.f;
		float Yaw = 0.f;
		/** Point local (X vers la salle, Y lateral, Z absolu ou relatif selon l'appelant) vers le monde (plan) */
		void ToWorld(float LX, float LY, float& OutX, float& OutY) const;
		/** Point du monde (plan) vers le repere local */
		void ToLocal(float WX, float WY, float& OutLX, float& OutLY) const;
	};

	/** Module physique d'un mecanisme (FBRMissionSpot::Module) */
	namespace Module
	{
		constexpr uint8_t None = 0;
		/** Vanne A des Poolrooms : porte les deux bassins, leurs regles graduees et leurs conduites */
		constexpr uint8_t PoolTanks = 1;
		/** Vanne B des Poolrooms : fixee sur l'avant du bassin B */
		constexpr uint8_t PoolWheelB = 2;
		/** Passage sec des Poolrooms : sas inonde devant l'echelle, deversoir */
		constexpr uint8_t PoolLock = 3;
		/** Passerelle du Niveau 8 : palier, interruption, appui, tablier, portique */
		constexpr uint8_t Bridge = 4;
		/** v4.13 : porte du passage du Level 6, dans une ouverture d'entree de l'aile LiDAR : cadre, battant, plaque */
		constexpr uint8_t LidarDoor = 5;
	}

	/** v4.13 : porte du passage du Level 6 (repere de la porte : origine au sol au milieu de l'ouverture, X vers la galerie,
	 *  Y lateral). Le battant ferme bloque l'ouverture (largeur DoorWidth du niveau, 140 cm), pivote sur ses charnieres
	 *  vers l'aile quand c'est la bonne porte */
	namespace LidarDoor
	{
		constexpr float Width = 140.f;
		constexpr float Height = 225.f;
		constexpr float LeafThick = 6.f;
		constexpr float OpenAngle = 95.f;
		/** Plaque (silhouette et code, en relief) sur la face cote galerie, a hauteur des yeux */
		constexpr float PlateZ = 150.f;
		constexpr float PlateSize = 56.f;
		/** Apres un refus, la porte se rearme (s) : on ne peut pas essayer les quatre portes d'un coup */
		constexpr float Rearm = 4.f;
	}

	// =================================================================================================================
	// Eau locale
	// =================================================================================================================

	/** Volume d'eau local : rectangle aligne sur les axes du monde, fond (sol de marche dans le volume), haut des parois et
	 *  surface courante (animee vers la cible donnee par l'etat de la mission) */
	struct FWaterBox
	{
		float MinX = 0.f, MinY = 0.f, MaxX = 0.f, MaxY = 0.f;
		float FloorZ = 0.f;
		float RimZ = 0.f;
		float Surface = 0.f;
	};
	/** Rectangle local [LX0, LX1] x [LY0, LY1] d'un repere, en boite du monde */
	FWaterBox MakeBox(const FFrame& F, float LX0, float LY0, float LX1, float LY1, float FloorZ, float RimZ, float Surface);

	struct FWaterQuery
	{
		/** Dans un volume local (en plan, et en hauteur entre son fond - 60 cm et son bord + 250 cm) */
		bool bLocal = false;
		/** De l'eau au-dessus du fond */
		bool bWater = false;
		/** Hauteur de la surface (-1e6 sans eau) et du fond */
		float Surface = -1.0e6f;
		float Floor = 0.f;
		int Box = -1;
	};
	/** Eau au point (X, Y, Z) : un volume local qui le contient l'emporte sur l'eau du niveau (bLevelWater, hauteur
	 *  LevelSurface, fond LevelFloor) */
	FWaterQuery WaterAt(const FWaterBox* Boxes, int Num, float X, float Y, float Z, bool bLevelWater, float LevelSurface, float LevelFloor);

	/** Profondeur au-dela de laquelle on nage (au-dessous : on marche dans l'eau), comme ABRCharacter::UpdateWater */
	constexpr float SwimDepth = 120.f;

	/** Approche a vitesse constante (cm/s) */
	float Approach(float Cur, float Target, float Speed, float Dt);

	// =================================================================================================================
	// Niveau 37 : deux bassins, deux vannes, un sas devant l'echelle de sortie
	// =================================================================================================================
	namespace Pool
	{
		/** Les deux bassins sont adosses au mur de la vanne A ; les vannes sont fixees sur leur face avant */
		constexpr float TankDepth = 120.f;
		/** Vanne B : a cote de la vanne A, sur la meme face (repere de la vanne A). v4.16 : 98 cm (avant : 58). Les boites visees
		 *  des deux volants (BRMech::Clearance::Of, R_Sluice : 32 cm de part et d'autre de leur axe, 24 cm de profondeur, la meme
		 *  face avant) se chevauchaient de 6 cm : de biais, du cote de B (au-dela de 54 degres de l'avant de A), la sphere de la
		 *  visee du client lancee vers le moyeu de A touchait la boite de B avant celle de A, le moyeu de A bien visible (Niveau 37,
		 *  graine 3053711763 : "mecanisme 2 (Vanne du bassin A) atteint mais PAS VISE", vise : vanne B). A 98 cm, 34 cm separent
		 *  les deux boites ; BRMech::Clearance::FanSteals le verifie pour les places de biais des tests et leurs arrivees */
		constexpr float WheelLateralB = 98.f;
		/** v4.16 : vidange du bassin B (conduite, bouche, grille, regard de l'indicateur de courant) entre les deux volants, et
		 *  regles graduees des bassins (repere de la vanne A). Avant : vidange a 96 cm et regle de B a 118 cm, de part et d'autre
		 *  du volant de B a 58 cm ; le volant de B a 98 cm y passerait. Chaque piece de l'avant du bassin B (volants et leurs
		 *  graduations, regard, regle) a sa bande laterale, sans recouvrement (Tools/verify_device_clearance.py, cas G) */
		constexpr float DrainLateral = 49.f;
		constexpr float GaugeLateralA = -78.f;
		constexpr float GaugeLateralB = 148.f;
		/** Bassin A (repere de la vanne A, X depuis la face avant des bassins vers le mur : -TankDepth..0) */
		constexpr float AY0 = -112.f, AY1 = -8.f;
		/** Bassin B, plus bas, a cote */
		constexpr float BY0 = 8.f, BY1 = 152.f;
		constexpr float Wall = 10.f;
		/** Une marque de regle graduee (cm) et la hauteur de la marque 0 au-dessus du fond */
		constexpr float MarkStep = 15.f;
		constexpr float MarkZero = 8.f;
		constexpr int MarksA = 5;
		constexpr int MarksB = 7;
		/** Fonds absolus : A au-dessus de B (A se deverse dans B par la vanne A) ; les deux au-dessus de l'eau des canaux */
		float BottomA(float LevelWater);
		float BottomB(float LevelWater);
		float RimA(float LevelWater);
		float RimB(float LevelWater);
		/** Surface d'un bassin a une marque (bornee a sa regle) */
		float SurfaceA(int Mark, float LevelWater);
		float SurfaceB(int Mark, float LevelWater);
		/** Marque lue sur la regle pour une surface (arrondie) */
		int MarkOfA(float Surface, float LevelWater);
		int MarkOfB(float Surface, float LevelWater);
		/** Vitesse d'animation d'une surface de bassin (cm/s) : un cran de vanne se voit couler en ~0,6 s */
		constexpr float TankSpeed = 26.f;

		/** Sas du passage sec, en repere de la porte (origine a l'entree du sas, X vers la salle) : de -ChannelLength
		 *  (mur de l'echelle) a 0 */
		constexpr float ChannelLength = 280.f;
		constexpr float ChannelHalfWidth = 85.f;
		constexpr float ChannelWall = 12.f;
		/** Murets du sas (au-dessus du dallage) et garde-corps */
		constexpr float ChannelWallHeight = 120.f;
		constexpr float RailHeight = 200.f;
		/** Marche d'entree devant le sas (le dallage est au-dessus de l'eau des canaux) */
		constexpr float EntryStepDepth = 40.f;
		/** Dallage du sas : au ras des trottoirs, au-dessus de l'eau des canaux */
		float SlabTop(float LevelWater, float DeckHeight);
		float EntryStepTop(float LevelWater, float DeckHeight);
		/** Eau du sas ferme (on y marcherait, on n'y nagerait pas) et sas vide (sous le dallage) */
		float ChannelFull(float Slab);
		float ChannelDry(float Slab);
		/** Vidange (cm/s) une fois les bassins equilibres */
		constexpr float DrainSpeed = 25.f;
		/** Hauteur visible du deversoir (au-dessus du dallage) pour une surface donnee : il descend juste devant l'eau */
		float WeirHeight(float Surface, float Slab);
		constexpr float WeirMax = 112.f;
	}

	// =================================================================================================================
	// Niveau 8 : passerelle sur une vraie interruption, entre deux appuis
	// =================================================================================================================
	namespace Bridge
	{
		/** Repere de la passerelle : origine au pied de l'arete avant du palier de l'echelle (sol de la grotte), X vers la
		 *  salle, Y lateral */
		constexpr float FarDepth = 110.f;     // palier de l'echelle (du mur a l'arete)
		constexpr float FarTop = 140.f;       // hauteur du palier
		constexpr float Gap = 220.f;          // l'interruption
		constexpr float NearDepth = 90.f;     // appui d'arrivee
		constexpr float NearTop = 90.f;
		constexpr float StepDepth = 30.f;     // deux marches de 30 cm derriere l'appui
		constexpr int Steps = 2;
		constexpr float HalfWidth = 110.f;    // largeur des appuis : on passe a cote, au sol, des deux cotes
		constexpr float DeckThick = 12.f;
		constexpr float DeckHalfWidth = 80.f;
		/** Le tablier repose sur l'appui d'arrivee sur cette longueur */
		constexpr float Bearing = 20.f;
		/** Longueur totale du module depuis le mur */
		constexpr float TotalLength = FarDepth + Gap + NearDepth + Steps * StepDepth;
		/** Charniere (haut de l'arete du palier) : X = 0, Z = FarTop - DeckThick / 2 (axe du tablier) */
		float HingeZ();
		/** Longueur du tablier et inclinaison une fois pose (degres, positif vers le bas) */
		float DeckLength();
		float LoweredPitch();
		/** Tangage du tablier (degres, convention Unreal : positif = nez vers le haut) pour une progression 0..1 :
		 *  0 = leve (vertical), 1 = pose sur l'appui */
		float DeckPitch(float Progress);
		/** Bout libre du tablier (axe), en repere local */
		void DeckTip(float Progress, float& OutX, float& OutZ);
		/** Hauteur du dessus du tablier a la distance X de l'arete (tablier pose) */
		float DeckTopAt(float X);
		/** Etape suivante d'une entite au sol (From) vers Goal (positions monde, centre du corps) quand l'un des deux est
		 *  sur le module : marches, appui, tablier pose, palier. Tablier leve : attente au pied du palier. false : rien a
		 *  contourner */
		bool Detour(const FFrame& Frame, bool bDown, const FVec3& From, const FVec3& Goal, FVec3& OutWaypoint);
		/** Vitesse d'animation de la progression (par seconde) */
		constexpr float Speed = 0.35f;
		/** Saut (cm) que le personnage peut gagner (JumpZVelocity 360, gravite 980) */
		constexpr float JumpRise = 66.f;
		/** Marche franchie sans sauter (MaxStepHeight) */
		constexpr float MaxStep = 35.f;
	}

	// =================================================================================================================
	// v4.15 : emprise des mecanismes et places libres. Le placement (ABRWorld::PlaceMission), les rochers des grottes
	// (ABRChunk), les tests du jeu et les bancs (Tools/Mechanisms, Tools/verify_device_clearance.py) partagent ces regles :
	// le point d'interaction d'un mecanisme (centre de sa boite visee, ABRMissionDevice::GetInteractPoint) n'est jamais
	// dans le volume bloquant d'un autre (cage, porte fermee, module), et la place devant lui reste libre. v4.15 (correctifs 3) :
	// et son couloir de visee (de son point vise a la place ordinaire devant lui) ne passe ni par la boite visee ni par le volume
	// d'un autre : la visee du client depuis cette place le prend, lui
	// =================================================================================================================

	/** Hauteur de travail des commandes murales (cm au-dessus du sol) */
	namespace Device
	{
		constexpr float WorkZ = 120.f;
	}

	/** v4.13 : cage de confinement du Level 6, contre le mur (repere du mecanisme : X depuis le mur) ; son volume bloquant
	 *  (tous les canaux) occupe toute la cage : on ne la traverse pas, le scanner la touche */
	namespace LidarCage
	{
		constexpr float Gap = 12.f;
		constexpr float Depth = 110.f;
		constexpr float HalfWidth = 60.f;
		constexpr float Height = 210.f;
	}

	namespace Clearance
	{
		/** Capsule du joueur (ABRCharacter : rayon 34 cm) et marge des tests */
		constexpr float PlayerRadius = 34.f;
		constexpr float Margin = 2.f;
		/** Place debout devant un point vise : centre de la capsule a cette distance devant lui */
		constexpr float StandGap = PlayerRadius + Margin;
		/** Profondeur libre devant un mecanisme que le decor (rochers) lui laisse : on s'y tient face a lui, a portee */
		constexpr float StandDepth = 120.f;
		/** Un volume plus bas que la marche franchie sans sauter ne gene pas la place debout (on monte dessus) */
		constexpr float StepHeight = 35.f;
		/** v4.15 (correctifs 3) : sonde du point d'interaction des tests (ABRWorld::FindMissionDeviceObstruction) : un point
		 *  vraiment dans un volume la touche, un point devant un mur ne la touche pas. Rayon toujours sous le plus petit ecart
		 *  entre l'origine murale d'un mecanisme et son point vise (Aim.X : marques de la cloture 4 cm ; indices, cles, mire
		 *  5 cm ; cadrans et boutons 6 cm). Avant : 5 cm, tangente au mur pour les mecanismes a 5 cm (Chaos compte le contact) */
		constexpr float AimProbe = 1.f;
		/** v4.15 (correctifs 3) : visee du client (ABRCharacter::UpdateFocus) : rayon de la sphere lancee dans l'axe du regard */
		constexpr float FocusSweep = 7.f;
		/** v4.15 (correctifs 3) : place ordinaire devant un point vise (premier cercle de BRTestV412::FindMissionStand) et
		 *  yeux d'un joueur debout au-dessus du sol (capsule de 88 cm de demi-hauteur, camera 74 cm au-dessus de son centre) */
		constexpr float StandRange = 110.f;
		constexpr float StandEye = 162.f;

		/** Boite locale (cm) : X vers la salle depuis l'origine du mecanisme, Y lateral (axe droit de l'acteur), Z depuis
		 *  le sol de l'emplacement */
		struct FBox
		{
			float X0 = 0.f, Y0 = 0.f, Z0 = 0.f, X1 = 0.f, Y1 = 0.f, Z1 = 0.f;
		};

		struct FFootprint
		{
			/** Boite visee (ABRMissionDevice::Box) : centre (point d'interaction) et demi-tailles */
			FVec3 Aim;
			FVec3 AimExtent;
			/** Volume qui bloque le passage et la vue tant que le mecanisme est la (cage, porte fermee, module) */
			bool bSolid = false;
			FBox Solid;
			/** v4.15 (correctifs 3) : la boite visee arrete la visee du client (faux pour une porte ou un passage de la mission :
			 *  leur boite n'a pas de collision, ABRMissionDevice::SetShown) */
			bool bAimable = true;
		};
		/** Emprise d'un mecanisme d'apres sa sorte, son role et son module (BRMech::Module) ; WallThickness : epaisseur des
		 *  murs du niveau (cadre des portes du Level 6). ABRMissionDevice::Init en tire sa boite visee */
		FFootprint Of(BRMission::EKind Kind, uint8_t Role, uint8_t ModuleId, float WallThickness);

		/** Mecanisme pose : repere (origine au sol de l'emplacement, lacet multiple de 90 degres) et emprise */
		struct FPlaced
		{
			FFrame Frame;
			FFootprint Print;
		};
		/** Point d'interaction d'un mecanisme pose (monde) */
		FVec3 AimPoint(const FPlaced& P);
		/** Point du monde dans le volume d'un mecanisme pose, grossi de Grow (plan et hauteur) */
		bool InSolid(const FPlaced& P, float X, float Y, float Z, float Grow);
		/** v4.15 : deux mecanismes se genent : le point vise de l'un dans le volume de l'autre (la ligne de vue de l'hote
		 *  finit dedans : action toujours refusee), la place debout juste devant lui coupee par ce volume, ou deux volumes
		 *  qui se chevauchent */
		bool Clash(const FPlaced& A, const FPlaced& B);
		/** v4.15 (correctifs 3) : couloir de visee de Me coupe par Other. Du point vise de Me vers les yeux d'un joueur debout a
		 *  la place ordinaire (StandRange devant lui, StandEye au-dessus de son sol), et vers ce meme point a hauteur du point
		 *  vise (place plus lointaine, joueur accroupi) : la boite visee de Other (si elle arrete la visee) ou son volume passe
		 *  a moins de FocusSweep + Margin. Le client viserait Other depuis la place devant Me (Niveau 10 : tableau des granges
		 *  entre le moulin et le frein, face a face sur leurs poteaux). Geometrie pure : meme resultat chez tous */
		bool CutsFocusLane(const FPlaced& Me, const FPlaced& Other);
		/** v4.15 (correctifs 3) : l'un des deux couloirs de visee coupe par l'autre mecanisme */
		bool FocusClash(const FPlaced& A, const FPlaced& B);
		/** v4.16 : ecart (cm) entre les boites visees de A et de B (lacets au quart de tour) ; 0 si elles se touchent ou se
		 *  chevauchent, -1 si l'une n'arrete pas la visee (porte ou passage de la mission). Niveau 37 : vannes A et B a 58 cm
		 *  l'une de l'autre (v4.12 a v4.16) : 0, boites chevauchees de 6 cm */
		float AimGap(const FPlaced& A, const FPlaced& B);
		/** v4.16 : places de biais des tests du jeu (BRTestV412::FindMissionStand : cercles FanRings, pas de FanStep degres depuis
		 *  l'avant du mecanisme), jugees par FanSteals de l'avant jusqu'a FanTurns pas de chaque cote. Une marche des tests
		 *  s'arrete a FanArrival du but (place autour du mecanisme ; FanArrivalFront pour la place devant lui, au premier cercle)
		 *  puis vise ; portee de la visee du client FanReach (BRTestV412 : ClientFocusReach). Tools/verify_device_clearance.py lit
		 *  les memes valeurs dans BRAutoTestV412.cpp et BRAutoTestWalk.h (ECHEC si elles different) */
		constexpr int NumFanRings = 5;
		constexpr float FanRings[NumFanRings] = { StandRange, 160.f, 210.f, 75.f, 240.f };
		constexpr float FanStep = 22.5f;
		constexpr int FanTurns = 2;
		constexpr float FanArrival = 35.f;
		constexpr float FanArrivalFront = 70.f;
		constexpr float FanReach = 250.f;
		/** Pas de la grille des points d'arrivee (cm) */
		constexpr float FanGrid = 5.f;
		/** v4.16 : visee de Me prise par Other depuis une place de biais. Chaque place des cercles FanRings, de l'avant de Me
		 *  jusqu'a FanTurns pas de FanStep de chaque cote, et chaque arrivee de la marche vers elle (grille de FanGrid dans le
		 *  disque de FanArrival, FanArrivalFront pour la place devant au premier cercle), au sol de Me releve de FloorLift :
		 *  capsule (PlayerRadius + Margin) hors des volumes de Me et de Other, yeux (StandEye) a portee (FanReach), ligne de vue
		 *  de l'hote libre du volume de Other (ABRWorld::CanReachMissionDevice ignore le mecanisme vise). La sphere de la visee du
		 *  client (FocusSweep, ABRCharacter::UpdateFocus) lancee des yeux vers le point vise de Me touche la boite visee de Other
		 *  au plus tard avec celle de Me, avant tout volume : visee prise. Rend le nombre de ces points (0 : la visee prend
		 *  toujours Me). Niveau 37, graine 3053711763 : vanne B a 58 cm de la vanne A ; d'apres le rapport et la capture (yeux a
		 *  169 cm, 51 a 54 degres de l'avant de A du cote de B), place a 45 degres au cercle de 160 cm et arrivee plus pres de B :
		 *  visee prise par B (seuil du modele : 54 degres). OutFirst : yeux du premier de ces points, depuis
		 *  le point vise de Me (X devant lui, Y lateral, Z vers le haut). Geometrie pure, nombre de pas fixe : meme resultat chez
		 *  tous */
		int FanSteals(const FPlaced& Me, const FPlaced& Other, float FloorLift, FVec3* OutFirst = nullptr);
		/** v4.15 (correctifs 4) : verdict de l'invariant du placement pour le couloir de visee de Me face a Other
		 *  (ABRWorld::PlaceMission, LogMissionVolumeInvariant) */
		enum class ELaneCut : uint8_t
		{
			/** Couloir degage (CutsFocusLane faux) */
			Clear,
			/** Couloir coupe : avertissement du journal */
			Logged,
			/** v4.15 (correctifs 3, relecture) : vanne B des Poolrooms (MeModule Module::PoolWheelB) devant les bassins de la vanne A
			 *  (OtherModule Module::PoolTanks). Le module les assemble apres PickSlot, qui ne les a jamais juges (vanne B fixee sur
			 *  l'avant du bassin B) : son point vise est a 8 cm du volume des bassins (moins que FocusSweep + Margin), mais sa boite
			 *  visee depasse de 20 cm l'avant des bassins : depuis la place devant elle, la visee du client la touche avant eux */
			PoolModule,
			/** Porte, grille ou portail ferme de la mission devant la sortie gardee (porte sans module : dalle de 30 cm, boite
			 *  visee sans collision) : la place ordinaire de Me (StandRange devant son point vise, a son sol) est dans cette dalle
			 *  grossie de la capsule (PlayerRadius). Personne ne s'y tient et la dalle ne prend pas la visee : la place se prend
			 *  autour, jugee en jeu (BRTestV412::FindMissionStand, visee du client comprise). Niveau 2 : vannes face a la porte de
			 *  la vapeur ; Niveau 4 : bouton du code a cote de la grille de l'hotel. La porte est posee avant les groupes : PickSlot
			 *  ne la voit jamais (placement inchange) */
			StandInGate,
			/** Passerelle du Niveau 8 : seule son enveloppe (Of : boite du module, sol de l'interruption compris) coupe le
			 *  couloir ; ses pieces (BridgeParts : palier, appui, marches, poteaux, tablier leve) le laissent degage (treuil de la
			 *  salle sur la paroi a cote de l'interruption, place ordinaire au sol de l'interruption) */
			BridgeEnvelope
		};
		/** v4.15 (correctifs 4) : verdict pour le couloir de Me (module MeModule) face a Other (module OtherModule). Une porte
		 *  dont la dalle coupe le couloir depuis une place libre (hors de la dalle), ou une piece de la passerelle qui le coupe,
		 *  reste journalisee (Logged) ; le sas des Poolrooms aussi (son volume est celui de ses murets) */
		ELaneCut LaneCut(const FPlaced& Me, uint8_t MeModule, const FPlaced& Other, uint8_t OtherModule);
		/** v4.15 (correctifs 3) : couloir coupe que l'invariant du placement journalise en avertissement : LaneCut == Logged.
		 *  Avant (correctifs 4) : tout couloir coupe sauf la vanne B des Poolrooms ; la porte de la mission (Niveaux 2 et 4,
		 *  place ordinaire dans la porte fermee) et l'enveloppe de la passerelle (Niveau 8, chaque graine) etaient
		 *  journalisees a chaque chargement, sans gene reelle */
		bool LaneCutLogged(const FPlaced& Me, uint8_t MeModule, const FPlaced& Other, uint8_t OtherModule);
		/** v4.15 (correctifs 4) : pieces de la passerelle du Niveau 8 qui arretent la vue de l'hote ou la visee du client
		 *  (ABRMissionDevice::BuildBridge, AddSolid), repere du module : palier, appui et ses marches, poteaux du portique et de
		 *  la main courante, tablier leve (tant que les treuils servent). Rend leur nombre (au plus MaxBridgeParts) */
		constexpr int MaxBridgeParts = 9;
		int BridgeParts(FBox* Out, int MaxParts);

		/** Rectangle oriente (plan) : centre, demi-tailles, lacet quelconque (degres) */
		struct FRect
		{
			float CX = 0.f, CY = 0.f, HX = 0.f, HY = 0.f, Yaw = 0.f;
		};
		/** Separation des axes : deux rectangles orientes se chevauchent (contact compris) */
		bool Overlap(const FRect& A, const FRect& B);
		/** Rectangle du monde d'une boite locale d'un repere */
		FRect RectOf(const FFrame& F, const FBox& B);
		/** Zone que le decor doit laisser libre autour d'un mecanisme pose : son volume, sa boite visee et la place devant
		 *  (StandDepth), elargie de la capsule */
		FRect KeepOut(const FPlaced& P);
		/** Zone libre devant une sortie posee par la mission (echelle, porte) : son pied et la place devant */
		FRect ExitKeepOut(const FFrame& F);

		/** Emplacement candidat du placement : repere, decalage lateral sur sa face, mural, encore libre (ni pris, ni
		 *  ecarte par l'appelant : devant une sortie, une porte de l'aile) */
		struct FSlot
		{
			FFrame Frame;
			float Lateral = 0.f;
			bool bWall = true;
			bool bOpen = true;
		};
		/** Premier emplacement libre ou un mecanisme d'emprise Print ne gene aucun de ceux deja poses et ou, mural, son
		 *  volume tient dans la face (|lateral| + demi-largeur <= FaceHalf). -1 si aucun. Sans volume ni voisin a volume,
		 *  c'est le premier emplacement libre (ordre du placement d'avant la v4.15). v4.15 (correctifs) : un volume mural
		 *  prend ensuite, sur la face de ce premier emplacement (meme lacet, meme point devant le centre de la cellule),
		 *  l'emplacement libre de plus grand |Lateral| ou il tient et ne gene personne (le premier a egalite) : le reste de la
		 *  face reste d'un seul tenant pour ses voisins (Level 6 : cage a +-58, poste a -+58). v4.15 (correctifs 3) : bFocusLanes :
		 *  d'abord sans couloir de visee coupe (FocusClash) avec ceux deja poses ; si aucun emplacement ne le permet, le choix
		 *  sans cette regle (le placement n'echoue jamais plus souvent qu'avant). false : le choix d'avant les couloirs
		 *  (comparaison des reprises du placement 2, ABRWorld::PlaceMission) */
		int PickSlot(const FFootprint& Print, const FSlot* Slots, int NumSlots, const FPlaced* Placed, int NumPlaced, float FaceHalf, bool bFocusLanes);
	}

	// =================================================================================================================
	// v4.15 (correctifs 4) : sorties tirees par les chunks sur une face de paroi et rochers des grottes. Le jeu (ABRChunk::PlanExits,
	// ABRChunk::StepPlan etape 5, ABRChunk::BuildCellProps, ABRWorld::IsChunkExitKeepOut) et le banc
	// Tools/Departure/test_departure.cpp partagent ces tirages : un rocher n'entre plus dans la zone libre (Clearance::ExitKeepOut)
	// d'une echelle tiree par un chunk (Niveau 8, partie classique : echelle enterree dans la boite de collision d'un rocher,
	// point vise par le depart de groupe, accroche et colonne du grimpeur compris ; ni visee, ni montee). v4.15 (correctifs 5) :
	// la face tiree laisse le grimpeur a plat sur le sol du pied (Grid::StandFlat : marches des trottoirs du Niveau 37)
	// =================================================================================================================
	namespace Grid
	{
		/** Hachage de la grille : identique a BRHash::Hash et BRHash::Rand (BRTypes.h) */
		uint32_t Hash(int32_t A, int32_t B, int32_t C, uint32_t Seed);
		float Rand(int32_t A, int32_t B, int32_t C, uint32_t Seed);
		/** Directions des faces d'une cellule, dans l'ordre d'ABRChunk (GDirs) : +X, -X, +Y, -Y */
		constexpr int32_t DirX[4] = { 1, -1, 0, 0 };
		constexpr int32_t DirY[4] = { 0, 0, 1, -1 };

		/** Le monde vu par le tirage d'une sortie murale (ABRWorld : grille, bassins, fosses, mission ; cachettes du chunk) */
		struct FFaceQuery
		{
			virtual ~FFaceQuery() = default;
			/** Une sortie peut se poser dans la cellule : marchable, hors du depart, d'un bassin, d'une cachette, d'une salle de
			 *  fosses et des cellules de la mission ; niveau fini : atteignable sans traverser de fosse */
			virtual bool CellOk(int32_t X, int32_t Y) const = 0;
			/** La face (DX, DY) de la cellule est une paroi : cellule voisine pleine (bOutSolid) ou mur fin */
			virtual bool IsFace(int32_t X, int32_t Y, int32_t DX, int32_t DY, bool& bOutSolid) const = 0;
			/** v4.15 (correctifs 5) : hauteur du sol de marche en (X, Y) qui ne depend que de la grille (ABRWorld::DeckZAt :
			 *  trottoirs du Niveau 37 et leur marche immergee) ; 0 sans trottoirs (tous les autres niveaux) */
			virtual float FloorAt(float /*X*/, float /*Y*/) const { return 0.f; }
		};
		/** Face tiree : cellule, direction (DirX/DirY), cellule voisine pleine (sinon mur fin) */
		struct FWallFace
		{
			int32_t CellX = 0;
			int32_t CellY = 0;
			int Dir = 0;
			bool bSolid = false;
		};

		/** v4.15 (correctifs 5) : accroche du grimpeur devant le pied d'une echelle (ABRExit::GetClimbAnchor : montants a 14 cm
		 *  du mur, capsule de 34 cm de rayon) */
		constexpr float ClimbAnchor = 52.f;
		/** v4.15 (correctifs 5) : sol du pied d'une sortie murale, pris a cette distance devant sa face (ABRChunk::PlanExits) */
		constexpr float FootProbe = 30.f;
		/** v4.15 (correctifs 5) : place debout devant une sortie (point vise par le depart de groupe a 1 m, BRGather::ViewPoint) */
		constexpr float StandAhead = 100.f;
		/** v4.15 (correctifs 5) : ecart de hauteur du sol admis sous le grimpeur et devant la sortie (cm) */
		constexpr float StandFlatTol = 1.f;
		/** v4.15 (correctifs 5) : le sol sous l'empreinte de la capsule du grimpeur (accroche ClimbAnchor, rayon du joueur et
		 *  marge des tests : profondeurs ClimbAnchor - 36, ClimbAnchor, ClimbAnchor + 36 et StandAhead, laterales -36, 0, +36)
		 *  est a la hauteur du pied (Q.FloorAt a FootProbe devant la face), a StandFlatTol pres. Avant : Niveau 37 (Poolrooms,
		 *  partie classique), echelle sur un mur sans trottoir, au fond du canal, a cote du trottoir d'un mur lateral : sa
		 *  marche immergee (40 x 34 cm, jusqu'a 220 cm de sa ligne) entrait de 1,19 cm dans la capsule du controle du
		 *  developpeur et de 4 cm dans celle du joueur (montee bloquee depuis le canal, descente arretee au-dessus du fond).
		 *  Sans trottoirs, le sol est plat partout : toujours vrai */
		bool StandFlat(const FWallFace& Face, float CellSize, float WallThickness, const FFaceQuery& Q);
		/** Face de paroi de la sortie ExitIndex (indice dans les sorties du niveau) du chunk (branche murale d'ABRChunk::PlanExits) :
		 *  24 essais tires (sels 1500 + ExitIndex * 64 + essai), puis, niveau fini (bBounded), toutes les faces du chunk dans un
		 *  ordre tire (sel 1500 + ExitIndex * 64 + 63). false : aucune face. v4.15 (correctifs 5) : une face ou le grimpeur ne
		 *  tient pas a plat (StandFlat, cellule CellSize, mur fin WallThickness) passe a l'essai suivant (memes sels, memes
		 *  essais : seules les faces refusees changent ; aucun tirage ne change sans trottoirs) */
		bool PickWallFace(int32_t ChunkX, int32_t ChunkY, int ExitIndex, uint32_t Seed, int ChunkCells, bool bBounded, float CellSize, float WallThickness, const FFaceQuery& Q,
			FWallFace& Out);
		/** Repere de la sortie posee sur cette face : pied sur la face (devant un mur fin : en retrait de la moitie de son
		 *  epaisseur), au sol FloorZ, X vers la cellule (lacet multiple de 90 degres) */
		FFrame WallFaceFrame(const FWallFace& Face, float CellSize, float WallThickness, float FloorZ);
	}

	namespace Cave
	{
		/** Rocher pose (SM_Rock) : position (cm, monde ; double comme le FVector du jeu), lacet (degres), echelle dans le plan et
		 *  en hauteur */
		struct FRock
		{
			double X = 0.0;
			double Y = 0.0;
			double Z = 0.0;
			float Yaw = 0.f;
			float Scale = 1.f;
			float ScaleZ = 1.f;
		};
		/** Bornes du modele SM_Rock (UStaticMesh::GetBoundingBox : donnee du jeu, lue par l'appelant ; le banc reprend celles
		 *  de RawAssets/Meshes/SM_Rock.fbx) */
		struct FMeshBounds
		{
			FVec3 Min;
			FVec3 Max;
		};
		/** Rocher de la face K (Grid::DirX/DirY) de la cellule pleine (X, Y), contre la cellule voisine libre (ABRChunk::StepPlan,
		 *  etape 5 : 55 % des faces, echelle 1 a 1,9, 1,4 fois plus haut, decale le long de la face, 30 cm dans
		 *  la paroi). false : pas de rocher sur cette face */
		bool WallRock(int32_t X, int32_t Y, int K, uint32_t Seed, float CellSize, FRock& Out);
		/** Rocher dans une cellule libre (ABRChunk::BuildCellProps, grottes ; l'appelant ecarte le depart, la cellule devant une
		 *  sortie et la mission). false : pas de rocher */
		bool CellRock(int32_t X, int32_t Y, uint32_t Seed, float CellSize, float PropDensity, FRock& Out);
		/** Boite de collision du rocher dans le plan (ABRChunk::AddProp : bornes du modele a l'echelle, tournees) */
		Clearance::FRect RockRect(const FRock& Rock, const FMeshBounds& Bounds);
		/** Bas et haut de cette boite (monde) */
		void RockHeight(const FRock& Rock, const FMeshBounds& Bounds, float& OutZ0, float& OutZ1);
		/** Le rocher entre dans la zone libre (Clearance::ExitKeepOut : pied, colonne du grimpeur, place devant) d'une de ces
		 *  sorties */
		bool InExitKeepOut(const Clearance::FRect& Rock, const FFrame* Exits, int NumExits);
	}
}
