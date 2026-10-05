#include "BRHUD.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BREntity.h"
#include "BRItems.h"
#include "BRKeys.h"
#include "BRLevels.h"
#include "BRPlayerController.h"
#include "BRSave.h"
#include "BRWorld.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineFontServices.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"

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
		Btn_MenuCard = 990,      // 990..996 : cartes du carrousel des niveaux (993 = carte centrale)
		Btn_SaveDelete = 1500,   // 1500 + emplacement : corbeille d'une partie
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

	float EaseOut(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return 1.f - (1.f - T) * (1.f - T) * (1.f - T);
	}

	float Smooth(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float T)
	{
		return A + (B - A) * T;
	}

	/**
	 * Police "objet" de l'interface. Le Canvas n'affiche un texte que si sa FSlateFontInfo porte une UFont
	 * (FCanvasTextItem::HasValidText teste Font) : la police Slate par defaut seule (v4.0, v4.1) ne s'affichait pas.
	 * On prend la police Roboto du moteur (police "runtime" composite de preference).
	 */
	UFont* UiFontObject()
	{
		static TWeakObjectPtr<UFont> Cached;
		if (UFont* F = Cached.Get())
		{
			return F;
		}
		UFont* Best = nullptr;
		if (GEngine)
		{
			for (UFont* F : { GEngine->GetMediumFont(), GEngine->GetLargeFont(), GEngine->GetSmallFont() })
			{
				if (F && F->FontCacheType == EFontCacheType::Runtime)
				{
					Best = F;
					break;
				}
				Best = Best ? Best : F;
			}
		}
		Cached = Best;
		return Best;
	}

	/** Graisse demandee si la police la contient, sinon la plus proche ; NAME_None : premiere police de la famille */
	FName PickTypeface(const FCompositeFont* Composite, int32 Weight)
	{
		static const TCHAR* const Wanted[4][2] = {
			{ TEXT("Light"), TEXT("Regular") },
			{ TEXT("Regular"), nullptr },
			{ TEXT("Bold"), nullptr },
			{ TEXT("Black"), TEXT("Bold") } };
		if (!Composite)
		{
			return FName(Wanted[Weight][0]);
		}
		for (const TCHAR* Name : Wanted[Weight])
		{
			if (!Name)
			{
				break;
			}
			const FName Face(Name);
			for (const FTypefaceEntry& Entry : Composite->DefaultTypeface.Fonts)
			{
				if (Entry.Name == Face)
				{
					return Face;
				}
			}
		}
		return NAME_None;
	}

	/** Police de l'interface v4 : Roboto, graisse Light / Regular / Bold / Black */
	FSlateFontInfo UiFontInfo(float Size, int32 Weight, float U)
	{
		// Tailles entieres : chaque taille occupe sa place dans l'atlas des polices
		const float Pt = FMath::Max(6.f, FMath::RoundToFloat(Size * U));
		const int32 Wt = FMath::Clamp(Weight, 0, 3);
		UFont* Font = UiFontObject();
		if (!Font)
		{
			static const FName Faces[] = { FName(TEXT("Light")), FName(TEXT("Regular")), FName(TEXT("Bold")), FName(TEXT("Black")) };
			return FCoreStyle::GetDefaultFontStyle(Faces[Wt], Pt);
		}
		static TWeakObjectPtr<UFont> ResolvedFor;
		static FName Faces[4];
		if (ResolvedFor.Get() != Font)
		{
			ResolvedFor = Font;
			const FSlateFontInfo Probe(Font, Pt);
			for (int32 i = 0; i < 4; ++i)
			{
				Faces[i] = PickTypeface(Probe.GetCompositeFont(), i);
			}
		}
		return FSlateFontInfo(Font, Pt, Faces[Wt]);
	}

	/** "Classe 1 : Sur - Stable" -> "CLASSE 1" */
	FString ClassShort(const FString& ClassText)
	{
		int32 Colon = INDEX_NONE;
		const FString Head = ClassText.FindChar(TEXT(':'), Colon) ? ClassText.Left(Colon).TrimEnd() : ClassText;
		return Head.ToUpper();
	}

	/** Carte du carrousel a dessiner : niveau et ecart (en cartes) avec le centre */
	struct FCardDraw
	{
		int32 Index;
		float Off;
	};

	/** Astuces du menu titre ({Action} : touche configuree) */
	const TCHAR* const MenuTips[] = {
		TEXT("Smilers : ne braquez JAMAIS votre lampe sur eux. \u00c9teignez-la, ne courez pas et reculez lentement."),
		TEXT("Hounds : ne fuyez pas en courant. Faites-leur face, regardez-les et reculez calmement."),
		TEXT("Partygoers : ne soutenez pas leur regard. Pendant les coupures de courant, cachez-vous."),
		TEXT("L'eau d'amande apaise l'esprit : buvez-en ({Drink}) quand votre sant\u00e9 mentale baisse."),
		TEXT("Au Niveau 0, ramassez les cassettes VHS et filmez une coupure de courant pour ouvrir la sortie."),
		TEXT("En multijoueur, c'est le joueur qui a le PC le plus puissant qui doit h\u00e9berger la partie."),
		TEXT("{Inventory} ouvre l'inventaire : glissez les objets, double-cliquez pour les utiliser."),
		TEXT("Placards, trous dans les murs : une fois cach\u00e9, les entit\u00e9s ne vous voient plus."),
		TEXT("Deathmoths : \u00e9teignez votre lampe d\u00e8s que vous entendez des battements d'ailes."),
		TEXT("Si vous entendez frapper au Niveau 0, \u00e9loignez-vous : la Bacteria n'est pas loin."),
		TEXT("Toutes les touches se changent dans Param\u00e8tres > Touches (jusqu'\u00e0 3 par action)."),
		TEXT("Coop : un co\u00e9quipier \u00e0 terre se rel\u00e8ve si vous maintenez {Interact} pr\u00e8s de lui."),
	};

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
	TextF(Label, X, Y - 20.f * U, FLinearColor(1.f, 1.f, 1.f, 0.78f), 9.5f, EUiWeight::Bold);
	const float R = H * 0.5f;
	RoundRect(X, Y, W, H, R, FLinearColor(0.f, 0.f, 0.f, 0.5f));
	const float F = FMath::Clamp(Fill, 0.f, 1.f);
	if (F > 0.f)
	{
		RoundRect(X, Y, FMath::Max(H, W * F), H, R, C);
	}
}

void ABRHUD::Frame(float X, float Y, float W, float H, const FLinearColor& C, float Thickness)
{
	DrawRect(C, X, Y, W, Thickness);
	DrawRect(C, X, Y + H - Thickness, W, Thickness);
	DrawRect(C, X, Y + Thickness, Thickness, H - 2.f * Thickness);
	DrawRect(C, X + W - Thickness, Y + Thickness, Thickness, H - 2.f * Thickness);
}

void ABRHUD::Panel(float X, float Y, float W, float H, const FString& Title)
{
	const float U = Ui();
	RoundRect(X, Y, W, H, 14.f * U, PanelBg);
	RoundRect(X, Y, W, H, 14.f * U, FLinearColor(0.95f, 0.78f, 0.25f, 0.22f), true);
	if (!Title.IsEmpty())
	{
		TextSpaced(Title, X + 20.f * U, Y + 13.f * U, Yellow, 11.5f, EUiWeight::Bold, 2.5f * U);
		DrawRect(FLinearColor(0.95f, 0.78f, 0.25f, 0.25f), X + 18.f * U, Y + 42.f * U, W - 36.f * U, FMath::Max(1.f, U));
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
// Primitives v4 : texte net (polices Slate), formes arrondies, degrades, halos
// =====================================================================================================================

void ABRHUD::TextF(const FString& S, float X, float Y, const FLinearColor& C, float Size, EUiWeight Weight, EUiAlign Align, bool bShadow)
{
	if (S.IsEmpty() || C.A <= 0.004f || !Canvas)
	{
		return;
	}
	if (Align != EUiAlign::Left)
	{
		const float W = TextSize(S, Size, Weight).X;
		X -= Align == EUiAlign::Center ? W * 0.5f : W;
	}
	FCanvasTextItem Item(FVector2D(FMath::RoundToFloat(X), FMath::RoundToFloat(Y)), FText::FromString(S), UiFontInfo(Size, static_cast<int32>(Weight), Ui()), C);
	if (bShadow)
	{
		const float Off = FMath::Max(1.f, FMath::RoundToFloat(1.5f * Ui()));
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.6f * C.A), FVector2D(Off, Off));
	}
	Canvas->DrawItem(Item);
}

float ABRHUD::TextSpaced(const FString& S, float X, float Y, const FLinearColor& C, float Size, EUiWeight Weight, float Spacing, EUiAlign Align)
{
	TArray<float> Widths;
	float Total = 0.f;
	for (int32 i = 0; i < S.Len(); ++i)
	{
		const float CW = TextSize(S.Mid(i, 1), Size, Weight).X;
		Widths.Add(CW);
		Total += CW + (i + 1 < S.Len() ? Spacing : 0.f);
	}
	if (Align != EUiAlign::Left)
	{
		X -= Align == EUiAlign::Center ? Total * 0.5f : Total;
	}
	for (int32 i = 0; i < S.Len(); ++i)
	{
		TextF(S.Mid(i, 1), X, Y, C, Size, Weight, EUiAlign::Left, false);
		X += Widths[i] + Spacing;
	}
	return Total;
}

FVector2f ABRHUD::TextSize(const FString& S, float Size, EUiWeight Weight) const
{
	const FSlateFontInfo Font = UiFontInfo(Size, static_cast<int32>(Weight), Ui());
	if (FEngineFontServices::IsInitialized())
	{
		const TSharedRef<FSlateFontMeasure> Measure = FEngineFontServices::Get().GetFontMeasure();
		const float LineH = static_cast<float>(Measure->GetMaxCharacterHeight(Font));
		if (S.IsEmpty())
		{
			return FVector2f(0.f, LineH);
		}
		const FVector2D M(Measure->Measure(S, Font));
		return FVector2f(static_cast<float>(M.X), FMath::Max(static_cast<float>(M.Y), LineH));
	}
	const float Px = Font.Size * 4.f / 3.f;
	return FVector2f(S.Len() * Px * 0.52f, Px * 1.2f);
}

TArray<FString> ABRHUD::WrapF(const FString& S, float MaxWidth, float Size, EUiWeight Weight) const
{
	TArray<FString> Lines;
	TArray<FString> Words;
	S.ParseIntoArray(Words, TEXT(" "), true);
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Test = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		if (TextSize(Test, Size, Weight).X > MaxWidth && !Line.IsEmpty())
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

FString ABRHUD::Ellipsize(const FString& S, float MaxWidth, float Size, EUiWeight Weight) const
{
	if (TextSize(S, Size, Weight).X <= MaxWidth)
	{
		return S;
	}
	FString Out = S;
	while (Out.Len() > 1 && TextSize(Out + TEXT("\u2026"), Size, Weight).X > MaxWidth)
	{
		Out = Out.LeftChop(1);
	}
	return Out.TrimEnd() + TEXT("\u2026");
}

UTexture* ABRHUD::UiTex(const TCHAR* Name)
{
	UBRAssets* A = UBRAssets::Get(this);
	return A ? A->Icon(FName(Name)) : nullptr;
}

void ABRHUD::RoundRect(float X, float Y, float W, float H, float R, const FLinearColor& C, bool bOutline)
{
	if (C.A <= 0.003f || W < 1.f || H < 1.f)
	{
		return;
	}
	X = FMath::RoundToFloat(X);
	Y = FMath::RoundToFloat(Y);
	W = FMath::RoundToFloat(W);
	H = FMath::RoundToFloat(H);
	R = FMath::Min(FMath::RoundToFloat(R), FMath::FloorToFloat(FMath::Min(W, H) * 0.5f));
	UTexture* T = UiTex(bOutline ? TEXT("UI_RoundLine") : TEXT("UI_Round"));
	if (!T || R < 2.f)
	{
		if (bOutline)
		{
			Frame(X, Y, W, H, C, FMath::Max(1.f, FMath::RoundToFloat(Ui())));
		}
		else
		{
			DrawRect(C, X, Y, W, H);
		}
		return;
	}
	// 9 tranches : coins de R pixels (16 texels sur 64), bords etires, centre plein
	const float E = 0.25f;
	const float MW = W - 2.f * R;
	const float MH = H - 2.f * R;
	auto Piece = [&](float PX, float PY, float PW, float PH, float U0, float V0, float UL, float VL)
	{
		if (PW > 0.f && PH > 0.f)
		{
			DrawTexture(T, PX, PY, PW, PH, U0, V0, UL, VL, C, BLEND_Translucent);
		}
	};
	Piece(X, Y, R, R, 0.f, 0.f, E, E);
	Piece(X + W - R, Y, R, R, 1.f - E, 0.f, E, E);
	Piece(X, Y + H - R, R, R, 0.f, 1.f - E, E, E);
	Piece(X + W - R, Y + H - R, R, R, 1.f - E, 1.f - E, E, E);
	Piece(X + R, Y, MW, R, E, 0.f, 1.f - 2.f * E, E);
	Piece(X + R, Y + H - R, MW, R, E, 1.f - E, 1.f - 2.f * E, E);
	Piece(X, Y + R, R, MH, 0.f, E, E, 1.f - 2.f * E);
	Piece(X + W - R, Y + R, R, MH, 1.f - E, E, E, 1.f - 2.f * E);
	if (!bOutline)
	{
		Piece(X + R, Y + R, MW, MH, E, E, 1.f - 2.f * E, 1.f - 2.f * E);
	}
}

void ABRHUD::Gradient(float X, float Y, float W, float H, const FLinearColor& C, int32 Dir)
{
	if (C.A <= 0.003f || W < 1.f || H < 1.f)
	{
		return;
	}
	const bool bHoriz = Dir < 2;
	if (UTexture* T = UiTex(bHoriz ? TEXT("UI_GradH") : TEXT("UI_GradV")))
	{
		// Une demi-texel de marge : le filtrage ne melange pas les deux extremites
		const float In = 0.5f / 256.f;
		const bool bFlip = Dir == 1 || Dir == 3;
		const float A0 = bFlip ? 1.f - In : In;
		const float AL = (bFlip ? -1.f : 1.f) * (1.f - 2.f * In);
		if (bHoriz)
		{
			DrawTexture(T, X, Y, W, H, A0, 0.25f, AL, 0.5f, C, BLEND_Translucent);
		}
		else
		{
			DrawTexture(T, X, Y, W, H, 0.25f, A0, 0.5f, AL, C, BLEND_Translucent);
		}
		return;
	}
	// Repli sans texture : bandes successives
	const int32 N = 24;
	for (int32 i = 0; i < N; ++i)
	{
		const float T0 = static_cast<float>(i) / N;
		const float K = 1.f - (T0 + 0.5f / N);
		const float Fade = K * K * (3.f - 2.f * K);
		const int32 Step = (Dir == 0 || Dir == 2) ? i : N - 1 - i;
		const FLinearColor BandC = WithAlpha(C, Fade);
		if (bHoriz)
		{
			DrawRect(BandC, X + W * Step / N, Y, W / N + 1.f, H);
		}
		else
		{
			DrawRect(BandC, X, Y + H * Step / N, W, H / N + 1.f);
		}
	}
}

void ABRHUD::Glow(float CX, float CY, float RX, float RY, const FLinearColor& C)
{
	if (C.A <= 0.003f)
	{
		return;
	}
	if (UTexture* T = UiTex(TEXT("UI_Radial")))
	{
		DrawTexture(T, CX - RX, CY - RY, RX * 2.f, RY * 2.f, 0.f, 0.f, 1.f, 1.f, C, BLEND_Translucent);
	}
}

float ABRHUD::KeyCap(float X, float Y, const FString& Key, const FString& Label, float Alpha, bool bDraw)
{
	const float U = Ui();
	const FVector2f KS = TextSize(Key, 10.5f, EUiWeight::Bold);
	const float KH = 26.f * U;
	const float KW = FMath::Max(KH, KS.X + 18.f * U);
	float W = KW;
	FVector2f LS = FVector2f::ZeroVector;
	if (!Label.IsEmpty())
	{
		LS = TextSize(Label, 11.5f, EUiWeight::Regular);
		W += 9.f * U + LS.X;
	}
	if (bDraw)
	{
		RoundRect(X, Y, KW, KH, 6.f * U, FLinearColor(1.f, 0.93f, 0.75f, 0.12f * Alpha));
		RoundRect(X, Y, KW, KH, 6.f * U, FLinearColor(1.f, 0.9f, 0.6f, 0.38f * Alpha), true);
		DrawRect(FLinearColor(1.f, 0.9f, 0.6f, 0.22f * Alpha), X + 5.f * U, Y + KH - 2.f * U, KW - 10.f * U, FMath::Max(1.f, U));
		TextF(Key, X + KW * 0.5f, Y + (KH - KS.Y) * 0.5f, WithAlpha(Ink, Alpha), 10.5f, EUiWeight::Bold, EUiAlign::Center, false);
		if (!Label.IsEmpty())
		{
			TextF(Label, X + KW + 9.f * U, Y + (KH - LS.Y) * 0.5f, WithAlpha(InkDim, Alpha), 11.5f, EUiWeight::Regular, EUiAlign::Left, false);
		}
	}
	return W;
}

void ABRHUD::KeyHints(float X, float Y, const TArray<TPair<FString, FString>>& Hints, float Alpha, bool bCenter)
{
	const float Gap = 28.f * Ui();
	if (bCenter)
	{
		float Total = 0.f;
		for (const TPair<FString, FString>& H : Hints)
		{
			Total += KeyCap(0.f, 0.f, H.Key, H.Value, Alpha, false) + Gap;
		}
		X -= (Total - Gap) * 0.5f;
	}
	for (const TPair<FString, FString>& H : Hints)
	{
		X += KeyCap(X, Y, H.Key, H.Value, Alpha) + Gap;
	}
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
	UiDt = Dt;

	ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	ABRCharacter* C = PC ? Cast<ABRCharacter>(PC->GetPawn()) : nullptr;
	ABRWorld* W = ABRWorld::Get(this);

	// Animations d'ouverture du menu titre et de la pause
	const bool bMenuNow = PC && PC->IsInMenu();
	if (bMenuNow && !bWasInMenu)
	{
		MenuIntro = 0.f;
		LastMenuPage = -1;
		Carousel = -1000.f;
		TipClock = FMath::FRand() * 90.f; // une astuce au hasard a chaque lancement
	}
	bWasInMenu = bMenuNow;
	const bool bPauseNow = PC && PC->IsPauseMenuOpen();
	if (bPauseNow && !bWasPaused)
	{
		PauseTime = 0.f;
		for (float& S : PauseSel)
		{
			S = 0.f;
		}
	}
	bWasPaused = bPauseNow;

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
	if (!bInv)
	{
		DrawTitleCard(); // bandes noires et titre du niveau, sous les informations du HUD
	}
	if (C && !C->IsDead() && !bInv)
	{
		DrawRecording(C, W);
		if (!W || W->GetTitleTime() < 0.5f)
		{
			DrawCrosshair(C);
		}
		DrawStats(C);
		DrawQuickBar(C);
		DrawObjectiveTracker(W);
	}
	if (PC && PC->IsDevMode() && !bInv)
	{
		DrawDevOverlay(PC, C, W);
	}
	if (C && C->IsReadingNote() && !bInv)
	{
		DrawNote(C);
	}
	if (C && C->IsDead())
	{
		DrawDeath(C);
	}
	if (C && C->GetScareKind() >= 0)
	{
		DrawJumpscare(C);
	}
	if (bInv)
	{
		DrawInventory(PC, C, W);
	}
	DrawMessages(Dt);
	if (PC && PC->GetActiveSave() && PC->GetTimeSinceSave() < 3.f && !bInv)
	{
		DrawSaveIndicator(PC->GetTimeSinceSave());
	}
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
// Menu titre (v4.0) : logo, cartes animees, carrousel des niveaux, astuces
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
	if (!A->HasContent())
	{
		TextF(TEXT("Textures indisponibles : ouvrez le projet dans l'\u00e9diteur (plugin Python actif) pour importer les ressources."), CX, Y,
			Danger, 11.5f, EUiWeight::Regular, EUiAlign::Center);
		Y += 20.f * U;
	}
	else if (A->IsUsingRuntimeContent())
	{
		TextF(TEXT("Mode secours : textures lues dans RawAssets/. Relancez l'import (Output Log > Python : import backrooms_setup; backrooms_setup.run(True))"),
			CX, Y, FLinearColor(1.f, 0.8f, 0.35f, 0.85f), 11.f, EUiWeight::Regular, EUiAlign::Center);
		Y += 20.f * U;
	}
	if (!A->Sound(TEXT("S_Hum")))
	{
		TextF(TEXT("Sons non import\u00e9s : le jeu sera silencieux tant que l'import Python n'aura pas \u00e9t\u00e9 fait."), CX, Y,
			FLinearColor(1.f, 0.8f, 0.35f, 0.85f), 11.f, EUiWeight::Regular, EUiAlign::Center);
	}
}

void ABRHUD::DrawLogo(float X, float Y, float W, float A, bool bFlicker)
{
	if (A <= 0.003f)
	{
		return;
	}
	UTexture* T = UiTex(TEXT("UI_Logo"));
	const float Aspect = (T && T->GetSurfaceWidth() > 0.f) ? T->GetSurfaceHeight() / T->GetSurfaceWidth() : 413.f / 1809.f;
	const float H = W * Aspect;
	float F = 1.f;
	if (bFlicker)
	{
		// Toutes les 6,5 s, le neon du logo gresille une fraction de seconde
		const float Ph = FMath::Fmod(Clock, 6.5f);
		if (Ph > 5.85f && FMath::Sin(Clock * 57.f) > -0.2f)
		{
			F = 0.45f + 0.25f * FMath::Abs(FMath::Sin(Clock * 23.f));
		}
	}
	Glow(X + W * 0.5f, Y + H * 0.55f, W * 0.66f, H * 1.35f, FLinearColor(1.f, 0.76f, 0.22f, 0.12f * A * F));
	if (T)
	{
		DrawTexture(T, X, Y, W, H, 0.f, 0.f, 1.f, 1.f, FLinearColor(1.f, 1.f, 1.f, A * (0.35f + 0.65f * F)), BLEND_Translucent);
	}
	else
	{
		TextF(TEXT("THE BACKROOMS"), X + W * 0.03f, Y + H * 0.2f, WithAlpha(Yellow, A * F), W / Ui() * 0.075f, EUiWeight::Black);
	}
}

void ABRHUD::DrawMenuBackdrop(bool bCentered, float A)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float U = Ui();
	// Le niveau reste visible derriere le menu (camera qui derive, flou de profondeur) : on l'assombrit vers le texte
	DrawRect(FLinearColor(0.02f, 0.017f, 0.008f, (bCentered ? 0.5f : 0.2f) * A), 0.f, 0.f, W, H);
	if (!bCentered)
	{
		Gradient(0.f, 0.f, W * 0.68f, H, FLinearColor(0.012f, 0.01f, 0.004f, 0.93f * A), 0);
	}
	Gradient(0.f, H * 0.55f, W, H * 0.45f, FLinearColor(0.008f, 0.007f, 0.003f, 0.85f * A), 3);
	Gradient(0.f, 0.f, W, H * 0.24f, FLinearColor(0.f, 0.f, 0.f, 0.5f * A), 2);
	// Lumiere chaude des neons, en haut a droite
	Glow(W * 0.8f, H * 0.12f, W * 0.42f, H * 0.5f, FLinearColor(1.f, 0.8f, 0.35f, 0.05f * A));
	if (FBRSettings::Get().bVHSEffect)
	{
		Scanlines(0.022f * A);
		// Bande de "tracking" qui descend l'ecran de temps en temps
		const float Cycle = FMath::Fmod(Clock, 9.f);
		if (Cycle < 0.8f)
		{
			const float BY = H * (Cycle / 0.8f);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.035f * A), 0.f, BY, W, FMath::Max(1.f, 2.f * U));
			DrawRect(FLinearColor(1.f, 0.95f, 0.85f, 0.018f * A), 0.f, BY + 5.f * U, W, 16.f * U);
		}
	}
}

