using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public enum Precipitation { None, Rain, Snow }

    public struct SpawnEntry
    {
        public string mob; public int weight, min, max;
        public SpawnEntry(string m, int w, int mn, int mx) { mob = m; weight = w; min = mn; max = mx; }
    }

    public sealed class Biome
    {
        public int id;
        public string key, name;
        public DimensionId dim = DimensionId.Overworld;
        public float temperature = 0.8f, downfall = 0.4f;
        public Color32 grass, foliage, water = new Color32(63, 118, 228, 255), sky, fog = new Color32(192, 216, 255, 255), waterFog = new Color32(5, 5, 51, 255);
        public Precipitation precipitation = Precipitation.Rain;
        public string top = "grass_block", filler = "dirt", underwater = "sand";
        public int fillerDepth = 3;
        // vegetation
        public string[] trees = new string[0]; public float treesPerChunk = 0;
        public float grassDensity = 0, fernDensity = 0, flowerDensity = 0, tallGrassChance = 0.1f;
        public string[] flowers = new string[0];
        public bool sugarCane, cactus, deadBush, mushrooms, lilyPads, seagrass, kelp, coral, sweetBerries, bamboo, melons, pumpkins = true, icebergs, boulders, fallenLogs, leafLitter, bushes;
        public bool isOcean, isRiver, isBeach, isCave, isMountain, snowy, frozenWater;
        // mobs
        public readonly List<SpawnEntry> creatures = new List<SpawnEntry>();
        public readonly List<SpawnEntry> monsters = new List<SpawnEntry>();
        public readonly List<SpawnEntry> water_ = new List<SpawnEntry>();
        public readonly List<SpawnEntry> ambient = new List<SpawnEntry>();
        // nether/end particles
        public Color32 ambientParticle; public float particleDensity;
        public string ambientSound;

        static readonly List<Biome> all = new List<Biome>();
        static readonly Dictionary<string, Biome> byKey = new Dictionary<string, Biome>();
        public static IReadOnlyList<Biome> All => all;
        public static Biome Get(int id) => id >= 0 && id < all.Count ? all[id] : all[0];
        public static Biome Get(string key) => byKey.TryGetValue(key, out var b) ? b : null;
        public static int Id(string key) => byKey.TryGetValue(key, out var b) ? b.id : 0;

        static Biome R(string key, float temp, float down, uint grass, uint foliage, uint water = 0x3F76E4)
        {
            var b = new Biome { key = key, name = Blocks.PrettyName(key), temperature = temp, downfall = down, grass = MathX.Hex(grass), foliage = MathX.Hex(foliage), water = MathX.Hex(water) };
            b.id = all.Count; all.Add(b); byKey[key] = b;
            b.sky = SkyColorFor(temp);
            if (temp < 0.15f) { b.precipitation = Precipitation.Snow; b.snowy = true; }
            if (down <= 0f) b.precipitation = Precipitation.None;
            return b;
        }
        static Color32 SkyColorFor(float t)
        {
            float k = Mathf.Clamp(t / 3f, -1f, 1f);
            Color c = Color.HSVToRGB(0.62222f - k * 0.05f, 0.5f + k * 0.1f, 1f);
            return c;
        }

        public static Biome Plains, Ocean, River, Desert, Nether_Wastes, The_End, Deep_Dark, Lush_Caves, Dripstone_Caves, Sulfur_Caves;

        static void Animals(Biome b, bool farm = true)
        {
            if (farm) { b.creatures.Add(new SpawnEntry("sheep", 12, 2, 4)); b.creatures.Add(new SpawnEntry("pig", 10, 2, 4)); b.creatures.Add(new SpawnEntry("chicken", 10, 2, 4)); b.creatures.Add(new SpawnEntry("cow", 8, 2, 4)); }
        }
        static void Monsters(Biome b, bool zombies = true)
        {
            b.monsters.Add(new SpawnEntry("spider", 100, 1, 2));
            if (zombies) b.monsters.Add(new SpawnEntry("zombie", 95, 2, 4));
            b.monsters.Add(new SpawnEntry("zombie_villager", 5, 1, 1));
            b.monsters.Add(new SpawnEntry("skeleton", 100, 1, 3));
            b.monsters.Add(new SpawnEntry("creeper", 100, 1, 2));
            b.monsters.Add(new SpawnEntry("slime", 20, 1, 3));
            b.monsters.Add(new SpawnEntry("enderman", 10, 1, 2));
            b.monsters.Add(new SpawnEntry("witch", 5, 1, 1));
            b.ambient.Add(new SpawnEntry("bat", 10, 2, 4));
        }

        static bool inited;
        public static void Init()
        {
            if (inited) return; inited = true;
            // ---------------- temperate
            Plains = R("plains", 0.8f, 0.4f, 0x91BD59, 0x77AB2F); Plains.trees = new[] { "oak" }; Plains.treesPerChunk = 0.12f; Plains.grassDensity = 0.35f; Plains.flowerDensity = 0.02f;
            Plains.flowers = new[] { "dandelion", "poppy", "azure_bluet", "oxeye_daisy", "cornflower", "red_tulip", "orange_tulip", "white_tulip", "pink_tulip" }; Plains.bushes = true;
            Animals(Plains); Plains.creatures.Add(new SpawnEntry("horse", 5, 2, 6)); Plains.creatures.Add(new SpawnEntry("donkey", 1, 1, 3)); Monsters(Plains);
            var sp = R("sunflower_plains", 0.8f, 0.4f, 0x91BD59, 0x77AB2F); sp.trees = new[] { "oak" }; sp.treesPerChunk = 0.05f; sp.grassDensity = 0.35f; sp.flowerDensity = 0.03f; sp.flowers = new[] { "sunflower", "dandelion", "poppy" }; Animals(sp); Monsters(sp);
            var forest = R("forest", 0.7f, 0.8f, 0x79C05A, 0x59AE30); forest.trees = new[] { "oak", "oak", "oak", "birch" }; forest.treesPerChunk = 9; forest.grassDensity = 0.12f; forest.flowerDensity = 0.01f; forest.flowers = new[] { "lilac", "rose_bush", "peony", "dandelion", "poppy" }; forest.fallenLogs = true; forest.leafLitter = true;
            Animals(forest); forest.creatures.Add(new SpawnEntry("wolf", 5, 4, 4)); Monsters(forest);
            var ff = R("flower_forest", 0.7f, 0.8f, 0x79C05A, 0x59AE30); ff.trees = new[] { "oak", "birch" }; ff.treesPerChunk = 5; ff.grassDensity = 0.1f; ff.flowerDensity = 0.25f;
            ff.flowers = new[] { "dandelion", "poppy", "allium", "azure_bluet", "red_tulip", "orange_tulip", "white_tulip", "pink_tulip", "oxeye_daisy", "cornflower", "lily_of_the_valley", "lilac", "rose_bush", "peony" }; Animals(ff); ff.creatures.Add(new SpawnEntry("rabbit", 4, 2, 3)); Monsters(ff);
            var birch = R("birch_forest", 0.6f, 0.6f, 0x88BB67, 0x6BA941); birch.trees = new[] { "birch" }; birch.treesPerChunk = 9; birch.grassDensity = 0.12f; birch.flowerDensity = 0.01f; birch.flowers = new[] { "lilac", "rose_bush", "peony", "lily_of_the_valley" }; birch.fallenLogs = true; birch.leafLitter = true; Animals(birch); Monsters(birch);
            var dark = R("dark_forest", 0.7f, 0.8f, 0x507A32, 0x59AE30); dark.trees = new[] { "dark_oak", "dark_oak", "dark_oak", "huge_red_mushroom", "huge_brown_mushroom", "oak", "birch" }; dark.treesPerChunk = 16; dark.grassDensity = 0.08f; dark.mushrooms = true; dark.leafLitter = true; Animals(dark); Monsters(dark);
            dark.monsters.Add(new SpawnEntry("creaking", 0, 1, 1));
            var pale = R("pale_garden", 0.7f, 0.8f, 0x778272, 0x878D76, 0x76889D); pale.trees = new[] { "pale_oak", "pale_oak", "pale_oak", "creaking_heart_tree" }; pale.treesPerChunk = 14; pale.grassDensity = 0.05f; pale.flowers = new[] { "closed_eyeblossom", "open_eyeblossom" }; pale.flowerDensity = 0.03f;
            pale.sky = MathX.Hex(0xB9B9B9); pale.fog = MathX.Hex(0x817770); pale.pumpkins = false; Monsters(pale); pale.creatures.Clear();
            var meadow = R("meadow", 0.5f, 0.8f, 0x83BB6D, 0x63A948, 0x0E4ECF); meadow.trees = new[] { "oak", "birch" }; meadow.treesPerChunk = 0.1f; meadow.grassDensity = 0.5f; meadow.flowerDensity = 0.12f;
            meadow.flowers = new[] { "dandelion", "poppy", "allium", "azure_bluet", "oxeye_daisy", "cornflower", "wildflowers" }; meadow.isMountain = true;
            meadow.creatures.Add(new SpawnEntry("donkey", 1, 1, 2)); meadow.creatures.Add(new SpawnEntry("rabbit", 2, 2, 6)); meadow.creatures.Add(new SpawnEntry("sheep", 2, 2, 4)); Monsters(meadow);
            var cherry = R("cherry_grove", 0.5f, 0.8f, 0xB6DB61, 0xB6DB61, 0x5DB7EF); cherry.trees = new[] { "cherry" }; cherry.treesPerChunk = 3; cherry.grassDensity = 0.3f; cherry.flowerDensity = 0.05f; cherry.flowers = new[] { "pink_petals" }; cherry.isMountain = true;
            cherry.creatures.Add(new SpawnEntry("pig", 1, 1, 2)); cherry.creatures.Add(new SpawnEntry("rabbit", 2, 2, 6)); cherry.creatures.Add(new SpawnEntry("bee", 2, 2, 3)); Monsters(cherry);
            var swamp = R("swamp", 0.8f, 0.9f, 0x6A7039, 0x6A7039, 0x617B64); swamp.trees = new[] { "swamp_oak" }; swamp.treesPerChunk = 2; swamp.grassDensity = 0.1f; swamp.flowerDensity = 0.01f; swamp.flowers = new[] { "blue_orchid" }; swamp.lilyPads = true; swamp.mushrooms = true; swamp.sugarCane = true; swamp.seagrass = true;
            swamp.fog = MathX.Hex(0xC0D8FF); swamp.waterFog = MathX.Hex(0x232317); Animals(swamp); Monsters(swamp); swamp.monsters.Add(new SpawnEntry("slime", 100, 1, 1)); swamp.creatures.Add(new SpawnEntry("frog", 10, 2, 5));
            var mangrove = R("mangrove_swamp", 0.8f, 0.9f, 0x6A7039, 0x8DB127, 0x3A7A6A); mangrove.trees = new[] { "mangrove" }; mangrove.treesPerChunk = 7; mangrove.grassDensity = 0.05f; mangrove.top = "mud"; mangrove.filler = "mud"; mangrove.underwater = "mud"; mangrove.seagrass = true; mangrove.lilyPads = true;
            mangrove.creatures.Add(new SpawnEntry("frog", 10, 2, 5)); Monsters(mangrove); mangrove.water_.Add(new SpawnEntry("tropical_fish", 25, 8, 8));
            // ---------------- cold
            var taiga = R("taiga", 0.25f, 0.8f, 0x86B783, 0x68A464); taiga.trees = new[] { "spruce", "spruce", "pine" }; taiga.treesPerChunk = 9; taiga.grassDensity = 0.1f; taiga.fernDensity = 0.12f; taiga.sweetBerries = true; taiga.fallenLogs = true;
            Animals(taiga); taiga.creatures.Add(new SpawnEntry("wolf", 8, 4, 4)); taiga.creatures.Add(new SpawnEntry("rabbit", 4, 2, 3)); taiga.creatures.Add(new SpawnEntry("fox", 8, 2, 4)); Monsters(taiga);
            var ogt = R("old_growth_spruce_taiga", 0.25f, 0.8f, 0x86B87F, 0x68A55F); ogt.trees = new[] { "mega_spruce", "spruce", "spruce" }; ogt.treesPerChunk = 10; ogt.fernDensity = 0.25f; ogt.grassDensity = 0.1f; ogt.top = "podzol"; ogt.boulders = true; ogt.sweetBerries = true; ogt.mushrooms = true;
            Animals(ogt); ogt.creatures.Add(new SpawnEntry("wolf", 8, 4, 4)); ogt.creatures.Add(new SpawnEntry("fox", 8, 2, 4)); Monsters(ogt);
            var snowyTaiga = R("snowy_taiga", -0.5f, 0.4f, 0x80B497, 0x60A17B, 0x3D57D6); snowyTaiga.trees = new[] { "spruce", "pine" }; snowyTaiga.treesPerChunk = 8; snowyTaiga.fernDensity = 0.08f; snowyTaiga.frozenWater = true;
            Animals(snowyTaiga); snowyTaiga.creatures.Add(new SpawnEntry("wolf", 8, 4, 4)); snowyTaiga.creatures.Add(new SpawnEntry("fox", 8, 2, 4)); snowyTaiga.creatures.Add(new SpawnEntry("rabbit", 4, 2, 3)); Monsters(snowyTaiga);
            var snowyPlains = R("snowy_plains", 0f, 0.5f, 0x80B497, 0x60A17B, 0x3D57D6); snowyPlains.trees = new[] { "spruce" }; snowyPlains.treesPerChunk = 0.1f; snowyPlains.grassDensity = 0.02f; snowyPlains.frozenWater = true;
            snowyPlains.creatures.Add(new SpawnEntry("rabbit", 10, 2, 3)); snowyPlains.creatures.Add(new SpawnEntry("polar_bear", 1, 1, 2)); Monsters(snowyPlains, false); snowyPlains.monsters.Add(new SpawnEntry("stray", 80, 4, 4)); snowyPlains.monsters.Add(new SpawnEntry("zombie", 95, 2, 4));
            var iceSpikes = R("ice_spikes", 0f, 0.5f, 0x80B497, 0x60A17B, 0x3D57D6); iceSpikes.top = "snow_block"; iceSpikes.frozenWater = true; iceSpikes.trees = new[] { "ice_spike" }; iceSpikes.treesPerChunk = 0.6f;
            iceSpikes.creatures.Add(new SpawnEntry("rabbit", 10, 2, 3)); iceSpikes.creatures.Add(new SpawnEntry("polar_bear", 1, 1, 2)); Monsters(iceSpikes, false); iceSpikes.monsters.Add(new SpawnEntry("stray", 80, 4, 4));
            var grove = R("grove", -0.2f, 0.8f, 0x80B497, 0x60A17B, 0x3D57D6); grove.trees = new[] { "spruce" }; grove.treesPerChunk = 7; grove.top = "snow_block"; grove.isMountain = true; grove.frozenWater = true;
            grove.creatures.Add(new SpawnEntry("wolf", 8, 4, 4)); grove.creatures.Add(new SpawnEntry("fox", 8, 2, 4)); grove.creatures.Add(new SpawnEntry("rabbit", 4, 2, 3)); Monsters(grove);
            var snowySlopes = R("snowy_slopes", -0.3f, 0.9f, 0x80B497, 0x60A17B, 0x3D57D6); snowySlopes.top = "snow_block"; snowySlopes.filler = "snow_block"; snowySlopes.isMountain = true; snowySlopes.frozenWater = true;
            snowySlopes.creatures.Add(new SpawnEntry("rabbit", 4, 2, 3)); snowySlopes.creatures.Add(new SpawnEntry("goat", 5, 1, 3)); Monsters(snowySlopes);
            var jagged = R("jagged_peaks", -0.7f, 0.9f, 0x80B497, 0x60A17B, 0x3D57D6); jagged.top = "snow_block"; jagged.filler = "stone"; jagged.isMountain = true; jagged.creatures.Add(new SpawnEntry("goat", 5, 1, 3)); Monsters(jagged);
            var frozenPeaks = R("frozen_peaks", -0.7f, 0.9f, 0x80B497, 0x60A17B, 0x3D57D6); frozenPeaks.top = "snow_block"; frozenPeaks.filler = "packed_ice"; frozenPeaks.isMountain = true; frozenPeaks.creatures.Add(new SpawnEntry("goat", 5, 1, 3)); Monsters(frozenPeaks);
            var stonyPeaks = R("stony_peaks", 1f, 0.3f, 0x9ABE4B, 0x82AC1E); stonyPeaks.top = "stone"; stonyPeaks.filler = "stone"; stonyPeaks.isMountain = true; Monsters(stonyPeaks);
            var wh = R("windswept_hills", 0.2f, 0.3f, 0x8AB689, 0x6DA36B, 0x3D57D6); wh.trees = new[] { "oak", "spruce" }; wh.treesPerChunk = 0.4f; wh.grassDensity = 0.2f; wh.isMountain = true;
            Animals(wh); wh.creatures.Add(new SpawnEntry("llama", 5, 4, 6)); Monsters(wh);
            var wgh = R("windswept_gravelly_hills", 0.2f, 0.3f, 0x8AB689, 0x6DA36B, 0x3D57D6); wgh.top = "gravel"; wgh.filler = "gravel"; wgh.isMountain = true; wgh.trees = new[] { "spruce" }; wgh.treesPerChunk = 0.3f; Animals(wgh); Monsters(wgh);
            // ---------------- warm / dry
            Desert = R("desert", 2f, 0f, 0xBFB755, 0xAEA42A, 0x32A598); Desert.top = "sand"; Desert.filler = "sand"; Desert.fillerDepth = 5; Desert.cactus = true; Desert.deadBush = true; Desert.pumpkins = false;
            Desert.creatures.Add(new SpawnEntry("rabbit", 4, 2, 3)); Desert.creatures.Add(new SpawnEntry("camel", 1, 1, 1)); Desert.creatures.Add(new SpawnEntry("armadillo", 2, 1, 2)); Monsters(Desert, false); Desert.monsters.Add(new SpawnEntry("husk", 80, 4, 4)); Desert.monsters.Add(new SpawnEntry("zombie", 19, 4, 4));
            var savanna = R("savanna", 2f, 0f, 0xBFB755, 0xAEA42A, 0x2C8B9C); savanna.trees = new[] { "acacia", "acacia", "oak" }; savanna.treesPerChunk = 1f; savanna.grassDensity = 0.4f; savanna.tallGrassChance = 0.3f;
            Animals(savanna); savanna.creatures.Add(new SpawnEntry("horse", 1, 2, 6)); savanna.creatures.Add(new SpawnEntry("donkey", 1, 1, 1)); savanna.creatures.Add(new SpawnEntry("llama", 8, 4, 4)); savanna.creatures.Add(new SpawnEntry("armadillo", 10, 2, 3)); Monsters(savanna);
            var savPlat = R("savanna_plateau", 2f, 0f, 0xBFB755, 0xAEA42A, 0x2C8B9C); savPlat.trees = new[] { "acacia" }; savPlat.treesPerChunk = 1.2f; savPlat.grassDensity = 0.4f; Animals(savPlat); savPlat.creatures.Add(new SpawnEntry("llama", 8, 4, 4)); Monsters(savPlat);
            var badlands = R("badlands", 2f, 0f, 0x90814D, 0x9E814D, 0x4E7F81); badlands.top = "red_sand"; badlands.filler = "terracotta"; badlands.deadBush = true; badlands.cactus = true; badlands.pumpkins = false; badlands.sky = MathX.Hex(0x6EB1FF);
            badlands.creatures.Add(new SpawnEntry("armadillo", 6, 1, 2)); Monsters(badlands);
            var eroded = R("eroded_badlands", 2f, 0f, 0x90814D, 0x9E814D, 0x4E7F81); eroded.top = "red_sand"; eroded.filler = "terracotta"; eroded.deadBush = true; eroded.pumpkins = false; eroded.creatures.Add(new SpawnEntry("armadillo", 6, 1, 2)); Monsters(eroded);
            var wooded = R("wooded_badlands", 2f, 0f, 0x90814D, 0x9E814D, 0x4E7F81); wooded.top = "coarse_dirt"; wooded.filler = "terracotta"; wooded.trees = new[] { "oak" }; wooded.treesPerChunk = 3; wooded.grassDensity = 0.2f; wooded.creatures.Add(new SpawnEntry("armadillo", 6, 1, 2)); Monsters(wooded);
            var jungle = R("jungle", 0.95f, 0.9f, 0x59C93C, 0x30BB0B, 0x14A2C5); jungle.trees = new[] { "jungle", "jungle", "mega_jungle", "jungle_bush", "jungle_bush", "oak" }; jungle.treesPerChunk = 30; jungle.grassDensity = 0.3f; jungle.fernDensity = 0.1f; jungle.melons = true; jungle.flowers = new[] { "dandelion", "poppy" }; jungle.flowerDensity = 0.005f;
            Animals(jungle); jungle.creatures.Add(new SpawnEntry("parrot", 40, 1, 2)); jungle.creatures.Add(new SpawnEntry("panda", 1, 1, 2)); jungle.creatures.Add(new SpawnEntry("ocelot", 2, 1, 3)); Monsters(jungle);
            var sparse = R("sparse_jungle", 0.95f, 0.8f, 0x64C73F, 0x3EB80F, 0x14A2C5); sparse.trees = new[] { "jungle", "jungle_bush", "oak" }; sparse.treesPerChunk = 2; sparse.grassDensity = 0.3f; sparse.melons = true; Animals(sparse); Monsters(sparse);
            var bamboo = R("bamboo_jungle", 0.95f, 0.9f, 0x59C93C, 0x30BB0B, 0x14A2C5); bamboo.trees = new[] { "jungle", "mega_jungle", "jungle_bush" }; bamboo.treesPerChunk = 10; bamboo.bamboo = true; bamboo.grassDensity = 0.2f; bamboo.top = "podzol";
            Animals(bamboo); bamboo.creatures.Add(new SpawnEntry("panda", 80, 1, 2)); bamboo.creatures.Add(new SpawnEntry("parrot", 40, 1, 2)); bamboo.creatures.Add(new SpawnEntry("ocelot", 2, 1, 1)); Monsters(bamboo);
            var mush = R("mushroom_fields", 0.9f, 1f, 0x55C93F, 0x2BBB0F); mush.top = "mycelium"; mush.trees = new[] { "huge_red_mushroom", "huge_brown_mushroom" }; mush.treesPerChunk = 1; mush.mushrooms = true; mush.creatures.Add(new SpawnEntry("mooshroom", 8, 4, 8)); mush.pumpkins = false;
            // ---------------- water & coast
            River = R("river", 0.5f, 0.5f, 0x8EB971, 0x71A74D); River.top = "sand"; River.filler = "sand"; River.underwater = "sand"; River.isRiver = true; River.sugarCane = true; River.seagrass = true; River.water_.Add(new SpawnEntry("salmon", 5, 1, 5)); River.water_.Add(new SpawnEntry("squid", 2, 1, 4)); Monsters(River);
            River.monsters.Add(new SpawnEntry("drowned", 100, 1, 1));
            var frozenRiver = R("frozen_river", 0f, 0.5f, 0x80B497, 0x60A17B, 0x3938C9); frozenRiver.isRiver = true; frozenRiver.frozenWater = true; frozenRiver.top = "sand"; frozenRiver.underwater = "gravel"; Monsters(frozenRiver);
            var beach = R("beach", 0.8f, 0.4f, 0x91BD59, 0x77AB2F); beach.top = "sand"; beach.filler = "sand"; beach.isBeach = true; beach.sugarCane = true; beach.creatures.Add(new SpawnEntry("turtle", 5, 2, 5)); Monsters(beach);
            var snowyBeach = R("snowy_beach", 0.05f, 0.3f, 0x80B497, 0x60A17B, 0x3D57D6); snowyBeach.top = "sand"; snowyBeach.filler = "sand"; snowyBeach.isBeach = true; snowyBeach.frozenWater = true; Monsters(snowyBeach);
            var stonyShore = R("stony_shore", 0.2f, 0.3f, 0x8AB689, 0x6DA36B, 0x3D57D6); stonyShore.top = "stone"; stonyShore.filler = "stone"; stonyShore.isBeach = true; Monsters(stonyShore);
            Ocean = R("ocean", 0.5f, 0.5f, 0x8EB971, 0x71A74D); Ocean.isOcean = true; Ocean.top = "gravel"; Ocean.filler = "gravel"; Ocean.underwater = "gravel"; Ocean.seagrass = true; Ocean.kelp = true;
            Ocean.water_.Add(new SpawnEntry("squid", 10, 1, 4)); Ocean.water_.Add(new SpawnEntry("cod", 10, 3, 6)); Ocean.water_.Add(new SpawnEntry("dolphin", 1, 1, 2)); Monsters(Ocean); Ocean.monsters.Add(new SpawnEntry("drowned", 100, 1, 1));
            var deepOcean = R("deep_ocean", 0.5f, 0.5f, 0x8EB971, 0x71A74D); deepOcean.isOcean = true; deepOcean.top = "gravel"; deepOcean.filler = "gravel"; deepOcean.underwater = "gravel"; deepOcean.seagrass = true; deepOcean.kelp = true;
            deepOcean.water_.Add(new SpawnEntry("squid", 10, 1, 4)); deepOcean.water_.Add(new SpawnEntry("cod", 10, 3, 6)); deepOcean.water_.Add(new SpawnEntry("dolphin", 1, 1, 2)); Monsters(deepOcean); deepOcean.monsters.Add(new SpawnEntry("drowned", 100, 1, 1));
            var warm = R("warm_ocean", 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x43D5EE); warm.isOcean = true; warm.top = "sand"; warm.filler = "sand"; warm.underwater = "sand"; warm.coral = true; warm.seagrass = true;
            warm.water_.Add(new SpawnEntry("tropical_fish", 25, 8, 8)); warm.water_.Add(new SpawnEntry("pufferfish", 15, 1, 3)); warm.water_.Add(new SpawnEntry("dolphin", 2, 1, 2)); Monsters(warm); warm.monsters.Add(new SpawnEntry("drowned", 5, 1, 1)); warm.water_.Add(new SpawnEntry("nautilus", 2, 1, 1));
            var lukewarm = R("lukewarm_ocean", 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x45ADF2); lukewarm.isOcean = true; lukewarm.top = "sand"; lukewarm.filler = "sand"; lukewarm.underwater = "sand"; lukewarm.seagrass = true; lukewarm.kelp = true;
            lukewarm.water_.Add(new SpawnEntry("tropical_fish", 25, 8, 8)); lukewarm.water_.Add(new SpawnEntry("cod", 15, 3, 6)); lukewarm.water_.Add(new SpawnEntry("pufferfish", 5, 1, 3)); Monsters(lukewarm); lukewarm.monsters.Add(new SpawnEntry("drowned", 5, 1, 1));
            var cold = R("cold_ocean", 0.5f, 0.5f, 0x8EB971, 0x71A74D, 0x3D57D6); cold.isOcean = true; cold.top = "gravel"; cold.filler = "gravel"; cold.underwater = "gravel"; cold.seagrass = true; cold.kelp = true;
            cold.water_.Add(new SpawnEntry("cod", 15, 3, 6)); cold.water_.Add(new SpawnEntry("salmon", 15, 1, 5)); cold.water_.Add(new SpawnEntry("squid", 3, 1, 4)); Monsters(cold); cold.monsters.Add(new SpawnEntry("drowned", 100, 1, 1));
            var frozenOcean = R("frozen_ocean", 0f, 0.5f, 0x80B497, 0x60A17B, 0x3938C9); frozenOcean.isOcean = true; frozenOcean.top = "gravel"; frozenOcean.underwater = "gravel"; frozenOcean.frozenWater = true; frozenOcean.icebergs = true; frozenOcean.precipitation = Precipitation.Snow;
            frozenOcean.water_.Add(new SpawnEntry("salmon", 15, 1, 5)); frozenOcean.creatures.Add(new SpawnEntry("polar_bear", 1, 1, 2)); Monsters(frozenOcean, false); frozenOcean.monsters.Add(new SpawnEntry("stray", 100, 1, 1)); frozenOcean.monsters.Add(new SpawnEntry("drowned", 100, 1, 1));
            // ---------------- caves (3D)
            Lush_Caves = R("lush_caves", 0.5f, 0.5f, 0x8EB971, 0x71A74D); Lush_Caves.isCave = true; Lush_Caves.creatures.Add(new SpawnEntry("axolotl", 10, 4, 6)); Lush_Caves.water_.Add(new SpawnEntry("tropical_fish", 25, 8, 8)); Monsters(Lush_Caves);
            Dripstone_Caves = R("dripstone_caves", 0.8f, 0.4f, 0x91BD59, 0x77AB2F); Dripstone_Caves.isCave = true; Monsters(Dripstone_Caves); Dripstone_Caves.monsters.Add(new SpawnEntry("drowned", 95, 4, 4));
            Deep_Dark = R("deep_dark", 0.8f, 0.4f, 0x91BD59, 0x77AB2F); Deep_Dark.isCave = true; Deep_Dark.fog = MathX.Hex(0x0A1A1A);
            Sulfur_Caves = R("sulfur_caves", 1.2f, 0.2f, 0xA8B24A, 0x9AA83A, 0x8AB860); Sulfur_Caves.isCave = true; Sulfur_Caves.fog = MathX.Hex(0x9A9A5A); Sulfur_Caves.monsters.Add(new SpawnEntry("sulfur_cube", 60, 1, 3)); Monsters(Sulfur_Caves);
            // ---------------- nether
            Nether_Wastes = NetherB("nether_wastes", 0x330808, 0x3A0A0A); Nether_Wastes.monsters.Add(new SpawnEntry("ghast", 50, 4, 4)); Nether_Wastes.monsters.Add(new SpawnEntry("zombified_piglin", 100, 4, 4)); Nether_Wastes.monsters.Add(new SpawnEntry("magma_cube", 2, 4, 4)); Nether_Wastes.monsters.Add(new SpawnEntry("enderman", 1, 4, 4)); Nether_Wastes.monsters.Add(new SpawnEntry("piglin", 15, 4, 4)); Nether_Wastes.creatures.Add(new SpawnEntry("strider", 60, 1, 2));
            var crimson = NetherB("crimson_forest", 0x330303, 0x7A0A0A); crimson.ambientParticle = MathX.Hex(0xE84A4A); crimson.particleDensity = 0.025f; crimson.monsters.Add(new SpawnEntry("zombified_piglin", 1, 2, 4)); crimson.monsters.Add(new SpawnEntry("hoglin", 9, 3, 4)); crimson.monsters.Add(new SpawnEntry("piglin", 5, 3, 4)); crimson.creatures.Add(new SpawnEntry("strider", 60, 1, 2));
            var warped = NetherB("warped_forest", 0x1A051A, 0x0A3A3A); warped.ambientParticle = MathX.Hex(0x3ACCC0); warped.particleDensity = 0.012f; warped.monsters.Add(new SpawnEntry("enderman", 1, 4, 4)); warped.creatures.Add(new SpawnEntry("strider", 60, 1, 2));
            var soulValley = NetherB("soul_sand_valley", 0x1B4745, 0x2A4A48); soulValley.ambientParticle = MathX.Hex(0x9AD8D0); soulValley.particleDensity = 0.004f; soulValley.monsters.Add(new SpawnEntry("skeleton", 20, 5, 5)); soulValley.monsters.Add(new SpawnEntry("ghast", 50, 4, 4)); soulValley.monsters.Add(new SpawnEntry("enderman", 1, 4, 4)); soulValley.creatures.Add(new SpawnEntry("strider", 60, 1, 2));
            var deltas = NetherB("basalt_deltas", 0x685F70, 0x5A5460); deltas.ambientParticle = MathX.Hex(0x9A9AA0); deltas.particleDensity = 0.12f; deltas.monsters.Add(new SpawnEntry("ghast", 40, 1, 1)); deltas.monsters.Add(new SpawnEntry("magma_cube", 100, 2, 5)); deltas.creatures.Add(new SpawnEntry("strider", 60, 1, 2));
            // ---------------- end
            The_End = R("the_end", 0.5f, 0f, 0x8EB971, 0x71A74D); The_End.dim = DimensionId.End; The_End.sky = MathX.Hex(0x000000); The_End.fog = MathX.Hex(0x0A080C); The_End.monsters.Add(new SpawnEntry("enderman", 10, 4, 4)); The_End.precipitation = Precipitation.None;
            var endHigh = R("end_highlands", 0.5f, 0f, 0x8EB971, 0x71A74D); endHigh.dim = DimensionId.End; endHigh.sky = MathX.Hex(0x000000); endHigh.fog = MathX.Hex(0x0A080C); endHigh.monsters.Add(new SpawnEntry("enderman", 10, 4, 4)); endHigh.precipitation = Precipitation.None;
            var endBarrens = R("end_barrens", 0.5f, 0f, 0x8EB971, 0x71A74D); endBarrens.dim = DimensionId.End; endBarrens.sky = MathX.Hex(0x000000); endBarrens.fog = MathX.Hex(0x0A080C); endBarrens.monsters.Add(new SpawnEntry("enderman", 10, 4, 4)); endBarrens.precipitation = Precipitation.None;
        }

        static Biome NetherB(string key, uint fog, uint sky)
        {
            var b = R(key, 2f, 0f, 0xBFB755, 0xAEA42A);
            b.dim = DimensionId.Nether; b.fog = MathX.Hex(fog); b.sky = MathX.Hex(sky); b.precipitation = Precipitation.None; b.pumpkins = false;
            return b;
        }

        public string Display => name;
    }
}
