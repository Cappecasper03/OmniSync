using UnrealBuildTool;

public class OmniSync : ModuleRules
{
	public OmniSync(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
		[
			"Core"
		]);

		PrivateDependencyModuleNames.AddRange(
		[
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"ToolMenus",
			"EditorStyle",
			"EditorFramework",
			"Projects",
			"Json",
			"JsonUtilities",
			"DeveloperSettings",
			"DirectoryWatcher"
		]);
	}
}