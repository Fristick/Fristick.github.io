// v4.16 (echelles) : -BRAutoTest -BRAutoTestV416Ladder [-BRAutoTestStrict] : fin de la validation des echelles apres les
// correctifs 5 de la v4.15 (places des echelles des chunks du Niveau 37, BRMech::Grid::StandFlat ; deplacement de secours de la
// montee pres du pied, ABRCharacter::UpdateClimb) et la regle v4.16 (BRGather::ClimbAfterHit : essai de la marche avant la reprise
// verticale, ClimbReleaseOnDescent : lacher en descente pres du pied). Partie classique imposee (les sorties secondaires des
// chunks n'existent pas dans le parcours lineaire), graines DU NIVEAU documentees, sans seconde transformation de graine
// (AddLevelSeedLoad) :
//   - Niveau 37, graine du niveau 2303284074 : echelle au fond du canal avec un sol plat devant elle (chunk (2, -1), a la place
//     de la regle du jeu (11750, -1560, 0), l'ancienne place (11750, -2060, 0) vide) : caisses de 20 et 50 cm (ci-dessous) ;
//   - Niveau 37, graine du niveau 115083815 : les echelles vers le Niveau 2 tirees par les chunks -6..6 (regle du jeu,
//     ABRWorld::IsChunkExitDrawn et PickChunkWallFace) ; celles posees sur un trottoir avec une place dans le canal devant elles
//     (grille : sol 0 sous l'empreinte de la capsule, hors des murs de la cellule) ; le joueur y est conduit (vol du
//     developpeur, sans collision, le temps que le chunk se construise), l'echelle construite est jugee (point vise et capsule
//     du grimpeur libres) et la place du canal verifiee par les collisions. Avant (final4/v415fix) : seule l'echelle du chunk
//     (-1, -1) etait essayee, sans place dans le canal devant elle (canal de 60 cm entre deux trottoirs) : NON VERIFIE ;
//   - prise depuis le canal (visee et touche d'interaction), avancer tout de suite puis 0,5 s apres : montee au-dessus du
//     trottoir ; lacher : debout sur le trottoir ; puis montee de 2 m et descente jusqu'au lacher sur le trottoir ;
//   - cas limites du deplacement de secours, avec des boites de collision posees par le test (prise directe, StartClimb) :
//     mur haut entre le joueur et l'accroche (aucune hausse, aucune traversee), plafond bas au-dessus du canal (aucune hausse,
//     tete jamais dedans), caisse sous l'accroche en descente (lacher sur elle, jamais accroche au-dessus) ; au pied d'une echelle
//     au fond du canal (sol plat) : caisse de 20 cm franchie, caisse de 50 cm jamais (aucune hausse) ; a chaque image, la capsule
//     du grimpeur (reduite de 1,5 cm) n'entre dans aucune collision ;
//   - montee complete depuis le canal : une demande au sommet, arrivee au Niveau 2 ;
//   - UpdateClimb est commun : Niveau 8, graine du niveau 233637237, echelle BRExit_42 (tablier incline), et Level 6, graine du
//     niveau 2702236446, echelle de l'alcove de l'aile : montee de 2 m, descente jusqu'au lacher au pied (debout, jamais coince),
//     reprise et lacher en hauteur (Saut) : chute au sol, jamais en l'air sans echelle.
// Reseau (-BRNetTest, a la fin du plan reseau ; v4.16 relecture : toujours, plus seulement avec -BRAutoTestV416Ladder) : Niveau 37
// a la graine du niveau 115083815 (son chunk (-1, -1), construit au depart, porte une echelle vers le Niveau 2 sur un trottoir :
// banc et final4/v415fix ; a 2303284074, net_final4, aucune dans les chunks construits au depart) : echelles des chunks construits
// posees par le tirage et libres, meme tirage et meme echelle chez l'hote et le client, l'hote au sommet (une demande, attente
// stable, 1/2), le client loin puis a pied, rassemblement 2/2, depart et arrivee au Niveau 2 chez chacun. Une etape impossible
// (chunk absent, echelle introuvable, place prise) est notee "NON VERIFIE", jamais comptee comme reussie.
// v4.16 (echelles, relecture) : la place de la regle du jeu au chunk (2, -1) (graine du niveau 2303284074) est jugee par une etape
// a elle (avant : seulement si l'echelle retenue pour les caisses etait celle-la, sans ligne sinon) ; une echelle disparue en
// cours de cas est notee (avant : etape finie sans ligne) ; la prise depuis le canal (AddV416LadderPlan, AddV416LadderGoTo,
// AddV416LadderCanalGrab) sert aussi a -BRAutoTestV415Fix, ou l'echelle du chunk (-1, -1) n'a pas de place dans le canal.
// v4.16 (echelles, correctifs, v416m1) : la demi-hauteur du joueur est celle de sa capsule (CapsuleHalf416) : pendant le vol du
// developpeur (collisions de l'acteur coupees), GetSimpleCollisionHalfHeight rend 0. Avant : chaque place du canal etait jugee
// avec une boule de 36 cm 5 cm au-dessus du sol ("place libre NON" pour les 6 echelles essayees, 9 NON VERIFIE), et le cas du
// Level 6 visait des pieds trop hauts, atteignait le sommet de la montee et partait au Niveau 8 ("echelle disparue pendant la
// montee"). Un depart pendant le cas des Niveaux 8 et 6 est maintenant juge comme tel (une demande au sommet, arrivee), la suite
// du cas NON VERIFIEE ; marge sous le sommet de 25 cm (TopMargin, avant 60 cm : cas du Level 6 impossible, sommet a 196 cm).
#include "BRAutoTest.h"
#include "BRAutoTestNet.h"
#include "BRAutoTestWalk.h"
#include "Backrooms.h"
#include "BRAssets.h"
#include "BRCharacter.h"
#include "BRChunk.h"
#include "BREntity.h"
#include "BRGatherLogic.h"
#include "BRInteractables.h"
#include "BRLevels.h"
#include "BRLidarLogic.h"
#include "BRMechLogic.h"
#include "BRPlayerController.h"
#include "BRWorld.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "Misc/Crc.h"

namespace BRTestV416Ladder
{
	/** Graines du niveau documentees : 115083815 (final3/v412 et full), 2303284074 (net_final3). A 115083815, les echelles sur un
	 *  trottoir avec un canal devant elles (sonde Saved/TestRuns/v416/ladder/probe : chunks (2, 2), (5, 3), (-5, 5)...) ; a
	 *  2303284074, l'echelle au fond du canal du chunk (2, -1) (11750, -1560, 0) a un sol plat devant elle (cas des caisses) */
	constexpr uint32 Pool37Seed = 115083815u;
	constexpr uint32 Pool37FlatSeed = 2303284074u;
	/** A 2303284074, place de l'echelle du chunk (2, -1) selon la regle du jeu (banc test_departure, section 5) et ancienne place
	 *  (avant BRMech::Grid::StandFlat, net_final3) */
	constexpr double FlatBenchX = 11750.0;
	constexpr double FlatBenchY = -1560.0;
	constexpr double FlatOldY = -2060.0;
	constexpr int32 PoolTarget = 2;
	/** Chunks dont le tirage est rejoue (-PlanRange..PlanRange) */
	constexpr int32 PlanRange = 6;
	/** Echelles essayees au plus par liste (les plus proches du joueur) */
	constexpr int32 MaxTried = 5;
	/** Echelle du chunk (-1, -1) a la graine 115083815 (banc, final4/v415fix) : celle du test reseau */
	constexpr double NetLadderX = -1250.0;
	constexpr double NetLadderY = -1560.0;
	/** Niveau 8, echelle BRExit_42 (final2/v413, correctifs 4) */
	constexpr uint32 Level8Seed = 233637237u;
	constexpr double Level8X = 4000.0;
	constexpr double Level8Y = -5400.0;
	constexpr int32 Level8Target = 9;
	/** v4.16 : Level 6, graine 4136 (graine du niveau 2702236446 : final4/v413 et v415fix) : echelle de sortie vers le Niveau 8 dans
	 *  l'alcove de l'aile (BRLidar::FWing::LadderSlot, ABRWorld::PlaceMission) */
	constexpr uint32 Level6Seed = 2702236446u;
	constexpr int32 Level6Target = 8;
	/** Hausse admise sans reprise verticale (cm) : la montee d'une image avant le contact (banc : NoLift), bien moins qu'une
	 *  marche (35 cm) ; la regle des correctifs 5 levait de 37 cm (sol plat) a 105 cm (canal sous un trottoir) */
	constexpr float NoLift = 10.f;
	/** v4.16 (echelles, correctifs) : marge sous le sommet de la montee des cas qui ne doivent pas partir (cm, centre de la
	 *  capsule) : la demande de depart part a 1 cm du sommet (BRGather::FClimbWait) ; 25 cm couvrent la montee d'une image
	 *  jusqu'a 6 img/s (1,4 m/s). Avant : 60 cm, et l'echelle de l'alcove de l'aile du Level 6 (plafond a 290 cm, sommet de la
	 *  montee a 196 cm du pied, sans conduit) n'aurait laisse que 48 cm de montee : cas NON VERIFIE (moins de 80 cm) */
	constexpr float TopMargin = 25.f;
	/** Barrieres du test reseau (le client signale l'etape atteinte) ; empreinte de l'echelle du client : MarkHash + empreinte % 100000 */
	constexpr int32 MarkStart = 41610;
	constexpr int32 MarkNoLevel = 41611;
	constexpr int32 MarkNoPlace = 41613;
	constexpr int32 MarkHash = 7160000;

	const TCHAR* OuiNon(bool b)
	{
		return b ? TEXT("oui") : TEXT("NON");
	}

	FVector LadderFrontOf(const ABRExit* E)
	{
		return FRotator(0.f, E->GetActorRotation().Yaw, 0.f).Vector();
	}

	/** Le point que vise la touche d'interaction sur l'echelle (comme AddWalkExitSteps) */
	FVector LadderAimOf(const ABRExit* E)
	{
		return E->GetClimbAnchor() + FVector(0.f, 0.f, 60.f);
	}

	/** v4.16 (echelles, correctifs) : demi-hauteur de la capsule du joueur (composant, a l'echelle), quel que soit l'etat de ses
	 *  collisions. AActor::GetSimpleCollisionCylinder (donc GetSimpleCollisionHalfHeight) ne lit la capsule que si les collisions
	 *  de l'acteur sont actives (IsRootComponentCollisionRegistered) ; sinon le cylindre des composants qui ont des collisions :
	 *  aucun, 0 cm, pendant le vol du developpeur (ABRCharacter::SetDevFly : SetActorEnableCollision(false)). Avant : FreeStand,
	 *  appele pendant le vol (AddV416LadderGoTo), posait une capsule (36, -4 cm), une boule de 36 cm centree 5 cm au-dessus du sol,
	 *  toujours dans le sol : "place libre NON" pour chaque echelle du Niveau 37 (v416m1) ; le cas du Level 6 visait des pieds a
	 *  136 cm du pied (demi-hauteur 0) au lieu de 48, atteignait le sommet de la montee et partait au Niveau 8 */
	float CapsuleHalf416(const ABRCharacter* C)
	{
		const UCapsuleComponent* Cap = C ? C->GetCapsuleComponent() : nullptr;
		return Cap ? Cap->GetScaledCapsuleHalfHeight() : 88.f;
	}

	float FeetZOf(const ABRCharacter* C)
	{
		return static_cast<float>(C->GetActorLocation().Z) - CapsuleHalf416(C);
	}

	void LookAt416(ABRCharacter* C, const FVector& Target)
	{
		if (AController* Ctrl = C ? C->GetController() : nullptr)
		{
			Ctrl->SetControlRotation((Target - C->GetEyeLocation()).Rotation());
		}
	}

	/** Joueur pose (sans vitesse) en Stand (centre de la capsule), regard vers Look : mise en place, pas une marche */
	void PutAt416(ABRCharacter* C, const FVector& Stand, const FVector& Look)
	{
		C->SetActorLocation(Stand, false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* M = C->GetCharacterMovement())
		{
			M->StopMovementImmediately();
		}
		LookAt416(C, Look);
	}

	FString CompName(const UPrimitiveComponent* P)
	{
		return P ? FString::Printf(TEXT("%s (%s)"), *GetNameSafe(P->GetOwner()), *P->GetName()) : FString(TEXT("aucun"));
	}

	/** Echelle vers Target posee par un chunk, a moins de Tol cm (a l'horizontale) de Foot */
	ABRExit* LadderNear(UWorld* World, const FVector& Foot, int32 Target, double Tol)
	{
		ABRExit* Best = nullptr;
		double BestD = Tol;
		for (TActorIterator<ABRExit> It(World); It; ++It)
		{
			const double D = FVector::Dist2D(It->GetActorLocation(), Foot);
			if (It->Target == Target && It->IsClimbable() && Cast<ABRChunk>(It->GetOwner()) && D < BestD)
			{
				Best = *It;
				BestD = D;
			}
		}
		return Best;
	}

	/** Echelle vers Target la plus proche de Spot (a l'horizontale, a moins de Tol cm), affichee : les sorties de la mission
	 *  (Level 6) restent cachees tant que leur chunk n'est pas construit ; posee par un chunk ou par la mission */
	ABRExit* ShownLadderNear(UWorld* World, const FVector& Spot, int32 Target, double Tol)
	{
		ABRExit* Best = nullptr;
		double BestD = Tol;
		for (TActorIterator<ABRExit> It(World); It; ++It)
		{
			const double D = FVector::Dist2D(It->GetActorLocation(), Spot);
			if (It->Target == Target && It->IsClimbable() && !It->IsHidden() && D < BestD)
			{
				Best = *It;
				BestD = D;
			}
		}
		return Best;
	}

	/** Level 6 : centre de la cellule de l'alcove de l'echelle de l'aile (BRLidar::ToCell(aile, WingCells - 1, LadderSlot), comme
	 *  ABRWorld::PlaceMission) */
	bool WingLadderSpot(const ABRWorld* W, FVector& Out)
	{
		const BRLidar::FWing& Wing = W->GetLidarWing();
		if (!Wing.bValid)
		{
			return false;
		}
		int CellX = 0;
		int CellY = 0;
		BRLidar::ToCell(Wing, BRLidar::WingCells - 1, Wing.LadderSlot, CellX, CellY);
		Out = W->CellCenter(FIntPoint(CellX, CellY), 0.f);
		return true;
	}

	/** Entites a moins de Radius cm ecartees (ABREntity::Dismiss, hote) : une frayeur bloquerait les entrees du grimpeur */
	int32 DismissNear(UWorld* World, const FVector& At, float Radius)
	{
		int32 N = 0;
		for (TActorIterator<ABREntity> It(World); It; ++It)
		{
			if (FVector::Dist(It->GetActorLocation(), At) < Radius)
			{
				It->Dismiss();
				++N;
			}
		}
		return N;
	}

