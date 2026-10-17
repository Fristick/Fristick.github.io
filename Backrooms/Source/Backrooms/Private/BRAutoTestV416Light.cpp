// v4.16 : -BRAutoTest -BRAutoTestV416Light : lumiere de gameplay alignee sur la lumiere visible (prompt v4.16, 3.1 et 3.2 ;
// ABRWorld::LightLevelAt, BRWorldLight.cpp, BRLight::Occlusion).
//   1. Grille : sur 8 graines (Levels 0, 1, 2, 4, 5, 8 et 37), empreinte de la grille de lumiere (aretes, piliers, cellules
//      pleines, plafonniers) comparee a celle du banc hors moteur (Tools/Light/test_light.cpp, section D : meme portage) ;
//      points d'emission des plafonniers de jeu = lumieres construites par les chunks ; accord de la regle de la grille avec
//      une trace physique de l'emetteur au point (murs du decor construit ; les objets poses, rochers et ecoincons d'arche
//      sans collision expliquent les ecarts, notes) ; part des positions dont la lumiere change sur cette graine.
//   2. Level 0, graine 9 (scene du kit, ombres MegaLights) : meme endroit avec et sans mur (le plafonnier derriere le mur ne
//      compte plus, un Smiler pose la ne se dissipe plus), passage (axe, jambage, linteau selon la hauteur), coupure :
//      alimentation vue par les regles = fondu regulier a chaque image, quels que soient le vacillement local et le reglage
//      FLASHS (normaux puis attenues), source eteinte (noir : 0) ; profils Performance, Qualite, RTX fluide, Cinematique, puis
//      ombres de toutes les lampes et neons surfaciques coupes : memes valeurs au bit pres ; meme graine rechargee : memes
//      valeurs (comme un client qui construit le niveau).
//   3. Source de mission (3.2) : source de test enregistree comme celles des mecanismes (SetGameplayLight), point vise : le
//      joueur ; rien entre les deux ; une capsule de coequipier (profil Pawn, comme un joueur), puis une creature, entre les
//      deux : la valeur ne baisse pas (le filtre d'avant la v4.16, journal : capsule touchee, la coupait) ; porte physique
//      fermee (boite Blocker des mecanismes) : la lumiere est coupee, porte ouverte : elle revient ; mur du decor : coupee ;
//      source eteinte : 0 ; LightLevelAt (caches) = recalcul sans cache dans chaque etat.
//   4. Cout par image : creatures qui lisent la lumiere a chaque image (Smilers, Deathmoths, Bacteria), immobiles puis libres,
//      source de mission allumee pres d'elles : appels, calculs, cache, plafonniers, obstacles, traces par image et par
//      creature ; temps d'un appel (cache vide, puis plein).
// Une verification impossible (pas de mur avec un plafonnier derriere, chunk absent, materiel sans ray tracing...) est
// notee "NON VERIFIE", jamais comptee comme reussie.
#include "BRAutoTest.h"
#include "Backrooms.h"
#include "BRCharacter.h"
#include "BRChunk.h"
#include "BREntity.h"
#include "BRInteractables.h"
#include "BRLevels.h"
#include "BRLightLogic.h"
#include "BRPlayerController.h"
#include "BRWorld.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LocalLightComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"

namespace BRTestV416Light
{
	/** Empreintes de la grille de lumiere calculees par le banc hors moteur (Tools/Light/test_light.cpp, section D) */
	struct FPrint
	{
		int32 Level;
		uint32 UserSeed;
		uint32 Expected;
	};
	const FPrint Prints[] = {
		{ 0, 9u, 0x33387e10u }, { 0, 4242u, 0xb142f19cu }, { 1, 4242u, 0xd0705669u }, { 2, 4242u, 0x94f0c4d7u },
		{ 4, 4242u, 0x8d3f54e3u }, { 5, 4242u, 0xb752f85fu }, { 8, 4242u, 0x9c633728u }, { 37, 4242u, 0x5b6e31f0u } };
	constexpr int32 PrintHalf = 12;

