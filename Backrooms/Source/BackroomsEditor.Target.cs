using UnrealBuildTool;
using System.Collections.Generic;

public class BackroomsEditorTarget : TargetRules
{
	public BackroomsEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Backrooms");
	}
}