void ABRHUD::DrawCard(float X, float Y, float W, float H, float Sel, const FString& Label, const FString& Sub, const TCHAR* IconName, float Alpha, bool bDanger)
{
	if (Alpha <= 0.003f)
	{
		return;
	}
	const float U = Ui();
	const float S = Smooth(Sel);
	const FLinearColor Accent = bDanger ? FLinearColor(0.93f, 0.33f, 0.25f, 1.f) : Yellow;
	const FLinearColor DarkInk(0.07f, 0.055f, 0.02f, 1.f);
	X += S * 14.f * U;
	const float R = 14.f * U;

	// Halo, ombre portee, fond (jaune quand la carte est selectionnee), lisere, reflet
	Glow(X + W * 0.42f, Y + H * 0.5f, W * 0.75f, H * 1.5f, WithAlpha(Accent, 0.16f * S * Alpha));
	RoundRect(X, Y + 4.f * U, W, H, R, FLinearColor(0.f, 0.f, 0.f, 0.3f * Alpha));
	RoundRect(X, Y, W, H, R, WithAlpha(Mix(FLinearColor(0.05f, 0.045f, 0.03f, 0.78f), FLinearColor(Accent.R, Accent.G, Accent.B, 0.97f), S), Alpha));
	RoundRect(X, Y, W, H, R, FLinearColor(1.f, 0.88f, 0.5f, 0.13f * (1.f - S) * Alpha), true);
	Gradient(X + R, Y + 1.f * U, W - 2.f * R, H * 0.45f, FLinearColor(1.f, 1.f, 1.f, (0.03f + 0.07f * S) * Alpha), 2);

	// Icone dans un cercle
	const float D = H - 30.f * U;
	const float CircX = X + 16.f * U;
	const float CircY = Y + (H - D) * 0.5f;
	RoundRect(CircX, CircY, D, D, D * 0.5f, WithAlpha(Mix(FLinearColor(Accent.R, Accent.G, Accent.B, 0.13f), FLinearColor(0.07f, 0.055f, 0.02f, 0.92f), S), Alpha));
	if (UTexture* T = UiTex(IconName))
	{
		DrawTexture(T, CircX + D * 0.22f, CircY + D * 0.22f, D * 0.56f, D * 0.56f, 0.f, 0.f, 1.f, 1.f, WithAlpha(Accent, Alpha), BLEND_Translucent);
	}

	// Libelle et sous-titre
	const float TX = CircX + D + 20.f * U;
	const FVector2f LS = TextSize(Label, 19.f, EUiWeight::Bold);
	const FVector2f SS = Sub.IsEmpty() ? FVector2f::ZeroVector : TextSize(Sub, 11.5f, EUiWeight::Regular);
	const float Block = LS.Y + (Sub.IsEmpty() ? 0.f : SS.Y - 4.f * U);
	const float TY = Y + (H - Block) * 0.5f;
	TextF(Label, TX, TY, WithAlpha(Mix(Ink, DarkInk, S), Alpha), 19.f, EUiWeight::Bold, EUiAlign::Left, S < 0.5f);
	if (!Sub.IsEmpty())
	{
		TextF(Ellipsize(Sub, X + W - TX - 60.f * U, 11.5f, EUiWeight::Regular), TX, TY + LS.Y - 4.f * U,
			WithAlpha(Mix(InkDim, FLinearColor(0.14f, 0.11f, 0.04f, 0.9f), S), Alpha), 11.5f, EUiWeight::Regular, EUiAlign::Left, false);
	}

	// Chevron (glisse vers la droite a la selection)
	if (UTexture* T = UiTex(TEXT("UI_IconArrow")))
	{
		const float AS = 22.f * U;
		DrawTexture(T, X + W - AS - 22.f * U - (1.f - S) * 8.f * U, Y + (H - AS) * 0.5f, AS, AS, 0.f, 0.f, 1.f, 1.f,
			WithAlpha(Mix(FLinearColor(1.f, 0.9f, 0.6f, 0.22f), DarkInk, S), Alpha), BLEND_Translucent);
	}
}

void ABRHUD::MenuCard(int32 Item, float X, float Y, float W, float H, const FString& Sub, const TCHAR* IconName, bool bInteractive, float Appear)
{
	const ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	const float Ap = FMath::Clamp(Appear, 0.f, 1.f);
	const bool bSel = PC && PC->GetMenuCursor() == Item && Ap > 0.5f;
	float& S = MenuSel[FMath::Clamp(Item, 0, 7)];
	S = FMath::FInterpTo(S, bSel ? 1.f : 0.f, UiDt, 14.f);
	const bool bDanger = PC && PC->GetMenuPage() == EBRMenuPage::Main && Item == 3; // QUITTER
	DrawCard(X - (1.f - Ap) * 40.f * Ui(), Y, W, H, S, PC ? PC->GetMenuItemLabel(Item) : FString(), Sub, IconName, Ap, bDanger);
	if (bInteractive && Ap > 0.5f)
	{
		AddButton(Btn_Menu + Item, X, Y, W, H);
	}
}

void ABRHUD::MenuPill(int32 Item, float X, float Y, float W, float H, const TCHAR* IconName, bool bPrimary, bool bInteractive, float Alpha, bool bDanger,
	bool bDisabled)
{
	const ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	if (!PC || Alpha <= 0.003f)
	{
		return;
	}
	const float U = Ui();
	float& Sel = MenuSel[FMath::Clamp(Item, 0, 7)];
	Sel = FMath::FInterpTo(Sel, PC->GetMenuCursor() == Item ? 1.f : 0.f, UiDt, 14.f);
	const float S = Smooth(Sel);
	const FLinearColor DarkInk(0.07f, 0.055f, 0.02f, 1.f);
	// Couleur d'accent : jaune, rouge (suppression) ou gris (bouton inactif : niveau verrouille)
	const FLinearColor Accent = bDisabled ? FLinearColor(0.42f, 0.4f, 0.36f, 1.f) : (bDanger ? FLinearColor(0.9f, 0.3f, 0.24f, 1.f) : Yellow);
	const FLinearColor Fill = bPrimary ? Mix(FLinearColor(Accent.R * 0.86f, Accent.G * 0.83f, Accent.B * 0.75f, 0.92f), FLinearColor(Accent.R, Accent.G, Accent.B, 1.f), S)
									   : Mix(FLinearColor(0.05f, 0.045f, 0.03f, 0.82f), FLinearColor(Accent.R, Accent.G, Accent.B, 0.97f), S);
	const bool bDarkText = (bPrimary || S > 0.5f) && !bDisabled;
	if (!bDisabled)
	{
		Glow(X + W * 0.5f, Y + H * 0.5f, W * 0.8f, H * 1.7f, WithAlpha(Accent, (bPrimary ? 0.08f + 0.12f * S : 0.14f * S) * Alpha));
	}
	RoundRect(X, Y + 4.f * U, W, H, H * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.3f * Alpha));
	RoundRect(X, Y, W, H, H * 0.5f, WithAlpha(Fill, Alpha));
	Gradient(X + H * 0.5f, Y + 1.f * U, W - H, H * 0.45f, FLinearColor(1.f, 1.f, 1.f, 0.08f * Alpha), 2);
	if (bPrimary)
	{
		RoundRect(X - 4.f * U, Y - 4.f * U, W + 8.f * U, H + 8.f * U, H * 0.5f + 4.f * U, FLinearColor(Accent.R, Accent.G, Accent.B, 0.6f * S * Alpha), true);
	}
	else
	{
		RoundRect(X, Y, W, H, H * 0.5f, FLinearColor(1.f, 0.88f, 0.5f, 0.2f * (1.f - S) * Alpha), true);
	}
	const FString Label = PC->GetMenuItemLabel(Item);
	const FVector2f LS = TextSize(Label, 14.5f, EUiWeight::Bold);
	const float IS = 18.f * U;
	const float Total = IS + 12.f * U + LS.X;
	const float TX = X + (W - Total) * 0.5f;
	const FLinearColor TextC = bDisabled ? FLinearColor(0.85f, 0.82f, 0.75f, 0.8f) : (bDarkText ? DarkInk : Ink);
	if (UTexture* T = UiTex(IconName))
	{
		DrawTexture(T, TX, Y + (H - IS) * 0.5f, IS, IS, 0.f, 0.f, 1.f, 1.f, WithAlpha(bDisabled ? TextC : (bDarkText ? DarkInk : Accent), Alpha), BLEND_Translucent);
	}
	TextF(Label, TX + IS + 12.f * U, Y + (H - LS.Y) * 0.5f, WithAlpha(TextC, Alpha), 14.5f, EUiWeight::Bold, EUiAlign::Left, false);
	if (bInteractive)
	{
		AddButton(Btn_Menu + Item, X, Y, W, H);
	}
}

void ABRHUD::DrawTips(float X, float Y, float W, float A)
{
	if (A <= 0.003f)
	{
		return;
	}
	const float U = Ui();
	const int32 N = UE_ARRAY_COUNT(MenuTips);
	const float Period = 7.5f;
	const int32 Index = FMath::FloorToInt(TipClock / Period) % N;
	const float Ph = FMath::Fmod(TipClock, Period);
	const float TA = A * FMath::Clamp(Ph / 0.45f, 0.f, 1.f) * FMath::Clamp((Period - Ph) / 0.45f, 0.f, 1.f);
	const float H = 100.f * U;
	RoundRect(X, Y, W, H, 14.f * U, FLinearColor(0.035f, 0.03f, 0.018f, 0.62f * A));
	RoundRect(X, Y, W, H, 14.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.1f * A), true);
	if (UTexture* T = UiTex(TEXT("UI_IconTip")))
	{
		DrawTexture(T, X + 20.f * U, Y + 16.f * U, 22.f * U, 22.f * U, 0.f, 0.f, 1.f, 1.f, WithAlpha(Yellow, 0.95f * A), BLEND_Translucent);
	}
	TextSpaced(TEXT("ASTUCE"), X + 52.f * U, Y + 18.f * U, WithAlpha(Yellow, 0.9f * A), 10.f, EUiWeight::Bold, 3.f * U);
	TextF(FString::Printf(TEXT("%d / %d"), Index + 1, N), X + W - 20.f * U, Y + 18.f * U, WithAlpha(InkDim, 0.6f * A), 10.f, EUiWeight::Regular,
		EUiAlign::Right, false);
	const TArray<FString> Lines = WrapF(BRKeys::Expand(MenuTips[Index]), W - 72.f * U, 13.f, EUiWeight::Regular);
	float LY = Y + 44.f * U;
	for (int32 i = 0; i < Lines.Num() && i < 2; ++i)
	{
		TextF(Lines[i], X + 52.f * U, LY, WithAlpha(Ink, 0.92f * TA), 13.f, EUiWeight::Regular, EUiAlign::Left, false);
		LY += 21.f * U;
	}
	// Temps restant avant l'astuce suivante
	DrawRect(FLinearColor(1.f, 0.82f, 0.22f, 0.3f * A), X + 20.f * U, Y + H - 9.f * U, (W - 40.f * U) * (Ph / Period), FMath::Max(1.f, 2.f * U));
}