	const FIntPoint Dirs[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

	/** Arete entre la cellule A et sa voisine A + Dir (grille de la lumiere de gameplay) */
	uint8 EdgeToward(const ABRWorld* W, const FIntPoint& A, const FIntPoint& Dir)
	{
		if (Dir.X > 0)
		{
			return W->GetLightCell(A.X, A.Y).EdgeE;
		}
		if (Dir.X < 0)
		{
			return W->GetLightCell(A.X - 1, A.Y).EdgeE;
		}
		if (Dir.Y > 0)
		{
			return W->GetLightCell(A.X, A.Y).EdgeN;
		}
		return W->GetLightCell(A.X, A.Y - 1).EdgeN;
	}

	/** Cellules autour de Start, anneau par anneau (ordre fixe) */
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

	/** Cellule praticable de la grille, chunk construit, ni bassin ni salle de fosses */
	bool UsableCell(const ABRWorld* W, const FIntPoint& C)
	{
		return W->IsWalkable(C) && W->IsCellInBounds(C.X, C.Y) && !W->IsPoolCell(C.X, C.Y) && !W->IsPitRoomCell(C.X, C.Y)
			&& W->IsChunkLoaded(W->CellToChunk(C));
	}

	/** Plafonnier stable (allume, ne clignote pas) */
	bool StableLamp(const FBRLightCell& L)
	{
		return L.bLamp && !L.bFlicker;
	}

	/** Apport d'un plafonnier en P selon la loi d'apport (sans occlusion ni alimentation) */
	float LampWeight(const ABRWorld* W, const FBRLightCell& L, const FVector& P)
	{
		const float Radius = W->Def().LightRadius;
		const float Dist = static_cast<float>(FVector::Dist2D(P, L.Anchor));
		if (!L.bLamp || !(Dist < Radius))
		{
			return 0.f;
		}
		const float K = 1.f - Dist / Radius;
		return K * K * (L.bFlicker ? BRLight::FlickerWeight : 1.f);
	}

	/** Trace physique de l'emetteur au point (canal WorldStatic, sans les capsules) : vrai si rien ne coupe ; OutHit : obstacle */
	bool PhysicsSees(const UWorld* InWorld, const AActor* Ignored, const FVector& From, const FVector& To, FString* OutHit)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestV416Light), false, Ignored);
		FCollisionResponseParams R;
		R.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		FHitResult Hit;
		if (!InWorld || !InWorld->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic, Q, R))
		{
			return true;
		}
		if (OutHit)
		{
			const AActor* A = Hit.GetActor();
			const UPrimitiveComponent* C = Hit.GetComponent();
			*OutHit = FString::Printf(TEXT("%s.%s a %.0f cm"), A ? *A->GetName() : TEXT("?"), C ? *C->GetName() : TEXT("?"), Hit.Distance);
		}
		return false;
	}

	/** Point d'une cellule a Off cm de son centre vers Dir (et Side cm de cote), a Z cm du sol */
	FVector CellPoint(const ABRWorld* W, const FIntPoint& Cell, const FIntPoint& Dir, float Off, float Side, float Z)
	{
		const FVector Out(static_cast<float>(Dir.X), static_cast<float>(Dir.Y), 0.f);
		const FVector Along(-Out.Y, Out.X, 0.f);
		const FVector P = W->CellCenter(Cell) + Out * Off + Along * Side;
		return FVector(P.X, P.Y, W->FloorZAt(P) + Z);
	}

	/** Joueur pose au sol en P, immobile */
	void PlacePlayer(const ABRWorld* W, ABRCharacter* C, const FVector& P)
	{
		const float Half = C->GetSimpleCollisionHalfHeight();
		C->SetActorLocation(FVector(P.X, P.Y, W->FloorZAt(P) + Half + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* M = C->GetCharacterMovement())
		{
			M->StopMovementImmediately();
		}
	}

	// ---------------------------------------------------------------------------------------------- etat des etapes
	/** Scene du mur : cellule A, voisine B (derriere le mur) et son plafonnier, point a 40 cm de la face du mur dans A */
	bool bWallScene = false;
	FIntPoint WallA, WallB, WallDir;
	FVector WallSmilerP, WallPlayerP;
	/** Meme endroit sans mur (arete ouverte vers un plafonnier stable) */
	bool bOpenScene = false;
	FIntPoint OpenA, OpenB;
	FVector OpenP;
	/** Passage (arete Door) */
	bool bDoorScene = false;
	FIntPoint DoorA, DoorB, DoorDir;
	TWeakObjectPtr<ABREntity> WallSmiler;
	/** Points compares d'un profil a l'autre et apres le rechargement */
	TArray<FVector> Probes;
	TArray<float> Baseline;
	FVector LitP = FVector::ZeroVector;
	/** Coupure : images examinees, ecarts, alimentation precedente, images ou le vacillement local differe des regles */
	int32 PhaseFrames = 0;
	int32 PhaseMismatch = 0;
	int32 PhaseNonMono = 0;
	int32 PhaseDecoupled = 0;
	float PhasePrev = -1.f;
	FString PhaseFirstBad;
	/** Source de mission de test */
	constexpr uint8 TestSlot = 7;
	FVector SourceP = FVector::ZeroVector;
	float SourceAdd = 0.f;
	float ClearMission = 0.f;
	TWeakObjectPtr<ACharacter> Mate;
	TWeakObjectPtr<ABREntity> Creature;
	TWeakObjectPtr<AActor> Door;
	/** Cout */
	TArray<TWeakObjectPtr<ABREntity>> CostEntities;
	uint64 CostFrame0 = 0;
	double CostT0 = 0.0;
	/** Totaux des niveaux (accord de la grille avec la physique, positions changees) */
	int32 TotalPairs = 0;
	int32 TotalAgree = 0;

	/** Releve des points (valeur de LightLevelAt) */
	void Sample(const ABRWorld* W, TArray<float>& Out)
	{
		Out.Reset();
		for (const FVector& P : Probes)
		{
			Out.Add(W->LightLevelAt(P));
		}
	}

	int32 CountDiff(const TArray<float>& A, const TArray<float>& B)
	{
		int32 N = A.Num() == B.Num() ? 0 : FMath::Max(A.Num(), B.Num());
		for (int32 I = 0; I < FMath::Min(A.Num(), B.Num()); ++I)
		{
			N += A[I] == B[I] ? 0 : 1; // au bit pres
		}
		return N;
	}

	ABREntity* SpawnFrozen(ABRWorld* W, EBREntityKind Kind, const FVector& P, float Dilation)
	{
		const FBREntityInfo& Info = ABREntity::Info(Kind);
		ABREntity* E = W->SpawnEntity(Kind, FVector(P.X, P.Y, W->FloorZAt(P) + (Info.bFlying ? Info.HoverHeight : Info.HalfHeight + 5.f)));
		if (E)
		{
			E->CustomTimeDilation = Dilation;
		}
		return E;
	}

	/** Courant plein (pas de coupure naturelle en cours) : les valeurs de reference ont ete relevees ainsi */
	bool PowerSteady(const ABRWorld* W)
	{
		return !W->IsBlackout() && !W->IsPowerFlickering() && W->GetGameplayPower() == 1.f;
	}

	/** Apport de la source de mission de test au centre de Target selon la loi (sans occlusion), a sa position du moment */
	float SourceLaw(const AActor* Target)
	{
		return BRLight::Contribution(static_cast<float>(FVector::Dist(Target->GetActorLocation(), SourceP)), BRLight::BeaconRadius, BRLight::BeaconIntensity);
	}
}

