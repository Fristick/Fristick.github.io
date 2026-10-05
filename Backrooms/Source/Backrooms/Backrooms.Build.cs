using UnrealBuildTool;

public class Backrooms : ModuleRules
{
	public Backrooms(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule"
		});

		// FImage : lecture des textures de RawAssets/ quand l'import Python n'a pas ete fait
		// Slate : champ de saisie de l'adresse IP ; Sockets : adresse IP locale affichee a l'hote
		// RHI : envoi de la simulation de l'eau a sa texture (BRWaterSim) ; RenderCore : temps par image (test automatique)
		// v4.9 : dependances directes declarees (et non obtenues par un autre module) : AssetRegistry (prechargement,
		// BRAssets.cpp), ApplicationCore (modes de fenetre, BRDisplay.cpp)
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ApplicationCore",
			"AssetRegistry",
			"ImageCore",
			"ImageWrapper",
			"RenderCore",
			"RHI",
			"Slate",
			"SlateCore",
			"Sockets"
		});
	}
}
