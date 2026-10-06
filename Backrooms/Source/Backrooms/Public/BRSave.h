// Sauvegardes (v4.1) : une partie = un emplacement (Saved/SaveGames/BR_Partie_<n>.sav).
// On y garde les niveaux deja explores (les seuls que l'on peut choisir ensuite), le dernier niveau atteint,
// l'inventaire et l'etat du joueur, le journal (entites rencontrees, notes lues), le temps de jeu.
// v4.7 (format 2) : etat de la session pour une vraie reprise (graine, objectifs, objets ramasses, point de reprise),
// mort en attente (fermer le jeu a terre ne l'annule pas), copie de secours, ecritures ordonnees sur un thread de fond.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BRSave.generated.h"

/** Une case d'inventaire non vide (poches, sac ou equipement) */
USTRUCT()
struct FBRSavedItem
{
	GENERATED_BODY()

	/** EBRSlotGroup */
	UPROPERTY(SaveGame)
	uint8 Group = 0;

	UPROPERTY(SaveGame)
	int32 Index = 0;

	/** EBRItem */
	UPROPERTY(SaveGame)
	uint8 Item = 0;

	UPROPERTY(SaveGame)
	int32 Count = 0;
};

/** v4.7 : ce qu'il faut pour reprendre un niveau tel qu'on l'a laisse (meme disposition, objectifs, objets ramasses) */
USTRUCT()
struct FBRSessionState
{
	GENERATED_BODY()

	/** Une session peut etre reprise (faux : niveau neuf, par exemple apres une mort) */
	UPROPERTY(SaveGame)
	bool bValid = false;

	UPROPERTY(SaveGame)
	int32 Level = 0;

	/** Graine de generation du niveau */
	UPROPERTY(SaveGame)
	uint32 Seed = 0;

	UPROPERTY(SaveGame)
	int32 VHSFound = 0;

	/** v4.11 (format 4) : version de generation de la session. 1 (valeur par defaut, donc celle de toute session d'un
	 *  format anterieur) : ancien mode, garde jusqu'a la sortie du niveau (cassettes VHS du Niveau 0, sorties libres) ;
	 *  2 : missions v4.11 (BRMission::GenVersion). La graine seule ne suffit pas : l'algorithme a change */
	UPROPERTY(SaveGame)
	int32 GenVersion = 1;

	/** v4.11 : etat de la mission (mecanismes, objets d'equipe, verrous leves), lie au plan par son empreinte */
	UPROPERTY(SaveGame)
	TArray<uint8> Mission;

	/** v4.11 : documents facultatifs (cassettes VHS des nouvelles parties) trouves dans ce niveau */
	UPROPERTY(SaveGame)
	int32 LoreFound = 0;

	UPROPERTY(SaveGame)
	bool bBlackoutRecorded = false;

	UPROPERTY(SaveGame)
	bool bEntityRecorded = false;

	/** Objets deja ramasses dans ce niveau (identifiants stables de ABRPickup) : ils ne reapparaissent pas */
	UPROPERTY(SaveGame)
	TArray<uint64> Collected;

	/** Dernier point sur et au sol du joueur (pas au-dessus d'une fosse, pas dans l'eau profonde) */
	UPROPERTY(SaveGame)
	bool bHasSpot = false;

	UPROPERTY(SaveGame)
	FVector Spot = FVector::ZeroVector;

	UPROPERTY(SaveGame)
	float Yaw = 0.f;
};

