// Vagues a la surface de l'eau : equation des ondes resolue sur une grille de 19 m qui suit le joueur.
// Les joueurs et les entites fendent l'eau (sillage), les plongeons et les gouttes font des ronds, les murs
// et les piliers renvoient les vagues. Le resultat (pentes de la surface) est envoye chaque pas a une texture
// lue par le materiau de l'eau (reflets, refraction) et par le carrelage (caustiques).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BRWaterSim.generated.h"

class ABRWorld;
class UTexture2D;

UCLASS()
class BACKROOMS_API UBRWaterSim : public UObject
{
	GENERATED_BODY()

public:
	/** Taille d'une case de la grille (cm) et nombre de cases par cote : 6 cm x 320 = 19,2 m */
	static constexpr float Texel = 6.f;
	static constexpr int32 GridRes = 320;

	/** Eau immobile, murs a relire (nouveau niveau) */
	void Reset();
	/** Rond dans l'eau : creux de Depth cm sur un rayon de Radius cm (plongeon, pas, goutte) */
	void AddImpulse(const FVector2D& Pos, float Radius, float Depth);
	/**
	 * Corps qui fend l'eau pendant cette image. Dir (unitaire) : sens de la marche, l'eau monte devant le corps et se
	 * creuse derriere, de Push cm par seconde. Dir nul : la surface monte ou descend sur place (Push signe).
	 */
	void AddMover(const FVector2D& Pos, const FVector2D& Dir, float Radius, float Push);
	/** Avance la simulation par pas fixes ; la grille se recentre sur Focus (le joueur) */
	void Tick(float Dt, const FVector& Focus, const ABRWorld* World);

	UTexture2D* GetTexture() const { return Texture; }
	/** Mesures (test automatique) : plus haute vague (cm) et temps de calcul de la derniere image (ms) */
	float GetPeak() const;
	float GetLastTickMs() const { return LastTickMs; }
	/** Parametre WaterSimWindow des materiaux : centre X, centre Y, cote de la zone (cm), intensite */
	FLinearColor GetWindow() const;

private:
	struct FStamp
	{
		FVector2f Pos;
		float Radius;
		float Amount;
	};

	void Step(float StepDt);
	void Stamp(const FStamp& S, float Scale);
	/** Recentre la grille : les cases qui entrent dans la zone repartent d'une eau calme */
	void Recenter(const FIntPoint& NewOrigin, const ABRWorld* World);
	void ResetCell(int32 GX, int32 GY, const ABRWorld* World);
	/** Un mur, un pilier ou une cellule pleine occupe ce point du monde */
	bool IsBlocked(float X, float Y, const ABRWorld* World);
	uint8 CellBits(int32 X, int32 Y, const ABRWorld* World);
	void Upload();

	static int32 Wrap(int32 V) { return ((V % GridRes) + GridRes) % GridRes; }

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Texture;

	/** Hauteur de la surface (cm) maintenant et au pas precedent, rangees en tore : case monde G -> G mod GridRes */
	TArray<float> H;
	TArray<float> HPrev;
	/** 1 = eau libre, 0 = obstacle */
	TArray<uint8> Open;
	/** Case monde du coin bas-gauche de la zone simulee */
	FIntPoint Origin = FIntPoint::ZeroValue;
	bool bValid = false;
	float Accum = 0.f;
	float LastTickMs = 0.f;
	TArray<FStamp> Movers;
	TArray<FStamp> Impulses;
	/** Murs des cellules du niveau deja lus (cle : cellule) */
	TMap<FIntPoint, uint8> CellCache;
};
