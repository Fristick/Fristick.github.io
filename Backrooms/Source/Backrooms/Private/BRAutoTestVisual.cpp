// v4.15 (realisme) : banc visuel du test automatique (-BRAutoTest -BRAutoTestVisual).
// Memes graines, memes points de vue et memes profils a chaque version : les captures et les mesures se comparent d'un
// lot a l'autre (avant / apres, Saved/AutoTest copie a part par le lanceur) :
//   - Level 0 (graine 9, scene de reference du kit), Levels 1, 2 et 37 (graine 4242), Level 6 (graine 4136) ;
//   - par niveau et par profil (Performance, Qualite, RTX fluide ; -BRProfile=<0|1|2|4> : ce profil seul), trois poses :
//     vue du point de depart, demi-tour, et un point a 10 m vers +X (cellule praticable la plus proche, regard vers +X) ;
//     a chaque pose, une attente (exposition, Lumen), une mesure de 4 s (ligne de Mesures.csv, scene
//     vis_<niveau>_<profil>_<pose>) et une capture du meme nom ;
//   - au Level 0, en plus : la salle de fosses de la graine 9 (point de vue de la commande BRPits, mesure et capture), puis
//     une coupure de courant avec un Smiler face au joueur, lampe eteinte (_coupure) puis allumee (_lampe) : la lampe doit
//     rester lisible et les yeux du Smiler visibles dans le noir.
// Le niveau est recharge pour chaque profil : meme etat de depart (chunks, lumieres, exposition). Les reglages du joueur
// sont retablis a la fin (rien n'est enregistre). Duree : 10 a 15 minutes.
// -BRMegaLights=<0|1> (hors version finale) impose OMBRES DE TOUTES LES LAMPES dans tous les profils (deux passages a
// comparer) ; l'etat reel de MegaLights est ecrit avec le mode de rendu de chaque mesure.
#include "BRAutoTest.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BREntity.h"
#include "BRLevels.h"
#include "BRPlayerController.h"
#include "BRWorld.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace BRTestVisual
{
	struct FVisLevel
	{
		int32 Level;
		uint32 Seed;
	};
	/** Niveaux et graines du banc (Level 0 : graine de la scene du kit ; Level 6 : graine du test du scanner) */
	const FVisLevel VisLevels[] = { { 0, 9u }, { 1, 4242u }, { 2, 4242u }, { 37, 4242u }, { 6, 4136u } };

	struct FVisProfile
	{
		int32 Index;
		const TCHAR* Name;
	};
	const FVisProfile VisProfiles[] = { { 0, TEXT("Performance") }, { 1, TEXT("Qualite") }, { 4, TEXT("RTXFluide") }, { 2, TEXT("Cinematique") } };

	/** Attente avant chaque mesure (exposition, cache de surfaces de Lumen) et duree de la mesure (s) */
	constexpr float SettleSeconds = 3.f;
	constexpr float MeasureSeconds = 4.f;

	/** Pose de reference du niveau charge (relevee apres le chargement) */
	FVector SpawnLoc = FVector::ZeroVector;
	FRotator SpawnRot = FRotator::ZeroRotator;
	TWeakObjectPtr<ABREntity> DarkSmiler;
	int32 ShotCount = 0;
	int32 MeasureCount = 0;
	double SuiteStart = 0.0;

	/** Profils mesures : les trois du banc, ou celui de -BRProfile= */
	TArray<FVisProfile> Profiles()
	{
		TArray<FVisProfile> Out;
		int32 Forced = -1;
		if (FParse::Value(FCommandLine::Get(), TEXT("BRProfile="), Forced))
		{
			for (const FVisProfile& P : VisProfiles)
			{
				if (P.Index == Forced)
				{
					Out.Add(P);
				}
			}
		}
		if (Out.Num() == 0)
		{
			Out = { VisProfiles[0], VisProfiles[1], VisProfiles[2] };
		}
		return Out;
	}

	FString SceneName(int32 Level, const TCHAR* Profile, const TCHAR* Pose)
	{
		return FString::Printf(TEXT("vis_%d_%s_%s"), Level, Profile, Pose);
	}
}

