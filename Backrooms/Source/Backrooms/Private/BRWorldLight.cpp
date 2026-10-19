// v4.16 : lumiere de gameplay alignee sur la lumiere visible (prompt v4.16, 3.1 et 3.2).
//
// ABRWorld::LightLevelAt est ce que les regles lisent (Smiler qui se dissipe ou s'approche, entites de l'ombre qui
// apparaissent, Deathmoth, Bacteria, sante mentale, hallucinations, fond du bourdonnement). Jusqu'a la v4.15 :
//   - les plafonniers comptaient par distance a plat, a travers les murs : avec OMBRES DE TOUTES LES LAMPES (MegaLights), un
//     coin derriere un mur parait noir mais restait "eclaire" pour les regles (vis_0_*_mur_smiler : 0,72 dont 0,35 d'une lampe
//     derriere le mur) ;
//   - leur alimentation etait GetPower(), le vacillement de l'image tire au hasard chez chaque joueur et qui depend du
//     reglage FLASHS : 0,04, 0,95 ou 0,01 a 0,3 s d'un meme retour du courant (banc visuel, trois lancements) ;
//   - la trace d'une source de mission (canal WorldStatic, seul l'emetteur ignore) s'arretait sur la capsule d'un
//     personnage ou d'une creature (canal WorldStatic ne veut pas dire objets statiques seulement : une capsule Pawn bloque
//     ce canal). Relecture v4.16 : la trace finissait au centre de la cible (GetActorLocation) : la capsule de la cible
//     elle-meme l'arretait, toujours ; une source de mission ne comptait donc jamais pour un joueur ou une creature
//     (dissipation d'un Smiler, Deathmoth, Bacteria, sante mentale), seulement pour les apparitions (point sans cible) ;
//   - relecture v4.16 : un plafonnier de la grille que le chunk ne pose pas (sous la trappe d'une echelle de sortie)
//     comptait quand meme.
// Depuis la v4.16 :
//   - un plafonnier compte seulement si sa lumiere atteint le point selon la grille partagee (BRLight::Occlusion : murs
//     epais, jambages et linteau des passages, arches, poteaux et piliers, cellules pleines, plafond, sol perce ; une porte
//     de mission fermee sur une ouverture de la grille est un mur) et s'il est pose (pas sous une trappe d'echelle) : graine
//     et grille, memes resultats chez l'hote et chez les clients, aucune trace physique, rien du rendu (profil, Lumen,
//     MegaLights, FLASHS, carte graphique) ;
//   - l'alimentation vue par les regles est BRLight::PhasePower (fondu regulier de la phase repliquee) ;
//   - la trace d'une source de mission ignore les capsules (ECC_Pawn, comme le bruit : ABRWorld::FindRecentNoise) et la
//     cible ; murs des chunks et portes physiques fermees (boite Blocker des mecanismes, WorldDynamic) l'arretent toujours.
// Cout borne : plafonniers des cellules voisines seulement (+-R, comme avant), grille en cache par chunk (aretes, piliers,
// plafonniers, trappe : calcules une fois par chargement), plafonniers evalues au noeud du reseau de 10 cm le plus proche du
// point et mis en cache par noeud (relecture v4.16 : une creature qui bouge reprend le resultat tant qu'elle reste pres du
// meme noeud ; la valeur ne depend que du noeud), aucune trace pour les plafonniers ; sources de mission : une trace par
// source a portee, en cache 0,2 s par 10 cm. Le courant n'entre pas dans les caches (multiplie a chaque demande) ; un nouveau
// niveau, une nouvelle graine, un nouveau chargement, une nouvelle disposition de la mission ou un changement des portes de
// mission (revision de la mission, verifiee a chaque demande, plafonniers ou non) les vident.
#include "BRWorld.h"
#include "Backrooms.h"
#include "BRLightLogic.h"
#include "BRLightPrints.h"
#include "BRMechLogic.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

/** v4.16 (relecture) : calculs de la lumiere de gameplay (classe imbriquee d'ABRWorld : acces a ses membres prives ; BRWorld.h
 *  n'en declare que le nom) */
