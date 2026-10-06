// v4.10 : etapes du test automatique propres a la v4.10 (-BRAutoTest -BRAutoTestV410 ; reseau : -BRNetTest ; paquet :
// -BRAutoTest -BRSmokeTest ; session longue : -BRAutoTest -BRAutoTestSoak=<minutes>).
// Chaque etape verifie un defaut reproduit sur la v4.9 et corrige en v4.10 :
//   - polices : tout le texte en carres dans le jeu lance seul (police du moteur copiee vide) ;
//   - sauvegardes : un fichier abime arretait le jeu (assertion d'Unreal) au lieu d'etre mis de cote ;
//   - prechargement : catalogue vide dans le jeu non empaquete, cle du Smiler differente entre le catalogue et le test,
//     objets des autres niveaux retenus par les caches ;
//   - preparation des niveaux : ressources chargees au premier usage pendant la construction, sans attente, sans
//     delai maximal ni retour possible ; une demande remplacee ne doit jamais etre finalisee ;
//   - confirmation de l'affichage : Tab, deplacements et menus passaient sous le dialogue ;
//   - reseau : soins rapproches et soin au moment d'un coup (objet consomme = soin accepte par le serveur), client
//     ignore par les entites tant qu'il prepare le niveau.
// Une verification qui ne peut pas etre faite ici est notee "NON VERIFIE", jamais comptee comme reussie.
#include "BRAutoTest.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BRDisplay.h"
#include "BRFonts.h"
#include "BRHUD.h"
#include "BRLevels.h"
#include "BRLoc.h"
#include "BRPlayerController.h"
#include "BRSave.h"
#include "BRWorld.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fonts/FontCache.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Rendering/SlateRenderer.h"
#include "RHI.h"
#include "UObject/UObjectArray.h"
#include "Widgets/SWindow.h"
#include "Engine/GameViewportClient.h"

