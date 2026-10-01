using UnrealBuildTool;
using System.Collections.Generic;

public class BackroomsTarget : TargetRules
{
	public BackroomsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Backrooms");
	}
}
