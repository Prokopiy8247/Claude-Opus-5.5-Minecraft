// Tree and vegetation feature builders (deterministic, chunk-clipped).
#pragma once

#include "CoreMinimal.h"
#include "Gen/MCGenCommon.h"

enum class EMCTree : uint8
{
	Oak, FancyOak, Birch, TallBirch, Spruce, Pine, MegaSpruce, Jungle, MegaJungle, JungleBush, Acacia, DarkOak,
	Mangrove, Cherry, PaleOak, Azalea, SwampOak, RedMushroom, BrownMushroom, CrimsonFungus, WarpedFungus, Chorus, Count
};

namespace MCFeatures
{
	/** Build a tree whose lowest log sits at Base. Returns false if the tree could not be placed. */
	UNREAL_MINECRAFT_API void Tree(FMCGenWriter& W, FMCRandom& R, EMCTree Type, const FMCBlockPos& Base, bool bFromSapling = false);
	/** Blob of Replace-able blocks (ores, dirt patches) of approximately Size blocks. */
	UNREAL_MINECRAFT_API void Blob(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& Center, int32 Size, FMCState Block, FMCState DeepBlock, bool bOnlyStone = true, float AirExposureSkip = 0.f);
	UNREAL_MINECRAFT_API void Disk(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& Center, int32 Radius, FMCState Block, TFunctionRef<bool(FMCState)> CanReplace);
	UNREAL_MINECRAFT_API void Geode(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& Center);
	UNREAL_MINECRAFT_API void Iceberg(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& Base);
	UNREAL_MINECRAFT_API void IceSpike(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& Base);
	UNREAL_MINECRAFT_API void Fossil(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& Base, bool bNether);
	UNREAL_MINECRAFT_API void CoralReef(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& Base);
	UNREAL_MINECRAFT_API void Boulder(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& Base, FMCState Block);
	UNREAL_MINECRAFT_API void BasaltColumn(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& Base, int32 Height, int32 Radius);
	UNREAL_MINECRAFT_API void DesertWell(FMCGenWriter& W, const FMCBlockPos& Base);
	UNREAL_MINECRAFT_API void LavaLake(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& Center, FMCState Fluid);
}
