using System;
using System.Collections.Generic;

namespace MCR
{
    // =====================================================================================================
    //  Nether fortress: nether-brick bridges running along x and z on tall pillars, with corridors, blaze
    //  spawners off the crossings, nether wart on soul sand, and fortress loot.
    // =====================================================================================================
    /// <summary>
    /// Fortresses and bastions share one placement grid of 27-chunk regions: each region hosts at most one
    /// "Nether complex", a fortress two times in five and a bastion otherwise, decided by a seeded hash.
    /// </summary>
    public static class NetherComplex
    {
        public const int Spacing = 27, Separation = 4;

        public static bool IsFortress(int seed, int blockX, int blockZ)
        {
            int rx = MathX.FloorDiv(blockX >> 4, Spacing), rz = MathX.FloorDiv(blockZ >> 4, Spacing);
            var rng = new RNG(seed, rx, rz, 30084232);
            return rng.Next(5) < 2;
        }
    }

    public sealed class FortressStructure : StructureBase
    {
        static readonly string[] Mobs = { "zombified_piglin", "wither_skeleton", "blaze" };

        public FortressStructure() { id = "fortress"; dim = DimensionId.Nether; spacing = NetherComplex.Spacing; separation = NetherComplex.Separation; salt = 60013; maxRadiusChunks = 4; }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
        {
            if (!(g is NetherGenerator)) return false;
            if (!NetherComplex.IsFortress(g.seed, x, z)) return false;
            // fortresses favour the wastes and soulsand valley; never in the lava sea
            string b = g.BiomeAtApprox(x, z);
            if (b == "basalt_deltas") return false;
            return g.ApproxSurface(x, z) > NetherGenerator.LavaLevel + 4;
        }

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            int y = rng.Range(48, 76);
            var st = NewStart(x, y, z);
            var bridges = new List<BBox>();
            // the main runs: a long x bridge and a long z bridge crossing at the origin
            int lx = rng.Range(34, 60), lz = rng.Range(34, 60);
            var runX = new Bridge
            {
                x0 = x - lx / 2, x1 = x + lx / 2, z0 = z - 1, z1 = z + 1, y = y, alongX = true,
                seed = NextSeed(ref rng), gen = g
            };
            var runZ = new Bridge
            {
                x0 = x - 1, x1 = x + 1, z0 = z - lz / 2, z1 = z + lz / 2, y = y + (rng.Chance(0.5f) ? 4 : -4), alongX = false,
                seed = NextSeed(ref rng), gen = g
            };
            foreach (var br in new[] { runX, runZ })
            {
                br.Finish();
                bridges.Add(br.box);
                st.pieces.Add(br);
            }
            // a few side branches hanging off the main runs
            for (int i = 0; i < 3; i++)
            {
                var host = rng.NextBool() ? runX : runZ;
                int along = host.alongX ? rng.Range(host.x0 + 6, host.x1 - 6) : rng.Range(host.z0 + 6, host.z1 - 6);
                Dir d = rng.NextBool() ? Dir.North : Dir.South;
                if (!host.alongX) d = rng.NextBool() ? Dir.East : Dir.West;
                var o = DirUtil.Offset[(int)d];
                int len = rng.Range(10, 20);
                var br = new Bridge
                {
                    x0 = Math.Min(along, along + o.x * len), x1 = Math.Max(along, along + o.x * len),
                    z0 = Math.Min(host.z0, host.z0 + o.z * len), z1 = Math.Max(host.z1, host.z1 + o.z * len),
                    y = host.y, alongX = o.x != 0, seed = NextSeed(ref rng), gen = g
                };
                br.Finish();
                bool clash = false;
                foreach (var q in bridges) if (q.Intersects(br.box.Grow(-3))) { clash = true; break; }
                if (clash) continue;
                bridges.Add(br.box);
                st.pieces.Add(br);
                // a crossing with a blaze spawner at the branch mouth
                var cross = new Crossing { x = along, z = host.alongX ? host.z0 : host.z0, y = host.y, seed = NextSeed(ref rng), gen = g };
                cross.Finish(host.alongX);
                st.pieces.Add(cross);
            }
            // the main crossing: corridor, stairwell and the blaze spawner chamber
            var main = new Crossing { x = x, z = z, y = y, main = true, seed = NextSeed(ref rng), gen = g };
            main.Finish(true);
            st.pieces.Add(main);
            // nether wart farm and a storeroom inside the main crossing
            st.pieces.Add(new WartFarm { x = x + 6, y = y, z = z, seed = NextSeed(ref rng), gen = g });
            return Finish(st, cx, cz);
        }

