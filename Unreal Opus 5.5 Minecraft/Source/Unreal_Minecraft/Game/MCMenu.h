// Container menus (inventory screens): slots bound to containers + Minecraft click semantics.
#pragma once

#include "CoreMinimal.h"
#include "Core/MCCore.h"
#include "Items/MCItems.h"
#include "World/MCBlockEntity.h"

class AMCPlayer;
struct FMCRecipe;

enum class EMCMenuType : uint8
{
	None, Inventory, Crafting, Chest, Furnace, BlastFurnace, Smoker, Brewing, Enchanting, Anvil, Grindstone, Stonecutter,
	Smithing, Loom, Hopper, Dispenser, Beacon, Cartography, Shulker, Crafter, Merchant
};

enum class EMCClick : uint8
{
	Pick,        // normal left / right click
	QuickMove,   // shift click
	Swap,        // number key 1-9 / F (offhand)
	Clone,       // middle click (creative)
	Throw,       // Q over a slot
	PickAll,     // double click
	QuickCraft   // drag distribution (finished)
};

enum EMCSlotFlags : uint16
{
	MCSF_None = 0,
	MCSF_Output = 1 << 0,     // take only
	MCSF_Head = 1 << 1,
	MCSF_Chest = 1 << 2,
	MCSF_Legs = 1 << 3,
	MCSF_Feet = 1 << 4,
	MCSF_Offhand = 1 << 5,
	MCSF_Fuel = 1 << 6,
	MCSF_Filtered = 1 << 7,   // uses the Filter predicate
	MCSF_Player = 1 << 8,     // part of the player inventory
	MCSF_Hotbar = 1 << 9,
	MCSF_Single = 1 << 10     // max stack 1 (bottles, lapis?)
};

struct FMCSlot
{
	FMCContainer* Container = nullptr;
	int32 Index = 0;
	FVector2D UIPos = FVector2D::ZeroVector; // top-left in panel pixels (18 px grid)
	uint16 Flags = 0;
	int32 MaxStack = 64;
	TFunction<bool(const FMCItemStack&)> Filter;
	FName EmptyIcon;                         // silhouette icon when empty (armour, fuel...)

	FMCItemStack& Get() const { return (*Container)[Index]; }
	bool IsOutput() const { return (Flags & MCSF_Output) != 0; }
};

class UNREAL_MINECRAFT_API FMCMenu : public TSharedFromThis<FMCMenu>
{
public:
	FMCMenu(EMCMenuType InType, AMCPlayer* InPlayer);
	virtual ~FMCMenu() = default;

	EMCMenuType Type;
	FString Title;
	AMCPlayer* Player = nullptr;
	FMCBlockPos Pos;
	bool bHasPos = false;
	TSharedPtr<FMCBlockEntity> BE;
	TArray<FMCSlot> Slots;
	int32 PlayerSlotStart = 0;    // first player inventory slot
	FVector2D PanelSize = FVector2D(176, 166);
	FVector2D PlayerInvPos = FVector2D(8, 84);
	FMCContainer Local;           // menu-owned temp slots (crafting grids, results, anvil inputs)
	int32 LocalOutputSlot = -1;

	/** Adds the 27 main + 9 hotbar player slots at panel position (X, Y) of the main inventory. */
	void AddPlayerInventory(float X, float Y);
	int32 AddSlot(FMCContainer* C, int32 Index, float X, float Y, uint16 Flags = 0);

	virtual void Click(int32 SlotIndex, int32 Button, EMCClick Mode, int32 HotbarKey = -1);
	/** Drag-distribute the carried stack over slots (Button 0 = even split, 1 = one each, 2 = full stacks in creative). */
	virtual void QuickCraftDistribute(const TArray<int32>& SlotIndices, int32 Button);
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex);
	virtual void SlotsChanged() {}
	virtual void OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken) {}
	virtual bool CanPlace(int32 SlotIndex, const FMCItemStack& S) const;
	virtual bool CanTake(int32 SlotIndex) const { return true; }
	virtual void Tick() {}
	virtual void Removed();
	virtual bool StillValid() const;
	virtual bool ButtonClick(int32 Id) { return false; }
	/** Generic readouts for widgets (progress bars, costs, levels). */
	virtual float GetProgress(int32 Which) const { return 0.f; }
	virtual int32 GetData(int32 Which) const { return 0; }
	virtual FString GetText(int32 Which) const { return FString(); }
	virtual void SetText(int32 Which, const FString& Text) {}

	bool MoveItemStackTo(FMCItemStack& Stack, int32 From, int32 To, bool bReverse);
	FMCItemStack& Carried();
	void DropCarried();

