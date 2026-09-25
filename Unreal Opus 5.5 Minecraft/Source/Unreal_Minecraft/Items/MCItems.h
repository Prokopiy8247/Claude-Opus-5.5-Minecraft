// Item registry and item stacks.
#pragma once

#include "CoreMinimal.h"
#include "Core/MCCore.h"
#include "Blocks/MCBlocks.h"

using FMCItemId = uint16;

enum class EMCItemKind : uint8
{
	Misc, Block, Tool, Weapon, Armor, Food, Bucket, SpawnEgg, Potion, Bow, Crossbow, Trident, Shield, Spear,
	Projectile, Placeable, Music, Map, Elytra, FishingRod, Firework, Book, Dye, Egg, Boat, Minecart, BoneMeal,
	Seeds, FlintAndSteel, Shears, Compass, Clock, EyeOfEnder, EndCrystal, Totem, Horn, Brush, Bundle, Harness,
	Saddle, Lead, NameTag, Bottle, Wax, Mace, Spyglass, Glowstick, Snowball, EnderPearl, ExpBottle, FireCharge,
	Head, Banner, Carrot, Sign
};

enum class EMCArmorSlot : uint8 { None, Head, Chest, Legs, Feet, Body };

enum class EMCEquipSlot : uint8 { MainHand, OffHand, Head, Chest, Legs, Feet, Count };

enum class EMCRarity : uint8 { Common, Uncommon, Rare, Epic };

enum class EMCEffect : uint8
{
	None = 0, Speed, Slowness, Haste, MiningFatigue, Strength, InstantHealth, InstantDamage, JumpBoost, Nausea,
	Regeneration, Resistance, FireResistance, WaterBreathing, Invisibility, Blindness, NightVision, Hunger,
	Weakness, Poison, Wither, HealthBoost, Absorption, Saturation, Glowing, Levitation, Luck, SlowFalling,
	ConduitPower, DolphinsGrace, BadOmen, Darkness, Infested, Oozing, Weaving, WindCharged, Count
};

struct FMCEffectInstance
{
	EMCEffect Effect = EMCEffect::None;
	int32 Duration = 0;  // ticks
	uint8 Amplifier = 0;
	bool bAmbient = false;
	bool bShowParticles = true;
	friend FArchive& operator<<(FArchive& Ar, FMCEffectInstance& E)
	{
		uint8 Eff = (uint8)E.Effect; Ar << Eff; E.Effect = (EMCEffect)Eff;
		Ar << E.Duration << E.Amplifier << E.bAmbient << E.bShowParticles;
		return Ar;
	}
};

struct FMCFood
{
	int32 Nutrition = 0;
	float Saturation = 0.f;    // saturation modifier (vanilla definition)
	bool bAlwaysEdible = false;
	bool bFastEat = false;
	float EatSeconds = 1.6f;
	TArray<TPair<FMCEffectInstance, float>> Effects; // effect + probability
	FName Remainder;           // bowl, bottle
};

enum class EMCEnchant : uint8
{
	None = 0, Protection, FireProtection, FeatherFalling, BlastProtection, ProjectileProtection, Respiration,
	AquaAffinity, Thorns, DepthStrider, FrostWalker, BindingCurse, SoulSpeed, SwiftSneak, Sharpness, Smite,
	BaneOfArthropods, Knockback, FireAspect, Looting, SweepingEdge, Efficiency, SilkTouch, Unbreaking, Fortune,
	Power, Punch, Flame, Infinity, LuckOfTheSea, Lure, Loyalty, Impaling, Riptide, Channeling, Multishot,
	QuickCharge, Piercing, Mending, VanishingCurse, Density, Breach, WindBurst, Lunge, Count
};