        /// <summary>Straight nether-brick bridge with railings, arches beneath and pillar supports.</summary>
        sealed class Bridge : StructurePiece
        {
            public WorldGenerator gen;
            public int x0, z0, x1, z1, y;
            public bool alongX;

            public void Finish() => box = new BBox(Math.Min(x0, x1), y - 26, Math.Min(z0, z1), Math.Max(x0, x1) + 1, y + 6, Math.Max(z0, z1) + 1);

            public override void Build(StructureWriter w)
            {
                var b = new SB(w, seed).World();
                const string NB = "nether_bricks", F = "nether_brick_fence";
                int lo = alongX ? x0 : z0, hi = alongX ? x1 : z1;
                for (int t = lo; t <= hi; t++)
                {
                    int xx = alongX ? t : x0, zz = alongX ? z0 : t;
                    if (alongX)
                    {
                        b.Fill(xx, y, zz - 1, xx, y, zz + 1, NB);
                        if ((t & 1) == 0) { b.Set(xx, y + 1, zz - 1, F); b.Set(xx, y + 1, zz + 1, F); }
                        else { b.Set(xx, y + 1, zz - 1, NB); b.Set(xx, y + 1, zz + 1, NB); }
                        // arch ribs
                        if (t % 6 == 0)
                        {
                            b.Fill(xx, y - 1, zz - 1, xx, y - 1, zz + 1, NB);
                            b.Set(xx, y - 2, zz - 1, NB); b.Set(xx, y - 2, zz + 1, NB);
                        }
                        if (t % 12 == 0 && b.Owns(xx, y - 3, zz)) b.Fill(xx - 1, y - 22, zz - 1, xx + 1, y - 1, zz + 1, NB);
                    }
                    else
                    {
                        b.Fill(xx - 1, y, zz, xx + 1, y, zz, NB);
                        if ((t & 1) == 0) { b.Set(xx - 1, y + 1, zz, F); b.Set(xx + 1, y + 1, zz, F); }
                        else { b.Set(xx - 1, y + 1, zz, NB); b.Set(xx + 1, y + 1, zz, NB); }
                        if (t % 6 == 0)
                        {
                            b.Fill(xx - 1, y - 1, zz, xx + 1, y - 1, zz, NB);
                            b.Set(xx - 1, y - 2, zz, NB); b.Set(xx + 1, y - 2, zz, NB);
                        }
                        if (t % 12 == 0 && b.Owns(xx, y - 3, zz)) b.Fill(xx - 1, y - 22, zz - 1, xx + 1, y - 1, zz + 1, NB);
                    }
                    // torch every so often
                    if (t % 8 == 3) b.Torch(xx, y + 2, zz, Dir.Up);
                }
                // a couple of wither skeletons patrol the bridge
                int mid = (lo + hi) / 2;
                b.Mob(alongX ? mid : x0, y + 1, alongX ? z0 : mid, Mobs[(int)(b.R(0, 0, 0, 3) * 2)]);
                if ((hi - lo) > 30) b.Mob(alongX ? lo + 6 : x0, y + 1, alongX ? z0 : lo + 6, "wither_skeleton");
            }
        }

        /// <summary>Nether-brick crossing room: 11x11 hall, blaze spawners, ladder stairwell, loot.</summary>
        sealed class Crossing : StructurePiece
        {
            public WorldGenerator gen;
            public int x, y, z;
            public bool main;
            const int H = 8;

            public void Finish(bool alongX)
                => box = new BBox(x - 6, y - 12, z - 6, x + 7, y + H + 4, z + 7);

