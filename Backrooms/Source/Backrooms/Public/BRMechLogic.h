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
		/** Vanne B : a cote de la vanne A, sur la meme face (repere de la vanne A) */
		constexpr float WheelLateralB = 58.f;
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
	// dans le volume bloquant d'un autre (cage, porte fermee, module), et la place devant lui reste libre
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
		 *  face reste d'un seul tenant pour ses voisins (Level 6 : cage a +-58, poste a -+58) */
		int PickSlot(const FFootprint& Print, const FSlot* Slots, int NumSlots, const FPlaced* Placed, int NumPlaced, float FaceHalf);
	}
}
