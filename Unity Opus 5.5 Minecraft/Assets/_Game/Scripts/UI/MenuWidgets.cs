using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Per-menu widgets drawn over the generic container panel: furnace flame and arrow, brewing bubbles,
    /// enchanting offers, the anvil name field and cost, stonecutter and loom pickers, beacon powers and the
    /// villager trade list. Clicks on a widget are consumed here so the container never sees them.
    /// </summary>
    public static partial class MenuWidgets
    {
        /// <summary>True while a text field in a menu has focus (the screen must not treat E as "close").</summary>
        public static bool TextFocus;
        static readonly Color32 Dark = new Color32(64, 64, 64, 255);
        static readonly Color32 White = new Color32(255, 255, 255, 255);

        static bool Over(Hud hud, float x, float y, float w, float h) => hud.mouseX >= x && hud.mouseX < x + w && hud.mouseY >= y && hud.mouseY < y + h;

        static bool Click(Hud hud, float x, float y, float w, float h)
        {
            if (!hud.mouseClicked || !Over(hud, x, y, w, h)) return false;
            hud.mouseClicked = false;
            Sounds.PlayUI("ui.button.click");
            return true;
        }

        static partial void DrawSpecificBackground(ContainerScreen s, UiRenderer ui, Hud hud, Menu m, float ox, float oy)
        {
            TextFocus = m is AnvilMenu && anvilFocus;
            switch (m)
            {
                case FurnaceMenu f: Furnace(ui, f, ox, oy); break;
                case BrewingMenu b: Brewing(ui, b, ox, oy); break;
                case SmithingMenu _: Arrow(ui, ox + 68f, oy + 49f); break;
                case GrindstoneMenu _: Arrow(ui, ox + 94f, oy + 34f); break;
                case AnvilMenu a: Anvil(ui, hud, a, ox, oy); break;
            }
        }

        static partial void DrawSpecificForeground(ContainerScreen s, UiRenderer ui, Hud hud, Menu m, float ox, float oy)
        {
            switch (m)
            {
                case EnchantMenu e: Enchanting(ui, hud, e, ox, oy); break;
                case StonecutterMenu st: Stonecutter(ui, hud, st, ox, oy); break;
                case LoomMenu l: Loom(ui, hud, l, ox, oy); break;
                case BeaconMenu b: BeaconWidgets(ui, hud, b, ox, oy); break;
                case MerchantMenu mm: Merchant(ui, hud, mm, ox, oy); break;
            }
        }

        static void Arrow(UiRenderer ui, float x, float y) => ui.Icon("arrow_full", x, y, new Color32(140, 140, 140, 255));

        // ------------------------------------------------------------------ furnace
        static void Furnace(UiRenderer ui, FurnaceMenu f, float ox, float oy)
        {
            var fr = FurnaceMenu.FlameRect;
            ui.Icon("flame_empty", ox + fr.x, oy + fr.y);
            float burn = Mathf.Clamp01(f.BurnProgress);
            if (f.Lit && burn > 0f)
            {
                // the flame empties from the top as the fuel burns down
                int h = Mathf.CeilToInt(14 * burn);
                ui.IconPart("flame_full", ox + fr.x, oy + fr.y, 0, 14 - h, 14, h, White);
            }
            var ar = FurnaceMenu.ArrowRect;
            ui.Icon("arrow_empty", ox + ar.x, oy + ar.y);
            int w = Mathf.RoundToInt(22 * Mathf.Clamp01(f.CookProgress));
            if (w > 0) ui.IconPart("arrow_full", ox + ar.x, oy + ar.y, 0, 0, w, 15, White);
        }

        // ------------------------------------------------------------------ brewing stand
        static void Brewing(UiRenderer ui, BrewingMenu b, float ox, float oy)
        {
            ui.Icon("brew_bubbles_empty", ox + 63f, oy + 14f);
            ui.Icon("brew_arrow_empty", ox + 97f, oy + 16f);
            float prog = Mathf.Clamp01(b.BrewProgress);
            if (prog > 0f)
            {
                int h = Mathf.RoundToInt(28 * prog);
                ui.IconPart("brew_arrow_full", ox + 97f, oy + 16f, 0, 0, 9, h, White);
                int bh = Mathf.RoundToInt(29 * ((b.BubbleFrame + 1) / 7f));
                ui.IconPart("brew_bubbles_full", ox + 63f, oy + 14f, 0, 29 - bh, 12, bh, White);
            }
            ui.Rect(ox + 60f, oy + 44f, 18f, 4f, new Color32(40, 20, 10, 255));
            int fw = Mathf.RoundToInt(18 * Mathf.Clamp01(b.FuelLevel));
            if (fw > 0) ui.IconPart("brew_fuel", ox + 60f, oy + 44f, 0, 0, fw, 4, White);
        }

        // ------------------------------------------------------------------ anvil
        static bool anvilFocus;
        static void Anvil(UiRenderer ui, Hud hud, AnvilMenu a, float ox, float oy)
        {
            float fx = ox + 59f, fy = oy + 20f, fw = 110f, fh = 14f;
            ui.Rect(fx, fy, fw, fh, new Color32(0, 0, 0, 255));
            ui.Outline(fx, fy, fw, fh, anvilFocus ? White : new Color32(140, 140, 140, 255));
            string name = a.itemName ?? "";
            ui.Text(name + (anvilFocus && Mathf.FloorToInt(Time.time * 2f) % 2 == 0 ? "_" : ""), fx + 3f, fy + 3f, White, 1);
            if (hud.mouseClicked)
            {
                bool inField = Over(hud, fx, fy, fw, fh);
                if (inField) { anvilFocus = true; hud.mouseClicked = false; }
                else anvilFocus = false;
            }
            if (anvilFocus)
            {
                bool changed = false;
                foreach (var c in hud.typedChars)
                    if (c >= ' ' && name.Length < AnvilMenu.MaxNameLength) { name += c; changed = true; }
                if (hud.pressedBackspace && name.Length > 0) { name = name.Substring(0, name.Length - 1); changed = true; }
                if (hud.pressedEnter) anvilFocus = false;
                if (changed) a.SetItemName(name);
                hud.typedChars.Clear();
                hud.pressedE = false; hud.pressedQ = false; hud.pressedF = false; hud.numberKey = -1;
            }
            ui.Text("+", ox + 54f, oy + 49f, Dark, 0);
            if (a.cost > 0)
            {
                string label = a.tooExpensive ? "Too Expensive!" : "Enchantment Cost: " + a.cost;
                var col = a.tooExpensive || !a.CanAfford ? new Color32(255, 96, 96, 255) : new Color32(128, 255, 32, 255);
                float tw = ui.TextWidth(label);
                ui.Rect(ox + 168f - tw - 4f, oy + 67f, tw + 4f, 11f, new Color32(0, 0, 0, 110));
                ui.Text(label, ox + 168f - tw - 2f, oy + 69f, col, 1);
            }
        }

        // ------------------------------------------------------------------ enchanting table
        static void Enchanting(UiRenderer ui, Hud hud, EnchantMenu e, float ox, float oy)
        {
            // the open book is drawn as a small two-page glyph above the item slot
            ui.Rect(ox + 14f, oy + 16f, 36f, 22f, new Color32(92, 60, 40, 255));
            ui.Rect(ox + 16f, oy + 18f, 15f, 18f, new Color32(236, 226, 196, 255));
            ui.Rect(ox + 33f, oy + 18f, 15f, 18f, new Color32(226, 214, 184, 255));
            for (int i = 0; i < 3; i++)
            {
                float bx = ox + 60f, by = oy + 14f + i * 19f;
                EnchantMenu.EnchantOffer offer = null;
                foreach (var o in e.offers) if (o.id == i) offer = o;
                bool ok = offer != null && e.CanAfford(offer);
                bool hover = ok && Over(hud, bx, by, 108f, 19f);
                ui.Icon(offer == null ? "enchant_slot_off" : hover ? "enchant_slot_hover" : ok ? "enchant_slot" : "enchant_slot_off", bx, by);
                if (offer == null) continue;
                // the rune text is scrambled glyphs; the hint underneath names the headline enchantment
                ui.Text(Runes(offer.cost * 31 + i), bx + 20f, by + 2f, ok ? new Color32(104, 96, 64, 255) : new Color32(72, 64, 44, 255), 0);
                ui.Text(offer.cost.ToString(), bx + 104f - ui.TextWidth(offer.cost.ToString()), by + 9f, ok ? new Color32(128, 255, 32, 255) : new Color32(64, 128, 16, 255), 1);
                ui.Text((i + 1).ToString(), bx + 4f, by + 6f, ok ? new Color32(128, 255, 32, 255) : Dark, 1);
                if (hover)
                {
                    string hint = offer.enchant != null ? offer.enchant.name + " " + GuiScreen.Roman(offer.level) + " . . . ?" : "?";
                    hud.pendingTooltip = null;
                    ui.Rect(hud.mouseX + 8f, hud.mouseY - 12f, ui.TextWidth(hint) + 6f, 12f, Styles.TooltipBg);
                    ui.Text(hint, hud.mouseX + 11f, hud.mouseY - 10f, White, 1);
                }
                if (ok && Click(hud, bx, by, 108f, 19f)) e.ClickButton(i, 0);
            }
            if (e.bookshelves > 0) ui.Text("Bookshelves: " + e.bookshelves, ox + 8f, oy + 74f, Dark, 0);
        }

        static string Runes(int seed)
        {
            var rng = new RNG(seed);
            const string glyphs = "abcdefghijklmnopqrstuvwxyz";
            var sb = new System.Text.StringBuilder();
            int n = 8 + rng.Next(6);
            for (int i = 0; i < n; i++) { sb.Append(glyphs[rng.Next(glyphs.Length)]); if (rng.Next(5) == 0) sb.Append(' '); }
            return sb.ToString();
        }

        // ------------------------------------------------------------------ stonecutter
        static void Stonecutter(UiRenderer ui, Hud hud, StonecutterMenu st, float ox, float oy)
        {
            float gx = ox + 52f, gy = oy + 15f;
            ui.Rect(gx - 1f, gy - 1f, 66f, 56f, new Color32(0, 0, 0, 120));
            for (int i = 0; i < st.recipes.Count && i < 12; i++)
            {
                float x = gx + (i % 4) * 16f, y = gy + (i / 4) * 18f;
                bool sel = i == st.selectedRecipe;
                bool hover = Over(hud, x, y, 16f, 18f);
                ui.Rect(x, y, 16f, 18f, sel ? new Color32(90, 170, 90, 255) : hover ? new Color32(200, 200, 200, 255) : new Color32(160, 160, 160, 255));
                hud.DrawItemWithCount(ui, st.recipes[i].result, x, y + 1f, null);
                if (Click(hud, x, y, 16f, 18f)) st.SelectRecipe(i);
            }
        }

        // ------------------------------------------------------------------ loom
        static void Loom(UiRenderer ui, Hud hud, LoomMenu l, float ox, float oy)
        {
            float gx = ox + 60f, gy = oy + 13f;
            ui.Rect(gx - 1f, gy - 1f, 58f, 58f, new Color32(0, 0, 0, 120));
            for (int i = 0; i < l.patterns.Count && i < 16; i++)
            {
                float x = gx + (i % 4) * 14f, y = gy + (i / 4) * 14f;
                bool sel = i == l.selectedPattern;
                bool hover = Over(hud, x, y, 14f, 14f);
                ui.Rect(x, y, 14f, 14f, sel ? new Color32(90, 170, 90, 255) : hover ? new Color32(210, 210, 210, 255) : new Color32(170, 170, 170, 255));
                // each pattern shows its initial as a compact glyph on a white banner field
                ui.Rect(x + 3f, y + 1f, 8f, 12f, White);
                var pat = l.patterns[i];
                ui.Text(pat.Length > 0 ? pat.Substring(0, 1).ToUpperInvariant() : "?", x + 5f, y + 3f, Dark, 0);
                if (hover) hud.ShowActionBar(Blocks.PrettyName(pat));
                if (Click(hud, x, y, 14f, 14f)) l.SelectPattern(i);
            }
        }

        // ------------------------------------------------------------------ beacon
        static void BeaconWidgets(UiRenderer ui, Hud hud, BeaconMenu b, float ox, float oy)
        {
            var rows = Beacon.PowersByLevel;
            ui.Text("Primary Power", ox + 30f, oy + 10f, new Color32(224, 224, 224, 255), 1);
            ui.Text("Secondary Power", ox + 146f, oy + 10f, new Color32(224, 224, 224, 255), 1);
            for (int row = 0; row < rows.Length; row++)
            {
                bool unlocked = row < b.levels;
                for (int k = 0; k < rows[row].Length; k++)
                {
                    float x, y;
                    if (row < BeaconMenu.SecondaryTier) { x = ox + 30f + k * 24f; y = oy + 22f + row * 25f; }
                    else { x = ox + 146f + k * 24f; y = oy + 47f; }
                    string power = rows[row][k];
                    bool sel = row < BeaconMenu.SecondaryTier ? b.primary == power : b.secondary == power;
                    PowerButton(ui, hud, x, y, power, sel, unlocked, () => b.ClickButton(row, k));
                }
            }
            if (b.levels > BeaconMenu.SecondaryTier && b.primary != null)
            {
                float x = ox + 146f + rows[Mathf.Min(BeaconMenu.SecondaryTier, rows.Length - 1)].Length * 24f, y = oy + 47f;
                PowerButton(ui, hud, x, y, b.primary, b.secondary == b.primary, true, () => b.ClickButton(BeaconMenu.SecondaryTier, BeaconMenu.UpgradePrimary));
                ui.Text("II", x + 14f, y + 13f, White, 1);
            }
            // confirm / cancel
            float cx = ox + 164f, cy = oy + 107f;
            bool can = b.CanConfirm;
            ui.Rect(cx, cy, 22f, 22f, can ? Styles.Button : Styles.ButtonDisabled);
            ui.Outline(cx, cy, 22f, 22f, Styles.PanelLight);
            ui.Icon("check", cx + 5f, cy + 7f, can ? White : Styles.TextGray);
            if (can && Click(hud, cx, cy, 22f, 22f)) b.ClickButton(BeaconMenu.Confirm, 0);
            float xx = ox + 190f;
            ui.Rect(xx, cy, 22f, 22f, Styles.Button);
            ui.Outline(xx, cy, 22f, 22f, Styles.PanelLight);
            ui.Icon("cross", xx + 6f, cy + 7f, White);
            if (Click(hud, xx, cy, 22f, 22f)) { b.ClickButton(BeaconMenu.Cancel, 0); hud.pressedEscape = true; }
            ui.Text("Pyramid levels: " + b.levels, ox + 10f, oy + 112f, new Color32(224, 224, 224, 255), 1);
        }

        static void PowerButton(UiRenderer ui, Hud hud, float x, float y, string power, bool selected, bool unlocked, System.Action onClick)
        {
            var eff = Effect.Get(power);
            var col = !unlocked ? Styles.ButtonDisabled : selected ? new Color32(96, 176, 96, 255) : Over(hud, x, y, 22f, 22f) ? Styles.ButtonHi : Styles.Button;
            ui.Rect(x, y, 22f, 22f, col);
            ui.Outline(x, y, 22f, 22f, selected ? Styles.Yellow : Styles.PanelLight);
            if (eff != null) ui.Sprite(ItemIcons.Atlas, x + 2f, y + 2f, 18f, 18f, ItemIcons.EffectUV(eff), unlocked ? White : new Color32(120, 120, 120, 255));
            if (Over(hud, x, y, 22f, 22f) && eff != null) hud.ShowActionBar(eff.name);
            if (unlocked && Click(hud, x, y, 22f, 22f)) onClick();
        }

        // ------------------------------------------------------------------ villager trading
        static void Merchant(UiRenderer ui, Hud hud, MerchantMenu mm, float ox, float oy)
        {
            // offers list on the left
            float lx = ox + 5f, ly = oy + 16f;
            ui.Rect(lx - 1f, ly - 1f, 89f, MerchantMenu.VisibleRows * 20f + 2f, new Color32(0, 0, 0, 90));
            var offers = mm.Offers;
            if (hud.scrollDelta != 0 && Over(hud, lx, ly, 89f, MerchantMenu.VisibleRows * 20f)) { mm.Scroll(-hud.scrollDelta); hud.scrollDelta = 0; }
            for (int row = 0; row < MerchantMenu.VisibleRows; row++)
            {
                var o = mm.RowOffer(row);
                if (o == null) break;
                int index = mm.scroll + row;
                float y = ly + row * 20f;
                bool sel = index == mm.selected;
                bool hover = Over(hud, lx, y, 88f, 20f);
                bool disabled = o.uses >= o.maxUses;
                ui.Rect(lx, y, 88f, 19f, sel ? new Color32(210, 210, 150, 255) : hover ? new Color32(210, 210, 210, 255) : new Color32(170, 170, 170, 255));
                hud.DrawItemWithCount(ui, o.costA, lx + 2f, y + 1f, null);
                if (o.costB != null && !o.costB.IsEmpty) hud.DrawItemWithCount(ui, o.costB, lx + 22f, y + 1f, null);
                ui.Icon(disabled ? "trade_x" : "trade_arrow", lx + 46f, y + 5f, disabled ? White : Dark);
                hud.DrawItemWithCount(ui, o.result, lx + 66f, y + 1f, null);
                if (hover) hud.pendingTooltip = o.result;
                if (Click(hud, lx, y, 88f, 19f)) mm.SelectOffer(index);
            }
            // level bar
            if (mm.ShowLevel)
            {
                float bx = ox + 136f, by = oy + 16f;
                ui.Text(mm.LevelName, ox + 190f, oy + 5f, Dark, 0, true);
                ui.Rect(bx, by, 102f, 5f, new Color32(0, 0, 0, 255));
                ui.Rect(bx + 1f, by + 1f, 100f * Mathf.Clamp01(mm.LevelProgress), 3f, new Color32(128, 255, 32, 255));
            }
        }
    }
}
