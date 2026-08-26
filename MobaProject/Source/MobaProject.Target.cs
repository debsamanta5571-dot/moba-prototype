// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class MobaProjectTarget : TargetRules
{
	public MobaProjectTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;

		ExtraModuleNames.AddRange( new string[] { "MobaProject" } );
		DisablePlugins.Add("CommonConversation");
		DisablePlugins.Add("ConversationToolset");
	}
}