struct ABRWorld::FLightImpl
{
	/** Compteurs de l'image en cours (passe a la suivante a la premiere demande d'une nouvelle image) */
	static FBRLightStats& StatsNow(const ABRWorld& W);
	/** Disposition de la mission (version, variante, module de secours, parcours lineaire) */
	static uint32 MissionKeyOf(const ABRWorld& W);
	/** Caches perimes (niveau, graine, chargement, disposition de la mission) : vides ; revision de la mission changee :
	 *  traces a refaire, aretes fermees relues */
	static void Validate(const ABRWorld& W);
	/** Aretes fermees pour la lumiere : portes de mission sur une ouverture de la grille et crochet de test */
	static void ClosedEdgesNow(const ABRWorld& W, TSet<FIntPoint>& Out);
	/** Aretes fermees relues ; changees : grille et plafonniers a refaire */
	static void RecheckClosedEdges(const ABRWorld& W);
	/** Trappe de l'echelle de sortie du chunk (memes tirages et memes cotes qu'ABRChunk::PlanExits) */
	static bool PredictHatch(const ABRWorld& W, const FIntPoint& Chunk, BRLight::FHatch& Out);
	/** Cellule calculee d'apres la grille (aretes, pilier, bloc plein, plafonnier, trappe du chunk) */
	static FBRLightCell MakeCell(const ABRWorld& W, int32 X, int32 Y, const FBRLightCache::FChunk& Chunk);
	static const FBRLightCell& CellRef(const ABRWorld& W, int32 X, int32 Y);
	/** Noeud du reseau de BRLight::SnapStep cm le plus proche de P, et sa cle */
	static FVector Node(const FVector& P);
	static FIntVector NodeKey(const FVector& P);
	/** Plafonniers au noeud de P (cache par noeud) */
	static FBRLightCache::FResult Ceiling(const ABRWorld& W, const FVector& P);
	/** Sources de mission en P ; Sample : detail (traces fraiches, composants touches, filtres de diagnostic) */
	static float Mission(const ABRWorld& W, const FVector& P, const AActor* Target, FBRLightSample* Sample);
};

/** Grille du monde vue par BRLight::Occlusion (cellules en cache par chunk ; le cache est valide pendant tout le calcul) */
struct ABRWorld::FLightGridView
{
	const ABRWorld& W;
	explicit FLightGridView(const ABRWorld& InWorld) : W(InWorld) {}
	unsigned char EdgeE(int X, int Y) const { return FLightImpl::CellRef(W, X, Y).EdgeE; }
	unsigned char EdgeN(int X, int Y) const { return FLightImpl::CellRef(W, X, Y).EdgeN; }
	bool Pillar(int X, int Y) const { return FLightImpl::CellRef(W, X, Y).bPillar; }
	bool SolidBox(int X, int Y, double Lo[3], double Hi[3]) const
	{
		const FBRLightCell& C = FLightImpl::CellRef(W, X, Y);
		if (!C.bSolid)
		{
			return false;
		}
		Lo[0] = C.SolidLo.X;
		Lo[1] = C.SolidLo.Y;
		Lo[2] = C.SolidLo.Z;
		Hi[0] = C.SolidHi.X;
		Hi[1] = C.SolidHi.Y;
		Hi[2] = C.SolidHi.Z;
		return true;
	}
	bool FloorOpen(double X, double Y) const
	{
		const FVector Q(X, Y, 0.0);
		const FIntPoint Cell = W.WorldToCell(Q);
		if (W.IsOverPit(Q, 0.f) || W.IsPoolCell(Cell.X, Cell.Y))
		{
			return true;
		}
		// Bassin ou sas d'un mecanisme dont le fond est sous le sol (FloorZAt) : ouvert par le haut
		for (const FLocalWater& LW : W.LocalWaters)
		{
			if (LW.Box.FloorZ < 0.f && X >= LW.Box.MinX && X <= LW.Box.MaxX && Y >= LW.Box.MinY && Y <= LW.Box.MaxY)
			{
				return true;
			}
		}
		return false;
	}
	bool Lamp(int X, int Y, BRLight::Occlusion::FLamp& Out) const
	{
		const FBRLightCell& C = FLightImpl::CellRef(W, X, Y);
		Out.bOn = C.bLamp;
		Out.bFlicker = C.bFlicker;
		Out.AX = C.Anchor.X;
		Out.AY = C.Anchor.Y;
		Out.Emitter.X = C.Emitter.X;
		Out.Emitter.Y = C.Emitter.Y;
		Out.Emitter.Z = C.Emitter.Z;
		return C.bLamp;
	}
};

namespace
{
	BRLight::Occlusion::FVec BRWorldLightVec(const FVector& V)
	{
		BRLight::Occlusion::FVec Out;
		Out.X = V.X;
		Out.Y = V.Y;
		Out.Z = V.Z;
		return Out;
	}

	/** Nom lisible d'un obstacle touche (journal des tests) */
	FString BRWorldLightHitName(const FHitResult& Hit)
	{
		const AActor* A = Hit.GetActor();
		const UPrimitiveComponent* C = Hit.GetComponent();
		return FString::Printf(TEXT("%s.%s"), A ? *A->GetName() : TEXT("?"), C ? *C->GetName() : TEXT("?"));
	}

