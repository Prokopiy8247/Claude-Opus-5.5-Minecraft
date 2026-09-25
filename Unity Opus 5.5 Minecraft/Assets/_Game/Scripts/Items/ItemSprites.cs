using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Original procedural 16x16 item sprites. Shapes are drawn with primitives / small templates in flat colours and then
    /// auto-shaded (top-left highlight, bottom-right shade) and outlined, giving a consistent pixel-art style.
    /// Every sprite becomes a layer "item/&lt;name&gt;" in the shared texture array.
    /// </summary>
    public static partial class ItemSprites
    {
        public static readonly HashSet<string> Fallbacks = new HashSet<string>();
        static readonly List<string> registered = new List<string>();

        /// <summary>Sprites that need their own layer (non-block items + overlay layers).</summary>
        public static void RegisterTextures()
        {
            registered.Clear();
            foreach (var it in Items.All)
            {
                if (it.block != null && !NeedsFlatSprite(it)) continue;
                string n = SpriteName(it);
                if (n == null) continue;
                Tex.Id("item/" + n);
                registered.Add(n);
            }
            foreach (var extra in new[] { "potion_bottle", "potion_liquid", "splash_bottle", "lingering_bottle", "tipped_arrow_head", "leather_helmet_overlay", "bow_pulling_0", "bow_pulling_1", "bow_pulling_2", "crossbow_charged", "empty_armor_slot_helmet", "empty_armor_slot_chestplate", "empty_armor_slot_leggings", "empty_armor_slot_boots", "empty_slot_shield", "empty_slot_smithing", "barrier" })
                Tex.Id("item/" + extra);
        }

        /// <summary>Block items that are displayed as a flat sprite rather than a 3D block icon.</summary>
        public static bool NeedsFlatSprite(Item it)
        {
            if (it.block == null) return true;
            string id = it.id;
            return id.EndsWith("_door") || id == "cake" || id.EndsWith("_bed") || id == "brewing_stand" || id == "cauldron" || id == "flower_pot" || id == "hopper" || id == "campfire" || id == "soul_campfire" || id.EndsWith("_sign") || id.EndsWith("_hanging_sign") || id == "bell" || id == "lantern" || id == "soul_lantern" || id == "copper_lantern" || id == "iron_chain" || id == "copper_chain" || id == "redstone_wire" || id == "repeater" || id == "comparator" || id == "sugar_cane" || id == "kelp" || id == "nether_wart" || id == "cocoa" || id == "sweet_berry_bush" || id == "cave_vines" || id == "frogspawn" || id == "pointed_dripstone" || id == "wheat" || id == "carrots" || id == "potatoes" || id == "beetroots" || id == "pumpkin_stem" || id == "melon_stem" || id == "armor_stand";
        }

        public static string SpriteName(Item it)
        {
            if (it is SpawnEggItem) return it.id;
            return it.iconName ?? it.id;
        }

        // ------------------------------------------------------------------ palette helpers
        static Color32 C(uint rgb) => MathX.Hex(rgb);
        static Color32 Sh(Color32 c, float f) => Img.Shade(c, f);
        static Color32 Mix(Color32 a, Color32 b, float t) => MathX.Lerp(a, b, t);

        public static readonly Dictionary<string, (Color32 main, Color32 dark)> Tiers = new Dictionary<string, (Color32, Color32)>
        {
            {"wooden", (C(0xA0824E), C(0x6A5230))}, {"stone", (C(0x8C8C8C), C(0x5A5A5A))}, {"copper", (C(0xD2784E), C(0x8E4A2E))},
            {"iron", (C(0xE0E0E0), C(0x9A9A9A))}, {"golden", (C(0xF8D848), C(0xC09020))}, {"diamond", (C(0x5CEAD8), C(0x2A9A90))}, {"netherite", (C(0x5A5058), C(0x362E34))},
        };
        public static readonly Dictionary<string, Color32> ArmorColors = new Dictionary<string, Color32>
        {
            {"leather", C(0xA0643E)}, {"copper", C(0xD2784E)}, {"chainmail", C(0x9A9AA4)}, {"iron", C(0xDCDCDC)}, {"golden", C(0xF4D050)}, {"diamond", C(0x5CE0D4)}, {"netherite", C(0x564C54)}, {"turtle", C(0x4AA04A)},
        };

        // ------------------------------------------------------------------ canvas
        sealed class Cv
        {
            public readonly Color32[] px = new Color32[256];
            public readonly bool[] noShade = new bool[256];
            public void Set(int x, int y, Color32 c, bool flat = false) { if (x < 0 || y < 0 || x > 15 || y > 15) return; px[y * 16 + x] = c; noShade[y * 16 + x] = flat; }
            public Color32 Get(int x, int y) => (x < 0 || y < 0 || x > 15 || y > 15) ? default : px[y * 16 + x];
            public bool Has(int x, int y) => Get(x, y).a > 0;
            public void Line(float x0, float y0, float x1, float y1, Color32 c, int thick = 1)
            {
                int steps = Mathf.CeilToInt(Mathf.Max(Mathf.Abs(x1 - x0), Mathf.Abs(y1 - y0))) * 2 + 1;
                for (int i = 0; i <= steps; i++)
                {
                    float t = i / (float)steps;
                    int x = Mathf.RoundToInt(Mathf.Lerp(x0, x1, t)), y = Mathf.RoundToInt(Mathf.Lerp(y0, y1, t));
                    for (int dy = 0; dy < thick; dy++) for (int dx = 0; dx < thick; dx++) Set(x + dx, y + dy, c);
                }
            }
            public void Rect(int x, int y, int w, int h, Color32 c) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) Set(i, j, c); }
            public void Ellipse(float cx, float cy, float rx, float ry, Color32 c)
            {
                for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++)
                    {
                        float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
                        if (dx * dx + dy * dy <= 1f) Set(x, y, c);
                    }
            }
            public void Poly(Color32 c, params Vector2[] pts)
            {
                for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++)
                        if (Inside(new Vector2(x + 0.5f, y + 0.5f), pts)) Set(x, y, c);
            }
            static bool Inside(Vector2 p, Vector2[] poly)
            {
                bool inside = false;
                for (int i = 0, j = poly.Length - 1; i < poly.Length; j = i++)
                    if (((poly[i].y > p.y) != (poly[j].y > p.y)) && (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)) inside = !inside;
                return inside;
            }
            public void Tmpl(string[] rows, Dictionary<char, Color32> pal, int ox = 0, int oy = 0)
            {
                for (int y = 0; y < rows.Length; y++)
                    for (int x = 0; x < rows[y].Length; x++)
                    {
                        char ch = rows[y][x];
                        if (ch == '.' || ch == ' ') continue;
                        if (pal.TryGetValue(ch, out var c)) Set(x + ox, y + oy, c);
                    }
            }
            /// <summary>Top-left light, bottom-right shade, dark outline.</summary>
            public Color32[] Finish(bool outline = true, float light = 1.22f, float dark = 0.72f)
            {
                var src = (Color32[])px.Clone();
                var o = new Color32[256];
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++)
                    {
                        var c = src[y * 16 + x];
                        if (c.a == 0) continue;
                        if (noShade[y * 16 + x]) { o[y * 16 + x] = c; continue; }
                        bool tl = !Has2(src, x - 1, y) || !Has2(src, x, y - 1);
                        bool br = !Has2(src, x + 1, y) || !Has2(src, x, y + 1);
                        if (tl && !br) c = Img.Shade(c, light);
                        else if (br && !tl) c = Img.Shade(c, dark);
                        o[y * 16 + x] = c;
                    }
                if (outline)
                {
                    var o2 = (Color32[])o.Clone();
                    for (int y = 0; y < 16; y++)
                        for (int x = 0; x < 16; x++)
                        {
                            if (src[y * 16 + x].a != 0) continue;
                            Color32 n = default; bool any = false;
                            foreach (var (dx, dy) in new[] { (1, 0), (-1, 0), (0, 1), (0, -1) })
                                if (Has2(src, x + dx, y + dy)) { n = src[(y + dy) * 16 + x + dx]; any = true; break; }
                            if (any) o2[y * 16 + x] = Img.Shade(n, 0.38f);
                        }
                    o = o2;
                }
                // generator rows are top-down already
                return o;
            }
            static bool Has2(Color32[] a, int x, int y) => x >= 0 && y >= 0 && x < 16 && y < 16 && a[y * 16 + x].a > 0;
        }

        static Dictionary<char, Color32> P(params object[] kv)
        {
            var d = new Dictionary<char, Color32>();
            for (int i = 0; i + 1 < kv.Length; i += 2) d[(char)kv[i]] = (Color32)kv[i + 1];
            return d;
        }

        static readonly Color32 Wood = C(0x8A6A3A), WoodD = C(0x5E4524), White = C(0xFFFFFF), Black = C(0x151515);

        // ------------------------------------------------------------------ main entry
        public static Color32[] Pixels(string name)
        {
            Color32[] r = null;
            try { r = Make(name); } catch (Exception e) { Debug.LogWarning("Item sprite failed " + name + ": " + e.Message); }
            if (r == null) { Fallbacks.Add(name); r = FallbackSprite(name); }
            return r;
        }

        static Color32[] FallbackSprite(string name)
        {
            var cv = new Cv();
            var col = MathX.Hex((uint)(Hash.StringHash(name) & 0xFFFFFF) | 0x404040);
            cv.Ellipse(8, 8, 5, 5, col);
            cv.Rect(6, 6, 4, 4, Sh(col, 1.3f));
            return cv.Finish();
        }

        static Color32[] Make(string n)
        {
            var cv = new Cv();
            // ---------------- overlays / slot icons
            switch (n)
            {
                case "empty_armor_slot_helmet": Armor(cv, "helmet", C(0x8B8B8B)); return Ghost(cv);
                case "empty_armor_slot_chestplate": Armor(cv, "chestplate", C(0x8B8B8B)); return Ghost(cv);
                case "empty_armor_slot_leggings": Armor(cv, "leggings", C(0x8B8B8B)); return Ghost(cv);
                case "empty_armor_slot_boots": Armor(cv, "boots", C(0x8B8B8B)); return Ghost(cv);
                case "empty_slot_shield": Shield(cv, C(0x8B8B8B), C(0x8B8B8B)); return Ghost(cv);
                case "empty_slot_smithing": cv.Rect(4, 3, 8, 10, C(0x8B8B8B)); return Ghost(cv);
                case "barrier": cv.Line(3, 3, 12, 12, C(0xE02020), 2); cv.Line(12, 3, 3, 12, C(0xE02020), 2); return cv.Finish(false);
            }
            // ---------------- tools & weapons
            foreach (var t in Tiers)
            {
                if (!n.StartsWith(t.Key + "_")) continue;
                string kind = n.Substring(t.Key.Length + 1);
                var (m, d) = t.Value;
                switch (kind)
                {
                    case "sword": Sword(cv, m, d); return cv.Finish();
                    case "pickaxe": Pickaxe(cv, m, d); return cv.Finish();
                    case "axe": AxeT(cv, m, d); return cv.Finish();
                    case "shovel": Shovel(cv, m, d); return cv.Finish();
                    case "hoe": Hoe(cv, m, d); return cv.Finish();
                    case "spear": Spear(cv, m, d); return cv.Finish();
                }
            }
            // ---------------- armor
            foreach (var a in ArmorColors)
            {
                if (!n.StartsWith(a.Key + "_")) continue;
                string kind = n.Substring(a.Key.Length + 1);
                if (kind == "helmet" || kind == "chestplate" || kind == "leggings" || kind == "boots")
                {
                    Armor(cv, kind, a.Value);
                    if (a.Key == "chainmail") for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) if (cv.Has(x, y) && ((x + y) % 3 == 0)) cv.Set(x, y, C(0x505058));
                    if (a.Key == "diamond" || a.Key == "golden" || a.Key == "iron") for (int x = 4; x < 12; x++) if (cv.Has(x, 5)) cv.Set(x, 5, Sh(a.Value, 1.25f));
                    return cv.Finish();
                }
                if (kind == "horse_armor") { HorseArmor(cv, a.Value); return cv.Finish(); }
            }
            if (n == "wolf_armor") { HorseArmor(cv, C(0xB89A7A)); return cv.Finish(); }
            if (n.EndsWith("_spawn_egg")) { SpawnEgg(cv, n.Substring(0, n.Length - 10)); return cv.Finish(); }
            if (n.StartsWith("music_disc_")) { Disc(cv, n.Substring(11)); return cv.Finish(); }
            if (n.EndsWith("_dye")) { Dye(cv, n.Substring(0, n.Length - 4)); return cv.Finish(); }
            if (n.EndsWith("_boat") || n.EndsWith("_raft")) { Boat(cv, n); return cv.Finish(); }
            if (n.EndsWith("minecart")) { Minecart(cv, n); return cv.Finish(); }
            if (n.EndsWith("_bucket") || n == "bucket") { Bucket(cv, n); return cv.Finish(); }
            if (n.EndsWith("_door")) { Door(cv, n); return cv.Finish(); }
            if (n.EndsWith("_sign") || n.EndsWith("_hanging_sign")) { Sign(cv, n); return cv.Finish(); }
            if (n.EndsWith("_bed")) { Bed(cv, n); return cv.Finish(); }
            if (n.EndsWith("_smithing_template")) { Template(cv, n); return cv.Finish(); }
            if (n.EndsWith("_ingot")) { Ingot(cv, IngotColor(n)); return cv.Finish(); }
            if (n.EndsWith("_nugget")) { Nugget(cv, n == "gold_nugget" ? C(0xF8D848) : n == "iron_nugget" ? C(0xD8D8D8) : C(0xD2784E)); return cv.Finish(); }
            if (n.StartsWith("raw_")) { RawOre(cv, n == "raw_iron" ? C(0xD8AF93) : n == "raw_gold" ? C(0xE8B830) : C(0xC8764A)); return cv.Finish(); }
            return Misc(cv, n);
        }

        static Color32[] Ghost(Cv cv)
        {
            var o = cv.Finish(false);
            for (int i = 0; i < o.Length; i++) if (o[i].a > 0) o[i] = new Color32(o[i].r, o[i].g, o[i].b, 90);
            return o;
        }

        static Color32 IngotColor(string n)
        {
            if (n.StartsWith("iron")) return C(0xDCDCDC);
            if (n.StartsWith("gold")) return C(0xF8D040);
            if (n.StartsWith("copper")) return C(0xD2784E);
            if (n.StartsWith("netherite")) return C(0x4A4248);
            return C(0xAAAAAA);
        }

        // ------------------------------------------------------------------ tool shapes (handle bottom-left -> head top-right)
        static void Handle(Cv cv, int x0, int y0, int x1, int y1)
        {
            cv.Line(x0, y0, x1, y1, Wood, 1);
            cv.Line(x0 + 1, y0, x1 + 1, y1, WoodD, 1);
        }
        static void Sword(Cv cv, Color32 m, Color32 d)
        {
            // blade
            cv.Line(5, 10, 13, 2, m, 2);
            cv.Line(6, 10, 13, 3, d, 1);
            cv.Set(14, 1, m); cv.Set(14, 2, Sh(m, 1.2f)); cv.Set(13, 1, Sh(m, 1.2f));
            // guard
            cv.Line(2, 9, 6, 13, WoodD, 1); cv.Line(3, 9, 7, 13, Sh(WoodD, 0.9f), 1);
            // grip + pommel
            cv.Line(1, 14, 4, 11, Wood, 1);
            cv.Set(0, 15, m); cv.Set(1, 15, d);
        }
        static void Pickaxe(Cv cv, Color32 m, Color32 d)
        {
            Handle(cv, 1, 14, 10, 5);
            for (int i = 0; i <= 20; i++)
            {
                float th = Mathf.Lerp(8f, 82f, i / 20f) * Mathf.Deg2Rad;
                float R = 12.5f;
                float x = 1 + R * Mathf.Cos(th), y = 14 - R * Mathf.Sin(th);
                cv.Set(Mathf.RoundToInt(x), Mathf.RoundToInt(y), m);
                float R2 = 11.3f;
                float x2 = 1 + R2 * Mathf.Cos(th), y2 = 14 - R2 * Mathf.Sin(th);
                bool edge = i < 3 || i > 17;
                if (!edge) cv.Set(Mathf.RoundToInt(x2), Mathf.RoundToInt(y2), d);
            }
        }
        static void AxeT(Cv cv, Color32 m, Color32 d)
        {
            Handle(cv, 1, 14, 11, 4);
            cv.Poly(m, new Vector2(6, 2), new Vector2(10, 0.5f), new Vector2(12.5f, 3), new Vector2(10, 6.5f), new Vector2(7.5f, 5.5f));
            cv.Poly(d, new Vector2(10.5f, 3.5f), new Vector2(12.5f, 3.2f), new Vector2(10, 6.5f), new Vector2(9.5f, 5.5f));
            cv.Line(11, 4, 13, 2, Wood);
        }
        static void Shovel(Cv cv, Color32 m, Color32 d)
        {
            Handle(cv, 1, 14, 9, 6);
            cv.Poly(m, new Vector2(9, 5.5f), new Vector2(11.5f, 1.5f), new Vector2(15, 1), new Vector2(14.5f, 4.5f), new Vector2(10.5f, 7));
            cv.Line(12, 5, 14, 3, d);
        }
        static void Hoe(Cv cv, Color32 m, Color32 d)
        {
            Handle(cv, 1, 14, 11, 4);
            cv.Line(7, 3, 11, 3, m, 2);
            cv.Line(11, 3, 13, 5, m, 1);
            cv.Line(7, 4, 10, 4, d, 1);
        }
        static void Spear(Cv cv, Color32 m, Color32 d)
        {
            Handle(cv, 0, 15, 10, 5);
            cv.Poly(m, new Vector2(9, 6.5f), new Vector2(11, 3), new Vector2(15.5f, 0.5f), new Vector2(13, 5), new Vector2(9.5f, 7.5f));
            cv.Line(10, 6, 14, 2, d);
            cv.Line(8, 6, 10, 8, Sh(d, 0.8f));
        }

        static void Armor(Cv cv, string kind, Color32 m)
        {
            string[] rows;
            switch (kind)
            {
                case "helmet": rows = new[] { "", "", "", "....mmmmmmmm", "...mmmmmmmmmm", "..mmmmmmmmmmmm", "..mmmmmmmmmmmm", "..mmm......mmm", "..mmm......mmm", "..mmm......mmm", "..mm........mm" }; break;
                case "chestplate": rows = new[] { "", "..mmmm....mmmm", ".mmmmmm..mmmmmm", ".mmmmmmmmmmmmmm", ".mmmmmmmmmmmmmm", ".mmmmmmmmmmmmmm", "..mmmmmmmmmmmm", "...mmmmmmmmmm", "...mmmmmmmmmm", "...mmmmmmmmmm", "...mmmmmmmmmm", "...mmmmmmmmmm", "....mmmmmmmm" }; break;
                case "leggings": rows = new[] { "", "", "...mmmmmmmmmm", "...mmmmmmmmmm", "...mmmmmmmmmm", "...mmmmmmmmmm", "...mmmm..mmmm", "...mmmm..mmmm", "...mmmm..mmmm", "...mmm....mmm", "...mmm....mmm", "...mmm....mmm", "...mmm....mmm", "...mmm....mmm" }; break;
                default: rows = new[] { "", "", "", "", "", "", "", "...mmm....mmm", "...mmm....mmm", "...mmm....mmm", "..mmmm....mmmm", ".mmmmm....mmmmm", ".mmmmm....mmmmm" }; break;
            }
            cv.Tmpl(rows, P('m', m));
        }
        static void HorseArmor(Cv cv, Color32 m)
        {
            cv.Tmpl(new[] { "", "", "..........mmm", ".........mmmmm", "........mmmmmm", "..mmmmmmmmmmm", ".mmmmmmmmmmm", ".mmmmmmmmmm", ".mm.......mm", ".mm.......mm", ".mm.......mm" }, P('m', m));
        }
        static void Shield(Cv cv, Color32 m, Color32 rim)
        {
            cv.Tmpl(new[] { "", "...rrrrrrrrrr", "...rmmmmmmmmr", "...rmmmmmmmmr", "...rmmmmmmmmr", "...rmmmmmmmmr", "...rmmmmmmmmr", "...rmmmmmmmmr", "...rmmmmmmmmr", "....rmmmmmmr", ".....rmmmmr", "......rmmr", ".......rr" }, P('m', m, 'r', rim));
        }

        // ------------------------------------------------------------------ materials
        static void Ingot(Cv cv, Color32 m)
        {
            cv.Poly(m, new Vector2(2, 9), new Vector2(9, 4), new Vector2(14, 6), new Vector2(7, 12));
            cv.Poly(Sh(m, 0.75f), new Vector2(2, 9), new Vector2(7, 12), new Vector2(7, 13.5f), new Vector2(2, 10.5f));
            cv.Poly(Sh(m, 0.6f), new Vector2(7, 12), new Vector2(14, 6), new Vector2(14, 7.5f), new Vector2(7, 13.5f));
        }
        static void Nugget(Cv cv, Color32 m) { cv.Ellipse(7, 9, 3, 2.5f, m); cv.Ellipse(10, 7, 2.5f, 2, Sh(m, 1.1f)); }
        static void RawOre(Cv cv, Color32 m) { cv.Ellipse(8, 9, 5, 4, m); cv.Ellipse(6, 6.5f, 3, 2.5f, Sh(m, 1.1f)); cv.Set(9, 8, Sh(m, 0.7f)); cv.Set(6, 10, Sh(m, 0.7f)); cv.Set(10, 11, Sh(m, 0.7f)); }
        static void Gem(Cv cv, Color32 m)
        {
            cv.Poly(m, new Vector2(3, 6), new Vector2(6, 3), new Vector2(10, 3), new Vector2(13, 6), new Vector2(8, 13));
            cv.Line(3, 6, 13, 6, Sh(m, 1.3f));
            cv.Line(6, 3, 8, 6, Sh(m, 1.2f)); cv.Line(10, 3, 8, 6, Sh(m, 1.2f));
            cv.Set(5, 4, White);
        }
        static void Dust(Cv cv, Color32 m)
        {
            cv.Ellipse(8, 11, 6, 3, m); cv.Ellipse(8, 9, 4, 2.5f, Sh(m, 1.1f)); cv.Ellipse(8, 7.5f, 2, 1.5f, Sh(m, 1.2f));
            for (int i = 0; i < 6; i++) cv.Set(3 + i * 2, 10 + (i % 2), Sh(m, 0.8f));
        }
        static void Dye(Cv cv, string color)
        {
            Color32 m = TextureGen.DyeColors.TryGetValue(color, out var c) ? c : C(0x808080);
            cv.Ellipse(8, 9, 5, 5, m);
            cv.Ellipse(7, 7, 2, 2, Sh(m, 1.25f));
            cv.Line(7, 3, 9, 3, Sh(m, 0.8f)); cv.Set(8, 2, Sh(m, 0.8f));
        }

        static void SpawnEgg(Cv cv, string mob)
        {
            var def = MobRegistry.Get(mob);
            Color32 a = def != null ? def.eggBase : C(0x808080), b = def != null ? def.eggSpots : C(0x404040);
            cv.Ellipse(8, 9, 5, 6.5f, a);
            var r = new RNG(Hash.StringHash(mob));
            for (int i = 0; i < 7; i++)
            {
                int x = 4 + r.Next(8), y = 4 + r.Next(10);
                if (cv.Has(x, y)) { cv.Set(x, y, b); if (r.NextBool() && cv.Has(x + 1, y)) cv.Set(x + 1, y, b); }
            }
        }
        static void Disc(Cv cv, string name)
        {
            var label = MathX.Hex((uint)(Hash.StringHash(name) & 0xFFFFFF) | 0x303030);
            cv.Ellipse(8, 8, 6.5f, 6.5f, C(0x222222));
            cv.Ellipse(8, 8, 4.5f, 4.5f, C(0x2E2E2E));
            cv.Ellipse(8, 8, 2.5f, 2.5f, label);
            cv.Set(8, 8, C(0x111111), true);
            cv.Set(5, 5, C(0x5A5A5A)); cv.Set(4, 6, C(0x4A4A4A));
        }
        static void Boat(Cv cv, string n)
        {
            string wood = n.Replace("_chest_boat", "").Replace("_boat", "").Replace("_chest_raft", "").Replace("_raft", "");
            var pal = TextureGen.Wood(wood == "bamboo" ? "bamboo" : wood);
            cv.Tmpl(new[] { "", "", "", "", "", "", "m............m", "mm..........mm", ".mmmmmmmmmmmmm", ".mddddddddddm", "..mmmmmmmmmm" }, P('m', pal.plank, 'd', pal.plankDark), 1, 0);
            if (n.Contains("chest")) { cv.Rect(6, 4, 5, 4, C(0xA0703A)); cv.Rect(8, 5, 1, 2, C(0x3A3A3A)); }
        }
        static void Minecart(Cv cv, string n)
        {
            cv.Tmpl(new[] { "", "", "", "", "", "", "..mmmmmmmmmmmm", "..mddddddddddm", "..mddddddddddm", "..mmmmmmmmmmmm", "...o..o...o..o", "..ooo.....ooo" }, P('m', C(0x9A9AA0), 'd', C(0x3A3A40), 'o', C(0x2A2A2A)));
            string content = n.Replace("_minecart", "");
            if (content == "chest") cv.Rect(4, 3, 8, 4, C(0xA0703A));
            else if (content == "hopper") cv.Rect(4, 4, 8, 3, C(0x4A4A4A));
            else if (content == "tnt") { cv.Rect(4, 3, 8, 4, C(0xD83A2A)); cv.Rect(4, 4, 8, 1, C(0xE8E0D0)); }
            else if (content == "furnace") { cv.Rect(4, 3, 8, 4, C(0x7A7A7A)); cv.Rect(6, 4, 3, 2, C(0x2A2A2A)); }
        }
        static void Bucket(Cv cv, string n)
        {
            cv.Tmpl(new[] { "", "", "", "..mmmmmmmmmmmm", "..mcccccccccm", "..mcccccccccm", "...mmmmmmmmm", "...mmmmmmmmm", "...mmmmmmmmm", "....mmmmmmm", "....mmmmmmm", ".....mmmmm" }, P('m', C(0xC8C8C8), 'c', C(0x3A3A3A)));
            cv.Line(2, 3, 5, 0, C(0x9A9A9A)); cv.Line(13, 3, 10, 0, C(0x9A9A9A)); cv.Line(5, 0, 10, 0, C(0x9A9A9A));
            Color32? fill = null;
            if (n == "water_bucket") fill = C(0x3A62E0);
            else if (n == "lava_bucket") fill = C(0xFF7A10);
            else if (n == "milk_bucket") fill = C(0xF8F8F8);
            else if (n == "powder_snow_bucket") fill = C(0xE8F0F8);
            else if (n != "bucket") fill = C(0x3A62E0);
            if (fill.HasValue) { for (int x = 3; x < 13; x++) { cv.Set(x, 4, fill.Value, true); cv.Set(x, 5, Sh(fill.Value, 0.85f), true); } }
            if (n.EndsWith("_bucket") && n != "water_bucket" && n != "lava_bucket" && n != "milk_bucket" && n != "powder_snow_bucket")
            {
                string mob = n.Replace("_bucket", "");
                var def = MobRegistry.Get(mob);
                var c = def != null ? def.eggBase : C(0xD08050);
                cv.Ellipse(8, 8, 3, 1.5f, c);
            }
        }
        static void Door(Cv cv, string n)
        {
            string wood = n.Replace("_door", "");
            Color32 m, d;
            if (wood == "iron") { m = C(0xD0D0D0); d = C(0x8A8A8A); }
            else if (wood.Contains("copper")) { m = C(0xC8744A); d = C(0x8A4A2E); if (wood.Contains("oxidized")) { m = C(0x5AA890); d = C(0x3A7A68); } else if (wood.Contains("weathered")) { m = C(0x6A9A7A); d = C(0x4A7058); } else if (wood.Contains("exposed")) { m = C(0xA88A6A); d = C(0x7A5E48); } }
            else { var p = TextureGen.Wood(wood); m = p.plank; d = p.plankDark; }
            cv.Rect(4, 0, 8, 16, m);
            cv.Rect(5, 1, 6, 5, d); cv.Rect(5, 9, 6, 6, d);
            cv.Set(10, 8, C(0x303030)); cv.Set(10, 7, C(0x303030));
        }
        static void Sign(Cv cv, string n)
        {
            string wood = n.Replace("_hanging_sign", "").Replace("_wall_sign", "").Replace("_sign", "");
            var p = TextureGen.Wood(wood);
            if (n.Contains("hanging")) { cv.Line(4, 1, 4, 4, C(0x6A6A6A)); cv.Line(11, 1, 11, 4, C(0x6A6A6A)); cv.Rect(2, 5, 12, 8, p.plank); }
            else { cv.Rect(1, 2, 14, 8, p.plank); cv.Rect(7, 10, 2, 5, p.bark); }
            for (int y = 4; y < 9; y += 2) cv.Line(3, y + (n.Contains("hanging") ? 3 : 0) - 1, 12, y + (n.Contains("hanging") ? 3 : 0) - 1, Sh(p.plank, 0.7f));
        }
        static void Bed(Cv cv, string n)
        {
            string col = n.Replace("_bed", "");
            var c = TextureGen.DyeColors.TryGetValue(col, out var dc) ? dc : C(0xA02020);
            cv.Rect(1, 7, 14, 4, c); cv.Rect(1, 6, 4, 2, C(0xF0F0F0)); cv.Rect(1, 11, 2, 2, C(0x8A6A3A)); cv.Rect(13, 11, 2, 2, C(0x8A6A3A));
        }
        static void Template(Cv cv, string n)
        {
            cv.Rect(3, 1, 10, 14, C(0x3A4A5A));
            cv.Rect(4, 2, 8, 12, C(0x5A6A7A));
            var accent = n.StartsWith("netherite") ? C(0x6A5A64) : C(0x7AA0C0);
            cv.Poly(accent, new Vector2(6, 5), new Vector2(10, 5), new Vector2(8, 11));
        }

        static void Bottle(Cv cv, Color32 liquid, bool splash, bool lingering, bool empty = false)
        {
            var glass = C(0xC8D8E8);
            if (splash) { cv.Rect(6, 1, 4, 2, glass); cv.Set(7, 0, C(0x8A6A3A)); cv.Set(8, 0, C(0x8A6A3A)); }
            else { cv.Rect(7, 1, 2, 3, glass); cv.Rect(6, 0, 4, 1, C(0x8A6A3A)); }
            cv.Ellipse(8, 10, 5, 5, glass);
            if (!empty)
            {
                for (int y = 8; y < 16; y++) for (int x = 0; x < 16; x++) if (cv.Has(x, y) && (x - 8) * (x - 8) + (y - 10) * (y - 10) < 16) cv.Set(x, y, liquid);
                if (lingering) for (int x = 5; x < 11; x++) cv.Set(x, 8, Sh(liquid, 1.3f));
            }
            cv.Set(6, 8, White); cv.Set(5, 9, White);
        }

        // ------------------------------------------------------------------ everything else
        static Color32[] Misc(Cv cv, string n)
        {
            switch (n)
            {
                // ---- overlays for tinted items
                case "potion_bottle": Bottle(cv, default, false, false, true); return cv.Finish();
                case "splash_bottle": Bottle(cv, default, true, false, true); return cv.Finish();
                case "lingering_bottle": Bottle(cv, default, true, true, true); return cv.Finish();
                case "potion_liquid": for (int y = 7; y < 15; y++) for (int x = 3; x < 13; x++) if ((x - 8) * (x - 8) + (y - 10) * (y - 10) < 16) cv.Set(x, y, White); return cv.Finish(false);
                case "potion": Bottle(cv, C(0x385DC6), false, false); return cv.Finish();
                case "splash_potion": Bottle(cv, C(0x385DC6), true, false); return cv.Finish();
                case "lingering_potion": Bottle(cv, C(0x385DC6), true, true); return cv.Finish();
                case "glass_bottle": Bottle(cv, default, false, false, true); return cv.Finish();
                case "experience_bottle": Bottle(cv, C(0x9AE030), false, false); return cv.Finish();
                case "honey_bottle": Bottle(cv, C(0xF0A020), false, false); return cv.Finish();
                case "dragon_breath": Bottle(cv, C(0xE070D0), false, false); return cv.Finish();
                // ---- sticks & rods
                case "stick": cv.Line(4, 12, 11, 5, Wood, 1); cv.Line(5, 12, 12, 5, WoodD, 1); return cv.Finish();
                case "blaze_rod": cv.Line(4, 12, 11, 5, C(0xF8C030), 2); cv.Set(6, 9, C(0xFFF090)); cv.Set(9, 6, C(0xFFF090)); return cv.Finish();
                case "breeze_rod": cv.Line(4, 12, 11, 5, C(0xA0B8F0), 2); cv.Set(6, 9, White); cv.Set(9, 6, White); return cv.Finish();
                case "bone": cv.Line(4, 12, 11, 5, C(0xE8E4D0), 2); cv.Ellipse(4, 12.5f, 1.8f, 1.8f, C(0xE8E4D0)); cv.Ellipse(12, 4.5f, 1.8f, 1.8f, C(0xE8E4D0)); return cv.Finish();
                case "string": for (int i = 0; i < 12; i++) cv.Set(3 + i, 4 + (int)(Mathf.Sin(i * 0.8f) * 2 + 4), C(0xF0F0F0)); cv.Line(3, 12, 8, 10, C(0xE0E0E0)); return cv.Finish();
                case "feather": cv.Line(4, 13, 12, 3, C(0xE0E0E0), 1); cv.Poly(C(0xF8F8F8), new Vector2(6, 9), new Vector2(11, 2), new Vector2(13, 3), new Vector2(8, 11)); return cv.Finish();
                case "flint": cv.Poly(C(0x4A4A4A), new Vector2(5, 3), new Vector2(11, 4), new Vector2(12, 11), new Vector2(7, 13), new Vector2(4, 9)); cv.Set(7, 6, C(0x8A8A8A)); return cv.Finish();
                case "coal": RawOre(cv, C(0x2A2A2A)); return cv.Finish();
                case "charcoal": RawOre(cv, C(0x3A3028)); return cv.Finish();
                case "diamond": Gem(cv, C(0x5CEAD8)); return cv.Finish();
                case "emerald": cv.Poly(C(0x2AC860), new Vector2(8, 1), new Vector2(12, 5), new Vector2(12, 11), new Vector2(8, 15), new Vector2(4, 11), new Vector2(4, 5)); cv.Line(8, 3, 8, 13, C(0x6AF090)); return cv.Finish();
                case "lapis_lazuli": cv.Poly(C(0x2A4AC8), new Vector2(4, 4), new Vector2(11, 3), new Vector2(13, 9), new Vector2(8, 13), new Vector2(3, 10)); cv.Set(7, 6, C(0x8AA8FF)); cv.Set(9, 9, C(0x8AA8FF)); return cv.Finish();
                case "quartz": cv.Poly(C(0xECE4DA), new Vector2(5, 2), new Vector2(9, 1), new Vector2(12, 8), new Vector2(9, 14), new Vector2(5, 11)); cv.Line(7, 3, 9, 11, C(0xFFFFFF)); return cv.Finish();
                case "amethyst_shard": cv.Poly(C(0xA070E0), new Vector2(3, 13), new Vector2(9, 2), new Vector2(13, 4), new Vector2(7, 14)); cv.Line(5, 12, 10, 4, C(0xD0A8FF)); return cv.Finish();
                case "echo_shard": cv.Poly(C(0x0A5060), new Vector2(3, 13), new Vector2(9, 2), new Vector2(13, 4), new Vector2(7, 14)); cv.Line(5, 12, 10, 4, C(0x30D0E0)); return cv.Finish();
                case "prismarine_shard": cv.Poly(C(0x5AA898), new Vector2(3, 12), new Vector2(10, 2), new Vector2(13, 5), new Vector2(6, 14)); return cv.Finish();
                case "prismarine_crystals": cv.Ellipse(6, 9, 3, 3, C(0xB8E8D8)); cv.Ellipse(10, 6, 3, 3, C(0x90D8C8)); cv.Ellipse(10, 11, 2, 2, C(0xD8F8E8)); return cv.Finish();
                case "netherite_scrap": RawOre(cv, C(0x5A3A30)); cv.Line(5, 7, 10, 10, C(0x8A6A5A)); return cv.Finish();
                case "redstone": Dust(cv, C(0xD01A10)); return cv.Finish();
                case "glowstone_dust": Dust(cv, C(0xF8D060)); return cv.Finish();
                case "gunpowder": Dust(cv, C(0x5A5A5A)); return cv.Finish();
                case "sugar": Dust(cv, C(0xF4F4F4)); return cv.Finish();
                case "blaze_powder": Dust(cv, C(0xF8A020)); return cv.Finish();
                case "sulfur_dust": Dust(cv, C(0xD8CC40)); return cv.Finish();
                case "cinnabar_dust": Dust(cv, C(0xC03A30)); return cv.Finish();
                case "bone_meal": Dust(cv, C(0xE8E8F0)); return cv.Finish();
                case "clay_ball": cv.Ellipse(8, 8.5f, 5, 4.5f, C(0xA0A8B8)); return cv.Finish();
                case "brick": cv.Poly(C(0xB05A40), new Vector2(2, 9), new Vector2(9, 5), new Vector2(14, 7), new Vector2(7, 12)); cv.Poly(C(0x7A3A28), new Vector2(2, 9), new Vector2(7, 12), new Vector2(7, 13.5f), new Vector2(2, 10.5f)); return cv.Finish();
                case "nether_brick": cv.Poly(C(0x4A1E24), new Vector2(2, 9), new Vector2(9, 5), new Vector2(14, 7), new Vector2(7, 12)); cv.Poly(C(0x2A1014), new Vector2(2, 9), new Vector2(7, 12), new Vector2(7, 13.5f), new Vector2(2, 10.5f)); return cv.Finish();
                case "leather": cv.Poly(C(0xA0643E), new Vector2(3, 3), new Vector2(13, 3), new Vector2(12, 13), new Vector2(4, 13)); cv.Set(5, 5, C(0xC08058)); return cv.Finish();
                case "rabbit_hide": cv.Poly(C(0xC8A078), new Vector2(3, 4), new Vector2(13, 3), new Vector2(12, 13), new Vector2(4, 12)); return cv.Finish();
                case "slime_ball": cv.Ellipse(8, 8.5f, 5, 5, C(0x7AC860)); cv.Ellipse(7, 7, 2, 2, C(0xB0F090)); return cv.Finish();
                case "magma_cream": cv.Ellipse(8, 8.5f, 5, 5, C(0xC84A20)); cv.Ellipse(8, 8.5f, 2.5f, 2.5f, C(0xF8B030)); return cv.Finish();
                case "ghast_tear": cv.Poly(C(0xD8F0F8), new Vector2(8, 2), new Vector2(11, 8), new Vector2(8, 13), new Vector2(5, 8)); return cv.Finish();
                case "nether_star": cv.Poly(C(0xF8F8D8), new Vector2(8, 1), new Vector2(10, 6), new Vector2(15, 8), new Vector2(10, 10), new Vector2(8, 15), new Vector2(6, 10), new Vector2(1, 8), new Vector2(6, 6)); cv.Ellipse(8, 8, 2, 2, C(0xFFFFF0)); return cv.Finish();
                case "shulker_shell": cv.Poly(C(0x9A6A9A), new Vector2(2, 12), new Vector2(3, 5), new Vector2(8, 2), new Vector2(13, 5), new Vector2(14, 12)); cv.Line(3, 9, 13, 9, C(0x6A4A6A)); return cv.Finish();
                case "phantom_membrane": cv.Poly(C(0xC8C0A0), new Vector2(2, 4), new Vector2(14, 3), new Vector2(12, 12), new Vector2(4, 13)); cv.Line(4, 5, 11, 11, C(0xA09880)); return cv.Finish();
                case "nautilus_shell": cv.Ellipse(8, 8, 6, 5, C(0xE8D8C0)); for (int i = 0; i < 4; i++) cv.Line(8, 8, 8 + (int)(Mathf.Cos(i * 1.6f) * 5), 8 + (int)(Mathf.Sin(i * 1.6f) * 4), C(0xB08A60)); return cv.Finish();
                case "heart_of_the_sea": cv.Ellipse(8, 8, 5, 5, C(0x2A7AD8)); cv.Ellipse(8, 8, 2.5f, 2.5f, C(0x70D8F8)); return cv.Finish();
                case "turtle_scute": cv.Poly(C(0x4AA04A), new Vector2(4, 4), new Vector2(12, 4), new Vector2(14, 9), new Vector2(8, 13), new Vector2(2, 9)); return cv.Finish();
                case "armadillo_scute": cv.Poly(C(0xC88A7A), new Vector2(4, 4), new Vector2(12, 4), new Vector2(14, 9), new Vector2(8, 13), new Vector2(2, 9)); return cv.Finish();
                case "honeycomb": for (int i = 0; i < 3; i++) cv.Poly(C(0xF0A830), new Vector2(3 + i * 3, 6 + (i % 2) * 3), new Vector2(6 + i * 3, 4 + (i % 2) * 3), new Vector2(9 + i * 3, 6 + (i % 2) * 3), new Vector2(9 + i * 3, 10 + (i % 2) * 3), new Vector2(6 + i * 3, 12 + (i % 2) * 3), new Vector2(3 + i * 3, 10 + (i % 2) * 3)); return cv.Finish();
                case "ink_sac": cv.Ellipse(8, 9, 5, 5, C(0x2A2A3A)); cv.Rect(7, 2, 2, 3, C(0x2A2A3A)); return cv.Finish();
                case "glow_ink_sac": cv.Ellipse(8, 9, 5, 5, C(0x2A8A8A)); cv.Rect(7, 2, 2, 3, C(0x2A8A8A)); cv.Ellipse(8, 9, 2, 2, C(0x8AF8E8)); return cv.Finish();
                case "paper": cv.Poly(C(0xF4F4F0), new Vector2(3, 2), new Vector2(13, 2), new Vector2(13, 14), new Vector2(3, 14)); for (int y = 5; y < 13; y += 2) cv.Line(5, y, 11, y, C(0xC8C8C0)); return cv.Finish();
                case "book": case "writable_book": case "written_book": case "enchanted_book":
                    {
                        var cover = n == "enchanted_book" ? C(0x7A3AA8) : n == "written_book" ? C(0x6A4A2A) : C(0x8A4A2A);
                        cv.Rect(3, 2, 10, 12, cover); cv.Rect(11, 3, 2, 10, C(0xF0E8D8)); cv.Line(3, 2, 3, 13, Sh(cover, 0.7f));
                        if (n == "writable_book") cv.Line(8, 11, 13, 5, C(0xF0F0F0));
                        if (n == "enchanted_book") { cv.Set(6, 6, C(0xE8C8FF)); cv.Set(7, 8, C(0xE8C8FF)); cv.Set(8, 5, C(0xE8C8FF)); }
                        return cv.Finish();
                    }
                case "map": case "filled_map": cv.Rect(2, 2, 12, 12, C(0xE8D8B0)); if (n == "filled_map") { cv.Rect(4, 4, 5, 4, C(0x6AA04A)); cv.Rect(8, 8, 4, 3, C(0x4A6AD8)); } else cv.Line(4, 8, 11, 8, C(0xB8A880)); return cv.Finish();
                case "compass": case "recovery_compass": cv.Ellipse(8, 8, 6, 6, n == "compass" ? C(0x9A9AA0) : C(0x3A6A7A)); cv.Ellipse(8, 8, 4.5f, 4.5f, C(0xE8E8E0)); cv.Line(8, 8, 8, 4, C(0xE02020)); cv.Line(8, 8, 8, 11, C(0x505050)); return cv.Finish();
                case "clock": cv.Ellipse(8, 8, 6, 6, C(0xF8D048)); cv.Ellipse(8, 8, 4.5f, 4.5f, C(0x3A7AD8)); cv.Rect(4, 8, 9, 4, C(0x2A7A2A)); cv.Ellipse(8, 5, 1.5f, 1.5f, C(0xFFE060)); return cv.Finish();
                case "spyglass": cv.Line(3, 12, 12, 3, C(0xC8784A), 2); cv.Line(10, 5, 13, 2, C(0xE8A060), 2); cv.Set(13, 2, C(0xA8E0F8)); return cv.Finish();
                case "brush": cv.Line(3, 12, 9, 6, Wood, 1); cv.Line(9, 6, 11, 4, C(0xC8784A), 2); cv.Poly(C(0xE8E0D0), new Vector2(11, 1), new Vector2(15, 2), new Vector2(14, 6), new Vector2(11, 5)); return cv.Finish();
                case "goat_horn": for (int i = 0; i < 10; i++) { float t = i / 9f; cv.Ellipse(3 + t * 10, 12 - Mathf.Sin(t * 2.6f) * 8, 1 + t * 1.5f, 1 + t * 1.5f, Mix(C(0xE8DCC0), C(0xA89878), t)); } return cv.Finish();
                case "name_tag": cv.Poly(C(0xE8D8B0), new Vector2(2, 6), new Vector2(5, 3), new Vector2(14, 3), new Vector2(14, 11), new Vector2(5, 11), new Vector2(2, 8)); cv.Set(4, 7, C(0x3A3A3A)); cv.Line(1, 7, 0, 10, C(0x8A6A3A)); return cv.Finish();
                case "lead": for (int i = 0; i < 20; i++) { float a = i / 20f * Mathf.PI * 2; cv.Set(8 + (int)(Mathf.Cos(a) * 5), 7 + (int)(Mathf.Sin(a) * 4), C(0xB89A6A)); } cv.Line(8, 11, 11, 15, C(0xB89A6A)); return cv.Finish();
                case "saddle": cv.Poly(C(0x8A4A28), new Vector2(2, 6), new Vector2(6, 4), new Vector2(10, 4), new Vector2(14, 6), new Vector2(12, 10), new Vector2(4, 10)); cv.Line(5, 10, 5, 14, C(0x4A4A4A)); cv.Line(11, 10, 11, 14, C(0x4A4A4A)); cv.Rect(4, 13, 3, 2, C(0xB0B0B0)); cv.Rect(10, 13, 3, 2, C(0xB0B0B0)); return cv.Finish();
                case "harness": cv.Poly(C(0x8A5A3A), new Vector2(2, 5), new Vector2(14, 5), new Vector2(13, 10), new Vector2(3, 10)); cv.Rect(6, 6, 4, 3, C(0xA8D8F0)); return cv.Finish();
                case "flower_banner_pattern": cv.Rect(3, 2, 10, 12, C(0xE8D8B0)); cv.Ellipse(8, 8, 3, 3, C(0x8A6A3A)); return cv.Finish();
                case "trial_key": case "ominous_trial_key": { var k = n == "trial_key" ? C(0xB0A080) : C(0x6A8AA0); cv.Ellipse(5, 5, 3, 3, k); cv.Ellipse(5, 5, 1.2f, 1.2f, default); cv.Line(7, 7, 13, 13, k, 1); cv.Line(11, 11, 12, 10, k); cv.Line(13, 13, 14, 12, k); return cv.Finish(); }
                case "resin_clump": cv.Ellipse(8, 9, 5, 4, C(0xE07A20)); cv.Ellipse(7, 7, 2, 2, C(0xF8B050)); return cv.Finish();
                case "rabbit_foot": cv.Ellipse(7, 10, 3, 4, C(0xC8A078)); cv.Rect(9, 2, 2, 6, C(0xB89068)); return cv.Finish();
                case "disc_fragment_5": cv.Poly(C(0x2E2E2E), new Vector2(3, 3), new Vector2(13, 5), new Vector2(8, 13)); cv.Set(7, 6, C(0x6ACAD8)); return cv.Finish();
                case "firework_star": cv.Ellipse(8, 8, 4, 4, C(0x7A7A7A)); cv.Set(6, 6, C(0xE83A3A)); cv.Set(9, 7, C(0x3AE83A)); cv.Set(7, 10, C(0x3A3AE8)); return cv.Finish();
                case "firework_rocket": cv.Rect(6, 4, 4, 9, C(0xD83A2A)); cv.Rect(6, 7, 4, 2, C(0xE8E0D0)); cv.Poly(C(0x7A7A7A), new Vector2(6, 4), new Vector2(8, 1), new Vector2(10, 4)); cv.Line(8, 13, 8, 15, C(0x8A6A3A)); return cv.Finish();
                case "totem_of_undying": cv.Rect(5, 1, 6, 5, C(0xF0D048)); cv.Set(6, 3, C(0x2A8A2A)); cv.Set(9, 3, C(0x2A8A2A)); cv.Rect(3, 6, 10, 3, C(0xE0B830)); cv.Rect(5, 9, 6, 6, C(0xF0D048)); return cv.Finish();
                case "egg": cv.Ellipse(8, 9, 4, 5.5f, C(0xE8D8B8)); cv.Set(9, 7, C(0xC8A878)); return cv.Finish();
                case "snowball": cv.Ellipse(8, 8.5f, 4.5f, 4.5f, C(0xF8F8F8)); return cv.Finish();
                case "wind_charge": cv.Ellipse(8, 8.5f, 4.5f, 4.5f, C(0xC8D8F8)); for (int i = 0; i < 3; i++) cv.Line(4 + i * 2, 6 + i * 2, 11 - i, 5 + i * 3, C(0x8AA8E8)); return cv.Finish();
                case "fire_charge": cv.Ellipse(8, 8.5f, 4.5f, 4.5f, C(0x3A2A20)); cv.Set(6, 7, C(0xF8A020)); cv.Set(9, 9, C(0xF86020)); cv.Set(8, 6, C(0xF8D040)); return cv.Finish();
                case "ender_pearl": cv.Ellipse(8, 8.5f, 4.5f, 4.5f, C(0x1A6A5A)); cv.Ellipse(8, 8.5f, 2.5f, 2.5f, C(0x2AA890)); cv.Set(7, 7, C(0x8AF0D8)); return cv.Finish();
                case "ender_eye": cv.Ellipse(8, 8.5f, 4.5f, 4.5f, C(0x2A8A5A)); cv.Ellipse(8, 8.5f, 2.5f, 1.5f, C(0xD8F8A0)); cv.Rect(8, 7, 1, 3, C(0x103010)); return cv.Finish();
                case "flint_and_steel": cv.Poly(C(0x4A4A4A), new Vector2(8, 8), new Vector2(13, 9), new Vector2(12, 14), new Vector2(8, 13)); cv.Line(3, 8, 6, 3, C(0xB0B0B0), 2); cv.Line(6, 3, 9, 5, C(0xB0B0B0), 1); return cv.Finish();
                case "shears": cv.Line(4, 12, 11, 3, C(0xE0E0E0), 1); cv.Line(5, 3, 12, 12, C(0xD0D0D0), 1); cv.Ellipse(4, 12, 2, 2, C(0x8A6A3A)); cv.Ellipse(12, 12, 2, 2, C(0x8A6A3A)); return cv.Finish();
                case "fishing_rod": cv.Line(2, 14, 12, 2, Wood, 1); for (int y = 3; y < 12; y++) cv.Set(13, y, C(0xE0E0E0), true); cv.Set(13, 12, C(0xB0B0B0)); return cv.Finish();
                case "carrot_on_a_stick": cv.Line(2, 14, 12, 2, Wood, 1); cv.Line(13, 3, 13, 9, C(0xE0E0E0)); cv.Poly(C(0xF08A20), new Vector2(11, 9), new Vector2(15, 9), new Vector2(13, 15)); return cv.Finish();
                case "warped_fungus_on_a_stick": cv.Line(2, 14, 12, 2, Wood, 1); cv.Line(13, 3, 13, 9, C(0xE0E0E0)); cv.Ellipse(13, 11, 2.5f, 2, C(0x2AA890)); return cv.Finish();
                case "bow": case "bow_pulling_0": case "bow_pulling_1": case "bow_pulling_2":
                    {
                        int pull = n == "bow" ? -1 : n[n.Length - 1] - '0';
                        for (int i = 0; i <= 16; i++) { float a = Mathf.Lerp(-1.3f, 1.3f, i / 16f); int x = 4 + (int)Mathf.Round(Mathf.Cos(a) * 6), y = 8 + (int)Mathf.Round(Mathf.Sin(a) * 7); cv.Set(x, y, Wood); cv.Set(x + 1, y, WoodD); }
                        int sx = 4 - Math.Max(0, pull + 1);
                        cv.Line(6, 1, sx, 8, C(0xE8E8E8)); cv.Line(sx, 8, 6, 15, C(0xE8E8E8));
                        if (pull >= 0) { cv.Line(sx, 8, 14, 8, C(0x8A6A3A)); cv.Set(14, 8, C(0xB0B0B0)); cv.Set(15, 8, C(0xB0B0B0)); }
                        return cv.Finish();
                    }
                case "crossbow": case "crossbow_charged":
                    cv.Line(3, 12, 12, 3, Wood, 2); cv.Line(2, 5, 10, 13, C(0x6A6A6A), 1); cv.Line(2, 5, 7, 7, C(0xE0E0E0)); cv.Line(10, 13, 8, 8, C(0xE0E0E0));
                    if (n.EndsWith("charged")) cv.Line(5, 10, 13, 2, C(0xB0B0B0));
                    return cv.Finish();
                case "arrow": case "spectral_arrow": case "tipped_arrow":
                    {
                        cv.Line(3, 12, 12, 3, Wood, 1);
                        var head = n == "spectral_arrow" ? C(0xF8D040) : C(0xB8B8B8);
                        cv.Poly(head, new Vector2(10, 2.5f), new Vector2(14, 1), new Vector2(13.5f, 5.5f));
                        cv.Line(2, 11, 4, 13, n == "spectral_arrow" ? C(0xF8E080) : C(0xF0F0F0)); cv.Line(2, 13, 3, 14, C(0xE0E0E0));
                        return cv.Finish();
                    }
                case "tipped_arrow_head": cv.Poly(White, new Vector2(10, 2.5f), new Vector2(14, 1), new Vector2(13.5f, 5.5f)); return cv.Finish(false);
                case "trident": cv.Line(2, 14, 11, 5, C(0x3A8A8A), 1); cv.Line(3, 14, 12, 5, C(0x2A6A6A), 1); cv.Line(9, 3, 13, 7, C(0x5AC8B8), 1); cv.Line(11, 5, 14, 2, C(0x5AC8B8), 1); cv.Line(9, 3, 10, 1, C(0x5AC8B8)); cv.Line(13, 7, 15, 6, C(0x5AC8B8)); return cv.Finish();
                case "shield": Shield(cv, C(0xA0824E), C(0x8A8A90)); cv.Line(8, 2, 8, 11, C(0x7A5E34)); return cv.Finish();
                case "mace": cv.Line(2, 14, 8, 8, C(0x7A6A58), 1); cv.Line(3, 14, 9, 8, C(0x5A4A3A), 1); cv.Ellipse(10.5f, 5.5f, 3.5f, 3.5f, C(0x6A6A70)); cv.Set(10, 1, C(0x9A9AA0)); cv.Set(14, 5, C(0x9A9AA0)); cv.Set(7, 5, C(0x9A9AA0)); cv.Set(11, 9, C(0x9A9AA0)); return cv.Finish();
                case "elytra": cv.Poly(C(0x8A8AA8), new Vector2(7, 2), new Vector2(2, 5), new Vector2(3, 14), new Vector2(7, 10)); cv.Poly(C(0x9A9AB8), new Vector2(9, 2), new Vector2(14, 5), new Vector2(13, 14), new Vector2(9, 10)); return cv.Finish();
                case "end_crystal": cv.Ellipse(8, 8, 5, 5, C(0xC8A8E8)); cv.Rect(6, 6, 4, 4, C(0xF070A8)); cv.Rect(7, 7, 2, 2, C(0xFFD0F0)); return cv.Finish();
                case "armor_stand": cv.Rect(7, 2, 2, 11, Wood); cv.Rect(4, 4, 8, 1, Wood); cv.Rect(5, 9, 6, 1, Wood); cv.Rect(3, 13, 10, 2, C(0xA0A0A0)); return cv.Finish();
                case "item_frame": cv.Rect(2, 2, 12, 12, C(0x8A6A3A)); cv.Rect(4, 4, 8, 8, C(0xA0643E)); return cv.Finish();
                case "painting": cv.Rect(1, 3, 14, 10, C(0x8A6A3A)); cv.Rect(2, 4, 12, 8, C(0x6AA0D8)); cv.Rect(2, 9, 12, 3, C(0x4A8A3A)); cv.Ellipse(11, 6, 1.5f, 1.5f, C(0xF8E060)); return cv.Finish();
                case "bowl": cv.Tmpl(new[] { "", "", "", "", "", "", "..mmmmmmmmmmmm", "..mddddddddddm", "...mmmmmmmmmm", "....mmmmmmmm", ".....mmmmmm" }, P('m', C(0x8A6A3A), 'd', C(0x5E4524))); return cv.Finish();
                case "mushroom_stew": case "beetroot_soup": case "rabbit_stew": case "suspicious_stew":
                    {
                        var soup = n == "beetroot_soup" ? C(0xA02030) : n == "rabbit_stew" ? C(0xB07040) : n == "suspicious_stew" ? C(0xA07A5A) : C(0xB08860);
                        cv.Tmpl(new[] { "", "", "", "", "", "", "..mmmmmmmmmmmm", "..mssssssssssm", "...mmmmmmmmmm", "....mmmmmmmm", ".....mmmmmm" }, P('m', C(0x8A6A3A), 's', soup));
                        cv.Ellipse(8, 6.5f, 5, 1.5f, soup);
                        return cv.Finish();
                    }
                // ---- food
                case "apple": case "golden_apple": case "enchanted_golden_apple": { var c = n == "apple" ? C(0xD82A20) : C(0xF8D040); cv.Ellipse(8, 9, 5, 5, c); cv.Line(8, 2, 8, 5, C(0x5A3A1A)); cv.Rect(9, 3, 3, 2, C(0x3AA02A)); cv.Set(6, 7, Sh(c, 1.4f)); return cv.Finish(); }
                case "bread": cv.Ellipse(8, 9, 7, 3.5f, C(0xC89040)); for (int i = 0; i < 3; i++) cv.Line(4 + i * 3, 7, 6 + i * 3, 10, C(0xE8C080)); return cv.Finish();
                case "cookie": cv.Ellipse(8, 8.5f, 5, 5, C(0xC8883A)); cv.Set(6, 7, C(0x4A2A10)); cv.Set(9, 9, C(0x4A2A10)); cv.Set(7, 11, C(0x4A2A10)); cv.Set(10, 6, C(0x4A2A10)); return cv.Finish();
                case "pumpkin_pie": cv.Ellipse(8, 9, 6, 4, C(0xE8A050)); cv.Ellipse(8, 8, 4, 2.5f, C(0xD86A20)); return cv.Finish();
                case "cake": cv.Rect(2, 7, 12, 6, C(0xF8F0E8)); cv.Rect(2, 6, 12, 2, C(0xF8F8F8)); for (int x = 3; x < 13; x += 3) cv.Set(x, 6, C(0xE02020)); cv.Rect(2, 11, 12, 2, C(0xC8883A)); return cv.Finish();
                case "beef": case "porkchop": case "mutton": case "chicken": case "rabbit":
                case "cooked_beef": case "cooked_porkchop": case "cooked_mutton": case "cooked_chicken": case "cooked_rabbit":
                    {
                        bool cooked = n.StartsWith("cooked_");
                        string kind = cooked ? n.Substring(7) : n;
                        Color32 c = kind == "beef" ? (cooked ? C(0x7A4A2A) : C(0xD84040)) : kind == "porkchop" ? (cooked ? C(0xC89A6A) : C(0xF0A0A0)) : kind == "mutton" ? (cooked ? C(0x9A5A3A) : C(0xD85A4A)) : kind == "chicken" ? (cooked ? C(0xD8A060) : C(0xF0C8B0)) : (cooked ? C(0xA87040) : C(0xE0A090));
                        if (kind == "chicken") { cv.Ellipse(9, 7, 4.5f, 4, c); cv.Line(6, 10, 3, 13, C(0xF0F0E0), 2); }
                        else if (kind == "mutton" || kind == "rabbit") { cv.Ellipse(9, 8, 4, 5, c); cv.Line(6, 11, 3, 14, C(0xF0F0E0), 1); }
                        else { cv.Ellipse(8, 8.5f, 6, 4.5f, c); cv.Ellipse(9, 8, 3, 2, Sh(c, cooked ? 0.8f : 1.15f)); if (kind == "beef") { cv.Line(4, 7, 11, 10, C(0xF0E0D0)); } }
                        return cv.Finish();
                    }
                case "cod": case "cooked_cod": case "salmon": case "cooked_salmon": case "tropical_fish": case "pufferfish":
                    {
                        Color32 c = n == "cod" ? C(0xB89A6A) : n == "cooked_cod" ? C(0xD8C8A0) : n == "salmon" ? C(0xA83A30) : n == "cooked_salmon" ? C(0xC87A50) : n == "tropical_fish" ? C(0xF0902A) : C(0xE8C040);
                        if (n == "pufferfish") { cv.Ellipse(8, 8, 5, 5, c); for (int i = 0; i < 8; i++) { float a = i * 0.785f; cv.Set(8 + (int)(Mathf.Cos(a) * 6), 8 + (int)(Mathf.Sin(a) * 6), C(0xF0E0A0)); } cv.Set(10, 7, Black); return cv.Finish(); }
                        cv.Ellipse(7, 8, 5.5f, 3, c);
                        cv.Poly(Sh(c, 0.85f), new Vector2(11, 8), new Vector2(15, 4.5f), new Vector2(15, 11.5f));
                        cv.Set(4, 7, Black, true);
                        if (n == "tropical_fish") { cv.Line(6, 5, 6, 11, C(0xF8F8F8)); cv.Line(9, 5, 9, 11, C(0xF8F8F8)); }
                        return cv.Finish();
                    }
                case "potato": case "baked_potato": case "poisonous_potato": { var c = n == "potato" ? C(0xD8B060) : n == "baked_potato" ? C(0xC89040) : C(0xB8C060); cv.Ellipse(8, 8.5f, 5, 4, c); cv.Set(6, 7, Sh(c, 0.7f)); cv.Set(10, 9, Sh(c, 0.7f)); return cv.Finish(); }
                case "carrot": case "golden_carrot": { var c = n == "carrot" ? C(0xF08A20) : C(0xF8D040); cv.Poly(c, new Vector2(3, 13), new Vector2(10, 4), new Vector2(13, 7)); cv.Line(10, 4, 13, 1, C(0x3AA02A)); cv.Line(11, 5, 14, 3, C(0x3AA02A)); return cv.Finish(); }
                case "beetroot": cv.Ellipse(8, 10, 4.5f, 4.5f, C(0xA02030)); cv.Line(8, 5, 6, 1, C(0x3AA02A)); cv.Line(8, 5, 10, 1, C(0x3AA02A)); return cv.Finish();
                case "melon_slice": cv.Poly(C(0xE83A3A), new Vector2(2, 13), new Vector2(14, 13), new Vector2(8, 3)); cv.Line(2, 13, 14, 13, C(0x3AA02A)); cv.Set(7, 9, Black); cv.Set(9, 10, Black); cv.Set(8, 7, Black); return cv.Finish();
                case "glistering_melon_slice": cv.Poly(C(0xE8603A), new Vector2(2, 13), new Vector2(14, 13), new Vector2(8, 3)); cv.Line(2, 13, 14, 13, C(0xF8D040)); cv.Set(7, 9, C(0xF8E080)); cv.Set(9, 10, C(0xF8E080)); return cv.Finish();
                case "sweet_berries": cv.Ellipse(6, 9, 2.5f, 2.5f, C(0xC0203A)); cv.Ellipse(10, 10, 2.5f, 2.5f, C(0xC0203A)); cv.Ellipse(8, 6, 2.5f, 2.5f, C(0xD0304A)); cv.Line(8, 2, 8, 4, C(0x3A7A2A)); return cv.Finish();
                case "glow_berries": cv.Ellipse(6, 9, 2.5f, 2.5f, C(0xF8A030)); cv.Ellipse(10, 10, 2.5f, 2.5f, C(0xF8A030)); cv.Line(8, 2, 8, 7, C(0x3A7A2A)); return cv.Finish();
                case "rotten_flesh": cv.Poly(C(0x8A6A3A), new Vector2(3, 5), new Vector2(12, 3), new Vector2(13, 11), new Vector2(5, 13)); cv.Set(6, 7, C(0x5A8A3A)); cv.Set(10, 9, C(0x5A8A3A)); return cv.Finish();
                case "spider_eye": case "fermented_spider_eye": { var c = n == "spider_eye" ? C(0xA02030) : C(0x8A5040); cv.Ellipse(8, 8.5f, 5, 5, c); cv.Ellipse(9, 7, 2, 2, C(0xF8F8F8)); cv.Set(9, 7, Black, true); return cv.Finish(); }
                case "chorus_fruit": cv.Ellipse(8, 8.5f, 5, 5, C(0x8A5A8A)); cv.Set(6, 6, C(0xC8A0C8)); cv.Set(10, 9, C(0xC8A0C8)); return cv.Finish();
                case "popped_chorus_fruit": cv.Ellipse(8, 8.5f, 5, 5, C(0xA880A8)); cv.Ellipse(8, 8, 2, 2, C(0xE0C8E0)); return cv.Finish();
                case "dried_kelp": cv.Poly(C(0x3A4A2A), new Vector2(4, 3), new Vector2(12, 4), new Vector2(11, 13), new Vector2(5, 12)); return cv.Finish();
                case "wheat": for (int i = 0; i < 3; i++) cv.Line(5 + i * 2, 14, 7 + i * 2, 3, C(0xD8B048), 1); cv.Line(4, 10, 12, 10, C(0xB08A30)); return cv.Finish();
                case "wheat_seeds": case "pumpkin_seeds": case "melon_seeds": case "beetroot_seeds":
                    {
                        var c = n == "wheat_seeds" ? C(0x5A9A2A) : n == "pumpkin_seeds" ? C(0xE8D8A0) : n == "melon_seeds" ? C(0x2A2A1A) : C(0x9A6A3A);
                        var r = new RNG(Hash.StringHash(n));
                        for (int i = 0; i < 7; i++) { int x = 3 + r.Next(10), y = 4 + r.Next(9); cv.Set(x, y, c); cv.Set(x + 1, y, Sh(c, 1.2f)); }
                        return cv.Finish();
                    }
                case "cocoa_beans": cv.Ellipse(8, 8.5f, 3, 4.5f, C(0x7A4A2A)); cv.Line(8, 5, 8, 12, C(0x5A3A1A)); return cv.Finish();
                case "nether_wart": cv.Ellipse(8, 10, 5, 3, C(0x8A1A1A)); cv.Ellipse(6, 7, 2, 2, C(0xA02020)); cv.Ellipse(10, 7, 2, 2, C(0xA02020)); return cv.Finish();
                case "sugar_cane": cv.Line(5, 15, 5, 1, C(0x8AC860), 1); cv.Line(9, 15, 9, 3, C(0x7AB850), 1); cv.Set(6, 6, C(0x5A9A3A)); cv.Set(10, 9, C(0x5A9A3A)); return cv.Finish();
                case "kelp": cv.Line(7, 15, 8, 1, C(0x3A8A2A), 1); cv.Line(8, 12, 11, 9, C(0x4A9A3A)); cv.Line(8, 7, 5, 4, C(0x4A9A3A)); return cv.Finish();
                case "milk_bucket": Bucket(cv, n); return cv.Finish();
                // ---- block items drawn flat
                case "redstone_wire": Dust(cv, C(0xD01A10)); return cv.Finish();
                case "repeater": case "comparator": cv.Rect(1, 9, 14, 4, C(0xA0A0A0)); cv.Rect(4, 5, 2, 5, C(0xD02020)); cv.Rect(10, 5, 2, 5, C(0xD02020)); if (n == "comparator") cv.Rect(7, 3, 2, 3, C(0xD02020)); return cv.Finish();
                case "lantern": case "soul_lantern": case "copper_lantern": { var g = n == "soul_lantern" ? C(0x60E0F0) : n == "copper_lantern" ? C(0x80F0A0) : C(0xF8C060); cv.Rect(5, 5, 6, 8, C(0x3A3A40)); cv.Rect(6, 6, 4, 6, g); cv.Rect(6, 3, 4, 2, C(0x3A3A40)); cv.Line(8, 0, 8, 2, C(0x5A5A60)); return cv.Finish(); }
                case "iron_chain": case "copper_chain": { var c = n == "iron_chain" ? C(0x5A5A6A) : C(0xB86A40); for (int y = 1; y < 15; y += 4) { cv.Rect(7, y, 2, 3, c); cv.Set(6, y + 2, c); cv.Set(9, y + 2, c); } return cv.Finish(); }
                case "bell": cv.Poly(C(0xF0C838), new Vector2(4, 12), new Vector2(5, 5), new Vector2(8, 3), new Vector2(11, 5), new Vector2(12, 12)); cv.Rect(3, 12, 10, 2, C(0xD8A828)); cv.Rect(7, 1, 2, 2, C(0x6A6A6A)); return cv.Finish();
                case "brewing_stand": cv.Line(8, 2, 8, 12, C(0xB0A060)); cv.Rect(3, 12, 10, 2, C(0x6A6A6A)); cv.Ellipse(4, 10, 1.5f, 1.5f, C(0xC8D8E8)); cv.Ellipse(12, 10, 1.5f, 1.5f, C(0xC8D8E8)); return cv.Finish();
                case "cauldron": cv.Tmpl(new[] { "", "", "", "..mmmmmmmmmmmm", "..mddddddddddm", "..mddddddddddm", "..mmmmmmmmmmmm", "..mmmmmmmmmmmm", "..mmmmmmmmmmmm", "..mmmmmmmmmmmm", "..mm........mm", "..mm........mm" }, P('m', C(0x4A4A50), 'd', C(0x2A2A2E))); return cv.Finish();
                case "hopper": cv.Poly(C(0x4A4A50), new Vector2(1, 2), new Vector2(15, 2), new Vector2(15, 7), new Vector2(11, 11), new Vector2(9, 15), new Vector2(7, 15), new Vector2(5, 11), new Vector2(1, 7)); cv.Rect(3, 3, 10, 2, C(0x2A2A2E)); return cv.Finish();
                case "flower_pot": cv.Tmpl(new[] { "", "", "", "", "", "", "....mmmmmmmm", "....mddddddm", ".....mmmmmm", ".....mmmmmm", ".....mmmmmm", ".....mmmmmm" }, P('m', C(0xA85A3A), 'd', C(0x5A3A2A))); return cv.Finish();
                case "campfire": case "soul_campfire": cv.Line(2, 13, 13, 9, C(0x6A5030), 2); cv.Line(2, 9, 13, 13, C(0x5A4428), 2); Flame(cv, n == "campfire" ? C(0xF8A020) : C(0x60D8F0)); return cv.Finish();
                case "nether_wart_block": return null;
                case "cave_vines": cv.Line(8, 0, 8, 15, C(0x5A8A2A)); cv.Ellipse(8, 12, 2, 2, C(0xF8A030)); return cv.Finish();
                case "sweet_berry_bush": cv.Ellipse(8, 9, 6, 5, C(0x3A6A2A)); cv.Set(6, 7, C(0xC0203A)); cv.Set(10, 10, C(0xC0203A)); return cv.Finish();
                case "cocoa": cv.Ellipse(8, 9, 3, 4.5f, C(0xB0703A)); return cv.Finish();
                case "pointed_dripstone": cv.Poly(C(0x9A7A64), new Vector2(5, 1), new Vector2(11, 1), new Vector2(8, 15)); return cv.Finish();
                case "frogspawn": for (int i = 0; i < 6; i++) { cv.Ellipse(4 + (i % 3) * 4, 5 + (i / 3) * 5, 1.5f, 1.5f, C(0x5A5A6A)); } return cv.Finish();
                case "wheat_crop": return null;
                case "carrots": case "potatoes": case "beetroots": case "pumpkin_stem": case "melon_stem": cv.Line(8, 15, 8, 6, C(0x3A8A2A)); cv.Line(8, 9, 5, 6, C(0x4A9A3A)); cv.Line(8, 8, 11, 5, C(0x4A9A3A)); return cv.Finish();
            }
            if (n.StartsWith("bow")) return null;
            return null;
        }

        static void Flame(Cv cv, Color32 c)
        {
            cv.Poly(c, new Vector2(5, 11), new Vector2(7, 3), new Vector2(8, 6), new Vector2(10, 2), new Vector2(11, 11));
            cv.Poly(Sh(c, 1.3f), new Vector2(7, 10), new Vector2(8, 6), new Vector2(9, 10));
        }
    }
}
