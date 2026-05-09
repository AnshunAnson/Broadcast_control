using UnrealBuildTool;
using System.Collections.Generic;

public class BrodcastMVPTarget : TargetRules
{
	public BrodcastMVPTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.Add("BrodcastMVP");
	}
}