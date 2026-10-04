#include "BRHUD.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BREntity.h"
#include "BRItems.h"
#include "BRKeys.h"
#include "BRLevels.h"
#include "BRPlayerController.h"
#include "BRWorld.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// Palette de l'interface (jaune "Escape Together" sur fond sombre translucide)
	const FLinearColor Yellow(1.f, 0.82f, 0.22f, 1.f);
	const FLinearColor YellowDim(0.95f, 0.78f, 0.25f, 0.45f);
	const FLinearColor Ink(0.93f, 0.91f, 0.84f, 1.f);
	const FLinearColor InkDim(0.75f, 0.72f, 0.62f, 0.8f);
	const FLinearColor PanelBg(0.035f, 0.03f, 0.012f, 0.72f);
	const FLinearColor SlotBg(0.f, 0.f, 0.f, 0.5f);
	const FLinearColor Danger(0.95f, 0.3f, 0.22f, 1.f);
	const FLinearColor Done(0.55f, 0.88f, 0.5f, 0.9f);

	enum EButtonId
	{
		Btn_TabCharacter = 100,
		Btn_TabJournal = 101,
		Btn_TabSettings = 102,
		Btn_TabKeys = 103,
		Btn_InspectUse = 200,
		Btn_InspectClose = 201,
		Btn_SettingBase = 300,   // 300 + ligne * 2 (+0 = moins, +1 = plus)
		Btn_SettingRow = 600,    // 600 + ligne (clic sur la ligne entiere)
		Btn_KeysReset = 900,
		Btn_PauseResume = 950,
		Btn_PauseSettings = 951,
		Btn_PauseKeys = 952,
		Btn_PauseQuit = 953,
		Btn_PauseMainMenu = 954,
		Btn_Menu = 960,          // 960 + element de la page du menu principal
		Btn_MenuLevelPrev = 980,
		Btn_MenuLevelNext = 981,
		Btn_KeySlot = 1000       // 1000 + action * 3 + case
	};

	FLinearColor ClassColor(int32 Class)
	{
		switch (Class)
		{
		case 0:
		case 1:
			return FLinearColor(0.55f, 0.95f, 0.55f);
		case 2:
			return FLinearColor(1.f, 0.9f, 0.4f);
		case 3:
			return FLinearColor(1.f, 0.6f, 0.25f);
		default:
			return FLinearColor(1.f, 0.3f, 0.25f);
		}
	}

	FString Timecode(float Seconds)
	{
		const int32 T = FMath::FloorToInt(Seconds);
		return FString::Printf(TEXT("%02d:%02d:%02d"), T / 3600, (T / 60) % 60, T % 60);
	}

	FLinearColor WithAlpha(FLinearColor C, float A)
	{
		C.A *= A;
		return C;
	}

}

FString ABRHUD::ControlsLine(int32 Line) const
{
	using namespace BRKeys;
	if (Line == 0)
	{
		return FString::Printf(TEXT("%s%s%s%s  se d\u00e9placer     %s  courir     %s  s'accroupir     %s  sauter     %s  interagir     %s  vue 3e personne"),
			*Primary(EBRAction::MoveForward), *Primary(EBRAction::MoveLeft), *Primary(EBRAction::MoveBackward), *Primary(EBRAction::MoveRight),
			*Primary(EBRAction::Sprint), *Primary(EBRAction::Crouch), *Primary(EBRAction::Jump), *Primary(EBRAction::Interact),
			*Primary(EBRAction::ThirdPerson));
	}
	return FString::Printf(TEXT("%s  lampe     %s  vision nocturne     %s-%s  poches     %s  eau d'amande     %s  bandage     %s  piles     %s  inventaire     %s  pause"),
		*Primary(EBRAction::Flashlight), *Primary(EBRAction::NightVision), *Primary(EBRAction::Pocket1), *Primary(EBRAction::Pocket4),
		*Primary(EBRAction::Drink), *Primary(EBRAction::Bandage), *Primary(EBRAction::Battery), *Primary(EBRAction::Inventory),
		*Primary(EBRAction::Pause));
}

float ABRHUD::Ui() const
{
	return Canvas ? FMath::Max(0.5f, Canvas->ClipY / 1080.f) : 1.f;
}

void ABRHUD::Notify(const UObject* WorldContext, const FString& Text, float Duration, FLinearColor Color)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContext, 0);
	if (ABRHUD* H = PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr)
	{
		H->AddMessage(Text, Duration, Color);
	}
}

void ABRHUD::AddMessage(const FString& Text, float Duration, const FLinearColor& Color)
{
	for (FMsg& M : Messages)
	{
		if (M.Text == Text)
		{
			M.Age = 0.f;
			return;
		}
	}
	FMsg M;
	M.Text = Text;
	M.Duration = Duration;
	M.Color = Color;
	Messages.Add(M);
	if (Messages.Num() > 5)
	{
		Messages.RemoveAt(0);
	}
}

// =====================================================================================================================
// Primitives
// =====================================================================================================================

void ABRHUD::Txt(const FString& S, float X, float Y, const FLinearColor& C, float Scale, UFont* Font, bool bCenter, bool bShadow)
{
	if (!Font)
	{
		Font = GEngine->GetMediumFont();
	}
	if (bCenter)
	{
		X -= TextW(S, Font, Scale) * 0.5f;
	}
	if (bShadow)
	{
		DrawText(S, FLinearColor(0.f, 0.f, 0.f, C.A * 0.8f), X + 2.f * Ui(), Y + 2.f * Ui(), Font, Scale);
	}
	DrawText(S, C, X, Y, Font, Scale);
}

void ABRHUD::TxtRight(const FString& S, float RightX, float Y, const FLinearColor& C, float Scale, UFont* Font)
{
	Txt(S, RightX - TextW(S, Font ? Font : GEngine->GetMediumFont(), Scale), Y, C, Scale, Font);
}

float ABRHUD::TextW(const FString& S, UFont* Font, float Scale)
{
	float TW = 0.f;
	float TH = 0.f;
	GetTextSize(S, TW, TH, Font ? Font : GEngine->GetMediumFont(), Scale);
	return TW;
}

TArray<FString> ABRHUD::Wrap(const FString& S, float MaxWidth, UFont* Font, float Scale)
{
	TArray<FString> Lines;
	TArray<FString> Words;
	S.ParseIntoArray(Words, TEXT(" "), true);
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Test = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		if (TextW(Test, Font, Scale) > MaxWidth && !Line.IsEmpty())
		{
			Lines.Add(Line);
			Line = Word;
		}
		else
		{
			Line = Test;
		}
	}
	if (!Line.IsEmpty())
	{
		Lines.Add(Line);
	}
	return Lines;
}

void ABRHUD::Bar(float X, float Y, float W, float H, float Fill, const FLinearColor& C, const FString& Label)
{
	const float U = Ui();
	Txt(Label, X, Y - 18.f * U, FLinearColor(1.f, 1.f, 1.f, 0.75f), 0.8f * U, GEngine->GetSmallFont());
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, Y, W, H);
	DrawRect(C, X, Y, W * FMath::Clamp(Fill, 0.f, 1.f), H);
}

void ABRHUD::Frame(float X, float Y, float W, float H, const FLinearColor& C, float Thickness)
{
	DrawRect(C, X, Y, W, Thickness);
	DrawRect(C, X, Y + H - Thickness, W, Thickness);
	DrawRect(C, X, Y + Thickness, Thickness, H - 2.f * Thickness);
	DrawRect(C, X + W - Thickness, Y + Thickness, Thickness, H - 2.f * Thickness);
}

void ABRHUD::Corners(float X, float Y, float W, float H, float Len, const FLinearColor& C, float Thickness)
{
	const float RX = X + W;
	const float BY = Y + H;
	DrawRect(C, X, Y, Len, Thickness);
	DrawRect(C, X, Y, Thickness, Len);
	DrawRect(C, RX - Len, Y, Len, Thickness);
	DrawRect(C, RX - Thickness, Y, Thickness, Len);
	DrawRect(C, X, BY - Thickness, Len, Thickness);
	DrawRect(C, X, BY - Len, Thickness, Len);
	DrawRect(C, RX - Len, BY - Thickness, Len, Thickness);
	DrawRect(C, RX - Thickness, BY - Len, Thickness, Len);
}

void ABRHUD::Panel(float X, float Y, float W, float H, const FString& Title)
{
	const float U = Ui();
	DrawRect(PanelBg, X, Y, W, H);
	Frame(X, Y, W, H, FLinearColor(0.95f, 0.78f, 0.25f, 0.18f), 1.f * U);
	Corners(X, Y, W, H, 16.f * U, Yellow, 2.f * U);
	if (!Title.IsEmpty())
	{
		Txt(Title, X + 18.f * U, Y + 12.f * U, Yellow, 0.95f * U, GEngine->GetMediumFont(), false, false);
		DrawRect(YellowDim, X + 18.f * U, Y + 42.f * U, W - 36.f * U, 1.f * U);
	}
}

void ABRHUD::VLabel(const FString& S, float X, float Y, const FLinearColor& C, float Scale)
{
	// Libelle vertical : lettres empilees
	UFont* Font = GEngine->GetSmallFont();
	float LY = Y;
	for (int32 i = 0; i < S.Len(); ++i)
	{
		const FString Ch = S.Mid(i, 1);
		Txt(Ch, X - TextW(Ch, Font, Scale) * 0.5f, LY, C, Scale, Font, false, false);
		LY += 15.f * Scale;
	}
}

void ABRHUD::TrendBox(float X, float Y, float Size, float Trend)
{
	const float U = Ui();
	Frame(X, Y, Size, Size, YellowDim, 1.f * U);
	const float CX = X + Size * 0.5f;
	const float CY = Y + Size * 0.5f;
	const float A = Size * 0.28f;
	if (FMath::Abs(Trend) < 0.05f)
	{
		DrawRect(InkDim, CX - A, CY - 1.f * U, A * 2.f, 2.f * U);
		return;
	}
	const bool bUp = Trend > 0.f;
	const FLinearColor C = bUp ? Done : Danger;
	const float Dir = bUp ? -1.f : 1.f;
	// fleche : tige + pointe
	DrawLine(CX, CY - Dir * A, CX, CY + Dir * A, C, 2.f * U);
	DrawLine(CX, CY + Dir * A, CX - A * 0.7f, CY + Dir * A * 0.3f, C, 2.f * U);
	DrawLine(CX, CY + Dir * A, CX + A * 0.7f, CY + Dir * A * 0.3f, C, 2.f * U);
}

void ABRHUD::Icon(UTexture* Tex, float X, float Y, float W, float H, const FLinearColor& Tint)
{
	if (Tex)
	{
		DrawTexture(Tex, X, Y, W, H, 0.f, 0.f, 1.f, 1.f, Tint, BLEND_Translucent);
	}
}

void ABRHUD::Scanlines(float Alpha)
{
	const float U = Ui();
	const float Step = FMath::Max(3.f, 4.f * U);
	const float Offset = FMath::Fmod(Clock * 20.f * U, Step);
	for (float Y = Offset; Y < Canvas->ClipY; Y += Step)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, Alpha), 0.f, Y, Canvas->ClipX, FMath::Max(1.f, U));
	}
}

bool ABRHUD::Hover(float X, float Y, float W, float H) const
{
	return MouseX >= X && MouseX <= X + W && MouseY >= Y && MouseY <= Y + H;
}

void ABRHUD::AddButton(int32 Id, float X, float Y, float W, float H)
{
	FButton B;
	B.Id = Id;
	B.X = X;
	B.Y = Y;
	B.W = W;
	B.H = H;
	Buttons.Add(B);
}

int32 ABRHUD::ButtonAt(float X, float Y) const
{
	// Les derniers boutons ajoutes sont au-dessus
	for (int32 i = Buttons.Num() - 1; i >= 0; --i)
	{
		const FButton& B = Buttons[i];
		if (X >= B.X && X <= B.X + B.W && Y >= B.Y && Y <= B.Y + B.H)
		{
			return B.Id;
		}
	}
	return INDEX_NONE;
}

ABRHUD::FSlotBox* ABRHUD::FindSlot(float X, float Y)
{
	for (FSlotBox& B : Slots)
	{
		if (X >= B.X && X <= B.X + B.S && Y >= B.Y && Y <= B.Y + B.S)
		{
			return &B;
		}
	}
	return nullptr;
}

UTexture* ABRHUD::ItemIcon(EBRItem Item)
{
	UBRAssets* A = UBRAssets::Get(this);
	return A ? A->Icon(BRItems::Get(Item).Icon) : nullptr;
}

// =====================================================================================================================
// Boucle principale
// =====================================================================================================================

void ABRHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	const float Dt = LastTime > 0.0 ? FMath::Min(static_cast<float>(Now - LastTime), 0.1f) : 0.f;
	LastTime = Now;
	Clock += Dt;

	ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	ABRCharacter* C = PC ? Cast<ABRCharacter>(PC->GetPawn()) : nullptr;
	ABRWorld* W = ABRWorld::Get(this);

	if (PC && PC->IsInMenu())
	{
		if (W && W->GetFade() > 0.001f)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, W->GetFade()), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		}
		DrawMenu();
		if (PC->IsInventoryOpen())
		{
			DrawInventory(PC, C, W); // parametres et touches depuis le menu titre
		}
		DrawMessages(Dt);
		return;
	}

	if (PC && C && C->IsDead() && PC->IsInventoryOpen())
	{
		PC->SetInventoryOpen(false);
	}
	const bool bInv = PC && PC->IsInventoryOpen();
	if (!bInv && bWasInventoryOpen)
	{
		Dragging = FSlotRef();
		Inspecting = FSlotRef();
		HoverSlot = FSlotRef();
	}
	bWasInventoryOpen = bInv;

	if (C && !bInv)
	{
		DrawTeammates(C);
		DrawVoiceIndicator(PC);
	}
	if (C && !C->IsDead() && !bInv)
	{
		DrawCamcorder(C, W);
		DrawCrosshair(C);
		DrawStats(C);
		DrawQuickBar(C);
		DrawObjectiveTracker(W);
	}
	if (!bInv)
	{
		DrawTitleCard();
	}
	if (C && C->IsReadingNote() && !bInv)
	{
		DrawNote(C);
	}
	if (C && C->IsDead())
	{
		DrawDeath(C);
	}
	if (bInv)
	{
		DrawInventory(PC, C, W);
	}
	DrawMessages(Dt);
	if (W)
	{
		if (W->GetGlitch() > 0.01f)
		{
			DrawGlitch(W->GetGlitch());
		}
		if (W->GetFade() > 0.001f)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, W->GetFade()), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		}
	}
	if (PC && PC->IsPauseMenuOpen() && !bInv)
	{
		DrawPause(PC);
	}
}

// =====================================================================================================================
// Menu titre, cartes de titre
// =====================================================================================================================

void ABRHUD::DrawContentWarning(float Y)
{
	UBRAssets* A = UBRAssets::Get(this);
	if (!A)
	{
		return;
	}
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	UFont* Medium = GEngine->GetMediumFont();
	if (!A->HasContent())
	{
		Txt(TEXT("Textures indisponibles : ouvrez le projet dans l'\u00e9diteur (plugin Python actif) pour importer les ressources."), CX, Y,
			Danger, 0.8f * U, Medium, true);
	}
	else if (A->IsUsingRuntimeContent())
	{
		Txt(TEXT("Mode secours : textures lues dans RawAssets/. Relancez l'import (Output Log > Python : import backrooms_setup; backrooms_setup.run(True))"),
			CX, Y, FLinearColor(1.f, 0.8f, 0.35f, 0.85f), 0.72f * U, Medium, true);
	}
	if (!A->Sound(TEXT("S_Hum")))
	{
		Txt(TEXT("Sons non import\u00e9s : le jeu sera silencieux tant que l'import Python n'aura pas \u00e9t\u00e9 fait."), CX, Y + 22.f * U,
			FLinearColor(1.f, 0.8f, 0.35f, 0.85f), 0.72f * U, Medium, true);
	}
}

void ABRHUD::MenuButton(int32 Item, const FString& Label, float CX, float Y, float W, float H, bool bInteractive)
{
	const ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	const float U = Ui();
	const float X = CX - W * 0.5f;
	const bool bSel = PC && PC->GetMenuCursor() == Item;
	DrawRect(bSel ? WithAlpha(Yellow, 0.9f) : FLinearColor(0.f, 0.f, 0.f, 0.55f), X, Y, W, H);
	Frame(X, Y, W, H, bSel ? Yellow : YellowDim, 1.f * U);
	if (bSel)
	{
		Txt(TEXT(">"), X + 18.f * U, Y + (H - 26.f * U) * 0.5f, FLinearColor(0.05f, 0.04f, 0.01f), 1.05f * U, GEngine->GetMediumFont(), false, false);
	}
	Txt(Label, CX, Y + (H - 26.f * U) * 0.5f, bSel ? FLinearColor(0.05f, 0.04f, 0.01f) : Ink, 1.05f * U, GEngine->GetMediumFont(), true, false);
	if (bInteractive)
	{
		AddButton(Btn_Menu + Item, X, Y, W, H);
	}
}

void ABRHUD::DrawLevelCard(float Y, bool bFull)
{
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	const ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	const TArray<FBRLevelDef>& All = BRLevels::All();
	const int32 Index = PC ? FMath::Clamp(PC->GetMenuIndex(), 0, All.Num() - 1) : 0;
	const FBRLevelDef& D = All[Index];

	// Selecteur de niveau : fleches cliquables de part et d'autre
	const FString Sel = FString::Printf(TEXT("NIVEAU %d"), D.Number);
	Txt(Sel, CX, Y, FLinearColor::White, 1.7f * U, Large, true);
	const float SW = TextW(Sel, Large, 1.7f * U);
	const float AW = 54.f * U;
	const float AH = 54.f * U;
	const float LX = CX - SW * 0.5f - AW - 40.f * U;
	const float RX = CX + SW * 0.5f + 40.f * U;
	for (int32 k = 0; k < 2; ++k)
	{
		const float AX = k == 0 ? LX : RX;
		const bool bHov = Hover(AX, Y - 4.f * U, AW, AH);
		Frame(AX, Y - 4.f * U, AW, AH, bHov ? Yellow : YellowDim, 1.f * U);
		Txt(k == 0 ? TEXT("<") : TEXT(">"), AX + AW * 0.5f, Y + 4.f * U, bHov ? Yellow : Ink, 1.3f * U, Large, true, false);
		AddButton(k == 0 ? Btn_MenuLevelPrev : Btn_MenuLevelNext, AX, Y - 4.f * U, AW, AH);
	}
	Txt(FString::Printf(TEXT("\u00ab %s \u00bb  -  %s"), *D.Title, *D.Nickname), CX, Y + 70.f * U, FLinearColor(1.f, 0.9f, 0.6f), 1.1f * U, Medium, true);
	Txt(D.ClassText, CX, Y + 108.f * U, ClassColor(D.SurvivalClass), 0.95f * U, Medium, true);
	if (!bFull)
	{
		return;
	}
	const TArray<FString> Lines = Wrap(D.Description, Canvas->ClipX * 0.5f, Medium, 0.9f * U);
	float LY = Y + 150.f * U;
	for (const FString& L : Lines)
	{
		Txt(L, CX, LY, FLinearColor(0.85f, 0.85f, 0.85f), 0.9f * U, Medium, true);
		LY += 26.f * U;
	}
	if (D.bRequireObjectives)
	{
		Txt(FString::Printf(TEXT("Objectifs : %d cassettes VHS + filmer pendant une coupure de courant"), D.VHSRequired), CX, LY + 6.f * U,
			Yellow, 0.82f * U, Medium, true);
		LY += 26.f * U;
	}
	if (Index == 0)
	{
		Txt(TEXT("(Recommand\u00e9 pour commencer : le vrai d\u00e9but de l'aventure)"), CX, LY + 6.f * U, FLinearColor(0.6f, 0.9f, 0.6f, 0.8f), 0.8f * U, Medium, true);
	}
}

void ABRHUD::HandleMenuMouse(ABRPlayerController* PC)
{
	if (!PC || !PlayerOwner)
	{
		return;
	}
	// Le survol ne deplace la selection que si la souris bouge (sinon le clavier reprend la main)
	const bool bMoved = FMath::Abs(MouseX - LastMenuMouseX) > 1.f || FMath::Abs(MouseY - LastMenuMouseY) > 1.f;
	LastMenuMouseX = MouseX;
	LastMenuMouseY = MouseY;
	const int32 Id = ButtonAt(MouseX, MouseY);
	if (bMoved && Id >= Btn_Menu && Id < Btn_Menu + 8)
	{
		PC->SetMenuCursor(Id - Btn_Menu);
	}
	if (!PlayerOwner->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		return;
	}
	if (Id >= Btn_Menu && Id < Btn_Menu + 8)
	{
		PC->MenuActivate(Id - Btn_Menu);
	}
	else if (Id == Btn_MenuLevelPrev || Id == Btn_MenuLevelNext)
	{
		PC->MenuShiftLevel(Id == Btn_MenuLevelPrev ? -1 : 1);
	}
}

void ABRHUD::DrawMenu()
{
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	const float H = Canvas->ClipY;
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	if (!PC)
	{
		return;
	}
	const bool bInteractive = !PC->IsInventoryOpen();
	if (PlayerOwner)
	{
		PlayerOwner->GetMousePosition(MouseX, MouseY);
	}
	Buttons.Reset();

	DrawRect(FLinearColor(0.02f, 0.02f, 0.01f, 0.62f), 0.f, 0.f, Canvas->ClipX, H);
	Scanlines(0.05f);

	const float Flick = (FMath::Sin(Clock * 23.f) > 0.97f) ? 0.5f : 1.f;
	Txt(TEXT("THE BACKROOMS"), CX, H * 0.1f, FLinearColor(1.f, 0.92f, 0.6f, Flick), 2.8f * U, Large, true);
	Txt(TEXT("Si vous ne faites pas attention et que vous noclippez hors de la r\u00e9alit\u00e9 au mauvais endroit..."),
		CX, H * 0.2f, FLinearColor(0.85f, 0.82f, 0.7f, 0.9f), 1.f * U, Medium, true);

	const float BW = 480.f * U;
	const float BH = 54.f * U;
	const float Gap = 14.f * U;
	const float Blink = 0.6f + 0.4f * FMath::Sin(Clock * 3.f);
	FString Hint;
	switch (PC->GetMenuPage())
	{
	case EBRMenuPage::Main:
	{
		float Y = H * 0.34f;
		for (int32 i = 0; i < PC->GetMenuItemCount(); ++i)
		{
			MenuButton(i, PC->GetMenuItemLabel(i), CX, Y, BW, BH, bInteractive);
			Y += BH + Gap;
		}
		// Conseil multijoueur, visible des le lancement
		const FString Tip = TEXT("MULTIJOUEUR : c'est le joueur qui a l'ordinateur le plus puissant qui devrait h\u00e9berger la partie.");
		const float TW = TextW(Tip, Medium, 0.85f * U) + 40.f * U;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), CX - TW * 0.5f, Y + 10.f * U, TW, 40.f * U);
		Frame(CX - TW * 0.5f, Y + 10.f * U, TW, 40.f * U, YellowDim, 1.f * U);
		Txt(Tip, CX, Y + 19.f * U, Yellow, 0.85f * U, Medium, true, false);
		Hint = TEXT("[ \u2191 / \u2193 ]  choisir        [ ENTR\u00c9E ]  valider        [ FIN ]  quitter");
		break;
	}
	case EBRMenuPage::Solo:
	{
		Txt(TEXT("PARTIE SOLO"), CX, H * 0.27f, Yellow, 1.1f * U, Medium, true);
		DrawLevelCard(H * 0.31f, true);
		float Y = H * 0.67f;
		for (int32 i = 0; i < PC->GetMenuItemCount(); ++i)
		{
			MenuButton(i, PC->GetMenuItemLabel(i), CX, Y, BW * 0.8f, BH * 0.9f, bInteractive);
			Y += BH * 0.9f + Gap;
		}
		Hint = TEXT("[ \u2190 / \u2192 ]  choisir le niveau        [ ENTR\u00c9E ]  noclipper        [ \u00c9CHAP ]  retour");
		break;
	}
	case EBRMenuPage::Multi:
	{
		Txt(TEXT("MULTIJOUEUR  -  COOP\u00c9RATION JUSQU'\u00c0 4 JOUEURS"), CX, H * 0.25f, Yellow, 1.1f * U, Medium, true);
		{
			// Qui doit heberger ? Le PC de l'hote fait tourner le monde et les entites pour tout le groupe
			const float BoxW = FMath::Min(Canvas->ClipX - 80.f * U, 1180.f * U);
			const float BoxY = H * 0.25f + 40.f * U;
			DrawRect(FLinearColor(0.12f, 0.09f, 0.f, 0.6f), CX - BoxW * 0.5f, BoxY, BoxW, 62.f * U);
			Frame(CX - BoxW * 0.5f, BoxY, BoxW, 62.f * U, Yellow, 1.5f * U);
			Txt(TEXT("CONSEIL : le joueur qui a l'ordinateur le plus puissant (et la meilleure connexion) devrait h\u00e9berger."), CX, BoxY + 8.f * U,
				Yellow, 0.9f * U, Medium, true, false);
			Txt(TEXT("Son PC fait tourner le monde, les entit\u00e9s et leurs d\u00e9placements pour tout le groupe ; les autres le rejoignent avec son adresse IP."),
				CX, BoxY + 34.f * U, Ink, 0.75f * U, Medium, true, false);
		}
		DrawLevelCard(H * 0.37f, false);
		float Y = H * 0.52f;
		for (int32 i = 0; i < PC->GetMenuItemCount(); ++i)
		{
			MenuButton(i, PC->GetMenuItemLabel(i), CX, Y, BW, BH, bInteractive);
			Y += BH + Gap;
		}
		const FString Ip = PC->GetLocalAddress().IsEmpty() ? FString(TEXT("inconnue")) : PC->GetLocalAddress();
		Txt(FString::Printf(TEXT("Votre adresse IP : %s   (port 7777, UDP)"), *Ip), CX, Y + 10.f * U, FLinearColor(0.75f, 0.95f, 0.75f), 0.95f * U, Medium, true);
		Txt(TEXT("M\u00eame r\u00e9seau (LAN) : donnez cette adresse \u00e0 vos amis.  Par Internet : redirigez le port UDP 7777 vers ce PC sur la box,"),
			CX, Y + 40.f * U, InkDim, 0.78f * U, Medium, true);
		Txt(TEXT("ou utilisez un r\u00e9seau virtuel (Radmin VPN, ZeroTier, Tailscale...) et son adresse IP.  Chat vocal de proximit\u00e9 : Param\u00e8tres."),
			CX, Y + 62.f * U, InkDim, 0.78f * U, Medium, true);
		Hint = TEXT("[ \u2190 / \u2192 ]  niveau        [ \u2191 / \u2193 ]  choisir        [ ENTR\u00c9E ]  valider        [ \u00c9CHAP ]  retour");
		break;
	}
	case EBRMenuPage::Join:
	{
		// Le champ de saisie (Slate) est centre a l'ecran : on dessine autour
		Txt(TEXT("REJOINDRE UNE PARTIE"), CX, H * 0.3f, Yellow, 1.3f * U, Large, true);
		Txt(TEXT("ADRESSE IP DE L'H\u00d4TE"), CX, H * 0.5f - 74.f * U, Ink, 0.95f * U, Medium, true);
		Frame(CX - 268.f * U, H * 0.5f - 34.f * U, 536.f * U, 68.f * U, YellowDim, 1.f * U);
		float Y = H * 0.5f + 58.f * U;
		for (int32 i = 0; i < PC->GetMenuItemCount(); ++i)
		{
			MenuButton(i, PC->GetMenuItemLabel(i), CX, Y, BW, BH, bInteractive);
			Y += BH + Gap;
		}
		Txt(TEXT("Exemples : 192.168.1.20   ou   26.45.120.7:7777   (l'h\u00f4te voit son adresse dans MULTIJOUEUR)"), CX, Y + 14.f * U, InkDim,
			0.8f * U, Medium, true);
		Hint = TEXT("[ ENTR\u00c9E ]  se connecter        [ \u00c9CHAP ]  retour");
		break;
	}
	}

	if (!PC->GetMenuStatus().IsEmpty())
	{
		Txt(PC->GetMenuStatus(), CX, H * 0.8f, FLinearColor(1.f, 0.7f, 0.35f), 0.95f * U, Medium, true);
	}
	Txt(Hint, CX, H * 0.845f, FLinearColor(1.f, 1.f, 1.f, Blink), 0.95f * U, Medium, true);
	if (PC->GetMenuPage() == EBRMenuPage::Main || PC->GetMenuPage() == EBRMenuPage::Solo)
	{
		Txt(ControlsLine(0), CX, H * 0.88f, FLinearColor(0.7f, 0.7f, 0.7f, 0.85f), 0.75f * U, Medium, true);
		Txt(ControlsLine(1), CX, H * 0.905f, FLinearColor(0.7f, 0.7f, 0.7f, 0.85f), 0.75f * U, Medium, true);
	}
	DrawContentWarning(H * 0.925f);
	if (bInteractive)
	{
		HandleMenuMouse(PC);
	}
	Txt(TEXT("Inspir\u00e9 du Backrooms Wiki (backrooms-wiki.wikidot.com) - CC BY-SA 3.0  |  \u00a9 1992 THRESHOLD SYSTEMS"), CX, H * 0.96f,
		FLinearColor(0.5f, 0.5f, 0.5f, 0.7f), 0.7f * U, Medium, true);
}

