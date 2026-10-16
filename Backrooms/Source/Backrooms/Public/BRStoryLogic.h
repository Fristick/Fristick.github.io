// v4.13 : fil narratif de la campagne, sans moteur : ordre des niveaux, etat narratif tenu par l'hote (etapes accomplies,
// evenements vus, point de reprise), regle d'acces des transitions et migration des anciennes parties. Le jeu (ABRWorld,
// ABRPlayerController, BRSaves) et le banc Tools/Story/test_story.cpp utilisent ces fonctions.
//
// Le fil principal est le meme dans toutes les versions : Level 0 -> 1 -> Poolrooms (37) -> 2 -> 3 -> 4 -> 5 -> 6 -> 8
// -> 9 -> 10 -> 11 -> dernier quai. La disponibilite des niveaux (BRContent) ne change pas cet ordre : elle dit seulement
// ou s'arrete le chapitre publie. Les Poolrooms sont une etape obligatoire : rien au-dela ne s'ouvre tant que leur sortie
// n'a pas ete prise.
//
// v4.15 : parcours lineaire (Flag::Linear, choisi a la NOUVELLE PARTIE) : meme fil, une graine fixe par niveau tiree de la
// graine du parcours (LevelSeedFor), une seule sortie par niveau (celle de la mission : ChunkExitAllowed), une mort en solo
// relance le niveau en cours depuis zero (DeathRuleFor). Les anciennes parties, REJOUER et le mode developpeur restent
// au parcours classique.
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

	/** v4.15 : nature du parcours de la partie (FState::Flags) */
	namespace Flag
	{
		/** Parcours lineaire : graine fixe par niveau, une seule sortie par niveau, mort solo = niveau recommence */
		constexpr uint32_t Linear = 1u << 0;
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
		/** v4.15 : parcours de la partie (bits Flag::) ; 0 : classique (anciennes parties, migration) */
		uint32_t Flags = 0;
		/** v4.15 : graine du parcours lineaire, tiree a la nouvelle partie (0 : aucune) : graine de chaque niveau */
		uint32_t RunSeed = 0;
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
	/** Le groupe quitte From par sa sortie de progression vers To : etape accomplie, plus loin atteint, reprise.
	 *  v4.14 : le niveau gare (Parked) n'est plus efface ici : il reste jusqu'a la reprise reussie de sa session
	 *  (OnParkedRestored), meme si le niveau gare n'est pas disponible dans cette version */
	void OnProgress(FState& S, int From, int To);
	/** v4.14 : la session garee d'une ancienne partie vient d'etre reprise (ou il n'y avait rien a reprendre) : l'etape
	 *  de migration est terminee */
	void OnParkedRestored(FState& S);
	/** v4.14 : faut-il reprendre la session garee en arrivant dans Level (Poolrooms traversees, niveau gare) */
	bool WantsParked(const FState& S, int Level);
	/** Arrivee dans un niveau du fil deja atteint (retour, reprise) : point de reprise */
	void OnArrive(FState& S, int Level);
	/** Fin du contenu disponible atteinte en quittant Level (lot Lot) : etape accomplie, fin de chapitre ; l'histoire
	 *  reprendra au niveau suivant du fil quand il sera publie */
	void OnChapterEnd(FState& S, int Level, int Lot);
	/** Point de « Continuer » : la progression garee d'une ancienne partie si les Poolrooms sont traversees et son niveau
	 *  disponible (v4.14) ; sinon la reprise si elle est disponible (et permise) ; sinon le dernier niveau atteint
	 *  disponible en remontant le fil ; sinon le Level 0 */
	int ResumePoint(const FState& S, FAvailable Available, void* Ctx);

	/** v4.14 : nature de la session, du point de vue de la sauvegarde (meme ordre que EBRSessionMode du jeu) */
	enum class ESession : uint8_t
	{
		/** Mode developpeur, menu, invite : aucune partie a ecrire */
		Free = 0,
		/** Histoire (nouvelle partie, CONTINUER) : la seule session qui ecrit la partie de campagne */
		Story = 1,
		/** REJOUER un niveau decouvert : etat temporaire */
		Replay = 2
	};
	/** v4.14 : ce qu'une session peut ecrire dans la partie de campagne. REJOUER et le mode developpeur jouent sur un etat
	 *  temporaire (le personnage et le monde de la session) : equipement, morts, session a reprendre, niveau courant,
	 *  campagne, histoire et temps de jeu de la partie ne changent pas. Seul transfert explicite : en rejeu, les documents
	 *  lus sont ajoutes au journal de la partie (union, jamais de retrait) */
	struct FWritePolicy
	{
		/** Equipement, sante, batterie, statistiques du personnage */
		bool bCharacter = false;
		/** Morts, mort en cours (reprise apres une fermeture pendant la mort) */
		bool bDeath = false;
		/** Session a reprendre, niveau courant, niveaux explores enregistres par l'ecriture */
		bool bSession = false;
		/** Fragments de route, objectifs facultatifs, fins vues */
		bool bCampaign = false;
		/** Etat narratif */
		bool bStory = false;
		bool bPlayTime = false;
		/** Journal remplace par celui du monde (histoire) */
		bool bJournal = false;
		/** Journal complete par union (rejeu) */
		bool bJournalUnion = false;
	};
	FWritePolicy WritePolicyFor(ESession Mode);

	/** v4.15 : la partie suit le parcours lineaire */
	bool IsLinear(const FState& S);
	/** v4.15 : graine fixe d'un niveau du parcours lineaire (melange de RunSeed et du niveau, jamais 0). 0 (pas de graine
	 *  imposee) : partie classique, RunSeed nul, Level 0 (il garde IntroSeed) ou niveau hors du fil */
	uint32_t LevelSeedFor(const FState& S, int Level);

	/** v4.15 : ce que devient une mort */
	enum class EDeathRule : uint8_t
	{
		/** Retour au Level 0 (histoire classique en solo, mode libre) */
		BackToStart,
		/** Le niveau en cours recommence de zero (REJOUER ; parcours lineaire en solo : meme graine, mission neuve,
		 *  equipement de depart) */
		RestartLevel,
		/** Reveil au point de depart du niveau en cours, apres la fenetre de reanimation (partie en reseau) */
		RespawnInPlace
	};
	/** v4.15 : regle de la mort : reseau -> RespawnInPlace (quel que soit le parcours) ; REJOUER, ou histoire lineaire en
	 *  solo -> RestartLevel ; sinon BackToStart. bLinear ne compte que pour une session d'histoire */
	EDeathRule DeathRuleFor(ESession Mode, bool bLinear, bool bNet);

	/** v4.15 : une sortie tiree par un chunk est-elle posee ? Classique : toujours. Lineaire : jamais quand la mission du
	 *  niveau a sa sortie garantie vers l'avant ; sans elle (securite), seules les sorties gardees par la mission (sortie de
	 *  progression) restent, les sorties de retour disparaissent */
	bool ChunkExitAllowed(bool bLinear, bool bForwardGuaranteed, bool bTargetGuarded);

	/** Ancienne partie (formats 1 a 4) : ce qu'elle sait de sa progression */
	struct FLegacy
	{
		/** Niveaux explores, dans l'ordre de PREMIERE decouverte (AddUnique) : ce n'est pas un journal des trajets */
		const int* Explored = nullptr;
		int NumExplored = 0;
		/** Niveau ou la partie s'est arretee */
		int Current = 0;
		/** Fins vues (BRMission : bits 1 et 2 ; v4.12 : fin du lot n = bit 1 << (1 + n)) */
		uint8_t Endings = 0;
		/** Format du fichier (UBRSaveGame::LoadedVersion ; 0 : inconnu). Format 1 (v4.1 a v4.6) : la console des v4.1 a
		 *  v4.3 ajoutait les niveaux visites aux decouvertes, l'ordre ne prouve donc rien */
		int Format = 0;
	};
	/** v4.14 : ce que l'ancienne partie permet de conclure sur la traversee des Poolrooms */
	enum class EPoolProof : uint8_t
	{
		/** Jamais visitees (ni decouvertes, ni niveau courant) : pas de traversee */
		Absent,
		/** Traversee certaine : le Niveau 4 (destination de leur echelle avant la v4.13) a ete decouvert apres les
		 *  Poolrooms alors qu'aucun autre acces au Niveau 4 n'etait encore connu (ni 1, ni 3, ni 5, ni le Niveau 11 et ses
		 *  destinations au hasard), dans un format sans ajout par la console */
		Proven,
		/** Visitees, mais l'historique ne permet pas de conclure (retours, revisites, Niveau 4 atteignable autrement,
		 *  destination deja decouverte) : aucune preuve n'est inventee */
		Ambiguous
	};
	EPoolProof PoolroomsProof(const FLegacy& L);
	struct FMigration
	{
		FState State;
		/** La session sauvegardee (niveau Current) est garee : elle reprendra apres la traversee des Poolrooms */
		bool bParkSession = false;
		/** Traversee des Poolrooms creditee (PoolProof == Proven seulement) */
		bool bPoolroomsCredited = false;
		EPoolProof PoolProof = EPoolProof::Absent;
	};
	/** Migration d'une ancienne partie : etapes depassees creditees, point de reprise coherent. Poolrooms : creditees
	 *  seulement sur preuve ; absentes ou ambigues avec une partie allee au-dela, la traversee est organisee (la
	 *  progression ancienne est garee intacte, pas perdue ni effacee). v4.15 : toujours le parcours classique (Flags 0,
	 *  RunSeed 0) */
	FMigration MigrateLegacy(const FLegacy& L);
}
