using System;
using System.Collections.Generic;

namespace MCR
{
    // =====================================================================================================
    //  Mineshaft: a network of plank-supported corridors with rails, cobwebs, a cave-spider spawner and chests.
    //  Corridors stay level; the network meanders in y between separate legs so it feels layered.
    // =====================================================================================================
    public sealed class MineshaftStructure : StructureBase
    {
        public MineshaftStructure() { id = "mineshaft"; spacing = 22; separation = 6; salt = 30011; maxRadiusChunks = 5; }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
        {
            if (!(g is OverworldGenerator)) return false;
            return rng.Chance(0.7f);
        }

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            var st = NewStart(x, rng.Range(-44, 32), z);
            var cells = new List<BBox>();
            int px = x, pz = z, py = st.y;
            Dir d = DirUtil.Horizontal[rng.Next(4)];
            int legs = 0, nodes = rng.Range(6, 11);
            bool spiderPlaced = false, chestPlaced = false;
            for (int i = 0; i < nodes; i++)
            {
                if (rng.Chance(0.3f)) d = rng.NextBool() ? DirUtil.RotateCW(d) : DirUtil.RotateCCW(d);
                int len = rng.Range(10, 22);
                var o = DirUtil.Offset[(int)d];
                int ex = px + o.x * len, ez = pz + o.z * len;
                int ey = py + (rng.Chance(0.3f) ? rng.Range(-7, 7) : 0);
                ey = Sr.Clamp(ey, -52, 38);
                if (!TryLeg(g, st, cells, px, pz, py, ex, ez, ey, o.x != 0, ref rng, ref spiderPlaced, ref chestPlaced)) { }
                else legs++;
                px = ex; pz = ez; py = ey;
                // braided cross corridor
                if (rng.Chance(0.5f))
                {
                    Dir cd = rng.NextBool() ? DirUtil.RotateCW(d) : DirUtil.RotateCCW(d);
                    var co = DirUtil.Offset[(int)cd];
                    int cl = rng.Range(7, 15);
                    if (TryLeg(g, st, cells, px, pz, py, px + co.x * cl, pz + co.z * cl, py, co.x != 0, ref rng, ref spiderPlaced, ref chestPlaced)) legs++;
                }
            }
            if (legs < 3) return null;
            var room = new MineshaftRoom { x = px, z = pz, y = py, seed = NextSeed(ref rng), gen = g };
            room.ComputeBox();
            bool clash = false;
            foreach (var q in cells) if (q.Intersects(room.box)) { clash = true; break; }
            if (!clash) st.pieces.Add(room);
            return Finish(st, cx, cz);
        }

