using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public static partial class Blocks
    {
        // Frequently used blocks (resolved after registration)
        public static Block Air, Stone, Dirt, Grass, Sand, RedSand, Gravel, Water, Lava, Bedrock, Cobblestone, Deepslate, Netherrack,
            SoulSand, SoulSoil, EndStone, Obsidian, Glowstone, Snow, SnowBlock, Ice, Clay, Sandstone, RedSandstone, Terracotta, Basalt, Blackstone,
            OakLog, OakLeaves, Magma, Tuff, Calcite, Podzol, Mycelium, CoarseDirt, Mud, Moss, PackedIce, BlueIce, Fire, NetherPortal, Gravel2;

        public static readonly string[] Colors = { "white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray", "light_gray", "cyan", "purple", "blue", "brown", "green", "red", "black" };
        public static readonly string[] WoodTypes = { "oak", "spruce", "birch", "jungle", "acacia", "dark_oak", "mangrove", "cherry", "pale_oak", "bamboo", "crimson", "warped" };

        static Block Cube(string id, string tex, float hard, float res = -1, SoundType snd = SoundType.Stone, CreativeTab tab = CreativeTab.Building)
        {
            var b = Reg(id, new Block()).Hard(hard, res).Snd(snd).Tab(tab);
            b.T1(tex ?? id);
            return b;
        }
        static Block StoneCube(string id, float hard = 1.5f, float res = 6f, int tier = Tier.Wood, string tex = null, CreativeTab tab = CreativeTab.Building, SoundType snd = SoundType.Stone)
        {
            var b = Cube(id, tex ?? id, hard, res, snd, tab);
            b.Pick(tier);
            return b;
        }
        static SlabBlock Slab(string id, Block full) { var s = Reg(id, new SlabBlock(full)); return s; }
        static StairsBlock Stairs(string id, Block full) { var s = Reg(id, new StairsBlock(full)); return s; }
        static WallBlock Wall(string id, Block full) { var s = Reg(id, new WallBlock(full)); return s; }

        /// <summary>Registers slab/stairs(/wall) variants of a block with MC naming (strips plural "s" for bricks/tiles).</summary>
        static void Variants(Block full, string baseName, bool slab = true, bool stairs = true, bool wall = false)
        {
            if (stairs) Stairs(baseName + "_stairs", full);
            if (slab) Slab(baseName + "_slab", full);
            if (wall) Wall(baseName + "_wall", full);
        }

        static void RegisterAll()
        {
            Air = Reg("air", new AirBlock());
            // ------------------------------------------------------------------ stone family
            Stone = StoneCube("stone").Drops("cobblestone"); Variants(Stone, "stone", true, true, false);
            Cobblestone = StoneCube("cobblestone", 2f, 6f); Variants(Cobblestone, "cobblestone", true, true, true);
            var mossyCobble = StoneCube("mossy_cobblestone", 2f, 6f); Variants(mossyCobble, "mossy_cobblestone", true, true, true);
            var smoothStone = StoneCube("smooth_stone", 2f, 6f); Slab("smooth_stone_slab", smoothStone);
            var stoneBricks = StoneCube("stone_bricks"); Variants(stoneBricks, "stone_brick", true, true, true);
            var mossyStoneBricks = StoneCube("mossy_stone_bricks"); Variants(mossyStoneBricks, "mossy_stone_brick", true, true, true);
            StoneCube("cracked_stone_bricks"); StoneCube("chiseled_stone_bricks");
            foreach (var n in new[] { "granite", "diorite", "andesite" })
            {
                var b = StoneCube(n); Variants(b, n, true, true, true);
                var p = StoneCube("polished_" + n); Variants(p, "polished_" + n, true, true, false);
            }
            Deepslate = Reg("deepslate", new PillarBlock("deepslate_top", "deepslate")).Hard(3f, 6f).Pick().Snd(SoundType.Deepslate).Drops("cobbled_deepslate");
            var cobDeep = StoneCube("cobbled_deepslate", 3.5f, 6f, snd: SoundType.Deepslate); Variants(cobDeep, "cobbled_deepslate", true, true, true);
            var polDeep = StoneCube("polished_deepslate", 3.5f, 6f, snd: SoundType.Deepslate); Variants(polDeep, "polished_deepslate", true, true, true);
            var deepBricks = StoneCube("deepslate_bricks", 3.5f, 6f, snd: SoundType.Deepslate); Variants(deepBricks, "deepslate_brick", true, true, true);
            var deepTiles = StoneCube("deepslate_tiles", 3.5f, 6f, snd: SoundType.Deepslate); Variants(deepTiles, "deepslate_tile", true, true, true);
            StoneCube("cracked_deepslate_bricks", 3.5f, 6f, snd: SoundType.Deepslate); StoneCube("cracked_deepslate_tiles", 3.5f, 6f, snd: SoundType.Deepslate);
            StoneCube("chiseled_deepslate", 3.5f, 6f, snd: SoundType.Deepslate);
            Reg("reinforced_deepslate", new Block()).Hard(55f, 1200f).Snd(SoundType.Deepslate).T3("reinforced_deepslate_top", "reinforced_deepslate_bottom", "reinforced_deepslate_side").Drops("air");
            Tuff = StoneCube("tuff", snd: SoundType.Tuff); Variants(Tuff, "tuff", true, true, true);
            var polTuff = StoneCube("polished_tuff", snd: SoundType.Tuff); Variants(polTuff, "polished_tuff", true, true, true);
            var tuffBricks = StoneCube("tuff_bricks", snd: SoundType.Tuff); Variants(tuffBricks, "tuff_brick", true, true, true);
            StoneCube("chiseled_tuff", snd: SoundType.Tuff); StoneCube("chiseled_tuff_bricks", snd: SoundType.Tuff);
            Calcite = StoneCube("calcite", 0.75f, 0.75f, snd: SoundType.Calcite).Tab(CreativeTab.Natural);
            StoneCube("dripstone_block", 1.5f, 1f).Tab(CreativeTab.Natural);
            var bricks = StoneCube("bricks", 2f, 6f); Variants(bricks, "brick", true, true, true);
            Mud = Cube("mud", "mud", 0.5f, 0.5f, SoundType.Mud, CreativeTab.Natural).Shovel();
            Cube("packed_mud", "packed_mud", 1f, 3f, SoundType.Mud);
            var mudBricks = StoneCube("mud_bricks", 1.5f, 3f, snd: SoundType.Mud); Variants(mudBricks, "mud_brick", true, true, true);
            // sandstone
            Sandstone = Reg("sandstone", new Block()).Hard(0.8f).Pick().T3("sandstone_top", "sandstone_bottom", "sandstone"); Variants(Sandstone, "sandstone", true, true, true);
            Reg("chiseled_sandstone", new Block()).Hard(0.8f).Pick().T3("sandstone_top", "sandstone_top", "chiseled_sandstone");
            Reg("cut_sandstone", new Block()).Hard(0.8f).Pick().T3("sandstone_top", "sandstone_top", "cut_sandstone"); Slab("cut_sandstone_slab", Get("cut_sandstone"));
            var smoothSand = Reg("smooth_sandstone", new Block()).Hard(2f, 6f).Pick().T1("sandstone_top"); Variants(smoothSand, "smooth_sandstone", true, true, false);
            RedSandstone = Reg("red_sandstone", new Block()).Hard(0.8f).Pick().T3("red_sandstone_top", "red_sandstone_bottom", "red_sandstone"); Variants(RedSandstone, "red_sandstone", true, true, true);
            Reg("chiseled_red_sandstone", new Block()).Hard(0.8f).Pick().T3("red_sandstone_top", "red_sandstone_top", "chiseled_red_sandstone");
            Reg("cut_red_sandstone", new Block()).Hard(0.8f).Pick().T3("red_sandstone_top", "red_sandstone_top", "cut_red_sandstone"); Slab("cut_red_sandstone_slab", Get("cut_red_sandstone"));
            var smoothRed = Reg("smooth_red_sandstone", new Block()).Hard(2f, 6f).Pick().T1("red_sandstone_top"); Variants(smoothRed, "smooth_red_sandstone", true, true, false);
            // prismarine
            var pris = StoneCube("prismarine"); Variants(pris, "prismarine", true, true, true);
            var prisB = StoneCube("prismarine_bricks"); Variants(prisB, "prismarine_brick", true, true, false);
            var darkPris = StoneCube("dark_prismarine"); Variants(darkPris, "dark_prismarine", true, true, false);
            Cube("sea_lantern", "sea_lantern", 0.3f, 0.3f, SoundType.Glass, CreativeTab.Functional).Light(15).Drops("prismarine_crystals");
            Cube("sponge", "sponge", 0.6f, 0.6f, SoundType.Grass, CreativeTab.Functional); Cube("wet_sponge", "wet_sponge", 0.6f, 0.6f, SoundType.Grass, CreativeTab.Functional);

            // ------------------------------------------------------------------ natural terrain
            Grass = Reg("grass_block", new GrassBlock("grass_block_top", "grass_block_side", "dirt", true));
            Dirt = Cube("dirt", "dirt", 0.5f, 0.5f, SoundType.Gravel, CreativeTab.Natural).Shovel();
            CoarseDirt = Cube("coarse_dirt", "coarse_dirt", 0.5f, 0.5f, SoundType.Gravel, CreativeTab.Natural).Shovel();
            Cube("rooted_dirt", "rooted_dirt", 0.5f, 0.5f, SoundType.Gravel, CreativeTab.Natural).Shovel();
            Podzol = Reg("podzol", new GrassBlock("podzol_top", "podzol_side", "dirt", false)); ((GrassBlock)Podzol).randomTicks = false;
            Mycelium = Reg("mycelium", new GrassBlock("mycelium_top", "mycelium_side", "dirt", false));
            var path = Reg("dirt_path", new ShortBlock(15)).Hard(0.65f).Shovel().Snd(SoundType.Grass).T3("dirt_path_top", "dirt", "dirt_path_side").Drops("dirt").Tab(CreativeTab.Natural);
            Reg("farmland", new FarmlandBlock());
            Moss = Cube("moss_block", "moss_block", 0.1f, 0.1f, SoundType.Moss, CreativeTab.Natural).Hoe();
            Reg("moss_carpet", new CarpetBlock("moss_block")).Snd(SoundType.Moss).Tab(CreativeTab.Natural);
            Cube("pale_moss_block", "pale_moss_block", 0.1f, 0.1f, SoundType.Moss, CreativeTab.Natural).Hoe();
            Reg("pale_moss_carpet", new CarpetBlock("pale_moss_block")).Snd(SoundType.Moss).Tab(CreativeTab.Natural);
            Sand = Reg("sand", new GravityBlock()).Hard(0.5f).Shovel().Snd(SoundType.Sand).T1("sand").Tab(CreativeTab.Natural);
            RedSand = Reg("red_sand", new GravityBlock()).Hard(0.5f).Shovel().Snd(SoundType.Sand).T1("red_sand").Tab(CreativeTab.Natural);
            Gravel = Reg("gravel", new GravityBlock()).Hard(0.6f).Shovel().Snd(SoundType.Gravel).T1("gravel").Tab(CreativeTab.Natural);
            Clay = Cube("clay", "clay", 0.6f, 0.6f, SoundType.Gravel, CreativeTab.Natural).Shovel().Drops("clay_ball");
            Snow = Reg("snow", new SnowLayerBlock());
            SnowBlock = Cube("snow_block", "snow", 0.2f, 0.2f, SoundType.Snow, CreativeTab.Natural).Tool(ToolType.Shovel, 0, true).Drops("snowball");
            Ice = Reg("ice", new IceBlock()).T1("ice").Pick(0).Tab(CreativeTab.Natural); Ice.requiresTool = false;
            PackedIce = Cube("packed_ice", "packed_ice", 0.5f, 0.5f, SoundType.Glass, CreativeTab.Natural).Slip(0.98f).Drops("air");
            BlueIce = Cube("blue_ice", "blue_ice", 2.8f, 2.8f, SoundType.Glass, CreativeTab.Natural).Slip(0.989f).Drops("air");
            Bedrock = Cube("bedrock", "bedrock", -1f, 3600000f, SoundType.Stone, CreativeTab.Natural);
            Obsidian = StoneCube("obsidian", 50f, 1200f, Tier.Diamond);
            StoneCube("crying_obsidian", 50f, 1200f, Tier.Diamond).Light(10);
            Reg("cobweb", new CobwebBlock());
            Magma = Reg("magma_block", new MagmaBlock());
            Cube("bone_block", "bone_block_side", 2f, 2f, SoundType.Bone, CreativeTab.Natural).Pick();
            Get("bone_block").T2("bone_block_top", "bone_block_side");

            // ------------------------------------------------------------------ ores
            OreFamily("coal", "coal", 0, 2, Tier.Wood, 1, 1);
            OreFamily("iron", "raw_iron", 0, 0, Tier.Stone, 1, 1);
            OreFamily("copper", "raw_copper", 0, 0, Tier.Stone, 2, 5);
            OreFamily("gold", "raw_gold", 0, 0, Tier.Iron, 1, 1);
            OreFamily("lapis", "lapis_lazuli", 2, 5, Tier.Stone, 4, 9);
            OreFamily("diamond", "diamond", 3, 7, Tier.Iron, 1, 1);
            OreFamily("emerald", "emerald", 3, 7, Tier.Iron, 1, 1);
            Reg("redstone_ore", new RedstoneOreBlock()).Hard(3f, 3f).Pick(Tier.Iron).T1("redstone_ore");
            Reg("deepslate_redstone_ore", new RedstoneOreBlock()).Hard(4.5f, 3f).Pick(Tier.Iron).Snd(SoundType.Deepslate).T1("deepslate_redstone_ore");
            Reg("nether_gold_ore", new OreBlock("gold_nugget", 0, 1, 2, 6)).Hard(3f, 3f).Pick().Snd(SoundType.Netherrack).T1("nether_gold_ore");
            Reg("nether_quartz_ore", new OreBlock("quartz", 2, 5)).Hard(3f, 3f).Pick().Snd(SoundType.Netherrack).T1("nether_quartz_ore");
            Reg("ancient_debris", new Block()).Hard(30f, 1200f).Pick(Tier.Diamond).Snd(SoundType.Metal).T2("ancient_debris_top", "ancient_debris_side").Tab(CreativeTab.Natural);
            Cube("raw_iron_block", "raw_iron_block", 5f, 6f, SoundType.Stone, CreativeTab.Natural).Pick(Tier.Stone);
            Cube("raw_copper_block", "raw_copper_block", 5f, 6f, SoundType.Stone, CreativeTab.Natural).Pick(Tier.Stone);
            Cube("raw_gold_block", "raw_gold_block", 5f, 6f, SoundType.Stone, CreativeTab.Natural).Pick(Tier.Iron);
            StoneCube("coal_block", 5f, 6f).Flam(5, 5);
            StoneCube("iron_block", 5f, 6f, Tier.Stone, snd: SoundType.Metal);
            StoneCube("gold_block", 3f, 6f, Tier.Iron, snd: SoundType.Metal);
            StoneCube("diamond_block", 5f, 6f, Tier.Iron, snd: SoundType.Metal);
            StoneCube("emerald_block", 5f, 6f, Tier.Iron, snd: SoundType.Metal);
            StoneCube("lapis_block", 3f, 3f, Tier.Stone);
            Reg("redstone_block", new RedstoneBlock()).Hard(5f, 6f).Pick().Snd(SoundType.Metal).T1("redstone_block").Tab(CreativeTab.Redstone);
            StoneCube("netherite_block", 50f, 1200f, Tier.Diamond, snd: SoundType.Metal);
            StoneCube("amethyst_block", 1.5f, 1.5f, snd: SoundType.Amethyst).Tab(CreativeTab.Natural);
            StoneCube("budding_amethyst", 1.5f, 1.5f, snd: SoundType.Amethyst).Tab(CreativeTab.Natural).Drops("air");
            var quartz = Reg("quartz_block", new Block()).Hard(0.8f).Pick().T3("quartz_block_top", "quartz_block_bottom", "quartz_block_side"); Variants(quartz, "quartz", true, true, false);
            Reg("chiseled_quartz_block", new Block()).Hard(0.8f).Pick().T2("chiseled_quartz_block_top", "chiseled_quartz_block");
            Reg("quartz_pillar", new PillarBlock("quartz_pillar_top", "quartz_pillar")).Hard(0.8f).Pick();
            StoneCube("quartz_bricks", 0.8f, 0.8f);
            var smoothQuartz = Reg("smooth_quartz", new Block()).Hard(2f, 6f).Pick().T1("quartz_block_bottom"); Variants(smoothQuartz, "smooth_quartz", true, true, false);

            // copper family (oxidation stages)
            string[] ox = { "", "exposed_", "weathered_", "oxidized_" };
            for (int i = 0; i < 4; i++)
            {
                string pre = ox[i];
                string cbName = i == 0 ? "copper_block" : pre + "copper";
                Reg(cbName, new CopperBlock(i)).Hard(3f, 6f).Pick(Tier.Stone).Snd(SoundType.Copper).T1(cbName);
                var cut = Reg(pre + "cut_copper", new CopperBlock(i)).Hard(3f, 6f).Pick(Tier.Stone).Snd(SoundType.Copper); cut.T1(pre + "cut_copper");
                Variants(cut, pre + "cut_copper", true, true, false);
                Reg(pre + "chiseled_copper", new CopperBlock(i)).Hard(3f, 6f).Pick(Tier.Stone).Snd(SoundType.Copper).T1(pre + "chiseled_copper");
                var grate = Reg(pre + "copper_grate", new GlassBlock(RenderLayer.Cutout)).Hard(3f, 6f).Pick(Tier.Stone).Snd(SoundType.Copper).T1(pre + "copper_grate");
                grate.selfCullSameType = false; grate.creativeTab = CreativeTab.Building;
                Reg(pre + "copper_bulb", new CopperBulbBlock(i)).Hard(3f, 6f).Pick(Tier.Stone).Snd(SoundType.Copper).Tab(CreativeTab.Redstone);
                Reg(pre + "copper_door", new DoorBlock(pre + "copper_door", false)).Hard(3f, 6f).Pick(Tier.Stone).Snd(SoundType.Copper).Tab(CreativeTab.Building);
                Reg(pre + "copper_trapdoor", new TrapdoorBlock(pre + "copper_trapdoor", false)).Hard(3f, 6f).Pick(Tier.Stone).Snd(SoundType.Copper);
                Reg(pre + "copper_bars", new PaneBlock(pre + "copper_bars", pre + "copper_bars", RenderLayer.Cutout, true)).Tab(CreativeTab.Building);
                Reg(pre + "copper_chain", new ChainBlock(pre + "copper_chain"));
                Reg(pre + "copper_lantern", new LanternBlock(pre + "copper_lantern", 15));
            }

            // ------------------------------------------------------------------ wood families
            foreach (var w in WoodTypes) WoodFamily(w);
            OakLog = Get("oak_log"); OakLeaves = Get("oak_leaves");
            foreach (var n in new[] { "azalea", "flowering_azalea" })
            {
                var l = Reg(n + "_leaves", new LeavesBlock(n + "_leaves", TintType.None)); l.saplingId = n;
            }
            Reg("azalea", new PlantBlock("azalea_side")).Snd(SoundType.Grass).randomOffset = false;
            Reg("flowering_azalea", new PlantBlock("flowering_azalea_side")).Snd(SoundType.Grass).randomOffset = false;
            Reg("mangrove_roots", new LeavesBlock("mangrove_roots", TintType.None)).Hard(0.7f).Axe().Snd(SoundType.Wood).Tab(CreativeTab.Natural);
            Reg("muddy_mangrove_roots", new PillarBlock("muddy_mangrove_roots_top", "muddy_mangrove_roots_side")).Hard(0.7f).Shovel().Snd(SoundType.Mud).Tab(CreativeTab.Natural);

            // ------------------------------------------------------------------ plants
            var shortGrass = Reg("short_grass", new PlantBlock("short_grass")).Tint(TintType.Grass).Replaceable(); shortGrass.heightPx = 13;
            Reg("fern", new PlantBlock("fern")).Tint(TintType.Grass).Replaceable();
            Reg("dead_bush", new PlantBlock("dead_bush", SoilKind.Sand)).Replaceable();
            Reg("bush", new PlantBlock("bush")).Tint(TintType.Grass).Replaceable();
            Reg("tall_grass", new TallPlantBlock("tall_grass_bottom", "tall_grass_top")).Tint(TintType.Grass).Replaceable();
            Reg("large_fern", new TallPlantBlock("large_fern_bottom", "large_fern_top")).Tint(TintType.Grass).Replaceable();
            foreach (var f in new[] { "dandelion", "poppy", "blue_orchid", "allium", "azure_bluet", "red_tulip", "orange_tulip", "white_tulip", "pink_tulip",
                "oxeye_daisy", "cornflower", "lily_of_the_valley", "wither_rose", "torchflower", "open_eyeblossom", "closed_eyeblossom" })
            {
                var p = Reg(f, new PlantBlock(f)); p.heightPx = 10; p.widthPx = 6; p.randomOffset = true; p.size = 0.9f;
                if (f == "open_eyeblossom") p.lightEmission = 3;
            }
            Reg("pink_petals", new CarpetBlock("pink_petals", 3)).Snd(SoundType.Grass).Tab(CreativeTab.Natural).Transparent(RenderLayer.Cutout);
            Get("pink_petals").solid = false;
            Reg("wildflowers", new CarpetBlock("wildflowers", 3)).Snd(SoundType.Grass).Tab(CreativeTab.Natural).Transparent(RenderLayer.Cutout);
            Get("wildflowers").solid = false;
            Reg("leaf_litter", new CarpetBlock("leaf_litter", 1)).Snd(SoundType.Grass).Tab(CreativeTab.Natural).Transparent(RenderLayer.Cutout);
            Get("leaf_litter").solid = false;
            foreach (var f in new[] { "sunflower", "lilac", "rose_bush", "peony" })
                Reg(f, new TallPlantBlock(f + "_bottom", f + "_top"));
            var brownMush = Reg("brown_mushroom", new PlantBlock("brown_mushroom", SoilKind.Mushroom)); brownMush.heightPx = 6; brownMush.lightEmission = 1; brownMush.randomOffset = false; brownMush.size = 0.8f;
            var redMush = Reg("red_mushroom", new PlantBlock("red_mushroom", SoilKind.Mushroom)); redMush.heightPx = 6; redMush.randomOffset = false; redMush.size = 0.8f;
            Cube("brown_mushroom_block", "brown_mushroom_block", 0.2f, 0.2f, SoundType.Wood, CreativeTab.Natural).Axe().Drops("brown_mushroom");
            Cube("red_mushroom_block", "red_mushroom_block", 0.2f, 0.2f, SoundType.Wood, CreativeTab.Natural).Axe().Drops("red_mushroom");
            Cube("mushroom_stem", "mushroom_stem", 0.2f, 0.2f, SoundType.Wood, CreativeTab.Natural).Axe().Drops("air");
            Reg("sugar_cane", new SugarCaneBlock());
            Reg("cactus", new CactusBlock());
            Reg("vine", new VineBlock());
            Reg("glow_lichen", new VineBlock("glow_lichen", TintType.None)).Light(7);
            var pumpkin = Reg("pumpkin", new Block()).Hard(1f).Axe().Snd(SoundType.Wood).T2("pumpkin_top", "pumpkin_side").Tab(CreativeTab.Natural);
            Reg("carved_pumpkin", new HorizontalBlock("pumpkin_top", "pumpkin_side", "carved_pumpkin")).Hard(1f).Axe().Snd(SoundType.Wood).Tab(CreativeTab.Natural);
            Reg("jack_o_lantern", new HorizontalBlock("pumpkin_top", "pumpkin_side", "jack_o_lantern")).Hard(1f).Axe().Snd(SoundType.Wood).Light(15).Tab(CreativeTab.Functional);
            Reg("melon", new Block()).Hard(1f).Axe().Snd(SoundType.Wood).T2("melon_top", "melon_side").Tab(CreativeTab.Natural).Drops("melon_slice");
            Reg("hay_block", new PillarBlock("hay_block_top", "hay_block_side")).Hard(0.5f).Hoe().Snd(SoundType.Grass).Tab(CreativeTab.Building).Flam(60, 20);
            // crops
            Reg("wheat", new CropBlock(7, "wheat", new[] { 0, 1, 2, 3, 4, 5, 6, 7 }, "wheat_seeds", "wheat"));
            Reg("carrots", new CropBlock(7, "carrots", new[] { 0, 0, 1, 1, 2, 2, 2, 3 }, "carrot", "carrot"));
            Reg("potatoes", new CropBlock(7, "potatoes", new[] { 0, 0, 1, 1, 2, 2, 2, 3 }, "potato", "potato"));
            Reg("beetroots", new CropBlock(3, "beetroots", new[] { 0, 1, 2, 3 }, "beetroot_seeds", "beetroot"));

            // ------------------------------------------------------------------ colored blocks
            for (int i = 0; i < 16; i++)
            {
                string c = Colors[i];
                Cube(c + "_wool", c + "_wool", 0.8f, 0.8f, SoundType.Wool, CreativeTab.Colored).Flam(30, 60);
                Reg(c + "_carpet", new CarpetBlock(c + "_wool"));
                StoneCube(c + "_concrete", 1.8f, 1.8f).Tab(CreativeTab.Colored);
                Reg(c + "_concrete_powder", new ConcretePowderBlock(c + "_concrete")).Hard(0.5f).Shovel().Snd(SoundType.Sand).T1(c + "_concrete_powder").Tab(CreativeTab.Colored);
                StoneCube(c + "_terracotta", 1.25f, 4.2f).Tab(CreativeTab.Colored);
                Reg(c + "_glazed_terracotta", new HorizontalBlock(c + "_glazed_terracotta", c + "_glazed_terracotta", c + "_glazed_terracotta", true)).Hard(1.4f).Pick().Tab(CreativeTab.Colored);
                Reg(c + "_stained_glass", new GlassBlock(RenderLayer.Translucent)).T1(c + "_stained_glass").Tab(CreativeTab.Colored);
                Reg(c + "_stained_glass_pane", new PaneBlock(c + "_stained_glass", c + "_stained_glass_pane_top", RenderLayer.Translucent));
            }
            Terracotta = StoneCube("terracotta", 1.25f, 4.2f).Tab(CreativeTab.Colored);
            Reg("glass", new GlassBlock()).T1("glass").Tab(CreativeTab.Colored);
            Reg("tinted_glass", new GlassBlock(RenderLayer.Translucent)).T1("tinted_glass").Tab(CreativeTab.Colored).Opacity(15);
            Reg("glass_pane", new PaneBlock("glass", "glass_pane_top", RenderLayer.Cutout));
            Reg("iron_bars", new PaneBlock("iron_bars", "iron_bars", RenderLayer.Cutout, true)).Tab(CreativeTab.Building);

            // ------------------------------------------------------------------ nether
            Netherrack = StoneCube("netherrack", 0.4f, 0.4f, snd: SoundType.Netherrack).Tab(CreativeTab.Natural);
            SoulSand = Reg("soul_sand", new SoulSandBlock());
            SoulSoil = Cube("soul_soil", "soul_soil", 0.5f, 0.5f, SoundType.SoulSand, CreativeTab.Natural).Shovel();
            Basalt = Reg("basalt", new PillarBlock("basalt_top", "basalt_side")).Hard(1.25f, 4.2f).Pick().Snd(SoundType.Basalt).Tab(CreativeTab.Natural);
            Reg("polished_basalt", new PillarBlock("polished_basalt_top", "polished_basalt_side")).Hard(1.25f, 4.2f).Pick().Snd(SoundType.Basalt);
            StoneCube("smooth_basalt", 1.25f, 4.2f, snd: SoundType.Basalt);
            Blackstone = Reg("blackstone", new Block()).Hard(1.5f, 6f).Pick().T2("blackstone_top", "blackstone"); Variants(Blackstone, "blackstone", true, true, true);
            var polBlack = StoneCube("polished_blackstone", 2f, 6f); Variants(polBlack, "polished_blackstone", true, true, true);
            var polBlackBricks = StoneCube("polished_blackstone_bricks", 1.5f, 6f); Variants(polBlackBricks, "polished_blackstone_brick", true, true, true);
            StoneCube("cracked_polished_blackstone_bricks", 1.5f, 6f); StoneCube("chiseled_polished_blackstone", 1.5f, 6f);
            StoneCube("gilded_blackstone", 1.5f, 6f).Tab(CreativeTab.Natural);
            Glowstone = Cube("glowstone", "glowstone", 0.3f, 0.3f, SoundType.Glass, CreativeTab.Natural).Light(15).Drops("glowstone_dust");
            var netherBricks = StoneCube("nether_bricks", 2f, 6f, snd: SoundType.NetherBricks); Variants(netherBricks, "nether_brick", true, true, true);
            StoneCube("cracked_nether_bricks", 2f, 6f, snd: SoundType.NetherBricks); StoneCube("chiseled_nether_bricks", 2f, 6f, snd: SoundType.NetherBricks);
            var redNB = StoneCube("red_nether_bricks", 2f, 6f, snd: SoundType.NetherBricks); Variants(redNB, "red_nether_brick", true, true, true);
            Reg("nether_brick_fence", new FenceBlock(netherBricks, "nether"));
            Cube("nether_wart_block", "nether_wart_block", 1f, 1f, SoundType.Grass, CreativeTab.Natural).Hoe();
            Cube("warped_wart_block", "warped_wart_block", 1f, 1f, SoundType.Grass, CreativeTab.Natural).Hoe();
            Cube("shroomlight", "shroomlight", 1f, 1f, SoundType.Grass, CreativeTab.Natural).Hoe().Light(15);
            Reg("crimson_nylium", new GrassBlock("crimson_nylium", "crimson_nylium_side", "netherrack", false)).Hard(0.4f).Pick().Snd(SoundType.Nylium).spreadsFrom = "netherrack";
            Get("crimson_nylium").dropItemId = "netherrack";
            Reg("warped_nylium", new GrassBlock("warped_nylium", "warped_nylium_side", "netherrack", false)).Hard(0.4f).Pick().Snd(SoundType.Nylium).spreadsFrom = "netherrack";
            Get("warped_nylium").dropItemId = "netherrack";
            Reg("crimson_roots", new PlantBlock("crimson_roots", SoilKind.Nether)).Replaceable().Snd(SoundType.Fungus);
            Reg("warped_roots", new PlantBlock("warped_roots", SoilKind.Nether)).Replaceable().Snd(SoundType.Fungus);
            Reg("nether_sprouts", new PlantBlock("nether_sprouts", SoilKind.Nether)).Replaceable().Snd(SoundType.Fungus);
            var cf = Reg("crimson_fungus", new PlantBlock("crimson_fungus", SoilKind.Nether)).Snd(SoundType.Fungus); cf.randomOffset = false; cf.heightPx = 9;
            var wf = Reg("warped_fungus", new PlantBlock("warped_fungus", SoilKind.Nether)).Snd(SoundType.Fungus); wf.randomOffset = false; wf.heightPx = 9;
            Reg("weeping_vines", new PlantBlock("weeping_vines", SoilKind.Any)).Climb();
            Reg("twisting_vines", new PlantBlock("twisting_vines", SoilKind.Any)).Climb();

            // ------------------------------------------------------------------ end
            EndStone = StoneCube("end_stone", 3f, 9f).Tab(CreativeTab.Natural);
            var esb = StoneCube("end_stone_bricks", 3f, 9f); Variants(esb, "end_stone_brick", true, true, true);
            var purpur = StoneCube("purpur_block", 1.5f, 6f); Variants(purpur, "purpur", true, true, false);
            Reg("purpur_pillar", new PillarBlock("purpur_pillar_top", "purpur_pillar")).Hard(1.5f, 6f).Pick();

            // ------------------------------------------------------------------ light sources & utility
            Reg("torch", new TorchBlock("torch", 14));
            Reg("soul_torch", new TorchBlock("soul_torch", 10)).flameParticle = "soul_flame";
            Reg("copper_torch", new TorchBlock("copper_torch", 14)).flameParticle = "copper_flame";
            Reg("redstone_torch", new RedstoneTorchBlock());
            Reg("lantern", new LanternBlock("lantern", 15));
            Reg("soul_lantern", new LanternBlock("soul_lantern", 10));
            Reg("iron_chain", new ChainBlock("iron_chain"));
            Reg("ladder", new LadderBlock());
            Reg("iron_door", new DoorBlock("iron_door", true)).Tab(CreativeTab.Redstone);
            Reg("iron_trapdoor", new TrapdoorBlock("iron_trapdoor", true)).Tab(CreativeTab.Redstone);
            Fire = Reg("fire", new FireBlock(false));
            Reg("soul_fire", new FireBlock(true));
            NetherPortal = Reg("nether_portal", new PortalBlock());

            // ------------------------------------------------------------------ fluids
            var water = Reg("water", new FluidBlock(0)); water.stillTex = Tex.Id("water_still"); water.flowTex = Tex.Id("water_flow"); Water = water;
            var lava = Reg("lava", new FluidBlock(1)); lava.stillTex = Tex.Id("lava_still"); lava.flowTex = Tex.Id("lava_flow"); Lava = lava;
            water.SetAllTex(water.stillTex); lava.SetAllTex(lava.stillTex);

            // ------------------------------------------------------------------ 26.2 sulfur caves
            var sulfur = StoneCube("sulfur", 1.5f, 3f).Tab(CreativeTab.Natural); Variants(sulfur, "sulfur", true, true, true);
            Reg("potent_sulfur", new PotentSulfurBlock()).Hard(1.5f, 3f).Pick().T1("potent_sulfur").Tab(CreativeTab.Natural).Light(4);
            var cinnabar = StoneCube("cinnabar", 1.5f, 3f).Tab(CreativeTab.Natural); Variants(cinnabar, "cinnabar", true, true, true);
            var polSulfur = StoneCube("polished_sulfur", 1.5f, 6f); Variants(polSulfur, "polished_sulfur", true, true, true);
            var sulfurBricks = StoneCube("sulfur_bricks", 1.5f, 6f); Variants(sulfurBricks, "sulfur_brick", true, true, true);
            var polCinnabar = StoneCube("polished_cinnabar", 1.5f, 6f); Variants(polCinnabar, "polished_cinnabar", true, true, true);
            var cinnabarBricks = StoneCube("cinnabar_bricks", 1.5f, 6f); Variants(cinnabarBricks, "cinnabar_brick", true, true, true);
            StoneCube("chiseled_sulfur", 1.5f, 6f); StoneCube("chiseled_cinnabar", 1.5f, 6f);
            var spike = Reg("sulfur_spike", new PlantBlock("sulfur_spike", SoilKind.Any)).Hard(1f).Pick(); spike.randomOffset = false; spike.heightPx = 14; spike.solid = true;
            spike.Tab(CreativeTab.Natural);

            RegisterMore();
        }

        static void OreFamily(string name, string drop, int xpMin, int xpMax, int tier, int min, int max)
        {
            Reg(name + "_ore", new OreBlock(drop, xpMin, xpMax, min, max)).Hard(3f, 3f).Pick(tier).T1(name + "_ore");
            Reg("deepslate_" + name + "_ore", new OreBlock(drop, xpMin, xpMax, min, max)).Hard(4.5f, 3f).Pick(tier).Snd(SoundType.Deepslate).T1("deepslate_" + name + "_ore");
        }

        static void WoodFamily(string w)
        {
            bool nether = w == "crimson" || w == "warped";
            bool bamboo = w == "bamboo";
            SoundType snd = nether ? SoundType.Wood : (w == "cherry" ? SoundType.Cherry : (bamboo ? SoundType.Bamboo : SoundType.Wood));
            string logName = nether ? w + "_stem" : (bamboo ? "bamboo_block" : w + "_log");
            string woodName = nether ? w + "_hyphae" : (bamboo ? null : w + "_wood");
            int fl = nether ? 0 : 5, fs = nether ? 0 : 5;
            var log = Reg(logName, new PillarBlock(logName + "_top", logName)).Hard(2f).Axe().Snd(snd).Flam(fl, fs).Tab(CreativeTab.Building);
            var strippedLog = Reg("stripped_" + logName, new PillarBlock("stripped_" + logName + "_top", "stripped_" + logName)).Hard(2f).Axe().Snd(snd).Flam(fl, fs);
            log.strippedId = strippedLog.id;
            if (woodName != null)
            {
                var wood = Reg(woodName, new PillarBlock(logName, logName)).Hard(2f).Axe().Snd(snd).Flam(fl, fs);
                var sw = Reg("stripped_" + woodName, new PillarBlock("stripped_" + logName, "stripped_" + logName)).Hard(2f).Axe().Snd(snd).Flam(fl, fs);
                wood.strippedId = sw.id;
            }
            var planks = Reg(w + "_planks", new Block()).Hard(2f, 3f).Axe().Snd(snd).T1(w + "_planks").Flam(nether ? 0 : 5, nether ? 0 : 20);
            Stairs(w + "_stairs", planks); Slab(w + "_slab", planks);
            if (bamboo)
            {
                var mosaic = Reg("bamboo_mosaic", new Block()).Hard(2f, 3f).Axe().Snd(snd).T1("bamboo_mosaic").Flam(5, 20);
                Stairs("bamboo_mosaic_stairs", mosaic); Slab("bamboo_mosaic_slab", mosaic);
            }
            Reg(w + "_fence", new FenceBlock(planks, "wood")).Tab(CreativeTab.Building);
            Reg(w + "_fence_gate", new FenceGateBlock(planks));
            Reg(w + "_door", new DoorBlock(w + "_door", false)).Axe().Snd(snd);
            Reg(w + "_trapdoor", new TrapdoorBlock(w + "_trapdoor", false)).Axe().Snd(snd);
            Reg(w + "_pressure_plate", new PressurePlateBlock(planks, PressurePlateBlock.Kind.Wood)).Tab(CreativeTab.Redstone);
            Reg(w + "_button", new ButtonBlock(planks, 30)).Tab(CreativeTab.Redstone);
            if (!nether && !bamboo)
            {
                TintType tt = w == "spruce" ? TintType.Spruce : (w == "birch" ? TintType.Birch : (w == "mangrove" ? TintType.Mangrove : (w == "cherry" || w == "pale_oak" ? TintType.None : TintType.Foliage)));
                var leaves = Reg(w + "_leaves", new LeavesBlock(w + "_leaves", tt));
                string sap = w == "mangrove" ? "mangrove_propagule" : w + "_sapling";
                leaves.saplingId = sap;
                leaves.dropsApples = w == "oak" || w == "dark_oak";
                Reg(sap, new SaplingBlock(sap, w));
            }
        }
    }

    /// <summary>Block with a horizontal facing (front texture), e.g. carved pumpkin, glazed terracotta.</summary>
    public class HorizontalBlock : Block
    {
        public int topTex, sideTex, frontTex;
        public bool rotateAllFaces;
        public HorizontalBlock(string top, string side, string front, bool glazed = false)
        {
            stateCount = 4; topTex = Tex.Id(top); sideTex = Tex.Id(side); frontTex = Tex.Id(front); rotateAllFaces = glazed;
            SetTex(topTex, topTex, sideTex); particleTex = frontTex;
        }
        readonly int[] t = new int[6];
        static readonly int[] noRot = new int[6];
        public override void Emit(MeshCtx ctx, int meta)
        {
            Dir f = StateBits.FacingDir(meta);
            if (rotateAllFaces)
            {
                for (int i = 0; i < 6; i++) t[i] = frontTex;
                var rots = new int[6];
                rots[1] = meta & 3; rots[0] = (4 - (meta & 3)) & 3;
                for (int i = 2; i < 6; i++) rots[i] = (meta + i) & 3;
                ctx.CubeRot(t, rots, 0xFFFFFFFFu);
                return;
            }
            t[0] = topTex; t[1] = topTex;
            for (int i = 2; i < 6; i++) t[i] = sideTex;
            t[(int)f] = frontTex;
            ctx.CubeRot(t, noRot, ctx.TintColor(tint));
        }
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(DirUtil.Opposite(ctx.playerFacing)));
    }

    /// <summary>Copper block with oxidation stage (0..3); random ticks advance oxidation, axes scrape, honeycomb waxes (waxed = separate flag via item).</summary>
    public class CopperBlock : Block
    {
        public int stage;
        public CopperBlock(int stage) { this.stage = stage; randomTicks = stage < 3; creativeTab = CreativeTab.Building; }
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (stage >= 3 || !rng.Chance(0.0569f * 0.25f)) return;
            var next = Blocks.Get(Oxidation.Next(id));
            if (next != null) w.SetBlock(pos, next, meta);
        }
    }

    public static class Oxidation
    {
        static readonly string[] Pre = { "", "exposed_", "weathered_", "oxidized_" };
        public static string Next(string id) => Shift(id, 1);
        public static string Prev(string id) => Shift(id, -1);
        static string Shift(string id, int d)
        {
            int stage = 0; string baseName = id;
            for (int i = 3; i >= 1; i--) if (id.StartsWith(Pre[i])) { stage = i; baseName = id.Substring(Pre[i].Length); break; }
            int ns = stage + d;
            if (ns < 0 || ns > 3) return null;
            if (baseName == "copper_block" || baseName == "copper") return ns == 0 ? "copper_block" : Pre[ns] + "copper";
            return Pre[ns] + baseName;
        }
    }

    /// <summary>26.2 potent sulfur: emits noxious particles; geyser-like bursts when water above.</summary>
    public class PotentSulfurBlock : Block
    {
        public PotentSulfurBlock() { randomTicks = true; }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            Int3 up = pos.Offset(Dir.Up);
            if (w.IsWater(up))
            {
                if (rng.Chance(0.3f)) Particles.Bubble(w, up.Center + new Vector3(rng.Range(-0.3f, 0.3f), -0.4f, rng.Range(-0.3f, 0.3f)), true);
            }
            else if (w.IsAir(up) && rng.Chance(0.15f)) Particles.SulfurCloud(w, up.Center - new Vector3(0, 0.4f, 0));
        }
        public override void OnSteppedOn(World w, Int3 pos, int meta, Entity e)
        {
            if (e is LivingEntity le && w.rand.Chance(0.02f)) le.AddEffect(new EffectInstance(Effect.Nausea, 100, 0));
        }
    }
}
