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
		// v4.10 : Json (rapport structure du test automatique, lu par un lanceur externe)
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ApplicationCore",
			"AssetRegistry",
			"Json",
			"ImageCore",
			"ImageWrapper",
			"RenderCore",
			"RHI",
			"Slate",
			"SlateCore",
			"Sockets"
		});

		// v4.12 : canal de contenu d'une version publiee (0 : niveaux publies ; 1 : test interne), fixe a la compilation
		// et jamais ouvert par un reglage ou la ligne de commande. Variable d'environnement au moment de la construction :
		// BR_CONTENT_CHANNEL=public (par defaut) ou internal. Les versions de developpement choisissent au lancement
		// (-BRContent=public|internal|all, tout par defaut) : voir BRLevels::Channel
		string Channel = System.Environment.GetEnvironmentVariable("BR_CONTENT_CHANNEL") ?? "public";
		int ChannelValue = Channel.Trim().ToLowerInvariant() == "internal" ? 1 : 0;
		PublicDefinitions.Add("BR_CONTENT_CHANNEL=" + ChannelValue);
	}
}
