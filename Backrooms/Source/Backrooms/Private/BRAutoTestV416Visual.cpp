// v4.16 (vis) : banc de la finition graphique et du HUD (-BRAutoTest -BRAutoTestV416Visual). Captures et rapport :
//   1. HUD : inventaire (PERSONNAGE, CARNET, JOURNAL, PARAMETRES > VIDEO, TOUCHES) et pause, rendus hors ecran aux tailles
//      exactes 1280 x 720, 1920 x 1080 et 2560 x 1080 (cible de rendu : la fenetre de test ne peut pas depasser l'ecran),
//      dans les 22 langues : chaque texte dessine est releve (ABRHUD::bTestRecordTexts) et controle : dans l'image, dans son
//      panneau, dans la largeur permise par son appelant, sans chevauchement ; textes raccourcis et tailles effectives notes.
//      Captures v416_hud_<taille>_<langue>_<onglet> (fr, de, ru, ar, ja) ;
//   2. vraies fenetres : 1280 x 720, puis les plus grandes fenetres 16:9 et 64:27 que l'ecran accepte (taille obtenue
//      comparee a la taille demandee), memes controles sur l'image reelle ; modes d'affichage : libelles de la ligne MODE
//      D'AFFICHAGE lus dans chaque mode essaye, essai du plein ecran fenetre (sans bordures : mode et taille de l'ecran obtenus)
//      et retour a la fenetre du lancement (le plein ecran exclusif seulement avec -BRV416PleinEcran, sinon NON VERIFIE) ;
//   3. manette : invites des boutons (HUD, pause, inventaire, scanner) = liaisons reelles de la manette ; inventaire a la
//      manette par de vraies touches injectees (LB / RB, croix, Y, B ; dans PERSONNAGE, curseur des cases, X inspecter, A
//      equiper) ; cartes de la pause a la croix et A, puis dans l'inventaire ouvert depuis la pause (jeu arrete) RB / LB et
//      B (retour a la pause) ; vision nocturne du camescope reste dans le sac (croix haut). v4.17 (interface) : onglets par
//      LB / RB (la croix choisira la colonne du carnet) ; deplacer un objet (ABRHUD::PadCarry, B annule le port), PARAMETRES
//      (ABRHUD::PadSettingsMove / PadSettingsAdjust : LUMINOSITE d'un cran puis retour) ; carnet long a 1280 x 720 et taille
//      1,25 (une ligne par appui, ABRHUD::PadScrollLines) ; captures du carnet des vraies fenetres prises avant la fermeture ;
//   4. scanner du Level 6 (graine 4136) : HUD contextuel (rappel des touches au clavier et a la manette, recharge, cone du
//      focus, releve de la mire, repos, aide des parasites derriere et devant) ;
//   5. les neuf entites (Level 4, graine 4242 ; Smiler dans le noir) : premiere personne au repos (posee au sol) et en menace
//      (geste arme), troisieme personne locale de face et de cote (affectation des materiaux, appui des pieds, poses) ;
//      ecart au sol du bas des volumes visibles controle (pieces rigides : leurs vrais sommets quand l'editeur les lit, sinon
//      leurs boites). La vue d'un vrai coequipier (client) n'est pas faite ici : NON VERIFIE (banc reseau) ;
//   6. kit du Level 0 (graine 9) : raccords des plinthes le long de chaque face de mur, plafonds et sols cellule par cellule
//      (boites des instances posees), captures des raccords (angle, poteau droit, jambage, plafond contre mur, plafond entre
//      deux cellules) ;
//   7. decalques d'usure (Levels 0 graines 9 et 4242, 1 et 2 graine 4242) : surface sous chaque decalque (traces), aucun sur
//      une ouverture, un objet ou en partie dans le vide (murs), taches au sol posees (ni flottantes ni enfouies), nombre par
//      chunk et tirages ecartes par la borne (chaque chunk garde min(tirages, borne)), distance utile pour la vue locale,
//      motifs. Chunks construits et en place seulement ; un support (mur, sol, plafond) bati par un chunk pas encore construit
//      (bord de la zone construite) n'est pas juge : NON VERIFIE, et le decalque doit etre au-dela de son effacement.
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
#include "Components/StaticMeshComponent.h"
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
#include "Rendering/PositionVertexBuffer.h"
#include "StaticMeshResources.h"
#include "UObject/StrongObjectPtr.h"

