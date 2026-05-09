using UnrealBuildTool;

public class VPBroadcast : ModuleRules
{
    public VPBroadcast(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "Engine",
            "Sockets",
            "Networking",
            "Json",
            "JsonUtilities",
            "HTTP",
            "MaterialShaderQualitySettings",
            "Niagara",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CoreUObject",
            "Slate",
            "SlateCore",
        });
    }
}