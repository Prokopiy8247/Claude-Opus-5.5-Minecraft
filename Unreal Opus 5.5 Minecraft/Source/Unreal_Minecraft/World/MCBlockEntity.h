// Block entities: containers, furnaces, hoppers, brewing stands, spawners, beacons...
#pragma once

#include "CoreMinimal.h"
#include "Core/MCCore.h"
#include "Items/MCItems.h"

class FMCWorld;

enum class EMCBlockEntityType : uint8
{
	None, Chest, Barrel, ShulkerBox, Furnace, BlastFurnace, Smoker, Hopper, Dispenser, Dropper, BrewingStand,
	Beacon, Spawner, Campfire, Jukebox, Lectern, EnderChest, Crafter, ChiseledBookshelf, Comparator, EndGateway,
	CopperChest, TrappedChest, Shelf, TrialSpawner, Vault, DecoratedPot, DaylightDetector, Beehive, Count
};

class UNREAL_MINECRAFT_API FMCContainer
{
public:
	TArray<FMCItemStack> Slots;

	void Init(int32 N) { Slots.SetNum(N); }
	int32 Num() const { return Slots.Num(); }
	FMCItemStack& operator[](int32 I) { return Slots[I]; }
	const FMCItemStack& operator[](int32 I) const { return Slots[I]; }
	bool IsEmpty() const;
	int32 CountItem(FMCItemId Id) const;
	/** Insert into slots [From, To). Returns the amount that did not fit. */
	int32 Insert(FMCItemStack& Stack, int32 From = 0, int32 To = -1);
	bool CanInsert(const FMCItemStack& Stack, int32 From = 0, int32 To = -1) const;
	/** Redstone comparator signal (0..15). */
	int32 ComparatorSignal() const;
	void Serialize(FArchive& Ar);
};

class UNREAL_MINECRAFT_API FMCBlockEntity : public TSharedFromThis<FMCBlockEntity>
{
public:
	explicit FMCBlockEntity(EMCBlockEntityType InType) : Type(InType) {}
	virtual ~FMCBlockEntity() = default;

	EMCBlockEntityType Type;
	FMCBlockPos Pos;
	FString CustomName;
	FName LootTable;          // unopened structure loot
	uint64 LootSeed = 0;

	virtual bool Ticks() const { return false; }
	virtual void Tick(FMCWorld& W) {}
	virtual FMCContainer* GetContainer() { return nullptr; }
	virtual void Serialize(FArchive& Ar, int32 Version);
	/** Drop contents when the block is broken. */
	virtual void DropContents(FMCWorld& W);
	/** Resolve loot table into the container on first open. */
	void UnpackLoot(FMCWorld& W);

	static TSharedPtr<FMCBlockEntity> Create(EMCBlockEntityType Type);
};

class UNREAL_MINECRAFT_API FMCContainerEntity : public FMCBlockEntity
{
public:
	FMCContainerEntity(EMCBlockEntityType InType, int32 Size) : FMCBlockEntity(InType) { Inv.Init(Size); }
	FMCContainer Inv;
	int32 Viewers = 0;        // open-lid animation
	float LidAngle = 0.f;
	virtual FMCContainer* GetContainer() override { return &Inv; }
	virtual void Serialize(FArchive& Ar, int32 Version) override;
};

enum class EMCFurnaceKind : uint8 { Furnace, Blast, Smoker };

class UNREAL_MINECRAFT_API FMCFurnaceEntity : public FMCContainerEntity
{
public:
	explicit FMCFurnaceEntity(EMCFurnaceKind InKind);
	EMCFurnaceKind Kind;
	int32 BurnTime = 0;       // remaining fuel ticks
	int32 BurnDuration = 0;   // total ticks of the current fuel
	int32 CookTime = 0;
	int32 CookTotal = 200;
	float StoredXP = 0.f;
	// slots: 0 input, 1 fuel, 2 output
	virtual bool Ticks() const override { return true; }
	virtual void Tick(FMCWorld& W) override;
	virtual void Serialize(FArchive& Ar, int32 Version) override;
	bool IsLit() const { return BurnTime > 0; }
	int32 TakeXP();
};

class UNREAL_MINECRAFT_API FMCHopperEntity : public FMCContainerEntity
{
public:
	FMCHopperEntity() : FMCContainerEntity(EMCBlockEntityType::Hopper, 5) {}
	int32 Cooldown = 0;
	virtual bool Ticks() const override { return true; }
	virtual void Tick(FMCWorld& W) override;
	virtual void Serialize(FArchive& Ar, int32 Version) override;
};

