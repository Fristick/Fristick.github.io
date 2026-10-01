#include "BRHUD.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BREntity.h"
#include "BRLevels.h"
#include "BRPlayerController.h"
#include "BRWorld.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"

namespace
{
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

	const TCHAR* ControlsLine1 = TEXT("ZQSD / WASD  se d\u00e9placer     Maj  courir     Ctrl / C  s'accroupir     Espace  sauter");
	const TCHAR* ControlsLine2 = TEXT("F  lampe torche     E  interagir     B  boire de l'eau d'amande     R  changer les piles     Tab  journal     P  pause");
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

void ABRHUD::Txt(const FString& S, float X, float Y, const FLinearColor& C, float Scale, UFont* Font, bool bCenter, bool bShadow)
{
	if (!Font)
	{
		Font = GEngine->GetMediumFont();
	}
	if (bCenter)
	{
		float W = 0.f, H = 0.f;
		GetTextSize(S, W, H, Font, Scale);
		X -= W * 0.5f;
	}
	if (bShadow)
	{
		DrawText(S, FLinearColor(0.f, 0.f, 0.f, C.A * 0.8f), X + 2.f * Ui(), Y + 2.f * Ui(), Font, Scale);
	}
	DrawText(S, C, X, Y, Font, Scale);
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
		float W = 0.f, H = 0.f;
		GetTextSize(Test, W, H, Font, Scale);
		if (W > MaxWidth && !Line.IsEmpty())
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
		DrawMessages(Dt);
		return;
	}

