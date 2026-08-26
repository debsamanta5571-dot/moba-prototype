using UnrealBuildTool;
using System.Collections.Generic;

public class MobaProjectServerTarget : TargetRules
{
	public MobaProjectServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		ExtraModuleNames.AddRange(new string[] { "MobaProject" });
		DisablePlugins.Add("CommonConversation");
		DisablePlugins.Add("ConversationToolset");
	}
}
