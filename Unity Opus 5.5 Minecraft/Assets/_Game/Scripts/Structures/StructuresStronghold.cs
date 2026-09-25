using System;
using System.Collections.Concurrent;
using System.Collections.Generic;

namespace MCR
{
    /// <summary>
    /// Strongholds: 128 per world laid out on eight rings around the origin (first ring 1280-2816 blocks, three
    /// strongholds), snapped to an 8-chunk region grid so Locate() finds the nearest within 30 region steps.
    /// Layout is a random tree of stone-brick pieces grown from a spiral staircase; the portal room is guaranteed.
    /// </summary>
    public sealed class StrongholdStructure : StructureBase
    {
        const int Grid = 8;
        static readonly int[] RingCounts = { 3, 6, 10, 15, 21, 28, 36, 9 };
        static readonly int[] RingStart = { 1280, 4352, 7424, 10496, 13568, 16640, 19712, 22784 };
        const int RingWidth = 1536;

        public StrongholdStructure() { id = "stronghold"; spacing = Grid; separation = 1; salt = 20011; maxRadiusChunks = 7; }

        // ---------------------------------------------------------------- ring placement
        static readonly ConcurrentDictionary<int, HashSet<long>> ringRegions = new ConcurrentDictionary<int, HashSet<long>>();
        static long RKey(int rx, int rz) => ((long)rx << 32) ^ (uint)rz;

        static HashSet<long> Regions(int seed) => ringRegions.GetOrAdd(seed, s =>
        {
            var set = new HashSet<long>();
            var rng = new RNG(s, 0, 0, 77101);
            for (int ring = 0; ring < RingCounts.Length; ring++)
            {
                int n = RingCounts[ring];
                double baseAng = rng.NextDouble() * Math.PI * 2;
                for (int i = 0; i < n; i++)
                {
                    double ang = baseAng + i * Math.PI * 2 / n + (rng.NextDouble() - 0.5) * (Math.PI * 2 / n) * 0.35;
                    double dist = RingStart[ring] + rng.NextDouble() * RingWidth;
                    int bx = (int)Math.Round(Math.Cos(ang) * dist), bz = (int)Math.Round(Math.Sin(ang) * dist);
                    set.Add(RKey(MathX.FloorDiv(bx >> 4, Grid), MathX.FloorDiv(bz >> 4, Grid)));
                }
            }
            return set;
        });

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
        {
            if (!(g is OverworldGenerator)) return false;
            return Regions(g.seed).Contains(RKey(MathX.FloorDiv(x >> 4, Grid), MathX.FloorDiv(z >> 4, Grid)));
        }

        // ---------------------------------------------------------------- layout
        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            int ground = StructureTerrain.Ground(g, x, z);
            int y0 = rng.Range(18, 34);
            if (y0 + 12 > ground - 6) y0 = ground - 18;
            y0 = Math.Max(y0, -36);
            List<ShPiece> best = null;
            for (int attempt = 0; attempt < 10; attempt++)
            {
                var lr = new RNG((long)(rng.NextULong() ^ (ulong)attempt));
                var list = Grow(g, x, y0, z, ref lr, attempt >= 8);
                if (list != null) { best = list; break; }
            }
            if (best == null) return null;
            var st = NewStart(x, y0 + 10, z);
            foreach (var p in best) st.pieces.Add(p);
            return Finish(st, cx, cz);
        }

        struct Open { public int piece, exit, depth; }