	DrawRecOverlay();
	if (C && !C->IsDead())
	{
		DrawCrosshair();
		DrawStats();
	}
	DrawTitleCard();
	DrawMessages(Dt);
	if (C && C->IsReadingNote())
	{
		DrawNote();
	}
	if (C && C->IsDead())
	{
		DrawDeath();
	}
	if (PC && PC->IsJournalOpen())
	{
		DrawJournal();
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
	if (PC && PC->IsPauseMenuOpen())
	{
		DrawPause();
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
	const TArray<FBRLevelDef>& All = BRLevels::All();
	const int32 Index = PC ? FMath::Clamp(PC->GetMenuIndex(), 0, All.Num() - 1) : 0;
	const FBRLevelDef& D = All[Index];

	DrawRect(FLinearColor(0.02f, 0.02f, 0.01f, 0.62f), 0.f, 0.f, Canvas->ClipX, H);

	const float Flick = (FMath::Sin(Clock * 23.f) > 0.97f) ? 0.5f : 1.f;
	Txt(TEXT("THE BACKROOMS"), CX, H * 0.14f, FLinearColor(1.f, 0.92f, 0.6f, Flick), 2.8f * U, Large, true);
	Txt(TEXT("Si vous ne faites pas attention et que vous noclippez hors de la r\u00e9alit\u00e9 au mauvais endroit..."),
		CX, H * 0.25f, FLinearColor(0.85f, 0.82f, 0.7f, 0.9f), 1.f * U, Medium, true);

	const FString Sel = FString::Printf(TEXT("<     NIVEAU %d     >"), D.Number);
	Txt(Sel, CX, H * 0.36f, FLinearColor::White, 1.7f * U, Large, true);
	Txt(FString::Printf(TEXT("\u00ab %s \u00bb  -  %s"), *D.Title, *D.Nickname), CX, H * 0.44f, FLinearColor(1.f, 0.9f, 0.6f), 1.1f * U, Medium, true);
	Txt(D.ClassText, CX, H * 0.485f, ClassColor(D.SurvivalClass), 0.95f * U, Medium, true);

	const TArray<FString> Lines = Wrap(D.Description, Canvas->ClipX * 0.5f, Medium, 0.9f * U);
	float Y = H * 0.54f;
	for (const FString& L : Lines)
	{
		Txt(L, CX, Y, FLinearColor(0.85f, 0.85f, 0.85f), 0.9f * U, Medium, true);
		Y += 26.f * U;
	}
	if (Index == 0)
	{
		Txt(TEXT("(Recommand\u00e9 pour commencer : le vrai d\u00e9but de l'aventure)"), CX, Y + 6.f * U, FLinearColor(0.6f, 0.9f, 0.6f, 0.8f), 0.8f * U, Medium, true);
	}

	const float Blink = 0.6f + 0.4f * FMath::Sin(Clock * 3.f);
	Txt(TEXT("[ \u2190 / \u2192 ]  choisir le niveau          [ ENTR\u00c9E ]  noclipper          [ FIN ]  quitter"), CX, H * 0.78f,
		FLinearColor(1.f, 1.f, 1.f, Blink), 1.f * U, Medium, true);
	Txt(ControlsLine1, CX, H * 0.86f, FLinearColor(0.7f, 0.7f, 0.7f, 0.85f), 0.8f * U, Medium, true);
	Txt(ControlsLine2, CX, H * 0.89f, FLinearColor(0.7f, 0.7f, 0.7f, 0.85f), 0.8f * U, Medium, true);
	Txt(TEXT("Inspir\u00e9 du Backrooms Wiki (backrooms-wiki.wikidot.com) - CC BY-SA 3.0"), CX, H * 0.95f,
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

void ABRHUD::DrawRecOverlay()
{
	ABRWorld* W = ABRWorld::Get(this);
	const float U = Ui();
	UFont* Medium = GEngine->GetMediumFont();
	const float X = 40.f * U;
	const float Y = 34.f * U;
	if (FMath::Fmod(Clock, 1.4f) < 0.9f)
	{
		DrawRect(FLinearColor(0.9f, 0.05f, 0.05f, 0.9f), X, Y + 5.f * U, 14.f * U, 14.f * U);
	}
	Txt(TEXT("REC"), X + 22.f * U, Y, FLinearColor(1.f, 1.f, 1.f, 0.85f), 0.95f * U, Medium);
	if (W)
	{
		Txt(Timecode(W->GetLevelTime()), X + 80.f * U, Y, FLinearColor(1.f, 1.f, 1.f, 0.7f), 0.95f * U, Medium);
		const FString Lv = FString::Printf(TEXT("NIVEAU %d"), W->GetLevelNumber());
		float TW = 0.f, TH = 0.f;
		GetTextSize(Lv, TW, TH, Medium, 0.95f * U);
		Txt(Lv, Canvas->ClipX - 40.f * U - TW, Y, FLinearColor(1.f, 1.f, 1.f, 0.7f), 0.95f * U, Medium);
	}
	// coins du viseur de camescope
	const float L = 40.f * U;
	const float Th = 3.f * U;
	const float M = 22.f * U;
	const FLinearColor CC(1.f, 1.f, 1.f, 0.35f);
	const float RX = Canvas->ClipX - M;
	const float BY = Canvas->ClipY - M;
	DrawRect(CC, M, M, L, Th);
	DrawRect(CC, M, M, Th, L);
	DrawRect(CC, RX - L, M, L, Th);
	DrawRect(CC, RX - Th, M, Th, L);
	DrawRect(CC, M, BY - Th, L, Th);
	DrawRect(CC, M, BY - L, Th, L);
	DrawRect(CC, RX - L, BY - Th, L, Th);
	DrawRect(CC, RX - Th, BY - L, Th, L);
}

void ABRHUD::DrawStats()
{
	ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	ABRCharacter* C = PC ? Cast<ABRCharacter>(PC->GetPawn()) : nullptr;
	if (!C)
	{
		return;
	}
	const float U = Ui();
	const float X = 50.f * U;
	const float BW = 230.f * U;
	const float BH = 7.f * U;
	float Y = Canvas->ClipY - 190.f * U;
	Bar(X, Y, BW, BH, C->Health / 100.f, FLinearColor(0.85f, 0.15f, 0.12f, 0.9f), TEXT("SANT\u00c9"));
	Y += 38.f * U;
	const float Pulse = C->Sanity < 30.f ? 0.6f + 0.4f * FMath::Sin(Clock * 6.f) : 1.f;
	Bar(X, Y, BW, BH, C->Sanity / 100.f, FLinearColor(0.6f * Pulse, 0.45f * Pulse, 0.95f * Pulse, 0.9f), TEXT("SANT\u00c9 MENTALE"));
	Y += 38.f * U;
	Bar(X, Y, BW, BH, C->Stamina / 100.f, FLinearColor(0.9f, 0.9f, 0.85f, 0.8f), TEXT("ENDURANCE"));
	Y += 38.f * U;
	Bar(X, Y, BW, BH, C->Battery / 100.f, FLinearColor(1.f, 0.85f, 0.3f, C->IsFlashlightOn() ? 0.95f : 0.45f), TEXT("PILES DE LA LAMPE"));
	Y += 22.f * U;
	Txt(FString::Printf(TEXT("Eau d'amande x%d      Piles x%d"), C->AlmondWater, C->Batteries), X, Y, FLinearColor(1.f, 1.f, 1.f, 0.8f), 0.85f * U,
		GEngine->GetMediumFont());
}

void ABRHUD::DrawCrosshair()
{
	ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	ABRCharacter* C = PC ? Cast<ABRCharacter>(PC->GetPawn()) : nullptr;
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const bool bFocus = C && !C->GetFocusPrompt().IsEmpty();
	const float S = (bFocus ? 6.f : 3.f) * U;
	DrawRect(FLinearColor(1.f, 1.f, 1.f, bFocus ? 0.9f : 0.45f), CX - S * 0.5f, CY - S * 0.5f, S, S);
	if (bFocus)
	{
		Txt(C->GetFocusPrompt(), CX, CY + 30.f * U, FLinearColor(1.f, 1.f, 1.f, 0.95f), 1.f * U, GEngine->GetMediumFont(), true);
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

void ABRHUD::DrawNote()
{
	ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	ABRCharacter* C = PC ? Cast<ABRCharacter>(PC->GetPawn()) : nullptr;
	if (!C)
	{
		return;
	}
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
	for (const FString& L : Wrap(C->GetOpenNote(), W - 80.f * U, Medium, 1.05f * U))
	{
		Txt(L, X + 40.f * U, LY, FLinearColor(0.12f, 0.1f, 0.25f), 1.05f * U, Medium, false, false);
		LY += 34.f * U;
	}
	Txt(TEXT("[E] Ranger la note"), X + W * 0.5f, Y + H - 50.f * U, FLinearColor(0.3f, 0.25f, 0.2f), 0.9f * U, Medium, true, false);
}

void ABRHUD::DrawJournal()
{
	ABRWorld* W = ABRWorld::Get(this);
	ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	ABRCharacter* C = PC ? Cast<ABRCharacter>(PC->GetPawn()) : nullptr;
	if (!W)
	{
		return;
	}
	const float U = Ui();
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	const FBRLevelDef& D = W->Def();
	DrawRect(FLinearColor(0.03f, 0.03f, 0.02f, 0.9f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

	// Colonne gauche : niveau
	float X = 80.f * U;
	float Y = 70.f * U;
	const float ColW = Canvas->ClipX * 0.4f;
	Txt(TEXT("JOURNAL DU VAGABOND"), X, Y, FLinearColor(1.f, 0.92f, 0.6f), 1.4f * U, Large);
	Y += 70.f * U;
	Txt(FString::Printf(TEXT("NIVEAU %d - \u00ab %s \u00bb"), D.Number, *D.Title), X, Y, FLinearColor::White, 1.1f * U, Large);
	Y += 44.f * U;
	Txt(D.Nickname, X, Y, FLinearColor(0.8f, 0.8f, 0.8f), 0.95f * U, Medium);
	Y += 30.f * U;
	Txt(D.ClassText, X, Y, ClassColor(D.SurvivalClass), 0.95f * U, Medium);
	Y += 40.f * U;
	for (const FString& L : Wrap(D.Description, ColW, Medium, 0.9f * U))
	{
		Txt(L, X, Y, FLinearColor(0.85f, 0.85f, 0.85f), 0.9f * U, Medium);
		Y += 27.f * U;
	}
	Y += 20.f * U;
	FString Visited;
	for (int32 N : W->GetVisitedLevels())
	{
		Visited += Visited.IsEmpty() ? FString::FromInt(N) : FString(TEXT(", ")) + FString::FromInt(N);
	}
	Txt(TEXT("Niveaux visit\u00e9s : ") + Visited, X, Y, FLinearColor(0.7f, 0.8f, 0.9f), 0.9f * U, Medium);
	Y += 30.f * U;
	if (C)
	{
		Txt(FString::Printf(TEXT("Temps sur ce niveau : %s      Notes lues : %d"), *Timecode(W->GetLevelTime()), C->NotesRead), X, Y,
			FLinearColor(0.7f, 0.8f, 0.9f), 0.9f * U, Medium);
		Y += 30.f * U;
	}
	Y += 20.f * U;
	Txt(TEXT("Pour quitter ce niveau : cherchez un passage (mur qui gr\u00e9sille, porte, \u00e9chelle...)."), X, Y,
		FLinearColor(0.9f, 0.85f, 0.6f), 0.85f * U, Medium);
	Y += 26.f * U;
	Txt(TEXT("Les sorties \u00e9mettent un bourdonnement \u00e9lectrique : \u00e9coutez."), X, Y, FLinearColor(0.9f, 0.85f, 0.6f), 0.85f * U, Medium);

	// Colonne droite : entites
	X = Canvas->ClipX * 0.52f;
	Y = 140.f * U;
	const float RW = Canvas->ClipX * 0.42f;
	Txt(TEXT("ENTIT\u00c9S RENCONTR\u00c9ES"), X, Y, FLinearColor(1.f, 0.45f, 0.4f), 1.1f * U, Large);
	Y += 50.f * U;
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
		Txt(FString::Printf(TEXT("%s - %s"), *Info.Number, *Info.Name), X, Y, FLinearColor(1.f, 0.85f, 0.8f), 1.f * U, Medium);
		Y += 30.f * U;
		for (const FString& L : Wrap(Info.Description, RW, Medium, 0.82f * U))
		{
			Txt(L, X + 16.f * U, Y, FLinearColor(0.82f, 0.82f, 0.82f), 0.82f * U, Medium);
			Y += 24.f * U;
		}
		for (const FString& L : Wrap(TEXT("Conseil : ") + Info.Advice, RW, Medium, 0.82f * U))
		{
			Txt(L, X + 16.f * U, Y, FLinearColor(0.6f, 0.95f, 0.65f), 0.82f * U, Medium);
			Y += 24.f * U;
		}
		Y += 12.f * U;
	}
	if (!bAny)
	{
		Txt(TEXT("Aucune pour l'instant... et c'est tr\u00e8s bien comme \u00e7a."), X, Y, FLinearColor(0.7f, 0.7f, 0.7f), 0.9f * U, Medium);
	}

	Txt(ControlsLine1, Canvas->ClipX * 0.5f, Canvas->ClipY - 80.f * U, FLinearColor(0.6f, 0.6f, 0.6f), 0.8f * U, Medium, true);
	Txt(ControlsLine2, Canvas->ClipX * 0.5f, Canvas->ClipY - 52.f * U, FLinearColor(0.6f, 0.6f, 0.6f), 0.8f * U, Medium, true);
}

void ABRHUD::DrawDeath()
{
	ABRPlayerController* PC = Cast<ABRPlayerController>(PlayerOwner);
	ABRCharacter* C = PC ? Cast<ABRCharacter>(PC->GetPawn()) : nullptr;
	if (!C)
	{
		return;
	}
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
	Txt(TEXT("VOUS \u00caTES MORT"), CX, H * 0.38f, FLinearColor(0.9f, 0.1f, 0.08f, A), 2.4f * U, GEngine->GetLargeFont(), true);
	if (!C->GetKilledBy().IsEmpty())
	{
		Txt(TEXT("Tu\u00e9 par : ") + C->GetKilledBy(), CX, H * 0.48f, FLinearColor(1.f, 0.8f, 0.8f, A), 1.1f * U, GEngine->GetMediumFont(), true);
	}
	const float A2 = FMath::Clamp((T - 2.2f) / 1.f, 0.f, 1.f);
	Txt(TEXT("Vous vous r\u00e9veillez... sur une moquette humide. Encore."), CX, H * 0.56f, FLinearColor(0.9f, 0.85f, 0.6f, A2), 1.f * U,
		GEngine->GetMediumFont(), true);
}

void ABRHUD::DrawPause()
{
	const float U = Ui();
	const float CX = Canvas->ClipX * 0.5f;
	const float H = Canvas->ClipY;
	UFont* Medium = GEngine->GetMediumFont();
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), 0.f, 0.f, Canvas->ClipX, H);
	Txt(TEXT("PAUSE"), CX, H * 0.25f, FLinearColor::White, 2.2f * U, GEngine->GetLargeFont(), true);
	Txt(ControlsLine1, CX, H * 0.42f, FLinearColor(0.85f, 0.85f, 0.85f), 0.9f * U, Medium, true);
	Txt(ControlsLine2, CX, H * 0.46f, FLinearColor(0.85f, 0.85f, 0.85f), 0.9f * U, Medium, true);
	Txt(TEXT("[ P ]  reprendre          [ FIN ]  quitter le jeu"), CX, H * 0.58f, FLinearColor(1.f, 0.92f, 0.6f), 1.f * U, Medium, true);
	Txt(TEXT("Console (touche \u00b2) : BRLevel 37  |  BRGod  |  BRSpawn 0-7  |  BRGiveAll  |  BRSensitivity 1.5  |  BRInvertY"), CX, H * 0.7f,
		FLinearColor(0.6f, 0.6f, 0.6f), 0.8f * U, Medium, true);
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