	/** Les deux requetes de la verification du developpeur (ABRExit::CheckDepartureViewPoint) : point vise par le depart de groupe,
	 *  capsule du grimpeur a l'accroche ; "libre" ou ce qui les prend */
	FString LadderTaken(UWorld* World, const ABRExit* Exit, bool bClimber)
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestV416LadderClear), false, Exit);
		FCollisionResponseParams R;
		R.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		constexpr float Radius = BRMech::Clearance::PlayerRadius - BRMech::Clearance::Margin;
		constexpr float CapHalf = 88.f - 2.f * BRMech::Clearance::Margin;
		const FVector Anchor = Exit->GetClimbAnchor();
		const FVector At = bClimber ? FVector(Anchor.X, Anchor.Y, Exit->GetActorLocation().Z + 5.f + 2.f * BRMech::Clearance::Margin + CapHalf)
			: ABRWorld::DepartureViewPoint(Exit);
		const FCollisionShape Shape = bClimber ? FCollisionShape::MakeCapsule(Radius, CapHalf) : FCollisionShape::MakeSphere(10.f);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, At, FQuat::Identity, ECC_WorldStatic, Shape, Q, R);
		for (const FOverlapResult& O : Overlaps)
		{
			if (O.bBlockingHit)
			{
				return FString::Printf(TEXT("PRIS dans %s (%s)"), *GetNameSafe(O.GetActor()), *GetNameSafe(O.GetComponent()));
			}
		}
		return FString(TEXT("libre"));
	}

	/** Ce qui prend la capsule du grimpeur (reduite de 1,5 cm : un contact ne compte pas ; pions ignores) : vide si rien */
	FString InsideWhat(const ABRCharacter* C)
	{
		UWorld* World = C->GetWorld();
		const UCapsuleComponent* Cap = C->GetCapsuleComponent();
		if (!World || !Cap)
		{
			return FString();
		}
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestV416Inside), false, C);
		FCollisionResponseParams R;
		R.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, C->GetActorLocation(), C->GetActorQuat(), ECC_Pawn,
			FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius() - 1.5f, Cap->GetScaledCapsuleHalfHeight() - 1.5f), Q, R);
		for (const FOverlapResult& O : Overlaps)
		{
			if (O.bBlockingHit)
			{
				return FString::Printf(TEXT("%s (%s)"), *GetNameSafe(O.GetActor()), *GetNameSafe(O.GetComponent()));
			}
		}
		return FString();
	}

	/** Place debout (centre de la capsule, 5 cm au-dessus du sol) en (XY.X, XY.Y) : sol (trace) a FloorZ (10 cm pres), capsule
	 *  libre (rayon 36, pions ignores). v4.16 (echelles, correctifs) : demi-hauteur de la capsule elle-meme (CapsuleHalf416), aussi
	 *  pendant le vol du developpeur */
	bool FreeStand(const ABRWorld* W, const ABRCharacter* C, const FVector& XY, float FloorZ, FVector& Out)
	{
		const float Half = CapsuleHalf416(C);
		float Floor = 0.f;
		if (!BRTestV412::FloorUnder(W, FVector(XY.X, XY.Y, 0.f), FloorZ + 100.f, C, Floor) || FMath::Abs(Floor - FloorZ) > 10.f)
		{
			return false;
		}
		const FVector Stand(XY.X, XY.Y, Floor + Half + 5.f);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestV416Stand), false, C);
		FCollisionResponseParams R;
		R.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		if (W->GetWorld()->OverlapAnyTestByChannel(Stand, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(36.f, Half - 4.f), Q, R))
		{
			return false;
		}
		Out = Stand;
		return true;
	}

	/** Place debout devant l'accroche de E (90, 70 ou 120 cm devant elle) : sol a 40 cm du pied au plus (tablier incline du Niveau
	 *  8), capsule libre */
	bool FrontStand(const ABRWorld* W, const ABRCharacter* C, const ABRExit* E, FVector& Out)
	{
		const FVector Anchor = E->GetClimbAnchor();
		const FVector Front = LadderFrontOf(E);
		const float FootZ = static_cast<float>(E->GetActorLocation().Z);
		for (const float Ahead : { 90.f, 70.f, 120.f })
		{
			const FVector P = Anchor + Front * Ahead;
			float Floor = 0.f;
			if (BRTestV412::FloorUnder(W, FVector(P.X, P.Y, 0.f), FootZ + 100.f, C, Floor) && FMath::Abs(Floor - FootZ) <= 40.f && FreeStand(W, C, P, Floor, Out))
			{
				return true;
			}
		}
		return false;
	}

	/** Une face (k : 0 +X, 1 -X, 2 +Y, 3 -Y) de la cellule est fermee (mur, porte, cellule pleine) */
	bool SideClosed(const ABRWorld* W, const FIntPoint& Cell, int32 K)
	{
		const int32 DX = BRMech::Grid::DirX[K];
		const int32 DY = BRMech::Grid::DirY[K];
		const EBREdge E = DX != 0 ? W->EdgeE(DX > 0 ? Cell.X : Cell.X - 1, Cell.Y) : W->EdgeN(Cell.X, DY > 0 ? Cell.Y : Cell.Y - 1);
		return E != EBREdge::Open || !W->IsWalkable(Cell + FIntPoint(DX, DY));
	}

	/** Place sur la grille seule (avant la construction du chunk) : cellule de l'echelle, ou voisine par une arete ouverte ; sol a
	 *  FloorZ (0,5 cm pres, ABRWorld::FloorZAt : ni bassin, ni volume d'eau local, ni marche, ni trottoir) sous l'empreinte de la
	 *  capsule (9 points a 40 cm) ; a 100 cm au moins d'une face fermee de sa cellule (mur de 120 cm, capsule et marge) ; hors des
	 *  cellules de la mission et du depart */
	bool GridStand(const ABRWorld* W, const FIntPoint& LadderCell, const FVector& P, float FloorZ)
	{
		const FBRLevelDef& D = W->Def();
		const FIntPoint Cell = W->WorldToCell(P);
		if (!W->IsWalkable(Cell) || W->IsPoolCell(Cell.X, Cell.Y) || W->IsMissionCell(Cell.X, Cell.Y) || W->IsSpawnArea(Cell.X, Cell.Y))
		{
			return false;
		}
		if (Cell != LadderCell)
		{
			const FIntPoint DC = Cell - LadderCell;
			if (FMath::Abs(DC.X) + FMath::Abs(DC.Y) != 1)
			{
				return false;
			}
			const EBREdge E = DC.X != 0 ? W->EdgeE(FMath::Min(Cell.X, LadderCell.X), Cell.Y) : W->EdgeN(Cell.X, FMath::Min(Cell.Y, LadderCell.Y));
			if (E != EBREdge::Open)
			{
				return false;
			}
		}
		for (int32 I = -1; I <= 1; ++I)
		{
			for (int32 J = -1; J <= 1; ++J)
			{
				if (FMath::Abs(W->FloorZAt(P + FVector(40.f * I, 40.f * J, 0.f)) - FloorZ) > 0.5f)
				{
					return false;
				}
			}
		}
		const float S = D.CellSize;
		const float LX = static_cast<float>(P.X) - Cell.X * S;
		const float LY = static_cast<float>(P.Y) - Cell.Y * S;
		const float Dist[4] = { S - LX, LX, S - LY, LY };
		for (int32 K = 0; K < 4; ++K)
		{
			if (SideClosed(W, Cell, K) && Dist[K] < D.WallThickness * 0.5f + 40.f)
			{
				return false;
			}
		}
		return true;
	}

	/** Une echelle des chunks tiree par la regle du jeu, ses places de depart (grille) */
	struct FCandidate
	{
		FVector Foot = FVector::ZeroVector;
		FIntPoint Chunk = FIntPoint::ZeroValue;
		FIntPoint Cell = FIntPoint::ZeroValue;
		FVector Front = FVector(1.0, 0.0, 0.0);
		/** Places (XY ; Z : sol attendu) et leur position (devant l'accroche, de cote) */
		TArray<FVector> Stands;
		TArray<FVector2D> StandAt;
	};

	/** Suivi d'une prise image par image : pieds, tete, capsule dans une collision */
	struct FTrack
	{
		float StartFeet = 0.f;
		float MaxFeet = -1.0e9f;
		float MaxHead = -1.0e9f;
		int32 Samples = 0;
		int32 Inside = 0;
		FString InsideFirst;

		void Begin(const ABRCharacter* C)
		{
			StartFeet = FeetZOf(C);
			MaxFeet = StartFeet;
			MaxHead = static_cast<float>(C->GetActorLocation().Z) + CapsuleHalf416(C);
			Samples = 0;
			Inside = 0;
			InsideFirst.Reset();
		}

		void Sample(const ABRCharacter* C)
		{
			++Samples;
			MaxFeet = FMath::Max(MaxFeet, FeetZOf(C));
			MaxHead = FMath::Max(MaxHead, static_cast<float>(C->GetActorLocation().Z) + CapsuleHalf416(C));
			const FString What = InsideWhat(C);
			if (!What.IsEmpty())
			{
				if (Inside == 0)
				{
					InsideFirst = What;
				}
				++Inside;
			}
		}

		FString InsideText() const
		{
			return Inside == 0 ? FString::Printf(TEXT("capsule dans une collision : 0 image sur %d"), Samples)
				: FString::Printf(TEXT("capsule DANS UNE COLLISION : %d image(s) sur %d (%s)"), Inside, Samples, *InsideFirst);
		}
	};

	/** Etat des etapes (solo et reseau) */
	struct FLadderTest
	{
		/** Tirage : echelles vers le Niveau 2 sur un trottoir avec une place dans le canal, au fond du canal avec un sol plat devant */
		TArray<FCandidate> Deck;
		TArray<FCandidate> Flat;
		int32 Drawn = 0;
		int32 OnDeck = 0;
		/** Echelle en cours (Deck ou Flat) : construite, libre, place verifiee */
		bool bReady = false;
		FCandidate Cur;
		TWeakObjectPtr<ABRExit> Exit;
		FVector Stand = FVector::ZeroVector;
		FVector2D StandAt = FVector2D::ZeroVector;
		float FootZ = 0.f;
		float TopZ = 0.f;
		int32 Index = 0;
		FString Tried;
		/** Sous-etapes */
		int32 Phase = 0;
		int32 Trial = 0;
		double Clock = 0.0;
		double Since = 0.0;
		bool bFlying = false;
		bool bDirect = false;
		bool bOk = false;
		float Value = 0.f;
		int32 RequestsMark = 0;
		/** Montee et descente des Niveaux 8 et 6 : pieds atteints et vises, pieds au sol apres la descente, entites ecartees,
		 *  invincibilite d'avant l'etape */
		float Climbed = 0.f;
		float GroundFeet = 0.f;
		int32 Cleared = 0;
		bool bGodWas = false;
		/** v4.16 (echelles, correctifs) : phase du cas des Niveaux 8 et 6 quand le niveau a ete quitte (0 : jamais) */
		int32 LeftPhase = 0;
		FTrack Track;
		TArray<TWeakObjectPtr<AActor>> Blocks;
		/** Reseau */
		bool bNetOk = false;
		int32 Serial = 0;
		uint32 Hash = 0;
		bool bDepStarted = false;
		bool bDepPhase2 = false;
		bool bDepCancelled = false;
		uint8 DepReason = 0;
		uint8 MaxReady = 0;
		uint8 MaxNeeded = 0;
		int32 GatherChanges = 0;
		FString GatherState;
		BRTestNet::FPlacement Place;
	};
	FLadderTest L16;

	/** Tirage des echelles vers le Niveau 2 des chunks -PlanRange..PlanRange (regle du jeu : ABRWorld::IsChunkExitDrawn et
	 *  PickChunkWallFace ; pied comme ABRChunk::PlanExits), puis leurs places de depart sur la grille : sur un trottoir, dans le
	 *  canal devant elles (140 a 250 cm devant l'accroche, jusqu'a 150 cm de cote, 12 au plus) ; au fond du canal, sol plat de
	 *  l'accroche a 200 cm devant elle (place a 160 cm). Les plus proches du joueur d'abord */
	void PlanLadders(const ABRWorld* W, const FVector& From)
	{
		L16.Deck.Reset();
		L16.Flat.Reset();
		L16.Drawn = 0;
		L16.OnDeck = 0;
		const FBRLevelDef& D = W->Def();
		for (int32 CX = -PlanRange; CX <= PlanRange; ++CX)
		{
			for (int32 CY = -PlanRange; CY <= PlanRange; ++CY)
			{
				for (int32 I = 0; I < D.Exits.Num(); ++I)
				{
					BRMech::Grid::FWallFace Face;
					const FIntPoint Chunk(CX, CY);
					if (D.Exits[I].Style != EBRExitStyle::Ladder || D.Exits[I].Target != PoolTarget || !W->IsChunkExitDrawn(Chunk, I) || !W->PickChunkWallFace(Chunk, I, nullptr, Face))
					{
						continue;
					}
					++L16.Drawn;
					const FVector Dir(BRMech::Grid::DirX[Face.Dir], BRMech::Grid::DirY[Face.Dir], 0.f);
					const float Inset = Face.bSolid ? 0.f : D.WallThickness * 0.5f;
					FCandidate K;
					K.Foot = W->CellCenter(FIntPoint(Face.CellX, Face.CellY), 0.f) + Dir * (D.CellSize * 0.5f - Inset);
					K.Foot.Z = W->FloorZAt(K.Foot - Dir * BRMech::Grid::FootProbe);
					K.Chunk = Chunk;
					K.Cell = FIntPoint(Face.CellX, Face.CellY);
					K.Front = -Dir;
					const FVector Side(-K.Front.Y, K.Front.X, 0.f);
					const FVector Anchor = K.Foot + K.Front * BRMech::Grid::ClimbAnchor;
					if (K.Foot.Z >= 30.f)
					{
						++L16.OnDeck;
						for (float Ahead = 140.f; Ahead <= 250.f && K.Stands.Num() < 12; Ahead += 10.f)
						{
							for (const float Lat : { 0.f, 25.f, -25.f, 50.f, -50.f, 75.f, -75.f, 100.f, -100.f, 150.f, -150.f })
							{
								const FVector P = Anchor + K.Front * Ahead + Side * Lat;
								if (K.Stands.Num() < 12 && GridStand(W, K.Cell, P, 0.f))
								{
									K.Stands.Add(FVector(P.X, P.Y, 0.f));
									K.StandAt.Add(FVector2D(Ahead, Lat));
								}
							}
						}
						if (K.Stands.Num() > 0)
						{
							L16.Deck.Add(K);
						}
						continue;
					}
					// Au fond du canal : sol plat de l'accroche a 200 cm devant elle, sur 80 cm de large (caisses des cas limites)
					bool bFlat = true;
					for (float Ahead = 0.f; Ahead <= 200.f && bFlat; Ahead += 10.f)
					{
						for (const float Lat : { -40.f, 0.f, 40.f })
						{
							const FVector P = Anchor + K.Front * Ahead + Side * Lat;
							bFlat = bFlat && FMath::Abs(W->FloorZAt(P) - K.Foot.Z) < 0.5f && (W->WorldToCell(P) == K.Cell || GridStand(W, K.Cell, P, K.Foot.Z));
						}
					}
					const FVector FlatStand = Anchor + K.Front * 160.f;
					if (bFlat && GridStand(W, K.Cell, FlatStand, K.Foot.Z))
					{
						K.Stands.Add(FVector(FlatStand.X, FlatStand.Y, K.Foot.Z));
						K.StandAt.Add(FVector2D(160.f, 0.f));
						L16.Flat.Add(K);
					}
				}
			}
		}
		auto Closer = [&From](const FCandidate& A, const FCandidate& B) { return FVector::Dist2D(A.Foot, From) < FVector::Dist2D(B.Foot, From); };
		L16.Deck.Sort(Closer);
		L16.Flat.Sort(Closer);
	}

	/** v4.16 : pieds des echelles vers le Niveau 2 tirees par les chunks -PlanRange..PlanRange (regle du jeu, pied comme
	 *  ABRChunk::PlanExits), dans l'ordre du balayage ; OutText : ces pieds (empreinte comparee entre l'hote et le client) */
	TArray<FVector> DrawnFeet(const ABRWorld* W, FString& OutText)
	{
		TArray<FVector> Out;
		OutText.Reset();
		const FBRLevelDef& D = W->Def();
		for (int32 CX = -PlanRange; CX <= PlanRange; ++CX)
		{
			for (int32 CY = -PlanRange; CY <= PlanRange; ++CY)
			{
				for (int32 I = 0; I < D.Exits.Num(); ++I)
				{
					BRMech::Grid::FWallFace Face;
					const FIntPoint Chunk(CX, CY);
					if (D.Exits[I].Style != EBRExitStyle::Ladder || D.Exits[I].Target != PoolTarget || !W->IsChunkExitDrawn(Chunk, I) || !W->PickChunkWallFace(Chunk, I, nullptr, Face))
					{
						continue;
					}
					const FVector Dir(BRMech::Grid::DirX[Face.Dir], BRMech::Grid::DirY[Face.Dir], 0.f);
					const float Inset = Face.bSolid ? 0.f : D.WallThickness * 0.5f;
					FVector Foot = W->CellCenter(FIntPoint(Face.CellX, Face.CellY), 0.f) + Dir * (D.CellSize * 0.5f - Inset);
					Foot.Z = W->FloorZAt(Foot - Dir * BRMech::Grid::FootProbe);
					Out.Add(Foot);
					OutText += FString::Printf(TEXT("%s(%.0f, %.0f, %.0f)"), OutText.IsEmpty() ? TEXT("") : TEXT(" "), Foot.X, Foot.Y, Foot.Z);
				}
			}
		}
		return Out;
	}

	FString CandidateText(const TArray<FCandidate>& List)
	{
		FString Out;
		for (const FCandidate& K : List)
		{
			Out += FString::Printf(TEXT("%s(%.0f, %.0f, %.0f) chunk (%d, %d), %d place(s)"), Out.IsEmpty() ? TEXT("") : TEXT(" ; "), K.Foot.X, K.Foot.Y, K.Foot.Z, K.Chunk.X, K.Chunk.Y,
				K.Stands.Num());
		}
		return Out.IsEmpty() ? FString(TEXT("aucune")) : Out;
	}

	/** Boite de collision posee par le test (profil BlockAll, comme le decor ; cube visible pour les captures) */
	AActor* SpawnBlock(UWorld* World, const FVector& Center, const FVector& Extent, float Yaw)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Center), SP);
		if (!A)
		{
			return nullptr;
		}
		UBoxComponent* Box = NewObject<UBoxComponent>(A, TEXT("BRTestV416Block"));
		Box->SetMobility(EComponentMobility::Movable);
		Box->SetBoxExtent(Extent, false);
		Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		A->SetRootComponent(Box);
		Box->RegisterComponent();
		Box->SetWorldLocationAndRotation(Center, FRotator(0.f, Yaw, 0.f));
		if (UBRAssets* Assets = UBRAssets::Get(A))
		{
			if (UStaticMesh* Cube = Assets->Cube())
			{
				UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(A, TEXT("BRTestV416BlockMesh"));
				Mesh->SetMobility(EComponentMobility::Movable);
				Mesh->SetStaticMesh(Cube);
				Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Mesh->SetupAttachment(Box);
				Mesh->SetRelativeScale3D(Extent / 50.f);
				Mesh->RegisterComponent();
			}
		}
		L16.Blocks.Add(A);
		return A;
	}

	void ClearBlocks()
	{
		for (const TWeakObjectPtr<AActor>& B : L16.Blocks)
		{
			if (AActor* A = B.Get())
			{
				A->Destroy();
			}
		}
		L16.Blocks.Reset();
	}

	/** v4.16 (echelles, relecture) : vol du developpeur de la conduite arrete (etape finie, ou bloquee : sinon le joueur volerait,
	 *  sans collision, jusque dans le niveau suivant) */
	void StopFlying416(ABRCharacter* C)
	{
		if (L16.bFlying && C)
		{
			C->SetDevFly(false);
		}
		L16.bFlying = false;
	}

	/** Fin d'une prise : lacher l'echelle, vol du developpeur arrete, boites retirees */
	void EndClimbCase(ABRCharacter* C)
	{
		if (C && C->IsClimbing())
		{
			C->StopClimb();
		}
		ClearBlocks();
		L16.Phase = 0;
		L16.Trial = 0;
	}

	/** Echelle en cours retrouvee (son chunk a pu etre reconstruit) */
	ABRExit* CurrentLadder(UWorld* World)
	{
		if (!L16.bReady)
		{
			return nullptr;
		}
		ABRExit* E = L16.Exit.Get();
		if (!E)
		{
			E = LadderNear(World, L16.Cur.Foot, PoolTarget, 2.0);
			L16.Exit = E;
		}
		return E;
	}

	/** Depart d'une place ou le client se tient loin du point de rassemblement (comme le test reseau v4.12) : 9 a 12 m, meme
	 *  etage (sol a FloorZ, 80 cm pres), capsule libre, point vise par le depart en vue */
	bool FarStand416(UWorld* World, const ABRWorld* W, const ABRCharacter* C, const ABRExit* E, FVector& Out)
	{
		const FVector Gather = E->GetActorLocation() + LadderFrontOf(E) * 100.f;
		const FVector Aim = ABRWorld::DepartureViewPoint(E);
		const float Half = CapsuleHalf416(C);
		const float FloorZ = static_cast<float>(E->GetActorLocation().Z);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRTestV416FarStand), false, C);
		Q.AddIgnoredActor(E);
		FCollisionResponseParams WallsOnly;
		WallsOnly.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		for (const float Rad : { 1000.f, 1100.f, 1200.f, 950.f })
		{
			for (int32 K = 0; K < 16; ++K)
			{
				const FVector P = Gather + FVector(1.f, 0.f, 0.f).RotateAngleAxis(K * 22.5f, FVector::UpVector) * Rad;
				const FIntPoint Cell = W->WorldToCell(P);
				if (!W->IsWalkable(Cell) || W->IsPoolCell(Cell.X, Cell.Y) || !W->IsChunkLoaded(W->CellToChunk(Cell)))
				{
					continue;
				}
				FHitResult Floor;
				if (!World->LineTraceSingleByChannel(Floor, FVector(P.X, P.Y, FloorZ + 150.f), FVector(P.X, P.Y, FloorZ - 150.f), ECC_WorldStatic, Q, WallsOnly)
					|| FMath::Abs(Floor.ImpactPoint.Z - FloorZ) > 80.f)
				{
					continue;
				}
				const FVector Spot(P.X, P.Y, Floor.ImpactPoint.Z + Half + 5.f);
				if (FVector::Dist2D(Spot, Gather) < 900.f || World->OverlapAnyTestByChannel(Spot, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(36.f, Half - 4.f), Q, WallsOnly))
				{
					continue;
				}
				if (World->LineTraceTestByChannel(Spot + FVector(0.f, 0.f, Half * 0.75f), Aim, ECC_WorldStatic, Q, WallsOnly))
				{
					continue;
				}
				Out = Spot;
				return true;
			}
		}
		return false;
	}
}

