// Shared declarations for the behaviour implementation files (not part of the public API).
#pragma once

#include "CoreMinimal.h"
#include "Blocks/MCBlockBehaviors.h"
#include "World/MCWorld.h"
#include "World/MCBlockEntity.h"
#include "Items/MCItems.h"
#include "Game/MCPlayer.h"
#include "Game/MCGame.h"
#include "Game/MCMob.h"
#include "Game/MCEntities.h"

class FMCGenWriter;

namespace MCBeh
{
	void Register(EMCBeh Id, FMCBlockBehavior* B);

	// registration entry points of the implementation files
	void RegisterPlacement();
	void RegisterRedstone();
	void RegisterPlants();
	void RegisterWorld();
	void RegisterFunctional();

	/** Signal suppression while dust computes its own level (Minecraft "shouldSignal"). */
	extern bool GWireSignalsDisabled;

	FORCEINLINE const FMCBlock& BlockAt(const FMCWorld& W, const FMCBlockPos& P) { return FMCBlocks::GetByState(W.GetState(P)); }
	FORCEINLINE uint8 Meta(FMCState S) { return FMCBlocks::MetaOf(S); }
	FORCEINLINE FMCState WithMeta(FMCState S, uint8 M) { return FMCBlocks::GetByState(S).State(M); }
	FORCEINLINE bool IsBlock(FMCState S, const TCHAR* Name) { return FMCBlocks::GetByState(S).Name == FName(Name); }
	FMCState StateOf(const TCHAR* Name, uint8 Meta = 0);
	/** Break a block that lost its support (drops as if mined by hand). */
	void BreakUnsupported(FMCWorld& W, const FMCBlockPos& P);
	/** Horizontal face the player looks towards. */
	EMCFace LookFacing(const FMCPlaceContext& C);
	/** Nearest of all six directions the player looks towards. */
	EMCFace LookFacing6(const FMCPlaceContext& C);
	bool HeldIs(AMCPlayer* P, const TCHAR* Item);
	bool HeldHasTag(AMCPlayer* P, const TCHAR* Tag);
	const FMCItem* HeldItem(AMCPlayer* P);
	int32 FluidTickDelay(const FMCWorld& W, bool bLava);
	/** Grow a feature (tree, huge mushroom, fungus) into the live world. */
	void RunFeatureInWorld(FMCWorld& W, const FMCBlockPos& Center, int32 RadiusBlocks, TFunctionRef<void(::FMCGenWriter&)> Fn);
	/** Bone meal a block. Returns true if something happened. */
	bool ApplyBoneMeal(FMCWorld& W, const FMCBlockPos& P, AMCPlayer* Player);
	/** Place the dropped item of a block (respecting silk touch / fortune rules handled by the loot code). */
	void SpawnDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, AMCEntity* Breaker);
	int32 PowerAt(const FMCWorld& W, const FMCBlockPos& P);
	bool IsRainingAbove(const FMCWorld& W, const FMCBlockPos& P);
}
