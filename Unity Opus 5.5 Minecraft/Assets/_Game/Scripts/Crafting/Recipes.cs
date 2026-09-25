using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace MCR
{
    public sealed class Ingredient
    {
        public readonly HashSet<Item> items = new HashSet<Item>();
        public string display;
        public bool Matches(ItemStack s) => s != null && !s.IsEmpty && items.Contains(s.item);
        public Item First => items.FirstOrDefault();
        public static Ingredient Of(string spec)
        {
            var ing = new Ingredient { display = spec };
            if (spec.StartsWith("#")) { foreach (var it in Tags.Get(spec.Substring(1))) ing.items.Add(it); }
            else foreach (var part in spec.Split('|')) { var it = Items.Get(part); if (it != null) ing.items.Add(it); }
            return ing.items.Count > 0 ? ing : null;
        }
    }

    public static class Tags
    {
        static readonly Dictionary<string, List<Item>> tags = new Dictionary<string, List<Item>>();
        public static IEnumerable<Item> Get(string tag) => tags.TryGetValue(tag, out var l) ? l : Enumerable.Empty<Item>();
        public static bool Has(string tag, Item it) => it != null && tags.TryGetValue(tag, out var l) && l.Contains(it);
        public static void Add(string tag, string itemId) { var it = Items.Get(itemId); if (it == null) return; if (!tags.TryGetValue(tag, out var l)) tags[tag] = l = new List<Item>(); if (!l.Contains(it)) l.Add(it); }
        public static void Init()
        {
            tags.Clear();
            foreach (var w in Blocks.WoodTypes)
            {
                Add("planks", w + "_planks");
                bool nether = w == "crimson" || w == "warped";
                string log = nether ? w + "_stem" : w == "bamboo" ? "bamboo_block" : w + "_log";
                Add("logs", log); Add("logs", "stripped_" + log);
                Add(w + "_logs", log); Add(w + "_logs", "stripped_" + log);
                if (!nether && w != "bamboo") { Add("logs", w + "_wood"); Add("logs", "stripped_" + w + "_wood"); Add(w + "_logs", w + "_wood"); Add(w + "_logs", "stripped_" + w + "_wood"); }
                else if (nether) { Add("logs", w + "_hyphae"); Add("logs", "stripped_" + w + "_hyphae"); Add(w + "_logs", w + "_hyphae"); Add(w + "_logs", "stripped_" + w + "_hyphae"); }
                if (!nether) { Add("burnable_logs", log); Add("burnable_logs", "stripped_" + log); if (w != "bamboo") { Add("burnable_logs", w + "_wood"); Add("burnable_logs", "stripped_" + w + "_wood"); } }
                Add("wooden_slabs", w + "_slab");
            }
            foreach (var c in Blocks.Colors) { Add("wool", c + "_wool"); Add("dyes", c + "_dye"); Add("terracotta", c + "_terracotta"); }
            Add("terracotta", "terracotta");
            Add("stone_tool_materials", "cobblestone"); Add("stone_tool_materials", "blackstone"); Add("stone_tool_materials", "cobbled_deepslate");
            Add("stone_crafting_materials", "cobblestone"); Add("stone_crafting_materials", "blackstone"); Add("stone_crafting_materials", "cobbled_deepslate");
            Add("coals", "coal"); Add("coals", "charcoal");
            Add("sand", "sand"); Add("sand", "red_sand");
            Add("soul_fire_base", "soul_sand"); Add("soul_fire_base", "soul_soil");
            Add("fish", "cod"); Add("fish", "salmon");
            Add("leaves", "oak_leaves");
            foreach (var w in Blocks.WoodTypes) { var l = Items.Get(w + "_leaves"); if (l != null) Add("leaves", w + "_leaves"); }
            Add("small_flowers", "dandelion"); Add("small_flowers", "poppy");
        }
    }

    public abstract class Recipe
    {
        public string id;
        public ItemStack result;
        public string category = "misc";
        public abstract bool Matches(CraftingGrid g);
        public virtual ItemStack Assemble(CraftingGrid g) => result.Copy();
        public virtual ItemStack[] Remainders(CraftingGrid g)
        {
            var r = new ItemStack[g.Size];
            for (int i = 0; i < g.Size; i++)
            {
                var s = g.items[i];
                if (s == null) continue;
                if (s.item.craftRemainder != null) r[i] = new ItemStack(s.item.craftRemainder, 1);
                else if (s.item.id == "honey_bottle" || s.item.id == "dragon_breath") r[i] = new ItemStack("glass_bottle", 1);
            }
            return r;
        }
        public abstract List<Ingredient> AllIngredients();
        public virtual int GridW => 3;
    }

    public sealed class ShapedRecipe : Recipe
    {
        public int w, h; public Ingredient[] pattern;
        public override int GridW => w;
        public override bool Matches(CraftingGrid g)
        {
            for (int ox = 0; ox <= g.w - w; ox++)
                for (int oy = 0; oy <= g.h - h; oy++)
                {
                    if (MatchAt(g, ox, oy, false)) return true;
                    if (MatchAt(g, ox, oy, true)) return true;
                }
            return false;
        }
        bool MatchAt(CraftingGrid g, int ox, int oy, bool mirror)
        {
            for (int y = 0; y < g.h; y++)
                for (int x = 0; x < g.w; x++)
                {
                    int px = x - ox, py = y - oy;
                    Ingredient ing = null;
                    if (px >= 0 && py >= 0 && px < w && py < h) ing = pattern[py * w + (mirror ? w - 1 - px : px)];
                    var s = g.At(x, y);
                    if (ing == null) { if (s != null && !s.IsEmpty) return false; }
                    else if (!ing.Matches(s)) return false;
                }
            return true;
        }
        public override List<Ingredient> AllIngredients() => pattern.Where(p => p != null).ToList();
    }

    public sealed class ShapelessRecipe : Recipe
    {
        public List<Ingredient> ings = new List<Ingredient>();
        public override bool Matches(CraftingGrid g)
        {
            var remaining = new List<Ingredient>(ings);
            for (int i = 0; i < g.Size; i++)
            {
                var s = g.items[i];
                if (s == null || s.IsEmpty) continue;
                int idx = remaining.FindIndex(r => r.Matches(s));
                if (idx < 0) return false;
                remaining.RemoveAt(idx);
            }
            return remaining.Count == 0;
        }
        public override List<Ingredient> AllIngredients() => ings;
    }

    /// <summary>Special recipes computed dynamically (armor dye, map cloning, firework, repair...).</summary>
    public sealed class SpecialRecipe : Recipe
    {
        public Func<CraftingGrid, ItemStack> fn;
        public override bool Matches(CraftingGrid g) => fn(g) != null;
        public override ItemStack Assemble(CraftingGrid g) => fn(g);
        public override List<Ingredient> AllIngredients() => new List<Ingredient>();
    }

    public sealed class SmeltingRecipe
    {
        public Ingredient input; public ItemStack result; public float xp; public string kind; // furnace/smoker/blast_furnace/campfire
    }

    public sealed class StonecutterRecipe { public Item input; public ItemStack result; }
    public sealed class SmithingRecipe { public string template, baseItem, addition; public string result; }

    public static partial class Recipes
    {
        public static readonly List<Recipe> Crafting = new List<Recipe>();
        public static readonly List<SmeltingRecipe> Smelting = new List<SmeltingRecipe>();
        public static readonly List<StonecutterRecipe> Stonecutting = new List<StonecutterRecipe>();
        public static readonly List<SmithingRecipe> Smithing = new List<SmithingRecipe>();
        static readonly Dictionary<Item, int> fuel = new Dictionary<Item, int>();
        public static bool Initialized;
        static int autoId;

        public static Recipe FindCrafting(CraftingGrid g, World w)
        {
            bool any = false;
            for (int i = 0; i < g.Size; i++) if (g.items[i] != null && !g.items[i].IsEmpty) { any = true; break; }
            if (!any) return null;
            foreach (var r in Crafting) if (r.Matches(g)) return r;
            return null;
        }

        public static SmeltingRecipe FindSmelting(Item input, string kind)
        {
            if (input == null) return null;
            foreach (var r in Smelting)
            {
                if (!r.input.items.Contains(input)) continue;
                if (kind == "furnace" && r.kind == "furnace") return r;
                if (kind == r.kind) return r;
                if (kind == "blast_furnace" && r.kind == "furnace" && r.result.item != null && IsOreOrArmor(input)) return r;
                if ((kind == "smoker" || kind == "campfire") && r.kind == "furnace" && input is FoodItem) return r;
            }
            return null;
        }
        static bool IsOreOrArmor(Item it) => it.id.Contains("ore") || it.id.StartsWith("raw_") || it is ArmorItem || it is ToolItem || it.id == "ancient_debris";

        public static int FuelValue(Item it)
        {
            if (it == null) return 0;
            if (fuel.TryGetValue(it, out int v)) return v;
            return it.fuelTicks;
        }

        // ------------------------------------------------------------------ registration helpers
        static string NextId(string res) => res + "_" + (autoId++);
        public static void Shaped(string result, int count, string[] rows, params object[] keys)
        {
            var map = new Dictionary<char, Ingredient>();
            for (int i = 0; i + 1 < keys.Length; i += 2)
            {
                var ing = Ingredient.Of((string)keys[i + 1]);
                if (ing == null) return;
                map[(char)keys[i]] = ing;
            }
            var it = Items.Get(result);
            if (it == null) return;
            int h = rows.Length, w = rows.Max(r => r.Length);
            var pat = new Ingredient[w * h];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    char ch = x < rows[y].Length ? rows[y][x] : ' ';
                    pat[y * w + x] = ch == ' ' ? null : (map.TryGetValue(ch, out var ing) ? ing : null);
                    if (ch != ' ' && !map.ContainsKey(ch)) return;
                }
            Crafting.Add(new ShapedRecipe { id = NextId(result), result = new ItemStack(it, count), w = w, h = h, pattern = pat });
        }
        public static void Shapeless(string result, int count, params string[] ings)
        {
            var it = Items.Get(result);
            if (it == null) return;
            var r = new ShapelessRecipe { id = NextId(result), result = new ItemStack(it, count) };
            foreach (var i in ings) { var ing = Ingredient.Of(i); if (ing == null) return; r.ings.Add(ing); }
            Crafting.Add(r);
        }
        public static void Smelt(string input, string result, float xp, bool food = false, bool ore = false, int count = 1)
        {
            var ing = Ingredient.Of(input); var it = Items.Get(result);
            if (ing == null || it == null) return;
            Smelting.Add(new SmeltingRecipe { input = ing, result = new ItemStack(it, count), xp = xp, kind = "furnace" });
            if (food) { Smelting.Add(new SmeltingRecipe { input = ing, result = new ItemStack(it, count), xp = xp, kind = "smoker" }); Smelting.Add(new SmeltingRecipe { input = ing, result = new ItemStack(it, count), xp = xp, kind = "campfire" }); }
            if (ore) Smelting.Add(new SmeltingRecipe { input = ing, result = new ItemStack(it, count), xp = xp, kind = "blast_furnace" });
        }
        public static void Cut(string input, string result, int count = 1)
        {
            var i = Items.Get(input); var r = Items.Get(result);
            if (i == null || r == null) return;
            Stonecutting.Add(new StonecutterRecipe { input = i, result = new ItemStack(r, count) });
        }
        public static void Fuel(string id, int ticks) { var it = Items.Get(id); if (it != null) fuel[it] = ticks; }

        public static List<Recipe> RecipesFor(Item it) => Crafting.Where(r => r.result.item == it).ToList();
        public static List<StonecutterRecipe> CutsFor(Item input) => Stonecutting.Where(r => r.input == input).ToList();

        public static void Init()
        {
            if (Initialized) return;
            Crafting.Clear(); Smelting.Clear(); Stonecutting.Clear(); Smithing.Clear(); fuel.Clear(); autoId = 0;
            Tags.Init();
            RegisterFamilies();
            RegisterCore();
            RegisterSmelting();
            RegisterSpecial();
            Initialized = true;
            Debug.Log($"[Recipes] {Crafting.Count} crafting, {Smelting.Count} smelting, {Stonecutting.Count} stonecutting, {Smithing.Count} smithing");
        }

        static bool Has(string id) => Items.Get(id) != null;

        static void RegisterFamilies()
        {
            // ---- wood families
            foreach (var w in Blocks.WoodTypes)
            {
                bool nether = w == "crimson" || w == "warped", bamboo = w == "bamboo";
                string planks = w + "_planks";
                if (bamboo) Shaped(planks, 2, new[] { "L" }, 'L', "bamboo_block|stripped_bamboo_block");
                else Shapeless(planks, 4, "#" + w + "_logs");
                if (!nether && !bamboo) { Shaped(w + "_wood", 3, new[] { "LL", "LL" }, 'L', w + "_log"); Shaped("stripped_" + w + "_wood", 3, new[] { "LL", "LL" }, 'L', "stripped_" + w + "_log"); }
                if (nether) { Shaped(w + "_hyphae", 3, new[] { "LL", "LL" }, 'L', w + "_stem"); Shaped("stripped_" + w + "_hyphae", 3, new[] { "LL", "LL" }, 'L', "stripped_" + w + "_stem"); }
                Shaped(w + "_stairs", 4, new[] { "P  ", "PP ", "PPP" }, 'P', planks);
                Shaped(w + "_slab", 6, new[] { "PPP" }, 'P', planks);
                Shaped(w + "_fence", 3, new[] { "PSP", "PSP" }, 'P', planks, 'S', "stick");
                Shaped(w + "_fence_gate", 1, new[] { "SPS", "SPS" }, 'P', planks, 'S', "stick");
                Shaped(w + "_door", 3, new[] { "PP", "PP", "PP" }, 'P', planks);
                Shaped(w + "_trapdoor", 2, new[] { "PPP", "PPP" }, 'P', planks);
                Shaped(w + "_pressure_plate", 1, new[] { "PP" }, 'P', planks);
                Shapeless(w + "_button", 1, planks);
                if (!nether) { if (bamboo) Shaped("bamboo_raft", 1, new[] { "P P", "PPP" }, 'P', planks); else Shaped(w + "_boat", 1, new[] { "P P", "PPP" }, 'P', planks); }
                if (!nether) { if (bamboo) Shapeless("bamboo_chest_raft", 1, "bamboo_raft", "chest"); else Shapeless(w + "_chest_boat", 1, w + "_boat", "chest"); }
                Shaped(w + "_shelf", 6, new[] { "SSS", "   ", "SSS" }, 'S', "stripped_" + (nether ? w + "_stem" : bamboo ? "bamboo_block" : w + "_log"));
            }
            Shaped("bamboo_mosaic", 1, new[] { "S", "S" }, 'S', "bamboo_slab");
            Shaped("bamboo_block", 1, new[] { "BBB", "BBB", "BBB" }, 'B', "bamboo");
            // ---- stone-like families: slabs, stairs, walls + stonecutting
            foreach (var b in Blocks.All)
            {
                if (b is StairsBlock st && st.full != null && !(st.full.id.EndsWith("_planks") || st.full.id == "bamboo_mosaic"))
                {
                    Shaped(b.id, 4, new[] { "P  ", "PP ", "PPP" }, 'P', st.full.id);
                    if (st.full.tool == ToolType.Pickaxe) Cut(st.full.id, b.id, 1);
                }
                else if (b is SlabBlock sl && sl.full != null && !(sl.full.id.EndsWith("_planks") || sl.full.id == "bamboo_mosaic"))
                {
                    Shaped(b.id, 6, new[] { "PPP" }, 'P', sl.full.id);
                    if (sl.full.tool == ToolType.Pickaxe) Cut(sl.full.id, b.id, 2);
                }
            }
            foreach (var b in Blocks.All)
                if (b is WallBlock)
                {
                    string src = WallSource(b.id);
                    if (src != null) { Shaped(b.id, 6, new[] { "PPP", "PPP" }, 'P', src); Cut(src, b.id, 1); }
                }
            // stone -> derivatives
            foreach (var (from, to) in new[] { ("stone", "stone_bricks"), ("stone", "chiseled_stone_bricks"), ("stone_bricks", "chiseled_stone_bricks"), ("cobbled_deepslate", "polished_deepslate"), ("cobbled_deepslate", "deepslate_bricks"), ("cobbled_deepslate", "deepslate_tiles"), ("cobbled_deepslate", "chiseled_deepslate"),
                ("polished_deepslate", "deepslate_bricks"), ("deepslate_bricks", "deepslate_tiles"), ("granite", "polished_granite"), ("diorite", "polished_diorite"), ("andesite", "polished_andesite"), ("tuff", "polished_tuff"), ("tuff", "tuff_bricks"), ("tuff", "chiseled_tuff"), ("tuff_bricks", "chiseled_tuff_bricks"),
                ("sandstone", "cut_sandstone"), ("sandstone", "chiseled_sandstone"), ("red_sandstone", "cut_red_sandstone"), ("red_sandstone", "chiseled_red_sandstone"), ("blackstone", "polished_blackstone"), ("blackstone", "polished_blackstone_bricks"), ("polished_blackstone", "polished_blackstone_bricks"), ("blackstone", "chiseled_polished_blackstone"),
                ("basalt", "polished_basalt"), ("quartz_block", "quartz_bricks"), ("quartz_block", "chiseled_quartz_block"), ("quartz_block", "quartz_pillar"), ("purpur_block", "purpur_pillar"), ("end_stone", "end_stone_bricks"), ("nether_bricks", "chiseled_nether_bricks"), ("mud_bricks", "mud_bricks"),
                ("sulfur", "polished_sulfur"), ("sulfur", "sulfur_bricks"), ("sulfur", "chiseled_sulfur"), ("cinnabar", "polished_cinnabar"), ("cinnabar", "cinnabar_bricks"), ("cinnabar", "chiseled_cinnabar"), ("prismarine_bricks", "prismarine_bricks") })
                if (from != to) Cut(from, to, 1);
            foreach (var cu in new[] { "", "exposed_", "weathered_", "oxidized_" })
            {
                string blk = cu == "" ? "copper_block" : cu + "copper";
                Cut(blk, cu + "cut_copper", 4); Cut(blk, cu + "chiseled_copper", 4); Cut(blk, cu + "copper_grate", 4);
                Shaped(cu + "cut_copper", 4, new[] { "CC", "CC" }, 'C', blk);
                Shaped(cu + "copper_grate", 4, new[] { " C ", "C C", " C " }, 'C', blk);
                Shaped(cu + "chiseled_copper", 1, new[] { "S", "S" }, 'S', cu + "cut_copper_slab");
                Shaped(cu + "copper_bulb", 4, new[] { " C ", "CBC", " R " }, 'C', blk, 'B', "blaze_rod", 'R', "redstone");
            }
            // ---- colored families
            foreach (var c in Blocks.Colors)
            {
                string dye = c + "_dye";
                if (c != "white") Shapeless(c + "_wool", 1, dye, "#wool");
                Shaped(c + "_carpet", 3, new[] { "WW" }, 'W', c + "_wool");
                Shaped(c + "_stained_glass", 8, new[] { "GGG", "GDG", "GGG" }, 'G', "glass", 'D', dye);
                Shaped(c + "_stained_glass_pane", 16, new[] { "GGG", "GGG" }, 'G', c + "_stained_glass");
                Shaped(c + "_terracotta", 8, new[] { "TTT", "TDT", "TTT" }, 'T', "terracotta", 'D', dye);
                Shapeless(c + "_concrete_powder", 8, dye, "sand", "sand", "sand", "sand", "gravel", "gravel", "gravel", "gravel");
                Smelt(c + "_terracotta", c + "_glazed_terracotta", 0.1f);
                Shaped(c + "_bed", 1, new[] { "WWW", "PPP" }, 'W', c + "_wool", 'P', "#planks");
                Shaped(c + "_candle", 1, new[] { "D", "C" }, 'D', dye, 'C', "candle");
                if (Has(c + "_shulker_box")) Shapeless(c + "_shulker_box", 1, "shulker_box", dye);
            }
            // dyes from flowers
            foreach (var (flower, dye, n) in new[] { ("dandelion", "yellow_dye", 1), ("poppy", "red_dye", 1), ("blue_orchid", "light_blue_dye", 1), ("allium", "magenta_dye", 1), ("azure_bluet", "light_gray_dye", 1),
                ("red_tulip", "red_dye", 1), ("orange_tulip", "orange_dye", 1), ("white_tulip", "light_gray_dye", 1), ("pink_tulip", "pink_dye", 1), ("oxeye_daisy", "light_gray_dye", 1), ("cornflower", "blue_dye", 1),
                ("lily_of_the_valley", "white_dye", 1), ("wither_rose", "black_dye", 1), ("sunflower", "yellow_dye", 2), ("lilac", "magenta_dye", 2), ("rose_bush", "red_dye", 2), ("peony", "pink_dye", 2),
                ("bone_meal", "white_dye", 1), ("ink_sac", "black_dye", 1), ("cocoa_beans", "brown_dye", 1), ("lapis_lazuli", "blue_dye", 1), ("beetroot", "red_dye", 1), ("torchflower", "orange_dye", 1), ("pink_petals", "pink_dye", 1), ("open_eyeblossom", "orange_dye", 1), ("closed_eyeblossom", "gray_dye", 1), ("cactus_placeholder", "green_dye", 1) })
                if (Has(flower)) Shapeless(dye, n, flower);
            Smelt("cactus", "green_dye", 1f);
            Smelt("sea_pickle", "lime_dye", 0.1f);
            Shapeless("orange_dye", 2, "red_dye", "yellow_dye"); Shapeless("magenta_dye", 2, "purple_dye", "pink_dye"); Shapeless("light_blue_dye", 2, "blue_dye", "white_dye");
            Shapeless("lime_dye", 2, "green_dye", "white_dye"); Shapeless("pink_dye", 2, "red_dye", "white_dye"); Shapeless("gray_dye", 2, "black_dye", "white_dye");
            Shapeless("light_gray_dye", 2, "gray_dye", "white_dye"); Shapeless("cyan_dye", 2, "blue_dye", "green_dye"); Shapeless("purple_dye", 2, "blue_dye", "red_dye");
            // ---- storage blocks (9x) and back
            foreach (var (item, block) in new[] { ("coal", "coal_block"), ("iron_ingot", "iron_block"), ("gold_ingot", "gold_block"), ("diamond", "diamond_block"), ("emerald", "emerald_block"), ("lapis_lazuli", "lapis_block"),
                ("redstone", "redstone_block"), ("netherite_ingot", "netherite_block"), ("raw_iron", "raw_iron_block"), ("raw_copper", "raw_copper_block"), ("raw_gold", "raw_gold_block"), ("copper_ingot", "copper_block"),
                ("wheat", "hay_block"), ("slime_ball", "slime_block"), ("bone_meal", "bone_block"), ("dried_kelp", "dried_kelp_block") })
            {
                Shaped(block, 1, new[] { "XXX", "XXX", "XXX" }, 'X', item);
                Shapeless(item, 9, block);
            }
            foreach (var (nug, ing) in new[] { ("iron_nugget", "iron_ingot"), ("gold_nugget", "gold_ingot"), ("copper_nugget", "copper_ingot") })
            { Shaped(ing, 1, new[] { "XXX", "XXX", "XXX" }, 'X', nug); Shapeless(nug, 9, ing); }
            // ---- tools & armor
            var toolMats = new[] { ("wooden", "#planks"), ("stone", "#stone_tool_materials"), ("copper", "copper_ingot"), ("iron", "iron_ingot"), ("golden", "gold_ingot"), ("diamond", "diamond") };
            foreach (var (n, m) in toolMats)
            {
                Shaped(n + "_pickaxe", 1, new[] { "MMM", " S ", " S " }, 'M', m, 'S', "stick");
                Shaped(n + "_axe", 1, new[] { "MM", "MS", " S" }, 'M', m, 'S', "stick");
                Shaped(n + "_shovel", 1, new[] { "M", "S", "S" }, 'M', m, 'S', "stick");
                Shaped(n + "_hoe", 1, new[] { "MM", " S", " S" }, 'M', m, 'S', "stick");
                Shaped(n + "_sword", 1, new[] { "M", "M", "S" }, 'M', m, 'S', "stick");
                Shaped(n + "_spear", 1, new[] { "  M", " S ", "S  " }, 'M', m, 'S', "stick");
            }
            foreach (var (n, m) in new[] { ("leather", "leather"), ("copper", "copper_ingot"), ("iron", "iron_ingot"), ("golden", "gold_ingot"), ("diamond", "diamond") })
            {
                Shaped(n + "_helmet", 1, new[] { "MMM", "M M" }, 'M', m);
                Shaped(n + "_chestplate", 1, new[] { "M M", "MMM", "MMM" }, 'M', m);
                Shaped(n + "_leggings", 1, new[] { "MMM", "M M", "M M" }, 'M', m);
                Shaped(n + "_boots", 1, new[] { "M M", "M M" }, 'M', m);
            }
            Shaped("turtle_helmet", 1, new[] { "SSS", "S S" }, 'S', "turtle_scute");
            Shaped("wolf_armor", 1, new[] { "S  ", "SSS", "S S" }, 'S', "armadillo_scute");
            foreach (var n in new[] { "pickaxe", "axe", "shovel", "hoe", "sword", "spear", "helmet", "chestplate", "leggings", "boots" })
                Smithing.Add(new SmithingRecipe { template = "netherite_upgrade_smithing_template", baseItem = "diamond_" + n, addition = "netherite_ingot", result = "netherite_" + n });
            Shaped("netherite_upgrade_smithing_template", 2, new[] { "DTD", "DND", "DDD" }, 'D', "diamond", 'T', "netherite_upgrade_smithing_template", 'N', "netherrack");
        }

        static string WallSource(string wallId)
        {
            string baseName = wallId.Substring(0, wallId.Length - 5);
            foreach (var cand in new[] { baseName, baseName + "s", baseName.Replace("_brick", "_bricks"), baseName.Replace("_tile", "_tiles") })
                if (Blocks.Get(cand) != null && !(Blocks.Get(cand) is WallBlock)) return cand;
            return null;
        }
    }
}
