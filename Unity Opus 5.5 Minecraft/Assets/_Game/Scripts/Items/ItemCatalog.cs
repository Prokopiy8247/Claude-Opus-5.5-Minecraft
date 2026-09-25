using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public static partial class Items
    {
        static Item Mat(string id, int stack = 64, CreativeTab tab = CreativeTab.Ingredients) => Reg(id, new Item { tab = tab, maxStack = stack });
        static FoodItem Food(string id, int nut, float sat) => Reg(id, new FoodItem(nut, sat));

        static void RegisterAll()
        {
            // ------------------------------------------------------------------ basic materials
            foreach (var id in new[] { "stick", "coal", "charcoal", "raw_iron", "raw_copper", "raw_gold", "iron_ingot", "copper_ingot", "gold_ingot", "netherite_ingot", "netherite_scrap",
                "iron_nugget", "gold_nugget", "copper_nugget", "diamond", "emerald", "lapis_lazuli", "quartz", "amethyst_shard", "redstone", "glowstone_dust", "flint", "clay_ball", "brick", "nether_brick",
                "leather", "rabbit_hide", "feather", "string", "bone", "gunpowder", "slime_ball", "magma_cream", "blaze_rod", "blaze_powder", "ghast_tear",
                "nether_star", "shulker_shell", "phantom_membrane", "prismarine_shard", "prismarine_crystals", "nautilus_shell", "heart_of_the_sea", "turtle_scute", "armadillo_scute", "honeycomb",
                "ink_sac", "glow_ink_sac", "paper", "book", "sugar", "wheat", "wheat_seeds", "pumpkin_seeds", "melon_seeds", "beetroot_seeds", "cocoa_beans", "nether_wart", "echo_shard", "disc_fragment_5",
                "breeze_rod", "trial_key", "ominous_trial_key", "resin_clump", "sulfur_dust", "cinnabar_dust", "rabbit_foot", "fermented_spider_eye", "glistering_melon_slice",
                "popped_chorus_fruit", "dragon_breath", "firework_star", "name_tag", "lead", "saddle", "flower_banner_pattern", "goat_horn", "brush", "spyglass", "recovery_compass", "clock", "compass", "map", "filled_map", "writable_book", "written_book", "bowl", "glass_bottle", "experience_bottle", "totem_of_undying", "harness" })
            {
                if (id.EndsWith("_placeholder")) continue;
                Mat(id);
            }
            foreach (var d in Blocks.Colors) Mat(d + "_dye");
            Reg("ender_pearl", new SimpleThrowable("ender_pearl", 16) { tab = CreativeTab.Ingredients });
            Reg("ender_eye", new EnderEyeItem());
            Get("name_tag").tab = CreativeTab.Tools; Get("lead").tab = CreativeTab.Tools; Get("saddle").tab = CreativeTab.Tools; Get("saddle").maxStack = 1;
            Get("clock").tab = CreativeTab.Tools; Get("compass").tab = CreativeTab.Tools; Get("recovery_compass").tab = CreativeTab.Tools; Get("spyglass").tab = CreativeTab.Tools; Get("spyglass").maxStack = 1;
            Get("brush").tab = CreativeTab.Tools; Get("brush").maxStack = 1; Get("brush").maxDamage = 64; Get("goat_horn").tab = CreativeTab.Tools; Get("goat_horn").maxStack = 1;
            Get("totem_of_undying").tab = CreativeTab.Combat; Get("totem_of_undying").maxStack = 1; Get("totem_of_undying").rarity = Rarity.Uncommon;
            Get("nether_star").glint = true; Get("nether_star").rarity = Rarity.Uncommon; Get("nether_star").fireResistant = true;
            Get("experience_bottle").rarity = Rarity.Uncommon; Get("experience_bottle").glint = true; Get("experience_bottle").tab = CreativeTab.Tools;
            Get("harness").tab = CreativeTab.Tools; Get("harness").maxStack = 1;
            Get("map").tab = CreativeTab.Tools; Get("filled_map").hiddenInCreative = true; Get("writable_book").tab = CreativeTab.Tools; Get("writable_book").maxStack = 1; Get("written_book").hiddenInCreative = true;
            Get("netherite_ingot").fireResistant = true; Get("netherite_scrap").fireResistant = true;
            Get("coal").fuelTicks = 1600; Get("charcoal").fuelTicks = 1600; Get("blaze_rod").fuelTicks = 2400; Get("stick").fuelTicks = 100;
            Reg("bone_meal", new BoneMealItem());
            Reg("fire_charge", new FlintAndSteelItem(true));
            Reg("flint_and_steel", new FlintAndSteelItem(false));
            Reg("enchanted_book", new EnchantedBookItem());
            Reg("egg", new SimpleThrowable("egg", 16)); Get("egg").tab = CreativeTab.Ingredients;
            Reg("snowball", new SimpleThrowable("snowball", 16));
            Reg("wind_charge", new SimpleThrowable("wind_charge", 64));
            // ------------------------------------------------------------------ tools
            var tiers = new[] { ("wooden", ToolTier.Wood), ("stone", ToolTier.Stone), ("copper", ToolTier.Copper), ("iron", ToolTier.Iron), ("golden", ToolTier.Gold), ("diamond", ToolTier.Diamond), ("netherite", ToolTier.Netherite) };
            float[] swordDmg = { 4, 5, 5, 6, 4, 7, 8 }, axeDmg = { 7, 9, 9, 9, 7, 9, 10 }, axeSpd = { 0.8f, 0.8f, 0.8f, 0.9f, 1f, 1f, 1f };
            float[] spearDmg = { 3, 4, 4, 5, 3, 6, 7 };
            for (int i = 0; i < tiers.Length; i++)
            {
                var (n, t) = tiers[i];
                Reg(n + "_sword", new ToolItem(ToolType.Sword, t, swordDmg[i], 1.6f));
                Reg(n + "_pickaxe", new ToolItem(ToolType.Pickaxe, t, 2 + t.damageBonus, 1.2f));
                Reg(n + "_axe", new ToolItem(ToolType.Axe, t, axeDmg[i], axeSpd[i]));
                Reg(n + "_shovel", new ToolItem(ToolType.Shovel, t, 2.5f + t.damageBonus, 1f));
                Reg(n + "_hoe", new ToolItem(ToolType.Hoe, t, 1, 1f + Mathf.Min(3, t.level - 1)));
                Reg(n + "_spear", new SpearItem(t, spearDmg[i]));
            }
            Reg("shears", new ToolItem(ToolType.Shears, ToolTier.Iron, 1, 4) { maxDamage = 238, tab = CreativeTab.Tools });
            Get("shears").tier = 0;
            Reg("mace", new MaceItem());
            Reg("bow", new BowItem());
            Reg("crossbow", new CrossbowItem());
            Reg("trident", new TridentItem());
            Reg("shield", new ShieldItem());
            Reg("fishing_rod", new FishingRodItem());
            Reg("arrow", new Item { tab = CreativeTab.Combat });
            Reg("spectral_arrow", new Item { tab = CreativeTab.Combat });
            Reg("tipped_arrow", new PotionItem(PotionKind.Arrow));
            Reg("elytra", new ElytraItem());
            Reg("firework_rocket", new FireworkItem());
            Reg("carrot_on_a_stick", new Item { maxStack = 1, maxDamage = 25, tab = CreativeTab.Tools });
            Reg("warped_fungus_on_a_stick", new Item { maxStack = 1, maxDamage = 100, tab = CreativeTab.Tools });
            // ------------------------------------------------------------------ armor
            foreach (var m in ArmorMaterial.All)
            {
                Reg(m.name + "_helmet", new ArmorItem(m, ArmorSlot.Head));
                Reg(m.name + "_chestplate", new ArmorItem(m, ArmorSlot.Chest));
                Reg(m.name + "_leggings", new ArmorItem(m, ArmorSlot.Legs));
                Reg(m.name + "_boots", new ArmorItem(m, ArmorSlot.Feet));
            }
            Reg("turtle_helmet", new ArmorItem(ArmorMaterial.Turtle, ArmorSlot.Head));
            Get("leather_helmet").displayName = "Leather Cap"; Get("leather_chestplate").displayName = "Leather Tunic"; Get("leather_leggings").displayName = "Leather Pants";
            Reg("netherite_upgrade_smithing_template", new Item { tab = CreativeTab.Ingredients, rarity = Rarity.Uncommon });
            foreach (var h in new[] { "leather_horse_armor", "iron_horse_armor", "golden_horse_armor", "diamond_horse_armor", "wolf_armor" }) Reg(h, new Item { maxStack = 1, tab = CreativeTab.Combat });
            // ------------------------------------------------------------------ food
            Food("apple", 4, 0.3f); Food("bread", 5, 0.6f);
            Food("beef", 3, 0.3f).meat = true; Food("cooked_beef", 8, 0.8f).meat = true;
            Food("porkchop", 3, 0.3f).meat = true; Food("cooked_porkchop", 8, 0.8f).meat = true;
            var ch = Food("chicken", 2, 0.3f); ch.meat = true; ch.effects.Add((new EffectInstance(Effect.Hunger, 600, 0), 0.3f));
            Food("cooked_chicken", 6, 0.6f).meat = true;
            Food("mutton", 2, 0.3f).meat = true; Food("cooked_mutton", 6, 0.8f).meat = true;
            Food("rabbit", 3, 0.3f).meat = true; Food("cooked_rabbit", 5, 0.6f).meat = true;
            Food("cod", 2, 0.1f); Food("cooked_cod", 5, 0.6f); Food("salmon", 2, 0.1f); Food("cooked_salmon", 6, 0.8f);
            Food("tropical_fish", 1, 0.1f);
            var puffer = Food("pufferfish", 1, 0.1f); puffer.effects.Add((new EffectInstance(Effect.Poison, 1200, 1), 1f)); puffer.effects.Add((new EffectInstance(Effect.Hunger, 300, 2), 1f)); puffer.effects.Add((new EffectInstance(Effect.Nausea, 300, 0), 1f));
            Food("potato", 1, 0.3f); Food("baked_potato", 5, 0.6f);
            var poison = Food("poisonous_potato", 2, 0.3f); poison.effects.Add((new EffectInstance(Effect.Poison, 100, 0), 0.6f));
            Food("carrot", 3, 0.6f); Food("golden_carrot", 6, 1.2f); Food("beetroot", 1, 0.6f);
            var soup = Food("beetroot_soup", 6, 0.6f); soup.maxStack = 1; soup.remainder = "bowl";
            var mush = Food("mushroom_stew", 6, 0.6f); mush.maxStack = 1; mush.remainder = "bowl";
            var rab = Food("rabbit_stew", 10, 0.6f); rab.maxStack = 1; rab.remainder = "bowl";
            var sus = Food("suspicious_stew", 6, 0.6f); sus.maxStack = 1; sus.remainder = "bowl"; sus.alwaysEdible = true;
            var ga = Food("golden_apple", 4, 1.2f); ga.alwaysEdible = true; ga.rarity = Rarity.Rare; ga.effects.Add((new EffectInstance(Effect.Regeneration, 100, 1), 1f)); ga.effects.Add((new EffectInstance(Effect.Absorption, 2400, 0), 1f));
            var ega = Food("enchanted_golden_apple", 4, 1.2f); ega.alwaysEdible = true; ega.glint = true; ega.rarity = Rarity.Epic;
            ega.effects.Add((new EffectInstance(Effect.Regeneration, 400, 1), 1f)); ega.effects.Add((new EffectInstance(Effect.Absorption, 2400, 3), 1f)); ega.effects.Add((new EffectInstance(Effect.Resistance, 6000, 0), 1f)); ega.effects.Add((new EffectInstance(Effect.FireResistance, 6000, 0), 1f));
            Food("melon_slice", 2, 0.3f); Food("sweet_berries", 2, 0.1f).fast = false; Food("glow_berries", 2, 0.1f);
            Food("cookie", 2, 0.1f); Food("pumpkin_pie", 8, 0.3f);
            var rotten = Food("rotten_flesh", 4, 0.1f); rotten.effects.Add((new EffectInstance(Effect.Hunger, 600, 0), 0.8f)); rotten.meat = true;
            var spider = Food("spider_eye", 2, 0.8f); spider.effects.Add((new EffectInstance(Effect.Poison, 100, 0), 1f));
            var chorus = Food("chorus_fruit", 4, 0.3f); chorus.alwaysEdible = true;
            Food("dried_kelp", 1, 0.3f).fast = true;
            var honey = Food("honey_bottle", 6, 0.1f); honey.remainder = "glass_bottle"; honey.maxStack = 16;
            Reg("milk_bucket", new BucketItem("milk"));
            // ------------------------------------------------------------------ buckets & utilities
            Reg("bucket", new BucketItem(null)); Reg("water_bucket", new BucketItem("water")); Reg("lava_bucket", new BucketItem("lava")); Reg("powder_snow_bucket", new BucketItem("powder_snow"));
            foreach (var f in new[] { "cod", "salmon", "pufferfish", "tropical_fish", "axolotl", "tadpole" }) Reg(f + "_bucket", new MobBucketItem(f));
            Get("lava_bucket").fuelTicks = 20000;
            Reg("minecart", new MinecartItem(null)); Reg("chest_minecart", new MinecartItem("chest")); Reg("hopper_minecart", new MinecartItem("hopper")); Reg("tnt_minecart", new MinecartItem("tnt")); Reg("furnace_minecart", new MinecartItem("furnace"));
            foreach (var w in new[] { "oak", "spruce", "birch", "jungle", "acacia", "dark_oak", "mangrove", "cherry", "pale_oak" })
            {
                Reg(w + "_boat", new BoatItem(w, false));
                Reg(w + "_chest_boat", new BoatItem(w, true));
            }
            Reg("bamboo_raft", new BoatItem("bamboo", false)); Reg("bamboo_chest_raft", new BoatItem("bamboo", true));
            Reg("end_crystal", new EndCrystalItem());
            Reg("armor_stand", new Item { maxStack = 16, tab = CreativeTab.Functional });
            Reg("item_frame", new Item { tab = CreativeTab.Functional }); Reg("painting", new Item { tab = CreativeTab.Functional });
            // block placers (items that place differently-named blocks)
            ReplaceWithPlacer("redstone", "redstone_wire", CreativeTab.Redstone);
            SetPlacer("repeater", "repeater", CreativeTab.Redstone);
            SetPlacer("comparator", "comparator", CreativeTab.Redstone);
            ReplaceWithPlacer("wheat_seeds", "wheat", CreativeTab.Natural);
            ReplaceWithPlacer("carrot", "carrots", CreativeTab.Food);
            ReplaceWithPlacer("potato", "potatoes", CreativeTab.Food);
            ReplaceWithPlacer("beetroot_seeds", "beetroots", CreativeTab.Natural);
            ReplaceWithPlacer("pumpkin_seeds", "pumpkin_stem", CreativeTab.Natural);
            ReplaceWithPlacer("melon_seeds", "melon_stem", CreativeTab.Natural);
            ReplaceWithPlacer("nether_wart", "nether_wart", CreativeTab.Natural);
            ReplaceWithPlacer("cocoa_beans", "cocoa", CreativeTab.Natural);
            ReplaceWithPlacer("sweet_berries", "sweet_berry_bush", CreativeTab.Food);
            ReplaceWithPlacer("glow_berries", "cave_vines", CreativeTab.Food);
            // ------------------------------------------------------------------ potions
            Reg("potion", new PotionItem(PotionKind.Drink)); Reg("splash_potion", new PotionItem(PotionKind.Splash)); Reg("lingering_potion", new PotionItem(PotionKind.Lingering));
            // ------------------------------------------------------------------ discs
            foreach (var d in new[] { "13", "cat", "blocks", "chirp", "far", "mall", "mellohi", "stal", "strad", "ward", "11", "wait", "otherside", "pigstep", "relic", "creator", "precipice" })
                Reg("music_disc_" + d, new Item { maxStack = 1, tab = CreativeTab.Tools, rarity = Rarity.Rare, displayName = "Music Disc", description = "Original composition: " + d });
            // ------------------------------------------------------------------ spawn eggs
            foreach (var def in MobRegistry.All)
            {
                if (!def.hasEgg) continue;
                Reg(def.id + "_spawn_egg", new SpawnEggItem(def.id) { displayName = def.displayName + " Spawn Egg" });
            }
            // extras for bosses in creative (benchmark request: direct test-spawn)
            if (Get("ender_dragon_spawn_egg") == null) Reg("ender_dragon_spawn_egg", new SpawnEggItem("ender_dragon") { displayName = "Ender Dragon Spawn Egg" });
            if (Get("wither_spawn_egg") == null) Reg("wither_spawn_egg", new SpawnEggItem("wither") { displayName = "Wither Spawn Egg" });
        }

        /// <summary>Make an existing item place a block when used on a block.</summary>
        static void ReplaceWithPlacer(string itemId, string blockId, CreativeTab tab)
        {
            var old = Get(itemId);
            var b = Blocks.Get(blockId);
            if (old == null || b == null) return;
            var placer = new BlockPlacerItem(blockId) { tab = tab, maxStack = old.maxStack, fuelTicks = old.fuelTicks };
            ReplaceItem(old, placer);
            if (old is FoodItem f)
            {
                // keep edible behaviour: wrap as edible placer
                var ef = new EdiblePlacerItem(blockId, f.nutrition, f.saturationMod) { tab = tab };
                ReplaceItem(placer, ef);
                b.item = ef;
            }
            else b.item = placer;
        }
        static void SetPlacer(string itemId, string blockId, CreativeTab tab)
        {
            var b = Blocks.Get(blockId);
            if (b == null) return;
            var existing = Get(itemId);
            var placer = new BlockPlacerItem(blockId) { tab = tab };
            if (existing != null) ReplaceItem(existing, placer); else Reg(itemId, placer);
            b.item = Get(itemId);
        }
        static void ReplaceItem(Item old, Item nu)
        {
            nu.id = old.id; nu.index = old.index; nu.displayName = old.displayName; nu.iconName = old.iconName;
            if (nu.description == null) nu.description = old.description;
            All[old.index] = nu;
            byId[old.id] = nu;
        }
    }

    /// <summary>Item that places a block with a different id (redstone dust, seeds...).</summary>
    public class BlockPlacerItem : Item
    {
        public string blockId;
        public BlockPlacerItem(string block) { blockId = block; }
        public override UseResult UseOn(ref UseOnContext ctx)
        {
            var b = Blocks.Get(blockId);
            if (b == null) return UseResult.Pass;
            return BlockItem.Place(ref ctx, b);
        }
    }

    public class EdiblePlacerItem : FoodItem
    {
        public string blockId;
        public EdiblePlacerItem(string block, int nut, float sat) : base(nut, sat) { blockId = block; }
        public override UseResult UseOn(ref UseOnContext ctx)
        {
            var b = Blocks.Get(blockId);
            if (b == null) return UseResult.Pass;
            var r = BlockItem.Place(ref ctx, b);
            return r;
        }
    }

    public class LilyPadItem : BlockItem
    {
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            if (!p.RaycastBlocks(true, out var hit)) return UseResult.Pass;
            if (!w.IsWater(hit.pos)) return UseResult.Pass;
            var up = hit.pos.Offset(Dir.Up);
            if (!w.IsAir(up)) return UseResult.Fail;
            w.SetState(up, block.DefaultState);
            Sounds.PlayBlock(SoundType.Grass, SoundEvent.Place, up.Center);
            if (!p.IsCreative) s.count--;
            p.SwingArm();
            return UseResult.Success;
        }
    }

    public class SpawnEggItem : Item
    {
        public string mobId;
        public SpawnEggItem(string mob) { mobId = mob; tab = CreativeTab.SpawnEggs; }
        public override UseResult UseOn(ref UseOnContext ctx)
        {
            var w = ctx.world;
            if (w.GetBlock(ctx.pos) is SpawnerBlock) return UseResult.Pass;
            Int3 p = w.GetBlock(ctx.pos).replaceable ? ctx.pos : ctx.pos.Offset(ctx.face);
            var m = MobRegistry.Spawn(w, mobId, new Vector3(p.x + 0.5f, p.y + (ctx.face == Dir.Up || !w.GetBlock(ctx.pos).replaceable ? 0 : 0), p.z + 0.5f), SpawnReason.SpawnEgg);
            if (m == null) return UseResult.Fail;
            if (ctx.stack.customName != null) m.customName = ctx.stack.customName;
            if (!ctx.player.IsCreative) ctx.stack.count--;
            ctx.player.SwingArm();
            return UseResult.Success;
        }
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            if (!p.RaycastBlocks(true, out var hit)) return UseResult.Pass;
            if (!w.IsWater(hit.pos)) return UseResult.Pass;
            var m = MobRegistry.Spawn(w, mobId, hit.pos.Center, SpawnReason.SpawnEgg);
            if (m != null && !p.IsCreative) s.count--;
            return m != null ? UseResult.Success : UseResult.Fail;
        }
    }

    public class EnchantedBookItem : Item
    {
        public EnchantedBookItem() { maxStack = 1; tab = CreativeTab.Tools; rarity = Rarity.Uncommon; glint = true; }
        public override void AppendTooltip(ItemStack s, List<string> lines)
        {
            string st = s.Get("stored_enchants");
            if (st == null) return;
            foreach (var p in st.Split(','))
            {
                var kv = p.Split(':');
                var e = Enchant.Get(kv[0]);
                if (e != null && kv.Length > 1 && int.TryParse(kv[1], out int l)) lines.Add((e.curse ? "§c" : "§7") + e.LevelName(l));
            }
        }
        public static ItemStack Make(Enchant e, int level)
        {
            var s = new ItemStack("enchanted_book", 1);
            s.Set("stored_enchants", e.id + ":" + level);
            return s;
        }
        public static List<(Enchant, int)> Stored(ItemStack s)
        {
            var list = new List<(Enchant, int)>();
            string st = s?.Get("stored_enchants");
            if (st == null) return list;
            foreach (var p in st.Split(','))
            {
                var kv = p.Split(':');
                var e = Enchant.Get(kv[0]);
                if (e != null && kv.Length > 1 && int.TryParse(kv[1], out int l)) list.Add((e, l));
            }
            return list;
        }
    }
}
