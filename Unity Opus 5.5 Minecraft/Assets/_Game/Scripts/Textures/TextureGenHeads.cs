using System;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Mob head textures for SkullBlock. The head is an 8x8x8 px box whose faces use world-aligned uvs, so a
    /// floor-standing head samples cols 4-11: its sides read rows 8-15 and its top rows 4-11. The
    /// <c>&lt;kind&gt;</c> tile is therefore head "material" (hair on the upper rows, skin or bone below) and the
    /// <c>&lt;kind&gt;_face</c> tile carries an 8x8 face at cols 4-11 / rows 8-15 on top of the same material.
    /// </summary>
    public static partial class TextureGen
    {
        static Img Heads(Img i, string n)
        {
            bool face = n.EndsWith("_face", StringComparison.Ordinal);
            string kind = face ? n.Substring(0, n.Length - 5) : n;
            HeadStyle s;
            switch (kind)
            {
                case "skeleton_skull": s = new HeadStyle { skin = C(0xC9C9C0), hair = C(0xC9C9C0), hairRows = 0, dark = C(0x2C2C2A), mottle = 0.07f }; break;
                case "wither_skeleton_skull": s = new HeadStyle { skin = C(0x333336), hair = C(0x333336), hairRows = 0, dark = C(0x0B0B0D), mottle = 0.1f }; break;
                case "zombie_head": s = new HeadStyle { skin = C(0x4E8A3E), hair = C(0x2E4A26), hairRows = 10, dark = C(0x14241A), mottle = 0.08f }; break;
                case "creeper_head": s = new HeadStyle { skin = C(0x5EAA48), hair = C(0x5EAA48), hairRows = 0, dark = C(0x0E1A0C), mottle = 0.22f }; break;
                case "dragon_head": s = new HeadStyle { skin = C(0x1E1E26), hair = C(0x2A2A34), hairRows = 10, dark = C(0x0A0A0E), mottle = 0.18f }; break;
                case "piglin_head": s = new HeadStyle { skin = C(0xD49A84), hair = C(0x4E3324), hairRows = 9, dark = C(0x3A2218), mottle = 0.06f }; break;
                case "player_head": s = new HeadStyle { skin = C(0xC48C62), hair = C(0x3A2818), hairRows = 10, dark = C(0x2A1C10), mottle = 0.05f }; break;
                default: return null;
            }
            HeadMaterial(i, s, kind);
            if (face) HeadFace(i, s, kind);
            return i;
        }

        struct HeadStyle { public Color32 skin, hair, dark; public int hairRows; public float mottle; }

        static void HeadMaterial(Img i, HeadStyle s, string kind)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    bool hair = y < s.hairRows;
                    // ragged hairline
                    if (s.hairRows > 0 && y == s.hairRows && (Hash.Get(x, 3, 77) & 3) == 0) hair = true;
                    Color32 c = hair ? s.hair : s.skin;
                    float f = 1f;
                    if (kind == "creeper_head" || kind == "dragon_head")
                        f = 0.82f + ((Hash.Get(x >> 1, y >> 1, 19) & 255) / 255f) * 0.36f;   // 2x2 camo / scale blocks
                    i[x, y] = TN(c, f, s.mottle);
                }
            if (kind == "skeleton_skull" || kind == "wither_skeleton_skull")
            {
                // hairline cracks in the bone
                i.Line(5, 2, 7, 5, Tone(s.skin, 0.8f)); i.Line(10, 9, 12, 12, Tone(s.skin, 0.8f));
            }
        }

        /// <summary>Paints the 8x8 face into cols 4-11, rows 8-15.</summary>
        static void HeadFace(Img i, HeadStyle s, string kind)
        {
            const int ox = 4, oy = 8;
            // start from clean skin across the face so the hair only frames it
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++)
                {
                    bool hair = s.hairRows > 0 && y < 1;
                    float f = 1f;
                    if (kind == "creeper_head" || kind == "dragon_head")
                        f = 0.82f + ((Hash.Get((x + ox) >> 1, (y + oy) >> 1, 19) & 255) / 255f) * 0.36f;
                    i[ox + x, oy + y] = TN(hair ? s.hair : s.skin, f, s.mottle);
                }
            void P(int x, int y, Color32 c) => i.Set(ox + x, oy + y, c);
            switch (kind)
            {
                case "skeleton_skull":
                case "wither_skeleton_skull":
                    {
                        Color32 socket = s.dark, rim = Tone(s.skin, 0.78f), tooth = Tone(s.skin, 1.12f);
                        for (int y = 2; y <= 3; y++) { P(1, y, socket); P(2, y, socket); P(5, y, socket); P(6, y, socket); }
                        P(1, 1, rim); P(2, 1, rim); P(5, 1, rim); P(6, 1, rim);
                        P(3, 4, socket); P(4, 4, socket);
                        for (int x = 1; x < 7; x++) P(x, 6, (x & 1) == 0 ? socket : tooth);
                        P(0, 7, rim); P(7, 7, rim);
                        break;
                    }
                case "zombie_head":
                    {
                        P(1, 3, s.dark); P(2, 3, C(0x2E5A4E)); P(5, 3, C(0x2E5A4E)); P(6, 3, s.dark);
                        P(1, 2, Tone(s.skin, 0.8f)); P(2, 2, Tone(s.skin, 0.8f)); P(5, 2, Tone(s.skin, 0.8f)); P(6, 2, Tone(s.skin, 0.8f));
                        P(3, 4, Tone(s.skin, 0.75f)); P(4, 4, Tone(s.skin, 0.75f));
                        P(3, 5, Tone(s.skin, 0.62f)); P(4, 5, Tone(s.skin, 0.62f));
                        for (int x = 2; x <= 5; x++) P(x, 6, s.dark);
                        P(1, 7, Tone(s.skin, 0.85f));
                        break;
                    }
                case "creeper_head":
                    {
                        Color32 k = s.dark;
                        for (int y = 2; y <= 3; y++) { P(1, y, k); P(2, y, k); P(5, y, k); P(6, y, k); }
                        for (int y = 4; y <= 6; y++) { P(3, y, k); P(4, y, k); }
                        for (int y = 5; y <= 7; y++) { P(2, y, k); P(5, y, k); }
                        break;
                    }
                case "dragon_head":
                    {
                        Color32 eye = C(0xC060F0), eyeD = C(0x7A2EA8);
                        P(0, 3, s.dark); P(1, 3, eye); P(2, 3, eyeD);
                        P(5, 3, eyeD); P(6, 3, eye); P(7, 3, s.dark);
                        P(1, 2, s.hair); P(2, 2, s.hair); P(5, 2, s.hair); P(6, 2, s.hair);
                        P(2, 6, s.dark); P(5, 6, s.dark);
                        for (int x = 1; x < 7; x++) P(x, 7, (x & 1) == 0 ? C(0xD8D8E0) : s.dark);
                        break;
                    }
                case "piglin_head":
                    {
                        Color32 snout = Tone(s.skin, 1.12f), snoutD = Tone(s.skin, 0.78f);
                        P(1, 2, s.dark); P(2, 2, C(0xE8E0D0)); P(5, 2, C(0xE8E0D0)); P(6, 2, s.dark);
                        for (int y = 3; y <= 6; y++) for (int x = 2; x <= 5; x++) P(x, y, snout);
                        for (int x = 2; x <= 5; x++) { P(x, 3, Tone(snout, 1.06f)); P(x, 6, snoutD); }
                        P(3, 5, s.dark); P(4, 5, s.dark);
                        P(1, 6, C(0xEDE4CC)); P(1, 7, C(0xD2C8AE)); P(6, 6, C(0xEDE4CC)); P(6, 7, C(0xD2C8AE));
                        break;
                    }
                case "player_head":
                    {
                        // hair frames the face, then eyes, nose and mouth
                        for (int y = 0; y < 2; y++) for (int x = 0; x < 8; x++) P(x, y, TN(s.hair, 1f, 0.12f));
                        P(0, 2, s.hair); P(7, 2, s.hair);
                        P(1, 3, C(0xF2F2F2)); P(2, 3, C(0x3A5AA8)); P(5, 3, C(0x3A5AA8)); P(6, 3, C(0xF2F2F2));
                        P(3, 4, Tone(s.skin, 0.86f)); P(4, 4, Tone(s.skin, 0.86f));
                        P(3, 5, Tone(s.skin, 0.76f)); P(4, 5, Tone(s.skin, 0.76f));
                        for (int x = 2; x <= 5; x++) P(x, 6, C(0x7A4A34));
                        P(2, 7, Tone(s.skin, 0.9f)); P(5, 7, Tone(s.skin, 0.9f));
                        break;
                    }
            }
        }
    }
}
