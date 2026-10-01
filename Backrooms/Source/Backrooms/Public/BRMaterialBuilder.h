// Construction des materiaux maitres a la volee (editeur uniquement).
// Meme graphe que Content/Python/backrooms_setup.py : sert de secours si l'import Python n'a pas
// cree les materiaux (plugin desactive, erreur d'import, ancienne version...).
#pragma once

#include "CoreMinimal.h"

class UMaterial;
class UTexture;

enum class EBRMasterMaterial : uint8
{
	World,  // murs/sols/plafonds : triplanaire, normal maps, salete, caustiques
	Mesh,   // modeles Blender
	Skin,   // peau / chair (subsurface)
	Water   // eau Single Layer Water
};

namespace BRMaterialBuilder
{
	/** true si l'executable peut compiler des materiaux (editeur, PIE, -game non cuisine) */
	bool IsAvailable();

	/**
	 * Construit le materiau. LoadTexture fournit les textures par defaut (T_Grime, T_Caustics...) ; elle peut
	 * renvoyer nullptr (les textures du moteur sont alors utilisees). Retourne nullptr hors editeur.
	 */
	UMaterial* Build(EBRMasterMaterial Which, UObject* Outer, TFunctionRef<UTexture*(FName)> LoadTexture);
}
