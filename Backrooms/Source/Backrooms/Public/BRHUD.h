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
class UBRSaveGame;

UCLASS()
class BACKROOMS_API ABRHUD : public AHUD
{
	GENERATED_BODY()
	/** v4.8 : le test automatique mesure et coupe les textes comme le HUD (langues sans espaces, ecriture arabe) */
	friend class ABRAutoTest;

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

	enum class ETab : uint8 { Character, Journal, Settings, Keys };

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

	/** Graisse et alignement du texte de l'interface v4 (polices Slate du moteur : Roboto Light / Regular / Bold / Black) */
	enum class EUiWeight : uint8 { Light, Regular, Bold, Black };
	enum class EUiAlign : uint8 { Left, Center, Right };

	// ---- Menu principal (v4.0 : logo, cartes animees, carrousel des niveaux, astuces)
	void DrawMenu();
	void DrawMenuBackdrop(bool bCentered, float A);
	void DrawMenuMain(ABRPlayerController* PC, bool bInteractive);
	void DrawMenuSolo(ABRPlayerController* PC, bool bInteractive);
	void DrawMenuMulti(ABRPlayerController* PC, bool bInteractive);
	void DrawMenuJoin(ABRPlayerController* PC, bool bInteractive);
	void DrawMenuFooter(ABRPlayerController* PC, float A);
	/** Logo "THE BACKROOMS" (UI_Logo), avec un neon qui gresille de temps en temps */
	void DrawLogo(float X, float Y, float W, float A, bool bFlicker);
	/** Viseur du camescope autour du menu (REC, compteur) */
	void DrawTips(float X, float Y, float W, float A);
	/** Carte du carrousel : apercu du niveau (ses vraies textures), numero, titre, classe */
	void DrawLevelCard(int32 Index, float CX, float Top, float Scale, float Alpha, float Sel, bool bLocked, bool bCurrent);
	// ---- Parties (v4.1)
	void DrawMenuSaves(ABRPlayerController* PC, bool bInteractive);
	void DrawMenuNewSave(ABRPlayerController* PC, bool bInteractive);
	/** v4.8 : page Langue (22 langues sur deux colonnes, noms natifs) */
	void DrawMenuLanguage(ABRPlayerController* PC, bool bInteractive);
	/** Carte d'une partie : dernier niveau, nom, progression, temps de jeu, date */
	void DrawSaveCard(int32 Item, const UBRSaveGame* Save, float X, float Y, float W, float H, bool bInteractive, float Appear);
	/** Les niveaux en vignettes : explores (apercu) ou verrouilles (cadenas) */
	void DrawLevelGrid(const UBRSaveGame* Save, float X, float Y, float W, float A);
	/** Icone de sauvegarde automatique (en jeu) */
	void DrawSaveIndicator(float Since);
	void DrawLevelScene(const FBRLevelDef& D, float X, float Y, float W, float H, float Scale, float Alpha);
	/** Carte cliquable du menu : icone dans un cercle, libelle, sous-titre ; Sel = animation de selection (0..1) */
	void DrawCard(float X, float Y, float W, float H, float Sel, const FString& Label, const FString& Sub, const TCHAR* IconName, float Alpha, bool bDanger = false);
	void MenuCard(int32 Item, float X, float Y, float W, float H, const FString& Sub, const TCHAR* IconName, bool bInteractive, float Appear);
	/** Bouton en pastille (pages Solo et Rejoindre) */
	void MenuPill(int32 Item, float X, float Y, float W, float H, const TCHAR* IconName, bool bPrimary, bool bInteractive, float Alpha, bool bDanger = false,
		bool bDisabled = false);
	void HandleMenuMouse(ABRPlayerController* PC);
	/** Noms des coequipiers au-dessus de leur tete */
	void DrawTeammates(ABRCharacter* C);
	/** Micro : transmet / touche pour parler (en multijoueur) */
	void DrawVoiceIndicator(ABRPlayerController* PC);
	/** Pause en multijoueur : joueurs, hote et latence */
	void DrawPlayerList();
	void DrawTitleCard();
	void DrawRecording(ABRCharacter* C, ABRWorld* W);
	/** v4.4 : pastille "DEV" et aide des raccourcis du mode developpeur */
	void DrawDevOverlay(ABRPlayerController* PC, ABRCharacter* C, ABRWorld* W);
	/** v4.4 : decor a l'ecran du jumpscare en cours (griffures, neige, essaim, confettis...), propre a chaque entite */
	void DrawJumpscare(ABRCharacter* C);
	void DrawStats(ABRCharacter* C);
	void DrawQuickBar(ABRCharacter* C);
	void DrawObjectiveTracker(ABRWorld* W);
	void DrawCrosshair(ABRCharacter* C);
	void DrawMessages(float Dt);
	void DrawNote(ABRCharacter* C);
	void DrawDeath(ABRCharacter* C);
	void DrawPause(ABRPlayerController* PC);
	void DrawGlitch(float Amount);
	void DrawContentWarning(float Y);

	// ---- Inventaire (TAB)
	void DrawInventory(ABRPlayerController* PC, ABRCharacter* C, ABRWorld* W);
	void LayoutCharacterTab(ABRCharacter* C);
	void DrawCharacterTab(ABRCharacter* C, ABRWorld* W);
	void DrawJournalTab(ABRCharacter* C, ABRWorld* W);
	void DrawSettingsTab(ABRPlayerController* PC);
	void DrawKeysTab(ABRPlayerController* PC);
	void HandlePauseMouse(ABRPlayerController* PC);
	FString ControlsLine(int32 Line) const;
	void HandleInventoryMouse(ABRPlayerController* PC, ABRCharacter* C);
	void DrawSlot(ABRCharacter* C, const FSlotBox& Box);
	void DrawTooltip(ABRCharacter* C);
	void DrawInspect(ABRCharacter* C);

