using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public static partial class Recipes
    {
        static void RegisterCore()
        {
            Shaped("stick", 4, new[] { "P", "P" }, 'P', "#planks");
            Shaped("stick", 1, new[] { "B", "B" }, 'B', "bamboo");
            Shaped("crafting_table", 1, new[] { "PP", "PP" }, 'P', "#planks");
            Shaped("chest", 1, new[] { "PPP", "P P", "PPP" }, 'P', "#planks");
            Shapeless("trapped_chest", 1, "chest", "tripwire_hook|redstone_torch");
            Shaped("copper_chest", 1, new[] { "CCC", "C C", "CCC" }, 'C', "copper_ingot");
            Shaped("barrel", 1, new[] { "PSP", "P P", "PSP" }, 'P', "#planks", 'S', "#wooden_slabs");
            Shaped("furnace", 1, new[] { "CCC", "C C", "CCC" }, 'C', "#stone_crafting_materials");
            Shaped("smoker", 1, new[] { " L ", "LFL", " L " }, 'L', "#logs", 'F', "furnace");
            Shaped("blast_furnace", 1, new[] { "III", "IFI", "SSS" }, 'I', "iron_ingot", 'F', "furnace", 'S', "smooth_stone");
            Shaped("torch", 4, new[] { "C", "S" }, 'C', "#coals", 'S', "stick");
            Shaped("soul_torch", 4, new[] { "C", "S", "B" }, 'C', "#coals", 'S', "stick", 'B', "#soul_fire_base");
            Shaped("copper_torch", 4, new[] { "N", "C", "S" }, 'N', "copper_nugget", 'C', "#coals", 'S', "stick");
            Shaped("redstone_torch", 1, new[] { "R", "S" }, 'R', "redstone", 'S', "stick");
            Shaped("lantern", 1, new[] { "NNN", "NTN", "NNN" }, 'N', "iron_nugget", 'T', "torch");
            Shaped("soul_lantern", 1, new[] { "NNN", "NTN", "NNN" }, 'N', "iron_nugget", 'T', "soul_torch");
            Shaped("copper_lantern", 1, new[] { "NNN", "NTN", "NNN" }, 'N', "copper_nugget", 'T', "copper_torch");
            Shaped("campfire", 1, new[] { " S ", "SCS", "LLL" }, 'S', "stick", 'C', "#coals", 'L', "#logs");
            Shaped("soul_campfire", 1, new[] { " S ", "SBS", "LLL" }, 'S', "stick", 'B', "#soul_fire_base", 'L', "#logs");
            Shaped("ladder", 3, new[] { "S S", "SSS", "S S" }, 'S', "stick");
            Shaped("scaffolding", 6, new[] { "BSB", "B B", "B B" }, 'B', "bamboo", 'S', "string");
            Shaped("glass_pane", 16, new[] { "GGG", "GGG" }, 'G', "glass");
            Shaped("iron_bars", 16, new[] { "III", "III" }, 'I', "iron_ingot");
            Shaped("copper_bars", 16, new[] { "III", "III" }, 'I', "copper_ingot");
            Shaped("iron_chain", 1, new[] { "N", "I", "N" }, 'N', "iron_nugget", 'I', "iron_ingot");
            Shaped("copper_chain", 1, new[] { "N", "I", "N" }, 'N', "copper_nugget", 'I', "copper_ingot");
            Shaped("iron_door", 3, new[] { "II", "II", "II" }, 'I', "iron_ingot");
            Shaped("iron_trapdoor", 1, new[] { "II", "II" }, 'I', "iron_ingot");
            Shaped("copper_door", 3, new[] { "II", "II", "II" }, 'I', "copper_ingot");
            Shaped("copper_trapdoor", 1, new[] { "II", "II" }, 'I', "copper_ingot");
            Shaped("bucket", 1, new[] { "I I", " I " }, 'I', "iron_ingot");
            Shapeless("flint_and_steel", 1, "iron_ingot", "flint");
            Shaped("shears", 1, new[] { " I", "I " }, 'I', "iron_ingot");
            Shaped("compass", 1, new[] { " I ", "IRI", " I " }, 'I', "iron_ingot", 'R', "redstone");
            Shaped("clock", 1, new[] { " G ", "GRG", " G " }, 'G', "gold_ingot", 'R', "redstone");
            Shaped("recovery_compass", 1, new[] { "EEE", "ECE", "EEE" }, 'E', "echo_shard", 'C', "compass");
            Shaped("spyglass", 1, new[] { "A", "C", "C" }, 'A', "amethyst_shard", 'C', "copper_ingot");
            Shaped("brush", 1, new[] { "F", "C", "S" }, 'F', "feather", 'C', "copper_ingot", 'S', "stick");
            Shaped("fishing_rod", 1, new[] { "  S", " ST", "S T" }, 'S', "stick", 'T', "string");
            Shaped("carrot_on_a_stick", 1, new[] { "R ", " C" }, 'R', "fishing_rod", 'C', "carrot");
            Shaped("warped_fungus_on_a_stick", 1, new[] { "R ", " F" }, 'R', "fishing_rod", 'F', "warped_fungus");
            Shaped("bow", 1, new[] { " ST", "S T", " ST" }, 'S', "stick", 'T', "string");
            Shaped("arrow", 4, new[] { "F", "S", "E" }, 'F', "flint", 'S', "stick", 'E', "feather");
            Shaped("spectral_arrow", 2, new[] { " G ", "GAG", " G " }, 'G', "glowstone_dust", 'A', "arrow");
            Shaped("crossbow", 1, new[] { "SIS", "THT", " S " }, 'S', "stick", 'I', "iron_ingot", 'T', "string", 'H', "tripwire_hook|string");
            Shaped("shield", 1, new[] { "PIP", "PPP", " P " }, 'P', "#planks", 'I', "iron_ingot");
            Shaped("mace", 1, new[] { "H", "B" }, 'H', "heavy_core", 'B', "breeze_rod");
            Shaped("lead", 2, new[] { "SS ", "SB ", "  S" }, 'S', "string", 'B', "slime_ball");
            Shaped("saddle", 1, new[] { " L ", "LIL" }, 'L', "leather", 'I', "iron_ingot");
            Shaped("harness", 1, new[] { "LLL", "GWG" }, 'L', "leather", 'G', "glass", 'W', "#wool");
            Shaped("minecart", 1, new[] { "I I", "III" }, 'I', "iron_ingot");
            Shapeless("chest_minecart", 1, "minecart", "chest"); Shapeless("hopper_minecart", 1, "minecart", "hopper"); Shapeless("tnt_minecart", 1, "minecart", "tnt"); Shapeless("furnace_minecart", 1, "minecart", "furnace");
            Shaped("rail", 16, new[] { "I I", "ISI", "I I" }, 'I', "iron_ingot", 'S', "stick");
            Shaped("powered_rail", 6, new[] { "G G", "GSG", "GRG" }, 'G', "gold_ingot", 'S', "stick", 'R', "redstone");
            Shaped("detector_rail", 6, new[] { "I I", "IPI", "IRI" }, 'I', "iron_ingot", 'P', "stone_pressure_plate", 'R', "redstone");
            Shaped("activator_rail", 6, new[] { "ISI", "IRI", "ISI" }, 'I', "iron_ingot", 'S', "stick", 'R', "redstone_torch");
            Shaped("tnt", 1, new[] { "GSG", "SGS", "GSG" }, 'G', "gunpowder", 'S', "#sand");
            Shaped("lever", 1, new[] { "S", "C" }, 'S', "stick", 'C', "cobblestone");
            Shapeless("stone_button", 1, "stone"); Shapeless("polished_blackstone_button", 1, "polished_blackstone");
            Shaped("stone_pressure_plate", 1, new[] { "SS" }, 'S', "stone");
            Shaped("light_weighted_pressure_plate", 1, new[] { "GG" }, 'G', "gold_ingot");
            Shaped("heavy_weighted_pressure_plate", 1, new[] { "II" }, 'I', "iron_ingot");
            Shaped("repeater", 1, new[] { "TRT", "SSS" }, 'T', "redstone_torch", 'R', "redstone", 'S', "stone");
            Shaped("comparator", 1, new[] { " T ", "TQT", "SSS" }, 'T', "redstone_torch", 'Q', "quartz", 'S', "stone");
            Shaped("redstone_lamp", 1, new[] { " R ", "RGR", " R " }, 'R', "redstone", 'G', "glowstone");
            Shaped("piston", 1, new[] { "PPP", "CIC", "CRC" }, 'P', "#planks", 'C', "cobblestone", 'I', "iron_ingot", 'R', "redstone");
            Shaped("sticky_piston", 1, new[] { "S", "P" }, 'S', "slime_ball", 'P', "piston");
            Shaped("observer", 1, new[] { "CCC", "RRQ", "CCC" }, 'C', "cobblestone", 'R', "redstone", 'Q', "quartz");
            Shaped("dispenser", 1, new[] { "CCC", "CBC", "CRC" }, 'C', "cobblestone", 'B', "bow", 'R', "redstone");
            Shaped("dropper", 1, new[] { "CCC", "C C", "CRC" }, 'C', "cobblestone", 'R', "redstone");
            Shaped("hopper", 1, new[] { "I I", "ICI", " I " }, 'I', "iron_ingot", 'C', "chest");
            Shaped("target", 1, new[] { " R ", "RHR", " R " }, 'R', "redstone", 'H', "hay_block");
            Shaped("note_block", 1, new[] { "PPP", "PRP", "PPP" }, 'P', "#planks", 'R', "redstone");
            Shaped("jukebox", 1, new[] { "PPP", "PDP", "PPP" }, 'P', "#planks", 'D', "diamond");
            Shaped("daylight_detector", 1, new[] { "GGG", "QQQ", "SSS" }, 'G', "glass", 'Q', "quartz", 'S', "#wooden_slabs");
            Shaped("lightning_rod", 1, new[] { "C", "C", "C" }, 'C', "copper_ingot");
            Shaped("crafter", 1, new[] { "III", "ICI", "RDR" }, 'I', "iron_ingot", 'C', "crafting_table", 'R', "redstone", 'D', "dropper");
            Shaped("slime_block", 1, new[] { "SSS", "SSS", "SSS" }, 'S', "slime_ball");
            Shaped("honey_block", 1, new[] { "HH", "HH" }, 'H', "honey_bottle");
            Shaped("bookshelf", 1, new[] { "PPP", "BBB", "PPP" }, 'P', "#planks", 'B', "book");
            Shaped("chiseled_bookshelf", 1, new[] { "PPP", "SSS", "PPP" }, 'P', "#planks", 'S', "#wooden_slabs");
            Shapeless("book", 1, "paper", "paper", "paper", "leather");
            Shaped("paper", 3, new[] { "SSS" }, 'S', "sugar_cane");
            Shapeless("sugar", 1, "sugar_cane");
            Shapeless("writable_book", 1, "book", "ink_sac", "feather");
            Shaped("enchanting_table", 1, new[] { " B ", "DOD", "OOO" }, 'B', "book", 'D', "diamond", 'O', "obsidian");
            Shaped("anvil", 1, new[] { "BBB", " I ", "III" }, 'B', "iron_block", 'I', "iron_ingot");
            Shaped("grindstone", 1, new[] { "SLS", "P P" }, 'S', "stick", 'L', "stone_slab", 'P', "#planks");
            Shaped("smithing_table", 1, new[] { "II", "PP", "PP" }, 'I', "iron_ingot", 'P', "#planks");
            Shaped("stonecutter", 1, new[] { " I ", "SSS" }, 'I', "iron_ingot", 'S', "stone");
            Shaped("loom", 1, new[] { "SS", "PP" }, 'S', "string", 'P', "#planks");
            Shaped("cartography_table", 1, new[] { "PP", "WW", "WW" }, 'P', "paper", 'W', "#planks");
            Shaped("fletching_table", 1, new[] { "FF", "WW", "WW" }, 'F', "flint", 'W', "#planks");
            Shaped("composter", 1, new[] { "S S", "S S", "SSS" }, 'S', "#wooden_slabs");
            Shaped("lectern", 1, new[] { "SSS", " B ", " S " }, 'S', "#wooden_slabs", 'B', "bookshelf");
            Shaped("cauldron", 1, new[] { "I I", "I I", "III" }, 'I', "iron_ingot");
            Shaped("brewing_stand", 1, new[] { " B ", "CCC" }, 'B', "blaze_rod", 'C', "#stone_crafting_materials");
            Shaped("beacon", 1, new[] { "GGG", "GNG", "OOO" }, 'G', "glass", 'N', "nether_star", 'O', "obsidian");
            Shaped("respawn_anchor", 1, new[] { "CCC", "GGG", "CCC" }, 'C', "crying_obsidian", 'G', "glowstone");
            Shaped("lodestone", 1, new[] { "SSS", "SIS", "SSS" }, 'S', "chiseled_stone_bricks", 'I', "iron_ingot");
            Shaped("ender_chest", 1, new[] { "OOO", "OEO", "OOO" }, 'O', "obsidian", 'E', "ender_eye");
            Shaped("shulker_box", 1, new[] { "S", "C", "S" }, 'S', "shulker_shell", 'C', "chest");
            Shaped("end_crystal", 1, new[] { "GGG", "GEG", "GTG" }, 'G', "glass", 'E', "ender_eye", 'T', "ghast_tear");
            Shapeless("ender_eye", 1, "ender_pearl", "blaze_powder");
            Shapeless("blaze_powder", 2, "blaze_rod");
            Shapeless("fire_charge", 3, "gunpowder", "blaze_powder", "#coals");
            Shapeless("magma_cream", 1, "blaze_powder", "slime_ball");
            Shapeless("fermented_spider_eye", 1, "spider_eye", "brown_mushroom", "sugar");
            Shaped("glistering_melon_slice", 1, new[] { "NNN", "NMN", "NNN" }, 'N', "gold_nugget", 'M', "melon_slice");
            Shaped("golden_carrot", 1, new[] { "NNN", "NCN", "NNN" }, 'N', "gold_nugget", 'C', "carrot");
            Shaped("golden_apple", 1, new[] { "GGG", "GAG", "GGG" }, 'G', "gold_ingot", 'A', "apple");
            Shaped("end_rod", 4, new[] { "B", "P" }, 'B', "blaze_rod", 'P', "popped_chorus_fruit");
            Shaped("purpur_block", 4, new[] { "PP", "PP" }, 'P', "popped_chorus_fruit");
            Shaped("glass_bottle", 3, new[] { "G G", " G " }, 'G', "glass");
            Shaped("bowl", 4, new[] { "P P", " P " }, 'P', "#planks");
            Shaped("bread", 1, new[] { "WWW" }, 'W', "wheat");
            Shaped("cookie", 8, new[] { "WCW" }, 'W', "wheat", 'C', "cocoa_beans");
            Shapeless("pumpkin_pie", 1, "pumpkin", "sugar", "egg");
            Shapeless("mushroom_stew", 1, "brown_mushroom", "red_mushroom", "bowl");
            Shapeless("beetroot_soup", 1, "beetroot", "beetroot", "beetroot", "beetroot", "beetroot", "beetroot", "bowl");
            Shapeless("rabbit_stew", 1, "cooked_rabbit", "carrot", "baked_potato", "brown_mushroom|red_mushroom", "bowl");
            Shaped("melon", 1, new[] { "MMM", "MMM", "MMM" }, 'M', "melon_slice");
            Shapeless("melon_seeds", 1, "melon_slice"); Shapeless("pumpkin_seeds", 4, "pumpkin");
            Shaped("jack_o_lantern", 1, new[] { "P", "T" }, 'P', "carved_pumpkin", 'T', "torch");
            Shaped("snow_block", 1, new[] { "SS", "SS" }, 'S', "snowball");
            Shaped("snow", 6, new[] { "SSS" }, 'S', "snow_block");
            Shaped("clay", 1, new[] { "CC", "CC" }, 'C', "clay_ball");
            Shaped("bricks", 1, new[] { "BB", "BB" }, 'B', "brick");
            Shaped("nether_bricks", 1, new[] { "BB", "BB" }, 'B', "nether_brick");
            Shaped("nether_brick_fence", 6, new[] { "BNB", "BNB" }, 'B', "nether_bricks", 'N', "nether_brick");
            Shaped("red_nether_bricks", 1, new[] { "NW", "WN" }, 'N', "nether_brick", 'W', "nether_wart");
            Shaped("nether_wart_block", 1, new[] { "WWW", "WWW", "WWW" }, 'W', "nether_wart");
            Shaped("quartz_block", 1, new[] { "QQ", "QQ" }, 'Q', "quartz");
            Shaped("quartz_pillar", 2, new[] { "Q", "Q" }, 'Q', "quartz_block");
            Shaped("quartz_bricks", 4, new[] { "QQ", "QQ" }, 'Q', "quartz_block");
            Shaped("glowstone", 1, new[] { "GG", "GG" }, 'G', "glowstone_dust");
            Shaped("stone_bricks", 4, new[] { "SS", "SS" }, 'S', "stone");
            Shapeless("mossy_stone_bricks", 1, "stone_bricks", "vine|moss_block");
            Shapeless("mossy_cobblestone", 1, "cobblestone", "vine|moss_block");
            Shaped("polished_granite", 4, new[] { "SS", "SS" }, 'S', "granite"); Shaped("polished_diorite", 4, new[] { "SS", "SS" }, 'S', "diorite"); Shaped("polished_andesite", 4, new[] { "SS", "SS" }, 'S', "andesite");
            Shapeless("granite", 1, "diorite", "quartz"); Shaped("diorite", 2, new[] { "CQ", "QC" }, 'C', "cobblestone", 'Q', "quartz"); Shapeless("andesite", 2, "diorite", "cobblestone");
            Shaped("polished_deepslate", 4, new[] { "SS", "SS" }, 'S', "cobbled_deepslate"); Shaped("deepslate_bricks", 4, new[] { "SS", "SS" }, 'S', "polished_deepslate"); Shaped("deepslate_tiles", 4, new[] { "SS", "SS" }, 'S', "deepslate_bricks");
            Shaped("polished_tuff", 4, new[] { "SS", "SS" }, 'S', "tuff"); Shaped("tuff_bricks", 4, new[] { "SS", "SS" }, 'S', "polished_tuff");
            Shaped("polished_blackstone", 4, new[] { "SS", "SS" }, 'S', "blackstone"); Shaped("polished_blackstone_bricks", 4, new[] { "SS", "SS" }, 'S', "polished_blackstone");
            Shaped("polished_basalt", 4, new[] { "SS", "SS" }, 'S', "basalt");
            Shaped("sandstone", 1, new[] { "SS", "SS" }, 'S', "sand"); Shaped("red_sandstone", 1, new[] { "SS", "SS" }, 'S', "red_sand");
            Shaped("cut_sandstone", 4, new[] { "SS", "SS" }, 'S', "sandstone"); Shaped("cut_red_sandstone", 4, new[] { "SS", "SS" }, 'S', "red_sandstone");
            Shaped("chiseled_sandstone", 1, new[] { "S", "S" }, 'S', "sandstone_slab"); Shaped("chiseled_red_sandstone", 1, new[] { "S", "S" }, 'S', "red_sandstone_slab");
            Shaped("chiseled_stone_bricks", 1, new[] { "S", "S" }, 'S', "stone_brick_slab");
            Shaped("prismarine", 1, new[] { "SS", "SS" }, 'S', "prismarine_shard"); Shaped("prismarine_bricks", 1, new[] { "SSS", "SSS", "SSS" }, 'S', "prismarine_shard");
            Shaped("dark_prismarine", 1, new[] { "SSS", "SIS", "SSS" }, 'S', "prismarine_shard", 'I', "black_dye");
            Shaped("sea_lantern", 1, new[] { "SCS", "CCC", "SCS" }, 'S', "prismarine_shard", 'C', "prismarine_crystals");
            Shaped("end_stone_bricks", 4, new[] { "SS", "SS" }, 'S', "end_stone");
            Shaped("purpur_pillar", 1, new[] { "S", "S" }, 'S', "purpur_slab");
            Shaped("mud_bricks", 4, new[] { "SS", "SS" }, 'S', "packed_mud");
            Shapeless("packed_mud", 1, "mud", "wheat");
            Shapeless("mud", 1, "dirt", "water_bucket");
            Shaped("coarse_dirt", 4, new[] { "DG", "GD" }, 'D', "dirt", 'G', "gravel");
            Shaped("packed_ice", 1, new[] { "III", "III", "III" }, 'I', "ice"); Shaped("blue_ice", 1, new[] { "III", "III", "III" }, 'I', "packed_ice");
            Shaped("moss_carpet", 3, new[] { "MM" }, 'M', "moss_block"); Shaped("pale_moss_carpet", 3, new[] { "MM" }, 'M', "pale_moss_block");
            Shaped("amethyst_block", 1, new[] { "AA", "AA" }, 'A', "amethyst_shard");
            Shaped("tinted_glass", 2, new[] { " A ", "AGA", " A " }, 'A', "amethyst_shard", 'G', "glass");
            Shaped("candle", 1, new[] { "S", "H" }, 'S', "string", 'H', "honeycomb");
            Shapeless("netherite_ingot", 1, "netherite_scrap", "netherite_scrap", "netherite_scrap", "netherite_scrap", "gold_ingot", "gold_ingot", "gold_ingot", "gold_ingot");
            Shaped("leather", 1, new[] { "RR", "RR" }, 'R', "rabbit_hide");
            Shaped("white_wool", 1, new[] { "SS", "SS" }, 'S', "string");
            Shaped("item_frame", 1, new[] { "SSS", "SLS", "SSS" }, 'S', "stick", 'L', "leather");
            Shaped("painting", 1, new[] { "SSS", "SWS", "SSS" }, 'S', "stick", 'W', "#wool");
            Shaped("armor_stand", 1, new[] { "SSS", " S ", "SPS" }, 'S', "stick", 'P', "smooth_stone_slab");
            Shaped("flower_pot", 1, new[] { "B B", " B " }, 'B', "brick");
            Shaped("map", 1, new[] { "PPP", "PCP", "PPP" }, 'P', "paper", 'C', "compass");
            Shaped("firework_rocket", 3, new[] { "PG" }, 'P', "paper", 'G', "gunpowder");
            Shaped("dried_kelp_block", 1, new[] { "KKK", "KKK", "KKK" }, 'K', "dried_kelp");
            Shaped("hay_block", 1, new[] { "WWW", "WWW", "WWW" }, 'W', "wheat");
            Shaped("sulfur", 1, new[] { "DD", "DD" }, 'D', "sulfur_dust");
            Shaped("cinnabar", 1, new[] { "DD", "DD" }, 'D', "cinnabar_dust");
            Shaped("polished_sulfur", 4, new[] { "SS", "SS" }, 'S', "sulfur"); Shaped("sulfur_bricks", 4, new[] { "SS", "SS" }, 'S', "polished_sulfur");
            Shaped("polished_cinnabar", 4, new[] { "SS", "SS" }, 'S', "cinnabar"); Shaped("cinnabar_bricks", 4, new[] { "SS", "SS" }, 'S', "polished_cinnabar");
            Shaped("ochre_froglight", 1, new[] { " M ", "M M", " M " }, 'M', "magma_cream");
            // fuel table
            Fuel("lava_bucket", 20000); Fuel("coal_block", 16000); Fuel("dried_kelp_block", 4001); Fuel("blaze_rod", 2400); Fuel("coal", 1600); Fuel("charcoal", 1600);
            foreach (var b in Blocks.All) if (b.item != null && b.flammability > 0 && b.item.fuelTicks == 0) Fuel(b.id, b.solid ? 300 : 100);
            foreach (var w in Blocks.WoodTypes) { Fuel(w + "_planks", 300); Fuel(w + "_slab", 150); Fuel(w + "_door", 200); Fuel(w + "_button", 100); }
            Fuel("stick", 100); Fuel("bamboo", 50); Fuel("scaffolding", 50); Fuel("bowl", 100); Fuel("bow", 300); Fuel("fishing_rod", 300); Fuel("crafting_table", 300); Fuel("ladder", 300);
        }

        static void RegisterSmelting()
        {
            foreach (var (ore, res, xp) in new[] { ("iron_ore|deepslate_iron_ore|raw_iron", "iron_ingot", 0.7f), ("gold_ore|deepslate_gold_ore|raw_gold|nether_gold_ore", "gold_ingot", 1f), ("copper_ore|deepslate_copper_ore|raw_copper", "copper_ingot", 0.7f),
                ("coal_ore|deepslate_coal_ore", "coal", 0.1f), ("diamond_ore|deepslate_diamond_ore", "diamond", 1f), ("emerald_ore|deepslate_emerald_ore", "emerald", 1f), ("lapis_ore|deepslate_lapis_ore", "lapis_lazuli", 0.2f),
                ("redstone_ore|deepslate_redstone_ore", "redstone", 0.7f), ("nether_quartz_ore", "quartz", 0.2f), ("ancient_debris", "netherite_scrap", 2f) })
                Smelt(ore, res, xp, false, true);
            foreach (var (raw, cooked) in new[] { ("beef", "cooked_beef"), ("porkchop", "cooked_porkchop"), ("chicken", "cooked_chicken"), ("mutton", "cooked_mutton"), ("rabbit", "cooked_rabbit"), ("cod", "cooked_cod"), ("salmon", "cooked_salmon"), ("potato", "baked_potato"), ("kelp", "dried_kelp") })
                Smelt(raw, cooked, 0.35f, true);
            Smelt("#sand", "glass", 0.1f);
            Smelt("cobblestone", "stone", 0.1f); Smelt("stone", "smooth_stone", 0.1f); Smelt("stone_bricks", "cracked_stone_bricks", 0.1f);
            Smelt("cobbled_deepslate", "deepslate", 0.1f); Smelt("deepslate_bricks", "cracked_deepslate_bricks", 0.1f); Smelt("deepslate_tiles", "cracked_deepslate_tiles", 0.1f);
            Smelt("clay_ball", "brick", 0.3f); Smelt("clay", "terracotta", 0.35f); Smelt("netherrack", "nether_brick", 0.1f); Smelt("nether_bricks", "cracked_nether_bricks", 0.1f);
            Smelt("#logs", "charcoal", 0.15f); Smelt("sandstone", "smooth_sandstone", 0.1f); Smelt("red_sandstone", "smooth_red_sandstone", 0.1f); Smelt("quartz_block", "smooth_quartz", 0.1f);
            Smelt("basalt", "smooth_basalt", 0.1f); Smelt("polished_blackstone_bricks", "cracked_polished_blackstone_bricks", 0.1f); Smelt("chorus_fruit", "popped_chorus_fruit", 0.1f);
            Smelt("wet_sponge", "sponge", 0.15f); Smelt("sea_pickle", "lime_dye", 0.1f);
            foreach (var t in new[] { "iron", "golden" })
                foreach (var n in new[] { "pickaxe", "axe", "shovel", "hoe", "sword", "helmet", "chestplate", "leggings", "boots", "spear" })
                    Smelt(t + "_" + n, t == "iron" ? "iron_nugget" : "gold_nugget", 0.1f, false, true);
        }

        static void RegisterSpecial()
        {
            // tool repair: two damaged tools of same kind
            Crafting.Add(new SpecialRecipe
            {
                id = "repair",
                result = new ItemStack(Items.Get("stick"), 1),
                fn = g =>
                {
                    ItemStack a = null, b = null;
                    foreach (var s in g.items)
                    {
                        if (s == null) continue;
                        if (a == null) a = s; else if (b == null) b = s; else return null;
                    }
                    if (a == null || b == null || a.item != b.item || !a.item.IsDamageable || a.count != 1 || b.count != 1) return null;
                    int max = a.item.maxDamage;
                    int dur = (max - a.damage) + (max - b.damage) + max * 5 / 100;
                    var r = new ItemStack(a.item, 1) { damage = Mathf.Max(0, max - dur) };
                    return r;
                }
            });
            // leather armor dyeing
            Crafting.Add(new SpecialRecipe
            {
                id = "armor_dye",
                result = new ItemStack(Items.Get("leather_chestplate"), 1),
                fn = g =>
                {
                    ItemStack armor = null; var dyes = new List<Color32>();
                    foreach (var s in g.items)
                    {
                        if (s == null) continue;
                        if (s.item is ArmorItem ai && ai.material == ArmorMaterial.Leather) { if (armor != null) return null; armor = s; }
                        else if (s.item.id.EndsWith("_dye")) dyes.Add(TextureGen.DyeColors[s.item.id.Substring(0, s.item.id.Length - 4)]);
                        else return null;
                    }
                    if (armor == null || dyes.Count == 0) return null;
                    int r = 0, gg = 0, b = 0; foreach (var d in dyes) { r += d.r; gg += d.g; b += d.b; }
                    var res = armor.CopyWithCount(1);
                    res.Set("color", ((r / dyes.Count) << 16 | (gg / dyes.Count) << 8 | (b / dyes.Count)).ToString());
                    return res;
                }
            });
            // tipped arrows: 8 arrows + lingering potion
            Crafting.Add(new SpecialRecipe
            {
                id = "tipped_arrow",
                result = new ItemStack(Items.Get("tipped_arrow"), 8),
                fn = g =>
                {
                    if (g.w < 3) return null;
                    var center = g.At(1, 1);
                    if (center == null || center.item.id != "lingering_potion") return null;
                    for (int i = 0; i < 9; i++) { if (i == 4) continue; if (g.items[i] == null || g.items[i].item.id != "arrow") return null; }
                    var r = new ItemStack("tipped_arrow", 8); r.Set("potion", center.Get("potion"));
                    return r;
                }
            });
        }
    }

    public static class Composting
    {
        public static float Chance(Item it)
        {
            if (it == null) return 0;
            string id = it.id;
            if (id.EndsWith("_seeds") || id == "short_grass" || id.EndsWith("_leaves") || id == "kelp" || id == "dried_kelp" || id == "seagrass" || id == "sweet_berries" || id == "glow_berries" || id == "moss_carpet") return 0.3f;
            if (id.EndsWith("_sapling") || id == "cactus" || id == "sugar_cane" || id == "vine" || id == "melon_slice" || id == "tall_grass" || id == "nether_sprouts" || id == "dried_kelp_block") return 0.5f;
            if (id == "apple" || id == "beetroot" || id == "carrot" || id == "potato" || id == "wheat" || id == "fern" || id.Contains("mushroom") || id == "pumpkin" || id == "melon" || id == "moss_block" || id == "lily_pad" || id == "cocoa_beans" || id == "nether_wart" || Blocks.Get(id) is PlantBlock) return 0.65f;
            if (id == "bread" || id == "baked_potato" || id == "cookie" || id == "hay_block" || id.EndsWith("_wart_block")) return 0.85f;
            if (id == "cake" || id == "pumpkin_pie") return 1f;
            return 0;
        }
    }
}
