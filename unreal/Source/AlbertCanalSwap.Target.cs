using UnrealBuildTool;
public class AlbertCanalSwapTarget : TargetRules
{
	public AlbertCanalSwapTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("AlbertCanalSwap");
	}
}