	/** Filtre de la trace d'une source de mission (v4.16) : les capsules des personnages et des creatures (Pawn) ne coupent pas
	 *  la lumiere ; murs (WorldStatic) et portes physiques (WorldDynamic qui bloquent) la coupent */
	FCollisionResponseParams BRWorldLightResponse()
	{
		FCollisionResponseParams R;
		R.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);
		return R;
	}

	/** Cle d'une arete entre deux cellules voisines (comme les portes de mission : X * 2 + (arete Est), Y de la cellule basse) */
	FIntPoint BRWorldLightEdgeKey(const FIntPoint& A, const FIntPoint& B)
	{
		const FIntPoint Lo(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		return FIntPoint(Lo.X * 2 + (A.Y == B.Y ? 1 : 0), Lo.Y);
	}

	/** Pas du cache des traces des sources de mission (cm) et duree de validite (s) */
	constexpr float BRWorldLightTraceStep = 10.f;
	constexpr double BRWorldLightTraceTTL = 0.2;
	/** Bornes des caches (effaces quand elles sont atteintes) */
	constexpr int32 BRWorldLightMaxChunks = 512;
	constexpr int32 BRWorldLightMaxResults = 4096;
	constexpr int32 BRWorldLightMaxTraces = 2048;
}

// ------------------------------------------------------------------------------------------------ calculs (FLightImpl)
FBRLightStats& ABRWorld::FLightImpl::StatsNow(const ABRWorld& W)
{
	FBRLightCache& C = W.LightCache;
	if (GFrameCounter != C.StatsFrameNo)
	{
		C.Last = C.Frame;
		C.Frame = FBRLightStats();
		C.Frame.Frames = 1;
		C.Total.Frames += 1;
		C.StatsFrameNo = GFrameCounter;
	}
	return C.Frame;
}

uint32 ABRWorld::FLightImpl::MissionKeyOf(const ABRWorld& W)
{
	return static_cast<uint32>(W.MissionGen) | (static_cast<uint32>(W.MissionTier) << 8) | (W.MissionPlan.bFallback ? (1u << 16) : 0u)
		| (W.bLinearLevel ? (1u << 17) : 0u);
}

void ABRWorld::FLightImpl::Validate(const ABRWorld& W)
{
	FBRLightCache& C = W.LightCache;
	const FBRLevelDef& D = W.Def();
	const uint32 Key = MissionKeyOf(W);
	if (!C.bValid || C.Seed != W.Seed || C.Serial != W.LoadedSerial || C.Level != D.Number || C.MissionKey != Key || C.MissionCells != W.MissionCells.Num()
		|| C.MissionExits != W.MissionExitSpots.Num())
	{
		W.InvalidateLightCache();
		C.bValid = true;
		C.Seed = W.Seed;
		C.Serial = W.LoadedSerial;
		C.Level = D.Number;
		C.MissionKey = Key;
		C.MissionCells = W.MissionCells.Num();
		C.MissionExits = W.MissionExitSpots.Num();
		C.Rev = W.NetMission.Rev;
		// Cotes que batissent les chunks (memes regles que le banc : BRLight::Occlusion::MakeGeom)
		C.Geom = BRLight::Occlusion::MakeGeom(D.CellSize, D.WallThickness, D.WallHeight, D.DoorWidth, D.bLintels, D.bArches, D.ArchApex, D.PillarSize,
			D.Layout == EBRLayout::Rooms && D.PillarChance > 0.f, D.bCeiling);
		ClosedEdgesNow(W, C.ClosedEdges);
		// Tests : empreinte de la grille de lumiere de ce niveau, a comparer entre l'hote et les clients (et avec le banc)
		if (D.Fixture != EBRFixture::None && (FParse::Param(FCommandLine::Get(), TEXT("BRAutoTest")) || FParse::Param(FCommandLine::Get(), TEXT("BRNetTest"))))
		{
			UE_LOG(LogBackrooms, Display, TEXT("[Lumiere] Niveau %d, graine du niveau %u (%s) : empreinte de la grille de lumiere %08x (cellules -%d..%d)"), D.Number,
				W.Seed, W.HasAuthority() ? TEXT("hote") : TEXT("client"), W.GetLightGridFingerprint(BRLightPrints::Half), BRLightPrints::Half, BRLightPrints::Half - 1);
		}
		return;
	}
	if (W.NetMission.Rev != C.Rev)
	{
		// Etat de la mission change : traces des sources de mission a refaire (portes, battants) ; la grille seulement si une
		// porte de mission posee sur une ouverture de la grille a change (Level 6)
		C.Rev = W.NetMission.Rev;
		C.Traces.Reset();
		RecheckClosedEdges(W);
	}
}

