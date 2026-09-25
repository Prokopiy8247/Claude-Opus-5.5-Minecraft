using System;
using UnityEngine;

namespace MCR
{
    /// <summary>16x16 (or NxN) RGBA image with pixel-art painting helpers. Row 0 is the TOP row.</summary>
    public sealed class Img
    {
        public readonly int W, H;
        public readonly Color32[] px;
        public RNG rng;

        public Img(int w = 16, int h = 16, int seed = 0) { W = w; H = h; px = new Color32[w * h]; rng = new RNG(seed); }
        public static Img Seeded(string name, int frame = 0, int size = 16) => new Img(size, size, Hash.StringHash(name) * 31 + frame * 7919);

        public Color32 this[int x, int y]
        {
            get => px[(((y % H) + H) % H) * W + (((x % W) + W) % W)];
            set => px[(((y % H) + H) % H) * W + (((x % W) + W) % W)] = value;
        }
        public bool In(int x, int y) => x >= 0 && y >= 0 && x < W && y < H;
        public void Set(int x, int y, Color32 c) { if (In(x, y)) px[y * W + x] = c; }
        public Color32 Get(int x, int y) => In(x, y) ? px[y * W + x] : new Color32(0, 0, 0, 0);

        public Img Fill(Color32 c) { for (int i = 0; i < px.Length; i++) px[i] = c; return this; }
        public Img Clear() => Fill(new Color32(0, 0, 0, 0));
        public Img Copy() { var i = new Img(W, H); Array.Copy(px, i.px, px.Length); i.rng = rng; return i; }
        public void CopyFrom(Img o) { Array.Copy(o.px, px, Math.Min(px.Length, o.px.Length)); }

        public static Color32 C(uint rgb, byte a = 255) => MathX.Hex(rgb, a);
        public static Color32 C(int r, int g, int b, int a = 255) => new Color32((byte)Mathf.Clamp(r, 0, 255), (byte)Mathf.Clamp(g, 0, 255), (byte)Mathf.Clamp(b, 0, 255), (byte)Mathf.Clamp(a, 0, 255));
        public static Color32 Shade(Color32 c, float f) => new Color32((byte)Mathf.Clamp(c.r * f, 0, 255), (byte)Mathf.Clamp(c.g * f, 0, 255), (byte)Mathf.Clamp(c.b * f, 0, 255), c.a);
        public static Color32 Add(Color32 c, int d) => new Color32((byte)Mathf.Clamp(c.r + d, 0, 255), (byte)Mathf.Clamp(c.g + d, 0, 255), (byte)Mathf.Clamp(c.b + d, 0, 255), c.a);
        public static Color32 Mix(Color32 a, Color32 b, float t) => MathX.Lerp(a, b, t);
        public static Color32 WithA(Color32 c, byte a) { c.a = a; return c; }
        public static Color32 Gray(int v, int a = 255) => C(v, v, v, a);