void ABRHUD::DrawMenuFooter(ABRPlayerController* PC, float A)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	TextF(TEXT("v4.5   \u00b7   Inspir\u00e9 du Backrooms Wiki (CC BY-SA 3.0)   \u00b7   \u00a9 1992 THRESHOLD SYSTEMS"), W - 100.f * U, H - 34.f * U,
		WithAlpha(InkDim, 0.55f * A), 9.5f, EUiWeight::Light, EUiAlign::Right, false);
	// Message de connexion / d'erreur reseau : pastille en haut au centre
	if (!PC->GetMenuStatus().IsEmpty())
	{
		const FString& St = PC->GetMenuStatus();
		const FVector2f SS = TextSize(St, 13.f, EUiWeight::Regular);
		const float PH = 40.f * U;
		const float PW = SS.X + 60.f * U;
		const float PX = (W - PW) * 0.5f;
		const float PY = 40.f * U;
		RoundRect(PX, PY, PW, PH, PH * 0.5f, FLinearColor(0.05f, 0.04f, 0.02f, 0.88f * A));
		RoundRect(PX, PY, PW, PH, PH * 0.5f, FLinearColor(1.f, 0.7f, 0.35f, 0.35f * A), true);
		const float D = 10.f * U;
		RoundRect(PX + 20.f * U, PY + (PH - D) * 0.5f, D, D, D * 0.5f, FLinearColor(1.f, 0.7f, 0.35f, (0.6f + 0.4f * FMath::Sin(Clock * 5.f)) * A));
		TextF(St, PX + 40.f * U, PY + (PH - SS.Y) * 0.5f, FLinearColor(1.f, 0.88f, 0.7f, A), 13.f, EUiWeight::Regular, EUiAlign::Left, false);
	}
	DrawContentWarning(10.f * U);
}

void ABRHUD::DrawMenu()
{
	ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	if (!PC)
	{
		return;
	}
	const bool bInteractive = !PC->IsInventoryOpen();
	PlayerOwner->GetMousePosition(MouseX, MouseY);
	Buttons.Reset();

	MenuIntro += UiDt;
	TipClock += UiDt;
	const int32 Page = static_cast<int32>(PC->GetMenuPage());
	if (Page != LastMenuPage)
	{
		LastMenuPage = Page;
		MenuPageTime = 0.f;
		for (float& S : MenuSel)
		{
			S = 0.f;
		}
	}
	MenuPageTime += UiDt;

	const float A = EaseOut(MenuIntro / 1.4f);
	const EBRMenuPage P = PC->GetMenuPage();
	DrawMenuBackdrop(P == EBRMenuPage::Solo || P == EBRMenuPage::Join || P == EBRMenuPage::NewSave, A);
	switch (P)
	{
	case EBRMenuPage::Main:
		DrawMenuMain(PC, bInteractive);
		break;
	case EBRMenuPage::Solo:
		DrawMenuSolo(PC, bInteractive);
		break;
	case EBRMenuPage::Multi:
		DrawMenuMulti(PC, bInteractive);
		break;
	case EBRMenuPage::Join:
		DrawMenuJoin(PC, bInteractive);
		break;
	case EBRMenuPage::Saves:
		DrawMenuSaves(PC, bInteractive);
		break;
	case EBRMenuPage::NewSave:
		DrawMenuNewSave(PC, bInteractive);
		break;
	}
	DrawMenuFooter(PC, A);
	if (bInteractive)
	{
		HandleMenuMouse(PC);
	}
	// Ouverture du jeu : fondu depuis le noir
	if (MenuIntro < 1.6f)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f - EaseOut(MenuIntro / 1.6f)), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	}
}

void ABRHUD::DrawMenuMain(ABRPlayerController* PC, bool bInteractive)
{
	const float U = Ui();
	const float X0 = FMath::Max(60.f * U, Canvas->ClipX * 0.0625f);
	const float In = EaseOut((MenuIntro - 0.2f) / 1.f);

	// Logo (les lettres commencent a 3 % de l'image) et accroche
	const float LogoW = 660.f * U;
	const float LogoY = 92.f * U;
	DrawLogo(X0 - LogoW * 0.03f - (1.f - In) * 50.f * U, LogoY, LogoW, In, true);
	const float TY = LogoY + LogoW * 413.f / 1809.f * 0.86f + 18.f * U;
	DrawRect(WithAlpha(Yellow, In), X0, TY + 11.f * U, 30.f * U, FMath::Max(1.f, 3.f * U));
	TextF(TEXT("Si vous noclippez hors de la r\u00e9alit\u00e9 au mauvais endroit\u2026"), X0 + 44.f * U, TY, WithAlpha(Ink, 0.88f * In), 14.5f, EUiWeight::Light);
	TextF(TEXT("\u2026vous atterrissez dans les Backrooms."), X0 + 44.f * U, TY + 25.f * U, WithAlpha(Yellow, 0.95f * In), 14.5f, EUiWeight::Regular);

	// Cartes : SOLO / MULTIJOUEUR / PARAMETRES / QUITTER (entree en cascade)
	FString SoloSub = TEXT("Nouvelle partie : partir seul dans l'inconnu");
	if (PC->GetSaveOrder().Num() > 0)
	{
		if (const UBRSaveGame* Last = PC->GetSaveInSlot(PC->GetSaveOrder()[0]))
		{
			SoloSub = FString::Printf(TEXT("Reprendre \u00ab %s \u00bb (Niveau %d) ou nouvelle partie"), *Last->SaveName, Last->CurrentLevel);
		}
	}
	const TCHAR* Subs[] = {
		*SoloSub,
		TEXT("Jusqu'\u00e0 4 explorateurs \u00b7 le meilleur PC h\u00e9berge"),
		TEXT("Graphismes, son, touches, chat vocal"),
		TEXT("Revenir \u00e0 la r\u00e9alit\u00e9\u2026 si elle existe"),
	};
	const TCHAR* Icons[] = { TEXT("UI_IconSolo"), TEXT("UI_IconMulti"), TEXT("UI_IconSettings"), TEXT("UI_IconQuit") };
	const float CW = 540.f * U;
	const float CH = 90.f * U;
	const float Gap = 14.f * U;
	float Y = 336.f * U;
	const float Base = FMath::Min(MenuPageTime, MenuIntro - 0.45f);
	for (int32 i = 0; i < PC->GetMenuItemCount() && i < 4; ++i)
	{
		MenuCard(i, X0, Y, CW, CH, Subs[i], Icons[i], bInteractive, EaseOut((Base - 0.07f * i) / 0.45f));
		Y += CH + Gap;
	}
	DrawTips(X0, Y + 12.f * U, CW, EaseOut((Base - 0.4f) / 0.5f));

	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(TEXT("\u2191 \u2193"), TEXT("Choisir"));
	Hints.Emplace(TEXT("ENTR\u00c9E"), TEXT("Valider"));
	Hints.Emplace(TEXT("FIN"), TEXT("Quitter"));
	KeyHints(X0, Canvas->ClipY - 72.f * U, Hints, In, false);
}

void ABRHUD::DrawLevelScene(const FBRLevelDef& D, float X, float Y, float W, float H, float Scale, float Alpha)
{
	// Apercu "en coupe" du niveau avec ses vraies textures : plafond et ses lampes, mur, plinthe, sol, eau, brouillard
	UBRAssets* A = UBRAssets::Get(this);
	const float U = Ui();
	const float Tile = 92.f * U * Scale;
	const bool bOpen = D.bOutdoor || !D.bCeiling;
	const bool bDark = D.Fixture == EBRFixture::None && !bOpen;
	const float Lum = bDark ? 0.16f : 1.f;
	const FLinearColor Light(D.LightColor.R, D.LightColor.G, D.LightColor.B, 1.f);
	auto Surface = [&](const FBRSurface& Surf, float SX, float SY, float SW, float SH, float Rep, float Bright)
	{
		if (SW < 1.f || SH < 1.f)
		{
			return;
		}
		FLinearColor C = Surf.Tint * Light * (Bright * Lum);
		C.A = Alpha;
		UTexture* T = (A && !Surf.Texture.IsNone()) ? A->Texture(Surf.Texture) : nullptr;
		if (T)
		{
			DrawTexture(T, SX, SY, SW, SH, 0.f, 0.f, SW / Rep, SH / Rep, C, BLEND_Translucent);
		}
		else
		{
			DrawRect(FLinearColor(C.R * 0.5f, C.G * 0.48f, C.B * 0.4f, Alpha), SX, SY, SW, SH);
		}
	};

	DrawRect(FLinearColor(0.015f, 0.015f, 0.015f, Alpha), X, Y, W, H);
	float WallTop = Y;
	float WallBot = Y + H * 0.74f;
	if (bOpen)
	{
		FLinearColor SkyTop(0.f, 0.f, 0.f, 1.f);
		FLinearColor SkyLow(0.03f, 0.03f, 0.03f, 1.f);
		switch (D.Sky)
		{
		case EBRSky::Night:
			SkyTop = FLinearColor(0.005f, 0.01f, 0.03f, 1.f);
			SkyLow = FLinearColor(0.05f, 0.06f, 0.11f, 1.f);
			break;
		case EBRSky::Overcast:
			SkyTop = FLinearColor(0.38f, 0.41f, 0.46f, 1.f);
			SkyLow = FLinearColor(0.7f, 0.72f, 0.74f, 1.f);
			break;
		case EBRSky::Day:
			SkyTop = FLinearColor(0.22f, 0.42f, 0.78f, 1.f);
			SkyLow = FLinearColor(0.72f, 0.8f, 0.9f, 1.f);
			break;
		default:
			break;
		}
		DrawRect(WithAlpha(SkyLow, Alpha), X, Y, W, H * 0.62f);
		Gradient(X, Y, W, H * 0.62f, WithAlpha(SkyTop, Alpha), 2);
		WallTop = Y + H * 0.36f;
		WallBot = Y + H * 0.62f;
	}
	else
	{
		const float CeilH = H * 0.16f;
		Surface(D.Ceiling, X, Y, W, CeilH, Tile, 0.72f);
		WallTop = Y + CeilH;
	}
	if (!D.Wall.Texture.IsNone())
	{
		Surface(D.Wall, X, WallTop, W, WallBot - WallTop, Tile, 0.95f);
	}
	if (D.bTrim && !bOpen)
	{
		DrawRect(FLinearColor(D.Trim.Tint.R * 0.3f * Lum, D.Trim.Tint.G * 0.28f * Lum, D.Trim.Tint.B * 0.24f * Lum, Alpha), X, WallBot - 4.f * U * Scale, W, 4.f * U * Scale);
	}
	Surface(D.Floor, X, WallBot, W, Y + H - WallBot, Tile * 0.8f, 0.7f);
	if (D.bWater)
	{
		const float WY = WallBot - H * 0.07f;
		DrawRect(FLinearColor(0.3f, 0.68f, 0.7f, 0.5f * Alpha), X, WY, W, Y + H - WY);
		DrawRect(FLinearColor(0.85f, 1.f, 1.f, 0.4f * Alpha), X, WY, W, FMath::Max(1.f, 2.f * U * Scale));
		Gradient(X, WY, W, (Y + H - WY) * 0.6f, FLinearColor(0.75f, 0.95f, 1.f, 0.18f * Alpha), 2);
	}
	// v4.1 : sol du parking (places peintes) et flaques qui refletent les lampes
	const FBRSurface& Ground = D.Road.Puddles > D.Floor.Puddles ? D.Road : D.Floor;
	if (D.bGarage)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const float LX = X + W * (0.12f + 0.25f * i);
			DrawLine(LX, Y + H, LX + W * 0.07f, WallBot + 2.f * U * Scale, FLinearColor(0.9f, 0.9f, 0.85f, 0.55f * Alpha), FMath::Max(1.f, 1.5f * U * Scale));
		}
	}
	if (Ground.Puddles > 0.f)
	{
		const float FloorH = Y + H - WallBot;
		for (int32 i = 0; i < 3; ++i)
		{
			const float PX = X + W * (0.2f + 0.3f * i);
			const float PY = WallBot + FloorH * (0.35f + 0.2f * (i % 2));
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.25f * Alpha), PX - W * 0.11f, PY - FloorH * 0.06f, W * 0.22f, FloorH * 0.12f);
			Glow(PX, PY, W * 0.1f, FloorH * 0.09f, WithAlpha(Light, 0.45f * Alpha));
		}
	}
	// Lampes : plafonniers (rangee de trois) ou lampadaires a l'horizon
	if (D.Fixture != EBRFixture::None)
	{
		if (!bOpen)
		{
			const float CeilH = WallTop - Y;
			for (int32 i = 0; i < 3; ++i)
			{
				const float LX = X + W * (0.2f + 0.3f * i);
				Glow(LX, WallTop, W * 0.26f, H * 0.42f, WithAlpha(Light, 0.26f * Alpha));
				DrawRect(WithAlpha(Mix(Light, FLinearColor::White, 0.6f), Alpha), LX - W * 0.07f, Y + CeilH * 0.38f, W * 0.14f, FMath::Max(2.f, 5.f * U * Scale));
			}
		}
		else
		{
			for (int32 i = 0; i < 2; ++i)
			{
				const float LX = X + W * (0.28f + 0.46f * i);
				DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, Alpha), LX - 1.5f * U * Scale, WallTop - H * 0.1f, 3.f * U * Scale, WallBot - WallTop + H * 0.1f);
				Glow(LX, WallTop - H * 0.1f, W * 0.18f, H * 0.2f, WithAlpha(Light, 0.55f * Alpha));
			}
		}
	}
	// Brouillard du niveau puis vignettage
	DrawRect(FLinearColor(D.FogColor.R, D.FogColor.G, D.FogColor.B, FMath::Clamp(D.FogDensity * 2.2f, 0.04f, 0.32f) * Alpha), X, Y, W, H);
	Gradient(X, Y, W * 0.3f, H, FLinearColor(0.f, 0.f, 0.f, 0.5f * Alpha), 0);
	Gradient(X + W * 0.7f, Y, W * 0.3f, H, FLinearColor(0.f, 0.f, 0.f, 0.5f * Alpha), 1);
	Gradient(X, Y + H * 0.55f, W, H * 0.45f, FLinearColor(0.f, 0.f, 0.f, 0.45f * Alpha), 3);
	Frame(X, Y, W, H, FLinearColor(1.f, 1.f, 1.f, 0.06f * Alpha), FMath::Max(1.f, U));
}

void ABRHUD::DrawLevelCard(int32 Index, float CX, float Top, float Scale, float Alpha, float Sel, bool bLocked, bool bCurrent)
{
	const TArray<FBRLevelDef>& All = BRLevels::All();
	if (!All.IsValidIndex(Index) || Alpha <= 0.003f)
	{
		return;
	}
	const FBRLevelDef& D = All[Index];
	const float U = Ui();
	const float S = Scale;
	const float W = 294.f * U * S;
	const float H = 392.f * U * S;
	const float X = CX - W * 0.5f;
	const float Y = Top + (392.f * U - H) * 0.5f;
	const float R = 16.f * U * S;
	const FLinearColor Accent = bLocked ? FLinearColor(0.55f, 0.52f, 0.46f, 1.f) : Yellow;

	Glow(CX, Y + H * 0.5f, W * 0.95f, H * 0.75f, FLinearColor(Accent.R, Accent.G * 0.95f, Accent.B, (bLocked ? 0.08f : 0.16f) * Sel * Alpha));
	RoundRect(X + 4.f * U * S, Y + 10.f * U * S, W, H, R, FLinearColor(0.f, 0.f, 0.f, 0.5f * Alpha));
	RoundRect(X, Y, W, H, R, bLocked ? FLinearColor(0.035f, 0.033f, 0.03f, 0.97f * Alpha) : FLinearColor(0.06f, 0.052f, 0.035f, 0.97f * Alpha));

	const float Pad = 10.f * U * S;
	const float PW = W - 2.f * Pad;
	const float PH = H * 0.52f;
	const float PX = X + Pad;
	const float PY = Y + Pad;
	if (bLocked)
	{
		// Niveau pas encore explore : ecran neigeux, cadenas
		DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, Alpha), PX, PY, PW, PH);
		const float Step = FMath::Max(2.f, 3.f * U * S);
		for (float LY = PY; LY < PY + PH; LY += Step)
		{
			const float N = FMath::Frac(FMath::Sin((LY - PY) * 12.9898f + Index * 78.233f + FMath::FloorToFloat(Clock * 12.f) * 3.7f) * 43758.5453f);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, (0.015f + 0.05f * N) * Alpha), PX, LY, PW, FMath::Max(1.f, U));
		}
		Gradient(PX, PY, PW, PH * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.5f * Alpha), 2);
		Gradient(PX, PY + PH * 0.5f, PW, PH * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.5f * Alpha), 3);
		const float LD = 64.f * U * S;
		RoundRect(CX - LD * 0.5f, PY + (PH - LD) * 0.5f, LD, LD, LD * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.55f * Alpha));
		RoundRect(CX - LD * 0.5f, PY + (PH - LD) * 0.5f, LD, LD, LD * 0.5f, FLinearColor(1.f, 1.f, 1.f, 0.18f * Alpha), true);
		if (UTexture* T = UiTex(TEXT("UI_IconLock")))
		{
			DrawTexture(T, CX - LD * 0.25f, PY + (PH - LD) * 0.5f + LD * 0.22f, LD * 0.5f, LD * 0.5f, 0.f, 0.f, 1.f, 1.f, FLinearColor(0.85f, 0.82f, 0.75f, 0.85f * Alpha),
				BLEND_Translucent);
		}
		Frame(PX, PY, PW, PH, FLinearColor(1.f, 1.f, 1.f, 0.06f * Alpha), FMath::Max(1.f, U));
	}
	else
	{
		DrawLevelScene(D, PX, PY, PW, PH, S, Alpha);
	}
	if (bCurrent)
	{
		// La ou la partie s'est arretee
		const FString Badge = TEXT("DERNI\u00c8RE POSITION");
		const FVector2f BS = TextSize(Badge, 8.5f * S, EUiWeight::Bold);
		const float BH = 20.f * U * S;
		const float BW = BS.X + 22.f * U * S;
		RoundRect(CX - BW * 0.5f, PY + 8.f * U * S, BW, BH, BH * 0.5f, WithAlpha(Yellow, 0.95f * Alpha));
		TextF(Badge, CX, PY + 8.f * U * S + (BH - BS.Y) * 0.5f, FLinearColor(0.07f, 0.055f, 0.02f, Alpha), 8.5f * S, EUiWeight::Bold, EUiAlign::Center, false);
	}

	const FLinearColor Dim = bLocked ? FLinearColor(0.55f, 0.53f, 0.48f, 0.8f) : InkDim;
	float TY = PY + PH + 12.f * U * S;
	TextSpaced(TEXT("NIVEAU"), CX, TY, WithAlpha(Dim, Alpha), 9.5f * S, EUiWeight::Light, 4.f * U * S, EUiAlign::Center);
	TY += 15.f * U * S;
	const FLinearColor NumC = bLocked ? FLinearColor(0.5f, 0.48f, 0.43f, 1.f) : Mix(Ink, Yellow, Sel);
	TextF(FString::FromInt(D.Number), CX, TY, WithAlpha(NumC, Alpha), 40.f * S, EUiWeight::Black, EUiAlign::Center);
	TY += TextSize(TEXT("0"), 40.f * S, EUiWeight::Black).Y - 6.f * U * S;
	if (bLocked)
	{
		TextF(TEXT("? ? ?"), CX, TY, WithAlpha(Dim, Alpha), 14.f * S, EUiWeight::Bold, EUiAlign::Center, false);
		TY += 21.f * U * S;
		TextF(TEXT("Non explor\u00e9"), CX, TY, WithAlpha(Dim, Alpha), 11.f * S, EUiWeight::Regular, EUiAlign::Center, false);
	}
	else
	{
		TextF(Ellipsize(D.Title, PW, 14.f * S, EUiWeight::Bold), CX, TY, WithAlpha(Ink, Alpha), 14.f * S, EUiWeight::Bold, EUiAlign::Center, false);
		TY += 21.f * U * S;
		TextF(Ellipsize(D.Nickname, PW, 11.f * S, EUiWeight::Regular), CX, TY, WithAlpha(InkDim, Alpha), 11.f * S, EUiWeight::Regular, EUiAlign::Center, false);
	}

	// Classe de survie (inconnue tant que le niveau n'est pas explore)
	const FString Cls = bLocked ? FString(TEXT("VERROUILL\u00c9")) : ClassShort(D.ClassText);
	const FLinearColor CC = bLocked ? FLinearColor(0.6f, 0.57f, 0.52f, 1.f) : ClassColor(D.SurvivalClass);
	const FVector2f CS = TextSize(Cls, 9.5f * S, EUiWeight::Bold);
	const float PillH = 22.f * U * S;
	const float PillW = CS.X + 34.f * U * S;
	const float PillY = Y + H - PillH - 14.f * U * S;
	RoundRect(CX - PillW * 0.5f, PillY, PillW, PillH, PillH * 0.5f, FLinearColor(CC.R, CC.G, CC.B, 0.16f * Alpha));
	const float Dot = 7.f * U * S;
	RoundRect(CX - PillW * 0.5f + 11.f * U * S, PillY + (PillH - Dot) * 0.5f, Dot, Dot, Dot * 0.5f, WithAlpha(CC, Alpha));
	TextF(Cls, CX + 6.f * U * S, PillY + (PillH - CS.Y) * 0.5f, WithAlpha(CC, Alpha), 9.5f * S, EUiWeight::Bold, EUiAlign::Center, false);

	if (Sel > 0.01f)
	{
		RoundRect(X - 3.f * U, Y - 3.f * U, W + 6.f * U, H + 6.f * U, R + 3.f * U, WithAlpha(Accent, Sel * Alpha), true);
	}
	else
	{
		RoundRect(X, Y, W, H, R, FLinearColor(1.f, 0.9f, 0.6f, 0.1f * Alpha), true);
	}
}