void ABRWorld::FLightImpl::ClosedEdgesNow(const ABRWorld& W, TSet<FIntPoint>& Out)
{
	Out.Reset();
	TArray<TPair<FIntPoint, FIntPoint>> Closed;
	W.GetClosedMissionEdges(Closed);
	for (const TPair<FIntPoint, FIntPoint>& E : Closed)
	{
		Out.Add(BRWorldLightEdgeKey(E.Key, E.Value));
	}
	const FBRLightCache& C = W.LightCache;
	if (C.TestEdgesSerial == W.LoadedSerial)
	{
		Out.Append(C.TestClosedEdges);
	}
}

void ABRWorld::FLightImpl::RecheckClosedEdges(const ABRWorld& W)
{
	FBRLightCache& C = W.LightCache;
	TSet<FIntPoint> Now;
	ClosedEdgesNow(W, Now);
	if (Now.Num() != C.ClosedEdges.Num() || Now.Difference(C.ClosedEdges).Num() > 0)
	{
		C.Chunks.Reset();
		C.Memo = nullptr;
		C.MemoChunk = FIntPoint(MAX_int32, MAX_int32);
		C.Results.Reset();
		C.ClosedEdges = MoveTemp(Now);
		++C.Total.Invalidations;
	}
}

bool ABRWorld::FLightImpl::PredictHatch(const ABRWorld& W, const FIntPoint& Chunk, BRLight::FHatch& Out)
{
	const FBRLevelDef& D = W.Def();
	if (!D.bCeiling || D.Fixture == EBRFixture::None)
	{
		return false;
	}
	const float S = D.CellSize;
	for (int32 I = 0; I < D.Exits.Num(); ++I)
	{
		// ABRChunk::PlanExits : la premiere echelle tiree par le chunk (ordre des sorties du niveau) perce le plafond d'une trappe
		if (D.Exits[I].Style != EBRExitStyle::Ladder || !W.IsChunkExitDrawn(Chunk, I))
		{
			continue;
		}
		// Cachettes du chunk : aucune (seul le Level 0 en a, et sa seule sortie est la porte de service : banc, section B)
		BRMech::Grid::FWallFace Face;
		if (!W.PickChunkWallFace(Chunk, I, nullptr, Face))
		{
			continue;
		}
		// Pied et orientation de l'echelle, puis conduit : memes operations que le chunk
		const FIntPoint Cell(Face.CellX, Face.CellY);
		const FIntPoint Dir(BRMech::Grid::DirX[Face.Dir], BRMech::Grid::DirY[Face.Dir]);
		const float Inset = Face.bSolid ? 0.f : D.WallThickness * 0.5f;
		const FVector Pos = W.CellCenter(Cell, 0.f) + FVector(Dir.X, Dir.Y, 0.f) * (S * 0.5f - Inset);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-Dir.Y), static_cast<float>(-Dir.X)));
		const FVector Fwd = FRotator(0.f, Yaw, 0.f).Vector();
		Out = BRLight::HatchRect(Pos.X, Pos.Y, Fwd.X, Fwd.Y);
		return true;
	}
	return false;
}