void ABRAutoTest::AddV416LightSteps()
{
	using namespace BRTestV416Light;

	Add(TEXT("v4.16 lumiere : debut"), 0.f, [this]()
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
		FBRSettings::Get().Flashes = 0; // flashs normaux : le vacillement local est tire au hasard (il ne doit rien decider)
		PC->ApplySettings();
		TotalPairs = 0;
		TotalAgree = 0;
		FLevelReport R;
		R.Title = TEXT("v4.16 : lumiere de gameplay (murs, passages, coupure, profils, sources de mission, cout)");
		R.Scene = TEXT("v4.16");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		Note(FString::Printf(TEXT("ray tracing materiel %s ; seuils : apparition sous %.2f, dissipation du Smiler au-dessus de %.2f"),
			ABRPlayerController::IsHardwareRayTracingAvailable() ? TEXT("disponible") : TEXT("indisponible"), BRLight::NeedsDark, BRLight::SmilerVanish));
		return true;
	});

	// ------------------------------------------------------------------------------------------------ 1. grille par niveau
	for (const FPrint& Pr : Prints)
	{
		const int32 Level = Pr.Level;
		if (!BRLevels::Exists(Level))
		{
			continue;
		}
		const uint32 UserSeed = Pr.UserSeed;
		const uint32 Expected = Pr.Expected;
		AddSeedLoad(Level, UserSeed, FString::Printf(TEXT("v4.16 lumiere : Niveau %d, -BRSeed=%u"), Level, UserSeed));
		Add(FString::Printf(TEXT("v4.16 lumiere : grille du Niveau %d (graine %u)"), Level, UserSeed), 0.f, [this, Level, UserSeed, Expected]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C || W->GetLevelNumber() != Level)
			{
				return false;
			}
			const FIntPoint Here = W->WorldToCell(C->GetActorLocation());
			// Chunks autour du point de depart construits (la trace physique en a besoin)
			int32 Missing = 0;
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					const FIntPoint Ch = W->CellToChunk(Here) + FIntPoint(DX, DY);
					Missing += (W->IsChunkInBounds(Ch) && !W->IsChunkLoaded(Ch)) ? 1 : 0;
				}
			}
			if (Missing > 0 && StepTime < 20.f)
			{
				return false;
			}
			const FBRLevelDef& D = W->Def();
			// a. Empreinte (meme portage que le banc ; ABRWorld l'ecrit aussi au journal, chez l'hote et chez chaque client)
			const uint32 Got = W->GetLightGridFingerprint(PrintHalf);
			const FString PrintText = FString::Printf(TEXT("Niveau %d, -BRSeed=%u (graine du niveau %u) : empreinte de la grille de lumiere %08x, banc %08x"),
				Level, UserSeed, W->GetSeed(), Got, Expected);
			Note(PrintText, Got != Expected);
			// b. Points d'emission des plafonniers de jeu = lumieres construites
			TArray<FVector> Built;
			for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
			{
				if (!It->IsReady())
				{
					continue;
				}
				TInlineComponentArray<ULocalLightComponent*> Lights(*It);
				for (const ULocalLightComponent* L : Lights)
				{
					Built.Add(L->GetComponentLocation());
				}
			}
			// Echelles des sorties : une echelle perce le plafond d'une trappe et le chunk ne pose pas le plafonnier a moins de 70 cm
			// d'elle (ABRChunk, etape des lumieres) ; la regle le compte (avant comme apres la v4.16) : ecart connu, note a part
			TArray<FVector> Ladders;
			for (TActorIterator<ABRExit> It(GetWorld()); It; ++It)
			{
				if (It->Style == EBRExitStyle::Ladder)
				{
					Ladders.Add(It->GetActorLocation());
				}
			}
			int32 Lamps = 0;
			int32 Matched = 0;
			int32 Off = 0;
			int32 Hatch = 0;
			FString FirstOff;
			const int32 Span = 10;
			for (int32 DX = -Span; DX <= Span; ++DX)
			{
				for (int32 DY = -Span; DY <= Span; ++DY)
				{
					const FIntPoint Cell = Here + FIntPoint(DX, DY);
					if (!W->IsChunkLoaded(W->CellToChunk(Cell)))
					{
						continue;
					}
					const FBRLightCell L = W->GetLightCell(Cell.X, Cell.Y);
					if (!L.bLamp)
					{
						continue;
					}
					++Lamps;
					bool bFound = false;
					for (const FVector& B : Built)
					{
						// Point d'emission : FLightRecord::Pos ; une lumiere surfacique est posee juste sous le plafond, a la meme verticale
						if (FVector::Dist2D(B, L.Emitter) < 2.0 && B.Z <= D.WallHeight + 1.0 && B.Z >= L.Emitter.Z - 1.0)
						{
							bFound = true;
							break;
						}
					}
					Matched += bFound ? 1 : 0;
					bool bNearLadder = false;
					for (const FVector& Lad : Ladders)
					{
						bNearLadder = bNearLadder || FVector::Dist2D(Lad, L.Anchor) < 260.0;
					}
					if (!bFound && bNearLadder)
					{
						++Hatch;
					}
					else if (!bFound && ++Off == 1)
					{
						FirstOff = FString::Printf(TEXT("cellule (%d, %d), emetteur %s"), Cell.X, Cell.Y, *L.Emitter.ToCompactString());
					}
				}
			}
			if (Lamps == 0)
			{
				Skip(FString::Printf(TEXT("Niveau %d : aucun plafonnier de jeu dans les chunks construits (points d'emission non compares)"), Level));
			}
			else
			{
				Note(FString::Printf(TEXT("Niveau %d : %d plafonnier(s) de jeu dans les chunks construits, %d a la place d'une lumiere construite (%d lumiere(s) dans les chunks) ; %d pres d'une trappe d'echelle (non construit, compte par la regle : ecart connu)%s%s"),
					Level, Lamps, Matched, Built.Num(), Hatch, Off > 0 ? TEXT(" ; premier ecart : ") : TEXT(""), *FirstOff), Off > 0);
			}
			// c. Accord de la grille avec une trace physique (emetteur -> point), d. positions dont la lumiere change
			int32 Pairs = 0;
			int32 Agree = 0;
			int32 GridHides = 0;
			int32 PhysHides = 0;
			TMap<FString, int32> HitNames;
			int32 Cells = 0;
			int32 Changed = 0;
			int32 DarkOld = 0;
			int32 DarkNew = 0;
			int32 VanishOld = 0;
			int32 VanishNew = 0;
			const float S = D.CellSize;
			const float Free = S - D.WallThickness - 80.f;
			const int32 R = FMath::Clamp(FMath::CeilToInt(D.LightRadius / D.CellSize), 1, 4);
			for (int32 DX = -8; DX <= 8; ++DX)
			{
				for (int32 DY = -8; DY <= 8; ++DY)
				{
					const FIntPoint Cell = Here + FIntPoint(DX, DY);
					if (!UsableCell(W, Cell))
					{
						continue;
					}
					const uint32 Hh = BRHash::Hash(Cell.X, Cell.Y, 9001, W->GetSeed());
					const FVector Center = W->CellCenter(Cell);
					const FVector Pxy(Center.X + ((Hh & 0xFFFu) / 4095.0 - 0.5) * Free, Center.Y + (((Hh >> 12) & 0xFFFu) / 4095.0 - 0.5) * Free, 0.0);
					const float Floor = W->FloorZAt(Pxy);
					const FVector Player(Pxy.X, Pxy.Y, Floor + 92.f);
					const FVector Smiler(Pxy.X, Pxy.Y, Floor + 165.f);
					const FBRLightSample SP = W->SampleLight(Player, nullptr, false);
					const FBRLightSample SS = W->SampleLight(Smiler, nullptr, false);
					++Cells;
					Changed += SP.CeilingAll != SP.CeilingVisible ? 1 : 0;
					DarkOld += SP.CeilingAll < 0.12f ? 1 : 0;
					DarkNew += SP.CeilingVisible < 0.12f ? 1 : 0;
					VanishOld += SS.CeilingAll > BRLight::SmilerVanish ? 1 : 0;
					VanishNew += SS.CeilingVisible > BRLight::SmilerVanish ? 1 : 0;
					if (Pairs >= 900)
					{
						continue;
					}
					for (int32 LX = -R; LX <= R; ++LX)
					{
						for (int32 LY = -R; LY <= R; ++LY)
						{
							const FIntPoint LampCell = Cell + FIntPoint(LX, LY);
							const FBRLightCell L = W->GetLightCell(LampCell.X, LampCell.Y);
							if (!L.bLamp || !W->IsChunkLoaded(W->CellToChunk(LampCell)) || LampWeight(W, L, Player) <= 0.f)
							{
								continue;
							}
							for (const FVector& P : { Player, Smiler })
							{
								const bool bGrid = W->IsLampVisibleFrom(LampCell, P);
								FString Hit;
								const bool bPhys = PhysicsSees(GetWorld(), C, L.Emitter, P, &Hit);
								++Pairs;
								Agree += bGrid == bPhys ? 1 : 0;
								GridHides += (!bGrid && bPhys) ? 1 : 0;
								PhysHides += (bGrid && !bPhys) ? 1 : 0;
								if (bGrid != bPhys)
								{
									// Nom de l'obstacle sans le numero d'instance (regroupement)
									FString Key = bPhys ? FString(TEXT("(rien : la grille cache)")) : Hit.Left(Hit.Find(TEXT(" a ")));
									HitNames.FindOrAdd(Key) += 1;
								}
							}
						}
					}
				}
			}
			FString Top;
			HitNames.ValueSort([](int32 A, int32 B) { return A > B; });
			int32 Shown = 0;
			for (const TPair<FString, int32>& Pair : HitNames)
			{
				if (Shown++ >= 4)
				{
					break;
				}
				Top += FString::Printf(TEXT("%s%s x%d"), Top.IsEmpty() ? TEXT("") : TEXT(", "), *Pair.Key, Pair.Value);
			}
			TotalPairs += Pairs;
			TotalAgree += Agree;
			if (Pairs == 0)
			{
				Skip(FString::Printf(TEXT("Niveau %d : aucune paire plafonnier / point dans les chunks construits (accord avec la physique non mesure)"), Level));
			}
			else
			{
				const float Pct = 100.f * Agree / Pairs;
				// Un desaccord massif voudrait dire que la grille ne decrit pas le decor construit (seuil large : objets poses, rochers,
				// ecoincons sans collision restent dans les ecarts)
				Note(FString::Printf(TEXT("Niveau %d : regle de la grille et trace physique d'accord sur %d / %d paires (%.1f %%) ; la grille seule cache : %d ; la physique seule cache : %d%s%s"),
					Level, Agree, Pairs, Pct, GridHides, PhysHides, Top.IsEmpty() ? TEXT("") : TEXT(" ; ecarts : "), *Top), Pct < 80.f);
			}
			if (Cells > 0)
			{
				Note(FString::Printf(TEXT("Niveau %d (graine %u) : %d positions du joueur, lumiere changee pour %.1f %% ; joueur dans le noir %.1f -> %.1f %% ; Smiler qui se dissipe %.1f -> %.1f %% (regle d'avant -> v4.16, courant plein)"),
					Level, W->GetSeed(), Cells, 100.f * Changed / Cells, 100.f * DarkOld / Cells, 100.f * DarkNew / Cells, 100.f * VanishOld / Cells, 100.f * VanishNew / Cells));
			}
			return true;
		});
		LimitLastStep(40.f);
	}

	// ------------------------------------------------------------------------------------------------ 2. Level 0, graine 9
	AddSeedLoad(0, 9u, TEXT("v4.16 lumiere : Level 0, -BRSeed=9 (scenes de controle)"));
	Add(TEXT("v4.16 lumiere : scenes du mur, du passage, points de controle"), 2.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C || W->GetLevelNumber() != 0)
		{
			return false;
		}
		const FBRLevelDef& D = W->Def();
		const float S = D.CellSize;
		const float Face = S * 0.5f - D.WallThickness * 0.5f;
		const FIntPoint Start = W->WorldToCell(C->GetActorLocation());
		bWallScene = bOpenScene = bDoorScene = false;
		// Mur : A sans plafonnier, B derriere un mur avec un plafonnier stable dont l'apport au point depasse NeedsDark
		bWallScene = ForRings(Start, 8, [&](const FIntPoint& A)
		{
			if (!UsableCell(W, A) || W->GetLightCell(A.X, A.Y).bLamp)
			{
				return false;
			}
			for (const FIntPoint& Dir : Dirs)
			{
				const FIntPoint B = A + Dir;
				const FBRLightCell L = W->GetLightCell(B.X, B.Y);
				if (EdgeToward(W, A, Dir) != static_cast<uint8>(EBREdge::Wall) || !UsableCell(W, B) || !StableLamp(L))
				{
					continue;
				}
				const FVector P = CellPoint(W, A, Dir, Face - 40.f, 0.f, 165.f);
				if (LampWeight(W, L, P) < BRLight::NeedsDark)
				{
					continue;
				}
				WallA = A;
				WallB = B;
				WallDir = Dir;
				WallSmilerP = P;
				WallPlayerP = CellPoint(W, A, Dir, Face - 40.f, 0.f, 92.f);
				return true;
			}
			return false;
		});
		// Sans mur : meme position relative, arete ouverte vers un plafonnier stable
		bOpenScene = ForRings(Start, 8, [&](const FIntPoint& A)
		{
			if (!UsableCell(W, A))
			{
				return false;
			}
			for (const FIntPoint& Dir : Dirs)
			{
				const FIntPoint B = A + Dir;
				if (EdgeToward(W, A, Dir) != static_cast<uint8>(EBREdge::Open) || !UsableCell(W, B) || !StableLamp(W->GetLightCell(B.X, B.Y)))
				{
					continue;
				}
				OpenA = A;
				OpenB = B;
				OpenP = CellPoint(W, A, Dir, Face - 40.f, 0.f, 165.f);
				return true;
			}
			return false;
		});
		// Passage : arete Door vers un plafonnier stable
		bDoorScene = ForRings(Start, 10, [&](const FIntPoint& A)
		{
			if (!UsableCell(W, A))
			{
				return false;
			}
			for (const FIntPoint& Dir : Dirs)
			{
				const FIntPoint B = A + Dir;
				if (EdgeToward(W, A, Dir) != static_cast<uint8>(EBREdge::Door) || !UsableCell(W, B) || !StableLamp(W->GetLightCell(B.X, B.Y)))
				{
					continue;
				}
				DoorA = A;
				DoorB = B;
				DoorDir = Dir;
				return true;
			}
			return false;
		});

		// a. Meme endroit, avec et sans mur
		if (!bWallScene)
		{
			Skip(TEXT("Level 0 : aucun mur avec un plafonnier stable derriere a moins de 8 cellules (scene du mur)"));
		}
		else
		{
			const FBRLightCell L = W->GetLightCell(WallB.X, WallB.Y);
			for (const FVector& P : { WallSmilerP, WallPlayerP })
			{
				const FBRLightSample Sm = W->SampleLight(P, nullptr, true);
				const bool bVisible = W->IsLampVisibleFrom(WallB, P);
				FString Hit;
				const bool bPhys = PhysicsSees(GetWorld(), C, L.Emitter, P, &Hit);
				const float Wt = LampWeight(W, L, P);
				Note(FString::Printf(TEXT("mur (cellule (%d, %d), plafonnier (%d, %d) derriere le mur, point a %.0f cm du sol) : lumiere de jeu %.3f (plafonniers %.3f dont %d/%d caches ; regle d'avant %.3f) ; apport du plafonnier derriere le mur %.3f : %s ; trace physique : %s"),
					WallA.X, WallA.Y, WallB.X, WallB.Y, P.Z - W->FloorZAt(P), Sm.Value, Sm.CeilingVisible, Sm.LampsHidden, Sm.Lamps, Sm.CeilingAll * Sm.Power, Wt,
					bVisible ? TEXT("COMPTE") : TEXT("ne compte plus"), bPhys ? TEXT("rien") : *Hit), bVisible);
				if (bPhys)
				{
					Note(TEXT("mur : la trace physique de la lampe au point ne touche rien alors que la grille y voit un mur"), true);
				}
			}
			if (bOpenScene)
			{
				const FBRLightCell OL = W->GetLightCell(OpenB.X, OpenB.Y);
				const bool bVisible = W->IsLampVisibleFrom(OpenB, OpenP);
				FString Hit;
				const bool bPhys = PhysicsSees(GetWorld(), C, OL.Emitter, OpenP, &Hit);
				Note(FString::Printf(TEXT("sans mur (cellule (%d, %d), plafonnier (%d, %d) de l'autre cote d'une arete ouverte, meme position) : apport %.3f %s ; trace physique : %s"),
					OpenA.X, OpenA.Y, OpenB.X, OpenB.Y, LampWeight(W, OL, OpenP), bVisible ? TEXT("compte") : TEXT("NE COMPTE PAS"), bPhys ? TEXT("rien") : *Hit), !bVisible);
			}
			else
			{
				Skip(TEXT("Level 0 : aucune arete ouverte vers un plafonnier stable (meme endroit sans mur)"));
			}
			// Un Smiler pose dans le coin : dissipe avant la v4.16 si la regle d'avant y depassait le seuil
			const FBRLightSample Sm = W->SampleLight(WallSmilerP, nullptr, false);
			if (ABREntity* E = SpawnFrozen(W, EBREntityKind::Smiler, WallSmilerP, 0.05f))
			{
				E->SetActorLocation(WallSmilerP);
				WallSmiler = E;
				TestSpawned.Add(E);
				Note(FString::Printf(TEXT("mur : Smiler pose au point (lumiere de jeu %.3f, regle d'avant %.3f, dissipation au-dessus de %.2f)"), Sm.Value, Sm.CeilingAll * Sm.Power,
					BRLight::SmilerVanish));
			}
			PlacePlayer(W, C, CellPoint(W, WallA, WallDir, -(Face - 50.f), 0.f, 0.f));
			if (AController* Ctrl = C->GetController())
			{
				Ctrl->SetControlRotation((WallSmilerP - C->GetEyeLocation()).Rotation());
			}
		}

		// b. Passage : dans l'axe pres du mur, au fond de la cellule (joueur, Smiler), derriere le jambage
		if (!bDoorScene)
		{
			Skip(TEXT("Level 0 : aucun passage vers un plafonnier stable a moins de 10 cellules (scene du passage)"));
		}
		else
		{
			const FBRLightCell L = W->GetLightCell(DoorB.X, DoorB.Y);
			const FBRLightCell AL = W->GetLightCell(DoorA.X, DoorA.Y);
			struct FDoorProbe
			{
				const TCHAR* Name;
				float Off;
				float Side;
				float Z;
			};
			const float Jamb = static_cast<float>(W->GetLightGeom().DoorHalf) + 70.f;
			const FDoorProbe DoorProbes[] = { { TEXT("axe, 40 cm du mur, joueur"), Face - 40.f, 0.f, 92.f }, { TEXT("axe, 40 cm du mur, Smiler"), Face - 40.f, 0.f, 165.f },
				{ TEXT("axe, fond de la cellule, joueur"), -(Face - 40.f), 0.f, 92.f }, { TEXT("axe, fond de la cellule, Smiler"), -(Face - 40.f), 0.f, 165.f },
				{ TEXT("derriere le jambage, joueur"), Face - 40.f, Jamb, 92.f } };
			for (const FDoorProbe& DP : DoorProbes)
			{
				const FVector P = CellPoint(W, DoorA, DoorDir, DP.Off, DP.Side, DP.Z);
				const bool bVisible = W->IsLampVisibleFrom(DoorB, P);
				FString Hit;
				const bool bPhys = PhysicsSees(GetWorld(), C, L.Emitter, P, &Hit);
				const bool bExpectSeen = DP.Side == 0.f && DP.Off > 0.f && DP.Z < 100.f;
				const bool bExpectHidden = DP.Side > 0.f;
				Note(FString::Printf(TEXT("passage (cellule (%d, %d), plafonnier (%d, %d)%s) : %s : %s (apport %.3f) ; trace physique : %s"), DoorA.X, DoorA.Y, DoorB.X, DoorB.Y,
					AL.bLamp ? TEXT(", la cellule a son plafonnier") : TEXT(""), DP.Name, bVisible ? TEXT("eclaire") : TEXT("cache"), LampWeight(W, L, P), bPhys ? TEXT("rien") : *Hit),
					(bExpectSeen && !bVisible) || (bExpectHidden && bVisible));
			}
		}

		// Points de controle (compares d'un profil a l'autre, d'une coupure et d'un rechargement) : scenes et 24 cellules
		Probes.Reset();
		if (bWallScene)
		{
			Probes.Add(WallSmilerP);
			Probes.Add(WallPlayerP);
		}
		if (bOpenScene)
		{
			Probes.Add(OpenP);
		}
		if (bDoorScene)
		{
			Probes.Add(CellPoint(W, DoorA, DoorDir, -(Face - 40.f), 0.f, 165.f));
		}
		ForRings(Start, 6, [&](const FIntPoint& Cell)
		{
			if (UsableCell(W, Cell))
			{
				const uint32 Hh = BRHash::Hash(Cell.X, Cell.Y, 9002, W->GetSeed());
				Probes.Add(CellPoint(W, Cell, FIntPoint(1, 0), ((Hh & 0xFFu) / 255.f - 0.5f) * (S - 200.f), (((Hh >> 8) & 0xFFu) / 255.f - 0.5f) * (S - 200.f),
					(Hh & 0x10000u) ? 165.f : 92.f));
			}
			return Probes.Num() >= 28;
		});
		// Point bien eclaire (coupure)
		LitP = FVector::ZeroVector;
		float Best = 0.f;
		for (const FVector& P : Probes)
		{
			const float V = W->SampleLight(P, nullptr, false).CeilingVisible;
			if (V > Best)
			{
				Best = V;
				LitP = P;
			}
		}
		Sample(W, Baseline);
		Note(FString::Printf(TEXT("%d points de controle releves (point le plus eclaire : plafonniers %.3f)"), Probes.Num(), Best));
		return true;
	});
	Add(TEXT("v4.16 lumiere : Smiler du coin sombre"), 1.5f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (!bWallScene)
		{
			return true;
		}
		Shot(TEXT("v416_mur_smiler"));
		const FBRLightSample Sm = W->SampleLight(WallSmilerP, nullptr, false);
		const ABREntity* E = WallSmiler.Get();
		const bool bVanishing = !E || E->IsVanishing();
		if (Sm.Value > BRLight::SmilerVanish)
		{
			Note(FString::Printf(TEXT("mur : le point reste eclaire par d'autres plafonniers visibles (%.3f) : le Smiler %s (attendu : il se dissipe)"), Sm.Value,
				bVanishing ? TEXT("se dissipe") : TEXT("reste")), !bVanishing);
		}
		else
		{
			const bool bOldWould = Sm.CeilingAll * Sm.Power > BRLight::SmilerVanish;
			Note(FString::Printf(TEXT("mur : lumiere de jeu %.3f sous le seuil (regle d'avant %.3f%s) : le Smiler %s (capture v416_mur_smiler)"), Sm.Value, Sm.CeilingAll * Sm.Power,
				bOldWould ? TEXT(" : il s'y dissipait") : TEXT(""), bVanishing ? TEXT("SE DISSIPE") : TEXT("reste dans le coin sombre")), bVanishing);
		}
		if (ABREntity* Live = WallSmiler.Get())
		{
			Live->Destroy();
		}
		WallSmiler.Reset();
		return true;
	});

	// Etat lisible de la lumiere qui compte (inventaire, sous la sante mentale) : capture a juger (petite fenetre, langues)
	Add(TEXT("v4.16 lumiere : inventaire ouvert"), 0.8f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return false;
		}
		PC->SetInventoryOpen(true);
		return true;
	});
	Add(TEXT("v4.16 lumiere : capture de l'inventaire"), 0.3f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		const float V = W->LightLevelAt(C->GetActorLocation(), C);
		static const TCHAR* const Words[] = { TEXT("dans le noir"), TEXT("penombre"), TEXT("eclaire") };
		Note(FString::Printf(TEXT("inventaire : lumiere de jeu du joueur %.3f : \"%s\" affiche sous la sante mentale (capture v416_inventaire_lumiere)"), V,
			Words[BRLight::Band(V)]));
		Shot(TEXT("v416_inventaire_lumiere"));
		return true;
	});
	Add(TEXT("v4.16 lumiere : inventaire ferme"), 0.3f, [this]()
	{
		if (ABRPlayerController* PC = GetPC())
		{
			PC->SetInventoryOpen(false);
		}
		return true;
	});

	// ------------------------------------------------------------------------------------------------ coupure : alimentation des regles
	struct FPhaseRun
	{
		const TCHAR* Name;
		int32 Flashes;
		bool bFailing;
	};
	const FPhaseRun Runs[] = { { TEXT("defaillance, flashs normaux"), 0, true }, { TEXT("retour, flashs attenues"), 1, false } };
	for (const FPhaseRun& Run : Runs)
	{
		const FString Name = Run.Name;
		const int32 Flashes = Run.Flashes;
		const bool bFailing = Run.bFailing;
		Add(FString::Printf(TEXT("v4.16 lumiere : coupure (%s) : depart"), *Name), 0.f, [this, Flashes, bFailing]()
		{
			ABRWorld* W = GetBRWorld();
			ABRPlayerController* PC = GetPC();
			if (!W || !PC)
			{
				return false;
			}
			FBRSettings::Get().Flashes = Flashes;
			PC->ApplySettings();
			PhaseFrames = PhaseMismatch = PhaseNonMono = PhaseDecoupled = 0;
			PhasePrev = -1.f;
			PhaseFirstBad.Reset();
			if (bFailing)
			{
				W->ForceBlackout();
			}
			else
			{
				W->ForceRestore();
			}
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 lumiere : coupure (%s) : chaque image"), *Name), 0.f, [this, Name, bFailing]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W)
			{
				return false;
			}
			const bool bInPhase = W->IsPowerFlickering();
			if (bInPhase)
			{
				const FBRLightSample Sm = W->SampleLight(LitP, nullptr, false);
				const float Expected = FMath::Clamp(Sm.Power >= 0.05f ? Sm.CeilingVisible * Sm.Power : 0.f, 0.f, 1.5f);
				const float Got = W->LightLevelAt(LitP);
				++PhaseFrames;
				if (Got != Expected)
				{
					++PhaseMismatch;
					if (PhaseFirstBad.IsEmpty())
					{
						PhaseFirstBad = FString::Printf(TEXT("image %d : %.4f au lieu de %.4f"), PhaseFrames, Got, Expected);
					}
				}
				if (PhasePrev >= 0.f && (bFailing ? Sm.Power > PhasePrev + 1e-6f : Sm.Power < PhasePrev - 1e-6f))
				{
					++PhaseNonMono;
				}
				PhaseDecoupled += FMath::Abs(Sm.VisualPower - Sm.Power) > 0.05f ? 1 : 0;
				PhasePrev = Sm.Power;
				return false;
			}
			if (PhaseFrames == 0)
			{
				if (StepTime < 5.f)
				{
					return false;
				}
				Skip(FString::Printf(TEXT("coupure (%s) : la phase de vacillement n'a pas ete observee"), *Name));
				return true;
			}
			Note(FString::Printf(TEXT("coupure (%s) : %d image(s) ; lumiere de jeu = plafonniers visibles x alimentation des regles a chaque image : %s ; alimentation %s ; vacillement local different des regles sur %d image(s)%s%s"),
				*Name, PhaseFrames, PhaseMismatch == 0 ? TEXT("oui") : TEXT("NON"), PhaseNonMono == 0 ? (bFailing ? TEXT("toujours decroissante") : TEXT("toujours croissante")) : TEXT("NON MONOTONE"),
				PhaseDecoupled, PhaseFirstBad.IsEmpty() ? TEXT("") : TEXT(" ; ecart : "), *PhaseFirstBad), PhaseMismatch > 0 || PhaseNonMono > 0);
			return true;
		});
		LimitLastStep(15.f);
		if (bFailing)
		{
			// Noir : source eteinte
			Add(TEXT("v4.16 lumiere : coupure : noir"), 0.f, [this]()
			{
				ABRWorld* W = GetBRWorld();
				if (!W)
				{
					return false;
				}
				if (!W->IsBlackout() || W->GetGameplayPower() > 0.f)
				{
					if (StepTime > 8.f)
					{
						Skip(TEXT("coupure : le noir n'a pas ete atteint"));
						return true;
					}
					return false;
				}
				TArray<float> Dark;
				Sample(W, Dark);
				float Max = 0.f;
				for (const float V : Dark)
				{
					Max = FMath::Max(Max, V);
				}
				Note(FString::Printf(TEXT("coupure : noir : alimentation des regles %.2f (image : %.2f) ; plus forte lumiere de jeu aux %d points : %.3f"), W->GetGameplayPower(),
					W->GetPower(), Dark.Num(), Max), Max > 0.f);
				return true;
			});
			LimitLastStep(15.f);
		}
	}
	Add(TEXT("v4.16 lumiere : courant revenu"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC)
		{
			return false;
		}
		if (!PowerSteady(W))
		{
			if (StepTime > 10.f)
			{
				Skip(TEXT("courant revenu : le courant plein n'est pas revenu en 10 s (valeurs non comparees)"));
				return true;
			}
			return false;
		}
		FBRSettings::Get().Flashes = 0;
		PC->ApplySettings();
		TArray<float> Back;
		Sample(W, Back);
		const int32 Diff = CountDiff(Back, Baseline);
		Note(FString::Printf(TEXT("courant revenu : %d / %d point(s) differents des valeurs d'avant la coupure"), Diff, Baseline.Num()), Diff > 0);
		return true;
	});
	LimitLastStep(15.f);

	// ------------------------------------------------------------------------------------------------ profils graphiques
	struct FProfileRun
	{
		int32 Index;
		const TCHAR* Name;
	};
	const FProfileRun ProfileRuns[] = { { 0, TEXT("Performance") }, { 1, TEXT("Qualite") }, { 4, TEXT("RTX fluide") }, { 2, TEXT("Cinematique") } };
	for (const FProfileRun& PR : ProfileRuns)
	{
		const int32 Index = PR.Index;
		const FString Name = PR.Name;
		Add(FString::Printf(TEXT("v4.16 lumiere : profil %s"), *Name), 1.5f, [this, Index]()
		{
			ABRPlayerController* PC = GetPC();
			if (!PC)
			{
				return false;
			}
			ABRPlayerController::ApplyGraphicsProfile(Index);
			PC->ApplySettings();
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 lumiere : profil %s : valeurs"), *Name), 0.f, [this, Name]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W)
			{
				return false;
			}
			if (!PowerSteady(W))
			{
				// Coupure naturelle pendant la mesure : on attend le courant plein des valeurs de reference
				if (StepTime > 55.f)
				{
					Skip(FString::Printf(TEXT("profil %s : coupure de courant en cours pendant toute la mesure"), *Name));
					return true;
				}
				return false;
			}
			TArray<float> Now;
			Sample(W, Now);
			const int32 Diff = CountDiff(Now, Baseline);
			Note(FString::Printf(TEXT("profil %s (MegaLights %s) : %d / %d point(s) differents (au bit pres)"), *Name, ABRWorld::IsMegaLightsActive() ? TEXT("actif") : TEXT("inactif"),
				Diff, Baseline.Num()), Diff > 0);
			return true;
		});
	}
	Add(TEXT("v4.16 lumiere : ombres de toutes les lampes et neons surfaciques coupes"), 1.5f, [this]()
	{
		ABRPlayerController* PC = GetPC();
		if (!PC)
		{
			return false;
		}
		FBRSettings::Get().bMegaLights = false;
		FBRSettings::Get().bAreaLights = false;
		FBRSettings::Get().Flashes = 2;
		PC->ApplySettings();
		return true;
	});
	Add(TEXT("v4.16 lumiere : reglages coupes : valeurs"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRPlayerController* PC = GetPC();
		if (!W || !PC)
		{
			return false;
		}
		if (!PowerSteady(W) && StepTime <= 55.f)
		{
			return false; // coupure naturelle : on attend le courant plein des valeurs de reference
		}
		TArray<float> Now;
		Sample(W, Now);
		const int32 Diff = CountDiff(Now, Baseline);
		if (!PowerSteady(W))
		{
			Skip(TEXT("reglages coupes : coupure de courant en cours pendant toute la mesure"));
		}
		else
		{
			Note(FString::Printf(TEXT("ombres de toutes les lampes, neons surfaciques et flashs coupes : %d / %d point(s) differents"), Diff, Baseline.Num()), Diff > 0);
		}
		if (bSettingsSaved)
		{
			FBRSettings::Get() = SavedSettings;
			FBRSettings::Get().Flashes = 0;
			PC->ApplySettings();
		}
		return true;
	});

	// ------------------------------------------------------------------------------------------------ 3. source de mission (3.2)
	Add(TEXT("v4.16 lumiere : source de mission, pose"), 0.8f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		// Zone du depart (cellules -1..1, aretes ouvertes) : source au bord ouest de la cellule (0, 0), joueur au centre de (1, 0).
		// Le joueur se pose (5 cm de chute) avant les mesures
		const float S = W->Def().CellSize;
		SourceP = FVector(0.0, 0.5 * S, W->FloorZAt(FVector(1.0, 0.5 * S, 0.0)) + 150.0);
		PlacePlayer(W, C, FVector(1.5 * S, 0.5 * S, 0.0));
		W->SetGameplayLight(this, TestSlot, SourceP, BRLight::BeaconRadius, BRLight::BeaconIntensity, true);
		return true;
	});
	Add(TEXT("v4.16 lumiere : source de mission, rien entre"), 0.3f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		SourceAdd = BRTestV416Light::SourceLaw(C);
		const FBRLightSample Sm = W->SampleLight(C->GetActorLocation(), C, true);
		ClearMission = Sm.Mission;
		Note(FString::Printf(TEXT("source de mission : rien entre la source et le joueur (%.0f cm) : apport %.3f (loi %.3f), filtre d'avant %.3f ; obstacles : %s / avant : %s"),
			FVector::Dist(C->GetActorLocation(), SourceP), Sm.Mission, SourceAdd, Sm.MissionOldFilter, Sm.MissionHits.IsEmpty() ? TEXT("aucun") : *Sm.MissionHits,
			Sm.MissionOldHits.IsEmpty() ? TEXT("aucun") : *Sm.MissionOldHits), FMath::Abs(Sm.Mission - SourceAdd) > 1e-4f || SourceAdd <= 0.01f);
		return true;
	});
	struct FBlock
	{
		const TCHAR* Name;
		int32 Kind; // 0 coequipier, 1 creature, 2 porte fermee, 3 porte ouverte, 4 source eteinte
	};
	const FBlock Blocks[] = { { TEXT("capsule de coequipier entre les deux"), 0 }, { TEXT("creature (Skin-Stealer) entre les deux"), 1 }, { TEXT("porte physique fermee entre les deux"), 2 },
		{ TEXT("porte ouverte"), 3 }, { TEXT("source eteinte"), 4 } };
	for (const FBlock& Bk : Blocks)
	{
		const FString Name = Bk.Name;
		const int32 Kind = Bk.Kind;
		Add(FString::Printf(TEXT("v4.16 lumiere : source de mission, %s : pose"), *Name), 0.3f, [this, Kind]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			const FVector Mid = (SourceP + C->GetActorLocation()) * 0.5;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (Kind == 0)
			{
				// Capsule d'un personnage (profil Pawn, celui du joueur : ABRCharacter n'en change pas), immobile
				if (ACharacter* M = GetWorld()->SpawnActor<ACharacter>(ACharacter::StaticClass(), FTransform(Mid), Params))
				{
					M->GetCapsuleComponent()->SetCapsuleSize(C->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), C->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
					M->SetActorLocation(FVector(Mid.X, Mid.Y, C->GetActorLocation().Z));
					M->GetCharacterMovement()->DisableMovement();
					Mate = M;
				}
			}
			else if (Kind == 1)
			{
				if (AActor* M = Mate.Get())
				{
					M->Destroy();
				}
				if (ABREntity* E = SpawnFrozen(W, EBREntityKind::SkinStealer, Mid, 0.02f))
				{
					Creature = E;
					TestSpawned.Add(E);
				}
			}
			else if (Kind == 2)
			{
				if (ABREntity* E = Creature.Get())
				{
					E->Destroy();
				}
				// Meme reglage que le battant d'un mecanisme ferme (ABRMissionDevice::Blocker) : boite WorldDynamic qui bloque tout
				if (AActor* A = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Mid), Params))
				{
					UBoxComponent* B = NewObject<UBoxComponent>(A, TEXT("V416DoorBlocker"));
					A->SetRootComponent(B);
					B->RegisterComponent();
					B->SetWorldLocation(FVector(Mid.X, Mid.Y, W->FloorZAt(Mid) + 125.0));
					B->SetBoxExtent(FVector(15.f, 90.f, 125.f));
					B->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					B->SetCollisionResponseToAllChannels(ECR_Block);
					Door = A;
				}
			}
			else if (Kind == 3)
			{
				// Ouverte : comme un battant ouvert, la boite ne bloque plus
				if (AActor* A = Door.Get())
				{
					if (UPrimitiveComponent* B = Cast<UPrimitiveComponent>(A->GetRootComponent()))
					{
						B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					}
				}
			}
			else
			{
				W->SetGameplayLight(this, TestSlot, SourceP, BRLight::BeaconRadius, BRLight::BeaconIntensity, false);
			}
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 lumiere : source de mission, %s : valeur"), *Name), 0.f, [this, Kind, Name]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			if ((Kind == 0 && !Mate.IsValid()) || (Kind == 1 && !Creature.IsValid()) || (Kind == 2 && !Door.IsValid()))
			{
				Skip(FString::Printf(TEXT("source de mission, %s : obstacle de test non cree"), *Name));
				return true;
			}
			const FBRLightSample Sm = W->SampleLight(C->GetActorLocation(), C, true);
			const float Cached = W->LightLevelAt(C->GetActorLocation(), C);
			bool bBad = FMath::Abs(Cached - Sm.Value) > 1e-5f;
			// Sans obstacle : la loi a la position du moment (le joueur a pu se poser de quelques millimetres depuis la mesure a vide)
			const float Clear = Kind == 4 ? ClearMission : SourceLaw(C);
			FString Expect;
			if (Kind <= 1 || Kind == 3)
			{
				bBad = bBad || FMath::Abs(Sm.Mission - Clear) > 1e-4f;
				Expect = TEXT("attendu : comme sans obstacle");
			}
			else
			{
				bBad = bBad || Sm.Mission != 0.f;
				Expect = TEXT("attendu : 0");
			}
			const bool bOldCut = Sm.MissionOldFilter < Clear - 1e-4f;
			Note(FString::Printf(TEXT("source de mission, %s : apport %.3f (sans obstacle %.3f ; %s) ; filtre d'avant la v4.16 : %.3f%s ; obstacles touches : %s / avant : %s ; LightLevelAt %.3f, recalcul %.3f"),
				*Name, Sm.Mission, Clear, *Expect, Sm.MissionOldFilter, (Kind <= 1 && bOldCut) ? TEXT(" (defaut reproduit : la capsule coupait la lumiere)") : TEXT(""),
				Sm.MissionHits.IsEmpty() ? TEXT("aucun") : *Sm.MissionHits, Sm.MissionOldHits.IsEmpty() ? TEXT("aucun") : *Sm.MissionOldHits, Cached, Sm.Value), bBad);
			if (Kind <= 1 && !bOldCut)
			{
				Note(FString::Printf(TEXT("source de mission, %s : le filtre d'avant ne coupait pas non plus (la capsule n'est pas sur la ligne ?)"), *Name));
			}
			return true;
		});
	}
	Add(TEXT("v4.16 lumiere : source de mission derriere un mur"), 0.3f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if (AActor* A = Door.Get())
		{
			A->Destroy();
		}
		if (!bWallScene)
		{
			Skip(TEXT("source de mission derriere un mur : pas de scene du mur"));
			return true;
		}
		const float Face = W->Def().CellSize * 0.5f - W->Def().WallThickness * 0.5f;
		// Source dans B, a 60 cm de la face du mur ; joueur dans A, a 60 cm de l'autre face
		SourceP = CellPoint(W, WallB, FIntPoint(-WallDir.X, -WallDir.Y), Face - 60.f, 0.f, 150.f);
		PlacePlayer(W, C, CellPoint(W, WallA, WallDir, Face - 60.f, 0.f, 0.f));
		W->SetGameplayLight(this, TestSlot, SourceP, BRLight::BeaconRadius, BRLight::BeaconIntensity, true);
		return true;
	});
	Add(TEXT("v4.16 lumiere : source de mission derriere un mur : valeur"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if (!bWallScene)
		{
			return true;
		}
		const FBRLightSample Sm = W->SampleLight(C->GetActorLocation(), C, true);
		const float Add = BRLight::Contribution(static_cast<float>(FVector::Dist(C->GetActorLocation(), SourceP)), BRLight::BeaconRadius, BRLight::BeaconIntensity);
		Note(FString::Printf(TEXT("source de mission derriere un mur : apport %.3f (loi sans mur %.3f, attendu 0) ; obstacle touche : %s"), Sm.Mission, Add,
			Sm.MissionHits.IsEmpty() ? TEXT("aucun") : *Sm.MissionHits), Sm.Mission != 0.f || Sm.MissionHits.IsEmpty());
		return true;
	});

	// ------------------------------------------------------------------------------------------------ 4. cout par image
	Add(TEXT("v4.16 lumiere : cout, creatures immobiles"), 3.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		C->bGodMode = true;
		// Source de mission allumee pres des creatures (traces)
		SourceP = C->GetActorLocation() + FVector(0.f, 0.f, 60.f);
		W->SetGameplayLight(this, TestSlot, SourceP, BRLight::BeaconRadius, BRLight::BeaconIntensity, true);
		CostEntities.Reset();
		const EBREntityKind Kinds[] = { EBREntityKind::Smiler, EBREntityKind::Smiler, EBREntityKind::Deathmoth, EBREntityKind::Deathmoth, EBREntityKind::Bacteria };
		int32 K = 0;
		ForRings(W->WorldToCell(C->GetActorLocation()), 7, [&](const FIntPoint& Cell)
		{
			if (Cell == W->WorldToCell(C->GetActorLocation()) || !UsableCell(W, Cell))
			{
				return false;
			}
			// Un Smiler dans la lumiere se dissipe aussitot (et ne lit plus la lumiere) : cellules sombres seulement
			const FVector At = W->CellCenter(Cell);
			if (Kinds[K] == EBREntityKind::Smiler && W->LightLevelAt(FVector(At.X, At.Y, W->FloorZAt(At) + 165.f)) > BRLight::SmilerVanish)
			{
				return false;
			}
			if (ABREntity* E = SpawnFrozen(W, Kinds[K], At, 0.05f))
			{
				CostEntities.Add(E);
				TestSpawned.Add(E);
			}
			return ++K >= static_cast<int32>(UE_ARRAY_COUNT(Kinds));
		});
		W->ResetLightStats();
		CostFrame0 = GFrameCounter;
		CostT0 = FPlatformTime::Seconds();
		return true;
	});
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		Add(Pass == 0 ? FString(TEXT("v4.16 lumiere : cout, creatures immobiles : bilan")) : FString(TEXT("v4.16 lumiere : cout, creatures libres : bilan")), Pass == 0 ? 3.f : 0.f,
			[this, Pass]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W)
			{
				return false;
			}
			int32 Alive = 0;
			for (const TWeakObjectPtr<ABREntity>& E : CostEntities)
			{
				Alive += E.IsValid() ? 1 : 0;
			}
			const FBRLightStats T = W->GetLightStats(true);
			const double Frames = static_cast<double>(FMath::Max<uint64>(1, GFrameCounter - CostFrame0));
			const double PerEntity = Alive > 0 ? T.Traces / Frames / Alive : 0.0;
			Note(FString::Printf(TEXT("cout (%s, %d creature(s), %.0f images en %.1f s) : par image %.1f appel(s), %.1f calcul(s), %.1f repris du cache, %.1f plafonnier(s), %.0f obstacle(s), %.2f trace(s) (%.2f par creature), %.1f trace(s) reprise(s) ; %d chunk(s) de grille mis en cache"),
				Pass == 0 ? TEXT("immobiles") : TEXT("libres"), Alive, Frames, FPlatformTime::Seconds() - CostT0, T.Queries / Frames, T.Computes / Frames, T.CacheHits / Frames,
				T.Lamps / Frames, T.Boxes / Frames, T.Traces / Frames, PerEntity, T.TraceHits / Frames, T.ChunkBuilds), PerEntity > 2.0);
			if (Alive == 0)
			{
				Skip(TEXT("cout par image : aucune creature n'a pu etre posee"));
			}
			if (Pass == 0)
			{
				// Creatures libres pour la mesure suivante (elles bougent : moins de resultats repris du cache)
				for (const TWeakObjectPtr<ABREntity>& E : CostEntities)
				{
					if (ABREntity* Live = E.Get())
					{
						Live->CustomTimeDilation = 1.f;
					}
				}
				W->ResetLightStats();
				CostFrame0 = GFrameCounter;
				CostT0 = FPlatformTime::Seconds();
			}
			return true;
		});
	}
	Add(TEXT("v4.16 lumiere : temps d'un appel"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		for (const TWeakObjectPtr<ABREntity>& E : CostEntities)
		{
			if (ABREntity* Live = E.Get())
			{
				Live->Destroy();
			}
		}
		CostEntities.Reset();
		W->SetGameplayLight(this, TestSlot, SourceP, BRLight::BeaconRadius, BRLight::BeaconIntensity, false);
		// 2000 points distincts autour du joueur : cache vide (grille comprise), puis les memes points (cache plein)
		TArray<FVector> Pts;
		const FVector Base = C->GetActorLocation();
		for (int32 I = 0; I < 2000; ++I)
		{
			const uint32 Hh = BRHash::Hash(I, 17, 9003, W->GetSeed());
			Pts.Add(Base + FVector(((Hh & 0x3FFu) / 1023.0 - 0.5) * 2000.0, (((Hh >> 10) & 0x3FFu) / 1023.0 - 0.5) * 2000.0, ((Hh >> 20) & 0x7Fu) * 1.0 - 20.0));
		}
		W->InvalidateLightCache();
		W->ResetLightStats();
		double T0 = FPlatformTime::Seconds();
		float Sink = 0.f;
		for (const FVector& P : Pts)
		{
			Sink += W->LightLevelAt(P);
		}
		const double Cold = (FPlatformTime::Seconds() - T0) * 1.0e6 / Pts.Num();
		const FBRLightStats T = W->GetLightStats(true);
		T0 = FPlatformTime::Seconds();
		for (const FVector& P : Pts)
		{
			Sink += W->LightLevelAt(P);
		}
		const double Warm = (FPlatformTime::Seconds() - T0) * 1.0e6 / Pts.Num();
		Note(FString::Printf(TEXT("temps d'un appel de LightLevelAt : %.2f us (cache vide : %d chunk(s) de grille construits, %.1f plafonnier(s) et %.0f obstacle(s) par appel), %.3f us (cache plein) ; somme %.1f"),
			Cold, T.ChunkBuilds, static_cast<double>(T.Lamps) / Pts.Num(), static_cast<double>(T.Boxes) / Pts.Num(), Warm, Sink), Cold > 200.0);
		return true;
	});

	// ------------------------------------------------------------------------------------------------ meme graine rechargee
	AddSeedLoad(0, 9u, TEXT("v4.16 lumiere : Level 0, -BRSeed=9 recharge"));
	Add(TEXT("v4.16 lumiere : meme graine rechargee : valeurs"), 1.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->GetLevelNumber() != 0)
		{
			return false;
		}
		TArray<float> Now;
		Sample(W, Now);
		const int32 Diff = CountDiff(Now, Baseline);
		Note(FString::Printf(TEXT("Level 0 recharge (graine %u) : %d / %d point(s) differents de la premiere construction (meme regle chez l'hote et chez les clients : graine et grille)"),
			W->GetSeed(), Diff, Baseline.Num()), Diff > 0 || Baseline.Num() == 0);
		return true;
	});

	Add(TEXT("v4.16 lumiere : bilan"), 0.f, [this]()
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
		if (ABRWorld* W = GetBRWorld())
		{
			W->SetGameplayLight(this, TestSlot, SourceP, BRLight::BeaconRadius, BRLight::BeaconIntensity, false);
		}
		for (const TWeakObjectPtr<AActor>& A : { TWeakObjectPtr<AActor>(Mate.Get()), TWeakObjectPtr<AActor>(Creature.Get()), Door })
		{
			if (AActor* Live = A.Get())
			{
				Live->Destroy();
			}
		}
		FLevelReport R;
		R.Title = TEXT("v4.16 : lumiere de gameplay : bilan");
		R.Scene = TEXT("v4.16");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		Note(FString::Printf(TEXT("lumiere de gameplay : regle de la grille et trace physique d'accord sur %d / %d paires plafonnier / point (%.1f %%), tous niveaux"), TotalAgree,
			TotalPairs, TotalPairs > 0 ? 100.f * TotalAgree / TotalPairs : 0.f));
		return true;
	});
}