void ABRHUD::DrawTitleCard()
{
	ABRWorld* W = ABRWorld::Get(this);
	if (!W)
	{
		return;
	}
	const float T = W->GetTitleTime();
	if (T <= 0.f || W->IsTransitioning())
	{
		return;
	}
	const float A = FMath::Clamp((7.f - T) / 1.2f, 0.f, 1.f) * FMath::Clamp(T / 1.5f, 0.f, 1.f);
	const FBRLevelDef& D = W->Def();
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	const float H = Canvas->ClipY;
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	Txt(FString::Printf(TEXT("NIVEAU %d"), D.Number), CX, H * 0.33f, FLinearColor(1.f, 1.f, 1.f, A), 2.4f * U, Large, true);
	Txt(FString::Printf(TEXT("\u00ab %s \u00bb"), *D.Title), CX, H * 0.43f, FLinearColor(1.f, 0.92f, 0.65f, A), 1.3f * U, Large, true);
	Txt(D.Nickname, CX, H * 0.49f, FLinearColor(0.85f, 0.85f, 0.85f, A), 1.f * U, Medium, true);
	FLinearColor CC = ClassColor(D.SurvivalClass);
	CC.A = A;
	Txt(D.ClassText, CX, H * 0.53f, CC, 0.9f * U, Medium, true);
}

// =====================================================================================================================
// HUD de jeu
// =====================================================================================================================

void ABRHUD::DrawCamcorder(ABRCharacter* C, ABRWorld* W)
{
	if (!C->HasCamcorderInHand())
	{
		return;
	}
	const float U = Ui();
	UFont* Medium = GEngine->GetMediumFont();
	const float X = 40.f * U;
	const float Y = 34.f * U;
	const bool bNV = C->IsNightVision();
	const FLinearColor TextC = bNV ? FLinearColor(0.75f, 1.f, 0.75f, 0.9f) : FLinearColor(1.f, 1.f, 1.f, 0.85f);
	if (!FBRSettings::Get().bVHSEffect)
	{
		// Ecran normal : seulement les informations de jeu (tache d'enregistrement, vision nocturne)
		if (W && !W->GetRecordLabel().IsEmpty())
		{
			Txt(TEXT("ENREGISTREMENT : ") + W->GetRecordLabel(), X, Y, Yellow, 0.8f * U, Medium);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X, Y + 26.f * U, 220.f * U, 6.f * U);
			DrawRect(Yellow, X, Y + 26.f * U, 220.f * U * W->GetRecordProgress(), 6.f * U);
		}
		if (bNV)
		{
			TxtRight(TEXT("VISION NOCTURNE"), Canvas->ClipX - 40.f * U, Y, FLinearColor(0.5f, 1.f, 0.5f, 0.9f), 0.8f * U, Medium);
		}
		return;
	}
	if (FMath::Fmod(Clock, 1.4f) < 0.9f)
	{
		DrawRect(FLinearColor(0.9f, 0.05f, 0.05f, 0.9f), X, Y + 5.f * U, 14.f * U, 14.f * U);
	}
	Txt(TEXT("REC"), X + 22.f * U, Y, TextC, 0.95f * U, Medium);
	if (W)
	{
		Txt(Timecode(W->GetLevelTime()), X + 80.f * U, Y, WithAlpha(TextC, 0.85f), 0.95f * U, Medium);
		TxtRight(FString::Printf(TEXT("NIVEAU %d"), W->GetLevelNumber()), Canvas->ClipX - 40.f * U, Y, WithAlpha(TextC, 0.85f), 0.95f * U, Medium);

		// Tache d'enregistrement en cours
		if (!W->GetRecordLabel().IsEmpty())
		{
			const float RY = Y + 34.f * U;
			Txt(TEXT("ENREGISTREMENT : ") + W->GetRecordLabel(), X, RY, Yellow, 0.8f * U, Medium);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X, RY + 26.f * U, 220.f * U, 6.f * U);
			DrawRect(Yellow, X, RY + 26.f * U, 220.f * U * W->GetRecordProgress(), 6.f * U);
		}
	}
	// Batterie du camescope
	const float BW = 46.f * U;
	const float BH = 20.f * U;
	const float BX = Canvas->ClipX - 40.f * U - BW;
	const float BY = Y + 36.f * U;
	Frame(BX, BY, BW, BH, WithAlpha(TextC, 0.8f), 2.f * U);
	DrawRect(WithAlpha(TextC, 0.8f), BX + BW, BY + BH * 0.3f, 4.f * U, BH * 0.4f);
	const float Fill = FMath::Clamp(C->Battery / 100.f, 0.f, 1.f);
	DrawRect(Fill < 0.2f ? Danger : WithAlpha(TextC, 0.8f), BX + 4.f * U, BY + 4.f * U, (BW - 8.f * U) * Fill, BH - 8.f * U);
	if (bNV)
	{
		TxtRight(TEXT("VISION NOCTURNE"), BX - 12.f * U, BY - 2.f * U, FLinearColor(0.5f, 1.f, 0.5f, 0.9f), 0.8f * U, Medium);
	}

	// Coins du viseur
	Corners(22.f * U, 22.f * U, Canvas->ClipX - 44.f * U, Canvas->ClipY - 44.f * U, 40.f * U, FLinearColor(1.f, 1.f, 1.f, 0.35f), 3.f * U);
	if (bNV)
	{
		Scanlines(0.05f);
	}
}

void ABRHUD::DrawStats(ABRCharacter* C)
{
	const float U = Ui();
	const float X = 50.f * U;
	const float BW = 210.f * U;
	const float BH = 6.f * U;
	float Y = Canvas->ClipY - 150.f * U;
	// Oxygene : seulement sous l'eau (et le temps de reprendre son souffle)
	if (C->IsUnderwater() || C->GetBreath() < 99.5f)
	{
		const float Low = C->GetBreath() < 30.f ? 0.55f + 0.45f * FMath::Sin(Clock * 8.f) : 1.f;
		Bar(X, Y - 36.f * U, BW, BH, C->GetBreath() / 100.f, FLinearColor(0.35f * Low, 0.85f * Low, 1.f * Low, 0.9f), TEXT("OXYG\u00c8NE"));
	}
	Bar(X, Y, BW, BH, C->Health / 100.f, FLinearColor(0.85f, 0.15f, 0.12f, 0.85f), TEXT("SANT\u00c9"));
	Y += 36.f * U;
	const float Pulse = C->Sanity < 30.f ? 0.6f + 0.4f * FMath::Sin(Clock * 6.f) : 1.f;
	Bar(X, Y, BW, BH, C->Sanity / 100.f, FLinearColor(0.9f * Pulse, 0.45f * Pulse, 0.3f * Pulse, 0.85f), TEXT("SANT\u00c9 MENTALE"));
	Y += 36.f * U;
	Bar(X, Y, BW, BH, C->Stamina / 100.f, FLinearColor(0.9f, 0.9f, 0.85f, 0.75f), TEXT("ENDURANCE"));
	// Piles : la batterie du camescope s'affiche dans le viseur, sauf si l'effet camescope est desactive
	if ((C->HasLightSource() && !C->HasCamcorderInHand()) || (C->HasCamcorderInHand() && !FBRSettings::Get().bVHSEffect))
	{
		Y += 36.f * U;
		Bar(X, Y, BW, BH, C->Battery / 100.f, FLinearColor(1.f, 0.85f, 0.3f, C->IsFlashlightOn() ? 0.9f : 0.45f), TEXT("PILES"));
	}
}

void ABRHUD::DrawQuickBar(ABRCharacter* C)
{
	const float U = Ui();
	const float S = 58.f * U;
	const float G = 8.f * U;
	const float Total = S * ABRCharacter::NumPockets + G * (ABRCharacter::NumPockets - 1);
	const float X0 = (Canvas->ClipX - Total) * 0.5f;
	const float Y = Canvas->ClipY - S - 34.f * U;
	UFont* Small = GEngine->GetSmallFont();
	for (int32 i = 0; i < ABRCharacter::NumPockets; ++i)
	{
		const float X = X0 + i * (S + G);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), X, Y, S, S);
		Frame(X, Y, S, S, FLinearColor(0.95f, 0.78f, 0.25f, 0.3f), 1.f * U);
		Txt(FString::FromInt(i + 1), X + 4.f * U, Y + 2.f * U, InkDim, 0.7f * U, Small);
		const FBRItemSlot* It = C->Pockets.IsValidIndex(i) ? &C->Pockets[i] : nullptr;
		if (It && !It->IsEmpty())
		{
			Icon(ItemIcon(It->Item), X + S * 0.12f, Y + S * 0.12f, S * 0.76f, S * 0.76f, FLinearColor(1.f, 1.f, 1.f, 0.9f));
			if (It->Count > 1)
			{
				TxtRight(FString::Printf(TEXT("x%d"), It->Count), X + S - 4.f * U, Y + S - 18.f * U, Ink, 0.7f * U, Small);
			}
		}
	}
}

