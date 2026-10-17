// v4.16 (vis) : banc de la finition graphique et du HUD (-BRAutoTest -BRAutoTestV416Visual). Captures et rapport :
//   1. HUD : inventaire (PERSONNAGE, CARNET, JOURNAL, PARAMETRES > VIDEO, TOUCHES) et pause, rendus hors ecran aux tailles
//      exactes 1280 x 720, 1920 x 1080 et 2560 x 1080 (cible de rendu : la fenetre de test ne peut pas depasser l'ecran),
//      dans les 22 langues : chaque texte dessine est releve (ABRHUD::bTestRecordTexts) et controle : dans l'image, dans son
//      panneau, dans la largeur permise par son appelant, sans chevauchement ; textes raccourcis et tailles effectives notes.
//      Captures v416_hud_<taille>_<langue>_<onglet> (fr, de, ru, ar, ja) ;
//   2. vraies fenetres : 1280 x 720, puis les plus grandes fenetres 16:9 et 64:27 que l'ecran accepte (tailles obtenues
//      notees), memes controles sur l'image reelle ; modes d'affichage : les trois presents dans PARAMETRES > VIDEO, essai du
//      plein ecran fenetre (sans bordures) et retour (le plein ecran exclusif seulement avec -BRV416PleinEcran) ;
//   3. manette : invites des boutons (HUD, pause, inventaire, scanner) = liaisons reelles de la manette ; inventaire a la
//      manette par de vraies touches injectees (LB / RB, croix, Y, B) ;
//   4. scanner du Level 6 (graine 4136) : HUD contextuel (rappel des touches au clavier et a la manette, recharge, cone du
//      focus, releve de la mire, repos, aide des parasites derriere et devant) ;
//   5. les neuf entites (Level 4, graine 4242 ; Smiler dans le noir) : premiere personne au repos et en menace (geste arme),
//      troisieme personne de face et de cote (vue d'un coequipier : affectation des materiaux, appui des pieds, poses) ;
//      hauteur du pied le plus bas notee ;
//   6. kit du Level 0 (graine 9) : raccords des plinthes le long de chaque face de mur, plafonds et sols cellule par cellule
//      (boites des instances posees), captures des raccords (angle, poteau droit, jambage, plafond contre mur, plafond entre
//      deux cellules) ;
//   7. decalques d'usure (Levels 0 graines 9 et 4242, 1 et 2 graine 4242) : surface sous chaque decalque (traces), aucun sur
//      une ouverture ou un objet, taches au sol posees (ni flottantes ni enfouies), nombre par chunk, distance utile, motifs.
// Une condition manquante (scanner, kit, decalques, cible de rendu, entite) est notee NON VERIFIE, jamais comptee reussie.
// Reglages, langue, affichage et manette du joueur retablis au bilan.
#include "BRAutoTest.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BRChunk.h"
#include "BRDecalLogic.h"
#include "BRDisplay.h"
#include "BREntity.h"
#include "BRHUD.h"
#include "BRKeys.h"
#include "BRLevels.h"
#include "BRLidar.h"
#include "BRLidarLogic.h"
#include "BRLoc.h"
#include "BRMission.h"
#include "BRPlayerController.h"
#include "BRWorld.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Materials/Material.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