namespace BRTestV410
{
	const TCHAR* YesNo(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	ABRHUD* Hud(ABRPlayerController* PC)
	{
		return PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr;
	}

	/** Caracteres de chaque ecriture de l'interface : chacun doit etre dessinable sans police de dernier recours */
	struct FSample
	{
		const TCHAR* Script;
		const TCHAR* Text;
	};
	const FSample Samples[] = {
		{ TEXT("latin"), TEXT("Pr\u00e9paration \u00c9CHAP 0123456789 %\u2026\u00ab\u00bb") },
		{ TEXT("cyrillique"), TEXT("\u0416\u0438\u0437\u043d\u044c") },
		{ TEXT("arabe"), TEXT("\u0628\u0642\u0627\u0621") },
		{ TEXT("chinois"), TEXT("\u7b80\u4f53\u4e2d\u6587") },
		{ TEXT("japonais"), TEXT("\u3072\u3089\u304c\u306a") },
		{ TEXT("coreen"), TEXT("\ud55c\uad6d\uc5b4") },
	};

	/** Ecritures dont un caractere n'a pas de glyphe dans la police de l'interface (texte en carres) ; false si Slate absent */
	bool MissingGlyphs(TArray<FString>& OutMissing)
	{
		if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
		{
			return false;
		}
		const TSharedRef<FSlateFontCache> Cache = FSlateApplication::Get().GetRenderer()->GetFontCache();
		for (int32 Weight : { 1, 2 })
		{
			const FSlateFontInfo Font = ABRHUD::GetUiFont(14.f, Weight);
			for (const FSample& S : Samples)
			{
				for (const TCHAR* P = S.Text; *P; ++P)
				{
					if (*P == TEXT(' '))
					{
						continue;
					}
					float Scale = 1.f;
					const FFontData& Data = Cache->GetFontDataForCodepoint(Font, static_cast<UTF32CHAR>(*P), Scale);
					if (!Cache->CanLoadCodepoint(Data, static_cast<UTF32CHAR>(*P), EFontFallback::FF_NoFallback))
					{
						OutMissing.AddUnique(FString::Printf(TEXT("%s U+%04X (graisse %d)"), S.Script, static_cast<uint32>(*P), Weight));
					}
				}
			}
			// Nombres, dates et heures formates par la langue courante (espaces fines du francais, de l'anglais...), tels que
			// l'interface les dessine
			const FString Formatted = ABRHUD::FontSafe(BRLoc::Fmt(FText::FromString(TEXT("{A} {B} {C}")), { { TEXT("A"), BRLoc::Int(1234567) }, { TEXT("B"), BRLoc::Num(-3.5, 1) },
				{ TEXT("C"), BRLoc::Arg(FText::AsDateTime(FDateTime(2026, 10, 6, 13, 45, 0), EDateTimeStyle::Short, EDateTimeStyle::Short, FText::GetInvariantTimeZone()).ToString()) } }));
			for (const TCHAR Ch : Formatted)
			{
				if (Ch == TEXT(' '))
				{
					continue;
				}
				float Scale = 1.f;
				const FFontData& Data = Cache->GetFontDataForCodepoint(Font, static_cast<UTF32CHAR>(Ch), Scale);
				if (!Cache->CanLoadCodepoint(Data, static_cast<UTF32CHAR>(Ch), EFontFallback::FF_NoFallback))
				{
					OutMissing.AddUnique(FString::Printf(TEXT("nombre/heure (%s) U+%04X (graisse %d)"), *Formatted, static_cast<uint32>(Ch), Weight));
				}
			}
		}
		return true;
	}

	float RamMB()
	{
		return static_cast<float>(FPlatformMemory::GetStats().UsedPhysical / (1024.0 * 1024.0));
	}

	float TexMB()
	{
		FTextureMemoryStats Tex;
		RHIGetTextureMemoryStats(Tex);
		return static_cast<float>(FMath::Max<int64>(0, Tex.StreamingMemorySize + Tex.NonStreamingMemorySize) / (1024.0 * 1024.0));
	}

	TArray<ABRCharacter*> RemotePlayers(ABRWorld* W)
	{
		TArray<ABRCharacter*> All;
		TArray<ABRCharacter*> Out;
		if (W)
		{
			W->GetPlayers(All);
		}
		for (ABRCharacter* C : All)
		{
			if (C && !C->IsLocallyControlled())
			{
				Out.Add(C);
			}
		}
		return Out;
	}
}

using namespace BRTestV410;

void ABRAutoTest::AddV410Steps()
{
	AddLoad(0, 5.f, TEXT("v4.10 : polices, sauvegardes, prechargement, preparation, affichage (Niveau 0)"));
	Add(TEXT("v4.10 : preparation"), 0.5f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRWorld* W = GetBRWorld();
		if (!PC || !GetPlayer() || !W || !Hud(PC) || W->GetTitleTime() > 0.f)
		{
			return false;
		}
		Report().Scene = TEXT("v4.10");
		// Ecran de la fenetre au lancement (compare apres les essais d'affichage : la fenetre ne doit pas changer d'ecran)
		{
			const BRDisplay::FMonitor M0 = BRDisplay::ActiveMonitor();
			TestWindowMonitor = M0.bKnown ? M0.Index : INDEX_NONE;
			Note(FString::Printf(TEXT("ecran de la fenetre au lancement : %d sur %d%s, %dx%d"), M0.Index + 1, M0.Count, M0.bPrimary ? TEXT(" (principal)") : TEXT(""), M0.Size.X, M0.Size.Y));
		}
		if (!bSettingsSaved)
		{
			SavedSettings = FBRSettings::Get();
			bSettingsSaved = true;
		}
		GetPlayer()->bGodMode = true;
		PC->SetInventoryOpen(false);
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Polices
	Add(TEXT("v4.10 polices : caracteres de chaque ecriture"), 0.f, [this]()
	{
		TArray<FString> Missing;
		if (!MissingGlyphs(Missing))
		{
			Skip(TEXT("polices : Slate absent (serveur dedie ?) : glyphes non verifies"));
			return true;
		}
		const TArray<FString> Files = BRFonts::MissingFiles();
		Note(FString::Printf(TEXT("polices : %d caractere(s) sans glyphe (latin, cyrillique, arabe, chinois, japonais, coreen) %s ; fichiers absents : %s"), Missing.Num(),
			Missing.Num() ? *FString::Join(Missing, TEXT(", ")).Left(400) : TEXT(""), Files.Num() ? *FString::Join(Files, TEXT(", ")) : TEXT("aucun")),
			Missing.Num() > 0 || Files.Num() > 0);
		TestFirstLog = LogLineCount();
		GetPC()->SetInventoryOpen(true, 0);
		return true;
	});
	Add(TEXT("v4.10 polices : inventaire dessine"), 0.8f, [this]()
	{
		return true;
	});
	Add(TEXT("v4.10 polices : capture"), 0.4f, [this]()
	{
		const bool bLastResort = LogContains(TestFirstLog, TEXT("getting last resort font data"));
		Note(FString::Printf(TEXT("inventaire dessine pendant 0,8 s : police de dernier recours utilisee (texte en carres) : %s (attendu : non)"),
			bLastResort ? TEXT("OUI") : TEXT("non")), bLastResort);
		Shot(TEXT("V410_inventaire"));
		return true;
	});
	Add(TEXT("v4.10 polices : fin"), 0.2f, [this]()
	{
		GetPC()->SetInventoryOpen(false);
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Sauvegardes
	Add(TEXT("v4.10 sauvegardes : fichiers abimes"), 0.f, [this]()
	{
		BRSaves::SetTestPrefix(TEXT("BR_AutoTestV410_"));
		const int32 Slot = 3;
		const FString Main = BRSaves::FilePath(BRSaves::SlotName(Slot));
		const FString Backup = BRSaves::FilePath(BRSaves::BackupSlotName(Slot));
		auto Reset = [&]()
		{
			BRSaves::Delete(Slot);
			IFileManager::Get().Delete(*BRSaves::FilePath(BRSaves::UnreadableSlotName(Slot)), false, true, true);
		};
		Reset();
		// Ecriture normale : controle d'integrite a la fin du fichier, relecture identique
		UBRSaveGame* S = NewObject<UBRSaveGame>();
		S->SaveName = TEXT("v410");
		S->PlayTime = 42.f;
		BRSaves::Write(Slot, S);
		TArray<uint8> Good;
		FFileHelper::LoadFileToArray(Good, *Main);
		const bool bTrailer = Good.Num() > 12 && FMemory::Memcmp(Good.GetData() + Good.Num() - 12, "BRSV", 4) == 0;
		const UBRSaveGame* L = BRSaves::Load(Slot);
		const bool bRound = L && FMath::IsNearlyEqual(L->PlayTime, 42.f);
		// Fichier d'une version precedente (sans controle) : toujours lisible
		TArray<uint8> Legacy = Good;
		Legacy.SetNum(FMath::Max(0, Legacy.Num() - 12));
		FFileHelper::SaveArrayToFile(Legacy, *Main);
		FFileHelper::SaveArrayToFile(Legacy, *Backup);
		const UBRSaveGame* LL = BRSaves::Load(Slot);
		const bool bLegacy = LL && FMath::IsNearlyEqual(LL->PlayTime, 42.f);
		// Octet change au milieu (principal et secours) : controle faux, fichier mis de cote, aucun arret
		TArray<uint8> Flipped = Good;
		Flipped[Flipped.Num() / 2] ^= 0xFF;
		FFileHelper::SaveArrayToFile(Flipped, *Main);
		FFileHelper::SaveArrayToFile(Flipped, *Backup);
		const bool bFlipRefused = BRSaves::Load(Slot) == nullptr;
		Reset();
		// Fichier coupe en deux
		TArray<uint8> Half = Good;
		Half.SetNum(Good.Num() / 2);
		FFileHelper::SaveArrayToFile(Half, *Main);
		FFileHelper::SaveArrayToFile(Half, *Backup);
		const bool bHalfRefused = BRSaves::Load(Slot) == nullptr;
		Reset();
		// Octets au hasard (plantage reproduit par le test v4.7 sur la v4.9 : nom de classe de plus de 1024 caracteres)
		TArray<uint8> Garbage;
		Garbage.Init(0x5A, 3000);
		FFileHelper::SaveArrayToFile(Garbage, *Main);
		FFileHelper::SaveArrayToFile(Garbage, *Backup);
		const bool bGarbageRefused = BRSaves::Load(Slot) == nullptr;
		// En-tete valide, donnees remplacees : chaine geante dans le nom de la classe
		TArray<uint8> Forged = Good;
		for (int32 i = Forged.Num() / 3; i < Forged.Num() - 12; ++i)
		{
			Forged[i] = 0x7F;
		}
		Forged.SetNum(Forged.Num() - 12);
		FFileHelper::SaveArrayToFile(Forged, *Main);
		FFileHelper::SaveArrayToFile(Forged, *Backup);
		const bool bForgedRefused = BRSaves::Load(Slot) == nullptr;
		Reset();
		BRSaves::TakeLoadMessages();
		BRSaves::SetTestPrefix(FString());
		const bool bOk = bTrailer && bRound && bLegacy && bFlipRefused && bHalfRefused && bGarbageRefused && bForgedRefused;
		Note(FString::Printf(TEXT("sauvegardes : controle d'integrite ecrit %s, relue %s, fichier v4.9 sans controle lu %s, octet change refuse %s, fichier coupe refuse %s, octets au hasard refuses %s, donnees forgees refusees %s (jeu toujours en marche)"),
			YesNo(bTrailer), YesNo(bRound), YesNo(bLegacy), YesNo(bFlipRefused), YesNo(bHalfRefused), YesNo(bGarbageRefused), YesNo(bForgedRefused)), !bOk);
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Prechargement
	Add(TEXT("v4.10 prechargement : catalogue"), 0.f, [this]()
	{
		if (!UBRAssets::IsPreloadDone() && StepTime < 30.f)
		{
			return false;
		}
		const int32 Catalog = UBRAssets::PreloadCatalogCount();
		if (Catalog == 0)
		{
			Skip(TEXT("catalogue de prechargement vide : aucune ressource importee"));
			return true;
		}
		// Chaque ensemble des niveaux existe au catalogue (cles construites par les memes fonctions)
		TArray<FString> Empty;
		TSet<FString> Requested;
		for (const FBRLevelDef& D : BRLevels::All())
		{
			for (const FString& K : UBRAssets::EssentialSetsForLevel(D.Number))
			{
				Requested.Add(K);
				if (UBRAssets::CatalogSetSize(K) == 0 && !K.StartsWith(TEXT("tex:")))
				{
					Empty.AddUnique(FString::Printf(TEXT("%s (Niveau %d)"), *K, D.Number));
				}
			}
		}
		const FString Smiler = UBRAssets::EntitySetKey(EBREntityKind::Smiler);
		Note(FString::Printf(TEXT("catalogue : %d ensemble(s) ; cle du Smiler %s (%d ressources) ; ensembles demandes par un niveau mais vides : %s"), Catalog, *Smiler,
			UBRAssets::CatalogSetSize(Smiler), Empty.Num() ? *FString::Join(Empty, TEXT(", ")) : TEXT("aucun")), Empty.Num() > 0 || UBRAssets::CatalogSetSize(Smiler) == 0);
		// Decors propres : classes hors de "common" (sinon ils resteraient en memoire dans tous les niveaux)
		Note(FString::Printf(TEXT("decors propres a certains niveaux : SM_Desk -> %s, SM_HotelDoor -> %s, SM_Wheat -> %s, SM_StreetLamp -> %s, SM_LightPanel -> %s"),
			*UBRAssets::SetKeyForAsset(TEXT("SM_Desk"), false), *UBRAssets::SetKeyForAsset(TEXT("SM_HotelDoor"), false), *UBRAssets::SetKeyForAsset(TEXT("SM_Wheat"), false),
			*UBRAssets::SetKeyForAsset(TEXT("SM_StreetLamp"), false), *UBRAssets::SetKeyForAsset(TEXT("SM_LightPanel"), false)),
			UBRAssets::SetKeyForAsset(TEXT("SM_Desk"), false) == TEXT("common"));
		return true;
	});
	Add(TEXT("v4.10 preparation : demande remplacee"), 0.f, [this]()
	{
		// Une demande remplacee n'est jamais consideree comme prete (une transition annulee ne finalise pas l'ancienne)
		UBRAssets::PrepareLevel(37, 900001u);
		UBRAssets::PrepareLevel(4, 900002u);
		const bool bOldRefused = !UBRAssets::IsLevelPrepared(900001u);
		Note(FString::Printf(TEXT("preparation : demande remplacee jamais prete %s"), YesNo(bOldRefused)), !bOldRefused);
		return true;
	});

	// Transition reelle : preparation pendant le fondu, puis construction sans chargement au premier usage
	auto AddTransition = [this](int32 Target, const TCHAR* Label)
	{
		Add(FString::Printf(TEXT("v4.10 preparation : vers le Niveau %d"), Target), 0.f, [this, Target]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W || W->IsTransitioning())
			{
				return false;
			}
			UBRAssets::SyncLoadsInBuild = 0;
			UBRAssets::SyncLoadBuildNames.Reset();
			bTestPrepShown = false;
			TestTimer = FPlatformTime::Seconds();
			W->RequestTransition(Target, false);
			return true;
		});
		Add(FString::Printf(TEXT("v4.10 preparation : arrivee au Niveau %d"), Target), 0.5f, [this, Target, Label]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W)
			{
				return false;
			}
			bTestPrepShown |= W->IsPreparing();
			if ((W->IsTransitioning() || W->GetLevelNumber() != Target) && StepTime < 60.f)
			{
				return false;
			}
			const float Seconds = static_cast<float>(FPlatformTime::Seconds() - TestTimer);
			const bool bArrived = W->GetLevelNumber() == Target && !W->IsTransitioning();
			Note(FString::Printf(TEXT("%s : arrivee %s en %.1f s (ecran de preparation montre : %s) ; chargements synchrones pendant la construction : %d %s"), Label,
				YesNo(bArrived), Seconds, bTestPrepShown ? TEXT("oui") : TEXT("non, tout etait pret"), UBRAssets::SyncLoadsInBuild,
				UBRAssets::SyncLoadBuildNames.Num() ? *FString::Join(UBRAssets::SyncLoadBuildNames, TEXT(", ")).Left(400) : TEXT("")),
				!bArrived || UBRAssets::SyncLoadsInBuild > 0);
			return true;
		});
	};
	AddTransition(37, TEXT("vers les Poolrooms"));
	AddTransition(0, TEXT("retour au Niveau 0"));
	// Memoire : un niveau dont des ressources ne servent ni au Niveau 0 ni a ses voisins (les Poolrooms sont voisines du
	// Niveau 0 : leurs ressources y restent prechargees, a juste titre). Aller-retour, ramasse-miettes, puis plus rien de
	// ces ressources en memoire (caches Loaded et MatCache compris).
	Add(TEXT("v4.10 memoire : ramasse-miettes avant le releve"), 1.f, [this]()
	{
		// Ressources relachees par les etapes precedentes (demande remplacee : Niveau 4) : hors de la memoire avant le releve
		if (GEngine)
		{
			GEngine->ForceGarbageCollection(true);
		}
		return true;
	});
	Add(TEXT("v4.10 memoire : choix du niveau"), 0.f, [this]()
	{
		TArray<FString> Level0 = UBRAssets::EssentialSetsForLevel(0);
		Level0.Append(UBRAssets::NeighborSetsForLevel(0));
		TestLevel = INDEX_NONE;
		int32 Best = 0;
		for (const FBRLevelDef& D : BRLevels::All())
		{
			if (D.Number == 0)
			{
				continue;
			}
			TArray<FString> Only;
			int32 Size = 0;
			for (const FString& K : UBRAssets::EssentialSetsForLevel(D.Number))
			{
				if (!Level0.Contains(K) && UBRAssets::CatalogSetSize(K) > 0)
				{
					Only.Add(K);
					Size += UBRAssets::CatalogSetSize(K);
				}
			}
			if (Size > Best)
			{
				Best = Size;
				TestLevel = D.Number;
				TestOnlySets = Only;
			}
		}
		if (TestLevel == INDEX_NONE)
		{
			Skip(TEXT("memoire : aucun niveau n'a de ressources absentes du Niveau 0 et de ses voisins (catalogue vide ?)"));
			TestLevel = 0;
			TestOnlySets.Reset();
		}
		else
		{
			Note(FString::Printf(TEXT("memoire : Niveau %d choisi (%d ressource(s) hors des ensembles du Niveau 0 : %s)"), TestLevel, Best, *FString::Join(TestOnlySets, TEXT(", "))));
			// Releve avant l'aller : une ressource deja en memoire au Niveau 0 y sert (decor, materiau d'un modele) ; elle ne
			// doit pas etre comptee comme retenue au retour, mais le catalogue ne la classe pas dans le Niveau 0 (a corriger)
			for (const FString& K : TestOnlySets)
			{
				if (UBRAssets::ResidentCount(K) > 0)
				{
					Note(FString::Printf(TEXT("memoire : %s deja en memoire au Niveau 0 avant l'aller (%d/%d) : utilise par le Niveau 0 sans etre dans ses ensembles"), *K,
						UBRAssets::ResidentCount(K), UBRAssets::CatalogSetSize(K)), true);
				}
			}
		}
		return true;
	});
	Add(TEXT("v4.10 memoire : aller"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		if (TestLevel != 0)
		{
			W->RequestTransition(TestLevel, false);
		}
		return true;
	});
	Add(TEXT("v4.10 memoire : arrivee"), 1.f, [this]()
	{
		const ABRWorld* W = GetBRWorld();
		if (W && (W->IsTransitioning() || W->GetLevelNumber() != TestLevel) && StepTime < 60.f)
		{
			return false;
		}
		if (TestLevel != 0)
		{
			int32 Resident = 0;
			for (const FString& K : TestOnlySets)
			{
				Resident += UBRAssets::ResidentCount(K);
			}
			Note(FString::Printf(TEXT("au Niveau %d : %d ressource(s) propres en memoire (attendu : chargees)"), TestLevel, Resident), Resident == 0);
		}
		return true;
	});
	Add(TEXT("v4.10 memoire : retour"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		if (W->GetLevelNumber() != 0)
		{
			W->RequestTransition(0, false);
		}
		return true;
	});
	Add(TEXT("v4.10 memoire : ramasse-miettes"), 1.f, [this]()
	{
		const ABRWorld* W = GetBRWorld();
		if (W && (W->IsTransitioning() || W->GetLevelNumber() != 0) && StepTime < 60.f)
		{
			return false;
		}
		if (GEngine)
		{
			GEngine->ForceGarbageCollection(true);
		}
		return true;
	});
	Add(TEXT("v4.10 memoire : objets retenus"), 1.f, [this]()
	{
		if (TestOnlySets.Num() == 0)
		{
			return true; // deja note NON VERIFIE
		}
		const UBRAssets* A = UBRAssets::Get(this);
		TArray<FString> Still;
		for (const FString& K : TestOnlySets)
		{
			if (UBRAssets::ResidentCount(K) > 0)
			{
				Still.Add(FString::Printf(TEXT("%s (%d/%d)"), *K, UBRAssets::ResidentCount(K), UBRAssets::CatalogSetSize(K)));
			}
		}
		Note(FString::Printf(TEXT("apres le Niveau %d, retour au Niveau 0 et ramasse-miettes : ressources propres encore en memoire : %s ; caches %d ressource(s), %d materiau(x) ; RAM %.0f Mo, textures %.0f Mo"),
			TestLevel, Still.Num() ? *FString::Join(Still, TEXT(", ")) : TEXT("aucune"), A ? A->GetLoadedCount() : -1, A ? A->GetMaterialCacheCount() : -1, RamMB(), TexMB()),
			Still.Num() > 0);
		return true;
	});

	// Preparation lente (simulee) : ecran sobre, retour au menu propose apres 10 s, entree au plus tard a 25 s
	Add(TEXT("v4.10 preparation lente : debut"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		UBRAssets::TestPrepareStallUntil = FPlatformTime::Seconds() + 60.0;
		TestTimer = FPlatformTime::Seconds();
		W->RequestTransition(37, false);
		return true;
	});
	Add(TEXT("v4.10 preparation lente : retour au menu propose"), 0.4f, [this]()
	{
		const ABRWorld* W = GetBRWorld();
		const bool bHeld = W && W->IsPreparing() && W->GetPrepareTime() >= ABRWorld::PrepareMenuDelay + 1.f;
		if (W && W->IsTransitioning() && !bHeld && StepTime < 40.f)
		{
			return false;
		}
		const bool bShown = W && W->IsPreparing() && W->GetPrepareTime() >= ABRWorld::PrepareMenuDelay;
		Note(FString::Printf(TEXT("preparation bloquee : ecran de preparation tenu %.0f s, retour au menu propose : %s"), W ? W->GetPrepareTime() : 0.f, YesNo(bShown)), !bShown);
		Shot(TEXT("V410_preparation_longue"));
		return true;
	});
	Add(TEXT("v4.10 preparation lente : entree au delai maximal"), 0.f, [this]()
	{
		const ABRWorld* W = GetBRWorld();
		if (W && (W->IsTransitioning() || W->GetLevelNumber() != 37) && StepTime < 40.f)
		{
			return false;
		}
		UBRAssets::TestPrepareStallUntil = 0.0;
		const float Seconds = static_cast<float>(FPlatformTime::Seconds() - TestTimer);
		const bool bIn = W && W->GetLevelNumber() == 37 && !W->IsTransitioning();
		Note(FString::Printf(TEXT("preparation jamais terminee : entree dans le niveau apres %.0f s (delai maximal %.0f s + fondus), jamais d'ecran noir sans fin : %s"), Seconds,
			ABRWorld::PrepareMaxSeconds, YesNo(bIn)), !bIn || Seconds > ABRWorld::PrepareMaxSeconds + 8.f);
		return true;
	});
	AddLoad(0, 3.f, TEXT("v4.10 : affichage et confort (Niveau 0)"));

	// ------------------------------------------------------------------------------------------------ Affichage
	Add(TEXT("v4.10 affichage : dialogue sur l'inventaire"), 0.6f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!BRDisplay::CanChange())
		{
			Skip(TEXT("dialogue de l'affichage : fenetre de l'editeur (lancer le jeu seul : -game)"));
			return true;
		}
		TestWindow = BRDisplay::Current().Window;
		TestResolution = BRDisplay::Current().Resolution;
		bDisplayTestable = true;
		PC->SetInventoryOpen(true, 2);
		BRDisplay::Request(2, FIntPoint(1280, 720));
		return true;
	});
	Add(TEXT("v4.10 affichage : Tab et deplacement sous le dialogue"), 0.3f, [this]()
	{
		if (!bDisplayTestable)
		{
			return true;
		}
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		const bool bPending = BRDisplay::IsPending();
		PC->OnInventory(FInputActionValue());
		const bool bStillOpen = PC->IsInventoryOpen();
		const FVector Before = C ? C->GetActorLocation() : FVector::ZeroVector;
		PC->OnMove(FInputActionValue(FVector2D(1.0, 0.0)));
		const bool bCursor = PC->bShowMouseCursor;
		Note(FString::Printf(TEXT("dialogue de l'affichage ouvert sur l'inventaire : en attente %s, Tab ignore (inventaire toujours ouvert) %s, jeu bloque (CanPlay faux) %s, curseur visible %s"),
			YesNo(bPending), YesNo(bStillOpen), YesNo(!PC->CanPlay()), YesNo(bCursor)), !bPending || !bStillOpen || PC->CanPlay() || !bCursor);
		TestSpot = Before;
		Shot(TEXT("V410_affichage_dialogue"));
		return true;
	});
	Add(TEXT("v4.10 affichage : retablir"), 0.8f, [this]()
	{
		if (!bDisplayTestable)
		{
			return true;
		}
		const ABRCharacter* C = GetPlayer();
		const bool bMoved = C && FVector::Dist2D(C->GetActorLocation(), TestSpot) > 5.f;
		Note(FString::Printf(TEXT("pendant le dialogue : le personnage a bouge : %s (attendu : non)"), bMoved ? TEXT("OUI") : TEXT("non")), bMoved);
		BRDisplay::Revert();
		return true;
	});
	Add(TEXT("v4.10 affichage : etat restaure"), 0.f, [this]()
	{
		if (!bDisplayTestable)
		{
			return true;
		}
		ABRPlayerController* PC = GetPC();
		const bool bOk = !BRDisplay::IsPending() && PC->IsInventoryOpen() && PC->bShowMouseCursor;
		Note(FString::Printf(TEXT("apres RETABLIR : dialogue ferme %s, inventaire toujours ouvert %s, curseur visible %s"), YesNo(!BRDisplay::IsPending()), YesNo(PC->IsInventoryOpen()),
			YesNo(PC->bShowMouseCursor)), !bOk);
		PC->SetInventoryOpen(false);
		return true;
	});
	Add(TEXT("v4.10 affichage : dialogue en exploration, coup recu"), 0.5f, [this]()
	{
		if (!bDisplayTestable)
		{
			return true;
		}
		ABRCharacter* C = GetPlayer();
		C->bGodMode = false;
		TestHealth = C->Health;
		TestHits = 0;
		// Reference : l'affichage juste avant cet essai (RETABLIR a pu adapter la taille a la zone utile de l'ecran)
		TestWindow = BRDisplay::Current().Window;
		TestResolution = BRDisplay::Current().Resolution;
		BRDisplay::Request(2, FIntPoint(1280, 720));
		C->ReceiveAttack(10.f, 0.f, nullptr);
		return true;
	});
	Add(TEXT("v4.10 affichage : expiration"), 0.f, [this]()
	{
		if (!bDisplayTestable)
		{
			return true;
		}
		if (BRDisplay::IsPending() && StepTime < 20.f)
		{
			if (TestHits == 0)
			{
				TestHits = 1;
				ABRPlayerController* PC = GetPC();
				ABRCharacter* C = GetPlayer();
				Note(FString::Printf(TEXT("dialogue en exploration : curseur visible %s, jeu bloque %s, le monde continue (coup recu pendant le dialogue : sante %.0f -> %.0f)"),
					YesNo(PC->bShowMouseCursor), YesNo(!PC->CanPlay()), TestHealth, C->Health), !PC->bShowMouseCursor || PC->CanPlay() || C->Health >= TestHealth);
				C->bGodMode = true;
				C->Health = 100.f;
			}
			return false;
		}
		ABRPlayerController* PC = GetPC();
		const BRDisplay::FMode Now = BRDisplay::Current();
		const bool bBack = Now.Window == TestWindow && Now.Resolution == TestResolution;
		Note(FString::Printf(TEXT("sans reponse en 15 s : affichage d'avant %s (%d %dx%d, attendu %d %dx%d), curseur cache %s, jeu rendu %s"), YesNo(bBack), Now.Window, Now.Resolution.X,
			Now.Resolution.Y, TestWindow, TestResolution.X, TestResolution.Y, YesNo(!PC->bShowMouseCursor), YesNo(PC->CanPlay())),
			!bBack || PC->bShowMouseCursor || !PC->CanPlay());
		return true;
	});