protected:
	int32 MaxStackFor(int32 SlotIndex, const FMCItemStack& S) const;
};

/** Player inventory with 2x2 crafting and armour/offhand slots. */
class UNREAL_MINECRAFT_API FMCInventoryMenu : public FMCMenu
{
public:
	explicit FMCInventoryMenu(AMCPlayer* P);
	virtual void SlotsChanged() override;
	virtual void OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken) override;
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual void Removed() override;
	virtual bool StillValid() const override { return true; }
	int32 CraftStart = 0, ResultSlot = 0, ArmorStart = 0, OffhandSlot = 0;
};

/** 3x3 crafting table. */
class UNREAL_MINECRAFT_API FMCCraftingMenu : public FMCMenu
{
public:
	FMCCraftingMenu(AMCPlayer* P, const FMCBlockPos& InPos);
	virtual void SlotsChanged() override;
	virtual void OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken) override;
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual void Removed() override;
	/** Fill the grid from the recipe book (moves ingredients from the inventory). */
	bool PlaceRecipe(const FMCRecipe& R, bool bMax);
	int32 GridW = 3;
};

class UNREAL_MINECRAFT_API FMCChestMenu : public FMCMenu
{
public:
	FMCChestMenu(AMCPlayer* P, EMCMenuType InType, FMCContainer* A, FMCContainer* B, int32 Rows, const FString& InTitle);
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual void Removed() override;
	int32 Rows = 3;
	TArray<TSharedPtr<FMCBlockEntity>> Viewed;
};

class UNREAL_MINECRAFT_API FMCFurnaceMenu : public FMCMenu
{
public:
	FMCFurnaceMenu(AMCPlayer* P, TSharedPtr<FMCFurnaceEntity> F);
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual void OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken) override;
	virtual float GetProgress(int32 Which) const override;
	virtual bool CanPlace(int32 SlotIndex, const FMCItemStack& S) const override;
	TSharedPtr<FMCFurnaceEntity> Furnace;
};

class UNREAL_MINECRAFT_API FMCBrewingMenu : public FMCMenu
{
public:
	FMCBrewingMenu(AMCPlayer* P, TSharedPtr<FMCBrewingEntity> B);
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual float GetProgress(int32 Which) const override;
	virtual bool CanPlace(int32 SlotIndex, const FMCItemStack& S) const override;
	TSharedPtr<FMCBrewingEntity> Stand;
};

class UNREAL_MINECRAFT_API FMCEnchantMenu : public FMCMenu
{
public:
	FMCEnchantMenu(AMCPlayer* P, const FMCBlockPos& InPos);
	virtual void SlotsChanged() override;
	virtual bool ButtonClick(int32 Id) override;
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual void Removed() override;
	virtual int32 GetData(int32 Which) const override;   // 0..2 costs, 3..5 hint enchant id, 6..8 hint level
	virtual FString GetText(int32 Which) const override;
	virtual bool CanPlace(int32 SlotIndex, const FMCItemStack& S) const override;
	int32 Costs[3] = { 0, 0, 0 };
	EMCEnchant Hints[3] = { EMCEnchant::None, EMCEnchant::None, EMCEnchant::None };
	int32 HintLevels[3] = { 0, 0, 0 };
	int32 Bookshelves = 0;
	static TArray<FMCEnchantLevel> SelectEnchantments(FMCRandom& R, const FMCItemStack& Stack, int32 Level, bool bTreasure);
};

