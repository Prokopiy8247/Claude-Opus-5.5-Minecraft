using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Original procedural particle sprites stored as layers "particle/&lt;name&gt;" in the block texture array.</summary>
    public static class ParticleTextures
    {
        public static readonly string[] Names =
        {
            "generic_0", "generic_1", "generic_2", "generic_3", "generic_4", "generic_5", "generic_6", "generic_7",
            "flame", "soul_flame", "copper_flame", "small_flame", "lava", "bubble", "splash", "drip", "heart", "angry", "note", "crit", "enchanted_hit", "spell", "spell_ambient",
            "portal", "glyph_0", "glyph_1", "glyph_2", "glyph_3", "glyph_4", "glyph_5", "glyph_6", "glyph_7",
            "happy", "end_rod", "dust", "spore", "petal", "leaf", "sulfur", "explosion_0", "explosion_1", "explosion_2", "explosion_3", "explosion_4", "explosion_5", "explosion_6", "explosion_7",
            "sweep_0", "sweep_1", "sweep_2", "sweep_3", "snowflake", "damage", "totem", "glow", "soul", "sonic_0", "sonic_1", "sonic_2", "sonic_3", "rain", "snow", "spark", "cloud", "wax", "scrape", "dragon_breath", "firefly",
            "destroy_0", "destroy_1", "destroy_2", "destroy_3", "destroy_4", "destroy_5", "destroy_6", "destroy_7", "destroy_8", "destroy_9",
        };

        static readonly Dictionary<string, int> layers = new Dictionary<string, int>();

        public static void Register()
        {
            layers.Clear();
            foreach (var n in Names) layers[n] = Tex.Id("particle/" + n);
        }

        public static int Layer(string name) => layers.TryGetValue(name, out int l) ? l : 0;

        static Color32 C(uint rgb, byte a = 255) => Img.C(rgb, a);

        public static Color32[] Pixels(string name)
        {
            var i = Img.Seeded("particle_" + name);
            i.Clear();
            if (name.StartsWith("generic_"))
            {
                int k = name[8] - '0';
                float rad = 1.5f + k * 0.85f;
                Blob(i, 8, 8, rad, Img.Gray(255), 0.35f);
                return i.px;
            }
            if (name.StartsWith("explosion_"))
            {
                int k = name[10] - '0';
                float rad = 7.5f - k * 0.5f;
                Blob(i, 8, 8, rad, Img.Gray(235 - k * 12), 0.55f);
                return i.px;
            }
            if (name.StartsWith("glyph_"))
            {
                int k = name[6] - '0';
                var rr = new RNG(k * 17 + 3);
                for (int s = 0; s < 5; s++)
                {
                    int x0 = 5 + rr.Next(6), y0 = 4 + rr.Next(8);
                    int x1 = Mathf.Clamp(x0 + rr.Range(-3, 3), 4, 11), y1 = Mathf.Clamp(y0 + rr.Range(-3, 3), 4, 11);
                    i.Line(x0, y0, x1, y1, C(0xFFFFFF));
                }
                return i.px;
            }
            if (name.StartsWith("sweep_"))
            {
                int k = name[6] - '0';
                for (int a = 0; a < 40; a++)
                {
                    float ang = Mathf.Lerp(-1.2f, 1.2f, a / 39f);
                    float rad = 6.5f;
                    int x = 8 + Mathf.RoundToInt(Mathf.Sin(ang) * rad), y = 10 - Mathf.RoundToInt(Mathf.Cos(ang) * rad * 0.8f);
                    byte v = (byte)(255 - k * 40);
                    i.Set(x, y, new Color32(v, v, v, 255)); i.Set(x, y + 1, new Color32(v, v, v, 255));
                }
                return i.px;
            }
            if (name.StartsWith("sonic_"))
            {
                int k = name[6] - '0';
                float rad = 3 + k * 1.5f;
                for (int a = 0; a < 64; a++) { float ang = a / 64f * Mathf.PI * 2; i.Set(8 + Mathf.RoundToInt(Mathf.Cos(ang) * rad), 8 + Mathf.RoundToInt(Mathf.Sin(ang) * rad), C(0x6FE8F0)); }
                return i.px;
            }
            if (name.StartsWith("destroy_"))
            {
                int k = name[8] - '0';
                var rr = new RNG(4242);
                // cracks grow with stage: dark translucent lines
                int lines = 2 + k * 3;
                for (int s = 0; s < lines; s++)
                {
                    int x = 8 + rr.Range(-2, 2), y = 8 + rr.Range(-2, 2);
                    int len = 2 + rr.Next(3 + k);
                    for (int st = 0; st < len; st++)
                    {
                        x += rr.Range(-1, 1); y += rr.Range(-1, 1);
                        i.Set(x & 15, y & 15, C(0x000000, 160));
                    }
                }
                return i.px;
            }
            switch (name)
            {
                case "flame": Flame(i, C(0xFFE08A), C(0xFF9A2A), C(0xD8401A)); break;
                case "soul_flame": Flame(i, C(0xB8FFFF), C(0x48D8E0), C(0x1A7A90)); break;
                case "copper_flame": Flame(i, C(0xC8FFD0), C(0x5AE08A), C(0x1A905A)); break;
                case "small_flame": Blob(i, 8, 9, 1.8f, C(0xFFC050), 0f); i.Set(8, 7, C(0xFFE8A0)); break;
                case "lava": Blob(i, 8, 8, 2.2f, C(0xFF9A20), 0f); i.Set(8, 8, C(0xFFE060)); break;
                case "bubble": Ring(i, 8, 8, 3.2f, C(0xDDEEFF)); i.Set(7, 6, C(0xFFFFFF)); break;
                case "splash": for (int k = 0; k < 6; k++) i.Set(6 + (k % 3) * 2, 6 + (k / 3) * 3, C(0xCFE4FF)); break;
                case "drip": Blob(i, 8, 9, 1.6f, C(0xFFFFFF), 0f); i.Set(8, 7, C(0xFFFFFF)); break;
                case "heart":
                    i.Sprite(new[] { ".rr.rr.", "rRRrRRr", "rRRRRRr", ".rRRRr.", "..rRr..", "...r..." }, Pal('r', C(0x9A0A0A), 'R', C(0xFF3A3A)), 4, 5); break;
                case "damage":
                    i.Sprite(new[] { ".rr.rr.", "rRRrRRr", "rRRRRRr", ".rRRRr.", "..rRr..", "...r..." }, Pal('r', C(0x2A0A0A), 'R', C(0x6A1A1A)), 4, 5); break;
                case "angry":
                    Blob(i, 8, 8, 4f, C(0x5A5A5A), 0.2f); i.Line(5, 6, 7, 8, C(0xFF2020)); i.Line(11, 6, 9, 8, C(0xFF2020)); break;
                case "note":
                    i.Sprite(new[] { "....##", "....#.#", "....#..", "....#..", "..###..", ".####..", ".###..." }, Pal('#', C(0xFFFFFF)), 4, 4); break;
                case "crit": Star(i, C(0xFFFFFF), 5); break;
                case "enchanted_hit": Star(i, C(0xFFFFFF), 4); break;
                case "spell": Star(i, C(0xFFFFFF), 3); i.Set(8, 8, C(0xFFFFFF)); break;
                case "spell_ambient": Blob(i, 8, 8, 2f, C(0xFFFFFF), 0.3f); break;
                case "portal": i.Rect(7, 7, 2, 2, C(0xFFFFFF)); i.Set(6, 8, C(0xDDDDDD)); i.Set(9, 7, C(0xDDDDDD)); break;
                case "happy": Star(i, C(0xFFFFFF), 3); break;
                case "end_rod": Star(i, C(0xFFFFFF), 4); Blob(i, 8, 8, 1.2f, C(0xFFFFFF), 0); break;
                case "dust": i.Rect(7, 7, 3, 3, C(0xFFFFFF)); break;
                case "spore": i.Rect(7, 7, 2, 2, C(0xFFFFFF)); break;
                case "petal": i.Sprite(new[] { "..##.", ".####", "#####", ".###.", "..#.." }, Pal('#', C(0xFFFFFF)), 5, 5); break;
                case "leaf": i.Sprite(new[] { "...#", "..##", ".###", "###.", "##..", "#..." }, Pal('#', C(0xFFFFFF)), 6, 5); break;
                case "sulfur": Blob(i, 8, 8, 3.5f, C(0xF0E860), 0.4f); break;
                case "snowflake": i.Line(8, 4, 8, 12, C(0xFFFFFF)); i.Line(4, 8, 12, 8, C(0xFFFFFF)); i.Line(5, 5, 11, 11, C(0xE0F0FF)); i.Line(11, 5, 5, 11, C(0xE0F0FF)); break;
                case "totem": Star(i, C(0xFFFFFF), 4); break;
                case "glow": Blob(i, 8, 8, 1.6f, C(0xFFFFFF), 0); break;
                case "soul": Blob(i, 8, 10, 2.5f, C(0x9AF0FF), 0.2f); i.Set(7, 9, C(0x103040)); i.Set(9, 9, C(0x103040)); i.Set(8, 6, C(0x9AF0FF)); break;
                case "rain": i.VLine(8, 1, 14, C(0x6A8AE8, 255)); i.VLine(3, 5, 12, C(0x6A8AE8, 255)); i.VLine(12, 2, 9, C(0x6A8AE8, 255)); break;
                case "snow": for (int k = 0; k < 7; k++) { var rr = new RNG(k * 13 + 1); i.Set(rr.Next(16), rr.Next(16), C(0xFFFFFF)); } break;
                case "spark": Blob(i, 8, 8, 1.3f, C(0xFFFFFF), 0); i.Set(8, 6, C(0xFFFFFF)); i.Set(8, 10, C(0xFFFFFF)); i.Set(6, 8, C(0xFFFFFF)); i.Set(10, 8, C(0xFFFFFF)); break;
                case "cloud": Blob(i, 8, 8, 5.5f, C(0xFFFFFF), 0.3f); break;
                case "wax": Star(i, C(0xFFB030), 3); break;
                case "scrape": Star(i, C(0x60E0C0), 3); break;
                case "dragon_breath": Blob(i, 8, 8, 3f, C(0xE070FF), 0.3f); break;
                case "firefly": Blob(i, 8, 8, 1.2f, C(0xF8FF90), 0); break;
                default: Blob(i, 8, 8, 3f, C(0xFFFFFF), 0.3f); break;
            }
            return i.px;
        }

        static Dictionary<char, Color32> Pal(params object[] kv)
        {
            var d = new Dictionary<char, Color32>();
            for (int k = 0; k + 1 < kv.Length; k += 2) d[(char)kv[k]] = (Color32)kv[k + 1];
            return d;
        }

        static void Blob(Img i, float cx, float cy, float rad, Color32 c, float noise)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                    float d = Mathf.Sqrt(dx * dx + dy * dy);
                    float edge = rad + (noise > 0 ? (Hash.Float01(77, x, y) - 0.5f) * noise * 3f : 0);
                    if (d <= edge)
                    {
                        float shade = 1f - (d / Mathf.Max(0.5f, rad)) * 0.25f;
                        i[x, y] = Img.Shade(c, shade);
                    }
                }
        }
        static void Ring(Img i, float cx, float cy, float rad, Color32 c)
        {
            for (int a = 0; a < 48; a++) { float ang = a / 48f * Mathf.PI * 2; i.Set(Mathf.RoundToInt(cx + Mathf.Cos(ang) * rad - 0.5f), Mathf.RoundToInt(cy + Mathf.Sin(ang) * rad - 0.5f), c); }
        }
        static void Star(Img i, Color32 c, int r)
        {
            i.Line(8 - r, 8, 8 + r, 8, c); i.Line(8, 8 - r, 8, 8 + r, c);
            i.Line(8 - r / 2, 8 - r / 2, 8 + r / 2, 8 + r / 2, c); i.Line(8 + r / 2, 8 - r / 2, 8 - r / 2, 8 + r / 2, c);
        }
        static void Flame(Img i, Color32 core, Color32 mid, Color32 edge)
        {
            string[] rows = { "......e.....", ".....em.....", ".....emm....", "....emme....", "...emmcme...", "...emccme...", "..emmccmme..", "..emcccme...", "..emcccmme..", "...emccme...", "....emme....", ".....ee....." };
            i.Sprite(rows, Pal('e', edge, 'm', mid, 'c', core), 2, 2);
        }
    }
}