	// Ecran de la fenetre : taille, zone utile ; fenetres proposees dans la zone utile ; plusieurs ecrans : chacun reconnu
	// Fenetre deplacee sur un autre ecran (comme le ferait le joueur), puis essai d'affichage et RETABLIR : la fenetre doit
	// rester sur cet ecran
	Add(TEXT("v4.10 affichage : fenetre sur un autre ecran"), 1.f, [this]()
	{
		TestSpot = FVector::ZeroVector;
		const BRDisplay::FMonitor M = BRDisplay::ActiveMonitor();
		TSharedPtr<SWindow> Win = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
		if (!bDisplayTestable || !Win.IsValid() || M.Count < 2 || BRDisplay::Current().Window != 2)
		{
			Skip(TEXT("fenetre sur un autre ecran : un seul ecran, pas de fenetre, ou jeu pas en fenetre"));
			return true;
		}
		FDisplayMetrics Metrics;
		FSlateApplication::Get().GetCachedDisplayMetrics(Metrics);
		for (int32 i = 0; i < Metrics.MonitorInfo.Num(); ++i)
		{
			if (i != M.Index)
			{
				const FPlatformRect& Work = Metrics.MonitorInfo[i].WorkArea;
				const FVector2D Pos = Win->GetPositionInScreen();
				TestSpot = FVector(Pos.X, Pos.Y, 1.f); // position d'origine (retablie a la fin)
				TestOrigWindow = BRDisplay::Current().Window;
				TestOrigResolution = BRDisplay::Current().Resolution;
				TestHits = i;
				Win->MoveWindowTo(FVector2D(Work.Left + 60, Work.Top + 60));
				return true;
			}
		}
		return true;
	});
	Add(TEXT("v4.10 affichage : essai sur l'autre ecran"), 1.5f, [this]()
	{
		if (TestSpot.Z <= 0.f)
		{
			return true;
		}
		const BRDisplay::FMonitor M = BRDisplay::ActiveMonitor();
		Note(FString::Printf(TEXT("fenetre deplacee sur l'ecran %d : ecran reconnu par le jeu %d (%dx%d, zone utile %dx%d)"), TestHits + 1, M.Index + 1, M.Size.X, M.Size.Y,
			M.WorkSize.X, M.WorkSize.Y), M.Index != TestHits);
		TestFirstLog = M.Index;
		BRDisplay::Request(2, FIntPoint(1024, 576));
		return true;
	});
	Add(TEXT("v4.10 affichage : retablir sur l'autre ecran"), 1.5f, [this]()
	{
		if (TestSpot.Z <= 0.f)
		{
			return true;
		}
		const BRDisplay::FMonitor M = BRDisplay::ActiveMonitor();
		Note(FString::Printf(TEXT("essai 1024x576 sur cet ecran : fenetre sur l'ecran %d (attendu %d)"), M.Index + 1, TestHits + 1), M.Index != TestHits);
		BRDisplay::Revert();
		return true;
	});
	Add(TEXT("v4.10 affichage : retour de la fenetre"), 0.f, [this]()
	{
		if (TestSpot.Z <= 0.f)
		{
			return true;
		}
		const BRDisplay::FMonitor M = BRDisplay::ActiveMonitor();
		Note(FString::Printf(TEXT("apres RETABLIR : fenetre sur l'ecran %d (attendu %d), taille %dx%d"), M.Index + 1, TestHits + 1, BRDisplay::Effective().Resolution.X,
			BRDisplay::Effective().Resolution.Y), M.Index != TestHits);
		Shot(TEXT("V410_autre_ecran"));
		if (TSharedPtr<SWindow> Win = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr)
		{
			Win->MoveWindowTo(FVector2D(TestSpot.X, TestSpot.Y));
		}
		return true;
	});
	Add(TEXT("v4.10 affichage : affichage du joueur retabli"), 1.f, [this]()
	{
		if (TestSpot.Z <= 0.f)
		{
			return true;
		}
		// Le mode d'origine, sur l'ecran d'origine, redevient la configuration confirmee (rien du test ne reste enregistre)
		if (BRDisplay::Request(TestOrigWindow, TestOrigResolution))
		{
			BRDisplay::Confirm();
		}
		return true;
	});
	Add(TEXT("v4.10 affichage : affichage du joueur verifie"), 0.f, [this]()
	{
		if (TestSpot.Z <= 0.f)
		{
			return true;
		}
		const BRDisplay::FMode Now = BRDisplay::Current();
		const BRDisplay::FMonitor M = BRDisplay::ActiveMonitor();
		const bool bOk = Now.Window == TestOrigWindow && Now.Resolution == TestOrigResolution && M.Index == TestWindowMonitor;
		Note(FString::Printf(TEXT("affichage du joueur retabli : mode %d %dx%d sur l'ecran %d (attendu %d %dx%d sur l'ecran %d)"), Now.Window, Now.Resolution.X, Now.Resolution.Y,
			M.Index + 1, TestOrigWindow, TestOrigResolution.X, TestOrigResolution.Y, TestWindowMonitor + 1), !bOk);
		return true;
	});
	Add(TEXT("v4.10 affichage : ecran de la fenetre"), 1.f, [this]()
	{
		const BRDisplay::FMonitor M = BRDisplay::ActiveMonitor();
		if (!M.bKnown)
		{
			Skip(TEXT("ecrans : informations indisponibles (Slate absent)"));
			return true;
		}
		TArray<FString> TooBig;
		for (const FIntPoint& R : BRDisplay::ResolutionsFor(2))
		{
			if (R.X > M.WorkSize.X || R.Y > M.WorkSize.Y)
			{
				TooBig.Add(FString::Printf(TEXT("%dx%d"), R.X, R.Y));
			}
		}
		if (TestWindowMonitor != INDEX_NONE)
		{
			Note(FString::Printf(TEXT("apres les essais d'affichage, la fenetre est sur l'ecran %d (au lancement : %d) : %s"), M.Index + 1, TestWindowMonitor + 1,
				M.Index == TestWindowMonitor ? TEXT("meme ecran") : TEXT("ECRAN CHANGE")), M.Index != TestWindowMonitor);
		}
		Note(FString::Printf(TEXT("ecran de la fenetre : %d sur %d%s, %dx%d, zone utile %dx%d, %d DPI ; tailles fenetrees plus grandes que la zone utile : %s"), M.Index + 1, M.Count,
			M.bPrimary ? TEXT(" (principal)") : TEXT(""), M.Size.X, M.Size.Y, M.WorkSize.X, M.WorkSize.Y, M.DPI, TooBig.Num() ? *FString::Join(TooBig, TEXT(", ")) : TEXT("aucune")),
			TooBig.Num() > 0);
		if (M.Count < 2)
		{
			Skip(TEXT("un seul ecran branche : choix de l'ecran qui contient la fenetre non verifie sur plusieurs ecrans"));
		}
		else
		{
			// Chaque ecran est reconnu par un point en son centre (fenetre deplacee sur un autre ecran)
			FDisplayMetrics Metrics;
			FSlateApplication::Get().GetCachedDisplayMetrics(Metrics);
			int32 Wrong = 0;
			for (int32 i = 0; i < Metrics.MonitorInfo.Num(); ++i)
			{
				const FPlatformRect& R = Metrics.MonitorInfo[i].DisplayRect;
				Wrong += BRDisplay::MonitorAt(FIntPoint((R.Left + R.Right) / 2, (R.Top + R.Bottom) / 2)).Index != i ? 1 : 0;
			}
			Note(FString::Printf(TEXT("plusieurs ecrans : %d ecran(s), %d mal reconnu(s)"), Metrics.MonitorInfo.Num(), Wrong), Wrong > 0);
		}
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Interface
	// Notifications sur l'inventaire : une seule pastille dans la bande du haut, au-dessus du trait et des onglets
	Add(TEXT("v4.10 interface : notifications sur l'inventaire"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		PC->SetInventoryOpen(true, 0);
		ABRHUD::Notify(this, TEXT("[Z][Q][S][D] : se d\u00e9placer \u00b7 [MAJ GAUCHE] : courir \u00b7 [F] : lampe \u00b7 [CTRL GAUCHE] : s'accroupir"), 6.f, FLinearColor::White);
		ABRHUD::Notify(this, TEXT("OBJECTIFS : trouver 6 cassettes VHS et filmer pendant une coupure de courant pour stabiliser la sortie. Ce texte est volontairement tr\u00e8s long pour v\u00e9rifier qu'il est raccourci sur une seule ligne sans recouvrir les panneaux de l'inventaire."), 6.f,
			FLinearColor(1.f, 0.85f, 0.4f));
		return true;
	});
	Add(TEXT("v4.10 interface : notifications : mesure"), 0.6f, [this]()
	{
		ABRHUD* H = Hud(GetPC());
		const float Line = H->LastTopLineY;
		const bool bOk = H->LastMsgBottom > 0.f && H->LastMsgBottom <= Line;
		Note(FString::Printf(TEXT("inventaire ouvert, 2 notifications : pastille de %.0f a %.0f px (trait du haut a %.0f px, onglets en dessous) : %s"), H->LastMsgTop, H->LastMsgBottom,
			Line, bOk ? TEXT("au-dessus des panneaux") : TEXT("RECOUVRE les panneaux")), !bOk);
		Shot(TEXT("V410_inventaire_notifications"));
		return true;
	});
	// Aide des parametres : la plus longue (allemand), zone fixe, defilement, rien sous le mode de rendu
	Add(TEXT("v4.10 interface : aide la plus longue"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = Hud(PC);
		TestLanguage = BRLoc::Current().Code;
		BRLoc::SetLanguage(TEXT("de"));
		int32 Longest = INDEX_NONE;
		int32 LongestLen = 0;
		for (int32 i = 0; i < PC->GetSettingsCount(); ++i)
		{
			const int32 Len = PC->GetSettingHint(i).Len();
			if (Len > LongestLen)
			{
				LongestLen = Len;
				Longest = i;
			}
		}
		PC->SetInventoryOpen(true, 2);
		H->SettingsCategory = Longest != INDEX_NONE ? PC->GetSettingCategory(Longest) : 0;
		H->TestHintSetting = Longest;
		TestHits = Longest;
		H->LastHintBottom = 0.f;
		return true;
	});
	Add(TEXT("v4.10 interface : aide : mesure"), 0.8f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = Hud(PC);
		// Zone telle que le HUD l'a dessinee (l'echelle de l'interface lue ici, hors du dessin, peut differer)
		const float HelpBottom = H->LastHintZoneBottom;
		const float HelpTop = H->LastHintZoneTop;
		const bool bInside = H->LastHintBottom <= HelpBottom + 1.f;
		const bool bRows = H->LastRowsBottom <= HelpTop + 1.f;
		Note(FString::Printf(TEXT("aide la plus longue (allemand, ligne %d) : %d ligne(s), %d visible(s) ; bas du texte %.0f px, bas de la zone %.0f px : %s ; lignes des reglages au-dessus de l'aide : %s"),
			TestHits, H->LastHintLines, H->LastHintVisible, H->LastHintBottom, HelpBottom, bInside ? TEXT("dans la zone") : TEXT("DEBORDE"), YesNo(bRows)), !bInside || !bRows);
		Shot(TEXT("V410_parametres_aide"));
		TestBandages = H->LastHintLines;
		return true;
	});
	Add(TEXT("v4.10 interface : aide : defilement"), 6.5f, [this]()
	{
		return true;
	});
	Add(TEXT("v4.10 interface : aide : defilement mesure"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		ABRHUD* H = Hud(PC);
		if (TestBandages > 3)
		{
			Note(FString::Printf(TEXT("aide de %d lignes : defilement automatique apres 6,5 s : %s (decalage %.0f)"), TestBandages, YesNo(H->HintScroll > 0.f), H->HintScroll), H->HintScroll <= 0.f);
			Shot(TEXT("V410_parametres_aide_defilee"));
		}
		else
		{
			Note(FString::Printf(TEXT("aide la plus longue : %d ligne(s), tient dans la zone sans defilement"), TestBandages));
		}
		H->TestHintSetting = INDEX_NONE;
		BRLoc::SetLanguage(TestLanguage);
		PC->SetInventoryOpen(false);
		return true;
	});
	// Signal de manque d'air : fixe sans flashs, pulsation avec les flashs normaux
	Add(TEXT("v4.10 confort : signal d'air sans flashs"), 0.f, [this]()
	{
		ABRHUD* H = Hud(GetPC());
		FBRSettings::Get().Flashes = 2;
		H->TestAirBreath = 10.f;
		SoakRamFirst = 10.f;
		SoakRamMax = -10.f;
		return true;
	});
	auto SamplePulse = [this](const TCHAR* Label, bool bExpectSteady)
	{
		Add(FString::Printf(TEXT("v4.10 confort : signal d'air (%s)"), Label), 0.f, [this, Label, bExpectSteady]()
		{
			ABRHUD* H = Hud(GetPC());
			if (StepTime > 0.1f)
			{
				SoakRamFirst = FMath::Min(SoakRamFirst, H->LastAirCuePulse);
				SoakRamMax = FMath::Max(SoakRamMax, H->LastAirCuePulse);
			}
			if (StepTime < 1.5f)
			{
				return false;
			}
			const float Range = SoakRamMax - SoakRamFirst;
			const bool bOk = bExpectSteady ? Range < 0.01f : Range > 0.2f;
			Note(FString::Printf(TEXT("signal \"manque d'air\" pendant 1,4 s, %s : intensite de %.2f a %.2f (%s)"), Label, SoakRamFirst, SoakRamMax,
				bExpectSteady ? TEXT("attendu : fixe") : TEXT("attendu : pulsation")), !bOk);
			if (bExpectSteady)
			{
				Shot(TEXT("V410_air_sans_flashs"));
				FBRSettings::Get().Flashes = 0;
				SoakRamFirst = 10.f;
				SoakRamMax = -10.f;
			}
			else
			{
				H->TestAirBreath = -1.f;
				SoakRamFirst = 0.f;
				SoakRamMax = 0.f;
			}
			return true;
		});
	};
	SamplePulse(TEXT("flashs : aucun"), true);
	SamplePulse(TEXT("flashs : normaux"), false);

	// ------------------------------------------------------------------------------------------------ Fin
	Add(TEXT("v4.10 polices : tout le test"), 0.f, [this]()
	{
		const bool bLastResort = LogContains(0, TEXT("last resort font data"));
		Note(FString::Printf(TEXT("depuis le debut du test (menus, inventaire, preparation, dialogue) : police de dernier recours utilisee : %s (attendu : non)"),
			bLastResort ? TEXT("OUI") : TEXT("non")), bLastResort);
		return true;
	});
	Add(TEXT("v4.10 : reglages du joueur retablis"), 0.f, [this]()
	{
		if (bSettingsSaved)
		{
			FBRSettings::Get() = SavedSettings;
			if (ABRPlayerController* PC = GetPC())
			{
				PC->ApplySettings();
			}
		}
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = false;
		}
		UBRAssets::TestPrepareStallUntil = 0.0;
		return true;
	});
}

