// v4.16 (cooperation) : test reseau v4.16 (-BRNetTest -BRAutoTestV416Net), lance par Tools/Tests/run_nettest.ps1 : l'hote et 1 a 3
// clients sur ce PC (certains en -nullrhi), un client lance en cours de partie ou une reconnexion selon -BRNetV416Late, latence,
// gigue et pertes simulees par le moteur sur chaque machine (-PktLag, -PktLagVariance, -PktLoss : verifiees ici).
//
// Options : -BRNetV416Clients=<n> (hote : clients attendus au debut, 0 a 3), -BRNetV416Late=porte|lidar|passage|reconnexion
// (toutes les machines), -BRNetV416Tardif (le client lance en cours de partie), -BRNetTestName=<nom> (dossier
// Saved/NetTest_<nom>, BRAutoTest.cpp). Le plan reseau ordinaire (-BRNetTest seul) ne change pas.
//
// Sans -BRNetV416Late (scenarios de base, 2 a 4 joueurs) :
//   A. ramassage simultane : les clients demandent le meme objet au meme instant du serveur (l'hote s'abstient a partir de
//      deux clients : sa demande, appliquee sur place, passait toujours la premiere) : une seule attribution ;
//   B. depart de groupe simultane par la porte de service du Level 0 (meme regle) : un seul depart, liste des joueurs
//      attendus, un seul changement de niveau ;
//   C. Poolrooms : C1 mort d'un client a plus de 150 m de ses coequipiers (toujours repliques chez lui depuis la relecture),
//      coequipiers debout : fenetre entiere, puis {Saut} a terre (abandon) accepte par l'hote ; C2 toute l'equipe a terre (le dernier
//      coequipier debout tombe apres) ; C3 le client lointain tombe apres tous les autres : 6 s comme en v4.15. Aucun reveil
//      refuse par l'hote, une seule mort chacun ; apres chaque reveil, chez l'hote : plus en chargement, position recue,
//      aucune chute constatee ;
//   D. Poolrooms : sas du passage sec manipule par un client, hote a plus de 35 m : deversoir (collision) et eau chez l'hote ;
//      la derniere action est demandee au meme instant par deux clients : etat de la mission et retours conformes aux regles
//      appliquees une demande apres l'autre ;
//   E. Niveau 8 : passerelle abaissee par l'action d'un client, hote loin : tablier (collision), navigation des entites ;
//   F. Level 6 : inventaire, pause, changement de touches et vraie perte de focus de la fenetre (reduite par le test,
//      FSlateApplication::IsActive) pendant un scan focalise, un sprint et une interaction maintenus (touches simulees par les
//      entrees du joueur) : scan, son et actions arretes, nouvel appui requis (hote, puis le premier client) ;
//   G. Level 6 : un client tombe dans une fosse de l'aile LiDAR puis se reveille : aucune seconde chute imputee a sa position
//      d'avant le reveil, plus en chargement chez l'hote.
// Avec -BRNetV416Late :
//   porte       : partie d'histoire de l'hote (sauvegarde de test), porte de service du Level 0 ouverte, objet rendu au monde, puis
//                 un client rejoint : meme porte sauvegardee, mur perce, passage libre (collision et vue), porte ouverte et
//                 toujours prise par la visee, aucun reveil impose, objet rendu visible ;
//   lidar       : Level 6, calibrage et deux postes faits, puis un client rejoint : meme plan, memes etats, rien de remis a zero ;
//   passage     : Level 6 jusqu'a la bonne porte du passage ouverte : chez le client tardif, la meme porte ouverte (battant,
//                 collision) ;
//   reconnexion : un client se reconnecte pendant un chargement du niveau : entrees deverrouillees, plus "en chargement" chez
//                 l'hote, aucune chute imputee a une ancienne position, objet rendu au monde visible.
// Barrieres : un client signale son etape (ServerCheat 4, NetTestMark) ; l'hote ouvre l'etape suivante par le score de son
// PlayerState (replique a tous, inutilise par le jeu ; aucun test n'existe en version finale).
// Comparaisons entre machines : chacune ecrit V416.txt dans son dossier ; l'hote les lit a la fin (meme PC ; sur deux PC : NON
// VERIFIE). Une condition qui manque (decor absent, materiel, fenetre jamais active, rendu nul) est NON VERIFIE, jamais reussie.
#include "BRAutoTest.h"
#include "BRAutoTestNet.h"
#include "BRAutoTestWalk.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BRHUD.h"
#include "BRInteractables.h"
#include "BRKeys.h"
#include "BRLevels.h"
#include "BRLidar.h"
#include "BRMechLogic.h"
#include "BRMission.h"
#include "BRMissionLogic.h"
#include "BRMissionSolver.h"
#include "BRPlayerController.h"
#include "BRSave.h"
#include "BRStoryLogic.h"
#include "BRTxnLogic.h"
#include "BRWorld.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Widgets/SWindow.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace BRM = BRMission;

namespace BRTestV416Net
{
	const TCHAR* const SavePrefix = TEXT("BR_AutoTestV416_");
	constexpr int32 StorySlot = 5;
	/** Graines utilisateur (ABRWorld::SeedFromUser) : Level 0 de la porte, Poolrooms (bassins et sas : banc v4.12), Niveau 8
	 *  (passerelle : banc v4.12), Level 6 */
	constexpr uint32 DoorSeed = 9;
	constexpr uint32 PoolSeed = 4120;
	constexpr uint32 BridgeSeed = 4128;
	constexpr uint32 LidarSeed = 4126;
	/** Distance (cm) au-dela de laquelle l'hote n'animait plus un mecanisme (ABRWorld::UpdateMissionDevices) : l'hote se tient
	 *  plus loin pour les essais D et E */
	constexpr float HostFar = 4500.f;
	constexpr int32 MarkFinished = BRTestNet::MarkFinished;
	/** Touche ajoutee au scan focalise pendant l'essai des touches (3e emplacement, vide par defaut) ; fonction : pas de
	 *  copie d'EKeys a l'initialisation statique (ordre des unites de compilation non garanti dans un executable monolithique) */
	FKey ExtraKey()
	{
		return EKeys::NumPadNine;
	}

	const TCHAR* Oui(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	struct FOpts
	{
		int32 Clients = 1;
		FString Late;
		bool bLate = false;
		bool bParsed = false;
	};
	FOpts GOpts;

	const FOpts& Opts()
	{
		if (!GOpts.bParsed)
		{
			GOpts.bParsed = true;
			FParse::Value(FCommandLine::Get(), TEXT("BRNetV416Clients="), GOpts.Clients);
			GOpts.Clients = FMath::Clamp(GOpts.Clients, 0, 3);
			FParse::Value(FCommandLine::Get(), TEXT("BRNetV416Late="), GOpts.Late);
			GOpts.Late = GOpts.Late.ToLower();
			if (GOpts.Late == TEXT("aucun") || GOpts.Late == TEXT("none"))
			{
				GOpts.Late.Reset();
			}
			GOpts.bLate = FParse::Param(FCommandLine::Get(), TEXT("BRNetV416Tardif"));
		}
		return GOpts;
	}

	// ------------------------------------------------------------------------------------------------------ barrieres
	/** Etat de joueur de l'hote : le sien chez l'hote, le plus petit identifiant chez un client (attribue dans l'ordre d'arrivee) */
	APlayerState* HostState(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		if (World->GetNetMode() != NM_Client)
		{
			const APlayerController* Local = UGameplayStatics::GetPlayerController(World, 0);
			return Local ? Local->PlayerState.Get() : nullptr;
		}
		const AGameStateBase* GS = World->GetGameState();
		APlayerState* Best = nullptr;
		if (GS)
		{
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (PS && (!Best || PS->GetPlayerId() < Best->GetPlayerId()))
				{
					Best = PS;
				}
			}
		}
		return Best;
	}

	void SetHostPhase(UWorld* World, int32 Phase)
	{
		APlayerState* PS = HostState(World);
		if (PS && World && World->GetNetMode() != NM_Client && PS->GetScore() < static_cast<float>(Phase))
		{
			PS->SetScore(static_cast<float>(Phase));
			PS->ForceNetUpdate();
			UE_LOG(LogBackrooms, Log, TEXT("[AutoTest] V416 : etape %d ouverte par l'hote"), Phase);
		}
	}

	int32 HostPhase(UWorld* World)
	{
		const APlayerState* PS = HostState(World);
		return PS ? FMath::RoundToInt(PS->GetScore()) : 0;
	}

	/** v4.17 : camera posee devant un coequipier (capture de sa combinaison) */
	TWeakObjectPtr<ACameraActor> MateCam;

	struct FMate
	{
		ABRCharacter* Pawn = nullptr;
		ABRPlayerController* Ctrl = nullptr;
		int32 Slot = 0;
		int32 Mark = 0;
		int32 Id = -1;
	};

	/** Hote : les personnages des clients, leur place (rang de l'identifiant) et la derniere etape signalee */
	TArray<FMate> Mates(ABRWorld* W)
	{
		TArray<FMate> Out;
		TArray<ABRCharacter*> All;
		if (W)
		{
			W->GetPlayers(All);
		}
		for (ABRCharacter* C : All)
		{
			if (!C || C->IsLocallyControlled())
			{
				continue;
			}
			FMate M;
			M.Pawn = C;
			M.Ctrl = Cast<ABRPlayerController>(C->GetController());
			M.Slot = W->PlayerSlot(C->GetPlayerState());
			M.Mark = M.Ctrl ? M.Ctrl->NetTestMark : 0;
			M.Id = C->GetPlayerState() ? C->GetPlayerState()->GetPlayerId() : -1;
			Out.Add(M);
		}
		return Out;
	}

	bool MateAt(ABRWorld* W, int32 Slot, FMate& Out)
	{
		for (const FMate& M : Mates(W))
		{
			if (M.Slot == Slot)
			{
				Out = M;
				return true;
			}
		}
		return false;
	}

	int32 MySlot(const ABRWorld* W, const APlayerController* PC)
	{
		return (W && PC) ? W->PlayerSlot(PC->PlayerState) : 0;
	}

	// ------------------------------------------------------------------------------------------------------ simultaneite
	/** v4.16 (relecture) : demandes simultanees de plusieurs machines. Temps du serveur (AGameStateBase::GetServerWorldTimeSeconds,
	 *  le meme partout a la latence pres) : l'hote ouvre l'etape juste apres un multiple de SyncPeriod ; chaque machine qui la
	 *  voit ouverte vise le multiple le plus proche de son estimation + SyncLead (SyncTarget). Les demandes partent ensemble et
	 *  arrivent chez l'hote dans des images voisines (avant : l'hote demandait le premier, appliquait sur place, et les
	 *  clients ne demandaient qu'apres avoir recu l'ouverture : rien n'etait en concurrence) */
	constexpr double SyncPeriod = 10.0;
	constexpr double SyncLead = 6.0;

	double ServerNow(const UWorld* World)
	{
		const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? GS->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	}

	/** Hote : l'etape peut s'ouvrir maintenant (debut d'une periode) */
	bool SyncOpenNow(const UWorld* World)
	{
		return FMath::Fmod(ServerNow(World), SyncPeriod) < 0.5;
	}

	/** Instant commun des demandes, pour une etape vue ouverte maintenant. v4.16 (correctif) : le multiple le plus proche, et
	 *  non plus celui d'avant. Chez un client, GetServerWorldTimeSeconds n'est qu'une estimation (temps de l'hote envoye
	 *  toutes les 0,1 s, sans la latence, ecart lisse sur des centaines d'envois : GameStateBase.cpp) : un peu en retard, une
	 *  etape ouverte 0 a 0,5 s apres le multiple P y semblait ouverte avant P, d'ou la visee P - 4, deja passee, et une
	 *  demande aussitot (net416_4j : etape 11 ouverte a 20.48.44:607, objet deja pris chez un client a 44:664 ; etape 21
	 *  ouverte image 429, les trois demandes du depart recues images 4430 et 4431, 6 s avant l'instant de l'hote). Ecart
	 *  admis desormais : moins de SyncPeriod / 2 de part et d'autre (avant : de 0 a SyncLead - 0,5 s, en avance seulement) */
	double SyncTarget(const UWorld* World)
	{
		return FMath::RoundToDouble(ServerNow(World) / SyncPeriod) * SyncPeriod + SyncLead;
	}

	/** Ecart (images de l'hote) entre les demandes recues de ces personnages depuis la remise a zero (0 : pas de demande) */
	FString FrameSpread(const TArray<const ABRCharacter*>& Who, int32& OutWith)
	{
		uint64 Lo = 0;
		uint64 Hi = 0;
		OutWith = 0;
		for (const ABRCharacter* C : Who)
		{
			const uint64 F = C ? C->ServerLastRequestFrame : 0;
			if (F == 0)
			{
				continue;
			}
			Lo = OutWith == 0 ? F : FMath::Min(Lo, F);
			Hi = OutWith == 0 ? F : FMath::Max(Hi, F);
			++OutWith;
		}
		return OutWith == 0 ? FString(TEXT("aucune")) : FString::Printf(TEXT("images %llu a %llu (ecart %llu)"), Lo, Hi, Hi - Lo);
	}

	/** Une etape pour ce client : signalee une seule fois */
	int32 GSentMark = 0;
	void Mark(APlayerController* PC, int32 Value)
	{
		ABRPlayerController* BRPC = Cast<ABRPlayerController>(PC);
		if (BRPC && Value > GSentMark)
		{
			GSentMark = Value;
			BRTestNet::SendMark(BRPC, Value);
		}
	}

