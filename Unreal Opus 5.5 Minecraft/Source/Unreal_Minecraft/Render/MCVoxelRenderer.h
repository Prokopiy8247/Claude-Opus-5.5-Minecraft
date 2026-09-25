// Voxel renderer: owns the chunk mesh components of the active dimension, schedules async meshing,
// manages the terrain / item materials and texture arrays, block outline + crack overlay and a pool of
// dynamic point lights for nearby light-emitting blocks.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/MCCore.h"
#include "Render/MCMesher.h"
#include "Containers/Queue.h"
#include "MCVoxelRenderer.generated.h"

class FMCWorld;
class UMCChunkMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2DArray;
class UTexture2D;
class UPointLightComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;

/** Finished mesh handed from a worker to the game thread. */
struct FMCMeshResult
{
	int64 Key = 0;
	uint32 Revision = 0;
	uint64 WorldSerial = 0;
	TUniquePtr<FMCChunkMeshData> Data;
	TArray<FIntVector4> Emitters; // x, y, z (world block), w = light level
};

UCLASS()
class UNREAL_MINECRAFT_API AMCVoxelRenderer : public AActor
{
	GENERATED_BODY()
public:
	AMCVoxelRenderer();

	/** Bind to a dimension (drops all current geometry). */
	void SetWorld(FMCWorld* InWorld);
	FMCWorld* GetWorld_MC() const { return World; }
	/** Re-mesh every loaded chunk (texture pack / settings change). */
	void RebuildAll();
	/** Per-frame update: collect dirty chunks, launch meshing, apply results, update lights and materials. */
	void UpdateRenderer(float DeltaSeconds, const FVector& CameraBlocks);

	/** Block selection outline and break progress (0 = none). */
	void SetSelection(bool bShow, const FMCBlockPos& P, uint16 State, float BreakProgress);

	/** Global material parameters (sky brightness etc.) pushed to every layer material. */
	void SetGlobalParams(float SkyBrightness, float AmbientFloor, float NightVision, float TimeSeconds, float Wetness, const FLinearColor& FogColor);

	static int64 MakeKey(const FMCChunkPos& C, int32 Group) { return ((int64)(uint32)C.X << 32) | ((int64)(uint32)(C.Y & 0x1FFFFFF) << 3) | (int64)(Group & 7); }

	// ---- materials & textures (shared with item visuals through MCRender::GVoxelMaterials)
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> LayerMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> ItemMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> LayerMIDs;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> ItemMIDs;
	UPROPERTY(Transient) TObjectPtr<UTexture2DArray> AlbedoArray;
	UPROPERTY(Transient) TObjectPtr<UTexture2DArray> NormalArray;
	UPROPERTY(Transient) TObjectPtr<UTexture2DArray> OrmeArray;
	UPROPERTY(Transient) TObjectPtr<UTexture2DArray> IconArray;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> CrackAtlas;

	// ---- statistics (F3)
	int32 StatComponents = 0;
	int32 StatTriangles = 0;
	int32 StatPendingMeshes = 0;
	int32 StatMeshesThisSecond = 0;
	int32 StatLights = 0;
	double StatMeshMs = 0.0;

	/** Chunk render distance (chunks); groups outside are hidden. */
	int32 RenderDistance = 12;
	/** Number of chunk sections drawn within the last Tolerance seconds (diagnostics). */
	int32 CountDrawnSections(float Tolerance) const;

	virtual void Tick(float DeltaSeconds) override {}
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	FMCWorld* World = nullptr;
	uint64 WorldSerial = 1;
	UPROPERTY(Transient) TMap<int64, TObjectPtr<UMCChunkMeshComponent>> Groups;
	UPROPERTY(Transient) TArray<TObjectPtr<UMCChunkMeshComponent>> FreeComponents;
	TMap<int64, uint32> GroupRevision;        // latest requested revision per group
	TMap<int64, TArray<FIntVector4>> GroupEmitters;
	TSet<FMCChunkPos> PendingChunks;          // chunks with dirty sections waiting for meshing
	TSet<int64> InFlight;
	TSharedPtr<TQueue<TSharedPtr<FMCMeshResult>, EQueueMode::Mpsc>> Results;
	TSharedPtr<FThreadSafeCounter> TasksRunning;
	uint32 RevisionCounter = 1;
	bool bMaterialsReady = false;
	double StatTimer = 0.0;
	int32 MeshCounter = 0;

	// selection / crack
	UPROPERTY(Transient) TObjectPtr<UMCChunkMeshComponent> Outline;
	UPROPERTY(Transient) TObjectPtr<UMCChunkMeshComponent> Crack;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> OutlineMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> CrackMaterials;
	FMCBlockPos SelPos;
	uint16 SelState = 0;
	int32 SelStage = -2;

	// light pool
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> LightPool;
	double LightTimer = 0.0;

	void InitMaterials();
	void ApplyResult(TSharedPtr<FMCMeshResult> R);
	UMCChunkMeshComponent* AcquireComponent(int64 Key, const FMCChunkPos& C, int32 Group);
	void ReleaseGroup(int64 Key);
	void DropChunk(const FMCChunkPos& C);
	bool CanMesh(const FMCChunkPos& C) const;
	void LaunchMesh(const FMCChunkPos& C, int32 Group);
	void UpdateLights(const FVector& CameraBlocks);
	void UpdateVisibility(const FVector& CameraBlocks);
};
