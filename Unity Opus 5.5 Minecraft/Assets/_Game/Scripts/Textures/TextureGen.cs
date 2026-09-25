using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Procedural, original pixel-art block textures (16x16). Every texture name requested by the block registry
    /// resolves to a recipe here. Output rows are top-to-bottom.
    /// </summary>
    public static partial class TextureGen
    {
        static Color32 C(uint rgb, byte a = 255) => Img.C(rgb, a);
        static Color32 C(int r, int g, int b, int a = 255) => Img.C(r, g, b, a);

        public static int GetFrameCount(string name)
        {
            switch (name)
            {
                case "water_still": case "water_flow": case "lava_still": case "lava_flow":
                case "nether_portal": case "fire": case "soul_fire": case "end_portal": return 16;
                case "magma": return 3;
                case "sea_lantern": return 5;
                case "prismarine": return 4;
            }
            return 1;
        }

        public static Color32[] Generate(string name)
        {
            string baseName = name; int frame = 0;
            int hash = name.IndexOf('#');
            if (hash >= 0) { baseName = name.Substring(0, hash); int.TryParse(name.Substring(hash + 1), out frame); }
            Img img = null;
            try { img = Make(baseName, frame); }
            catch (Exception e) { Debug.LogWarning("Texture gen failed for " + name + ": " + e.Message); }
            if (img == null) { img = Missing(); if (baseName != "missing") MissingNames.Add(baseName); }
            return img.px;
        }
        public static readonly HashSet<string> MissingNames = new HashSet<string>();

        static Img Missing()
        {
            var i = new Img();
            for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) i[x, y] = ((x / 8 + y / 8) % 2 == 0) ? C(0xF800F8) : C(0x000000);
            return i;
        }

        // ------------------------------------------------------------------ palettes
        public struct WoodPal { public Color32 plank, plankDark, bark, barkDark, inner, ring, stripped, strippedDark; }
        public static WoodPal Wood(string w)
        {
            switch (w)
            {
                case "spruce": return new WoodPal { plank = C(0x73573A), plankDark = C(0x503B25), bark = C(0x4A3219), barkDark = C(0x33220F), inner = C(0x7B5E3D), ring = C(0x5E4428), stripped = C(0x735335), strippedDark = C(0x5E4428) };
                case "birch": return new WoodPal { plank = C(0xC4B27A), plankDark = C(0x9C8A55), bark = C(0xDCDBD4), barkDark = C(0x2E2B26), inner = C(0xD2C08A), ring = C(0xB8A368), stripped = C(0xC9B57D), strippedDark = C(0xA89460) };
                case "jungle": return new WoodPal { plank = C(0xA07350), plankDark = C(0x74513A), bark = C(0x57451A), barkDark = C(0x3F3011), inner = C(0xAA7C55), ring = C(0x8A5E3D), stripped = C(0xAB8454), strippedDark = C(0x8A6641) };
                case "acacia": return new WoodPal { plank = C(0xA85A32), plankDark = C(0x7C4123), bark = C(0x676056), barkDark = C(0x4C463E), inner = C(0xB06238), ring = C(0x8C4A28), stripped = C(0xAE5D3B), strippedDark = C(0x8C4A28) };
                case "dark_oak": return new WoodPal { plank = C(0x422B14), plankDark = C(0x2D1C0C), bark = C(0x3C2E1A), barkDark = C(0x281E10), inner = C(0x503619), ring = C(0x3A2610), stripped = C(0x4A3521), strippedDark = C(0x36261A) };
                case "mangrove": return new WoodPal { plank = C(0x753630), plankDark = C(0x552520), bark = C(0x544228), barkDark = C(0x3D2F1C), inner = C(0x7A3B34), ring = C(0x5E2B26), stripped = C(0x7A3730), strippedDark = C(0x5E2B26) };
                case "cherry": return new WoodPal { plank = C(0xE2B2AC), plankDark = C(0xC48E88), bark = C(0x36212C), barkDark = C(0x24151D), inner = C(0xDFA3A1), ring = C(0xC78484), stripped = C(0xD99E98), strippedDark = C(0xB77C79) };
                case "pale_oak": return new WoodPal { plank = C(0xE4DAD6), plankDark = C(0xC4B8B2), bark = C(0x8C827E), barkDark = C(0x6A615E), inner = C(0xE6DCD6), ring = C(0xCFC3BD), stripped = C(0xE8DFDB), strippedDark = C(0xCABFBA) };
                case "bamboo": return new WoodPal { plank = C(0xC1AD50), plankDark = C(0x9C8A34), bark = C(0x7F8E3B), barkDark = C(0x5E6B27), inner = C(0xC9B55E), ring = C(0xA6933C), stripped = C(0xC8B45A), strippedDark = C(0xA6933C) };
                case "crimson": return new WoodPal { plank = C(0x653046), plankDark = C(0x4A2032), bark = C(0x5D1A1E), barkDark = C(0x8C1E28), inner = C(0x7A3450), ring = C(0x5C2236), stripped = C(0x8A3A55), strippedDark = C(0x6B2A42) };
                case "warped": return new WoodPal { plank = C(0x2B6863), plankDark = C(0x1C4A46), bark = C(0x3A3A4D), barkDark = C(0x16968C), inner = C(0x39706A), ring = C(0x245652), stripped = C(0x3A8E86), strippedDark = C(0x2B6F69) };
                default: return new WoodPal { plank = C(0xA2824E), plankDark = C(0x7C633B), bark = C(0x6D5532), barkDark = C(0x544025), inner = C(0xB6915A), ring = C(0x957646), stripped = C(0xB29157), strippedDark = C(0x967645) };
            }
        }

        public static readonly Dictionary<string, Color32> DyeColors = new Dictionary<string, Color32>
        {
            {"white", C(0xE9ECEC)}, {"orange", C(0xF07613)}, {"magenta", C(0xBD44B3)}, {"light_blue", C(0x3AAFD9)},
            {"yellow", C(0xF8C627)}, {"lime", C(0x70B919)}, {"pink", C(0xED8DAC)}, {"gray", C(0x3E4447)},
            {"light_gray", C(0x8E8E86)}, {"cyan", C(0x158991)}, {"purple", C(0x792AAC)}, {"blue", C(0x35399D)},
            {"brown", C(0x724728)}, {"green", C(0x546D1B)}, {"red", C(0xA12722)}, {"black", C(0x141519)},
        };
        public static readonly Dictionary<string, Color32> ConcreteColors = new Dictionary<string, Color32>
        {
            {"white", C(0xCFD5D6)}, {"orange", C(0xE06100)}, {"magenta", C(0xA9309F)}, {"light_blue", C(0x2389C6)},
            {"yellow", C(0xF0AF15)}, {"lime", C(0x5EA818)}, {"pink", C(0xD5658E)}, {"gray", C(0x36393D)},
            {"light_gray", C(0x7D7D73)}, {"cyan", C(0x157788)}, {"purple", C(0x64209C)}, {"blue", C(0x2C2E8F)},
            {"brown", C(0x603C1F)}, {"green", C(0x495B24)}, {"red", C(0x8E2020)}, {"black", C(0x080A0F)},
        };
        public static readonly Dictionary<string, Color32> TerracottaColors = new Dictionary<string, Color32>
        {
            {"white", C(0xD1B2A1)}, {"orange", C(0xA15325)}, {"magenta", C(0x95576C)}, {"light_blue", C(0x716C89)},
            {"yellow", C(0xBA8523)}, {"lime", C(0x677534)}, {"pink", C(0xA14E4E)}, {"gray", C(0x392A23)},
            {"light_gray", C(0x876A61)}, {"cyan", C(0x565B5B)}, {"purple", C(0x764656)}, {"blue", C(0x4A3B5B)},
            {"brown", C(0x4D3323)}, {"green", C(0x4C532A)}, {"red", C(0x8F3D2E)}, {"black", C(0x251610)},
        };

        static bool TryColor(string name, string suffix, out string color)
        {
            color = null;
            if (!name.EndsWith(suffix)) return false;
            string c = name.Substring(0, name.Length - suffix.Length);
            if (DyeColors.ContainsKey(c)) { color = c; return true; }
            return false;
        }

        // ------------------------------------------------------------------ main resolver
        static Img Make(string n, int frame)
        {
            var img = Img.Seeded(n, 0);
            // --- colour families
            string col;
            if (TryColor(n, "_wool", out col)) return img.Wool(DyeColors[col]);
            if (TryColor(n, "_concrete", out col)) return img.Noise(ConcreteColors[col], 0.025f);
            if (TryColor(n, "_concrete_powder", out col)) { img.Noise(Img.Shade(ConcreteColors[col], 1.08f), 0.1f); return img.Speckle(Img.Shade(ConcreteColors[col], 0.85f), 0.15f).Speckle(Img.Shade(ConcreteColors[col], 1.2f), 0.1f); }
            if (TryColor(n, "_terracotta", out col)) return Terracotta(img, TerracottaColors[col]);
            if (TryColor(n, "_glazed_terracotta", out col)) return Glazed(img, col);
            if (TryColor(n, "_stained_glass", out col)) { var dc = DyeColors[col]; return img.GlassFrame(Img.WithA(Img.Shade(dc, 0.8f), 230), Img.WithA(dc, 120), Img.WithA(Img.Shade(dc, 1.3f), 170)); }
            if (TryColor(n, "_stained_glass_pane_top", out col)) { var dc = DyeColors[col]; img.Fill(Img.WithA(Img.Shade(dc, 0.8f), 230)); return img; }
            // --- wood families
            foreach (var w in Blocks.WoodTypes)
            {
                var wp = Wood(w);
                string log = w == "crimson" || w == "warped" ? w + "_stem" : (w == "bamboo" ? "bamboo_block" : w + "_log");
                if (n == w + "_planks") return w == "bamboo" ? BambooPlanks(img, wp) : img.Planks(wp.plank, wp.plankDark);
                if (n == log) return LogSide(img, w, wp);
                if (n == log + "_top") return LogTop(img, w, wp);
                if (n == "stripped_" + log) return StrippedSide(img, wp);
                if (n == "stripped_" + log + "_top") return img.Rings(wp.stripped, wp.strippedDark, wp.strippedDark);
                if (n == w + "_door_top" || n == w + "_door_bottom") return Door(img, w, wp, n.EndsWith("_top"));
                if (n == w + "_trapdoor") return Trapdoor(img, w, wp);
                if (n == w + "_leaves") return Leaves(img, w);
                if (n == w + "_sapling" || (w == "mangrove" && n == "mangrove_propagule")) return Sapling(img, w, wp);
            }
            if (n == "bamboo_mosaic") { img.Fill(Wood("bamboo").plank); for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) { if (y % 8 == 7 || x % 8 == 0) img[x, y] = Wood("bamboo").plankDark; else if (((x / 8) + (y / 8)) % 2 == 0 && x % 2 == 1) img[x, y] = Img.Shade(Wood("bamboo").plank, 0.92f); } return img; }
            // --- copper families
            if (n.Contains("copper") && !n.Contains("ore") && !n.StartsWith("raw_") && n != "copper_torch")
            {
                var r = Copper(img, n);
                if (r != null) return r;
            }
            // --- ores
            if (n.EndsWith("_ore")) return Ore(img, n);
            return Specific(img, n, frame);
        }

        // ------------------------------------------------------------------ stone-like
        public static Img Stone(Img i, Color32 c, float amt = 0.1f)
        {
            i.Noise(c, amt * 0.5f);
            i.Clusters(Img.Shade(c, 0.88f), 10, 2, 4);
            i.Clusters(Img.Shade(c, 1.1f), 8, 2, 3);
            return i;
        }
        static Img Cobble(Img i, Color32 stone, Color32 mortar, Color32? moss = null)
        {
            // voronoi-ish rounded stones
            var cells = new Vector2[10];
            for (int k = 0; k < cells.Length; k++) cells[k] = new Vector2(i.rng.NextFloat() * 16, i.rng.NextFloat() * 16);
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float best = 999, second = 999; int bi = 0;
                    for (int k = 0; k < cells.Length; k++)
                        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++)
                            {
                                float dx = x + 0.5f - (cells[k].x + ox * 16), dy = y + 0.5f - (cells[k].y + oy * 16);
                                float d = dx * dx + dy * dy;
                                if (d < best) { second = best; best = d; bi = k; } else if (d < second) second = d;
                            }
                    float edge = Mathf.Sqrt(second) - Mathf.Sqrt(best);
                    Color32 c;
                    if (edge < 0.9f) c = mortar;
                    else
                    {
                        float f = 0.85f + ((bi * 37) % 7) / 7f * 0.3f;
                        if (edge < 1.8f) f *= 0.9f;
                        f += (i.rng.NextFloat() - 0.5f) * 0.08f;
                        c = Img.Shade(stone, f);
                    }
                    i[x, y] = c;
                }
            if (moss.HasValue) i.Clusters(moss.Value, 9, 2, 5, Img.Shade(moss.Value, 0.8f));
            return i;
        }
        static Img Polished(Img i, Color32 c)
        {
            i.Noise(c, 0.05f);
            i.Clusters(Img.Shade(c, 0.93f), 4, 2, 3);
            i.RectOutline(0, 0, 16, 16, Img.Shade(c, 0.8f));
            i.HLine(1, 1, 14, Img.Shade(c, 1.12f)); i.VLine(1, 1, 14, Img.Shade(c, 1.12f));
            return i;
        }
        static Img StoneBricks(Img i, Color32 c, Color32 mortar)
        {
            i.Bricks(c, mortar, 8, 4, 0.12f, true);
            return i;
        }
        static Img Terracotta(Img i, Color32 c)
        {
            i.Noise(c, 0.04f);
            i.Clusters(Img.Shade(c, 0.94f), 6, 2, 4);
            return i;
        }
        static Img Glazed(Img i, string col)
        {
            var c = DyeColors[col];
            var light = Img.Shade(c, 1.35f); var dark = Img.Shade(c, 0.55f); var mid = c;
            var accent = col == "white" ? C(0x2E7FC2) : (col == "black" ? C(0xB02E26) : (col == "yellow" ? C(0x4A8AD8) : Img.Mix(c, C(0xFFFFFF), 0.6f)));
            // rotationally-asymmetric motif made of quarter patterns
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    int qx = x % 8, qy = y % 8;
                    bool border = qx == 0 || qy == 0;
                    bool diag = qx == qy || qx + qy == 7;
                    bool corner = qx < 3 && qy < 3;
                    Color32 cc = mid;
                    if (border) cc = dark;
                    else if (corner) cc = accent;
                    else if (diag) cc = light;
                    else if ((qx + qy) % 3 == 0) cc = Img.Shade(mid, 0.85f);
                    if (x >= 8 && y < 8 && diag) cc = accent;
                    i[x, y] = cc;
                }
            return i;
        }

        // ------------------------------------------------------------------ wood helpers
        static Img LogSide(Img i, string w, WoodPal p)
        {
            if (w == "birch")
            {
                i.Noise(p.bark, 0.04f);
                for (int k = 0; k < 7; k++) { int x = i.rng.Next(16), y = i.rng.Next(16); int len = i.rng.Range(2, 5); i.HLine(y, x, x + len, p.barkDark); if (i.rng.Chance(0.5f)) i.Set(x + 1, y + 1, Img.Shade(p.barkDark, 1.5f)); }
                return i;
            }
            if (w == "bamboo")
            {
                i.Fill(p.bark);
                for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) { float f = (x % 4 == 0) ? 0.85f : ((x % 4 == 2) ? 1.08f : 1f); i[x, y] = Img.Shade(p.bark, f + (i.rng.NextFloat() - 0.5f) * 0.05f); }
                i.HLine(5, 0, 15, p.barkDark); i.HLine(12, 0, 15, p.barkDark);
                return i;
            }
            if (w == "crimson" || w == "warped")
            {
                i.Bark(p.bark, Img.Shade(p.bark, 0.8f));
                for (int k = 0; k < 5; k++) { int x = i.rng.Next(16); int y0 = i.rng.Next(16); int len = i.rng.Range(3, 7); i.VLine(x, y0, y0 + len, p.barkDark); }
                return i;
            }
            if (w == "cherry") { i.Bark(p.bark, p.barkDark); for (int y = 2; y < 16; y += 5) i.HLine(y, 0, 15, Img.Shade(p.bark, 1.25f)); return i; }
            if (w == "pale_oak") { i.Bark(p.bark, p.barkDark); i.Clusters(Img.Shade(p.bark, 1.15f), 5, 1, 3); return i; }
            if (w == "acacia") { i.Bark(p.bark, p.barkDark); i.Clusters(Img.Shade(p.bark, 1.15f), 4, 2, 3); return i; }
            return i.Bark(p.bark, p.barkDark);
        }
        static Img LogTop(Img i, string w, WoodPal p)
        {
            if (w == "bamboo")
            {
                i.Fill(p.bark); i.RectOutline(0, 0, 16, 16, p.barkDark);
                for (int y = 2; y < 14; y++) for (int x = 2; x < 14; x++) i[x, y] = Img.Shade(p.inner, 1f + (i.rng.NextFloat() - 0.5f) * 0.06f);
                for (int k = 0; k < 4; k++) { int cx = 4 + (k % 2) * 7, cy = 4 + (k / 2) * 7; i.Rect(cx, cy, 2, 2, p.barkDark); }
                return i;
            }
            if (w == "crimson" || w == "warped")
            {
                i.Rings(p.inner, p.ring, p.bark);
                i.RectOutline(1, 1, 14, 14, p.barkDark);
                return i;
            }
            return i.Rings(p.inner, p.ring, p.bark);
        }
        static Img StrippedSide(Img i, WoodPal p)
        {
            for (int x = 0; x < 16; x++)
            {
                float cf = 0.94f + (Hash.Get(x, 5) & 15) / 15f * 0.12f;
                for (int y = 0; y < 16; y++) i[x, y] = Img.Shade(p.stripped, cf + (i.rng.NextFloat() - 0.5f) * 0.05f);
            }
            for (int k = 0; k < 4; k++) { int x = i.rng.Next(16); i.VLine(x, i.rng.Next(8), 8 + i.rng.Next(8), p.strippedDark); }
            return i;
        }
        static Img BambooPlanks(Img i, WoodPal p)
        {
            for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++)
                {
                    float f = x % 4 == 3 ? 0.78f : (x % 4 == 0 ? 1.08f : 1f);
                    i[x, y] = Img.Shade(p.plank, f + (i.rng.NextFloat() - 0.5f) * 0.05f);
                }
            i.HLine(7, 0, 15, p.plankDark);
            return i;
        }
        static Img Door(Img i, string w, WoodPal p, bool top)
        {
            i.Planks(p.plank, p.plankDark);
            // vertical boards for doors
            for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++)
                {
                    float f = (x % 5 == 4) ? 0.8f : 1f;
                    i[x, y] = Img.Shade(p.plank, f + (i.rng.NextFloat() - 0.5f) * 0.07f);
                }
            i.RectOutline(0, 0, 16, 16, p.plankDark);
            if (top)
            {
                bool windows = w == "oak" || w == "birch" || w == "jungle" || w == "acacia" || w == "cherry" || w == "pale_oak" || w == "crimson" || w == "warped" || w == "mangrove";
                if (windows)
                {
                    var glass = new Color32(0, 0, 0, 0);
                    i.Rect(3, 3, 4, 5, glass); i.Rect(9, 3, 4, 5, glass);
                    i.RectOutline(2, 2, 6, 7, p.plankDark); i.RectOutline(8, 2, 6, 7, p.plankDark);
                }
                else { i.RectOutline(3, 2, 10, 12, p.plankDark); }
            }
            else
            {
                i.RectOutline(3, 2, 10, 10, p.plankDark);
                i.Set(12, 1, C(0x3A3A3A)); i.Set(12, 0, C(0x3A3A3A)); i.Set(13, 1, C(0x3A3A3A));
            }
            return i;
        }
        static Img Trapdoor(Img i, string w, WoodPal p)
        {
            i.Planks(p.plank, p.plankDark);
            i.RectOutline(0, 0, 16, 16, p.plankDark);
            i.RectOutline(1, 1, 14, 14, Img.Shade(p.plank, 1.1f));
            var hole = new Color32(0, 0, 0, 0);
            for (int y = 3; y < 13; y += 5) for (int x = 3; x < 13; x += 5) i.Rect(x, y, 3, 3, hole);
            return i;
        }
        static Img Leaves(Img i, string w)
        {
            Color32 baseC; bool tinted = true;
            switch (w)
            {
                case "cherry": baseC = C(0xE9A7C4); tinted = false; break;
                case "pale_oak": baseC = C(0xA3A99A); tinted = false; break;
                default: baseC = C(0xC8C8C8); break;
            }
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float r = i.rng.NextFloat();
                    uint h = Hash.Get(x / 2 + 3, y / 2 + (x % 2));
                    float cl = (h & 255) / 255f;
                    if (r < 0.16f) { i[x, y] = new Color32(0, 0, 0, 0); continue; }
                    float f = 0.62f + cl * 0.45f;
                    if ((x + y) % 4 == 0) f *= 0.85f;
                    i[x, y] = Img.Shade(baseC, f);
                }
            if (w == "cherry") { i.Clusters(C(0xF7C8DC), 6, 1, 3); i.Clusters(C(0xC7759A), 5, 1, 2); }
            if (w == "spruce" || w == "dark_oak") i.Multiply(0.92f);
            if (w == "azalea") i.Colorize(C(0x3E5716), C(0x7FA13A));
            return i;
        }
        static Img Sapling(Img i, string w, WoodPal p)
        {
            i.Clear();
            Color32 leaf = w == "cherry" ? C(0xE7A2C1) : w == "pale_oak" ? C(0x9AA092) : w == "spruce" ? C(0x3E6A39) : w == "birch" ? C(0x6A9A39) : w == "jungle" ? C(0x3D8A1C) : w == "acacia" ? C(0x6A8A22) : w == "dark_oak" ? C(0x2E6A16) : w == "mangrove" ? C(0x7AA23A) : C(0x4E8C25);
            var dk = Img.Shade(leaf, 0.7f);
            var stem = p.bark;
            if (w == "mangrove")
            {
                string[] prop = {
                    "................",
                    ".......gg.......",
                    "......gGGg......",
                    ".....gGGGGg.....",
                    "......gGGg......",
                    ".......ss.......",
                    ".......ss.......",
                    ".......ss.......",
                    ".......gg.......",
                    ".......gg.......",
                    ".......gg.......",
                    ".......gg.......",
                    "........g.......",
                    "........g.......",
                    "................",
                    "................" };
                return i.Sprite(prop, new Dictionary<char, Color32> { { 'g', dk }, { 'G', leaf }, { 's', stem } });
            }
            string[] spr = {
                "................",
                "......gGg.......",
                "....gGGGGGg.....",
                "...gGGgGGGGg....",
                "..gGGGGGgGGGg...",
                "...gGGGsGGGg....",
                "....gGgsgGg.....",
                ".....g.s.g......",
                ".......s........",
                "......gs........",
                ".......sg.......",
                ".......s........",
                ".......s........",
                ".......s........",
                ".......s........",
                "................" };
            if (w == "spruce")
                spr = new[] {
                "................",
                ".......g........",
                "......gGg.......",
                ".....gGGGg......",
                "......gGg.......",
                ".....gGsGg......",
                "....gGGsGGg.....",
                ".....gGsGg......",
                "....gGGsGGg.....",
                "...gGGGsGGGg....",
                ".......s........",
                ".......s........",
                ".......s........",
                ".......s........",
                ".......s........",
                "................" };
            return i.Sprite(spr, new Dictionary<char, Color32> { { 'g', dk }, { 'G', leaf }, { 's', stem } });
        }

        // ------------------------------------------------------------------ ores & minerals
        static Img Ore(Img i, string n)
        {
            bool deep = n.StartsWith("deepslate_");
            string kind = n.Replace("deepslate_", "").Replace("_ore", "");
            Img b;
            if (kind == "nether_gold" || kind == "nether_quartz") b = Specific(i, "netherrack", 0);
            else if (deep) b = DeepslateBase(i);
            else b = Stone(i, C(0x7D7D7D));
            Color32 hi, mid, lo;
            int count = 6;
            switch (kind)
            {
                case "coal": hi = C(0x505050); mid = C(0x2A2A2A); lo = C(0x151515); break;
                case "iron": hi = C(0xE8C8AE); mid = C(0xD8AF93); lo = C(0xA07A60); break;
                case "copper": hi = C(0xE8906A); mid = C(0xB6613E); lo = C(0x5E9E7E); count = 7; break;
                case "gold": hi = C(0xFFF69A); mid = C(0xFCE34B); lo = C(0xC49A20); break;
                case "lapis": hi = C(0x3B6FE0); mid = C(0x1E48B8); lo = C(0x0F2B78); count = 7; break;
                case "diamond": hi = C(0xB4FFF6); mid = C(0x5DECDF); lo = C(0x1A9C92); break;
                case "emerald": hi = C(0x7BFF9E); mid = C(0x17DD62); lo = C(0x098A36); count = 4; break;
                case "redstone": hi = C(0xFF4A3A); mid = C(0xE00000); lo = C(0x8A0000); count = 7; break;
                case "nether_gold": hi = C(0xFFF69A); mid = C(0xFCD34B); lo = C(0xC47A20); count = 8; break;
                case "nether_quartz": hi = C(0xFFFFFF); mid = C(0xE8E0D6); lo = C(0xB8A898); count = 7; break;
                default: hi = C(0xFFFFFF); mid = C(0xAAAAAA); lo = C(0x555555); break;
            }
            // place ore nuggets (2x2-ish blobs with highlight)
            for (int k = 0; k < count; k++)
            {
                int x = b.rng.Range(1, 13), y = b.rng.Range(1, 13);
                int sz = b.rng.Range(2, 3);
                for (int dy = 0; dy < sz; dy++) for (int dx = 0; dx < sz; dx++)
                    {
                        if ((dx == sz - 1 && dy == sz - 1) && b.rng.Chance(0.5f)) continue;
                        b[x + dx, y + dy] = (dx == 0 && dy == 0) ? hi : mid;
                    }
                b[x + sz, y + sz - 1] = lo;
                if (b.rng.Chance(0.5f)) b[x - 1, y + 1] = lo;
            }
            return b;
        }
        static Img DeepslateBase(Img i)
        {
            var c = C(0x4F4F54);
            for (int y = 0; y < 16; y++)
            {
                float rowF = 0.92f + (Hash.Get(y, 91) & 15) / 15f * 0.16f;
                for (int x = 0; x < 16; x++)
                {
                    float f = rowF + (i.rng.NextFloat() - 0.5f) * 0.1f;
                    if ((Hash.Get(x / 3, y) & 7) == 0) f *= 0.85f;
                    i[x, y] = Img.Shade(c, f);
                }
            }
            return i;
        }
        static Img Mineral(Img i, Color32 c, int style)
        {
            i.Noise(c, 0.04f);
            if (style == 0)
            {
                // panel with inner square
                i.RectOutline(0, 0, 16, 16, Img.Shade(c, 0.75f));
                i.HLine(1, 1, 14, Img.Shade(c, 1.2f)); i.VLine(1, 1, 14, Img.Shade(c, 1.2f));
                i.RectOutline(4, 4, 8, 8, Img.Shade(c, 0.88f));
            }
            else if (style == 1)
            {
                // gem block with diagonal facets
                i.RectOutline(0, 0, 16, 16, Img.Shade(c, 0.7f));
                for (int k = 2; k < 14; k++) { i.Set(k, 16 - k, Img.Shade(c, 1.25f)); i.Set(k, k, Img.Shade(c, 0.85f)); }
                i.HLine(1, 1, 14, Img.Shade(c, 1.2f));
            }
            else
            {
                i.Clusters(Img.Shade(c, 0.8f), 8, 2, 4); i.Clusters(Img.Shade(c, 1.15f), 8, 2, 3);
            }
            return i;
        }

        static Img Copper(Img i, string n)
        {
            int stage = 0; string rest = n;
            if (n.StartsWith("exposed_")) { stage = 1; rest = n.Substring(8); }
            else if (n.StartsWith("weathered_")) { stage = 2; rest = n.Substring(10); }
            else if (n.StartsWith("oxidized_")) { stage = 3; rest = n.Substring(9); }
            Color32[] bases = { C(0xC06C4F), C(0xA1806A), C(0x6D9A7B), C(0x52A286) };
            Color32 b = bases[stage];
            var dk = Img.Shade(b, 0.78f); var lt = Img.Shade(b, 1.18f);
            Color32 patina = C(0x55B59A);
            Img Weather(Img im)
            {
                if (stage >= 1) im.Clusters(stage == 1 ? C(0x8FA38A) : patina, stage * 5, 2, 4);
                return im;
            }
            switch (rest)
            {
                case "copper_block": case "copper":
                    i.Noise(b, 0.05f); i.RectOutline(0, 0, 16, 16, dk); i.HLine(1, 1, 14, lt); i.VLine(1, 1, 14, lt);
                    i.Clusters(Img.Shade(b, 0.9f), 5, 2, 3); return Weather(i);
                case "cut_copper":
                    i.Noise(b, 0.04f);
                    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) if (x % 8 == 7 || y % 8 == 7) i[x, y] = dk; else if (x % 8 == 0 || y % 8 == 0) i[x, y] = lt;
                    return Weather(i);
                case "chiseled_copper":
                    i.Noise(b, 0.04f); i.RectOutline(0, 0, 16, 16, dk); i.RectOutline(3, 3, 10, 10, dk); i.RectOutline(6, 6, 4, 4, lt);
                    return Weather(i);
                case "copper_grate":
                    i.Clear();
                    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) if (x % 4 == 0 || y % 4 == 0 || x == 15 || y == 15) i[x, y] = (x + y) % 3 == 0 ? dk : b;
                    return Weather(i);
                case "copper_bulb": case "copper_bulb_lit":
                    {
                        bool lit = rest.EndsWith("lit");
                        i.Noise(b, 0.04f); i.RectOutline(0, 0, 16, 16, dk); i.RectOutline(1, 1, 14, 14, lt);
                        var glow = lit ? C(0xFFE08A) : C(0x5A4A3A);
                        i.Rect(4, 4, 8, 8, glow); i.RectOutline(4, 4, 8, 8, dk);
                        if (lit) { i.Rect(6, 6, 4, 4, C(0xFFF8D8)); }
                        return Weather(i);
                    }
                case "copper_door_top": case "copper_door_bottom":
                    i.Noise(b, 0.05f); i.RectOutline(0, 0, 16, 16, dk); i.RectOutline(2, 2, 12, 12, lt);
                    if (rest.EndsWith("top")) { i.Rect(4, 4, 3, 4, new Color32(0, 0, 0, 0)); i.Rect(9, 4, 3, 4, new Color32(0, 0, 0, 0)); }
                    else i.Set(12, 1, dk);
                    return Weather(i);
                case "copper_trapdoor":
                    i.Noise(b, 0.05f); i.RectOutline(0, 0, 16, 16, dk);
                    for (int y = 3; y < 13; y += 5) for (int x = 3; x < 13; x += 5) i.Rect(x, y, 3, 3, new Color32(0, 0, 0, 0));
                    return Weather(i);
                case "copper_bars":
                    i.Clear(); for (int x = 1; x < 16; x += 4) i.VLine(x, 0, 15, b); i.HLine(1, 0, 15, dk); i.HLine(14, 0, 15, dk);
                    return i;
                case "copper_chain":
                    return Chain(i, b, dk);
                case "copper_lantern":
                    return Lantern(i, b, dk, C(0x9CF0C8));
            }
            return null;
        }

        static Img Chain(Img i, Color32 c, Color32 dk)
        {
            i.Clear();
            for (int y = 0; y < 16; y++)
            {
                bool link = (y / 4) % 2 == 0;
                if (link) { i.Set(1, y, c); i.Set(2, y, dk); if (y % 4 == 0 || y % 4 == 3) { i.Set(0, y, dk); } }
                else { i.Set(4, y, c); i.Set(3, y, dk); if (y % 4 == 0 || y % 4 == 3) i.Set(5, y, dk); }
            }
            return i;
        }
        static Img Lantern(Img i, Color32 metal, Color32 dk, Color32 glow)
        {
            i.Clear();
            // body front (0..6 x 7..14 in image coords -> v 2..9) ; top cap at (0..4, 1..5)
            i.Rect(0, 2, 6, 7, dk);
            i.Rect(1, 3, 4, 5, glow); i.Rect(2, 4, 2, 3, Img.Shade(glow, 1.2f));
            i.HLine(2, 0, 5, metal); i.HLine(8, 0, 5, metal);
            i.Rect(0, 9, 4, 2, metal); // cap
            i.Rect(0, 11, 4, 4, metal); i.Rect(1, 12, 2, 2, dk);
            i.Rect(11, 4, 3, 2, dk); // handle
            return i;
        }
    }
}
