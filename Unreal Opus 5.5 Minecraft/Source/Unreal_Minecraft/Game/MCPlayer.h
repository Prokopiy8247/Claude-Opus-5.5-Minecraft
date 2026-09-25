// The player: first-person camera, Minecraft movement (walk/sprint/sneak/swim/fly), block interaction,
// inventory, hunger, experience, game modes, respawning.
#pragma once

#include "CoreMinimal.h"
#include "Game/MCLiving.h"
#include "Game/MCMenu.h"
#include "World/MCBlockEntity.h"
#include "MCPlayer.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UMCHeldItemComponent;
class UMCRigComponent;

/** Inventory layout: 0-8 hotbar, 9-35 main, 36-39 armour (feet, legs, chest, head), 40 offhand. */
namespace MCInv
{
	constexpr int32 Hotbar = 0, Main = 9, Armor = 36, Offhand = 40, Size = 41;
	FORCEINLINE int32 ArmorSlot(EMCArmorSlot S)
	{
		switch (S) { case EMCArmorSlot::Feet: return 36; case EMCArmorSlot::Legs: return 37; case EMCArmorSlot::Chest: return 38; case EMCArmorSlot::Head: return 39; default: return -1; }
	}
}

/** Latched input for one simulation tick (filled by the controller every frame). */
struct FMCPlayerInput
{
	float Forward = 0.f, Strafe = 0.f;
	bool bJump = false, bSneak = false, bSprint = false;
	bool bJumpPressed = false;      // edge since last tick (double tap -> fly toggle)
	bool bForwardPressed = false;   // edge (double tap W -> sprint)
	bool bAttackHeld = false, bAttackPressed = false;
	bool bUseHeld = false, bUsePressed = false;
	bool bPickPressed = false, bDropPressed = false, bDropStack = false, bSwapPressed = false;
};

UCLASS()
class UNREAL_MINECRAFT_API AMCPlayer : public AMCLiving
{
	GENERATED_BODY()
public:
	AMCPlayer();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UMCHeldItemComponent> HeldItem;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UMCRigComponent> Body;

	// ---- inventory
	FMCContainer Inventory;
	FMCContainer EnderInventory;
	int32 Selected = 0;
	FMCItemStack CarriedStack;
	TSharedPtr<FMCMenu> Menu;              // open container screen (null when only the HUD is shown)
	TSharedPtr<FMCInventoryMenu> InvMenu;  // persistent player inventory menu (2x2 crafting)
	bool bInventoryOpen = false;
	TArray<FName> KnownRecipes;

	// ---- abilities / mode
	EMCGameMode GameMode = EMCGameMode::Creative;
	bool bFlying = false;
	bool bMayFly = false;
	bool bInstabuild = false;
	float FlySpeed = 0.05f;
	float WalkSpeed = 0.1f;

	// ---- survival stats
	int32 FoodLevel = 20;
	float Saturation = 5.f;
	float Exhaustion = 0.f;
	int32 FoodTimer = 0;
	int32 XPLevel = 0;
	float XPProgress = 0.f;
	int32 XPTotal = 0;
	int32 EnchantSeed = 0;
	int32 Score = 0;

	// ---- respawn
	FMCBlockPos SpawnPoint;
	EMCDimension SpawnDim = EMCDimension::Overworld;
	bool bHasSpawnPoint = false;
	bool bSpawnForced = false;
	bool bDeadScreen = false;
	bool bSeenCredits = false;

	// ---- interaction
	FMCBlockPos MiningPos;
	bool bMining = false;
	float MiningProgress = 0.f;       // 0..1
	int32 MiningTicks = 0;
	int32 DestroyDelay = 0;           // creative / after-break delay
	int32 UseDelay = 0;               // right click repeat delay (4 ticks)
	int32 AttackStrengthTicker = 0;
	int32 UseItemRemaining = 0;       // eating/drinking/drawing
	int32 UseItemDuration = 0;
	bool bUsingItem = false;
	bool bUsingOffhand = false;
	bool bBlocking = false;
	int32 ItemCooldown = 0;           // ender pearl, chorus fruit, shield disable
	FMCItemStack LastHeldForAnim;
	float EquipAnim = 1.f;
	FVector SpearChargeStart;

	// ---- sleeping
	bool bSleeping = false;
	FMCBlockPos BedPos;
	int32 SleepCounter = 0;

	// ---- input / state machines
	FMCPlayerInput Input;
	int32 JumpTriggerTime = 0;
	int32 SprintTriggerTime = 0;
	int32 CameraMode = 0;             // 0 first person, 1 third person back, 2 front
	float FOVModifier = 1.f;
	float FOVOverride = 0.f;          // options FOV (degrees), 0 = default 70
	float BobPhase = 0.f;
	float PrevBobAmount = 0.f, BobAmount = 0.f;
	float ViewYaw = 0.f, ViewPitch = 0.f; // per-frame look (applied to Yaw/Pitch each tick)
	float EyeHeightCurrent = 1.62f, PrevEyeHeight = 1.62f;
	int32 TicksSinceDeath = 0;
	int32 RegenTimer = 0;
	int32 PortalTransitionTime = 0;
	float PortalOverlay = 0.f, PrevPortalOverlay = 0.f;
	int32 NumSelectedChanges = 0;