void ABRHUD::DrawMenuSolo(ABRPlayerController* PC, bool bInteractive)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	const float X0 = FMath::Max(60.f * U, W * 0.0625f);
	const TArray<FBRLevelDef>& All = BRLevels::All();
	const int32 Num = All.Num();
	if (Num == 0)
	{
		return;
	}
	const int32 Sel = FMath::Clamp(PC->GetMenuIndex(), 0, Num - 1);
	const float In = EaseOut(MenuPageTime / 0.45f);
	const UBRSaveGame* Save = PC->GetActiveSave();
	int32 ExploredCount = 0;
	for (const FBRLevelDef& L : All)
	{
		ExploredCount += PC->IsLevelUnlocked(L.Number) ? 1 : 0;
	}

	// En-tete : partie choisie et progression
	DrawLogo(X0 - 240.f * U * 0.03f, 46.f * U, 240.f * U, In, false);
	TextSpaced(PC->IsHostFlow() ? TEXT("H\u00c9BERGER : CHOISISSEZ UN NIVEAU") : TEXT("CHOISISSEZ UN NIVEAU"), CX, 100.f * U, WithAlpha(Yellow, In), 13.f, EUiWeight::Bold,
		5.f * U, EUiAlign::Center);
	const FString Progress = FString::Printf(TEXT("%s  \u00b7  %d / %d niveaux explor\u00e9s"),
		Save ? *FString::Printf(TEXT("\u00ab %s \u00bb"), *Save->SaveName) : TEXT("Sans partie"), ExploredCount, Num);
	TextF(Progress, CX, 126.f * U, WithAlpha(Ink, 0.9f * In), 12.5f, EUiWeight::Regular, EUiAlign::Center);
	{
		const float BW = 320.f * U;
		const float BY = 152.f * U;
		RoundRect(CX - BW * 0.5f, BY, BW, 4.f * U, 2.f * U, FLinearColor(1.f, 1.f, 1.f, 0.12f * In));
		RoundRect(CX - BW * 0.5f, BY, FMath::Max(4.f * U, BW * ExploredCount / Num), 4.f * U, 2.f * U, WithAlpha(Yellow, In));
	}

	// Carrousel : sa position suit la selection par le plus court chemin (la liste boucle)
	if (Carousel < -500.f)
	{
		Carousel = static_cast<float>(Sel);
	}
	float Delta = static_cast<float>(Sel) - Carousel;
	Delta -= Num * FMath::RoundToFloat(Delta / Num);
	Carousel += Delta * FMath::Min(1.f, UiDt * 11.f);
	Carousel = FMath::Fmod(Carousel + Num, static_cast<float>(Num));

	const float Top = 182.f * U;
	const float Spacing = 336.f * U;
	TArray<FCardDraw> Cards;
	const int32 Center = FMath::RoundToInt(Carousel);
	for (int32 k = -3; k <= 3; ++k)
	{
		const float Off = static_cast<float>(Center + k) - Carousel;
		if (FMath::Abs(Off) <= 2.6f)
		{
			Cards.Add({ ((Center + k) % Num + Num) % Num, Off });
		}
	}
	// Les plus eloignees d'abord : la carte centrale passe devant
	Cards.Sort([](const FCardDraw& L, const FCardDraw& R) { return FMath::Abs(L.Off) > FMath::Abs(R.Off); });
	for (const FCardDraw& Cd : Cards)
	{
		const float AOff = FMath::Abs(Cd.Off);
		const float Scale = 1.f - 0.15f * FMath::Min(AOff, 2.f);
		const float Alpha = In * FMath::Clamp(1.f - 0.36f * AOff, 0.f, 1.f) * FMath::Clamp((2.6f - AOff) / 0.4f, 0.f, 1.f);
		const float CardCX = CX + Cd.Off * Spacing * (1.f - 0.06f * AOff);
		const int32 LevelNum = All[Cd.Index].Number;
		DrawLevelCard(Cd.Index, CardCX, Top, Scale, Alpha, FMath::Clamp(1.f - AOff * 2.f, 0.f, 1.f), !PC->IsLevelUnlocked(LevelNum),
			Save && Save->CurrentLevel == LevelNum);
		if (bInteractive && AOff < 2.2f)
		{
			// Ecart avec la selection (la liste boucle) : un clic sur une carte voisine la selectionne
			int32 Rel = Cd.Index - Sel;
			Rel -= Num * FMath::RoundToInt(static_cast<float>(Rel) / Num);
			if (FMath::Abs(Rel) <= 3)
			{
				const float CW = 294.f * U * Scale;
				const float CH = 392.f * U * Scale;
				AddButton(Btn_MenuCard + 3 + Rel, CardCX - CW * 0.5f, Top + (392.f * U - CH) * 0.5f, CW, CH);
			}
		}
	}

	// Pagination : fleches et points (pleins : explores, creux : verrouilles)
	const float PagerY = Top + 392.f * U + 30.f * U;
	const float Dot = 8.f * U;
	const float DotGap = 9.f * U;
	const float Wide = 16.f * U;
	const float DotsW = Num * Dot + (Num - 1) * DotGap + Wide;
	float DX = CX - DotsW * 0.5f;
	for (int32 i = 0; i < Num; ++i)
	{
		const bool bOpen = PC->IsLevelUnlocked(All[i].Number);
		const float DW = i == Sel ? Dot + Wide : Dot;
		const FLinearColor DC = i == Sel ? (bOpen ? WithAlpha(Yellow, In) : FLinearColor(0.6f, 0.57f, 0.52f, In))
										 : FLinearColor(1.f, 1.f, 1.f, (bOpen ? 0.5f : 0.14f) * In);
		RoundRect(DX, PagerY - Dot * 0.5f, DW, Dot, Dot * 0.5f, DC);
		DX += DW + DotGap;
	}
	const float AD = 38.f * U;
	for (int32 k = 0; k < 2; ++k)
	{
		const float AX = k == 0 ? CX - DotsW * 0.5f - 26.f * U - AD : CX + DotsW * 0.5f + 26.f * U;
		const float AY = PagerY - AD * 0.5f;
		const bool bHov = bInteractive && Hover(AX, AY, AD, AD);
		RoundRect(AX, AY, AD, AD, AD * 0.5f, bHov ? WithAlpha(Yellow, 0.95f * In) : FLinearColor(0.05f, 0.045f, 0.03f, 0.8f * In));
		RoundRect(AX, AY, AD, AD, AD * 0.5f, FLinearColor(1.f, 0.88f, 0.5f, 0.25f * In), true);
		if (UTexture* T = UiTex(TEXT("UI_IconArrow")))
		{
			const float IS = AD * 0.46f;
			DrawTexture(T, AX + (AD - IS) * 0.5f, AY + (AD - IS) * 0.5f, IS, IS, k == 0 ? 1.f : 0.f, 0.f, k == 0 ? -1.f : 1.f, 1.f,
				bHov ? FLinearColor(0.07f, 0.055f, 0.02f, In) : WithAlpha(Yellow, In), BLEND_Translucent);
		}
		if (bInteractive)
		{
			AddButton(k == 0 ? Btn_MenuLevelPrev : Btn_MenuLevelNext, AX, AY, AD, AD);
		}
	}

	// Details du niveau choisi
	const FBRLevelDef& D = All[Sel];
	const bool bLocked = !PC->IsLevelUnlocked(D.Number);
	float Y = PagerY + 34.f * U;
	if (bLocked)
	{
		TextSpaced(TEXT("NIVEAU NON EXPLOR\u00c9"), CX, Y, FLinearColor(0.75f, 0.72f, 0.65f, In), 12.f, EUiWeight::Bold, 3.f * U, EUiAlign::Center);
		Y += 30.f * U;
		TextF(TEXT("Vous n'avez pas encore atteint ce niveau dans cette partie."), CX, Y, WithAlpha(Ink, 0.85f * In), 13.5f, EUiWeight::Regular, EUiAlign::Center);
		Y += 22.f * U;
		TextF(TEXT("Trouvez en jeu une sortie qui y m\u00e8ne : il deviendra s\u00e9lectionnable ici."), CX, Y, WithAlpha(InkDim, In), 13.f, EUiWeight::Light,
			EUiAlign::Center);
		Y += 22.f * U;
	}
	else
	{
		TextF(D.ClassText, CX, Y, WithAlpha(ClassColor(D.SurvivalClass), In), 12.5f, EUiWeight::Bold, EUiAlign::Center);
		Y += 28.f * U;
		const TArray<FString> Lines = WrapF(D.Description, FMath::Min(880.f * U, W - 160.f * U), 13.5f, EUiWeight::Regular);
		for (int32 i = 0; i < Lines.Num() && i < 3; ++i)
		{
			TextF(Lines[i], CX, Y, WithAlpha(Ink, 0.9f * In), 13.5f, EUiWeight::Regular, EUiAlign::Center);
			Y += 22.f * U;
		}
		Y += 6.f * U;
		if (D.bRequireObjectives)
		{
			TextF(FString::Printf(TEXT("Objectifs : %d cassettes VHS + filmer pendant une coupure de courant"), D.VHSRequired), CX, Y,
				WithAlpha(Yellow, In), 12.f, EUiWeight::Regular, EUiAlign::Center);
			Y += 21.f * U;
		}
		TArray<FString> Names;
		for (const FBREntitySpawn& E : D.Entities)
		{
			Names.AddUnique(ABREntity::Info(E.Kind).Name);
		}
		if (D.bPatrolEntity)
		{
			Names.AddUnique(ABREntity::Info(D.PatrolKind).Name);
		}
		if (D.BlackoutSmilers > 0)
		{
			Names.AddUnique(ABREntity::Info(EBREntityKind::Smiler).Name);
		}
		TextF(Names.Num() > 0 ? TEXT("Entit\u00e9s : ") + FString::Join(Names, TEXT("  \u00b7  ")) : FString(TEXT("Aucune entit\u00e9 signal\u00e9e")), CX, Y,
			WithAlpha(InkDim, In), 12.f, EUiWeight::Regular, EUiAlign::Center);
		Y += 21.f * U;
		// Sorties connues : un niveau deja explore est nomme, les autres restent un mystere
		TArray<FString> Exits;
		for (const FBRExitDef& X : D.Exits)
		{
			Exits.AddUnique(X.Target < 0 ? FString(TEXT("al\u00e9atoire")) : (PC->IsLevelUnlocked(X.Target) ? FString::Printf(TEXT("Niveau %d"), X.Target)
				: FString(TEXT("???"))));
		}
		if (Exits.Num() > 0)
		{
			TextF(TEXT("Sorties : ") + FString::Join(Exits, TEXT("  \u00b7  ")), CX, Y, WithAlpha(InkDim, 0.85f * In), 12.f, EUiWeight::Regular, EUiAlign::Center);
			Y += 21.f * U;
		}
	}

	// NOCLIPPER (ou HEBERGER) / RETOUR
	const float BH = 60.f * U;
	const float PW0 = 300.f * U;
	const float PW1 = 210.f * U;
	const float BG = 18.f * U;
	const float BX = CX - (PW0 + PW1 + BG) * 0.5f;
	const float BY = FMath::Max(836.f * U, Y + 18.f * U);
	MenuPill(0, BX, BY, PW0, BH, bLocked ? TEXT("UI_IconLock") : TEXT("UI_IconPlay"), true, bInteractive, In, false, bLocked);
	MenuPill(1, BX + PW0 + BG, BY, PW1, BH, TEXT("UI_IconBack"), false, bInteractive, In);

	TextF(ControlsLine(0), CX, H - 134.f * U, WithAlpha(InkDim, 0.75f * In), 10.5f, EUiWeight::Regular, EUiAlign::Center, false);
	TextF(ControlsLine(1), CX, H - 112.f * U, WithAlpha(InkDim, 0.75f * In), 10.5f, EUiWeight::Regular, EUiAlign::Center, false);
	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(TEXT("\u2190 \u2192"), TEXT("Niveau"));
	Hints.Emplace(TEXT("\u2191 \u2193"), TEXT("Choisir"));
	Hints.Emplace(TEXT("ENTR\u00c9E"), TEXT("Valider"));
	Hints.Emplace(TEXT("\u00c9CHAP"), TEXT("Parties"));
	KeyHints(CX, H - 76.f * U, Hints, In, true);
}

