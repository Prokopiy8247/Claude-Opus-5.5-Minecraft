using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>The F3+F4 game mode switcher: a full-screen row of mode buttons with the current one highlighted.</summary>
    public sealed class GameModeSwitcherScreen : GuiScreen
    {
        static readonly GameMode[] modes = { GameMode.Survival, GameMode.Creative, GameMode.Adventure, GameMode.Spectator };
        static readonly string[] blurbs =
        {
            "Collect resources, manage health and hunger, fight mobs.",
            "Unlimited blocks, instant breaking, free flight, no damage.",
            "Explore a map you cannot alter; use levers and doors.",
            "Fly through everything; no interaction with the world.",
        };
        int selected;

        public override void OnOpen()
        {
            var p = P;
            selected = p != null ? System.Array.IndexOf(modes, p.gameMode) : 0;
            pauseGame = true; drawWorldBehind = true; dismissOnEscape = true;
        }

        public override void Input(Hud hud)
        {
            if (hud.pressedEscape) hud.Pop();
            if (hud.keyLeft) selected = (selected + modes.Length - 1) % modes.Length;
            if (hud.keyRight) selected = (selected + 1) % modes.Length;
            if (hud.pressedEnter) Apply();
        }

        void Apply()
        {
            var p = P;
            if (p == null) { hud.Pop(); return; }
            p.SetGameMode(modes[selected]);
            Gm.session.defaultMode = modes[selected];
            hud.Chat("Game mode set to " + modes[selected]);
            hud.Pop();
        }

        public override void Render(UiRenderer ui, Hud hud)
        {
            Dim(0.7f);
            ui.Text("Game Mode Switcher", W * 0.5f, 14f, Styles.Text, 1, true);
            ui.Text("F3+F4 - arrow keys or click to choose - Enter to confirm", W * 0.5f, 26f, Styles.TextGray, 1, true);
            float bw = Mathf.Min(150f, (W - 30f) / 4f), bh = 150f;
            float total = bw * 4 + 18f;
            float x0 = (W - total) * 0.5f, y0 = (H - bh) * 0.5f;
            for (int i = 0; i < 4; i++)
            {
                float x = x0 + i * (bw + 6f);
                bool sel = i == selected;
                bool hover = hud.mouseX >= x && hud.mouseX < x + bw && hud.mouseY >= y0 && hud.mouseY < y0 + bh;
                ui.Rect(x, y0, bw, bh, sel ? new Color32(200, 200, 210, 255) : hover ? new Color32(150, 150, 160, 255) : new Color32(110, 110, 118, 255));
                ui.Outline(x, y0, bw, bh, sel ? Styles.Yellow : Styles.TextGray, sel ? 2f : 1f);
                DrawModeIcon(ui, modes[i], x + bw * 0.5f - 16f, y0 + 12f);
                ui.Text(modes[i].ToString(), x + bw * 0.5f, y0 + 62f, sel ? new Color32(0, 0, 0, 255) : Styles.Text, 1, true);
                foreach (var line in UiRenderer.Wrap(blurbs[i], bw - 12f))
                    ui.Text(line, x + bw * 0.5f, y0 + 76f + 10f * System.Array.IndexOf(UiRenderer.Wrap(blurbs[i], bw - 12f).ToArray(), line), sel ? new Color32(40, 40, 40, 255) : Styles.TextGray, 0, true);
                if (sel || hover) ui.Text(sel ? "Selected" : "Click", x + bw * 0.5f, y0 + bh - 14f, sel ? new Color32(0, 0, 0, 255) : Styles.Text, 1, true);
                if (hover && hud.mouseClicked) { selected = i; Apply(); return; }
            }
            ui.Text("Changing to spectator keeps your position; switching back restores your inventory.", W * 0.5f, H - 20f, Styles.TextGray, 1, true);
        }

        /// <summary>Small pictogram per mode, drawn from atlas parts so no new artwork is needed.</summary>
        static void DrawModeIcon(UiRenderer ui, GameMode m, float x, float y)
        {
            switch (m)
            {
                case GameMode.Survival:
                    ui.Icon("heart_full", x + 2f, y + 4f, new Color32(255, 255, 255, 255), 2f);
                    ui.Icon("food_full", x + 16f, y + 8f, new Color32(255, 255, 255, 255), 2f);
                    break;
                case GameMode.Creative:
                    ui.Icon("flame_full", x + 8f, y, new Color32(255, 255, 255, 255), 1.6f);
                    break;
                case GameMode.Adventure:
                    ui.Icon("armor_full", x + 6f, y + 2f, new Color32(255, 255, 255, 255), 2.4f);
                    break;
                default:
                    ui.Icon("air_full", x + 6f, y + 4f, new Color32(255, 255, 255, 255), 2.4f);
                    break;
            }
        }
    }

    /// <summary>Advancements list (opened with L): original goals that the gameplay code reports through Achievements.</summary>
    public sealed class AdvancementsScreen : GuiScreen
    {
        int scroll;
        public override void OnOpen() { title = "Advancements"; pauseGame = true; }
        public override void Input(Hud hud)
        {
            if (hud.pressedEscape) hud.Pop();
            if (hud.scrollDelta != 0) scroll = Mathf.Clamp(scroll - hud.scrollDelta, 0, Mathf.Max(0, Achievements.All.Count - 12));
        }

        public override void Render(UiRenderer ui, Hud hud)
        {
            Dim(0.7f);
            float w = Mathf.Min(300f, W - 20f), h = Mathf.Min(200f, H - 20f);
            float x = (W - w) * 0.5f, y = (H - h) * 0.5f;
            Panel(x, y, w, h);
            int done = Achievements.CompletedCount;
            ui.Text(title + "   " + done + " / " + Achievements.All.Count, x + w * 0.5f, y + 6f, Styles.Text, 1, true);
            float listY = y + 20f;
            ui.Rect(x + 6f, listY, w - 12f, h - 44f, new Color32(0, 0, 0, 130));
            ui.PushClip(x + 6f, listY, w - 12f, h - 44f);
            for (int i = 0; i < 12 && i + scroll < Achievements.All.Count; i++)
            {
                var a = Achievements.All[i + scroll];
                float ry = listY + 2f + i * 14f;
                bool got = Achievements.Has(a.id);
                ui.Icon(got ? "check" : "cross", x + 10f, ry + 2f, got ? Styles.Green : Styles.TextGray);
                ui.Text(a.title, x + 24f, ry + 2f, got ? Styles.Green : Styles.Text, 1);
                ui.Text(got ? "Earned" : a.hint, x + w - 78f, ry + 2f, Styles.TextGray, 0);
            }
            ui.PopClip();
            if (Button(x + w - 70f, y + h - 20f, 60f, 16f, "Close")) hud.Pop();
        }
    }

    /// <summary>Simple key-remapping-free control list shown as a help overlay (F3+Q).</summary>
    public sealed class HelpScreen : GuiScreen
    {
        public override void OnOpen() { pauseGame = false; }
        public override void Render(UiRenderer ui, Hud hud)
        {
            var binds = Gm.KeyBindings;
            float w = 230f, h = binds.Count * 11f + 26f;
            float x = (W - w) * 0.5f, y = (H - h) * 0.5f;
            Panel(x, y, w, h);
            ui.Text("Controls", x + w * 0.5f, y + 6f, Styles.Text, 1, true);
            int i = 0;
            foreach (var kv in binds)
            {
                ui.Text(kv.Key, x + 10f, y + 20f + i * 11f, Styles.TextGray, 1);
                ui.Text(kv.Value, x + w - 10f, y + 20f + i * 11f, Styles.Text, 1, true);
                i++;
            }
            if (hud.pressedEscape || hud.pressedQ) hud.Pop();
        }
    }
}
