// Block behaviour hooks (placement rules, interaction, ticking, redstone, drops).
#pragma once

#include "CoreMinimal.h"
#include "Blocks/MCBlocks.h"

class FMCWorld;
class AMCPlayer;
class AMCEntity;
struct FMCItemStack;
class FMCBlockEntity;

/** Everything needed to decide which state to place. */
struct FMCPlaceContext
{
	FMCWorld* World = nullptr;
	AMCPlayer* Player = nullptr;
	FMCBlockPos Pos;              // target cell for the new block
	FMCBlockPos ClickedPos;       // block that was clicked
	EMCFace ClickedFace = EMCFace::Up;
	FVector HitFrac = FVector(0.5); // hit point inside the clicked block (0..1)
	float Yaw = 0.f;              // player yaw (deg)
	float Pitch = 0.f;            // player pitch (deg, + = looking up)
	bool bSneaking = false;
	FMCState Existing = 0;        // state currently at Pos (e.g. slab doubling)

	/** Horizontal facing pointing towards the player (what most blocks use as "front"). */
	EMCFace FacingTowardsPlayer() const;
	/** Horizontal direction the player is looking. */
	EMCFace PlayerLookFacing() const;
	/** Hit position on the clicked face, in the target cell's space (0..1). */
	FVector HitInTarget() const;
};

class UNREAL_MINECRAFT_API FMCBlockBehavior
{
public:
	virtual ~FMCBlockBehavior() = default;

	/** State to place (default: base state). Return 0 (air) to cancel. */
	virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& Ctx) const { return B.BaseState; }
	/** Support check (plants on soil, torches on solid faces). */
	virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const;
	/** Called after a player placed the block (double doors, beds...). */
	virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const {}
	/** Right click. Return true when the interaction consumed the click. */
	virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& HitFrac) const { return false; }
	/** Left click start (note blocks, redstone ore glow, dragon egg teleport). */
	virtual void OnAttack(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const {}
	/** Called when the block was removed/replaced. */
	virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState OldState, FMCState NewState) const {}
	/** Neighbour changed (support checks, redstone). */
	virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const;
	virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const {}
	virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const {}
	/** An entity's bounding box overlaps the block cell. */
	virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const {}
	/** An entity stands on top of the block. */
	virtual void OnSteppedOn(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const {}
	/** An entity fell onto the block (hay/slime damage reduction). Returns damage multiplier. */
	virtual float FallDamageMultiplier(FMCState S) const { return 1.f; }
	/** An entity landed on the block after falling Distance blocks (farmland trampling, slime/bed bounce, turtle eggs). */
	virtual void OnFallOn(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E, float Distance) const {}
	/** Random client-side display tick near the player (particles: flames, smoke, drips, portal sparkles...). */
	virtual void OnAnimateTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const {}

	/** Redstone: weak power emitted towards Dir (Dir = direction from this block to the receiver). */
	virtual int32 GetWeakPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const { return 0; }
	/** Redstone: strong power (powers the block it is attached to). */
	virtual int32 GetStrongPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const { return 0; }
	virtual bool IsPowerSource(FMCState S) const { return false; }
	/** Can redstone dust visually connect to this block from side F? */
	virtual bool ConnectsToRedstone(FMCState S, EMCFace F) const { return false; }
	virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const { return 0; }

	/** Special drops. Return false to use the default drop rule. */
	virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const { return false; }
	/** Block entity factory (containers, furnaces, signs...). */
	virtual TSharedPtr<FMCBlockEntity> CreateBlockEntity(const FMCBlockPos& P, FMCState S) const { return nullptr; }
	/** Light emission may depend on state (lit furnace, lamps). */
	virtual uint8 GetLightEmission(FMCState S, uint8 Default) const { return Default; }
	/** Whether a fluid can flow into / replace this block. */
	virtual bool IsFluidReplaceable(FMCState S) const { return false; }
	/** A projectile hit this block (target blocks, TNT with flame arrows, bells, chorus flowers). */
	virtual void OnProjectileHit(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* Projectile, const FVector& HitPointBlocks) const {}
	/** Removed by an explosion (TNT chain reactions). Called before the block is cleared. */
	virtual void OnExploded(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* Source) const {}
	/** Fire / lava ignited this block (TNT). Returns true if handled. */
	virtual bool OnIgnite(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* Igniter) const { return false; }
};

/** Shared helpers used by behaviours. */
namespace MCBehaviorUtil
{
	/** Does the block at P offer a full sturdy face on side Face (torch / button / rail support)? */
	UNREAL_MINECRAFT_API bool IsFaceSturdy(const FMCWorld& W, const FMCBlockPos& P, EMCFace Face);
	/** Solid top surface for plants / carpets / pressure plates. */
	UNREAL_MINECRAFT_API bool HasSolidTop(const FMCWorld& W, const FMCBlockPos& P);
	UNREAL_MINECRAFT_API void DropAsItem(FMCWorld& W, const FMCBlockPos& P, FMCState S);
	UNREAL_MINECRAFT_API void PopItem(FMCWorld& W, const FMCBlockPos& P, const FMCItemStack& Stack);
	/** True if the player holds nothing in the main hand. */
	UNREAL_MINECRAFT_API bool HandEmpty(AMCPlayer* Player);
	UNREAL_MINECRAFT_API bool IsCreative(AMCPlayer* Player);
	UNREAL_MINECRAFT_API bool IsWaterAt(const FMCWorld& W, const FMCBlockPos& P);
	UNREAL_MINECRAFT_API bool IsLavaAt(const FMCWorld& W, const FMCBlockPos& P);
}