            public override void Build(StructureWriter w)
            {
                var b = new SB(w, seed).World();
                const string NB = "nether_bricks", F = "nether_brick_fence";
                b.Fill3(x - 5, y, z - 5, x + 5, y, z + 5, NB, "cracked_nether_bricks", 0.15f, null, 0f, 1);
                b.Walls3(x - 5, y + 1, z - 5, x + 5, y + H, z + 5, NB, "cracked_nether_bricks", 0.15f, null, 0f, 2);
                b.Clear(x - 4, y + 1, z - 4, x + 4, y + H - 1, z + 4);
                // windows of fence along the upper walls
                for (int i = -4; i <= 4; i += 2)
                {
                    b.Set(x + i, y + 5, z - 5, F); b.Set(x + i, y + 5, z + 5, F);
                    b.Set(x - 5, y + 5, z + i, F); b.Set(x + 5, y + 5, z + i, F);
                }
                // corner pillars outside
                foreach (var (px, pz) in new[] { (x - 5, z - 5), (x + 5, z - 5), (x - 5, z + 5), (x + 5, z + 5) })
                    b.Fill(px, y + 1, pz, px, y + H, pz, NB);
                // openings into the bridges: cut on all four sides
                for (int i = -1; i <= 1; i++)
                {
                    b.Clear(x + i, y + 1, z - 5, x + i, y + 3, z - 5);
                    b.Clear(x + i, y + 1, z + 5, x + i, y + 3, z + 5);
                    b.Clear(x - 5, y + 1, z + i, x - 5, y + 3, z + i);
                    b.Clear(x + 5, y + 1, z + i, x + 5, y + 3, z + i);
                }
                // roof walkway
                b.Fill3(x - 5, y + H + 1, z - 5, x + 5, y + H + 1, z + 5, NB, "cracked_nether_bricks", 0.2f, null, 0f, 3);
                for (int i = -5; i <= 5; i++) { b.Set(x + i, y + H + 2, z - 5, F); b.Set(x + i, y + H + 2, z + 5, F); }
                for (int i = -5; i <= 5; i++) { b.Set(x - 5, y + H + 2, z + i, F); b.Set(x + 5, y + H + 2, z + i, F); }
                b.Air(x, y + H + 1, z + 5); b.Air(x, y + H + 1, z - 5);
                // ladder stairwell down to a lower level
                b.Fill(x - 1, y - 9, z - 1, x + 1, y - 9, z + 1, NB);
                b.Fill(x - 1, y - 9, z - 1, x + 1, y - 9, z + 1, NB);
                b.Ladders(x - 2, y - 8, y - 1, z - 2, Dir.East);
                b.Air(x - 2, y - 8, z - 2);
                b.Clear(x - 2, y - 8, z - 2, x - 2, y - 1, z - 2);
                b.Clear(x - 2, y - 1, z - 2, x - 2, y - 1, z - 1);
                b.Fill(x - 5, y - 8, z - 5, x + 5, y - 5, z + 5, NB);
                b.Clear(x - 4, y - 7, z - 4, x + 4, y - 6, z + 4);
                // blaze spawners in the basement (both sides of the hall)
                if (main)
                {
                    b.Spawner(x - 3, y - 7, z, "blaze");
                    b.Spawner(x + 3, y - 7, z, "blaze");
                    b.Chest(x, y - 7, z - 3, "nether_fortress", Dir.South);
                }
                else
                {
                    b.Spawner(x, y - 7, z, "blaze");
                }
                b.Chest(x, y + 1, z + 3, "nether_fortress", Dir.North);
                b.Set(x - 2, y + 1, z - 2, "nether_brick_fence");
                // inhabitants
                b.Mob(x, y + 1, z, main ? "blaze" : "wither_skeleton");
                b.Mob(x + 2, y + 1, z - 2, "zombified_piglin");
                b.Mob(x - 2, y + 1, z + 2, "wither_skeleton");
                if (main) b.Mob(x, y - 6, z, "magma_cube");
            }
        }

        /// <summary>Nether wart growing on soul sand beside a lava channel.</summary>
        sealed class WartFarm : StructurePiece
        {
            public WorldGenerator gen;
            public int x, y, z;