        List<ShPiece> Grow(WorldGenerator g, int x, int y0, int z, ref RNG rng, bool forcePortal)
        {
            var pieces = new List<ShPiece>();
            var start = new ShStart { depth = 0, seed = NextSeed(ref rng) };
            Dir d0 = DirUtil.Horizontal[rng.Next(4)];
            start.hasEntrance = false;
            int rot0 = FramedPiece.RotForFront(DirUtil.Opposite(d0));
            start.PlaceCentered(x, y0, z, 5, 5, rot0, 1, 11, 1);
            start.CalcFoot();
            pieces.Add(start);
            var open = new List<Open>();
            open.Add(new Open { piece = 0, exit = 0, depth = 1 });
            int limit = (maxRadiusChunks << 4) - 10;
            int libraries = 0, rooms = 0, prisons = 0, chests = 0;
            bool portal = false;
            int guard = 0;
            while (open.Count > 0 && pieces.Count < 48 && guard++ < 400)
            {
                int oi = rng.Next(open.Count);
                var o = open[oi]; open.RemoveAt(oi);
                var parent = pieces[o.piece];
                parent.WorldExit(o.exit, out int ex, out int ey, out int ez, out Dir ed);
                ShPiece child = null;
                for (int tries = 0; tries < 5 && child == null; tries++)
                {
                    ShPiece c = forcePortal && !portal && o.depth >= 2 ? new ShPortalRoom() : Pick(ref rng, o.depth, portal, libraries, rooms, prisons, chests, tries);
                    if (c == null) break;
                    c.depth = o.depth;
                    c.seed = NextSeed(ref rng);
                    c.doorType = rng.Next(5) == 0 ? 3 : rng.Next(4);
                    if (c is ShPortalRoom) c.doorType = 3;
                    c.Configure(ref rng);
                    c.AnchorTo(ex + DirUtil.Offset[(int)ed].x, ey, ez + DirUtil.Offset[(int)ed].z, ed);
                    var fb = c.foot;
                    if (fb.x0 < x - limit || fb.x1 > x + limit || fb.z0 < z - limit || fb.z1 > z + limit) continue;
                    if (fb.y0 < g.world.minY + 8) continue;
                    bool hit = false;
                    foreach (var q in pieces) if (q.foot.Intersects(fb)) { hit = true; break; }
                    if (hit) continue;
                    child = c;
                }
                if (child == null) continue;
                parent.exitMask |= 1 << o.exit;
                pieces.Add(child);
                if (child is ShLibrary) libraries++;
                else if (child is ShRoomCrossing) rooms++;
                else if (child is ShPrison) prisons++;
                else if (child is ShCorridor cc && cc.chest) chests++;
                else if (child is ShPortalRoom) portal = true;
                if (o.depth < 9)
                    for (int i = 0; i < child.ExitCount; i++)
                    {
                        // side exits of corridors are optional branches
                        if (child.OptionalExit(i) && !rng.Chance(0.35f)) continue;
                        open.Add(new Open { piece = pieces.Count - 1, exit = i, depth = o.depth + 1 });
                    }
            }
            if (!portal)
            {
                // attach the portal room to the deepest free exit that has room for it
                var cand = new List<(int piece, int exit, int depth)>();
                for (int pi = 1; pi < pieces.Count; pi++)
                    for (int e = 0; e < pieces[pi].ExitCount; e++)
                        if ((pieces[pi].exitMask & (1 << e)) == 0) cand.Add((pi, e, pieces[pi].depth));
                cand.Sort((a, b) => b.depth.CompareTo(a.depth));
                foreach (var c in cand)
                {
                    var parent = pieces[c.piece];
                    parent.WorldExit(c.exit, out int ex, out int ey, out int ez, out Dir ed);
                    var pr = new ShPortalRoom { depth = c.depth + 1, seed = NextSeed(ref rng), doorType = 3 };
                    pr.Configure(ref rng);
                    pr.AnchorTo(ex + DirUtil.Offset[(int)ed].x, ey, ez + DirUtil.Offset[(int)ed].z, ed);
                    var fb = pr.foot;
                    if (fb.x0 < x - limit || fb.x1 > x + limit || fb.z0 < z - limit || fb.z1 > z + limit) continue;
                    bool hit = false;
                    foreach (var q in pieces) if (q.foot.Intersects(fb)) { hit = true; break; }
                    if (hit) continue;
                    parent.exitMask |= 1 << c.exit;
                    pieces.Add(pr);
                    portal = true;
                    break;
                }
            }
            return portal ? pieces : null;
        }

