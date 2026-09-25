using System;
using System.Collections.Generic;
using System.Text;
using UnityEngine;

namespace MCR
{
    // ============================================================================ shared pieces
    /// <summary>Helpers shared by the block and villager menus.</summary>
    public static class MenuUtil
    {
        /// <summary>Menus stay open while the player is within 8 blocks of what they belong to.</summary>
        public const float ReachSq = 64f;
        /// <summary>Item data key for the anvil's prior-work penalty.</summary>
        public const string RepairCostKey = "repair_cost";

        public static bool Near(Player p, Int3 pos) => (p.position - pos.Center).sqrMagnitude < ReachSq;
        public static bool AtBlock(Player p, Int3 pos, string id) => p.world.GetBlock(pos).id == id && Near(p, pos);

        /// <summary>
        /// Would <c>Menu.MoveItemTo(s, start, end, ...)</c> place the whole stack? Mirrors its order: merge into
        /// matching stacks first, then a single empty slot.
        /// </summary>
        public static bool CanFit(Menu m, ItemStack s, int start, int end)
        {
            int room = 0;
            bool usedEmpty = false;
            for (int i = start; i < end; i++)
            {
                var sl = m.slots[i];
                if (!sl.MayPlace(s)) continue;
                var cur = sl.Item;
                if (cur == null || cur.IsEmpty)
                {
                    if (!sl.active || usedEmpty) continue;
                    usedEmpty = true;
                    room += sl.MaxStackFor(s);
                }
                else if (cur.Stackable(s)) room += Math.Max(0, sl.MaxStackFor(s) - cur.count);
                if (room >= s.count) return true;
            }
            return false;
        }

        public static bool IsEnchantedBook(ItemStack s) => s != null && s.item.id == "enchanted_book";

        /// <summary>An item's enchantments as a fresh map; enchanted books report their stored list.</summary>
        public static Dictionary<Enchant, int> EnchantsOf(ItemStack s)
        {
            var d = new Dictionary<Enchant, int>();
            if (s == null) return d;
            if (IsEnchantedBook(s))
            {
                foreach (var (e, l) in EnchantedBookItem.Stored(s)) d[e] = d.TryGetValue(e, out int had) ? Math.Max(had, l) : l;
            }
            else if (s.enchants != null)
            {
                foreach (var kv in s.enchants) d[kv.Key] = kv.Value;
            }
            return d;
        }

        /// <summary>Writes enchantments back the way the item keeps them: stored list on books, applied otherwise.</summary>
        public static void SetEnchants(ItemStack s, Dictionary<Enchant, int> d)
        {
            if (IsEnchantedBook(s))
            {
                if (d.Count == 0) { s.Set("stored_enchants", null); return; }
                var sb = new StringBuilder();
                foreach (var kv in d)
                {
                    if (sb.Length > 0) sb.Append(',');
                    sb.Append(kv.Key.id).Append(':').Append(kv.Value);
                }
                s.Set("stored_enchants", sb.ToString());
            }
            else s.enchants = d.Count == 0 ? null : new Dictionary<Enchant, int>(d);
        }

        public static int PriorWork(ItemStack s) => s == null ? 0 : s.GetInt(RepairCostKey);
        public static void SetPriorWork(ItemStack s, int v) => s.Set(RepairCostKey, v > 0 ? v.ToString() : null);

        /// <summary>Colour name of a dye item id ("red_dye" -> "red"), or null when it is not a dye.</summary>
        public static string DyeColor(string itemId) =>
            itemId != null && itemId.EndsWith("_dye") ? itemId.Substring(0, itemId.Length - 4) : null;
    }

    /// <summary>A slot the player can take from but never put into (furnace output).</summary>
    public class TakeOnlySlot : Slot
    {
        public TakeOnlySlot(IContainer c, int index, int x, int y) : base(c, index, x, y) { }
        public override bool MayPlace(ItemStack s) => false;
    }

    /// <summary>
    /// Base for menus whose slots are temporary inputs feeding one computed result: smithing, anvil, grindstone,
    /// stonecutter, loom and villager trading. The inputs live in a <see cref="CraftingGrid"/> owned by the menu
    /// and go back to the player on close; the result is recomputed whenever an input changes.
    /// </summary>
    public abstract class WorkstationMenu : ContainerMenu
    {
        protected readonly CraftingGrid inputs;
        readonly Output output;
        bool consuming;

        protected WorkstationMenu(Player p, int inputCount)
        {
            player = p;
            inputs = new CraftingGrid(inputCount, 1);
            output = new Output(this);
            inputs.onChanged = OnInputsChanged;
        }

        /// <summary>The computed result, visible to the UI even while <see cref="CanTake"/> keeps it out of the slot.</summary>
        public ItemStack Preview => output.stack;

        /// <summary>Whether the player may take the result right now (the anvil checks levels here).</summary>
        public virtual bool CanTake(Player p) => true;

        /// <summary>Recalculate the result from the inputs; call <see cref="SetOutput"/> with it (or null).</summary>
        protected abstract void Recompute();
        /// <summary>The result has left the slot: consume whatever produced it.</summary>
        protected abstract void OnResultTaken(Player p, ItemStack taken);

        protected Slot AddInput(int index, int x, int y) => AddSlot(new Slot(inputs, index, x, y));
        protected Slot AddResult(int x, int y) => AddSlot(new StationResultSlot(this, x, y));
        protected void FinishLayout(int invX = 8, int invY = 84)
        {
            containerSlots = slots.Count;
            AddPlayerSlots(player.inventory, invX, invY);
        }

        protected ItemStack In(int i) { var s = inputs.items[i]; return s != null && !s.IsEmpty ? s : null; }
        protected void SetOutput(ItemStack s) => output.stack = s != null && s.IsEmpty ? null : s;

        /// <summary>Removes <paramref name="n"/> items from input <paramref name="i"/>.</summary>
        protected void ConsumeInput(int i, int n)
        {
            var s = inputs.items[i];
            if (s == null || n <= 0) return;
            s.count -= n;
            if (s.count <= 0) inputs.items[i] = null;
        }

        void OnInputsChanged() { if (!consuming) Recompute(); }

        void TakeResult(Player p, ItemStack taken)
        {
            // a partial take (throwing one of a stack) still pays the whole price, so hand over the rest too
            var rest = output.stack;
            output.stack = null;
            if (rest != null && rest.count > 0) p.inventory.AddOrDrop(rest);
            consuming = true;
            try { OnResultTaken(p, taken); }
            finally { consuming = false; }
            Recompute();
        }