// v4.16 (echelles, relecture) : etapes partagees avec -BRAutoTestV415Fix (AddV415FixSteps : la prise depuis le canal y est jugee
// sur une echelle des chunks qui a une place dans le canal devant elle, celle du chunk (-1, -1) n'en ayant pas)

// Partie classique imposee avant le chargement (ABRWorld::SetLinearTestOverride) : le parcours lineaire ne tire aucune sortie
// des chunks (une seule sortie, celle de la mission) ; Niveau 37 charge a la graine DU NIVEAU (AddLevelSeedLoad, sans seconde
// transformation), puis tirage des echelles et de leurs places (grille seule, chunks pas encore construits)
void ABRAutoTest::AddV416LadderPlan(uint32 Seed)
{
	Add(FString::Printf(TEXT("v4.16 echelles : partie classique imposee (Niveau 37, graine du niveau %u)"), Seed), 0.f, [this]()
	{
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		// v4.16 (echelles, relecture) : jamais de vol du developpeur (sans collision) herite d'une etape bloquee
		BRTestV416Ladder::StopFlying416(GetPlayer());
		W->SetLinearTestOverride(0);
		return true;
	});
	AddLevelSeedLoad(37, Seed, FString::Printf(TEXT("v4.16 echelles : Niveau 37 (graine du niveau %u, partie classique)"), Seed));
	Add(FString::Printf(TEXT("v4.16 echelles : echelles des chunks vers le Niveau 2 et places de depart (graine du niveau %u)"), Seed), 0.5f, [this, Seed]()
	{
		using namespace BRTestV416Ladder;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		L16.Deck.Reset();
		L16.Flat.Reset();
		L16.Drawn = 0;
		L16.OnDeck = 0;
		if (W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 37 || W->GetSeed() != Seed)
		{
			Skip(FString::Printf(TEXT("Niveau 37 (graine du niveau %u) non charge (Niveau %d, graine %u) : echelles des chunks non verifiees"), Seed, W->GetLevelNumber(), W->GetSeed()));
			return true;
		}
		if (W->IsLinearLevel())
		{
			Skip(TEXT("Niveau 37 charge en parcours lineaire malgre la partie classique imposee : aucune sortie des chunks, echelles non verifiees"));
			return true;
		}
		if (BRLevels::ResolveExit(37, PoolTarget).Kind == BRContent::EExit::Sealed)
		{
			Skip(TEXT("Niveau 37 : passage vers le Niveau 2 condamne dans ce canal : echelles des chunks non verifiees"));
			return true;
		}
		PlanLadders(W, C->GetActorLocation());
		Note(FString::Printf(TEXT("Niveau 37 (graine du niveau %u, partie classique) : %d echelle(s) vers le Niveau 2 tirees (chunks -%d..%d), %d sur un trottoir ; sur un trottoir avec "
			"une place dans le canal devant elle (grille) : %s ; au fond du canal avec un sol plat devant elle : %s"), Seed, L16.Drawn, PlanRange, PlanRange, L16.OnDeck,
			*CandidateText(L16.Deck), *CandidateText(L16.Flat)));
		if (L16.Drawn == 0)
		{
			Skip(FString::Printf(TEXT("Niveau 37 (graine du niveau %u) : aucune echelle vers le Niveau 2 tiree dans les chunks -%d..%d : prises non verifiees"), Seed, PlanRange, PlanRange));
		}
		return true;
	});
}

// Conduite a une echelle de la liste (Deck : sur un trottoir, place dans le canal ; Flat : au fond du canal), chunk construit,
// echelle jugee, place verifiee par les collisions ; la suivante si la place est prise. Aucune : NON VERIFIE
void ABRAutoTest::AddV416LadderGoTo(bool bFlatList, const FString& Title)
{
	Add(Title + TEXT(" : debut"), 0.f, [this]()
	{
		using namespace BRTestV416Ladder;
		L16.bReady = false;
		L16.Exit.Reset();
		L16.Index = 0;
		L16.Phase = 0;
		// v4.16 (echelles, relecture) : premier essai de la prise depuis le canal (aussi apres une etape bloquee, et dans
		// -BRAutoTestV415Fix)
		L16.Trial = 0;
		L16.Tried.Reset();
		return true;
	});
	Add(Title, 0.f, [this, bFlatList]()
	{
		using namespace BRTestV416Ladder;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		const TArray<FCandidate>& List = bFlatList ? L16.Flat : L16.Deck;
		const TCHAR* Kind = bFlatList ? TEXT("au fond du canal (sol plat devant elle)") : TEXT("sur un trottoir avec une place dans le canal devant elle");
		const double Now = FPlatformTime::Seconds();
		auto Finish416 = [&]()
		{
			StopFlying416(C);
			L16.Phase = 0;
			return true;
		};
		if (L16.Phase == 0)
		{
			if (W->GetLevelNumber() != 37 || W->IsLinearLevel())
			{
				Skip(FString::Printf(TEXT("echelle %s : Niveau 37 en partie classique absent (Niveau %d) : prises non verifiees"), Kind, W->GetLevelNumber()));
				return Finish416();
			}
			// Les MaxTried plus proches au plus (30 s chacune pour la construction de son chunk)
			if (L16.Index >= FMath::Min(List.Num(), MaxTried))
			{
				Skip(FString::Printf(TEXT("echelle %s : NON VERIFIE, aucune construite avec sa place libre parmi les %d essayee(s) sur %d tiree(s) (%s)"), Kind, FMath::Min(List.Num(), MaxTried),
					List.Num(), L16.Tried.IsEmpty() ? TEXT("aucune") : *L16.Tried));
				return Finish416();
			}
			L16.Cur = List[L16.Index];
			L16.Exit.Reset();
			// Vol du developpeur (sans collision) au-dessus de la premiere place, le temps que le chunk se construise
			if (!L16.bFlying)
			{
				C->SetDevFly(true);
				L16.bFlying = true;
			}
			C->SetActorLocation(FVector(L16.Cur.Stands[0].X, L16.Cur.Stands[0].Y, L16.Cur.Foot.Z + 300.f), false, nullptr, ETeleportType::TeleportPhysics);
			L16.Clock = Now;
			L16.Phase = 1;
			return false;
		}
		if (L16.Phase == 1)
		{
			ABRExit* E = W->IsChunkLoaded(L16.Cur.Chunk) ? LadderNear(GetWorld(), L16.Cur.Foot, PoolTarget, 2.0) : nullptr;
			const FIntPoint StandChunk = W->CellToChunk(W->WorldToCell(L16.Cur.Stands[0]));
			if ((!E || !W->IsChunkLoaded(StandChunk)) && Now - L16.Clock < 30.0)
			{
				return false;
			}
			if (!E)
			{
				L16.Tried += FString::Printf(TEXT("%s(%.0f, %.0f, %.0f) : %s"), L16.Tried.IsEmpty() ? TEXT("") : TEXT(" ; "), L16.Cur.Foot.X, L16.Cur.Foot.Y, L16.Cur.Foot.Z,
					W->IsChunkLoaded(L16.Cur.Chunk) ? TEXT("echelle introuvable dans le chunk construit") : TEXT("chunk non construit en 30 s"));
				++L16.Index;
				L16.Phase = 0;
				return false;
			}
			// Echelle construite : point vise et capsule du grimpeur libres, puis la premiere place libre
			const FString View = LadderTaken(GetWorld(), E, false);
			const FString Climb = LadderTaken(GetWorld(), E, true);
			FVector Stand = FVector::ZeroVector;
			int32 Found = INDEX_NONE;
			for (int32 K = 0; K < L16.Cur.Stands.Num() && Found == INDEX_NONE; ++K)
			{
				if (FreeStand(W, C, L16.Cur.Stands[K], static_cast<float>(L16.Cur.Stands[K].Z), Stand))
				{
					Found = K;
				}
			}
			if (View != TEXT("libre") || Climb != TEXT("libre") || Found == INDEX_NONE)
			{
				L16.Tried += FString::Printf(TEXT("%s(%.0f, %.0f, %.0f) : point vise %s, capsule du grimpeur %s, place libre %s"), L16.Tried.IsEmpty() ? TEXT("") : TEXT(" ; "),
					L16.Cur.Foot.X, L16.Cur.Foot.Y, L16.Cur.Foot.Z, *View, *Climb, OuiNon(Found != INDEX_NONE));
				++L16.Index;
				L16.Phase = 0;
				return false;
			}
			C->SetDevFly(false);
			L16.bFlying = false;
			PutAt416(C, Stand, LadderAimOf(E));
			L16.Exit = E;
			L16.Stand = Stand;
			L16.StandAt = L16.Cur.StandAt[Found];
			L16.FootZ = static_cast<float>(E->GetActorLocation().Z);
			L16.TopZ = E->GetClimbTopZ();
			L16.bReady = true;
			// La sortie est gardee par la mission du niveau : resolue par l'hote (seule l'echelle est jugee ici)
			W->DebugCompleteMission();
			const FVector Anchor = E->GetClimbAnchor();
			Note(FString::Printf(TEXT("Niveau 37 (graine du niveau %u) : echelle %s : %s en (%.0f, %.0f, %.0f), chunk (%d, %d), sommet de la montee a %.0f cm du pied ; point vise %s, "
				"capsule du grimpeur %s ; place libre en (%.0f, %.0f, %.0f), %.0f cm devant l'accroche, %.0f cm de cote (accroche (%.0f, %.0f, %.0f))%s"), W->GetSeed(), Kind, *E->GetName(),
				L16.Cur.Foot.X, L16.Cur.Foot.Y, L16.FootZ, L16.Cur.Chunk.X, L16.Cur.Chunk.Y, L16.TopZ - L16.FootZ, *View, *Climb, Stand.X, Stand.Y, Stand.Z - CapsuleHalf416(C) - 5.f,
				L16.StandAt.X, L16.StandAt.Y, Anchor.X, Anchor.Y, Anchor.Z, L16.Tried.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" ; ecartees avant : %s"), *L16.Tried)));
			// v4.16 (echelles, relecture) : la place de la regle du jeu au chunk (2, -1) (graine du niveau 2303284074) n'est plus jugee
			// ici (avant : seulement si l'echelle retenue etait celle-la, sinon aucune ligne) mais par une etape a elle, apres les caisses
			Shot(bFlatList ? TEXT("v416_echelle_fond_du_canal") : TEXT("v416_echelle_trottoir_canal"));
			return Finish416();
		}
		return Finish416();
	});
	LimitLastStep(200.f);
}

