#include "Gen/MCGenCommon.h"
#include "Gen/MCWorldGen.h"
#include "World/MCBlockEntity.h"

// ---------------------------------------------------------------------------------------------------------------------
// Writer

void FMCGenWriter::Fill(int32 AX, int32 AY, int32 AZ, int32 BX, int32 BY, int32 BZ, FMCState S, bool bHollow, FMCState Inner)
{
	const int32 MinX = FMath::Max(FMath::Min(AX, BX), X0), MaxX = FMath::Min(FMath::Max(AX, BX), X0 + 15);
	const int32 MinY = FMath::Max(FMath::Min(AY, BY), Y0), MaxY = FMath::Min(FMath::Max(AY, BY), Y0 + 15);
	const int32 MinZ = FMath::Max(FMath::Min(AZ, BZ), MC::MinZ), MaxZ = FMath::Min(FMath::Max(AZ, BZ), MC::MaxZ);
	const int32 LoX = FMath::Min(AX, BX), HiX = FMath::Max(AX, BX), LoY = FMath::Min(AY, BY), HiY = FMath::Max(AY, BY), LoZ = FMath::Min(AZ, BZ), HiZ = FMath::Max(AZ, BZ);
	for (int32 Z = MinZ; Z <= MaxZ; ++Z)
		for (int32 Y = MinY; Y <= MaxY; ++Y)
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const bool bShell = X == LoX || X == HiX || Y == LoY || Y == HiY || Z == LoZ || Z == HiZ;
				C.SetRaw(X - X0, Y - Y0, Z, (bHollow && !bShell) ? Inner : S);
			}
}

void FMCGenWriter::FillIfAir(int32 AX, int32 AY, int32 AZ, int32 BX, int32 BY, int32 BZ, FMCState S)
{
	const int32 MinX = FMath::Max(FMath::Min(AX, BX), X0), MaxX = FMath::Min(FMath::Max(AX, BX), X0 + 15);
	const int32 MinY = FMath::Max(FMath::Min(AY, BY), Y0), MaxY = FMath::Min(FMath::Max(AY, BY), Y0 + 15);
	const int32 MinZ = FMath::Max(FMath::Min(AZ, BZ), MC::MinZ), MaxZ = FMath::Min(FMath::Max(AZ, BZ), MC::MaxZ);
	for (int32 Z = MinZ; Z <= MaxZ; ++Z)
		for (int32 Y = MinY; Y <= MaxY; ++Y)
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const FMCState Cur = C.Get(X - X0, Y - Y0, Z);
				if (Cur == 0 || FMCBlocks::IsReplaceable(Cur) || FMCBlocks::IsFluid(Cur)) C.SetRaw(X - X0, Y - Y0, Z, S);
			}
}

void FMCGenWriter::AddBlockEntity(int32 X, int32 Y, int32 Z, TSharedPtr<FMCBlockEntity> BE)
{
	if (!Inside(X, Y, Z) || !BE) return;
	BE->Pos = FMCBlockPos(X, Y, Z);
	C.BlockEntities.Add(FMCChunk::LocalIndex(X - X0, Y - Y0, Z), BE);
}

void FMCGenWriter::AddChest(int32 X, int32 Y, int32 Z, EMCFace Facing, FName LootTable, uint64 Seed, FName ChestBlock)
{
	if (!Inside(X, Y, Z)) return;
	const FMCBlock* B = FMCBlocks::Find(ChestBlock);
	if (!B) return;
	Set(X, Y, Z, B->State(MCMeta::FromFacing4(Facing)));
	const bool bBarrel = ChestBlock == TEXT("barrel");
	TSharedPtr<FMCContainerEntity> BE = MakeShared<FMCContainerEntity>(bBarrel ? EMCBlockEntityType::Barrel : EMCBlockEntityType::Chest, 27);
	BE->LootTable = LootTable;
	BE->LootSeed = Seed;
	AddBlockEntity(X, Y, Z, BE);
}

void FMCGenWriter::AddSpawner(int32 X, int32 Y, int32 Z, FName Mob)
{
	if (!Inside(X, Y, Z)) return;
	Set(X, Y, Z, MCGen::S(TEXT("spawner")));
	TSharedPtr<FMCSpawnerEntity> BE = MakeShared<FMCSpawnerEntity>();
	BE->Mob = Mob;
	AddBlockEntity(X, Y, Z, BE);
}

