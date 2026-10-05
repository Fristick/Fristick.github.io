#include "BRSave.h"
#include "Backrooms.h"

#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

// v4.7 : les fichiers sont lus et ecrits ici directement, au meme endroit que le systeme de sauvegarde generique
// d'Unreal sur PC (Saved/SaveGames/<nom>.sav) : les parties v4.1-v4.6 sont retrouvees telles quelles.
// - Instantane : l'objet est serialise sur le thread du jeu (etat coherent), puis le fichier est ecrit sur un thread
//   de fond, dans l'ordre des demandes (une seule file, un seul ecrivain).
// - Ecriture atomique : fichier temporaire puis renommage ; le principal, puis la copie de secours.
// - Lecture : principal, sinon copie de secours ; si les deux sont illisibles, le fichier est mis de cote.

namespace BRSaves
{
	namespace
	{
		struct FWriteJob
		{
			int32 Slot = INDEX_NONE;
			uint32 RequestId = 0;
			TArray<uint8> Bytes;
		};

		FCriticalSection& QueueLock()
		{
			static FCriticalSection Lock;
			return Lock;
		}
		TArray<FWriteJob>& Queue()
		{
			static TArray<FWriteJob> Jobs;
			return Jobs;
		}
		bool& WorkerRunning()
		{
			static bool bRunning = false;
			return bRunning;
		}
		/** v4.8 : echecs d'ecriture gardes jusqu'a leur acquittement (une reussite suivante ne les efface plus) */
		TArray<FWriteFailure>& Failures()
		{
			static TArray<FWriteFailure> List;
			return List;
		}
		uint32& NextRequestId()
		{
			static uint32 Id = 0;
			return Id;
		}
		/** v4.8 : emplacements dont le fichier vient d'une version plus recente du jeu (format) : jamais reecrits */
		TMap<int32, int32>& FutureSlots()
		{
			static TMap<int32, int32> Map;
			return Map;
		}
		/** File protegee par QueueLock */
		void AddFailure(int32 Slot, uint32 RequestId, const FString& Reason)
		{
			FWriteFailure F;
			F.Slot = Slot;
			F.RequestId = RequestId;
			F.Reason = Reason;
			F.When = FDateTime::Now();
			Failures().Add(F);
		}
		TFuture<void>& Worker()
		{
			static TFuture<void> Future;
			return Future;
		}
		TArray<FString>& Messages()
		{
			static TArray<FString> List;
			return List;
		}

