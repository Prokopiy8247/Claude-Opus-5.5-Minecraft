using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public sealed class PotionType
    {
        public string id, name; public Color32 color;
        public readonly List<EffectInstance> effects = new List<EffectInstance>();
    }

    public static class Potions
    {
        static readonly Dictionary<string, PotionType> byId = new Dictionary<string, PotionType>();
        public static readonly List<PotionType> All = new List<PotionType>();
        static bool inited;

        static PotionType R(string id, string name, uint color, params EffectInstance[] fx)
        {
            var p = new PotionType { id = id, name = name, color = MathX.Hex(color) };
            p.effects.AddRange(fx);
            byId[id] = p; All.Add(p);
            return p;
        }
        static EffectInstance E(Effect e, int sec, int amp = 0) => new EffectInstance(e, sec * 20, amp);

        static void Init()
        {
            if (inited) return; inited = true;
            R("water", "Water", 0x385DC6); R("mundane", "Mundane", 0x385DC6); R("thick", "Thick", 0x385DC6); R("awkward", "Awkward", 0x385DC6);
            R("night_vision", "Night Vision", 0x1F1FA1, E(Effect.NightVision, 180)); R("long_night_vision", "Night Vision", 0x1F1FA1, E(Effect.NightVision, 480));
            R("invisibility", "Invisibility", 0xF6F6F6, E(Effect.Invisibility, 180)); R("long_invisibility", "Invisibility", 0xF6F6F6, E(Effect.Invisibility, 480));
            R("leaping", "Leaping", 0xFDFF84, E(Effect.JumpBoost, 180)); R("long_leaping", "Leaping", 0xFDFF84, E(Effect.JumpBoost, 480)); R("strong_leaping", "Leaping", 0xFDFF84, E(Effect.JumpBoost, 90, 1));
            R("fire_resistance", "Fire Resistance", 0xFF9900, E(Effect.FireResistance, 180)); R("long_fire_resistance", "Fire Resistance", 0xFF9900, E(Effect.FireResistance, 480));
            R("swiftness", "Swiftness", 0x33EBFF, E(Effect.Speed, 180)); R("long_swiftness", "Swiftness", 0x33EBFF, E(Effect.Speed, 480)); R("strong_swiftness", "Swiftness", 0x33EBFF, E(Effect.Speed, 90, 1));
            R("slowness", "Slowness", 0x8BAFE0, E(Effect.Slowness, 90)); R("long_slowness", "Slowness", 0x8BAFE0, E(Effect.Slowness, 240)); R("strong_slowness", "Slowness", 0x8BAFE0, E(Effect.Slowness, 20, 3));
            R("turtle_master", "the Turtle Master", 0x8B8B8B, E(Effect.Slowness, 20, 3), E(Effect.Resistance, 20, 2));
            R("water_breathing", "Water Breathing", 0x98DAC0, E(Effect.WaterBreathing, 180)); R("long_water_breathing", "Water Breathing", 0x98DAC0, E(Effect.WaterBreathing, 480));
            R("healing", "Healing", 0xF82423, new EffectInstance(Effect.InstantHealth, 1)); R("strong_healing", "Healing", 0xF82423, new EffectInstance(Effect.InstantHealth, 1, 1));
            R("harming", "Harming", 0xA9656A, new EffectInstance(Effect.InstantDamage, 1)); R("strong_harming", "Harming", 0xA9656A, new EffectInstance(Effect.InstantDamage, 1, 1));
            R("poison", "Poison", 0x87A363, E(Effect.Poison, 45)); R("long_poison", "Poison", 0x87A363, E(Effect.Poison, 90)); R("strong_poison", "Poison", 0x87A363, E(Effect.Poison, 21, 1));
            R("regeneration", "Regeneration", 0xCD5CAB, E(Effect.Regeneration, 45)); R("long_regeneration", "Regeneration", 0xCD5CAB, E(Effect.Regeneration, 90)); R("strong_regeneration", "Regeneration", 0xCD5CAB, E(Effect.Regeneration, 22, 1));
            R("strength", "Strength", 0xFFC700, E(Effect.Strength, 180)); R("long_strength", "Strength", 0xFFC700, E(Effect.Strength, 480)); R("strong_strength", "Strength", 0xFFC700, E(Effect.Strength, 90, 1));
            R("weakness", "Weakness", 0x484D48, E(Effect.Weakness, 90)); R("long_weakness", "Weakness", 0x484D48, E(Effect.Weakness, 240));
            R("slow_falling", "Slow Falling", 0xF3CFB9, E(Effect.SlowFalling, 90)); R("long_slow_falling", "Slow Falling", 0xF3CFB9, E(Effect.SlowFalling, 240));
            R("luck", "Luck", 0x59C106, E(Effect.Luck, 300));
            R("wind_charged", "Wind Charging", 0xBDC9FF, E(Effect.WindCharged, 180)); R("weaving", "Weaving", 0x78695A, E(Effect.Weaving, 180));
            R("oozing", "Oozing", 0x99FFA3, E(Effect.Oozing, 180)); R("infested", "Infestation", 0x8C9B8C, E(Effect.Infested, 180));
        }

        public static PotionType Get(string id) { Init(); return id != null && byId.TryGetValue(id, out var p) ? p : null; }
        public static IEnumerable<PotionType> List() { Init(); return All; }

        public static ItemStack Make(string item, string potion) { var s = new ItemStack(item, 1); s.Set("potion", potion); return s; }
    }

    public static class Brewing
    {
        static readonly Dictionary<(string, string), string> mixes = new Dictionary<(string, string), string>();
        static bool inited;
        static void M(string from, string ing, string to) => mixes[(from, ing)] = to;

        static void Init()
        {
            if (inited) return; inited = true;
            M("water", "nether_wart", "awkward"); M("water", "glowstone_dust", "thick"); M("water", "redstone", "mundane");
            foreach (var ing in new[] { "sugar", "rabbit_foot", "glistering_melon_slice", "spider_eye", "ghast_tear", "blaze_powder", "magma_cream" }) M("water", ing, "mundane");
            M("water", "fermented_spider_eye", "weakness");
            M("awkward", "golden_carrot", "night_vision"); M("awkward", "rabbit_foot", "leaping"); M("awkward", "magma_cream", "fire_resistance");
            M("awkward", "sugar", "swiftness"); M("awkward", "pufferfish", "water_breathing"); M("awkward", "glistering_melon_slice", "healing");
            M("awkward", "spider_eye", "poison"); M("awkward", "ghast_tear", "regeneration"); M("awkward", "blaze_powder", "strength");
            M("awkward", "turtle_helmet", "turtle_master"); M("awkward", "phantom_membrane", "slow_falling");
            M("awkward", "breeze_rod", "wind_charged"); M("awkward", "cobweb", "weaving"); M("awkward", "slime_block", "oozing"); M("awkward", "stone", "infested");
            M("night_vision", "fermented_spider_eye", "invisibility"); M("long_night_vision", "fermented_spider_eye", "long_invisibility");
            M("leaping", "fermented_spider_eye", "slowness"); M("swiftness", "fermented_spider_eye", "slowness"); M("long_leaping", "fermented_spider_eye", "long_slowness"); M("long_swiftness", "fermented_spider_eye", "long_slowness");
            M("healing", "fermented_spider_eye", "harming"); M("poison", "fermented_spider_eye", "harming"); M("strong_healing", "fermented_spider_eye", "strong_harming"); M("strong_poison", "fermented_spider_eye", "strong_harming");
            foreach (var id in new[] { "night_vision", "invisibility", "leaping", "fire_resistance", "swiftness", "slowness", "water_breathing", "poison", "regeneration", "strength", "weakness", "slow_falling" })
                if (Potions.Get("long_" + id) != null) M(id, "redstone", "long_" + id);
            foreach (var id in new[] { "leaping", "swiftness", "slowness", "healing", "harming", "poison", "regeneration", "strength" })
                if (Potions.Get("strong_" + id) != null) M(id, "glowstone_dust", "strong_" + id);
        }

        static bool IsBottle(ItemStack s) => s != null && (s.item.id == "potion" || s.item.id == "splash_potion" || s.item.id == "lingering_potion");

        public static bool IsIngredient(Item it)
        {
            Init();
            if (it == null) return false;
            if (it.id == "gunpowder" || it.id == "dragon_breath") return true;
            foreach (var k in mixes.Keys) if (k.Item2 == it.id) return true;
            return false;
        }

        static ItemStack Result(ItemStack bottle, ItemStack ing)
        {
            if (!IsBottle(bottle) || ing == null) return null;
            string pot = bottle.Get("potion") ?? "water";
            if (ing.item.id == "gunpowder" && bottle.item.id == "potion") return Potions.Make("splash_potion", pot);
            if (ing.item.id == "dragon_breath" && bottle.item.id == "splash_potion") return Potions.Make("lingering_potion", pot);
            if (mixes.TryGetValue((pot, ing.item.id), out var to)) return Potions.Make(bottle.item.id, to);
            return null;
        }

        public static bool CanBrewAny(ItemStack[] items, ItemStack ing)
        {
            Init();
            for (int i = 0; i < 3; i++) if (Result(items[i], ing) != null) return true;
            return false;
        }

        public static void Brew(ItemStack[] items, ItemStack ing)
        {
            Init();
            for (int i = 0; i < 3; i++) { var r = Result(items[i], ing); if (r != null) items[i] = r; }
        }
    }

    public static class Beacon
    {
        static bool IsBase(Block b) => b.id == "iron_block" || b.id == "gold_block" || b.id == "diamond_block" || b.id == "emerald_block" || b.id == "netherite_block";

        public static int ComputeLevels(World w, Int3 pos)
        {
            int levels = 0;
            for (int l = 1; l <= 4; l++)
            {
                int y = pos.y - l;
                if (y < w.minY) break;
                bool ok = true;
                for (int x = pos.x - l; x <= pos.x + l && ok; x++)
                    for (int z = pos.z - l; z <= pos.z + l && ok; z++)
                        if (!IsBase(w.GetBlock(new Int3(x, y, z)))) ok = false;
                if (!ok) break;
                levels = l;
            }
            return levels;
        }

        public static readonly string[][] PowersByLevel =
        {
            new[] { "speed", "haste" }, new[] { "resistance", "jump_boost" }, new[] { "strength" }, new[] { "regeneration" }
        };
    }

    public static class Dispensing
    {
        /// <summary>Dispense (or drop) one item from the stack; returns the remaining stack.</summary>
        public static ItemStack Dispense(World w, Int3 pos, Dir facing, ItemStack stack, bool dropper)
        {
            if (stack == null) return null;
            Vector3 n = DirUtil.Normal[(int)facing];
            Vector3 front = pos.Center + n * 0.7f;
            Int3 fp = pos.Offset(facing);
            string id = stack.item.id;
            if (dropper)
            {
                var target = HopperBlock.ContainerAt(w, fp);
                if (target != null)
                {
                    var one = stack.CopyWithCount(1);
                    if (HopperBlock.Insert(target, one, DirUtil.Opposite(facing))) { stack.count--; Sounds.Play("block.dispenser.dispense", pos.Center, 1f, 1f); }
                    return stack;
                }
                DropItem(w, front, n, stack.Split(1));
                Sounds.Play("block.dispenser.dispense", pos.Center, 1f, 1f);
                return stack;
            }
            bool used = true;
            switch (id)
            {
                case "arrow": case "spectral_arrow": case "tipped_arrow":
                    {
                        var a = new Arrow { world = w };
                        a.SetPosition(front - Vector3.up * 0.1f);
                        a.Launch(n + new Vector3(0, 0.1f, 0), 1.1f, 6f);
                        a.pickup = Arrow.Pickup.Allowed; a.ammoId = id;
                        if (id == "tipped_arrow") a.potion = stack.Get("potion");
                        w.AddEntity(a);
                        Sounds.Play("entity.arrow.shoot", pos.Center, 1f, 1.2f);
                        stack.count--; break;
                    }
                case "snowball": case "egg": case "ender_pearl": case "wind_charge": case "experience_bottle":
                    {
                        var pr = Projectile.Create(id, w);
                        if (pr != null) { pr.SetPosition(front); pr.Launch(n + new Vector3(0, 0.1f, 0), 1.1f, 6f); w.AddEntity(pr); }
                        stack.count--; break;
                    }
                case "splash_potion": case "lingering_potion":
                    {
                        var tp = new ThrownPotion { world = w, potionStack = stack.CopyWithCount(1), lingering = id == "lingering_potion" };
                        tp.SetPosition(front); tp.Launch(n + new Vector3(0, 0.1f, 0), 0.6f, 3f); w.AddEntity(tp);
                        stack.count--; break;
                    }
                case "fire_charge":
                    {
                        var fb = new SmallFireball { world = w };
                        fb.SetPosition(front); fb.velocity = n * 0.4f + Random.insideUnitSphere * 0.02f; w.AddEntity(fb);
                        Sounds.Play("entity.blaze.shoot", pos.Center, 1f, 1f);
                        stack.count--; break;
                    }
                case "firework_rocket":
                    Firework.Launch(w, null, front, n * 0.5f, true, stack.GetInt("flight", 1)); stack.count--; break;
                case "tnt":
                    if (w.IsAir(fp)) { PrimedTnt.Spawn(w, fp.Center - new Vector3(0, 0.5f, 0), 80, null); stack.count--; } else used = false; break;
                case "water_bucket": case "lava_bucket": case "powder_snow_bucket":
                    {
                        var b = w.GetBlock(fp);
                        if (b.isAir || b.replaceable || b.isLiquid)
                        {
                            Block place = id == "water_bucket" ? Blocks.Water : id == "lava_bucket" ? Blocks.Lava : Blocks.Get("powder_snow");
                            w.SetState(fp, place.DefaultState);
                            return new ItemStack("bucket", 1);
                        }
                        used = false; break;
                    }
                case "bucket":
                    {
                        ushort s = w.GetState(fp); var b = Blocks.ByState[s];
                        if (b is FluidBlock fl && s == b.baseState)
                        {
                            w.SetState(fp, 0);
                            var filled = new ItemStack(fl.fluidKind == 0 ? "water_bucket" : "lava_bucket", 1);
                            stack.count--;
                            if (stack.count <= 0) return filled;
                            if (w.GetBlockEntity(pos) is DispenserEntity de && de.AddItem(filled) != null) DropItem(w, front, n, filled);
                            return stack;
                        }
                        used = false; break;
                    }
                case "bone_meal":
                    if (BoneMealItem.Apply(w, fp, DirUtil.Opposite(facing))) stack.count--; else used = false; break;
                case "flint_and_steel":
                    {
                        var b = w.GetBlock(fp);
                        if (b is TntBlock tnt) { tnt.Ignite(w, fp, null); stack.HurtAndBreak(1, null); }
                        else if (b.isAir) { if (!Portals.TryLightPortal(w, fp)) w.SetState(fp, Blocks.Fire.DefaultState); stack.HurtAndBreak(1, null); }
                        else used = false;
                        break;
                    }
                case "shears":
                    {
                        bool sheared = false;
                        foreach (var e in w.GetEntities(new AABB(fp.x, fp.y, fp.z, fp.x + 1, fp.y + 1, fp.z + 1)))
                            if (e is SheepMob sh && !sh.sheared && !sh.IsBaby) { sh.Shear(); sheared = true; break; }
                        if (sheared) stack.HurtAndBreak(1, null); else used = false;
                        break;
                    }
                case "minecart": case "chest_minecart": case "hopper_minecart": case "tnt_minecart": case "furnace_minecart":
                    if (w.GetBlock(fp) is RailBlock) { Minecart.Spawn(w, fp.ToVector3() + new Vector3(0.5f, 0.0625f, 0.5f), (stack.item as MinecartItem)?.content); stack.count--; } else used = false; break;
                case "armor_stand": used = false; break;
                case "shulker_box":
                default:
                    if (stack.item is ArmorItem ai)
                    {
                        foreach (var e in w.GetEntities(new AABB(fp.x, fp.y, fp.z, fp.x + 1, fp.y + 1, fp.z + 1)))
                            if (e is Player p && p.inventory.armor[(int)ai.slot] == null) { p.inventory.armor[(int)ai.slot] = stack.Split(1); goto done; }
                    }
                    if (stack.item is SpawnEggItem egg) { MobRegistry.Spawn(w, egg.mobId, fp.ToVector3() + new Vector3(0.5f, 0, 0.5f), SpawnReason.Dispenser); stack.count--; break; }
                    if (stack.item.block != null && (stack.item.block is ShulkerBoxBlock || stack.item.id.EndsWith("_shulker_box") || stack.item.id == "carved_pumpkin" || stack.item.id.EndsWith("_skull") || stack.item.id.EndsWith("_head")) && w.GetBlock(fp).replaceable)
                    {
                        w.SetState(fp, stack.item.block.DefaultState);
                        stack.item.block.OnPlaced(w, fp, 0, null, stack);
                        stack.count--; break;
                    }
                    DropItem(w, front, n, stack.Split(1));
                    break;
            }
        done:
            if (used) { Sounds.Play("block.dispenser.dispense", pos.Center, 1f, 1f); for (int i = 0; i < 4; i++) Particles.Smoke(w, front, 1, 0.1f); }
            else Sounds.Play("block.dispenser.fail", pos.Center, 1f, 1.2f);
            return stack;
        }

        static void DropItem(World w, Vector3 pos, Vector3 dir, ItemStack s)
        {
            float v = Random.value * 0.1f + 0.2f;
            w.SpawnItem(pos - Vector3.up * 0.15f, s, dir * v + new Vector3(Random.Range(-0.03f, 0.03f), 0.2f, Random.Range(-0.03f, 0.03f)));
        }
    }

    /// <summary>Rail shape connection logic.</summary>
    public static class Rails
    {
        // shape: 0 NS,1 EW, 2 asc E, 3 asc W, 4 asc N, 5 asc S, 6 SE,7 SW,8 NW,9 NE   (N=+Z, E=+X)
        static bool IsRail(World w, Int3 p) => w.GetBlock(p) is RailBlock;

        static bool HasRail(World w, Int3 p, Dir d, out int dy)
        {
            var n = p.Offset(d);
            dy = 0;
            if (IsRail(w, n)) return true;
            if (IsRail(w, n.Offset(Dir.Up))) { dy = 1; return true; }
            if (IsRail(w, n.Offset(Dir.Down))) { dy = -1; return true; }
            return false;
        }

        public static void UpdateShape(World w, Int3 pos, bool placing)
        {
            ushort s = w.GetState(pos);
            if (!(Blocks.ByState[s] is RailBlock rb)) return;
            int meta = s - rb.baseState;
            bool n = HasRail(w, pos, Dir.North, out int dn), so = HasRail(w, pos, Dir.South, out int ds);
            bool e = HasRail(w, pos, Dir.East, out int de), we = HasRail(w, pos, Dir.West, out int dw);
            int shape = RailBlock.Shape(meta);
            bool canCurve = rb.kind == RailKind.Normal;
            if (n || so) shape = 0;
            if ((e || we) && !n && !so) shape = 1;
            if (canCurve)
            {
                if (so && e && !n && !we) shape = 6;
                else if (so && we && !n && !e) shape = 7;
                else if (n && we && !so && !e) shape = 8;
                else if (n && e && !so && !we) shape = 9;
            }
            if (shape == 0) { if (dn == 1) shape = 4; else if (ds == 1) shape = 5; }
            if (shape == 1) { if (de == 1) shape = 2; else if (dw == 1) shape = 3; }
            int nm = (meta & 16) | shape;
            if (nm != meta) w.SetState(pos, rb.State(nm), SetFlags.Hooks);
        }

        public static bool PoweredByNeighbourRail(World w, Int3 pos, int meta, RailBlock self, int depth)
        {
            if (depth > 8) return false;
            int shape = RailBlock.Shape(meta);
            Dir a, b;
            if (shape == 1 || shape == 2 || shape == 3) { a = Dir.East; b = Dir.West; } else { a = Dir.North; b = Dir.South; }
            foreach (var d in new[] { a, b })
            {
                Int3 p = pos;
                for (int i = 1; i <= 8; i++)
                {
                    p = p.Offset(d);
                    ushort s = w.GetState(p);
                    var bl = Blocks.ByState[s];
                    if (!(bl is RailBlock r2) || r2.kind != self.kind)
                    {
                        s = w.GetState(p.Offset(Dir.Up)); bl = Blocks.ByState[s];
                        if (bl is RailBlock r3 && r3.kind == self.kind) p = p.Offset(Dir.Up);
                        else { s = w.GetState(p.Offset(Dir.Down)); bl = Blocks.ByState[s]; if (bl is RailBlock r4 && r4.kind == self.kind) p = p.Offset(Dir.Down); else break; }
                    }
                    if (Redstone.IsPowered(w, p)) return true;
                }
            }
            return false;
        }

        /// <summary>Direction vectors of the rail's two ends (x,z) and the y slope.</summary>
        public static void Ends(int shape, out Vector3 e0, out Vector3 e1)
        {
            switch (shape)
            {
                case 1: e0 = new Vector3(-1, 0, 0); e1 = new Vector3(1, 0, 0); break;
                case 2: e0 = new Vector3(-1, 0, 0); e1 = new Vector3(1, 1, 0); break;
                case 3: e0 = new Vector3(-1, 1, 0); e1 = new Vector3(1, 0, 0); break;
                case 4: e0 = new Vector3(0, 0, -1); e1 = new Vector3(0, 1, 1); break;
                case 5: e0 = new Vector3(0, 1, -1); e1 = new Vector3(0, 0, 1); break;
                case 6: e0 = new Vector3(0, 0, -1); e1 = new Vector3(1, 0, 0); break;   // S + E
                case 7: e0 = new Vector3(0, 0, -1); e1 = new Vector3(-1, 0, 0); break;  // S + W
                case 8: e0 = new Vector3(0, 0, 1); e1 = new Vector3(-1, 0, 0); break;   // N + W
                case 9: e0 = new Vector3(0, 0, 1); e1 = new Vector3(1, 0, 0); break;    // N + E
                default: e0 = new Vector3(0, 0, -1); e1 = new Vector3(0, 0, 1); break;
            }
        }
    }
}