// Deux prises depuis la meme place du canal : avancer tout de suite (puis lacher : debout sur le trottoir), puis 0,5 s apres
// (puis montee de 2 m et descente jusqu'au lacher sur le trottoir). Visee et touche d'interaction ; echelle pas visee depuis le
// canal : NON VERIFIE pour la visee, et la prise directe (StartClimb) juge quand meme la montee
void ABRAutoTest::AddV416LadderCanalGrab()
{
	Add(TEXT("v4.16 echelles : prise depuis le canal, montee au-dessus du trottoir, lacher, montee et descente"), 0.f, [this]()
	{
		using namespace BRTestV416Ladder;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		ABRExit* E = L16.bReady ? CurrentLadder(GetWorld()) : nullptr;
		if (!E || L16.Cur.Foot.Z < 30.f)
		{
			if (L16.Phase == 0 && L16.Trial == 0)
			{
				Skip(TEXT("prise depuis le canal devant une echelle sur son trottoir : NON VERIFIE (aucune echelle sur un trottoir construite avec une place libre dans le canal, voir plus haut)"));
			}
			else
			{
				// v4.16 (echelles, relecture) : echelle disparue au milieu du cas (avant : la ligne "aucune echelle construite")
				Note(FString::Printf(TEXT("prise depuis le canal : echelle sur son trottoir DISPARUE pendant le cas (essai %d, phase %d) : verdict non rendu"), L16.Trial + 1, L16.Phase), true);
			}
			EndClimbCase(C);
			return true;
		}
		const double Now = FPlatformTime::Seconds();
		const float Delay = L16.Trial == 0 ? 0.f : 0.5f;
		const TCHAR* When = L16.Trial == 0 ? TEXT("avancer tout de suite") : TEXT("avancer 0,5 s apres la prise");
		const FVector Anchor = E->GetClimbAnchor();
		switch (L16.Phase)
		{
		case 0:
		{
			PutAt416(C, L16.Stand, LadderAimOf(E));
			L16.Clock = Now;
			L16.bDirect = false;
			L16.Phase = 1;
			return false;
		}
		case 1:
		{
			LookAt416(C, LadderAimOf(E));
			if (Now - L16.Clock < 0.6)
			{
				return false;
			}
			if (C->FocusActor.Get() != E)
			{
				if (Now - L16.Clock < 3.0)
				{
					return false;
				}
				Skip(FString::Printf(TEXT("prise depuis le canal (%s) : visee NON VERIFIEE, echelle pas visee depuis le canal a %.0f cm de son pied (vise : %s) ; prise directe (StartClimb) "
					"pour juger la montee"), When, FVector::Dist2D(C->GetActorLocation(), E->GetActorLocation()), *BRTestV412::FocusName(W, C->FocusActor.Get())));
				L16.bDirect = true;
				C->StartClimb(E);
			}
			else
			{
				C->Interact();
			}
			L16.Track.Begin(C);
			L16.Clock = Now;
			L16.Phase = 2;
			return false;
		}
		case 2:
		{
			L16.Track.Sample(C);
			if (!C->IsClimbing())
			{
				if (Now - L16.Clock > 1.0)
				{
					Note(FString::Printf(TEXT("prise depuis le canal (%s) : echelle visee mais PAS PRISE par l'interaction"), When), true);
					EndClimbCase(C);
					return true;
				}
				return false;
			}
			if (Now - L16.Clock >= Delay)
			{
				C->InputMove(FVector2D(0.f, 1.f));
			}
			const float Feet = FeetZOf(C);
			const double D2 = FVector::Dist2D(C->GetActorLocation(), Anchor);
			const bool bUp = Feet >= L16.FootZ + 10.f && D2 < 10.0;
			if (!bUp && Now - L16.Clock < Delay + 3.0)
			{
				return false;
			}
			const bool bOk = bUp && L16.Track.Inside == 0;
			Note(FString::Printf(TEXT("echelle sur son trottoir (pied a %.0f cm) prise depuis le canal (pieds a %.0f cm, %.0f cm devant l'accroche, %.0f cm de cote, %s), %s : %s (pieds a "
				"%.0f cm, a %.0f cm de l'accroche, %.2f s apres la prise ; dernier obstacle : %s) ; %s (attendu : montee, ABRCharacter::UpdateClimb)"), L16.FootZ, L16.Track.StartFeet,
				L16.StandAt.X, L16.StandAt.Y, L16.bDirect ? TEXT("prise directe") : TEXT("visee et touche d'interaction"), When, bUp ? TEXT("montee au-dessus du trottoir")
				: TEXT("ARRETEE (la montee depuis le canal ne passe pas le bord)"), Feet, D2, Now - L16.Clock, *CompName(C->ClimbBlocker.Get()), *L16.Track.InsideText()), !bOk);
			if (!bUp)
			{
				Shot(FString::Printf(TEXT("v416_canal_%d"), L16.Trial));
				EndClimbCase(C);
				return true;
			}
			L16.Clock = Now;
			if (L16.Trial == 0)
			{
				C->Interact(); // lacher l'echelle au-dessus du trottoir
				L16.Phase = 3;
			}
			else
			{
				L16.Phase = 4;
			}
			return false;
		}
		case 3:
		{
			// Lachee au-dessus du trottoir : debout sur lui (ni accrochee, ni en l'air)
			if (Now - L16.Clock < 1.5)
			{
				return false;
			}
			const UCharacterMovementComponent* M = C->GetCharacterMovement();
			const bool bGround = M && M->IsMovingOnGround();
			const float Feet = FeetZOf(C);
			Note(FString::Printf(TEXT("prise depuis le canal (avancer tout de suite) puis lacher (Interagir) : debout sur le trottoir %s (pieds a %.1f cm, pied de l'echelle a %.0f cm), "
				"echelle tenue %s"), OuiNon(bGround && FMath::Abs(Feet - L16.FootZ) < 6.f), Feet, L16.FootZ, OuiNon(C->IsClimbing())), C->IsClimbing() || !bGround
				|| FMath::Abs(Feet - L16.FootZ) >= 6.f);
			L16.Trial = 1;
			L16.Phase = 0;
			return false;
		}
		case 4:
		{
			// Montee de 2 m au-dessus du pied. v4.16 (echelles, correctifs) : TopMargin sous le sommet de la montee au plus (une
			// seconde echelle d'un chunk n'a pas de conduit : sommet de la montee a 288 cm du pied d'un trottoir, des pieds a 200 cm
			// l'atteignaient, demande de depart et depart au Niveau 2 au milieu du cas)
			L16.Track.Sample(C);
			if (C->IsClimbing() && FeetZOf(C) < L16.FootZ + FMath::Min(200.f, L16.TopZ - CapsuleHalf416(C) - TopMargin - L16.FootZ) && Now - L16.Clock < 5.0)
			{
				C->InputMove(FVector2D(0.f, 1.f));
				return false;
			}
			L16.Value = FeetZOf(C) - L16.FootZ;
			L16.Clock = Now;
			L16.Phase = 5;
			return false;
		}
		case 5:
		{
			// Descente jusqu'au lacher (StopClimb a 4 cm du pied, ou ClimbReleaseOnDescent), jamais coince
			L16.Track.Sample(C);
			if (C->IsClimbing() && Now - L16.Clock < 8.0)
			{
				C->InputMove(FVector2D(0.f, -1.f));
				return false;
			}
			L16.Since = Now - L16.Clock;
			L16.bOk = !C->IsClimbing();
			L16.Clock = Now;
			L16.Phase = 6;
			return false;
		}
		default:
		{
			if (Now - L16.Clock < 1.5)
			{
				return false;
			}
			const UCharacterMovementComponent* M = C->GetCharacterMovement();
			const bool bGround = M && M->IsMovingOnGround();
			const float Feet = FeetZOf(C);
			const bool bOk = L16.bOk && bGround && FMath::Abs(Feet - L16.FootZ) < 6.f && L16.Track.Inside == 0;
			Note(FString::Printf(TEXT("prise depuis le canal (avancer 0,5 s apres), montee jusqu'a %.0f cm au-dessus du pied, puis descente : echelle lachee %s apres %.2f s, debout sur le "
				"trottoir %s (pieds a %.1f cm) ; %s (attendu : lachee au pied, debout, jamais coincee)"), L16.Value, L16.bOk ? TEXT("oui") : TEXT("NON (COINCEE en descente)"), L16.Since,
				OuiNon(bGround && FMath::Abs(Feet - L16.FootZ) < 6.f), Feet, *L16.Track.InsideText()), !bOk);
			if (!bOk)
			{
				Shot(TEXT("v416_canal_descente"));
			}
			EndClimbCase(C);
			return true;
		}
		}
	});
	LimitLastStep(45.f);
}

