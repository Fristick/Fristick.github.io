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
	/** Point vise par les tests de vue (rassemblement et demande) */
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
}