struct FMCEnchantLevel
{
	EMCEnchant Id = EMCEnchant::None;
	uint8 Level = 0;
	friend FArchive& operator<<(FArchive& Ar, FMCEnchantLevel& E)
	{
		uint8 Id = (uint8)E.Id; Ar << Id; E.Id = (EMCEnchant)Id; Ar << E.Level; return Ar;
	}
	bool operator==(const FMCEnchantLevel& O) const { return Id == O.Id && Level == O.Level; }
};

/** Optional per-stack data (kept out of line to keep stacks small). */
struct FMCItemExtra
{
	TArray<FMCEnchantLevel> Enchants;
	TArray<FMCEnchantLevel> StoredEnchants; // enchanted books
	FString CustomName;
	int32 RepairCost = 0;
	uint8 Potion = 0;          // potion type id
	uint8 Color = 255;         // dye colour (leather, shulker, firework)
	uint8 FlightDuration = 1;  // fireworks
	int32 Charge = 0;          // crossbow loaded projectile count / misc
	FName Loaded;              // crossbow loaded item
	int32 MapId = -1;
	FMCBlockPos Lodestone;     // compass target
	bool bHasLodestone = false;
	TArray<uint8> BlockEntityData; // serialized container contents (shulker boxes, bundles)

	bool IsEmpty() const
	{
		return Enchants.Num() == 0 && StoredEnchants.Num() == 0 && CustomName.IsEmpty() && RepairCost == 0 && Potion == 0 && Color == 255
			&& Charge == 0 && Loaded.IsNone() && MapId < 0 && !bHasLodestone && FlightDuration == 1 && BlockEntityData.Num() == 0;
	}
	bool Equals(const FMCItemExtra& O) const
	{
		return Enchants == O.Enchants && StoredEnchants == O.StoredEnchants && CustomName == O.CustomName && Potion == O.Potion
			&& Color == O.Color && FlightDuration == O.FlightDuration && Charge == O.Charge && Loaded == O.Loaded && MapId == O.MapId
			&& BlockEntityData == O.BlockEntityData;
	}
};

struct UNREAL_MINECRAFT_API FMCItemStack
{
	FMCItemId Id = 0;
	int32 Count = 0;
	int32 Damage = 0;
	TSharedPtr<FMCItemExtra> Extra;

	FMCItemStack() = default;
	FMCItemStack(FMCItemId InId, int32 InCount, int32 InDamage = 0) : Id(InId), Count(InCount), Damage(InDamage) {}
	static FMCItemStack Of(FName Item, int32 Count = 1);

	FORCEINLINE bool IsEmpty() const { return Id == 0 || Count <= 0; }
	void Clear() { Id = 0; Count = 0; Damage = 0; Extra.Reset(); }
	const struct FMCItem& Item() const;
	int32 MaxStack() const;
	bool CanStackWith(const FMCItemStack& O) const;
	FMCItemStack Split(int32 N);
	FMCItemStack Copy() const;
	FMCItemExtra& MutableExtra();
	bool HasExtra() const { return Extra.IsValid() && !Extra->IsEmpty(); }
	int32 GetEnchant(EMCEnchant E) const;
	void AddEnchant(EMCEnchant E, uint8 Level);
	bool IsEnchanted() const { return Extra.IsValid() && Extra->Enchants.Num() > 0; }
	FString GetDisplayName() const;
	/** Damages the item. Returns true if it broke. */
	bool DamageItem(int32 Amount, FMCRandom& R);
	bool IsDamageable() const;
	friend FArchive& operator<<(FArchive& Ar, FMCItemStack& S);
};

