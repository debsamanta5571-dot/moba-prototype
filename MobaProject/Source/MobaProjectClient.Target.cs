using UnrealBuildTool;
using System.Collections.Generic;

public class MobaProjectClientTarget : TargetRules
{
	public MobaProjectClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Client;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		ExtraModuleNames.AddRange(new string[] { "MobaProject" });
	}
}