namespace BRTestV416Vis
{
	const TCHAR* Oui(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	/** Tailles du HUD hors ecran (exactes) */
	struct FSizeCase
	{
		int32 W;
		int32 H;
		const TCHAR* Name;
	};
	const FSizeCase OffSizes[] = { { 1280, 720, TEXT("1280x720") }, { 1920, 1080, TEXT("1920x1080") }, { 2560, 1080, TEXT("2560x1080") } };
	/** Langues capturees (les autres sont controlees sans capture) */
	const TCHAR* const ShotLangs[] = { TEXT("fr"), TEXT("de"), TEXT("ru"), TEXT("ar"), TEXT("ja") };
	/** Ecrans du HUD controles : onglets de l'inventaire (ABRHUD::ETab : 0 personnage, 4 carnet, 1 journal, 2 parametres, 3
	 *  touches), la pause (-1) et le HUD du jeu, sans menu (-2) */
	struct FScreen
	{
		int32 Tab;
		const TCHAR* Name;
	};
	const FScreen Screens[] = { { 0, TEXT("personnage") }, { 4, TEXT("carnet") }, { 1, TEXT("journal") }, { 2, TEXT("parametres") }, { 3, TEXT("touches") }, { -1, TEXT("pause") }, { -2, TEXT("jeu") } };

	/** Graines (memes poses d'une version a l'autre) */
	constexpr uint32 HudSeed = 4242u;     // Level 1 : mission (carnet rempli)
	constexpr uint32 LidarSeed = 4136u;   // Level 6
	constexpr uint32 GallerySeed = 4242u; // Level 4 (bureau eclaire)
	constexpr uint32 KitSeed = 9u;        // Level 0 : scene du kit

	/** Fond des rendus hors ecran (image du jeu assombrie par l'inventaire) */
	const FLinearColor Backdrop(0.05f, 0.045f, 0.03f, 1.f);

	/** Un texte dessine (copie de ABRHUD::FTestText) */
	struct FDrawnText
	{
		FString Text;
		float X = 0.f, Y = 0.f, W = 0.f, H = 0.f;
		float PX = 0.f, PY = 0.f, PW = 0.f, PH = 0.f;
		float MaxW = 0.f;
		float Points = 0.f;
		bool bCut = false;
	};

	/** Bilan des controles de textes */
	struct FTextReport
	{
		int32 Draws = 0;
		int32 Texts = 0;
		int32 OutCanvas = 0;
		int32 OverWidth = 0;
		int32 OutPanel = 0;
		int32 Overlaps = 0;
		int32 Cut = 0;
		int32 AtFloor = 0;
		float MinPx = 1.0e9f;
		TArray<FString> Examples;
		TArray<FString> CutExamples;
		void Example(const FString& S)
		{
			if (Examples.Num() < 14)
			{
				Examples.Add(S);
			}
		}
		int32 Problems() const { return OutCanvas + OverWidth + OutPanel + Overlaps; }
	};

	FString Short(const FString& S)
	{
		return S.Len() > 48 ? S.Left(46) + TEXT("..") : S;
	}

	/** Controles d'une image : textes dans l'image (W x H), dans leur panneau, dans la largeur permise, sans chevauchement */
	void CheckTexts(const TArray<FDrawnText>& T, float W, float H, const FString& Where, FTextReport& R)
	{
		++R.Draws;
		R.Texts += T.Num();
		for (int32 i = 0; i < T.Num(); ++i)
		{
			const FDrawnText& A = T[i];
			const float Px = A.Points * 96.f / 72.f;
			R.MinPx = FMath::Min(R.MinPx, Px);
			R.AtFloor += A.Points <= 6.01f ? 1 : 0;
			if (A.bCut)
			{
				++R.Cut;
				if (R.CutExamples.Num() < 10)
				{
					R.CutExamples.Add(FString::Printf(TEXT("%s \"%s\""), *Where, *Short(A.Text)));
				}
			}
			if (A.X < -1.f || A.Y < -1.f || A.X + A.W > W + 1.f || A.Y + A.H > H + 1.f)
			{
				++R.OutCanvas;
				R.Example(FString::Printf(TEXT("%s : hors de l'image (%.0f, %.0f, %.0f x %.0f) \"%s\""), *Where, A.X, A.Y, A.W, A.H, *Short(A.Text)));
			}
			if (A.MaxW > 0.f && A.W > A.MaxW + 1.5f)
			{
				++R.OverWidth;
				R.Example(FString::Printf(TEXT("%s : %.0f px pour %.0f permis \"%s\""), *Where, A.W, A.MaxW, *Short(A.Text)));
			}
			if (A.PW > 0.f && (A.X < A.PX - 2.f || A.X + A.W > A.PX + A.PW + 2.f || A.Y < A.PY - 2.f || A.Y + A.H > A.PY + A.PH + 2.f))
			{
				++R.OutPanel;
				R.Example(FString::Printf(TEXT("%s : hors de son panneau (texte %.0f..%.0f, panneau %.0f..%.0f) \"%s\""), *Where, A.X, A.X + A.W, A.PX, A.PX + A.PW, *Short(A.Text)));
			}
			// Chevauchement : coeur des lettres (60 % de la hauteur de ligne), 1 px de marge laterale
			const float AX0 = A.X + 1.f, AX1 = A.X + A.W - 1.f, AY0 = A.Y + A.H * 0.2f, AY1 = A.Y + A.H * 0.8f;
			for (int32 j = i + 1; j < T.Num(); ++j)
			{
				const FDrawnText& B = T[j];
				if (A.Text == B.Text && FMath::Abs(A.X - B.X) < 0.6f && FMath::Abs(A.Y - B.Y) < 0.6f)
				{
					continue; // meme texte dessine deux fois au meme endroit
				}
				const float BX0 = B.X + 1.f, BX1 = B.X + B.W - 1.f, BY0 = B.Y + B.H * 0.2f, BY1 = B.Y + B.H * 0.8f;
				const float IW = FMath::Min(AX1, BX1) - FMath::Max(AX0, BX0);
				const float IH = FMath::Min(AY1, BY1) - FMath::Max(AY0, BY0);
				if (IW > 1.f && IH > 1.f && IW * IH > 4.f)
				{
					++R.Overlaps;
					R.Example(FString::Printf(TEXT("%s : \"%s\" chevauche \"%s\" (%.0f x %.0f px)"), *Where, *Short(A.Text), *Short(B.Text), IW, IH));
				}
			}
		}
	}

	FString ReportLine(const FTextReport& R)
	{
		return FString::Printf(TEXT("%d image(s), %d texte(s) : hors de l'image %d, au-dela de la largeur permise %d, hors du panneau %d, chevauchements %d ; "
			"raccourcis (points de suspension) %d ; plus petit texte %.1f px (cadratin), %d texte(s) au plancher de 6 points"), R.Draws, R.Texts, R.OutCanvas, R.OverWidth,
			R.OutPanel, R.Overlaps, R.Cut, R.MinPx < 1.0e8f ? R.MinPx : 0.f, R.AtFloor);
	}

	/** Cibles de rendu par taille (gardees du ramasse-miettes, liberees au bilan) */
	TMap<int64, TStrongObjectPtr<UTextureRenderTarget2D>> Targets;

	UTextureRenderTarget2D* Target(UObject* Context, int32 W, int32 H)
	{
		const int64 Key = static_cast<int64>(W) * 100000 + H;
		if (const TStrongObjectPtr<UTextureRenderTarget2D>* Found = Targets.Find(Key))
		{
			return Found->Get();
		}
		UTextureRenderTarget2D* RT = UKismetRenderingLibrary::CreateRenderTarget2D(Context, W, H, RTF_RGBA8_SRGB, Backdrop, false, false);
		if (RT)
		{
			Targets.Add(Key, TStrongObjectPtr<UTextureRenderTarget2D>(RT));
		}
		return RT;
	}

	// ---- etat du banc
	bool bSaved = false;
	FBRSettings SavedSettings;
	FString SavedLang;
	BRDisplay::FMode SavedMode;
	bool bDisplayTouched = false;
	int32 SavedStaticAid = 0;
	FTextReport OffReport;
	TMap<FString, FTextReport> PerLanguage;
	FTextReport PadReport;
	FTextReport RealReport;
	FTextReport LidarReport;
	int32 ShotCount = 0;
	/** Jauges d'endurance et de sante mentale du dernier rendu hors ecran (ABRHUD::StatusGaugesDrawn) ; bilan : rendus de
	 *  PERSONNAGE (2 attendues), autres ecrans (0 attendue), commandes de la console sous la pause d'un joueur (0 attendue) */
	int32 LastGauges = -1;
	int32 GaugeCharDraws = 0;
	int32 GaugeCharBad = 0;
	int32 GaugeOtherDraws = 0;
	int32 GaugeOtherBad = 0;
	int32 ConsoleHints = 0;
	int32 Phase = 0;
	double PhaseClock = 0.0;
	int32 Index = 0;
	FVector Anchor = FVector::ZeroVector;
	FRotator AnchorRot = FRotator::ZeroRotator;
	TWeakObjectPtr<ABREntity> Entity;
	FString PadFooterSeen;

	/** Graphemes de controle : langue d'un rapport */
	FString LangTag()
	{
		return BRLoc::Current().Code;
	}

	/** Une case hors du sol praticable (mur, fosse, bassin) */
	bool PlainCell(const ABRWorld* W, const FIntPoint& Cell)
	{
		return W->IsWalkable(Cell) && !W->IsPoolCell(Cell.X, Cell.Y) && !W->IsPitRoomCell(Cell.X, Cell.Y) && W->IsChunkLoaded(W->CellToChunk(Cell));
	}

	/** Materiaux d'un acteur (maillages visibles) : emplacements, vides, materiau par defaut du moteur (damier gris : affectation
	 *  perdue) ; noms des maillages fautifs */
	FString MaterialAudit(const AActor* A, int32& OutBad)
	{
		int32 Meshes = 0, Slots = 0, Empty = 0, Default = 0;
		TArray<FString> Where;
		const UMaterial* EngineDefault = UMaterial::GetDefaultMaterial(MD_Surface);
		TArray<UMeshComponent*> Comps;
		A->GetComponents<UMeshComponent>(Comps);
		for (const UMeshComponent* M : Comps)
		{
			if (!M || !M->IsVisible() || M->bHiddenInGame)
			{
				continue;
			}
			++Meshes;
			for (int32 i = 0; i < M->GetNumMaterials(); ++i)
			{
				++Slots;
				const UMaterialInterface* Mi = M->GetMaterial(i);
				const bool bEmpty = Mi == nullptr;
				const bool bDefault = !bEmpty && Mi->GetMaterial() == EngineDefault;
				Empty += bEmpty ? 1 : 0;
				Default += bDefault ? 1 : 0;
				if ((bEmpty || bDefault) && Where.Num() < 4)
				{
					Where.Add(FString::Printf(TEXT("%s[%d]"), *M->GetName(), i));
				}
			}
		}
		OutBad = Empty + Default;
		return FString::Printf(TEXT("%d maillage(s) visible(s), %d emplacement(s) de materiau, %d vide(s), %d au materiau par defaut du moteur%s%s"), Meshes, Slots, Empty, Default,
			Where.Num() ? TEXT(" : ") : TEXT(""), *FString::Join(Where, TEXT(", ")));
	}

	/** Joueur pose sans vitesse, regard vers Target */
	void PlaceLooking(ABRCharacter* C, const FVector& Feet, const FVector& Target)
	{
		const float Half = C->GetSimpleCollisionHalfHeight();
		C->SetActorLocation(FVector(Feet.X, Feet.Y, Feet.Z + Half + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* M = C->GetCharacterMovement())
		{
			M->StopMovementImmediately();
		}
		if (AController* Ctrl = C->GetController())
		{
			Ctrl->SetControlRotation((Target - C->GetEyeLocation()).Rotation());
		}
	}

	// ---- decalques : mesures sur les chunks construits
	struct FDecalStats
	{
		int32 Chunks = 0;
		int32 Projected = 0;
		int32 Patches = 0;
		int32 Expected = 0;
		int32 KindCount[4] = { 0, 0, 0, 0 };
		int32 MaxPerChunk = 0;
		int32 MaxPatchesPerChunk = 0;
		int32 MinPerChunk = MAX_int32;
		int32 Samples = 0;
		int32 OnSurface = 0;
		int32 Void = 0;
		int32 Blocked = 0;
		int32 OnObject = 0;
		int32 Oblique = 0;
		int32 DecalsFloating = 0;
		int32 DecalsPartial = 0;
		int32 OverOpening = 0;
		int32 PatchSamples = 0;
		int32 PatchFloating = 0;
		int32 PatchBuried = 0;
		int32 PatchHidden = 0;
		float PatchMaxGap = 0.f;
		int32 Flipped = 0;
		int32 Scuffs = 0;
		float FadeMin = 1.0e9f;
		float FadeMax = 0.f;
		int32 WithinUseful = 0;
		TArray<FString> Examples;
		TMap<FString, int32> ObjectHits;
		TMap<int32, int32> Signatures;
		void Example(const FString& S)
		{
			if (Examples.Num() < 8)
			{
				Examples.Add(S);
			}
		}
	};
}

void ABRAutoTest::AddV416VisualSteps()
{
	using namespace BRTestV416Vis;

	// ------------------------------------------------------------------------------------------------ outils (contexte membre :
	// acces amis au HUD et au controleur)
	auto HudOf = [this]() -> ABRHUD*
	{
		ABRPlayerController* PC = GetPC();
		return PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr;
	};
	auto Grab = [](const ABRHUD* H, TArray<FDrawnText>& Out)
	{
		Out.Reset();
		for (const ABRHUD::FTestText& T : H->TestTexts)
		{
			FDrawnText D;
			D.Text = T.Text;
			D.X = T.Rect.X;
			D.Y = T.Rect.Y;
			D.W = T.Rect.Z;
			D.H = T.Rect.W;
			D.PX = T.Panel.X;
			D.PY = T.Panel.Y;
			D.PW = T.Panel.Z;
			D.PH = T.Panel.W;
			D.MaxW = T.MaxW;
			D.Points = T.Points;
			D.bCut = T.bCut;
			Out.Add(D);
		}
	};
	// Prepare l'ecran demande (onglet de l'inventaire ou pause) sans attendre d'image
	auto ShowScreen = [this, HudOf](int32 Tab, bool bParkMouse = true)
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = HudOf();
		if (!PC || !H)
		{
			return;
		}
		H->Messages.Reset();
		if (Tab == -2)
		{
			// HUD du jeu : ni inventaire ni pause
			PC->SetInventoryOpen(false);
			if (PC->IsPauseMenuOpen())
			{
				PC->TogglePause();
			}
			return;
		}
		if (Tab < 0)
		{
			PC->SetInventoryOpen(false);
			if (!PC->IsPauseMenuOpen())
			{
				PC->TogglePause();
			}
			H->PauseTime = 10.f; // animation d'ouverture finie
			H->bWasPaused = true; // sinon le prochain dessin la remet a zero (pause vue pour la premiere fois)
			return;
		}
		if (PC->IsPauseMenuOpen())
		{
			PC->TogglePause();
		}
		PC->SetInventoryOpen(true, Tab);
		H->Tab = static_cast<ABRHUD::ETab>(Tab);
		H->SettingsCategory = 1; // VIDEO : le mode d'affichage
		H->Inspecting = ABRHUD::FSlotRef();
		H->Dragging = ABRHUD::FSlotRef();
		if (bParkMouse)
		{
			PC->SetMouseLocation(5, 5); // aucune case survolee (pas d'infobulle au hasard)
		}
	};
	auto CloseScreens = [this]()
	{
		if (ABRPlayerController* PC = GetPC())
		{
			PC->SetInventoryOpen(false);
			if (PC->IsPauseMenuOpen())
			{
				PC->TogglePause();
			}
		}
	};
	// Dessine le HUD dans une cible de rendu de W x H (taille exacte, independante de l'ecran) ; Png : capture (vide : aucune)
	auto Offscreen = [this, HudOf, Grab](int32 W, int32 H, const FString& Png, TArray<FDrawnText>& Out) -> bool
	{
		ABRHUD* Hud = HudOf();
		UWorld* World = GetWorld();
		if (!Hud || !World || !FApp::CanEverRender())
		{
			return false;
		}
		UTextureRenderTarget2D* RT = BRTestV416Vis::Target(this, W, H);
		if (!RT)
		{
			return false;
		}
		UKismetRenderingLibrary::ClearRenderTarget2D(this, RT, BRTestV416Vis::Backdrop);
		UCanvas* Cv = nullptr;
		FVector2D Size(0.0, 0.0);
		FDrawToRenderTargetContext Ctx;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, RT, Cv, Size, Ctx);
		if (!Cv)
		{
			return false;
		}
		UCanvas* PrevCanvas = Hud->Canvas;
		UCanvas* PrevDebug = Hud->DebugCanvas;
		Hud->bTestRecordTexts = true;
		Hud->SetCanvas(Cv, Cv);
		Hud->DrawHUD();
		BRTestV416Vis::LastGauges = Hud->StatusGaugesDrawn; // jauges de ce rendu (remis a zero au debut de DrawHUD)
		Hud->SetCanvas(PrevCanvas, PrevDebug);
		Hud->bTestRecordTexts = false;
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Ctx);
		Grab(Hud, Out);
		if (!Png.IsEmpty())
		{
			UKismetRenderingLibrary::ExportRenderTarget(this, RT, OutDir, Png + TEXT(".png"));
			++BRTestV416Vis::ShotCount;
		}
		return true;
	};
	auto Section = [this](const FString& Title, int32 Level)
	{
		FLevelReport R;
		R.Level = Level;
		R.Title = Title;
		R.Scene = TEXT("v416vis");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
	};
	auto ShotNamed = [this](const FString& Name)
	{
		Shot(Name);
		++BRTestV416Vis::ShotCount;
	};

	// ================================================================================================ 0. debut
	Add(TEXT("v4.16 visuel : debut"), 0.f, [this, Section]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return false;
		}
		Section(TEXT("v4.16 visuel : HUD, manette, langues, modes d'affichage"), 0);
		if (!BRTestV416Vis::bSaved)
		{
			BRTestV416Vis::SavedSettings = FBRSettings::Get();
			BRTestV416Vis::SavedLang = BRLoc::Current().Code;
			BRTestV416Vis::SavedMode = BRDisplay::Current();
			BRTestV416Vis::SavedStaticAid = ABRLidar::Settings().StaticAid;
			BRTestV416Vis::bSaved = true;
		}
		// HUD du joueur ordinaire : taille 1, opacite pleine, pas de mode developpeur (sa pastille et ses aides)
		FBRSettings& S = FBRSettings::Get();
		S.UiScale = 1.f;
		S.HudOpacity = 1.f;
		S.bDevMode = false;
		S.bMotionBlur = false;
		PC->ApplySettings();
		PC->bPadActive = false;
		BRKeys::SetGamepadPrompts(false);
		const BRDisplay::FMonitor M = BRDisplay::ActiveMonitor();
		const BRDisplay::FMode Eff = BRDisplay::Effective();
		Note(FString::Printf(TEXT("ecran : %d x %d (zone utile %d x %d, %d ppp, %d ecran(s)) ; fenetre du jeu : mode %d, %d x %d ; affichage modifiable : %s"), M.Size.X, M.Size.Y,
			M.WorkSize.X, M.WorkSize.Y, M.DPI, M.Count, Eff.Window, Eff.Resolution.X, Eff.Resolution.Y, BRTestV416Vis::Oui(BRDisplay::CanChange())));
		return true;
	});

	// ================================================================================================ 1. Level 1 : HUD
	AddSeedLoad(1, HudSeed, TEXT("v4.16 visuel : Level 1 (HUD, carnet rempli)"));
	Add(TEXT("v4.16 visuel : carnet rempli (indices et observations lus, aide au rappel)"), 1.f, [this, HudOf]()
	{
		ABRWorld* W = GetBRWorld();
		ABRHUD* H = HudOf();
		if (!W || !H || !GetPlayer())
		{
			return StepTime > 20.f;
		}
		int32 Read = 0;
		if (W->IsMissionActive())
		{
			const BRMission::FPlan& P = W->GetMissionPlan();
			for (int32 D = 0; D < P.NumDevices; ++D)
			{
				if (P.Devices[D].Kind == BRMission::EKind::Clue || P.Devices[D].Kind == BRMission::EKind::Observe)
				{
					W->DebugCompleteMissionDevice(D);
					++Read;
				}
			}
		}
		Note(FString::Printf(TEXT("Level 1 : mission %s, %d indice(s) et observation(s) lus pour remplir le carnet"), BRTestV416Vis::Oui(W->IsMissionActive()), Read));
		if (!W->IsMissionActive())
		{
			Skip(TEXT("Level 1 sans mission : carnet sans etapes ni observations"));
		}
		return true;
	});
	Add(TEXT("v4.16 visuel : carnet, aide au rappel"), 0.5f, [this, HudOf, ShowScreen]()
	{
		ABRHUD* H = HudOf();
		if (!H)
		{
			return true;
		}
		ShowScreen(4);
		if (StepTime < 0.3f || H->NotebookHintStep < 0)
		{
			return StepTime > 3.f; // le carnet dessine fixe l'etape en cours (cle de l'aide)
		}
		H->NotebookHint.FindOrAdd(H->NotebookHintStep) = 2; // aide et rappel : la colonne la plus longue
		return true;
	});

	// ------------------------------------------------------------------------------------------------ 1.1 hors ecran, 22 langues
	for (int32 LangIndex = 0; LangIndex < BRLoc::Languages().Num(); ++LangIndex)
	{
		const FString Code = BRLoc::Languages()[LangIndex].Code;
		Add(FString::Printf(TEXT("v4.16 visuel : langue %s"), *Code), 0.3f, [this, Code]()
		{
			if (!BRLoc::SetLanguage(Code))
			{
				Skip(FString::Printf(TEXT("%s : langue refusee (donnees ICU absentes ?) : HUD non controle dans cette langue"), *Code));
			}
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 visuel : HUD hors ecran en %s"), *Code), 0.1f, [this, Code, ShowScreen, CloseScreens, Offscreen]()
		{
			if (LangTag() != Code)
			{
				return true;
			}
			bool bShotLang = false;
			for (const TCHAR* L : ShotLangs)
			{
				bShotLang |= Code == L;
			}
			FTextReport& Lang = PerLanguage.FindOrAdd(Code);
			int32 Failed = 0;
			for (const FScreen& Sc : Screens)
			{
				ShowScreen(Sc.Tab);
				for (const FSizeCase& Sz : OffSizes)
				{
					// Captures : personnage et carnet dans les cinq langues ; les autres ecrans en francais, allemand et arabe
					const bool bPng = bShotLang && (Sc.Tab == 0 || Sc.Tab == 4 || Code == TEXT("fr") || Code == TEXT("de") || Code == TEXT("ar"));
					TArray<FDrawnText> Texts;
					if (!Offscreen(Sz.W, Sz.H, bPng ? FString::Printf(TEXT("v416_hud_%s_%s_%s"), Sz.Name, *Code, Sc.Name) : FString(), Texts))
					{
						++Failed;
						continue;
					}
					const FString Where = FString::Printf(TEXT("%s %s %s"), Sz.Name, *Code, Sc.Name);
					CheckTexts(Texts, static_cast<float>(Sz.W), static_cast<float>(Sz.H), Where, OffReport);
					CheckTexts(Texts, static_cast<float>(Sz.W), static_cast<float>(Sz.H), Where, Lang);
					// Pas de barre de vie ; endurance et sante mentale dans PERSONNAGE seulement ; commandes de la console reservees
					// au mode developpeur
					if (Sc.Tab == 0)
					{
						++GaugeCharDraws;
						GaugeCharBad += LastGauges == 2 ? 0 : 1;
					}
					else
					{
						++GaugeOtherDraws;
						GaugeOtherBad += LastGauges == 0 ? 0 : 1;
					}
					for (const FDrawnText& T : Texts)
					{
						ConsoleHints += T.Text.Contains(TEXT("BRGod")) ? 1 : 0;
					}
				}
			}
			CloseScreens();
			if (Failed > 0)
			{
				Skip(FString::Printf(TEXT("%s : %d rendu(s) hors ecran impossibles (cible de rendu indisponible)"), *Code, Failed));
			}
			return true;
		});
	}
	Add(TEXT("v4.16 visuel : bilan des langues"), 0.f, [this]()
	{
		Note(FString::Printf(TEXT("HUD hors ecran (1280x720, 1920x1080, 2560x1080 ; 5 onglets et la pause ; %d langues) : %s"), PerLanguage.Num(), *ReportLine(OffReport)),
			OffReport.Problems() > 0);
		for (const FString& E : OffReport.Examples)
		{
			Note(TEXT("  ") + E, true);
		}
		for (const TPair<FString, FTextReport>& KV : PerLanguage)
		{
			if (KV.Value.Problems() > 0 || KV.Value.Cut > 0)
			{
				Note(FString::Printf(TEXT("  %s : %d probleme(s), %d texte(s) raccourci(s)"), *KV.Key, KV.Value.Problems(), KV.Value.Cut));
			}
		}
		for (const FString& E : OffReport.CutExamples)
		{
			Note(TEXT("  raccourci : ") + E);
		}
		if (OffReport.Draws == 0)
		{
			Skip(TEXT("HUD hors ecran : aucun rendu (cible de rendu indisponible) : tailles 1920x1080 et 2560x1080 non verifiees"));
		}
		else
		{
			Note(FString::Printf(TEXT("jauges : endurance et sante mentale dans PERSONNAGE (%d rendu(s), %d sans les deux jauges) ; aucune ailleurs (%d rendu(s) du jeu, "
				"du carnet, du journal, des parametres, des touches et de la pause, %d avec une jauge) ; aucune barre de vie (le HUD n'en dessine pas) ; commandes de la "
				"console sous la pause d'un joueur (mode developpeur coupe) : %d (attendu 0)"), GaugeCharDraws, GaugeCharBad, GaugeOtherDraws, GaugeOtherBad, ConsoleHints),
				GaugeCharBad > 0 || GaugeOtherBad > 0 || ConsoleHints > 0 || GaugeCharDraws == 0);
		}
		BRLoc::SetLanguage(TEXT("fr"));
		return true;
	});

	// ------------------------------------------------------------------------------------------------ 1.2 manette
	Add(TEXT("v4.16 visuel : manette, boutons des invites = liaisons reelles"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC || !PC->Mapping)
		{
			Skip(TEXT("manette : liaisons du controleur absentes"));
			return true;
		}
		TArray<FString> Bad;
		int32 Checked = 0;
		for (int32 A = 0; A < BRKeys::NumActions(); ++A)
		{
			const EBRAction Act = static_cast<EBRAction>(A);
			const FKey Pad = BRKeys::GamepadKey(Act);
			if (!Pad.IsValid())
			{
				continue;
			}
			// Les quatre directions du deplacement : une seule action (MoveAction) sur le stick gauche
			const bool bMove = Act == EBRAction::MoveForward || Act == EBRAction::MoveBackward || Act == EBRAction::MoveLeft || Act == EBRAction::MoveRight;
			const UInputAction* Want = bMove ? PC->MoveAction.Get() : PC->ActionFor(A);
			bool bFound = false;
			for (const FEnhancedActionKeyMapping& M : PC->Mapping->GetMappings())
			{
				bFound |= M.Key == Pad && M.Action == Want;
			}
			++Checked;
			if (!bFound)
			{
				Bad.Add(FString::Printf(TEXT("%s : %s"), *BRKeys::ActionLabel(Act), *BRKeys::GamepadName(Pad)));
			}
		}
		Note(FString::Printf(TEXT("manette : %d action(s) nommees a la manette, %d sans la liaison annoncee%s%s"), Checked, Bad.Num(), Bad.Num() ? TEXT(" : ") : TEXT(""),
			*FString::Join(Bad, TEXT(", "))), Bad.Num() > 0 || Checked < 10);
		return true;
	});
	Add(TEXT("v4.16 visuel : manette, invites"), 0.3f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return true;
		}
		PC->bPadActive = true; // la manette a servi en dernier (PlayerTick : invites)
		return true;
	});
	Add(TEXT("v4.16 visuel : manette, textes des invites"), 0.f, [this, HudOf]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = HudOf();
		if (!PC || !H)
		{
			return true;
		}
		const bool bPad = BRKeys::UsesGamepadPrompts();
		const FString Interact = BRKeys::Primary(EBRAction::Interact);
		const FString Inventory = BRKeys::Tag(EBRAction::Inventory);
		const FString Move = BRKeys::Expand(TEXT("{MoveForward}{MoveLeft}{MoveBackward}{MoveRight}"));
		// Echelle (Player.MoveforwardMonterMovebackwardDescendreJu) : chaque direction garde son bouton
		const FString Ladder = BRKeys::Expand(TEXT("{MoveForward} / {MoveBackward} / {Jump}"));
		const FString Stick = TEXT("[") + BRKeys::GamepadName(EKeys::Gamepad_Left2D) + TEXT("]");
		const FString Line0 = H->ControlsLine(0);
		const bool bOk = bPad && Interact == TEXT("X") && Inventory == TEXT("[") + BRKeys::GamepadName(EKeys::Gamepad_Special_Left) + TEXT("]")
			&& Move == Stick && Ladder == Stick + TEXT(" / ") + Stick + TEXT(" / [A]") && BRKeys::Primary(EBRAction::LidarFocus) == TEXT("RT")
			&& Line0.StartsWith(BRKeys::GamepadName(EKeys::Gamepad_Left2D) + TEXT(" "));
		Note(FString::Printf(TEXT("invites a la manette (manette active %s) : interagir \"%s\", inventaire \"%s\", deplacement \"%s\", echelle \"%s\", scan \"%s\" ; "
			"commandes de la pause : %s"), Oui(bPad), *Interact, *Inventory, *Move, *Ladder, *BRKeys::Primary(EBRAction::LidarFocus), *Short(Line0)), !bOk);
		return true;
	});
	for (const TCHAR* PadLang : { TEXT("fr"), TEXT("de") })
	{
		const FString Code = PadLang;
		Add(FString::Printf(TEXT("v4.16 visuel : manette, langue %s"), *Code), 0.3f, [this, Code]()
		{
			BRLoc::SetLanguage(Code);
			if (ABRPlayerController* PC = GetPC())
			{
				PC->bPadActive = true;
			}
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 visuel : manette, inventaire hors ecran (%s)"), *Code), 0.1f, [this, Code, ShowScreen, CloseScreens, Offscreen]()
		{
			if (ABRPlayerController* PC = GetPC())
			{
				PC->bPadActive = true;
			}
			BRKeys::SetGamepadPrompts(true); // pour les rendus de cette image (le controleur le reprend a chaque image)
			for (const int32 Tab : { 0, 4, -1 })
			{
				ShowScreen(Tab, false);
				for (const FSizeCase& Sz : OffSizes)
				{
					TArray<FDrawnText> Texts;
					const bool bPng = Code == TEXT("de") || Sz.W == 1920;
					if (!Offscreen(Sz.W, Sz.H, bPng ? FString::Printf(TEXT("v416_manette_%s_%s_%s"), Sz.Name, *Code, Tab == 0 ? TEXT("personnage") : (Tab == 4 ? TEXT("carnet") : TEXT("pause")))
						: FString(), Texts))
					{
						continue;
					}
					CheckTexts(Texts, static_cast<float>(Sz.W), static_cast<float>(Sz.H), FString::Printf(TEXT("manette %s %s onglet %d"), Sz.Name, *Code, Tab), PadReport);
					for (const FDrawnText& T : Texts)
					{
						if (T.Text.Contains(TEXT("[LB]")))
						{
							PadFooterSeen = T.Text;
						}
					}
				}
			}
			CloseScreens();
			return true;
		});
	}
	Add(TEXT("v4.16 visuel : manette, bilan hors ecran"), 0.f, [this]()
	{
		Note(FString::Printf(TEXT("manette, inventaire et pause hors ecran : %s ; pied de l'inventaire : \"%s\""), *ReportLine(PadReport), *PadFooterSeen),
			PadReport.Problems() > 0 || PadFooterSeen.IsEmpty());
		for (const FString& E : PadReport.Examples)
		{
			Note(TEXT("  ") + E, true);
		}
		BRLoc::SetLanguage(TEXT("fr"));
		return true;
	});
	// Inventaire a la manette par de vraies touches (injectees dans le controleur, comme une manette branchee)
	struct FPadPress
	{
		FKey Key;
		const TCHAR* What;
		int32 WantTab;      // onglet attendu apres l'appui (-1 : pas de controle d'onglet)
		bool bWantClosed;   // inventaire ferme apres l'appui
		bool bWantHint;     // aide du carnet augmentee
	};
	const TArray<FPadPress> Presses = {
		{ EKeys::Gamepad_RightShoulder, TEXT("RB : onglet suivant (CARNET)"), 4, false, false },
		{ EKeys::Gamepad_DPad_Right, TEXT("croix droite : onglet suivant (JOURNAL)"), 1, false, false },
		{ EKeys::Gamepad_LeftShoulder, TEXT("LB : onglet precedent (CARNET)"), 4, false, false },
		{ EKeys::Gamepad_DPad_Down, TEXT("croix bas : defilement du carnet"), 4, false, false },
		{ EKeys::Gamepad_FaceButton_Top, TEXT("Y : aide du carnet"), 4, false, true },
		{ EKeys::Gamepad_DPad_Left, TEXT("croix gauche : onglet precedent (PERSONNAGE)"), 0, false, false },
		{ EKeys::Gamepad_FaceButton_Right, TEXT("B : fermer"), -1, true, false },
	};
	Add(TEXT("v4.16 visuel : manette, inventaire ouvert sur PERSONNAGE"), 0.4f, [this, HudOf, ShowScreen]()
	{
		ABRHUD* H = HudOf();
		ShowScreen(0, false);
		if (H)
		{
			H->NotebookHint.Reset();
			H->NotebookLeftScroll = 0.f;
		}
		if (ABRPlayerController* PC = GetPC())
		{
			PC->bPadActive = true;
		}
		return true;
	});
	for (const FPadPress& Press : Presses)
	{
		Add(FString::Printf(TEXT("v4.16 visuel : manette, %s"), Press.What), 0.f, [this, HudOf, Press, ShotNamed]()
		{
			ABRPlayerController* PC = GetPC();
			ABRHUD* H = HudOf();
			if (!PC || !H)
			{
				return true;
			}
			if (BRTestV416Vis::Phase == 0)
			{
				BRTestV416Vis::Phase = 1;
				BRTestV416Vis::Index = H->LastNotebookHintLevel;
				BRTestV416Vis::PhaseClock = H->NotebookLeftScroll;
				PC->InputKey(FInputKeyEventArgs::CreateSimulated(Press.Key, IE_Pressed, 1.f));
				return false;
			}
			if (BRTestV416Vis::Phase == 1)
			{
				// Une image pour l'evenement, une pour le dessin
				if (StepTime < 0.15f)
				{
					return false;
				}
				BRTestV416Vis::Phase = 2;
				const bool bDown = PC->IsInputKeyDown(Press.Key);
				const int32 TabNow = static_cast<int32>(H->Tab);
				const bool bTab = Press.WantTab < 0 || TabNow == Press.WantTab;
				const bool bClosed = !Press.bWantClosed || !PC->IsInventoryOpen();
				const bool bHint = !Press.bWantHint || H->LastNotebookHintLevel > BRTestV416Vis::Index;
				const bool bOk = bTab && bClosed && bHint;
				if (!bOk && !bDown)
				{
					Skip(FString::Printf(TEXT("manette, %s : la touche injectee n'est pas enfoncee pour le controleur (injection sans effet ici : a verifier avec une vraie manette)"),
						Press.What));
				}
				else
				{
					Note(FString::Printf(TEXT("manette, %s : touche enfoncee %s, onglet %d, inventaire %s, aide du carnet %d -> %d, defilement du carnet %.0f -> %.0f"), Press.What,
						BRTestV416Vis::Oui(bDown), TabNow, PC->IsInventoryOpen() ? TEXT("ouvert") : TEXT("ferme"), BRTestV416Vis::Index, H->LastNotebookHintLevel,
						BRTestV416Vis::PhaseClock, H->NotebookLeftScroll), !bOk);
				}
				if (Press.WantTab == 4 && Press.bWantHint)
				{
					ShotNamed(TEXT("v416_manette_carnet_aide"));
				}
				PC->InputKey(FInputKeyEventArgs::CreateSimulated(Press.Key, IE_Released, 0.f));
				return false;
			}
			if (StepTime < 0.3f)
			{
				return false;
			}
			BRTestV416Vis::Phase = 0;
			return true;
		});
	}
	Add(TEXT("v4.16 visuel : manette, pause et HUD en jeu"), 0.6f, [this, HudOf, ShowScreen]()
	{
		if (ABRPlayerController* PC = GetPC())
		{
			PC->bPadActive = true;
		}
		ShowScreen(-1, false);
		return true;
	});
	Add(TEXT("v4.16 visuel : manette, capture de la pause"), 0.2f, [this, ShotNamed]()
	{
		ABRPlayerController* PC = GetPC();
		if (PC)
		{
			PC->bPadActive = true;
		}
		Note(FString::Printf(TEXT("pause a la manette (fenetre) : invites de la manette %s"), Oui(BRKeys::UsesGamepadPrompts())), !BRKeys::UsesGamepadPrompts());
		ShotNamed(TEXT("v416_manette_pause_fenetre"));
		return true;
	});
	Add(TEXT("v4.16 visuel : clavier, capture de la pause"), 0.4f, [this]()
	{
		if (ABRPlayerController* PC = GetPC())
		{
			PC->bPadActive = false;
		}
		return true;
	});
	Add(TEXT("v4.16 visuel : clavier, pause"), 0.2f, [this, ShotNamed, CloseScreens]()
	{
		ShotNamed(TEXT("v416_clavier_pause_fenetre"));
		CloseScreens();
		return true;
	});

	// ------------------------------------------------------------------------------------------------ 1.3 vraies fenetres
	struct FWindowCase
	{
		const TCHAR* Name;
		int32 Kind; // 0 : 1280 x 720 ; 1 : plus grande 16:9 ; 2 : plus grande 64:27
	};
	const TArray<FWindowCase> WindowCases = { { TEXT("petite_1280x720"), 0 }, { TEXT("169_max"), 1 }, { TEXT("ultralarge_max"), 2 } };
	for (const FWindowCase& Wc : WindowCases)
	{
		Add(FString::Printf(TEXT("v4.16 visuel : fenetre %s"), Wc.Name), 2.5f, [this, Wc]()
		{
			if (!BRDisplay::CanChange())
			{
				Skip(FString::Printf(TEXT("fenetre %s : affichage non modifiable (editeur)"), Wc.Name));
				return true;
			}
			const BRDisplay::FMonitor M = BRDisplay::ActiveMonitor();
			const int32 MaxW = FMath::Max(640, M.WorkSize.X - 16);
			const int32 MaxH = FMath::Max(360, M.WorkSize.Y - 40);
			FIntPoint Want(1280, 720);
			if (Wc.Kind == 1)
			{
				const int32 W = FMath::Min(MaxW, MaxH * 16 / 9) & ~1;
				Want = FIntPoint(W, W * 9 / 16);
			}
			else if (Wc.Kind == 2)
			{
				const int32 W = FMath::Min(MaxW, FMath::Min(2560, MaxH * 64 / 27)) & ~1;
				Want = FIntPoint(W, W * 27 / 64);
			}
			const bool bOk = BRDisplay::Request(2, Want);
			BRDisplay::Confirm();
			BRTestV416Vis::bDisplayTouched = true;
			Note(FString::Printf(TEXT("fenetre %s : %d x %d demandee (zone utile %d x %d) : acceptee %s"), Wc.Name, Want.X, Want.Y, M.WorkSize.X, M.WorkSize.Y, Oui(bOk)), !bOk);
			return true;
		});
		for (const int32 Tab : { 0, 4 })
		{
			Add(FString::Printf(TEXT("v4.16 visuel : fenetre %s, onglet %d"), Wc.Name, Tab), 0.8f, [this, Tab, HudOf, ShowScreen]()
			{
				ShowScreen(Tab);
				if (ABRHUD* H = HudOf())
				{
					H->bTestRecordTexts = true;
				}
				return true;
			});
			Add(FString::Printf(TEXT("v4.16 visuel : fenetre %s, capture %d"), Wc.Name, Tab), 0.2f, [this, Wc, Tab, HudOf, Grab, ShotNamed, CloseScreens]()
			{
				ABRHUD* H = HudOf();
				const BRDisplay::FMode Eff = BRDisplay::Effective();
				FIntPoint View(0, 0);
				if (ABRPlayerController* PC = GetPC())
				{
					PC->GetViewportSize(View.X, View.Y);
				}
				if (H)
				{
					TArray<FDrawnText> Texts;
					Grab(H, Texts);
					H->bTestRecordTexts = false;
					CheckTexts(Texts, static_cast<float>(View.X), static_cast<float>(View.Y), FString::Printf(TEXT("fenetre %s (%d x %d) onglet %d"), Wc.Name, View.X, View.Y, Tab),
						RealReport);
				}
				Note(FString::Printf(TEXT("fenetre %s, onglet %d : fenetre obtenue %d x %d (mode %d), image %d x %d"), Wc.Name, Tab, Eff.Resolution.X, Eff.Resolution.Y, Eff.Window,
					View.X, View.Y));
				ShotNamed(FString::Printf(TEXT("v416_fenetre_%s_%s"), Wc.Name, Tab == 0 ? TEXT("personnage") : TEXT("carnet")));
				if (Tab == 4)
				{
					CloseScreens();
				}
				return true;
			});
		}
	}
	Add(TEXT("v4.16 visuel : vraies fenetres, bilan"), 0.f, [this]()
	{
		Note(FString::Printf(TEXT("vraies fenetres : %s"), *ReportLine(RealReport)), RealReport.Problems() > 0);
		for (const FString& E : RealReport.Examples)
		{
			Note(TEXT("  ") + E, true);
		}
		if (RealReport.Draws == 0 && BRDisplay::CanChange())
		{
			Skip(TEXT("vraies fenetres : aucun texte releve"));
		}
		return true;
	});

	// ------------------------------------------------------------------------------------------------ 1.4 modes d'affichage
	Add(TEXT("v4.16 visuel : modes d'affichage presents"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return true;
		}
		const int32 Row = ABRPlayerController::FindSettingRow(TEXT("WindowMode"));
		const FString Names[] = { BR_STR(NSLOCTEXT("BR", "Menu.PleinEcran", "PLEIN \u00c9CRAN")), BR_STR(NSLOCTEXT("BR", "Menu.FenetreSansBordure", "FEN\u00caTR\u00c9 SANS BORDURE")),
			BR_STR(NSLOCTEXT("BR", "Menu.Fenetre", "FEN\u00caTR\u00c9")) };
		const bool bDistinct = !Names[0].IsEmpty() && !Names[1].IsEmpty() && !Names[2].IsEmpty() && Names[0] != Names[1] && Names[1] != Names[2] && Names[0] != Names[2];
		const bool bRow = Row != INDEX_NONE && PC->GetSettingCategory(Row) == 1 && !PC->GetSettingLabel(Row).IsEmpty() && !PC->GetSettingValue(Row).IsEmpty();
		const int32 Full = BRDisplay::ResolutionsFor(0).Num();
		const int32 Borderless = BRDisplay::ResolutionsFor(1).Num();
		const int32 Windowed = BRDisplay::ResolutionsFor(2).Num();
		Note(FString::Printf(TEXT("PARAMETRES > VIDEO : ligne \"%s\" (valeur \"%s\") %s ; trois modes : %s / %s / %s ; definitions proposees : plein ecran %d, sans bordures %d, fenetre %d ; "
			"plein ecran exclusif sur cette plateforme %s"), Row != INDEX_NONE ? *PC->GetSettingLabel(Row) : TEXT("ABSENTE"), Row != INDEX_NONE ? *PC->GetSettingValue(Row) : TEXT(""),
			Oui(bRow), *Names[0], *Names[1], *Names[2], Full, Borderless, Windowed, Oui(BRDisplay::PlatformHasExclusiveFullscreen())),
			!bRow || !bDistinct || Full == 0 || Borderless == 0 || Windowed == 0);
		return true;
	});
	for (int32 Mode = 1; Mode >= 0; --Mode)
	{
		Add(FString::Printf(TEXT("v4.16 visuel : essai du mode %d"), Mode), 2.5f, [this, Mode, ShowScreen]()
		{
			if (!BRDisplay::CanChange())
			{
				Skip(TEXT("modes d'affichage : affichage non modifiable (editeur)"));
				return true;
			}
			if (Mode == 0 && !FParse::Param(FCommandLine::Get(), TEXT("BRV416PleinEcran")))
			{
				Note(TEXT("plein ecran exclusif : present (ligne et definitions ci-dessus), non essaye sur cette machine (option -BRV416PleinEcran : change la definition de l'ecran)"));
				return true;
			}
			ShowScreen(2);
			const bool bOk = BRDisplay::Request(Mode, FIntPoint::ZeroValue);
			BRTestV416Vis::bDisplayTouched = true;
			Note(FString::Printf(TEXT("essai du mode %d : demande acceptee %s, en attente de confirmation %s"), Mode, Oui(bOk), Oui(BRDisplay::IsPending())), !bOk);
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 visuel : mode %d obtenu"), Mode), 0.f, [this, Mode, ShotNamed, CloseScreens]()
		{
			if (!BRDisplay::IsPending())
			{
				return true;
			}
			const BRDisplay::FMode Cur = BRDisplay::Current();
			const BRDisplay::FMode Eff = BRDisplay::Effective();
			FString Value;
			if (ABRPlayerController* PC = GetPC())
			{
				const int32 Row = ABRPlayerController::FindSettingRow(TEXT("WindowMode"));
				Value = Row != INDEX_NONE ? PC->GetSettingValue(Row) : FString();
			}
			Note(FString::Printf(TEXT("mode %d : demande %d en %d x %d, obtenu %d en %d x %d ; valeur affichee \"%s\""), Mode, Cur.Window, Cur.Resolution.X, Cur.Resolution.Y,
				Eff.Window, Eff.Resolution.X, Eff.Resolution.Y, *Value), Cur.Window != Mode);
			ShotNamed(FString::Printf(TEXT("v416_mode_affichage_%d"), Mode));
			BRDisplay::Revert();
			CloseScreens();
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 visuel : retour apres le mode %d"), Mode), 2.f, []()
		{
			return true;
		});
	}
	// Fenetre d'origine pour la suite (mesures des decalques, scanner, entites et kit a la taille habituelle des bancs)
	Add(TEXT("v4.16 visuel : fenetre d'origine"), 2.5f, []()
	{
		if (BRTestV416Vis::bDisplayTouched && BRDisplay::CanChange())
		{
			if (BRDisplay::IsPending())
			{
				BRDisplay::Revert();
			}
			BRDisplay::Request(BRTestV416Vis::SavedMode.Window, BRTestV416Vis::SavedMode.Resolution);
			BRDisplay::Confirm();
		}
		return true;
	});
	Add(TEXT("v4.16 visuel : fenetre d'origine obtenue"), 0.f, [this]()
	{
		FIntPoint View(0, 0);
		if (ABRPlayerController* PC = GetPC())
		{
			PC->GetViewportSize(View.X, View.Y);
		}
		const BRDisplay::FMode Eff = BRDisplay::Effective();
		Note(FString::Printf(TEXT("fenetre d'origine retablie : mode %d, %d x %d (image %d x %d)"), Eff.Window, Eff.Resolution.X, Eff.Resolution.Y, View.X, View.Y));
		return true;
	});

	// ------------------------------------------------------------------------------------------------ 1.5 decalques du Level 1
	auto DecalSteps = [this, Section](int32 Level, const FString& Tag)
	{
		Add(FString::Printf(TEXT("v4.16 decalques : %s, creation finie"), *Tag), 0.f, [this, Level, Tag, Section]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W || W->GetLevelNumber() != Level || !W->IsLevelReady())
			{
				return StepTime > 30.f;
			}
			// Les composants sont crees un par un dans le budget de l'image : attendre que chaque chunk ait les siens
			int32 Missing = 0;
			for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
			{
				Missing += It->IsReady() ? 0 : 1;
				TArray<UDecalComponent*> Decals;
				It->GetComponents<UDecalComponent>(Decals);
				int32 Patches = 0;
				TArray<UInstancedStaticMeshComponent*> Isms;
				It->GetComponents<UInstancedStaticMeshComponent>(Isms);
				UBRAssets* A = UBRAssets::Get(this);
				for (const UInstancedStaticMeshComponent* I : Isms)
				{
					if (A && I && I->GetMaterial(0) == A->DecalMaterial(EBRDecal::Damp))
					{
						Patches += I->GetInstanceCount();
					}
				}
				Missing += FMath::Max(0, It->GetDecalCount() - Decals.Num() - Patches);
			}
			if (Missing > 0 && StepTime < 20.f)
			{
				return false;
			}
			Section(FString::Printf(TEXT("v4.16 decalques : %s"), *Tag), Level);
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 decalques : %s, mesures"), *Tag), 0.f, [this, Level, Tag]()
		{
			ABRWorld* W = GetBRWorld();
			UBRAssets* A = UBRAssets::Get(this);
			ABRCharacter* C = GetPlayer();
			if (!W || !A || !C || W->GetLevelNumber() != Level)
			{
				Skip(FString::Printf(TEXT("decalques %s : monde absent"), *Tag));
				return true;
			}
			const FBRLevelDef& Def = W->Def();
			if (Def.DecalDensity <= 0.f || !A->DecalMaterial(EBRDecal::WaterStain))
			{
				Skip(FString::Printf(TEXT("decalques %s : niveau sans decalques ou materiau absent"), *Tag));
				return true;
			}
			FDecalStats St;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(BRV416Decal), false, C);
			FCollisionResponseParams Resp;
			Resp.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
			const float S = Def.CellSize;
			const float T = Def.WallThickness;
			const float H = Def.WallHeight;
			// Vue de reference de la mesure : largeur interne et champ de la camera du joueur
			int32 VX = 1920, VY = 1080;
			GetPC()->GetViewportSize(VX, VY);
			const float Hfov = GetPC()->PlayerCameraManager ? GetPC()->PlayerCameraManager->GetFOVAngle() : 88.f;
			const float RenderW = static_cast<float>(VX) * FMath::Clamp(FBRSettings::Get().RenderScale / 100.f, 0.3f, 1.f);
			const FVector Eye = C->GetEyeLocation();
			// Occurrences par face de mur (cellule, cote) pour les voisinages
			TMap<FIntVector, int32> FaceKind;
			for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
			{
				ABRChunk* Ch = *It;
				if (!Ch->IsReady())
				{
					continue; // encore en construction (decalques crees un par un)
				}
				TArray<UDecalComponent*> Decals;
				Ch->GetComponents<UDecalComponent>(Decals);
				int32 Patches = 0;
				TArray<UInstancedStaticMeshComponent*> Isms;
				Ch->GetComponents<UInstancedStaticMeshComponent>(Isms);
				++St.Chunks;
				St.Expected += Ch->GetDecalCount();
				St.MaxPerChunk = FMath::Max(St.MaxPerChunk, Decals.Num());
				St.MinPerChunk = FMath::Min(St.MinPerChunk, Decals.Num());
				for (UDecalComponent* Dc : Decals)
				{
					int32 Kind = -1;
					for (int32 K = 0; K < 4; ++K)
					{
						Kind = Dc->GetDecalMaterial() == A->DecalMaterial(static_cast<EBRDecal>(K)) ? K : Kind;
					}
					if (Kind < 0)
					{
						continue;
					}
					++St.Projected;
					++St.KindCount[Kind];
					const FTransform Tr = Dc->GetComponentTransform();
					const FVector P = Tr.GetLocation();
					const FVector XAx = Tr.GetUnitAxis(EAxis::X);
					const FVector YAx = Tr.GetUnitAxis(EAxis::Y);
					const FVector ZAx = Tr.GetUnitAxis(EAxis::Z);
					const FVector Half = Dc->DecalSize;
					const float MaxHalf = static_cast<float>(FMath::Max3(Half.X, Half.Y, Half.Z));
					const float FadeAt = BRDecal::FadeOutDistance(MaxHalf, Dc->FadeScreenSize, RenderW, Hfov);
					St.FadeMin = FMath::Min(St.FadeMin, FadeAt);
					St.FadeMax = FMath::Max(St.FadeMax, FadeAt);
					St.WithinUseful += FVector::Dist(Eye, P) <= FadeAt ? 1 : 0;
					if (Kind == static_cast<int32>(EBRDecal::Scuff))
					{
						++St.Scuffs;
						St.Flipped += YAx.Z > 0.f ? 1 : 0; // image retournee : le bas de l'image vers le haut
					}
					// Signature (sorte, decalage dans la cellule a 10 cm, tailles a 5 cm) : repetition d'une cellule a l'autre
					const FIntPoint Cell = W->WorldToCell(P - XAx * 40.f);
					const FVector Local = P - W->CellCenter(Cell);
					const int32 Sig = Kind * 100000000 + FMath::FloorToInt32((Local.X + 400.f) / 10.f) * 1000000 + FMath::FloorToInt32((Local.Y + 400.f) / 10.f) * 10000
						+ FMath::FloorToInt32(Half.Y / 5.f) * 100 + FMath::FloorToInt32(Half.Z / 5.f);
					++St.Signatures.FindOrAdd(Sig);
					if (Kind != static_cast<int32>(EBRDecal::CeilingStain))
					{
						const int32 Side = FMath::Abs(XAx.X) > FMath::Abs(XAx.Y) ? (XAx.X > 0.f ? 0 : 1) : (XAx.Y > 0.f ? 2 : 3);
						FaceKind.Add(FIntVector(Cell.X, Cell.Y, Side), Kind);
					}
					// Surface sous le decalque : 5 x 3 points de l'image, trace le long de l'axe de projection
					int32 On = 0, Total = 0;
					for (int32 a = -2; a <= 2; ++a)
					{
						for (int32 b = -1; b <= 1; ++b)
						{
							const FVector At = P + ZAx * (Half.Z * 0.8f * a / 2.f) + YAx * (Half.Y * 0.8f * b);
							const FVector From = At - XAx * (Half.X + 40.f);
							const FVector To = At + XAx * (Half.X + 40.f);
							FHitResult Hit;
							++Total;
							++St.Samples;
							if (!GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q, Resp))
							{
								++St.Void;
								continue;
							}
							const float D = static_cast<float>(FVector::DotProduct(Hit.ImpactPoint - P, XAx));
							if (D < -Half.X)
							{
								++St.Blocked;
								continue;
							}
							if (D > Half.X)
							{
								++St.Void;
								continue;
							}
							++On;
							++St.OnSurface;
							if (FVector::DotProduct(Hit.ImpactNormal, -XAx) < 0.9)
							{
								++St.Oblique;
							}
							if (Hit.GetActor() && !Hit.GetActor()->IsA<ABRChunk>())
							{
								++St.OnObject;
								++St.ObjectHits.FindOrAdd(Hit.GetActor()->GetClass()->GetName());
							}
						}
					}
					if (On == 0)
					{
						++St.DecalsFloating;
						St.Example(FString::Printf(TEXT("sorte %d sans surface en (%.0f, %.0f, %.0f)"), Kind, P.X, P.Y, P.Z));
					}
					else if (On < Total)
					{
						++St.DecalsPartial;
					}
					// Ouverture : un point d'une face de mur qui tombe dans un passage (grille)
					if (Kind != static_cast<int32>(EBRDecal::CeilingStain))
					{
						for (int32 a = -2; a <= 2; ++a)
						{
							const FVector At = P + ZAx * (Half.Z * 0.95f * a / 2.f);
							const FIntPoint In = W->WorldToCell(At - XAx * 30.f);
							const FVector Ctr = W->CellCenter(In);
							const bool bEastWest = FMath::Abs(XAx.X) > FMath::Abs(XAx.Y);
							const float Along = static_cast<float>(bEastWest ? At.Y - Ctr.Y : At.X - Ctr.X);
							const EBREdge E = bEastWest ? W->EdgeE(XAx.X > 0.f ? In.X : In.X - 1, In.Y) : W->EdgeN(In.X, XAx.Y > 0.f ? In.Y : In.Y - 1);
							const float DW = FMath::Min(Def.DoorWidth, S - 2.f * T - 20.f);
							if (E == EBREdge::Door && FMath::Abs(Along) < DW * 0.5f)
							{
								++St.OverOpening;
								St.Example(FString::Printf(TEXT("sorte %d au-dessus d'un passage en (%.0f, %.0f, %.0f)"), Kind, At.X, At.Y, At.Z));
								break;
							}
						}
					}
				}
				// Taches au sol (instances) : posees a 0,6 cm du sol praticable, cachees sous les murs
				for (const UInstancedStaticMeshComponent* I : Isms)
				{
					if (!I || I->GetMaterial(0) != A->DecalMaterial(EBRDecal::Damp))
					{
						continue;
					}
					Patches += I->GetInstanceCount();
					for (int32 n = 0; n < I->GetInstanceCount(); ++n)
					{
						FTransform Tr;
						I->GetInstanceTransform(n, Tr, true);
						const FVector Sc = Tr.GetScale3D() * 50.f;
						for (int32 a = -1; a <= 1; ++a)
						{
							for (int32 b = -1; b <= 1; ++b)
							{
								const FVector At = Tr.TransformPositionNoScale(FVector(Sc.X * 0.9f * a, Sc.Y * 0.9f * b, 0.f));
								const FIntPoint Cell = W->WorldToCell(At);
								const FVector Ctr = W->CellCenter(Cell);
								const float LX = static_cast<float>(At.X - Ctr.X), LY = static_cast<float>(At.Y - Ctr.Y);
								const float Edge = S * 0.5f - T * 0.5f;
								// Sous un mur (cellule pleine, ou dans l'epaisseur d'une arete mur / d'un jambage) : cache, normal
								const bool bSolid = !W->IsWalkable(Cell);
								const bool bInWall = (LX > Edge && W->EdgeE(Cell.X, Cell.Y) != EBREdge::Open) || (LX < -Edge && W->EdgeE(Cell.X - 1, Cell.Y) != EBREdge::Open)
									|| (LY > Edge && W->EdgeN(Cell.X, Cell.Y) != EBREdge::Open) || (LY < -Edge && W->EdgeN(Cell.X, Cell.Y - 1) != EBREdge::Open);
								++St.PatchSamples;
								if (bSolid || bInWall)
								{
									++St.PatchHidden;
									continue;
								}
								FHitResult Hit;
								if (!GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 40.f), At - FVector(0.f, 0.f, 60.f), ECC_Visibility, Q, Resp))
								{
									++St.PatchFloating;
									St.Example(FString::Printf(TEXT("tache au sol au-dessus du vide en (%.0f, %.0f)"), At.X, At.Y));
									continue;
								}
								const float Gap = static_cast<float>(At.Z - Hit.ImpactPoint.Z);
								St.PatchMaxGap = FMath::Max(St.PatchMaxGap, FMath::Abs(Gap));
								if (Gap > 2.f)
								{
									++St.PatchFloating;
									St.Example(FString::Printf(TEXT("tache au sol a %.1f cm du sol en (%.0f, %.0f)"), Gap, At.X, At.Y));
								}
								else if (Gap < 0.1f)
								{
									++St.PatchBuried;
								}
							}
						}
					}
				}
				St.Patches += Patches;
				St.MaxPatchesPerChunk = FMath::Max(St.MaxPatchesPerChunk, Patches);
			}
			if (St.Chunks == 0 || St.Projected + St.Patches == 0)
			{
				Skip(FString::Printf(TEXT("decalques %s : aucun decalque dans les chunks construits"), *Tag));
				return true;
			}
			// Voisinage : deux faces voisines d'une meme ligne de mur portant la meme sorte, rapporte au hasard
			int32 Pairs = 0, Same = 0;
			for (const TPair<FIntVector, int32>& KV : FaceKind)
			{
				const FIntVector N = KV.Key.Z >= 2 ? FIntVector(KV.Key.X + 1, KV.Key.Y, KV.Key.Z) : FIntVector(KV.Key.X, KV.Key.Y + 1, KV.Key.Z);
				++Pairs;
				if (const int32* Other = FaceKind.Find(N))
				{
					Same += *Other == KV.Value ? 1 : 0;
				}
			}
			int32 TopSig = 0;
			for (const TPair<int32, int32>& KV : St.Signatures)
			{
				TopSig = FMath::Max(TopSig, KV.Value);
			}
			const int32 Walls = St.KindCount[0] + St.KindCount[3];
			const float Rate = Pairs > 0 ? static_cast<float>(Walls) / FMath::Max(1, St.Chunks * Def.ChunkCells * Def.ChunkCells * 4) : 0.f;
			FString Objects;
			for (const TPair<FString, int32>& KV : St.ObjectHits)
			{
				Objects += FString::Printf(TEXT("%s%s x%d"), Objects.IsEmpty() ? TEXT("") : TEXT(", "), *KV.Key, KV.Value);
			}
			const int32 Created = St.Projected + St.Patches;
			Note(FString::Printf(TEXT("%s : %d chunk(s), %d decalque(s) crees sur %d tires (aureoles %d, taches de plafond %d, eraflures %d ; taches au sol %d) ; par chunk %d a %d projetes "
				"(borne %d), %d taches au sol au plus (borne %d)"), *Tag, St.Chunks, Created, St.Expected, St.KindCount[0], St.KindCount[1], St.KindCount[3], St.Patches,
				St.MinPerChunk == MAX_int32 ? 0 : St.MinPerChunk, St.MaxPerChunk, BRDecal::MaxProjectedPerChunk, St.MaxPatchesPerChunk, BRDecal::MaxFloorPatchesPerChunk),
				Created != St.Expected || St.MaxPerChunk > BRDecal::MaxProjectedPerChunk || St.MaxPatchesPerChunk > BRDecal::MaxFloorPatchesPerChunk);
			Note(FString::Printf(TEXT("%s : surface sous les decalques projetes : %d point(s) sur %d sur la surface, %d dans le vide, %d derriere un obstacle, %d sur un objet (%s), %d obliques ; "
				"decalques sans aucune surface %d, partiellement dans le vide %d, au-dessus d'un passage %d"), *Tag, St.OnSurface, St.Samples, St.Void, St.Blocked, St.OnObject,
				Objects.IsEmpty() ? TEXT("aucun") : *Objects, St.Oblique, St.DecalsFloating, St.DecalsPartial, St.OverOpening), St.DecalsFloating > 0 || St.OverOpening > 0);
			Note(FString::Printf(TEXT("%s : taches au sol : %d point(s), %d caches sous un mur, %d flottant(s) (> 2 cm), %d enfoui(s) (< 1 mm), ecart au sol le plus grand %.1f cm"), *Tag,
				St.PatchSamples, St.PatchHidden, St.PatchFloating, St.PatchBuried, St.PatchMaxGap), St.PatchFloating > 0);
			Note(FString::Printf(TEXT("%s : motifs : %.2f %% des faces de cellule portent une aureole ou une eraflure ; meme sorte sur deux faces voisines %d / %d ; %d signatures pour %d decalques "
				"projetes, la plus frequente %d fois ; eraflures retournees %d / %d"), *Tag, Rate * 100.f, Same, Pairs, St.Signatures.Num(), St.Projected, TopSig, St.Flipped,
				St.Scuffs), St.Projected >= 20 && TopSig * 10 > St.Projected);
			Note(FString::Printf(TEXT("%s : distance utile (image de %.0f px de large, champ %.0f degres) : de %.1f a %.1f m selon la taille ; %d decalque(s) projete(s) sur %d a portee du joueur"),
				*Tag, RenderW, Hfov, St.FadeMin / 100.f, St.FadeMax / 100.f, St.WithinUseful, St.Projected),
				St.FadeMax > BRDecal::UsefulDistance * BRDecal::EngineFovFactor(BRDecal::ReferenceWidth, BRDecal::ReferenceHFovDeg) / BRDecal::EngineFovFactor(RenderW, Hfov) + 50.f);
			for (const FString& E : St.Examples)
			{
				Note(TEXT("  ") + E);
			}
			return true;
		});
	};
	DecalSteps(1, TEXT("Level 1, graine 4242"));

	// ================================================================================================ 2. Level 6 : scanner
	AddSeedLoad(6, LidarSeed, TEXT("v4.16 visuel : Level 6 (HUD contextuel du scanner)"));
	Add(TEXT("v4.16 scanner : rappel des touches a l'arrivee (clavier)"), 0.f, [this, HudOf, Section, Grab, ShotNamed]()
	{
		ABRCharacter* C = GetPlayer();
		ABRHUD* H = HudOf();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		if (!L || !H)
		{
			if (StepTime < 6.f)
			{
				return false;
			}
			Section(TEXT("v4.16 scanner : HUD contextuel"), 6);
			Skip(TEXT("scanner ou HUD absent : HUD contextuel du scanner non verifie"));
			BRTestV416Vis::Phase = -1;
			return true;
		}
		if (BRTestV416Vis::Phase == 0)
		{
			Section(TEXT("v4.16 scanner : HUD contextuel"), 6);
			H->Messages.Reset();
			H->bTestRecordTexts = true;
			BRTestV416Vis::Phase = 1;
			return false;
		}
		TArray<FDrawnText> Texts;
		Grab(H, Texts);
		H->bTestRecordTexts = false;
		FIntPoint V(0, 0);
		GetPC()->GetViewportSize(V.X, V.Y);
		CheckTexts(Texts, static_cast<float>(V.X), static_cast<float>(V.Y), TEXT("scanner, rappel clavier"), LidarReport);
		FString Keys;
		for (const FDrawnText& T : Texts)
		{
			Keys += T.Text + TEXT(" | ");
		}
		Note(FString::Printf(TEXT("arrivee : rappel des touches %.2f, recharge %.2f, cone %.2f ; textes : %s"), H->LastLidarHintAlpha, H->LastLidarChargeAlpha, H->LastLidarConeAlpha,
			*Keys.Left(300)), H->LastLidarHintAlpha <= 0.f);
		ShotNamed(TEXT("v416_lidar_rappel_clavier"));
		BRTestV416Vis::Phase = 0;
		return true;
	});
	Add(TEXT("v4.16 scanner : rappel des touches a la manette"), 0.f, [this, HudOf, Grab, ShotNamed]()
	{
		ABRHUD* H = HudOf();
		if (!H || BRTestV416Vis::Phase < 0)
		{
			return true;
		}
		GetPC()->bPadActive = true; // a chaque image (un mouvement de souris rend la main au clavier)
		if (BRTestV416Vis::Phase == 0)
		{
			H->bTestRecordTexts = true;
			BRTestV416Vis::Phase = 1;
			return false;
		}
		if (StepTime < 0.3f)
		{
			return false;
		}
		TArray<FDrawnText> Texts;
		Grab(H, Texts);
		H->bTestRecordTexts = false;
		bool bLT = false, bRT = false;
		for (const FDrawnText& T : Texts)
		{
			bLT |= T.Text == TEXT("LT");
			bRT |= T.Text == TEXT("RT");
		}
		Note(FString::Printf(TEXT("rappel du scanner a la manette : LT %s, RT %s (rappel %.2f)"), Oui(bLT), Oui(bRT), H->LastLidarHintAlpha), H->LastLidarHintAlpha > 0.f && !(bLT && bRT));
		ShotNamed(TEXT("v416_lidar_rappel_manette"));
		BRTestV416Vis::Phase = 0;
		return true;
	});
	Add(TEXT("v4.16 scanner : recharge"), 0.f, [this, HudOf, ShotNamed]()
	{
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		ABRHUD* H = HudOf();
		if (!L || !H || BRTestV416Vis::Phase < 0)
		{
			return true;
		}
		if (BRTestV416Vis::Phase == 0)
		{
			GetPC()->bPadActive = false; // de nouveau au clavier
			L->Pulse();
			BRTestV416Vis::Phase = 1;
			return false;
		}
		if (StepTime < 0.3f)
		{
			return false;
		}
		Note(FString::Printf(TEXT("recharge apres une impulsion : barre %.2f (recharge restante %.2f)"), H->LastLidarChargeAlpha, L->GetPulseCharge()), H->LastLidarChargeAlpha <= 0.f);
		ShotNamed(TEXT("v416_lidar_recharge"));
		BRTestV416Vis::Phase = 0;
		return true;
	});
	Add(TEXT("v4.16 scanner : cone du focus"), 0.f, [this, HudOf, ShotNamed]()
	{
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		ABRHUD* H = HudOf();
		if (!L || !H || BRTestV416Vis::Phase < 0)
		{
			return true;
		}
		L->SetFocus(true);
		if (StepTime < 0.5f)
		{
			return false;
		}
		Note(FString::Printf(TEXT("scan focalise : cone %.2f"), H->LastLidarConeAlpha), H->LastLidarConeAlpha <= 0.f);
		ShotNamed(TEXT("v416_lidar_cone"));
		L->SetFocus(false);
		return true;
	});
	Add(TEXT("v4.16 scanner : releve de la mire"), 0.f, [this, HudOf, ShotNamed]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		ABRHUD* H = HudOf();
		if (!W || !L || !H || BRTestV416Vis::Phase < 0)
		{
			return true;
		}
		ABRMissionDevice* Target = nullptr;
		const BRMission::FPlan& P = W->GetMissionPlan();
		for (int32 I = 0; I < P.NumDevices; ++I)
		{
			Target = (!Target && P.Devices[I].Role == BRMission::R_LidarTarget) ? W->FindMissionDevice(I) : Target;
		}
		if (!Target)
		{
			Skip(TEXT("scanner : mire de calibrage introuvable : releve non capture"));
			return true;
		}
		if (BRTestV416Vis::Phase == 0)
		{
			const FVector Front = Target->GetInteractPoint() + Target->GetActorForwardVector() * 300.f;
			C->SetActorLocation(FVector(Front.X, Front.Y, C->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			BRTestV416Vis::Phase = 1;
			return false;
		}
		if (AController* Ctrl = C->GetController())
		{
			Ctrl->SetControlRotation((Target->GetInteractPoint() - C->GetEyeLocation()).Rotation());
		}
		L->SetFocus(true);
		if (L->GetFocusDevice() != Target && StepTime < 4.f)
		{
			return false;
		}
		if (BRTestV416Vis::Phase == 1)
		{
			BRTestV416Vis::Phase = 2;
			BRTestV416Vis::PhaseClock = StepTime;
			return false;
		}
		if (StepTime - BRTestV416Vis::PhaseClock < 0.4f)
		{
			return false;
		}
		Note(FString::Printf(TEXT("releve de la mire au scan focalise : cible %s, cone %.2f"), Oui(L->GetFocusDevice() == Target), H->LastLidarConeAlpha), L->GetFocusDevice() != Target);
		ShotNamed(TEXT("v416_lidar_releve"));
		BRTestV416Vis::Phase = 0;
		return true;
	});
	Add(TEXT("v4.16 scanner : calibrage, puis repos"), 0.f, [this, HudOf, ShotNamed]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		ABRHUD* H = HudOf();
		if (!W || !L || !H || BRTestV416Vis::Phase < 0)
		{
			return true;
		}
		if (!W->IsLidarCalibrated() && StepTime < 15.f)
		{
			L->SetFocus(true); // le releve calibre le scanner (mire dans l'axe)
			return false;
		}
		L->SetFocus(false);
		if (StepTime < 22.f && (H->LastLidarHintAlpha > 0.f || H->LastLidarChargeAlpha > 0.f || H->LastLidarConeAlpha > 0.f))
		{
			return false; // le rappel s'efface (3 s) apres le calibrage
		}
		Note(FString::Printf(TEXT("au repos apres le calibrage (%s) : rappel %.2f, recharge %.2f, cone %.2f (attendu : rien)"), Oui(W->IsLidarCalibrated()), H->LastLidarHintAlpha,
			H->LastLidarChargeAlpha, H->LastLidarConeAlpha), W->IsLidarCalibrated() && (H->LastLidarHintAlpha > 0.f || H->LastLidarChargeAlpha > 0.f || H->LastLidarConeAlpha > 0.f));
		if (!W->IsLidarCalibrated())
		{
			Skip(TEXT("scanner : calibrage non obtenu en 15 s : etat de repos non verifie"));
		}
		ShotNamed(TEXT("v416_lidar_repos"));
		return true;
	});
	for (int32 Facing = 0; Facing < 2; ++Facing)
	{
		Add(FString::Printf(TEXT("v4.16 scanner : aide des parasites (%s)"), Facing ? TEXT("devant") : TEXT("derriere")), 0.f, [this, HudOf, Facing, ShotNamed]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			ABRHUD* H = HudOf();
			if (!W || !C || !H || BRTestV416Vis::Phase < 0)
			{
				return true;
			}
			if (BRTestV416Vis::Phase == 0)
			{
				FVector Static;
				if (!W->GetLidarStatic(Static))
				{
					W->DebugCompleteMission(); // les parasites n'apparaissent qu'une fois la mission resolue
				}
				BRTestV416Vis::Phase = 1;
				return false;
			}
			FVector Static;
			if (!W->GetLidarStatic(Static))
			{
				if (StepTime < 3.f)
				{
					return false;
				}
				Skip(TEXT("scanner : parasites absents meme mission resolue : aide des parasites non capturee"));
				BRTestV416Vis::Phase = 0;
				return true;
			}
			if (BRTestV416Vis::Phase == 1)
			{
				ABRLidar::Settings().StaticAid = 2; // grande, avec les chevrons des bords
				// Une cellule praticable a 8-14 m des parasites
				const FIntPoint SC = W->WorldToCell(Static);
				FVector Spot = FVector::ZeroVector;
				bool bFound = false;
				for (int32 R = 2; R <= 5 && !bFound; ++R)
				{
					for (int32 DX = -R; DX <= R && !bFound; ++DX)
					{
						for (int32 DY = -R; DY <= R && !bFound; ++DY)
						{
							const FIntPoint Cell(SC.X + DX, SC.Y + DY);
							const float D = static_cast<float>(FVector::Dist2D(W->CellCenter(Cell), Static));
							if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) == R && PlainCell(W, Cell) && D > 800.f && D < 1400.f && !W->IsOverPit(W->CellCenter(Cell), 60.f))
							{
								Spot = W->CellCenter(Cell);
								bFound = true;
							}
						}
					}
				}
				if (!bFound)
				{
					Skip(TEXT("scanner : aucune place a 8-14 m des parasites"));
					BRTestV416Vis::Phase = 0;
					return true;
				}
				const FVector Away = Spot + (Spot - Static).GetSafeNormal2D() * 300.f;
				PlaceLooking(C, FVector(Spot.X, Spot.Y, W->FloorZAt(Spot)), Facing ? Static : FVector(Away.X, Away.Y, Static.Z));
				BRTestV416Vis::Phase = 2;
				BRTestV416Vis::PhaseClock = StepTime;
				return false;
			}
			if (StepTime - BRTestV416Vis::PhaseClock < 0.5f)
			{
				return false;
			}
			Note(FString::Printf(TEXT("aide des parasites (%s, reglage grande + bords) : affichee %.2f a %.0f m"), Facing ? TEXT("devant") : TEXT("derriere"), H->LastLidarStaticAlpha,
				FVector::Dist(C->GetEyeLocation(), Static) / 100.f), H->LastLidarStaticAlpha <= 0.f);
			ShotNamed(FString::Printf(TEXT("v416_lidar_parasites_%s"), Facing ? TEXT("devant") : TEXT("derriere")));
			ABRLidar::Settings().StaticAid = BRTestV416Vis::SavedStaticAid;
			BRTestV416Vis::Phase = 0;
			return true;
		});
	}
	Add(TEXT("v4.16 scanner : bilan"), 0.f, [this]()
	{
		BRTestV416Vis::Phase = 0;
		if (LidarReport.Draws > 0)
		{
			Note(FString::Printf(TEXT("textes du scanner : %s"), *ReportLine(LidarReport)), LidarReport.Problems() > 0);
			for (const FString& E : LidarReport.Examples)
			{
				Note(TEXT("  ") + E, true);
			}
		}
		if (ABRPlayerController* PC = GetPC())
		{
			PC->bPadActive = false;
		}
		return true;
	});

	// ================================================================================================ 3. Level 4 : entites
	AddSeedLoad(4, GallerySeed, TEXT("v4.16 visuel : Level 4 (entites)"));
	Add(TEXT("v4.16 entites : debut"), 0.f, [this, Section]()
	{
		ABRCharacter* C = GetPlayer();
		if (!C)
		{
			return false;
		}
		Section(TEXT("v4.16 entites : premiere et troisieme personne"), 4);
		int32 Bad = 0;
		const FString Mats = MaterialAudit(C, Bad);
		Note(FString::Printf(TEXT("joueur (combinaison, vue des coequipiers) : %s"), *Mats), Bad > 0);
		BRTestV416Vis::Anchor = C->GetActorLocation();
		BRTestV416Vis::AnchorRot = GetPC() ? GetPC()->GetControlRotation() : FRotator::ZeroRotator;
		return true;
	});
	for (int32 K = 0; K < static_cast<int32>(EBREntityKind::Count); ++K)
	{
		const EBREntityKind Kind = static_cast<EBREntityKind>(K);
		const FString* Source = FTextInspector::GetSourceString(ABREntity::Info(Kind).Name);
		const FString Name = (Source ? *Source : FString::FromInt(K)).Replace(TEXT("-"), TEXT("")).Replace(TEXT(" "), TEXT(""));
		const bool bDark = ABREntity::Info(Kind).bNeedsDark;
		if (bDark)
		{
			Add(FString::Printf(TEXT("v4.16 entites : noir pour %s"), *Name), 0.f, [this]()
			{
				ABRWorld* W = GetBRWorld();
				if (!W)
				{
					return true;
				}
				if (!W->IsBlackout())
				{
					W->ForceBlackout();
				}
				return (W->IsBlackout() && W->GetPower() <= 0.01f) || StepTime > 12.f;
			});
			Add(FString::Printf(TEXT("v4.16 entites : adaptation au noir (%s)"), *Name), 6.f, []()
			{
				return true;
			});
		}
		Add(FString::Printf(TEXT("v4.16 entites : %s, apparition"), *Name), 1.2f, [this, Kind, Name]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			ABRPlayerController* PC = GetPC();
			if (!W || !C || !PC)
			{
				return false;
			}
			if (C->IsThirdPerson())
			{
				C->ToggleThirdPerson();
			}
			C->SetActorLocation(BRTestV416Vis::Anchor, false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(BRTestV416Vis::AnchorRot);
			if (C->IsFlashlightOn())
			{
				C->ToggleFlashlight();
			}
			const FBREntityInfo& Info = ABREntity::Info(Kind);
			FVector Spot;
			if (!FindSpotInFront(320.f, Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 5.f, Spot))
			{
				Skip(FString::Printf(TEXT("%s : pas de place devant le joueur"), *Name));
				return true;
			}
			ABREntity* E = W->SpawnEntity(Kind, Spot);
			if (!E)
			{
				Note(FString::Printf(TEXT("%s : echec de l'apparition"), *Name), true);
				return true;
			}
			FVector ToPlayer = C->GetActorLocation() - Spot;
			ToPlayer.Z = 0.f;
			E->SetActorRotation(ToPlayer.Rotation());
			E->CustomTimeDilation = 0.02f;
			BRTestV416Vis::Entity = E;
			PC->SetControlRotation((Spot + FVector(0.f, 0.f, Info.bFlying ? 0.f : Info.HalfHeight * 0.3f) - C->GetEyeLocation()).Rotation());
			return true;
		});
		// Premiere personne : au repos, puis en menace (geste arme, comme avant un coup : MulticastAttackWindup)
		Add(FString::Printf(TEXT("v4.16 entites : %s, premiere personne"), *Name), 0.3f, [this, Name, HudOf, ShotNamed]()
		{
			ABREntity* E = BRTestV416Vis::Entity.Get();
			if (!E)
			{
				return true;
			}
			if (ABRHUD* H = HudOf())
			{
				H->Messages.Reset();
			}
			ShotNamed(FString::Printf(TEXT("v416_entite_%s_1p_repos"), *Name));
			E->CustomTimeDilation = 1.f;
			if (UFunction* F = E->FindFunction(FName(TEXT("MulticastAttackWindup"))))
			{
				struct FWindupParms
				{
					float Duration;
				} Parms{ 1.5f };
				E->ProcessEvent(F, &Parms);
			}
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 entites : %s, menace"), *Name), 0.2f, [this, Name, ShotNamed]()
		{
			ABREntity* E = BRTestV416Vis::Entity.Get();
			if (!E)
			{
				return true;
			}
			E->CustomTimeDilation = 0.02f;
			ShotNamed(FString::Printf(TEXT("v416_entite_%s_1p_menace"), *Name));
			Note(FString::Printf(TEXT("%s : menace capturee (phase d'attaque %d, avertissement %.2f)"), *Name, static_cast<int32>(E->GetAttackPhase()), E->GetWarning()));
			return true;
		});
		// Troisieme personne : de face (derriere le joueur), puis de cote (le joueur deplace a 90 degres : vue d'un coequipier)
		for (int32 View = 0; View < 2; ++View)
		{
			Add(FString::Printf(TEXT("v4.16 entites : %s, troisieme personne %d"), *Name, View), 1.f, [this, View, HudOf]()
			{
				ABREntity* E = BRTestV416Vis::Entity.Get();
				ABRCharacter* C = GetPlayer();
				ABRPlayerController* PC = GetPC();
				ABRWorld* W = GetBRWorld();
				if (!E || !C || !PC || !W)
				{
					return true;
				}
				if (!C->IsThirdPerson())
				{
					C->ToggleThirdPerson();
				}
				if (View == 1)
				{
					// Une place praticable a 90 degres de l'axe joueur -> entite, a 3,5 m de l'entite
					const FVector EL = E->GetActorLocation();
					const FVector Axis = (BRTestV416Vis::Anchor - EL).GetSafeNormal2D();
					for (const float Sign : { 1.f, -1.f })
					{
						const FVector Side = EL + FVector(-Axis.Y, Axis.X, 0.f) * Sign * 350.f;
						if (PlainCell(W, W->WorldToCell(Side)))
						{
							PlaceLooking(C, FVector(Side.X, Side.Y, W->FloorZAt(Side)), EL);
							break;
						}
					}
				}
				PC->SetControlRotation((E->GetActorLocation() - C->GetActorLocation()).Rotation() + FRotator(-8.f, 0.f, 0.f));
				if (ABRHUD* H = HudOf())
				{
					H->Messages.Reset();
				}
				return true;
			});
			Add(FString::Printf(TEXT("v4.16 entites : %s, capture troisieme personne %d"), *Name, View), 0.2f, [this, Name, View, HudOf, ShotNamed]()
			{
				if (ABRHUD* H = HudOf())
				{
					H->Messages.Reset();
				}
				if (BRTestV416Vis::Entity.IsValid())
				{
					ShotNamed(FString::Printf(TEXT("v416_entite_%s_3p_%s"), *Name, View ? TEXT("cote") : TEXT("face")));
				}
				return true;
			});
		}
		Add(FString::Printf(TEXT("v4.16 entites : %s, appui des pieds et retrait"), *Name), 0.3f, [this, Kind, Name]()
		{
			ABREntity* E = BRTestV416Vis::Entity.Get();
			ABRCharacter* C = GetPlayer();
			if (C && C->IsThirdPerson())
			{
				C->ToggleThirdPerson();
			}
			if (!E)
			{
				return true;
			}
			const FBREntityInfo& Info = ABREntity::Info(Kind);
			int32 Bad = 0;
			const FString Mats = MaterialAudit(E, Bad);
			Note(FString::Printf(TEXT("%s : materiaux : %s"), *Name, *Mats), Bad > 0);
			if (!Info.bFlying)
			{
				// Sol sous l'entite (sans elle ni les personnages) ; os le plus bas des maillages a squelette ; bas des volumes visibles
				FCollisionQueryParams Q(SCENE_QUERY_STAT(BRV416Feet), false, E);
				Q.AddIgnoredActor(C);
				FCollisionResponseParams Resp;
				Resp.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
				FHitResult Hit;
				const FVector L = E->GetActorLocation();
				if (GetWorld()->LineTraceSingleByChannel(Hit, L + FVector(0.f, 0.f, 50.f), L - FVector(0.f, 0.f, 400.f), ECC_Visibility, Q, Resp))
				{
					float LowBone = 1.0e9f;
					FName LowName = NAME_None;
					float LowBounds = 1.0e9f;
					TArray<UPrimitiveComponent*> Prims;
					E->GetComponents<UPrimitiveComponent>(Prims);
					for (const UPrimitiveComponent* P : Prims)
					{
						if (!P || !P->IsVisible() || P->IsA<UCapsuleComponent>())
						{
							continue;
						}
						LowBounds = FMath::Min(LowBounds, static_cast<float>(P->Bounds.Origin.Z - P->Bounds.BoxExtent.Z));
						if (const USkinnedMeshComponent* Sk = Cast<USkinnedMeshComponent>(P))
						{
							for (int32 B = 0; B < Sk->GetNumBones(); ++B)
							{
								const FName Bone = Sk->GetBoneName(B);
								const float Z = static_cast<float>(Sk->GetBoneLocation(Bone, EBoneSpaces::WorldSpace).Z);
								if (Z < LowBone)
								{
									LowBone = Z;
									LowName = Bone;
								}
							}
						}
					}
					const float Floor = static_cast<float>(Hit.ImpactPoint.Z);
					Note(FString::Printf(TEXT("%s : appui au sol : %s ; bas des volumes visibles a %+.1f cm du sol"), *Name,
						LowBone < 1.0e8f ? *FString::Printf(TEXT("os le plus bas \"%s\" a %+.1f cm"), *LowName.ToString(), LowBone - Floor) : TEXT("sans squelette (pieces rigides)"),
						LowBounds < 1.0e8f ? LowBounds - Floor : 0.f));
				}
			}
			E->Destroy();
			BRTestV416Vis::Entity.Reset();
			return true;
		});
		if (bDark)
		{
			Add(FString::Printf(TEXT("v4.16 entites : retour du courant apres %s"), *Name), 5.f, [this]()
			{
				if (ABRWorld* W = GetBRWorld())
				{
					if (W->IsBlackout())
					{
						W->ForceRestore();
					}
				}
				return true;
			});
		}
	}

	// ================================================================================================ 4. Level 0 : kit et decalques
	AddSeedLoad(0, KitSeed, TEXT("v4.16 visuel : Level 0, graine 9 (kit)"));
	Add(TEXT("v4.16 kit : raccords des modules"), 2.f, [this, Section]()
	{
		ABRWorld* W = GetBRWorld();
		UBRAssets* A = UBRAssets::Get(this);
		if (!W || !A || !W->IsLevelReady() || W->GetLevelNumber() != 0)
		{
			return StepTime > 40.f;
		}
		// Chunks construits jusqu'au bout (modules, plinthes, decalques : crees par etapes dans le budget de l'image)
		TSet<FIntPoint> Ready;
		int32 Building = 0;
		for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
		{
			if (It->IsReady())
			{
				Ready.Add(It->Coord);
			}
			else
			{
				++Building;
			}
		}
		if (Building > 0 && StepTime < 30.f)
		{
			return false;
		}
		Section(TEXT("v4.16 kit du Level 0 : raccords"), 0);
		if (!A->UseLevel0Kit())
		{
			Skip(FString::Printf(TEXT("kit du Level 0 non utilise (%s) : raccords non verifies"), *A->GetLevel0KitStatus()));
			return true;
		}
		const FBRLevelDef& Def = W->Def();
		const float S = Def.CellSize;
		const float T = Def.WallThickness;
		const float H = Def.WallHeight;
		// Boites (monde) des instances du kit, par famille
		TArray<FBox> Baseboards, Posts, PostCores, Walls, Ceilings, Floors, Casings;
		for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
		{
			TArray<UInstancedStaticMeshComponent*> Isms;
			It->GetComponents<UInstancedStaticMeshComponent>(Isms);
			for (const UInstancedStaticMeshComponent* I : Isms)
			{
				const UStaticMesh* M = I ? I->GetStaticMesh() : nullptr;
				if (!M || !M->GetPathName().Contains(TEXT("/Level0Kit/")))
				{
					continue;
				}
				const FString Mn = M->GetName();
				const FBox Local = M->GetBoundingBox();
				for (int32 n = 0; n < I->GetInstanceCount(); ++n)
				{
					FTransform Tr;
					I->GetInstanceTransform(n, Tr, true);
					const FBox B = Local.TransformBy(Tr);
					if (Mn.StartsWith(TEXT("SM_L0_Baseboard")))
					{
						Baseboards.Add(B);
					}
					else if (Mn.StartsWith(TEXT("SM_L0_Wall_Corner")) || Mn.StartsWith(TEXT("SM_L0_Wall_Junction")) || Mn.StartsWith(TEXT("SM_L0_Wall_End"))
						|| Mn.StartsWith(TEXT("SM_L0_Wall_Straight")) || Mn.StartsWith(TEXT("SM_L0_Pillar")))
					{
						Posts.Add(B);
						const FVector C0 = Tr.GetLocation();
						const float Half = Mn.StartsWith(TEXT("SM_L0_Pillar")) ? 22.5f : T * 0.5f;
						PostCores.Add(FBox(C0 - FVector(Half, Half, 0.f), C0 + FVector(Half, Half, H)));
					}
					else if (Mn.StartsWith(TEXT("SM_L0_Wall_")) || Mn.StartsWith(TEXT("SM_L0_Doorway")) || Mn.StartsWith(TEXT("SM_L0_ServiceDoor_Wall")))
					{
						Walls.Add(B);
					}
					else if (Mn.StartsWith(TEXT("SM_L0_Ceiling")))
					{
						Ceilings.Add(B);
					}
					else if (Mn.StartsWith(TEXT("SM_L0_Floor")))
					{
						Floors.Add(B);
					}
					else if (Mn.StartsWith(TEXT("SM_L0_DoorCasing")))
					{
						Casings.Add(B);
					}
				}
			}
		}
		// Cellule mesurable : son chunk et ceux de ses huit voisines sont construits (un module d'une arete ou d'un sommet au bord
		// d'un chunk peut etre pose par le chunk voisin)
		auto Measurable = [W, &Ready](int32 X, int32 Y)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					if (!Ready.Contains(W->CellToChunk(FIntPoint(X + DX, Y + DY))))
					{
						return false;
					}
				}
			}
			return true;
		};
		auto Inside = [](const TArray<FBox>& Boxes, const FVector& P, float Tol)
		{
			for (const FBox& B : Boxes)
			{
				if (P.X >= B.Min.X - Tol && P.X <= B.Max.X + Tol && P.Y >= B.Min.Y - Tol && P.Y <= B.Max.Y + Tol && P.Z >= B.Min.Z - Tol && P.Z <= B.Max.Z + Tol)
				{
					return true;
				}
			}
			return false;
		};
		// Plinthes : le long de chaque face de mur (aretes mur et jambages des passages) des cellules praticables construites, un
		// point par 2 cm a 0,8 cm devant la face et 6 cm du sol ; un point dans un mur ou un poteau voisin est cache ; sinon une
		// plinthe (ou celle d'un poteau) doit le couvrir. Trous : suites de points non couverts
		int32 Faces = 0, Points = 0, Hidden = 0, Covered = 0, Gaps = 0;
		float MaxGap = 0.f;
		TArray<FString> GapWhere;
		const float DW = FMath::Min(Def.DoorWidth, S - 2.f * T - 20.f);
		for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
		{
			const FIntPoint Ch = It->Coord;
			const int32 N = Def.ChunkCells;
			for (int32 X = Ch.X * N; X < Ch.X * N + N; ++X)
			{
				for (int32 Y = Ch.Y * N; Y < Ch.Y * N + N; ++Y)
				{
					const FIntPoint Cell(X, Y);
					if (!W->IsWalkable(Cell) || W->IsPitRoomCell(X, Y) || !Measurable(X, Y))
					{
						continue;
					}
					for (int32 Side = 0; Side < 4; ++Side)
					{
						const FIntPoint Dir = Side == 0 ? FIntPoint(1, 0) : (Side == 1 ? FIntPoint(-1, 0) : (Side == 2 ? FIntPoint(0, 1) : FIntPoint(0, -1)));
						const EBREdge E = Dir.X != 0 ? W->EdgeE(Dir.X > 0 ? X : X - 1, Y) : W->EdgeN(X, Dir.Y > 0 ? Y : Y - 1);
						const bool bService = Dir.X != 0 ? W->IsServiceDoorEdge(Dir.X > 0 ? X : X - 1, Y, true) : W->IsServiceDoorEdge(X, Dir.Y > 0 ? Y : Y - 1, false);
						if (E == EBREdge::Open || bService)
						{
							continue;
						}
						++Faces;
						const FVector Out(Dir.X, Dir.Y, 0.f);
						const FVector Along(-Dir.Y, Dir.X, 0.f);
						const FVector Face = W->CellCenter(Cell) + Out * (S * 0.5f - T * 0.5f - 0.8f) + FVector(0.f, 0.f, 6.f);
						float Run = 0.f;
						for (float D = -S * 0.5f; D <= S * 0.5f; D += 2.f)
						{
							if (E == EBREdge::Door && FMath::Abs(D) < DW * 0.5f + 0.5f)
							{
								Run = 0.f; // ouverture du passage : pas de plinthe
								continue;
							}
							const FVector P = Face + Along * D;
							++Points;
							if (Inside(Walls, P, -0.2f) || Inside(PostCores, P, -0.2f))
							{
								++Hidden;
								Run = 0.f;
								continue;
							}
							if (Inside(Baseboards, P, 0.05f) || Inside(Posts, P, 0.05f) || Inside(Casings, P, 0.05f))
							{
								++Covered;
								Run = 0.f;
								continue;
							}
							if (Run == 0.f)
							{
								++Gaps;
								if (GapWhere.Num() < 6)
								{
									GapWhere.Add(FString::Printf(TEXT("(%.0f, %.0f) face %d de la cellule (%d, %d)"), P.X, P.Y, Side, X, Y));
								}
							}
							Run += 2.f;
							MaxGap = FMath::Max(MaxGap, Run);
						}
					}
				}
			}
		}
		Note(FString::Printf(TEXT("plinthes : %d face(s) de mur, %d point(s) (%d caches dans un mur ou un poteau), %d couverts, %d trou(s) (le plus long %.0f cm)%s%s"), Faces, Points,
			Hidden, Covered, Gaps, MaxGap, GapWhere.Num() ? TEXT(" : ") : TEXT(""), *FString::Join(GapWhere, TEXT(", "))), Gaps > 0 || Faces == 0);
		// Plafonds et sols : une boite par cellule praticable, sans trou entre deux cellules, hauteur uniforme
		int32 Cells = 0, NoCeil = 0, NoFloor = 0, CeilSeams = 0;
		float CeilMin = 1.0e9f, CeilMax = -1.0e9f, FloorMin = 1.0e9f, FloorMax = -1.0e9f;
		for (const FBox& B : Ceilings)
		{
			CeilMin = FMath::Min(CeilMin, static_cast<float>(B.Min.Z));
			CeilMax = FMath::Max(CeilMax, static_cast<float>(B.Min.Z));
		}
		for (const FBox& B : Floors)
		{
			FloorMin = FMath::Min(FloorMin, static_cast<float>(B.Max.Z));
			FloorMax = FMath::Max(FloorMax, static_cast<float>(B.Max.Z));
		}
		for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
		{
			const FIntPoint Ch = It->Coord;
			const int32 N = Def.ChunkCells;
			for (int32 X = Ch.X * N; X < Ch.X * N + N; ++X)
			{
				for (int32 Y = Ch.Y * N; Y < Ch.Y * N + N; ++Y)
				{
					if (!W->IsWalkable(FIntPoint(X, Y)) || W->IsPitRoomCell(X, Y) || !Measurable(X, Y))
					{
						continue;
					}
					++Cells;
					const FVector C0 = W->CellCenter(FIntPoint(X, Y));
					const bool bCeil = Inside(Ceilings, C0 + FVector(0.f, 0.f, H + 1.f), 0.1f);
					NoCeil += bCeil ? 0 : 1;
					NoFloor += Inside(Floors, C0 + FVector(0.f, 0.f, -1.f), 0.1f) ? 0 : 1;
					// Joint avec la cellule voisine a l'est, sans mur entre elles, les deux sous un plafond du kit : il continue
					const bool bEastCeil = Inside(Ceilings, W->CellCenter(FIntPoint(X + 1, Y)) + FVector(0.f, 0.f, H + 1.f), 0.1f);
					if (bCeil && bEastCeil && W->EdgeE(X, Y) == EBREdge::Open && W->IsWalkable(FIntPoint(X + 1, Y)) && !W->IsPitRoomCell(X + 1, Y))
					{
						for (float D = -0.6f; D <= 0.6f; D += 0.3f)
						{
							if (!Inside(Ceilings, C0 + FVector(S * 0.5f + D, 0.f, H + 1.f), 0.02f))
							{
								++CeilSeams;
								break;
							}
						}
					}
				}
			}
		}
		Note(FString::Printf(TEXT("plafonds et sols du kit : %d cellule(s) praticable(s) (hors salles de fosses) ; sans plafond %d (trappes d'echelle comprises), sans sol %d ; joints de "
			"plafond ouverts entre deux cellules %d ; dessous des plafonds de %.1f a %.1f cm, dessus des sols de %.1f a %.1f cm ; %d chambranle(s), %d plinthe(s), %d poteau(x)"), Cells, NoCeil,
			NoFloor, CeilSeams, CeilMin, CeilMax, FloorMin, FloorMax, Casings.Num(), Baseboards.Num(), Posts.Num()),
			CeilSeams > 0 || (Ceilings.Num() > 0 && CeilMax - CeilMin > 0.5f) || (Floors.Num() > 0 && FloorMax - FloorMin > 0.5f));
		return true;
	});
	// Captures des raccords, a hauteur d'oeil (accroupi pour les plinthes) : angle, poteau droit, jambage, plafond contre mur,
	// plafond entre deux cellules
	struct FJoint
	{
		const TCHAR* Name;
		int32 Kind; // 0 angle interieur, 1 mur continu (poteau droit), 2 jambage, 3 plafond contre mur, 4 plafond entre deux cellules
	};
	const TArray<FJoint> Joints = { { TEXT("angle"), 0 }, { TEXT("poteau_droit"), 1 }, { TEXT("jambage"), 2 }, { TEXT("plafond_mur"), 3 }, { TEXT("plafond_cellules"), 4 } };
	for (const FJoint& J : Joints)
	{
		Add(FString::Printf(TEXT("v4.16 kit : pose %s"), J.Name), 1.5f, [this, J, HudOf]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return true;
			}
			if (ABRHUD* H = HudOf())
			{
				H->Messages.Reset();
			}
			const FBRLevelDef& Def = W->Def();
			const float S = Def.CellSize;
			const float T = Def.WallThickness;
			const FIntPoint Start = W->WorldToCell(C->GetActorLocation());
			bool bFound = false;
			for (int32 R = 0; R <= 8 && !bFound; ++R)
			{
				for (int32 DX = -R; DX <= R && !bFound; ++DX)
				{
					for (int32 DY = -R; DY <= R && !bFound; ++DY)
					{
						if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R)
						{
							continue;
						}
						const FIntPoint Cell(Start.X + DX, Start.Y + DY);
						if (!PlainCell(W, Cell) || W->IsOverPit(W->CellCenter(Cell), 60.f))
						{
							continue;
						}
						const FVector Ctr = W->CellCenter(Cell);
						const float Floor = W->FloorZAt(Ctr);
						const bool bN = W->EdgeN(Cell.X, Cell.Y) == EBREdge::Wall;
						const bool bE = W->EdgeE(Cell.X, Cell.Y) == EBREdge::Wall;
						const float In = S * 0.5f - T * 0.5f; // face du mur depuis le centre
						if (J.Kind == 0 && bN && bE)
						{
							// Angle nord-est, vu de la diagonale, accroupi
							BRTestV416Vis::Anchor = FVector(Ctr.X + In, Ctr.Y + In, Floor + 6.f);
							PlaceLooking(C, FVector(Ctr.X + In - 95.f, Ctr.Y + In - 95.f, Floor), BRTestV416Vis::Anchor);
							bFound = true;
						}
						else if (J.Kind == 1 && bN && W->EdgeN(Cell.X + 1, Cell.Y) == EBREdge::Wall && W->EdgeE(Cell.X, Cell.Y) == EBREdge::Open
							&& W->EdgeE(Cell.X, Cell.Y + 1) != EBREdge::Wall && PlainCell(W, FIntPoint(Cell.X + 1, Cell.Y)))
						{
							// Mur nord continu au-dessus du coin nord-est (poteau droit entre deux modules)
							BRTestV416Vis::Anchor = FVector(Ctr.X + S * 0.5f, Ctr.Y + In, Floor + 6.f);
							PlaceLooking(C, FVector(Ctr.X + S * 0.5f, Ctr.Y + In - 110.f, Floor), BRTestV416Vis::Anchor);
							bFound = true;
						}
						else if (J.Kind == 2 && W->EdgeN(Cell.X, Cell.Y) == EBREdge::Door)
						{
							const float DW = FMath::Min(Def.DoorWidth, S - 2.f * T - 20.f);
							BRTestV416Vis::Anchor = FVector(Ctr.X + DW * 0.5f + 5.f, Ctr.Y + In, Floor + 40.f);
							PlaceLooking(C, FVector(Ctr.X + DW * 0.5f + 60.f, Ctr.Y + In - 120.f, Floor), BRTestV416Vis::Anchor);
							bFound = true;
						}
						else if (J.Kind == 3 && bN)
						{
							BRTestV416Vis::Anchor = FVector(Ctr.X, Ctr.Y + In, Floor + Def.WallHeight);
							PlaceLooking(C, FVector(Ctr.X, Ctr.Y + In - 90.f, Floor), BRTestV416Vis::Anchor);
							bFound = true;
						}
						else if (J.Kind == 4 && W->EdgeE(Cell.X, Cell.Y) == EBREdge::Open && PlainCell(W, FIntPoint(Cell.X + 1, Cell.Y)) && !W->CellLight(Cell.X, Cell.Y).bHas)
						{
							BRTestV416Vis::Anchor = FVector(Ctr.X + S * 0.5f, Ctr.Y + 30.f, Floor + Def.WallHeight);
							PlaceLooking(C, FVector(Ctr.X + S * 0.5f - 80.f, Ctr.Y, Floor), BRTestV416Vis::Anchor);
							bFound = true;
						}
					}
				}
			}
			if (!bFound)
			{
				Skip(FString::Printf(TEXT("kit : aucune pose \"%s\" a moins de 8 cellules du depart"), J.Name));
				BRTestV416Vis::Phase = -1;
				return true;
			}
			BRTestV416Vis::Phase = 0;
			const bool bLow = J.Kind <= 2;
			if (bLow != C->bIsCrouched)
			{
				C->ToggleCrouch();
			}
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 kit : regard %s"), J.Name), 0.4f, [this]()
		{
			ABRCharacter* C = GetPlayer();
			if (C && BRTestV416Vis::Phase >= 0)
			{
				if (AController* Ctrl = C->GetController())
				{
					Ctrl->SetControlRotation((BRTestV416Vis::Anchor - C->GetEyeLocation()).Rotation());
				}
			}
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 kit : capture %s"), J.Name), 0.2f, [this, J, ShotNamed]()
		{
			if (BRTestV416Vis::Phase >= 0)
			{
				ShotNamed(FString::Printf(TEXT("v416_kit_%s"), J.Name));
			}
			BRTestV416Vis::Phase = 0;
			return true;
		});
	}
	Add(TEXT("v4.16 kit : debout"), 0.5f, [this]()
	{
		if (ABRCharacter* C = GetPlayer())
		{
			if (C->bIsCrouched)
			{
				C->ToggleCrouch();
			}
		}
		return true;
	});
	DecalSteps(0, TEXT("Level 0, graine 9"));
	AddSeedLoad(0, 4242u, TEXT("v4.16 visuel : Level 0, graine 4242 (decalques)"));
	DecalSteps(0, TEXT("Level 0, graine 4242"));
	AddSeedLoad(2, 4242u, TEXT("v4.16 visuel : Level 2, graine 4242 (decalques)"));
	DecalSteps(2, TEXT("Level 2, graine 4242"));

	// ================================================================================================ 5. bilan
	Add(TEXT("v4.16 visuel : bilan"), 2.f, [this, Section, CloseScreens]()
	{
		CloseScreens();
		Section(TEXT("v4.16 visuel : bilan"), 0);
		if (BRTestV416Vis::bSaved)
		{
			FBRSettings::Get() = BRTestV416Vis::SavedSettings;
			ABRLidar::Settings().StaticAid = BRTestV416Vis::SavedStaticAid;
			BRLoc::SetLanguage(BRTestV416Vis::SavedLang);
			if (ABRPlayerController* PC = GetPC())
			{
				PC->bPadActive = false;
				PC->ApplySettings();
			}
			BRKeys::SetGamepadPrompts(false);
			if (BRTestV416Vis::bDisplayTouched && BRDisplay::CanChange())
			{
				if (BRDisplay::IsPending())
				{
					BRDisplay::Revert();
				}
				BRDisplay::Request(BRTestV416Vis::SavedMode.Window, BRTestV416Vis::SavedMode.Resolution);
				BRDisplay::Confirm();
			}
			BRTestV416Vis::bSaved = false;
		}
		BRTestV416Vis::Targets.Reset();
		Note(FString::Printf(TEXT("banc v4.16 visuel : %d capture(s) (v416_*.png) ; reglages, langue (%s), affichage et manette du joueur retablis"), BRTestV416Vis::ShotCount,
			*BRTestV416Vis::SavedLang));
		return true;
	});
}
