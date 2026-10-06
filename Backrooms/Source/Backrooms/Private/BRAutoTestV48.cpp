// v4.8 : etapes du test automatique propres a la v4.8 (-BRAutoTest, ou seules avec -BRAutoTest -BRAutoTestV48).
// Chaque etape verifie un defaut corrige ou une fonction ajoutee en v4.8 :
//   - sauvegardes : partie d'un format plus recent lue sans etre reecrite (fichier identique octet pour octet), echec
//     d'ecriture garde jusqu'a son acquittement, meme apres une ecriture reussie ;
//   - journal : notes d'une ancienne sauvegarde (texte francais) converties en identifiants, texte dans la langue choisie ;
//   - combinaison du joueur : materiau de chaque section (ni materiau par defaut, ni materiau d'erreur, usage "squelette"),
//     problemes de materiaux signales pendant la session ;
//   - profils graphiques appliques a ce qui est deja charge (type des lumieres des chunks construits, Hound complet ou
//     allege) ;
//   - profil RTX fluide : mesures pendant une course au Niveau 0 et dans les Poolrooms (objectif : 60 images/s) ;
//   - langues : les 22 langues (aucune cle sans traduction, texte different du francais, nombres et dates au format de la
//     langue, coupure des lignes, sens d'ecriture), preference enregistree et reprise au lancement, captures de la page
//     Langue (arabe, japonais, chinois, russe), du menu (arabe, allemand) et des parametres (espagnol).
// Les fichiers de sauvegarde du test ont leur propre prefixe (BR_AutoTest_) ; la langue et les reglages du joueur sont
// retablis a la fin.
#include "BRAutoTest.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BRChunk.h"
#include "BRConfig.h"
#include "BREntity.h"
#include "BRFonts.h"
#include "BRHUD.h"
#include "BRLevels.h"
#include "BRLoc.h"
#include "BRPlayerController.h"
#include "BRSave.h"
#include "BRWorld.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Internationalization/Text.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	const TCHAR* const V48Prefix = TEXT("BR_AutoTest_");
	/** Emplacements du test : ancienne partie avec des notes, partie d'un format plus recent, echec d'ecriture */
	constexpr int32 V48NotesSlot = 2;
	constexpr int32 V48FutureSlot = 3;
	constexpr int32 V48FailSlot = 4;
	/** Section des reglages du joueur ou la langue est enregistree (BRLoc.cpp) */
	const TCHAR* const V48LanguageSection = TEXT("/Script/Backrooms.BRSettings");

	struct FV48Profile
	{
		int32 Index;
		const TCHAR* Name;
	};
	/** Ordre choisi pour que chaque profil change quelque chose (lumieres surfaciques ou ponctuelles, Hound complet ou allege) */
	const FV48Profile V48Profiles[] = { { 2, TEXT("Cinematique") }, { 0, TEXT("Performance") }, { 4, TEXT("RTX_fluide") }, { 1, TEXT("Qualite") } };

	void DeleteV48Files()
	{
		for (const int32 Slot : { V48NotesSlot, V48FutureSlot, V48FailSlot })
		{
			// Retire aussi la protection "format plus recent" que le test a posee sur l'emplacement
			BRSaves::Delete(Slot);
			for (const FString& Name : { BRSaves::UnreadableSlotName(Slot), BRSaves::LegacySlotName(Slot, 1), BRSaves::LegacySlotName(Slot, 2) })
			{
				IFileManager::Get().Delete(*BRSaves::FilePath(Name), false, true, true);
			}
			const FString Tmp = BRSaves::FilePath(BRSaves::SlotName(Slot)) + TEXT(".tmp");
			IFileManager::Get().DeleteDirectory(*Tmp, false, true);
			IFileManager::Get().Delete(*Tmp, false, true, true);
		}
	}

	/** Sans espaces ni retours a la ligne (le texte coupe doit garder tous ses caracteres) */
	FString WithoutSpaces(const FString& S)
	{
		FString Out;
		Out.Reserve(S.Len());
		for (const TCHAR Ch : S)
		{
			if (!FChar::IsWhitespace(Ch))
			{
				Out.AppendChar(Ch);
			}
		}
		return Out;
	}

	/** Ponctuation qui ne commence jamais une ligne (regles Unicode de coupure, chinois et japonais compris) */
	bool IsClosingPunctuation(TCHAR Ch)
	{
		static const TCHAR Closing[] = TEXT("\u3001\u3002\uFF0C\uFF0E\uFF01\uFF1F\uFF09\uFF1A\uFF1B\u300D\u300F\u3011\u3009\u300B)]!?.,:;");
		for (const TCHAR* P = Closing; *P; ++P)
		{
			if (*P == Ch)
			{
				return true;
			}
		}
		return false;
	}

	int32 LanguageIndex(const TCHAR* Code)
	{
		return BRLoc::Languages().IndexOfByPredicate([Code](const BRLoc::FLanguage& L) { return FCString::Strcmp(L.Code, Code) == 0; });
	}
}