        /// <summary>Shift-clicking the result is all-or-nothing, so a full inventory leaves the inputs alone.</summary>
        public override ItemStack QuickMove(int idx)
        {
            var slot = slots[idx];
            if (!slot.isResult) return base.QuickMove(idx);
            if (!slot.HasItem || !slot.MayPickup(player)) return null;
            var stack = slot.Item;
            if (!MenuUtil.CanFit(this, stack, containerSlots, slots.Count)) return null;
            var copy = stack.Copy();
            var taken = slot.Remove(stack.count);
            slot.OnTake(player, taken);
            MoveItemTo(taken, containerSlots, slots.Count, true);
            if (taken.count > 0) player.inventory.AddOrDrop(taken);
            SlotChanged(slot);
            return copy;
        }

        public override void Removed()
        {
            base.Removed();
            for (int i = 0; i < inputs.Size; i++)
                if (inputs.items[i] != null) { player.inventory.AddOrDrop(inputs.items[i]); inputs.items[i] = null; }
            output.stack = null;
        }

        /// <summary>
        /// One-slot result store. Reads come back empty while the menu refuses the take, which closes every take
        /// path in <see cref="Menu.Click"/> at once: pickup, number-key swap and shift-click all read the slot first.
        /// </summary>
        sealed class Output : IContainer
        {
            readonly WorkstationMenu menu;
            public ItemStack stack;
            public Output(WorkstationMenu m) { menu = m; }
            public int Size => 1;
            public ItemStack Get(int i) => stack != null && menu.CanTake(menu.player) ? stack : null;
            public void Set(int i, ItemStack s) => stack = s != null && s.IsEmpty ? null : s;
            public int MaxStackSize => 64;
            public void SetChanged() { }
            public bool StillValid(Player p) => true;
        }

        /// <summary>Result slot whose take rule belongs to the menu instead of a crafting recipe.</summary>
        sealed class StationResultSlot : ResultSlot
        {
            readonly WorkstationMenu menu;
            public StationResultSlot(WorkstationMenu m, int x, int y) : base(m.output, m.inputs, x, y) { menu = m; }
            public override bool MayPickup(Player p) => menu.CanTake(p);
            public override void OnTake(Player p, ItemStack taken)
            {
                if (taken != null && !taken.IsEmpty) menu.TakeResult(p, taken);
            }
        }
    }

    // ============================================================================ furnace / blast furnace / smoker
    /// <summary>
    /// Furnace family: input, fuel and output over the entity's own three slots, so smelting keeps running with
    /// the menu closed. The entity's <c>kind</c> picks the recipe set.
    /// </summary>
    public class FurnaceMenu : ContainerMenu
    {
        public readonly FurnaceEntity furnace;
        public const int InputIdx = 0, FuelIdx = 1, OutputIdx = 2;
        /// <summary>Where the UI draws the flame (fills upward by <see cref="BurnProgress"/>) and the arrow (fills rightward by <see cref="CookProgress"/>).</summary>
        public static readonly RectInt FlameRect = new RectInt(56, 36, 14, 14), ArrowRect = new RectInt(79, 34, 24, 17);

        public FurnaceMenu(Player p, FurnaceEntity fe, string title)
        {
            player = p; furnace = fe; this.title = title; background = fe.kind;
            AddSlot(new Slot(fe, InputIdx, 56, 17));
            AddSlot(new Slot(fe, FuelIdx, 56, 53)).filter = FitsFuelSlot;
            AddSlot(new TakeOnlySlot(fe, OutputIdx, 116, 35));
            containerSlots = 3;
            AddPlayerSlots(p.inventory, 8, 84);
        }

        public float BurnProgress => furnace.BurnFrac;
        public float CookProgress => furnace.CookFrac;
        public bool Lit => furnace.IsLit;

        // an empty bucket may sit in the fuel slot because that is where a spent lava bucket ends up
        static bool FitsFuelSlot(ItemStack s) => Recipes.FuelValue(s.item) > 0 || s.item.id == "bucket";

        public override bool StillValid() => furnace.StillValid(player);

        /// <summary>The banked smelting experience is paid out once the player has emptied the output slot.</summary>
        public override void SlotChanged(Slot s)
        {
            if (s.menuIndex == OutputIdx && !s.HasItem) furnace.TakeXp(player);
        }

        protected override bool QuickMoveIntoContainer(ItemStack stack)
        {
            if (Recipes.FindSmelting(stack.item, furnace.kind) != null) return MoveItemTo(stack, InputIdx, InputIdx + 1, false);
            if (Recipes.FuelValue(stack.item) > 0) return MoveItemTo(stack, FuelIdx, FuelIdx + 1, false);
            return false;
        }
    }

    // ============================================================================ smithing table
    /// <summary>Smithing table: template + base + addition. The upgraded item keeps everything the base carried.</summary>
    public class SmithingMenu : WorkstationMenu
    {
        public readonly Int3 pos;
        public const int TemplateIdx = 0, BaseIdx = 1, AdditionIdx = 2, ResultIdx = 3;

        public SmithingMenu(Player p, Int3 pos) : base(p, 3)
        {
            this.pos = pos; title = "Upgrade Gear"; background = "smithing";
            var t = AddInput(TemplateIdx, 8, 48);
            t.filter = IsTemplate; t.emptyIcon = "empty_slot_smithing";
            AddInput(BaseIdx, 26, 48);
            AddInput(AdditionIdx, 44, 48);
            AddResult(98, 48);
            FinishLayout();
        }

        static bool IsTemplate(ItemStack s) => s.item.id.EndsWith("_smithing_template");

        SmithingRecipe Match()
        {
            var t = In(TemplateIdx); var b = In(BaseIdx); var a = In(AdditionIdx);
            if (t == null || b == null || a == null) return null;
            foreach (var r in Recipes.Smithing)
                if (r.template == t.item.id && r.baseItem == b.item.id && r.addition == a.item.id) return r;
            return null;
        }

        protected override void Recompute()
        {
            var r = Match();
            var it = r != null ? Items.Get(r.result) : null;
            if (it == null) { SetOutput(null); return; }
            // damage, enchantments, name and custom data all carry over to the upgraded item
            var outp = In(BaseIdx).CopyWithCount(1);
            outp.item = it;
            SetOutput(outp);
        }

