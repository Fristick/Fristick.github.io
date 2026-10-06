// v4.9 : etapes du test automatique propres a la v4.9 (-BRAutoTest, ou seules avec -BRAutoTest -BRAutoTestV49).
// Chaque etape verifie un defaut corrige ou une fonction ajoutee en v4.9 :
//   - HUD : aucune jauge de statut pendant l'exploration (aucun reglage ne les y remet), endurance et sante mentale dans
//     l'onglet Personnage seulement, aucun personnage fictif dans les parametres du menu titre, un seul avertissement
//     quand les piles deviennent faibles ;
//   - Tab : l'inventaire s'ouvre sur l'onglet Personnage, la touche pressee pendant une reaffectation ne ferme pas
//     l'ecran, une touche reaffectee reconstruit les commandes, le sprint ne reprend pas tout seul, le jeu continue ;
//   - interface : taille (inventaire dans l'ecran a 0,8, 1,25 en allemand et 1 en arabe), opacite, reticule, objets
//     rapides et objectifs (brievement, toujours, masques) ;
//   - parametres : categories, libelles et aides des nouvelles lignes, reglages RT bloques si le RHI ne le permet pas ;
//   - affichage (jeu lance seul) : essai, retour, confirmation, expiration en 15 s, profil graphique sans effet sur la
//     fenetre ; dans l'editeur : etat seulement (la fenetre est celle de l'editeur) ;
//   - rendu : cadence d'animation d'une creature proche (pleine, a l'ecran ou non), decisions a chaque image, creatures
//     dans les reflets (variables du moteur), lumieres changees de type sur plusieurs images (mesure de 3 s) ;
//   - prechargement par ensembles : commun + niveau + voisins, memes ensembles en revenant au Niveau 0.
// Les reglages, la langue, les touches, les poches et l'affichage du joueur sont retablis a la fin.
#include "BRAutoTest.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BRChunk.h"
#include "BRDisplay.h"
#include "BREntity.h"
#include "BRHUD.h"
#include "BRKeys.h"
#include "BRLevels.h"
#include "BRLoc.h"
#include "BRPlayerController.h"
#include "BRWorld.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	ABRHUD* HudOf(ABRPlayerController* PC)
	{
		return PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr;
	}

	FIntPoint ViewportSize()
	{
		FVector2D Size(0.0, 0.0);
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(Size);
		}
		return FIntPoint(FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y));
	}

	int32 CVarInt(const TCHAR* Name, bool& bExists)
	{
		IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name);
		bExists = V != nullptr;
		return V ? V->GetInt() : 0;
	}

	const TCHAR* YesNo(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	FString ModeText(const BRDisplay::FMode& M)
	{
		static const TCHAR* const Names[] = { TEXT("plein ecran"), TEXT("plein ecran fenetre"), TEXT("fenetre") };
		return FString::Printf(TEXT("%s %dx%d"), Names[FMath::Clamp(M.Window, 0, 2)], M.Resolution.X, M.Resolution.Y);
	}

	/** Fenetre d'essai : 1280x720 si le bureau le permet, sinon la premiere taille proposee */
	FIntPoint TrialResolution()
	{
		const TArray<FIntPoint> Sizes = BRDisplay::ResolutionsFor(2);
		for (const FIntPoint& S : Sizes)
		{
			if (S == FIntPoint(1280, 720))
			{
				return S;
			}
		}
		return Sizes.Num() > 0 ? Sizes[0] : FIntPoint(1280, 720);
	}

	/** Cadence de la pose attendue (meme regle que ABREntity::ApplySkins) */
	float ExpectedSkinPeriod(bool bSeen, float Dist)
	{
		return bSeen ? (Dist > 2500.f ? 0.05f : 0.f) : (Dist < 1500.f ? 0.f : (Dist < 3000.f ? 1.f / 30.f : 0.2f));
	}

	/** Informations d'exploration dessinees (ABRHUD::ExploreDrawn) */
	FString ExploreText(int32 Bits)
	{
		TArray<FString> Parts;
		if (Bits & 1)
		{
			Parts.Add(TEXT("objets rapides"));
		}
		if (Bits & 2)
		{
			Parts.Add(TEXT("objectifs"));
		}
		if (Bits & 4)
		{
			Parts.Add(TEXT("point du reticule"));
		}
		return Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : FString(TEXT("rien"));
	}

	/** Coequipier (personnage d'un autre joueur) */
	ABRCharacter* FindMate(UWorld* World, ABRCharacter* Me)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<ABRCharacter> It(World); It; ++It)
		{
			if (*It != Me && !It->IsLocallyControlled())
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Un mur entre deux points (meme requete que le nom du coequipier dans ABRHUD::DrawTeammates) */
	bool IsBlocked(UWorld* World, const FVector& From, const FVector& To, const AActor* A, const AActor* B)
	{
		FCollisionQueryParams Query(FName(TEXT("BRAutoTestV49")), false);
		Query.AddIgnoredActor(A);
		Query.AddIgnoredActor(B);
		return World->LineTraceTestByChannel(From, To, ECC_Visibility, Query);
	}
}

