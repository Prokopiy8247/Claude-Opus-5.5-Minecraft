// Block registration: registrar helpers + natural terrain, ores and mineral blocks.
#include "Blocks/MCBlockRegistrar.h"

void MCRegisterWoodBlocks(FMCBlockRegistrar& R);
void MCRegisterBuildingBlocks(FMCBlockRegistrar& R);
void MCRegisterColoredBlocks(FMCBlockRegistrar& R);
void MCRegisterFunctionalBlocks(FMCBlockRegistrar& R);
void MCRegisterRedstoneBlocks(FMCBlockRegistrar& R);
void MCRegisterPlantBlocks(FMCBlockRegistrar& R);
void MCRegisterNetherEndBlocks(FMCBlockRegistrar& R);

FString FMCBlockRegistrar::Pretty(const TCHAR* Id)
{
	FString S(Id);
	TArray<FString> Parts;
	S.ParseIntoArray(Parts, TEXT("_"));
	FString Out;
	for (FString& P : Parts)
	{
		if (P.Len() == 0) continue;
		if (P == TEXT("of") || P == TEXT("the") || P == TEXT("on") || P == TEXT("a")) { Out += (Out.IsEmpty() ? P : TEXT(" ") + P); continue; }
		P[0] = FChar::ToUpper(P[0]);
		Out += (Out.IsEmpty() ? P : TEXT(" ") + P);
	}
	return Out;
}

FMCBlockBuilder FMCBlockRegistrar::Add(const TCHAR* Name, const TCHAR* Display)
{
	FMCBlock& B = Blocks.AddDefaulted_GetRef();
	B.Name = FName(Name);
	B.DisplayName = Display ? FString(Display) : Pretty(Name);
	B.TexNames[0] = B.TexNames[1] = B.TexNames[2] = B.TexNames[3] = B.TexNames[4] = B.TexNames[5] = FName(Name);
	B.Flags = MCB_Solid | MCB_Opaque;
	B.LightOpacity = 15;
	B.Hardness = 1.f;
	B.BlastResistance = 1.f;
	return FMCBlockBuilder(B);
}

FMCBlock* FMCBlockRegistrar::Find(const TCHAR* Name)
{
	const FName N(Name);
	for (int32 i = Blocks.Num() - 1; i >= 0; --i)
	{
		if (Blocks[i].Name == N) return &Blocks[i];
	}
	return nullptr;
}

static FString StripSuffix(const FString& S, const TCHAR* Suffix)
{
	return S.EndsWith(Suffix) ? S.LeftChop(FCString::Strlen(Suffix)) : S;
}

/** Base name used for derived pieces: "stone_bricks" -> "stone_brick", "oak_planks" -> "oak", "bricks" -> "brick". */
static FString DerivedBase(const FString& Base)
{
	if (Base.EndsWith(TEXT("_planks"))) return StripSuffix(Base, TEXT("_planks"));
	if (Base.EndsWith(TEXT("_bricks"))) return StripSuffix(Base, TEXT("_bricks")) + TEXT("_brick");
	if (Base.EndsWith(TEXT("_tiles"))) return StripSuffix(Base, TEXT("_tiles")) + TEXT("_tile");
	if (Base == TEXT("bricks")) return TEXT("brick");
	if (Base.EndsWith(TEXT("_block")) && !Base.StartsWith(TEXT("purpur"))) return StripSuffix(Base, TEXT("_block"));
	if (Base == TEXT("purpur_block")) return TEXT("purpur");
	return Base;
}

FMCBlockBuilder FMCBlockRegistrar::Derive(const TCHAR* Base, const TCHAR* Name, const TCHAR* Suffix)
{
	const FMCBlock* BaseB = Find(Base);
	check(BaseB);
	const FMCBlock Copy = *BaseB;
	const FString FinalName = Name ? FString(Name) : DerivedBase(Base) + Suffix;
	FMCBlockBuilder Bd = Add(*FinalName);
	FMCBlock& B = Bd.B;
	for (int32 i = 0; i < 6; ++i) B.TexNames[i] = Copy.TexNames[i];
	B.Hardness = Copy.Hardness;
	B.BlastResistance = Copy.BlastResistance;
	B.Tool = Copy.Tool;
	B.MinTier = Copy.MinTier;
	B.bRequiresTool = Copy.bRequiresTool;
	B.Sound = Copy.Sound;
	B.Tab = EMCTab::Building;
	B.Tint = Copy.Tint;
	B.TintColor = Copy.TintColor;
	B.MapColor = Copy.MapColor;
	B.Family = Copy.Family.IsNone() ? Copy.Name : Copy.Family;
	B.FireSpread = Copy.FireSpread;
	B.FireBurn = Copy.FireBurn;
	B.Flags = MCB_Solid | (Copy.Flags & (MCB_Flammable));
	if (Copy.Layer == EMCLayer::Cutout || Copy.Layer == EMCLayer::Translucent) B.Layer = Copy.Layer;
	// axis-oriented bases (logs / pillars): use side texture everywhere except caps
	if (Copy.Orient == EMCCubeOrient::Axis)
	{
		for (int32 i = 2; i < 6; ++i) B.TexNames[i] = Copy.TexNames[(int32)EMCFace::North];
	}
	return Bd;
}

