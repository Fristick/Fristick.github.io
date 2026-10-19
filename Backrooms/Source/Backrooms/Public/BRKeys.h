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
	PushToTalk,
	/** v4.13 : scanner LiDAR du Level 6 : impulsion large, scan focalise (maintenu) */
	LidarPulse,
	LidarFocus,
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

	/** v4.16 : invites a la manette. Quand la manette a servi en dernier (ABRPlayerController::IsUsingGamepad, mis a jour
	 *  a chaque image), Primary, Tag et Expand nomment le bouton fixe de l'action (GamepadKey) au lieu de la touche du
	 *  clavier ; une action sans bouton (poches, chat vocal) garde sa touche. Avant : toujours le clavier ("[E] Ramasser"
	 *  devant un joueur a la manette) */
	void SetGamepadPrompts(bool bPad);
	bool UsesGamepadPrompts();
	/** Bouton de la manette lie a l'action par ABRPlayerController::RebuildMappings (FKey invalide : aucun ; les quatre
	 *  actions du deplacement : stick gauche, nomme une seule fois par Expand quand elles se suivent) */
	FKey GamepadKey(EBRAction Action);
	/** Nom court d'un bouton de la manette (A, B, X, Y, LB, RB, LT, RT, LS, RS, VUE, MENU, CROIX...) */
	FString GamepadName(const FKey& Key);
	/** v4.16 (relecture) : nom de la croix entiere (ses quatre directions) */
	FString GamepadDpadName();
}