// =====================================================================================================================
// Multijoueur
// =====================================================================================================================

void ABRAutoTest::AddNetV410Steps(bool bClient)
{
	// L'hote blesse chaque client (coup decide par le serveur) ; chaque client se soigne deux fois de suite
	Add(TEXT("v4.10 soins : blessure"), 2.f, [this, bClient]()
	{
		if (bClient)
		{
			// La sante du client est decidee par le serveur ; son mode invincible (test) est un etat qu'il envoie : il le quitte
			if (ABRCharacter* Me = GetPlayer())
			{
				Me->bGodMode = false;
			}
			return true;
		}
		const TArray<ABRCharacter*> Mates = RemotePlayers(GetBRWorld());
		for (ABRCharacter* Mate : Mates)
		{
			// Le client a quitte le mode invincible et fini de preparer son niveau (etat recu par le serveur)
			if (((Mate->NetFlags & 16) != 0 || Mate->IsLevelLoading() || Mate->DeathState.bDead || Mate->bServerDying) && StepTime < 30.f)
			{
				return false;
			}
		}
		for (ABRCharacter* Mate : Mates)
		{
			// Sante pleine d'abord (le client vient d'etre releve a 35 par les tests precedents) : le coup de 45 ne tue pas ;
			// la nouvelle sante part au client avec l'effet du coup
			Mate->Health = 100.f;
			Mate->ReceiveAttack(45.f, 0.f, nullptr);
			// v4.11 : le stock de soin connu de l'hote ne se redeclare plus (une seule declaration par session : correction du
			// constat 2.1). Les bandages que le client se donne pour ce test (trois au plus : deux soins rapproches, puis un
			// pendant la serie de coups) sont credites ici par l'hote, comme des ramassages acceptes
			Mate->CreditHealItem(EBRItem::Bandage, 3);
			Note(FString::Printf(TEXT("soins : coup de 45 au client (sante vue par le serveur : %.0f)"), Mate->Health));
		}
		return true;
	});
	Add(TEXT("v4.10 soins : deux soins rapproches"), 0.f, [this, bClient]()
	{
		ABRCharacter* C = GetPlayer();
		if (!bClient || !C)
		{
			return true;
		}
		if (C->Health > 70.f && StepTime < 30.f)
		{
			return false; // coup pas encore recu
		}
		if (C->Health > 70.f)
		{
			Note(TEXT("soins : coup du serveur jamais recu en 30 s"), true);
		}
		if (C->CountItem(EBRItem::Bandage) < 2)
		{
			C->AddItem(EBRItem::Bandage, 2 - C->CountItem(EBRItem::Bandage));
		}
		TestBandages = C->CountItem(EBRItem::Bandage);
		TestHealth = C->Health;
		C->LocalLastHealTime = -100.f;
		C->QuickUse(EBRItem::Bandage);
		C->QuickUse(EBRItem::Bandage); // second appui dans la meme image : refuse sans rien consommer
		TestTimer = FPlatformTime::Seconds();
		return true;
	});
	Add(TEXT("v4.10 soins : reponse du serveur"), 0.f, [this, bClient]()
	{
		ABRCharacter* C = GetPlayer();
		if (!bClient || !C)
		{
			return true;
		}
		if (C->IsHealPending() && StepTime < 5.f)
		{
			return false;
		}
		const int32 Used = TestBandages - C->CountItem(EBRItem::Bandage);
		const bool bOk = Used == 1 && FMath::IsNearlyEqual(C->Health, C->LastServerHealth, 0.6f) && C->Health > TestHealth;
		Note(FString::Printf(TEXT("deux soins dans la meme image : bandages utilises %d (attendu : 1), sante %.0f -> %.0f, sante officielle recue %.0f (reponse en %.0f ms)"), Used,
			TestHealth, C->Health, C->LastServerHealth, (FPlatformTime::Seconds() - TestTimer) * 1000.0), !bOk);
		return true;
	});
	// Soin au milieu d'une serie de coups. Les deux machines avancent a leur rythme : elles se calent sur l'etat replique.
	// L'hote frappe (2 toutes les 0,2 s) jusqu'a 1,5 s apres le second soin accepte ; le client se soigne des que les coups
	// arrivent (sante en baisse).
	Add(TEXT("v4.10 soins : soin pendant une serie de coups"), 0.f, [this, bClient]()
	{
		ABRCharacter* C = GetPlayer();
		if (bClient && C)
		{
			TestBandages = C->CountItem(EBRItem::Bandage);
			if (TestBandages < 1)
			{
				C->AddItem(EBRItem::Bandage, 1);
				TestBandages = C->CountItem(EBRItem::Bandage);
			}
			TestHealth = C->Health;
		}
		TestHits = 0;
		TestTimer = 0.0;
		FallStart = 0.0;
		bTestPrepShown = false;
		return true;
	});
	Add(TEXT("v4.10 soins : coups"), 0.f, [this, bClient]()
	{
		if (!bClient)
		{
			const TArray<ABRCharacter*> Mates = RemotePlayers(GetBRWorld());
			bool bFirstAccepted = false;
			bool bSecondAccepted = false;
			for (ABRCharacter* Mate : Mates)
			{
				bFirstAccepted |= Mate->ServerHealsAccepted >= 1;
				bSecondAccepted |= Mate->ServerHealsAccepted >= 2;
			}
			// Les coups commencent apres le premier soin accepte (le client en est alors a cette etape)
			if (!bFirstAccepted)
			{
				if (StepTime > 20.f)
				{
					Note(TEXT("coups pendant le soin : premier soin du client jamais accepte en 20 s"), true);
					return true;
				}
				return false;
			}
			if (FallStart <= 0.0)
			{
				FallStart = FPlatformTime::Seconds();
			}
			if (bSecondAccepted && TestTimer <= 0.0)
			{
				TestTimer = FPlatformTime::Seconds();
			}
			if ((TestTimer > 0.0 && FPlatformTime::Seconds() - TestTimer > 1.5) || StepTime > 20.f)
			{
				Note(FString::Printf(TEXT("coups du serveur pendant le soin : %d coup(s) de 1 ; second soin %s"), TestHits, bSecondAccepted ? TEXT("accepte") : TEXT("JAMAIS RECU")),
					!bSecondAccepted);
				return true;
			}
			if (FPlatformTime::Seconds() - FallStart >= TestHits * 0.25)
			{
				for (ABRCharacter* Mate : Mates)
				{
					Mate->ReceiveAttack(1.f, 0.f, nullptr);
				}
				++TestHits;
			}
			return false;
		}
		ABRCharacter* C = GetPlayer();
		if (!C)
		{
			return true;
		}
		if (!bTestPrepShown)
		{
			// Le premier coup arrive : soin tout de suite (au milieu de la serie)
			if (C->Health < TestHealth - 1.f && !C->IsHealPending())
			{
				C->LocalLastHealTime = -100.f;
				C->QuickUse(EBRItem::Bandage);
				bTestPrepShown = true;
				TestTimer = FPlatformTime::Seconds();
			}
			if (StepTime > 20.f)
			{
				Note(TEXT("soin pendant les coups : aucun coup recu en 20 s"), true);
				return true;
			}
			return false;
		}
		return FPlatformTime::Seconds() - TestTimer > 3.0;
	});
	Add(TEXT("v4.10 soins : coherence"), 1.5f, [this, bClient]()
	{
		if (bClient)
		{
			ABRCharacter* C = GetPlayer();
			if (C && bTestPrepShown)
			{
				const int32 Used = TestBandages - C->CountItem(EBRItem::Bandage);
				const bool bOk = Used == 1 && FMath::IsNearlyEqual(C->Health, C->LastServerHealth, 2.f);
				Note(FString::Printf(TEXT("soin au milieu d'une serie de coups : bandage utilise %d (attendu : 1), sante locale %.1f, derniere sante officielle %.1f"), Used, C->Health,
					C->LastServerHealth), !bOk);
			}
			if (C)
			{
				C->bGodMode = true;
			}
			return true;
		}
		for (ABRCharacter* Mate : RemotePlayers(GetBRWorld()))
		{
			Note(FString::Printf(TEXT("serveur : soins acceptes %d, refuses %d, sante officielle du client %.1f, objets de soin connus : bandages %d"), Mate->ServerHealsAccepted,
				Mate->ServerHealsRefused, Mate->Health, Mate->GetServerHealStock(EBRItem::Bandage)), Mate->ServerHealsAccepted != 2);
			Mate->bGodMode = true;
		}
		return true;
	});
}

