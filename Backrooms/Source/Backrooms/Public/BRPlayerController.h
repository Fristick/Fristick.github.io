// Entrees (Enhanced Input cree entierement en C++), menu titre, pause, journal, commandes console.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "BRPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class ABRCharacter;

UCLASS()
class BACKROOMS_API ABRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABRPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	bool IsInMenu() const { return bInMenu; }
	int32 GetMenuIndex() const { return MenuIndex; }
	bool IsJournalOpen() const { return bJournal; }
	bool IsPauseMenuOpen() const { return bPauseMenu; }

	float MouseSensitivity = 1.f;
	bool bInvertY = false;

	// ---- Commandes console (touche ` ou \u00b2) ----
	/** Aller a un niveau : BRLevel 37 */
	UFUNCTION(Exec)
	void BRLevel(int32 Number);

	/** Invincibilite */
	UFUNCTION(Exec)
	void BRGod();

	/** Faire apparaitre une entite devant soi : BRSpawn 0..7 */
	UFUNCTION(Exec)
	void BRSpawn(int32 Kind);

	/** Sensibilite de la souris : BRSensitivity 1.5 */
	UFUNCTION(Exec)
	void BRSensitivity(float Value);

	UFUNCTION(Exec)
	void BRInvertY();

	/** +5 eau d'amande, +5 piles */
	UFUNCTION(Exec)
	void BRGiveAll();

protected:
	UPROPERTY()
	TObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveAction;
	UPROPERTY()
	TObjectPtr<UInputAction> LookAction;
	UPROPERTY()
	TObjectPtr<UInputAction> LookPadAction;
	UPROPERTY()
	TObjectPtr<UInputAction> JumpAction;
	UPROPERTY()
	TObjectPtr<UInputAction> SprintAction;
	UPROPERTY()
	TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY()
	TObjectPtr<UInputAction> FlashAction;
	UPROPERTY()
	TObjectPtr<UInputAction> InteractAction;
	UPROPERTY()
	TObjectPtr<UInputAction> DrinkAction;
	UPROPERTY()
	TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY()
	TObjectPtr<UInputAction> JournalAction;
	UPROPERTY()
	TObjectPtr<UInputAction> PauseAction;
	UPROPERTY()
	TObjectPtr<UInputAction> QuitAction;
	UPROPERTY()
	TObjectPtr<UInputAction> MenuPrevAction;
	UPROPERTY()
	TObjectPtr<UInputAction> MenuNextAction;
	UPROPERTY()
	TObjectPtr<UInputAction> MenuConfirmAction;

private:
	void EnsureInput();
	void AddMappingToPlayer();
	UInputAction* MakeAction(const TCHAR* Name, EInputActionValueType Type, bool bWhenPaused = false);
	void MapKey(UInputAction* Action, const FKey& Key, bool bSwizzle = false, bool bNegate = false);
	ABRCharacter* GetBRCharacter() const;
	bool CanPlay() const;

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnLookPad(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpCompleted(const FInputActionValue& Value);
	void OnSprintStarted(const FInputActionValue& Value);
	void OnSprintCompleted(const FInputActionValue& Value);
	void OnCrouch(const FInputActionValue& Value);
	void OnFlash(const FInputActionValue& Value);
	void OnInteract(const FInputActionValue& Value);
	void OnDrink(const FInputActionValue& Value);
	void OnReload(const FInputActionValue& Value);
	void OnJournalStarted(const FInputActionValue& Value);
	void OnJournalCompleted(const FInputActionValue& Value);
	void OnPause(const FInputActionValue& Value);
	void OnQuit(const FInputActionValue& Value);
	void OnMenuPrev(const FInputActionValue& Value);
	void OnMenuNext(const FInputActionValue& Value);
	void OnMenuConfirm(const FInputActionValue& Value);

	bool bInMenu = true;
	bool bJournal = false;
	bool bPauseMenu = false;
	bool bMappingAdded = false;
	int32 MenuIndex = 0;
};