        static ShPiece Pick(ref RNG rng, int depth, bool portal, int libs, int rooms, int prisons, int chests, int tries)
        {
            if (tries >= 3) return new ShCorridor { minLen = 5, maxLen = 5 };
            int total = 0;
            int wCorr = 36, wChest = chests < 4 ? 8 : 0, wTurn = 12, wCross = 10, wRoom = rooms < 4 ? 12 : 0, wStairs = depth > 1 ? 7 : 0, wSpiral = depth > 1 ? 4 : 0;
            int wPrison = prisons < 2 ? 5 : 0, wLib = libs < 2 && depth >= 2 ? 6 : 0, wPortal = !portal && depth >= 4 ? 9 : 0;
            total = wCorr + wChest + wTurn + wCross + wRoom + wStairs + wSpiral + wPrison + wLib + wPortal;
            int r = rng.Next(total);
            if ((r -= wCorr) < 0) return new ShCorridor();
            if ((r -= wChest) < 0) return new ShCorridor { chest = true, minLen = 7, maxLen = 7 };
            if ((r -= wTurn) < 0) return new ShTurn();
            if ((r -= wCross) < 0) return new ShCrossing();
            if ((r -= wRoom) < 0) return new ShRoomCrossing();
            if ((r -= wStairs) < 0) return new ShStairsDown();
            if ((r -= wSpiral) < 0) return new ShSpiralDown();
            if ((r -= wPrison) < 0) return new ShPrison();
            if ((r -= wLib) < 0) return new ShLibrary();
            return new ShPortalRoom();
        }

        // =================================================================================================
        //  Pieces. Local frame: entrance on the south wall (z = 0) centred on x = sx/2, piece extends to +z.
        // =================================================================================================
        abstract class ShPiece : FramedPiece
        {
            public int depth, doorType, exitMask;
            public bool hasEntrance = true;
            public BBox foot;     // footprint box used for collisions (no padding)
            protected int yMin, yMax = 4;
            public virtual int ExitCount => 0;
            public virtual bool OptionalExit(int i) => false;
            /// <summary>Exit opening centre (floor level) on a wall, with its outward local direction.</summary>
            public virtual void Exit(int i, out int lx, out int ly, out int lz, out Dir d) { lx = sx / 2; ly = 0; lz = sz - 1; d = Dir.North; }
            public virtual void Configure(ref RNG rng) { }

            public void AnchorTo(int tx, int ty, int tz, Dir d)
            {
                int r = RotForFront(DirUtil.Opposite(d));
                int ex = sx / 2, ox, oz;
                switch (r)
                {
                    case 1: ox = tx; oz = tz - (sx - 1 - ex); break;
                    case 2: ox = tx - (sx - 1 - ex); oz = tz - (sz - 1); break;
                    case 3: ox = tx - (sz - 1); oz = tz - ex; break;
                    default: ox = tx - ex; oz = tz; break;
                }
                Place(ox, ty, oz, sx, sz, r, -yMin + 1, yMax + 1, 1);
                CalcFoot();
            }
            public void CalcFoot() => foot = new BBox(ox, oy + yMin, oz, ox + WorldSizeX, oy + yMax + 1, oz + WorldSizeZ);

            public void WorldExit(int i, out int wx, out int wy, out int wz, out Dir wd)
            {
                Exit(i, out int lx, out int ly, out int lz, out Dir d);
                var t = new SBFrameMath(ox, oz, sx, sz, rot);
                t.Map(lx, lz, out wx, out wz);
                wy = oy + ly;
                wd = DirUtil.IsHorizontal(d) ? DirUtil.FromHorizIndex(DirUtil.HorizIndex(d) + rot) : d;
            }

            protected static void Bricks(SB b, int x0, int y0, int z0, int x1, int y1, int z1)
                => b.Fill3(x0, y0, z0, x1, y1, z1, "stone_bricks", "mossy_stone_bricks", 0.16f, "cracked_stone_bricks", 0.12f, 1);
            protected static void BrickShell(SB b, int x0, int y0, int z0, int x1, int y1, int z1)
            {
                Bricks(b, x0, y0, z0, x1, y0, z1);
                Bricks(b, x0, y1, z0, x1, y1, z1);
                b.Walls3(x0, y0 + 1, z0, x1, y1 - 1, z1, "stone_bricks", "mossy_stone_bricks", 0.16f, "cracked_stone_bricks", 0.12f, 2);
                b.Clear(x0 + 1, y0 + 1, z0 + 1, x1 - 1, y1 - 1, z1 - 1);
            }

