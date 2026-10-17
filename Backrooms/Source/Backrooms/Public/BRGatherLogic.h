// v4.12 : rassemblement d'un depart de groupe autour de la sortie choisie, et attente au sommet d'une echelle. C++ pur,
// sans moteur : le jeu (ABRWorld, ABRCharacter) et le banc Tools/Departure/test_departure.cpp utilisent ces fonctions.
//
// Avant la v4.12, le rassemblement etait un point unique (150 cm au-dessus du pied d'une echelle) et une tolerance
// verticale de 260 cm : le grimpeur arrive au sommet d'une echelle a conduit a 430-600 cm (plafond + moitie du conduit)
// et n'etait jamais compte ; sa demande repartait a chaque image. Desormais :
//   - une sortie a une forme : un pied (au sol), une orientation, et pour une echelle son conduit (accroche, sommet) ;
//   - un joueur est rassemble s'il est SUR cette echelle (dans la colonne de son conduit, a n'importe quelle hauteur de
//     la montee : pas de ligne de vue a tester, le conduit est ferme), ou au MEME ETAGE que le pied de la sortie, a moins
//     de 8 m du point de rassemblement, avec une ligne de vue vers ce point (testee par l'appelant) ;
//   - la tolerance verticale reste celle d'un etage (260 cm autour du centre d'un joueur debout au pied) : un joueur
//     separe par un plafond n'est jamais compte, la regle n'est pas elargie ;
//   - les tests de vue de la demande et du rassemblement visent le meme point (point de rassemblement + 90 cm).
// v4.15 : une porte (ascenseur, porte d'hotel, porte de la mission) est posee sur la face meme du mur : le point vise etait
// exactement sur la surface de la boite de collision du mur, et l'arrondi decidait si la vue etait coupee (depart refuse
// devant la porte). Le point vise par la vue est desormais 50 cm devant le pied, dans la piece ; le point de
// rassemblement (zone de 8 m, position repliquee) et la portee de la porte ne changent pas.
#pragma once

#include <cstdint>

namespace BRGather
{
	struct FVec
	{
		float X = 0.f;
		float Y = 0.f;
		float Z = 0.f;
	};

	/** Rayon (cm, horizontal) autour du point de rassemblement */
	constexpr float Radius = 800.f;
	/** Ecart vertical toleree (cm) entre le centre d'un joueur et celui d'un joueur debout au pied de la sortie */
	constexpr float Height = 260.f;
	/** Demi-hauteur de la capsule d'un joueur debout */
	constexpr float CapsuleHalf = 88.f;
	/** Rayon (cm) de la colonne d'une echelle autour de l'accroche du grimpeur (le conduit fait 124 x 114 cm) */
	constexpr float LadderColumn = 70.f;
	/** Hauteur de la vue testee au-dessus du point de rassemblement */
	constexpr float ViewHeight = 90.f;
	/** Portee d'une porte pour la demander (3D, depuis le centre du joueur) */
	constexpr float DoorReach = 360.f;
	/** v4.15 : recul (cm, vers la piece) du point vise par la vue devant une porte posee sur la face d'un mur */
	constexpr float DoorViewInset = 50.f;

	enum class EStyle : uint8_t
	{
		Door,
		Ladder,
		Barn
	};

	/** Forme d'une sortie vue par le depart de groupe */
	struct FExitShape
	{
		EStyle Style = EStyle::Door;
		/** Pied de la sortie (au sol) */
		FVec Foot;
		/** Direction (horizontale, unitaire) vers la piece */
		FVec Forward = { 1.f, 0.f, 0.f };
		/** Echelle : point ou se tient le grimpeur (XY) et hauteur (centre de capsule, monde) du sommet de la montee */
		FVec Anchor;
		float TopZ = 0.f;
	};

