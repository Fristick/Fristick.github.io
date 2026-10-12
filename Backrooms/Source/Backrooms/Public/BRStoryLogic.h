// v4.13 : fil narratif de la campagne, sans moteur : ordre des niveaux, etat narratif tenu par l'hote (etapes accomplies,
// evenements vus, point de reprise), regle d'acces des transitions et migration des anciennes parties. Le jeu (ABRWorld,
// ABRPlayerController, BRSaves) et le banc Tools/Story/test_story.cpp utilisent ces fonctions.
//
// Le fil principal est le meme dans toutes les versions : Level 0 -> 1 -> Poolrooms (37) -> 2 -> 3 -> 4 -> 5 -> 6 -> 8
// -> 9 -> 10 -> 11 -> dernier quai. La disponibilite des niveaux (BRContent) ne change pas cet ordre : elle dit seulement
// ou s'arrete le chapitre publie. Les Poolrooms sont une etape obligatoire : rien au-dela ne s'ouvre tant que leur sortie
// n'a pas ete prise.
#pragma once

#include <cstdint>

namespace BRStory
{
	constexpr int NumSteps = 12;
	/** Fil principal (identifiants de niveaux, ceux du projet) */
	extern const int Order[NumSteps];
	/** Le dernier quai (fin de la campagne), comme BRMission::EndingTarget */
	constexpr int Ending = 999;
	constexpr int Poolrooms = 37;

	/** Position dans le fil (-1 : hors du fil) */
	int IndexOf(int Level);
	/** Niveau suivant (Ending apres le Niveau 11 ; -1 hors du fil) */
	int NextOf(int Level);
	/** Niveau precedent (-1 pour le Level 0 et hors du fil) */
	int PrevOf(int Level);

	/** Evenements narratifs vus (une fois par partie) */
	namespace Event
	{
		/** Reveil du Level 0 */
		constexpr uint32_t IntroWake = 1u << 0;
		/** Porte de service du Level 0 ouverte */
		constexpr uint32_t DoorOpened = 1u << 1;
		/** Aide d'orientation donnee apres une longue errance au Level 0 */
		constexpr uint32_t DraftHint = 1u << 2;
		/** Inscription de L. au-dessus du sas des Poolrooms */
		constexpr uint32_t PoolInscription = 1u << 3;
		/** Marque de L. sur la porte du passage vers la station (fin du chapitre 1) */
		constexpr uint32_t StationSign = 1u << 4;
		/** Ancienne partie : traversee des Poolrooms organisee a la migration */
		constexpr uint32_t MigratedDetour = 1u << 5;
		/** Premiere preuve au Level 1 : la liaison de service remise sous tension */
		constexpr uint32_t ServiceLink = 1u << 6;
	}

	/** Etat narratif d'une partie (tenu par l'hote, replique, sauvegarde) */
	struct FState
	{
		/** Plus loin atteint dans le fil (index dans Order) */
		int Reached = 0;
		/** Etapes accomplies (bit = index dans Order) : niveau quitte par sa sortie de progression (Level 0 : porte de
		 *  service). Une etape accomplie le reste : au retour, sa mission est deja resolue */
		uint32_t Done = 0;
		uint32_t Events = 0;
		/** Fins de chapitre atteintes (bit 1 << lot) */
		uint32_t ChapterEnds = 0;
		/** Dernier point de reprise sur (niveau) */
		int Resume = 0;
		/** Ancienne partie : niveau ou reprendre ses progres apres l'etape manquante des Poolrooms (-1 : aucun) */
		int Parked = -1;
		/** Level 0 de la partie : graine tiree a la nouvelle partie (0 : pas encore tiree). Mort, retour, reprise,
		 *  reconnexion : le meme Level 0, la porte au meme endroit */
		uint32_t IntroSeed = 0;
		/** Porte de service retenue pour IntroSeed : cellule, face vers l'exterieur (0 +X, 1 -X, 2 +Y, 3 -Y ; -1 : aucune),
		 *  palier du choix (0 bande visee, 1 bande elargie, 2 secours) et version de generation (DoorPlaceVersion) */
		int DoorX = 0;
		int DoorY = 0;
		int DoorDir = -1;
		int DoorTier = 0;
		int DoorVersion = 0;
	};

	/** Version du placement de la porte de service (bande visee, bande elargie, secours). Une porte sauvegardee reste
	 *  a sa place tant qu'elle est encore un emplacement valide, meme si cette version change */
	constexpr int DoorPlaceVersion = 2;

	bool IsDone(const FState& S, int Level);
	void SetDone(FState& S, int Level);

	enum class EAccess : uint8_t
	{
		Allowed,
		/** Ni atteint, ni prepare par l'etape precedente */
		NotReached,
		/** Au-dela des Poolrooms, avant leur traversee */
		PoolroomsRequired,
		/** Hors du fil */
		Unknown
	};
	/** L'hote laisse-t-il entrer dans To : niveau deja atteint, ou etape precedente accomplie ; jamais au-dela des
	 *  Poolrooms tant qu'elles ne sont pas traversees */
	EAccess CanEnter(const FState& S, int To);

	/** Disponibilite d'un niveau dans la version (BRContent), fournie par l'appelant */
	using FAvailable = bool (*)(int Level, void* Ctx);

	/** Destination de la sortie de progression de From : NextOf(From) ; apres les Poolrooms d'une ancienne partie garee,
	 *  son niveau gare (s'il est disponible) */
	int ProgressTarget(const FState& S, int From, FAvailable Available, void* Ctx);
	/** Le groupe quitte From par sa sortie de progression vers To : etape accomplie, plus loin atteint, reprise */
	void OnProgress(FState& S, int From, int To);
	/** Arrivee dans un niveau du fil deja atteint (retour, reprise) : point de reprise */
	void OnArrive(FState& S, int Level);
	/** Fin du contenu disponible atteinte en quittant Level (lot Lot) : etape accomplie, fin de chapitre ; l'histoire
	 *  reprendra au niveau suivant du fil quand il sera publie */
	void OnChapterEnd(FState& S, int Level, int Lot);
	/** Point de « Continuer » : la reprise si elle est disponible (et permise) ; sinon le dernier niveau atteint disponible
	 *  en remontant le fil ; sinon le Level 0 */
	int ResumePoint(const FState& S, FAvailable Available, void* Ctx);

	/** Ancienne partie (formats 1 a 4) : ce qu'elle sait de sa progression */
	struct FLegacy
	{
		/** Niveaux explores, dans l'ordre de decouverte */
		const int* Explored = nullptr;
		int NumExplored = 0;
		/** Niveau ou la partie s'est arretee */
		int Current = 0;
		/** Fins vues (BRMission : bits 1 et 2 ; v4.12 : fin du lot n = bit 1 << (1 + n)) */
		uint8_t Endings = 0;
	};
	struct FMigration
	{
		FState State;
		/** La session sauvegardee (niveau Current) est garee : elle reprendra apres la traversee des Poolrooms */
		bool bParkSession = false;
		/** Traversee des Poolrooms prouvee par l'ordre de decouverte (echelle 37 -> 4 des versions anterieures) */
		bool bPoolroomsCredited = false;
	};
	/** Migration d'une ancienne partie : etapes depassees creditees, point de reprise coherent, traversee des Poolrooms
	 *  organisee si la partie est allee au-dela sans les traverser (sa progression est garee, pas perdue) */
	FMigration MigrateLegacy(const FLegacy& L);
}