FBRLightCell ABRWorld::FLightImpl::MakeCell(const ABRWorld& W, int32 X, int32 Y, const FBRLightCache::FChunk& Chunk)
{
	const FBRLevelDef& D = W.Def();
	const FBRLightCache& Cache = W.LightCache;
	FBRLightCell C;
	C.EdgeE = static_cast<uint8>(W.EdgeE(X, Y));
	C.EdgeN = static_cast<uint8>(W.EdgeN(X, Y));
	if (Cache.ClosedEdges.Num() > 0)
	{
		// Porte de mission fermee sur une ouverture de la grille (battant de l'aile du Level 6) : un mur pour la lumiere
		if (Cache.ClosedEdges.Contains(FIntPoint(X * 2 + 1, Y)))
		{
			C.EdgeE = static_cast<uint8>(EBREdge::Wall);
		}
		if (Cache.ClosedEdges.Contains(FIntPoint(X * 2, Y)))
		{
			C.EdgeN = static_cast<uint8>(EBREdge::Wall);
		}
	}
	C.bPillar = W.HasPillar(X, Y);
	const double S = D.CellSize;
	if (W.IsSolid(X, Y))
	{
		C.bSolid = true;
		C.SolidLo = FVector(X * S, Y * S, -BRLight::Occlusion::Huge);
		C.SolidHi = FVector((X + 1) * S, (Y + 1) * S, BRLight::Occlusion::Huge);
		if (D.Layout == EBRLayout::Suburbs && D.Spacing > 2)
		{
			// Niveau 9 : la maison de la parcelle (boite de repli de son modele : 8 x 7 x 4,5 m, au sommet commun de ses 2 x 2
			// cellules), pas toute la parcelle : la lumiere passe dans le jardin et au-dessus du toit
			const int32 LX = BRHash::PosMod(X, D.Spacing);
			const int32 LY = BRHash::PosMod(Y, D.Spacing);
			const double CX = (X - (LX - 1) % 2 + 1) * S;
			const double CY = (Y - (LY - 1) % 2 + 1) * S;
			C.SolidLo = FVector(FMath::Max(C.SolidLo.X, CX - 400.0), FMath::Max(C.SolidLo.Y, CY - 350.0), -BRLight::Occlusion::Huge);
			C.SolidHi = FVector(FMath::Min(C.SolidHi.X, CX + 400.0), FMath::Min(C.SolidHi.Y, CY + 350.0), 450.0);
			C.bSolid = C.SolidHi.X > C.SolidLo.X && C.SolidHi.Y > C.SolidLo.Y;
		}
	}
	const FBRLightInfo L = W.CellLight(X, Y);
	if (L.bHas && !L.bBroken && W.IsCellInBounds(X, Y))
	{
		// Meme ancre qu'avant la v4.16 (distance de la loi d'apport, et position que le chunk compare a sa trappe) ; point
		// d'emission : celui de la lumiere construite
		C.Anchor = W.CellCenter(FIntPoint(X, Y)) + L.Offset;
		BRLight::Occlusion::FVec E;
		if (BRLight::Occlusion::Emitter(static_cast<int>(D.Fixture), D.WallHeight, C.Anchor.X, C.Anchor.Y, L.Offset.X, L.Offset.Y, E))
		{
			// v4.16 (relecture) : sous la trappe d'une echelle de sortie, le chunk ne pose pas ce plafonnier : il ne compte pas
			C.bUnderHatch = Chunk.bHatch && BRLight::UnderHatch(Chunk.Hatch, C.Anchor.X, C.Anchor.Y);
			C.bLamp = !C.bUnderHatch;
			C.bFlicker = L.bFlicker;
			C.Emitter = FVector(E.X, E.Y, E.Z);
		}
	}
	return C;
}

const FBRLightCell& ABRWorld::FLightImpl::CellRef(const ABRWorld& W, int32 X, int32 Y)
{
	FBRLightCache& C = W.LightCache;
	const FIntPoint Chunk = W.CellToChunk(FIntPoint(X, Y));
	const int32 N = FMath::Max(1, W.Def().ChunkCells);
	if (!C.Memo || Chunk != C.MemoChunk)
	{
		C.Memo = C.Chunks.Find(Chunk);
		if (!C.Memo)
		{
			if (C.Chunks.Num() >= BRWorldLightMaxChunks)
			{
				C.Chunks.Reset();
			}
			FBRLightCache::FChunk New;
			New.bHatch = PredictHatch(W, Chunk, New.Hatch);
			New.Cells.SetNum(N * N);
			for (int32 LY = 0; LY < N; ++LY)
			{
				for (int32 LX = 0; LX < N; ++LX)
				{
					New.Cells[LY * N + LX] = MakeCell(W, Chunk.X * N + LX, Chunk.Y * N + LY, New);
				}
			}
			C.Memo = &C.Chunks.Add(Chunk, MoveTemp(New));
			++StatsNow(W).ChunkBuilds;
			++C.Total.ChunkBuilds;
		}
		C.MemoChunk = Chunk;
	}
	return C.Memo->Cells[BRHash::PosMod(Y, N) * N + BRHash::PosMod(X, N)];
}

FVector ABRWorld::FLightImpl::Node(const FVector& P)
{
	return FVector(BRLight::SnapCoord(P.X), BRLight::SnapCoord(P.Y), BRLight::SnapCoord(P.Z));
}

FIntVector ABRWorld::FLightImpl::NodeKey(const FVector& P)
{
	return FIntVector(static_cast<int32>(BRLight::SnapIndex(P.X)), static_cast<int32>(BRLight::SnapIndex(P.Y)), static_cast<int32>(BRLight::SnapIndex(P.Z)));
}