class UNREAL_MINECRAFT_API FMCBrewingEntity : public FMCContainerEntity
{
public:
	FMCBrewingEntity() : FMCContainerEntity(EMCBlockEntityType::BrewingStand, 5) {}
	// slots 0..2 bottles, 3 ingredient, 4 fuel (blaze powder)
	int32 BrewTime = 0;
	int32 Fuel = 0;
	FMCItemId BrewingIngredient = 0;
	virtual bool Ticks() const override { return true; }
	virtual void Tick(FMCWorld& W) override;
	virtual void Serialize(FArchive& Ar, int32 Version) override;
	bool CanBrew() const;
	void DoBrew();
};

class UNREAL_MINECRAFT_API FMCSpawnerEntity : public FMCBlockEntity
{
public:
	FMCSpawnerEntity() : FMCBlockEntity(EMCBlockEntityType::Spawner) {}
	FName Mob = TEXT("pig");
	int32 Delay = 200;
	float Spin = 0.f;
	virtual bool Ticks() const override { return true; }
	virtual void Tick(FMCWorld& W) override;
	virtual void Serialize(FArchive& Ar, int32 Version) override;
};

class UNREAL_MINECRAFT_API FMCCampfireEntity : public FMCContainerEntity
{
public:
	FMCCampfireEntity() : FMCContainerEntity(EMCBlockEntityType::Campfire, 4) {}
	int32 CookTimes[4] = { 0, 0, 0, 0 };
	virtual bool Ticks() const override { return true; }
	virtual void Tick(FMCWorld& W) override;
	virtual void Serialize(FArchive& Ar, int32 Version) override;
};

class UNREAL_MINECRAFT_API FMCBeaconEntity : public FMCBlockEntity
{
public:
	FMCBeaconEntity() : FMCBlockEntity(EMCBlockEntityType::Beacon) {}
	int32 Levels = 0;
	EMCEffect Primary = EMCEffect::None;
	EMCEffect Secondary = EMCEffect::None;
	int32 Timer = 0;
	FMCContainer Payment; // 1 slot
	virtual bool Ticks() const override { return true; }
	virtual void Tick(FMCWorld& W) override;
	virtual FMCContainer* GetContainer() override { return &Payment; }
	virtual void Serialize(FArchive& Ar, int32 Version) override;
	void UpdateLevels(FMCWorld& W);
};

class UNREAL_MINECRAFT_API FMCEndGatewayEntity : public FMCBlockEntity
{
public:
	FMCEndGatewayEntity() : FMCBlockEntity(EMCBlockEntityType::EndGateway) {}
	FMCBlockPos Exit;
	bool bHasExit = false;
	int32 Cooldown = 0;
	virtual bool Ticks() const override { return true; }
	virtual void Tick(FMCWorld& W) override { if (Cooldown > 0) --Cooldown; }
	virtual void Serialize(FArchive& Ar, int32 Version) override;
};

/** Daylight detector: updates its power every second (a block entity so it survives chunk reloads). */
class UNREAL_MINECRAFT_API FMCDaylightEntity : public FMCBlockEntity
{
public:
	FMCDaylightEntity() : FMCBlockEntity(EMCBlockEntityType::DaylightDetector) {}
	int32 Timer = 0;
	virtual bool Ticks() const override { return true; }
	virtual void Tick(FMCWorld& W) override;
};

/** Bee nest / beehive: stored bees and honey level. */
class UNREAL_MINECRAFT_API FMCBeehiveEntity : public FMCBlockEntity
{
public:
	FMCBeehiveEntity() : FMCBlockEntity(EMCBlockEntityType::Beehive) {}
	int32 Bees = 0;
	int32 Honey = 0;
	int32 Timer = 0;
	virtual bool Ticks() const override { return true; }
	virtual void Tick(FMCWorld& W) override;
	virtual void Serialize(FArchive& Ar, int32 Version) override;
};

class UNREAL_MINECRAFT_API FMCComparatorEntity : public FMCBlockEntity
{
public:
	FMCComparatorEntity() : FMCBlockEntity(EMCBlockEntityType::Comparator) {}
	int32 Output = 0;
	virtual void Serialize(FArchive& Ar, int32 Version) override;
};