        protected override void OnResultTaken(Player p, ItemStack taken)
        {
            ConsumeInput(TemplateIdx, 1);
            ConsumeInput(BaseIdx, 1);
            ConsumeInput(AdditionIdx, 1);
            Sounds.Play("block.smithing_table.use", pos.Center, 1f, 1f);
            Achievements.OnCraft(p, taken.item.id);
        }

        protected override bool QuickMoveIntoContainer(ItemStack stack)
        {
            if (IsTemplate(stack)) return MoveItemTo(stack, TemplateIdx, TemplateIdx + 1, false);
            foreach (var r in Recipes.Smithing)
            {
                if (r.baseItem == stack.item.id) return MoveItemTo(stack, BaseIdx, BaseIdx + 1, false);
                if (r.addition == stack.item.id) return MoveItemTo(stack, AdditionIdx, AdditionIdx + 1, false);
            }
            return false;
        }

        public override bool StillValid() => MenuUtil.AtBlock(player, pos, "smithing_table");
    }

    // ============================================================================ shulker box
    /// <summary>Shulker box: 27 slots laid out like a single chest. The lid's open count is released on close.</summary>
    public class ShulkerMenu : ContainerMenu
    {
        public readonly ShulkerBoxEntity box;
        public ShulkerMenu(Player p, ShulkerBoxEntity sb)
        {
            player = p; box = sb; background = "shulker_box"; height = 114 + 3 * 18;
            var b = p.world.GetBlock(sb.pos);
            title = sb.customName ?? (b is ShulkerBoxBlock ? b.displayName : "Shulker Box");
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 9; c++)
                    AddSlot(new Slot(sb, r * 9 + c, 8 + c * 18, 18 + r * 18)).filter = NotShulker;
            containerSlots = 27;
            AddPlayerSlots(p.inventory, 8, 18 + 3 * 18 + 14);
        }

        // boxes inside boxes would make storage recursive
        static bool NotShulker(ItemStack s) => !(s.item.block is ShulkerBoxBlock);

