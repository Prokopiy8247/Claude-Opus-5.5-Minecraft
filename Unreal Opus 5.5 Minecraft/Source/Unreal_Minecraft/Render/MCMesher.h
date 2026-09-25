// Chunk mesher: turns a padded voxel snapshot into render geometry (runs on worker threads).
#pragma once

#include "CoreMinimal.h"
#include "Core/MCCore.h"
#include "Blocks/MCBlocks.h"
#include "Render/MCChunkMeshComponent.h"

class FMCWorld;

/** Height of one render group (4 sections). */
namespace MCRender
{
	constexpr int32 GroupHeight = 64;
	constexpr int32 NumGroups = MC::WorldHeight / GroupHeight; // 6
	FORCEINLINE int32 GroupOfZ(int32 Z) { return (Z - MC::MinZ) / GroupHeight; }
	FORCEINLINE int32 GroupBaseZ(int32 G) { return MC::MinZ + G * GroupHeight; }

	enum EVertexFlags : uint32
	{
		VF_Tint = 1,
		VF_Emissive = 2,
		VF_Wave = 4,
		VF_Foliage = 8,
		VF_Flow = 16,
		VF_Particle = 32   // unlit billboard: emissive = albedo * vertex colour, opacity *= vertex alpha
	};
}

/** Snapshot of one render group plus a one block border (copied on the game thread). */
struct FMCMeshInput
{
	static constexpr int32 SX = 18, SY = 18, SZ = MCRender::GroupHeight + 2;
	FMCChunkPos Chunk;
	int32 Group = 0;
	int32 BaseZ = 0;
	EMCDimension Dim = EMCDimension::Overworld;
	TArray<uint16> States;
	TArray<uint8> Light;
	FColor GrassTint[SX * SY];
	FColor FoliageTint[SX * SY];
	FColor WaterTint[SX * SY];
	uint32 Revision = 0;

	FORCEINLINE static int32 Index(int32 X, int32 Y, int32 Z) { return (X + 1) + (Y + 1) * SX + (Z + 1) * SX * SY; }
	FORCEINLINE FMCState Get(int32 X, int32 Y, int32 Z) const { return States[Index(X, Y, Z)]; }
	FORCEINLINE uint8 GetLight(int32 X, int32 Y, int32 Z) const { return Light[Index(X, Y, Z)]; }
	FORCEINLINE static int32 Col(int32 X, int32 Y) { return (X + 1) + (Y + 1) * SX; }

	/** Build from the world (game thread). Returns false if the group is completely empty. */
	bool Capture(const FMCWorld& World, const FMCChunkPos& C, int32 G);
};

class UNREAL_MINECRAFT_API FMCMesher
{
public:
	static TUniquePtr<FMCChunkMeshData> Build(const FMCMeshInput& In);
};

/** Shared biome colour helpers (also used for item rendering). */
namespace MCBiomeColors
{
	UNREAL_MINECRAFT_API FColor Grass(uint8 Biome);
	UNREAL_MINECRAFT_API FColor Foliage(uint8 Biome);
	UNREAL_MINECRAFT_API FColor Water(uint8 Biome);
}