void ABRAutoTest::AddVisualSteps()
{
	using namespace BRTestVisual;
	const TArray<FVisProfile> Chosen = Profiles();

	Add(TEXT("v4.15 banc visuel : debut"), 0.f, [this]()
	{
		if (!GetPC() || !GetBRWorld())
		{
			return false;
		}
		if (!bSettingsSaved)
		{
			SavedSettings = FBRSettings::Get();
			bSettingsSaved = true;
		}
		BRTestVisual::ShotCount = 0;
		BRTestVisual::MeasureCount = 0;
		BRTestVisual::SuiteStart = FPlatformTime::Seconds();
		FLevelReport R;
		R.Title = TEXT("v4.15 banc visuel (poses fixes)");
		R.Scene = TEXT("vis");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		int32 ForcedMega = -1;
		FParse::Value(FCommandLine::Get(), TEXT("BRMegaLights="), ForcedMega);
		Note(FString::Printf(TEXT("banc visuel : OMBRES DE TOUTES LES LAMPES %s ; ray tracing materiel %s"),
			ForcedMega == 0 ? TEXT("imposees a 0 (-BRMegaLights=0)") : (ForcedMega == 1 ? TEXT("imposees a 1 (-BRMegaLights=1)") : TEXT("selon chaque profil")),
			ABRPlayerController::IsHardwareRayTracingAvailable() ? TEXT("actif") : TEXT("indisponible")));
		return true;
	});

	for (const FVisLevel& L : VisLevels)
	{
		const int32 Level = L.Level;
		if (!BRLevels::Exists(Level))
		{
			continue;
		}
		for (const FVisProfile& P : Chosen)
		{
			const int32 ProfileIndex = P.Index;
			const FString Profile = P.Name;
			// Profil applique AVANT le chargement : les lumieres sont creees du bon type et le niveau part du meme etat
			Add(FString::Printf(TEXT("v4.15 visuel : profil %s (Niveau %d)"), *Profile, Level), 0.5f, [this, ProfileIndex]()
			{
				ABRPlayerController* PC = GetPC();
				ABRWorld* W = GetBRWorld();
				if (!PC || !W || W->IsTransitioning())
				{
					return false;
				}
				ABRPlayerController::ApplyGraphicsProfile(ProfileIndex);
				PC->ApplySettings();
				return true;
			});
			AddSeedLoad(Level, L.Seed, FString::Printf(TEXT("v4.15 visuel : Niveau %d, graine %u, %s"), Level, L.Seed, *Profile));
			Add(FString::Printf(TEXT("v4.15 visuel : pose de depart (Niveau %d, %s)"), Level, *Profile), 0.f, [this]()
			{
				ABRCharacter* C = GetPlayer();
				ABRPlayerController* PC = GetPC();
				if (!C || !PC)
				{
					return false;
				}
				BRTestVisual::SpawnLoc = C->GetActorLocation();
				BRTestVisual::SpawnRot = FRotator(0.f, PC->GetControlRotation().Yaw, 0.f);
				return true;
			});

			// Trois poses : depart, demi-tour, 10 m vers +X
			static const TCHAR* const Poses[] = { TEXT("vue"), TEXT("dos"), TEXT("x10") };
			for (int32 PoseIndex = 0; PoseIndex < static_cast<int32>(UE_ARRAY_COUNT(Poses)); ++PoseIndex)
			{
				const FString Scene = SceneName(Level, *Profile, Poses[PoseIndex]);
				Add(FString::Printf(TEXT("v4.15 visuel : %s"), *Scene), SettleSeconds, [this, PoseIndex, Scene, Level]()
				{
					ABRWorld* W = GetBRWorld();
					ABRCharacter* C = GetPlayer();
					ABRPlayerController* PC = GetPC();
					if (!W || !C || !PC)
					{
						return false;
					}
					FVector Loc = BRTestVisual::SpawnLoc;
					FRotator Rot = BRTestVisual::SpawnRot;
					if (PoseIndex == 1)
					{
						Rot.Yaw += 180.f;
					}
					else if (PoseIndex == 2)
					{
						// Cellule praticable la plus proche du point situe 10 m vers +X (ordre de recherche fixe : la meme
						// cellule a chaque version pour la meme graine), ni bassin, ni fosse, chunk construit
						const float Half = C->GetSimpleCollisionHalfHeight();
						const FIntPoint Target = W->WorldToCell(BRTestVisual::SpawnLoc + FVector(1000.f, 0.f, 0.f));
						bool bFound = false;
						for (int32 Ring = 0; Ring <= 4 && !bFound; ++Ring)
						{
							for (int32 DX = -Ring; DX <= Ring && !bFound; ++DX)
							{
								for (int32 DY = -Ring; DY <= Ring && !bFound; ++DY)
								{
									if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Ring)
									{
										continue;
									}
									const FIntPoint Cell(Target.X + DX, Target.Y + DY);
									const FVector Center = W->CellCenter(Cell);
									if (!W->IsWalkable(Cell) || W->IsPoolCell(Cell.X, Cell.Y) || W->IsOverPit(Center, 60.f) || !W->IsChunkLoaded(W->CellToChunk(Cell)))
									{
										continue;
									}
									Loc = FVector(Center.X, Center.Y, W->FloorZAt(Center) + Half + 5.f);
									bFound = true;
								}
							}
						}
						if (!bFound)
						{
							Note(FString::Printf(TEXT("%s : aucune cellule praticable a 10 m vers +X : pose de depart gardee"), *Scene));
						}
						Rot = FRotator(0.f, 0.f, 0.f);
					}
					C->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
					if (UCharacterMovementComponent* Move = C->GetCharacterMovement())
					{
						Move->StopMovementImmediately();
					}
					PC->SetControlRotation(Rot);
					C->Sanity = 100.f;
					C->Health = 100.f;
					FLevelReport R;
					R.Level = Level;
					R.Title = Scene;
					R.Scene = Scene;
					R.FirstLogLine = LogLineCount();
					Reports.Add(R);
					return true;
				});
				Add(FString::Printf(TEXT("v4.15 visuel : mesure %s"), *Scene), MeasureSeconds, [this]()
				{
					StartMeasure();
					return true;
				});
				Add(FString::Printf(TEXT("v4.15 visuel : capture %s"), *Scene), 0.3f, [this, Scene]()
				{
					FLevelReport& R = Report();
					EndMeasure(R);
					if (R.Chunks == 0)
					{
						Note(FString::Printf(TEXT("%s : aucune salle chargee"), *Scene), true);
					}
					Shot(Scene);
					++BRTestVisual::ShotCount;
					++BRTestVisual::MeasureCount;
					return true;
				});
			}

			if (Level != 0)
			{
				continue;
			}

			// ---------------------------------------------------------------- Level 0 : salle de fosses de la graine 9
			const FString PitScene = SceneName(0, *Profile, TEXT("fosses"));
			Add(FString::Printf(TEXT("v4.15 visuel : %s"), *PitScene), SettleSeconds, [this, PitScene]()
			{
				ABRPlayerController* PC = GetPC();
				ABRCharacter* C = GetPlayer();
				if (!PC || !C)
				{
					return false;
				}
				PC->BRPits();
				C->Sanity = 100.f;
				FLevelReport R;
				R.Level = 0;
				R.Title = PitScene;
				R.Scene = PitScene;
				R.FirstLogLine = LogLineCount();
				Reports.Add(R);
				return true;
			});
			Add(FString::Printf(TEXT("v4.15 visuel : mesure %s"), *PitScene), MeasureSeconds, [this]()
			{
				StartMeasure();
				return true;
			});
			Add(FString::Printf(TEXT("v4.15 visuel : capture %s"), *PitScene), 0.3f, [this, PitScene]()
			{
				EndMeasure(Report());
				Shot(PitScene);
				++BRTestVisual::ShotCount;
				++BRTestVisual::MeasureCount;
				return true;
			});

			// ---------------------------------------------------------------- Level 0 : coupure, Smiler, lampe
			const FString DarkScene = SceneName(0, *Profile, TEXT("coupure"));
			const FString LampScene = SceneName(0, *Profile, TEXT("lampe"));
			Add(FString::Printf(TEXT("v4.15 visuel : coupure (%s)"), *Profile), 0.f, [this]()
			{
				ABRWorld* W = GetBRWorld();
				ABRCharacter* C = GetPlayer();
				ABRPlayerController* PC = GetPC();
				if (!W || !C || !PC)
				{
					return false;
				}
				C->SetActorLocation(BRTestVisual::SpawnLoc, false, nullptr, ETeleportType::TeleportPhysics);
				PC->SetControlRotation(BRTestVisual::SpawnRot);
				if (C->IsFlashlightOn())
				{
					C->ToggleFlashlight();
				}
				W->ForceBlackout();
				return true;
			});
			Add(FString::Printf(TEXT("v4.15 visuel : noir complet (%s)"), *Profile), 0.f, [this, DarkScene]()
			{
				ABRWorld* W = GetBRWorld();
				ABRCharacter* C = GetPlayer();
				if (!W || !C)
				{
					return false;
				}
				if (!W->IsBlackout() || W->GetPower() > 0.01f)
				{
					if (StepTime > 10.f)
					{
						Note(FString::Printf(TEXT("%s : la coupure de courant n'atteint pas le noir (alimentation %.2f)"), *DarkScene, W->GetPower()), true);
						return true;
					}
					return false;
				}
				// Un Smiler face au joueur, presque fige : ses yeux doivent rester visibles dans le noir
				const FBREntityInfo& Info = ABREntity::Info(EBREntityKind::Smiler);
				FVector Spot;
				if (FindSpotInFront(450.f, Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 5.f, Spot))
				{
					if (ABREntity* E = W->SpawnEntity(EBREntityKind::Smiler, Spot))
					{
						FVector ToPlayer = C->GetActorLocation() - Spot;
						ToPlayer.Z = 0.f;
						E->SetActorRotation(ToPlayer.Rotation());
						E->CustomTimeDilation = 0.02f;
						BRTestVisual::DarkSmiler = E;
						if (ABRPlayerController* PC = GetPC())
						{
							PC->SetControlRotation((Spot - C->GetEyeLocation()).Rotation());
						}
					}
				}
				if (!BRTestVisual::DarkSmiler.IsValid())
				{
					Note(FString::Printf(TEXT("%s : pas de place pour le Smiler devant le joueur"), *DarkScene));
				}
				return true;
			});
			// L'exposition descend vers son minimum (adaptation au noir) avant la capture
			Add(FString::Printf(TEXT("v4.15 visuel : adaptation au noir (%s)"), *Profile), 5.f, []()
			{
				return true;
			});
			Add(FString::Printf(TEXT("v4.15 visuel : capture %s"), *DarkScene), 0.3f, [this, DarkScene]()
			{
				ABRWorld* W = GetBRWorld();
				Note(FString::Printf(TEXT("%s : alimentation %.2f, Smiler %s"), *DarkScene, W ? W->GetPower() : -1.f,
					BRTestVisual::DarkSmiler.IsValid() ? TEXT("present") : TEXT("absent")));
				Shot(DarkScene);
				++BRTestVisual::ShotCount;
				if (ABRCharacter* C = GetPlayer())
				{
					if (!C->IsFlashlightOn())
					{
						C->ToggleFlashlight();
					}
				}
				return true;
			});
			Add(FString::Printf(TEXT("v4.15 visuel : lampe dans le noir (%s)"), *Profile), 2.f, []()
			{
				return true;
			});
			Add(FString::Printf(TEXT("v4.15 visuel : capture %s"), *LampScene), 0.3f, [this, LampScene]()
			{
				ABRCharacter* C = GetPlayer();
				if (C && !C->IsFlashlightOn())
				{
					Note(FString::Printf(TEXT("%s : la lampe ne s'allume pas"), *LampScene), true);
				}
				if (ABREntity* E = BRTestVisual::DarkSmiler.Get())
				{
					if (E->IsVanishing())
					{
						Note(FString::Printf(TEXT("%s : le Smiler disparait dans le faisceau (capture a regarder)"), *LampScene));
					}
				}
				Shot(LampScene);
				++BRTestVisual::ShotCount;
				return true;
			});
			Add(FString::Printf(TEXT("v4.15 visuel : fin de la coupure (%s)"), *Profile), 0.2f, [this]()
			{
				if (ABREntity* E = BRTestVisual::DarkSmiler.Get())
				{
					E->Destroy();
				}
				BRTestVisual::DarkSmiler.Reset();
				if (ABRCharacter* C = GetPlayer())
				{
					if (C->IsFlashlightOn())
					{
						C->ToggleFlashlight();
					}
				}
				return true;
			});
		}
	}

	Add(TEXT("v4.15 banc visuel : bilan"), 0.f, [this]()
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
		FLevelReport R;
		R.Title = TEXT("v4.15 banc visuel : bilan");
		R.Scene = TEXT("vis");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		Note(FString::Printf(TEXT("banc visuel : %d capture(s), %d mesure(s) en %.0f s (captures vis_<niveau>_<profil>_<pose>.png, lignes vis_* de Mesures.csv)"),
			BRTestVisual::ShotCount, BRTestVisual::MeasureCount, FPlatformTime::Seconds() - BRTestVisual::SuiteStart));
		return true;
	});
}