	// ------------------------------------------------------------------------------------------------------ fichiers
	/** Une valeur a comparer entre machines (regle "egal" : la meme partout ; "somme=N" : somme des valeurs entieres) */
	void Put(const FString& Dir, const FString& Bloc, const FString& Key, const FString& Value, const TCHAR* Rule)
	{
		FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s|%s|%s|%s\n"), *Bloc, *Key, *Value, Rule), *FPaths::Combine(Dir, TEXT("V416.txt")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}

	struct FPeerValue
	{
		FString Machine;
		FString Value;
		FString Rule;
	};

	/** Valeurs de toutes les machines de cet essai (dossiers Saved/NetTest_* ecrits apres Since) : cle "bloc|cle" */
	int32 ReadPeers(const FDateTime& Since, TMap<FString, TArray<FPeerValue>>& Out)
	{
		const FString Saved = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		TArray<FString> Dirs;
		IFileManager::Get().FindFiles(Dirs, *FPaths::Combine(Saved, TEXT("NetTest_*")), false, true);
		int32 Machines = 0;
		for (const FString& D : Dirs)
		{
			const FString File = FPaths::Combine(Saved, D, TEXT("V416.txt"));
			if (!IFileManager::Get().FileExists(*File) || IFileManager::Get().GetTimeStamp(*File) < Since)
			{
				continue;
			}
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *File))
			{
				continue;
			}
			++Machines;
			TArray<FString> Lines;
			Text.ParseIntoArrayLines(Lines);
			for (const FString& L : Lines)
			{
				TArray<FString> Parts;
				L.ParseIntoArray(Parts, TEXT("|"), false);
				if (Parts.Num() >= 4)
				{
					FPeerValue V;
					V.Machine = D.RightChop(8);
					V.Value = Parts[2];
					V.Rule = Parts[3];
					Out.FindOrAdd(Parts[0] + TEXT("|") + Parts[1]).Add(V);
				}
			}
		}
		return Machines;
	}

	// ------------------------------------------------------------------------------------------------------ places
	/** Sol sous (P.X, P.Y) pres de la hauteur attendue FloorZ (trace WorldStatic de FloorZ + 100 a FloorZ - 410, sous tout
	 *  plafond, qui n'ignore que Self) */
	bool FloorAt(const ABRWorld* W, const AActor* Self, const FVector& P, float FloorZ, float& OutZ)
	{
		return BRTestV412::FloorUnder(W, FVector(P.X, P.Y, FloorZ + 40.f), FloorZ + 40.f, Self, OutZ);
	}

	void Hover(ABRCharacter* C, bool bOn)
	{
		if (UCharacterMovementComponent* M = C ? C->GetCharacterMovement() : nullptr)
		{
			M->StopMovementImmediately();
			M->SetMovementMode(bOn ? MOVE_Flying : MOVE_Walking);
		}
	}

	/** Regard vers le plafond : aucune cible pour l'interaction */
	void LookUp(ABRCharacter* C)
	{
		if (AController* Ctrl = C ? C->GetController() : nullptr)
		{
			FRotator R = Ctrl->GetControlRotation();
			R.Pitch = 80.f;
			Ctrl->SetControlRotation(R);
		}
	}

	/** Une cellule marchable (hors bassin), dont le chunk est construit et le sol trouve, a plus de MinDist de Avoid, la plus
	 *  proche de From */
	bool FarSpot(const ABRWorld* W, const ABRCharacter* C, const FVector& Avoid, float MinDist, FVector& Out)
	{
		if (!W || !C)
		{
			return false;
		}
		const FIntPoint Here = W->WorldToCell(C->GetActorLocation());
		const float Half = C->GetSimpleCollisionHalfHeight();
		for (int32 Ring = 0; Ring < 40; ++Ring)
		{
			for (int32 DX = -Ring; DX <= Ring; ++DX)
			{
				for (int32 DY = -Ring; DY <= Ring; ++DY)
				{
					if (FMath::Abs(DX) != Ring && FMath::Abs(DY) != Ring)
					{
						continue;
					}
					const FIntPoint Cell(Here.X + DX, Here.Y + DY);
					const FVector P = W->CellCenter(Cell, C->GetActorLocation().Z);
					if (!W->IsWalkable(Cell) || W->IsPoolCell(Cell.X, Cell.Y) || !W->IsChunkLoaded(W->CellToChunk(Cell)) || FVector::Dist2D(P, Avoid) < MinDist)
					{
						continue;
					}
					float Z = 0.f;
					if (FloorAt(W, C, P, W->FloorZAt(P), Z))
					{
						Out = FVector(P.X, P.Y, Z + Half + 5.f);
						return true;
					}
				}
			}
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------ missions
	ABRMissionDevice* FindModule(const ABRWorld* W, uint8 Module)
	{
		if (W)
		{
			for (ABRMissionDevice* D : W->GetMissionDevices())
			{
				if (IsValid(D) && D->GetModule() == Module)
				{
					return D;
				}
			}
		}
		return nullptr;
	}

	uint32 StateCrc(const ABRWorld* W)
	{
		const TArray<uint8> Blob = W ? W->GetMissionBlob() : TArray<uint8>();
		return FCrc::MemCrc32(Blob.GetData(), Blob.Num());
	}

	/** Ouverture affichee de chaque mecanisme ("indice:valeur" separes par des virgules) */
	FString Gates(const ABRWorld* W)
	{
		FString Out;
		if (W && W->IsMissionActive())
		{
			BRM::FEval E;
			W->GetMissionEval(E);
			for (int32 I = 0; I < W->GetMissionPlan().NumDevices; ++I)
			{
				Out += FString::Printf(TEXT("%s%d:%d"), Out.IsEmpty() ? TEXT("") : TEXT(","), I, static_cast<int32>(E.Gate[I]));
			}
		}
		return Out;
	}

	/** Solution de la mission depuis l'etat courant (solveur des bancs, actions du joueur) */
	bool Solve(const ABRWorld* W, TArray<BRM::FSolveStep>& Out)
	{
		Out.Reset();
		if (!W || !W->IsMissionActive())
		{
			return false;
		}
		BRM::FState Copy = W->GetMissionState();
		TUniquePtr<BRM::FSolver> Solver = MakeUnique<BRM::FSolver>(W->GetMissionPlan(), Copy, W->GetMissionCampaign());
		Solver->bPlayerActions = true;
		Solver->bRecord = true;
		const bool bOk = Solver->Solve(0) && !Solver->bOverflow;
		for (int32 I = 0; I < Solver->NumSteps; ++I)
		{
			Out.Add(Solver->Steps[I]);
		}
		return bOk && Out.Num() > 0;
	}

	/** v4.16 (correctif) : la mission est a une action de sa resolution selon l'etat de cette machine. Le nombre d'actions du
	 *  solveur n'est pas ce qui reste : il rejoue sa solution depuis l'etat donne (indice relu et manivelles finies : retour
	 *  "deja fait" ; vannes du sas ramenees a zero puis remises), soit 9 actions au sas et 4 a la passerelle quand une seule
	 *  reste (banc hors moteur, graines 115083815 et 1401743838 ; 6 et 13 depuis l'etat initial). Une action reste si la mission
	 *  n'est pas resolue et si la derniere action du solveur (celle que l'hote reserve au client), appliquee seule a l'etat de
	 *  cette machine comme l'hote l'applique (ABRWorld::ServerMissionAct : une unite pour une manivelle ou une observation),
	 *  la resout. Un etat perime (etat initial) n'en a aucune. OutReplayed : actions du solveur depuis cet etat (journal) */
	bool OneActionLeft(const ABRWorld* W, BRM::FSolveStep& Out, int32& OutReplayed)
	{
		OutReplayed = 0;
		TArray<BRM::FSolveStep> Steps;
		const bool bOk = Solve(W, Steps);
		OutReplayed = Steps.Num();
		if (!bOk)
		{
			return false;
		}
		BRM::FEval Before;
		W->GetMissionEval(Before);
		const BRM::FPlan& Plan = W->GetMissionPlan();
		const BRM::FSolveStep& Last = Steps.Last();
		if (Before.bSolved || Last.Device >= Plan.NumDevices)
		{
			return false;
		}
		const BRM::FDevice& D = Plan.Devices[Last.Device];
		const bool bHeld = D.Kind == BRM::EKind::Observe || D.Kind == BRM::EKind::Crank;
		BRM::FState After = W->GetMissionState();
		BRM::Act(Plan, After, W->GetMissionCampaign(), Last.Device, bHeld ? BRM::EAction::Hold : BRM::EAction::Use, bHeld ? 1 : 0);
		BRM::FEval E;
		BRM::Evaluate(Plan, After, W->GetMissionCampaign(), E);
		if (!E.bSolved)
		{
			return false;
		}
		Out = Last;
		return true;
	}

	/** v4.16 (relecture) : une seconde place valable devant Dev (BRTestV412::CheckMissionStand, cercles de FindMissionStand), a
	 *  plus d'un metre de la premiere (celle que prend le client de la place 1 : meme capsule, meme recherche) */
	bool SecondStand(const ABRWorld* W, const ABRCharacter* C, const ABRMissionDevice* Dev, FVector& Out)
	{
		FVector First;
		if (!W || !C || !Dev || !BRTestV412::FindMissionStand(W, C, Dev, false, First))
		{
			return false;
		}
		const FVector Aim = Dev->GetInteractPoint();
		const FVector Ahead = Dev->GetActorForwardVector().GetSafeNormal2D();
		const FVector Fwd = Ahead.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Ahead;
		for (const float Dist : { BRMech::Clearance::StandRange, 160.f, 210.f, 75.f, 240.f })
		{
			for (int32 Turn = 0; Turn < 16; ++Turn)
			{
				const FVector P = Aim + Fwd.RotateAngleAxis(Turn * 22.5f, FVector::UpVector) * Dist;
				FVector Stand;
				if (FVector::Dist2D(P, First) > 100.f && BRTestV412::CheckMissionStand(W, C, Dev, P, false, Stand) == BRTestV412::EStand::Ok)
				{
					Out = Stand;
					return true;
				}
			}
		}
		return false;
	}

	FString RoleOf(const ABRWorld* W, int32 Device)
	{
		return (W && Device >= 0 && Device < W->GetMissionPlan().NumDevices) ? FString::Printf(TEXT("role %d"), static_cast<int32>(W->GetMissionPlan().Devices[Device].Role))
			: FString(TEXT("?"));
	}

	ABRExit* FindServiceDoor(UWorld* World)
	{
		for (TActorIterator<ABRExit> It(World); It; ++It)
		{
			if (It->Style == EBRExitStyle::ServiceDoor && !It->IsActorBeingDestroyed())
			{
				return *It;
			}
		}
		return nullptr;
	}

	ABRPickup* SharedPickup(UWorld* World, const ABRWorld* W)
	{
		ABRPickup* Best = nullptr;
		if (!World || !W)
		{
			return nullptr;
		}
		const FVector Origin = W->CellCenter(FIntPoint(0, 0));
		for (TActorIterator<ABRPickup> It(World); It; ++It)
		{
			if (It->IsActorBeingDestroyed() || It->Item == EBRItem::VHSTape || It->Item == EBRItem::Note || It->IsPending()
				|| FVector::Dist2D(It->GetActorLocation(), Origin) > 3000.f || !W->IsChunkLoaded(W->CellToChunk(W->WorldToCell(It->GetActorLocation()))))
			{
				continue;
			}
			if (!Best || It->Id < Best->Id)
			{
				Best = *It;
			}
		}
		return Best;
	}

	ABRPickup* PickupById(UWorld* World, uint64 Id)
	{
		for (TActorIterator<ABRPickup> It(World); It; ++It)
		{
			if (It->Id == Id && !It->IsActorBeingDestroyed())
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Place autour d'un point pour la place Slot (angle selon la place), a vue du point et sur un sol */
	bool AroundSpot(const ABRWorld* W, const ABRCharacter* C, const FVector& Target, int32 Slot, FVector& Out)
	{
		UWorld* World = C ? C->GetWorld() : nullptr;
		if (!World)
		{
			return false;
		}
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestV416Around), false, C);
		const float Half = C->GetSimpleCollisionHalfHeight();
		for (const float Radius : { 85.f, 110.f, 60.f })
		{
			for (int32 Turn = 0; Turn < 8; ++Turn)
			{
				const float Ang = FMath::DegreesToRadians(90.f * static_cast<float>(Slot) + 45.f * static_cast<float>(Turn));
				const FVector P = Target + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * Radius;
				float Z = 0.f;
				if (!FloorAt(W, C, P, Target.Z, Z))
				{
					continue;
				}
				const FVector Stand(P.X, P.Y, Z + Half + 5.f);
				if (World->OverlapAnyTestByChannel(Stand, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(36.f, Half - 4.f), Q)
					|| World->LineTraceTestByChannel(Stand + FVector(0.f, 0.f, 60.f), Target + FVector(0.f, 0.f, 10.f), ECC_WorldStatic, Q))
				{
					continue;
				}
				Out = Stand;
				return true;
			}
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------ entrees
	/** Evenement de touche simule, par les entrees du joueur (Enhanced Input, comme une vraie touche) */
	void Inject(APlayerController* PC, const FKey& Key, EInputEvent Event)
	{
		if (!PC || !Key.IsValid())
		{
			return;
		}
		const FInputKeyEventArgs Args = FInputKeyEventArgs::CreateSimulated(Key, Event, Event == IE_Released ? 0.f : 1.f, -1,
			IPlatformInputDeviceMapper::Get().GetDefaultInputDevice());
		PC->InputKey(Args);
	}

	TSharedPtr<FGenericWindow> NativeWindow()
	{
		if (GEngine && GEngine->GameViewport)
		{
			const TSharedPtr<SWindow> Win = GEngine->GameViewport->GetWindow();
			if (Win.IsValid())
			{
				return Win->GetNativeWindow();
			}
		}
		return nullptr;
	}

	bool AppActive()
	{
		return FSlateApplication::IsInitialized() && FSlateApplication::Get().IsActive();
	}

	/** Fenetre du jeu au premier plan (Windows refuse le premier plan a un processus en arriere-plan : la file d'entree du
	 *  premier plan est attachee le temps de la demande) */
	void Activate()
	{
		const TSharedPtr<FGenericWindow> Native = NativeWindow();
		if (!Native.IsValid())
		{
			return;
		}
		Native->Restore();
		Native->BringToFront(true);
		Native->HACK_ForceToFront();
#if PLATFORM_WINDOWS
		HWND Hwnd = static_cast<HWND>(Native->GetOSWindowHandle());
		if (Hwnd)
		{
			HWND Front = ::GetForegroundWindow();
			const DWORD FrontThread = Front ? ::GetWindowThreadProcessId(Front, nullptr) : 0;
			const DWORD OwnThread = ::GetCurrentThreadId();
			const bool bAttached = FrontThread != 0 && FrontThread != OwnThread && ::AttachThreadInput(FrontThread, OwnThread, 1) != 0;
			::ShowWindow(Hwnd, SW_RESTORE);
			::BringWindowToTop(Hwnd);
			::SetForegroundWindow(Hwnd);
			::SetActiveWindow(Hwnd);
			if (bAttached)
			{
				::AttachThreadInput(FrontThread, OwnThread, 0);
			}
		}
#endif
	}

	/** Vraie perte de focus : la fenetre du jeu est reduite (le systeme active une autre fenetre) */
	void MinimizeWindow()
	{
		const TSharedPtr<FGenericWindow> Native = NativeWindow();
		if (Native.IsValid())
		{
			Native->Minimize();
		}
	}

	/** Le son du scan focalise joue (bHasSound : le scanner a un son a jouer) */
	bool LidarHum(const ABRLidar* L, bool& bHasSound)
	{
		bHasSound = false;
		if (!L)
		{
			return false;
		}
		TInlineComponentArray<UAudioComponent*> Comps(L);
		bool bPlaying = false;
		for (const UAudioComponent* A : Comps)
		{
			if (A && A->Sound)
			{
				bHasSound = true;
				bPlaying |= A->IsPlaying();
			}
		}
		return bPlaying;
	}

	// ------------------------------------------------------------------------------------------------------ etat
	struct FRun
	{
		int32 Slot = 0;
		int32 Phase = 0;
		double Clock = 0.0;
		double Clock2 = 0.0;
		int32 LogFrom = 0;
		FDateTime Since;
		BRTestNet::FPlacement Place;
		FVector Spot = FVector::ZeroVector;
		FVector Spot2 = FVector::ZeroVector;
		bool bSkipBlock = false;
		// v4.16 (relecture) : demandes simultanees : instant commun (temps du serveur), concurrence entre au moins deux clients
		// (l'hote s'abstient), demandes recues par l'hote avant l'essai
		double SyncAt = 0.0;
		bool bRace = false;
		int32 RequestsBefore = 0;
		int32 MissionReqBefore[2] = { 0, 0 };
		BRMission::FState MissionBefore;
		// A. ramassage
		uint64 PickupId = 0;
		EBRItem PickupItem = EBRItem::None;
		int32 CountBefore = 0;
		int32 AcceptedBefore = 0;
		// B. depart
		uint16 DepIdBefore = 0;
		int32 SerialBefore = 0;
		int32 MaxNeeded = 0;
		int32 MaxReady = 0;
		uint16 DepIdSeen = 0;
		// C. morts
		bool bVictim = false;
		bool bWasDead = false;
		int32 Deaths = 0;
		int32 Wakes = 0;
		float DelayAtDeath = -1.f;
		double DeathAt = 0.0;
		double WakeAt = 0.0;
		// v4.16 : {Saut} a terre chez un client (abandon, ServerReportGiveUp) ; aide affichee a la mort ; chutes constatees par
		// l'hote (fosse : a la mort, puis apres le reveil)
		bool bJumpTried = false;
		bool bJumpNoted = false;
		float TimerBeforeJump = -1.f;
		/** v4.16 (fusion) : C3, hote et autres clients a terre au coup mortel du client lointain */
		bool bOthersDownAtHit = false;
		bool bHelpAtDeath = false;
		int32 FallsBefore = 0;
		int32 FallsBySlot[4] = { 0, 0, 0, 0 };
		int32 FallsAtDeath = -1;
		bool bSeenDead = false;
		bool bSeenAwake = false;
		FIntPoint PitCorner = FIntPoint::ZeroValue;
		EBRDeathCause CauseAtDeath = EBRDeathCause::None;
		// D, E. mecanismes loin de l'hote
		int32 Device = -1;
		uint8 Action = 0;
		int32 RevBefore = 0;
		int32 ResultsBefore = 0;
		bool bClientActed = false;
		bool bTickSeen = false;
		// F. entrees
		int32 InPhase = 0;
		double InClock = 0.0;
		TArray<FKey> Keys;
		bool bKeysSaved = false;
		FKey KeyFocus;
		FKey KeyInteract;
		FKey KeySprint;
		int32 InputProblems = 0;
		/** v4.16 (relecture) : le scanner avait un son a jouer pendant le scan (sinon : arret du son NON VERIFIE) */
		bool bScanSound = false;
		// arrivee tardive, reconnexion
		TArray<int32> KnownIds;
		int32 LateId = -1;
		float IntroBefore = -1.f;
		double IntroClock = 0.0;
		uint32 Events = 0;
		uint32 CrcBefore = 0;
		int32 MissionRevBefore = 0;
		uint64 ReturnedId = 0;
		/** reconnexion : le client etait deja revenu (et pret) quand l'objet a ete rendu au monde (cas non reproduit) */
		bool bBackEarly = false;
		/** entrees : repetitions ignorees par le controleur avant l'essai (ABRPlayerController::IgnoredStaleRepeats) */
		int32 RepeatsBefore = 0;
		int32 OrigSlot = INDEX_NONE;
		bool bStorySave = false;
		// reconnexion : etat garde d'un acteur de test a l'autre (meme processus)
		bool bReconnectPending = false;
		int32 ReconnectFromSerial = 0;
		TArray<FString> CarriedNotes;
		TArray<FString> CarriedProblems;
		TArray<FString> CarriedSkips;
	};
	FRun S;

	/** B. Depart de groupe vu par cette machine depuis l'essai (identifiant, joueurs attendus et rassembles au plus). v4.16
	 *  (correctif) : releve a chaque image des le debut des demandes, et plus seulement apres l'instant commun : un depart dont
	 *  tous les joueurs sont rassembles part 1,2 s apres la premiere demande (ABRWorld::UpdateDeparture), et l'hote qui
	 *  s'abstient attendait encore l'instant commun quand celle d'un client l'a cree (net416_4j : depart 1 cree a 20.49.04:633,
	 *  niveau 1 prepare a 05:839, l'hote n'a regarde qu'apres : "0 depart(s) annonce(s)", attendus et rassembles 0) */
	void SeeDeparture(const ABRWorld* W)
	{
		if (!W)
		{
			return;
		}
		const FBRNetDeparture& Dep = W->GetDeparture();
		if (Dep.Id != S.DepIdBefore && Dep.Phase == 1)
		{
			S.DepIdSeen = Dep.Id;
			S.MaxNeeded = FMath::Max<int32>(S.MaxNeeded, Dep.Needed);
			S.MaxReady = FMath::Max<int32>(S.MaxReady, Dep.Ready);
		}
	}

	/** Place ce joueur : chez un client, deplacement recu tel quel par l'hote (BRTestNet::FPlacement) ; chez l'hote, direct */
	void PlaceStart(ABRCharacter* C, const FVector& Stand, const FVector* Face, bool bClient)
	{
		if (!C)
		{
			return;
		}
		if (bClient)
		{
			S.Place.Start(C, Stand, Face, FPlatformTime::Seconds());
			return;
		}
		C->SetActorLocation(Stand, false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* M = C->GetCharacterMovement())
		{
			M->StopMovementImmediately();
		}
		if (AController* Ctrl = Face ? C->GetController() : nullptr)
		{
			Ctrl->SetControlRotation((*Face - C->GetEyeLocation()).Rotation());
		}
	}

	/** La place est tenue (client : recue par l'hote depuis 0,6 s) */
	bool PlaceHeld(ABRCharacter* C, bool bClient)
	{
		return !bClient || S.Place.Update(C, FPlatformTime::Seconds());
	}

	/** v4.16 (relecture) : hote : client reveille au point de depart (spec. : pas d'invulnerabilite restee par erreur, pas de
	 *  chute imputee a une ancienne position). Plus en chargement (les entites ignorent un joueur en chargement : invulnerable de
	 *  fait), un mouvement recu depuis son placement (sinon sa position connue reste celle de sa mort), debout, aucune chute
	 *  constatee depuis FallsBefore */
	bool WokeCleanly(const ABRCharacter* M, int32 FallsBefore, FString& Out)
	{
		if (!M)
		{
			Out = TEXT("personnage absent chez l'hote");
			return false;
		}
		const bool bLoading = M->IsLevelLoading();
		const bool bStale = M->HasStaleServerPosition();
		const bool bDown = M->GetDeathState().bDead;
		const int32 Falls = M->GetServerPitFalls() - FallsBefore;
		Out = FString::Printf(TEXT("en chargement chez l'hote %s, position d'avant le placement %s, a terre %s, chutes constatees depuis %d (attendu 0)"),
			bLoading ? TEXT("OUI") : TEXT("non"), bStale ? TEXT("ENCORE") : TEXT("non"), bDown ? TEXT("OUI") : TEXT("non"), Falls);
		return !bLoading && !bStale && !bDown && Falls == 0;
	}

	/** Suivi d'une mort et d'un reveil remis a zero (essais C et G) */
	void ResetDeathTrack()
	{
		S.bWasDead = false;
		S.Deaths = S.Wakes = 0;
		S.DelayAtDeath = -1.f;
		S.bJumpTried = S.bJumpNoted = false;
		S.bHelpAtDeath = false;
		S.CauseAtDeath = EBRDeathCause::None;
		S.FallsAtDeath = -1;
		S.bSeenDead = S.bSeenAwake = false;
	}

	/** Suivi chez ce joueur : premiere mort (delai, aide affichee, cause) et reveils */
	void TrackDeath(const ABRWorld* W, const ABRCharacter* C, double Now)
	{
		const bool bDead = C->IsDead();
		if (bDead && !S.bWasDead)
		{
			++S.Deaths;
			if (S.Deaths == 1)
			{
				S.DelayAtDeath = W->GetDeathTimer();
				S.DeathAt = Now;
				S.bHelpAtDeath = W->HasLivingTeammate();
				S.CauseAtDeath = C->GetDeathCause();
			}
		}
		if (!bDead && S.bWasDead)
		{
			++S.Wakes;
			S.WakeAt = Now;
		}
		S.bWasDead = bDead;
	}

	/** v4.16 : client a terre : un appui sur {Saut} 1,5 s apres la mort (abandon) ; l'hote doit l'accepter (ServerReportGiveUp) :
	 *  reveil au point de depart dans la seconde, puis toujours debout 2,5 s apres. Avant : reveil signale comme ordinaire,
	 *  refuse par l'hote (fenetre de reanimation), retour a terre au point de depart avec l'equipement de depart. Une seule
	 *  note : vrai quand elle est prete (OutNote, bOutProblem) */
	bool TryGiveUp(ABRWorld* W, ABRCharacter* C, bool bDead, double Now, FString& OutNote, bool& bOutProblem)
	{
		if (!W || !C || S.bJumpNoted)
		{
			return false;
		}
		if (!S.bJumpTried)
		{
			if (bDead && S.Deaths == 1 && Now - S.DeathAt > 1.5)
			{
				S.bJumpTried = true;
				S.TimerBeforeJump = W->GetDeathTimer();
				S.Clock2 = Now;
				C->InputJump(true);
				C->InputJump(false);
			}
			return false;
		}
		const bool bWoke = S.Wakes > 0 && S.WakeAt >= S.Clock2;
		if (!(bWoke && Now - S.WakeAt >= 2.5) && Now - S.Clock2 < 6.0)
		{
			return false;
		}
		S.bJumpNoted = true;
		const double After = bWoke ? S.WakeAt - S.Clock2 : -1.0;
		const bool bUp = bWoke && !C->IsDead();
		OutNote = FString::Printf(TEXT("client a terre : appui sur {Saut} (abandon) %.1f s avant la fin du delai : reveil %.1f s apres l'appui (attendu moins de 1,5 s), "
			"toujours debout 2,5 s apres %s, a terre %d fois (attendu 1 ; v4.15 : reveil refuse par l'hote, retour a terre au point de depart)"),
			S.TimerBeforeJump, After, Oui(bUp), S.Deaths);
		bOutProblem = !bWoke || After > 1.5 || !bUp || S.Deaths != 1;
		return true;
	}

	/** Joueur invincible des tests : sante mentale et endurance tenues (la folie ignore le mode invincible) */
	void Keep(ABRCharacter* C)
	{
		if (C && C->bGodMode)
		{
			C->Sanity = 100.f;
			C->Stamina = 100.f;
		}
	}
}

namespace V6 = BRTestV416Net;

void ABRAutoTest::AddNetV416Plan()
{
	const bool bOnClient = GetNetMode() == NM_Client;
	const V6::FOpts& Opt6 = V6::Opts();
	{
		FLevelReport Rep;
		Rep.Title = bOnClient ? (Opt6.bLate ? TEXT("Cooperation v4.16 (client tardif)") : TEXT("Cooperation v4.16 (client)")) : TEXT("Cooperation v4.16 (hote)");
		Reports.Add(Rep);
	}
	const bool bPhase2 = bOnClient && V6::S.bReconnectPending;
	if (!bPhase2)
	{
		V6::S = V6::FRun();
		V6::GSentMark = 0;
	}
	V6::S.Since = FDateTime::UtcNow() - FTimespan::FromSeconds(5.0);

	// Barriere : chaque client signale qu'il est arrive a l'etape Id ; l'hote l'ouvre quand tous y sont
	auto Barrier = [this, bOnClient](int32 Id, const TCHAR* What)
	{
		Add(FString::Printf(TEXT("v4.16 reseau : %s (barriere %d)"), What, Id), 0.f, [this, bOnClient, Id]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W || !GetPlayer() || !GetPC())
			{
				return false;
			}
			V6::Keep(GetPlayer());
			if (bOnClient)
			{
				V6::Mark(GetPC(), Id);
				if (V6::HostPhase(GetWorld()) >= Id)
				{
					return true;
				}
				if (StepTime > 400.f)
				{
					Note(FString::Printf(TEXT("client : barriere %d jamais ouverte par l'hote (etape de l'hote %d)"), Id, V6::HostPhase(GetWorld())), true);
					return true;
				}
				return false;
			}
			int32 Behind = 0;
			for (const V6::FMate& M : V6::Mates(W))
			{
				Behind += M.Mark < Id ? 1 : 0;
			}
			if (Behind > 0 && StepTime < 380.f)
			{
				return false;
			}
			if (Behind > 0)
			{
				Note(FString::Printf(TEXT("hote : %d client(s) n'ont pas atteint la barriere %d en 380 s : etape suivante quand meme"), Behind, Id), true);
			}
			V6::SetHostPhase(GetWorld(), Id);
			return true;
		});
		LimitLastStep(420.f);
	};

	// Changement de niveau demande par l'hote (graine utilisateur, parcours classique impose) ; chacun attend d'y etre
	auto GoLevel = [this, bOnClient](int32 Level, uint32 UserSeed, const TCHAR* What)
	{
		Add(FString::Printf(TEXT("v4.16 reseau : %s, Niveau %d"), What, Level), 2.f, [this, bOnClient, Level, UserSeed]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			const uint32 Seed = ABRWorld::SeedFromUser(UserSeed, Level);
			if (!bOnClient && V6::S.Phase != 900 + Level)
			{
				if (W->IsTransitioning())
				{
					return false;
				}
				V6::S.Phase = 900 + Level;
				if (W->GetLevelNumber() != Level || W->GetSeed() != Seed)
				{
					W->SetLinearTestOverride(0);
					W->RequestTransition(Level, false, Seed);
					return false;
				}
			}
			const bool bThere = !W->IsTransitioning() && W->IsLevelReady() && W->GetLevelNumber() == Level && W->GetSeed() == Seed;
			if (!bThere && StepTime < 150.f)
			{
				return false;
			}
			V6::S.Phase = 0;
			C->bGodMode = true;
			V6::Keep(C);
			Note(FString::Printf(TEXT("%s : Niveau %d, graine %u (attendue %u) %s"), bOnClient ? TEXT("client") : TEXT("hote"), W->GetLevelNumber(), W->GetSeed(), Seed,
				bThere ? TEXT("charge") : TEXT("NON CHARGE en 150 s")), !bThere);
			V6::S.bSkipBlock = !bThere;
			return true;
		});
		LimitLastStep(170.f);
	};

	// ================================================================================================= reconnexion : 2e partie
	if (bPhase2)
	{
		Add(TEXT("v4.16 reconnexion : etat d'avant la reconnexion"), 0.f, [this]()
		{
			for (const FString& T : V6::S.CarriedNotes)
			{
				Note(TEXT("(avant la reconnexion) ") + T);
			}
			for (const FString& T : V6::S.CarriedProblems)
			{
				Note(TEXT("(avant la reconnexion) ") + T, true);
			}
			for (const FString& T : V6::S.CarriedSkips)
			{
				Skip(TEXT("(avant la reconnexion) ") + T);
			}
			V6::S.CarriedNotes.Reset();
			V6::S.CarriedProblems.Reset();
			V6::S.CarriedSkips.Reset();
			V6::S.bReconnectPending = false;
			V6::GSentMark = 0;
			return true;
		});
		Add(TEXT("v4.16 reconnexion : de retour dans la partie"), 3.f, [this]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			ABRPlayerController* PC = GetPC();
			if (!W || !C || !PC || !W->IsLevelReady() || W->IsTransitioning())
			{
				if (StepTime > 180.f)
				{
					Note(TEXT("client reconnecte : niveau du groupe pas construit en 180 s"), true);
					return true;
				}
				return false;
			}
			C->bGodMode = true;
			V6::Keep(C);
			return true;
		});
		LimitLastStep(200.f);
		Add(TEXT("v4.16 reconnexion : entrees, chargement, objet rendu"), 0.f, [this]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			const bool bInput = !C->IsInputLocked();
			const bool bLoading = C->IsLevelLoading();
			Note(FString::Printf(TEXT("client reconnecte : Niveau %d (graine %u, chargement %d ; avant : %d), entrees deverrouillees %s, chargement termine chez lui %s, a terre %s"),
				W->GetLevelNumber(), W->GetSeed(), W->GetLevelSerial(), V6::S.ReconnectFromSerial, V6::Oui(bInput), V6::Oui(!bLoading), C->IsDead() ? TEXT("OUI") : TEXT("non")),
				!bInput || bLoading || C->IsDead());
			// v4.16 : personnage recu apres la fin du fondu d'arrivee (latence, pertes) : chemin de ABRWorld::Tick emprunte ici ?
			Note(FString::Printf(TEXT("client reconnecte : personnage recu apres la fin de l'arrivee (chemin v4.16) %s"),
				LogContains(0, TEXT("Personnage place apres la fin de l'arrivee")) ? TEXT("oui") : TEXT("non (personnage recu pendant l'arrivee)")));
			const TArray<FBRReturnedPickup> Returned = W->GetReturnedList();
			if (Returned.Num() == 0)
			{
				Skip(TEXT("client reconnecte : aucun objet rendu au monde chez l'hote : affichage apres la reconnexion non juge"));
			}
			else
			{
				int32 Present = 0;
				for (const FBRReturnedPickup& R : Returned)
				{
					Present += V6::PickupById(GetWorld(), R.Id) ? 1 : 0;
				}
				Note(FString::Printf(TEXT("client reconnecte : objets rendus au monde visibles %d/%d (attendu tous ; v4.15 : 0, poses seulement quand la liste changeait)"), Present,
					Returned.Num()), Present != Returned.Num());
			}
			// Comparaison avec l'hote : meme niveau, meme graine, meme chargement apres la reconnexion (le dossier de ce client a
			// ete vide a son retour : ses valeurs d'avant n'existent plus)
			V6::Put(OutDir, TEXT("reconnexion"), TEXT("niveau"), FString::Printf(TEXT("%d/%u/%d"), W->GetLevelNumber(), W->GetSeed(), W->GetLevelSerial()), TEXT("egal"));
			V6::Mark(GetPC(), 85);
			V6::S.Clock = FPlatformTime::Seconds();
			return true;
		});
		Add(TEXT("v4.16 reconnexion : quelques pas"), 0.f, [this]()
		{
			ABRCharacter* C = GetPlayer();
			if (!C)
			{
				return false;
			}
			// De vrais mouvements partent a l'hote (sa position connue cesse d'etre celle d'avant le placement)
			C->AddMovementInput(C->GetActorForwardVector(), 1.f);
			if (StepTime < 1.5f)
			{
				return false;
			}
			V6::Mark(GetPC(), 86);
			return true;
		});
		Add(TEXT("v4.16 reconnexion : fin"), 0.f, [this]()
		{
			if (V6::HostPhase(GetWorld()) < 87 && StepTime < 90.f)
			{
				return false;
			}
			V6::Mark(GetPC(), V6::MarkFinished);
			Finish();
			return true;
		});
		LimitLastStep(100.f);
		return;
	}

	// ================================================================================================= debut commun
	Add(TEXT("v4.16 reseau : attente des joueurs"), 2.f, [this, bOnClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		ABRCharacter* C = GetPlayer();
		if (!W || !PC || !C || !W->IsLevelReady() || W->IsTransitioning() || !PC->IsNetGame())
		{
			return false;
		}
		C->bGodMode = true;
		V6::Keep(C);
		if (bOnClient)
		{
			return true;
		}
		const int32 Want = V6::Opts().Clients;
		const TArray<V6::FMate> M = V6::Mates(W);
		int32 Loading = 0;
		for (const V6::FMate& X : M)
		{
			Loading += X.Pawn->IsLevelLoading() ? 1 : 0;
		}
		if ((M.Num() < Want || Loading > 0) && StepTime < 90.f + 90.f * Want)
		{
			return false;
		}
		Note(FString::Printf(TEXT("hote : %d client(s) present(s) (attendu %d), %d encore en chargement : %d joueur(s)"), M.Num(), Want, Loading, M.Num() + 1),
			M.Num() < Want || Loading > 0);
		if (Want >= 3 && M.Num() >= 3)
		{
			Note(TEXT("v4.16 : quatre joueurs (hote et trois clients) sur ce PC"));
		}
		return true;
	});
	LimitLastStep(400.f);
	Add(TEXT("v4.16 reseau : etat et reseau simule"), 0.5f, [this, bOnClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC)
		{
			return false;
		}
		V6::S.Slot = V6::MySlot(W, PC);
		const AGameStateBase* GS = GetWorld()->GetGameState();
		const TCHAR* Who = bOnClient ? (V6::Opts().bLate ? TEXT("client tardif") : TEXT("client")) : TEXT("hote");
		Note(FString::Printf(TEXT("%s (place %d) : niveau %d, graine %u, chargement %d, %d joueur(s), rendu %s, fenetre active %s"), Who, V6::S.Slot, W->GetLevelNumber(),
			W->GetSeed(), W->GetLevelSerial(), GS ? GS->PlayerArray.Num() : 0, FApp::CanEverRender() ? TEXT("oui") : TEXT("aucun (-nullrhi)"), V6::Oui(V6::AppActive())));
		int32 WantLag = 0;
		int32 WantVar = 0;
		int32 WantLoss = 0;
		const bool bLag = FParse::Value(FCommandLine::Get(), TEXT("PktLag="), WantLag);
		const bool bVar = FParse::Value(FCommandLine::Get(), TEXT("PktLagVariance="), WantVar);
		const bool bLoss = FParse::Value(FCommandLine::Get(), TEXT("PktLoss="), WantLoss);
#if DO_ENABLE_NET_TEST
		if (const UNetDriver* Driver = GetWorld()->GetNetDriver())
		{
			const FPacketSimulationSettings& P = Driver->PacketSimulationSettings;
			const bool bSame = P.PktLag == WantLag && P.PktLagVariance == WantVar && P.PktLoss == WantLoss;
			Note(FString::Printf(TEXT("%s : reseau simule du moteur : latence %d ms, gigue +-%d ms, pertes %d %% a l'envoi (demande : %d, %d, %d)"), Who, P.PktLag, P.PktLagVariance,
				P.PktLoss, WantLag, WantVar, WantLoss));
			if ((bLag || bVar || bLoss) && !bSame)
			{
				Skip(FString::Printf(TEXT("%s : reseau simule demande sur la ligne de commande mais pas applique par le pilote reseau : essais sous latence NON VERIFIES"), Who));
			}
		}
		else
		{
			Skip(TEXT("pilote reseau absent : reseau simule non lu"));
		}
#else
		Skip(TEXT("reseau simule indisponible dans cette compilation (DO_ENABLE_NET_TEST) : essais sous latence NON VERIFIES"));
		(void)bLag;
		(void)bVar;
		(void)bLoss;
#endif
		// v4.16 (relecture) : le client tardif arrive ou en est le groupe (Level 6 pour lidar et passage), pas au Level 0 du debut
		// des autres : ses valeurs propres sont comparees plus loin (lidar|..., porte|emplacement). Avant : DIFFERENT, donc un
		// PROBLEME, dans les essais lidar et passage meme quand le jeu etait correct
		if (!V6::Opts().bLate)
		{
			V6::Put(OutDir, TEXT("debut"), TEXT("niveau"), FString::Printf(TEXT("%d/%u"), W->GetLevelNumber(), W->GetSeed()), TEXT("egal"));
		}
		return true;
	});

	// v4.17 (prompt § 4.1) : combinaison d'un coequipier vue ici (l'hote voit un client, un client voit l'hote) : materiaux de
	// ses slots de combinaison sur le personnage replique (M_BR_Gear attendu), puis capture de face a 2,3 m
	if (Opt6.Late.IsEmpty())
	{
		Add(TEXT("v4.17 reseau : coequipier vu ici, materiaux"), 1.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			ABRPlayerController* PC = GetPC();
			if (!W || !C || !PC)
			{
				return false;
			}
			const TCHAR* Who = bOnClient ? TEXT("client") : TEXT("hote");
			const TArray<V6::FMate> M = V6::Mates(W);
			ABRCharacter* Mate = M.Num() > 0 ? M[0].Pawn : nullptr;
			if (!Mate)
			{
				Skip(FString::Printf(TEXT("%s : aucun coequipier replique ici : combinaison d'un coequipier NON VERIFIEE"), Who));
				return true;
			}
			int32 Suit = 0;
			int32 Gear = 0;
			FString Others;
			TInlineComponentArray<UMeshComponent*> Meshes;
			Mate->GetComponents(Meshes);
			for (UMeshComponent* Mesh : Meshes)
			{
				if (!Mesh || !Mesh->IsVisible())
				{
					continue;
				}
				const TArray<FName> Slots = Mesh->GetMaterialSlotNames();
				for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
				{
					const FString Slot = Slots.IsValidIndex(i) ? Slots[i].ToString() : FString();
					if (!Slot.Contains(TEXT("Hazmat")))
					{
						continue;
					}
					++Suit;
					UMaterialInterface* Mat = Mesh->GetMaterial(i);
					const UMaterial* Base = Mat ? Mat->GetBaseMaterial() : nullptr;
					const FString BaseName = Base ? Base->GetName() : FString(TEXT("aucun"));
					if (BaseName.StartsWith(TEXT("M_BR_Gear")))
					{
						++Gear;
					}
					else
					{
						Others += FString::Printf(TEXT(" %s=%s"), *Slot, *BaseName);
					}
				}
			}
			Note(FString::Printf(TEXT("%s : combinaison du coequipier (place %d) : %d slot(s), dont %d sur M_BR_Gear%s"), Who, M[0].Slot, Suit, Gear,
				Others.IsEmpty() ? TEXT("") : *(TEXT(" ; autres :") + Others)), Suit == 0 || Gear != Suit);
			if (!FApp::CanEverRender())
			{
				Skip(FString::Printf(TEXT("%s : sans rendu (-nullrhi) : capture du coequipier NON FAITE"), Who));
				return true;
			}
			// Devant lui, sinon a sa droite, a sa gauche ou derriere : le premier cote sans mur a 2,3 m
			const FVector Chest = Mate->GetActorLocation() + FVector(0.f, 0.f, 20.f);
			const FVector Fwd = FRotator(0.f, Mate->GetActorRotation().Yaw, 0.f).Vector();
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
			FVector Loc = Chest + Fwd * 230.f;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(BRV417MateCam), false, Mate);
			Q.AddIgnoredActor(C);
			for (const FVector& Dir : { Fwd, Right, -Right, -Fwd })
			{
				FHitResult Hit;
				if (!GetWorld()->LineTraceSingleByChannel(Hit, Chest, Chest + Dir * 230.f, ECC_Visibility, Q))
				{
					Loc = Chest + Dir * 230.f;
					break;
				}
			}
			const FRotator Rot = (Chest - Loc).Rotation();
			ACameraActor* Cam = V6::MateCam.Get();
			if (!Cam)
			{
				Cam = GetWorld()->SpawnActor<ACameraActor>(Loc, Rot);
				V6::MateCam = Cam;
			}
			if (Cam)
			{
				Cam->SetActorLocationAndRotation(Loc, Rot);
				if (UCameraComponent* Mine = Cam->GetCameraComponent())
				{
					Mine->SetFieldOfView(70.f);
					if (C->Camera)
					{
						Mine->PostProcessSettings = C->Camera->PostProcessSettings;
						Mine->PostProcessBlendWeight = C->Camera->PostProcessBlendWeight;
					}
					Mine->bConstrainAspectRatio = false;
				}
				PC->SetViewTarget(Cam);
			}
			if (ABRHUD* H = Cast<ABRHUD>(PC->GetHUD()))
			{
				H->TestCaptureMode = 2; // scene seule
			}
			return true;
		});
		Add(TEXT("v4.17 reseau : coequipier vu ici, capture"), 1.5f, [this, bOnClient]()
		{
			if (V6::MateCam.IsValid())
			{
				Shot(bOnClient ? TEXT("v417_coequipier_vu_par_client") : TEXT("v417_coequipier_vu_par_hote"));
			}
			return true;
		});
		Add(TEXT("v4.17 reseau : coequipier vu ici, retour"), 0.5f, [this]()
		{
			if (ABRPlayerController* PC = GetPC())
			{
				if (ABRCharacter* C = GetPlayer())
				{
					PC->SetViewTarget(C);
				}
				if (ABRHUD* H = Cast<ABRHUD>(PC->GetHUD()))
				{
					H->TestCaptureMode = 0;
				}
			}
			if (ACameraActor* Cam = V6::MateCam.Get())
			{
				Cam->Destroy();
			}
			V6::MateCam.Reset();
			return true;
		});
	}

	const FString Late = Opt6.Late;
	if (Late.IsEmpty())
	{
		// ============================================================================================= A. ramassage simultane
		Barrier(10, TEXT("ramassage simultane"));
		Add(TEXT("v4.16 reseau : ramassage simultane, places"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			ABRPickup* P = V6::SharedPickup(GetWorld(), W);
			if (!P && StepTime < 10.f)
			{
				return false;
			}
			V6::S.PickupId = P ? P->Id : 0;
			if (!P)
			{
				Skip(TEXT("ramassage simultane : aucun objet commun a moins de 30 m du depart"));
				return true;
			}
			V6::S.PickupItem = P->Item;
			V6::S.CountBefore = C->CountItem(P->Item);
			V6::S.AcceptedBefore = C->ServerPickupsAccepted;
			if (!bOnClient)
			{
				for (const V6::FMate& M : V6::Mates(W))
				{
					V6::S.AcceptedBefore += M.Pawn->ServerPickupsAccepted;
				}
			}
			FVector Stand;
			if (!V6::AroundSpot(W, C, P->GetActorLocation(), V6::S.Slot, Stand))
			{
				Stand = P->GetActorLocation() + FVector(60.f, 0.f, C->GetSimpleCollisionHalfHeight() + 5.f);
			}
			const FVector Aim = P->GetActorLocation();
			V6::PlaceStart(C, Stand, &Aim, bOnClient);
			return true;
		});
		Add(TEXT("v4.16 reseau : ramassage simultane, chacun en place"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			if (V6::S.PickupId != 0 && !V6::PlaceHeld(C, bOnClient) && StepTime < 10.f)
			{
				return false;
			}
			if (bOnClient)
			{
				V6::Mark(GetPC(), 11);
				if (V6::HostPhase(GetWorld()) < 11 && StepTime < 60.f)
				{
					return false;
				}
				V6::S.SyncAt = V6::SyncTarget(GetWorld()); // v4.16 (relecture) : instant commun des demandes
				return true;
			}
			const TArray<V6::FMate> M = V6::Mates(W);
			for (const V6::FMate& X : M)
			{
				if (X.Mark < 11 && StepTime < 50.f)
				{
					return false;
				}
			}
			// v4.16 (relecture) : a partir de deux clients, eux seuls demandent (au meme instant) ; l'hote compte les demandes recues
			V6::S.bRace = M.Num() >= 2;
			if (V6::S.bRace && !V6::SyncOpenNow(GetWorld()) && StepTime < 62.f)
			{
				return false;
			}
			V6::S.RequestsBefore = 0;
			for (const V6::FMate& X : M)
			{
				V6::S.RequestsBefore += X.Pawn->ServerPickupsAccepted + X.Pawn->ServerPickupsRefused;
				X.Pawn->ServerLastRequestFrame = 0;
			}
			V6::SetHostPhase(GetWorld(), 11);
			V6::S.SyncAt = V6::SyncTarget(GetWorld());
			return true;
		});
		LimitLastStep(70.f);
		Add(TEXT("v4.16 reseau : ramassage simultane, demandes"), 0.f, [this, bOnClient]()
		{
			ABRCharacter* C = GetPlayer();
			if (!C || V6::S.PickupId == 0)
			{
				return true;
			}
			if (V6::ServerNow(GetWorld()) < V6::S.SyncAt && StepTime < 15.f)
			{
				return false;
			}
			if (!bOnClient && V6::S.bRace)
			{
				return true; // l'hote s'abstient : sa demande serait appliquee sur place avant celles des clients
			}
			if (ABRPickup* P = V6::PickupById(GetWorld(), V6::S.PickupId))
			{
				C->RequestPickup(P);
			}
			else
			{
				Note(TEXT("ramassage simultane : l'objet avait deja disparu chez cette machine avant sa demande"));
			}
			return true;
		});
		LimitLastStep(20.f);
		Add(TEXT("v4.16 reseau : ramassage simultane, un seul attributaire"), 1.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C || V6::S.PickupId == 0)
			{
				return true;
			}
			if (C->IsPickupPending() && StepTime < 10.f)
			{
				return false;
			}
			if (!bOnClient && StepTime < 2.f)
			{
				return false; // les demandes des clients arrivent
			}
			if (!bOnClient && V6::S.bRace && StepTime < 8.f)
			{
				// v4.16 (relecture) : chaque client a demande (latence et pertes simulees : renvois)
				int32 Got = -V6::S.RequestsBefore;
				int32 Want = 0;
				for (const V6::FMate& M : V6::Mates(W))
				{
					Got += M.Pawn->ServerPickupsAccepted + M.Pawn->ServerPickupsRefused;
					++Want;
				}
				if (Got < Want)
				{
					return false;
				}
			}
			const int32 Gain = C->CountItem(V6::S.PickupItem) > V6::S.CountBefore ? 1 : 0;
			V6::Put(OutDir, TEXT("ramassage"), TEXT("gagnant"), FString::FromInt(Gain), TEXT("somme=1"));
			if (bOnClient)
			{
				Note(FString::Printf(TEXT("client : ramassage simultane, objet recu %s"), V6::Oui(Gain > 0)));
				return true;
			}
			int32 Accepted = C->ServerPickupsAccepted;
			int32 Players = 1;
			int32 Requests = -V6::S.RequestsBefore;
			TArray<const ABRCharacter*> Who;
			for (const V6::FMate& M : V6::Mates(W))
			{
				Accepted += M.Pawn->ServerPickupsAccepted;
				Requests += M.Pawn->ServerPickupsAccepted + M.Pawn->ServerPickupsRefused;
				Who.Add(M.Pawn);
				++Players;
			}
			Accepted -= V6::S.AcceptedBefore;
			Note(FString::Printf(TEXT("hote : ramassage simultane a %d joueurs : %d attribution(s) (attendu 1), objet retire pour tous %s"), Players, Accepted,
				V6::Oui(W->IsCollected(V6::S.PickupId))), Accepted != 1 || !W->IsCollected(V6::S.PickupId));
			// v4.16 (relecture) : la concurrence n'est reelle qu'entre clients (la demande de l'hote est appliquee sur place, avant)
			if (!V6::S.bRace)
			{
				Skip(TEXT("ramassage simultane : demandes en concurrence NON VERIFIEES (moins de 2 clients : la demande de l'hote, appliquee sur place, passe toujours la premiere)"));
			}
			else
			{
				int32 With = 0;
				const FString Spread = V6::FrameSpread(Who, With);
				Note(FString::Printf(TEXT("hote : ramassage simultane : %d demande(s) de clients recues au meme instant du serveur (attendu %d, l'hote s'abstient) : %s"), Requests,
					Players - 1, *Spread));
				if (Requests < 2)
				{
					Skip(TEXT("ramassage simultane : moins de deux demandes de clients recues par l'hote : concurrence NON VERIFIEE"));
				}
			}
			return true;
		});
		LimitLastStep(20.f);

		// ============================================================================================= B. depart simultane
		Barrier(20, TEXT("depart de groupe simultane"));
		Add(TEXT("v4.16 reseau : depart simultane, devant la porte de service"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			if (V6::S.Phase == 0)
			{
				V6::S.bSkipBlock = false;
				if (W->GetLevelNumber() != 0 || !W->HasServiceDoor())
				{
					Skip(TEXT("depart simultane : groupe hors du Level 0 ou sans porte de service"));
					V6::S.bSkipBlock = true;
					return true;
				}
				// Le chunk de la porte se construit autour du joueur : on s'en approche en vol
				const FVector Door = W->GetServiceDoorLocation();
				const FVector In = FVector(-W->GetServiceDoorDir().X, -W->GetServiceDoorDir().Y, 0.f);
				V6::S.Spot = Door + In * 300.f + FVector(0.f, 0.f, 150.f);
				V6::Hover(C, true);
				C->SetActorLocation(V6::S.Spot, false, nullptr, ETeleportType::TeleportPhysics);
				V6::S.Phase = 1;
				return false;
			}
			ABRExit* Door = V6::FindServiceDoor(GetWorld());
			if (V6::S.Phase == 1)
			{
				if (!Door && StepTime < 30.f)
				{
					return false;
				}
				if (!Door)
				{
					V6::Hover(C, false);
					Skip(TEXT("depart simultane : porte de service non construite en 30 s"));
					V6::S.bSkipBlock = true;
					V6::S.Phase = 0;
					return true;
				}
				const FVector Fwd = Door->GetActorForwardVector();
				const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();
				const FVector Base = Door->GetActorLocation() + Fwd * 150.f + Right * ((static_cast<float>(V6::S.Slot) - 1.5f) * 55.f);
				float Z = 0.f;
				if (!V6::FloorAt(W, C, Base, Door->GetActorLocation().Z, Z))
				{
					Z = Door->GetActorLocation().Z;
				}
				V6::Hover(C, false);
				const FVector Face = Door->GetActorLocation() + FVector(0.f, 0.f, 110.f);
				V6::PlaceStart(C, FVector(Base.X, Base.Y, Z + C->GetSimpleCollisionHalfHeight() + 5.f), &Face, bOnClient);
				V6::S.Phase = 2;
				return false;
			}
			if (!V6::PlaceHeld(C, bOnClient) && StepTime < 40.f)
			{
				return false;
			}
			V6::S.Phase = 0;
			V6::S.SerialBefore = W->GetLevelSerial();
			V6::S.DepIdBefore = W->GetDeparture().Id;
			V6::S.DepIdSeen = V6::S.DepIdBefore;
			return true;
		});
		LimitLastStep(60.f);
		Add(TEXT("v4.16 reseau : depart simultane, tous en place"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W)
			{
				return false;
			}
			if (bOnClient)
			{
				V6::Mark(GetPC(), 21);
				if (V6::HostPhase(GetWorld()) < 21 && StepTime < 90.f)
				{
					return false;
				}
				V6::S.SyncAt = V6::SyncTarget(GetWorld()); // v4.16 (relecture) : instant commun des demandes
				return true;
			}
			const TArray<V6::FMate> M = V6::Mates(W);
			for (const V6::FMate& X : M)
			{
				if (X.Mark < 21 && StepTime < 80.f)
				{
					return false;
				}
			}
			// v4.16 (relecture) : a partir de deux clients, eux seuls prennent la porte (au meme instant)
			V6::S.bRace = M.Num() >= 2;
			if (V6::S.bRace && !V6::SyncOpenNow(GetWorld()) && StepTime < 92.f)
			{
				return false;
			}
			V6::S.RequestsBefore = 0;
			for (const V6::FMate& X : M)
			{
				V6::S.RequestsBefore += X.Pawn->ServerDepartureRequests;
				X.Pawn->ServerLastRequestFrame = 0;
			}
			V6::S.MaxNeeded = V6::S.MaxReady = 0;
			V6::SetHostPhase(GetWorld(), 21);
			V6::S.SyncAt = V6::SyncTarget(GetWorld());
			return true;
		});
		LimitLastStep(100.f);
		Add(TEXT("v4.16 reseau : depart simultane, demandes"), 0.f, [this, bOnClient]()
		{
			ABRCharacter* C = GetPlayer();
			ABRExit* Door = V6::FindServiceDoor(GetWorld());
			// v4.16 (correctif) : le depart cree par la demande d'un autre joueur est releve pendant l'attente de l'instant commun
			V6::SeeDeparture(GetBRWorld());
			if (V6::S.bSkipBlock || !C || !Door)
			{
				return true;
			}
			if (V6::ServerNow(GetWorld()) < V6::S.SyncAt && StepTime < 15.f)
			{
				return false;
			}
			V6::S.Clock = FPlatformTime::Seconds();
			if (!bOnClient && V6::S.bRace)
			{
				return true; // l'hote s'abstient : sa demande creerait le depart sur place, avant celles des clients
			}
			// Chacun prend la porte au meme moment (touche d'interaction : ABRExit::Use, la demande ordinaire)
			Door->Use(C);
			return true;
		});
		LimitLastStep(20.f);
		Add(TEXT("v4.16 reseau : depart simultane, un seul depart"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W || V6::S.bSkipBlock)
			{
				return true;
			}
			V6::SeeDeparture(W);
			const bool bArrived = !W->IsTransitioning() && W->IsLevelReady() && W->GetLevelSerial() != V6::S.SerialBefore;
			if (!bArrived && StepTime < 60.f)
			{
				return false;
			}
			if (bArrived && StepTime < 4.f)
			{
				return false; // un second depart aurait le temps de partir
			}
			const TCHAR* Who = bOnClient ? TEXT("client") : TEXT("hote");
			const int32 Loads = W->GetLevelSerial() - V6::S.SerialBefore;
			Note(FString::Printf(TEXT("%s : depart simultane : niveau %d, %d chargement(s) (attendu 1), depart %d (avant %d)"), Who, W->GetLevelNumber(), Loads,
				static_cast<int32>(V6::S.DepIdSeen), static_cast<int32>(V6::S.DepIdBefore)), Loads != 1 || W->GetLevelNumber() != 1);
			V6::Put(OutDir, TEXT("depart"), TEXT("niveau"), FString::Printf(TEXT("%d/%u"), W->GetLevelNumber(), W->GetSeed()), TEXT("egal"));
			if (!bOnClient)
			{
				const TArray<V6::FMate> M = V6::Mates(W);
				const int32 Players = M.Num() + 1;
				const int32 Departures = static_cast<int32>(static_cast<uint16>(V6::S.DepIdSeen - V6::S.DepIdBefore));
				Note(FString::Printf(TEXT("hote : %d joueur(s) devant la porte, %d depart(s) annonce(s) (attendu 1), joueurs attendus au plus %d (attendu %d), rassembles au plus %d"),
					Players, Departures, V6::S.MaxNeeded, Players, V6::S.MaxReady), Departures != 1 || V6::S.MaxNeeded != Players || V6::S.MaxReady != Players);
				// v4.16 (relecture) : la concurrence n'est reelle qu'entre clients (la porte prise par l'hote cree le depart sur place)
				if (!V6::S.bRace)
				{
					Skip(TEXT("depart simultane : demandes en concurrence NON VERIFIEES (moins de 2 clients : la demande de l'hote, appliquee sur place, passe toujours la premiere)"));
				}
				else
				{
					int32 Requests = -V6::S.RequestsBefore;
					TArray<const ABRCharacter*> Askers;
					for (const V6::FMate& X : M)
					{
						Requests += X.Pawn->ServerDepartureRequests;
						Askers.Add(X.Pawn);
					}
					int32 With = 0;
					const FString Spread = V6::FrameSpread(Askers, With);
					Note(FString::Printf(TEXT("hote : depart simultane : %d demande(s) de clients recues au meme instant du serveur (attendu %d, l'hote s'abstient) : %s"), Requests,
						M.Num(), *Spread));
					if (Requests < 2)
					{
						Skip(TEXT("depart simultane : moins de deux demandes de clients recues par l'hote : concurrence NON VERIFIEE"));
					}
				}
			}
			return true;
		});
		LimitLastStep(70.f);

		// ============================================================================================= C. morts et reveils
		Barrier(30, TEXT("morts et reveils"));
		GoLevel(37, V6::PoolSeed, TEXT("morts et reveils"));
		// v4.16 (relecture) : eloignement du client de la place 1 a 230-300 m (essais C1 et C3). ReadyMark : en place (mode
		// invincible quitte), SkipMark : essai impossible ; bAllMortal : les autres clients quittent aussi le mode invincible
		auto GoFar = [this, bOnClient](int32 ReadyMark, int32 SkipMark, bool bAllMortal, const TCHAR* What)
		{
			Add(FString::Printf(TEXT("v4.16 reseau : %s, eloignement"), What), 0.f, [this, bOnClient, ReadyMark, SkipMark, bAllMortal, What]()
			{
				ABRWorld* W = GetBRWorld();
				ABRCharacter* C = GetPlayer();
				if (!W || !C)
				{
					return false;
				}
				if (V6::S.Phase == 0)
				{
					V6::ResetDeathTrack();
				}
				V6::S.bVictim = bOnClient && V6::S.Slot == 1 && !V6::S.bSkipBlock;
				if (!bOnClient)
				{
					V6::FMate Victim;
					if (!V6::MateAt(W, 1, Victim))
					{
						Skip(FString::Printf(TEXT("%s : aucun client (au moins un client requis)"), What));
						V6::S.bSkipBlock = true;
					}
					return true;
				}
				if (!V6::S.bVictim)
				{
					if (bAllMortal)
					{
						C->bGodMode = false;
						V6::Mark(GetPC(), ReadyMark);
					}
					return true;
				}
				if (V6::S.Phase == 0)
				{
					const FVector Base = C->GetActorLocation();
					for (const float Dist : { 23000.f, 26000.f, 30000.f })
					{
						for (const float Yaw : { 0.f, 90.f, 180.f, 270.f })
						{
							const FIntPoint Cell = W->WorldToCell(Base + FRotator(0.f, Yaw, 0.f).Vector() * Dist);
							if (V6::S.Phase == 0 && W->IsWalkable(Cell) && !W->IsPoolCell(Cell.X, Cell.Y))
							{
								V6::S.Spot = W->CellCenter(Cell, Base.Z + 200.f);
								V6::S.Phase = 1;
							}
						}
					}
					if (V6::S.Phase == 0)
					{
						Skip(FString::Printf(TEXT("%s : aucune cellule marchable a 230-300 m"), What));
						V6::Mark(GetPC(), SkipMark);
						V6::S.bVictim = false;
						return true;
					}
					V6::Hover(C, true);
					BRTestNet::TeleportForServer(C, V6::S.Spot);
					V6::Hover(C, true);
					V6::S.Clock = FPlatformTime::Seconds();
					return false;
				}
				if (V6::S.Phase == 1)
				{
					float Z = 0.f;
					const FIntPoint Cell = W->WorldToCell(V6::S.Spot);
					if (!W->IsChunkLoaded(W->CellToChunk(Cell)) || !V6::FloorAt(W, C, V6::S.Spot, W->FloorZAt(V6::S.Spot), Z))
					{
						if (FPlatformTime::Seconds() - V6::S.Clock < 40.0)
						{
							return false;
						}
						V6::Hover(C, false);
						Skip(FString::Printf(TEXT("%s : sol lointain pas construit en 40 s"), What));
						V6::Mark(GetPC(), SkipMark);
						V6::S.bVictim = false;
						V6::S.Phase = 0;
						return true;
					}
					V6::Hover(C, false);
					V6::S.Place.Start(C, FVector(V6::S.Spot.X, V6::S.Spot.Y, Z + C->GetSimpleCollisionHalfHeight() + 5.f), nullptr, FPlatformTime::Seconds());
					V6::S.Phase = 2;
					V6::S.Clock = FPlatformTime::Seconds();
					return false;
				}
				if (V6::S.Phase == 2)
				{
					if (!V6::S.Place.Update(C, FPlatformTime::Seconds()) && FPlatformTime::Seconds() - V6::S.Clock < 10.0)
					{
						return false;
					}
					V6::S.Phase = 3;
					V6::S.Clock = FPlatformTime::Seconds();
					return false;
				}
				// v4.16 (relecture) : les personnages des coequipiers restent repliques ici a toute distance (ABRCharacter::IsNetRelevantFor ;
				// avant : plus rien au-dela de 150 m, apres le delai de pertinence du moteur, 5 s). Attente de 8 s, puis compte
				if (FPlatformTime::Seconds() - V6::S.Clock < 8.0)
				{
					return false;
				}
				TArray<ABRCharacter*> Seen;
				W->GetPlayers(Seen);
				V6::S.Phase = 0;
				const AGameStateBase* GS = GetWorld()->GetGameState();
				const int32 Others = GS ? GS->PlayerArray.Num() - 1 : 0;
				float Nearest = -1.f;
				for (const ABRCharacter* X : Seen)
				{
					if (X != C)
					{
						const float D = static_cast<float>(FVector::Dist2D(X->GetActorLocation(), C->GetActorLocation())) / 100.f;
						Nearest = Nearest < 0.f ? D : FMath::Min(Nearest, D);
					}
				}
				Note(FString::Printf(TEXT("client (%s) : a %.0f m du depart, coequipier le plus proche a %.0f m : personnages des coequipiers toujours repliques ici %d/%d (attendu tous ; v4.15 : aucun au-dela de 150 m)"),
					What, FVector::Dist2D(C->GetActorLocation(), W->CellCenter(FIntPoint(0, 0))) / 100.f, Nearest, Seen.Num() - 1, Others), Seen.Num() - 1 < Others);
				if (Nearest >= 0.f && Nearest < 150.f)
				{
					Skip(FString::Printf(TEXT("%s : coequipier le plus proche a %.0f m (moins de 150 m) : replication a distance NON VERIFIEE"), What, Nearest));
				}
				C->bGodMode = false;
				V6::ResetDeathTrack();
				V6::Mark(GetPC(), ReadyMark);
				return true;
			});
			LimitLastStep(120.f);
		};
		// C1 : le client de la place 1 s'eloigne a plus de 150 m (v4.15 : les autres personnages n'etaient plus repliques chez lui ;
		// v4.16, relecture : ils le restent), meurt, coequipiers debout
		GoFar(31, 32, false, TEXT("mort loin des coequipiers"));
		Add(TEXT("v4.16 reseau : mort loin des coequipiers, coup de l'hote"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W || bOnClient || V6::S.bSkipBlock)
			{
				return true;
			}
			V6::FMate Victim;
			if (!V6::MateAt(W, 1, Victim))
			{
				V6::S.bSkipBlock = true;
				return true;
			}
			if (Victim.Mark < 31 && StepTime < 110.f)
			{
				return false;
			}
			if (Victim.Mark != 31)
			{
				V6::SetHostPhase(GetWorld(), 33);
				V6::S.Phase = -1;
				return true; // essai non fait (note chez le client)
			}
			if ((Victim.Pawn->NetFlags & 16) != 0 && StepTime < 120.f)
			{
				return false; // le client quitte le mode invincible (etat envoye a l'hote)
			}
			V6::S.LogFrom = LogLineCount();
			V6::S.FallsBefore = Victim.Pawn->GetServerPitFalls();
			Victim.Pawn->ReceiveAttack(1000.f, 0.f, nullptr);
			V6::S.Clock = FPlatformTime::Seconds();
			V6::S.Phase = 0;
			Note(FString::Printf(TEXT("hote : coup mortel sur le client lointain (a %.0f m), mort officielle %s"),
				FVector::Dist2D(Victim.Pawn->GetActorLocation(), GetPlayer() ? GetPlayer()->GetActorLocation() : FVector::ZeroVector) / 100.f, V6::Oui(Victim.Pawn->GetDeathState().bDead)));
			return true;
		});
		LimitLastStep(130.f);
		// Suivi de la mort et du reveil chez la victime ; chez l'hote, aucun reveil refuse
		Add(TEXT("v4.16 reseau : mort loin des coequipiers, a terre puis reveil"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			if (!bOnClient)
			{
				if (V6::S.bSkipBlock || V6::S.Phase == -1)
				{
					V6::S.Phase = 0;
					return true;
				}
				V6::FMate Victim;
				const bool bThere = V6::MateAt(W, 1, Victim);
				if (bThere && Victim.Mark < 33 && StepTime < 90.f)
				{
					return false;
				}
				const bool bRefused = LogContains(V6::S.LogFrom, TEXT("reveil refuse"));
				// v4.16 (fusion) : compteur de l'hote (le releve du journal ne garde que les avertissements)
				const bool bGaveUp = bThere && Victim.Pawn && Victim.Pawn->ServerGiveUpsAccepted > 0;
				Note(FString::Printf(TEXT("hote : reveil du client mort loin de ses coequipiers refuse %s (attendu non ; v4.15 : refuse, le client se croyait seul), "
					"abandon a terre accepte %s (attendu oui, v4.16 : ServerReportGiveUp)"), bRefused ? TEXT("OUI") : TEXT("non"), bGaveUp ? TEXT("oui") : TEXT("NON")), bRefused || !bGaveUp);
				// v4.16 (relecture) : apres le reveil (et quelques pas du client)
				if (bThere)
				{
					FString Woke;
					const bool bClean = V6::WokeCleanly(Victim.Pawn, V6::S.FallsBefore, Woke);
					Note(TEXT("hote : client mort loin de ses coequipiers, apres son reveil : ") + Woke, !bClean);
				}
				V6::SetHostPhase(GetWorld(), 33);
				return true;
			}
			if (!V6::S.bVictim)
			{
				return V6::HostPhase(GetWorld()) >= 33 || StepTime > 120.f;
			}
			const double Now = FPlatformTime::Seconds();
			V6::TrackDeath(W, C, Now);
			const bool bDead = C->IsDead();
			C->Sanity = 100.f;
			// v4.16 : {Saut} a terre chez un client : abandon accepte par l'hote
			FString JumpNote;
			bool bJumpProblem = false;
			if (V6::TryGiveUp(W, C, bDead, Now, JumpNote, bJumpProblem))
			{
				Note(JumpNote, bJumpProblem);
			}
			if (V6::S.Deaths == 0 && StepTime < 15.f)
			{
				return false;
			}
			// Reveil, puis 4 s debout (un refus de l'hote remettrait a terre) ; quelques pas apres 1,5 s (mouvements recus par
			// l'hote depuis la nouvelle place)
			if (!bDead && V6::S.Wakes > 0 && Now - V6::S.WakeAt > 1.5)
			{
				C->AddMovementInput(C->GetActorForwardVector(), 1.f);
			}
			if ((bDead || V6::S.Wakes == 0 || Now - V6::S.WakeAt < 4.0) && StepTime < 70.f)
			{
				return false;
			}
			const float Window = BRDeath::ReviveWindow(EBRDeathCause::Injury);
			const bool bDelayOk = V6::S.DelayAtDeath > Window - 1.f;
			Note(FString::Printf(TEXT("client mort loin de ses coequipiers : delai a terre %.1f s (attendu %.0f : coequipiers debout, repliques a toute distance ; v4.15 : 6), "
				"aide d'un coequipier affichee %s (attendu oui), a terre %d fois (attendu 1), reveils %d, reveil %.1f s apres la mort"), V6::S.DelayAtDeath, Window,
				V6::Oui(V6::S.bHelpAtDeath), V6::S.Deaths, V6::S.Wakes, V6::S.Wakes > 0 ? V6::S.WakeAt - V6::S.DeathAt : -1.0),
				!bDelayOk || !V6::S.bHelpAtDeath || V6::S.Deaths != 1 || V6::S.Wakes != 1 || bDead);
			if (!V6::S.bJumpNoted)
			{
				Skip(TEXT("client a terre : appui sur {Saut} non essaye (reveil avant l'appui) : abandon chez un client NON VERIFIE"));
			}
			C->bGodMode = true;
			V6::Mark(GetPC(), 33);
			return true;
		});
		LimitLastStep(140.f);

		// C2 : toute l'equipe a terre : la place 1 d'abord, les autres 3 s apres (dernier coequipier debout qui tombe)
		Barrier(34, TEXT("toute l'equipe a terre"));
		Add(TEXT("v4.16 reseau : equipe a terre, preparation"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			V6::ResetDeathTrack();
			V6::S.Phase = 0;
			if (bOnClient)
			{
				C->bGodMode = false;
				V6::Mark(GetPC(), 35);
				return true;
			}
			const TArray<V6::FMate> M = V6::Mates(W);
			if (M.Num() == 0)
			{
				Skip(TEXT("equipe a terre : aucun client"));
				V6::S.bSkipBlock = true;
				return true;
			}
			V6::S.bSkipBlock = false;
			for (const V6::FMate& X : M)
			{
				if ((X.Mark < 35 || (X.Pawn->NetFlags & 16) != 0 || X.Pawn->IsDead()) && StepTime < 40.f)
				{
					return false;
				}
			}
			V6::S.LogFrom = LogLineCount();
			for (const V6::FMate& X : M)
			{
				V6::S.FallsBySlot[FMath::Clamp(X.Slot, 0, 3)] = X.Pawn->GetServerPitFalls();
			}
			C->bGodMode = false;
			V6::FMate First;
			if (V6::MateAt(W, 1, First))
			{
				First.Pawn->ReceiveAttack(1000.f, 0.f, nullptr);
			}
			V6::S.Clock = FPlatformTime::Seconds();
			return true;
		});
		LimitLastStep(60.f);
		Add(TEXT("v4.16 reseau : equipe a terre, les autres tombent"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C || bOnClient || V6::S.bSkipBlock)
			{
				return true;
			}
			if (FPlatformTime::Seconds() - V6::S.Clock < 3.0)
			{
				return false;
			}
			for (const V6::FMate& X : V6::Mates(W))
			{
				if (X.Slot != 1 && !X.Pawn->IsDead())
				{
					X.Pawn->ReceiveAttack(1000.f, 0.f, nullptr);
				}
			}
			C->ReceiveAttack(1000.f, 0.f, nullptr);
			Note(FString::Printf(TEXT("hote : place 1 a terre, puis les autres et l'hote 3 s apres (hote a terre %s)"), V6::Oui(C->IsDead())));
			V6::SetHostPhase(GetWorld(), 36);
			return true;
		});
		Add(TEXT("v4.16 reseau : equipe a terre, reveils"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			if (V6::S.bSkipBlock && !bOnClient)
			{
				C->bGodMode = true;
				return true;
			}
			const double Now = FPlatformTime::Seconds();
			V6::TrackDeath(W, C, Now);
			const bool bDead = C->IsDead();
			C->Sanity = 100.f;
			if (V6::S.Deaths == 0 && StepTime < 60.f)
			{
				return false;
			}
			// v4.16 (relecture) : quelques pas apres le reveil (mouvements recus par l'hote depuis la nouvelle place)
			if (bOnClient && !bDead && V6::S.Wakes > 0 && Now - V6::S.WakeAt > 1.5 && Now - V6::S.WakeAt < 4.0)
			{
				C->AddMovementInput(C->GetActorForwardVector(), 1.f);
			}
			if ((bDead || V6::S.Wakes == 0 || Now - V6::S.WakeAt < 4.0) && StepTime < 110.f)
			{
				return false;
			}
			const TCHAR* Who = bOnClient ? TEXT("client") : TEXT("hote");
			const double Wake = V6::S.Wakes > 0 ? V6::S.WakeAt - V6::S.DeathAt : -1.0;
			const float Window = BRDeath::ReviveWindow(EBRDeathCause::Injury);
			// Place 1 : tombee la premiere (fenetre entiere), puis plus personne debout 3 s apres : reveil 6 s plus tard
			const bool bFirst = bOnClient && V6::S.Slot == 1;
			const bool bShort = !bFirst || (Wake > 0.0 && Wake < Window - 5.0);
			if (V6::S.Phase != 37)
			{
				V6::S.Phase = 37;
				Note(FString::Printf(TEXT("%s (place %d) : a terre %d fois (attendu 1), reveils %d, reveil %.1f s apres sa mort (delai a la mort %.1f s)%s"), Who, V6::S.Slot,
					V6::S.Deaths, V6::S.Wakes, Wake, V6::S.DelayAtDeath, bFirst ? TEXT(" ; tombee la premiere : attendu 3 + 6 s, avant la fin de sa fenetre") : TEXT("")),
					V6::S.Deaths != 1 || V6::S.Wakes != 1 || bDead || !bShort);
				C->bGodMode = true;
			}
			if (bOnClient)
			{
				V6::S.Phase = 0;
				V6::Mark(GetPC(), 37);
				return true;
			}
			for (const V6::FMate& X : V6::Mates(W))
			{
				if (X.Mark < 37 && StepTime < 140.f)
				{
					return false;
				}
			}
			V6::S.Phase = 0;
			const bool bRefused = LogContains(V6::S.LogFrom, TEXT("reveil refuse"));
			Note(FString::Printf(TEXT("hote : equipe a terre, reveil refuse par l'hote %s (attendu non ; v4.15 : refuse a la place 1, qui retombait au point de depart)"),
				bRefused ? TEXT("OUI") : TEXT("non")), bRefused);
			// v4.16 (relecture) : chaque client apres son reveil (et quelques pas)
			for (const V6::FMate& X : V6::Mates(W))
			{
				FString Woke;
				const bool bClean = V6::WokeCleanly(X.Pawn, V6::S.FallsBySlot[FMath::Clamp(X.Slot, 0, 3)], Woke);
				Note(FString::Printf(TEXT("hote : equipe a terre, client de la place %d apres son reveil : %s"), X.Slot, *Woke), !bClean);
			}
			return true;
		});
		LimitLastStep(160.f);

		// C3 (v4.16, relecture) : le client de la place 1 s'eloigne de nouveau ; tous les autres (hote compris) tombent d'abord,
		// puis lui. Plus personne debout a sa mort : 6 s comme en v4.15 (la premiere correction supposait vivant tout coequipier
		// non replique : 30 s). Les autres, tombes avec un coequipier debout, ramenent leur delai a 6 s a sa chute. Aucun reveil
		// refuse, une mort chacun. Etapes : clients prets 38 (place 1 : 39 si l'essai est impossible), fin 40 ; hote 38 (coups
		// portes), 39 (fin)
		GoFar(38, 39, true, TEXT("mort loin apres les autres"));
		Add(TEXT("v4.16 reseau : mort loin apres les autres, coups de l'hote"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C || bOnClient)
			{
				return true;
			}
			V6::FMate Victim;
			if (V6::S.bSkipBlock || !V6::MateAt(W, 1, Victim))
			{
				V6::S.Phase = -1;
				V6::SetHostPhase(GetWorld(), 39);
				return true;
			}
			const TArray<V6::FMate> M = V6::Mates(W);
			if (V6::S.Phase == 0)
			{
				bool bWait = false;
				for (const V6::FMate& X : M)
				{
					bWait |= X.Mark < 38 || (X.Mark == 38 && ((X.Pawn->NetFlags & 16) != 0 || X.Pawn->IsDead()));
				}
				if (bWait && StepTime < 130.f)
				{
					return false;
				}
				if (Victim.Mark != 38)
				{
					// essai impossible chez la victime (note chez elle) ou jamais pret
					V6::S.Phase = -1;
					V6::SetHostPhase(GetWorld(), 39);
					return true;
				}
				// Tous les autres a terre d'abord (coequipier lointain debout : fenetre de 30 s pour eux)
				V6::S.LogFrom = LogLineCount();
				V6::S.FallsBefore = Victim.Pawn->GetServerPitFalls();
				for (const V6::FMate& X : M)
				{
					V6::S.FallsBySlot[FMath::Clamp(X.Slot, 0, 3)] = X.Pawn->GetServerPitFalls();
					if (X.Slot != 1)
					{
						X.Pawn->ReceiveAttack(1000.f, 0.f, nullptr);
					}
				}
				C->bGodMode = false;
				C->ReceiveAttack(1000.f, 0.f, nullptr);
				V6::S.Clock = FPlatformTime::Seconds();
				V6::S.Phase = 1;
				return false;
			}
			// 1,5 s plus tard (etats de mort recus par la victime), le client lointain
			if (V6::S.Phase == 1)
			{
				if (FPlatformTime::Seconds() - V6::S.Clock < 1.5)
				{
					return false;
				}
				bool bOthersDown = C->GetDeathState().bDead;
				for (const V6::FMate& X : M)
				{
					bOthersDown &= X.Slot == 1 || X.Pawn->GetDeathState().bDead;
				}
				V6::S.bOthersDownAtHit = bOthersDown;
				Victim.Pawn->ReceiveAttack(1000.f, 0.f, nullptr);
				V6::S.Clock = FPlatformTime::Seconds();
				V6::S.Phase = 2;
				return false;
			}
			// v4.16 (fusion) : la mort officielle suit le signalement du client (ServerReportDeath, ou l'hote l'applique seul
			// au bout du delai de ABRCharacter::TickServerVitals) : avant, la note etait faite dans l'image du coup, toujours NON
			const double SinceHit = FPlatformTime::Seconds() - V6::S.Clock;
			if (!Victim.Pawn->GetDeathState().bDead && SinceHit < 3.0)
			{
				return false;
			}
			Note(FString::Printf(TEXT("hote : mort loin apres les autres : hote et autres clients a terre %s, puis coup mortel sur le client lointain (a %.0f m de l'hote), mort officielle %s (%.1f s apres le coup, attendu au plus 3 s)"),
				V6::Oui(V6::S.bOthersDownAtHit), FVector::Dist2D(Victim.Pawn->GetActorLocation(), C->GetActorLocation()) / 100.f, V6::Oui(Victim.Pawn->GetDeathState().bDead), SinceHit),
				!V6::S.bOthersDownAtHit || !Victim.Pawn->GetDeathState().bDead);
			V6::S.Phase = 0;
			V6::SetHostPhase(GetWorld(), 38);
			return true;
		});
		LimitLastStep(150.f);
		Add(TEXT("v4.16 reseau : mort loin apres les autres, reveils"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			if (!bOnClient && V6::S.Phase == -1)
			{
				V6::S.Phase = 0;
				C->bGodMode = true;
				return true;
			}
			const double Now = FPlatformTime::Seconds();
			if (bOnClient && V6::HostPhase(GetWorld()) >= 39 && V6::S.Deaths == 0)
			{
				// essai abandonne par l'hote (victime absente ou essai impossible chez elle)
				C->bGodMode = true;
				V6::Mark(GetPC(), 40);
				return true;
			}
			V6::TrackDeath(W, C, Now);
			const bool bDead = C->IsDead();
			C->Sanity = 100.f;
			if (V6::S.Deaths == 0 && StepTime < 60.f)
			{
				return false;
			}
			if (bOnClient && !bDead && V6::S.Wakes > 0 && Now - V6::S.WakeAt > 1.5 && Now - V6::S.WakeAt < 4.0)
			{
				C->AddMovementInput(C->GetActorForwardVector(), 1.f);
			}
			if ((bDead || V6::S.Wakes == 0 || Now - V6::S.WakeAt < 4.0) && StepTime < 110.f)
			{
				return false;
			}
			const TCHAR* Who = bOnClient ? TEXT("client") : TEXT("hote");
			const double Wake = V6::S.Wakes > 0 ? V6::S.WakeAt - V6::S.DeathAt : -1.0;
			if (V6::S.Phase != 40)
			{
				V6::S.Phase = 40;
				if (V6::S.bVictim)
				{
					// Le client lointain, tombe le dernier : plus personne debout, delai de 6 s, pas d'aide proposee
					const bool bShortOk = V6::S.DelayAtDeath > 0.f && V6::S.DelayAtDeath <= BRTxn::DownedAloneSeconds + 0.5f;
					Note(FString::Printf(TEXT("client lointain tombe apres tous les autres : delai a terre %.1f s (attendu 6 : plus personne debout, coequipiers lointains repliques ; "
						"premiere correction v4.16 : 30), aide d'un coequipier affichee %s (attendu non), a terre %d fois (attendu 1), reveils %d, reveil %.1f s apres la mort"),
						V6::S.DelayAtDeath, V6::Oui(V6::S.bHelpAtDeath), V6::S.Deaths, V6::S.Wakes, Wake),
						!bShortOk || V6::S.bHelpAtDeath || V6::S.Deaths != 1 || V6::S.Wakes != 1 || bDead || Wake > 10.0);
				}
				else
				{
					Note(FString::Printf(TEXT("%s (place %d) tombe avant le client lointain : a terre %d fois (attendu 1), reveils %d, reveil %.1f s apres sa mort (delai a la mort %.1f s ; "
						"ramene a 6 s a la chute du dernier : attendu avant 12 s)"), Who, V6::S.Slot, V6::S.Deaths, V6::S.Wakes, Wake, V6::S.DelayAtDeath),
						V6::S.Deaths != 1 || V6::S.Wakes != 1 || bDead || Wake > 12.0);
				}
				C->bGodMode = true;
			}
			if (bOnClient)
			{
				V6::S.Phase = 0;
				V6::Mark(GetPC(), 40);
				return true;
			}
			const TArray<V6::FMate> M = V6::Mates(W);
			for (const V6::FMate& X : M)
			{
				if (X.Mark < 40 && StepTime < 140.f)
				{
					return false;
				}
			}
			V6::S.Phase = 0;
			const bool bRefused = LogContains(V6::S.LogFrom, TEXT("reveil refuse"));
			Note(FString::Printf(TEXT("hote : mort loin apres les autres, reveil refuse par l'hote %s (attendu non)"), bRefused ? TEXT("OUI") : TEXT("non")), bRefused);
			for (const V6::FMate& X : M)
			{
				FString Woke;
				const bool bClean = V6::WokeCleanly(X.Pawn, V6::S.FallsBySlot[FMath::Clamp(X.Slot, 0, 3)], Woke);
				Note(FString::Printf(TEXT("hote : mort loin apres les autres, client de la place %d apres son reveil : %s"), X.Slot, *Woke), !bClean);
			}
			V6::SetHostPhase(GetWorld(), 39);
			return true;
		});
		LimitLastStep(160.f);

		// ============================================================================================= D. sas loin de l'hote
		Barrier(40, TEXT("sas des Poolrooms loin de l'hote"));
		// Places : l'hote a plus de 45 m du sas, le client de la place 1 devant son entree
		auto FarPlaces = [this, bOnClient](uint8 Module, int32 MarkId, const TCHAR* What, float FrontX)
		{
			Add(FString::Printf(TEXT("v4.16 reseau : %s, places"), What), 0.f, [this, bOnClient, Module, MarkId, What, FrontX]()
			{
				ABRWorld* W = GetBRWorld();
				ABRCharacter* C = GetPlayer();
				if (!W || !C)
				{
					return false;
				}
				ABRMissionDevice* Dev = V6::FindModule(W, Module);
				if (V6::S.Phase == 0)
				{
					V6::S.bSkipBlock = false;
					V6::S.bTickSeen = false;
					if (!Dev)
					{
						Skip(FString::Printf(TEXT("%s : module absent dans cette disposition"), What));
						V6::S.bSkipBlock = true;
						V6::Mark(GetPC(), MarkId);
						if (!bOnClient)
						{
							V6::SetHostPhase(GetWorld(), MarkId);
						}
						return true;
					}
					if (!bOnClient)
					{
						V6::FMate Op;
						if (!V6::MateAt(W, 1, Op))
						{
							Skip(FString::Printf(TEXT("%s : aucun client pour manipuler le mecanisme"), What));
							V6::S.bSkipBlock = true;
							V6::SetHostPhase(GetWorld(), MarkId);
							return true;
						}
						FVector Far;
						if (FVector::Dist2D(C->GetActorLocation(), Dev->GetActorLocation()) < V6::HostFar && V6::FarSpot(W, C, Dev->GetActorLocation(), V6::HostFar + 500.f, Far))
						{
							C->SetActorLocation(Far, false, nullptr, ETeleportType::TeleportPhysics);
							if (UCharacterMovementComponent* M = C->GetCharacterMovement())
							{
								M->StopMovementImmediately();
							}
						}
						V6::S.Phase = 5;
						return false;
					}
					if (V6::S.Slot != 1)
					{
						V6::S.Phase = 5;
						return false;
					}
					// Devant le mecanisme, sur son axe (repere du module)
					const BRMech::FFrame F = Dev->GetModuleFrame();
					float X = 0.f;
					float Y = 0.f;
					F.ToWorld(FrontX, 0.f, X, Y);
					float Z = 0.f;
					if (!V6::FloorAt(W, C, FVector(X, Y, F.Z), F.Z, Z))
					{
						Z = F.Z;
					}
					V6::S.Spot2 = FVector(X, Y, Z + C->GetSimpleCollisionHalfHeight() + 5.f);
					const FVector Face = Dev->GetActorLocation() + FVector(0.f, 0.f, 120.f);
					V6::S.Place.Start(C, V6::S.Spot2, &Face, FPlatformTime::Seconds());
					V6::S.Phase = 4;
					return false;
				}
				if (V6::S.Phase == 4)
				{
					if (!V6::S.Place.Update(C, FPlatformTime::Seconds()) && StepTime < 30.f)
					{
						return false;
					}
					V6::S.Phase = 5;
				}
				if (bOnClient)
				{
					V6::Mark(GetPC(), MarkId);
					if (V6::HostPhase(GetWorld()) < MarkId && StepTime < 120.f)
					{
						return false;
					}
					V6::S.Phase = 0;
					return true;
				}
				for (const V6::FMate& X : V6::Mates(W))
				{
					if (X.Mark < MarkId && StepTime < 100.f)
					{
						return false;
					}
				}
				V6::S.Phase = 0;
				Note(FString::Printf(TEXT("hote : %s : hote a %.0f m du mecanisme (au-dela de 35 m, son Tick y etait coupe chez l'hote)"), What,
					FVector::Dist2D(C->GetActorLocation(), Dev->GetActorLocation()) / 100.f));
				if (FVector::Dist2D(C->GetActorLocation(), Dev->GetActorLocation()) < 3600.f)
				{
					Skip(FString::Printf(TEXT("%s : l'hote n'a pas trouve de place a plus de 36 m du mecanisme : cas non reproduit"), What));
					V6::S.bSkipBlock = true;
				}
				V6::SetHostPhase(GetWorld(), MarkId);
				return true;
			});
			LimitLastStep(140.f);
		};
		// Actions : l'hote joue la solution sauf la derniere action ; le client de la place 1 la fait (vraie demande, regles de
		// l'hote : portee, vue) ; a defaut l'hote l'applique (NON VERIFIE pour l'action du client). Etapes de l'hote : MarkId + 1
		// (derniere action au client), MarkId + 2 (instant commun, v4.16 relecture), MarkId + 4 (fait), MarkId + 5 (essai
		// abandonne) ; le client signale MarkId + 2 (en place) puis MarkId + 3. bRace : le client de la place 2 demande la meme
		// action au meme instant du serveur (interactions simultanees sur un mecanisme)
		auto FarActions = [this, bOnClient](int32 MarkId, const TCHAR* What, bool bRace)
		{
			Add(FString::Printf(TEXT("v4.16 reseau : %s, actions de l'hote"), What), 1.f, [this, bOnClient, MarkId, What]()
			{
				ABRWorld* W = GetBRWorld();
				if (!W || bOnClient)
				{
					return true;
				}
				if (V6::S.bSkipBlock)
				{
					V6::SetHostPhase(GetWorld(), MarkId + 5);
					return true;
				}
				TArray<BRM::FSolveStep> Steps;
				if (!V6::Solve(W, Steps))
				{
					Skip(FString::Printf(TEXT("%s : solveur sans solution depuis cet etat"), What));
					V6::S.bSkipBlock = true;
					V6::SetHostPhase(GetWorld(), MarkId + 5);
					return true;
				}
				for (int32 I = 0; I + 1 < Steps.Num(); ++I)
				{
					W->DebugMissionAct(Steps[I].Device, Steps[I].Action);
				}
				V6::S.Device = Steps.Last().Device;
				V6::S.Action = Steps.Last().Action;
				// v4.16 (correctif) : revision de l'etat publie, comparee a celle que le client lit (meme etat recu)
				Note(FString::Printf(TEXT("hote : %s : %d action(s) jouees par l'hote, la derniere (mecanisme %d, %s, action %d) revient au client ; etat de la mission : revision %d"),
					What, Steps.Num() - 1, V6::S.Device, *V6::RoleOf(W, V6::S.Device), static_cast<int32>(V6::S.Action), static_cast<int32>(W->GetMissionRev())));
				V6::SetHostPhase(GetWorld(), MarkId + 1);
				return true;
			});
			Add(FString::Printf(TEXT("v4.16 reseau : %s, action du client"), What), 0.f, [this, bOnClient, MarkId, What, bRace]()
			{
				ABRWorld* W = GetBRWorld();
				ABRCharacter* C = GetPlayer();
				if (!W || !C)
				{
					return false;
				}
				if (!bOnClient)
				{
					if (V6::S.bSkipBlock)
					{
						return true;
					}
					V6::FMate Op;
					const bool bOp = V6::MateAt(W, 1, Op);
					// v4.16 (relecture) : deuxieme client (place 2) sur le meme mecanisme, au meme instant du serveur
					V6::FMate Op2;
					const bool bOp2 = bRace && V6::MateAt(W, 2, Op2);
					if (V6::S.Phase == 0)
					{
						// Acteurs en place (MarkId + 2 ; MarkId + 3 : essai impossible chez eux), puis l'instant commun
						const bool bReady = (!bOp || Op.Mark >= MarkId + 2) && (!bOp2 || Op2.Mark >= MarkId + 2);
						if (!bReady && StepTime < 120.f)
						{
							return false;
						}
						if (!V6::SyncOpenNow(GetWorld()) && StepTime < 132.f)
						{
							return false;
						}
						V6::S.MissionBefore = W->GetMissionState();
						V6::S.MissionReqBefore[0] = bOp ? Op.Pawn->ServerMissionRequests : 0;
						V6::S.MissionReqBefore[1] = bOp2 ? Op2.Pawn->ServerMissionRequests : 0;
						if (bOp)
						{
							Op.Pawn->ServerLastRequestFrame = 0;
						}
						if (bOp2)
						{
							Op2.Pawn->ServerLastRequestFrame = 0;
						}
						V6::SetHostPhase(GetWorld(), MarkId + 2);
						V6::S.Phase = 1;
						return false;
					}
					if (((bOp && Op.Mark < MarkId + 3) || (bOp2 && Op2.Mark < MarkId + 3)) && StepTime < 170.f)
					{
						return false;
					}
					V6::S.Phase = 0;
					if (bRace && !bOp2)
					{
						Skip(FString::Printf(TEXT("%s : interactions simultanees sur un mecanisme NON VERIFIEES (moins de 2 clients)"), What));
					}
					else if (bRace && bOp)
					{
						// Les deux demandes, appliquees par l'hote l'une apres l'autre (comme le jeu), depuis l'etat d'avant : meme etat,
						// memes retours (ordre quelconque). Une demande perdue, doublee ou appliquee sur un etat perime se verrait ici
						const int32 N1 = Op.Pawn->ServerMissionRequests - V6::S.MissionReqBefore[0];
						const int32 N2 = Op2.Pawn->ServerMissionRequests - V6::S.MissionReqBefore[1];
						const uint8 F1 = Op.Pawn->ServerLastMissionFeedback;
						const uint8 F2 = Op2.Pawn->ServerLastMissionFeedback;
						const auto Refused = [](uint8 Fb)
						{
							return Fb == BRMissionText::TooFar || Fb == BRMissionText::NotNow;
						};
						if (N1 != 1 || N2 != 1 || Refused(F1) || Refused(F2))
						{
							Skip(FString::Printf(TEXT("%s : interactions simultanees NON VERIFIEES : demandes recues par l'hote %d et %d (attendu 1 et 1), retours %d et %d (hors de portee %d, refus %d)"),
								What, N1, N2, static_cast<int32>(F1), static_cast<int32>(F2), static_cast<int32>(BRMissionText::TooFar), static_cast<int32>(BRMissionText::NotNow)));
						}
						else
						{
							const BRM::FPlan& Plan = W->GetMissionPlan();
							const BRM::FDevice& D = Plan.Devices[FMath::Clamp(V6::S.Device, 0, Plan.NumDevices - 1)];
							const bool bHeld = D.Kind == BRM::EKind::Observe || D.Kind == BRM::EKind::Crank;
							BRM::FState Sim = V6::S.MissionBefore;
							uint8 Expect[2] = { 0, 0 };
							for (int32 K = 0; K < 2; ++K)
							{
								if (K == 1 && D.Kind == BRM::EKind::Button && Expect[0] == static_cast<uint8>(BRM::EFeedback::Wrong))
								{
									Expect[1] = BRMissionText::Cooldown; // ABRWorld::ServerMissionAct : bouton rearme apres une erreur
									continue;
								}
								const BRM::FResult R = BRM::Act(Plan, Sim, W->GetMissionCampaign(), V6::S.Device, bHeld ? BRM::EAction::Hold : BRM::EAction::Use, bHeld ? 1 : 0);
								Expect[K] = (D.Kind == BRM::EKind::Item && R.Feedback == BRM::EFeedback::AlreadyDone) ? BRMissionText::Taken : static_cast<uint8>(R.Feedback);
							}
							const bool bState = FMemory::Memcmp(&Sim, &W->GetMissionState(), sizeof(BRM::FState)) == 0;
							const bool bFeedback = (F1 == Expect[0] && F2 == Expect[1]) || (F1 == Expect[1] && F2 == Expect[0]);
							const int32 Dones = (F1 == static_cast<uint8>(BRM::EFeedback::Done) ? 1 : 0) + (F2 == static_cast<uint8>(BRM::EFeedback::Done) ? 1 : 0);
							TArray<const ABRCharacter*> Who;
							Who.Add(Op.Pawn);
							Who.Add(Op2.Pawn);
							int32 With = 0;
							const FString Spread = V6::FrameSpread(Who, With);
							Note(FString::Printf(TEXT("hote : %s : meme action demandee par 2 clients au meme instant du serveur (%s) : retours %d et %d (attendu %d puis %d, dans un ordre "
								"quelconque), %d \"fait\", etat de la mission celui des deux demandes appliquees l'une apres l'autre %s"), What, *Spread, static_cast<int32>(F1),
								static_cast<int32>(F2), static_cast<int32>(Expect[0]), static_cast<int32>(Expect[1]), Dones, V6::Oui(bState)), !bState || !bFeedback);
						}
					}
					BRM::FEval E;
					W->GetMissionEval(E);
					if (!E.bSolved)
					{
						const uint8 Fb = W->DebugMissionAct(V6::S.Device, V6::S.Action);
						Skip(FString::Printf(TEXT("%s : la derniere action n'est pas venue du client (etape %d) : appliquee par l'hote (retour %d) ; l'animation loin de l'hote reste jugee"),
							What, Op.Mark, static_cast<int32>(Fb)));
					}
					else
					{
						Note(FString::Printf(TEXT("hote : %s : mission resolue par l'action du client"), What));
					}
					V6::SetHostPhase(GetWorld(), MarkId + 4);
					return true;
				}
				const double Now = FPlatformTime::Seconds();
				if (V6::HostPhase(GetWorld()) >= MarkId + 4 || StepTime > 170.f)
				{
					if (V6::S.Phase != 0)
					{
						V6::Hover(C, false);
					}
					V6::S.Phase = 0;
					return true;
				}
				const bool bActor = V6::S.Slot == 1 || (bRace && V6::S.Slot == 2);
				if (!bActor || V6::S.Phase == 9 || V6::HostPhase(GetWorld()) < MarkId + 1)
				{
					return false;
				}
				if (V6::S.Phase == 0)
				{
					// La solution restante, depuis l'etat recu de l'hote : une action. v4.16 (correctif) : jugee par V6::OneActionLeft
					// (la derniere action du solveur, seule, resout la mission recue) et non plus par le nombre d'actions que le
					// solveur rejoue depuis cet etat (9 au sas, 4 a la passerelle : relectures et reglages repris compris)
					BRM::FSolveStep Last;
					int32 Replayed = 0;
					const bool bOne = V6::OneActionLeft(W, Last, Replayed);
					if (!bOne && StepTime < 15.f)
					{
						return false;
					}
					ABRMissionDevice* Target = bOne ? W->FindMissionDevice(Last.Device) : nullptr;
					if (!Target)
					{
						BRM::FEval E;
						W->GetMissionEval(E);
						Note(bOne ? FString::Printf(TEXT("client (place %d) : %s : mecanisme %d de la derniere action absent chez ce client"), V6::S.Slot, What,
								static_cast<int32>(Last.Device))
							: FString::Printf(TEXT("client (place %d) : %s : etat recu de l'hote (revision %d) a une action de la resolution NON apres %.0f s (mission resolue %s, "
								"le solveur rejoue %d action(s) depuis cet etat)"), V6::S.Slot, What, static_cast<int32>(W->GetMissionRev()), StepTime, V6::Oui(E.bSolved), Replayed), true);
						V6::Mark(GetPC(), MarkId + 3);
						V6::S.Phase = 9;
						return false;
					}
					Note(FString::Printf(TEXT("client (place %d) : %s : etat recu de l'hote (revision %d) a une action de la resolution oui : mecanisme %d, action %d (le solveur "
						"rejoue %d action(s) depuis cet etat)"), V6::S.Slot, What, static_cast<int32>(W->GetMissionRev()), static_cast<int32>(Last.Device),
						static_cast<int32>(Last.Action), Replayed));
					V6::S.Device = Last.Device;
					V6::S.Action = Last.Action;
					// Vers le mecanisme (son chunk se construit autour du joueur), en vol
					V6::Hover(C, true);
					BRTestNet::TeleportForServer(C, Target->GetInteractPoint() + Target->GetActorForwardVector() * 160.f + FVector(0.f, 0.f, 40.f));
					V6::Hover(C, true);
					V6::S.Phase = 6;
					V6::S.Clock = Now;
					return false;
				}
				ABRMissionDevice* Dev = W->FindMissionDevice(V6::S.Device);
				if (V6::S.Phase == 6)
				{
					if ((!Dev || !Dev->IsShown()) && Now - V6::S.Clock < 20.0)
					{
						return false;
					}
					FVector Stand;
					V6::Hover(C, false);
					// v4.16 (relecture) : la place 2 prend une autre place valable, a plus d'un metre de celle de la place 1
					const bool bStand = Dev && (V6::S.Slot == 1 ? BRTestV412::FindMissionStand(W, C, Dev, false, Stand) : V6::SecondStand(W, C, Dev, Stand));
					if (!bStand)
					{
						Note(FString::Printf(TEXT("client (place %d) : %s : aucune place devant le mecanisme %d"), V6::S.Slot, What, V6::S.Device), V6::S.Slot == 1);
						if (V6::S.Slot != 1)
						{
							Skip(FString::Printf(TEXT("%s : pas de seconde place devant le mecanisme : interactions simultanees NON VERIFIEES"), What));
						}
						V6::Mark(GetPC(), MarkId + 3);
						V6::S.Phase = 9;
						return false;
					}
					const FVector Aim = Dev->GetInteractPoint();
					V6::S.Place.Start(C, Stand, &Aim, Now);
					V6::S.Phase = 1;
					V6::S.Clock = Now;
					return false;
				}
				if (V6::S.Phase == 1)
				{
					if (!V6::S.Place.Update(C, Now) && Now - V6::S.Clock < 10.0)
					{
						return false;
					}
					// v4.16 (relecture) : en place ; la demande part a l'instant commun ouvert par l'hote
					V6::Mark(GetPC(), MarkId + 2);
					V6::S.Phase = 7;
					V6::S.Clock = Now;
					return false;
				}
				if (V6::S.Phase == 7)
				{
					// (plus long que l'attente de l'hote, 120 s + l'instant commun : aucune demande ne part avant l'etat note par l'hote)
					if (V6::HostPhase(GetWorld()) < MarkId + 2 && Now - V6::S.Clock < 135.0)
					{
						return false;
					}
					V6::S.SyncAt = V6::SyncTarget(GetWorld());
					V6::S.Phase = 8;
					V6::S.Clock = Now;
					return false;
				}
				if (V6::S.Phase == 8)
				{
					if (V6::ServerNow(GetWorld()) < V6::S.SyncAt && Now - V6::S.Clock < 15.0)
					{
						return false;
					}
					V6::S.ResultsBefore = C->MissionResultsReceived;
					C->RequestMissionAction(V6::S.Device, V6::S.Action);
					V6::S.Phase = 2;
					V6::S.Clock = Now;
					return false;
				}
				if (V6::S.Phase == 2)
				{
					if (C->MissionResultsReceived == V6::S.ResultsBefore && Now - V6::S.Clock < 10.0)
					{
						return false;
					}
					const bool bAnswered = C->MissionResultsReceived != V6::S.ResultsBefore;
					Note(FString::Printf(TEXT("client (place %d) : %s : derniere action (mecanisme %d, action %d) demandee a l'hote : reponse %s, retour %d"), V6::S.Slot, What,
						V6::S.Device, static_cast<int32>(V6::S.Action), V6::Oui(bAnswered), static_cast<int32>(C->LastMissionFeedback)));
					// Retour devant le module (le mecanisme manipule peut en etre loin) ; la place 2 reste a sa place
					if (V6::S.Slot == 1)
					{
						const FVector Face = V6::S.Spot2 + FVector(0.f, 0.f, 50.f);
						V6::S.Place.Start(C, V6::S.Spot2, &Face, Now);
					}
					V6::S.Phase = 3;
					V6::S.Clock = Now;
					return false;
				}
				if (!V6::S.Place.Update(C, Now) && Now - V6::S.Clock < 10.0)
				{
					return false;
				}
				V6::Mark(GetPC(), MarkId + 3);
				V6::S.Phase = 9;
				return false;
			});
			LimitLastStep(200.f);
		};
		FarPlaces(BRMech::Module::PoolLock, 41, TEXT("sas loin de l'hote"), 160.f);
		FarActions(41, TEXT("sas loin de l'hote"), true);
		Add(TEXT("v4.16 reseau : sas loin de l'hote, deversoir et eau"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			ABRMissionDevice* Lock = V6::FindModule(W, BRMech::Module::PoolLock);
			if (!Lock || V6::S.bSkipBlock || V6::HostPhase(GetWorld()) >= 46)
			{
				return true; // essai abandonne (note chez l'hote)
			}
			namespace P = BRMech::Pool;
			const float Slab = P::SlabTop(W->Def().WaterHeight, W->Def().DeckHeight);
			const float Surface = W->GetLocalWaterSurface(Lock, 0);
			// Un client loin du sas (places 2 et 3) ne l'anime pas, par conception (personne n'y est chez lui) : pas juge. L'hote,
			// lui, doit l'animer pour le client qui s'y trouve (son personnage y bouge selon les collisions de l'hote)
			if (bOnClient && FVector::Dist2D(C->GetActorLocation(), Lock->GetActorLocation()) > 3400.f)
			{
				Note(FString::Printf(TEXT("client (place %d) a %.0f m du sas : animation locale non jugee (aucun joueur de cette machine pres du sas)"), V6::S.Slot,
					FVector::Dist2D(C->GetActorLocation(), Lock->GetActorLocation()) / 100.f));
				return true;
			}
			V6::S.bTickSeen |= Lock->IsActorTickEnabled();
			// Vidange (25 cm/s, 130 cm) puis le deversoir suit l'eau
			if (FMath::Abs(Surface - P::ChannelDry(Slab)) > 1.f && StepTime < 15.f)
			{
				return false;
			}
			if (StepTime < 2.f)
			{
				return false;
			}
			const BRMech::FFrame F = Lock->GetModuleFrame();
			float AX = 0.f, AY = 0.f, BX = 0.f, BY = 0.f;
			F.ToWorld(60.f, 0.f, AX, AY);
			F.ToWorld(-60.f, 0.f, BX, BY);
			FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestV416Weir), false, C);
			FHitResult Hit;
			const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, FVector(AX, AY, Slab + 30.f), FVector(BX, BY, Slab + 30.f), ECC_WorldStatic, Q) && Hit.GetActor() == Lock;
			const TCHAR* Who = bOnClient ? TEXT("client") : TEXT("hote");
			const float Dist = static_cast<float>(FVector::Dist2D(C->GetActorLocation(), Lock->GetActorLocation())) / 100.f;
			Note(FString::Printf(TEXT("%s (a %.0f m du sas) : sas vide %s (surface %.0f, dallage %.0f), entree a 30 cm au-dessus du dallage libre %s%s ; Tick du sas vu %s"), Who, Dist,
				V6::Oui(FMath::Abs(Surface - P::ChannelDry(Slab)) <= 1.f), Surface, Slab, V6::Oui(!bHit),
				bHit ? *FString::Printf(TEXT(" (deversoir leve : %s, v4.15 chez un hote a plus de 35 m)"), *GetNameSafe(Hit.GetComponent())) : TEXT(""), V6::Oui(V6::S.bTickSeen)),
				bHit || FMath::Abs(Surface - P::ChannelDry(Slab)) > 1.f);
			V6::Put(OutDir, TEXT("sas"), TEXT("entree_libre"), FString::FromInt(bHit ? 0 : 1), TEXT("egal"));
			return true;
		});
		LimitLastStep(30.f);

		// ============================================================================================= E. passerelle loin de l'hote
		Barrier(50, TEXT("passerelle du Niveau 8 loin de l'hote"));
		GoLevel(8, V6::BridgeSeed, TEXT("passerelle loin de l'hote"));
		FarPlaces(BRMech::Module::Bridge, 51, TEXT("passerelle loin de l'hote"), BRMech::Bridge::Gap + BRMech::Bridge::NearDepth + BRMech::Bridge::Steps * BRMech::Bridge::StepDepth + 120.f);
		FarActions(51, TEXT("passerelle loin de l'hote"), false);
		Add(TEXT("v4.16 reseau : passerelle loin de l'hote, tablier pose"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			ABRMissionDevice* Bridge = V6::FindModule(W, BRMech::Module::Bridge);
			if (!Bridge || V6::S.bSkipBlock || V6::HostPhase(GetWorld()) >= 56)
			{
				return true; // essai abandonne (note chez l'hote)
			}
			namespace B = BRMech::Bridge;
			if (bOnClient && FVector::Dist2D(C->GetActorLocation(), Bridge->GetActorLocation()) > 3400.f)
			{
				Note(FString::Printf(TEXT("client (place %d) a %.0f m de la passerelle : animation locale non jugee (aucun joueur de cette machine pres de la passerelle)"),
					V6::S.Slot, FVector::Dist2D(C->GetActorLocation(), Bridge->GetActorLocation()) / 100.f));
				return true;
			}
			V6::S.bTickSeen |= Bridge->IsActorTickEnabled();
			if (Bridge->GetModuleProgress() < 0.99f && StepTime < 12.f)
			{
				return false;
			}
			const BRMech::FFrame F = Bridge->GetModuleFrame();
			// Tablier : la collision suit l'animation ; vue de haut au milieu de l'interruption
			const float MidX = B::Gap * 0.5f;
			float DX = 0.f, DY = 0.f;
			F.ToWorld(MidX, 0.f, DX, DY);
			const float Top = F.Z + B::DeckTopAt(MidX);
			FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestV416Deck), false, C);
			FHitResult Hit;
			const bool bDeck = GetWorld()->LineTraceSingleByChannel(Hit, FVector(DX, DY, Top + 120.f), FVector(DX, DY, Top - 120.f), ECC_WorldStatic, Q) && Hit.GetActor() == Bridge;
			// Navigation des entites (hote) : du pied des marches vers le palier, par le tablier
			float FX = 0.f, FY = 0.f, LX = 0.f, LY = 0.f;
			F.ToWorld(B::Gap + B::NearDepth + B::Steps * B::StepDepth + 45.f, 0.f, FX, FY);
			F.ToWorld(-B::FarDepth * 0.5f, 0.f, LX, LY);
			FVector WP;
			const bool bNav = W->MissionNavDetour(FVector(FX, FY, F.Z + 88.f), FVector(LX, LY, F.Z + B::FarTop + 88.f), WP) && WP.Z > F.Z + B::NearTop;
			const TCHAR* Who = bOnClient ? TEXT("client") : TEXT("hote");
			const float Dist = static_cast<float>(FVector::Dist2D(C->GetActorLocation(), Bridge->GetActorLocation())) / 100.f;
			Note(FString::Printf(TEXT("%s (a %.0f m de la passerelle) : tablier pose %s (progression %.2f en %.1f s), sol du tablier au milieu de l'interruption %s, navigation des entites par le tablier %s ; Tick vu %s"),
				Who, Dist, V6::Oui(Bridge->GetModuleProgress() >= 0.99f), Bridge->GetModuleProgress(), StepTime, V6::Oui(bDeck), V6::Oui(bNav), V6::Oui(V6::S.bTickSeen)),
				Bridge->GetModuleProgress() < 0.99f || !bDeck || (!bOnClient && !bNav));
			V6::Put(OutDir, TEXT("passerelle"), TEXT("posee"), FString::FromInt(Bridge->GetModuleProgress() >= 0.99f ? 1 : 0), TEXT("egal"));
			return true;
		});
		LimitLastStep(30.f);

		// ============================================================================================= F. entrees maintenues
		Barrier(60, TEXT("inventaire, pause, touches et perte de focus"));
		GoLevel(6, V6::LidarSeed, TEXT("entrees maintenues"));
		// Une machine a la fois : l'hote (etape 61), puis le client de la place 1 (etape 63)
		auto InputTurn = [this, bOnClient](int32 TurnSlot, int32 OpenPhase, int32 DoneMark)
		{
			Add(FString::Printf(TEXT("v4.16 entrees : tour de la place %d"), TurnSlot), 0.f, [this, bOnClient, TurnSlot, OpenPhase, DoneMark]()
			{
				ABRWorld* W = GetBRWorld();
				ABRCharacter* C = GetPlayer();
				ABRPlayerController* PC = GetPC();
				if (!W || !C || !PC)
				{
					return false;
				}
				V6::Keep(C);
				const bool bMine = V6::S.Slot == TurnSlot && (TurnSlot == 0 ? !bOnClient : bOnClient);
				if (!bMine)
				{
					if (!bOnClient)
					{
						// Hote : attend la fin du tour du client
						V6::FMate Turn;
						if (!V6::MateAt(W, TurnSlot, Turn))
						{
							Skip(FString::Printf(TEXT("entrees : aucun client a la place %d"), TurnSlot));
							V6::SetHostPhase(GetWorld(), OpenPhase + 1);
							return true;
						}
						V6::SetHostPhase(GetWorld(), OpenPhase);
						if (Turn.Mark < DoneMark && StepTime < 150.f)
						{
							return false;
						}
						V6::SetHostPhase(GetWorld(), OpenPhase + 1);
						return true;
					}
					return V6::HostPhase(GetWorld()) >= OpenPhase + 1 || StepTime > 200.f;
				}
				if (bOnClient && V6::HostPhase(GetWorld()) < OpenPhase && StepTime < 150.f)
				{
					return false;
				}
				ABRLidar* L = C->GetLidar();
				const double Now = FPlatformTime::Seconds();
				const double Since = Now - V6::S.InClock;
				auto Next = [Now](int32 To)
				{
					V6::S.InPhase = To;
					V6::S.InClock = Now;
				};
				const TCHAR* Who = bOnClient ? TEXT("client") : TEXT("hote");
				bool bHum = false;
				bool bHasSound = false;
				if (L)
				{
					bHum = V6::LidarHum(L, bHasSound);
				}
				switch (V6::S.InPhase)
				{
				case 0:
					if (!L && StepTime < 10.f)
					{
						return false;
					}
					if (!L || !FApp::CanEverRender())
					{
						Skip(FString::Printf(TEXT("%s : entrees maintenues NON VERIFIEES (%s)"), Who, !L ? TEXT("scanner absent") : TEXT("rendu nul, -nullrhi")));
						Next(99);
						return false;
					}
					V6::S.Keys.Reset();
					for (int32 A = 0; A < BRKeys::NumActions(); ++A)
					{
						for (int32 K = 0; K < BRKeys::SlotsPerAction; ++K)
						{
							V6::S.Keys.Add(BRKeys::GetKey(static_cast<EBRAction>(A), K));
						}
					}
					V6::S.bKeysSaved = true;
					V6::S.KeyFocus = BRKeys::GetKey(EBRAction::LidarFocus, 0);
					V6::S.KeyInteract = BRKeys::GetKey(EBRAction::Interact, 0);
					V6::S.KeySprint = BRKeys::GetKey(EBRAction::Sprint, 0);
					V6::S.InputProblems = 0;
					V6::S.bScanSound = false;
					if (!V6::S.KeyFocus.IsValid() || !V6::S.KeyInteract.IsValid() || !V6::S.KeySprint.IsValid())
					{
						Skip(FString::Printf(TEXT("%s : entrees maintenues NON VERIFIEES (touche du scan, d'interaction ou de course non affectee)"), Who));
						Next(99);
						return false;
					}
					if (PC->IsInventoryOpen())
					{
						PC->SetInventoryOpen(false);
					}
					if (PC->IsPauseMenuOpen())
					{
						PC->TogglePause();
					}
					V6::LookUp(C);
					Next(10);
					return false;
				// ------------------------------------------------------------------------- inventaire pendant le scan
				case 10:
					V6::Inject(PC, V6::S.KeyFocus, IE_Pressed);
					Next(11);
					return false;
				case 11:
					if (Since < 0.4)
					{
						return false;
					}
					if (!L->IsFocusing())
					{
						Skip(FString::Printf(TEXT("%s : le scan ne demarre pas par la touche simulee (%s) : inventaire, pause et touches NON VERIFIES"), Who, *V6::S.KeyFocus.ToString()));
						V6::Inject(PC, V6::S.KeyFocus, IE_Released);
						Next(40);
						return false;
					}
					Note(FString::Printf(TEXT("%s : scan focalise par la touche %s : en marche oui, son %s"), Who, *V6::S.KeyFocus.ToString(), bHasSound ? V6::Oui(bHum) : TEXT("absent (aucun son)")));
					// v4.16 (relecture) : sans son a jouer, "son arrete" ne prouve rien : NON VERIFIE (avant : compte comme arrete)
					V6::S.bScanSound = bHasSound;
					if (!bHasSound)
					{
						Skip(FString::Printf(TEXT("%s : son du scan NON VERIFIE : le scanner n'a aucun son a jouer pendant le scan (arret du son non juge)"), Who));
					}
					PC->SetInventoryOpen(true);
					Next(12);
					return false;
				case 12:
					if (Since < 0.3)
					{
						return false;
					}
					Note(FString::Printf(TEXT("%s : inventaire ouvert pendant le scan maintenu : scan arrete %s, son arrete %s"), Who, V6::Oui(!L->IsFocusing()),
						V6::S.bScanSound ? V6::Oui(!bHum) : TEXT("non juge (aucun son)")), L->IsFocusing() || bHum);
					PC->SetInventoryOpen(false);
					Next(13);
					return false;
				case 13:
					if (Since < 0.6)
					{
						return false;
					}
					Note(FString::Printf(TEXT("%s : inventaire ferme, touche toujours tenue : le scan ne reprend pas seul %s"), Who, V6::Oui(!L->IsFocusing())), L->IsFocusing());
					// v4.16 : la touche tenue (X par defaut) se repete au clavier ; la premiere repetition apres la purge des touches
					// devenait un nouvel appui (Unreal 5.8) et relancait le scan
					V6::S.RepeatsBefore = PC->IgnoredStaleRepeats;
					V6::Inject(PC, V6::S.KeyFocus, IE_Repeat);
					Next(16);
					return false;
				case 16:
					if (Since < 0.5)
					{
						return false;
					}
					Note(FString::Printf(TEXT("%s : inventaire ferme, repetition de la touche toujours tenue : scan repris seul %s (attendu non ; v4.15 : oui), repetition(s) ignoree(s) %d"),
						Who, L->IsFocusing() ? TEXT("OUI") : TEXT("non"), PC->IgnoredStaleRepeats - V6::S.RepeatsBefore), L->IsFocusing());
					V6::Inject(PC, V6::S.KeyFocus, IE_Released);
					Next(14);
					return false;
				case 14:
					if (Since < 0.25)
					{
						return false;
					}
					V6::Inject(PC, V6::S.KeyFocus, IE_Pressed);
					Next(15);
					return false;
				case 15:
					if (Since < 0.4)
					{
						return false;
					}
					Note(FString::Printf(TEXT("%s : nouvel appui apres l'inventaire : scan en marche %s"), Who, V6::Oui(L->IsFocusing())), !L->IsFocusing());
					V6::Inject(PC, V6::S.KeyFocus, IE_Released);
					Next(20);
					return false;
				// ------------------------------------------------------------------------- pause avec interaction et scan
				case 20:
					if (Since < 0.3)
					{
						return false;
					}
					V6::Inject(PC, V6::S.KeyInteract, IE_Pressed);
					V6::Inject(PC, V6::S.KeyFocus, IE_Pressed);
					Next(21);
					return false;
				case 21:
					if (Since < 0.4)
					{
						return false;
					}
					if (!C->bInteractHeld || !L->IsFocusing())
					{
						Skip(FString::Printf(TEXT("%s : pause : interaction maintenue %s, scan %s avant la pause (touches simulees) : essai incomplet"), Who, V6::Oui(C->bInteractHeld),
							V6::Oui(L->IsFocusing())));
					}
					PC->TogglePause();
					Next(22);
					return false;
				case 22:
					if (Since < 0.4)
					{
						return false;
					}
					Note(FString::Printf(TEXT("%s : menu pause ouvert %s, interaction maintenue arretee %s (v4.15 : continuait sous la pause), scan arrete %s, son arrete %s"), Who,
						V6::Oui(PC->IsPauseMenuOpen()), V6::Oui(!C->bInteractHeld), V6::Oui(!L->IsFocusing()), V6::S.bScanSound ? V6::Oui(!bHum) : TEXT("non juge (aucun son)")),
						C->bInteractHeld || L->IsFocusing() || bHum);
					PC->TogglePause();
					Next(23);
					return false;
				case 23:
					if (Since < 0.6)
					{
						return false;
					}
					Note(FString::Printf(TEXT("%s : pause fermee, touches toujours tenues : ni interaction ni scan sans nouvel appui %s"), Who, V6::Oui(!C->bInteractHeld && !L->IsFocusing())),
						C->bInteractHeld || L->IsFocusing());
					V6::Inject(PC, V6::S.KeyInteract, IE_Released);
					V6::Inject(PC, V6::S.KeyFocus, IE_Released);
					Next(30);
					return false;
				// ------------------------------------------------------------------------- touche ajoutee pendant le scan
				case 30:
					if (Since < 0.3)
					{
						return false;
					}
					V6::Inject(PC, V6::S.KeyFocus, IE_Pressed);
					Next(31);
					return false;
				case 31:
					if (Since < 0.4)
					{
						return false;
					}
					PC->SetInventoryOpen(true);
					BRKeys::SetKey(EBRAction::LidarFocus, 2, V6::ExtraKey());
					BRKeys::Save();
					Next(32);
					return false;
				case 32:
					if (Since < 0.4)
					{
						return false;
					}
					// v4.16 (relecture) : le son aussi (avant : non verifie a cette etape)
					Note(FString::Printf(TEXT("%s : touche %s ajoutee au scan pendant l'appui : entrees reconstruites %s, scan arrete %s, son arrete %s"), Who, *V6::ExtraKey().ToString(),
						V6::Oui(PC->MappingRevision == BRKeys::Revision()), V6::Oui(!L->IsFocusing()), V6::S.bScanSound ? V6::Oui(!bHum) : TEXT("non juge (aucun son)")),
						L->IsFocusing() || bHum);
					PC->SetInventoryOpen(false);
					Next(33);
					return false;
				case 33:
					if (Since < 0.6)
					{
						return false;
					}
					Note(FString::Printf(TEXT("%s : apres le changement de touches, touche toujours tenue : pas de reprise seule %s"), Who, V6::Oui(!L->IsFocusing())), L->IsFocusing());
					V6::Inject(PC, V6::S.KeyFocus, IE_Released);
					Next(34);
					return false;
				case 34:
					if (Since < 0.25)
					{
						return false;
					}
					V6::Inject(PC, V6::ExtraKey(), IE_Pressed);
					Next(35);
					return false;
				case 35:
					if (Since < 0.4)
					{
						return false;
					}
					Note(FString::Printf(TEXT("%s : nouvelle touche %s : scan en marche %s"), Who, *V6::ExtraKey().ToString(), V6::Oui(L->IsFocusing())), !L->IsFocusing());
					V6::Inject(PC, V6::ExtraKey(), IE_Released);
					BRKeys::SetKey(EBRAction::LidarFocus, 2, V6::S.Keys.IsValidIndex(static_cast<int32>(EBRAction::LidarFocus) * BRKeys::SlotsPerAction + 2)
						? V6::S.Keys[static_cast<int32>(EBRAction::LidarFocus) * BRKeys::SlotsPerAction + 2] : FKey());
					BRKeys::Save();
					Next(40);
					return false;
				// ------------------------------------------------------------------------- vraie perte de focus de la fenetre
				case 40:
					if (Since < 0.4)
					{
						return false;
					}
					if (!V6::NativeWindow().IsValid())
					{
						Skip(FString::Printf(TEXT("%s : perte de focus NON VERIFIEE (aucune fenetre native)"), Who));
						Next(99);
						return false;
					}
					V6::Activate();
					Next(41);
					return false;
				case 41:
					if (!V6::AppActive() && Since < 4.0)
					{
						return false;
					}
					if (!V6::AppActive())
					{
						Skip(FString::Printf(TEXT("%s : perte de focus NON VERIFIEE : la fenetre du jeu n'a pas obtenu le premier plan (autre fenetre au premier plan)"), Who));
						Next(99);
						return false;
					}
					V6::Inject(PC, V6::S.KeyFocus, IE_Pressed);
					V6::Inject(PC, V6::S.KeySprint, IE_Pressed);
					V6::Inject(PC, V6::S.KeyInteract, IE_Pressed);
					Next(42);
					return false;
				case 42:
					if (Since < 0.5)
					{
						return false;
					}
					Note(FString::Printf(TEXT("%s : avant la perte de focus (fenetre active oui) : scan %s, course %s, interaction maintenue %s"), Who, V6::Oui(L->IsFocusing()),
						V6::Oui(C->bWantsSprint), V6::Oui(C->bInteractHeld)));
					V6::MinimizeWindow();
					Next(43);
					return false;
				case 43:
					if (V6::AppActive() && Since < 4.0)
					{
						return false;
					}
					if (V6::AppActive())
					{
						Skip(FString::Printf(TEXT("%s : perte de focus NON VERIFIEE : la fenetre reduite reste active pour Slate (FSlateApplication::IsActive)"), Who));
						V6::Activate();
						Next(47);
						return false;
					}
					if (Since < 0.5)
					{
						return false;
					}
					Note(FString::Printf(TEXT("%s : fenetre reduite par le test (FSlateApplication::IsActive faux) : scan arrete %s, son arrete %s, course arretee %s, interaction maintenue arretee %s"),
						Who, V6::Oui(!L->IsFocusing()), V6::S.bScanSound ? V6::Oui(!bHum) : TEXT("non juge (aucun son)"), V6::Oui(!C->bWantsSprint), V6::Oui(!C->bInteractHeld)),
						L->IsFocusing() || bHum || C->bWantsSprint || C->bInteractHeld);
					V6::Activate();
					Next(44);
					return false;
				case 44:
					if (!V6::AppActive() && Since < 5.0)
					{
						return false;
					}
					if (!V6::AppActive())
					{
						Skip(FString::Printf(TEXT("%s : retour au premier plan refuse par le systeme : nouvel appui apres Alt+Tab NON VERIFIE"), Who));
						Next(47);
						return false;
					}
					// Touches toujours tenues au retour : le clavier repete (IE_Repeat) ; elles restent ignorees jusqu'au relachement
					V6::S.RepeatsBefore = PC->IgnoredStaleRepeats;
					V6::Inject(PC, V6::S.KeySprint, IE_Repeat);
					V6::Inject(PC, V6::S.KeyFocus, IE_Repeat);
					V6::Inject(PC, V6::S.KeyInteract, IE_Repeat);
					Next(45);
					return false;
				case 45:
					if (Since < 0.5)
					{
						return false;
					}
					Note(FString::Printf(TEXT("%s : retour au premier plan, touches toujours tenues (repetition) : scan repris seul %s, course reprise seule %s, interaction reprise seule %s "
						"(attendu non ; v4.15 : la repetition devenait un nouvel appui), repetition(s) ignoree(s) %d/3"), Who, L->IsFocusing() ? TEXT("OUI") : TEXT("non"),
						C->bWantsSprint ? TEXT("OUI") : TEXT("non"), C->bInteractHeld ? TEXT("OUI") : TEXT("non"), PC->IgnoredStaleRepeats - V6::S.RepeatsBefore),
						L->IsFocusing() || C->bWantsSprint || C->bInteractHeld);
					V6::Inject(PC, V6::S.KeySprint, IE_Released);
					V6::Inject(PC, V6::S.KeyFocus, IE_Released);
					V6::Inject(PC, V6::S.KeyInteract, IE_Released);
					Next(46);
					return false;
				case 46:
					if (Since < 0.3)
					{
						return false;
					}
					V6::Inject(PC, V6::S.KeyFocus, IE_Pressed);
					Next(48);
					return false;
				case 47:
					V6::Inject(PC, V6::S.KeySprint, IE_Released);
					V6::Inject(PC, V6::S.KeyFocus, IE_Released);
					V6::Inject(PC, V6::S.KeyInteract, IE_Released);
					Next(99);
					return false;
				case 48:
					if (Since < 0.4)
					{
						return false;
					}
					Note(FString::Printf(TEXT("%s : nouvel appui apres le retour au premier plan : scan en marche %s"), Who, V6::Oui(L->IsFocusing())), !L->IsFocusing());
					V6::Inject(PC, V6::S.KeyFocus, IE_Released);
					Next(99);
					return false;
				default:
					break;
				}
				// Fin du tour : touches d'origine, menus fermes
				if (Since < 0.3)
				{
					return false;
				}
				if (V6::S.bKeysSaved)
				{
					int32 Idx = 0;
					for (int32 A = 0; A < BRKeys::NumActions(); ++A)
					{
						for (int32 K = 0; K < BRKeys::SlotsPerAction; ++K)
						{
							if (V6::S.Keys.IsValidIndex(Idx) && BRKeys::GetKey(static_cast<EBRAction>(A), K) != V6::S.Keys[Idx])
							{
								BRKeys::SetKey(static_cast<EBRAction>(A), K, V6::S.Keys[Idx]);
							}
							++Idx;
						}
					}
					BRKeys::Save();
					V6::S.bKeysSaved = false;
				}
				if (PC->IsInventoryOpen())
				{
					PC->SetInventoryOpen(false);
				}
				if (PC->IsPauseMenuOpen())
				{
					PC->TogglePause();
				}
				V6::S.InPhase = 0;
				if (bOnClient)
				{
					V6::Mark(GetPC(), DoneMark);
					return V6::HostPhase(GetWorld()) >= OpenPhase + 1 || StepTime > 200.f;
				}
				V6::SetHostPhase(GetWorld(), OpenPhase + 1);
				return true;
			});
			LimitLastStep(220.f);
		};
		InputTurn(0, 61, 61);
		InputTurn(1, 63, 64);

		// ============================================================================================= G. chute puis reveil
		// v4.16 (relecture) : le client de la place 1 tombe dans une fosse de l'aile LiDAR (Level 6), puis se reveille au point de
		// depart : l'hote ne doit constater aucune seconde chute (v4.15, ServerPlacedAt : sa position connue restait celle du fond
		// de la fosse dans l'image du reveil, et le joueur a peine reveille etait tue de nouveau), ni le garder en chargement.
		// Etapes : place 1 en place 67 (68 : essai impossible), reveille 69 ; hote 67 (chute permise)
		Barrier(66, TEXT("chute dans une fosse puis reveil"));
		Add(TEXT("v4.16 reseau : chute dans une fosse, place"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			if (!bOnClient)
			{
				V6::FMate Victim;
				V6::S.bSkipBlock = !V6::MateAt(W, 1, Victim);
				if (V6::S.bSkipBlock)
				{
					Skip(TEXT("chute dans une fosse : aucun client (au moins un client requis)"));
				}
				return true;
			}
			if (V6::S.Slot != 1)
			{
				V6::S.bVictim = false;
				return true;
			}
			const double Now = FPlatformTime::Seconds();
			if (V6::S.Phase == 0)
			{
				V6::S.bVictim = false;
				V6::ResetDeathTrack();
				// Premier coin perce de l'aile (fosse centree sur le coin de la grille, comme ABRWorld::IsOverPit)
				const BRLidar::FWing& Wing = W->GetLidarWing();
				bool bFound = false;
				for (int32 X = Wing.MinX; X < Wing.MinX + BRLidar::WingCells - 1 && !bFound; ++X)
				{
					for (int32 Y = Wing.MinY; Y < Wing.MinY + BRLidar::WingCells - 1 && !bFound; ++Y)
					{
						if (W->HasPitAtCorner(X, Y))
						{
							V6::S.PitCorner = FIntPoint(X, Y);
							bFound = true;
						}
					}
				}
				if (!W->HasPits() || !bFound)
				{
					Skip(FString::Printf(TEXT("chute dans une fosse : aucune fosse dans l'aile LiDAR de ce niveau (Niveau %d, aile valide %s) : essai NON VERIFIE"), W->GetLevelNumber(),
						V6::Oui(Wing.bValid)));
					V6::Mark(GetPC(), 68);
					return true;
				}
				V6::S.Spot = W->CellCenter(V6::S.PitCorner, C->GetActorLocation().Z + 200.f);
				V6::Hover(C, true);
				BRTestNet::TeleportForServer(C, V6::S.Spot);
				V6::Hover(C, true);
				V6::S.Phase = 1;
				V6::S.Clock = Now;
				return false;
			}
			if (V6::S.Phase == 1)
			{
				float Z = 0.f;
				const FIntPoint Cell = W->WorldToCell(V6::S.Spot);
				if (!W->IsChunkLoaded(W->CellToChunk(Cell)) || !V6::FloorAt(W, C, V6::S.Spot, W->FloorZAt(V6::S.Spot), Z))
				{
					if (Now - V6::S.Clock < 40.0)
					{
						return false;
					}
					V6::Hover(C, false);
					Skip(TEXT("chute dans une fosse : sol de l'aile LiDAR pas construit en 40 s : essai NON VERIFIE"));
					V6::Mark(GetPC(), 68);
					V6::S.Phase = 0;
					return true;
				}
				V6::Hover(C, false);
				V6::S.Place.Start(C, FVector(V6::S.Spot.X, V6::S.Spot.Y, Z + C->GetSimpleCollisionHalfHeight() + 5.f), nullptr, Now);
				V6::S.Phase = 2;
				V6::S.Clock = Now;
				return false;
			}
			if (!V6::S.Place.Update(C, Now) && Now - V6::S.Clock < 10.0)
			{
				return false;
			}
			V6::S.Phase = 0;
			V6::S.bVictim = true;
			C->bGodMode = false;
			V6::Mark(GetPC(), 67);
			return true;
		});
		LimitLastStep(80.f);
		Add(TEXT("v4.16 reseau : chute dans une fosse, chute"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			ABRPlayerController* PC = GetPC();
			if (!W || !C || !PC)
			{
				return false;
			}
			if (!bOnClient)
			{
				if (V6::S.bSkipBlock)
				{
					return true;
				}
				V6::FMate Victim;
				const bool bThere = V6::MateAt(W, 1, Victim);
				if (bThere && Victim.Mark < 67 && StepTime < 90.f)
				{
					return false;
				}
				if (!bThere || Victim.Mark != 67)
				{
					V6::S.bSkipBlock = true; // essai impossible (note chez le client) ou jamais pret
					return true;
				}
				if ((Victim.Pawn->NetFlags & 16) != 0 && StepTime < 100.f)
				{
					return false; // mode invincible quitte (etat envoye a l'hote)
				}
				V6::S.LogFrom = LogLineCount();
				V6::S.FallsBefore = Victim.Pawn->GetServerPitFalls();
				V6::S.FallsAtDeath = -1;
				V6::S.bSeenDead = V6::S.bSeenAwake = false;
				V6::SetHostPhase(GetWorld(), 67);
				return true;
			}
			if (!V6::S.bVictim)
			{
				return true;
			}
			if (V6::HostPhase(GetWorld()) < 67 && StepTime < 110.f)
			{
				return false;
			}
			const double Now = FPlatformTime::Seconds();
			V6::TrackDeath(W, C, Now);
			if (V6::S.Deaths > 0)
			{
				return true;
			}
			if (StepTime > 12.f)
			{
				Note(FString::Printf(TEXT("chute dans une fosse : toujours debout apres 12 s de marche vers la fosse du coin (%d, %d) (Z = %.0f cm)"), V6::S.PitCorner.X, V6::S.PitCorner.Y,
					C->GetActorLocation().Z), true);
				V6::S.bVictim = false;
				C->bGodMode = true;
				V6::Mark(GetPC(), 69);
				return true;
			}
			// Vers le centre de la fosse (coin de la grille), en diagonale depuis la cellule
			const float CellSz = W->CellSize();
			const FVector Hole((V6::S.PitCorner.X + 1) * CellSz, (V6::S.PitCorner.Y + 1) * CellSz, C->GetActorLocation().Z);
			const FVector To = (Hole - C->GetActorLocation()).GetSafeNormal2D();
			PC->SetControlRotation(FRotator(-35.f, To.Rotation().Yaw, 0.f));
			C->AddMovementInput(To, 1.f);
			return false;
		});
		LimitLastStep(130.f);
		Add(TEXT("v4.16 reseau : chute dans une fosse, reveil"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			if (!bOnClient)
			{
				if (V6::S.bSkipBlock)
				{
					V6::S.bSkipBlock = false;
					return true;
				}
				V6::FMate Victim;
				const bool bThere = V6::MateAt(W, 1, Victim);
				if (bThere)
				{
					// Chutes constatees par l'hote a la mort (0 si le client l'a signalee le premier, 1 si l'hote l'a vue)
					if (Victim.Pawn->GetDeathState().bDead && !V6::S.bSeenDead)
					{
						V6::S.bSeenDead = true;
						V6::S.FallsAtDeath = Victim.Pawn->GetServerPitFalls();
					}
					V6::S.bSeenAwake |= V6::S.bSeenDead && !Victim.Pawn->GetDeathState().bDead;
				}
				if (bThere && Victim.Mark < 69 && StepTime < 60.f)
				{
					return false;
				}
				const bool bRefused = LogContains(V6::S.LogFrom, TEXT("reveil refuse"));
				const int32 AtDeath = V6::S.FallsAtDeath >= 0 ? V6::S.FallsAtDeath - V6::S.FallsBefore : -1;
				FString Woke;
				const bool bClean = bThere && V6::WokeCleanly(Victim.Pawn, V6::S.FallsAtDeath >= 0 ? V6::S.FallsAtDeath : V6::S.FallsBefore, Woke);
				Note(FString::Printf(TEXT("hote : chute du client dans une fosse : mort vue %s, chutes constatees a la mort %d (0 : signalee par le client, 1 : vue par l'hote), reveil vu %s, "
					"reveil refuse %s ; apres le reveil : %s (v4.15 : seconde chute a la position du fond de la fosse, le joueur reveille etait tue de nouveau)"), V6::Oui(V6::S.bSeenDead), AtDeath,
					V6::Oui(V6::S.bSeenAwake), bRefused ? TEXT("OUI") : TEXT("non"), *Woke), !V6::S.bSeenDead || !V6::S.bSeenAwake || bRefused || !bClean || AtDeath > 1);
				return true;
			}
			if (!V6::S.bVictim)
			{
				return true;
			}
			const double Now = FPlatformTime::Seconds();
			V6::TrackDeath(W, C, Now);
			const bool bDead = C->IsDead();
			if (!bDead && V6::S.Wakes > 0 && Now - V6::S.WakeAt > 1.5 && Now - V6::S.WakeAt < 4.0)
			{
				C->AddMovementInput(C->GetActorForwardVector(), 1.f);
			}
			if ((bDead || V6::S.Wakes == 0 || Now - V6::S.WakeAt < 4.0) && StepTime < 40.f)
			{
				return false;
			}
			const FVector L = C->GetActorLocation();
			if (V6::S.CauseAtDeath != EBRDeathCause::Fall)
			{
				Skip(FString::Printf(TEXT("chute dans une fosse : mort d'une autre cause (%s) avant la fosse : chute puis reveil NON VERIFIES"), BRDeath::CauseId(V6::S.CauseAtDeath)));
			}
			Note(FString::Printf(TEXT("client tombe dans une fosse : cause %s, delai a terre %.1f s (attendu 6), a terre %d fois (attendu 1), reveils %d, reveil %.1f s apres la mort, "
				"reveil hors des fosses %s (%.0f, %.0f, %.0f)"), BRDeath::CauseId(V6::S.CauseAtDeath), V6::S.DelayAtDeath, V6::S.Deaths, V6::S.Wakes,
				V6::S.Wakes > 0 ? V6::S.WakeAt - V6::S.DeathAt : -1.0, V6::Oui(!W->IsOverPit(L, 60.f) && !bDead), L.X, L.Y, L.Z),
				V6::S.Deaths != 1 || V6::S.Wakes != 1 || bDead || W->IsOverPit(L, 60.f));
			C->bGodMode = true;
			V6::S.bVictim = false;
			V6::Mark(GetPC(), 69);
			return true;
		});
		LimitLastStep(80.f);
		Add(TEXT("v4.16 reseau : materiel absent"), 0.f, [this, bOnClient]()
		{
			if (!bOnClient)
			{
				Skip(TEXT("second ecran, deuxieme machine et quatre joueurs physiques : materiel absent de ce PC (essais a faire a la main)"));
			}
			return true;
		});
	}
	else if (Late == TEXT("porte"))
	{
		// ============================================================================================= arrivee apres la porte
		Barrier(70, TEXT("porte du Level 0 avant l'arrivee d'un client"));
		Add(TEXT("v4.16 tardif : partie d'histoire de l'hote, Level 0"), 2.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRPlayerController* PC = GetPC();
			ABRCharacter* C = GetPlayer();
			if (!W || !PC || !C)
			{
				return false;
			}
			const uint32 Seed = ABRWorld::SeedFromUser(V6::DoorSeed, 0);
			if (!bOnClient && V6::S.Phase == 0)
			{
				if (W->IsTransitioning())
				{
					return false;
				}
				// Partie d'histoire de test (prefixe : les parties du joueur ne sont pas touchees)
				BRSaves::SetTestPrefix(V6::SavePrefix);
				BRSaves::Delete(V6::StorySlot);
				UBRSaveGame* Save = NewObject<UBRSaveGame>();
				Save->SaveName = TEXT("Cooperation v4.16");
				Save->Created = FDateTime::Now();
				Save->CurrentLevel = 0;
				Save->MarkExplored(0);
				Save->SetStory(BRStory::FState());
				const bool bWritten = BRSaves::Write(V6::StorySlot, Save);
				TestOrigSlot = BRSaves::ActiveSlot();
				TestOrigSave = PC->ActiveSave;
				bTestDevSession = PC->bDevSession;
				SwapTestSave(PC, Save, V6::StorySlot);
				BRSaves::ReplaySession() = false;
				PC->bDevSession = false;
				V6::S.bStorySave = true;
				W->SetLinearTestOverride(0);
				W->SetStory(Save->GetStory(), EBRSessionMode::Story, true);
				W->RequestTransition(0, false, Seed);
				Note(FString::Printf(TEXT("hote : partie d'histoire de test ecrite %s (emplacement %d)"), V6::Oui(bWritten), V6::StorySlot), !bWritten);
				V6::S.Phase = 1;
				return false;
			}
			const bool bThere = !W->IsTransitioning() && W->IsLevelReady() && W->GetLevelNumber() == 0 && W->GetSeed() == Seed;
			if ((!bThere || (W->GetIntroElapsed() >= 0.f && W->GetIntroElapsed() < 4.f)) && StepTime < 120.f)
			{
				return false;
			}
			V6::S.Phase = 0;
			C->bGodMode = true;
			Note(FString::Printf(TEXT("%s : Level 0 de la partie (graine %u) %s, session d'histoire %s, reveil du Level 0 joue %s"), bOnClient ? TEXT("client") : TEXT("hote"), W->GetSeed(),
				bThere ? TEXT("charge") : TEXT("NON CHARGE"), V6::Oui(W->IsStorySession()), V6::Oui(W->HasStoryEvent(BRStory::Event::IntroWake))), !bThere);
			return true;
		});
		LimitLastStep(140.f);
		Add(TEXT("v4.16 tardif : porte ouverte, objet rendu au monde"), 1.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRPlayerController* PC = GetPC();
			ABRCharacter* C = GetPlayer();
			if (!W || !PC || !C)
			{
				return false;
			}
			if (bOnClient)
			{
				if (V6::HostPhase(GetWorld()) < 71 && StepTime < 60.f)
				{
					return false;
				}
				V6::S.IntroBefore = W->GetIntroElapsed();
				V6::S.IntroClock = FPlatformTime::Seconds();
				V6::Mark(GetPC(), 71);
				return true;
			}
			W->ServerOpenServiceDoor(C);
			// Un objet rendu au monde (joueur parti avant l'accuse de reception) : le client tardif doit le voir
			W->ServerReturnPickup(0x76416ull, EBRItem::Battery, W->CellCenter(FIntPoint(0, 0), 25.f) + FVector(110.f, 0.f, 0.f));
			const TArray<FBRReturnedPickup> Returned = W->GetReturnedList();
			V6::S.ReturnedId = Returned.Num() > 0 ? Returned.Last().Id : 0;
			PC->WriteActiveSave(true);
			const UBRSaveGame* Reloaded = BRSaves::Load(V6::StorySlot);
			const BRStory::FState Saved = Reloaded ? Reloaded->GetStory() : BRStory::FState();
			const bool bSaved = Reloaded && Saved.DoorX == W->GetServiceDoorCell().X && Saved.DoorY == W->GetServiceDoorCell().Y && Saved.IntroSeed == W->GetSeed();
			Note(FString::Printf(TEXT("hote : porte de service ouverte %s en (%d, %d) ; partie relue : porte sauvegardee (%d, %d) face %d, graine %u : %s ; objet rendu au monde %s"),
				V6::Oui(W->IsServiceDoorOpen()), W->GetServiceDoorCell().X, W->GetServiceDoorCell().Y, Saved.DoorX, Saved.DoorY, Saved.DoorDir, Saved.IntroSeed, V6::Oui(bSaved),
				V6::Oui(V6::S.ReturnedId != 0)), !W->IsServiceDoorOpen() || !bSaved);
			V6::Put(OutDir, TEXT("porte"), TEXT("emplacement"), FString::Printf(TEXT("%d,%d,%d,%d"), W->GetServiceDoorCell().X, W->GetServiceDoorCell().Y,
				W->GetServiceDoorDir().X, W->GetServiceDoorDir().Y), TEXT("egal"));
			V6::S.IntroBefore = W->GetIntroElapsed();
			V6::S.IntroClock = FPlatformTime::Seconds();
			V6::S.Events = W->GetNetStory().Events;
			V6::S.KnownIds.Reset();
			for (const V6::FMate& M : V6::Mates(W))
			{
				V6::S.KnownIds.Add(M.Id);
			}
			V6::SetHostPhase(GetWorld(), 71);
			UE_LOG(LogBackrooms, Display, TEXT("[AutoTest] V416 CLIENT TARDIF ATTENDU (porte)"));
			return true;
		});
		LimitLastStep(70.f);
	}
	else if (Late == TEXT("lidar") || Late == TEXT("passage"))
	{
		// ============================================================================================= arrivee pendant le Level 6
		const bool bPassage = Late == TEXT("passage");
		Barrier(70, TEXT("Level 6 avant l'arrivee d'un client"));
		GoLevel(6, V6::LidarSeed, TEXT("client tardif au Level 6"));
		Add(TEXT("v4.16 tardif : mission du Level 6 avancee par l'hote"), 1.f, [this, bOnClient, bPassage]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W)
			{
				return false;
			}
			if (bOnClient)
			{
				return V6::HostPhase(GetWorld()) >= 71 || StepTime > 60.f;
			}
			TArray<BRM::FSolveStep> Steps;
			if (!V6::Solve(W, Steps))
			{
				Skip(TEXT("Level 6 : solveur sans solution : arrivee tardive non jugee"));
			}
			const BRM::FPlan& P = W->GetMissionPlan();
			int32 Posts = 0;
			int32 Applied = 0;
			int32 RightDoor = -1;
			for (const BRM::FSolveStep& St : Steps)
			{
				if (St.Device >= P.NumDevices)
				{
					break;
				}
				const uint8 Role = P.Devices[St.Device].Role;
				if (!bPassage && (Posts >= 2 || Role == BRM::R_LidarDoor))
				{
					break;
				}
				const uint8 Fb = W->DebugMissionAct(St.Device, St.Action);
				++Applied;
				Posts += (Role == BRM::R_LidarPost && Fb == static_cast<uint8>(BRM::EFeedback::Done)) ? 1 : 0;
				if (bPassage && Role == BRM::R_LidarDoor && Fb == static_cast<uint8>(BRM::EFeedback::Done))
				{
					RightDoor = St.Device;
					break;
				}
			}
			V6::S.CrcBefore = V6::StateCrc(W);
			V6::S.MissionRevBefore = W->GetMissionRev();
			V6::S.Device = RightDoor;
			Note(FString::Printf(TEXT("hote : Level 6 : %d action(s) de la solution jouees, %d poste(s) remis sous tension%s ; plan %08x, etat %08x, revision %d"), Applied, Posts,
				bPassage ? *FString::Printf(TEXT(", bonne porte du passage ouverte : mecanisme %d"), RightDoor) : TEXT(""), BRM::Fingerprint(P), V6::S.CrcBefore,
				V6::S.MissionRevBefore), bPassage ? RightDoor < 0 : Posts < 2);
			V6::Put(OutDir, TEXT("lidar"), TEXT("plan"), FString::Printf(TEXT("%08x"), BRM::Fingerprint(P)), TEXT("egal"));
			V6::Put(OutDir, TEXT("lidar"), TEXT("etat"), FString::Printf(TEXT("%08x"), V6::S.CrcBefore), TEXT("egal"));
			V6::Put(OutDir, TEXT("lidar"), TEXT("ouvertures"), V6::Gates(W), TEXT("egal"));
			V6::Put(OutDir, TEXT("lidar"), TEXT("aile"), FString::Printf(TEXT("%d,%d,%d,%d,%d"), W->GetLidarWing().MinX, W->GetLidarWing().MinY, W->GetLidarWing().OutX,
				W->GetLidarWing().OutY, W->GetLidarWing().LadderSlot), TEXT("egal"));
			V6::S.KnownIds.Reset();
			for (const V6::FMate& M : V6::Mates(W))
			{
				V6::S.KnownIds.Add(M.Id);
			}
			V6::SetHostPhase(GetWorld(), 71);
			UE_LOG(LogBackrooms, Display, TEXT("[AutoTest] V416 CLIENT TARDIF ATTENDU (%s)"), bPassage ? TEXT("passage") : TEXT("lidar"));
			return true;
		});
	}
	else if (Late == TEXT("reconnexion"))
	{
		// ============================================================================================= reconnexion pendant un chargement
		Barrier(80, TEXT("reconnexion pendant un chargement"));
		Add(TEXT("v4.16 reconnexion : objet rendu au monde, chargement du niveau"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			if (bOnClient)
			{
				if (V6::S.Slot != 1)
				{
					return V6::HostPhase(GetWorld()) >= 82 || StepTime > 60.f;
				}
				// Le client de la place 1 se reconnecte des que son chargement commence (le groupe recharge le niveau)
				if (V6::HostPhase(GetWorld()) < 82 && StepTime < 60.f)
				{
					return false;
				}
				if (!W->IsTransitioning() && StepTime < 60.f)
				{
					return false;
				}
				const FURL& Url = GetWorld()->URL;
				const FString Cmd = FString::Printf(TEXT("open %s:%d"), *Url.Host, Url.Port);
				V6::S.ReconnectFromSerial = W->GetLevelSerial();
				for (const FLevelReport& R : Reports)
				{
					for (const FString& N : R.Notes)
					{
						if (!N.StartsWith(TEXT("PROBLEME : ")) && !N.StartsWith(TEXT("NON VERIFIE : ")))
						{
							V6::S.CarriedNotes.Add(N);
						}
					}
				}
				for (const FString& Pb : Problems)
				{
					V6::S.CarriedProblems.Add(Pb);
				}
				for (const FString& Sk : Skipped)
				{
					V6::S.CarriedSkips.Add(Sk);
				}
				V6::S.CarriedNotes.Add(FString::Printf(TEXT("client : reconnexion pendant le chargement (%s, transition %s, preparation %s) : %s"), *GetNameSafe(GetWorld()),
					V6::Oui(W->IsTransitioning()), V6::Oui(W->IsPreparing()), *Cmd));
				V6::S.bReconnectPending = true;
				UE_LOG(LogBackrooms, Display, TEXT("[AutoTest] V416 : reconnexion du client (%s)"), *Cmd);
				// Cet acteur ne conclut pas (EndPlay du changement de carte) : le test continue dans le suivant
				bFinished = true;
				GEngine->Exec(GetWorld(), *Cmd);
				return true;
			}
			V6::FMate Leaver;
			if (!V6::MateAt(W, 1, Leaver))
			{
				Skip(TEXT("reconnexion : aucun client (au moins un client requis)"));
				V6::S.bSkipBlock = true;
				return true;
			}
			// L'objet rendu au monde est pose une fois le niveau recharge (un chargement vide la liste) : le client qui revient
			// doit le voir (etape suivante)
			V6::S.KnownIds.Reset();
			for (const V6::FMate& M : V6::Mates(W))
			{
				V6::S.KnownIds.Add(M.Id);
			}
			V6::S.LateId = Leaver.Id;
			V6::S.SerialBefore = W->GetLevelSerial();
			V6::S.ReturnedId = 0;
			V6::S.bBackEarly = false;
			V6::SetHostPhase(GetWorld(), 82);
			W->RequestTransition(W->GetLevelNumber(), false, W->GetSeed());
			V6::S.Clock = FPlatformTime::Seconds();
			Note(FString::Printf(TEXT("hote : rechargement du niveau %d (le client de la place 1 se reconnecte pendant son chargement)"), W->GetLevelNumber()));
			return true;
		});
		LimitLastStep(80.f);
		Add(TEXT("v4.16 reconnexion : depart puis retour du client"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W || bOnClient || V6::S.bSkipBlock)
			{
				return true;
			}
			const TArray<V6::FMate> M = V6::Mates(W);
			bool bOldThere = false;
			const V6::FMate* Back = nullptr;
			for (const V6::FMate& X : M)
			{
				bOldThere |= X.Id == V6::S.LateId;
				if (!V6::S.KnownIds.Contains(X.Id))
				{
					Back = &X;
				}
			}
			if (V6::S.Phase == 0)
			{
				// Le client part, puis le niveau de l'hote est recharge (nouveau chargement, construit, hors transition)
				const bool bReloaded = !W->IsTransitioning() && W->IsLevelReady() && W->GetLevelSerial() != V6::S.SerialBefore;
				if ((bOldThere || !bReloaded) && StepTime < 60.f)
				{
					return false;
				}
				Note(FString::Printf(TEXT("hote : client de la place 1 parti %s, niveau recharge %s (chargement %d, avant %d)"), V6::Oui(!bOldThere), V6::Oui(bReloaded),
					W->GetLevelSerial(), V6::S.SerialBefore), bOldThere || !bReloaded);
				if (bReloaded)
				{
					// Objet rendu au monde pendant que le client se reconnecte : il le recoit avec l'etat du monde, avant d'avoir un
					// niveau (v4.15 : jamais pose chez lui)
					V6::S.bBackEarly = Back != nullptr && !Back->Pawn->IsLevelLoading();
					W->ServerReturnPickup(0x76417ull, EBRItem::Battery, W->CellCenter(FIntPoint(0, 0), 25.f) + FVector(0.f, 110.f, 0.f));
					const TArray<FBRReturnedPickup> Returned = W->GetReturnedList();
					V6::S.ReturnedId = Returned.Num() > 0 ? Returned.Last().Id : 0;
					Note(FString::Printf(TEXT("hote : objet rendu au monde apres le rechargement %s (client deja revenu et pret : %s)"), V6::Oui(V6::S.ReturnedId != 0),
						V6::S.bBackEarly ? TEXT("oui") : TEXT("non")), V6::S.ReturnedId == 0);
				}
				V6::S.Phase = 1;
				return false;
			}
			if ((!Back || Back->Mark < 85) && StepTime < 240.f)
			{
				return false;
			}
			V6::S.Phase = 0;
			if (!Back)
			{
				Note(TEXT("hote : client de la place 1 pas revenu en 240 s"), true);
				V6::S.bSkipBlock = true;
				return true;
			}
			if (V6::S.bBackEarly)
			{
				Skip(TEXT("reconnexion : le client etait deja revenu quand l'objet a ete rendu au monde : affichage d'un objet recu avant le niveau non reproduit"));
			}
			V6::S.LateId = Back->Id;
			Note(FString::Printf(TEXT("hote : client revenu (nouvel identifiant %d) : encore en chargement chez l'hote %s, a terre %s, chutes constatees %d (attendu 0)"), Back->Id,
				Back->Pawn->IsLevelLoading() ? TEXT("OUI") : TEXT("non"), Back->Pawn->GetDeathState().bDead ? TEXT("OUI") : TEXT("non"), Back->Pawn->GetServerPitFalls()),
				Back->Pawn->IsLevelLoading() || Back->Pawn->GetDeathState().bDead || Back->Pawn->GetServerPitFalls() != 0);
			return true;
		});
		LimitLastStep(320.f);
		Add(TEXT("v4.16 reconnexion : position recue, aucune chute imputee"), 1.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W || bOnClient)
			{
				return true;
			}
			if (!V6::S.bSkipBlock)
			{
				const V6::FMate* Back = nullptr;
				const TArray<V6::FMate> M = V6::Mates(W);
				for (const V6::FMate& X : M)
				{
					Back = X.Id == V6::S.LateId ? &X : Back;
				}
				if (Back && Back->Mark < 86 && StepTime < 40.f)
				{
					return false;
				}
				if (Back)
				{
					Note(FString::Printf(TEXT("hote : client reconnecte apres ses premiers pas : position d'avant le placement %s, en chargement %s, a terre %s, chutes %d (attendu 0)"),
						Back->Pawn->HasStaleServerPosition() ? TEXT("ENCORE") : TEXT("non"), Back->Pawn->IsLevelLoading() ? TEXT("OUI") : TEXT("non"),
						Back->Pawn->GetDeathState().bDead ? TEXT("OUI") : TEXT("non"), Back->Pawn->GetServerPitFalls()),
						Back->Pawn->HasStaleServerPosition() || Back->Pawn->IsLevelLoading() || Back->Pawn->GetDeathState().bDead || Back->Pawn->GetServerPitFalls() != 0);
				}
				V6::Put(OutDir, TEXT("reconnexion"), TEXT("niveau"), FString::Printf(TEXT("%d/%u/%d"), W->GetLevelNumber(), W->GetSeed(), W->GetLevelSerial()), TEXT("egal"));
			}
			V6::SetHostPhase(GetWorld(), 87);
			return true;
		});
		LimitLastStep(60.f);
	}
	else
	{
		Note(FString::Printf(TEXT("scenario -BRNetV416Late=%s inconnu (porte, lidar, passage, reconnexion)"), *Late), true);
	}

	// ================================================================================================= arrivee tardive (suite)
	if (Late == TEXT("porte") || Late == TEXT("lidar") || Late == TEXT("passage"))
	{
		const bool bDoor = Late == TEXT("porte");
		const bool bPassage = Late == TEXT("passage");
		// Hote : attend le client tardif (lance par run_nettest.ps1 a la ligne "V416 CLIENT TARDIF ATTENDU") et sa fin de chargement
		Add(TEXT("v4.16 tardif : arrivee du client"), 0.f, [this, bOnClient]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W)
			{
				return false;
			}
			if (bOnClient)
			{
				return true;
			}
			const V6::FMate* New = nullptr;
			const TArray<V6::FMate> M = V6::Mates(W);
			for (const V6::FMate& X : M)
			{
				New = !V6::S.KnownIds.Contains(X.Id) ? &X : New;
			}
			if ((!New || New->Pawn->IsLevelLoading()) && StepTime < 360.f)
			{
				return false;
			}
			if (!New)
			{
				Skip(TEXT("client tardif absent en 360 s (lanceur sans -LateJoin ?) : arrivee en cours de partie NON VERIFIEE"));
				V6::S.LateId = -1;
			}
			else
			{
				V6::S.LateId = New->Id;
				Note(FString::Printf(TEXT("hote : client tardif arrive (identifiant %d, place %d), chargement termine chez l'hote %s"), New->Id, New->Slot,
					V6::Oui(!New->Pawn->IsLevelLoading())), New->Pawn->IsLevelLoading());
			}
			V6::SetHostPhase(GetWorld(), 72);
			return true;
		});
		LimitLastStep(380.f);
		// Client tardif : ce qu'il trouve
		Add(TEXT("v4.16 tardif : ce que trouve le client tardif"), 0.f, [this, bOnClient, bDoor, bPassage]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			if (!bOnClient || !V6::Opts().bLate)
			{
				return true;
			}
			const double Now = FPlatformTime::Seconds();
			if (V6::S.Phase == 0)
			{
				// Arrivee : entrees et reveil (aucun reveil du Level 0 n'est rejoue pour celui qui arrive)
				if (StepTime < 3.f)
				{
					return false;
				}
				const float Intro = W->GetIntroElapsed();
				Note(FString::Printf(TEXT("client tardif : arrive au Niveau %d (graine %u, chargement %d), entrees deverrouillees %s, reveil du Level 0 rejoue %s, en chargement %s"),
					W->GetLevelNumber(), W->GetSeed(), W->GetLevelSerial(), V6::Oui(!C->IsInputLocked()), Intro >= 0.f ? TEXT("OUI") : TEXT("non"), C->IsLevelLoading() ? TEXT("OUI") : TEXT("non")),
					C->IsInputLocked() || Intro >= 0.f || C->IsLevelLoading());
				Note(FString::Printf(TEXT("client tardif : personnage recu apres la fin de l'arrivee (chemin v4.16) %s"),
					LogContains(0, TEXT("Personnage place apres la fin de l'arrivee")) ? TEXT("oui") : TEXT("non (personnage recu pendant l'arrivee)")));
				// Objet rendu au monde chez l'hote : present ici
				const TArray<FBRReturnedPickup> Returned = W->GetReturnedList();
				if (bDoor)
				{
					int32 Present = 0;
					for (const FBRReturnedPickup& R : Returned)
					{
						Present += V6::PickupById(GetWorld(), R.Id) ? 1 : 0;
					}
					if (Returned.Num() == 0)
					{
						Skip(TEXT("client tardif : liste des objets rendus au monde vide (attendu 1 de l'hote) : affichage non juge"));
					}
					else
					{
						Note(FString::Printf(TEXT("client tardif : objets rendus au monde visibles %d/%d (v4.15 : 0, poses seulement quand la liste changeait)"), Present, Returned.Num()),
							Present != Returned.Num());
					}
				}
				if (bDoor)
				{
					const FBRNetStory& St = W->GetNetStory();
					const FIntPoint HostDir = FIntPoint(St.DoorDir == 0 ? 1 : (St.DoorDir == 1 ? -1 : 0), St.DoorDir == 2 ? 1 : (St.DoorDir == 3 ? -1 : 0));
					const bool bSame = W->HasServiceDoor() && W->GetServiceDoorCell() == FIntPoint(St.DoorX, St.DoorY) && W->GetServiceDoorDir() == HostDir && St.IntroSeed == W->GetSeed();
					Note(FString::Printf(TEXT("client tardif : porte en (%d, %d) face (%d, %d), porte de la partie de l'hote (%d, %d) face %d : meme emplacement %s, reprise de la partie %s ; ouverte %s"),
						W->GetServiceDoorCell().X, W->GetServiceDoorCell().Y, W->GetServiceDoorDir().X, W->GetServiceDoorDir().Y, St.DoorX, St.DoorY, St.DoorDir, V6::Oui(bSame),
						V6::Oui(W->WasServiceDoorRestored()), V6::Oui(W->IsServiceDoorOpen())), !bSame || !W->IsServiceDoorOpen());
					V6::Put(OutDir, TEXT("porte"), TEXT("emplacement"), FString::Printf(TEXT("%d,%d,%d,%d"), W->GetServiceDoorCell().X, W->GetServiceDoorCell().Y,
						W->GetServiceDoorDir().X, W->GetServiceDoorDir().Y), TEXT("egal"));
					// Vers la porte (son chunk se construit autour du joueur)
					const FVector In(-W->GetServiceDoorDir().X, -W->GetServiceDoorDir().Y, 0.f);
					V6::S.Spot = W->GetServiceDoorLocation() + In * 250.f + FVector(0.f, 0.f, 150.f);
					V6::Hover(C, true);
					C->SetActorLocation(V6::S.Spot, false, nullptr, ETeleportType::TeleportPhysics);
					V6::S.Phase = 1;
					V6::S.Clock = Now;
					return false;
				}
				// Level 6 : meme plan, memes etats ; rien n'est remis au debut
				const BRM::FPlan& P = W->GetMissionPlan();
				BRM::FEval E;
				W->GetMissionEval(E);
				int32 PostsOn = 0;
				for (int32 I = 0; I < P.NumDevices; ++I)
				{
					PostsOn += (P.Devices[I].Role == BRM::R_LidarPost && W->GetMissionState().Dev[I] >= P.Devices[I].Positions) ? 1 : 0;
				}
				Note(FString::Printf(TEXT("client tardif : Level 6, plan %08x, etat %08x, revision %d, %d poste(s) sous tension, avis de mission %d (attendu 0 : rien ne recommence)"),
					BRM::Fingerprint(P), V6::StateCrc(W), W->GetMissionRev(), PostsOn, static_cast<int32>(W->GetMissionNotice())), W->GetMissionNotice() != 0 || PostsOn < 2);
				V6::Put(OutDir, TEXT("lidar"), TEXT("plan"), FString::Printf(TEXT("%08x"), BRM::Fingerprint(P)), TEXT("egal"));
				V6::Put(OutDir, TEXT("lidar"), TEXT("etat"), FString::Printf(TEXT("%08x"), V6::StateCrc(W)), TEXT("egal"));
				V6::Put(OutDir, TEXT("lidar"), TEXT("ouvertures"), V6::Gates(W), TEXT("egal"));
				V6::Put(OutDir, TEXT("lidar"), TEXT("aile"), FString::Printf(TEXT("%d,%d,%d,%d,%d"), W->GetLidarWing().MinX, W->GetLidarWing().MinY, W->GetLidarWing().OutX,
					W->GetLidarWing().OutY, W->GetLidarWing().LadderSlot), TEXT("egal"));
				if (!bPassage)
				{
					V6::S.Phase = 9;
					return false;
				}
				// La bonne porte du passage : ouverte ici aussi (battant, collision)
				int32 Right = -1;
				for (int32 I = 0; I < P.NumDevices; ++I)
				{
					Right = (P.Devices[I].Role == BRM::R_LidarDoor && E.Gate[I] >= 255) ? I : Right;
				}
				ABRMissionDevice* Door = W->FindMissionDevice(Right);
				if (!Door)
				{
					Note(TEXT("client tardif : aucune porte du passage ouverte dans l'etat recu"), true);
					V6::S.Phase = 9;
					return false;
				}
				V6::S.Device = Right;
				V6::S.Spot = Door->GetActorLocation() + Door->GetActorForwardVector() * 220.f + FVector(0.f, 0.f, 150.f);
				V6::Hover(C, true);
				C->SetActorLocation(V6::S.Spot, false, nullptr, ETeleportType::TeleportPhysics);
				V6::S.Phase = 5;
				V6::S.Clock = Now;
				return false;
			}
			if (V6::S.Phase == 1)
			{
				ABRExit* Door = V6::FindServiceDoor(GetWorld());
				if ((!Door || !W->IsChunkLoaded(W->CellToChunk(W->GetServiceDoorCell()))) && Now - V6::S.Clock < 30.0)
				{
					return false;
				}
				V6::Hover(C, false);
				V6::S.Phase = 9;
				if (!Door)
				{
					Note(TEXT("client tardif : porte de service non construite en 30 s"), true);
					return false;
				}
				// Mur perce (arete de la porte), battant ouvert (pose sans animation), passage libre pour la vue et pour la capsule
				const FIntPoint Cell = W->GetServiceDoorCell();
				const FIntPoint Dir = W->GetServiceDoorDir();
				const bool bEdge = Dir.X != 0 ? W->IsServiceDoorEdge(Dir.X > 0 ? Cell.X : Cell.X - 1, Cell.Y, true) : W->IsServiceDoorEdge(Cell.X, Dir.Y > 0 ? Cell.Y : Cell.Y - 1, false);
				const FTransform T = Door->GetActorTransform();
				const float Wall = W->Def().WallThickness;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestV416Door), false, C);
				FHitResult SightHit;
				const bool bSight = !GetWorld()->LineTraceSingleByChannel(SightHit, T.TransformPosition(FVector(150.f, 0.f, 110.f)), T.TransformPosition(FVector(-Wall - 40.f, 0.f, 110.f)),
					ECC_Visibility, Q);
				FHitResult BodyHit;
				const bool bBody = !GetWorld()->SweepSingleByChannel(BodyHit, T.TransformPosition(FVector(150.f, 0.f, 95.f)), T.TransformPosition(FVector(-Wall + 10.f, 0.f, 95.f)),
					FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.f, 86.f), Q);
				// v4.16 (reseau, correctifs) : acteur et composant de l'obstacle (avant : composant seul, "Box" : boite d'une sortie ou
				// d'un mecanisme ?)
				auto HitName = [](const FHitResult& H) { return FString::Printf(TEXT(" (%s, %s)"), *GetNameSafe(H.GetActor()), *GetNameSafe(H.GetComponent())); };
				Note(FString::Printf(TEXT("client tardif : porte construite, battant a %.0f degres (ouverte %s), mur perce %s, vue par l'ouverture libre %s%s, passage de la capsule libre %s%s"),
					Door->GetServiceDoorAngle(), V6::Oui(Door->IsServiceDoorOpen()), V6::Oui(bEdge), V6::Oui(bSight), bSight ? TEXT("") : *HitName(SightHit), V6::Oui(bBody),
					bBody ? TEXT("") : *HitName(BodyHit)),
					!Door->IsServiceDoorOpen() || Door->GetServiceDoorAngle() < 90.f || !bEdge || !bSight || !bBody);
				// v4.16 (reseau, correctifs) : ouverte, la boite d'interaction ne coupe plus la vue (ABRExit::SetServiceDoorOpen) : la
				// porte doit rester prise par la visee (sa tolerance), de pres, au sol, regard sur le centre de la boite
				const UCapsuleComponent* Cap = C->GetCapsuleComponent();
				V6::S.Spot2 = T.TransformPosition(FVector(10.f, 0.f, 105.f));
				V6::PlaceStart(C, T.TransformPosition(FVector(120.f, 0.f, 0.f)) + FVector(0.f, 0.f, (Cap ? Cap->GetScaledCapsuleHalfHeight() : 88.f) + 5.f), &V6::S.Spot2, true);
				V6::S.Phase = 2;
				V6::S.Clock = Now;
				return false;
			}
			if (V6::S.Phase == 2)
			{
				ABRExit* Door = V6::FindServiceDoor(GetWorld());
				const bool bHeld = V6::PlaceHeld(C, true);
				if (AController* Ctrl = C->GetController())
				{
					Ctrl->SetControlRotation((V6::S.Spot2 - C->GetEyeLocation()).Rotation());
				}
				const bool bSeen = Door && C->FocusActor.Get() == Door;
				if ((!bHeld || !bSeen) && Now - V6::S.Clock < 5.0)
				{
					return false;
				}
				V6::S.Phase = 9;
				if (!Door)
				{
					Note(TEXT("client tardif : porte de service disparue avant la visee de la porte ouverte"), true);
					return false;
				}
				const float Dist = static_cast<float>(FVector::Dist2D(C->GetActorLocation(), Door->GetActorLocation())) / 100.f;
				if (!bSeen && !bHeld)
				{
					Skip(FString::Printf(TEXT("client tardif : porte ouverte visee par l'interaction NON VERIFIE : place a 1,2 m devant la porte non tenue en 5 s (a %.1f m, %d replacement(s))"),
						Dist, V6::S.Place.Reapplied));
					return false;
				}
				Note(FString::Printf(TEXT("client tardif : porte ouverte visee par l'interaction a %.1f m : %s (vise : %s)"), Dist, V6::Oui(bSeen),
					*BRTestV412::FocusName(W, C->FocusActor.Get())), !bSeen);
				return false;
			}
			if (V6::S.Phase == 5)
			{
				ABRMissionDevice* Door = W->FindMissionDevice(V6::S.Device);
				if ((!Door || !Door->IsShown()) && Now - V6::S.Clock < 30.0)
				{
					return false;
				}
				V6::Hover(C, false);
				V6::S.Phase = 9;
				if (!Door || !Door->IsShown())
				{
					Note(TEXT("client tardif : porte du passage pas construite en 30 s"), true);
					return false;
				}
				const FTransform T = Door->GetActorTransform();
				FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestV416LidarDoor), false, C);
				FHitResult Hit;
				const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, T.TransformPosition(FVector(90.f, 0.f, 110.f)), T.TransformPosition(FVector(-90.f, 0.f, 110.f)),
					ECC_WorldStatic, Q) && Hit.GetActor() == Door;
				Note(FString::Printf(TEXT("client tardif : bonne porte du passage (mecanisme %d) : battant ouvert %s (progression %.2f), passage libre %s"), V6::S.Device,
					V6::Oui(Door->GetModuleProgress() >= 0.99f), Door->GetModuleProgress(), V6::Oui(!bBlocked)), Door->GetModuleProgress() < 0.99f || bBlocked);
				return false;
			}
			V6::S.Phase = 0;
			V6::Mark(GetPC(), 73);
			return true;
		});
		LimitLastStep(90.f);
		// Hote et premiers clients : rien de rejoue ni de remis a zero par l'arrivee
		Add(TEXT("v4.16 tardif : rien de rejoue chez les autres"), 0.f, [this, bOnClient, bDoor]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			if (bOnClient && V6::Opts().bLate)
			{
				return true;
			}
			if (bOnClient)
			{
				if (V6::HostPhase(GetWorld()) < 74 && StepTime < 400.f)
				{
					return false;
				}
			}
			else
			{
				const V6::FMate* New = nullptr;
				const TArray<V6::FMate> M = V6::Mates(W);
				for (const V6::FMate& X : M)
				{
					New = X.Id == V6::S.LateId ? &X : New;
				}
				if (New && New->Mark < 73 && StepTime < 120.f)
				{
					return false;
				}
			}
			const TCHAR* Who = bOnClient ? TEXT("client") : TEXT("hote");
			if (bDoor)
			{
				const float Intro = W->GetIntroElapsed();
				const double Elapsed = FPlatformTime::Seconds() - V6::S.IntroClock;
				// Le reveil n'est pas relance : le temps ecoule depuis le reveil a continue de croitre
				const bool bNoWake = V6::S.IntroBefore < 0.f ? Intro < 0.f : Intro > V6::S.IntroBefore + Elapsed * 0.5;
				Note(FString::Printf(TEXT("%s : apres l'arrivee du client tardif : reveil du Level 0 relance %s (temps depuis le reveil %.0f s, avant %.0f s), entrees deverrouillees %s, porte ouverte %s"),
					Who, bNoWake ? TEXT("non") : TEXT("OUI"), Intro, V6::S.IntroBefore, V6::Oui(!C->IsInputLocked()), V6::Oui(W->IsServiceDoorOpen())),
					!bNoWake || C->IsInputLocked() || !W->IsServiceDoorOpen());
				if (!bOnClient)
				{
					Note(FString::Printf(TEXT("hote : evenements de l'histoire inchanges %s"), V6::Oui(W->GetNetStory().Events == V6::S.Events)), W->GetNetStory().Events != V6::S.Events);
				}
			}
			else if (!bOnClient)
			{
				const bool bSame = V6::StateCrc(W) == V6::S.CrcBefore && static_cast<int32>(W->GetMissionRev()) == V6::S.MissionRevBefore;
				Note(FString::Printf(TEXT("hote : apres l'arrivee du client tardif : etat de mission %08x (avant %08x), revision %d (avant %d) : aucun retour au debut %s"), V6::StateCrc(W),
					V6::S.CrcBefore, W->GetMissionRev(), V6::S.MissionRevBefore, V6::Oui(bSame)), !bSame);
			}
			if (!bOnClient)
			{
				V6::SetHostPhase(GetWorld(), 74);
			}
			return true;
		});
		LimitLastStep(420.f);
	}

	// ================================================================================================= fin commune
	Add(TEXT("v4.16 reseau : fin"), 1.f, [this, bOnClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (bOnClient)
		{
			if (V6::HostPhase(GetWorld()) < 74 && V6::Opts().bLate && StepTime < 60.f)
			{
				return false;
			}
			V6::Mark(GetPC(), 95);
			if (V6::HostPhase(GetWorld()) < 95 && !V6::Opts().bLate && StepTime < 400.f)
			{
				return false;
			}
			return true;
		}
		for (const V6::FMate& M : V6::Mates(W))
		{
			if (M.Mark < 95 && StepTime < 300.f)
			{
				return false;
			}
		}
		V6::SetHostPhase(GetWorld(), 95);
		return true;
	});
	LimitLastStep(420.f);
	Add(TEXT("v4.16 reseau : comparaison entre machines et fermeture"), 1.f, [this, bOnClient]()
	{
		ABRWorld* W = GetBRWorld();
		if (bOnClient)
		{
			V6::Mark(GetPC(), V6::MarkFinished);
			Finish();
			return true;
		}
		// Les clients ecrivent leur rapport puis quittent la partie
		bool bWaiting = false;
		if (W)
		{
			for (const V6::FMate& M : V6::Mates(W))
			{
				bWaiting |= M.Mark != V6::MarkFinished;
			}
		}
		if (bWaiting && StepTime < 120.f)
		{
			return false;
		}
		TMap<FString, TArray<V6::FPeerValue>> Values;
		const int32 Machines = V6::ReadPeers(V6::S.Since, Values);
		if (Machines < 2)
		{
			Skip(FString::Printf(TEXT("comparaison entre machines NON VERIFIEE : %d fichier(s) V416.txt de cet essai (autre PC, ou clients absents)"), Machines));
		}
		else
		{
			int32 Same = 0;
			for (const TPair<FString, TArray<V6::FPeerValue>>& Pair : Values)
			{
				const TArray<V6::FPeerValue>& Vs = Pair.Value;
				if (Vs.Num() == 0)
				{
					continue;
				}
				FString List;
				for (const V6::FPeerValue& V : Vs)
				{
					List += FString::Printf(TEXT("%s%s=%s"), List.IsEmpty() ? TEXT("") : TEXT(", "), *V.Machine, *V.Value);
				}
				if (Vs[0].Rule.StartsWith(TEXT("somme=")))
				{
					int32 Sum = 0;
					for (const V6::FPeerValue& V : Vs)
					{
						Sum += FCString::Atoi(*V.Value);
					}
					const int32 Want = FCString::Atoi(*Vs[0].Rule.RightChop(6));
					Note(FString::Printf(TEXT("entre machines : %s : somme %d (attendu %d) sur %d machine(s) : %s"), *Pair.Key, Sum, Want, Vs.Num(), *List), Sum != Want);
					continue;
				}
				bool bEqual = true;
				for (const V6::FPeerValue& V : Vs)
				{
					bEqual &= V.Value == Vs[0].Value;
				}
				Same += (bEqual && Vs.Num() > 1) ? 1 : 0;
				Note(FString::Printf(TEXT("entre machines : %s : %s sur %d machine(s) : %s"), *Pair.Key, bEqual ? TEXT("identique") : TEXT("DIFFERENT"), Vs.Num(), *List), !bEqual);
			}
			Note(FString::Printf(TEXT("entre machines : %d machine(s), %d valeur(s) identiques"), Machines, Same));
		}
		// Partie d'histoire de test : la partie d'avant revient, les fichiers de test sont effaces
		if (V6::S.bStorySave)
		{
			if (ABRPlayerController* PC = GetPC())
			{
				SwapTestSave(PC, TestOrigSave, TestOrigSlot);
				PC->bDevSession = bTestDevSession;
			}
			BRSaves::Delete(V6::StorySlot);
			BRSaves::SetTestPrefix(FString());
			V6::S.bStorySave = false;
		}
		if (W)
		{
			W->SetLinearTestOverride(-1);
		}
		Finish();
		return true;
	});
	LimitLastStep(140.f);
}
