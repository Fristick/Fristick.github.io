// v4.15 (realisme) : banc visuel du test automatique (-BRAutoTest -BRAutoTestVisual).
// Memes graines, memes points de vue et memes profils a chaque version : les captures et les mesures se comparent d'un
// lot a l'autre (avant / apres, Saved/AutoTest copie a part par le lanceur) :
//   - Level 0 (graine 9, scene de reference du kit), Levels 1, 2 et 37 (graine 4242), Level 6 (graine 4136) ;
//   - par niveau et par profil (Performance, Qualite, RTX fluide ; -BRProfile=<0|1|2|4> : ce profil seul), trois poses :
//     vue du point de depart, demi-tour, et un point a 10 m vers +X (cellule praticable la plus proche, regard vers +X) ;
//     a chaque pose, une attente (exposition, Lumen), une mesure de 4 s (ligne de Mesures.csv, scene
//     vis_<niveau>_<profil>_<pose>) et une capture du meme nom ;
//   - Levels 0 et 2 : gros plans des surfaces (captures seules) : face a un mur a 80 cm (mur_x : mur le plus proche le long
//     de X, mur_y : le long de Y), sol (regard a -60 degres), plafond (regard a +55 degres, sous le plafonnier allume le
//     plus proche qui a une tache d'eau : meme tirage que les decalques, ABRChunk::CeilingStainAt) ;
//   - au Level 0, en plus : la vue de depart sans l'effet VHS (vue_sansvhs : la camera "propre", la plus proche de la
//     cible), la salle de fosses de la graine 9 (point de vue de la commande BRPits, mesure et capture), un Smiler pres
//     d'un mur derriere lequel brille un plafonnier (mur_smiler : avec MegaLights, l'endroit parait sombre alors que la
//     lumiere de jeu, qui ne voit pas les murs, le compte eclaire ; capture a juger), puis une coupure de courant avec un
//     Smiler face au joueur, lampe eteinte (_coupure) puis allumee (_lampe) : la lampe doit rester lisible et les yeux du
//     Smiler visibles dans le noir ; enfin le retour du courant (_retour_03, _retour_15, _retour_40 : 0,3 s, 1,5 s et 4 s
//     apres, au point de depart : rallumage decale, retard de l'eclairage global, bruit et trainees de MegaLights).
// Les reglages d'apparence du joueur sont fixes pendant le banc (VHS et grain actifs, luminosite 0, flashs normaux, flou
// de mouvement coupe) puis retablis a la fin, comme le profil (rien n'est enregistre). Le niveau est recharge pour chaque
// profil : meme etat de depart (chunks, lumieres, exposition). Duree : 12 a 17 minutes.
// -BRMegaLights=<0|1> (hors version finale) impose OMBRES DE TOUTES LES LAMPES dans tous les profils (deux passages a
// comparer) ; l'etat reel de MegaLights est ecrit avec le mode de rendu de chaque mesure.
#include "BRAutoTest.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BRChunk.h"
#include "BREntity.h"
#include "BRLevels.h"
#include "BRLightLogic.h"
#include "BRPlayerController.h"
#include "BRWorld.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
	/** Gros plans : attente de l'exposition apres le changement de pose (s) */
	constexpr float CloseSettleSeconds = 2.f;
	/** Noir d'une coupure : l'exposition du Level 0 descend lentement (vitesse 0,7 IL/s, plancher 4,5) ; au-dela de 1,5 IL
	 *  de l'objectif, elle tend vers lui avec une constante de temps de 1,5 / vitesse = 2,1 s : 9 s la posent a moins de
	 *  0,1 IL (5 s laissaient 0,3 a 0,55 IL de trop) ; la lampe allumee, la remontee (1,5 IL/s) se pose en 4 s */
	constexpr float DarkSettleSeconds = 9.f;
	constexpr float LampSettleSeconds = 4.f;

	/** Pose de reference du niveau charge (relevee apres le chargement) */
	FVector SpawnLoc = FVector::ZeroVector;
	FRotator SpawnRot = FRotator::ZeroRotator;
	TWeakObjectPtr<ABREntity> DarkSmiler;
	TWeakObjectPtr<ABREntity> WallSmiler;
	double RestoreStart = 0.0;
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

	/** Cellules autour de Start, anneau par anneau (ordre fixe : la meme cellule a chaque version pour la meme graine) */
	template <typename FnT>
	bool ForRings(const FIntPoint& Start, int32 MaxRing, FnT&& Fn)
	{
		for (int32 Ring = 0; Ring <= MaxRing; ++Ring)
		{
			for (int32 DX = -Ring; DX <= Ring; ++DX)
			{
				for (int32 DY = -Ring; DY <= Ring; ++DY)
				{
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) == Ring && Fn(FIntPoint(Start.X + DX, Start.Y + DY)))
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	/** Gros plan d'un mur : le mur le plus proche du point de depart le long de X (bAlongY : de Y), +X (+Y) d'abord a
	 *  egalite ; pose a 80 cm de sa face, a la hauteur du point de depart, regard vers lui */
	bool WallPose(const UWorld* InWorld, const AActor* Ignored, bool bAlongY, FVector& OutLoc, FRotator& OutRot)
	{
		if (!InWorld)
		{
			return false;
		}
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(BRVisualWall), false, Ignored);
		const FVector From = SpawnLoc + FVector(0.f, 0.f, 60.f);
		float Best = 1.0e9f;
		bool bFound = false;
		for (int32 Sign = 1; Sign >= -1; Sign -= 2)
		{
			const FVector Dir = bAlongY ? FVector(0.f, static_cast<float>(Sign), 0.f) : FVector(static_cast<float>(Sign), 0.f, 0.f);
			FHitResult Hit;
			if (!InWorld->LineTraceSingleByChannel(Hit, From, From + Dir * 1500.f, ECC_Visibility, Query) || Hit.Distance + 1.f >= Best)
			{
				continue;
			}
			Best = Hit.Distance;
			FVector Face = Hit.ImpactNormal.GetSafeNormal2D();
			if (Face.IsNearlyZero())
			{
				Face = -Dir;
			}
			OutLoc = FVector(Hit.ImpactPoint.X + Face.X * 80.f, Hit.ImpactPoint.Y + Face.Y * 80.f, SpawnLoc.Z);
			OutRot = FRotator(0.f, (-Face).Rotation().Yaw, 0.f);
			bFound = true;
		}
		return bFound;
	}

	/** Gros plan du plafond : sous le plafonnier allume le plus proche du point de depart qui a une tache d'eau (meme regle
	 *  que les decalques : ABRChunk::CeilingStainAt), regard a +55 degres vers le milieu entre le diffuseur et la tache */
	bool CeilingPose(const ABRWorld* W, const ABRCharacter* C, FVector& OutLoc, FRotator& OutRot, FIntPoint& OutCell)
	{
		if (!W || !C)
		{
			return false;
		}
		const FBRLevelDef& Def = W->Def();
		return ForRings(W->WorldToCell(SpawnLoc), 6, [&](const FIntPoint& Cell)
		{
			if (!W->IsChunkLoaded(W->CellToChunk(Cell)))
			{
				return false;
			}
			const FBRLightInfo Lamp = W->CellLight(Cell.X, Cell.Y);
			FVector Stain = FVector::ZeroVector;
			float Radius = 0.f;
			float Spin = 0.f;
			if (!Lamp.bHas || Lamp.bBroken || !ABRChunk::CeilingStainAt(W, Cell.X, Cell.Y, Stain, Radius, Spin))
			{
				return false;
			}
			const FVector Center = W->CellCenter(Cell);
			const FVector Fixture = Center + Lamp.Offset;
			FVector Dir = (Stain - Fixture).GetSafeNormal2D();
			if (Dir.IsNearlyZero())
			{
				Dir = FVector(1.f, 0.f, 0.f);
			}
			const float Half = C->GetSimpleCollisionHalfHeight();
			const float FloorZ = W->FloorZAt(Center);
			const float EyeZ = FloorZ + Half + 5.f + C->BaseEyeHeight;
			const float Reach = FMath::Max(0.f, FloorZ + Def.WallHeight - EyeZ) / FMath::Tan(FMath::DegreesToRadians(55.f));
			const FVector Target = (Fixture + Stain) * 0.5f;
			const float Lim = FMath::Max(0.f, Def.CellSize * 0.5f - Def.WallThickness * 0.5f - 50.f);
			OutLoc = FVector(FMath::Clamp(Target.X - Dir.X * Reach, Center.X - Lim, Center.X + Lim),
				FMath::Clamp(Target.Y - Dir.Y * Reach, Center.Y - Lim, Center.Y + Lim), FloorZ + Half + 5.f);
			OutRot = FRotator(55.f, Dir.Rotation().Yaw, 0.f);
			OutCell = Cell;
			return true;
		});
	}

	/** Endroit d'une cellule sans lampe allumee, a 40 cm d'un mur derriere lequel brille un plafonnier stable : cette lampe
	 *  seule y donne une lumiere de jeu d'au moins BRLight::NeedsDark (le modele de jeu ne voit pas les murs) */
	bool WallSmilerSpot(const ABRWorld* W, FVector& OutSpot, FVector& OutPlayer, float& OutLamp)
	{
		if (!W)
		{
			return false;
		}
		const FBRLevelDef& Def = W->Def();
		static const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		return ForRings(W->WorldToCell(SpawnLoc), 6, [&](const FIntPoint& Cell)
		{
			if (!W->IsWalkable(Cell) || W->IsPoolCell(Cell.X, Cell.Y) || W->IsPitRoomCell(Cell.X, Cell.Y) || !W->IsChunkLoaded(W->CellToChunk(Cell)))
			{
				return false;
			}
			const FBRLightInfo Own = W->CellLight(Cell.X, Cell.Y);
			if (Own.bHas && !Own.bBroken)
			{
				return false;
			}
			for (const FIntPoint& Dir : Dirs)
			{
				const FIntPoint Next(Cell.X + Dir.X, Cell.Y + Dir.Y);
				const EBREdge Edge = Dir.X != 0 ? W->EdgeE(Dir.X > 0 ? Cell.X : Cell.X - 1, Cell.Y) : W->EdgeN(Cell.X, Dir.Y > 0 ? Cell.Y : Cell.Y - 1);
				const FBRLightInfo Behind = W->CellLight(Next.X, Next.Y);
				if (Edge != EBREdge::Wall || !W->IsWalkable(Next) || !Behind.bHas || Behind.bBroken || Behind.bFlicker)
				{
					continue;
				}
				const FVector Out(static_cast<float>(Dir.X), static_cast<float>(Dir.Y), 0.f);
				const FVector Center = W->CellCenter(Cell);
				const FVector Spot = Center + Out * (Def.CellSize * 0.5f - Def.WallThickness * 0.5f - 40.f);
				const float Dist = static_cast<float>(FVector::Dist2D(Spot, W->CellCenter(Next) + Behind.Offset));
				const float Lamp = Dist < Def.LightRadius ? FMath::Square(1.f - Dist / Def.LightRadius) : 0.f;
				if (Lamp < BRLight::NeedsDark)
				{
					continue;
				}
				OutSpot = Spot;
				OutPlayer = Center - Out * FMath::Max(0.f, Def.CellSize * 0.5f - Def.WallThickness * 0.5f - 50.f);
				OutLamp = Lamp;
				return true;
			}
			return false;
		});
	}

	/** Chunks dont les lampes se rallument encore (rallumage decale apres une coupure) */
	int32 RestrikingChunks(UWorld* InWorld)
	{
		int32 Count = 0;
		if (InWorld)
		{
			for (TActorIterator<ABRChunk> It(InWorld); It; ++It)
			{
				Count += It->IsRestriking() ? 1 : 0;
			}
		}
		return Count;
	}
}