            /// <summary>Cuts openings for connected exits and builds the entrance doorway.</summary>
            protected void Doorways(SB b)
            {
                for (int i = 0; i < ExitCount; i++)
                {
                    if ((exitMask & (1 << i)) == 0) continue;
                    Exit(i, out int lx, out int ly, out int lz, out Dir d);
                    if (d == Dir.North || d == Dir.South) b.Clear(lx - 1, ly + 1, lz, lx + 1, ly + 3, lz);
                    else b.Clear(lx, ly + 1, lz - 1, lx, ly + 3, lz + 1);
                }
                if (!hasEntrance) return;
                int ex = sx / 2;
                switch (doorType)
                {
                    case 0: b.Clear(ex - 1, 1, 0, ex + 1, 3, 0); break;
                    case 1: b.Clear(ex, 1, 0, ex, 2, 0); b.Door(ex, 1, 0, "oak_door", Dir.North); break;
                    case 2:
                        b.Fill(ex - 1, 1, 0, ex + 1, 3, 0, "iron_bars");
                        b.Clear(ex, 1, 0, ex, 2, 0);
                        break;
                    default:
                        b.Clear(ex, 1, 0, ex, 2, 0);
                        b.Door(ex, 1, 0, "iron_door", Dir.North);
                        // buttons on both sides of the iron door (attached to this piece's wall)
                        b.Set(ex + 1, 2, -1, "stone_button", DirUtil.HorizIndex(Dir.South) | (1 << 2));
                        b.Set(ex + 1, 2, 1, "stone_button", DirUtil.HorizIndex(Dir.North) | (1 << 2));
                        break;
                }
            }
        }

        /// <summary>Stand-alone rotation math for layout-time exit positions (same mapping as SB).</summary>
        struct SBFrameMath
        {
            readonly int ox, oz, sx, sz, rot;
            public SBFrameMath(int ox, int oz, int sx, int sz, int rot) { this.ox = ox; this.oz = oz; this.sx = sx; this.sz = sz; this.rot = rot; }
            public void Map(int lx, int lz, out int wx, out int wz)
            {
                switch (rot)
                {
                    case 1: wx = ox + lz; wz = oz + (sx - 1 - lx); break;
                    case 2: wx = ox + (sx - 1 - lx); wz = oz + (sz - 1 - lz); break;
                    case 3: wx = ox + (sz - 1 - lz); wz = oz + lx; break;
                    default: wx = ox + lx; wz = oz + lz; break;
                }
            }
        }

        // ---------------------------------------------------------------- spiral staircase start
        sealed class ShStart : ShPiece
        {
            public ShStart() { sx = 5; sz = 5; yMin = 0; yMax = 10; }
            public override int ExitCount => 1;
            public override void Exit(int i, out int lx, out int ly, out int lz, out Dir d) { lx = 2; ly = 0; lz = 4; d = Dir.North; }
            static readonly int[] RX = { 1, 1, 1, 2, 3, 3, 3, 2 }, RZ = { 3, 2, 1, 1, 1, 2, 3, 3 };
            protected override void Emit(SB b)
            {
                BrickShell(b, 0, 0, 0, 4, 10, 4);
                Bricks(b, 2, 1, 2, 2, 9, 2);
                // stairs wind up around the central pillar; the lowest step sits beside the exit
                for (int i = 0; i < 8; i++)
                {
                    int nx = RX[(i + 1) % 8] - RX[i], nz = RZ[(i + 1) % 8] - RZ[i];
                    Dir f = nx > 0 ? Dir.East : nx < 0 ? Dir.West : nz > 0 ? Dir.North : Dir.South;
                    b.Stairs(RX[i], 1 + i, RZ[i], "stone_brick_stairs", f);
                }
                b.Torch(2, 3, 1, Dir.South);
                b.Torch(2, 6, 3, Dir.North);
                Doorways(b);
            }
        }

