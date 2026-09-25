// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Unreal_Minecraft : ModuleRules
{
	public Unreal_Minecraft(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Default;
		// many implementation files use anonymous namespaces with short helper names
		bUseUnity = false;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"ProceduralMeshComponent", "Slate", "SlateCore", "RenderCore", "RHI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"ApplicationCore", "AudioMixer", "AudioExtensions", "ImageWrapper", "Json", "UMG"
		});
	}
}
