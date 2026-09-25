using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Base class for a full-screen or overlay interface. Screens are pushed onto the HUD's stack.</summary>
    public abstract class GuiScreen
    {
        public Hud hud;
        public bool pauseGame = true;
        public bool drawWorldBehind = true;
        public bool dismissOnEscape = true;
        public string title = "";

        protected float W => hud.W;
        protected float H => hud.H;
        protected UiRenderer ui => hud.ui;
        protected GameManager Gm => GameManager.Instance;
        protected Player P => GameManager.Instance?.player;

        public virtual void OnOpen() { }
        public virtual void OnClose() { }
        public virtual void Update(float dt) { }
        public virtual void Input(Hud h) { }
        public abstract void Render(UiRenderer ui, Hud hud);

        // ------------------------------------------------------------------ shared helpers
        protected void Dim(float alpha = 0.45f) => ui.Rect(0, 0, W, H, new Color32(0, 0, 0, (byte)(alpha * 255)));

        /// <summary>Container panel: light grey fill, a rounded black rim, white bevel top-left and dark bevel bottom-right.</summary>
        protected void Panel(float x, float y, float w, float h, string bg = null) => DrawPanel(ui, x, y, w, h);

        public static void DrawPanel(UiRenderer ui, float x, float y, float w, float h)
        {
            var rim = new Color32(0, 0, 0, 255);
            var shade = new Color32(85, 85, 85, 255);
            ui.Rect(x + 2f, y, w - 4f, 1f, rim); ui.Rect(x + 2f, y + h - 1f, w - 4f, 1f, rim);
            ui.Rect(x, y + 2f, 1f, h - 4f, rim); ui.Rect(x + w - 1f, y + 2f, 1f, h - 4f, rim);
            ui.Rect(x + 1f, y + 1f, 1f, 1f, rim); ui.Rect(x + w - 2f, y + 1f, 1f, 1f, rim);
            ui.Rect(x + 1f, y + h - 2f, 1f, 1f, rim); ui.Rect(x + w - 2f, y + h - 2f, 1f, 1f, rim);
            ui.Rect(x + 1f, y + 1f, w - 2f, h - 2f, Styles.Panel);
            ui.Rect(x + 2f, y + 1f, w - 5f, 2f, Styles.PanelLight); ui.Rect(x + 1f, y + 2f, 2f, h - 5f, Styles.PanelLight);
            ui.Rect(x + 3f, y + h - 3f, w - 5f, 2f, shade); ui.Rect(x + w - 3f, y + 3f, 2f, h - 5f, shade);
        }

        /// <summary>Stone-grey button with a dark rim; the rim turns white under the mouse.</summary>
        protected bool Button(float x, float y, float w, float h, string label, bool enabled = true)
        {
            bool hover = enabled && hud.mouseX >= x && hud.mouseX < x + w && hud.mouseY >= y && hud.mouseY < y + h;
            DrawButtonFace(ui, x, y, w, h, enabled, hover);
            var labelColor = enabled ? Styles.Text : new Color32(160, 160, 160, 255);
            ui.Text(label, x + w * 0.5f, y + (h - 8f) * 0.5f, labelColor, 1, true);
            return enabled && hover && hud.mouseClicked;
        }

        public static void DrawButtonFace(UiRenderer ui, float x, float y, float w, float h, bool enabled, bool hover)
        {
            ui.Rect(x, y, w, h, hover ? Styles.Text : new Color32(0, 0, 0, 255));
            if (!enabled)
            {
                ui.Rect(x + 1f, y + 1f, w - 2f, h - 2f, new Color32(44, 44, 44, 255));
                return;
            }
            ui.Rect(x + 1f, y + 1f, w - 2f, h - 2f, new Color32(112, 112, 112, 255));
            ui.Rect(x + 1f, y + 1f, w - 2f, 1f, new Color32(166, 166, 166, 255));
            ui.Rect(x + 1f, y + 2f, 1f, h - 3f, new Color32(150, 150, 150, 255));
            ui.Rect(x + 2f, y + h - 3f, w - 3f, 2f, new Color32(76, 76, 76, 255));
            ui.Rect(x + w - 2f, y + 2f, 1f, h - 4f, new Color32(88, 88, 88, 255));
            // a faint stone speckle so wide buttons do not look flat
            for (int i = 0; i < (int)(w * h / 90f); i++)
            {
                uint hsh = (uint)(i * 2654435761u) ^ (uint)(w * 131f) ^ (uint)(h * 7f);
                float px = x + 2f + (hsh % 997) / 997f * (w - 5f), py = y + 2f + ((hsh >> 10) % 991) / 991f * (h - 6f);
                ui.Rect(Mathf.Floor(px), Mathf.Floor(py), 1f, 1f, (hsh & 1) == 0 ? new Color32(124, 124, 124, 255) : new Color32(100, 100, 100, 255));
            }
        }

        protected bool IconButton(float x, float y, float w, float h, string icon, bool enabled = true)
        {
            bool hover = enabled && hud.mouseX >= x && hud.mouseX < x + w && hud.mouseY >= y && hud.mouseY < y + h;
            DrawButtonFace(ui, x, y, w, h, enabled, hover);
            var r = UiAtlas.Region(icon);
            ui.Icon(icon, x + (w - r.width) * 0.5f, y + (h - r.height) * 0.5f, enabled ? Styles.Text : Styles.TextGray);
            return enabled && hover && hud.mouseClicked;
        }

        /// <summary>A clickable text option that highlights on hover (used by the title screen and lists).</summary>
        protected bool TextButton(float x, float y, float w, string label, Color32? baseColor = null)
        {
            bool hover = hud.mouseX >= x && hud.mouseX < x + w && hud.mouseY >= y && hud.mouseY < y + 9f;
            var c = baseColor ?? (hover ? Styles.Yellow : Styles.Text);
            if (hover) ui.Text("> " + label + " <", x + w * 0.5f, y, Styles.Yellow, 1, true);
            else ui.Text(label, x + w * 0.5f, y, c, 1, true);
            return hover && hud.mouseClicked;
        }

        // ------------------------------------------------------------------ slots
        /// <summary>Draws one menu slot at its menu-relative position, plus its item, hover highlight and tooltip request.</summary>
        protected void DrawSlot(Menu menu, Slot slot, float ox, float oy, ref Slot hoverSlot)
        {
            float x = ox + slot.x, y = oy + slot.y;
            ui.Rect(x, y, 18f, 18f, Styles.SlotBg);
            ui.Rect(x, y, 18f, 1f, Styles.SlotDark);
            ui.Rect(x, y, 1f, 18f, Styles.SlotDark);
            ui.Rect(x + 1f, y + 17f, 17f, 1f, Styles.PanelLight);
            ui.Rect(x + 17f, y + 1f, 1f, 17f, Styles.PanelLight);
            if (slot.emptyIcon != null && !slot.HasItem) DrawEmptyIcon(slot.emptyIcon, x + 1f, y + 1f);
            if (slot.HasItem)
            {
                hud.DrawItemWithCount(ui, slot.Item, x + 1f, y + 1f, P);
                int drag = menu.DragPreview(slot);
                if (drag >= 0) ui.Text(drag.ToString(), x + 1f, y + 10f, Styles.Text, 1);
                if (menu.dragSlots.Contains(slot)) ui.Rect(x, y, 18f, 18f, new Color32(255, 255, 255, 70));
            }
            bool hover = hud.mouseX >= x && hud.mouseX < x + 18f && hud.mouseY >= y && hud.mouseY < y + 18f;
            if (hover)
            {
                hoverSlot = slot;
                ui.Rect(x, y, 18f, 18f, Styles.SlotHighlight);
            }
        }

        /// <summary>
        /// Silhouette of an empty slot. The armour and shield outlines are item sprites ("item/empty_armor_slot_*",
        /// already faded by ItemSprites), not UiAtlas regions, so they come from the item icon atlas.
        /// </summary>
        void DrawEmptyIcon(string name, float x, float y)
        {
            if (UiAtlas.Has(name)) { ui.Icon(name, x, y, new Color32(255, 255, 255, 90)); return; }
            if (!Tex.Has("item/" + name)) return;
            var uv = ItemIcons.UV("i:" + name); // builds the atlas on first use, so read Atlas after it
            ui.Sprite(ItemIcons.Atlas, x, y, 16f, 16f, uv, new Color32(255, 255, 255, 255));
        }

        /// <summary>Renders the carried stack under the cursor and the tooltip for the hovered slot.</summary>
        protected void DrawCarriedAndTooltip(Menu menu, Slot hoverSlot)
        {
            if (menu.carried != null && !menu.carried.IsEmpty) hud.DrawItemWithCount(ui, menu.carried, hud.mouseX - 8f, hud.mouseY - 8f, P);
            if (hoverSlot != null && hoverSlot.HasItem) DrawTooltip(hoverSlot.Item, hud.mouseX + 8f, hud.mouseY + 8f);
            else if (hoverSlot != null && hoverSlot.emptyIcon != null) { }
        }

        public void DrawTooltip(ItemStack s, float x, float y)
        {
            if (s == null || s.IsEmpty) return;
            var lines = new List<string>();
            lines.Add(s.DisplayName);
            var extra = new List<string>();
            s.item.AppendTooltip(s, extra);
            lines.AddRange(extra);
            if (s.enchants != null && s.enchants.Count > 0)
                foreach (var kv in s.enchants) lines.Add("§7" + kv.Key.name + (kv.Value > 1 ? " " + Roman(kv.Value) : ""));
            if (s.MaxDamage > 0 && s.damage > 0) lines.Add("§7Durability: " + (s.MaxDamage - s.damage) + " / " + s.MaxDamage);
            if (s.item.block != null) lines.Add("§8" + s.item.block.id);
            DrawTooltipBlock(lines, x, y);
        }

        void DrawTooltipBlock(List<string> lines, float x, float y)
        {
            float w = 0;
            foreach (var l in lines) w = Mathf.Max(w, ui.TextWidth(l));
            w += 6f;
            float h = lines.Count * 10f + 4f;
            if (x + w > W - 2f) x = W - w - 2f;
            if (y + h > H - 2f) y = H - h - 2f;
            ui.Rect(x, y, w, h, Styles.TooltipBg);
            ui.Rect(x, y, w, 1f, Styles.TooltipBorderTop);
            ui.Rect(x, y + h - 1f, w, 1f, Styles.TooltipBorderBottom);
            ui.Rect(x, y + 1f, 1f, h - 2f, Styles.TooltipBorderBottom);
            ui.Rect(x + w - 1f, y + 1f, 1f, h - 2f, Styles.TooltipBorderBottom);
            for (int i = 0; i < lines.Count; i++) ui.Text(lines[i], x + 3f, y + 2f + i * 10f, Styles.Text, 1);
        }

        public static string Roman(int n) => n <= 1 ? "" : n <= 3 ? new string('I', n) : n == 4 ? "IV" : n == 5 ? "V" : n.ToString();
    }
}
