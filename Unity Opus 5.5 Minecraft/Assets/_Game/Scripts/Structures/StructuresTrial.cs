using System;
using System.Collections.Generic;

namespace MCR
{
    // =====================================================================================================
    //  Trial chambers (compact): a buried tuff-and-copper complex. A tall atrium with trial spawners sits in
    //  the middle; four wings (corridor + chamber) branch off it, each with its own challenge and loot.
    // =====================================================================================================
    public sealed class TrialChambersStructure : StructureBase
    {
        const int S = 48;
        public TrialChambersStructure() { id = "trial_chambers"; spacing = 34; separation = 12; salt = 50047; maxRadiusChunks = 3; }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
        {
            if (!(g is OverworldGenerator)) return false;
            return StructureTerrain.Ground(g, x, z) > -10 && rng.Chance(0.75f);
        }

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            int y = rng.Range(-38, -22);
            var st = NewStart(x, y + 1, z);
            var p = new Chambers { seed = NextSeed(ref rng) };
            for (int i = 0; i < 4; i++) p.wingKind[i] = rng.Next(4);
            p.Place(x - S / 2, y, z - S / 2, S, S, rng.Next(4), 2, 14, 0);
            st.pieces.Add(p);
            return Finish(st, cx, cz);
        }

        sealed class Chambers : FramedPiece
        {
            public readonly int[] wingKind = new int[4];
            const int A0 = 16, A1 = 32;   // atrium bounds (17 x 17)

            static readonly string[] Spawns = { "zombie", "skeleton", "spider", "husk", "stray", "cave_spider", "slime", "bogged", "silverfish" };

            protected override void Emit(SB b)
            {
                Atrium(b);
                // wings: north, east, south, west (child frames face away from the atrium)
                Wing(b.Sub(19, 0, A1 + 1, 11, 15, 0), wingKind[0], 0);
                Wing(b.Sub(A1 + 1, 0, 19, 11, 15, 1), wingKind[1], 1);
                Wing(b.Sub(19, 0, 1, 11, 15, 2), wingKind[2], 2);
                Wing(b.Sub(1, 0, 19, 11, 15, 3), wingKind[3], 3);
            }

            static void TuffShell(SB b, int x0, int y0, int z0, int x1, int y1, int z1)
            {
                b.Fill3(x0, y0, z0, x1, y0, z1, "polished_tuff", "tuff_bricks", 0.2f, null, 0f, 1);
                b.Fill3(x0, y1, z0, x1, y1, z1, "tuff_bricks", "chiseled_tuff_bricks", 0.1f, null, 0f, 2);
                b.Walls3(x0, y0 + 1, z0, x1, y1 - 1, z1, "tuff_bricks", "polished_tuff", 0.25f, "chiseled_tuff", 0.05f, 3);
                b.Clear(x0 + 1, y0 + 1, z0 + 1, x1 - 1, y1 - 1, z1 - 1);
            }

            void Atrium(SB b)
            {
                TuffShell(b, A0, 0, A0, A1, 11, A1);
                int c = (A0 + A1) / 2;
                // copper floor rings
                for (int x = A0 + 1; x < A1; x++)
                    for (int z = A0 + 1; z < A1; z++)
                    {
                        int ring = Math.Max(Math.Abs(x - c), Math.Abs(z - c));
                        string id = ring == 7 ? "cut_copper" : ring == 5 ? "oxidized_cut_copper" : ring == 3 ? "weathered_cut_copper" : "polished_tuff";
                        b.Set(x, 0, z, id);
                    }
                b.Fill(c - 1, 0, c - 1, c + 1, 0, c + 1, "chiseled_tuff");
                // central dais with the breeze spawner, and four more trial spawners on plinths
                b.Fill(c - 1, 1, c - 1, c + 1, 1, c + 1, "tuff_brick_slab");
                b.TrialSpawner(c, 2, c, "breeze");
                foreach (var (px, pz, i) in new[] { (c - 5, c - 5, 0), (c + 5, c - 5, 1), (c - 5, c + 5, 2), (c + 5, c + 5, 3) })
                {
                    b.Set(px, 1, pz, "chiseled_tuff_bricks");
                    string mob = Spawns[(int)(b.R(px, 0, pz, 7) * Spawns.Length) % Spawns.Length];
                    b.TrialSpawner(px, 2, pz, mob);
                }
                // pillars with copper bulbs and chain chandeliers
                foreach (var (px, pz) in new[] { (A0 + 2, A0 + 2), (A1 - 2, A0 + 2), (A0 + 2, A1 - 2), (A1 - 2, A1 - 2) })
                {
                    b.Fill(px, 1, pz, px, 10, pz, "tuff_bricks");
                    b.Set(px, 4, pz, "copper_bulb", 1);
                    b.Set(px, 8, pz, "copper_bulb", 1);
                }
                for (int i = -4; i <= 4; i += 8)
                    for (int j = -4; j <= 4; j += 8)
                    {
                        b.Fill(c + i, 8, c + j, c + i, 10, c + j, "copper_chain", 0);
                        b.Lantern(c + i, 7, c + j, true, "copper_lantern");
                    }
                // skylight grates in the ceiling and a balcony ring of copper grates
                b.Fill(c - 2, 11, c - 2, c + 2, 11, c + 2, "copper_grate");
                for (int x = A0 + 1; x < A1; x++)
                {
                    b.Set(x, 6, A0 + 1, "oxidized_copper_grate");
                    b.Set(x, 6, A1 - 1, "oxidized_copper_grate");
                }
                for (int z = A0 + 2; z < A1 - 1; z++)
                {
                    b.Set(A0 + 1, 6, z, "oxidized_copper_grate");
                    b.Set(A1 - 1, 6, z, "oxidized_copper_grate");
                }
                // ladders up to the balcony
                b.Ladders(c, 1, 6, A0 + 1, Dir.North);
                b.Air(c, 6, A0 + 1);
                // loot around the room
                b.Chest(A0 + 1, 1, c, "trial_chambers", Dir.East);
                b.Chest(A1 - 1, 1, c, "trial_chambers", Dir.West);
                b.Barrel(A0 + 1, 7, A0 + 2, "trial_chambers");
                b.Barrel(A1 - 1, 7, A1 - 2, "trial_chambers");
                // doorways into the four wings
                b.Clear(c - 1, 1, A1, c + 1, 3, A1);
                b.Clear(c - 1, 1, A0, c + 1, 3, A0);
                b.Clear(A0, 1, c - 1, A0, 3, c + 1);
                b.Clear(A1, 1, c - 1, A1, 3, c + 1);
            }