	// ---- Primitives v4 : texte net en plusieurs graisses, formes arrondies, degrades, halos
	/** Size : taille de police (points) a 1080p, mise a l'echelle de l'ecran */
	void TextF(const FString& S, float X, float Y, const FLinearColor& C, float Size, EUiWeight Weight = EUiWeight::Regular,
		EUiAlign Align = EUiAlign::Left, bool bShadow = true);
	/** Texte espace (lettres separees de Spacing pixels) ; renvoie la largeur */
	float TextSpaced(const FString& S, float X, float Y, const FLinearColor& C, float Size, EUiWeight Weight, float Spacing,
		EUiAlign Align = EUiAlign::Left);
	FVector2f TextSize(const FString& S, float Size, EUiWeight Weight) const;
	/** Lignes d'au plus MaxWidth pixels. v4.8 : coupure selon les regles Unicode de la langue (entre les ideogrammes en
	 *  chinois et en japonais, jamais devant un point ou un guillemet fermant), mots trop longs coupes entre deux graphemes, retours a la ligne gardes */
	TArray<FString> WrapF(const FString& S, float MaxWidth, float Size, EUiWeight Weight) const;
	/** Raccourcit avec des points de suspension sans couper un grapheme (lettre + accents, paire de substitution) */
	FString Ellipsize(const FString& S, float MaxWidth, float Size, EUiWeight Weight) const;
	/** Taille a laquelle S tient dans MaxWidth (reduite jusqu'a MinScale x Size) */
	float FitSize(const FString& S, float MaxWidth, float Size, EUiWeight Weight, float MinScale = 0.72f) const;
	/** TextF ajuste a MaxWidth : taille reduite si besoin, puis points de suspension */
	void TextFit(const FString& S, float X, float Y, float MaxWidth, const FLinearColor& C, float Size, EUiWeight Weight = EUiWeight::Regular,
		EUiAlign Align = EUiAlign::Left, bool bShadow = true);
	/** Paragraphe deja coupe : aligne a gauche, ou a droite dans une langue ecrite de droite a gauche (arabe, persan) */
	void DrawParagraph(const TArray<FString>& Lines, float X, float Y, float W, float LineH, const FLinearColor& C, float Size, EUiWeight Weight);
	UTexture* UiTex(const TCHAR* Name);
	/** Rectangle arrondi (UI_Round decoupee en 9) ; bOutline : contour seul (UI_RoundLine) */
	void RoundRect(float X, float Y, float W, float H, float R, const FLinearColor& C, bool bOutline = false);
	/** Degrade : Dir 0 opaque a gauche, 1 opaque a droite, 2 opaque en haut, 3 opaque en bas */
	void Gradient(float X, float Y, float W, float H, const FLinearColor& C, int32 Dir);
	void Glow(float CX, float CY, float RX, float RY, const FLinearColor& C);
	/** Touche dessinee comme une touche de clavier, suivie de son action ; renvoie la largeur */
	float KeyCap(float X, float Y, const FString& Key, const FString& Label, float Alpha, bool bDraw = true);
	void KeyHints(float X, float Y, const TArray<TPair<FString, FString>>& Hints, float Alpha, bool bCenter);

	// ---- Primitives (v4.8 : dessinees avec la police de l'interface et ses ecritures, a la taille de l'ancienne police)
	void Txt(const FString& S, float X, float Y, const FLinearColor& C, float Scale, UFont* Font, bool bCenter = false, bool bShadow = true);
	void TxtRight(const FString& S, float RightX, float Y, const FLinearColor& C, float Scale, UFont* Font);
	float TextW(const FString& S, UFont* Font, float Scale);
	/** Ligne d'un paragraphe de largeur W (Wrap) : a gauche, ou a droite en arabe et en persan */
	void TxtLine(const FString& S, float X, float Y, float W, const FLinearColor& C, float Scale, UFont* Font);
	void Bar(float X, float Y, float W, float H, float Fill, const FLinearColor& C, const FString& Label);
	void Frame(float X, float Y, float W, float H, const FLinearColor& C, float Thickness);
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
	float LastMenuMouseX = -1.f;
	float LastMenuMouseY = -1.f;

	// Animations de l'interface v4
	float UiDt = 0.f;
	float MenuIntro = 0.f;      // temps depuis l'ouverture du menu titre
	float MenuPageTime = 0.f;   // temps depuis le dernier changement de page
	int32 LastMenuPage = -1;
	float MenuSel[24] = {};     // selection animee des elements de la page (24 : page Langue, 22 langues + RETOUR)
	int32 CoverageFor = -2;     // v4.8 : langue dont la couverture des traductions est en cache
	int32 CoverageMissing = 0;
	float Carousel = -1000.f;   // position animee du carrousel des niveaux (en niveaux)
	float TipClock = 0.f;
	float PauseTime = 0.f;
	float PauseSel[5] = { 0.f, 0.f, 0.f, 0.f, 0.f };
	bool bWasInMenu = false;
	bool bWasPaused = false;
	bool bWasInventoryOpen = false;

	// Zone de l'inventaire (calculee a chaque image)
	float IX = 0.f, IY = 0.f, IW = 0.f, IH = 0.f;
	float ColW[3] = { 0.f, 0.f, 0.f };
	float ColX[3] = { 0.f, 0.f, 0.f };
	float SilX = 0.f, SilY = 0.f, SilW = 0.f, SilH = 0.f;
};
