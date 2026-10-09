using UnrealBuildTool;
public class AlbertCanalSwapEditorTarget : TargetRules
{
	public AlbertCanalSwapEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("AlbertCanalSwap");
	}
}