void ABRHUD::DrawObjectiveTracker(ABRWorld* W)
{
	if (!W || W->GetTitleTime() > 0.f || W->IsTransitioning())
	{
		return;
	}
	TArray<FBRObjective> Objs;
	W->GetObjectives(Objs);
	const float U = Ui();
	UFont* Small = GEngine->GetSmallFont();
	const float RX = Canvas->ClipX - 44.f * U;
	float Y = 110.f * U;
	for (const FBRObjective& O : Objs)
	{
		if (!O.bRequired && O.IsDone())
		{
			continue;
		}
		const FLinearColor Col = O.IsDone() ? Done : (O.bRequired ? WithAlpha(Yellow, 0.9f) : WithAlpha(InkDim, 0.75f));
		const FString Line = O.Goal > 0 ? FString::Printf(TEXT("%s  %d/%d"), *O.Text, O.Progress, O.Goal) : O.Text;
		TxtRight(Line, RX, Y, Col, 0.75f * U, Small);
		Y += 22.f * U;
	}
}

void ABRHUD::DrawCrosshair(ABRCharacter* C)
{
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const bool bFocus = !C->GetFocusPrompt().IsEmpty();
	const float S = (bFocus ? 6.f : 3.f) * U;
	if (C->IsHidden())
	{
		const float Pulse = 0.65f + 0.2f * FMath::Sin(Clock * 2.f);
		Txt(TEXT("CACH\u00c9"), CX, Canvas->ClipY - 150.f * U, FLinearColor(0.75f, 0.9f, 1.f, Pulse), 1.1f * U, GEngine->GetMediumFont(), true);
	}
	DrawRect(FLinearColor(1.f, 1.f, 1.f, bFocus ? 0.9f : 0.45f), CX - S * 0.5f, CY - S * 0.5f, S, S);
	if (bFocus)
	{
		Txt(C->GetFocusPrompt(), CX, CY + 30.f * U, FLinearColor(1.f, 1.f, 1.f, 0.95f), 1.f * U, GEngine->GetMediumFont(), true);
	}
	if (C->GetReviveProgress() > 0.f)
	{
		Bar(CX - 130.f * U, CY + 70.f * U, 260.f * U, 8.f * U, C->GetReviveProgress(), FLinearColor(0.55f, 1.f, 0.55f, 0.9f), TEXT("R\u00c9ANIMATION"));
	}
}

void ABRHUD::DrawMessages(float Dt)
{
	const float U = Ui();
	float Y = Canvas->ClipY * 0.1f;
	for (int32 i = Messages.Num() - 1; i >= 0; --i)
	{
		Messages[i].Age += Dt;
		if (Messages[i].Age > Messages[i].Duration)
		{
			Messages.RemoveAt(i);
		}
	}
	for (const FMsg& M : Messages)
	{
		const float A = FMath::Clamp(M.Duration - M.Age, 0.f, 1.f) * FMath::Clamp(M.Age * 4.f, 0.f, 1.f);
		FLinearColor C = M.Color;
		C.A = A;
		Txt(M.Text, Canvas->ClipX * 0.5f, Y, C, 1.f * U, GEngine->GetMediumFont(), true);
		Y += 30.f * U;
	}
}

void ABRHUD::DrawNote(ABRCharacter* C)
{
	const float U = Ui();
	const float W = 760.f * U;
	const float H = 460.f * U;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = (Canvas->ClipY - H) * 0.5f;
	UFont* Medium = GEngine->GetMediumFont();
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	DrawRect(FLinearColor(0.86f, 0.83f, 0.71f, 0.97f), X, Y, W, H);
	DrawRect(FLinearColor(0.75f, 0.68f, 0.5f, 0.6f), X, Y, W, 6.f * U);
	Txt(TEXT("Une note froiss\u00e9e..."), X + 40.f * U, Y + 30.f * U, FLinearColor(0.25f, 0.18f, 0.1f), 1.1f * U, Medium, false, false);
	float LY = Y + 90.f * U;
	for (const FString& L : Wrap(BRKeys::Expand(C->GetOpenNote()), W - 80.f * U, Medium, 1.05f * U))
	{
		Txt(L, X + 40.f * U, LY, FLinearColor(0.12f, 0.1f, 0.25f), 1.05f * U, Medium, false, false);
		LY += 34.f * U;
	}
	Txt(BRKeys::Expand(TEXT("{Interact} Ranger la note  (elle reste dans le journal : {Inventory})")), X + W * 0.5f, Y + H - 50.f * U, FLinearColor(0.3f, 0.25f, 0.2f), 0.9f * U,
		Medium, true, false);
}

void ABRHUD::DrawDeath(ABRCharacter* C)
{
	const float T = C->GetDeathTime();
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	const float H = Canvas->ClipY;
	DrawRect(FLinearColor(0.15f, 0.f, 0.f, FMath::Clamp(T / 2.5f, 0.f, 0.85f)), 0.f, 0.f, Canvas->ClipX, H);
	if (T < 0.25f)
	{
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.6f * (1.f - T / 0.25f)), 0.f, 0.f, Canvas->ClipX, H);
	}
	const float A = FMath::Clamp((T - 0.8f) / 1.f, 0.f, 1.f);
	const ABRPlayerController* OwnerPC = Cast<ABRPlayerController>(PlayerOwner);
	if (OwnerPC && OwnerPC->IsNetGame())
	{
		// Cooperation : a terre, un coequipier peut nous relever
		const ABRWorld* W = ABRWorld::Get(this);
		const float Left = W ? FMath::Max(0.f, W->GetDeathTimer()) : 0.f;
		const bool bHelp = W && W->HasLivingTeammate();
		Txt(TEXT("\u00c0 TERRE"), CX, H * 0.34f, FLinearColor(0.9f, 0.1f, 0.08f, A), 2.4f * U, GEngine->GetLargeFont(), true);
		if (!C->GetKilledBy().IsEmpty())
		{
			Txt(TEXT("Abattu par : ") + C->GetKilledBy(), CX, H * 0.44f, FLinearColor(1.f, 0.8f, 0.8f, A), 1.1f * U, GEngine->GetMediumFont(), true);
		}
		if (bHelp)
		{
			Txt(FString::Printf(TEXT("Un co\u00e9quipier peut vous relever : il doit maintenir %s pr\u00e8s de vous."), *BRKeys::Tag(EBRAction::Interact)),
				CX, H * 0.52f, FLinearColor(0.75f, 1.f, 0.75f, A), 1.f * U, GEngine->GetMediumFont(), true);
		}
		Txt(FString::Printf(TEXT("R\u00e9veil au point de d\u00e9part du niveau dans %d s      %s  abandonner"), FMath::CeilToInt(Left),
			*BRKeys::Tag(EBRAction::Jump)), CX, H * 0.58f, FLinearColor(0.9f, 0.85f, 0.6f, A), 1.f * U, GEngine->GetMediumFont(), true);
		return;
	}
	Txt(TEXT("VOUS \u00caTES MORT"), CX, H * 0.38f, FLinearColor(0.9f, 0.1f, 0.08f, A), 2.4f * U, GEngine->GetLargeFont(), true);
	if (!C->GetKilledBy().IsEmpty())
	{
		Txt(TEXT("Tu\u00e9 par : ") + C->GetKilledBy(), CX, H * 0.48f, FLinearColor(1.f, 0.8f, 0.8f, A), 1.1f * U, GEngine->GetMediumFont(), true);
	}
	const float A2 = FMath::Clamp((T - 2.2f) / 1.f, 0.f, 1.f);
	const ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	const bool bNet = PC && PC->IsNetGame();
	Txt(bNet ? TEXT("Vous allez vous r\u00e9veiller au point de d\u00e9part du niveau... vos co\u00e9quipiers continuent sans vous.")
			 : TEXT("Vous vous r\u00e9veillez... sur une moquette humide. Encore."),
		CX, H * 0.56f, FLinearColor(0.9f, 0.85f, 0.6f, A2), 1.f * U, GEngine->GetMediumFont(), true);
}

void ABRHUD::DrawPause(ABRPlayerController* PC)
{
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	const float H = Canvas->ClipY;
	UFont* Medium = GEngine->GetMediumFont();
	if (PlayerOwner)
	{
		PlayerOwner->GetMousePosition(MouseX, MouseY);
	}
	Buttons.Reset();
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), 0.f, 0.f, Canvas->ClipX, H);
	Scanlines(0.05f);
	Txt(TEXT("PAUSE"), CX, H * 0.14f, Yellow, 2.2f * U, GEngine->GetLargeFont(), true);
	if (PC && PC->IsNetGame())
	{
		const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		const int32 Count = GS ? GS->PlayerArray.Num() : 1;
		Txt(FString::Printf(TEXT("PARTIE EN LIGNE  -  %d joueur%s  -  le jeu continue pendant la pause"), Count, Count > 1 ? TEXT("s") : TEXT("")),
			CX, H * 0.235f, FLinearColor(0.75f, 0.95f, 0.75f), 0.9f * U, Medium, true);
		DrawPlayerList();
	}

	// Boutons cliquables
	const TCHAR* Labels[] = { TEXT("REPRENDRE"), TEXT("PARAM\u00c8TRES / GRAPHISMES"), TEXT("TOUCHES"), TEXT("MENU PRINCIPAL"), TEXT("QUITTER LE JEU") };
	const int32 Ids[] = { Btn_PauseResume, Btn_PauseSettings, Btn_PauseKeys, Btn_PauseMainMenu, Btn_PauseQuit };
	const float BW = 460.f * U;
	const float BH = 50.f * U;
	float Y = H * 0.29f;
	for (int32 i = 0; i < 5; ++i)
	{
		const float X = CX - BW * 0.5f;
		const bool bHov = Hover(X, Y, BW, BH);
		DrawRect(bHov ? WithAlpha(Yellow, 0.9f) : FLinearColor(0.f, 0.f, 0.f, 0.55f), X, Y, BW, BH);
		Frame(X, Y, BW, BH, bHov ? Yellow : YellowDim, 1.f * U);
		Txt(Labels[i], CX, Y + 10.f * U, bHov ? FLinearColor(0.05f, 0.04f, 0.01f) : Ink, 1.f * U, Medium, true, false);
		AddButton(Ids[i], X, Y, BW, BH);
		Y += BH + 14.f * U;
	}
	HandlePauseMouse(PC);

	Txt(ControlsLine(0), CX, H * 0.72f, FLinearColor(0.85f, 0.85f, 0.85f), 0.85f * U, Medium, true);
	Txt(ControlsLine(1), CX, H * 0.755f, FLinearColor(0.85f, 0.85f, 0.85f), 0.85f * U, Medium, true);
	Txt(FString::Printf(TEXT("%s  reprendre     %s  param\u00e8tres et touches     [ FIN ]  quitter"), *BRKeys::Tag(EBRAction::Pause),
		*BRKeys::Tag(EBRAction::Inventory)), CX, H * 0.81f, FLinearColor(1.f, 0.92f, 0.6f), 0.9f * U, Medium, true);
	Txt(TEXT("Console (touche \u00b2) : BRLevel 37  |  BRGod  |  BRSpawn 0-8  |  BRGiveAll  |  BRBlackout  |  BRObjectives"), CX,
		H * 0.88f, FLinearColor(0.6f, 0.6f, 0.6f), 0.8f * U, Medium, true);
}

void ABRHUD::HandlePauseMouse(ABRPlayerController* PC)
{
	if (!PC || !PlayerOwner || !PlayerOwner->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		return;
	}
	switch (ButtonAt(MouseX, MouseY))
	{
	case Btn_PauseResume:
		PC->TogglePause();
		break;
	case Btn_PauseSettings:
		PC->SetInventoryOpen(true, static_cast<int32>(ETab::Settings));
		break;
	case Btn_PauseKeys:
		PC->SetInventoryOpen(true, static_cast<int32>(ETab::Keys));
		break;
	case Btn_PauseMainMenu:
		PC->ReturnToMainMenu();
		break;
	case Btn_PauseQuit:
		PC->QuitToDesktop();
		break;
	default:
		break;
	}
}

