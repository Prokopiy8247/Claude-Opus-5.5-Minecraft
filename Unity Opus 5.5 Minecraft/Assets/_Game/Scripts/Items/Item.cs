using System;
using System.Collections.Generic;
using System.Text;
using UnityEngine;

namespace MCR
{
    public enum CreativeTab : byte
    {
        Building, Colored, Natural, Functional, Redstone, Tools, Combat, Food, Ingredients, SpawnEggs, Operator, None
    }

    public enum UseAnim : byte { None, Eat, Drink, Block, Bow, Crossbow, Spear, Trident, Brush, Horn, Spyglass }
    public enum ArmorSlot : byte { None = 255, Feet = 0, Legs = 1, Chest = 2, Head = 3 }
    public enum Rarity : byte { Common, Uncommon, Rare, Epic }
    public enum HeldModel : byte { Flat, Block, Tool, Custom }

    /// <summary>Context for using an item on a block.</summary>
    public struct UseOnContext
    {
        public World world;
        public Player player;
        public ItemStack stack;
        public Int3 pos;
        public Dir face;
        public Vector3 hit;       // absolute hit point
        public bool sneaking;
        public Int3 Adjacent => pos.Offset(face);
    }

    public enum UseResult : byte { Pass, Success, Consume, Fail }

    /// <summary>Base item. Immutable after registration.</summary>
    public class Item
    {
        public string id;
        public int index;
        public string displayName;
        public int maxStack = 64;
        public int maxDamage = 0;
        public CreativeTab tab = CreativeTab.Ingredients;
        public bool hiddenInCreative = false;
        public Rarity rarity = Rarity.Common;
        public Block block;               // block placed by this item (block items)
        public ToolType toolType = ToolType.None;
        public int tier = 0;
        public float miningSpeed = 1f;
        public float attackDamage = 1f;   // total damage (incl. base fist 1)
        public float attackSpeed = 4f;    // attacks per second
        public int enchantability = 0;
        public string repairItem;
        public int fuelTicks = 0;
        public bool fireResistant = false;
        public string craftRemainder;     // e.g. bucket for milk bucket
        public string iconName;           // icon key (defaults to id)
        public HeldModel heldModel = HeldModel.Flat;
        public string modelName;          // Blender model name for held/world rendering
        public int iconIndex = -1;        // index into item icon atlas
        public Color32 tintColor = new Color32(255, 255, 255, 255);
        public bool glint = false;
        public string description;

        public virtual bool IsDamageable => maxDamage > 0;
        public virtual string GetName(ItemStack s) => displayName;
        public virtual void AppendTooltip(ItemStack s, List<string> lines) { if (description != null) lines.Add("§7" + description); }

        /// <summary>Right click on a block. Default: place block if this is a block item.</summary>
        public virtual UseResult UseOn(ref UseOnContext ctx) => UseResult.Pass;
        /// <summary>Right click in air (or after UseOn passes).</summary>
        public virtual UseResult Use(World w, Player p, ItemStack s) => UseResult.Pass;
        /// <summary>Called each tick while using (holding right mouse). ticksUsed counts up.</summary>
        public virtual void UsingTick(World w, Player p, ItemStack s, int ticksUsed) { }
        public virtual void ReleaseUsing(World w, Player p, ItemStack s, int ticksUsed) { }
        public virtual void FinishUsing(World w, Player p, ItemStack s) { }
        public virtual int UseDuration(ItemStack s) => 0;
        public virtual UseAnim GetUseAnim(ItemStack s) => UseAnim.None;
        public virtual UseResult InteractEntity(World w, Player p, ItemStack s, Entity target) => UseResult.Pass;
        public virtual void OnHitEntity(ItemStack s, LivingEntity target, LivingEntity attacker) { }
        public virtual float GetMiningSpeed(ItemStack s, Block b)
        {
            if (toolType == ToolType.None) return 1f;
            if (toolType == ToolType.Sword)
            {
                if (b.id == "cobweb") return 15f;
                if (b is LeavesBlock || b.sound == SoundType.Crop) return 1.5f;
                return 1f;
            }
            if (toolType == ToolType.Shears)
            {
                if (b.id == "cobweb" || b is LeavesBlock) return 15f;
                if (b.id.EndsWith("wool")) return 5f;
                if (b.id == "vine" || b.id == "glow_lichen") return 2f;
                return 1f;
            }
            return b.tool == toolType ? miningSpeed : 1f;
        }
        public virtual bool IsCorrectToolFor(Block b)
        {
            if (!b.requiresTool) return true;
            if (toolType == ToolType.Shears && (b.id == "cobweb" || b is LeavesBlock || b.id.EndsWith("wool"))) return true;
            if (toolType == ToolType.Sword && b.id == "cobweb") return true;
            return b.tool == toolType && tier >= b.toolTier;
        }
        public virtual void InventoryTick(ItemStack s, Entity holder, int slot, bool selected) { }
        public override string ToString() => id;
    }

