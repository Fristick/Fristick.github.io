// v4.15 (correctifs) : -BRAutoTest -BRAutoTestV415Fix : les seules etapes de non-regression des correctifs v4.15 des places
// des mecanismes et des tests de marche (Saved/TestRuns/analysis : diag_v413_3, diag_v413_4, diag_interchange_verified),
// pour une verification courte :
//   - Niveaux 8 et 9, graine 4111 (niveaux 643828852 et 774538619) : rochers et maison hors des places des mecanismes ;
//     chaque mecanisme a une place debout a portee de la visee et selon la regle de l'hote, essai a blanc depuis elle ;
//   - Level 6, graines 4136, 4126, 6 et 208 : salle de l'aile, aucun avertissement, postes et cages atteignables, poste 1
//     tourne depuis sa place ;
//   - Poolrooms, graine 1217121623 (niveau 125584919) : mission a pied, vanne B depuis l'eau des canaux, sortie par le sas,
//     sans nager ;
//   - v4.15 (correctifs 4) : Niveau 2, graine 4120 (niveau 554144564, vannes 4 a 6 face a la porte de la vapeur) : places des
//     mecanismes (la place trouvee est celle d'ou la visee du client prend le mecanisme, BRTestV412::CheckMissionStand) ;
//     classement d'un blocage de la marche (une creature seule devant le marcheur, puis la meme devant un mur :
//     BRTestV412::ClassifyBlock) et marche vers le mecanisme 4 avec un Smiler immobile pose sur le chemin, dans un couloir
//     sombre (comme final2/full) : ecarte par le test, mecanisme atteint, aucun PROBLEME.
// Les memes etapes font partie de -BRAutoTestV411 (AddMissionSteps : places des mecanismes de chaque niveau), de
// -BRAutoTestV412 (Poolrooms) et de -BRAutoTestV413 (Level 6), donc du test complet : cette suite n'y est pas ajoutee une
// seconde fois. Une verification qui ne peut pas etre faite est notee "NON VERIFIE", jamais comptee comme reussie.
#include "BRAutoTest.h"
#include "BRAutoTestWalk.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BREntity.h"
#include "BRLightLogic.h"
#include "BRMission.h"
#include "BRWorld.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace BRTestV415Fix
{
	/** v4.15 (correctifs 4) : creature posee par les etapes d'un blocage, etat de la marche forcee */
	TWeakObjectPtr<ABREntity> BlockEntity;
	bool bWalking = false;
	bool bLightWas = false;
	int32 BlockWaypoint = -1;

	/** Joueur pose (sans vitesse) en P (au sol), regard vers Dir : mise en place d'une verification, pas une marche */
	void PlaceAt(const ABRWorld* W, ABRCharacter* C, const FVector& P, const FVector& Dir)
	{
		const float Half = C->GetSimpleCollisionHalfHeight();
		C->SetActorLocation(FVector(P.X, P.Y, W->FloorZAt(P) + Half + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* M = C->GetCharacterMovement())
		{
			M->StopMovementImmediately();
		}
		if (AController* Ctrl = C->GetController())
		{
			Ctrl->SetControlRotation(Dir.Rotation());
		}
	}

	/** Creature figee (temps ralenti 50 fois, comme les captures du banc visuel) au sol ou a sa hauteur de vol en P */
	ABREntity* SpawnFrozen(ABRWorld* W, EBREntityKind Kind, const FVector& P)
	{
		const FBREntityInfo& Info = ABREntity::Info(Kind);
		ABREntity* E = W->SpawnEntity(Kind, FVector(P.X, P.Y, W->FloorZAt(P) + (Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 5.f)));
		if (E)
		{
			E->CustomTimeDilation = 0.02f;
		}
		return E;
	}
}

void ABRAutoTest::AddV415FixSteps()
{
	Add(TEXT("v4.15 correctifs : preparation"), 0.f, [this]()
	{
		FLevelReport R;
		R.Title = TEXT("v4.15 (correctifs) : places des mecanismes (Niveaux 6, 8, 9), Poolrooms a pied sans nager");
		R.Scene = TEXT("v4.15");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		return true;
	});
	// Niveaux 8 et 9 : la graine de -BRAutoTestV411 (rochers du Niveau 8, perron de la maison du Niveau 9)
	for (const int32 PlaceLevel : { 8, 9 })
	{
		AddSeedLoad(PlaceLevel, 4111, FString::Printf(TEXT("v4.15 correctifs : Niveau %d (graine 4111)"), PlaceLevel));
		AddMissionPlaceSweep(PlaceLevel, false);
	}
	// Level 6 : aile, postes et cages (BRAutoTestV413Lidar.cpp)
	AddV415LidarPlaceSteps();
	// Poolrooms : vanne B et sas a pied (BRAutoTestV412.cpp)
	AddV415PoolroomsSteps();

	// v4.15 (correctifs 4) : Niveau 2, graine 4120 (niveau 554144564) : vannes face a la porte de la vapeur (la graine du Niveau 2
	// de -BRAutoTestV411, 4111, n'a pas cette disposition) ; puis un blocage de la marche par une creature
	AddSeedLoad(2, 4120, TEXT("v4.15 correctifs 4 : Niveau 2 (graine 4120)"));
	AddMissionPlaceSweep(2, false);
	Add(TEXT("v4.15 correctifs 4 : classement d'un blocage (creature seule, creature devant un mur)"), 0.5f, [this]()
	{
		using namespace BRTestV415Fix;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->GetLevelNumber() != 2)
		{
			Skip(TEXT("Niveau 2 non atteint : classement d'un blocage non verifie"));
			return true;
		}
		// Une cellule du couloir avec une face fermee (mur) et une face ouverte sur un couloir qui continue
		const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		const FIntPoint Here = W->WorldToCell(C->GetActorLocation());
		FIntPoint Cell(0, 0), Closed(0, 0), Open(0, 0);
		bool bFound = false;
		for (int32 R = 1; R <= 6 && !bFound; ++R)
		{
			for (int32 X = Here.X - R; X <= Here.X + R && !bFound; ++X)
			{
				for (int32 Y = Here.Y - R; Y <= Here.Y + R && !bFound; ++Y)
				{
					const FIntPoint Ce(X, Y);
					if (!W->IsWalkable(Ce) || W->IsSpawnArea(X, Y) || W->IsMissionCell(X, Y))
					{
						continue;
					}
					for (const FIntPoint& A : Dirs)
					{
						for (const FIntPoint& B : Dirs)
						{
							if (!bFound && A != B && !W->CanStep(Ce, Ce + A) && W->CanStep(Ce - A, Ce) && W->CanStep(Ce, Ce + B) && W->CanStep(Ce + B, Ce + B + B)
								&& W->CanStep(Ce - B, Ce))
							{
								Cell = Ce;
								Closed = A;
								Open = B;
								bFound = true;
							}
						}
					}
				}
			}
		}
		if (!bFound)
		{
			Skip(TEXT("classement d'un blocage : aucune cellule de couloir avec un mur et une suite a moins de 6 cellules"));
			return true;
		}
		// Le marcheur 55 cm avant le centre, une creature 90 cm devant lui (contre lui, sans le toucher), puis la suite du chemin
		const FVector Center = W->CellCenter(Cell, 0.f);
		auto Check = [&](const FIntPoint& D, BRTestV412::FBlock& Out)
		{
			const FVector Dir(D.X, D.Y, 0.f);
			const FVector P0 = Center - Dir * 55.f;
			PlaceAt(W, C, P0, Dir);
			ABREntity* E = SpawnFrozen(W, EBREntityKind::Hound, P0 + Dir * 90.f);
			if (!E)
			{
				return false;
			}
			Out = BRTestV412::ClassifyBlock(W, C, P0 + Dir * 300.f, P0 + Dir * 400.f);
			const bool bFirst = Out.First == E;
			E->Destroy();
			return bFirst;
		};
		BRTestV412::FBlock Wall, Free;
		const bool bWallFirst = Check(Closed, Wall);
		const bool bFreeFirst = Check(Open, Free);
		if (!bWallFirst && !bFreeFirst && !Wall.bBlocked && !Free.bBlocked)
		{
			Skip(FString::Printf(TEXT("classement d'un blocage : creature non creee ou pas touchee (cellule %d, %d)"), Cell.X, Cell.Y));
			return true;
		}
		const bool bOk = bWallFirst && !Wall.bEntity && Wall.Static != nullptr && bFreeFirst && Free.bEntity && Free.Static == nullptr;
		Note(FString::Printf(TEXT("classement d'un blocage, cellule (%d, %d) : creature devant le marcheur dans le couloir libre : %s (%s) ; la meme devant un mur a 140 cm : %s "
			"(%s) (attendu : creature seule, puis decor : le mur reste un blocage du decor malgre la creature)"), Cell.X, Cell.Y, Free.bEntity ? TEXT("creature seule") : TEXT("DECOR"),
			*Free.What, Wall.bEntity ? TEXT("CREATURE SEULE") : TEXT("decor"), *Wall.What), !bOk);
		return true;
	});
	Add(TEXT("v4.15 correctifs 4 : marche vers le mecanisme 4 avec un Smiler sur le chemin"), 0.5f, [this]()
	{
		using namespace BRTestV415Fix;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->GetLevelNumber() != 2 || !W->IsMissionActive())
		{
			Skip(TEXT("Niveau 2 non atteint ou sans mission : marche avec un Smiler sur le chemin non verifiee"));
			return true;
		}
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		auto End = [&]()
		{
			BRTestV412::ReleaseJump(C);
			if (ABREntity* E = BlockEntity.Get())
			{
				E->Destroy();
			}
			BlockEntity.Reset();
			if (C->IsFlashlightOn() != bLightWas)
			{
				C->ToggleFlashlight();
			}
			bWalking = false;
			return true;
		};
		if (!bWalking)
		{
			ABRMissionDevice* Dev = W->FindMissionDevice(4);
			if (!Dev)
			{
				Skip(TEXT("marche avec un Smiler : mecanisme 4 absent"));
				return true;
			}
			// Lampe eteinte (un faisceau sur un Smiler le fait charger) ; marche vers la cellule du mecanisme
			bLightWas = C->IsFlashlightOn();
			if (bLightWas)
			{
				C->ToggleFlashlight();
			}
			BRTestV412::BeginWalk(W, C, W->CellCenter(Dev->GetCell(), static_cast<float>(C->GetActorLocation().Z)), 200.f, true);
			// Un point de passage en ligne droite dans un couloir (murs des deux cotes), sombre (la lumiere dissiperait le Smiler),
			// a plus de 3 points du depart
			const TArray<FVector>& Pts = BRTestV412::Walk.Points;
			BlockWaypoint = -1;
			for (int32 K = 3; K + 1 < Pts.Num() && K <= 14 && BlockWaypoint < 0; ++K)
			{
				const FIntPoint A = W->WorldToCell(Pts[K - 1]), B = W->WorldToCell(Pts[K]), N = W->WorldToCell(Pts[K + 1]);
				const FIntPoint D = B - A;
				const FIntPoint Side(-D.Y, D.X);
				const FBREntityInfo& Info = ABREntity::Info(EBREntityKind::Smiler);
				const FVector At(Pts[K].X, Pts[K].Y, W->FloorZAt(Pts[K]) + Info.HoverHeight);
				if (N - B == D && !W->CanStep(B, B + Side) && !W->CanStep(B, B - Side) && W->LightLevelAt(At) < BRLight::SmilerVanish * 0.8f)
				{
					BlockWaypoint = K;
				}
			}
			if (BlockWaypoint < 0)
			{
				Skip(FString::Printf(TEXT("marche avec un Smiler : aucun point de passage en couloir droit et sombre parmi %d"), Pts.Num()));
				return End();
			}
			BlockEntity = SpawnFrozen(W, EBREntityKind::Smiler, Pts[BlockWaypoint]);
			if (!BlockEntity.IsValid())
			{
				Skip(TEXT("marche avec un Smiler : Smiler non cree"));
				return End();
			}
			bWalking = true;
			return false;
		}
		const BRTestV412::EWalk R = BRTestV412::StepWalk(W, C, Dt, 150.0, C->GetScareKind() >= 0 || C->IsInputLocked());
		if (R == BRTestV412::EWalk::Moving)
		{
			return false;
		}
		const FIntPoint Cell = W->WorldToCell(BRTestV412::Walk.Points.IsValidIndex(BlockWaypoint) ? BRTestV412::Walk.Points[BlockWaypoint] : C->GetActorLocation());
		if (R == BRTestV412::EWalk::Arrived && BRTestV412::Walk.EntityClears == 0)
		{
			Skip(FString::Printf(TEXT("marche avec un Smiler pose a la cellule (%d, %d) : arrivee sans blocage (Smiler %s) : %s"), Cell.X, Cell.Y,
				BlockEntity.IsValid() ? (BlockEntity->IsVanishing() ? TEXT("dissipe") : TEXT("contourne")) : TEXT("disparu"), *BRTestV412::WalkStats()));
			return End();
		}
		// Arrivee apres avoir ecarte le Smiler : attendu. Bloque : PROBLEME (NON VERIFIE seulement si une creature seule barre
		// encore le passage, ce que ce cas ne doit pas produire : une seule creature, ecartee des la premiere fois)
		Note(FString::Printf(TEXT("marche vers le mecanisme 4 (graine du niveau %u), Smiler immobile pose a la cellule (%d, %d) : %s ; %s"), W->GetSeed(), Cell.X, Cell.Y,
			R == BRTestV412::EWalk::Arrived ? TEXT("ecarte par le test, mecanisme atteint") : TEXT("NON ATTEINT"), *BRTestV412::WalkStats()), R != BRTestV412::EWalk::Arrived);
		return End();
	});
	LimitLastStep(170.f);
}