        // ---------------------------------------------------------------- corridors
        sealed class ShCorridor : ShPiece
        {
            public bool chest;
            public int minLen = 5, maxLen = 13;
            public override void Configure(ref RNG rng) { sx = 5; sz = rng.Range(minLen, maxLen); yMin = 0; yMax = 4; }
            public override int ExitCount => sz >= 9 && !chest ? 3 : 1;
            public override bool OptionalExit(int i) => i > 0;
            public override void Exit(int i, out int lx, out int ly, out int lz, out Dir d)
            {
                ly = 0;
                if (i == 0) { lx = 2; lz = sz - 1; d = Dir.North; }
                else if (i == 1) { lx = 0; lz = sz / 2; d = Dir.West; }
                else { lx = 4; lz = sz / 2; d = Dir.East; }
            }
            protected override void Emit(SB b)
            {
                BrickShell(b, 0, 0, 0, 4, 4, sz - 1);
                // torches near both ends (the middle of the walls may be cut by side exits)
                b.Torch(1, 3, 1, Dir.East);
                if (sz >= 7) b.Torch(3, 3, sz - 2, Dir.West);
                if (chest)
                {
                    // chest niche in the east wall with a slab step
                    b.Clear(4, 1, 3, 4, 2, 3);
                    b.Chest(4, 1, 3, "stronghold_corridor", Dir.West);
                    b.Set(3, 1, 3, "stone_brick_slab", 0);
                    Bricks(b, 5, 0, 2, 5, 3, 4);
                }
                // occasional rubble
                for (int z = 1; z < sz - 1; z++)
                    if (b.P(1, 1, z, 0.06f, 5)) b.Set(1, 1, z, "cobblestone");
                Doorways(b);
            }
        }

        sealed class ShTurn : ShPiece
        {
            bool right;
            public override void Configure(ref RNG rng) { sx = 5; sz = 5; yMin = 0; yMax = 4; right = rng.NextBool(); }
            public override int ExitCount => 1;
            public override void Exit(int i, out int lx, out int ly, out int lz, out Dir d) { ly = 0; lz = 2; if (right) { lx = 4; d = Dir.East; } else { lx = 0; d = Dir.West; } }
            protected override void Emit(SB b)
            {
                BrickShell(b, 0, 0, 0, 4, 4, 4);
                b.Torch(2, 3, 3, Dir.South);
                Doorways(b);
            }
        }

        sealed class ShCrossing : ShPiece
        {
            public override void Configure(ref RNG rng) { sx = 11; sz = 11; yMin = 0; yMax = 6; }
            public override int ExitCount => 3;
            public override bool OptionalExit(int i) => i > 0 && false;
            public override void Exit(int i, out int lx, out int ly, out int lz, out Dir d)
            {
                ly = 0;
                if (i == 0) { lx = 5; lz = 10; d = Dir.North; }
                else if (i == 1) { lx = 0; lz = 5; d = Dir.West; }
                else { lx = 10; lz = 5; d = Dir.East; }
            }
            protected override void Emit(SB b)
            {
                BrickShell(b, 0, 0, 0, 10, 6, 10);
                // four pillars with torches and a raised walkway ring
                foreach (var (px, pz) in new[] { (3, 3), (7, 3), (3, 7), (7, 7) })
                {
                    Bricks(b, px, 1, pz, px, 5, pz);
                    b.Torch(px, 3, pz - 1 < 3 ? pz - 1 : pz + 1, pz < 5 ? Dir.South : Dir.North);
                }
                b.Fill(4, 5, 4, 6, 5, 6, "stone_brick_slab", 1);
                b.Set(5, 0, 5, "chiseled_stone_bricks");
                Doorways(b);
            }
        }