        public override bool StillValid() => box.StillValid(player);
        public override void Removed() { base.Removed(); box.Close(); }
    }

    // ============================================================================ anvil
    /// <summary>
    /// Anvil: slot 0 is the item being worked, slot 1 the sacrifice — a second copy of the item or an enchanted
    /// book to merge, a repair material, or nothing when only renaming. The pending action and its level cost are
    /// recomputed on every change and paid when the result is taken.
    /// </summary>
    public class AnvilMenu : WorkstationMenu
    {
        public readonly Int3 pos;
        /// <summary>Contents of the rename field. Kept here so it survives slot changes; applied on take.</summary>
        public string itemName;
        /// <summary>Level cost of the previewed action. Still set when <see cref="tooExpensive"/>, for the label.</summary>
        public int cost;
        public bool tooExpensive;
        int materialUsed;
        ItemStack nameSource;

        /// <summary>Anything costing this many levels or more is refused ("Too Expensive!").</summary>
        public const int CostCap = 40;
        public const int MaxNameLength = 50;
        const int RenameCost = 1;
        const float WearChance = 0.12f;

        public AnvilMenu(Player p, Int3 pos) : base(p, 2)
        {
            this.pos = pos; title = "Repair & Name"; background = "anvil";
            AddInput(0, 27, 47);
            AddInput(1, 76, 47);
            AddResult(134, 47);
            FinishLayout();
        }

        public override bool CanTake(Player p) => cost > 0 && !tooExpensive && (p.IsCreative || p.xpLevel >= cost);
        public bool CanAfford => player.IsCreative || player.xpLevel >= cost;

        /// <summary>Rename field hook. An empty name clears an existing custom name.</summary>
        public void SetItemName(string name)
        {
            itemName = CleanName(name);
            Recompute();
        }

        static string CleanName(string s)
        {
            if (s == null) return null;
            // control characters would corrupt the save format and section signs would inject formatting codes
            var sb = new StringBuilder(Math.Min(s.Length, MaxNameLength));
            foreach (char c in s)
            {
                if (c < ' ' || c == '\u007f' || c == '§') continue;
                if (sb.Length >= MaxNameLength) break;
                sb.Append(c);
            }
            return sb.ToString().Trim();
        }

        /// <summary>Repair items are item ids, except wooden gear and shields, which name the plank tag.</summary>
        static bool IsRepairMaterial(Item gear, Item mat)
        {
            string r = gear.repairItem;
            return r != null && (mat.id == r || Tags.Has(r, mat));
        }

        /// <summary>Levels per enchantment level moved: the rarer the enchantment (lower weight) the dearer.</summary>
        static int RarityCost(Enchant e) => e.weight >= 10 ? 1 : e.weight >= 5 ? 2 : e.weight >= 2 ? 4 : 8;

        protected override void Recompute()
        {
            var left = In(0); var right = In(1);
            // a different item in the left slot resets the field to that item's name, as the text box shows it
            if (left != nameSource) { nameSource = left; itemName = left?.DisplayName; }
            cost = 0; tooExpensive = false; materialUsed = 0;
            SetOutput(null);
            if (left == null) return;

            bool creative = player.IsCreative;
            var outp = left.Copy();
            int work = 0;

            if (right != null)
            {
                bool book = MenuUtil.IsEnchantedBook(right);
                if (left.item.IsDamageable && IsRepairMaterial(left.item, right.item))
                {
                    // each unit of material restores a quarter of the maximum durability
                    if (left.damage <= 0 || left.count > 1) return;
                    int quarter = Math.Max(1, left.item.maxDamage / 4);
                    int dmg = left.damage, used = 0;
                    while (dmg > 0 && used < right.count) { dmg = Math.Max(0, dmg - quarter); used++; }
                    outp.damage = dmg;
                    materialUsed = used;
                    work += used;
                }
                else
                {
                    // merging needs a second copy of the same item, or an enchanted book
                    if (left.count > 1 || left.item.id == "book") return;
                    if (!book && right.item != left.item) return;
                    if (!book && left.item.IsDamageable && left.damage > 0)
                    {
                        int max = left.item.maxDamage;
                        int remaining = (max - left.damage) + (max - right.damage) + max * 12 / 100;
                        int dmg = Math.Max(0, max - remaining);
                        if (dmg < outp.damage) { outp.damage = dmg; work += 2; }
                    }
                    var have = MenuUtil.EnchantsOf(outp);
                    bool applied = false, rejected = false;
                    foreach (var kv in MenuUtil.EnchantsOf(right))
                    {
                        var e = kv.Key;
                        int cur = have.TryGetValue(e, out int c) ? c : 0;
                        // two equal levels step up one; otherwise the better level wins
                        int level = cur == kv.Value ? kv.Value + 1 : Math.Max(cur, kv.Value);
                        bool fits = creative || MenuUtil.IsEnchantedBook(left) || e.CanApplyTo(left.item);
                        foreach (var other in have.Keys)
                            if (other != e && !e.CompatibleWith(other)) { fits = false; work++; }
                        if (!fits) { rejected = true; continue; }
                        applied = true;
                        level = Math.Min(level, e.maxLevel);
                        have[e] = level;
                        int weight = RarityCost(e);
                        if (book) weight = Math.Max(1, weight / 2);
                        work += weight * level;
                    }
                    if (rejected && !applied) return;
                    MenuUtil.SetEnchants(outp, have);
                }
            }

            bool renaming = false;
            if (itemName != null && itemName != left.DisplayName)
            {
                if (itemName.Length == 0) { if (left.customName != null) { outp.customName = null; renaming = true; } }
                else { outp.customName = itemName; renaming = true; }
            }
            if (renaming) work += RenameCost;
            if (work <= 0) return;

            cost = MenuUtil.PriorWork(left) + MenuUtil.PriorWork(right) + work;
            bool renameOnly = renaming && work == RenameCost;
            if (renameOnly && cost >= CostCap) cost = CostCap - 1;   // a plain rename is never refused
            if (cost >= CostCap && !creative) { tooExpensive = true; return; }
            // renaming alone leaves the penalty as it was; any real work doubles it for next time
            if (!renameOnly) MenuUtil.SetPriorWork(outp, Math.Max(MenuUtil.PriorWork(left), MenuUtil.PriorWork(right)) * 2 + 1);
            SetOutput(outp);
        }

        protected override void OnResultTaken(Player p, ItemStack taken)
        {
            ApplyName(taken);
            bool paid = !p.IsCreative;
            if (paid) p.AddLevels(-cost);
            ConsumeInput(0, inputs.items[0] != null ? inputs.items[0].count : 0);
            // a material repair only uses what it needed; any other sacrifice is used up whole
            ConsumeInput(1, materialUsed > 0 ? materialUsed : (inputs.items[1] != null ? inputs.items[1].count : 0));
            Wear(p, paid);
        }

        /// <summary>The rename lives in the menu; stamp it onto what the player actually takes.</summary>
        void ApplyName(ItemStack s)
        {
            if (itemName == null || nameSource == null || itemName == nameSource.DisplayName) return;
            if (itemName.Length > 0) s.customName = itemName;
            else s.customName = null;
        }

        /// <summary>
        /// A paid use has a 12% chance to wear the anvil one stage (anvil, chipped, damaged, gone) through the same
        /// <see cref="AnvilBlock.Damaged"/> chain a hard landing uses, so both paths agree on the progression.
        /// </summary>
        void Wear(Player p, bool paid)
        {
            var w = p.world;
            var anvil = w.GetBlock(pos) as AnvilBlock;
            if (paid && anvil != null && w.rand.NextFloat() < WearChance)
            {
                var next = anvil.Damaged();
                if (next == null)
                {
                    ushort old = w.GetState(pos);
                    w.SetState(pos, 0);
                    Particles.BlockBreak(w, pos, old);
                    Sounds.Play("block.anvil.destroy", pos.Center, 1f, 1f);
                    return;
                }
                w.SetState(pos, next.State(w.GetMeta(pos)));
            }
            Sounds.Play("block.anvil.use", pos.Center, 1f, 0.9f + w.rand.NextFloat() * 0.2f);
        }

        public override bool StillValid() => player.world.GetBlock(pos) is AnvilBlock && MenuUtil.Near(player, pos);
    }

    // ============================================================================ grindstone
    /// <summary>
    /// Grindstone: strips every enchantment except curses and pays some of their value back as experience. Two
    /// copies of the same gear are also fused into one with their durability combined.
    /// </summary>
    public class GrindstoneMenu : WorkstationMenu
    {
        public readonly Int3 pos;
        int xpValue;   // sum of the removed enchantments' minimum costs

        public GrindstoneMenu(Player p, Int3 pos) : base(p, 2)
        {
            this.pos = pos; title = "Repair & Disenchant"; background = "grindstone";
            AddInput(0, 49, 19).filter = Grindable;
            AddInput(1, 49, 40).filter = Grindable;
            AddResult(129, 34);
            FinishLayout();
        }

        static bool Grindable(ItemStack s) => s.item.IsDamageable || s.IsEnchanted || MenuUtil.IsEnchantedBook(s);

        /// <summary>Curses of <paramref name="s"/>; everything else is dropped and adds to the payout.</summary>
        Dictionary<Enchant, int> Strip(ItemStack s)
        {
            var kept = new Dictionary<Enchant, int>();
            foreach (var kv in MenuUtil.EnchantsOf(s))
            {
                if (kv.Key.curse) kept[kv.Key] = kv.Value;
                else xpValue += kv.Key.MinCost(kv.Value);
            }
            return kept;
        }

        protected override void Recompute()
        {
            xpValue = 0;
            SetOutput(null);
            var a = In(0); var b = In(1);
            if (a == null && b == null) return;
            ItemStack outp;
            Dictionary<Enchant, int> kept;
            if (a != null && b != null)
            {
                // a pair only fuses as two of the same damageable item
                if (a.item != b.item || !a.item.IsDamageable || a.count > 1 || b.count > 1) return;
                int max = a.item.maxDamage;
                int remaining = (max - a.damage) + (max - b.damage) + max * 5 / 100;
                outp = a.Copy();
                outp.damage = Math.Max(0, max - remaining);
                kept = Strip(a);
                foreach (var kv in Strip(b)) kept[kv.Key] = kept.TryGetValue(kv.Key, out int l) ? Math.Max(l, kv.Value) : kv.Value;
            }
            else
            {
                var only = a ?? b;
                outp = only.Copy();
                kept = Strip(only);
                // a lone item must actually lose something (an enchantment or its anvil history)
                if (xpValue == 0 && MenuUtil.PriorWork(only) == 0) return;
                // a book with nothing left on it is an ordinary book again
                if (MenuUtil.IsEnchantedBook(only) && kept.Count == 0) outp = new ItemStack("book", only.count) { customName = only.customName };
            }
            MenuUtil.SetEnchants(outp, kept);
            MenuUtil.SetPriorWork(outp, 0);
            SetOutput(outp);
        }

        protected override void OnResultTaken(Player p, ItemStack taken)
        {
            // pay between half and all of the removed enchantments' value
            int half = (xpValue + 1) / 2;
            int xp = half + (xpValue > half ? p.world.rand.Next(xpValue - half + 1) : 0);
            ConsumeInput(0, inputs.items[0] != null ? inputs.items[0].count : 0);
            ConsumeInput(1, inputs.items[1] != null ? inputs.items[1].count : 0);
            if (xp > 0) XpOrb.Spawn(p.world, pos.Center + Vector3.up * 0.5f, xp);
            Sounds.Play("block.grindstone.use", pos.Center, 1f, 1f);
        }

        protected override bool QuickMoveIntoContainer(ItemStack stack) => Grindable(stack) && MoveItemTo(stack, 0, 2, false);

        public override bool StillValid() => MenuUtil.AtBlock(player, pos, "grindstone");
    }

    // ============================================================================ stonecutter
    /// <summary>Stonecutter: one input and a list of cuts from <see cref="Recipes.CutsFor"/>; the UI picks one.</summary>
    public class StonecutterMenu : WorkstationMenu
    {
        public readonly Int3 pos;
        /// <summary>Index into <see cref="recipes"/>, or -1 while nothing is chosen.</summary>
        public int selectedRecipe = -1;
        /// <summary>Cuts available for the current input, in display order.</summary>
        public readonly List<StonecutterRecipe> recipes = new List<StonecutterRecipe>();
        Item listFor;

        public StonecutterMenu(Player p, Int3 pos) : base(p, 1)
        {
            this.pos = pos; title = "Stonecutter"; background = "stonecutter";
            AddInput(0, 20, 33);
            AddResult(143, 33);
            FinishLayout();
        }

        /// <summary>Called by the recipe buttons. Out-of-range indices clear the selection.</summary>
        public void SelectRecipe(int index)
        {
            selectedRecipe = index >= 0 && index < recipes.Count ? index : -1;
            Recompute();
        }

        protected override void Recompute()
        {
            var inp = In(0);
            var it = inp?.item;
            if (it != listFor)
            {
                // a different input means a different list, so the old choice no longer applies
                listFor = it;
                recipes.Clear();
                selectedRecipe = -1;
                if (it != null) recipes.AddRange(Recipes.CutsFor(it));
            }
            bool valid = it != null && selectedRecipe >= 0 && selectedRecipe < recipes.Count;
            SetOutput(valid ? recipes[selectedRecipe].result.Copy() : null);
        }

        protected override void OnResultTaken(Player p, ItemStack taken)
        {
            ConsumeInput(0, 1);
            Sounds.Play("ui.stonecutter.take_result", pos.Center, 1f, 1f);
            Achievements.OnCraft(p, taken.item.id);
        }

        static bool HasCuts(Item it)
        {
            foreach (var r in Recipes.Stonecutting) if (r.input == it) return true;
            return false;
        }

        protected override bool QuickMoveIntoContainer(ItemStack stack) => HasCuts(stack.item) && MoveItemTo(stack, 0, 1, false);

        public override bool StillValid() => MenuUtil.AtBlock(player, pos, "stonecutter");
    }

    // ============================================================================ loom
    /// <summary>
    /// Loom: banner + dye (+ an optional pattern item for the special designs). The result is a copy of the
    /// banner with one more layer appended to <c>data["patterns"]</c> as <c>pattern:color</c>, comma separated.
    /// </summary>
    public class LoomMenu : WorkstationMenu
    {
        public readonly Int3 pos;
        /// <summary>Index into <see cref="patterns"/>, or -1 while nothing is chosen.</summary>
        public int selectedPattern = -1;
        /// <summary>Designs the pattern grid offers for the current inputs.</summary>
        public readonly List<string> patterns = new List<string>();
        public const int BannerIdx = 0, DyeIdx = 1, PatternIdx = 2, ResultIdx = 3;
        /// <summary>Banners hold at most this many layers.</summary>
        public const int MaxLayers = 6;

        static readonly string[] BasicPatterns =
        {
            "stripe_bottom", "stripe_top", "stripe_left", "stripe_right", "stripe_center", "stripe_middle",
            "stripe_downright", "stripe_downleft", "small_stripes", "cross", "straight_cross", "diagonal_left",
            "diagonal_right", "diagonal_up_left", "diagonal_up_right", "half_vertical", "half_vertical_right",
            "half_horizontal", "half_horizontal_bottom", "square_bottom_left", "square_bottom_right", "square_top_left",
            "square_top_right", "triangle_bottom", "triangle_top", "triangles_bottom", "triangles_top", "circle",
            "rhombus", "border", "curly_border", "bricks", "gradient", "gradient_up",
        };

        bool listOpen;
        Item listItem;

        public LoomMenu(Player p, Int3 pos) : base(p, 3)
        {
            this.pos = pos; title = "Loom"; background = "loom";
            AddInput(BannerIdx, 13, 26).filter = IsBanner;
            AddInput(DyeIdx, 33, 26).filter = s => MenuUtil.DyeColor(s.item.id) != null;
            AddInput(PatternIdx, 23, 45).filter = s => PatternOf(s.item) != null;
            AddResult(143, 58);
            FinishLayout();
        }

        static bool IsBanner(ItemStack s) => s.item.id.EndsWith("_banner");

        /// <summary>The design a banner-pattern item unlocks ("creeper_banner_pattern" -> "creeper").</summary>
        static string PatternOf(Item it)
        {
            const string suffix = "_banner_pattern";
            return it != null && it.id.EndsWith(suffix) ? it.id.Substring(0, it.id.Length - suffix.Length) : null;
        }

        static int LayerCount(string layers)
        {
            if (string.IsNullOrEmpty(layers)) return 0;
            int n = 1;
            foreach (char c in layers) if (c == ',') n++;
            return n;
        }

        /// <summary>Called by the pattern grid. Out-of-range indices clear the selection.</summary>
        public void SelectPattern(int index)
        {
            selectedPattern = index >= 0 && index < patterns.Count ? index : -1;
            Recompute();
        }

        /// <summary>The grid lists the basic designs once a banner and dye are in, or only the pattern item's design.</summary>
        void RefreshPatterns(bool open, Item patternItem)
        {
            if (open == listOpen && patternItem == listItem) return;
            string keep = selectedPattern >= 0 && selectedPattern < patterns.Count ? patterns[selectedPattern] : null;
            listOpen = open; listItem = patternItem;
            patterns.Clear();
            if (open)
            {
                string special = PatternOf(patternItem);
                if (special != null) patterns.Add(special); else patterns.AddRange(BasicPatterns);
            }
            selectedPattern = keep != null ? patterns.IndexOf(keep) : -1;
        }

        protected override void Recompute()
        {
            var banner = In(BannerIdx); var dye = In(DyeIdx); var pat = In(PatternIdx);
            RefreshPatterns(banner != null && dye != null, pat?.item);
            SetOutput(null);
            if (banner == null || dye == null || selectedPattern < 0) return;
            string layers = banner.Get("patterns");
            if (LayerCount(layers) >= MaxLayers) return;
            var outp = banner.CopyWithCount(1);
            string layer = patterns[selectedPattern] + ":" + MenuUtil.DyeColor(dye.item.id);
            outp.Set("patterns", string.IsNullOrEmpty(layers) ? layer : layers + "," + layer);
            SetOutput(outp);
        }

        protected override void OnResultTaken(Player p, ItemStack taken)
        {
            // the pattern item is a stencil and is kept
            ConsumeInput(BannerIdx, 1);
            ConsumeInput(DyeIdx, 1);
            Sounds.Play("ui.loom.take_result", pos.Center, 1f, 1f);
            Achievements.OnCraft(p, taken.item.id);
        }

        protected override bool QuickMoveIntoContainer(ItemStack stack)
        {
            if (IsBanner(stack)) return MoveItemTo(stack, BannerIdx, BannerIdx + 1, false);
            if (MenuUtil.DyeColor(stack.item.id) != null) return MoveItemTo(stack, DyeIdx, DyeIdx + 1, false);
            if (PatternOf(stack.item) != null) return MoveItemTo(stack, PatternIdx, PatternIdx + 1, false);
            return false;
        }

        public override bool StillValid() => MenuUtil.AtBlock(player, pos, "loom");
    }

    // ============================================================================ enchanting table
    /// <summary>
    /// Enchanting table: slot 0 holds the item, slot 1 the lapis. Up to three offers are rolled from the nearby
    /// bookshelves and the player's enchantment seed, so reopening the table shows the same numbers until an
    /// enchant goes through. <see cref="ClickButton"/> takes one.
    /// </summary>
    public class EnchantMenu : ContainerMenu
    {
        /// <summary>One button: the hinted enchantment and level, the level requirement, and its button index (0..2).</summary>
        public sealed class EnchantOffer
        {
            public Enchant enchant;
            public int level;
            public int cost;
            public int id;
            /// <summary>Lapis (and levels) actually spent: one per button position.</summary>
            public int Price => id + 1;
        }

        public readonly Int3 pos;
        /// <summary>Offers currently shown; each knows its button through <see cref="EnchantOffer.id"/>.</summary>
        public readonly List<EnchantOffer> offers = new List<EnchantOffer>(3);
        /// <summary>Bookshelves powering the table (0..15), counted when the menu opens.</summary>
        public readonly int bookshelves;
        public const int ItemIdx = 0, LapisIdx = 1;

        readonly SimpleContainer table = new SimpleContainer(2);
        readonly EnchantOffer[] pool = { new EnchantOffer(), new EnchantOffer(), new EnchantOffer() };
        readonly List<Int3> shelves = new List<Int3>();
        ItemStack rolledFor;

        public EnchantMenu(Player p, Int3 pos)
        {
            player = p; this.pos = pos; title = "Enchant"; background = "enchanting_table";
            AddSlot(new Slot(table, ItemIdx, 15, 47)).maxStack = 1;
            AddSlot(new Slot(table, LapisIdx, 35, 47)).filter = s => s.item.id == "lapis_lazuli";
            containerSlots = 2;
            AddPlayerSlots(p.inventory, 8, 84);
            bookshelves = CountBookshelves(p.world, pos, shelves);
            table.onChanged = OnSlotsChanged;
            RollOffers();
        }

        /// <summary>
        /// Bookshelves in the ring two blocks out from the table, at its height and one above, with a clear block
        /// between shelf and table. Capped at 15, which already unlocks the top cost of 30.
        /// </summary>
        public static int CountBookshelves(World w, Int3 pos, List<Int3> found = null)
        {
            int n = 0;
            for (int dz = -2; dz <= 2; dz++)
                for (int dx = -2; dx <= 2; dx++)
                {
                    if (Math.Abs(dx) < 2 && Math.Abs(dz) < 2) continue;
                    for (int dy = 0; dy <= 1; dy++)
                    {
                        var gap = w.GetBlock(pos.Offset(dx / 2, dy, dz / 2));
                        if (!gap.isAir && !(gap.replaceable && !gap.isLiquid)) continue;
                        var at = pos.Offset(dx, dy, dz);
                        if (w.GetBlock(at).id != "bookshelf") continue;
                        n++;
                        found?.Add(at);
                    }
                }
            return Math.Min(15, n);
        }

        static bool Enchantable(ItemStack s) =>
            s != null && !s.IsEmpty && !s.IsEnchanted && (s.item.id == "book" || s.item.enchantability > 0);

        public bool CanAfford(EnchantOffer o)
        {
            if (player.IsCreative) return true;
            var lapis = table.items[LapisIdx];
            return player.xpLevel >= o.cost && lapis != null && lapis.count >= o.Price;
        }

        void OnSlotsChanged()
        {
            // only a different item re-rolls; topping up the lapis keeps the offers
            if (table.items[ItemIdx] != rolledFor) RollOffers();
        }

        void RollOffers()
        {
            offers.Clear();
            var item = table.items[ItemIdx];
            rolledFor = item;
            if (!Enchantable(item)) return;
            var rng = new RNG(player.xpSeed, bookshelves, 0, 0x454E43);
            int roll = rng.Range(1, 8) + bookshelves / 2 + rng.Range(0, bookshelves);
            TryOffer(item, 0, Math.Max(roll / 3, 1));
            TryOffer(item, 1, roll * 2 / 3 + 1);
            TryOffer(item, 2, Math.Max(roll, bookshelves * 2));
        }

        void TryOffer(ItemStack item, int id, int cost)
        {
            if (cost < id + 1) return;   // a button never asks for fewer levels than it burns
            var list = Roll(item, id, cost);
            if (list.Count == 0) return;
            var o = pool[id];
            o.id = id; o.cost = cost; o.enchant = list[0].e; o.level = list[0].lvl;
            offers.Add(o);
        }

        /// <summary>The hint and the real enchant share this seed, so the button shows what the item gets.</summary>
        List<(Enchant e, int lvl)> Roll(ItemStack item, int id, int cost)
        {
            var rng = new RNG(player.xpSeed, id, cost, item.item.index);
            var list = Enchant.Select(ref rng, item.item, cost, false);
            // books come out one enchantment lighter so they are not strictly better than gear
            if (item.item.id == "book" && list.Count > 1) list.RemoveAt(rng.Next(list.Count));
            return list;
        }

        /// <summary>Takes offer <paramref name="id"/> (0..2). Returns false when it cannot be paid for.</summary>
        public bool ClickButton(int id, int button)
        {
            EnchantOffer o = null;
            foreach (var x in offers) if (x.id == id) { o = x; break; }
            var item = table.items[ItemIdx];
            if (o == null || !Enchantable(item) || !CanAfford(o)) return false;
            var list = Roll(item, id, o.cost);
            if (list.Count == 0) return false;

            if (item.item.id == "book")
            {
                var book = new ItemStack("enchanted_book", 1) { customName = item.customName };
                var stored = new Dictionary<Enchant, int>();
                foreach (var (e, l) in list) stored[e] = l;
                MenuUtil.SetEnchants(book, stored);
                table.items[ItemIdx] = book;
            }
            else foreach (var (e, l) in list) item.AddEnchant(e, l);

            var w = player.world;
            if (!player.IsCreative)
            {
                player.AddLevels(-o.Price);
                var lapis = table.items[LapisIdx];
                lapis.count -= o.Price;
                if (lapis.count <= 0) table.items[LapisIdx] = null;
            }
            player.xpSeed = w.rand.NextInt();   // a fresh seed, so the next item gets new offers
            Sounds.Play("block.enchantment_table.use", pos.Center, 1f, 0.9f + w.rand.NextFloat() * 0.1f);
            SpawnGlyphs(w);
            RollOffers();
            return true;
        }

        /// <summary>Glyphs stream from each powering shelf to the book (from the air around it with no shelves).</summary>
        void SpawnGlyphs(World w)
        {
            Vector3 to = pos.Center + Vector3.up * 0.8f;
            if (shelves.Count == 0)
            {
                for (int i = 0; i < 12; i++)
                    Particles.Enchant(w, to + new Vector3(w.rand.Range(-1.5f, 1.5f), w.rand.Range(0.2f, 1.2f), w.rand.Range(-1.5f, 1.5f)), to);
                return;
            }
            foreach (var s in shelves)
                for (int i = 0; i < 3; i++)
                    Particles.Enchant(w, s.Center + new Vector3(w.rand.Range(-0.3f, 0.3f), 0.6f, w.rand.Range(-0.3f, 0.3f)), to);
        }

        protected override bool QuickMoveIntoContainer(ItemStack stack)
        {
            if (stack.item.id == "lapis_lazuli") return MoveItemTo(stack, LapisIdx, LapisIdx + 1, false);
            // one item at a time, and only onto an empty table
            return !slots[ItemIdx].HasItem && MoveItemTo(stack, ItemIdx, ItemIdx + 1, false);
        }

        public override bool StillValid() => MenuUtil.AtBlock(player, pos, "enchanting_table");

        public override void Removed()
        {
            base.Removed();
            for (int i = 0; i < table.Size; i++)
                if (table.items[i] != null) { player.inventory.AddOrDrop(table.items[i]); table.items[i] = null; }
        }
    }

    // ============================================================================ brewing stand
    /// <summary>Brewing stand: three bottles, one ingredient, blaze powder fuel, all living on the entity.</summary>
    public class BrewingMenu : ContainerMenu
    {
        public readonly BrewingStandEntity stand;
        public const int IngredientIdx = 3, FuelIdx = 4;
        // mirror the numbers BrewingStandEntity.Tick uses
        const int BrewTicks = 400, FuelPerPowder = 20;

        public BrewingMenu(Player p, BrewingStandEntity be)
        {
            player = p; stand = be; title = "Brewing Stand"; background = "brewing_stand";
            AddBottle(be, 0, 56, 51);
            AddBottle(be, 1, 79, 58);
            AddBottle(be, 2, 102, 51);
            AddSlot(new Slot(be, IngredientIdx, 79, 17)).filter = s => Brewing.IsIngredient(s.item);
            AddSlot(new Slot(be, FuelIdx, 17, 17)).filter = s => s.item.id == "blaze_powder";
            containerSlots = 5;
            AddPlayerSlots(p.inventory, 8, 84);
        }

        void AddBottle(BrewingStandEntity be, int i, int x, int y)
        {
            var s = AddSlot(new Slot(be, i, x, y));
            s.filter = IsBottle;
            s.maxStack = 1;
        }

        static bool IsBottle(ItemStack s)
        {
            string id = s.item.id;
            return id == "potion" || id == "splash_potion" || id == "lingering_potion" || id == "glass_bottle";
        }

        public bool IsBrewing => stand.brewTime > 0;
        /// <summary>0..1 along the brew arrow.</summary>
        public float BrewProgress => stand.brewTime > 0 ? 1f - stand.brewTime / (float)BrewTicks : 0f;
        /// <summary>0..1 of the fuel bar (one blaze powder fills it).</summary>
        public float FuelLevel => Mathf.Clamp01(stand.fuel / (float)FuelPerPowder);
        /// <summary>Bubble animation frame 0..6, cycling while a brew runs.</summary>
        public int BubbleFrame => stand.brewTime > 0 ? (stand.brewTime / 2) % 7 : 0;

        public override bool StillValid() => stand.StillValid(player);

        protected override bool QuickMoveIntoContainer(ItemStack stack)
        {
            // blaze powder fuels first and only then counts as an ingredient
            if (stack.item.id == "blaze_powder" && MoveItemTo(stack, FuelIdx, FuelIdx + 1, false)) return true;
            if (Brewing.IsIngredient(stack.item)) return MoveItemTo(stack, IngredientIdx, IngredientIdx + 1, false);
            if (IsBottle(stack)) return MoveItemTo(stack, 0, 3, false);
            return false;
        }
    }

    // ============================================================================ beacon
    /// <summary>
    /// Beacon: one payment slot and the power buttons. Choices are staged in the menu and written to the entity
    /// by the confirm button, which spends the payment; <see cref="BeaconEntity.Tick"/> keeps them applied.
    /// </summary>
    public class BeaconMenu : ContainerMenu
    {
        public readonly BeaconEntity beacon;
        /// <summary>Pyramid tiers under the beacon (0..4), measured when the menu opens.</summary>
        public readonly int levels;
        /// <summary>Staged choices shown by the UI; null means none.</summary>
        public string primary, secondary;

        /// <summary>Row of <see cref="Beacon.PowersByLevel"/> offered as the secondary power.</summary>
        public const int SecondaryTier = 3;
        /// <summary>Button index on the secondary row meaning "the primary at level II".</summary>
        public const int UpgradePrimary = -1;
        /// <summary>Button ids for the two action buttons.</summary>
        public const int Confirm = 100, Cancel = 101;

        static readonly string[] PaymentItems = { "iron_ingot", "gold_ingot", "diamond", "emerald", "netherite_ingot" };
        readonly SimpleContainer payment = new SimpleContainer(1);

        public BeaconMenu(Player p, BeaconEntity be)
        {
            player = p; beacon = be; title = "Beacon"; background = "beacon"; width = 230; height = 219;
            var pay = AddSlot(new Slot(payment, 0, 136, 110));
            pay.filter = s => Array.IndexOf(PaymentItems, s.item.id) >= 0;
            pay.maxStack = 1;
            containerSlots = 1;
            AddPlayerSlots(p.inventory, 36, 137);
            levels = Beacon.ComputeLevels(p.world, be.pos);
            primary = be.primary;
            secondary = be.secondary;
        }

        public bool HasPayment => payment.items[0] != null && !payment.items[0].IsEmpty;
        public bool CanConfirm => HasPayment && primary != null && TierOf(primary) < levels && TierOf(primary) < SecondaryTier;

        /// <summary>Row of <see cref="Beacon.PowersByLevel"/> a power sits in, or -1.</summary>
        public static int TierOf(string power)
        {
            var rows = Beacon.PowersByLevel;
            for (int t = 0; t < rows.Length; t++)
                if (Array.IndexOf(rows[t], power) >= 0) return t;
            return -1;
        }

        /// <summary>
        /// <paramref name="id"/> is a row of <see cref="Beacon.PowersByLevel"/> and <paramref name="button"/> the entry
        /// in it: rows below <see cref="SecondaryTier"/> stage the primary, that row stages the secondary (use
        /// <see cref="UpgradePrimary"/> for primary II). <see cref="Confirm"/> and <see cref="Cancel"/> are the
        /// action buttons. Returns true when something changed.
        /// </summary>
        public bool ClickButton(int id, int button)
        {
            if (id == Confirm) return Apply();
            if (id == Cancel) { primary = beacon.primary; secondary = beacon.secondary; return true; }
            var rows = Beacon.PowersByLevel;
            if (id < 0 || id >= rows.Length || id >= levels) return false;
            if (id < SecondaryTier)
            {
                if (button < 0 || button >= rows[id].Length) return false;
                // a staged "primary II" follows the primary it upgrades
                if (secondary != null && secondary == primary) secondary = rows[id][button];
                primary = rows[id][button];
                return true;
            }
            if (button == UpgradePrimary) { if (primary == null) return false; secondary = primary; return true; }
            if (button < 0 || button >= rows[id].Length) return false;
            secondary = rows[id][button];
            return true;
        }

        bool Apply()
        {
            if (!CanConfirm) return false;
            beacon.primary = primary;
            beacon.secondary = levels > SecondaryTier ? secondary : null;
            beacon.levels = levels;
            var pay = payment.items[0];
            pay.count--;
            if (pay.count <= 0) payment.items[0] = null;
            ApplyNow();
            Sounds.Play("block.beacon.power_select", beacon.pos.Center, 1f, 1f);
            return true;
        }

        /// <summary>
        /// Paying applies the effect straight away (same range, duration and amplifier as the entity's tick) rather
        /// than on the entity's next 4-second pulse.
        /// </summary>
        void ApplyNow()
        {
            var w = beacon.world;
            if (w == null || levels <= 0 || beacon.primary == null || !w.CanSeeSky(beacon.pos.Offset(Dir.Up))) return;
            float range = levels * 10 + 10;
            int duration = (9 + levels * 2) * 20;
            var eff = Effect.Get(beacon.primary);
            int amp = beacon.secondary == beacon.primary ? 1 : 0;
            foreach (var e in w.entities)
            {
                if (!(e is Player p) || (p.position - beacon.pos.Center).sqrMagnitude >= range * range) continue;
                if (eff != null) p.AddEffect(new EffectInstance(eff, duration, amp, true));
                if (beacon.secondary == "regeneration") p.AddEffect(new EffectInstance(Effect.Regeneration, duration, 0, true));
            }
        }

        public override bool StillValid() =>
            beacon.world != null && beacon.world.GetBlockEntity(beacon.pos) == beacon && MenuUtil.Near(player, beacon.pos);

        public override void Removed()
        {
            base.Removed();
            if (payment.items[0] != null) { player.inventory.AddOrDrop(payment.items[0]); payment.items[0] = null; }
        }
    }

    // ============================================================================ hopper / dispenser
    /// <summary>
    /// Hopper: five slots. Opened for a hopper block entity and for a hopper minecart, so it only relies on
    /// <see cref="IContainer"/> and lets the container decide whether it is still valid.
    /// </summary>
    public class HopperMenu : ContainerMenu
    {
        public readonly IContainer container;
        public HopperMenu(Player p, IContainer c)
        {
            player = p; container = c; background = "hopper"; height = 133;
            title = c is ContainerEntity ce && ce.customName != null ? ce.customName : c is Minecart ? "Minecart with Hopper" : "Item Hopper";
            int n = Math.Min(5, c.Size);
            for (int i = 0; i < n; i++) AddSlot(new Slot(c, i, 44 + i * 18, 20));
            containerSlots = n;
            AddPlayerSlots(p.inventory, 8, 51);
        }
        public override bool StillValid() => container.StillValid(player);
    }

    /// <summary>Dispenser and dropper: a 3x3 grid over the entity's nine slots.</summary>
    public class DispenserMenu : ContainerMenu
    {
        public readonly DispenserEntity entity;
        public DispenserMenu(Player p, DispenserEntity de, string title)
        {
            player = p; entity = de; this.title = de.customName ?? title; background = "dispenser";
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3; c++)
                    AddSlot(new Slot(de, r * 3 + c, 62 + c * 18, 17 + r * 18));
            containerSlots = 9;
            AddPlayerSlots(p.inventory, 8, 84);
        }
        public override bool StillValid() => entity.StillValid(player);
    }
}
