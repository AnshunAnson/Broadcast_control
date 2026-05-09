using UnrealBuildTool;
using System.Collections.Generic;

public class BrodcastMVPEditorTarget : TargetRules
{
	public BrodcastMVPEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.Add("BrodcastMVP");
	}
}