using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public interface IContainer
    {
        int Size { get; }
        ItemStack Get(int i);
        void Set(int i, ItemStack s);
        int MaxStackSize { get; }
        void SetChanged();
        bool StillValid(Player p);
    }

    public class SimpleContainer : IContainer
    {
        public ItemStack[] items;
        public Action onChanged;
        public SimpleContainer(int size) { items = new ItemStack[size]; }
        public int Size => items.Length;
        public ItemStack Get(int i) => items[i];
        public void Set(int i, ItemStack s) { items[i] = s != null && s.IsEmpty ? null : s; SetChanged(); }
        public virtual int MaxStackSize => 64;
        public virtual void SetChanged() => onChanged?.Invoke();
        public virtual bool StillValid(Player p) => true;
        public void Clear() { for (int i = 0; i < items.Length; i++) items[i] = null; SetChanged(); }
        public bool IsEmpty() { foreach (var s in items) if (s != null && !s.IsEmpty) return false; return true; }

        /// <summary>Insert as much as possible; returns leftover (or null).</summary>
        public ItemStack AddItem(ItemStack stack)
        {
            if (stack == null || stack.IsEmpty) return null;
            for (int i = 0; i < items.Length && stack.count > 0; i++)
            {
                var s = items[i];
                if (s != null && s.Stackable(stack))
                {
                    int move = Math.Min(stack.count, Math.Min(s.MaxStack, MaxStackSize) - s.count);
                    if (move > 0) { s.count += move; stack.count -= move; }
                }
            }
            for (int i = 0; i < items.Length && stack.count > 0; i++)
            {
                if (items[i] == null || items[i].IsEmpty) { items[i] = stack.CopyWithCount(Math.Min(stack.count, Math.Min(stack.MaxStack, MaxStackSize))); stack.count -= items[i].count; }
            }
            SetChanged();
            return stack.count > 0 ? stack : null;
        }

        public string Serialize()
        {
            var parts = new string[items.Length];
            for (int i = 0; i < items.Length; i++) parts[i] = items[i]?.Serialize() ?? "";
            return string.Join(";", parts);
        }
        public void Deserialize(string s)
        {
            if (string.IsNullOrEmpty(s)) return;
            var parts = s.Split(';');
            for (int i = 0; i < items.Length && i < parts.Length; i++) items[i] = ItemStack.Deserialize(parts[i]);
        }

        /// <summary>Redstone comparator signal (0..15) for a container.</summary>
        public static int ComparatorSignal(IContainer c)
        {
            if (c == null) return 0;
            float f = 0; int n = 0;
            for (int i = 0; i < c.Size; i++)
            {
                var s = c.Get(i);
                if (s != null && !s.IsEmpty) { f += s.count / (float)Math.Min(c.MaxStackSize, s.MaxStack); n++; }
            }
            f /= c.Size;
            return Mathf.FloorToInt(f * 14f) + (n > 0 ? 1 : 0);
        }
    }

    /// <summary>Player inventory: 0-8 hotbar, 9-35 main, armor[4] (feet,legs,chest,head), offhand.</summary>
    public class PlayerInventory : IContainer
    {
        public readonly ItemStack[] main = new ItemStack[36];
        public readonly ItemStack[] armor = new ItemStack[4];
        public ItemStack offhand;
        public int selected;
        public Player player;
        public int Size => 41;
        public int MaxStackSize => 64;
        public PlayerInventory(Player p) { player = p; }

        public ItemStack Get(int i)
        {
            if (i < 36) return main[i];
            if (i < 40) return armor[i - 36];
            return offhand;
        }
        public void Set(int i, ItemStack s)
        {
            if (s != null && s.IsEmpty) s = null;
            if (i < 36) main[i] = s; else if (i < 40) armor[i - 36] = s; else offhand = s;
        }
        public void SetChanged() { }
        public bool StillValid(Player p) => true;
        public ItemStack Selected => main[selected];
        public void SetSelected(ItemStack s) => main[selected] = s != null && s.IsEmpty ? null : s;

        public void Cleanup()
        {
            for (int i = 0; i < 36; i++) if (main[i] != null && main[i].IsEmpty) main[i] = null;
            for (int i = 0; i < 4; i++) if (armor[i] != null && armor[i].IsEmpty) armor[i] = null;
            if (offhand != null && offhand.IsEmpty) offhand = null;
        }

        public int FindSlot(Item it)
        {
            for (int i = 0; i < 36; i++) if (main[i] != null && main[i].item == it) return i;
            return -1;
        }
        public bool Contains(string id)
        {
            var it = Items.Get(id);
            if (it == null) return false;
            if (FindSlot(it) >= 0) return true;
            return offhand != null && offhand.item == it;
        }
        public int Count(Item it)
        {
            int n = 0;
            for (int i = 0; i < 36; i++) if (main[i] != null && main[i].item == it) n += main[i].count;
            if (offhand != null && offhand.item == it) n += offhand.count;
            return n;
        }
        public bool Remove(Item it, int count)
        {
            if (Count(it) < count) return false;
            if (offhand != null && offhand.item == it) { int t = Math.Min(count, offhand.count); offhand.count -= t; count -= t; if (offhand.count <= 0) offhand = null; }
            for (int i = 35; i >= 0 && count > 0; i--)
            {
                if (main[i] == null || main[i].item != it) continue;
                int t = Math.Min(count, main[i].count);
                main[i].count -= t; count -= t;
                if (main[i].count <= 0) main[i] = null;
            }
            return true;
        }

        /// <summary>Adds the stack into the inventory (hotbar first like MC). Returns true if fully added. Modifies stack.count.</summary>
        public bool Add(ItemStack stack)
        {
            if (stack == null || stack.IsEmpty) return true;
            // merge into existing stacks (selected slot, offhand, then others)
            if (main[selected] != null && main[selected].Stackable(stack)) Merge(ref main[selected], stack);
            if (offhand != null && offhand.Stackable(stack)) Merge(ref offhand, stack);
            for (int i = 0; i < 36 && stack.count > 0; i++) if (main[i] != null && main[i].Stackable(stack)) Merge(ref main[i], stack);
            for (int i = 0; i < 36 && stack.count > 0; i++)
            {
                if (main[i] == null)
                {
                    int n = Math.Min(stack.count, stack.MaxStack);
                    main[i] = stack.CopyWithCount(n);
                    stack.count -= n;
                }
            }
            return stack.count <= 0;
        }
        static void Merge(ref ItemStack into, ItemStack from)
        {
            int n = Math.Min(from.count, into.MaxStack - into.count);
            if (n <= 0) return;
            into.count += n; from.count -= n;
        }
        public void AddOrDrop(ItemStack stack)
        {
            if (!Add(stack) && player != null && player.world != null) player.DropItem(stack, false);
        }

        public int ArmorValue
        {
            get
            {
                int v = 0;
                foreach (var a in armor) if (a != null && a.item is ArmorItem ai) v += ai.defense;
                return v;
            }
        }
        public float ArmorToughness
        {
            get
            {
                float v = 0;
                foreach (var a in armor) if (a != null && a.item is ArmorItem ai) v += ai.toughness;
                return v;
            }
        }
        public int FirstEmpty() { for (int i = 0; i < 36; i++) if (main[i] == null) return i; return -1; }
        public void Clear() { for (int i = 0; i < 36; i++) main[i] = null; for (int i = 0; i < 4; i++) armor[i] = null; offhand = null; }
        public bool IsEmpty() { for (int i = 0; i < 41; i++) if (Get(i) != null) return false; return true; }

        public string Serialize()
        {
            var parts = new string[41];
            for (int i = 0; i < 41; i++) parts[i] = Get(i)?.Serialize() ?? "";
            return string.Join(";", parts) + "#" + selected;
        }
        public void Deserialize(string s)
        {
            if (string.IsNullOrEmpty(s)) return;
            int hash = s.LastIndexOf('#');
            if (hash >= 0) { int.TryParse(s.Substring(hash + 1), out selected); s = s.Substring(0, hash); }
            var parts = s.Split(';');
            for (int i = 0; i < 41 && i < parts.Length; i++) Set(i, ItemStack.Deserialize(parts[i]));
        }
    }

    // ============================================================================ Slots & menus
    public class Slot
    {
        public IContainer container;
        public int index;
        public int x, y;
        public int menuIndex;
        public Func<ItemStack, bool> filter;
        public int maxStack = 64;
        public bool active = true;
        public bool isResult;
        public string emptyIcon; // background sprite (armor silhouettes etc.)
        public Slot(IContainer c, int index, int x, int y) { container = c; this.index = index; this.x = x; this.y = y; }
        public ItemStack Item { get => container.Get(index); set { container.Set(index, value); container.SetChanged(); } }
        public bool HasItem => Item != null && !Item.IsEmpty;
        public virtual bool MayPlace(ItemStack s) => !isResult && (filter == null || filter(s));
        public virtual bool MayPickup(Player p) => true;
        public virtual int MaxStackFor(ItemStack s) => Math.Min(maxStack, Math.Min(container.MaxStackSize, s?.MaxStack ?? 64));
        public virtual void OnTake(Player p, ItemStack taken) { }
        public ItemStack Remove(int n)
        {
            var s = Item;
            if (s == null) return null;
            var r = s.Split(n);
            if (s.count <= 0) Item = null; else container.SetChanged();
            return r;
        }
    }

    public enum ClickType { Pickup, QuickMove, Swap, Clone, Throw, QuickCraft, PickupAll }

    public abstract class Menu
    {
        public readonly List<Slot> slots = new List<Slot>();
        public ItemStack carried;
        public Player player;
        public string title = "";
        public int width = 176, height = 166;
        public string background = "inventory";
        // drag (quick craft) state
        public int dragButton = -1;
        public readonly List<Slot> dragSlots = new List<Slot>();

        protected Slot AddSlot(Slot s) { s.menuIndex = slots.Count; slots.Add(s); return s; }

        protected void AddPlayerSlots(PlayerInventory inv, int x0, int y0)
        {
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 9; c++)
                    AddSlot(new Slot(inv, 9 + r * 9 + c, x0 + c * 18, y0 + r * 18));
            for (int c = 0; c < 9; c++) AddSlot(new Slot(inv, c, x0 + c * 18, y0 + 58));
        }

        public virtual bool StillValid() => true;
        public virtual void Removed()
        {
            if (carried != null && !carried.IsEmpty) { player.inventory.AddOrDrop(carried); carried = null; }
        }
        public virtual void SlotChanged(Slot s) { }
        public virtual void Tick() { }

        /// <summary>Shift-click behaviour: move the stack in slot to the "other" section. Return leftover-copy or null.</summary>
        public abstract ItemStack QuickMove(int slotIndex);

        /// <summary>Try to move a stack into slots [start,end). reverse = fill from end.</summary>
        protected bool MoveItemTo(ItemStack stack, int start, int end, bool reverse)
        {
            bool moved = false;
            if (stack.MaxStack > 1)
            {
                for (int k = 0; k < end - start && stack.count > 0; k++)
                {
                    int i = reverse ? end - 1 - k : start + k;
                    var sl = slots[i];
                    var cur = sl.Item;
                    if (cur != null && cur.Stackable(stack) && sl.MayPlace(stack))
                    {
                        int max = sl.MaxStackFor(stack);
                        int n = Math.Min(stack.count, max - cur.count);
                        if (n > 0) { cur.count += n; stack.count -= n; sl.container.SetChanged(); moved = true; }
                    }
                }
            }
            for (int k = 0; k < end - start && stack.count > 0; k++)
            {
                int i = reverse ? end - 1 - k : start + k;
                var sl = slots[i];
                if (!sl.HasItem && sl.MayPlace(stack) && sl.active)
                {
                    int n = Math.Min(stack.count, sl.MaxStackFor(stack));
                    sl.Item = stack.CopyWithCount(n);
                    stack.count -= n; moved = true;
                    break;
                }
            }
            return moved;
        }

        public void Click(int slotIndex, int button, ClickType type)
        {
            Slot slot = slotIndex >= 0 && slotIndex < slots.Count ? slots[slotIndex] : null;
            switch (type)
            {
                case ClickType.Pickup:
                    if (slotIndex == -999) // outside: drop carried
                    {
                        if (carried != null)
                        {
                            if (button == 0) { player.DropItem(carried, true); carried = null; }
                            else { player.DropItem(carried.Split(1), true); if (carried.count <= 0) carried = null; }
                        }
                        return;
                    }
                    if (slot == null || !slot.active) return;
                    ClickPickup(slot, button);
                    break;
                case ClickType.QuickMove:
                    if (slot == null || !slot.HasItem || !slot.MayPickup(player)) return;
                    if (slot.isResult)
                    {
                        // craft repeatedly until can't
                        var first = slot.Item?.item;
                        for (int guard = 0; guard < 64 && slot.HasItem && slot.Item.item == first; guard++)
                        {
                            var r = QuickMove(slotIndex);
                            if (r == null) break;
                        }
                    }
                    else QuickMove(slotIndex);
                    break;
                case ClickType.Swap:
                    if (slot == null) return;
                    {
                        var inv = player.inventory;
                        int hb = button; // 0-8 hotbar, 40 offhand
                        var hotItem = hb == 40 ? inv.offhand : inv.main[hb];
                        var slotItem = slot.Item;
                        if (slot.isResult)
                        {
                            if (slotItem != null && hotItem == null) { var taken = slot.Remove(slotItem.count); slot.OnTake(player, taken); if (hb == 40) inv.offhand = taken; else inv.main[hb] = taken; SlotChanged(slot); }
                            return;
                        }
                        if (hotItem != null && !slot.MayPlace(hotItem)) return;
                        slot.Item = hotItem;
                        if (hb == 40) inv.offhand = slotItem; else inv.main[hb] = slotItem;
                        SlotChanged(slot);
                    }
                    break;
                case ClickType.Clone:
                    if (player.IsCreative && slot != null && slot.HasItem && carried == null)
                        carried = slot.Item.CopyWithCount(slot.Item.MaxStack);
                    break;
                case ClickType.Throw:
                    if (slot != null && slot.HasItem && carried == null && slot.MayPickup(player))
                    {
                        var t = slot.Remove(button == 1 ? slot.Item.count : 1);
                        slot.OnTake(player, t);
                        player.DropItem(t, true);
                        SlotChanged(slot);
                    }
                    break;
                case ClickType.PickupAll:
                    if (carried == null || slot == null) return;
                    for (int pass = 0; pass < 2; pass++)
                        foreach (var s in slots)
                        {
                            if (carried.count >= carried.MaxStack) break;
                            if (!s.HasItem || s.isResult || !s.Item.Stackable(carried)) continue;
                            if (pass == 0 && s.Item.count == s.Item.MaxStack) continue;
                            int n = Math.Min(s.Item.count, carried.MaxStack - carried.count);
                            var taken = s.Remove(n);
                            carried.count += taken.count;
                            SlotChanged(s);
                        }
                    break;
            }
        }

        void ClickPickup(Slot slot, int button)
        {
            var inSlot = slot.Item;
            if (slot.isResult)
            {
                if (inSlot == null) return;
                if (carried == null)
                {
                    var t = slot.Remove(inSlot.count);
                    slot.OnTake(player, t);
                    carried = t;
                }
                else if (carried.Stackable(inSlot) && carried.count + inSlot.count <= carried.MaxStack)
                {
                    var t = slot.Remove(inSlot.count);
                    slot.OnTake(player, t);
                    carried.count += t.count;
                }
                SlotChanged(slot);
                return;
            }
            if (inSlot == null)
            {
                if (carried == null) return;
                if (!slot.MayPlace(carried)) return;
                int n = button == 0 ? carried.count : 1;
                n = Math.Min(n, slot.MaxStackFor(carried));
                slot.Item = carried.Split(n);
                if (carried.count <= 0) carried = null;
            }
            else if (slot.MayPickup(player))
            {
                if (carried == null)
                {
                    int n = button == 0 ? inSlot.count : (inSlot.count + 1) / 2;
                    carried = slot.Remove(n);
                    slot.OnTake(player, carried);
                }
                else if (slot.MayPlace(carried))
                {
                    if (carried.Stackable(inSlot))
                    {
                        int n = button == 0 ? carried.count : 1;
                        n = Math.Min(n, slot.MaxStackFor(carried) - inSlot.count);
                        if (n > 0) { inSlot.count += n; carried.count -= n; slot.container.SetChanged(); if (carried.count <= 0) carried = null; }
                    }
                    else if (carried.count <= slot.MaxStackFor(carried))
                    {
                        slot.Item = carried; carried = inSlot;
                    }
                }
                else if (carried.Stackable(inSlot))
                {
                    int n = Math.Min(inSlot.count, carried.MaxStack - carried.count);
                    if (n > 0) { var t = slot.Remove(n); carried.count += t.count; slot.OnTake(player, t); }
                }
            }
            SlotChanged(slot);
        }

        // -------- drag distribution (quick craft): button 0 = split evenly, 1 = one each, 2 = clone full stacks (creative)
        public void BeginDrag(int button) { dragButton = button; dragSlots.Clear(); }
        public void DragOver(Slot s)
        {
            if (dragButton < 0 || carried == null || s == null || !s.active || s.isResult) return;
            if (dragSlots.Contains(s)) return;
            if (s.HasItem && !s.Item.Stackable(carried)) return;
            if (!s.MayPlace(carried)) return;
            if (dragButton != 2 && dragSlots.Count >= carried.count) return;
            dragSlots.Add(s);
        }
        public void EndDrag()
        {
            if (dragButton < 0 || carried == null) { dragButton = -1; dragSlots.Clear(); return; }
            if (dragSlots.Count == 1 && dragButton != 2)
            {
                int b = dragButton; dragButton = -1; var s = dragSlots[0]; dragSlots.Clear();
                ClickPickup(s, b);
                return;
            }
            int total = carried.count;
            foreach (var s in dragSlots)
            {
                int per = dragButton == 0 ? total / dragSlots.Count : (dragButton == 1 ? 1 : carried.MaxStack);
                if (dragButton != 2 && carried.count <= 0) break;
                int cur = s.HasItem ? s.Item.count : 0;
                int max = s.MaxStackFor(carried);
                int n = Math.Min(per, max - cur);
                if (dragButton != 2) n = Math.Min(n, carried.count);
                if (n <= 0) continue;
                if (s.HasItem) { s.Item.count += n; s.container.SetChanged(); }
                else s.Item = carried.CopyWithCount(n);
                if (dragButton != 2) carried.count -= n;
                SlotChanged(s);
            }
            if (carried.count <= 0) carried = null;
            dragButton = -1; dragSlots.Clear();
        }
        /// <summary>Preview: how many items would land in a dragged slot.</summary>
        public int DragPreview(Slot s)
        {
            if (!dragSlots.Contains(s) || carried == null) return -1;
            int per = dragButton == 0 ? carried.count / Mathf.Max(1, dragSlots.Count) : (dragButton == 1 ? 1 : carried.MaxStack);
            int cur = s.HasItem ? s.Item.count : 0;
            return Math.Min(cur + per, s.MaxStackFor(carried));
        }
    }

    /// <summary>Crafting grid container with change notification.</summary>
    public class CraftingGrid : SimpleContainer
    {
        public readonly int w, h;
        public CraftingGrid(int w, int h) : base(w * h) { this.w = w; this.h = h; }
        public ItemStack At(int x, int y) => x < 0 || y < 0 || x >= w || y >= h ? null : items[y * w + x];
    }

    public class ResultSlot : Slot
    {
        public CraftingGrid grid;
        public ResultSlot(IContainer result, CraftingGrid grid, int x, int y) : base(result, 0, x, y) { this.grid = grid; isResult = true; }
        public override void OnTake(Player p, ItemStack taken)
        {
            if (taken == null) return;
            var recipe = Recipes.FindCrafting(grid, p.world);
            var rem = recipe != null ? recipe.Remainders(grid) : null;
            for (int i = 0; i < grid.Size; i++)
            {
                var s = grid.items[i];
                if (s == null) continue;
                s.count--;
                if (s.count <= 0) grid.items[i] = null;
                if (rem != null && rem[i] != null)
                {
                    if (grid.items[i] == null) grid.items[i] = rem[i];
                    else p.inventory.AddOrDrop(rem[i]);
                }
            }
            if (recipe != null) { p.OnCrafted(recipe, taken); p.knownRecipes.Add(recipe.id); }
            grid.SetChanged();
        }
    }

    // ============================================================================ Player inventory menu (2x2 crafting)
    public class InventoryMenu : Menu
    {
        public readonly CraftingGrid craft = new CraftingGrid(2, 2);
        public readonly SimpleContainer result = new SimpleContainer(1);
        public const int ResultIdx = 0, CraftStart = 1, ArmorStart = 5, InvStart = 9, HotbarStart = 36, OffhandIdx = 45;
        public InventoryMenu(Player p)
        {
            player = p; background = "inventory"; title = "Crafting";
            AddSlot(new ResultSlot(result, craft, 154, 28));
            for (int r = 0; r < 2; r++) for (int c = 0; c < 2; c++) AddSlot(new Slot(craft, r * 2 + c, 98 + c * 18, 18 + r * 18));
            string[] icons = { "empty_armor_slot_helmet", "empty_armor_slot_chestplate", "empty_armor_slot_leggings", "empty_armor_slot_boots" };
            for (int i = 0; i < 4; i++)
            {
                int armorIdx = 3 - i; // head first
                var s = AddSlot(new Slot(p.inventory, 36 + armorIdx, 8, 8 + i * 18));
                int slotType = armorIdx;
                s.filter = st => (st.item is ArmorItem ai && (int)ai.slot == slotType) || (slotType == 3 && (st.item.id == "carved_pumpkin" || st.item.id.EndsWith("_head") || st.item.id.EndsWith("_skull"))) || (slotType == 2 && st.item.id == "elytra");
                s.maxStack = 1; s.emptyIcon = icons[i];
            }
            AddPlayerSlots(p.inventory, 8, 84);
            var off = AddSlot(new Slot(p.inventory, 40, 77, 62)); off.emptyIcon = "empty_slot_shield";
            craft.onChanged = UpdateResult;
        }
        void UpdateResult()
        {
            var r = Recipes.FindCrafting(craft, player.world);
            result.items[0] = r?.Assemble(craft);
        }
        public override void Removed()
        {
            base.Removed();
            for (int i = 0; i < craft.Size; i++) if (craft.items[i] != null) { player.inventory.AddOrDrop(craft.items[i]); craft.items[i] = null; }
            result.items[0] = null;
        }
        public override ItemStack QuickMove(int idx)
        {
            var slot = slots[idx];
            if (!slot.HasItem) return null;
            var stack = slot.Item;
            var copy = stack.Copy();
            if (idx == ResultIdx)
            {
                var taken = stack.Copy();
                if (!CanFit(taken, InvStart, OffhandIdx)) return null;
                slot.Remove(stack.count);
                slot.OnTake(player, taken);
                MoveItemTo(taken, InvStart, OffhandIdx, true);
                if (taken.count > 0) player.DropItem(taken, false);
                return copy;
            }
            if (idx >= CraftStart && idx < InvStart) { if (!MoveItemTo(stack, InvStart, OffhandIdx, false)) return null; }
            else if (stack.item is ArmorItem ai && !slots[ArmorStart + (3 - (int)ai.slot)].HasItem) { MoveItemTo(stack, ArmorStart + (3 - (int)ai.slot), ArmorStart + (3 - (int)ai.slot) + 1, false); }
            else if (stack.item.id == "shield" && !slots[OffhandIdx].HasItem) MoveItemTo(stack, OffhandIdx, OffhandIdx + 1, false);
            else if (idx >= InvStart && idx < HotbarStart) { if (!MoveItemTo(stack, HotbarStart, OffhandIdx, false)) return null; }
            else if (idx >= HotbarStart && idx < OffhandIdx) { if (!MoveItemTo(stack, InvStart, HotbarStart, false)) return null; }
            else if (!MoveItemTo(stack, InvStart, OffhandIdx, false)) return null;
            if (stack.count <= 0) slot.Item = null; else slot.container.SetChanged();
            SlotChanged(slot);
            return copy;
        }
        protected bool CanFit(ItemStack s, int start, int end)
        {
            int room = 0;
            for (int i = start; i < end; i++)
            {
                var it = slots[i].Item;
                if (it == null) room += s.MaxStack;
                else if (it.Stackable(s)) room += s.MaxStack - it.count;
                if (room >= s.count) return true;
            }
            return false;
        }
    }

    /// <summary>Base for menus of [container slots][player inventory 27][hotbar 9].</summary>
    public abstract class ContainerMenu : Menu
    {
        protected int containerSlots;
        public override ItemStack QuickMove(int idx)
        {
            var slot = slots[idx];
            if (!slot.HasItem) return null;
            var stack = slot.Item;
            var copy = stack.Copy();
            int invStart = containerSlots, end = slots.Count;
            if (slot.isResult)
            {
                var taken = stack.Copy();
                slot.Remove(stack.count);
                slot.OnTake(player, taken);
                MoveItemTo(taken, invStart, end, true);
                if (taken.count > 0) player.DropItem(taken, false);
                SlotChanged(slot);
                return copy;
            }
            if (idx < containerSlots)
            {
                if (!MoveItemTo(stack, invStart, end, true)) return null;
            }
            else
            {
                if (!QuickMoveIntoContainer(stack))
                {
                    // move between inventory and hotbar
                    int hotStart = end - 9;
                    if (idx < hotStart) { if (!MoveItemTo(stack, hotStart, end, false)) return null; }
                    else if (!MoveItemTo(stack, invStart, hotStart, false)) return null;
                }
            }
            if (stack.count <= 0) slot.Item = null; else slot.container.SetChanged();
            SlotChanged(slot);
            return copy;
        }
        protected virtual bool QuickMoveIntoContainer(ItemStack stack) => MoveItemTo(stack, 0, containerSlots, false);
    }

    public class ChestMenu : ContainerMenu
    {
        public IContainer container; public int rows;
        public ChestMenu(Player p, IContainer c, int rows, string title)
        {
            player = p; container = c; this.rows = rows; this.title = title; background = "chest" + rows;
            height = 114 + rows * 18;
            for (int r = 0; r < rows; r++) for (int col = 0; col < 9; col++) AddSlot(new Slot(c, r * 9 + col, 8 + col * 18, 18 + r * 18));
            containerSlots = rows * 9;
            AddPlayerSlots(p.inventory, 8, 18 + rows * 18 + 14);
        }
        public override bool StillValid() => container.StillValid(player);
        public override void Removed() { base.Removed(); if (container is ChestEntity ce) ce.CloseBy(player); if (container is DoubleChestContainer dc) dc.Close(player); if (container is BarrelEntity be) be.Close(); if (container is EnderChestContainer ec) ec.Close(); if (container is ShulkerBoxEntity sb) sb.Close(); }
    }

    public class CraftingTableMenu : ContainerMenu
    {
        public readonly CraftingGrid craft = new CraftingGrid(3, 3);
        public readonly SimpleContainer result = new SimpleContainer(1);
        public Int3 pos;
        public CraftingTableMenu(Player p, Int3 pos)
        {
            player = p; this.pos = pos; title = "Crafting"; background = "crafting_table";
            AddSlot(new ResultSlot(result, craft, 124, 35));
            for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) AddSlot(new Slot(craft, r * 3 + c, 30 + c * 18, 17 + r * 18));
            containerSlots = 10;
            AddPlayerSlots(p.inventory, 8, 84);
            craft.onChanged = () => { var rc = Recipes.FindCrafting(craft, player.world); result.items[0] = rc?.Assemble(craft); };
        }
        protected override bool QuickMoveIntoContainer(ItemStack stack) => false;
        public override ItemStack QuickMove(int idx)
        {
            if (idx >= 1 && idx < 10)
            {
                var slot = slots[idx]; if (!slot.HasItem) return null;
                var stack = slot.Item; var copy = stack.Copy();
                if (!MoveItemTo(stack, 10, slots.Count, false)) return null;
                if (stack.count <= 0) slot.Item = null; else slot.container.SetChanged();
                return copy;
            }
            return base.QuickMove(idx);
        }
        public override bool StillValid() => player.world.GetBlock(pos).id == "crafting_table" && (player.position - pos.Center).sqrMagnitude < 64;
        public override void Removed()
        {
            base.Removed();
            for (int i = 0; i < craft.Size; i++) if (craft.items[i] != null) { player.inventory.AddOrDrop(craft.items[i]); craft.items[i] = null; }
        }
    }
}
