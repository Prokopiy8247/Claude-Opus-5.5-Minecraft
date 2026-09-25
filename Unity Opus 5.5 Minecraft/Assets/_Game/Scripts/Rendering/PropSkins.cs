using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Original skins for the non-mob models: boats, minecarts, chests, torches/lanterns and campfires are blocks,
    /// so this covers the props that are real 3D models (vehicles, end crystals, held weapons, armour overlays).
    /// </summary>
    public static class PropSkins
    {
        static readonly HashSet<string> handled = new HashSet<string>
        {
            "minecart", "boat", "chest_boat", "raft", "end_crystal",
            "chest", "ender_chest", "trapped_chest", "copper_chest",
            "shield", "trident", "spear", "bow", "crossbow", "armor"
        };

        public static bool Handles(string skin) => skin != null && handled.Contains(skin);

        static Color32 C(uint rgb, byte a = 255) => new Color32((byte)(rgb >> 16), (byte)(rgb >> 8), (byte)rgb, a);
        static Color32 Glow(Color32 c) { c.a = 190; return c; }
        static Color32 Sh(Color32 c, float f) => Img.Shade(c, f);

        public static void Paint(SkinCanvas cv, ModelDef def, string skin, string variantSalt)
        {
            switch (skin)
            {
                case "minecart": Minecart(cv, def); break;
                case "boat": BoatSkin(cv, def, false); break;
                case "chest_boat": BoatSkin(cv, def, true); break;
                case "raft": RaftSkin(cv, def); break;
                case "end_crystal": EndCrystalSkin(cv, def); break;
                case "chest": ChestSkin(cv, def, C(0x8B6239), C(0x6A4A28), C(0x2A2A2A)); break;
                case "trapped_chest": ChestSkin(cv, def, C(0x8B6239), C(0x6A4A28), C(0xC03030)); break;
                case "ender_chest": ChestSkin(cv, def, C(0x2A2A38), C(0x1A1A24), C(0x2AD0B0)); break;
                case "copper_chest": ChestSkin(cv, def, C(0xC07850), C(0x8E5636), C(0x3A2A20)); break;
                case "shield": ShieldSkin(cv, def); break;
                case "trident": TridentSkin(cv, def, C(0x3AA8A0), C(0x1E6E6A)); break;
                case "spear": TridentSkin(cv, def, C(0xA0824E), C(0x6A5230)); break;
                case "bow": BowSkin(cv, def); break;
                case "crossbow": CrossbowSkin(cv, def); break;
                case "armor": ArmorSkin(cv, def, variantSalt); break;
                default: Fallback(cv, def); break;
            }
        }

        static void Fallback(SkinCanvas cv, ModelDef def)
        {
            foreach (var b in def.bones)
                foreach (var c in b.cubes) cv.AllFaces(c, C(0xB0B0B8), 0.05f);
        }

        // ------------------------------------------------------------------ vehicles
        static void Minecart(SkinCanvas cv, ModelDef def)
        {
            Color32 iron = C(0x6E6E76), dark = C(0x3E3E46), rim = C(0x9A9AA4);
            foreach (var b in def.bones)
                foreach (var c in b.cubes)
                {
                    bool floor = c.name == "bottom";
                    cv.AllFaces(c, floor ? dark : iron, 0.06f);
                    cv.Face(c, 0, floor ? dark : rim, 0.04f); // tops catch light
                    if (!floor)
                    {
                        // a couple of vertical rivet bands on the side walls
                        var r = cv.R(c, 2);
                        for (int i = 1; i < r.width; i += 4) cv.Line(r.x + i, r.y + 1, r.x + i, r.y + r.height - 2, Sh(iron, 1.25f));
                    }
                }
        }

        static void BoatSkin(SkinCanvas cv, ModelDef def, bool chest)
        {
            Color32 plank = C(0x9E7A4E), plankDark = C(0x6E5433), inner = C(0x7A5C38);
            foreach (var b in def.bones)
            {
                foreach (var cube in b.cubes)
                {
                    if (cube.name == "chest") { ChestSkin(cv, def, C(0x8B6239), C(0x6A4A28), C(0x2A2A2A), b, cube, true); continue; }
                    cv.AllFaces(cube, plank, 0.07f);
                    cv.Face(cube, 0, plankDark, 0.05f);   // deck top
                    cv.Face(cube, 1, inner, 0.05f);       // underside/inside
                    // plank seams down the hull sides
                    foreach (int f in new[] { 2, 4 })
                    {
                        var r = cv.R(cube, f);
                        for (int i = 2; i < r.height; i += 4) cv.Line(r.x + 1, r.y + i, r.x + r.width - 2, r.y + i, plankDark);
                    }
                }
                if (b.name.StartsWith("paddle"))
                {
                    foreach (var cube in b.cubes) cv.AllFaces(cube, cube.name == "blade" ? C(0x8A6A3A) : plankDark, 0.05f);
                }
            }
        }

        static void RaftSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 log = C(0x8A6A3A), logDark = C(0x5E4524);
            foreach (var b in def.bones)
                foreach (var cube in b.cubes)
                {
                    cv.AllFaces(cube, cube.name == "deck" ? log : logDark, 0.08f);
                    // individual log stripes across the deck
                    if (cube.name == "deck")
                    {
                        var r = cv.R(cube, 0);
                        for (int i = 2; i < r.width; i += 3) cv.Line(r.x + i, r.y + 1, r.x + i, r.y + r.height - 2, logDark);
                    }
                }
        }

        static void EndCrystalSkin(SkinCanvas cv, ModelDef def)
        {
            foreach (var b in def.bones)
                foreach (var cube in b.cubes)
                {
                    if (cube.name == "glass" || cube.name == "glass2")
                    {
                        // translucent shell with a glowing lattice
                        var glow = Glow(C(0xE07CF0));
                        cv.AllFaces(cube, new Color32(0xC0, 0x60, 0xE0, 90), 0.05f);
                        var r = cv.R(cube, 3);
                        for (int i = 0; i < r.height; i += 2) cv.Line(r.x, r.y + i, r.x + r.width - 1, r.y + i, glow);
                        for (int i = 0; i < r.width; i += 2) cv.Line(r.x + i, r.y, r.x + i, r.y + r.height - 1, glow);
                    }
                    else if (cube.name == "core")
                    {
                        cv.AllFaces(cube, Glow(C(0xFFE8A0)), 0.04f);
                    }
                    else
                    {
                        cv.AllFaces(cube, C(0x3A2A3A), 0.06f);
                        cv.Face(cube, 0, C(0x54405A), 0.05f);
                    }
                }
        }

        // ------------------------------------------------------------------ chests
        static void ChestSkin(SkinCanvas cv, ModelDef def, Color32 main, Color32 dark, Color32 lockColor)
            => ChestSkin(cv, def, main, dark, lockColor, null, null, false);

        static void ChestSkin(SkinCanvas cv, ModelDef def, Color32 main, Color32 dark, Color32 lockColor, BoneDef onlyBone, CubeDef onlyCube, bool inheritSlot)
        {
            foreach (var b in def.bones)
            {
                if (onlyBone != null && b != onlyBone) continue;
                foreach (var cube in b.cubes)
                {
                    if (onlyCube != null && cube != onlyCube) continue;
                    if (cube.name == "lock")
                    {
                        cv.AllFaces(cube, lockColor, 0.03f);
                        continue;
                    }
                    cv.AllFaces(cube, main, 0.07f);
                    // raised border on the front face and a hinge band on the back face
                    var front = cv.R(cube, 3);
                    for (int i = 0; i < front.width; i++) { cv.Set(front.x + i, front.y, dark); cv.Set(front.x + i, front.y + front.height - 1, dark); }
                    for (int j = 0; j < front.height; j++) { cv.Set(front.x, front.y + j, dark); cv.Set(front.x + front.width - 1, front.y + j, dark); }
                    if (cube.name == "lid")
                    {
                        var top = cv.R(cube, 0);
                        cv.Speck(top.x, top.y, top.width, top.height, Sh(main, 1.08f), 0.08f);
                    }
                }
            }
        }

        // ------------------------------------------------------------------ held items
        static void ShieldSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 plate = C(0x6E4A2A), rim = C(0x4A2E18), face = C(0xC8C8CC), boss = C(0xB8B8C0);
            foreach (var b in def.bones)
                foreach (var cube in b.cubes)
                {
                    if (cube.name == "handle") { cv.AllFaces(cube, rim, 0.04f); continue; }
                    cv.AllFaces(cube, plate, 0.06f);
                    // a plain round metal face in the middle of the plank shield
                    var f = cv.R(cube, 3);
                    int cx = f.x + f.width / 2, cy = f.y + f.height / 2;
                    for (int y = 0; y < f.height; y++)
                        for (int x = 0; x < f.width; x++)
                        {
                            float dx = (x - f.width / 2f) / (f.width * 0.34f), dy = (y - f.height / 2f) / (f.height * 0.36f);
                            if (dx * dx + dy * dy <= 1f) cv.Set(f.x + x, f.y + y, face);
                        }
                    cv.Fill(cx - 1, cy - 1, 2, 2, boss);
                    for (int i = 0; i < f.height; i++) { cv.Set(f.x, f.y + i, rim); cv.Set(f.x + f.width - 1, f.y + i, rim); }
                }
        }

        static void TridentSkin(SkinCanvas cv, ModelDef def, Color32 metal, Color32 metalDark)
        {
            foreach (var b in def.bones)
                foreach (var cube in b.cubes)
                {
                    bool shaft = cube.name == "grip" || b.name == "shaft" && cube.name == null;
                    cv.AllFaces(cube, shaft ? C(0x6E7A6A) : metal, 0.05f);
                    var f = cv.R(cube, 3);
                    for (int i = 0; i < f.height; i += 3) cv.Set(f.x, f.y + i, metalDark);
                    if (cube.name == "point") cv.AllFaces(cube, Sh(metal, 1.3f), 0.03f);
                }
        }

        static void BowSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 wood = C(0x8A6A3A), wooden = C(0x5E4524), stringC = C(0xE8E8E0);
            foreach (var b in def.bones)
                foreach (var cube in b.cubes)
                {
                    if (cube.name == "string") { cv.AllFaces(cube, stringC, 0.02f); continue; }
                    cv.AllFaces(cube, b.name == "grip" ? wooden : wood, 0.06f);
                    // limb tips are lighter, mimicking a carved taper
                    if (b.name.EndsWith("_limb"))
                    {
                        var f = cv.R(cube, 3);
                        cv.Rect(f.x, f.y, f.width, 2, Sh(wood, 1.2f), 0.03f);
                        cv.Rect(f.x, f.y + f.height - 2, f.width, 2, wooden, 0.03f);
                    }
                }
        }

        static void CrossbowSkin(SkinCanvas cv, ModelDef def)
        {
            Color32 wood = C(0x7A5A32), metal = C(0x9A9AA4), stringC = C(0xE8E8E0);
            foreach (var b in def.bones)
                foreach (var cube in b.cubes)
                {
                    if (cube.name == "string") { cv.AllFaces(cube, stringC, 0.02f); continue; }
                    if (b.name == "limbs") { cv.AllFaces(cube, metal, 0.05f); continue; }
                    cv.AllFaces(cube, wood, 0.06f);
                    var f = cv.R(cube, 3);
                    cv.Rect(f.x, f.y + f.height / 2, f.width, 2, metal, 0.03f);
                }
        }

        static readonly Dictionary<string, (Color32 main, Color32 trim)> ArmorPalette = new Dictionary<string, (Color32, Color32)>
        {
            {"leather", (C(0xA0643E), C(0x70432A))}, {"copper", (C(0xD2784E), C(0x8E4A2E))}, {"chainmail", (C(0x9A9AA4), C(0x6A6A74))},
            {"iron", (C(0xDCDCDC), C(0xA0A0A0))}, {"golden", (C(0xF4D050), C(0xC09020))}, {"diamond", (C(0x5CE0D4), C(0x2A9A90))},
            {"netherite", (C(0x564C54), C(0x362E34))}, {"turtle", (C(0x4AA04A), C(0x2A6A2A))},
        };

        static void ArmorSkin(SkinCanvas cv, ModelDef def, string variantSalt)
        {
            string kind = variantSalt ?? "iron";
            if (!ArmorPalette.TryGetValue(kind, out var pal)) pal = ArmorPalette["iron"];
            var trim = pal.trim;
            foreach (var b in def.bones)
                foreach (var cube in b.cubes)
                {
                    cv.AllFaces(cube, pal.main, 0.05f);
                    // banding on the trim rows so plate armour reads as layered
                    foreach (int f in new[] { 2, 3, 4, 5 })
                    {
                        var r = cv.R(cube, f);
                        cv.Line(r.x, r.y, r.x + r.width - 1, r.y, trim);
                        cv.Line(r.x, r.y + r.height - 1, r.x + r.width - 1, r.y + r.height - 1, trim);
                    }
                    if (cube.name != null && (cube.name.StartsWith("helmet") || cube.name.StartsWith("chest")))
                    {
                        // a simple highlight so the front plate catches the eye
                        var r = cv.R(cube, 3);
                        cv.Rect(r.x + 1, r.y + 2, r.width - 2, 1, Sh(pal.main, 1.25f), 0.02f);
                    }
                }
        }
    }
}
