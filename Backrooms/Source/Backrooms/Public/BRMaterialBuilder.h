// Construction des materiaux maitres a la volee (editeur uniquement).
// Meme graphe que Content/Python/backrooms_setup.py : sert de secours si l'import Python n'a pas
// cree les materiaux (plugin desactive, erreur d'import, ancienne version...).
#pragma once

#include "CoreMinimal.h"

class UMaterial;
class UTexture;

enum class EBRMasterMaterial : uint8
{
	World,       // murs/sols/plafonds : triplanaire, normal maps, salete, caustiques
	Mesh,        // modeles Blender
	Skin,        // peau / chair (subsurface)
	WaterSurface // eau translucide : refraction, absorption selon la profondeur, vagues simulees
};

namespace BRMaterialBuilder
{
	/** Code HLSL des noeuds Custom, identique a celui du script Python */
	/** Houle et clapot de fond (P, T, Amp, Chop) -> float3(pente X, pente Y, hauteur) */
	const FString& WaterSurfaceHLSL();
	/** Vagues simulees autour du joueur (P, Win, SimTex) -> float2(pente X, pente Y) */
	const FString& WaterSimHLSL();
	/** Decalage de l'image du fond par la refraction (VN, PixD, D0, Strength) -> float2 */
	const FString& WaterRefractHLSL();
	/** Lumiere qui traverse l'eau : Fresnel, absorption selon le trajet, vue de dessous -> float4(rgb, voile) */
	const FString& WaterShadeHLSL();
	/** Caustiques sur le carrelage, deformees par les vagues simulees -> multiplicateur de couleur */
	const FString& CausticsHLSL();
	/** Flaques et sol mouille (WP, N, T, Amount, Wet, Tex) -> float4(flaque, mouille, pente XY des gouttes) */
	const FString& PuddlesHLSL();

	/** true si l'executable peut compiler des materiaux (editeur, PIE, -game non cuisine) */
	bool IsAvailable();

	/**
	 * Construit le materiau. LoadTexture fournit les textures par defaut (T_Grime, T_Caustics...) ; elle peut
	 * renvoyer nullptr (les textures du moteur sont alors utilisees). Retourne nullptr hors editeur.
	 */
	UMaterial* Build(EBRMasterMaterial Which, UObject* Outer, TFunctionRef<UTexture*(FName)> LoadTexture);
}