FMCBlockBuilder FMCBlockRegistrar::Slab(const TCHAR* Base, const TCHAR* Name)
{
	FMCBlockBuilder B = Derive(Base, Name, TEXT("_slab"));
	B.Model(EMCModel::Slab, 2).Beh(EMCBeh::Slab).Fam(*B.B.Family.ToString(), TEXT("slab"));
	return B;
}

FMCBlockBuilder FMCBlockRegistrar::Stairs(const TCHAR* Base, const TCHAR* Name)
{
	FMCBlockBuilder B = Derive(Base, Name, TEXT("_stairs"));
	B.Model(EMCModel::Stairs, 3).Beh(EMCBeh::Stairs).Fam(*B.B.Family.ToString(), TEXT("stairs"));
	return B;
}

FMCBlockBuilder FMCBlockRegistrar::Wall(const TCHAR* Base, const TCHAR* Name)
{
	FMCBlockBuilder B = Derive(Base, Name, TEXT("_wall"));
	B.Model(EMCModel::Wall, 0).Tab(EMCTab::Building).Fam(*B.B.Family.ToString(), TEXT("wall")).Tag(TEXT("walls"));
	return B;
}

FMCBlockBuilder FMCBlockRegistrar::Fence(const TCHAR* Base, const TCHAR* Name, bool bNether)
{
	FMCBlockBuilder B = Derive(Base, Name, TEXT("_fence"));
	B.Model(EMCModel::Fence, 0).Fam(*B.B.Family.ToString(), TEXT("fence")).Tag(TEXT("fences")).Tab(EMCTab::Building);
	if (!bNether) B.Tag(TEXT("wooden_fences")).Fuel(300);
	return B;
}

FMCBlockBuilder FMCBlockRegistrar::FenceGate(const TCHAR* Base, const TCHAR* Name)
{
	FMCBlockBuilder B = Derive(Base, Name, TEXT("_fence_gate"));
	B.Model(EMCModel::FenceGate, 4).Beh(EMCBeh::FenceGate).Flag(MCB_Interact | MCB_Redstone).Fam(*B.B.Family.ToString(), TEXT("fence_gate")).Tab(EMCTab::Redstone);
	return B;
}

FMCBlockBuilder FMCBlockRegistrar::Button(const TCHAR* Base, const TCHAR* Name, bool bWood)
{
	FMCBlockBuilder B = Derive(Base, Name, TEXT("_button"));
	B.Model(EMCModel::Button, 5).Beh(EMCBeh::Button).NoCollision().Flag(MCB_Interact | MCB_Redstone | MCB_NeedsSupport)
		.Hard(0.5f).Tool(EMCTool::None, 0, false).Tab(EMCTab::Redstone).Fam(*B.B.Family.ToString(), bWood ? TEXT("wood_button") : TEXT("stone_button"));
	return B;
}

FMCBlockBuilder FMCBlockRegistrar::PressurePlate(const TCHAR* Base, const TCHAR* Name, bool bWood)
{
	FMCBlockBuilder B = Derive(Base, Name, TEXT("_pressure_plate"));
	B.Model(EMCModel::PressurePlate, 1).Beh(EMCBeh::PressurePlate).NoCollision().Flag(MCB_Redstone | MCB_NeedsSupport)
		.Hard(0.5f).Tab(EMCTab::Redstone).Fam(*B.B.Family.ToString(), bWood ? TEXT("wood_plate") : TEXT("stone_plate"));
	return B;
}

void FMCBlockRegistrar::StoneSet(const TCHAR* Base, bool bWall, const TCHAR* StairsName, const TCHAR* SlabName, const TCHAR* WallName)
{
	Stairs(Base, StairsName);
	Slab(Base, SlabName);
	if (bWall) Wall(Base, WallName);
}

// ---------------------------------------------------------------------------------------------------------------------

