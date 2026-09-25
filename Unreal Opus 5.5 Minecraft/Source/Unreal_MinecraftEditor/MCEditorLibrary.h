// Editor-only procedural asset generation: the /Game assets the runtime looks for.
//
// The runtime synthesises its own textures (MCVoxelRenderer::InitMaterials), so the editor
// side only has to author the material graphs that consume them, the default map, and the
// import of the Blender-authored hero meshes.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MCEditorLibrary.generated.h"

class UMaterial;
class UMaterialExpression;
class UMaterialInterface;

UENUM()
enum class EMCOpus55Layer : uint8
{
	Opaque, Cutout, Translucent, Water, Lava, Portal, EndPortal
};

UCLASS()
class UNREAL_MINECRAFTEDITOR_API UMCEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Everything the runtime loads from /Game. Returns a short log of what happened. */
	UFUNCTION(BlueprintCallable, Category = "Opus 5.5|Content")
	static FString BuildAllContent();

	// ---- materials
	UFUNCTION(BlueprintCallable, Category = "Opus 5.5|Content")
	static UMaterial* BuildVoxelMaterial(EMCOpus55Layer Layer, bool bItemVariant);

	UFUNCTION(BlueprintCallable, Category = "Opus 5.5|Content")
	static UMaterial* BuildEntityMaterial();

	UFUNCTION(BlueprintCallable, Category = "Opus 5.5|Content")
	static UMaterial* BuildGlowMaterial();

	UFUNCTION(BlueprintCallable, Category = "Opus 5.5|Content")
	static UMaterial* BuildBeamMaterial();

	UFUNCTION(BlueprintCallable, Category = "Opus 5.5|Content")
	static UMaterial* BuildSkyMaterial();

	UFUNCTION(BlueprintCallable, Category = "Opus 5.5|Content")
	static UMaterial* BuildCloudMaterial();

	// ---- map
	/** Author /Game/Opus55Minecraft/Maps/L_Opus55World with the environment actors the game expects. */
	UFUNCTION(BlueprintCallable, Category = "Opus 5.5|Content")
	static bool BuildDefaultMap();

	// ---- imported hero meshes
	/** Imports every FBX under Saved/Opus55Fbx into /Game/Opus55Minecraft/{Mobs,Items,Entities}. */
	UFUNCTION(BlueprintCallable, Category = "Opus 5.5|Content")
	static int32 ImportHeroMeshes();

	/** Verification pass: every asset path the runtime requests, with a pass/fail line each. */
	UFUNCTION(BlueprintCallable, Category = "Opus 5.5|Content")
	static FString VerifyContent();

	/** First vertex colour of a static mesh as stored in the mesh description and in the GPU buffer. */
	UFUNCTION(BlueprintCallable, Category = "Opus 5.5|Content")
	static FString ProbeMeshColors(const FString& MeshPath);

	/** Asset paths the runtime loads, as reported by the game code. */
	UFUNCTION(BlueprintCallable, Category = "Opus 5.5|Content")
	static TArray<FString> RequiredMaterialPaths();

	UFUNCTION(BlueprintCallable, Category = "Opus 5.5|Content")
	static TArray<FString> RequiredMobRigNames();
};
