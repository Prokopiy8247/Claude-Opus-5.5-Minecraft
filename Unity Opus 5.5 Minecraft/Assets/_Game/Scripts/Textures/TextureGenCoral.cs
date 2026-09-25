using System;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Coral textures: five species, each with a shrub, a solid block and a fan, plus the dead variants.
    /// </summary>
    public static partial class TextureGen
    {
        struct CoralPal { public Color32 tip, mid, dark, nub; }

        static CoralPal Coral(string kind, bool dead)
        {
            CoralPal p;
            switch (kind)
            {
                case "tube": p = new CoralPal { tip = C(0x3A5AE0), mid = C(0x2A3FB0), dark = C(0x18287A), nub = C(0x6A8AF8) }; break;
                case "brain": p = new CoralPal { tip = C(0xE86AB0), mid = C(0xC8458E), dark = C(0x8E2660), nub = C(0xF8A0D0) }; break;
                case "bubble": p = new CoralPal { tip = C(0xC86AD8), mid = C(0xA23AB8), dark = C(0x6E2080), nub = C(0xE89AF0) }; break;
                case "fire": p = new CoralPal { tip = C(0xE84A3A), mid = C(0xC02820), dark = C(0x841410), nub = C(0xFF8A6A) }; break;
                default: p = new CoralPal { tip = C(0xE8D24A), mid = C(0xC8AE22), dark = C(0x8E7A14), nub = C(0xFFF08A) }; break;   // horn
            }
            if (dead)
            {
                // dead coral is a desaturated grey, but keeps a trace of the original hue
                p.tip = Img.Mix(Img.Gray(0x9A), p.tip, 0.18f);
                p.mid = Img.Mix(Img.Gray(0x7A), p.mid, 0.18f);
                p.dark = Img.Mix(Img.Gray(0x55), p.dark, 0.18f);
                p.nub = Img.Mix(Img.Gray(0xB4), p.nub, 0.15f);
            }
            return p;
        }

        static Img CoralFamily(Img i, string n)
        {
            bool dead = n.StartsWith("dead_");
            string s = dead ? n.Substring(5) : n;
            string kind, form;
            if (s.EndsWith("_coral_block")) { form = "block"; kind = s.Substring(0, s.Length - 12); }
            else if (s.EndsWith("_coral_fan")) { form = "fan"; kind = s.Substring(0, s.Length - 10); }
            else if (s.EndsWith("_coral")) { form = "coral"; kind = s.Substring(0, s.Length - 6); }
            else return null;
            switch (kind) { case "tube": case "brain": case "bubble": case "fire": case "horn": break; default: return null; }
            var p = Coral(kind, dead);
            switch (form)
            {
                case "block": return CoralBlock(i, p, kind, dead);
                case "coral": return CoralShrub(i, p, kind);
                default: return CoralFan(i, p, kind);
            }
        }

        /// <summary>Solid coral block: a dense colony of the species' shape.</summary>
        static Img CoralBlock(Img i, CoralPal p, string kind, bool dead)
        {
            i.Fill(p.mid);
            switch (kind)
            {
                case "brain":
                    // meandering brain folds
                    for (int y = 0; y < 16; y++)
                        for (int x = 0; x < 16; x++)
                        {
                            float w = Mathf.Sin(x * 0.9f) + Mathf.Sin(y * 1.15f) + Mathf.Sin((x + y) * 0.45f);
                            if (Mathf.Abs(w) < 0.35f) i[x, y] = p.dark;
                            else if (w > 1.4f) i[x, y] = p.tip;
                            else if (w < -1.5f) i[x, y] = p.nub;
                        }
                    break;
                case "bubble":
                    // packed spheres with highlights
                    for (int cy = 2; cy < 16; cy += 5)
                        for (int cx = 2; cx < 16; cx += 5)
                            for (int y = -2; y <= 2; y++)
                                for (int x = -2; x <= 2; x++)
                                {
                                    float d = x * x + y * y;
                                    if (d > 5f) continue;
                                    i[cx + x, cy + y] = d < 1f ? p.dark : (d < 3f ? p.mid : p.tip);
                                }
                    for (int cy = 2; cy < 16; cy += 5)
                        for (int cx = 2; cx < 16; cx += 5) i.Set(cx - 1, cy - 1, p.nub);
                    break;
                case "fire":
                    // branching fingers seen head-on
                    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) i[x, y] = Img.Shade(p.mid, 0.94f + (Rnd01() - 0.5f) * 0.1f);
                    for (int k = 0; k < 6; k++)
                    {
                        int x = 1 + k * 2 + RndInt(2);
                        for (int y = 0; y < 16; y++) i.Set(x, y, (y % 4 == 0) ? p.dark : p.tip);
                    }
                    for (int k = 0; k < 5; k++) i.HLine(1 + k * 3, 0, 15, p.nub);
                    break;
                case "tube":
                    for (int y = 0; y < 16; y++)
                        for (int x = 0; x < 16; x++)
                        {
                            int g = (x + 1) / 5, h = (y + 1) / 5;
                            i[x, y] = ((g + h) % 2 == 0) ? p.mid : p.tip;
                        }
                    // tube openings
                    for (int cy = 3; cy < 16; cy += 5)
                        for (int cx = 3; cx < 16; cx += 5)
                        {
                            i.RectOutline(cx - 1, cy - 1, 3, 3, p.dark);
                            i.Set(cx, cy, p.nub);
                        }
                    break;
                default: // horn
                    for (int y = 0; y < 16; y++)
                        for (int x = 0; x < 16; x++)
                        {
                            float w = Mathf.Sin(x * 0.7f + y * 0.3f) + Mathf.Cos(y * 0.8f);
                            i[x, y] = w > 1.1f ? p.tip : (w < -0.9f ? p.dark : p.mid);
                        }
                    for (int k = 0; k < 8; k++) i.Line(RndInt(16), 0, RndInt(16), 15, p.dark);
                    break;
            }
            // rounded edge so the block reads as a lumpy colony rather than a flat tile
            for (int k = 0; k < 8; k++)
            {
                int x = RndInt(16), y = RndInt(16);
                Blob(i, x, y, 1 + RndInt(2), p.nub, 0.4f);
            }
            if (dead)
            {
                i.Clusters(C(0x6E6E66), 12, 1, 3);
                i.Speckle(C(0x3E3E38), 0.08f);
            }
            i.RectOutline(0, 0, 16, 16, Img.Shade(p.dark, 0.9f));
            return i;
        }

        /// <summary>Free-standing coral shrub: an upright branching growth with a clear silhouette.</summary>
        static Img CoralShrub(Img i, CoralPal p, string kind)
        {
            i.Clear();
            Color32 mid = p.mid, tip = p.tip, dark = p.dark, nub = p.nub;
            switch (kind)
            {
                case "tube":
                    // a cluster of tapering tubes
                    for (int k = 0; k < 4; k++)
                    {
                        int x = 3 + k * 3;
                        int h = 7 + (k == 1 || k == 2 ? 4 : 0);
                        for (int y = 16 - h; y < 16; y++)
                        {
                            i.Set(x, y, y < 16 - h + 2 ? nub : mid);
                            i.Set(x + 1, y, dark);
                        }
                        // open mouth
                        i.Set(x, 16 - h, nub);
                        i.Set(x + 1, 16 - h, Img.Shade(nub, 0.8f));
                    }
                    i.HLine(15, 2, 13, dark);
                    break;
                case "brain":
                    // a rounded lumpy mound
                    for (int y = 5; y < 16; y++)
                        for (int x = 1; x < 15; x++)
                        {
                            float nx = (x - 7.5f) / 7f, ny = (y - 14f) / 9f;
                            if (nx * nx + ny * ny > 1f) continue;
                            float w = Mathf.Sin(x * 1.1f) + Mathf.Cos(y * 1.3f);
                            i[x, y] = w > 0.6f ? tip : (w < -0.7f ? dark : mid);
                        }
                    i.Set(5, 7, nub); i.Set(10, 6, nub); i.Set(7, 5, nub);
                    break;
                case "bubble":
                    for (int k = 0; k < 7; k++)
                    {
                        int cx = 2 + RndInt(12), cy = 6 + RndInt(10);
                        int r = 2 + RndInt(2);
                        for (int y = -r; y <= r; y++)
                            for (int x = -r; x <= r; x++)
                            {
                                float d = (x * x + y * y) / (float)(r * r);
                                if (d > 1f) continue;
                                i[cx + x, cy + y] = d < 0.4f ? dark : (d < 0.75f ? mid : tip);
                            }
                        i.Set(cx - 1, cy - 1, nub);
                    }
                    break;
                case "fire":
                    for (int k = 0; k < 5; k++)
                    {
                        int x = 2 + k * 3;
                        int h = 6 + RndInt(7);
                        for (int y = 16 - h; y < 16; y++)
                        {
                            i.Set(x, y, y < 16 - h + 3 ? tip : mid);
                            i.Set(x + 1, y, dark);
                            if (Rnd01() < 0.3f) i.Set(x + 2, y, mid);
                        }
                        i.Set(x, 16 - h, nub);
                    }
                    break;
                default: // horn
                    for (int k = 0; k < 4; k++)
                    {
                        int x = 2 + k * 4;
                        int h = 9 + RndInt(6);
                        for (int y = 16 - h; y < 16; y++)
                        {
                            int sway = Mathf.RoundToInt(Mathf.Sin((y - (16 - h)) * 0.5f + k) * 1.5f);
                            i.Set(x + sway, y, y < 16 - h + 3 ? nub : mid);
                            i.Set(x + sway + 1, y, dark);
                        }
                    }
                    break;
            }
            // common base matt
            i.HLine(15, 3, 13, dark);
            i.HLine(14, 4, 12, Img.Shade(mid, 0.85f));
            return i;
        }

        /// <summary>Fan: a radial fan silhouette flattened onto the tile.</summary>
        static Img CoralFan(Img i, CoralPal p, string kind)
        {
            i.Clear();
            int arms = kind == "brain" ? 7 : (kind == "bubble" ? 9 : 8);
            for (int k = 0; k < arms; k++)
            {
                float t = k / (float)(arms - 1);
                float a = Mathf.Lerp(-1.1f, 1.1f, t);
                float dx = Mathf.Sin(a), dy = Mathf.Cos(a);
                int len = 12 + ((k % 2 == 0) ? 2 : 0);
                for (int s = 0; s <= len; s++)
                {
                    int x = 7 + Mathf.RoundToInt(dx * s * 0.95f);
                    int y = 15 - Mathf.RoundToInt(dy * s);
                    var c = (s > len - 3) ? p.tip : (s % 4 == 0 ? p.nub : p.mid);
                    i.Set(x, y, c);
                    if (kind == "brain") { i.Set(x + 1, y, Img.Shade(c, 0.85f)); i.Set(x - 1, y, p.dark); }
                    else if (kind == "fire" && s % 3 == 0) i.Set(x + 1, y, p.dark);
                }
            }
            // cross veins
            for (int s = 2; s < 10; s += 3)
                for (int x = 7 - s; x <= 7 + s; x++) i.Set(x, 15 - s, Img.Shade(p.dark, 0.95f));
            // stalk
            for (int y = 12; y < 16; y++) { i.Set(7, y, p.dark); i.Set(8, y, Img.Shade(p.dark, 0.8f)); }
            return i;
        }
    }
}