class UNREAL_MINECRAFT_API FMCAnvilMenu : public FMCMenu
{
public:
	FMCAnvilMenu(AMCPlayer* P, const FMCBlockPos& InPos);
	virtual void SlotsChanged() override;
	virtual void OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken) override;
	virtual bool CanTake(int32 SlotIndex) const override;
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual void Removed() override;
	virtual int32 GetData(int32 Which) const override { return Which == 0 ? Cost : 0; }
	virtual FString GetText(int32 Which) const override { return ItemName; }
	virtual void SetText(int32 Which, const FString& Text) override { ItemName = Text; SlotsChanged(); }
	int32 Cost = 0;
	int32 RepairItemCount = 0;
	FString ItemName;
};

class UNREAL_MINECRAFT_API FMCGrindstoneMenu : public FMCMenu
{
public:
	FMCGrindstoneMenu(AMCPlayer* P, const FMCBlockPos& InPos);
	virtual void SlotsChanged() override;
	virtual void OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken) override;
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual void Removed() override;
};

class UNREAL_MINECRAFT_API FMCStonecutterMenu : public FMCMenu
{
public:
	FMCStonecutterMenu(AMCPlayer* P, const FMCBlockPos& InPos);
	virtual void SlotsChanged() override;
	virtual bool ButtonClick(int32 Id) override;
	virtual void OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken) override;
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual void Removed() override;
	virtual int32 GetData(int32 Which) const override;   // 0 = count, 1 = selected, 2+i = result item id
	TArray<FMCItemStack> Options;
	int32 Selected = -1;
};

class UNREAL_MINECRAFT_API FMCSmithingMenu : public FMCMenu
{
public:
	FMCSmithingMenu(AMCPlayer* P, const FMCBlockPos& InPos);
	virtual void SlotsChanged() override;
	virtual void OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken) override;
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual void Removed() override;
};

class UNREAL_MINECRAFT_API FMCSimpleContainerMenu : public FMCMenu
{
public:
	/** Hopper (5 slots in a row), dispenser/dropper (3x3), crafter (3x3). */
	FMCSimpleContainerMenu(AMCPlayer* P, EMCMenuType InType, TSharedPtr<FMCBlockEntity> InBE, const FString& InTitle);
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
};

class UNREAL_MINECRAFT_API FMCBeaconMenu : public FMCMenu
{
public:
	FMCBeaconMenu(AMCPlayer* P, TSharedPtr<FMCBeaconEntity> B);
	virtual bool ButtonClick(int32 Id) override; // 1..: primary effect index, 100+: secondary
	virtual int32 GetData(int32 Which) const override;
	virtual bool CanPlace(int32 SlotIndex, const FMCItemStack& S) const override;
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual void Removed() override;
	TSharedPtr<FMCBeaconEntity> Beacon;
	EMCEffect PendingPrimary = EMCEffect::None, PendingSecondary = EMCEffect::None;
};

class UNREAL_MINECRAFT_API FMCLoomMenu : public FMCMenu
{
public:
	FMCLoomMenu(AMCPlayer* P, const FMCBlockPos& InPos);
	virtual void SlotsChanged() override;
	virtual bool ButtonClick(int32 Id) override;
	virtual void OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken) override;
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual void Removed() override;
	int32 Pattern = -1;
};

class UNREAL_MINECRAFT_API FMCCartographyMenu : public FMCMenu
{
public:
	FMCCartographyMenu(AMCPlayer* P, const FMCBlockPos& InPos);
	virtual void SlotsChanged() override;
	virtual void OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken) override;
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual void Removed() override;
};

struct FMCTrade
{
	FMCItemStack CostA, CostB, Result;
	int32 Uses = 0, MaxUses = 12, XP = 2;
	float PriceMultiplier = 0.05f;
};

class UNREAL_MINECRAFT_API FMCMerchantMenu : public FMCMenu
{
public:
	FMCMerchantMenu(AMCPlayer* P, class AMCMob* InMerchant);
	virtual void SlotsChanged() override;
	virtual bool ButtonClick(int32 Id) override;
	virtual void OnTakeOutput(int32 SlotIndex, const FMCItemStack& Taken) override;
	virtual FMCItemStack QuickMoveStack(int32 SlotIndex) override;
	virtual void Removed() override;
	virtual bool StillValid() const override;
	virtual int32 GetData(int32 Which) const override;
	TWeakObjectPtr<class AMCMob> Merchant;
	int32 SelectedTrade = 0;
	TArray<FMCTrade>* Trades() const;
};