void FMCGenWriter::AddSpawn(FName Mob, double X, double Y, double Z, int32 Variant, FName Extra)
{
	if (!InsideXY(MC::FloorToInt(X), MC::FloorToInt(Y))) return;
	FMCPendingSpawn S;
	S.Mob = Mob;
	S.Pos = FVector(X, Y, Z);
	S.Variant = Variant;
	S.Extra = Extra;
	C.PendingSpawns.Add(S);
}

void FMCGenWriter::AddTick(int32 X, int32 Y, int32 Z)
{
	if (Inside(X, Y, Z)) C.PendingTicks.Add(FMCBlockPos(X, Y, Z));
}

void FMCGenWriter::SetBiomeColumn(int32 X, int32 Y, uint8 Biome)
{
	if (!InsideXY(X, Y)) return;
	C.FillBiomeColumn((X - X0) >> 2, (Y - Y0) >> 2, Biome);
}

// ---------------------------------------------------------------------------------------------------------------------
// Block shortcuts

namespace MCGen
{
	FMCState S(const TCHAR* Block)
	{
		const FMCBlock* B = FMCBlocks::Find(FName(Block));
		if (!B) { UE_LOG(LogOpus55, Warning, TEXT("WorldGen: unknown block %s"), Block); return 0; }
		return B->BaseState;
	}
	FMCState WithMeta(const TCHAR* Block, uint8 Meta)
	{
		const FMCBlock* B = FMCBlocks::Find(FName(Block));
		return B ? B->State(Meta) : 0;
	}
	FMCState Facing(FMCState Base, EMCFace F)
	{
		const FMCBlock& B = FMCBlocks::GetByState(Base);
		return B.State((FMCBlocks::MetaOf(Base) & ~3) | MCMeta::FromFacing4(F));
	}
	bool GetStructureStartChunk(const FMCStructureSet& Set, uint64 Seed, int32 RX, int32 RY, FMCChunkPos& Out)
	{
		FMCRandom R(MCHash::Hash3(Seed ^ (uint64)Set.Salt * 0x2545F4914F6CDD1Dull, RX, RY, (int32)Set.Salt));
		const int32 Range = FMath::Max(1, Set.Spacing - Set.Separation);
		int32 OX, OY;
		if (Set.bTriangular) { OX = (R.NextInt(Range) + R.NextInt(Range)) / 2; OY = (R.NextInt(Range) + R.NextInt(Range)) / 2; }
		else { OX = R.NextInt(Range); OY = R.NextInt(Range); }
		Out = FMCChunkPos(RX * Set.Spacing + OX, RY * Set.Spacing + OY);
		return true;
	}
}

static FMCGenBlocks GGenBlocks;
static bool GGenBlocksInit = false;

const FMCGenBlocks& FMCGenBlocks::Get()
{
	if (!GGenBlocksInit) { GGenBlocks.Init(); GGenBlocksInit = true; }
	return GGenBlocks;
}

