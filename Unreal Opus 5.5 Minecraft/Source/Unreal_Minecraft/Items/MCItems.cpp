#include "Items/MCItems.h"
#include "Blocks/MCBlocks.h"

TArray<FMCItem> FMCItems::Items;
TMap<FName, FMCItemId> FMCItems::NameToId;
TArray<FMCItemId> FMCItems::BlockToItem;
bool FMCItems::bInitialized = false;

void MCRegisterAllItems(class FMCItemRegistrar& R);

class FMCItemRegistrar
{
public:
	TArray<FMCItem>& Items;
	explicit FMCItemRegistrar(TArray<FMCItem>& In) : Items(In) {}
	FMCItem& Add(const TCHAR* Name, EMCItemKind Kind, EMCTab Tab, int32 MaxStack = 64)
	{
		FMCItem& I = Items.AddDefaulted_GetRef();
		I.Name = FName(Name);
		I.Kind = Kind;
		I.Tab = Tab;
		I.MaxStack = MaxStack;
		FString S(Name);
		TArray<FString> Parts; S.ParseIntoArray(Parts, TEXT("_"));
		for (FString& P : Parts) { if (P.Len() && P != TEXT("of") && P != TEXT("the") && P != TEXT("on")) P[0] = FChar::ToUpper(P[0]); I.DisplayName += (I.DisplayName.IsEmpty() ? P : TEXT(" ") + P); }
		return I;
	}
};

void FMCItems::Init()
{
	if (bInitialized) return;
	FMCBlocks::Init();
	Items.Reset();
	NameToId.Reset();
	FMCItemRegistrar R(Items);
	// id 0 = empty
	R.Add(TEXT("air"), EMCItemKind::Misc, EMCTab::None).bHidden = true;
	MCRegisterAllItems(R);

	BlockToItem.Init(0, FMCBlocks::NumBlocks());
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		Items[i].Id = (FMCItemId)i;
		if (!NameToId.Contains(Items[i].Name)) NameToId.Add(Items[i].Name, (FMCItemId)i);
		if (Items[i].IconName.IsNone()) Items[i].IconName = Items[i].Name;
	}
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		if (Items[i].Block != 0 && BlockToItem[Items[i].Block] == 0) BlockToItem[Items[i].Block] = (FMCItemId)i;
	}
	// link blocks -> items (pick block / drops)
	for (FMCBlock& B : FMCBlocks::Mutable())
	{
		if (!B.ItemName.IsNone())
		{
			if (const FMCItemId* Id = NameToId.Find(B.ItemName)) { B.ItemId = *Id; continue; }
		}
		B.ItemId = BlockToItem.IsValidIndex(B.Id) ? BlockToItem[B.Id] : 0;
	}
	bInitialized = true;
	UE_LOG(LogOpus55, Log, TEXT("Registered %d items"), Items.Num());
}

bool FMCItems::IsInitialized() { return bInitialized; }
const FMCItem& FMCItems::Get(FMCItemId Id) { return Items.IsValidIndex(Id) ? Items[Id] : Items[0]; }
const FMCItem* FMCItems::Find(FName Name) { const FMCItemId* Id = NameToId.Find(Name); return Id ? &Items[*Id] : nullptr; }
FMCItemId FMCItems::FindId(FName Name) { const FMCItemId* Id = NameToId.Find(Name); return Id ? *Id : 0; }
FMCItemId FMCItems::ForBlock(FMCBlockId Block) { return BlockToItem.IsValidIndex(Block) ? BlockToItem[Block] : 0; }

// ------------------------------------------------------------------------------------------ item stack

FMCItemStack FMCItemStack::Of(FName Item, int32 Count)
{
	return FMCItemStack(FMCItems::FindId(Item), Count);
}

const FMCItem& FMCItemStack::Item() const { return FMCItems::Get(Id); }
int32 FMCItemStack::MaxStack() const { return Item().MaxStack; }

bool FMCItemStack::CanStackWith(const FMCItemStack& O) const
{
	if (IsEmpty() || O.IsEmpty()) return true;
	if (Id != O.Id || Damage != O.Damage) return false;
	if (Item().MaxStack <= 1) return false;
	const bool bA = HasExtra(), bB = O.HasExtra();
	if (bA != bB) return false;
	return !bA || Extra->Equals(*O.Extra);
}

FMCItemStack FMCItemStack::Split(int32 N)
{
	N = FMath::Min(N, Count);
	FMCItemStack R = Copy();
	R.Count = N;
	Count -= N;
	if (Count <= 0) Clear();
	return R;
}

FMCItemStack FMCItemStack::Copy() const
{
	FMCItemStack R(Id, Count, Damage);
	if (Extra.IsValid()) R.Extra = MakeShared<FMCItemExtra>(*Extra);
	return R;
}

FMCItemExtra& FMCItemStack::MutableExtra()
{
	if (!Extra.IsValid()) Extra = MakeShared<FMCItemExtra>();
	else if (!Extra.IsUnique()) Extra = MakeShared<FMCItemExtra>(*Extra);
	return *Extra;
}

int32 FMCItemStack::GetEnchant(EMCEnchant E) const
{
	if (!Extra.IsValid()) return 0;
	for (const FMCEnchantLevel& L : Extra->Enchants) if (L.Id == E) return L.Level;
	return 0;
}

void FMCItemStack::AddEnchant(EMCEnchant E, uint8 Level)
{
	FMCItemExtra& X = MutableExtra();
	TArray<FMCEnchantLevel>& Arr = Item().Name == TEXT("enchanted_book") ? X.StoredEnchants : X.Enchants;
	for (FMCEnchantLevel& L : Arr) if (L.Id == E) { L.Level = FMath::Max(L.Level, Level); return; }
	FMCEnchantLevel L; L.Id = E; L.Level = Level;
	Arr.Add(L);
}

FString FMCItemStack::GetDisplayName() const
{
	if (Extra.IsValid() && !Extra->CustomName.IsEmpty()) return Extra->CustomName;
	const FMCItem& I = Item();
	if (I.Kind == EMCItemKind::Potion && Extra.IsValid() && Extra->Potion > 0 && Extra->Potion < MCPotions::Num())
	{
		const FString Prefix = I.Name == TEXT("splash_potion") ? TEXT("Splash ") : (I.Name == TEXT("lingering_potion") ? TEXT("Lingering ") : TEXT(""));
		return Prefix + MCPotions::Get(Extra->Potion).Name;
	}
	return I.DisplayName;
}

bool FMCItemStack::IsDamageable() const { return Item().MaxDamage > 0; }

bool FMCItemStack::DamageItem(int32 Amount, FMCRandom& R)
{
	if (!IsDamageable() || Amount <= 0) return false;
	const int32 Unbreaking = GetEnchant(EMCEnchant::Unbreaking);
	int32 Applied = 0;
	for (int32 i = 0; i < Amount; ++i)
	{
		if (Unbreaking > 0 && R.NextInt(Unbreaking + 1) > 0) continue;
		++Applied;
	}
	Damage += Applied;
	if (Damage >= Item().MaxDamage)
	{
		Clear();
		return true;
	}
	return false;
}

