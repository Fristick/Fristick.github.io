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
//     ce canal) : un coequipier entre une balise et un Smiler annulait la lumiere de la balise.
// Depuis la v4.16 :
//   - un plafonnier compte seulement si sa lumiere atteint le point selon la grille partagee (BRLight::Occlusion : murs
//     epais, jambages et linteau des passages, arches, poteaux et piliers, cellules pleines, plafond, sol perce ; une porte
//     de mission fermee sur une ouverture de la grille est un mur) : graine et grille, memes resultats chez l'hote et chez
//     les clients, aucune trace physique, rien du rendu (profil, Lumen, MegaLights, FLASHS, carte graphique) ;
//   - l'alimentation vue par les regles est BRLight::PhasePower (fondu regulier de la phase repliquee) ;
//   - la trace d'une source de mission ignore les capsules (ECC_Pawn, comme le bruit : ABRWorld::FindRecentNoise) et la
//     cible ; murs des chunks et portes physiques fermees (boite Blocker des mecanismes, WorldDynamic) l'arretent toujours.
// Cout borne : plafonniers des cellules voisines seulement (+-R, comme avant), grille en cache par chunk (aretes, piliers,
// plafonniers : calcules une fois par chargement), resultat par point en cache (une creature immobile ne recalcule rien),
// aucune trace pour les plafonniers ; sources de mission : une trace par source a portee, en cache 0,2 s par 10 cm. Le
// courant n'entre pas dans les caches (multiplie a chaque demande) ; un nouveau niveau, une nouvelle graine, un nouveau
// chargement ou un changement des portes de mission (revision de la mission) les vident.
#include "BRWorld.h"
#include "Backrooms.h"
#include "BRLightLogic.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

