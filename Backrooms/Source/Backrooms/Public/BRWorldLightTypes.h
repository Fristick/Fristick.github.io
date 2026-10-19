// v4.16 : types de la lumiere de gameplay (ABRWorld::LightLevelAt, BRWorldLight.cpp ; prompt v4.16, 3.1 et 3.2) : cellule de
// la grille vue par l'occlusion des plafonniers, detail d'un releve (tests), compteurs du cout et caches. Dans un en-tete a
// part (relecture v4.16) : BRWorld.h ne garde que les declarations de l'interface et un seul membre (FBRLightCache).
#pragma once

#include "CoreMinimal.h"
#include "BRLightLogic.h"

class AActor;

/** v4.16 : cellule de la grille vue par la lumiere de gameplay (BRWorldLight.cpp, en cache par chunk) : aretes Est et Nord
 *  (EBREdge ; une porte de mission fermee sur une ouverture de la grille compte comme un mur), pilier du sommet (+X, +Y),
 *  bloc d'une cellule pleine, plafonnier allume (present, pas casse, dans l'enceinte, pas sous une trappe d'echelle) */
struct FBRLightCell
{
	uint8 EdgeE = 0;
	uint8 EdgeN = 0;
	bool bPillar = false;
	bool bSolid = false;
	bool bLamp = false;
	bool bFlicker = false;
	/** v4.16 (relecture) : plafonnier de la grille que le chunk ne pose pas (a moins de BRLight::HatchMargin cm de la trappe
	 *  d'une echelle de sortie, ABRChunk, etape des lumieres) : bLamp est faux, l'ancre, le point d'emission et bFlicker
	 *  restent remplis (l'empreinte decrit la grille : elle compte ce plafonnier, comme le banc) */
	bool bUnderHatch = false;
	/** Centre de la cellule + decalage du plafonnier : la distance de la loi d'apport (inchangee) */
	FVector Anchor = FVector::ZeroVector;
	/** Point d'emission (cotes d'ABRChunk::AddLight) : l'occlusion part de la */
	FVector Emitter = FVector::ZeroVector;
	FVector SolidLo = FVector::ZeroVector;
	FVector SolidHi = FVector::ZeroVector;
};

/** v4.16 (tests, diagnostic) : detail de la lumiere de gameplay en un point (ABRWorld::SampleLight) */
struct FBRLightSample
{
	/** ABRWorld::LightLevelAt */
	float Value = 0.f;
	/** Alimentation vue par les regles (BRLight::PhasePower) et alimentation de l'image (vacillement local) */
	float Power = 1.f;
	float VisualPower = 1.f;
	/** v4.16 (relecture) : noeud du reseau de BRLight::SnapStep cm ou les plafonniers sont evalues (le plus proche du point) */
	FVector Node = FVector::ZeroVector;
	/** Plafonniers a portee dont la lumiere atteint le noeud / tous (la regle d'avant la v4.16, murs ignores), avant
	 *  l'alimentation */
	float CeilingVisible = 0.f;
	float CeilingAll = 0.f;
	int32 Lamps = 0;
	int32 LampsHidden = 0;
	/** Sources de mission en ligne de vue (filtre v4.16 : ni capsules ni cible) */
	float Mission = 0.f;
	/** Diagnostic (bDiagnose) : filtre d'avant la v4.16 (seul l'emetteur ignore : la capsule de la cible elle-meme arrete
	 *  la trace qui finit en son centre) */
	float MissionOldFilter = 0.f;
	/** Diagnostic (bDiagnose, relecture v4.16) : emetteur et cible ignores, capsules des autres personnages et creatures
	 *  comprises : isole l'effet d'un coequipier ou d'une creature sur le rayon */
	float MissionPawnFilter = 0.f;
	int32 MissionSources = 0;
	/** Obstacles touches par les traces des sources de mission (v4.16, filtre d'avant, capsules comprises) : "acteur.composant" */
	FString MissionHits;
	FString MissionOldHits;
	FString MissionPawnHits;
	/** Acteur touche par chaque trace coupee (filtre d'avant ; capsules comprises sans la cible) */
	TArray<TWeakObjectPtr<const AActor>> MissionOldHitActors;
	TArray<TWeakObjectPtr<const AActor>> MissionPawnHitActors;
	/** Obstacles de la grille testes */
	int32 Boxes = 0;
};

