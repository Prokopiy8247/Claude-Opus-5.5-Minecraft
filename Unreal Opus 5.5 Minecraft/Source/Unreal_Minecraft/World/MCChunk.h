// Chunk column storage: 24 vertical sections of 16x16x16 blocks, light, biomes, block entities.
#pragma once

#include "CoreMinimal.h"
#include "Core/MCCore.h"
#include "Blocks/MCBlocks.h"

class FMCBlockEntity;

struct FMCSection
{
	uint16 States[MC::SectionVolume];
	uint8 Light[MC::SectionVolume]; // high nibble = sky light, low nibble = block light
	int32 NonAir = 0;
	int32 Ticking = 0;             // blocks with random ticks

	explicit FMCSection(uint8 DefaultSky)
	{
		FMemory::Memzero(States, sizeof(States));
		FMemory::Memset(Light, (uint8)(DefaultSky << 4), sizeof(Light));
	}
	void Recount();
};

/** Entity spawn requested by world generation (structures, animals). */
struct FMCPendingSpawn
{
	FName Mob;
	FVector Pos = FVector::ZeroVector; // block units
	int32 Variant = -1;
	bool bPersistent = true;
	FName Extra;
};

enum class EMCChunkStage : uint8
{
	None,
	Generating,
	Generated,  // blocks present, light not computed
	Lighting,
	Lit,        // fully usable
};

class UNREAL_MINECRAFT_API FMCChunk
{
public:
	FMCChunk(const FMCChunkPos& InPos, EMCDimension InDim);
	~FMCChunk();

	FMCChunkPos Pos;
	EMCDimension Dim;
	uint8 DefaultSky = 15;
	TUniquePtr<FMCSection> Sections[MC::NumSections];
	uint8 Biomes[4 * 4 * (MC::WorldHeight / 4)]; // 4x4x4 biome cells
	int16 HeightMap[256];    // Z of the highest block that blocks light (MinZ-1 if none)
	int16 MotionHeight[256]; // Z of the highest solid/fluid block
	TMap<int32, TSharedPtr<FMCBlockEntity>> BlockEntities; // key = packed local index
	TArray<FMCPendingSpawn> PendingSpawns;
	TArray<FMCBlockPos> PendingTicks; // blocks that need an initial scheduled tick (fluids placed by generation)
	TArray<uint8> SavedEntities;      // serialized entities of this chunk (restored when the chunk is lit)

	EMCChunkStage Stage = EMCChunkStage::None;
	bool bModified = false;    // must be saved
	bool bLoadedFromSave = false;
	bool bSpawnsProcessed = false;
	bool bPopulated = false;
	uint32 MeshDirty = 0;      // bit per section
	uint32 EditCounter = 0;
	double LastSeenTime = 0.0;
	int64 InhabitedTicks = 0;

	static FORCEINLINE int32 SectionOf(int32 Z) { return (Z - MC::MinZ) >> 4; }
	static FORCEINLINE bool InRange(int32 Z) { return Z >= MC::MinZ && Z <= MC::MaxZ; }
	static FORCEINLINE int32 LocalIndex(int32 LX, int32 LY, int32 Z) { return LX | (LY << 4) | ((Z - MC::MinZ) << 8); }
	static FORCEINLINE void UnpackLocal(int32 Idx, int32& LX, int32& LY, int32& Z) { LX = Idx & 15; LY = (Idx >> 4) & 15; Z = (Idx >> 8) + MC::MinZ; }

	FORCEINLINE FMCState Get(int32 LX, int32 LY, int32 Z) const
	{
		if (!InRange(Z)) return 0;
		const FMCSection* S = Sections[SectionOf(Z)].Get();
		return S ? S->States[MC::SectionIndex(LX, LY, (Z - MC::MinZ) & 15)] : 0;
	}
	FORCEINLINE uint8 GetLightRaw(int32 LX, int32 LY, int32 Z) const
	{
		if (Z > MC::MaxZ) return (uint8)(DefaultSky << 4);
		if (Z < MC::MinZ) return 0;
		const FMCSection* S = Sections[SectionOf(Z)].Get();
		return S ? S->Light[MC::SectionIndex(LX, LY, (Z - MC::MinZ) & 15)] : (uint8)(DefaultSky << 4);
	}
	FORCEINLINE uint8 GetSky(int32 LX, int32 LY, int32 Z) const { return GetLightRaw(LX, LY, Z) >> 4; }
	FORCEINLINE uint8 GetBlockLight(int32 LX, int32 LY, int32 Z) const { return GetLightRaw(LX, LY, Z) & 15; }

	/** Raw write without any update logic. Returns previous state. */
	FMCState SetRaw(int32 LX, int32 LY, int32 Z, FMCState S);
	void SetLightRaw(int32 LX, int32 LY, int32 Z, uint8 Packed);
	void SetSky(int32 LX, int32 LY, int32 Z, uint8 V);
	void SetBlockLight(int32 LX, int32 LY, int32 Z, uint8 V);

	FMCSection* GetOrCreateSection(int32 SectionIndex);
	FORCEINLINE const FMCSection* GetSection(int32 SectionIndex) const { return Sections[SectionIndex].Get(); }

	FORCEINLINE uint8 GetBiome(int32 LX, int32 LY, int32 Z) const
	{
		const int32 BZ = FMath::Clamp((Z - MC::MinZ) >> 2, 0, MC::WorldHeight / 4 - 1);
		return Biomes[(LX >> 2) | ((LY >> 2) << 2) | (BZ << 4)];
	}
	FORCEINLINE void SetBiomeCell(int32 CX, int32 CY, int32 CZ, uint8 B) { Biomes[CX | (CY << 2) | (CZ << 4)] = B; }
	void FillBiomeColumn(int32 CX, int32 CY, uint8 B);

	FORCEINLINE int32 GetHeight(int32 LX, int32 LY) const { return HeightMap[MC::ColumnIndex(LX, LY)]; }
	FORCEINLINE int32 GetMotionHeight(int32 LX, int32 LY) const { return MotionHeight[MC::ColumnIndex(LX, LY)]; }
	void RecalcHeightmaps();
	void UpdateHeightmapAt(int32 LX, int32 LY, int32 Z, FMCState NewState);
	int32 HighestSection() const;

	FMCBlockEntity* GetBlockEntity(int32 LX, int32 LY, int32 Z) const;
	void MarkAllDirty() { MeshDirty = 0xFFFFFFu; }
	void MarkDirtyAt(int32 Z) { if (InRange(Z)) MeshDirty |= 1u << SectionOf(Z); }

	/** Binary (de)serialisation of blocks, light, biomes, block entities. */
	void Serialize(FArchive& Ar, int32 Version);
	int64 MemoryUsage() const;
};
