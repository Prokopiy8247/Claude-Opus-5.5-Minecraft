using UnrealBuildTool;

public class Unreal_MinecraftEditor : ModuleRules
{
	public Unreal_MinecraftEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "Unreal_Minecraft"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd", "AssetTools", "AssetRegistry", "ImageWrapper", "ImageCore",
			"AudioEditor", "RenderCore", "RHI", "Slate", "SlateCore", "Json", "MaterialEditor", "EditorScriptingUtilities", "Projects", "MeshDescription", "StaticMeshDescription"
		});
	}
}