namespace BRTestV416Vis
{
	const TCHAR* Oui(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	/** v4.16 (relecture 2) : plus bas des vrais sommets d'un maillage statique, dans le monde (LOD 0 ; donnees lisibles par le
	 *  processeur : editeur, ou maillage qui l'autorise) ; false sans ces donnees */
	bool LowestVertexZ(const UStaticMeshComponent* Sm, float& OutZ)
	{
		const UStaticMesh* Mesh = Sm ? Sm->GetStaticMesh() : nullptr;
		const FStaticMeshRenderData* RD = Mesh ? Mesh->GetRenderData() : nullptr;
		if (!RD || RD->LODResources.Num() == 0)
		{
			return false;
		}
		const FPositionVertexBuffer& Pos = RD->LODResources[0].VertexBuffers.PositionVertexBuffer;
		if (!Pos.GetAllowCPUAccess() || !Pos.GetVertexData() || Pos.GetNumVertices() == 0)
		{
			return false;
		}
		const FTransform Tr = Sm->GetComponentTransform();
		float Low = TNumericLimits<float>::Max();
		for (uint32 i = 0; i < Pos.GetNumVertices(); ++i)
		{
			Low = FMath::Min(Low, static_cast<float>(Tr.TransformPosition(FVector(Pos.VertexPosition(i))).Z));
		}
		OutZ = Low;
		return true;
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
		/** v4.16 (relecture) : couche (infobulle, inspection : au-dessus du reste, sur leur propre fond) */
		int32 Layer = 0;
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

	/** v4.16 (relecture) : le texte montre la valeur repere de la sante (MarkerHealth = 93) en pourcentage : "93 %", "93%", en
	 *  chiffres latins, arabes ou persans, espaces insecables compris */
	bool ShowsMarkerPercent(const FString& S)
	{
		const TCHAR* const Forms[] = { TEXT("93"), TEXT("\u0669\u0663"), TEXT("\u06f9\u06f3") };
		for (const TCHAR* Form : Forms)
		{
			int32 From = 0;
			while (true)
			{
				const int32 At = S.Find(Form, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
				if (At == INDEX_NONE)
				{
					break;
				}
				int32 K = At + 2;
				while (K < S.Len() && (S[K] == TEXT(' ') || S[K] == 0x00A0 || S[K] == 0x202F || S[K] == 0x2009))
				{
					++K;
				}
				if (K < S.Len() && (S[K] == TEXT('%') || S[K] == 0x066A))
				{
					return true;
				}
				From = At + 1;
			}
		}
		return false;
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
			// Hors du panneau : contenance stricte (2 px de marge). v4.16 (relecture) : chaque panneau du HUD est ferme a la fin de
			// son contenu (ABRHUD::EndTestPanel) : un texte dessine apres lui est rapporte a la bande de l'inventaire ou a l'image ;
			// une ligne entiere sous le bord du panneau est donc un debordement (avant : ignoree, il fallait qu'elle touche le panneau)
			if (A.PW > 0.f && (A.X < A.PX - 2.f || A.X + A.W > A.PX + A.PW + 2.f || A.Y < A.PY - 2.f || A.Y + A.H > A.PY + A.PH + 2.f))
			{
				++R.OutPanel;
				R.Example(FString::Printf(TEXT("%s : hors de son panneau (texte %.0f..%.0f x %.0f..%.0f, panneau %.0f..%.0f x %.0f..%.0f) \"%s\""), *Where, A.X, A.X + A.W, A.Y,
					A.Y + A.H, A.PX, A.PX + A.PW, A.PY, A.PY + A.PH, *Short(A.Text)));
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
				if (A.Layer != B.Layer)
				{
					continue; // v4.16 (relecture) : une infobulle ou une inspection cache ce qui est dessous (son fond)
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
	/** Mode enregistre du joueur (GameUserSettings : retabli au bilan) et fenetre reellement ouverte au lancement (-windowed,
	 *  -ResX / -ResY : retablie pour la suite du banc et comparee). v4.16 (relecture) : avant, la fenetre du milieu du banc
	 *  etait le mode enregistre, qui peut differer de la fenetre du lancement */
	BRDisplay::FMode SavedMode;
	BRDisplay::FMode SavedEff;
	bool bDisplayTouched = false;
	/** v4.16 (relecture) : taille demandee de chaque vraie fenetre (comparee a la taille obtenue) ; mode essaye (-1 : aucun) */
	FIntPoint WantSizes[3] = { FIntPoint::ZeroValue, FIntPoint::ZeroValue, FIntPoint::ZeroValue };
	int32 ModeTried = -1;
	/** v4.16 (relecture) : libelle de la ligne MODE D'AFFICHAGE lu et juste pour ce mode reellement obtenu (0, 1, 2) */
	bool LabelSeen[3] = { false, false, false };
	/** v4.16 (relecture) : kit du Level 0 mesure (niveau pret, kit utilise) : sinon les captures des raccords sont sautees */
	bool bKitReady = false;
	/** v4.16 (relecture) : etat note avant un appui injecte (onglet, aide, defilement, reste a defiler) */
	int32 PressInt = 0;
	float PressFloat = 0.f;
	float PressRoom = 0.f;
	/** v4.17 (correction) : carnet long : defilement de la colonne OBSERVATIONS avant l'appui, langue d'avant l'essai */
	float PressRight = 0.f;
	FString LongLang;
	/** v4.16 (relecture) : sac et equipement modifies pour l'essai des objets a la manette (retablis), batterie et vision
	 *  nocturne avant l'essai du camescope */
	bool bInvSaved = false;
	FBRItemSlot SavedStorage1;
	FBRItemSlot SavedHead;
	float SavedBattery = 100.f;
	/** v4.16 (relecture) : sante, endurance, sante mentale du joueur avant les rendus (valeurs reperes : une sante affichee en
	 *  chiffres serait vue) */
	bool bVitalsSaved = false;
	float SavedHealth = 100.f;
	float SavedStamina = 100.f;
	float SavedSanity = 100.f;
	constexpr float MarkerHealth = 93.f;
	int32 HealthReadouts = 0;
	int32 GameHudBars = 0;
	int32 GameHudDraws = 0;
	/** v4.16 (relecture) : invites de la pause a la manette : boutons de la manette vus (A) et touche FIN du clavier vue */
	int32 PadPauseDraws = 0;
	int32 PadPauseConfirmSeen = 0;
	int32 PadPauseEndSeen = 0;
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
	int32 LastBars = 0;
	int32 GaugeCharDraws = 0;
	int32 GaugeCharBad = 0;
	int32 GaugeOtherDraws = 0;
	int32 GaugeOtherBad = 0;
	int32 ConsoleHints = 0;
	int32 Phase = 0;
	double PhaseClock = 0.0;
	FVector Anchor = FVector::ZeroVector;
	FRotator AnchorRot = FRotator::ZeroRotator;
	TWeakObjectPtr<ABREntity> Entity;
	FString PadFooterSeen;
	/** v4.17 (interface) : 1re case du sac et 4e poche avant l'essai du deplacement a la manette (retablies) ; ligne
	 *  LUMINOSITE, sens du premier cran, valeur et texte affiche avant l'essai des PARAMETRES a la manette */
	bool bCarrySaved = false;
	FBRItemSlot SavedCarryStorage;
	FBRItemSlot SavedCarryPocket;
	int32 BrightRow = INDEX_NONE;
	int32 BrightDir = 1;
	float BrightBefore = 0.f;
	FString BrightShownBefore;

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

	/** v4.16 (relecture) : boites (monde) rangees par case de grille au sol ; Inside ne teste que les boites de la case du point
	 *  (chaque boite est rangee dans toutes les cases qu'elle touche, marge Margin comprise : Tol <= Margin) */
	struct FBoxGrid
	{
		float CellSize = 100.f;
		const TArray<FBox>* Boxes = nullptr;
		TMap<FIntPoint, TArray<int32>> Buckets;

		FIntPoint KeyOf(double X, double Y) const
		{
			return FIntPoint(FMath::FloorToInt32(static_cast<float>(X) / CellSize), FMath::FloorToInt32(static_cast<float>(Y) / CellSize));
		}
		void Build(const TArray<FBox>& In, float InCellSize, float Margin)
		{
			Boxes = &In;
			CellSize = FMath::Max(1.f, InCellSize);
			Buckets.Reset();
			for (int32 i = 0; i < In.Num(); ++i)
			{
				const FBox& B = In[i];
				const FIntPoint Lo = KeyOf(B.Min.X - Margin, B.Min.Y - Margin);
				const FIntPoint Hi = KeyOf(B.Max.X + Margin, B.Max.Y + Margin);
				for (int32 KX = Lo.X; KX <= Hi.X; ++KX)
				{
					for (int32 KY = Lo.Y; KY <= Hi.Y; ++KY)
					{
						Buckets.FindOrAdd(FIntPoint(KX, KY)).Add(i);
					}
				}
			}
		}
		bool Inside(const FVector& P, float Tol) const
		{
			const TArray<int32>* List = Boxes ? Buckets.Find(KeyOf(P.X, P.Y)) : nullptr;
			if (!List)
			{
				return false;
			}
			for (const int32 i : *List)
			{
				const FBox& B = (*Boxes)[i];
				if (P.X >= B.Min.X - Tol && P.X <= B.Max.X + Tol && P.Y >= B.Min.Y - Tol && P.Y <= B.Max.Y + Tol && P.Z >= B.Min.Z - Tol && P.Z <= B.Max.Z + Tol)
				{
					return true;
				}
			}
			return false;
		}
	};

	// ---- decalques : mesures sur les chunks construits
	struct FDecalStats
	{
		int32 Chunks = 0;
		int32 Projected = 0;
		int32 Patches = 0;
		int32 Expected = 0;
		/** v4.16 (relecture) : tirages ecartes par les bornes (ABRChunk::GetDecalDropped) */
		int32 Dropped = 0;
		/** v4.16 (relecture) : decalques de mur en partie dans le vide (au moins un point sur rien, au moins un sur la surface) */
		int32 WallPartVoid = 0;
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
		/** v4.16 (relecture 2) : bornes : chunks dont les tirages depassent une borne ; chunks qui ne gardent pas exactement
		 *  min(tirages, borne) d'une sorte (ABRChunk::GetDecalDrawn) */
		int32 CappedChunks = 0;
		int32 CapMismatch = 0;
		/** v4.16 (relecture 2) : bord de la zone construite : point sans surface dont le support (mur, sol, plafond) est bati
		 *  par un chunk absent ou pas pret (le mur d'une ligne de la grille est bati par le chunk de la cellule a l'ouest ou au
		 *  sud ; le sol et le plafond par celui de la cellule). Non juge, compte a part ; le decalque doit etre au-dela de sa
		 *  distance d'effacement (sinon il se verrait sans support : probleme) */
		int32 FrontierSamples = 0;
		int32 DecalsFrontier = 0;
		int32 FrontierVisible = 0;
		float FrontierNearest = 1.0e9f;
		int32 PatchFrontier = 0;
		int32 PatchFrontierVisible = 0;
		float PatchFrontierNearest = 1.0e9f;
		/** v4.16 (relecture 2) : taches au sol : point sous un objet ou un mur pose sur le sol (rayon parti dedans a 40 cm, puis
		 *  au ras de la tache) ; points dans l'ouverture d'un passage (juges au sol ; avant : comptes caches sous le mur) */
		int32 PatchUnderObject = 0;
		int32 PatchDoorway = 0;
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
			D.Layer = T.Layer;
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
		BRTestV416Vis::LastBars = Hud->TestBarsDrawn;       // v4.16 (relecture) : barres de ce rendu
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
			BRTestV416Vis::SavedEff = BRDisplay::Effective();
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
		const BRDisplay::FMode Cur = BRDisplay::Current();
		Note(FString::Printf(TEXT("ecran : %d x %d (zone utile %d x %d, %d ppp, %d ecran(s)) ; fenetre du jeu : mode %d, %d x %d (mode enregistre du joueur : %d, %d x %d) ; "
			"affichage modifiable : %s"), M.Size.X, M.Size.Y, M.WorkSize.X, M.WorkSize.Y, M.DPI, M.Count, Eff.Window, Eff.Resolution.X, Eff.Resolution.Y, Cur.Window,
			Cur.Resolution.X, Cur.Resolution.Y, BRTestV416Vis::Oui(BRDisplay::CanChange())));
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
			if (StepTime <= 20.f)
			{
				return false;
			}
			// v4.16 (relecture) : avant, l'etape passait sans rien noter
			Skip(TEXT("Level 1 : monde, HUD ou joueur absent en 20 s : carnet rempli NON VERIFIE (carnet des rendus sans indices ni observations)"));
			return true;
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
			Skip(TEXT("carnet : HUD absent : colonne la plus longue (aide et rappel) NON VERIFIEE"));
			return true;
		}
		ShowScreen(4);
		if (StepTime < 0.3f || H->NotebookHintStep < 0)
		{
			if (StepTime <= 3.f)
			{
				return false; // le carnet dessine fixe l'etape en cours (cle de l'aide)
			}
			// v4.16 (relecture) : avant, l'etape passait sans rien noter et les rendus du carnet n'avaient pas l'aide
			Skip(TEXT("carnet : aucune etape en cours en 3 s (pas de mission ou mission resolue) : colonne la plus longue (aide et rappel) NON VERIFIEE"));
			return true;
		}
		H->NotebookHint.FindOrAdd(H->NotebookHintStep) = 2; // aide et rappel : la colonne la plus longue
		Note(FString::Printf(TEXT("carnet : aide et rappel de l'etape en cours montres pour les rendus (cle %d)"), H->NotebookHintStep));
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
			// v4.16 (relecture) : valeurs reperes (sante 93, endurance et sante mentale 100) : une sante montree en pourcentage
			// serait relevee ; retablies au bilan des langues
			if (ABRCharacter* Hero = GetPlayer())
			{
				if (!bVitalsSaved)
				{
					SavedHealth = Hero->Health;
					SavedStamina = Hero->Stamina;
					SavedSanity = Hero->Sanity;
					bVitalsSaved = true;
				}
				Hero->Health = MarkerHealth;
				Hero->Stamina = 100.f;
				Hero->Sanity = 100.f;
			}
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
						// v4.16 (relecture) : sante en chiffres (hors PARAMETRES, ou un reglage peut valoir 93 %)
						HealthReadouts += (Sc.Tab != 2 && ShowsMarkerPercent(T.Text)) ? 1 : 0;
					}
					if (Sc.Tab == -2)
					{
						// HUD du jeu, sans reanimation ni action maintenue : aucune barre (Bar) attendue
						++GameHudDraws;
						GameHudBars += LastBars;
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
	// v4.16 (relecture) : controle positif de la ligne des commandes de la console : en mode developpeur (hors Shipping), la pause
	// la montre et le releve la voit. Sans lui, "0 (attendu 0)" pouvait venir d'un texte change ou coupe
	Add(TEXT("v4.16 visuel : commandes de la console en mode developpeur (controle positif)"), 0.f, [this, ShowScreen, CloseScreens, Offscreen]()
	{
#if UE_BUILD_SHIPPING
		(void)ShowScreen;
		(void)CloseScreens;
		(void)Offscreen;
		Skip(TEXT("commandes de la console : version Shipping (pas de mode developpeur) : controle positif NON VERIFIE"));
#else
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			Skip(TEXT("commandes de la console : controleur absent : controle positif NON VERIFIE"));
			return true;
		}
		BRLoc::SetLanguage(TEXT("fr"));
		FBRSettings::Get().bDevMode = true;
		ShowScreen(-1);
		TArray<FDrawnText> Texts;
		const bool bDrawn = Offscreen(1920, 1080, FString(), Texts);
		int32 Seen = 0;
		for (const FDrawnText& T : Texts)
		{
			Seen += (T.Text.Contains(TEXT("BRGod")) && T.Text.Contains(TEXT("BRLevel"))) ? 1 : 0;
		}
		FBRSettings::Get().bDevMode = false;
		CloseScreens();
		if (!bDrawn)
		{
			Skip(TEXT("commandes de la console : rendu hors ecran impossible : controle positif NON VERIFIE"));
		}
		else
		{
			Note(FString::Printf(TEXT("commandes de la console sous la pause en mode developpeur (controle positif) : %d ligne(s) relevee(s) (attendu au moins 1)"), Seen), Seen == 0);
		}
#endif
		return true;
	});
	Add(TEXT("v4.16 visuel : bilan des langues"), 0.f, [this]()
	{
		Note(FString::Printf(TEXT("HUD hors ecran (1280x720, 1920x1080, 2560x1080 ; 5 onglets, la pause et le HUD du jeu ; %d langues) : %s"), PerLanguage.Num(), *ReportLine(OffReport)),
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
			// v4.16 (relecture) : barre de vie mesuree : sante a la valeur repere 93 jamais montree en pourcentage, aucune barre
			// (Bar) sur le HUD du jeu ; avant, la phrase "aucune barre de vie" etait ecrite sans mesure
			Note(FString::Printf(TEXT("jauges : endurance et sante mentale dans PERSONNAGE (%d rendu(s), %d sans les deux jauges) ; aucune ailleurs (%d rendu(s) du jeu, "
				"du carnet, du journal, des parametres, des touches et de la pause, %d avec une jauge) ; barre de vie : sante a %.0f montree en pourcentage %d fois, "
				"%d barre(s) sur %d rendu(s) du HUD du jeu (attendu 0 et 0) ; commandes de la console sous la pause d'un joueur (mode developpeur coupe) : %d (attendu 0)"),
				GaugeCharDraws, GaugeCharBad, GaugeOtherDraws, GaugeOtherBad, MarkerHealth, HealthReadouts, GameHudBars, GameHudDraws, ConsoleHints),
				GaugeCharBad > 0 || GaugeOtherBad > 0 || ConsoleHints > 0 || GaugeCharDraws == 0 || HealthReadouts > 0 || GameHudBars > 0 || GameHudDraws == 0);
		}
		if (bVitalsSaved)
		{
			if (ABRCharacter* Hero = GetPlayer())
			{
				Hero->Health = SavedHealth;
				Hero->Stamina = SavedStamina;
				Hero->Sanity = SavedSanity;
			}
			bVitalsSaved = false;
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
					// v4.16 (relecture) : pause a la manette : chaque invite nomme un bouton de la manette (A pour valider), plus
					// la touche FIN du clavier
					const FString EndKey(BR_STR(NSLOCTEXT("BR", "HUD.Fin", "FIN")));
					const FString PadConfirm = BRKeys::GamepadName(EKeys::Gamepad_FaceButton_Bottom);
					bool bConfirm = false, bEnd = false;
					for (const FDrawnText& T : Texts)
					{
						if (T.Text.Contains(TEXT("[LB]")))
						{
							PadFooterSeen = T.Text;
						}
						bConfirm |= T.Text == PadConfirm;
						bEnd |= T.Text == EndKey;
					}
					if (Tab == -1)
					{
						++PadPauseDraws;
						PadPauseConfirmSeen += bConfirm ? 1 : 0;
						PadPauseEndSeen += bEnd ? 1 : 0;
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
		if (PadPauseDraws == 0)
		{
			Skip(TEXT("pause a la manette : aucun rendu hors ecran : invites de la pause NON VERIFIEES"));
		}
		else
		{
			Note(FString::Printf(TEXT("pause a la manette (%d rendu(s)) : bouton A (valider) montre %d fois, touche FIN du clavier montree %d fois (attendu : a chaque rendu, jamais)"),
				PadPauseDraws, PadPauseConfirmSeen, PadPauseEndSeen), PadPauseConfirmSeen != PadPauseDraws || PadPauseEndSeen > 0);
		}
		for (const FString& E : PadReport.Examples)
		{
			Note(TEXT("  ") + E, true);
		}
		BRLoc::SetLanguage(TEXT("fr"));
		return true;
	});
	// Inventaire a la manette par de vraies touches (injectees dans le controleur, comme une manette branchee). v4.16 (relecture) :
	// chaque appui a sa propre verification : Before note l'etat avant l'appui ; Check, deux images plus tard (touche encore
	// enfoncee), dit si le resultat attendu est la et le decrit (Detail) ; une precondition absente y est notee NON VERIFIE
	// v4.17 (seconde relecture, C1 et C3) : bPad = false : appui du clavier (manette inactive, invites du clavier)
	auto AddPress = [this](bool bPad, const FString& What, const FKey& Key, TFunction<void()> Before, TFunction<bool(FString&)> Check)
	{
		const FString Dev = bPad ? TEXT("manette") : TEXT("clavier");
		Add(FString::Printf(TEXT("v4.16 visuel : %s, %s"), *Dev, *What), 0.f, [this, bPad, Dev, What, Key, Before, Check]()
		{
			ABRPlayerController* PC = GetPC();
			if (!PC)
			{
				Skip(FString::Printf(TEXT("%s, %s : controleur absent : NON VERIFIE"), *Dev, *What));
				return true;
			}
			if (BRTestV416Vis::Phase == 0)
			{
				BRTestV416Vis::Phase = 1;
				PC->bPadActive = bPad;
				if (Before)
				{
					Before();
				}
				PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.f));
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
				const bool bDown = PC->IsInputKeyDown(Key);
				FString Detail;
				const bool bOk = Check(Detail);
				if (!bOk && !bDown)
				{
					Skip(FString::Printf(TEXT("%s, %s : la touche injectee n'est pas enfoncee pour le controleur (injection sans effet ici : a verifier avec une vraie %s) ; %s"),
						*Dev, *What, bPad ? TEXT("manette") : TEXT("touche"), *Detail));
				}
				else
				{
					Note(FString::Printf(TEXT("%s, %s : touche enfoncee %s ; %s"), *Dev, *What, BRTestV416Vis::Oui(bDown), *Detail), !bOk);
				}
				PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.f));
				return false;
			}
			if (StepTime < 0.3f)
			{
				return false;
			}
			BRTestV416Vis::Phase = 0;
			return true;
		});
	};
	auto AddPadPress = [AddPress](const FString& What, const FKey& Key, TFunction<void()> Before, TFunction<bool(FString&)> Check)
	{
		AddPress(true, What, Key, MoveTemp(Before), MoveTemp(Check));
	};
	// Onglet attendu apres l'appui (et inventaire ouvert)
	auto TabIs = [this, HudOf](int32 Want)
	{
		return [this, HudOf, Want](FString& Detail)
		{
			ABRHUD* H = HudOf();
			ABRPlayerController* PC = GetPC();
			const int32 Now = H ? static_cast<int32>(H->Tab) : -1;
			Detail = FString::Printf(TEXT("onglet %d (attendu %d), inventaire %s"), Now, Want, PC && PC->IsInventoryOpen() ? TEXT("ouvert") : TEXT("ferme"));
			return H && PC && PC->IsInventoryOpen() && Now == Want;
		};
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
	AddPadPress(TEXT("RB : onglet suivant (CARNET)"), EKeys::Gamepad_RightShoulder, nullptr, TabIs(4));
	// v4.17 (interface) : RB (avant : croix droite) : dans le carnet, la croix gauche / droite choisit la colonne a faire defiler
	// (ABRHUD::PadScrollFocus, branche par le controleur)
	AddPadPress(TEXT("RB : onglet suivant (JOURNAL)"), EKeys::Gamepad_RightShoulder, nullptr, TabIs(1));
	AddPadPress(TEXT("LB : onglet precedent (CARNET)"), EKeys::Gamepad_LeftShoulder, nullptr, TabIs(4));
	AddPadPress(TEXT("Y : aide du carnet"), EKeys::Gamepad_FaceButton_Top, [this, HudOf]()
	{
		if (ABRHUD* H = HudOf())
		{
			PressInt = H->LastNotebookHintLevel;
		}
	}, [this, HudOf, ShotNamed](FString& Detail)
	{
		ABRHUD* H = HudOf();
		if (!H)
		{
			Detail = TEXT("HUD absent");
			return false;
		}
		ShotNamed(TEXT("v416_manette_carnet_aide"));
		Detail = FString::Printf(TEXT("onglet %d, aide du carnet %d -> %d"), static_cast<int32>(H->Tab), PressInt, H->LastNotebookHintLevel);
		if (H->NotebookHintStep < 0)
		{
			Skip(TEXT("manette, Y : aucune etape en cours dans le carnet : aide a la manette NON VERIFIEE"));
			return static_cast<int32>(H->Tab) == 4;
		}
		return static_cast<int32>(H->Tab) == 4 && H->LastNotebookHintLevel > PressInt;
	});
	// v4.16 (relecture) : le defilement est verifie (avant : seulement l'onglet) ; il faut du contenu a faire defiler
	AddPadPress(TEXT("croix bas : defilement du carnet"), EKeys::Gamepad_DPad_Down, [this, HudOf]()
	{
		if (ABRHUD* H = HudOf())
		{
			PressFloat = H->NotebookLeftScroll;
			PressRoom = H->LastNotebookLeftRoom;
		}
	}, [this, HudOf](FString& Detail)
	{
		ABRHUD* H = HudOf();
		if (!H)
		{
			Detail = TEXT("HUD absent");
			return false;
		}
		Detail = FString::Printf(TEXT("onglet %d, defilement du carnet %.0f -> %.0f (reste a defiler avant l'appui : %.0f)"), static_cast<int32>(H->Tab), PressFloat,
			H->NotebookLeftScroll, PressRoom);
		if (PressRoom < 1.f)
		{
			Skip(TEXT("manette, croix bas : colonne du carnet plus courte que sa zone (ou deja en bas) : defilement a la manette NON VERIFIE"));
			return static_cast<int32>(H->Tab) == 4;
		}
		return static_cast<int32>(H->Tab) == 4 && H->NotebookLeftScroll > PressFloat + 0.5f;
	});
	// v4.17 (interface) : LB (avant : croix gauche), pour la meme raison
	AddPadPress(TEXT("LB : onglet precedent (PERSONNAGE)"), EKeys::Gamepad_LeftShoulder, nullptr, TabIs(0));
	// v4.16 (relecture) : objets du sac a la manette : une frontale posee dans la 2e case du sac (tete vide), curseur sur la 1re ;
	// croix droite (curseur), X (inspecter), B (fermer l'inspection), A (equiper), B (fermer l'inventaire). Sac retabli ensuite
	Add(TEXT("v4.16 visuel : manette, objets du sac (preparation)"), 0.3f, [this, HudOf]()
	{
		ABRCharacter* C = GetPlayer();
		ABRHUD* H = HudOf();
		FBRItemSlot* S1 = C ? C->GetSlot(EBRSlotGroup::Storage, 1) : nullptr;
		FBRItemSlot* Head = C ? C->GetSlot(EBRSlotGroup::Equipment, static_cast<int32>(EBREquipSlot::Head)) : nullptr;
		if (!H || !S1 || !Head)
		{
			Skip(TEXT("manette, objets du sac : joueur, HUD ou cases absents : utilisation des objets du sac a la manette NON VERIFIEE"));
			return true;
		}
		if (!bInvSaved)
		{
			SavedStorage1 = *S1;
			SavedHead = *Head;
			bInvSaved = true;
		}
		*S1 = FBRItemSlot{ EBRItem::Headlamp, 1 };
		Head->Clear();
		C->OnEquipmentChanged();
		H->PadCursor.Group = EBRSlotGroup::Storage;
		H->PadCursor.Index = 0;
		H->Inspecting = ABRHUD::FSlotRef();
		return true;
	});
	auto CursorOn = [this, HudOf](EBRSlotGroup WantGroup, int32 WantIndex)
	{
		return [this, HudOf, WantGroup, WantIndex](FString& Detail)
		{
			ABRHUD* H = HudOf();
			if (!H)
			{
				Detail = TEXT("HUD absent");
				return false;
			}
			Detail = FString::Printf(TEXT("curseur sur la case %d du groupe %d (attendu %d du groupe %d), onglet %d"), H->PadCursor.Index, static_cast<int32>(H->PadCursor.Group),
				WantIndex, static_cast<int32>(WantGroup), static_cast<int32>(H->Tab));
			return static_cast<int32>(H->Tab) == 0 && H->PadCursor.Group == WantGroup && H->PadCursor.Index == WantIndex;
		};
	};
	AddPadPress(TEXT("croix droite dans PERSONNAGE : curseur sur la 2e case du sac"), EKeys::Gamepad_DPad_Right, nullptr, CursorOn(EBRSlotGroup::Storage, 1));
	AddPadPress(TEXT("X : inspecter la frontale"), EKeys::Gamepad_FaceButton_Left, nullptr, [this, HudOf, ShotNamed](FString& Detail)
	{
		ABRHUD* H = HudOf();
		const bool bOk = H && H->Inspecting.IsValid() && H->Inspecting.Group == EBRSlotGroup::Storage && H->Inspecting.Index == 1;
		Detail = FString::Printf(TEXT("inspection %s"), H && H->Inspecting.IsValid() ? *FString::Printf(TEXT("de la case %d du groupe %d"), H->Inspecting.Index,
			static_cast<int32>(H->Inspecting.Group)) : TEXT("fermee"));
		ShotNamed(TEXT("v416_manette_inspecter"));
		return bOk;
	});
	AddPadPress(TEXT("B : fermer l'inspection (inventaire ouvert)"), EKeys::Gamepad_FaceButton_Right, nullptr, [this, HudOf](FString& Detail)
	{
		ABRHUD* H = HudOf();
		ABRPlayerController* PC = GetPC();
		const bool bOpen = PC && PC->IsInventoryOpen();
		Detail = FString::Printf(TEXT("inspection %s, inventaire %s"), H && H->Inspecting.IsValid() ? TEXT("ouverte") : TEXT("fermee"), bOpen ? TEXT("ouvert") : TEXT("ferme"));
		return H && !H->Inspecting.IsValid() && bOpen;
	});
	AddPadPress(TEXT("A : equiper la frontale"), EKeys::Gamepad_FaceButton_Bottom, nullptr, [this, ShotNamed](FString& Detail)
	{
		ABRCharacter* C = GetPlayer();
		const FBRItemSlot* S1 = C ? C->GetSlot(EBRSlotGroup::Storage, 1) : nullptr;
		const bool bOk = C && C->GetEquipped(EBREquipSlot::Head) == EBRItem::Headlamp && S1 && S1->IsEmpty();
		Detail = FString::Printf(TEXT("tete : %s, 2e case du sac %s"), C && C->GetEquipped(EBREquipSlot::Head) == EBRItem::Headlamp ? TEXT("frontale") : TEXT("autre"),
			S1 && S1->IsEmpty() ? TEXT("vide") : TEXT("occupee"));
		ShotNamed(TEXT("v416_manette_equipe"));
		return bOk;
	});
	// v4.17 (seconde relecture, C1 et C3) : meme sac au clavier seul : frontale remise dans la 2e case (tete vide), curseur
	// dessus mais cache (souris). Entree ne fait rien tant que le curseur n'est pas montre (avant : la frontale etait equipee
	// sans que le joueur voie le curseur) ; les fleches le montrent ; l'infobulle nomme alors Entree et la touche LAMPE (avant :
	// les boutons A et Y de la manette) ; Entree equipe
	Add(TEXT("v4.17 visuel : clavier, objets du sac (preparation)"), 0.2f, [this, HudOf]()
	{
		ABRCharacter* C = GetPlayer();
		ABRHUD* H = HudOf();
		ABRPlayerController* PC = GetPC();
		FBRItemSlot* S1 = C ? C->GetSlot(EBRSlotGroup::Storage, 1) : nullptr;
		FBRItemSlot* Head = C ? C->GetSlot(EBRSlotGroup::Equipment, static_cast<int32>(EBREquipSlot::Head)) : nullptr;
		if (!H || !S1 || !Head || !PC || !PC->IsInventoryOpen())
		{
			Skip(TEXT("clavier, objets du sac : joueur, HUD, cases ou inventaire absents : inventaire au clavier NON VERIFIE"));
			return true;
		}
		*S1 = FBRItemSlot{ EBRItem::Headlamp, 1 };
		Head->Clear();
		C->OnEquipmentChanged();
		H->PadCursor.Group = EBRSlotGroup::Storage;
		H->PadCursor.Index = 1;
		H->Inspecting = ABRHUD::FSlotRef();
		PC->bPadActive = false;
		BRKeys::SetKeyboardCursor(false);
		return true;
	});
	auto HeadlampState = [this](FString& Detail)
	{
		ABRCharacter* C = GetPlayer();
		const FBRItemSlot* S1 = C ? C->GetSlot(EBRSlotGroup::Storage, 1) : nullptr;
		const bool bWorn = C && C->GetEquipped(EBREquipSlot::Head) == EBRItem::Headlamp;
		const bool bInBag = S1 && !S1->IsEmpty() && S1->Item == EBRItem::Headlamp;
		Detail = FString::Printf(TEXT("tete : %s, 2e case du sac : %s, curseur du clavier %s"), bWorn ? TEXT("frontale") : TEXT("autre ou vide"),
			bInBag ? TEXT("frontale") : TEXT("autre ou vide"), BRKeys::UsesKeyboardCursor() ? TEXT("montre") : TEXT("cache"));
		return bWorn ? 1 : (bInBag ? 0 : -1);
	};
	AddPress(false, TEXT("Entree, curseur cache : rien ne change (C1)"), EKeys::Enter, nullptr, [this, HeadlampState](FString& Detail)
	{
		ABRPlayerController* PC = GetPC();
		const bool bDown = PC && PC->IsInputKeyDown(EKeys::Enter);
		const bool bOpen = PC && PC->IsInventoryOpen();
		const int32 State = HeadlampState(Detail);
		Detail += FString::Printf(TEXT(", inventaire %s"), bOpen ? TEXT("ouvert") : TEXT("ferme"));
		// Touche non recue : rien ne prouve l'absence d'effet (NON VERIFIE par AddPress)
		return bDown && bOpen && State == 0 && !BRKeys::UsesKeyboardCursor();
	});
	AddPress(false, TEXT("fleche gauche : curseur montre, 1re case du sac"), EKeys::Left, nullptr, [this, HudOf](FString& Detail)
	{
		ABRHUD* H = HudOf();
		Detail = H ? FString::Printf(TEXT("curseur sur la case %d du groupe %d (attendu 0 du groupe %d), curseur du clavier %s"), H->PadCursor.Index,
			static_cast<int32>(H->PadCursor.Group), static_cast<int32>(EBRSlotGroup::Storage), BRKeys::UsesKeyboardCursor() ? TEXT("montre") : TEXT("cache"))
			: FString(TEXT("HUD absent"));
		return H && BRKeys::UsesKeyboardCursor() && H->PadCursor.Group == EBRSlotGroup::Storage && H->PadCursor.Index == 0;
	});
	AddPress(false, TEXT("fleche droite : infobulle de la frontale aux touches du clavier (C3)"), EKeys::Right, nullptr, [this, HudOf, Offscreen, ShotNamed](FString& Detail)
	{
		ABRHUD* H = HudOf();
		if (!H || H->PadCursor.Group != EBRSlotGroup::Storage || H->PadCursor.Index != 1 || !BRKeys::UsesKeyboardCursor())
		{
			Detail = TEXT("curseur du clavier absent ou pas sur la 2e case du sac");
			return false;
		}
		TArray<FDrawnText> Texts;
		if (!Offscreen(1920, 1080, FString(), Texts))
		{
			Skip(TEXT("clavier, infobulle : aucun rendu hors ecran : noms des touches NON VERIFIES"));
			return true;
		}
		ShotNamed(TEXT("v417_clavier_infobulle"));
		const FString KeyUse = TEXT("[") + BRKeys::KeyName(EKeys::Enter) + TEXT("]");
		const FString KeyCarry = TEXT("[") + BRKeys::Primary(EBRAction::Flashlight) + TEXT("]");
		const FString PadUse = TEXT("[") + BRKeys::GamepadName(EKeys::Gamepad_FaceButton_Bottom) + TEXT("]");
		FString Tip;
		bool bPad = false;
		for (const FDrawnText& T : Texts)
		{
			if (T.Text.StartsWith(KeyUse) && T.Text.Contains(KeyCarry))
			{
				Tip = T.Text;
			}
			bPad |= T.Text.StartsWith(PadUse);
		}
		Detail = FString::Printf(TEXT("infobulle \"%s\" (attendu : %s puis %s), invite de la manette %s"), *Tip, *KeyUse, *KeyCarry, bPad ? TEXT("PRESENTE") : TEXT("absente"));
		return !Tip.IsEmpty() && !bPad;
	});
	AddPress(false, TEXT("Entree, curseur montre : equiper la frontale"), EKeys::Enter, nullptr, [this, HeadlampState](FString& Detail)
	{
		const bool bOk = HeadlampState(Detail) == 1;
		BRKeys::SetKeyboardCursor(false); // la suite du banc revient a la manette
		return bOk;
	});
	AddPadPress(TEXT("B : fermer l'inventaire"), EKeys::Gamepad_FaceButton_Right, nullptr, [this](FString& Detail)
	{
		ABRPlayerController* PC = GetPC();
		Detail = FString::Printf(TEXT("inventaire %s"), PC && PC->IsInventoryOpen() ? TEXT("ouvert") : TEXT("ferme"));
		return PC && !PC->IsInventoryOpen();
	});
	Add(TEXT("v4.16 visuel : manette, sac retabli"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		FBRItemSlot* S1 = C ? C->GetSlot(EBRSlotGroup::Storage, 1) : nullptr;
		FBRItemSlot* Head = C ? C->GetSlot(EBRSlotGroup::Equipment, static_cast<int32>(EBREquipSlot::Head)) : nullptr;
		if (bInvSaved && S1 && Head)
		{
			*S1 = SavedStorage1;
			*Head = SavedHead;
			C->OnEquipmentChanged();
		}
		bInvSaved = false;
		return true;
	});
	// v4.17 (interface, C7) : deplacer un objet a la manette. ABRHUD::PadCarry est appele ici directement (le controleur branche Y
	// dessus) : eau d'amande de la 1re case du sac vers la 4e poche (capture pendant le port), puis retour. B pendant un port
	// passe par une vraie touche : B est deja branche sur ABRHUD::PadBack, qui annule le port sans fermer l'inventaire
	Add(TEXT("v4.17 visuel : manette, deplacer un objet (preparation)"), 0.4f, [this, HudOf, ShowScreen]()
	{
		ABRCharacter* C = GetPlayer();
		ABRHUD* H = HudOf();
		FBRItemSlot* S0 = C ? C->GetSlot(EBRSlotGroup::Storage, 0) : nullptr;
		FBRItemSlot* P3 = C ? C->GetSlot(EBRSlotGroup::Pockets, 3) : nullptr;
		if (!H || !S0 || !P3)
		{
			Skip(TEXT("manette, deplacer un objet : joueur, HUD ou cases absents : deplacement a la manette NON VERIFIE"));
			return true;
		}
		if (!BRTestV416Vis::bCarrySaved)
		{
			BRTestV416Vis::SavedCarryStorage = *S0;
			BRTestV416Vis::SavedCarryPocket = *P3;
			BRTestV416Vis::bCarrySaved = true;
		}
		*S0 = FBRItemSlot{ EBRItem::AlmondWater, 2 };
		P3->Clear();
		ShowScreen(0, false);
		if (ABRPlayerController* PC = GetPC())
		{
			PC->bPadActive = true;
		}
		H->PadCancelCarry();
		H->PadCursor.Group = EBRSlotGroup::Storage;
		H->PadCursor.Index = 0;
		return true;
	});
	Add(TEXT("v4.17 visuel : manette, Y : prendre l'eau d'amande (PadCarry)"), 0.25f, [this, HudOf]()
	{
		ABRHUD* H = HudOf();
		if (!H || !BRTestV416Vis::bCarrySaved)
		{
			return true; // deja note NON VERIFIE a la preparation
		}
		const bool bAct = H->PadCarry();
		const bool bOk = bAct && H->IsPadCarrying() && H->Dragging.Group == EBRSlotGroup::Storage && H->Dragging.Index == 0;
		Note(FString::Printf(TEXT("manette, deplacer : Y sur la 1re case du sac : action %s, objet porte %s (case %d du groupe %d)"), BRTestV416Vis::Oui(bAct),
			BRTestV416Vis::Oui(H->IsPadCarrying()), H->Dragging.Index, static_cast<int32>(H->Dragging.Group)), !bOk);
		// Le curseur va sur la 4e poche : l'objet porte le suit (capture a l'etape suivante)
		H->PadCursor.Group = EBRSlotGroup::Pockets;
		H->PadCursor.Index = 3;
		return true;
	});
	Add(TEXT("v4.17 visuel : manette, capture pendant le port"), 0.3f, [this, HudOf, ShotNamed]()
	{
		ABRHUD* H = HudOf();
		if (!H || !BRTestV416Vis::bCarrySaved)
		{
			return true;
		}
		Note(FString::Printf(TEXT("manette, deplacer : pendant le port, curseur sur la case %d du groupe %d, objet porte %s"), H->PadCursor.Index,
			static_cast<int32>(H->PadCursor.Group), BRTestV416Vis::Oui(H->IsPadCarrying())), !H->IsPadCarrying());
		ShotNamed(TEXT("v417_manette_port"));
		return true;
	});
	Add(TEXT("v4.17 visuel : manette, Y : poser dans la 4e poche"), 0.25f, [this, HudOf]()
	{
		ABRHUD* H = HudOf();
		ABRCharacter* C = GetPlayer();
		if (!H || !C || !BRTestV416Vis::bCarrySaved)
		{
			return true;
		}
		const bool bAct = H->PadCarry();
		const FBRItemSlot* S0 = C->GetSlot(EBRSlotGroup::Storage, 0);
		const FBRItemSlot* P3 = C->GetSlot(EBRSlotGroup::Pockets, 3);
		const bool bInPocket = P3 && P3->Item == EBRItem::AlmondWater && P3->Count == 2;
		const bool bOk = bAct && !H->IsPadCarrying() && bInPocket && S0 && S0->IsEmpty();
		Note(FString::Printf(TEXT("manette, deplacer : Y sur la 4e poche : poche %s (x%d), 1re case du sac %s, port termine %s"), bInPocket ? TEXT("eau d'amande") : TEXT("autre"),
			P3 ? P3->Count : 0, S0 && S0->IsEmpty() ? TEXT("vide") : TEXT("occupee"), BRTestV416Vis::Oui(!H->IsPadCarrying())), !bOk);
		return true;
	});
	Add(TEXT("v4.17 visuel : manette, retour de la poche vers le sac (PadCarry deux fois)"), 0.25f, [this, HudOf]()
	{
		ABRHUD* H = HudOf();
		ABRCharacter* C = GetPlayer();
		if (!H || !C || !BRTestV416Vis::bCarrySaved)
		{
			return true;
		}
		H->PadCursor.Group = EBRSlotGroup::Pockets;
		H->PadCursor.Index = 3;
		const bool bTake = H->PadCarry();
		H->PadCursor.Group = EBRSlotGroup::Storage;
		H->PadCursor.Index = 0;
		const bool bDrop = H->PadCarry();
		const FBRItemSlot* S0 = C->GetSlot(EBRSlotGroup::Storage, 0);
		const FBRItemSlot* P3 = C->GetSlot(EBRSlotGroup::Pockets, 3);
		const bool bBack = S0 && S0->Item == EBRItem::AlmondWater && S0->Count == 2 && P3 && P3->IsEmpty();
		Note(FString::Printf(TEXT("manette, deplacer : retour : prise %s, pose %s, eau d'amande dans la 1re case du sac %s, 4e poche vide %s"), BRTestV416Vis::Oui(bTake),
			BRTestV416Vis::Oui(bDrop), BRTestV416Vis::Oui(S0 && S0->Item == EBRItem::AlmondWater), BRTestV416Vis::Oui(P3 && P3->IsEmpty())), !(bTake && bDrop && bBack));
		return true;
	});
	Add(TEXT("v4.17 visuel : manette, port a annuler (preparation)"), 0.2f, [this, HudOf]()
	{
		ABRHUD* H = HudOf();
		if (H && BRTestV416Vis::bCarrySaved)
		{
			H->PadCursor.Group = EBRSlotGroup::Storage;
			H->PadCursor.Index = 0;
			H->PadCarry();
		}
		return true;
	});
	AddPadPress(TEXT("B pendant un port : annule sans fermer l'inventaire"), EKeys::Gamepad_FaceButton_Right, nullptr, [this, HudOf](FString& Detail)
	{
		ABRHUD* H = HudOf();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!BRTestV416Vis::bCarrySaved)
		{
			Detail = TEXT("preparation impossible (deja notee)");
			return true;
		}
		const FBRItemSlot* S0 = C ? C->GetSlot(EBRSlotGroup::Storage, 0) : nullptr;
		const bool bOpen = PC && PC->IsInventoryOpen();
		Detail = FString::Printf(TEXT("port %s, inventaire %s, 1re case du sac %s"), H && H->IsPadCarrying() ? TEXT("en cours") : TEXT("annule"), bOpen ? TEXT("ouvert") : TEXT("ferme"),
			S0 && S0->Item == EBRItem::AlmondWater ? TEXT("eau d'amande") : TEXT("autre"));
		return H && !H->IsPadCarrying() && bOpen && S0 && S0->Item == EBRItem::AlmondWater;
	});
	Add(TEXT("v4.17 visuel : manette, sac et poche retablis"), 0.f, [this, HudOf]()
	{
		if (ABRHUD* H = HudOf())
		{
			H->PadCancelCarry();
		}
		ABRCharacter* C = GetPlayer();
		FBRItemSlot* S0 = C ? C->GetSlot(EBRSlotGroup::Storage, 0) : nullptr;
		FBRItemSlot* P3 = C ? C->GetSlot(EBRSlotGroup::Pockets, 3) : nullptr;
		if (BRTestV416Vis::bCarrySaved && S0 && P3)
		{
			*S0 = BRTestV416Vis::SavedCarryStorage;
			*P3 = BRTestV416Vis::SavedCarryPocket;
		}
		BRTestV416Vis::bCarrySaved = false;
		return true;
	});
	// v4.17 (interface, C6) : PARAMETRES a la manette. ABRHUD::PadSettingsMove et PadSettingsAdjust sont appeles directement (le
	// controleur branche la croix dessus) : curseur sur LUMINOSITE (VIDEO), un cran (applique au dessin suivant, comme un clic),
	// valeur relue, capture, puis retour d'un cran
	Add(TEXT("v4.17 visuel : manette, PARAMETRES > VIDEO (preparation)"), 0.4f, [this, HudOf, ShowScreen]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = HudOf();
		BRTestV416Vis::BrightRow = INDEX_NONE;
		if (!PC || !H)
		{
			Skip(TEXT("manette, PARAMETRES : controleur ou HUD absent : reglage a la manette NON VERIFIE"));
			return true;
		}
		ShowScreen(2, false); // VIDEO
		PC->bPadActive = true;
		H->SettingsPadRow = INDEX_NONE;
		const FString Want = BR_STR(NSLOCTEXT("BR", "Menu.Luminosite", "LUMINOSIT\u00c9"));
		for (int32 i = 0; i < PC->GetSettingsCount(); ++i)
		{
			if (PC->GetSettingCategory(i) == 1 && PC->GetSettingLabel(i) == Want)
			{
				BRTestV416Vis::BrightRow = i;
			}
		}
		if (BRTestV416Vis::BrightRow == INDEX_NONE)
		{
			Skip(TEXT("manette, PARAMETRES : ligne LUMINOSITE introuvable dans VIDEO : reglage a la manette NON VERIFIE"));
		}
		return true;
	});
	Add(TEXT("v4.17 visuel : manette, croix bas jusqu'a LUMINOSITE (PadSettingsMove)"), 0.3f, [this, HudOf]()
	{
		ABRHUD* H = HudOf();
		ABRPlayerController* PC = GetPC();
		if (!H || !PC || BRTestV416Vis::BrightRow == INDEX_NONE)
		{
			return true;
		}
		int32 Moves = 0;
		bool bAct = true;
		while (bAct && H->GetPadSetting() != BRTestV416Vis::BrightRow && Moves < PC->GetSettingsCount())
		{
			bAct = H->PadSettingsMove(1);
			++Moves;
		}
		const bool bOk = bAct && H->GetPadSetting() == BRTestV416Vis::BrightRow;
		Note(FString::Printf(TEXT("manette, PARAMETRES : %d appui(s) de la croix bas, ligne choisie %d (LUMINOSITE : %d)"), Moves, H->GetPadSetting(), BRTestV416Vis::BrightRow), !bOk);
		BRTestV416Vis::BrightBefore = FBRSettings::Get().Brightness;
		BRTestV416Vis::BrightShownBefore = PC->GetSettingValue(BRTestV416Vis::BrightRow);
		BRTestV416Vis::BrightDir = BRTestV416Vis::BrightBefore < 1.45f ? 1 : -1; // au maximum : le premier cran descend
		return true;
	});
	Add(TEXT("v4.17 visuel : manette, croix droite : LUMINOSITE d'un cran (PadSettingsAdjust)"), 0.3f, [this, HudOf]()
	{
		ABRHUD* H = HudOf();
		if (!H || BRTestV416Vis::BrightRow == INDEX_NONE || H->GetPadSetting() != BRTestV416Vis::BrightRow)
		{
			return true;
		}
		const bool bAct = H->PadSettingsAdjust(BRTestV416Vis::BrightDir);
		Note(FString::Printf(TEXT("manette, PARAMETRES : cran %+d demande sur LUMINOSITE : %s"), BRTestV416Vis::BrightDir, BRTestV416Vis::Oui(bAct)), !bAct);
		return true;
	});
	Add(TEXT("v4.17 visuel : manette, LUMINOSITE relue et capture"), 0.3f, [this, HudOf, ShotNamed]()
	{
		ABRHUD* H = HudOf();
		ABRPlayerController* PC = GetPC();
		if (!H || !PC || BRTestV416Vis::BrightRow == INDEX_NONE)
		{
			return true;
		}
		const float After = FBRSettings::Get().Brightness;
		const bool bChanged = FMath::Abs(After - (BRTestV416Vis::BrightBefore + 0.1f * BRTestV416Vis::BrightDir)) < 0.01f;
		const bool bHint = H->HintSetting == BRTestV416Vis::BrightRow;
		Note(FString::Printf(TEXT("manette, PARAMETRES : LUMINOSITE %.1f -> %.1f (affiche \"%s\" -> \"%s\") ; aide de la ligne choisie montree %s"), BRTestV416Vis::BrightBefore, After,
			*BRTestV416Vis::BrightShownBefore, *PC->GetSettingValue(BRTestV416Vis::BrightRow), BRTestV416Vis::Oui(bHint)), !bChanged || !bHint);
		ShotNamed(TEXT("v417_manette_parametres"));
		return true;
	});
	Add(TEXT("v4.17 visuel : manette, croix gauche : LUMINOSITE revient d'un cran"), 0.3f, [this, HudOf]()
	{
		ABRHUD* H = HudOf();
		if (H && BRTestV416Vis::BrightRow != INDEX_NONE)
		{
			H->PadSettingsAdjust(-BRTestV416Vis::BrightDir);
		}
		return true;
	});
	Add(TEXT("v4.17 visuel : manette, LUMINOSITE revenue"), 0.f, [this, HudOf, CloseScreens]()
	{
		ABRHUD* H = HudOf();
		ABRPlayerController* PC = GetPC();
		if (H && PC && BRTestV416Vis::BrightRow != INDEX_NONE)
		{
			const float Back = FBRSettings::Get().Brightness;
			const bool bBack = FMath::Abs(Back - BRTestV416Vis::BrightBefore) < 0.01f;
			Note(FString::Printf(TEXT("manette, PARAMETRES : retour d'un cran : LUMINOSITE %.1f (avant l'essai %.1f, affiche \"%s\")"), Back, BRTestV416Vis::BrightBefore,
				*PC->GetSettingValue(BRTestV416Vis::BrightRow)), !bBack);
			if (!bBack)
			{
				FBRSettings::Get().Brightness = BRTestV416Vis::BrightBefore;
				PC->ApplySettings();
			}
			H->SettingsPadRow = INDEX_NONE;
		}
		CloseScreens();
		return true;
	});
	// v4.16 (relecture) : camescope reste dans le sac : sa vision nocturne a la manette (croix haut), deux fois (allumee, eteinte)
	Add(TEXT("v4.16 visuel : manette, camescope dans le sac (preparation)"), 0.3f, [this, CloseScreens]()
	{
		CloseScreens();
		ABRCharacter* C = GetPlayer();
		if (C)
		{
			SavedBattery = C->Battery;
			C->Battery = 100.f;
			PressInt = C->IsNightVision() ? 1 : 0;
		}
		return true;
	});
	for (int32 Turn = 0; Turn < 2; ++Turn)
	{
		AddPadPress(Turn == 0 ? FString(TEXT("croix haut : vision nocturne du camescope reste dans le sac")) : FString(TEXT("croix haut : vision nocturne coupee")),
			EKeys::Gamepad_DPad_Up, nullptr, [this, Turn](FString& Detail)
		{
			ABRCharacter* C = GetPlayer();
			if (!C || !C->HasCamcorder())
			{
				Detail = TEXT("pas de camescope sur le joueur");
				Skip(TEXT("manette : camescope absent du sac : vision nocturne depuis le sac NON VERIFIEE"));
				return true;
			}
			const bool bWant = Turn == 0 ? PressInt == 0 : PressInt != 0;
			Detail = FString::Printf(TEXT("camescope dans le sac oui, vision nocturne %s (attendu %s)"), Oui(C->IsNightVision()), Oui(bWant));
			return C->IsNightVision() == bWant;
		});
	}
	Add(TEXT("v4.16 visuel : manette, camescope retabli"), 0.f, [this]()
	{
		if (ABRCharacter* C = GetPlayer())
		{
			if (C->IsNightVision() != (PressInt != 0))
			{
				C->ToggleNightVision();
			}
			C->Battery = SavedBattery;
		}
		return true;
	});
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
			PC->SetPauseCursor(0);
		}
		Note(FString::Printf(TEXT("pause a la manette (fenetre) : invites de la manette %s"), Oui(BRKeys::UsesGamepadPrompts())), !BRKeys::UsesGamepadPrompts());
		ShotNamed(TEXT("v416_manette_pause_fenetre"));
		return true;
	});
	// v4.16 (relecture) : cartes de la pause a la manette : croix bas (PARAMETRES), A (ouvre les parametres), B (retour a la pause)
	AddPadPress(TEXT("pause, croix bas : carte PARAMETRES"), EKeys::Gamepad_DPad_Down, [this]()
	{
		if (ABRPlayerController* PC = GetPC())
		{
			PC->SetPauseCursor(0);
		}
	}, [this](FString& Detail)
	{
		ABRPlayerController* PC = GetPC();
		Detail = FString::Printf(TEXT("pause %s, carte %d (attendu 1)"), PC && PC->IsPauseMenuOpen() ? TEXT("ouverte") : TEXT("fermee"), PC ? PC->GetPauseCursor() : -1);
		return PC && PC->IsPauseMenuOpen() && PC->GetPauseCursor() == 1;
	});
	AddPadPress(TEXT("pause, A : ouvrir PARAMETRES"), EKeys::Gamepad_FaceButton_Bottom, nullptr, TabIs(2));
	// v4.16 (relecture 2) : dans l'inventaire ouvert depuis la pause (jeu arrete en solo), les epaules changent d'onglet et B
	// ramene a la pause. Avant : IA_Reload, IA_Drink et IA_Crouch inactives pendant la pause (B sans effet : "inventaire
	// ouvert, pause ouverte")
	auto PausedTabIs = [this, HudOf](int32 Want)
	{
		return [this, HudOf, Want](FString& Detail)
		{
			ABRHUD* H = HudOf();
			ABRPlayerController* PC = GetPC();
			const int32 Now = H ? static_cast<int32>(H->Tab) : -1;
			Detail = FString::Printf(TEXT("onglet %d (attendu %d), inventaire %s, pause %s"), Now, Want, PC && PC->IsInventoryOpen() ? TEXT("ouvert") : TEXT("ferme"),
				PC && PC->IsPauseMenuOpen() ? TEXT("ouverte") : TEXT("fermee"));
			return H && PC && PC->IsInventoryOpen() && PC->IsPauseMenuOpen() && Now == Want;
		};
	};
	AddPadPress(TEXT("pause, RB : onglet suivant (TOUCHES)"), EKeys::Gamepad_RightShoulder, nullptr, PausedTabIs(3));
	AddPadPress(TEXT("pause, LB : onglet precedent (PARAMETRES)"), EKeys::Gamepad_LeftShoulder, nullptr, PausedTabIs(2));
	AddPadPress(TEXT("pause, B : retour a la pause"), EKeys::Gamepad_FaceButton_Right, nullptr, [this](FString& Detail)
	{
		ABRPlayerController* PC = GetPC();
		Detail = FString::Printf(TEXT("inventaire %s, pause %s"), PC && PC->IsInventoryOpen() ? TEXT("ouvert") : TEXT("ferme"), PC && PC->IsPauseMenuOpen() ? TEXT("ouverte") : TEXT("fermee"));
		return PC && !PC->IsInventoryOpen() && PC->IsPauseMenuOpen();
	});
	Add(TEXT("v4.16 visuel : clavier, capture de la pause"), 0.4f, [this, ShowScreen]()
	{
		ShowScreen(-1, false);
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
			// v4.16 (relecture) : Request ne dit pas si la fenetre a change (vrai des que l'affichage est modifiable) : la taille
			// obtenue est comparee a celle-ci a la capture
			WantSizes[Wc.Kind] = Want;
			BRDisplay::Request(2, Want);
			BRDisplay::Confirm();
			BRTestV416Vis::bDisplayTouched = true;
			Note(FString::Printf(TEXT("fenetre %s : %d x %d demandee (zone utile %d x %d)"), Wc.Name, Want.X, Want.Y, M.WorkSize.X, M.WorkSize.Y));
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
			Add(FString::Printf(TEXT("v4.16 visuel : fenetre %s, capture %d"), Wc.Name, Tab), 0.2f, [this, Wc, Tab, HudOf, Grab, ShotNamed]()
			{
				if (!BRDisplay::CanChange())
				{
					if (ABRHUD* H0 = HudOf())
					{
						H0->bTestRecordTexts = false;
					}
					return true; // deja note NON VERIFIE a la demande
				}
				ABRHUD* H = HudOf();
				const BRDisplay::FMode Eff = BRDisplay::Effective();
				FIntPoint View(0, 0);
				if (ABRPlayerController* PC = GetPC())
				{
					PC->GetViewportSize(View.X, View.Y);
				}
				// v4.16 (relecture) : la fenetre obtenue est-elle celle demandee (fenetre, a 2 px pres) ? Avant : seulement notee
				const FIntPoint Want = WantSizes[Wc.Kind];
				const bool bSize = Eff.Window == 2 && FMath::Abs(Eff.Resolution.X - Want.X) <= 2 && FMath::Abs(Eff.Resolution.Y - Want.Y) <= 2
					&& FMath::Abs(View.X - Want.X) <= 2 && FMath::Abs(View.Y - Want.Y) <= 2;
				if (H)
				{
					TArray<FDrawnText> Texts;
					Grab(H, Texts);
					H->bTestRecordTexts = false;
					CheckTexts(Texts, static_cast<float>(View.X), static_cast<float>(View.Y), FString::Printf(TEXT("fenetre %s (%d x %d) onglet %d"), Wc.Name, View.X, View.Y, Tab),
						RealReport);
				}
				if (Tab == 0)
				{
					Note(FString::Printf(TEXT("fenetre %s : demandee %d x %d, obtenue %d x %d (mode %d), image %d x %d : %s"), Wc.Name, Want.X, Want.Y, Eff.Resolution.X,
						Eff.Resolution.Y, Eff.Window, View.X, View.Y, bSize ? TEXT("taille obtenue") : TEXT("TAILLE NON OBTENUE (textes controles a la taille obtenue)")), !bSize);
				}
				ShotNamed(FString::Printf(TEXT("v416_fenetre_%s_%s"), Wc.Name, Tab == 0 ? TEXT("personnage") : TEXT("carnet")));
				return true;
			});
			if (Tab == 4)
			{
				// v4.17 (K14) : l'inventaire se ferme a l'etape suivante : la capture, demandee a l'etape d'avant et prise a la fin
				// de l'image suivante, le montre encore (avant : ferme dans la meme etape, les captures du carnet etaient vides)
				Add(FString::Printf(TEXT("v4.17 visuel : fenetre %s, inventaire ferme apres la capture"), Wc.Name), 0.f, [CloseScreens]()
				{
					CloseScreens();
					return true;
				});
			}
		}
		if (Wc.Kind == 0)
		{
			// v4.17 (interface, C8) : carnet long a 1280 x 720 et taille d'interface 1,25 (aide et rappel) : il reste du texte a
			// faire defiler (HadScrollRoom) et un appui (ABRHUD::PadScrollLines(1), ce que le controleur appelle pour la croix bas)
			// fait descendre la colonne de gauche d'une ligne (PadScrollLine), ou de ce qui restait
			Add(TEXT("v4.17 visuel : carnet long (taille d'interface 1,25, aide et rappel)"), 0.6f, [this, HudOf, ShowScreen]()
			{
				ABRHUD* H = HudOf();
				if (!H)
				{
					Skip(TEXT("carnet long : HUD absent : une ligne par appui NON VERIFIE"));
					return true;
				}
				FBRSettings::Get().UiScale = 1.25f;
				// v4.17 (correction, QA visuelle : le carnet long n'etait jamais long) : tous les indices et observations du niveau
				// lus, carnet en allemand. Essai seulement : l'etat de la mission est ecrit ici sans passer par les appareils
				// (le chargement suivant le refait)
				if (ABRWorld* W = GetBRWorld())
				{
					const BRMission::FPlan& MPlan = W->GetMissionPlan();
					BRMission::FState& State = const_cast<BRMission::FState&>(W->GetMissionState());
					for (int32 D = 0; D < MPlan.NumDevices; ++D)
					{
						if (MPlan.Devices[D].Kind == BRMission::EKind::Clue)
						{
							State.Dev[D] = 1;
						}
						else if (MPlan.Devices[D].Kind == BRMission::EKind::Observe)
						{
							State.Dev[D] = static_cast<uint8>(MPlan.Devices[D].Positions);
						}
					}
				}
				BRTestV416Vis::LongLang = FString(BRLoc::Current().Code);
				BRLoc::SetLanguage(TEXT("de"));
				ShowScreen(4);
				if (H->NotebookHintStep >= 0)
				{
					H->NotebookHint.FindOrAdd(H->NotebookHintStep) = 2;
				}
				H->PadScrollFocus(0);
				return true;
			});
			Add(TEXT("v4.17 visuel : carnet long, une ligne par appui (PadScrollLines)"), 0.2f, [this, HudOf, ShotNamed]()
			{
				ABRHUD* H = HudOf();
				if (!H)
				{
					return true;
				}
				FIntPoint View(0, 0);
				if (ABRPlayerController* PC = GetPC())
				{
					PC->GetViewportSize(View.X, View.Y);
				}
				if (BRTestV416Vis::Phase == 0)
				{
					BRTestV416Vis::PressRoom = H->LastNotebookLeftRoom;
					BRTestV416Vis::PressFloat = H->NotebookLeftScroll;
					BRTestV416Vis::PressRight = H->NotebookScroll;
					if (!H->HadScrollRoom())
					{
						Skip(FString::Printf(TEXT("carnet long : rien a faire defiler a %d x %d (texte cache %s, reste a gauche %.0f) : une ligne par appui NON VERIFIE"), View.X, View.Y,
							BRTestV416Vis::Oui(H->HadScrollRoom()), BRTestV416Vis::PressRoom));
						return true;
					}
					H->PadScrollLines(1);
					BRTestV416Vis::Phase = 1;
					return false;
				}
				if (StepTime < 0.15f)
				{
					return false;
				}
				BRTestV416Vis::Phase = 0;
				// v4.17 (correction) : la colonne de gauche si elle deborde (descente exacte : une ligne ou ce qui restait), sinon
				// OBSERVATIONS (une ligne au plus, au moins un pixel)
				const bool bLeft = BRTestV416Vis::PressRoom >= 1.f;
				const float Want = FMath::Min(ABRHUD::PadScrollLine, BRTestV416Vis::PressRoom);
				const float Got = bLeft ? H->NotebookLeftScroll - BRTestV416Vis::PressFloat : H->NotebookScroll - BRTestV416Vis::PressRight;
				const bool bBad = bLeft ? FMath::Abs(Got - Want) > 0.5f : (Got < 0.5f || Got > ABRHUD::PadScrollLine + 0.5f);
				Note(FString::Printf(TEXT("carnet long (de, tous les indices lus) a %d x %d : texte cache %s, reste a gauche %.0f, zones %d, observations %d ; un appui (PadScrollLines(1)), colonne %s : %+.1f (attendu %s)"),
					View.X, View.Y, BRTestV416Vis::Oui(H->HadScrollRoom()), BRTestV416Vis::PressRoom, H->GetScrollZoneCount(), H->LastNotebookObservations,
					bLeft ? TEXT("de gauche") : TEXT("OBSERVATIONS"), Got, bLeft ? *FString::Printf(TEXT("+%.0f"), Want) : *FString::Printf(TEXT("+1 a +%.0f"), ABRHUD::PadScrollLine)),
					bBad);
				ShotNamed(TEXT("v417_carnet_long"));
				return true;
			});
			Add(TEXT("v4.17 visuel : carnet long, taille d'interface retablie"), 0.f, [this, CloseScreens]()
			{
				FBRSettings::Get().UiScale = 1.f;
				if (!BRTestV416Vis::LongLang.IsEmpty())
				{
					BRLoc::SetLanguage(BRTestV416Vis::LongLang);
					BRTestV416Vis::LongLang.Reset();
				}
				CloseScreens();
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
	// v4.16 (relecture) : libelles lus dans la ligne MODE D'AFFICHAGE pour chaque mode vraiment obtenu (avant : les trois noms du
	// banc compares entre eux) ; mode obtenu = fenetre reelle (Effective), et non le mode demande (Current, ecrit par Request)
	auto ModeLabel = [](int32 Window)
	{
		const FString Names[] = { BR_STR(NSLOCTEXT("BR", "Menu.PleinEcran", "PLEIN \u00c9CRAN")), BR_STR(NSLOCTEXT("BR", "Menu.FenetreSansBordure", "FEN\u00caTR\u00c9 SANS BORDURE")),
			BR_STR(NSLOCTEXT("BR", "Menu.Fenetre", "FEN\u00caTR\u00c9")) };
		return Names[FMath::Clamp(Window, 0, 2)];
	};
	auto RowValue = [this]()
	{
		const ABRPlayerController* PC = GetPC();
		const int32 Row = ABRPlayerController::FindSettingRow(TEXT("WindowMode"));
		return (PC && Row != INDEX_NONE) ? PC->GetSettingValue(Row) : FString();
	};
	Add(TEXT("v4.16 visuel : modes d'affichage presents"), 0.f, [this, ModeLabel, RowValue]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			Skip(TEXT("modes d'affichage : controleur absent : ligne MODE D'AFFICHAGE NON VERIFIEE"));
			return true;
		}
		for (bool& B : LabelSeen)
		{
			B = false;
		}
		const int32 Row = ABRPlayerController::FindSettingRow(TEXT("WindowMode"));
		const bool bRow = Row != INDEX_NONE && PC->GetSettingCategory(Row) == 1 && !PC->GetSettingLabel(Row).IsEmpty();
		const BRDisplay::FMode Eff = BRDisplay::Effective();
		const FString Value = RowValue();
		const bool bLabel = Value == ModeLabel(Eff.Window) && BRDisplay::Current().Window == Eff.Window;
		LabelSeen[FMath::Clamp(Eff.Window, 0, 2)] = bLabel;
		const bool bDistinct = ModeLabel(0) != ModeLabel(1) && ModeLabel(1) != ModeLabel(2) && ModeLabel(0) != ModeLabel(2);
		const int32 Full = BRDisplay::ResolutionsFor(0).Num();
		const int32 Borderless = BRDisplay::ResolutionsFor(1).Num();
		const int32 Windowed = BRDisplay::ResolutionsFor(2).Num();
		Note(FString::Printf(TEXT("PARAMETRES > VIDEO : ligne \"%s\" %s ; mode en cours %d, valeur affichee \"%s\" (attendu \"%s\") ; noms distincts %s ; definitions proposees : "
			"plein ecran %d, sans bordures %d, fenetre %d ; plein ecran exclusif sur cette plateforme %s"), Row != INDEX_NONE ? *PC->GetSettingLabel(Row) : TEXT("ABSENTE"), Oui(bRow),
			Eff.Window, *Value, *ModeLabel(Eff.Window), Oui(bDistinct), Full, Borderless, Windowed, Oui(BRDisplay::PlatformHasExclusiveFullscreen())),
			!bRow || !bLabel || !bDistinct || Full == 0 || Borderless == 0 || Windowed == 0);
		return true;
	});
	for (int32 Mode = 1; Mode >= 0; --Mode)
	{
		Add(FString::Printf(TEXT("v4.16 visuel : essai du mode %d"), Mode), 2.5f, [this, Mode, ShowScreen]()
		{
			ModeTried = -1;
			if (!BRDisplay::CanChange())
			{
				Skip(FString::Printf(TEXT("mode %d : affichage non modifiable (editeur) : NON VERIFIE"), Mode));
				return true;
			}
			if (Mode == 0 && !FParse::Param(FCommandLine::Get(), TEXT("BRV416PleinEcran")))
			{
				// v4.16 (relecture) : avant, une simple note ; le mode n'est ni obtenu ni lu dans la ligne
				Skip(TEXT("plein ecran exclusif : present dans la liste des definitions, non essaye (option -BRV416PleinEcran : change la definition de l'ecran) : "
					"mode obtenu et libelle NON VERIFIES"));
				return true;
			}
			ShowScreen(2);
			BRDisplay::Request(Mode, FIntPoint::ZeroValue);
			ModeTried = Mode;
			BRTestV416Vis::bDisplayTouched = true;
			Note(FString::Printf(TEXT("essai du mode %d : demande faite, en attente de confirmation %s"), Mode, Oui(BRDisplay::IsPending())), !BRDisplay::IsPending());
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 visuel : mode %d obtenu"), Mode), 0.f, [this, Mode, ShotNamed, CloseScreens, ModeLabel, RowValue]()
		{
			if (ModeTried != Mode)
			{
				return true; // essai saute : deja note NON VERIFIE
			}
			if (!BRDisplay::IsPending())
			{
				// v4.16 (relecture) : avant, l'etape sortait sans rien noter
				Skip(FString::Printf(TEXT("mode %d : aucune demande en attente apres l'essai (retour automatique ?) : mode obtenu NON VERIFIE"), Mode));
				CloseScreens();
				return true;
			}
			const BRDisplay::FMode Cur = BRDisplay::Current();
			const BRDisplay::FMode Eff = BRDisplay::Effective();
			const BRDisplay::FMonitor Mon = BRDisplay::ActiveMonitor();
			const FString Value = RowValue();
			const bool bMode = Eff.Window == Mode;
			// Sans bordures : la fenetre a la taille de l'ecran
			const bool bSize = Mode != 1 || (FMath::Abs(Eff.Resolution.X - Mon.Size.X) <= 2 && FMath::Abs(Eff.Resolution.Y - Mon.Size.Y) <= 2);
			const bool bLabel = Value == ModeLabel(Mode);
			LabelSeen[Mode] = bLabel;
			Note(FString::Printf(TEXT("mode %d : demande %d en %d x %d, obtenu %d en %d x %d (ecran %d x %d) ; valeur affichee \"%s\" (attendu \"%s\")"), Mode, Cur.Window,
				Cur.Resolution.X, Cur.Resolution.Y, Eff.Window, Eff.Resolution.X, Eff.Resolution.Y, Mon.Size.X, Mon.Size.Y, *Value, *ModeLabel(Mode)), !bMode || !bSize || !bLabel);
			ShotNamed(FString::Printf(TEXT("v416_mode_affichage_%d"), Mode));
			return true;
		});
		// v4.16 (fusion) : le retour attend la capture. Avant, Revert() dans la meme image que la demande de capture : la
		// fenetre changeait de taille avant la lecture de l'image, et Unreal 5.8 s'arretait (assertion D3D12, copie hors de
		// l'image : 'BackBuffer0' 1520 x 641 lu depuis (-8, -135))
		Add(FString::Printf(TEXT("v4.16 visuel : retour apres le mode %d"), Mode), 1.f, [this, Mode, CloseScreens]()
		{
			if (ModeTried == Mode && BRDisplay::IsPending())
			{
				BRDisplay::Revert();
			}
			CloseScreens();
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 visuel : fenetre revenue apres le mode %d"), Mode), 2.f, []()
		{
			return true;
		});
	}
	// Fenetre du lancement pour la suite (mesures des decalques, scanner, entites et kit a la taille habituelle des bancs).
	// v4.16 (relecture) : la fenetre reellement ouverte au lancement (SavedEff), et non le mode enregistre du joueur
	Add(TEXT("v4.16 visuel : fenetre d'origine"), 2.5f, []()
	{
		if (BRTestV416Vis::bDisplayTouched && BRDisplay::CanChange())
		{
			if (BRDisplay::IsPending())
			{
				BRDisplay::Revert();
			}
			BRDisplay::Request(BRTestV416Vis::SavedEff.Window, BRTestV416Vis::SavedEff.Resolution);
			BRDisplay::Confirm();
		}
		return true;
	});
	Add(TEXT("v4.16 visuel : fenetre d'origine obtenue"), 0.f, [this, ModeLabel, RowValue]()
	{
		FIntPoint View(0, 0);
		if (ABRPlayerController* PC = GetPC())
		{
			PC->GetViewportSize(View.X, View.Y);
		}
		const BRDisplay::FMode Eff = BRDisplay::Effective();
		const BRDisplay::FMode& Want = BRTestV416Vis::SavedEff;
		const bool bSame = Eff.Window == Want.Window && FMath::Abs(Eff.Resolution.X - Want.Resolution.X) <= 2 && FMath::Abs(Eff.Resolution.Y - Want.Resolution.Y) <= 2;
		if (!BRDisplay::CanChange())
		{
			Skip(TEXT("fenetre d'origine : affichage non modifiable (editeur) : NON VERIFIE"));
			return true;
		}
		// Retour en fenetre : la ligne le dit (troisieme libelle lu, si l'essai du mode sans bordures l'a quitte)
		if (Eff.Window == 2 && RowValue() == ModeLabel(2))
		{
			LabelSeen[2] = true;
		}
		Note(FString::Printf(TEXT("fenetre d'origine retablie : mode %d, %d x %d (image %d x %d) ; fenetre du lancement : mode %d, %d x %d : %s ; libelles lus dans la ligne : "
			"plein ecran %s, sans bordures %s, fenetre %s"), Eff.Window, Eff.Resolution.X, Eff.Resolution.Y, View.X, View.Y, Want.Window, Want.Resolution.X, Want.Resolution.Y,
			bSame ? TEXT("identique") : TEXT("DIFFERENTE"), LabelSeen[0] ? TEXT("oui") : TEXT("non lu"), Oui(LabelSeen[1]), Oui(LabelSeen[2])), !bSame);
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
				if (StepTime <= 30.f)
				{
					return false;
				}
				Section(FString::Printf(TEXT("v4.16 decalques : %s"), *Tag), Level); // l'etape suivante le note NON VERIFIE
				return true;
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
			if (!W || !A || !C || W->GetLevelNumber() != Level || !W->IsLevelReady())
			{
				Skip(FString::Printf(TEXT("decalques %s : monde absent ou niveau non pret : decalques NON VERIFIES"), *Tag));
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
			// Vue de la mesure : celle qui regle l'effacement des decalques (ABRChunk::DecalView : largeur interne de la vue locale,
			// champ du reglage) ; champ courant de la camera note a cote. v4.16 (relecture) : avant, une estimation du banc
			float RenderW = BRDecal::ReferenceWidth;
			float Hfov = BRDecal::ReferenceHFovDeg;
			ABRChunk::DecalView(RenderW, Hfov);
			const float CameraFov = GetPC() && GetPC()->PlayerCameraManager ? GetPC()->PlayerCameraManager->GetFOVAngle() : Hfov;
			const FVector Eye = C->GetEyeLocation();
			// v4.16 (relecture 2) : chunks construits et en place (prets, pas en demontage, comme le banc de lumiere) : seuls ceux-la
			// sont mesures et portent un support. Le mur d'une ligne de la grille est bati par le chunk de la cellule a l'ouest (ligne
			// X) ou au sud (ligne Y) de la ligne (ABRChunk::StepPlan, etapes 1 et 2), le mur d'enceinte ouest ou sud par le chunk du
			// bord (etape 3) ; le sol et le plafond d'une cellule par son chunk. Un decalque de la colonne ouest (ou de la rangee
			// sud) d'un chunk, sur sa face ouest (ou sud), est donc pose sur le mur du chunk voisin. Rapport v416vis (b1b19d3) : les
			// 4 decalques "sans aucune surface" (Level 1 : eraflures en (-9540, -1679), (-9540, -1094), (-4740, -9093) ; Level 2 :
			// aureole en (2484, -6195)) et la tache au sol "au-dessus du vide" en (7391..7630, 4915..4935) (seuil d'un passage de la
			// ligne Y = 4800, pose par le chunk (1, 0)) avaient leur support dans un chunk hors de la vue (Level 1 : (-3, -1),
			// (-2, -2), (1, 1) ; Level 2 : (1, -4)) : 12 et 32 chunks construits, exactement les chunks de centre a moins de
			// ViewDistance + 0,75 chunk du depart (ABRWorld::UpdateStreaming). Le bord de la zone construite est a plus de 50 m du
			// joueur, au-dela de l'effacement des decalques (35 m) : rien ne s'y voit. Ces points ne sont plus comptes "dans le vide"
			// mais a part (support non construit), et le decalque doit etre au-dela de sa distance d'effacement
			TSet<FIntPoint> Live;
			for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
			{
				if (It->IsReady() && !It->IsTearingDown())
				{
					Live.Add(It->Coord);
				}
			}
			auto WallOwner = [W, S, T](const FVector& OnFace, const FVector& Into)
			{
				const FVector Mid = OnFace + Into * (T * 0.5f);
				const bool bLineX = FMath::Abs(Into.X) > FMath::Abs(Into.Y);
				const int32 Line = FMath::RoundToInt32(static_cast<float>((bLineX ? Mid.X : Mid.Y) / S));
				const int32 Along = FMath::FloorToInt32(static_cast<float>((bLineX ? Mid.Y : Mid.X) / S));
				FIntPoint Cell = bLineX ? FIntPoint(Line - 1, Along) : FIntPoint(Along, Line - 1);
				if (!W->IsCellInBounds(Cell.X, Cell.Y))
				{
					Cell = bLineX ? FIntPoint(Line, Along) : FIntPoint(Along, Line); // enceinte ouest ou sud : chunk du bord
				}
				return W->CellToChunk(Cell);
			};
			const float DoorW = FMath::Min(Def.DoorWidth, S - 2.f * T - 20.f);
			// Occurrences par face de mur (cellule, cote) pour les voisinages
			TMap<FIntVector, int32> FaceKind;
			for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
			{
				ABRChunk* Ch = *It;
				if (!Ch->IsReady() || Ch->IsTearingDown())
				{
					continue; // encore en construction (decalques crees un par un) ou en demontage
				}
				TArray<UDecalComponent*> Decals;
				Ch->GetComponents<UDecalComponent>(Decals);
				int32 Patches = 0;
				int32 ChunkProjected = 0;
				TArray<UInstancedStaticMeshComponent*> Isms;
				Ch->GetComponents<UInstancedStaticMeshComponent>(Isms);
				++St.Chunks;
				St.Expected += Ch->GetDecalCount();
				St.Dropped += Ch->GetDecalDropped();
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
					++ChunkProjected;
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
					// Surface sous le decalque : 5 x 3 points de l'image, trace le long de l'axe de projection. v4.16 (relecture 2) : un
					// point sans surface dont le support est bati par un chunk non construit est compte a part (FrontierHere)
					const bool bWallKind = Kind != static_cast<int32>(EBRDecal::CeilingStain);
					int32 On = 0, Total = 0, VoidHere = 0, FrontierHere = 0;
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
							const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q, Resp);
							const float D = bHit ? static_cast<float>(FVector::DotProduct(Hit.ImpactPoint - P, XAx)) : 0.f;
							if (bHit && D < -Half.X)
							{
								++St.Blocked;
								continue;
							}
							if (!bHit || D > Half.X)
							{
								const FIntPoint Owner = bWallKind ? WallOwner(At, XAx) : W->CellToChunk(W->WorldToCell(At));
								if (!Live.Contains(Owner))
								{
									++St.FrontierSamples;
									++FrontierHere;
									continue;
								}
								++St.Void;
								++VoidHere;
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
					if (FrontierHere > 0)
					{
						// Support dans un chunk non construit : le decalque ne doit pas se voir (au-dela de sa distance d'effacement)
						++St.DecalsFrontier;
						const float Dist = static_cast<float>(FVector::Dist(Eye, P));
						St.FrontierNearest = FMath::Min(St.FrontierNearest, Dist);
						if (Dist < FadeAt)
						{
							++St.FrontierVisible;
							St.Example(FString::Printf(TEXT("sorte %d sans support construit a %.1f m du joueur (effacement a %.1f m) en (%.0f, %.0f, %.0f)"), Kind, Dist / 100.f,
								FadeAt / 100.f, P.X, P.Y, P.Z));
						}
					}
					if (On == 0 && FrontierHere < Total)
					{
						++St.DecalsFloating;
						St.Example(FString::Printf(TEXT("sorte %d sans surface en (%.0f, %.0f, %.0f)"), Kind, P.X, P.Y, P.Z));
					}
					else if (On > 0 && On < Total)
					{
						++St.DecalsPartial;
					}
					if (Kind != static_cast<int32>(EBRDecal::CeilingStain))
					{
						const bool bEastWest = FMath::Abs(XAx.X) > FMath::Abs(XAx.Y);
						// v4.16 (relecture) : decalque de mur dont une partie ne tombe sur rien (geometrie reelle : bout de mur, trou,
						// jambage plus court que prevu) ; avant, compte sans jamais etre un probleme
						bool bOpening = false;
						if (VoidHere > 0 && On > 0)
						{
							++St.WallPartVoid;
							const FIntPoint Own = W->WorldToCell(P - XAx * 30.f);
							const EBREdge OwnEdge = bEastWest ? W->EdgeE(XAx.X > 0.f ? Own.X : Own.X - 1, Own.Y) : W->EdgeN(Own.X, XAx.Y > 0.f ? Own.Y : Own.Y - 1);
							if (OwnEdge == EBREdge::Door)
							{
								// Eraflure de jambage qui deborde dans le passage (mesure sur la geometrie, independante de BuildDecals)
								bOpening = true;
								St.Example(FString::Printf(TEXT("sorte %d sur un jambage, %d point(s) sur %d dans le vide (passage) en (%.0f, %.0f, %.0f)"), Kind, VoidHere, Total, P.X,
									P.Y, P.Z));
							}
							else
							{
								St.Example(FString::Printf(TEXT("sorte %d en partie dans le vide (%d point(s) sur %d) en (%.0f, %.0f, %.0f)"), Kind, VoidHere, Total, P.X, P.Y, P.Z));
							}
						}
						// Ouverture : un point d'une face de mur qui tombe dans un passage (grille ; meme largeur que BuildDecals : controle
						// de coherence, la mesure independante est celle de la geometrie ci-dessus)
						for (int32 a = -2; a <= 2 && !bOpening; ++a)
						{
							const FVector At = P + ZAx * (Half.Z * 0.95f * a / 2.f);
							const FIntPoint In = W->WorldToCell(At - XAx * 30.f);
							const FVector Ctr = W->CellCenter(In);
							const float Along = static_cast<float>(bEastWest ? At.Y - Ctr.Y : At.X - Ctr.X);
							const EBREdge E = bEastWest ? W->EdgeE(XAx.X > 0.f ? In.X : In.X - 1, In.Y) : W->EdgeN(In.X, XAx.Y > 0.f ? In.Y : In.Y - 1);
							const float DW = FMath::Min(Def.DoorWidth, S - 2.f * T - 20.f);
							if (E == EBREdge::Door && FMath::Abs(Along) < DW * 0.5f)
							{
								bOpening = true;
								St.Example(FString::Printf(TEXT("sorte %d au-dessus d'un passage en (%.0f, %.0f, %.0f)"), Kind, At.X, At.Y, At.Z));
							}
						}
						St.OverOpening += bOpening ? 1 : 0;
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
								// Sous un mur (cellule pleine, ou dans l'epaisseur d'une arete mur / d'un jambage) : cache, normal. v4.16 (relecture
								// 2) : dans l'ouverture d'un passage (a moins de la demi-largeur du passage de son milieu), le sol se voit : le point
								// est juge au sol (avant : compte cache, ce qui laissait le seuil humide sans controle)
								bool bDoorway = false;
								auto Covers = [&bDoorway, DoorW](EBREdge E, float AlongEdge)
								{
									if (E == EBREdge::Door && FMath::Abs(AlongEdge) < DoorW * 0.5f)
									{
										bDoorway = true;
										return false;
									}
									return E != EBREdge::Open;
								};
								const bool bSolid = !W->IsWalkable(Cell);
								const bool bInWall = (LX > Edge && Covers(W->EdgeE(Cell.X, Cell.Y), LY)) || (LX < -Edge && Covers(W->EdgeE(Cell.X - 1, Cell.Y), LY))
									|| (LY > Edge && Covers(W->EdgeN(Cell.X, Cell.Y), LX)) || (LY < -Edge && Covers(W->EdgeN(Cell.X, Cell.Y - 1), LX));
								++St.PatchSamples;
								if (bSolid || bInWall)
								{
									++St.PatchHidden;
									continue;
								}
								St.PatchDoorway += bDoorway ? 1 : 0;
								// v4.16 (relecture 2) : un rayon parti dans un objet ou un mur (40 cm au-dessus de la tache) touche a son depart :
								// l'ecart lu etait alors -40 cm ("ecart au sol le plus grand 40.0 cm" aux quatre graines de v416vis, compte
								// enfoui). Il est relance au ras de la tache (2 cm au-dessus du sol) ; s'il y part encore dedans, la tache passe
								// sous un objet ou un mur pose sur le sol (cachee, comptee a part)
								FHitResult Hit;
								bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 40.f), At - FVector(0.f, 0.f, 60.f), ECC_Visibility, Q, Resp);
								if (bHit && (Hit.bStartPenetrating || Hit.Time <= 0.f))
								{
									bHit = GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 1.5f), At - FVector(0.f, 0.f, 60.f), ECC_Visibility, Q, Resp);
									if (bHit && (Hit.bStartPenetrating || Hit.Time <= 0.f))
									{
										++St.PatchUnderObject;
										continue;
									}
								}
								if (!bHit)
								{
									// Sol de la cellule bati par un chunk non construit (seuil d'un passage sur la ligne nord ou est du chunk) : non
									// juge ; la tache ne doit pas se voir (au-dela de la distance d'affichage du lot, BRDecal::FloorPatchCull)
									if (!Live.Contains(W->CellToChunk(Cell)))
									{
										++St.PatchFrontier;
										const float Dist = static_cast<float>(FVector::Dist(Eye, At));
										St.PatchFrontierNearest = FMath::Min(St.PatchFrontierNearest, Dist);
										if (Dist < BRDecal::FloorPatchCull)
										{
											++St.PatchFrontierVisible;
											St.Example(FString::Printf(TEXT("tache au sol sans sol construit a %.1f m du joueur (affichage jusqu'a %.0f m) en (%.0f, %.0f)"), Dist / 100.f,
												BRDecal::FloorPatchCull / 100.f, At.X, At.Y));
										}
										continue;
									}
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
				St.MaxPerChunk = FMath::Max(St.MaxPerChunk, ChunkProjected);
				St.MinPerChunk = FMath::Min(St.MinPerChunk, ChunkProjected);
				// v4.16 (relecture 2) : la borne : chaque chunk garde exactement min(tirages, borne) de chaque sorte (rien de plus, rien
				// de moins : les ecartes sont repartis par BRDecal::KeepUnderCap, banc test_decals "Borne par chunk : repartition")
				const int32 DrawnProj = Ch->GetDecalDrawn(false);
				const int32 DrawnPatch = Ch->GetDecalDrawn(true);
				St.CappedChunks += (DrawnProj > BRDecal::MaxProjectedPerChunk || DrawnPatch > BRDecal::MaxFloorPatchesPerChunk) ? 1 : 0;
				if (ChunkProjected != FMath::Min(DrawnProj, BRDecal::MaxProjectedPerChunk) || Patches != FMath::Min(DrawnPatch, BRDecal::MaxFloorPatchesPerChunk))
				{
					++St.CapMismatch;
					St.Example(FString::Printf(TEXT("chunk (%d, %d) : %d projete(s) pour %d tire(s), %d tache(s) au sol pour %d tiree(s) (bornes %d et %d)"), Ch->Coord.X, Ch->Coord.Y,
						ChunkProjected, DrawnProj, Patches, DrawnPatch, BRDecal::MaxProjectedPerChunk, BRDecal::MaxFloorPatchesPerChunk));
				}
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
			// v4.16 (relecture) : tirages ecartes par la borne comptes (avant : invisibles, "crees sur tires" comparait deux nombres
			// pris apres la borne). v4.16 (relecture 2) : un ecart par la borne n'est plus un probleme : la borne est l'exigence
			// (decalques bornes par chunk) et l'ecart regulier est son fonctionnement voulu ; v416vis, Level 0 graine 4242 : un chunk
			// a tire 42 decalques projetes, 40 gardes (le banc admet la borne dans 1 % des chunks du cas dense). Probleme : un chunk
			// qui garde plus que la borne, ou autre chose que min(tirages, borne) d'une sorte, ou un decalque garde non cree
			Note(FString::Printf(TEXT("%s : %d chunk(s), %d decalque(s) crees sur %d gardes, %d tirage(s) ecarte(s) par la borne dans %d chunk(s) (aureoles %d, taches de plafond %d, "
				"eraflures %d ; taches au sol %d) ; par chunk %d a %d projetes (borne %d), %d taches au sol au plus (borne %d) ; chunks qui ne gardent pas min(tirages, borne) : %d"), *Tag,
				St.Chunks, Created, St.Expected, St.Dropped, St.CappedChunks, St.KindCount[0], St.KindCount[1], St.KindCount[3], St.Patches, St.MinPerChunk == MAX_int32 ? 0 : St.MinPerChunk,
				St.MaxPerChunk, BRDecal::MaxProjectedPerChunk, St.MaxPatchesPerChunk, BRDecal::MaxFloorPatchesPerChunk, St.CapMismatch),
				Created != St.Expected || St.CapMismatch > 0 || St.MaxPerChunk > BRDecal::MaxProjectedPerChunk || St.MaxPatchesPerChunk > BRDecal::MaxFloorPatchesPerChunk);
			// v4.16 (relecture) : un decalque sur un objet (autre acteur que le chunk ; les objets du chunk ne recoivent plus les
			// decalques) ou un decalque de mur en partie dans le vide sont des problemes (avant : comptes seulement). v4.16 (relecture
			// 2) : support bati par un chunk non construit compte a part ; probleme s'il est a portee de vue (avant son effacement)
			const FString FrontierText = St.DecalsFrontier > 0
				? FString::Printf(TEXT("%d decalque(s) (%d point(s)), le plus proche a %.1f m du joueur, %d avant son effacement"), St.DecalsFrontier, St.FrontierSamples,
					St.FrontierNearest / 100.f, St.FrontierVisible)
				: FString(TEXT("aucun"));
			Note(FString::Printf(TEXT("%s : surface sous les decalques projetes : %d point(s) sur %d sur la surface, %d dans le vide, %d derriere un obstacle, %d sur un objet (%s), %d obliques ; "
				"decalques sans aucune surface %d, partiellement dans le vide %d (dont murs %d), au-dessus d'un passage %d ; support dans un chunk non construit (bord de la zone "
				"construite, non juge) : %s"), *Tag, St.OnSurface, St.Samples, St.Void, St.Blocked, St.OnObject, Objects.IsEmpty() ? TEXT("aucun") : *Objects, St.Oblique,
				St.DecalsFloating, St.DecalsPartial, St.WallPartVoid, St.OverOpening, *FrontierText),
				St.DecalsFloating > 0 || St.OverOpening > 0 || St.OnObject > 0 || St.WallPartVoid > 0 || St.FrontierVisible > 0);
			const FString PatchFrontierText = St.PatchFrontier > 0
				? FString::Printf(TEXT("%d point(s), le plus proche a %.1f m du joueur, %d avant %.0f m"), St.PatchFrontier, St.PatchFrontierNearest / 100.f, St.PatchFrontierVisible,
					BRDecal::FloorPatchCull / 100.f)
				: FString(TEXT("aucun"));
			Note(FString::Printf(TEXT("%s : taches au sol : %d point(s), %d caches sous un mur, %d sous un objet ou un mur pose sur le sol, %d dans l'ouverture d'un passage (juges), "
				"%d flottant(s) (> 2 cm), %d enfoui(s) (< 1 mm), ecart au sol le plus grand %.1f cm ; sol dans un chunk non construit (non juge) : %s"), *Tag, St.PatchSamples,
				St.PatchHidden, St.PatchUnderObject, St.PatchDoorway, St.PatchFloating, St.PatchBuried, St.PatchMaxGap, *PatchFrontierText),
				St.PatchFloating > 0 || St.PatchFrontierVisible > 0);
			if (St.DecalsFrontier > 0 || St.PatchFrontier > 0)
			{
				// Condition manquante pour ceux-la (leur support n'existe pas encore) : NON VERIFIE, jamais compte reussi
				Skip(FString::Printf(TEXT("decalques %s : %d decalque(s) projete(s) et %d point(s) de taches au sol au bord de la zone construite (mur ou sol bati par un chunk pas "
					"encore construit) : leur surface NON VERIFIEE (a verifier joueur deplace vers eux)"), *Tag, St.DecalsFrontier, St.PatchFrontier));
			}
			Note(FString::Printf(TEXT("%s : motifs : %.2f %% des faces de cellule portent une aureole ou une eraflure ; meme sorte sur deux faces voisines %d / %d ; %d signatures pour %d decalques "
				"projetes, la plus frequente %d fois ; eraflures retournees %d / %d"), *Tag, Rate * 100.f, Same, Pairs, St.Signatures.Num(), St.Projected, TopSig, St.Flipped,
				St.Scuffs), St.Projected >= 20 && TopSig * 10 > St.Projected);
			// v4.16 (relecture) : effacement calcule pour la vue locale : au plus tard a la distance utile a toute definition
			Note(FString::Printf(TEXT("%s : distance utile (vue locale : image de %.0f px de large, champ du reglage %.0f degres, camera %.0f) : de %.1f a %.1f m selon la taille "
				"(au plus %.0f m) ; %d decalque(s) projete(s) sur %d a portee du joueur"), *Tag, RenderW, Hfov, CameraFov, St.FadeMin / 100.f, St.FadeMax / 100.f,
				BRDecal::UsefulDistance / 100.f, St.WithinUseful, St.Projected), St.FadeMax > BRDecal::UsefulDistance + 50.f);
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
		if (H->LastLidarHintAlpha <= 0.f)
		{
			// v4.16 (relecture) : rappel deja efface : avant, "LT NON, RT NON" passait sans probleme ni NON VERIFIE
			Skip(TEXT("rappel du scanner a la manette : rappel des touches plus affiche : boutons LT / RT NON VERIFIES"));
		}
		else
		{
			Note(FString::Printf(TEXT("rappel du scanner a la manette : LT %s, RT %s (rappel %.2f)"), Oui(bLT), Oui(bRT), H->LastLidarHintAlpha), !(bLT && bRT));
		}
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
		Note(FString::Printf(TEXT("joueur (combinaison, troisieme personne locale) : %s"), *Mats), Bad > 0);
		// v4.16 (relecture) : la troisieme personne de ce banc est celle du joueur local, sans session : la vue d'un vrai coequipier
		// (client : pose et animation recues par le reseau, maillage de la combinaison vu par les autres, composants caches au
		// proprietaire) n'est pas exercee ici
		Skip(TEXT("vue d'un coequipier reel (client qui regarde l'entite et le joueur de l'hote) : NON VERIFIE (banc solo ; a faire au test reseau)"));
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
			if (!Info.bFlying)
			{
				// v4.16 (relecture) : posee au sol avant d'etre figee (avant : apparue 5 cm au-dessus, figee aussitot en pleine
				// chute : la capture au repos la montrait en l'air)
				FCollisionQueryParams FloorQ(SCENE_QUERY_STAT(BRV416Land), false, E);
				FloorQ.AddIgnoredActor(C);
				FCollisionResponseParams FloorResp;
				FloorResp.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
				FHitResult FloorHit;
				if (GetWorld()->LineTraceSingleByChannel(FloorHit, Spot + FVector(0.f, 0.f, 50.f), Spot - FVector(0.f, 0.f, 400.f), ECC_Visibility, FloorQ, FloorResp))
				{
					const float Half = E->GetCapsuleComponent() ? E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : Info.HalfHeight;
					E->SetActorLocation(FVector(Spot.X, Spot.Y, FloorHit.ImpactPoint.Z + Half + 1.f), false, nullptr, ETeleportType::TeleportPhysics);
				}
				if (UCharacterMovementComponent* Move = E->GetCharacterMovement())
				{
					Move->StopMovementImmediately();
					Move->SetMovementMode(MOVE_Walking);
				}
			}
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
			// v4.17 (appuis) : une Bacteria en bond ou portee par ses bras quitte le sol par conception (demi-tour en bonds de 25
			// degres) : on attend qu'elle soit posee, 1,5 s au plus, avant de mesurer son appui
			static double PosedWait = -1.0;
			const double NowT = FPlatformTime::Seconds();
			if (!E->IsPosed() && (PosedWait < 0.0 || NowT - PosedWait < 1.5))
			{
				if (PosedWait < 0.0)
				{
					PosedWait = NowT;
				}
				return false;
			}
			const bool bPosedAtCheck = E->IsPosed();
			PosedWait = -1.0;
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
					// v4.16 (relecture 2) : pieces rigides (maillages statiques) : le plus bas de leurs vrais sommets (donnees du
					// maillage lisibles par le processeur : editeur), au lieu du bas de leur boite tournee. La boite d'une piece
					// tournee (Bounds : boite du maillage tournee puis remise droite) descend sous ses sommets : bras de la Bacteria
					// au repos, boite 1,8 cm sous la griffe la plus basse, 3,9 cm a l'armee (sommets du modele fourni, hors moteur) ;
					// v416vis "-3.4 cm" etait cette boite. Le bas des boites reste note ; sans les sommets, la boite sert
					float LowBoxes = 1.0e9f;
					int32 VertexParts = 0, BoxParts = 0;
					TArray<UPrimitiveComponent*> Prims;
					E->GetComponents<UPrimitiveComponent>(Prims);
					for (const UPrimitiveComponent* P : Prims)
					{
						if (!P || !P->IsVisible() || P->IsA<UCapsuleComponent>())
						{
							continue;
						}
						const float BoxLow = static_cast<float>(P->Bounds.Origin.Z - P->Bounds.BoxExtent.Z);
						LowBoxes = FMath::Min(LowBoxes, BoxLow);
						float VertLow = 0.f;
						if (const UStaticMeshComponent* Sm = Cast<UStaticMeshComponent>(P); Sm && LowestVertexZ(Sm, VertLow))
						{
							LowBounds = FMath::Min(LowBounds, VertLow);
							++VertexParts;
						}
						else
						{
							LowBounds = FMath::Min(LowBounds, BoxLow);
							BoxParts += Cast<UStaticMeshComponent>(P) ? 1 : 0;
						}
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
					const UCharacterMovementComponent* Move = E->GetCharacterMovement();
					const bool bGround = Move && Move->IsMovingOnGround();
					const bool bRigid = LowBone >= 1.0e8f;
					const float Gap = LowBounds < 1.0e8f ? LowBounds - Floor : 0.f;
					// v4.16 (relecture) : ecart au sol controle (3 cm) : au-dessus, l'entite flotte ; en dessous, seulement pour les
					// pieces rigides (les bornes d'un maillage a squelette viennent de son corps physique et peuvent passer sous le sol)
					const bool bFloating = LowBounds < 1.0e8f && Gap > 3.f;
					const bool bSunk = LowBounds < 1.0e8f && bRigid && Gap < -3.f;
					if (!bGround || LowBounds >= 1.0e8f)
					{
						Skip(FString::Printf(TEXT("%s : appui au sol NON VERIFIE (%s)"), *Name, !bGround ? TEXT("entite pas au sol en marche") : TEXT("aucun volume visible")));
					}
					const FString Measure = VertexParts > 0
						? FString::Printf(TEXT("sommets de %d piece(s) rigide(s)%s ; bas des boites a %+.1f cm"), VertexParts, BoxParts > 0 ? *FString::Printf(TEXT(", boites de %d"), BoxParts)
							: TEXT(""), LowBoxes - Floor)
						: FString(TEXT("boites"));
					Note(FString::Printf(TEXT("%s : appui au sol (au sol en marche %s%s) : %s ; bas des volumes visibles a %+.1f cm du sol (borne 3 cm%s ; %s)"), *Name, Oui(bGround),
						bPosedAtCheck ? TEXT("") : TEXT(", encore en bond ou portee apres 1,5 s d'attente"),
						!bRigid ? *FString::Printf(TEXT("os le plus bas \"%s\" a %+.1f cm"), *LowName.ToString(), LowBone - Floor) : TEXT("sans squelette (pieces rigides)"),
						Gap, bRigid ? TEXT(", des deux cotes") : TEXT(" au-dessus"), *Measure), bGround && (bFloating || bSunk));
				}
				else
				{
					Skip(FString::Printf(TEXT("%s : sol introuvable sous l'entite : appui au sol NON VERIFIE"), *Name));
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
		bKitReady = false;
		if (!W || !A || !W->IsLevelReady() || W->GetLevelNumber() != 0)
		{
			if (StepTime <= 40.f)
			{
				return false;
			}
			// v4.16 (relecture) : avant, l'etape passait sans section ni note, et les captures des raccords suivaient sur le niveau
			// charge, quel qu'il soit
			Section(TEXT("v4.16 kit du Level 0 : raccords"), 0);
			Skip(TEXT("kit du Level 0 : niveau non pret en 40 s : plinthes, plafonds, sols et captures des raccords NON VERIFIES"));
			return true;
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
			Skip(FString::Printf(TEXT("kit du Level 0 non utilise (%s) : raccords et leurs captures NON VERIFIES"), *A->GetLevel0KitStatus()));
			return true;
		}
		bKitReady = true;
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
		// v4.16 (relecture) : boites rangees par case de grille (FBoxGrid) : chaque point ne teste que les boites de sa case (avant :
		// toutes les boites pour chacun des quelque 2,6e5 points, environ 1e9 tests dans une seule image)
		FBoxGrid WallsG, PostCoresG, BaseboardsG, PostsG, CasingsG, CeilingsG, FloorsG;
		WallsG.Build(Walls, S, 1.f);
		PostCoresG.Build(PostCores, S, 1.f);
		BaseboardsG.Build(Baseboards, S, 1.f);
		PostsG.Build(Posts, S, 1.f);
		CasingsG.Build(Casings, S, 1.f);
		CeilingsG.Build(Ceilings, S, 1.f);
		FloorsG.Build(Floors, S, 1.f);
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
							if (WallsG.Inside(P, -0.2f) || PostCoresG.Inside(P, -0.2f))
							{
								++Hidden;
								Run = 0.f;
								continue;
							}
							if (BaseboardsG.Inside(P, 0.05f) || PostsG.Inside(P, 0.05f) || CasingsG.Inside(P, 0.05f))
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
					const bool bCeil = CeilingsG.Inside(C0 + FVector(0.f, 0.f, H + 1.f), 0.1f);
					NoCeil += bCeil ? 0 : 1;
					NoFloor += FloorsG.Inside(C0 + FVector(0.f, 0.f, -1.f), 0.1f) ? 0 : 1;
					// Joint avec la cellule voisine a l'est, sans mur entre elles, les deux sous un plafond du kit : il continue
					const bool bEastCeil = CeilingsG.Inside(W->CellCenter(FIntPoint(X + 1, Y)) + FVector(0.f, 0.f, H + 1.f), 0.1f);
					if (bCeil && bEastCeil && W->EdgeE(X, Y) == EBREdge::Open && W->IsWalkable(FIntPoint(X + 1, Y)) && !W->IsPitRoomCell(X + 1, Y))
					{
						for (float D = -0.6f; D <= 0.6f; D += 0.3f)
						{
							if (!CeilingsG.Inside(C0 + FVector(S * 0.5f + D, 0.f, H + 1.f), 0.02f))
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
			// v4.16 (relecture) : sans le kit mesure (niveau non pret, kit absent : deja note NON VERIFIE), pas de capture des
			// raccords sur un autre decor
			if (!W || !C || !bKitReady || W->GetLevelNumber() != 0)
			{
				BRTestV416Vis::Phase = -1;
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
		// v4.16 (relecture) : sac et valeurs reperes retablis si une etape a ete interrompue avant de le faire
		if (ABRCharacter* Hero = GetPlayer())
		{
			FBRItemSlot* S1 = Hero->GetSlot(EBRSlotGroup::Storage, 1);
			FBRItemSlot* Head = Hero->GetSlot(EBRSlotGroup::Equipment, static_cast<int32>(EBREquipSlot::Head));
			if (BRTestV416Vis::bInvSaved && S1 && Head)
			{
				*S1 = BRTestV416Vis::SavedStorage1;
				*Head = BRTestV416Vis::SavedHead;
				Hero->OnEquipmentChanged();
			}
			if (BRTestV416Vis::bVitalsSaved)
			{
				Hero->Health = BRTestV416Vis::SavedHealth;
				Hero->Stamina = BRTestV416Vis::SavedStamina;
				Hero->Sanity = BRTestV416Vis::SavedSanity;
			}
		}
		BRTestV416Vis::bInvSaved = false;
		BRTestV416Vis::bVitalsSaved = false;
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