	/** Point de rassemblement : dans la piece devant une echelle (1 m), a l'entree d'une grange (5,2 m), a la porte */
	FVec GatherPoint(const FExitShape& Shape);
	/** Point vise par les tests de vue (rassemblement et demande) : point de rassemblement + 90 cm ; v4.15 : porte, 50 cm
	 *  devant le pied (le pied est sur la face du mur) */
	FVec ViewPoint(const FExitShape& Shape);
	/** Sur l'echelle : dans la colonne de son conduit, entre le pied et un peu au-dessus du sommet de la montee */
	bool OnLadder(const FExitShape& Shape, const FVec& Pos);
	/** Au meme etage que le pied, a moins de Radius du point de rassemblement (ligne de vue a tester par l'appelant) */
	bool InFloorZone(const FExitShape& Shape, const FVec& Pos);

	enum class EGather : uint8_t
	{
		/** Pas rassemble */
		No,
		/** Au meme etage : rassemble si la ligne de vue vers ViewPoint est libre */
		FloorNeedsView,
		/** Sur l'echelle : rassemble */
		Ladder
	};
	EGather Classify(const FExitShape& Shape, const FVec& Pos);

	/** Peut demander le depart par cette sortie : sur l'echelle ; a portee de la porte ; devant la grange. La ligne de vue
	 *  (vers ViewPoint) reste a tester sauf sur l'echelle */
	EGather CanRequest(const FExitShape& Shape, const FVec& Pos);

	/** Attente au sommet d'une echelle (cote grimpeur) : une demande par montee, un etat stable, l'annulation en
	 *  descendant. Aucune demande n'est renvoyee a chaque image */
	struct FClimbWait
	{
		enum class EState : uint8_t
		{
			/** En montee (ou redescendu : prochaine arrivee au sommet = nouvelle demande) */
			Climbing,
			/** Demande envoyee, en attente du groupe */
			Waiting,
			/** Depart refuse, annule ou expire : on reste au sommet sans redemander ; redescendre d'un metre rearme */
			Stopped,
			/** Le groupe part */
			Leaving
		};
		EState State = EState::Climbing;
		float SinceRequest = 0.f;
		/** Numero du depart vu pendant cette attente (0 : aucun) */
		uint16_t SeenDeparture = 0;
		bool bCancelSent = false;

		struct FOut
		{
			/** Envoyer la demande de depart (une fois par arrivee au sommet) */
			bool bSendRequest = false;
			/** Envoyer l'annulation (l'initiateur redescend pendant le rassemblement) */
			bool bSendCancel = false;
			/** Ne pas monter plus haut que le sommet de la montee */
			bool bClampTop = false;
		};
		/** Phase : celle du depart replique (0 aucun, 1 rassemblement, 2 depart, 3 annule) ; Id : son numero ;
		 *  bForThisExit : ce depart vise la destination de cette echelle (sinon il est ignore) */
		FOut Update(float Dt, float Z, float TopZ, float UpInput, uint8_t Phase, uint16_t DepartureId, bool bForThisExit, bool bInitiator);
		/** L'hote a refuse la demande : on reste au sommet, sans redemander */
		void OnRefused();
		/** Quitte l'echelle : tout est oublie */
		void Reset();
	};
	/** Descente qui rearme une demande apres un arret (cm sous le sommet) */
	constexpr float RearmDrop = 100.f;
	/** Sans depart vu apres une demande, l'attente s'arrete (refus perdu, hote ancien) */
	constexpr float RequestTimeout = 3.f;

