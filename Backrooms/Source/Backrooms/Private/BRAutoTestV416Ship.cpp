// v4.16 (paquet) : etapes du test automatique du paquet distribue et des sauvegardes.
//
// -BRAutoTest -BRAutoTestChapter : traversee du premier chapitre publie, 0 -> 1 -> 37 -> 2 -> fin du chapitre, jouable DANS le
// paquet Windows Shipping (avec -BRSmokeTest, le seul plan qu'une version publiee accepte : ABRAutoTest::IsRequested) :
//   - parties de test (prefixe BR_AutoTestChapitre_, pose des le lancement par BRSaves : les parties du joueur ne sont ni
//     lues ni ecrites) ; canal publie (compile dans le paquet, impose hors Shipping) ; parcours lineaire ;
//   - NOUVELLE PARTIE par le vrai menu (SOLO, VOS PARTIES, NOUVELLE PARTIE, COMMENCER). -BRChapterSeed=<graine du parcours>
//     [-BRChapterIntro=<graine du Level 0>] : une disposition donnee (partie ecrite comme NOUVELLE PARTIE l'ecrit, prise par
//     VOS PARTIES puis CONTINUER), pour rejouer une traversee dont le rapport donne les graines ;
//   - Level 0 : marche jusqu'a la porte de service, touche d'interaction ; Niveaux 1, 37 et 2 : missions resolues en
//     marchant (solveur aux seules informations visibles, chaque action par la touche d'interaction sur le mecanisme vise),
//     sorties prises a pied (BRAutoTestV412.cpp, BRAutoTestV413.cpp) ; fin du chapitre ;
//   - a chaque arrivee, la partie relue du disque (etape accomplie, reprise, niveau explore, session, graine du niveau,
//     parcours, morts) ; Poolrooms resolues : fermeture du jeu (l'ecriture de QUITTER) puis reprise par VOS PARTIES et
//     CONTINUER : meme graine, meme etat de mission, sortie ouverte ; fin du chapitre ecrite par le jeu lui-meme (fichier
//     relu sans ecriture du test) ; apres un redemarrage : carte de la partie, CONTINUER au Niveau 2, mission accomplie.
//   -BRChapterPhase=1|2|3 : la meme traversee en trois lancements separes par de VRAIES fermetures du jeu
//   (Tools/Build/check_package.py --chapter) : 1 jusqu'aux Poolrooms resolues puis fermeture ; 2 relance, reprise et fin du
//   chapitre ; 3 relance apres la fin, verifications, nettoyage. 0 (defaut) : tout dans un lancement (la fermeture est
//   l'ecriture de QUITTER, la reprise relit le fichier par le menu). Etat de chaque phase : Chapitre.json (dossier du
//   rapport) et -BRChapterOut=<fichier> ; etat passe d'une phase a la suivante : Saved/SaveGames/BR_AutoTestChapitre_Etat.txt
//   (efface au nettoyage, comme les parties de test). v4.16 (relecture) : dans un paquet Shipping, Saved est le dossier du
//   JOUEUR (%LOCALAPPDATA%\Backrooms\Saved : parties de test et etat a cote de ses parties BR_Partie_*) ; une traversee
//   arretee avant le nettoyage les y laisse : check_package.py les en retire a la fin du controle, quelle qu'en soit l'issue.
//
// -BRAutoTest -BRAutoTestV416Ship : REJOUER et sauvegardes, sur des parties de test (prefixe BR_AutoTestV416Ship_) :
//   - campagne lineaire reprise au Niveau 2 par le menu : consommation (eau d'amande par l'objet rapide), degats (decides par
//     le serveur), fermeture : le fichier les garde ;
//   - REJOUER du Niveau 1 par le menu : consommation, degats, note lue, entite vue, ecritures (minuterie, pause, bloquante),
//     mort, reveil, fermeture : le fichier relu garde equipement, sante, morts, temps, niveau, session, campagne et histoire a
//     l'identique ; seuls la note et l'entite s'ajoutent au journal (union) ;
//   - CONTINUER apres le rejeu : session du Niveau 2 reprise (graine, mission), equipement et sante de la campagne, note et
//     entite du rejeu au carnet ;
//   - partie du format 5 : copie d'origine (_Format5) faite au chargement, fichier non reecrit par la lecture ; REJOUER et note
//     lue : seule l'union est ecrite (format 6), la copie reste identique a l'original.
// Une verification qui ne peut pas etre faite est notee "NON VERIFIE", jamais comptee comme reussie.
#include "BRAutoTest.h"
#include "BRAutoTestWalk.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BRContentLogic.h"
#include "BRInteractables.h"
#include "BRLevels.h"
#include "BRMission.h"
#include "BRPlayerController.h"
#include "BRSave.h"
#include "BRStoryLogic.h"
#include "BRWorld.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace BRTestV416Ship
{
	// ------------------------------------------------------------------------------------------------- Traversee du chapitre
	/** Prefixe des parties de la traversee : le meme que BRSaves (BRSave.cpp, Prefix) pose des le lancement */
	const TCHAR* const ChapterPrefix = TEXT("BR_AutoTestChapitre_");
	const TCHAR* const ChapterSaveName = TEXT("Traversee du chapitre");
	/** Etat passe d'une phase a la suivante (Saved/SaveGames, a cote des parties de test) */
	const TCHAR* const ChapterMarker = TEXT("BR_AutoTestChapitre_Etat.txt");

	struct FChapter
	{
		int32 Phase = 0;
		int32 Slot = INDEX_NONE;
		uint32 RunSeed = 0;
		uint32 IntroSeed = 0;
		uint32 ForcedRunSeed = 0;
		uint32 ForcedIntro = 0;
		uint32 PoolSeed = 0;
		FString PoolMission;
		uint32 Level2Seed = 0;
		FString Level2Mission;
		int32 Deaths = 0;
		int32 Serial = 0;
		float Clock = 0.f;
		int32 Step = 0;
		/** Niveaux ou le jeu a amene le joueur pendant ce lancement (dans l'ordre) */
		TArray<int32> Arrivals;
		/** Etat lu de la phase precedente (phases 2 et 3) */
		bool bMarker = false;
		int32 MarkerPhase = -1;
		bool bMarkerValid = false;
		bool bMarkerEnd = false;
		/** Jalons de la phase */
		bool bPrefixAtStart = false;
		bool bClosed = false;
		bool bResumed = false;
		bool bChapterEnd = false;
		bool bEndSaved = false;
		bool bAfterRestart = false;
		int32 Continue = -1;
		bool bCleaned = false;
		/** Relecture de la partie apres une arrivee : derniere lecture du fichier (une par seconde) */
		double ReadClock = 0.0;
		/** Une etape de la chaine n'a pas abouti (niveau non atteint) : la suite de la traversee n'est pas jugee */
		bool bBroken = false;
		FString BrokenWhy;
	};
	FChapter Ch;

	const TCHAR* Oui(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	FString Hex(const TArray<uint8>& Bytes)
	{
		return Bytes.Num() > 0 ? BytesToHex(Bytes.GetData(), Bytes.Num()) : FString(TEXT("-"));
	}

	FString MarkerPath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), ChapterMarker);
	}

	/** Configuration du binaire (rapport) */
	const TCHAR* BuildName()
	{
#if UE_BUILD_SHIPPING
		return TEXT("Shipping (paquet)");
#elif WITH_EDITOR
		return TEXT("Development (binaire de l'editeur, -game)");
#else
		return TEXT("Development (jeu)");
#endif
	}

	/** Toutes les copies d'une partie de test (principal, secours, illisible, recuperation, anciens formats) */
	void DeleteSlotFiles()
	{
		for (int32 SaveIdx = 0; SaveIdx < BRSaves::MaxSlots; ++SaveIdx)
		{
			for (const FString& FileName : { BRSaves::SlotName(SaveIdx), BRSaves::BackupSlotName(SaveIdx), BRSaves::UnreadableSlotName(SaveIdx), BRSaves::RecoverySlotName(SaveIdx) })
			{
				IFileManager::Get().Delete(*BRSaves::FilePath(FileName), false, true, true);
			}
			for (int32 V = 1; V < UBRSaveGame::CurrentVersion; ++V)
			{
				IFileManager::Get().Delete(*BRSaves::FilePath(BRSaves::LegacySlotName(SaveIdx, V)), false, true, true);
			}
		}
	}

	int32 CountTestFiles()
	{
		int32 N = 0;
		for (int32 SaveIdx = 0; SaveIdx < BRSaves::MaxSlots; ++SaveIdx)
		{
			for (const FString& FileName : { BRSaves::SlotName(SaveIdx), BRSaves::BackupSlotName(SaveIdx), BRSaves::UnreadableSlotName(SaveIdx), BRSaves::RecoverySlotName(SaveIdx) })
			{
				N += IFileManager::Get().FileExists(*BRSaves::FilePath(FileName)) ? 1 : 0;
			}
		}
		return N;
	}

	int32 ResumeOf(const BRStory::FState& S)
	{
		return BRStory::ResumePoint(S, [](int Lv, void*) { return BRLevels::Exists(Lv); }, nullptr);
	}

	bool ReadMarker(FChapter& Out)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *MarkerPath()))
		{
			return false;
		}
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		TMap<FString, FString> Kv;
		for (const FString& L : Lines)
		{
			FString K;
			FString V;
			if (L.Split(TEXT("="), &K, &V))
			{
				Kv.Add(K.TrimStartAndEnd(), V.TrimStartAndEnd());
			}
		}
		auto U32 = [&Kv](const TCHAR* K) { const FString* V = Kv.Find(K); return V ? static_cast<uint32>(FCString::Strtoui64(**V, nullptr, 10)) : 0u; };
		auto I32 = [&Kv](const TCHAR* K, int32 Def) { const FString* V = Kv.Find(K); return V ? FCString::Atoi(**V) : Def; };
		auto Str = [&Kv](const TCHAR* K) { const FString* V = Kv.Find(K); return V ? *V : FString(); };
		Out.MarkerPhase = I32(TEXT("phase"), -1);
		Out.bMarkerValid = I32(TEXT("valide"), 0) == 1;
		Out.bMarkerEnd = I32(TEXT("fin"), 0) == 1;
		Out.Slot = I32(TEXT("emplacement"), INDEX_NONE);
		Out.RunSeed = U32(TEXT("graine_parcours"));
		Out.IntroSeed = U32(TEXT("graine_level0"));
		Out.PoolSeed = U32(TEXT("graine_poolrooms"));
		Out.PoolMission = Str(TEXT("mission_poolrooms"));
		Out.Level2Seed = U32(TEXT("graine_niveau2"));
		Out.Level2Mission = Str(TEXT("mission_niveau2"));
		Out.Deaths = I32(TEXT("morts"), 0);
		Out.bMarker = true;
		return Out.MarkerPhase >= 1 && Out.Slot >= 0 && Out.Slot < BRSaves::MaxSlots && Out.RunSeed != 0;
	}

	bool WriteMarker(const FChapter& C, bool bValid)
	{
		const FString Text = FString::Printf(TEXT("phase=%d\nvalide=%d\nfin=%d\nemplacement=%d\nnom=%s\ngraine_parcours=%u\ngraine_level0=%u\ngraine_poolrooms=%u\n"
			"mission_poolrooms=%s\ngraine_niveau2=%u\nmission_niveau2=%s\nmorts=%d\n"), C.Phase, bValid ? 1 : 0, C.bEndSaved ? 1 : 0, C.Slot, ChapterSaveName, C.RunSeed,
			C.IntroSeed, C.PoolSeed, *C.PoolMission, C.Level2Seed, *C.Level2Mission, C.Deaths);
		return FFileHelper::SaveStringToFile(Text, *MarkerPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	/** Sortie de la mission vers Target (posee par le monde), visible */
	ABRExit* MissionExitTo(UWorld* InWorld, const ABRWorld* W, int32 Target)
	{
		if (!InWorld || !W)
		{
			return nullptr;
		}
		for (TActorIterator<ABRExit> It(InWorld); It; ++It)
		{
			if (It->Target == Target && !It->IsHidden() && It->GetOwner() == W)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** La sortie de progression vers Target est ouverte selon la regle de l'hote (CanUseExit) ; construite, son voyant n'est pas
	 *  rouge. OutWhat : etat du voyant pour le rapport */
	bool ForwardOpen(UWorld* InWorld, const ABRWorld* W, int32 Target, FString& OutWhat)
	{
		FString Why;
		const bool bRule = W && W->CanUseExit(Target, Why);
		const ABRExit* Exit = MissionExitTo(InWorld, W, Target);
		OutWhat = Exit ? FString(Exit->GetMissionOpen() == 1 ? TEXT("voyant vert") : (Exit->GetMissionOpen() == 0 ? TEXT("voyant ROUGE") : TEXT("sans voyant")))
			: FString(TEXT("sortie pas encore construite"));
		if (!bRule)
		{
			OutWhat += TEXT(", refusee : ") + Why;
		}
		return bRule && (!Exit || Exit->GetMissionOpen() != 0);
	}

	// ------------------------------------------------------------------------------------------------- REJOUER et sauvegardes
	const TCHAR* const ShipPrefix = TEXT("BR_AutoTestV416Ship_");
	constexpr int32 CampaignSlot = 0;
	constexpr int32 MigrationSlot = 1;
	constexpr uint32 ShipRunSeed = 41600u;

	struct FShip
	{
		int32 Serial = 0;
		/** Instant de la derniere demande CONTINUER ou REJOUER (la sante remonte de 1 par seconde au calme) */
		double MenuClock = 0.0;
		uint32 Seed2 = 0;
		TArray<uint8> Mission2;
		/** Fichier de la campagne apres sa fermeture : champs durables (hors journal), notes et entites */
		FString Durable;
		TArray<FString> Notes;
		TArray<int32> Discovered;
		float Health = 0.f;
		int32 Water = 0;
		int32 Bandages = 0;
		/** Rejeu : note lue, entite vue, journal du fichier apres les ecritures du rejeu */
		FString NoteRead;
		int32 EntitySeen = -1;
		TArray<FString> NotesAfter;
		TArray<int32> DiscoveredAfter;
		bool bReplayOk = false;
		/** Migration : octets du format 5, champs durables de la partie migree */
		TArray<uint8> OldBytes;
		FString MigratedDurable;
		TArray<FString> MigratedNotes;
		FString NoteRead2;
		TWeakObjectPtr<ABRPickup> Spawned;
		/** Menu ouvert au debut du test (retabli) */
		bool bInMenuAtStart = true;
	};
	FShip Sh;

	int32 FileCount(const UBRSaveGame* G, EBRItem Item)
	{
		int32 N = 0;
		if (G)
		{
			for (const FBRSavedItem& I : G->Items)
			{
				N += I.Item == static_cast<uint8>(Item) ? I.Count : 0;
			}
		}
		return N;
	}

	/** Champs durables d'une partie, journal (notes, entites) excepte : ce que REJOUER ne doit jamais changer */
	FString DurableOf(const UBRSaveGame* G)
	{
		if (!G)
		{
			return TEXT("(partie illisible)");
		}
		FString ItemList;
		for (const FBRSavedItem& I : G->Items)
		{
			ItemList += FString::Printf(TEXT("%d:%d:%d:%d,"), I.Group, I.Index, I.Item, I.Count);
		}
		const BRStory::FState St = G->GetStory();
		return FString::Printf(TEXT("objets[%s] sante %.1f mental %.1f piles %.1f morts %d temps %.0f niveau %d explores %d campagne %d/%d/%d mort en attente %d "
			"session %d/%d/%u/%d octets gare %d/%d histoire %d/%x/%x/%x/%d/%d parcours %u/%u Level 0 %u"), *ItemList, G->Health, G->Sanity, G->Battery, G->Deaths,
			G->PlayTime, G->CurrentLevel, G->Explored.Num(), G->RouteBits, G->OptionalFound, G->Endings, G->bPendingDeath ? 1 : 0, G->Session.bValid ? 1 : 0,
			G->Session.Level, G->Session.Seed, G->Session.Mission.Num(), G->ParkedSession.bValid ? 1 : 0, G->ParkedSession.Level, St.Reached, St.Done, St.Events,
			St.ChapterEnds, St.Resume, St.Parked, St.Flags, St.RunSeed, St.IntroSeed);
	}

	/** Tous les elements de Sub sont dans Super */
	template <typename T>
	bool Includes(const TArray<T>& Super, const TArray<T>& Sub)
	{
		for (const T& X : Sub)
		{
			if (!Super.Contains(X))
			{
				return false;
			}
		}
		return true;
	}

	FString JoinNotes(const TArray<FString>& Notes)
	{
		return Notes.Num() > 0 ? FString::Join(Notes, TEXT(", ")) : FString(TEXT("aucune"));
	}

	FString JoinKinds(const TArray<int32>& Kinds)
	{
		FString Out;
		for (const int32 K : Kinds)
		{
			Out += (Out.IsEmpty() ? TEXT("") : TEXT(", ")) + FString::FromInt(K);
		}
		return Out.IsEmpty() ? FString(TEXT("aucune")) : Out;
	}

	/** Partie de campagne de test : histoire arretee a l'arrivee au Niveau 2 (Level 0, Niveau 1 et Poolrooms accomplis), sans
	 *  session ; equipement et note de depart ; bLinear : parcours lineaire */
	UBRSaveGame* MakeCampaign(const TCHAR* Title, bool bLinear, const FString& FirstNote, int32 Water, float Health)
	{
		UBRSaveGame* G = NewObject<UBRSaveGame>();
		G->SaveName = Title;
		G->Created = FDateTime::Now();
		for (const int32 Lv : { 0, 1, 37, 2 })
		{
			G->MarkExplored(Lv);
		}
		G->CurrentLevel = 2;
		G->bHasPlayer = true;
		FBRSavedItem W;
		W.Group = static_cast<uint8>(EBRSlotGroup::Pockets);
		W.Index = 0;
		W.Item = static_cast<uint8>(EBRItem::AlmondWater);
		W.Count = Water;
		FBRSavedItem B = W;
		B.Index = 1;
		B.Item = static_cast<uint8>(EBRItem::Bandage);
		B.Count = 1;
		G->Items = { W, B };
		G->Health = Health;
		G->PlayTime = 1600.f;
		if (!FirstNote.IsEmpty())
		{
			G->Notes.Add(FirstNote);
		}
		BRStory::FState St;
		BRStory::SetDone(St, 0);
		BRStory::SetDone(St, 1);
		BRStory::SetDone(St, BRStory::Poolrooms);
		St.Reached = BRStory::IndexOf(2);
		St.Resume = 2;
		St.Events = BRStory::Event::IntroWake | BRStory::Event::DoorOpened;
		if (bLinear)
		{
			St.Flags = BRStory::Flag::Linear;
			St.RunSeed = ShipRunSeed;
		}
		G->SetStory(St);
		return G;
	}

	/** Identifiants des notes des niveaux (Niveau 1 d'abord) */
	TArray<FString> KnownNotes()
	{
		TArray<FString> Ids;
		for (const int32 Lv : { 1, 0, 2, 37 })
		{
			if (BRLevels::Exists(Lv))
			{
				for (const FBRNote& N : BRLevels::Get(Lv).Notes)
				{
					Ids.AddUnique(N.Id.ToString());
				}
			}
		}
		return Ids;
	}
}

// =====================================================================================================================
// Traversee du premier chapitre publie (paquet Shipping compris)
// =====================================================================================================================

void ABRAutoTest::AddChapterSteps()
{
	using namespace BRTestV416Ship;
	Ch = FChapter();
	FParse::Value(FCommandLine::Get(), TEXT("BRChapterPhase="), Ch.Phase);
	Ch.Phase = FMath::Clamp(Ch.Phase, 0, 3);
	FParse::Value(FCommandLine::Get(), TEXT("BRChapterSeed="), Ch.ForcedRunSeed);
	FParse::Value(FCommandLine::Get(), TEXT("BRChapterIntro="), Ch.ForcedIntro);
	const int32 Phase = Ch.Phase;
	// Canal publie : compile dans le paquet (BR_CONTENT_CHANNEL) ; hors Shipping, impose pour ce lancement (sans effet en Shipping)
	BRLevels::SetChannelOverride(static_cast<int32>(BRContent::EChannel::Public));
	const bool bPublic = BRLevels::Channel() == BRContent::EChannel::Public;
	FChapter Prev;
	const bool bPrevOk = Phase >= 2 && ReadMarker(Prev) && Prev.MarkerPhase == Phase - 1 && Prev.bMarkerValid && (Phase == 2 || Prev.bMarkerEnd);
	if (Phase >= 2 && bPrevOk)
	{
		Ch.Slot = Prev.Slot;
		Ch.RunSeed = Prev.RunSeed;
		Ch.IntroSeed = Prev.IntroSeed;
		Ch.PoolSeed = Prev.PoolSeed;
		Ch.PoolMission = Prev.PoolMission;
		Ch.Level2Seed = Prev.Level2Seed;
		Ch.Level2Mission = Prev.Level2Mission;
		Ch.Deaths = Prev.Deaths;
		Ch.bMarker = true;
	}

	Add(FString::Printf(TEXT("chapitre : preparation (phase %d)"), Phase), 0.f, [this, Phase, bPublic, bPrevOk, Prev]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if ((!W || !PC || !GetPlayer() || W->IsTransitioning() || !W->IsLevelReady()) && StepTime < 90.f)
		{
			return false;
		}
		FLevelReport R;
		R.Title = FString::Printf(TEXT("v4.16 : traversee du premier chapitre publie (phase %d%s)"), Phase, Phase == 0 ? TEXT(" : un seul lancement") : TEXT(" sur 3"));
		R.Scene = TEXT("chapitre");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		if (!W || !PC)
		{
			Note(TEXT("chapitre : monde ou joueur absent 90 s apres le lancement"), true);
			Ch.bBroken = true;
			Ch.BrokenWhy = TEXT("jeu pas pret");
			return true;
		}
		// Les parties du joueur n'ont pas ete lues : le prefixe de test etait en place des le lancement (BRSaves)
		Ch.bPrefixAtStart = BRSaves::SlotName(0) == FString(ChapterPrefix) + TEXT("1");
		BRSaves::SetTestPrefix(ChapterPrefix);
		TestOrigSlot = BRSaves::ActiveSlot();
		TestOrigSave = PC->ActiveSave;
		bTestDevSession = PC->bDevSession;
		const int32 Lot = BRContent::CurrentLot(BRLevels::Channel());
		Note(FString::Printf(TEXT("chapitre : binaire %s, canal %d (publie %s, chapitre %d), parcours de la partie : lineaire ; parties de test %s* des le lancement %s (parties du "
			"joueur ni lues ni ecrites)"), BuildName(), static_cast<int32>(BRLevels::Channel()), Oui(bPublic), Lot, ChapterPrefix, Oui(Ch.bPrefixAtStart)), !Ch.bPrefixAtStart);
		if (!bPublic)
		{
			Skip(FString::Printf(TEXT("chapitre : contenu de ce binaire autre que la version publiee (canal %d) : la traversee du premier chapitre publie ne s'applique pas"),
				static_cast<int32>(BRLevels::Channel())));
			Ch.bBroken = true;
			Ch.BrokenWhy = TEXT("canal non publie");
			return true;
		}
		if (Phase >= 2 && !bPrevOk)
		{
			Skip(FString::Printf(TEXT("chapitre, phase %d : la phase %d n'a pas laisse d'etat valable (%s) : phase non jouee"), Phase, Phase - 1,
				!Prev.bMarker ? TEXT("aucun etat") : *FString::Printf(TEXT("phase %d, valide %s, fin %s"), Prev.MarkerPhase, Oui(Prev.bMarkerValid), Oui(Prev.bMarkerEnd))));
			Ch.bBroken = true;
			Ch.BrokenWhy = TEXT("phase precedente incomplete");
			return true;
		}
		if (Phase <= 1)
		{
			// Une traversee neuve : aucune partie de test, aucun etat d'une traversee precedente
			DeleteSlotFiles();
			IFileManager::Get().Delete(*MarkerPath(), false, true, true);
		}
		else
		{
			Note(FString::Printf(TEXT("chapitre, phase %d : etat de la phase %d relu (emplacement %d, graine du parcours %u, Poolrooms %u, Niveau 2 %u)"), Phase, Phase - 1, Ch.Slot + 1,
				Ch.RunSeed, Ch.PoolSeed, Ch.Level2Seed));
		}
		return true;
	});
	LimitLastStep(100.f);

	// Etape de la chaine qui n'a pas d'objet si la traversee est deja interrompue
	auto Broken = [this](const TCHAR* What) -> bool
	{
		if (!BRTestV416Ship::Ch.bBroken)
		{
			return false;
		}
		if (!BRTestV416Ship::Ch.BrokenWhy.IsEmpty())
		{
			Skip(FString::Printf(TEXT("chapitre : %s non verifie (traversee interrompue plus haut : %s)"), What, *BRTestV416Ship::Ch.BrokenWhy));
		}
		return true;
	};
	// Etapes partagees (marche, mission, sortie) sautees d'un bloc quand la traversee est deja interrompue (avant : elles
	// cherchaient leur niveau et notaient chacune "non atteint") : une etape de garde, puis les etapes du bloc
	auto Guarded = [this](const TFunction<void()>& AddBlock)
	{
		const int32 Guard = Plan.Num();
		Add(TEXT("chapitre : suite de la traversee"), 0.f, []() { return true; });
		AddBlock();
		const int32 End = Plan.Num();
		Plan[Guard].Action = [this, End]()
		{
			if (BRTestV416Ship::Ch.bBroken)
			{
				StepIndex = End - 1; // l'index avance apres cette etape : premiere etape apres le bloc
			}
			return true;
		};
	};
	// Arrivee dans un niveau du fil par une sortie prise a pied : la partie relue du disque apres l'ecriture de l'arrivee
	auto AddArrival = [this, Broken](int32 Level, int32 From)
	{
		Add(FString::Printf(TEXT("chapitre : arrivee au Niveau %d"), Level), 3.f, [this, Broken, Level, From]()
		{
			if (Broken(*FString::Printf(TEXT("arrivee au Niveau %d"), Level)))
			{
				return true;
			}
			ABRWorld* W = GetBRWorld();
			if (!W)
			{
				return false;
			}
			const FString NotVerified = From >= 0 ? BRTestV412::ExitNotVerified(From) : FString();
			if (!NotVerified.IsEmpty())
			{
				Skip(FString::Printf(TEXT("chapitre : %s : arrivee au Niveau %d non verifiee"), *NotVerified, Level));
				BRTestV416Ship::Ch.bBroken = true;
				BRTestV416Ship::Ch.BrokenWhy.Reset(); // la cause est deja notee
				return true;
			}
			if (W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != Level)
			{
				if (StepTime > 45.f)
				{
					// La sortie du niveau precedent a deja note son echec (sortie non atteinte, pas de depart) : la suite n'est pas jugee
					Skip(FString::Printf(TEXT("chapitre : Niveau %d non atteint (niveau actuel %d) : suite de la traversee non verifiee"), Level, W->GetLevelNumber()));
					BRTestV416Ship::Ch.bBroken = true;
					BRTestV416Ship::Ch.BrokenWhy.Reset();
					return true;
				}
				return false;
			}
			if (ABRCharacter* C = GetPlayer())
			{
				C->bGodMode = true;
			}
			Report().Level = Level;
			BRTestV416Ship::Ch.Arrivals.Add(Level);
			BRTestV416Ship::Ch.ReadClock = 0.0;
			return true;
		});
		LimitLastStep(60.f);
		Add(FString::Printf(TEXT("chapitre : Niveau %d, partie relue"), Level), 0.f, [this, Level, From]() -> bool
		{
			using namespace BRTestV416Ship;
			ABRWorld* W = GetBRWorld();
			ABRPlayerController* PC = GetPC();
			if (Ch.bBroken || !W || !PC)
			{
				return true;
			}
			// L'ecriture de l'arrivee (2 s apres le chargement) peut tomber pendant le fondu d'arrivee (attente des shaders, 8 s au
			// plus) : ABRPlayerController::WriteActiveSave n'y ecrit pas encore la session du niveau, la suivante le fait
			// (enregistrement automatique, 60 s). Le fichier est relu chaque seconde jusqu'a la session du niveau, 70 s au plus
			const double Now = FPlatformTime::Seconds();
			if (Now - Ch.ReadClock < 1.0)
			{
				return false;
			}
			Ch.ReadClock = Now;
			BRSaves::Flush();
			const UBRSaveGame* G = BRSaves::Load(Ch.Slot);
			if (!(G && G->Session.bValid && G->Session.Level == Level && G->Session.Seed == W->GetSeed()) && StepTime < 70.f && W->GetLevelNumber() == Level)
			{
				return false;
			}
			const BRStory::FState St = G ? G->GetStory() : BRStory::FState();
			const uint32 WantSeed = Level == 0 ? St.IntroSeed : BRStory::LevelSeedFor(St, Level);
			const bool bFile = G && G->LoadedVersion == UBRSaveGame::CurrentVersion && G->SaveName == ChapterSaveName && BRStory::IsLinear(St) && St.RunSeed == Ch.RunSeed
				&& (From < 0 || BRStory::IsDone(St, From)) && St.Resume == Level && St.Reached == BRStory::IndexOf(Level) && G->IsExplored(Level) && G->CurrentLevel == Level
				&& G->Session.bValid && G->Session.Level == Level && G->Session.Seed == W->GetSeed() && G->Deaths == Ch.Deaths && !G->bPendingDeath;
			const bool bWorld = W->IsStorySession() && W->IsLinearLevel() && W->GetNetLevelFlags() == 1 && WantSeed != 0 && W->GetSeed() == WantSeed
				&& BRSaves::ActiveSlot() == Ch.Slot;
			Note(FString::Printf(TEXT("chapitre, Niveau %d : partie relue du disque (format %d, %.0f s apres l'arrivee) : %s accompli %s, reprise %d, plus loin %d, explore %s, session du "
				"Niveau %d graine %u (monde %u, attendue %u), parcours lineaire %s (graine %u), morts %d ; monde : session d'histoire %s, niveau lineaire %s"), Level,
				G ? G->LoadedVersion : -1, StepTime + 3.f, From < 0 ? TEXT("depart") : *FString::Printf(TEXT("Niveau %d"), From), Oui(From < 0 || BRStory::IsDone(St, From)), St.Resume,
				St.Reached, Oui(G && G->IsExplored(Level)), G ? G->Session.Level : -1, G ? G->Session.Seed : 0u, W->GetSeed(), WantSeed, Oui(BRStory::IsLinear(St)), St.RunSeed,
				G ? G->Deaths : -1, Oui(W->IsStorySession()), Oui(W->IsLinearLevel())), !bFile || !bWorld);
			Shot(FString::Printf(TEXT("chapitre_arrivee_%d"), Level));
			return true;
		});
		LimitLastStep(80.f);
	};

	if (Phase <= 1)
	{
		// ------------------------------------------------------------------------------------------------- Nouvelle partie
		Add(TEXT("chapitre : NOUVELLE PARTIE par le menu"), 0.f, [this, Broken]()
		{
			using namespace BRTestV416Ship;
			if (Broken(TEXT("NOUVELLE PARTIE")))
			{
				return true;
			}
			ABRWorld* W = GetBRWorld();
			ABRPlayerController* PC = GetPC();
			if (!W || !PC || W->IsTransitioning())
			{
				return false;
			}
			// Le vrai menu : SOLO (page des parties relue du disque), puis NOUVELLE PARTIE -> COMMENCER
			PC->SetInventoryOpen(false);
			PC->bHostFlow = false;
			PC->bInMenu = true;
			PC->SetMenuPage(EBRMenuPage::Main);
			PC->UpdateInputMode();
			PC->MenuActivate(0); // SOLO
			const bool bSaves = PC->MenuPage == EBRMenuPage::Saves;
			const int32 Cards = PC->GetSaveOrder().Num();
			Ch.Serial = W->GetLevelSerial();
			if (Ch.ForcedRunSeed != 0)
			{
				// Disposition donnee : la partie que NOUVELLE PARTIE ecrirait, avec ces graines, prise par VOS PARTIES puis CONTINUER
				UBRSaveGame* S = NewObject<UBRSaveGame>();
				S->SaveName = ChapterSaveName;
				S->Created = FDateTime::Now();
				S->CurrentLevel = 0;
				S->MarkExplored(0);
				BRStory::FState St;
				St.Flags |= BRStory::Flag::Linear;
				St.RunSeed = Ch.ForcedRunSeed;
				St.IntroSeed = Ch.ForcedIntro;
				S->SetStory(St);
				Ch.Slot = BRSaves::FreeSlot();
				const bool bWritten = Ch.Slot != INDEX_NONE && BRSaves::Write(Ch.Slot, S);
				PC->SetMenuPage(EBRMenuPage::Saves);
				int32 Item = INDEX_NONE;
				for (int32 It = 0; It < PC->GetMenuItemCount(); ++It)
				{
					Item = PC->GetMenuSaveSlot(It) == Ch.Slot ? It : Item;
				}
				if (Item != INDEX_NONE)
				{
					PC->MenuActivate(Item); // la partie : page SOLO (CONTINUER, REJOUER)
				}
				const bool bSolo = PC->MenuPage == EBRMenuPage::Solo;
				if (bSolo)
				{
					PC->MenuActivate(0); // CONTINUER
				}
				Note(FString::Printf(TEXT("chapitre : disposition donnee (-BRChapterSeed=%u -BRChapterIntro=%u) : partie ecrite %s (emplacement %d), carte %s, CONTINUER %s"),
					Ch.ForcedRunSeed, Ch.ForcedIntro, Oui(bWritten), Ch.Slot + 1, Oui(Item != INDEX_NONE), Oui(bSolo)), !bWritten || Item == INDEX_NONE || !bSolo);
				return true;
			}
			int32 NewItem = INDEX_NONE;
			for (int32 It = 0; It < PC->GetMenuItemCount(); ++It)
			{
				NewItem = PC->GetMenuSaveSlot(It) == ABRPlayerController::MenuItemNew ? It : NewItem;
			}
			if (!bSaves || NewItem == INDEX_NONE)
			{
				Note(FString::Printf(TEXT("chapitre : page VOS PARTIES %s, NOUVELLE PARTIE %s (%d partie(s) de test)"), Oui(bSaves), Oui(NewItem != INDEX_NONE), Cards), true);
				Ch.bBroken = true;
				Ch.BrokenWhy = TEXT("NOUVELLE PARTIE impossible");
				return true;
			}
			PC->MenuActivate(NewItem);
			const bool bPage = PC->MenuPage == EBRMenuPage::NewSave;
			PC->NewSaveName = ChapterSaveName; // le nom tape dans le champ de la page
			Ch.Slot = BRSaves::FreeSlot();
			PC->MenuActivate(0); // COMMENCER : ABRPlayerController::StartNewSave
			Note(FString::Printf(TEXT("chapitre : SOLO -> VOS PARTIES (%d partie(s) de test) -> NOUVELLE PARTIE %s -> COMMENCER : emplacement %d"), Cards, Oui(bPage), Ch.Slot + 1),
				!bPage || Ch.Slot == INDEX_NONE);
			return true;
		});
		Add(TEXT("chapitre : Level 0 de la nouvelle partie"), 3.f, [this, Broken]()
		{
			using namespace BRTestV416Ship;
			if (Broken(TEXT("Level 0 de la nouvelle partie")))
			{
				return true;
			}
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			ABRPlayerController* PC = GetPC();
			if (!W || !C || !PC || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == Ch.Serial || W->GetLevelNumber() != 0)
			{
				if (StepTime > 90.f)
				{
					Note(TEXT("chapitre : la nouvelle partie n'a pas commence au Level 0 en 90 s"), true);
					Ch.bBroken = true;
					Ch.BrokenWhy = TEXT("Level 0 non charge");
					return true;
				}
				return false;
			}
			C->bGodMode = true;
			Report().Level = 0;
			Ch.Arrivals.Add(0);
			const BRStory::FState St = W->GetStory();
			Ch.RunSeed = St.RunSeed;
			Ch.IntroSeed = W->GetSeed();
			Ch.ReadClock = 0.0;
			const bool bOk = W->IsStorySession() && W->IsLinearLevel() && BRStory::IsLinear(St) && St.RunSeed != 0 && St.IntroSeed == W->GetSeed() && BRSaves::ActiveSlot() == Ch.Slot
				&& (Ch.ForcedRunSeed == 0 || St.RunSeed == Ch.ForcedRunSeed) && (Ch.ForcedIntro == 0 || W->GetSeed() == Ch.ForcedIntro) && !W->IsMissionActive();
			Note(FString::Printf(TEXT("chapitre : Level 0 de la partie : session d'histoire %s, niveau lineaire %s, graine du parcours %u, graine du Level 0 %u, mission %s ; pour "
				"rejouer cette disposition : -BRChapterSeed=%u -BRChapterIntro=%u"), Oui(W->IsStorySession()), Oui(W->IsLinearLevel()), St.RunSeed, W->GetSeed(),
				W->IsMissionActive() ? TEXT("ACTIVE") : TEXT("aucune (Level 0 sans enigme)"), St.RunSeed, W->GetSeed()), !bOk);
			return true;
		});
		LimitLastStep(100.f);
		Add(TEXT("chapitre : Level 0, partie relue"), 0.f, [this]()
		{
			using namespace BRTestV416Ship;
			ABRWorld* W = GetBRWorld();
			if (Ch.bBroken || !W)
			{
				return true;
			}
			// Ecriture de l'arrivee (2 s apres le chargement) : la partie neuve est au format du jeu, lineaire, au Level 0 (relue
			// chaque seconde jusqu'a la graine du Level 0, 20 s au plus)
			const double Now = FPlatformTime::Seconds();
			if (Now - Ch.ReadClock < 1.0)
			{
				return false;
			}
			Ch.ReadClock = Now;
			BRSaves::Flush();
			const UBRSaveGame* G = BRSaves::Load(Ch.Slot);
			const BRStory::FState St = G ? G->GetStory() : BRStory::FState();
			if (St.IntroSeed != Ch.IntroSeed && StepTime < 20.f)
			{
				return false;
			}
			const bool bOk = G && G->LoadedVersion == UBRSaveGame::CurrentVersion && G->SaveName == ChapterSaveName && BRStory::IsLinear(St) && St.RunSeed == Ch.RunSeed
				&& St.IntroSeed == Ch.IntroSeed && G->IsExplored(0) && G->Deaths == 0 && St.Done == 0;
			Note(FString::Printf(TEXT("chapitre : nouvelle partie relue du disque : format %d (jeu %d), nom \"%s\", parcours lineaire %s, graine du parcours %u, graine du Level 0 %u, "
				"etapes 0x%x, morts %d"), G ? G->LoadedVersion : -1, UBRSaveGame::CurrentVersion, G ? *G->SaveName : TEXT("-"), Oui(BRStory::IsLinear(St)), St.RunSeed,
				St.IntroSeed, St.Done, G ? G->Deaths : -1), !bOk);
			Shot(TEXT("chapitre_level0"));
			return true;
		});
		LimitLastStep(30.f);
		// Level 0 -> Niveau 1 : la porte de service, a pied, ouverte par la touche d'interaction (BRAutoTestV413.cpp)
		Guarded([this]() { AddServiceDoorWalkSteps(TEXT("chapitre"), true); });
		AddArrival(1, 0);
		// Niveau 1 -> Poolrooms : mission resolue en marchant, sortie prise a pied (BRAutoTestV412.cpp)
		Guarded([this]()
		{
			AddWalkMissionSteps(1, 0u, false);
			AddWalkExitSteps(1);
		});
		AddArrival(BRStory::Poolrooms, 1);
		Guarded([this]() { AddWalkMissionSteps(BRStory::Poolrooms, 0u, false); });
		Add(TEXT("chapitre : Poolrooms resolues, fermeture du jeu"), 0.f, [this, Broken]()
		{
			using namespace BRTestV416Ship;
			if (Broken(TEXT("fermeture aux Poolrooms")))
			{
				return true;
			}
			ABRWorld* W = GetBRWorld();
			ABRPlayerController* PC = GetPC();
			if (!W || !PC)
			{
				return false;
			}
			const FString NotVerified = BRTestV412::MissionNotVerified(W, BRStory::Poolrooms);
			if (!NotVerified.IsEmpty())
			{
				Skip(FString::Printf(TEXT("chapitre : %s : fermeture et reprise non verifiees"), *NotVerified));
				Ch.bBroken = true;
				Ch.BrokenWhy.Reset();
				return true;
			}
			BRMission::FEval E;
			W->GetMissionEval(E);
			if (W->GetLevelNumber() != BRStory::Poolrooms || !E.bSolved)
			{
				Note(FString::Printf(TEXT("chapitre : Poolrooms %s, mission resolue %s : fermeture non faite"), Oui(W->GetLevelNumber() == BRStory::Poolrooms), Oui(E.bSolved)), true);
				Ch.bBroken = true;
				Ch.BrokenWhy = TEXT("mission des Poolrooms non resolue");
				return true;
			}
			// Fermeture du jeu : l'ecriture bloquante de QUITTER (ABRPlayerController::QuitToDesktop). Phase 1 : le jeu se ferme
			// vraiment juste apres (fin du plan) ; un lancement : la reprise relit ce fichier par le menu
			PC->WriteActiveSave(true);
			BRSaves::Flush();
			const UBRSaveGame* G = BRSaves::Load(Ch.Slot);
			const TArray<uint8> Blob = W->GetMissionBlob();
			const BRStory::FState St = G ? G->GetStory() : BRStory::FState();
			FString ExitWhat;
			const bool bExit = ForwardOpen(GetWorld(), W, BRStory::NextOf(BRStory::Poolrooms), ExitWhat);
			const bool bOk = G && G->Session.bValid && G->Session.Level == BRStory::Poolrooms && G->Session.Seed == W->GetSeed() && G->Session.Mission == Blob
				&& BRStory::IsDone(St, 1) && St.Resume == BRStory::Poolrooms && BRStory::IsLinear(St) && bExit;
			Ch.PoolSeed = W->GetSeed();
			Ch.PoolMission = Hex(Blob);
			Ch.Deaths = G ? G->Deaths : 0;
			Ch.bClosed = bOk;
			Note(FString::Printf(TEXT("chapitre : fermeture aux Poolrooms resolues (ecriture de QUITTER) : session du Niveau %d, graine %u (monde %u), etat de mission identique %s "
				"(%d octets), sortie vers le Niveau 2 ouverte %s (%s), reprise %d"), G ? G->Session.Level : -1, G ? G->Session.Seed : 0u, W->GetSeed(), Oui(G && G->Session.Mission == Blob),
				Blob.Num(), Oui(bExit), *ExitWhat, St.Resume), !bOk);
			Shot(TEXT("chapitre_fermeture"));
			return true;
		});
	}

	if (Phase == 0 || Phase == 2)
	{
		// ------------------------------------------------------------------------------------------------- Reprise
		Add(TEXT("chapitre : relance, VOS PARTIES, CONTINUER"), 0.f, [this, Broken, Phase]()
		{
			using namespace BRTestV416Ship;
			if (Broken(TEXT("reprise par CONTINUER")))
			{
				return true;
			}
			ABRWorld* W = GetBRWorld();
			ABRPlayerController* PC = GetPC();
			if (!W || !PC || W->IsTransitioning())
			{
				return false;
			}
			if (Phase == 0 && !Ch.bClosed)
			{
				Skip(TEXT("chapitre : fermeture aux Poolrooms non faite : reprise non verifiee"));
				Ch.bBroken = true;
				Ch.BrokenWhy.Reset();
				return true;
			}
			// La page VOS PARTIES relit les parties du disque ; la carte de la partie, puis CONTINUER
			PC->SetInventoryOpen(false);
			PC->bHostFlow = false;
			PC->bInMenu = true;
			PC->SetMenuPage(EBRMenuPage::Main);
			PC->UpdateInputMode();
			PC->MenuActivate(0); // SOLO
			int32 Item = INDEX_NONE;
			for (int32 It = 0; It < PC->GetMenuItemCount(); ++It)
			{
				Item = PC->GetMenuSaveSlot(It) == Ch.Slot ? It : Item;
			}
			const UBRSaveGame* Card = PC->GetSaveInSlot(Ch.Slot);
			const bool bCard = Card && Card->SaveName == ChapterSaveName && BRStory::IsLinear(Card->GetStory()) && Card->Session.bValid && Card->Session.Level == BRStory::Poolrooms
				&& Card->Session.Seed == Ch.PoolSeed;
			if (Item == INDEX_NONE || !Card)
			{
				Note(FString::Printf(TEXT("chapitre : relance : partie de test absente de VOS PARTIES (emplacement %d)"), Ch.Slot + 1), true);
				Ch.bBroken = true;
				Ch.BrokenWhy = TEXT("partie absente");
				return true;
			}
			PC->MenuActivate(Item); // page SOLO de la partie
			const int32 Cont = PC->GetContinueLevel();
			const bool bSolo = PC->MenuPage == EBRMenuPage::Solo;
			Ch.Serial = W->GetLevelSerial();
			if (bSolo)
			{
				PC->MenuActivate(0); // CONTINUER
			}
			Note(FString::Printf(TEXT("chapitre : relance%s : carte de la partie (LINEAIRE %s, session des Poolrooms graine %u) %s, CONTINUER vers le Niveau %d (attendu %d)"),
				Phase == 2 ? TEXT(" apres une vraie fermeture du jeu") : TEXT(""), Oui(Card && BRStory::IsLinear(Card->GetStory())), Card ? Card->Session.Seed : 0u, Oui(bCard), Cont,
				BRStory::Poolrooms), !bCard || !bSolo || Cont != BRStory::Poolrooms);
			return true;
		});
		Add(TEXT("chapitre : reprise aux Poolrooms"), 1.f, [this, Broken]()
		{
			using namespace BRTestV416Ship;
			if (Broken(TEXT("reprise aux Poolrooms")))
			{
				return true;
			}
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == Ch.Serial || W->GetLevelNumber() != BRStory::Poolrooms)
			{
				if (StepTime > 90.f)
				{
					Note(TEXT("chapitre : CONTINUER n'a pas recharge les Poolrooms en 90 s"), true);
					Ch.bBroken = true;
					Ch.BrokenWhy = TEXT("Poolrooms non rechargees");
					return true;
				}
				return false;
			}
			C->bGodMode = true;
			// Streaming au repos autour de la place reprise (la sortie du sas doit etre construite pour la marche qui suit)
			if (W->GetPreparingChunkCount() > 0 && StepTime < 45.f)
			{
				return false;
			}
			Report().Level = BRStory::Poolrooms;
			BRMission::FEval E;
			W->GetMissionEval(E);
			FString ExitWhat;
			const bool bExit = ForwardOpen(GetWorld(), W, BRStory::NextOf(BRStory::Poolrooms), ExitWhat);
			const FString Blob = Hex(W->GetMissionBlob());
			const bool bOk = W->GetSeed() == Ch.PoolSeed && Blob == Ch.PoolMission && E.bSolved && W->IsLinearLevel() && W->IsStorySession() && bExit && !C->IsDead();
			Ch.bResumed = bOk;
			Note(FString::Printf(TEXT("chapitre : reprise aux Poolrooms : graine %u (avant la fermeture %u), etat de mission identique %s, mission resolue %s, sortie vers le Niveau 2 "
				"ouverte %s (%s), niveau lineaire %s"), W->GetSeed(), Ch.PoolSeed, Oui(Blob == Ch.PoolMission), Oui(E.bSolved), Oui(bExit), *ExitWhat, Oui(W->IsLinearLevel())), !bOk);
			Shot(TEXT("chapitre_reprise"));
			return true;
		});
		LimitLastStep(100.f);
		Guarded([this]() { AddWalkExitSteps(BRStory::Poolrooms); });
		AddArrival(2, BRStory::Poolrooms);
		Guarded([this]()
		{
			AddWalkMissionSteps(2, 0u, false);
			AddWalkExitSteps(2);
		});
		Add(TEXT("chapitre : fin du chapitre enregistree par le jeu"), 0.f, [this, Broken]()
		{
			using namespace BRTestV416Ship;
			if (Broken(TEXT("fin du chapitre")))
			{
				return true;
			}
			ABRWorld* W = GetBRWorld();
			ABRPlayerController* PC = GetPC();
			if (!W || !PC)
			{
				return false;
			}
			const FString NotVerified = BRTestV412::ExitNotVerified(2);
			if (!NotVerified.IsEmpty())
			{
				Skip(FString::Printf(TEXT("chapitre : %s : fin du chapitre non verifiee"), *NotVerified));
				Ch.bBroken = true;
				Ch.BrokenWhy.Reset();
				return true;
			}
			if (StepTime < 1.5f)
			{
				return false; // ecriture de la fin (ABRPlayerController::OnEndingShown, sur un thread de fond)
			}
			// Le fichier tel que le jeu l'a ecrit a l'ecran de fin (aucune ecriture du test)
			BRSaves::Flush();
			const UBRSaveGame* G = BRSaves::Load(Ch.Slot);
			const BRStory::FState St = G ? G->GetStory() : BRStory::FState();
			const int32 Lot = BRContent::CurrentLot(BRLevels::Channel());
			const BRStory::FState Live = W->GetStory();
			Ch.bChapterEnd = (Live.ChapterEnds & (1u << Lot)) != 0 && W->GetLevelNumber() == 2;
			BRMission::FEval E;
			W->GetMissionEval(E);
			const int32 Cont = G ? ResumeOf(St) : -1;
			const bool bOk = Ch.bChapterEnd && G && (St.ChapterEnds & (1u << Lot)) != 0 && BRStory::IsDone(St, 0) && BRStory::IsDone(St, 1) && BRStory::IsDone(St, BRStory::Poolrooms)
				&& BRStory::IsDone(St, 2) && (St.Events & BRStory::Event::StationSign) != 0 && (G->Endings & (1 << (1 + Lot))) != 0 && BRStory::IsLinear(St) && St.RunSeed == Ch.RunSeed
				&& Cont == 2 && G->Deaths == Ch.Deaths && G->Session.bValid && G->Session.Level == 2 && E.bSolved;
			Ch.bEndSaved = bOk;
			Ch.Level2Seed = W->GetSeed();
			Ch.Level2Mission = G ? Hex(G->Session.Mission) : FString();
			Note(FString::Printf(TEXT("chapitre : fin du chapitre %d : atteinte %s ; partie ecrite par le jeu et relue : fin du chapitre %s, etapes 0x%x (0, 1, 37 et 2 accomplis %s), marque "
				"de L. %s, fin vue %s, parcours lineaire %s, CONTINUER au Niveau %d (attendu 2 tant que le Niveau 3 n'est pas publie), session du Niveau 2 %s, morts %d"), Lot,
				Oui(Ch.bChapterEnd), Oui((St.ChapterEnds & (1u << Lot)) != 0), St.Done, Oui(BRStory::IsDone(St, 2) && BRStory::IsDone(St, BRStory::Poolrooms) && BRStory::IsDone(St, 1)
				&& BRStory::IsDone(St, 0)), Oui((St.Events & BRStory::Event::StationSign) != 0), Oui(G && (G->Endings & (1 << (1 + Lot))) != 0), Oui(BRStory::IsLinear(St)), Cont,
				Oui(G && G->Session.bValid && G->Session.Level == 2), G ? G->Deaths : -1), !bOk);
			if (Ch.bChapterEnd)
			{
				Ch.Arrivals.Add(-2); // la fin du chapitre
			}
			return true;
		});
	}

	if (Phase == 0 || Phase == 3)
	{
		// ------------------------------------------------------------------------------------------------- Apres la fin
		Add(TEXT("chapitre : apres la fin, VOS PARTIES et CONTINUER"), 0.f, [this, Broken, Phase]()
		{
			using namespace BRTestV416Ship;
			if (Broken(TEXT("partie apres la fin du chapitre")))
			{
				return true;
			}
			ABRWorld* W = GetBRWorld();
			ABRPlayerController* PC = GetPC();
			if (!W || !PC || W->IsTransitioning())
			{
				return false;
			}
			if (Phase == 0 && !Ch.bEndSaved)
			{
				Skip(TEXT("chapitre : fin du chapitre non enregistree plus haut : partie apres la fin non verifiee"));
				Ch.bBroken = true;
				Ch.BrokenWhy.Reset();
				return true;
			}
			PC->SetInventoryOpen(false);
			PC->bHostFlow = false;
			PC->bInMenu = true;
			PC->SetMenuPage(EBRMenuPage::Main);
			PC->UpdateInputMode();
			PC->MenuActivate(0); // SOLO : parties relues du disque
			int32 Item = INDEX_NONE;
			for (int32 It = 0; It < PC->GetMenuItemCount(); ++It)
			{
				Item = PC->GetMenuSaveSlot(It) == Ch.Slot ? It : Item;
			}
			const UBRSaveGame* Card = PC->GetSaveInSlot(Ch.Slot);
			const int32 Lot = BRContent::CurrentLot(BRLevels::Channel());
			const BRStory::FState St = Card ? Card->GetStory() : BRStory::FState();
			if (Item == INDEX_NONE || !Card)
			{
				Note(FString::Printf(TEXT("chapitre : apres la fin : partie de test absente de VOS PARTIES (emplacement %d)"), Ch.Slot + 1), true);
				Ch.bBroken = true;
				Ch.BrokenWhy = TEXT("partie absente");
				return true;
			}
			PC->MenuActivate(Item);
			Ch.Continue = PC->GetContinueLevel();
			const bool bSolo = PC->MenuPage == EBRMenuPage::Solo;
			const bool bCard = (St.ChapterEnds & (1u << Lot)) != 0 && BRStory::IsDone(St, 2) && BRStory::IsLinear(St) && St.RunSeed == Ch.RunSeed && Card->Deaths == Ch.Deaths;
			Ch.Serial = W->GetLevelSerial();
			if (bSolo)
			{
				PC->MenuActivate(0); // CONTINUER
			}
			Note(FString::Printf(TEXT("chapitre : %s : carte de la partie : fin du chapitre %d %s, Niveau 2 accompli %s, parcours lineaire %s ; CONTINUER vers le Niveau %d (attendu 2)"),
				Phase == 3 ? TEXT("apres un nouveau redemarrage du jeu") : TEXT("apres la fin"), Lot, Oui((St.ChapterEnds & (1u << Lot)) != 0), Oui(BRStory::IsDone(St, 2)),
				Oui(BRStory::IsLinear(St)), Ch.Continue), !bCard || !bSolo || Ch.Continue != 2);
			return true;
		});
		Add(TEXT("chapitre : Niveau 2 apres la fin"), 1.f, [this, Broken]()
		{
			using namespace BRTestV416Ship;
			if (Broken(TEXT("Niveau 2 apres la fin")))
			{
				return true;
			}
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == Ch.Serial || W->GetLevelNumber() != 2)
			{
				if (StepTime > 90.f)
				{
					Note(TEXT("chapitre : CONTINUER apres la fin n'a pas charge le Niveau 2 en 90 s"), true);
					Ch.bBroken = true;
					Ch.BrokenWhy = TEXT("Niveau 2 non charge");
					return true;
				}
				return false;
			}
			C->bGodMode = true;
			Report().Level = 2;
			BRMission::FEval E;
			W->GetMissionEval(E);
			const int32 Lot = BRContent::CurrentLot(BRLevels::Channel());
			const BRStory::FState St = W->GetStory();
			const FString Blob = Hex(W->GetMissionBlob());
			FString ExitWhat;
			const bool bExit = ForwardOpen(GetWorld(), W, BRStory::NextOf(2), ExitWhat);
			const bool bOk = W->GetSeed() == Ch.Level2Seed && Blob == Ch.Level2Mission && E.bSolved && (St.ChapterEnds & (1u << Lot)) != 0 && W->IsLinearLevel() && W->IsStorySession()
				&& bExit;
			Ch.bAfterRestart = bOk;
			Note(FString::Printf(TEXT("chapitre : Niveau 2 repris apres la fin : graine %u (attendue %u), etat de mission identique %s, mission accomplie %s, sortie de la fin ouverte %s (%s), "
				"fin du chapitre gardee %s, niveau lineaire %s"), W->GetSeed(), Ch.Level2Seed, Oui(Blob == Ch.Level2Mission), Oui(E.bSolved), Oui(bExit), *ExitWhat,
				Oui((St.ChapterEnds & (1u << Lot)) != 0), Oui(W->IsLinearLevel())), !bOk);
			Shot(TEXT("chapitre_apres_la_fin"));
			return true;
		});
		LimitLastStep(100.f);
		Add(TEXT("chapitre : nettoyage des parties de test"), 0.f, [this]()
		{
			using namespace BRTestV416Ship;
			ABRPlayerController* PC = GetPC();
			// Aucune partie active : ni l'ecriture differee, ni celle de la fermeture du jeu ne recreent une partie de test
			SwapTestSave(PC, nullptr, INDEX_NONE);
			if (PC)
			{
				PC->bDevSession = bTestDevSession;
			}
			TestOrigSave = nullptr;
			BRSaves::ReplaySession() = false;
			DeleteSlotFiles();
			IFileManager::Get().Delete(*MarkerPath(), false, true, true);
			const int32 Left = CountTestFiles() + (IFileManager::Get().FileExists(*MarkerPath()) ? 1 : 0);
			Ch.bCleaned = Left == 0;
			if (ABRCharacter* C = GetPlayer())
			{
				C->bGodMode = false;
			}
			Note(FString::Printf(TEXT("chapitre : parties de test et etat de la traversee effaces (%d fichier(s) restant(s)) ; aucune partie active"), Left), Left != 0);
			return true;
		});
	}

	// ------------------------------------------------------------------------------------------------- Bilan de la phase
	Add(TEXT("chapitre : bilan"), 0.f, [this, Phase]()
	{
		using namespace BRTestV416Ship;
		const bool bPhaseDone = Phase == 1 ? Ch.bClosed : (Phase == 2 ? Ch.bResumed && Ch.bEndSaved : (Phase == 3 ? Ch.bAfterRestart && Ch.Continue == 2 && Ch.bCleaned
			: Ch.bClosed && Ch.bResumed && Ch.bEndSaved && Ch.bAfterRestart && Ch.Continue == 2 && Ch.bCleaned));
		const bool bValid = bPhaseDone && Problems.Num() == 0 && Skipped.Num() == 0 && !Ch.bBroken;
		if (Phase == 1 || Phase == 2)
		{
			WriteMarker(Ch, bValid);
		}
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("phase"), Phase);
		Root->SetStringField(TEXT("binaire"), BuildName());
		Root->SetStringField(TEXT("canal"), BRLevels::Channel() == BRContent::EChannel::Public ? TEXT("public") : (BRLevels::Channel() == BRContent::EChannel::Internal ? TEXT("internal") : TEXT("all")));
		Root->SetNumberField(TEXT("chapitre"), BRContent::CurrentLot(BRLevels::Channel()));
		Root->SetBoolField(TEXT("parties_de_test_des_le_lancement"), Ch.bPrefixAtStart);
		Root->SetNumberField(TEXT("graine_parcours"), Ch.RunSeed);
		Root->SetNumberField(TEXT("graine_level0"), Ch.IntroSeed);
		TArray<TSharedPtr<FJsonValue>> Levels;
		for (const int32 L : Ch.Arrivals)
		{
			if (L >= 0)
			{
				Levels.Add(MakeShared<FJsonValueNumber>(L));
			}
		}
		Root->SetArrayField(TEXT("niveaux"), Levels);
		Root->SetBoolField(TEXT("fermeture"), Ch.bClosed);
		Root->SetBoolField(TEXT("reprise"), Ch.bResumed);
		Root->SetBoolField(TEXT("fin_du_chapitre"), Ch.bChapterEnd);
		Root->SetBoolField(TEXT("fin_enregistree"), Ch.bEndSaved);
		Root->SetBoolField(TEXT("apres_relance"), Ch.bAfterRestart);
		Root->SetNumberField(TEXT("continuer"), Ch.Continue);
		Root->SetBoolField(TEXT("nettoyage"), Ch.bCleaned);
		Root->SetNumberField(TEXT("problemes"), Problems.Num());
		Root->SetNumberField(TEXT("non_verifies"), Skipped.Num());
		Root->SetBoolField(TEXT("valide"), bValid);
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		FFileHelper::SaveStringToFile(Json, *FPaths::Combine(OutDir, TEXT("Chapitre.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString Extra;
		if (ParsePathOption(TEXT("BRChapterOut="), Extra))
		{
			FFileHelper::SaveStringToFile(Json, *Extra, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}
		FString Path;
		for (const int32 L : Ch.Arrivals)
		{
			Path += (Path.IsEmpty() ? TEXT("") : TEXT(" -> ")) + (L == -2 ? FString(TEXT("fin du chapitre")) : FString::FromInt(L));
		}
		Note(FString::Printf(TEXT("chapitre, phase %d : %s ; niveaux atteints dans ce lancement : %s ; fermeture %s, reprise %s, fin du chapitre %s (enregistree %s), apres relance %s, "
			"nettoyage %s ; %d probleme(s), %d non verifie(s)"), Phase, bValid ? TEXT("PHASE VALIDE") : TEXT("phase non validee"), Path.IsEmpty() ? TEXT("aucun") : *Path,
			Oui(Ch.bClosed), Oui(Ch.bResumed), Oui(Ch.bChapterEnd), Oui(Ch.bEndSaved), Oui(Ch.bAfterRestart), Oui(Ch.bCleaned), Problems.Num(), Skipped.Num()));
		// Sortie : en phase 1 et 2, la partie de test reste active : l'ecriture de la fermeture du jeu (EndPlay) la complete
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = false;
		}
		return true;
	});
	Add(TEXT("Fin"), 0.f, [this]()
	{
		Finish();
		return true;
	});
}

// =====================================================================================================================
// REJOUER et sauvegardes
// =====================================================================================================================

void ABRAutoTest::AddV416ShipSteps()
{
	using namespace BRTestV416Ship;
	Sh = FShip();

	// Page VOS PARTIES (relue du disque) puis la carte de l'emplacement ; bReplay : REJOUER de ReplayLevel, sinon CONTINUER
	auto OpenSave = [this](int32 SaveSlot, bool bReplay, int32 ReplayLevel) -> FString
	{
		ABRPlayerController* PC = GetPC();
		ABRWorld* W = GetBRWorld();
		if (!PC || !W)
		{
			return TEXT("controleur ou monde absent");
		}
		PC->SetInventoryOpen(false);
		PC->bHostFlow = false;
		PC->bInMenu = true;
		PC->SetMenuPage(EBRMenuPage::Main);
		PC->UpdateInputMode();
		PC->MenuActivate(0); // SOLO
		int32 Item = INDEX_NONE;
		for (int32 It = 0; It < PC->GetMenuItemCount(); ++It)
		{
			Item = PC->GetMenuSaveSlot(It) == SaveSlot ? It : Item;
		}
		if (Item == INDEX_NONE)
		{
			return FString::Printf(TEXT("partie de l'emplacement %d absente de VOS PARTIES"), SaveSlot + 1);
		}
		PC->MenuActivate(Item);
		if (PC->MenuPage != EBRMenuPage::Solo)
		{
			return TEXT("page SOLO de la partie non ouverte");
		}
		Sh.Serial = W->GetLevelSerial();
		Sh.MenuClock = FPlatformTime::Seconds();
		if (bReplay)
		{
			PC->MenuIndex = FMath::Max(0, BRLevels::IndexOf(ReplayLevel)); // carrousel des niveaux decouverts
			PC->MenuActivate(1); // REJOUER
		}
		else
		{
			PC->MenuActivate(0); // CONTINUER
		}
		return FString();
	};
	// Une note pas encore au carnet, lue par l'effet de la touche d'interaction (ABRPickup::Collect) puis rangee (seconde touche)
	auto ReadNewNote = [this](const TArray<FString>& Known, FString& OutId, FString& OutHow) -> bool
	{
		ABRCharacter* C = GetPlayer();
		UWorld* World = GetWorld();
		if (!C || !World)
		{
			return false;
		}
		ABRPickup* Found = nullptr;
		for (TActorIterator<ABRPickup> It(World); It; ++It)
		{
			if (It->Item == EBRItem::Note && !It->NoteText.IsEmpty() && !Known.Contains(It->NoteText) && !C->ReadNotes.Contains(It->NoteText) && !It->IsActorBeingDestroyed())
			{
				Found = *It;
				break;
			}
		}
		OutHow = TEXT("note posee par le niveau");
		if (!Found)
		{
			// Aucune note neuve dans les salles construites : une note du jeu posee devant le joueur (meme objet, meme lecture)
			FString Id;
			for (const FString& N : KnownNotes())
			{
				if (Id.IsEmpty() && !Known.Contains(N) && !C->ReadNotes.Contains(N))
				{
					Id = N;
				}
			}
			if (Id.IsEmpty())
			{
				return false;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Found = World->SpawnActor<ABRPickup>(ABRPickup::StaticClass(), FTransform(C->GetActorLocation() + C->GetActorForwardVector() * 80.f), Params);
			if (!Found)
			{
				return false;
			}
			Found->Init(EBRItem::Note, 0, Id);
			Sh.Spawned = Found;
			OutHow = TEXT("note du jeu posee par le test (aucune note neuve dans les salles construites)");
		}
		OutId = Found->NoteText;
		Found->Collect(C);
		const bool bRead = C->ReadNotes.Contains(OutId);
		C->Interact(); // ranger la note
		return bRead;
	};

	// Journal du fichier apres le rejeu : rien de retire depuis la premiere ecriture du rejeu, rien d'autre que les notes lues et
	// les entites vues pendant cette session (union)
	auto JournalOk = [this](const UBRSaveGame* G) -> bool
	{
		const ABRWorld* W = GetBRWorld();
		const ABRCharacter* C = GetPlayer();
		if (!G)
		{
			return false;
		}
		TArray<FString> NotesAllowed = Sh.Notes;
		for (const FString& N : C ? C->ReadNotes : TArray<FString>())
		{
			NotesAllowed.AddUnique(N);
		}
		TArray<int32> KindsAllowed = Sh.Discovered;
		for (const int32 K : W ? W->GetDiscoveredList() : TArray<int32>())
		{
			KindsAllowed.AddUnique(K);
		}
		return BRTestV416Ship::Includes(G->Notes, Sh.NotesAfter) && BRTestV416Ship::Includes(NotesAllowed, G->Notes) && BRTestV416Ship::Includes(G->Discovered, Sh.DiscoveredAfter)
			&& BRTestV416Ship::Includes(KindsAllowed, G->Discovered);
	};

	// ------------------------------------------------------------------------------------------------- Campagne
	Add(TEXT("v4.16 sauvegardes : preparation"), 0.f, [this, OpenSave]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning() || !W->IsLevelReady())
		{
			return false;
		}
		FLevelReport R;
		R.Title = TEXT("v4.16 : REJOUER et sauvegardes (consommation, degats, document, mort, fermeture, reprise, migration)");
		R.Scene = TEXT("v4.16 sauvegardes");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		TestOrigSlot = BRSaves::ActiveSlot();
		TestOrigSave = PC->ActiveSave;
		bTestDevSession = PC->bDevSession;
		Sh.bInMenuAtStart = PC->bInMenu;
		BRSaves::SetTestPrefix(ShipPrefix);
		DeleteSlotFiles();
		const TArray<FString> Ids = KnownNotes();
		const FString First = Ids.Num() > 1 ? Ids.Last() : FString();
		const bool bWritten = BRSaves::Write(CampaignSlot, MakeCampaign(TEXT("Campagne v4.16"), true, First, 3, 100.f));
		BRSaves::Flush();
		const FString Why = OpenSave(CampaignSlot, false, 0);
		Note(FString::Printf(TEXT("campagne de test ecrite %s (emplacement %d, parcours lineaire, 3 eaux d'amande, 1 bandage, note \"%s\") ; VOS PARTIES -> CONTINUER %s"), BRTestV416Ship::Oui(bWritten),
			CampaignSlot + 1, *First, Why.IsEmpty() ? TEXT("oui") : *Why), !bWritten || !Why.IsEmpty());
		return true;
	});
	Add(TEXT("v4.16 campagne : Niveau 2"), 3.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == Sh.Serial || W->GetLevelNumber() != 2)
		{
			if (StepTime > 90.f)
			{
				Note(TEXT("campagne : Niveau 2 non charge par CONTINUER en 90 s"), true);
				return true;
			}
			return false;
		}
		C->bGodMode = true;
		Report().Level = 2;
		Sh.Seed2 = W->GetSeed();
		const bool bOk = W->IsStorySession() && W->IsLinearLevel() && C->CountItem(EBRItem::AlmondWater) == 3 && C->CountItem(EBRItem::Bandage) == 1;
		Note(FString::Printf(TEXT("campagne : Niveau 2 (graine %u), session d'histoire %s, equipement de la partie %s (eau %d, bandages %d)"), Sh.Seed2, BRTestV416Ship::Oui(W->IsStorySession()),
			BRTestV416Ship::Oui(C->CountItem(EBRItem::AlmondWater) == 3), C->CountItem(EBRItem::AlmondWater), C->CountItem(EBRItem::Bandage)), !bOk);
		return true;
	});
	LimitLastStep(100.f);
	Add(TEXT("v4.16 campagne : consommation, degats, fermeture"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C || W->GetLevelNumber() != 2 || W->IsTransitioning())
		{
			Skip(TEXT("campagne : Niveau 2 absent : consommation et degats non verifies"));
			return true;
		}
		// Degats decides par le serveur (regle du jeu), puis l'eau d'amande par l'objet rapide (meme chemin que la touche)
		C->bGodMode = false;
		C->ReceiveAttack(30.f, 0.f, nullptr);
		C->bGodMode = true;
		const float Hurt = C->Health;
		C->QuickUse(EBRItem::AlmondWater);
		const int32 WaterNow = C->CountItem(EBRItem::AlmondWater);
		// Fermeture : l'ecriture bloquante de QUITTER
		PC->WriteActiveSave(true);
		BRSaves::Flush();
		const UBRSaveGame* G = BRSaves::Load(CampaignSlot);
		Sh.Mission2 = W->GetMissionBlob();
		Sh.Durable = BRTestV416Ship::DurableOf(G);
		Sh.Notes = G ? G->Notes : TArray<FString>();
		Sh.Discovered = G ? G->Discovered : TArray<int32>();
		Sh.Health = G ? G->Health : -1.f;
		Sh.Water = BRTestV416Ship::FileCount(G, EBRItem::AlmondWater);
		Sh.Bandages = BRTestV416Ship::FileCount(G, EBRItem::Bandage);
		const bool bOk = Hurt <= 71.f && WaterNow == 2 && G && Sh.Water == 2 && Sh.Bandages == 1 && FMath::Abs(G->Health - C->Health) < 0.6f && G->Session.bValid && G->Session.Level == 2
			&& G->Session.Seed == Sh.Seed2 && G->Session.Mission == Sh.Mission2 && G->Deaths == 0;
		Note(FString::Printf(TEXT("campagne : degats 30 -> sante %.0f, eau d'amande bue (eau %d -> %d, sante %.0f) ; fichier apres la fermeture : eau %d, bandages %d, sante %.0f, session du "
			"Niveau %d graine %u, mission identique %s, morts %d"), Hurt, 3, WaterNow, C->Health, Sh.Water, Sh.Bandages, G ? G->Health : -1.f, G ? G->Session.Level : -1,
			G ? G->Session.Seed : 0u, BRTestV416Ship::Oui(G && G->Session.Mission == Sh.Mission2), G ? G->Deaths : -1), !bOk);
		return true;
	});

	// ------------------------------------------------------------------------------------------------- REJOUER
	Add(TEXT("v4.16 rejeu : REJOUER du Niveau 1 par le menu"), 0.f, [this, OpenSave]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		const FString Why = OpenSave(CampaignSlot, true, 1);
		Note(FString::Printf(TEXT("rejeu : VOS PARTIES -> la partie -> REJOUER le Niveau 1 %s"), Why.IsEmpty() ? TEXT("oui") : *Why), !Why.IsEmpty());
		return true;
	});
	Add(TEXT("v4.16 rejeu : Niveau 1 rejoue"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == Sh.Serial || W->GetLevelNumber() != 1)
		{
			if (StepTime > 90.f)
			{
				Note(TEXT("rejeu : Niveau 1 non charge par REJOUER en 90 s"), true);
				return true;
			}
			return false;
		}
		C->bGodMode = true;
		Report().Level = 1;
		const bool bReplay = PC->GetSessionMode() == EBRSessionMode::Replay && !PC->WritesCampaign() && W->IsReplaySession() && !W->IsLinearLevel();
		const bool bKit = C->CountItem(EBRItem::AlmondWater) == Sh.Water && C->CountItem(EBRItem::Bandage) == Sh.Bandages;
		Note(FString::Printf(TEXT("rejeu : session de rejeu %s (la partie n'est pas ecrite), niveau classique %s, equipement de la campagne repris %s (eau %d)"), BRTestV416Ship::Oui(bReplay),
			BRTestV416Ship::Oui(!W->IsLinearLevel()), BRTestV416Ship::Oui(bKit), C->CountItem(EBRItem::AlmondWater)), !bReplay || !bKit);
		return true;
	});
	LimitLastStep(100.f);
	Add(TEXT("v4.16 rejeu : consommation, degats, document, entite, ecritures"), 0.5f, [this, ReadNewNote]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C || W->GetLevelNumber() != 1 || W->IsTransitioning() || PC->GetSessionMode() != EBRSessionMode::Replay)
		{
			Skip(TEXT("rejeu : Niveau 1 rejoue absent : ecritures du rejeu non verifiees"));
			return true;
		}
		C->bGodMode = false;
		C->ReceiveAttack(25.f, 0.f, nullptr);
		C->bGodMode = true;
		const int32 WaterBefore = C->CountItem(EBRItem::AlmondWater);
		C->QuickUse(EBRItem::AlmondWater);
		const int32 WaterAfter = C->CountItem(EBRItem::AlmondWater);
		FString How;
		const bool bNote = ReadNewNote(Sh.Notes, Sh.NoteRead, How);
		// Une entite pas encore au journal de la partie, vue (la decouverte que fait le jeu quand une entite entre dans la vue)
		Sh.EntitySeen = -1;
		for (int32 K = 0; K < static_cast<int32>(EBREntityKind::Count) && Sh.EntitySeen < 0; ++K)
		{
			Sh.EntitySeen = (!Sh.Discovered.Contains(K) && !W->GetDiscoveredList().Contains(K)) ? K : -1;
		}
		if (Sh.EntitySeen >= 0)
		{
			W->Discover(static_cast<EBREntityKind>(Sh.EntitySeen));
		}
		// Les ecritures ordinaires d'une session : minuterie, pause ouverte puis fermee, ecriture bloquante
		PC->WriteActiveSave(false);
		PC->TogglePause();
		PC->TogglePause();
		PC->WriteActiveSave(true);
		BRSaves::Flush();
		const UBRSaveGame* G = BRSaves::Load(CampaignSlot);
		const FString Now = BRTestV416Ship::DurableOf(G);
		Sh.NotesAfter = G ? G->Notes : TArray<FString>();
		Sh.DiscoveredAfter = G ? G->Discovered : TArray<int32>();
		// Union : rien de retire, la note et l'entite du rejeu ajoutees, rien d'autre que ce que le joueur a lu ou vu
		TArray<FString> NotesAllowed = Sh.Notes;
		for (const FString& N : C->ReadNotes)
		{
			NotesAllowed.AddUnique(N);
		}
		TArray<int32> KindsAllowed = Sh.Discovered;
		for (const int32 K : W->GetDiscoveredList())
		{
			KindsAllowed.AddUnique(K);
		}
		const bool bSame = Now == Sh.Durable;
		const bool bNotes = BRTestV416Ship::Includes(Sh.NotesAfter, Sh.Notes) && BRTestV416Ship::Includes(NotesAllowed, Sh.NotesAfter) && (!bNote || Sh.NotesAfter.Contains(Sh.NoteRead));
		const bool bKinds = BRTestV416Ship::Includes(Sh.DiscoveredAfter, Sh.Discovered) && BRTestV416Ship::Includes(KindsAllowed, Sh.DiscoveredAfter)
			&& (Sh.EntitySeen < 0 || Sh.DiscoveredAfter.Contains(Sh.EntitySeen));
		Sh.bReplayOk = bSame && bNotes && bKinds;
		Note(FString::Printf(TEXT("rejeu : degats 25 (sante %.0f), eau bue %d -> %d, note lue \"%s\" (%s), entite %d vue ; apres les ecritures du rejeu (minuterie, pause, bloquante), "
			"fichier : champs durables identiques %s%s ; notes [%s] (attendu : celles de la partie et la note du rejeu) ; entites [%s] (attendu : celles de la partie et l'entite du rejeu)"),
			C->Health, WaterBefore, WaterAfter, *Sh.NoteRead, *How, Sh.EntitySeen, BRTestV416Ship::Oui(bSame), bSame ? TEXT("") : *(TEXT(" ; maintenant : ") + Now),
			*BRTestV416Ship::JoinNotes(Sh.NotesAfter), *BRTestV416Ship::JoinKinds(Sh.DiscoveredAfter)), !Sh.bReplayOk || WaterAfter != WaterBefore - 1);
		if (!bNote)
		{
			Skip(TEXT("rejeu : aucune note neuve a lire (ni dans les salles construites, ni parmi les notes du jeu) : union des documents non verifiee"));
		}
		if (Sh.EntitySeen < 0)
		{
			Skip(TEXT("rejeu : toutes les entites deja au journal : union des entites non verifiee"));
		}
		if (ABRPickup* P = Sh.Spawned.Get())
		{
			P->Destroy();
		}
		return true;
	});
	Add(TEXT("v4.16 rejeu : mort"), 0.3f, [this, JournalOk]()
	{
		ABRCharacter* C = GetPlayer();
		ABRWorld* W = GetBRWorld();
		if (!C || !W || W->GetLevelNumber() != 1)
		{
			Skip(TEXT("rejeu : Niveau 1 rejoue absent : mort non verifiee"));
			return true;
		}
		C->bGodMode = false;
		C->ReceiveAttack(500.f, 0.f, nullptr);
		BRSaves::Flush();
		const UBRSaveGame* G = BRSaves::Load(CampaignSlot);
		const bool bSame = BRTestV416Ship::DurableOf(G) == Sh.Durable && JournalOk(G);
		Note(FString::Printf(TEXT("rejeu : mort %s ; fichier inchange (ni mort comptee, ni mort en attente, ni session effacee, journal garde) %s"), BRTestV416Ship::Oui(C->IsDead()),
			BRTestV416Ship::Oui(bSame)), !C->IsDead() || !bSame);
		return true;
	});
	Add(TEXT("v4.16 rejeu : reveil dans le niveau rejoue"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if ((W->IsTransitioning() || C->IsDead() || !W->IsLevelReady()) && StepTime < 30.f)
		{
			return false;
		}
		C->bGodMode = true;
		// La note lue avant la mort reste au carnet de la vie suivante du rejeu (elle passera a la prochaine ecriture)
		const bool bOk = !C->IsDead() && W->GetLevelNumber() == 1 && (Sh.NoteRead.IsEmpty() || C->ReadNotes.Contains(Sh.NoteRead));
		Note(FString::Printf(TEXT("rejeu : reveil au Niveau %d (attendu 1, le niveau rejoue), note du rejeu toujours au carnet %s"), W->GetLevelNumber(),
			BRTestV416Ship::Oui(Sh.NoteRead.IsEmpty() || C->ReadNotes.Contains(Sh.NoteRead))), !bOk);
		return true;
	});
	LimitLastStep(40.f);
	Add(TEXT("v4.16 rejeu : fermeture du jeu"), 0.f, [this, JournalOk]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return false;
		}
		PC->WriteActiveSave(true); // QUITTER
		BRSaves::Flush();
		const UBRSaveGame* G = BRSaves::Load(CampaignSlot);
		const FString Now = BRTestV416Ship::DurableOf(G);
		const bool bJournal = JournalOk(G);
		const bool bOk = Now == Sh.Durable && bJournal;
		Note(FString::Printf(TEXT("rejeu : apres la fermeture, partie relue du disque : champs durables identiques a la campagne %s, journal = union %s%s"), BRTestV416Ship::Oui(Now == Sh.Durable),
			BRTestV416Ship::Oui(bJournal), Now == Sh.Durable ? TEXT("") : *(TEXT(" ; maintenant : ") + Now)), !bOk);
		return true;
	});

	// ------------------------------------------------------------------------------------------------- Reprise de la campagne
	Add(TEXT("v4.16 campagne : CONTINUER apres le rejeu"), 0.f, [this, OpenSave]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		const FString Why = OpenSave(CampaignSlot, false, 0);
		Note(FString::Printf(TEXT("campagne : VOS PARTIES -> la partie -> CONTINUER %s"), Why.IsEmpty() ? TEXT("oui") : *Why), !Why.IsEmpty());
		return true;
	});
	Add(TEXT("v4.16 campagne : session reprise"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == Sh.Serial || W->GetLevelNumber() != 2)
		{
			if (StepTime > 90.f)
			{
				Note(TEXT("campagne : CONTINUER apres le rejeu n'a pas charge le Niveau 2 en 90 s"), true);
				return true;
			}
			return false;
		}
		C->bGodMode = true;
		Report().Level = 2;
		const BRStory::FState St = W->GetStory();
		const bool bSession = W->GetSeed() == Sh.Seed2 && W->GetMissionBlob() == Sh.Mission2 && W->IsStorySession() && W->IsLinearLevel();
		// Sante de la partie (au moins 25 a la reprise), puis la remontee au calme depuis CONTINUER (1 par seconde au plus). La
		// partie en memoire n'est pas comparee : l'ecriture de l'arrivee (2 s apres le chargement) y recopie deja la sante remontee
		const float WantHealth = FMath::Max(25.f, Sh.Health);
		const float Regen = static_cast<float>(FPlatformTime::Seconds() - Sh.MenuClock) + 1.f;
		const bool bKit = C->CountItem(EBRItem::AlmondWater) == Sh.Water && C->CountItem(EBRItem::Bandage) == Sh.Bandages && C->Health >= WantHealth - 0.6f
			&& C->Health <= WantHealth + Regen;
		const bool bJournal = BRTestV416Ship::Includes(C->ReadNotes, Sh.NotesAfter) && BRTestV416Ship::Includes(W->GetDiscoveredList(), Sh.DiscoveredAfter);
		const bool bStory = St.Resume == 2 && BRStory::IsLinear(St) && St.RunSeed == ShipRunSeed && PC->ActiveSave && PC->ActiveSave->Deaths == 0;
		Note(FString::Printf(TEXT("campagne reprise apres le rejeu : graine %u (attendue %u), mission identique %s ; equipement et sante de la campagne %s (eau %d, bandages %d, sante %.0f, "
			"attendu %d, %d, %.0f) ; carnet : notes et entites du rejeu presentes %s ; histoire et morts inchangees %s"), W->GetSeed(), Sh.Seed2,
			BRTestV416Ship::Oui(W->GetMissionBlob() == Sh.Mission2), BRTestV416Ship::Oui(bKit), C->CountItem(EBRItem::AlmondWater), C->CountItem(EBRItem::Bandage), C->Health, Sh.Water,
			Sh.Bandages, WantHealth, BRTestV416Ship::Oui(bJournal), BRTestV416Ship::Oui(bStory)), !bSession || !bKit || !bJournal || !bStory);
		return true;
	});
	LimitLastStep(100.f);

	// ------------------------------------------------------------------------------------------------- Migration 5 -> 6
	Add(TEXT("v4.16 migration : partie du format 5 lue par le menu"), 0.f, [this, OpenSave]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return false;
		}
		UBRSaveGame* Old = BRTestV416Ship::MakeCampaign(TEXT("Partie v4.14 (format 5)"), false, Sh.Notes.Num() > 0 ? Sh.Notes[0] : FString(), 2, 90.f);
		Old->Version = 5;
		UGameplayStatics::SaveGameToMemory(Old, Sh.OldBytes);
		const FString MainPath = BRSaves::FilePath(BRSaves::SlotName(MigrationSlot));
		FFileHelper::SaveArrayToFile(Sh.OldBytes, *MainPath);
		// VOS PARTIES relit les parties (migration en memoire, copie d'origine) ; REJOUER du Niveau 1 depuis cette partie
		const FString Why = OpenSave(MigrationSlot, true, 1);
		const UBRSaveGame* Card = PC->GetSaveInSlot(MigrationSlot);
		TArray<uint8> Copy;
		TArray<uint8> Main;
		const bool bCopy = FFileHelper::LoadFileToArray(Copy, *BRSaves::FilePath(BRSaves::LegacySlotName(MigrationSlot, 5))) && Copy == Sh.OldBytes;
		const bool bUntouched = FFileHelper::LoadFileToArray(Main, *MainPath) && Main == Sh.OldBytes;
		Sh.MigratedDurable = BRTestV416Ship::DurableOf(Card);
		Sh.MigratedNotes = Card ? Card->Notes : TArray<FString>();
		const bool bOk = Card && Card->LoadedVersion == 5 && Card->GetStory().Flags == 0 && bCopy && bUntouched && Why.IsEmpty();
		Note(FString::Printf(TEXT("format 5 lu par VOS PARTIES : migre en memoire (format lu %d, parcours classique %s), copie d'origine _Format5 identique %s, fichier non reecrit par "
			"la lecture %s ; REJOUER du Niveau 1 %s"), Card ? Card->LoadedVersion : -1, BRTestV416Ship::Oui(Card && Card->GetStory().Flags == 0), BRTestV416Ship::Oui(bCopy),
			BRTestV416Ship::Oui(bUntouched), Why.IsEmpty() ? TEXT("oui") : *Why), !bOk);
		return true;
	});
	Add(TEXT("v4.16 migration : REJOUER, note lue, fermeture"), 1.f, [this, ReadNewNote]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelSerial() == Sh.Serial || W->GetLevelNumber() != 1)
		{
			if (StepTime > 90.f)
			{
				Note(TEXT("migration : Niveau 1 non charge par REJOUER en 90 s"), true);
				return true;
			}
			return false;
		}
		C->bGodMode = true;
		FString How;
		const bool bNote = ReadNewNote(Sh.MigratedNotes, Sh.NoteRead2, How);
		PC->WriteActiveSave(true); // QUITTER
		BRSaves::Flush();
		const UBRSaveGame* G = BRSaves::Load(MigrationSlot);
		TArray<uint8> Copy;
		const bool bCopy = FFileHelper::LoadFileToArray(Copy, *BRSaves::FilePath(BRSaves::LegacySlotName(MigrationSlot, 5))) && Copy == Sh.OldBytes;
		const FString Now = BRTestV416Ship::DurableOf(G);
		const bool bUnion = G && BRTestV416Ship::Includes(G->Notes, Sh.MigratedNotes) && (!bNote || G->Notes.Contains(Sh.NoteRead2));
		const bool bOk = PC->GetSessionMode() == EBRSessionMode::Replay && G && Now == Sh.MigratedDurable && bUnion && bCopy && (!bNote || G->LoadedVersion == UBRSaveGame::CurrentVersion);
		Note(FString::Printf(TEXT("migration : REJOUER, note lue \"%s\" (%s), fermeture : fichier relu au format %d, champs durables de la partie migree identiques %s%s, notes [%s], copie "
			"_Format5 toujours identique a l'original %s"), *Sh.NoteRead2, *How, G ? G->LoadedVersion : -1, BRTestV416Ship::Oui(Now == Sh.MigratedDurable),
			Now == Sh.MigratedDurable ? TEXT("") : *(TEXT(" ; maintenant : ") + Now), G ? *BRTestV416Ship::JoinNotes(G->Notes) : TEXT("-"), BRTestV416Ship::Oui(bCopy)), !bOk);
		if (!bNote)
		{
			Skip(TEXT("migration : aucune note neuve a lire : ecriture de l'union apres migration non verifiee"));
		}
		if (ABRPickup* P = Sh.Spawned.Get())
		{
			P->Destroy();
		}
		return true;
	});
	LimitLastStep(100.f);

	Add(TEXT("v4.16 sauvegardes : retour a l'etat d'avant le test"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		SwapTestSave(PC, TestOrigSave, TestOrigSlot);
		if (PC)
		{
			PC->bDevSession = bTestDevSession;
			PC->bInMenu = Sh.bInMenuAtStart;
		}
		TestOrigSave = nullptr;
		BRSaves::ReplaySession() = false;
		if (W)
		{
			W->SetStory(BRStory::FState(), EBRSessionMode::Free);
		}
		if (ABRCharacter* C = GetPlayer())
		{
			C->bGodMode = true;
		}
		BRTestV416Ship::DeleteSlotFiles();
		BRSaves::SetTestPrefix(FString());
		if (PC)
		{
			PC->SetMenuPage(EBRMenuPage::Main);
			PC->UpdateInputMode();
		}
		Note(TEXT("v4.16 sauvegardes : partie active, mode et parties de test retablis"));
		return true;
	});
}