/** Grille du monde vue par BRLight::Occlusion (cellules en cache par chunk ; le cache est valide pendant tout le calcul) */
struct ABRWorld::FLightGridView
{
	const ABRWorld& W;
	explicit FLightGridView(const ABRWorld& InWorld) : W(InWorld) {}
	unsigned char EdgeE(int X, int Y) const { return W.LightCellRef(X, Y).EdgeE; }
	unsigned char EdgeN(int X, int Y) const { return W.LightCellRef(X, Y).EdgeN; }
	bool Pillar(int X, int Y) const { return W.LightCellRef(X, Y).bPillar; }
	bool SolidBox(int X, int Y, double Lo[3], double Hi[3]) const
	{
		const FBRLightCell& C = W.LightCellRef(X, Y);
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
		const FBRLightCell& C = W.LightCellRef(X, Y);
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

	/** Pas du cache des traces des sources de mission (cm) et duree de validite (s) */
	constexpr float BRWorldLightTraceStep = 10.f;
	constexpr double BRWorldLightTraceTTL = 0.2;
	/** Bornes des caches (effaces quand elles sont atteintes) */
	constexpr int32 BRWorldLightMaxChunks = 512;
	constexpr int32 BRWorldLightMaxResults = 4096;
	constexpr int32 BRWorldLightMaxTraces = 2048;
}

float ABRWorld::GetGameplayPower() const
{
	return BRLight::PhasePower(static_cast<int>(BlackoutPhase), BlackoutTimer);
}

FBRLightStats& ABRWorld::LightStatsNow() const
{
	if (GFrameCounter != LightStatsFrameNo)
	{
		LightLast = LightFrame;
		LightFrame = FBRLightStats();
		LightFrame.Frames = 1;
		LightTotal.Frames += 1;
		LightStatsFrameNo = GFrameCounter;
	}
	return LightFrame;
}

FBRLightStats ABRWorld::GetLightStats(bool bTotal) const
{
	return bTotal ? LightTotal : LightLast;
}

void ABRWorld::ResetLightStats() const
{
	LightTotal = FBRLightStats();
}

void ABRWorld::InvalidateLightCache() const
{
	LightChunks.Reset();
	LightMemo = nullptr;
	LightMemoChunk = FIntPoint(MAX_int32, MAX_int32);
	LightResults.Reset();
	LightTraces.Reset();
	LightClosedEdges.Reset();
	bLightCacheValid = false;
	++LightTotal.Invalidations;
}

BRLight::Occlusion::FGeom ABRWorld::GetLightGeom() const
{
	ValidateLightCache();
	return LightGeom;
}

void ABRWorld::ValidateLightCache() const
{
	const FBRLevelDef& D = Def();
	if (!bLightCacheValid || LightCacheSeed != Seed || LightCacheSerial != LoadedSerial || LightCacheLevel != D.Number)
	{
		InvalidateLightCache();
		bLightCacheValid = true;
		LightCacheSeed = Seed;
		LightCacheSerial = LoadedSerial;
		LightCacheLevel = D.Number;
		LightCacheRev = NetMission.Rev;
		// Cotes que batissent les chunks (memes regles que le banc : BRLight::Occlusion::MakeGeom)
		LightGeom = BRLight::Occlusion::MakeGeom(D.CellSize, D.WallThickness, D.WallHeight, D.DoorWidth, D.bLintels, D.bArches, D.ArchApex, D.PillarSize,
			D.Layout == EBRLayout::Rooms && D.PillarChance > 0.f, D.bCeiling);
		TArray<TPair<FIntPoint, FIntPoint>> Closed;
		GetClosedMissionEdges(Closed);
		for (const TPair<FIntPoint, FIntPoint>& E : Closed)
		{
			const FIntPoint Lo(FMath::Min(E.Key.X, E.Value.X), FMath::Min(E.Key.Y, E.Value.Y));
			LightClosedEdges.Add(FIntPoint(Lo.X * 2 + (E.Key.Y == E.Value.Y ? 1 : 0), Lo.Y));
		}
		// Tests : empreinte de la grille de lumiere de ce niveau, a comparer entre l'hote et les clients (et avec le banc)
		if (D.Fixture != EBRFixture::None && (FParse::Param(FCommandLine::Get(), TEXT("BRAutoTest")) || FParse::Param(FCommandLine::Get(), TEXT("BRNetTest"))))
		{
			UE_LOG(LogBackrooms, Display, TEXT("[Lumiere] Niveau %d, graine du niveau %u (%s) : empreinte de la grille de lumiere %08x (cellules -12..11)"), D.Number, Seed,
				HasAuthority() ? TEXT("hote") : TEXT("client"), GetLightGridFingerprint(12));
		}
		return;
	}
	if (NetMission.Rev != LightCacheRev)
	{
		// Etat de la mission change : traces des sources de mission a refaire (portes, battants) ; la grille seulement si une
		// porte de mission posee sur une ouverture de la grille a change (Level 6)
		LightCacheRev = NetMission.Rev;
		LightTraces.Reset();
		TArray<TPair<FIntPoint, FIntPoint>> Closed;
		GetClosedMissionEdges(Closed);
		TSet<FIntPoint> Now;
		for (const TPair<FIntPoint, FIntPoint>& E : Closed)
		{
			const FIntPoint Lo(FMath::Min(E.Key.X, E.Value.X), FMath::Min(E.Key.Y, E.Value.Y));
			Now.Add(FIntPoint(Lo.X * 2 + (E.Key.Y == E.Value.Y ? 1 : 0), Lo.Y));
		}
		if (Now.Num() != LightClosedEdges.Num() || Now.Difference(LightClosedEdges).Num() > 0)
		{
			LightChunks.Reset();
			LightMemo = nullptr;
			LightMemoChunk = FIntPoint(MAX_int32, MAX_int32);
			LightResults.Reset();
			LightClosedEdges = MoveTemp(Now);
			++LightTotal.Invalidations;
		}
	}
}

FBRLightCell ABRWorld::MakeLightCell(int32 X, int32 Y) const
{
	const FBRLevelDef& D = Def();
	FBRLightCell C;
	C.EdgeE = static_cast<uint8>(EdgeE(X, Y));
	C.EdgeN = static_cast<uint8>(EdgeN(X, Y));
	if (LightClosedEdges.Num() > 0)
	{
		// Porte de mission fermee sur une ouverture de la grille (battant de l'aile du Level 6) : un mur pour la lumiere
		if (LightClosedEdges.Contains(FIntPoint(X * 2 + 1, Y)))
		{
			C.EdgeE = static_cast<uint8>(EBREdge::Wall);
		}
		if (LightClosedEdges.Contains(FIntPoint(X * 2, Y)))
		{
			C.EdgeN = static_cast<uint8>(EBREdge::Wall);
		}
	}
	C.bPillar = HasPillar(X, Y);
	const double S = D.CellSize;
	if (IsSolid(X, Y))
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
	const FBRLightInfo L = CellLight(X, Y);
	if (L.bHas && !L.bBroken && IsCellInBounds(X, Y))
	{
		// Meme ancre qu'avant la v4.16 (distance de la loi d'apport) ; point d'emission : celui de la lumiere construite
		C.Anchor = CellCenter(FIntPoint(X, Y)) + L.Offset;
		BRLight::Occlusion::FVec E;
		if (BRLight::Occlusion::Emitter(static_cast<int>(D.Fixture), D.WallHeight, C.Anchor.X, C.Anchor.Y, L.Offset.X, L.Offset.Y, E))
		{
			C.bLamp = true;
			C.bFlicker = L.bFlicker;
			C.Emitter = FVector(E.X, E.Y, E.Z);
		}
	}
	return C;
}

const FBRLightCell& ABRWorld::LightCellRef(int32 X, int32 Y) const
{
	const FIntPoint Cell(X, Y);
	const FIntPoint Chunk = CellToChunk(Cell);
	const int32 N = FMath::Max(1, Def().ChunkCells);
	if (!LightMemo || Chunk != LightMemoChunk)
	{
		LightMemo = LightChunks.Find(Chunk);
		if (!LightMemo)
		{
			if (LightChunks.Num() >= BRWorldLightMaxChunks)
			{
				LightChunks.Reset();
			}
			FLightChunk New;
			New.Cells.SetNum(N * N);
			for (int32 LY = 0; LY < N; ++LY)
			{
				for (int32 LX = 0; LX < N; ++LX)
				{
					New.Cells[LY * N + LX] = MakeLightCell(Chunk.X * N + LX, Chunk.Y * N + LY);
				}
			}
			LightMemo = &LightChunks.Add(Chunk, MoveTemp(New));
			++LightStatsNow().ChunkBuilds;
			++LightTotal.ChunkBuilds;
		}
		LightMemoChunk = Chunk;
	}
	return LightMemo->Cells[BRHash::PosMod(Y, N) * N + BRHash::PosMod(X, N)];
}

FBRLightCell ABRWorld::GetLightCell(int32 X, int32 Y) const
{
	ValidateLightCache();
	return LightCellRef(X, Y);
}

uint32 ABRWorld::GetLightGridFingerprint(int32 Half) const
{
	ValidateLightCache();
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
			const FBRLightCell& C = LightCellRef(X, Y);
			Feed(C.EdgeE | (C.EdgeN << 2) | (C.bPillar ? 16 : 0) | (C.bSolid ? 32 : 0) | (C.bLamp ? 64 : 0) | (C.bFlicker ? 128 : 0));
			if (C.bLamp)
			{
				Feed(static_cast<int32>(FMath::FloorToDouble(C.Anchor.X + 0.5)));
				Feed(static_cast<int32>(FMath::FloorToDouble(C.Anchor.Y + 0.5)));
			}
		}
	}
	return H;
}