void ABRAutoTest::AddVisualSteps()
{
	using namespace BRTestVisual;
	const TArray<FVisProfile> Chosen = Profiles();

	Add(TEXT("v4.15 banc visuel : debut"), 0.f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC || !GetBRWorld())
		{
			return false;
		}
		if (!bSettingsSaved)
		{
			SavedSettings = FBRSettings::Get();
			bSettingsSaved = true;
		}
		// Apparence fixee (BackroomsPlayer.ini du joueur ignore) : les captures se comparent d'une execution et d'un lot a
		// l'autre ; retablie au bilan
		FBRSettings& S = FBRSettings::Get();
		S.bVHSEffect = true;
		S.bFilmGrain = true;
		S.Brightness = 0.f;
		S.Flashes = 0;
		S.bMotionBlur = false;
		PC->ApplySettings();
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
		Note(TEXT("banc visuel : apparence fixee pendant le banc : effet VHS et grain actifs, luminosite 0, flashs normaux, flou de mouvement coupe (vue_sansvhs : VHS coupe)"));
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
						const bool bFound = BRTestVisual::ForRings(W->WorldToCell(BRTestVisual::SpawnLoc + FVector(1000.f, 0.f, 0.f)), 4, [&](const FIntPoint& Cell)
						{
							const FVector Center = W->CellCenter(Cell);
							if (!W->IsWalkable(Cell) || W->IsPoolCell(Cell.X, Cell.Y) || W->IsOverPit(Center, 60.f) || !W->IsChunkLoaded(W->CellToChunk(Cell)))
							{
								return false;
							}
							Loc = FVector(Center.X, Center.Y, W->FloorZAt(Center) + Half + 5.f);
							return true;
						});
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
				if (Level == 0 && PoseIndex == 0)
				{
					// Meme vue sans l'effet VHS (grain divise par deux, aberration, salissure et reflets de l'objectif, vignettage
					// adoucis) : l'image "propre" ; capture seule, puis VHS retabli
					const FString CleanScene = SceneName(0, *Profile, TEXT("vue_sansvhs"));
					Add(FString::Printf(TEXT("v4.15 visuel : VHS coupe (%s)"), *Profile), 1.f, []()
					{
						FBRSettings::Get().bVHSEffect = false;
						return true;
					});
					Add(FString::Printf(TEXT("v4.15 visuel : capture %s"), *CleanScene), 0.3f, [this, CleanScene]()
					{
						Shot(CleanScene);
						++BRTestVisual::ShotCount;
						return true;
					});
					Add(FString::Printf(TEXT("v4.15 visuel : VHS retabli (%s)"), *Profile), 0.f, []()
					{
						FBRSettings::Get().bVHSEffect = true;
						return true;
					});
				}
			}

			if (Level == 0 || Level == 2)
			{
				// ------------------------------------------------------------ gros plans : murs, sol, plafond (captures seules)
				static const TCHAR* const Close[] = { TEXT("mur_x"), TEXT("mur_y"), TEXT("sol"), TEXT("plafond") };
				for (int32 CloseIndex = 0; CloseIndex < static_cast<int32>(UE_ARRAY_COUNT(Close)); ++CloseIndex)
				{
					const FString Scene = SceneName(Level, *Profile, Close[CloseIndex]);
					Add(FString::Printf(TEXT("v4.15 visuel : %s"), *Scene), CloseSettleSeconds, [this, CloseIndex, Scene]()
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
						bool bOk = true;
						if (CloseIndex <= 1)
						{
							bOk = BRTestVisual::WallPose(GetWorld(), C, CloseIndex == 1, Loc, Rot);
						}
						else if (CloseIndex == 2)
						{
							Rot.Pitch = -60.f;
						}
						else
						{
							FIntPoint Cell(0, 0);
							bOk = BRTestVisual::CeilingPose(W, C, Loc, Rot, Cell);
							if (bOk)
							{
								Note(FString::Printf(TEXT("%s : tache d'eau du plafond de la cellule (%d, %d)"), *Scene, Cell.X, Cell.Y));
							}
						}
						if (!bOk)
						{
							Note(FString::Printf(TEXT("%s : pose introuvable (%s) : point de depart, regard vers le %s"), *Scene,
								CloseIndex <= 1 ? TEXT("aucun mur a moins de 15 m") : TEXT("aucun plafonnier allume avec une tache a moins de 6 cellules"),
								CloseIndex <= 1 ? TEXT("depart") : TEXT("plafond")));
							Rot.Pitch = CloseIndex <= 1 ? 0.f : 55.f;
						}
						C->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
						if (UCharacterMovementComponent* Move = C->GetCharacterMovement())
						{
							Move->StopMovementImmediately();
						}
						PC->SetControlRotation(Rot);
						C->Sanity = 100.f;
						return true;
					});
					Add(FString::Printf(TEXT("v4.15 visuel : capture %s"), *Scene), 0.3f, [this, Scene]()
					{
						Shot(Scene);
						++BRTestVisual::ShotCount;
						return true;
					});
				}
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

			// ---------------------------------------------------------------- Level 0 : Smiler pres d'un mur eclaire derriere
			// Connu (v4.15) : avec MegaLights, chaque plafonnier a son ombre et ne traverse plus les murs a l'image ; la lumiere
			// de jeu (ABRWorld::LightLevelAt) compte toujours les plafonniers a travers les murs. Un Smiler place ici se trouve
			// dans un coin qui parait sombre mais qui est "eclaire" pour les regles (il ne peut pas y apparaitre et s'y
			// dissipe au-dessus de BRLight::SmilerVanish). Capture a juger avant de garder MegaLights par defaut
			const FString WallScene = SceneName(0, *Profile, TEXT("mur_smiler"));
			Add(FString::Printf(TEXT("v4.15 visuel : %s"), *WallScene), CloseSettleSeconds, [this, WallScene]()
			{
				ABRWorld* W = GetBRWorld();
				ABRCharacter* C = GetPlayer();
				ABRPlayerController* PC = GetPC();
				if (!W || !C || !PC)
				{
					return false;
				}
				if (C->IsFlashlightOn())
				{
					C->ToggleFlashlight();
				}
				FVector Spot = FVector::ZeroVector;
				FVector Stand = FVector::ZeroVector;
				float Lamp = 0.f;
				if (!BRTestVisual::WallSmilerSpot(W, Spot, Stand, Lamp))
				{
					Note(FString::Printf(TEXT("%s : aucun mur avec un plafonnier allume derriere a moins de 6 cellules"), *WallScene));
					return true;
				}
				const float Half = C->GetSimpleCollisionHalfHeight();
				C->SetActorLocation(FVector(Stand.X, Stand.Y, W->FloorZAt(Stand) + Half + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
				if (UCharacterMovementComponent* Move = C->GetCharacterMovement())
				{
					Move->StopMovementImmediately();
				}
				const FBREntityInfo& Info = ABREntity::Info(EBREntityKind::Smiler);
				const FVector At(Spot.X, Spot.Y, W->FloorZAt(Spot) + (Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 5.f));
				if (ABREntity* E = W->SpawnEntity(EBREntityKind::Smiler, At))
				{
					FVector ToPlayer = C->GetActorLocation() - At;
					ToPlayer.Z = 0.f;
					E->SetActorRotation(ToPlayer.Rotation());
					E->CustomTimeDilation = 0.02f;
					BRTestVisual::WallSmiler = E;
				}
				PC->SetControlRotation((At - C->GetEyeLocation()).Rotation());
				Note(FString::Printf(TEXT("%s : lumiere de jeu %.2f a l'endroit du Smiler, dont %.2f du plafonnier derriere le mur (sombre a partir de %.2f, dissipation a %.2f) ; Smiler %s"),
					*WallScene, W->LightLevelAt(At), Lamp, BRLight::NeedsDark, BRLight::SmilerVanish, BRTestVisual::WallSmiler.IsValid() ? TEXT("present") : TEXT("absent")));
				return true;
			});
			Add(FString::Printf(TEXT("v4.15 visuel : capture %s"), *WallScene), 0.3f, [this, WallScene]()
			{
				if (ABREntity* E = BRTestVisual::WallSmiler.Get())
				{
					Note(FString::Printf(TEXT("%s : Smiler %s"), *WallScene, E->IsVanishing() ? TEXT("en train de se dissiper (lumiere de jeu)") : TEXT("present")));
				}
				Shot(WallScene);
				++BRTestVisual::ShotCount;
				return true;
			});

			// ---------------------------------------------------------------- Level 0 : coupure, Smiler, lampe, retour
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
				if (ABREntity* E = BRTestVisual::WallSmiler.Get())
				{
					E->Destroy();
				}
				BRTestVisual::WallSmiler.Reset();
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
			Add(FString::Printf(TEXT("v4.15 visuel : adaptation au noir (%s)"), *Profile), DarkSettleSeconds, []()
			{
				return true;
			});
			Add(FString::Printf(TEXT("v4.15 visuel : capture %s"), *DarkScene), 0.3f, [this, DarkScene]()
			{
				ABRWorld* W = GetBRWorld();
				Note(FString::Printf(TEXT("%s : alimentation %.2f, Smiler %s, %.0f s d'adaptation au noir"), *DarkScene, W ? W->GetPower() : -1.f,
					BRTestVisual::DarkSmiler.IsValid() ? TEXT("present") : TEXT("absent"), BRTestVisual::DarkSettleSeconds));
				Shot(DarkScene);
				++BRTestVisual::ShotCount;
				return true;
			});
			// La lampe s'allume a l'etape suivante seulement : la capture est prise a la fin de l'image ou elle est demandee
			Add(FString::Printf(TEXT("v4.15 visuel : lampe dans le noir (%s)"), *Profile), LampSettleSeconds, [this]()
			{
				if (ABRCharacter* C = GetPlayer())
				{
					if (!C->IsFlashlightOn())
					{
						C->ToggleFlashlight();
					}
				}
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
			// Retour du courant (ABRWorld::ForceRestore : phase de retour, rallumage decale de chaque lampe), au point de depart,
			// lampe eteinte : captures a 0,3 s, 1,5 s et 4 s (temps reel ecrit dans le rapport)
			Add(FString::Printf(TEXT("v4.15 visuel : retour du courant (%s)"), *Profile), 0.3f, [this]()
			{
				ABRWorld* W = GetBRWorld();
				ABRCharacter* C = GetPlayer();
				ABRPlayerController* PC = GetPC();
				if (!W || !C || !PC)
				{
					return false;
				}
				if (ABREntity* E = BRTestVisual::DarkSmiler.Get())
				{
					E->Destroy();
				}
				BRTestVisual::DarkSmiler.Reset();
				if (C->IsFlashlightOn())
				{
					C->ToggleFlashlight();
				}
				C->SetActorLocation(BRTestVisual::SpawnLoc, false, nullptr, ETeleportType::TeleportPhysics);
				PC->SetControlRotation(BRTestVisual::SpawnRot);
				if (!W->IsBlackout())
				{
					Note(TEXT("retour du courant : la coupure etait deja finie (captures de retour sans valeur)"), true);
				}
				W->ForceRestore();
				BRTestVisual::RestoreStart = FPlatformTime::Seconds();
				return true;
			});
			static const TCHAR* const Returns[] = { TEXT("retour_03"), TEXT("retour_15"), TEXT("retour_40") };
			static const float ReturnWaits[] = { 1.2f, 2.5f, 0.3f };
			for (int32 ReturnIndex = 0; ReturnIndex < static_cast<int32>(UE_ARRAY_COUNT(Returns)); ++ReturnIndex)
			{
				const FString Scene = SceneName(0, *Profile, Returns[ReturnIndex]);
				Add(FString::Printf(TEXT("v4.15 visuel : capture %s"), *Scene), ReturnWaits[ReturnIndex], [this, Scene]()
				{
					ABRWorld* W = GetBRWorld();
					Note(FString::Printf(TEXT("%s : %.2f s apres le retour, alimentation %.2f, %d chunk(s) en rallumage, MegaLights %s"), *Scene,
						FPlatformTime::Seconds() - BRTestVisual::RestoreStart, W ? W->GetPower() : -1.f, BRTestVisual::RestrikingChunks(GetWorld()),
						ABRWorld::IsMegaLightsActive() ? TEXT("actif") : TEXT("inactif")));
					Shot(Scene);
					++BRTestVisual::ShotCount;
					return true;
				});
			}
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
