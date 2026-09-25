using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public enum SpawnReason { Natural, SpawnEgg, Spawner, Structure, Breeding, Bucket, Command, Dispenser, Conversion, Summon, Reinforcement, ChunkGen }
    public enum MobCategory { Monster, Creature, WaterCreature, WaterAmbient, Ambient, Boss, Misc }

    public sealed class MobDrop
    {
        public string item; public int min, max; public float chance = 1f, lootingChance; public string cooked; public bool playerKillOnly;
        public MobDrop(string item, int min, int max, float chance = 1f, string cooked = null) { this.item = item; this.min = min; this.max = max; this.chance = chance; this.cooked = cooked; }
    }

    public sealed class MobDef
    {
        public string id, displayName, model;
        public MobCategory category = MobCategory.Creature;
        public float maxHealth = 10, speed = 0.25f, attackDamage = 2, armor, knockbackResistance, followRange = 16, flySpeed;
        public float width = 0.6f, height = 1.8f, eyeFactor = 0.85f;
        public bool hostile, neutral, aquatic, amphibious, flying, fireImmune, undead, arthropod, burnsInDay, canBreed, noGravity, hasEgg = true, boss;
        public int xp = 5;
        public List<MobDrop> drops = new List<MobDrop>();
        public Color32 eggBase = new Color32(128, 128, 128, 255), eggSpots = new Color32(64, 64, 64, 255);
        public Func<Mob> factory;
        public string[] foods = new string[0];
        public string[] variants;
        public string soundId;
        public int ambientInterval = 120;
        public float modelScale = 1f;

        public MobDef Drop(string item, int min, int max, float chance = 1f, string cooked = null, float looting = 0.01f) { drops.Add(new MobDrop(item, min, max, chance, cooked) { lootingChance = looting }); return this; }
        public MobDef RareDrop(string item, float chance) { drops.Add(new MobDrop(item, 1, 1, chance) { lootingChance = 0.01f, playerKillOnly = true }); return this; }
    }

    public static class MobRegistry
    {
        static readonly Dictionary<string, MobDef> byId = new Dictionary<string, MobDef>();
        static readonly List<MobDef> all = new List<MobDef>();
        static bool inited;

        public static IReadOnlyList<MobDef> All { get { Init(); return all; } }
        public static MobDef Get(string id) { Init(); return id != null && byId.TryGetValue(id, out var d) ? d : null; }
        public static bool IsHostile(string id) { var d = Get(id); return d != null && (d.hostile || d.boss); }
        public static bool IsRaider(string id) => id == "pillager" || id == "vindicator" || id == "evoker" || id == "ravager" || id == "witch" || id == "illusioner";

        static MobDef R(string id, MobCategory cat, float hp, float speed, float w, float h, uint egg1, uint egg2, Func<Mob> f)
        {
            var d = new MobDef { id = id, displayName = Blocks.PrettyName(id), model = id, category = cat, maxHealth = hp, speed = speed, width = w, height = h, eggBase = MathX.Hex(egg1), eggSpots = MathX.Hex(egg2), factory = f, soundId = id };
            if (cat == MobCategory.Monster) { d.hostile = true; d.xp = 5; }
            else if (cat == MobCategory.Creature || cat == MobCategory.WaterCreature || cat == MobCategory.WaterAmbient || cat == MobCategory.Ambient) d.xp = 1 + UnityEngine.Random.Range(0, 3);
            byId[id] = d; all.Add(d);
            return d;
        }

        static void Init()
        {
            if (inited) return; inited = true;
            string[] meatFoods = { "beef", "cooked_beef", "porkchop", "cooked_porkchop", "chicken", "cooked_chicken", "mutton", "cooked_mutton", "rabbit", "cooked_rabbit", "rotten_flesh" };
            // ------------------------------------------------------------ passive / utility
            var d = R("pig", MobCategory.Creature, 10, 0.25f, 0.9f, 0.9f, 0xF0A5A2, 0xDB635F, () => new AnimalMob()); d.canBreed = true; d.foods = new[] { "carrot", "potato", "beetroot" }; d.Drop("porkchop", 1, 3, 1, "cooked_porkchop");
            d = R("cow", MobCategory.Creature, 10, 0.2f, 0.9f, 1.4f, 0x443626, 0xA1A1A1, () => new AnimalMob()); d.canBreed = true; d.foods = new[] { "wheat" }; d.Drop("beef", 1, 3, 1, "cooked_beef").Drop("leather", 0, 2);
            d = R("mooshroom", MobCategory.Creature, 10, 0.2f, 0.9f, 1.4f, 0xA00F10, 0xB7B7B7, () => new AnimalMob()); d.canBreed = true; d.foods = new[] { "wheat" }; d.Drop("beef", 1, 3, 1, "cooked_beef").Drop("leather", 0, 2);
            d = R("sheep", MobCategory.Creature, 8, 0.23f, 0.9f, 1.3f, 0xE7E7E7, 0xFFB5B5, () => new SheepMob()); d.canBreed = true; d.foods = new[] { "wheat" }; d.Drop("mutton", 1, 2, 1, "cooked_mutton").Drop("$wool", 1, 1);
            d = R("chicken", MobCategory.Creature, 4, 0.25f, 0.4f, 0.7f, 0xA1A1A1, 0xFF0000, () => new ChickenMob()); d.canBreed = true; d.foods = new[] { "wheat_seeds", "melon_seeds", "pumpkin_seeds", "beetroot_seeds" }; d.Drop("chicken", 1, 1, 1, "cooked_chicken").Drop("feather", 0, 2);
            d = R("rabbit", MobCategory.Creature, 3, 0.3f, 0.4f, 0.5f, 0x995F40, 0x734831, () => new RabbitMob()); d.canBreed = true; d.foods = new[] { "carrot", "golden_carrot", "dandelion" }; d.Drop("rabbit", 0, 1, 1, "cooked_rabbit").Drop("rabbit_hide", 0, 1).Drop("rabbit_foot", 1, 1, 0.1f);
            d = R("horse", MobCategory.Creature, 22, 0.225f, 1.3965f, 1.6f, 0xC09E7D, 0xEEE500, () => new HorseMob()); d.canBreed = true; d.foods = new[] { "golden_apple", "golden_carrot", "apple", "wheat", "sugar", "hay_block" }; d.Drop("leather", 0, 2);
            d = R("donkey", MobCategory.Creature, 20, 0.175f, 1.3965f, 1.5f, 0x534539, 0x867566, () => new HorseMob()); d.canBreed = true; d.foods = new[] { "golden_apple", "golden_carrot", "apple", "wheat" }; d.Drop("leather", 0, 2);
            d = R("mule", MobCategory.Creature, 20, 0.175f, 1.3965f, 1.6f, 0x1B0200, 0x51331D, () => new HorseMob()); d.foods = new[] { "golden_apple", "golden_carrot", "apple" }; d.Drop("leather", 0, 2);
            d = R("skeleton_horse", MobCategory.Creature, 15, 0.2f, 1.3965f, 1.6f, 0x68684F, 0xE5E5D8, () => new HorseMob()); d.undead = true; d.Drop("bone", 0, 2);
            d = R("zombie_horse", MobCategory.Creature, 15, 0.2f, 1.3965f, 1.6f, 0x315234, 0x97C284, () => new HorseMob()); d.undead = true; d.Drop("rotten_flesh", 0, 2);
            d = R("llama", MobCategory.Creature, 22, 0.175f, 0.9f, 1.87f, 0xC09E7D, 0x995F40, () => new LlamaMob()); d.canBreed = true; d.foods = new[] { "hay_block", "wheat" }; d.Drop("leather", 0, 2); d.neutral = true;
            d = R("trader_llama", MobCategory.Creature, 22, 0.175f, 0.9f, 1.87f, 0xEAA430, 0x456296, () => new LlamaMob()); d.Drop("leather", 0, 2); d.neutral = true;
            d = R("camel", MobCategory.Creature, 32, 0.09f, 1.7f, 2.375f, 0xFCC369, 0xCB9337, () => new CamelMob()); d.canBreed = true; d.foods = new[] { "cactus" };
            d = R("camel_husk", MobCategory.Monster, 32, 0.09f, 1.7f, 2.375f, 0x8A7A5A, 0x5A4A3A, () => new CamelMob()); d.undead = true; d.Drop("rotten_flesh", 0, 3);
            d = R("wolf", MobCategory.Creature, 8, 0.3f, 0.6f, 0.85f, 0xD7D3D3, 0xCEAF96, () => new WolfMob()); d.neutral = true; d.attackDamage = 4; d.foods = meatFoods; d.canBreed = true;
            d = R("cat", MobCategory.Creature, 10, 0.3f, 0.6f, 0.7f, 0xEFC88E, 0x957256, () => new CatMob()); d.foods = new[] { "cod", "salmon" }; d.canBreed = true; d.Drop("string", 0, 2);
            d = R("ocelot", MobCategory.Creature, 10, 0.3f, 0.6f, 0.7f, 0xEFDE7D, 0x564434, () => new CatMob()); d.foods = new[] { "cod", "salmon" }; d.canBreed = true;
            d = R("fox", MobCategory.Creature, 10, 0.3f, 0.6f, 0.7f, 0xD5B69F, 0xCC6920, () => new FoxMob()); d.foods = new[] { "sweet_berries", "glow_berries" }; d.canBreed = true; d.attackDamage = 2;
            d = R("goat", MobCategory.Creature, 10, 0.2f, 0.9f, 1.3f, 0xA5947C, 0x55493E, () => new GoatMob()); d.canBreed = true; d.foods = new[] { "wheat" }; d.attackDamage = 2;
            d = R("polar_bear", MobCategory.Creature, 30, 0.25f, 1.4f, 1.4f, 0xF2F2F2, 0x959590, () => new NeutralBeastMob()); d.neutral = true; d.attackDamage = 6; d.Drop("cod", 0, 2, 0.75f).Drop("salmon", 0, 2, 0.25f);
            d = R("panda", MobCategory.Creature, 20, 0.15f, 1.3f, 1.25f, 0xE7E7E7, 0x1B1B22, () => new NeutralBeastMob()); d.neutral = true; d.attackDamage = 6; d.canBreed = true; d.foods = new[] { "bamboo" }; d.Drop("bamboo", 1, 1);
            d = R("turtle", MobCategory.Creature, 30, 0.25f, 1.2f, 0.4f, 0xE7E7E7, 0x00AFAF, () => new TurtleMob()); d.amphibious = true; d.canBreed = true; d.foods = new[] { "seagrass" }; d.Drop("seagrass", 0, 2);
            d = R("frog", MobCategory.Creature, 10, 1.0f, 0.5f, 0.5f, 0xD07444, 0xFFC77C, () => new FrogMob()); d.amphibious = true; d.foods = new[] { "slime_ball" }; d.canBreed = true;
            d = R("tadpole", MobCategory.WaterAmbient, 6, 1.0f, 0.4f, 0.3f, 0x6D533D, 0x160A00, () => new FishMob()); d.aquatic = true; d.xp = 0;
            d = R("axolotl", MobCategory.WaterCreature, 14, 1.0f, 0.75f, 0.42f, 0xFBC1E3, 0xA62D74, () => new AxolotlMob()); d.amphibious = true; d.aquatic = true; d.foods = new[] { "tropical_fish_bucket" }; d.canBreed = true; d.attackDamage = 2;
            d = R("armadillo", MobCategory.Creature, 12, 0.14f, 0.7f, 0.65f, 0xAD716D, 0x824848, () => new AnimalMob()); d.canBreed = true; d.foods = new[] { "spider_eye" }; d.Drop("armadillo_scute", 0, 1, 0.3f);
            d = R("sniffer", MobCategory.Creature, 14, 0.1f, 1.9f, 1.75f, 0x871E09, 0x5A9E5C, () => new SnifferMob()); d.canBreed = true; d.foods = new[] { "torchflower_seeds" };
            d = R("villager", MobCategory.Misc, 20, 0.5f, 0.6f, 1.95f, 0x563C33, 0xBD8B72, () => new VillagerMob()); d.xp = 0; d.speed = 0.5f;
            d = R("wandering_trader", MobCategory.Misc, 20, 0.5f, 0.6f, 1.95f, 0x456296, 0xEAA430, () => new VillagerMob()); d.xp = 0;
            d = R("iron_golem", MobCategory.Misc, 100, 0.25f, 1.4f, 2.7f, 0xDBCDC1, 0x74A332, () => new IronGolemMob()); d.attackDamage = 15; d.knockbackResistance = 1; d.neutral = true; d.Drop("iron_ingot", 3, 5).Drop("poppy", 0, 2); d.xp = 0;
            d = R("snow_golem", MobCategory.Misc, 4, 0.2f, 0.7f, 1.9f, 0xD9F2F2, 0x81A4A4, () => new SnowGolemMob()); d.Drop("snowball", 0, 15); d.xp = 0;
            d = R("copper_golem", MobCategory.Misc, 12, 0.2f, 0.49f, 0.98f, 0xC46E4B, 0x5AA890, () => new CopperGolemMob()); d.xp = 0;
            d = R("allay", MobCategory.Creature, 20, 0.1f, 0.35f, 0.6f, 0x00DAFF, 0x00ADFF, () => new FlyerMob()); d.flying = true; d.flySpeed = 0.1f; d.noGravity = true;
            d = R("bat", MobCategory.Ambient, 6, 0.1f, 0.5f, 0.9f, 0x4C3E30, 0x0F0F0F, () => new BatMob()); d.flying = true; d.noGravity = true; d.xp = 0;
            d = R("parrot", MobCategory.Creature, 6, 0.2f, 0.5f, 0.9f, 0x0DA70B, 0xFF0000, () => new FlyerMob()); d.flying = true; d.flySpeed = 0.4f; d.foods = new[] { "wheat_seeds", "melon_seeds", "pumpkin_seeds", "beetroot_seeds" }; d.Drop("feather", 1, 2);
            d = R("bee", MobCategory.Creature, 10, 0.3f, 0.7f, 0.6f, 0xEDC343, 0x43241B, () => new BeeMob()); d.flying = true; d.noGravity = true; d.flySpeed = 0.6f; d.neutral = true; d.attackDamage = 2; d.arthropod = true; d.foods = new[] { "dandelion", "poppy", "cornflower", "allium" }; d.canBreed = true;
            d = R("happy_ghast", MobCategory.Creature, 20, 0.05f, 4f, 4f, 0xF8F8F8, 0x8AB0E0, () => new HappyGhastMob()); d.flying = true; d.noGravity = true; d.flySpeed = 0.05f; d.foods = new[] { "snowball" };
            d = R("ghastling", MobCategory.Creature, 20, 0.05f, 1f, 1f, 0xF8F8F8, 0xB0D0F0, () => new HappyGhastMob()); d.flying = true; d.noGravity = true; d.flySpeed = 0.04f; d.foods = new[] { "snowball" };
            // ------------------------------------------------------------ water
            d = R("cod", MobCategory.WaterAmbient, 3, 1f, 0.5f, 0.3f, 0xC1A76A, 0xE5C48B, () => new FishMob()); d.aquatic = true; d.Drop("cod", 1, 1, 1, "cooked_cod").Drop("bone_meal", 1, 1, 0.05f);
            d = R("salmon", MobCategory.WaterAmbient, 3, 1f, 0.7f, 0.4f, 0xA00F10, 0x0E8474, () => new FishMob()); d.aquatic = true; d.Drop("salmon", 1, 1, 1, "cooked_salmon").Drop("bone_meal", 1, 1, 0.05f);
            d = R("tropical_fish", MobCategory.WaterAmbient, 3, 1f, 0.5f, 0.4f, 0xEF6915, 0xFFF9EF, () => new FishMob()); d.aquatic = true; d.Drop("tropical_fish", 1, 1).Drop("bone_meal", 1, 1, 0.05f);
            d = R("pufferfish", MobCategory.WaterAmbient, 3, 1f, 0.7f, 0.7f, 0xF6B201, 0x37C3F2, () => new PufferfishMob()); d.aquatic = true; d.Drop("pufferfish", 1, 1).Drop("bone_meal", 1, 1, 0.05f);
            d = R("squid", MobCategory.WaterCreature, 10, 0.7f, 0.8f, 0.8f, 0x223B4D, 0x708899, () => new SquidMob()); d.aquatic = true; d.Drop("ink_sac", 1, 3);
            d = R("glow_squid", MobCategory.WaterCreature, 10, 0.7f, 0.8f, 0.8f, 0x095656, 0x85F1BC, () => new SquidMob()); d.aquatic = true; d.Drop("glow_ink_sac", 1, 3);
            d = R("dolphin", MobCategory.WaterCreature, 10, 1.2f, 0.9f, 0.6f, 0x223B4D, 0xF9F9F9, () => new DolphinMob()); d.aquatic = true; d.neutral = true; d.attackDamage = 3; d.Drop("cod", 0, 1, 1, "cooked_cod");
            d = R("nautilus", MobCategory.WaterCreature, 15, 0.9f, 0.9f, 0.9f, 0xE8D8C0, 0xB06040, () => new FishMob()); d.aquatic = true; d.Drop("nautilus_shell", 0, 1, 0.1f);
            d = R("zombie_nautilus", MobCategory.Monster, 15, 0.9f, 0.9f, 0.9f, 0x6A8A6A, 0x3A5A3A, () => new FishMob()); d.aquatic = true; d.undead = true; d.Drop("rotten_flesh", 0, 2);
            d = R("guardian", MobCategory.Monster, 30, 0.5f, 0.85f, 0.85f, 0x5A8272, 0xF17D30, () => new GuardianMob()); d.aquatic = true; d.attackDamage = 6; d.xp = 10; d.Drop("prismarine_shard", 0, 2).Drop("cod", 0, 1, 0.5f, "cooked_cod").Drop("prismarine_crystals", 0, 1, 0.4f);
            d = R("elder_guardian", MobCategory.Monster, 80, 0.3f, 1.9975f, 1.9975f, 0xCECCBA, 0x747693, () => new GuardianMob()); d.aquatic = true; d.attackDamage = 8; d.xp = 10; d.Drop("prismarine_shard", 0, 2).Drop("wet_sponge", 1, 1).Drop("prismarine_crystals", 0, 1, 0.5f);
            // ------------------------------------------------------------ overworld hostiles
            d = R("zombie", MobCategory.Monster, 20, 0.23f, 0.6f, 1.95f, 0x00AFAF, 0x799C65, () => new ZombieMob()); d.attackDamage = 3; d.armor = 2; d.undead = true; d.burnsInDay = true; d.followRange = 35; d.Drop("rotten_flesh", 0, 2).RareDrop("iron_ingot", 0.025f).RareDrop("carrot", 0.025f).RareDrop("potato", 0.025f);
            d = R("husk", MobCategory.Monster, 20, 0.23f, 0.6f, 1.95f, 0x797061, 0xE6CC94, () => new ZombieMob()); d.attackDamage = 3; d.armor = 2; d.undead = true; d.followRange = 35; d.Drop("rotten_flesh", 0, 2).RareDrop("iron_ingot", 0.025f);
            d = R("drowned", MobCategory.Monster, 20, 0.23f, 0.6f, 1.95f, 0x8FF1D7, 0x799C65, () => new ZombieMob()); d.attackDamage = 3; d.armor = 2; d.undead = true; d.burnsInDay = true; d.amphibious = true; d.Drop("rotten_flesh", 0, 2).RareDrop("copper_ingot", 0.11f);
            d = R("zombie_villager", MobCategory.Monster, 20, 0.23f, 0.6f, 1.95f, 0x563C33, 0x799C65, () => new ZombieMob()); d.attackDamage = 3; d.armor = 2; d.undead = true; d.burnsInDay = true; d.Drop("rotten_flesh", 0, 2);
            d = R("skeleton", MobCategory.Monster, 20, 0.25f, 0.6f, 1.99f, 0xC1C1C1, 0x494949, () => new SkeletonMob()); d.undead = true; d.burnsInDay = true; d.Drop("bone", 0, 2).Drop("arrow", 0, 2);
            d = R("stray", MobCategory.Monster, 20, 0.25f, 0.6f, 1.99f, 0x617677, 0xDDEAEA, () => new SkeletonMob()); d.undead = true; d.burnsInDay = true; d.Drop("bone", 0, 2).Drop("arrow", 0, 2);
            d = R("bogged", MobCategory.Monster, 16, 0.25f, 0.6f, 1.99f, 0x8A9C72, 0x314D1B, () => new SkeletonMob()); d.undead = true; d.burnsInDay = true; d.Drop("bone", 0, 2).Drop("arrow", 0, 2);
            d = R("parched", MobCategory.Monster, 16, 0.25f, 0.6f, 1.99f, 0xD8C8A0, 0x8A6A4A, () => new SkeletonMob()); d.undead = true; d.Drop("bone", 0, 2).Drop("arrow", 0, 2);
            d = R("creeper", MobCategory.Monster, 20, 0.25f, 0.6f, 1.7f, 0x0DA70B, 0x000000, () => new CreeperMob()); d.Drop("gunpowder", 0, 2);
            d = R("spider", MobCategory.Monster, 16, 0.3f, 1.4f, 0.9f, 0x342D27, 0xA80E0E, () => new SpiderMob()); d.arthropod = true; d.attackDamage = 2; d.Drop("string", 0, 2).RareDrop("spider_eye", 0.33f);
            d = R("cave_spider", MobCategory.Monster, 12, 0.3f, 0.7f, 0.5f, 0x0C424E, 0xA80E0E, () => new SpiderMob()); d.arthropod = true; d.attackDamage = 2; d.Drop("string", 0, 2).RareDrop("spider_eye", 0.33f);
            d = R("enderman", MobCategory.Monster, 40, 0.3f, 0.6f, 2.9f, 0x161616, 0x000000, () => new EndermanMob()); d.neutral = true; d.hostile = false; d.attackDamage = 7; d.followRange = 64; d.Drop("ender_pearl", 0, 1);
            d = R("slime", MobCategory.Monster, 16, 0.2f, 2.04f, 2.04f, 0x51A03E, 0x7EBF6E, () => new SlimeMob()); d.attackDamage = 4; d.Drop("slime_ball", 0, 2);
            d = R("witch", MobCategory.Monster, 26, 0.25f, 0.6f, 1.95f, 0x340000, 0x51A03E, () => new WitchMob()); d.Drop("glass_bottle", 0, 2, 0.5f).Drop("glowstone_dust", 0, 2, 0.5f).Drop("gunpowder", 0, 2, 0.5f).Drop("redstone", 0, 2, 0.5f).Drop("spider_eye", 0, 2, 0.5f).Drop("sugar", 0, 2, 0.5f).Drop("stick", 0, 2, 0.5f);
            d = R("silverfish", MobCategory.Monster, 8, 0.25f, 0.4f, 0.3f, 0x6E6E6E, 0x303030, () => new HostileMeleeMob()); d.arthropod = true; d.attackDamage = 1;
            d = R("endermite", MobCategory.Monster, 8, 0.25f, 0.4f, 0.3f, 0x161616, 0x6E6E6E, () => new HostileMeleeMob()); d.arthropod = true; d.attackDamage = 2; d.xp = 3;
            d = R("phantom", MobCategory.Monster, 20, 0.7f, 0.9f, 0.5f, 0x43518A, 0x88FF00, () => new PhantomMob()); d.undead = true; d.burnsInDay = true; d.flying = true; d.noGravity = true; d.attackDamage = 6; d.Drop("phantom_membrane", 0, 1);
            d = R("pillager", MobCategory.Monster, 24, 0.35f, 0.6f, 1.95f, 0x532F36, 0x959B9B, () => new IllagerMob()); d.attackDamage = 5; d.Drop("arrow", 0, 2).RareDrop("crossbow", 0.085f);
            d = R("vindicator", MobCategory.Monster, 24, 0.35f, 0.6f, 1.95f, 0x959B9B, 0x275E61, () => new IllagerMob()); d.attackDamage = 5; d.Drop("emerald", 0, 1).RareDrop("iron_axe", 0.085f);
            d = R("evoker", MobCategory.Monster, 24, 0.5f, 0.6f, 1.95f, 0x959B9B, 0x1E1C1A, () => new IllagerMob()); d.Drop("totem_of_undying", 1, 1).Drop("emerald", 0, 1); d.xp = 10;
            d = R("vex", MobCategory.Monster, 14, 0.3f, 0.4f, 0.8f, 0x7A90A4, 0xE8EDF1, () => new FlyerMob()); d.flying = true; d.noGravity = true; d.attackDamage = 4; d.flySpeed = 0.25f; d.hostile = true; d.xp = 3;
            d = R("ravager", MobCategory.Monster, 100, 0.3f, 1.95f, 2.2f, 0x757470, 0x5B5049, () => new HostileMeleeMob()); d.attackDamage = 12; d.knockbackResistance = 0.75f; d.xp = 20; d.Drop("saddle", 1, 1);
            d = R("breeze", MobCategory.Monster, 30, 0.63f, 0.6f, 1.77f, 0xAF94DF, 0x9166DF, () => new BreezeMob()); d.Drop("breeze_rod", 1, 2);
            d = R("creaking", MobCategory.Monster, 1, 0.4f, 0.9f, 2.7f, 0x5F5F5F, 0xFC7812, () => new CreakingMob()); d.attackDamage = 3; d.xp = 0; d.hasEgg = true;
            d = R("warden", MobCategory.Monster, 500, 0.3f, 0.9f, 2.9f, 0x0F4649, 0x39D6E0, () => new WardenMob()); d.attackDamage = 30; d.knockbackResistance = 1; d.xp = 5; d.Drop("sculk_catalyst", 1, 1); d.followRange = 24;
            d = R("sulfur_cube", MobCategory.Monster, 8, 0.2f, 1.02f, 1.02f, 0xD8CC40, 0xA8A020, () => new SlimeMob()); d.attackDamage = 2; d.Drop("sulfur_dust", 0, 2);
            // ------------------------------------------------------------ nether
            d = R("ghast", MobCategory.Monster, 10, 0.1f, 4f, 4f, 0xF9F9F9, 0xBCBCBC, () => new GhastMob()); d.flying = true; d.noGravity = true; d.fireImmune = true; d.followRange = 100; d.Drop("gunpowder", 0, 2).Drop("ghast_tear", 0, 1); d.flySpeed = 0.05f;
            d = R("blaze", MobCategory.Monster, 20, 0.23f, 0.6f, 1.8f, 0xF6B201, 0xFFF87E, () => new BlazeMob()); d.fireImmune = true; d.attackDamage = 6; d.followRange = 48; d.xp = 10; d.Drop("blaze_rod", 0, 1);
            d = R("piglin", MobCategory.Monster, 16, 0.35f, 0.6f, 1.95f, 0x995F40, 0xF9F3A4, () => new PiglinMob()); d.neutral = true; d.hostile = true; d.attackDamage = 5;
            d = R("piglin_brute", MobCategory.Monster, 50, 0.35f, 0.6f, 1.95f, 0x592A10, 0xF9F3A4, () => new PiglinMob()); d.attackDamage = 7; d.xp = 20;
            d = R("zombified_piglin", MobCategory.Monster, 20, 0.23f, 0.6f, 1.95f, 0xEA9393, 0x4C7129, () => new ZombifiedPiglinMob()); d.neutral = true; d.hostile = false; d.fireImmune = true; d.undead = true; d.attackDamage = 5; d.armor = 2; d.Drop("rotten_flesh", 0, 1).Drop("gold_nugget", 0, 1).RareDrop("gold_ingot", 0.025f);
            d = R("hoglin", MobCategory.Monster, 40, 0.3f, 1.3965f, 1.4f, 0xC66E55, 0x5F6464, () => new HostileMeleeMob()); d.attackDamage = 6; d.knockbackResistance = 0.6f; d.canBreed = true; d.foods = new[] { "crimson_fungus" }; d.Drop("porkchop", 2, 4, 1, "cooked_porkchop").Drop("leather", 0, 1);
            d = R("zoglin", MobCategory.Monster, 40, 0.3f, 1.3965f, 1.4f, 0xC66E55, 0xE6E6E6, () => new HostileMeleeMob()); d.attackDamage = 6; d.knockbackResistance = 0.6f; d.undead = true; d.Drop("rotten_flesh", 1, 3);
            d = R("magma_cube", MobCategory.Monster, 16, 0.2f, 2.04f, 2.04f, 0x340000, 0xFCFC00, () => new SlimeMob()); d.fireImmune = true; d.attackDamage = 6; d.armor = 12; d.Drop("magma_cream", 0, 1, 0.25f);
            d = R("wither_skeleton", MobCategory.Monster, 20, 0.25f, 0.7f, 2.4f, 0x141414, 0x474D4D, () => new SkeletonMob()); d.undead = true; d.fireImmune = true; d.attackDamage = 8; d.Drop("bone", 0, 2).Drop("coal", 0, 1, 0.33f).RareDrop("wither_skeleton_skull", 0.025f);
            d = R("strider", MobCategory.Creature, 20, 0.175f, 0.9f, 1.7f, 0x9C3436, 0x4D494D, () => new StriderMob()); d.fireImmune = true; d.canBreed = true; d.foods = new[] { "warped_fungus" }; d.Drop("string", 2, 5);
            // ------------------------------------------------------------ end
            d = R("shulker", MobCategory.Monster, 30, 0f, 1f, 1f, 0x946794, 0x4D3852, () => new ShulkerMob()); d.armor = 20; d.knockbackResistance = 1; d.Drop("shulker_shell", 0, 1, 0.5f, null, 0.0625f); d.noGravity = true;
            // ------------------------------------------------------------ bosses
            d = R("ender_dragon", MobCategory.Boss, 200, 0.5f, 16f, 8f, 0x1C1C1C, 0xE079FA, () => new EnderDragonMob()); d.boss = true; d.flying = true; d.noGravity = true; d.fireImmune = true; d.xp = 12000; d.hostile = true; d.hasEgg = false;
            d = R("wither", MobCategory.Boss, 300, 0.6f, 0.9f, 3.5f, 0x141414, 0x4D72A0, () => new WitherBoss()); d.boss = true; d.flying = true; d.noGravity = true; d.fireImmune = true; d.undead = true; d.xp = 50; d.armor = 4; d.hostile = true; d.followRange = 40; d.hasEgg = false;
            foreach (var md in all) if (md.hostile && md.category == MobCategory.Monster && md.xp == 0 && md.id != "creaking") md.xp = 5;
        }

        public static Mob Create(MobDef def, World w)
        {
            var m = def.factory != null ? def.factory() : new AnimalMob();
            m.world = w;
            m.Setup(def);
            return m;
        }

        public static Mob Spawn(World w, string id, Vector3 pos, SpawnReason reason)
        {
            var def = Get(id);
            if (def == null || w == null) return null;
            if (id == "ender_dragon" && w.dim == DimensionId.End && w.session?.dragonFight != null)
            {
                var existing = w.session.dragonFight.dragon;
                if (existing != null && !existing.removed && reason == SpawnReason.SpawnEgg) { }
            }
            var m = Create(def, w);
            m.SetPosition(pos);
            m.yaw = m.prevYaw = m.bodyYaw = m.headYaw = UnityEngine.Random.value * 360f;
            m.spawnReason = reason;
            if (reason == SpawnReason.SpawnEgg || reason == SpawnReason.Spawner || reason == SpawnReason.Structure || reason == SpawnReason.Command || reason == SpawnReason.Summon) m.persistent = reason != SpawnReason.Spawner;
            m.OnInitialSpawn(reason);
            w.AddEntity(m);
            return m;
        }
    }
}
