using UnrealBuildTool;
public class AlbertCanalSwap : ModuleRules
{
	public AlbertCanalSwap(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "XRBase", "HTTP", "Json", "JsonUtilities" });
	}
}
