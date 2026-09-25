using System;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Plant and organic textures: bookshelves, stems and crops, bamboo, chorus, underwater plants, hanging
    /// plants, dripleaf, cave vines, dripstone, amethyst buds, campfire logs and the creaking heart.
    /// Cross-model plants are drawn on a transparent tile, bottom-anchored, like the flowers in TextureGen2.cs.
    /// </summary>
    public static partial class TextureGen
    {
        static Img Plants(Img i, string n)
        {
            switch (n)
            {
                case "bookshelf": return Bookshelf(i);
                case "chiseled_bookshelf_front": return ChiseledBookshelf(i, "front");
                case "chiseled_bookshelf_side": return ChiseledBookshelf(i, "side");
                case "chiseled_bookshelf_top": return ChiseledBookshelf(i, "top");
                case "ancient_debris_side": return AncientDebris(i, false);
                case "ancient_debris_top": return AncientDebris(i, true);
                case "melon_stem": return StemTex(i, false);
                case "pumpkin_stem": return StemTex(i, false);
                case "melon_stem_attached": return StemTex(i, true);
                case "pumpkin_stem_attached": return StemTex(i, true);
                case "bamboo_stalk": return BambooStalk(i);
                case "bamboo_large_leaves": return BambooLeaves(i, true);
                case "bamboo_small_leaves": return BambooLeaves(i, false);
                case "bamboo_stage0": return BambooShoot(i);
                case "chorus_plant": return ChorusBody(i);
                case "chorus_flower": return ChorusFlower(i, false);
                case "chorus_flower_dead": return ChorusFlower(i, true);
                case "seagrass": return Seagrass(i, 0);
                case "tall_seagrass_bottom": return Seagrass(i, 1);
                case "tall_seagrass_top": return Seagrass(i, 2);
                case "kelp": return Kelp(i, true);
                case "kelp_plant": return Kelp(i, false);
                case "dried_kelp_side": return DriedKelp(i, false);
                case "dried_kelp_top": return DriedKelp(i, true);
                case "sea_pickle": return SeaPickle(i);
                case "hanging_roots": return HangingRoots(i);
                case "pale_hanging_moss": return PaleHangingMoss(i);
                case "spore_blossom": return SporeBlossom(i);
                case "spore_blossom_base": return SporeBlossomBase(i);
                case "big_dripleaf_top": return BigDripleafTop(i);
                case "big_dripleaf_stem": return DripleafStem(i, true);
                case "small_dripleaf_top": return SmallDripleaf(i);
                case "small_dripleaf_stem": return DripleafStem(i, false);
                case "cave_vines": return CaveVines(i, false, true);
                case "cave_vines_lit": return CaveVines(i, true, true);
                case "cave_vines_plant": return CaveVines(i, false, false);
                case "cave_vines_plant_lit": return CaveVines(i, true, false);
                case "powder_snow": return PowderSnow(i);
                case "pointed_dripstone": return PointedDripstone(i, false);
                case "pointed_dripstone_up": return PointedDripstone(i, false);
                case "pointed_dripstone_down": return PointedDripstone(i, true);
                case "amethyst_cluster": return Amethyst(i, 3);
                case "large_amethyst_bud": return Amethyst(i, 2);
                case "medium_amethyst_bud": return Amethyst(i, 1);
                case "small_amethyst_bud": return Amethyst(i, 0);
                case "campfire_log": return CampfireLog(i, false, false);
                case "campfire_log_lit": return CampfireLog(i, true, false);
                case "soul_campfire_log_lit": return CampfireLog(i, true, true);
                case "creaking_heart": return CreakingHeart(i, false);
                case "creaking_heart_top": return CreakingHeart(i, true);
            }
            if (n.StartsWith("nether_wart_stage", StringComparison.Ordinal)) return NetherWart(i, n[n.Length - 1] - '0');
            if (n.StartsWith("cocoa_stage", StringComparison.Ordinal)) return Cocoa(i, n[n.Length - 1] - '0');
            if (n.StartsWith("sweet_berry_bush_stage", StringComparison.Ordinal)) return SweetBerries(i, n[n.Length - 1] - '0');
            return null;
        }

        // ==================================================================== blocks

        static Img PowderSnow(Img i)
        {
            i.PaletteNoise(new[] { C(0xF0F8FA), C(0xE4EEF3), C(0xFBFEFF), C(0xD6E2EA) }, new[] { 0.34f, 0.26f, 0.24f, 0.16f }, 1);
            for (int k = 0; k < 10; k++) i.Set(RndInt(16), RndInt(16), C(0xFFFFFF));
            for (int k = 0; k < 6; k++) i.Set(RndInt(16), RndInt(16), C(0xC8D6E0));
            return i;
        }

        static Img AncientDebris(Img i, bool top)
        {
            i.PaletteNoise(new[] { C(0x5E463A), C(0x4C372C), C(0x6E5445), C(0x3C2A22) }, new[] { 0.34f, 0.28f, 0.22f, 0.16f }, 1);
            if (top)
            {
                // concentric growth rings with metallic knots
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++)
                    {
                        int d = Math.Max(Math.Abs(x * 2 - 15), Math.Abs(y * 2 - 15)) / 2;
                        if (d % 3 == 1) i[x, y] = Img.Shade(i[x, y], 0.8f);
                    }
                for (int k = 0; k < 5; k++)
                {
                    int x = 2 + RndInt(12), y = 2 + RndInt(12);
                    i.Rect(x, y, 2, 2, C(0x8E5E4C)); i.Set(x, y, C(0xB08070)); i.Set(x + 1, y + 1, C(0x5A3A2E));
                }
            }
            else
            {
                // vertical fibres running up the side
                for (int x = 0; x < 16; x += 3)
                    for (int y = 0; y < 16; y++)
                        if ((Hash.Get(x, y / 3, 13) & 3) != 0) i[x, y] = Img.Shade(i[x, y], 0.82f);
                for (int k = 0; k < 6; k++)
                {
                    int x = RndInt(15), y = RndInt(15);
                    i.Set(x, y, C(0x9A6452)); i.Set(x, y + 1, C(0x7A4A3A));
                }
            }
            i.RectOutline(0, 0, 16, 16, C(0x3A2820));
            return i;
        }

        static Img Bookshelf(Img i)
        {
            var oak = Wood("oak");
            var spines = new[] { C(0xA83A2E), C(0x2E5AA8), C(0x3A8A3A), C(0xC8A020), C(0x8A3A9A), C(0xC06A2A), C(0x2A8A8A), C(0x6A4A2A) };
            WoodBody(i, oak);
            // two shelves of books: rows 1-6 and 9-14, plank shelves between
            for (int s = 0; s < 2; s++)
            {
                int y0 = 1 + s * 8;
                for (int y = y0; y < y0 + 6; y++) for (int x = 1; x < 15; x++) i[x, y] = C(0x3A2A18);
                int x0 = 1;
                int bookIdx = s * 5;
                while (x0 < 15)
                {
                    int w = 1 + (int)(Hash.Get(bookIdx, 7) % 2);
                    int h = 4 + (int)(Hash.Get(bookIdx, 9) % 3);
                    var c = spines[Hash.Get(bookIdx, 11) % (uint)spines.Length];
                    for (int x = x0; x < Math.Min(15, x0 + w); x++)
                        for (int y = y0 + 6 - h; y < y0 + 6; y++)
                        {
                            float f = y == y0 + 6 - h ? 1.2f : (x == x0 ? 1.06f : 0.92f);
                            i[x, y] = Cl(c, f, 0.06f);
                        }
                    if ((Hash.Get(bookIdx, 13) & 1) == 0) i.Set(x0, y0 + 6 - h + 1, C(0xE8C050));
                    x0 += w + ((Hash.Get(bookIdx, 17) % 5) == 0 ? 1 : 0);
                    bookIdx++;
                }
            }
            i.HLine(0, 0, 15, Img.Shade(oak.plank, 1.1f));
            i.HLine(7, 0, 15, Img.Shade(oak.plank, 1.08f)); i.HLine(8, 0, 15, oak.plankDark);
            i.HLine(15, 0, 15, Img.Shade(oak.plankDark, 0.85f));
            i.VLine(0, 0, 15, oak.plankDark); i.VLine(15, 0, 15, oak.plankDark);
            return i;
        }

        static Img ChiseledBookshelf(Img i, string face)
        {
            var oak = Wood("oak");
            if (face == "top")
            {
                WoodBody(i, oak, 0.94f);
                i.RectOutline(0, 0, 16, 16, Img.Shade(oak.plankDark, 0.8f));
                i.RectOutline(1, 1, 14, 14, Img.Shade(oak.plank, 1.08f));
                return i;
            }
            if (face == "side")
            {
                WoodBody(i, oak);
                i.RectOutline(0, 0, 16, 16, Img.Shade(oak.plankDark, 0.8f));
                i.HLine(7, 1, 14, Img.Shade(oak.plankDark, 1.05f));
                i.HLine(8, 1, 14, Img.Shade(oak.plank, 1.1f));
                return i;
            }
            // front: 2 rows x 3 slots, some with books
            WoodBody(i, oak, 0.9f);
            i.RectOutline(0, 0, 16, 16, Img.Shade(oak.plankDark, 0.8f));
            var spines = new[] { C(0xA83A2E), C(0x2E5AA8), C(0x3A8A3A), C(0xC8A020), C(0x8A3A9A), C(0xC06A2A) };
            for (int row = 0; row < 2; row++)
                for (int col = 0; col < 3; col++)
                {
                    int x = 1 + col * 5, y = 1 + row * 7;
                    i.Rect(x, y, 4, 6, C(0x2E2014));
                    i.RectOutline(x, y, 4, 6, Img.Shade(oak.plankDark, 0.9f));
                    bool filled = ((row * 3 + col) * 5 + 1) % 3 != 0;
                    if (!filled) continue;
                    for (int b = 0; b < 2; b++)
                    {
                        var c = spines[(row * 3 + col + b * 2) % spines.Length];
                        for (int yy = y + 1; yy < y + 5; yy++) i[x + 1 + b, yy] = Cl(c, yy == y + 1 ? 1.2f : 1f, 0.05f);
                    }
                }
            return i;
        }

        // ==================================================================== crops & stems

        /// <summary>
        /// Stems are tinted by the block (TintType.Stem / the attached colour), so they are drawn in light greys;
        /// the growing stem uses a cross model, the attached stem a flat quad bending toward the fruit.
        /// </summary>
        static Img StemTex(Img i, bool attached)
        {
            i.Clear();
            Color32 a = Img.Gray(200), b = Img.Gray(160), c = Img.Gray(232);
            if (!attached)
            {
                for (int y = 2; y < 16; y++)
                {
                    int x = 7 + ((y / 3) & 1);
                    i.Set(x, y, (y & 1) == 0 ? a : b);
                }
                i.Set(6, 5, c); i.Set(5, 4, a); i.Set(9, 8, c); i.Set(10, 7, a); i.Set(6, 11, c); i.Set(5, 10, a);
                i.Set(7, 1, c);
                return i;
            }
            // attached: the stalk rises then arcs sideways toward the fruit (to the right)
            for (int y = 7; y < 16; y++) i.Set(3, y, (y & 1) == 0 ? a : b);
            for (int k = 0; k < 10; k++)
            {
                int x = 3 + k, y = 7 - Mathf.RoundToInt(Mathf.Sin(k / 9f * Mathf.PI) * 3f);
                i.Set(x, y, (k & 1) == 0 ? a : b);
            }
            i.Set(13, 7, c); i.Set(14, 8, c);
            i.Set(2, 12, c); i.Set(4, 10, c);
            return i;
        }

        /// <summary>Nether wart: lumpy red stalks growing up from the soul sand (crop "#" model).</summary>
        static Img NetherWart(Img i, int stage)
        {
            i.Clear();
            var body = C(0x7A1E22);
            var light = C(0xA82A2E);
            var hi = C(0xC8443E);
            var dark = C(0x4E1014);
            int h = 4 + stage * 4;               // 4, 8, 12 px tall
            int stalks = 3 + stage;              // more stalks as it matures
            for (int k = 0; k < stalks; k++)
            {
                int x = 1 + k * (14 / stalks) + (int)(Hash.Get(k, stage + 17) % 2);
                int top = 16 - h + (int)(Hash.Get(k * 3, stage) % 3);
                for (int y = top; y < 16; y++)
                {
                    i.Set(x, y, body);
                    i.Set(x + 1, y, dark);
                    if ((y - top) % 3 == 1) { i.Set(x - 1, y, light); i.Set(x + 2, y, dark); }
                }
                i.Set(x, top - 1, light); i.Set(x + 1, top - 1, body);
                i.Set(x, top, hi);
                if (stage == 2) { i.Set(x - 1, top, light); i.Set(x + 2, top, body); i.Set(x, top - 2, body); }
            }
            return i;
        }

        /// <summary>
        /// Cocoa pod: CocoaBlock builds a small box that samples sub-windows around the tile centre, so the
        /// pod skin covers the whole tile (ridged, with the stalk nub at the top centre).
        /// </summary>
        static Img Cocoa(Img i, int stage)
        {
            Color32 baseC = stage == 0 ? C(0x6E9A36) : (stage == 1 ? C(0xB0742A) : C(0xB8661E));
            Color32 light = stage == 0 ? C(0x8EBA4E) : (stage == 1 ? C(0xD09A4A) : C(0xDA8A34));
            Color32 dark = stage == 0 ? C(0x4A6E20) : (stage == 1 ? C(0x7A4A18) : C(0x7A3A10));
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                    i[x, y] = Cl(baseC, 1f, 0.08f);
            for (int x = 1; x < 16; x += 3) i.VLine(x, 0, 15, dark);
            for (int x = 0; x < 16; x += 3) i.VLine(x, 0, 15, light);
            i.Rect(7, 0, 2, 3, C(0x6A4A26));
            i.Set(7, 0, C(0x8A6436));
            return i;
        }

        static Img SweetBerries(Img i, int stage)
        {
            i.Clear();
            Color32 stem = C(0x3E6A22), leaf = C(0x4E8A2E), leafL = C(0x62A43A);
            int top = stage == 0 ? 8 : 3;
            for (int y = top; y < 16; y++)
            {
                int spread = Math.Min(7, (y - top) / 2 + 2);
                for (int x = 8 - spread; x < 8 + spread; x++)
                {
                    if (Rnd01() < 0.24f) continue;
                    i[x, y] = Rnd01() < 0.35f ? leafL : (Rnd01() < 0.5f ? leaf : stem);
                }
            }
            int berries = stage == 0 ? 0 : (stage == 1 ? 3 : (stage == 2 ? 5 : 8));
            Color32 berry = stage == 1 ? C(0x6E9A36) : C(0xB0202C), berryHi = stage == 1 ? C(0x9ACA5A) : C(0xE0404A);
            for (int k = 0; k < berries; k++)
            {
                int x = 3 + (int)(Hash.Get(k, 3) % 10), y = top + 3 + (int)(Hash.Get(k, 5) % (uint)Math.Max(1, 12 - top));
                i.Set(x, y, berry); i.Set(x + 1, y, berry); i.Set(x, y + 1, Img.Shade(berry, 0.7f)); i.Set(x, y, berryHi);
            }
            return i;
        }

        // ==================================================================== bamboo

        /// <summary>
        /// Atlas layout expected by BambooBlock: the culm sides sample cols 0-2 (uv 0-3/16), the top/bottom
        /// sample cols 13-15 rows 13-15. The rest repeats the culm so particles look right.
        /// </summary>
        static Img BambooStalk(Img i)
        {
            var p = Wood("bamboo");
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    int c = x % 3;
                    float f = c == 0 ? 1.12f : (c == 1 ? 1f : 0.82f);
                    i[x, y] = Cl(p.bark, f, 0.05f);
                }
            foreach (int y in new[] { 4, 12 })
                for (int x = 0; x < 16; x++) { i[x, y] = Img.Shade(p.barkDark, 1.1f); i[x, y + 1] = Img.Shade(p.bark, 1.22f); }
            for (int y = 13; y < 16; y++) for (int x = 13; x < 16; x++) i[x, y] = C(0xC8C07A);
            i.Set(14, 14, C(0xE0D898));
            return i;
        }

        static Img BambooLeaves(Img i, bool large)
        {
            i.Clear();
            Color32 leaf = C(0x5A9A34), dark = C(0x3A6A1E), light = C(0x7ABA4A);
            int count = large ? 6 : 3;
            for (int k = 0; k < count; k++)
            {
                float a = (k / (float)count) * Mathf.PI * 2f + 0.4f;
                int cx = 8, cy = 3 + (k & 1) * 4;
                int len = large ? 7 : 4;
                for (int s = 1; s <= len; s++)
                {
                    int x = cx + Mathf.RoundToInt(Mathf.Cos(a) * s), y = cy + Mathf.RoundToInt(Mathf.Sin(a) * s * 0.6f + s * 0.35f);
                    i.Set(x, y, s < 2 ? dark : leaf);
                    if (s > 1 && s < len) i.Set(x, y + 1, s % 2 == 0 ? light : dark);
                }
            }
            return i;
        }

        static Img BambooShoot(Img i)
        {
            i.Clear();
            var p = Wood("bamboo");
            for (int y = 9; y < 16; y++) { i.Set(7, y, Img.Shade(p.bark, 1.08f)); i.Set(8, y, Img.Shade(p.bark, 0.82f)); }
            i.Set(7, 8, Img.Shade(p.bark, 1.3f));
            i.Set(6, 10, C(0x6EAA3E)); i.Set(5, 9, C(0x6EAA3E)); i.Set(4, 9, C(0x4E8A2E));
            i.Set(9, 12, C(0x6EAA3E)); i.Set(10, 11, C(0x6EAA3E)); i.Set(11, 11, C(0x4E8A2E));
            i.HLine(13, 7, 8, Img.Shade(p.barkDark, 1.1f));
            return i;
        }

        // ==================================================================== chorus

        static Img ChorusBody(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float f = 0.9f + ((Hash.Get(x, y >> 1, 3) & 255) / 255f) * 0.24f;
                    if ((Hash.Get(x >> 1, y >> 1, 5) & 3) == 0) f *= 0.84f;
                    i[x, y] = Img.Shade(C(0x5E4A66), f);
                }
            for (int k = 0; k < 6; k++) Blob(i, RndInt(16), RndInt(16), 1, C(0x7E6A8A), 0.6f);
            for (int k = 0; k < 5; k++) Blob(i, RndInt(16), RndInt(16), 1, C(0x3E2E44), 0.6f);
            i.RectOutline(0, 0, 16, 16, C(0x3A2A42));
            return i;
        }

        static Img ChorusFlower(Img i, bool dead)
        {
            Color32 petal = dead ? C(0x6A5A64) : C(0xB48AC0);
            Color32 petalHi = dead ? C(0x7E6E78) : C(0xD8B4E2);
            Color32 petalLo = dead ? C(0x4A3E46) : C(0x7E5A8A);
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(petalLo, 1f, 0.08f);
            for (int k = 0; k < 4; k++)
            {
                int ox = 1 + (k % 2) * 7, oy = 1 + (k / 2) * 7;
                for (int y = 0; y < 7; y++)
                    for (int x = 0; x < 7; x++)
                    {
                        float d = (x - 3f) * (x - 3f) + (y - 3f) * (y - 3f);
                        if (d > 9.5f) continue;
                        i[ox + x, oy + y] = Cl(d < 2f ? petalHi : petal, d > 6f ? 0.86f : 1.02f, 0.05f);
                    }
            }
            i.Rect(6, 6, 4, 4, dead ? C(0x3A3036) : C(0x5E3E6A));
            i.Set(7, 7, dead ? C(0x564A50) : C(0xE8C8F0));
            i.RectOutline(0, 0, 16, 16, Img.Shade(petalLo, 0.8f));
            return i;
        }

        // ==================================================================== underwater

        /// <summary>mode 0 = short seagrass, 1 = tall bottom half (reaches the top edge), 2 = tall top half.</summary>
        static Img Seagrass(Img i, int mode)
        {
            i.Clear();
            Color32 a = C(0x2E7A22), b = C(0x4AA82E), c = C(0x1E5A18);
            for (int k = 0; k < 6; k++)
            {
                int x = 1 + k * 3 - (k & 1);
                int top = mode == 1 ? 0 : (mode == 2 ? 4 + (int)(Hash.Get(k, 31) % 8) : 3 + (int)(Hash.Get(k, 29) % 6));
                for (int y = top; y <= 15; y++)
                {
                    int xx = x + (((y + k * 2) / 5) & 1);
                    i.Set(xx, y, ((y + k) % 4 == 0) ? b : a);
                    if ((y + k) % 6 == 0) i.Set(xx + 1, y, c);
                }
                i.Set(x, top, b);
            }
            return i;
        }

        /// <summary>Kelp: a central stem with alternating fronds; the top piece ends in a bud.</summary>
        static Img Kelp(Img i, bool top)
        {
            i.Clear();
            Color32 stem = C(0x2E6E1E), stemD = C(0x225216), leaf = C(0x3E8A28), leafL = C(0x56A836), bladder = C(0x6A9A2A);
            int y0 = top ? 3 : 0;
            for (int y = y0; y < 16; y++) { i.Set(7, y, stem); i.Set(8, y, stemD); }
            for (int k = 0; k < 6; k++)
            {
                int y = y0 + k * 3 + 1;
                if (y > 15) break;
                bool right = (k & 1) == 0;
                for (int t = 1; t <= 4; t++)
                {
                    int x = right ? 8 + t : 7 - t;
                    int yy = y - (t > 2 ? 1 : 0);
                    i.Set(x, yy, t == 4 ? leafL : leaf);
                    i.Set(x, yy + 1, Img.Shade(leaf, 0.8f));
                }
                i.Set(right ? 9 : 6, y + 1, bladder);
            }
            if (top) { i.Set(7, 2, leafL); i.Set(8, 2, leaf); i.Set(7, 1, leafL); i.Set(6, 2, leaf); i.Set(9, 3, leaf); }
            return i;
        }

        static Img DriedKelp(Img i, bool top)
        {
            if (top)
            {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++)
                    {
                        // rolled-up spiral seen end-on
                        float d = Mathf.Sqrt((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f));
                        float a = Mathf.Atan2(y - 7.5f, x - 7.5f);
                        float s = Mathf.Sin(d * 1.6f - a);
                        i[x, y] = Cl(s > 0.2f ? C(0x3E4E24) : C(0x28331A), 1f, 0.06f);
                    }
                i.RectOutline(0, 0, 16, 16, C(0x1C2410));
                return i;
            }
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float f = (y % 4 == 0) ? 0.82f : ((y % 4 == 1) ? 1.1f : 1f);
                    i[x, y] = Cl(C(0x3A4A22), f, 0.08f);
                }
            foreach (int y in new[] { 2, 13 }) for (int x = 0; x < 16; x++) i[x, y] = (x & 1) == 0 ? C(0x6E5E36) : C(0x54482A);
            return i;
        }

        /// <summary>Sea pickles are small boxes sampling many sub-windows of this tile, so it is opaque skin everywhere.</summary>
        static Img SeaPickle(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float f = ((x & 3) == 0) ? 0.84f : (((x & 3) == 1) ? 1.1f : 1f);
                    i[x, y] = Cl(C(0x6E8A3A), f, 0.07f);
                }
            for (int k = 0; k < 10; k++) i.Set(RndInt(16), RndInt(16), C(0xB8D878));
            for (int y = 0; y < 16; y += 4) for (int x = 1; x < 16; x += 4) i.Set(x, y, C(0xD8EE9A));
            return i;
        }

        // ==================================================================== hanging plants

        static Img HangingRoots(Img i)
        {
            i.Clear();
            Color32 a = C(0x8A6A42), b = C(0xA8825A), c = C(0x6A4E2C);
            for (int k = 0; k < 7; k++)
            {
                int x = 1 + k * 2 + (k & 1);
                int len = 6 + (int)(Hash.Get(k, 23) % 9);
                for (int y = 0; y < len; y++)
                {
                    float wob = Mathf.Sin(y * 0.6f + k) * 0.9f;
                    i.Set(Mathf.RoundToInt(x + wob), y, (y + k) % 5 == 0 ? b : ((y + k) % 7 == 0 ? c : a));
                }
                i.Set(Mathf.RoundToInt(x + Mathf.Sin(len * 0.6f + k) * 0.9f), len, b);
            }
            for (int k = 0; k < 4; k++) { i.Set(2 + k * 4, 0, c); i.Set(3 + k * 4, 0, c); i.Set(2 + k * 4, 1, a); }
            return i;
        }

        static Img PaleHangingMoss(Img i)
        {
            i.Clear();
            Color32 a = C(0x8E958A), b = C(0xB8C0B0), c = C(0x6E756A);
            for (int k = 0; k < 7; k++)
            {
                int x = 1 + k * 2 + (k & 1);
                int len = 7 + (int)(Hash.Get(k, 41) % 9);
                for (int y = 0; y < len; y++)
                {
                    int xx = x + ((y / 5 + k) & 1);
                    i.Set(xx, y, (y + k) % 3 == 0 ? b : a);
                    if ((y + k) % 5 == 0) i.Set(xx + 1, y, c);
                }
                i.Set(x, len, b);
                if (len < 15) i.Set(x + 1, len + 1, c);
            }
            return i;
        }

        /// <summary>Spore blossom: seen from below on the ceiling quad; pink petals around a green core.</summary>
        static Img SporeBlossom(Img i)
        {
            i.Clear();
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float dx = x - 7.5f, dy = y - 7.5f;
                    float d = Mathf.Sqrt(dx * dx + dy * dy);
                    float petal = Mathf.Cos(Mathf.Atan2(dy, dx) * 4f) * 0.5f + 0.5f;
                    float edge = 5.2f + petal * 2.2f;
                    if (d > edge) continue;
                    if (d < 2.1f) i[x, y] = d < 1.2f ? C(0x9AD05A) : C(0x5E9A3A);
                    else if (d > edge - 1.1f) i[x, y] = C(0x9E3A58);
                    else i[x, y] = petal > 0.55f ? C(0xD8688A) : C(0xB84E70);
                }
            return i;
        }

        /// <summary>Spore blossom base: the short hanging petals on the cross model below the ceiling quad.</summary>
        static Img SporeBlossomBase(Img i)
        {
            i.Clear();
            for (int k = 0; k < 4; k++)
            {
                int x = 2 + k * 3 + (k > 1 ? 1 : 0);
                i.Set(x, 0, C(0x5E9A3A)); i.Set(x + 1, 0, C(0x4A7E2E));
                for (int y = 1; y < 5; y++) { i.Set(x, y, C(0xD8688A)); i.Set(x + 1, y, C(0xB84E70)); }
                i.Set(x, 5, C(0x9E3A58));
            }
            return i;
        }

        // ==================================================================== dripleaf

        static Img BigDripleafTop(Img i)
        {
            i.Clear();
            Color32 a = C(0x5A9A2E), b = C(0x76B842), c = C(0x3A6E1A), rim = C(0x2E5A14);
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float nx = (x - 7.5f) / 7.6f, ny = (y - 7.5f) / 7.6f;
                    float d = nx * nx + ny * ny;
                    if (d > 1f) continue;
                    i[x, y] = d > 0.78f ? rim : (((x * 3 + y) % 7 == 0) ? b : a);
                }
            // veins radiating from where the stem meets the leaf
            for (int k = 0; k < 5; k++)
            {
                float ang = -Mathf.PI * 0.5f + (k - 2) * 0.55f;
                for (int s = 1; s < 8; s++)
                    i.Set(8 + Mathf.RoundToInt(Mathf.Cos(ang) * s * 0.9f), 12 + Mathf.RoundToInt(Mathf.Sin(ang) * s * 1.2f), c);
            }
            return i;
        }

        static Img DripleafStem(Img i, bool big)
        {
            i.Clear();
            Color32 a = C(0x5A9A2E), b = C(0x76B842), c = C(0x3A6E1A);
            for (int y = 0; y < 16; y++)
            {
                int x = 7 + (big ? Mathf.RoundToInt(Mathf.Sin(y * 0.3f) * 1.2f) : 0);
                i.Set(x, y, a); i.Set(x + 1, y, c);
                if (y % 5 == 2) i.Set(x - 1, y, b);
            }
            if (!big) { i.Set(5, 11, b); i.Set(4, 10, a); i.Set(10, 13, b); i.Set(11, 12, a); }
            return i;
        }

        static Img SmallDripleaf(Img i)
        {
            i.Clear();
            Color32 a = C(0x5A9A2E), b = C(0x76B842), c = C(0x3A6E1A);
            int[] xs = { 3, 8, 12 };
            int[] tops = { 5, 2, 6 };
            for (int k = 0; k < 3; k++)
            {
                int x = xs[k], t = tops[k];
                for (int y = t + 2; y < 16; y++) i.Set(x, y, (y & 1) == 0 ? a : c);
                for (int dx = -2; dx <= 2; dx++) { i.Set(x + dx, t, dx == 0 ? c : b); i.Set(x + dx, t + 1, a); }
                i.Set(x - 3, t + 1, c); i.Set(x + 3, t + 1, c);
            }
            return i;
        }

        // ==================================================================== cave vines

        /// <summary>tip = the lowest vine piece; lit pieces carry glow berries.</summary>
        static Img CaveVines(Img i, bool lit, bool tip)
        {
            i.Clear();
            Color32 stem = C(0x3A6E1E), leaf = C(0x5E9A38), leafD = C(0x2E5416);
            int[] xs = { 2, 7, 12 };
            for (int k = 0; k < 3; k++)
            {
                int x = xs[k];
                int len = tip ? 9 + (int)(Hash.Get(k, 61) % 6) : 16;
                for (int y = 0; y < len; y++)
                {
                    int xx = x + (((y / 4) + k) & 1);
                    i.Set(xx, y, stem);
                    if (y % 4 == 1) { i.Set(xx - 1, y, leaf); i.Set(xx - 2, y + 1, leafD); }
                    if (y % 4 == 3) { i.Set(xx + 1, y, leaf); i.Set(xx + 2, y + 1, leafD); }
                }
            }
            if (lit)
            {
                Color32 berry = C(0xF2A42A), hi = C(0xFFE08A), lo = C(0xB8680E);
                foreach (var (bx, by) in new[] { (3, 6), (8, 11), (13, 4), (4, 13) })
                {
                    if (tip && by > 12) continue;
                    i.Set(bx, by, hi); i.Set(bx + 1, by, berry); i.Set(bx, by + 1, lo); i.Set(bx + 1, by + 1, berry);
                }
            }
            return i;
        }

        // ==================================================================== dripstone & amethyst

        /// <summary>Pointed dripstone on a cross model; "down" hangs from the ceiling (wide at the top).</summary>
        static Img PointedDripstone(Img i, bool down)
        {
            i.Clear();
            Color32 a = C(0x8A6B58), b = C(0xA8876E), c = C(0x5E463A);
            for (int y = 0; y < 16; y++)
            {
                int t = down ? y : 15 - y;                  // distance from the base
                float w = (15 - t) / 15f;                   // 1 at the base, 0 at the tip
                int half = Mathf.Max(0, Mathf.RoundToInt(w * 4.2f));
                for (int x = 8 - half - 1; x <= 7 + half + 1; x++)
                {
                    bool edge = x == 8 - half - 1 || x == 7 + half + 1;
                    i.Set(x, y, edge ? c : (x < 8 ? ((t + x) % 4 == 0 ? b : a) : Img.Shade(a, 0.84f)));
                }
            }
            int tipY = down ? 15 : 0;
            i.Set(7, tipY, b); i.Set(8, tipY, c);
            return i;
        }

        /// <summary>Amethyst buds and cluster, growing from the bottom edge (cross model / wall decal).</summary>
        static Img Amethyst(Img i, int size)
        {
            i.Clear();
            Color32 a = C(0x9A6ED6), b = C(0xC8A2F0), c = C(0x6A44A6), d = C(0xF0E0FF);
            void Crystal(int cx, int h, int half)
            {
                for (int y = 16 - h; y < 16; y++)
                {
                    int t = y - (16 - h);
                    int hw = Mathf.Min(half, t / 2);
                    for (int x = cx - hw; x <= cx + hw; x++)
                        i.Set(x, y, x < cx ? b : (x == cx ? a : c));
                }
                i.Set(cx, 16 - h, d);
            }
            switch (size)
            {
                case 0: Crystal(8, 4, 1); Crystal(5, 2, 0); break;
                case 1: Crystal(8, 7, 1); Crystal(5, 4, 1); Crystal(11, 3, 0); break;
                case 2: Crystal(8, 10, 2); Crystal(4, 6, 1); Crystal(12, 7, 1); break;
                default: Crystal(8, 14, 2); Crystal(3, 9, 1); Crystal(13, 10, 1); Crystal(6, 6, 1); Crystal(11, 5, 1); break;
            }
            return i;
        }

        // ==================================================================== campfire & creaking heart

        static Img CampfireLog(Img i, bool lit, bool soul)
        {
            var p = Wood("oak");
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float f = (y % 4 == 0) ? 0.84f : ((y % 4 == 1) ? 1.08f : 1f);
                    i[x, y] = Cl(p.bark, f, 0.08f);
                }
            for (int k = 0; k < 6; k++) i.HLine(RndInt(16), RndInt(8), 8 + RndInt(8), Img.Shade(p.barkDark, 1.05f));
            if (!lit) return i;
            Color32 ember = soul ? C(0x2ED0DC) : C(0xF08A1A), hot = soul ? C(0xC8FFFF) : C(0xFFD070), coal = soul ? C(0x0E5A64) : C(0x8A2E0A);
            // charred, glowing underside and ember flecks
            for (int y = 11; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float r = Rnd01();
                    if (r < 0.35f) i[x, y] = coal;
                    else if (r < 0.55f) i[x, y] = ember;
                    else if (r < 0.62f) i[x, y] = hot;
                    else i[x, y] = Img.Shade(p.barkDark, 0.7f);
                }
            for (int k = 0; k < 8; k++) i.Set(RndInt(16), RndInt(11), ember);
            return i;
        }

        static Img CreakingHeart(Img i, bool top)
        {
            var bark = C(0x5A4632);
            if (top)
            {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++)
                    {
                        int d = Math.Max(Math.Abs(x * 2 - 15), Math.Abs(y * 2 - 15)) / 2;
                        i[x, y] = Cl(d >= 7 ? Img.Shade(bark, 0.75f) : ((d & 1) == 1 ? Img.Shade(bark, 0.9f) : Img.Shade(bark, 1.1f)), 1f, 0.06f);
                    }
                i.Rect(6, 6, 4, 4, C(0x24180E));
                i.Set(7, 7, C(0xD8741E)); i.Set(8, 8, C(0xF0A040)); i.Set(8, 7, C(0x9A4A10)); i.Set(7, 8, C(0x9A4A10));
                return i;
            }
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float f = (x % 4 == 0) ? 0.84f : 1f;
                    if ((Hash.Get(x, y >> 2, 3) & 7) == 0) f *= 0.86f;
                    i[x, y] = Cl(bark, f, 0.08f);
                }
            // hollow with a glowing heart
            i.Rect(5, 3, 6, 10, C(0x24180E));
            i.RectOutline(4, 2, 8, 12, Img.Shade(bark, 0.72f));
            i.RectOutline(5, 3, 6, 10, C(0x120A04));
            i.Set(7, 6, C(0xE8862A)); i.Set(8, 6, C(0xE8862A));
            i.Set(6, 7, C(0xC86A1A)); i.Set(7, 7, C(0xFFC060)); i.Set(8, 7, C(0xFFC060)); i.Set(9, 7, C(0xC86A1A));
            i.Set(6, 8, C(0xA8500A)); i.Set(7, 8, C(0xE8862A)); i.Set(8, 8, C(0xE8862A)); i.Set(9, 8, C(0xA8500A));
            i.Set(7, 9, C(0xA8500A)); i.Set(8, 9, C(0xA8500A));
            return i;
        }
    }
}