void ABRHUD::DrawMenuMulti(ABRPlayerController* PC, bool bInteractive)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float X0 = FMath::Max(60.f * U, W * 0.0625f);
	const float In = EaseOut(MenuPageTime / 0.45f);

	DrawLogo(X0 - 240.f * U * 0.03f, 46.f * U, 240.f * U, In, false);
	TextF(TEXT("MULTIJOUEUR"), X0, 124.f * U, WithAlpha(Ink, In), 32.f, EUiWeight::Black);
	TextF(TEXT("Coop\u00e9ration jusqu'\u00e0 4 explorateurs  \u00b7  chat vocal de proximit\u00e9"), X0, 178.f * U, WithAlpha(InkDim, In), 13.f, EUiWeight::Light);

	const TCHAR* Subs[] = {
		TEXT("Cr\u00e9er la partie sur ce PC (port 7777)"),
		TEXT("Entrer l'adresse IP de l'h\u00f4te"),
		TEXT("Revenir au menu principal"),
	};
	const TCHAR* Icons[] = { TEXT("UI_IconHost"), TEXT("UI_IconJoin"), TEXT("UI_IconBack") };
	const float CW = 540.f * U;
	const float CH = 90.f * U;
	float Y = 250.f * U;
	for (int32 i = 0; i < PC->GetMenuItemCount() && i < 3; ++i)
	{
		MenuCard(i, X0, Y, CW, CH, Subs[i], Icons[i], bInteractive, EaseOut((MenuPageTime - 0.07f * i) / 0.45f));
		Y += CH + 14.f * U;
	}

	// Colonne de droite : qui doit heberger, niveau de depart, adresse IP
	const float PX = FMath::Max(X0 + CW + 70.f * U, W - 56.f * U - 600.f * U);
	const float PW = FMath::Max(320.f * U, FMath::Min(600.f * U, W - PX - 56.f * U));
	const float A = EaseOut((MenuPageTime - 0.15f) / 0.5f);
	float PY = 250.f * U;
	{
		const TArray<FString> Lines = WrapF(TEXT("Le joueur qui a l'ordinateur le plus puissant (et la meilleure connexion). Son PC fait tourner le monde, les entit\u00e9s et leurs d\u00e9placements pour tout le groupe ; les autres le rejoignent avec son adresse IP."),
			PW - 96.f * U, 12.5f, EUiWeight::Regular);
		const float BoxH = 54.f * U + Lines.Num() * 20.f * U + 14.f * U;
		RoundRect(PX, PY, PW, BoxH, 14.f * U, FLinearColor(0.17f, 0.13f, 0.02f, 0.75f * A));
		RoundRect(PX, PY, PW, BoxH, 14.f * U, WithAlpha(Yellow, 0.35f * A), true);
		const float D = 44.f * U;
		RoundRect(PX + 20.f * U, PY + 18.f * U, D, D, D * 0.5f, WithAlpha(Yellow, 0.16f * A));
		if (UTexture* T = UiTex(TEXT("UI_IconHost")))
		{
			DrawTexture(T, PX + 20.f * U + D * 0.2f, PY + 18.f * U + D * 0.2f, D * 0.6f, D * 0.6f, 0.f, 0.f, 1.f, 1.f, WithAlpha(Yellow, A), BLEND_Translucent);
		}
		TextSpaced(TEXT("QUI DOIT H\u00c9BERGER ?"), PX + 80.f * U, PY + 22.f * U, WithAlpha(Yellow, A), 11.f, EUiWeight::Bold, 2.5f * U);
		float LY = PY + 48.f * U;
		for (const FString& L : Lines)
		{
			TextF(L, PX + 80.f * U, LY, WithAlpha(Ink, 0.92f * A), 12.5f, EUiWeight::Regular, EUiAlign::Left, false);
			LY += 20.f * U;
		}
		PY += BoxH + 16.f * U;
	}
	{
		// Partie hebergee : choisie a l'etape suivante, parmi vos sauvegardes
		const TArray<FString> Lines = WrapF(TEXT("Apr\u00e8s H\u00c9BERGER, choisissez une de vos parties puis un niveau d\u00e9j\u00e0 explor\u00e9. Les niveaux que le groupe d\u00e9couvre s'ajoutent \u00e0 votre partie ; vos amis jouent dans la v\u00f4tre."),
			PW - 96.f * U, 12.f, EUiWeight::Regular);
		const float BoxH = 54.f * U + Lines.Num() * 19.f * U + 12.f * U;
		RoundRect(PX, PY, PW, BoxH, 14.f * U, FLinearColor(0.05f, 0.045f, 0.03f, 0.78f * A));
		RoundRect(PX, PY, PW, BoxH, 14.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.12f * A), true);
		const float D = 44.f * U;
		RoundRect(PX + 20.f * U, PY + 18.f * U, D, D, D * 0.5f, WithAlpha(Yellow, 0.12f * A));
		if (UTexture* T = UiTex(TEXT("UI_IconSave")))
		{
			DrawTexture(T, PX + 20.f * U + D * 0.25f, PY + 18.f * U + D * 0.25f, D * 0.5f, D * 0.5f, 0.f, 0.f, 1.f, 1.f, WithAlpha(Yellow, A), BLEND_Translucent);
		}
		TextSpaced(TEXT("VOTRE PARTIE"), PX + 80.f * U, PY + 22.f * U, WithAlpha(InkDim, A), 10.f, EUiWeight::Bold, 3.f * U);
		float LY = PY + 46.f * U;
		for (const FString& L : Lines)
		{
			TextF(L, PX + 80.f * U, LY, WithAlpha(Ink, 0.9f * A), 12.f, EUiWeight::Regular, EUiAlign::Left, false);
			LY += 19.f * U;
		}
		PY += BoxH + 16.f * U;
	}
	{
		// Adresse IP a donner aux amis
		const FString Ip = PC->GetLocalAddress().IsEmpty() ? FString(TEXT("inconnue")) : PC->GetLocalAddress();
		const TArray<FString> Help = WrapF(TEXT("M\u00eame r\u00e9seau (LAN) : donnez cette adresse \u00e0 vos amis. Par Internet : redirigez le port UDP 7777 vers ce PC sur la box, ou utilisez un r\u00e9seau virtuel (Radmin VPN, ZeroTier, Tailscale\u2026) et son adresse. Chat vocal de proximit\u00e9 : Param\u00e8tres."),
			PW - 44.f * U, 11.5f, EUiWeight::Regular);
		const float BoxH = 100.f * U + Help.Num() * 18.f * U + 12.f * U;
		RoundRect(PX, PY, PW, BoxH, 14.f * U, FLinearColor(0.05f, 0.045f, 0.03f, 0.78f * A));
		RoundRect(PX, PY, PW, BoxH, 14.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.12f * A), true);
		TextSpaced(TEXT("VOTRE ADRESSE IP"), PX + 22.f * U, PY + 18.f * U, WithAlpha(InkDim, A), 10.f, EUiWeight::Bold, 3.f * U);
		const FVector2f IS = TextSize(Ip, 26.f, EUiWeight::Black);
		TextF(Ip, PX + 22.f * U, PY + 38.f * U, WithAlpha(Yellow, A), 26.f, EUiWeight::Black);
		TextF(TEXT("port 7777 (UDP)"), PX + 34.f * U + IS.X, PY + 38.f * U + IS.Y - 30.f * U, WithAlpha(InkDim, A), 12.f, EUiWeight::Light, EUiAlign::Left, false);
		float LY = PY + 100.f * U;
		for (const FString& L : Help)
		{
			TextF(L, PX + 22.f * U, LY, WithAlpha(InkDim, 0.95f * A), 11.5f, EUiWeight::Regular, EUiAlign::Left, false);
			LY += 18.f * U;
		}
	}

	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(TEXT("\u2191 \u2193"), TEXT("Choisir"));
	Hints.Emplace(TEXT("ENTR\u00c9E"), TEXT("Valider"));
	Hints.Emplace(TEXT("\u00c9CHAP"), TEXT("Retour"));
	KeyHints(X0, H - 72.f * U, Hints, In, false);
}

void ABRHUD::DrawMenuJoin(ABRPlayerController* PC, bool bInteractive)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	const float CY = H * 0.5f;
	const float X0 = FMath::Max(60.f * U, W * 0.0625f);
	const float In = EaseOut(MenuPageTime / 0.4f);

	DrawLogo(X0 - 240.f * U * 0.03f, 46.f * U, 240.f * U, In, false);
	// Carte centrale autour du champ de saisie (widget Slate de 520 x 52, centre a l'ecran)
	const float CW = 700.f * U;
	const float CH = 384.f * U;
	const float CardX = CX - CW * 0.5f;
	const float CardY = CY - 196.f * U + (1.f - In) * 24.f * U;
	Glow(CX, CY, CW * 0.8f, CH * 0.9f, FLinearColor(1.f, 0.8f, 0.3f, 0.06f * In));
	RoundRect(CardX, CardY + 6.f * U, CW, CH, 22.f * U, FLinearColor(0.f, 0.f, 0.f, 0.35f * In));
	RoundRect(CardX, CardY, CW, CH, 22.f * U, FLinearColor(0.05f, 0.045f, 0.03f, 0.92f * In));
	RoundRect(CardX, CardY, CW, CH, 22.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.16f * In), true);
	// Badge rond a cheval sur le bord superieur
	const float BD = 64.f * U;
	RoundRect(CX - BD * 0.5f, CardY - BD * 0.5f, BD, BD, BD * 0.5f, WithAlpha(Yellow, In));
	if (UTexture* T = UiTex(TEXT("UI_IconJoin")))
	{
		DrawTexture(T, CX - BD * 0.3f, CardY - BD * 0.3f, BD * 0.6f, BD * 0.6f, 0.f, 0.f, 1.f, 1.f, FLinearColor(0.07f, 0.055f, 0.02f, In), BLEND_Translucent);
	}
	TextF(TEXT("REJOINDRE UNE PARTIE"), CX, CardY + 46.f * U, WithAlpha(Ink, In), 24.f, EUiWeight::Black, EUiAlign::Center);
	TextF(TEXT("Entrez l'adresse IP de l'h\u00f4te (il la voit dans son menu MULTIJOUEUR)"), CX, CardY + 92.f * U, WithAlpha(InkDim, In), 12.5f,
		EUiWeight::Light, EUiAlign::Center);
	const float FW = 548.f * U;
	const float FH = 68.f * U;
	RoundRect(CX - FW * 0.5f, CY - FH * 0.5f, FW, FH, 12.f * U, FLinearColor(0.f, 0.f, 0.f, 0.45f * In));
	RoundRect(CX - FW * 0.5f, CY - FH * 0.5f, FW, FH, 12.f * U, FLinearColor(1.f, 0.84f, 0.3f, (0.5f + 0.25f * FMath::Sin(Clock * 3.f)) * In), true);

	const float BH = 56.f * U;
	const float PW0 = 260.f * U;
	const float PW1 = 180.f * U;
	const float BG = 16.f * U;
	const float BX = CX - (PW0 + PW1 + BG) * 0.5f;
	MenuPill(0, BX, CY + 66.f * U, PW0, BH, TEXT("UI_IconPlay"), true, bInteractive, In);
	MenuPill(1, BX + PW0 + BG, CY + 66.f * U, PW1, BH, TEXT("UI_IconBack"), false, bInteractive, In);
	TextF(TEXT("Exemples :  192.168.1.20   \u00b7   26.45.120.7:7777"), CX, CardY + CH - 40.f * U, WithAlpha(InkDim, 0.85f * In), 11.5f,
		EUiWeight::Regular, EUiAlign::Center, false);

	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(TEXT("ENTR\u00c9E"), TEXT("Se connecter"));
	Hints.Emplace(TEXT("\u00c9CHAP"), TEXT("Retour"));
	KeyHints(CX, H - 76.f * U, Hints, In, true);
}

void ABRHUD::DrawSaveCard(int32 Item, const UBRSaveGame* Save, float X, float Y, float W, float H, bool bInteractive, float Appear)
{
	const ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	if (!PC || !Save)
	{
		return;
	}
	const float U = Ui();
	const float Ap = FMath::Clamp(Appear, 0.f, 1.f);
	float& Sel = MenuSel[FMath::Clamp(Item, 0, 7)];
	Sel = FMath::FInterpTo(Sel, PC->GetMenuCursor() == Item && !PC->IsConfirmingDelete() ? 1.f : 0.f, UiDt, 14.f);
	const float S = Smooth(Sel);
	const FLinearColor DarkInk(0.07f, 0.055f, 0.02f, 1.f);
	const float CX0 = X - (1.f - Ap) * 40.f * U + S * 14.f * U;
	const float R = 14.f * U;
	const TArray<FBRLevelDef>& All = BRLevels::All();
	const int32 LevelIdx = FMath::Max(0, BRLevels::IndexOf(Save->CurrentLevel));
	const FBRLevelDef& D = All[FMath::Clamp(LevelIdx, 0, All.Num() - 1)];

	Glow(CX0 + W * 0.42f, Y + H * 0.5f, W * 0.75f, H * 1.5f, WithAlpha(Yellow, 0.16f * S * Ap));
	RoundRect(CX0, Y + 4.f * U, W, H, R, FLinearColor(0.f, 0.f, 0.f, 0.3f * Ap));
	RoundRect(CX0, Y, W, H, R, WithAlpha(Mix(FLinearColor(0.05f, 0.045f, 0.03f, 0.8f), FLinearColor(1.f, 0.82f, 0.22f, 0.97f), S), Ap));
	RoundRect(CX0, Y, W, H, R, FLinearColor(1.f, 0.88f, 0.5f, 0.13f * (1.f - S) * Ap), true);
	Gradient(CX0 + R, Y + 1.f * U, W - 2.f * R, H * 0.45f, FLinearColor(1.f, 1.f, 1.f, (0.03f + 0.07f * S) * Ap), 2);

	// Pastille : numero du dernier niveau atteint
	const float DD = H - 28.f * U;
	const float DX = CX0 + 16.f * U;
	const float DY = Y + (H - DD) * 0.5f - 2.f * U;
	RoundRect(DX, DY, DD, DD, DD * 0.5f, WithAlpha(Mix(FLinearColor(1.f, 0.82f, 0.22f, 0.13f), FLinearColor(0.07f, 0.055f, 0.02f, 0.92f), S), Ap));
	const FString Num = FString::FromInt(Save->CurrentLevel);
	const FVector2f NS = TextSize(Num, 17.f, EUiWeight::Black);
	TextF(Num, DX + DD * 0.5f, DY + (DD - NS.Y) * 0.5f, WithAlpha(Yellow, Ap), 17.f, EUiWeight::Black, EUiAlign::Center, false);

	// Nom, dernier niveau ; a droite : progression, temps de jeu, date
	const float TX = DX + DD + 18.f * U;
	const FLinearColor LabelC = Mix(Ink, DarkInk, S);
	const FLinearColor SubC = Mix(InkDim, FLinearColor(0.14f, 0.11f, 0.04f, 0.9f), S);
	const FVector2f LS = TextSize(Save->SaveName, 18.f, EUiWeight::Bold);
	const float TY = Y + (H - LS.Y - 16.f * U) * 0.5f - 2.f * U;
	const float RightW = 170.f * U;
	TextF(Ellipsize(Save->SaveName, W - (TX - CX0) - RightW - 20.f * U, 18.f, EUiWeight::Bold), TX, TY, WithAlpha(LabelC, Ap), 18.f, EUiWeight::Bold,
		EUiAlign::Left, S < 0.5f);
	TextF(Ellipsize(FString::Printf(TEXT("Niveau %d  \u00b7  %s"), D.Number, *D.Title), W - (TX - CX0) - RightW - 20.f * U, 11.5f, EUiWeight::Regular), TX,
		TY + LS.Y - 3.f * U, WithAlpha(SubC, Ap), 11.5f, EUiWeight::Regular, EUiAlign::Left, false);
	const float RX = CX0 + W - 22.f * U;
	TextF(FString::Printf(TEXT("%d / %d niveaux"), Save->Explored.Num(), All.Num()), RX, TY + 2.f * U, WithAlpha(Mix(Yellow, DarkInk, S), Ap), 11.f,
		EUiWeight::Bold, EUiAlign::Right, false);
	TextF(BRSaves::FormatPlayTime(Save->PlayTime) + TEXT("  \u00b7  ") + BRSaves::FormatDate(Save->LastPlayed), RX, TY + LS.Y - 3.f * U, WithAlpha(SubC, Ap),
		10.f, EUiWeight::Regular, EUiAlign::Right, false);

	// Barre de progression (niveaux explores) au bas de la carte
	const float BarX = CX0 + R;
	const float BarW = W - 2.f * R;
	const float BarY = Y + H - 9.f * U;
	RoundRect(BarX, BarY, BarW, 3.f * U, 1.5f * U, FLinearColor(S > 0.5f ? 0.f : 1.f, S > 0.5f ? 0.f : 1.f, S > 0.5f ? 0.f : 1.f, 0.12f * Ap));
	RoundRect(BarX, BarY, FMath::Max(3.f * U, BarW * Save->Explored.Num() / FMath::Max(1, All.Num())), 3.f * U, 1.5f * U,
		WithAlpha(Mix(Yellow, DarkInk, S), 0.9f * Ap));
	if (bInteractive && Ap > 0.5f)
	{
		AddButton(Btn_Menu + Item, X, Y, W, H);
	}
}

void ABRHUD::DrawLevelGrid(const UBRSaveGame* Save, float X, float Y, float W, float A)
{
	// Les 12 niveaux en vignettes : apercu des niveaux explores, cadenas pour les autres
	const ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	const TArray<FBRLevelDef>& All = BRLevels::All();
	const float U = Ui();
	const int32 Cols = 4;
	const float Gap = 10.f * U;
	const float TW = (W - (Cols - 1) * Gap) / Cols;
	const float TH = TW * 0.62f;
	for (int32 i = 0; i < All.Num(); ++i)
	{
		const FBRLevelDef& D = All[i];
		const float TX = X + (i % Cols) * (TW + Gap);
		const float TY = Y + (i / Cols) * (TH + Gap);
		const bool bOpen = Save ? Save->IsExplored(D.Number) : (PC && PC->IsLevelUnlocked(D.Number));
		if (bOpen)
		{
			DrawLevelScene(D, TX, TY, TW, TH, 0.45f, A);
		}
		else
		{
			DrawRect(FLinearColor(0.03f, 0.03f, 0.028f, 0.9f * A), TX, TY, TW, TH);
			if (UTexture* T = UiTex(TEXT("UI_IconLock")))
			{
				const float LS = TH * 0.34f;
				DrawTexture(T, TX + (TW - LS) * 0.5f, TY + (TH - LS) * 0.5f - 4.f * U, LS, LS, 0.f, 0.f, 1.f, 1.f, FLinearColor(0.6f, 0.57f, 0.52f, 0.6f * A),
					BLEND_Translucent);
			}
		}
		const FString Num = FString::FromInt(D.Number);
		const FVector2f NS = TextSize(Num, 10.f, EUiWeight::Black);
		RoundRect(TX + 5.f * U, TY + TH - NS.Y - 7.f * U, NS.X + 12.f * U, NS.Y + 2.f * U, 6.f * U, FLinearColor(0.f, 0.f, 0.f, 0.6f * A));
		TextF(Num, TX + 11.f * U, TY + TH - NS.Y - 6.f * U, bOpen ? WithAlpha(Yellow, A) : FLinearColor(0.6f, 0.57f, 0.52f, A), 10.f, EUiWeight::Black, EUiAlign::Left,
			false);
		const bool bHere = Save && Save->CurrentLevel == D.Number;
		Frame(TX, TY, TW, TH, bHere ? WithAlpha(Yellow, 0.95f * A) : FLinearColor(1.f, 1.f, 1.f, 0.08f * A), FMath::Max(1.f, (bHere ? 2.f : 1.f) * U));
	}
}

