// v4.11 : missions de niveau dans le jeu : mecanismes places dans le niveau (ABRMissionDevice) et textes composes dans
// la langue de chaque joueur (BRMissionText). La logique (regles, etapes, sorties) est dans BRMissionLogic.h ; l'etat
// est tenu par l'hote (ABRWorld) et replique a tous.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BRMissionLogic.h"
#include "BRTypes.h"
#include "BRMission.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class ABRCharacter;

/** Mecanisme de mission (panneau, cadran, vanne, prise, cle, manivelle, bouton, porte...). Cree localement chez chaque
 *  joueur d'apres le plan de mission (meme graine, meme placement) ; son etat vient de l'etat replique par l'hote. Aucun
 *  Tick permanent : le monde active le Tick des seuls mecanismes animes proches d'un joueur. */
UCLASS()
class BACKROOMS_API ABRMissionDevice : public AActor
{
	GENERATED_BODY()

public:
	ABRMissionDevice();

	/** Construit l'aspect du mecanisme d'apres son role ; Index : indice dans le plan */
	void Init(int32 InIndex, const BRMission::FPlan& Plan, const FBRMissionSpot& Spot);
	/** Etat replique recu : position, rempli, pris, progression ; Gate : ouverture (0..255) des elements visibles */
	void ApplyState(const BRMission::FPlan& Plan, const BRMission::FState& State, const BRMission::FEval& Eval, bool bAnimate);
	/** Son et lueur breve quand l'hote annonce un changement de ce mecanisme */
	void PlayFeedback(uint8 Feedback);
	virtual void Tick(float DeltaSeconds) override;

	/** Peut etre vise (les elements visibles seuls, portes et passages, ne s'utilisent pas) */
	bool IsInteractable() const;
	/** Action maintenue (observation, manivelle) */
	bool IsHoldAction() const;
	/** Texte de l'invite (nom, etat, action) dans la langue du joueur */
	FString GetPrompt() const;
	/** Point vise et verifie par l'hote (distance 3D, ligne de vue) */
	FVector GetInteractPoint() const;
	/** Cellule de la grille */
	FIntPoint GetCell() const { return Cell; }
	int32 GetIndex() const { return Index; }
	/** Le mecanisme doit-il s'animer (neon qui clignote, balise, vapeur) quand un joueur est proche */
	bool WantsTick() const { return bAnimated; }
	/** v4.11 : machine alimentee en marche (relais, disjoncteur, generateur, treuil, balise) : le Clump s'en ecarte */
	bool IsRunning() const;
	/** Visible (son chunk est construit). v4.12 : masque, il ne s'anime plus (Tick coupe), son etat logique reste ; a la
	 *  reapparition, il reprend la bonne position sans rejouer d'animation */
	void SetShown(bool bShow);
	bool IsShown() const { return bShown; }
	/** v4.12 (mesures) : Ticks des mecanismes et temps passe (secondes) depuis la derniere remise a zero */
	static int64 TickCount;
	static double TickSeconds;
	static void ResetTickStats()
	{
		TickCount = 0;
		TickSeconds = 0.0;
	}

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	/** Zone visee (canal Visibility) */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> Box;

	/** Corps bloquant (portes et passages fermes) */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> Blocker;

	/** Toutes les pieces du mecanisme */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	/** Pieces mobiles : aiguille, levier, molette, volet ; lampe d'etat */
	UPROPERTY()
	TObjectPtr<USceneComponent> Moving;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MovingMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> LampMID;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GlowMID;

	/** Inscriptions universelles (lettres, chiffres, angles) */
	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> Labels;

	/** Balises du Niveau 6 : seule lumiere du mecanisme, sans ombre */
	UPROPERTY()
	TObjectPtr<UPointLightComponent> BeaconLight;

	/** v4.12 : lumiere reelle d'un element eclaire par la mission (sortie de secours du Niveau 6, porche du Niveau 9) */
	UPROPERTY()
	TObjectPtr<UPointLightComponent> GateLight;