    /// <summary>Mutable stack of items with optional data (damage, enchantments, name, custom data).</summary>
    [Serializable]
    public sealed class ItemStack
    {
        public Item item;
        public int count;
        public int damage;
        public Dictionary<Enchant, int> enchants;
        public string customName;
        /// <summary>Generic key/value data (potion type, dye color, stored enchantments, fireworks, etc.).</summary>
        public Dictionary<string, string> data;

        public ItemStack(Item item, int count = 1) { this.item = item; this.count = count; }
        public ItemStack(string id, int count = 1) { item = Items.Get(id); this.count = item == null ? 0 : count; }

        public bool IsEmpty => item == null || count <= 0;
        public int MaxStack => item == null ? 64 : (item.maxStack);
        public bool IsDamaged => damage > 0;

        public ItemStack Copy()
        {
            var s = new ItemStack(item, count) { damage = damage, customName = customName };
            if (enchants != null) s.enchants = new Dictionary<Enchant, int>(enchants);
            if (data != null) s.data = new Dictionary<string, string>(data);
            return s;
        }
        public ItemStack CopyWithCount(int n) { var s = Copy(); s.count = n; return s; }

        public ItemStack Split(int n)
        {
            n = Math.Min(n, count);
            var s = CopyWithCount(n);
            count -= n;
            return s;
        }

        public bool SameItem(ItemStack o) => o != null && o.item == item;

        /// <summary>Can the two stacks merge (same item, same data)?</summary>
        public bool Stackable(ItemStack o)
        {
            if (o == null || o.item != item) return false;
            if (item.maxStack <= 1) return false;
            if (damage != o.damage) return false;
            if (customName != o.customName) return false;
            if (!DictEq(enchants, o.enchants)) return false;
            if (!DictEq(data, o.data)) return false;
            return true;
        }

        static bool DictEq<K, V>(Dictionary<K, V> a, Dictionary<K, V> b)
        {
            int ac = a?.Count ?? 0, bc = b?.Count ?? 0;
            if (ac != bc) return false;
            if (ac == 0) return true;
            foreach (var kv in a)
            {
                if (!b.TryGetValue(kv.Key, out var v)) return false;
                if (!EqualityComparer<V>.Default.Equals(v, kv.Value)) return false;
            }
            return true;
        }

        public int GetEnchant(Enchant e) => enchants != null && enchants.TryGetValue(e, out int l) ? l : 0;
        public void AddEnchant(Enchant e, int level)
        {
            if (enchants == null) enchants = new Dictionary<Enchant, int>();
            enchants[e] = level;
        }
        public bool IsEnchanted => enchants != null && enchants.Count > 0;
        public bool HasGlint => IsEnchanted || item.glint || (data != null && data.ContainsKey("stored_enchants"));

        public string Get(string key) => data != null && data.TryGetValue(key, out var v) ? v : null;
        public int GetInt(string key, int def = 0) => int.TryParse(Get(key), out int v) ? v : def;
        public void Set(string key, string value)
        {
            if (value == null) { data?.Remove(key); if (data != null && data.Count == 0) data = null; return; }
            if (data == null) data = new Dictionary<string, string>();
            data[key] = value;
        }

        public string DisplayName => customName ?? item.GetName(this);
        public int MaxDamage => item.maxDamage;

        /// <summary>Apply durability damage honouring Unbreaking. Returns true if the item broke.</summary>
        public bool HurtAndBreak(int amount, LivingEntity owner)
        {
            if (!item.IsDamageable) return false;
            if (owner is Player p && p.IsCreative) return false;
            int unbreaking = GetEnchant(Enchant.Unbreaking);
            for (int i = 0; i < amount; i++)
            {
                if (unbreaking > 0 && UnityEngine.Random.value > 1f / (unbreaking + 1)) continue;
                damage++;
            }
            if (damage >= item.maxDamage)
            {
                count--;
                damage = 0;
                if (owner != null) { Sounds.Play("item.break", owner.position, 0.8f, 0.9f + UnityEngine.Random.value * 0.2f); Particles.ItemBreak(owner.world, owner.EyePosition, this); }
                return true;
            }
            return false;
        }

        public float DurabilityFraction => item.maxDamage <= 0 ? 1f : 1f - (float)damage / item.maxDamage;

