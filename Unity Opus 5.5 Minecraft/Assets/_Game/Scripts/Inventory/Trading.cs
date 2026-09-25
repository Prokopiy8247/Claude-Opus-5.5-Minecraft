using System;
using System.Collections.Generic;
using System.Text;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// One villager trade. <see cref="xp"/> is what the villager learns from it (see <c>VillagerMob.OnTrade</c>);
    /// <see cref="rewarding"/> marks trades that also pay the player experience, which <c>OnTrade</c> does.
    /// </summary>
    public class MerchantOffer
    {
        public ItemStack costA, costB;
        public ItemStack result;
        public int xp;
        public int uses, maxUses;
        public bool rewarding = true;
        /// <summary>Villager level that unlocked this offer, so the menu can fill in levels that have none.</summary>
        public int tier = 1;
        /// <summary>Game time the offer was last refilled (a villager restocks at most every half day).</summary>
        public long restockedAt;

        public MerchantOffer(ItemStack a, ItemStack b, ItemStack result, int maxUses, int xp)
        {
            costA = a; costB = b; this.result = result; this.maxUses = maxUses; this.xp = xp;
        }

        public bool OutOfStock => uses >= maxUses;

        /// <summary>Do the two payment stacks cover the price in either order? A one-cost trade wants the other slot empty.</summary>
        public bool Accepts(ItemStack a, ItemStack b) => (Covers(a, costA) && Covers(b, costB)) || (Covers(b, costA) && Covers(a, costB));

        /// <summary>True when the payment lines up as (costA, costB) rather than swapped.</summary>
        public bool InOrder(ItemStack a, ItemStack b) => Covers(a, costA) && Covers(b, costB);

        static bool Covers(ItemStack pay, ItemStack cost)
        {
            if (cost == null || cost.IsEmpty) return pay == null || pay.IsEmpty;
            return pay != null && pay.item == cost.item && pay.count >= cost.count;
        }

        // ------------------------------------------------------------------ compact string form
        // fields are ',' separated; Trading joins offers with ';', which ItemStack.Serialize already escapes
        public string Serialize()
        {
            var sb = new StringBuilder();
            sb.Append(Esc(costA)).Append(',').Append(Esc(costB)).Append(',').Append(Esc(result)).Append(',')
              .Append(uses).Append(',').Append(maxUses).Append(',').Append(xp).Append(',')
              .Append(rewarding ? 1 : 0).Append(',').Append(tier).Append(',').Append(restockedAt);
            return sb.ToString();
        }

        public static MerchantOffer Deserialize(string s)
        {
            if (string.IsNullOrEmpty(s)) return null;
            var f = s.Split(',');
            if (f.Length < 6) return null;
            var a = Unesc(f[0]);
            var b = Unesc(f[1]);
            var r = Unesc(f[2]);
            if (a == null || r == null) return null;   // an item this build no longer knows
            int.TryParse(f[3], out int uses);
            int.TryParse(f[4], out int maxUses);
            int.TryParse(f[5], out int xp);
            var o = new MerchantOffer(a, b, r, Math.Max(1, maxUses), xp) { uses = uses };
            if (f.Length > 6) o.rewarding = f[6] != "0";
            if (f.Length > 7 && int.TryParse(f[7], out int tier)) o.tier = tier;
            if (f.Length > 8 && long.TryParse(f[8], out long at)) o.restockedAt = at;
            return o;
        }

        // ItemStack.Serialize escapes ';' but not ',', which stored enchantments use as their own separator
        static string Esc(ItemStack s) => s == null || s.IsEmpty ? "" : s.Serialize().Replace(",", "%2C");
        static ItemStack Unesc(string s) => string.IsNullOrEmpty(s) ? null : ItemStack.Deserialize(s.Replace("%2C", ","));

        public override string ToString() => $"{costA} + {costB} -> {result} ({uses}/{maxUses})";
    }

    /// <summary>
    /// Villager trade tables, list generation and persistence. Every profession has a small pool of deals per
    /// level and a villager draws two from each level it reaches, so its list grows as it levels up and two
    /// villagers of the same trade rarely match. Later levels carry the dearer, rarer goods.
    /// </summary>
    public static class Trading
    {
        const int OffersPerLevel = 2;
        const int WandererCommonCount = 5, WandererRareCount = 1;

        enum Kind : byte { Plain, Enchanted, Book, Dyed, Tipped }

        /// <summary>One table entry. Ids may contain "{color}", replaced by one random dye colour per offer.</summary>
        sealed class Deal
        {
            public string a, b, r;
            public int an, bn, rn;
            public int spread;      // extra random units on the first cost, so villagers differ
            public int uses, xp;
            public Kind kind;

            public MerchantOffer Make(int tier, ref RNG rng)
            {
                string color = Blocks.Colors[rng.Next(Blocks.Colors.Length)];
                var costA = Stack(a, an + (spread > 0 ? rng.Next(spread + 1) : 0), color);
                var costB = b != null ? Stack(b, bn, color) : null;
                if (costA == null || (b != null && costB == null)) return null;
                ItemStack res;
                if (kind == Kind.Book)
                {
                    var e = BookEnchant(ref rng);
                    if (e == null) return null;
                    int lvl = rng.Range(1, e.maxLevel);
                    res = EnchantedBookItem.Make(e, lvl);
                    int price = 3 + lvl * 4 + rng.Next(3 + lvl * 6);
                    if (e.treasure) price *= 2;
                    costA.count = Math.Min(64, price);
                }
                else
                {
                    res = Stack(r, rn, color);
                    if (res == null) return null;
                    if (kind == Kind.Enchanted)
                    {
                        int power = 5 + rng.Next(15);
                        Loot.EnchantRandomly(res, ref rng, power, false);
                        costA.count = Math.Min(64, costA.count + power / 3);
                    }
                    else if (kind == Kind.Dyed)
                    {
                        if (TextureGen.DyeColors.TryGetValue(color, out var c))
                            res.Set("color", ((c.r << 16) | (c.g << 8) | c.b).ToString());
                    }
                    else if (kind == Kind.Tipped) res.Set("potion", TippedPotions[rng.Next(TippedPotions.Length)]);
                }
                return new MerchantOffer(costA, costB, res, uses, xp) { tier = tier };
            }
        }

        static ItemStack Stack(string id, int n, string color)
        {
            if (id.IndexOf('{') >= 0) id = id.Replace("{color}", color);
            var it = Items.Get(id);
            return it == null ? null : new ItemStack(it, Math.Max(1, Math.Min(n, it.maxStack)));
        }

        static Enchant BookEnchant(ref RNG rng)
        {
            var all = Enchant.All;
            int n = 0;
            for (int i = 0; i < all.Count; i++) if (!all[i].curse) n++;
            if (n == 0) return null;
            int k = rng.Next(n);
            for (int i = 0; i < all.Count; i++) if (!all[i].curse && k-- == 0) return all[i];
            return null;
        }

        static readonly string[] TippedPotions =
        {
            "swiftness", "slowness", "strength", "healing", "harming", "poison", "regeneration",
            "fire_resistance", "water_breathing", "night_vision", "invisibility", "leaping", "weakness", "slow_falling",
        };

        // ------------------------------------------------------------------ deal builders
        /// <summary>The villager buys <paramref name="n"/> (plus a little per villager) of an item for one emerald.</summary>
        static Deal Buy(string item, int n, int uses, int xp) =>
            new Deal { a = item, an = n, spread = Math.Max(1, n / 6), r = "emerald", rn = 1, uses = uses, xp = xp };
        /// <summary>The villager sells <paramref name="n"/> of an item for <paramref name="price"/> emeralds.</summary>
        static Deal Sell(string item, int n, int price, int uses, int xp, Kind kind = Kind.Plain) =>
            new Deal { a = "emerald", an = price, r = item, rn = n, uses = uses, xp = xp, kind = kind };
        /// <summary>Emeralds plus a raw material for a finished good.</summary>
        static Deal Work(int price, string with, int withN, string item, int n, int uses, int xp, Kind kind = Kind.Plain) =>
            new Deal { a = "emerald", an = price, b = with, bn = withN, r = item, rn = n, uses = uses, xp = xp, kind = kind };
        /// <summary>An enchanted book: a random enchantment, priced by its level, bound into a plain book.</summary>
        static Deal Book(int uses, int xp) =>
            new Deal { a = "emerald", an = 1, b = "book", bn = 1, uses = uses, xp = xp, kind = Kind.Book };

        // ------------------------------------------------------------------ tables: [level-1][deals]
        static readonly Dictionary<string, Deal[][]> Tables = new Dictionary<string, Deal[][]>
        {
            ["farmer"] = new[]
            {
                new[] { Buy("wheat", 20, 16, 2), Buy("potato", 26, 16, 2), Buy("carrot", 22, 16, 2), Sell("bread", 6, 1, 16, 1) },
                new[] { Buy("pumpkin", 6, 12, 5), Sell("pumpkin_pie", 4, 1, 12, 5), Sell("apple", 4, 1, 16, 5) },
                new[] { Buy("melon", 4, 12, 10), Sell("cookie", 18, 3, 12, 10), Buy("sugar_cane", 24, 16, 10) },
                new[] { Sell("cake", 1, 1, 12, 15), Buy("egg", 16, 16, 15), Buy("beetroot", 15, 16, 15) },
                new[] { Sell("golden_carrot", 3, 3, 12, 30), Sell("glistering_melon_slice", 3, 4, 12, 30) },
            },
            ["fisherman"] = new[]
            {
                new[] { Buy("string", 20, 16, 2), Buy("coal", 10, 16, 2), Work(1, "cod", 6, "cooked_cod", 6, 16, 1), Sell("cod_bucket", 1, 3, 16, 1) },
                new[] { Buy("cod", 15, 16, 5), Work(1, "salmon", 6, "cooked_salmon", 6, 16, 5), Sell("campfire", 1, 2, 12, 5) },
                new[] { Buy("salmon", 13, 16, 10), Sell("fishing_rod", 1, 3, 3, 10, Kind.Enchanted) },
                new[] { Buy("tropical_fish", 6, 12, 15), Buy("pufferfish", 4, 12, 15) },
                new[] { Buy("oak_boat", 1, 12, 30), Sell("pufferfish_bucket", 1, 4, 12, 30), Sell("salmon_bucket", 1, 4, 12, 30) },
            },
            ["shepherd"] = new[]
            {
                new[] { Buy("white_wool", 18, 16, 2), Buy("brown_wool", 18, 16, 2), Buy("black_wool", 18, 16, 2), Sell("shears", 1, 2, 12, 1) },
                new[] { Buy("white_dye", 12, 16, 5), Buy("black_dye", 12, 16, 5), Sell("{color}_wool", 1, 1, 16, 5), Sell("{color}_carpet", 4, 1, 16, 5) },
                new[] { Buy("yellow_dye", 12, 16, 10), Buy("red_dye", 12, 16, 10), Sell("{color}_bed", 1, 3, 12, 10) },
                new[] { Buy("blue_dye", 12, 16, 15), Buy("green_dye", 12, 16, 15), Sell("{color}_banner", 1, 3, 12, 15) },
                new[] { Sell("painting", 3, 2, 12, 30), Sell("{color}_bed", 1, 2, 12, 30) },
            },
            ["fletcher"] = new[]
            {
                new[] { Buy("stick", 32, 16, 2), Sell("arrow", 16, 1, 12, 1), Work(1, "gravel", 10, "flint", 10, 12, 1) },
                new[] { Buy("flint", 26, 12, 5), Sell("bow", 1, 2, 12, 5) },
                new[] { Buy("string", 14, 16, 10), Sell("crossbow", 1, 3, 12, 10) },
                new[] { Buy("feather", 24, 16, 15), Sell("bow", 1, 2, 3, 15, Kind.Enchanted) },
                new[] { Buy("tripwire_hook", 8, 12, 30), Sell("crossbow", 1, 3, 3, 30, Kind.Enchanted), Work(2, "arrow", 5, "tipped_arrow", 5, 12, 30, Kind.Tipped) },
            },
            ["librarian"] = new[]
            {
                new[] { Buy("paper", 24, 16, 2), Book(12, 1), Sell("bookshelf", 1, 9, 12, 1) },
                new[] { Buy("book", 4, 12, 10), Book(12, 5), Sell("lantern", 1, 1, 12, 5) },
                new[] { Buy("ink_sac", 5, 12, 20), Book(12, 10), Sell("glass", 4, 1, 12, 10) },
                new[] { Buy("writable_book", 2, 12, 30), Book(12, 15), Sell("clock", 1, 5, 12, 15), Sell("compass", 1, 4, 12, 15) },
                new[] { Sell("name_tag", 1, 20, 12, 30), Sell("lectern", 1, 6, 12, 30) },
            },
            ["cartographer"] = new[]
            {
                new[] { Buy("paper", 24, 16, 2), Sell("map", 1, 7, 12, 1) },
                new[] { Buy("glass_pane", 11, 16, 10), Sell("item_frame", 1, 7, 12, 5) },
                new[] { Buy("compass", 1, 12, 20), Sell("spyglass", 1, 6, 12, 10) },
                new[] { Sell("flower_banner_pattern", 1, 8, 12, 15), Sell("{color}_banner", 1, 3, 12, 15), Sell("painting", 1, 2, 12, 15) },
                new[] { Sell("globe_banner_pattern", 1, 8, 12, 30), Sell("lodestone", 1, 12, 12, 30), Sell("compass", 1, 3, 12, 30) },
            },
            ["cleric"] = new[]
            {
                new[] { Buy("rotten_flesh", 32, 16, 2), Sell("redstone", 2, 1, 12, 1) },
                new[] { Buy("gold_ingot", 3, 12, 10), Sell("lapis_lazuli", 1, 1, 12, 5) },
                new[] { Buy("rabbit_foot", 2, 12, 20), Sell("glowstone", 1, 4, 12, 10) },
                new[] { Buy("turtle_scute", 4, 12, 30), Buy("glass_bottle", 9, 12, 30), Sell("ender_pearl", 1, 5, 12, 15) },
                new[] { Buy("nether_wart", 22, 12, 30), Sell("experience_bottle", 1, 3, 12, 30) },
            },
            ["armorer"] = new[]
            {
                new[] { Buy("coal", 15, 16, 2), Sell("iron_leggings", 1, 8, 12, 1), Sell("iron_boots", 1, 4, 12, 1), Sell("iron_chestplate", 1, 10, 12, 1) },
                new[] { Buy("iron_ingot", 4, 12, 10), Sell("bell", 1, 30, 12, 5), Sell("chainmail_boots", 1, 2, 12, 5), Sell("chainmail_leggings", 1, 3, 12, 5) },
                new[] { Buy("lava_bucket", 1, 12, 20), Buy("diamond", 1, 12, 20), Sell("chainmail_helmet", 1, 2, 12, 10), Sell("shield", 1, 5, 12, 10) },
                new[] { Sell("diamond_leggings", 1, 16, 3, 15, Kind.Enchanted), Sell("diamond_boots", 1, 10, 3, 15, Kind.Enchanted) },
                new[] { Sell("diamond_helmet", 1, 10, 3, 30, Kind.Enchanted), Sell("diamond_chestplate", 1, 19, 3, 30, Kind.Enchanted) },
            },
            ["weaponsmith"] = new[]
            {
                new[] { Buy("coal", 15, 16, 2), Sell("iron_axe", 1, 3, 12, 1), Sell("iron_sword", 1, 2, 3, 1, Kind.Enchanted) },
                new[] { Buy("iron_ingot", 4, 12, 10), Sell("bell", 1, 30, 12, 5), Sell("iron_spear", 1, 4, 12, 5) },
                new[] { Buy("flint", 24, 12, 20), Sell("iron_sword", 1, 4, 12, 10) },
                new[] { Buy("diamond", 1, 12, 30), Sell("diamond_axe", 1, 12, 3, 15, Kind.Enchanted) },
                new[] { Sell("diamond_sword", 1, 10, 3, 30, Kind.Enchanted), Sell("diamond_spear", 1, 11, 3, 30, Kind.Enchanted) },
            },
            ["toolsmith"] = new[]
            {
                new[] { Buy("coal", 15, 16, 2), Sell("stone_axe", 1, 1, 12, 1), Sell("stone_shovel", 1, 1, 12, 1), Sell("stone_pickaxe", 1, 1, 12, 1) },
                new[] { Buy("iron_ingot", 4, 12, 10), Sell("bell", 1, 30, 12, 5), Sell("stone_hoe", 1, 1, 12, 5) },
                new[] { Buy("flint", 30, 12, 20), Sell("iron_axe", 1, 2, 3, 10, Kind.Enchanted), Sell("iron_pickaxe", 1, 3, 3, 10, Kind.Enchanted), Sell("diamond_hoe", 1, 4, 3, 10) },
                new[] { Buy("diamond", 1, 12, 30), Sell("diamond_axe", 1, 12, 3, 15, Kind.Enchanted), Sell("diamond_shovel", 1, 5, 3, 15, Kind.Enchanted) },
                new[] { Sell("diamond_pickaxe", 1, 13, 3, 30, Kind.Enchanted), Sell("diamond_shovel", 1, 7, 3, 30, Kind.Enchanted) },
            },
            ["butcher"] = new[]
            {
                new[] { Buy("chicken", 14, 16, 2), Buy("porkchop", 7, 16, 2), Buy("rabbit", 4, 16, 2), Sell("rabbit_stew", 1, 1, 12, 1) },
                new[] { Buy("coal", 15, 16, 2), Sell("cooked_porkchop", 5, 1, 16, 5), Sell("cooked_chicken", 8, 1, 16, 5) },
                new[] { Buy("mutton", 7, 16, 20), Buy("beef", 10, 16, 20) },
                new[] { Buy("dried_kelp_block", 10, 12, 30), Sell("cooked_mutton", 5, 1, 16, 15) },
                new[] { Buy("sweet_berries", 10, 12, 30), Sell("cooked_beef", 4, 1, 16, 30) },
            },
            ["leatherworker"] = new[]
            {
                new[] { Buy("leather", 6, 16, 2), Sell("leather_leggings", 1, 3, 12, 1, Kind.Dyed), Sell("leather_chestplate", 1, 7, 12, 1, Kind.Dyed) },
                new[] { Buy("flint", 26, 12, 10), Sell("leather_helmet", 1, 5, 12, 5, Kind.Dyed), Sell("leather_boots", 1, 4, 12, 5, Kind.Dyed) },
                new[] { Buy("rabbit_hide", 9, 12, 20), Sell("leather_chestplate", 1, 7, 12, 10, Kind.Dyed) },
                new[] { Buy("turtle_scute", 4, 12, 30), Sell("leather_horse_armor", 1, 6, 12, 15, Kind.Dyed) },
                new[] { Sell("saddle", 1, 6, 12, 30), Sell("leather_helmet", 1, 5, 12, 30, Kind.Dyed) },
            },
            ["mason"] = new[]
            {
                new[] { Buy("clay_ball", 10, 16, 2), Sell("brick", 10, 1, 16, 1) },
                new[] { Buy("stone", 20, 16, 10), Sell("chiseled_stone_bricks", 4, 1, 16, 5) },
                new[] { Buy("granite", 16, 16, 20), Buy("andesite", 16, 16, 20), Sell("polished_andesite", 4, 1, 16, 10), Sell("polished_diorite", 4, 1, 16, 10) },
                new[] { Buy("quartz", 12, 12, 30), Sell("{color}_terracotta", 1, 1, 12, 15), Sell("{color}_glazed_terracotta", 1, 1, 12, 15) },
                new[] { Sell("quartz_pillar", 1, 1, 12, 30), Sell("quartz_block", 1, 1, 12, 30) },
            },
        };

        // the wandering trader has no levels: five everyday goods and one rarity, rolled once per visit
        static readonly Deal[] WandererCommon =
        {
            Sell("oak_sapling", 1, 5, 8, 1), Sell("birch_sapling", 1, 5, 8, 1), Sell("spruce_sapling", 1, 5, 8, 1),
            Sell("jungle_sapling", 1, 5, 8, 1), Sell("acacia_sapling", 1, 5, 8, 1), Sell("cherry_sapling", 1, 5, 8, 1),
            Sell("{color}_dye", 3, 1, 12, 1), Sell("dandelion", 1, 1, 12, 1), Sell("poppy", 1, 1, 12, 1),
            Sell("cornflower", 1, 1, 12, 1), Sell("wheat_seeds", 1, 1, 12, 1), Sell("pumpkin_seeds", 1, 1, 12, 1),
            Sell("melon_seeds", 1, 1, 12, 1), Sell("sea_pickle", 1, 2, 5, 1), Sell("sugar_cane", 1, 1, 8, 1),
            Sell("pumpkin", 1, 1, 4, 1), Sell("kelp", 1, 3, 12, 1), Sell("cactus", 1, 3, 8, 1), Sell("lily_pad", 2, 1, 5, 1),
            Sell("sand", 8, 1, 8, 1), Sell("red_sand", 4, 1, 6, 1), Sell("moss_block", 2, 1, 5, 1), Sell("vine", 1, 1, 12, 1),
            Sell("brown_mushroom", 1, 1, 12, 1), Sell("red_mushroom", 1, 1, 12, 1), Sell("glowstone", 1, 2, 5, 1),
            Sell("slime_ball", 1, 4, 5, 1), Sell("nautilus_shell", 1, 5, 5, 1),
        };
        static readonly Deal[] WandererRare =
        {
            Sell("tropical_fish_bucket", 1, 5, 4, 1), Sell("pufferfish_bucket", 1, 5, 4, 1), Sell("packed_ice", 1, 3, 6, 1),
            Sell("blue_ice", 1, 6, 6, 1), Sell("gunpowder", 1, 1, 8, 1), Sell("podzol", 3, 3, 6, 1), Sell("oak_log", 8, 1, 4, 1),
        };

        // ------------------------------------------------------------------ generation
        /// <summary>
        /// Rolls offers for <paramref name="profession"/>. By default the whole list up to <paramref name="level"/>;
        /// with <paramref name="append"/> only the offers that level adds, for extending an existing list.
        /// </summary>
        public static List<MerchantOffer> Generate(string profession, int level, RNG rng, bool append = false)
        {
            var list = new List<MerchantOffer>();
            if (profession == "wandering_trader")
            {
                if (!append)
                {
                    Draw(WandererCommon, WandererCommonCount, 1, ref rng, list);
                    Draw(WandererRare, WandererRareCount, 1, ref rng, list);
                }
                return list;
            }
            if (profession == null || !Tables.TryGetValue(profession, out var tiers)) return list;   // none / nitwit
            if (level < 1 || (append && level > tiers.Length)) return list;
            level = Math.Min(level, tiers.Length);
            for (int l = append ? level : 1; l <= level; l++) Draw(tiers[l - 1], OffersPerLevel, l, ref rng, list);
            return list;
        }

        /// <summary>Draws up to <paramref name="n"/> different deals from a pool; deals naming unknown items are skipped.</summary>
        static void Draw(Deal[] pool, int n, int tier, ref RNG rng, List<MerchantOffer> into)
        {
            var order = new int[pool.Length];
            for (int i = 0; i < order.Length; i++) order[i] = i;
            for (int i = 0; i < order.Length && n > 0; i++)
            {
                int j = i + rng.Next(order.Length - i);
                int t = order[i]; order[i] = order[j]; order[j] = t;
                var o = pool[order[i]].Make(tier, ref rng);
                if (o == null) continue;
                into.Add(o);
                n--;
            }
        }

        // ------------------------------------------------------------------ persistence
        public static string Serialize(List<MerchantOffer> offers)
        {
            if (offers == null || offers.Count == 0) return "";
            var sb = new StringBuilder();
            for (int i = 0; i < offers.Count; i++)
            {
                if (i > 0) sb.Append(';');
                sb.Append(offers[i].Serialize());
            }
            return sb.ToString();
        }

        public static List<MerchantOffer> Deserialize(string s)
        {
            var list = new List<MerchantOffer>();
            if (string.IsNullOrEmpty(s)) return list;
            foreach (var part in s.Split(';'))
            {
                var o = MerchantOffer.Deserialize(part);
                if (o != null) list.Add(o);
            }
            return list;
        }
    }

    /// <summary>
    /// Villager trading. The left column lists the villager's offers, each row showing its two costs and its
    /// result (<see cref="VisibleRows"/> at a time, scrollable); the two payment slots and the result slot on the
    /// right carry out the trade. Taking the result pays the price and reports to <c>VillagerMob.OnTrade</c>,
    /// which owns the villager's experience, sounds and level-ups.
    /// </summary>
    public class MerchantMenu : WorkstationMenu
    {
        public readonly VillagerMob villager;
        /// <summary>Offer the player picked in the list, or -1. Picking also pulls its price out of the inventory.</summary>
        public int selected = -1;
        /// <summary>Index of the first offer shown in the list.</summary>
        public int scroll;

        public const int PayA = 0, PayB = 1, ResultIdx = 2;
        public const int VisibleRows = 7, MaxLevel = 5;
        /// <summary>Trade experience a villager needs to leave each level, indexed by its current level (the thresholds <c>VillagerMob.OnTrade</c> uses).</summary>
        public static readonly int[] LevelXp = { 0, 10, 70, 150, 250 };
        static readonly string[] LevelNames = { "Novice", "Apprentice", "Journeyman", "Expert", "Master" };
        /// <summary>Villagers refill a used offer at most twice a day.</summary>
        const long RestockTicks = 12000;

        MerchantOffer active;

        public MerchantMenu(Player p, VillagerMob v) : base(p, 2)
        {
            villager = v; background = "villager"; width = 276; height = 166;
            title = v.customName ?? (IsTrader ? "Wandering Trader" : Blocks.PrettyName(v.Profession));
            AddInput(PayA, 136, 37);
            AddInput(PayB, 162, 37);
            AddResult(220, 37);
            FinishLayout(108, 84);
            EnsureOffers();
            Restock();
            v.tradingWith = p;
        }

        bool IsTrader => villager.def != null && villager.def.id == "wandering_trader";
        string ProfessionKey => IsTrader ? "wandering_trader" : villager.Profession;
        long Now => villager.world.session != null ? villager.world.session.gameTime : villager.world.tickCount;

        public List<MerchantOffer> Offers => villager.offers;
        public int Level => Mathf.Clamp(villager.level, 1, MaxLevel);
        public string LevelName => LevelNames[Level - 1];
        /// <summary>Wandering traders have no levels, so their window shows no bar.</summary>
        public bool ShowLevel => !IsTrader;

        /// <summary>0..1 through the current level, from the villager's trade experience.</summary>
        public float LevelProgress
        {
            get
            {
                int l = Level;
                if (l >= MaxLevel) return 1f;
                int lo = LevelXp[l - 1], hi = LevelXp[l];
                return Mathf.Clamp01((villager.tradeXp - lo) / (float)(hi - lo));
            }
        }

        /// <summary>Offer shown in list row <paramref name="row"/> (0..VisibleRows-1), or null.</summary>
        public MerchantOffer RowOffer(int row)
        {
            var list = villager.offers;
            int i = scroll + row;
            return list != null && row >= 0 && row < VisibleRows && i < list.Count ? list[i] : null;
        }

        public void Scroll(int delta)
        {
            int count = villager.offers != null ? villager.offers.Count : 0;
            scroll = Mathf.Clamp(scroll + delta, 0, Math.Max(0, count - VisibleRows));
        }

        /// <summary>Offer list click: selects the offer and moves its price from the inventory into the payment slots.</summary>
        public void SelectOffer(int index)
        {
            var list = villager.offers;
            if (list == null || index < 0 || index >= list.Count) return;
            selected = index;
            var o = list[index];
            // the old payment goes back first so the slots can be refilled with exactly this offer's currency
            for (int i = 0; i < 2; i++)
            {
                var s = inputs.items[i];
                if (s == null) continue;
                player.inventory.Add(s);
                if (s.count <= 0) inputs.items[i] = null;
            }
            Fill(PayA, o.costA);
            Fill(PayB, o.costB);
            inputs.SetChanged();
        }

        /// <summary>Pulls up to a full stack of the cost's item from the main inventory into a payment slot.</summary>
        void Fill(int slot, ItemStack cost)
        {
            if (cost == null || cost.IsEmpty) return;
            var cur = inputs.items[slot];
            if (cur != null && cur.item != cost.item) return;
            var main = player.inventory.main;
            int max = cost.MaxStack;
            for (int i = 0; i < main.Length; i++)
            {
                int have = cur != null ? cur.count : 0;
                if (have >= max) break;
                var s = main[i];
                if (s == null || s.item != cost.item || (cur != null && !cur.Stackable(s))) continue;
                int n = Math.Min(s.count, max - have);
                if (cur == null) { cur = s.Split(n); inputs.items[slot] = cur; }
                else { cur.count += n; s.count -= n; }
                if (s.count <= 0) main[i] = null;
            }
        }

        protected override void Recompute()
        {
            active = null;
            var a = In(PayA); var b = In(PayB);
            var list = villager.offers;
            if (list != null && (a != null || b != null))
            {
                // the picked offer wins; otherwise the first offer the payment happens to cover
                if (selected >= 0 && selected < list.Count && !list[selected].OutOfStock && list[selected].Accepts(a, b)) active = list[selected];
                else foreach (var o in list) if (!o.OutOfStock && o.Accepts(a, b)) { active = o; break; }
            }
            SetOutput(active != null ? active.result.Copy() : null);
        }

        protected override void OnResultTaken(Player p, ItemStack taken)
        {
            var o = active;
            if (o == null) return;
            // each cost comes out of whichever slot holds it
            bool inOrder = o.InOrder(In(PayA), In(PayB));
            Pay(inOrder ? PayA : PayB, o.costA);
            Pay(inOrder ? PayB : PayA, o.costB);
            if (o.uses == 0) o.restockedAt = Now;
            o.uses++;
            villager.OnTrade(o);
            CatchUpLevels();
        }

        void Pay(int slot, ItemStack cost)
        {
            if (cost != null && !cost.IsEmpty) ConsumeInput(slot, cost.count);
        }

        /// <summary>
        /// <c>OnTrade</c> levels a villager at most once per trade; this applies any further thresholds by the same
        /// rule (a large trade, or experience granted elsewhere), adding each new level's offers.
        /// </summary>
        void CatchUpLevels()
        {
            var v = villager;
            if (IsTrader || v.offers == null) return;
            while (v.level >= 1 && v.level < MaxLevel && v.tradeXp >= LevelXp[v.level])
            {
                v.level++;
                v.offers.AddRange(Trading.Generate(ProfessionKey, v.level, new RNG(v.id * 131 + v.level), true));
            }
        }

        /// <summary>
        /// Makes sure the villager has offers for every level it has reached: a first list when it has none, and
        /// the offers of any level that is missing (a level set outside of trading, or an older save).
        /// </summary>
        void EnsureOffers()
        {
            var v = villager;
            if (v.offers == null || v.offers.Count == 0)
            {
                v.offers = Trading.Generate(ProfessionKey, Level, new RNG(v.id * 7919 + v.world.seed));
                return;
            }
            if (IsTrader) return;
            for (int l = 1; l <= Level; l++)
            {
                bool have = false;
                foreach (var o in v.offers) if (o.tier == l) { have = true; break; }
                if (!have) v.offers.AddRange(Trading.Generate(ProfessionKey, l, new RNG(v.id * 131 + l), true));
            }
        }

        /// <summary>Opening the window half a day after an offer was first used refills it; traders never restock.</summary>
        void Restock()
        {
            if (IsTrader || villager.offers == null) return;
            long now = Now;
            foreach (var o in villager.offers)
            {
                if (o.uses <= 0) continue;
                if (o.restockedAt > now) o.restockedAt = now;   // clock went backwards (world copied between saves)
                else if (now - o.restockedAt >= RestockTicks) o.uses = 0;
            }
        }

        protected override bool QuickMoveIntoContainer(ItemStack stack)
        {
            // only currency some offer asks for goes to the payment slots; anything else shuffles inventory <-> hotbar
            var list = villager.offers;
            if (list == null) return false;
            foreach (var o in list)
                if ((o.costA != null && o.costA.item == stack.item) || (o.costB != null && o.costB.item == stack.item))
                    return MoveItemTo(stack, PayA, PayB + 1, false);
            return false;
        }

        public override bool StillValid()
        {
            var v = villager;
            return v.IsAlive && v.world == player.world && (v.tradingWith == null || v.tradingWith == player)
                && (v.position - player.position).sqrMagnitude < MenuUtil.ReachSq;
        }

        public override void Removed()
        {
            base.Removed();
            if (villager.tradingWith == player) villager.tradingWith = null;
        }
    }
}