	// ---- statistics
	int64 StatBlocksMined = 0, StatBlocksPlaced = 0, StatMobsKilled = 0, StatDeaths = 0;
	double StatDistanceWalked = 0.0;

	// AMCEntity / AMCLiving
	virtual void InitEntity() override;
	virtual void TickEntity() override;
	virtual void UpdateVisual(float Alpha, float DeltaSeconds) override;
	virtual bool Hurt(const FMCDamage& Damage, float Amount) override;
	virtual void Die(const FMCDamage& Damage) override;
	virtual void Travel(float Strafe, float Up, float Forward) override;
	virtual void Jump() override;
	virtual void AIStep() override;
	virtual float GetSpeed() const override;
	virtual FMCItemStack& GetItem(EMCEquipSlot S) override;
	virtual const FMCItemStack& GetItem(EMCEquipSlot S) const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual FString GetDisplayName() const override { return TEXT("Steve"); }
	virtual bool IsPickable() const override { return false; }
	virtual void OnDeathAnimationFinished() override {}
	virtual void OnDimensionChanged() override;

	// ---- modes
	bool IsCreative() const { return GameMode == EMCGameMode::Creative; }
	bool IsSpectator() const { return GameMode == EMCGameMode::Spectator; }
	bool IsSurvivalLike() const { return GameMode == EMCGameMode::Survival || GameMode == EMCGameMode::Adventure; }
	void SetGameMode(EMCGameMode M);

	// ---- inventory
	FMCItemStack& Held() { return Inventory[Selected]; }
	const FMCItemStack& HeldConst() const { return Inventory.Slots[Selected]; }
	/** Adds a stack to the inventory. Returns true if everything fit (Stack is reduced). */
	bool AddItem(FMCItemStack& Stack);
	/** Adds or drops what does not fit. */
	void GiveItem(const FMCItemStack& Stack);
	void DropStack(const FMCItemStack& Stack, bool bThrow);
	void DropSelected(bool bWholeStack);
	void SwapHands();
	void SelectSlot(int32 Slot);
	void ScrollHotbar(int32 Delta);
	/** Middle click: pick the targeted block (creative: spawn it, survival: find in inventory). */
	void PickBlock();
	int32 FindSlot(FMCItemId Id) const;
	void ConsumeHeld(int32 Amount = 1, bool bOffhand = false);
	/** Replace the held item (bucket -> filled bucket), respecting stacks and creative. */
	void ReplaceHeld(const FMCItemStack& NewStack, bool bOffhand = false);
	void DamageHeld(int32 Amount, bool bOffhand = false);

	// ---- menus
	void OpenMenu(TSharedPtr<FMCMenu> NewMenu);
	void OpenInventory();
	void CloseMenu();
	bool IsMenuOpen() const { return Menu.IsValid() || bInventoryOpen; }
	FMCMenu* ActiveMenu() const { return Menu.IsValid() ? Menu.Get() : (bInventoryOpen ? InvMenu.Get() : nullptr); }
	/** Opens the right screen for a container block. */
	bool OpenBlockContainer(const FMCBlockPos& P);

	// ---- survival
	void CauseExhaustion(float Amount);
	void EatFood(const FMCItem& Item);
	bool CanEat(bool bAlwaysEdible) const;
	void GiveXP(int32 Amount);
	void GiveXPLevels(int32 Levels);
	int32 XPNeededForNextLevel() const;
	void TickFood();
	void Respawn();

	// ---- interaction
	void HandleAttack(bool bPressed, bool bHeld);
	void HandleUse(bool bPressed, bool bHeld);
	void StopMining();
	bool AttackEntity(AMCEntity* Target);
	bool UseItemOn(const struct FMCRayHit& Hit, bool bOffhand);
	bool UseItemInAir(bool bOffhand);
	void StartUsingItem(bool bOffhand, int32 Duration);
	void StopUsingItem(bool bRelease);
	void FinishUsingItem();
	float GetDestroySpeed(FMCState S) const;
	bool CanHarvest(FMCState S) const;
	float GetAttackStrengthScale(float Adjust = 0.5f) const;
	float GetAttackDelayTicks() const;
	float GetReach() const;
	/** Current crosshair target (block or entity). */
	bool GetTarget(struct FMCRayHit& OutBlock, AMCEntity*& OutEntity) const;
	void Sleep(const FMCBlockPos& Bed);
	void WakeUp(bool bSetSpawn);
	void SetSpawnPoint(const FMCBlockPos& P, EMCDimension Dim, bool bForced);
	void SendMessage(const FString& Msg) const;
	void ShowActionBar(const FString& Msg) const;
	bool IsUsingSpyglass() const;
	float GetFOVMultiplier() const;
	void HandlePortalTransitions();

protected:
	void TickMovementStates();
	void TickMining();
	void TickUseItem();
	void TickCooldowns();
	void TickPickup();
	void UpdateCamera(float Alpha, float DeltaSeconds);
	void PlaceBlockFromItem(const struct FMCRayHit& Hit, const FMCItemStack& Stack, bool bOffhand);
	bool TryPlaceBlock(const FMCBlockPos& Target, const struct FMCRayHit& Hit, const FMCItemStack& Stack, bool bOffhand);
	int32 SinceLastHurt = 0;
	FMCBlockPos LastMiningPos;
	bool bWasOnGroundForStep = false;
	double StepDistance = 0.0;
	double NextStepSound = 1.0;
};