        public override string ToString() => IsEmpty ? "empty" : $"{count}x {item.id}";

        public static bool IsNullOrEmpty(ItemStack s) => s == null || s.IsEmpty;

        // --------------- serialization (compact string) ---------------
        public string Serialize()
        {
            if (IsEmpty) return "";
            var sb = new StringBuilder();
            sb.Append(item.id).Append('*').Append(count);
            if (damage > 0) sb.Append("|d=").Append(damage);
            if (customName != null) sb.Append("|n=").Append(Escape(customName));
            if (enchants != null) foreach (var kv in enchants) sb.Append("|e=").Append(kv.Key.id).Append(':').Append(kv.Value);
            if (data != null) foreach (var kv in data) sb.Append("|k=").Append(Escape(kv.Key)).Append('=').Append(Escape(kv.Value));
            return sb.ToString();
        }

        static string Escape(string s) => s.Replace("%", "%25").Replace("|", "%7C").Replace("=", "%3D").Replace(";", "%3B");
        static string Unescape(string s) => s.Replace("%3B", ";").Replace("%3D", "=").Replace("%7C", "|").Replace("%25", "%");

        public static ItemStack Deserialize(string str)
        {
            if (string.IsNullOrEmpty(str)) return null;
            var parts = str.Split('|');
            var head = parts[0].Split('*');
            var it = Items.Get(head[0]);
            if (it == null) return null;
            int cnt = head.Length > 1 && int.TryParse(head[1], out int c) ? c : 1;
            var s = new ItemStack(it, cnt);
            for (int i = 1; i < parts.Length; i++)
            {
                string p = parts[i];
                if (p.StartsWith("d=")) int.TryParse(p.Substring(2), out s.damage);
                else if (p.StartsWith("n=")) s.customName = Unescape(p.Substring(2));
                else if (p.StartsWith("e="))
                {
                    var ev = p.Substring(2).Split(':');
                    var e = Enchant.Get(ev[0]);
                    if (e != null && ev.Length > 1 && int.TryParse(ev[1], out int lvl)) s.AddEnchant(e, lvl);
                }
                else if (p.StartsWith("k="))
                {
                    string kv = p.Substring(2);
                    int eq = kv.IndexOf('=');
                    if (eq > 0) s.Set(Unescape(kv.Substring(0, eq)), Unescape(kv.Substring(eq + 1)));
                }
            }
            return s;
        }
    }

    public static partial class Items
    {
        public static readonly List<Item> All = new List<Item>();
        static readonly Dictionary<string, Item> byId = new Dictionary<string, Item>();
        public static bool Initialized;

        public static Item Get(string id)
        {
            if (id == null) return null;
            if (id.StartsWith("minecraft:")) id = id.Substring(10);
            return byId.TryGetValue(id, out var it) ? it : null;
        }
        public static bool Exists(string id) => Get(id) != null;

        public static T Reg<T>(string id, T it) where T : Item
        {
            if (byId.ContainsKey(id)) { Debug.LogError("Duplicate item id " + id); return (T)byId[id]; }
            it.id = id;
            if (string.IsNullOrEmpty(it.displayName)) it.displayName = Blocks.PrettyName(id);
            if (it.iconName == null) it.iconName = id;
            it.index = All.Count;
            All.Add(it);
            byId[id] = it;
            return it;
        }

        public static Item Reg(string id, CreativeTab tab = CreativeTab.Ingredients, int maxStack = 64)
            => Reg(id, new Item { tab = tab, maxStack = maxStack });

        public static void Init()
        {
            if (Initialized) return;
            All.Clear(); byId.Clear();
            // block items first (for every block that has an item)
            foreach (var b in Blocks.All)
            {
                if (b.noItem || b.isAir) continue;
                var bi = CreateBlockItem(b);
                Reg(b.id, bi);
                b.item = bi;
            }
            RegisterAll();
            // resolve block item links for blocks that drop items registered later
            Initialized = true;
            Debug.Log($"[Items] Registered {All.Count} items");
        }

        static Item CreateBlockItem(Block b)
        {
            BlockItem bi = b.CustomItem() ?? new BlockItem();
            bi.block = b;
            bi.displayName = b.displayName;
            bi.tab = b.creativeTab;
            bi.hiddenInCreative = b.hiddenInCreative;
            bi.heldModel = HeldModel.Block;
            if (b.flammability > 0 && b.fireSpread > 0 && bi.fuelTicks == 0) bi.fuelTicks = b.solid ? 300 : 100;
            return bi;
        }
    }
}