            // wing frame: x 0..10 across, z 0..14 away from the atrium; corridor z 0..4, chamber z 5..14
            void Wing(SB w, int kind, int idx)
            {
                // corridor
                TuffShell(w, 3, 0, 0, 7, 4, 5);
                w.Clear(4, 1, 0, 6, 3, 0);
                w.Set(3, 2, 2, "copper_grate"); w.Set(7, 2, 2, "copper_grate");
                w.Set(5, 3, 2, "copper_bulb", 1);
                w.Fill(4, 0, 1, 6, 0, 4, "cut_copper");
                // chamber
                TuffShell(w, 0, 0, 5, 10, 8, 14);
                w.Clear(4, 1, 5, 6, 3, 5);
                w.Door(5, 1, 5, "copper_door", Dir.North);
                w.Clear(4, 1, 5, 4, 3, 5); w.Clear(6, 1, 5, 6, 3, 5);
                w.Set(1, 5, 6, "copper_bulb", 1); w.Set(9, 5, 13, "copper_bulb", 1);
                w.Set(9, 5, 6, "copper_bulb", 1); w.Set(1, 5, 13, "copper_bulb", 1);
                string mob = Spawns[(kind * 3 + idx) % Spawns.Length];
                switch (kind)
                {
                    case 0: // arena: two spawners and a reward chest
                        w.Fill(2, 1, 8, 2, 1, 8, "chiseled_tuff");
                        w.TrialSpawner(2, 2, 8, mob);
                        w.Set(8, 1, 11, "chiseled_tuff");
                        w.TrialSpawner(8, 2, 11, Spawns[(kind * 3 + idx + 4) % Spawns.Length]);
                        w.Chest(5, 1, 13, "trial_chambers", Dir.South);
                        break;
                    case 1: // storage: barrels and shelves of copper
                        for (int x = 1; x <= 9; x += 2) w.Barrel(x, 1, 13, "trial_chambers");
                        w.Fill(1, 1, 7, 1, 3, 12, "cut_copper");
                        w.TrialSpawner(5, 1, 9, mob);
                        w.Chest(9, 1, 7, "trial_chambers", Dir.West);
                        break;
                    case 2: // parkour: copper platforms up to a chest on a ledge
                        w.Fill(1, 4, 12, 3, 4, 13, "cut_copper");
                        w.Set(4, 1, 10, "copper_block"); w.Set(6, 2, 11, "copper_block"); w.Set(4, 3, 12, "copper_block");
                        w.Chest(2, 5, 13, "trial_chambers", Dir.South);
                        w.TrialSpawner(8, 1, 8, mob);
                        break;
                    default: // vault stand-in: loot behind copper bars, key spawner in the middle
                        w.Fill(3, 1, 12, 7, 3, 12, "copper_bars");
                        w.Chest(5, 1, 13, "trial_chambers", Dir.South);
                        w.Barrel(3, 1, 13, "trial_chambers");
                        w.TrialSpawner(5, 1, 9, mob);
                        w.Clear(5, 1, 12, 5, 2, 12);
                        w.Door(5, 1, 12, "copper_door", Dir.North);
                        break;
                }
                // decorative pots are not available: candles on the corners instead
                w.Set(1, 1, 6, "candle", 6); w.Set(9, 1, 6, "candle", 5);
            }
        }
    }
}
