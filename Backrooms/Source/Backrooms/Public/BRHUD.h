// Interface dessinee au Canvas (aucun asset UMG necessaire) : HUD du camescope, barre des poches,
// et l'ecran d'inventaire (TAB) inspire de Backrooms : Escape Together.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BRTypes.h"
#include "BRHUD.generated.h"

class UFont;
class UTexture;
class ABRCharacter;
class ABRWorld;
class ABRPlayerController;

UCLASS()
class BACKROOMS_API ABRHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Affiche un message temporaire en haut de l'ecran */
	static void Notify(const UObject* WorldContext, const FString& Text, float Duration = 4.f, FLinearColor Color = FLinearColor::White);
	void AddMessage(const FString& Text, float Duration, const FLinearColor& Color);

private:
	struct FMsg
	{
		FString Text;
		float Age = 0.f;
		float Duration = 4.f;
		FLinearColor Color = FLinearColor::White;
	};

	enum class ETab : uint8 { Character, Journal, Settings };

	/** Reference a une case d'inventaire */
	struct FSlotRef
	{
		EBRSlotGroup Group = EBRSlotGroup::Pockets;
		int32 Index = INDEX_NONE;
		bool IsValid() const { return Index != INDEX_NONE; }
		bool operator==(const FSlotRef& O) const { return Group == O.Group && Index == O.Index; }
	};

	struct FSlotBox
	{
		FSlotRef Ref;
		float X = 0.f, Y = 0.f, S = 0.f;
		FString Caption;   // "1", "TETE"...
	};

	struct FButton
	{
		int32 Id = 0;
		float X = 0.f, Y = 0.f, W = 0.f, H = 0.f;
	};

	// ---- Jeu
	void DrawMenu();
	void DrawTitleCard();
	void DrawCamcorder(ABRCharacter* C, ABRWorld* W);
	void DrawStats(ABRCharacter* C);
	void DrawQuickBar(ABRCharacter* C);
	void DrawObjectiveTracker(ABRWorld* W);
	void DrawCrosshair(ABRCharacter* C);
	void DrawMessages(float Dt);
	void DrawNote(ABRCharacter* C);
	void DrawDeath(ABRCharacter* C);
	void DrawPause();
	void DrawGlitch(float Amount);
	void DrawContentWarning(float Y);

	// ---- Inventaire (TAB)
	void DrawInventory(ABRPlayerController* PC, ABRCharacter* C, ABRWorld* W);
	void LayoutCharacterTab(ABRCharacter* C);
	void DrawCharacterTab(ABRCharacter* C, ABRWorld* W);
	void DrawJournalTab(ABRCharacter* C, ABRWorld* W);
	void DrawSettingsTab(ABRPlayerController* PC);
	void HandleInventoryMouse(ABRPlayerController* PC, ABRCharacter* C);
	void DrawSlot(ABRCharacter* C, const FSlotBox& Box);
	void DrawTooltip(ABRCharacter* C);
	void DrawInspect(ABRCharacter* C);

	// ---- Primitives
	void Txt(const FString& S, float X, float Y, const FLinearColor& C, float Scale, UFont* Font, bool bCenter = false, bool bShadow = true);
	void TxtRight(const FString& S, float RightX, float Y, const FLinearColor& C, float Scale, UFont* Font);
	float TextW(const FString& S, UFont* Font, float Scale);
	void Bar(float X, float Y, float W, float H, float Fill, const FLinearColor& C, const FString& Label);
	void Frame(float X, float Y, float W, float H, const FLinearColor& C, float Thickness);
	void Corners(float X, float Y, float W, float H, float Len, const FLinearColor& C, float Thickness);
	void Panel(float X, float Y, float W, float H, const FString& Title);
	void VLabel(const FString& S, float X, float Y, const FLinearColor& C, float Scale);
	void TrendBox(float X, float Y, float Size, float Trend);
	void Icon(UTexture* Tex, float X, float Y, float W, float H, const FLinearColor& Tint);
	void Scanlines(float Alpha);
	bool Hover(float X, float Y, float W, float H) const;
	int32 ButtonAt(float X, float Y) const;
	void AddButton(int32 Id, float X, float Y, float W, float H);
	TArray<FString> Wrap(const FString& S, float MaxWidth, UFont* Font, float Scale);
	float Ui() const;
	FSlotBox* FindSlot(float X, float Y);
	UTexture* ItemIcon(EBRItem Item);

	TArray<FMsg> Messages;
	double LastTime = 0.0;
	float Clock = 0.f;

	// Inventaire
	ETab Tab = ETab::Character;
	TArray<FSlotBox> Slots;
	TArray<FButton> Buttons;
	FSlotRef Dragging;
	FSlotRef HoverSlot;
	FSlotRef Inspecting;
	FSlotRef LastClickSlot;
	double LastClickTime = 0.0;
	float MouseX = 0.f;
	float MouseY = 0.f;
	int32 HoverSetting = INDEX_NONE;
	bool bWasInventoryOpen = false;

	// Zone de l'inventaire (calculee a chaque image)
	float IX = 0.f, IY = 0.f, IW = 0.f, IH = 0.f;
	float ColW[3] = { 0.f, 0.f, 0.f };
	float ColX[3] = { 0.f, 0.f, 0.f };
	float SilX = 0.f, SilY = 0.f, SilW = 0.f, SilH = 0.f;
};