bool ABRWorld::IsLampVisibleFrom(const FIntPoint& LampCell, const FVector& P) const
{
	ValidateLightCache();
	const FBRLightCell C = LightCellRef(LampCell.X, LampCell.Y);
	return C.bLamp && BRLight::Occlusion::Visible(FLightGridView(*this), LightGeom, BRWorldLightVec(C.Emitter), BRWorldLightVec(P));
}

ABRWorld::FLightResult ABRWorld::CeilingLightAt(const FVector& P) const
{
	ValidateLightCache();
	FBRLightStats& Now = LightStatsNow();
	if (const FLightResult* Found = LightResults.Find(P))
	{
		++Now.CacheHits;
		++LightTotal.CacheHits;
		return *Found;
	}
	const FBRLevelDef& D = Def();
	BRLight::Occlusion::FStats St;
	FLightResult R;
	BRLight::Occlusion::CeilingSum(FLightGridView(*this), LightGeom, D.LightRadius, BRWorldLightVec(P), R.All, R.Visible, &St);
	++Now.Computes;
	++LightTotal.Computes;
	Now.Lamps += St.Lamps;
	Now.Hidden += St.Hidden;
	Now.Boxes += St.Boxes;
	LightTotal.Lamps += St.Lamps;
	LightTotal.Hidden += St.Hidden;
	LightTotal.Boxes += St.Boxes;
	if (LightResults.Num() >= BRWorldLightMaxResults)
	{
		LightResults.Reset();
	}
	LightResults.Add(P, R);
	return R;
}