        /// <summary>Per-pixel brightness noise around base colour.</summary>
        public Img Noise(Color32 baseC, float amount = 0.12f)
        {
            for (int i = 0; i < px.Length; i++)
            {
                float f = 1f + (rng.NextFloat() * 2f - 1f) * amount;
                px[i] = Shade(baseC, f);
            }
            return this;
        }
        /// <summary>Palette noise: picks from a palette with weights (clusters by smoothing).</summary>
        public Img PaletteNoise(Color32[] pal, float[] weights = null, int smooth = 0)
        {
            int n = pal.Length;
            var idx = new int[W * H];
            for (int i = 0; i < idx.Length; i++)
            {
                float r = rng.NextFloat();
                if (weights == null) idx[i] = rng.Next(n);
                else
                {
                    float acc = 0; idx[i] = n - 1;
                    for (int k = 0; k < n; k++) { acc += weights[k]; if (r < acc) { idx[i] = k; break; } }
                }
            }
            for (int s = 0; s < smooth; s++)
            {
                var ni = new int[idx.Length];
                for (int y = 0; y < H; y++)
                    for (int x = 0; x < W; x++)
                    {
                        // majority of 3x3 neighbourhood with random tie-break
                        var counts = new int[n];
                        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) counts[idx[((y + dy + H) % H) * W + (x + dx + W) % W]]++;
                        int best = idx[y * W + x], bc = -1;
                        for (int k = 0; k < n; k++) if (counts[k] > bc || (counts[k] == bc && rng.NextBool())) { bc = counts[k]; best = k; }
                        ni[y * W + x] = rng.Chance(0.2f) ? idx[y * W + x] : best;
                    }
                idx = ni;
            }
            for (int i = 0; i < idx.Length; i++) px[i] = pal[idx[i]];
            return this;
        }
        public Img Speckle(Color32 c, float density, int size = 1)
        {
            int count = Mathf.RoundToInt(W * H * density);
            for (int i = 0; i < count; i++)
            {
                int x = rng.Next(W), y = rng.Next(H);
                for (int dy = 0; dy < size; dy++) for (int dx = 0; dx < size; dx++) this[x + dx, y + dy] = c;
            }
            return this;
        }
        /// <summary>Random small clusters (2-4 px) of colour.</summary>
        public Img Clusters(Color32 c, int count, int minSize = 2, int maxSize = 4, Color32? edge = null)
        {
            for (int i = 0; i < count; i++)
            {
                int x = rng.Next(W), y = rng.Next(H);
                int sz = rng.Range(minSize, maxSize);
                for (int k = 0; k < sz; k++)
                {
                    this[x, y] = c;
                    if (edge.HasValue && rng.Chance(0.5f)) this[x + 1, y + 1] = edge.Value;
                    int d = rng.Next(4);
                    if (d == 0) x++; else if (d == 1) x--; else if (d == 2) y++; else y--;
                }
            }
            return this;
        }
        public Img Rect(int x0, int y0, int w, int h, Color32 c)
        {
            for (int y = y0; y < y0 + h; y++) for (int x = x0; x < x0 + w; x++) Set(x, y, c);
            return this;
        }
        public Img RectOutline(int x0, int y0, int w, int h, Color32 c)
        {
            for (int x = x0; x < x0 + w; x++) { Set(x, y0, c); Set(x, y0 + h - 1, c); }
            for (int y = y0; y < y0 + h; y++) { Set(x0, y, c); Set(x0 + w - 1, y, c); }
            return this;
        }
        public Img HLine(int y, int x0, int x1, Color32 c) { for (int x = x0; x <= x1; x++) Set(x, y, c); return this; }
        public Img VLine(int x, int y0, int y1, Color32 c) { for (int y = y0; y <= y1; y++) Set(x, y, c); return this; }
        public Img Line(int x0, int y0, int x1, int y1, Color32 c)
        {
            int dx = Math.Abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -Math.Abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
            while (true)
            {
                Set(x0, y0, c);
                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 >= dy) { err += dy; x0 += sx; }
                if (e2 <= dx) { err += dx; y0 += sy; }
            }
            return this;
        }
        /// <summary>Bevel: lighten top/left edge, darken bottom/right (polished look).</summary>
        public Img Bevel(float light = 1.18f, float dark = 0.78f, int x0 = 0, int y0 = 0, int w = -1, int h = -1)
        {
            if (w < 0) w = W; if (h < 0) h = H;
            for (int x = x0; x < x0 + w; x++) { Set(x, y0, Shade(Get(x, y0), light)); Set(x, y0 + h - 1, Shade(Get(x, y0 + h - 1), dark)); }
            for (int y = y0 + 1; y < y0 + h - 1; y++) { Set(x0, y, Shade(Get(x0, y), light)); Set(x0 + w - 1, y, Shade(Get(x0 + w - 1, y), dark)); }
            return this;
        }
        public Img Multiply(float f) { for (int i = 0; i < px.Length; i++) px[i] = Shade(px[i], f); return this; }
        public Img Tint(Color32 t)
        {
            for (int i = 0; i < px.Length; i++) { var c = px[i]; px[i] = new Color32((byte)(c.r * t.r / 255), (byte)(c.g * t.g / 255), (byte)(c.b * t.b / 255), c.a); }
            return this;
        }
        /// <summary>Map luminance of pixels to a ramp between dark and light colour.</summary>
        public Img Colorize(Color32 dark, Color32 light)
        {
            for (int i = 0; i < px.Length; i++)
            {
                var c = px[i];
                float l = (c.r * 0.3f + c.g * 0.59f + c.b * 0.11f) / 255f;
                var n = Mix(dark, light, l); n.a = c.a; px[i] = n;
            }
            return this;
        }
        public Img SetAlpha(byte a) { for (int i = 0; i < px.Length; i++) if (px[i].a > 0) px[i].a = a; return this; }
        public Img Overlay(Img o) { for (int i = 0; i < px.Length; i++) if (o.px[i].a > 0) px[i] = o.px[i]; return this; }
        public Img FlipH()
        {
            for (int y = 0; y < H; y++) for (int x = 0; x < W / 2; x++) { var t = px[y * W + x]; px[y * W + x] = px[y * W + W - 1 - x]; px[y * W + W - 1 - x] = t; }
            return this;
        }
        public Img Rotate90()
        {
            var n = new Color32[px.Length];
            for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) n[x * W + (W - 1 - y)] = px[y * W + x];
            Array.Copy(n, px, px.Length);
            return this;
        }
        public Img ShiftRows(int shiftEveryRow) { return this; }

        /// <summary>Paint an ASCII sprite. Each char maps through the palette; '.' or ' ' is skipped.</summary>
        public Img Sprite(string[] rows, System.Collections.Generic.Dictionary<char, Color32> pal, int ox = 0, int oy = 0)
        {
            for (int y = 0; y < rows.Length; y++)
            {
                string r = rows[y];
                for (int x = 0; x < r.Length; x++)
                {
                    char ch = r[x];
                    if (ch == '.' || ch == ' ') continue;
                    if (pal.TryGetValue(ch, out var c)) Set(ox + x, oy + y, c);
                }
            }
            return this;
        }

        // ----- structural patterns -----
        /// <summary>Brick pattern: rows of bricks with mortar lines. brickW x brickH including mortar.</summary>
        public Img Bricks(Color32 brick, Color32 mortar, int brickW = 8, int brickH = 4, float noise = 0.1f, bool offsetRows = true)
        {
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++)
                {
                    int row = y / brickH;
                    int off = offsetRows && (row % 2 == 1) ? brickW / 2 : 0;
                    bool m = (y % brickH == brickH - 1) || ((x + off) % brickW == brickW - 1);
                    if (m) px[y * W + x] = Shade(mortar, 1f + (rng.NextFloat() - 0.5f) * noise);
                    else
                    {
                        int bid = row * 31 + (x + off) / brickW;
                        float bf = 1f + ((Hash.Get(bid, row) & 255) / 255f - 0.5f) * 0.12f;
                        float f = bf * (1f + (rng.NextFloat() - 0.5f) * noise);
                        // highlight top-left of each brick
                        if (y % brickH == 0) f *= 1.1f;
                        if ((x + off) % brickW == 0) f *= 1.05f;
                        px[y * W + x] = Shade(brick, f);
                    }
                }
            return this;
        }
        /// <summary>Plank boards: horizontal boards of height 4 with dark separators and grain.</summary>
        public Img Planks(Color32 wood, Color32 dark)
        {
            for (int y = 0; y < H; y++)
            {
                int board = y / 4;
                int seam = (Hash.Get(board * 13 + 5, board) % 3) == 0 ? -1 : (int)(Hash.Get(board, 77) % 16);
                for (int x = 0; x < W; x++)
                {
                    Color32 c;
                    if (y % 4 == 3) c = dark;
                    else
                    {
                        float f = 1f + (rng.NextFloat() - 0.5f) * 0.08f;
                        // grain streaks
                        uint g = Hash.Get(board * 97 + y, x / 3);
                        if ((g & 7) == 0) f *= 0.9f;
                        if (y % 4 == 0) f *= 1.06f;
                        c = Shade(wood, f);
                        if (x == seam && y % 4 != 3) c = Shade(dark, 1.05f);
                    }
                    px[y * W + x] = c;
                }
            }
            return this;
        }
        /// <summary>Vertical bark streaks.</summary>
        public Img Bark(Color32 a, Color32 b, Color32? spot = null)
        {
            for (int x = 0; x < W; x++)
            {
                float colF = ((Hash.Get(x, 3) & 255) / 255f);
                for (int y = 0; y < H; y++)
                {
                    float n = ((Hash.Get(x * 7, y / 3 + (int)(Hash.Get(x, 9) % 5)) & 255) / 255f);
                    Color32 c = n > 0.55f ? a : b;
                    c = Shade(c, 0.92f + colF * 0.16f + (rng.NextFloat() - 0.5f) * 0.06f);
                    px[y * W + x] = c;
                }
            }
            if (spot.HasValue) Clusters(spot.Value, 4, 1, 3);
            return this;
        }
        /// <summary>Log top: concentric square-ish rings with bark border.</summary>
        public Img Rings(Color32 inner, Color32 ring, Color32 bark)
        {
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++)
                {
                    int d = Math.Max(Math.Abs(x * 2 - 15), Math.Abs(y * 2 - 15)) / 2; // 0..7
                    Color32 c;
                    if (d >= 7) c = bark;
                    else if (d % 2 == 1) c = ring;
                    else c = inner;
                    c = Shade(c, 1f + (rng.NextFloat() - 0.5f) * 0.08f);
                    px[y * W + x] = c;
                }
            return this;
        }
        /// <summary>Stained/clear glass frame.</summary>
        public Img GlassFrame(Color32 frame, Color32 fill, Color32 streak)
        {
            Fill(fill);
            RectOutline(0, 0, W, H, frame);
            for (int i = 0; i < 3; i++) Set(3 + i, 3 + i, streak);
            Set(4, 3, streak); Set(10, 9, streak); Set(11, 10, streak); Set(12, 11, streak); Set(11, 11, streak);
            return this;
        }
        /// <summary>Wool: fibrous loops texture.</summary>
        public Img Wool(Color32 c)
        {
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++)
                {
                    float f = 1f;
                    int k = (x + y * 3 + (int)(Hash.Get(x / 2, y / 2) % 4)) % 4;
                    if (k == 0) f = 1.07f; else if (k == 2) f = 0.93f;
                    f += (rng.NextFloat() - 0.5f) * 0.06f;
                    px[y * W + x] = Shade(c, f);
                }
            return this;
        }
        /// <summary>Tint-mask convention for opaque textures: alpha 128 = tinted, 255 = untinted.</summary>
        public Img MarkTinted(Func<int, int, bool> which = null)
        {
            for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (which == null || which(x, y)) px[y * W + x].a = 128;
            return this;
        }
    }
}