/** v4.16 : compteurs de la lumiere de gameplay (cout : appels, cache, plafonniers, obstacles, traces) */
struct FBRLightStats
{
	int32 Queries = 0;
	/** Resultat des plafonniers repris du cache (meme noeud du reseau) / calcule */
	int32 CacheHits = 0;
	int32 Computes = 0;
	int32 Lamps = 0;
	int32 Hidden = 0;
	int32 Boxes = 0;
	/** Traces des sources de mission lancees / reprises du cache */
	int32 Traces = 0;
	int32 TraceHits = 0;
	/** Chunks de la grille mis en cache ; caches vides (niveau, disposition, portes de mission) */
	int32 ChunkBuilds = 0;
	int32 Invalidations = 0;
	/** Images comptees */
	int32 Frames = 0;
	void Add(const FBRLightStats& O)
	{
		Queries += O.Queries; CacheHits += O.CacheHits; Computes += O.Computes; Lamps += O.Lamps; Hidden += O.Hidden; Boxes += O.Boxes;
		Traces += O.Traces; TraceHits += O.TraceHits; ChunkBuilds += O.ChunkBuilds; Invalidations += O.Invalidations; Frames += O.Frames;
	}
};

/** v4.16 : caches de la lumiere de gameplay (un membre d'ABRWorld, BRWorldLight.cpp) : grille par chunk, plafonniers par noeud
 *  du reseau, traces des sources de mission ; valides pour un niveau, une graine, un chargement, une disposition de la
 *  mission et un etat des portes de mission */
struct FBRLightCache
{
	struct FChunk
	{
		TArray<FBRLightCell> Cells;
		/** v4.16 (relecture) : trappe de l'echelle de sortie de ce chunk (une au plus, comme ABRChunk::PlanExits) */
		bool bHatch = false;
		BRLight::FHatch Hatch;
	};
	struct FResult
	{
		float Visible = 0.f;
		float All = 0.f;
	};
	struct FTraceKey
	{
		uint64 Light = 0;
		FIntVector Spot = FIntVector::ZeroValue;
		uint32 Target = 0;
		bool operator==(const FTraceKey& O) const { return Light == O.Light && Spot == O.Spot && Target == O.Target; }
		friend uint32 GetTypeHash(const FTraceKey& K) { return HashCombine(HashCombine(GetTypeHash(K.Light), GetTypeHash(K.Spot)), K.Target); }
	};
	struct FTrace
	{
		double Time = 0.0;
		bool bVisible = true;
	};
	TMap<FIntPoint, FChunk> Chunks;
	FIntPoint MemoChunk = FIntPoint(MAX_int32, MAX_int32);
	const FChunk* Memo = nullptr;
	/** Cle : noeud du reseau (BRLight::SnapIndex des trois coordonnees) */
	TMap<FIntVector, FResult> Results;
	TMap<FTraceKey, FTrace> Traces;
	/** Aretes fermees (portes de mission sur une ouverture de la grille, et celles du crochet de test) : cle X * 2 + (Est), Y */
	TSet<FIntPoint> ClosedEdges;
	/** v4.16 (relecture, tests) : aretes fermees par ABRWorld::SetLightTestClosedEdge, pour le chargement TestEdgesSerial */
	TSet<FIntPoint> TestClosedEdges;
	int32 TestEdgesSerial = -1;
	BRLight::Occlusion::FGeom Geom;
	bool bValid = false;
	uint32 Seed = 0;
	int32 Serial = -1;
	int32 Level = MIN_int32;
	/** v4.16 (relecture) : disposition de la mission (version, variante, parcours, cellules et sorties reservees) : les trappes
	 *  des echelles des chunks en dependent (ABRWorld::IsChunkExitDrawn, PickChunkWallFace) */
	uint32 MissionKey = 0;
	int32 MissionCells = -1;
	int32 MissionExits = -1;
	uint16 Rev = 0;
	FBRLightStats Frame;
	FBRLightStats Last;
	FBRLightStats Total;
	uint64 StatsFrameNo = 0;
};
