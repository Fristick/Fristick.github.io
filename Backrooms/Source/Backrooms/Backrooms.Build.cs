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
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ImageCore",
			"ImageWrapper",
			"Slate",
			"SlateCore",
			"Sockets"
		});
	}
}
