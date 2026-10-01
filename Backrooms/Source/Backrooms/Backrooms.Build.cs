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
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ImageCore",
			"ImageWrapper"
		});
	}
}
