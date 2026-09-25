using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Terrain loading screen: a dirt-textured progress bar while the spawn chunks stream in.</summary>
    public sealed class LoadingScreen : GuiScreen
    {
        readonly string message;
        float t;
        public LoadingScreen(string message) { this.message = message; pauseGame = true; drawWorldBehind = true; dismissOnEscape = false; }
        public override void Update(float dt) { t += dt; }

        public override void Render(UiRenderer ui, Hud hud)
        {
            ui.Rect(0, 0, W, H, new Color32(20, 20, 24, 235));
            ui.Text("MCR", W * 0.5f, H * 0.22f, Styles.Text, 1, true, 3f);
            float p = Gm != null ? Gm.loadingProgress : 0f;
            float bw = Mathf.Min(240f, W - 40f), bh = 12f;
            float bx = (W - bw) * 0.5f, by = H * 0.5f;
            ui.Text(message, bx, by - 12f, Styles.TextGray, 1);
            ui.Rect(bx, by, bw, bh, new Color32(0, 0, 0, 200));
            ui.Outline(bx, by, bw, bh, Styles.PanelLight);
            int fill = Mathf.RoundToInt((bw - 2f) * Mathf.Clamp01(p));
            if (fill > 0)
            {
                ui.IconPart("flame_empty", bx + 1f, by + 1f, 0, 0, 1, 1, Styles.Text);
                ui.Rect(bx + 1f, by + 1f, fill, bh - 2f, new Color32(120, 170, 80, 255));
            }
            ui.Text(Mathf.RoundToInt(p * 100f) + "%", W * 0.5f, by + bh + 4f, Styles.Text, 1, true);
            ui.Text("Tip: hold Space in the air twice to fly in creative", W * 0.5f, H - 24f, Styles.TextGray, 1, true);
        }
    }

    /// <summary>Credits after the Ender Dragon is defeated.</summary>
    public sealed class CreditsScreen : GuiScreen
    {
        float scroll;
        static readonly string[] lines =
        {
            "End Poem",
            "",
            "The dragon is defeated.",
            "The world you built remains.",
            "",
            "MCR - a single-player recreation",
            "written in C# for Unity",
            "",
            "Procedural terrain, lighting and structures",
            "Procedural textures, models and sounds",
            "No original assets are used:",
            "every pixel and every note is generated in code",
            "",
            "Thanks for playing.",
            "",
            "",
        };
        public override void Update(float dt) { scroll += dt * 9f; }
        public override void Input(Hud hud) { if (hud.pressedEscape || hud.mouseClicked) hud.Pop(); }

        public override void Render(UiRenderer ui, Hud hud)
        {
            ui.Rect(0, 0, W, H, new Color32(0, 0, 0, 250));
            for (int i = 0; i < lines.Length; i++)
            {
                float y = H - scroll + i * 12f;
                if (y < -14f || y > H + 14f) continue;
                var c = i == 0 ? Styles.Yellow : Styles.Text;
                ui.Text(lines[i], W * 0.5f, y, c, 0, true);
            }
            if (scroll > lines.Length * 12f + H) hud.Pop();
            ui.Text("Press Esc to skip", W * 0.5f, H - 10f, Styles.TextGray, 1, true);
        }
    }
}