FBRLightCache::FResult ABRWorld::FLightImpl::Ceiling(const ABRWorld& W, const FVector& P)
{
	FBRLightCache& C = W.LightCache;
	FBRLightStats& Now = StatsNow(W);
	const FIntVector Key = NodeKey(P);
	if (const FBRLightCache::FResult* Found = C.Results.Find(Key))
	{
		++Now.CacheHits;
		++C.Total.CacheHits;
		return *Found;
	}
	const FBRLevelDef& D = W.Def();
	BRLight::Occlusion::FStats St;
	FBRLightCache::FResult R;
	BRLight::Occlusion::CeilingSum(FLightGridView(W), C.Geom, D.LightRadius, BRWorldLightVec(Node(P)), R.All, R.Visible, &St);
	++Now.Computes;
	++C.Total.Computes;
	Now.Lamps += St.Lamps;
	Now.Hidden += St.Hidden;
	Now.Boxes += St.Boxes;
	C.Total.Lamps += St.Lamps;
	C.Total.Hidden += St.Hidden;
	C.Total.Boxes += St.Boxes;
	if (C.Results.Num() >= BRWorldLightMaxResults)
	{
		C.Results.Reset();
	}
	C.Results.Add(Key, R);
	return R;
}

float ABRWorld::FLightImpl::Mission(const ABRWorld& W, const FVector& P, const AActor* Target, FBRLightSample* Sample)
{
	UWorld* InWorld = W.GetWorld();
	if (W.GameplayLights.Num() == 0 || !InWorld)
	{
		return 0.f;
	}
	FBRLightCache& C = W.LightCache;
	FBRLightStats& Now = StatsNow(W);
	const double Time = InWorld->GetTimeSeconds();
	const FIntVector Spot(FMath::RoundToInt(P.X / BRWorldLightTraceStep), FMath::RoundToInt(P.Y / BRWorldLightTraceStep),
		FMath::RoundToInt(P.Z / BRWorldLightTraceStep));
	float Sum = 0.f;
	for (const TPair<uint64, FGameplayLight>& Pair : W.GameplayLights)
	{
		const FGameplayLight& L = Pair.Value;
		const float Dist = static_cast<float>(FVector::Dist(P, L.Pos));
		const float Add = BRLight::Contribution(Dist, L.Radius, L.Intensity);
		if (Add <= 0.01f)
		{
			continue;
		}
		const AActor* LightActor = L.Owner.Get();
		// v4.16 : canal WorldStatic sans les capsules (Pawn) ni la cible : seuls murs et portes physiques arretent la lumiere
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRGameplayLight), false);
		if (LightActor)
		{
			Q.AddIgnoredActor(LightActor);
		}
		if (Target)
		{
			Q.AddIgnoredActor(Target);
		}
		bool bVisible = true;
		if (Sample)
		{
			// Diagnostic : traces fraiches, avec le filtre v4.16, celui d'avant (seul l'emetteur ignore : la capsule de la cible
			// arrete la trace) et un filtre qui garde les capsules sans la cible (effet d'un coequipier ou d'une creature)
			++Sample->MissionSources;
			FHitResult Hit;
			bVisible = !InWorld->LineTraceSingleByChannel(Hit, L.Pos, P, ECC_WorldStatic, Q, BRWorldLightResponse());
			if (!bVisible)
			{
				Sample->MissionHits += (Sample->MissionHits.IsEmpty() ? TEXT("") : TEXT(", ")) + BRWorldLightHitName(Hit);
			}
			FCollisionQueryParams Old(SCENE_QUERY_STAT(BRGameplayLightOld), false);
			if (LightActor)
			{
				Old.AddIgnoredActor(LightActor);
			}
			FHitResult OldHit;
			if (InWorld->LineTraceSingleByChannel(OldHit, L.Pos, P, ECC_WorldStatic, Old))
			{
				Sample->MissionOldHits += (Sample->MissionOldHits.IsEmpty() ? TEXT("") : TEXT(", ")) + BRWorldLightHitName(OldHit);
				Sample->MissionOldHitActors.Add(OldHit.GetActor());
			}
			else
			{
				Sample->MissionOldFilter += Add;
			}
			FCollisionQueryParams Pawns(SCENE_QUERY_STAT(BRGameplayLightPawns), false);
			if (LightActor)
			{
				Pawns.AddIgnoredActor(LightActor);
			}
			if (Target)
			{
				Pawns.AddIgnoredActor(Target);
			}
			FHitResult PawnHit;
			if (InWorld->LineTraceSingleByChannel(PawnHit, L.Pos, P, ECC_WorldStatic, Pawns))
			{
				Sample->MissionPawnHits += (Sample->MissionPawnHits.IsEmpty() ? TEXT("") : TEXT(", ")) + BRWorldLightHitName(PawnHit);
				Sample->MissionPawnHitActors.Add(PawnHit.GetActor());
			}
			else
			{
				Sample->MissionPawnFilter += Add;
			}
			++Now.Traces;
			++C.Total.Traces;
		}
		else
		{
			const FBRLightCache::FTraceKey Key{ Pair.Key, Spot, Target ? Target->GetUniqueID() : 0u };
			FBRLightCache::FTrace* Cached = C.Traces.Find(Key);
			if (Cached && Time - Cached->Time <= BRWorldLightTraceTTL && Time >= Cached->Time)
			{
				bVisible = Cached->bVisible;
				++Now.TraceHits;
				++C.Total.TraceHits;
			}
			else
			{
				bVisible = !InWorld->LineTraceTestByChannel(L.Pos, P, ECC_WorldStatic, Q, BRWorldLightResponse());
				++Now.Traces;
				++C.Total.Traces;
				if (!Cached)
				{
					if (C.Traces.Num() >= BRWorldLightMaxTraces)
					{
						C.Traces.Reset();
					}
					Cached = &C.Traces.Add(Key);
				}
				Cached->Time = Time;
				Cached->bVisible = bVisible;
			}
		}
		if (bVisible)
		{
			Sum += Add;
		}
	}
	return Sum;
}

