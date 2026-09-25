using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Original advancement goals, reported from the gameplay code. Each has an id, a title, a hint shown while
    /// locked, and a fixed on-screen toast position; progress is per-world and saved with the player.
    /// </summary>
    public static class Achievements
    {
        public sealed class Goal
        {
            public string id, title, hint;
            public int x, y;          // toast grid slot (columns, rows)
            public Goal parent;
            public bool IsChild => parent != null;
        }

        static readonly List<Goal> all = new List<Goal>();
        static readonly Dictionary<string, Goal> byId = new Dictionary<string, Goal>();
        static readonly HashSet<string> earned = new HashSet<string>();
        static readonly List<Goal> toastQueue = new List<Goal>();
        static Goal current;
        static int currentTicks;

        public static IReadOnlyList<Goal> All => all;
        public static bool Has(string id) => id != null && earned.Contains(id);
        public static int CompletedCount => earned.Count;

        static Goal G(string id, string title, string hint, int x, int y, string parent = null)
        {
            var g = new Goal { id = id, title = title, hint = hint, x = x, y = y, parent = parent != null && byId.TryGetValue(parent, out var p) ? p : null };
            all.Add(g); byId[id] = g;
            return g;
        }

        static bool inited;
        static void Init()
        {
            if (inited) return; inited = true;
            // the opening tree
            G("root", "Taking Inventory", "Open your inventory", 0, 0);
            G("crafting_table", "Benchmarking", "Craft a crafting table", 1, 0, "root");
            G("get_wood", "Getting Wood", "Punch a tree until it drops", 1, 1, "root");
            G("stone_age", "Stone Age", "Mine stone with your new pickaxe", 2, 0, "crafting_table");
            G("build_pickaxe", "Time to Mine!", "Use planks and sticks to make a pickaxe", 2, 1, "crafting_table");
            G("build_sword", "Time to Strike!", "Use planks and sticks to make a sword", 2, -1, "crafting_table");
            G("furnace", "Hot Topic", "Construct a furnace out of eight cobblestone", 3, 0, "stone_age");
            G("smelt_iron", "Acquire Hardware", "Smelt an iron ingot", 4, 1, "furnace");
            G("get_iron_tool", "Isn't It Iron Pick", "Upgrade your pickaxe", 5, 1, "smelt_iron");
            G("diamonds", "Diamonds!", "Acquire diamonds", 5, 0, "get_iron_tool");
            G("get_diamond_tool", "Diamonds Forever", "Craft a diamond tool", 6, 0, "diamonds");
            G("enchant", "Enchanter", "Use an enchanting table", 6, -1, "diamonds");
            G("obsidian", "Ice Bucket Challenge", "Obtain a block of obsidian", 6, 1, "get_iron_tool");
            // the Nether
            G("enter_the_nether_prepare", "We Need to Go Deeper", "Build, light and enter a Nether portal", 7, 0, "obsidian");
            G("enter_the_nether", "Into the Fire", "Enter the Nether", 7, -1, "enter_the_nether_prepare");
            G("follow_ender_eye", "Eye Spy", "Follow an Eye of Ender", 8, -1, "enter_the_nether");
            G("brew", "Local Brewery", "Brew a potion", 7, 1, "enter_the_nether");
            // the End
            G("enter_the_end", "The End?", "Enter the End portal", 9, -1, "follow_ender_eye");
            G("the_end", "Free the End", "Defeat the Ender Dragon", 10, -1, "enter_the_end");
            G("the_end_again", "The End Again", "Respawn the dragon and defeat it once more", 11, -1, "the_end");
            G("elytra", "Great View From Up Here", "Find an elytra and glide", 10, 0, "the_end");
            G("summon_wither", "Withering Heights", "Summon the Wither", 9, 1, "enter_the_nether");
            G("kill_wither", "Bring Home the Beacon", "Defeat the Wither", 10, 1, "summon_wither");
            // exploration and building
            G("shield", "Not Today, Thank You", "Deflect a projectile with a shield", 3, 1, "build_sword");
            G("bed", "Sweet Dreams", "Sleep in a bed to set your spawn", 3, -1, "crafting_table");
            G("enchant_gear", "Overpowered", "Enchant a piece of gear", 7, -2, "enchant");
            G("tame_an_animal", "Best Friends Forever", "Tame an animal", 4, -2, "get_wood");
            G("bred_animals", "Repopulation", "Breed two animals", 5, -2, "tame_an_animal");
            G("fishy_business", "Fishy Business", "Catch a fish with a fishing rod", 6, 2, "get_wood");
            G("kill_a_mob", "Monster Hunter", "Kill a hostile monster", 3, 2, "root");
            G("kill_all_mobs", "Monsters Hunted", "Kill one of every hostile monster", 4, 3, "kill_a_mob");
            G("use_ladder", "Getting an Upgrade", "Climb a ladder", 2, 2, "root");
            G("sniper_duel", "Sniper Duel", "Kill a Skeleton from at least 50 blocks away", 4, 2, "kill_a_mob");
            G("return_to_sender", "Return to Sender", "Destroy a Ghast fireball by hitting it back", 8, 2, "enter_the_nether");
            G("beacon", "Beaconator", "Build a beacon and activate it", 8, 1, "kill_wither");
            G("iron_golem", "Hired Help", "Summon an iron golem", 5, -3, "bred_animals");
            G("snow_golem", "Snow Trouble", "Summon a snow golem", 6, -3, "iron_golem");
            G("trade", "What a Deal!", "Trade with a villager", 6, -2, "get_wood");
            G("raid", "We Win!", "Defeat a raid", 8, -2, "trade");
            G("deep_dark", "Sneak 100", "Sneak to a sculk shrieker without being heard", 9, 2, "kill_a_mob");
        }

        public static void Reset()
        {
            earned.Clear();
            toastQueue.Clear();
            current = null;
            currentTicks = 0;
        }

        // ------------------------------------------------------------------ reporting
        public static void Grant(Player p, string id)
        {
            Init();
            if (id == null || p == null) return;
            if (!byId.ContainsKey(id)) { Debug.LogWarning("Unknown advancement id: " + id); return; }
            if (!earned.Add(id)) return;
            var g = byId[id];
            toastQueue.Add(g);
            Sounds.PlayUI("ui.toast.challenge_complete");
            if (toastQueue.Count > 8) toastQueue.RemoveAt(0);
        }

        public static void OnCraft(Player p, string itemId) => MapCraft(p, itemId);
        public static void OnPickup(Player p, string itemId) => MapPickup(p, itemId);
        public static void OnKill(Player p, string mobId) => MapKill(p, mobId);
        public static void OnMine(Player p, string blockId) => MapMine(p, blockId);
        public static void OnCraft(Player p, Recipe r, ItemStack result) { if (result != null && result.item != null) MapCraft(p, result.item.id); }
        public static void OnSmelt(Player p, string itemId) { if (itemId == "iron_ingot") Grant(p, "smelt_iron"); }

        static void MapCraft(Player p, string id)
        {
            switch (id)
            {
                case "crafting_table": Grant(p, "crafting_table"); Grant(p, "root"); break;
                case "furnace": Grant(p, "furnace"); break;
                case "wooden_pickaxe": case "stone_pickaxe": case "iron_pickaxe": case "golden_pickaxe": Grant(p, "build_pickaxe"); Grant(p, "get_iron_tool"); break;
                case "wooden_sword": case "stone_sword": case "iron_sword": case "golden_sword": Grant(p, "build_sword"); break;
                case "iron_axe": case "iron_shovel": case "iron_hoe": Grant(p, "get_iron_tool"); break;
                case "diamond_pickaxe": case "diamond_axe": case "diamond_shovel": case "diamond_sword": Grant(p, "get_diamond_tool"); Grant(p, "diamonds"); break;
                case "shield": Grant(p, "shield"); break;
                case "white_bed": case "red_bed": case "blue_bed": case "black_bed": Grant(p, "bed"); break;
                case "beacon": Grant(p, "beacon"); break;
                case "elytra": Grant(p, "elytra"); break;
            }
        }

        static void MapPickup(Player p, string id)
        {
            if (id == null) return;
            if (id.EndsWith("_log") || id.EndsWith("_wood") || id == "bamboo") Grant(p, "get_wood");
            if (id == "diamond") Grant(p, "diamonds");
            if (id == "obsidian") Grant(p, "obsidian");
            if (id == "elytra") Grant(p, "elytra");
        }

        static void MapMine(Player p, string id)
        {
            if (id == null) return;
            if (id == "stone" || id == "cobblestone" || id == "deepslate" || id == "cobbled_deepslate") Grant(p, "stone_age");
            if (id == "diamond_ore" || id == "deepslate_diamond_ore") Grant(p, "diamonds");
            if (id == "obsidian") Grant(p, "obsidian");
            if (id == "ancient_debris") Grant(p, "obsidian");
        }

        static void MapKill(Player p, string mobId)
        {
            if (mobId == null) return;
            if (MobRegistry.IsHostile(mobId)) Grant(p, "kill_a_mob");
            if (mobId == "ender_dragon") Grant(p, "the_end");
            if (mobId == "wither") Grant(p, "kill_wither");
        }

        // ------------------------------------------------------------------ toast rendering
        /// <summary>Drives the toast stack; called once per frame from the HUD.</summary>
        public static void Tick()
        {
            if (current == null)
            {
                if (toastQueue.Count == 0) return;
                current = toastQueue[0];
                toastQueue.RemoveAt(0);
                currentTicks = 0;
            }
            currentTicks++;
            if (currentTicks > 100) { current = null; currentTicks = 0; }
        }

        public static Goal ToastGoal => current;
        /// <summary>0..1 progress of the toast slide, or -1 when nothing is showing.</summary>
        public static float ToastProgress
        {
            get
            {
                if (current == null) return -1f;
                if (currentTicks < 12) return currentTicks / 12f;
                if (currentTicks > 88) return Mathf.Clamp01((100 - currentTicks) / 12f);
                return 1f;
            }
        }

        /// <summary>All goals, in tree order, with the ids already earned filtered out.</summary>
        public static IEnumerable<Goal> Locked()
        {
            Init();
            foreach (var g in all) if (!earned.Contains(g.id)) yield return g;
        }

        public static string Serialize()
        {
            if (earned.Count == 0) return "";
            var list = new List<string>(earned);
            list.Sort();
            return string.Join(",", list);
        }

        public static void Deserialize(string s)
        {
            Init();
            earned.Clear();
            if (string.IsNullOrEmpty(s)) return;
            foreach (var id in s.Split(','))
            {
                var t = id.Trim();
                if (t.Length > 0 && byId.ContainsKey(t)) earned.Add(t);
            }
        }
    }
}