void ABRHUD::DrawMenuSaves(ABRPlayerController* PC, bool bInteractive)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float X0 = FMath::Max(60.f * U, W * 0.0625f);
	const float In = EaseOut(MenuPageTime / 0.45f);
	const bool bConfirm = PC->IsConfirmingDelete();

	DrawLogo(X0 - 240.f * U * 0.03f, 46.f * U, 240.f * U, In, false);
	TextF(PC->IsHostFlow() ? TEXT("H\u00c9BERGER UNE PARTIE") : TEXT("VOS PARTIES"), X0, 124.f * U, WithAlpha(Ink, In), 32.f, EUiWeight::Black);
	TextF(PC->IsHostFlow() ? TEXT("Choisissez la partie que le groupe va jouer : ses niveaux explor\u00e9s seront propos\u00e9s.")
						   : TEXT("Reprenez une partie, ou commencez-en une nouvelle (au Niveau 0)."),
		X0, 178.f * U, WithAlpha(InkDim, In), 13.f, EUiWeight::Light);

	// Liste : parties (de la plus recente a la plus ancienne), NOUVELLE PARTIE, RETOUR
	const float CW = 600.f * U;
	float Y = 236.f * U;
	// (pendant la confirmation d'une suppression, seules les parties restent affichees, inactives)
	const bool bListActive = bInteractive && !bConfirm;
	const int32 Count = bConfirm ? PC->GetSaveOrder().Num() : PC->GetMenuItemCount();
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Slot = PC->GetMenuSaveSlot(i);
		const float Appear = EaseOut((MenuPageTime - 0.06f * i) / 0.45f);
		if (Slot >= 0)
		{
			const float CH = 84.f * U;
			DrawSaveCard(i, PC->GetSaveInSlot(Slot), X0, Y, CW, CH, bListActive, Appear);
			// Corbeille a droite de la carte
			const float TD = 34.f * U;
			const float TX = X0 + CW + 30.f * U;
			const float TY = Y + (CH - TD) * 0.5f;
			const bool bHov = bListActive && Hover(TX, TY, TD, TD);
			const bool bShow = bHov || PC->GetMenuCursor() == i || Hover(X0, Y, CW, CH);
			RoundRect(TX, TY, TD, TD, TD * 0.5f, bHov ? FLinearColor(0.9f, 0.3f, 0.24f, 0.95f * Appear) : FLinearColor(0.05f, 0.045f, 0.03f, (bShow ? 0.8f : 0.4f) * Appear));
			if (UTexture* T = UiTex(TEXT("UI_IconTrash")))
			{
				DrawTexture(T, TX + TD * 0.27f, TY + TD * 0.25f, TD * 0.46f, TD * 0.46f, 0.f, 0.f, 1.f, 1.f,
					bHov ? FLinearColor(1.f, 1.f, 1.f, Appear) : FLinearColor(0.95f, 0.5f, 0.42f, (bShow ? 0.9f : 0.35f) * Appear), BLEND_Translucent);
			}
			if (bListActive)
			{
				AddButton(Btn_SaveDelete + Slot, TX, TY, TD, TD);
			}
			Y += CH + 10.f * U;
		}
		else
		{
			const float CH = 70.f * U;
			const bool bNew = Slot == ABRPlayerController::MenuItemNew;
			MenuCard(i, X0, Y, CW, CH, bNew ? TEXT("Commence au Niveau 0, avec l'\u00e9quipement de d\u00e9part") : (PC->IsHostFlow() ? TEXT("Multijoueur")
				: TEXT("Menu principal")), bNew ? TEXT("UI_IconPlus") : TEXT("UI_IconBack"), bInteractive, Appear);
			Y += CH + 10.f * U;
		}
	}

	// Panneau de droite : details de la partie sous le curseur (ou explication d'une nouvelle partie)
	const float PX = FMath::Max(X0 + CW + 90.f * U, W - 56.f * U - 560.f * U);
	const float PW = FMath::Max(300.f * U, FMath::Min(560.f * U, W - PX - 56.f * U));
	const float A = EaseOut((MenuPageTime - 0.15f) / 0.5f);
	const int32 Focus = PC->GetMenuSaveSlot(PC->GetMenuCursor());
	const UBRSaveGame* Shown = (!bConfirm && Focus >= 0) ? PC->GetSaveInSlot(Focus) : nullptr;
	float PY = 236.f * U;
	if (Shown)
	{
		const TArray<FBRLevelDef>& All = BRLevels::All();
		const FBRLevelDef& D = All[FMath::Clamp(BRLevels::IndexOf(Shown->CurrentLevel), 0, All.Num() - 1)];
		const float TileW = (PW - 44.f * U - 30.f * U) / 4.f;
		const float GridH = 3.f * TileW * 0.62f + 20.f * U;
		const float BoxH = 200.f * U + GridH;
		RoundRect(PX, PY, PW, BoxH, 16.f * U, FLinearColor(0.05f, 0.045f, 0.03f, 0.82f * A));
		RoundRect(PX, PY, PW, BoxH, 16.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.12f * A), true);
		TextF(Ellipsize(Shown->SaveName, PW - 44.f * U, 24.f, EUiWeight::Black), PX + 22.f * U, PY + 16.f * U, WithAlpha(Ink, A), 24.f, EUiWeight::Black);
		float LY = PY + 62.f * U;
		const FString Lines[] = {
			FString::Printf(TEXT("Derni\u00e8re position : Niveau %d  \u00b7  %s"), D.Number, *D.Title),
			FString::Printf(TEXT("Temps de jeu : %s  \u00b7  morts : %d  \u00b7  entit\u00e9s rencontr\u00e9es : %d / %d"), *BRSaves::FormatPlayTime(Shown->PlayTime),
				Shown->Deaths, Shown->Discovered.Num(), static_cast<int32>(EBREntityKind::Count)),
			FString::Printf(TEXT("Cr\u00e9\u00e9e le %s  \u00b7  jou\u00e9e le %s"), *BRSaves::FormatDate(Shown->Created).Left(10), *BRSaves::FormatDate(Shown->LastPlayed)),
		};
		for (const FString& L : Lines)
		{
			TextF(Ellipsize(L, PW - 44.f * U, 12.f, EUiWeight::Regular), PX + 22.f * U, LY, WithAlpha(InkDim, A), 12.f, EUiWeight::Regular, EUiAlign::Left, false);
			LY += 21.f * U;
		}
		LY += 14.f * U;
		TextSpaced(FString::Printf(TEXT("NIVEAUX EXPLOR\u00c9S  %d / %d"), Shown->Explored.Num(), All.Num()), PX + 22.f * U, LY, WithAlpha(Yellow, A), 10.5f,
			EUiWeight::Bold, 2.5f * U);
		DrawLevelGrid(Shown, PX + 22.f * U, LY + 26.f * U, PW - 44.f * U, A);
	}
	else if (!bConfirm && Focus == ABRPlayerController::MenuItemNew)
	{
		const TArray<FString> Lines = WrapF(TEXT("Vous commencez au Niveau 0, avec l'\u00e9quipement de d\u00e9part. Chaque niveau que vous d\u00e9couvrez en jeu devient s\u00e9lectionnable quand vous reprenez la partie. Sauvegarde automatique \u00e0 chaque niveau, puis toutes les minutes."),
			PW - 44.f * U, 12.5f, EUiWeight::Regular);
		const float BoxH = 112.f * U + Lines.Num() * 20.f * U;
		RoundRect(PX, PY, PW, BoxH, 16.f * U, FLinearColor(0.17f, 0.13f, 0.02f, 0.75f * A));
		RoundRect(PX, PY, PW, BoxH, 16.f * U, WithAlpha(Yellow, 0.35f * A), true);
		const float D = 44.f * U;
		RoundRect(PX + 22.f * U, PY + 20.f * U, D, D, D * 0.5f, WithAlpha(Yellow, 0.16f * A));
		if (UTexture* T = UiTex(TEXT("UI_IconPlus")))
		{
			DrawTexture(T, PX + 22.f * U + D * 0.25f, PY + 20.f * U + D * 0.25f, D * 0.5f, D * 0.5f, 0.f, 0.f, 1.f, 1.f, WithAlpha(Yellow, A), BLEND_Translucent);
		}
		TextSpaced(TEXT("NOUVELLE PARTIE"), PX + 80.f * U, PY + 24.f * U, WithAlpha(Yellow, A), 11.f, EUiWeight::Bold, 2.5f * U);
		TextF(FString::Printf(TEXT("Emplacements libres : %d / 6"), 6 - PC->GetSaveOrder().Num()), PX + 80.f * U, PY + 44.f * U, WithAlpha(InkDim, A), 11.f,
			EUiWeight::Regular, EUiAlign::Left, false);
		float LY = PY + 84.f * U;
		for (const FString& L : Lines)
		{
			TextF(L, PX + 22.f * U, LY, WithAlpha(Ink, 0.92f * A), 12.5f, EUiWeight::Regular, EUiAlign::Left, false);
			LY += 20.f * U;
		}
	}

	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(TEXT("\u2191 \u2193"), TEXT("Choisir"));
	Hints.Emplace(TEXT("ENTR\u00c9E"), TEXT("Ouvrir"));
	Hints.Emplace(TEXT("SUPPR"), TEXT("Supprimer"));
	Hints.Emplace(TEXT("\u00c9CHAP"), TEXT("Retour"));
	KeyHints(X0, H - 72.f * U, Hints, In, false);

	// Confirmation de suppression
	if (bConfirm)
	{
		const UBRSaveGame* Gone = PC->GetSaveInSlot(PC->GetDeleteSlot());
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, W, H);
		const float CW2 = 640.f * U;
		const float CH2 = 280.f * U;
		const float CX = W * 0.5f;
		const float CardY = H * 0.5f - CH2 * 0.5f;
		Glow(CX, H * 0.5f, CW2 * 0.8f, CH2, FLinearColor(0.9f, 0.3f, 0.24f, 0.08f));
		RoundRect(CX - CW2 * 0.5f, CardY, CW2, CH2, 22.f * U, FLinearColor(0.06f, 0.04f, 0.035f, 0.97f));
		RoundRect(CX - CW2 * 0.5f, CardY, CW2, CH2, 22.f * U, FLinearColor(0.9f, 0.35f, 0.28f, 0.45f), true);
		const float BD = 60.f * U;
		RoundRect(CX - BD * 0.5f, CardY - BD * 0.5f, BD, BD, BD * 0.5f, FLinearColor(0.9f, 0.3f, 0.24f, 1.f));
		if (UTexture* T = UiTex(TEXT("UI_IconTrash")))
		{
			DrawTexture(T, CX - BD * 0.28f, CardY - BD * 0.28f, BD * 0.56f, BD * 0.56f, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, BLEND_Translucent);
		}
		TextF(TEXT("SUPPRIMER LA PARTIE ?"), CX, CardY + 46.f * U, Ink, 22.f, EUiWeight::Black, EUiAlign::Center);
		if (Gone)
		{
			TextF(FString::Printf(TEXT("\u00ab %s \u00bb sera effac\u00e9e d\u00e9finitivement"), *Gone->SaveName), CX, CardY + 92.f * U, InkDim, 13.f, EUiWeight::Regular,
				EUiAlign::Center);
			const int32 NumExp = Gone->Explored.Num();
			TextF(FString::Printf(TEXT("%d niveau%s explor\u00e9%s  \u00b7  %s de jeu"), NumExp, NumExp > 1 ? TEXT("s") : TEXT(""), NumExp > 1 ? TEXT("s") : TEXT(""),
				*BRSaves::FormatPlayTime(Gone->PlayTime)), CX,
				CardY + 116.f * U, WithAlpha(InkDim, 0.8f), 12.f, EUiWeight::Light, EUiAlign::Center);
		}
		const float BH = 56.f * U;
		const float PW0 = 250.f * U;
		const float PW1 = 190.f * U;
		const float BX = CX - (PW0 + PW1 + 16.f * U) * 0.5f;
		MenuPill(0, BX, CardY + CH2 - BH - 34.f * U, PW0, BH, TEXT("UI_IconTrash"), true, bInteractive, 1.f, true);
		MenuPill(1, BX + PW0 + 16.f * U, CardY + CH2 - BH - 34.f * U, PW1, BH, TEXT("UI_IconBack"), false, bInteractive, 1.f);
	}
}

void ABRHUD::DrawMenuNewSave(ABRPlayerController* PC, bool bInteractive)
{
	const float U = Ui();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	const float CY = H * 0.5f;
	const float X0 = FMath::Max(60.f * U, W * 0.0625f);
	const float In = EaseOut(MenuPageTime / 0.4f);

	DrawLogo(X0 - 240.f * U * 0.03f, 46.f * U, 240.f * U, In, false);
	// Carte centrale autour du champ du nom (widget Slate de 520 x 52, centre a l'ecran)
	const float CW = 700.f * U;
	const float CH = 384.f * U;
	const float CardX = CX - CW * 0.5f;
	const float CardY = CY - 196.f * U + (1.f - In) * 24.f * U;
	Glow(CX, CY, CW * 0.8f, CH * 0.9f, FLinearColor(1.f, 0.8f, 0.3f, 0.06f * In));
	RoundRect(CardX, CardY + 6.f * U, CW, CH, 22.f * U, FLinearColor(0.f, 0.f, 0.f, 0.35f * In));
	RoundRect(CardX, CardY, CW, CH, 22.f * U, FLinearColor(0.05f, 0.045f, 0.03f, 0.92f * In));
	RoundRect(CardX, CardY, CW, CH, 22.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.16f * In), true);
	const float BD = 64.f * U;
	RoundRect(CX - BD * 0.5f, CardY - BD * 0.5f, BD, BD, BD * 0.5f, WithAlpha(Yellow, In));
	if (UTexture* T = UiTex(TEXT("UI_IconSave")))
	{
		DrawTexture(T, CX - BD * 0.28f, CardY - BD * 0.28f, BD * 0.56f, BD * 0.56f, 0.f, 0.f, 1.f, 1.f, FLinearColor(0.07f, 0.055f, 0.02f, In), BLEND_Translucent);
	}
	TextF(PC->IsHostFlow() ? TEXT("NOUVELLE PARTIE EN LIGNE") : TEXT("NOUVELLE PARTIE"), CX, CardY + 46.f * U, WithAlpha(Ink, In), 24.f, EUiWeight::Black,
		EUiAlign::Center);
	TextF(TEXT("Donnez-lui un nom. Vous commencerez au Niveau 0, avec l'\u00e9quipement de d\u00e9part."), CX, CardY + 92.f * U, WithAlpha(InkDim, In), 12.5f,
		EUiWeight::Light, EUiAlign::Center);
	const float FW = 548.f * U;
	const float FH = 68.f * U;
	RoundRect(CX - FW * 0.5f, CY - FH * 0.5f, FW, FH, 12.f * U, FLinearColor(0.f, 0.f, 0.f, 0.45f * In));
	RoundRect(CX - FW * 0.5f, CY - FH * 0.5f, FW, FH, 12.f * U, FLinearColor(1.f, 0.84f, 0.3f, (0.5f + 0.25f * FMath::Sin(Clock * 3.f)) * In), true);

	const float BH = 56.f * U;
	const float PW0 = 260.f * U;
	const float PW1 = 180.f * U;
	const float BG = 16.f * U;
	const float BX = CX - (PW0 + PW1 + BG) * 0.5f;
	MenuPill(0, BX, CY + 66.f * U, PW0, BH, TEXT("UI_IconPlay"), true, bInteractive, In);
	MenuPill(1, BX + PW0 + BG, CY + 66.f * U, PW1, BH, TEXT("UI_IconBack"), false, bInteractive, In);
	TextF(FString::Printf(TEXT("Emplacement %d / 6   \u00b7   sauvegarde automatique \u00e0 chaque niveau"), PC->GetSaveOrder().Num() + 1), CX, CardY + CH - 40.f * U,
		WithAlpha(InkDim, 0.85f * In), 11.5f, EUiWeight::Regular, EUiAlign::Center, false);

	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(TEXT("ENTR\u00c9E"), TEXT("Commencer"));
	Hints.Emplace(TEXT("\u00c9CHAP"), TEXT("Retour"));
	KeyHints(CX, H - 76.f * U, Hints, In, true);
}

