using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Data-driven loot tables for structure containers and mob drops.</summary>
    public static class Loot
    {
        public sealed class Entry
        {
            public string item; public int weight, min, max; public string fn; // fn: "enchant", "enchant_book", "damage", "potion:<id>", "stew", "map", "treasure_book"
            public Entry(string item, int weight, int min = 1, int max = 1, string fn = null) { this.item = item; this.weight = weight; this.min = min; this.max = max; this.fn = fn; }
        }
        public sealed class Pool { public int rollsMin, rollsMax; public readonly List<Entry> entries = new List<Entry>(); }
        public sealed class Table { public readonly List<Pool> pools = new List<Pool>(); }

        static readonly Dictionary<string, Table> tables = new Dictionary<string, Table>();
        static bool inited;

        static Pool P(Table t, int rmin, int rmax, params Entry[] es) { var p = new Pool { rollsMin = rmin, rollsMax = rmax }; p.entries.AddRange(es); t.pools.Add(p); return p; }
        static Entry E(string i, int w, int mn = 1, int mx = 1, string fn = null) => new Entry(i, w, mn, mx, fn);
        static Table T(string id) { var t = new Table(); tables[id] = t; return t; }

        static void Init()
        {
            if (inited) return; inited = true;
            var t = T("dungeon");
            P(t, 1, 3, E("saddle", 20), E("golden_apple", 15), E("enchanted_golden_apple", 2), E("music_disc_13", 15), E("music_disc_cat", 15), E("name_tag", 20), E("golden_horse_armor", 10), E("iron_horse_armor", 15), E("diamond_horse_armor", 5), E("enchanted_book", 10, 1, 1, "enchant_book"));
            P(t, 1, 4, E("iron_ingot", 10, 1, 4), E("gold_ingot", 5, 1, 4), E("bread", 20), E("wheat", 20, 1, 4), E("bucket", 10), E("redstone", 15, 1, 4), E("coal", 15, 1, 4), E("melon_seeds", 10, 2, 4), E("pumpkin_seeds", 10, 2, 4), E("beetroot_seeds", 10, 2, 4));
            P(t, 3, 3, E("bone", 10, 1, 8), E("gunpowder", 10, 1, 8), E("rotten_flesh", 10, 1, 8), E("string", 10, 1, 8));
            t = T("mineshaft");
            P(t, 1, 1, E("golden_apple", 20), E("enchanted_golden_apple", 1), E("name_tag", 30), E("enchanted_book", 10, 1, 1, "enchant_book"), E("iron_pickaxe", 5), E("air", 5));
            P(t, 2, 4, E("iron_ingot", 10, 1, 5), E("gold_ingot", 5, 1, 3), E("redstone", 5, 4, 9), E("lapis_lazuli", 5, 4, 9), E("diamond", 3, 1, 2), E("coal", 10, 3, 8), E("bread", 15, 1, 3), E("glow_berries", 15, 3, 6), E("melon_seeds", 10, 2, 4), E("pumpkin_seeds", 10, 2, 4), E("beetroot_seeds", 10, 2, 4));
            P(t, 3, 3, E("rail", 20, 4, 8), E("powered_rail", 5, 1, 4), E("detector_rail", 5, 1, 4), E("activator_rail", 5, 1, 4), E("torch", 15, 1, 16));
            t = T("desert_pyramid");
            P(t, 2, 4, E("diamond", 5, 1, 3), E("iron_ingot", 15, 1, 5), E("gold_ingot", 15, 2, 7), E("emerald", 15, 1, 3), E("bone", 25, 4, 6), E("spider_eye", 25, 1, 3), E("rotten_flesh", 25, 3, 7), E("saddle", 20), E("iron_horse_armor", 15), E("golden_horse_armor", 10), E("diamond_horse_armor", 5), E("enchanted_book", 20, 1, 1, "enchant_book"), E("golden_apple", 20), E("enchanted_golden_apple", 2), E("air", 15));
            P(t, 4, 4, E("bone", 10, 1, 8), E("gunpowder", 10, 1, 8), E("rotten_flesh", 10, 1, 8), E("string", 10, 1, 8), E("sand", 10, 1, 8));
            t = T("jungle_temple");
            P(t, 2, 6, E("diamond", 3, 1, 3), E("iron_ingot", 10, 1, 5), E("gold_ingot", 15, 2, 7), E("emerald", 2, 1, 3), E("bone", 20, 4, 6), E("rotten_flesh", 16, 3, 7), E("saddle", 3), E("iron_horse_armor", 1), E("golden_horse_armor", 1), E("diamond_horse_armor", 1), E("enchanted_book", 1, 1, 1, "enchant_book"), E("bamboo", 15, 1, 3));
            t = T("stronghold_corridor");
            P(t, 2, 3, E("ender_pearl", 10), E("diamond", 3, 1, 3), E("iron_ingot", 10, 1, 5), E("gold_ingot", 5, 1, 3), E("redstone", 5, 4, 9), E("bread", 15, 1, 3), E("apple", 15, 1, 3), E("iron_pickaxe", 5), E("iron_sword", 5), E("iron_chestplate", 5), E("iron_helmet", 5), E("iron_leggings", 5), E("iron_boots", 5), E("golden_apple", 1), E("saddle", 1), E("iron_horse_armor", 1), E("golden_horse_armor", 1), E("diamond_horse_armor", 1), E("enchanted_book", 1, 1, 1, "enchant_book"));
            t = T("stronghold_crossing");
            P(t, 1, 4, E("iron_ingot", 10, 1, 5), E("gold_ingot", 5, 1, 3), E("redstone", 5, 4, 9), E("coal", 10, 3, 8), E("bread", 15, 1, 3), E("apple", 15, 1, 3), E("iron_pickaxe", 1), E("enchanted_book", 1, 1, 1, "enchant_book"));
            t = T("stronghold_library");
            P(t, 2, 10, E("book", 20, 1, 3), E("paper", 20, 2, 7), E("map", 1), E("compass", 1), E("enchanted_book", 10, 1, 1, "enchant_book"));
            t = T("village_house");
            P(t, 3, 8, E("bread", 10, 1, 4), E("apple", 10, 1, 5), E("wheat", 10, 1, 7), E("wheat_seeds", 10, 1, 5), E("potato", 10, 1, 7), E("carrot", 10, 1, 7), E("emerald", 2, 1, 1), E("book", 3, 1, 1), E("feather", 5, 1, 1), E("egg", 10, 1, 2), E("torch", 10, 1, 4), E("oak_sapling", 5, 1, 2), E("beetroot_seeds", 5, 1, 5));
            t = T("village_weaponsmith");
            P(t, 3, 8, E("diamond", 3, 1, 3), E("iron_ingot", 10, 1, 5), E("gold_ingot", 5, 1, 3), E("bread", 15, 1, 3), E("apple", 15, 1, 3), E("iron_pickaxe", 5), E("iron_sword", 5), E("iron_chestplate", 5), E("iron_helmet", 5), E("iron_leggings", 5), E("iron_boots", 5), E("obsidian", 5, 3, 7), E("oak_sapling", 5, 3, 7), E("saddle", 3), E("iron_horse_armor", 1), E("golden_horse_armor", 1), E("diamond_horse_armor", 1));
            t = T("village_toolsmith");
            P(t, 3, 8, E("diamond", 1, 1, 3), E("iron_ingot", 5, 1, 5), E("gold_ingot", 1, 1, 3), E("bread", 15, 1, 3), E("iron_pickaxe", 5), E("coal", 1, 1, 3), E("stick", 20, 1, 3), E("iron_shovel", 5));
            t = T("village_armorer");
            P(t, 1, 5, E("iron_ingot", 2, 1, 3), E("bread", 4, 1, 4), E("iron_helmet", 1), E("emerald", 1, 1, 1));
            t = T("village_temple");
            P(t, 3, 8, E("redstone", 2, 1, 4), E("bread", 7, 1, 4), E("rotten_flesh", 7, 1, 4), E("lapis_lazuli", 1, 1, 4), E("gold_ingot", 1, 1, 4), E("emerald", 1, 1, 4));
            t = T("buried_treasure");
            P(t, 1, 1, E("heart_of_the_sea", 1));
            P(t, 5, 8, E("iron_ingot", 20, 1, 4), E("gold_ingot", 10, 1, 4), E("tnt", 5, 1, 2));
            P(t, 1, 3, E("emerald", 5, 4, 8), E("diamond", 5, 1, 2), E("prismarine_crystals", 5, 1, 5));
            P(t, 0, 1, E("leather_chestplate", 1), E("iron_sword", 1));
            P(t, 2, 2, E("cooked_cod", 1, 2, 4), E("cooked_salmon", 1, 2, 4));
            t = T("shipwreck_supply");
            P(t, 3, 10, E("paper", 8, 1, 12), E("potato", 7, 2, 6), E("poisonous_potato", 7, 2, 6), E("carrot", 7, 4, 8), E("wheat", 7, 8, 21), E("coal", 6, 2, 8), E("rotten_flesh", 5, 5, 24), E("pumpkin", 2, 1, 3), E("bamboo", 2, 1, 3), E("gunpowder", 3, 1, 5), E("tnt", 1, 1, 2), E("leather_helmet", 3, 1, 1, "enchant"), E("leather_chestplate", 3, 1, 1, "enchant"), E("leather_leggings", 3, 1, 1, "enchant"), E("leather_boots", 3, 1, 1, "enchant"));
            t = T("shipwreck_treasure");
            P(t, 3, 6, E("iron_ingot", 90, 1, 5), E("gold_ingot", 10, 1, 5), E("emerald", 40, 1, 5), E("diamond", 5), E("experience_bottle", 5));
            P(t, 2, 5, E("iron_nugget", 50, 1, 10), E("gold_nugget", 10, 1, 10), E("lapis_lazuli", 20, 1, 10));
            t = T("ruined_portal");
            P(t, 4, 8, E("obsidian", 40, 1, 2), E("flint", 40, 1, 4), E("iron_nugget", 40, 9, 18), E("flint_and_steel", 40), E("fire_charge", 40), E("golden_apple", 15), E("gold_nugget", 15, 4, 24), E("golden_sword", 15, 1, 1, "enchant"), E("golden_axe", 15, 1, 1, "enchant"), E("golden_hoe", 15, 1, 1, "enchant"), E("golden_shovel", 15, 1, 1, "enchant"), E("golden_pickaxe", 15, 1, 1, "enchant"), E("golden_boots", 15, 1, 1, "enchant"), E("golden_chestplate", 15, 1, 1, "enchant"), E("golden_helmet", 15, 1, 1, "enchant"), E("golden_leggings", 15, 1, 1, "enchant"), E("glistering_melon_slice", 5, 4, 12), E("golden_horse_armor", 5), E("light_weighted_pressure_plate", 5), E("golden_carrot", 5, 4, 12), E("clock", 5), E("gold_ingot", 5, 2, 8), E("bell", 1), E("enchanted_golden_apple", 1), E("gold_block", 1, 1, 2));
            t = T("nether_fortress");
            P(t, 2, 4, E("diamond", 5, 1, 3), E("iron_ingot", 5, 1, 5), E("gold_ingot", 15, 1, 3), E("golden_sword", 5), E("golden_chestplate", 5), E("flint_and_steel", 5), E("nether_wart", 5, 3, 7), E("saddle", 10), E("golden_horse_armor", 8), E("iron_horse_armor", 5), E("diamond_horse_armor", 3), E("obsidian", 2, 2, 4));
            t = T("bastion_treasure");
            P(t, 3, 3, E("netherite_ingot", 15), E("ancient_debris", 10), E("netherite_scrap", 8), E("ancient_debris", 4, 2, 2), E("diamond_sword", 6, 1, 1, "enchant"), E("diamond_chestplate", 6, 1, 1, "enchant"), E("diamond_helmet", 6, 1, 1, "enchant"), E("diamond_leggings", 6, 1, 1, "enchant"), E("diamond_boots", 6, 1, 1, "enchant"), E("diamond", 6, 2, 6), E("enchanted_golden_apple", 2));
            P(t, 3, 4, E("spectral_arrow", 1, 12, 25), E("gold_block", 1, 2, 5), E("iron_block", 1, 2, 5), E("gold_ingot", 1, 3, 9), E("iron_ingot", 1, 3, 9), E("crying_obsidian", 1, 3, 5), E("quartz", 1, 8, 23), E("gilded_blackstone", 1, 5, 15), E("magma_cream", 1, 3, 8));
            t = T("bastion_other");
            P(t, 1, 1, E("diamond_pickaxe", 6, 1, 1, "enchant"), E("diamond_shovel", 6), E("crossbow", 6, 1, 1, "enchant"), E("ancient_debris", 12), E("netherite_scrap", 4), E("spectral_arrow", 10, 10, 22), E("piglin_banner_pattern", 9), E("music_disc_pigstep", 5), E("golden_carrot", 12, 6, 17), E("golden_apple", 9), E("enchanted_book", 10, 1, 1, "enchant_book"));
            P(t, 2, 2, E("iron_sword", 2, 1, 1, "enchant"), E("iron_block", 2), E("golden_boots", 1, 1, 1, "enchant"), E("golden_axe", 1, 1, 1, "enchant"), E("gold_block", 2), E("crossbow", 1), E("gold_ingot", 2, 1, 6), E("iron_ingot", 2, 1, 6), E("golden_sword", 2), E("golden_chestplate", 2), E("golden_helmet", 2), E("golden_leggings", 2), E("golden_boots", 2), E("crying_obsidian", 2, 1, 5));
            P(t, 3, 4, E("gilded_blackstone", 2, 1, 5), E("chain", 2, 2, 10), E("magma_cream", 2, 2, 6), E("bone_block", 2, 3, 6), E("iron_nugget", 1, 2, 8), E("obsidian", 1, 4, 6), E("gold_nugget", 1, 2, 8), E("string", 1, 4, 6), E("arrow", 2, 5, 17), E("cooked_porkchop", 1, 1, 1));
            T("bastion_bridge").pools.AddRange(tables["bastion_other"].pools);
            T("bastion_hoglin_stable").pools.AddRange(tables["bastion_other"].pools);
            t = T("end_city_treasure");
            P(t, 2, 6, E("diamond", 5, 2, 7), E("iron_ingot", 10, 4, 8), E("gold_ingot", 15, 2, 7), E("emerald", 2, 2, 6), E("beetroot_seeds", 5, 1, 10), E("saddle", 3), E("iron_horse_armor", 1), E("golden_horse_armor", 1), E("diamond_horse_armor", 1), E("diamond_sword", 3, 1, 1, "enchant"), E("diamond_boots", 3, 1, 1, "enchant"), E("diamond_chestplate", 3, 1, 1, "enchant"), E("diamond_leggings", 3, 1, 1, "enchant"), E("diamond_helmet", 3, 1, 1, "enchant"), E("diamond_pickaxe", 3, 1, 1, "enchant"), E("diamond_shovel", 3, 1, 1, "enchant"), E("iron_sword", 3, 1, 1, "enchant"), E("iron_boots", 3, 1, 1, "enchant"), E("iron_chestplate", 3, 1, 1, "enchant"), E("iron_leggings", 3, 1, 1, "enchant"), E("iron_helmet", 3, 1, 1, "enchant"), E("iron_pickaxe", 3, 1, 1, "enchant"), E("iron_shovel", 3, 1, 1, "enchant"));
            t = T("igloo");
            P(t, 2, 8, E("apple", 15, 1, 3), E("coal", 15, 1, 4), E("gold_nugget", 10, 1, 3), E("stone_axe", 2), E("rotten_flesh", 10), E("emerald", 1), E("wheat", 10, 2, 3));
            P(t, 1, 1, E("golden_apple", 1));
            t = T("pillager_outpost");
            P(t, 0, 1, E("crossbow", 1));
            P(t, 2, 3, E("wheat", 7, 3, 5), E("potato", 5, 2, 5), E("carrot", 5, 3, 5));
            P(t, 1, 3, E("dark_oak_log", 1, 2, 3));
            P(t, 2, 3, E("experience_bottle", 7), E("string", 4, 1, 6), E("arrow", 4, 2, 7), E("tripwire_hook", 3, 1, 3), E("iron_ingot", 3, 1, 3), E("enchanted_book", 1, 1, 1, "enchant_book"));
            t = T("ancient_city");
            P(t, 5, 10, E("enchanted_golden_apple", 1), E("music_disc_otherside", 1), E("compass", 2), E("sculk_catalyst", 2), E("name_tag", 2), E("diamond_hoe", 2, 1, 1, "enchant"), E("lead", 2), E("diamond_horse_armor", 2), E("saddle", 2), E("music_disc_13", 2), E("music_disc_cat", 2), E("diamond_leggings", 2, 1, 1, "enchant"), E("enchanted_book", 3, 1, 1, "enchant_book"), E("sculk", 3, 4, 10), E("sculk_sensor", 3, 1, 3), E("candle", 3, 1, 4), E("amethyst_shard", 3, 1, 15), E("experience_bottle", 3, 1, 3), E("glow_berries", 3, 1, 15), E("iron_leggings", 3, 1, 1, "enchant"), E("echo_shard", 4, 1, 4), E("disc_fragment_5", 4, 1, 3), E("potion", 5, 1, 3, "potion:strong_regeneration"), E("book", 5, 3, 10), E("bone", 5, 1, 15), E("soul_torch", 5, 1, 15), E("coal", 7, 6, 15));
            t = T("trial_chambers");
            P(t, 2, 4, E("emerald", 4, 2, 4), E("arrow", 4, 4, 14), E("iron_ingot", 3, 1, 3), E("honey_bottle", 2, 1, 1), E("trial_key", 2), E("diamond", 1, 1, 2), E("golden_carrot", 3, 1, 3), E("bread", 3, 1, 3), E("wind_charge", 3, 4, 12), E("ominous_trial_key", 1));
            t = T("underwater_ruin");
            P(t, 2, 8, E("coal", 10, 1, 4), E("stone_axe", 2), E("rotten_flesh", 5), E("emerald", 1), E("wheat", 10, 2, 3), E("gold_nugget", 5, 1, 3), E("leather_chestplate", 1), E("golden_helmet", 1), E("fishing_rod", 5, 1, 1, "enchant"), E("map", 5));
            t = T("trail_ruins");
            P(t, 1, 1, E("emerald", 2), E("wheat", 2), E("wooden_hoe", 2), E("clay", 2), E("brick", 2), E("yellow_dye", 2), E("blue_dye", 2), E("light_blue_dye", 2), E("white_dye", 2), E("orange_dye", 2), E("red_candle", 2), E("green_candle", 2), E("purple_candle", 2), E("brown_candle", 2), E("magenta_stained_glass_pane", 1), E("pink_stained_glass_pane", 1), E("blue_stained_glass_pane", 1), E("light_blue_stained_glass_pane", 1), E("red_stained_glass_pane", 1), E("yellow_stained_glass_pane", 1), E("purple_stained_glass_pane", 1), E("dead_bush", 1), E("flower_pot", 1), E("string", 1), E("lead", 1));
            t = T("woodland_mansion");
            P(t, 1, 3, E("lead", 20), E("golden_apple", 15), E("enchanted_golden_apple", 2), E("music_disc_13", 15), E("music_disc_cat", 15), E("name_tag", 20), E("chainmail_chestplate", 10), E("diamond_hoe", 15), E("diamond_chestplate", 5), E("enchanted_book", 10, 1, 1, "enchant_book"));
            P(t, 1, 4, E("iron_ingot", 10, 1, 4), E("gold_ingot", 5, 1, 4), E("bread", 20), E("wheat", 20, 1, 4), E("bucket", 10), E("redstone", 15, 1, 4), E("coal", 15, 1, 4), E("melon_seeds", 10, 2, 4), E("pumpkin_seeds", 10, 2, 4), E("beetroot_seeds", 10, 2, 4));
            t = T("spawn_bonus_chest");
            P(t, 1, 1, E("stone_axe", 1), E("wooden_axe", 3));
            P(t, 1, 1, E("stone_pickaxe", 1), E("wooden_pickaxe", 3));
            P(t, 3, 3, E("apple", 5, 1, 2), E("bread", 3, 1, 2), E("salmon", 3, 1, 2));
            P(t, 4, 4, E("stick", 10, 1, 12), E("oak_planks", 10, 1, 12), E("oak_log", 3, 1, 3), E("spruce_log", 3, 1, 3), E("birch_log", 3, 1, 3));
            T("chest").pools.AddRange(tables["dungeon"].pools);
            T("barrel").pools.AddRange(tables["village_house"].pools);
        }

        public static IEnumerable<string> TableIds { get { Init(); return tables.Keys; } }

        public static void FillContainer(string table, ItemStack[] items, ref RNG rng)
        {
            Init();
            if (table == null || !tables.TryGetValue(table, out var t)) { if (table != null) Debug.LogWarning("Unknown loot table " + table); return; }
            var stacks = new List<ItemStack>();
            foreach (var p in t.pools)
            {
                int rolls = rng.Range(p.rollsMin, p.rollsMax);
                int total = 0; foreach (var e in p.entries) total += e.weight;
                if (total <= 0) continue;
                for (int r = 0; r < rolls; r++)
                {
                    int pick = rng.Next(total);
                    Entry chosen = null;
                    foreach (var e in p.entries) { pick -= e.weight; if (pick < 0) { chosen = e; break; } }
                    if (chosen == null || chosen.item == "air") continue;
                    var s = MakeStack(chosen, ref rng);
                    if (s != null) stacks.Add(s);
                }
            }
            // split some stacks and scatter into random empty slots (vanilla-like distribution)
            var empty = new List<int>();
            for (int i = 0; i < items.Length; i++) if (items[i] == null) empty.Add(i);
            rng.Shuffle(empty);
            var expanded = new List<ItemStack>();
            foreach (var s in stacks)
            {
                if (s.count > 1 && rng.Chance(0.5f) && expanded.Count + stacks.Count < empty.Count) { var half = s.Split(Mathf.Max(1, s.count / 2)); expanded.Add(half); }
                expanded.Add(s);
            }
            for (int i = 0; i < expanded.Count && i < empty.Count; i++) items[empty[i]] = expanded[i];
        }

        static ItemStack MakeStack(Entry e, ref RNG rng)
        {
            var it = Items.Get(e.item);
            if (it == null) return null;
            var s = new ItemStack(it, Mathf.Min(it.maxStack, rng.Range(e.min, e.max)));
            if (e.fn == null) return s;
            if (e.fn == "enchant") EnchantRandomly(s, ref rng, 5 + rng.Next(25), false);
            else if (e.fn == "enchant_book")
            {
                var list = new List<Enchant>();
                foreach (var en in Enchant.All) if (!en.curse || rng.Chance(0.2f)) list.Add(en);
                var pick = list[rng.Next(list.Count)];
                s = EnchantedBookItem.Make(pick, rng.Range(1, pick.maxLevel));
            }
            else if (e.fn.StartsWith("potion:")) s.Set("potion", e.fn.Substring(7));
            return s;
        }

        public static void EnchantRandomly(ItemStack s, ref RNG rng, int level, bool treasure)
        {
            var sel = Enchant.Select(ref rng, s.item, level, treasure);
            foreach (var (e, l) in sel) s.AddEnchant(e, l);
        }

        /// <summary>Blocks that drop themselves with Silk Touch (instead of their normal drop).</summary>
        public static bool SilkTouchable(Block b)
        {
            if (b == null || b.item == null) return false;
            if (b.hardness < 0) return false;
            if (b is FluidBlock || b is FireBlock || b is PortalBlock) return false;
            string id = b.id;
            if (id.EndsWith("_bed") || id.EndsWith("_door") || id == "spawner" || id == "budding_amethyst" || id == "farmland" || id == "dirt_path") return false;
            return true;
        }

        // ------------------------------------------------------------------ mob drops
        public static void MobDrops(Mob m, int looting, List<ItemStack> into, ref RNG rng)
        {
            var d = m.def;
            if (d.drops == null) return;
            bool baby = m.IsBaby;
            if (baby) return;
            foreach (var dr in d.drops)
            {
                if (dr.chance < 1f && rng.NextFloat() > dr.chance + looting * dr.lootingChance) continue;
                if (dr.playerKillOnly && !(m.lastAttacker is Player)) continue;
                int n = rng.Range(dr.min, dr.max) + (looting > 0 && dr.max > 0 ? rng.Range(0, looting) : 0);
                if (n <= 0) continue;
                string id = dr.item;
                if (m.onFire && dr.cooked != null) id = dr.cooked;
                if (id == "$wool") id = (m is SheepMob sh ? sh.ColorName : "white") + "_wool";
                var it = Items.Get(id);
                if (it == null) continue;
                into.Add(new ItemStack(it, n));
            }
        }
    }
}
