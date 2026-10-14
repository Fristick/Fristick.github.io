// v4.13 : test du jeu du Level 6 sans lumiere (scanner LiDAR) : aile et mission placees, scanner local, calibrage au scan
// focalise, mission jouee par de vraies interactions (solveur des bancs), traversee de la zone de trous jusqu'a l'echelle,
// conversion d'une ancienne session (chaine de balises v4.12, en cours ou terminee), sauvegarde et reprise d'une mission
// LiDAR en cours, et en reseau : meme aile et meme etat chez l'hote et le client, points locaux a chaque machine.
// Prepare, non execute ici (pas d'Unreal dans l'environnement de developpement) : lancer -BRAutoTestV413 ou -BRNetTest.
#include "BRAutoTest.h"
#include "BRWorld.h"
#include "BRCharacter.h"
#include "BRPlayerController.h"
#include "BRMission.h"
#include "BRMissionLogic.h"
#include "BRLidar.h"
#include "BRLidarLogic.h"
#include "BRSave.h"
#include "Backrooms.h"

#include "Engine/World.h"

namespace BRM = BRMission;

namespace BRTestLidar
{
	/** Graine du test (-BRSeed equivalente) */
	constexpr uint32 LidarSeed = 4136;

	const TCHAR* Oui(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	int32 FindRoleIndex(const BRM::FPlan& P, uint8 Role)
	{
		for (int32 I = 0; I < P.NumDevices; ++I)
		{
			if (P.Devices[I].Role == Role)
			{
				return I;
			}
		}
		return -1;
	}

	void FaceAt(ABRCharacter* C, const FVector& Target)
	{
		if (AController* Ctrl = C ? C->GetController() : nullptr)
		{
			Ctrl->SetControlRotation((Target - C->GetEyeLocation()).Rotation());
		}
	}

	/** Session du Level 6 sauvegardee avec cet etat de mission (plan courant ou ancien plan v4.12) */
	FBRSessionState MakeSession(uint32 LevelSeed, const BRM::FPlan& P, const BRM::FState& S)
	{
		uint8 Buffer[11 + BRM::MaxDevices];
		const int32 Size = BRM::Serialize(P, S, Buffer, sizeof(Buffer));
		FBRSessionState Session;
		Session.bValid = Size > 0;
		Session.Level = 6;
		Session.Seed = LevelSeed;
		Session.GenVersion = BRM::GenVersion;
		Session.PlaceVersion = BRM::PlaceVersion;
		Session.Mission = TArray<uint8>(Buffer, Size);
		return Session;
	}

	/** Etat d'une session v4.12 de la chaine de balises : plaque lue, Lit balises allumees, ou mission terminee */
	BRM::FState LegacyState(const BRM::FPlan& Old, int32 Lit, bool bSolved)
	{
		BRM::FState OS;
		BRM::InitState(Old, OS);
		OS.Dev[0] = 1;
		for (int32 B = 0; B < Lit; ++B)
		{
			OS.Dev[Old.Params[B]] = Old.Devices[Old.Params[B]].Positions;
		}
		OS.Solved = bSolved ? 1 : 0;
		return OS;
	}
}

void ABRAutoTest::AddV413LidarSteps()
{
	using namespace BRTestLidar;
	const uint32 LevelSeed = ABRWorld::SeedFromUser(LidarSeed, 6);
	AddSeedLoad(6, LidarSeed, TEXT("v4.13 Level 6 LiDAR"));
	Add(TEXT("v4.13 Level 6 : aile, mission, scanner"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || !W->IsLevelReady() || W->GetLevelNumber() != 6)
		{
			return StepTime > 30.f;
		}
		if (!C->GetLidar() && StepTime < 5.f)
		{
			return false; // le scanner est cree au premier Tick du joueur local
		}
		const BRLidar::FWing& Wing = W->GetLidarWing();
		const BRM::FPlan& P = W->GetMissionPlan();
		int32 Doors = 0, DoorModules = 0;
		for (const ABRMissionDevice* D : W->GetMissionDevices())
		{
			if (D && D->GetDeviceDef().Role == BRM::R_LidarDoor)
			{
				++Doors;
				DoorModules += D->GetModule() == BRMech::Module::LidarDoor ? 1 : 0;
			}
		}
		const bool bOk = W->IsLidarLevel() && Wing.bValid && W->IsMissionActive() && P.Level == 6 && P.NumDevices == 15 && Doors == 4 && DoorModules == 4
			&& W->GetLidarMarks().Num() > 0 && !W->IsLidarCalibrated() && C->GetLidar() != nullptr;
		Note(FString::Printf(TEXT("Level 6 : aile au chunk (%d, %d), entree vers (%d, %d), echelle dans l'alcove %d ; mission %s (%d mecanismes), portes %d dont %d dans l'aile, "
			"reperes au sol %d, zone sure (non calibre) %s, scanner local %s"), Wing.ChunkX, Wing.ChunkY, Wing.OutX, Wing.OutY, Wing.LadderSlot + 1,
			Oui(W->IsMissionActive()), P.NumDevices, Doors, DoorModules, W->GetLidarMarks().Num(), Oui(!W->IsLidarCalibrated()), Oui(C->GetLidar() != nullptr)), !bOk);
		// Vision nocturne brouillee : la demande est refusee (pas de changement de reglage)
		if (C->HasCamcorder())
		{
			C->ToggleNightVision();
			Note(FString::Printf(TEXT("vision nocturne au Level 6 : refusee %s"), Oui(!C->IsNightVision())), C->IsNightVision());
		}
		else
		{
			Skip(TEXT("vision nocturne : pas de camescope sur le joueur du test"));
		}
		Shot(TEXT("v413_lidar_arrivee"));
		return true;
	});
	Add(TEXT("v4.13 Level 6 : impulsion"), 0.f, [this]()
	{
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		if (!L)
		{
			Note(TEXT("scanner absent : impulsion non testee"), true);
			return true;
		}
		if (TestMissionPhase != 61)
		{
			ABRLidar::ResetStats();
			TestCountA = L->GetPointCount();
			const bool bFired = L->Pulse();
			TestMissionPhase = 61;
			Note(FString::Printf(TEXT("impulsion tiree %s ; seconde impulsion pendant la recharge refusee %s"), Oui(bFired), Oui(!L->Pulse())), !bFired);
			return false;
		}
		if (StepTime < 0.6f)
		{
			return false; // v4.14 : PulseSeconds (0,2 s) a toute cadence, plus si l'image depasse le plafond par image
		}
		TestMissionPhase = 0;
		const int64 Rays = ABRLidar::Stats.Rays;
		const int32 Points = L->GetPointCount() - TestCountA;
		Note(FString::Printf(TEXT("impulsion : %lld rayons traces (attendu %d), %d points affiches, %d points au plus ; mesures : %s"), Rays, BRLidar::PulseRays, Points,
			BRLidar::MaxPoints, *ABRLidar::Stats.ToString()), Rays < BRLidar::PulseRays || Points <= 0);
		Shot(TEXT("v413_lidar_impulsion"));
		return true;
	});
	Add(TEXT("v4.13 Level 6 : calibrage au scan focalise"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		const int32 Calib = W ? FindRoleIndex(W->GetMissionPlan(), BRM::R_LidarTarget) : -1;
		ABRMissionDevice* D = (W && Calib >= 0) ? W->FindMissionDevice(Calib) : nullptr;
		if (!W || !C || !L || !D)
		{
			Note(TEXT("mire ou scanner absent : calibrage non teste"), true);
			return true;
		}
		if (TestMissionPhase != 62)
		{
			// Devant la mire, a 3 m, dans l'axe (la zone sure)
			const FVector Front = D->GetInteractPoint() + D->GetActorForwardVector() * 300.f;
			C->SetActorLocation(FVector(Front.X, Front.Y, C->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			TestMissionPhase = 62;
			return false;
		}
		FaceAt(C, D->GetInteractPoint());
		L->SetFocus(true);
		const bool bDone = W->IsLidarCalibrated();
		if (!bDone && StepTime < 15.f)
		{
			return false;
		}
		L->SetFocus(false);
		TestMissionPhase = 0;
		Note(FString::Printf(TEXT("calibrage au scan focalise (mire a 3 m, en ligne de vue) : %s en %.1f s ; entites autorisees ensuite %s"), bDone ? TEXT("fait") : TEXT("ECHEC"),
			StepTime, Oui(W->IsLidarCalibrated())), !bDone);
		Shot(TEXT("v413_lidar_calibrage"));
		return true;
	});
	// Mission jouee par de vraies interactions (memes actions que le solveur des bancs), puis l'echelle : la traversee de
	// la zone de trous se fait a pied (une chute serait signalee par le test de marche)
	AddWalkMissionSteps(6, LidarSeed, false);
	Add(TEXT("v4.13 Level 6 : plaques et cages relevees"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || !W->IsMissionActive())
		{
			return true;
		}
		BRM::FEval E;
		W->GetMissionEval(E);
		FVector Static;
		const bool bStatic = W->GetLidarStatic(Static);
		Note(FString::Printf(TEXT("mission du Level 6 : resolue %s, etapes %d/%d %d/%d %d/%d %d/%d, emetteur de parasites actif %s, erreurs commises %d"), Oui(E.bSolved),
			E.Progress[0], E.Goal[0], E.Progress[1], E.Goal[1], E.Progress[2], E.Goal[2], E.Progress[3], E.Goal[3], Oui(bStatic), W->GetMissionState().Mistakes),
			!E.bSolved || !bStatic);
		Shot(TEXT("v413_lidar_passage_ouvert"));
		return true;
	});
	AddWalkExitSteps(6);
	Add(TEXT("v4.13 Level 6 : arrivee au Niveau 8"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if ((!W || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 8) && StepTime < 40.f)
		{
			return false;
		}
		const bool bOk = W && W->GetLevelNumber() == 8 && C && !C->DiedInPit();
		Note(FString::Printf(TEXT("traversee de l'aile et echelle vers le Niveau 8 : %s, sans chute %s ; scanner retire hors du Level 6 %s"), bOk ? TEXT("faite") : TEXT("ECHEC"),
			Oui(C && !C->DiedInPit()), Oui(C && !C->GetLidar())), !bOk);
		return true;
	});
	// Reprise d'une ancienne session (v4.12 : chaine de balises, deux balises allumees) : convertie en postes
	Add(TEXT("v4.13 Level 6 : ancienne session convertie"), 0.f, [this, LevelSeed]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC || W->IsTransitioning())
		{
			return false;
		}
		BRM::FPlan Old;
		BRM::BuildLegacyPlan6(LevelSeed, false, Old);
		BRSaves::PendingResume() = MakeSession(LevelSeed, Old, LegacyState(Old, 2, false));
		TestCountC = W->GetLevelSerial();
		W->RequestTransition(6, false, LevelSeed);
		return true;
	});
	Add(TEXT("v4.13 Level 6 : progression des balises gardee"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 6 || W->GetLevelSerial() == TestCountC)
		{
			return StepTime > 30.f;
		}
		BRM::FEval E;
		W->GetMissionEval(E);
		const bool bOk = W->GetMissionNotice() == 3 && E.Steps[0] == BRM::EStep::Done && E.Progress[1] == 2 && !E.bSolved;
		Note(FString::Printf(TEXT("ancienne session (plaque lue, 2 balises allumees) : avis %d (attendu 3 : progression gardee), calibrage %s, postes %d/4, copie de la sauvegarde demandee"),
			W->GetMissionNotice(), Oui(E.Steps[0] == BRM::EStep::Done), E.Progress[1]), !bOk);
		return true;
	});
	// Sauvegarde et reprise d'une mission LiDAR en cours (format v4.13) : meme aile, meme progression, pas de reprise du debut
	Add(TEXT("v4.13 Level 6 : sauvegarde et reprise"), 0.f, [this, LevelSeed]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 6)
		{
			return StepTime > 30.f;
		}
		BRM::FEval E;
		W->GetMissionEval(E);
		TestCountA = E.Progress[1];
		TestCountB = W->GetLidarWing().ChunkX * 1000 + W->GetLidarWing().ChunkY;
		BRSaves::PendingResume() = MakeSession(LevelSeed, W->GetMissionPlan(), W->GetMissionState());
		TestCountC = W->GetLevelSerial();
		W->RequestTransition(6, false, LevelSeed);
		return true;
	});
	Add(TEXT("v4.13 Level 6 : sauvegarde et reprise, verification"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 6 || W->GetLevelSerial() == TestCountC)
		{
			return StepTime > 30.f;
		}
		BRM::FEval E;
		W->GetMissionEval(E);
		const bool bSameWing = W->GetLidarWing().ChunkX * 1000 + W->GetLidarWing().ChunkY == TestCountB;
		const bool bOk = bSameWing && W->GetMissionNotice() != 2 && E.Steps[0] == BRM::EStep::Done && E.Progress[1] == TestCountA && W->IsLidarCalibrated();
		Note(FString::Printf(TEXT("reprise d'une mission LiDAR en cours : meme aile %s, avis %d (2 = reprise du debut), calibrage %s, postes %d/4 (avant : %d)"), Oui(bSameWing),
			W->GetMissionNotice(), Oui(E.Steps[0] == BRM::EStep::Done), E.Progress[1], TestCountA), !bOk);
		return true;
	});
	// Une ancienne session terminee reste terminee : le Level 6 fini ne bloque jamais la sortie vers le Niveau 8
	Add(TEXT("v4.13 Level 6 : ancienne session terminee"), 0.f, [this, LevelSeed]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		BRM::FPlan Old;
		BRM::BuildLegacyPlan6(LevelSeed, false, Old);
		BRSaves::PendingResume() = MakeSession(LevelSeed, Old, LegacyState(Old, 0, true));
		TestCountC = W->GetLevelSerial();
		W->RequestTransition(6, false, LevelSeed);
		return true;
	});
	Add(TEXT("v4.13 Level 6 : ancienne session terminee, verification"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 6 || W->GetLevelSerial() == TestCountC)
		{
			return StepTime > 30.f;
		}
		BRM::FEval E;
		W->GetMissionEval(E);
		const bool bOk = E.bSolved && W->GetMissionNotice() == 3;
		Note(FString::Printf(TEXT("ancienne session terminee : mission terminee %s, avis %d (attendu 3), sortie vers le Niveau 8 ouverte %s"), Oui(E.bSolved), W->GetMissionNotice(),
			Oui(E.bSolved)), !bOk);
		return true;
	});
}

