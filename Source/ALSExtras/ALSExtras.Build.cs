using UnrealBuildTool;

public class ALSExtras : ModuleRules
{
	public ALSExtras(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_1;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject","GameplayTags", "Engine", "EnhancedInput", "AIModule", "ALS", "ALSCamera"
		});
	}
}