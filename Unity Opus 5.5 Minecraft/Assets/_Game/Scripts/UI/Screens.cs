using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Title screen: single player, options, quit, plus the world picker and world creation.</summary>
    public sealed class TitleScreen : GuiScreen
    {
        float t;
        readonly List<string> logo = new List<string>();
        public TitleScreen() { pauseGame = true; drawWorldBehind = false; dismissOnEscape = false; }

        public override void Update(float dt) { t += dt; }

        public override void Render(UiRenderer ui, Hud hud)
        {
            if (TitleArt.HasPanorama)
            {
                // the world camera draws the turning panorama behind; a light veil keeps the buttons readable
                ui.Rect(0, 0, W, H, new Color32(0, 0, 0, 40));
            }
            else
            {
                // no captured panorama shipped: a slowly drifting sky so the title never looks static
                var bg = new Color32((byte)(40 + 12 * Mathf.Sin(t * 0.3f)), (byte)(70 + 20 * Mathf.Sin(t * 0.3f + 1f)), 190, 255);
                ui.Rect(0, 0, W, H, bg);
                for (int i = 0; i < 60; i++)
                {
                    float x = ((i * 137.5f + t * 6f) % (W + 40f)) - 20f;
                    float y = (i * 53f) % H;
                    ui.Rect(x, y, 1f, 1f, new Color32(255, 255, 255, (byte)(120 + 80 * Mathf.Sin(t + i))));
                }
                ui.Rect(0, H * 0.68f, W, H * 0.32f, new Color32(30, 50, 90, 255));
            }

            // stone logo built from the interface font, with a pulsing splash line tucked under its right end
            var logo = TitleArt.Logo("MINECRAFT");
            float lw = Mathf.Min(W * 0.8f, 274f), lh = lw * logo.height / (float)logo.width;
            float lx = (W - lw) * 0.5f, ly = H * 0.10f;
            ui.Sprite(logo, lx, ly, lw, lh, 0f, 1f, 1f, 0f, new Color32(255, 255, 255, 255));
            ui.Text("JAVA EDITION RECREATION", W * 0.5f, ly + lh + 3f, new Color32(220, 220, 220, 255), 1, true);
            float pulse = 1f + Mathf.Abs(Mathf.Sin(t * 5f)) * 0.08f;
            ui.Text(Splash, lx + lw * 0.78f, ly + lh - 6f, Styles.Yellow, 1, true, pulse);

            float bx = W * 0.5f - 100f, by = H * 0.45f;
            if (Button(bx, by, 200f, 20f, "Singleplayer")) { Sounds.PlayUI("ui.button.click"); hud.Replace(new WorldListScreen()); }
            if (Button(bx, by + 24f, 200f, 20f, "Create New World")) { Sounds.PlayUI("ui.button.click"); hud.Replace(new CreateWorldScreen()); }
            if (Button(bx, by + 48f, 200f, 20f, "Options...")) { Sounds.PlayUI("ui.button.click"); hud.Push(new OptionsScreen()); }
            if (Button(bx, by + 72f, 200f, 20f, "Quit Game")) Quit();

            ui.Text(BuildInfo(), 2f, H - 10f, Styles.Text, 1);
            string note = "Original assets, not affiliated with Mojang";
            ui.Text(note, W - ui.TextWidth(note) - 1f, H - 10f, Styles.Text, 1);
        }

        public static string BuildInfo() => "MCR " + GameManager.Version + " - Unity " + Application.unityVersion;

        static readonly string[] splashes =
        {
            "Also try building!", "Made of blocks!", "Now with dragons!", "100% procedural!", "Punch a tree!",
            "Mind the creepers!", "Redstone inside!", "Infinite-ish!", "Pixel perfect!", "Crafted in Unity!",
        };
        static string splash;
        static string Splash => splash ?? (splash = splashes[Random.Range(0, splashes.Length)]);

        void Quit()
        {
            Application.Quit();
        }
    }

    /// <summary>Lists the worlds in the saves folder with play / delete / recreate actions.</summary>
    public sealed class WorldListScreen : GuiScreen
    {
        List<WorldInfo> list = new List<WorldInfo>();
        int selected;
        int scroll;
        bool confirmDelete;

        public override void OnOpen()
        {
            title = "Select World";
            Refresh();
        }

        void Refresh()
        {
            list = SaveManager.ListWorlds();
            SimulateMissing();
        }

        /// <summary>Offer a couple of ready-made worlds so the first launch always has something to play.</summary>
        void SimulateMissing()
        {
            if (list.Count > 0) return;
        }

        public override void Render(UiRenderer ui, Hud hud)
        {
            Dim(0.62f);
            float w = Mathf.Min(320f, W - 20f), h = Mathf.Min(220f, H - 20f);
            float x = (W - w) * 0.5f, y = (H - h) * 0.5f;
            Panel(x, y, w, h);
            ui.Text(title, x + w * 0.5f, y + 6f, Styles.Text, 1, true);

            float listY = y + 20f;
            float listH = h - 46f;
            ui.Rect(x + 6f, listY, w - 12f, listH, new Color32(0, 0, 0, 130));
            ui.PushClip(x + 6f, listY, w - 12f, listH);
            int rows = Mathf.FloorToInt(listH / 22f);
            for (int i = 0; i < rows && i + scroll < list.Count; i++)
            {
                int idx = i + scroll;
                var info = list[idx];
                float ry = listY + i * 22f;
                bool sel = idx == selected;
                ui.Rect(x + 7f, ry, w - 14f, 21f, sel ? new Color32(255, 255, 255, 60) : new Color32(0, 0, 0, 90));
                ui.Text(info.name, x + 11f, ry + 2f, sel ? Styles.Yellow : Styles.Text, 1);
                ui.Text("seed " + info.seed + " - " + info.mode + " - " + info.lastPlayed.ToString("yyyy-MM-dd HH:mm"), x + 11f, ry + 12f, Styles.TextGray, 1);
                if (hud.mouseX >= x + 7f && hud.mouseX < x + w - 7f && hud.mouseY >= ry && hud.mouseY < ry + 21f && hud.mouseClicked)
                {
                    if (selected == idx) { Play(info); return; }
                    selected = idx; Sounds.PlayUI("ui.button.click");
                }
            }
            ui.PopClip();
            if (list.Count == 0) ui.Text("No worlds yet - create one", x + w * 0.5f, listY + 10f, Styles.TextGray, 1, true);

            float by = y + h - 22f;
            if (Button(x + 6f, by, 70f, 18f, "Play")) { if (list.Count > 0) Play(list[Mathf.Clamp(selected, 0, list.Count - 1)]); }
            if (Button(x + 80f, by, 70f, 18f, confirmDelete ? "Sure?" : "Delete", list.Count > 0))
            {
                if (confirmDelete && list.Count > 0)
                {
                    SaveManager.Delete(list[Mathf.Clamp(selected, 0, list.Count - 1)].folder);
                    confirmDelete = false; Refresh();
                }
                else confirmDelete = true;
            }
            if (Button(x + 154f, by, 70f, 18f, "Create")) { Sounds.PlayUI("ui.button.click"); hud.Replace(new CreateWorldScreen()); }
            if (Button(x + w - 76f, by, 70f, 18f, "Cancel")) { Sounds.PlayUI("ui.button.click"); hud.Replace(new TitleScreen()); }
        }

        void Play(WorldInfo info)
        {
            Sounds.PlayUI("ui.button.click");
            Gm.Load(info);
        }
    }

    /// <summary>World creation: name, seed, gamemode, cheats and world type.</summary>
    public sealed class CreateWorldScreen : GuiScreen
    {
        string name = "New World";
        string seedText = "";
        GameMode mode = GameMode.Creative;
        bool cheatMode = true;
        public static bool SeedVisible = true;
        public override void OnOpen() { title = "Create New World"; name = "World " + (SaveManager.ListWorlds().Count + 1); }

        bool seedFocused;

        public override void Input(Hud hud)
        {
            // typing goes to the focused box (click a box or press Tab to switch)
            if (hud.pressedTab) seedFocused = !seedFocused;
            if (seedFocused)
            {
                foreach (var c in hud.typedChars) if (seedText.Length < 32 && c >= ' ' && c != 127) seedText += c;
                if (hud.pressedBackspace && seedText.Length > 0) seedText = seedText.Substring(0, seedText.Length - 1);
            }
            else
            {
                foreach (var c in hud.typedChars) if (name.Length < 24 && (char.IsLetterOrDigit(c) || c == ' ' || c == '-' || c == '_')) name += c;
                if (hud.pressedBackspace && name.Length > 0) name = name.Substring(0, name.Length - 1);
            }
            if (hud.pressedEscape) hud.Replace(new TitleScreen());
        }

        /// <summary>Numbers are used as they are; any other text is hashed the way Java hashes strings, so a seed phrase always gives the same world.</summary>
        public static int ParseSeed(string text)
        {
            text = text.Trim();
            if (long.TryParse(text, out long n)) return n >= int.MinValue && n <= int.MaxValue ? (int)n : (int)(n ^ (n >> 32));
            int h = 0;
            foreach (char c in text) h = unchecked(31 * h + c);
            return h;
        }

        bool Box(float x, float y, float w, float h) => hud.mouseClicked && hud.mouseX >= x && hud.mouseX < x + w && hud.mouseY >= y && hud.mouseY < y + h;

        public override void Render(UiRenderer ui, Hud hud)
        {
            Dim(0.7f);
            float w = Mathf.Min(320f, W - 20f), h = Mathf.Min(190f, H - 20f);
            float x = (W - w) * 0.5f, y = (H - h) * 0.5f;
            Panel(x, y, w, h);
            ui.Text(title, x + w * 0.5f, y + 6f, Styles.Text, 1, true);

            float lx = x + 10f, cx2 = x + 110f;
            ui.Text("World Name", lx, y + 26f, Styles.TextGray, 1);
            bool blink = Mathf.FloorToInt(Time.time * 2f) % 2 == 0;
            if (Box(cx2, y + 24f, w - 120f, 14f)) seedFocused = false;
            if (Box(cx2, y + 44f, w - 120f, 14f)) seedFocused = true;
            ui.Rect(cx2 - 1f, y + 23f, w - 118f, 16f, seedFocused ? new Color32(90, 90, 90, 255) : Styles.Text);
            ui.Rect(cx2, y + 24f, w - 120f, 14f, new Color32(0, 0, 0, 255));
            ui.Text(name + (!seedFocused && blink ? "_" : ""), cx2 + 2f, y + 27f, Styles.Text, 1);

            ui.Text("Seed", lx, y + 46f, Styles.TextGray, 1);
            ui.Rect(cx2 - 1f, y + 43f, w - 118f, 16f, seedFocused ? Styles.Text : new Color32(90, 90, 90, 255));
            ui.Rect(cx2, y + 44f, w - 120f, 14f, new Color32(0, 0, 0, 255));
            if (string.IsNullOrEmpty(seedText) && !seedFocused) ui.Text("random", cx2 + 2f, y + 47f, Styles.TextGray, 1);
            else ui.Text(seedText + (seedFocused && blink ? "_" : ""), cx2 + 2f, y + 47f, Styles.Text, 1);
            ui.Text("Leave blank for a random seed", cx2 + 2f, y + 60f, new Color32(100, 100, 100, 255), 1);

            ui.Text("Game Mode", lx, y + 80f, Styles.TextGray, 1);
            float gx = cx2;
            foreach (var m in new[] { GameMode.Survival, GameMode.Creative })
            {
                bool sel = mode == m;
                if (Button(gx, y + 78f, 62f, 16f, sel ? "[ " + m + " ]" : m.ToString())) { mode = m; Sounds.PlayUI("ui.button.click"); }
                gx += 66f;
            }
            ui.Text("Allow Cheats", lx, y + 100f, Styles.TextGray, 1);
            if (Button(cx2, y + 98f, 40f, 16f, cheatMode ? "ON" : "OFF")) { cheatMode = !cheatMode; Sounds.PlayUI("ui.button.click"); }

            if (Button(x + 10f, y + h - 24f, 90f, 18f, "Create"))
            {
                Sounds.PlayUI("ui.button.click");
                int seed = string.IsNullOrWhiteSpace(seedText) ? Random.Range(int.MinValue, int.MaxValue) : ParseSeed(seedText);
                Gm.NewWorld(name, seed, (int)mode, cheatMode);
            }
            if (Button(x + w - 100f, y + h - 24f, 90f, 18f, "Cancel")) { Sounds.PlayUI("ui.button.click"); hud.Replace(new TitleScreen()); }
        }
    }

    /// <summary>Pause menu: back to game, options, save and quit to title.</summary>
    public sealed class PauseScreen : GuiScreen
    {
        public override void OnOpen() { title = "Game Menu"; }
        public override void Input(Hud hud) { if (hud.pressedEscape) { Sounds.PlayUI("ui.button.click"); hud.Pop(); } }

        public override void Render(UiRenderer ui, Hud hud)
        {
            // buttons float over the dimmed world, two columns in the middle row, like the original layout
            Dim(0.55f);
            float x = W * 0.5f - 102f, y = H * 0.25f + 8f;
            ui.Text(title, W * 0.5f, H * 0.25f - 20f, Styles.Text, 1, true);
            if (Button(x, y, 204f, 20f, "Back to Game")) { Sounds.PlayUI("ui.button.click"); hud.Pop(); }
            if (Button(x, y + 24f, 100f, 20f, "Advancements")) { Sounds.PlayUI("ui.button.click"); hud.Push(new AdvancementsScreen()); }
            if (Button(x + 104f, y + 24f, 100f, 20f, "Statistics")) { Sounds.PlayUI("ui.button.click"); hud.Push(new HelpScreen()); }
            if (Button(x, y + 48f, 100f, 20f, "Save World")) { Sounds.PlayUI("ui.button.click"); Gm.SaveAll(); hud.ShowActionBar("World saved"); }
            if (Button(x + 104f, y + 48f, 100f, 20f, "Game Modes")) { Sounds.PlayUI("ui.button.click"); hud.Push(new GameModeSwitcherScreen()); }
            if (Button(x, y + 72f, 100f, 20f, "Options...")) { Sounds.PlayUI("ui.button.click"); hud.Push(new OptionsScreen()); }
            if (Button(x + 104f, y + 72f, 100f, 20f, "Open to LAN", false)) { }
            if (Button(x, y + 96f, 204f, 20f, "Save and Quit to Title")) { Sounds.PlayUI("ui.button.click"); Gm.SaveAll(); Gm.QuitToTitle(); }
        }
    }

    /// <summary>Options: video, audio, controls, language and a couple of game rules.</summary>
    public sealed class OptionsScreen : GuiScreen
    {
        int tab;
        static readonly string[] tabs = { "Video", "Audio", "Controls", "Game" };

        public override void OnOpen() { title = "Options"; }
        public override void Input(Hud hud) { if (hud.pressedEscape) { Sounds.PlayUI("ui.button.click"); Cancel(); } }
        void Cancel() { hud.Pop(); Gm?.ApplySettings(); }

        public override void Render(UiRenderer ui, Hud hud)
        {
            Dim(0.7f);
            float w = Mathf.Min(340f, W - 16f), h = Mathf.Min(240f, H - 16f);
            float x = (W - w) * 0.5f, y = (H - h) * 0.5f;
            Panel(x, y, w, h);
            ui.Text(title, x + w * 0.5f, y + 6f, Styles.Text, 1, true);

            float tx = x + 6f;
            for (int i = 0; i < tabs.Length; i++)
            {
                bool sel = tab == i;
                if (Button(tx, y + 20f, 58f, 16f, tabs[i])) { tab = i; Sounds.PlayUI("ui.button.click"); }
                tx += 60f;
            }
            ui.Rect(x + 6f, y + 38f, w - 12f, 1f, Styles.PanelDark);

            float ly = y + 46f;
            switch (tab)
            {
                case 0: RenderVideo(ui, hud, x + 10f, ly, w - 20f); break;
                case 1: RenderAudio(ui, hud, x + 10f, ly, w - 20f); break;
                case 2: RenderControls(ui, hud, x + 10f, ly, w - 20f); break;
                default: RenderGame(ui, hud, x + 10f, ly, w - 20f); break;
            }

            if (Button(x + w - 70f, y + h - 22f, 60f, 16f, "Done")) { Sounds.PlayUI("ui.button.click"); Cancel(); }
        }

        bool Slider(float x, float y, float w, string label, float value, out float newValue)
        {
            ui.Text(label, x, y + 3f, Styles.TextGray, 1);
            float sx = x + w - 120f, sw = 116f;
            ui.Rect(sx, y + 4f, sw, 8f, new Color32(0, 0, 0, 200));
            ui.Rect(sx + 1f, y + 5f, (sw - 2f) * Mathf.Clamp01(value), 6f, new Color32(120, 160, 230, 255));
            float knob = sx + 1f + (sw - 10f) * Mathf.Clamp01(value);
            bool hover = hud.mouseY >= y && hud.mouseY < y + 16f;
            ui.Rect(knob, y + 1f, 8f, 14f, hover ? Styles.ButtonHi : Styles.Button);
            ui.Outline(knob, y + 1f, 8f, 14f, Styles.PanelLight);
            newValue = value;
            if (hud.mouseDown && hover && hud.mouseX >= sx - 4f && hud.mouseX <= sx + sw + 4f) newValue = Mathf.Clamp01((hud.mouseX - sx - 1f) / (sw - 10f));
            return newValue != value;
        }

        void RenderVideo(UiRenderer ui, Hud hud, float x, float y, float w)
        {
            var gm = Gm;
            float v;
            if (Slider(x, y, w, "Render Distance", (gm.renderDistance - 2) / 30f, out v)) gm.renderDistance = Mathf.RoundToInt(2 + v * 30);
            if (Slider(x, y + 20f, w, "Simulation Distance", (gm.simulationDistance - 2) / 14f, out v)) gm.simulationDistance = Mathf.RoundToInt(2 + v * 14);
            if (Slider(x, y + 40f, w, "Field of View", (gm.fov - 50f) / 70f, out v)) gm.fov = 50f + v * 70f;
            if (Slider(x, y + 60f, w, "Brightness", gm.brightness, out v)) gm.brightness = v;
            if (Slider(x, y + 80f, w, "GUI Scale", hud.GuiScale == 0 ? 0.5f : hud.GuiScale / 6f, out v)) { hud.GuiScale = v < 0.55f ? 0 : Mathf.Clamp(Mathf.RoundToInt(v * 6f), 1, 6); hud.ApplyScale(); }
            if (Slider(x, y + 100f, w, "Master Volume", Sounds.masterVolume, out v)) Sounds.masterVolume = v;
            if (Button(x, y + 120f, 90f, 16f, "Vsync: " + (gm.vsync ? "ON" : "OFF"))) gm.vsync = !gm.vsync;
            if (Button(x + 96f, y + 120f, 90f, 16f, "Smooth Lighting: " + (gm.smoothLighting ? "ON" : "OFF"))) gm.smoothLighting = !gm.smoothLighting;
            if (Button(x + 192f, y + 120f, 90f, 16f, "Clouds: " + (gm.clouds ? "ON" : "OFF"))) gm.clouds = !gm.clouds;
            ui.Text("Particles: " + gm.particlesLevel, x, y + 142f, Styles.TextGray, 1);
            if (Button(x + 104f, y + 140f, 40f, 14f, "All")) gm.particlesLevel = 0;
            if (Button(x + 148f, y + 140f, 40f, 14f, "Few")) gm.particlesLevel = 1;
            if (Button(x + 192f, y + 140f, 46f, 14f, "None")) gm.particlesLevel = 2;
        }

        void RenderAudio(UiRenderer ui, Hud hud, float x, float y, float w)
        {
            float v;
            if (Slider(x, y, w, "Master", Sounds.masterVolume, out v)) Sounds.masterVolume = v;
            if (Slider(x, y + 20f, w, "Music", Sounds.musicVolume, out v)) Sounds.musicVolume = v;
            if (Slider(x, y + 40f, w, "Blocks", Sounds.blockVolume, out v)) Sounds.blockVolume = v;
            if (Slider(x, y + 60f, w, "Hostile Mobs", Sounds.hostileVolume, out v)) Sounds.hostileVolume = v;
            if (Slider(x, y + 80f, w, "Friendly Mobs", Sounds.friendlyVolume, out v)) Sounds.friendlyVolume = v;
            if (Slider(x, y + 100f, w, "Players", Sounds.playerVolume, out v)) Sounds.playerVolume = v;
            if (Slider(x, y + 120f, w, "Ambient", Sounds.ambientVolume, out v)) Sounds.ambientVolume = v;
            if (Slider(x, y + 140f, w, "Weather", Sounds.weatherVolume, out v)) Sounds.weatherVolume = v;
        }

        void RenderControls(UiRenderer ui, Hud hud, float x, float y, float w)
        {
            var binds = Gm.KeyBindings;
            float ly = y;
            foreach (var kv in binds)
            {
                if (ly > y + 150f) break;
                ui.Text(kv.Key, x, ly + 3f, Styles.TextGray, 1);
                string label = kv.Value;
                var key = UiAtlas.Region("tab_top");
                ui.Rect(x + w - 74f, ly, 68f, 14f, new Color32(0, 0, 0, 200));
                ui.Text(label, x + w - 40f, ly + 3f, Styles.Text, 1, true);
                ly += 17f;
            }
            ui.Text("Sensitivity", x, ly + 3f, Styles.TextGray, 1);
            float v;
            if (Slider(x, ly + 16f, w, "", Gm.mouseSensitivity, out v)) Gm.mouseSensitivity = v;
        }

        void RenderGame(UiRenderer ui, Hud hud, float x, float y, float w)
        {
            var s = Gm.session;
            if (s == null) { ui.Text("No world loaded", x, y, Styles.TextGray, 1); return; }
            ui.Text("Difficulty", x, y + 3f, Styles.TextGray, 1);
            float dx = x + 120f;
            foreach (var d in new[] { Difficulty.Peaceful, Difficulty.Easy, Difficulty.Normal, Difficulty.Hard })
            {
                if (Button(dx, y, 56f, 16f, s.difficulty == d ? "[" + d + "]" : d.ToString())) { s.difficulty = d; Sounds.PlayUI("ui.button.click"); }
                dx += 60f;
            }
            ui.Text("Game Mode", x, y + 24f, Styles.TextGray, 1);
            dx = x + 120f;
            foreach (var m in new[] { GameMode.Survival, GameMode.Creative, GameMode.Adventure, GameMode.Spectator })
            {
                if (Button(dx, y + 21f, 56f, 16f, s.defaultMode == m ? "[" + m + "]" : m.ToString())) { (Gm.player ?? null)?.SetGameMode(m); s.defaultMode = m; Sounds.PlayUI("ui.button.click"); }
                dx += 60f;
            }
            float ry = y + 48f;
            RuleToggle(ui, hud, x, ry, w, "Daylight Cycle", s.doDaylightCycle, b => s.doDaylightCycle = b); ry += 18f;
            RuleToggle(ui, hud, x, ry, w, "Weather Cycle", s.doWeatherCycle, b => s.doWeatherCycle = b); ry += 18f;
            RuleToggle(ui, hud, x, ry, w, "Mob Spawning", s.doMobSpawning, b => s.doMobSpawning = b); ry += 18f;
            RuleToggle(ui, hud, x, ry, w, "Keep Inventory", s.keepInventory, b => s.keepInventory = b); ry += 18f;
            RuleToggle(ui, hud, x, ry, w, "Fire Tick", s.doFireTick, b => s.doFireTick = b); ry += 18f;
            RuleToggle(ui, hud, x, ry, w, "Mob Griefing", s.mobGriefing, b => s.mobGriefing = b); ry += 18f;
            RuleToggle(ui, hud, x, ry, w, "Natural Regeneration", s.naturalRegeneration, b => s.naturalRegeneration = b); ry += 18f;
            ui.Text("Random Tick Speed", x, ry + 3f, Styles.TextGray, 1);
            float v;
            if (Slider(x, ry + 16f, w, "", Mathf.Clamp01(s.randomTickSpeed / 30f), out v)) s.randomTickSpeed = Mathf.RoundToInt(v * 30f);
        }

        void RuleToggle(UiRenderer ui, Hud hud, float x, float y, float w, string label, bool value, System.Action<bool> set)
        {
            ui.Text(label, x, y + 3f, Styles.TextGray, 1);
            if (Button(x + w - 74f, y, 68f, 14f, value ? "ON" : "OFF")) { set(!value); Sounds.PlayUI("ui.button.click"); }
        }
    }

    /// <summary>Death screen: the message, the cause and the respawn / title buttons.</summary>
    public sealed class DeathScreen : GuiScreen
    {
        public override void OnOpen() { dismissOnEscape = false; }
        public override void Input(Hud hud)
        {
            if (hud.pressedEnter) Respawn();
        }
        void Respawn()
        {
            var p = P;
            if (p == null) return;
            p.Respawn();
            hud.Pop();
        }

        public override void Render(UiRenderer ui, Hud hud)
        {
            var p = P;
            ui.Rect(0, 0, W, H, new Color32(96, 0, 0, 150));
            ui.Text("You Died!", W * 0.5f, H * 0.28f, new Color32(255, 85, 85, 255), 2, true, 3f);
            if (p != null && !string.IsNullOrEmpty(p.lastDeathMessage))
                ui.Text(p.lastDeathMessage, W * 0.5f, H * 0.28f + 30f, Styles.Text, 1, true);
            float bx = W * 0.5f - 100f, by = H * 0.52f;

            if (Button(bx, by, 200f, 20f, "Respawn", true)) Respawn();
            if (Button(bx, by + 24f, 200f, 20f, "Title Screen")) { Gm.SaveAll(); Gm.QuitToTitle(); }
            ui.Text("Score: " + (p != null ? p.score : 0), W * 0.5f, by + 52f, Styles.Yellow, 1, true);
        }
    }

    /// <summary>
    /// Chat with command support: history with Up/Down, a caret moved with Left/Right (typing, Backspace and Delete
    /// act at it), command suggestions while typing a "/" line, Tab completion that cycles through them (Shift+Tab
    /// backwards), and /help.
    /// </summary>
    public sealed class ChatScreen : GuiScreen
    {
        public string text = "";
        /// <summary>Insertion point in <see cref="text"/>; -1 until opened, then clamped to the text.</summary>
        public int caret = -1;

        // completion: candidates for the word that ends at the caret, recomputed whenever the line or caret changes
        // other than by Tab itself, so repeated Tab presses cycle and any edit starts over
        readonly List<string> candidates = new List<string>();
        int candidateIndex = -1, wordStart;
        string suggestedFor, completedText;
        int suggestedCaret = -1, completedCaret = -1;
        const int MaxShown = 10;

        public override void OnOpen() { pauseGame = false; dismissOnEscape = true; if (caret < 0 || caret > text.Length) caret = text.Length; }
        public override void OnClose() { hud.SetChatOpen(false); }

        public override void Update(float dt) { }

        public override void Input(Hud hud)
        {
            caret = Mathf.Clamp(caret, 0, text.Length);
            foreach (var c in hud.typedChars)
                if (c >= ' ' && c != 127 && text.Length < 256) { text = text.Insert(caret, c.ToString()); caret++; }
            if (hud.pressedBackspace && caret > 0) { text = text.Remove(caret - 1, 1); caret--; }
            if (hud.pressedDelete && caret < text.Length) text = text.Remove(caret, 1);
            if (hud.keyLeft && caret > 0) caret--;
            if (hud.keyRight && caret < text.Length) caret++;
            if (hud.keyUp)
            {
                if (hud.historyIndex > 0) { hud.historyIndex--; text = hud.chatHistory[hud.historyIndex]; caret = text.Length; }
            }
            if (hud.keyDown)
            {
                if (hud.historyIndex < hud.chatHistory.Count - 1) { hud.historyIndex++; text = hud.chatHistory[hud.historyIndex]; }
                else { hud.historyIndex = hud.chatHistory.Count; text = ""; }
                caret = text.Length;
            }
            RefreshSuggestions();
            if (hud.pressedTab) CompleteWord(hud.shift ? -1 : 1);
            if (hud.pressedEnter)
            {
                string line = text.Trim();
                hud.Pop();
                if (line.Length > 0)
                {
                    hud.chatHistory.Add(line);
                    if (line.StartsWith("/")) Commands.Execute(line.Substring(1), GameManager.Instance?.player);
                    else hud.Chat("<" + (GameManager.Instance?.player?.playerName ?? "Player") + "> " + line);
                }
                return;
            }
            if (hud.pressedEscape) hud.Pop();
        }

        public override void Render(UiRenderer ui, Hud hud)
        {
            float y = H - 26f;
            ui.Rect(0, y, W, 18f, new Color32(0, 0, 0, 160));
            int at = Mathf.Clamp(caret, 0, text.Length);
            ui.Text("> " + text, 2f, y + 5f, Styles.Text, 1);
            if (Mathf.FloorToInt(Time.time * 2f) % 2 == 0)
            {
                // a bar between characters, an underscore after the last one
                float cx = Mathf.Floor(2f + ui.TextWidth("> " + text.Substring(0, at))) - 1f;
                if (at < text.Length) ui.Rect(cx, y + 4f, 1f, 10f, Styles.Text);
                else ui.Text("_", cx + 1f, y + 5f, Styles.Text, 1);
            }
            RenderSuggestions(ui, y);
        }

        // ------------------------------------------------------------------ completion
        /// <summary>Recomputes the candidates when the line or caret moved since they were computed (or since the last
        /// Tab), which also resets the cycling index.</summary>
        void RefreshSuggestions()
        {
            if (text == completedText && caret == completedCaret) return;
            if (text == suggestedFor && caret == suggestedCaret) return;
            suggestedFor = text; suggestedCaret = caret;
            completedText = null; completedCaret = -1;
            candidates.Clear();
            candidateIndex = -1;
            if (!text.StartsWith("/")) return;
            // the word being completed runs from the last space before the caret (or the "/") up to the caret
            wordStart = caret > 0 ? text.LastIndexOf(' ', caret - 1) + 1 : 0;
            if (wordStart == 0) wordStart = 1;
            if (wordStart > caret) return;
            candidates.AddRange(Commands.Complete(text.Substring(0, caret)));
        }

        /// <summary>Tab: puts the next candidate (Shift+Tab: the previous one) in place of the word before the caret.</summary>
        void CompleteWord(int step)
        {
            if (candidates.Count == 0) return;
            candidateIndex = candidateIndex < 0 ? (step > 0 ? 0 : candidates.Count - 1) : ((candidateIndex + step) % candidates.Count + candidates.Count) % candidates.Count;
            string pick = candidates[candidateIndex];
            // the replaced span is what was typed on the first press, then the previous pick
            text = text.Substring(0, wordStart) + pick + text.Substring(caret);
            caret = wordStart + pick.Length;
            completedText = text; completedCaret = caret;
        }

        /// <summary>Suggestion box above the input line, aligned with the word being completed; the Tab pick is yellow.</summary>
        void RenderSuggestions(UiRenderer ui, float inputY)
        {
            if (candidates.Count == 0 || !text.StartsWith("/")) return;
            string typed = text.Substring(wordStart, Mathf.Clamp(caret - wordStart, 0, text.Length - wordStart));
            if (candidates.Count == 1 && string.Equals(candidates[0], typed, System.StringComparison.OrdinalIgnoreCase)) return;
            int shown = Mathf.Min(MaxShown, candidates.Count);
            int first = candidateIndex < 0 ? 0 : Mathf.Clamp(candidateIndex - shown / 2, 0, candidates.Count - shown);
            float w = 0f;
            for (int i = 0; i < shown; i++) w = Mathf.Max(w, ui.TextWidth(candidates[first + i]));
            bool more = candidates.Count > shown;
            string count = candidates.Count + " matches - Tab";
            if (more) w = Mathf.Max(w, ui.TextWidth(count));
            float x = Mathf.Min(2f + ui.TextWidth("> " + text.Substring(0, wordStart)) - 1f, W - w - 6f);
            float h = (shown + (more ? 1 : 0)) * 10f + 2f;
            float y = inputY - h - 1f;
            ui.Rect(x - 2f, y, w + 5f, h, new Color32(0, 0, 0, 210));
            for (int i = 0; i < shown; i++)
            {
                int idx = first + i;
                ui.Text(candidates[idx], x, y + 2f + i * 10f, idx == candidateIndex ? Styles.Yellow : Styles.TextDim, 1);
            }
            if (more) ui.Text(count, x, y + 2f + shown * 10f, Styles.TextGray, 1);
        }
    }
}