void ABRAutoTest::AddV416LadderSteps()
{
	Add(TEXT("v4.16 echelles : preparation"), 0.f, [this]()
	{
		FLevelReport R;
		R.Title = TEXT("v4.16 (echelles) : echelles des chunks du Niveau 37 et deplacement de secours de la montee");
		R.Scene = TEXT("v4.16");
		R.FirstLogLine = LogLineCount();
		Reports.Add(R);
		BRTestV416Ladder::L16 = BRTestV416Ladder::FLadderTest();
		return true;
	});

	// ---------------------------------------------------------------------------------------------------------------------
	// A. Graine du niveau 2303284074 : echelle au fond du canal, sol plat devant elle : une marche basse seulement (caisses posees
	//    par le test)
	// ---------------------------------------------------------------------------------------------------------------------
	AddV416LadderPlan(BRTestV416Ladder::Pool37FlatSeed);
	AddV416LadderGoTo(true, TEXT("v4.16 echelles : echelle au fond du canal (sol plat), construite et libre"));
	// Caisse de Height cm entre le joueur (160 cm devant l'accroche) et l'accroche (de 80 a 120 cm devant elle, 160 cm de large) ;
	// prise directe (StartClimb : la visee n'est pas jugee ici), avancer 4 s. bPass : franchie et l'accroche atteinte ; sinon
	// aucune hausse au-dela de NoLift, toujours loin de l'accroche ; puis reculer : lachee au pied
	auto AddFlatBox = [this](float Height, bool bPass)
	{
		Add(FString::Printf(TEXT("v4.16 echelles : caisse de %.0f cm devant l'accroche (sol plat)"), Height), 0.f, [this, Height, bPass]()
		{
			using namespace BRTestV416Ladder;
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			ABRExit* E = L16.bReady ? CurrentLadder(GetWorld()) : nullptr;
			const TCHAR* Expect = bPass ? TEXT("franchie (marche basse)") : TEXT("jamais franchie, aucune hausse le long d'elle");
			if (!E || L16.Cur.Foot.Z >= 30.f)
			{
				if (L16.Phase == 0)
				{
					Skip(FString::Printf(TEXT("caisse de %.0f cm devant une echelle au fond du canal : NON VERIFIE (aucune echelle au fond du canal construite avec sa place, voir plus haut) ; "
						"attendu : %s"), Height, Expect));
				}
				else
				{
					// v4.16 (echelles, relecture) : echelle disparue au milieu du cas (chunk reconstruit ?) : notee (avant : etape finie
					// sans ligne, ni "franchie" ni NON VERIFIE)
					Note(FString::Printf(TEXT("caisse de %.0f cm : echelle au fond du canal DISPARUE pendant la prise (phase %d) : verdict non rendu (attendu : %s)"), Height, L16.Phase, Expect),
						true);
				}
				EndClimbCase(C);
				return true;
			}
			const double Now = FPlatformTime::Seconds();
			const FVector Front = LadderFrontOf(E);
			const FVector Anchor = E->GetClimbAnchor();
			switch (L16.Phase)
			{
			case 0:
			{
				PutAt416(C, L16.Stand, LadderAimOf(E));
				SpawnBlock(GetWorld(), FVector(Anchor.X, Anchor.Y, L16.FootZ) + Front * 100.f + FVector(0.f, 0.f, Height * 0.5f), FVector(20.f, 80.f, Height * 0.5f),
					static_cast<float>(Front.Rotation().Yaw));
				L16.Clock = Now;
				L16.Phase = 1;
				return false;
			}
			case 1:
			{
				if (Now - L16.Clock < 0.6)
				{
					return false;
				}
				C->StartClimb(E);
				L16.Track.Begin(C);
				L16.Clock = Now;
				L16.Phase = 2;
				return false;
			}
			case 2:
			{
				L16.Track.Sample(C);
				C->InputMove(FVector2D(0.f, 1.f));
				const double D2 = FVector::Dist2D(C->GetActorLocation(), Anchor);
				const bool bAt = D2 < 10.0 && C->IsClimbing();
				if (!bAt && Now - L16.Clock < 4.0)
				{
					return false;
				}
				const float Rise = L16.Track.MaxFeet - L16.Track.StartFeet;
				const bool bOk = (bPass ? (bAt && L16.Track.MaxFeet >= L16.FootZ + Height) : (!bAt && C->IsClimbing() && Rise < NoLift && D2 > 40.0)) && L16.Track.Inside == 0;
				Note(FString::Printf(TEXT("echelle au fond du canal (pied a %.0f cm), caisse de %.0f cm entre le joueur et l'accroche, avancer 4 s : %s (pieds au plus %.1f cm au-dessus du "
					"depart, a %.0f cm de l'accroche, %.2f s ; dernier obstacle : %s) ; %s (attendu : %s, BRGather::ClimbAfterHit)"), L16.FootZ, Height, bAt ? TEXT("accroche atteinte")
					: (Rise >= NoLift ? TEXT("LEVE contre la caisse") : TEXT("arrete au sol")), Rise, D2, Now - L16.Clock, *CompName(C->ClimbBlocker.Get()), *L16.Track.InsideText(), Expect), !bOk);
				if (!bOk)
				{
					Shot(FString::Printf(TEXT("v416_caisse_%.0f"), Height));
				}
				L16.Clock = Now;
				L16.Phase = 3;
				return false;
			}
			case 3:
			{
				// Reculer : lachee au pied (pieds a 4 cm du pied : StopClimb), jamais accrochee
				if (C->IsClimbing() && Now - L16.Clock < 6.0)
				{
					C->InputMove(FVector2D(0.f, -1.f));
					return false;
				}
				Note(FString::Printf(TEXT("caisse de %.0f cm : puis reculer : echelle lachee %s (pieds a %.1f cm du pied, %.2f s)"), Height, OuiNon(!C->IsClimbing()), FeetZOf(C) - L16.FootZ,
					Now - L16.Clock), C->IsClimbing());
				EndClimbCase(C);
				return true;
			}
			default:
				EndClimbCase(C);
				return true;
			}
		});
		LimitLastStep(30.f);
	};
	AddFlatBox(20.f, true);
	AddFlatBox(50.f, false);
	// v4.16 (echelles, relecture) : place de la regle des correctifs 5 au chunk (2, -1) (BRMech::Grid::StandFlat ; banc
	// test_departure section 5 : (11750, -1560, 0) ; net_final3 : ancienne place (11750, -2060, 0) ; net_final4 : chunk pas construit
	// au depart, NON VERIFIE), jugee a chaque fois dans le chunk construit, quelle que soit l'echelle retenue pour les caisses (avant :
	// seulement si c'etait celle-la, sans ligne sinon). Chunk absent : vol du developpeur au-dessus de la place, 30 s au plus ; pas
	// construit : NON VERIFIE ; construit sans echelle a cette place : PROBLEME
	Add(TEXT("v4.16 echelles : chunk (2, -1), echelle a la place de la regle du jeu (graine du niveau 2303284074)"), 0.f, [this]()
	{
		using namespace BRTestV416Ladder;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		const FVector BenchFoot(FlatBenchX, FlatBenchY, 0.0);
		const FVector OldFoot(FlatBenchX, FlatOldY, 0.0);
		auto Finish416 = [&]()
		{
			StopFlying416(C);
			return true;
		};
		if (W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != 37 || W->GetSeed() != Pool37FlatSeed || W->IsLinearLevel())
		{
			Skip(FString::Printf(TEXT("Niveau 37 (graine du niveau %u, partie classique) non charge (Niveau %d, graine %u, lineaire %s) : place de la regle du jeu au chunk (2, -1) "
				"(%.0f, %.0f, 0) non verifiee"), Pool37FlatSeed, W->GetLevelNumber(), W->GetSeed(), OuiNon(W->IsLinearLevel()), FlatBenchX, FlatBenchY));
			return Finish416();
		}
		const FIntPoint Chunk = W->CellToChunk(W->WorldToCell(BenchFoot));
		const FIntPoint OldChunk = W->CellToChunk(W->WorldToCell(OldFoot));
		const bool bBuilt = W->IsChunkLoaded(Chunk) && W->IsChunkLoaded(OldChunk);
		ABRExit* E = bBuilt ? LadderNear(GetWorld(), BenchFoot, PoolTarget, 100.0) : nullptr;
		if (!E && StepTime < 30.f)
		{
			// Vol du developpeur (sans collision) au-dessus de la place, le temps que son chunk se construise
			if (!bBuilt && !L16.bFlying)
			{
				C->SetDevFly(true);
				L16.bFlying = true;
				C->SetActorLocation(BenchFoot + FVector(0.f, 0.f, 300.f), false, nullptr, ETeleportType::TeleportPhysics);
			}
			return false;
		}
		if (!bBuilt)
		{
			Skip(FString::Printf(TEXT("Niveau 37 (graine du niveau %u), chunk (%d, %d) : NON VERIFIE, chunk non construit en 30 s : place de la regle du jeu (%.0f, %.0f, 0) et ancienne "
				"place (%.0f, %.0f, 0) non jugees"), Pool37FlatSeed, Chunk.X, Chunk.Y, FlatBenchX, FlatBenchY, FlatBenchX, FlatOldY));
			return Finish416();
		}
		// La regle du jeu tire-t-elle toujours une echelle a cette place (tirage des chunks -PlanRange..PlanRange) ?
		FString DrawnText;
		bool bPlanned = false;
		for (const FVector& F : DrawnFeet(W, DrawnText))
		{
			bPlanned = bPlanned || FVector::Dist(F, BenchFoot) < 1.0;
		}
		const bool bAtBench = E && FVector::Dist(E->GetActorLocation(), BenchFoot) < 1.0;
		const bool bOldEmpty = LadderNear(GetWorld(), OldFoot, PoolTarget, 50.0) == nullptr;
		const FString Where = E ? FString::Printf(TEXT("%s en (%.0f, %.0f, %.0f)"), *E->GetName(), E->GetActorLocation().X, E->GetActorLocation().Y, E->GetActorLocation().Z)
			: FString(TEXT("AUCUNE echelle vers le Niveau 2 a moins de 1 m"));
		Note(FString::Printf(TEXT("Niveau 37 (graine du niveau %u), chunk (%d, %d) construit : echelle vers le Niveau 2 a la place de la regle du jeu (banc : (%.0f, %.0f, 0)) %s (%s ; "
			"tiree a cette place par la regle %s) ; ancienne place (%.0f, %.0f, 0) sans echelle %s"), Pool37FlatSeed, Chunk.X, Chunk.Y, FlatBenchX, FlatBenchY, OuiNon(bAtBench), *Where,
			OuiNon(bPlanned), FlatBenchX, FlatOldY, OuiNon(bOldEmpty)), !bAtBench || !bOldEmpty);
		return Finish416();
	});
	LimitLastStep(40.f);

	// ---------------------------------------------------------------------------------------------------------------------
	// B. Graine du niveau 115083815 : echelle sur son trottoir, prise depuis le canal devant elle
	// ---------------------------------------------------------------------------------------------------------------------
	AddV416LadderPlan(BRTestV416Ladder::Pool37Seed);
	AddV416LadderGoTo(false, TEXT("v4.16 echelles : echelle sur son trottoir, place dans le canal devant elle"));
	AddV416LadderCanalGrab();

	// Cas limites depuis la place du canal (prise directe : la boite posee peut couper la visee) : bWall : mur haut (10 cm
	// d'epaisseur, 400 cm de haut, 300 cm de large) entre le joueur et la marche ; sinon plafond bas (dalle 15 cm au-dessus de la
	// tete, du bord de la marche a 100 cm derriere le joueur). Attendu : aucune hausse au-dela de NoLift (avant, v4.15 correctifs 5 :
	// levee jusqu'a une marche au-dessus du pied, 103 cm), toujours loin de l'accroche, jamais dans une collision, tete sous le
	// plafond ; puis reculer : lachee tout de suite (pieds sous le pied de l'echelle)
	auto AddCanalObstacle = [this](bool bWall)
	{
		Add(bWall ? TEXT("v4.16 echelles : mur haut entre le canal et l'accroche") : TEXT("v4.16 echelles : plafond bas au-dessus du canal"), 0.f, [this, bWall]()
		{
			using namespace BRTestV416Ladder;
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			const TCHAR* What = bWall ? TEXT("mur haut (400 cm) entre le joueur dans le canal et l'accroche") : TEXT("plafond bas (15 cm au-dessus de la tete) au-dessus du canal");
			ABRExit* E = L16.bReady ? CurrentLadder(GetWorld()) : nullptr;
			if (!E || L16.Cur.Foot.Z < 30.f)
			{
				if (L16.Phase == 0)
				{
					Skip(FString::Printf(TEXT("%s : NON VERIFIE (aucune echelle sur un trottoir construite avec une place dans le canal) ; attendu : aucune hausse"), What));
				}
				else
				{
					// v4.16 (echelles, relecture) : echelle disparue au milieu du cas : notee (avant : la ligne "aucune echelle construite")
					Note(FString::Printf(TEXT("%s : echelle sur son trottoir DISPARUE pendant le cas (phase %d) : verdict non rendu (attendu : aucune hausse)"), What, L16.Phase), true);
				}
				EndClimbCase(C);
				return true;
			}
			const double Now = FPlatformTime::Seconds();
			const FVector Front = LadderFrontOf(E);
			const FVector Anchor = E->GetClimbAnchor();
			const float Yaw = static_cast<float>(Front.Rotation().Yaw);
			const float Half = CapsuleHalf416(C);
			// Place du joueur le long de l'avant de l'echelle (devant l'accroche) et hauteur de son sol
			const double Ahead = FVector::DotProduct(L16.Stand - Anchor, Front);
			const float FloorZ = static_cast<float>(L16.Stand.Z) - Half - 5.f;
			const float SlabBottom = FloorZ + 2.f * Half + 5.f + 15.f;
			switch (L16.Phase)
			{
			case 0:
			{
				PutAt416(C, L16.Stand, LadderAimOf(E));
				const FVector Lateral = L16.Stand - Front * Ahead;
				if (bWall)
				{
					// 15 cm devant la capsule du joueur
					const FVector Center = Lateral + Front * (Ahead - 34.0 - 15.0 - 5.0);
					SpawnBlock(GetWorld(), FVector(Center.X, Center.Y, FloorZ + 200.f), FVector(5.f, 150.f, 200.f), Yaw);
				}
				else
				{
					// Du bord de la marche immergee (108 cm devant l'accroche : 220 cm de la ligne du mur) a 100 cm derriere le joueur
					const double From = 100.0;
					const double To = Ahead + 100.0;
					const FVector Center = Lateral + Front * ((From + To) * 0.5);
					SpawnBlock(GetWorld(), FVector(Center.X, Center.Y, SlabBottom + 15.f), FVector(static_cast<float>((To - From) * 0.5), 200.f, 15.f), Yaw);
				}
				L16.Clock = Now;
				L16.Phase = 1;
				return false;
			}
			case 1:
			{
				if (Now - L16.Clock < 0.6)
				{
					return false;
				}
				C->StartClimb(E);
				L16.Track.Begin(C);
				L16.Clock = Now;
				L16.Phase = 2;
				return false;
			}
			case 2:
			{
				L16.Track.Sample(C);
				C->InputMove(FVector2D(0.f, 1.f));
				if (Now - L16.Clock < 3.0)
				{
					return false;
				}
				const float Rise = L16.Track.MaxFeet - L16.Track.StartFeet;
				const double D2 = FVector::Dist2D(C->GetActorLocation(), Anchor);
				const double Along = FVector::DotProduct(C->GetActorLocation() - Anchor, Front);
				// Mur : la capsule reste de son cote (centre a 34 cm au moins de la face du mur cote joueur, posee a 49 cm devant lui ;
				// 1 cm pres)
				const bool bSide = !bWall || Along >= Ahead - 15.0 - 1.0;
				const bool bHead = bWall || L16.Track.MaxHead <= SlabBottom + 0.5f;
				const bool bOk = C->IsClimbing() && Rise < NoLift && D2 > 40.0 && bSide && bHead && L16.Track.Inside == 0;
				Note(FString::Printf(TEXT("%s, echelle sur son trottoir (pied a %.0f cm), prise directe, avancer 3 s : pieds au plus %.1f cm au-dessus du depart (attendu : moins de %.0f ; "
					"avant, v4.15 correctifs 5 : levee jusqu'a %.0f cm), a %.0f cm de l'accroche, %s%s ; dernier obstacle : %s ; %s"), What, L16.FootZ, Rise, NoLift,
					L16.FootZ + 35.f - FloorZ, D2, bWall ? *FString::Printf(TEXT("cote du mur garde %s"), OuiNon(bSide)) : *FString::Printf(TEXT("tete au plus a %.1f cm du bas du plafond (attendu <= 0)"),
					L16.Track.MaxHead - SlabBottom), C->IsClimbing() ? TEXT("") : TEXT(", ECHELLE LACHEE"), *CompName(C->ClimbBlocker.Get()), *L16.Track.InsideText()), !bOk);
				if (!bOk)
				{
					Shot(bWall ? TEXT("v416_mur_haut") : TEXT("v416_plafond_bas"));
				}
				L16.Clock = Now;
				L16.Phase = 3;
				return false;
			}
			default:
			{
				if (C->IsClimbing() && Now - L16.Clock < 2.0)
				{
					C->InputMove(FVector2D(0.f, -1.f));
					return false;
				}
				Note(FString::Printf(TEXT("%s : puis reculer : echelle lachee %s (%.2f s, pieds a %.1f cm)"), What, OuiNon(!C->IsClimbing()), Now - L16.Clock, FeetZOf(C)), C->IsClimbing());
				EndClimbCase(C);
				return true;
			}
			}
		});
		LimitLastStep(30.f);
	};
	AddCanalObstacle(true);
	AddCanalObstacle(false);

	// Descente sur une caisse de 20 cm posee sur le trottoir sous l'accroche : lachee sur elle (BRGather::ClimbReleaseOnDescent),
	// debout dessus. Avant (v4.15 correctifs 5) : accrochee au-dessus (pieds a 20 cm du pied, plus de 4 cm), Saut ou Interagir
	Add(TEXT("v4.16 echelles : descente sur une caisse sous l'accroche"), 0.f, [this]()
	{
		using namespace BRTestV416Ladder;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		ABRExit* E = L16.bReady ? CurrentLadder(GetWorld()) : nullptr;
		if (!E || L16.Cur.Foot.Z < 30.f)
		{
			if (L16.Phase == 0)
			{
				Skip(TEXT("descente sur une caisse sous l'accroche : NON VERIFIE (aucune echelle sur un trottoir construite) ; attendu : lachee sur la caisse"));
			}
			else
			{
				// v4.16 (echelles, relecture) : echelle disparue au milieu du cas : notee (avant : la ligne "aucune echelle construite")
				Note(FString::Printf(TEXT("descente sur une caisse sous l'accroche : echelle sur son trottoir DISPARUE pendant le cas (phase %d) : verdict non rendu (attendu : lachee sur la "
					"caisse)"), L16.Phase), true);
			}
			EndClimbCase(C);
			return true;
		}
		const double Now = FPlatformTime::Seconds();
		const FVector Anchor = E->GetClimbAnchor();
		const FVector Front = LadderFrontOf(E);
		switch (L16.Phase)
		{
		case 0:
		{
			// Prise depuis le canal (la place verifiee), montee a 150 cm au-dessus du pied
			PutAt416(C, L16.Stand, LadderAimOf(E));
			L16.Clock = Now;
			L16.Phase = 1;
			return false;
		}
		case 1:
		{
			if (Now - L16.Clock < 0.6)
			{
				return false;
			}
			C->StartClimb(E);
			L16.Clock = Now;
			L16.Phase = 2;
			return false;
		}
		case 2:
		{
			if (C->IsClimbing() && FeetZOf(C) < L16.FootZ + 150.f && Now - L16.Clock < 6.0)
			{
				C->InputMove(FVector2D(0.f, 1.f));
				return false;
			}
			if (!C->IsClimbing() || FeetZOf(C) < L16.FootZ + 100.f || FVector::Dist2D(C->GetActorLocation(), Anchor) > 10.0)
			{
				Skip(FString::Printf(TEXT("descente sur une caisse sous l'accroche : NON VERIFIE, grimpeur pas a l'accroche au-dessus du trottoir (tenue %s, pieds a %.0f cm du pied)"),
					OuiNon(C->IsClimbing()), FeetZOf(C) - L16.FootZ));
				EndClimbCase(C);
				return true;
			}
			SpawnBlock(GetWorld(), FVector(Anchor.X, Anchor.Y, L16.FootZ + 10.f), FVector(30.f, 30.f, 10.f), static_cast<float>(Front.Rotation().Yaw));
			L16.Track.Begin(C);
			L16.Clock = Now;
			L16.Phase = 3;
			return false;
		}
		case 3:
		{
			L16.Track.Sample(C);
			if (C->IsClimbing() && Now - L16.Clock < 5.0)
			{
				C->InputMove(FVector2D(0.f, -1.f));
				return false;
			}
			L16.Since = Now - L16.Clock;
			L16.Value = FeetZOf(C) - L16.FootZ;
			L16.bOk = !C->IsClimbing();
			L16.Clock = Now;
			L16.Phase = 4;
			return false;
		}
		default:
		{
			if (Now - L16.Clock < 1.0)
			{
				return false;
			}
			const UCharacterMovementComponent* M = C->GetCharacterMovement();
			const bool bGround = M && M->IsMovingOnGround();
			const float Feet = FeetZOf(C) - L16.FootZ;
			const bool bOk = L16.bOk && bGround && FMath::Abs(Feet - 20.f) < 3.f && L16.Track.Inside == 0;
			Note(FString::Printf(TEXT("descente sur une caisse de 20 cm posee sur le trottoir sous l'accroche : echelle %s apres %.2f s, pieds a %.1f cm du pied au lacher ; puis debout "
				"sur la caisse %s (pieds a %.1f cm) ; %s (attendu : lachee sur la caisse, BRGather::ClimbReleaseOnDescent ; avant : accrochee au-dessus)"), L16.bOk ? TEXT("lachee")
				: TEXT("TOUJOURS TENUE (COINCEE en descente)"), L16.Since, L16.Value, OuiNon(bGround && FMath::Abs(Feet - 20.f) < 3.f), Feet, *L16.Track.InsideText()), !bOk);
			if (!bOk)
			{
				Shot(TEXT("v416_descente_caisse"));
			}
			EndClimbCase(C);
			return true;
		}
		}
	});
	LimitLastStep(30.f);

	// Montee complete depuis le canal : une demande au sommet, arrivee au Niveau 2 (seul : depart immediat)
	Add(TEXT("v4.16 echelles : montee complete depuis le canal, une demande au sommet, arrivee au Niveau 2"), 0.f, [this]()
	{
		using namespace BRTestV416Ladder;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		ABRExit* E = (L16.bReady && L16.Phase <= 2) ? CurrentLadder(GetWorld()) : L16.Exit.Get();
		if (L16.Phase == 0 && (!E || L16.Cur.Foot.Z < 30.f))
		{
			Skip(TEXT("montee complete depuis le canal : NON VERIFIE (aucune echelle sur un trottoir construite avec une place dans le canal)"));
			EndClimbCase(C);
			return true;
		}
		switch (L16.Phase)
		{
		case 0:
		{
			W->DebugCompleteMission();
			PutAt416(C, L16.Stand, LadderAimOf(E));
			L16.RequestsMark = C->ClimbRequestsSent;
			L16.Serial = W->GetLevelSerial();
			L16.Track.Begin(C);
			L16.Clock = Now;
			L16.Phase = 1;
			return false;
		}
		case 1:
		{
			if (!E)
			{
				Note(TEXT("montee complete : echelle disparue avant la prise"), true);
				EndClimbCase(C);
				return true;
			}
			LookAt416(C, LadderAimOf(E));
			if (Now - L16.Clock < 0.6)
			{
				return false;
			}
			if (C->FocusActor.Get() == E)
			{
				C->Interact();
				L16.bDirect = false;
			}
			else if (Now - L16.Clock > 3.0)
			{
				C->StartClimb(E);
				L16.bDirect = true;
			}
			else
			{
				return false;
			}
			L16.Clock = Now;
			L16.Phase = 2;
			return false;
		}
		case 2:
		{
			if (W->IsTransitioning() || W->GetLevelSerial() != L16.Serial)
			{
				L16.Clock = Now;
				L16.Phase = 3;
				return false;
			}
			L16.Track.Sample(C);
			if (!C->IsClimbing() && Now - L16.Clock > 1.0)
			{
				Note(TEXT("montee complete depuis le canal : echelle PAS PRISE (ou lachee avant le sommet)"), true);
				EndClimbCase(C);
				return true;
			}
			C->InputMove(FVector2D(0.f, 1.f));
			if (Now - L16.Clock > 20.0)
			{
				Note(FString::Printf(TEXT("montee complete depuis le canal : pas de depart apres 20 s (pieds au plus a %.0f cm du pied, sommet de la montee a %.0f cm ; attente %d, demandes %d ; "
					"dernier obstacle : %s)"), L16.Track.MaxFeet - L16.FootZ, L16.TopZ - L16.FootZ - CapsuleHalf416(C), static_cast<int32>(C->GetClimbWaitState()),
					C->ClimbRequestsSent - L16.RequestsMark, *CompName(C->ClimbBlocker.Get())), true);
				EndClimbCase(C);
				return true;
			}
			return false;
		}
		default:
		{
			const BRContent::FExitResolution Res = BRLevels::ResolveExit(37, PoolTarget);
			const bool bEnd = Res.Kind == BRContent::EExit::ChapterEnd;
			const bool bThere = bEnd ? W->IsChapterEnd() : (!W->IsTransitioning() && W->IsLevelReady() && W->GetLevelNumber() == Res.Target);
			if (!bThere && Now - L16.Clock < 40.0)
			{
				return false;
			}
			const int32 Requests = C->ClimbRequestsSent - L16.RequestsMark;
			const bool bTop = L16.Track.MaxFeet + CapsuleHalf416(C) >= L16.TopZ - 2.f;
			const bool bOk = bThere && Requests == 1 && bTop && L16.Track.Inside == 0;
			Note(FString::Printf(TEXT("echelle sur son trottoir montee depuis le canal (%s) : sommet de la montee %s, demandes de depart au sommet %d (attendu 1), %s %s ; %s"),
				L16.bDirect ? TEXT("prise directe") : TEXT("visee et touche d'interaction"), bTop ? TEXT("atteint") : TEXT("NON ATTEINT"), Requests, bEnd ? TEXT("fin du contenu disponible")
				: *FString::Printf(TEXT("arrivee au Niveau %d"), Res.Target), OuiNon(bThere), *L16.Track.InsideText()), !bOk);
			if (bEnd && W->IsChapterEnd())
			{
				W->CloseEnding(false);
			}
			L16.bReady = false;
			L16.Exit.Reset();
			EndClimbCase(C);
			return true;
		}
		}
	});
	LimitLastStep(80.f);

	// ---------------------------------------------------------------------------------------------------------------------
	// C. UpdateClimb est commun : echelle BRExit_42 du Niveau 8 (tablier incline) et echelle de l'alcove de l'aile du Level 6.
	//    Montee de 2 m (moins pres d'un sommet bas), descente jusqu'au lacher au pied (debout, jamais coince), puis reprise et
	//    lacher en hauteur (Saut) : chute au sol, jamais en l'air sans echelle
	// ---------------------------------------------------------------------------------------------------------------------
	auto AddUpDown = [this](int32 Level, uint32 Seed, int32 Target, bool bWing, const FString& Name)
	{
		AddLevelSeedLoad(Level, Seed, FString::Printf(TEXT("v4.16 echelles : Niveau %d (graine du niveau %u, partie classique)"), Level, Seed));
		Add(FString::Printf(TEXT("v4.16 echelles : %s, montee, descente, lacher en hauteur"), *Name), 0.f, [this, Level, Seed, Target, bWing, Name]()
		{
			using namespace BRTestV416Ladder;
			ABRWorld* W = GetBRWorld();
			ABRCharacter* C = GetPlayer();
			if (!W || !C)
			{
				return false;
			}
			const double Now = FPlatformTime::Seconds();
			const float Half = CapsuleHalf416(C);
			ABRExit* E = L16.Exit.Get();
			auto Done416 = [&]()
			{
				if (L16.bFlying)
				{
					C->SetDevFly(false);
					L16.bFlying = false;
				}
				if (L16.Phase > 0)
				{
					C->bGodMode = L16.bGodWas;
				}
				EndClimbCase(C);
				L16.Exit.Reset();
				return true;
			};
			// v4.16 (echelles, correctifs) : niveau quitte pendant le cas (transition, ou numero de niveau change) : le sommet de la
			// montee atteint envoie la demande de depart (BRGather::FClimbWait), seul le depart est immediat (ABRExit::FinishClimb,
			// ABRWorld::RequestDeparture) et l'echelle part avec le niveau. Avant (v416m1, Level 6) : le cas continuait jusqu'a la
			// destruction de l'echelle et notait "disparue pendant la montee" (PROBLEME), sans dire que le niveau etait quitte. Jugement
			// en phase 7, apres l'arrivee
			if (L16.Phase >= 1 && L16.Phase < 7 && (W->IsTransitioning() || W->GetLevelSerial() != L16.Serial))
			{
				L16.LeftPhase = L16.Phase;
				L16.Clock = Now;
				L16.Phase = 7;
				return false;
			}
			switch (L16.Phase)
			{
			case 0:
			{
				if (W->IsTransitioning() || !W->IsLevelReady() || W->GetLevelNumber() != Level || W->GetSeed() != Seed || W->IsLinearLevel())
				{
					Skip(FString::Printf(TEXT("Niveau %d (graine du niveau %u, partie classique) non charge (Niveau %d, graine %u, lineaire %s) : %s NON VERIFIEE"), Level, Seed,
						W->GetLevelNumber(), W->GetSeed(), OuiNon(W->IsLinearLevel()), *Name));
					return Done416();
				}
				// Ou chercher l'echelle : place connue (Niveau 8) ou alcove de l'aile (Level 6, sortie de la mission, cachee tant que son
				// chunk n'est pas construit) ; vol du developpeur au-dessus d'elle le temps que son chunk se construise
				FVector Spot(Level8X, Level8Y, 0.0);
				if (bWing && !WingLadderSpot(W, Spot))
				{
					Skip(FString::Printf(TEXT("Level 6 (graine du niveau %u) : aile invalide : %s NON VERIFIEE"), Seed, *Name));
					return Done416();
				}
				E = ShownLadderNear(GetWorld(), Spot, Target, bWing ? 400.0 : 50.0);
				if (!E)
				{
					if (!L16.bFlying && !W->IsChunkLoaded(W->CellToChunk(W->WorldToCell(Spot))))
					{
						C->SetDevFly(true);
						L16.bFlying = true;
						C->SetActorLocation(Spot + FVector(0.f, 0.f, 300.f), false, nullptr, ETeleportType::TeleportPhysics);
					}
					if (StepTime < 30.f)
					{
						return false;
					}
					Skip(FString::Printf(TEXT("Niveau %d : NON VERIFIE, aucune echelle vers le Niveau %d a %.0f cm de (%.0f, %.0f) en 30 s : %s non jugee"), Level, Target, bWing ? 400.0 : 50.0,
						Spot.X, Spot.Y, *Name));
					return Done416();
				}
				if (L16.bFlying)
				{
					C->SetDevFly(false);
					L16.bFlying = false;
				}
				FVector Stand = FVector::ZeroVector;
				if (!FrontStand(W, C, E, Stand))
				{
					Skip(FString::Printf(TEXT("Niveau %d : %s (%s) : NON VERIFIE, aucune place libre a 70-120 cm devant son accroche"), Level, *Name, *E->GetName()));
					return Done416();
				}
				L16.Exit = E;
				L16.FootZ = static_cast<float>(E->GetActorLocation().Z);
				L16.TopZ = E->GetClimbTopZ();
				// Pieds au plus a 2 m du pied, TopMargin sous le sommet de la montee (aucune demande de depart). v4.16 (echelles,
				// correctifs) : Half est la capsule elle-meme (avant : 0 cm dans l'image de la fin du vol du developpeur, des pieds vises
				// a 136 cm au Level 6 au lieu de 48 : sommet atteint, depart vers le Niveau 8, "echelle disparue pendant la montee")
				L16.Value = FMath::Min(200.f, L16.TopZ - Half - TopMargin - L16.FootZ);
				if (L16.Value < 80.f)
				{
					Skip(FString::Printf(TEXT("Niveau %d : %s (%s) : NON VERIFIE, sommet de la montee a %.0f cm du pied seulement"), Level, *Name, *E->GetName(), L16.TopZ - L16.FootZ));
					return Done416();
				}
				// v4.16 (echelles, correctifs) : niveau et demandes de depart d'avant la prise (un depart pendant le cas se voit, phase 7)
				L16.Serial = W->GetLevelSerial();
				L16.RequestsMark = C->ClimbRequestsSent;
				L16.LeftPhase = 0;
				// La sortie est gardee par la mission du niveau (resolue ici : seule l'echelle est jugee) ; une frayeur bloquerait les
				// entrees du grimpeur : entites proches ecartees (hote)
				W->DebugCompleteMission();
				L16.Cleared = DismissNear(GetWorld(), E->GetActorLocation(), 3000.f);
				L16.bGodWas = C->bGodMode;
				C->bGodMode = true;
				PutAt416(C, Stand, LadderAimOf(E));
				L16.Clock = Now;
				L16.bDirect = false;
				L16.Phase = 1;
				return false;
			}
			case 1:
			{
				if (!E)
				{
					Note(FString::Printf(TEXT("Niveau %d : %s disparue"), Level, *Name), true);
					return Done416();
				}
				LookAt416(C, LadderAimOf(E));
				if (Now - L16.Clock < 0.6)
				{
					return false;
				}
				if (C->FocusActor.Get() == E)
				{
					C->Interact();
				}
				else if (Now - L16.Clock > 3.0)
				{
					L16.bDirect = true;
					C->StartClimb(E);
				}
				else
				{
					return false;
				}
				L16.Track.Begin(C);
				L16.Clock = Now;
				L16.Phase = 2;
				return false;
			}
			case 2:
			{
				// Montee jusqu'aux pieds a FootZ + Value
				if (!E)
				{
					Note(FString::Printf(TEXT("Niveau %d : %s disparue pendant la montee"), Level, *Name), true);
					return Done416();
				}
				L16.Track.Sample(C);
				if (!C->IsClimbing() && Now - L16.Clock > 1.0)
				{
					Note(FString::Printf(TEXT("Niveau %d : %s PAS PRISE (%s)"), Level, *Name, L16.bDirect ? TEXT("prise directe") : TEXT("visee et touche d'interaction")), true);
					return Done416();
				}
				if (FeetZOf(C) < L16.FootZ + L16.Value && Now - L16.Clock < 8.0)
				{
					C->InputMove(FVector2D(0.f, 1.f));
					return false;
				}
				L16.Climbed = FeetZOf(C) - L16.FootZ;
				L16.Clock = Now;
				L16.Phase = 3;
				return false;
			}
			case 3:
			{
				// Descente jusqu'au lacher (StopClimb a 4 cm du pied, ou BRGather::ClimbReleaseOnDescent), jamais coince
				L16.Track.Sample(C);
				if (C->IsClimbing() && Now - L16.Clock < 8.0)
				{
					C->InputMove(FVector2D(0.f, -1.f));
					return false;
				}
				L16.Since = Now - L16.Clock;
				L16.bOk = !C->IsClimbing();
				L16.Clock = Now;
				L16.Phase = 4;
				return false;
			}
			case 4:
			{
				if (Now - L16.Clock < 1.5)
				{
					return false;
				}
				const UCharacterMovementComponent* M = C->GetCharacterMovement();
				const bool bGround = M && M->IsMovingOnGround();
				const bool bOk = L16.bOk && bGround && L16.Climbed >= L16.Value - 10.f && L16.Track.Inside == 0;
				// v4.16 (echelles, relecture) : GetNameSafe : l'echelle (pointeur faible) a pu disparaitre pendant la descente (avant :
				// *E->GetName() avant le test de E, plantage au lieu d'une ligne)
				Note(FString::Printf(TEXT("Niveau %d (graine du niveau %u, partie classique), %s (%s, %s ; %d entite(s) proche(s) ecartee(s)) : montee jusqu'a %.0f cm au-dessus du pied (attendu "
					"%.0f), puis descente : echelle lachee %s apres %.2f s, au sol %s (pieds a %.1f cm du pied) ; %s (attendu : lachee au pied, au sol, jamais coincee)"), Level, Seed, *Name,
					*GetNameSafe(E), L16.bDirect ? TEXT("prise directe") : TEXT("visee et touche d'interaction"), L16.Cleared, L16.Climbed, L16.Value, L16.bOk ? TEXT("oui")
					: TEXT("NON (COINCEE en descente)"), L16.Since, OuiNon(bGround), FeetZOf(C) - L16.FootZ, *L16.Track.InsideText()), !bOk);
				if (!bOk)
				{
					Shot(FString::Printf(TEXT("v416_niveau%d_descente"), Level));
				}
				if (!E)
				{
					Note(FString::Printf(TEXT("Niveau %d : %s DISPARUE apres la descente : reprise au pied et lacher en hauteur (Saut) non joues"), Level, *Name), true);
					return Done416();
				}
				if (!L16.bOk || !bGround)
				{
					// v4.16 (echelles, relecture) : la suite de l'etape n'est pas jouee : notee (avant : sans ligne)
					Skip(FString::Printf(TEXT("Niveau %d : %s : reprise au pied et lacher en hauteur (Saut) NON VERIFIES, descente en echec (voir la ligne precedente)"), Level, *Name));
					return Done416();
				}
				// Reprise au pied (prise directe : la visee est jugee plus haut), montee a 120 cm, puis Saut
				L16.GroundFeet = FeetZOf(C);
				C->StartClimb(E);
				L16.Track.Begin(C);
				L16.Clock = Now;
				L16.Phase = 5;
				return false;
			}
			case 5:
			{
				L16.Track.Sample(C);
				const float Goal = FMath::Min(120.f, L16.Value);
				if (C->IsClimbing() && FeetZOf(C) < L16.FootZ + Goal && Now - L16.Clock < 5.0)
				{
					C->InputMove(FVector2D(0.f, 1.f));
					return false;
				}
				if (!C->IsClimbing())
				{
					Note(FString::Printf(TEXT("Niveau %d : %s : reprise au pied PAS PRISE ou lachee avant %.0f cm"), Level, *Name, Goal), true);
					return Done416();
				}
				L16.Since = FeetZOf(C) - L16.FootZ;
				C->InputJump(true);
				C->InputJump(false);
				L16.Clock = Now;
				L16.Phase = 6;
				return false;
			}
			case 7:
			{
				// v4.16 (echelles, correctifs) : niveau quitte pendant le cas. Une demande au sommet de la montee puis l'arrivee a la
				// destination : le depart attendu au sommet (pas une echelle disparue, pas un PROBLEME du jeu) ; la descente et le lacher
				// en hauteur ne sont pas joues : NON VERIFIES. Un depart sans une demande au sommet : PROBLEME
				const BRContent::FExitResolution Res = BRLevels::ResolveExit(Level, Target);
				const bool bEnd = Res.Kind == BRContent::EExit::ChapterEnd;
				const bool bThere = bEnd ? W->IsChapterEnd()
					: (!W->IsTransitioning() && W->IsLevelReady() && W->GetLevelSerial() != L16.Serial && W->GetLevelNumber() == Res.Target);
				if (!bThere && Now - L16.Clock < 40.0)
				{
					return false;
				}
				const int32 Requests = C->ClimbRequestsSent - L16.RequestsMark;
				const bool bTop = L16.Track.MaxFeet + Half >= L16.TopZ - 2.f;
				const bool bExpected = Requests == 1 && bTop;
				Note(FString::Printf(TEXT("Niveau %d : %s : niveau quitte pendant le cas (phase %d) : sommet de la montee %s (pieds au plus a %.0f cm du pied, vises %.0f, au sommet a %.0f), "
					"demandes de depart au sommet %d (attendu 1), %s %s ; %s"), Level, *Name, L16.LeftPhase, bTop ? TEXT("atteint") : TEXT("NON ATTEINT"), L16.Track.MaxFeet - L16.FootZ,
					L16.Value, L16.TopZ - Half - L16.FootZ, Requests, bEnd ? TEXT("fin du contenu disponible") : *FString::Printf(TEXT("arrivee au Niveau %d"), Res.Target), OuiNon(bThere),
					bExpected ? TEXT("depart attendu au sommet (seul : depart immediat, ABRExit::FinishClimb), l'echelle part avec le niveau")
					: TEXT("DEPART SANS UNE DEMANDE AU SOMMET DE LA MONTEE")), !bExpected || !bThere);
				Skip(FString::Printf(TEXT("Niveau %d : %s : descente jusqu'au lacher au pied et lacher en hauteur (Saut) NON VERIFIES (niveau quitte pendant le cas)"), Level, *Name));
				if (bEnd && W->IsChapterEnd())
				{
					W->CloseEnding(false);
				}
				return Done416();
			}
			default:
			{
				// Lachee en hauteur : au sol vite (chute libre de 120 cm : 0,5 s), pieds revenus au sol de la descente
				const UCharacterMovementComponent* M = C->GetCharacterMovement();
				const bool bGround = M && M->IsMovingOnGround();
				if ((C->IsClimbing() || !bGround) && Now - L16.Clock < 3.0)
				{
					return false;
				}
				const float Fall = static_cast<float>(Now - L16.Clock);
				const float Back = FeetZOf(C) - L16.GroundFeet;
				const bool bOk = !C->IsClimbing() && bGround && Fall < 1.5f && FMath::Abs(Back) < 15.f && L16.Track.Inside == 0;
				Note(FString::Printf(TEXT("Niveau %d : %s reprise au pied, lachee (Saut) les pieds a %.0f cm du pied : echelle tenue %s, au sol %s apres %.2f s (attendu : moins de 1,5 s), pieds "
					"a %.1f cm du sol de la descente ; %s (attendu : chute au sol, jamais en l'air sans echelle)"), Level, *Name, L16.Since, OuiNon(C->IsClimbing()), OuiNon(bGround), Fall, Back,
					*L16.Track.InsideText()), !bOk);
				if (!bOk)
				{
					Shot(FString::Printf(TEXT("v416_niveau%d_lacher"), Level));
				}
				return Done416();
			}
			}
		});
		// v4.16 (echelles, correctifs) : 40 s de plus pour l'arrivee apres un depart pendant le cas (phase 7)
		LimitLastStep(115.f);
	};
	AddUpDown(8, BRTestV416Ladder::Level8Seed, BRTestV416Ladder::Level8Target, false, TEXT("echelle BRExit_42 du Niveau 8 (tablier incline)"));
	AddUpDown(6, BRTestV416Ladder::Level6Seed, BRTestV416Ladder::Level6Target, true, TEXT("echelle de l'alcove de l'aile du Level 6"));
	Add(TEXT("v4.16 echelles : retour a la regle normale du parcours"), 0.f, [this]()
	{
		if (ABRWorld* W = GetBRWorld())
		{
			W->SetLinearTestOverride(-1);
		}
		if (ABRCharacter* C = GetPlayer())
		{
			if (C->IsDevFlying())
			{
				C->SetDevFly(false);
			}
			BRTestV416Ladder::EndClimbCase(C);
		}
		BRTestV416Ladder::L16.Exit.Reset();
		return true;
	});
}

