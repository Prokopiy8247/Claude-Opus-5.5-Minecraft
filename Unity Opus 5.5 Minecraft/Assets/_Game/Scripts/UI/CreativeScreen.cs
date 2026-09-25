using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Creative inventory: a searchable, tabbed catalogue of every registered item, with the player's own
    /// inventory and hotbar underneath. Clicking a catalogue entry puts the stack on the cursor, so it can be
    /// dropped into any slot; shift-click sends it straight to the first free slot. The "Survival Inv." tab swaps
    /// the catalogue for the ordinary player inventory (2x2 crafting, armour, offhand), driven by the player's own
    /// <see cref="InventoryMenu"/> with the same slot drawing and click protocol as <see cref="ContainerScreen"/>.
    /// </summary>
    public sealed class CreativeScreen : GuiScreen
    {
        sealed class TabDef
        {
            public string label, icon, iconItem;
            public CreativeTab cat;
            public int row, col;
            public TabDef(string label, string icon, CreativeTab cat, int row, int col, string iconItem)
            {
                this.label = label; this.icon = icon; this.cat = cat; this.row = row; this.col = col; this.iconItem = iconItem;
            }
        }

        // Array order is the tab index that LastTab and the scripted "tab N" step use; the screen position comes
        // from row (0 above the panel, 1 below) and col (6 is the detached right-hand slot, as in the original).
        // The glyph is only drawn when the icon item is not registered.
        static readonly TabDef[] tabs =
        {
            new TabDef("Search", "?", CreativeTab.None, 0, 6, "compass"),
            new TabDef("Building", "#", CreativeTab.Building, 0, 0, "bricks"),
            new TabDef("Colored", "@", CreativeTab.Colored, 0, 1, "cyan_wool"),
            new TabDef("Natural", "&", CreativeTab.Natural, 0, 2, "grass_block"),
            new TabDef("Functional", "*", CreativeTab.Functional, 0, 3, "crafting_table"),
            new TabDef("Redstone", "R", CreativeTab.Redstone, 0, 4, "redstone"),
            new TabDef("Tools", "T", CreativeTab.Tools, 1, 0, "iron_pickaxe"),
            new TabDef("Combat", "S", CreativeTab.Combat, 1, 1, "golden_sword"),
            new TabDef("Food", "F", CreativeTab.Food, 1, 2, "golden_apple"),
            new TabDef("Ingredients", "I", CreativeTab.Ingredients, 1, 3, "iron_ingot"),
            new TabDef("Spawn Eggs", "E", CreativeTab.SpawnEggs, 1, 4, "creeper_spawn_egg"),
            new TabDef("Survival Inv.", "P", CreativeTab.None, 1, 6, "chest"),
            new TabDef("Operator", "O", CreativeTab.Operator, 1, 5, "command_block"),
        };

        /// <summary>Index of the tab that shows the player's survival inventory instead of the catalogue.</summary>
        public const int SurvivalTab = 11;

        /// <summary>Tab restored on the next open. Written whenever the tab changes, never on close (see <see cref="SelectTab"/>).</summary>
        public static int LastTab = 1;
        public static string LastSearch = "";

        const int Cols = 9;
        const float TabW = 26f, TabStep = 27f;
        int tab, scroll, rows = 5;
        string search = "";
        bool searchFocused;
        ItemStack carried;
        readonly List<Item> results = new List<Item>();
        static ItemStack[] tabIcons;

        // survival tab: pointer state for the borrowed InventoryMenu, as ContainerScreen keeps it
        Slot hoverSlot, lastClickSlot;
        float lastClickTime;
        bool dragging;
        int dragButton;

        public override void OnOpen()
        {
            title = "Creative Inventory";
            pauseGame = true;
            tab = Mathf.Clamp(LastTab, 0, tabs.Length - 1);
            search = LastSearch;
            Rebuild();
        }
        public override void OnClose()
        {
            LastSearch = search;
            var p = P;
            if (p != null && p.inventoryMenu != null)
            {
                EndDrag(p.inventoryMenu);
                // the survival tab borrows the real inventory menu: its cursor stack and 2x2 grid go back to the player
                p.inventoryMenu.Removed();
            }
            if (carried != null && !carried.IsEmpty) { P?.inventory.AddOrDrop(carried); carried = null; }
        }

        /// <summary>
        /// Switches tab and remembers it at once rather than on close, so a caller that picks the tab while the screen
        /// is open (the scripted "tab N" step sets LastTab, then pops and re-pushes) is not overwritten by OnClose.
        /// The cursor stack follows the player between the catalogue and the survival tab.
        /// </summary>
        void SelectTab(int i)
        {
            i = Mathf.Clamp(i, 0, tabs.Length - 1);
            if (i == tab) return;
            var menu = P?.inventoryMenu;
            if (menu != null)
            {
                EndDrag(menu);
                if (i == SurvivalTab && carried != null && menu.carried == null) { menu.carried = carried; carried = null; }
                else if (tab == SurvivalTab && menu.carried != null && carried == null) { carried = menu.carried; menu.carried = null; }
            }
            hoverSlot = null;
            tab = i;
            LastTab = i;
            Rebuild();
        }

        void EndDrag(Menu menu)
        {
            if (!dragging) return;
            dragging = false;
            menu.EndDrag();
        }

        void Rebuild()
        {
            results.Clear();
            if (tab == SurvivalTab) { scroll = 0; return; }
            string q = search.Trim().ToLowerInvariant();
            if (tab == 0)
            {
                if (q.Length == 0) return;
                foreach (var it in Items.All)
                {
                    if (it.hiddenInCreative) continue;
                    if (it.id.Contains(q) || it.displayName.ToLowerInvariant().Contains(q)) results.Add(it);
                }
                results.Sort((a, b) => (a.id.StartsWith(q) ? 0 : 1).CompareTo(b.id.StartsWith(q) ? 0 : 1));
            }
            else
            {
                var cat = tabs[tab].cat;
                foreach (var it in Items.All) if (!it.hiddenInCreative && it.tab == cat) results.Add(it);
                results.Sort((a, b) => string.CompareOrdinal(a.id, b.id));
            }
            scroll = 0;
        }

        // ------------------------------------------------------------------ geometry
        float Px => Mathf.Floor((W - PanelW) * 0.5f);
        float Py => Mathf.Floor((H - PanelH) * 0.5f);
        // tall enough to hold the catalogue rows, the inventory and the hotbar; the tab strips add 28 above and below
        const float PanelW = 195f, PanelH = 180f;

        public override void Input(Hud hud)
        {
            if (hud.pressedEscape || (hud.pressedE && !searchFocused))
            {
                Sounds.PlayUI("ui.button.click");
                hud.Pop();
                return;
            }
            if (searchFocused)
            {
                bool changed = false;
                foreach (var c in hud.typedChars) if (c >= ' ' && search.Length < 32) { search += c; changed = true; }
                if (hud.pressedBackspace && search.Length > 0) { search = search.Substring(0, search.Length - 1); changed = true; }
                if (hud.pressedEnter) searchFocused = false;
                if (changed) { if (tab != 0) SelectTab(0); Rebuild(); }
            }
            if (tab == SurvivalTab) SurvivalKeys(hud);
            if (hud.scrollDelta != 0)
            {
                int total = Mathf.CeilToInt(results.Count / (float)Cols);
                scroll = Mathf.Clamp(scroll - hud.scrollDelta, 0, Mathf.Max(0, total - rows));
            }
        }

        public override void Render(UiRenderer ui, Hud hud)
        {
            var p = P;
            if (p == null) { ui.Text("No world loaded", W * 0.5f, H * 0.5f, Styles.Text, 1, true); return; }
            Dim(0.7f);
            float x = Px, y = Py;
            int hoverTab = HandleTabs(hud, x, y);
            DrawTabs(ui, hud, x, y, false); // unselected tabs sit behind the panel...
            Panel(x, y, PanelW, PanelH);
            DrawTabs(ui, hud, x, y, true);  // ...the selected one overlaps its rim
            if (tab == SurvivalTab) RenderSurvival(ui, hud, p, x, y);
            else RenderCatalogue(ui, hud, p, x, y);
            if (hoverTab >= 0) DrawLabel(ui, hud, tabs[hoverTab].label);
        }

        // ------------------------------------------------------------------ tabs
        /// <summary>Sprite origin of a 26x32 tab: the top row ends on the panel's rim, the bottom row starts on it.</summary>
        Vector2 TabPos(int i, float x, float y)
        {
            var t = tabs[i];
            float tx = t.col < 6 ? x + t.col * TabStep : x + PanelW - TabW;
            return new Vector2(tx, t.row == 0 ? y - 28f : y + PanelH - 4f);
        }

        /// <summary>The part of a tab that shows outside the panel is its click area.</summary>
        bool OverTab(Hud hud, int i, float x, float y)
        {
            var o = TabPos(i, x, y);
            float top = tabs[i].row == 0 ? o.y : o.y + 4f;
            return hud.mouseX >= o.x && hud.mouseX < o.x + TabW && hud.mouseY >= top && hud.mouseY < top + 28f;
        }

        /// <summary>Tab under the pointer, or -1. A left click on a tab switches to it and is consumed, so it never
        /// reaches the slots or the "click outside" handling.</summary>
        int HandleTabs(Hud hud, float x, float y)
        {
            int over = -1;
            for (int i = 0; i < tabs.Length; i++) if (OverTab(hud, i, x, y)) over = i;
            if (over >= 0 && hud.mouseClicked)
            {
                hud.mouseClicked = false;
                if (over != tab) { SelectTab(over); Sounds.PlayUI("ui.button.click"); }
            }
            return over;
        }

        void DrawTabs(UiRenderer ui, Hud hud, float x, float y, bool selected)
        {
            for (int i = 0; i < tabs.Length; i++) if ((i == tab) == selected) DrawTab(ui, hud, i, x, y);
        }

        void DrawTab(UiRenderer ui, Hud hud, int i, float x, float y)
        {
            var t = tabs[i];
            bool sel = i == tab;
            var o = TabPos(i, x, y);
            ui.Icon((t.row == 0 ? "tab_top" : "tab_bottom") + (sel ? "_sel" : ""), o.x, o.y, Styles.Text);
            // centred in the part outside the panel: rows 2-27 of a top tab, 4-29 of a bottom one
            float iy = t.row == 0 ? o.y + 7f : o.y + 9f;
            var icon = TabIcon(i);
            if (icon != null) hud.DrawItemWithCount(ui, icon, o.x + 5f, iy, null);
            else ui.Text(t.icon, o.x + 13f, iy + 4f, sel ? Styles.Yellow : Styles.Text, 1, true);
        }

        static ItemStack TabIcon(int i)
        {
            if (tabIcons == null)
            {
                tabIcons = new ItemStack[tabs.Length];
                for (int k = 0; k < tabs.Length; k++)
                {
                    var it = Items.Get(tabs[k].iconItem);
                    if (it != null) tabIcons[k] = new ItemStack(it, 1);
                }
            }
            return tabIcons[i];
        }

        /// <summary>A one-line tooltip beside the pointer (tab names, the clear button).</summary>
        void DrawLabel(UiRenderer ui, Hud hud, string text)
        {
            float w = ui.TextWidth(text) + 6f;
            float lx = Mathf.Min(hud.mouseX + 8f, W - w - 2f), ly = Mathf.Max(2f, hud.mouseY - 14f);
            ui.Rect(lx, ly, w, 12f, Styles.TooltipBg);
            ui.Outline(lx, ly, w, 12f, Styles.TooltipBorderBottom);
            ui.Text(text, lx + 3f, ly + 2f, Styles.Text, 1);
        }

        /// <summary>Destroy-style button: empties the inventory and the cursor.</summary>
        void ClearButton(UiRenderer ui, Hud hud, Player p, float bx, float by)
        {
            bool over = Hover(hud, bx, by);
            if (IconButton(bx, by, 18f, 18f, "cross")) { hud.mouseClicked = false; ClearInventory(p); }
            if (over) DrawLabel(ui, hud, "Clear Inventory");
        }

        // ------------------------------------------------------------------ catalogue tabs
        void RenderCatalogue(UiRenderer ui, Hud hud, Player p, float x, float y)
        {
            ui.Text(title, x + PanelW * 0.5f, y + 4f, Styles.Text, 1, true);

            // ---- grid area
            float gx = x + 6f, gy = y + 20f, gw = Cols * 18f;
            if (tab == 0)
            {
                ui.Rect(gx, gy, gw, 14f, new Color32(0, 0, 0, 200));
                ui.Outline(gx, gy, gw, 14f, searchFocused ? Styles.Text : Styles.PanelDark);
                string shown = search.Length == 0 ? (searchFocused ? "" : "Search items...") : search;
                var col = search.Length == 0 ? Styles.TextGray : Styles.Text;
                ui.Text(shown + (searchFocused && Mathf.FloorToInt(Time.time * 2f) % 2 == 0 ? "_" : ""), gx + 2f, gy + 3f, col, 1);
                if (search.Length > 0)
                {
                    string n = results.Count.ToString();
                    ui.Text(n, gx + gw - 2f - ui.TextWidth(n), gy + 3f, Styles.TextGray, 1);
                }
                if (hud.mouseClicked && hud.mouseX >= gx && hud.mouseX < gx + gw && hud.mouseY >= gy && hud.mouseY < gy + 14f) searchFocused = true;
                else if (hud.mouseClicked) searchFocused = false;
                gy += 18f;
            }
            else searchFocused = false;

            // 88 = inventory (3 rows + gap) + hotbar + margins below the grid
            rows = Mathf.Max(1, Mathf.FloorToInt((y + PanelH - 88f - gy) / 18f));
            int total = Mathf.CeilToInt(results.Count / (float)Cols);
            scroll = Mathf.Clamp(scroll, 0, Mathf.Max(0, total - rows));

            ui.PushClip(gx, gy, gw, rows * 18f);
            for (int r = 0; r < rows; r++)
                for (int c = 0; c < Cols; c++)
                {
                    int idx = (r + scroll) * Cols + c;
                    float sx = gx + c * 18f, sy = gy + r * 18f;
                    if (idx >= results.Count) continue;
                    var it = results[idx];
                    // the catalogue shows single items; a click takes one (repeat clicks add one), shift-click or
                    // middle-click take a full stack, as in the original
                    var stack = new ItemStack(it, 1);
                    hud.DrawItemWithCount(ui, stack, sx + 1f, sy + 1f, p);
                    if (hud.mouseX >= sx && hud.mouseX < sx + 18f && hud.mouseY >= sy && hud.mouseY < sy + 18f)
                    {
                        ui.Rect(sx, sy, 18f, 18f, Styles.SlotHighlight);
                        hud.pendingTooltip = stack;
                        bool full = hud.shift || hud.mouseClickedMiddle;
                        if (hud.mouseClicked || hud.mouseClickedMiddle)
                        {
                            int n = full ? Mathf.Max(1, it.maxStack) : 1;
                            if (hud.shift && hud.mouseClicked) { p.inventory.Add(new ItemStack(it, n)); }
                            else if (carried == null || carried.IsEmpty) carried = new ItemStack(it, n);
                            else if (carried.item == it) carried.count = Mathf.Min(carried.MaxStack, carried.count + n);
                            else carried = new ItemStack(it, n);
                            hud.mouseClicked = false; hud.mouseClickedMiddle = false;
                            Sounds.PlayUI("ui.button.click");
                        }
                    }
                }
            ui.PopClip();

            // scrollbar on the right of the grid
            if (total > rows && results.Count > 0)
            {
                float sbx = gx + gw + 1f, trackH = rows * 18f;
                ui.Rect(sbx, gy, 10f, trackH, new Color32(0, 0, 0, 120));
                float frac = rows / (float)total;
                float knob = Mathf.Max(12f, trackH * frac);
                float kpos = (trackH - knob) * (scroll / (float)Mathf.Max(1, total - rows));
                ui.Icon("scroll_knob", sbx - 1f, gy + kpos, Styles.Text);
                if (hud.mouseDown && hud.mouseX >= sbx - 1f && hud.mouseX < sbx + 11f && hud.mouseY >= gy && hud.mouseY < gy + trackH)
                {
                    float t2 = Mathf.Clamp01((hud.mouseY - gy - knob * 0.5f) / Mathf.Max(1f, trackH - knob));
                    scroll = Mathf.Clamp(Mathf.RoundToInt(t2 * (total - rows)), 0, Mathf.Max(0, total - rows));
                }
            }

            // ---- the player's own inventory and hotbar
            float invY = gy + rows * 18f + 6f;
            var inv = p.inventory;
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 9; c++)
                {
                    int slot = 9 + r * 9 + c;
                    float sx = gx + c * 18f, sy = invY + r * 18f;
                    DrawSlotFrame(ui, sx, sy);
                    var s = inv.main[slot];
                    if (s != null) hud.DrawItemWithCount(ui, s, sx + 1f, sy + 1f, p);
                    if (Hover(hud, sx, sy)) ClickInto(ref inv.main[slot], hud, false, p);
                }
            float hotY = invY + 3 * 18f + 4f;
            for (int c = 0; c < 9; c++)
            {
                float sx = gx + c * 18f;
                DrawSlotFrame(ui, sx, hotY);
                var s = inv.main[c];
                if (s != null) hud.DrawItemWithCount(ui, s, sx + 1f, hotY + 1f, p);
                if (Hover(hud, sx, hotY)) ClickInto(ref inv.main[c], hud, false, p);
            }
            // the HUD's selection frame, centred on the selected slot (it overlaps the neighbours' edges, as on the HUD)
            ui.Icon("hotbar_selection", gx + inv.selected * 18f - 3f, hotY - 3f);

            // ---- clear button beside the hotbar, then the carried stack over everything
            ClearButton(ui, hud, p, gx + gw + 4f, hotY);
            if (carried != null && !carried.IsEmpty)
            {
                hud.DrawItemWithCount(ui, carried, hud.mouseX - 8f, hud.mouseY - 8f, p);
                if (hud.mouseClicked && hud.mouseY < invY - 4f) { inv.AddOrDrop(carried); carried = null; }
            }
        }

        // ------------------------------------------------------------------ survival inventory tab
        /// <summary>
        /// The player's own <see cref="InventoryMenu"/> centred in the panel: armour, preview box, 2x2 crafting with its
        /// result, offhand, main inventory and hotbar. Slots are drawn by the shared <see cref="GuiScreen.DrawSlot"/> and
        /// clicks go through <see cref="Menu.Click"/>, so stacks move exactly as on the survival screen.
        /// </summary>
        void RenderSurvival(UiRenderer ui, Hud hud, Player p, float x, float y)
        {
            var menu = p.inventoryMenu;
            if (menu == null) return;
            float ox = x + Mathf.Floor((PanelW - menu.width) * 0.5f), oy = y + Mathf.Floor((PanelH - menu.height) * 0.5f);

            // player preview box and the crafting arrow, as ContainerScreen.DrawBackground draws them for InventoryMenu
            ui.Rect(ox + 26f, oy + 8f, 51f, 72f, new Color32(0, 0, 0, 255));
            ui.Rect(ox + 27f, oy + 9f, 49f, 70f, new Color32(60, 60, 70, 255));
            ui.Text(p.playerName, ox + 51f, oy + 70f, Styles.TextGray, 0, true);
            ui.Icon("arrow_full", ox + 134f, oy + 28f, new Color32(140, 140, 140, 255));
            ui.Text("Crafting", ox + 97f, oy + 6f, new Color32(64, 64, 64, 255), 0);

            hoverSlot = null;
            foreach (var s in menu.slots) if (s.active) DrawSlot(menu, s, ox, oy, ref hoverSlot);
            // the selected hotbar slot gets the HUD's selection frame
            foreach (var s in menu.slots)
                if (ReferenceEquals(s.container, p.inventory) && s.index == p.inventory.selected)
                {
                    ui.Icon("hotbar_selection", ox + s.x - 3f, oy + s.y - 3f);
                    break;
                }

            ClearButton(ui, hud, p, ox + 154f, oy + 62f);
            HandleMenuMouse(hud, menu, x, y);
            DrawCarriedAndTooltip(menu, hoverSlot);
        }

        /// <summary>ContainerScreen's click protocol: pick up / place, right-click halves or places one, shift-click
        /// quick-moves, double-click gathers, a press on a slot while carrying starts a drag across slots, and a click
        /// outside the panel throws the cursor stack.</summary>
        void HandleMenuMouse(Hud hud, Menu menu, float x, float y)
        {
            if (dragging)
            {
                if (hoverSlot != null) menu.DragOver(hoverSlot);
                bool released = dragButton == 0 ? hud.mouseReleased : hud.mouseReleasedRight;
                if (released) { menu.EndDrag(); dragging = false; }
                return;
            }
            if (!hud.mouseClicked && !hud.mouseClickedRight) return;
            int button = hud.mouseClicked ? 0 : 1;
            if (hoverSlot == null)
            {
                if (!OverScreen(hud, x, y)) menu.Click(-999, button, ClickType.Pickup);
                return;
            }
            bool dbl = button == 0 && lastClickSlot == hoverSlot && Time.unscaledTime - lastClickTime < 0.3f;
            lastClickSlot = hoverSlot; lastClickTime = Time.unscaledTime;
            if (dbl && menu.carried != null) { menu.Click(hoverSlot.menuIndex, 0, ClickType.PickupAll); return; }
            if (hud.shift) { menu.Click(hoverSlot.menuIndex, button, ClickType.QuickMove); Sounds.PlayUI("ui.button.click", 0.2f); return; }
            if (menu.carried != null && !hoverSlot.isResult && (!hoverSlot.HasItem || hoverSlot.Item.Stackable(menu.carried)))
            {
                // a press without movement resolves as a normal pickup/place in EndDrag
                menu.BeginDrag(button);
                menu.DragOver(hoverSlot);
                dragging = true; dragButton = button;
                return;
            }
            menu.Click(hoverSlot.menuIndex, button, ClickType.Pickup);
        }

        /// <summary>Hotkeys over a survival-tab slot, as in ContainerScreen: 1-9 swap with the hotbar, F with the offhand,
        /// Q throws one (Ctrl: the stack), middle-click copies a full stack.</summary>
        void SurvivalKeys(Hud hud)
        {
            var p = P;
            if (p == null || p.inventoryMenu == null || hoverSlot == null) return;
            var menu = p.inventoryMenu;
            if (hud.numberKey >= 0) menu.Click(hoverSlot.menuIndex, hud.numberKey, ClickType.Swap);
            if (hud.pressedF) menu.Click(hoverSlot.menuIndex, 40, ClickType.Swap);
            if (hud.pressedQ) menu.Click(hoverSlot.menuIndex, hud.ctrl ? 1 : 0, ClickType.Throw);
            if (hud.mouseClickedMiddle && p.IsCreative) menu.Click(hoverSlot.menuIndex, 2, ClickType.Clone);
        }

        /// <summary>Over the panel or its tab strips: clicks there never throw the cursor stack.</summary>
        bool OverScreen(Hud hud, float x, float y) =>
            hud.mouseX >= x && hud.mouseX < x + PanelW && hud.mouseY >= y - 28f && hud.mouseY < y + PanelH + 28f;

        static void DrawSlotFrame(UiRenderer ui, float sx, float sy)
        {
            ui.Rect(sx, sy, 18f, 18f, Styles.SlotBg);
            ui.Rect(sx, sy, 18f, 1f, Styles.SlotDark);
            ui.Rect(sx, sy, 1f, 18f, Styles.SlotDark);
            ui.Rect(sx + 1f, sy + 17f, 17f, 1f, Styles.PanelLight);
            ui.Rect(sx + 17f, sy + 1f, 1f, 17f, Styles.PanelLight);
        }

        static bool Hover(Hud hud, float x, float y) => hud.mouseX >= x && hud.mouseX < x + 18f && hud.mouseY >= y && hud.mouseY < y + 18f;

        /// <summary>Cursor interaction for the player's own slots: pick up, place one, or shift-send to the hotbar.</summary>
        void ClickInto(ref ItemStack slot, Hud hud, bool disabled, Player p)
        {
            if (disabled || !hud.mouseClicked) return;
            if (slot != null && hud.mouseX < 0) return;
            if (carried == null)
            {
                if (slot == null) return;
                carried = slot;
                slot = null;
                Sounds.PlayUI("ui.button.click");
            }
            else
            {
                if (slot == null)
                {
                    slot = carried;
                    carried = null;
                    Sounds.PlayUI("ui.button.click");
                }
                else if (slot.item == carried.item)
                {
                    int n = Mathf.Min(carried.count, slot.MaxStack - slot.count);
                    slot.count += n; carried.count -= n;
                    if (carried.count <= 0) carried = null;
                }
            }
        }

        void ClearInventory(Player p)
        {
            Sounds.PlayUI("ui.button.click");
            for (int i = 0; i < 36; i++) p.inventory.main[i] = null;
            for (int i = 0; i < 4; i++) p.inventory.armor[i] = null;
            p.inventory.offhand = null;
            carried = null;
            if (p.inventoryMenu != null) p.inventoryMenu.carried = null;
        }
    }
}
