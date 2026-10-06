// v4.12 : disponibilite des niveaux par version (publication progressive), sans dependance au moteur.
//
// Tous les niveaux restent dans le projet ; chacun a une etape :
//   - Published : publie, jouable par tous ;
//   - Internal  : en test interne (version de test, ou version de developpement) ;
//   - Developed : developpe, en attente de sa mise a jour de contenu (version de developpement seulement).
// Une version a un canal : Public (version publiee : niveaux publies seulement), Internal (publies + test interne), All
// (tout, pour developper et tester). Le canal d'une version publiee est fixe a la compilation (Backrooms.Build.cs) : ni
// la ligne de commande ni un fichier de configuration ne l'ouvrent.
//
// Toutes les entrees passent par ces regles : menus, portes, echelles, noclip, progression de la campagne, destinations
// au hasard, demandes reseau (validees par l'hote). Une sortie vers un niveau indisponible :
//   - est redirigee si une redirection est prevue pour ce lot (lot 1 : l'echelle des Poolrooms remonte au Niveau 1,
//     l'ascenseur du Niveau 1 descend au Niveau 2) ;
//   - devient la FIN DU CONTENU DISPONIBLE si c'est la sortie de progression du niveau et qu'aucune autre ne l'est ;
//   - sinon, elle est condamnee (visible, annoncee "pas encore accessible"), et la mission du niveau garde une autre
//     sortie : aucune mission ne demande d'atteindre un niveau non publie.
// Les identifiants de niveaux sont stables (numeros du wiki ; 1001 et 1002 reserves aux propositions Level ! et Level
// Fun) ; une sauvegarde garde ses niveaux, decouvertes et fins quelle que soit la version.
#pragma once

#include <cstdint>

namespace BRContent
{
	enum class EStage : uint8_t
	{
		Developed,
		Internal,
		Published
	};

	enum class EChannel : uint8_t
	{
		Public,
		Internal,
		All
	};

	struct FLevelEntry
	{
		int Level = 0;
		EStage Stage = EStage::Developed;
		/** Lot de contenu (1 : lancement) */
		int Lot = 0;
	};

	/** Les niveaux du projet, avec leur etape et leur lot (configuration centrale) */
	const FLevelEntry* Levels(int& OutCount);
	/** Lot de lancement et nom de code de la version */
	constexpr int LaunchLot = 1;
	/** Destination speciale : fin du contenu disponible (positive, distincte de BRMission::EndingTarget = 999) */
	constexpr int ChapterEndTarget = 998;
	/** Version du protocole reseau (une partie ne melange pas deux versions) */
	constexpr int NetVersion = 412;

	bool IsKnown(int Level);
	EStage StageOf(int Level);
	int LotOf(int Level);
	bool IsAvailable(int Level, EChannel Channel);
	/** Dernier lot entierement disponible dans ce canal (celui dont la fin est annoncee) */
	int CurrentLot(EChannel Channel);

	enum class EExit : uint8_t
	{
		/** Destination disponible (Target, eventuellement redirigee) */
		Go,
		/** Fin du contenu disponible (sortie de progression vers un niveau pas encore publie) */
		ChapterEnd,
		/** Passage condamne : visible, annonce, ne mene nulle part pour l'instant */
		Sealed,
		/** Fin de la campagne (Niveau 11 -> dernier quai) */
		Ending
	};
	struct FExitResolution
	{
		EExit Kind = EExit::Sealed;
		/** Go : niveau d'arrivee (-1 : tire au hasard parmi les niveaux disponibles) */
		int Target = -1;
		/** Go : la destination a ete redirigee pour ce canal */
		bool bRedirected = false;
	};
	/** Sorties d'un niveau telles que le jeu les definit (BRLevels.cpp) : cibles, et "gardee par la mission" */
	struct FExitList
	{
		static constexpr int Max = 8;
		int Targets[Max] = {};
		bool Guarded[Max] = {};
		int Num = 0;
		void Add(int Target, bool bGuarded)
		{
			if (Num < Max)
			{
				Targets[Num] = Target;
				Guarded[Num] = bGuarded;
				++Num;
			}
		}
	};
	FExitResolution ResolveExit(int From, int Target, bool bGuarded, const FExitList& FromExits, EChannel Channel);
	/** Sortie de progression a garder par la mission (porte, passerelle) : celle de la base si elle mene quelque part
	 *  (niveau disponible, redirection ou fin du contenu), sinon une autre sortie gardee disponible ; -1 sinon */
	int AdaptedForward(int From, int BaseForward, const FExitList& FromExits, EChannel Channel);
	/** Niveaux tires au hasard (porte d'immeuble du Niveau 11, poursuite apres une fin) : disponibles, sauf Exclude */
	int RandomChoices(EChannel Channel, int Exclude, int* Out, int Max);
	/** Signature du contenu d'une version (niveaux disponibles, lot, protocole) : deux machines ne jouent ensemble que si
	 *  elles ont la meme */
	uint32_t Signature(EChannel Channel);

	/** Niveau de reprise d'une partie : son dernier niveau s'il est disponible (et explore) ; sinon (partie d'une version
	 *  de test) le dernier niveau disponible de la liste d'exploration (ordre de decouverte), sinon le Niveau 0.
	 *  bOutMoved : le dernier niveau n'est pas disponible dans ce canal. La partie elle-meme n'est pas modifiee */
	int ResumeLevel(int Current, const int* Explored, int NumExplored, EChannel Channel, bool* bOutMoved);

	/** Compatibilite d'un joueur qui rejoint, validee par l'hote : protocole, puis contenu */
	enum class EJoin : uint8_t
	{
		Ok,
		/** Autre version du jeu (ou client sans controle : version anterieure a la v4.12) */
		Version,
		/** Meme version, autre contenu (version publiee contre version de test, autre lot) */
		Content
	};
	/** ClientNet : -1 si absent ; ClientSignature : valeur envoyee (ignoree si bHasSignature est faux) */
	EJoin CheckJoin(int ClientNet, bool bHasSignature, uint32_t ClientSignature, EChannel HostChannel);
}