// ------------------------------------------------------------------------------------------------ interface d'ABRWorld
float ABRWorld::GetGameplayPower() const
{
	return BRLight::PhasePower(static_cast<int>(BlackoutPhase), BlackoutTimer);
}

FBRLightStats ABRWorld::GetLightStats(bool bTotal) const
{
	return bTotal ? LightCache.Total : LightCache.Last;
}

void ABRWorld::ResetLightStats() const
{
	LightCache.Total = FBRLightStats();
}

void ABRWorld::InvalidateLightCache() const
{
	FBRLightCache& C = LightCache;
	C.Chunks.Reset();
	C.Memo = nullptr;
	C.MemoChunk = FIntPoint(MAX_int32, MAX_int32);
	C.Results.Reset();
	C.Traces.Reset();
	C.ClosedEdges.Reset();
	C.bValid = false;
	++C.Total.Invalidations;
}

BRLight::Occlusion::FGeom ABRWorld::GetLightGeom() const
{
	FLightImpl::Validate(*this);
	return LightCache.Geom;
}

FBRLightCell ABRWorld::GetLightCell(int32 X, int32 Y) const
{
	FLightImpl::Validate(*this);
	return FLightImpl::CellRef(*this, X, Y);
}

bool ABRWorld::GetLightHatch(const FIntPoint& Chunk, BRLight::FHatch& Out) const
{
	FLightImpl::Validate(*this);
	const int32 N = FMath::Max(1, Def().ChunkCells);
	FLightImpl::CellRef(*this, Chunk.X * N, Chunk.Y * N); // chunk de la grille en cache (sa trappe avec lui)
	const FBRLightCache::FChunk* Found = LightCache.Chunks.Find(Chunk);
	if (!Found || !Found->bHatch)
	{
		return false;
	}
	Out = Found->Hatch;
	return true;
}

void ABRWorld::SetLightTestClosedEdge(const FIntPoint& A, const FIntPoint& B, bool bClosed)
{
	FLightImpl::Validate(*this);
	FBRLightCache& C = LightCache;
	if (C.TestEdgesSerial != LoadedSerial)
	{
		C.TestClosedEdges.Reset();
		C.TestEdgesSerial = LoadedSerial;
	}
	if (bClosed)
	{
		C.TestClosedEdges.Add(BRWorldLightEdgeKey(A, B));
	}
	else
	{
		C.TestClosedEdges.Remove(BRWorldLightEdgeKey(A, B));
	}
	// Meme chemin qu'une porte de mission qui change (revision de la mission) : aretes relues, grille refaite si elles changent
	FLightImpl::RecheckClosedEdges(*this);
}

uint32 ABRWorld::GetLightGridFingerprint(int32 Half) const
{
	FLightImpl::Validate(*this);
	uint32 H = 2166136261u;
	auto Feed = [&H](int32 V)
	{
		const uint32 U = static_cast<uint32>(V);
		for (int32 B = 0; B < 4; ++B)
		{
			H ^= (U >> (8 * B)) & 0xFFu;
			H *= 16777619u;
		}
	};
	for (int32 Y = -Half; Y < Half; ++Y)
	{
		for (int32 X = -Half; X < Half; ++X)
		{
			const FBRLightCell& C = FLightImpl::CellRef(*this, X, Y);
			// Plafonnier de la grille (pose ou sous une trappe : le banc ne tire pas les sorties) : l'empreinte decrit la grille
			const bool bGridLamp = C.bLamp || C.bUnderHatch;
			Feed(C.EdgeE | (C.EdgeN << 2) | (C.bPillar ? 16 : 0) | (C.bSolid ? 32 : 0) | (bGridLamp ? 64 : 0) | (C.bFlicker ? 128 : 0));
			if (bGridLamp)
			{
				Feed(static_cast<int32>(FMath::FloorToDouble(C.Anchor.X + 0.5)));
				Feed(static_cast<int32>(FMath::FloorToDouble(C.Anchor.Y + 0.5)));
			}
		}
	}
	return H;
}

