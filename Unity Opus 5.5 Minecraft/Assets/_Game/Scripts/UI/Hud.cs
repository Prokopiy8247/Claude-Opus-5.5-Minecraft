using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// The single entry point for everything the player reads on screen: the survival HUD, the debug overlay,
    /// chat/action bar, the screen stack (title, world select, pause, options, death) and the menu containers.
    /// Screens are pushed onto a small stack so overlays layer predictably.
    /// </summary>
    public sealed partial class Hud
    {
        public readonly UiRenderer ui = new UiRenderer();

        public sealed class ChatLine
        {
            public string text;
            public float age;
            public int ticksLeft;
        }

        public readonly List<ChatLine> chat = new List<ChatLine>();
        public readonly List<string> chatHistory = new List<string>();
        public int historyIndex = -1;

        public string actionBar = "";
        public int actionBarTicks;
        public string title, subtitle;
        public int titleTicks, titleFadeIn = 10, titleStay = 70, titleFadeOut = 20;
        public float titleAge;

        public bool showDebug;
        public bool debugCharts;
        public float totemTicks;
        public float hurtFlash;
        public float portalFlash;
        public float dragonBarTicks;
        public string bossName;
        public float bossFrac = 1f;
        public bool bossDarken;
        public bool showBossBar => dragonBarTicks > 0;

        public int screenW, screenH;
        public float scale = 2f;
        public int GuiScale = 0; // 0 = auto

        GameManager gm;
        public readonly List<GuiScreen> screens = new List<GuiScreen>();
        public GuiScreen Current => screens.Count > 0 ? screens[screens.Count - 1] : null;
        public bool AnyScreen => screens.Count > 0;

        // input derived state, filled by the input layer each frame
        public float mouseX, mouseY;
        public bool mouseDown, mouseDownRight, mouseClicked, mouseClickedRight;
        public bool shift, ctrl, alt;
        public string typed;
        public int scrollDelta;
        public bool pressedEnter, pressedEscape, pressedBackspace, pressedDelete;
        public readonly List<char> typedChars = new List<char>();
        public bool keyUp, keyDown, keyLeft, keyRight;
        public bool hoverTyped;
        public bool mouseReleased, mouseReleasedRight, mouseClickedMiddle, doubleClick;
        public int numberKey = -1;
        public bool pressedQ, pressedF, pressedE, pressedTab;

        int fpsCounter;
        float fpsTimer;
        int fps;

        public void Init(GameManager manager)
        {
            gm = manager;
            ApplyScale();
        }

        public void ApplyScale()
        {
            screenW = UnityEngine.Screen.width; screenH = UnityEngine.Screen.height;
            int s = GuiScale;
            if (s <= 0)
            {
                s = 1;
                while (s * 320 <= screenW && s * 240 <= screenH) s++;
                s--;
                s = Mathf.Clamp(s, 1, 6);
            }
            scale = s;
        }

        /// <summary>Width and height of the drawing area in GUI units.</summary>
        public float W => screenW / scale;
        public float H => screenH / scale;

        // ------------------------------------------------------------------ screens
        public void Push(GuiScreen s)
        {
            if (s == null) return;
            s.hud = this;
            s.OnOpen();
            screens.Add(s);
            ClearEdges(); // the key or click that opened a screen must not also act inside it (E would close it again)
        }
        public void Pop()
        {
            if (screens.Count == 0) return;
            var s = screens[screens.Count - 1];
            screens.RemoveAt(screens.Count - 1);
            s.OnClose();
            ClearEdges(); // ...nor land on the screen underneath
        }
        public void ClearScreens()
        {
            while (screens.Count > 0) Pop();
        }
        public void Replace(GuiScreen s) { ClearScreens(); Push(s); }
        public bool HasScreen<T>() where T : GuiScreen
        {
            foreach (var s in screens) if (s is T) return true;
            return false;
        }
        public void RemoveScreens<T>() where T : GuiScreen
        {
            bool removed = false;
            for (int i = screens.Count - 1; i >= 0; i--)
                if (screens[i] is T) { var s = screens[i]; screens.RemoveAt(i); s.OnClose(); removed = true; }
            if (removed) ClearEdges();
        }

        // ------------------------------------------------------------------ chat
        public bool ChatOpen => HasScreen<ChatScreen>();

        public void Chat(string msg)
        {
            if (string.IsNullOrEmpty(msg)) return;
            foreach (var line in msg.Split('\n'))
            {
                chat.Add(new ChatLine { text = line, ticksLeft = 200 });
                if (!msg.StartsWith("<")) chatHistory.Add(line);
            }
            while (chat.Count > 100) chat.RemoveAt(0);
            while (chatHistory.Count > 100) chatHistory.RemoveAt(0);
            historyIndex = chatHistory.Count;
        }

        public void ShowActionBar(string msg) { actionBar = msg; actionBarTicks = 60; }

        public void SetTitle(string t, string sub = null)
        {
            title = t; subtitle = sub; titleTicks = 0; titleAge = 0;
        }

        public void ShowTotem() { totemTicks = 60; }

        public void OpenChat(string preset = null)
        {
            if (ChatOpen) return;
            var c = new ChatScreen();
            c.text = preset ?? "";
            Push(c);
        }

        public void SetChatOpen(bool open)
        {
            if (open) OpenChat();
            else if (ChatOpen) Pop();
        }

        public void OnHurt(float amount) { hurtFlash = Mathf.Min(1f, hurtFlash + Mathf.Clamp01(amount / 8f)); }

        public void ShowBossBar(string name, float frac, bool darken)
        {
            bossName = name; bossFrac = frac; bossDarken = darken;
            dragonBarTicks = 40;
        }

        // ------------------------------------------------------------------ frame
        public void Update(float dt)
        {
            fpsCounter++;
            fpsTimer += dt;
            if (fpsTimer >= 1f) { fps = fpsCounter; fpsCounter = 0; fpsTimer -= 1f; }
            if (actionBarTicks > 0) actionBarTicks--;
            if (totemTicks > 0) totemTicks -= dt * 60f;
            if (hurtFlash > 0) hurtFlash = Mathf.Max(0, hurtFlash - dt * 1.6f);
            if (dragonBarTicks > 0) dragonBarTicks--;
            if (title != null)
            {
                titleAge += dt * 20f;
                titleTicks++;
                if (titleTicks > titleFadeIn + titleStay + titleFadeOut) title = null;
            }
            for (int i = chat.Count - 1; i >= 0; i--)
            {
                chat[i].age += dt;
                if (!ChatOpen)
                {
                    chat[i].ticksLeft--;
                    if (chat[i].ticksLeft <= 0) chat.RemoveAt(i);
                }
            }
            // the portal/overlay flashes fade with the player's own portal effect
            portalFlash = gm?.player != null ? Mathf.Lerp(gm.player.prevPortalEffect, gm.player.portalEffect, 0f) / 80f : 0f;
            for (int i = screens.Count - 1; i >= 0; i--) screens[i].Update(dt);
        }

        /// <summary>
        /// Feeds this frame's input to the topmost screen. Key edges are consumed here; click, release and wheel edges
        /// stay live until the end of <see cref="Render"/>, because buttons, tabs and slots handle their clicks while
        /// they draw (clearing them here, before the frame's Render, left every on-screen button dead).
        /// </summary>
        public void Input()
        {
            var s = Current;
            if (s != null) s.Input(this);
            ClearKeyEdges();
        }

        /// <summary>Drops this frame's key and pointer edges; held buttons and the pointer position are kept.</summary>
        void ClearEdges()
        {
            ClearKeyEdges();
            ClearMouseEdges();
        }

        void ClearKeyEdges()
        {
            typedChars.Clear(); typed = null;
            pressedEnter = pressedEscape = pressedBackspace = pressedDelete = false;
            keyUp = keyDown = keyLeft = keyRight = false;
            numberKey = -1;
            pressedQ = pressedF = pressedE = pressedTab = false;
        }

        void ClearMouseEdges()
        {
            mouseClicked = false; mouseClickedRight = false;
            mouseReleased = mouseReleasedRight = mouseClickedMiddle = doubleClick = false;
            scrollDelta = 0;
        }

        // ------------------------------------------------------------------ render
        /// <summary>Stack a screen asked to show a tooltip for; drawn after everything else so it is never covered.</summary>
        public ItemStack pendingTooltip;

        public void Render()
        {
            try { RenderFrame(); }
            finally { ClearMouseEdges(); } // every screen has had its chance at this frame's clicks
        }

        void RenderFrame()
        {
            ApplyScale();
            ItemIcons.FlushIfDirty();
            ui.Begin(screenW, screenH, scale);
            PlayerPreview.BeginFrame();
            pendingTooltip = null;
            var s = Current;
            bool inWorld = gm != null && gm.session != null && gm.player != null;
            if (inWorld && !HideGui && (s == null || s.drawWorldBehind)) RenderHud(ui);
            if (inWorld && showDebug && !HideGui) RenderDebug(ui);
            if (inWorld && !HideGui && s == null) RenderOverlays(ui);
            else if (inWorld) RenderWorldOverlaysOnly(ui);
            for (int i = 0; i < screens.Count; i++)
            {
                if (i < screens.Count - 1) RenderUnderneath(screens[i]);
                else screens[i].Render(ui, this);
            }
            if (pendingTooltip != null && Current != null) Current.DrawTooltip(pendingTooltip, mouseX + 8f, mouseY + 8f);
            ItemIcons.FlushIfDirty();
            ui.End();
            ui.Present();
            PlayerPreview.EndFrame();
        }

        /// <summary>
        /// Screens below the top one still draw, but with the pointer hidden from them: a click belongs to the top
        /// screen only (otherwise the pause menu's buttons would fire through the options screen above it).
        /// </summary>
        void RenderUnderneath(GuiScreen s)
        {
            float mx = mouseX, my = mouseY;
            bool down = mouseDown, downR = mouseDownRight, click = mouseClicked, clickR = mouseClickedRight, clickM = mouseClickedMiddle;
            bool rel = mouseReleased, relR = mouseReleasedRight;
            int wheel = scrollDelta;
            mouseX = mouseY = -10000f;
            mouseDown = mouseDownRight = mouseClicked = mouseClickedRight = mouseClickedMiddle = mouseReleased = mouseReleasedRight = false;
            scrollDelta = 0;
            try { s.Render(ui, this); }
            finally
            {
                mouseX = mx; mouseY = my;
                mouseDown = down; mouseDownRight = downR; mouseClicked = click; mouseClickedRight = clickR; mouseClickedMiddle = clickM;
                mouseReleased = rel; mouseReleasedRight = relR;
                scrollDelta = wheel;
            }
        }

        void RenderWorldOverlaysOnly(UiRenderer ui)
        {
            var p = gm?.player;
            if (p == null) return;
            if (p.eyeInWater) ui.Rect(0, 0, W, H, new Color32(20, 60, 160, 90));
        }

        public bool HideGui;

        // ------------------------------------------------------------------ HUD pieces
        void RenderHud(UiRenderer ui)
        {
            var p = gm?.player;
            if (p == null || p.world == null) return;
            float cx = W * 0.5f, bottom = H - 4f;
            var third = p.ThirdPerson;
            if (p.IsSpectator) return;
            bool survival = !p.IsCreative;

            float hotbarY = H - 22f;
            // hearts and hunger sit above the experience bar, which sits just above the hotbar (survival only)
            float iconRowY = hotbarY - 17f;
            if (survival)
            {
                float xpY = hotbarY - 7f;
                float xpX = (W - 182f) * 0.5f;
                ui.Icon("xp_bg", xpX, xpY);
                float frac = p.xpLevel >= 30 ? 1f : p.xpProgress;
                int fillW = Mathf.RoundToInt(180 * Mathf.Clamp01(frac));
                if (fillW > 0) ui.IconPart("xp_fill", xpX + 1, xpY + 1, 0, 0, fillW, 3, Styles.Text, 1f);
                if (p.xpLevel > 0)
                {
                    string lv = p.xpLevel.ToString();
                    ui.Text(lv, cx, xpY - 7f + (p.xpLevel > 999 ? 0 : 1), Styles.Green, 1, true);
                }
            }

            // hotbar
            float hbX = (W - 182f) * 0.5f;
            ui.Icon("hotbar", hbX, hotbarY);
            for (int i = 0; i < 9; i++)
            {
                float sx = hbX + 3f + i * 20f;
                var stack = p.inventory.main[i];
                DrawItemWithCount(ui, stack, sx, hotbarY + 3f, p);
            }
            ui.Icon("hotbar_selection", hbX + p.inventory.selected * 20f, hotbarY - 1f);

            // offhand slot, mirrored to the other side
            var off = p.inventory.offhand;
            if (off != null && !off.IsEmpty)
            {
                float ox = hbX - 29f;
                ui.Icon("offhand_slot", ox, hotbarY - 1f);
                DrawItemWithCount(ui, off, ox + 4f, hotbarY + 3f, p);
            }

            if (survival)
            {
                // hearts and hunger mirror around the hotbar; armour and air sit above them
                int maxHp = Mathf.CeilToInt(p.maxHealth + p.absorption);
                int rows = Mathf.CeilToInt(maxHp / 20f);
                for (int row = 0; row < rows; row++)
                {
                    float y = iconRowY - (rows - 1 - row) * 10f;
                    for (int i = 0; i < 10; i++)
                    {
                        int idx = row * 10 + i;
                        if (idx >= maxHp) break;
                        float hx = cx - 91f + i * 8f;
                        float hp = p.health + p.absorption - idx * 2f;
                        string kind = HeartKind(p);
                        // low health makes the hearts jitter, like the original
                        float jy = p.health <= 4f && p.world != null ? ((p.world.tickCount + idx * 7) % 3 == 0 ? -1f : 0f) : 0f;
                        // every slot shows its dark container; the heart (or half) is drawn over it
                        ui.Icon(p.hurtTime > 0 && (p.hurtTime / 3) % 2 == 0 ? "heart_container_blink" : "heart_container", hx, y + jy);
                        if (hp >= 2f) ui.Icon("heart_" + kind, hx, y + jy);
                        else if (hp > 0f) ui.Icon("heart_" + kind + "_half", hx, y + jy);
                        if (hp > 0f && p.absorption > 0 && idx >= Mathf.CeilToInt(p.health / 2f)) ui.Icon("heart_absorbing", hx, y + jy);
                    }
                }
                // hunger
                int food = Mathf.RoundToInt(p.hunger.food);
                var hungerIcon = p.HasEffect(Effect.Hunger) ? "food_hunger" : "food_full";
                var hungerHalf = p.HasEffect(Effect.Hunger) ? "food_hunger_half" : "food_half";
                for (int i = 0; i < 10; i++)
                {
                    float hx = cx + 91f - 9f - i * 8f;
                    int v = food - i * 2;
                    ui.Icon("food_container", hx, iconRowY);
                    if (v >= 2) ui.Icon(hungerIcon, hx, iconRowY);
                    else if (v == 1) ui.Icon(hungerHalf, hx, iconRowY);
                }
                // armour row above hearts
                int armor = p.ArmorValue;
                // the armour row only appears while some armour is worn; then all ten slots show
                for (int i = 0; i < 10 && armor > 0; i++)
                {
                    int v = armor - i * 2;
                    float ax = cx - 91f + i * 8f;
                    if (v >= 2) ui.Icon("armor_full", ax, iconRowY - 10f);
                    else if (v == 1) ui.Icon("armor_half", ax, iconRowY - 10f);
                    else ui.Icon("armor_empty", ax, iconRowY - 10f);
                }
                // air bubbles, shown only while submerged
                if (p.air < p.maxAir)
                {
                    int bubbles = Mathf.CeilToInt(p.air / (float)p.maxAir * 10f);
                    for (int i = 0; i < 10; i++)
                    {
                        float ax = cx + 91f - 9f - i * 8f;
                        ui.Icon(i < bubbles ? "air_full" : "air_pop", ax, iconRowY - 10f);
                    }
                }
            }

            // left-hand status messages (chat) and the right-hand effect list
            RenderChat(ui);
            RenderEffects(ui);
            if (showBossBar) RenderBossBar(ui);
            RenderToast(ui);
            if (p.sleeping) RenderSleepOverlay(ui, p);
        }

        static string HeartKind(Player p)
        {
            if (p.HasEffect(Effect.Wither)) return "withered";
            if (p.HasEffect(Effect.Poison)) return "poisoned";
            if (p.inPowderSnow) return "frozen";
            return "full";
        }

        void RenderChat(UiRenderer ui)
        {
            if (chat.Count == 0) return;
            float y = H - 48f;
            int shown = 0;
            for (int i = chat.Count - 1; i >= 0 && shown < 10; i--, shown++)
            {
                var line = chat[i];
                float alpha = ChatOpen ? 1f : Mathf.Clamp01(line.ticksLeft / 40f) * 0.8f + 0.2f;
                var c = new Color32(255, 255, 255, (byte)(alpha * 255));
                float w = ui.TextWidth(line.text);
                ui.Rect(2f, y - shown * 9f - 1f, w + 2f, 9f, new Color32(0, 0, 0, (byte)(alpha * 110)));
                ui.Text(line.text, 3f, y - shown * 9f, c);
            }
            if (actionBarTicks > 0 && !string.IsNullOrEmpty(actionBar))
            {
                float alpha = Mathf.Clamp01(actionBarTicks / 20f);
                ui.Text(actionBar, W * 0.5f, H - 68f, new Color32(255, 255, 255, (byte)(alpha * 255)), 1, true);
            }
        }

        void RenderEffects(UiRenderer ui)
        {
            var p = gm?.player;
            if (p == null || p.effects.Count == 0) return;
            float x = W - 26f, y = 4f;
            foreach (var kv in p.effects)
            {
                var e = kv.Key; var inst = kv.Value;
                if (!inst.showParticles && inst.ambient) { }
                ui.Rect(x, y, 24f, 24f, new Color32(0, 0, 0, 150));
                ui.Rect(x + 1f, y + 1f, 22f, 22f, new Color32((byte)(e.color.r / 3), (byte)(e.color.g / 3), (byte)(e.color.b / 3), 200));
                ui.Sprite(ItemIcons.Atlas, x + 1f, y + 1f, 22f, 22f, ItemIcons.EffectUV(e), new Color32(255, 255, 255, 255));
                string lvl = inst.amplifier > 0 ? Roman(inst.amplifier + 1) : "";
                ui.Text(lvl, x + 23f, y + 15f, Styles.Text, 1, true);
                int secs = inst.duration / 20;
                string time = secs >= 60 ? (secs / 60) + ":" + (secs % 60).ToString("00") : secs.ToString();
                ui.Text(time, x + 12f, y + 25f, Styles.Text, 1, true);
                y += 30f;
            }
        }

        static string Roman(int n) => n <= 1 ? "" : n == 2 ? "II" : n == 3 ? "III" : n == 4 ? "IV" : n == 5 ? "V" : n.ToString();

        long lastToastTick = -1;
        /// <summary>The "Advancement Made!" card sliding in at the top right.</summary>
        void RenderToast(UiRenderer ui)
        {
            // the toast clock runs on game ticks (a card stays about five seconds)
            if (gm != null && gm.TicksRun != lastToastTick) { lastToastTick = gm.TicksRun; Achievements.Tick(); }
            var g = Achievements.ToastGoal;
            float t = Achievements.ToastProgress;
            if (g == null || t <= 0f) return;
            const float w = 160f, h = 32f;
            float x = W - w * t, y = 0f;
            GuiScreen.DrawPanel(ui, x, y, w, h);
            ui.Rect(x + 3f, y + 3f, w - 6f, h - 6f, new Color32(33, 33, 33, 255));
            ui.Text("Advancement Made!", x + 30f, y + 7f, Styles.Yellow, 1);
            ui.Text(g.title, x + 30f, y + 18f, Styles.Text, 1);
            DrawItemWithCount(ui, new ItemStack(ToastIcon(g.id), 1), x + 8f, y + 8f, null);
        }

        static string ToastIcon(string id)
        {
            switch (id)
            {
                case "get_wood": return "oak_log";
                case "crafting_table": return "crafting_table";
                case "build_pickaxe": return "wooden_pickaxe";
                case "stone_age": return "cobblestone";
                case "diamonds": case "get_diamond_tool": return "diamond";
                case "enter_the_nether": case "enter_the_nether_prepare": return "obsidian";
                case "enter_the_end": case "the_end": return "end_stone";
                case "elytra": return "elytra";
                case "kill_a_mob": return "iron_sword";
                default: return "grass_block";
            }
        }

        void RenderBossBar(UiRenderer ui)
        {
            float w = Mathf.Min(182f, W - 20f);
            float x = (W - w) * 0.5f, y = 12f;
            ui.Rect(x - 1f, y - 1f, w + 2f, 7f, new Color32(0, 0, 0, 220));
            ui.Rect(x, y, w, 5f, new Color32(60, 10, 60, 220));
            int fill = Mathf.RoundToInt((w - 2f) * Mathf.Clamp01(bossFrac));
            if (fill > 0) ui.Rect(x + 1f, y + 1f, fill, 3f, new Color32(232, 64, 200, 255));
            ui.Text(bossName, W * 0.5f, y - 10f, Styles.Text, 1, true);
        }

        void RenderSleepOverlay(UiRenderer ui, Player p)
        {
            float t = p.sleepTimer / 100f;
            float a = Mathf.Clamp01(t) * 0.9f;
            ui.Rect(0, 0, W, H, new Color32(0, 0, 0, (byte)(a * 255)));
            ui.Text("Sleeping...", W * 0.5f, H * 0.5f, new Color32(200, 200, 200, 255), 0, true);
        }

        void RenderOverlays(UiRenderer ui)
        {
            var p = gm?.player;
            if (p == null) return;
            if (hurtFlash > 0.01f) ui.Rect(0, 0, W, H, new Color32(200, 0, 0, (byte)(hurtFlash * 90)));
            if (p.portalEffect > 0) ui.Rect(0, 0, W, H, new Color32(120, 40, 200, (byte)(p.portalEffect / 80f * 170)));
            if (p.eyeInWater) ui.Rect(0, 0, W, H, new Color32(20, 60, 160, 90));
            if (p.inLava) ui.Rect(0, 0, W, H, new Color32(200, 90, 20, 150));
            bool burning = p.fireTicks > 0 && !p.HasEffect(Effect.FireResistance) && (!p.IsCreative || p.inLava);
            if (burning)
            {
                // two large animated flames rising from the lower corners, drawn from the fire block's own animation
                var tex = FireStrip(out int frames);
                if (tex != null)
                {
                    int frame = Mathf.FloorToInt(Time.time * 20f) % Mathf.Max(1, frames);
                    float v0 = 1f - (frame + 1) / (float)frames, v1 = 1f - frame / (float)frames;
                    float fh = H * 0.62f, fw = W * 0.46f;
                    var col = new Color32(255, 255, 255, 230);
                    ui.Sprite(tex, W * 0.5f - fw - W * 0.02f, H - fh * 0.92f, fw, fh, 0f, v1, 1f, v0, col);
                    ui.Sprite(tex, W * 0.5f + W * 0.02f, H - fh * 0.92f, fw, fh, 1f, v1, 0f, v0, col);
                }
            }
            if (totemTicks > 0)
            {
                float t = 1f - totemTicks / 60f;
                float a = Mathf.Sin(t * Mathf.PI) * 0.9f;
                ui.Rect(0, 0, W, H, new Color32(255, 230, 120, (byte)(a * 90)));
                ui.Text("Totem of Undying saved you", W * 0.5f, H * 0.35f, new Color32(255, 255, 255, (byte)(a * 255)), 1, true);
            }
            if (title != null) RenderTitle(ui);
            // crosshair last so nothing covers it
            if (!HideGui && !thirdPersonActive()) Crosshair(ui);
        }

        bool thirdPersonActive() => gm?.player != null && gm.player.ThirdPerson;

        static Texture2D fireStrip;
        static int fireFrames;
        /// <summary>The fire block animation frames stacked into one texture (frame 0 at the top) for the burn overlay.</summary>
        static Texture2D FireStrip(out int frames)
        {
            frames = fireFrames;
            if (fireStrip != null) return fireStrip;
            if (!Tex.Has("fire")) return null;
            int baseLayer = Tex.Id("fire");
            int n = Mathf.Max(1, Tex.Frames(baseLayer));
            var px = new Color32[16 * 16 * n];
            for (int f = 0; f < n; f++)
            {
                var layer = Res.GetLayerPixels(baseLayer + f); // top-down rows
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++)
                    {
                        // texture rows run bottom-up: frame 0 occupies the top 16 rows
                        int row = (n - 1 - f) * 16 + (15 - y);
                        px[row * 16 + x] = layer != null && layer.Length >= 256 ? layer[y * 16 + x] : default;
                    }
            }
            fireStrip = new Texture2D(16, 16 * n, TextureFormat.RGBA32, false) { name = "fire_overlay", filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp };
            fireStrip.SetPixels32(px);
            fireStrip.Apply(false, true);
            fireFrames = n;
            frames = n;
            return fireStrip;
        }

        void RenderTitle(UiRenderer ui)
        {
            float alpha = 1f;
            float scaleF = 1f;
            if (titleTicks < titleFadeIn) { alpha = titleTicks / (float)titleFadeIn; scaleF = Mathf.Lerp(1.4f, 1f, alpha); }
            else if (titleTicks > titleFadeIn + titleStay) { alpha = Mathf.Clamp01(1f - (titleTicks - titleFadeIn - titleStay) / (float)titleFadeOut); }
            alpha = Mathf.Clamp01(alpha);
            if (alpha <= 0) return;
            var c = new Color32(255, 255, 255, (byte)(alpha * 255));
            ui.Text(title, W * 0.5f, H * 0.32f, c, 2, true, 4f * scaleF);
            if (!string.IsNullOrEmpty(subtitle)) ui.Text(subtitle, W * 0.5f, H * 0.32f + 26f, c, 2, true, 2f * scaleF);
        }

        void Crosshair(UiRenderer ui)
        {
            var p = gm?.player;
            var c = new Color32(255, 255, 255, 235);
            if (p != null && p.world != null)
            {
                // the crosshair inverts over bright backgrounds and turns red when aiming at an attackable entity
                var hit = p.RaycastEntity(p.EntityReach);
                if (hit is LivingEntity le && le.IsAlive && le.Attackable) c = new Color32(255, 70, 70, 235);
            }
            ui.Icon("crosshair", W * 0.5f - 7.5f, H * 0.5f - 7.5f, c);
            // attack cooldown indicator under the crosshair
            if (p != null && p.IsSurvivalLike && p.AttackStrength() < 0.95f)
            {
                float a = p.AttackStrength();
                ui.IconPart("attack_bg", W * 0.5f - 8f, H * 0.5f + 6f, 0, 0, 16, 4, c);
                int wpx = Mathf.CeilToInt(14 * a);
                if (wpx > 0) ui.IconPart("attack_fill", W * 0.5f - 7f, H * 0.5f + 7f, 0, 0, wpx, 2, c);
            }
        }

        /// <summary>Item icon with the stack count and durability bar.</summary>
        public void DrawItemWithCount(UiRenderer ui, ItemStack s, float x, float y, Player p)
        {
            if (s == null || s.IsEmpty) return;
            ItemIcons.Draw(ui, s, x, y, 16f);
            if (s.count > 1)
            {
                string n = s.count.ToString();
                ui.Text(n, x + 16f, y + 9f, Styles.Text, 1, false);
            }
            if (s.damage > 0 && s.MaxDamage > 0)
            {
                float f = s.DurabilityFraction;
                var col = f > 0.6f ? Styles.Green : f > 0.25f ? Styles.Yellow : Styles.Red;
                ui.Rect(x + 2f, y + 13f, 13f, 2f, new Color32(0, 0, 0, 255));
                ui.Rect(x + 2f, y + 13f, Mathf.Max(1f, 13f * f), 1f, col);
            }
        }

        // ------------------------------------------------------------------ debug screen
        void RenderDebug(UiRenderer ui)
        {
            var p = gm?.player;
            var w = p?.world;
            var sb = new System.Text.StringBuilder(1024);
            sb.Append("Minecraft recreation (MCR) ").Append(GameManager.VersionString).Append('\n');
            sb.Append(fps).Append(" fps   ").Append(screenW).Append('x').Append(screenH).Append("   gui x").Append(scale.ToString("0")).Append('\n');
            if (p != null)
            {
                sb.Append("XYZ: ").Append(p.position.x.ToString("0.000")).Append(" / ").Append(p.position.y.ToString("0.000")).Append(" / ").Append(p.position.z.ToString("0.000")).Append('\n');
                sb.Append("Block: ").Append(Mathf.FloorToInt(p.position.x)).Append(' ').Append(Mathf.FloorToInt(p.position.y)).Append(' ').Append(Mathf.FloorToInt(p.position.z)).Append("   Chunk: ")
                  .Append(Mathf.FloorToInt(p.position.x) >> 4).Append(' ').Append(Mathf.FloorToInt(p.position.z) >> 4).Append('\n');
                sb.Append("Facing: ").Append(CompassFacing(p.yaw)).Append(" (").Append(MathX.WrapAngle(p.yaw).ToString("0.0")).Append(" / ").Append(p.pitch.ToString("0.0")).Append(")\n");
                if (w != null)
                {
                    sb.Append("Biome: ").Append(w.GetSurfaceBiome(Mathf.FloorToInt(p.position.x), Mathf.FloorToInt(p.position.z)).name).Append('\n');
                    sb.Append("Light: ").Append(w.GetLightLevel(Int3.Floor(p.position))).Append(" (sky ").Append(w.GetSkyLight(Int3.Floor(p.position))).Append(", block ").Append(w.GetBlockLight(Int3.Floor(p.position))).Append(")\n");
                    bool canSee = w.CanSeeSky(Int3.Floor(p.position));
                    sb.Append("Time: ").Append(w.session.TimeOfDayTicks.ToString("0000")).Append(" (day ").Append(w.session.DayCount).Append(")  ").Append(canSee ? "sky visible" : "sky hidden").Append('\n');
                    sb.Append("Dimension: ").Append(w.dim).Append("  Seed: ").Append(w.seed).Append('\n');
                    sb.Append("Chunks: ").Append(w.chunks.Count).Append(" loaded, ").Append(gm != null && gm.chunks != null ? gm.chunks.RenderCount : 0).Append(" rendered\n");
                    sb.Append("Entities: ").Append(w.entities.Count).Append("  Particles: ").Append(Particles.Count).Append('\n');
                }
                sb.Append("Health: ").Append(p.health.ToString("0.0")).Append('/').Append(p.maxHealth.ToString("0")).Append("  Food: ").Append(p.hunger.food).Append("  Air: ").Append(p.air).Append('\n');
                sb.Append("Mode: ").Append(p.gameMode).Append("  Difficulty: ").Append(w != null && w.session != null ? w.session.difficulty.ToString() : "?")
                  .Append("  Fly: ").Append(p.abilities.flying ? "on" : "off").Append('\n');
            }
            sb.Append("Targeted: ").Append(TargetedDescription(p));
            var text = sb.ToString();
            var lines = text.Split('\n');
            int widest = 0;
            foreach (var l in lines) widest = Mathf.Max(widest, (int)ui.TextWidth(l));
            ui.Rect(1f, 1f, widest + 3f, lines.Length * 9f + 2f, new Color32(0, 0, 0, 130));
            ui.Text(text, 2f, 2f, new Color32(255, 255, 255, 255), 0);
            if (debugCharts) RenderDebugCharts(ui, p);
        }

        static string CompassFacing(float yaw)
        {
            float y = MathX.WrapAngle(yaw);
            if (y < 0) y += 360f;
            string[] dirs = { "south", "south-west", "west", "north-west", "north", "north-east", "east", "south-east" };
            int i = Mathf.RoundToInt(y / 45f) % 8;
            string axis = i % 2 == 0 ? " (+Z)" : "";
            if (i == 4) axis = " (-Z)";
            if (i == 2) axis = " (-X)";
            if (i == 6) axis = " (+X)";
            return dirs[i] + axis;
        }

        string TargetedDescription(Player p)
        {
            if (p == null) return "none";
            if (p.RaycastBlocks(false, out var hit))
            {
                var b = Blocks.ByState[hit.state];
                return b.id + " at " + hit.pos.x + " " + hit.pos.y + " " + hit.pos.z;
            }
            return "none";
        }

        void RenderDebugCharts(UiRenderer ui, Player p)
        {
            // tiny frame-time and ping history graphs, drawn as bars in the lower right
            float x0 = W - 124f, y0 = H - 64f;
            ui.Rect(x0 - 2f, y0 - 2f, 122f, 62f, new Color32(0, 0, 0, 140));
            for (int i = 0; i < 60; i++)
            {
                float h = Mathf.Clamp(frameTimes[(frameHead + i) % frameTimes.Length] / 50f, 0, 1) * 30f;
                var c = h > 20f ? Styles.Red : h > 12f ? Styles.Yellow : Styles.Green;
                ui.Rect(x0 + i * 2f, y0 + 30f - h, 1f, h, c);
            }
            ui.Text("Frame time (ms)", x0, y0 - 10f, Styles.Text, 1);
        }

        readonly float[] frameTimes = new float[60];
        int frameHead;
        public void RecordFrameTime(float ms) { frameTimes[frameHead] = ms; frameHead = (frameHead + 1) % frameTimes.Length; }

        public static bool Available => true;
    }
}