void ABRAutoTest::AddNetV413LidarSteps(bool bClient)
{
	using namespace BRTestLidar;
	const uint32 LevelSeed = ABRWorld::SeedFromUser(LidarSeed, 6);
	Add(TEXT("v4.13 reseau : Level 6"), 1.f, [this, bClient, LevelSeed]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (!bClient && TestMissionPhase != 63)
		{
			if (W->IsTransitioning())
			{
				return false;
			}
			TestMissionPhase = 63;
			W->RequestTransition(6, false, LevelSeed);
			return false;
		}
		if ((W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 6) && StepTime < 60.f)
		{
			return false;
		}
		TestMissionPhase = 0;
		const BRLidar::FWing& Wing = W->GetLidarWing();
		// Meme aile et meme plan chez chacun (graine repliquee) ; empreinte du plan dans le journal des deux machines
		Note(FString::Printf(TEXT("%s : Level 6 charge %s ; aile (%d, %d)-(%d, %d) entree (%d, %d) echelle %d ; plan %08x"), bClient ? TEXT("client") : TEXT("hote"),
			Oui(W->GetLevelNumber() == 6), Wing.MinX, Wing.MinY, Wing.MinX + BRLidar::WingCells - 1, Wing.MinY + BRLidar::WingCells - 1, Wing.OutX, Wing.OutY, Wing.LadderSlot,
			BRM::Fingerprint(W->GetMissionPlan())), W->GetLevelNumber() != 6 || !Wing.bValid);
		return true;
	});
	Add(TEXT("v4.13 reseau : points locaux, calibrage partage"), 0.f, [this, bClient]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		ABRLidar* L = C ? C->GetLidar() : nullptr;
		if (!W || !C || W->GetLevelNumber() != 6)
		{
			return true;
		}
		if (!L && StepTime < 5.f)
		{
			return false;
		}
		if (TestMissionPhase != 64 && TestMissionPhase != 65)
		{
			TestMissionPhase = 64;
			// L'hote calibre (action ordinaire, verifiee chez lui) ; le client doit le voir par la replication
			if (!bClient)
			{
				const int32 Calib = FindRoleIndex(W->GetMissionPlan(), BRM::R_LidarTarget);
				if (Calib >= 0)
				{
					W->DebugCompleteMissionDevice(Calib);
				}
			}
		}
		// Une impulsion n'est acceptee qu'une fois le joueur libre d'agir (arrivee dans le niveau, titre) : elle est redemandee
		// jusqu'a ce qu'elle parte, puis ses points se posent sur quelques images
		if (TestMissionPhase == 64 && L && L->Pulse())
		{
			TestMissionPhase = 65;
		}
		const int32 Points = L ? L->GetPointCount() : 0;
		if ((!W->IsLidarCalibrated() || Points <= 0) && StepTime < 20.f)
		{
			return false;
		}
		const bool bPulsed = TestMissionPhase == 65;
		TestMissionPhase = 0;
		Note(FString::Printf(TEXT("%s : scanner local %s, impulsion partie %s, %d points (non repliques), calibrage de l'hote recu %s"), bClient ? TEXT("client") : TEXT("hote"),
			Oui(L != nullptr), Oui(bPulsed), Points, Oui(W->IsLidarCalibrated())), !L || Points <= 0 || !W->IsLidarCalibrated());
		Skip(TEXT("4 joueurs et client arrive en cours de partie : non reproductibles dans ce banc (deux instances, connectees au debut) ; l'etat de mission parvient a un client "
			"tardif par la premiere replication (OnRep_Mission), ses points de scanner sont les siens"));
		return true;
	});
}
