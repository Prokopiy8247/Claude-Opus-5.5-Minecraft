// Biome registry (Overworld, Nether, End).
#pragma once

#include "CoreMinimal.h"

enum class EMCBiome : uint8
{
	Plains, SunflowerPlains, SnowyPlains, IceSpikes, Desert, Swamp, MangroveSwamp, Forest, FlowerForest, BirchForest,
	OldGrowthBirchForest, DarkForest, PaleGarden, Taiga, OldGrowthPineTaiga, OldGrowthSpruceTaiga, SnowyTaiga, Savanna,
	SavannaPlateau, WindsweptSavanna, WindsweptHills, WindsweptGravellyHills, WindsweptForest, Jungle, SparseJungle,
	BambooJungle, Badlands, ErodedBadlands, WoodedBadlands, Meadow, CherryGrove, Grove, SnowySlopes, FrozenPeaks,
	JaggedPeaks, StonyPeaks, River, FrozenRiver, Beach, SnowyBeach, StonyShore, WarmOcean, LukewarmOcean,
	DeepLukewarmOcean, Ocean, DeepOcean, ColdOcean, DeepColdOcean, FrozenOcean, DeepFrozenOcean, MushroomFields,
	DripstoneCaves, LushCaves, DeepDark, SulfurCaves,
	NetherWastes, CrimsonForest, WarpedForest, SoulSandValley, BasaltDeltas,
	TheEnd, EndHighlands, EndMidlands, SmallEndIslands, EndBarrens,
	Count
};

struct FMCBiomeDef
{
	EMCBiome Id = EMCBiome::Plains;
	FName Name;
	FString Display;
	float Temperature = 0.8f;
	float Downfall = 0.4f;
	FColor Grass = FColor(145, 189, 89);
	FColor Foliage = FColor(119, 171, 47);
	FColor Water = FColor(63, 118, 228);
	FColor WaterFog = FColor(5, 5, 51);
	FColor Sky = FColor(120, 167, 255);
	FColor Fog = FColor(192, 216, 255);
	bool bSnowy = false;      // snow instead of rain
	bool bOcean = false;
	bool bCave = false;
	bool bDry = false;        // no rain
	bool bNether = false;
	bool bEnd = false;
	bool bFrozenWater = false;
};

class UNREAL_MINECRAFT_API FMCBiomes
{
public:
	static const FMCBiomeDef& Get(uint8 Id);
	static const FMCBiomeDef& Get(EMCBiome Id) { return Get((uint8)Id); }
	static int32 Num() { return (int32)EMCBiome::Count; }
	static EMCBiome FromName(const FString& Name);
private:
	static void Init();
};