void ABRHUD::DrawKeysTab(ABRPlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	const float U = Ui();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float W = FMath::Min(IW, 1240.f * U);
	const float X = IX + (IW - W) * 0.5f;
	Panel(X, IY, W, IH, TEXT("TOUCHES"));

	const int32 Count = BRKeys::NumActions();
	const float Top = IY + 56.f * U;
	const float RowH = FMath::Min(40.f * U, (IH - 140.f * U) / FMath::Max(1, Count));
	const float SlotW = 170.f * U;
	const float SlotGap = 12.f * U;
	const float SlotsX = X + W - 24.f * U - 3.f * SlotW - 2.f * SlotGap;
	// En-tetes de colonnes
	const TCHAR* Heads[] = { TEXT("TOUCHE 1"), TEXT("TOUCHE 2"), TEXT("TOUCHE 3") };
	for (int32 k = 0; k < 3; ++k)
	{
		Txt(Heads[k], SlotsX + k * (SlotW + SlotGap) + SlotW * 0.5f, Top - 4.f * U, InkDim, 0.65f * U, Small, true, false);
	}
	float Y = Top + 18.f * U;
	const bool bBlink = FMath::Fmod(Clock, 0.8f) < 0.5f;
	for (int32 A = 0; A < Count; ++A)
	{
		const EBRAction Act = static_cast<EBRAction>(A);
		const bool bRowHov = Hover(X + 10.f * U, Y, W - 20.f * U, RowH - 4.f * U);
		if (bRowHov)
		{
			DrawRect(FLinearColor(1.f, 0.85f, 0.3f, 0.05f), X + 10.f * U, Y, W - 20.f * U, RowH - 4.f * U);
		}
		Txt(BRKeys::ActionLabel(Act), X + 28.f * U, Y + (RowH - 4.f * U) * 0.5f - 10.f * U, bRowHov ? Yellow : Ink, 0.78f * U, Medium, false, false);
		for (int32 Slot = 0; Slot < BRKeys::SlotsPerAction; ++Slot)
		{
			const float SX = SlotsX + Slot * (SlotW + SlotGap);
			const float SY = Y + 2.f * U;
			const float SH = RowH - 8.f * U;
			const bool bCap = PC->IsCapturingKey() && PC->GetCaptureAction() == A && PC->GetCaptureSlot() == Slot;
			const bool bHov = Hover(SX, SY, SlotW, SH);
			DrawRect(bCap ? WithAlpha(Yellow, bBlink ? 0.85f : 0.45f) : FLinearColor(0.f, 0.f, 0.f, 0.5f), SX, SY, SlotW, SH);
			Frame(SX, SY, SlotW, SH, bHov || bCap ? Yellow : YellowDim, 1.f * U);
			const FKey K = BRKeys::GetKey(Act, Slot);
			const FString Label = bCap ? FString(TEXT("APPUYEZ...")) : BRKeys::KeyName(K);
			const FLinearColor Col = bCap ? FLinearColor(0.05f, 0.04f, 0.01f) : (K.IsValid() ? Ink : WithAlpha(InkDim, 0.5f));
			float LS = 0.72f * U;
			const float LW = TextW(Label, Small, LS);
			if (LW > SlotW - 12.f * U)
			{
				LS *= (SlotW - 12.f * U) / LW;
			}
			Txt(Label, SX + SlotW * 0.5f, SY + SH * 0.5f - 9.f * U, Col, LS, Small, true, false);
			AddButton(Btn_KeySlot + A * BRKeys::SlotsPerAction + Slot, SX, SY, SlotW, SH);
		}
		DrawRect(FLinearColor(0.95f, 0.78f, 0.25f, 0.1f), X + 18.f * U, Y + RowH - 3.f * U, W - 36.f * U, 1.f * U);
		Y += RowH;
	}

	// Bouton de reinitialisation + aide
	const FString ResetLabel(TEXT("TOUCHES PAR D\u00c9FAUT"));
	const float RW = TextW(ResetLabel, Medium, 0.8f * U) + 40.f * U;
	const float RH = 36.f * U;
	const float RX = X + 24.f * U;
	const float RY = IY + IH - RH - 16.f * U;
	const bool bHovR = Hover(RX, RY, RW, RH);
	DrawRect(bHovR ? WithAlpha(Yellow, 0.9f) : FLinearColor(0.f, 0.f, 0.f, 0.5f), RX, RY, RW, RH);
	Frame(RX, RY, RW, RH, Yellow, 1.f * U);
	Txt(ResetLabel, RX + RW * 0.5f, RY + 6.f * U, bHovR ? FLinearColor(0.05f, 0.04f, 0.01f) : Yellow, 0.8f * U, Medium, true, false);
	AddButton(Btn_KeysReset, RX, RY, RW, RH);
	const FString Help = PC->IsCapturingKey()
		? FString(TEXT("Appuyez sur la nouvelle touche (clavier ou bouton de souris).  \u00c9chap : annuler   -   Retour arri\u00e8re : effacer"))
		: FString(TEXT("Cliquez sur une case puis appuyez sur une touche. Clic droit : effacer. Les changements s'appliquent imm\u00e9diatement."));
	Txt(Help, RX + RW + 24.f * U, RY + 8.f * U, PC->IsCapturingKey() ? Yellow : InkDim, 0.72f * U, Small, false, false);
}

void ABRHUD::DrawGlitch(float Amount)
{
	const int32 Count = FMath::RoundToInt(Amount * 16.f);
	for (int32 i = 0; i < Count; ++i)
	{
		const float Y = FMath::FRandRange(0.f, Canvas->ClipY);
		const float H = FMath::FRandRange(2.f, 30.f) * Ui();
		const float X = FMath::FRandRange(-200.f, Canvas->ClipX * 0.5f);
		const float W = FMath::FRandRange(Canvas->ClipX * 0.2f, Canvas->ClipX * 1.2f);
		const FLinearColor C(FMath::FRand(), FMath::FRand() * 0.6f, FMath::FRand(), Amount * 0.45f);
		DrawRect(C, X, Y, W, H);
	}
}

// =====================================================================================================================
// Inventaire (TAB)
// =====================================================================================================================

