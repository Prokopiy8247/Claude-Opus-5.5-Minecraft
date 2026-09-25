using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public static partial class TextureGen
    {
        static readonly Color32 T = new Color32(0, 0, 0, 0);
        static Dictionary<char, Color32> Pal(params object[] kv)
        {
            var d = new Dictionary<char, Color32>();
            for (int i = 0; i + 1 < kv.Length; i += 2) d[(char)kv[i]] = kv[i + 1] is Color32 c ? c : C((uint)(int)kv[i + 1]);
            return d;
        }

        static Img Dirt(Img i)
        {
            i.PaletteNoise(new[] { C(0x8B6340), C(0x7A5535), C(0x9A7148), C(0x6B4A2E), C(0xA27B52) }, new[] { 0.34f, 0.26f, 0.2f, 0.12f, 0.08f }, 1);
            return i;
        }
        static Img GrassTop(Img i)
        {
            i.PaletteNoise(new[] { Img.Gray(160), Img.Gray(142), Img.Gray(178), Img.Gray(128), Img.Gray(196) }, new[] { 0.3f, 0.25f, 0.22f, 0.13f, 0.1f }, 1);
            return i.MarkTinted();
        }
        static Img GrassSide(Img i, bool snowy)
        {
            Dirt(i);
            for (int x = 0; x < 16; x++)
            {
                int depth = 3 + (int)(Hash.Get(x, 1234) % 3);
                if (x > 0 && Hash.Get(x, 55) % 4 == 0) depth += 1;
                for (int y = 0; y < depth; y++)
                {
                    if (snowy) i[x, y] = y == depth - 1 ? Img.Gray(220) : Img.Gray(245 - (int)(Hash.Get(x, y) % 20));
                    else { var g = Img.Gray(150 + (int)(Hash.Get(x * 3, y) % 40)); g.a = 128; i[x, y] = g; }
                }
                if (!snowy && Hash.Get(x, 99) % 3 == 0) { var g = Img.Gray(135); g.a = 128; i[x, depth] = g; }
            }
            return i;
        }

        static Img Liquid(Img i, bool water, bool flow, int frame)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float t = frame / 16f * Mathf.PI * 2f;
                    float fy = flow ? y + frame : y;
                    float v = Mathf.Sin((x * 0.7f + fy * 0.45f) + t) * 0.5f + Mathf.Sin((x * 0.3f - fy * 0.9f) - t * 2f) * 0.3f
                              + ((Hash.Get(x, (int)fy & 15) & 255) / 255f - 0.5f) * 0.35f;
                    if (water)
                    {
                        int g = (int)(185 + v * 28);
                        i[x, y] = C(g - 10, g, g + 10, 190);
                    }
                    else
                    {
                        float h = Mathf.Clamp01(0.55f + v * 0.35f);
                        Color32 dark = C(0xC24A0A), mid = C(0xE86E12), hot = C(0xFFB33A), white = C(0xFFE58A);
                        Color32 c = h < 0.4f ? Img.Mix(dark, mid, h / 0.4f) : (h < 0.8f ? Img.Mix(mid, hot, (h - 0.4f) / 0.4f) : Img.Mix(hot, white, (h - 0.8f) / 0.2f));
                        i[x, y] = c;
                    }
                }
            return i;
        }

        static Img Portal(Img i, int frame, bool end)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float t = frame / 16f * Mathf.PI * 2;
                    float cx = x - 7.5f, cy = y - 7.5f;
                    float r = Mathf.Sqrt(cx * cx + cy * cy);
                    float a = Mathf.Atan2(cy, cx);
                    float v = Mathf.Sin(a * 3 + r * 0.9f - t * 2) * 0.5f + 0.5f;
                    v = v * 0.6f + ((Hash.Get(x + frame * 3, y) & 255) / 255f) * 0.25f;
                    if (end)
                    {
                        float s = (Hash.Get(x * 5 + frame, y * 3) & 255) / 255f;
                        i[x, y] = s > 0.93f ? C(0xB8FFE0) : Img.Mix(C(0x0A0F1C), C(0x1E3A40), v * 0.6f);
                    }
                    else i[x, y] = Img.WithA(Img.Mix(C(0x3A0A8C), C(0xB44BFF), v), 200);
                }
            return i;
        }

        static Img Fire(Img i, int frame, bool soul)
        {
            i.Clear();
            for (int x = 0; x < 16; x++)
            {
                float h = 6 + ((Hash.Get(x, frame) % 9)) + Mathf.Sin(x * 0.9f + frame * 0.8f) * 2.5f;
                for (int y = 0; y < 16; y++)
                {
                    int fromBottom = 15 - y;
                    if (fromBottom > h) continue;
                    float f = fromBottom / Mathf.Max(1f, h);
                    if (((Hash.Get(x + frame * 7, y) & 7) == 0) && f > 0.6f) continue;
                    Color32 c = soul ? (f < 0.3f ? C(0xE8FFFF) : f < 0.7f ? C(0x5FDFE8) : C(0x1C8F9A))
                                     : (f < 0.25f ? C(0xFFF3B0) : f < 0.55f ? C(0xFFC33A) : f < 0.8f ? C(0xF08A1A) : C(0xC0461A));
                    i[x, y] = c;
                }
            }
            return i;
        }

        static Img Flower(Img i, string[] rows, Dictionary<char, Color32> pal) { i.Clear(); return i.Sprite(rows, pal); }

        static readonly string[] FlowerStem = {
            "................", "................", "................", "................",
            "................", "................", "................", ".......s........",
            ".......s........", "......ls........", ".....ll.s.......", ".......s.ll.....",
            ".......sll......", ".......s........", ".......s........", ".......s........" };

        static Img SimpleFlower(Img i, Color32 petal, Color32 center, Color32 dark, int style = 0)
        {
            i.Clear();
            var stem = C(0x3F8A2A); var leaf = C(0x4FA232);
            i.Sprite(FlowerStem, Pal('s', stem, 'l', leaf));
            if (style == 0) // round bloom
            {
                i.Sprite(new[] { "......pp........", ".....pPPp.......", "....pPcPPp......", "....pPPPPp......", ".....pPPp.......", "......pp........" }, Pal('p', dark, 'P', petal, 'c', center), 1, 2);
            }
            else if (style == 1) // tulip
            {
                i.Sprite(new[] { ".....p.p.p......", ".....PpPpP......", ".....PPPPP......", ".....dPPPd......", "......ddd......." }, Pal('p', petal, 'P', petal, 'd', dark), 1, 3);
            }
            else if (style == 2) // daisy
            {
                i.Sprite(new[] { ".....P.P.P......", "......PPP.......", "....PPcccPP.....", "......PcP.......", ".....P.P.P......" }, Pal('P', petal, 'c', center), 1, 2);
            }
            else if (style == 3) // cluster (allium, bluet)
            {
                i.Sprite(new[] { ".....pPp........", "....PpPpP.......", "....pPcPp.......", "....PpPpP.......", ".....pPp........" }, Pal('p', dark, 'P', petal, 'c', center), 2, 2);
            }
            return i;
        }

        static Img Crop(Img i, string kind, int stage)
        {
            i.Clear();
            int maxStage = kind == "wheat" ? 7 : 3;
            float f = (stage + 1f) / (maxStage + 1f);
            int h = 3 + Mathf.RoundToInt(f * 12);
            Color32 stem = kind == "wheat" ? (stage >= 7 ? C(0xC7A640) : Img.Mix(C(0x3F9A2A), C(0xB8A13A), f * f)) : C(0x3F9A2A);
            Color32 leaf = kind == "wheat" ? Img.Shade(stem, 1.15f) : C(0x5BBA35);
            for (int k = 0; k < 5; k++)
            {
                int x = 1 + k * 3 + (int)(Hash.Get(k, stage) % 2);
                int top = 16 - h + (int)(Hash.Get(k * 7, stage) % 3);
                for (int y = Mathf.Max(0, top); y < 16; y++) i[x, y] = stem;
                if (h > 5) { i.Set(x - 1, top + 2, leaf); i.Set(x + 1, top + 4, leaf); }
                if (kind == "wheat" && stage >= 5) { i.Set(x, top, C(0xD9BC4E)); i.Set(x, top + 1, C(0xB5972E)); i.Set(x + 1, top + 1, C(0xD9BC4E)); }
            }
            if (stage == maxStage)
            {
                if (kind == "carrots") for (int k = 0; k < 4; k++) { int x = 2 + k * 4; i.Rect(x, 13, 2, 3, C(0xF58A1A)); i.Set(x, 12, C(0xFFA83A)); }
                if (kind == "potatoes") for (int k = 0; k < 4; k++) { int x = 1 + k * 4; i.Rect(x, 13, 3, 2, C(0xC9A45C)); i.Set(x + 1, 13, C(0xE0BE78)); }
                if (kind == "beetroots") for (int k = 0; k < 3; k++) { int x = 2 + k * 5; i.Rect(x, 12, 3, 3, C(0x9E1F2E)); i.Set(x + 1, 12, C(0xC43244)); }
            }
            return i;
        }

        static Img Torch(Img i, Color32 flameHi, Color32 flameLo, Color32 stick, bool unlit = false)
        {
            i.Clear();
            for (int y = 8; y < 16; y++) { i.Set(7, y, stick); i.Set(8, y, Img.Shade(stick, 0.8f)); }
            if (unlit) { i.Set(7, 6, C(0x5A3A3A)); i.Set(8, 6, C(0x4A2A2A)); i.Set(7, 7, C(0x6A4A3A)); i.Set(8, 7, C(0x5A3A2A)); return i; }
            i.Set(7, 6, flameHi); i.Set(8, 6, flameHi); i.Set(7, 7, flameLo); i.Set(8, 7, flameHi);
            i.Set(7, 5, Img.WithA(flameHi, 200));
            return i;
        }

        static Img Specific(Img i, string n, int frame)
        {
            switch (n)
            {
                case "missing": return Missing();
                // ------------------------------------------------ terrain
                case "stone": return Stone(i, C(0x7D7D7D));
                case "cobblestone": return Cobble(i, C(0x7A7A7A), C(0x505050));
                case "mossy_cobblestone": return Cobble(i, C(0x7A7A7A), C(0x505050), C(0x5B7A2E));
                case "smooth_stone": i.Noise(C(0x9E9E9E), 0.03f); i.RectOutline(0, 0, 16, 16, C(0x7A7A7A)); return i;
                case "stone_bricks": return StoneBricks(i, C(0x7B7B7B), C(0x5A5A5A));
                case "mossy_stone_bricks": StoneBricks(i, C(0x7B7B7B), C(0x5A5A5A)); return i.Clusters(C(0x5E7A31), 10, 2, 5, C(0x4A6328));
                case "cracked_stone_bricks": StoneBricks(i, C(0x777777), C(0x555555)); i.Line(3, 1, 7, 6, C(0x404040)); i.Line(7, 6, 5, 11, C(0x404040)); i.Line(11, 8, 14, 14, C(0x404040)); return i;
                case "chiseled_stone_bricks": i.Noise(C(0x7B7B7B), 0.05f); i.RectOutline(0, 0, 16, 16, C(0x5A5A5A)); i.RectOutline(3, 3, 10, 10, C(0x5A5A5A)); i.RectOutline(5, 5, 6, 6, C(0x9A9A9A)); return i;
                case "granite": i.PaletteNoise(new[] { C(0x9A6B58), C(0x8A5E4C), C(0xB07D69), C(0x6E4A3C) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i;
                case "polished_granite": return Polished(i, C(0x9A6B58));
                case "diorite": i.PaletteNoise(new[] { C(0xBDBDBD), C(0xD6D6D6), C(0x9E9E9E), C(0xECECEC) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i;
                case "polished_diorite": return Polished(i, C(0xC4C4C4));
                case "andesite": i.PaletteNoise(new[] { C(0x888888), C(0x777777), C(0x9A9A9A), C(0x6A6A6A) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i;
                case "polished_andesite": return Polished(i, C(0x848684));
                case "deepslate": return DeepslateBase(i);
                case "deepslate_top": i.Noise(C(0x505055), 0.08f); i.Clusters(C(0x404045), 8, 2, 4); return i;
                case "cobbled_deepslate": return Cobble(i, C(0x4E4E52), C(0x2E2E32));
                case "polished_deepslate": return Polished(i, C(0x48484C));
                case "deepslate_bricks": return StoneBricks(i, C(0x4A4A4E), C(0x2E2E32));
                case "cracked_deepslate_bricks": StoneBricks(i, C(0x484848), C(0x2E2E32)); i.Line(2, 2, 6, 8, C(0x202024)); i.Line(9, 5, 13, 13, C(0x202024)); return i;
                case "deepslate_tiles": i.Bricks(C(0x3E3E42), C(0x252528), 4, 4, 0.12f, false); return i;
                case "cracked_deepslate_tiles": i.Bricks(C(0x3E3E42), C(0x252528), 4, 4, 0.12f, false); i.Line(1, 3, 8, 9, C(0x18181A)); return i;
                case "chiseled_deepslate": i.Noise(C(0x46464A), 0.05f); i.RectOutline(0, 0, 16, 16, C(0x2A2A2E)); i.RectOutline(4, 4, 8, 8, C(0x2A2A2E)); i.Rect(6, 6, 4, 4, C(0x5A5A60)); return i;
                case "reinforced_deepslate_side": i.Noise(C(0x5A5E5A), 0.06f); i.RectOutline(0, 0, 16, 16, C(0x2E302E)); i.Rect(0, 5, 16, 6, C(0x3E423E)); i.HLine(5, 0, 15, C(0x8A9A8A)); return i;
                case "reinforced_deepslate_top": case "reinforced_deepslate_bottom": i.Noise(C(0x4E524E), 0.06f); i.RectOutline(0, 0, 16, 16, C(0x2E302E)); i.RectOutline(3, 3, 10, 10, C(0x8A9A8A)); return i;
                case "tuff": i.PaletteNoise(new[] { C(0x6C6D66), C(0x5E5F58), C(0x7A7B72), C(0x55564F) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i;
                case "polished_tuff": return Polished(i, C(0x626560));
                case "tuff_bricks": return StoneBricks(i, C(0x62655E), C(0x44463F));
                case "chiseled_tuff": i.Noise(C(0x62655E), 0.05f); i.RectOutline(0, 0, 16, 16, C(0x44463F)); i.Rect(0, 6, 16, 4, C(0x75786F)); return i;
                case "chiseled_tuff_bricks": StoneBricks(i, C(0x62655E), C(0x44463F)); i.Rect(5, 5, 6, 6, C(0x7A7D74)); return i;
                case "calcite": i.PaletteNoise(new[] { C(0xDFDFDA), C(0xEDEDE8), C(0xCFCFC8), C(0xF5F5F0) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i;
                case "dripstone_block": i.PaletteNoise(new[] { C(0x866B5C), C(0x75594B), C(0x987C6C), C(0x634B3F) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); for (int x = 0; x < 16; x += 3) i.VLine(x, 0, 15, Img.Shade(C(0x75594B), 0.9f)); return i;
                case "bricks": return i.Bricks(C(0x965A4A), C(0xA7A39A), 8, 4, 0.14f, true);
                case "mud": i.PaletteNoise(new[] { C(0x3C393D), C(0x333035), C(0x46434A) }, new[] { 0.45f, 0.35f, 0.2f }, 1); return i;
                case "packed_mud": i.PaletteNoise(new[] { C(0x8E6B50), C(0x7E5E45), C(0x9E7A5C) }, new[] { 0.4f, 0.35f, 0.25f }, 1); return i;
                case "mud_bricks": return i.Bricks(C(0x896649), C(0x644A34), 8, 4, 0.1f, true);
                case "dirt": return Dirt(i);
                case "coarse_dirt": Dirt(i); return i.Clusters(C(0x5A4028), 14, 1, 3).Clusters(C(0x9E8B78), 8, 1, 2);
                case "rooted_dirt": Dirt(i); return i.Clusters(C(0xB08A5E), 8, 2, 5);
                case "grass_block_top": return GrassTop(i);
                case "grass_block_side": return GrassSide(i, false);
                case "grass_block_snow": return GrassSide(i, true);
                case "podzol_top": i.PaletteNoise(new[] { C(0x5E4020), C(0x7A5226), C(0x4A3218), C(0x8C6030) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i;
                case "podzol_side": Dirt(i); for (int x = 0; x < 16; x++) { int d = 3 + (int)(Hash.Get(x, 7) % 2); for (int y = 0; y < d; y++) i[x, y] = Img.Shade(C(0x6A4822), 0.9f + (Hash.Get(x, y) % 20) / 100f); } return i;
                case "mycelium_top": i.PaletteNoise(new[] { C(0x6F6265), C(0x8A7B7E), C(0x5E5357), C(0x9C8C90) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i.Speckle(C(0xB4A6AE), 0.05f);
                case "mycelium_side": Dirt(i); for (int x = 0; x < 16; x++) { int d = 3 + (int)(Hash.Get(x, 9) % 2); for (int y = 0; y < d; y++) i[x, y] = Img.Shade(C(0x7A6C70), 0.9f + (Hash.Get(x, y) % 20) / 100f); } return i;
                case "dirt_path_top": i.PaletteNoise(new[] { C(0x9A7B45), C(0x8A6C3A), C(0xAA8B55) }, new[] { 0.4f, 0.35f, 0.25f }, 1); return i;
                case "dirt_path_side": Dirt(i); for (int x = 0; x < 16; x++) { i[x, 0] = C(0x00000000); i[x, 1] = C(0x9A7B45); i[x, 2] = C(0x8A6C3A); } return i;
                case "farmland": Dirt(i); for (int y = 0; y < 16; y += 4) i.HLine(y, 0, 15, C(0x5A3E22)); return i;
                case "farmland_moist": Dirt(i); i.Multiply(0.6f); for (int y = 0; y < 16; y += 4) i.HLine(y, 0, 15, C(0x2E1E10)); return i;
                case "moss_block": i.PaletteNoise(new[] { C(0x5C7A2E), C(0x4D6A26), C(0x6C8C38), C(0x42591F) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 2); return i;
                case "pale_moss_block": i.PaletteNoise(new[] { C(0x8E958A), C(0x7E857A), C(0xA0A79A), C(0x6E756A) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 2); return i;
                case "sand": i.PaletteNoise(new[] { C(0xDBCFA3), C(0xD2C492), C(0xE3D8B0), C(0xC9B985) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 0); return i;
                case "red_sand": i.PaletteNoise(new[] { C(0xBE6621), C(0xB05C1C), C(0xCA742E), C(0xA2521A) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 0); return i;
                case "gravel": i.PaletteNoise(new[] { C(0x857F7E), C(0x6E6867), C(0x9E9896), C(0x5A5453), C(0xB0A9A7) }, new[] { 0.3f, 0.25f, 0.2f, 0.15f, 0.1f }, 1); return i;
                case "clay": i.Noise(C(0xA2A7B4), 0.04f); return i.Clusters(C(0x959AA7), 6, 2, 4);
                case "snow": i.Noise(C(0xF2FAFA), 0.03f); return i.Speckle(C(0xE2EEF0), 0.1f);
                case "ice": i.Noise(C(0x92B6F4), 0.04f); i.SetAlpha(170); i.Line(2, 3, 5, 3, Img.WithA(C(0xC8DCFF), 200)); i.Line(9, 10, 13, 10, Img.WithA(C(0xC8DCFF), 200)); return i;
                case "packed_ice": i.Noise(C(0x8DB2F0), 0.05f); i.Line(1, 4, 6, 4, C(0xB8D0FA)); i.Line(8, 11, 14, 11, C(0xB8D0FA)); i.Line(4, 13, 7, 13, C(0x7A9CDA)); return i;
                case "blue_ice": i.Noise(C(0x74A7F4), 0.05f); i.Line(2, 2, 7, 2, C(0xA8C8FA)); i.Line(9, 12, 14, 12, C(0xA8C8FA)); return i;
                case "bedrock": i.PaletteNoise(new[] { C(0x575757), C(0x3A3A3A), C(0x707070), C(0x222222), C(0x8A8A8A) }, new[] { 0.3f, 0.25f, 0.2f, 0.15f, 0.1f }, 1); return i;
                case "obsidian": i.PaletteNoise(new[] { C(0x14121E), C(0x1E1A2E), C(0x0C0A12), C(0x2C2444) }, new[] { 0.4f, 0.3f, 0.2f, 0.1f }, 1); return i.Speckle(C(0x3A3058), 0.04f);
                case "crying_obsidian": Specific(i, "obsidian", 0); return i.Clusters(C(0x8A2AE8), 8, 1, 3).Speckle(C(0xC060FF), 0.03f);
                case "bone_block_side": i.Noise(C(0xE5E1CF), 0.03f); for (int x = 0; x < 16; x += 4) i.VLine(x, 0, 15, C(0xC9C4AE)); return i;
                case "bone_block_top": i.Noise(C(0xE5E1CF), 0.03f); i.RectOutline(2, 2, 12, 12, C(0xC9C4AE)); i.Rect(5, 5, 6, 6, C(0xC9C4AE)); return i;
                case "cobweb": i.Clear(); { var w = Img.WithA(C(0xEAEAEA), 220); i.Line(0, 0, 15, 15, w); i.Line(15, 0, 0, 15, w); i.VLine(8, 0, 15, w); i.HLine(8, 0, 15, w); i.RectOutline(4, 4, 8, 8, w); i.RectOutline(2, 2, 12, 12, w); } return i;
                // ------------------------------------------------ minerals/storage
                case "coal_block": return Mineral(i, C(0x1A1A1A), 2);
                case "iron_block": return Mineral(i, C(0xDCDCDC), 0);
                case "gold_block": return Mineral(i, C(0xF7D232), 0);
                case "diamond_block": return Mineral(i, C(0x62E6DD), 1);
                case "emerald_block": return Mineral(i, C(0x2BD66A), 1);
                case "lapis_block": return Mineral(i, C(0x1F43A8), 2);
                case "redstone_block": return Mineral(i, C(0xB01A0A), 2);
                case "netherite_block": return Mineral(i, C(0x42393C), 0);
                case "raw_iron_block": i.Noise(C(0xA88A6E), 0.1f); return i.Clusters(C(0xD2B294), 8, 2, 3);
                case "raw_copper_block": i.Noise(C(0x9A5A3A), 0.1f); return i.Clusters(C(0xD08A60), 8, 2, 3);
                case "raw_gold_block": i.Noise(C(0xD2A42A), 0.1f); return i.Clusters(C(0xF8E070), 8, 2, 3);
                case "amethyst_block": i.PaletteNoise(new[] { C(0x8A62C6), C(0x7A50B6), C(0xA27ED8), C(0x5E3A96) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i;
                case "budding_amethyst": Specific(i, "amethyst_block", 0); return i.Clusters(C(0x3A2466), 6, 1, 2);
                case "quartz_block_side": case "quartz_block_top": case "quartz_block_bottom": i.Noise(C(0xEBE5DE), 0.025f); return i;
                case "chiseled_quartz_block": i.Noise(C(0xEBE5DE), 0.025f); i.RectOutline(0, 0, 16, 16, C(0xC9C0B6)); i.RectOutline(3, 3, 10, 10, C(0xC9C0B6)); return i;
                case "chiseled_quartz_block_top": i.Noise(C(0xEBE5DE), 0.025f); i.RectOutline(0, 0, 16, 16, C(0xC9C0B6)); i.RectOutline(5, 5, 6, 6, C(0xC9C0B6)); return i;
                case "quartz_pillar": i.Noise(C(0xEBE5DE), 0.025f); i.VLine(0, 0, 15, C(0xC9C0B6)); i.VLine(15, 0, 15, C(0xC9C0B6)); i.VLine(5, 0, 15, C(0xD8D0C6)); i.VLine(10, 0, 15, C(0xD8D0C6)); return i;
                case "quartz_pillar_top": i.Noise(C(0xEBE5DE), 0.025f); i.RectOutline(0, 0, 16, 16, C(0xC9C0B6)); i.RectOutline(3, 3, 10, 10, C(0xD8D0C6)); return i;
                case "quartz_bricks": return i.Bricks(C(0xEAE4DC), C(0xC9C0B6), 8, 4, 0.04f, true);
                // ------------------------------------------------ sandstone
                case "sandstone": i.Noise(C(0xD8CB9B), 0.04f); i.HLine(3, 0, 15, C(0xC7B887)); i.HLine(9, 0, 15, C(0xC7B887)); i.HLine(15, 0, 15, C(0xBCAC7A)); return i;
                case "sandstone_top": i.Noise(C(0xDDD1A3), 0.035f); return i;
                case "sandstone_bottom": i.Noise(C(0xD8CB9B), 0.05f); return i.Speckle(C(0xC7B887), 0.1f);
                case "chiseled_sandstone": i.Noise(C(0xD8CB9B), 0.04f); i.RectOutline(0, 0, 16, 16, C(0xBCAC7A)); i.Rect(4, 4, 8, 8, C(0xC7B887)); i.Rect(6, 6, 4, 4, C(0xDDD1A3)); i.HLine(1, 0, 15, C(0xE5DAB0)); return i;
                case "cut_sandstone": i.Noise(C(0xD8CB9B), 0.03f); i.RectOutline(0, 0, 16, 8, C(0xC2B381)); i.RectOutline(0, 8, 16, 8, C(0xC2B381)); return i;
                case "red_sandstone": i.Noise(C(0xB8621F), 0.04f); i.HLine(3, 0, 15, C(0xA5561A)); i.HLine(9, 0, 15, C(0xA5561A)); i.HLine(15, 0, 15, C(0x984E17)); return i;
                case "red_sandstone_top": i.Noise(C(0xBE6723), 0.035f); return i;
                case "red_sandstone_bottom": i.Noise(C(0xB8621F), 0.05f); return i.Speckle(C(0xA5561A), 0.1f);
                case "chiseled_red_sandstone": i.Noise(C(0xB8621F), 0.04f); i.RectOutline(0, 0, 16, 16, C(0x984E17)); i.Rect(4, 4, 8, 8, C(0xA5561A)); i.Rect(6, 6, 4, 4, C(0xC87330)); return i;
                case "cut_red_sandstone": i.Noise(C(0xB8621F), 0.03f); i.RectOutline(0, 0, 16, 8, C(0x9E531A)); i.RectOutline(0, 8, 16, 8, C(0x9E531A)); return i;
                case "terracotta": return Terracotta(i, C(0x985E43));
                // ------------------------------------------------ prismarine / ocean
                case "prismarine":
                    {
                        float t = frame / 4f;
                        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++)
                            {
                                float v = ((Hash.Get(x / 2, y / 2) & 255) / 255f + t) % 1f;
                                i[x, y] = Img.Mix(C(0x5A9C8E), C(0x6FB7A6), v);
                            }
                        return i.Clusters(C(0x4A8A7A), 5, 2, 4);
                    }
                case "prismarine_bricks": return i.Bricks(C(0x63A99A), C(0x3F7E6E), 8, 8, 0.06f, false);
                case "dark_prismarine": i.Bricks(C(0x345C4C), C(0x264438), 8, 8, 0.06f, false); return i.RectOutline(0, 0, 16, 16, C(0x1E3A30));
                case "sea_lantern":
                    {
                        i.Noise(C(0xD4E6DE), 0.04f);
                        float t = frame / 5f * Mathf.PI * 2;
                        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) { float v = Mathf.Sin(x * 0.6f + t) * Mathf.Cos(y * 0.6f - t); if (v > 0.5f) i[x, y] = C(0xF4FFF8); }
                        i.RectOutline(0, 0, 16, 16, C(0xA8C8BC)); return i;
                    }
                case "sponge": i.Noise(C(0xC9C44A), 0.06f); for (int k = 0; k < 14; k++) { int x = i.rng.Next(15), y = i.rng.Next(15); i.Set(x, y, C(0x958F28)); i.Set(x + 1, y, C(0xA7A232)); } return i;
                case "wet_sponge": Specific(i, "sponge", 0); return i.Tint(C(0xA8A860)).Multiply(0.85f);
                // ------------------------------------------------ nether
                case "netherrack": i.PaletteNoise(new[] { C(0x6E2E2E), C(0x5A2222), C(0x823A38), C(0x4A1A1A), C(0x94443E) }, new[] { 0.3f, 0.25f, 0.2f, 0.15f, 0.1f }, 1); return i;
                case "soul_sand": i.PaletteNoise(new[] { C(0x523E30), C(0x463428), C(0x5E4A3A) }, new[] { 0.4f, 0.35f, 0.25f }, 1); for (int k = 0; k < 3; k++) { int x = 2 + k * 5, y = 3 + (k % 2) * 6; i.Set(x, y, C(0x2A1E16)); i.Set(x + 2, y, C(0x2A1E16)); i.HLine(y + 2, x, x + 2, C(0x2A1E16)); } return i;
                case "soul_soil": i.PaletteNoise(new[] { C(0x4C3A2E), C(0x3E2E24), C(0x5A4638) }, new[] { 0.4f, 0.35f, 0.25f }, 1); return i;
                case "basalt_side": for (int x = 0; x < 16; x++) for (int y = 0; y < 16; y++) i[x, y] = Img.Shade(C(0x505055), 0.85f + ((Hash.Get(x, y / 4) & 255) / 255f) * 0.25f); return i;
                case "basalt_top": i.Noise(C(0x57575C), 0.08f); i.RectOutline(1, 1, 14, 14, C(0x46464B)); return i;
                case "polished_basalt_side": for (int x = 0; x < 16; x++) for (int y = 0; y < 16; y++) i[x, y] = Img.Shade(C(0x5E5E63), x % 5 == 0 ? 0.85f : 1f); return i;
                case "polished_basalt_top": return Polished(i, C(0x5E5E63));
                case "smooth_basalt": i.Noise(C(0x48484D), 0.05f); return i;
                case "blackstone": i.PaletteNoise(new[] { C(0x2A2428), C(0x221D20), C(0x362E33), C(0x1A1618) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i;
                case "blackstone_top": i.Noise(C(0x2E282C), 0.07f); i.RectOutline(2, 2, 12, 12, C(0x221D20)); return i;
                case "polished_blackstone": return Polished(i, C(0x36303A));
                case "polished_blackstone_bricks": return i.Bricks(C(0x302A30), C(0x1C181C), 8, 4, 0.08f, true);
                case "cracked_polished_blackstone_bricks": i.Bricks(C(0x302A30), C(0x1C181C), 8, 4, 0.08f, true); i.Line(2, 1, 8, 8, C(0x121012)); return i;
                case "chiseled_polished_blackstone": Polished(i, C(0x36303A)); i.RectOutline(4, 3, 8, 10, C(0x1C181C)); i.Rect(6, 5, 4, 3, C(0x4A424A)); return i;
                case "gilded_blackstone": Specific(i, "blackstone", 0); return i.Clusters(C(0xF0B832), 7, 1, 3);
                case "glowstone": i.PaletteNoise(new[] { C(0xB88A3E), C(0xFFD27A), C(0xE0A850), C(0x8A5E2A), C(0xFFF0B8) }, new[] { 0.25f, 0.25f, 0.25f, 0.15f, 0.1f }, 1); return i;
                case "nether_bricks": return i.Bricks(C(0x3C1C22), C(0x1E0C10), 8, 4, 0.1f, true);
                case "cracked_nether_bricks": i.Bricks(C(0x3C1C22), C(0x1E0C10), 8, 4, 0.1f, true); i.Line(3, 2, 7, 9, C(0x120608)); return i;
                case "chiseled_nether_bricks": i.Noise(C(0x3C1C22), 0.06f); i.RectOutline(0, 0, 16, 16, C(0x1E0C10)); i.Rect(5, 4, 6, 8, C(0x2A1016)); return i;
                case "red_nether_bricks": return i.Bricks(C(0x5A0C0E), C(0x2E0406), 8, 4, 0.1f, true);
                case "nether_wart_block": i.PaletteNoise(new[] { C(0x7A0A0A), C(0x8E1414), C(0x640606), C(0xA22020) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i;
                case "warped_wart_block": i.PaletteNoise(new[] { C(0x167B7A), C(0x14938E), C(0x0E605E), C(0x22A8A0) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i;
                case "shroomlight": i.PaletteNoise(new[] { C(0xF09A48), C(0xFFC06A), C(0xD87A30), C(0xFFE0A0) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i;
                case "crimson_nylium": i.PaletteNoise(new[] { C(0x8A1E1E), C(0x9E2828), C(0x741414), C(0xB23434) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i;
                case "crimson_nylium_side": Specific(i, "netherrack", 0); for (int x = 0; x < 16; x++) { int d = 3 + (int)(Hash.Get(x, 3) % 3); for (int y = 0; y < d; y++) i[x, y] = Img.Shade(C(0x8A1E1E), 0.9f + (Hash.Get(x, y) % 20) / 100f); } return i;
                case "warped_nylium": i.PaletteNoise(new[] { C(0x2B7266), C(0x248A7C), C(0x1E5E54), C(0x36A290) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i;
                case "warped_nylium_side": Specific(i, "netherrack", 0); for (int x = 0; x < 16; x++) { int d = 3 + (int)(Hash.Get(x, 4) % 3); for (int y = 0; y < d; y++) i[x, y] = Img.Shade(C(0x2B7266), 0.9f + (Hash.Get(x, y) % 20) / 100f); } return i;
                case "crimson_roots": return Flower(i, new[] { "................", "................", "................", "................", "................", "....r.....r.....", "....r..r..r.....", ".r..rr.r.rr..r..", ".rr..r.rr.r..r..", "..r..rr.r.r.rr..", "..rr..r.r.rr.r..", "...r..rrr.r..r..", "...rr..rr.r.rr..", "....r..r..rr.r..", "....r..r...r.r..", "....r..r...r.r.." }, Pal('r', C(0xA8202A)));
                case "warped_roots": return Flower(i, new[] { "................", "................", "................", "................", "................", "....w.....w.....", "....w..w..w.....", ".w..ww.w.ww..w..", ".ww..w.ww.w..w..", "..w..ww.w.w.ww..", "..ww..w.w.ww.w..", "...w..www.w..w..", "...ww..ww.w.ww..", "....w..w..ww.w..", "....w..w...w.w..", "....w..w...w.w.." }, Pal('w', C(0x1E9C8E)));
                case "nether_sprouts": return Flower(i, new[] { "................", "................", "................", "................", "................", "................", "................", "................", "................", "...w......w.....", "...w..w...w..w..", "..ww..w..ww..w..", "..w..ww..w..ww..", ".ww..w..ww..w...", ".w..ww..w..ww...", ".w..w...w..w...." }, Pal('w', C(0x16A08E)));
                case "crimson_fungus": return Flower(i, new[] { "................", "................", "................", "................", "................", ".....rrrrr......", "....rRRRRRr.....", "...rRRyRRRRr....", "...rRRRRRyRr....", "....rrrrrrr.....", ".......s........", ".......s........", ".......s........", "......ss........", ".......s........", ".......s........" }, Pal('r', C(0x7A1414), 'R', C(0xAE2222), 'y', C(0xF0C040), 's', C(0xE8A8A0)));
                case "warped_fungus": return Flower(i, new[] { "................", "................", "................", "................", "................", ".....ttttt......", "....tTTTTTt.....", "...tTToTTTTt....", "...tTTTTToTt....", "....ttttttt.....", ".......s........", ".......s........", ".......s........", "......ss........", ".......s........", ".......s........" }, Pal('t', C(0x146E66), 'T', C(0x1FA094), 'o', C(0xF08A30), 's', C(0xD8C8B0)));
                case "weeping_vines": return Flower(i, new[] { ".......r........", ".......r........", "......rr........", "......r.........", "......rr........", ".......r........", ".......rr.......", "........r.......", "........r.......", ".......rr.......", ".......r........", "......rr........", "......r.........", "......rR........", ".......R........", "................" }, Pal('r', C(0x8E1414), 'R', C(0xC82020)));
                case "twisting_vines": return Flower(i, new[] { "................", ".......T........", ".......t........", "......tt........", "......t.........", "......tt........", ".......t........", ".......tt.......", "........t.......", "........t.......", ".......tt.......", ".......t........", "......tt........", "......t.........", "......tt........", ".......t........" }, Pal('t', C(0x14887A), 'T', C(0x24C0A8)));
                case "magma":
                    {
                        i.PaletteNoise(new[] { C(0x6A2008), C(0x8A300C), C(0x4A1604) }, new[] { 0.4f, 0.35f, 0.25f }, 1);
                        for (int k = 0; k < 7; k++)
                        {
                            int x = (int)(Hash.Get(k, 11) % 14) + 1, y = (int)(Hash.Get(k, 13) % 14) + 1;
                            bool on = (k + frame) % 3 != 0;
                            i.Set(x, y, on ? C(0xFFA82A) : C(0xC04A10)); i.Set(x + 1, y, on ? C(0xFFD060) : C(0xD86018)); i.Set(x, y + 1, C(0xC04A10));
                        }
                        return i;
                    }
                // ------------------------------------------------ end
                case "end_stone": i.PaletteNoise(new[] { C(0xDBDFA3), C(0xCFD393), C(0xE6E9B6), C(0xC2C688) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i.Clusters(C(0xB8BC7E), 5, 1, 3);
                case "end_stone_bricks": return i.Bricks(C(0xDCE0A6), C(0xB4B87E), 8, 4, 0.06f, true);
                case "purpur_block": i.Noise(C(0xA97CA9), 0.04f); for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) if (x % 8 == 0 || y % 8 == 0) i[x, y] = C(0x8C5E8C); return i;
                case "purpur_pillar": i.Noise(C(0xAB7FAB), 0.04f); for (int x = 0; x < 16; x += 4) i.VLine(x, 0, 15, C(0x8C5E8C)); return i;
                case "purpur_pillar_top": i.Noise(C(0xAB7FAB), 0.04f); i.RectOutline(0, 0, 16, 16, C(0x8C5E8C)); i.RectOutline(4, 4, 8, 8, C(0x8C5E8C)); return i;
                // ------------------------------------------------ glass & misc
                case "glass": i.Clear(); i.RectOutline(0, 0, 16, 16, C(0xDCEEF0)); i.Set(3, 3, C(0xFFFFFF)); i.Set(4, 3, C(0xFFFFFF)); i.Set(3, 4, C(0xFFFFFF)); i.Set(12, 11, C(0xE8F4F6)); i.Set(11, 12, C(0xE8F4F6)); i.Set(10, 13, C(0xE8F4F6)); return i;
                case "glass_pane_top": i.Fill(C(0xDCEEF0)); return i;
                case "tinted_glass": i.Fill(C(0x2A2230, 200)); i.RectOutline(0, 0, 16, 16, C(0x4A3E56, 240)); return i;
                case "iron_bars": i.Clear(); for (int x = 1; x < 16; x += 4) { i.VLine(x, 0, 15, C(0x8C8C8C)); i.VLine(x + 1, 0, 15, C(0x6E6E6E)); } i.HLine(1, 0, 15, C(0x7A7A7A)); i.HLine(14, 0, 15, C(0x7A7A7A)); return i;
                case "torch": return Torch(i, C(0xFFF6B0), C(0xFFB830), C(0x6A4A26));
                case "soul_torch": return Torch(i, C(0xC8FFFF), C(0x4ED8E0), C(0x6A4A26));
                case "copper_torch": return Torch(i, C(0xC8FFB0), C(0x62D870), C(0x6A4A26));
                case "redstone_torch": return Torch(i, C(0xFF6A5A), C(0xE01A0A), C(0x6A4A26));
                case "redstone_torch_off": return Torch(i, C(0x6A2A2A), C(0x4A1A1A), C(0x6A4A26), true);
                case "lantern": return Lantern(i, C(0x4E4E56), C(0x2E2E34), C(0xFFD27A));
                case "soul_lantern": return Lantern(i, C(0x4E4E56), C(0x2E2E34), C(0x7AE8F0));
                case "iron_chain": return Chain(i, C(0x5A5E6A), C(0x3A3E48));
                case "ladder": i.Clear(); { var w = C(0x8A6A3E); var d = C(0x5E4628); i.VLine(2, 0, 15, w); i.VLine(3, 0, 15, d); i.VLine(12, 0, 15, w); i.VLine(13, 0, 15, d); for (int y = 1; y < 16; y += 4) { i.HLine(y, 2, 13, w); i.HLine(y + 1, 2, 13, d); } } return i;
                case "vine": i.Clear(); for (int k = 0; k < 40; k++) { int x = i.rng.Next(16), y = i.rng.Next(16); var g = Img.Gray(120 + i.rng.Next(80)); i.Set(x, y, g); i.Set(x, y + 1, Img.Shade(g, 0.85f)); } for (int y = 0; y < 16; y++) if (y % 3 != 0) i.Set(5 + (y / 5) % 3, y, Img.Gray(110)); return i;
                case "glow_lichen": i.Clear(); for (int k = 0; k < 30; k++) { int x = i.rng.Next(16), y = i.rng.Next(16); i.Set(x, y, C(0x8AA890)); if (i.rng.Chance(0.3f)) i.Set(x + 1, y, C(0xD8F0C8)); } return i;
                case "water_still": return Liquid(i, true, false, frame);
                case "water_flow": return Liquid(i, true, true, frame);
                case "lava_still": return Liquid(i, false, false, frame);
                case "lava_flow": return Liquid(i, false, true, frame);
                case "nether_portal": return Portal(i, frame, false);
                case "end_portal": return Portal(i, frame, true);
                case "fire": return Fire(i, frame, false);
                case "soul_fire": return Fire(i, frame, true);
                // ------------------------------------------------ plants
                case "short_grass": i.Clear(); for (int k = 0; k < 9; k++) { int x = 1 + k * 2 - (k % 2); int h = 7 + (int)(Hash.Get(k, 5) % 8); for (int y = 16 - h; y < 16; y++) i.Set(x, y, Img.Gray(140 + (int)(Hash.Get(x, y) % 70))); if (i.rng.Chance(0.5f)) i.Set(x + 1, 16 - h + 2, Img.Gray(170)); } return i;
                case "fern": return Flower(i, new[] { "................", "................", ".......g........", "......gGg.......", "....g.gGg.g.....", "...gGg.G.gGg....", "..g.gGgGgGg.g...", ".gGg..gGg..gGg..", "..gGg.gGg.gGg...", "g..gGggGggGg..g.", "gGg..gGGGg..gGg.", ".gGg..gGg..gGg..", "..gGggGGGggGg...", "....ggGGGgg.....", "......gGg.......", ".......G........" }, Pal('g', Img.Gray(135), 'G', Img.Gray(175)));
                case "dead_bush": return Flower(i, new[] { "................", "................", "....b.....b.....", ".....b...b..b...", "..b...b.b..b....", "...b...bb.b.....", "....b..b.b...b..", "b....b.bbb..b...", ".b....bbb..b....", "..bb...bb.b.....", "....b..bbb......", ".....b.bb.......", "......bb........", ".......b........", ".......b........", ".......b........" }, Pal('b', C(0x946428)));
                case "bush": return Flower(i, new[] { "................", "................", "................", "................", "................", "......gGg.......", "....gGGgGGg.....", "...gGgGGGgGg....", "..gGGGgGgGGGg...", "..gGgGGGGGgGg...", ".gGGGgGgGgGGGg..", ".gGgGGGGGGGgGg..", "..gGGgGgGgGGg...", "...gGGGgGGGg....", ".....gGGGg......", "................" }, Pal('g', Img.Gray(120), 'G', Img.Gray(165)));
                case "tall_grass_bottom": i.Clear(); for (int k = 0; k < 8; k++) { int x = 1 + k * 2; for (int y = 0; y < 16; y++) i.Set(x + ((y / 5 + k) % 2), y, Img.Gray(140 + (int)(Hash.Get(x, y) % 60))); } return i;
                case "tall_grass_top": i.Clear(); for (int k = 0; k < 8; k++) { int x = 1 + k * 2; int top = 2 + (int)(Hash.Get(k, 3) % 9); for (int y = top; y < 16; y++) i.Set(x + ((y / 5 + k) % 2), y, Img.Gray(145 + (int)(Hash.Get(x, y) % 60))); } return i;
                case "large_fern_bottom": case "large_fern_top": Specific(i, "fern", 0); return i;
                case "sunflower_bottom": case "lilac_bottom": case "rose_bush_bottom": case "peony_bottom":
                    return Flower(i, new[] { ".....g..s..g....", "......gGsGg.....", "....g.GGsGG.g...", "...gGG.GsG.GGg..", "..gGGGG.s.GGGGg.", "...gGGGGsGGGGg..", ".....gGGsGGg....", ".......Gs.......", "......gGsG......", ".....gGGsGGg....", "....gGGGsGGGg...", ".....gGGsGGg....", "......gGsGg.....", "........s.......", "........s.......", "........s......." }, Pal('g', C(0x3C7A26), 'G', C(0x52A032), 's', C(0x467E2C)));
                case "sunflower_top": return Flower(i, new[] { "................", "................", "................", "................", "................", "................", "................", "................", "................", "................", ".......s........", ".......s........", ".......s........", ".......s........", ".......s........", ".......s........" }, Pal('s', C(0x467E2C)));
                case "sunflower_front": i.Clear(); i.Sprite(new[] { "....yyyyyyy.....", "...yYYYYYYYy....", "..yYYbbbbbYYy...", ".yYYbBBBBBbYYy..", ".yYbBBbBbBBbYy..", ".yYbBbBBBbBbYy..", ".yYbBBBBBBBbYy..", ".yYbBbBBBbBbYy..", ".yYbBBbBbBBbYy..", ".yYYbBBBBBbYYy..", "..yYYbbbbbYYy...", "...yYYYYYYYy....", "....yyyyyyy....." }, Pal('y', C(0xE0A814), 'Y', C(0xFFD02A), 'b', C(0x5A3A12), 'B', C(0x7A5018)), 1, 1); return i;
                case "lilac_top": return SimpleFlower(i, C(0xD48AD8), C(0xF0B8F0), C(0xA062A8), 3);
                case "rose_bush_top": return SimpleFlower(i, C(0xE02838), C(0xFF5A5A), C(0x9A1020), 0);
                case "peony_top": return SimpleFlower(i, C(0xF0B8E0), C(0xFFD8F0), C(0xC88AB8), 3);
                case "dandelion": return SimpleFlower(i, C(0xFFE02A), C(0xFFF28A), C(0xD8A812), 0);
                case "poppy": return SimpleFlower(i, C(0xE0181C), C(0x2A2A18), C(0x9A0A10), 0);
                case "blue_orchid": return SimpleFlower(i, C(0x3AB2F0), C(0x9AE0FF), C(0x1A70B0), 2);
                case "allium": return SimpleFlower(i, C(0xC878F0), C(0xE8B0FF), C(0x8A42B8), 3);
                case "azure_bluet": return SimpleFlower(i, C(0xE8F0F0), C(0xF0E070), C(0xB0C0C8), 3);
                case "red_tulip": return SimpleFlower(i, C(0xE0261C), C(0xE0261C), C(0x9A1210), 1);
                case "orange_tulip": return SimpleFlower(i, C(0xF28A1C), C(0xF28A1C), C(0xB05A10), 1);
                case "white_tulip": return SimpleFlower(i, C(0xF0F0F0), C(0xF0F0F0), C(0xB8C8C0), 1);
                case "pink_tulip": return SimpleFlower(i, C(0xF0A8D0), C(0xF0A8D0), C(0xC070A0), 1);
                case "oxeye_daisy": return SimpleFlower(i, C(0xF4F4F0), C(0xF8D020), C(0xC0C0B8), 2);
                case "cornflower": return SimpleFlower(i, C(0x4A6AF0), C(0x2A3AA0), C(0x2A40B8), 2);
                case "lily_of_the_valley": return SimpleFlower(i, C(0xF8F8F8), C(0xE8F0E0), C(0xC0D0B8), 3);
                case "wither_rose": return SimpleFlower(i, C(0x2A2A22), C(0x0E0E0A), C(0x1A1A14), 0);
                case "torchflower": return SimpleFlower(i, C(0xF0782A), C(0xFFD24A), C(0xB84A18), 1);
                case "open_eyeblossom": return SimpleFlower(i, C(0xF0A040), C(0xFFE8A0), C(0x8A5020), 2);
                case "closed_eyeblossom": return SimpleFlower(i, C(0x7A6A70), C(0x5A4A50), C(0x4A3A40), 1);
                case "pink_petals": i.Clear(); for (int k = 0; k < 10; k++) { int x = i.rng.Next(14), y = i.rng.Next(14); i.Set(x, y, C(0xF4B8D4)); i.Set(x + 1, y, C(0xF8C8E0)); i.Set(x, y + 1, C(0xE89AC0)); } return i;
                case "wildflowers": i.Clear(); for (int k = 0; k < 10; k++) { int x = i.rng.Next(14), y = i.rng.Next(14); var c = k % 3 == 0 ? C(0xF8E050) : (k % 3 == 1 ? C(0xF0F0F0) : C(0xB88AE8)); i.Set(x, y, c); i.Set(x + 1, y, Img.Shade(c, 0.9f)); i.Set(x, y + 1, C(0x5A9A32)); } return i;
                case "leaf_litter": i.Clear(); for (int k = 0; k < 14; k++) { int x = i.rng.Next(14), y = i.rng.Next(14); var c = k % 2 == 0 ? C(0xA06A2A) : C(0x7A4A1A); i.Set(x, y, c); i.Set(x + 1, y, c); i.Set(x, y + 1, Img.Shade(c, 0.85f)); } return i;
                case "brown_mushroom": return Flower(i, new[] { "................", "................", "................", "................", "................", "................", "................", "................", "................", "......bbbb......", ".....bBBBBb.....", ".....bBBBBb.....", "......wwww......", ".......ww.......", ".......ww.......", "................" }, Pal('b', C(0x7A5438), 'B', C(0x9A6E4C), 'w', C(0xD8CBB8)));
                case "red_mushroom": return Flower(i, new[] { "................", "................", "................", "................", "................", "................", "................", "................", "................", "......rrrr......", ".....rRwRRr.....", ".....rRRRwr.....", "......wwww......", ".......ww.......", ".......ww.......", "................" }, Pal('r', C(0xA81818), 'R', C(0xD82828), 'w', C(0xE8E0D8)));
                case "brown_mushroom_block": i.Noise(C(0x947A5E), 0.06f); return i.Clusters(C(0x7C644A), 6, 2, 4);
                case "red_mushroom_block": i.Noise(C(0xC42A28), 0.04f); return i.Clusters(C(0xE8E0D8), 5, 2, 4);
                case "mushroom_stem": i.Noise(C(0xD4CBB8), 0.04f); for (int x = 0; x < 16; x += 3) i.VLine(x, 0, 15, C(0xC4B8A2)); return i;
                case "sugar_cane": i.Clear(); for (int k = 0; k < 3; k++) { int x = 3 + k * 5; for (int y = 0; y < 16; y++) { i.Set(x, y, Img.Gray(y % 5 == 0 ? 190 : 150)); i.Set(x + 1, y, Img.Gray(y % 5 == 0 ? 170 : 125)); } i.Set(x + 2, 3 + k * 3, Img.Gray(140)); i.Set(x - 1, 9 + k, Img.Gray(140)); } return i;
                case "cactus_side": i.Noise(C(0x5A8A2A), 0.06f); for (int x = 1; x < 15; x += 3) i.VLine(x, 0, 15, C(0x4A7422)); for (int k = 0; k < 10; k++) i.Set(i.rng.Next(16), i.rng.Next(16), C(0xE8E0B0)); i.VLine(0, 0, 15, C(0x3A5E1A)); i.VLine(15, 0, 15, C(0x3A5E1A)); return i;
                case "cactus_top": i.Noise(C(0x6A9A34), 0.05f); i.RectOutline(0, 0, 16, 16, C(0x3A5E1A)); i.RectOutline(4, 4, 8, 8, C(0x5A8A2A)); return i;
                case "cactus_bottom": i.Noise(C(0x8AA85A), 0.05f); i.RectOutline(0, 0, 16, 16, C(0x3A5E1A)); return i;
                case "pumpkin_side": for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) i[x, y] = Img.Shade(C(0xE3901D), (x % 4 == 0 ? 0.82f : 1f) + (i.rng.NextFloat() - 0.5f) * 0.06f); return i;
                case "pumpkin_top": i.Noise(C(0xD8861A), 0.05f); i.Rect(6, 6, 4, 4, C(0x7A6428)); i.Rect(7, 7, 2, 2, C(0x5A4A1E)); return i;
                case "carved_pumpkin": Specific(i, "pumpkin_side", 0); i.Rect(3, 4, 3, 3, C(0x3A2208)); i.Rect(10, 4, 3, 3, C(0x3A2208)); i.Rect(3, 10, 10, 2, C(0x3A2208)); i.Rect(5, 12, 2, 1, C(0x3A2208)); i.Rect(9, 12, 2, 1, C(0x3A2208)); return i;
                case "jack_o_lantern": Specific(i, "pumpkin_side", 0); i.Rect(3, 4, 3, 3, C(0xFFD24A)); i.Rect(10, 4, 3, 3, C(0xFFD24A)); i.Rect(3, 10, 10, 2, C(0xFFD24A)); i.Rect(5, 12, 2, 1, C(0xFFD24A)); i.Rect(9, 12, 2, 1, C(0xFFD24A)); return i;
                case "melon_side": for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) i[x, y] = Img.Shade((x / 2) % 2 == 0 ? C(0x6E9A1E) : C(0x98B628), 1f + (i.rng.NextFloat() - 0.5f) * 0.08f); return i;
                case "melon_top": i.Noise(C(0x8AAE24), 0.06f); i.RectOutline(0, 0, 16, 16, C(0x5E8A18)); i.Rect(7, 7, 2, 2, C(0x6A5A20)); return i;
                case "hay_block_side": for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) i[x, y] = Img.Shade(C(0xC8A82E), ((x + y / 3) % 3 == 0 ? 0.85f : 1f) + (i.rng.NextFloat() - 0.5f) * 0.1f); i.HLine(3, 0, 15, C(0x8A5A1E)); i.HLine(12, 0, 15, C(0x8A5A1E)); return i;
                case "hay_block_top": i.Noise(C(0xC8A82E), 0.1f); return i.Speckle(C(0xA88A22), 0.2f);
                case "azalea_side": case "flowering_azalea_side": { Flower(i, new[] { "................", "................", "...gGGGgGGGg....", "..gGGgGGGgGGg...", "..gGGGGgGGGGg...", "..gGgGGGGGgGg...", "...gGGGgGGGg....", "....ggGGGgg.....", "......bbb.......", ".......b........", ".......b........", "......b.b.......", ".......b........", ".......b........", "................", "................" }, Pal('g', C(0x4A6A1E), 'G', C(0x6A8E2E), 'b', C(0x6A4A2A))); if (n.StartsWith("flowering")) i.Clusters(C(0xD86AC8), 5, 1, 2); return i; }
                case "azalea_leaves": Leaves(i, "azalea"); return i;
                case "flowering_azalea_leaves": Leaves(i, "azalea"); return i.Clusters(C(0xD86AC8), 6, 1, 3);
                case "mangrove_roots": i.Clear(); for (int k = 0; k < 6; k++) { int x = 1 + k * 3; for (int y = 0; y < 16; y++) i.Set(x + (y / 4 + k) % 2, y, C(0x4A3A22)); } i.HLine(4, 0, 15, C(0x5A4628)); i.HLine(11, 0, 15, C(0x5A4628)); return i;
                case "muddy_mangrove_roots_side": case "muddy_mangrove_roots_top": Specific(i, "mud", 0); for (int k = 0; k < 5; k++) { int x = 1 + k * 3; for (int y = 0; y < 16; y++) i.Set(x, y, C(0x4A3A22)); } return i;
                case "wheat_stage0": case "wheat_stage1": case "wheat_stage2": case "wheat_stage3": case "wheat_stage4": case "wheat_stage5": case "wheat_stage6": case "wheat_stage7":
                    return Crop(i, "wheat", n[n.Length - 1] - '0');
                case "carrots_stage0": case "carrots_stage1": case "carrots_stage2": case "carrots_stage3": return Crop(i, "carrots", n[n.Length - 1] - '0');
                case "potatoes_stage0": case "potatoes_stage1": case "potatoes_stage2": case "potatoes_stage3": return Crop(i, "potatoes", n[n.Length - 1] - '0');
                case "beetroots_stage0": case "beetroots_stage1": case "beetroots_stage2": case "beetroots_stage3": return Crop(i, "beetroots", n[n.Length - 1] - '0');
                // ------------------------------------------------ redstone
                case "redstone_dust_dot": i.Clear(); for (int y = 5; y < 11; y++) for (int x = 5; x < 11; x++) if (!((x == 5 || x == 10) && (y == 5 || y == 10))) i[x, y] = Img.Gray(200 + (int)(Hash.Get(x, y) % 55)); return i;
                case "redstone_dust_line": i.Clear(); for (int y = 0; y < 16; y++) for (int x = 6; x < 10; x++) i[x, y] = Img.Gray(200 + (int)(Hash.Get(x, y) % 55)); return i;
                case "redstone_lamp": i.Noise(C(0x6A4A2A), 0.06f); i.RectOutline(0, 0, 16, 16, C(0x3A2A18)); i.Rect(3, 3, 10, 10, C(0x8A5E34)); for (int k = 3; k < 13; k += 3) { i.HLine(k, 3, 12, C(0x5A3E22)); i.VLine(k, 3, 12, C(0x5A3E22)); } return i;
                case "redstone_lamp_on": i.Noise(C(0xB87838), 0.06f); i.RectOutline(0, 0, 16, 16, C(0x6A4A2A)); i.Rect(3, 3, 10, 10, C(0xFFD890)); for (int k = 3; k < 13; k += 3) { i.HLine(k, 3, 12, C(0xE8A860)); i.VLine(k, 3, 12, C(0xE8A860)); } return i;
                case "repeater": case "repeater_on": i.Noise(C(0xA0A0A0), 0.03f); i.RectOutline(0, 0, 16, 16, C(0x7A7A7A)); i.VLine(7, 2, 13, n.EndsWith("on") ? C(0xFF2A1A) : C(0x6A1A14)); i.VLine(8, 2, 13, n.EndsWith("on") ? C(0xE01A0A) : C(0x5A1410)); return i;
                case "comparator": case "comparator_on": i.Noise(C(0xA0A0A0), 0.03f); i.RectOutline(0, 0, 16, 16, C(0x7A7A7A)); { var rc = n.EndsWith("on") ? C(0xFF2A1A) : C(0x6A1A14); i.Line(4, 12, 8, 3, rc); i.Line(11, 12, 8, 3, rc); i.HLine(12, 4, 11, rc); } return i;
                case "observer_front": i.Noise(C(0x646464), 0.04f); i.RectOutline(0, 0, 16, 16, C(0x3A3A3A)); i.Rect(2, 4, 12, 8, C(0x2A2A2A)); i.Rect(3, 6, 4, 4, C(0x9A9A9A)); i.Rect(9, 6, 4, 4, C(0x9A9A9A)); i.Rect(4, 7, 2, 2, C(0x1A1A1A)); i.Rect(10, 7, 2, 2, C(0x1A1A1A)); return i;
                case "observer_back": i.Noise(C(0x646464), 0.04f); i.RectOutline(0, 0, 16, 16, C(0x3A3A3A)); i.Rect(6, 6, 4, 4, C(0x4A1A14)); return i;
                case "observer_back_on": i.Noise(C(0x646464), 0.04f); i.RectOutline(0, 0, 16, 16, C(0x3A3A3A)); i.Rect(6, 6, 4, 4, C(0xFF3A2A)); return i;
                case "observer_side": i.Noise(C(0x646464), 0.04f); i.RectOutline(0, 0, 16, 16, C(0x3A3A3A)); i.HLine(7, 1, 14, C(0x4A4A4A)); i.HLine(8, 1, 14, C(0x7A7A7A)); return i;
                case "observer_top": i.Noise(C(0x6A6A6A), 0.04f); i.RectOutline(0, 0, 16, 16, C(0x3A3A3A)); i.VLine(7, 1, 14, C(0x4A4A4A)); i.VLine(8, 1, 14, C(0x7A7A7A)); return i;
                case "piston_top": i.Planks(C(0xA08050), C(0x6A5030)); i.RectOutline(0, 0, 16, 16, C(0x5A5A5A)); i.RectOutline(1, 1, 14, 14, C(0x8A8A8A)); return i;
                case "piston_top_sticky": Specific(i, "piston_top", 0); i.Rect(2, 2, 12, 12, C(0x6AB84A)); i.Clusters(C(0x8AD86A), 5, 1, 3); return i;
                case "piston_side": i.Noise(C(0x7A7A7A), 0.06f); { var wd = new Img(16, 16, 5).Planks(C(0xA08050), C(0x6A5030)); for (int y = 0; y < 4; y++) for (int x = 0; x < 16; x++) i[x, y] = wd[x, y]; } i.HLine(4, 0, 15, C(0x4A4A4A)); i.Rect(6, 8, 4, 8, C(0x9A9A9A)); return i;
                case "piston_bottom": Cobble(i, C(0x7A7A7A), C(0x505050)); i.RectOutline(0, 0, 16, 16, C(0x4A4A4A)); return i;
                case "piston_inner": i.Noise(C(0x6A6A6A), 0.06f); i.Rect(6, 6, 4, 4, C(0x9A9A9A)); return i;
                case "daylight_detector_top": case "daylight_detector_inverted_top": i.Noise(n.Contains("inverted") ? C(0x4A5A8A) : C(0xE0D8B8), 0.04f); i.RectOutline(0, 0, 16, 16, C(0x6A5030)); for (int x = 2; x < 14; x += 3) i.VLine(x, 2, 13, C(0x8A8070)); return i;
                case "daylight_detector_side": i.Planks(C(0xA08050), C(0x6A5030)); i.HLine(0, 0, 15, C(0xC8C0A0)); return i;
                case "target_side": i.Noise(C(0xE8DACA), 0.04f); i.RectOutline(2, 2, 12, 12, C(0xD02020)); i.RectOutline(5, 5, 6, 6, C(0xD02020)); i.Rect(7, 7, 2, 2, C(0xD02020)); return i;
                case "target_top": Specific(i, "target_side", 0); return i;
                case "note_block": i.Planks(C(0x6A4A30), C(0x4A3220)); i.RectOutline(0, 0, 16, 16, C(0x3A2818)); i.Rect(5, 4, 2, 6, C(0x2A1A10)); i.Rect(3, 9, 4, 3, C(0x2A1A10)); i.Rect(10, 3, 2, 6, C(0x2A1A10)); i.Rect(8, 8, 4, 3, C(0x2A1A10)); i.HLine(3, 5, 11, C(0x2A1A10)); return i;
                case "tnt_side": for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) i[x, y] = Img.Shade(C(0xD83A2A), (x % 4 == 0 ? 0.8f : 1f) + (i.rng.NextFloat() - 0.5f) * 0.06f); i.Rect(0, 5, 16, 6, C(0xE8E0D0)); i.Sprite(new[] { "ttt.n..n.ttt", ".t..nn.n..t.", ".t..n.nn..t.", ".t..n..n..t." }, Pal('t', C(0x1A1A1A), 'n', C(0x1A1A1A)), 2, 6); return i;
                case "tnt_top": i.Noise(C(0xB8423A), 0.05f); i.RectOutline(0, 0, 16, 16, C(0x8A2A20)); i.Rect(6, 6, 4, 4, C(0x3A3A3A)); i.Rect(7, 7, 2, 2, C(0x6A6A6A)); return i;
                case "tnt_bottom": i.Noise(C(0xB8423A), 0.05f); i.RectOutline(0, 0, 16, 16, C(0x8A2A20)); return i;
                case "lever": i.Clear(); for (int y = 6; y < 16; y++) { i.Set(7, y, C(0x8A6A3E)); i.Set(8, y, C(0x6A4E2A)); } i.Set(7, 6, C(0xA08050)); i.Set(8, 6, C(0xA08050)); return i;
                // ------------------------------------------------ 26.2 sulfur caves
                case "sulfur": i.PaletteNoise(new[] { C(0xCFC23A), C(0xB8AC2A), C(0xE0D65A), C(0xA89A24) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i.Clusters(C(0xEEE88A), 4, 1, 3);
                case "potent_sulfur": i.PaletteNoise(new[] { C(0xB8C83A), C(0xA2B22A), C(0xD0E05A), C(0x8A9A20) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); return i.Clusters(C(0xF0FF8A), 7, 1, 3);
                case "cinnabar": i.PaletteNoise(new[] { C(0xB23A30), C(0x9A2E26), C(0xC8504A), C(0x7A2420) }, new[] { 0.35f, 0.3f, 0.2f, 0.15f }, 1); for (int y = 2; y < 16; y += 5) i.HLine(y, 0, 15, C(0x8A2A22)); return i;
                case "polished_sulfur": return Polished(i, C(0xC8BC38));
                case "polished_cinnabar": return Polished(i, C(0xAE3A30));
                case "sulfur_bricks": return i.Bricks(C(0xC8BC38), C(0x8A8022), 8, 4, 0.08f, true);
                case "cinnabar_bricks": return i.Bricks(C(0xA8362E), C(0x6A201A), 8, 4, 0.08f, true);
                case "chiseled_sulfur": Polished(i, C(0xC8BC38)); i.RectOutline(4, 4, 8, 8, C(0x8A8022)); return i;
                case "chiseled_cinnabar": Polished(i, C(0xAE3A30)); i.RectOutline(4, 4, 8, 8, C(0x6A201A)); return i;
                case "sulfur_spike": return Flower(i, new[] { "................", ".......y........", ".......y........", "......yY........", "......yY........", "......yYy.......", ".....yYYy.......", ".....yYYy.......", ".....yYYYy......", "....yYYYYy......", "....yYYYYy......", "....yYYYYYy.....", "...yYYYYYYy.....", "...yYYYYYYYy....", "..yYYYYYYYYYy...", "..yyyyyyyyyyy..." }, Pal('y', C(0xA89A24), 'Y', C(0xD8CC48)));
            }
            return MoreTextures(i, n, frame);
        }
    }
}
