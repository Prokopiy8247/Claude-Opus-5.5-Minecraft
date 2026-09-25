using System;
using System.Collections.Generic;

namespace MCR
{
    // =====================================================================================================
    //  Shipwreck: a battered wooden ship resting on the sea floor (or a beach): hull, deck, stern cabin with
    //  the treasure chest, supply chest below deck at the bow, masts with torn sails.
    // =====================================================================================================
    public sealed class ShipwreckStructure : StructureBase
    {
        static readonly string[] Woods = { "oak", "spruce", "dark_oak", "jungle", "birch" };

        public ShipwreckStructure() { id = "shipwreck"; spacing = 24; separation = 4; salt = 40087; maxRadiusChunks = 2; }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
        {
            if (!(g is OverworldGenerator)) return false;
            var bio = StructureTerrain.BiomeAt(g, x, z);
            if (!bio.isOcean && !bio.isBeach) return false;
            int h = StructureTerrain.Ground(g, x, z);
            return h > g.world.minY + 30 && h < g.world.seaLevel + 4 && StructureTerrain.Relief(g, x - 8, z - 8, x + 8, z + 8) <= 8;
        }

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            int len = rng.Range(15, 23);
            int rot = rng.Next(4);
            int wsx = (rot & 1) == 1 ? len : 9, wsz = (rot & 1) == 1 ? 9 : len;
            int ox = x - wsx / 2, oz = z - wsz / 2;
            int fy = StructureTerrain.FloorLevel(g, ox, oz, ox + wsx - 1, oz + wsz - 1);
            var st = NewStart(x, fy, z);
            var p = new Ship { wood = Woods[rng.Next(Woods.Length)], broken = rng.Chance(0.35f), seed = NextSeed(ref rng) };
            p.Place(ox, fy, oz, 9, len, rot, 4, 16, 2);
            st.pieces.Add(p);
            return Finish(st, cx, cz);
        }

        sealed class Ship : FramedPiece
        {
            public string wood;
            public bool broken;

            int HalfW(int z, int L)
            {
                if (z == 0) return 2;
                if (z >= L - 2) return 1;
                if (z == L - 3) return 2;
                return 3;
            }

            protected override void Emit(SB b)
            {
                b.waterTop = b.gen.world.seaLevel;
                int L = sz, C = 4;
                string planks = wood + "_planks", log = wood == "jungle" ? "jungle_log" : wood + "_log";
                string fence = wood + "_fence", stairs = wood + "_stairs", slab = wood + "_slab";
                int end = broken ? L * 2 / 3 : L;
                for (int z = 0; z < end; z++)
                {
                    int hw = HalfW(z, L);
                    b.Log(C, -1, z, log, 2);
                    for (int x = C - hw; x <= C + hw; x++)
                    {
                        bool side = x == C - hw || x == C + hw;
                        // hull shell with random holes
                        if (!side && !b.P(x, 0, z, 0.1f, 1)) b.Set(x, 0, z, planks);
                        if (side)
                            for (int y = 0; y <= 3; y++)
                                if (!b.P(x, y, z, 0.12f, 2)) b.Set(x, y, z, planks);
                        if (!side)
                        {
                            b.Air(x, 1, z); b.Air(x, 2, z);
                            if (!b.P(x, 3, z, 0.15f, 3)) b.Set(x, 3, z, planks); else b.Air(x, 3, z);
                        }
                        if (side && !b.P(x, 4, z, 0.4f, 4)) b.Set(x, 4, z, fence);
                    }
                }
                // prow and stern trim
                if (!broken) { b.Stairs(C, 3, L - 1, stairs, Dir.North); b.Set(C, 4, L - 1, fence); b.Set(C, 5, L - 1, fence); }
                b.StairsRow(C - 2, 3, 0, C + 2, 0, stairs, Dir.South);
                // stern cabin
                b.Walls(C - 3, 4, 1, C + 3, 6, 5, planks);
                b.Clear(C - 2, 4, 2, C + 2, 6, 4);
                b.Clear(C, 4, 5, C, 5, 5);
                b.Fill(C - 3, 7, 1, C + 3, 7, 5, slab);
                b.Set(C - 3, 5, 3, "glass_pane"); b.Set(C + 3, 5, 3, "glass_pane");
                b.Chest(C - 2, 4, 2, "shipwreck_treasure", Dir.East);
                b.Chest(C + 2, 4, 2, "shipwreck_supply", Dir.West);
                b.Lantern(C, 6, 3, true);
                // hold: supply chest at the bow end below deck
                int bowZ = Math.Min(end - 3, L - 5);
                b.Chest(C, 1, bowZ, "shipwreck_supply", Dir.South);
                b.Barrel(C - 1, 1, bowZ - 1, "shipwreck_supply");
                // masts with yards and tattered sails
                Mast(b, C, L / 2, log, fence);
                if (!broken && L >= 18) Mast(b, C, L / 2 + 5, log, fence);
                if (broken)
                {
                    // the missing bow lies scattered on the floor
                    for (int i = 0; i < 12; i++)
                    {
                        int dx = (int)(b.R(i, 0, 0, 9) * 9) - 4 + C;
                        int dz = end + (int)(b.R(0, 0, i, 10) * 5);
                        int top = b.TerrainTop(dx, dz, 4, -6);
                        if (top != int.MinValue && b.P(dx, 0, dz, 0.6f, 11)) b.Set(dx, top + 1, dz, planks);
                    }
                }
                b.Mob(C, 1, L / 2 - 2, "drowned");
            }