void ABRHUD::DrawInventory(ABRPlayerController* PC, ABRCharacter* C, ABRWorld* W)
{
	const float U = Ui();
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();

	if (PlayerOwner)
	{
		PlayerOwner->GetMousePosition(MouseX, MouseY);
	}
	Buttons.Reset();
	Slots.Reset();

	// Fond sombre teinte de jaune + lignes de balayage
	DrawRect(FLinearColor(0.025f, 0.022f, 0.006f, 0.84f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	Scanlines(0.07f);

	IW = FMath::Min(Canvas->ClipX - 120.f * U, 1760.f * U);
	IX = (Canvas->ClipX - IW) * 0.5f;
	IY = 165.f * U;
	IH = Canvas->ClipY - IY - 115.f * U;
	const float Gap = IW * 0.02f;
	ColW[0] = IW * 0.27f;
	ColW[1] = IW * 0.42f;
	ColW[2] = IW * 0.27f;
	ColX[0] = IX;
	ColX[1] = ColX[0] + ColW[0] + Gap;
	ColX[2] = ColX[1] + ColW[1] + Gap;

	// En-tete "MENU >" et onglets
	Txt(TEXT("MENU >"), IX, 44.f * U, Yellow, 1.25f * U, Large, false, false);
	DrawRect(YellowDim, IX, 92.f * U, IW, 1.f * U);
	if (PC)
	{
		const int32 Wanted = PC->ConsumeRequestedTab();
		if (Wanted >= 0 && Wanted <= 3)
		{
			Tab = static_cast<ETab>(Wanted);
		}
	}
	if (!C && (Tab == ETab::Character || Tab == ETab::Journal))
	{
		Tab = ETab::Settings;
	}
	const TCHAR* TabNames[] = { TEXT("PERSONNAGE"), TEXT("JOURNAL"), TEXT("PARAM\u00c8TRES"), TEXT("TOUCHES") };
	float TX = IX;
	for (int32 i = 0; i < 4; ++i)
	{
		const bool bSel = static_cast<int32>(Tab) == i;
		const FString Label = bSel ? FString(TEXT("> ")) + TabNames[i] : FString(TabNames[i]);
		const float LW = TextW(Label, Medium, 0.95f * U);
		const bool bHov = Hover(TX - 6.f * U, 104.f * U, LW + 12.f * U, 34.f * U);
		Txt(Label, TX, 106.f * U, bSel ? Yellow : (bHov ? Ink : InkDim), 0.95f * U, Medium, false, false);
		if (bSel)
		{
			DrawRect(Yellow, TX, 138.f * U, LW, 2.f * U);
		}
		AddButton(Btn_TabCharacter + i, TX - 6.f * U, 104.f * U, LW + 12.f * U, 34.f * U);
		TX += LW + 48.f * U;
	}
	if (W)
	{
		TxtRight(FString::Printf(TEXT("NIVEAU %d  \u00ab %s \u00bb"), W->GetLevelNumber(), *W->Def().Title), IX + IW, 106.f * U, InkDim, 0.85f * U, Medium);
	}

	switch (Tab)
	{
	case ETab::Character:
		LayoutCharacterTab(C);
		DrawCharacterTab(C, W);
		break;
	case ETab::Journal:
		DrawJournalTab(C, W);
		break;
	case ETab::Settings:
		DrawSettingsTab(PC);
		break;
	case ETab::Keys:
		DrawKeysTab(PC);
		break;
	}

	if (Inspecting.IsValid())
	{
		DrawInspect(C);
	}
	HandleInventoryMouse(PC, C);

	// Pied de page
	const float FY = Canvas->ClipY - 70.f * U;
	DrawRect(YellowDim, IX, FY - 14.f * U, IW, 1.f * U);
	Txt(TEXT("\u00a9 1992 THRESHOLD SYSTEMS"), IX, FY, InkDim, 0.8f * U, Small, false, false);
	if (Tab == ETab::Character)
	{
		TxtRight(BRKeys::Expand(TEXT("[GLISSER]  D\u00c9PLACER     [DOUBLE-CLIC]  UTILISER / \u00c9QUIPER     [CLIC DROIT]  INSPECTER     {Inventory}  FERMER")), IX + IW, FY,
			InkDim, 0.8f * U, Small);
	}
	else
	{
		TxtRight(Tab == ETab::Keys ? FString(TEXT("[CLIC]  CHANGER     [CLIC DROIT]  EFFACER     [\u00c9CHAP]  ANNULER / FERMER"))
			: FString(TEXT("[CLIC]  CHOISIR     [\u00c9CHAP]  FERMER")), IX + IW, FY, InkDim, 0.8f * U, Small);
	}

	// Infobulle et objet en cours de deplacement (au-dessus de tout)
	if (Tab == ETab::Character && !Inspecting.IsValid())
	{
		DrawTooltip(C);
	}
	if (Dragging.IsValid() && C)
	{
		if (const FBRItemSlot* It = C->GetSlot(Dragging.Group, Dragging.Index))
		{
			if (!It->IsEmpty())
			{
				const float S = 84.f * U;
				Icon(ItemIcon(It->Item), MouseX - S * 0.5f, MouseY - S * 0.5f, S, S, FLinearColor(1.f, 1.f, 1.f, 0.85f));
			}
		}
	}
}

void ABRHUD::LayoutCharacterTab(ABRCharacter* C)
{
	const float U = Ui();
	const float G = 10.f * U;

	// Colonne du milieu : poches (4) + stockage (5 x 4)
	const float InnerX = ColX[1] + 52.f * U;
	const float InnerW = ColW[1] - 70.f * U;
	const float Avail = IH - 60.f * U - 34.f * U - 40.f * U - 3.f * G - 30.f * U;
	const float S = FMath::Min((InnerW - 4.f * G) / 5.f, Avail / 5.f);
	float Y = IY + 64.f * U;
	for (int32 i = 0; i < ABRCharacter::NumPockets; ++i)
	{
		FSlotBox B;
		B.Ref.Group = EBRSlotGroup::Pockets;
		B.Ref.Index = i;
		B.X = InnerX + i * (S + G);
		B.Y = Y;
		B.S = S;
		B.Caption = FString::FromInt(i + 1);
		Slots.Add(B);
	}
	Y += S + 40.f * U;
	for (int32 i = 0; i < ABRCharacter::NumStorage; ++i)
	{
		FSlotBox B;
		B.Ref.Group = EBRSlotGroup::Storage;
		B.Ref.Index = i;
		B.X = InnerX + (i % 5) * (S + G);
		B.Y = Y + (i / 5) * (S + G);
		B.S = S;
		Slots.Add(B);
	}

	// Colonne de droite : silhouette + 4 emplacements d'equipement
	const float EqS = FMath::Min(S * 0.9f, 104.f * U);
	const float Margin = 16.f * U;
	SilH = FMath::Min(IH - 90.f * U, (ColW[2] - 2.f * EqS - 4.f * Margin) * 2.f);
	SilW = SilH * 0.5f;
	SilX = ColX[2] + (ColW[2] - SilW) * 0.5f;
	SilY = IY + 60.f * U + (IH - 60.f * U - SilH) * 0.5f;
	const float LeftX = ColX[2] + Margin;
	const float RightX = ColX[2] + ColW[2] - EqS - Margin;
	struct FEq
	{
		EBREquipSlot Slot;
		float X;
		float Y;
	};
	const FEq Eqs[] = {
		{ EBREquipSlot::Head, LeftX, SilY + SilH * 0.02f },
		{ EBREquipSlot::Chest, RightX, SilY + SilH * 0.2f },
		{ EBREquipSlot::Hand, LeftX, SilY + SilH * 0.44f },
		{ EBREquipSlot::Belt, RightX, SilY + SilH * 0.5f },
	};
	for (const FEq& E : Eqs)
	{
		FSlotBox B;
		B.Ref.Group = EBRSlotGroup::Equipment;
		B.Ref.Index = static_cast<int32>(E.Slot);
		B.X = E.X;
		B.Y = E.Y;
		B.S = EqS;
		B.Caption = BRItems::SlotName(E.Slot);
		Slots.Add(B);
	}
	(void)C;
}

void ABRHUD::DrawSlot(ABRCharacter* C, const FSlotBox& Box)
{
	const float U = Ui();
	UFont* Small = GEngine->GetSmallFont();
	const FBRItemSlot* It = C ? C->GetSlot(Box.Ref.Group, Box.Ref.Index) : nullptr;
	const bool bHov = HoverSlot == Box.Ref && !Inspecting.IsValid();
	const bool bSrc = Dragging == Box.Ref;
	const bool bTarget = Dragging.IsValid() && bHov && !bSrc;
	const float X = Box.X;
	const float Y = Box.Y;
	const float S = Box.S;

	DrawRect(SlotBg, X, Y, S, S);
	DrawRect(FLinearColor(1.f, 0.85f, 0.3f, bHov ? 0.08f : 0.035f), X, Y, S, S * 0.45f);
	Frame(X, Y, S, S, bTarget ? Yellow : (bHov ? WithAlpha(Yellow, 0.85f) : YellowDim), (bHov ? 2.f : 1.f) * U);

	const bool bEmpty = !It || It->IsEmpty();
	if (Box.Ref.Group == EBRSlotGroup::Pockets)
	{
		Txt(Box.Caption, X + 5.f * U, Y + S - 20.f * U, InkDim, 0.7f * U, Small, false, false);
	}
	if (Box.Ref.Group == EBRSlotGroup::Equipment)
	{
		Txt(Box.Caption, X, Y - 22.f * U, Yellow, 0.75f * U, Small, false, false);
		if (bEmpty)
		{
			Txt(TEXT("VIDE"), X + S * 0.5f, Y + S * 0.5f - 9.f * U, WithAlpha(InkDim, 0.5f), 0.7f * U, Small, true, false);
		}
	}
	if (bEmpty)
	{
		return;
	}
	const FBRItemInfo& Info = BRItems::Get(It->Item);
	const float A = bSrc ? 0.3f : 1.f;
	// Nom court au-dessus de l'icone (reduit s'il est trop long)
	float LS = 0.62f * U;
	const float LW = TextW(Info.Short, Small, LS);
	if (LW > S - 8.f * U)
	{
		LS *= (S - 8.f * U) / LW;
	}
	Txt(Info.Short, X + S * 0.5f, Y + 4.f * U, WithAlpha(Ink, A), LS, Small, true, false);
	UTexture* Tex = ItemIcon(It->Item);
	if (Tex)
	{
		Icon(Tex, X + S * 0.16f, Y + S * 0.22f, S * 0.68f, S * 0.68f, FLinearColor(1.f, 1.f, 1.f, A));
	}
	else
	{
		DrawRect(WithAlpha(YellowDim, A), X + S * 0.3f, Y + S * 0.35f, S * 0.4f, S * 0.4f);
	}
	if (It->Count > 1)
	{
		TxtRight(FString::Printf(TEXT("x%d"), It->Count), X + S - 6.f * U, Y + S - 20.f * U, WithAlpha(Ink, A), 0.75f * U, Small);
	}
}

void ABRHUD::DrawCharacterTab(ABRCharacter* C, ABRWorld* W)
{
	const float U = Ui();
	UFont* Small = GEngine->GetSmallFont();
	UFont* Medium = GEngine->GetMediumFont();

	// ---------------- Colonne gauche : OBJECTIFS + BIOMETRIE
	const float LX = ColX[0];
	const float LW = ColW[0];
	const float ObjH = IH * 0.44f;
	Panel(LX, IY, LW, ObjH, TEXT("OBJECTIFS"));
	float Y = IY + 58.f * U;
	if (W)
	{
		TArray<FBRObjective> Objs;
		W->GetObjectives(Objs);
		for (const FBRObjective& O : Objs)
		{
			const FLinearColor Col = O.IsDone() ? Done : (O.bRequired ? Ink : InkDim);
			const FString Count = O.Goal > 0 ? FString::Printf(TEXT("%d/%d"), O.Progress, O.Goal) : FString();
			const float CountW = TextW(Count, Small, 0.8f * U);
			float S = 0.78f * U;
			const float TW = TextW(O.Text, Small, S);
			if (TW > LW - 60.f * U - CountW)
			{
				S *= (LW - 60.f * U - CountW) / TW;
			}
			DrawRect(O.bRequired ? Yellow : YellowDim, LX + 18.f * U, Y + 6.f * U, 6.f * U, 6.f * U);
			Txt(O.Text, LX + 32.f * U, Y, Col, S, Small, false, false);
			TxtRight(Count, LX + LW - 18.f * U, Y, Col, 0.8f * U, Small);
			Y += 24.f * U;
			if (!O.IsDone() && O.Partial > 0.01f)
			{
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), LX + 32.f * U, Y, LW - 50.f * U, 4.f * U);
				DrawRect(Yellow, LX + 32.f * U, Y, (LW - 50.f * U) * O.Partial, 4.f * U);
				Y += 10.f * U;
			}
			Y += 8.f * U;
		}
		if (W->Def().bRequireObjectives)
		{
			const bool bOk = W->AreObjectivesComplete();
			Y = FMath::Max(Y, IY + ObjH - 60.f * U);
			for (const FString& L : Wrap(bOk ? TEXT("Les sorties sont stables : trouvez un mur qui gr\u00e9sille.")
				: TEXT("Objectifs requis pour stabiliser les sorties du niveau."), LW - 40.f * U, Small, 0.7f * U))
			{
				Txt(L, LX + 18.f * U, Y, bOk ? Done : InkDim, 0.7f * U, Small, false, false);
				Y += 18.f * U;
			}
		}
	}

	const float BioY = IY + ObjH + 18.f * U;
	const float BioH = IH - ObjH - 18.f * U;
	Panel(LX, BioY, LW, BioH, TEXT("BIOM\u00c9TRIE"));
	if (C)
	{
		struct FRow
		{
			const TCHAR* Label;
			float Value;
			float Trend;
			FLinearColor Color;
		};
		const float SanityK = FMath::Clamp(C->Sanity / 100.f, 0.f, 1.f);
		const float BatTrend = (C->IsFlashlightOn() || C->IsNightVision()) ? -1.f : 0.f;
		const FRow Rows[] = {
			{ TEXT("SANT\u00c9 MENTALE"), C->Sanity, C->GetSanityTrend(), FMath::Lerp(FLinearColor(0.9f, 0.18f, 0.12f), FLinearColor(0.95f, 0.55f, 0.35f), SanityK) },
			{ TEXT("SANT\u00c9"), C->Health, C->GetHealthTrend(), FLinearColor(0.85f, 0.2f, 0.16f) },
			{ TEXT("ENDURANCE"), C->Stamina, C->GetStaminaTrend(), FLinearColor(0.92f, 0.9f, 0.82f) },
			{ TEXT("PILES"), C->Battery, BatTrend, FLinearColor(1.f, 0.82f, 0.3f) },
		};
		const float Box = 26.f * U;
		const float BarW = LW - 36.f * U - Box - 12.f * U;
		float RY = BioY + 62.f * U;
		const float Step = FMath::Min(66.f * U, (BioH - 80.f * U) / 4.f);
		for (const FRow& R : Rows)
		{
			Txt(R.Label, LX + 18.f * U, RY, Ink, 0.75f * U, Small, false, false);
			TxtRight(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(R.Value)), LX + 18.f * U + BarW, RY, InkDim, 0.7f * U, Small);
			const float BY = RY + 22.f * U;
			const float BH = 12.f * U;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), LX + 18.f * U, BY, BarW, BH);
			DrawRect(R.Color, LX + 18.f * U, BY, BarW * FMath::Clamp(R.Value / 100.f, 0.f, 1.f), BH);
			for (int32 k = 1; k < 10; ++k)
			{
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), LX + 18.f * U + BarW * k / 10.f, BY, 2.f * U, BH);
			}
			TrendBox(LX + LW - 18.f * U - Box, BY + BH * 0.5f - Box * 0.5f, Box, R.Trend);
			RY += Step;
		}
	}

	// ---------------- Colonne du milieu : INVENTAIRE
	Panel(ColX[1], IY, ColW[1], IH, TEXT("INVENTAIRE"));
	const FSlotBox* FirstPocket = nullptr;
	const FSlotBox* FirstStorage = nullptr;
	for (const FSlotBox& B : Slots)
	{
		if (B.Ref.Group == EBRSlotGroup::Pockets && !FirstPocket)
		{
			FirstPocket = &B;
		}
		if (B.Ref.Group == EBRSlotGroup::Storage && !FirstStorage)
		{
			FirstStorage = &B;
		}
	}
	if (FirstPocket)
	{
		VLabel(TEXT("POCHES"), ColX[1] + 28.f * U, FirstPocket->Y, Yellow, 0.75f * U);
	}
	if (FirstStorage)
	{
		VLabel(TEXT("STOCKAGE"), ColX[1] + 28.f * U, FirstStorage->Y, Yellow, 0.75f * U);
	}

	// ---------------- Colonne de droite : EQUIPEMENT
	Panel(ColX[2], IY, ColW[2], IH, TEXT("\u00c9QUIPEMENT"));
	if (UBRAssets* A = UBRAssets::Get(this))
	{
		Icon(A->Icon(TEXT("I_Silhouette")), SilX, SilY, SilW, SilH, FLinearColor(0.85f, 0.82f, 0.72f, 0.95f));
	}
	// Traits reliant chaque emplacement au corps
	for (const FSlotBox& B : Slots)
	{
		if (B.Ref.Group != EBRSlotGroup::Equipment)
		{
			continue;
		}
		FVector2D Body(SilX + SilW * 0.5f, SilY + SilH * 0.1f);
		switch (static_cast<EBREquipSlot>(B.Ref.Index))
		{
		case EBREquipSlot::Chest:
			Body = FVector2D(SilX + SilW * 0.55f, SilY + SilH * 0.3f);
			break;
		case EBREquipSlot::Hand:
			Body = FVector2D(SilX + SilW * 0.16f, SilY + SilH * 0.5f);
			break;
		case EBREquipSlot::Belt:
			Body = FVector2D(SilX + SilW * 0.6f, SilY + SilH * 0.5f);
			break;
		default:
			break;
		}
		const bool bLeft = B.X < SilX;
		const float SX = bLeft ? B.X + B.S : B.X;
		const float SY = B.Y + B.S * 0.5f;
		DrawLine(SX, SY, Body.X, Body.Y, YellowDim, 1.5f * U);
		DrawRect(Yellow, Body.X - 3.f * U, Body.Y - 3.f * U, 6.f * U, 6.f * U);
	}

	for (const FSlotBox& B : Slots)
	{
		DrawSlot(C, B);
	}
	(void)Medium;
}

void ABRHUD::DrawTooltip(ABRCharacter* C)
{
	if (!C || !HoverSlot.IsValid() || Dragging.IsValid())
	{
		return;
	}
	const FBRItemSlot* It = C->GetSlot(HoverSlot.Group, HoverSlot.Index);
	if (!It || It->IsEmpty())
	{
		return;
	}
	const float U = Ui();
	UFont* Small = GEngine->GetSmallFont();
	UFont* Medium = GEngine->GetMediumFont();
	const FBRItemInfo& Info = BRItems::Get(It->Item);
	const float W = 380.f * U;
	const TArray<FString> Lines = Wrap(BRKeys::Expand(Info.Description), W - 28.f * U, Small, 0.72f * U);
	FString Hint;
	if (Info.bConsumable)
	{
		Hint = FString::Printf(TEXT("[DOUBLE-CLIC]  %s"), *Info.UseVerb.ToUpper());
	}
	else if (Info.Slot != EBREquipSlot::None)
	{
		Hint = HoverSlot.Group == EBRSlotGroup::Equipment ? TEXT("[DOUBLE-CLIC]  RETIRER") : TEXT("[DOUBLE-CLIC]  \u00c9QUIPER");
	}
	const float H = 52.f * U + Lines.Num() * 19.f * U + (Hint.IsEmpty() ? 0.f : 26.f * U);
	float X = MouseX + 20.f * U;
	float Y = MouseY + 20.f * U;
	X = FMath::Min(X, Canvas->ClipX - W - 10.f * U);
	Y = FMath::Min(Y, Canvas->ClipY - H - 10.f * U);
	DrawRect(FLinearColor(0.02f, 0.018f, 0.008f, 0.95f), X, Y, W, H);
	Frame(X, Y, W, H, YellowDim, 1.f * U);
	Txt(FString::Printf(TEXT("%s%s"), *Info.Name.ToUpper(), It->Count > 1 ? *FString::Printf(TEXT("  x%d"), It->Count) : TEXT("")), X + 14.f * U,
		Y + 10.f * U, Yellow, 0.85f * U, Medium, false, false);
	float LY = Y + 44.f * U;
	for (const FString& L : Lines)
	{
		Txt(L, X + 14.f * U, LY, Ink, 0.72f * U, Small, false, false);
		LY += 19.f * U;
	}
	if (!Hint.IsEmpty())
	{
		Txt(Hint, X + 14.f * U, LY + 6.f * U, InkDim, 0.68f * U, Small, false, false);
	}
}