	// v4.16 (echelles) : suite du deplacement balaye d'une image de montee (ABRCharacter::UpdateClimb : traction vers l'accroche
	// et montee ou descente) quand une surface l'arrete, et lacher de l'echelle en descente. Mouvement du pion local seulement
	// (rien sur le reseau). Le banc Tools/Departure/test_departure.cpp (section 6) rejoue ces regles dans un modele balaye.
	//   - Sol praticable touche : glisser le long de lui (v4.15 correctifs 3, tablier incline du Niveau 8).
	//   - En montant, paroi non praticable, pieds encore sous le pied de l'echelle plus une marche (MaxStepHeight) : le reste de
	//     la montee a la verticale (v4.15 correctifs 5 : echelle sur un trottoir du Niveau 37 prise depuis le canal) SEULEMENT si
	//     l'essai de la marche est libre : capsule (reduite de ClimbProbeMargin) levee a la verticale jusqu'a des pieds a
	//     ClimbStepProbeFeet, puis menee a cette hauteur jusqu'a l'aplomb de l'accroche, sans rien toucher. v4.16 : avant (v4.15
	//     correctifs 5), sans cet essai, un mur haut, une caisse plus haute qu'une marche, un coequipier ou un plafond bas entre le
	//     joueur et l'accroche le levaient quand meme jusqu'a une marche au-dessus du pied (37 cm sur un sol plat ; 105 cm depuis
	//     le canal sous une echelle posee sur un trottoir), colle a la paroi, loin de l'echelle, sans pouvoir passer.
	//   - En descendant, paroi : le reste de la descente a la verticale (v4.15 correctifs 5).
	//   - v4.16 : en descendant, pieds a une marche au plus au-dessus du pied qui ne descendent plus (moins de ClimbStuckRatio de
	//     la descente de l'image : sol, marche ou bord sous le grimpeur) : il lache l'echelle (ClimbReleaseOnDescent) et se tient
	//     sur ce qu'il touche. Avant : il restait accroche au-dessus (16 cm sur la marche immergee d'un trottoir lateral du Niveau
	//     37 ; le dessus d'une caisse sous l'accroche), seuls Saut ou Interagir le liberaient.
	enum class EClimbHit : uint8_t
	{
		/** Rien de plus (pas d'impact, paroi en montee hors de la regle ou essai de la marche pris) */
		Stop,
		/** Glisser le long du sol praticable touche */
		Slide,
		/** Reste de la montee a la verticale (balaye) */
		Rise,
		/** Reste de la descente a la verticale (balaye) */
		Lower
	};
	struct FClimbHitIn
	{
		/** Le deplacement balaye de l'image a ete arrete */
		bool bBlocked = false;
		/** La surface touchee est un sol praticable (Normal.Z >= sol praticable du mouvement) */
		bool bWalkable = false;
		/** Part verticale du deplacement de l'image (cm ; > 0 en montant) */
		float DZ = 0.f;
		/** Pieds du grimpeur apres le deplacement balaye, et pied de l'echelle (cm, monde) */
		float FeetZ = 0.f;
		float FootZ = 0.f;
		/** Hauteur d'une marche (UCharacterMovementComponent::MaxStepHeight) */
		float StepHeight = 0.f;
		/** Essai de la marche libre (rempli par l'appelant quand ClimbNeedsStepProbe) */
		bool bStepClear = false;
	};
	/** L'essai de la marche decide de la suite (deux balayages chez l'appelant, seulement dans ce cas) */
	bool ClimbNeedsStepProbe(const FClimbHitIn& In);
	EClimbHit ClimbAfterHit(const FClimbHitIn& In);
	/** Pieds de la capsule de l'essai de la marche : une marche au-dessus du pied de l'echelle, plus ClimbProbeMargin */
	float ClimbStepProbeFeet(float FootZ, float StepHeight);
	/** Descente (DZ < 0) presque arretee (Descended, cm descendus pendant l'image, < -DZ * ClimbStuckRatio) les pieds a une marche
	 *  au plus (plus ClimbProbeMargin) au-dessus du pied : lacher l'echelle */
	bool ClimbReleaseOnDescent(float DZ, float Descended, float FeetZ, float FootZ, float StepHeight);
	/** Marge de l'essai de la marche (cm) : rayon et demi-hauteur de sa capsule reduits d'autant (un contact a 0,1 cm laisse par
	 *  le deplacement balaye ne compte pas), pieds de l'essai d'autant au-dessus d'une marche */
	constexpr float ClimbProbeMargin = 1.f;
	/** Part de la descente d'une image en dessous de laquelle la descente est arretee */
	constexpr float ClimbStuckRatio = 0.25f;
}