UCLASS()
class BACKROOMS_API UBRSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Format du fichier (1 : v4.1 a v4.6 ; 2 : v4.7, session reprise ; 3 : v4.8, journal par identifiants de notes ;
	 *  4 : v4.11, missions de niveau et campagne) */
	static constexpr int32 CurrentVersion = 4;

	/** Format du fichier. La valeur par defaut reste 1 : les proprietes egales a celles de l'objet par defaut ne sont
	 *  pas ecrites, et un fichier v4.1-v4.6 (Version = 1, donc absente du fichier) doit etre reconnu comme tel.
	 *  BRSaves::Write et WriteAsync y mettent CurrentVersion avant d'ecrire. */
	UPROPERTY(SaveGame)
	int32 Version = 1;

	/** Nom donne par le joueur a la creation */
	UPROPERTY(SaveGame)
	FString SaveName;

	UPROPERTY(SaveGame)
	FDateTime Created;

	UPROPERTY(SaveGame)
	FDateTime LastPlayed;

	/** Temps de jeu cumule (s), menus exclus */
	UPROPERTY(SaveGame)
	float PlayTime = 0.f;

	/** Dernier niveau atteint (propose par defaut au chargement) */
	UPROPERTY(SaveGame)
	int32 CurrentLevel = 0;

	/** Niveaux deja explores : les seuls que l'on peut choisir en reprenant la partie */
	UPROPERTY(SaveGame)
	TArray<int32> Explored;

	/** L'etat du joueur a ete enregistre au moins une fois (sinon : equipement de depart) */
	UPROPERTY(SaveGame)
	bool bHasPlayer = false;

	UPROPERTY(SaveGame)
	TArray<FBRSavedItem> Items;

	UPROPERTY(SaveGame)
	float Health = 100.f;

	UPROPERTY(SaveGame)
	float Sanity = 100.f;

	UPROPERTY(SaveGame)
	float Battery = 100.f;

	/** Journal : entites rencontrees (EBREntityKind) et notes lues */
	UPROPERTY(SaveGame)
	TArray<int32> Discovered;

	/** Notes lues. v4.8 (format 3) : identifiants stables (Note.L0.3) ; le texte suit la langue du joueur. Formats 1 et
	 *  2 : texte francais de la note, converti a la migration (un texte inconnu reste tel quel) */
	UPROPERTY(SaveGame)
	TArray<FString> Notes;

	UPROPERTY(SaveGame)
	int32 Deaths = 0;

	/** v4.11 : campagne. Fragments de route (bits, Niveau 11), objectifs facultatifs remplis (bits, variante de la fin),
	 *  fins vues (1 principale, 2 variante) */
	UPROPERTY(SaveGame)
	uint8 RouteBits = 0;

	UPROPERTY(SaveGame)
	uint16 OptionalFound = 0;

	UPROPERTY(SaveGame)
	uint8 Endings = 0;

	/** v4.7 : session en cours (reprise fidele apres fermeture du jeu) */
	UPROPERTY(SaveGame)
	FBRSessionState Session;

	/** v4.7 : mort pas encore resolue au moment de l'ecriture (jeu ferme a terre ou pendant le fondu).
	 *  Au chargement : equipement de depart et niveau neuf, comme si la mort etait allee a son terme. */
	UPROPERTY(SaveGame)
	bool bPendingDeath = false;

	/** Niveau ou l'on se reveille apres cette mort (seul : Niveau 0 ; en equipe : le niveau en cours) */
	UPROPERTY(SaveGame)
	int32 PendingDeathLevel = 0;

	/** Charge depuis la copie de secours (le fichier principal etait illisible) ; non enregistre */
	bool bRecovered = false;
	/** Format lu sur le disque avant migration ; non enregistre */
	int32 LoadedVersion = CurrentVersion;
	/** v4.8 : fichier d'un format plus recent que ce jeu : lecture seule (ni reprise, ni reecriture) ; non enregistre */
	bool bFutureFormat = false;

	bool IsExplored(int32 Level) const { return Explored.Contains(Level); }
	void MarkExplored(int32 Level) { Explored.AddUnique(Level); }
};

/** v4.8 : cause d'un echec d'ecriture (le message est compose a l'affichage, dans la langue du joueur) */
enum class EBRSaveError : uint8
{
	/** Le fichier temporaire n'a pas pu etre ecrit (disque plein, dossier protege) */
	TempWrite,
	/** Le fichier n'a pas pu etre remplace (verrouille par un autre programme) */
	Replace,
	/** Partie d'un format plus recent : reecriture refusee, fichier preserve */
	FutureFormat
};

/** v4.8 : ecriture de sauvegarde echouee, gardee jusqu'a son acquittement */
struct FWriteFailure
{
	int32 Slot = INDEX_NONE;
	/** Numero de la demande d'ecriture (croissant) */
	uint32 RequestId = 0;
	EBRSaveError Error = EBRSaveError::TempWrite;
	/** Fichier concerne (nom sans dossier) */
	FString File;
	/** Format du fichier preserve (FutureFormat) */
	int32 Version = 0;
	/** Description technique (journal) */
	FString Reason;
	FDateTime When;
};