void ABRHUD::DrawInspect(ABRCharacter* C)
{
	const FBRItemSlot* It = C ? C->GetSlot(Inspecting.Group, Inspecting.Index) : nullptr;
	if (!It || It->IsEmpty())
	{
		Inspecting = FSlotRef();
		return;
	}
	const float U = Ui();
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const FBRItemInfo& Info = BRItems::Get(It->Item);
	const float W = 760.f * U;
	const float H = 340.f * U;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = (Canvas->ClipY - H) * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	Panel(X, Y, W, H, TEXT("INSPECTER"));
	const float IconS = 220.f * U;
	DrawRect(SlotBg, X + 24.f * U, Y + 66.f * U, IconS, IconS);
	Frame(X + 24.f * U, Y + 66.f * U, IconS, IconS, YellowDim, 1.f * U);
	// Petit balancement de l'objet inspecte
	const float Bob = FMath::Sin(Clock * 1.7f) * 4.f * U;
	Icon(ItemIcon(It->Item), X + 34.f * U, Y + 76.f * U + Bob, IconS - 20.f * U, IconS - 20.f * U, FLinearColor::White);

	const float TX = X + IconS + 50.f * U;
	const float TW = W - IconS - 74.f * U;
	Txt(Info.Name.ToUpper(), TX, Y + 64.f * U, Yellow, 1.05f * U, Large, false, false);
	FString Kind = Info.bConsumable ? TEXT("CONSOMMABLE") : TEXT("OBJET");
	if (Info.Slot != EBREquipSlot::None)
	{
		Kind = TEXT("\u00c9QUIPEMENT - ") + BRItems::SlotName(Info.Slot);
	}
	Txt(FString::Printf(TEXT("%s     QUANTIT\u00c9 : %d"), *Kind, It->Count), TX, Y + 112.f * U, InkDim, 0.75f * U, Small, false, false);
	float LY = Y + 146.f * U;
	for (const FString& L : Wrap(BRKeys::Expand(Info.Description), TW, Medium, 0.8f * U))
	{
		Txt(L, TX, LY, Ink, 0.8f * U, Medium, false, false);
		LY += 24.f * U;
	}

	// Boutons
	const float BH = 38.f * U;
	const float BY = Y + H - BH - 20.f * U;
	FString UseLabel;
	if (Info.bConsumable)
	{
		UseLabel = Info.UseVerb.ToUpper();
	}
	else if (Info.Slot != EBREquipSlot::None)
	{
		UseLabel = Inspecting.Group == EBRSlotGroup::Equipment ? TEXT("RETIRER") : TEXT("\u00c9QUIPER");
	}
	float BX = TX;
	if (!UseLabel.IsEmpty())
	{
		const float BW = TextW(UseLabel, Medium, 0.85f * U) + 40.f * U;
		const bool bHov = Hover(BX, BY, BW, BH);
		DrawRect(bHov ? WithAlpha(Yellow, 0.9f) : FLinearColor(0.f, 0.f, 0.f, 0.5f), BX, BY, BW, BH);
		Frame(BX, BY, BW, BH, Yellow, 1.f * U);
		Txt(UseLabel, BX + BW * 0.5f, BY + 6.f * U, bHov ? FLinearColor(0.05f, 0.04f, 0.01f) : Yellow, 0.85f * U, Medium, true, false);
		AddButton(Btn_InspectUse, BX, BY, BW, BH);
		BX += BW + 16.f * U;
	}
	const FString CloseLabel(TEXT("FERMER"));
	const float CW = TextW(CloseLabel, Medium, 0.85f * U) + 40.f * U;
	const bool bHovC = Hover(BX, BY, CW, BH);
	DrawRect(bHovC ? WithAlpha(Ink, 0.85f) : FLinearColor(0.f, 0.f, 0.f, 0.5f), BX, BY, CW, BH);
	Frame(BX, BY, CW, BH, InkDim, 1.f * U);
	Txt(CloseLabel, BX + CW * 0.5f, BY + 6.f * U, bHovC ? FLinearColor(0.05f, 0.04f, 0.01f) : Ink, 0.85f * U, Medium, true, false);
	AddButton(Btn_InspectClose, BX, BY, CW, BH);
}

void ABRHUD::DrawJournalTab(ABRCharacter* C, ABRWorld* W)
{
	if (!W)
	{
		return;
	}
	const float U = Ui();
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const FBRLevelDef& D = W->Def();
	const float Gap = IW * 0.02f;
	const float LW = IW * 0.49f;
	const float RX = IX + LW + Gap;
	const float RW = IW - LW - Gap;

	// ---- Niveau actuel + notes
	Panel(IX, IY, LW, IH, TEXT("JOURNAL DU VAGABOND"));
	float X = IX + 20.f * U;
	float Y = IY + 60.f * U;
	const float ColWidth = LW - 40.f * U;
	Txt(FString::Printf(TEXT("NIVEAU %d - \u00ab %s \u00bb"), D.Number, *D.Title), X, Y, Ink, 0.95f * U, Large, false, false);
	Y += 40.f * U;
	Txt(D.Nickname, X, Y, InkDim, 0.85f * U, Medium, false, false);
	Y += 26.f * U;
	Txt(D.ClassText, X, Y, ClassColor(D.SurvivalClass), 0.85f * U, Medium, false, false);
	Y += 32.f * U;
	for (const FString& L : Wrap(D.Description, ColWidth, Medium, 0.8f * U))
	{
		Txt(L, X, Y, Ink, 0.8f * U, Medium, false, false);
		Y += 23.f * U;
	}
	Y += 12.f * U;
	FString Visited;
	for (int32 N : W->GetVisitedLevels())
	{
		Visited += Visited.IsEmpty() ? FString::FromInt(N) : FString(TEXT(", ")) + FString::FromInt(N);
	}
	Txt(TEXT("Niveaux visit\u00e9s : ") + Visited, X, Y, FLinearColor(0.7f, 0.8f, 0.9f), 0.8f * U, Medium, false, false);
	Y += 26.f * U;
	Txt(FString::Printf(TEXT("Temps sur ce niveau : %s"), *Timecode(W->GetLevelTime())), X, Y, FLinearColor(0.7f, 0.8f, 0.9f), 0.8f * U, Medium,
		false, false);
	Y += 26.f * U;
	for (const FString& L : Wrap(TEXT("Pour quitter un niveau : cherchez un passage (mur qui gr\u00e9sille, porte, \u00e9chelle...). ")
		TEXT("Les sorties \u00e9mettent un bourdonnement \u00e9lectrique : \u00e9coutez."), ColWidth, Small, 0.75f * U))
	{
		Txt(L, X, Y, Yellow, 0.75f * U, Small, false, false);
		Y += 20.f * U;
	}
	Y += 14.f * U;
	if (C)
	{
		Txt(FString::Printf(TEXT("NOTES TROUV\u00c9ES (%d)"), C->ReadNotes.Num()), X, Y, Yellow, 0.85f * U, Medium, false, false);
		Y += 30.f * U;
		for (int32 i = C->ReadNotes.Num() - 1; i >= 0 && Y < IY + IH - 40.f * U; --i)
		{
			const TArray<FString> NoteLines = Wrap(TEXT("\u00ab ") + C->ReadNotes[i] + TEXT(" \u00bb"), ColWidth, Small, 0.7f * U);
			for (const FString& L : NoteLines)
			{
				if (Y > IY + IH - 30.f * U)
				{
					break;
				}
				Txt(L, X, Y, InkDim, 0.7f * U, Small, false, false);
				Y += 18.f * U;
			}
			Y += 8.f * U;
		}
	}

	// ---- Entites
	Panel(RX, IY, RW, IH, TEXT("ENTIT\u00c9S RENCONTR\u00c9ES"));
	X = RX + 20.f * U;
	Y = IY + 60.f * U;
	const float EW = RW - 40.f * U;
	bool bAny = false;
	for (int32 K = 0; K < static_cast<int32>(EBREntityKind::Count); ++K)
	{
		const EBREntityKind Kind = static_cast<EBREntityKind>(K);
		if (!W->IsDiscovered(Kind))
		{
			continue;
		}
		bAny = true;
		const FBREntityInfo& Info = ABREntity::Info(Kind);
		Txt(FString::Printf(TEXT("%s - %s"), *Info.Number, *Info.Name), X, Y, FLinearColor(1.f, 0.6f, 0.5f), 0.9f * U, Medium, false, false);
		Y += 28.f * U;
		for (const FString& L : Wrap(Info.Description, EW, Small, 0.72f * U))
		{
			Txt(L, X + 12.f * U, Y, Ink, 0.72f * U, Small, false, false);
			Y += 19.f * U;
		}
		for (const FString& L : Wrap(TEXT("Conseil : ") + Info.Advice, EW, Small, 0.72f * U))
		{
			Txt(L, X + 12.f * U, Y, Done, 0.72f * U, Small, false, false);
			Y += 19.f * U;
		}
		Y += 12.f * U;
		if (Y > IY + IH - 60.f * U)
		{
			break;
		}
	}
	if (!bAny)
	{
		Txt(TEXT("Aucune pour l'instant... et c'est tr\u00e8s bien comme \u00e7a."), X, Y, InkDim, 0.8f * U, Medium, false, false);
		Y += 30.f * U;
		for (const FString& L : Wrap(TEXT("Astuce : filmer une entit\u00e9 au cam\u00e9scope pendant 3 secondes ajoute sa fiche au journal."), EW, Small, 0.72f * U))
		{
			Txt(L, X, Y, InkDim, 0.72f * U, Small, false, false);
			Y += 19.f * U;
		}
	}
}

void ABRHUD::DrawSettingsTab(ABRPlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	const float U = Ui();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float PanelW = FMath::Min(IW, 1760.f * U);
	const float PanelX = IX + (IW - PanelW) * 0.5f;
	Panel(PanelX, IY, PanelW, IH, TEXT("PARAM\u00c8TRES"));

	// Deux colonnes : controles, son et affichage a gauche, graphismes a droite
	const int32 Count = PC->GetSettingsCount();
	const int32 PerColumn = (Count + 1) / 2;
	const float ColGap = 30.f * U;
	const float W = (PanelW - ColGap) * 0.5f;
	const float RowH = FMath::Min(56.f * U, (IH - 160.f * U) / FMath::Max(1, PerColumn));
	HoverSetting = INDEX_NONE;
	const float Btn = 34.f * U;
	const float ValueW = 250.f * U;
	for (int32 i = 0; i < Count; ++i)
	{
		const float X = PanelX + (i < PerColumn ? 0.f : W + ColGap);
		const float Y = IY + 64.f * U + (i % PerColumn) * RowH;
		const bool bHov = Hover(X + 10.f * U, Y, W - 20.f * U, RowH - 6.f * U);
		if (bHov)
		{
			HoverSetting = i;
			DrawRect(FLinearColor(1.f, 0.85f, 0.3f, 0.06f), X + 10.f * U, Y, W - 20.f * U, RowH - 6.f * U);
		}
		AddButton(Btn_SettingRow + i, X + 10.f * U, Y, W - 20.f * U - (ValueW + Btn * 2.f + 40.f * U), RowH - 6.f * U);
		const float TY = Y + (RowH - 6.f * U) * 0.5f - 11.f * U;
		Txt(PC->GetSettingLabel(i), X + 28.f * U, TY, bHov ? Yellow : Ink, 0.85f * U, Medium, false, false);

		// [<]  valeur  [>]
		const float PX = X + W - 28.f * U - Btn;
		const float MX = PX - ValueW - Btn;
		const float BY = Y + (RowH - 6.f * U - Btn) * 0.5f;
		const bool bHovM = Hover(MX, BY, Btn, Btn);
		const bool bHovP = Hover(PX, BY, Btn, Btn);
		Frame(MX, BY, Btn, Btn, bHovM ? Yellow : YellowDim, 1.f * U);
		Frame(PX, BY, Btn, Btn, bHovP ? Yellow : YellowDim, 1.f * U);
		Txt(TEXT("<"), MX + Btn * 0.5f, BY + 4.f * U, bHovM ? Yellow : Ink, 0.85f * U, Medium, true, false);
		Txt(TEXT(">"), PX + Btn * 0.5f, BY + 4.f * U, bHovP ? Yellow : Ink, 0.85f * U, Medium, true, false);
		Txt(PC->GetSettingValue(i), MX + Btn + ValueW * 0.5f, TY, Yellow, 0.85f * U, Medium, true, false);
		AddButton(Btn_SettingBase + i * 2, MX, BY, Btn, Btn);
		AddButton(Btn_SettingBase + i * 2 + 1, PX, BY, Btn, Btn);
		DrawRect(FLinearColor(0.95f, 0.78f, 0.25f, 0.12f), X + 18.f * U, Y + RowH - 4.f * U, W - 36.f * U, 1.f * U);
	}
	const float X = PanelX;
	const float W2 = PanelW;

	// Aide de la ligne survolee
	const FString Hint = HoverSetting != INDEX_NONE ? PC->GetSettingHint(HoverSetting) : FString();
	float HY = IY + IH - 84.f * U;
	for (const FString& L : Wrap(Hint.IsEmpty() ? FString(TEXT("Les r\u00e9glages sont sauvegard\u00e9s automatiquement (BackroomsPlayer.ini).")) : Hint,
		W2 - 56.f * U, Small, 0.75f * U))
	{
		Txt(L, X + 28.f * U, HY, InkDim, 0.75f * U, Small, false, false);
		HY += 20.f * U;
	}
	Txt(TEXT("RTX : n\u00e9cessite une carte compatible ray tracing (sinon Lumen logiciel est utilis\u00e9 automatiquement)."), X + 28.f * U,
		IY + IH - 36.f * U, WithAlpha(InkDim, 0.7f), 0.68f * U, Small, false, false);
}

