using System;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Furniture, storage and machinery: furnaces and their lit states, dispensers, hoppers, jukebox,
    /// spawners, scaffolding, froglights, rails, the end/respawn blocks and small decorative odds and ends.
    /// </summary>
    public static partial class TextureGen
    {
        // ---------------------------------------------------------------- furnace palette
        static readonly Color32 FStone = C(0x6E6E72);
        static readonly Color32 FStoneD = C(0x4A4A4E);
        static readonly Color32 FFlue = C(0x24242A);

        static Img Furniture(Img i, string n)
        {
            switch (n)
            {
                // -------------------------------------------------- furnace
                case "furnace_side": return FurnaceSide(i);
                case "furnace_top": return FurnaceTop(i);
                case "furnace_front": return FurnaceFront(i, false);
                case "furnace_front_on": return FurnaceFront(i, true);
                // -------------------------------------------------- smoker (oak casing, iron mouth)
                case "smoker_side": return SmokerSide(i);
                case "smoker_top": return SmokerTop(i);
                case "smoker_front": return SmokerFront(i, false);
                case "smoker_front_on": return SmokerFront(i, true);
                // -------------------------------------------------- blast furnace (brick casing)
                case "blast_furnace_side": return BlastSide(i);
                case "blast_furnace_top": return BlastTop(i);
                case "blast_furnace_front": return BlastFront(i, false);
                case "blast_furnace_front_on": return BlastFront(i, true);
                // -------------------------------------------------- dispenser / dropper
                case "dispenser_front": return DispenserFace(i, false, false);
                case "dispenser_front_vertical": return DispenserFace(i, true, false);
                case "dropper_front": return DispenserFace(i, false, true);
                case "dropper_front_vertical": return DispenserFace(i, true, true);
                // -------------------------------------------------- hopper
                case "hopper_outside": return HopperOutside(i);
                case "hopper_inside": return HopperInside(i);
                case "hopper_top": return HopperTop(i);
                // -------------------------------------------------- jukebox
                case "jukebox_side":
                    {
                        var dk = Wood("dark_oak");
                        WoodBody(i, dk, 0.95f);
                        i.RectOutline(0, 0, 16, 16, Tone(dk.plankDark, 0.8f));
                        i.HLine(1, 0, 15, Tone(dk.plank, 1.1f));
                        i.HLine(3, 0, 15, Tone(dk.plankDark, 1.05f));
                        i.HLine(15, 0, 15, Tone(dk.plankDark, 0.8f));
                        return i;
                    }
                case "jukebox_top":
                    {
                        var dk = Wood("dark_oak");
                        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++)
                            i[x, y] = Img.Shade(dk.plank, 0.9f + (Rnd01() - 0.5f) * 0.08f);
                        // record slot with a black disc: grooves, red label, spindle hole
                        i.Rect(2, 2, 12, 12, Tone(dk.plankDark, 0.7f));
                        for (int y = 2; y < 14; y++)
                            for (int x = 2; x < 14; x++)
                            {
                                float d = Mathf.Sqrt((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f));
                                if (d > 5.7f) continue;
                                Color32 c = ((int)(d * 1.4f) & 1) == 0 ? C(0x1C1C22) : C(0x2C2C34);
                                if (d < 2.2f) c = d < 0.8f ? C(0x0E0E10) : C(0xB42A26);
                                i[x, y] = c;
                            }
                        i.Set(6, 6, C(0xD84A42)); i.Set(4, 5, C(0x4A4A56)); i.Set(5, 4, C(0x4A4A56));
                        i.RectOutline(0, 0, 16, 16, Tone(dk.plankDark, 0.8f));
                        i.Set(3, 3, C(0x4A4A52));
                        return i;
                    }
                // -------------------------------------------------- spawners
                case "spawner": return Spawner(i, false);
                case "trial_spawner": return Spawner(i, true);
                // -------------------------------------------------- scaffolding
                case "scaffolding_side":
                    {
                        i.Clear();
                        var bam = Wood("bamboo").bark;
                        Color32 dk = Img.Shade(bam, 0.7f), lt = Img.Shade(bam, 1.18f);
                        for (int x = 0; x < 16; x += 5)
                        {
                            i.VLine(x, 0, 15, dk); i.VLine(x + 1, 0, 15, lt); i.VLine(x + 2, 0, 15, bam);
                        }
                        for (int y = 1; y < 16; y += 6) { i.HLine(y, 0, 15, lt); i.HLine(y + 1, 0, 15, dk); }
                        return i;
                    }
                case "scaffolding_top":
                case "scaffolding_bottom":
                    {
                        i.Clear();
                        var bam = Wood("bamboo").bark;
                        Color32 dk = Img.Shade(bam, 0.7f), lt = Img.Shade(bam, 1.18f);
                        for (int y = 0; y < 16; y += 5)
                        {
                            i.HLine(y, 0, 15, dk); i.HLine(y + 1, 0, 15, lt); i.HLine(y + 2, 0, 15, bam);
                        }
                        for (int x = 1; x < 16; x += 6) { i.VLine(x, 0, 15, lt); i.VLine(x + 1, 0, 15, dk); }
                        return i;
                    }
                // -------------------------------------------------- froglights
                case "ochre_froglight_side": return Froglight(i, C(0xD9B65C), C(0xF6E3A4), C(0x8E6F32), false);
                case "ochre_froglight_top": return Froglight(i, C(0xE4C86E), C(0xFFF3C4), C(0x9A7A38), true);
                case "verdant_froglight_side": return Froglight(i, C(0xC2D6AE), C(0xEAF6E0), C(0x7E9668), false);
                case "verdant_froglight_top": return Froglight(i, C(0xD2E4C0), C(0xF6FFF2), C(0x8AA474), true);
                case "pearlescent_froglight_side": return Froglight(i, C(0xDCC2CE), C(0xF8E8F0), C(0xA28896), false);
                case "pearlescent_froglight_top": return Froglight(i, C(0xEAD0DC), C(0xFFF6FC), C(0xB49AA8), true);
                // -------------------------------------------------- slime & honey
                case "slime_block": return SlimeBlock(i);
                case "honey_block_side": return HoneySide(i);
                case "honey_block_top": return HoneyTop(i);
                case "honey_block_bottom": return HoneyBottom(i);
                // -------------------------------------------------- iron door / trapdoor
                case "iron_door_top": return IronDoor(i, true);
                case "iron_door_bottom": return IronDoor(i, false);
                case "iron_trapdoor": return IronTrapdoor(i);
                // -------------------------------------------------- rails
                case "rail": return Rail(i, RailIron, RailKindMark.None, false);
                case "rail_corner": return RailCorner(i);
                case "powered_rail": return Rail(i, RailGold, RailKindMark.Redstone, false);
                case "powered_rail_on": return Rail(i, RailGold, RailKindMark.Redstone, true);
                case "detector_rail": return Rail(i, RailIron, RailKindMark.Plate, false);
                case "detector_rail_on": return Rail(i, RailIron, RailKindMark.Plate, true);
                case "activator_rail": return Rail(i, RailIron, RailKindMark.Redstone, false, true);
                case "activator_rail_on": return Rail(i, RailIron, RailKindMark.Redstone, true, true);
                // -------------------------------------------------- end blocks
                case "end_portal_frame_side": return EndFrameSide(i);
                case "end_portal_frame_top": return EndFrameTop(i, false);
                case "end_portal_frame_eye": return EndFrameTop(i, true);
                case "end_rod": return EndRod(i);
                case "lightning_rod": return LightningRod(i);
                // -------------------------------------------------- respawn anchor
                case "respawn_anchor_side": return AnchorSide(i);
                case "respawn_anchor_bottom": return AnchorBottom(i);
                case "respawn_anchor_top_0": case "respawn_anchor_top_1": case "respawn_anchor_top_2":
                case "respawn_anchor_top_3": case "respawn_anchor_top_4":
                    return AnchorTop(i, n[n.Length - 1] - '0');
                // -------------------------------------------------- barrel
                case "barrel_side": return BarrelSide(i);
                case "barrel_top": return BarrelEnd(i, false);
                case "barrel_top_open": return BarrelEnd(i, true);
                case "barrel_bottom": return BarrelEnd(i, false);
                // -------------------------------------------------- lily pad (grayscale; tinted by the block)
                case "lily_pad": return LilyPad(i);
                // -------------------------------------------------- lodestone
                case "lodestone_side": return LodestoneSide(i);
                case "lodestone_top": return LodestoneTop(i);
                // -------------------------------------------------- flower pot (quads sample the whole tile)
                case "flower_pot": return FlowerPot(i);
                // -------------------------------------------------- dragon egg
                case "dragon_egg": return DragonEgg(i);
                // -------------------------------------------------- beacon core
                case "beacon": return BeaconCore(i);
                // -------------------------------------------------- bell
                case "bell_body": return BellBody(i);
                // -------------------------------------------------- brewing stand
                case "brewing_stand": return BrewingStand(i);
                case "brewing_stand_base": return BrewingBase(i);
                // -------------------------------------------------- cauldron
                case "cauldron_side": return CauldronSide(i);
                case "cauldron_top": return CauldronTop(i);
                case "cauldron_bottom": return CauldronBottom(i);
                case "cauldron_inner": return CauldronInner(i);
                // -------------------------------------------------- composter
                case "composter_side": return ComposterSide(i);
                case "composter_top": return ComposterTop(i);
                case "composter_bottom": return ComposterBottom(i);
                case "composter_compost": return Compost(i, false);
                case "composter_ready": return Compost(i, true);
                // -------------------------------------------------- crafter
                case "crafter_top": return CrafterTop(i);
                case "crafter_side": return CrafterSide(i);
                case "crafter_front": return CrafterFront(i);
            }
            return null;
        }

        // ==================================================================== furnace family

        static Img FurnaceSide(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float f = 1f;
                    if ((y & 7) == 0) f = 1.08f; else if ((y & 7) == 7) f = 0.9f;
                    if ((x & 7) == 0) f *= 1.04f;
                    i[x, y] = Cl(FStone, f, 0.07f);
                }
            i.RectOutline(0, 0, 16, 16, FStoneD);
            i.HLine(1, 1, 14, Tone(FStone, 1.16f));
            i.HLine(7, 1, 14, FStoneD);
            for (int k = 0; k < 6; k++) Blob(i, RndInt(16), RndInt(16), 1, Tone(FStone, 0.9f), 0.7f);
            return i;
        }

        static Img FurnaceTop(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(FStone, 1.02f, 0.07f);
            i.RectOutline(0, 0, 16, 16, FStoneD);
            i.HLine(1, 0, 15, Tone(FStone, 1.16f));
            // firebox collar
            i.RectOutline(4, 4, 8, 8, FStoneD);
            i.RectOutline(5, 5, 6, 6, Tone(FStone, 1.1f));
            i.Rect(6, 6, 4, 4, C(0x2E2E34));
            i.Set(7, 7, C(0x14141A)); i.Set(8, 8, C(0x14141A));
            return i;
        }

        static Img FurnaceFront(Img i, bool lit)
        {
            FurnaceSide(i);
            i.HLine(0, 0, 15, FStoneD); i.HLine(1, 0, 15, Tone(FStone, 1.16f));
            // arched firebox
            i.Rect(2, 6, 12, 3, FStoneD);
            i.Rect(3, 5, 10, 1, Tone(FStone, 1.1f));
            i.Rect(2, 9, 12, 5, FStoneD);
            for (int y = 7; y < 15; y++)
                for (int x = 3; x < 13; x++)
                {
                    float dh = (y - 6) / 8f;
                    i[x, y] = lit ? FurnaceGlow(i, x, y, dh) : FFlue;
                }
            if (lit)
            {
                for (int x = 4; x < 12; x += 2) i.VLine(x, 9, 13, C(0x1E1408));
                i.HLine(6, 3, 12, C(0xFFB44A));
            }
            else
            {
                for (int x = 4; x < 12; x += 2) i.VLine(x, 8, 13, C(0x14141A));
                i.HLine(8, 3, 12, C(0x33333A));
            }
            i.HLine(14, 0, 15, FStoneD);
            i.HLine(15, 0, 15, C(0x3A3A40));
            // ash lip
            i.HLine(14, 2, 13, C(0x3A3A40));
            return i;
        }

        /// <summary>Fire inside a furnace mouth: hot at the bottom, cooling upward, with flame tongues.</summary>
        static Color32 FurnaceGlow(Img i, int x, int y, float t)
        {
            float seed = (Hash.Get(x, y, 91) & 255) / 255f;
            float tongue = Mathf.Sin(x * 1.7f + 0.6f) * 0.18f;      // flame tongues licking upward
            float v = t * 1.1f + tongue + seed * 0.3f - 0.08f;
            if (v < 0.12f) return C(0x241610);
            if (v < 0.34f) return Img.Mix(C(0x3A1E0C), C(0x8A3208), v / 0.34f);
            if (v < 0.62f) return Img.Mix(C(0x8A3208), C(0xE07A18), (v - 0.34f) / 0.28f);
            if (v < 0.85f) return Img.Mix(C(0xE07A18), C(0xFFC64A), (v - 0.62f) / 0.23f);
            return Img.Mix(C(0xFFC64A), C(0xFFF2C0), Mathf.Min(1f, (v - 0.85f) / 0.15f));
        }

        static Img SmokerSide(Img i)
        {
            var oak = Wood("oak");
            WoodBody(i, oak, 0.88f);
            i.RectOutline(0, 0, 16, 16, Tone(oak.plankDark, 0.85f));
            i.HLine(1, 0, 15, Tone(oak.plank, 1.1f));
            // iron banding across the barrel
            for (int y = 4; y < 6; y++) for (int x = 0; x < 16; x++) i[x, y] = Img.Shade(FStone, y == 4 ? 1.15f : 0.9f);
            for (int y = 12; y < 14; y++) for (int x = 0; x < 16; x++) i[x, y] = Img.Shade(FStone, y == 12 ? 1.15f : 0.9f);
            return i;
        }

        static Img SmokerTop(Img i)
        {
            var oak = Wood("oak");
            WoodBody(i, oak, 0.95f);
            i.RectOutline(0, 0, 16, 16, Tone(oak.plankDark, 0.85f));
            i.RectOutline(5, 5, 6, 6, FStoneD);
            i.Rect(6, 6, 4, 4, C(0x2A2A30));
            i.Set(7, 7, C(0x14141A)); i.Set(8, 8, C(0x14141A));
            i.Set(6, 6, C(0x9A9A9E));
            return i;
        }

        static Img SmokerFront(Img i, bool lit)
        {
            var oak = Wood("oak");
            WoodBody(i, oak, 0.88f);
            i.RectOutline(0, 0, 16, 16, Tone(oak.plankDark, 0.85f));
            i.HLine(1, 0, 15, Tone(oak.plank, 1.1f));
            i.HLine(3, 0, 15, Tone(oak.plankDark, 1.05f));
            // iron mouth
            for (int y = 6; y < 15; y++)
                for (int x = 2; x < 14; x++)
                    i[x, y] = lit ? FurnaceGlow(i, x, y, (y - 5) / 9f) : (y < 8 ? C(0x2E2E34) : FFlue);
            i.RectOutline(2, 6, 12, 9, FStoneD);
            i.HLine(6, 2, 13, Tone(FStone, 1.2f));
            if (lit) i.HLine(5, 3, 12, C(0xFFB44A));
            // hanging hooks
            i.Set(4, 8, C(0x9A9A9E)); i.Set(11, 8, C(0x9A9A9E));
            return i;
        }

        static Img BlastSide(Img i)
        {
            var brick = C(0x4E4E54);
            for (int y = 0; y < 16; y++)
            {
                int row = y / 4;
                int off = (row & 1) == 0 ? 0 : 4;
                for (int x = 0; x < 16; x++)
                {
                    bool mortar = (y & 3) == 3 || ((x + off) & 7) == 7;
                    i[x, y] = Cl(mortar ? Tone(brick, 0.72f) : brick, 1f + ((Hash.Get((x + off) >> 3, row, 5) & 15) / 15f - 0.5f) * 0.1f, 0.05f);
                }
            }
            i.HLine(0, 0, 15, Tone(brick, 1.2f));
            i.RectOutline(0, 0, 16, 16, C(0x2A2A2E));
            return i;
        }

        static Img BlastTop(Img i)
        {
            BlastSide(i);
            i.RectOutline(4, 4, 8, 8, C(0x2A2A2E));
            i.Rect(6, 6, 4, 4, C(0x6E6E74));
            i.Set(7, 7, C(0xA0A0A6)); i.Set(8, 8, C(0x4A4A50));
            return i;
        }

        static Img BlastFront(Img i, bool lit)
        {
            BlastSide(i);
            i.RectOutline(2, 5, 12, 10, C(0x24242A));
            for (int y = 6; y < 15; y++)
                for (int x = 3; x < 13; x++)
                    i[x, y] = lit ? FurnaceGlow(i, x, y, (y - 5) / 10f) : (y < 8 ? C(0x26262C) : FFlue);
            if (lit) { i.HLine(5, 3, 12, C(0xFFB44A)); for (int x = 4; x < 12; x += 3) i.VLine(x, 10, 14, C(0x1E1206)); }
            else for (int x = 4; x < 12; x += 2) i.VLine(x, 9, 14, C(0x14141A));
            // iron lintel
            for (int y = 4; y < 6; y++) for (int x = 1; x < 15; x++) i[x, y] = Img.Shade(FStone, y == 4 ? 1.1f : 0.9f);
            return i;
        }

        // ==================================================================== dispenser / dropper

        /// <summary>Dispenser / dropper face: a stone plate with nine barrel mouths (or, for a dropper, one nozzle).</summary>
        static Img DispenserFace(Img i, bool vertical, bool dropper)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(FStone, 1f, 0.06f);
            i.RectOutline(0, 0, 16, 16, FStoneD);
            i.HLine(1, 0, 15, Tone(FStone, 1.16f));
            i.HLine(15, 0, 15, C(0x3A3A40));
            i.RectOutline(1, 1, 14, 14, Tone(FStone, 0.86f));
            if (dropper)
            {
                // one raised nozzle
                i.RectOutline(4, 4, 8, 8, Tone(FStone, 1.2f));
                i.Rect(5, 5, 6, 6, C(0x2A2A30));
                i.Rect(6, 6, 4, 4, C(0x14141A));
                i.HLine(5, 5, 10, C(0x14141A));
                i.Set(6, 7, C(0x6E6E74));
                return i;
            }
            // three by three barrel mouths
            for (int gy = 0; gy < 3; gy++)
                for (int gx = 0; gx < 3; gx++)
                {
                    int x = 2 + gx * 4, y = 2 + gy * 4;
                    if (vertical) { x = 2 + gy * 4; y = 2 + gx * 4; }
                    i.RectOutline(x, y, 4, 4, Tone(FStone, 1.22f));
                    i.Rect(x + 1, y + 1, 2, 2, C(0x16161C));
                    i.Set(x + 1, y + 1, C(0x0C0C10));
                }
            return i;
        }

        // ==================================================================== hopper

        static Img HopperOutside(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                    i[x, y] = Cl(C(0x4A4A52), y < 4 ? 1.1f : 1f, 0.06f);
            i.RectOutline(0, 0, 16, 16, C(0x2A2A30));
            i.HLine(1, 0, 15, C(0x70707A));
            i.HLine(3, 0, 15, C(0x36363C));
            for (int k = 0; k < 4; k++) Blob(i, RndInt(16), 6 + RndInt(10), 1, C(0x3A3A42), 0.7f);
            i.HLine(15, 0, 15, C(0x26262C));
            return i;
        }

        static Img HopperInside(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x2A2A30), 1f, 0.05f);
            i.RectOutline(0, 0, 16, 16, C(0x16161A));
            i.RectOutline(1, 1, 14, 14, C(0x3A3A42));
            i.Rect(4, 4, 8, 8, C(0x101014));
            i.RectOutline(4, 4, 8, 8, C(0x22222A));
            Blob(i, 6, 6, 2, C(0x33333C), 0.8f);
            return i;
        }

        static Img HopperTop(Img i)
        {
            // only the 2 px rim ring is visible on the model; the middle is the dark mouth
            for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x1C1C22), 1f, 0.05f);
            i.RectOutline(0, 0, 16, 16, C(0x2A2A30));
            i.RectOutline(1, 1, 14, 14, C(0x5E5E68));
            i.HLine(1, 1, 14, C(0x7A7A84));
            i.RectOutline(2, 2, 12, 12, C(0x2A2A30));
            return i;
        }

        // ==================================================================== spawners

        /// <summary>Cage of bars with empty space between them (cutout), plus a faint glow inside.</summary>
        static Img Spawner(Img i, bool trial)
        {
            i.Clear();
            Color32 bar = trial ? C(0x6E5A3A) : C(0x22303A);
            Color32 barL = trial ? C(0x9A8050) : C(0x36495A);
            Color32 barD = trial ? C(0x4A3C26) : C(0x161E26);
            for (int x = 1; x < 16; x += 3)
            {
                i.VLine(x, 0, 15, bar);
                i.Set(x, 0, barL); i.Set(x, 15, barD);
            }
            for (int y = 1; y < 16; y += 3)
            {
                i.HLine(y, 0, 15, y % 2 == 0 ? bar : barD);
                i.Set(0, y, barL); i.Set(15, y, barD);
            }
            i.Set(1, 1, barL); i.Set(2, 1, barL);
            // inner glow seen through the cage
            Color32 core = trial ? C(0x8A6A20) : C(0x2A4A62);
            Color32 coreHi = trial ? C(0xD8B44A) : C(0x3E6E90);
            Blob(i, 7, 7, 3, Img.WithA(core, 210), 0.85f);
            Blob(i, 7, 7, 2, Img.WithA(coreHi, 230), 0.7f);
            if (trial) { i.Set(7, 7, C(0xF0D070)); i.Set(8, 8, C(0xF0D070)); }
            return i;
        }

        // ==================================================================== froglights

        static Img Froglight(Img i, Color32 baseC, Color32 lite, Color32 dark, bool top)
        {
            if (top)
            {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++)
                    {
                        float d = Mathf.Sqrt((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f));
                        int ring = (int)(d * 0.9f);
                        float f = (ring & 1) == 0 ? 1.06f : 0.94f;
                        if (d < 2.2f) f = 1.2f;
                        i[x, y] = TN(f > 1f ? lite : baseC, f, 0.05f);
                    }
                i.RectOutline(0, 0, 16, 16, dark);
                return i;
            }
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    // vertical organic ridges and pits
                    float w = Mathf.Sin(x * 0.75f) + Mathf.Cos(y * 0.55f + x * 0.2f);
                    Color32 c = w > 0.7f ? lite : (w < -0.8f ? dark : baseC);
                    i[x, y] = Cl(c, 1f, 0.06f);
                }
            for (int k = 0; k < 5; k++) Blob(i, RndInt(16), RndInt(16), 1 + RndInt(2), Img.Shade(lite, 0.96f), 0.6f);
            i.RectOutline(0, 0, 16, 16, dark);
            return i;
        }

        // ==================================================================== slime & honey

        static Img SlimeBlock(Img i)
        {
            i.Clear();
            Color32 gel = C(0x74C162, 150), gelL = C(0x9CE08A, 190), gelD = C(0x4E9040, 170);
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float d = Mathf.Sqrt((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f));
                    float f = 1f - d * 0.012f;
                    i[x, y] = Img.WithA(Img.Shade(gel, f + (Rnd01() - 0.5f) * 0.06f), 150);
                }
            i.RectOutline(0, 0, 16, 16, Img.WithA(C(0x3E7A32), 210));
            i.RectOutline(2, 2, 12, 12, Img.WithA(C(0x5EA84E), 190));
            Blob(i, 5, 5, 3, Img.WithA(gelL, 200), 0.6f);
            i.Set(4, 4, C(0xC8F4B8, 220)); i.Set(5, 4, C(0xC8F4B8, 220)); i.Set(4, 5, C(0xC8F4B8, 220));
            i.Set(11, 11, gelD); i.Set(12, 11, gelD);
            return i;
        }

        static Img HoneySide(Img i)
        {
            Color32 amber = C(0xE8A028);
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    // long vertical drips
                    float w = Mathf.Sin(x * 0.9f + y * 0.25f);
                    float f = w > 0.5f ? 1.1f : (w < -0.6f ? 0.86f : 1f);
                    var c = TN(amber, f, 0.05f);
                    c.a = 235;
                    i[x, y] = c;
                }
            for (int x = 2; x < 16; x += 5)
            {
                i.Set(x, 0, C(0xF8C860, 245)); i.Set(x, 1, C(0xF0B840, 245));
                i.Set(x, 15, C(0xB0740E, 245));
            }
            i.HLine(0, 0, 15, C(0xFFD67A, 245));
            i.HLine(15, 0, 15, C(0xA86A0A, 245));
            return i;
        }

        static Img HoneyTop(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float d = Mathf.Sqrt((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f));
                    var c = TN(C(0xE8A028), 1.14f - d * 0.03f, 0.05f);
                    c.a = 235;
                    i[x, y] = c;
                }
            // hexagon-ish cells
            for (int gy = 0; gy < 3; gy++)
                for (int gx = 0; gx < 3; gx++)
                {
                    int cx = 3 + gx * 5, cy = 3 + gy * 5;
                    for (int k = 0; k < 6; k++)
                    {
                        float a = k * Mathf.PI / 3f;
                        i.Set(cx + Mathf.RoundToInt(Mathf.Cos(a) * 2f), cy + Mathf.RoundToInt(Mathf.Sin(a) * 2f), C(0xC87E14, 240));
                    }
                }
            i.RectOutline(0, 0, 16, 16, C(0xB0740E, 245));
            return i;
        }

        static Img HoneyBottom(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) { var c = Cl(C(0xC87E1A), 0.95f, 0.07f); c.a = 240; i[x, y] = c; }
            i.RectOutline(0, 0, 16, 16, C(0x8A5A10, 245));
            Blob(i, 5, 5, 2, C(0xE8A83A, 240), 0.8f);
            Blob(i, 11, 11, 2, C(0xE8A83A, 240), 0.8f);
            return i;
        }

        // ==================================================================== iron door & trapdoor

        static Img IronDoor(Img i, bool top)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0xC4C4C8), 1f, 0.04f);
            i.RectOutline(0, 0, 16, 16, C(0x7A7A80));
            i.RectOutline(1, 1, 14, 14, C(0xE6E6EA));
            i.VLine(2, 2, 13, C(0x9A9AA0));
            i.RectOutline(3, 3, 10, 10, C(0x8A8A90));
            if (top)
            {
                // riveted upper panel with two slots
                i.Rect(4, 4, 8, 3, C(0x9AA0A8));
                i.RectOutline(4, 4, 8, 3, C(0x6A6A70));
                i.HLine(5, 5, 10, C(0x4A4A50));
                i.Set(3, 3, C(0xE8E8EC)); i.Set(12, 3, C(0xE8E8EC));
                i.Set(3, 12, C(0xE8E8EC)); i.Set(12, 12, C(0xE8E8EC));
            }
            else
            {
                // handle and lower panel
                i.Rect(3, 3, 10, 8, C(0xB0B0B6));
                i.RectOutline(3, 3, 10, 8, C(0x8A8A90));
                i.Rect(11, 3, 3, 3, C(0x6A6A70));
                i.Rect(12, 4, 2, 2, C(0xD8D8DE));
                i.Set(3, 3, C(0xE8E8EC)); i.Set(12, 11, C(0x8A8A90));
            }
            i.HLine(15, 0, 15, C(0x6A6A70));
            return i;
        }

        static Img IronTrapdoor(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0xBEBEC4), 1f, 0.04f);
            i.RectOutline(0, 0, 16, 16, C(0x78787E));
            i.RectOutline(1, 1, 14, 14, C(0xE4E4E8));
            // drilled hole grid, with the four corner pixels left solid
            for (int gy = 0; gy < 3; gy++)
                for (int gx = 0; gx < 3; gx++)
                {
                    int x = 2 + gx * 5, y = 2 + gy * 5;
                    i.RectOutline(x, y, 4, 4, C(0x9A9AA0));
                    i.Rect(x + 1, y + 1, 2, 2, C(0x3E3E44));
                    i.Set(x + 1, y + 1, C(0x2A2A2E));
                }
            i.HLine(15, 0, 15, C(0x5E5E64));
            return i;
        }

        // ==================================================================== rails

        static readonly Color32 RailIron = C(0xB4B4BA);
        static readonly Color32 RailGold = C(0xE8BE48);
        enum RailKindMark { None, Redstone, Plate }

        /// <summary>
        /// Rails are drawn as a floor decal with u = east and v = north (generator row 0 is the north edge), so
        /// the sleepers run east-west and the two rails run north-south at cols 4-5 and 10-11. RailBlock
        /// rotates the decal for east-west track.
        /// </summary>
        static Img Rail(Img i, Color32 metal, RailKindMark mark, bool lit, bool activator = false)
        {
            i.Clear();
            Color32 wood = C(0x8E6C40), woodD = C(0x5E4428), woodL = C(0xAA8650);
            for (int s = 0; s < 4; s++)
            {
                int y = 1 + s * 4;
                for (int x = 2; x <= 13; x++) { i[x, y] = Cl(wood, x == 2 ? 1.1f : 1f, 0.06f); i[x, y + 1] = Cl(woodD, 1f, 0.06f); }
                i.Set(2, y, woodL); i.Set(13, y + 1, Img.Shade(woodD, 0.85f));
            }
            if (mark == RailKindMark.Redstone)
            {
                Color32 wire = lit ? C(0xE8301E) : C(0x5E1A14), wireHi = lit ? C(0xFF8A6A) : C(0x7A2A20);
                for (int y = 0; y < 16; y++)
                {
                    if (activator && (y & 3) == 3) continue;       // activator: dashed line
                    i[7, y] = (y % 5 == 0) ? wireHi : wire;
                    i[8, y] = Img.Shade(wire, 0.8f);
                }
            }
            else if (mark == RailKindMark.Plate)
            {
                Color32 plate = lit ? C(0xC8402E) : C(0x8E8E8E);
                for (int y = 6; y <= 9; y++) for (int x = 6; x <= 9; x++) i[x, y] = Img.Shade(plate, (y == 6 || x == 6) ? 1.18f : ((y == 9 || x == 9) ? 0.78f : 1f));
            }
            Color32 hi = Img.Shade(metal, 1.16f), lo = Img.Shade(metal, 0.64f);
            foreach (int x in new[] { 4, 10 })
                for (int y = 0; y < 16; y++) { i[x, y] = hi; i[x + 1, y] = lo; }
            // rail spikes where the rails cross the sleepers
            for (int s = 0; s < 4; s++)
            {
                int y = 1 + s * 4;
                foreach (int x in new[] { 3, 6, 9, 12 }) i.Set(x, y, Img.Shade(metal, 0.78f));
            }
            return i;
        }

        /// <summary>
        /// Curved track for the south-east shape (RailBlock rotates it for the others): the rails leave the
        /// south edge at cols 4-5 / 10-11 and the east edge at rows 4-5 / 10-11, bending around that corner.
        /// </summary>
        static Img RailCorner(Img i)
        {
            i.Clear();
            Color32 wood = C(0x8E6C40), woodD = C(0x5E4428), woodL = C(0xAA8650);
            Color32 hi = Img.Shade(RailIron, 1.16f), lo = Img.Shade(RailIron, 0.64f);
            // sleepers: four radial spokes sweeping through the corner
            for (int k = 0; k < 4; k++)
            {
                float a = (k + 0.5f) / 4f * (Mathf.PI * 0.5f);
                for (int r = 3; r <= 15; r++)
                {
                    int x = 15 - Mathf.RoundToInt(Mathf.Cos(a) * r);
                    int y = 15 - Mathf.RoundToInt(Mathf.Sin(a) * r);
                    if (x < 0 || y < 0) break;
                    i.Set(x, y, r > 13 ? woodD : wood);
                    i.Set(x, y - 1, r > 13 ? wood : woodL);
                }
            }
            // two rails bending around the corner centre at (16,16)
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float dx = 15.5f - x, dy = 15.5f - y;
                    float r = Mathf.Sqrt(dx * dx + dy * dy);
                    if (r >= 11.4f && r < 13.6f) i[x, y] = r >= 12.5f ? hi : lo;
                    else if (r >= 5.4f && r < 7.6f) i[x, y] = r >= 6.5f ? hi : lo;
                }
            return i;
        }

        // ==================================================================== end blocks

        static Img EndFrameSide(Img i)
        {
            Base(i, "end_stone");
            // cyan inlay panel in the middle of the side
            i.Rect(3, 2, 10, 12, C(0x2C6E66));
            i.Rect(4, 3, 8, 10, C(0x3E9488));
            i.Rect(5, 4, 6, 6, C(0x17615A));
            i.Rect(6, 5, 4, 4, C(0x7FD8C8));
            i.Rect(7, 6, 2, 2, C(0xC8FFF4));
            i.RectOutline(3, 2, 10, 12, C(0x1A4A44));
            i.RectOutline(0, 0, 16, 16, C(0xB8B884));
            return i;
        }

        static Img EndFrameTop(Img i, bool eye)
        {
            Base(i, "end_stone");
            i.RectOutline(0, 0, 16, 16, C(0xB8B884));
            // square socket
            for (int y = 3; y < 13; y++)
                for (int x = 3; x < 13; x++)
                {
                    int d = Math.Max(Math.Abs(x - 7), Math.Abs(y - 7));
                    Color32 c;
                    if (d >= 4) c = C(0x2C6E66);
                    else if (d == 3) c = C(0x3E9488);
                    else c = C(0x14342F);
                    i[x, y] = Cl(c, 1f, 0.05f);
                }
            i.RectOutline(3, 3, 10, 10, C(0x1A4A44));
            if (!eye) return i;
            // ender eye: a green orb set into the socket, bright rim light on the upper left
            for (int y = 3; y < 13; y++)
                for (int x = 3; x < 13; x++)
                {
                    float dx = x - 7.5f, dy = y - 7.5f;
                    float d = Mathf.Sqrt(dx * dx + dy * dy);
                    if (d > 4.6f) continue;
                    if (d > 3.4f) i[x, y] = C(0x15463A);
                    else if (d > 2.2f) i[x, y] = C(0x24A05A);
                    else i[x, y] = d > 1.1f ? C(0x3BD276) : C(0x8CF0AC);
                }
            i.Set(6, 5, C(0xD8FFE4)); i.Set(5, 5, C(0xA8F4C0)); i.Set(5, 6, C(0xA8F4C0));
            i.RectOutline(3, 3, 10, 10, C(0x123830));
            return i;
        }

        /// <summary>
        /// Atlas layout expected by EndRodBlock (shared by the lightning rod): shaft sides at cols 0-1 rows 1-15,
        /// shaft tip at cols 2-3 rows 14-15, base plate top at cols 2-5 rows 4-7 (its 1 px sides read row 4).
        /// </summary>
        static Img RodAtlas(Img i, Color32 shaft, Color32 shaftShade, Color32 tip, Color32 plate)
        {
            i.Clear();
            for (int y = 0; y < 16; y++) { i.Set(0, y, Cl(shaft, 1f, 0.04f)); i.Set(1, y, Cl(shaftShade, 1f, 0.04f)); }
            i.Set(0, 1, Tone(shaft, 1.2f)); i.Set(1, 1, shaft);
            for (int y = 4; y <= 7; y++)
                for (int x = 2; x <= 5; x++)
                {
                    float f = (y == 4 || x == 2) ? 1.14f : ((y == 7 || x == 5) ? 0.8f : 1f);
                    i[x, y] = Cl(plate, f, 0.04f);
                }
            for (int y = 14; y <= 15; y++) for (int x = 2; x <= 3; x++) i[x, y] = (x == 2 && y == 14) ? Tone(tip, 1.15f) : tip;
            return i;
        }

        static Img EndRod(Img i) => RodAtlas(i, C(0xF4EEFC), C(0xC8BCDC), C(0xFFFFFF), C(0xCFC2E2));

        static Img LightningRod(Img i) => RodAtlas(i, C(0xD8804E), C(0xA05428), C(0xF0A474), C(0xC06A3C));

        // ==================================================================== respawn anchor

        static Img AnchorSide(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x2C2434), 1f, 0.07f);
            i.RectOutline(0, 0, 16, 16, C(0x140E1C));
            i.RectOutline(1, 1, 14, 14, C(0x40324E));
            // carved triangular glyph
            for (int k = 0; k < 8; k++)
            {
                i.Set(7 - k / 2, 4 + k, C(0x8A6EC8));
                i.Set(8 + k / 2, 4 + k, C(0x6E52A8));
            }
            i.Set(7, 4, C(0xC8A8FF)); i.Set(8, 4, C(0xA888E8));
            i.HLine(11, 4, 11, C(0x8A6EC8));
            i.Set(3, 3, C(0x55468C)); i.Set(12, 3, C(0x55468C));
            i.Set(3, 12, C(0x55468C)); i.Set(12, 12, C(0x55468C));
            return i;
        }

        static Img AnchorBottom(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x241C2C), 1f, 0.05f);
            i.RectOutline(0, 0, 16, 16, C(0x100A18));
            i.RectOutline(3, 3, 10, 10, C(0x3A2E46));
            i.Set(7, 7, C(0x55468C)); i.Set(8, 8, C(0x55468C));
            return i;
        }

        static Img AnchorTop(Img i, int level)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x2C2434), 1f, 0.06f);
            i.RectOutline(0, 0, 16, 16, C(0x140E1C));
            i.RectOutline(1, 1, 14, 14, C(0x40324E));
            i.RectOutline(3, 3, 10, 10, C(0x1C1428));
            if (level == 0)
            {
                i.Rect(5, 5, 6, 6, C(0x16101E));
                i.RectOutline(5, 5, 6, 6, C(0x40324E));
                return i;
            }
            float t = (level - 1) / 4f;
            Color32 deep = Img.Mix(C(0x3A1460), C(0xE060FF), t);
            Color32 glow = Img.Mix(C(0x8A2ACC), C(0xFFD8FF), t);
            for (int y = 4; y < 12; y++)
                for (int x = 4; x < 12; x++)
                {
                    float d = Mathf.Sqrt((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f));
                    if (d > 4f) continue;
                    i[x, y] = Cl(d < 2f ? glow : deep, 1.1f - d * 0.06f, 0.06f);
                }
            i.RectOutline(4, 4, 8, 8, Img.Shade(deep, 0.7f));
            i.Set(7, 7, C(0xFFFFFF)); i.Set(8, 8, C(0xFFE8FF));
            i.Set(6, 6, glow); i.Set(9, 9, glow);
            return i;
        }

        // ==================================================================== barrel

        static Img BarrelSide(Img i)
        {
            var p = Wood("spruce");
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    int stave = x / 4;
                    float f = 0.94f + (Hash.Get(stave, 41) & 15) / 15f * 0.12f;
                    if (x % 4 == 0) f *= 1.1f; else if (x % 4 == 3) f *= 0.86f;
                    i[x, y] = Cl(p.plank, f, 0.07f);
                }
            // two iron hoops
            foreach (int y in new[] { 1, 11 })
            {
                for (int x = 0; x < 16; x++) { i[x, y] = Cl(C(0x55555C), 1.18f, 0.05f); i[x, y + 1] = Cl(C(0x46464C), 0.94f, 0.05f); }
                for (int x = 2; x < 16; x += 5) i.Set(x, y + 1, C(0x8E8E96));
            }
            i.HLine(0, 0, 15, Tone(p.plankDark, 0.95f));
            i.HLine(15, 0, 15, Tone(p.plankDark, 0.9f));
            return i;
        }

        static Img BarrelEnd(Img i, bool open)
        {
            var p = Wood("spruce");
            i.Planks(p.plank, p.plankDark);
            i.RectOutline(0, 0, 16, 16, Tone(p.plankDark, 0.8f));
            i.RectOutline(1, 1, 14, 14, Tone(p.plank, 0.9f));
            i.RectOutline(2, 2, 12, 12, Tone(p.plankDark, 0.9f));
            if (open)
            {
                for (int y = 3; y < 13; y++)
                    for (int x = 3; x < 13; x++)
                        i[x, y] = Cl(C(0x241809), 1f, 0.14f);
                i.HLine(3, 3, 12, C(0x3C2A16));
                i.VLine(3, 3, 12, C(0x33240F));
                Blob(i, 8, 8, 3, C(0x160E06), 0.7f);
            }
            else
            {
                for (int y = 3; y < 13; y++)
                    for (int x = 3; x < 13; x++)
                        i[x, y] = Cl(p.plank, (y % 5 == 2 ? 0.84f : 1.04f), 0.07f);
                i.HLine(7, 3, 12, p.plankDark);
                for (int x = 3; x < 13; x += 3) i.VLine(x, 3, 12, Tone(p.plankDark, 1.08f));
                // iron band across the lid
                for (int y = 7; y < 9; y++) for (int x = 2; x < 14; x++) i[x, y] = y == 7 ? C(0x6E6E76) : C(0x44444A);
                i.Set(7, 7, C(0x9A9AA2)); i.Set(8, 8, C(0x9A9AA2));
            }
            return i;
        }

        // ==================================================================== lily pad

        /// <summary>
        /// Grayscale pad so the block's lily tint colours it. A round pad with a wedge notch and radial veins.
        /// </summary>
        static Img LilyPad(Img i)
        {
            i.Clear();
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float dx = x - 7.5f, dy = y - 7.5f;
                    float d = Mathf.Sqrt(dx * dx + dy * dy);
                    if (d > 7.4f) continue;
                    float a = Mathf.Atan2(dy, dx);
                    if (a > 0.30f && a < 0.95f && d > 1.1f) continue;      // notch
                    int g = 176 + (int)(Hash.Get(x >> 1, (y >> 1) + 7) % 34);
                    if (d > 6.3f) g -= 26;
                    i[x, y] = Img.Gray(g);
                }
            for (int k = 0; k < 8; k++)
            {
                float a = 1.25f + k * 0.72f;
                for (int s = 1; s < 7; s++)
                {
                    int x = 8 + Mathf.RoundToInt(Mathf.Cos(a) * s - 0.5f), y = 8 + Mathf.RoundToInt(Mathf.Sin(a) * s - 0.5f);
                    if (i.Get(x, y).a > 0) i.Set(x, y, Img.Gray(132));
                }
            }
            i.Set(7, 7, Img.Gray(120)); i.Set(8, 8, Img.Gray(146));
            return i;
        }

        // ==================================================================== lodestone

        static Img LodestoneSide(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float f = (y % 4 == 3) ? 0.86f : 1f;
                    i[x, y] = Cl(C(0x7E7E86), f, 0.07f);
                }
            i.RectOutline(0, 0, 16, 16, C(0x52525A));
            i.HLine(1, 0, 15, C(0x9C9CA4));
            for (int k = 0; k < 5; k++) Blob(i, RndInt(16), RndInt(16), 1, C(0x6A6A72), 0.7f);
            return i;
        }

        static Img LodestoneTop(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x8A8A92), 1f, 0.06f);
            i.RectOutline(0, 0, 16, 16, C(0x52525A));
            i.RectOutline(2, 2, 12, 12, C(0x656570));
            i.Rect(4, 4, 8, 8, C(0x2E2E36));
            i.RectOutline(4, 4, 8, 8, C(0x1C1C22));
            // compass rose: red north needle over a stone ring
            for (int k = 0; k < 8; k++)
            {
                float a = k * Mathf.PI / 4f;
                i.Set(7 + Mathf.RoundToInt(Mathf.Cos(a) * 3f), 7 + Mathf.RoundToInt(Mathf.Sin(a) * 3f), C(0x9A9AA2));
            }
            i.VLine(7, 5, 7, C(0xC8362A)); i.VLine(8, 5, 7, C(0xC8362A));
            i.Set(7, 5, C(0xE8584A)); i.Set(8, 5, C(0xE8584A));
            i.VLine(7, 9, 10, C(0xD8D8E0)); i.VLine(8, 9, 10, C(0xD8D8E0));
            i.Set(7, 7, C(0xE8E8F0)); i.Set(8, 7, C(0xE8E8F0));
            return i;
        }

        // ==================================================================== flower pot / dragon egg / beacon / bell

        /// <summary>FlowerPotBlock builds its sides from quads that sample the whole tile, so this is bare pot wall.</summary>
        static Img FlowerPot(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x9C5E42), 1f, 0.06f);
            i.RectOutline(0, 0, 16, 16, C(0x6C3C26));
            i.HLine(0, 0, 15, C(0xC07C58));
            i.HLine(1, 0, 15, C(0xB06848));
            i.HLine(3, 0, 15, C(0x7E482E));
            i.HLine(15, 0, 15, C(0x5C3220));
            for (int k = 0; k < 6; k++) Blob(i, RndInt(16), 4 + RndInt(12), 1, C(0xAB6E50), 0.7f);
            return i;
        }

        static Img DragonEgg(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x131019), 1f, 0.16f);
            // purple scale speckles, denser toward the top where the model's dome is
            for (int k = 0; k < 44; k++)
            {
                int x = RndInt(16), y = RndInt(16);
                float t = 1f - y / 17f;
                Color32 c = Img.Mix(C(0x2E2246), C(0xC08AFF), Rnd01() * 0.5f + t * 0.5f);
                i.Set(x, y, c);
                if (Rnd01() < 0.5f) i.Set(x + 1, y, Img.Shade(c, 0.8f));
                if (Rnd01() < 0.35f) i.Set(x, y + 1, Img.Shade(c, 0.66f));
            }
            i.Set(6, 2, C(0xC8A0FF)); i.Set(7, 3, C(0xB080F0)); i.Set(9, 1, C(0xD8B8FF));
            i.RectOutline(0, 0, 16, 16, C(0x08060E));
            return i;
        }

        static Img BeaconCore(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x7AE0DA), 1f, 0.1f);
            i.RectOutline(2, 2, 12, 12, C(0x2E8A84));
            i.RectOutline(3, 3, 10, 10, C(0xB8FBF4));
            i.Rect(5, 5, 6, 6, C(0xE8FFFC));
            i.Set(7, 7, C(0xFFFFFF)); i.Set(8, 8, C(0xFFFFFF));
            i.RectOutline(0, 0, 16, 16, C(0x2A7A76));
            return i;
        }

        /// <summary>
        /// Opaque gold, laid out for the bell model: the lip band spans rows 4-7 and the body fills the rest.
        /// </summary>
        static Img BellBody(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float f = 1f;
                    if (x < 4) f = 1.1f; else if (x > 11) f = 0.88f;
                    if (y == 0 || y == 4) f *= 1.12f; else if (y == 15) f *= 0.8f;
                    if (y >= 5 && y <= 7) f *= 0.95f;
                    i[x, y] = Cl(C(0xE4B22E), f, 0.05f);
                }
            i.HLine(4, 0, 15, C(0xF8DA7C));
            i.HLine(5, 0, 15, C(0xC8981E));
            i.VLine(5, 0, 3, C(0xF4D06A));
            i.HLine(15, 0, 15, C(0xA87810));
            return i;
        }

        /// <summary>
        /// Atlas layout expected by BrewingStandBlock: the rod's sides are cols 7-8 rows 0-13 and its cap cols
        /// 7-8 rows 0-1; the bottle-arm plane samples the right half (cols 8-15), the left half mirrors it.
        /// </summary>
        static Img BrewingStand(Img i)
        {
            i.Clear();
            Color32 rod = C(0xD8B04A), rodD = Img.Shade(rod, 0.72f);
            for (int y = 0; y < 14; y++) { i.Set(7, y, rod); i.Set(8, y, rodD); }
            i.Set(7, 0, C(0xFFE89A)); i.Set(8, 0, C(0xE0B848)); i.Set(7, 1, C(0xF0CC68));
            Color32[] potion = { C(0xC84A8A), C(0x3AC0A8) };
            for (int side = 0; side < 2; side++)
            {
                int dir = side == 0 ? 1 : -1;
                int arm0 = side == 0 ? 9 : 2, arm1 = side == 0 ? 13 : 6;
                i.HLine(2, arm0, arm1, C(0x8A8A92));
                i.Set(side == 0 ? 9 : 6, 3, C(0x6A6A72));
                int bx = side == 0 ? 11 : 1;                     // bottle occupies bx..bx+3
                i.Rect(bx + 1, 3, 2, 2, C(0xC8DCEC));             // neck
                i.HLine(3, bx + 1, bx + 2, C(0x7A5A34));          // cork
                for (int y = 5; y <= 10; y++)
                    for (int x = bx; x <= bx + 3; x++)
                    {
                        bool edge = x == bx || x == bx + 3 || y == 10;
                        i[x, y] = edge ? C(0x8EAAC4) : (y >= 7 ? potion[side] : C(0xD2E4F2));
                    }
                i.Set(bx + (dir > 0 ? 1 : 2), 7, Tone(potion[side], 1.3f));
                i.Set(bx, 5, C(0xEEF6FF));
            }
            return i;
        }

        static Img BrewingBase(Img i)
        {
            Base(i, "cobblestone");
            i.HLine(0, 0, 15, C(0xA8A8A8));
            i.HLine(15, 0, 15, C(0x4A4A4A));
            return i;
        }

        // ==================================================================== cauldron

        static Img CauldronSide(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float f = 1f;
                    if (y < 3) f = 1.08f; else if (y > 13) f = 0.9f;
                    i[x, y] = Cl(C(0x4A4A50), f, 0.06f);
                }
            i.RectOutline(0, 0, 16, 16, C(0x2A2A30));
            i.HLine(1, 0, 15, C(0x74747C));
            i.HLine(2, 1, 14, C(0x5E5E66));
            i.HLine(15, 0, 15, C(0x36363C));
            // riveted band
            for (int y = 5; y < 8; y++) for (int x = 0; x < 16; x++) i[x, y] = Img.Shade(C(0x5A5A62), y == 5 ? 1.12f : (y == 7 ? 0.86f : 1f));
            for (int x = 2; x < 16; x += 5) i.Set(x, 6, C(0x8E8E96));
            return i;
        }

        static Img CauldronTop(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x56565E), 1f, 0.06f);
            i.RectOutline(0, 0, 16, 16, C(0x2A2A30));
            i.RectOutline(1, 1, 14, 14, C(0x8A8A92));
            i.RectOutline(2, 2, 12, 12, C(0x40404A));
            i.Rect(3, 3, 10, 10, C(0x2E2E36));
            return i;
        }

        static Img CauldronBottom(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x4E4E56), 1f, 0.06f);
            i.RectOutline(0, 0, 16, 16, C(0x2A2A30));
            i.RectOutline(3, 3, 10, 10, C(0x3A3A42));
            i.Set(7, 7, C(0x2A2A30)); i.Set(8, 8, C(0x2A2A30));
            return i;
        }

        static Img CauldronInner(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    float d = Mathf.Sqrt((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f));
                    i[x, y] = Cl(C(0x2A2A30), 0.85f + d * 0.02f, 0.05f);
                }
            i.RectOutline(0, 0, 16, 16, C(0x16161A));
            i.Rect(5, 5, 6, 6, C(0x24242A));
            return i;
        }

        // ==================================================================== composter

        static Img ComposterSide(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                    i[x, y] = Cl(C(0x9A6E3E), ((y / 4) & 1) == 0 ? 1.04f : 0.94f, 0.08f);
            for (int y = 3; y < 16; y += 4) i.HLine(y, 0, 15, C(0x6A4A26));
            for (int x = 3; x < 16; x += 8) i.VLine(x, 0, 15, C(0x6A4A26));
            i.RectOutline(0, 0, 16, 16, C(0x54381C));
            i.HLine(0, 0, 15, C(0xBE8C52));
            return i;
        }

        /// <summary>
        /// The rim ring shows on the wall tops; the centre is also the top of the floor slab seen inside an
        /// empty composter (opaque layer), so it is dark floor boards rather than transparent.
        /// </summary>
        static Img ComposterTop(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    int d = Math.Max(Math.Abs(x * 2 - 15), Math.Abs(y * 2 - 15));
                    if (d >= 12) i[x, y] = Cl(C(0x9A6E3E), (d / 2 & 1) == 0 ? 1.06f : 0.94f, 0.08f);
                    else i[x, y] = Cl(C(0x4E3418), (y % 4 == 3) ? 0.8f : 1f, 0.08f);
                }
            i.RectOutline(0, 0, 16, 16, C(0x54381C));
            i.RectOutline(2, 2, 12, 12, C(0x3A2610));
            return i;
        }

        static Img ComposterBottom(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x7E5A32), 1f, 0.06f);
            i.RectOutline(0, 0, 16, 16, C(0x54381C));
            i.RectOutline(3, 3, 10, 10, C(0x6A4A26));
            return i;
        }

        /// <summary>Compost surface: dark scraps when filling, pale bone meal when ready.</summary>
        static Img Compost(Img i, bool ready)
        {
            if (ready)
            {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0xD6D0B6), 1f, 0.09f);
                for (int k = 0; k < 26; k++) i.Set(RndInt(16), RndInt(16), C(0xF4F0E2));
                for (int k = 0; k < 12; k++) Blob(i, RndInt(16), RndInt(16), 1, C(0xB8AE92), 0.8f);
            }
            else
            {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x5E3E1E), 1f, 0.14f);
                for (int k = 0; k < 22; k++)
                {
                    int x = RndInt(16), y = RndInt(16);
                    var c = new[] { C(0x3E2A12), C(0x7A5426), C(0x4E3A18), C(0x8A6636) }[RndInt(4)];
                    i.Set(x, y, c); if (Rnd01() < 0.5f) i.Set(x + 1, y, Img.Shade(c, 0.85f));
                }
            }
            i.RectOutline(0, 0, 16, 16, ready ? C(0xA89E80) : C(0x3A2410));
            return i;
        }

        // ==================================================================== crafter

        static Img CrafterTop(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x6A6A72), 1f, 0.05f);
            i.RectOutline(0, 0, 16, 16, C(0x3E3E46));
            i.RectOutline(1, 1, 14, 14, C(0x8A8A94));
            // crafting grid with two placeholder items
            for (int y = 3; y < 14; y++)
                for (int x = 3; x < 14; x++)
                {
                    int cx = (x - 3) % 4, cy = (y - 3) % 4;
                    if (cx == 3 || cy == 3) i[x, y] = C(0x40404A);
                    else if (cx == 0 || cy == 0) i[x, y] = C(0x7A7A84);
                    else i[x, y] = C(0x55555E);
                }
            i.Rect(5, 5, 2, 2, C(0xD8A03A)); i.Set(5, 5, C(0xF0C86A));
            i.Rect(10, 10, 2, 2, C(0x9A9AA4)); i.Set(10, 10, C(0xC8C8D0));
            return i;
        }

        static Img CrafterSide(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x6A6A72), 1f, 0.05f);
            i.RectOutline(0, 0, 16, 16, C(0x3E3E46));
            i.RectOutline(2, 3, 12, 3, C(0x8A8A94));
            i.Rect(3, 4, 10, 1, C(0x2E2E36));
            for (int x = 4; x < 13; x += 3) i.VLine(x, 4, 4, C(0x6E6E78));
            i.Rect(3, 9, 10, 4, C(0x55555E));
            i.RectOutline(3, 9, 10, 4, C(0x8A8A94));
            i.Set(5, 11, C(0xC04A2A)); i.Set(10, 11, C(0x3AC04A));
            return i;
        }

        static Img CrafterFront(Img i)
        {
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++) i[x, y] = Cl(C(0x6A6A72), 1f, 0.05f);
            i.RectOutline(0, 0, 16, 16, C(0x3E3E46));
            // three output slots, each with a red/green indicator
            for (int k = 0; k < 3; k++)
            {
                int y = 3 + k * 4;
                i.RectOutline(3, y, 10, 3, C(0x8A8A94));
                i.Rect(4, y + 1, 8, 1, C(0x2E2E36));
                i.Set(12, y + 1, k == 2 ? C(0x3AC04A) : C(0xC04A2A));
            }
            i.Rect(2, 13, 12, 2, C(0x4A4A52));
            i.HLine(13, 2, 13, C(0x8A8A94));
            return i;
        }
    }
}
