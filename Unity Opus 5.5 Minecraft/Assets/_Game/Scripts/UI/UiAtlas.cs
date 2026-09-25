using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// The interface sprite sheet, painted entirely in code at startup: font glyphs, a white texel for solid fills,
    /// HUD icons (hearts, hunger, armour, air), the hotbar, crosshair, container widgets (arrows, flames, bubbles),
    /// creative tabs and scrollbar pieces. Regions are shelf-packed; lookups return UVs.
    /// </summary>
    public static class UiAtlas
    {
        public const int Size = 512;
        static Texture2D tex;
        static Color32[] px;
        static int shelfX, shelfY, shelfH;
        static readonly Dictionary<string, RectInt> regions = new Dictionary<string, RectInt>();
        static readonly Dictionary<char, RectInt> glyphRegions = new Dictionary<char, RectInt>();
        static Vector4 whiteUV;

        public static Texture2D Tex { get { Ensure(); return tex; } }
        /// <summary>Kept for API symmetry with <see cref="UiRenderer.Rect"/>: solid fills sample the atlas' white texel.</summary>
        public static Texture2D White => Tex;
        public static Vector4 WhiteUV { get { Ensure(); return whiteUV; } }

        public static bool Has(string name) { Ensure(); return regions.ContainsKey(name); }
        public static RectInt Region(string name) { Ensure(); return regions.TryGetValue(name, out var r) ? r : new RectInt(0, 0, 1, 1); }

        /// <summary>UVs (u0, v0, u1, v1) for a named region, flipped so v0 is the top edge in screen space.</summary>
        public static Vector4 UV(string name) => UV(Region(name));
        public static Vector4 UV(RectInt r) => new Vector4(r.x / (float)Size, 1f - r.y / (float)Size, (r.x + r.width) / (float)Size, 1f - (r.y + r.height) / (float)Size);
        /// <summary>UVs for a sub-rectangle of a named region (pixel offsets inside the region).</summary>
        public static Vector4 UVSub(string name, int x, int y, int w, int h)
        {
            var r = Region(name);
            return UV(new RectInt(r.x + x, r.y + y, w, h));
        }

        public static Vector4 GlyphUV(char c)
        {
            Ensure();
            if (!glyphRegions.TryGetValue(c, out var r)) r = glyphRegions['?'];
            return UV(r);
        }

        // ------------------------------------------------------------------ packing
        static RectInt Alloc(int w, int h)
        {
            if (shelfX + w + 1 > Size) { shelfX = 0; shelfY += shelfH + 1; shelfH = 0; }
            var r = new RectInt(shelfX, shelfY, w, h);
            shelfX += w + 1;
            shelfH = Mathf.Max(shelfH, h);
            return r;
        }

        static void Put(RectInt r, int x, int y, Color32 c)
        {
            if (x < 0 || y < 0 || x >= r.width || y >= r.height) return;
            int ax = r.x + x, ay = r.y + y; // top-down in atlas space; flipped on upload
            px[ay * Size + ax] = c;
        }

        static RectInt Add(string name, int w, int h, System.Action<RectInt> paint)
        {
            var r = Alloc(w, h);
            paint(r);
            regions[name] = r;
            return r;
        }

        static RectInt Template(string name, string[] rows, Dictionary<char, Color32> pal)
        {
            int w = 0; foreach (var row in rows) w = Mathf.Max(w, row.Length);
            return Add(name, w, rows.Length, r =>
            {
                for (int y = 0; y < rows.Length; y++)
                    for (int x = 0; x < rows[y].Length; x++)
                        if (pal.TryGetValue(rows[y][x], out var c)) Put(r, x, y, c);
            });
        }

        static Dictionary<char, Color32> Pal(params object[] kv)
        {
            var d = new Dictionary<char, Color32>();
            for (int i = 0; i + 1 < kv.Length; i += 2) d[(char)kv[i]] = (Color32)kv[i + 1];
            return d;
        }
        static Color32 C(uint rgb, byte a = 255) => new Color32((byte)(rgb >> 16), (byte)(rgb >> 8), (byte)rgb, a);

        // ------------------------------------------------------------------ build
        static void Ensure()
        {
            if (tex != null) return;
            px = new Color32[Size * Size];
            shelfX = shelfY = shelfH = 0;
            regions.Clear(); glyphRegions.Clear();
            var white = Add("white", 4, 4, r => { for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) Put(r, x, y, new Color32(255, 255, 255, 255)); });
            whiteUV = UV(new RectInt(white.x + 1, white.y + 1, 2, 2));
            BuildFont();
            BuildHud();
            BuildWidgets();
            // upload bottom-up
            var flipped = new Color32[px.Length];
            for (int y = 0; y < Size; y++) System.Array.Copy(px, y * Size, flipped, (Size - 1 - y) * Size, Size);
            tex = new Texture2D(Size, Size, TextureFormat.RGBA32, false) { name = "MCR_UI", filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp };
            tex.SetPixels32(flipped);
            tex.Apply(false, false);
            px = null;
        }

        static void BuildFont()
        {
            var white = new Color32(255, 255, 255, 255);
            for (int code = 32; code < 127; code++)
            {
                char c = (char)code;
                var glyph = FontData.Glyph(c);
                var (left, width) = FontData.Metrics(c);
                int w = Mathf.Max(1, width);
                var r = Alloc(w, FontData.GlyphH);
                for (int gy = 0; gy < FontData.GlyphH; gy++)
                    for (int gx = 0; gx < w; gx++)
                        if ((glyph[gy] & (1 << (FontData.GlyphW - 1 - (gx + left)))) != 0) Put(r, gx, gy, white);
                glyphRegions[c] = r;
            }
        }

        static void BuildHud()
        {
            var o = C(0x000000); // outline
            // hearts: container, full, half, and effect variants
            string[] heart =
            {
                ".oo...oo.",
                "oRRo.oRRo",
                "oRWRoRRRo",
                "oRRRRRRRo",
                "oRRRRRRRo",
                ".oRRRRRo.",
                "..oRRRo..",
                "...oRo...",
                "....o....",
            };
            Template("heart_container", Mask(heart, 'R', 'D'), Pal('o', o, 'D', C(0x3A0E0E), 'W', C(0x3A0E0E)));
            Template("heart_container_blink", Mask(heart, 'R', 'D'), Pal('o', C(0xFFFFFF), 'D', C(0x3A0E0E), 'W', C(0x3A0E0E)));
            void HeartSet(string key, Color32 main, Color32 hi, Color32 dark)
            {
                Template("heart_" + key, heart, Pal('o', o, 'R', main, 'W', hi));
                Template("heart_" + key + "_half", Half(heart), Pal('o', o, 'R', main, 'W', hi, 'D', C(0x3A0E0E)));
            }
            HeartSet("full", C(0xE01B1B), C(0xFFB0B0), C(0x8A0E0E));
            HeartSet("poisoned", C(0x8AA83A), C(0xD8F0A0), C(0x4A6020));
            HeartSet("withered", C(0x3A3A3A), C(0x8A8A8A), C(0x1A1A1A));
            HeartSet("absorbing", C(0xE8C020), C(0xFFF0A0), C(0x9A7A10));
            HeartSet("frozen", C(0x6AB8E8), C(0xD8F0FF), C(0x2A6A9A));
            // hunger drumstick
            string[] food =
            {
                ".....ooo.",
                "....oBBBo",
                "...oBBHBo",
                "..oBBBBBo",
                ".oBBBBBo.",
                "oWoBBBo..",
                "oWWoBo...",
                ".ooo.....",
                ".........",
            };
            Template("food_container", Mask(food, 'B', 'D', 'H'), Pal('o', o, 'D', C(0x2A1A0E), 'W', C(0x2A1A0E)));
            Template("food_full", food, Pal('o', o, 'B', C(0xB8652A), 'H', C(0xF0A060), 'W', C(0xF0F0E8)));
            Template("food_half", HalfRight(food), Pal('o', o, 'B', C(0xB8652A), 'H', C(0xF0A060), 'W', C(0xF0F0E8), 'D', C(0x2A1A0E)));
            Template("food_hunger", food, Pal('o', o, 'B', C(0x6A8A3A), 'H', C(0xA8C870), 'W', C(0xD0E0C0)));
            Template("food_hunger_half", HalfRight(food), Pal('o', o, 'B', C(0x6A8A3A), 'H', C(0xA8C870), 'W', C(0xD0E0C0), 'D', C(0x2A1A0E)));
            // armour chestplate
            string[] armor =
            {
                "ooo...ooo",
                "oSSoooSSo",
                "oSHSSSSSo",
                ".oSSSSSo.",
                ".oSSSSSo.",
                ".oSSSSSo.",
                ".oSSSSSo.",
                ".ooooooo.",
                ".........",
            };
            Template("armor_empty", Mask(armor, 'S', 'D', 'H'), Pal('o', o, 'D', C(0x2A2A2A)));
            Template("armor_full", armor, Pal('o', o, 'S', C(0xD8D8D8), 'H', C(0xFFFFFF)));
            Template("armor_half", Half(armor, 'S', 'D', 'H'), Pal('o', o, 'S', C(0xD8D8D8), 'H', C(0xFFFFFF), 'D', C(0x2A2A2A)));
            // air bubbles
            string[] bubble =
            {
                "..ooooo..",
                ".oBBBBBo.",
                "oBWWBBBBo",
                "oBWBBBBBo",
                "oBBBBBBBo",
                "oBBBBBBBo",
                ".oBBBBBo.",
                "..ooooo..",
                ".........",
            };
            Template("air_full", bubble, Pal('o', C(0x1E5AC8), 'B', C(0x5AA0F0, 160), 'W', C(0xFFFFFF)));
            Template("air_pop", new[]
            {
                "..o...o..",
                ".o.....o.",
                "o.......o",
                ".........",
                "o.......o",
                ".........",
                ".o.....o.",
                "..o...o..",
                ".........",
            }, Pal('o', C(0x5AA0F0)));
            // horse/mount hearts share the heart shape, tinted pale
            HeartSet("mount", C(0xE8E0D0), C(0xFFFFFF), C(0x8A8070));
            // crosshair
            Add("crosshair", 15, 15, r =>
            {
                var c = new Color32(255, 255, 255, 230);
                for (int i = 0; i < 15; i++) { if (i < 6 || i > 8) { Put(r, i, 7, c); Put(r, 7, i, c); } }
                Put(r, 7, 7, c);
            });
            // attack indicator (below crosshair) background + fill
            Add("attack_bg", 16, 4, r => { for (int x = 0; x < 16; x++) for (int y = 0; y < 4; y++) Put(r, x, y, (x == 0 || y == 0 || x == 15 || y == 3) ? C(0x000000, 200) : C(0x2A2A2A, 200)); });
            Add("attack_fill", 16, 4, r => { for (int x = 1; x < 15; x++) for (int y = 1; y < 3; y++) Put(r, x, y, C(0xE0E0E0)); });
            // hotbar and selection frame
            Add("hotbar", 182, 22, r =>
            {
                for (int x = 0; x < 182; x++)
                    for (int y = 0; y < 22; y++)
                    {
                        bool border = x == 0 || y == 0 || x == 181 || y == 21;
                        Color32 c = border ? C(0x000000, 220) : C(0x1E1E1E, 150);
                        // slot frames every 20px
                        int sx = (x - 1) % 20;
                        if (!border && (sx == 0 || sx == 19 || y == 1 || y == 20)) c = C(0x8A8A8A, 230);
                        if (!border && (sx == 1 || y == 2)) c = C(0x5A5A5A, 200);
                        Put(r, x, y, c);
                    }
            });
            Add("hotbar_selection", 24, 24, r =>
            {
                for (int x = 0; x < 24; x++)
                    for (int y = 0; y < 24; y++)
                    {
                        bool outer = x == 0 || y == 0 || x == 23 || y == 23;
                        bool inner = x == 1 || y == 1 || x == 22 || y == 22;
                        if (outer) Put(r, x, y, C(0x000000, 230));
                        else if (inner) Put(r, x, y, C(0xFFFFFF));
                        else if (x == 2 || y == 2 || x == 21 || y == 21) Put(r, x, y, C(0xA0A0A0));
                    }
            });
            Add("offhand_slot", 22, 24, r =>
            {
                for (int x = 0; x < 22; x++)
                    for (int y = 0; y < 24; y++)
                    {
                        bool border = x == 0 || y == 0 || x == 21 || y == 23;
                        Put(r, x, y, border ? C(0x000000, 220) : (x == 1 || y == 1 || x == 20 || y == 22) ? C(0x8A8A8A, 230) : C(0x1E1E1E, 150));
                    }
            });
            // experience bar
            Add("xp_bg", 182, 5, r => { for (int x = 0; x < 182; x++) for (int y = 0; y < 5; y++) Put(r, x, y, (y == 0 || y == 4 || x == 0 || x == 181) ? C(0x000000) : C(0x1A2A10)); });
            Add("xp_fill", 182, 5, r => { for (int x = 1; x < 181; x++) for (int y = 1; y < 4; y++) Put(r, x, y, y == 1 ? C(0xC8FF6A) : C(0x80E020)); });
            // boss bar
            Add("boss_bg", 182, 5, r => { for (int x = 0; x < 182; x++) for (int y = 0; y < 5; y++) Put(r, x, y, (y == 0 || y == 4 || x == 0 || x == 181) ? C(0x000000) : C(0x3A0A3A)); });
            Add("boss_fill_pink", 182, 5, r => { for (int x = 1; x < 181; x++) for (int y = 1; y < 4; y++) Put(r, x, y, y == 1 ? C(0xFF8AF0) : C(0xE040C8)); });
            Add("boss_fill_purple", 182, 5, r => { for (int x = 1; x < 181; x++) for (int y = 1; y < 4; y++) Put(r, x, y, y == 1 ? C(0xC08AFF) : C(0x8A40D8)); });
        }

        static void BuildWidgets()
        {
            var o = C(0x000000);
            // crafting / furnace arrows: outline + fill states
            Add("arrow_empty", 22, 15, r => Arrow(r, C(0x8B8B8B), C(0x6A6A6A)));
            Add("arrow_full", 22, 15, r => Arrow(r, C(0xFFFFFF), C(0xD8D8D8)));
            // furnace flame
            string[] flame =
            {
                "......oo......",
                ".....oYYo.....",
                ".....oYYo.....",
                "....oYOOYo....",
                "...oYOOOOYo...",
                "...oYOROOYo...",
                "..oYOORROOYo..",
                "..oYORRRROYo..",
                ".oYOORRRROOYo.",
                ".oYORRRRRROYo.",
                ".oYORRRRRROYo.",
                "..oYORRRROYo..",
                "...oYOOOOYo...",
                "....oooooo....",
            };
            Template("flame_full", flame, Pal('o', C(0x5A2A00), 'Y', C(0xFFE040), 'O', C(0xFF8A00), 'R', C(0xE83A00)));
            Template("flame_empty", flame, Pal('o', C(0x5A5A5A), 'Y', C(0x7A7A7A), 'O', C(0x6A6A6A), 'R', C(0x6A6A6A)));
            // brewing: bubbles column and vertical progress arrow
            Add("brew_bubbles_full", 12, 29, r => Bubbles(r, C(0xFFFFFF)));
            Add("brew_bubbles_empty", 12, 29, r => Bubbles(r, C(0x6A6A6A)));
            Add("brew_arrow_full", 9, 28, r => DownArrow(r, C(0xFFFFFF)));
            Add("brew_arrow_empty", 9, 28, r => DownArrow(r, C(0x6A6A6A)));
            Add("brew_fuel", 18, 4, r => { for (int x = 0; x < 18; x++) for (int y = 0; y < 4; y++) Put(r, x, y, y == 0 ? C(0xFFC060) : C(0xE08A20)); });
            // scrollbar knob (enabled / disabled)
            Add("scroll_knob", 12, 15, r => Knob(r, C(0xC6C6C6), C(0xFFFFFF), C(0x555555)));
            Add("scroll_knob_off", 12, 15, r => Knob(r, C(0x9A9A9A), C(0xB0B0B0), C(0x555555)));
            // creative tab shapes (26x32): selected tab merges with the panel below it
            Add("tab_top", 26, 32, r => Tab(r, false, false));
            Add("tab_top_sel", 26, 32, r => Tab(r, true, false));
            Add("tab_bottom", 26, 32, r => Tab(r, false, true));
            Add("tab_bottom_sel", 26, 32, r => Tab(r, true, true));
            // enchantment option plates
            Add("enchant_slot", 108, 19, r => Plate(r, C(0x8A6A5A), C(0xB09080), C(0x5A3A2A)));
            Add("enchant_slot_hover", 108, 19, r => Plate(r, C(0xA07A68), C(0xD0B0A0), C(0x5A3A2A)));
            Add("enchant_slot_off", 108, 19, r => Plate(r, C(0x6A5A52), C(0x7A6A62), C(0x4A3A32)));
            // villager trade arrow and level bar
            Add("trade_arrow", 10, 9, r =>
            {
                for (int y = 0; y < 9; y++) for (int x = 0; x < 10; x++)
                    {
                        bool shaft = y >= 3 && y <= 5 && x < 6;
                        bool head = x >= 5 && Mathf.Abs(y - 4) <= (9 - x);
                        if (shaft || head) Put(r, x, y, C(0xE0E0E0));
                    }
            });
            Add("trade_x", 10, 9, r => { for (int i = 0; i < 9; i++) { Put(r, i, i, C(0xE02020)); Put(r, i + 1, i, C(0xE02020)); Put(r, 8 - i, i, C(0xE02020)); Put(r, 9 - i, i, C(0xE02020)); } });
            // beacon confirm/cancel check marks
            Add("check", 11, 9, r => { int[] ys = { 5, 6, 7, 6, 5, 4, 3, 2, 1, 0, 0 }; for (int x = 0; x < 11; x++) { Put(r, x, ys[x], C(0x55FF55)); Put(r, x, ys[x] + 1, C(0x2A9A2A)); } });
            Add("cross", 9, 9, r => { for (int i = 0; i < 9; i++) { Put(r, i, i, C(0xFF5555)); Put(r, 8 - i, i, C(0xFF5555)); } });
            // anvil hammer icon and the "too expensive" bar reuse text; recipe book toggle omitted
            // mouse pointer for gamepad-less menus is the OS cursor; nothing to draw
            _ = o;
        }

        // ------------------------------------------------------------------ small painters
        static string[] Mask(string[] rows, char fill, char with, char also = '\0')
        {
            var o = new string[rows.Length];
            for (int i = 0; i < rows.Length; i++)
            {
                var r = rows[i].Replace(fill, with);
                if (also != '\0') r = r.Replace(also, with);
                r = r.Replace('W', with).Replace('H', with);
                o[i] = r;
            }
            return o;
        }
        /// <summary>Left half keeps its fill; the right half becomes the empty-container colour 'D'.</summary>
        static string[] Half(string[] rows, char fill = 'R', char empty = 'D', char hi = 'W')
        {
            var o = new string[rows.Length];
            for (int i = 0; i < rows.Length; i++)
            {
                var chars = rows[i].ToCharArray();
                for (int x = chars.Length / 2 + 1; x < chars.Length; x++) if (chars[x] == fill || chars[x] == hi) chars[x] = empty;
                o[i] = new string(chars);
            }
            return o;
        }
        /// <summary>Hunger empties from the left (the drumstick bone is on the left).</summary>
        static string[] HalfRight(string[] rows)
        {
            var o = new string[rows.Length];
            for (int i = 0; i < rows.Length; i++)
            {
                var chars = rows[i].ToCharArray();
                for (int x = 0; x < chars.Length / 2; x++) if (chars[x] == 'B' || chars[x] == 'H' || chars[x] == 'W') chars[x] = 'D';
                o[i] = new string(chars);
            }
            return o;
        }

        static void Arrow(RectInt r, Color32 fill, Color32 edge)
        {
            for (int y = 0; y < 15; y++)
                for (int x = 0; x < 22; x++)
                {
                    bool shaft = y >= 5 && y <= 9 && x < 14;
                    bool head = x >= 13 && Mathf.Abs(y - 7) <= (21 - x);
                    if (!(shaft || head)) continue;
                    bool border = (shaft && (y == 5 || y == 9) && x < 14) || (head && Mathf.Abs(y - 7) == (21 - x));
                    Put(r, x, y, border ? edge : fill);
                }
        }

        static void Bubbles(RectInt r, Color32 c)
        {
            int[][] dots = { new[] { 2, 26 }, new[] { 6, 22 }, new[] { 3, 18 }, new[] { 8, 14 }, new[] { 4, 10 }, new[] { 7, 6 }, new[] { 5, 2 } };
            foreach (var d in dots)
                for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (Mathf.Abs(dx) + Mathf.Abs(dy) < 2) Put(r, d[0] + dx, d[1] + dy, c);
        }

        static void DownArrow(RectInt r, Color32 c)
        {
            for (int y = 0; y < 28; y++)
                for (int x = 0; x < 9; x++)
                {
                    bool shaft = x >= 3 && x <= 5 && y < 22;
                    bool head = y >= 21 && Mathf.Abs(x - 4) <= (27 - y);
                    if (shaft || head) Put(r, x, y, c);
                }
        }

        static void Knob(RectInt r, Color32 fill, Color32 hi, Color32 dark)
        {
            for (int y = 0; y < 15; y++)
                for (int x = 0; x < 12; x++)
                {
                    Color32 c = fill;
                    if (x == 0 || y == 0) c = hi;
                    if (x == 11 || y == 14) c = dark;
                    Put(r, x, y, c);
                }
        }

        static void Tab(RectInt r, bool selected, bool bottom)
        {
            var fill = selected ? C(0xC6C6C6) : C(0x9A9A9A);
            for (int y = 0; y < 32; y++)
                for (int x = 0; x < 26; x++)
                {
                    int yy = bottom ? 31 - y : y;
                    bool corner = (x < 2 && yy < 2) || (x > 23 && yy < 2);
                    if (corner) continue;
                    if (!selected && yy > 28) continue; // unselected tabs stop short of the panel
                    Color32 c = fill;
                    if (x == 0 || x == 25 || yy == 0 || (x == 1 && yy == 1) || (x == 24 && yy == 1)) c = C(0x000000);
                    else if (x == 1 || yy == 1) c = C(0xFFFFFF);
                    else if (x == 24) c = C(0x555555);
                    Put(r, x, y, c);
                }
        }

        static void Plate(RectInt r, Color32 fill, Color32 hi, Color32 dark)
        {
            for (int y = 0; y < r.height; y++)
                for (int x = 0; x < r.width; x++)
                {
                    Color32 c = fill;
                    if (x == 0 || y == 0) c = hi;
                    if (x == r.width - 1 || y == r.height - 1) c = dark;
                    Put(r, x, y, c);
                }
        }

        public static void Clear()
        {
            if (tex != null) Object.Destroy(tex);
            tex = null;
        }
    }
}