        sealed class ShRoomCrossing : ShPiece
        {
            int kind;
            public override void Configure(ref RNG rng) { sx = 11; sz = 11; yMin = 0; yMax = 6; kind = rng.Next(3); }
            public override int ExitCount => 3;
            public override bool OptionalExit(int i) => i > 0;
            public override void Exit(int i, out int lx, out int ly, out int lz, out Dir d)
            {
                ly = 0;
                if (i == 0) { lx = 5; lz = 10; d = Dir.North; }
                else if (i == 1) { lx = 0; lz = 5; d = Dir.West; }
                else { lx = 10; lz = 5; d = Dir.East; }
            }
            protected override void Emit(SB b)
            {
                BrickShell(b, 0, 0, 0, 10, 6, 10);
                switch (kind)
                {
                    case 0: // central pillar lit on four sides
                        Bricks(b, 5, 1, 5, 5, 5, 5);
                        b.Torch(5, 3, 4, Dir.South); b.Torch(5, 3, 6, Dir.North); b.Torch(4, 3, 5, Dir.West); b.Torch(6, 3, 5, Dir.East);
                        break;
                    case 1: // fountain
                        b.Walls(3, 1, 3, 7, 1, 7, "stone_brick_slab", 0);
                        b.Fill(4, 1, 4, 6, 1, 6, "water");
                        Bricks(b, 5, 1, 5, 5, 4, 5);
                        b.Set(5, 5, 5, "water");
                        b.Torch(1, 3, 5, Dir.East); b.Torch(9, 3, 5, Dir.West);
                        break;
                    default: // storeroom with a mezzanine
                        b.Fill(1, 3, 6, 9, 3, 9, "oak_planks");
                        for (int x = 1; x <= 9; x++) b.Set(x, 4, 6, "oak_fence");
                        b.Air(5, 4, 6);
                        Bricks(b, 5, 1, 6, 5, 2, 6);
                        b.Ladders(5, 1, 3, 5, Dir.South);
                        b.Chest(2, 4, 9, "stronghold_crossing", Dir.South);
                        b.Chest(8, 1, 9, "stronghold_crossing", Dir.South);
                        b.Torch(1, 5, 8, Dir.East); b.Torch(9, 2, 3, Dir.West);
                        break;
                }
                Doorways(b);
            }
        }

        sealed class ShStairsDown : ShPiece
        {
            public override void Configure(ref RNG rng) { sx = 5; sz = 11; yMin = -7; yMax = 4; }
            public override int ExitCount => 1;
            public override void Exit(int i, out int lx, out int ly, out int lz, out Dir d) { lx = 2; ly = -7; lz = 10; d = Dir.North; }
            protected override void Emit(SB b)
            {
                BrickShell(b, 0, -7, 0, 4, 4, 10);
                // solid landing at the top, stair run descending north, bottom floor is the shell floor
                Bricks(b, 1, -6, 0, 3, 0, 1);
                for (int z = 2; z <= 8; z++)
                {
                    int top = 2 - z;
                    if (top - 1 >= -6) Bricks(b, 1, -6, z, 3, top - 1, z);
                    b.StairsRow(1, top, z, 3, z, "stone_brick_stairs", Dir.South);
                }
                b.Torch(1, 2, 1, Dir.East);
                b.Torch(3, -4, 9, Dir.West);
                Doorways(b);
            }
        }

        sealed class ShSpiralDown : ShPiece
        {
            public override void Configure(ref RNG rng) { sx = 5; sz = 5; yMin = -7; yMax = 4; }
            public override int ExitCount => 1;
            public override void Exit(int i, out int lx, out int ly, out int lz, out Dir d) { lx = 2; ly = -7; lz = 4; d = Dir.North; }
            // descending ring starting beside the entrance (south side) ending beside the north exit
            static readonly int[] RX = { 3, 3, 3, 2, 1, 1, 1 }, RZ = { 1, 2, 3, 3, 3, 2, 1 };
            protected override void Emit(SB b)
            {
                BrickShell(b, 0, -7, 0, 4, 4, 4);
                Bricks(b, 2, -6, 2, 2, 3, 2);
                // floor at the bottom, entrance landing at the top
                Bricks(b, 1, -7, 1, 3, -7, 3);
                b.Set(2, 0, 1, "stone_bricks");
                for (int i = 0; i < 7; i++)
                {
                    // floating steps hug the pillar so the bottom level stays walkable to the exit
                    int y = -1 - i;
                    int px = RX[i], pz = RZ[i];
                    int bx = i == 0 ? 2 : RX[i - 1], bz = i == 0 ? 1 : RZ[i - 1];
                    Dir f = bx > px ? Dir.East : bx < px ? Dir.West : bz > pz ? Dir.North : Dir.South;
                    b.Stairs(px, y, pz, "stone_brick_stairs", f);
                }
                b.Torch(2, 2, 3, Dir.South);
                Doorways(b);
            }
        }