void ABRAutoTest::AddV49Steps()
{
	AddLoad(0, 5.f, TEXT("v4.9 : interface, commandes, affichage, rendu (Niveau 0)"));
	Add(TEXT("v4.9 : preparation"), 0.8f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		ABRWorld* W = GetBRWorld();
		if (!PC || !C || !W || !HudOf(PC) || W->GetTitleTime() > 0.f)
		{
			return false;
		}
		Report().Scene = TEXT("v4.9");
		if (!bSettingsSaved)
		{
			SavedSettings = FBRSettings::Get();
			bSettingsSaved = true;
		}
		TestLanguage = BRLoc::Current().Code;
		FBRSettings& S = FBRSettings::Get();
		S.UiScale = 1.f;
		S.HudOpacity = 1.f;
		S.CrosshairMode = 0;
		S.QuickBarMode = 0;
		S.ObjectivesMode = 0;
		PC->ApplySettings();
		PC->SetInventoryOpen(false);
		C->Stamina = 100.f;
		C->Sanity = 100.f;
		return true;
	});

	// ------------------------------------------------------------------------------------------------ HUD et Tab
	Add(TEXT("v4.9 HUD : aucune jauge en exploration"), 0.f, [this]()
	{
		ABRHUD* H = HudOf(GetPC());
		if (!H)
		{
			return true;
		}
		const int32 N = H->LastFrameStatusGauges;
		Note(FString::Printf(TEXT("exploration : %d jauge(s) de statut a l'ecran (attendu : 0 ; ni vie, ni endurance, ni sante mentale)"), N), N != 0);
		Shot(TEXT("V49_exploration"));
		return true;
	});
	Add(TEXT("v4.9 Tab : ouverture"), 0.6f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (ABRHUD* H = HudOf(PC))
		{
			// Visite precedente sur l'onglet Parametres : l'inventaire doit quand meme s'ouvrir sur l'onglet Personnage
			H->Tab = ABRHUD::ETab::Settings;
			PC->OnInventory(FInputActionValue());
		}
		return true;
	});
	Add(TEXT("v4.9 Tab : onglet Personnage et deux jauges"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = HudOf(PC);
		if (!H)
		{
			return true;
		}
		const bool bOpen = PC->IsInventoryOpen();
		const bool bCharacter = H->Tab == ABRHUD::ETab::Character;
		const int32 N = H->LastFrameStatusGauges;
		const bool bPaused = UGameplayStatics::IsGamePaused(this);
		Note(FString::Printf(TEXT("Tab : inventaire ouvert %s, onglet Personnage %s (visite precedente : Parametres), jauges %d (attendu : 2, endurance et sante mentale), jeu en pause : %s (attendu : non)"),
			YesNo(bOpen), YesNo(bCharacter), N, bPaused ? TEXT("OUI") : TEXT("non")), !bOpen || !bCharacter || N != 2 || bPaused);
		Shot(TEXT("V49_inventaire_personnage"));
		if (ABRCharacter* C = GetPlayer())
		{
			C->Stamina = 22.f;
			C->Sanity = 30.f;
		}
		return true;
	});
	Add(TEXT("v4.9 Tab : jauges basses"), 0.6f, []()
	{
		return true;
	});
	Add(TEXT("v4.9 Tab : capture des jauges basses"), 0.2f, [this]()
	{
		Shot(TEXT("V49_jauges_basses"));
		return true;
	});
	Add(TEXT("v4.9 Tab : fermeture"), 0.5f, [this]()
	{
		if (ABRCharacter* C = GetPlayer())
		{
			C->Stamina = 100.f;
			C->Sanity = 100.f;
		}
		if (ABRPlayerController* PC = GetPC())
		{
			PC->OnInventory(FInputActionValue());
		}
		return true;
	});
	Add(TEXT("v4.9 Tab : ferme"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = HudOf(PC);
		if (!H)
		{
			return true;
		}
		Note(FString::Printf(TEXT("Tab de nouveau : inventaire ferme %s, jauges %d (attendu : 0)"), YesNo(!PC->IsInventoryOpen()), H->LastFrameStatusGauges),
			PC->IsInventoryOpen() || H->LastFrameStatusGauges != 0);
		return true;
	});
	Add(TEXT("v4.9 Tab : coup recu inventaire ouvert"), 0.5f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!PC || !C)
		{
			return true;
		}
		// L'inventaire n'est ni une pause ni une protection : le coup porte, l'ecran reste coherent
		PC->SetInventoryOpen(true);
		C->bGodMode = false;
		C->Health = 100.f;
		C->ReceiveAttack(15.f, 0.f, nullptr);
		return true;
	});
	Add(TEXT("v4.9 Tab : apres le coup"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!PC || !C)
		{
			return true;
		}
		const float Health = C->Health;
		const bool bOpen = PC->IsInventoryOpen();
		Note(FString::Printf(TEXT("coup de 15 inventaire ouvert : sante %.0f (attendu : moins de 100, sans barre affichee), inventaire toujours ouvert %s"), Health, YesNo(bOpen)),
			Health >= 100.f || !bOpen);
		C->Health = 100.f;
		C->bGodMode = true;
		PC->SetInventoryOpen(false);
		return true;
	});
	Add(TEXT("v4.9 Tab : pendant la reaffectation d'une touche"), 0.3f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return true;
		}
		// La touche de l'inventaire pressee pendant la capture sert a la reaffectation : l'ecran des touches reste ouvert
		PC->SetInventoryOpen(true, 3);
		PC->BeginKeyCapture(static_cast<int32>(EBRAction::Inventory), 1);
		PC->OnInventory(FInputActionValue());
		const bool bStillOpen = PC->IsInventoryOpen();
		const bool bStillCapturing = PC->IsCapturingKey();
		Note(FString::Printf(TEXT("Tab pendant la reaffectation : ecran des touches toujours ouvert %s, capture toujours en cours %s"), YesNo(bStillOpen), YesNo(bStillCapturing)),
			!bStillOpen || !bStillCapturing);
		PC->CancelKeyCapture();
		PC->SetInventoryOpen(false);
		return true;
	});
	Add(TEXT("v4.9 Tab : touche reaffectee"), 0.4f, [this]()
	{
		TestKeys.Reset();
		for (int32 Slot = 0; Slot < BRKeys::SlotsPerAction; ++Slot)
		{
			TestKeys.Add(BRKeys::GetKey(EBRAction::Inventory, Slot));
		}
		BRKeys::SetKey(EBRAction::Inventory, 0, EKeys::K); // K : libre par defaut
		return true;
	});
	Add(TEXT("v4.9 Tab : commandes reconstruites"), 0.4f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return true;
		}
		const bool bKey = BRKeys::GetKey(EBRAction::Inventory, 0) == EKeys::K;
		const bool bRebuilt = PC->MappingRevision == BRKeys::Revision();
		PC->OnInventory(FInputActionValue());
		const bool bOpens = PC->IsInventoryOpen();
		PC->OnInventory(FInputActionValue());
		const bool bCloses = !PC->IsInventoryOpen();
		Note(FString::Printf(TEXT("inventaire reaffecte a K : touche enregistree %s, commandes reconstruites %s, ouverture %s, fermeture %s"), YesNo(bKey), YesNo(bRebuilt), YesNo(bOpens),
			YesNo(bCloses)), !bKey || !bRebuilt || !bOpens || !bCloses);
		for (int32 Slot = 0; Slot < TestKeys.Num(); ++Slot)
		{
			BRKeys::SetKey(EBRAction::Inventory, Slot, TestKeys[Slot]);
		}
		TestKeys.Reset();
		return true;
	});
	Add(TEXT("v4.9 Tab : sprint purge"), 0.5f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!PC || !C)
		{
			return true;
		}
		C->SetSprinting(true);
		PC->SetInventoryOpen(true);
		const bool bStopped = !C->IsSprinting();
		PC->SetInventoryOpen(false);
		Note(FString::Printf(TEXT("ouverture de l'inventaire en courant : sprint arrete %s"), YesNo(bStopped)), !bStopped);
		return true;
	});
	Add(TEXT("v4.9 Tab : pas de reprise toute seule"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		if (!C)
		{
			return true;
		}
		// Touches tenues purgees a la fermeture (FlushPressedKeys) : la course ne reprend qu'avec un nouvel appui
		Note(FString::Printf(TEXT("apres la fermeture : sprint repris tout seul : %s (attendu : non ; l'appui physique tenu se verifie a la main)"),
			C->IsSprinting() ? TEXT("OUI") : TEXT("non")), C->IsSprinting());
		return true;
	});
	Add(TEXT("v4.9 HUD : parametres du menu titre"), 0.8f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return true;
		}
		bWasInMenu = PC->bInMenu;
		PC->bInMenu = true;
		PC->SetMenuPage(EBRMenuPage::Main);
		PC->UpdateInputMode();
		PC->SetInventoryOpen(true, 0);
		return true;
	});
	Add(TEXT("v4.9 HUD : sans personnage fictif"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = HudOf(PC);
		if (!H)
		{
			return true;
		}
		// Au menu titre, ni onglet Personnage ni Journal : seulement les parametres et les touches, sans jauges
		const bool bNoCharacter = H->Tab != ABRHUD::ETab::Character && H->Tab != ABRHUD::ETab::Journal;
		Note(FString::Printf(TEXT("parametres depuis le menu titre : onglet %d (2 parametres, 3 touches), jauges %d (attendu : 0)"), static_cast<int32>(H->Tab),
			H->LastFrameStatusGauges), !bNoCharacter || H->LastFrameStatusGauges != 0);
		Shot(TEXT("V49_menu_parametres"));
		return true;
	});
	Add(TEXT("v4.9 HUD : retour en partie"), 0.4f, [this]()
	{
		if (ABRPlayerController* PC = GetPC())
		{
			PC->SetInventoryOpen(false);
			PC->bInMenu = bWasInMenu;
			PC->SetMenuPage(EBRMenuPage::Main);
			PC->UpdateInputMode();
		}
		return true;
	});
	Add(TEXT("v4.9 HUD : piles faibles"), 0.6f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		if (!C)
		{
			return true;
		}
		bTestFlashlight = C->IsFlashlightOn();
		if (!bTestFlashlight)
		{
			C->ToggleFlashlight();
		}
		C->bLowBatteryWarned = false;
		C->Battery = 14.5f;
		return true;
	});
	Add(TEXT("v4.9 HUD : avertissement des piles"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		ABRHUD* H = HudOf(GetPC());
		if (!C || !H)
		{
			return true;
		}
		Note(FString::Printf(TEXT("piles a %.0f %% : avertissement unique affiche %s, jauges %d (attendu : 0 ; la charge n'est plus une jauge)"), C->Battery, YesNo(C->bLowBatteryWarned),
			H->LastFrameStatusGauges), !C->bLowBatteryWarned || H->LastFrameStatusGauges != 0);
		Shot(TEXT("V49_piles_faibles"));
		return true;
	});
	Add(TEXT("v4.9 HUD : piles retablies"), 0.2f, [this]()
	{
		if (ABRCharacter* C = GetPlayer())
		{
			C->Battery = 100.f;
			if (C->IsFlashlightOn() != bTestFlashlight)
			{
				C->ToggleFlashlight();
			}
		}
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Interface d'exploration
	struct FV49Ui
	{
		const TCHAR* Name;
		int32 Mode;      // objets rapides et objectifs : 0 brievement, 1 toujours, 2 masques
		int32 Crosshair; // 0 point, 1 sur les objets, 2 aucun
		float Opacity;
		int32 Expect;    // ExploreDrawn attendu
	};
	const FV49Ui Uis[] = { { TEXT("toujours"), 1, 0, 0.4f, 7 }, { TEXT("masques"), 2, 2, 1.f, 0 } };
	for (const FV49Ui& U : Uis)
	{
		const FV49Ui Case = U;
		Add(FString::Printf(TEXT("v4.9 interface : %s"), Case.Name), 0.6f, [this, Case]()
		{
			ABRPlayerController* PC = GetPC();
			if (!PC)
			{
				return true;
			}
			FBRSettings& S = FBRSettings::Get();
			S.QuickBarMode = Case.Mode;
			S.ObjectivesMode = Case.Mode;
			S.CrosshairMode = Case.Crosshair;
			S.HudOpacity = Case.Opacity;
			PC->ApplySettings();
			return true;
		});
		Add(FString::Printf(TEXT("v4.9 interface : verification %s"), Case.Name), 0.f, [this, Case]()
		{
			ABRHUD* H = HudOf(GetPC());
			if (!H)
			{
				return true;
			}
			const int32 E = H->LastFrameExplore;
			Note(FString::Printf(TEXT("interface %s (opacite %.0f %%) : %s a l'ecran (attendu : %s), jauges %d (attendu : 0)"), Case.Name, Case.Opacity * 100.f, *ExploreText(E),
				*ExploreText(Case.Expect), H->LastFrameStatusGauges), E != Case.Expect || H->LastFrameStatusGauges != 0);
			Shot(FString::Printf(TEXT("V49_interface_%s"), Case.Name));
			return true;
		});
	}
	Add(TEXT("v4.9 interface : brievement"), 0.6f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = HudOf(PC);
		if (!H)
		{
			return true;
		}
		FBRSettings& S = FBRSettings::Get();
		S.QuickBarMode = 0;
		S.ObjectivesMode = 0;
		S.CrosshairMode = 1;
		S.HudOpacity = 1.f;
		PC->ApplySettings();
		// Derniers changements anciens : rien ne doit s'afficher tant que rien ne change
		H->QuickBarShown = -100.f;
		H->ObjectiveShown = -100.f;
		return true;
	});
	Add(TEXT("v4.9 interface : sans changement"), 0.f, [this]()
	{
		ABRHUD* H = HudOf(GetPC());
		ABRCharacter* C = GetPlayer();
		if (!H || !C)
		{
			return true;
		}
		const int32 E = H->LastFrameExplore;
		const bool bFocus = !C->GetFocusPrompt().IsEmpty();
		const bool bOk = (E & 3) == 0 && ((E & 4) != 0) == bFocus;
		Note(FString::Printf(TEXT("interface breve, rien de nouveau : %s a l'ecran (attendu : rien, point seulement sur un objet utilisable : %s)"), *ExploreText(E), YesNo(bFocus)), !bOk);
		// Une poche change (ramassage) : les objets rapides doivent apparaitre
		if (C->Pockets.IsValidIndex(ABRCharacter::NumPockets - 1))
		{
			FBRItemSlot& Last = C->Pockets[ABRCharacter::NumPockets - 1];
			TestPocket = Last;
			if (Last.Item == EBRItem::EnergyBar)
			{
				++Last.Count;
			}
			else
			{
				Last.Item = EBRItem::EnergyBar;
				Last.Count = 1;
			}
		}
		return true;
	});
	Add(TEXT("v4.9 interface : apres un ramassage"), 0.4f, []()
	{
		return true;
	});
	Add(TEXT("v4.9 interface : objets rapides montres"), 4.6f, [this]()
	{
		ABRHUD* H = HudOf(GetPC());
		if (!H)
		{
			return true;
		}
		const bool bShown = (H->LastFrameExplore & 1) != 0;
		Note(FString::Printf(TEXT("ramassage : objets rapides affiches %s"), YesNo(bShown)), !bShown);
		Shot(TEXT("V49_objets_rapides_brefs"));
		return true;
	});
	Add(TEXT("v4.9 interface : objets rapides caches ensuite"), 0.f, [this]()
	{
		ABRHUD* H = HudOf(GetPC());
		ABRCharacter* C = GetPlayer();
		if (!H || !C)
		{
			return true;
		}
		const bool bQuickHidden = (H->LastFrameExplore & 1) == 0;
		Note(FString::Printf(TEXT("5 s plus tard : objets rapides caches %s"), YesNo(bQuickHidden)), !bQuickHidden);
		if (C->Pockets.IsValidIndex(ABRCharacter::NumPockets - 1))
		{
			C->Pockets[ABRCharacter::NumPockets - 1] = TestPocket;
		}
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Taille de l'interface
	struct FV49Scale
	{
		float Scale;
		const TCHAR* Lang;
		const TCHAR* Name;
	};
	const FV49Scale Scales[] = { { 0.8f, nullptr, TEXT("080") }, { 1.25f, TEXT("de"), TEXT("125_de") }, { 1.f, TEXT("ar"), TEXT("100_ar") } };
	for (const FV49Scale& Sc : Scales)
	{
		const FV49Scale Case = Sc;
		Add(FString::Printf(TEXT("v4.9 taille : %s"), Case.Name), 0.8f, [this, Case]()
		{
			ABRPlayerController* PC = GetPC();
			ABRHUD* H = HudOf(PC);
			if (!H)
			{
				return true;
			}
			FBRSettings::Get().UiScale = Case.Scale;
			PC->ApplySettings();
			if (Case.Lang)
			{
				BRLoc::SetLanguage(Case.Lang);
			}
			PC->SetInventoryOpen(true, 2);
			H->SettingsCategory = 2;
			return true;
		});
		Add(FString::Printf(TEXT("v4.9 taille : verification %s"), Case.Name), 0.f, [this, Case]()
		{
			ABRPlayerController* PC = GetPC();
			ABRHUD* H = HudOf(PC);
			if (!H)
			{
				return true;
			}
			const FIntPoint V = ViewportSize();
			const bool bInside = V.X <= 0 || (H->IX >= -1.f && H->IY >= -1.f && H->IX + H->IW <= V.X + 1.f && H->IY + H->IH <= V.Y + 1.f);
			int32 Outside = 0;
			for (const ABRHUD::FButton& B : H->Buttons)
			{
				if (B.X < -1.f || B.Y < -1.f || B.X + B.W > V.X + 1.f || B.Y + B.H > V.Y + 1.f)
				{
					++Outside;
				}
			}
			Note(FString::Printf(TEXT("taille %.2f (%s) : ecran %dx%d, inventaire %.0fx%.0f en (%.0f, %.0f) dans l'ecran %s, %d bouton(s) dont %d hors de l'ecran, jauges %d"),
				Case.Scale, BRLoc::Current().Code, V.X, V.Y, H->IW, H->IH, H->IX, H->IY, YesNo(bInside), H->Buttons.Num(), Outside, H->LastFrameStatusGauges),
				!bInside || Outside > 0 || H->LastFrameStatusGauges != 0);
			Shot(FString::Printf(TEXT("V49_taille_%s"), Case.Name));
			return true;
		});
		Add(FString::Printf(TEXT("v4.9 taille : fermeture %s"), Case.Name), 0.2f, [this]()
		{
			if (ABRPlayerController* PC = GetPC())
			{
				PC->SetInventoryOpen(false);
			}
			return true;
		});
	}
	Add(TEXT("v4.9 taille : retablie"), 0.f, [this]()
	{
		FBRSettings::Get().UiScale = bSettingsSaved ? SavedSettings.UiScale : 1.f;
		if (!TestLanguage.IsEmpty())
		{
			BRLoc::SetLanguage(TestLanguage);
		}
		if (ABRPlayerController* PC = GetPC())
		{
			PC->ApplySettings();
		}
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Parametres
	Add(TEXT("v4.9 parametres : lignes"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return true;
		}
		int32 PerCat[4] = { 0, 0, 0, 0 };
		TArray<FString> Bad;
		for (int32 i = 0; i < PC->GetSettingsCount(); ++i)
		{
			const int32 Cat = PC->GetSettingCategory(i);
			if (Cat < 0 || Cat > 3)
			{
				Bad.Add(FString::Printf(TEXT("ligne %d : categorie %d"), i, Cat));
				continue;
			}
			++PerCat[Cat];
			if (PC->GetSettingLabel(i).IsEmpty())
			{
				Bad.Add(FString::Printf(TEXT("ligne %d : sans libelle"), i));
			}
		}
		for (const TCHAR* Name : { TEXT("Resolution"), TEXT("CreatureReflections"), TEXT("UiScale"), TEXT("HudOpacity"), TEXT("Crosshair"), TEXT("QuickBar"), TEXT("Objectives") })
		{
			const int32 Row = ABRPlayerController::FindSettingRow(Name);
			if (Row == INDEX_NONE || PC->GetSettingValue(Row).IsEmpty() || PC->GetSettingHint(Row).IsEmpty())
			{
				Bad.Add(FString::Printf(TEXT("%s : sans valeur ou sans aide"), Name));
			}
		}
		if (PerCat[2] != 5)
		{
			Bad.Add(FString::Printf(TEXT("INTERFACE : %d ligne(s) au lieu de 5"), PerCat[2]));
		}
		Note(FString::Printf(TEXT("parametres : %d lignes (jeu %d, video %d, interface %d, graphismes %d)%s%s"), PC->GetSettingsCount(), PerCat[0], PerCat[1], PerCat[2], PerCat[3],
			Bad.Num() > 0 ? TEXT(" ; ") : TEXT(""), *FString::Join(Bad, TEXT(", "))), Bad.Num() > 0);
		return true;
	});
	Add(TEXT("v4.9 parametres : ray tracing indisponible"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return true;
		}
		if (ABRPlayerController::IsHardwareRayTracingAvailable())
		{
			Note(TEXT("ray tracing materiel actif sur cette machine : reglages RT modifiables (blocage verifie sur une machine ou un RHI sans RT)"));
			return true;
		}
		FBRSettings& S = FBRSettings::Get();
		const bool bBefore = S.bHardwareRT;
		PC->AdjustSetting(ABRPlayerController::FindSettingRow(TEXT("HardwareRT")), 1);
		const bool bSame = S.bHardwareRT == bBefore;
		const FString Reason = ABRPlayerController::RayTracingUnavailableReason();
		Note(FString::Printf(TEXT("ray tracing materiel indisponible : reglage inchange %s ; raison affichee : %s"), YesNo(bSame), Reason.IsEmpty() ? TEXT("AUCUNE") : *Reason),
			!bSame || Reason.IsEmpty());
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Affichage
	Add(TEXT("v4.9 affichage : etat"), 0.f, [this]()
	{
		const BRDisplay::FMode Cur = BRDisplay::Current();
		const BRDisplay::FMode Eff = BRDisplay::Effective();
		TestWindow = Cur.Window;
		TestResolution = Cur.Resolution;
		bDisplayTestable = BRDisplay::CanChange() && !BRDisplay::IsPending();
		Note(FString::Printf(TEXT("affichage : demande %s, obtenu %s ; plein ecran exclusif sur cette plateforme : %s ; resolutions proposees : plein ecran %d, sans bordures %d, fenetre %d"),
			*ModeText(Cur), *ModeText(Eff), YesNo(BRDisplay::PlatformHasExclusiveFullscreen()), BRDisplay::ResolutionsFor(0).Num(), BRDisplay::ResolutionsFor(1).Num(),
			BRDisplay::ResolutionsFor(2).Num()));
		if (!BRDisplay::CanChange())
		{
			Note(TEXT("affichage : editeur : essai, retour, confirmation et expiration non testes (jeu lance seul : -game ou paquet, avec -BRAutoTest -BRAutoTestV49)"));
		}
		return true;
	});
	Add(TEXT("v4.9 affichage : essai"), 1.5f, [this]()
	{
		if (!bDisplayTestable)
		{
			return true;
		}
		const FIntPoint Trial = TrialResolution();
		const bool bOk = BRDisplay::Request(2, Trial);
		Note(FString::Printf(TEXT("essai : fenetre %dx%d demandee : acceptee %s, en attente de confirmation %s"), Trial.X, Trial.Y, YesNo(bOk), YesNo(BRDisplay::IsPending())),
			!bOk || !BRDisplay::IsPending());
		return true;
	});
	Add(TEXT("v4.9 affichage : retour"), 0.2f, [this]()
	{
		if (!bDisplayTestable)
		{
			return true;
		}
		const BRDisplay::FMode Cur = BRDisplay::Current();
		const BRDisplay::FMode Eff = BRDisplay::Effective();
		const bool bOk = Cur.Window == 2 && Cur.Resolution == TrialResolution();
		Note(FString::Printf(TEXT("pendant l'essai : demande %s, obtenu %s, %.0f s pour confirmer"), *ModeText(Cur), *ModeText(Eff), BRDisplay::SecondsLeft()), !bOk);
		Shot(TEXT("V49_affichage_confirmation"));
		return true;
	});
	Add(TEXT("v4.9 affichage : RETABLIR"), 1.5f, [this]()
	{
		if (bDisplayTestable)
		{
			BRDisplay::Revert();
		}
		return true;
	});
	Add(TEXT("v4.9 affichage : confirmation"), 1.5f, [this]()
	{
		if (!bDisplayTestable)
		{
			return true;
		}
		const BRDisplay::FMode Cur = BRDisplay::Current();
		const bool bOk = !BRDisplay::IsPending() && Cur.Window == TestWindow && Cur.Resolution == TestResolution;
		Note(FString::Printf(TEXT("RETABLIR : retour a %s %s (obtenu %s)"), *ModeText(Cur), YesNo(bOk), *ModeText(BRDisplay::Effective())), !bOk);
		BRDisplay::Request(2, TrialResolution());
		BRDisplay::Confirm();
		return true;
	});
	Add(TEXT("v4.9 affichage : confirme"), 1.5f, [this]()
	{
		if (!bDisplayTestable)
		{
			return true;
		}
		const BRDisplay::FMode Cur = BRDisplay::Current();
		const bool bOk = !BRDisplay::IsPending() && Cur.Window == 2 && Cur.Resolution == TrialResolution();
		Note(FString::Printf(TEXT("CONSERVER : %s garde, plus d'attente %s"), *ModeText(Cur), YesNo(bOk)), !bOk);
		// Retour a l'affichage du joueur, confirme
		BRDisplay::Request(TestWindow, TestResolution);
		BRDisplay::Confirm();
		return true;
	});
	Add(TEXT("v4.9 affichage : profil graphique"), 1.f, [this]()
	{
		if (!bDisplayTestable)
		{
			return true;
		}
		const BRDisplay::FMode Cur = BRDisplay::Current();
		const bool bOk = Cur.Window == TestWindow && Cur.Resolution == TestResolution;
		Note(FString::Printf(TEXT("affichage du joueur retabli : %s %s"), *ModeText(Cur), YesNo(bOk)), !bOk);
		// Un profil graphique ne touche ni au mode ni a la resolution de la fenetre
		ABRPlayerController::ApplyGraphicsProfile(0);
		if (ABRPlayerController* PC = GetPC())
		{
			PC->ApplySettings();
		}
		return true;
	});
	Add(TEXT("v4.9 affichage : expiration"), BRDisplay::ConfirmSeconds + 2.f, [this]()
	{
		if (!bDisplayTestable)
		{
			return true;
		}
		const BRDisplay::FMode Cur = BRDisplay::Current();
		const bool bOk = !BRDisplay::IsPending() && Cur.Window == TestWindow && Cur.Resolution == TestResolution;
		Note(FString::Printf(TEXT("profil PERFORMANCE applique : affichage inchange %s (%s)"), YesNo(bOk), *ModeText(Cur)), !bOk);
		// Sans reponse pendant 15 s : retour automatique
		BRDisplay::Request(2, TrialResolution());
		return true;
	});
	Add(TEXT("v4.9 affichage : apres l'expiration"), 0.f, [this]()
	{
		if (!bDisplayTestable)
		{
			return true;
		}
		const BRDisplay::FMode Cur = BRDisplay::Current();
		const bool bOk = !BRDisplay::IsPending() && Cur.Window == TestWindow && Cur.Resolution == TestResolution;
		Note(FString::Printf(TEXT("sans confirmation en %.0f s : retour automatique a %s %s"), BRDisplay::ConfirmSeconds, *ModeText(Cur), YesNo(bOk)), !bOk);
		if (BRDisplay::IsPending())
		{
			BRDisplay::Revert();
		}
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Rendu
	Add(TEXT("v4.9 creatures : apparition"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || !GetPlayer())
		{
			return false;
		}
		TestRotation = PC->GetControlRotation();
		FVector Spot;
		const FBREntityInfo& Info = ABREntity::Info(EBREntityKind::Hound);
		if (FindSpotInFront(900.f, Info.HalfHeight + 5.f, Spot))
		{
			TestEntity = W->SpawnEntity(EBREntityKind::Hound, Spot);
		}
		if (ABREntity* E = TestEntity.Get())
		{
			E->CustomTimeDilation = 0.05f; // reste sur place, son Tick continue a chaque image
		}
		else
		{
			Note(TEXT("creatures : pas de place pour le Hound devant le joueur"), true);
		}
		return true;
	});
	auto CheckCadence = [this](const TCHAR* When)
	{
		ABREntity* E = TestEntity.Get();
		ABRCharacter* C = GetPlayer();
		if (!E || !C)
		{
			return;
		}
		TInlineComponentArray<UPoseableMeshComponent*> Skins(E);
		const UPoseableMeshComponent* Skin = Skins.Num() > 0 ? Skins[0] : nullptr;
		const float Dist = static_cast<float>(FVector::Dist(C->GetActorLocation(), E->GetActorLocation()));
		const bool bEveryFrame = E->PrimaryActorTick.TickInterval <= 0.f;
		if (!Skin)
		{
			Note(FString::Printf(TEXT("Hound %s : pieces rigides (pas de pose a cadencer), decisions a chaque image %s"), When, YesNo(bEveryFrame)), !bEveryFrame);
			return;
		}
		const bool bSeen = Skin->WasRecentlyRendered(0.25f);
		const float Expected = ExpectedSkinPeriod(bSeen, Dist);
		const float Got = E->GetSkinPeriod();
		const bool bOk = FMath::IsNearlyEqual(Got, Expected, 0.001f) && bEveryFrame && (Dist >= 1500.f || Got == 0.f);
		Note(FString::Printf(TEXT("Hound %s a %.1f m (%s) : pose toutes les %.3f s (attendu %.3f ; pleine cadence a moins de 15 m, a l'ecran ou non), decisions a chaque image %s"),
			When, Dist / 100.f, bSeen ? TEXT("a l'ecran") : TEXT("hors champ"), Got, Expected, YesNo(bEveryFrame)), !bOk);
	};
	Add(TEXT("v4.9 creatures : de face"), 0.2f, [this, CheckCadence]()
	{
		CheckCadence(TEXT("de face"));
		Shot(TEXT("V49_creature_de_face"));
		return true;
	});
	Add(TEXT("v4.9 creatures : demi-tour"), 1.f, [this]()
	{
		if (ABRPlayerController* PC = GetPC())
		{
			PC->SetControlRotation(TestRotation + FRotator(0.f, 180.f, 0.f));
		}
		return true;
	});
	Add(TEXT("v4.9 creatures : de dos"), 0.f, [this, CheckCadence]()
	{
		// Hors de la vue principale mais encore visible dans un reflet ou par son ombre : pleine cadence si proche
		CheckCadence(TEXT("dans le dos"));
		if (ABRPlayerController* PC = GetPC())
		{
			PC->SetControlRotation(TestRotation);
		}
		if (ABREntity* E = TestEntity.Get())
		{
			E->Destroy();
		}
		TestEntity.Reset();
		return true;
	});
	Add(TEXT("v4.9 reflets : creatures"), 0.f, [this]()
	{
		bool bRetraceCVar = false;
		bool bSkCVar = false;
		const int32 Retrace = CVarInt(TEXT("r.Lumen.Reflections.HardwareRayTracing.Retrace.HitLighting"), bRetraceCVar);
		const int32 Sk = CVarInt(TEXT("r.RayTracing.Geometry.SkeletalMeshes"), bSkCVar);
		const FBRSettings& S = FBRSettings::Get();
		// Meme regle que ABRPlayerController::ApplySettings
		const bool bRT = S.bHardwareRT && ABRPlayerController::IsHardwareRayTracingAvailable();
		const bool bHitLighting = bRT && S.bRTHitLighting;
		const bool bRetrace = bRT && !bHitLighting && S.CreatureReflections == 1 && bRetraceCVar;
		const int32 WantSk = (bHitLighting || (S.bRTShadows && bRT) || bRetrace) ? 1 : 0;
		const bool bOk = !bSkCVar || Sk == WantSk;
		Note(FString::Printf(TEXT("creatures dans les reflets : reglage %d, ray tracing materiel %s, reprise des rayons dans ce moteur : %s (valeur %d), maillages a squelette dans la scene RT : %d (attendu %d)"),
			S.CreatureReflections, bRT ? TEXT("actif") : TEXT("inactif"), bRetraceCVar ? TEXT("oui") : TEXT("non : ECRAN (MOTEUR)"), Retrace, Sk, WantSk), !bOk);
		return true;
	});
	Add(TEXT("v4.9 lumieres : changement de type"), 3.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC)
		{
			return false;
		}
		FBRSettings& S = FBRSettings::Get();
		bTestAreaLights = S.bAreaLights;
		W->ResetChunkStats();
		FLevelReport R;
		R.Level = W->GetLevelNumber();
		R.Title = TEXT("v4.9 : lumieres changees de type (mesure de 3 s)");
		R.Scene = TEXT("v4.9_lumieres");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		S.bAreaLights = !bTestAreaLights;
		PC->ApplySettings();
		StartMeasure();
		return true;
	});
	Add(TEXT("v4.9 lumieres : mesure"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return true;
		}
		// Lus avant EndMeasure (qui remet les compteurs a zero)
		const int32 Recreated = W->LightsRecreated;
		const float MaxMs = W->MaxLightRefreshMs;
		const float Slice = FMath::Max(0.25f, static_cast<float>(W->FrameBudgetMs) * 0.25f);
		EndMeasure(Report());
		const bool bArea = FBRSettings::Get().bAreaLights;
		int32 Ready = 0;
		int32 Mismatch = 0;
		for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
		{
			if (!It->IsReady() || It->IsTearingDown())
			{
				continue;
			}
			++Ready;
			Mismatch += It->LightTypesMatch(bArea) ? 0 : 1;
		}
		const bool bOk = Mismatch == 0 && MaxMs <= Slice + 4.f;
		Note(FString::Printf(TEXT("lumieres %s : %d recreee(s) en 3 s, pire tranche %.2f ms (tranche prevue %.2f ms, au moins une lumiere), %d chunk(s) construits dont %d pas encore a jour"),
			bArea ? TEXT("surfaciques") : TEXT("ponctuelles"), Recreated, MaxMs, Slice, Ready, Mismatch), !bOk);
		FBRSettings::Get().bAreaLights = bTestAreaLights;
		if (ABRPlayerController* PC = GetPC())
		{
			PC->ApplySettings();
		}
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Prechargement
	Add(TEXT("v4.9 prechargement : Niveau 0"), 0.f, [this]()
	{
		if (!UBRAssets::IsPreloadDone() && StepTime < 30.f)
		{
			return false;
		}
		// v4.10 : un prechargement inacheve en 30 s est un echec, pas une etape qu'on passe
		if (!UBRAssets::IsPreloadDone())
		{
			Note(TEXT("prechargement du Niveau 0 non termine en 30 s"), true);
		}
		TestSets = UBRAssets::PreloadedSetNames();
		const int32 Catalog = UBRAssets::PreloadCatalogCount();
		if (Catalog == 0)
		{
			// v4.10 : catalogue vide : jamais presente comme une reussite
			Skip(TEXT("prechargement : catalogue vide (aucune ressource importee) : ensembles non verifies"));
			return true;
		}
		const bool bAll = FParse::Param(FCommandLine::Get(), TEXT("BRPreloadAll"));
		// v4.10 : cle construite par la meme fonction que le catalogue (avant : "entity:Smiler" ici, "entity:0" dans le catalogue)
		const FString SmilerKey = UBRAssets::EntitySetKey(EBREntityKind::Smiler);
		const int32 SmilerSize = UBRAssets::CatalogSetSize(SmilerKey);
		if (SmilerSize == 0)
		{
			Note(FString::Printf(TEXT("ensemble %s vide au catalogue : modeles du Smiler absents"), *SmilerKey), true);
		}
		const bool bOk = TestSets.Contains(TEXT("common")) && TestSets.Contains(SmilerKey) && (bAll || TestSets.Num() < Catalog)
			&& UBRAssets::ResidentCount(SmilerKey) == SmilerSize;
		Note(FString::Printf(TEXT("prechargement au Niveau 0 : %d ensemble(s) sur %d au catalogue (attendu : commun, Smiler, textures du niveau et des voisins), Smiler %d/%d en memoire : %s"),
			TestSets.Num(), Catalog, UBRAssets::ResidentCount(SmilerKey), SmilerSize, *FString::Join(TestSets, TEXT(", ")).Left(500)), !bOk);
		for (const FString& Line : UBRAssets::PreloadReport())
		{
			Note(TEXT("  ") + Line);
		}
		return true;
	});
	Add(TEXT("v4.9 transition : inventaire ouvert"), 0.f, [this]()
	{
		// Changement de niveau pendant l'inventaire : il doit se fermer au debut de la transition
		if (ABRPlayerController* PC = GetPC())
		{
			PC->SetInventoryOpen(true);
		}
		return true;
	});
	AddLoad(37, 4.f, TEXT("v4.9 : prechargement (Niveau 37)"));
	Add(TEXT("v4.9 prechargement : Niveau 37"), 0.f, [this]()
	{
		if (!UBRAssets::IsPreloadDone() && StepTime < 30.f)
		{
			return false;
		}
		if (ABRPlayerController* PC = GetPC())
		{
			Note(FString::Printf(TEXT("changement de niveau inventaire ouvert : inventaire ferme a l'arrivee %s"), YesNo(!PC->IsInventoryOpen())), PC->IsInventoryOpen());
		}
		if (!UBRAssets::IsPreloadDone())
		{
			Note(TEXT("prechargement du Niveau 37 non termine en 30 s"), true);
		}
		const TArray<FString> Sets = UBRAssets::PreloadedSetNames();
		if (UBRAssets::PreloadCatalogCount() == 0)
		{
			Skip(TEXT("prechargement au Niveau 37 : catalogue vide"));
			return true;
		}
		const bool bOk = Sets.Contains(TEXT("common"));
		Note(FString::Printf(TEXT("prechargement au Niveau 37 : %d ensemble(s) : %s"), Sets.Num(), *FString::Join(Sets, TEXT(", ")).Left(500)), !bOk);
		for (const FString& Line : UBRAssets::PreloadReport())
		{
			Note(TEXT("  ") + Line);
		}
		return true;
	});
	AddLoad(0, 4.f, TEXT("v4.9 : prechargement (retour au Niveau 0)"));
	Add(TEXT("v4.9 prechargement : retour au Niveau 0"), 0.f, [this]()
	{
		if (!UBRAssets::IsPreloadDone() && StepTime < 30.f)
		{
			return false;
		}
		const TArray<FString> Sets = UBRAssets::PreloadedSetNames();
		if (UBRAssets::PreloadCatalogCount() == 0)
		{
			Skip(TEXT("retour au Niveau 0 : catalogue vide"));
			return true;
		}
		// Les ensembles du Niveau 37 sont relaches : memes ensembles qu'au premier passage (pas d'accumulation)
		const bool bOk = Sets == TestSets;
		Note(FString::Printf(TEXT("retour au Niveau 0 : %d ensemble(s) (premier passage : %d) : memes ensembles %s"), Sets.Num(), TestSets.Num(), YesNo(Sets == TestSets)), !bOk);
		// Objets retenus apres le retour (pas seulement les handles) : test v4.10 (BRAutoTestV410.cpp), sur un niveau dont des
		// ressources ne servent ni au Niveau 0 ni a ses voisins (celles du Niveau 37 y sont toutes prechargees)
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Fin
	Add(TEXT("v4.9 : reglages du joueur retablis"), 0.f, [this]()
	{
		if (bSettingsSaved)
		{
			FBRSettings::Get() = SavedSettings;
			bSettingsSaved = false;
			if (ABRPlayerController* PC = GetPC())
			{
				PC->ApplySettings();
			}
		}
		if (!TestLanguage.IsEmpty())
		{
			BRLoc::SetLanguage(TestLanguage);
		}
		if (ABREntity* E = TestEntity.Get())
		{
			E->Destroy();
		}
		TestEntity.Reset();
		if (BRDisplay::IsPending())
		{
			BRDisplay::Revert();
		}
		Note(FString::Printf(TEXT("reglages, langue (%s), touches et affichage du joueur retablis"), BRLoc::Current().Code));
		return true;
	});
}

void ABRAutoTest::AddNetV49Steps(bool bClient)
{
	// L'hote se place pres du client, puis derriere un mur : son nom s'affiche a vue et disparait derriere le mur (avant :
	// visible a travers les murs et les etages)
	Add(TEXT("Reseau v4.9 : coequipier a vue"), 0.8f, [this, bClient]()
	{
		if (bClient)
		{
			return true;
		}
		ABRWorld* W = GetBRWorld();
		ABRCharacter* Me = GetPlayer();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* Mate = NetMate.IsValid() ? NetMate.Get() : FindMate(GetWorld(), Me);
		if (!W || !Me || !PC || !Mate)
		{
			Note(TEXT("noms : aucun coequipier"), true);
			return true;
		}
		const FVector Head = Mate->GetActorLocation() + FVector(0.f, 0.f, 70.f);
		for (int32 k = 0; k < 8; ++k)
		{
			const FVector P = Mate->GetActorLocation() + FRotator(0.f, k * 45.f, 0.f).Vector() * 300.f;
			if (!W->IsWalkable(W->WorldToCell(P)) || IsBlocked(GetWorld(), P + FVector(0.f, 0.f, Me->BaseEyeHeight), Head, Me, Mate))
			{
				continue;
			}
			Me->SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation((Head - (P + FVector(0.f, 0.f, Me->BaseEyeHeight))).Rotation());
			return true;
		}
		Note(TEXT("noms : pas de place degagee a 3 m du coequipier"));
		return true;
	});
	Add(TEXT("Reseau v4.9 : nom a vue"), 0.f, [this, bClient]()
	{
		if (bClient)
		{
			return true;
		}
		ABRHUD* H = HudOf(GetPC());
		if (!H)
		{
			return true;
		}
		Note(FString::Printf(TEXT("hote : coequipier a 3 m, en face : %d nom(s) affiche(s), %d cache(s) (attendu : au moins 1 affiche)"), H->LastFrameMateNames, H->LastFrameMatesHidden),
			H->LastFrameMateNames < 1);
		Shot(TEXT("Net_v49_nom_a_vue"));
		return true;
	});
	Add(TEXT("Reseau v4.9 : coequipier derriere un mur"), 0.8f, [this, bClient]()
	{
		if (bClient)
		{
			return true;
		}
		ABRWorld* W = GetBRWorld();
		ABRCharacter* Me = GetPlayer();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* Mate = NetMate.IsValid() ? NetMate.Get() : FindMate(GetWorld(), Me);
		bMateBehindWall = false;
		if (!W || !Me || !PC || !Mate)
		{
			return true;
		}
		const FVector MateLoc = Mate->GetActorLocation();
		const FVector Head = MateLoc + FVector(0.f, 0.f, 70.f);
		const FIntPoint Center = W->WorldToCell(MateLoc);
		for (int32 R = 2; R <= 12; ++R)
		{
			for (int32 dx = -R; dx <= R; ++dx)
			{
				for (int32 dy = -R; dy <= R; ++dy)
				{
					if (FMath::Max(FMath::Abs(dx), FMath::Abs(dy)) != R)
					{
						continue;
					}
					const FIntPoint Cell(Center.X + dx, Center.Y + dy);
					if (!W->IsWalkable(Cell))
					{
						continue;
					}
					const FVector P = W->CellCenter(Cell, MateLoc.Z);
					const FVector Eye = P + FVector(0.f, 0.f, Me->BaseEyeHeight);
					if (FVector::Dist(P, MateLoc) > 4500.f || !IsBlocked(GetWorld(), Eye, Head, Me, Mate))
					{
						continue;
					}
					Me->SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
					PC->SetControlRotation((Head - Eye).Rotation());
					bMateBehindWall = true;
					return true;
				}
			}
		}
		Note(TEXT("noms : aucune place derriere un mur trouvee pres du coequipier (verification sautee)"));
		return true;
	});
	Add(TEXT("Reseau v4.9 : nom cache"), 0.2f, [this, bClient]()
	{
		if (bClient)
		{
			return true;
		}
		ABRHUD* H = HudOf(GetPC());
		if (H && bMateBehindWall)
		{
			const bool bOk = H->LastFrameMateNames == 0 && H->LastFrameMatesHidden >= 1;
			Note(FString::Printf(TEXT("hote : coequipier derriere un mur : %d nom(s) affiche(s) (attendu : 0), %d cache(s)"), H->LastFrameMateNames, H->LastFrameMatesHidden), !bOk);
			Shot(TEXT("Net_v49_nom_cache"));
		}
		bMateBehindWall = false;
		return true;
	});
	Add(TEXT("Reseau v4.9 : retour pres du coequipier"), 0.3f, [this, bClient]()
	{
		ABRCharacter* Me = GetPlayer();
		ABRCharacter* Mate = NetMate.IsValid() ? NetMate.Get() : FindMate(GetWorld(), Me);
		if (!bClient && Me && Mate)
		{
			Me->SetActorLocation(Mate->GetActorLocation() + FVector(120.f, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		return true;
	});
}
