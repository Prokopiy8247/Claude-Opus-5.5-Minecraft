using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Second-stage procedural recipes, reached from the tail of <see cref="Specific"/>: the dye-coloured
    /// families (beds, candles, shulker boxes) and the workstations. Sibling partial files hold furniture,
    /// plants, coral, sculk and mob heads. Every pixel is produced by code in these files.
    ///
    /// Several of these tiles are only ever sampled through sub-rectangles by their block models (candles,
    /// bed sides, lecterns, stonecutter saws, ...); the comments name the visible window so the art stays
    /// aligned with the geometry in Blocks/FunctionalBlocks.cs. Remember generator row 0 is the top of the
    /// tile, i.e. uv v = 1.
    /// </summary>
    public static partial class TextureGen
    {
        /// <summary>Bump when any generator changes, so baked texture arrays are rebuilt instead of reused.</summary>
        public const int Version = 2;

        /// <summary>Dispatcher for everything Specific() does not know. Returns null when nothing matched.</summary>
        static Img MoreTextures(Img i, string n, int frame)
        {
            // deterministic per-texture noise stream, independent of generation order
            nseed = (uint)Hash.StringHash(n) ^ 0x9E3779B9u;
            return Beds(i, n) ?? Candles(i, n) ?? ShulkerBoxes(i, n) ?? Workstations(i, n)
                ?? Furniture(i, n) ?? Plants(i, n) ?? CoralFamily(i, n) ?? SculkFamily(i, n) ?? Heads(i, n);
        }

        // ==================================================================== shared helpers

        [ThreadStatic] static uint nseed;
        static float Rnd01()
        {
            nseed = nseed * 1664525u + 1013904223u;
            uint x = nseed; x ^= x >> 15; x *= 0x2c1b3c6du; x ^= x >> 12;
            return (x >> 8) / 16777216f;
        }
        static int RndInt(int n) => n <= 1 ? 0 : (int)(Rnd01() * n) % n;

        /// <summary>Tone shift that stays visible on very dark and very light colours: f &gt; 1 lifts toward white, f &lt; 1 sinks toward black.</summary>
        static Color32 Tone(Color32 c, float f)
        {
            var r = f >= 1f ? Img.Mix(c, C(0xFFFFFF), Mathf.Min(1f, f - 1f)) : Img.Mix(c, C(0x000000), Mathf.Min(1f, 1f - f));
            r.a = c.a;
            return r;
        }
        /// <summary>Tone with per-pixel noise.</summary>
        static Color32 TN(Color32 c, float f, float noise) => Tone(c, f + (Rnd01() - 0.5f) * noise);
        /// <summary>Multiplicative shade with per-pixel noise (the house style for mid-tones).</summary>
        static Color32 Cl(Color32 c, float lit, float n) => Img.Shade(c, lit + (Rnd01() - 0.5f) * n);

        /// <summary>Fill the whole tile with a noisy tone of one colour.</summary>
        static Img FillTN(Img i, Color32 c, float noise)
        {
            for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) i[x, y] = TN(c, 1f, noise);
            return i;
        }

        /// <summary>Rectangle with a raised bevel: light top/left, dark bottom/right.</summary>
        static Img BevelRect(Img i, int x0, int y0, int w, int h, Color32 c, bool sunken = false)
        {
            i.Rect(x0, y0, w, h, c);
            Color32 lt = Tone(c, 1.22f), dk = Tone(c, 0.72f);
            if (sunken) { var t = lt; lt = dk; dk = t; }
            i.HLine(y0, x0, x0 + w - 1, lt);
            i.VLine(x0, y0, y0 + h - 1, lt);
            i.HLine(y0 + h - 1, x0, x0 + w - 1, dk);
            i.VLine(x0 + w - 1, y0 + 1, y0 + h - 1, dk);
            return i;
        }

        /// <summary>Soft blob of pixels (sculk, pustules, moss).</summary>
        static Img Blob(Img i, int cx, int cy, int r, Color32 c, float dense = 0.7f)
        {
            for (int y = cy - r; y <= cy + r; y++)
                for (int x = cx - r; x <= cx + r; x++)
                {
                    float dx = (x - cx) / (float)Math.Max(1, r), dy = (y - cy) / (float)Math.Max(1, r);
                    float d = dx * dx + dy * dy;
                    if (d > 1.15f) continue;
                    if (d > 0.45f && Rnd01() > dense) continue;
                    i.Set(x, y, c);
                }
            return i;
        }

        /// <summary>One-pixel strand along a wobbly line.</summary>
        static void Strand(Img i, float x0, float y0, float x1, float y1, Color32 c, float wobble = 0.35f)
        {
            int steps = Mathf.Max(2, (int)(Mathf.Abs(x1 - x0) + Mathf.Abs(y1 - y0)) * 2);
            for (int s = 0; s <= steps; s++)
            {
                float t = s / (float)steps;
                float x = Mathf.Lerp(x0, x1, t) + (Rnd01() - 0.5f) * wobble;
                float y = Mathf.Lerp(y0, y1, t) + (Rnd01() - 0.5f) * wobble;
                i.Set(Mathf.RoundToInt(x), Mathf.RoundToInt(y), Img.Shade(c, 0.92f + Rnd01() * 0.16f));
            }
        }

        /// <summary>Copies another generated texture into this canvas (keeps our own rng).</summary>
        static Img Base(Img i, string other)
        {
            var src = Make(other, 0);
            if (src != null) i.CopyFrom(src);
            return i;
        }

        /// <summary>Oak-style planks with a darker frame: the body of most wooden workstations.</summary>
        static Img WoodBody(Img i, WoodPal p, float tone = 1f)
        {
            i.Planks(Img.Shade(p.plank, tone), Img.Shade(p.plankDark, tone));
            return i;
        }

        /// <summary>Table apron seen from the side: a top board (rows 0-2) and two legs.</summary>
        static Img TableSide(Img i, WoodPal p, Color32? top = null)
        {
            WoodBody(i, p, 0.92f);
            var board = top ?? p.plank;
            for (int y = 0; y < 3; y++) for (int x = 0; x < 16; x++) i[x, y] = Cl(board, y == 0 ? 1.12f : (y == 2 ? 0.82f : 1f), 0.06f);
            i.HLine(3, 0, 15, Img.Shade(p.plankDark, 0.8f));
            for (int y = 4; y < 16; y++)
            {
                i[0, y] = Img.Shade(p.plankDark, 0.8f); i[1, y] = Img.Shade(p.plank, 1.08f); i[2, y] = p.plankDark;
                i[13, y] = p.plankDark; i[14, y] = Img.Shade(p.plank, 1.02f); i[15, y] = Img.Shade(p.plankDark, 0.8f);
            }
            i.HLine(15, 0, 15, Img.Shade(p.plankDark, 0.75f));
            return i;
        }

        // ==================================================================== beds

        /// <summary>
        /// Beds. The mattress box is 16x6x16 px (y 3..9); tops use the full head / foot tile with world-aligned
        /// uvs (so the pillow is centred rather than tied to one facing), and the sides only ever show rows 7..12
        /// of <c>*_bed_side</c>: blanket on top (7..9), sheet (10..11), dark seam (12).
        /// </summary>
        static Img Beds(Img i, string n)
        {
            int k = n.LastIndexOf("_bed_", StringComparison.Ordinal);
            if (k <= 0) return null;
            string col = n.Substring(0, k), part = n.Substring(k + 5);
            if (!DyeColors.TryGetValue(col, out var dye)) return null;
            if (part != "head" && part != "foot" && part != "side" && part != "top" && part != "bottom") return null;
            Color32 hem = Tone(dye, 0.72f), fold = Tone(dye, 1.2f), stitch = Tone(dye, 0.86f);
            Color32 sheet = C(0xE4DED2), sheetDark = C(0xB9B1A3);
            if (part == "bottom") { var oak = Wood("oak"); return i.Planks(oak.plank, oak.plankDark); }
            // woven blanket everywhere
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float f = ((x + y) & 3) == 0 ? 0.95f : (((x - y) & 3) == 2 ? 1.04f : 1f);
                    i[x, y] = TN(dye, f, 0.05f);
                }
            if (part == "side")
            {
                for (int x = 0; x < 16; x++)
                {
                    i[x, 7] = TN(fold, 1f, 0.03f);
                    i[x, 10] = ((x & 1) == 0) ? hem : Tone(hem, 1.08f);
                    i[x, 11] = TN(sheet, 1f, 0.04f);
                    i[x, 12] = sheetDark;
                    for (int y = 13; y < 16; y++) i[x, y] = Cl(C(0x8A6A3E), y == 13 ? 1.1f : 0.9f, 0.08f);
                    for (int y = 0; y < 7; y++) i[x, y] = TN(dye, y == 6 ? 1.08f : 1f, 0.05f);
                }
                // quilting stitches along the blanket side
                for (int x = 2; x < 16; x += 4) i[x, 8] = stitch;
                return i;
            }
            // top: quilted squares + hem border
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                    if ((x % 5 == 2 && (y & 1) == 0) || (y % 5 == 2 && (x & 1) == 0)) i[x, y] = stitch;
            i.RectOutline(0, 0, 16, 16, hem);
            if (part == "head")
            {
                // centred pillow
                for (int y = 4; y <= 11; y++)
                    for (int x = 2; x <= 13; x++)
                    {
                        bool corner = (x == 2 || x == 13) && (y == 4 || y == 11);
                        if (corner) continue;
                        float f = y == 4 ? 1.06f : (y == 11 ? 0.84f : (x == 13 ? 0.9f : 1f));
                        i[x, y] = TN(sheet, f, 0.03f);
                    }
                i.HLine(8, 4, 11, Img.Shade(sheet, 0.93f)); // crease
                i.Set(3, 5, C(0xF4F0E8)); i.Set(4, 5, C(0xF4F0E8));
                i.HLine(12, 3, 12, Tone(dye, 0.8f));       // pillow shadow on the blanket
            }
            else
            {
                // turned-down fold at one end of the foot (reads as the blanket edge from any direction)
                i.HLine(1, 1, 14, fold);
                i.HLine(14, 1, 14, fold);
            }
            return i;
        }

        // ==================================================================== candles

        /// <summary>
        /// Candles: CandleBlock samples the sides at cols 0-1 / rows 2-7 and the top at cols 0-1 / rows 8-9. The
        /// rest of the tile repeats the wax so breaking particles look right.
        /// </summary>
        static Img Candles(Img i, string n)
        {
            Color32 wax;
            if (n == "candle" || n == "candle_lit") wax = C(0xE6DCC4);
            else if (n.EndsWith("_candle") && DyeColors.TryGetValue(n.Substring(0, n.Length - 7), out var dye)) wax = dye;
            else if (n.EndsWith("_candle_lit") && DyeColors.TryGetValue(n.Substring(0, n.Length - 11), out var dye2)) wax = dye2;
            else return null;
            Color32 lit = Tone(wax, 1.22f), shade = Tone(wax, 0.74f), drip = Tone(wax, 1.1f);
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                    i[x, y] = TN((x & 1) == 0 ? lit : shade, 1f, 0.04f);
            // a few wax drips running down the body (visible window rows 2-7)
            for (int x = 0; x < 16; x += 3) { int len = 1 + (int)(Hash.Get(x, 71) % 3); for (int y = 2; y < 2 + len; y++) i[x, y] = drip; }
            // top face (rows 8-9, cols 0-1): melted rim with the wick
            i[0, 8] = C(0x2A2420); i[1, 8] = Tone(wax, 1.1f);
            i[0, 9] = Tone(wax, 1.05f); i[1, 9] = Tone(wax, 0.92f);
            // rim highlight at the top of the body
            i[0, 2] = Tone(wax, 1.3f); i[1, 2] = Tone(wax, 1.05f);
            return i;
        }

        // ==================================================================== shulker boxes

        /// <summary>Shulker box shell: lid (rows 0-6), seam, base with ribbing. Same tile for every face.</summary>
        static Img ShulkerBoxes(Img i, string n)
        {
            Color32 shell;
            if (n == "shulker_box") shell = C(0x976A98);
            else if (n.EndsWith("_shulker_box") && DyeColors.TryGetValue(n.Substring(0, n.Length - 12), out var dye)) shell = dye;
            else return null;
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float f;
                    if (y < 7) f = y == 0 ? 1.22f : (y == 6 ? 0.8f : 1.1f);      // lid
                    else if (y == 7) f = 0.55f;                                     // seam
                    else f = y == 15 ? 0.74f : (y == 8 ? 1.04f : 0.96f);            // base
                    if (y > 8 && y < 15 && (x == 4 || x == 11)) f *= 0.9f;          // ribs
                    if (x == 0 || x == 15) f *= 0.86f;
                    i[x, y] = TN(shell, f, 0.04f);
                }
            // lid crown plate
            for (int x = 3; x <= 12; x++) { i[x, 2] = Tone(shell, 1.28f); i[x, 3] = Tone(shell, 1.16f); }
            i[3, 2] = Tone(shell, 1.1f); i[12, 2] = Tone(shell, 1.1f);
            return i;
        }

        // ==================================================================== workstations

        static Img Workstations(Img i, string n)
        {
            var oak = Wood("oak");
            var birch = Wood("birch");
            var darkOak = Wood("dark_oak");
            switch (n)
            {
                // -------------------------------------------------- crafting table
                case "crafting_table_top":
                    {
                        WoodBody(i, oak, 0.9f);
                        i.RectOutline(0, 0, 16, 16, Img.Shade(oak.plankDark, 0.72f));
                        i.RectOutline(1, 1, 14, 14, Img.Shade(oak.plank, 1.08f));
                        // 3x3 inset crafting grid
                        for (int y = 2; y < 14; y++)
                            for (int x = 2; x < 14; x++)
                            {
                                int cx = (x - 2) % 4, cy = (y - 2) % 4;
                                if (cx == 3 || cy == 3) i[x, y] = Img.Shade(oak.plankDark, 0.82f);
                                else if (cx == 0 || cy == 0) i[x, y] = Img.Shade(oak.plankDark, 1.02f);
                                else i[x, y] = Cl(oak.plank, 1.04f, 0.06f);
                            }
                        i.HLine(13, 2, 13, Img.Shade(oak.plankDark, 0.8f));
                        i.VLine(13, 2, 13, Img.Shade(oak.plankDark, 0.8f));
                        return i;
                    }
                case "crafting_table_front":
                    {
                        TableSide(i, oak);
                        // saw on the left panel
                        for (int x = 4; x <= 7; x++) { i[x, 5 + (x - 4)] = C(0xC8C8C8); i[x, 6 + (x - 4)] = C(0x8E8E8E); }
                        i.Set(8, 9, C(0xC8C8C8)); i.Set(8, 10, C(0x8E8E8E));
                        for (int x = 4; x <= 8; x += 2) i.Set(x, 7 + (x - 4), C(0x6E6E6E));
                        i.Rect(3, 4, 2, 2, C(0x6A4A26)); i.Set(3, 4, C(0x8A6436));
                        // hammer on the right panel
                        i.Rect(9, 5, 4, 2, C(0x8C8C92)); i.HLine(5, 9, 12, C(0xB4B4BA));
                        i.VLine(11, 7, 12, C(0x6A4A26)); i.VLine(10, 7, 12, C(0x8A6436));
                        return i;
                    }
                case "crafting_table_side":
                    {
                        TableSide(i, oak);
                        // pegboard with a hanging pickaxe outline
                        i.HLine(5, 4, 11, C(0x8C8C92)); i.Set(4, 6, C(0x8C8C92)); i.Set(11, 6, C(0x8C8C92));
                        i.HLine(4, 5, 10, C(0xB4B4BA));
                        i.VLine(8, 6, 12, C(0x6A4A26)); i.VLine(7, 6, 12, C(0x8A6436));
                        return i;
                    }
                // -------------------------------------------------- cartography table
                case "cartography_table_top":
                    {
                        WoodBody(i, darkOak);
                        i.RectOutline(0, 0, 16, 16, Img.Shade(darkOak.plankDark, 0.8f));
                        // paper sheet with a small map
                        for (int y = 2; y < 14; y++) for (int x = 2; x < 14; x++) i[x, y] = Cl(C(0xE6DEC6), 1f, 0.04f);
                        i.RectOutline(2, 2, 12, 12, C(0xB8AC8C));
                        for (int y = 4; y < 12; y++)
                            for (int x = 4; x < 12; x++)
                            {
                                float v = Mathf.Sin(x * 0.9f) + Mathf.Cos(y * 0.8f + x * 0.3f);
                                i[x, y] = v > 0.4f ? C(0x7EAE5E) : (v > -0.2f ? C(0xD8CCA0) : C(0x6E9AC8));
                            }
                        i.RectOutline(4, 4, 8, 8, C(0x8A7A58));
                        i.Set(7, 7, C(0xC0302A)); i.Set(8, 7, C(0xC0302A)); i.Set(7, 8, C(0xC0302A));
                        // pen and ruler lying on the table
                        i.HLine(14, 3, 9, C(0xB08A50)); i.VLine(13, 3, 8, C(0x3A3A40));
                        return i;
                    }
                case "cartography_table_side":
                    {
                        TableSide(i, darkOak, C(0xC8BC9C));
                        // a rolled map resting on the shelf
                        i.Rect(4, 7, 8, 3, C(0xE6DEC6));
                        i.HLine(7, 4, 11, C(0xF4EEDA)); i.HLine(9, 4, 11, C(0xB8AC8C));
                        i.VLine(4, 7, 9, C(0xC06A3A)); i.VLine(11, 7, 9, C(0xC06A3A));
                        i.HLine(12, 3, 12, Img.Shade(darkOak.plankDark, 0.9f));
                        return i;
                    }
                // -------------------------------------------------- fletching table
                case "fletching_table_top":
                    {
                        WoodBody(i, birch);
                        i.RectOutline(0, 0, 16, 16, Img.Shade(birch.plankDark, 0.8f));
                        i.RectOutline(1, 1, 14, 14, Img.Shade(birch.plank, 1.08f));
                        // two arrows laid diagonally and a feather
                        for (int k = 0; k < 2; k++)
                        {
                            int o = k * 5;
                            for (int s = 0; s < 8; s++) i.Set(3 + s + o / 2, 11 - s + o / 2, C(0x7A5A34));
                            i.Set(11 + o / 2, 3 + o / 2, C(0xD8D8DC)); i.Set(12 + o / 2, 3 + o / 2, C(0x9A9AA0)); i.Set(11 + o / 2, 2 + o / 2, C(0x9A9AA0));
                            i.Set(2 + o / 2, 12 + o / 2, C(0xF0F0F0)); i.Set(3 + o / 2, 12 + o / 2, C(0xE0E0E0));
                        }
                        i.Line(3, 4, 6, 3, C(0xF2F2F2)); i.Set(4, 5, C(0xD8D8D8));
                        return i;
                    }
                case "fletching_table_front":
                case "fletching_table_side":
                    {
                        TableSide(i, birch);
                        if (n.EndsWith("front"))
                        {
                            // target-like roundel
                            i.RectOutline(4, 5, 8, 8, C(0xB03A2A));
                            i.Rect(6, 7, 4, 4, C(0xE8E0CC));
                            i.Rect(7, 8, 2, 2, C(0xB03A2A));
                        }
                        else
                        {
                            // flint pieces and feathers on the shelf
                            i.Rect(4, 9, 2, 2, C(0x4A4A50)); i.Set(4, 9, C(0x6A6A70));
                            i.Line(8, 11, 11, 6, C(0xF0F0F0)); i.Line(9, 11, 12, 7, C(0xD0D0D0));
                        }
                        return i;
                    }
                // -------------------------------------------------- smithing table
                case "smithing_table_top":
                    {
                        FillTN(i, C(0x3C3E46), 0.06f);
                        i.RectOutline(0, 0, 16, 16, C(0x24252A));
                        i.RectOutline(1, 1, 14, 14, C(0x55575F));
                        i.Rect(3, 3, 10, 10, C(0x33353C));
                        i.RectOutline(3, 3, 10, 10, C(0x2A2B31));
                        i.HLine(3, 3, 12, C(0x4A4C54));
                        // rivets
                        foreach (var p in new[] { (2, 2), (13, 2), (2, 13), (13, 13) }) i.Set(p.Item1, p.Item2, C(0x9A9CA4));
                        return i;
                    }
                case "smithing_table_front":
                case "smithing_table_side":
                    {
                        TableSide(i, darkOak, C(0x4A4C54));
                        i.HLine(0, 0, 15, C(0x6A6C74));
                        i.HLine(2, 0, 15, C(0x2E3036));
                        if (n.EndsWith("front"))
                        {
                            // hammer and tongs
                            i.Rect(4, 6, 4, 2, C(0x8C8E96)); i.HLine(6, 4, 7, C(0xB0B2BA));
                            i.VLine(6, 8, 12, C(0x6A4A26));
                            i.Line(10, 6, 12, 12, C(0x55575F)); i.Line(12, 6, 10, 12, C(0x6E7078));
                        }
                        else
                        {
                            // iron corner plates
                            i.Rect(3, 5, 10, 2, C(0x4A4C54)); i.HLine(5, 3, 12, C(0x6A6C74));
                            i.Rect(3, 11, 10, 2, C(0x4A4C54)); i.HLine(11, 3, 12, C(0x6A6C74));
                        }
                        return i;
                    }
                case "smithing_table_bottom":
                    WoodBody(i, darkOak, 0.85f);
                    i.RectOutline(0, 0, 16, 16, Img.Shade(darkOak.plankDark, 0.8f));
                    return i;
                // -------------------------------------------------- loom
                case "loom_top":
                    {
                        WoodBody(i, oak, 1.02f);
                        i.RectOutline(0, 0, 16, 16, Img.Shade(oak.plankDark, 0.72f));
                        // warp threads stretched between two beams
                        i.Rect(1, 2, 14, 2, Img.Shade(oak.plankDark, 1.05f)); i.HLine(2, 1, 14, Img.Shade(oak.plank, 1.12f));
                        i.Rect(1, 12, 14, 2, Img.Shade(oak.plankDark, 1.05f)); i.HLine(12, 1, 14, Img.Shade(oak.plank, 1.12f));
                        for (int x = 2; x < 14; x += 2) i.VLine(x, 4, 11, C(0xE8E0D0));
                        return i;
                    }
                case "loom_side":
                    {
                        TableSide(i, oak);
                        i.VLine(7, 4, 14, Img.Shade(oak.plankDark, 0.9f)); i.VLine(8, 4, 14, Img.Shade(oak.plank, 1.05f));
                        i.Rect(3, 6, 10, 2, C(0xE8E0D0)); i.HLine(7, 3, 12, C(0xC8BEAA));
                        return i;
                    }
                case "loom_front":
                    {
                        TableSide(i, oak);
                        // cloth being woven, with a shuttle
                        for (int y = 4; y < 12; y++)
                            for (int x = 3; x < 13; x++)
                                i[x, y] = ((x + (y >> 1)) & 1) == 0 ? C(0xE0D8C8) : C(0xC4BAA6);
                        i.HLine(4, 3, 12, C(0xF0EADC));
                        i.HLine(12, 3, 12, Img.Shade(oak.plankDark, 0.9f));
                        i.Rect(6, 9, 4, 1, C(0x7A5A34)); i.Set(6, 9, C(0xA0784A));
                        return i;
                    }
                // -------------------------------------------------- lectern
                // base: 16x2x16 slab (sides show rows 14-15); post: front/back use cols 4-11 rows 3-13;
                // reading board: the top tile across its full width.
                case "lectern_top":
                    {
                        WoodBody(i, oak);
                        i.RectOutline(0, 0, 16, 16, Img.Shade(oak.plankDark, 0.72f));
                        i.HLine(1, 1, 14, Img.Shade(oak.plank, 1.12f));
                        i.Rect(2, 3, 12, 10, Img.Shade(oak.plank, 0.95f));
                        i.RectOutline(2, 3, 12, 10, Img.Shade(oak.plankDark, 0.9f));
                        i.HLine(13, 1, 14, Img.Shade(oak.plankDark, 1.05f));   // book ledge
                        return i;
                    }
                case "lectern_sides":
                case "lectern_front":
                    {
                        WoodBody(i, oak);
                        // vertical post boards
                        for (int y = 0; y < 16; y++)
                            for (int x = 4; x < 12; x++)
                                i[x, y] = Cl(oak.plank, x == 4 ? 1.12f : (x == 11 ? 0.82f : ((x == 7) ? 0.92f : 1f)), 0.06f);
                        i.HLine(3, 4, 11, Img.Shade(oak.plank, 1.16f));
                        i.HLine(13, 4, 11, Img.Shade(oak.plankDark, 0.9f));
                        if (n == "lectern_front")
                        {
                            // carved book-spine panel
                            BevelRect(i, 5, 5, 6, 6, Img.Shade(oak.plank, 0.9f), true);
                            i.VLine(7, 6, 9, C(0x8A3A2A)); i.VLine(8, 6, 9, C(0x2E4A8A));
                        }
                        else i.VLine(7, 4, 12, Img.Shade(oak.plankDark, 0.95f));
                        return i;
                    }
                case "lectern_base":
                    WoodBody(i, oak, 0.94f);
                    i.RectOutline(0, 0, 16, 16, Img.Shade(oak.plankDark, 0.72f));
                    i.HLine(14, 0, 15, Img.Shade(oak.plank, 1.1f));
                    i.RectOutline(4, 4, 8, 8, Img.Shade(oak.plankDark, 0.9f));
                    return i;
                case "lectern_book":
                    {
                        // open book on the reading board: visible at cols 3-12, rows 3-12
                        FillTN(i, C(0x6A3A22), 0.05f);
                        for (int y = 4; y <= 11; y++)
                            for (int x = 4; x <= 11; x++)
                                i[x, y] = Cl(C(0xEAE0C8), x == 7 || x == 8 ? 0.86f : 1f, 0.03f);
                        for (int y = 5; y <= 10; y += 2) { i.HLine(y, 4, 6, C(0xA89C80)); i.HLine(y, 9, 11, C(0xA89C80)); }
                        i.RectOutline(3, 3, 10, 10, C(0x4A2616));
                        return i;
                    }
                // -------------------------------------------------- grindstone
                // wheel box: flat faces (W/E) show cols 2-13 rows 0-11 of the side tile; the tread top shows
                // cols 4-11 rows 2-13 of the round tile; pivots show cols 2-13 rows 3-10.
                case "grindstone_side":
                    {
                        for (int y = 0; y < 16; y++)
                            for (int x = 0; x < 16; x++)
                            {
                                float d = Mathf.Sqrt((x - 7.5f) * (x - 7.5f) + (y - 5.5f) * (y - 5.5f));
                                float f = d > 5.6f ? 0.8f : (((int)d & 1) == 0 ? 1.06f : 0.92f);
                                i[x, y] = Cl(C(0x8C8C8C), f, 0.07f);
                            }
                        i.Rect(7, 5, 2, 2, C(0x4A4A4E));
                        i.Set(7, 5, C(0x6A6A6E));
                        return i;
                    }
                case "grindstone_round":
                    {
                        for (int y = 0; y < 16; y++)
                            for (int x = 0; x < 16; x++)
                                i[x, y] = Cl(C(0x8C8C8C), (y % 3 == 0) ? 0.84f : ((x == 4 || x == 11) ? 0.9f : 1.04f), 0.08f);
                        return i;
                    }
                case "grindstone_pivot":
                    {
                        FillTN(i, darkOak.plank, 0.08f);
                        i.RectOutline(2, 3, 12, 8, Img.Shade(darkOak.plankDark, 0.8f));
                        i.Rect(6, 4, 4, 4, C(0x55575E));
                        i.Set(7, 5, C(0x9A9CA4)); i.Set(8, 6, C(0x33353A));
                        return i;
                    }
                // -------------------------------------------------- stonecutter
                // body 16x9x16: sides show rows 7-15. Saw quad samples cols 1-14, rows 9-15 of the saw tile.
                case "stonecutter_top":
                    {
                        Base(i, "smooth_stone");
                        i.RectOutline(0, 0, 16, 16, C(0x5E5E5E));
                        i.RectOutline(1, 1, 14, 14, C(0xB2B2B2));
                        i.Rect(1, 7, 14, 2, C(0x2E2E32));
                        i.HLine(6, 1, 14, C(0x7A7A7A));
                        return i;
                    }
                case "stonecutter_side":
                    {
                        Base(i, "smooth_stone");
                        i.HLine(7, 0, 15, C(0xB8B8B8));
                        i.HLine(8, 0, 15, C(0x8A8A8A));
                        i.Rect(2, 10, 12, 3, C(0x6E6E6E));
                        i.HLine(10, 2, 13, C(0x5A5A5A));
                        i.HLine(12, 2, 13, C(0x9A9A9A));
                        i.HLine(15, 0, 15, C(0x5E5E5E));
                        return i;
                    }
                case "stonecutter_bottom":
                    Base(i, "smooth_stone");
                    i.Multiply(0.85f);
                    return i;
                case "stonecutter_saw":
                    {
                        i.Clear();
                        // upper half of a spinning blade rising out of the slot (rows 9-15)
                        for (int y = 8; y < 16; y++)
                            for (int x = 0; x < 16; x++)
                            {
                                float d = Mathf.Sqrt((x - 7.5f) * (x - 7.5f) + (y - 16.5f) * (y - 16.5f));
                                if (d > 7.6f) continue;
                                bool tooth = d > 6.5f;
                                if (tooth && ((x + y) & 1) == 1) continue;
                                i[x, y] = tooth ? C(0x9A9AA2) : (d < 2.2f ? C(0x55555C) : Cl(C(0xC8C8D0), d > 5.5f ? 0.9f : 1.05f, 0.05f));
                            }
                        i.Set(7, 15, C(0x2E2E32)); i.Set(8, 15, C(0x2E2E32));
                        return i;
                    }
                // -------------------------------------------------- enchanting table
                // body 16x12x16: sides show rows 4-15 (cloth band at 4-7, obsidian below).
                case "enchanting_table_top":
                    {
                        Base(i, "obsidian");
                        var cloth = C(0xA8242E);
                        for (int y = 1; y < 15; y++)
                            for (int x = 1; x < 15; x++)
                            {
                                int d = Math.Abs(x * 2 - 15) + Math.Abs(y * 2 - 15);   // diamond distance
                                if (d > 24) continue;
                                Color32 c = TN(cloth, (d > 20) ? 0.8f : 1f, 0.06f);
                                if (d == 21 || d == 22) c = C(0xE8B830);              // gold trim
                                if (d < 7) c = C(0x2EC8C0);                            // glowing gem
                                if (d == 7 || d == 8) c = C(0x1A7A7A);
                                i[x, y] = c;
                            }
                        i.Set(7, 7, C(0xB8FFF4));
                        return i;
                    }
                case "enchanting_table_side":
                    {
                        Base(i, "obsidian");
                        var cloth = C(0xA8242E);
                        for (int x = 0; x < 16; x++)
                        {
                            i[x, 4] = Tone(cloth, 1.12f);
                            i[x, 5] = TN(cloth, 1f, 0.06f);
                            i[x, 6] = ((x & 3) == 1) ? C(0xE8B830) : TN(cloth, 0.9f, 0.06f);   // fringe with gold tassels
                            i[x, 7] = ((x & 3) == 1) ? C(0xB88A20) : Tone(C(0x1C1826), 1f);
                        }
                        // purple runes carved into the obsidian
                        for (int k = 0; k < 4; k++) { int x = 2 + k * 4; i.Set(x, 10, C(0x6A3AA8)); i.Set(x + 1, 11, C(0x8A5AC8)); i.Set(x, 12, C(0x4A2A80)); }
                        return i;
                    }
                case "enchanting_table_bottom":
                    Base(i, "obsidian");
                    i.Multiply(0.85f);
                    return i;
                // -------------------------------------------------- anvils
                case "anvil": return Anvil(i);
                case "anvil_top": return AnvilTop(i, 0);
                case "chipped_anvil_top": return AnvilTop(i, 1);
                case "damaged_anvil_top": return AnvilTop(i, 2);
                // -------------------------------------------------- beacon core (box 3..13: visible cols 3-12)
                case "beacon":
                    {
                        FillTN(i, C(0x7AE0DA), 0.1f);
                        i.RectOutline(2, 2, 12, 12, C(0x3AA6A0));
                        i.Rect(5, 5, 6, 6, C(0xC8FFF8));
                        i.RectOutline(5, 5, 6, 6, C(0x9AF4EC));
                        i.Rect(7, 7, 2, 2, C(0xFFFFFF));
                        i.RectOutline(0, 0, 16, 16, C(0x2A7A76));
                        return i;
                    }
                // -------------------------------------------------- bell (opaque gold; body cols 5-10 rows 3-9, lip rows 10-11)
                case "bell_body":
                    {
                        var gold = C(0xE2B02C);
                        for (int y = 0; y < 16; y++)
                            for (int x = 0; x < 16; x++)
                            {
                                float f = x <= 6 ? 1.12f : (x >= 9 ? 0.86f : 1f);
                                if (y == 3 || y == 4) f *= 1.08f;
                                if (y == 10) f *= 1.14f; else if (y == 11) f *= 0.8f;
                                i[x, y] = Cl(gold, f, 0.05f);
                            }
                        i.VLine(6, 4, 9, C(0xFBE38A));
                        i.HLine(10, 4, 11, C(0xF6D36A));
                        return i;
                    }
                // -------------------------------------------------- brewing stand
                // rod: cols 7-8 rows 0-13 (+ rows 0-1 for its cap); the bottle arm plane shows the right half.
                case "brewing_stand":
                    {
                        i.Clear();
                        var rod = C(0xD8B04A);
                        for (int y = 0; y < 14; y++) { i.Set(7, y, rod); i.Set(8, y, Img.Shade(rod, 0.72f)); }
                        i.Set(7, 0, C(0xF8E08A)); i.Set(8, 0, C(0xC89A30)); i.Set(7, 1, C(0xE8C860));
                        // arms and hanging bottles (right half is used in-world, the left mirrors it)
                        for (int side = 0; side < 2; side++)
                        {
                            int dir = side == 0 ? 1 : -1;
                            int ax0 = side == 0 ? 9 : 2, ax1 = side == 0 ? 13 : 6;
                            i.HLine(4, ax0, ax1, C(0x8A8A92));
                            int bx = side == 0 ? 11 : 3;
                            i.Rect(bx, 5, 2, 2, C(0xC8D8E0));            // neck
                            i.Rect(bx - 1, 7, 4, 5, C(0xB8D0E8));         // body
                            i.Rect(bx, 9, 2, 3, side == 0 ? C(0xC84A6A) : C(0x4A7AC8));   // potion
                            i.Set(bx - 1, 7, C(0xE8F4FF));
                            i.HLine(12, bx - 1, bx + 2, C(0x8AA0B8));
                            i.Set(bx + (dir > 0 ? 0 : 1), 4, C(0xB8B8C0));
                        }
                        return i;
                    }
                case "brewing_stand_base":
                    {
                        Base(i, "cobblestone");
                        i.HLine(0, 0, 15, C(0xA0A0A0));
                        return i;
                    }
            }
            return null;
        }

        /// <summary>Anvil body: forged dark iron, deliberately plain (the model carves out its shape).</summary>
        static Img Anvil(Img i)
        {
            var iron = C(0x48484E);
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float f = 1f;
                    if ((Hash.Get(x / 3, y, 17) & 7) == 0) f = 0.9f;
                    if ((y & 3) == 0) f *= 1.05f;
                    i[x, y] = Cl(iron, f, 0.06f);
                }
            for (int k = 0; k < 5; k++) i.Set(RndInt(16), RndInt(16), C(0x5E5E66));
            return i;
        }

        /// <summary>Anvil working face; damage 1 and 2 add chips and cracks.</summary>
        static Img AnvilTop(Img i, int damage)
        {
            var iron = C(0x55555C);
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float f = (x >= 4 && x <= 11) ? 1.08f : 0.96f;       // polished strip down the middle
                    i[x, y] = Cl(iron, f, 0.05f);
                }
            i.VLine(3, 0, 15, C(0x3E3E44)); i.VLine(12, 0, 15, C(0x3E3E44));
            i.VLine(4, 0, 15, C(0x6E6E76));
            i.HLine(0, 3, 12, C(0x6A6A72)); i.HLine(15, 3, 12, C(0x35353B));
            if (damage >= 1)
            {
                var crack = C(0x2A2A2E);
                i.Line(5, 2, 8, 6, crack); i.Line(8, 6, 7, 9, crack);
                i.Rect(10, 12, 2, 2, C(0x3A3A40)); i.Set(10, 12, crack);
            }
            if (damage >= 2)
            {
                var crack = C(0x222226);
                i.Line(9, 3, 11, 8, crack); i.Line(11, 8, 9, 13, crack); i.Line(4, 10, 7, 13, crack);
                i.Rect(4, 5, 2, 3, C(0x3A3A40)); i.Set(5, 6, crack);
                i.Set(6, 14, crack); i.Set(7, 14, crack);
            }
            return i;
        }
    }
}
