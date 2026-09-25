using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Original procedural skin painters for the cuboid models. Each painter fills its model's box-UV regions with
    /// flat pixel-art colours, markings, faces and glow pixels (alpha 0.75 marks emissive in MCR/Entity).
    /// </summary>
    public static class SkinPainter
    {
        static Color32 C(uint rgb, byte a = 255) => new Color32((byte)(rgb >> 16), (byte)(rgb >> 8), (byte)rgb, a);
        static Color32 Sh(Color32 c, float f) => Img.Shade(c, f);
        static Color32 Glow(Color32 c) { c.a = 190; return c; }

        public static void Paint(string skin, string variantSalt, Color32[] px, int w, int h)
        {
            var def = MobModels.Get(skin);
            var cv = new SkinCanvas(px, w, h, skin + (variantSalt ?? ""));
            if (def == null) { for (int i = 0; i < px.Length; i++) px[i] = new Color32(255, 0, 255, 255); return; }
            if (PropSkins.Handles(skin)) { PropSkins.Paint(cv, def, skin, variantSalt); return; }
            int variant = 0;
            if (variantSalt != null) int.TryParse(variantSalt, out variant);
            switch (skin)
            {
                // ---------------- humanoids
                case "player": case "zombie": case "husk": case "drowned": case "zombie_villager": case "copper_golem": Humanoid(cv, def, skin); break;
                case "skeleton": case "stray": case "bogged": case "parched": case "wither_skeleton": SkeletonSkin(cv, def, skin); break;
                case "piglin": case "piglin_brute": case "zombified_piglin": PiglinSkin(cv, def, skin); break;
                case "villager": case "wandering_trader": case "zombie_villager_": VillagerSkin(cv, def, skin, variantSalt); break;
                case "pillager": case "vindicator": case "evoker": IllagerSkin(cv, def, skin); break;
                case "witch": WitchSkin(cv, def); break;
                case "enderman": EndermanSkin(cv, def); break;
                case "creeper": CreeperSkin(cv, def); break;
                case "warden": WardenSkin(cv, def); break;
                case "creaking": CreakingSkin(cv, def); break;
                case "iron_golem": GolemSkin(cv, def); break;
                case "snow_golem": SnowGolemSkin(cv, def); break;
                case "allay": case "vex": AllaySkin(cv, def, skin); break;
                // ---------------- animals
                case "pig": PigSkin(cv, def); break;
                case "cow": case "mooshroom": CowSkin(cv, def, skin); break;
                case "sheep": SheepSkin(cv, def, variantSalt); break;
                case "chicken": ChickenSkin(cv, def); break;
                case "rabbit": RabbitSkin(cv, def, variantSalt); break;
                case "horse": case "donkey": case "mule": case "skeleton_horse": case "zombie_horse": HorseSkin(cv, def, skin, variantSalt); break;
                case "llama": case "trader_llama": LlamaSkin(cv, def, skin, variantSalt); break;
                case "camel": case "camel_husk": CamelSkin(cv, def, skin); break;
                case "wolf": WolfSkin(cv, def); break;
                case "cat": case "ocelot": CatSkin(cv, def, skin, variantSalt); break;
                case "fox": FoxSkin(cv, def, variantSalt); break;
                case "goat": GoatSkin(cv, def, variantSalt); break;
                case "polar_bear": PolarBearSkin(cv, def); break;
                case "panda": PandaSkin(cv, def, variantSalt); break;
                case "turtle": TurtleSkin(cv, def); break;
                case "frog": FrogSkin(cv, def, variantSalt); break;
                case "axolotl": AxolotlSkin(cv, def, variantSalt); break;
                case "armadillo": ArmadilloSkin(cv, def); break;
                case "sniffer": SnifferSkin(cv, def); break;
                case "hoglin": case "zoglin": HoglinSkin(cv, def, skin); break;
                case "ravager": RavagerSkin(cv, def); break;
                case "strider": StriderSkin(cv, def, variantSalt); break;
                // ---------------- others
                case "spider": case "cave_spider": SpiderSkin(cv, def, skin); break;
                case "slime": case "magma_cube": case "sulfur_cube": SlimeSkin(cv, def, skin); break;
                case "ghast": case "happy_ghast": case "ghastling": GhastSkin(cv, def, skin); break;
                case "blaze": BlazeSkin(cv, def); break;
                case "breeze": BreezeSkin(cv, def); break;
                case "shulker": ShulkerSkin(cv, def, variantSalt); break;
                case "bat": BatSkin(cv, def); break;
                case "bee": BeeSkin(cv, def); break;
                case "parrot": ParrotSkin(cv, def); break;
                case "squid": case "glow_squid": SquidSkin(cv, def, skin); break;
                case "cod": case "salmon": case "tropical_fish": FishSkin(cv, def, skin, variantSalt); break;
                case "pufferfish": PufferSkin(cv, def); break;
                case "dolphin": DolphinSkin(cv, def); break;
                case "guardian": case "elder_guardian": GuardianSkin(cv, def, skin); break;
                case "phantom": PhantomSkin(cv, def); break;
                case "silverfish": case "endermite": BugSkin(cv, def, skin); break;
                case "tadpole": TadpoleSkin(cv, def); break;
                case "nautilus": case "zombie_nautilus": NautilusSkin(cv, def, skin); break;
                case "ender_dragon": DragonSkin(cv, def); break;
                case "wither": WitherSkin(cv, def); break;
                default: Fallback(cv, def); break;
            }
            // outline all cube edges with a darker tone so silhouettes read
            try { Outline(cv, def); } catch { }
        }

        // ================================================================= helpers
        static readonly int[] FRONT = { 3 }, BACK = { 5 }, SIDES = { 2, 4 }, TOP = { 0 }, BOTTOM = { 1 };

        /// <summary>
        /// Outer layers (hat, jacket, sleeves) sit a fraction of a pixel outside the base box. Solid fills skip them:
        /// painted opaque they would bury the face and clothing under a plain shell.
        /// </summary>
        static bool IsOverlay(CubeDef c) => c.name == "hat" || c.name == "jacket" || c.name == "sleeve" || c.name == "pants_layer";

        static void Box(SkinCanvas cv, BoneDef b, Color32 baseC, float noise = 0.07f)
        {
            if (b == null) return;
            foreach (var c in b.cubes) if (!IsOverlay(c)) cv.AllFaces(c, baseC, noise);
        }
        static void BoxFront(SkinCanvas cv, BoneDef b, Color32 c, float noise = 0.07f)
        {
            if (b == null) return;
            foreach (var cube in b.cubes) if (!IsOverlay(cube)) cv.Face(cube, 3, c, noise);
        }
        static void BoxSides(SkinCanvas cv, BoneDef b, Color32 c, float noise = 0.07f)
        {
            if (b == null) return;
            foreach (var cube in b.cubes) if (!IsOverlay(cube)) { cv.Face(cube, 2, c, noise); cv.Face(cube, 4, c, noise); }
        }
        static void BoxTop(SkinCanvas cv, BoneDef b, Color32 c, float noise = 0.07f)
        {
            if (b == null) return;
            foreach (var cube in b.cubes) if (!IsOverlay(cube)) cv.Face(cube, 0, c, noise);
        }
        static void BoxBottom(SkinCanvas cv, BoneDef b, Color32 c, float noise = 0.07f)
        {
            if (b == null) return;
            foreach (var cube in b.cubes) if (!IsOverlay(cube)) cv.Face(cube, 1, c, noise);
        }

        /// <summary>Paints the top <paramref name="rows"/> pixel rows of the four side faces of a bone's base cube.</summary>
        static void SideBand(SkinCanvas cv, BoneDef b, int fromRow, int rows, Color32 c, float noise = 0.06f)
        {
            if (b == null) return;
            foreach (var cube in b.cubes)
            {
                if (IsOverlay(cube)) continue;
                for (int f = 2; f <= 5; f++)
                {
                    var r = cv.R(cube, f);
                    int y0 = Mathf.Clamp(fromRow, 0, r.height), y1 = Mathf.Clamp(fromRow + rows, 0, r.height);
                    if (y1 > y0) cv.Rect(r.x, r.y + y0, r.width, y1 - y0, c, noise);
                }
            }
        }
        static void Mark(SkinCanvas cv, BoneDef b, int face, Color32 c, float density = 0.2f)
        {
            if (b == null) return;
            foreach (var cube in b.cubes) { var r = cv.R(cube, face); cv.Speck(r.x, r.y, r.width, r.height, c, density); }
        }

        /// <summary>Draw two eyes on the front face of a head cube.</summary>
        static void Eyes(SkinCanvas cv, BoneDef head, int cubeIndex, Color32 white, Color32 pupil, float eyeSizeFrac = 0.22f, bool glow = false, bool angry = false)
        {
            if (head == null || cubeIndex >= head.cubes.Count) return;
            var cube = head.cubes[cubeIndex];
            var r = cv.R(cube, 3);
            int ew = Mathf.Max(1, Mathf.RoundToInt(r.width * eyeSizeFrac));
            int eh = Mathf.Max(1, Mathf.RoundToInt(r.height * eyeSizeFrac));
            int y = r.y + Mathf.RoundToInt(r.height * 0.32f);
            int x1 = r.x + Mathf.RoundToInt(r.width * 0.10f);
            int x2 = r.x + r.width - Mathf.RoundToInt(r.width * 0.10f) - ew;
            for (int side = 0; side < 2; side++)
            {
                int x = side == 0 ? x1 : x2;
                cv.Fill(x, y, ew, eh, glow ? Glow(white) : white);
                int pw = Mathf.Max(1, ew / 2), ph = Mathf.Max(1, eh / 2);
                int pxo = side == 0 ? x + pw / 2 : x + ew - pw - pw / 2;
                cv.Fill(pxo, y + (eh - ph) / 2, pw, ph, glow ? Glow(pupil) : pupil);
                if (angry) cv.Fill(x, y + eh, ew, 1, Sh(pupil, 0.7f));
            }
        }

        static void Mouth(SkinCanvas cv, BoneDef head, int cubeIndex, Color32 c, float wFrac = 0.5f)
        {
            if (head == null || cubeIndex >= head.cubes.Count) return;
            var cube = head.cubes[cubeIndex];
            var r = cv.R(cube, 3);
            int mw = Mathf.RoundToInt(r.width * wFrac);
            int mh = Mathf.Max(1, Mathf.RoundToInt(r.height * 0.12f));
            cv.Fill(r.x + (r.width - mw) / 2, r.y + r.height - mh - Mathf.RoundToInt(r.height * 0.18f), mw, mh, c);
        }

        static void Outline(SkinCanvas cv, ModelDef def)
        {
            // darken border pixels of each face region so cube edges are readable
            foreach (var b in def.bones)
                foreach (var c in b.cubes)
                {
                    if (c.translucent) continue;
                    var faces = ModelDef.BoxFaces(c);
                    for (int f = 0; f < 6; f++)
                    {
                        var r = faces[f];
                        if (r.width <= 1 || r.height <= 1) continue;
                        for (int i = 0; i < r.width; i++)
                        {
                            var top = cv.Get(r.x + i, r.y); if (top.a > 0) cv.Set(r.x + i, r.y, Sh(top, 0.82f));
                            var bot = cv.Get(r.x + i, r.y + r.height - 1); if (bot.a > 0) cv.Set(r.x + i, r.y + r.height - 1, Sh(bot, 0.88f));
                        }
                    }
                }
        }

        // ================================================================= painters
        static void Humanoid(SkinCanvas cv, ModelDef def, string kind)
        {
            Color32 skin = C(0xC98E62), shirt, pants, shoe, hair;
            bool shortSleeves = true, hairWrap = true;
            switch (kind)
            {
                case "zombie": skin = C(0x5E8B3E); shirt = C(0x1E9C9C); pants = C(0x3F3A8E); shoe = C(0x303048); hair = C(0x3E6A2A); hairWrap = false; break;
                case "husk": skin = C(0x8C7B58); shirt = C(0x6A5634); pants = C(0x4A4028); shoe = C(0x3A3020); hair = C(0x6E6044); hairWrap = false; break;
                case "drowned": skin = C(0x5E9A8C); shirt = C(0x3A7462); pants = C(0x2E5C5A); shoe = C(0x1E3A3A); hair = C(0x2E5E4A); break;
                case "zombie_villager": skin = C(0x6A8E56); shirt = C(0x5A4430); pants = C(0x4A3826); shoe = C(0x2A3A2A); hair = C(0x4A6A3A); hairWrap = false; shortSleeves = false; break;
                case "copper_golem": skin = C(0xC46E4B); shirt = C(0xC46E4B); pants = C(0xB05F3E); shoe = C(0x9A5236); hair = C(0x8A4A30); shortSleeves = false; hairWrap = false; break;
                default: shirt = C(0x1FA3A6); pants = C(0x3B3FA6); shoe = C(0x4A4A4A); hair = C(0x3B2614); break;
            }
            Box(cv, def.Get("body"), shirt);
            var legs = new[] { def.Get("right_leg"), def.Get("left_leg") };
            foreach (var l in legs)
            {
                Box(cv, l, pants);
                BoxBottom(cv, l, shoe);
                SideBand(cv, l, 10, 2, shoe);
            }
            foreach (var an in new[] { "right_arm", "left_arm" })
            {
                var a = def.Get(an);
                if (shortSleeves)
                {
                    // bare forearms with a short sleeve at the shoulder
                    Box(cv, a, skin);
                    BoxTop(cv, a, shirt);
                    SideBand(cv, a, 0, 4, shirt);
                }
                else { Box(cv, a, shirt); BoxBottom(cv, a, skin); }
            }
            var head = def.Get("head");
            Box(cv, head, skin);
            BoxSides(cv, head, Sh(skin, 0.96f));
            BoxTop(cv, head, hair);
            if (kind != "copper_golem")
            {
                var h0 = head.cubes[0];
                var front = cv.R(h0, 3);
                // fringe along the brow, hair over the upper sides and most of the back
                cv.Rect(front.x, front.y, front.width, Mathf.Max(1, front.height / 8), hair);
                if (hairWrap)
                {
                    foreach (int f in new[] { 2, 4 })
                    {
                        var r = cv.R(h0, f);
                        cv.Rect(r.x, r.y, r.width, Mathf.Max(1, r.height * 3 / 8), hair);
                        cv.Rect(f == 2 ? r.x + r.width - 2 : r.x, r.y + r.height * 3 / 8, 2, 1, hair);
                    }
                    var back = cv.R(h0, 5);
                    cv.Rect(back.x, back.y, back.width, Mathf.Max(1, back.height * 5 / 8), hair);
                }
                Eyes(cv, head, 0, C(0xFFFFFF), kind.StartsWith("zombie") || kind == "husk" || kind == "drowned" ? C(0x1A1A1A) : C(0x4A3AA0), 0.2f, kind == "drowned" || kind == "husk");
                if (kind == "zombie" || kind == "husk" || kind == "zombie_villager") Mouth(cv, head, 0, C(0x2A2A2A), 0.4f);
                else Mouth(cv, head, 0, C(0x8A4A3A), 0.3f);
            }
            else
            {
                var bulb = def.Get("head");
                Box(cv, bulb, C(0x5AC8A8), 0.05f);
                Eyes(cv, head, 0, Glow(C(0xFFF090)), C(0x3A2A10), 0.25f, true);
            }
            if (kind == "player")
            {
                // a darker collar and front panel so the shirt reads as clothing
                BoxFront(cv, def.Get("body"), Sh(shirt, 0.9f));
                var body = def.Get("body");
                if (body != null && body.cubes.Count > 0)
                {
                    var r = cv.R(body.cubes[0], 3);
                    cv.Rect(r.x + r.width / 2 - 2, r.y, 4, 1, Sh(skin, 0.95f));
                }
            }
        }

        static void SkeletonSkin(SkinCanvas cv, ModelDef def, string kind)
        {
            Color32 bone = kind == "wither_skeleton" ? C(0x2A2A2A) : kind == "bogged" ? C(0x6A7A5A) : kind == "parched" ? C(0xC8B48A) : kind == "stray" ? C(0x8A9A9A) : C(0xC8C8C8);
            Box(cv, def.Get("body"), Sh(bone, 0.9f));
            Box(cv, def.Get("right_arm"), bone); Box(cv, def.Get("left_arm"), bone);
            Box(cv, def.Get("right_leg"), Sh(bone, 0.95f)); Box(cv, def.Get("left_leg"), Sh(bone, 0.95f));
            // ribcage
            var body = def.Get("body");
            foreach (var c in body.cubes)
            {
                var r = cv.R(c, 3);
                for (int i = 2; i < r.height - 1; i += 2) cv.Fill(r.x + 1, r.y + i, Mathf.Max(1, r.width - 2), 1, Sh(bone, 0.72f));
            }
            var head = def.Get("head");
            Box(cv, head, bone);
            // skull sockets + teeth
            var hr = cv.R(head.cubes[0], 3);
            int ew = Mathf.Max(2, hr.width / 4), eh = Mathf.Max(2, hr.height / 4);
            int ey = hr.y + hr.height / 3;
            cv.Fill(hr.x + hr.width / 5, ey, ew, eh, C(0x101010));
            cv.Fill(hr.x + hr.width - hr.width / 5 - ew, ey, ew, eh, C(0x101010));
            if (kind == "stray" || kind == "bogged" || kind == "parched")
            {
                cv.Fill(hr.x + hr.width / 5, ey, ew, eh, Glow(kind == "bogged" ? C(0x4A8A2A) : C(0x60D8F0)));
                cv.Fill(hr.x + hr.width - hr.width / 5 - ew, ey, ew, eh, Glow(kind == "bogged" ? C(0x4A8A2A) : C(0x60D8F0)));
            }
            cv.Fill(hr.x + hr.width / 4, hr.y + hr.height - 3, hr.width / 2, 1, Sh(bone, 0.6f));
            if (kind == "wither_skeleton") { Mark(cv, def.Get("body"), 3, C(0x101010), 0.12f); }
        }

        static void PiglinSkin(SkinCanvas cv, ModelDef def, string kind)
        {
            bool zombified = kind == "zombified_piglin";
            bool brute = kind == "piglin_brute";
            Color32 skin = zombified ? C(0x4C9A4C) : C(0xD89A78);
            Color32 snout = zombified ? C(0x3A7A3A) : C(0xC08060);
            Color32 cloth = zombified ? C(0x3A6A3A) : brute ? C(0x3A2A20) : C(0x6A4A2A);
            Box(cv, def.Get("body"), cloth);
            Box(cv, def.Get("right_arm"), skin); Box(cv, def.Get("left_arm"), skin);
            Box(cv, def.Get("right_leg"), Sh(skin, 1.05f)); Box(cv, def.Get("left_leg"), Sh(skin, 1.05f));
            var head = def.Get("head");
            Box(cv, head, skin);
            BoxBottom(cv, head, Sh(skin, 0.9f));
            // snout cube (index 1)
            foreach (var b in new[] { "right_ear", "left_ear" }) if (def.Has(b)) Box(cv, def.Get(b), Sh(skin, 0.92f));
            var snoutC = head.cubes.Count > 1 ? head.cubes[1] : head.cubes[0];
            var sr = cv.R(snoutC, 3);
            cv.Fill(sr.x + 1, sr.y + sr.height / 3, Mathf.Max(1, sr.width - 2), Mathf.Max(1, sr.height / 4), C(0x2A1A14));
            Eyes(cv, head, 0, zombified ? C(0xE8F8E8) : C(0xFFFFFF), zombified ? C(0x2A6A2A) : C(0x2A1A14), 0.18f);
            if (kind == "piglin" || brute)
            {
                // gold jewellery & tusks
                Mark(cv, def.Get("right_arm"), 3, C(0xF8D040), 0.25f);
                if (head.cubes.Count > 3) cv.Face(head.cubes[3], 3, C(0xF8E8C0));
                if (head.cubes.Count > 4) cv.Face(head.cubes[4], 3, C(0xF8E8C0));
            }
        }

        static void VillagerSkin(SkinCanvas cv, ModelDef def, string skinName, string variantSalt)
        {
            int variant = 0; if (variantSalt != null) int.TryParse(variantSalt, out variant);
            Color32 skin = skinName == "zombie_villager" ? C(0x7A9A7A) : C(0xD8A070);
            Color32 robe, belt, eyes = C(0xFFFFFF), pupil = C(0x2A3A6A);
            if (skinName == "wandering_trader") { robe = C(0x2A4A9A); belt = C(0xE0A030); }
            else
            {
                switch (variant % 14)
                {
                    case 1: robe = C(0x8A6A3A); belt = C(0x6A5030); break;      // farmer
                    case 2: robe = C(0xD8D8D8); belt = C(0xA0A0A0); break;      // librarian
                    case 3: robe = C(0xB0B0B0); belt = C(0x6A6A6A); break;      // armorer
                    case 4: robe = C(0x8A8A8A); belt = C(0x3A3A3A); break;      // weaponsmith
                    case 5: robe = C(0x6A6A6A); belt = C(0x4A4A4A); break;      // toolsmith
                    case 6: robe = C(0xA030A0); belt = C(0x6A206A); break;      // cleric
                    case 7: robe = C(0xD8C8C8); belt = C(0xA03030); break;      // butcher
                    case 8: robe = C(0x4A6A9A); belt = C(0x2A4A6A); break;      // fisherman
                    case 9: robe = C(0x8A7A5A); belt = C(0x6A5A3A); break;      // fletcher
                    case 10: robe = C(0x6A4A3A); belt = C(0x4A3020); break;     // leatherworker
                    case 11: robe = C(0x9A9A8A); belt = C(0x6A6A5A); break;     // mason
                    case 12: robe = C(0xE0E0E0); belt = C(0xB0B0B0); break;     // shepherd
                    case 13: robe = C(0xD8D0C0); belt = C(0x8A8A7A); break;     // cartographer
                    default: robe = C(0x6A5A4A); belt = C(0x4A3A2A); break;     // none / nitwit
                }
            }
            Box(cv, def.Get("body"), robe);
            var body = def.Get("body");
            foreach (var c in body.cubes) { var r = cv.R(c, 3); cv.Fill(r.x, r.y + r.height / 2, r.width, 1, belt); }
            Box(cv, def.Get("right_leg"), Sh(robe, 0.85f)); Box(cv, def.Get("left_leg"), Sh(robe, 0.85f));
            BoxBottom(cv, def.Get("right_leg"), C(0x3A2A1A)); BoxBottom(cv, def.Get("left_leg"), C(0x3A2A1A));
            var arms = def.Get("arms") ?? def.Get("right_arm");
            if (arms != null) foreach (var c in arms.cubes) cv.AllFaces(c, Sh(robe, 1.08f));
            var head = def.Get("head");
            Box(cv, head, skin);
            BoxTop(cv, head, Sh(skin, 0.9f));
            var hr = cv.R(head.cubes[0], 3);
            // brow + nose
            cv.Fill(hr.x, hr.y + hr.height - 2, hr.width, 1, Sh(skin, 0.8f));
            if (head.cubes.Count > 1) cv.Face(head.cubes[1], 3, Sh(skin, 1.05f));
            Eyes(cv, head, 0, eyes, pupil, 0.16f);
            if (skinName == "zombie_villager") { cv.Fill(hr.x + hr.width / 3, hr.y + hr.height - 3, hr.width / 4, 1, C(0x2A2A2A)); }
        }

        static void IllagerSkin(SkinCanvas cv, ModelDef def, string kind)
        {
            Color32 robe = kind == "evoker" ? C(0x2A2A3A) : kind == "vindicator" ? C(0x4A5A5A) : C(0x3A4A3A);
            Color32 skin = C(0x9AA0A0);
            Box(cv, def.Get("body"), robe);
            var body = def.Get("body");
            foreach (var c in body.cubes) { var r = cv.R(c, 3); cv.Fill(r.x, r.y + r.height - 3, r.width, 2, Sh(robe, 0.6f)); }
            Box(cv, def.Get("right_arm"), robe); Box(cv, def.Get("left_arm"), robe);
            foreach (var l in new[] { "right_leg", "left_leg" }) { Box(cv, def.Get(l), Sh(robe, 0.8f)); BoxBottom(cv, def.Get(l), C(0x2A2A2A)); }
            var head = def.Get("head");
            Box(cv, head, skin);
            BoxTop(cv, head, Sh(skin, 0.85f));
            if (head.cubes.Count > 1) cv.Face(head.cubes[1], 3, Sh(skin, 1.05f));
            Eyes(cv, head, 0, C(0xE8E8E8), kind == "evoker" ? C(0xE0E0FF) : C(0x3A3A3A), 0.16f);
            // bald head with a fringe
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x, hr.y + hr.height - 2, hr.width, 1, C(0x6A6A6A));
            if (kind == "vindicator") Mark(cv, head, 3, C(0x7A8080), 0.1f);
            if (kind == "evoker")
            {
                Mark(cv, def.Get("body"), 3, C(0x8A4AC8), 0.14f);
                var armR = def.Get("right_arm"); if (armR != null) Mark(cv, armR, 3, C(0x8A4AC8), 0.2f);
            }
        }

        static void WitchSkin(SkinCanvas cv, ModelDef def)
        {
            Box(cv, def.Get("body"), C(0x3A2A4A));
            Box(cv, def.Get("right_leg"), C(0x2A1A3A)); Box(cv, def.Get("left_leg"), C(0x2A1A3A));
            BoxBottom(cv, def.Get("right_leg"), C(0x1A0A1A)); BoxBottom(cv, def.Get("left_leg"), C(0x1A0A1A));
            if (def.Has("arms")) foreach (var c in def.Get("arms").cubes) cv.AllFaces(c, C(0x4A3A5A));
            var head = def.Get("head");
            Box(cv, head, C(0x6A8A5A));
            BoxTop(cv, head, C(0x5A7A4A));
            Eyes(cv, head, 0, C(0xFFFFFF), C(0x2A1A3A), 0.18f);
            if (head.cubes.Count > 1) cv.Face(head.cubes[1], 3, C(0x8A5A4A));
            if (head.cubes.Count > 2) cv.Face(head.cubes[2], 3, C(0x8A3A2A));   // wart
            // hat: paint only the hat cubes (index 3+)
            for (int i = 3; i < head.cubes.Count; i++)
            {
                var c = head.cubes[i];
                cv.AllFaces(c, C(0x1A0A2A), 0.04f);
                if (i == head.cubes.Count - 1) cv.Face(c, 0, C(0x3A2A4A));
            }
        }

        static void EndermanSkin(SkinCanvas cv, ModelDef def)
        {
            Box(cv, def.Get("body"), C(0x141414));
            Box(cv, def.Get("right_arm"), C(0x141414)); Box(cv, def.Get("left_arm"), C(0x141414));
            Box(cv, def.Get("right_leg"), C(0x101010)); Box(cv, def.Get("left_leg"), C(0x101010));
            var head = def.Get("head");
            Box(cv, head, C(0x141414));
            BoxTop(cv, head, C(0x0C0C0C));
            // glowing eyes + mouth
            var hr = cv.R(head.cubes[0], 3);
            int ew = Mathf.Max(2, hr.width / 3), eh = Mathf.Max(1, hr.height / 8);
            int ey = hr.y + hr.height / 3;
            cv.Fill(hr.x + hr.width / 8, ey, ew, eh, Glow(C(0xE8B0FF)));
            cv.Fill(hr.x + hr.width - hr.width / 8 - ew, ey, ew, eh, Glow(C(0xE8B0FF)));
            Mark(cv, def.Get("body"), 3, C(0x2A2A2A), 0.06f);
        }

        static void CreeperSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 green = C(0x4CA03E), dark = C(0x2A6A22), light = C(0x74C050);
            foreach (var n in new[] { "body", "head", "leg_fr", "leg_fl", "leg_br", "leg_bl" })
            {
                var b = def.Get(n); if (b == null) continue;
                Box(cv, b, green, 0.12f);
                Mark(cv, b, 3, dark, 0.18f);
                Mark(cv, b, 2, dark, 0.14f);
                Mark(cv, b, 4, dark, 0.14f);
                Mark(cv, b, 0, light, 0.1f);
            }
            var head = def.Get("head");
            var hr = cv.R(head.cubes[0], 3);
            // classic creeper face pattern (original geometry, own pixels)
            int w = hr.width, h = hr.height;
            void Px(int x, int y, int ww, int hh, Color32 c) => cv.Fill(hr.x + x, hr.y + y, ww, hh, c);
            Px(w / 8, h / 5, w / 5, h / 4, C(0x0A1A0A));
            Px(w - w / 8 - w / 5, h / 5, w / 5, h / 4, C(0x0A1A0A));
            Px(w / 2 - w / 8, h / 2, w / 4, h / 4, C(0x0A1A0A));
            Px(w / 8 + w / 5, h / 5 + h / 4 - h / 12, w / 6, h / 3, C(0x0A1A0A));
            Px(w - w / 8 - w / 5 - w / 6, h / 5 + h / 4 - h / 12, w / 6, h / 3, C(0x0A1A0A));
        }

        static void WardenSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 dark = C(0x0E3A3C), mid = C(0x14605E), glow = C(0x2AD0D8);
            foreach (var b in def.bones)
            {
                Box(cv, b, dark, 0.08f);
                Mark(cv, b, 3, mid, 0.2f);
                Mark(cv, b, 2, mid, 0.15f); Mark(cv, b, 4, mid, 0.15f);
            }
            var head = def.Get("head");
            BoxTop(cv, head, C(0x0A2A2C));
            // glowing chest core + eyes
            var body = def.Get("body");
            foreach (var c in body.cubes)
            {
                var r = cv.R(c, 3);
                cv.Fill(r.x + r.width / 2 - Mathf.Max(1, r.width / 8), r.y + r.height / 2, Mathf.Max(2, r.width / 4), Mathf.Max(2, r.height / 4), Glow(glow));
            }
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), 1, Glow(glow));
            cv.Fill(hr.x + hr.width - 2 - hr.width / 5, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), 1, Glow(glow));
            Mark(cv, head, 3, C(0x0A2A2C), 0.12f);
        }

        static void CreakingSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 wood = C(0x4A3A2A), dark = C(0x2A1E14), orange = C(0xFC7812), leaf = C(0x7A8A5A);
            foreach (var b in def.bones)
            {
                Box(cv, b, wood, 0.1f);
                Mark(cv, b, 3, dark, 0.2f);
                Mark(cv, b, 2, dark, 0.16f); Mark(cv, b, 4, dark, 0.16f);
            }
            var head = def.Get("head");
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + hr.width / 5, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 4), Mathf.Max(1, hr.height / 5), Glow(orange));
            cv.Fill(hr.x + hr.width - hr.width / 5 - Mathf.Max(1, hr.width / 4), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 4), Mathf.Max(1, hr.height / 5), Glow(orange));
            if (head.cubes.Count > 1) cv.AllFaces(head.cubes[1], leaf, 0.1f);
            if (head.cubes.Count > 2) cv.AllFaces(head.cubes[2], leaf, 0.1f);
        }

        static void GolemSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 iron = C(0xC8C0B8), dark = C(0x9A9088), vine = C(0x5A8A3A);
            foreach (var b in def.bones)
            {
                Box(cv, b, iron, 0.06f);
                Mark(cv, b, 3, dark, 0.15f);
            }
            var head = def.Get("head");
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 6), C(0x2A2A2A));
            cv.Fill(hr.x + hr.width - 1 - Mathf.Max(1, hr.width / 5), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 6), C(0x2A2A2A));
            int my = hr.y + hr.height - Mathf.Max(2, hr.height / 4);
            cv.Fill(hr.x + hr.width / 4, my, Mathf.Max(1, hr.width / 2), 1, C(0x4A3A2A));
            // vines and poppy
            Mark(cv, def.Get("body"), 3, vine, 0.1f);
            Mark(cv, def.Get("right_arm"), 3, vine, 0.08f);
            if (def.Has("nose")) cv.AllFaces(def.Get("nose").cubes[0], C(0xB0A8A0));
        }

        static void SnowGolemSkin(SkinCanvas cv, ModelDef def)
        {
            foreach (var n in new[] { "lower", "body" }) { var b = def.Get(n); if (b != null) Box(cv, b, C(0xF4F8FF), 0.03f); }
            var head = def.Get("head");
            Box(cv, head, C(0xF4F8FF), 0.03f);
            if (head.cubes.Count > 1)
            {
                var p = head.cubes[1];
                cv.AllFaces(p, C(0xD87818), 0.06f);
                var r = cv.R(p, 3);
                // pumpkin face
                cv.Fill(r.x + r.width / 5, r.y + r.height / 3, Mathf.Max(1, r.width / 5), Mathf.Max(1, r.height / 5), C(0x2A1408));
                cv.Fill(r.x + r.width - r.width / 5 - Mathf.Max(1, r.width / 5), r.y + r.height / 3, Mathf.Max(1, r.width / 5), Mathf.Max(1, r.height / 5), C(0x2A1408));
                cv.Fill(r.x + r.width / 4, r.y + r.height - 3, Mathf.Max(1, r.width / 2), 1, C(0x2A1408));
            }
            foreach (var a in new[] { "right_arm", "left_arm" }) { var b = def.Get(a); if (b != null) Box(cv, b, C(0x8A5A2A), 0.08f); }
        }

        static void AllaySkin(SkinCanvas cv, ModelDef def, string kind)
        {
            if (kind == "vex")
            {
                Box(cv, def.Get("body"), C(0x5A6A7A));
                Box(cv, def.Get("head"), C(0x7A8A9A));
                Box(cv, def.Get("right_arm"), C(0x5A6A7A)); Box(cv, def.Get("left_arm"), C(0x5A6A7A));
                foreach (var w in new[] { "right_wing", "left_wing" }) { var b = def.Get(w); if (b != null) foreach (var c in b.cubes) cv.AllFaces(c, C(0x2A3A4A)); }
                var hr = cv.R(def.Get("head").cubes[0], 3);
                cv.Fill(hr.x, hr.y + 1, hr.width, 1, Glow(C(0xFF3020)));
            }
            else
            {
                Box(cv, def.Get("body"), C(0x2AC8E0));
                Box(cv, def.Get("head"), C(0x40D8F0));
                Box(cv, def.Get("right_arm"), C(0x2AC8E0)); Box(cv, def.Get("left_arm"), C(0x2AC8E0));
                foreach (var w in new[] { "right_wing", "left_wing" }) { var b = def.Get(w); if (b != null) foreach (var c in b.cubes) cv.AllFaces(c, C(0x9AF0FF)); }
                var hr = cv.R(def.Get("head").cubes[0], 3);
                cv.Fill(hr.x + hr.width / 4, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 8), Mathf.Max(1, hr.height / 6), C(0x1A3A4A));
                cv.Fill(hr.x + hr.width - hr.width / 4 - Mathf.Max(1, hr.width / 8), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 8), Mathf.Max(1, hr.height / 6), C(0x1A3A4A));
            }
        }

        static void PigSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 pink = C(0xF0A5A2), dark = C(0xD88A88);
            Box(cv, def.Get("body"), pink);
            Box(cv, def.Get("head"), pink);
            Box(cv, def.Get("leg_fr"), dark); Box(cv, def.Get("leg_fl"), dark);
            Box(cv, def.Get("leg_br"), dark); Box(cv, def.Get("leg_bl"), dark);
            var head = def.Get("head");
            Eyes(cv, head, 0, C(0xFFFFFF), C(0x2A1A14), 0.2f);
            if (head.cubes.Count > 1)
            {
                cv.AllFaces(head.cubes[1], C(0xE09090));
                var r = cv.R(head.cubes[1], 3);
                cv.Fill(r.x + r.width / 5, r.y + r.height / 3, Mathf.Max(1, r.width / 6), Mathf.Max(1, r.height / 4), C(0x8A4A4A));
                cv.Fill(r.x + r.width - r.width / 5 - Mathf.Max(1, r.width / 6), r.y + r.height / 3, Mathf.Max(1, r.width / 6), Mathf.Max(1, r.height / 4), C(0x8A4A4A));
            }
            Mark(cv, def.Get("body"), 0, dark, 0.08f);
        }

        static void CowSkin(SkinCanvas cv, ModelDef def, string kind)
        {
            bool mooshroom = kind == "mooshroom";
            Color32 hide = mooshroom ? C(0xA00F10) : C(0x443626), spots = mooshroom ? C(0xC8C8C8) : C(0xD8D8D8);
            Box(cv, def.Get("body"), hide);
            Mark(cv, def.Get("body"), 3, spots, 0.22f);
            Mark(cv, def.Get("body"), 0, spots, 0.18f);
            Mark(cv, def.Get("body"), 2, spots, 0.16f);
            var head = def.Get("head");
            Box(cv, head, hide);
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x, hr.y + hr.height - 2, hr.width, 2, spots); // forehead blaze
            if (head.cubes.Count > 1) { cv.AllFaces(head.cubes[1], C(0xE0B0A0), 0.05f); var mr = cv.R(head.cubes[1], 3); cv.Fill(mr.x + 1, mr.y + mr.height / 3, Mathf.Max(1, mr.width / 6), Mathf.Max(1, mr.height / 5), C(0x8A5A4A)); cv.Fill(mr.x + mr.width - 1 - Mathf.Max(1, mr.width / 6), mr.y + mr.height / 3, Mathf.Max(1, mr.width / 6), Mathf.Max(1, mr.height / 5), C(0x8A5A4A)); }
            foreach (var h in new[] { "horn_r", "horn_l" }) if (def.Has(h)) Box(cv, def.Get(h), C(0xE8E0D0), 0.05f);
            Eyes(cv, head, 0, C(0xFFFFFF), C(0x2A1A14), 0.18f);
            foreach (var l in new[] { "leg_fr", "leg_fl", "leg_br", "leg_bl" }) { Box(cv, def.Get(l), hide); BoxBottom(cv, def.Get(l), C(0x2A1E14)); }
            if (def.Has("udder")) Box(cv, def.Get("udder"), C(0xE8A0A0), 0.05f);
            if (mooshroom && def.Has("mushroom1")) Box(cv, def.Get("mushroom1"), C(0xD02020), 0.1f);
        }

        static void SheepSkin(SkinCanvas cv, ModelDef def, string variantSalt)
        {
            int variant = 0; if (variantSalt != null) int.TryParse(variantSalt, out variant);
            Color32 wool = variant >= 0 && variant < Blocks.Colors.Length ? TextureGen.DyeColors[Blocks.Colors[variant]] : C(0xE7E7E7);
            var skin = C(0xD8C0B0);
            var body = def.Get("body");
            if (body != null) foreach (var c in body.cubes) cv.AllFaces(c, c.inflate > 0.5f ? wool : skin, c.inflate > 0.5f ? 0.14f : 0.06f);
            var head = def.Get("head");
            foreach (var c in head.cubes) cv.AllFaces(c, c.inflate > 0.5f ? wool : skin, c.inflate > 0.5f ? 0.12f : 0.06f);
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 6), Mathf.Max(1, hr.height / 6), C(0x2A2A2A));
            cv.Fill(hr.x + hr.width - 1 - Mathf.Max(1, hr.width / 6), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 6), Mathf.Max(1, hr.height / 6), C(0x2A2A2A));
            foreach (var l in new[] { "leg_fr", "leg_fl", "leg_br", "leg_bl" })
            {
                var b = def.Get(l); if (b == null) continue;
                foreach (var c in b.cubes) cv.AllFaces(c, c.inflate > 0.5f ? wool : skin, 0.1f);
                BoxBottom(cv, b, C(0x3A3A3A));
            }
        }

        static void ChickenSkin(SkinCanvas cv, ModelDef def)
        {
            Box(cv, def.Get("body"), C(0xE8E8E8), 0.05f);
            var head = def.Get("head");
            Box(cv, head, C(0xE8E8E8), 0.05f);
            if (head.cubes.Count > 1) cv.AllFaces(head.cubes[1], C(0xF8A020), 0.05f);   // beak
            if (head.cubes.Count > 2) cv.AllFaces(head.cubes[2], C(0xD02020), 0.05f);   // wattle
            Eyes(cv, head, 0, C(0xFFFFFF), C(0x1A1A1A), 0.2f);
            foreach (var w in new[] { "right_wing", "left_wing" }) { var b = def.Get(w); if (b != null) Box(cv, b, C(0xD8D8D8), 0.06f); }
            foreach (var l in new[] { "right_leg", "left_leg" }) { var b = def.Get(l); if (b != null) { Box(cv, b, C(0xF8A020), 0.05f); BoxBottom(cv, b, C(0xD88010)); } }
        }

        static void RabbitSkin(SkinCanvas cv, ModelDef def, string variantSalt)
        {
            int v = 0; if (variantSalt != null) int.TryParse(variantSalt, out v);
            Color32 fur = v == 1 ? C(0xE8E8E8) : v == 2 ? C(0x2A2A2A) : v == 4 ? C(0xD8B060) : v == 5 ? C(0xE8D8C0) : C(0x995F40);
            Color32 belly = Sh(fur, 1.3f);
            foreach (var b in def.bones) Box(cv, b, fur, 0.08f);
            BoxBottom(cv, def.Get("body"), belly);
            var head = def.Get("head");
            Eyes(cv, head, 0, C(0xD02020), C(0xD02020), 0.2f);
            if (head.cubes.Count > 1) cv.Face(head.cubes[1], 3, Sh(fur, 1.1f));
            if (head.cubes.Count > 2) cv.Face(head.cubes[2], 3, Sh(fur, 1.1f));
            if (head.cubes.Count > 1) { var r = cv.R(head.cubes[1], 3); cv.Fill(r.x + r.width / 2, r.y, 1, Mathf.Max(1, r.height / 3), C(0xE8B0B0)); }
            BoxBottom(cv, def.Get("leg_br"), belly); BoxBottom(cv, def.Get("leg_bl"), belly);
        }

        static void HorseSkin(SkinCanvas cv, ModelDef def, string kind, string variantSalt)
        {
            int v = 0; if (variantSalt != null) int.TryParse(variantSalt, out v);
            Color32[] coats = { C(0x8A6A4A), C(0x4A3A2A), C(0xA08060), C(0x6A5030), C(0xD8C8A8), C(0x3A3028), C(0xC8B090) };
            Color32[] manes = { C(0x2A1E14), C(0x1A1410), C(0x3A2A1A), C(0x2A1E14), C(0xE8E0D0), C(0x1A1410), C(0xD8CCB0) };
            Color32 coat, mane;
            if (kind == "skeleton_horse") { coat = C(0xC8C8B8); mane = C(0x8A8A7A); }
            else if (kind == "zombie_horse") { coat = C(0x4A6A3A); mane = C(0x2A3A22); }
            else { int idx = v % coats.Length; coat = coats[idx]; mane = manes[(v / coats.Length + idx) % manes.Length]; }
            foreach (var b in def.bones) Box(cv, b, coat, 0.07f);
            BoxTop(cv, def.Get("body"), Sh(coat, 1.06f));
            if (def.Has("mane")) foreach (var c in def.Get("mane").cubes) cv.AllFaces(c, mane, 0.08f);
            if (def.Has("tail")) Box(cv, def.Get("tail"), mane, 0.08f);
            var head = def.Get("head");
            if (head.cubes.Count > 1) { cv.AllFaces(head.cubes[1], Sh(coat, 0.85f), 0.05f); var mr = cv.R(head.cubes[1], 3); cv.Fill(mr.x + 1, mr.y + mr.height / 3, 1, Mathf.Max(1, mr.height / 5), C(0x2A1A14)); cv.Fill(mr.x + mr.width - 2, mr.y + mr.height / 3, 1, Mathf.Max(1, mr.height / 5), C(0x2A1A14)); }
            Eyes(cv, head, 0, C(0xFFFFFF), C(0x2A1A14), 0.2f);
            // white markings (socks/blaze) for variety
            if (v % 3 == 0) { foreach (var l in new[] { "leg_fr", "leg_bl" }) BoxBottom(cv, def.Get(l), C(0xF0F0F0)); }
            if (v % 5 == 0) { var hr = cv.R(head.cubes[0], 3); cv.Fill(hr.x + 1, hr.y, Mathf.Max(1, hr.width - 2), Mathf.Max(1, hr.height / 4), C(0xF0F0F0)); }
        }

        static void LlamaSkin(SkinCanvas cv, ModelDef def, string kind, string variantSalt)
        {
            int v = 0; if (variantSalt != null) int.TryParse(variantSalt, out v);
            Color32[] coats = { C(0xC09E7D), C(0xE0D8C0), C(0x8A6A4A), C(0x6A5A4A) };
            Color32 coat = coats[v % coats.Length];
            foreach (var b in def.bones) Box(cv, b, coat, 0.08f);
            BoxTop(cv, def.Get("body"), Sh(coat, 1.08f));
            var head = def.Get("head");
            if (head.cubes.Count > 1) cv.AllFaces(head.cubes[1], Sh(coat, 0.9f), 0.05f);
            Eyes(cv, head, 0, C(0xFFFFFF), C(0x2A1A14), 0.2f);
            if (def.Has("carpet")) Box(cv, def.Get("carpet"), kind == "trader_llama" ? C(0x2A4A9A) : C(0x8A2A2A), 0.1f);
        }

        static void CamelSkin(SkinCanvas cv, ModelDef def, string kind)
        {
            Color32 coat = kind == "camel_husk" ? C(0x8A7A5A) : C(0xFCC369);
            foreach (var b in def.bones) Box(cv, b, coat, 0.07f);
            if (def.Has("hump")) Box(cv, def.Get("hump"), Sh(coat, 0.9f), 0.1f);
            var head = def.Get("head");
            if (head.cubes.Count > 1) cv.AllFaces(head.cubes[1], Sh(coat, 0.85f), 0.06f);
            Eyes(cv, head, 0, C(0xFFFFFF), kind == "camel_husk" ? C(0x3A2A1A) : C(0x2A1A14), 0.2f);
        }

        static void WolfSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 fur = C(0xD7D3D3), dark = C(0xB0AAA8), belly = C(0xF0ECEC);
            foreach (var b in def.bones) Box(cv, b, fur, 0.08f);
            BoxBottom(cv, def.Get("body"), belly);
            if (def.Has("mane")) foreach (var c in def.Get("mane").cubes) cv.AllFaces(c, Sh(fur, 0.92f), 0.1f);
            if (def.Has("tail")) Box(cv, def.Get("tail"), Sh(fur, 0.95f), 0.08f);
            var head = def.Get("head");
            if (head.cubes.Count > 1) cv.AllFaces(head.cubes[1], C(0xE8E0DC), 0.05f);
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 5), C(0x2A1A14));
            cv.Fill(hr.x + hr.width - 2 - Mathf.Max(1, hr.width / 5), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 5), C(0x2A1A14));
            // red/brown collar band
            foreach (var c in def.Get("body").cubes) { var r = cv.R(c, 3); cv.Fill(r.x, r.y + r.height - 2, r.width, 1, C(0xA03020)); }
        }

        static void CatSkin(SkinCanvas cv, ModelDef def, string kind, string variantSalt)
        {
            int v = 0; if (variantSalt != null) int.TryParse(variantSalt, out v);
            Color32[] coats = { C(0xEFC88E), C(0x2A2A2A), C(0xD8D8D8), C(0xE8E0C0), C(0x8A6A4A), C(0xF0A050), C(0x6A5A6A), C(0x3A3A3A), C(0xE8D8B0), C(0x6A4A2A), C(0xC0A080) };
            Color32 coat = kind == "ocelot" ? C(0xEFDE7D) : coats[v % coats.Length];
            foreach (var b in def.bones) Box(cv, b, coat, 0.07f);
            if (v % 4 == 0 || kind == "ocelot") foreach (var b in new[] { "body", "head" }) { var bb = def.Get(b); if (bb != null) Mark(cv, bb, 3, Sh(coat, 0.7f), 0.12f); }
            BoxBottom(cv, def.Get("body"), Sh(coat, 1.25f));
            var head = def.Get("head");
            if (head.cubes.Count > 1) cv.AllFaces(head.cubes[1], Sh(coat, 1.15f), 0.05f);
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(2, hr.height / 4), C(0x7AD8D8));
            cv.Fill(hr.x + hr.width - 2 - Mathf.Max(1, hr.width / 5), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(2, hr.height / 4), C(0x7AD8D8));
            cv.Fill(hr.x + hr.width / 2 - 1, hr.y + hr.height / 3, 2, Mathf.Max(2, hr.height / 5), C(0x1A1A1A));
        }

        static void FoxSkin(SkinCanvas cv, ModelDef def, string variantSalt)
        {
            int v = 0; if (variantSalt != null) int.TryParse(variantSalt, out v);
            Color32 coat = v == 1 ? C(0xF0F0F0) : C(0xD8823A), belly = C(0xF0E8E0);
            foreach (var b in def.bones) Box(cv, b, coat, 0.08f);
            BoxBottom(cv, def.Get("body"), belly);
            if (def.Has("tail")) Box(cv, def.Get("tail"), coat, 0.1f);
            var head = def.Get("head");
            if (head.cubes.Count > 1) cv.AllFaces(head.cubes[1], belly, 0.05f);
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 5), C(0x2A1A14));
            cv.Fill(hr.x + hr.width - 2 - Mathf.Max(1, hr.width / 5), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 5), C(0x2A1A14));
        }

        static void GoatSkin(SkinCanvas cv, ModelDef def, string variantSalt)
        {
            int v = 0; if (variantSalt != null) int.TryParse(variantSalt, out v);
            Color32 coat = C(0xA5947C), horn = C(0x4A4038);
            foreach (var b in def.bones) Box(cv, b, coat, 0.08f);
            if (def.Has("fluff")) Box(cv, def.Get("fluff"), C(0xD8D0C0), 0.1f);
            foreach (var h in new[] { "horn_r", "horn_l" }) if (def.Has(h)) Box(cv, def.Get(h), horn, 0.06f);
            var head = def.Get("head");
            Eyes(cv, head, 0, C(0xE8E8D0), C(0x2A1A14), 0.22f);
            if (def.Has("beard")) Box(cv, def.Get("beard"), C(0xE0D8C8), 0.08f);
            if (v == 1) Mark(cv, def.Get("body"), 3, C(0xE8E8E8), 0.2f);
        }

        static void PolarBearSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 fur = C(0xF2F2F2), shade = C(0xD8D8D8);
            foreach (var b in def.bones) Box(cv, b, fur, 0.04f);
            BoxSides(cv, def.Get("body"), shade);
            if (def.Has("shoulder")) Box(cv, def.Get("shoulder"), fur, 0.04f);
            var head = def.Get("head");
            if (head.cubes.Count > 1) cv.AllFaces(head.cubes[1], C(0x2A2A2A), 0.05f);
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 6), Mathf.Max(1, hr.height / 6), C(0x1A1A1A));
            cv.Fill(hr.x + hr.width - 2 - Mathf.Max(1, hr.width / 6), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 6), Mathf.Max(1, hr.height / 6), C(0x1A1A1A));
        }

        static void PandaSkin(SkinCanvas cv, ModelDef def, string variantSalt)
        {
            int v = 0; if (variantSalt != null) int.TryParse(variantSalt, out v);
            Color32 white = C(0xE7E7E7), black = C(0x1B1B22);
            bool brown = v == 5, lazy = v == 1, aggressive = v == 3, weak = v == 4, playful = v == 2;
            var main = brown ? C(0xA06A3A) : white;
            var accent = brown ? C(0x6A4420) : black;
            foreach (var b in def.bones) Box(cv, b, main, 0.06f);
            BoxSides(cv, def.Get("body"), accent);
            foreach (var l in new[] { "leg_fr", "leg_fl", "leg_br", "leg_bl" }) { var b = def.Get(l); if (b != null) Box(cv, b, accent, 0.06f); }
            if (def.Has("shoulder")) Box(cv, def.Get("shoulder"), accent, 0.06f);
            var head = def.Get("head");
            if (head.cubes.Count > 1) cv.AllFaces(head.cubes[1], main, 0.05f);
            foreach (var e in new[] { "ear_r", "ear_l" }) if (def.Has(e)) Box(cv, def.Get(e), accent, 0.08f);
            var hr = cv.R(head.cubes[0], 3);
            int ew = Mathf.Max(1, hr.width / 6), eh = Mathf.Max(1, hr.height / 5);
            cv.Fill(hr.x + hr.width / 6, hr.y + hr.height / 3, ew, eh, accent);
            cv.Fill(hr.x + hr.width - hr.width / 6 - ew, hr.y + hr.height / 3, ew, eh, accent);
            cv.Fill(hr.x + hr.width / 6 + ew, hr.y + hr.height / 3, 1, eh, C(0xFFFFFF));
            cv.Fill(hr.x + hr.width - hr.width / 6 - ew - 1, hr.y + hr.height / 3, 1, eh, C(0xFFFFFF));
            if (aggressive) { cv.Fill(hr.x + hr.width / 5, hr.y + hr.height / 2, hr.width / 4, 1, C(0x8A1010)); }
            if (lazy) { var mr = cv.R(head.cubes[1], 3); cv.Fill(mr.x, mr.y, mr.width, mr.height, C(0x8A8A8A)); }
            if (v == 6) Mark(cv, head, 3, C(0xD8D8D8), 0.3f);
            _ = playful; _ = weak;
        }

        static void TurtleSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 shell = C(0x2A7A2A), scute = C(0x4AA04A), skin = C(0x6AA85A);
            Box(cv, def.Get("body"), shell, 0.08f);
            var body = def.Get("body");
            foreach (var c in body.cubes) { var r = cv.R(c, 0); for (int i = 0; i < r.width; i += 3) cv.Fill(r.x + i, r.y, 2, r.height, scute); }
            if (def.Has("belly")) Box(cv, def.Get("belly"), C(0xE8D8A0), 0.05f);
            var head = def.Get("head");
            Box(cv, head, skin, 0.06f);
            Eyes(cv, head, 0, C(0xFFFFFF), C(0x1A1A1A), 0.2f);
            foreach (var l in new[] { "leg_fr", "leg_fl", "leg_br", "leg_bl" }) { var b = def.Get(l); if (b != null) Box(cv, b, skin, 0.06f); }
        }

        static void FrogSkin(SkinCanvas cv, ModelDef def, string variantSalt)
        {
            int v = 0; if (variantSalt != null) int.TryParse(variantSalt, out v);
            Color32[] coats = { C(0xD07444), C(0xE8A03A), C(0x4AA060) };
            Color32 coat = coats[Mathf.Clamp(v, 0, 2)];
            foreach (var b in def.bones) Box(cv, b, coat, 0.08f);
            BoxBottom(cv, def.Get("body"), Sh(coat, 1.2f));
            var head = def.Get("head");
            foreach (var n in new[] { "eye_r", "eye_l" }) if (def.Has(n)) { Box(cv, def.Get(n), C(0x1A1A1A), 0.03f); var c = def.Get(n).cubes[0]; cv.Face(c, 0, C(0xE8D040)); }
            BoxTop(cv, head, C(0x8A5A30));
        }

        static void AxolotlSkin(SkinCanvas cv, ModelDef def, string variantSalt)
        {
            int v = 0; if (variantSalt != null) int.TryParse(variantSalt, out v);
            Color32[] coats = { C(0xFBC1E3), C(0xC89AE0), C(0xE0B060), C(0x8AD8D0), C(0x6AA0E0) };
            Color32 coat = coats[Mathf.Clamp(v, 0, 4)];
            foreach (var b in def.bones) Box(cv, b, coat, 0.07f);
            var head = def.Get("head");
            foreach (var g in new[] { "gill_r", "gill_l", "gill_top" }) if (def.Has(g)) Box(cv, def.Get(g), C(0xE85A8A), 0.1f);
            Eyes(cv, head, 0, C(0x1A1A1A), C(0x1A1A1A), 0.22f);
            Mark(cv, def.Get("body"), 0, Sh(coat, 0.8f), 0.15f);
        }

        static void ArmadilloSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 plate = C(0x9A7A6A), skin = C(0xC8A88A);
            Box(cv, def.Get("body"), plate, 0.07f);
            if (def.Has("shell")) foreach (var c in def.Get("shell").cubes) { cv.AllFaces(c, C(0x8A6A5A), 0.08f); var r = cv.R(c, 0); for (int i = 0; i < r.width; i += 3) cv.Fill(r.x + i, r.y, 1, r.height, C(0x6A4A3A)); }
            var head = def.Get("head");
            Box(cv, head, skin, 0.06f);
            Eyes(cv, head, 0, C(0x2A2A2A), C(0x2A2A2A), 0.2f);
            if (def.Has("tail")) Box(cv, def.Get("tail"), plate, 0.08f);
            foreach (var l in new[] { "leg_fr", "leg_fl", "leg_br", "leg_bl" }) { var b = def.Get(l); if (b != null) Box(cv, b, skin, 0.06f); }
        }

        static void SnifferSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 hide = C(0x871E09), fur = C(0x5A9E5C);
            Box(cv, def.Get("body"), hide, 0.07f);
            if (def.Has("fur")) Box(cv, def.Get("fur"), fur, 0.1f);
            var head = def.Get("head");
            Box(cv, head, hide, 0.06f);
            if (head.cubes.Count > 1) { cv.AllFaces(head.cubes[1], C(0x6A1408), 0.06f); }
            foreach (var e in new[] { "ear_r", "ear_l" }) if (def.Has(e)) Box(cv, def.Get(e), fur, 0.1f);
            Eyes(cv, head, 0, C(0xE8E0D8), C(0x2A1A14), 0.18f);
            foreach (var l in new[] { "leg_fr", "leg_fl", "leg_mr", "leg_ml", "leg_br", "leg_bl" }) { var b = def.Get(l); if (b != null) { Box(cv, b, hide, 0.06f); BoxBottom(cv, b, C(0x5A1408)); } }
        }

        static void HoglinSkin(SkinCanvas cv, ModelDef def, string kind)
        {
            bool zoglin = kind == "zoglin";
            Color32 hide = zoglin ? C(0x8A4A4A) : C(0xC66E55), mane = zoglin ? C(0x4A2A2A) : C(0x7A5030), tusk = C(0xE8E0D0);
            foreach (var b in def.bones) Box(cv, b, hide, 0.08f);
            if (def.Has("mane")) foreach (var c in def.Get("mane").cubes) cv.AllFaces(c, mane, 0.12f);
            var head = def.Get("head");
            foreach (var t in new[] { "tusk_r", "tusk_l" }) if (def.Has(t)) Box(cv, def.Get(t), tusk, 0.05f);
            foreach (var e in new[] { "ear_r", "ear_l" }) if (def.Has(e)) Box(cv, def.Get(e), Sh(hide, 0.85f), 0.08f);
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 7), Mathf.Max(1, hr.height / 8), C(0x1A0A0A));
            cv.Fill(hr.x + hr.width - 2 - Mathf.Max(1, hr.width / 7), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 7), Mathf.Max(1, hr.height / 8), C(0x1A0A0A));
            if (zoglin) Mark(cv, def.Get("body"), 3, C(0xD8D8D8), 0.1f);
        }

        static void RavagerSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 hide = C(0x757470), dark = C(0x5B5049), horn = C(0x3A3A38), eye = C(0xE8E8D0);
            foreach (var b in def.bones) Box(cv, b, hide, 0.08f);
            if (def.Has("belly")) Box(cv, def.Get("belly"), dark, 0.08f);
            if (def.Has("neck")) Box(cv, def.Get("neck"), hide, 0.08f);
            foreach (var h in new[] { "horn_r", "horn_l" }) if (def.Has(h)) Box(cv, def.Get(h), horn, 0.06f);
            var head = def.Get("head");
            BoxSides(cv, head, dark);
            Eyes(cv, head, 0, eye, C(0x2A1A14), 0.16f, true, true);
            if (def.Has("mouth")) Box(cv, def.Get("mouth"), C(0x8A2020), 0.06f);
            foreach (var l in new[] { "leg_fr", "leg_fl", "leg_br", "leg_bl" }) { var b = def.Get(l); if (b != null) BoxBottom(cv, b, C(0x3A3632)); }
        }

        static void StriderSkin(SkinCanvas cv, ModelDef def, string variantSalt)
        {
            int v = 0; if (variantSalt != null) int.TryParse(variantSalt, out v);
            Color32 coat = v == 1 ? C(0x8A3A4A) : C(0x9C3436), leg = C(0xC8A05A);
            Box(cv, def.Get("body"), coat, 0.1f);
            foreach (var b in def.bones) if (b.name.StartsWith("bristle")) foreach (var c in b.cubes) cv.AllFaces(c, C(0xE8D8C8), 0.12f);
            Box(cv, def.Get("right_leg"), leg, 0.08f); Box(cv, def.Get("left_leg"), leg, 0.08f);
            BoxBottom(cv, def.Get("right_leg"), C(0x8A6A3A)); BoxBottom(cv, def.Get("left_leg"), C(0x8A6A3A));
            var head = def.Get("head");
            if (head != null && head.cubes.Count > 0) { Eyes(cv, head, 0, C(0xFFFFFF), C(0x1A1A1A), 0.2f); }
            Mark(cv, def.Get("body"), 3, C(0x7A2426), 0.15f);
        }

        static void SpiderSkin(SkinCanvas cv, ModelDef def, string kind)
        {
            var body = kind == "cave_spider" ? C(0x0C424E) : C(0x342D27);
            var legs = Sh(body, 1.2f);
            foreach (var b in def.bones)
            {
                if (b.name.StartsWith("leg")) { Box(cv, b, legs, 0.1f); continue; }
                Box(cv, b, body, 0.1f);
            }
            // red eyes on the head front
            var head = def.Get("head");
            var hr = cv.R(head.cubes[0], 3);
            for (int i = 0; i < 4; i++)
            {
                int x = hr.x + (i % 2 == 0 ? hr.width / 5 : hr.width - hr.width / 5 - 1);
                int y = hr.y + hr.height / 3 + (i / 2) * 2;
                cv.Fill(x, y, Mathf.Max(1, hr.width / 8), Mathf.Max(1, hr.height / 8), Glow(C(0xD02020)));
            }
            Mark(cv, def.Get("abdomen"), 3, Sh(body, 0.7f), 0.14f);
        }

        static void SlimeSkin(SkinCanvas cv, ModelDef def, string kind)
        {
            if (kind == "magma_cube")
            {
                Color32 core = C(0x2A0A0A), hot = C(0xF8A020);
                foreach (var b in def.bones)
                {
                    if (b.name == "body") continue;
                    Box(cv, b, core, 0.12f);
                    foreach (var c in b.cubes) cv.Face(c, 0, hot, 0.15f);
                }
                Box(cv, def.Get("body"), hot, 0.1f);
                return;
            }
            bool sulfur = kind == "sulfur_cube";
            var outer = sulfur ? C(0xD8CC40) : C(0x51A03E);
            var inner = sulfur ? C(0xF0E870) : C(0x7EBF6E);
            foreach (var b in def.bones)
            {
                foreach (var c in b.cubes)
                {
                    if (c.translucent) { cv.AllFaces(c, new Color32(outer.r, outer.g, outer.b, 170), 0.08f); continue; }
                    if (c.name != null && c.name.StartsWith("eye")) { cv.AllFaces(c, C(0x1A1A1A), 0.02f); continue; }
                    if (c.name == "mouth") { cv.AllFaces(c, C(0x2A2A2A), 0.02f); continue; }
                    cv.AllFaces(c, inner, 0.1f);
                }
            }
            if (sulfur) Mark(cv, def.Get("body"), 3, C(0xA8A020), 0.2f);
        }

        static void GhastSkin(SkinCanvas cv, ModelDef def, string kind)
        {
            Color32 skin = kind == "happy_ghast" ? C(0xF4F4F4) : C(0xF9F9F9), shade = C(0xD8D8D8);
            Box(cv, def.Get("body"), skin, 0.05f);
            var body = def.Get("body");
            var r = cv.R(body.cubes[0], 3);
            // closed eyes + mouth (original pixel pattern)
            int ew = Mathf.Max(2, r.width / 6), eh = Mathf.Max(2, r.height / 12);
            cv.Fill(r.x + r.width / 5, r.y + r.height / 3, ew, eh, C(0x2A2A2A));
            cv.Fill(r.x + r.width - r.width / 5 - ew, r.y + r.height / 3, ew, eh, C(0x2A2A2A));
            cv.Fill(r.x + r.width / 3, r.y + r.height - r.height / 3, Mathf.Max(2, r.width / 4), Mathf.Max(2, r.height / 10), C(0x2A2A2A));
            for (int i = 1; i < body.cubes.Count; i++) cv.AllFaces(body.cubes[i], shade, 0.06f);
            foreach (var b in def.bones) if (b.name.StartsWith("tentacle")) Box(cv, b, Sh(skin, 0.9f), 0.08f);
            if (def.Has("harness")) Box(cv, def.Get("harness"), C(0x8A5A3A), 0.1f);
        }

        static void BlazeSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 core = C(0xF8B020), rod = C(0xF8D060), dark = C(0x8A5A10);
            Box(cv, def.Get("head"), core, 0.08f);
            var head = def.Get("head");
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 5), Glow(C(0xFFF0A0)));
            cv.Fill(hr.x + hr.width - 2 - Mathf.Max(1, hr.width / 5), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 5), Glow(C(0xFFF0A0)));
            foreach (var b in def.bones)
                if (b.name.StartsWith("rod"))
                    foreach (var c in b.cubes)
                    {
                        cv.AllFaces(c, rod, 0.15f);
                        var r = cv.R(c, 3);
                        cv.Fill(r.x + r.width / 3, r.y, Mathf.Max(1, r.width / 3), r.height, dark);
                    }
        }

        static void BreezeSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 body = C(0x8A6AB0), wind = C(0xC8B0E8);
            foreach (var b in def.bones)
            {
                if (b.name.StartsWith("wind")) { foreach (var c in b.cubes) cv.AllFaces(c, new Color32(wind.r, wind.g, wind.b, 160), 0.1f); continue; }
                Box(cv, b, body, 0.08f);
            }
            var head = def.Get("head");
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 5), Glow(C(0xE0D0FF)));
            cv.Fill(hr.x + hr.width - 2 - Mathf.Max(1, hr.width / 5), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 5), Glow(C(0xE0D0FF)));
            Mark(cv, def.Get("head"), 0, C(0x6A4A90), 0.15f);
        }

        static void ShulkerSkin(SkinCanvas cv, ModelDef def, string variantSalt)
        {
            int v = 0; if (variantSalt != null) int.TryParse(variantSalt, out v);
            var shell = v > 0 && v < Blocks.Colors.Length ? TextureGen.DyeColors[Blocks.Colors[v]] : C(0x946794);
            Box(cv, def.Get("base"), Sh(shell, 0.8f), 0.08f);
            Box(cv, def.Get("lid"), shell, 0.08f);
            var lid = def.Get("lid");
            var r = cv.R(lid.cubes[0], 0);
            for (int i = 0; i < r.width; i += 3) cv.Fill(r.x + i, r.y, 1, r.height, Sh(shell, 0.7f));
            Box(cv, def.Get("head"), C(0x3A2A3A), 0.05f);
            var head = def.Get("head");
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + 1, hr.y + hr.height / 4, Mathf.Max(2, hr.width / 3), Mathf.Max(1, hr.height / 5), Glow(C(0xE8D040)));
        }

        static void BatSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 fur = C(0x4C3E30), wing = C(0x2A2018);
            Box(cv, def.Get("body"), fur, 0.08f);
            Box(cv, def.Get("head"), fur, 0.08f);
            foreach (var w in new[] { "right_wing", "left_wing" }) { var b = def.Get(w); if (b != null) foreach (var c in b.cubes) cv.AllFaces(c, wing, 0.1f); }
            var head = def.Get("head");
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + 1, hr.y + hr.height / 3, 1, 1, C(0xD02020));
            cv.Fill(hr.x + hr.width - 2, hr.y + hr.height / 3, 1, 1, C(0xD02020));
            foreach (var e in new[] { "ear_r", "ear_l" }) if (def.Has(e)) Box(cv, def.Get(e), Sh(fur, 1.1f), 0.06f);
        }

        static void BeeSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 fur = C(0xEDC343), stripe = C(0x2A1A0A);
            Box(cv, def.Get("body"), fur, 0.06f);
            var body = def.Get("body");
            var br = cv.R(body.cubes[0], 3);
            for (int i = 1; i < 3; i++) cv.Fill(br.x + i * br.width / 3, br.y, Mathf.Max(1, br.width / 8), br.height, stripe);
            foreach (var w in new[] { "right_wing", "left_wing" }) { var b = def.Get(w); if (b != null) foreach (var c in b.cubes) cv.AllFaces(c, new Color32(220, 240, 255, 150), 0.05f); }
            if (def.Has("head")) { Box(cv, def.Get("head"), fur, 0.06f); var hr = cv.R(def.Get("head").cubes[0], 3); cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 4), C(0x1A1A1A)); cv.Fill(hr.x + hr.width - 2 - Mathf.Max(1, hr.width / 5), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 4), C(0x1A1A1A)); }
            foreach (var n in new[] { "antenna_r", "antenna_l" }) if (def.Has(n)) foreach (var c in def.Get(n).cubes) cv.AllFaces(c, C(0x1A1A0A), 0.05f);
            if (def.Has("legs")) Box(cv, def.Get("legs"), C(0x2A1A0A), 0.06f);
            if (def.Has("stinger")) cv.AllFaces(def.Get("stinger").cubes[0], C(0x8A2010), 0.05f);
        }

        static void ParrotSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 red = C(0xD82A2A), blue = C(0x2A6AD8), yellow = C(0xE8C830);
            Box(cv, def.Get("body"), red, 0.08f);
            BoxSides(cv, def.Get("body"), blue);
            var head = def.Get("head");
            Box(cv, head, red, 0.06f);
            if (head.cubes.Count > 1) cv.AllFaces(head.cubes[1], C(0x3A3A3A), 0.05f);
            if (head.cubes.Count > 2) cv.AllFaces(head.cubes[2], yellow, 0.06f);
            foreach (var w in new[] { "right_wing", "left_wing" }) { var b = def.Get(w); if (b != null) { Box(cv, b, blue, 0.08f); if (b.cubes.Count > 0) cv.Face(b.cubes[0], 0, yellow, 0.08f); } }
            if (def.Has("tail")) Box(cv, def.Get("tail"), C(0xE06030), 0.08f);
            foreach (var l in new[] { "right_leg", "left_leg" }) { var b = def.Get(l); if (b != null) Box(cv, b, C(0x8A8A8A), 0.06f); }
        }

        static void SquidSkin(SkinCanvas cv, ModelDef def, string kind)
        {
            bool glow = kind == "glow_squid";
            Color32 body = glow ? C(0x095656) : C(0x223B4D), spots = glow ? C(0x85F1BC) : C(0x708899);
            Box(cv, def.Get("body"), body, 0.08f);
            Mark(cv, def.Get("body"), 3, spots, 0.12f);
            foreach (var b in def.bones) if (b.name.StartsWith("tentacle")) Box(cv, b, Sh(body, 1.1f), 0.1f);
            if (glow) { var r = cv.R(def.Get("body").cubes[0], 3); cv.Fill(r.x + r.width / 3, r.y + r.height / 3, Mathf.Max(1, r.width / 3), Mathf.Max(1, r.height / 3), Glow(C(0xA8FFF0))); }
        }

        static void FishSkin(SkinCanvas cv, ModelDef def, string kind, string variantSalt)
        {
            int v = 0; if (variantSalt != null) int.TryParse(variantSalt, out v);
            Color32[] coats = { C(0xC1A76A), C(0xE8D8A0), C(0x8A7A4A) };
            Color32 body = kind == "salmon" ? C(0x8A3A34) : kind == "tropical_fish" ? C(0xF0902A) : coats[v % coats.Length];
            Color32 fin = kind == "salmon" ? C(0x6A2A26) : Sh(body, 0.8f);
            foreach (var b in def.bones) Box(cv, b, body, 0.08f);
            if (def.Has("fin_top")) Box(cv, def.Get("fin_top"), fin, 0.08f);
            BoxBottom(cv, def.Get("body"), Sh(body, 1.25f));
            var head = def.Get("head");
            var hr = cv.R(head.cubes[0], 3);
            if (kind == "tropical_fish")
            {
                cv.Fill(hr.x + 1, hr.y + 1, Mathf.Max(1, hr.width / 3), Mathf.Max(1, hr.height / 3), C(0xFFFFFF));
                cv.Fill(hr.x + hr.width / 2, hr.y + hr.height / 2, Mathf.Max(1, hr.width / 3), 1, C(0x2A2A2A));
            }
            cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 6), Mathf.Max(1, hr.height / 5), C(0x1A1A1A));
            cv.Fill(hr.x + hr.width - 2 - Mathf.Max(1, hr.width / 6), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 6), Mathf.Max(1, hr.height / 5), C(0x1A1A1A));
            if (kind == "salmon") Mark(cv, def.Get("body"), 3, C(0xD8A0A0), 0.16f);
        }

        static void PufferSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 body = C(0xE8C040), spike = C(0xF0E8A0), belly = C(0xF8F0C0);
            Box(cv, def.Get("body"), body, 0.08f);
            BoxBottom(cv, def.Get("body"), belly);
            if (def.Has("spikes")) foreach (var c in def.Get("spikes").cubes) cv.AllFaces(c, spike, 0.12f);
            foreach (var b in def.bones) if (b.name.StartsWith("spike")) foreach (var c in b.cubes) cv.AllFaces(c, spike, 0.12f);
            var head = def.Get("head");
            if (head != null && head.cubes.Count > 0) { var hr = cv.R(head.cubes[0], 3); cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 5), C(0x1A1A1A)); cv.Fill(hr.x + hr.width - 2 - Mathf.Max(1, hr.width / 5), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 5), C(0x1A1A1A)); }
            if (def.Has("tail")) Box(cv, def.Get("tail"), Sh(body, 0.85f), 0.08f);
        }

        static void DolphinSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 top = C(0x2A4A6A), belly = C(0xD8E8F0);
            Box(cv, def.Get("body"), top, 0.07f);
            BoxBottom(cv, def.Get("body"), belly);
            if (def.Has("fin_top")) Box(cv, def.Get("fin_top"), Sh(top, 0.9f), 0.08f);
            foreach (var f in new[] { "fin_r", "fin_l" }) if (def.Has(f)) Box(cv, def.Get(f), Sh(top, 1.1f), 0.08f);
            var head = def.Get("head");
            Box(cv, head, top, 0.06f);
            if (head.cubes.Count > 1) cv.AllFaces(head.cubes[1], belly, 0.05f);
            foreach (var b in def.bones) if (b.name.StartsWith("tail")) Box(cv, b, Sh(top, 0.95f), 0.08f);
            if (def.Has("tail_fin")) Box(cv, def.Get("tail_fin"), Sh(top, 0.9f), 0.08f);
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 6), Mathf.Max(1, hr.height / 5), C(0x1A1A1A));
            cv.Fill(hr.x + hr.width - 2 - Mathf.Max(1, hr.width / 6), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 6), Mathf.Max(1, hr.height / 5), C(0x1A1A1A));
        }

        static void GuardianSkin(SkinCanvas cv, ModelDef def, string kind)
        {
            bool elder = kind == "elder_guardian";
            Color32 body = elder ? C(0xCECCBA) : C(0x5A8272), spike = elder ? C(0x9A9A8A) : C(0x3A6252), fin = C(0xF17D30);
            Box(cv, def.Get("body"), body, 0.08f);
            foreach (var b in def.bones) if (b.name.StartsWith("spike")) Box(cv, b, spike, 0.1f);
            foreach (var b in def.bones) if (b.name.StartsWith("tail")) Box(cv, b, body, 0.08f);
            if (def.Has("fin")) Box(cv, def.Get("fin"), fin, 0.1f);
            if (def.Has("eye")) Box(cv, def.Get("eye"), C(0xE8E8E8), 0.03f);
            var head = def.Get("head");
            if (head != null && head.cubes.Count > 0)
            {
                var hr = cv.R(head.cubes[0], 3);
                cv.Fill(hr.x + hr.width / 5, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 6), Mathf.Max(1, hr.height / 5), C(0xE8E8D0));
                cv.Fill(hr.x + hr.width - hr.width / 5 - Mathf.Max(1, hr.width / 6), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 6), Mathf.Max(1, hr.height / 5), C(0xE8E8D0));
            }
            // eye cube glow
            if (def.Has("eye")) { var c = def.Get("eye").cubes[0]; cv.Face(c, 3, Glow(C(0xF17D30))); }
        }

        static void PhantomSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 body = C(0x43518A), wing = C(0x2A3468), eye = C(0x88FF00);
            foreach (var b in def.bones)
            {
                Box(cv, b, b.name.Contains("wing") ? wing : body, 0.08f);
                if (b.name.Contains("wing")) { foreach (var c in b.cubes) cv.Face(c, 0, C(0x1A2048), 0.1f); }
            }
            var head = def.Get("head");
            var hr = cv.R(head.cubes[0], 3);
            cv.Fill(hr.x + hr.width / 3, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 5), Glow(eye));
            cv.Fill(hr.x + hr.width - hr.width / 3 - Mathf.Max(1, hr.width / 5), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 5), Glow(eye));
        }

        static void BugSkin(SkinCanvas cv, ModelDef def, string kind)
        {
            Color32 body = kind == "endermite" ? C(0x2A2A3A) : C(0x6E6E6E), dark = Sh(body, 0.7f);
            foreach (var b in def.bones) Box(cv, b, body, 0.1f);
            foreach (var b in def.bones) { var r0 = b.cubes.Count > 0 ? cv.R(b.cubes[0], 0) : default; if (r0.width > 0) cv.Fill(r0.x, r0.y + r0.height / 2, r0.width, 1, dark); }
            var head = def.Get("head");
            if (head != null && head.cubes.Count > 0)
            {
                var hr = cv.R(head.cubes[0], 3);
                cv.Fill(hr.x + 1, hr.y + hr.height / 3, 1, 1, C(0xD02020));
                cv.Fill(hr.x + hr.width - 2, hr.y + hr.height / 3, 1, 1, C(0xD02020));
            }
        }

        static void TadpoleSkin(SkinCanvas cv, ModelDef def)
        {
            var body = C(0x6D533D);
            Box(cv, def.Get("body"), body, 0.08f);
            Box(cv, def.Get("tail"), Sh(body, 0.85f), 0.1f);
            var head = def.Get("head");
            if (head != null && head.cubes.Count > 0) { var hr = cv.R(head.cubes[0], 3); cv.Fill(hr.x, hr.y + hr.height / 3, 1, 1, C(0x1A0A00)); cv.Fill(hr.x + hr.width - 1, hr.y + hr.height / 3, 1, 1, C(0x1A0A00)); }
        }

        static void NautilusSkin(SkinCanvas cv, ModelDef def, string kind)
        {
            bool zombie = kind == "zombie_nautilus";
            Color32 shell = zombie ? C(0x6A8A6A) : C(0xE8D8C0), flesh = zombie ? C(0x3A5A3A) : C(0xB06040);
            Box(cv, def.Get("body"), shell, 0.08f);
            var r = cv.R(def.Get("body").cubes[0], 3);
            for (int i = 0; i < 4; i++) cv.Line(r.x + i * r.width / 4, r.y + r.height - 1, r.x + r.width / 2, r.y, Sh(shell, 0.7f));
            if (def.Has("face")) Box(cv, def.Get("face"), flesh, 0.06f);
            foreach (var b in def.bones) if (b.name.StartsWith("tentacle")) Box(cv, b, flesh, 0.08f);
        }

        static void DragonSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 body = C(0x1C1C1C), scale = C(0x303030), membrane = C(0x2A1A3A), eye = C(0xE079FA), edge = C(0x484848);
            foreach (var b in def.bones)
            {
                bool wing = b.name.Contains("wing");
                var baseC = wing ? membrane : body;
                foreach (var c in b.cubes)
                {
                    if (c.name != null && c.name.Contains("membrane")) { cv.AllFaces(c, new Color32(membrane.r, membrane.g, membrane.b, 200), 0.08f); continue; }
                    cv.AllFaces(c, baseC, wing ? 0.1f : 0.09f);
                }
                if (!wing) Mark(cv, b, 3, scale, 0.16f);
                if (b.name.StartsWith("tail") || b.name.StartsWith("neck")) { Mark(cv, b, 0, edge, 0.2f); }
            }
            var head = def.Get("head");
            var hr = cv.R(head.cubes[0], 3);
            int ew = Mathf.Max(2, hr.width / 6), eh = Mathf.Max(1, hr.height / 8);
            cv.Fill(hr.x + hr.width / 6, hr.y + hr.height / 3, ew, eh, Glow(eye));
            cv.Fill(hr.x + hr.width - hr.width / 6 - ew, hr.y + hr.height / 3, ew, eh, Glow(eye));
            if (head.cubes.Count > 1) Mark(cv, head, 3, C(0x0A0A0A), 0.2f);
            foreach (var n in new[] { "horn_r", "horn_l" }) if (def.Has(n)) Box(cv, def.Get(n), C(0x3A3A3A), 0.06f);
            if (def.Has("jaw")) Box(cv, def.Get("jaw"), C(0x2A2A2A), 0.08f);
        }

        static void WitherSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 bone = C(0x303038), dark = C(0x1A1A20), eye = Glow(C(0x6A9AE0));
            foreach (var b in def.bones)
            {
                Box(cv, b, bone, 0.08f);
                Mark(cv, b, 3, dark, 0.16f);
            }
            foreach (var hn in new[] { "head", "head_right", "head_left" })
            {
                var h = def.Get(hn); if (h == null) continue;
                var hr = cv.R(h.cubes[0], 3);
                cv.Fill(hr.x + 1, hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 6), eye);
                cv.Fill(hr.x + hr.width - 2 - Mathf.Max(1, hr.width / 5), hr.y + hr.height / 3, Mathf.Max(1, hr.width / 5), Mathf.Max(1, hr.height / 6), eye);
                for (int i = 3; i < hr.width - 3; i += 2) cv.Fill(hr.x + i, hr.y + hr.height - 2, 1, 1, C(0xE8E8E8));
            }
        }

        static void Fallback(SkinCanvas cv, ModelDef def)
        {
            var c = MathX.Hex((uint)(Hash.StringHash(def.name) & 0xFFFFFF) | 0x404040);
            foreach (var b in def.bones) Box(cv, b, c, 0.1f);
        }
    }

    public static class HeadExtensions
    {
        // helper kept for readability in painters
        public static bool Head(this ModelDef d) => d.Get("head") != null;
    }
}
