using System;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Sculk family textures: the mossy ground block, veins, catalyst, sensor (with tendrils) and shrieker.
    /// </summary>
    public static partial class TextureGen
    {
        static readonly Color32 SculkBase = C(0x0E2A2E);
        static readonly Color32 SculkMid = C(0x123A3E);
        static readonly Color32 SculkGlow = C(0x1E5A62);
        static readonly Color32 SculkBright = C(0x2E8A94);
        static readonly Color32 SculkPale = C(0x9ADFE0);

        static Img SculkFamily(Img i, string n)
        {
            switch (n)
            {
                case "sculk": return SculkGround(i, 0);
                case "sculk_catalyst_side": return SculkCatalystSide(i);
                case "sculk_catalyst_top": return SculkGround(i, 1);
                case "sculk_catalyst_bottom": return SculkGround(i, 2);
                case "sculk_sensor_top": return SculkSensorTop(i);
                case "sculk_sensor_side": return SculkSensorSide(i);
                case "sculk_sensor_bottom": return SculkSensorBottom(i);
                case "sculk_sensor_tendril_active": return SculkTendril(i, true);
                case "sculk_sensor_tendril_inactive": return SculkTendril(i, false);
                case "sculk_shrieker_top": return SculkShriekerTop(i);
                case "sculk_shrieker_side": return SculkShriekerSide(i);
                case "sculk_shrieker_bottom": return SculkGround(i, 2);
                case "sculk_vein": return SculkVine(i);
            }
            return null;
        }

        /// <summary>Mottled sculk ground. mode 0 = plain, 1 = with a raised glowing boss, 2 = underside.</summary>
        static Img SculkGround(Img i, int mode)
        {
            i.PaletteNoise(new[] { SculkBase, SculkMid, C(0x0A2024), C(0x163E44) }, new[] { 0.36f, 0.28f, 0.2f, 0.16f }, 1);
            // organic lumps and pale flecks
            for (int k = 0; k < 9; k++) Blob(i, RndInt(16), RndInt(16), 1 + RndInt(2), SculkMid, 0.5f);
            for (int k = 0; k < 14; k++) i.Set(RndInt(16), RndInt(16), SculkGlow);
            for (int k = 0; k < 6; k++) i.Set(RndInt(16), RndInt(16), SculkBright);
            for (int k = 0; k < 3; k++) i.Set(RndInt(16), RndInt(16), SculkPale);
            if (mode == 2)
            {
                i.Multiply(0.72f);
                i.Speckle(C(0x08161A), 0.25f);
            }
            else if (mode == 1)
            {
                // a raised, glowing growth in the middle
                for (int y = 2; y < 14; y++)
                    for (int x = 2; x < 14; x++)
                    {
                        float d = Mathf.Sqrt((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f));
                        if (d > 5.6f) continue;
                        float f = d < 2.5f ? 1.5f : (d < 4f ? 1.2f : 0.95f);
                        i[x, y] = Img.Shade(d < 2.5f ? SculkBright : SculkGlow, f + (Rnd01() - 0.5f) * 0.1f);
                    }
                i.RectOutline(6, 6, 4, 4, SculkPale);
                i.Set(7, 7, C(0xE8FFFF)); i.Set(8, 8, SculkPale);
                for (int k = 0; k < 6; k++) i.Set(3 + RndInt(10), 2 + RndInt(12), SculkBright);
            }
            return i;
        }

        static Img SculkCatalystSide(Img i)
        {
            SculkGround(i, 0);
            // bone-white ribs across the side
            for (int y = 2; y < 15; y += 4)
            {
                i.HLine(y, 1, 14, C(0xBFD8D4));
                i.HLine(y + 1, 2, 13, C(0x7A9A98));
            }
            for (int k = 0; k < 4; k++) i.VLine(2 + k * 4, 1, 14, C(0x5E7A78));
            i.RectOutline(0, 0, 16, 16, C(0x0A1E22));
            return i;
        }

        static Img SculkSensorTop(Img i)
        {
            SculkGround(i, 0);
            i.RectOutline(2, 2, 12, 12, C(0x0A1E22));
            for (int y = 3; y < 13; y++)
                for (int x = 3; x < 13; x++)
                {
                    float d = Mathf.Sqrt((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f));
                    if (d > 4.6f) continue;
                    if (d < 1.6f) i[x, y] = C(0x0C2226);
                    else if (d < 3f) i[x, y] = (Rnd01() < 0.5f) ? SculkBright : SculkGlow;
                    else i[x, y] = SculkMid;
                }
            i.Set(7, 7, SculkPale); i.Set(8, 8, SculkPale);
            // ring of glowing dots
            for (int k = 0; k < 8; k++)
            {
                float a = k * Mathf.PI / 4f;
                i.Set(7 + Mathf.RoundToInt(Mathf.Cos(a) * 3f), 7 + Mathf.RoundToInt(Mathf.Sin(a) * 3f), C(0x60E0E0));
            }
            return i;
        }

        /// <summary>Sensor body is 16x8x16 px, so only rows 8-15 of the side are visible.</summary>
        static Img SculkSensorSide(Img i)
        {
            SculkGround(i, 0);
            for (int x = 0; x < 16; x++)
            {
                i[x, 8] = (x & 3) == 1 ? SculkBright : SculkGlow;
                i[x, 9] = SculkMid;
                i[x, 15] = C(0x081A1E);
            }
            // soft glowing facets across the band
            for (int y = 10; y < 15; y++)
                for (int x = 0; x < 16; x++)
                {
                    float t = Mathf.Sin(x * 0.8f + y * 0.9f);
                    if (t > 0.6f) i[x, y] = SculkGlow;
                    else if (t < -0.7f) i[x, y] = C(0x0A2024);
                }
            i.Set(3, 11, SculkPale); i.Set(12, 12, SculkPale);
            return i;
        }

        static Img SculkSensorBottom(Img i)
        {
            SculkGround(i, 0);
            i.RectOutline(0, 0, 16, 16, C(0x08161A));
            i.RectOutline(3, 3, 10, 10, C(0x14343A));
            i.Clusters(C(0x0A2024), 8, 1, 3);
            return i;
        }

        /// <summary>Tendrils that reach up off the sensor. Transparent elsewhere.</summary>
        static Img SculkTendril(Img i, bool active)
        {
            i.Clear();
            Color32 a = active ? C(0x2EC8CC) : C(0x1E5A62);
            Color32 b = active ? C(0x8AF8F8) : C(0x3E8A94);
            Color32 d = active ? C(0x18808A) : C(0x0E3A40);
            for (int k = 0; k < 4; k++)
            {
                int x = 2 + k * 4;
                int top = 3 + RndInt(active ? 2 : 5);
                for (int y = top; y < 16; y++)
                {
                    int xx = x + Mathf.RoundToInt(Mathf.Sin((y - top) * 0.5f + k) * 1.4f);
                    i.Set(xx, y, (y < top + 3) ? b : a);
                    if (active && Rnd01() < 0.25f) i.Set(xx + 1, y, b);
                    i.Set(xx - 1, y, d);
                }
                i.Set(x, top, b);
                if (active) { i.Set(x, top - 1, b); i.Set(x, top - 2, Img.WithA(b, 180)); }
            }
            return i;
        }

        static Img SculkShriekerTop(Img i)
        {
            SculkGround(i, 2);
            i.RectOutline(1, 1, 14, 14, C(0x0A1E22));
            // a screaming mouth of concentric bone rings
            for (int r = 6; r >= 1; r--)
            {
                var c = (r % 2 == 0) ? C(0xBFD8D4) : C(0x5E8A88);
                for (int k = 0; k < 24; k++)
                {
                    float a = k * Mathf.PI / 12f;
                    i.Set(7 + Mathf.RoundToInt(Mathf.Cos(a) * r), 7 + Mathf.RoundToInt(Mathf.Sin(a) * r), c);
                }
            }
            i.Rect(6, 6, 4, 4, C(0x08161A));
            i.Set(7, 7, SculkBright); i.Set(8, 8, SculkBright);
            return i;
        }

        /// <summary>Shrieker body is 16x8x16 px: bone rim on rows 8-10, sculk below.</summary>
        static Img SculkShriekerSide(Img i)
        {
            SculkGround(i, 0);
            for (int x = 0; x < 16; x++)
            {
                i[x, 8] = C(0xD2E4E0);
                i[x, 9] = (x & 3) == 0 ? C(0x7A9A96) : C(0xB4CCC8);
                i[x, 10] = (x & 3) == 0 ? C(0x2E4A48) : C(0x7A9A96);
                i[x, 15] = C(0x081A1E);
            }
            // bone ribs running down from the rim
            for (int x = 1; x < 16; x += 4) { i.VLine(x, 11, 13, C(0x5E827E)); i.Set(x, 14, C(0x2E4A48)); }
            return i;
        }

        static Img SculkVine(Img i)
        {
            i.Clear();
            // veins crawl across the tile from a central stem
            int nodes = 5;
            for (int k = 0; k < nodes; k++)
            {
                int x = 2 + RndInt(12), y = 2 + RndInt(12);
                int len = 3 + RndInt(5);
                float a = Rnd01() * Mathf.PI * 2f;
                int ex = x + Mathf.RoundToInt(Mathf.Cos(a) * len), ey = y + Mathf.RoundToInt(Mathf.Sin(a) * len);
                Strand(i, x, y, ex, ey, SculkGlow, 0.8f);
                i.Set(x, y, SculkBright);
            }
            // a few bright tips
            for (int k = 0; k < 6; k++) i.Set(RndInt(16), RndInt(16), SculkBright);
            for (int k = 0; k < 3; k++) i.Set(RndInt(16), RndInt(16), SculkPale);
            return i;
        }
    }
}