		FString PathOf(const FString& Name)
		{
			return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), Name + TEXT(".sav"));
		}

		bool FileThere(const FString& Name)
		{
			return IFileManager::Get().FileExists(*PathOf(Name));
		}

		/** Fichier temporaire puis renommage : un arret brutal laisse l'ancien fichier entier, jamais un fichier coupe.
		 *  v4.8 : OutReason dit quelle etape a echoue */
		bool WriteFileAtomic(const FString& Name, const TArray<uint8>& Bytes, FString* OutReason = nullptr)
		{
			const FString Path = PathOf(Name);
			const FString Tmp = Path + TEXT(".tmp");
			if (!FFileHelper::SaveArrayToFile(Bytes, *Tmp))
			{
				if (OutReason)
				{
					*OutReason = FString::Printf(TEXT("ecriture de %s.tmp impossible (disque plein ou dossier protege)"), *Name);
				}
				return false;
			}
			if (!IFileManager::Get().Move(*Path, *Tmp, true, true))
			{
				if (OutReason)
				{
					*OutReason = FString::Printf(TEXT("remplacement de %s.sav impossible (fichier verrouille ?)"), *Name);
				}
				return false;
			}
			return true;
		}

		/** Principal, puis secours : a tout instant l'un des deux est complet */
		bool WriteBoth(int32 Slot, const TArray<uint8>& Bytes, FString* OutReason = nullptr)
		{
			FString MainReason;
			FString BackupReason;
			const bool bMain = WriteFileAtomic(SlotName(Slot), Bytes, &MainReason);
			const bool bBackup = WriteFileAtomic(BackupSlotName(Slot), Bytes, &BackupReason);
			if (OutReason)
			{
				*OutReason = !bMain ? MainReason : BackupReason;
			}
			return bMain && bBackup;
		}

		UBRSaveGame* FromBytes(const TArray<uint8>& Bytes)
		{
			// En-tete d'Unreal (quelques dizaines d'octets) + au moins le nom de la classe : en dessous, fichier tronque
			if (Bytes.Num() < 64)
			{
				return nullptr;
			}
			return Cast<UBRSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		}

		/** Instantane coherent, pris sur le thread du jeu. Jamais pour un fichier d'un format plus recent (refuse avant) */
		bool Snapshot(UBRSaveGame* Save, TArray<uint8>& Out)
		{
			check(!Save->bFutureFormat);
			Save->Version = UBRSaveGame::CurrentVersion;
			Save->LastPlayed = FDateTime::Now();
			return UGameplayStatics::SaveGameToMemory(Save, Out) && Out.Num() > 0;
		}

		void ProcessQueue()
		{
			for (;;)
			{
				FWriteJob Job;
				{
					FScopeLock Lock(&QueueLock());
					if (Queue().Num() == 0)
					{
						WorkerRunning() = false;
						return;
					}
					Job = MoveTemp(Queue()[0]);
					Queue().RemoveAt(0);
				}
				FString Reason;
				const bool bOk = WriteBoth(Job.Slot, Job.Bytes, &Reason);
				FScopeLock Lock(&QueueLock());
				if (!bOk)
				{
					// v4.8 : l'echec reste jusqu'a son acquittement, avec l'emplacement et le numero de la demande
					AddFailure(Job.Slot, Job.RequestId, Reason);
				}
			}
		}

		/** Ancien format : migre en memoire ; une copie intacte du fichier d'origine est gardee une fois.
		 *  v4.8 : format plus recent : le fichier est laisse intact, la partie est marquee en lecture seule et l'emplacement
		 *  protege contre toute reecriture (avant : lu tel quel, puis reecrit en format 2 a la sauvegarde suivante) */
		void Migrate(UBRSaveGame* Save, int32 Slot, const TArray<uint8>& OriginalBytes)
		{
			Save->LoadedVersion = Save->Version;
			if (Save->Version > UBRSaveGame::CurrentVersion)
			{
				Save->bFutureFormat = true;
				{
					FScopeLock Lock(&QueueLock());
					FutureSlots().Add(Slot, Save->Version);
				}
				UE_LOG(LogBackrooms, Warning, TEXT("%s : format %d plus recent que ce jeu (%d) : fichier preserve, partie en lecture seule"),
					*SlotName(Slot), Save->Version, UBRSaveGame::CurrentVersion);
				Messages().Add(FString::Printf(TEXT("Partie \u00ab %s \u00bb : cr\u00e9\u00e9e par une version plus r\u00e9cente du jeu (format %d). ")
					TEXT("Elle est conserv\u00e9e intacte et ne peut pas \u00eatre reprise ici."), *Save->SaveName, Save->Version));
				return;
			}
			if (Save->Version == UBRSaveGame::CurrentVersion)
			{
				return;
			}
			const FString Legacy = LegacySlotName(Slot, Save->Version);
			if (!FileThere(Legacy) && OriginalBytes.Num() > 0)
			{
				FFileHelper::SaveArrayToFile(OriginalBytes, *PathOf(Legacy));
			}
			// Format 1 : ni graine ni position. Inventaire, journal, niveaux explores et temps de jeu sont gardes ;
			// le prochain niveau charge sera une disposition neuve (comme en v4.6).
			Save->Session = FBRSessionState();
			Save->bPendingDeath = false;
			Save->PendingDeathLevel = 0;
			Save->Version = UBRSaveGame::CurrentVersion;
			UE_LOG(LogBackrooms, Log, TEXT("%s : sauvegarde au format %d migree vers le format %d (copie d'origine : %s.sav)"), *SlotName(Slot),
				Save->LoadedVersion, UBRSaveGame::CurrentVersion, *Legacy);
		}
	}

	namespace
	{
		FString& Prefix()
		{
			static FString Value = TEXT("BR_Partie_");
			return Value;
		}
	}

	void SetTestPrefix(const FString& InPrefix)
	{
		Flush();
		Prefix() = InPrefix.IsEmpty() ? FString(TEXT("BR_Partie_")) : InPrefix;
	}

	FString FilePath(const FString& SlotFileName)
	{
		return PathOf(SlotFileName);
	}

	FString SlotName(int32 Slot)
	{
		return FString::Printf(TEXT("%s%d"), *Prefix(), Slot + 1);
	}

	FString BackupSlotName(int32 Slot)
	{
		return SlotName(Slot) + TEXT("_Secours");
	}

	FString UnreadableSlotName(int32 Slot)
	{
		return SlotName(Slot) + TEXT("_Illisible");
	}

	FString LegacySlotName(int32 Slot, int32 Version)
	{
		return FString::Printf(TEXT("%s_Format%d"), *SlotName(Slot), Version);
	}

	UBRSaveGame* Load(int32 Slot)
	{
		if (Slot < 0 || Slot >= MaxSlots)
		{
			return nullptr;
		}
		Flush(); // ne jamais lire un fichier pendant qu'il est remplace
		const FString Main = SlotName(Slot);
		const FString Backup = BackupSlotName(Slot);
		const bool bMain = FileThere(Main);
		const bool bBackup = FileThere(Backup);
		{
			FScopeLock Lock(&QueueLock());
			FutureSlots().Remove(Slot); // relu ci-dessous (et protege de nouveau si besoin)
		}
		if (!bMain && !bBackup)
		{
			return nullptr;
		}
		TArray<uint8> Bytes;
		UBRSaveGame* Save = nullptr;
		if (bMain && FFileHelper::LoadFileToArray(Bytes, *PathOf(Main)))
		{
			Save = FromBytes(Bytes);
		}
		if (!Save && bBackup)
		{
			TArray<uint8> BackupBytes;
			if (FFileHelper::LoadFileToArray(BackupBytes, *PathOf(Backup)))
			{
				Save = FromBytes(BackupBytes);
			}
			if (Save)
			{
				// Le principal est illisible (ou absent) : on le garde de cote, puis on le remplace par la copie de secours
				if (bMain)
				{
					IFileManager::Get().Move(*PathOf(UnreadableSlotName(Slot)), *PathOf(Main), true, true);
				}
				WriteFileAtomic(Main, BackupBytes);
				Save->bRecovered = true;
				Bytes = MoveTemp(BackupBytes);
				UE_LOG(LogBackrooms, Warning, TEXT("%s illisible : partie reprise depuis la copie de secours"), *Main);
				Messages().Add(FString::Printf(TEXT("Partie \u00ab %s \u00bb : fichier principal illisible, reprise depuis la copie de secours."),
					*Save->SaveName));
			}
		}
		if (!Save)
		{
			// Rien de lisible : le fichier est mis de cote (pas efface) et l'emplacement redevient libre
			const FString Aside = UnreadableSlotName(Slot);
			if (bMain)
			{
				IFileManager::Get().Move(*PathOf(Aside), *PathOf(Main), true, true);
			}
			if (bBackup)
			{
				IFileManager::Get().Move(*PathOf(Aside + TEXT("_Secours")), *PathOf(Backup), true, true);
			}
			UE_LOG(LogBackrooms, Error, TEXT("%s illisible, sans copie de secours valide : mise de cote sous %s.sav"), *Main, *Aside);
			Messages().Add(FString::Printf(TEXT("Emplacement %d : sauvegarde illisible. Le fichier est gard\u00e9 \u00e0 part (Saved/SaveGames/%s.sav) et l'emplacement est libre."),
				Slot + 1, *Aside));
			return nullptr;
		}
		Migrate(Save, Slot, Bytes);
		return Save;
	}

	namespace
	{
		/** v4.8 : refus d'ecrire par-dessus une partie d'un format plus recent (fichier preserve) */
		bool RefuseFuture(int32 Slot, const UBRSaveGame* Save, uint32 RequestId)
		{
			FScopeLock Lock(&QueueLock());
			const int32* Future = FutureSlots().Find(Slot);
			if (!Future && !(Save && Save->bFutureFormat))
			{
				return false;
			}
			const int32 Version = Future ? *Future : Save->LoadedVersion;
			AddFailure(Slot, RequestId, FString::Printf(TEXT("partie d'un format plus recent (%d) : reecriture refusee, fichier preserve"), Version));
			UE_LOG(LogBackrooms, Warning, TEXT("%s : format %d plus recent que ce jeu : ecriture refusee"), *SlotName(Slot), Version);
			return true;
		}
	}

	bool Write(int32 Slot, UBRSaveGame* Save)
	{
		if (!Save || Slot < 0 || Slot >= MaxSlots)
		{
			return false;
		}
		const uint32 RequestId = ++NextRequestId();
		if (RefuseFuture(Slot, Save, RequestId))
		{
			return false;
		}
		TArray<uint8> Bytes;
		if (!Snapshot(Save, Bytes))
		{
			UE_LOG(LogBackrooms, Warning, TEXT("Echec de la serialisation de la sauvegarde %s"), *SlotName(Slot));
			return false;
		}
		Flush(); // les ecritures deja demandees passent avant
		FString Reason;
		const bool bOk = WriteBoth(Slot, Bytes, &Reason);
		if (!bOk)
		{
			FScopeLock Lock(&QueueLock());
			AddFailure(Slot, RequestId, Reason);
			UE_LOG(LogBackrooms, Warning, TEXT("Echec de l'ecriture de la sauvegarde %s (demande %u) : %s"), *SlotName(Slot), RequestId, *Reason);
		}
		return bOk;
	}

	bool WriteAsync(int32 Slot, UBRSaveGame* Save, uint32* OutRequestId)
	{
		if (!Save || Slot < 0 || Slot >= MaxSlots)
		{
			return false;
		}
		FWriteJob Job;
		Job.Slot = Slot;
		Job.RequestId = ++NextRequestId();
		if (OutRequestId)
		{
			*OutRequestId = Job.RequestId;
		}
		if (RefuseFuture(Slot, Save, Job.RequestId))
		{
			return false;
		}
		if (!Snapshot(Save, Job.Bytes))
		{
			UE_LOG(LogBackrooms, Warning, TEXT("Echec de la serialisation de la sauvegarde %s"), *SlotName(Slot));
			return false;
		}
		FScopeLock Lock(&QueueLock());
		// Un instantane plus recent du meme emplacement remplace celui qui attend encore (l'ordre est garde)
		bool bMerged = false;
		for (FWriteJob& Waiting : Queue())
		{
			if (Waiting.Slot == Slot)
			{
				Waiting.Bytes = MoveTemp(Job.Bytes);
				Waiting.RequestId = Job.RequestId;
				bMerged = true;
				break;
			}
		}
		if (!bMerged)
		{
			Queue().Add(MoveTemp(Job));
		}
		if (!WorkerRunning())
		{
			WorkerRunning() = true;
			Worker() = Async(EAsyncExecution::ThreadPool, []() { ProcessQueue(); });
		}
		return true;
	}

	void Flush()
	{
		for (;;)
		{
			TFuture<void> Pending;
			{
				FScopeLock Lock(&QueueLock());
				if (!WorkerRunning() && Queue().Num() == 0)
				{
					return;
				}
				Pending = MoveTemp(Worker());
			}
			if (Pending.IsValid())
			{
				Pending.Wait();
			}
			else
			{
				FPlatformProcess::Sleep(0.001f);
			}
		}
	}

	bool LastWriteSucceeded()
	{
		FScopeLock Lock(&QueueLock());
		return Failures().Num() == 0;
	}

	TArray<FWriteFailure> PendingFailures()
	{
		FScopeLock Lock(&QueueLock());
		return Failures();
	}

	void AcknowledgeFailures(uint32 UpToRequestId)
	{
		FScopeLock Lock(&QueueLock());
		Failures().RemoveAll([UpToRequestId](const FWriteFailure& F) { return F.RequestId <= UpToRequestId; });
	}

	int32 FutureFormatOf(int32 Slot)
	{
		FScopeLock Lock(&QueueLock());
		const int32* V = FutureSlots().Find(Slot);
		return V ? *V : 0;
	}

	TArray<FString> TakeLoadMessages()
	{
		TArray<FString> Out = MoveTemp(Messages());
		Messages().Reset();
		return Out;
	}

	void Delete(int32 Slot)
	{
		if (Slot < 0 || Slot >= MaxSlots)
		{
			return;
		}
		Flush();
		// La partie et sa copie de secours (et une eventuelle copie d'un ancien format) ; les fichiers illisibles mis de
		// cote restent, ils ne font plus partie d'aucun emplacement
		IFileManager::Get().Delete(*PathOf(SlotName(Slot)), false, true, true);
		IFileManager::Get().Delete(*PathOf(BackupSlotName(Slot)), false, true, true);
		IFileManager::Get().Delete(*PathOf(LegacySlotName(Slot, 1)), false, true, true);
		{
			FScopeLock Lock(&QueueLock());
			FutureSlots().Remove(Slot); // suppression explicite (confirmee) d'une partie d'un format plus recent
		}
		if (ActiveSlot() == Slot)
		{
			ActiveSlot() = INDEX_NONE;
			PendingResume() = FBRSessionState();
		}
	}

	int32 FreeSlot()
	{
		for (int32 i = 0; i < MaxSlots; ++i)
		{
			if (!FileThere(SlotName(i)) && !FileThere(BackupSlotName(i)))
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	int32& ActiveSlot()
	{
		static int32 Slot = INDEX_NONE;
		return Slot;
	}

	FBRSessionState& PendingResume()
	{
		static FBRSessionState Resume;
		return Resume;
	}

	FString FormatPlayTime(float Seconds)
	{
		const int32 Minutes = FMath::FloorToInt(FMath::Max(0.f, Seconds) / 60.f);
		if (Minutes < 60)
		{
			return FString::Printf(TEXT("%d min"), Minutes);
		}
		return FString::Printf(TEXT("%d h %02d"), Minutes / 60, Minutes % 60);
	}

	FString FormatDate(const FDateTime& Date)
	{
		return FString::Printf(TEXT("%02d/%02d/%04d %02d:%02d"), Date.GetDay(), Date.GetMonth(), Date.GetYear(), Date.GetHour(), Date.GetMinute());
	}
}
