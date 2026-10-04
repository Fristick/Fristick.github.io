// Sauvegardes (v4.1) : une partie = un emplacement (Saved/SaveGames/BR_Partie_<n>.sav).
// On y garde les niveaux deja explores (les seuls que l'on peut choisir ensuite), le dernier niveau atteint,
// l'inventaire et l'etat du joueur, le journal (entites rencontrees, notes lues), le temps de jeu.
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

UCLASS()
class BACKROOMS_API UBRSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
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

	UPROPERTY(SaveGame)
	TArray<FString> Notes;

	UPROPERTY(SaveGame)
	int32 Deaths = 0;

	bool IsExplored(int32 Level) const { return Explored.Contains(Level); }
	void MarkExplored(int32 Level) { Explored.AddUnique(Level); }
};

namespace BRSaves
{
	/** Nombre d'emplacements de sauvegarde */
	constexpr int32 MaxSlots = 6;

	BACKROOMS_API FString SlotName(int32 Slot);
	/** nullptr si l'emplacement est vide ou illisible */
	BACKROOMS_API UBRSaveGame* Load(int32 Slot);
	BACKROOMS_API bool Write(int32 Slot, UBRSaveGame* Save);
	BACKROOMS_API void Delete(int32 Slot);
	/** Premier emplacement libre (INDEX_NONE si tout est pris) */
	BACKROOMS_API int32 FreeSlot();
	/** Partie en cours : survit au rechargement de la carte (heberger une partie, revenir au menu). INDEX_NONE : aucune */
	BACKROOMS_API int32& ActiveSlot();
	/** "2 h 05", "14 min" */
	BACKROOMS_API FString FormatPlayTime(float Seconds);
	/** "04/10/2026 22:54" */
	BACKROOMS_API FString FormatDate(const FDateTime& Date);
}
