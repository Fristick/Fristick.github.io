// v4.12 : effets physiques des missions, sans moteur : eau locale (bassins et sas des Poolrooms) et passerelle du
// Niveau 8. Le jeu (ABRWorld, ABRMissionDevice, ABRCharacter, ABREntity) et le banc Tools/Mechanisms/test_mechanisms.cpp
// utilisent ces fonctions et ces cotes : la surface rendue, la profondeur, la nage, l'immersion de la camera, la ligne
// d'eau, les collisions et la validation de sortie suivent un seul etat, celui de la mission (replique, sauvegarde).
//
// Reperes : un module est construit dans le repere d'un mecanisme (FFrame : origine au sol, X vers la salle, Y lateral,
// Z vers le haut, lacet multiple de 90 degres). Les hauteurs absolues sont en cm, sol des salles a 0.
#pragma once

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
}