            public override void Build(StructureWriter w)
            {
                var b = new SB(w, seed).World();
                int X0 = x - 5, X1 = x + 5, Z0 = z - 5, Z1 = z + 5;
                box = new BBox(X0 - 1, y - 6, Z0 - 1, X1 + 2, y + 4, Z1 + 2);
                b.Fill3(X0, y, Z0, X1, y, Z1, "nether_bricks", "cracked_nether_bricks", 0.15f, null, 0f, 1);
                b.Walls3(X0, y + 1, Z0, X1, y + 3, Z1, "nether_bricks", "cracked_nether_bricks", 0.15f, null, 0f, 2);
                b.Clear(X0 + 1, y + 1, Z0 + 1, X1 - 1, y + 3, Z1 - 1);
                // soul sand beds with nether wart at various ages
                for (int i = X0 + 1; i <= X1 - 1; i++)
                    for (int j = Z0 + 1; j <= Z1 - 1; j++)
                    {
                        bool channel = ((i - X0) % 3 == 2);
                        if (channel) { b.Set(i, y, j, "lava"); continue; }
                        b.Set(i, y, j, "soul_sand");
                        int age = 1 + (int)(b.R(i, 0, j, 5) * 3);
                        b.Set(i, y + 1, j, "nether_wart", age);
                    }
                b.Torch(X0 + 1, y + 3, Z0 + 1, Dir.Up);
                b.Torch(X1 - 1, y + 3, Z1 - 1, Dir.Up);
                b.Chest(X0 + 1, y + 1, Z1 - 1, "nether_fortress", Dir.East);
                b.Mob(X0 + 2, y + 1, Z0 + 2, "zombified_piglin");
            }
        }
    }

    // =====================================================================================================
    //  Bastion remnant: blackstone complex with a central keep, gilded blackstone, gold blocks, lava moat,
    //  piglins / piglin brutes and bastion loot.
    // =====================================================================================================
    public sealed class BastionStructure : StructureBase
    {
        public BastionStructure() { id = "bastion_remnant"; dim = DimensionId.Nether; spacing = NetherComplex.Spacing; separation = NetherComplex.Separation; salt = 60029; maxRadiusChunks = 4; }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
        {
            if (!(g is NetherGenerator)) return false;
            if (NetherComplex.IsFortress(g.seed, x, z)) return false;
            if (g.BiomeAtApprox(x, z) == "basalt_deltas") return false;
            return g.ApproxSurface(x, z) > NetherGenerator.LavaLevel + 3;
        }

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            int y = rng.Range(42, 68);
            var st = NewStart(x, y, z);
            var keep = new Keep { seed = NextSeed(ref rng) };
            int S = 33;
            keep.Place(x - S / 2, y, z - S / 2, S, S, rng.Next(4), 16, 20, 2);
            st.pieces.Add(keep);
            return Finish(st, cx, cz);
        }

        sealed class Keep : FramedPiece
        {
            const string BS = "blackstone", PB = "polished_blackstone", PBB = "polished_blackstone_bricks";
            const string GB = "gilded_blackstone";

            protected override void Emit(SB b)
            {
                const int X = 32, Z = 32, C = 16;
                b.FoundationArea(0, 0, X, Z, 0, BS, 16);
                b.ClearUp(-2, -2, X + 2, Z + 2, 1, 22);
                // terrace floor
                b.Fill3(0, 0, 0, X, 0, Z, PB, BS, 0.3f, PBB, 0.1f, 1);
                // lava moat along the front half
                for (int x = 2; x <= X - 2; x++)
                    for (int z = 2; z <= 3; z++) b.Set(x, 0, z, "lava");
                for (int x = 2; x <= X - 2; x += 3) { b.Set(x, 0, 1, BS); b.Set(x, 0, 4, BS); }
                // outer wall with crenellations, a gate in the middle of the front
                for (int x = 0; x <= X; x++)
                    for (int z = 0; z <= Z; z++)
                    {
                        bool edge = x == 0 || x == X || z == 0 || z == Z;
                        if (!edge) continue;
                        int h = 5 + (x % 5 == 0 || z % 5 == 0 ? 1 : 0);
                        for (int y = 1; y <= h; y++) b.Set(x, y, z, b.P(x, y, z, 0.18f, 2) ? PBB : BS);
                        if (x % 2 == 0 || z % 2 == 0) b.Set(x, h + 1, z, BS);
                    }
                // gate: 3 wide, 4 tall with a bridge over the moat
                b.Clear(C - 1, 1, 0, C + 1, 4, 0);
                b.Fill(C - 1, 5, 0, C + 1, 5, 0, PB);
                b.Fill(C - 1, 0, 1, C + 1, 0, 4, PBB);
                b.Set(C, 6, 0, "chiseled_polished_blackstone");
                // inner courtyard: gold block stacks and gilded blackstone veins
                for (int i = 0; i < 10; i++)
                {
                    int gx = 3 + (int)(b.R(i, 0, 0, 3) * (X - 5));
                    int gz = 6 + (int)(b.R(0, 0, i, 4) * (Z - 8));
                    int gh = 1 + (int)(b.R(i, 1, i, 5) * 3);
                    b.Fill(gx, 1, gz, gx, gh, gz, "gold_block");
                }
                for (int i = 0; i < 20; i++)
                {
                    int gx = 2 + (int)(b.R(i, 0, 0, 6) * (X - 3));
                    int gz = 6 + (int)(b.R(0, 0, i, 7) * (Z - 7));
                    b.Set(gx, 0, gz, GB);
                }
                // central keep: 13x13 blackstone tower
                b.Fill3(C - 6, 1, C - 6, C + 6, 10, C + 6, BS, PBB, 0.25f, GB, 0.06f, 8);
                b.Clear(C - 5, 1, C - 5, C + 5, 9, C + 5);
                b.Fill3(C - 6, 11, C - 6, C + 6, 11, C + 6, PBB, PB, 0.3f, null, 0f, 9);
                for (int i = -6; i <= 6; i += 2) { b.Set(C + i, 12, C - 6, BS); b.Set(C + i, 12, C + 6, BS); b.Set(C - 6, 12, C + i, BS); b.Set(C + 6, 12, C + i, BS); }
                b.Clear(C, 1, C - 6, C + 1, 3, C - 6);
                // keep interior: ramparts, gold trove and the treasure chest
                b.Fill(C - 5, 5, C - 5, C + 5, 5, C + 5, PB);
                b.Clear(C - 4, 5, C - 4, C + 4, 5, C + 4);
                b.Ladders(C, 1, 5, C + 4, Dir.South);
                b.Fill(C - 3, 6, C - 3, C + 3, 6, C + 3, "gold_block");
                b.Clear(C - 2, 6, C - 2, C + 2, 6, C + 2);
                b.Chest(C, 7, C, "bastion_treasure", Dir.North);
                b.Set(C - 1, 7, C, "gilded_blackstone"); b.Set(C + 1, 7, C, "gilded_blackstone");
                // rampart walkway with gold brick accents
                for (int i = -6; i <= 6; i++) { b.Set(C + i, 12, C - 6, i % 3 == 0 ? "gold_block" : PBB); }
                // stable wing: hoglin pens beside the keep
                b.Fill3(2, 1, C - 4, 8, 4, C + 4, PBB, BS, 0.3f, GB, 0.05f, 10);
                b.Clear(3, 1, C - 3, 7, 3, C + 3);
                for (int z = C - 1; z <= C + 1; z++) b.Fill(3, 1, z, 7, 1, z, "crimson_planks");
                b.Fill(3, 2, C - 3, 7, 2, C - 3, "crimson_planks");
                b.Set(5, 1, C + 3, "gold_block");
                b.Chest(3, 1, C - 3, "bastion_hoglin_stable", Dir.East);
                b.Barrel(7, 1, C + 3, "bastion_other");
                b.Set(7, 1, C - 3, "crafting_table");
                b.Torch(4, 3, C, Dir.Up); b.Torch(6, 3, C, Dir.Up);
                // inhabitants
                b.Mob(C, 1, C - 4, "piglin");
                b.Mob(C + 2, 1, C + 2, "piglin_brute");
                b.Mob(5, 1, C, "hoglin");
                b.Mob(6, 1, C + 1, "hoglin");
                b.Mob(4, 1, C - 6, "piglin");
                b.Mob(C, 7, C - 2, "piglin_brute");
                b.Mob(10, 1, 10, "piglin");
            }
        }
    }
}
