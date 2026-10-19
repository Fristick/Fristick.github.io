// v4.16 : -BRAutoTest -BRAutoTestV416Light : lumiere de gameplay alignee sur la lumiere visible (prompt v4.16, 3.1 et 3.2 ;
// ABRWorld::LightLevelAt, BRWorldLight.cpp, BRLight::Occlusion).
//   1. Grille : sur 8 graines (Levels 0, 1, 2, 4, 5, 8 et 37), empreinte de la grille de lumiere (aretes, piliers, cellules
//      pleines, plafonniers) comparee a la table partagee avec le banc hors moteur (BRLightPrints.h ; Tools/Light/
//      test_light.cpp, section D : meme portage) ; relecture v4.16 : Niveau 9 de nuit (maisons, lampadaires : empreinte
//      relevee, le banc ne porte pas les rues) ; chaque plafonnier de jeu = une lumiere construite par les chunks, et aucun
//      sous une trappe d'echelle ; accord de la regle de la grille avec une trace physique de l'emetteur au point (murs du
//      decor construit) : la grille seule ne doit pas cacher plus de 3 % des paires (hors ecoincons d'arche sans collision
//      des Poolrooms, comptes a part ; les objets poses et rochers, que la physique seule voit, sont notes) ; part des
//      positions dont la lumiere change sur cette graine.
//   1 bis. Relecture v4.16 : trappes des echelles des chunks (partie classique imposee, Niveaux 8 et 37 aux graines des
//      tests v4.15) : trappe predite par le monde = echelle construite, plafonniers sous la trappe ni poses ni comptes.
//      Seuls les chunks construits ET en place comptent (un chunk qui sort de la vue reste IsReady() pendant son demontage,
//      ses sorties detruites d'abord) : ceux du depart sont verifies avant que le joueur parte ; il va ensuite a la trappe
//      visee, la premiere dont un plafonnier de la grille tombe dessous (cherchee jusqu'a 24 chunks du depart d'un niveau
//      ouvert : "plafonnier non pose" exerce), sinon la premiere ; un ecart d'echelle cite les sorties posees par le chunk.
//   2. Level 0, graine 9 (scene du kit, ombres MegaLights) : meme endroit avec et sans mur (le plafonnier derriere le mur ne
//      compte plus), arete fermee par le crochet de test (comme une porte de mission sur une ouverture de la grille : la
//      lumiere est coupee puis revient), Smiler du coin sombre (endroit ou la regle d'avant le dissipait et plus la v4.16, avec
//      un Smiler temoin dans la lumiere qui doit se dissiper), passage (axe, jambage, linteau selon la hauteur), coupure :
//      alimentation vue par les regles = fondu regulier a chaque image, quels que soient le vacillement local et le reglage
//      FLASHS (normaux puis attenues), source eteinte (noir : 0) ; profils Performance, Qualite, RTX fluide, Cinematique, puis
//      ombres de toutes les lampes et neons surfaciques coupes : memes valeurs au bit pres ; meme graine rechargee : memes
//      valeurs (comme un client qui construit le niveau).
//   3. Source de mission (3.2) : source de test enregistree comme celles des mecanismes (SetGameplayLight), point vise : le
//      joueur ; rien entre les deux (relecture v4.16 : le filtre d'avant renvoie 0, arrete par la capsule du joueur lui-meme :
//      le defaut d'avant reproduit) ; une capsule de coequipier (profil Pawn, comme un joueur), puis une creature, entre les
//      deux : la trace qui garde les capsules (sans la cible) doit toucher cet obstacle (sinon NON VERIFIE), et la valeur ne
//      baisse pas ; porte physique fermee (boite Blocker des mecanismes) : la lumiere est coupee, porte ouverte : elle
//      revient ; mur du decor : coupee ; source eteinte : 0 ; LightLevelAt (caches) = recalcul sans cache dans chaque etat.
//   4. Cout par image : creatures qui lisent la lumiere a chaque image (Smilers, Deathmoths, Bacteria), immobiles puis libres,
//      source de mission allumee pres d'elles : appels, calculs, cache (part reprise : noeuds de 10 cm), plafonniers,
//      obstacles, traces par image et par creature ; temps d'un appel (cache vide, puis plein).
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
#include "BRLightPrints.h"
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
	/** Graines de l'etape de la grille : la table partagee avec le banc (BRLightPrints.h), puis (relecture v4.16) le Niveau 9 de
	 *  nuit, sans empreinte du banc (il ne porte pas les rues) */
	struct FGridRun
	{
		int32 Level;
		uint32 UserSeed;
		uint32 Expected;
		bool bBench;
	};
	TArray<FGridRun> GridRuns()
	{
		TArray<FGridRun> Out;
		for (int32 K = 0; K < BRLightPrints::Count; ++K)
		{
			Out.Add({ BRLightPrints::Table[K].Level, BRLightPrints::Table[K].UserSeed, BRLightPrints::Table[K].Print, true });
		}
		Out.Add({ 9, 4242u, 0u, false });
		return Out;
	}
	constexpr int32 PrintHalf = BRLightPrints::Half;
	/** Relecture v4.16 : la grille seule (plus sombre que le decor construit) ne doit pas cacher plus de cette part des paires */
	constexpr float GridHideBudget = 0.03f;
	/** Relecture v4.16 : trappes des echelles des chunks, partie classique : graines du niveau des tests v4.15 (Level 8 :
	 *  echelle BRExit_42 vers le Niveau 9 ; Level 37 : echelles des chunks vers le Niveau 2) */
	struct FHatchRun
	{
		int32 Level;
		uint32 LevelSeed;
	};
	const FHatchRun HatchRuns[] = { { 8, 233637237u }, { 37, 115083815u } };
	/** Relecture v4.16 : niveau ouvert : la trappe visee (un plafonnier de la grille dessous, que le chunk ne pose pas) est
	 *  cherchee anneau par anneau jusqu'a ce nombre de chunks du depart quand les 12 x 12 chunks du centre n'en ont pas */
	constexpr int32 HatchSearchChunks = 24;

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

	/** Apport d'un plafonnier en P selon la loi d'apport (sans occlusion ni alimentation) ; aussi pour un plafonnier de la grille
	 *  sous une trappe (ce que la regle comptait sans qu'il soit pose) */
	float LampWeight(const ABRWorld* W, const FBRLightCell& L, const FVector& P)
	{
		const float Radius = W->Def().LightRadius;
		const float Dist = static_cast<float>(FVector::Dist2D(P, L.Anchor));
		if ((!L.bLamp && !L.bUnderHatch) || !(Dist < Radius))
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

	/** Relecture v4.16 : chunk construit et en place : pret ET pas en demontage. Un chunk qui sort de la vue (joueur deplace)
	 *  reste IsReady() pendant tout son demontage etale (ABRChunk::StepTeardown) ; ses acteurs partent les premiers (la derniere
	 *  sortie posee en tete) : ses echelles, objets et lumieres ne sont plus ceux de sa construction */
	bool LiveChunk(const ABRChunk& Chunk)
	{
		return Chunk.IsReady() && !Chunk.IsTearingDown();
	}

	/** Chunks construits et en place, par coordonnees */
	void LiveChunks(UWorld* InWorld, TMap<FIntPoint, ABRChunk*>& Out)
	{
		Out.Reset();
		for (TActorIterator<ABRChunk> It(InWorld); It; ++It)
		{
			if (LiveChunk(**It))
			{
				Out.Add(It->Coord, *It);
			}
		}
	}

	/** Lumieres construites par les chunks construits et en place (Only : ce chunk seulement) */
	void BuiltLights(UWorld* InWorld, const FIntPoint* Only, TArray<FVector>& Out)
	{
		Out.Reset();
		for (TActorIterator<ABRChunk> It(InWorld); It; ++It)
		{
			if (!LiveChunk(**It) || (Only && It->Coord != *Only))
			{
				continue;
			}
			TInlineComponentArray<ULocalLightComponent*> Lights(*It);
			for (const ULocalLightComponent* L : Lights)
			{
				Out.Add(L->GetComponentLocation());
			}
		}
	}

	/** Une lumiere construite a la place du plafonnier L : point d'emission (FLightRecord::Pos) ; une lumiere surfacique est
	 *  posee juste sous le plafond, a la meme verticale ; applique et lampadaire au-dessus de la hauteur des murs */
	bool HasBuiltLight(const ABRWorld* W, const FBRLightCell& L, const TArray<FVector>& Built)
	{
		const double Top = FMath::Max(static_cast<double>(W->Def().WallHeight), L.Emitter.Z) + 1.0;
		for (const FVector& B : Built)
		{
			if (FVector::Dist2D(B, L.Emitter) < 2.0 && B.Z <= Top && B.Z >= L.Emitter.Z - 1.0)
			{
				return true;
			}
		}
		return false;
	}

	/** Un des acteurs touches est A */
	bool HitsActor(const TArray<TWeakObjectPtr<const AActor>>& Hits, const AActor* A)
	{
		for (const TWeakObjectPtr<const AActor>& H : Hits)
		{
			if (A && H.Get() == A)
			{
				return true;
			}
		}
		return false;
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

	/** Nom lisible d'un style de sortie (diagnostic) */
	const TCHAR* ExitStyleName(EBRExitStyle Style)
	{
		switch (Style)
		{
		case EBRExitStyle::NoclipWall: return TEXT("mur noclip");
		case EBRExitStyle::NoclipFloor: return TEXT("sol noclip");
		case EBRExitStyle::Door: return TEXT("porte");
		case EBRExitStyle::HotelDoor: return TEXT("porte d'hotel");
		case EBRExitStyle::Elevator: return TEXT("ascenseur");
		case EBRExitStyle::Ladder: return TEXT("echelle");
		case EBRExitStyle::Barn: return TEXT("grange");
		case EBRExitStyle::HouseDoor: return TEXT("porte de maison");
		case EBRExitStyle::BuildingDoor: return TEXT("porte d'immeuble");
		case EBRExitStyle::ServiceDoor: return TEXT("porte de service");
		}
		return TEXT("?");
	}

	/** Relecture v4.16 : un plafonnier de la grille du chunk tombe sous sa trappe predite (le chunk ne le pose pas) : premiere
	 *  cellule du chunk, ordre fixe */
	bool LampUnderHatch(const ABRWorld* W, const FIntPoint& Chunk, FIntPoint& OutCell)
	{
		const int32 N = W->Def().ChunkCells;
		for (int32 LY = 0; LY < N; ++LY)
		{
			for (int32 LX = 0; LX < N; ++LX)
			{
				const FIntPoint Cell(Chunk.X * N + LX, Chunk.Y * N + LY);
				if (W->GetLightCell(Cell.X, Cell.Y).bUnderHatch)
				{
					OutCell = Cell;
					return true;
				}
			}
		}
		return false;
	}

	/** Relecture v4.16 : bilan des chunks a trappe verifies (etape 1 bis) ; Done : chunks deja verifies (une fois chacun) */
	struct FHatchTally
	{
		TSet<FIntPoint> Done;
		int32 Checked = 0;
		int32 WithLadder = 0;
		int32 Placed = 0;
		int32 PlacedBuilt = 0;
		int32 Under = 0;
		int32 UnderBuilt = 0;
		float Removed = 0.f;
		FString FirstBad;
	};

	/** Une echelle construite a cette trappe : pied dans le rectangle du conduit, a 10 cm pres */
	bool LadderAtHatch(const ABRExit& Exit, const BRLight::FHatch& H)
	{
		const FVector At = Exit.GetActorLocation();
		return Exit.Style == EBRExitStyle::Ladder && At.X > H.MinX - 10.0 && At.X < H.MaxX + 10.0 && At.Y > H.MinY - 10.0 && At.Y < H.MaxY + 10.0;
	}

	bool LadderAtHatch(UWorld* InWorld, const BRLight::FHatch& H)
	{
		for (TActorIterator<ABRExit> It(InWorld); It; ++It)
		{
			if (LadderAtHatch(**It, H))
			{
				return true;
			}
		}
		return false;
	}

	/** Relecture v4.16 : un chunk a trappe construit et en place (LiveChunk) : une echelle construite a la trappe predite (pied
	 *  dans le rectangle du conduit, a 10 cm pres) ; plafonniers de la grille poses (lumiere construite) et sous la trappe (aucune
	 *  lumiere). Premier ecart d'echelle : les sorties posees par ce chunk (style, pied, lacet), a cote du rectangle predit */
	void CheckHatchChunk(UWorld* InWorld, const ABRWorld* W, const ABRChunk& Chunk, const BRLight::FHatch& H, FHatchTally& T)
	{
		const FIntPoint Key = Chunk.Coord;
		T.Done.Add(Key);
		++T.Checked;
		bool bLadder = false;
		FString Exits;
		for (TActorIterator<ABRExit> It(InWorld); It; ++It)
		{
			const FVector At = It->GetActorLocation();
			bLadder = bLadder || LadderAtHatch(**It, H);
			if (It->GetOwner() == &Chunk)
			{
				Exits += FString::Printf(TEXT("%s%s (%.0f, %.0f, %.0f) lacet %.0f"), Exits.IsEmpty() ? TEXT("") : TEXT(", "), ExitStyleName(It->Style), At.X, At.Y, At.Z,
					It->GetActorRotation().Yaw);
			}
		}
		T.WithLadder += bLadder ? 1 : 0;
		if (!bLadder && T.FirstBad.IsEmpty())
		{
			T.FirstBad = FString::Printf(TEXT("aucune echelle construite a la trappe du chunk (%d, %d) (trappe predite %.0f..%.0f x %.0f..%.0f) ; sorties posees par ce chunk : %s"),
				Key.X, Key.Y, H.MinX, H.MaxX, H.MinY, H.MaxY, Exits.IsEmpty() ? TEXT("aucune") : *Exits);
		}
		const int32 N = W->Def().ChunkCells;
		TArray<FVector> Built;
		BuiltLights(InWorld, &Key, Built);
		for (int32 LY = 0; LY < N; ++LY)
		{
			for (int32 LX = 0; LX < N; ++LX)
			{
				const FIntPoint Cell(Key.X * N + LX, Key.Y * N + LY);
				const FBRLightCell L = W->GetLightCell(Cell.X, Cell.Y);
				const bool bBuilt = (L.bLamp || L.bUnderHatch) && HasBuiltLight(W, L, Built);
				if (L.bUnderHatch)
				{
					++T.Under;
					T.UnderBuilt += bBuilt ? 1 : 0;
					// Apport que la regle comptait sans que la lampe existe, a 1,65 m sous la trappe
					T.Removed = FMath::Max(T.Removed, LampWeight(W, L, FVector((H.MinX + H.MaxX) * 0.5, (H.MinY + H.MaxY) * 0.5, 165.0)));
					if (bBuilt && T.FirstBad.IsEmpty())
					{
						T.FirstBad = FString::Printf(TEXT("cellule (%d, %d) : plafonnier sous la trappe pourtant construit"), Cell.X, Cell.Y);
					}
				}
				else if (L.bLamp)
				{
					++T.Placed;
					T.PlacedBuilt += bBuilt ? 1 : 0;
					if (!bBuilt && T.FirstBad.IsEmpty())
					{
						T.FirstBad = FString::Printf(TEXT("cellule (%d, %d) : plafonnier de jeu sans lumiere construite"), Cell.X, Cell.Y);
					}
				}
			}
		}
	}

	/** Relecture v4.16 : chunks a trappe construits et en place, pas encore verifies */
	void CheckLiveHatches(UWorld* InWorld, const ABRWorld* W, const TArray<TPair<FIntPoint, BRLight::FHatch>>& List, FHatchTally& T)
	{
		TMap<FIntPoint, ABRChunk*> Live;
		LiveChunks(InWorld, Live);
		for (const TPair<FIntPoint, BRLight::FHatch>& Pair : List)
		{
			ABRChunk* const* Found = Live.Find(Pair.Key);
			if (Found && *Found && !T.Done.Contains(Pair.Key))
			{
				CheckHatchChunk(InWorld, W, **Found, Pair.Value, T);
			}
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
	/** Relecture v4.16 : coin sombre (regle d'avant au-dessus du seuil de dissipation, v4.16 en dessous) et Smiler temoin dans
	 *  la lumiere ; Smilers poses (bSpawned : ils ont existe) */
	bool bCornerScene = false;
	FVector CornerP = FVector::ZeroVector;
	FVector ControlP = FVector::ZeroVector;
	TWeakObjectPtr<ABREntity> CornerSmiler;
	TWeakObjectPtr<ABREntity> ControlSmiler;
	bool bCornerSpawned = false;
	bool bControlSpawned = false;
	/** Relecture v4.16 : trappes predites (chunk, rectangle) du niveau en cours de l'etape 1 bis ; trappe visee (indice ; avec un
	 *  plafonnier de la grille dessous ou non, cellule de ce plafonnier, anneau de chunks fouille) ; bilan des chunks verifies */
	TArray<TPair<FIntPoint, BRLight::FHatch>> Hatches;
	int32 HatchTarget = 0;
	bool bHatchTargetUnder = false;
	FIntPoint HatchUnderCell = FIntPoint::ZeroValue;
	int32 HatchSearched = 0;
	FHatchTally HatchTally;
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
		R.Title = TEXT("v4.16 : lumiere de gameplay (murs, passages, trappes, coupure, profils, sources de mission, cout)");
		R.Scene = TEXT("v4.16");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		Note(FString::Printf(TEXT("ray tracing materiel %s ; seuils : apparition sous %.2f, dissipation du Smiler au-dessus de %.2f"),
			ABRPlayerController::IsHardwareRayTracingAvailable() ? TEXT("disponible") : TEXT("indisponible"), BRLight::NeedsDark, BRLight::SmilerVanish));
		return true;
	});

	// ------------------------------------------------------------------------------------------------ 1. grille par niveau
	for (const FGridRun& Run : GridRuns())
	{
		const int32 Level = Run.Level;
		const uint32 UserSeed = Run.UserSeed;
		if (!BRLevels::Exists(Level))
		{
			// v4.16 (relecture) : niveau absent de ce canal : dit, pas saute en silence
			Add(FString::Printf(TEXT("v4.16 lumiere : Niveau %d indisponible"), Level), 0.f, [this, Level, UserSeed]()
			{
				Skip(FString::Printf(TEXT("Niveau %d (-BRSeed=%u) indisponible dans ce canal : grille de lumiere non verifiee"), Level, UserSeed));
				return true;
			});
			continue;
		}
		const uint32 Expected = Run.Expected;
		const bool bBench = Run.bBench;
		AddSeedLoad(Level, UserSeed, FString::Printf(TEXT("v4.16 lumiere : Niveau %d, -BRSeed=%u"), Level, UserSeed));
		Add(FString::Printf(TEXT("v4.16 lumiere : grille du Niveau %d (graine %u)"), Level, UserSeed), 0.f, [this, Level, UserSeed, Expected, bBench]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C || W->GetLevelNumber() != Level)
			{
				return false;
			}
			const FIntPoint Here = W->WorldToCell(C->GetActorLocation());
			// Chunks autour du point de depart construits et prets (la trace physique et les lumieres construites en ont besoin) ;
			// relecture v4.16 : en place (pas en demontage), comme les lumieres construites comparees (BuiltLights)
			TSet<FIntPoint> Ready;
			for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
			{
				if (LiveChunk(**It))
				{
					Ready.Add(It->Coord);
				}
			}
			int32 Missing = 0;
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					const FIntPoint Ch = W->CellToChunk(Here) + FIntPoint(DX, DY);
					Missing += (W->IsChunkInBounds(Ch) && !Ready.Contains(Ch)) ? 1 : 0;
				}
			}
			if (Missing > 0 && StepTime < 20.f)
			{
				return false;
			}
			const FBRLevelDef& D = W->Def();
			// a. Empreinte (meme portage que le banc ; ABRWorld l'ecrit aussi au journal, chez l'hote et chez chaque client)
			const uint32 Got = W->GetLightGridFingerprint(PrintHalf);
			if (bBench)
			{
				Note(FString::Printf(TEXT("Niveau %d, -BRSeed=%u (graine du niveau %u) : empreinte de la grille de lumiere %08x, banc %08x"), Level, UserSeed, W->GetSeed(), Got,
					Expected), Got != Expected);
			}
			else
			{
				Note(FString::Printf(TEXT("Niveau %d, -BRSeed=%u (graine du niveau %u) : empreinte de la grille de lumiere %08x"), Level, UserSeed, W->GetSeed(), Got));
				Skip(FString::Printf(TEXT("Niveau %d : empreinte non comparee au banc (le banc ne porte pas les rues des Niveaux 9 et 11)"), Level));
			}
			// b. Chaque plafonnier de jeu = une lumiere construite ; relecture v4.16 : aucun plafonnier de jeu sous une trappe
			// d'echelle (le chunk ne l'y pose pas, la regle ne le compte plus) ; tout ecart est un probleme
			TArray<FVector> Built;
			BuiltLights(GetWorld(), nullptr, Built);
			int32 Lamps = 0;
			int32 Matched = 0;
			int32 Off = 0;
			int32 Hatch = 0;
			int32 HatchBuilt = 0;
			FString FirstOff;
			const int32 Span = 10;
			for (int32 DX = -Span; DX <= Span; ++DX)
			{
				for (int32 DY = -Span; DY <= Span; ++DY)
				{
					const FIntPoint Cell = Here + FIntPoint(DX, DY);
					if (!Ready.Contains(W->CellToChunk(Cell)))
					{
						continue;
					}
					const FBRLightCell L = W->GetLightCell(Cell.X, Cell.Y);
					if (L.bUnderHatch)
					{
						++Hatch;
						HatchBuilt += HasBuiltLight(W, L, Built) ? 1 : 0;
						continue;
					}
					if (!L.bLamp)
					{
						continue;
					}
					++Lamps;
					const bool bFound = HasBuiltLight(W, L, Built);
					Matched += bFound ? 1 : 0;
					if (!bFound && ++Off == 1)
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
				Note(FString::Printf(TEXT("Niveau %d : %d plafonnier(s) de jeu dans les chunks construits, %d a la place d'une lumiere construite (%d lumiere(s) dans les chunks) ; sous une trappe d'echelle : %d, dont %d avec une lumiere construite (attendu 0)%s%s"),
					Level, Lamps, Matched, Built.Num(), Hatch, HatchBuilt, Off > 0 ? TEXT(" ; premier ecart : ") : TEXT(""), *FirstOff), Off > 0 || HatchBuilt > 0);
			}
			// c. Accord de la grille avec une trace physique (emetteur -> point), d. positions dont la lumiere change
			int32 Pairs = 0;
			int32 Agree = 0;
			int32 GridHides = 0;
			int32 ArchHides = 0;
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
			// Relecture v4.16 : cotes du niveau, et les memes avec des passages ouverts jusqu'au plafond (sans arche ni linteau) : une
			// paire que la grille seule cache et que ces cotes voient est cachee par l'arche ; dans les Poolrooms, ses ecoincons sont
			// sans collision (ABRChunk::AddDoorway : SM_ArchSpandrel) mais font de l'ombre : ecart attendu, compte a part
			const BRLight::Occlusion::FGeom Geom = W->GetLightGeom();
			BRLight::Occlusion::FGeom OpenTop = Geom;
			OpenTop.bArch = false;
			OpenTop.DoorTop = BRLight::Occlusion::Huge;
			for (int32 DX = -8; DX <= 8; ++DX)
			{
				for (int32 DY = -8; DY <= 8; ++DY)
				{
					const FIntPoint Cell = Here + FIntPoint(DX, DY);
					if (!UsableCell(W, Cell) || !Ready.Contains(W->CellToChunk(Cell)))
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
							if (!L.bLamp || !Ready.Contains(W->CellToChunk(LampCell)) || LampWeight(W, L, Player) <= 0.f)
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
								PhysHides += (bGrid && !bPhys) ? 1 : 0;
								if (!bGrid && bPhys)
								{
									if (Geom.bArch && W->IsLampVisibleFrom(LampCell, P, &OpenTop))
									{
										++ArchHides;
									}
									else
									{
										++GridHides;
									}
								}
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
				const float GridPct = 100.f * GridHides / Pairs;
				// Relecture v4.16 : la grille seule (rule plus sombre que le decor construit : jambage, linteau, sommet mal places) ne
				// doit pas depasser GridHideBudget des paires, dans chaque niveau ; la physique seule (objets poses, rochers, trottoirs
				// du Level 37 : la grille ne les connait pas) est notee ; un accord global sous 80 % reste un probleme
				Note(FString::Printf(TEXT("Niveau %d : regle de la grille et trace physique d'accord sur %d / %d paires (%.1f %%) ; la grille seule cache : %d (%.1f %%, au plus %.0f %%)%s ; la physique seule cache : %d%s%s"),
					Level, Agree, Pairs, Pct, GridHides, GridPct, GridHideBudget * 100.f,
					Geom.bArch ? *FString::Printf(TEXT(", plus %d par les ecoincons d'arche sans collision (attendu)"), ArchHides) : TEXT(""), PhysHides,
					Top.IsEmpty() ? TEXT("") : TEXT(" ; ecarts : "), *Top), Pct < 80.f || GridHides > GridHideBudget * Pairs);
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

	// ------------------------------------------------------------------------------------------------ 1 bis. trappes des echelles
	// Relecture v4.16 : partie classique imposee (le parcours lineaire ne tire aucune sortie des chunks a ces niveaux) ; chaque
	// trappe predite par le monde (ABRWorld::GetLightHatch, memes tirages qu'ABRChunk::PlanExits) doit etre celle d'une echelle
	// construite, et ses plafonniers ne sont ni poses par le chunk ni comptes par la regle
	Add(TEXT("v4.16 lumiere : partie classique imposee (trappes des echelles)"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		W->SetLinearTestOverride(0);
		return true;
	});
	for (const FHatchRun& Run : HatchRuns)
	{
		const int32 Level = Run.Level;
		const uint32 LevelSeed = Run.LevelSeed;
		if (!BRLevels::Exists(Level))
		{
			Add(FString::Printf(TEXT("v4.16 lumiere : trappes du Niveau %d indisponible"), Level), 0.f, [this, Level]()
			{
				Skip(FString::Printf(TEXT("Niveau %d indisponible dans ce canal : trappes des echelles non verifiees"), Level));
				return true;
			});
			continue;
		}
		AddLevelSeedLoad(Level, LevelSeed, FString::Printf(TEXT("v4.16 lumiere : Niveau %d (graine du niveau %u, partie classique)"), Level, LevelSeed));
		Add(FString::Printf(TEXT("v4.16 lumiere : trappes du Niveau %d : recherche"), Level), 0.f, [this, Level, LevelSeed]()
		{
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			Hatches.Reset();
			HatchTarget = 0;
			bHatchTargetUnder = false;
			HatchUnderCell = FIntPoint::ZeroValue;
			HatchSearched = 0;
			HatchTally = FHatchTally();
			if (!W || !C)
			{
				return false;
			}
			if (W->GetLevelNumber() != Level || W->GetSeed() != LevelSeed)
			{
				Skip(FString::Printf(TEXT("Niveau %d (graine du niveau %u) non charge : trappes des echelles non verifiees"), Level, LevelSeed));
				return true;
			}
			if (W->IsLinearLevel())
			{
				Skip(FString::Printf(TEXT("Niveau %d charge en parcours lineaire malgre la partie classique imposee : trappes des echelles non verifiees"), Level));
				return true;
			}
			// Chunks du niveau fini (ou 12 x 12 autour du depart) : trappes predites, ordre fixe
			const bool bBounded = W->Def().BoundsChunks > 0;
			const int32 B = bBounded ? W->Def().BoundsChunks : 6;
			for (int32 CX = -B; CX < B; ++CX)
			{
				for (int32 CY = -B; CY < B; ++CY)
				{
					BRLight::FHatch H;
					if (W->GetLightHatch(FIntPoint(CX, CY), H))
					{
						Hatches.Add(TPair<FIntPoint, BRLight::FHatch>(FIntPoint(CX, CY), H));
					}
				}
			}
			if (Hatches.Num() == 0)
			{
				Skip(FString::Printf(TEXT("Niveau %d (graine du niveau %u, partie classique) : aucune trappe d'echelle predite (regle de la trappe non exercee)"), Level, LevelSeed));
				return true;
			}
			const int32 Predicted = Hatches.Num();
			// Relecture v4.16 : trappe visee : la premiere (ordre fixe) dont un plafonnier de la grille tombe sous la trappe (le chunk
			// ne le pose pas, la regle ne le compte pas : "plafonnier non pose" exerce) ; niveau ouvert : sinon cherchee anneau par
			// anneau autour des 12 x 12 chunks, jusqu'a HatchSearchChunks du depart. Graine et grille seules : le meme choix a chaque
			// lancement. Aucune : la premiere trappe
			HatchTarget = -1;
			for (int32 I = 0; I < Hatches.Num() && HatchTarget < 0; ++I)
			{
				if (LampUnderHatch(W, Hatches[I].Key, HatchUnderCell))
				{
					HatchTarget = I;
				}
			}
			HatchSearched = B;
			for (int32 R = B + 1; !bBounded && R <= HatchSearchChunks && HatchTarget < 0; ++R)
			{
				HatchSearched = R;
				for (int32 CX = -R; CX < R && HatchTarget < 0; ++CX)
				{
					for (int32 CY = -R; CY < R && HatchTarget < 0; ++CY)
					{
						BRLight::FHatch H;
						const FIntPoint Chunk(CX, CY);
						if ((CX == -R || CX == R - 1 || CY == -R || CY == R - 1) && W->GetLightHatch(Chunk, H) && LampUnderHatch(W, Chunk, HatchUnderCell))
						{
							HatchTarget = Hatches.Add(TPair<FIntPoint, BRLight::FHatch>(Chunk, H));
						}
					}
				}
			}
			bHatchTargetUnder = HatchTarget >= 0;
			HatchTarget = FMath::Max(HatchTarget, 0);
			// Relecture v4.16 : chunks a trappe construits au depart (construction synchrone du chargement), verifies avant que le
			// joueur parte : ensuite ils sortent de la vue et se demontent (ABRChunk::StepTeardown : sorties detruites d'abord, le
			// chunk reste IsReady() ; avant, ils etaient encore comptes : "aucune echelle construite a la trappe du chunk (0, -2)")
			CheckLiveHatches(GetWorld(), W, Hatches, HatchTally);
			const int32 AtStart = HatchTally.Checked;
			// Le joueur va dans une cellule praticable voisine de la trappe visee (son chunk se construit)
			const FIntPoint TargetChunk = Hatches[HatchTarget].Key;
			const BRLight::FHatch& HT = Hatches[HatchTarget].Value;
			const FIntPoint HatchCell = W->WorldToCell(FVector((HT.MinX + HT.MaxX) * 0.5, (HT.MinY + HT.MaxY) * 0.5, 0.0));
			const bool bPlaced = ForRings(HatchCell, 3, [&](const FIntPoint& Cell)
			{
				if (Cell == HatchCell || !W->IsWalkable(Cell) || !W->IsCellInBounds(Cell.X, Cell.Y) || W->IsPoolCell(Cell.X, Cell.Y) || W->IsPitRoomCell(Cell.X, Cell.Y))
				{
					return false;
				}
				PlacePlayer(W, C, W->CellCenter(Cell));
				return true;
			});
			const BRLight::FHatch& H0 = Hatches[0].Value;
			const FString Where = bBounded ? FString(TEXT("dans l'enceinte")) : FString::Printf(TEXT("a moins de %d chunks du depart"), HatchSearched);
			const FString Target = bHatchTargetUnder
				? FString::Printf(TEXT("trappe visee au chunk (%d, %d) (%.0f..%.0f x %.0f..%.0f) : plafonnier de la grille de la cellule (%d, %d) dessous"), TargetChunk.X, TargetChunk.Y,
					HT.MinX, HT.MaxX, HT.MinY, HT.MaxY, HatchUnderCell.X, HatchUnderCell.Y)
				: FString::Printf(TEXT("aucun plafonnier de la grille sous une trappe predite %s : trappe visee : la premiere"), *Where);
			Note(FString::Printf(TEXT("Niveau %d (graine du niveau %u, partie classique) : %d trappe(s) d'echelle predite(s), premiere au chunk (%d, %d) (%.0f..%.0f x %.0f..%.0f) ; %d chunk(s) a trappe construits au depart, verifies avant le depart du joueur ; %s ; joueur %s"),
				Level, LevelSeed, Predicted, Hatches[0].Key.X, Hatches[0].Key.Y, H0.MinX, H0.MaxX, H0.MinY, H0.MaxY, AtStart, *Target,
				bPlaced ? TEXT("pose a cote") : TEXT("NON pose (aucune cellule libre)")));
			return true;
		});
		Add(FString::Printf(TEXT("v4.16 lumiere : trappes du Niveau %d : plafonniers"), Level), 0.5f, [this, Level]()
		{
			ABRWorld* W = GetBRWorld();
			if (!W || Hatches.Num() == 0 || W->GetLevelNumber() != Level)
			{
				return true;
			}
			// Relecture v4.16 : chunks construits ET en place (LiveChunk) ; ceux du depart, verifies a l'etape d'avant, se demontent
			const FIntPoint TargetChunk = Hatches[HatchTarget].Key;
			TMap<FIntPoint, ABRChunk*> Live;
			LiveChunks(GetWorld(), Live);
			if (!Live.Contains(TargetChunk) && StepTime < 30.f)
			{
				return false;
			}
			CheckLiveHatches(GetWorld(), W, Hatches, HatchTally);
			const FHatchTally& T = HatchTally;
			if (T.Checked == 0)
			{
				Skip(FString::Printf(TEXT("Niveau %d : chunk de la trappe pas construit en 30 s (trappes des echelles non verifiees)"), Level));
				return true;
			}
			// Diagnostic : chunks a trappe encore IsReady() mais en demontage (le joueur est parti) : pas comptes ; ceux qui n'ont deja
			// plus leur echelle (ce que l'etape comptait avant comme "aucune echelle construite")
			int32 Torn = 0;
			int32 TornNoLadder = 0;
			for (TActorIterator<ABRChunk> It(GetWorld()); It; ++It)
			{
				if (!It->IsReady() || !It->IsTearingDown())
				{
					continue;
				}
				for (const TPair<FIntPoint, BRLight::FHatch>& Pair : Hatches)
				{
					if (Pair.Key == It->Coord)
					{
						++Torn;
						TornNoLadder += LadderAtHatch(GetWorld(), Pair.Value) ? 0 : 1;
					}
				}
			}
			const FString TornText = Torn > 0 ? FString::Printf(TEXT(" ; %d chunk(s) a trappe en demontage, pas comptes (dont %d deja sans echelle)"), Torn, TornNoLadder) : FString();
			const bool bBad = T.WithLadder != T.Checked || T.UnderBuilt > 0 || T.PlacedBuilt != T.Placed;
			Note(FString::Printf(TEXT("Niveau %d, partie classique : %d chunk(s) a trappe construits : echelle a la trappe predite %d / %d ; plafonniers poses %d (lumiere construite %d) ; sous la trappe %d (lumiere construite %d, attendu 0 ; la regle ne les compte plus : jusqu'a %.3f retire sous la trappe)%s%s%s"),
				Level, T.Checked, T.WithLadder, T.Checked, T.Placed, T.PlacedBuilt, T.Under, T.UnderBuilt, T.Removed, *TornText, T.FirstBad.IsEmpty() ? TEXT("") : TEXT(" ; premier ecart : "),
				*T.FirstBad), bBad);
			if (!T.Done.Contains(TargetChunk))
			{
				Skip(FString::Printf(TEXT("Niveau %d : chunk (%d, %d) de la trappe visee pas construit en 30 s (trappe non verifiee)"), Level, TargetChunk.X, TargetChunk.Y));
			}
			if (T.Under == 0)
			{
				const FString Where = W->Def().BoundsChunks > 0 ? FString(TEXT("dans l'enceinte")) : FString::Printf(TEXT("a moins de %d chunks du depart"), HatchSearched);
				Skip(bHatchTargetUnder
					? FString::Printf(TEXT("Niveau %d : plafonnier de la grille de la cellule (%d, %d) sous la trappe visee, chunk pas verifie (plafonnier non pose non exerce)"), Level,
						HatchUnderCell.X, HatchUnderCell.Y)
					: FString::Printf(TEXT("Niveau %d : aucun plafonnier de la grille sous une trappe predite %s (plafonnier non pose non exerce)"), Level, *Where));
			}
			return true;
		});
		LimitLastStep(40.f);
	}
	Add(TEXT("v4.16 lumiere : parcours normal retabli"), 0.f, [this]()
	{
		if (ABRWorld* W = GetBRWorld())
		{
			W->SetLinearTestOverride(-1);
		}
		return true;
	});

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

		// c. Relecture v4.16 : coin sombre pour un Smiler : la regle d'avant (murs ignores) l'y dissipait, la v4.16 non (marge de
		// 0,05 de part et d'autre du seuil) ; temoin : un point ou la v4.16 depasse le seuil de 0,1 (un Smiler doit s'y dissiper,
		// sinon la lumiere ne decide rien en ce moment et l'etape n'est pas verifiee)
		bCornerScene = false;
		bCornerSpawned = false;
		bControlSpawned = false;
		CornerSmiler.Reset();
		ControlSmiler.Reset();
		FIntPoint CornerCell = Start;
		FIntPoint CornerDir(1, 0);
		if (PowerSteady(W))
		{
			const bool bCorner = ForRings(Start, 8, [&](const FIntPoint& A)
			{
				if (!UsableCell(W, A))
				{
					return false;
				}
				for (const FIntPoint& Dir : Dirs)
				{
					if (EdgeToward(W, A, Dir) != static_cast<uint8>(EBREdge::Wall))
					{
						continue;
					}
					for (const float Side : { 0.f, -80.f, 80.f })
					{
						const FVector P = CellPoint(W, A, Dir, Face - 45.f, Side, 165.f);
						const FBRLightSample Sm = W->SampleLight(P, nullptr, false);
						if (Sm.CeilingAll * Sm.Power > BRLight::SmilerVanish + 0.05f && Sm.Value < BRLight::SmilerVanish - 0.05f)
						{
							CornerP = P;
							CornerCell = A;
							CornerDir = Dir;
							return true;
						}
					}
				}
				return false;
			});
			const bool bControl = ForRings(Start, 6, [&](const FIntPoint& Cell)
			{
				if (!UsableCell(W, Cell) || (bCorner && FMath::Abs(Cell.X - CornerCell.X) + FMath::Abs(Cell.Y - CornerCell.Y) <= 1))
				{
					return false;
				}
				const FVector P = CellPoint(W, Cell, FIntPoint(1, 0), 0.f, 0.f, 165.f);
				if (W->SampleLight(P, nullptr, false).Value > BRLight::SmilerVanish + 0.1f)
				{
					ControlP = P;
					return true;
				}
				return false;
			});
			bCornerScene = bCorner && bControl;
			if (bCornerScene)
			{
				if (ABREntity* E = SpawnFrozen(W, EBREntityKind::Smiler, CornerP, 0.05f))
				{
					E->SetActorLocation(CornerP);
					CornerSmiler = E;
					bCornerSpawned = true;
					TestSpawned.Add(E);
				}
				if (ABREntity* E = SpawnFrozen(W, EBREntityKind::Smiler, ControlP, 0.05f))
				{
					E->SetActorLocation(ControlP);
					ControlSmiler = E;
					bControlSpawned = true;
					TestSpawned.Add(E);
				}
				// Le joueur au fond de la cellule du coin, tourne vers le Smiler (capture v416_mur_smiler)
				PlacePlayer(W, C, CellPoint(W, CornerCell, CornerDir, -(Face - 50.f), 0.f, 0.f));
				if (AController* Ctrl = C->GetController())
				{
					Ctrl->SetControlRotation((CornerP - C->GetEyeLocation()).Rotation());
				}
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
		if (!bCornerScene)
		{
			Skip(TEXT("Smiler du coin sombre : aucun coin ou la regle d'avant dissipait un Smiler et pas la v4.16 a moins de 8 cellules, aucun point temoin eclaire, ou coupure en cours (defaut d'avant non reproduit)"));
			return true;
		}
		Shot(TEXT("v416_mur_smiler"));
		const ABREntity* E = CornerSmiler.Get();
		const ABREntity* T = ControlSmiler.Get();
		if (!bCornerSpawned || !bControlSpawned)
		{
			Skip(FString::Printf(TEXT("Smiler du coin sombre : Smiler non pose (coin %s, temoin %s)"), bCornerSpawned ? TEXT("pose") : TEXT("NON pose"),
				bControlSpawned ? TEXT("pose") : TEXT("NON pose")));
		}
		else
		{
			// Un Smiler pose puis retire (dissipe, 0,6 s de son temps ralenti) compte comme dissipe
			const bool bControlVanish = !T || T->IsVanishing();
			const bool bCornerVanish = !E || E->IsVanishing();
			const FVector At = E ? E->GetActorLocation() : CornerP;
			const FBRLightSample Sm = W->SampleLight(At, nullptr, false);
			const float Old = Sm.CeilingAll * Sm.Power;
			const FString Values = FString::Printf(TEXT("coin %s : lumiere de jeu %.3f, regle d'avant %.3f (dissipation au-dessus de %.2f) ; temoin %s : %.3f"), *At.ToCompactString(),
				Sm.Value, Old, BRLight::SmilerVanish, *ControlP.ToCompactString(), W->SampleLight(T ? T->GetActorLocation() : ControlP, nullptr, false).Value);
			if (!bControlVanish)
			{
				Skip(FString::Printf(TEXT("Smiler du coin sombre : le Smiler temoin, dans la lumiere, ne se dissipe pas (%s) : la lumiere ne decide rien en ce moment ; %s"),
					T && T->IsHunting() ? TEXT("il poursuit le joueur") : TEXT("pas encore decide"), *Values));
			}
			else if (E && E->IsHunting() && !bCornerVanish)
			{
				Skip(FString::Printf(TEXT("Smiler du coin sombre : il poursuit le joueur (la lumiere ne le dissipe pas en poursuite) : non verifie ; %s"), *Values));
			}
			else if (!(Old > BRLight::SmilerVanish && Sm.Value <= BRLight::SmilerVanish))
			{
				Skip(FString::Printf(TEXT("Smiler du coin sombre : a sa position du moment, la regle d'avant ne le dissipait plus ou la v4.16 le dissiperait : non verifie ; %s"), *Values));
			}
			else
			{
				Note(FString::Printf(TEXT("Smiler du coin sombre : %s : il s'y dissipait avant la v4.16 ; le Smiler %s ; temoin dans la lumiere : se dissipe (capture v416_mur_smiler)"), *Values,
					bCornerVanish ? TEXT("SE DISSIPE") : TEXT("reste dans le coin sombre")), bCornerVanish);
			}
		}
		for (TWeakObjectPtr<ABREntity>* Ptr : { &CornerSmiler, &ControlSmiler })
		{
			if (ABREntity* Live = Ptr->Get())
			{
				Live->Destroy();
			}
			Ptr->Reset();
		}
		return true;
	});
	// Relecture v4.16 : plafonnier derriere une porte de mission. Les seules portes posees sur une ouverture de la grille sont
	// celles de l'aile du Level 6, qui n'a pas de plafonniers : le crochet de test ferme l'arete ouverte de la scene sans mur
	// (meme ensemble d'aretes, meme relecture et meme invalidation qu'une porte de mission qui change), puis la rouvre
	Add(TEXT("v4.16 lumiere : arete fermee comme une porte de mission"), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (!bOpenScene)
		{
			Skip(TEXT("Level 0 : pas d'arete ouverte vers un plafonnier stable : arete fermee par le crochet de test non verifiee"));
			return true;
		}
		if (!PowerSteady(W))
		{
			if (StepTime > 20.f)
			{
				Skip(TEXT("arete fermee comme une porte de mission : coupure de courant en cours pendant 20 s (valeurs non comparees)"));
				return true;
			}
			return false;
		}
		const float V0 = W->LightLevelAt(OpenP);
		const bool bSeen0 = W->IsLampVisibleFrom(OpenB, OpenP);
		const int32 Inv0 = W->GetLightStats(true).Invalidations;
		W->SetLightTestClosedEdge(OpenA, OpenB, true);
		const float V1 = W->LightLevelAt(OpenP);
		const bool bSeen1 = W->IsLampVisibleFrom(OpenB, OpenP);
		const int32 Inv1 = W->GetLightStats(true).Invalidations;
		W->SetLightTestClosedEdge(OpenA, OpenB, false);
		const float V2 = W->LightLevelAt(OpenP);
		const bool bSeen2 = W->IsLampVisibleFrom(OpenB, OpenP);
		const bool bOk = bSeen0 && !bSeen1 && bSeen2 && V1 < V0 && V2 == V0 && Inv1 > Inv0;
		Note(FString::Printf(TEXT("arete (%d, %d) - (%d, %d) fermee comme une porte de mission (crochet de test) : plafonnier (%d, %d) ouvert : %s (lumiere de jeu %.3f) ; ferme : %s (%.3f) ; rouvert : %s (%.3f) ; caches vides a la fermeture : %s"),
			OpenA.X, OpenA.Y, OpenB.X, OpenB.Y, OpenB.X, OpenB.Y, bSeen0 ? TEXT("compte") : TEXT("NE COMPTE PAS"), V0, bSeen1 ? TEXT("COMPTE ENCORE") : TEXT("ne compte plus"), V1,
			bSeen2 ? TEXT("compte") : TEXT("NE COMPTE PAS"), V2, Inv1 > Inv0 ? TEXT("oui") : TEXT("NON")), !bOk);
		Skip(TEXT("plafonnier derriere une porte de mission reelle (ABRWorld::GetClosedMissionEdges) : aucune porte de mission n'est posee sur une ouverture de la grille d'un niveau a plafonniers (aile du Level 6 seulement) ; chemin exerce par le crochet de test"));
		return true;
	});
	LimitLastStep(25.f);

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
		// Relecture v4.16 : reproduction du defaut d'avant : la trace finit au centre de la cible, seul l'emetteur etait ignore :
		// la capsule du joueur lui-meme l'arretait (une source de mission ne comptait jamais pour un joueur ou une creature)
		if (Sm.MissionOldFilter == 0.f && BRTestV416Light::HitsActor(Sm.MissionOldHitActors, C))
		{
			Note(FString::Printf(TEXT("source de mission, rien entre : filtre d'avant la v4.16 : 0.000, trace arretee par la capsule du joueur lui-meme (%s) : defaut d'avant reproduit (elle ne comptait jamais pour un joueur ou une creature)"),
				*Sm.MissionOldHits));
		}
		else
		{
			Skip(FString::Printf(TEXT("source de mission, rien entre : le filtre d'avant renvoie %.3f, obstacle %s : defaut d'avant (capsule de la cible) non reproduit"), Sm.MissionOldFilter,
				Sm.MissionOldHits.IsEmpty() ? TEXT("aucun") : *Sm.MissionOldHits));
		}
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
			// Relecture v4.16 : coequipier ou creature : la trace qui garde les capsules (sans la cible) doit toucher cet obstacle,
			// sinon il n'est pas sur le rayon et rien n'est demontre (le filtre d'avant renvoie 0 de toute facon : la capsule de la
			// cible l'arrete)
			const AActor* Obstacle = Kind == 0 ? static_cast<const AActor*>(Mate.Get()) : (Kind == 1 ? static_cast<const AActor*>(Creature.Get()) : nullptr);
			const bool bOnRay = Obstacle && BRTestV416Light::HitsActor(Sm.MissionPawnHitActors, Obstacle);
			const FString Detail = FString::Printf(TEXT("apport %.3f (sans obstacle %.3f ; %s) ; filtre d'avant la v4.16 : %.3f ; capsules comprises, sans la cible : %.3f ; obstacles touches : %s / avant : %s / capsules comprises : %s ; LightLevelAt %.3f, recalcul %.3f"),
				Sm.Mission, Clear, *Expect, Sm.MissionOldFilter, Sm.MissionPawnFilter, Sm.MissionHits.IsEmpty() ? TEXT("aucun") : *Sm.MissionHits,
				Sm.MissionOldHits.IsEmpty() ? TEXT("aucun") : *Sm.MissionOldHits, Sm.MissionPawnHits.IsEmpty() ? TEXT("aucun") : *Sm.MissionPawnHits, Cached, Sm.Value);
			if (Kind <= 1 && !bOnRay)
			{
				Skip(FString::Printf(TEXT("source de mission, %s : l'obstacle n'est pas sur le rayon (la trace qui garde les capsules ne le touche pas) : non verifie ; %s"), *Name, *Detail));
				return true;
			}
			Note(FString::Printf(TEXT("source de mission, %s : %s%s"), *Name, *Detail,
				Kind <= 1 ? TEXT(" (defaut reproduit : la capsule sur le rayon coupait la lumiere ; v4.16 : ignoree)") : TEXT("")), bBad);
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
			// Relecture v4.16 : part des plafonniers repris du cache (noeud de 10 cm : une creature qui bouge le reprend tant qu'elle
			// reste pres du meme noeud)
			const double HitRate = (T.CacheHits + T.Computes) > 0 ? 100.0 * T.CacheHits / (T.CacheHits + T.Computes) : 0.0;
			Note(FString::Printf(TEXT("cout (%s, %d creature(s), %.0f images en %.1f s) : par image %.1f appel(s), %.1f calcul(s), %.1f repris du cache (%.1f %% des plafonniers repris), %.1f plafonnier(s), %.0f obstacle(s), %.2f trace(s) (%.2f par creature), %.1f trace(s) reprise(s) ; %d chunk(s) de grille mis en cache"),
				Pass == 0 ? TEXT("immobiles") : TEXT("libres"), Alive, Frames, FPlatformTime::Seconds() - CostT0, T.Queries / Frames, T.Computes / Frames, T.CacheHits / Frames, HitRate,
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
			W->SetLinearTestOverride(-1); // relecture v4.16 : parcours normal (deja retabli apres les trappes)
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