void ABRHUD::HandleInventoryMouse(ABRPlayerController* PC, ABRCharacter* C)
{
	if (!PlayerOwner || !PC)
	{
		return;
	}
	const bool bPressed = PlayerOwner->WasInputKeyJustPressed(EKeys::LeftMouseButton);
	const bool bReleased = PlayerOwner->WasInputKeyJustReleased(EKeys::LeftMouseButton);
	const bool bHeld = PlayerOwner->IsInputKeyDown(EKeys::LeftMouseButton);
	const bool bRight = PlayerOwner->WasInputKeyJustPressed(EKeys::RightMouseButton);

	FSlotBox* Under = (C && Tab == ETab::Character && !Inspecting.IsValid()) ? FindSlot(MouseX, MouseY) : nullptr;
	HoverSlot = Under ? Under->Ref : FSlotRef();

	// Clic droit sur une touche : effacer
	if (bRight && Tab == ETab::Keys)
	{
		const int32 RId = ButtonAt(MouseX, MouseY);
		if (RId >= Btn_KeySlot)
		{
			PC->ClearKey((RId - Btn_KeySlot) / BRKeys::SlotsPerAction, (RId - Btn_KeySlot) % BRKeys::SlotsPerAction);
		}
		return;
	}

	if (bPressed)
	{
		const int32 Id = ButtonAt(MouseX, MouseY);
		if (Id != INDEX_NONE)
		{
			if (Id >= Btn_KeySlot)
			{
				PC->BeginKeyCapture((Id - Btn_KeySlot) / BRKeys::SlotsPerAction, (Id - Btn_KeySlot) % BRKeys::SlotsPerAction);
			}
			else if (Id == Btn_KeysReset)
			{
				PC->ResetKeys();
			}
			else if (Id >= Btn_TabCharacter && Id <= Btn_TabKeys)
			{
				const ETab NewTab = static_cast<ETab>(Id - Btn_TabCharacter);
				if (C || NewTab == ETab::Settings || NewTab == ETab::Keys)
				{
					Tab = NewTab;
				}
				Inspecting = FSlotRef();
				Dragging = FSlotRef();
				PC->CancelKeyCapture();
			}
			else if (Id == Btn_InspectUse && Inspecting.IsValid() && C)
			{
				C->UseSlot(Inspecting.Group, Inspecting.Index);
				const FBRItemSlot* After = C->GetSlot(Inspecting.Group, Inspecting.Index);
				if (!After || After->IsEmpty() || Inspecting.Group == EBRSlotGroup::Equipment || BRItems::Get(After->Item).Slot != EBREquipSlot::None)
				{
					Inspecting = FSlotRef();
				}
			}
			else if (Id == Btn_InspectClose)
			{
				Inspecting = FSlotRef();
			}
			else if (Id >= Btn_SettingRow && Id < Btn_KeysReset)
			{
				PC->AdjustSetting(Id - Btn_SettingRow, 1);
			}
			else if (Id >= Btn_SettingBase && Id < Btn_SettingRow)
			{
				const int32 Rel = Id - Btn_SettingBase;
				PC->AdjustSetting(Rel / 2, (Rel % 2) ? 1 : -1);
			}
			return;
		}
		if (Inspecting.IsValid())
		{
			Inspecting = FSlotRef(); // clic en dehors : ferme l'inspection
			return;
		}
		if (PC->IsCapturingKey())
		{
			return;
		}
		if (Under && C)
		{
			const double Now = FPlatformTime::Seconds();
			const FBRItemSlot* S = C->GetSlot(Under->Ref.Group, Under->Ref.Index);
			if (S && !S->IsEmpty())
			{
				if (LastClickSlot == Under->Ref && Now - LastClickTime < 0.35)
				{
					// Double-clic : utiliser / equiper / retirer
					C->UseSlot(Under->Ref.Group, Under->Ref.Index);
					LastClickTime = 0.0;
					Dragging = FSlotRef();
					return;
				}
				Dragging = Under->Ref;
			}
			LastClickSlot = Under->Ref;
			LastClickTime = Now;
		}
	}

	if (Dragging.IsValid() && (bReleased || !bHeld))
	{
		if (Under && C && !(Under->Ref == Dragging))
		{
			C->MoveItem(Dragging.Group, Dragging.Index, Under->Ref.Group, Under->Ref.Index);
		}
		Dragging = FSlotRef();
	}

	if (bRight && Under && C)
	{
		const FBRItemSlot* S = C->GetSlot(Under->Ref.Group, Under->Ref.Index);
		if (S && !S->IsEmpty())
		{
			Inspecting = Under->Ref;
			Dragging = FSlotRef();
			C->PlayUISound(TEXT("S_UIClick"));
		}
	}
}

// =====================================================================================================================
// Multijoueur
// =====================================================================================================================

void ABRHUD::DrawTeammates(ABRCharacter* C)
{
	UWorld* World = GetWorld();
	if (!World || !C || World->GetNetMode() == NM_Standalone)
	{
		return;
	}
	const float U = Ui();
	UFont* Medium = GEngine->GetMediumFont();
	for (TActorIterator<ABRCharacter> It(World); It; ++It)
	{
		ABRCharacter* Other = *It;
		if (!Other || Other == C)
		{
			continue;
		}
		const float Dist = static_cast<float>(FVector::Dist(Other->GetActorLocation(), C->GetActorLocation()));
		if (Dist > 5000.f)
		{
			continue;
		}
		const FVector Screen = Project(Other->GetActorLocation() + FVector(0.f, 0.f, 112.f));
		if (Screen.Z <= 0.f || Screen.X < 0.f || Screen.Y < 0.f || Screen.X > Canvas->ClipX || Screen.Y > Canvas->ClipY)
		{
			continue;
		}
		const APlayerState* PS = Other->GetPlayerState();
		FString Name = PS ? PS->GetPlayerName() : FString(TEXT("Explorateur"));
		if (Name.Len() > 20)
		{
			Name = Name.Left(20);
		}
		const float A = FMath::Clamp(1.2f - Dist / 5000.f, 0.35f, 1.f);
		if (Other->IsDead())
		{
			Name += TEXT("  (\u00e0 terre)");
		}
		const FLinearColor Col = Other->IsDead() ? FLinearColor(1.f, 0.45f, 0.4f, A) : FLinearColor(0.8f, 1.f, 0.8f, A);
		Txt(Name, static_cast<float>(Screen.X), static_cast<float>(Screen.Y) - 22.f * U, Col, 0.8f * U, Medium, true);
		// Il parle : petites barres qui bougent avec sa voix, a cote de son nom
		const ABRPlayerController* MyPC = Cast<ABRPlayerController>(PlayerOwner);
		const float Talk = MyPC ? FMath::Clamp(MyPC->GetTalkLevel(PS) * 6.f, 0.f, 1.f) : 0.f;
		if (Talk > 0.05f)
		{
			const float BX = static_cast<float>(Screen.X) + TextW(Name, Medium, 0.8f * U) * 0.5f + 10.f * U;
			for (int32 k = 0; k < 3; ++k)
			{
				const float BH = (4.f + 12.f * Talk * (0.6f + 0.4f * FMath::Sin(Clock * 18.f + k * 1.7f))) * U;
				DrawRect(FLinearColor(0.6f, 1.f, 0.6f, A), BX + k * 6.f * U, static_cast<float>(Screen.Y) - 6.f * U - BH, 3.f * U, BH);
			}
		}
		Txt(FString::Printf(TEXT("%d m"), FMath::RoundToInt(Dist / 100.f)), static_cast<float>(Screen.X), static_cast<float>(Screen.Y) - 2.f * U,
			WithAlpha(InkDim, A), 0.65f * U, Medium, true);
	}
}

void ABRHUD::DrawVoiceIndicator(ABRPlayerController* PC)
{
	if (!PC || !PC->IsNetGame())
	{
		return;
	}
	const float U = Ui();
	const float X = 50.f * U;
	const float Y = Canvas->ClipY - 236.f * U;
	UFont* Small = GEngine->GetSmallFont();
	const int32 Mode = FBRSettings::Get().VoiceMode;
	if (PC->IsTransmittingVoice())
	{
		const float Pulse = 0.7f + 0.3f * FMath::Sin(Clock * 6.f);
		DrawRect(FLinearColor(0.95f, 0.2f, 0.15f, Pulse), X, Y + 4.f * U, 10.f * U, 10.f * U);
		Txt(Mode == 0 ? TEXT("MICRO OUVERT") : TEXT("VOUS PARLEZ"), X + 18.f * U, Y, FLinearColor(1.f, 0.85f, 0.8f, 0.9f), 0.75f * U, Small, false);
	}
	else if (Mode == 1)
	{
		Txt(BRKeys::Tag(EBRAction::PushToTalk) + TEXT(" parler"), X, Y, WithAlpha(InkDim, 0.7f), 0.7f * U, Small, false);
	}
	else if (Mode == 2)
	{
		Txt(TEXT("MICRO COUP\u00c9"), X, Y, WithAlpha(InkDim, 0.6f), 0.7f * U, Small, false);
	}
}

void ABRHUD::DrawPlayerList()
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return;
	}
	const float U = Ui();
	UFont* Medium = GEngine->GetMediumFont();
	const float W = 380.f * U;
	const float X = Canvas->ClipX - W - 50.f * U;
	float Y = Canvas->ClipY * 0.29f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, Y, W, 44.f * U + GS->PlayerArray.Num() * 34.f * U);
	Frame(X, Y, W, 44.f * U + GS->PlayerArray.Num() * 34.f * U, YellowDim, 1.f * U);
	Txt(TEXT("JOUEURS"), X + 16.f * U, Y + 10.f * U, Yellow, 0.85f * U, Medium, false, false);
	TxtRight(TEXT("LATENCE"), X + W - 16.f * U, Y + 10.f * U, InkDim, 0.7f * U, Medium);
	Y += 44.f * U;
	for (int32 i = 0; i < GS->PlayerArray.Num(); ++i)
	{
		const APlayerState* PS = GS->PlayerArray[i];
		if (!PS)
		{
			continue;
		}
		// Le premier joueur de la liste est l'hote (c'est lui qui a cree la partie)
		FString Name = PS->GetPlayerName();
		if (Name.Len() > 18)
		{
			Name = Name.Left(18);
		}
		const bool bMe = PlayerOwner && PS == PlayerOwner->PlayerState;
		Txt(Name + (i == 0 ? TEXT("  (H\u00d4TE)") : TEXT("")), X + 16.f * U, Y, bMe ? Yellow : Ink, 0.78f * U, Medium, false, false);
		const int32 Ping = FMath::RoundToInt(PS->GetPingInMilliseconds());
		const FLinearColor PingCol = Ping < 80 ? Done : (Ping < 160 ? Yellow : Danger);
		TxtRight(i == 0 ? FString(TEXT("-")) : FString::Printf(TEXT("%d ms"), Ping), X + W - 16.f * U, Y, PingCol, 0.78f * U, Medium);
		Y += 34.f * U;
	}
}