        sealed class ShPrison : ShPiece
        {
            public override void Configure(ref RNG rng) { sx = 11; sz = 11; yMin = 0; yMax = 4; }
            public override int ExitCount => 1;
            public override void Exit(int i, out int lx, out int ly, out int lz, out Dir d) { lx = 5; ly = 0; lz = 10; d = Dir.North; }
            protected override void Emit(SB b)
            {
                BrickShell(b, 0, 0, 0, 10, 4, 10);
                // barred cell walls on both sides of a central hall
                b.Fill(3, 1, 1, 3, 3, 9, "iron_bars");
                b.Fill(7, 1, 1, 7, 3, 9, "iron_bars");
                Bricks(b, 1, 1, 5, 2, 3, 5); Bricks(b, 8, 1, 5, 9, 3, 5);
                Bricks(b, 3, 1, 5, 3, 3, 5); Bricks(b, 7, 1, 5, 7, 3, 5);
                foreach (int cz in new[] { 2, 7 })
                {
                    b.Clear(3, 1, cz, 3, 2, cz); b.Door(3, 1, cz, "iron_door", Dir.West);
                    b.Clear(7, 1, cz, 7, 2, cz); b.Door(7, 1, cz, "iron_door", Dir.East);
                }
                b.Torch(5, 3, 9, Dir.South);
                b.Torch(5, 3, 1, Dir.North);
                Doorways(b);
            }
        }

        sealed class ShLibrary : ShPiece
        {
            bool tall;
            public override void Configure(ref RNG rng) { tall = rng.NextBool(); sx = 15; sz = tall ? 15 : 11; yMin = 0; yMax = tall ? 10 : 6; }
            protected override void Emit(SB b)
            {
                int X = sx - 1, Z = sz - 1, H = yMax;
                BrickShell(b, 0, 0, 0, X, H, Z);
                b.Fill(1, 0, 1, X - 1, 0, Z - 1, "oak_planks");
                // bookshelf lining
                b.Fill(1, 1, Z - 1, X - 1, H - 1, Z - 1, "bookshelf");
                b.Fill(1, 1, 1, 1, H - 1, Z - 1, "bookshelf");
                b.Fill(X - 1, 1, 1, X - 1, H - 1, Z - 1, "bookshelf");
                b.Fill(1, 1, 1, X - 1, H - 1, 1, "bookshelf");
                b.Clear(sx / 2 - 1, 1, 1, sx / 2 + 1, 3, 1);
                // shelf rows
                for (int z = 4; z <= Z - 3; z += 3)
                {
                    b.Fill(3, 1, z, 5, 3, z, "bookshelf");
                    b.Fill(X - 5, 1, z, X - 3, 3, z, "bookshelf");
                }
                if (tall)
                {
                    // two-wide gallery around the walls, railing on its inner edge, ladder in a corner
                    b.Fill(2, 5, 2, X - 2, 5, Z - 2, "oak_planks");
                    b.Clear(4, 5, 4, X - 4, 5, Z - 4);
                    for (int x = 3; x <= X - 3; x++) { b.Set(x, 6, 3, "oak_fence"); b.Set(x, 6, Z - 3, "oak_fence"); }
                    for (int z = 3; z <= Z - 3; z++) { b.Set(3, 6, z, "oak_fence"); b.Set(X - 3, 6, z, "oak_fence"); }
                    b.Air(X - 3, 6, Z - 3);
                    b.Ladders(X - 2, 1, 6, Z - 2, Dir.West);
                    b.Chest(2, 6, Z - 2, "stronghold_library", Dir.South);
                    // chandelier
                    int c = sx / 2, cz = sz / 2;
                    b.Fill(c, H - 3, cz, c, H - 1, cz, "oak_fence");
                    b.Set(c - 1, H - 3, cz, "oak_fence"); b.Set(c + 1, H - 3, cz, "oak_fence");
                    b.Set(c, H - 3, cz - 1, "oak_fence"); b.Set(c, H - 3, cz + 1, "oak_fence");
                    b.Torch(c - 1, H - 2, cz); b.Torch(c + 1, H - 2, cz); b.Torch(c, H - 2, cz - 1); b.Torch(c, H - 2, cz + 1);
                }
                else
                {
                    b.Torch(sx / 2, 4, Z - 2, Dir.South);
                }
                b.Chest(sx / 2, 1, Z - 1, "stronghold_library", Dir.South);
                // cobwebs in the corners and between shelves
                for (int y = 1; y < H; y++)
                    for (int z = 2; z < Z - 1; z++)
                        for (int x = 2; x < X - 1; x++)
                            if (b.P(x, y, z, 0.035f, 17) && b.IsAir(x, y, z)) b.Set(x, y, z, "cobweb");
                b.Clear(sx / 2 - 1, 1, 1, sx / 2 + 1, 3, 2);
                Doorways(b);
            }
        }