struct FMCItem
{
	FMCItemId Id = 0;
	FName Name;
	FString DisplayName;
	EMCItemKind Kind = EMCItemKind::Misc;
	EMCTab Tab = EMCTab::Ingredients;
	int32 MaxStack = 64;
	int32 MaxDamage = 0;
	EMCRarity Rarity = EMCRarity::Common;
	FMCBlockId Block = 0;       // block item (placing)
	FMCBlockId WallBlock = 0;   // wall variant (torches, heads)
	// tools / weapons
	EMCTool ToolType = EMCTool::None;
	uint8 Tier = 0;
	float MiningSpeed = 1.f;
	float AttackDamage = 1.f;   // total damage (hand = 1)
	float AttackSpeed = 4.f;    // attacks per second
	float Reach = 0.f;          // extra reach (spear)
	int32 Enchantability = 0;
	FName RepairItem;
	// armor
	EMCArmorSlot ArmorSlot = EMCArmorSlot::None;
	int32 ArmorPoints = 0;
	float Toughness = 0.f;
	float KnockbackResist = 0.f;
	FName ArmorMaterial;
	// food
	TSharedPtr<FMCFood> Food;
	// misc
	int32 FuelTicks = 0;
	FName SpawnMob;            // spawn eggs
	FName Fluid;               // buckets
	FName Projectile;
	uint8 DyeColor = 255;
	FName MeshAsset;           // authored held/dropped model
	FName IconName;            // icon atlas key (defaults to Name)
	bool bFireResistant = false;
	bool bGlint = false;
	int32 CompostChance = 0;   // percent
	float CookXP = 0.f;
	FName Family;
	TArray<FName> Tags;
	bool bHidden = false;      // not listed in the creative inventory
	bool HasTag(FName T) const { return Tags.Contains(T); }
	bool IsBlock() const { return Block != 0; }
};

class UNREAL_MINECRAFT_API FMCItems
{
public:
	static void Init();
	static bool IsInitialized();
	static const FMCItem& Get(FMCItemId Id);
	static const FMCItem* Find(FName Name);
	static FMCItemId FindId(FName Name);
	static FMCItemId ForBlock(FMCBlockId Block);
	static const TArray<FMCItem>& All() { return Items; }
	static int32 Num() { return Items.Num(); }
	static TArray<FMCItem>& Mutable() { return Items; }

private:
	friend class FMCItemRegistrar;
	static TArray<FMCItem> Items;
	static TMap<FName, FMCItemId> NameToId;
	static TArray<FMCItemId> BlockToItem;
	static bool bInitialized;
};

namespace MCEffects
{
	UNREAL_MINECRAFT_API const TCHAR* Name(EMCEffect E);
	UNREAL_MINECRAFT_API EMCEffect FromName(const FString& S);
	UNREAL_MINECRAFT_API FColor Color(EMCEffect E);
	UNREAL_MINECRAFT_API bool IsBeneficial(EMCEffect E);
	UNREAL_MINECRAFT_API bool IsInstant(EMCEffect E);
}

namespace MCEnchants
{
	struct FInfo
	{
		const TCHAR* Id;
		const TCHAR* Name;
		uint8 MaxLevel;
		int32 Weight;
		bool bTreasure;
		bool bCurse;
	};
	UNREAL_MINECRAFT_API const FInfo& Info(EMCEnchant E);
	UNREAL_MINECRAFT_API EMCEnchant FromName(const FString& S);
	/** Can this enchantment go on this item? */
	UNREAL_MINECRAFT_API bool CanApply(EMCEnchant E, const FMCItem& I);
	UNREAL_MINECRAFT_API bool Compatible(EMCEnchant A, EMCEnchant B);
	UNREAL_MINECRAFT_API FString Roman(int32 Level);
	/** Minimum / maximum modified enchantment level for a given enchant level. */
	UNREAL_MINECRAFT_API int32 MinCost(EMCEnchant E, int32 Level);
	UNREAL_MINECRAFT_API int32 MaxCost(EMCEnchant E, int32 Level);
}

namespace MCPotions
{
	struct FPotionDef
	{
		const TCHAR* Id;
		const TCHAR* Name;
		EMCEffect Effect;
		int32 Duration;   // ticks
		uint8 Amplifier;
		FColor Color;
	};
	UNREAL_MINECRAFT_API int32 Num();
	UNREAL_MINECRAFT_API const FPotionDef& Get(int32 Index);
	UNREAL_MINECRAFT_API int32 Find(const TCHAR* Id);
}