        static bool TryLeg(WorldGenerator g, StructureStart st, List<BBox> cells, int x0, int z0, int y0, int x1, int z1, int y1, bool alongX, ref RNG rng, ref bool spider, ref bool chest)
        {
            // the corridor stores its floor heights from the min end to the max end
            bool reversed = alongX ? x1 < x0 : z1 < z0;
            var c = new MineshaftCorridor
            {
                x0 = Math.Min(x0, x1), x1 = Math.Max(x0, x1), z0 = Math.Min(z0, z1), z1 = Math.Max(z0, z1),
                y0 = reversed ? y1 : y0, y1 = reversed ? y0 : y1, alongX = alongX, len = Math.Abs(alongX ? x1 - x0 : z1 - z0),
                spider = !spider && rng.Chance(0.35f), chest = !chest && rng.Chance(0.45f),
                seed = NextSeed(ref rng), gen = g
            };
            c.Finish();
            bool clash = false;
            foreach (var q in cells) if (q.Intersects(c.box)) { clash = true; break; }
            if (clash) return false;
            if (c.box.x0 < st.x - 78 || c.box.x1 > st.x + 78 || c.box.z0 < st.z - 78 || c.box.z1 > st.z + 78) return false;
            cells.Add(c.box);
            st.pieces.Add(c);
            if (c.spider) spider = true;
            if (c.chest) chest = true;
            return true;
        }
    }

    /// <summary>One straight mined tunnel: 3x3 opening, plank props, rails, cobwebs, torches, loot.</summary>
    public sealed class MineshaftCorridor : StructurePiece
    {
        public WorldGenerator gen;
        public int x0, z0, x1, z1, y0, y1, len;   // y0 is the floor at the min end, y1 at the max end
        public bool alongX;
        public bool spider, chest;

        public void Finish()
        {
            int lo = Math.Min(y0, y1), hi = Math.Max(y0, y1);
            box = new BBox(x0 - 2, lo - 4, z0 - 2, x1 + 3, hi + 6, z1 + 3);
        }

        public override void Build(StructureWriter w)
        {
            var b = new SB(w, seed).World();
            // run along x, or along z via a quarter-turn frame so one code path serves both
            if (alongX) b.Frame(x0, 0, z0 - 1, len + 1, 3, 0);
            else b.Frame(x0 - 1, 0, z0, len + 1, 3, 3);
            // local: t = 0..len along the run, c = 0..2 across (1 = centre line)
            for (int t = 0; t <= len; t++)
            {
                int y = Y(t);
                b.Clear(t, y, 0, t, y + 3, 2);
                for (int c = 0; c <= 2; c++) if (b.BlockAt(t, y - 1, c).isAir || b.BlockAt(t, y - 1, c).isLiquid) b.Set(t, y - 1, c, "oak_planks");
            }
            for (int t = 1; t < len; t++) b.Rail(t, Y(t), 1, true);
            for (int t = 2; t < len; t += 5)
            {
                int y = Y(t);
                b.Fill(t, y + 1, 0, t, y + 2, 0, "oak_fence");
                b.Fill(t, y + 1, 2, t, y + 2, 2, "oak_fence");
                b.Fill(t, y + 3, 0, t, y + 3, 2, "oak_planks");
                if ((t / 5) % 2 == 0) b.Torch(t + 1, y + 2, 2, Dir.South);
            }
            for (int t = 0; t <= len; t++)
                for (int c = 0; c <= 2; c++)
                    for (int dy = 1; dy <= 3; dy++)
                        if (b.P(t, dy, c, 0.06f, 3) && b.IsAir(t, Y(t) + dy, c)) b.Set(t, Y(t) + dy, c, "cobweb");
            int mid = len / 2;
            if (chest) b.Chest(mid, Y(mid), 2, "mineshaft", Dir.South);
            if (spider)
            {
                int s = Math.Min(len - 1, mid + 3);
                b.Spawner(s, Y(s), 0, "cave_spider");
                for (int t = s - 2; t <= s + 2; t++)
                    for (int c = 0; c <= 2; c++)
                        for (int dy = 0; dy <= 3; dy++)
                            if (b.IsAir(t, Y(s) + dy, c) && b.P(t, dy, c, 0.45f, 9)) b.Set(t, Y(s) + dy, c, "cobweb");
            }
        }

        int Y(int t) => y0 + (y1 - y0) * t / Math.Max(1, len);
    }

    /// <summary>Timber room where two mineshaft legs meet.</summary>
    public sealed class MineshaftRoom : StructurePiece
    {
        public WorldGenerator gen;
        public int x, y, z;
        const int R = 5, H = 5;

        public void ComputeBox() => box = new BBox(x - R - 2, y - 4, z - R - 2, x + R + 3, y + H + 4, z + R + 3);

        public override void Build(StructureWriter w)
        {
            var b = new SB(w, seed).World();
            int x0 = x - R, x1 = x + R, z0 = z - R, z1 = z + R;
            b.Clear(x0, y + 1, z0, x1, y + H, z1);
            for (int zz = z0; zz <= z1; zz++)
                for (int xx = x0; xx <= x1; xx++)
                    if (b.BlockAt(xx, y, zz).isAir) b.Set(xx, y, zz, "oak_planks");
            b.Fill(x0, y + H + 1, z0, x1, y + H + 1, z1, "oak_planks");
            b.Fill(x0 + 1, y + H + 1, z0 + 1, x1 - 1, y + H + 1, z1 - 1, 0);
            // corner and mid posts
            foreach (var (px, pz) in new[] { (x0, z0), (x0, z1), (x1, z0), (x1, z1), (x0, z), (x1, z), (x, z0), (x, z1) })
                b.Fill(px, y + 1, pz, px, y + H, pz, "oak_log", 0);
            // side bays for storage
            for (int i = -1; i <= 1; i += 2)
            {
                b.Fill(x0 + 1, y + 1, z + i, x0 + 2, y + 3, z + i, "oak_fence");
                b.Fill(x1 - 2, y + 1, z + i, x1 - 1, y + 3, z + i, "oak_fence");
            }
            b.Torch(x, y + H, z - 3, Dir.South);
            b.Torch(x, y + H, z + 3, Dir.North);
            b.Torch(x - 3, y + H, z, Dir.East);
            b.Torch(x + 3, y + H, z, Dir.West);
            b.Chest(x - 3, y + 1, z - 3, "mineshaft", Dir.South);
            b.Chest(x + 3, y + 1, z + 3, "mineshaft", Dir.North);
            b.Barrel(x + 3, y + 1, z - 3, "mineshaft");
            b.Set(x - 3, y + 1, z + 3, "crafting_table");
            b.Set(x + 1, y + 1, z, "rail", 1);
            b.Set(x - 1, y + 1, z, "rail", 1);
            // cobweb drapes
            for (int i = 0; i < 26; i++)
            {
                int cx = x + (int)(b.R(i, 0, 0, 21) * (R * 2 + 1)) - R;
                int cz = z + (int)(b.R(0, 0, i, 22) * (R * 2 + 1)) - R;
                int cy = y + 2 + (int)(b.R(i, 1, i, 23) * (H - 1));
                if (b.IsAir(cx, cy, cz)) b.Set(cx, cy, cz, "cobweb");
            }
            b.Mob(x, y + 1, z, "cave_spider");
        }
    }

    // =====================================================================================================
    //  Dungeon: one mossy cobblestone room with chests and a spawner (small and common).
    // =====================================================================================================
    public sealed class DungeonStructure : StructureBase
    {
        public DungeonStructure() { id = "dungeon"; spacing = 18; separation = 4; salt = 30029; maxRadiusChunks = 2; }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
        {
            if (!(g is OverworldGenerator)) return false;
            return rng.Chance(0.55f);
        }

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            var st = NewStart(x, rng.Range(-56, 38), z);
            string[] spawns = { "zombie", "skeleton", "spider" };
            var room = new DungeonRoom
            {
                x = x, y = st.y, z = z,
                sx = rng.Range(5, 8), sz = rng.Range(5, 8),
                mob = spawns[rng.Next(spawns.Length)],
                seed = NextSeed(ref rng), gen = g
            };
            room.ComputeBox();
            st.pieces.Add(room);
            return Finish(st, cx, cz);
        }
    }

    public sealed class DungeonRoom : StructurePiece
    {
        public WorldGenerator gen;
        public int x, y, z, sx, sz;
        public string mob;
        const int H = 3;

        public void ComputeBox() => box = new BBox(x - sx / 2 - 2, y - 3, z - sz / 2 - 2, x + sx / 2 + 3, y + H + 4, z + sz / 2 + 3);

        public override void Build(StructureWriter w)
        {
            var b = new SB(w, seed).World();
            int x0 = x - sx / 2, x1 = x + sx - 1 - sx / 2;
            int z0 = z - sz / 2, z1 = z + sz - 1 - sz / 2;
            var r = new RNG(seed);

            // shell, then hollow it out (replacing whatever cave rock was there)
            b.Fill3(x0, y, z0, x1, y + H, z1, "mossy_cobblestone", "cobblestone", 0.35f, null, 0f, 4);
            b.Clear(x0 + 1, y + 1, z0 + 1, x1 - 1, y + H - 1, z1 - 1);
            b.Fill(x0, y + 1, z0, x0, y + H - 1, z1, "mossy_cobblestone");
            b.Fill(x1, y + 1, z0, x1, y + H - 1, z1, "mossy_cobblestone");
            b.Fill(x0, y + 1, z0, x1, y + H - 1, z0, "mossy_cobblestone");
            b.Fill(x0, y + 1, z1, x1, y + H - 1, z1, "mossy_cobblestone");

            // 1x2 openings, each on a random offset along one wall
            for (int i = 0; i < 4; i++)
            {
                if (!r.Chance(0.5f)) continue;
                Dir d = DirUtil.Horizontal[i];
                int cx, cz;
                if (d == Dir.West) { cx = x0; cz = z0 + 1 + r.Next(Math.Max(1, sz - 2)); }
                else if (d == Dir.East) { cx = x1; cz = z0 + 1 + r.Next(Math.Max(1, sz - 2)); }
                else if (d == Dir.South) { cz = z0; cx = x0 + 1 + r.Next(Math.Max(1, sx - 2)); }
                else { cz = z1; cx = x0 + 1 + r.Next(Math.Max(1, sx - 2)); }
                b.Clear(cx, y + 1, cz, cx, y + 2, cz);
            }

            b.Spawner(x, y + 1, z, mob);
            int n = r.Range(1, 2);
            for (int i = 0; i < n; i++)
            {
                if (r.NextBool())
                {
                    int cx = r.NextBool() ? x0 + 1 : x1 - 1;
                    b.Chest(cx, y + 1, z0 + 1 + r.Next(Math.Max(1, sz - 2)), "dungeon", r.NextBool() ? Dir.East : Dir.West);
                }
                else
                {
                    int cz = r.NextBool() ? z0 + 1 : z1 - 1;
                    b.Chest(x0 + 1 + r.Next(Math.Max(1, sx - 2)), y + 1, cz, "dungeon", r.NextBool() ? Dir.North : Dir.South);
                }
            }
            // no torches: the spawner needs the dark
            for (int i = 0; i < 2; i++) b.Mob(x0 + 1 + r.Next(Math.Max(1, sx - 2)), y + 1, z0 + 1 + r.Next(Math.Max(1, sz - 2)), mob);
        }
    }
}
