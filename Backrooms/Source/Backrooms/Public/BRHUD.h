// Interface dessinee au Canvas (aucun asset UMG necessaire).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BRHUD.generated.h"

class UFont;

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

	void DrawMenu();
	void DrawTitleCard();
	void DrawRecOverlay();
	void DrawStats();
	void DrawCrosshair();
	void DrawMessages(float Dt);
	void DrawNote();
	void DrawJournal();
	void DrawDeath();
	void DrawPause();
	void DrawGlitch(float Amount);

	void Txt(const FString& S, float X, float Y, const FLinearColor& C, float Scale, UFont* Font, bool bCenter = false, bool bShadow = true);
	void Bar(float X, float Y, float W, float H, float Fill, const FLinearColor& C, const FString& Label);
	TArray<FString> Wrap(const FString& S, float MaxWidth, UFont* Font, float Scale);
	float Ui() const;

	TArray<FMsg> Messages;
	double LastTime = 0.0;
	float Clock = 0.f;
};
