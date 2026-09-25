// Loot: block drops (tool/tier/silk/fortune rules), mob drops, and structure chest loot tables.
#pragma once

#include "CoreMinimal.h"
#include "Items/MCItems.h"

class FMCWorld;
class FMCContainer;

namespace MCLoot
{
	/** Drops for breaking a block with Tool (nullptr = hand / explosion). Honours requires-tool, silk touch and fortune. */
	UNREAL_MINECRAFT_API void GetBlockDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R);
	/** Can this tool harvest the block (drops anything)? */
	UNREAL_MINECRAFT_API bool CanHarvest(FMCState S, const FMCItemStack* Tool);
	/** Mob drops. Looting level, killed by player, burning (cooked meat). */
	UNREAL_MINECRAFT_API void GetMobDrops(FName Mob, int32 Variant, int32 Looting, bool bPlayerKill, bool bOnFire, bool bBaby, FMCRandom& R, TArray<FMCItemStack>& Out);
	/** Fill a container from a named structure loot table. */
	UNREAL_MINECRAFT_API void FillContainer(FName Table, uint64 Seed, FMCContainer& Inv);
	/** List of known chest loot tables (for the debug panel). */
	UNREAL_MINECRAFT_API void GetTableNames(TArray<FName>& Out);
	/** Random enchantment of an item at a given level (used by loot & enchanting). */
	UNREAL_MINECRAFT_API void EnchantRandomly(FMCItemStack& Stack, int32 Level, bool bTreasure, FMCRandom& R);
	/** Piglin bartering result for one gold ingot. */
	UNREAL_MINECRAFT_API FMCItemStack Barter(FMCRandom& R);
	/** Fishing catch. */
	UNREAL_MINECRAFT_API FMCItemStack Fish(int32 LuckOfTheSea, bool bOpenWater, FMCRandom& R);
}