static void RegisterNatural(FMCBlockRegistrar& R)
{
	using T = EMCTab;
	// 0: air must be first
	R.Add(TEXT("air"), TEXT("Air")).Shape(EMCShape::Air).NoFlag(MCB_Solid | MCB_Opaque).Opacity(0).Hard(0).Tab(T::None);

	R.Add(TEXT("stone")).Hard(1.5f, 6).Pick().DropItem(TEXT("cobblestone")).Snd(EMCSound::Stone).Tab(T::Natural).Flag(MCB_Stone).Map(0x707070).RandomRotate().Fam(TEXT("stone"), TEXT("base"));
	R.Add(TEXT("granite")).Hard(1.5f, 6).Pick().Tab(T::Natural).Flag(MCB_Stone).Map(0x976D4D).Fam(TEXT("granite"), TEXT("base"));
	R.Add(TEXT("polished_granite")).Hard(1.5f, 6).Pick().Tab(T::Building).Fam(TEXT("polished_granite"), TEXT("base"));
	R.Add(TEXT("diorite")).Hard(1.5f, 6).Pick().Tab(T::Natural).Flag(MCB_Stone).Map(0xFFFCF5).Fam(TEXT("diorite"), TEXT("base"));
	R.Add(TEXT("polished_diorite")).Hard(1.5f, 6).Pick().Tab(T::Building).Fam(TEXT("polished_diorite"), TEXT("base"));
	R.Add(TEXT("andesite")).Hard(1.5f, 6).Pick().Tab(T::Natural).Flag(MCB_Stone).Map(0x707070).Fam(TEXT("andesite"), TEXT("base"));
	R.Add(TEXT("polished_andesite")).Hard(1.5f, 6).Pick().Tab(T::Building).Fam(TEXT("polished_andesite"), TEXT("base"));
	R.Add(TEXT("tuff")).Hard(1.5f, 6).Pick().Snd(EMCSound::Deepslate).Tab(T::Natural).Flag(MCB_Stone).Fam(TEXT("tuff"), TEXT("base"));
	R.Add(TEXT("polished_tuff")).Hard(1.5f, 6).Pick().Snd(EMCSound::Deepslate).Tab(T::Building).Fam(TEXT("polished_tuff"), TEXT("base"));
	R.Add(TEXT("tuff_bricks")).Hard(1.5f, 6).Pick().Snd(EMCSound::Deepslate).Tab(T::Building).Fam(TEXT("tuff_bricks"), TEXT("base"));
	R.Add(TEXT("chiseled_tuff")).Hard(1.5f, 6).Pick().Snd(EMCSound::Deepslate).TexTS(TEXT("chiseled_tuff_top"), TEXT("chiseled_tuff")).Tab(T::Building);
	R.Add(TEXT("calcite")).Hard(0.75f).Pick().Snd(EMCSound::Stone).Tab(T::Natural).Flag(MCB_Stone).Map(0xDFE0DA);
	R.Add(TEXT("dripstone_block")).Hard(1.5f, 1).Pick().Snd(EMCSound::Stone).Tab(T::Natural).Flag(MCB_Stone).Map(0x866B5C);

	R.Add(TEXT("deepslate")).Hard(3.f, 6).Pick().DropItem(TEXT("cobbled_deepslate")).Snd(EMCSound::Deepslate).Tab(T::Natural)
		.TexTS(TEXT("deepslate_top"), TEXT("deepslate")).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Flag(MCB_Stone).Map(0x4A4A4F);
	R.Add(TEXT("cobbled_deepslate")).Hard(3.5f, 6).Pick().Snd(EMCSound::Deepslate).Tab(T::Building).Fam(TEXT("cobbled_deepslate"), TEXT("base"));
	R.Add(TEXT("polished_deepslate")).Hard(3.5f, 6).Pick().Snd(EMCSound::Deepslate).Tab(T::Building);
	R.Add(TEXT("deepslate_bricks")).Hard(3.5f, 6).Pick().Snd(EMCSound::Deepslate).Tab(T::Building);
	R.Add(TEXT("cracked_deepslate_bricks")).Hard(3.5f, 6).Pick().Snd(EMCSound::Deepslate).Tab(T::Building);
	R.Add(TEXT("deepslate_tiles")).Hard(3.5f, 6).Pick().Snd(EMCSound::Deepslate).Tab(T::Building);
	R.Add(TEXT("cracked_deepslate_tiles")).Hard(3.5f, 6).Pick().Snd(EMCSound::Deepslate).Tab(T::Building);
	R.Add(TEXT("chiseled_deepslate")).Hard(3.5f, 6).Pick().Snd(EMCSound::Deepslate).Tab(T::Building);
	R.Add(TEXT("reinforced_deepslate")).Hard(55.f, 1200).DropNone().TexTS(TEXT("reinforced_deepslate_top"), TEXT("reinforced_deepslate_side")).Tab(T::Natural);

	R.Add(TEXT("bedrock")).Hard(-1.f, 3600000).DropNone().Tab(T::Natural).Beh(EMCBeh::Bedrock).RandomRotate();

	// soils
	R.Add(TEXT("grass_block")).Hard(0.6f).Shovel().DropItem(TEXT("dirt")).Snd(EMCSound::Grass).Tab(T::Natural)
		.TexTBS(TEXT("grass_block_top"), TEXT("dirt"), TEXT("grass_block_side")).Tint(EMCTint::Grass).Meta(1).Orient(EMCCubeOrient::Snowy).Alt(0, TEXT("grass_block_snow"))
		.Beh(EMCBeh::Grass).Ticks().Flag(MCB_Soil | MCB_Dirt).Map(0x7FB238).RandomRotate();
	R.Add(TEXT("dirt")).Hard(0.5f).Shovel().Snd(EMCSound::Gravel).Tab(T::Natural).Flag(MCB_Soil | MCB_Dirt).Map(0x976D4D).RandomRotate();
	R.Add(TEXT("coarse_dirt")).Hard(0.5f).Shovel().Snd(EMCSound::Gravel).Tab(T::Natural).Flag(MCB_Soil | MCB_Dirt);
	R.Add(TEXT("rooted_dirt")).Hard(0.5f).Shovel().Snd(EMCSound::Gravel).Tab(T::Natural).Flag(MCB_Soil | MCB_Dirt);
	R.Add(TEXT("podzol")).Hard(0.5f).Shovel().DropItem(TEXT("dirt")).Snd(EMCSound::Gravel).Tab(T::Natural)
		.TexTBS(TEXT("podzol_top"), TEXT("dirt"), TEXT("podzol_side")).Flag(MCB_Soil | MCB_Dirt).RandomRotate();
	R.Add(TEXT("mycelium")).Hard(0.6f).Shovel().DropItem(TEXT("dirt")).Snd(EMCSound::Grass).Tab(T::Natural)
		.TexTBS(TEXT("mycelium_top"), TEXT("dirt"), TEXT("mycelium_side")).Beh(EMCBeh::Mycelium).Ticks().Flag(MCB_Soil | MCB_Dirt).RandomRotate();
	R.Add(TEXT("dirt_path")).Hard(0.65f).Shovel().DropItem(TEXT("dirt")).Snd(EMCSound::Grass).Tab(T::Natural)
		.TexTBS(TEXT("dirt_path_top"), TEXT("dirt"), TEXT("dirt_path_side")).Model(EMCModel::Path).Opacity(15);
	R.Add(TEXT("farmland")).Hard(0.6f).Shovel().DropItem(TEXT("dirt")).Snd(EMCSound::Gravel).Tab(T::Natural)
		.TexTBS(TEXT("farmland"), TEXT("dirt"), TEXT("dirt")).Model(EMCModel::Farmland, 3).Alt(0, TEXT("farmland_moist")).Beh(EMCBeh::Farmland).Ticks().Opacity(15);
	R.Add(TEXT("mud")).Hard(0.5f).Shovel().Snd(EMCSound::Mud).Tab(T::Natural).Flag(MCB_Soil | MCB_Dirt).Speed(0.8f);
	R.Add(TEXT("packed_mud")).Hard(1.f, 3).Snd(EMCSound::Mud).Tab(T::Building);
	R.Add(TEXT("mud_bricks")).Hard(1.5f, 3).Pick().Snd(EMCSound::Stone).Tab(T::Building);
	R.Add(TEXT("muddy_mangrove_roots")).Hard(0.7f).Shovel().Snd(EMCSound::Mud).TexTS(TEXT("muddy_mangrove_roots_top"), TEXT("muddy_mangrove_roots_side")).Tab(T::Natural);
	R.Add(TEXT("moss_block")).Hard(0.1f).Hoe().Snd(EMCSound::Moss).Tab(T::Natural).Flag(MCB_Soil).Map(0x4E6A26);
	R.Add(TEXT("pale_moss_block")).Hard(0.1f).Hoe().Snd(EMCSound::Moss).Tab(T::Natural).Flag(MCB_Soil).Map(0x9CA396);
	R.Add(TEXT("clay")).Hard(0.6f).Shovel().DropItem(TEXT("clay_ball"), 4, 4).Snd(EMCSound::Gravel).Tab(T::Natural).Map(0xA4A8B8);

	R.Add(TEXT("sand")).Hard(0.5f).Shovel().Snd(EMCSound::Sand).Tab(T::Natural).Beh(EMCBeh::Falling).Flag(MCB_Gravity | MCB_Sand).Map(0xF7E9A3).RandomRotate();
	R.Add(TEXT("red_sand")).Hard(0.5f).Shovel().Snd(EMCSound::Sand).Tab(T::Natural).Beh(EMCBeh::Falling).Flag(MCB_Gravity | MCB_Sand).Map(0xD87F33).RandomRotate();
	R.Add(TEXT("suspicious_sand")).Hard(0.25f).Shovel().DropNone().Snd(EMCSound::Sand).Tab(T::Natural).Beh(EMCBeh::Falling).Flag(MCB_Gravity);
	R.Add(TEXT("gravel")).Hard(0.6f).Shovel().DropSpecial().Snd(EMCSound::Gravel).Tab(T::Natural).Beh(EMCBeh::Falling).Flag(MCB_Gravity).Map(0x837D78).RandomRotate();
	R.Add(TEXT("suspicious_gravel")).Hard(0.25f).Shovel().DropNone().Snd(EMCSound::Gravel).Tab(T::Natural).Beh(EMCBeh::Falling).Flag(MCB_Gravity);
	R.Add(TEXT("sandstone")).Hard(0.8f).Pick().TexTBS(TEXT("sandstone_top"), TEXT("sandstone_bottom"), TEXT("sandstone")).Tab(T::Building).Flag(MCB_Stone).Fam(TEXT("sandstone"), TEXT("base"));
	R.Add(TEXT("chiseled_sandstone")).Hard(0.8f).Pick().TexTS(TEXT("sandstone_top"), TEXT("chiseled_sandstone")).Tab(T::Building);
	R.Add(TEXT("cut_sandstone")).Hard(0.8f).Pick().TexTS(TEXT("sandstone_top"), TEXT("cut_sandstone")).Tab(T::Building);
	R.Add(TEXT("smooth_sandstone")).Hard(2.f, 6).Pick().Tex(TEXT("sandstone_top")).Tab(T::Building);
	R.Add(TEXT("red_sandstone")).Hard(0.8f).Pick().TexTBS(TEXT("red_sandstone_top"), TEXT("red_sandstone_bottom"), TEXT("red_sandstone")).Tab(T::Building).Flag(MCB_Stone).Fam(TEXT("red_sandstone"), TEXT("base"));
	R.Add(TEXT("chiseled_red_sandstone")).Hard(0.8f).Pick().TexTS(TEXT("red_sandstone_top"), TEXT("chiseled_red_sandstone")).Tab(T::Building);
	R.Add(TEXT("cut_red_sandstone")).Hard(0.8f).Pick().TexTS(TEXT("red_sandstone_top"), TEXT("cut_red_sandstone")).Tab(T::Building);
	R.Add(TEXT("smooth_red_sandstone")).Hard(2.f, 6).Pick().Tex(TEXT("red_sandstone_top")).Tab(T::Building);

	// snow & ice
	R.Add(TEXT("snow"), TEXT("Snow")).Hard(0.1f).Shovel().DropItem(TEXT("snowball")).Snd(EMCSound::Snow).Tab(T::Natural)
		.Model(EMCModel::SnowLayer, 3).Beh(EMCBeh::SnowLayer).Replaceable().Flag(MCB_NeedsSupport).Ticks();
	R.Add(TEXT("snow_block")).Hard(0.2f).Shovel().DropItem(TEXT("snowball"), 4, 4).Tex(TEXT("snow")).Snd(EMCSound::Snow).Tab(T::Natural).Map(0xFFFFFF);
	R.Add(TEXT("powder_snow")).Hard(0.25f).DropNone().Snd(EMCSound::Snow).Tab(T::Natural).Beh(EMCBeh::PowderSnow).NoCollision().Opacity(1);
	R.Add(TEXT("ice")).Hard(0.5f).Pick(0).DropSilk().Snd(EMCSound::Glass).Tab(T::Natural).Translucent().Opacity(1)
		.Friction(0.98f).Beh(EMCBeh::Ice).Ticks().Flag(MCB_CullSame | MCB_Slippery).Map(0xA0A0FF);
	R.Add(TEXT("packed_ice")).Hard(0.5f).Pick(0).DropSilk().Snd(EMCSound::Glass).Tab(T::Natural).Friction(0.98f).Flag(MCB_Slippery);
	R.Add(TEXT("blue_ice")).Hard(2.8f).Pick(0).DropSilk().Snd(EMCSound::Glass).Tab(T::Natural).Friction(0.989f).Flag(MCB_Slippery);

	R.Add(TEXT("obsidian")).Hard(50.f, 1200).Pick(MCTier::Diamond).Snd(EMCSound::Stone).Tab(T::Building).Map(0x191919);
	R.Add(TEXT("crying_obsidian")).Hard(50.f, 1200).Pick(MCTier::Diamond).Light(10).Snd(EMCSound::Stone).Tab(T::Building);

	// fluids (meta = level)
	R.Add(TEXT("water"), TEXT("Water")).Shape(EMCShape::Liquid).NoFlag(MCB_Solid | MCB_Opaque).Flag(MCB_Fluid | MCB_Replaceable | MCB_NoItem)
		.Meta(4).Opacity(1).Hard(100.f, 100).DropNone().Tab(T::None).Tint(EMCTint::Water).Beh(EMCBeh::Water).Tex(TEXT("water_still"));
	R.Blocks.Last().Layer = EMCLayer::Water;
	R.Add(TEXT("lava"), TEXT("Lava")).Shape(EMCShape::Liquid).NoFlag(MCB_Solid | MCB_Opaque).Flag(MCB_Fluid | MCB_Replaceable | MCB_NoItem | MCB_Hot)
		.Meta(4).Opacity(1).Light(15).Hard(100.f, 100).DropNone().Tab(T::None).Beh(EMCBeh::Lava).Tex(TEXT("lava_still"));
	R.Blocks.Last().Layer = EMCLayer::Lava;

	// sulfur caves (26.2)
	R.Add(TEXT("sulfur")).Hard(1.2f, 4).Pick().Snd(EMCSound::Basalt).Tab(T::Natural).Flag(MCB_Stone).Map(0xC9B23A).Fam(TEXT("sulfur"), TEXT("base"));
	R.Add(TEXT("potent_sulfur")).Hard(1.5f, 4).Pick().Light(5).Snd(EMCSound::Amethyst).Tab(T::Natural).Beh(EMCBeh::PotentSulfur).Ticks().Map(0xE6D24A);
	R.Add(TEXT("polished_sulfur")).Hard(1.5f, 6).Pick().Tab(T::Building).Fam(TEXT("polished_sulfur"), TEXT("base"));
	R.Add(TEXT("sulfur_bricks")).Hard(1.5f, 6).Pick().Tab(T::Building).Fam(TEXT("sulfur_bricks"), TEXT("base"));
	R.Add(TEXT("cinnabar")).Hard(1.5f, 6).Pick().Snd(EMCSound::Stone).Tab(T::Natural).Flag(MCB_Stone).Map(0x9E2A24).Fam(TEXT("cinnabar"), TEXT("base"));
	R.Add(TEXT("polished_cinnabar")).Hard(1.5f, 6).Pick().Tab(T::Building).Fam(TEXT("polished_cinnabar"), TEXT("base"));
	R.Add(TEXT("cinnabar_bricks")).Hard(1.5f, 6).Pick().Tab(T::Building).Fam(TEXT("cinnabar_bricks"), TEXT("base"));
	R.Add(TEXT("sulfur_spike")).Hard(1.2f).Pick().Model(EMCModel::Dripstone, 4).Beh(EMCBeh::PointedDripstone).Light(3).Tab(T::Natural).Snd(EMCSound::Amethyst);
	R.Add(TEXT("pointed_dripstone")).Hard(1.5f, 3).Pick().Model(EMCModel::Dripstone, 4).Beh(EMCBeh::PointedDripstone).Tab(T::Natural).Snd(EMCSound::Stone);
}