void ABRHUD::DrawSaveIndicator(float Since)
{
	// Petite pastille en bas a droite pendant une sauvegarde automatique
	const float A = FMath::Clamp(Since / 0.2f, 0.f, 1.f) * FMath::Clamp((2.6f - Since) / 0.6f, 0.f, 1.f);
	if (A <= 0.01f)
	{
		return;
	}
	const float U = Ui();
	const FString Label = TEXT("Sauvegarde");
	const FVector2f LS = TextSize(Label, 10.5f, EUiWeight::Regular);
	const float PH = 30.f * U;
	const float PW = LS.X + 52.f * U;
	const float PX = Canvas->ClipX - PW - 50.f * U;
	const float PY = Canvas->ClipY - PH - 112.f * U;
	RoundRect(PX, PY, PW, PH, PH * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.45f * A));
	if (UTexture* T = UiTex(TEXT("UI_IconSave")))
	{
		const float IS = 15.f * U;
		const float Pulse = 0.6f + 0.4f * FMath::Sin(Since * 9.f);
		DrawTexture(T, PX + 13.f * U, PY + (PH - IS) * 0.5f, IS, IS, 0.f, 0.f, 1.f, 1.f, WithAlpha(Yellow, A * Pulse), BLEND_Translucent);
	}
	TextF(Label, PX + 36.f * U, PY + (PH - LS.Y) * 0.5f, FLinearColor(1.f, 1.f, 1.f, 0.85f * A), 10.5f, EUiWeight::Regular, EUiAlign::Left, false);
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
	else if (Id >= Btn_SaveDelete && Id < Btn_SaveDelete + 16)
	{
		PC->RequestDeleteSave(Id - Btn_SaveDelete);
	}
	else if (Id >= Btn_MenuCard && Id <= Btn_MenuCard + 6)
	{
		// Carte du carrousel : celle du centre lance la partie, une voisine devient la selection
		const int32 Rel = Id - Btn_MenuCard - 3;
		if (Rel == 0)
		{
			PC->MenuActivate(0);
		}
		for (int32 k = 0; k < FMath::Abs(Rel); ++k)
		{
			PC->MenuShiftLevel(Rel > 0 ? 1 : -1);
		}
	}
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
	// Carte de titre facon generique de film : bandes noires, numero du niveau, filet jaune qui s'etire
	const float E = 7.f - T; // temps ecoule depuis l'arrivee
	const float Bars = EaseOut(E / 0.8f) * FMath::Clamp(T / 0.9f, 0.f, 1.f);
	const float A = FMath::Clamp((E - 0.35f) / 1.f, 0.f, 1.f) * FMath::Clamp(T / 1.3f, 0.f, 1.f);
	const FBRLevelDef& D = W->Def();
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	const float H = Canvas->ClipY;
	const float BH = H * 0.085f * Bars;
	DrawRect(FLinearColor::Black, 0.f, 0.f, Canvas->ClipX, BH);
	DrawRect(FLinearColor::Black, 0.f, H - BH, Canvas->ClipX, BH);
	if (A <= 0.003f)
	{
		return;
	}
	const float Y0 = H * 0.29f;
	Glow(CX, Y0 + 150.f * U, 660.f * U, 320.f * U, FLinearColor(0.f, 0.f, 0.f, 0.65f * A));
	Glow(CX, Y0 + 90.f * U, 380.f * U, 160.f * U, FLinearColor(1.f, 0.78f, 0.25f, 0.07f * A));
	TextSpaced(TEXT("NIVEAU"), CX, Y0, FLinearColor(Ink.R, Ink.G, Ink.B, 0.9f * A), 15.f, EUiWeight::Regular, 12.f * U, EUiAlign::Center);
	const FString Num = FString::FromInt(D.Number);
	TextF(Num, CX, Y0 + 20.f * U, FLinearColor(1.f, 1.f, 1.f, A), 88.f, EUiWeight::Black, EUiAlign::Center);
	const float LY = Y0 + 20.f * U + TextSize(Num, 88.f, EUiWeight::Black).Y + 2.f * U;
	const float L = 230.f * U * EaseOut((E - 0.6f) / 1.2f);
	const float Th = FMath::Max(1.f, 2.f * U);
	DrawRect(WithAlpha(Yellow, 0.85f * A), CX - 18.f * U - L, LY, L, Th);
	DrawRect(WithAlpha(Yellow, 0.85f * A), CX + 18.f * U, LY, L, Th);
	RoundRect(CX - 5.f * U, LY + Th * 0.5f - 5.f * U, 10.f * U, 10.f * U, 5.f * U, WithAlpha(Yellow, A));
	TextF(FString::Printf(TEXT("\u00ab %s \u00bb"), *D.Title), CX, LY + 22.f * U, WithAlpha(Ink, A), 26.f, EUiWeight::Bold, EUiAlign::Center);
	TextF(D.Nickname, CX, LY + 66.f * U, FLinearColor(Ink.R, Ink.G, Ink.B, 0.85f * A), 16.f, EUiWeight::Regular, EUiAlign::Center);
	const FLinearColor CC = ClassColor(D.SurvivalClass);
	const FVector2f CS = TextSize(D.ClassText, 11.f, EUiWeight::Bold);
	const float PH = 28.f * U;
	const float PW = CS.X + 46.f * U;
	const float PY = LY + 108.f * U;
	RoundRect(CX - PW * 0.5f, PY, PW, PH, PH * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.45f * A));
	RoundRect(CX - PW * 0.5f, PY, PW, PH, PH * 0.5f, FLinearColor(CC.R, CC.G, CC.B, 0.18f * A));
	const float Dot = 8.f * U;
	RoundRect(CX - PW * 0.5f + 14.f * U, PY + (PH - Dot) * 0.5f, Dot, Dot, Dot * 0.5f, WithAlpha(CC, A));
	TextF(D.ClassText, CX + 8.f * U, PY + (PH - CS.Y) * 0.5f, WithAlpha(CC, A), 11.f, EUiWeight::Bold, EUiAlign::Center, false);
}

// =====================================================================================================================
// HUD de jeu
// =====================================================================================================================

void ABRHUD::DrawRecording(ABRCharacter* C, ABRWorld* W)
{
	// v4.2 : plus de viseur de camescope (REC, coins, point rouge) ; seulement la tache d'enregistrement en cours
	// et l'indicateur de vision nocturne
	if (!C->HasCamcorder())
	{
		return;
	}
	const float U = Ui();
	const float X = 40.f * U;
	const float Y = 34.f * U;
	if (W && !W->GetRecordLabel().IsEmpty())
	{
		const FString Label = TEXT("ENREGISTREMENT : ") + W->GetRecordLabel();
		const float PW = FMath::Max(260.f * U, TextSize(Label, 11.f, EUiWeight::Bold).X + 36.f * U);
		const float PH = 46.f * U;
		RoundRect(X, Y, PW, PH, 12.f * U, FLinearColor(0.f, 0.f, 0.f, 0.45f));
		TextF(Label, X + 18.f * U, Y + 7.f * U, Yellow, 11.f, EUiWeight::Bold);
		const float BarW = PW - 36.f * U;
		const float BarH = 5.f * U;
		const float BarY = Y + PH - 13.f * U;
		RoundRect(X + 18.f * U, BarY, BarW, BarH, BarH * 0.5f, FLinearColor(1.f, 1.f, 1.f, 0.15f));
		const float Progress = FMath::Clamp(W->GetRecordProgress(), 0.f, 1.f);
		if (Progress > 0.f)
		{
			RoundRect(X + 18.f * U, BarY, FMath::Max(BarH, BarW * Progress), BarH, BarH * 0.5f, Yellow);
		}
	}
	if (C->IsNightVision())
	{
		TextF(TEXT("VISION NOCTURNE"), Canvas->ClipX - 40.f * U, Y, FLinearColor(0.5f, 1.f, 0.5f, 0.92f), 11.f, EUiWeight::Bold, EUiAlign::Right);
		if (FBRSettings::Get().bVHSEffect)
		{
			Scanlines(0.05f);
		}
	}
}

void ABRHUD::DrawDevOverlay(ABRPlayerController* PC, ABRCharacter* C, ABRWorld* W)
{
	const float U = Ui();
	const FLinearColor Cyan(0.55f, 0.88f, 1.f, 0.95f);
	// Pastille permanente, en haut au centre : niveau courant et etat des aides
	FString Tag = W ? FString::Printf(TEXT("DEV  \u00b7  NIVEAU %d"), W->GetLevelNumber()) : FString(TEXT("DEV"));
	if (C && C->IsDevFlying())
	{
		Tag += TEXT("  \u00b7  VOL");
	}
	if (C && C->bGodMode)
	{
		Tag += TEXT("  \u00b7  INVINCIBLE");
	}
	if (PC->IsDevSession())
	{
		Tag += TEXT("  \u00b7  hors partie");
	}
	Tag += TEXT("  \u00b7  ") + PC->GetRenderModeText(true);
	const FVector2f TS = TextSize(Tag, 10.f, EUiWeight::Bold);
	const float PW = TS.X + 28.f * U;
	const float PH = 26.f * U;
	const float PX = (Canvas->ClipX - PW) * 0.5f;
	const float PY = 12.f * U;
	RoundRect(PX, PY, PW, PH, PH * 0.5f, FLinearColor(0.02f, 0.06f, 0.09f, 0.7f));
	RoundRect(PX, PY, PW, PH, PH * 0.5f, FLinearColor(0.55f, 0.88f, 1.f, 0.45f), true);
	TextF(Tag, PX + 14.f * U, PY + (PH - TS.Y) * 0.5f, Cyan, 10.f, EUiWeight::Bold);

	// v4.5 : modeles fournis remplaces par une forme de secours (import incomplet) : jamais en silence
	const TArray<FString>& Fallbacks = UBRAssets::GetFallbacks();
	if (Fallbacks.Num() > 0)
	{
		const FString Msg = TEXT("MOD\u00c8LES DE SECOURS (import incomplet) : ") + FString::Join(Fallbacks, TEXT("  \u00b7  "));
		TextF(Msg, Canvas->ClipX * 0.5f, PY + PH + 8.f * U, FLinearColor(1.f, 0.45f, 0.35f, 0.95f), 9.f, EUiWeight::Bold, EUiAlign::Center);
	}

	// Aide des raccourcis : a l'arrivee dans un niveau et apres chaque raccourci
	const float T = PC->GetDevHelpTime();
	if (T <= 0.f)
	{
		return;
	}
	const float A = FMath::Clamp(T / 0.6f, 0.f, 1.f);
	static const TCHAR* const Keys[][2] = {
		{ TEXT("PAGE PR\u00c9C. / SUIV."), TEXT("niveau suivant / pr\u00e9c\u00e9dent") },
		{ TEXT("D\u00c9BUT"), TEXT("nouvelle disposition du niveau") },
		{ TEXT("F6"), TEXT("vol libre \u00e0 travers les murs") },
		{ TEXT("F7"), TEXT("invincible") },
		{ TEXT("F10"), TEXT("jumpscare suivant (chaque entit\u00e9)") },
		{ TEXT("FIN"), TEXT("objectifs remplis") },
		{ TEXT("INSER"), TEXT("coupure de courant") },
	};
	const int32 N = UE_ARRAY_COUNT(Keys);
	const float RowH = 24.f * U;
	const float BW = 360.f * U;
	const float BH = 44.f * U + N * RowH;
	const float BX = 40.f * U;
	const float BY = Canvas->ClipY * 0.5f - BH * 0.5f;
	RoundRect(BX, BY, BW, BH, 12.f * U, FLinearColor(0.01f, 0.03f, 0.05f, 0.72f * A));
	RoundRect(BX, BY, BW, BH, 12.f * U, FLinearColor(0.55f, 0.88f, 1.f, 0.35f * A), true);
	TextF(TEXT("MODE D\u00c9VELOPPEUR"), BX + 16.f * U, BY + 12.f * U, WithAlpha(Cyan, A), 10.f, EUiWeight::Black);
	for (int32 i = 0; i < N; ++i)
	{
		const float Y = BY + 40.f * U + i * RowH;
		TextF(Keys[i][0], BX + 16.f * U, Y, WithAlpha(Yellow, A), 9.5f, EUiWeight::Bold);
		TextF(Keys[i][1], BX + 150.f * U, Y, FLinearColor(0.92f, 0.9f, 0.84f, 0.92f * A), 9.5f, EUiWeight::Regular);
	}
}

