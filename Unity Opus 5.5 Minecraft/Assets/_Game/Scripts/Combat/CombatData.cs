using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public sealed class DamageSource
    {
        public string id;
        public Entity attacker;      // entity responsible (shooter for projectiles)
        public Entity direct;        // direct entity (the projectile)
        public bool bypassArmor, isFire, isExplosion, isProjectile, isMagic, bypassInvul, isFall, scalesWithDifficulty;
        public Vector3? sourcePos;

        public DamageSource(string id) { this.id = id; }
        public DamageSource Clone() => (DamageSource)MemberwiseClone();

        public static readonly DamageSource Generic = new DamageSource("generic");
        public static readonly DamageSource Fall = new DamageSource("fall") { bypassArmor = true, isFall = true };
        public static readonly DamageSource Lava = new DamageSource("lava") { isFire = true };
        public static readonly DamageSource InFire = new DamageSource("inFire") { isFire = true };
        public static readonly DamageSource OnFire = new DamageSource("onFire") { isFire = true, bypassArmor = true };
        public static readonly DamageSource HotFloor = new DamageSource("hotFloor") { isFire = true };
        public static readonly DamageSource Drown = new DamageSource("drown") { bypassArmor = true };
        public static readonly DamageSource Starve = new DamageSource("starve") { bypassArmor = true };
        public static readonly DamageSource Cactus = new DamageSource("cactus");
        public static readonly DamageSource BerryBush = new DamageSource("sweetBerryBush");
        public static readonly DamageSource Void = new DamageSource("outOfWorld") { bypassArmor = true, bypassInvul = true };
        public static readonly DamageSource Suffocate = new DamageSource("inWall") { bypassArmor = true };
        public static readonly DamageSource Magic = new DamageSource("magic") { bypassArmor = true, isMagic = true };
        public static readonly DamageSource WitherEffect = new DamageSource("wither") { bypassArmor = true, isMagic = true };
        public static readonly DamageSource Lightning = new DamageSource("lightningBolt") { isFire = true };
        public static readonly DamageSource Freeze = new DamageSource("freeze") { bypassArmor = true };
        public static readonly DamageSource Kill = new DamageSource("genericKill") { bypassArmor = true, bypassInvul = true };
        public static readonly DamageSource FlyIntoWall = new DamageSource("flyIntoWall") { bypassArmor = true };
        public static readonly DamageSource DragonBreath = new DamageSource("dragonBreath") { bypassArmor = true, isMagic = true };
        public static readonly DamageSource Sonic = new DamageSource("sonic_boom") { bypassArmor = true, isMagic = true };

        public static DamageSource MobAttack(Entity mob) => new DamageSource("mob") { attacker = mob, direct = mob, scalesWithDifficulty = true };
        public static DamageSource PlayerAttack(Entity p) => new DamageSource("player") { attacker = p, direct = p };
        public static DamageSource ProjectileHit(Entity proj, Entity owner, string id = "arrow") => new DamageSource(id) { attacker = owner, direct = proj, isProjectile = true, scalesWithDifficulty = owner is Mob };
        public static DamageSource Explosion(Entity source, Vector3 pos) => new DamageSource("explosion") { attacker = source, isExplosion = true, sourcePos = pos, scalesWithDifficulty = true };
        public static DamageSource FireballHit(Entity ball, Entity owner) => new DamageSource("fireball") { attacker = owner, direct = ball, isFire = true, isProjectile = true };

        public string DeathMessage(string victim)
        {
            string killer = attacker != null ? attacker.DisplayName : null;
            switch (id)
            {
                case "fall": return victim + " hit the ground too hard";
                case "lava": return victim + " tried to swim in lava";
                case "inFire": return victim + " went up in flames";
                case "onFire": return victim + " burned to death";
                case "hotFloor": return victim + " discovered the floor was lava";
                case "drown": return victim + " drowned";
                case "starve": return victim + " starved to death";
                case "cactus": return victim + " was pricked to death";
                case "sweetBerryBush": return victim + " was poked to death by a sweet berry bush";
                case "outOfWorld": return victim + " fell out of the world";
                case "inWall": return victim + " suffocated in a wall";
                case "magic": return victim + " was killed by magic";
                case "wither": return victim + " withered away";
                case "explosion": return killer != null ? victim + " was blown up by " + killer : victim + " blew up";
                case "arrow": return victim + " was shot by " + (killer ?? "an arrow");
                case "fireball": return victim + " was fireballed by " + (killer ?? "a fireball");
                case "freeze": return victim + " froze to death";
                case "lightningBolt": return victim + " was struck by lightning";
                case "dragonBreath": return victim + " was roasted in dragon's breath";
                case "sonic_boom": return victim + " was obliterated by a sonically-charged shriek";
                case "flyIntoWall": return victim + " experienced kinetic energy";
                case "genericKill": return victim + " was killed";
            }
            if (killer != null) return victim + " was slain by " + killer;
            return victim + " died";
        }
    }

    // ============================================================================ Status effects
    public sealed class Effect
    {
        public string id, name; public bool beneficial; public Color32 color; public int index;
        static readonly List<Effect> all = new List<Effect>();
        static readonly Dictionary<string, Effect> byId = new Dictionary<string, Effect>();
        public static IReadOnlyList<Effect> All => all;
        static Effect R(string id, string name, bool good, uint rgb)
        {
            var e = new Effect { id = id, name = name, beneficial = good, color = MathX.Hex(rgb), index = all.Count };
            all.Add(e); byId[id] = e; return e;
        }
        public static Effect Get(string id) => id != null && byId.TryGetValue(id, out var e) ? e : null;

        public static readonly Effect Speed = R("speed", "Speed", true, 0x33EBFF);
        public static readonly Effect Slowness = R("slowness", "Slowness", false, 0x8BAFE0);
        public static readonly Effect Haste = R("haste", "Haste", true, 0xD9C043);
        public static readonly Effect MiningFatigue = R("mining_fatigue", "Mining Fatigue", false, 0x4A4217);
        public static readonly Effect Strength = R("strength", "Strength", true, 0xFFC700);
        public static readonly Effect InstantHealth = R("instant_health", "Instant Health", true, 0xF82423);
        public static readonly Effect InstantDamage = R("instant_damage", "Instant Damage", false, 0xA9656A);
        public static readonly Effect JumpBoost = R("jump_boost", "Jump Boost", true, 0xFDFF84);
        public static readonly Effect Nausea = R("nausea", "Nausea", false, 0x551D4A);
        public static readonly Effect Regeneration = R("regeneration", "Regeneration", true, 0xCD5CAB);
        public static readonly Effect Resistance = R("resistance", "Resistance", true, 0x9146F0);
        public static readonly Effect FireResistance = R("fire_resistance", "Fire Resistance", true, 0xFF9900);
        public static readonly Effect WaterBreathing = R("water_breathing", "Water Breathing", true, 0x98DAC0);
        public static readonly Effect Invisibility = R("invisibility", "Invisibility", true, 0xF6F6F6);
        public static readonly Effect Blindness = R("blindness", "Blindness", false, 0x1F1F23);
        public static readonly Effect NightVision = R("night_vision", "Night Vision", true, 0xC2FF66);
        public static readonly Effect Hunger = R("hunger", "Hunger", false, 0x587653);
        public static readonly Effect Weakness = R("weakness", "Weakness", false, 0x484D48);
        public static readonly Effect Poison = R("poison", "Poison", false, 0x87A363);
        public static readonly Effect Wither = R("wither", "Wither", false, 0x736156);
        public static readonly Effect HealthBoost = R("health_boost", "Health Boost", true, 0xF87D23);
        public static readonly Effect Absorption = R("absorption", "Absorption", true, 0x2552A5);
        public static readonly Effect Saturation = R("saturation", "Saturation", true, 0xF82423);
        public static readonly Effect Glowing = R("glowing", "Glowing", false, 0x94A061);
        public static readonly Effect Levitation = R("levitation", "Levitation", false, 0xCEFFFF);
        public static readonly Effect Luck = R("luck", "Luck", true, 0x59C106);
        public static readonly Effect SlowFalling = R("slow_falling", "Slow Falling", true, 0xF3CFB9);
        public static readonly Effect ConduitPower = R("conduit_power", "Conduit Power", true, 0x1DC2D1);
        public static readonly Effect DolphinsGrace = R("dolphins_grace", "Dolphin's Grace", true, 0x88A3BE);
        public static readonly Effect BadOmen = R("bad_omen", "Bad Omen", false, 0x0B6138);
        public static readonly Effect HeroOfTheVillage = R("hero_of_the_village", "Hero of the Village", true, 0x44FF44);
        public static readonly Effect Darkness = R("darkness", "Darkness", false, 0x292721);
        public static readonly Effect Oozing = R("oozing", "Oozing", false, 0x99FFA3);
        public static readonly Effect Weaving = R("weaving", "Weaving", false, 0x78695A);
        public static readonly Effect WindCharged = R("wind_charged", "Wind Charged", false, 0xBDC9FF);
        public static readonly Effect Infested = R("infested", "Infested", false, 0x8C9B8C);
    }

    public sealed class EffectInstance
    {
        public Effect effect; public int duration; public int amplifier; public bool ambient; public bool showParticles = true;
        public EffectInstance(Effect e, int duration, int amp = 0, bool ambient = false) { effect = e; this.duration = duration; amplifier = amp; this.ambient = ambient; }
        public EffectInstance Copy() => new EffectInstance(effect, duration, amplifier, ambient) { showParticles = showParticles };
        public static string FormatTime(int ticks)
        {
            if (ticks >= 20 * 60 * 60 * 24) return "∞";
            int s = ticks / 20;
            return (s / 60) + ":" + (s % 60).ToString("00");
        }
        public string Serialize() => effect.id + ":" + duration + ":" + amplifier;
        public static EffectInstance Parse(string s)
        {
            var p = s.Split(':');
            var e = Effect.Get(p[0]);
            if (e == null || p.Length < 3) return null;
            return new EffectInstance(e, int.Parse(p[1]), int.Parse(p[2]));
        }
    }

    // ============================================================================ Enchantments
    public enum EnchantTarget { Armor, ArmorHead, ArmorChest, ArmorLegs, ArmorFeet, Weapon, Digger, Bow, Crossbow, Trident, FishingRod, Breakable, Spear, Mace, Wearable }

    public sealed class Enchant
    {
        public string id, name; public int maxLevel; public EnchantTarget target; public int weight; public bool treasure, curse;
        public int minCostBase, minCostPerLevel, maxCostOffset;
        public string[] incompatible = new string[0];
        static readonly List<Enchant> all = new List<Enchant>();
        static readonly Dictionary<string, Enchant> byId = new Dictionary<string, Enchant>();
        public static IReadOnlyList<Enchant> All => all;
        public static Enchant Get(string id) => id != null && byId.TryGetValue(id, out var e) ? e : null;
        static Enchant R(string id, string name, int max, EnchantTarget t, int weight, int minBase, int minPer, int maxOff, bool treasure = false, bool curse = false, params string[] incompat)
        {
            var e = new Enchant { id = id, name = name, maxLevel = max, target = t, weight = weight, minCostBase = minBase, minCostPerLevel = minPer, maxCostOffset = maxOff, treasure = treasure, curse = curse, incompatible = incompat };
            all.Add(e); byId[id] = e; return e;
        }
        public int MinCost(int lvl) => minCostBase + (lvl - 1) * minCostPerLevel;
        public int MaxCost(int lvl) => MinCost(lvl) + maxCostOffset;

        public static readonly Enchant Protection = R("protection", "Protection", 4, EnchantTarget.Armor, 10, 1, 11, 11, false, false, "fire_protection", "blast_protection", "projectile_protection");
        public static readonly Enchant FireProtection = R("fire_protection", "Fire Protection", 4, EnchantTarget.Armor, 5, 10, 8, 8, false, false, "protection", "blast_protection", "projectile_protection");
        public static readonly Enchant FeatherFalling = R("feather_falling", "Feather Falling", 4, EnchantTarget.ArmorFeet, 5, 5, 6, 6);
        public static readonly Enchant BlastProtection = R("blast_protection", "Blast Protection", 4, EnchantTarget.Armor, 2, 5, 8, 8, false, false, "protection", "fire_protection", "projectile_protection");
        public static readonly Enchant ProjectileProtection = R("projectile_protection", "Projectile Protection", 4, EnchantTarget.Armor, 5, 3, 6, 6, false, false, "protection", "fire_protection", "blast_protection");
        public static readonly Enchant Respiration = R("respiration", "Respiration", 3, EnchantTarget.ArmorHead, 2, 10, 10, 30);
        public static readonly Enchant AquaAffinity = R("aqua_affinity", "Aqua Affinity", 1, EnchantTarget.ArmorHead, 2, 1, 0, 40);
        public static readonly Enchant Thorns = R("thorns", "Thorns", 3, EnchantTarget.ArmorChest, 1, 10, 20, 50);
        public static readonly Enchant DepthStrider = R("depth_strider", "Depth Strider", 3, EnchantTarget.ArmorFeet, 2, 10, 10, 15, false, false, "frost_walker");
        public static readonly Enchant FrostWalker = R("frost_walker", "Frost Walker", 2, EnchantTarget.ArmorFeet, 2, 10, 10, 15, true, false, "depth_strider");
        public static readonly Enchant SoulSpeed = R("soul_speed", "Soul Speed", 3, EnchantTarget.ArmorFeet, 1, 10, 10, 15, true);
        public static readonly Enchant SwiftSneak = R("swift_sneak", "Swift Sneak", 3, EnchantTarget.ArmorLegs, 1, 25, 25, 50, true);
        public static readonly Enchant Sharpness = R("sharpness", "Sharpness", 5, EnchantTarget.Weapon, 10, 1, 11, 20, false, false, "smite", "bane_of_arthropods");
        public static readonly Enchant Smite = R("smite", "Smite", 5, EnchantTarget.Weapon, 5, 5, 8, 20, false, false, "sharpness", "bane_of_arthropods");
        public static readonly Enchant BaneOfArthropods = R("bane_of_arthropods", "Bane of Arthropods", 5, EnchantTarget.Weapon, 5, 5, 8, 20, false, false, "sharpness", "smite");
        public static readonly Enchant Knockback = R("knockback", "Knockback", 2, EnchantTarget.Weapon, 5, 5, 20, 50);
        public static readonly Enchant FireAspect = R("fire_aspect", "Fire Aspect", 2, EnchantTarget.Weapon, 2, 10, 20, 50);
        public static readonly Enchant Looting = R("looting", "Looting", 3, EnchantTarget.Weapon, 2, 15, 9, 50);
        public static readonly Enchant SweepingEdge = R("sweeping_edge", "Sweeping Edge", 3, EnchantTarget.Weapon, 2, 5, 9, 15);
        public static readonly Enchant Efficiency = R("efficiency", "Efficiency", 5, EnchantTarget.Digger, 10, 1, 10, 50);
        public static readonly Enchant SilkTouch = R("silk_touch", "Silk Touch", 1, EnchantTarget.Digger, 1, 15, 0, 50, false, false, "fortune");
        public static readonly Enchant Unbreaking = R("unbreaking", "Unbreaking", 3, EnchantTarget.Breakable, 5, 5, 8, 50);
        public static readonly Enchant Fortune = R("fortune", "Fortune", 3, EnchantTarget.Digger, 2, 15, 9, 50, false, false, "silk_touch");
        public static readonly Enchant Power = R("power", "Power", 5, EnchantTarget.Bow, 10, 1, 10, 15);
        public static readonly Enchant Punch = R("punch", "Punch", 2, EnchantTarget.Bow, 2, 12, 20, 25);
        public static readonly Enchant Flame = R("flame", "Flame", 1, EnchantTarget.Bow, 2, 20, 0, 30);
        public static readonly Enchant Infinity = R("infinity", "Infinity", 1, EnchantTarget.Bow, 1, 20, 0, 30, false, false, "mending");
        public static readonly Enchant LuckOfTheSea = R("luck_of_the_sea", "Luck of the Sea", 3, EnchantTarget.FishingRod, 2, 15, 9, 50);
        public static readonly Enchant Lure = R("lure", "Lure", 3, EnchantTarget.FishingRod, 2, 15, 9, 50);
        public static readonly Enchant Loyalty = R("loyalty", "Loyalty", 3, EnchantTarget.Trident, 5, 12, 7, 38, false, false, "riptide");
        public static readonly Enchant Impaling = R("impaling", "Impaling", 5, EnchantTarget.Trident, 2, 1, 8, 20);
        public static readonly Enchant Riptide = R("riptide", "Riptide", 3, EnchantTarget.Trident, 2, 17, 7, 38, false, false, "loyalty", "channeling");
        public static readonly Enchant Channeling = R("channeling", "Channeling", 1, EnchantTarget.Trident, 1, 25, 0, 25, false, false, "riptide");
        public static readonly Enchant Multishot = R("multishot", "Multishot", 1, EnchantTarget.Crossbow, 2, 20, 0, 30, false, false, "piercing");
        public static readonly Enchant QuickCharge = R("quick_charge", "Quick Charge", 3, EnchantTarget.Crossbow, 5, 12, 20, 38);
        public static readonly Enchant Piercing = R("piercing", "Piercing", 4, EnchantTarget.Crossbow, 10, 1, 10, 49, false, false, "multishot");
        public static readonly Enchant Mending = R("mending", "Mending", 1, EnchantTarget.Breakable, 2, 25, 0, 50, true, false, "infinity");
        public static readonly Enchant Lunge = R("lunge", "Lunge", 3, EnchantTarget.Spear, 5, 5, 8, 20);
        public static readonly Enchant Density = R("density", "Density", 5, EnchantTarget.Mace, 5, 5, 8, 20, false, false, "breach");
        public static readonly Enchant Breach = R("breach", "Breach", 4, EnchantTarget.Mace, 2, 15, 9, 50, false, false, "density");
        public static readonly Enchant WindBurst = R("wind_burst", "Wind Burst", 3, EnchantTarget.Mace, 2, 15, 9, 50, true);
        public static readonly Enchant BindingCurse = R("binding_curse", "Curse of Binding", 1, EnchantTarget.Wearable, 1, 25, 0, 25, true, true);
        public static readonly Enchant VanishingCurse = R("vanishing_curse", "Curse of Vanishing", 1, EnchantTarget.Breakable, 1, 25, 0, 25, true, true);

        public static string Roman(int n)
        {
            switch (n) { case 1: return "I"; case 2: return "II"; case 3: return "III"; case 4: return "IV"; case 5: return "V"; case 6: return "VI"; case 7: return "VII"; case 8: return "VIII"; case 9: return "IX"; case 10: return "X"; }
            return n.ToString();
        }
        public string LevelName(int lvl) => maxLevel == 1 && lvl == 1 ? name : name + " " + Roman(lvl);

        public bool CanApplyTo(Item it)
        {
            if (it == null) return false;
            if (it.id == "book" || it.id == "enchanted_book") return true;
            switch (target)
            {
                case EnchantTarget.Armor: return it is ArmorItem;
                case EnchantTarget.ArmorHead: return it is ArmorItem a1 && a1.slot == ArmorSlot.Head;
                case EnchantTarget.ArmorChest: return it is ArmorItem a2 && a2.slot == ArmorSlot.Chest;
                case EnchantTarget.ArmorLegs: return it is ArmorItem a3 && a3.slot == ArmorSlot.Legs;
                case EnchantTarget.ArmorFeet: return it is ArmorItem a4 && a4.slot == ArmorSlot.Feet;
                case EnchantTarget.Weapon: return it.toolType == ToolType.Sword || it.toolType == ToolType.Axe;
                case EnchantTarget.Digger: return it.toolType == ToolType.Pickaxe || it.toolType == ToolType.Axe || it.toolType == ToolType.Shovel || it.toolType == ToolType.Hoe;
                case EnchantTarget.Bow: return it.id == "bow";
                case EnchantTarget.Crossbow: return it.id == "crossbow";
                case EnchantTarget.Trident: return it.id == "trident";
                case EnchantTarget.FishingRod: return it.id == "fishing_rod";
                case EnchantTarget.Spear: return it.toolType == ToolType.Spear;
                case EnchantTarget.Mace: return it.id == "mace";
                case EnchantTarget.Wearable: return it is ArmorItem || it.id == "elytra" || it.id == "carved_pumpkin";
                case EnchantTarget.Breakable: return it.IsDamageable;
            }
            return false;
        }
        public bool CompatibleWith(Enchant o) => o != this && System.Array.IndexOf(incompatible, o.id) < 0 && System.Array.IndexOf(o.incompatible, id) < 0;

        /// <summary>Randomly select enchantments like vanilla's enchanting table algorithm.</summary>
        public static List<(Enchant e, int lvl)> Select(ref RNG rng, Item item, int level, bool allowTreasure)
        {
            var result = new List<(Enchant, int)>();
            int ench = Mathf.Max(1, item.enchantability);
            if (item.id == "book") ench = 1;
            level += 1 + rng.Next(ench / 4 + 1) + rng.Next(ench / 4 + 1);
            float bonus = (rng.NextFloat() + rng.NextFloat() - 1f) * 0.15f;
            level = Mathf.Clamp(Mathf.RoundToInt(level + level * bonus), 1, int.MaxValue);
            var pool = new List<(Enchant, int)>();
            foreach (var e in all)
            {
                if (e.treasure && !allowTreasure) continue;
                if (e.curse) continue;
                if (!e.CanApplyTo(item)) continue;
                for (int l = e.maxLevel; l >= 1; l--)
                    if (level >= e.MinCost(l) && level <= e.MaxCost(l)) { pool.Add((e, l)); break; }
            }
            if (pool.Count == 0) return result;
            var first = WeightedPick(ref rng, pool);
            result.Add(first);
            while (rng.Next(50) <= level)
            {
                pool.RemoveAll(p => !p.Item1.CompatibleWith(result[result.Count - 1].Item1) || p.Item1 == result[result.Count - 1].Item1);
                if (pool.Count == 0) break;
                result.Add(WeightedPick(ref rng, pool));
                level /= 2;
            }
            return result;
        }
        static (Enchant, int) WeightedPick(ref RNG rng, List<(Enchant, int)> pool)
        {
            int total = 0; foreach (var p in pool) total += p.Item1.weight;
            int r = rng.Next(total);
            foreach (var p in pool) { r -= p.Item1.weight; if (r < 0) return p; }
            return pool[0];
        }
    }
}