bool ABRWorld::IsLampVisibleFrom(const FIntPoint& LampCell, const FVector& P, const BRLight::Occlusion::FGeom* Geom) const
{
	FLightImpl::Validate(*this);
	const FBRLightCell C = FLightImpl::CellRef(*this, LampCell.X, LampCell.Y);
	return C.bLamp && BRLight::Occlusion::Visible(FLightGridView(*this), Geom ? *Geom : LightCache.Geom, BRWorldLightVec(C.Emitter), BRWorldLightVec(P));
}

float ABRWorld::LightLevelAt(const FVector& P, const AActor* Target) const
{
	const FBRLevelDef& D = Def();
	if (D.bOutdoor && (D.Sky == EBRSky::Day || D.Sky == EBRSky::Overcast))
	{
		return 1.f;
	}
	// v4.16 (relecture) : caches verifies a chaque demande, plafonniers ou non (un niveau sans plafonnier, ou une coupure, voit
	// aussi la revision de la mission : traces des sources de mission refaites quand une porte change)
	FLightImpl::Validate(*this);
	FBRLightStats& Now = FLightImpl::StatsNow(*this);
	++Now.Queries;
	++LightCache.Total.Queries;
	// Plafonniers du niveau dont la lumiere atteint le noeud de P (grille), au courant vu par les regles
	float Acc = 0.f;
	const float Pow = GetGameplayPower();
	if (D.Fixture != EBRFixture::None && Pow >= 0.05f)
	{
		Acc = FLightImpl::Ceiling(*this, P).Visible * Pow;
	}
	// v4.12 : sources de mission allumees (balises, sortie eclairee, porche) : autonomes, meme loi que les plafonniers, en
	// 3D ; un mur ou une porte fermee entre la source et le point l'annule (v4.16 : pas une capsule)
	Acc += FLightImpl::Mission(*this, P, Target, nullptr);
	return FMath::Clamp(Acc, 0.f, 1.5f);
}

FBRLightSample ABRWorld::SampleLight(const FVector& P, const AActor* Target, bool bDiagnose) const
{
	FBRLightSample S;
	const FBRLevelDef& D = Def();
	S.Power = GetGameplayPower();
	S.VisualPower = Power;
	S.Node = FLightImpl::Node(P);
	if (D.bOutdoor && (D.Sky == EBRSky::Day || D.Sky == EBRSky::Overcast))
	{
		S.Value = 1.f;
		return S;
	}
	FLightImpl::Validate(*this);
	if (D.Fixture != EBRFixture::None)
	{
		BRLight::Occlusion::FStats St;
		BRLight::Occlusion::CeilingSum(FLightGridView(*this), LightCache.Geom, D.LightRadius, BRWorldLightVec(S.Node), S.CeilingAll, S.CeilingVisible, &St);
		S.Lamps = St.Lamps;
		S.LampsHidden = St.Hidden;
		S.Boxes = St.Boxes;
	}
	float Acc = (D.Fixture != EBRFixture::None && S.Power >= 0.05f) ? S.CeilingVisible * S.Power : 0.f;
	if (bDiagnose)
	{
		S.Mission = FLightImpl::Mission(*this, P, Target, &S);
	}
	else
	{
		S.Mission = FLightImpl::Mission(*this, P, Target, nullptr);
		S.MissionOldFilter = S.Mission;
		S.MissionPawnFilter = S.Mission;
	}
	Acc += S.Mission;
	S.Value = FMath::Clamp(Acc, 0.f, 1.5f);
	return S;
}

void ABRWorld::SetGameplayLight(const AActor* LightOwner, uint8 Slot, const FVector& Where, float Radius, float Intensity, bool bOn)
{
	if (!LightOwner)
	{
		return;
	}
	const uint64 Key = (static_cast<uint64>(LightOwner->GetUniqueID()) << 8) | Slot;
	if (!bOn)
	{
		if (GameplayLights.Remove(Key) > 0)
		{
			LightCache.Traces.Reset(); // v4.16 : traces en cache de cette source perimees
		}
		return;
	}
	FGameplayLight& L = GameplayLights.FindOrAdd(Key);
	if (!L.Pos.Equals(Where, 0.5) || L.Radius != Radius)
	{
		LightCache.Traces.Reset(); // v4.16 : source deplacee (battant, mecanisme) : traces a refaire
	}
	L.Pos = Where;
	L.Radius = Radius;
	L.Intensity = Intensity;
	L.Owner = LightOwner;
}