	/** v4.12 : sources de lumiere de gameplay de ce mecanisme (balise, element eclaire), d'apres son etat */
	void UpdateGameplayLights();

	/** Piece affichee quand l'objet est present (fusible, cle) ou insere (prise) */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ItemMesh;

	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, const FVector& Size, UMaterialInterface* Mat,
		USceneComponent* Parent = nullptr);
	UStaticMeshComponent* AddBoxPart(const FVector& Loc, const FVector& Size, UMaterialInterface* Mat, USceneComponent* Parent = nullptr,
		const FRotator& Rot = FRotator::ZeroRotator);
	/** Symbole en relief (triangle, cercle, carre, losange, croix, etoile), dans le plan YZ local */
	void AddSymbol(uint8 Symbol, const FVector& Center, float Size, UMaterialInterface* Mat, USceneComponent* Parent = nullptr);
	/** Inscription en caracteres universels (A-E, 0-9, degres) */
	void AddLabel(const FString& Text, const FVector& Loc, float Size, const FColor& Color, USceneComponent* Parent = nullptr);
	void BuildWallPlate(float W, float H, UMaterialInterface* Mat);

	int32 Index = -1;
	BRMission::FDevice Device;
	int32 Level = -1;
	FIntPoint Cell = FIntPoint::ZeroValue;
	bool bWall = true;
	bool bAnimated = false;
	bool bShown = true;
	/** Porte ou passage qui bloque physiquement tant qu'il est ferme */
	bool bBlocking = false;
	uint8 StateValue = 0;
	uint8 GateValue = 0;
	bool bSolved = false;
	bool bWarning = false;
	bool bLocked = false;
	float Time = 0.f;
	float FlashTime = -10.f;
	/** Position affichee (interpolee) et visee */
	float ShownPos = 0.f;
	float TargetPos = 0.f;
	/** Niveau 0 : clignotements de l'anomalie (serie, puis pause) */
	uint8 Blinks = 0;
};

namespace BRMissionText
{
	/** Nom d'un symbole dans la langue du joueur */
	BACKROOMS_API FString Symbol(uint8 Sym);
	/** Lettre A-E */
	BACKROOMS_API FString Letter(uint8 L);
	/** Nom d'un mecanisme (avec son inscription) */
	BACKROOMS_API FString DeviceName(const BRMission::FPlan& Plan, int32 Device);
	/** Etat lisible d'un mecanisme (position, marche/arret, progression), vide si rien a dire */
	BACKROOMS_API FString DeviceState(const BRMission::FPlan& Plan, const BRMission::FState& State, int32 Device);
	/** Lignes d'un indice ou d'une observation */
	BACKROOMS_API void ClueLines(const BRMission::FClue& Clue, TArray<FString>& Out);
	/** Titre d'une etape */
	BACKROOMS_API FString StepTitle(int32 Level, int32 Step);
	/** Aide de premier niveau d'une etape (ou chercher, quoi faire), sans la solution */
	BACKROOMS_API FString StepHint(int32 Level, int32 Step);
	/** Message court d'un retour (Feedback de BRMission, ou BRMissionText::Cooldown) */
	BACKROOMS_API FString Feedback(const BRMission::FPlan& Plan, int32 Device, uint8 Feedback, uint8 Related, uint8 Count);
	/** Changement visible quand la mission est resolue */
	BACKROOMS_API FString SolvedLine(int32 Level);
	/** Objectif facultatif du niveau (vide s'il n'y en a pas) */
	BACKROOMS_API FString OptionalLine(int32 Level);
	/** Retours propres au jeu (en plus de BRMission::EFeedback) */
	constexpr uint8 Cooldown = 200;
	constexpr uint8 TooFar = 201;
	constexpr uint8 NotNow = 202;
	constexpr uint8 Taken = 203;
}