            static void Mast(SB b, int x, int z, string log, string fence)
            {
                b.Fill(x, 4, z, x, 13, z, log, 0);
                b.Fill(x - 3, 11, z, x + 3, 11, z, fence);
                for (int sx = x - 2; sx <= x + 2; sx++)
                    for (int y = 7; y <= 10; y++)
                        if (!b.P(sx, y, z, 0.35f, 12)) b.Set(sx, y, z, "white_wool");
            }
        }
    }

    // =====================================================================================================
    //  Ocean ruin: sunken stone-brick (cold) or sandstone (warm) ruins with loot and drowned; sometimes a
    //  cluster of several.
    // =====================================================================================================
    public sealed class OceanRuinStructure : StructureBase
    {
        public OceanRuinStructure() { id = "ocean_ruin"; spacing = 20; separation = 8; salt = 40093; maxRadiusChunks = 2; }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
        {
            if (!(g is OverworldGenerator)) return false;
            if (!StructureTerrain.BiomeAt(g, x, z).isOcean) return false;
            int h = StructureTerrain.Ground(g, x, z);
            return h < g.world.seaLevel - 3 && h > g.world.minY + 30;
        }

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            string bio = g.BiomeAtApprox(x, z);
            bool warm = bio == "warm_ocean" || bio == "lukewarm_ocean";
            var st = NewStart(x, StructureTerrain.Ground(g, x, z) + 1, z);
            int count = rng.Chance(0.3f) ? rng.Range(3, 5) : 1;
            var used = new List<BBox>();
            for (int i = 0; i < count; i++)
            {
                int r = i == 0 ? rng.Range(4, 6) : rng.Range(3, 4);
                int px = x + (i == 0 ? 0 : rng.Range(-18, 18)), pz = z + (i == 0 ? 0 : rng.Range(-18, 18));
                int s = r * 2 + 1;
                int fy = StructureTerrain.FloorLevel(g, px - r, pz - r, px + r, pz + r);
                if (fy >= g.world.seaLevel - 2) continue;
                var bb = new BBox(px - r - 1, fy - 2, pz - r - 1, px + r + 2, fy + 10, pz + r + 2);
                bool clash = false;
                foreach (var q in used) if (q.Intersects(bb)) { clash = true; break; }
                if (clash) continue;
                used.Add(bb);
                var p = new Ruin { warm = warm, kind = rng.Next(3), seed = NextSeed(ref rng) };
                p.Place(px - r, fy, pz - r, s, s, rng.Next(4), 3, 10, 2);
                st.pieces.Add(p);
            }
            return Finish(st, cx, cz);
        }

        sealed class Ruin : FramedPiece
        {
            public bool warm; public int kind;

            protected override void Emit(SB b)
            {
                b.waterTop = b.gen.world.seaLevel;
                int X = sx - 1, Z = sz - 1;
                string a = warm ? "sandstone" : "stone_bricks";
                string alt = warm ? "cut_sandstone" : "mossy_stone_bricks";
                string alt2 = warm ? "chiseled_sandstone" : "cracked_stone_bricks";
                b.FoundationArea(0, 0, X, Z, 0, warm ? "sandstone" : "gravel", 4);
                b.Fill3(0, 0, 0, X, 0, Z, a, alt, 0.3f, alt2, 0.1f, 1);
                switch (kind)
                {
                    case 0: // a single room, walls broken to varying heights
                        for (int x = 0; x <= X; x++)
                            for (int z = 0; z <= Z; z++)
                            {
                                if (x != 0 && x != X && z != 0 && z != Z) { b.Air(x, 1, z); b.Air(x, 2, z); b.Air(x, 3, z); continue; }
                                int h = 1 + (int)(b.R(x, 0, z, 2) * 4);
                                for (int y = 1; y <= h; y++) b.Set(x, y, z, b.P(x, y, z, 0.3f, 3) ? alt : a);
                                for (int y = h + 1; y <= 4; y++) b.Air(x, y, z);
                            }
                        b.Clear(X / 2, 1, 0, X / 2, 2, 0);
                        b.Chest(1, 1, Z - 1, "underwater_ruin", Dir.South);
                        break;
                    case 1: // two storeys with a collapsed roof
                        b.Walls3(0, 1, 0, X, 3, Z, a, alt, 0.3f, alt2, 0.1f, 4);
                        b.Clear(1, 1, 1, X - 1, 3, Z - 1);
                        for (int x = 0; x <= X; x++)
                            for (int z = 0; z <= Z; z++)
                                if (!b.P(x, 4, z, 0.35f, 5)) b.Set(x, 4, z, warm ? "smooth_sandstone_slab" : "stone_brick_slab");
                        b.Walls3(1, 5, 1, X - 1, 6, Z - 1, a, alt, 0.3f, alt2, 0.15f, 6);
                        b.Clear(2, 5, 2, X - 2, 6, Z - 2);
                        b.Clear(X / 2, 1, 0, X / 2, 2, 0);
                        b.Chest(X - 1, 1, 1, "underwater_ruin", Dir.West);
                        b.Chest(2, 5, 2, "underwater_ruin", Dir.East);
                        break;
                    default: // columns and a fallen arch
                        for (int x = 0; x <= X; x += 2)
                        {
                            int h = 2 + (int)(b.R(x, 0, 0, 7) * 4);
                            b.Fill(x, 1, 0, x, h, 0, a);
                            h = 2 + (int)(b.R(x, 0, Z, 8) * 4);
                            b.Fill(x, 1, Z, x, h, Z, a);
                        }
                        b.Fill(0, 5, 0, X, 5, 0, alt2);
                        b.Clear(1, 1, 1, X - 1, 4, Z - 1);
                        b.Set(X / 2, 1, Z / 2, warm ? "chiseled_sandstone" : "chiseled_stone_bricks");
                        b.Chest(X / 2, 1, Z / 2 + 1, "underwater_ruin", Dir.South);
                        break;
                }
                // rubble and vents scattered outside the ruin, sitting on the sea floor
                for (int i = 0; i < 10; i++)
                {
                    int x = (int)(b.R(i, 0, 0, 10) * (sx + 6)) - 3, z = (int)(b.R(0, 0, i, 11) * (sz + 6)) - 3;
                    if (x >= 0 && x < sx && z >= 0 && z < sz) continue;
                    int g = b.TerrainTop(x, z, 4, -6);
                    if (g == int.MinValue) continue;
                    if (b.BlockAt(x, g, z).isLiquid) continue;
                    if (b.BlockAt(x, g + 1, z).id != "water") continue;
                    if (b.P(x, 0, z, 0.35f, 12)) b.Set(x, g + 1, z, "gravel");
                    else if (b.P(x, 0, z, 0.2f, 13)) b.Set(x, g + 1, z, "magma_block");
                    else if (warm) b.Set(x, g + 1, z, "sand");
                }
                if (warm)
                    for (int i = 0; i < 8; i++)
                    {
                        int x = (int)(b.R(i, 3, 0, 14) * (sx + 6)) - 3, z = (int)(b.R(0, 3, i, 15) * (sz + 6)) - 3;
                        if (x >= 0 && x < sx && z >= 0 && z < sz) continue;
                        int g = b.TerrainTop(x, z, 4, -6);
                        if (g == int.MinValue) continue;
                        string[] kinds = { "tube", "brain", "bubble", "fire", "horn" };
                        string k = kinds[(int)(b.R(x, 0, z, 16) * 5) % 5];
                        if (b.BlockAt(x, g + 1, z).id == "water") b.Set(x, g + 1, z, b.P(x, 0, z, 0.5f, 17) ? k + "_coral_block" : k + "_coral");
                    }
                b.Mob(sx / 2, 1, sz / 2, "drowned");
                if (sx >= 9) b.Mob(1, 1, 1, "drowned");
            }
        }
    }

    // =====================================================================================================
    //  Buried treasure: a single chest a few blocks under beach sand.
    // =====================================================================================================
    public sealed class BuriedTreasureStructure : StructureBase
    {
        public BuriedTreasureStructure() { id = "buried_treasure"; spacing = 10; separation = 2; salt = 40111; maxRadiusChunks = 1; }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
        {
            if (!(g is OverworldGenerator)) return false;
            var bio = StructureTerrain.BiomeAt(g, x, z);
            if (!bio.isBeach || bio.key == "stony_shore") return false;
            int h = StructureTerrain.Ground(g, x, z);
            return h >= g.world.seaLevel - 3 && h <= g.world.seaLevel + 5 && rng.Chance(0.6f);
        }

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            int h = StructureTerrain.Ground(g, x, z);
            var st = NewStart(x, h - 3, z);
            var p = new Treasure { depth = rng.Range(2, 4), seed = NextSeed(ref rng) };
            p.Place(x - 1, h, z - 1, 3, 3, 0, 8, 2, 1);
            st.pieces.Add(p);
            return Finish(st, cx, cz);
        }

        sealed class Treasure : FramedPiece
        {
            public int depth;
            protected override void Emit(SB b)
            {
                // real sand surface of this column (the whole 3x3 sits in one chunk most of the time)
                int top = b.TerrainTop(1, 1, 6, -8);
                if (top == int.MinValue) top = 0;
                int y = top - depth;
                b.Fill(0, y - 1, 0, 2, top, 2, "sand");
                b.Fill(0, y - 2, 0, 2, y - 2, 2, "sandstone");
                b.Chest(1, y, 1, "buried_treasure", Dir.North);
            }
        }
    }
}