void ABRAutoTest::AddV48Steps()
{
	// ------------------------------------------------------------------------------------------------ Sauvegardes
	Add(TEXT("v4.8 sauvegarde : preparation"), 0.f, [this]()
	{
		FLevelReport R;
		R.Title = TEXT("v4.8 : sauvegardes et journal (fichiers BR_AutoTest_*)");
		R.Scene = TEXT("v4.8");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		BRSaves::SetTestPrefix(V48Prefix);
		DeleteV48Files();
		BRSaves::TakeLoadMessages();
		return true;
	});
	Add(TEXT("v4.8 sauvegarde : partie d'un format plus recent"), 0.f, [this]()
	{
		// Une partie ecrite par une version future du jeu : lisible ici, jamais reecrite (avant : reecrite au format courant
		// a la sauvegarde suivante, ce qui perdait ce que la version future y avait mis)
		UBRSaveGame* Future = NewObject<UBRSaveGame>();
		Future->Version = UBRSaveGame::CurrentVersion + 6;
		Future->SaveName = TEXT("Partie d'une version future");
		Future->CurrentLevel = 37;
		Future->Explored = { 0, 1, 37 };
		TArray<uint8> Bytes;
		UGameplayStatics::SaveGameToMemory(Future, Bytes);
		const FString Path = BRSaves::FilePath(BRSaves::SlotName(V48FutureSlot));
		FFileHelper::SaveArrayToFile(Bytes, *Path);
		TestBytes = Bytes;
		UBRSaveGame* L = BRSaves::Load(V48FutureSlot);
		const int32 Format = BRSaves::FutureFormatOf(V48FutureSlot);
		// Le jeu essaie de l'ecrire (fin de partie, sauvegarde automatique) : refuse, l'echec est garde
		const bool bWriteRefused = L && !BRSaves::Write(V48FutureSlot, L);
		const bool bAsyncRefused = !BRSaves::WriteAsync(V48FutureSlot, NewObject<UBRSaveGame>());
		BRSaves::Flush();
		TArray<uint8> After;
		FFileHelper::LoadFileToArray(After, *Path);
		const bool bSame = After == TestBytes;
		const bool bNoBackup = !IFileManager::Get().FileExists(*BRSaves::FilePath(BRSaves::BackupSlotName(V48FutureSlot)));
		int32 Kept = 0;
		uint32 LastId = 0;
		for (const FWriteFailure& F : BRSaves::PendingFailures())
		{
			LastId = FMath::Max(LastId, F.RequestId);
			Kept += (F.Slot == V48FutureSlot && F.Error == EBRSaveError::FutureFormat && F.Version == Future->Version) ? 1 : 0;
		}
		const bool bOk = L && L->bFutureFormat && L->LoadedVersion == Future->Version && Format == Future->Version && bWriteRefused && bAsyncRefused
			&& bSame && bNoBackup && Kept == 2;
		Note(FString::Printf(TEXT("format plus recent (%d) : %s (lu en lecture seule %s, format signale %d, ecriture refusee %s, ecriture de fond refusee %s, fichier intact %s, aucune copie ecrite %s, echecs gardes %d / 2)"),
			Future->Version, bOk ? TEXT("OK") : TEXT("ECHEC"), L && L->bFutureFormat ? TEXT("oui") : TEXT("NON"), Format, bWriteRefused ? TEXT("oui") : TEXT("NON"),
			bAsyncRefused ? TEXT("oui") : TEXT("NON"), bSame ? TEXT("oui") : TEXT("NON"), bNoBackup ? TEXT("oui") : TEXT("NON"), Kept), !bOk);
		BRSaves::AcknowledgeFailures(LastId);
		return true;
	});
	Add(TEXT("v4.8 sauvegarde : echec d'ecriture garde"), 0.6f, [this]()
	{
		// Un dossier a la place du fichier temporaire : l'ecriture echoue comme sur un disque plein ou un dossier protege
		const FString Tmp = BRSaves::FilePath(BRSaves::SlotName(V48FailSlot)) + TEXT(".tmp");
		IFileManager::Get().Delete(*Tmp, false, true, true);
		IFileManager::Get().MakeDirectory(*Tmp, true);
		UBRSaveGame* S = NewObject<UBRSaveGame>();
		S->SaveName = TEXT("Echec d'ecriture");
		const bool bFailed = !BRSaves::Write(V48FailSlot, S);
		uint32 FailedId = 0;
		for (const FWriteFailure& F : BRSaves::PendingFailures())
		{
			if (F.Slot == V48FailSlot && F.Error == EBRSaveError::TempWrite)
			{
				FailedId = F.RequestId;
			}
		}
		IFileManager::Get().DeleteDirectory(*Tmp, false, true);
		// L'ecriture suivante reussit : l'echec precedent ne doit pas disparaitre sans avoir ete montre (v4.7 : efface)
		const bool bLater = BRSaves::Write(V48FailSlot, S);
		bool bStillKept = false;
		uint32 LastId = 0;
		for (const FWriteFailure& F : BRSaves::PendingFailures())
		{
			bStillKept |= F.RequestId == FailedId;
			LastId = FMath::Max(LastId, F.RequestId);
		}
		const bool bFlagged = !BRSaves::LastWriteSucceeded();
		// Le controleur montre l'echec au joueur (message dans sa langue) puis l'acquitte
		if (ABRPlayerController* PC = GetPC())
		{
			PC->ShowSaveFailures();
		}
		else
		{
			BRSaves::AcknowledgeFailures(LastId);
		}
		const bool bCleared = BRSaves::PendingFailures().Num() == 0 && BRSaves::LastWriteSucceeded();
		const bool bOk = bFailed && FailedId != 0 && bLater && bStillKept && bFlagged && bCleared;
		Note(FString::Printf(TEXT("echec d'ecriture : %s (echec constate %s, demande %u, ecriture suivante reussie %s, echec garde apres la reussite %s, montre puis acquitte %s)"),
			bOk ? TEXT("OK") : TEXT("ECHEC"), bFailed ? TEXT("oui") : TEXT("NON"), FailedId, bLater ? TEXT("oui") : TEXT("NON"), bStillKept ? TEXT("oui") : TEXT("NON"),
			bCleared ? TEXT("oui") : TEXT("NON")), !bOk);
		return true;
	});
	Add(TEXT("v4.8 sauvegarde : capture du message"), 0.f, [this]()
	{
		Shot(TEXT("V48_echec_sauvegarde"));
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Journal
	Add(TEXT("v4.8 journal : notes d'une sauvegarde v4.7"), 0.f, [this]()
	{
		const FBRNote& N = BRLevels::CommonNotes()[0];
		const FString* Source = FTextInspector::GetSourceString(N.Text);
		const FString Unknown = TEXT("Note d'une version retiree");
		// Format 2 (v4.7) : le journal gardait le texte francais des notes
		UBRSaveGame* Old = NewObject<UBRSaveGame>();
		Old->Version = 2;
		Old->SaveName = TEXT("Partie v4.7");
		Old->bHasPlayer = true;
		Old->Notes = { Source ? *Source : FString(), Unknown };
		TArray<uint8> Bytes;
		UGameplayStatics::SaveGameToMemory(Old, Bytes);
		FFileHelper::SaveArrayToFile(Bytes, *BRSaves::FilePath(BRSaves::SlotName(V48NotesSlot)));
		const UBRSaveGame* L = BRSaves::Load(V48NotesSlot);
		const bool bConverted = L && L->LoadedVersion == 2 && L->Notes.Num() == 2 && L->Notes[0] == N.Id.ToString() && L->Notes[1] == Unknown;
		// Le texte suit la langue choisie (l'identifiant ne depend pas de la langue)
		const FString Before = BRLoc::Current().Code;
		BRLoc::SetLanguage(TEXT("en"));
		const FString English = L && L->Notes.Num() > 0 ? BRLevels::NoteText(L->Notes[0]).ToString() : FString();
		const FString Kept = L && L->Notes.Num() > 1 ? BRLevels::NoteText(L->Notes[1]).ToString() : FString();
		BRLoc::SetLanguage(Before);
		const bool bTranslated = Source && !English.IsEmpty() && !English.Equals(*Source) && Kept == Unknown;
		Note(FString::Printf(TEXT("journal v4.7 -> v4.8 : %s (note convertie en %s, texte inconnu garde tel quel %s, texte en anglais : \"%s\")"),
			bConverted && bTranslated ? TEXT("OK") : TEXT("ECHEC"), L && L->Notes.Num() > 0 ? *L->Notes[0] : TEXT("(rien)"), Kept == Unknown ? TEXT("oui") : TEXT("NON"),
			*English.Left(60)), !(bConverted && bTranslated));
		DeleteV48Files();
		BRSaves::SetTestPrefix(FString());
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Combinaison
	AddSeedLoad(0, 4605, TEXT("v4.8 : combinaison, profils et RTX fluide (Niveau 0, graine 4605)"));
	Add(TEXT("v4.8 combinaison : materiaux"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		UBRAssets* A = UBRAssets::Get(this);
		if (!C || !A)
		{
			return false;
		}
		Report().Scene = TEXT("v4.8");
		UPoseableMeshComponent* Body = C->BodySkin;
		if (!Body || !Body->GetSkinnedAsset())
		{
			const bool bImported = A->SkeletalMesh(TEXT("SK_Hazmat")) != nullptr;
			Note(bImported ? TEXT("combinaison : SK_Hazmat importe mais non utilise (pieces rigides a la place)")
				: TEXT("combinaison : SK_Hazmat absent (import Python non fait) : pieces rigides de secours"), true);
			return true;
		}
		// Une ligne par section : LOD, slot, materiau final, parent, texture, usage, secours
		for (const FString& Line : A->DescribeSections(Body, TEXT("SK_Hazmat")))
		{
			Note(TEXT("  ") + Line);
		}
		int32 Bad = 0;
		const UMaterialInterface* Default = UMaterial::GetDefaultMaterial(MD_Surface);
		const UMaterialInterface* ErrorMat = A->ErrorMaterial();
		for (int32 i = 0; i < Body->GetNumMaterials(); ++i)
		{
			const UMaterialInterface* M = Body->GetMaterial(i);
			const UMaterial* Base = M ? M->GetMaterial() : nullptr;
			const bool bSkeletal = Base && Base->GetUsageByFlag(MATUSAGE_SkeletalMesh);
			if (!M || M == Default || M == ErrorMat || Base == Default || !bSkeletal)
			{
				++Bad;
				Note(FString::Printf(TEXT("combinaison : section %d : %s%s"), i, M ? *M->GetName() : TEXT("aucun materiau"),
					!M ? TEXT("") : (M == ErrorMat ? TEXT(" (materiau d'erreur)") : (M == Default || Base == Default ? TEXT(" (materiau par defaut)") : TEXT(" (sans usage squelette)")))),
					true);
			}
		}
		const TArray<FString>& MatProblems = UBRAssets::GetMaterialProblems();
		for (int32 i = 0; i < MatProblems.Num() && i < 12; ++i)
		{
			Note(TEXT("materiau : ") + MatProblems[i], true);
		}
		Note(FString::Printf(TEXT("combinaison : %d section(s), %d materiau(x) incorrect(s), %d probleme(s) de materiaux signales depuis le lancement"),
			Body->GetNumMaterials(), Bad, MatProblems.Num()));
		return true;
	});
	Add(TEXT("v4.8 combinaison : 3e personne"), 1.5f, [this]()
	{
		if (ABRCharacter* C = GetPlayer())
		{
			if (!C->IsThirdPerson())
			{
				C->ToggleThirdPerson();
			}
		}
		return true;
	});
	Add(TEXT("v4.8 combinaison : capture"), 0.3f, [this]()
	{
		Shot(TEXT("V48_combinaison_L00"));
		if (ABRCharacter* C = GetPlayer())
		{
			if (C->IsThirdPerson())
			{
				C->ToggleThirdPerson();
			}
		}
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Profils graphiques
	Add(TEXT("v4.8 profils : Hound present"), 0.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || !GetPlayer())
		{
			return false;
		}
		if (!bSettingsSaved)
		{
			SavedSettings = FBRSettings::Get();
			bSettingsSaved = true;
		}
		FVector Spot;
		const FBREntityInfo& Info = ABREntity::Info(EBREntityKind::Hound);
		if (FindSpotInFront(500.f, Info.HalfHeight + 5.f, Spot))
		{
			TestEntity = W->SpawnEntity(EBREntityKind::Hound, Spot);
		}
		if (!TestEntity.IsValid())
		{
			Note(TEXT("profils : pas de place pour le Hound devant le joueur"), true);
		}
		return true;
	});
	for (const FV48Profile& P : V48Profiles)
	{
		const int32 Index = P.Index;
		const FString Name = P.Name;
		Add(FString::Printf(TEXT("v4.8 profils : %s"), *Name), 3.f, [this, Index]()
		{
			ABRPlayerController::ApplyGraphicsProfile(Index);
			if (ABRPlayerController* PC = GetPC())
			{
				PC->ApplySettings();
			}
			return true;
		});
		Add(FString::Printf(TEXT("v4.8 profils : verification %s"), *Name), 0.2f, [this, Name]()
		{
			const FBRSettings& S = FBRSettings::Get();
			ABRWorld* W = GetBRWorld();
			// Chunks deja construits : leurs lumieres suivent le reglage (avant : seules les zones chargees ensuite)
			int32 Ready = 0;
			int32 Mismatch = 0;
			for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
			{
				if (!It->IsReady() || It->IsTearingDown())
				{
					continue;
				}
				++Ready;
				Mismatch += It->LightTypesMatch(S.bAreaLights) ? 0 : 1;
			}
			// Hound deja present : modele complet (Cinematique) ou allege
			FString Hound = TEXT("absent");
			bool bHoundOk = true;
			if (ABREntity* E = TestEntity.Get())
			{
				UBRAssets* A = UBRAssets::Get(this);
				const USkeletalMesh* Lite = A ? A->SkeletalMesh(TEXT("SK_HoundLite")) : nullptr;
				const USkeletalMesh* Full = A ? A->SkeletalMesh(TEXT("SK_Hound")) : nullptr;
				const USkeletalMesh* Want = (!S.bFullCreatures && Lite) ? Lite : Full;
				Hound = TEXT("pieces rigides");
				TInlineComponentArray<UPoseableMeshComponent*> Skins(E);
				for (const UPoseableMeshComponent* Skin : Skins)
				{
					if (const USkinnedAsset* Asset = Skin ? Skin->GetSkinnedAsset() : nullptr)
					{
						Hound = Asset->GetName();
						bHoundOk = !Want || Asset == Want;
						break;
					}
				}
			}
			const bool bOk = Mismatch == 0 && bHoundOk;
			Note(FString::Printf(TEXT("profil %s : %d chunk(s) construits, %d avec des lumieres d'un autre type (%s attendues) ; Hound : %s%s ; ombres locales : %d (jusqu'a %.0f m)"),
				*Name, Ready, Mismatch, S.bAreaLights ? TEXT("surfaciques") : TEXT("ponctuelles"), *Hound,
				bHoundOk ? TEXT("") : (S.bFullCreatures ? TEXT(" (ATTENDU : SK_Hound complet)") : TEXT(" (ATTENDU : SK_HoundLite)")), W ? W->GetShadowedLightCount() : 0,
				ABRWorld::LocalShadowDistance() / 100.f), !bOk);
			if (ABRPlayerController* PC = GetPC())
			{
				Note(TEXT("  ") + PC->GetRenderModeText(true));
			}
			Shot(TEXT("V48_profil_") + Name);
			return true;
		});
	}
	Add(TEXT("v4.8 profils : fin"), 0.f, [this]()
	{
		if (ABREntity* E = TestEntity.Get())
		{
			E->Destroy();
		}
		TestEntity.Reset();
		return true;
	});

	// ------------------------------------------------------------------------------------------------ RTX fluide
	Add(TEXT("v4.8 RTX fluide : application"), 3.f, [this]()
	{
		ABRPlayerController::ApplyGraphicsProfile(4);
		if (ABRPlayerController* PC = GetPC())
		{
			PC->ApplySettings();
		}
		FLevelReport R;
		R.Level = 0;
		R.Title = TEXT("RTX fluide : course au Niveau 0");
		R.Scene = TEXT("v4.8_rtx_course");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		return true;
	});
	Add(TEXT("v4.8 RTX fluide : course de 20 s"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if (!bSprintStarted)
		{
			bSprintStarted = true;
			SprintT0 = StepTime;
			W->ResetChunkStats();
			SprintHoles = 0;
			SprintFrames = 0;
			TestStart = C->GetActorLocation();
			StartMeasure();
		}
		const float T = StepTime - SprintT0;
		// Vitesse de sprint en ligne droite : la generation doit suivre sans a-coups. v4.10 : le Niveau 0 est fini depuis la
		// v4.3 (BoundsChunks) : la course fait des allers-retours entre les murs d'enceinte. Avant, elle sortait du niveau au
		// bout d'une dizaine de secondes et comptait comme "sans sol" toutes les images passees dehors (environ la moitie).
		FVector P = TestStart + FVector(0.f, 470.f * T, 0.f);
		if (W->Def().BoundsChunks > 0)
		{
			const float Half = W->Def().BoundsChunks * W->Def().ChunkCells * W->CellSize() - 2.f * W->CellSize();
			const float Span = 2.f * Half;
			const float U = FMath::Fmod(static_cast<float>(TestStart.Y) + Half + 470.f * T, 2.f * Span);
			P.Y = -Half + (U <= Span ? U : 2.f * Span - U);
		}
		P.Z = C->GetSimpleCollisionHalfHeight() + 5.f;
		C->SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
		++SprintFrames;
		SprintHoles += W->HasChunkFloor(W->CellToChunk(W->WorldToCell(P))) ? 0 : 1;
		if (T < 20.f)
		{
			return false;
		}
		bSprintStarted = false;
		FLevelReport& R = Report();
		EndMeasure(R);
		const bool bRT = ABRPlayerController::IsHardwareRayTracingAvailable();
		const FIntPoint Internal = ABRPlayerController::InternalResolution();
		Note(FString::Printf(TEXT("RTX fluide (course) : resolution interne %dx%d, 95 %% des images en %.1f ms (objectif 16,7 ms), 1 %% le plus lent %.0f img/s, %d image(s) sans sol sur %d"),
			Internal.X, Internal.Y, R.P95Ms, R.Low1FPS, SprintHoles, SprintFrames), SprintHoles > 0);
		if (!bRT)
		{
			Note(TEXT("ray tracing materiel indisponible sur cette machine (DirectX 12 et carte compatible requis) : mesure sans RT, objectif non evalue"));
		}
		else if (R.P95Ms > 16.7f)
		{
			Note(TEXT("objectif non atteint : 60 images/s (95 % des images sous 16,7 ms) en RTX fluide pendant la course"), true);
		}
		return true;
	});
	AddSeedLoad(37, 4605, TEXT("v4.8 RTX fluide : Poolrooms (Niveau 37, graine 4605)"));
	Add(TEXT("v4.8 RTX fluide : Poolrooms"), 4.f, [this]()
	{
		ABRPlayerController::ApplyGraphicsProfile(4);
		if (ABRPlayerController* PC = GetPC())
		{
			PC->ApplySettings();
		}
		Report().Scene = TEXT("v4.8_rtx_poolrooms");
		return true;
	});
	Add(TEXT("v4.8 RTX fluide : mesure Poolrooms"), 5.f, [this]()
	{
		StartMeasure();
		return true;
	});
	Add(TEXT("v4.8 RTX fluide : capture Poolrooms"), 0.f, [this]()
	{
		FLevelReport& R = Report();
		EndMeasure(R);
		Shot(TEXT("V48_RTX_fluide_L37"));
		if (ABRPlayerController::IsHardwareRayTracingAvailable() && R.P95Ms > 16.7f)
		{
			Note(TEXT("objectif non atteint : 60 images/s (95 % des images sous 16,7 ms) en RTX fluide dans les Poolrooms"), true);
		}
		// La combinaison du joueur dans les Poolrooms (la capture d'origine du defaut : combinaison sombre, sans texture)
		if (ABRCharacter* C = GetPlayer())
		{
			if (!C->IsThirdPerson())
			{
				C->ToggleThirdPerson();
			}
		}
		return true;
	});
	Add(TEXT("v4.8 combinaison : attente Poolrooms"), 1.5f, []()
	{
		return true;
	});
	Add(TEXT("v4.8 combinaison : capture Poolrooms"), 0.3f, [this]()
	{
		Shot(TEXT("V48_combinaison_L37"));
		if (ABRCharacter* C = GetPlayer())
		{
			if (C->IsThirdPerson())
			{
				C->ToggleThirdPerson();
			}
		}
		// Reglages du joueur retablis
		if (bSettingsSaved)
		{
			FBRSettings::Get() = SavedSettings;
			bSettingsSaved = false;
			if (ABRPlayerController* PC = GetPC())
			{
				PC->ApplySettings();
			}
		}
		return true;
	});

	// ------------------------------------------------------------------------------------------------ Langues
	Add(TEXT("v4.8 langues : preparation"), 0.f, [this]()
	{
		FLevelReport R;
		R.Title = FString::Printf(TEXT("v4.8 : langues (%d)"), BRLoc::Languages().Num());
		R.Scene = TEXT("v4.8");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		TestLanguage = BRLoc::Current().Code;
		TestLanguagePref = BRLoc::Preference();
		bLanguageSaved = true;
		const TArray<FString> MissingFonts = BRFonts::MissingFiles();
		Note(FString::Printf(TEXT("langue au lancement : %s (preference \"%s\", systeme : %s) ; %d textes ; polices manquantes : %d"), *TestLanguage, *TestLanguagePref,
			*BRLoc::DetectSystemLanguage(), BRLoc::KeyCount(), MissingFonts.Num()), MissingFonts.Num() > 0);
		for (const FString& F : MissingFonts)
		{
			Note(TEXT("police absente du paquet : Content/Fonts/") + F, true);
		}
		return true;
	});
	for (int32 i = 0; i < BRLoc::Languages().Num(); ++i)
	{
		Add(FString::Printf(TEXT("v4.8 langue %s"), BRLoc::Languages()[i].Code), 0.1f, [this, i]()
		{
			const BRLoc::FLanguage& L = BRLoc::Languages()[i];
			const FString Code = L.Code;
			if (!BRLoc::SetLanguage(Code))
			{
				Note(FString::Printf(TEXT("%s : langue refusee (donnees ICU absentes du paquet ?)"), *Code), true);
				return true;
			}
			TArray<FString> Missing;
			const int32 NMissing = BRLoc::CountMissing(&Missing);
			// Une note (long paragraphe) : traduite, puis coupee comme le HUD la dessine
			const FBRNote& N = BRLevels::CommonNotes()[0];
			const FString Text = N.Text.ToString();
			const FString* Source = FTextInspector::GetSourceString(N.Text);
			const bool bDiffers = Code == TEXT("fr") || !Source || !Text.Equals(*Source);
			const FString Number = BRLoc::Fmt(INVTEXT("{N}"), { { TEXT("N"), BRLoc::Num(1234.5, 1) } });
			const FString Date = BRSaves::FormatDate(FDateTime(2026, 10, 5, 14, 30, 0));
			const bool bRtlOk = L.bRightToLeft == BRLoc::IsRightToLeft();
			FString WrapInfo = TEXT("HUD absent, coupure non verifiee");
			bool bWrapOk = true;
			const ABRPlayerController* PC = GetPC();
			if (const ABRHUD* H = PC ? Cast<ABRHUD>(PC->GetHUD()) : nullptr)
			{
				const float MaxW = 360.f;
				const TArray<FString> Lines = H->WrapF(Text, MaxW, 16.f, ABRHUD::EUiWeight::Regular);
				float Widest = 0.f;
				int32 BadStarts = 0;
				FString Joined;
				for (const FString& Line : Lines)
				{
					Widest = FMath::Max(Widest, H->TextSize(Line, 16.f, ABRHUD::EUiWeight::Regular).X);
					const FString Trimmed = Line.TrimStart();
					BadStarts += (Trimmed.Len() > 0 && IsClosingPunctuation(Trimmed[0])) ? 1 : 0;
					Joined += Line;
				}
				const bool bLost = WithoutSpaces(Joined) != WithoutSpaces(Text);
				bWrapOk = Lines.Num() > 1 && Widest <= MaxW + 1.f && BadStarts == 0 && !bLost;
				WrapInfo = FString::Printf(TEXT("%d lignes, la plus large %.0f px sur %.0f%s%s"), Lines.Num(), Widest, MaxW,
					BadStarts > 0 ? TEXT(", PONCTUATION EN DEBUT DE LIGNE") : TEXT(""), bLost ? TEXT(", CARACTERES PERDUS") : TEXT(""));
				if (L.bRightToLeft)
				{
					// Arabe et persan : largeur du texte mis en forme (lettres liees)
					const float Width = H->TextSize(BRLevels::Get(0).Title.ToString(), 16.f, ABRHUD::EUiWeight::Regular).X;
					WrapInfo += FString::Printf(TEXT(", titre mis en forme %.0f px"), Width);
					bWrapOk &= Width > 0.f;
				}
			}
			const bool bOk = NMissing == 0 && bDiffers && bWrapOk && bRtlOk && !Number.IsEmpty() && !Date.IsEmpty();
			Note(FString::Printf(TEXT("%s (%s) : %d/%d textes traduits, %s, %s ; nombre %s, date %s ; sens %s ; coupure : %s"), *Code, L.NativeName,
				BRLoc::KeyCount() - NMissing, BRLoc::KeyCount(), BRLoc::IsReviewed(Code) ? TEXT("relue") : TEXT("traduction automatique a relire"),
				bDiffers ? TEXT("texte traduit") : TEXT("TEXTE FRANCAIS AFFICHE"), *Number, *Date,
				BRLoc::IsRightToLeft() ? TEXT("droite a gauche") : TEXT("gauche a droite"), *WrapInfo), !bOk);
			for (int32 k = 0; k < Missing.Num() && k < 5; ++k)
			{
				Note(FString::Printf(TEXT("  %s : sans traduction : %s"), *Code, *Missing[k]), true);
			}
			return true;
		});
	}
	Add(TEXT("v4.8 langues : preference enregistree"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		const int32 De = LanguageIndex(TEXT("de"));
		if (!PC || De == INDEX_NONE)
		{
			return true;
		}
		// Choix dans la page Langue : applique et enregistre (fichier de reglages de cette machine)
		PC->ChooseLanguage(De);
		FString Stored;
		BRConfig::Get().GetString(V48LanguageSection, TEXT("Language"), Stored);
		// Relance simulee : la langue enregistree est reprise, quelle que soit la langue en cours
		BRLoc::SetLanguage(TEXT("en"));
		BRLoc::ApplyStartupLanguage();
		const FString After = BRLoc::Current().Code;
		const bool bOk = Stored == TEXT("de") && BRLoc::Preference() == TEXT("de") && After == TEXT("de");
		Note(FString::Printf(TEXT("preference : \"%s\" enregistree dans %s, reprise au lancement : %s"), *Stored, *FPaths::GetCleanFilename(BRConfig::Path()), *After), !bOk);
		return true;
	});
	// Captures : page Langue dans quatre ecritures, menu principal en arabe et en allemand, parametres en espagnol
	Add(TEXT("v4.8 langues : page Langue"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return false;
		}
		bWasInMenu = PC->bInMenu;
		PC->SetInventoryOpen(false);
		PC->bInMenu = true;
		PC->SetMenuPage(EBRMenuPage::Language);
		PC->UpdateInputMode();
		return true;
	});
	for (const TCHAR* Code : { TEXT("ar"), TEXT("ja"), TEXT("zh-Hans"), TEXT("ru") })
	{
		const FString C = Code;
		Add(TEXT("v4.8 langues : page Langue en ") + C, 1.2f, [this, C]()
		{
			BRLoc::SetLanguage(C);
			if (ABRPlayerController* PC = GetPC())
			{
				PC->SetMenuPage(EBRMenuPage::Language); // curseur sur la langue courante
			}
			return true;
		});
		Add(TEXT("v4.8 langues : capture ") + C, 0.f, [this, C]()
		{
			Shot(TEXT("V48_langue_") + C);
			return true;
		});
	}
	for (const TCHAR* Code : { TEXT("ar"), TEXT("de") })
	{
		const FString C = Code;
		Add(TEXT("v4.8 langues : menu en ") + C, 1.2f, [this, C]()
		{
			BRLoc::SetLanguage(C);
			if (ABRPlayerController* PC = GetPC())
			{
				PC->SetMenuPage(EBRMenuPage::Main);
			}
			return true;
		});
		Add(TEXT("v4.8 langues : capture du menu ") + C, 0.f, [this, C]()
		{
			Shot(TEXT("V48_menu_") + C);
			return true;
		});
	}
	Add(TEXT("v4.8 langues : parametres en espagnol"), 1.2f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return true;
		}
		PC->bInMenu = bWasInMenu;
		PC->SetMenuPage(EBRMenuPage::Main);
		PC->UpdateInputMode();
		BRLoc::SetLanguage(TEXT("es"));
		PC->SetInventoryOpen(true, 2); // onglet PARAMETRES : les libelles les plus longs
		return true;
	});
	Add(TEXT("v4.8 langues : capture des parametres"), 0.f, [this]()
	{
		Shot(TEXT("V48_parametres_es"));
		if (ABRPlayerController* PC = GetPC())
		{
			PC->SetInventoryOpen(false);
		}
		return true;
	});
	Add(TEXT("v4.8 langues : langue du joueur retablie"), 0.f, [this]()
	{
		if (bLanguageSaved)
		{
			BRLoc::SavePreference(TestLanguagePref);
			BRLoc::SetLanguage(TestLanguage);
			bLanguageSaved = false;
			Note(FString::Printf(TEXT("langue retablie : %s (preference \"%s\")"), BRLoc::Current().Code, *BRLoc::Preference()));
		}
		return true;
	});
}

void ABRAutoTest::AddNetV48Steps(bool bClient)
{
	// Langue propre a chaque machine : le client passe en anglais (sans l'enregistrer), l'hote garde la sienne ; seuls des
	// identifiants et des valeurs circulent, chaque machine compose ses textes
	Add(TEXT("Reseau v4.8 : langue de chaque machine"), 1.f, [this, bClient]()
	{
		TestLanguage = BRLoc::Current().Code;
		if (bClient)
		{
			BRLoc::SetLanguage(TEXT("en"));
		}
		Note(FString::Printf(TEXT("%s : langue %s, %d texte(s) sans traduction"), bClient ? TEXT("client") : TEXT("hote"), BRLoc::Current().Code, BRLoc::CountMissing()),
			BRLoc::CountMissing() > 0);
		return true;
	});
	Add(TEXT("Reseau v4.8 : capture de la langue"), 0.f, [this, bClient]()
	{
		Shot(bClient ? TEXT("Net_langue_client") : TEXT("Net_langue_hote"));
		return true;
	});

	// Coup decide par le serveur : la sante du client baisse une seule fois (avant : appliquee chez lui en plus)
	Add(TEXT("Reseau v4.8 : coup decide par le serveur"), 0.f, [this, bClient]()
	{
		if (bClient)
		{
			if (ABRCharacter* Me = GetPlayer())
			{
				// Sante pleine des deux cotes (l'hote fait de meme sur le serveur) : le coup de 30 ne peut pas tuer
				Me->bGodMode = false;
				Me->Health = 100.f;
				TestHealth = Me->Health;
			}
			return true;
		}
		if (StepTime < 4.f)
		{
			return false; // le temps que le client ait quitte le mode invincible (etat replique)
		}
		ABRCharacter* Mate = NetMate.Get();
		if (!Mate)
		{
			Note(TEXT("reseau v4.8 : aucun coequipier"), true);
			return true;
		}
		Mate->bGodMode = false;
		Mate->Health = 100.f;
		const float Before = Mate->Health;
		Mate->ReceiveAttack(30.f, 0.f, nullptr);
		TestServerHealth = Mate->Health;
		Note(FString::Printf(TEXT("hote : coup de 30 sur le coequipier, sante vue par le serveur %.0f -> %.0f"), Before, TestServerHealth),
			Before - TestServerHealth < 20.f || TestServerHealth <= 0.f);
		return true;
	});
	Add(TEXT("Reseau v4.8 : sante apres le coup"), 0.f, [this, bClient]()
	{
		if (bClient)
		{
			const ABRCharacter* Me = GetPlayer();
			if (!Me)
			{
				return false;
			}
			if (Me->Health > TestHealth - 1.f && StepTime < 15.f)
			{
				return false;
			}
			const float Delta = TestHealth - Me->Health;
			Note(FString::Printf(TEXT("client : sante %.0f -> %.0f (baisse %.0f, coup de 30, gilet : 21)"), TestHealth, Me->Health, Delta), Delta < 1.f);
			return true;
		}
		if (StepTime < 3.f)
		{
			return false;
		}
		// Le client renvoie sa sante au serveur : s'il avait retire le coup une seconde fois, le serveur verrait la baisse
		const ABRCharacter* Mate = NetMate.Get();
		if (Mate)
		{
			const bool bOk = FMath::Abs(Mate->Health - TestServerHealth) <= 3.f;
			Note(FString::Printf(TEXT("hote : sante du coequipier 3 s apres le coup %.0f (juste apres : %.0f) : %s"), Mate->Health, TestServerHealth,
				bOk ? TEXT("une seule application") : TEXT("DOUBLE APPLICATION")), !bOk);
		}
		return true;
	});

	// Reveil premature : refuse par le serveur, le joueur reste a terre le temps restant
	Add(TEXT("Reseau v4.8 : coup mortel"), 0.f, [this, bClient]()
	{
		bRespawnRequested = false;
		TestFirstLog = LogLineCount();
		if (bClient)
		{
			return true;
		}
		if (StepTime < 3.f)
		{
			return false;
		}
		if (ABRCharacter* Mate = NetMate.Get())
		{
			Mate->ReceiveAttack(500.f, 0.f, nullptr);
		}
		return true;
	});
	Add(TEXT("Reseau v4.8 : reveil premature"), 0.f, [this, bClient]()
	{
		if (bClient)
		{
			ABRCharacter* Me = GetPlayer();
			if (!Me)
			{
				return false;
			}
			// A terre chez soi et vu a terre par le serveur (etat replique), puis demande de reveil tout de suite
			if (!(Me->IsDead() && Me->GetDeathState().bDead))
			{
				if (StepTime > 15.f)
				{
					Note(TEXT("client : pas a terre 15 s apres le coup mortel"), true);
					return true;
				}
				return false;
			}
			Me->ServerReportRespawn();
			bRespawnRequested = true;
			return true;
		}
		const ABRCharacter* Mate = NetMate.Get();
		if (!Mate)
		{
			return true;
		}
		if (!Mate->GetDeathState().bDead)
		{
			if (StepTime > 15.f)
			{
				Note(TEXT("hote : coequipier pas a terre 15 s apres le coup mortel"), true);
				return true;
			}
			return false;
		}
		return true;
	});
	Add(TEXT("Reseau v4.8 : reveil refuse"), 0.f, [this, bClient]()
	{
		if (StepTime < 3.f)
		{
			return false;
		}
		if (bClient)
		{
			const ABRCharacter* Me = GetPlayer();
			const ABRWorld* W = GetBRWorld();
			if (!bRespawnRequested || !Me || !W)
			{
				return true;
			}
			const bool bOk = Me->IsDead() && W->GetDeathTimer() > 0.f;
			Note(FString::Printf(TEXT("client : reveil demande trop tot : toujours a terre %s, temps restant %.1f s"), Me->IsDead() ? TEXT("oui") : TEXT("NON"), W->GetDeathTimer()),
				!bOk);
			return true;
		}
		const ABRCharacter* Mate = NetMate.Get();
		if (!Mate)
		{
			return true;
		}
		const FBRDeathState& DS = Mate->GetDeathState();
		const bool bRefused = LogContains(TestFirstLog, TEXT("reveil refuse"));
		const bool bOk = DS.bDead && DS.Event != 3 && bRefused;
		Note(FString::Printf(TEXT("hote : reveil premature du coequipier refuse %s, toujours a terre %s (evenement %d)"), bRefused ? TEXT("oui") : TEXT("NON"),
			DS.bDead ? TEXT("oui") : TEXT("NON"), DS.Event), !bOk);
		return true;
	});
	Add(TEXT("Reseau v4.8 : reanimation"), 0.f, [this, bClient]()
	{
		if (bClient)
		{
			return true;
		}
		ABRCharacter* Me = GetPlayer();
		ABRCharacter* Mate = NetMate.Get();
		if (Me && Mate)
		{
			Me->SetActorLocation(Mate->GetActorLocation() + FVector(120.f, 0.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
			Me->ServerRevive(Mate);
		}
		return true;
	});
	Add(TEXT("Reseau v4.8 : releve"), 1.f, [this, bClient]()
	{
		ABRCharacter* Target = bClient ? GetPlayer() : NetMate.Get();
		if (!Target)
		{
			return true;
		}
		const bool bDown = bClient ? Target->IsDead() : Target->GetDeathState().bDead;
		if (bDown && StepTime < 12.f)
		{
			return false;
		}
		Note(FString::Printf(TEXT("%s : releve apres le refus %s"), bClient ? TEXT("client") : TEXT("hote"), bDown ? TEXT("NON") : TEXT("oui")), bDown);
		Target->bGodMode = true;
		if (bClient)
		{
			BRLoc::SetLanguage(TestLanguage);
		}
		return true;
	});
}
