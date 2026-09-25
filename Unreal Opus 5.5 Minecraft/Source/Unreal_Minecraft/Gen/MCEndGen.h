// End generator: main island with obsidian spikes and exit portal, void ring, outer islands, end cities and ships.
#pragma once

#include "CoreMinimal.h"
#include "Gen/MCWorldGen.h"
#include "Gen/MCGenCommon.h"
#include "Core/MCNoise.h"

struct FMCEndSpike
{
	int32 X = 0, Y = 0;
	int32 Radius = 3;
	int32 Height = 80;
	bool bGuarded = false;
};

class UNREAL_MINECRAFT_API FMCEndGen : public FMCWorldGenerator
{
public:
	static constexpr int32 MainIslandTop = 60;
	static constexpr int32 OuterStart = 1000;

	FMCEndGen(uint64 InSeed);

	virtual void Generate(FMCChunk& C) const override;
	virtual uint8 GetBiomeAt(int32 X, int32 Y, int32 Z) const override;
	virtual int32 GetSurfaceHeight(int32 X, int32 Y) const override;
	virtual FMCBlockPos FindSpawn() const override { return FMCBlockPos(100, 0, 49); }
	virtual bool LocateStructure(FName Type, const FMCBlockPos& Origin, int32 MaxChunks, FMCBlockPos& Out) const override;
	virtual void GetStructureNames(TArray<FName>& Out) const override { Out.Add(TEXT("end_city")); }

	const TArray<FMCEndSpike>& GetSpikes() const { return Spikes; }
	/** Island density at a column (> 0 = land). Also returns top/bottom heights. */
	bool IslandColumn(int32 X, int32 Y, int32& OutTop, int32& OutBottom) const;
	/** Obsidian platform where players arrive (MC: 100, 49, 0). */
	static FMCBlockPos PlatformPos() { return FMCBlockPos(100, 0, 48); }
	/** Z of the exit portal layer (portal blocks go here inside the bedrock ring of radius 2.5). */
	int32 ExitPortalZ() const;
	/** Positions of the 20 end gateways (ring of radius 96), in spawn order. */
	TArray<FMCBlockPos> GatewayPositions() const;

private:
	FMCOctaveNoise NIsland, NDetail, NHeight, NBottom;
	TArray<FMCEndSpike> Spikes;
	FMCStructureSet CitySet;
	mutable FMCStructureCache StructCache;

	TSharedPtr<const FMCStructureStart> GetCity(const FMCChunkPos& SC) const;
	void BuildSpikes(FMCGenWriter& W) const;
	void Decorate(FMCGenWriter& W, int32 OCX, int32 OCY) const;
};