void FMCGenBlocks::Init()
{
	using MCGen::S;
	using MCGen::WithMeta;
	Air = 0; Stone = S(TEXT("stone")); Deepslate = S(TEXT("deepslate")); Dirt = S(TEXT("dirt")); Grass = S(TEXT("grass_block"));
	GrassSnowy = WithMeta(TEXT("grass_block"), 1); Sand = S(TEXT("sand")); RedSand = S(TEXT("red_sand")); Sandstone = S(TEXT("sandstone"));
	RedSandstone = S(TEXT("red_sandstone")); Gravel = S(TEXT("gravel")); Clay = S(TEXT("clay")); Water = S(TEXT("water")); Lava = S(TEXT("lava"));
	Bedrock = S(TEXT("bedrock")); Snow = S(TEXT("snow")); SnowBlock = S(TEXT("snow_block")); Ice = S(TEXT("ice")); PackedIce = S(TEXT("packed_ice"));
	BlueIce = S(TEXT("blue_ice")); Podzol = S(TEXT("podzol")); CoarseDirt = S(TEXT("coarse_dirt")); Mycelium = S(TEXT("mycelium")); Mud = S(TEXT("mud"));
	Calcite = S(TEXT("calcite")); Tuff = S(TEXT("tuff")); Granite = S(TEXT("granite")); Diorite = S(TEXT("diorite")); Andesite = S(TEXT("andesite"));
	Terracotta = S(TEXT("terracotta")); Obsidian = S(TEXT("obsidian")); MossBlock = S(TEXT("moss_block")); PaleMoss = S(TEXT("pale_moss_block"));
	RootedDirt = S(TEXT("rooted_dirt")); Cobblestone = S(TEXT("cobblestone")); MossyCobble = S(TEXT("mossy_cobblestone"));
	StoneBricks = S(TEXT("stone_bricks")); MossyStoneBricks = S(TEXT("mossy_stone_bricks")); CrackedStoneBricks = S(TEXT("cracked_stone_bricks"));
	Planks = S(TEXT("oak_planks"));
	OakLog = S(TEXT("oak_log")); OakLeaves = S(TEXT("oak_leaves")); BirchLog = S(TEXT("birch_log")); BirchLeaves = S(TEXT("birch_leaves"));
	SpruceLog = S(TEXT("spruce_log")); SpruceLeaves = S(TEXT("spruce_leaves")); JungleLog = S(TEXT("jungle_log")); JungleLeaves = S(TEXT("jungle_leaves"));
	AcaciaLog = S(TEXT("acacia_log")); AcaciaLeaves = S(TEXT("acacia_leaves")); DarkOakLog = S(TEXT("dark_oak_log")); DarkOakLeaves = S(TEXT("dark_oak_leaves"));
	MangroveLog = S(TEXT("mangrove_log")); MangroveLeaves = S(TEXT("mangrove_leaves")); MangroveRoots = S(TEXT("mangrove_roots"));
	MuddyMangroveRoots = S(TEXT("muddy_mangrove_roots")); CherryLog = S(TEXT("cherry_log")); CherryLeaves = S(TEXT("cherry_leaves"));
	PaleOakLog = S(TEXT("pale_oak_log")); PaleOakLeaves = S(TEXT("pale_oak_leaves")); AzaleaLeaves = S(TEXT("azalea_leaves"));
	FloweringAzaleaLeaves = S(TEXT("flowering_azalea_leaves"));
	ShortGrass = S(TEXT("short_grass")); TallGrass = S(TEXT("tall_grass")); TallGrassTop = WithMeta(TEXT("tall_grass"), 1); Fern = S(TEXT("fern"));
	LargeFern = S(TEXT("large_fern")); LargeFernTop = WithMeta(TEXT("large_fern"), 1); DeadBush = S(TEXT("dead_bush")); Cactus = S(TEXT("cactus"));
	SugarCane = S(TEXT("sugar_cane")); Dandelion = S(TEXT("dandelion")); Poppy = S(TEXT("poppy")); Cornflower = S(TEXT("cornflower"));
	OxeyeDaisy = S(TEXT("oxeye_daisy")); AzureBluet = S(TEXT("azure_bluet")); Allium = S(TEXT("allium")); BlueOrchid = S(TEXT("blue_orchid"));
	RedTulip = S(TEXT("red_tulip")); OrangeTulip = S(TEXT("orange_tulip")); WhiteTulip = S(TEXT("white_tulip")); PinkTulip = S(TEXT("pink_tulip"));
	LilyOfValley = S(TEXT("lily_of_the_valley")); Sunflower = S(TEXT("sunflower")); SunflowerTop = WithMeta(TEXT("sunflower"), 1);
	Lilac = S(TEXT("lilac")); LilacTop = WithMeta(TEXT("lilac"), 1); RoseBush = S(TEXT("rose_bush")); RoseBushTop = WithMeta(TEXT("rose_bush"), 1);
	Peony = S(TEXT("peony")); PeonyTop = WithMeta(TEXT("peony"), 1); BrownMushroom = S(TEXT("brown_mushroom")); RedMushroom = S(TEXT("red_mushroom"));
	BrownMushroomBlock = S(TEXT("brown_mushroom_block")); RedMushroomBlock = S(TEXT("red_mushroom_block")); MushroomStem = S(TEXT("mushroom_stem"));
	Pumpkin = S(TEXT("pumpkin")); Melon = S(TEXT("melon")); SweetBerries = WithMeta(TEXT("sweet_berry_bush"), 3);
	Vine = S(TEXT("vine")); LilyPad = S(TEXT("lily_pad")); Seagrass = S(TEXT("seagrass")); TallSeagrass = S(TEXT("tall_seagrass"));
	TallSeagrassTop = WithMeta(TEXT("tall_seagrass"), 1); Kelp = S(TEXT("kelp")); KelpPlant = S(TEXT("kelp_plant"));
	Bamboo = WithMeta(TEXT("bamboo"), 4); BambooLeaves = WithMeta(TEXT("bamboo"), 6); PinkPetals = S(TEXT("pink_petals"));
	Wildflowers = S(TEXT("wildflowers")); LeafLitter = S(TEXT("leaf_litter")); Bush = S(TEXT("bush")); FireflyBush = S(TEXT("firefly_bush"));
	PaleHangingMoss = S(TEXT("pale_hanging_moss")); CreakingHeart = S(TEXT("creaking_heart")); OpenEyeblossom = S(TEXT("open_eyeblossom"));
	ClosedEyeblossom = S(TEXT("closed_eyeblossom")); GlowLichen = WithMeta(TEXT("glow_lichen"), 1); CaveVines = S(TEXT("cave_vines"));
	CaveVinesLit = WithMeta(TEXT("cave_vines"), 1); Dripleaf = S(TEXT("big_dripleaf")); SmallDripleaf = S(TEXT("small_dripleaf"));
	SporeBlossom = S(TEXT("spore_blossom")); Azalea = S(TEXT("azalea")); FloweringAzalea = S(TEXT("flowering_azalea"));
	HangingRoots = S(TEXT("hanging_roots")); MossCarpet = S(TEXT("moss_carpet")); PaleMossCarpet = S(TEXT("pale_moss_carpet"));
	DripstoneBlock = S(TEXT("dripstone_block")); PointedDripUp = WithMeta(TEXT("pointed_dripstone"), 2); PointedDripDown = WithMeta(TEXT("pointed_dripstone"), 3);
	Sculk = S(TEXT("sculk")); SculkVein = WithMeta(TEXT("sculk_vein"), 1); SculkSensor = S(TEXT("sculk_sensor")); SculkShrieker = S(TEXT("sculk_shrieker"));
	SculkCatalyst = S(TEXT("sculk_catalyst")); Amethyst = S(TEXT("amethyst_block")); BuddingAmethyst = S(TEXT("budding_amethyst"));
	AmethystCluster = WithMeta(TEXT("amethyst_cluster"), 1); SmoothBasalt = S(TEXT("smooth_basalt")); Magma = S(TEXT("magma_block"));
	Sulfur = S(TEXT("sulfur")); PotentSulfur = S(TEXT("potent_sulfur")); Cinnabar = S(TEXT("cinnabar"));
	SulfurSpikeUp = WithMeta(TEXT("sulfur_spike"), 2); SulfurSpikeDown = WithMeta(TEXT("sulfur_spike"), 3);
	CoalOre = S(TEXT("coal_ore")); IronOre = S(TEXT("iron_ore")); CopperOre = S(TEXT("copper_ore")); GoldOre = S(TEXT("gold_ore"));
	RedstoneOre = S(TEXT("redstone_ore")); LapisOre = S(TEXT("lapis_ore")); DiamondOre = S(TEXT("diamond_ore")); EmeraldOre = S(TEXT("emerald_ore"));
	DCoalOre = S(TEXT("deepslate_coal_ore")); DIronOre = S(TEXT("deepslate_iron_ore")); DCopperOre = S(TEXT("deepslate_copper_ore"));
	DGoldOre = S(TEXT("deepslate_gold_ore")); DRedstoneOre = S(TEXT("deepslate_redstone_ore")); DLapisOre = S(TEXT("deepslate_lapis_ore"));
	DDiamondOre = S(TEXT("deepslate_diamond_ore")); DEmeraldOre = S(TEXT("deepslate_emerald_ore"));
	RawIronBlock = S(TEXT("raw_iron_block")); RawCopperBlock = S(TEXT("raw_copper_block"));
	Netherrack = S(TEXT("netherrack")); SoulSand = S(TEXT("soul_sand")); SoulSoil = S(TEXT("soul_soil")); Basalt = S(TEXT("basalt"));
	Blackstone = S(TEXT("blackstone")); Glowstone = S(TEXT("glowstone")); CrimsonNylium = S(TEXT("crimson_nylium")); WarpedNylium = S(TEXT("warped_nylium"));
	CrimsonStem = S(TEXT("crimson_stem")); WarpedStem = S(TEXT("warped_stem")); NetherWartBlock = S(TEXT("nether_wart_block"));
	WarpedWartBlock = S(TEXT("warped_wart_block")); Shroomlight = S(TEXT("shroomlight")); CrimsonFungus = S(TEXT("crimson_fungus"));
	WarpedFungus = S(TEXT("warped_fungus")); CrimsonRoots = S(TEXT("crimson_roots")); WarpedRoots = S(TEXT("warped_roots"));
	NetherSprouts = S(TEXT("nether_sprouts")); WeepingVines = S(TEXT("weeping_vines")); TwistingVines = S(TEXT("twisting_vines"));
	NetherGoldOre = S(TEXT("nether_gold_ore")); NetherQuartzOre = S(TEXT("nether_quartz_ore")); AncientDebris = S(TEXT("ancient_debris"));
	BoneBlock = S(TEXT("bone_block")); NetherBricks = S(TEXT("nether_bricks")); NetherBrickFence = S(TEXT("nether_brick_fence"));
	RedNetherBricks = S(TEXT("red_nether_bricks")); GildedBlackstone = S(TEXT("gilded_blackstone"));
	PolishedBlackstoneBricks = S(TEXT("polished_blackstone_bricks")); CrackedPBB = S(TEXT("cracked_polished_blackstone_bricks"));
	PolishedBlackstone = S(TEXT("polished_blackstone")); ChiseledPB = S(TEXT("chiseled_polished_blackstone"));
	Fire = S(TEXT("fire")); SoulFire = S(TEXT("soul_fire")); EndStone = S(TEXT("end_stone")); EndStoneBricks = S(TEXT("end_stone_bricks"));
	Purpur = S(TEXT("purpur_block")); PurpurPillar = S(TEXT("purpur_pillar")); ChorusPlant = S(TEXT("chorus_plant")); ChorusFlower = S(TEXT("chorus_flower"));
	EndRod = WithMeta(TEXT("end_rod"), 1);
	const TCHAR* CoralNames[5] = { TEXT("tube"), TEXT("brain"), TEXT("bubble"), TEXT("fire"), TEXT("horn") };
	for (int32 i = 0; i < 5; ++i)
	{
		CoralBlocks[i] = S(*(FString(CoralNames[i]) + TEXT("_coral_block")));
		Corals[i] = S(*(FString(CoralNames[i]) + TEXT("_coral")));
	}
	SeaPickle = WithMeta(TEXT("sea_pickle"), 2); PrismarineBricks = S(TEXT("prismarine_bricks")); Prismarine = S(TEXT("prismarine"));
	DarkPrismarine = S(TEXT("dark_prismarine")); SeaLantern = S(TEXT("sea_lantern")); Glass = S(TEXT("glass"));
	Torch = S(TEXT("torch")); WallTorchN = WithMeta(TEXT("torch"), 1); WallTorchS = WithMeta(TEXT("torch"), 2); WallTorchW = WithMeta(TEXT("torch"), 3);
	WallTorchE = WithMeta(TEXT("torch"), 4); Lantern = S(TEXT("lantern")); HangingLantern = WithMeta(TEXT("lantern"), 1); Chain = S(TEXT("chain"));
	Rail = S(TEXT("rail"));
	Cocoa = 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// Structure cache (LRU-ish)

TSharedPtr<const FMCStructureStart> FMCStructureCache::Find(uint64 Key) const
{
	FScopeLock L(&Lock);
	const TSharedPtr<const FMCStructureStart>* S = Map.Find(Key);
	return S ? *S : nullptr;
}

void FMCStructureCache::Add(uint64 Key, TSharedPtr<const FMCStructureStart> S)
{
	FScopeLock L(&Lock);
	if (Map.Contains(Key)) return;
	Map.Add(Key, S);
	Order.Add(Key);
	if (Order.Num() > 4096)
	{
		for (int32 i = 0; i < 1024; ++i) Map.Remove(Order[i]);
		Order.RemoveAt(0, 1024);
	}
}

bool FMCWorldGenerator::LocateBiome(uint8 Biome, const FMCBlockPos& Origin, int32 Radius, FMCBlockPos& Out) const
{
	for (int32 R = 0; R <= Radius; R += 32)
	{
		const int32 Steps = FMath::Max(1, (R * 2) / 32);
		for (int32 i = 0; i < Steps * 4; ++i)
		{
			const float A = (float)i / (Steps * 4) * 2.f * PI;
			const int32 X = Origin.X + (int32)(FMath::Cos(A) * R), Y = Origin.Y + (int32)(FMath::Sin(A) * R);
			const int32 Z = GetSurfaceHeight(X, Y);
			if (GetBiomeAt(X, Y, Z) == Biome || GetBiomeAt(X, Y, 0) == Biome || GetBiomeAt(X, Y, -40) == Biome)
			{
				const bool bCave = FMCBiomes::Get(Biome).bCave;
				Out = FMCBlockPos(X, Y, bCave ? (Biome == (uint8)EMCBiome::DeepDark ? -40 : 10) : Z + 1);
				return true;
			}
		}
	}
	return false;
}
