// Touches configurables : 3 touches par action, modifiables a tout moment (inventaire > TOUCHES, menu pause,
// menu titre) et sauvegardees dans GameUserSettings.ini.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

enum class EBRAction : uint8
{
	MoveForward,
	MoveBackward,
	MoveLeft,
	MoveRight,
	Jump,
	Sprint,
	Crouch,
	Interact,
	Flashlight,
	NightVision,
	Inventory,
	Pocket1,
	Pocket2,
	Pocket3,
	Pocket4,
	Drink,
	Bandage,
	Battery,
	ThirdPerson,
	Pause,
	Count
};

namespace BRKeys
{
	constexpr int32 SlotsPerAction = 3;

	int32 NumActions();
	/** Libelle de l'action (FR, majuscules) */
	FString ActionLabel(EBRAction Action);
	FKey GetKey(EBRAction Action, int32 Slot);
	/** Assigne une touche ; si elle servait ailleurs, elle y est retiree (OutRemovedFrom = action concernee) */
	void SetKey(EBRAction Action, int32 Slot, const FKey& Key, FString* OutRemovedFrom = nullptr);
	void ResetDefaults();
	void Load();
	void Save();
	/** Nom court d'une touche ("E", "MAJ G.", "&"...) */
	FString KeyName(const FKey& Key);
	/** Nom de la premiere touche valide d'une action, pour les messages "[E] Ramasser" */
	FString Primary(EBRAction Action);
	/** "[E]" */
	FString Tag(EBRAction Action);
	/** Remplace {Interact}, {Inventory}, {Flashlight}, {NightVision}, {Drink}, {Battery}, {Bandage}... par "[touche]" */
	FString Expand(const FString& Text);
	/** Touches que l'on peut assigner (clavier + boutons de souris sauf le clic gauche) */
	bool IsBindable(const FKey& Key);
	/** Incremente a chaque modification (le controleur reconstruit alors ses entrees) */
	int32 Revision();
}
