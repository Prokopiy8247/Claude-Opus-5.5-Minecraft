using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public static partial class Blocks
    {
        static void RegisterMore()
        {
            // ------------------------------------------------------------------ crafting & workstations
            Reg("crafting_table", new WorkstationBlock("crafting_table", Station.Crafting)).T6("oak_planks", "crafting_table_top", "crafting_table_front", "crafting_table_side", "crafting_table_side", "crafting_table_front").Hard(2.5f).Axe().Snd(SoundType.Wood).Tab(CreativeTab.Functional).Flam(5, 20);
            Reg("furnace", new FurnaceBlock("furnace")).Hard(3.5f).Pick().Tab(CreativeTab.Functional);
            Reg("smoker", new FurnaceBlock("smoker")).Hard(3.5f).Pick().Tab(CreativeTab.Functional);
            Reg("blast_furnace", new FurnaceBlock("blast_furnace")).Hard(3.5f).Pick().Tab(CreativeTab.Functional);
            Reg("smithing_table", new WorkstationBlock("smithing_table", Station.Smithing)).T6("smithing_table_bottom", "smithing_table_top", "smithing_table_front", "smithing_table_front", "smithing_table_side", "smithing_table_side").Hard(2.5f).Axe().Snd(SoundType.Wood).Tab(CreativeTab.Functional);
            Reg("fletching_table", new WorkstationBlock("fletching_table", Station.None)).T6("birch_planks", "fletching_table_top", "fletching_table_front", "fletching_table_front", "fletching_table_side", "fletching_table_side").Hard(2.5f).Axe().Snd(SoundType.Wood).Tab(CreativeTab.Functional);
            Reg("cartography_table", new WorkstationBlock("cartography_table", Station.None)).T6("dark_oak_planks", "cartography_table_top", "cartography_table_side", "cartography_table_side", "cartography_table_side", "cartography_table_side").Hard(2.5f).Axe().Snd(SoundType.Wood).Tab(CreativeTab.Functional);
            Reg("loom", new LoomBlock()).Hard(2.5f).Axe().Snd(SoundType.Wood).Tab(CreativeTab.Functional);
            Reg("stonecutter", new StonecutterBlock()).Hard(3.5f).Pick().Tab(CreativeTab.Functional);
            Reg("grindstone", new GrindstoneBlock()).Hard(2f).Pick().Tab(CreativeTab.Functional);
            Reg("enchanting_table", new EnchantingTableBlock()).Hard(5f, 1200f).Pick().Tab(CreativeTab.Functional).Light(7);
            Reg("anvil", new AnvilBlock(0)); Reg("chipped_anvil", new AnvilBlock(1)); Reg("damaged_anvil", new AnvilBlock(2));
            Reg("brewing_stand", new BrewingStandBlock());
            Reg("cauldron", new CauldronBlock());
            Reg("composter", new ComposterBlock());
            Reg("lectern", new LecternBlock());
            Reg("bookshelf", new Block()).T2("oak_planks", "bookshelf").Hard(1.5f).Axe().Snd(SoundType.Wood).Tab(CreativeTab.Functional).Flam(30, 20).Drops("book");
            Reg("chiseled_bookshelf", new HorizontalBlock("chiseled_bookshelf_top", "chiseled_bookshelf_side", "chiseled_bookshelf_front")).Hard(1.5f).Axe().Snd(SoundType.Wood).Tab(CreativeTab.Functional);
            foreach (var w in new[] { "oak", "spruce", "birch", "jungle", "acacia", "dark_oak", "mangrove", "cherry", "pale_oak", "bamboo", "crimson", "warped" })
                Reg(w + "_shelf", new ShelfBlock(w)).Tab(CreativeTab.Functional);
            // ------------------------------------------------------------------ storage
            Reg("chest", new ChestBlock(false)); Reg("trapped_chest", new ChestBlock(true)).Tab(CreativeTab.Redstone);
            Reg("copper_chest", new ChestBlock(false) { copper = true });
            Reg("ender_chest", new EnderChestBlock());
            Reg("barrel", new BarrelBlock());
            Reg("shulker_box", new ShulkerBoxBlock("shulker_box"));
            foreach (var c in Colors) Reg(c + "_shulker_box", new ShulkerBoxBlock(c + "_shulker_box"));
            // ------------------------------------------------------------------ beds
            foreach (var c in Colors) Reg(c + "_bed", new BedBlock(c));
            // ------------------------------------------------------------------ misc functional
            Reg("campfire", new CampfireBlock(false)); Reg("soul_campfire", new CampfireBlock(true));
            Reg("scaffolding", new ScaffoldingBlock());
            Reg("jukebox", new JukeboxBlock());
            Reg("spawner", new SpawnerBlock());
            Reg("trial_spawner", new SpawnerBlock { trial = true }).Hard(50f).T1("trial_spawner");
            Reg("beacon", new BeaconBlock());
            Reg("respawn_anchor", new RespawnAnchorBlock());
            Reg("lodestone", new Block()).T2("lodestone_top", "lodestone_side").Hard(3.5f).Pick().Tab(CreativeTab.Functional);
            Reg("bell", new BellBlock());
            Reg("flower_pot", new FlowerPotBlock());
            Reg("end_rod", new EndRodBlock());
            Reg("lightning_rod", new EndRodBlock { copperRod = true }).Hard(3f).Pick(Tier.Stone).Snd(SoundType.Copper).Tab(CreativeTab.Redstone);
            Reg("slime_block", new TranslucentCube(RenderLayer.Translucent, 1)).T1("slime_block").Hard(0).Snd(SoundType.Slime).Tab(CreativeTab.Redstone).jumpFactor = 1f;
            Get("slime_block").speedFactor = 1f;
            Reg("honey_block", new TranslucentCube(RenderLayer.Translucent, 1)).T3("honey_block_top", "honey_block_bottom", "honey_block_side").Hard(0).Snd(SoundType.Honey).Tab(CreativeTab.Redstone).Speed(0.4f).jumpFactor = 0.5f;
            Reg("powder_snow", new PowderSnowBlock());
            // ------------------------------------------------------------------ redstone
            Reg("redstone_wire", new RedstoneWireBlock());
            Reg("lever", new LeverBlock());
            Reg("stone_button", new ButtonBlock(Get("stone"), 20)); Reg("polished_blackstone_button", new ButtonBlock(Get("polished_blackstone"), 20));
            Reg("stone_pressure_plate", new PressurePlateBlock(Get("stone"), PressurePlateBlock.Kind.Stone)); Reg("polished_blackstone_pressure_plate", new PressurePlateBlock(Get("polished_blackstone"), PressurePlateBlock.Kind.Stone));
            Reg("light_weighted_pressure_plate", new PressurePlateBlock(Get("gold_block"), PressurePlateBlock.Kind.Light)); Reg("heavy_weighted_pressure_plate", new PressurePlateBlock(Get("iron_block"), PressurePlateBlock.Kind.Heavy));
            Reg("repeater", new RepeaterBlock()).noItem = true;
            Reg("comparator", new ComparatorBlock()).noItem = true;
            Reg("redstone_lamp", new RedstoneLampBlock());
            Reg("observer", new ObserverBlock());
            Reg("piston", new PistonBlock(false)); Reg("sticky_piston", new PistonBlock(true)); Reg("piston_head", new PistonHeadBlock());
            Reg("tnt", new TntBlock());
            Reg("target", new TargetBlock());
            Reg("note_block", new NoteBlock());
            Reg("daylight_detector", new DaylightDetectorBlock());
            Reg("dispenser", new DispenserBlock(false)); Reg("dropper", new DispenserBlock(true));
            Reg("hopper", new HopperBlock());
            Reg("rail", new RailBlock(RailKind.Normal)); Reg("powered_rail", new RailBlock(RailKind.Powered)); Reg("detector_rail", new RailBlock(RailKind.Detector)); Reg("activator_rail", new RailBlock(RailKind.Activator));
            Reg("crafter", new HorizontalBlock("crafter_top", "crafter_side", "crafter_front")).Hard(1.5f).Pick().Tab(CreativeTab.Redstone);
            // ------------------------------------------------------------------ portals / end
            Reg("end_portal_frame", new EndPortalFrameBlock());
            Reg("end_portal", new EndPortalBlock(false)); Reg("end_gateway", new EndPortalBlock(true));
            Reg("dragon_egg", new DragonEggBlock());
            Reg("chorus_plant", new ChorusPlantBlock()); Reg("chorus_flower", new ChorusFlowerBlock());
            // ------------------------------------------------------------------ skulls
            foreach (var sk in new[] { "skeleton_skull", "wither_skeleton_skull", "zombie_head", "creeper_head", "piglin_head", "dragon_head", "player_head" }) Reg(sk, new SkullBlock(sk));
            // ------------------------------------------------------------------ plants & crops (2)
            Reg("pumpkin_stem", new StemBlock("pumpkin")); Reg("melon_stem", new StemBlock("melon"));
            Reg("sweet_berry_bush", new SweetBerryBushBlock());
            Reg("nether_wart", new NetherWartBlock());
            Reg("cocoa", new CocoaBlock());
            Reg("bamboo", new BambooBlock());
            Reg("bamboo_sapling", new PlantBlock("bamboo_stage0")).NoItem();
            Reg("lily_pad", new LilyPadBlock());
            Reg("seagrass", new WaterPlantBlock("seagrass", false)); Reg("tall_seagrass", new WaterPlantBlock("tall_seagrass_bottom", true)).NoItem();
            Reg("kelp", new WaterPlantBlock("kelp", false) { kelpTop = true }); Reg("kelp_plant", new WaterPlantBlock("kelp_plant", false)).NoItem();
            Reg("dried_kelp_block", new Block()).T2("dried_kelp_top", "dried_kelp_side").Hard(0.5f).Hoe().Snd(SoundType.Grass).Tab(CreativeTab.Building).Flam(30, 60);
            foreach (var k in new[] { "tube", "brain", "bubble", "fire", "horn" })
            {
                Reg(k + "_coral_block", new Block()).T1(k + "_coral_block").Hard(1.5f).Pick().Snd(SoundType.Coral).Tab(CreativeTab.Natural);
                Reg("dead_" + k + "_coral_block", new Block()).T1("dead_" + k + "_coral_block").Hard(1.5f).Pick().Tab(CreativeTab.Natural);
                Reg(k + "_coral", new WaterPlantBlock(k + "_coral", false)).Snd(SoundType.Coral);
                Reg(k + "_coral_fan", new WaterPlantBlock(k + "_coral_fan", false)).Snd(SoundType.Coral);
            }
            Reg("sea_pickle", new SeaPickleBlock());
            Reg("cave_vines", new CaveVinesBlock()); Get("cave_vines").noItem = true;
            Reg("spore_blossom", new HangingPlantBlock("spore_blossom")).Light(0);
            Reg("hanging_roots", new HangingPlantBlock("hanging_roots"));
            Reg("pale_hanging_moss", new HangingPlantBlock("pale_hanging_moss"));
            Reg("big_dripleaf", new DripleafBlock(true)); Reg("small_dripleaf", new DripleafBlock(false));
            Reg("pointed_dripstone", new PointedDripstoneBlock());
            foreach (var (n, sz) in new[] { ("small_amethyst_bud", 0), ("medium_amethyst_bud", 1), ("large_amethyst_bud", 2), ("amethyst_cluster", 3) })
                Reg(n, new AmethystClusterBlock(n, sz));
            Reg("sculk", new Block()).T1("sculk").Hard(0.2f).Hoe().Snd(SoundType.Sculk).Tab(CreativeTab.Natural).Xp(1, 1);
            Reg("sculk_vein", new VineBlock("sculk_vein", TintType.None)).Snd(SoundType.Sculk);
            Reg("sculk_sensor", new SculkSensorBlock()).Hard(1.5f).Hoe().Snd(SoundType.Sculk).Tab(CreativeTab.Redstone);
            Reg("sculk_shrieker", new SculkShriekerBlock()).Hard(3f).Hoe().Snd(SoundType.Sculk).Tab(CreativeTab.Natural);
            Reg("sculk_catalyst", new Block()).T3("sculk_catalyst_top", "sculk_catalyst_bottom", "sculk_catalyst_side").Hard(3f).Hoe().Snd(SoundType.Sculk).Light(6).Tab(CreativeTab.Natural).Xp(5, 5);
            Reg("creaking_heart", new CreakingHeartBlock());
            Reg("ochre_froglight", new PillarBlock("ochre_froglight_top", "ochre_froglight_side")).Hard(0.3f).Light(15).Tab(CreativeTab.Natural);
            Reg("verdant_froglight", new PillarBlock("verdant_froglight_top", "verdant_froglight_side")).Hard(0.3f).Light(15).Tab(CreativeTab.Natural);
            Reg("pearlescent_froglight", new PillarBlock("pearlescent_froglight_top", "pearlescent_froglight_side")).Hard(0.3f).Light(15).Tab(CreativeTab.Natural);
            // candles
            Reg("candle", new CandleBlock("candle"));
            foreach (var c in Colors) Reg(c + "_candle", new CandleBlock(c + "_candle"));
        }
    }

    public enum Station { None, Crafting, Smithing }

    public class WorkstationBlock : Block
    {
        public Station station; string key;
        public WorkstationBlock(string key, Station s) { this.key = key; station = s; }
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (station == Station.Crafting) { player.OpenMenu(new CraftingTableMenu(player, pos)); return true; }
            if (station == Station.Smithing) { player.OpenMenu(new SmithingMenu(player, pos)); return true; }
            return false;
        }
    }

    public class FurnaceBlock : Block
    {
        public string kind;
        int front, frontOn, side, top;
        public FurnaceBlock(string kind)
        {
            this.kind = kind; stateCount = 8;
            front = Tex.Id(kind + "_front"); frontOn = Tex.Id(kind + "_front_on"); side = Tex.Id(kind + "_side"); top = Tex.Id(kind + "_top");
            SetTex(top, top, side); particleTex = front;
        }
        public override byte GetLightEmission(int meta) => (byte)((meta & 4) != 0 ? 13 : 0);
        readonly int[] t = new int[6];
        static readonly int[] noRot = new int[6];
        public override void Emit(MeshCtx ctx, int meta)
        {
            Dir f = StateBits.FacingDir(meta);
            t[0] = top; t[1] = top; for (int i = 2; i < 6; i++) t[i] = side;
            t[(int)f] = (meta & 4) != 0 ? frontOn : front;
            ctx.CubeRot(t, noRot, 0xFFFFFFFFu);
        }
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(DirUtil.Opposite(ctx.playerFacing)));
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new FurnaceEntity { kind = kind };
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (w.GetBlockEntity(pos) is FurnaceEntity fe) player.OpenMenu(new FurnaceMenu(player, fe, Blocks.PrettyName(kind)));
            return true;
        }
        public override int GetComparatorOutput(World w, Int3 pos, int meta) => w.GetBlockEntity(pos) is FurnaceEntity fe ? fe.ComparatorSignal() : 0;
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if ((meta & 4) == 0) return;
            Dir f = StateBits.FacingDir(meta);
            Vector3 n = DirUtil.Normal[(int)f];
            Vector3 p = pos.Center + n * 0.52f + new Vector3(0, rng.Range(-0.4f, 0.1f), 0) + Vector3.Cross(n, Vector3.up) * rng.Range(-0.3f, 0.3f);
            if (rng.Chance(0.1f)) Sounds.Play("block.furnace.fire_crackle", pos.Center, 1f, 1f);
            Particles.Smoke(w, p, 1, 0.2f); Particles.Flame(w, p, "flame");
            if (kind == "smoker") Particles.Smoke(w, pos.Center + Vector3.up * 0.6f, 1, 0.5f);
        }
    }

    // ============================================================================ chests
    /// <summary>meta: facing(0-1) | type(2-3): 0 single, 1 left, 2 right</summary>
    public class ChestBlock : Block
    {
        public bool trapped, copper;
        public ChestBlock(bool trapped)
        {
            this.trapped = trapped; stateCount = 12; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.None; isFullCubeShape = false;
            hardness = 2.5f; tool = ToolType.Axe; sound = SoundType.Wood; creativeTab = CreativeTab.Functional; SetAllTex(Tex.Id("oak_planks"));
            flammability = 0;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta) { }
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new ChestEntity();
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(1, 0, 1, 15, 14, 15));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(1, 0, 1, 15, 14, 15));
        public static int Type(int m) => (m >> 2) & 3;
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            Dir f = DirUtil.Opposite(ctx.playerFacing);
            int m = DirUtil.HorizIndex(f);
            if (!ctx.sneaking)
            {
                // connect with a single chest to the left/right facing the same way
                Dir left = DirUtil.RotateCCW(f), right = DirUtil.RotateCW(f);
                foreach (var (d, myType, otherType) in new[] { (left, 2, 1), (right, 1, 2) })
                {
                    Int3 np = ctx.pos.Offset(d);
                    ushort ns = ctx.world.GetState(np);
                    if (Blocks.ByState[ns] == this)
                    {
                        int nm = ns - baseState;
                        if (Type(nm) == 0 && StateBits.FacingDir(nm) == f)
                        {
                            ctx.world.SetState(np, State((nm & 3) | (otherType << 2)), 0);
                            return State(m | (myType << 2));
                        }
                    }
                }
            }
            return State(m);
        }
        public Int3? Partner(World w, Int3 pos, int meta)
        {
            int t = Type(meta);
            if (t == 0) return null;
            Dir f = StateBits.FacingDir(meta);
            Dir d = t == 1 ? DirUtil.RotateCW(f) : DirUtil.RotateCCW(f);
            Int3 p = pos.Offset(d);
            return w.GetBlock(p) == this ? p : (Int3?)null;
        }
        public override void OnRemoved(World w, Int3 pos, int meta, ushort newState)
        {
            var pp = Partner(w, pos, meta);
            if (pp.HasValue) { int pm = w.GetMeta(pp.Value); w.SetState(pp.Value, State(pm & 3), 0); }
        }
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (!(w.GetBlockEntity(pos) is ChestEntity ce)) return true;
            if (!Blocks.ByState[w.GetState(pos.Offset(Dir.Up))].isAir && Blocks.StateOpaque[w.GetState(pos.Offset(Dir.Up))]) return true;
            var pp = Partner(w, pos, meta);
            if (pp.HasValue && w.GetBlockEntity(pp.Value) is ChestEntity ce2)
            {
                bool meLeft = Type(meta) == 1;
                var dc = meLeft ? new DoubleChestContainer(ce, ce2) : new DoubleChestContainer(ce2, ce);
                ce.OpenBy(player); ce2.OpenBy(player);
                player.OpenMenu(new ChestMenu(player, dc, 6, "Large Chest"));
            }
            else { ce.OpenBy(player); player.OpenMenu(new ChestMenu(player, ce, 3, trapped ? "Chest" : (copper ? "Copper Chest" : "Chest"))); }
            return true;
        }
        public override bool IsRedstoneSource(int meta) => trapped;
        public override int GetWeakPower(World w, Int3 pos, int meta, Dir towards) => trapped && w.GetBlockEntity(pos) is ChestEntity ce ? Mathf.Min(15, ce.openers) : 0;
        public override int GetStrongPower(World w, Int3 pos, int meta, Dir towards) => towards == Dir.Down ? GetWeakPower(w, pos, meta, towards) : 0;
        public override int GetComparatorOutput(World w, Int3 pos, int meta) => w.GetBlockEntity(pos) is ChestEntity ce ? ce.ComparatorSignal() : 0;
    }

    public class EnderChestBlock : ChestBlock
    {
        public EnderChestBlock() : base(false) { stateCount = 4; hardness = 22.5f; blastResistance = 600; tool = ToolType.Pickaxe; requiresTool = true; lightEmission = 7; sound = SoundType.Stone; SetAllTex(Tex.Id("obsidian")); }
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(DirUtil.Opposite(ctx.playerFacing)));
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (!(w.GetBlockEntity(pos) is ChestEntity ce)) return true;
            ce.OpenBy(player);
            player.enderChest.chest = ce;
            player.OpenMenu(new ChestMenu(player, player.enderChest, 3, "Ender Chest"));
            return true;
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if (!CanHarvestWith(tool)) return;
            drops.Add(new ItemStack("obsidian", 8));
        }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            for (int i = 0; i < 2; i++) Particles.Portal(w, pos.Center + new Vector3(rng.Range(-0.5f, 0.5f), rng.Range(-0.3f, 0.5f), rng.Range(-0.5f, 0.5f)));
        }
    }

    /// <summary>meta: facing(0-5) | open(3)</summary>
    public class BarrelBlock : Block
    {
        int top, topOpen, side, bottom;
        public BarrelBlock()
        {
            stateCount = 16; hardness = 2.5f; tool = ToolType.Axe; sound = SoundType.Wood; creativeTab = CreativeTab.Functional;
            top = Tex.Id("barrel_top"); topOpen = Tex.Id("barrel_top_open"); side = Tex.Id("barrel_side"); bottom = Tex.Id("barrel_bottom"); SetAllTex(side);
        }
        readonly int[] t = new int[6], r = new int[6];
        public override void Emit(MeshCtx ctx, int meta)
        {
            Dir f = (Dir)(meta & 7);
            for (int i = 0; i < 6; i++) { t[i] = side; r[i] = PistonBlock.PistonUVRot((Dir)i, f); }
            t[(int)f] = (meta & 8) != 0 ? topOpen : top; t[(int)DirUtil.Opposite(f)] = bottom;
            ctx.CubeRot(t, r, 0xFFFFFFFFu);
        }
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            Vector3 look = MathX.YawPitchToDir(ctx.playerYaw, ctx.playerPitch);
            return State((int)DirUtil.Opposite(DirUtil.FromVector(look)));
        }
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new BarrelEntity();
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (w.GetBlockEntity(pos) is BarrelEntity be) { be.Open(); player.OpenMenu(new ChestMenu(player, be, 3, "Barrel")); }
            return true;
        }
        public override int GetComparatorOutput(World w, Int3 pos, int meta) => w.GetBlockEntity(pos) is BarrelEntity be ? be.ComparatorSignal() : 0;
    }

    public class ShulkerBoxBlock : Block
    {
        public ShulkerBoxBlock(string tex)
        {
            stateCount = 6; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Opaque; hardness = 2f; tool = ToolType.Pickaxe;
            creativeTab = CreativeTab.Colored; SetAllTex(Tex.Id(tex)); push = PushReaction.Destroy; isFullCubeShape = false;
        }
        public override int GetOccludingFaces(int meta) => 0;
        /// <summary>The closed box: a full cube in the box colour (the lid does not animate).</summary>
        public override void Emit(MeshCtx ctx, int meta) => ctx.Box(0, 0, 0, 1, 1, 1, faceTex, 0xFFFFFFFFu);
        public override ushort GetPlacementState(ref PlaceContext ctx) => State((int)ctx.clickedFace);
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new ShulkerBoxEntity();
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (w.GetBlockEntity(pos) is ShulkerBoxEntity sb) { sb.OpenBy(player); player.OpenMenu(new ShulkerMenu(player, sb)); }
            return true;
        }
        public override void OnPlaced(World w, Int3 pos, int meta, Entity placer, ItemStack stack)
        {
            string contents = stack?.Get("contents");
            if (contents != null && w.GetBlockEntity(pos) is ShulkerBoxEntity sb)
            {
                var parts = contents.Split(';');
                for (int i = 0; i < 27 && i < parts.Length; i++) sb.items[i] = ItemStack.Deserialize(parts[i]);
            }
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            var s = new ItemStack(item, 1);
            if (w.GetBlockEntity(pos) is ShulkerBoxEntity sb)
            {
                bool any = false; var parts = new string[27];
                for (int i = 0; i < 27; i++) { parts[i] = sb.items[i]?.Serialize() ?? ""; if (sb.items[i] != null) any = true; }
                if (any) s.Set("contents", string.Join(";", parts));
            }
            drops.Add(s);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(new AABB(0, 0, 0, 1, 1, 1));
        public override int GetComparatorOutput(World w, Int3 pos, int meta) => w.GetBlockEntity(pos) is ShulkerBoxEntity sb ? sb.ComparatorSignal() : 0;
    }

    // ============================================================================ beds
    /// <summary>meta: facing(0-1: toward head) | head(2) | occupied(3)</summary>
    public class BedBlock : Block
    {
        public string color;
        int topHead, topFoot, sideTex, wood;
        public BedBlock(string color)
        {
            this.color = color; stateCount = 16; opaqueCube = false; lightOpacity = 0; hardness = 0.2f; sound = SoundType.Wood; isFullCubeShape = false;
            topHead = Tex.Id(color + "_bed_head"); topFoot = Tex.Id(color + "_bed_foot"); sideTex = Tex.Id(color + "_bed_side"); wood = Tex.Id("oak_planks");
            SetAllTex(topFoot); creativeTab = CreativeTab.Functional; push = PushReaction.Destroy;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public static bool IsHead(int m) => (m & 4) != 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            Dir f = StateBits.FacingDir(meta);
            bool head = IsHead(meta);
            ctx.rot = DirUtil.HorizIndex(f);
            var t = new[] { wood, head ? topHead : topFoot, sideTex, sideTex, sideTex, sideTex };
            // mattress 16x6 (y 3..9)
            ctx.Box(0, 3f / 16, 0, 1, 9f / 16, 1, t, 0xFFFFFFFFu, head ? 0 : 0, true);
            // legs
            var lt = new[] { wood, wood, wood, wood, wood, wood };
            if (head) { ctx.Box(0, 0, 13f / 16, 3f / 16, 3f / 16, 1, lt, 0xFFFFFFFFu); ctx.Box(13f / 16, 0, 13f / 16, 1, 3f / 16, 1, lt, 0xFFFFFFFFu); }
            else { ctx.Box(0, 0, 0, 3f / 16, 3f / 16, 3f / 16, lt, 0xFFFFFFFFu); ctx.Box(13f / 16, 0, 0, 1, 3f / 16, 3f / 16, lt, 0xFFFFFFFFu); }
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 9, 16));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 9, 16));
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            Dir f = ctx.playerFacing;
            Int3 headPos = ctx.pos.Offset(f);
            if (!ctx.world.GetBlock(headPos).replaceable || !ctx.world.IsSturdy(headPos.Offset(Dir.Down), Dir.Up)) return 0;
            return State(DirUtil.HorizIndex(f));
        }
        public override void OnPlaced(World w, Int3 pos, int meta, Entity placer, ItemStack stack)
        {
            Dir f = StateBits.FacingDir(meta);
            w.SetState(pos.Offset(f), State(DirUtil.HorizIndex(f) | 4), SetFlags.Hooks);
        }
        public Int3 Other(Int3 pos, int meta) { Dir f = StateBits.FacingDir(meta); return IsHead(meta) ? pos.Offset(DirUtil.Opposite(f)) : pos.Offset(f); }
        public override void OnBroken(World w, Int3 pos, int meta, Entity breaker)
        {
            Int3 o = Other(pos, meta);
            if (w.GetBlock(o) == this) w.SetState(o, 0, SetFlags.Hooks);
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) { if (!IsHead(meta)) drops.Add(new ItemStack(item, 1)); else drops.Add(new ItemStack(item, 1)); }
        public override bool CanSurvive(World w, Int3 pos, int meta) => w.GetBlock(Other(pos, meta)) == this;
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            Int3 headPos = IsHead(meta) ? pos : Other(pos, meta);
            if (w.dim != DimensionId.Overworld)
            {
                w.SetState(pos, 0, SetFlags.Hooks);
                Int3 o = Other(pos, meta); if (w.GetBlock(o) == this) w.SetState(o, 0, SetFlags.Hooks);
                Explosion.Explode(w, null, headPos.Center, 5f, true, true);
                return true;
            }
            player.TrySleep(headPos);
            return true;
        }
        public override BlockItem CustomItem() => new TallBlockItem();
        public override void OnFallenUpon(World w, Int3 pos, int meta, Entity e, float fallDistance) => e.ApplyFallDamage(fallDistance * 0.5f);
    }

    // ============================================================================ campfire
    /// <summary>meta: facing(0-1) | lit(2)</summary>
    public class CampfireBlock : Block
    {
        public bool soul;
        int log, logLit, fire;
        public CampfireBlock(bool soul)
        {
            this.soul = soul; stateCount = 8; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 2f; tool = ToolType.Axe; sound = SoundType.Wood;
            log = Tex.Id("campfire_log"); logLit = Tex.Id(soul ? "soul_campfire_log_lit" : "campfire_log_lit"); fire = Tex.Id(soul ? "soul_fire" : "fire");
            SetAllTex(log); creativeTab = CreativeTab.Functional; isFullCubeShape = false;
        }
        public static bool Lit(int m) => (m & 4) != 0;
        public override int DefaultMeta => 4;
        public override byte GetLightEmission(int meta) => Lit(meta) ? (byte)(soul ? 10 : 15) : (byte)0;
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            ctx.rot = meta & 3;
            int lg = Lit(meta) ? logLit : log;
            var t = new[] { lg, lg, lg, lg, lg, lg };
            // four logs: two along X at bottom, two along Z on top
            ctx.Box(1f / 16, 0, 0, 5f / 16, 4f / 16, 1, t, 0xFFFFFFFFu);
            ctx.Box(11f / 16, 0, 0, 15f / 16, 4f / 16, 1, t, 0xFFFFFFFFu);
            ctx.Box(0, 3f / 16, 1f / 16, 1, 7f / 16, 5f / 16, t, 0xFFFFFFFFu);
            ctx.Box(0, 3f / 16, 11f / 16, 1, 7f / 16, 15f / 16, t, 0xFFFFFFFFu);
            ctx.rot = 0;
            // embers
            ctx.Box(5f / 16, 0, 5f / 16, 11f / 16, 1f / 16, 11f / 16, new[] { lg, Lit(meta) ? logLit : log, lg, lg, lg, lg }, 0xFFFFFFFFu);
            if (Lit(meta)) ctx.Cross(fire, 0xFFFFFFFFu, 1f, 0.05f, 1f);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 7, 16));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 7, 16));
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(ctx.playerFacing) | 4);
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new CampfireEntity();
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            var held = player.MainHand;
            if (held != null && Recipes.FindSmelting(held.item, "campfire") != null && w.GetBlockEntity(pos) is CampfireEntity ce)
            {
                if (ce.AddFood(held)) { if (!player.IsCreative) held.count--; return true; }
            }
            return false;
        }
        public override void OnEntityInside(World w, Int3 pos, int meta, Entity e)
        {
            if (Lit(meta) && e is LivingEntity le && !le.fireImmune && e.position.y < pos.y + 0.5f) le.Hurt(DamageSource.InFire, soul ? 2 : 1);
        }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (!Lit(meta)) return;
            if (rng.Chance(0.1f)) Sounds.Play("block.campfire.crackle", pos.Center, 0.5f + rng.NextFloat(), rng.Range(0.6f, 1.3f));
            Particles.CampfireSmoke(w, pos.Center + new Vector3(rng.Range(-0.2f, 0.2f), 0.3f, rng.Range(-0.2f, 0.2f)), w.GetBlock(pos.Offset(Dir.Down)).id == "hay_block");
            if (rng.Chance(0.2f)) Particles.Flame(w, pos.Center + new Vector3(rng.Range(-0.3f, 0.3f), -0.2f, rng.Range(-0.3f, 0.3f)), soul ? "soul_flame" : "flame");
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if (tool != null && tool.GetEnchant(Enchant.SilkTouch) > 0) drops.Add(new ItemStack(item, 1));
            else drops.Add(new ItemStack(soul ? "soul_soil" : "charcoal", soul ? 1 : 2));
        }
    }

    public class ScaffoldingBlock : Block
    {
        public ScaffoldingBlock()
        {
            stateCount = 8; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 0; sound = SoundType.Scaffolding; climbable = true;
            this.T3("scaffolding_top", "scaffolding_bottom", "scaffolding_side"); creativeTab = CreativeTab.Functional; isFullCubeShape = false; flammability = 60; fireSpread = 60;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            ctx.Box(0, 14f / 16, 0, 1, 1, 1, faceTex, 0xFFFFFFFFu, 0, false);
            var s = faceTex[2];
            var t = new[] { s, s, s, s, s, s };
            ctx.Box(0, 0, 0, 2f / 16, 14f / 16, 2f / 16, t, 0xFFFFFFFFu); ctx.Box(14f / 16, 0, 0, 1, 14f / 16, 2f / 16, t, 0xFFFFFFFFu);
            ctx.Box(0, 0, 14f / 16, 2f / 16, 14f / 16, 1, t, 0xFFFFFFFFu); ctx.Box(14f / 16, 0, 14f / 16, 1, 14f / 16, 1, t, 0xFFFFFFFFu);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 14, 0, 16, 16, 16));
        public override bool CanBeReplaced(int meta, ref PlaceContext ctx) => ctx.stack != null && ctx.stack.item.block == this && false;
    }

    public class JukeboxBlock : Block
    {
        public JukeboxBlock() { this.T2("jukebox_top", "jukebox_side"); hardness = 2f; tool = ToolType.Axe; sound = SoundType.Wood; creativeTab = CreativeTab.Functional; }
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new JukeboxEntity();
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (!(w.GetBlockEntity(pos) is JukeboxEntity je)) return false;
            if (je.record != null) { je.DropContents(); return true; }
            var held = player.MainHand;
            if (held != null && held.item.id.StartsWith("music_disc"))
            {
                je.record = held.CopyWithCount(1);
                if (!player.IsCreative) held.count--;
                Sounds.PlayMusicAt(pos, held.item.id);
                GameManager.Instance?.hud?.ShowActionBar("Now Playing: " + held.item.displayName);
                return true;
            }
            return false;
        }
        public override int GetComparatorOutput(World w, Int3 pos, int meta) => w.GetBlockEntity(pos) is JukeboxEntity je && je.record != null ? 15 : 0;
    }

    public class SpawnerBlock : Block
    {
        public bool trial;
        public SpawnerBlock()
        {
            opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 5f; tool = ToolType.Pickaxe; requiresTool = true; sound = SoundType.Metal;
            this.T1("spawner"); xpDropMin = 15; xpDropMax = 43; creativeTab = CreativeTab.SpawnEggs;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => trial ? new TrialSpawnerEntity() : new SpawnerEntity();
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) { }
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            var held = player.MainHand;
            if (held != null && held.item is SpawnEggItem egg && w.GetBlockEntity(pos) is SpawnerEntity se)
            {
                se.mob = egg.mobId;
                if (!player.IsCreative) held.count--;
                return true;
            }
            return false;
        }
    }

    public class BeaconBlock : Block
    {
        int glass, core, obs;
        public BeaconBlock()
        {
            opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Translucent; lightEmission = 15; hardness = 3f; creativeTab = CreativeTab.Functional;
            glass = Tex.Id("glass"); core = Tex.Id("beacon"); obs = Tex.Id("obsidian"); SetAllTex(glass);
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            ctx.Box(2f / 16, 0.1f / 16, 2f / 16, 14f / 16, 3f / 16, 14f / 16, new[] { obs, obs, obs, obs, obs, obs }, 0xFFFFFFFFu, 0, false, RenderLayer.Opaque);
            ctx.Box(3f / 16, 3f / 16, 3f / 16, 13f / 16, 14f / 16, 13f / 16, new[] { core, core, core, core, core, core }, 0xFFFFFFFFu, 0, false, RenderLayer.Opaque);
            ctx.Cube(new[] { glass, glass, glass, glass, glass, glass }, 0xFFFFFFFFu, RenderLayer.Translucent);
        }
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new BeaconEntity();
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (w.GetBlockEntity(pos) is BeaconEntity be) player.OpenMenu(new BeaconMenu(player, be));
            return true;
        }
    }

    public class RespawnAnchorBlock : Block
    {
        int[] tops = new int[5]; int side, bottom;
        public RespawnAnchorBlock()
        {
            stateCount = 5; hardness = 50f; blastResistance = 1200; tool = ToolType.Pickaxe; toolTier = Tier.Diamond; requiresTool = true; creativeTab = CreativeTab.Functional;
            for (int i = 0; i < 5; i++) tops[i] = Tex.Id("respawn_anchor_top_" + i);
            side = Tex.Id("respawn_anchor_side"); bottom = Tex.Id("respawn_anchor_bottom"); SetTex(tops[0], bottom, side);
        }
        public override byte GetLightEmission(int meta) => (byte)(meta == 0 ? 0 : 3 + meta * 3);
        readonly int[] t = new int[6];
        public override void Emit(MeshCtx ctx, int meta)
        {
            t[0] = bottom; t[1] = tops[meta]; for (int i = 2; i < 6; i++) t[i] = side;
            ctx.Cube(t, TintType.None);
        }
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            var held = player.MainHand;
            if (held != null && held.item.id == "glowstone" && meta < 4)
            {
                w.SetState(pos, State(meta + 1));
                if (!player.IsCreative) held.count--;
                Sounds.Play("block.respawn_anchor.charge", pos.Center, 1f, 1f);
                return true;
            }
            if (meta == 0) return false;
            if (w.dim != DimensionId.Nether)
            {
                w.SetState(pos, 0);
                Explosion.Explode(w, null, pos.Center, 5f, true, true);
                return true;
            }
            player.SetSpawnPoint(w.dim, pos, true);
            Sounds.Play("block.respawn_anchor.set_spawn", pos.Center, 1f, 1f);
            GameManager.Instance?.hud?.ShowActionBar("Respawn point set");
            return true;
        }
    }

    public class BellBlock : Block
    {
        int bell, stone, wood;
        public BellBlock()
        {
            stateCount = 4; opaqueCube = false; lightOpacity = 0; hardness = 5f; tool = ToolType.Pickaxe; requiresTool = true; sound = SoundType.Metal;
            bell = Tex.Id("bell_body"); stone = Tex.Id("stone"); wood = Tex.Id("dark_oak_planks"); SetAllTex(bell); creativeTab = CreativeTab.Functional; isFullCubeShape = false;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            ctx.rot = meta & 3;
            var b = new[] { bell, bell, bell, bell, bell, bell };
            ctx.BoxPx(5, 6, 5, 11, 13, 11, bell, 0xFFFFFFFFu, 0, false);
            ctx.BoxPx(4, 4, 4, 12, 6, 12, bell, 0xFFFFFFFFu, 0, false);
            ctx.BoxPx(0, 13, 7, 16, 15, 9, wood, 0xFFFFFFFFu);
            ctx.BoxPx(0, 0, 6, 2, 16, 10, stone, 0xFFFFFFFFu); ctx.BoxPx(14, 0, 6, 16, 16, 10, stone, 0xFFFFFFFFu);
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Rot(BoxUtil.Px(0, 0, 4, 16, 16, 12), meta & 3));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(ctx.playerFacing));
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit) { Ring(w, pos); return true; }
        public void Ring(World w, Int3 pos)
        {
            Sounds.Play("block.bell.use", pos.Center, 2f, 1f);
            foreach (var e in w.GetEntities(new AABB(pos.ToVector3() - Vector3.one * 32, pos.ToVector3() + Vector3.one * 32)))
                if (e is Mob m && MobRegistry.IsRaider(m.def.id)) m.glowing = true;
        }
        public override void OnProjectileHit(World w, Int3 pos, int meta, Entity projectile) => Ring(w, pos);
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos) { if (Redstone.IsPowered(w, pos)) Ring(w, pos); }
    }

    public class FlowerPotBlock : Block
    {
        public FlowerPotBlock() { stateCount = 1; opaqueCube = false; lightOpacity = 0; hardness = 0; SetAllTex(Tex.Id("flower_pot")); creativeTab = CreativeTab.Functional; isFullCubeShape = false; push = PushReaction.Destroy; }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            int t = faceTex[0]; int dirt = Tex.Id("dirt");
            ctx.BoxPx(5, 0, 5, 11, 6, 6, t, 0xFFFFFFFFu, 0, false); ctx.BoxPx(5, 0, 10, 11, 6, 11, t, 0xFFFFFFFFu, 0, false);
            ctx.BoxPx(5, 0, 6, 6, 6, 10, t, 0xFFFFFFFFu, 0, false); ctx.BoxPx(10, 0, 6, 11, 6, 10, t, 0xFFFFFFFFu, 0, false);
            ctx.BoxPx(6, 0, 6, 10, 4, 10, dirt, 0xFFFFFFFFu, 0, false);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(5, 0, 5, 11, 6, 11));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(5, 0, 5, 11, 6, 11));
    }

    /// <summary>End rod / lightning rod: facing(0-5).</summary>
    public class EndRodBlock : Block
    {
        public bool copperRod;
        public EndRodBlock()
        {
            stateCount = 6; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 0; lightEmission = 14; SetAllTex(Tex.Id("end_rod"));
            creativeTab = CreativeTab.Functional; isFullCubeShape = false;
        }
        public override byte GetLightEmission(int meta) => copperRod ? (byte)0 : (byte)14;
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            Dir f = (Dir)meta;
            int t = copperRod ? Tex.Id("lightning_rod") : faceTex[0];
            Matrix4x4 m = Matrix4x4.identity; Vector3 c = new Vector3(0.5f, 0.5f, 0.5f);
            if (f == Dir.Down) m = Matrix4x4.Translate(c) * Matrix4x4.Rotate(Quaternion.Euler(180, 0, 0)) * Matrix4x4.Translate(-c);
            else if (DirUtil.IsHorizontal(f)) m = Matrix4x4.Translate(c) * Matrix4x4.Rotate(Quaternion.Euler(0, DirUtil.ToYaw(f), 0) * Quaternion.Euler(90, 0, 0)) * Matrix4x4.Translate(-c);
            var uvRod = new Vector4[6]; for (int i = 2; i < 6; i++) uvRod[i] = new Vector4(0, 0, 2 / 16f, 15 / 16f); uvRod[0] = uvRod[1] = new Vector4(2 / 16f, 0, 4 / 16f, 2 / 16f);
            var uvBase = new Vector4[6]; for (int i = 2; i < 6; i++) uvBase[i] = new Vector4(2 / 16f, 5 / 16f, 6 / 16f, 6 / 16f); uvBase[0] = uvBase[1] = new Vector4(2 / 16f, 2 / 16f, 6 / 16f, 6 / 16f);
            ctx.XBoxPx(m, 7, 1, 7, 9, 16, 9, t, 0xFFFFFFFFu, RenderLayer.Cutout, uvRod);
            ctx.XBoxPx(m, 6, 0, 6, 10, 1, 10, t, 0xFFFFFFFFu, RenderLayer.Cutout, uvBase);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            Dir f = (Dir)meta; int a = DirUtil.Axis(f);
            if (a == 1) boxes.Add(BoxUtil.Px(6, 0, 6, 10, 16, 10)); else if (a == 0) boxes.Add(BoxUtil.Px(0, 6, 6, 16, 10, 10)); else boxes.Add(BoxUtil.Px(6, 6, 0, 10, 10, 16));
        }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
        public override ushort GetPlacementState(ref PlaceContext ctx) => State((int)ctx.clickedFace);
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (!copperRod && rng.Chance(0.2f)) Particles.EndRod(w, pos.Center + DirUtil.Normal[meta] * 0.45f);
        }
    }

    public class PowderSnowBlock : Block
    {
        public PowderSnowBlock() { opaqueCube = true; solid = false; this.T1("powder_snow"); hardness = 0.25f; sound = SoundType.Powder; creativeTab = CreativeTab.Natural; noItem = true; }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override Vector3 StuckSpeed(int meta) => new Vector3(0.9f, 1.5f, 0.9f);
        public override void OnEntityInside(World w, Int3 pos, int meta, Entity e)
        {
            if (e is LivingEntity le && !(le is Player p && p.IsCreative) && le.GetArmor(0)?.item.id != "leather_boots")
            {
                if (w.tickCount % 40 == 0) le.Hurt(DamageSource.Freeze, 1f);
            }
            if (e.onFire) { e.Extinguish(); w.SetState(pos, 0); }
        }
    }

    // ============================================================================ anvil / grindstone / stonecutter / loom / lectern / enchanting / brewing
    public class AnvilBlock : Block
    {
        public int damage;
        int top, body;
        public AnvilBlock(int dmg)
        {
            damage = dmg; stateCount = 4; opaqueCube = false; lightOpacity = 0; hardness = 5f; blastResistance = 1200; tool = ToolType.Pickaxe; requiresTool = true;
            sound = SoundType.Anvil; top = Tex.Id(dmg == 0 ? "anvil_top" : dmg == 1 ? "chipped_anvil_top" : "damaged_anvil_top"); body = Tex.Id("anvil");
            SetAllTex(body); gravity = true; creativeTab = CreativeTab.Functional; isFullCubeShape = false;
        }
        public Block Damaged() => damage == 0 ? Blocks.Get("chipped_anvil") : damage == 1 ? Blocks.Get("damaged_anvil") : null;
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            ctx.rot = meta & 3;
            var b = new[] { body, body, body, body, body, body };
            ctx.BoxPx(2, 0, 2, 14, 4, 14, body, 0xFFFFFFFFu);
            ctx.BoxPx(4, 4, 3, 12, 5, 13, body, 0xFFFFFFFFu, 0, false);
            ctx.BoxPx(6, 5, 4, 10, 10, 12, body, 0xFFFFFFFFu, 0, false);
            ctx.Box(3f / 16, 10f / 16, 0, 13f / 16, 1, 1, new[] { body, top, body, body, body, body }, 0xFFFFFFFFu);
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Rot(BoxUtil.Px(3, 0, 0, 13, 16, 16), meta & 3));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(DirUtil.RotateCW(ctx.playerFacing)));
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit) { player.OpenMenu(new AnvilMenu(player, pos)); return true; }
        public override void OnAdded(World w, Int3 pos, int meta, ushort oldState) => w.ScheduleTick(pos, this, 2);
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos) => w.ScheduleTick(pos, this, 2);
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            if (FallingBlockEntity.CanFallThrough(w, pos.Offset(Dir.Down))) FallingBlockEntity.Spawn(w, pos, w.GetState(pos));
        }
    }

    public class GrindstoneBlock : Block
    {
        int side, pivot, round;
        public GrindstoneBlock()
        {
            stateCount = 4; opaqueCube = false; lightOpacity = 0; side = Tex.Id("grindstone_side"); pivot = Tex.Id("grindstone_pivot"); round = Tex.Id("grindstone_round");
            SetAllTex(side); isFullCubeShape = false;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            ctx.rot = meta & 3;
            ctx.Box(4f / 16, 4f / 16, 2f / 16, 12f / 16, 16f / 16, 14f / 16, new[] { round, round, side, side, side, side }, 0xFFFFFFFFu);
            ctx.BoxPx(2, 7, 5, 4, 13, 11, pivot, 0xFFFFFFFFu); ctx.BoxPx(12, 7, 5, 14, 13, 11, pivot, 0xFFFFFFFFu);
            int legs = Tex.Id("dark_oak_log");
            ctx.BoxPx(2, 0, 6, 4, 7, 10, legs, 0xFFFFFFFFu); ctx.BoxPx(12, 0, 6, 14, 7, 10, legs, 0xFFFFFFFFu);
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Rot(BoxUtil.Px(2, 0, 2, 14, 16, 14), meta & 3));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(ctx.playerFacing));
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit) { player.OpenMenu(new GrindstoneMenu(player, pos)); return true; }
    }

    public class StonecutterBlock : Block
    {
        int top, side, bottom, saw;
        public StonecutterBlock()
        {
            stateCount = 4; opaqueCube = false; lightOpacity = 0; top = Tex.Id("stonecutter_top"); side = Tex.Id("stonecutter_side"); bottom = Tex.Id("stonecutter_bottom"); saw = Tex.Id("stonecutter_saw");
            SetTex(top, bottom, side); isFullCubeShape = false;
        }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public override void Emit(MeshCtx ctx, int meta)
        {
            ctx.rot = meta & 3;
            ctx.Box(0, 0, 0, 1, 9f / 16, 1, new[] { bottom, top, side, side, side, side }, 0xFFFFFFFFu);
            ctx.Quad(new Vector3(0.1f, 9f / 16, 0.5f), new Vector3(0.1f, 1, 0.5f), new Vector3(0.9f, 1, 0.5f), new Vector3(0.9f, 9f / 16, 0.5f), new Vector2(0.1f, 0), new Vector2(0.1f, 7 / 16f), new Vector2(0.9f, 7 / 16f), new Vector2(0.9f, 0), saw, 0xFFFFFFFFu, 0.9f, RenderLayer.Cutout, true);
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 9, 16));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 9, 16));
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(ctx.playerFacing));
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit) { player.OpenMenu(new StonecutterMenu(player, pos)); return true; }
    }

    public class LoomBlock : HorizontalBlock
    {
        public LoomBlock() : base("loom_top", "loom_side", "loom_front") { }
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(DirUtil.Opposite(ctx.playerFacing)));
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit) { player.OpenMenu(new LoomMenu(player, pos)); return true; }
    }

    public class LecternBlock : Block
    {
        int top, side, baseT, front;
        public LecternBlock()
        {
            stateCount = 8; opaqueCube = false; lightOpacity = 0; hardness = 2.5f; tool = ToolType.Axe; sound = SoundType.Wood; creativeTab = CreativeTab.Functional;
            top = Tex.Id("lectern_top"); side = Tex.Id("lectern_sides"); baseT = Tex.Id("lectern_base"); front = Tex.Id("lectern_front"); SetAllTex(side); isFullCubeShape = false;
        }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public override void Emit(MeshCtx ctx, int meta)
        {
            ctx.rot = meta & 3;
            ctx.Box(0, 0, 0, 1, 2f / 16, 1, new[] { baseT, baseT, baseT, baseT, baseT, baseT }, 0xFFFFFFFFu);
            ctx.Box(4f / 16, 2f / 16, 4f / 16, 12f / 16, 13f / 16, 12f / 16, new[] { side, side, front, front, side, side }, 0xFFFFFFFFu, 0, false);
            Matrix4x4 m = Matrix4x4.Translate(new Vector3(0.5f, 13f / 16, 0.5f)) * Matrix4x4.Rotate(Quaternion.Euler(-22.5f, 0, 0)) * Matrix4x4.Translate(new Vector3(-0.5f, -13f / 16, -0.5f));
            ctx.XBoxPx(m, 0, 12, 1, 16, 16, 15, top, 0xFFFFFFFFu, RenderLayer.Opaque);
            if ((meta & 4) != 0) ctx.XBoxPx(m, 3, 16, 3, 13, 17, 13, Tex.Id("lectern_book"), 0xFFFFFFFFu, RenderLayer.Opaque);
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { boxes.Add(BoxUtil.Px(0, 0, 0, 16, 2, 16)); boxes.Add(BoxUtil.Px(4, 2, 4, 12, 14, 12)); }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 15, 16));
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(DirUtil.Opposite(ctx.playerFacing)));
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new LecternEntity();
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (!(w.GetBlockEntity(pos) is LecternEntity le)) return false;
            var held = player.MainHand;
            if (le.book == null && held != null && (held.item.id == "writable_book" || held.item.id == "written_book" || held.item.id == "book"))
            {
                le.book = held.CopyWithCount(1); if (!player.IsCreative) held.count--;
                w.SetState(pos, State(meta | 4), 0);
                Sounds.Play("item.book.put", pos.Center, 1f, 1f);
                return true;
            }
            if (le.book != null && player.sneaking) { le.DropContents(); w.SetState(pos, State(meta & 3), 0); return true; }
            return le.book != null;
        }
    }

    public class EnchantingTableBlock : Block
    {
        public EnchantingTableBlock() { opaqueCube = false; lightOpacity = 0; this.T3("enchanting_table_top", "enchanting_table_bottom", "enchanting_table_side"); isFullCubeShape = false; }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public override void Emit(MeshCtx ctx, int meta) => ctx.Box(0, 0, 0, 1, 12f / 16, 1, faceTex, 0xFFFFFFFFu);
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 12, 16));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 12, 16));
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit) { player.OpenMenu(new EnchantMenu(player, pos)); return true; }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            for (int dx = -2; dx <= 2; dx++) for (int dz = -2; dz <= 2; dz++)
                {
                    if (Mathf.Abs(dx) < 2 && Mathf.Abs(dz) < 2) continue;
                    if (rng.Chance(0.06f) && w.GetBlock(new Int3(pos.x + dx, pos.y, pos.z + dz)).id == "bookshelf")
                        Particles.Enchant(w, new Vector3(pos.x + dx + 0.5f, pos.y + 1.5f, pos.z + dz + 0.5f), pos.Center + Vector3.up * 0.8f);
                }
        }
        public override bool HasBlockEntity => false;
    }

    public class BrewingStandBlock : Block
    {
        int baseT, rod;
        public BrewingStandBlock()
        {
            stateCount = 8; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 0.5f; tool = ToolType.Pickaxe; requiresTool = true; lightEmission = 1;
            baseT = Tex.Id("brewing_stand_base"); rod = Tex.Id("brewing_stand"); SetAllTex(rod); creativeTab = CreativeTab.Functional; isFullCubeShape = false;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            var uv = new Vector4[6]; for (int i = 2; i < 6; i++) uv[i] = new Vector4(7 / 16f, 2 / 16f, 9 / 16f, 16 / 16f); uv[0] = uv[1] = new Vector4(7 / 16f, 14 / 16f, 9 / 16f, 16 / 16f);
            ctx.XBoxPx(Matrix4x4.identity, 7, 0, 7, 9, 14, 9, rod, 0xFFFFFFFFu, RenderLayer.Cutout, uv);
            ctx.BoxPx(9, 0, 5, 15, 2, 11, baseT, 0xFFFFFFFFu, 0, false);
            ctx.BoxPx(2, 0, 1, 8, 2, 7, baseT, 0xFFFFFFFFu, 0, false);
            ctx.BoxPx(2, 0, 9, 8, 2, 15, baseT, 0xFFFFFFFFu, 0, false);
            // bottle arms (decal planes)
            ctx.Quad(new Vector3(0.5f, 0, 0.5f), new Vector3(0.5f, 1, 0.5f), new Vector3(1, 1, 0.5f), new Vector3(1, 0, 0.5f), new Vector2(0.5f, 0), new Vector2(0.5f, 1), new Vector2(1, 1), new Vector2(1, 0), rod, 0xFFFFFFFFu, 0.9f, RenderLayer.Cutout, true);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { boxes.Add(BoxUtil.Px(1, 0, 1, 15, 2, 15)); boxes.Add(BoxUtil.Px(7, 0, 7, 9, 14, 9)); }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(1, 0, 1, 15, 14, 15));
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new BrewingStandEntity();
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (w.GetBlockEntity(pos) is BrewingStandEntity be) player.OpenMenu(new BrewingMenu(player, be));
            return true;
        }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (w.GetBlockEntity(pos) is BrewingStandEntity be && be.brewTime > 0 && rng.Chance(0.3f)) Particles.Smoke(w, pos.Center + new Vector3(rng.Range(-0.2f, 0.2f), 0.2f, rng.Range(-0.2f, 0.2f)), 1, 0.2f);
        }
    }

    /// <summary>Cauldron with water level 0..3 (meta 0-3), lava (4), powder snow (5-7).</summary>
    public class CauldronBlock : Block
    {
        int side, top, inner, bottom, water, lava;
        public CauldronBlock()
        {
            stateCount = 8; opaqueCube = false; lightOpacity = 0; hardness = 2f; tool = ToolType.Pickaxe; requiresTool = true; creativeTab = CreativeTab.Functional; isFullCubeShape = false;
            side = Tex.Id("cauldron_side"); top = Tex.Id("cauldron_top"); inner = Tex.Id("cauldron_inner"); bottom = Tex.Id("cauldron_bottom"); water = Tex.Id("water_still"); lava = Tex.Id("lava_still");
            SetAllTex(side);
        }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public override byte GetLightEmission(int meta) => (byte)(meta == 4 ? 15 : 0);
        public override void Emit(MeshCtx ctx, int meta)
        {
            var t = new[] { bottom, top, side, side, side, side };
            ctx.Box(0, 3f / 16, 0, 1, 1, 2f / 16, t, 0xFFFFFFFFu); ctx.Box(0, 3f / 16, 14f / 16, 1, 1, 1, t, 0xFFFFFFFFu);
            ctx.Box(0, 3f / 16, 2f / 16, 2f / 16, 1, 14f / 16, t, 0xFFFFFFFFu); ctx.Box(14f / 16, 3f / 16, 2f / 16, 1, 1, 14f / 16, t, 0xFFFFFFFFu);
            ctx.Box(2f / 16, 3f / 16, 2f / 16, 14f / 16, 4f / 16, 14f / 16, new[] { bottom, inner, inner, inner, inner, inner }, 0xFFFFFFFFu, 0, false);
            ctx.BoxPx(0, 0, 0, 4, 3, 2, side, 0xFFFFFFFFu); ctx.BoxPx(12, 0, 0, 16, 3, 2, side, 0xFFFFFFFFu); ctx.BoxPx(0, 0, 14, 4, 3, 16, side, 0xFFFFFFFFu); ctx.BoxPx(12, 0, 14, 16, 3, 16, side, 0xFFFFFFFFu);
            if (meta > 0)
            {
                float h = meta == 4 ? 15f / 16 : (meta >= 5 ? (5 + (meta - 4) * 3) / 16f : (6 + meta * 3) / 16f);
                int tx = meta == 4 ? lava : (meta >= 5 ? Tex.Id("powder_snow") : water);
                uint col = meta <= 3 ? ctx.TintColor(TintType.Water) : 0xFFFFFFFFu;
                ctx.Quad(new Vector3(2 / 16f, h, 2 / 16f), new Vector3(2 / 16f, h, 14 / 16f), new Vector3(14 / 16f, h, 14 / 16f), new Vector3(14 / 16f, h, 2 / 16f), new Vector2(0, 0), new Vector2(0, 1), new Vector2(1, 1), new Vector2(1, 0), tx, col, 1f, meta <= 3 ? RenderLayer.Translucent : RenderLayer.Opaque);
            }
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            boxes.Add(BoxUtil.Px(0, 0, 0, 16, 4, 16)); boxes.Add(BoxUtil.Px(0, 0, 0, 16, 16, 2)); boxes.Add(BoxUtil.Px(0, 0, 14, 16, 16, 16)); boxes.Add(BoxUtil.Px(0, 0, 0, 2, 16, 16)); boxes.Add(BoxUtil.Px(14, 0, 0, 16, 16, 16));
        }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(new AABB(0, 0, 0, 1, 1, 1));
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            var held = player.MainHand;
            if (held == null) return false;
            string id = held.item.id;
            if (id == "water_bucket" && meta != 3) { w.SetState(pos, State(3)); if (!player.IsCreative) player.inventory.SetSelected(new ItemStack("bucket", 1)); Sounds.Play("item.bucket.empty", pos.Center, 1f, 1f); return true; }
            if (id == "lava_bucket" && meta == 0) { w.SetState(pos, State(4)); if (!player.IsCreative) player.inventory.SetSelected(new ItemStack("bucket", 1)); Sounds.Play("item.bucket.empty_lava", pos.Center, 1f, 1f); return true; }
            if (id == "bucket" && (meta == 3 || meta == 4)) { w.SetState(pos, State(0)); if (!player.IsCreative) { held.count--; player.inventory.AddOrDrop(new ItemStack(meta == 3 ? "water_bucket" : "lava_bucket", 1)); } Sounds.Play("item.bucket.fill", pos.Center, 1f, 1f); return true; }
            if (id == "glass_bottle" && meta >= 1 && meta <= 3) { w.SetState(pos, State(meta - 1)); if (!player.IsCreative) { held.count--; var pot = new ItemStack("potion", 1); pot.Set("potion", "water"); player.inventory.AddOrDrop(pot); } Sounds.Play("item.bottle.fill", pos.Center, 1f, 1f); return true; }
            if (id == "potion" && held.Get("potion") == "water" && meta < 3) { w.SetState(pos, State(meta + 1)); if (!player.IsCreative) player.inventory.SetSelected(new ItemStack("glass_bottle", 1)); Sounds.Play("item.bottle.empty", pos.Center, 1f, 1f); return true; }
            if (held.item is ArmorItem ai && ai.material == ArmorMaterial.Leather && held.data != null && held.data.ContainsKey("color") && meta >= 1 && meta <= 3) { held.Set("color", null); w.SetState(pos, State(meta - 1)); return true; }
            return false;
        }
        public override void OnEntityInside(World w, Int3 pos, int meta, Entity e)
        {
            if (meta == 4 && e is LivingEntity le) { le.SetOnFire(15); le.Hurt(DamageSource.Lava, 4f); }
            else if (meta >= 1 && meta <= 3 && e.onFire) { e.Extinguish(); w.SetState(pos, State(meta - 1)); }
        }
        public override int GetComparatorOutput(World w, Int3 pos, int meta) => meta <= 3 ? meta : 3;
    }

    public class ComposterBlock : Block
    {
        int side, top, bottom, compost, ready;
        public ComposterBlock()
        {
            stateCount = 9; opaqueCube = false; lightOpacity = 0; hardness = 0.6f; tool = ToolType.Axe; sound = SoundType.Wood; creativeTab = CreativeTab.Functional; isFullCubeShape = false;
            side = Tex.Id("composter_side"); top = Tex.Id("composter_top"); bottom = Tex.Id("composter_bottom"); compost = Tex.Id("composter_compost"); ready = Tex.Id("composter_ready"); SetAllTex(side);
        }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public override void Emit(MeshCtx ctx, int meta)
        {
            var t = new[] { bottom, top, side, side, side, side };
            ctx.Box(0, 0, 0, 1, 2f / 16, 1, t, 0xFFFFFFFFu);
            ctx.Box(0, 0, 0, 1, 1, 2f / 16, t, 0xFFFFFFFFu); ctx.Box(0, 0, 14f / 16, 1, 1, 1, t, 0xFFFFFFFFu);
            ctx.Box(0, 0, 2f / 16, 2f / 16, 1, 14f / 16, t, 0xFFFFFFFFu); ctx.Box(14f / 16, 0, 2f / 16, 1, 1, 14f / 16, t, 0xFFFFFFFFu);
            if (meta > 0)
            {
                float h = meta >= 8 ? 15f / 16 : (2 + meta * 2) / 16f;
                int tx = meta >= 8 ? ready : compost;
                ctx.Quad(new Vector3(2 / 16f, h, 2 / 16f), new Vector3(2 / 16f, h, 14 / 16f), new Vector3(14 / 16f, h, 14 / 16f), new Vector3(14 / 16f, h, 2 / 16f), new Vector2(0, 0), new Vector2(0, 1), new Vector2(1, 1), new Vector2(1, 0), tx, 0xFFFFFFFFu, 1f, RenderLayer.Opaque);
            }
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { boxes.Add(BoxUtil.Px(0, 0, 0, 16, 2, 16)); boxes.Add(BoxUtil.Px(0, 0, 0, 16, 16, 2)); boxes.Add(BoxUtil.Px(0, 0, 14, 16, 16, 16)); boxes.Add(BoxUtil.Px(0, 0, 0, 2, 16, 16)); boxes.Add(BoxUtil.Px(14, 0, 0, 16, 16, 16)); }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(new AABB(0, 0, 0, 1, 1, 1));
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (meta >= 8) { w.SpawnItem(pos.Center + Vector3.up * 0.6f, new ItemStack("bone_meal", 1)); w.SetState(pos, State(0)); Sounds.Play("block.composter.empty", pos.Center, 1f, 1f); return true; }
            var held = player.MainHand;
            if (held == null) return false;
            float chance = Composting.Chance(held.item);
            if (chance <= 0 || meta >= 7) return false;
            if (!player.IsCreative) held.count--;
            if (Random.value < chance) { w.SetState(pos, State(meta + 1)); Sounds.Play("block.composter.fill_success", pos.Center, 1f, 1f); if (meta + 1 == 7) w.ScheduleTick(pos, this, 20); }
            else Sounds.Play("block.composter.fill", pos.Center, 1f, 1f);
            Particles.HappyVillager(w, pos.Center + Vector3.up * 0.5f, 3);
            return true;
        }
        public override void OnScheduledTick(World w, Int3 pos, int meta) { if (meta == 7) { w.SetState(pos, State(8)); Sounds.Play("block.composter.ready", pos.Center, 1f, 1f); } }
        public override int GetComparatorOutput(World w, Int3 pos, int meta) => meta;
    }

    public class ShelfBlock : Block
    {
        public ShelfBlock(string wood)
        {
            stateCount = 4; opaqueCube = false; lightOpacity = 0; hardness = 2f; tool = ToolType.Axe; sound = SoundType.Wood; isFullCubeShape = false;
            SetAllTex(Tex.Id(wood == "bamboo" ? "bamboo_planks" : (wood == "crimson" || wood == "warped" ? wood + "_planks" : wood + "_planks")));
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            ctx.rot = meta & 3;
            int t = faceTex[0];
            ctx.BoxPx(0, 0, 0, 16, 16, 3, t, 0xFFFFFFFFu);
            ctx.BoxPx(0, 0, 3, 16, 2, 16, t, 0xFFFFFFFFu, 0, false);
            ctx.BoxPx(0, 14, 3, 16, 16, 16, t, 0xFFFFFFFFu, 0, false);
            ctx.BoxPx(0, 2, 3, 2, 14, 16, t, 0xFFFFFFFFu, 0, false); ctx.BoxPx(14, 2, 3, 16, 14, 16, t, 0xFFFFFFFFu, 0, false);
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(new AABB(0, 0, 0, 1, 1, 1));
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(ctx.playerFacing));
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new ShelfEntity();
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (!(w.GetBlockEntity(pos) is ShelfEntity se)) return false;
            // pick slot by horizontal hit offset
            Vector3 local = hit - pos.ToVector3();
            Dir f = StateBits.FacingDir(meta);
            Vector3 right = DirUtil.Normal[(int)DirUtil.RotateCW(f)];
            float along = Vector3.Dot(local - new Vector3(0.5f, 0, 0.5f), right) + 0.5f;
            int slot = Mathf.Clamp((int)(along * 3), 0, 2);
            var held = player.MainHand;
            var cur = se.items[slot];
            se.items[slot] = held != null ? held.Copy() : null;
            player.inventory.SetSelected(cur);
            se.MarkDirty();
            Sounds.Play("block.chiseled_bookshelf.insert", pos.Center, 1f, 1f);
            return true;
        }
    }

    // ============================================================================ hopper & dispenser
    /// <summary>meta: facing(0-4: down, N, S, W, E encoded as Dir) | disabled(3)</summary>
    public class HopperBlock : Block
    {
        int outside, inside, top;
        public HopperBlock()
        {
            stateCount = 16; opaqueCube = false; lightOpacity = 0; hardness = 3f; tool = ToolType.Pickaxe; requiresTool = true; sound = SoundType.Metal; creativeTab = CreativeTab.Redstone; isFullCubeShape = false;
            outside = Tex.Id("hopper_outside"); inside = Tex.Id("hopper_inside"); top = Tex.Id("hopper_top"); SetAllTex(outside);
        }
        public static Dir Facing(int m) { var d = (Dir)(m & 7); return d == Dir.Up ? Dir.Down : d; }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            var t = new[] { outside, top, outside, outside, outside, outside };
            ctx.Box(0, 10f / 16, 0, 1, 11f / 16, 1, new[] { outside, inside, outside, outside, outside, outside }, 0xFFFFFFFFu, 0, false);
            ctx.Box(0, 11f / 16, 0, 1, 1, 2f / 16, t, 0xFFFFFFFFu); ctx.Box(0, 11f / 16, 14f / 16, 1, 1, 1, t, 0xFFFFFFFFu);
            ctx.Box(0, 11f / 16, 2f / 16, 2f / 16, 1, 14f / 16, t, 0xFFFFFFFFu); ctx.Box(14f / 16, 11f / 16, 2f / 16, 1, 1, 14f / 16, t, 0xFFFFFFFFu);
            ctx.BoxPx(4, 4, 4, 12, 10, 12, outside, 0xFFFFFFFFu, 0, false);
            Dir f = Facing(meta);
            if (f == Dir.Down) ctx.BoxPx(6, 0, 6, 10, 4, 10, outside, 0xFFFFFFFFu);
            else
            {
                Vector3 n = DirUtil.Normal[(int)f];
                float x0 = 6, x1 = 10, z0 = 6, z1 = 10;
                if (n.x > 0) { x0 = 12; x1 = 16; } else if (n.x < 0) { x0 = 0; x1 = 4; }
                if (n.z > 0) { z0 = 12; z1 = 16; } else if (n.z < 0) { z0 = 0; z1 = 4; }
                ctx.BoxPx(x0, 4, z0, x1, 8, z1, outside, 0xFFFFFFFFu);
            }
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            boxes.Add(BoxUtil.Px(0, 10, 0, 16, 11, 16)); boxes.Add(BoxUtil.Px(0, 11, 0, 16, 16, 2)); boxes.Add(BoxUtil.Px(0, 11, 14, 16, 16, 16));
            boxes.Add(BoxUtil.Px(0, 11, 0, 2, 16, 16)); boxes.Add(BoxUtil.Px(14, 11, 0, 16, 16, 16)); boxes.Add(BoxUtil.Px(4, 4, 4, 12, 10, 12));
        }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(new AABB(0, 0, 0, 1, 1, 1));
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            Dir f = ctx.clickedFace == Dir.Up || ctx.clickedFace == Dir.Down ? Dir.Down : DirUtil.Opposite(ctx.clickedFace);
            return State((int)f);
        }
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new HopperEntity();
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (w.GetBlockEntity(pos) is HopperEntity he) player.OpenMenu(new HopperMenu(player, he));
            return true;
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            bool powered = Redstone.IsPowered(w, pos);
            bool disabled = (meta & 8) != 0;
            if (powered != disabled) w.SetState(pos, State(powered ? meta | 8 : meta & 7), 0);
        }
        public override int GetComparatorOutput(World w, Int3 pos, int meta) => w.GetBlockEntity(pos) is HopperEntity he ? he.ComparatorSignal() : 0;

        public static IContainer ContainerAt(World w, Int3 p)
        {
            var be = w.GetBlockEntity(p);
            var b = w.GetBlock(p);
            if (b is ChestBlock cb && be is ChestEntity ce)
            {
                if (b is EnderChestBlock) return null;
                var pp = cb.Partner(w, p, w.GetMeta(p));
                if (pp.HasValue && w.GetBlockEntity(pp.Value) is ChestEntity ce2) return ChestBlock.Type(w.GetMeta(p)) == 1 ? new DoubleChestContainer(ce, ce2) : new DoubleChestContainer(ce2, ce);
                return ce;
            }
            if (be is ContainerEntity c) return c;
            return null;
        }
        public static bool Insert(IContainer c, ItemStack one, Dir fromSide)
        {
            if (c is FurnaceEntity fe)
            {
                int slot = fromSide == Dir.Up ? 0 : 1;
                if (slot == 1 && Recipes.FuelValue(one.item) <= 0) return false;
                var cur = fe.items[slot];
                if (cur == null) { fe.items[slot] = one; fe.MarkDirty(); return true; }
                if (cur.Stackable(one) && cur.count < cur.MaxStack) { cur.count++; fe.MarkDirty(); return true; }
                return false;
            }
            for (int i = 0; i < c.Size; i++)
            {
                var cur = c.Get(i);
                if (cur != null && cur.Stackable(one) && cur.count < Mathf.Min(cur.MaxStack, c.MaxStackSize)) { cur.count++; c.SetChanged(); return true; }
            }
            for (int i = 0; i < c.Size; i++)
                if (c.Get(i) == null) { c.Set(i, one); return true; }
            return false;
        }
    }

    /// <summary>meta: facing(0-5) | triggered(3)</summary>
    public class DispenserBlock : Block
    {
        public bool dropper;
        int front, frontV, side, top;
        public DispenserBlock(bool dropper)
        {
            this.dropper = dropper; stateCount = 16; hardness = 3.5f; tool = ToolType.Pickaxe; requiresTool = true; creativeTab = CreativeTab.Redstone;
            string n = dropper ? "dropper" : "dispenser";
            front = Tex.Id(n + "_front"); frontV = Tex.Id(n + "_front_vertical"); side = Tex.Id("furnace_side"); top = Tex.Id("furnace_top"); SetTex(top, top, side); particleTex = front;
        }
        readonly int[] t = new int[6], r = new int[6];
        public override void Emit(MeshCtx ctx, int meta)
        {
            Dir f = (Dir)(meta & 7);
            for (int i = 0; i < 6; i++) { t[i] = DirUtil.IsHorizontal((Dir)i) ? side : top; r[i] = 0; }
            if (!DirUtil.IsHorizontal(f)) { for (int i = 2; i < 6; i++) t[i] = side; t[(int)f] = frontV; t[(int)DirUtil.Opposite(f)] = top; }
            else t[(int)f] = front;
            ctx.CubeRot(t, r, 0xFFFFFFFFu);
        }
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            Vector3 look = MathX.YawPitchToDir(ctx.playerYaw, ctx.playerPitch);
            return State((int)DirUtil.Opposite(DirUtil.FromVector(look)));
        }
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new DispenserEntity { dropper = dropper };
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (w.GetBlockEntity(pos) is DispenserEntity de) player.OpenMenu(new DispenserMenu(player, de, dropper ? "Dropper" : "Dispenser"));
            return true;
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            bool powered = Redstone.IsPowered(w, pos) || Redstone.IsPowered(w, pos.Offset(Dir.Up));
            bool trig = (meta & 8) != 0;
            if (powered && !trig) { w.SetState(pos, State(meta | 8), 0); w.ScheduleTick(pos, this, 4); }
            else if (!powered && trig) w.SetState(pos, State(meta & 7), 0);
        }
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            if (!(w.GetBlockEntity(pos) is DispenserEntity de)) return;
            var slots = new List<int>();
            for (int i = 0; i < 9; i++) if (de.items[i] != null) slots.Add(i);
            if (slots.Count == 0) { Sounds.Play("block.dispenser.fail", pos.Center, 1f, 1.2f); return; }
            int s = slots[w.rand.Next(slots.Count)];
            Dir f = (Dir)(meta & 7);
            var result = Dispensing.Dispense(w, pos, f, de.items[s], dropper);
            de.items[s] = result != null && !result.IsEmpty ? result : null;
            de.MarkDirty();
        }
        public override int GetComparatorOutput(World w, Int3 pos, int meta) => w.GetBlockEntity(pos) is DispenserEntity de ? de.ComparatorSignal() : 0;
    }

    // ============================================================================ rails
    public enum RailKind { Normal, Powered, Detector, Activator }
    /// <summary>meta: shape (0 NS,1 EW, 2 asc E, 3 asc W, 4 asc N, 5 asc S, 6 SE,7 SW,8 NW,9 NE) | powered(16)</summary>
    public class RailBlock : Block
    {
        public RailKind kind;
        int tex, texOn, texCorner;
        public RailBlock(RailKind k)
        {
            kind = k; stateCount = 32; opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 0.7f; sound = SoundType.Metal;
            string n = k == RailKind.Normal ? "rail" : k == RailKind.Powered ? "powered_rail" : k == RailKind.Detector ? "detector_rail" : "activator_rail";
            tex = Tex.Id(n); texOn = Tex.Id(k == RailKind.Normal ? "rail" : n + "_on"); texCorner = Tex.Id(k == RailKind.Normal ? "rail_corner" : n);
            SetAllTex(tex); creativeTab = CreativeTab.Redstone; isFullCubeShape = false; push = PushReaction.Normal;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public static int Shape(int m) => m & 15;
        public static bool Powered(int m) => (m & 16) != 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            int sh = Shape(meta);
            int t = Powered(meta) ? texOn : tex;
            if (sh >= 6 && sh <= 9 && kind == RailKind.Normal) { int rot = sh == 6 ? 0 : sh == 7 ? 1 : sh == 8 ? 2 : 3; ctx.FloorDecal(texCorner, 0xFFFFFFFFu, 1f / 16f, rot); return; }
            if (sh <= 1) { ctx.FloorDecal(t, 0xFFFFFFFFu, 1f / 16f, sh == 0 ? 0 : 1); return; }
            // ascending: tilted quad
            Vector3 a, b, c, d;
            float h0 = 1f / 16, h1 = 1f + 1f / 16;
            switch (sh)
            {
                case 2: a = new Vector3(0, h0, 0); b = new Vector3(0, h0, 1); c = new Vector3(1, h1, 1); d = new Vector3(1, h1, 0); break; // up toward east
                case 3: a = new Vector3(0, h1, 0); b = new Vector3(0, h1, 1); c = new Vector3(1, h0, 1); d = new Vector3(1, h0, 0); break; // up toward west
                case 4: a = new Vector3(0, h0, 0); b = new Vector3(0, h1, 1); c = new Vector3(1, h1, 1); d = new Vector3(1, h0, 0); break; // up toward north (+z)
                default: a = new Vector3(0, h1, 0); b = new Vector3(0, h0, 1); c = new Vector3(1, h0, 1); d = new Vector3(1, h1, 0); break;
            }
            bool alongX = sh == 2 || sh == 3;
            Vector2 u0 = new Vector2(0, 0), u1 = new Vector2(0, 1), u2 = new Vector2(1, 1), u3 = new Vector2(1, 0);
            if (alongX) { u0 = new Vector2(0, 1); u1 = new Vector2(1, 1); u2 = new Vector2(1, 0); u3 = new Vector2(0, 0); }
            ctx.Quad(a, b, c, d, u0, u1, u2, u3, t, 0xFFFFFFFFu, 1f, RenderLayer.Cutout, true);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(Shape(meta) >= 2 && Shape(meta) <= 5 ? BoxUtil.Px(0, 0, 0, 16, 8, 16) : BoxUtil.Px(0, 0, 0, 16, 2, 16));
        public override bool CanSurvive(World w, Int3 pos, int meta) => w.IsSturdy(pos.Offset(Dir.Down), Dir.Up) || w.GetBlock(pos.Offset(Dir.Down)).opaqueCube;
        public override bool CanPlaceAt(World w, Int3 pos, ushort state) => CanSurvive(w, pos, 0);
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(ctx.playerFacing == Dir.East || ctx.playerFacing == Dir.West ? 1 : 0);
        public override void OnPlaced(World w, Int3 pos, int meta, Entity placer, ItemStack stack)
        {
            Rails.UpdateShape(w, pos, true);
            foreach (var d in DirUtil.Horizontal) foreach (var dy in new[] { 0, 1, -1 }) { var n = pos.Offset(d).Offset(0, dy, 0); if (w.GetBlock(n) is RailBlock) Rails.UpdateShape(w, n, false); }
            OnNeighborChanged(w, pos, w.GetMeta(pos), pos);
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (!CanSurvive(w, pos, meta)) { w.BreakBlock(pos, true, null); return; }
            if (kind == RailKind.Powered || kind == RailKind.Activator)
            {
                bool p = Redstone.IsPowered(w, pos) || Rails.PoweredByNeighbourRail(w, pos, meta, this, 0);
                if (p != Powered(meta)) w.SetState(pos, State(p ? meta | 16 : meta & 15), SetFlags.Notify);
            }
        }
        public override bool IsRedstoneSource(int meta) => kind == RailKind.Detector;
        public override int GetWeakPower(World w, Int3 pos, int meta, Dir towards) => kind == RailKind.Detector && Powered(meta) ? 15 : 0;
        public override int GetStrongPower(World w, Int3 pos, int meta, Dir towards) => kind == RailKind.Detector && Powered(meta) && towards == Dir.Down ? 15 : 0;
        public override void OnEntityInside(World w, Int3 pos, int meta, Entity e)
        {
            if (kind == RailKind.Detector && e is Minecart && !Powered(meta))
            {
                w.SetState(pos, State(meta | 16), SetFlags.Notify); Redstone.NotifyAround(w, pos);
                w.ScheduleTick(pos, this, 20);
            }
        }
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            if (kind != RailKind.Detector || !Powered(meta)) return;
            var box = new AABB(pos.x + 0.1f, pos.y, pos.z + 0.1f, pos.x + 0.9f, pos.y + 0.9f, pos.z + 0.9f);
            bool any = false; foreach (var e in w.GetEntities(box)) if (e is Minecart) any = true;
            if (any) w.ScheduleTick(pos, this, 20);
            else { w.SetState(pos, State(meta & 15), SetFlags.Notify); Redstone.NotifyAround(w, pos); }
        }
    }

    // ============================================================================ end blocks
    /// <summary>meta: facing(0-1) | eye(2)</summary>
    public class EndPortalFrameBlock : Block
    {
        int top, side, bottom, eye;
        public EndPortalFrameBlock()
        {
            stateCount = 8; opaqueCube = false; lightOpacity = 0; hardness = -1; blastResistance = 3600000; lightEmission = 1; creativeTab = CreativeTab.Functional;
            top = Tex.Id("end_portal_frame_top"); side = Tex.Id("end_portal_frame_side"); bottom = Tex.Id("end_stone"); eye = Tex.Id("end_portal_frame_eye"); SetTex(top, bottom, side);
            isFullCubeShape = false;
        }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public static bool HasEye(int m) => (m & 4) != 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            ctx.rot = meta & 3;
            ctx.Box(0, 0, 0, 1, 13f / 16, 1, new[] { bottom, top, side, side, side, side }, 0xFFFFFFFFu);
            if (HasEye(meta)) ctx.BoxPx(4, 13, 4, 12, 16, 12, eye, 0xFFFFFFFFu, 0, false);
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { boxes.Add(BoxUtil.Px(0, 0, 0, 16, 13, 16)); if (HasEye(meta)) boxes.Add(BoxUtil.Px(4, 13, 4, 12, 16, 12)); }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(DirUtil.Opposite(ctx.playerFacing)));
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            var held = player.MainHand;
            if (HasEye(meta) || held == null || held.item.id != "ender_eye") return false;
            w.SetState(pos, State(meta | 4));
            if (!player.IsCreative) held.count--;
            Sounds.Play("block.end_portal_frame.fill", pos.Center, 1f, 1f);
            for (int i = 0; i < 16; i++) Particles.Smoke(w, pos.Center + new Vector3(Random.Range(-0.3f, 0.3f), 0.5f, Random.Range(-0.3f, 0.3f)), 1, 0.2f);
            Portals.TryActivateEndPortal(w, pos);
            return true;
        }
        public override int GetComparatorOutput(World w, Int3 pos, int meta) => HasEye(meta) ? 15 : 0;
    }

    public class EndPortalBlock : Block
    {
        public bool gateway;
        public EndPortalBlock(bool gateway)
        {
            this.gateway = gateway; opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; layer = RenderLayer.Opaque; lightEmission = 15; hardness = -1; blastResistance = 3600000;
            noItem = true; hiddenInCreative = true; SetAllTex(Tex.Id("end_portal")); isFullCubeShape = false; push = PushReaction.Block;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            if (gateway) { ctx.Cube(faceTex, 0xFFFFFFFFu, RenderLayer.Opaque); return; }
            int t = faceTex[0];
            ctx.Box(0, 11f / 16, 0, 1, 12f / 16, 1, new[] { t, t, t, t, t, t }, 0xFFFFFFFFu, (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5), false, RenderLayer.Opaque);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override bool Targetable(int meta) => false;
        public override bool HasBlockEntity => gateway;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => gateway ? new EndGatewayEntity() : null;
        public override void OnEntityInside(World w, Int3 pos, int meta, Entity e)
        {
            if (gateway) { Portals.UseGateway(w, pos, e); return; }
            if (e.position.y >= pos.y + 0.8f) return;
            // In the End an end_portal block only exists once the fight has built the exit fountain, so it hands off to
            // DragonFight (which re-checks that the dragon is dead). The stronghold's portal sits in the Overworld and
            // travels directly, whatever the fight state is.
            var fight = w.session?.dragonFight;
            if (w.dim == DimensionId.End && fight != null && fight.killed && e is Player p) fight.OnExitPortalUsed(p);
            else Portals.TravelEnd(e);
        }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (rng.Chance(0.3f)) Particles.EndPortalSmoke(w, pos.ToVector3() + new Vector3(rng.NextFloat(), 0.8f, rng.NextFloat()));
        }
    }

    public class DragonEggBlock : Block
    {
        public DragonEggBlock() { opaqueCube = false; lightOpacity = 0; this.T1("dragon_egg"); hardness = 3f; lightEmission = 1; gravity = true; creativeTab = CreativeTab.Functional; isFullCubeShape = false; }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            int t = faceTex[0];
            ctx.BoxPx(6, 15, 6, 10, 16, 10, t, 0xFFFFFFFFu); ctx.BoxPx(5, 14, 5, 11, 15, 11, t, 0xFFFFFFFFu); ctx.BoxPx(4, 13, 4, 12, 14, 12, t, 0xFFFFFFFFu);
            ctx.BoxPx(3, 11, 3, 13, 13, 13, t, 0xFFFFFFFFu); ctx.BoxPx(2, 8, 2, 14, 11, 14, t, 0xFFFFFFFFu); ctx.BoxPx(1, 3, 1, 15, 8, 15, t, 0xFFFFFFFFu);
            ctx.BoxPx(2, 1, 2, 14, 3, 14, t, 0xFFFFFFFFu); ctx.BoxPx(3, 0, 3, 13, 1, 13, t, 0xFFFFFFFFu);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(1, 0, 1, 15, 16, 15));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(1, 0, 1, 15, 16, 15));
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit) { Teleport(w, pos); return true; }
        public override void OnAttack(World w, Int3 pos, int meta, Player player) { if (!player.IsCreative) Teleport(w, pos); }
        void Teleport(World w, Int3 pos)
        {
            for (int i = 0; i < 1000; i++)
            {
                Int3 t = new Int3(pos.x + w.rand.Range(-15, 15), pos.y + w.rand.Range(-7, 7), pos.z + w.rand.Range(-15, 15));
                if (w.IsAir(t) && w.GetBlock(t.Offset(Dir.Down)).solid)
                {
                    ushort s = w.GetState(pos);
                    w.SetState(pos, 0); w.SetState(t, s);
                    for (int k = 0; k < 64; k++) Particles.Portal(w, Vector3.Lerp(pos.Center, t.Center, k / 64f));
                    return;
                }
            }
        }
    }

    public class ChorusPlantBlock : Block
    {
        public ChorusPlantBlock()
        {
            opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 0.4f; tool = ToolType.Axe; sound = SoundType.Wood; this.T1("chorus_plant"); creativeTab = CreativeTab.Natural; isFullCubeShape = false;
        }
        public override int GetOccludingFaces(int meta) => 0;
        static bool Conn(Block b) => b is ChorusPlantBlock || b is ChorusFlowerBlock || b.id == "end_stone";
        public override void Emit(MeshCtx ctx, int meta)
        {
            int t = faceTex[0];
            ctx.BoxPx(4, 4, 4, 12, 12, 12, t, 0xFFFFFFFFu, 0, false);
            if (Conn(ctx.NB(Dir.Up))) ctx.BoxPx(4, 12, 4, 12, 16, 12, t, 0xFFFFFFFFu, 1 << 0);
            if (Conn(ctx.NB(Dir.Down))) ctx.BoxPx(4, 0, 4, 12, 4, 12, t, 0xFFFFFFFFu, 1 << 1);
            if (Conn(ctx.NB(Dir.North))) ctx.BoxPx(4, 4, 12, 12, 12, 16, t, 0xFFFFFFFFu, 1 << 3);
            if (Conn(ctx.NB(Dir.South))) ctx.BoxPx(4, 4, 0, 12, 12, 4, t, 0xFFFFFFFFu, 1 << 2);
            if (Conn(ctx.NB(Dir.East))) ctx.BoxPx(12, 4, 4, 16, 12, 12, t, 0xFFFFFFFFu, 1 << 4);
            if (Conn(ctx.NB(Dir.West))) ctx.BoxPx(0, 4, 4, 4, 12, 12, t, 0xFFFFFFFFu, 1 << 5);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(3, 0, 3, 13, 16, 13));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(3, 0, 3, 13, 16, 13));
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) { if (rng.NextBool()) drops.Add(new ItemStack("chorus_fruit", 1)); }
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            var below = w.GetBlock(pos.Offset(Dir.Down));
            if (below is ChorusPlantBlock || below.id == "end_stone") return true;
            for (int d = 2; d < 6; d++) { var n = w.GetBlock(pos.Offset((Dir)d)); if (n is ChorusPlantBlock) { var nb = w.GetBlock(pos.Offset((Dir)d).Offset(Dir.Down)); if (nb is ChorusPlantBlock || nb.id == "end_stone") return true; } }
            return false;
        }
    }

    public class ChorusFlowerBlock : Block
    {
        public ChorusFlowerBlock() { stateCount = 6; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 0.4f; tool = ToolType.Axe; sound = SoundType.Wood; this.T1("chorus_flower"); randomTicks = true; creativeTab = CreativeTab.Natural; isFullCubeShape = false; }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            int t = meta >= 5 ? Tex.Id("chorus_flower_dead") : faceTex[0];
            ctx.BoxPx(2, 2, 2, 14, 14, 14, t, 0xFFFFFFFFu, 0, false);
            ctx.BoxPx(3, 0, 3, 13, 2, 13, Tex.Id("chorus_plant"), 0xFFFFFFFFu);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(1, 0, 1, 15, 15, 15));
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (meta >= 5) return;
            Int3 up = pos.Offset(Dir.Up);
            if (w.IsAir(up) && w.IsAir(up.Offset(Dir.Up)) && rng.Chance(0.2f))
            {
                w.SetBlock(pos, Blocks.Get("chorus_plant"));
                w.SetState(up, State(meta + 1));
            }
            else if (rng.Chance(0.1f)) w.SetState(pos, State(5));
        }
        public override void OnProjectileHit(World w, Int3 pos, int meta, Entity projectile) => w.BreakBlock(pos, true, projectile);
    }

    public class SkullBlock : Block
    {
        public string kind;
        public SkullBlock(string kind)
        {
            this.kind = kind; stateCount = 16; opaqueCube = false; lightOpacity = 0; hardness = 1f; creativeTab = CreativeTab.Functional; isFullCubeShape = false;
            SetAllTex(Tex.Id(kind));
        }
        public override int GetOccludingFaces(int meta) => 0;
        /// <summary>meta 0-7: floor rotation(0..7 in 45deg steps) ; 8-11 wall facing</summary>
        public override void Emit(MeshCtx ctx, int meta)
        {
            int t = faceTex[0];
            int face = Tex.Id(kind + "_face");
            if (meta >= 8)
            {
                ctx.rot = meta - 8;
                ctx.Box(4f / 16, 4f / 16, 0, 12f / 16, 12f / 16, 8f / 16, new[] { t, t, face, t, t, t }, 0xFFFFFFFFu, 0, false);
                ctx.rot = 0;
                return;
            }
            float ang = meta * 45f;
            Matrix4x4 m = Matrix4x4.Translate(new Vector3(0.5f, 0, 0.5f)) * Matrix4x4.Rotate(Quaternion.Euler(0, ang, 0)) * Matrix4x4.Translate(new Vector3(-0.5f, 0, -0.5f));
            ctx.XBox(m, new Vector3(4, 0, 4) / 16f, new Vector3(12, 8, 12) / 16f, new[] { t, t, face, t, t, t }, null, 0xFFFFFFFFu, RenderLayer.Opaque);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(meta >= 8 ? BoxUtil.Rot(BoxUtil.Px(4, 4, 0, 12, 12, 8), meta - 8) : BoxUtil.Px(4, 0, 4, 12, 8, 12));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            if (DirUtil.IsHorizontal(ctx.clickedFace)) return State(8 + DirUtil.HorizIndex(DirUtil.Opposite(ctx.clickedFace)));
            int rot = Mathf.RoundToInt(Mathf.Repeat(ctx.playerYaw + 180f, 360f) / 45f) & 7;
            return State(rot);
        }
        public override void OnPlaced(World w, Int3 pos, int meta, Entity placer, ItemStack stack)
        {
            if (kind == "wither_skeleton_skull") WitherSummon.TrySummon(w, pos);
        }
    }

    // ============================================================================ crops 2
    public class StemBlock : PlantBlock
    {
        public string fruit;
        public StemBlock(string fruit) : base(fruit + "_stem", SoilKind.Farmland)
        {
            this.fruit = fruit; stateCount = 12; randomTicks = true; randomOffset = false; noItem = true; tint = TintType.Stem; flammability = 0;
        }
        public override void Emit(MeshCtx ctx, int meta)
        {
            if (meta >= 8)
            {
                // attached: bend toward fruit
                Dir d = DirUtil.FromHorizIndex(meta - 8);
                int t = Tex.Id(fruit + "_stem_attached");
                ctx.rot = DirUtil.HorizIndex(d);
                ctx.Quad(new Vector3(0, 0, 0.5f), new Vector3(0, 0.6f, 0.5f), new Vector3(1, 0.6f, 0.5f), new Vector3(1, 0, 0.5f), new Vector2(0, 0), new Vector2(0, 0.6f), new Vector2(1, 0.6f), new Vector2(1, 0), t, MeshCtx.Pack(new Color32(224, 199, 28, 255)), 0.9f, RenderLayer.Cutout, true);
                ctx.rot = 0;
                return;
            }
            float h = (meta + 1) * 2 / 16f;
            byte r = (byte)(meta * 32), g = (byte)(255 - meta * 8), b = (byte)(meta * 4);
            ctx.Cross(tex, MeshCtx.Pack(new Color32(r, g, b, 255)), 1f, -1f / 16 + h - 1f, 1f);
        }
        public override bool CanSurvive(World w, Int3 pos, int meta) => w.GetBlock(pos.Offset(Dir.Down)).id == "farmland";
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (w.GetLightLevel(pos.Offset(Dir.Up)) < 9 || !rng.Chance(0.15f)) return;
            if (meta < 7) { w.SetState(pos, State(meta + 1)); return; }
            if (meta >= 8) return;
            int d = rng.Next(4);
            Int3 t = pos.Offset(DirUtil.Horizontal[d]);
            var below = w.GetBlock(t.Offset(Dir.Down));
            if (w.IsAir(t) && (below.id == "farmland" || below.id == "dirt" || below.id == "grass_block" || below.id == "coarse_dirt" || below.id == "podzol" || below.id == "moss_block"))
            {
                w.SetBlock(t, Blocks.Get(fruit));
                w.SetState(pos, State(8 + d));
            }
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            int n = meta >= 7 ? rng.Range(0, 3) : (rng.Chance(meta / 15f) ? 1 : 0);
            if (n > 0) drops.Add(new ItemStack(fruit + "_seeds", n));
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (meta >= 8 && w.GetBlock(pos.Offset(DirUtil.FromHorizIndex(meta - 8))).id != fruit) w.SetState(pos, State(7));
            base.OnNeighborChanged(w, pos, meta, fromPos);
        }
        public override Item GetPickItem(int meta) => Items.Get(fruit + "_seeds");
    }

    public class SweetBerryBushBlock : PlantBlock
    {
        public SweetBerryBushBlock() : base("sweet_berry_bush_stage0") { stateCount = 4; randomTicks = true; randomOffset = false; noItem = true; }
        public override void Emit(MeshCtx ctx, int meta) => ctx.Cross(Tex.Id("sweet_berry_bush_stage" + meta), 0xFFFFFFFFu, 1f, 0, meta == 0 ? 0.5f : 1f);
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng) { if (meta < 3 && rng.Chance(0.2f) && w.GetLightLevel(pos.Offset(Dir.Up)) >= 9) w.SetState(pos, State(meta + 1)); }
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (meta < 2) return false;
            w.SpawnItem(pos.Center, new ItemStack("sweet_berries", meta == 3 ? 2 + w.rand.Next(2) : 1 + w.rand.Next(2)));
            w.SetState(pos, State(1));
            Sounds.Play("block.sweet_berry_bush.pick_berries", pos.Center, 1f, 1f);
            return true;
        }
        public override Vector3 StuckSpeed(int meta) => new Vector3(0.8f, 0.75f, 0.8f);
        public override void OnEntityInside(World w, Int3 pos, int meta, Entity e)
        {
            if (meta > 0 && e is LivingEntity le && !(le is Mob m && (m.def.id == "fox" || m.def.id == "bee")))
            {
                Vector3 mv = e.position - e.prevPosition;
                if (mv.x * mv.x + mv.z * mv.z > 0.0001f) le.Hurt(DamageSource.BerryBush, 1f);
            }
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) { if (meta >= 2) drops.Add(new ItemStack("sweet_berries", 1 + rng.Next(2))); }
        public override Item GetPickItem(int meta) => Items.Get("sweet_berries");
    }

    public class NetherWartBlock : CropBlock
    {
        public NetherWartBlock() : base(3, "nether_wart", new[] { 0, 1, 1, 2 }, "nether_wart", "nether_wart") { crossModel = false; }
        public override bool SoilOk(Block b) => b.id == "soul_sand";
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng) { if (meta < 3 && rng.Chance(0.1f)) w.SetState(pos, State(meta + 1)); }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) => drops.Add(new ItemStack("nether_wart", meta >= 3 ? rng.Range(2, 4) : 1));
    }

    /// <summary>meta: facing(0-1) | age(2-3)</summary>
    public class CocoaBlock : Block
    {
        public CocoaBlock() { stateCount = 12; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 0.2f; tool = ToolType.Axe; randomTicks = true; noItem = true; SetAllTex(Tex.Id("cocoa_stage0")); isFullCubeShape = false; }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            int age = meta >> 2; int t = Tex.Id("cocoa_stage" + age);
            ctx.rot = meta & 3;
            float w = 4 + age * 2, h = 5 + age * 2;
            float x0 = 8 - w / 2, x1 = 8 + w / 2;
            ctx.BoxPx(x0, 12 - h, 1, x1, 12, 1 + w, t, 0xFFFFFFFFu, 0, false);
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { int age = meta >> 2; float wd = 4 + age * 2, h = 5 + age * 2; boxes.Add(BoxUtil.Rot(BoxUtil.Px(8 - wd / 2, 12 - h, 1, 8 + wd / 2, 12, 1 + wd), meta & 3)); }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng) { int age = meta >> 2; if (age < 2 && rng.Chance(0.2f)) w.SetState(pos, State((meta & 3) | ((age + 1) << 2))); }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) => drops.Add(new ItemStack("cocoa_beans", (meta >> 2) >= 2 ? 3 : 1));
        public override Item GetPickItem(int meta) => Items.Get("cocoa_beans");
    }

    public class BambooBlock : Block
    {
        int stalk, leaves;
        public BambooBlock()
        {
            stateCount = 2; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 1f; tool = ToolType.Axe; sound = SoundType.Bamboo; randomTicks = true;
            stalk = Tex.Id("bamboo_stalk"); leaves = Tex.Id("bamboo_large_leaves"); SetAllTex(stalk); creativeTab = CreativeTab.Natural; isFullCubeShape = false;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            uint h = Hash.Get(ctx.WorldX, ctx.WorldZ);
            float ox = (h & 7) / 16f - 0.2f, oz = ((h >> 3) & 7) / 16f - 0.2f;
            var uv = new Vector4[6]; for (int i = 2; i < 6; i++) uv[i] = new Vector4(0, 0, 3 / 16f, 1); uv[0] = uv[1] = new Vector4(13 / 16f, 0, 1, 3 / 16f);
            Matrix4x4 m = Matrix4x4.Translate(new Vector3(ox, 0, oz));
            ctx.XBoxPx(m, 6.5f, 0, 6.5f, 9.5f, 16, 9.5f, stalk, 0xFFFFFFFFu, RenderLayer.Cutout, uv);
            if (meta == 1) ctx.Cross(leaves, 0xFFFFFFFFu, 1f, 0, 1f);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(6.5f, 0, 6.5f, 9.5f, 16, 9.5f));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(5, 0, 5, 11, 16, 11));
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            var b = w.GetBlock(pos.Offset(Dir.Down));
            return b == this || b.id == "grass_block" || b.id == "dirt" || b.id == "sand" || b.id == "gravel" || b.id == "podzol" || b.id == "coarse_dirt" || b.id == "mud" || b.id == "moss_block" || b.id == "red_sand" || b.id == "mycelium";
        }
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (w.GetBlock(pos.Offset(Dir.Up)).isAir && rng.Chance(0.3f))
            {
                int h = 1; while (w.GetBlock(new Int3(pos.x, pos.y - h, pos.z)) == this) h++;
                if (h < 14) { w.SetState(pos.Offset(Dir.Up), State(1)); if (h > 2) w.SetState(new Int3(pos.x, pos.y - 2, pos.z), State(0), 0); }
            }
        }
        public override void OnBroken(World w, Int3 pos, int meta, Entity breaker)
        {
            Int3 up = pos.Offset(Dir.Up);
            if (w.GetBlock(up) == this) w.session?.Defer(() => { if (w.GetBlock(up) == this) w.BreakBlock(up, true, breaker); });
        }
    }

    public class LilyPadBlock : Block
    {
        public LilyPadBlock() { opaqueCube = false; solid = true; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 0; sound = SoundType.Grass; SetAllTex(Tex.Id("lily_pad")); tint = TintType.Lily; creativeTab = CreativeTab.Natural; isFullCubeShape = false; push = PushReaction.Destroy; }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            uint h = Hash.Get(ctx.WorldX, ctx.WorldZ);
            ctx.FloorDecal(faceTex[0], ctx.TintColor(TintType.Lily), 0.25f / 16f, (int)(h & 3));
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(1, 0, 1, 15, 1.5f, 15));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(1, 0, 1, 15, 1.5f, 15));
        public override bool CanSurvive(World w, Int3 pos, int meta) => w.IsWater(pos.Offset(Dir.Down)) || w.IsLava(pos.Offset(Dir.Down)) == false && w.GetBlock(pos.Offset(Dir.Down)).id == "ice";
        public override BlockItem CustomItem() => new LilyPadItem();
    }

    /// <summary>Always-waterlogged plants (seagrass, kelp, coral).</summary>
    public class WaterPlantBlock : PlantBlock
    {
        public bool tall, kelpTop;
        public WaterPlantBlock(string tex, bool tall) : base(tex, SoilKind.Any)
        {
            this.tall = tall; randomOffset = false; replaceable = true; creativeTab = CreativeTab.Natural; flammability = 0; fireSpread = 0;
            if (tex.Contains("coral")) { replaceable = false; hardness = 0; }
            if (tex.StartsWith("kelp")) randomTicks = true;
        }
        public override bool IsWaterLike(int meta) => true;
        public override byte GetLightOpacity(int meta) => 1;
        public override void Emit(MeshCtx ctx, int meta)
        {
            if (id.EndsWith("_fan")) { ctx.FloorDecal(tex, 0xFFFFFFFFu, 0.5f / 16f, 0); ctx.Cross(tex, 0xFFFFFFFFu, 0.9f, 0, 0.6f); return; }
            ctx.Cross(tex, 0xFFFFFFFFu, 1f, 0, 1f);
        }
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            var below = w.GetBlock(pos.Offset(Dir.Down));
            if (id.StartsWith("kelp")) return below.id.StartsWith("kelp") || below.solid;
            return below.solid && below.sturdy;
        }
        public override bool CanPlaceAt(World w, Int3 pos, ushort state) => w.IsWater(pos) && CanSurvive(w, pos, 0);
        public override void OnBroken(World w, Int3 pos, int meta, Entity breaker)
        {
            w.session?.Defer(() => { if (w.IsAir(pos)) w.SetState(pos, Blocks.Water.DefaultState); });
            if (id.StartsWith("kelp")) { Int3 up = pos.Offset(Dir.Up); if (w.GetBlock(up).id.StartsWith("kelp")) w.session?.Defer(() => w.BreakBlock(up, true, breaker)); }
        }
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (!kelpTop) return;
            Int3 up = pos.Offset(Dir.Up);
            if (w.IsWater(up) && w.GetState(up) == Blocks.Water.DefaultState && rng.Chance(0.14f))
            {
                w.SetBlock(pos, Blocks.Get("kelp_plant"));
                w.SetBlock(up, this);
            }
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if (id == "kelp_plant") { drops.Add(new ItemStack("kelp", 1)); return; }
            if (id == "seagrass" || id == "tall_seagrass") { if (tool != null && tool.item.toolType == ToolType.Shears) drops.Add(new ItemStack("seagrass", 1)); return; }
            if (id.Contains("coral") && (tool == null || tool.GetEnchant(Enchant.SilkTouch) == 0)) return;
            base.GetDrops(w, pos, meta, tool, drops, ref rng);
        }
    }

    public class SeaPickleBlock : Block
    {
        public SeaPickleBlock() { stateCount = 4; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 0; sound = SoundType.Slime; SetAllTex(Tex.Id("sea_pickle")); creativeTab = CreativeTab.Natural; isFullCubeShape = false; }
        public override int GetOccludingFaces(int meta) => 0;
        public override bool IsWaterLike(int meta) => true;
        public override byte GetLightEmission(int meta) => (byte)(6 + meta * 3);
        public override void Emit(MeshCtx ctx, int meta)
        {
            int t = faceTex[0];
            float[,] pos = { { 6, 6 }, { 3, 9 }, { 10, 4 }, { 9, 10 } };
            for (int i = 0; i <= meta; i++) ctx.BoxPx(pos[i, 0], 0, pos[i, 1], pos[i, 0] + 4, 6, pos[i, 1] + 4, t, 0xFFFFFFFFu, 0, false);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(2, 0, 2, 14, 7, 14));
        public override bool CanBeReplaced(int meta, ref PlaceContext ctx) => ctx.stack != null && ctx.stack.item.block == this && meta < 3;
        public override ushort GetPlacementState(ref PlaceContext ctx) { ushort cur = ctx.world.GetState(ctx.pos); return Blocks.ByState[cur] == this ? State(Mathf.Min(3, cur - baseState + 1)) : State(0); }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) => drops.Add(new ItemStack(item, meta + 1));
    }

    public class CaveVinesBlock : Block
    {
        int tex, lit;
        public CaveVinesBlock()
        {
            stateCount = 2; opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 0; climbable = true; sound = SoundType.Grass;
            tex = Tex.Id("cave_vines"); lit = Tex.Id("cave_vines_lit"); SetAllTex(tex); isFullCubeShape = false; push = PushReaction.Destroy;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override byte GetLightEmission(int meta) => (byte)(meta == 1 ? 14 : 0);
        public override void Emit(MeshCtx ctx, int meta) => ctx.Cross(meta == 1 ? lit : tex, 0xFFFFFFFFu, 1f, 0, 1f);
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(1, 0, 1, 15, 16, 15));
        public override bool CanSurvive(World w, Int3 pos, int meta) { var up = w.GetBlock(pos.Offset(Dir.Up)); return up == this || up.solid; }
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (meta != 1) return false;
            w.SpawnItem(pos.Center, new ItemStack("glow_berries", 1)); w.SetState(pos, State(0));
            Sounds.Play("block.cave_vines.pick_berries", pos.Center, 1f, 1f);
            return true;
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) { if (meta == 1) drops.Add(new ItemStack("glow_berries", 1)); }
        public override Item GetPickItem(int meta) => Items.Get("glow_berries");
    }

    public class HangingPlantBlock : PlantBlock
    {
        public HangingPlantBlock(string tex) : base(tex, SoilKind.Any) { randomOffset = false; creativeTab = CreativeTab.Natural; flammability = 0; fireSpread = 0; }
        public override bool CanSurvive(World w, Int3 pos, int meta) { var up = w.GetBlock(pos.Offset(Dir.Up)); return up.solid || up is LeavesBlock || up == this; }
        public override void Emit(MeshCtx ctx, int meta)
        {
            if (id == "spore_blossom")
            {
                int t = tex;
                ctx.Quad(new Vector3(0, 15.9f / 16, 0), new Vector3(0, 15.9f / 16, 1), new Vector3(1, 15.9f / 16, 1), new Vector3(1, 15.9f / 16, 0), new Vector2(0, 0), new Vector2(0, 1), new Vector2(1, 1), new Vector2(1, 0), t, 0xFFFFFFFFu, 0.8f, RenderLayer.Cutout, true);
                ctx.Cross(Tex.Id("spore_blossom_base"), 0xFFFFFFFFu, 0.8f, 0.4f, 0.6f);
                return;
            }
            ctx.Cross(tex, 0xFFFFFFFFu, 1f, 0, 1f);
        }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (id == "spore_blossom")
                for (int i = 0; i < 3; i++) Particles.Spore(w, pos.Center + new Vector3(rng.Range(-8f, 8f), rng.Range(-8f, 0f), rng.Range(-8f, 8f)));
        }
    }

    public class DripleafBlock : Block
    {
        public bool big;
        int leafTop, stem;
        public DripleafBlock(bool big)
        {
            this.big = big; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 0.1f; sound = SoundType.Grass; creativeTab = CreativeTab.Natural; isFullCubeShape = false;
            leafTop = Tex.Id(big ? "big_dripleaf_top" : "small_dripleaf_top"); stem = Tex.Id(big ? "big_dripleaf_stem" : "small_dripleaf_stem"); SetAllTex(leafTop); push = PushReaction.Destroy;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            if (big)
            {
                ctx.Quad(new Vector3(0, 15f / 16, 0), new Vector3(0, 15f / 16, 1), new Vector3(1, 15f / 16, 1), new Vector3(1, 15f / 16, 0), new Vector2(0, 0), new Vector2(0, 1), new Vector2(1, 1), new Vector2(1, 0), leafTop, 0xFFFFFFFFu, 1f, RenderLayer.Cutout, true);
                ctx.Cross(stem, 0xFFFFFFFFu, 0.7f, 0, 0.93f);
            }
            else ctx.Cross(leafTop, 0xFFFFFFFFu, 1f, 0, 1f);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { if (big) boxes.Add(BoxUtil.Px(0, 11, 0, 16, 15, 16)); }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(big ? BoxUtil.Px(0, 11, 0, 16, 15, 16) : BoxUtil.Px(2, 0, 2, 14, 13, 14));
    }

    /// <summary>meta 0 = hanging (tip down), 1 = standing (tip up).</summary>
    public class PointedDripstoneBlock : Block
    {
        public PointedDripstoneBlock() { stateCount = 2; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 1.5f; tool = ToolType.Pickaxe; SetAllTex(Tex.Id("pointed_dripstone")); creativeTab = CreativeTab.Natural; isFullCubeShape = false; }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            int t = meta == 0 ? Tex.Id("pointed_dripstone_down") : Tex.Id("pointed_dripstone");
            ctx.Cross(t, 0xFFFFFFFFu, 0.9f, 0, 1f);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(5, 0, 5, 11, 16, 11));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(5, 0, 5, 11, 16, 11));
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(ctx.clickedFace == Dir.Down ? 0 : 1);
        public override bool CanSurvive(World w, Int3 pos, int meta) => meta == 0 ? (w.GetBlock(pos.Offset(Dir.Up)).solid) : w.GetBlock(pos.Offset(Dir.Down)).solid;
        public override void OnFallenUpon(World w, Int3 pos, int meta, Entity e, float fallDistance) { if (meta == 1 && e is LivingEntity le) le.Hurt(DamageSource.Fall, Mathf.Max(0, (fallDistance - 2) * 2)); else base.OnFallenUpon(w, pos, meta, e, fallDistance); }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng) { if (meta == 0 && rng.Chance(0.02f)) Particles.Drip(w, pos.Center - Vector3.up * 0.5f, w.IsLava(pos.Offset(Dir.Up).Offset(Dir.Up))); }
    }

    public class AmethystClusterBlock : Block
    {
        public int size;
        public AmethystClusterBlock(string tex, int size)
        {
            this.size = size; stateCount = 6; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 1.5f; sound = SoundType.Amethyst; SetAllTex(Tex.Id(tex));
            lightEmission = (byte)(size + 1); creativeTab = CreativeTab.Natural; isFullCubeShape = false; push = PushReaction.Destroy;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            Dir f = (Dir)meta;
            if (f == Dir.Up) { ctx.Cross(faceTex[0], 0xFFFFFFFFu, 0.5f + size * 0.15f, 0, 1f); return; }
            if (f == Dir.Down) { ctx.Cross(faceTex[0], 0xFFFFFFFFu, 0.5f + size * 0.15f, 0.2f, 0.8f); return; }
            ctx.WallDecal(f, faceTex[0], 0xFFFFFFFFu, 3f / 16f);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(3, 0, 3, 13, 4 + size * 2, 13));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(3, 0, 3, 13, 4 + size * 2, 13));
        public override ushort GetPlacementState(ref PlaceContext ctx) => State((int)ctx.clickedFace);
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if (size == 3 && tool != null && tool.item.toolType == ToolType.Pickaxe) drops.Add(new ItemStack("amethyst_shard", 4));
            else if (size == 3) drops.Add(new ItemStack("amethyst_shard", 2));
        }
    }

    public class SculkSensorBlock : Block
    {
        public SculkSensorBlock() { stateCount = 2; opaqueCube = false; lightOpacity = 0; this.T3("sculk_sensor_top", "sculk_sensor_bottom", "sculk_sensor_side"); isFullCubeShape = false; }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public override byte GetLightEmission(int meta) => (byte)(meta == 1 ? 1 : 0);
        public override void Emit(MeshCtx ctx, int meta)
        {
            ctx.Box(0, 0, 0, 1, 8f / 16, 1, faceTex, 0xFFFFFFFFu);
            int tend = Tex.Id(meta == 1 ? "sculk_sensor_tendril_active" : "sculk_sensor_tendril_inactive");
            ctx.Cross(tend, 0xFFFFFFFFu, 0.8f, 0.5f, 0.5f);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 8, 16));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 8, 16));
        public override bool IsRedstoneSource(int meta) => true;
        public override int GetWeakPower(World w, Int3 pos, int meta, Dir towards) => meta == 1 ? 15 : 0;
        public void Activate(World w, Int3 pos)
        {
            if (w.GetMeta(pos) == 1) return;
            w.SetState(pos, State(1), SetFlags.Notify); Redstone.NotifyAround(w, pos);
            Sounds.Play("block.sculk_sensor.clicking", pos.Center, 1f, 1f);
            w.ScheduleTick(pos, this, 30);
        }
        public override void OnScheduledTick(World w, Int3 pos, int meta) { if (meta == 1) { w.SetState(pos, State(0), SetFlags.Notify); Redstone.NotifyAround(w, pos); } }
        public override void OnSteppedOn(World w, Int3 pos, int meta, Entity e) { if (!e.sneaking) Activate(w, pos); }
    }

    public class SculkShriekerBlock : Block
    {
        public SculkShriekerBlock() { opaqueCube = false; lightOpacity = 0; this.T3("sculk_shrieker_top", "sculk_shrieker_bottom", "sculk_shrieker_side"); isFullCubeShape = false; }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public override void Emit(MeshCtx ctx, int meta) => ctx.Box(0, 0, 0, 1, 8f / 16, 1, faceTex, 0xFFFFFFFFu);
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 8, 16));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 8, 16));
        public override void OnSteppedOn(World w, Int3 pos, int meta, Entity e)
        {
            if (e is Player p && !p.IsCreative && !p.sneaking) Warden.Shriek(w, pos, p);
        }
    }

    public class CreakingHeartBlock : PillarBlock
    {
        public CreakingHeartBlock() : base("creaking_heart_top", "creaking_heart") { hardness = 10f; tool = ToolType.Axe; sound = SoundType.Wood; creativeTab = CreativeTab.Natural; }
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new CreakingHeartEntity();
        public override byte GetLightEmission(int meta) => 0;
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (w.session != null && w.session.IsNight && rng.Chance(0.1f)) Particles.Effect(w, pos.Center + Random.insideUnitSphere * 0.6f, new Color32(255, 140, 40, 255), true);
        }
    }

    public class CandleBlock : Block
    {
        public CandleBlock(string tex)
        {
            stateCount = 8; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 0.1f; sound = SoundType.Wool; SetAllTex(Tex.Id(tex)); creativeTab = CreativeTab.Colored; isFullCubeShape = false; push = PushReaction.Destroy;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public static int Count(int m) => (m & 3) + 1;
        public static bool Lit(int m) => (m & 4) != 0;
        public override byte GetLightEmission(int meta) => Lit(meta) ? (byte)(3 * Count(meta)) : (byte)0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            int t = faceTex[0];
            var uv = new Vector4[6]; for (int i = 2; i < 6; i++) uv[i] = new Vector4(0, 8 / 16f, 2 / 16f, 14 / 16f); uv[0] = uv[1] = new Vector4(0, 6 / 16f, 2 / 16f, 8 / 16f);
            float[,] p = { { 7, 7 }, { 5, 9 }, { 9, 5 }, { 9, 9 } };
            for (int i = 0; i < Count(meta); i++) ctx.XBoxPx(Matrix4x4.identity, p[i, 0], 0, p[i, 1], p[i, 0] + 2, 6 - i, p[i, 1] + 2, t, 0xFFFFFFFFu, RenderLayer.Cutout, uv);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(5, 0, 5, 11, 6, 11));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(5, 0, 5, 11, 6, 11));
        public override bool CanBeReplaced(int meta, ref PlaceContext ctx) => ctx.stack != null && ctx.stack.item.block == this && Count(meta) < 4;
        public override ushort GetPlacementState(ref PlaceContext ctx) { ushort cur = ctx.world.GetState(ctx.pos); return Blocks.ByState[cur] == this ? State(Mathf.Min(3, ((cur - baseState) & 3) + 1) | ((cur - baseState) & 4)) : State(0); }
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            var held = player.MainHand;
            if (!Lit(meta) && held != null && (held.item.id == "flint_and_steel" || held.item.id == "fire_charge")) { w.SetState(pos, State(meta | 4)); Sounds.Play("item.flintandsteel.use", pos.Center, 1f, 1f); return true; }
            if (Lit(meta) && (held == null || held.IsEmpty)) { w.SetState(pos, State(meta & 3)); Sounds.Play("block.candle.extinguish", pos.Center, 1f, 1f); return true; }
            return false;
        }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng) { if (Lit(meta) && rng.Chance(0.3f)) Particles.Flame(w, pos.Center + new Vector3(0, 0.05f, 0), "small_flame"); }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) => drops.Add(new ItemStack(item, Count(meta)));
    }
}
