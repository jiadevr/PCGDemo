using UnrealBuildTool;

public class CityGenerator : ModuleRules
{
	public CityGenerator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"JIAPCGAidTool"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UMG",
				"UnrealEd",
				"EditorSubsystem",
				"AssetTools",
				"EditorScriptingUtilities",
				"Json",
				"JsonUtilities",
				"SubobjectDataInterface"
			}
		);
	}
}