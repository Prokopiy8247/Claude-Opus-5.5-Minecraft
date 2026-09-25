using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public abstract class BlockEntity
    {
        public World world;
        public Int3 pos;
        public bool ticks;
        public GameObject visual; // optional renderer object (chests, beds...)
        public virtual void Tick() { }
        public virtual void OnRemoved() { if (visual != null) { UnityEngine.Object.Destroy(visual); visual = null; } }
        /// <summary>Chunk unloaded: release render resources only (no gameplay side effects).</summary>
        public virtual void OnUnload() { if (visual != null) { UnityEngine.Object.Destroy(visual); visual = null; } }
        public virtual void DropContents() { }
        public virtual void Save(Dictionary<string, string> d) { }
        public virtual void Load(Dictionary<string, string> d) { }
        public string TypeId => GetType().Name;
        public virtual bool HasVisual => false;
        public virtual void UpdateVisual(float partial) { }

        static readonly Dictionary<string, Func<BlockEntity>> factory = new Dictionary<string, Func<BlockEntity>>();
        public static void RegisterType<T>() where T : BlockEntity, new() => factory[typeof(T).Name] = () => new T();
        public static BlockEntity Create(string type)
        {
            if (factory.Count == 0) RegisterDefaults();
            return factory.TryGetValue(type, out var f) ? f() : null;
        }
        static void RegisterDefaults()
        {
            RegisterType<ChestEntity>(); RegisterType<BarrelEntity>(); RegisterType<ShulkerBoxEntity>(); RegisterType<FurnaceEntity>();
            RegisterType<HopperEntity>(); RegisterType<DispenserEntity>(); RegisterType<SpawnerEntity>(); RegisterType<CampfireEntity>();
            RegisterType<BrewingStandEntity>(); RegisterType<ComparatorEntity>(); RegisterType<DaylightDetectorEntity>(); RegisterType<SignEntity>();
            RegisterType<JukeboxEntity>(); RegisterType<BeaconEntity>(); RegisterType<EndGatewayEntity>(); RegisterType<ShelfEntity>(); RegisterType<LecternEntity>();
            RegisterType<CreakingHeartEntity>(); RegisterType<TrialSpawnerEntity>();
        }

        protected static void DropAll(World w, Int3 p, IContainer c)
        {
            for (int i = 0; i < c.Size; i++)
            {
                var s = c.Get(i);
                if (s == null || s.IsEmpty) continue;
                w.SpawnItem(p.Center, s);
                c.Set(i, null);
            }
        }
    }

    public interface IContainerEntity
    {
        string LootTable { get; set; }
        IContainer Container { get; }
    }

    /// <summary>Base for block entities with an inventory.</summary>
    public abstract class ContainerEntity : BlockEntity, IContainer, IContainerEntity
    {
        public ItemStack[] items;
        public string lootTable;
        public string customName;
        protected ContainerEntity(int size) { items = new ItemStack[size]; }
        public string LootTable { get => lootTable; set => lootTable = value; }
        public IContainer Container => this;
        public int Size => items.Length;
        public ItemStack Get(int i) { UnpackLoot(); return items[i]; }
        public void Set(int i, ItemStack s) { items[i] = s != null && s.IsEmpty ? null : s; SetChanged(); }
        public virtual int MaxStackSize => 64;
        public virtual void SetChanged() { MarkDirty(); }
        public void MarkDirty()
        {
            if (world == null) return;
            var c = world.ChunkAtBlock(pos.x, pos.z);
            if (c != null) c.modsDirty = true;
        }
        public virtual bool StillValid(Player p) => world != null && world.GetBlockEntity(pos) == this && (p.position - pos.Center).sqrMagnitude < 64;
        public void UnpackLoot()
        {
            if (lootTable == null || world == null) return;
            var lt = lootTable; lootTable = null;
            var rng = new RNG(world.seed ^ pos.GetHashCode(), pos.x, pos.z, 77);
            Loot.FillContainer(lt, items, ref rng);
        }
        public override void DropContents() { UnpackLoot(); DropAll(world, pos, this); }
        public override void Save(Dictionary<string, string> d)
        {
            var parts = new string[items.Length];
            for (int i = 0; i < items.Length; i++) parts[i] = items[i]?.Serialize() ?? "";
            d["items"] = string.Join(";", parts);
            if (lootTable != null) d["loot"] = lootTable;
            if (customName != null) d["name"] = customName;
        }
        public override void Load(Dictionary<string, string> d)
        {
            if (d.TryGetValue("items", out var s))
            {
                var parts = s.Split(';');
                for (int i = 0; i < items.Length && i < parts.Length; i++) items[i] = ItemStack.Deserialize(parts[i]);
            }
            d.TryGetValue("loot", out lootTable);
            d.TryGetValue("name", out customName);
        }
        public int ComparatorSignal() => SimpleContainer.ComparatorSignal(this);
        public ItemStack AddItem(ItemStack stack, int start = 0, int end = -1)
        {
            if (end < 0) end = items.Length;
            if (stack == null || stack.IsEmpty) return null;
            for (int i = start; i < end && stack.count > 0; i++)
                if (items[i] != null && items[i].Stackable(stack))
                {
                    int m = Math.Min(stack.count, Math.Min(items[i].MaxStack, MaxStackSize) - items[i].count);
                    if (m > 0) { items[i].count += m; stack.count -= m; }
                }
            for (int i = start; i < end && stack.count > 0; i++)
                if (items[i] == null) { int m = Math.Min(stack.count, Math.Min(stack.MaxStack, MaxStackSize)); items[i] = stack.CopyWithCount(m); stack.count -= m; }
            SetChanged();
            return stack.count > 0 ? stack : null;
        }
    }

    public class ChestEntity : ContainerEntity
    {
        public int openers;
        public float lid, prevLid;
        public ChestEntity() : base(27) { ticks = true; }
        public override bool HasVisual => true;
        public void OpenBy(Player p)
        {
            if (openers++ == 0) Sounds.Play(world.GetBlock(pos).id == "ender_chest" ? "block.ender_chest.open" : "block.chest.open", pos.Center, 0.5f, 0.9f + UnityEngine.Random.value * 0.1f);
            world.NotifyNeighbors(pos);
        }
        public void CloseBy(Player p)
        {
            openers = Math.Max(0, openers - 1);
            if (openers == 0) Sounds.Play(world.GetBlock(pos).id == "ender_chest" ? "block.ender_chest.close" : "block.chest.close", pos.Center, 0.5f, 0.9f + UnityEngine.Random.value * 0.1f);
            world.NotifyNeighbors(pos);
        }
        public override void Tick()
        {
            prevLid = lid;
            float target = openers > 0 ? 1f : 0f;
            lid = Mathf.MoveTowards(lid, target, 0.1f);
        }
    }

    public class BarrelEntity : ContainerEntity
    {
        public int openers;
        public BarrelEntity() : base(27) { }
        public void Open() { if (openers++ == 0) { Sounds.Play("block.barrel.open", pos.Center, 0.5f, 1f); SetOpenState(true); } }
        public void Close() { openers = Math.Max(0, openers - 1); if (openers == 0) { Sounds.Play("block.barrel.close", pos.Center, 0.5f, 1f); SetOpenState(false); } }
        void SetOpenState(bool open)
        {
            var b = world.GetBlock(pos);
            int m = world.GetMeta(pos);
            if (b is BarrelBlock) world.SetState(pos, b.State(open ? (m | 8) : (m & 7)), 0);
        }
    }

    public class ShulkerBoxEntity : ChestEntity
    {
        public void Close() => CloseBy(null);
        public override void DropContents() { }
    }

    /// <summary>Wraps two chests as one 54-slot container.</summary>
    public class DoubleChestContainer : IContainer
    {
        public ChestEntity a, b; // a = top rows
        public DoubleChestContainer(ChestEntity a, ChestEntity b) { this.a = a; this.b = b; }
        public int Size => 54;
        public ItemStack Get(int i) => i < 27 ? a.Get(i) : b.Get(i - 27);
        public void Set(int i, ItemStack s) { if (i < 27) a.Set(i, s); else b.Set(i - 27, s); }
        public int MaxStackSize => 64;
        public void SetChanged() { a.SetChanged(); b.SetChanged(); }
        public bool StillValid(Player p) => a.StillValid(p) && b.StillValid(p);
        public void Close(Player p) { a.CloseBy(p); b.CloseBy(p); }
    }

    /// <summary>Per-player ender chest inventory.</summary>
    public class EnderChestContainer : SimpleContainer
    {
        public ChestEntity chest;
        public EnderChestContainer() : base(27) { }
        public void Close() { chest?.CloseBy(null); chest = null; }
        public override bool StillValid(Player p) => chest == null || chest.StillValid(p);
    }

    // ============================================================================ furnace
    public class FurnaceEntity : ContainerEntity
    {
        public int burnTime, burnDuration, cookTime, cookTotal = 200;
        public float storedXp;
        public string kind = "furnace"; // furnace, smoker, blast_furnace
        public FurnaceEntity() : base(3) { ticks = true; }
        public float BurnFrac => burnDuration > 0 ? (float)burnTime / burnDuration : 0;
        public float CookFrac => cookTotal > 0 ? (float)cookTime / cookTotal : 0;
        public bool IsLit => burnTime > 0;
        float SpeedMul => kind == "furnace" ? 1f : 2f;

        public override void Tick()
        {
            bool wasLit = IsLit;
            if (burnTime > 0) burnTime--;
            var input = items[0]; var fuel = items[1]; var output = items[2];
            var recipe = input != null ? Recipes.FindSmelting(input.item, kind) : null;
            bool canCook = recipe != null && CanOutput(recipe);
            if (!IsLit && canCook && fuel != null && Recipes.FuelValue(fuel.item) > 0)
            {
                burnDuration = burnTime = (int)(Recipes.FuelValue(fuel.item) / SpeedMul);
                if (fuel.item.id == "lava_bucket") { items[1] = new ItemStack("bucket", 1); }
                else { fuel.count--; if (fuel.count <= 0) items[1] = null; }
                MarkDirty();
            }
            if (IsLit && canCook)
            {
                cookTotal = (int)(200 / SpeedMul);
                cookTime++;
                if (cookTime >= cookTotal)
                {
                    cookTime = 0;
                    var result = recipe.result.Copy();
                    if (items[2] == null) items[2] = result; else items[2].count += result.count;
                    storedXp += recipe.xp;
                    input.count--; if (input.count <= 0) items[0] = null;
                    MarkDirty();
                }
            }
            else if (cookTime > 0) cookTime = Math.Max(0, cookTime - 2);
            if (wasLit != IsLit)
            {
                var b = world.GetBlock(pos);
                int m = world.GetMeta(pos);
                world.SetState(pos, b.State(IsLit ? (m | 4) : (m & 3)), SetFlags.Hooks);
            }
        }
        bool CanOutput(SmeltingRecipe r)
        {
            var o = items[2];
            if (o == null) return true;
            if (!o.Stackable(r.result)) return false;
            return o.count + r.result.count <= o.MaxStack;
        }
        public void TakeXp(Player p)
        {
            int xp = Mathf.FloorToInt(storedXp);
            if (UnityEngine.Random.value < storedXp - xp) xp++;
            storedXp = 0;
            if (xp > 0) XpOrb.Spawn(world, p.position + Vector3.up * 0.5f, xp);
        }
        public override void Save(Dictionary<string, string> d)
        {
            base.Save(d);
            d["burn"] = burnTime.ToString(); d["burnDur"] = burnDuration.ToString(); d["cook"] = cookTime.ToString(); d["xp"] = storedXp.ToString("R"); d["kind"] = kind;
        }
        public override void Load(Dictionary<string, string> d)
        {
            base.Load(d);
            if (d.TryGetValue("burn", out var a)) int.TryParse(a, out burnTime);
            if (d.TryGetValue("burnDur", out var b)) int.TryParse(b, out burnDuration);
            if (d.TryGetValue("cook", out var c)) int.TryParse(c, out cookTime);
            if (d.TryGetValue("xp", out var x)) float.TryParse(x, out storedXp);
            if (d.TryGetValue("kind", out var k)) kind = k;
        }
    }

    // ============================================================================ hopper / dispenser
    public class HopperEntity : ContainerEntity
    {
        int cooldown;
        public HopperEntity() : base(5) { ticks = true; }
        public override void Tick()
        {
            if (--cooldown > 0) return;
            cooldown = 0;
            int meta = world.GetMeta(pos);
            if ((meta & 8) != 0) return; // disabled by redstone
            bool moved = false;
            // push into container in facing direction
            Dir f = HopperBlock.Facing(meta);
            var target = HopperBlock.ContainerAt(world, pos.Offset(f));
            if (target != null)
            {
                for (int i = 0; i < 5 && !moved; i++)
                {
                    if (items[i] == null) continue;
                    var one = items[i].CopyWithCount(1);
                    if (HopperBlock.Insert(target, one, DirUtil.Opposite(f)))
                    {
                        items[i].count--; if (items[i].count <= 0) items[i] = null;
                        moved = true; MarkDirty();
                    }
                }
            }
            // pull from container above
            var src = HopperBlock.ContainerAt(world, pos.Offset(Dir.Up));
            if (src != null)
            {
                for (int i = 0; i < src.Size; i++)
                {
                    var s = src.Get(i);
                    if (s == null || s.IsEmpty) continue;
                    if (src is FurnaceEntity && i != 2) continue;
                    var one = s.CopyWithCount(1);
                    if (AddItem(one) == null) { s.count--; if (s.count <= 0) src.Set(i, null); src.SetChanged(); moved = true; break; }
                }
            }
            else
            {
                // suck item entities
                var box = new AABB(pos.x, pos.y + 1, pos.z, pos.x + 1, pos.y + 1.6f, pos.z + 1);
                foreach (var e in world.GetEntities(box))
                    if (e is ItemEntity ie && !ie.removed)
                    {
                        var left = AddItem(ie.stack.Copy());
                        if (left == null) ie.Remove(); else ie.stack.count = left.count;
                        moved = true; break;
                    }
            }
            if (moved) cooldown = 8;
        }
    }

    public class DispenserEntity : ContainerEntity
    {
        public bool dropper;
        public DispenserEntity() : base(9) { }
        public override void Save(Dictionary<string, string> d) { base.Save(d); d["dropper"] = dropper ? "1" : "0"; }
        public override void Load(Dictionary<string, string> d) { base.Load(d); if (d.TryGetValue("dropper", out var v)) dropper = v == "1"; }
    }

    public class BrewingStandEntity : ContainerEntity
    {
        public int brewTime, fuel;
        public BrewingStandEntity() : base(5) { ticks = true; } // 0-2 bottles, 3 ingredient, 4 fuel
        public override void Tick()
        {
            if (fuel <= 0 && items[4] != null && items[4].item.id == "blaze_powder") { fuel = 20; items[4].count--; if (items[4].count <= 0) items[4] = null; MarkDirty(); }
            bool canBrew = items[3] != null && fuel > 0 && Brewing.CanBrewAny(items, items[3]);
            if (brewTime > 0)
            {
                brewTime--;
                if (!canBrew) brewTime = 0;
                else if (brewTime == 0)
                {
                    Brewing.Brew(items, items[3]);
                    items[3].count--; if (items[3].count <= 0) items[3] = null;
                    fuel--;
                    Sounds.Play("block.brewing_stand.brew", pos.Center, 1f, 1f);
                    MarkDirty();
                }
            }
            else if (canBrew) brewTime = 400;
        }
        public override void Save(Dictionary<string, string> d) { base.Save(d); d["brew"] = brewTime.ToString(); d["fuel"] = fuel.ToString(); }
        public override void Load(Dictionary<string, string> d) { base.Load(d); if (d.TryGetValue("brew", out var b)) int.TryParse(b, out brewTime); if (d.TryGetValue("fuel", out var f)) int.TryParse(f, out fuel); }
    }

    // ============================================================================ spawner
    public class SpawnerEntity : BlockEntity
    {
        public string mob = "pig";
        int delay = 20;
        public float spin, prevSpin;
        public SpawnerEntity() { ticks = true; }
        public override bool HasVisual => true;
        public override void Tick()
        {
            prevSpin = spin;
            var p = GameManager.Instance?.player;
            if (p == null || p.world != world || (p.position - pos.Center).sqrMagnitude > 16 * 16) return;
            spin = (spin + 1000f / (delay + 200f)) % 360f;
            if (UnityEngine.Random.value < 0.3f) { Particles.Smoke(world, pos.Center + UnityEngine.Random.insideUnitSphere * 0.5f, 1, 0.3f); Particles.Flame(world, pos.Center + UnityEngine.Random.insideUnitSphere * 0.5f, "flame"); }
            if (--delay > 0) return;
            delay = 200 + world.rand.Next(600);
            if (world.session != null && world.session.difficulty == Difficulty.Peaceful && MobRegistry.IsHostile(mob)) return;
            var near = world.CountEntities(e => e is Mob m && m.def.id == mob && (e.position - pos.Center).sqrMagnitude < 81);
            if (near >= 6) return;
            int n = world.rand.Range(1, 4);
            for (int i = 0; i < n; i++)
            {
                var sp = pos.Center + new Vector3(world.rand.Range(-4f, 4f), world.rand.Range(-1, 2), world.rand.Range(-4f, 4f));
                Int3 bp = Int3.Floor(sp);
                if (!world.GetBlock(bp).solid && !world.GetBlock(bp.Offset(Dir.Up)).solid && world.GetBlock(bp.Offset(Dir.Down)).solid)
                {
                    var m = MobRegistry.Spawn(world, mob, new Vector3(bp.x + 0.5f, bp.y, bp.z + 0.5f), SpawnReason.Spawner);
                    if (m != null) Particles.Poof(world, m.position + Vector3.up * 0.5f, 0.6f, 1f);
                }
            }
        }
        public override void Save(Dictionary<string, string> d) { d["mob"] = mob; d["delay"] = delay.ToString(); }
        public override void Load(Dictionary<string, string> d) { if (d.TryGetValue("mob", out var m)) mob = m; if (d.TryGetValue("delay", out var v)) int.TryParse(v, out delay); }
    }

    public class TrialSpawnerEntity : SpawnerEntity { }

    public class CampfireEntity : BlockEntity
    {
        public readonly ItemStack[] food = new ItemStack[4];
        public readonly int[] cook = new int[4];
        public CampfireEntity() { ticks = true; }
        public override bool HasVisual => true;
        public override void Tick()
        {
            int m = world.GetMeta(pos);
            if (!CampfireBlock.Lit(m)) return;
            for (int i = 0; i < 4; i++)
            {
                if (food[i] == null) continue;
                if (++cook[i] >= 600)
                {
                    var r = Recipes.FindSmelting(food[i].item, "campfire");
                    if (r != null) world.SpawnItem(pos.Center + Vector3.up * 0.5f, r.result.Copy());
                    food[i] = null; cook[i] = 0;
                }
            }
        }
        public bool AddFood(ItemStack s)
        {
            for (int i = 0; i < 4; i++) if (food[i] == null) { food[i] = s.CopyWithCount(1); cook[i] = 0; return true; }
            return false;
        }
        public override void DropContents() { for (int i = 0; i < 4; i++) if (food[i] != null) { world.SpawnItem(pos.Center, food[i]); food[i] = null; } }
        public override void Save(Dictionary<string, string> d) { for (int i = 0; i < 4; i++) if (food[i] != null) { d["f" + i] = food[i].Serialize(); d["c" + i] = cook[i].ToString(); } }
        public override void Load(Dictionary<string, string> d) { for (int i = 0; i < 4; i++) { if (d.TryGetValue("f" + i, out var f)) food[i] = ItemStack.Deserialize(f); if (d.TryGetValue("c" + i, out var c)) int.TryParse(c, out cook[i]); } }
    }

    public class SignEntity : BlockEntity
    {
        public string[] lines = { "", "", "", "" };
        public override void Save(Dictionary<string, string> d) { d["text"] = string.Join("\n", lines); }
        public override void Load(Dictionary<string, string> d) { if (d.TryGetValue("text", out var t)) { var p = t.Split('\n'); for (int i = 0; i < 4 && i < p.Length; i++) lines[i] = p[i]; } }
    }

    public class JukeboxEntity : BlockEntity
    {
        public ItemStack record;
        public override void DropContents() { if (record != null) { world.SpawnItem(pos.Center + Vector3.up * 0.7f, record); record = null; Sounds.StopMusicAt(pos); } }
        public override void Save(Dictionary<string, string> d) { if (record != null) d["rec"] = record.Serialize(); }
        public override void Load(Dictionary<string, string> d) { if (d.TryGetValue("rec", out var r)) record = ItemStack.Deserialize(r); }
    }

    public class BeaconEntity : BlockEntity
    {
        public int levels;
        public string primary, secondary;
        public BeaconEntity() { ticks = true; }
        public override bool HasVisual => true;
        public override void Tick()
        {
            if (world.tickCount % 80 != 0) return;
            levels = Beacon.ComputeLevels(world, pos);
            if (levels > 0 && primary != null && world.CanSeeSky(pos.Offset(Dir.Up)))
            {
                float range = levels * 10 + 10;
                var eff = Effect.Get(primary);
                int amp = secondary == primary ? 1 : 0;
                foreach (var e in world.entities)
                    if (e is Player p && (p.position - pos.Center).sqrMagnitude < range * range)
                    {
                        if (eff != null) p.AddEffect(new EffectInstance(eff, (9 + levels * 2) * 20, amp, true));
                        if (secondary == "regeneration") p.AddEffect(new EffectInstance(Effect.Regeneration, (9 + levels * 2) * 20, 0, true));
                    }
            }
        }
        public override void Save(Dictionary<string, string> d) { if (primary != null) d["p"] = primary; if (secondary != null) d["s"] = secondary; }
        public override void Load(Dictionary<string, string> d) { d.TryGetValue("p", out primary); d.TryGetValue("s", out secondary); }
    }

    public class EndGatewayEntity : BlockEntity
    {
        public Int3 exit; public bool hasExit; public int cooldown;
        public EndGatewayEntity() { ticks = true; }
        public override bool HasVisual => true;
        public override void Tick() { if (cooldown > 0) cooldown--; }
        public override void Save(Dictionary<string, string> d) { if (hasExit) d["exit"] = exit.x + "," + exit.y + "," + exit.z; }
        public override void Load(Dictionary<string, string> d)
        {
            if (d.TryGetValue("exit", out var e)) { var p = e.Split(','); exit = new Int3(int.Parse(p[0]), int.Parse(p[1]), int.Parse(p[2])); hasExit = true; }
        }
    }

    public class ShelfEntity : ContainerEntity
    {
        public ShelfEntity() : base(3) { }
        public override bool HasVisual => true;
        public override int MaxStackSize => 64;
    }

    public class LecternEntity : BlockEntity
    {
        public ItemStack book;
        public override void DropContents() { if (book != null) { world.SpawnItem(pos.Center + Vector3.up * 0.5f, book); book = null; } }
        public override void Save(Dictionary<string, string> d) { if (book != null) d["book"] = book.Serialize(); }
        public override void Load(Dictionary<string, string> d) { if (d.TryGetValue("book", out var b)) book = ItemStack.Deserialize(b); }
    }

    public class CreakingHeartEntity : BlockEntity
    {
        public int creakingId = -1; int timer;
        public CreakingHeartEntity() { ticks = true; }
        public override void Tick()
        {
            if (++timer % 40 != 0) return;
            bool night = world.session != null && world.session.IsNight;
            var existing = creakingId >= 0 ? world.entities.Find(e => e.id == creakingId && !e.removed) : null;
            if (night && existing == null && world.session.difficulty != Difficulty.Peaceful)
            {
                var p = GameManager.Instance?.player;
                if (p == null || (p.position - pos.Center).sqrMagnitude > 32 * 32) return;
                for (int i = 0; i < 8; i++)
                {
                    var sp = pos.Center + new Vector3(world.rand.Range(-8, 8), 0, world.rand.Range(-8, 8));
                    int y = world.TopSurfaceY(Mathf.FloorToInt(sp.x), Mathf.FloorToInt(sp.z)) + 1;
                    var m = MobRegistry.Spawn(world, "creaking", new Vector3(Mathf.Floor(sp.x) + 0.5f, y, Mathf.Floor(sp.z) + 0.5f), SpawnReason.Structure);
                    if (m != null) { creakingId = m.id; if (m is CreakingMob cr) cr.heart = pos; break; }
                }
            }
            else if (!night && existing != null) { existing.Remove(); creakingId = -1; }
        }
        public override void OnRemoved()
        {
            base.OnRemoved();
            var existing = creakingId >= 0 ? world?.entities.Find(e => e.id == creakingId && !e.removed) : null;
            if (existing is LivingEntity le) le.KillFromCommand();
        }
    }
}