// =====================================================================================================================
// v4.16 (echelles) : reseau (-BRNetTest, apres les etapes des correctifs 5 de la v4.15) : Niveau 37 en partie classique a
// la graine du niveau 115083815 (son chunk (-1, -1), construit au depart, porte une echelle vers le Niveau 2 sur son trottoir).
// Meme echelle chez l'hote et le client (empreinte envoyee par le client), le client a 9-12 m du point de rassemblement, l'hote
// grimpe : une seule demande au sommet, attente stable, 1/2 (le depart attend le coequipier), puis le client a pied :
// rassemblement 2/2, depart (phase 2), arrivee au Niveau 2 chez chacun ; aucune autre demande pendant l'attente
// =====================================================================================================================

void ABRAutoTest::AddNetV416LadderSteps(bool bClient)
{
	// v4.16 (echelles, relecture) : toujours dans le plan reseau (avant : seulement avec -BRAutoTestV416Ladder). Le test reseau
	// ordinaire juge ainsi les echelles des chunks construits, que les etapes des correctifs 5 (graine du niveau 2303284074 : aucune
	// dans les chunks construits au depart) ne peuvent pas juger, et le depart de groupe par une echelle des chunks du Niveau 37
	Add(TEXT("v4.16 echelles reseau : Niveau 37 en partie classique (graine du niveau 115083815)"), 0.f, [this, bClient]()
	{
		using namespace BRTestV416Ladder;
		ABRWorld* W = GetBRWorld();
		if (!W || W->IsTransitioning())
		{
			return false;
		}
		L16 = FLadderTest();
		L16.Serial = W->GetLevelSerial();
		L16.bNetOk = true;
		if (bClient)
		{
			// 3 s d'abord : l'hote lit encore l'empreinte que le client vient d'envoyer a l'etape precedente (v4.15 correctifs 5,
			// MarkNet37Hash) ; une seule marque est lue a la fois, la suivante trop tot l'effacerait
			if (StepTime < 3.f)
			{
				return false;
			}
			BRTestNet::SendMark(GetPC(), MarkStart);
			return true;
		}
		const int32 Mark = BRTestNet::RemoteMark(W);
		if (Mark != MarkStart && Mark != BRTestNet::MarkFinished && BRTestNet::RemoteMate(W))
		{
			if (StepTime < 90.f)
			{
				return false;
			}
			Note(FString::Printf(TEXT("hote : le client n'est pas arrive a l'etape v4.16 des echelles en 90 s (derniere etape signalee %d) : niveau change quand meme"), Mark), true);
		}
		L16.Serial = W->GetLevelSerial();
		W->SetLinearTestOverride(0);
		W->RequestTransition(37, false, Pool37Seed);
		return true;
	});
	LimitLastStep(100.f);
	Add(TEXT("v4.16 echelles reseau : Niveau 37 et echelle des chunks chez chacun"), 0.5f, [this, bClient]()
	{
		using namespace BRTestV416Ladder;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		const TCHAR* Who = bClient ? TEXT("client") : TEXT("hote");
		const bool bThere = W->IsLevelReady() && !W->IsTransitioning() && W->GetLevelNumber() == 37 && W->GetLevelSerial() != L16.Serial;
		if ((!bThere || StepTime < 3.f) && StepTime < 120.f)
		{
			return false;
		}
		if (!bThere || W->GetSeed() != Pool37Seed || W->IsLinearLevel())
		{
			Note(FString::Printf(TEXT("%s : Niveau 37 a la graine du niveau %u en partie classique %s (Niveau %d, graine %u, lineaire %s)"), Who, Pool37Seed, bThere ? TEXT("NON (autre graine ou parcours)")
				: TEXT("NON charge en 120 s"), W->GetLevelNumber(), W->GetSeed(), OuiNon(W->IsLinearLevel())), true);
			L16.bNetOk = false;
			if (bClient)
			{
				BRTestNet::SendMark(GetPC(), MarkNoLevel);
			}
			return true;
		}
		C->bGodMode = true;
		// L'echelle vers le Niveau 2 des chunks construits la plus proche de (-1250, -1560) : celle du chunk (-1, -1) (banc), 30 s au plus
		TArray<ABRExit*> Built;
		for (TActorIterator<ABRExit> It(GetWorld()); It; ++It)
		{
			if (It->Target == PoolTarget && It->IsClimbable() && Cast<ABRChunk>(It->GetOwner()))
			{
				Built.Add(*It);
			}
		}
		if (Built.Num() == 0 && StepTime < 33.f)
		{
			return false;
		}
		if (Built.Num() == 0)
		{
			Skip(FString::Printf(TEXT("%s : Niveau 37 (graine du niveau %u) : NON VERIFIE, aucune echelle vers le Niveau 2 dans les chunks construits en 30 s : depart de groupe par une "
				"echelle des chunks non juge"), Who, Pool37Seed));
			L16.bNetOk = false;
			if (bClient)
			{
				BRTestNet::SendMark(GetPC(), MarkNoPlace);
			}
			return true;
		}
		const FVector Bench(NetLadderX, NetLadderY, 0.0);
		ABRExit* E = Built[0];
		for (ABRExit* B : Built)
		{
			E = FVector::Dist2D(B->GetActorLocation(), Bench) < FVector::Dist2D(E->GetActorLocation(), Bench) ? B : E;
		}
		const FString View = LadderTaken(GetWorld(), E, false);
		const FString Climb = LadderTaken(GetWorld(), E, true);
		const FVector L = E->GetActorLocation();
		const FString Text = FString::Printf(TEXT("(%.0f, %.0f, %.0f)"), L.X, L.Y, L.Z);
		// v4.16 : chaque echelle vers le Niveau 2 des chunks construits est posee par le tirage de la regle du jeu (meme pied, 1 cm
		// pres) avec son point vise et la capsule du grimpeur libres ; v4.15 correctifs 5 (graine du niveau 2303284074) : aucune
		// echelle dans les chunks construits au depart, NON VERIFIE (net_final4). L'empreinte envoyee couvre le tirage et l'echelle
		FString Drawn;
		const TArray<FVector> Feet = DrawnFeet(W, Drawn);
		int32 Outside = 0;
		int32 Taken = 0;
		for (const ABRExit* B : Built)
		{
			bool bAt = false;
			for (const FVector& F : Feet)
			{
				bAt = bAt || FVector::Dist2D(F, B->GetActorLocation()) < 1.0;
			}
			Outside += bAt ? 0 : 1;
			Taken += (LadderTaken(GetWorld(), B, false) == TEXT("libre") && LadderTaken(GetWorld(), B, true) == TEXT("libre")) ? 0 : 1;
		}
		L16.Hash = FCrc::StrCrc32(*(Drawn + TEXT(" | ") + Text));
		Note(FString::Printf(TEXT("%s : Niveau 37 (graine du niveau %u) : %d echelle(s) vers le Niveau 2 tirees (chunks -%d..%d) ; %d dans les chunks construits, hors du tirage %d (attendu 0), "
			"point vise ou capsule du grimpeur pris %d (attendu 0)"), Who, Pool37Seed, Feet.Num(), PlanRange, PlanRange, Built.Num(), Outside, Taken), Feet.Num() == 0 || Outside > 0 || Taken > 0);
		L16.Exit = E;
		L16.FootZ = static_cast<float>(L.Z);
		L16.TopZ = E->GetClimbTopZ();
		// v4.16 (reseau, correctifs) : chargement du Niveau 37 retenu (comme SerialMark des etapes reseau des correctifs 4) : le
		// depart de groupe (bLeft) et l'arrivee au Niveau 2 se jugent par rapport a lui. Avant : chargement d'avant le Niveau 37,
		// donc "deja parti" des la premiere image du rassemblement : l'hote laissait l'echelle 0,5 s apres l'avoir prise (centre a
		// 90 cm du pied, 0 demande) et le client concluait au depart sans l'avoir vu (net416_classic)
		L16.Serial = W->GetLevelSerial();
		const bool bFree = View == TEXT("libre") && Climb == TEXT("libre");
		Note(FString::Printf(TEXT("%s : Niveau 37 (graine du niveau %u, partie classique) : %d echelle(s) vers le Niveau 2 dans les chunks construits ; celle du depart de groupe : %s en %s "
			"(banc : (-1250, -1560, 68) %s), sur son trottoir %s, sommet de la montee a %.0f cm du pied ; point vise %s, capsule du grimpeur %s"), Who, Pool37Seed, Built.Num(), *E->GetName(),
			*Text, OuiNon(FVector::Dist(L, FVector(NetLadderX, NetLadderY, 68.0)) < 1.0), OuiNon(L.Z >= 30.0), L16.TopZ - L16.FootZ, *View, *Climb), !bFree);
		L16.bNetOk = bFree;
		return true;
	});
	LimitLastStep(130.f);
	// Le client se place loin (9-12 m du point de rassemblement, meme etage, a vue ; a defaut, il reste a sa place s'il est a plus
	// de 8 m), puis signale sa place avec l'empreinte de son echelle (une seule marque : MarkHash + empreinte) ; l'hote compare
	Add(TEXT("v4.16 echelles reseau : meme echelle chez chacun, client loin du point de rassemblement"), 0.f, [this, bClient]()
	{
		using namespace BRTestV416Ladder;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		ABRExit* E = L16.Exit.Get();
		if (bClient)
		{
			if (!L16.bNetOk || !E)
			{
				if (L16.bNetOk)
				{
					Note(TEXT("client : echelle du depart de groupe disparue"), true);
				}
				L16.bNetOk = false;
				BRTestNet::SendMark(GetPC(), MarkNoPlace);
				return true;
			}
			const FVector Gather = E->GetActorLocation() + LadderFrontOf(E) * 100.f;
			if (L16.Phase == 0)
			{
				// Jamais dans l'image qui suit une capture (saccade : le deplacement pourrait etre defait)
				if (StepTime < 0.3f)
				{
					return false;
				}
				FVector Spot;
				if (FarStand416(GetWorld(), W, C, E, Spot))
				{
					const FVector Look = ABRWorld::DepartureViewPoint(E);
					L16.Place.Start(C, Spot, &Look, Now);
					L16.Phase = 1;
					return false;
				}
				if (FVector::Dist2D(C->GetActorLocation(), Gather) < BRGather::Radius + 100.f)
				{
					Skip(TEXT("client : aucune place a 9-12 m du point de rassemblement de l'echelle (meme etage, a vue) et deja a moins de 9 m : rassemblement a pied non verifie"));
					L16.bNetOk = false;
					BRTestNet::SendMark(GetPC(), MarkNoPlace);
					return true;
				}
				L16.Place.Start(C, C->GetActorLocation(), nullptr, Now);
				L16.Phase = 1;
				return false;
			}
			if (!L16.Place.Update(C, Now, 0.6) && StepTime < 15.f)
			{
				return false;
			}
			Note(FString::Printf(TEXT("client : place a %.1f m du point de rassemblement de l'echelle (attendu : plus de 8 m), %d replacement(s)"), FVector::Dist2D(C->GetActorLocation(), Gather)
				/ 100.f, L16.Place.Reapplied));
			BRTestNet::SendMark(GetPC(), MarkHash + static_cast<int32>(L16.Hash % 100000u));
			L16.Phase = 0;
			return true;
		}
		if (!L16.bNetOk || !E)
		{
			return true;
		}
		const int32 Mark = BRTestNet::RemoteMark(W);
		if (!BRTestNet::RemoteMate(W))
		{
			Note(TEXT("hote : aucun client pour le depart de groupe par l'echelle des chunks"), true);
			L16.bNetOk = false;
			return true;
		}
		if (Mark == MarkNoPlace || Mark == MarkNoLevel)
		{
			Skip(TEXT("hote : le client n'a pas son echelle ou sa place (voir son rapport) : depart de groupe par l'echelle des chunks non verifie"));
			L16.bNetOk = false;
			return true;
		}
		if (Mark < MarkHash || Mark >= MarkHash + 100000)
		{
			if (StepTime < 60.f)
			{
				return false;
			}
			Note(FString::Printf(TEXT("hote : le client n'a pas signale sa place en 60 s (derniere etape signalee %d)"), Mark), true);
			L16.bNetOk = false;
			return true;
		}
		const int32 Mine = MarkHash + static_cast<int32>(L16.Hash % 100000u);
		Note(FString::Printf(TEXT("hote : client place ; tirage des echelles et echelle du depart de groupe identiques chez le client (empreinte %d, hote %d) : %s"), Mark - MarkHash, Mine - MarkHash,
			OuiNon(Mark == Mine)), Mark != Mine);
		L16.Phase = 0;
		return true;
	});
	LimitLastStep(75.f);
	// L'hote grimpe depuis son trottoir : une demande au sommet, attente stable 4 s, 1/2 (le depart attend le client) ; le client,
	// une fois le depart en rassemblement vu, marche jusqu'au point de rassemblement ; depart 2/2, une seule demande en tout
	Add(TEXT("v4.16 echelles reseau : hote au sommet, client a pied, rassemblement 2/2 puis depart"), 0.f, [this, bClient]()
	{
		using namespace BRTestV416Ladder;
		ABRWorld* W = GetBRWorld();
		ABRCharacter* C = GetPlayer();
		if (!W || !C)
		{
			return false;
		}
		if (!L16.bNetOk)
		{
			return true;
		}
		const double Now = FPlatformTime::Seconds();
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		const FBRNetDeparture& Dep = W->GetDeparture();
		const bool bLeft = W->IsTransitioning() || W->GetLevelSerial() != L16.Serial;
		if (Dep.Target == PoolTarget && (Dep.Phase == 1 || Dep.Phase == 2))
		{
			L16.bDepStarted = true;
			L16.MaxReady = FMath::Max(L16.MaxReady, Dep.Ready);
			L16.MaxNeeded = FMath::Max(L16.MaxNeeded, Dep.Needed);
			L16.bDepPhase2 = L16.bDepPhase2 || Dep.Phase == 2;
		}
		if (L16.bDepStarted && Dep.Phase == 3 && !L16.bDepCancelled)
		{
			L16.bDepCancelled = true;
			L16.DepReason = Dep.Reason;
		}
		if (bClient)
		{
			if (bLeft)
			{
				Note(FString::Printf(TEXT("client : depart de groupe par l'echelle des chunks lance (depart vu %s, rassembles au plus %d/%d, phase 2 recue %s ; marche : %s)"),
					OuiNon(L16.bDepStarted), L16.MaxReady, L16.MaxNeeded, OuiNon(L16.bDepPhase2), L16.Phase == 1 ? *BRTestV412::WalkStats() : TEXT("pas commencee")), !L16.bDepStarted);
				BRTestV412::ReleaseJump(C);
				L16.Phase = 0;
				return true;
			}
			if (L16.bDepCancelled)
			{
				Note(FString::Printf(TEXT("client : depart de groupe par l'echelle des chunks ANNULE (raison %d ; 1 : fin du delai), rassembles au plus %d/%d"), L16.DepReason, L16.MaxReady,
					L16.MaxNeeded), true);
				BRTestV412::ReleaseJump(C);
				L16.bNetOk = false;
				L16.Phase = 0;
				return true;
			}
			if (L16.Phase == 0)
			{
				// Depart en rassemblement depuis 6 s au moins, grimpeur compte (comme le test reseau v4.12) : marche vers le point
				if (!(Dep.Phase == 1 && Dep.Target == PoolTarget && Dep.Ready >= 1 && W->GetDepartureRemaining() <= 84.f))
				{
					if (StepTime > 90.f)
					{
						Note(FString::Printf(TEXT("client : depart pas pret pour la marche en 90 s (phase %d, %d/%d rassembles, %.0f s restantes)"), static_cast<int32>(Dep.Phase), Dep.Ready,
							Dep.Needed, W->GetDepartureRemaining()), true);
						L16.bNetOk = false;
						return true;
					}
					L16.Place.Update(C, Now, 0.6);
					return false;
				}
				const FVector Loc = Dep.Location;
				BRTestV412::BeginWalk(W, C, FVector(Loc.X, Loc.Y, C->GetActorLocation().Z), 250.f, true);
				Note(FString::Printf(TEXT("client : marche vers le point de rassemblement depuis (%.0f, %.0f, %.0f), a %.1f m, depart : %d/%d rassembles, %.0f s restantes"),
					C->GetActorLocation().X, C->GetActorLocation().Y, C->GetActorLocation().Z, FVector::Dist2D(C->GetActorLocation(), Loc) / 100.f, Dep.Ready, Dep.Needed,
					W->GetDepartureRemaining()));
				L16.Clock = Now;
				L16.Phase = 1;
				return false;
			}
			const BRTestV412::EWalk R = BRTestV412::StepWalk(W, C, Dt, 80.0, C->GetScareKind() >= 0 || C->IsInputLocked());
			if (Now - L16.Clock > 100.0)
			{
				Note(FString::Printf(TEXT("client : depart pas parti 100 s apres le debut de la marche (phase %d, %d/%d, marche %d : %s)"), static_cast<int32>(Dep.Phase), Dep.Ready, Dep.Needed,
					static_cast<int32>(R), *BRTestV412::WalkStats()), true);
				BRTestV412::ReleaseJump(C);
				L16.bNetOk = false;
				L16.Phase = 0;
				return true;
			}
			return false;
		}
		// Hote
		ABRExit* E = L16.Exit.Get();
		if (!E && L16.Phase <= 2)
		{
			Note(TEXT("hote : echelle des chunks disparue avant la montee"), true);
			L16.bNetOk = false;
			L16.Phase = 0;
			return true;
		}
		// Ce que l'hote voit du client (comptes du rassemblement, changements d'etat notes)
		if (const ABRCharacter* M = BRTestNet::RemoteMate(W))
		{
			if (Dep.Phase == 1)
			{
				const bool bCounted = W->IsGatheredForDeparture(M);
				const FString State = bCounted ? FString(TEXT("compte comme rassemble")) : FString(TEXT("pas encore rassemble"));
				if (State != L16.GatherState)
				{
					if (L16.GatherChanges < 8)
					{
						Note(FString::Printf(TEXT("hote : client %s (a %.1f m du point, %d/%d rassembles, %.0f s restantes), a %.1f s"), *State, FVector::Dist2D(M->GetActorLocation(), Dep.Location)
							/ 100.f, Dep.Ready, Dep.Needed, W->GetDepartureRemaining(), StepTime));
					}
					++L16.GatherChanges;
					L16.GatherState = State;
				}
			}
		}
		switch (L16.Phase)
		{
		case 0:
		{
			// Depuis son trottoir, juste devant l'accroche (placement direct : l'approche a pied est jugee en solo, v4.15 correctifs 5)
			W->DebugCompleteMission();
			const float Half = CapsuleHalf416(C);
			PutAt416(C, E->GetClimbAnchor() + LadderFrontOf(E) * BRMech::Clearance::PlayerRadius + FVector(0.f, 0.f, Half + 5.f), LadderAimOf(E));
			L16.RequestsMark = C->ClimbRequestsSent;
			L16.Clock = Now;
			L16.Phase = 1;
			return false;
		}
		case 1:
		{
			LookAt416(C, LadderAimOf(E));
			if (Now - L16.Clock < 0.4)
			{
				return false;
			}
			if (C->FocusActor.Get() != E)
			{
				if (Now - L16.Clock < 3.0)
				{
					return false;
				}
				Note(FString::Printf(TEXT("hote : echelle des chunks PAS VISEE depuis son trottoir (vise : %s)"), *BRTestV412::FocusName(W, C->FocusActor.Get())), true);
				L16.bNetOk = false;
				L16.Phase = 0;
				return true;
			}
			C->Interact();
			L16.Track.Begin(C);
			L16.Clock = Now;
			L16.Phase = 2;
			return false;
		}
		case 2:
		{
			if (bLeft)
			{
				L16.Clock = Now;
				L16.Phase = 4;
				return false;
			}
			L16.Track.Sample(C);
			if (!C->IsClimbing())
			{
				if (Now - L16.Clock < 1.0)
				{
					return false;
				}
				Note(TEXT("hote : echelle des chunks visee mais PAS PRISE"), true);
				L16.bNetOk = false;
				L16.Phase = 0;
				return true;
			}
			if (C->GetClimbWaitState() == BRGather::FClimbWait::EState::Climbing)
			{
				C->InputMove(FVector2D(0.f, 1.f));
				if (Now - L16.Clock > 20.0)
				{
					Note(FString::Printf(TEXT("hote : sommet de l'echelle des chunks non atteint en 20 s (pieds au plus a %.0f cm du pied ; dernier obstacle : %s)"), L16.Track.MaxFeet - L16.FootZ,
						*CompName(C->ClimbBlocker.Get())), true);
					L16.bNetOk = false;
					L16.Phase = 0;
					return true;
				}
				return false;
			}
			// Au sommet : 4 s sans toucher a rien ; une demande, attente stable, l'hote compte, le client pas encore (1/2)
			L16.Clock = Now;
			L16.Phase = 3;
			return false;
		}
		case 3:
		{
			if (Now - L16.Clock < 4.0 && !bLeft)
			{
				return false;
			}
			const int32 Requests = C->ClimbRequestsSent - L16.RequestsMark;
			const bool bGathered = W->IsGatheredForDeparture(C);
			const bool bWaiting = C->GetClimbWaitState() == BRGather::FClimbWait::EState::Waiting;
			const bool bOk = !bLeft && Requests == 1 && bWaiting && Dep.Phase == 1 && bGathered && Dep.Ready < Dep.Needed;
			Note(FString::Printf(TEXT("hote au sommet de l'echelle des chunks : %d demande(s) en 4 s (attendu 1), attente stable %s, grimpeur compte comme rassemble %s, %d/%d rassembles "
				"(attendu 1/2 : le depart attend le coequipier)%s"), Requests, OuiNon(bWaiting), OuiNon(bGathered), Dep.Ready, Dep.Needed, bLeft ? TEXT(", DEJA PARTI sans le client") : TEXT("")),
				!bOk);
			Shot(TEXT("v416_reseau_sommet_echelle_chunk"));
			L16.Clock = Now;
			L16.Phase = bLeft ? 4 : 5;
			return false;
		}
		case 5:
		{
			// Attente du client (le rassemblement dure 90 s au plus) ; aucune autre demande
			if (!bLeft && !L16.bDepCancelled && Now - L16.Clock < 100.0)
			{
				return false;
			}
			L16.Phase = 4;
			return false;
		}
		default:
		{
			const int32 Requests = C->ClimbRequestsSent - L16.RequestsMark;
			const bool bOk = bLeft && !L16.bDepCancelled && Requests == 1 && L16.bDepPhase2 && L16.MaxNeeded == 2 && L16.MaxReady == L16.MaxNeeded;
			Note(FString::Printf(TEXT("hote au sommet de l'echelle des chunks : %d demande(s) en tout (attendu 1), centre au plus haut a %.0f cm du pied (sommet a %.0f) ; rassembles au plus "
				"%d/%d (attendu 2/2), depart (phase 2) %s%s ; client compte apres %d changement(s) d'etat"), Requests, L16.Track.MaxFeet + CapsuleHalf416(C) - L16.FootZ,
				L16.TopZ - L16.FootZ, L16.MaxReady, L16.MaxNeeded, OuiNon(L16.bDepPhase2 && bLeft), L16.bDepCancelled ? *FString::Printf(TEXT(", ANNULE (raison %d ; 1 : fin du delai)"),
				L16.DepReason) : (bLeft ? TEXT("") : TEXT(", NON lance en 100 s")), L16.GatherChanges), !bOk);
			L16.Phase = 0;
			L16.bNetOk = bLeft;
			return true;
		}
		}
	});
	LimitLastStep(180.f);
	Add(TEXT("v4.16 echelles reseau : arrivee au Niveau 2 chez chacun"), 1.f, [this, bClient]()
	{
		using namespace BRTestV416Ladder;
		ABRWorld* W = GetBRWorld();
		if (!W)
		{
			return false;
		}
		if (!L16.bNetOk)
		{
			return true;
		}
		const BRContent::FExitResolution Res = BRLevels::ResolveExit(37, PoolTarget);
		const bool bEnd = Res.Kind == BRContent::EExit::ChapterEnd;
		const bool bThere = bEnd ? W->IsChapterEnd() : (!W->IsTransitioning() && W->IsLevelReady() && W->GetLevelNumber() == Res.Target && W->GetLevelSerial() != L16.Serial);
		if (!bThere && StepTime < 40.f)
		{
			return false;
		}
		Note(FString::Printf(TEXT("%s : depart de groupe par l'echelle des chunks du Niveau 37 vers %s %s (Niveau %d)"), bClient ? TEXT("client") : TEXT("hote"), bEnd ? TEXT("la fin du contenu")
			: *FString::Printf(TEXT("le Niveau %d"), Res.Target), bThere ? TEXT("effectue") : TEXT("ECHEC"), W->GetLevelNumber()), !bThere);
		if (bEnd && W->IsChapterEnd() && !bClient)
		{
			W->CloseEnding(false);
		}
		return true;
	});
	LimitLastStep(50.f);
	Add(TEXT("v4.16 echelles reseau : retour a la regle normale"), 0.f, [this, bClient]()
	{
		if (!bClient)
		{
			if (ABRWorld* W = GetBRWorld())
			{
				W->SetLinearTestOverride(-1);
			}
		}
		BRTestV416Ladder::L16.Exit.Reset();
		BRTestV416Ladder::L16.Phase = 0;
		return true;
	});
}
