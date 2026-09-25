// Lightweight mesh component for voxel chunk geometry.
// Vertex data is built on worker threads and uploaded straight to the GPU (no CPU copy kept).
#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "MCChunkMeshComponent.generated.h"

/** Packed vertex produced by the mesher. */
struct FMCMeshVertex
{
	FVector3f Pos;
	FVector3f Normal;
	FVector3f Tangent;
	FVector2f UV0;     // texture coordinates
	FVector2f UV1;     // x = texture layer, y = flags
	FVector2f UV2;     // x = sky light 0..1, y = block light 0..1
	FColor Color;      // rgb = tint, a = ambient occlusion
	float TangentSign = 1.f;
};

struct FMCMeshLayerData
{
	TArray<FMCMeshVertex> Vertices;
	TArray<uint32> Indices;
	bool IsEmpty() const { return Indices.Num() == 0; }
};

/** Geometry for all material layers of one render group. */
struct FMCChunkMeshData
{
	static constexpr int32 NumLayers = 7;
	FMCMeshLayerData Layers[NumLayers];
	FBox Bounds = FBox(ForceInit);
	int32 NumTriangles() const
	{
		int32 N = 0;
		for (const FMCMeshLayerData& L : Layers) N += L.Indices.Num() / 3;
		return N;
	}
	bool IsEmpty() const
	{
		for (const FMCMeshLayerData& L : Layers) if (!L.IsEmpty()) return false;
		return true;
	}
};

namespace MCRender
{
	/** Terrain layer materials owned by the voxel renderer (null until the renderer exists). */
	extern UNREAL_MINECRAFT_API TArray<TObjectPtr<UMaterialInterface>>* GVoxelMaterials;
	/** Item icon materials (layer 1 samples the item icon array instead of the terrain array). */
	extern UNREAL_MINECRAFT_API TArray<TObjectPtr<UMaterialInterface>>* GItemMaterials;
}

UCLASS(ClassGroup = (Opus55), meta = (BlueprintSpawnableComponent))
class UNREAL_MINECRAFT_API UMCChunkMeshComponent : public UMeshComponent
{
	GENERATED_BODY()
public:
	UMCChunkMeshComponent(const FObjectInitializer& ObjectInitializer);

	/** Replace geometry (takes ownership). */
	void SetMeshData(TUniquePtr<FMCChunkMeshData>&& InData);
	void ClearMesh();
	bool HasGeometry() const { return bHasGeometry; }

	/** Shared layer materials (same for every chunk). */
	TArray<TObjectPtr<UMaterialInterface>>* SharedMaterials = nullptr;

	// UPrimitiveComponent
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;

	int32 LastTriangles = 0;
	/** Set when the render state was recreated after the CPU data had been consumed: owner must re-mesh. */
	bool bNeedsRebuild = false;
	/** Keep a CPU copy so the proxy can be rebuilt on any render-state recreation (small item / block visuals
	 *  have no owner that re-meshes them, unlike terrain chunks). */
	bool bRetainData = false;

private:
	TUniquePtr<FMCChunkMeshData> PendingData;
	TUniquePtr<FMCChunkMeshData> RetainedData;
	FBox LocalBounds = FBox(FVector::ZeroVector, FVector(1600, 1600, 6400));
	bool bHasGeometry = false;
};