        sealed class ShPortalRoom : ShPiece
        {
            public override void Configure(ref RNG rng) { sx = 11; sz = 16; yMin = -1; yMax = 8; }
            protected override void Emit(SB b)
            {
                int X = 10, Z = 15, H = 8;
                BrickShell(b, 0, -1, 0, X, H, Z);
                Bricks(b, 1, 0, 1, X - 1, 0, Z - 1);
                // side lava pools beside the dais
                b.Fill(1, 0, 9, 2, 0, 13, "lava");
                b.Fill(8, 0, 9, 9, 0, 13, "lava");
                // iron-bar windows high on the side walls
                for (int z = 3; z <= Z - 3; z += 3) { b.Fill(0, 4, z, 0, 5, z, "iron_bars"); b.Fill(X, 4, z, X, 5, z, "iron_bars"); }
                // dais
                Bricks(b, 3, 1, 7, 7, 3, 14);
                // stairs up to the dais
                b.StairsRow(4, 1, 4, 6, 4, "stone_brick_stairs", Dir.North);
                Bricks(b, 4, 1, 5, 6, 1, 5);
                b.StairsRow(4, 2, 5, 6, 5, "stone_brick_stairs", Dir.North);
                Bricks(b, 4, 1, 6, 6, 2, 6);
                b.StairsRow(4, 3, 6, 6, 6, "stone_brick_stairs", Dir.North);
                // silverfish spawner on the landing
                b.Spawner(5, 4, 7, "silverfish");
                // portal frame ring (centre 5, 11), lava below the centre
                int cxl = 5, czl = 11, fy = 4;
                b.Fill(cxl - 1, fy - 1, czl - 1, cxl + 1, fy - 1, czl + 1, "lava");
                b.Clear(cxl - 1, fy, czl - 1, cxl + 1, fy + 2, czl + 1);
                for (int dz = -2; dz <= 2; dz++)
                    for (int dx = -2; dx <= 2; dx++)
                    {
                        bool ring = (Math.Abs(dx) == 2) != (Math.Abs(dz) == 2);
                        if (!ring) continue;
                        // frames face the centre of the ring
                        Dir f = dz == 2 ? Dir.South : dz == -2 ? Dir.North : dx == 2 ? Dir.West : Dir.East;
                        bool eye = b.R(cxl + dx, fy, czl + dz, 4242) < 0.1f;
                        b.Facing(cxl + dx, fy, czl + dz, "end_portal_frame", f, eye ? 4 : 0);
                    }
                // light
                b.Torch(1, 3, 2, Dir.East); b.Torch(X - 1, 3, 2, Dir.West);
                b.Torch(1, 5, Z - 1, Dir.East); b.Torch(X - 1, 5, Z - 1, Dir.West);
                Doorways(b);
            }
        }
    }
}