float ABRWorld::MissionLightAt(const FVector& P, const AActor* Target, FBRLightSample* Sample) const
{
	UWorld* InWorld = GetWorld();
	if (GameplayLights.Num() == 0 || !InWorld)
	{
		return 0.f;
	}
	FBRLightStats& Now = LightStatsNow();
	const double Time = InWorld->GetTimeSeconds();
	const FIntVector Spot(FMath::RoundToInt(P.X / BRWorldLightTraceStep), FMath::RoundToInt(P.Y / BRWorldLightTraceStep),
		FMath::RoundToInt(P.Z / BRWorldLightTraceStep));
	float Sum = 0.f;
	for (const TPair<uint64, FGameplayLight>& Pair : GameplayLights)
	{
		const FGameplayLight& L = Pair.Value;
		const float Dist = static_cast<float>(FVector::Dist(P, L.Pos));
		const float Add = BRLight::Contribution(Dist, L.Radius, L.Intensity);
		if (Add <= 0.01f)
		{
			continue;
		}
		// v4.16 : canal WorldStatic sans les capsules (Pawn) ni la cible : seuls murs et portes physiques arretent la lumiere
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BRGameplayLight), false);
		if (const AActor* LightActor = L.Owner.Get())
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
			// Diagnostic : traces fraiches, avec le filtre v4.16 puis avec celui d'avant (seul l'emetteur ignore)
			++Sample->MissionSources;
			FHitResult Hit;
			bVisible = !InWorld->LineTraceSingleByChannel(Hit, L.Pos, P, ECC_WorldStatic, Q, BRWorldLightResponse());
			if (!bVisible)
			{
				Sample->MissionHits += (Sample->MissionHits.IsEmpty() ? TEXT("") : TEXT(", ")) + BRWorldLightHitName(Hit);
			}
			FCollisionQueryParams Old(SCENE_QUERY_STAT(BRGameplayLightOld), false);
			if (const AActor* LightActor = L.Owner.Get())
			{
				Old.AddIgnoredActor(LightActor);
			}
			FHitResult OldHit;
			if (InWorld->LineTraceSingleByChannel(OldHit, L.Pos, P, ECC_WorldStatic, Old))
			{
				Sample->MissionOldHits += (Sample->MissionOldHits.IsEmpty() ? TEXT("") : TEXT(", ")) + BRWorldLightHitName(OldHit);
			}
			else
			{
				Sample->MissionOldFilter += Add;
			}
			++Now.Traces;
			++LightTotal.Traces;
		}
		else
		{
			const FLightTraceKey Key{ Pair.Key, Spot, Target ? Target->GetUniqueID() : 0u };
			FLightTrace* Cached = LightTraces.Find(Key);
			if (Cached && Time - Cached->Time <= BRWorldLightTraceTTL && Time >= Cached->Time)
			{
				bVisible = Cached->bVisible;
				++Now.TraceHits;
				++LightTotal.TraceHits;
			}
			else
			{
				bVisible = !InWorld->LineTraceTestByChannel(L.Pos, P, ECC_WorldStatic, Q, BRWorldLightResponse());
				++Now.Traces;
				++LightTotal.Traces;
				if (!Cached)
				{
					if (LightTraces.Num() >= BRWorldLightMaxTraces)
					{
						LightTraces.Reset();
					}
					Cached = &LightTraces.Add(Key);
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

float ABRWorld::LightLevelAt(const FVector& P, const AActor* Target) const
{
	const FBRLevelDef& D = Def();
	if (D.bOutdoor && (D.Sky == EBRSky::Day || D.Sky == EBRSky::Overcast))
	{
		return 1.f;
	}
	FBRLightStats& Now = LightStatsNow();
	++Now.Queries;
	++LightTotal.Queries;
	// Plafonniers du niveau dont la lumiere atteint P (grille), au courant vu par les regles
	float Acc = 0.f;
	const float Pow = GetGameplayPower();
	if (D.Fixture != EBRFixture::None && Pow >= 0.05f)
	{
		Acc = CeilingLightAt(P).Visible * Pow;
	}
	// v4.12 : sources de mission allumees (balises, sortie eclairee, porche) : autonomes, meme loi que les plafonniers, en
	// 3D ; un mur ou une porte fermee entre la source et le point l'annule (v4.16 : pas une capsule)
	Acc += MissionLightAt(P, Target, nullptr);
	return FMath::Clamp(Acc, 0.f, 1.5f);
}

FBRLightSample ABRWorld::SampleLight(const FVector& P, const AActor* Target, bool bDiagnose) const
{
	FBRLightSample S;
	const FBRLevelDef& D = Def();
	S.Power = GetGameplayPower();
	S.VisualPower = Power;
	if (D.bOutdoor && (D.Sky == EBRSky::Day || D.Sky == EBRSky::Overcast))
	{
		S.Value = 1.f;
		return S;
	}
	ValidateLightCache();
	if (D.Fixture != EBRFixture::None)
	{
		BRLight::Occlusion::FStats St;
		BRLight::Occlusion::CeilingSum(FLightGridView(*this), LightGeom, D.LightRadius, BRWorldLightVec(P), S.CeilingAll, S.CeilingVisible, &St);
		S.Lamps = St.Lamps;
		S.LampsHidden = St.Hidden;
		S.Boxes = St.Boxes;
	}
	float Acc = (D.Fixture != EBRFixture::None && S.Power >= 0.05f) ? S.CeilingVisible * S.Power : 0.f;
	if (bDiagnose)
	{
		S.Mission = MissionLightAt(P, Target, &S);
	}
	else
	{
		S.Mission = MissionLightAt(P, Target, nullptr);
		S.MissionOldFilter = S.Mission;
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
			LightTraces.Reset(); // v4.16 : traces en cache de cette source perimees
		}
		return;
	}
	FGameplayLight& L = GameplayLights.FindOrAdd(Key);
	if (!L.Pos.Equals(Where, 0.5) || L.Radius != Radius)
	{
		LightTraces.Reset(); // v4.16 : source deplacee (battant, mecanisme) : traces a refaire
	}
	L.Pos = Where;
	L.Radius = Radius;
	L.Intensity = Intensity;
	L.Owner = LightOwner;
}