namespace BRSaves
{
	/** Nombre d'emplacements de sauvegarde */
	constexpr int32 MaxSlots = 6;

	BACKROOMS_API FString SlotName(int32 Slot);
	/** v4.7 : copie de secours (meme contenu, ecrite juste apres le fichier principal) */
	BACKROOMS_API FString BackupSlotName(int32 Slot);
	/** v4.7 : fichier illisible mis de cote (l'emplacement redevient libre) */
	BACKROOMS_API FString UnreadableSlotName(int32 Slot);
	/** v4.7 : copie intacte d'une sauvegarde d'un ancien format, faite avant sa premiere reecriture */
	BACKROOMS_API FString LegacySlotName(int32 Slot, int32 Version);
	/** nullptr si l'emplacement est vide ou illisible. v4.7 : principal illisible -> copie de secours ; les deux
	 *  illisibles -> le fichier est mis de cote (UnreadableSlotName) et signale par TakeLoadMessages ; ancien format ->
	 *  migre en memoire (une copie intacte est gardee sous LegacySlotName) */
	BACKROOMS_API UBRSaveGame* Load(int32 Slot);
	/** Ecriture immediate (attend aussi les ecritures en cours). v4.8 : refusee (false, echec garde) si l'emplacement
	 *  contient une partie d'un format plus recent */
	BACKROOMS_API bool Write(int32 Slot, UBRSaveGame* Save);
	/** v4.7 : instantane serialise tout de suite (thread du jeu), fichier ecrit sur un thread de fond.
	 *  Les ecritures se font dans l'ordre des appels. false si l'instantane n'a pas pu etre fait */
	BACKROOMS_API bool WriteAsync(int32 Slot, UBRSaveGame* Save, uint32* OutRequestId = nullptr);
	/** v4.7 : attend la fin de toutes les ecritures en cours (fermeture du jeu, retour au menu) */
	BACKROOMS_API void Flush();
	/** v4.7 : false tant qu'un echec d'ecriture n'est pas acquitte (v4.8 : une reussite suivante ne l'efface plus) */
	BACKROOMS_API bool LastWriteSucceeded();
	/** v4.8 : echecs d'ecriture non acquittes (emplacement, numero de demande, raison) */
	BACKROOMS_API TArray<FWriteFailure> PendingFailures();
	/** v4.8 : acquitte les echecs jusqu'a cette demande (incluse), une fois montres au joueur */
	BACKROOMS_API void AcknowledgeFailures(uint32 UpToRequestId);
	/** v4.8 : format du fichier de cet emplacement s'il est plus recent que ce jeu (0 sinon) : lecture seule */
	BACKROOMS_API int32 FutureFormatOf(int32 Slot);
	/** v4.7 : messages pour le joueur depuis le dernier appel (sauvegarde restauree, fichier illisible mis de cote) */
	BACKROOMS_API TArray<FString> TakeLoadMessages();
	BACKROOMS_API void Delete(int32 Slot);
	/** v4.7 : tests automatiques : prefixe des emplacements (les parties du joueur ne sont jamais touchees) et chemin
	 *  complet d'un fichier de sauvegarde (Saved/SaveGames/<nom>.sav) */
	BACKROOMS_API void SetTestPrefix(const FString& Prefix);
	BACKROOMS_API FString FilePath(const FString& SlotFileName);
	/** Premier emplacement libre (INDEX_NONE si tout est pris) */
	BACKROOMS_API int32 FreeSlot();
	/** Partie en cours : survit au rechargement de la carte (heberger une partie, revenir au menu). INDEX_NONE : aucune */
	BACKROOMS_API int32& ActiveSlot();
	/** v4.7 : session a reprendre au prochain chargement de son niveau (survit au rechargement de la carte quand
	 *  l'hote ouvre une partie en ligne). Consommee par ABRWorld::LoadLevelNow si le niveau et la graine concordent */
	BACKROOMS_API FBRSessionState& PendingResume();
	/** "2 h 05", "14 min" */
	BACKROOMS_API FString FormatPlayTime(float Seconds);
	/** Date et heure au format de la langue courante */
	BACKROOMS_API FString FormatDate(const FDateTime& Date);
	/** v4.8 : date seule au format de la langue courante */
	BACKROOMS_API FString FormatDay(const FDateTime& Date);
}