// =====================================================================================================================
// Paquet : test de lancement
// =====================================================================================================================

void ABRAutoTest::AddSmokeSteps()
{
	{
		FLevelReport R;
		R.Title = TEXT("Lancement du paquet");
		Reports.Add(R);
	}
	Add(TEXT("lancement : niveau construit"), 2.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if ((!W || !W->IsLevelReady() || W->IsTransitioning() || !GetPlayer()) && StepTime < 90.f)
		{
			return false;
		}
		return true;
	});
	Add(TEXT("lancement : verifications"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		const FString Map = GetWorld() ? GetWorld()->GetMapName() : FString();
		Note(FString::Printf(TEXT("carte : %s"), *Map), !Map.Contains(TEXT("L_Backrooms")));
		Note(FString::Printf(TEXT("niveau construit : %s, %d chunk(s)"), YesNo(W && W->IsLevelReady()), W ? W->GetChunkCount() : 0), !W || W->GetChunkCount() == 0);
		const TArray<FString> Fonts = BRFonts::MissingFiles();
		Note(FString::Printf(TEXT("polices des autres ecritures absentes : %s"), Fonts.Num() ? *FString::Join(Fonts, TEXT(", ")) : TEXT("aucune")), Fonts.Num() > 0);
		TArray<FString> Glyphs;
		if (MissingGlyphs(Glyphs))
		{
			Note(FString::Printf(TEXT("caracteres sans glyphe : %d %s"), Glyphs.Num(), *FString::Join(Glyphs, TEXT(", ")).Left(300)), Glyphs.Num() > 0);
		}
		const int32 Langs = BRLoc::Languages().Num();
		TArray<FString> MissingKeys;
		const int32 Missing = BRLoc::CountMissing(&MissingKeys);
		Note(FString::Printf(TEXT("langues : %d ; textes manquants dans la langue courante (%s) : %d"), Langs, BRLoc::Current().Code, Missing), Langs < 22);
		Note(FString::Printf(TEXT("catalogue des ressources : %d ensemble(s)"), UBRAssets::PreloadCatalogCount()), UBRAssets::PreloadCatalogCount() == 0);
		if (C && GetWorld())
		{
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(BRSmokeGround), false, C);
			const bool bGround = GetWorld()->LineTraceSingleByChannel(Hit, C->GetActorLocation(), C->GetActorLocation() - FVector(0.f, 0.f, 400.f), ECC_Visibility, Q);
			Note(FString::Printf(TEXT("sol sous le joueur : %s"), YesNo(bGround)), !bGround);
		}
		Shot(TEXT("Lancement"));
		return true;
	});
	Add(TEXT("Fin"), 0.5f, [this]()
	{
		Finish();
		return true;
	});
}