FArchive& operator<<(FArchive& Ar, FMCItemStack& S)
{
	// items are saved by name so registry changes do not corrupt saves
	FName Name = Ar.IsSaving() ? S.Item().Name : NAME_None;
	Ar << Name;
	if (Ar.IsLoading()) S.Id = FMCItems::FindId(Name);
	Ar << S.Count << S.Damage;
	bool bExtra = Ar.IsSaving() ? S.HasExtra() : false;
	Ar << bExtra;
	if (bExtra)
	{
		if (Ar.IsLoading()) S.Extra = MakeShared<FMCItemExtra>();
		FMCItemExtra& X = *S.Extra;
		Ar << X.Enchants << X.StoredEnchants << X.CustomName << X.RepairCost << X.Potion << X.Color << X.FlightDuration << X.Charge << X.Loaded << X.MapId;
		Ar << X.Lodestone << X.bHasLodestone << X.BlockEntityData;
	}
	if (S.Id == 0) S.Count = 0;
	return Ar;
}

// ------------------------------------------------------------------------------------------ effects / enchantments / potions

namespace MCEffects
{
	struct FEffInfo { const TCHAR* Name; uint32 Color; bool bGood; bool bInstant; };
	static const FEffInfo Infos[(int32)EMCEffect::Count] = {
		{ TEXT("none"), 0xFFFFFF, true, false }, { TEXT("speed"), 0x33EBFF, true, false }, { TEXT("slowness"), 0x8BAFE0, false, false },
		{ TEXT("haste"), 0xD9C043, true, false }, { TEXT("mining_fatigue"), 0x4A4217, false, false }, { TEXT("strength"), 0xFFC700, true, false },
		{ TEXT("instant_health"), 0xF82423, true, true }, { TEXT("instant_damage"), 0xA9656A, false, true }, { TEXT("jump_boost"), 0xFDFF84, true, false },
		{ TEXT("nausea"), 0x551D4A, false, false }, { TEXT("regeneration"), 0xCD5CAB, true, false }, { TEXT("resistance"), 0x9146F0, true, false },
		{ TEXT("fire_resistance"), 0xFF9900, true, false }, { TEXT("water_breathing"), 0x98DAC0, true, false }, { TEXT("invisibility"), 0xF6F6F6, true, false },
		{ TEXT("blindness"), 0x1F1F23, false, false }, { TEXT("night_vision"), 0xC2FF66, true, false }, { TEXT("hunger"), 0x587653, false, false },
		{ TEXT("weakness"), 0x484D48, false, false }, { TEXT("poison"), 0x87A363, false, false }, { TEXT("wither"), 0x736156, false, false },
		{ TEXT("health_boost"), 0xF87D23, true, false }, { TEXT("absorption"), 0x2552A5, true, false }, { TEXT("saturation"), 0xF82423, true, true },
		{ TEXT("glowing"), 0x94A061, false, false }, { TEXT("levitation"), 0xCEFFFF, false, false }, { TEXT("luck"), 0x59C106, true, false },
		{ TEXT("slow_falling"), 0xF3CFB9, true, false }, { TEXT("conduit_power"), 0x1DC2D1, true, false }, { TEXT("dolphins_grace"), 0x88A3BE, true, false },
		{ TEXT("bad_omen"), 0x0B6138, false, false }, { TEXT("darkness"), 0x292721, false, false }, { TEXT("infested"), 0x8C9B8C, false, false },
		{ TEXT("oozing"), 0x99FFA3, false, false }, { TEXT("weaving"), 0x78695A, false, false }, { TEXT("wind_charged"), 0xBDC9FF, false, false }
	};
	const TCHAR* Name(EMCEffect E) { return Infos[(int32)E].Name; }
	EMCEffect FromName(const FString& S)
	{
		for (int32 i = 1; i < (int32)EMCEffect::Count; ++i) if (S.Equals(Infos[i].Name, ESearchCase::IgnoreCase)) return (EMCEffect)i;
		return EMCEffect::None;
	}
	FColor Color(EMCEffect E) { const uint32 C = Infos[(int32)E].Color; return FColor((C >> 16) & 255, (C >> 8) & 255, C & 255); }
	bool IsBeneficial(EMCEffect E) { return Infos[(int32)E].bGood; }
	bool IsInstant(EMCEffect E) { return Infos[(int32)E].bInstant; }
}

namespace MCEnchants
{
	static const FInfo InfoTable[(int32)EMCEnchant::Count] = {
		{ TEXT("none"), TEXT("None"), 0, 0, false, false },
		{ TEXT("protection"), TEXT("Protection"), 4, 10, false, false },
		{ TEXT("fire_protection"), TEXT("Fire Protection"), 4, 5, false, false },
		{ TEXT("feather_falling"), TEXT("Feather Falling"), 4, 5, false, false },
		{ TEXT("blast_protection"), TEXT("Blast Protection"), 4, 2, false, false },
		{ TEXT("projectile_protection"), TEXT("Projectile Protection"), 4, 5, false, false },
		{ TEXT("respiration"), TEXT("Respiration"), 3, 2, false, false },
		{ TEXT("aqua_affinity"), TEXT("Aqua Affinity"), 1, 2, false, false },
		{ TEXT("thorns"), TEXT("Thorns"), 3, 1, false, false },
		{ TEXT("depth_strider"), TEXT("Depth Strider"), 3, 2, false, false },
		{ TEXT("frost_walker"), TEXT("Frost Walker"), 2, 2, true, false },
		{ TEXT("binding_curse"), TEXT("Curse of Binding"), 1, 1, true, true },
		{ TEXT("soul_speed"), TEXT("Soul Speed"), 3, 1, true, false },
		{ TEXT("swift_sneak"), TEXT("Swift Sneak"), 3, 1, true, false },
		{ TEXT("sharpness"), TEXT("Sharpness"), 5, 10, false, false },
		{ TEXT("smite"), TEXT("Smite"), 5, 5, false, false },
		{ TEXT("bane_of_arthropods"), TEXT("Bane of Arthropods"), 5, 5, false, false },
		{ TEXT("knockback"), TEXT("Knockback"), 2, 5, false, false },
		{ TEXT("fire_aspect"), TEXT("Fire Aspect"), 2, 2, false, false },
		{ TEXT("looting"), TEXT("Looting"), 3, 2, false, false },
		{ TEXT("sweeping_edge"), TEXT("Sweeping Edge"), 3, 2, false, false },
		{ TEXT("efficiency"), TEXT("Efficiency"), 5, 10, false, false },
		{ TEXT("silk_touch"), TEXT("Silk Touch"), 1, 1, false, false },
		{ TEXT("unbreaking"), TEXT("Unbreaking"), 3, 5, false, false },
		{ TEXT("fortune"), TEXT("Fortune"), 3, 2, false, false },
		{ TEXT("power"), TEXT("Power"), 5, 10, false, false },
		{ TEXT("punch"), TEXT("Punch"), 2, 2, false, false },
		{ TEXT("flame"), TEXT("Flame"), 1, 2, false, false },
		{ TEXT("infinity"), TEXT("Infinity"), 1, 1, false, false },
		{ TEXT("luck_of_the_sea"), TEXT("Luck of the Sea"), 3, 2, false, false },
		{ TEXT("lure"), TEXT("Lure"), 3, 2, false, false },
		{ TEXT("loyalty"), TEXT("Loyalty"), 3, 5, false, false },
		{ TEXT("impaling"), TEXT("Impaling"), 5, 2, false, false },
		{ TEXT("riptide"), TEXT("Riptide"), 3, 2, false, false },
		{ TEXT("channeling"), TEXT("Channeling"), 1, 1, false, false },
		{ TEXT("multishot"), TEXT("Multishot"), 1, 2, false, false },
		{ TEXT("quick_charge"), TEXT("Quick Charge"), 3, 5, false, false },
		{ TEXT("piercing"), TEXT("Piercing"), 4, 10, false, false },
		{ TEXT("mending"), TEXT("Mending"), 1, 2, true, false },
		{ TEXT("vanishing_curse"), TEXT("Curse of Vanishing"), 1, 1, true, true },
		{ TEXT("density"), TEXT("Density"), 5, 5, false, false },
		{ TEXT("breach"), TEXT("Breach"), 4, 2, false, false },
		{ TEXT("wind_burst"), TEXT("Wind Burst"), 3, 2, true, false },
		{ TEXT("lunge"), TEXT("Lunge"), 3, 5, false, false },
	};