static void RegisterOres(FMCBlockRegistrar& R)
{
	using T = EMCTab;
	struct FOre { const TCHAR* Name; const TCHAR* Drop; uint8 Min, Max; uint8 Tier; float XP0, XP1; };
	const FOre Ores[] = {
		{ TEXT("coal"), TEXT("coal"), 1, 1, MCTier::Wood, 0, 2 },
		{ TEXT("iron"), TEXT("raw_iron"), 1, 1, MCTier::Stone, 0, 0 },
		{ TEXT("copper"), TEXT("raw_copper"), 2, 5, MCTier::Stone, 0, 0 },
		{ TEXT("gold"), TEXT("raw_gold"), 1, 1, MCTier::Iron, 0, 0 },
		{ TEXT("redstone"), TEXT("redstone"), 4, 5, MCTier::Iron, 1, 5 },
		{ TEXT("lapis"), TEXT("lapis_lazuli"), 4, 9, MCTier::Stone, 2, 5 },
		{ TEXT("diamond"), TEXT("diamond"), 1, 1, MCTier::Iron, 3, 7 },
		{ TEXT("emerald"), TEXT("emerald"), 1, 1, MCTier::Iron, 3, 7 },
	};
	for (const FOre& O : Ores)
	{
		const FString N = FString(O.Name) + TEXT("_ore");
		const FString DN = FString(TEXT("deepslate_")) + O.Name + TEXT("_ore");
		auto A = R.Add(*N).Hard(3.f, 3).Pick(O.Tier).DropItem(O.Drop, O.Min, O.Max, true).XP(O.XP0, O.XP1).Tab(T::Natural).Tag(TEXT("ores")).Fam(O.Name, TEXT("ore"));
		auto B = R.Add(*DN).Hard(4.5f, 3).Pick(O.Tier).DropItem(O.Drop, O.Min, O.Max, true).XP(O.XP0, O.XP1).Snd(EMCSound::Deepslate).Tab(T::Natural).Tag(TEXT("ores")).Fam(O.Name, TEXT("ore"));
		if (FCString::Strcmp(O.Name, TEXT("redstone")) == 0)
		{
			A.Orient(EMCCubeOrient::LitToggle).Alt(0, TEXT("redstone_ore")).Beh(EMCBeh::RedstoneOre).Ticks();
			B.Orient(EMCCubeOrient::LitToggle).Alt(0, TEXT("deepslate_redstone_ore")).Beh(EMCBeh::RedstoneOre).Ticks();
		}
	}
	R.Add(TEXT("nether_gold_ore")).Hard(3.f, 3).Pick().DropItem(TEXT("gold_nugget"), 2, 6, true).XP(0, 1).Snd(EMCSound::Netherrack).Tab(T::Natural).Tag(TEXT("ores"));
	R.Add(TEXT("nether_quartz_ore")).Hard(3.f, 3).Pick().DropItem(TEXT("quartz"), 1, 1, true).XP(2, 5).Snd(EMCSound::Netherrack).Tab(T::Natural).Tag(TEXT("ores"));
	R.Add(TEXT("ancient_debris")).Hard(30.f, 1200).Pick(MCTier::Diamond).TexTS(TEXT("ancient_debris_top"), TEXT("ancient_debris_side")).Snd(EMCSound::Stone).Tab(T::Natural);

	R.Add(TEXT("coal_block")).Hard(5.f, 6).Pick().Fuel(16000).Burn(5, 5).Tab(T::Building);
	R.Add(TEXT("iron_block")).Hard(5.f, 6).Pick(MCTier::Stone).Snd(EMCSound::Metal).Tab(T::Building).Tag(TEXT("beacon_base"));
	R.Add(TEXT("gold_block")).Hard(3.f, 6).Pick(MCTier::Iron).Snd(EMCSound::Metal).Tab(T::Building).Tag(TEXT("beacon_base"));
	R.Add(TEXT("diamond_block")).Hard(5.f, 6).Pick(MCTier::Iron).Snd(EMCSound::Metal).Tab(T::Building).Tag(TEXT("beacon_base"));
	R.Add(TEXT("emerald_block")).Hard(5.f, 6).Pick(MCTier::Iron).Snd(EMCSound::Metal).Tab(T::Building).Tag(TEXT("beacon_base"));
	R.Add(TEXT("lapis_block")).Hard(3.f, 3).Pick(MCTier::Stone).Tab(T::Building);
	R.Add(TEXT("redstone_block")).Hard(5.f, 6).Pick().Snd(EMCSound::Metal).Tab(T::Redstone).Beh(EMCBeh::RedstoneBlock).Flag(MCB_Redstone);
	R.Add(TEXT("netherite_block")).Hard(50.f, 1200).Pick(MCTier::Diamond).Snd(EMCSound::Metal).Tab(T::Building).Tag(TEXT("beacon_base"));
	R.Add(TEXT("raw_iron_block")).Hard(5.f, 6).Pick(MCTier::Stone).Tab(T::Natural);
	R.Add(TEXT("raw_copper_block")).Hard(5.f, 6).Pick(MCTier::Stone).Tab(T::Natural);
	R.Add(TEXT("raw_gold_block")).Hard(5.f, 6).Pick(MCTier::Iron).Tab(T::Natural);
	R.Add(TEXT("amethyst_block")).Hard(1.5f).Pick().Snd(EMCSound::Amethyst).Tab(T::Natural);
	R.Add(TEXT("budding_amethyst")).Hard(1.5f).Pick().DropNone().Snd(EMCSound::Amethyst).Tab(T::Natural).Beh(EMCBeh::Budding).Ticks();
	R.Add(TEXT("amethyst_cluster")).Hard(1.5f).Pick().Model(EMCModel::AmethystCluster, 3).Beh(EMCBeh::Facing6Away).Light(5).DropItem(TEXT("amethyst_shard"), 4, 4, true).Snd(EMCSound::Amethyst).Tab(T::Natural);
	R.Blocks.Last().Layer = EMCLayer::Cutout;

	// quartz
	R.Add(TEXT("quartz_block")).Hard(0.8f).Pick().TexTBS(TEXT("quartz_block_top"), TEXT("quartz_block_bottom"), TEXT("quartz_block_side")).Tab(T::Building).Fam(TEXT("quartz"), TEXT("base"));
	R.Add(TEXT("chiseled_quartz_block")).Hard(0.8f).Pick().TexTS(TEXT("chiseled_quartz_block_top"), TEXT("chiseled_quartz_block")).Tab(T::Building);
	R.Add(TEXT("quartz_bricks")).Hard(0.8f).Pick().Tab(T::Building);
	R.Add(TEXT("quartz_pillar")).Hard(0.8f).Pick().TexTS(TEXT("quartz_pillar_top"), TEXT("quartz_pillar")).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Tab(T::Building);
	R.Add(TEXT("smooth_quartz")).Hard(2.f, 6).Pick().Tex(TEXT("quartz_block_bottom")).Tab(T::Building);

	// copper family with oxidation stages
	const TCHAR* Stages[4] = { TEXT(""), TEXT("exposed_"), TEXT("weathered_"), TEXT("oxidized_") };
	for (int32 S = 0; S < 4; ++S)
	{
		const FString P = Stages[S];
		const FString BlockName = S == 0 ? TEXT("copper_block") : P + TEXT("copper");
		R.Add(*BlockName, *FMCBlockRegistrar::Pretty(*(P + TEXT("copper_block")))).Tex(*(P + TEXT("copper_block"))).Hard(3.f, 6).Pick(MCTier::Stone)
			.Snd(EMCSound::Copper).Tab(T::Building).Beh(EMCBeh::CopperOxidize).Ticks().Fam(TEXT("copper"), *FString::FromInt(S));
		R.Add(*(P + TEXT("cut_copper"))).Hard(3.f, 6).Pick(MCTier::Stone).Snd(EMCSound::Copper).Tab(T::Building).Beh(EMCBeh::CopperOxidize).Ticks().Fam(TEXT("cut_copper"), *FString::FromInt(S));
		R.Add(*(P + TEXT("chiseled_copper"))).Hard(3.f, 6).Pick(MCTier::Stone).Snd(EMCSound::Copper).Tab(T::Building).Beh(EMCBeh::CopperOxidize).Ticks().Fam(TEXT("chiseled_copper"), *FString::FromInt(S));
		R.Add(*(P + TEXT("copper_grate"))).Hard(3.f, 6).Pick(MCTier::Stone).Snd(EMCSound::Copper).Tab(T::Building).Cutout().Opacity(0).Flag(MCB_CullSame).Beh(EMCBeh::CopperOxidize).Ticks().Fam(TEXT("copper_grate"), *FString::FromInt(S));
		// waxed twins (honeycomb on a copper block swaps it for waxed_<name>): same look, never oxidise
		const FString Wx = TEXT("waxed_");
		R.Add(*(Wx + BlockName), *FMCBlockRegistrar::Pretty(*(Wx + BlockName))).Tex(*(P + TEXT("copper_block"))).Hard(3.f, 6).Pick(MCTier::Stone)
			.Snd(EMCSound::Copper).Tab(T::Building).Fam(TEXT("waxed_copper"), *FString::FromInt(S));
		R.Add(*(Wx + P + TEXT("cut_copper"))).Tex(*(P + TEXT("cut_copper"))).Hard(3.f, 6).Pick(MCTier::Stone).Snd(EMCSound::Copper).Tab(T::Building).Fam(TEXT("waxed_cut_copper"), *FString::FromInt(S));
		R.Add(*(Wx + P + TEXT("chiseled_copper"))).Tex(*(P + TEXT("chiseled_copper"))).Hard(3.f, 6).Pick(MCTier::Stone).Snd(EMCSound::Copper).Tab(T::Building).Fam(TEXT("waxed_chiseled_copper"), *FString::FromInt(S));
		R.Add(*(Wx + P + TEXT("copper_grate"))).Tex(*(P + TEXT("copper_grate"))).Hard(3.f, 6).Pick(MCTier::Stone).Snd(EMCSound::Copper).Tab(T::Building).Cutout().Opacity(0).Flag(MCB_CullSame).Fam(TEXT("waxed_copper_grate"), *FString::FromInt(S));
		R.Add(*(P + TEXT("copper_bulb"))).Hard(3.f, 6).Pick(MCTier::Stone).Snd(EMCSound::Copper).Tab(T::Redstone).Orient(EMCCubeOrient::LitToggle)
			.Alt(0, *(P + TEXT("copper_bulb_lit"))).Meta(2).Beh(EMCBeh::CopperBulb).Flag(MCB_Redstone).Light(0).Fam(TEXT("copper_bulb"), *FString::FromInt(S));
		R.Add(*(P + TEXT("copper_door"))).Hard(3.f, 6).Pick(MCTier::Stone).Snd(EMCSound::Copper).Tab(T::Redstone)
			.TexTBS(*(P + TEXT("copper_door_top")), *(P + TEXT("copper_door_bottom")), *(P + TEXT("copper_door_bottom"))).Model(EMCModel::Door, 5).Beh(EMCBeh::Door).Flag(MCB_Interact | MCB_Redstone);
		R.Blocks.Last().Layer = EMCLayer::Cutout;
		R.Add(*(P + TEXT("copper_trapdoor"))).Hard(3.f, 6).Pick(MCTier::Stone).Snd(EMCSound::Copper).Tab(T::Redstone).Model(EMCModel::Trapdoor, 4).Beh(EMCBeh::Trapdoor).Flag(MCB_Interact | MCB_Redstone);
		R.Blocks.Last().Layer = EMCLayer::Cutout;
	}
	R.Add(TEXT("copper_bars")).Hard(5.f, 6).Pick().Model(EMCModel::Bars).Snd(EMCSound::Copper).Tab(T::Building);
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("copper_chain")).Hard(5.f, 6).Pick().Model(EMCModel::Chain, 2).Beh(EMCBeh::Chain).Snd(EMCSound::Chain).Tab(T::Building);
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("copper_lantern")).Hard(3.5f).Pick().Model(EMCModel::Lantern, 1).Beh(EMCBeh::Lantern).Light(15).Snd(EMCSound::Lantern).Tab(T::Functional).Mesh(TEXT("SM_Lantern_Copper"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("copper_torch")).Hard(0.f).Model(EMCModel::Torch, 3).Beh(EMCBeh::Torch).NoCollision().Light(14).Flag(MCB_NeedsSupport).Snd(EMCSound::Wood).Tab(T::Functional).Mesh(TEXT("SM_Torch_Copper"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
}

void MCRegisterAllBlocks(TArray<FMCBlock>& Blocks)
{
	MCBehaviors::RegisterAll();
	FMCBlockRegistrar R(Blocks);
	RegisterNatural(R);
	RegisterOres(R);
	MCRegisterWoodBlocks(R);
	MCRegisterBuildingBlocks(R);
	MCRegisterColoredBlocks(R);
	MCRegisterFunctionalBlocks(R);
	MCRegisterRedstoneBlocks(R);
	MCRegisterPlantBlocks(R);
	MCRegisterNetherEndBlocks(R);
}