// =====================================================================================================================
// Session longue : memoire et fluidite sur la duree
// =====================================================================================================================

void ABRAutoTest::AddSoakSteps(float Minutes)
{
	AddLoad(0, 4.f, TEXT("v4.10 : session longue"));
	Add(TEXT("session longue : debut"), 0.f, [this, Minutes]()
	{
		SoakEnd = FPlatformTime::Seconds() + Minutes * 60.0;
		SoakRound = 0;
		SoakRamFirst = 0.f;
		SoakRamMax = 0.f;
		TestTimer = FPlatformTime::Seconds();
		UBRAssets::SyncLoadsInBuild = 0;
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = true;
		}
		SoakLines.Add(TEXT("minute;tour;niveau;ram_mo;textures_mo;objets;caches_ressources;caches_materiaux;ensembles;chargements_construction;img_s"));
		return true;
	});
	const int32 LoopStart = Plan.Num();
	Add(TEXT("session longue : changement de niveau"), 0.f, [this]()
	{
		// Les 11 autres niveaux, chacun suivi d'un retour au Niveau 0 (releve de reference)
		static const int32 Sequence[] = { 37, 0, 4, 0, 1, 0, 6, 0, 5, 0, 2, 0, 9, 0, 3, 0, 8, 0, 10, 0, 11, 0 };
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		const int32 Target = Sequence[SoakRound % UE_ARRAY_COUNT(Sequence)];
		TestLevel = Target;
		W->RequestTransition(Target, false);
		return true;
	});
	Add(TEXT("session longue : arrivee"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (W && (W->IsTransitioning() || W->GetLevelNumber() != TestLevel) && StepTime < 60.f)
		{
			return false;
		}
		if (!W || W->GetLevelNumber() != TestLevel)
		{
			Note(FString::Printf(TEXT("session longue : Niveau %d non atteint en 60 s"), TestLevel), true);
		}
		// Le joueur se promene (les chunks se construisent et se demontent)
		WalkYaw = FMath::FRandRange(0.f, 360.f);
		WalkTime = 12.f;
		StartMeasure();
		return true;
	});
	Add(TEXT("session longue : promenade"), 14.f, [this]()
	{
		return true;
	});
	Add(TEXT("session longue : releve"), 0.f, [this, LoopStart]()
	{
		FLevelReport& R = Report();
		EndMeasure(R);
		const ABRWorld* W = GetBRWorld();
		const UBRAssets* A = UBRAssets::Get(this);
		const float Ram = RamMB();
		if (W && W->GetLevelNumber() == 0)
		{
			// Releve de reference : au retour au Niveau 0 (meme niveau, memes ensembles attendus)
			if (SoakRamFirst <= 0.f && SoakRound >= 3)
			{
				SoakRamFirst = Ram;
			}
			SoakRamMax = FMath::Max(SoakRamMax, Ram);
		}
		SoakLines.Add(FString::Printf(TEXT("%.1f;%d;%d;%.0f;%.0f;%d;%d;%d;%d;%d;%.0f"), (FPlatformTime::Seconds() - TestTimer) / 60.0, SoakRound, W ? W->GetLevelNumber() : -1, Ram,
			TexMB(), GUObjectArray.GetObjectArrayNumMinusAvailable(), A ? A->GetLoadedCount() : -1, A ? A->GetMaterialCacheCount() : -1, UBRAssets::PreloadedSetNames().Num(),
			UBRAssets::SyncLoadsInBuild, R.AvgFPS));
		++SoakRound;
		if (FPlatformTime::Seconds() < SoakEnd)
		{
			StepIndex = LoopStart - 1; // tour suivant (l'index avance apres cette etape)
		}
		return true;
	});
	Add(TEXT("session longue : bilan"), 0.f, [this]()
	{
		FFileHelper::SaveStringArrayToFile(SoakLines, *FPaths::Combine(OutDir, TEXT("SessionLongue.csv")), FFileHelper::EEncodingOptions::ForceUTF8);
		const float Growth = SoakRamFirst > 0.f ? (SoakRamMax - SoakRamFirst) / SoakRamFirst : 0.f;
		Note(FString::Printf(TEXT("session longue : %d tour(s) en %.0f min ; RAM au Niveau 0 : %.0f Mo au 4e tour, %.0f Mo au plus haut (%+.0f %%) ; chargements synchrones pendant les constructions : %d ; detail : SessionLongue.csv"),
			SoakRound, (FPlatformTime::Seconds() - TestTimer) / 60.0, SoakRamFirst, SoakRamMax, Growth * 100.f, UBRAssets::SyncLoadsInBuild), Growth > 0.15f);
		for (int32 i = 1; i < SoakLines.Num(); ++i)
		{
			Note(TEXT("  ") + SoakLines[i]);
		}
		return true;
	});
}