void ABRHUD::DrawJumpscare(ABRCharacter* C)
{
	const int32 K = C->GetScareKind();
	const float T = C->GetScareTime();
	const float Dur = FMath::Max(0.1f, C->GetScareDuration());
	const float Impact = C->GetScareImpact();
	const bool bHit = T >= Impact;
	const float Since = FMath::Max(0.f, T - Impact);
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float U = Ui();
	const float Out = FMath::Clamp((Dur - T) / 0.25f, 0.f, 1.f); // fondu de sortie
	auto Vignette = [&](const FLinearColor& Col, float Frac)
	{
		Gradient(0.f, 0.f, W * Frac, H, Col, 0);
		Gradient(W * (1.f - Frac), 0.f, W * Frac, H, Col, 1);
		Gradient(0.f, 0.f, W, H * Frac, Col, 2);
		Gradient(0.f, H * (1.f - Frac), W, H * Frac, Col, 3);
	};
	// v4.5 : la peur vient du modele et de son geste ; l'ecran ne fait que ponctuer (effets divises par deux environ)
	// Une image sombre a l'impact (sauf le Smiler, qui finit sur un eclair)
	if (K != 0 && bHit && Since < 0.035f)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, W, H);
	}
	switch (static_cast<EBREntityKind>(K))
	{
	case EBREntityKind::Smiler:
	{
		// Le noir se referme autour du sourire, puis un eclair blanc
		Vignette(FLinearColor(0.f, 0.f, 0.f, 0.75f * Out), 0.32f);
		const float Flash = FMath::Clamp((T - (Dur - 0.35f)) / 0.12f, 0.f, 1.f) * FMath::Clamp((Dur - T) / 0.23f, 0.f, 1.f);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.55f * Flash), 0.f, 0.f, W, H);
		break;
	}
	case EBREntityKind::Hound:
	{
		// Trois griffures qui dechirent l'ecran, l'une apres l'autre, et un voile rouge
		if (bHit)
		{
			DrawRect(FLinearColor(0.6f, 0.f, 0.f, 0.18f * FMath::Exp(-Since * 3.f)), 0.f, 0.f, W, H);
			for (int32 i = 0; i < 3; ++i)
			{
				const float Appear = Since - i * 0.06f;
				if (Appear <= 0.f)
				{
					continue;
				}
				const float Len = FMath::Min(1.f, Appear / 0.08f);
				const float A = 0.6f * FMath::Clamp(1.4f - Since * 0.9f, 0.f, 1.f) * Out;
				const float X0 = W * (0.66f + i * 0.07f);
				const float Y0 = H * (0.1f + i * 0.04f);
				const float X1 = X0 - W * 0.42f * Len;
				const float Y1 = Y0 + H * 0.78f * Len;
				DrawLine(X0, Y0, X1, Y1, FLinearColor(0.15f, 0.f, 0.f, A), 26.f * U);
				DrawLine(X0, Y0, X1, Y1, FLinearColor(0.75f, 0.05f, 0.03f, A), 12.f * U);
				DrawLine(X0, Y0, X1, Y1, FLinearColor(1.f, 0.6f, 0.5f, 0.6f * A), 3.f * U);
			}
		}
		break;
	}
	case EBREntityKind::Faceling:
	{
		// La neige d'une television qui hurle
		if (bHit && Since < 0.45f)
		{
			const float A = (Since < 0.3f ? 0.35f : 0.35f * (0.45f - Since) / 0.15f);
			const float Cell = FMath::Max(4.f, 7.f * U);
			for (float Y = 0.f; Y < H; Y += Cell)
			{
				for (float X = 0.f; X < W; X += Cell * 3.f)
				{
					const float G = FMath::FRand();
					DrawRect(FLinearColor(G, G, G, A), X, Y, Cell * 3.f, Cell);
				}
			}
			const float Bar = FMath::Fmod(T * 1.7f, 1.f) * H;
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.25f * A), 0.f, Bar, W, 40.f * U);
		}
		else
		{
			Vignette(FLinearColor(0.f, 0.f, 0.f, 0.6f * FMath::Min(1.f, T / Impact) * Out), 0.3f);
		}
		break;
	}
	case EBREntityKind::SkinStealer:
	{
		// La scene vire a la chair : vignette rouge qui bat
		const float Pulse = 0.55f + 0.45f * FMath::Sin(T * 14.f);
		Vignette(FLinearColor(0.45f, 0.02f, 0.01f, 0.45f * Pulse * Out), 0.28f);
		break;
	}
	case EBREntityKind::Deathmoth:
	{
		// Un essaim de papillons de nuit traverse l'ecran
		for (int32 i = 0; i < 16; ++i)
		{
			const float Seed = static_cast<float>(i);
			const float Start = FMath::Frac(Seed * 0.6180339f) * 0.6f;
			const float P = (T - Start) / (0.6f + FMath::Frac(Seed * 0.37f) * 0.6f);
			if (P <= 0.f || P >= 1.f)
			{
				continue;
			}
			const bool bFromLeft = (i % 2) == 0;
			const float X = bFromLeft ? -0.1f * W + P * 1.2f * W : 1.1f * W - P * 1.2f * W;
			const float Y = H * (0.1f + FMath::Frac(Seed * 0.731f) * 0.8f) + FMath::Sin(P * 9.f + Seed) * 60.f * U;
			const float S = (18.f + FMath::Frac(Seed * 0.913f) * 46.f) * U;
			const float Flap = 0.25f + 0.75f * FMath::Abs(FMath::Sin(T * 38.f + Seed));
			const FLinearColor MothC(0.08f, 0.06f, 0.04f, 0.92f * Out);
			RoundRect(X - S * Flap, Y - S * 0.35f, S * Flap, S * 0.7f, S * 0.3f, MothC);
			RoundRect(X, Y - S * 0.35f, S * Flap, S * 0.7f, S * 0.3f, MothC);
			DrawRect(FLinearColor(0.03f, 0.02f, 0.01f, 0.95f * Out), X - S * 0.08f, Y - S * 0.4f, S * 0.16f, S * 0.8f);
		}
		Vignette(FLinearColor(0.12f, 0.08f, 0.03f, 0.35f * Out), 0.22f);
		break;
	}
	case EBREntityKind::Wretch:
	{
		// Images noires entre chaque a-coup
		const float Phase = (T - 0.f) / 0.75f * 5.f;
		if (!bHit && FMath::Frac(Phase) < 0.16f)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f), 0.f, 0.f, W, H);
		}
		Vignette(FLinearColor(0.02f, 0.03f, 0.02f, 0.45f * Out), 0.26f);
		break;
	}
	case EBREntityKind::Partygoer:
	{
		if (!bHit)
		{
			// Le silence : l'image s'assombrit un instant
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f * T / FMath::Max(Impact, 0.01f)), 0.f, 0.f, W, H);
			break;
		}
		// Confettis qui jaillissent du centre et retombent
		static const FLinearColor Colors[] = { FLinearColor(1.f, 0.2f, 0.25f), FLinearColor(0.2f, 0.6f, 1.f), FLinearColor(1.f, 0.85f, 0.1f),
			FLinearColor(0.3f, 0.95f, 0.4f), FLinearColor(0.9f, 0.3f, 1.f) };
		for (int32 i = 0; i < 60; ++i)
		{
			const float Seed = static_cast<float>(i);
			const float Ang = FMath::Frac(Seed * 0.6180339f) * 2.f * PI;
			const float Speed = (500.f + FMath::Frac(Seed * 0.377f) * 1100.f) * U;
			const float X = W * 0.5f + FMath::Cos(Ang) * Speed * Since;
			const float Y = H * 0.45f + FMath::Sin(Ang) * Speed * Since + 900.f * U * Since * Since;
			const float S = (8.f + FMath::Frac(Seed * 0.913f) * 10.f) * U;
			const float Flip = FMath::Abs(FMath::Sin(Since * 12.f + Seed));
			FLinearColor Col = Colors[i % UE_ARRAY_COUNT(Colors)];
			Col.A = Out;
			DrawRect(Col, X, Y, S, S * (0.3f + 0.7f * Flip));
		}
		// (le sourire est celui du modele, tout pres : plus de "=)" geant a l'ecran)
		if (Since < 0.9f)
		{
			TextF(TEXT("JOYEUX ANNIVERSAIRE"), W * 0.5f, H * 0.8f, FLinearColor(1.f, 1.f, 1.f, 0.6f * Out * (1.f - Since / 0.9f)), 16.f, EUiWeight::Bold,
				EUiAlign::Center);
		}
		break;
	}
	case EBREntityKind::Clump:
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(T * 18.f);
		Vignette(FLinearColor(0.25f, 0.f, 0.f, (0.3f + 0.2f * Pulse) * Out), 0.3f);
		break;
	}
	case EBREntityKind::Bacteria:
	{
		// L'image se brouille : bandes noires et blanches qui sautent
		const float Amount = FMath::Clamp(T / Dur * 1.4f, 0.f, 1.f);
		const int32 Bars = 3 + static_cast<int32>(Amount * 6.f);
		for (int32 i = 0; i < Bars; ++i)
		{
			const float Y = FMath::FRand() * H;
			const float BH = (2.f + FMath::FRand() * 26.f) * U;
			const bool bWhite = FMath::FRand() < 0.35f;
			DrawRect(bWhite ? FLinearColor(0.9f, 0.9f, 0.95f, 0.5f * Out) : FLinearColor(0.f, 0.f, 0.f, 0.85f * Out), 0.f, Y, W, BH);
		}
		Vignette(FLinearColor(0.f, 0.f, 0.f, 0.7f * Out), 0.3f);
		if (T > Dur - 0.18f)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f), 0.f, 0.f, W, H);
		}
		break;
	}
	default:
		break;
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
	// Piles : lampe et camescope (vision nocturne)
	if (C->HasLightSource() || C->HasCamcorder())
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
	for (int32 i = 0; i < ABRCharacter::NumPockets; ++i)
	{
		const float X = X0 + i * (S + G);
		RoundRect(X, Y, S, S, 10.f * U, FLinearColor(0.f, 0.f, 0.f, 0.38f));
		RoundRect(X, Y, S, S, 10.f * U, FLinearColor(0.95f, 0.78f, 0.25f, 0.24f), true);
		TextF(FString::FromInt(i + 1), X + 7.f * U, Y + 4.f * U, InkDim, 8.5f, EUiWeight::Bold);
		const FBRItemSlot* It = C->Pockets.IsValidIndex(i) ? &C->Pockets[i] : nullptr;
		if (It && !It->IsEmpty())
		{
			Icon(ItemIcon(It->Item), X + S * 0.14f, Y + S * 0.14f, S * 0.72f, S * 0.72f, FLinearColor(1.f, 1.f, 1.f, 0.92f));
			if (It->Count > 1)
			{
				TextF(FString::Printf(TEXT("x%d"), It->Count), X + S - 6.f * U, Y + S - 20.f * U, Ink, 9.5f, EUiWeight::Bold, EUiAlign::Right);
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
	const float RX = Canvas->ClipX - 58.f * U;
	float Y = 110.f * U;
	for (const FBRObjective& O : Objs)
	{
		if (!O.bRequired && O.IsDone())
		{
			continue;
		}
		const FLinearColor Col = O.IsDone() ? Done : (O.bRequired ? WithAlpha(Yellow, 0.92f) : WithAlpha(InkDim, 0.8f));
		const FString Line = O.Goal > 0 ? FString::Printf(TEXT("%s  %d/%d"), *O.Text, O.Progress, O.Goal) : O.Text;
		const FVector2f LS = TextSize(Line, 11.5f, EUiWeight::Regular);
		TextF(Line, RX, Y, Col, 11.5f, O.IsDone() ? EUiWeight::Light : EUiWeight::Regular, EUiAlign::Right);
		const float D = 7.f * U;
		RoundRect(RX + 10.f * U, Y + (LS.Y - D) * 0.5f, D, D, D * 0.5f, Col);
		Y += 24.f * U;
	}
}

void ABRHUD::DrawCrosshair(ABRCharacter* C)
{
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const bool bFocus = !C->GetFocusPrompt().IsEmpty();
	const float S = (bFocus ? 7.f : 4.f) * U;
	if (C->IsHidden())
	{
		const float Pulse = 0.65f + 0.2f * FMath::Sin(Clock * 2.f);
		TextSpaced(TEXT("CACH\u00c9"), CX, Canvas->ClipY - 152.f * U, FLinearColor(0.75f, 0.9f, 1.f, Pulse), 13.f, EUiWeight::Bold, 6.f * U, EUiAlign::Center);
	}
	RoundRect(CX - S * 0.5f - 1.f * U, CY - S * 0.5f - 1.f * U, S + 2.f * U, S + 2.f * U, S * 0.5f + 1.f * U, FLinearColor(0.f, 0.f, 0.f, bFocus ? 0.35f : 0.2f));
	RoundRect(CX - S * 0.5f, CY - S * 0.5f, S, S, S * 0.5f, FLinearColor(1.f, 1.f, 1.f, bFocus ? 0.92f : 0.5f));
	if (bFocus)
	{
		const FString P = C->GetFocusPrompt();
		const FVector2f PS = TextSize(P, 13.f, EUiWeight::Regular);
		const float PH = 32.f * U;
		const float PW = PS.X + 32.f * U;
		RoundRect(CX - PW * 0.5f, CY + 26.f * U, PW, PH, PH * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.42f));
		TextF(P, CX, CY + 26.f * U + (PH - PS.Y) * 0.5f, FLinearColor(1.f, 1.f, 1.f, 0.95f), 13.f, EUiWeight::Regular, EUiAlign::Center, false);
	}
	if (C->GetReviveProgress() > 0.f)
	{
		Bar(CX - 130.f * U, CY + 84.f * U, 260.f * U, 8.f * U, C->GetReviveProgress(), FLinearColor(0.55f, 1.f, 0.55f, 0.9f), TEXT("R\u00c9ANIMATION"));
	}
}

void ABRHUD::DrawMessages(float Dt)
{
	const float U = Ui();
	for (int32 i = Messages.Num() - 1; i >= 0; --i)
	{
		Messages[i].Age += Dt;
		if (Messages[i].Age > Messages[i].Duration)
		{
			Messages.RemoveAt(i);
		}
	}
	// Notifications : pastilles sombres avec un point de la couleur du message
	float Y = Canvas->ClipY * 0.1f;
	const float MaxW = FMath::Min(1100.f * U, Canvas->ClipX - 120.f * U);
	const float LH = 21.f * U;
	for (const FMsg& M : Messages)
	{
		const float A = FMath::Clamp(M.Duration - M.Age, 0.f, 1.f) * FMath::Clamp(M.Age * 5.f, 0.f, 1.f);
		const TArray<FString> Lines = WrapF(M.Text, MaxW - 70.f * U, 13.5f, EUiWeight::Regular);
		float TW = 0.f;
		for (const FString& L : Lines)
		{
			TW = FMath::Max(TW, TextSize(L, 13.5f, EUiWeight::Regular).X);
		}
		const float PH = Lines.Num() * LH + 18.f * U;
		const float PW = TW + 62.f * U;
		const float PX = (Canvas->ClipX - PW) * 0.5f;
		const float PY = Y - (1.f - EaseOut(M.Age * 4.f)) * 10.f * U;
		RoundRect(PX, PY, PW, PH, FMath::Min(PH * 0.5f, 20.f * U), FLinearColor(0.04f, 0.035f, 0.02f, 0.82f * A));
		RoundRect(PX, PY, PW, PH, FMath::Min(PH * 0.5f, 20.f * U), FLinearColor(M.Color.R, M.Color.G, M.Color.B, 0.3f * A), true);
		const float Dot = 10.f * U;
		RoundRect(PX + 20.f * U, PY + 9.f * U + (LH - Dot) * 0.5f, Dot, Dot, Dot * 0.5f, FLinearColor(M.Color.R, M.Color.G, M.Color.B, A));
		const FLinearColor TC = Mix(FLinearColor(M.Color.R, M.Color.G, M.Color.B, 1.f), FLinearColor::White, 0.35f);
		for (int32 k = 0; k < Lines.Num(); ++k)
		{
			TextF(Lines[k], PX + 42.f * U, PY + 9.f * U + k * LH, WithAlpha(TC, A), 13.5f, EUiWeight::Regular, EUiAlign::Left, false);
		}
		Y += PH + 10.f * U;
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
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	if (PlayerOwner)
	{
		PlayerOwner->GetMousePosition(MouseX, MouseY);
	}
	Buttons.Reset();
	PauseTime += UiDt;
	const float In = EaseOut(PauseTime / 0.35f);
	const float X0 = FMath::Max(60.f * U, W * 0.0625f) - (1.f - In) * 30.f * U;

	DrawRect(FLinearColor(0.01f, 0.01f, 0.005f, 0.45f * In), 0.f, 0.f, W, H);
	Gradient(0.f, 0.f, W * 0.62f, H, FLinearColor(0.012f, 0.01f, 0.004f, 0.9f * In), 0);
	Gradient(0.f, H * 0.6f, W, H * 0.4f, FLinearColor(0.f, 0.f, 0.f, 0.7f * In), 3);
	Scanlines(0.03f * In);

	DrawLogo(X0 - 240.f * U * 0.03f, 70.f * U, 240.f * U, In, false);
	TextF(TEXT("PAUSE"), X0, 138.f * U, WithAlpha(Ink, In), 44.f, EUiWeight::Black);
	if (PC && PC->IsNetGame())
	{
		const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		const int32 Count = GS ? GS->PlayerArray.Num() : 1;
		TextF(FString::Printf(TEXT("Partie en ligne  \u00b7  %d joueur%s  \u00b7  le jeu continue pendant la pause"), Count, Count > 1 ? TEXT("s") : TEXT("")),
			X0, 214.f * U, FLinearColor(0.7f, 0.95f, 0.7f, In), 13.f, EUiWeight::Light);
		DrawPlayerList();
	}
	else
	{
		TextF(TEXT("Le temps s'est arr\u00eat\u00e9\u2026 pour l'instant."), X0, 214.f * U, WithAlpha(InkDim, In), 13.f, EUiWeight::Light);
	}
	if (const UBRSaveGame* Save = PC ? PC->GetActiveSave() : nullptr)
	{
		TextF(FString::Printf(TEXT("Partie \u00ab %s \u00bb  \u00b7  %s de jeu  \u00b7  sauvegarde automatique"), *Save->SaveName,
			*BRSaves::FormatPlayTime(Save->PlayTime)), X0, 236.f * U, WithAlpha(Yellow, 0.8f * In), 11.f, EUiWeight::Regular);
	}

	// Cartes cliquables (meme style que le menu titre)
	const TCHAR* Labels[] = { TEXT("REPRENDRE"), TEXT("PARAM\u00c8TRES"), TEXT("TOUCHES"), TEXT("MENU PRINCIPAL"), TEXT("QUITTER LE JEU") };
	const TCHAR* Subs[] = {
		TEXT("Retourner dans les Backrooms"),
		TEXT("Graphismes, son, affichage, chat vocal"),
		TEXT("Changer les touches (jusqu'\u00e0 3 par action)"),
		(PC && PC->IsNetGame()) ? TEXT("Quitter la partie en ligne") : TEXT("Quitter la partie, revenir \u00e0 l'\u00e9cran titre"),
		TEXT("Fermer le jeu"),
	};
	const TCHAR* Icons[] = { TEXT("UI_IconPlay"), TEXT("UI_IconSettings"), TEXT("UI_IconKeys"), TEXT("UI_IconBack"), TEXT("UI_IconQuit") };
	const int32 Ids[] = { Btn_PauseResume, Btn_PauseSettings, Btn_PauseKeys, Btn_PauseMainMenu, Btn_PauseQuit };
	const float CW = 500.f * U;
	const float CH = 78.f * U;
	float Y = 262.f * U;
	for (int32 i = 0; i < 5; ++i)
	{
		const bool bHov = Hover(X0, Y, CW, CH);
		PauseSel[i] = FMath::FInterpTo(PauseSel[i], bHov ? 1.f : 0.f, UiDt, 14.f);
		DrawCard(X0, Y, CW, CH, PauseSel[i], Labels[i], Subs[i], Icons[i], In * EaseOut((PauseTime - 0.05f * i) / 0.3f), i == 4);
		AddButton(Ids[i], X0, Y, CW, CH);
		Y += CH + 12.f * U;
	}
	HandlePauseMouse(PC);

	TextSpaced(TEXT("COMMANDES"), CX, H - 170.f * U, WithAlpha(Yellow, 0.8f * In), 10.f, EUiWeight::Bold, 3.f * U, EUiAlign::Center);
	TextF(ControlsLine(0), CX, H - 144.f * U, FLinearColor(0.85f, 0.83f, 0.76f, 0.9f * In), 11.5f, EUiWeight::Regular, EUiAlign::Center, false);
	TextF(ControlsLine(1), CX, H - 120.f * U, FLinearColor(0.85f, 0.83f, 0.76f, 0.9f * In), 11.5f, EUiWeight::Regular, EUiAlign::Center, false);
	TArray<TPair<FString, FString>> Hints;
	Hints.Emplace(BRKeys::Primary(EBRAction::Pause), TEXT("Reprendre"));
	Hints.Emplace(BRKeys::Primary(EBRAction::Inventory), TEXT("Param\u00e8tres et touches"));
	Hints.Emplace(TEXT("FIN"), TEXT("Quitter"));
	KeyHints(CX, H - 84.f * U, Hints, In, true);
	TextF(TEXT("Console (touche \u00b2) : BRLevel 37  |  BRGod  |  BRSpawn 0-8  |  BRGiveAll  |  BRBlackout  |  BRObjectives"), CX, H - 38.f * U,
		WithAlpha(InkDim, 0.5f * In), 9.5f, EUiWeight::Light, EUiAlign::Center, false);
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
	// v4.5 : le mode de rendu reellement actif (et non celui demande) ; RHI et support du ray tracing : au demarrage
	Txt(PC->GetRenderModeText(), X + 28.f * U, IY + IH - 40.f * U, WithAlpha(Yellow, 0.85f), 0.66f * U, Small, false, false);
	Txt(TEXT("Imm\u00e9diat : profil, qualit\u00e9, ray tracing, reflets, ombres de la lampe, r\u00e9solution.  Au red\u00e9marrage : DirectX 12 / 11, support du ray tracing, ")
		TEXT("cache de skinning (Config/DefaultEngine.ini)."), X + 28.f * U, IY + IH - 22.f * U, WithAlpha(InkDim, 0.7f), 0.62f * U, Small, false, false);
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
	const float PW = 400.f * U;
	const float X = Canvas->ClipX - PW - 60.f * U;
	float Y = 262.f * U;
	const int32 N = GS->PlayerArray.Num();
	const float RowH = 40.f * U;
	const float BoxH = 58.f * U + N * RowH + 8.f * U;
	RoundRect(X, Y, PW, BoxH, 14.f * U, FLinearColor(0.05f, 0.045f, 0.03f, 0.85f));
	RoundRect(X, Y, PW, BoxH, 14.f * U, FLinearColor(1.f, 0.88f, 0.5f, 0.14f), true);
	TextSpaced(TEXT("JOUEURS"), X + 22.f * U, Y + 20.f * U, Yellow, 10.5f, EUiWeight::Bold, 3.f * U);
	TextF(TEXT("LATENCE"), X + PW - 22.f * U, Y + 20.f * U, InkDim, 9.5f, EUiWeight::Regular, EUiAlign::Right, false);
	Y += 54.f * U;
	for (int32 i = 0; i < N; ++i)
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
		if (bMe)
		{
			RoundRect(X + 10.f * U, Y - 4.f * U, PW - 20.f * U, RowH - 4.f * U, 10.f * U, FLinearColor(1.f, 0.82f, 0.22f, 0.08f));
		}
		const float D = 26.f * U;
		RoundRect(X + 20.f * U, Y + (RowH - 8.f * U - D) * 0.5f, D, D, D * 0.5f, bMe ? WithAlpha(Yellow, 0.9f) : FLinearColor(1.f, 1.f, 1.f, 0.14f));
		TextF(Name.Left(1), X + 20.f * U + D * 0.5f, Y + (RowH - 8.f * U - D) * 0.5f + 3.f * U, bMe ? FLinearColor(0.07f, 0.055f, 0.02f, 1.f) : Ink, 10.5f,
			EUiWeight::Bold, EUiAlign::Center, false);
		const FVector2f NS = TextSize(Name, 13.f, EUiWeight::Regular);
		const float NY = Y + (RowH - 8.f * U - NS.Y) * 0.5f;
		TextF(Name, X + 58.f * U, NY, bMe ? Yellow : Ink, 13.f, EUiWeight::Regular, EUiAlign::Left, false);
		if (i == 0)
		{
			const FVector2f HS = TextSize(TEXT("H\u00d4TE"), 8.5f, EUiWeight::Bold);
			const float HX = X + 66.f * U + NS.X;
			RoundRect(HX, NY + (NS.Y - 18.f * U) * 0.5f, HS.X + 16.f * U, 18.f * U, 9.f * U, FLinearColor(1.f, 0.82f, 0.22f, 0.2f));
			TextF(TEXT("H\u00d4TE"), HX + 8.f * U, NY + (NS.Y - HS.Y) * 0.5f, Yellow, 8.5f, EUiWeight::Bold, EUiAlign::Left, false);
		}
		const int32 Ping = FMath::RoundToInt(PS->GetPingInMilliseconds());
		const FLinearColor PingCol = Ping < 80 ? Done : (Ping < 160 ? Yellow : Danger);
		TextF(i == 0 ? FString(TEXT("\u2014")) : FString::Printf(TEXT("%d ms"), Ping), X + PW - 22.f * U, NY, PingCol, 12.f, EUiWeight::Bold,
			EUiAlign::Right, false);
		Y += RowH;
	}
}