	const FInfo& Info(EMCEnchant E) { return InfoTable[FMath::Clamp((int32)E, 0, (int32)EMCEnchant::Count - 1)]; }
	EMCEnchant FromName(const FString& S)
	{
		for (int32 i = 1; i < (int32)EMCEnchant::Count; ++i) if (S.Equals(InfoTable[i].Id, ESearchCase::IgnoreCase)) return (EMCEnchant)i;
		return EMCEnchant::None;
	}
	FString Roman(int32 L)
	{
		static const TCHAR* R[] = { TEXT(""), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V"), TEXT("VI"), TEXT("VII"), TEXT("VIII"), TEXT("IX"), TEXT("X") };
		return (L >= 0 && L <= 10) ? R[L] : FString::FromInt(L);
	}
	bool CanApply(EMCEnchant E, const FMCItem& I)
	{
		if (I.Name == TEXT("book") || I.Name == TEXT("enchanted_book")) return true;
		const bool bArmor = I.ArmorSlot != EMCArmorSlot::None;
		const bool bSword = I.ToolType == EMCTool::Sword;
		const bool bAxe = I.ToolType == EMCTool::Axe;
		const bool bDigger = I.ToolType == EMCTool::Pickaxe || I.ToolType == EMCTool::Shovel || I.ToolType == EMCTool::Axe || I.ToolType == EMCTool::Hoe;
		const bool bDurable = I.MaxDamage > 0;
		switch (E)
		{
		case EMCEnchant::Protection: case EMCEnchant::FireProtection: case EMCEnchant::BlastProtection: case EMCEnchant::ProjectileProtection: case EMCEnchant::Thorns: return bArmor;
		case EMCEnchant::FeatherFalling: case EMCEnchant::DepthStrider: case EMCEnchant::FrostWalker: case EMCEnchant::SoulSpeed: return I.ArmorSlot == EMCArmorSlot::Feet;
		case EMCEnchant::Respiration: case EMCEnchant::AquaAffinity: return I.ArmorSlot == EMCArmorSlot::Head;
		case EMCEnchant::SwiftSneak: return I.ArmorSlot == EMCArmorSlot::Legs;
		case EMCEnchant::Sharpness: case EMCEnchant::Smite: case EMCEnchant::BaneOfArthropods: return bSword || bAxe || I.Kind == EMCItemKind::Spear;
		case EMCEnchant::Knockback: case EMCEnchant::FireAspect: case EMCEnchant::Looting: return bSword || I.Kind == EMCItemKind::Spear;
		case EMCEnchant::SweepingEdge: return bSword;
		case EMCEnchant::Efficiency: return bDigger || I.ToolType == EMCTool::Shears;
		case EMCEnchant::SilkTouch: case EMCEnchant::Fortune: return bDigger;
		case EMCEnchant::Unbreaking: case EMCEnchant::Mending: return bDurable;
		case EMCEnchant::Power: case EMCEnchant::Punch: case EMCEnchant::Flame: case EMCEnchant::Infinity: return I.Kind == EMCItemKind::Bow;
		case EMCEnchant::LuckOfTheSea: case EMCEnchant::Lure: return I.Kind == EMCItemKind::FishingRod;
		case EMCEnchant::Loyalty: case EMCEnchant::Impaling: case EMCEnchant::Riptide: case EMCEnchant::Channeling: return I.Kind == EMCItemKind::Trident;
		case EMCEnchant::Multishot: case EMCEnchant::QuickCharge: case EMCEnchant::Piercing: return I.Kind == EMCItemKind::Crossbow;
		case EMCEnchant::Density: case EMCEnchant::Breach: case EMCEnchant::WindBurst: return I.Kind == EMCItemKind::Mace;
		case EMCEnchant::Lunge: return I.Kind == EMCItemKind::Spear;
		case EMCEnchant::BindingCurse: return bArmor;
		case EMCEnchant::VanishingCurse: return bDurable;
		default: return false;
		}
	}
	bool Compatible(EMCEnchant A, EMCEnchant B)
	{
		if (A == B) return false;
		auto Group = [](EMCEnchant E) -> int32
		{
			switch (E)
			{
			case EMCEnchant::Protection: case EMCEnchant::FireProtection: case EMCEnchant::BlastProtection: case EMCEnchant::ProjectileProtection: return 1;
			case EMCEnchant::Sharpness: case EMCEnchant::Smite: case EMCEnchant::BaneOfArthropods: return 2;
			case EMCEnchant::SilkTouch: case EMCEnchant::Fortune: return 3;
			case EMCEnchant::Infinity: case EMCEnchant::Mending: return 4;
			case EMCEnchant::DepthStrider: case EMCEnchant::FrostWalker: return 5;
			case EMCEnchant::Riptide: case EMCEnchant::Loyalty: return 6;
			case EMCEnchant::Multishot: case EMCEnchant::Piercing: return 7;
			case EMCEnchant::Density: case EMCEnchant::Breach: return 8;
			default: return 0;
			}
		};
		const int32 GA = Group(A), GB = Group(B);
		return GA == 0 || GA != GB;
	}
	int32 MinCost(EMCEnchant E, int32 L) { return 1 + (L - 1) * 10 + (Info(E).Weight >= 10 ? 0 : 5); }
	int32 MaxCost(EMCEnchant E, int32 L) { return MinCost(E, L) + 30; }
}

namespace MCPotions
{
	static const FPotionDef Defs[] = {
		{ TEXT("water"), TEXT("Water Bottle"), EMCEffect::None, 0, 0, FColor(56, 93, 198) },
		{ TEXT("awkward"), TEXT("Awkward Potion"), EMCEffect::None, 0, 0, FColor(56, 93, 198) },
		{ TEXT("mundane"), TEXT("Mundane Potion"), EMCEffect::None, 0, 0, FColor(56, 93, 198) },
		{ TEXT("thick"), TEXT("Thick Potion"), EMCEffect::None, 0, 0, FColor(56, 93, 198) },
		{ TEXT("healing"), TEXT("Potion of Healing"), EMCEffect::InstantHealth, 1, 0, FColor(248, 36, 35) },
		{ TEXT("strong_healing"), TEXT("Potion of Healing II"), EMCEffect::InstantHealth, 1, 1, FColor(248, 36, 35) },
		{ TEXT("regeneration"), TEXT("Potion of Regeneration"), EMCEffect::Regeneration, 900, 0, FColor(205, 92, 171) },
		{ TEXT("long_regeneration"), TEXT("Potion of Regeneration (Long)"), EMCEffect::Regeneration, 1800, 0, FColor(205, 92, 171) },
		{ TEXT("strength"), TEXT("Potion of Strength"), EMCEffect::Strength, 3600, 0, FColor(255, 199, 0) },
		{ TEXT("strong_strength"), TEXT("Potion of Strength II"), EMCEffect::Strength, 1800, 1, FColor(255, 199, 0) },
		{ TEXT("swiftness"), TEXT("Potion of Swiftness"), EMCEffect::Speed, 3600, 0, FColor(51, 235, 255) },
		{ TEXT("long_swiftness"), TEXT("Potion of Swiftness (Long)"), EMCEffect::Speed, 9600, 0, FColor(51, 235, 255) },
		{ TEXT("fire_resistance"), TEXT("Potion of Fire Resistance"), EMCEffect::FireResistance, 3600, 0, FColor(255, 153, 0) },
		{ TEXT("long_fire_resistance"), TEXT("Potion of Fire Resistance (Long)"), EMCEffect::FireResistance, 9600, 0, FColor(255, 153, 0) },
		{ TEXT("night_vision"), TEXT("Potion of Night Vision"), EMCEffect::NightVision, 3600, 0, FColor(194, 255, 102) },
		{ TEXT("long_night_vision"), TEXT("Potion of Night Vision (Long)"), EMCEffect::NightVision, 9600, 0, FColor(194, 255, 102) },
		{ TEXT("invisibility"), TEXT("Potion of Invisibility"), EMCEffect::Invisibility, 3600, 0, FColor(246, 246, 246) },
		{ TEXT("water_breathing"), TEXT("Potion of Water Breathing"), EMCEffect::WaterBreathing, 3600, 0, FColor(152, 218, 192) },
		{ TEXT("leaping"), TEXT("Potion of Leaping"), EMCEffect::JumpBoost, 3600, 0, FColor(253, 255, 132) },
		{ TEXT("slow_falling"), TEXT("Potion of Slow Falling"), EMCEffect::SlowFalling, 1800, 0, FColor(243, 207, 185) },
		{ TEXT("poison"), TEXT("Potion of Poison"), EMCEffect::Poison, 900, 0, FColor(135, 163, 99) },
		{ TEXT("harming"), TEXT("Potion of Harming"), EMCEffect::InstantDamage, 1, 0, FColor(169, 101, 106) },
		{ TEXT("weakness"), TEXT("Potion of Weakness"), EMCEffect::Weakness, 1800, 0, FColor(72, 77, 72) },
		{ TEXT("slowness"), TEXT("Potion of Slowness"), EMCEffect::Slowness, 1800, 0, FColor(139, 175, 224) },
		{ TEXT("turtle_master"), TEXT("Potion of the Turtle Master"), EMCEffect::Resistance, 400, 2, FColor(117, 113, 154) },
		{ TEXT("luck"), TEXT("Potion of Luck"), EMCEffect::Luck, 6000, 0, FColor(89, 193, 6) },
	};
	int32 Num() { return UE_ARRAY_COUNT(Defs); }
	const FPotionDef& Get(int32 Index) { return Defs[FMath::Clamp(Index, 0, Num() - 1)]; }
	int32 Find(const TCHAR* Id)
	{
		for (int32 i = 0; i < Num(); ++i) if (FCString::Stricmp(Defs[i].Id, Id) == 0) return i;
		return -1;
	}
}

// ------------------------------------------------------------------------------------------ registrations

void MCRegisterAllItems(FMCItemRegistrar& R)
{
	using K = EMCItemKind;
	using T = EMCTab;

	// ---------------------------------------------------------------- block items (one per placeable block)
	for (const FMCBlock& B : FMCBlocks::All())
	{
		if (B.Id == 0 || B.Has(MCB_NoItem)) continue;
		FMCItem& I = R.Add(*B.Name.ToString(), K::Block, B.Tab);
		I.DisplayName = B.DisplayName;
		I.Block = B.Id;
		I.FuelTicks = B.FuelTicks;
		I.Family = B.Family;
		I.Tags = B.Tags;
		if (B.Name.ToString().EndsWith(TEXT("_bed")) || B.Name.ToString().EndsWith(TEXT("_shulker_box")) || B.Name == TEXT("shulker_box") || B.Name == TEXT("cake")) I.MaxStack = 1;
		if (B.Name.ToString().Contains(TEXT("_sign"))) I.MaxStack = 16;
		if (B.Name.ToString().EndsWith(TEXT("_head")) || B.Name.ToString().EndsWith(TEXT("_skull"))) I.Rarity = EMCRarity::Uncommon;
		if (B.Name == TEXT("dragon_egg") || B.Name == TEXT("beacon") || B.Name == TEXT("conduit")) I.Rarity = EMCRarity::Epic;
	}
	// items that place blocks but have their own name
	auto Placer = [&](const TCHAR* Name, const TCHAR* BlockName, T Tab, K Kind = K::Placeable) -> FMCItem&
	{
		FMCItem& I = R.Add(Name, Kind, Tab);
		I.Block = FMCBlocks::FindId(FName(BlockName));
		return I;
	};
	Placer(TEXT("redstone"), TEXT("redstone_wire"), T::Redstone).DisplayName = TEXT("Redstone Dust");
	Placer(TEXT("wheat_seeds"), TEXT("wheat"), T::Natural, K::Seeds);
	Placer(TEXT("beetroot_seeds"), TEXT("beetroots"), T::Natural, K::Seeds);
	Placer(TEXT("melon_seeds"), TEXT("melon_stem"), T::Natural, K::Seeds);
	Placer(TEXT("pumpkin_seeds"), TEXT("pumpkin_stem"), T::Natural, K::Seeds);
	Placer(TEXT("torchflower_seeds"), TEXT("torchflower_crop"), T::Natural, K::Seeds);
	Placer(TEXT("sweet_berries"), TEXT("sweet_berry_bush"), T::Food, K::Food);
	Placer(TEXT("glow_berries"), TEXT("cave_vines"), T::Food, K::Food);

	// ---------------------------------------------------------------- materials / ingredients
	const TCHAR* Materials[] = {
		TEXT("stick"), TEXT("string"), TEXT("coal"), TEXT("charcoal"), TEXT("diamond"), TEXT("emerald"), TEXT("lapis_lazuli"), TEXT("quartz"), TEXT("amethyst_shard"),
		TEXT("raw_iron"), TEXT("raw_copper"), TEXT("raw_gold"), TEXT("iron_ingot"), TEXT("copper_ingot"), TEXT("gold_ingot"), TEXT("netherite_ingot"),
		TEXT("netherite_scrap"), TEXT("iron_nugget"), TEXT("gold_nugget"), TEXT("copper_nugget"), TEXT("flint"), TEXT("feather"), TEXT("leather"),
		TEXT("rabbit_hide"), TEXT("gunpowder"), TEXT("bone"), TEXT("bone_meal"), TEXT("slime_ball"), TEXT("ender_pearl"), TEXT("blaze_rod"),
		TEXT("blaze_powder"), TEXT("ghast_tear"), TEXT("magma_cream"), TEXT("nether_star"), TEXT("shulker_shell"), TEXT("phantom_membrane"),
		TEXT("clay_ball"), TEXT("brick"), TEXT("nether_brick"), TEXT("paper"), TEXT("book"), TEXT("sugar"), TEXT("glowstone_dust"),
		TEXT("prismarine_shard"), TEXT("prismarine_crystals"), TEXT("ink_sac"), TEXT("glow_ink_sac"), TEXT("honeycomb"), TEXT("scute"),
		TEXT("armadillo_scute"), TEXT("nautilus_shell"), TEXT("heart_of_the_sea"), TEXT("echo_shard"), TEXT("disc_fragment_5"), TEXT("breeze_rod"),
		TEXT("heavy_core"), TEXT("fermented_spider_eye"), TEXT("glistering_melon_slice"), TEXT("rabbit_foot"), TEXT("dragon_breath"),
		TEXT("popped_chorus_fruit"), TEXT("resin_clump"), TEXT("resin_brick"), TEXT("wind_charge"), TEXT("sulfur_shard"), TEXT("cinnabar_dust"),
		TEXT("netherite_upgrade_smithing_template"), TEXT("trial_key"), TEXT("ominous_trial_key"), TEXT("turtle_scute"), TEXT("firework_star"),
		TEXT("wheat")
	};
	for (const TCHAR* M : Materials)
	{
		FMCItem& I = R.Add(M, K::Misc, T::Ingredients);
		I.FuelTicks = 0;
	}
	auto FindLast = [&](const TCHAR* N) -> FMCItem* { for (int32 i = R.Items.Num() - 1; i >= 0; --i) if (R.Items[i].Name == N) return &R.Items[i]; return nullptr; };
	FindLast(TEXT("stick"))->FuelTicks = 100;
	FindLast(TEXT("coal"))->FuelTicks = 1600;
	FindLast(TEXT("charcoal"))->FuelTicks = 1600;
	FindLast(TEXT("blaze_rod"))->FuelTicks = 2400;
	FindLast(TEXT("nether_star"))->Rarity = EMCRarity::Rare;
	FindLast(TEXT("nether_star"))->bGlint = true;
	FindLast(TEXT("ender_pearl"))->MaxStack = 16;
	FindLast(TEXT("ender_pearl"))->Kind = K::EnderPearl;
	FindLast(TEXT("bone_meal"))->Kind = K::BoneMeal;
	FindLast(TEXT("wind_charge"))->Kind = K::Projectile;
	FindLast(TEXT("heart_of_the_sea"))->Rarity = EMCRarity::Uncommon;
	FindLast(TEXT("dragon_breath"))->Rarity = EMCRarity::Uncommon;
	FindLast(TEXT("wheat"))->Tab = T::Ingredients;
	FindLast(TEXT("netherite_ingot"))->bFireResistant = true;
	FindLast(TEXT("netherite_scrap"))->bFireResistant = true;

	// dyes
	const TCHAR* Dyes[16] = { TEXT("white"), TEXT("orange"), TEXT("magenta"), TEXT("light_blue"), TEXT("yellow"), TEXT("lime"), TEXT("pink"), TEXT("gray"),
		TEXT("light_gray"), TEXT("cyan"), TEXT("purple"), TEXT("blue"), TEXT("brown"), TEXT("green"), TEXT("red"), TEXT("black") };
	for (int32 i = 0; i < 16; ++i)
	{
		FMCItem& I = R.Add(*(FString(Dyes[i]) + TEXT("_dye")), K::Dye, T::Ingredients);
		I.DyeColor = (uint8)i;
		I.Tags.Add(TEXT("dyes"));
	}

	// ---------------------------------------------------------------- tools & weapons
	struct FTier { const TCHAR* Name; uint8 Tier; int32 Durability; float Speed; float Bonus; int32 Ench; const TCHAR* Repair; };
	const FTier Tiers[] = {
		{ TEXT("wooden"), MCTier::Wood, 59, 2.f, 0.f, 15, TEXT("#planks") },
		{ TEXT("stone"), MCTier::Stone, 131, 4.f, 1.f, 5, TEXT("#stone_tool_materials") },
		{ TEXT("copper"), MCTier::Copper, 190, 5.f, 1.f, 13, TEXT("copper_ingot") },
		{ TEXT("iron"), MCTier::Iron, 250, 6.f, 2.f, 14, TEXT("iron_ingot") },
		{ TEXT("golden"), MCTier::Gold, 32, 12.f, 0.f, 22, TEXT("gold_ingot") },
		{ TEXT("diamond"), MCTier::Diamond, 1561, 8.f, 3.f, 10, TEXT("diamond") },
		{ TEXT("netherite"), MCTier::Netherite, 2031, 9.f, 4.f, 15, TEXT("netherite_ingot") },
	};
	struct FToolType { const TCHAR* Suffix; EMCTool Tool; K Kind; float BaseDamage; float Speed; };
	const FToolType Types[] = {
		{ TEXT("_sword"), EMCTool::Sword, K::Weapon, 4.f, 1.6f },
		{ TEXT("_shovel"), EMCTool::Shovel, K::Tool, 2.5f, 1.f },
		{ TEXT("_pickaxe"), EMCTool::Pickaxe, K::Tool, 2.f, 1.2f },
		{ TEXT("_axe"), EMCTool::Axe, K::Tool, 7.f, 0.8f },
		{ TEXT("_hoe"), EMCTool::Hoe, K::Tool, 1.f, 1.f },
		{ TEXT("_spear"), EMCTool::None, K::Spear, 3.f, 1.1f },
	};
	for (const FTier& Tr : Tiers)
	{
		for (const FToolType& Ty : Types)
		{
			FMCItem& I = R.Add(*(FString(Tr.Name) + Ty.Suffix), Ty.Kind, Ty.Kind == K::Weapon || Ty.Kind == K::Spear ? T::Combat : T::Tools, 1);
			I.ToolType = Ty.Tool;
			I.Tier = Tr.Tier;
			I.MaxDamage = Tr.Durability;
			I.MiningSpeed = Tr.Speed;
			I.Enchantability = Tr.Ench;
			I.RepairItem = FName(Tr.Repair);
			I.Family = FName(Tr.Name);
			float Dmg = Ty.BaseDamage + Tr.Bonus;
			float Spd = Ty.Speed;
			if (Ty.Tool == EMCTool::Axe)
			{
				static const float AxeDmg[] = { 7, 9, 9, 9, 7, 9, 10 };
				static const float AxeSpd[] = { 0.8f, 0.8f, 0.8f, 0.9f, 1.0f, 1.0f, 1.0f };
				const int32 Idx = (int32)(&Tr - Tiers);
				Dmg = AxeDmg[Idx]; Spd = AxeSpd[Idx];
			}
			if (Ty.Tool == EMCTool::Hoe)
			{
				static const float HoeSpd[] = { 1.f, 2.f, 2.5f, 3.f, 1.f, 4.f, 4.f };
				Spd = HoeSpd[(int32)(&Tr - Tiers)];
				Dmg = 1.f;
			}
			if (Ty.Kind == K::Spear) { I.Reach = 1.5f; Dmg = 2.f + Tr.Bonus; }
			I.AttackDamage = Dmg;
			I.AttackSpeed = Spd;
			I.Tags.Add(FName(*(FString(TEXT("tools")) + Ty.Suffix)));
			if (Tr.Tier == MCTier::Netherite) { I.bFireResistant = true; }
			if (FCString::Strcmp(Tr.Name, TEXT("wooden")) == 0) I.FuelTicks = 200;
		}
	}
	auto Tool = [&](const TCHAR* N, K Kind, T Tab, int32 Dur, float Dmg = 1.f, float Spd = 4.f) -> FMCItem&
	{
		FMCItem& I = R.Add(N, Kind, Tab, 1); I.MaxDamage = Dur; I.AttackDamage = Dmg; I.AttackSpeed = Spd; return I;
	};
	Tool(TEXT("shears"), K::Shears, T::Tools, 238).ToolType = EMCTool::Shears;
	Tool(TEXT("flint_and_steel"), K::FlintAndSteel, T::Tools, 64);
	Tool(TEXT("fishing_rod"), K::FishingRod, T::Tools, 64).Enchantability = 1;
	Tool(TEXT("bow"), K::Bow, T::Combat, 384).Enchantability = 1;
	Tool(TEXT("crossbow"), K::Crossbow, T::Combat, 465).Enchantability = 1;
	{ FMCItem& Tr = Tool(TEXT("trident"), K::Trident, T::Combat, 250, 9.f, 1.1f); Tr.Rarity = EMCRarity::Rare; Tr.Enchantability = 1; }
	Tool(TEXT("shield"), K::Shield, T::Combat, 336);
	{ FMCItem& Mc = Tool(TEXT("mace"), K::Mace, T::Combat, 500, 6.f, 0.6f); Mc.Rarity = EMCRarity::Epic; Mc.Enchantability = 15; }
	Tool(TEXT("brush"), K::Brush, T::Tools, 64);
	Tool(TEXT("carrot_on_a_stick"), K::Carrot, T::Tools, 25);
	Tool(TEXT("warped_fungus_on_a_stick"), K::Carrot, T::Tools, 100);
	R.Add(TEXT("elytra"), K::Elytra, T::Tools, 1).MaxDamage = 432;
	FindLast(TEXT("elytra"))->Rarity = EMCRarity::Epic;
	FindLast(TEXT("elytra"))->ArmorSlot = EMCArmorSlot::Chest;
	R.Add(TEXT("compass"), K::Compass, T::Tools, 64);
	R.Add(TEXT("recovery_compass"), K::Compass, T::Tools, 64).Rarity = EMCRarity::Uncommon;
	R.Add(TEXT("clock"), K::Clock, T::Tools, 64);
	R.Add(TEXT("spyglass"), K::Spyglass, T::Tools, 1);
	R.Add(TEXT("map"), K::Map, T::Tools, 64);
	R.Add(TEXT("filled_map"), K::Map, T::Tools, 64).bHidden = true;
	R.Add(TEXT("bucket"), K::Bucket, T::Tools, 16);
	{ FMCItem& B = R.Add(TEXT("water_bucket"), K::Bucket, T::Tools, 1); B.Fluid = TEXT("water"); }
	{ FMCItem& B = R.Add(TEXT("lava_bucket"), K::Bucket, T::Tools, 1); B.Fluid = TEXT("lava"); B.FuelTicks = 20000; }
	{ FMCItem& B = R.Add(TEXT("powder_snow_bucket"), K::Bucket, T::Tools, 1); B.Fluid = TEXT("powder_snow"); }
	{ FMCItem& B = R.Add(TEXT("milk_bucket"), K::Bucket, T::Food, 1); B.Fluid = TEXT("milk"); }
	const TCHAR* FishBuckets[] = { TEXT("cod_bucket"), TEXT("salmon_bucket"), TEXT("pufferfish_bucket"), TEXT("tropical_fish_bucket"), TEXT("axolotl_bucket"), TEXT("tadpole_bucket") };
	for (const TCHAR* FB : FishBuckets) { FMCItem& B = R.Add(FB, K::Bucket, T::Tools, 1); B.Fluid = TEXT("water"); FString S(FB); B.SpawnMob = FName(*S.LeftChop(7)); }
	R.Add(TEXT("lead"), K::Lead, T::Tools, 64);
	R.Add(TEXT("name_tag"), K::NameTag, T::Tools, 64);
	R.Add(TEXT("saddle"), K::Saddle, T::Tools, 1);
	const TCHAR* Harness[] = { TEXT("white_harness"), TEXT("black_harness"), TEXT("red_harness"), TEXT("blue_harness") };
	for (const TCHAR* H : Harness) R.Add(H, K::Harness, T::Tools, 1);
	R.Add(TEXT("totem_of_undying"), K::Totem, T::Combat, 1).Rarity = EMCRarity::Uncommon;
	R.Add(TEXT("ender_eye"), K::EyeOfEnder, T::Ingredients, 64).DisplayName = TEXT("Eye of Ender");
	R.Add(TEXT("end_crystal"), K::EndCrystal, T::Combat, 64).Rarity = EMCRarity::Rare;
	R.Add(TEXT("firework_rocket"), K::Firework, T::Tools, 64);
	R.Add(TEXT("fire_charge"), K::FireCharge, T::Combat, 64);
	R.Add(TEXT("snowball"), K::Snowball, T::Combat, 16);
	R.Add(TEXT("egg"), K::Egg, T::Combat, 16);
	R.Add(TEXT("experience_bottle"), K::ExpBottle, T::Tools, 64).Rarity = EMCRarity::Uncommon;
	R.Add(TEXT("arrow"), K::Projectile, T::Combat, 64);
	R.Add(TEXT("spectral_arrow"), K::Projectile, T::Combat, 64);
	R.Add(TEXT("tipped_arrow"), K::Projectile, T::Combat, 64).bHidden = true;
	R.Add(TEXT("writable_book"), K::Book, T::Tools, 1);
	R.Add(TEXT("enchanted_book"), K::Book, T::Tools, 1).Rarity = EMCRarity::Uncommon;
	FindLast(TEXT("enchanted_book"))->bGlint = true;
	R.Add(TEXT("glass_bottle"), K::Bottle, T::Ingredients, 64);
	R.Add(TEXT("goat_horn"), K::Horn, T::Tools, 1);
	R.Add(TEXT("bundle"), K::Bundle, T::Tools, 1);
	R.Add(TEXT("armor_stand"), K::Misc, T::Functional, 16);
	R.Add(TEXT("item_frame"), K::Misc, T::Functional, 64);
	R.Add(TEXT("painting"), K::Misc, T::Functional, 64);
	R.Add(TEXT("music_disc_opus55"), K::Music, T::Tools, 1).Rarity = EMCRarity::Rare;
	R.Add(TEXT("music_disc_voxel"), K::Music, T::Tools, 1).Rarity = EMCRarity::Rare;

	// potions
	{ FMCItem& P = R.Add(TEXT("potion"), K::Potion, T::Food, 1); P.DisplayName = TEXT("Potion"); }
	{ FMCItem& P = R.Add(TEXT("splash_potion"), K::Potion, T::Combat, 1); P.DisplayName = TEXT("Splash Potion"); }
	{ FMCItem& P = R.Add(TEXT("lingering_potion"), K::Potion, T::Combat, 1); P.DisplayName = TEXT("Lingering Potion"); }

	// vehicles
	const TCHAR* BoatWoods[] = { TEXT("oak"), TEXT("spruce"), TEXT("birch"), TEXT("jungle"), TEXT("acacia"), TEXT("dark_oak"), TEXT("mangrove"), TEXT("cherry"), TEXT("pale_oak") };
	for (const TCHAR* W : BoatWoods)
	{
		{ FMCItem& B = R.Add(*(FString(W) + TEXT("_boat")), K::Boat, T::Tools, 1); B.Family = FName(W); B.FuelTicks = 1200; }
		{ FMCItem& B = R.Add(*(FString(W) + TEXT("_chest_boat")), K::Boat, T::Tools, 1); B.Family = FName(W); B.FuelTicks = 1200; }
	}
	{ FMCItem& B = R.Add(TEXT("bamboo_raft"), K::Boat, T::Tools, 1); B.Family = TEXT("bamboo"); }
	R.Add(TEXT("minecart"), K::Minecart, T::Tools, 1);
	R.Add(TEXT("chest_minecart"), K::Minecart, T::Tools, 1);
	R.Add(TEXT("furnace_minecart"), K::Minecart, T::Tools, 1);
	R.Add(TEXT("hopper_minecart"), K::Minecart, T::Tools, 1);
	R.Add(TEXT("tnt_minecart"), K::Minecart, T::Tools, 1);

	// ---------------------------------------------------------------- armor
	struct FArmorMat { const TCHAR* Name; int32 DurMul; int32 Pts[4]; float Tough; float KB; int32 Ench; const TCHAR* Repair; };
	const FArmorMat Mats[] = {
		{ TEXT("leather"), 5, { 1, 3, 2, 1 }, 0.f, 0.f, 15, TEXT("leather") },
		{ TEXT("copper"), 11, { 2, 4, 3, 1 }, 0.f, 0.f, 8, TEXT("copper_ingot") },
		{ TEXT("golden"), 7, { 2, 5, 3, 1 }, 0.f, 0.f, 25, TEXT("gold_ingot") },
		{ TEXT("chainmail"), 15, { 2, 5, 4, 1 }, 0.f, 0.f, 12, TEXT("iron_ingot") },
		{ TEXT("iron"), 15, { 2, 6, 5, 2 }, 0.f, 0.f, 9, TEXT("iron_ingot") },
		{ TEXT("diamond"), 33, { 3, 8, 6, 3 }, 2.f, 0.f, 10, TEXT("diamond") },
		{ TEXT("netherite"), 37, { 3, 8, 6, 3 }, 3.f, 0.1f, 15, TEXT("netherite_ingot") },
	};
	const TCHAR* Pieces[4] = { TEXT("_helmet"), TEXT("_chestplate"), TEXT("_leggings"), TEXT("_boots") };
	const int32 BaseDur[4] = { 11, 16, 15, 13 };
	const EMCArmorSlot Slots[4] = { EMCArmorSlot::Head, EMCArmorSlot::Chest, EMCArmorSlot::Legs, EMCArmorSlot::Feet };
	for (const FArmorMat& M : Mats)
	{
		for (int32 p = 0; p < 4; ++p)
		{
			FMCItem& I = R.Add(*(FString(M.Name) + Pieces[p]), K::Armor, T::Combat, 1);
			I.ArmorSlot = Slots[p];
			I.ArmorPoints = M.Pts[p];
			I.Toughness = M.Tough;
			I.KnockbackResist = M.KB;
			I.MaxDamage = BaseDur[p] * M.DurMul;
			I.Enchantability = M.Ench;
			I.RepairItem = FName(M.Repair);
			I.ArmorMaterial = FName(M.Name);
			I.bFireResistant = FCString::Strcmp(M.Name, TEXT("netherite")) == 0;
		}
	}
	{ FMCItem& T2 = R.Add(TEXT("turtle_helmet"), K::Armor, T::Combat, 1); T2.ArmorSlot = EMCArmorSlot::Head; T2.ArmorPoints = 2; T2.MaxDamage = 275; T2.ArmorMaterial = TEXT("turtle"); }
	const TCHAR* HorseArmor[] = { TEXT("leather_horse_armor"), TEXT("iron_horse_armor"), TEXT("golden_horse_armor"), TEXT("diamond_horse_armor"), TEXT("wolf_armor") };
	for (const TCHAR* H : HorseArmor) { FMCItem& I = R.Add(H, K::Armor, T::Combat, 1); I.ArmorSlot = EMCArmorSlot::Body; }

	// ---------------------------------------------------------------- food (nutrition, saturation modifier)
	struct FFoodDef { const TCHAR* Name; int32 Nut; float Sat; bool bFast; bool bAlways; };
	const FFoodDef Foods[] = {
		{ TEXT("apple"), 4, 0.3f, false, false }, { TEXT("golden_apple"), 4, 1.2f, false, true }, { TEXT("enchanted_golden_apple"), 4, 1.2f, false, true },
		{ TEXT("bread"), 5, 0.6f, false, false }, { TEXT("beef"), 3, 0.3f, false, false }, { TEXT("cooked_beef"), 8, 0.8f, false, false },
		{ TEXT("porkchop"), 3, 0.3f, false, false }, { TEXT("cooked_porkchop"), 8, 0.8f, false, false }, { TEXT("chicken"), 2, 0.3f, false, false },
		{ TEXT("cooked_chicken"), 6, 0.6f, false, false }, { TEXT("mutton"), 2, 0.3f, false, false }, { TEXT("cooked_mutton"), 6, 0.8f, false, false },
		{ TEXT("rabbit"), 3, 0.3f, false, false }, { TEXT("cooked_rabbit"), 5, 0.6f, false, false }, { TEXT("cod"), 2, 0.1f, false, false },
		{ TEXT("cooked_cod"), 5, 0.6f, false, false }, { TEXT("salmon"), 2, 0.1f, false, false }, { TEXT("cooked_salmon"), 6, 0.8f, false, false },
		{ TEXT("tropical_fish"), 1, 0.1f, false, false }, { TEXT("pufferfish"), 1, 0.1f, false, false }, { TEXT("potato"), 1, 0.3f, false, false },
		{ TEXT("baked_potato"), 5, 0.6f, false, false }, { TEXT("poisonous_potato"), 2, 0.3f, false, false }, { TEXT("carrot"), 3, 0.6f, false, false },
		{ TEXT("golden_carrot"), 6, 1.2f, false, false }, { TEXT("beetroot"), 1, 0.6f, false, false }, { TEXT("beetroot_soup"), 6, 0.6f, false, false },
		{ TEXT("mushroom_stew"), 6, 0.6f, false, false }, { TEXT("rabbit_stew"), 10, 0.6f, false, false }, { TEXT("suspicious_stew"), 6, 0.6f, false, true },
		{ TEXT("melon_slice"), 2, 0.3f, false, false }, { TEXT("cookie"), 2, 0.1f, false, false }, { TEXT("pumpkin_pie"), 8, 0.3f, false, false },
		{ TEXT("dried_kelp"), 1, 0.3f, true, false }, { TEXT("rotten_flesh"), 4, 0.1f, false, false }, { TEXT("spider_eye"), 2, 0.8f, false, false },
		{ TEXT("chorus_fruit"), 4, 0.3f, false, true }, { TEXT("honey_bottle"), 6, 0.1f, false, false },
		{ TEXT("sweet_berries"), 2, 0.1f, false, false }, { TEXT("glow_berries"), 2, 0.1f, false, false },
	};
	for (const FFoodDef& F : Foods)
	{
		FMCItem* Existing = nullptr;
		for (int32 i = R.Items.Num() - 1; i >= 0; --i) if (R.Items[i].Name == F.Name) { Existing = &R.Items[i]; break; }
		FMCItem& I = Existing ? *Existing : R.Add(F.Name, K::Food, T::Food);
		if (F.Nut == 0) { I.bHidden = true; continue; }
		I.Kind = K::Food;
		I.Tab = T::Food;
		I.Food = MakeShared<FMCFood>();
		I.Food->Nutrition = F.Nut;
		I.Food->Saturation = F.Sat;
		I.Food->bFastEat = F.bFast;
		I.Food->bAlwaysEdible = F.bAlways;
		I.Food->EatSeconds = F.bFast ? 0.8f : 1.6f;
		const FString N = F.Name;
		if (N.EndsWith(TEXT("_stew")) || N.EndsWith(TEXT("_soup"))) { I.MaxStack = 1; I.Food->Remainder = TEXT("bowl"); }
		if (N == TEXT("honey_bottle")) { I.MaxStack = 16; I.Food->Remainder = TEXT("glass_bottle"); }
		auto Eff = [&](EMCEffect E, int32 Dur, uint8 Amp, float Chance)
		{
			FMCEffectInstance X; X.Effect = E; X.Duration = Dur; X.Amplifier = Amp;
			I.Food->Effects.Add(TPair<FMCEffectInstance, float>(X, Chance));
		};
		if (N == TEXT("golden_apple")) { Eff(EMCEffect::Regeneration, 100, 1, 1.f); Eff(EMCEffect::Absorption, 2400, 0, 1.f); I.Rarity = EMCRarity::Rare; }
		if (N == TEXT("enchanted_golden_apple")) { Eff(EMCEffect::Regeneration, 400, 1, 1.f); Eff(EMCEffect::Absorption, 2400, 3, 1.f); Eff(EMCEffect::Resistance, 6000, 0, 1.f); Eff(EMCEffect::FireResistance, 6000, 0, 1.f); I.Rarity = EMCRarity::Epic; I.bGlint = true; }
		if (N == TEXT("rotten_flesh")) Eff(EMCEffect::Hunger, 600, 0, 0.8f);
		if (N == TEXT("chicken")) Eff(EMCEffect::Hunger, 600, 0, 0.3f);
		if (N == TEXT("spider_eye") || N == TEXT("poisonous_potato")) Eff(EMCEffect::Poison, 100, 0, N == TEXT("spider_eye") ? 1.f : 0.6f);
		if (N == TEXT("pufferfish")) { Eff(EMCEffect::Hunger, 300, 2, 1.f); Eff(EMCEffect::Nausea, 300, 0, 1.f); Eff(EMCEffect::Poison, 1200, 1, 1.f); }
		if (N == TEXT("golden_carrot")) I.Rarity = EMCRarity::Common;
	}
	R.Add(TEXT("bowl"), K::Misc, T::Ingredients, 64).FuelTicks = 100;
	FindLast(TEXT("carrot"))->Block = FMCBlocks::FindId(TEXT("carrots"));
	FindLast(TEXT("potato"))->Block = FMCBlocks::FindId(TEXT("potatoes"));

	// ---------------------------------------------------------------- spawn eggs (mob list is authoritative in the mob registry; names here)
	const TCHAR* Mobs[] = {
		TEXT("pig"), TEXT("cow"), TEXT("sheep"), TEXT("chicken"), TEXT("villager"), TEXT("horse"), TEXT("wolf"), TEXT("iron_golem"),
		TEXT("zombie"), TEXT("skeleton"), TEXT("creeper"), TEXT("spider"), TEXT("enderman"), TEXT("slime"), TEXT("witch"), TEXT("drowned"),
		TEXT("ghast"), TEXT("blaze"), TEXT("piglin"), TEXT("zombified_piglin"), TEXT("hoglin"), TEXT("magma_cube"), TEXT("wither_skeleton"),
		TEXT("strider"), TEXT("shulker"), TEXT("ender_dragon"), TEXT("wither"), TEXT("allay"), TEXT("armadillo"), TEXT("axolotl"), TEXT("bat"),
		TEXT("camel"), TEXT("cat"), TEXT("cod"), TEXT("dolphin"), TEXT("donkey"), TEXT("fox"), TEXT("frog"), TEXT("glow_squid"), TEXT("goat"),
		TEXT("happy_ghast"), TEXT("ghastling"), TEXT("llama"), TEXT("mooshroom"), TEXT("mule"), TEXT("nautilus"), TEXT("ocelot"), TEXT("panda"),
		TEXT("parrot"), TEXT("polar_bear"), TEXT("pufferfish"), TEXT("rabbit"), TEXT("salmon"), TEXT("skeleton_horse"), TEXT("sniffer"),
		TEXT("snow_golem"), TEXT("squid"), TEXT("tadpole"), TEXT("trader_llama"), TEXT("tropical_fish"), TEXT("turtle"), TEXT("wandering_trader"),
		TEXT("zombie_horse"), TEXT("copper_golem"), TEXT("sulfur_cube"), TEXT("bee"), TEXT("bogged"), TEXT("breeze"), TEXT("cave_spider"),
		TEXT("creaking"), TEXT("elder_guardian"), TEXT("endermite"), TEXT("evoker"), TEXT("guardian"), TEXT("husk"), TEXT("parched"),
		TEXT("phantom"), TEXT("piglin_brute"), TEXT("pillager"), TEXT("ravager"), TEXT("silverfish"), TEXT("stray"), TEXT("vex"),
		TEXT("vindicator"), TEXT("warden"), TEXT("zoglin"), TEXT("zombie_villager"), TEXT("zombie_nautilus"), TEXT("camel_husk")
	};
	for (const TCHAR* M : Mobs)
	{
		FMCItem& I = R.Add(*(FString(M) + TEXT("_spawn_egg")), K::SpawnEgg, T::SpawnEggs, 64);
		I.SpawnMob = FName(M);
		if (FCString::Strcmp(M, TEXT("ender_dragon")) == 0 || FCString::Strcmp(M, TEXT("wither")) == 0) I.Rarity = EMCRarity::Epic;
	}
}
