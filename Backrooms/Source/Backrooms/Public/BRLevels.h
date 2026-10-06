// Catalogue des niveaux (inspires de https://backrooms-wiki.wikidot.com/normal-levels-i)
#pragma once

#include "CoreMinimal.h"
#include "BRTypes.h"
#include "BRContentLogic.h"

namespace BRLevels
{
	/** v4.12 : niveaux DISPONIBLES dans cette version (canal de contenu), dans l'ordre du menu. Menus, tests, tirages au
	 *  hasard et transitions ne voient que ceux-la */
	const TArray<FBRLevelDef>& All();

	/** v4.12 : tous les niveaux du projet (disponibles ou non), dans l'ordre du menu */
	const TArray<FBRLevelDef>& Defined();

	/** Niveau par numero, disponible ou non (retourne le Niveau 0 s'il n'est pas defini) */
	const FBRLevelDef& Get(int32 Number);

	/** v4.12 : disponible dans cette version (publie, ou en test interne selon le canal) */
	bool Exists(int32 Number);
	/** v4.12 : defini dans le projet (disponible ou non) */
	bool IsDefined(int32 Number);

	/** Index dans All() (0 si introuvable) */
	int32 IndexOf(int32 Number);

	/** v4.12 : canal de contenu de cette version. Version publiee : fixe a la compilation (BR_CONTENT_CHANNEL,
	 *  Backrooms.Build.cs) ; developpement : -BRContent=public|internal|all (tout par defaut) */
	BRContent::EChannel Channel();
	/** v4.12 (tests, hors version publiee) : impose un canal (-1 : celui de la version) */
	void SetChannelOverride(int32 InChannel);
	/** v4.12 : ce que donne une sortie de From vers Target dans cette version (destination, fin du contenu disponible,
	 *  passage condamne, fin de la campagne) : la meme regle pour l'invite, l'usage, le depart et l'hote */
	BRContent::FExitResolution ResolveExit(int32 From, int32 Target);
	/** v4.12 : sortie de progression que la mission garde dans cette version (voir BRContent::AdaptedForward) */
	int32 AdaptedForward(int32 From, int32 BaseForward);
	/** v4.12 : signature du contenu (compatibilite reseau) */
	uint32 ContentSignature();
	/** v4.12 : options ajoutees a l'adresse d'une partie a rejoindre : protocole et signature du contenu */
	FString JoinOptions();
	/** v4.12 (hote, PreLogin) : vide si le joueur est compatible, sinon le refus
	 *  "BR_INCOMPATIBLE:version|content:<protocole>:<canal>:<lot>" (version et contenu de l'hote) */
	FString CheckJoinOptions(const FString& Options);
	/** Prefixe d'un refus de connexion pour incompatibilite */
	constexpr const TCHAR* JoinRefusalPrefix = TEXT("BR_INCOMPATIBLE:");
	/** v4.12 (client) : lit un refus de l'hote ; false si l'erreur n'en est pas un */
	bool ParseJoinRefusal(const FString& Error, bool& bOutVersion, int32& OutHostNet, int32& OutHostChannel, int32& OutHostLot);
	/** v4.12 : "4.12" pour le protocole 412 */
	FString VersionLabel(int32 NetVersion);
	/** v4.12 : nom d'un canal ("version publiee", "version de test interne", "version de developpement") */
	FText ChannelName(BRContent::EChannel InChannel);
	/** v4.12 : niveau de reprise d'une partie : son dernier niveau s'il est explore et disponible ; s'il n'est pas disponible
	 *  dans cette version (partie d'une version de test), le dernier niveau disponible explore, sinon le Niveau 0.
	 *  bOutMoved : le dernier niveau de la partie n'est pas disponible ici (la partie garde ses niveaux et sa progression) */
	int32 ResumeLevel(int32 Current, const TArray<int32>& Explored, bool* bOutMoved = nullptr);
	/** v4.12 : niveaux explores disponibles dans cette version */
	int32 CountAvailable(const TArray<int32>& Explored);
	/** v4.12 : nom d'un lot de contenu (fin du contenu disponible, menus) */
	FText LotName(int32 Lot);

	/** Notes generiques trouvables partout */
	const TArray<FBRNote>& CommonNotes();
	/** v4.8 : note d'apres son identifiant (tous niveaux) ; nullptr si inconnue */
	const FBRNote* FindNote(FName Id);
	/** v4.8 : identifiant d'une note d'apres son texte francais (sauvegardes v4.1-v4.7, qui gardaient le texte) */
	FName NoteIdFromLegacyText(const FString& FrenchText);
	/** v4.8 : texte d'une entree du journal : note connue (dans la langue courante), sinon le texte enregistre tel quel */
	FText NoteText(const FString& JournalEntry);
}
