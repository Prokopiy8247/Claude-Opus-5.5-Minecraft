using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Generic renderer and mouse handler for every <see cref="Menu"/>: draws the panel, the slots and any
    /// per-menu widgets (progress arrows, flames, option buttons), and turns clicks, shift-clicks, drags,
    /// double-clicks, number keys and Q into the menu's click protocol.
    /// </summary>
    public class ContainerScreen : GuiScreen
    {
        public readonly Menu menu;
        protected float ox, oy;
        Slot hoverSlot;
        float lastClickTime;
        Slot lastClickSlot;
        bool dragging;
        int dragButton;

        public ContainerScreen(Menu m) { menu = m; pauseGame = false; title = m.title; }

        public override void OnClose()
        {
            var p = P;
            if (p != null && p.menu == menu) p.CloseMenu();
        }

        public override void Input(Hud hud)
        {
            var p = P;
            if (p == null) return;
            if (hud.pressedEscape || (hud.pressedE && !MenuWidgets.TextFocus))
            {
                Sounds.PlayUI("ui.button.click");
                p.CloseMenu();
                hud.RemoveScreens<ContainerScreen>();
                return;
            }
            if (!menu.StillValid()) { p.CloseMenu(); hud.RemoveScreens<ContainerScreen>(); return; }
            if (hoverSlot != null && !MenuWidgets.TextFocus)
            {
                if (hud.numberKey >= 0) { menu.Click(hoverSlot.menuIndex, hud.numberKey, ClickType.Swap); }
                if (hud.pressedF) menu.Click(hoverSlot.menuIndex, 40, ClickType.Swap);
                if (hud.pressedQ) menu.Click(hoverSlot.menuIndex, hud.ctrl ? 1 : 0, ClickType.Throw);
                if (hud.mouseClickedMiddle && p.IsCreative) menu.Click(hoverSlot.menuIndex, 2, ClickType.Clone);
            }
        }

        public override void Update(float dt) { menu.Tick(); }

        public override void Render(UiRenderer ui, Hud hud)
        {
            var p = P;
            Dim(0.5f);
            float w = menu.width, h = menu.height;
            ox = Mathf.Floor((W - w) * 0.5f); oy = Mathf.Floor((H - h) * 0.5f);
            Panel(ox, oy, w, h);
            DrawBackground(ui, hud);
            // the player inventory has no title of its own (its "Crafting" label is drawn with the grid)
            if (!(menu is InventoryMenu)) ui.Text(menu.title, ox + 8f, oy + 6f, new Color32(64, 64, 64, 255), 0);
            if (menu.background != "inventory" && menu.background != "crafting_table" || menu is CraftingTableMenu)
            {
                float invLabel = PlayerInvLabelY();
                if (invLabel > 0) ui.Text("Inventory", ox + 8f, oy + invLabel, new Color32(64, 64, 64, 255), 0);
            }

            hoverSlot = null;
            foreach (var s in menu.slots) if (s.active) DrawSlot(menu, s, ox, oy, ref hoverSlot);
            DrawForeground(ui, hud);
            HandleMouse(hud);
            DrawCarriedAndTooltip(menu, hoverSlot);
        }

        /// <summary>Y offset (relative to the panel) of the "Inventory" caption, derived from the first player slot.</summary>
        float PlayerInvLabelY()
        {
            foreach (var s in menu.slots)
                if (s.container is PlayerInventory && s.index >= 9 && s.index < 36) return s.y - 11f;
            return -1f;
        }

        // ------------------------------------------------------------------ mouse
        void HandleMouse(Hud hud)
        {
            bool inside = hud.mouseX >= ox && hud.mouseX < ox + menu.width && hud.mouseY >= oy && hud.mouseY < oy + menu.height;
            // drag distribution: holding a button over several slots while carrying a stack
            if (dragging)
            {
                if (hoverSlot != null) menu.DragOver(hoverSlot);
                bool released = dragButton == 0 ? hud.mouseReleased : hud.mouseReleasedRight;
                if (released) { menu.EndDrag(); dragging = false; }
                return;
            }
            if (hud.mouseClicked || hud.mouseClickedRight)
            {
                int button = hud.mouseClicked ? 0 : 1;
                if (!inside && hoverSlot == null && !OverWidget(hud))
                {
                    menu.Click(-999, button, ClickType.Pickup);
                    return;
                }
                if (hoverSlot == null) { OnWidgetClick(hud, button); return; }
                bool dbl = button == 0 && lastClickSlot == hoverSlot && Time.unscaledTime - lastClickTime < 0.3f;
                lastClickSlot = hoverSlot; lastClickTime = Time.unscaledTime;
                if (dbl && menu.carried != null) { menu.Click(hoverSlot.menuIndex, 0, ClickType.PickupAll); return; }
                if (hud.shift) { menu.Click(hoverSlot.menuIndex, button, ClickType.QuickMove); Sounds.PlayUI("ui.button.click", 0.2f); return; }
                if (menu.carried != null && !hoverSlot.isResult && (!hoverSlot.HasItem || hoverSlot.Item.Stackable(menu.carried)))
                {
                    // start a potential drag; a click without movement resolves as a normal pickup/place in EndDrag
                    menu.BeginDrag(button);
                    menu.DragOver(hoverSlot);
                    dragging = true; dragButton = button;
                    return;
                }
                menu.Click(hoverSlot.menuIndex, button, ClickType.Pickup);
            }
        }

        protected virtual bool OverWidget(Hud hud) => false;
        protected virtual void OnWidgetClick(Hud hud, int button) { }

        // ------------------------------------------------------------------ per-menu decorations
        protected virtual void DrawBackground(UiRenderer ui, Hud hud)
        {
            if (menu is InventoryMenu)
            {
                // player preview box and the 2x2 crafting arrow
                ui.Rect(ox + 26f, oy + 8f, 51f, 72f, new Color32(0, 0, 0, 255));
                ui.Rect(ox + 27f, oy + 9f, 49f, 70f, new Color32(0, 0, 0, 255));
                PlayerPreview.Draw(ui, P, ox + 27f, oy + 9f, 49f, 70f, hud.mouseX, hud.mouseY);
                ui.Icon("arrow_full", ox + 134f, oy + 28f, new Color32(140, 140, 140, 255));
                ui.Text("Crafting", ox + 97f, oy + 6f, new Color32(64, 64, 64, 255), 0);
            }
            else if (menu is CraftingTableMenu)
            {
                ui.Icon("arrow_full", ox + 90f, oy + 35f, new Color32(140, 140, 140, 255));
            }
            MenuWidgets.DrawBackground(this, ui, hud, menu, ox, oy);
        }

        protected virtual void DrawForeground(UiRenderer ui, Hud hud)
        {
            MenuWidgets.DrawForeground(this, ui, hud, menu, ox, oy);
        }

        public bool ButtonAt(float x, float y, float w, float h, string label, bool enabled = true) => Button(ox + x, oy + y, w, h, label, enabled);
    }

    /// <summary>
    /// Menu-specific widgets (progress bars, option buttons, recipe lists). Kept apart from the generic screen so
    /// new menu types only need a branch here; unknown menus still work with slots alone.
    /// </summary>
    public static partial class MenuWidgets
    {
        static readonly Color32 Label = new Color32(64, 64, 64, 255);

        public static void DrawBackground(ContainerScreen s, UiRenderer ui, Hud hud, Menu m, float ox, float oy)
        {
            DrawSpecificBackground(s, ui, hud, m, ox, oy);
        }

        public static void DrawForeground(ContainerScreen s, UiRenderer ui, Hud hud, Menu m, float ox, float oy)
        {
            DrawSpecificForeground(s, ui, hud, m, ox, oy);
        }

        static partial void DrawSpecificBackground(ContainerScreen s, UiRenderer ui, Hud hud, Menu m, float ox, float oy);
        static partial void DrawSpecificForeground(ContainerScreen s, UiRenderer ui, Hud hud, Menu m, float ox, float oy);
    }
}
