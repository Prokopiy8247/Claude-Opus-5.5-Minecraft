using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Axis-aligned integer box (inclusive min, exclusive max).</summary>
    public struct BBox
    {
        public int x0, y0, z0, x1, y1, z1;
        public BBox(int x0, int y0, int z0, int x1, int y1, int z1) { this.x0 = x0; this.y0 = y0; this.z0 = z0; this.x1 = x1; this.y1 = y1; this.z1 = z1; }
        public bool IntersectsXZ(int cx0, int cz0, int cx1, int cz1) => x0 < cx1 && x1 > cx0 && z0 < cz1 && z1 > cz0;
        public bool Intersects(BBox o) => x0 < o.x1 && x1 > o.x0 && y0 < o.y1 && y1 > o.y0 && z0 < o.z1 && z1 > o.z0;
        public BBox Grow(int g) => new BBox(x0 - g, y0 - g, z0 - g, x1 + g, y1 + g, z1 + g);
        public int CenterX => (x0 + x1) / 2;
        public int CenterZ => (z0 + z1) / 2;
    }

    /// <summary>A structure piece: deterministic builder writing blocks through a clipped writer.</summary>
    public abstract class StructurePiece
    {
        public BBox box;
        public int seed;
        public abstract void Build(StructureWriter w);
    }

    public sealed class StructureStart
    {
        public string type;
        public int cx, cz;          // origin chunk
        public int x, y, z;         // origin block
        public BBox bounds;
        public readonly List<StructurePiece> pieces = new List<StructurePiece>();
        public int seed;
        public DimensionId dim;
    }

    /// <summary>Helper for building structure pieces with a local rotated frame and clipping.</summary>
    public sealed class StructureWriter
    {
        public ChunkWriter cw;
        public WorldGenerator gen;
        public RNG rng;
        public int ox, oy, oz;  // origin
        public int rot;         // quarter turns
        public int sizeX, sizeZ;
        public StructureWriter(ChunkWriter cw, WorldGenerator gen) { this.cw = cw; this.gen = gen; }

        public void Frame(int x, int y, int z, int rot, int sx, int sz) { ox = x; oy = y; oz = z; this.rot = rot & 3; sizeX = sx; sizeZ = sz; }

        /// <summary>Local (lx, lz) to world coordinates considering rotation within a footprint of sizeX x sizeZ.</summary>
        public void ToWorld(int lx, int lz, out int wx, out int wz)
        {
            switch (rot)
            {
                case 1: wx = ox + (sizeZ - 1 - lz); wz = oz + lx; break;
                case 2: wx = ox + (sizeX - 1 - lx); wz = oz + (sizeZ - 1 - lz); break;
                case 3: wx = ox + lz; wz = oz + (sizeX - 1 - lx); break;
                default: wx = ox + lx; wz = oz + lz; break;
            }
        }
        public Dir RotDir(Dir d) => DirUtil.IsHorizontal(d) ? DirUtil.FromHorizIndex(DirUtil.HorizIndex(d) + rot) : d;
        public int RotFacingMeta(Block b, int meta)
        {
            // rotate low two facing bits for horizontal-facing blocks
            if (b is StairsBlock || b is DoorBlock || b is TrapdoorBlock || b is FenceGateBlock || b is HorizontalBlock || b is LadderBlock || b is ChestBlock || b is FurnaceBlock || b is BedBlock || b is AnvilBlock || b is LecternBlock || b is RepeaterBlock || b is ComparatorBlock || b is CampfireBlock || b is BellBlock || b is EndPortalFrameBlock || b is GrindstoneBlock || b is StonecutterBlock || b is LoomBlock)
                return (meta & ~3) | (((meta & 3) + rot) & 3);
            if (b is TorchBlock && (meta & 7) > 0) return (meta & 8) | (1 + (((meta & 7) - 1 + rot) & 3));
            if (b is PillarBlock && (rot & 1) == 1 && meta > 0) return meta == 1 ? 2 : 1;
            return meta;
        }

        public ushort Get(int lx, int y, int lz) { ToWorld(lx, lz, out int wx, out int wz); return cw.Get(wx, oy + y, wz); }
        public void Set(int lx, int y, int lz, ushort s)
        {
            ToWorld(lx, lz, out int wx, out int wz);
            if (!cw.CanWrite(wx, oy + y, wz)) return;
            if (s != 0 && rot != 0) { var b = Blocks.ByState[s]; s = b.State(RotFacingMeta(b, s - b.baseState)); }
            cw.Set(wx, oy + y, wz, s);
        }
        public void Set(int lx, int y, int lz, string id, int meta = -1)
        {
            var b = Blocks.Get(id);
            if (b == null) return;
            Set(lx, y, lz, meta < 0 ? b.DefaultState : b.State(meta));
        }
        public void Fill(int x0, int y0, int z0, int x1, int y1, int z1, ushort s)
        {
            for (int y = y0; y <= y1; y++) for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) Set(x, y, z, s);
        }
        public void Fill(int x0, int y0, int z0, int x1, int y1, int z1, string id, int meta = -1)
        {
            var b = Blocks.Get(id); if (b == null) return;
            Fill(x0, y0, z0, x1, y1, z1, meta < 0 ? b.DefaultState : b.State(meta));
        }
        public void Hollow(int x0, int y0, int z0, int x1, int y1, int z1, string wall, bool clearInside = true)
        {
            var b = Blocks.Get(wall); ushort ws = b.DefaultState;
            for (int y = y0; y <= y1; y++) for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++)
                    {
                        bool edge = x == x0 || x == x1 || y == y0 || y == y1 || z == z0 || z == z1;
                        if (edge) Set(x, y, z, ws); else if (clearInside) Set(x, y, z, 0);
                    }
        }
        /// <summary>Fill downward from y-1 until solid ground (foundation).</summary>
        public void Foundation(int lx, int lz, int fromY, string id, int maxDepth = 24)
        {
            var b = Blocks.Get(id); if (b == null) return;
            for (int y = fromY - 1; y > fromY - 1 - maxDepth; y--)
            {
                ushort cur = Get(lx, y, lz);
                var cb = Blocks.ByState[cur];
                if (cur != 0 && !cb.isLiquid && !cb.replaceable && !(cb is LeavesBlock) && !(cb is PlantBlock)) break;
                Set(lx, y, lz, b.DefaultState);
            }
        }
        public void ClearAbove(int lx, int lz, int fromY, int height)
        {
            for (int y = fromY; y < fromY + height; y++) Set(lx, y, lz, 0);
        }
        public void Chest(int lx, int y, int lz, string lootTable, Dir facing = Dir.North, string block = "chest")
        {
            ToWorld(lx, lz, out int wx, out int wz);
            if (!cw.CanWrite(wx, oy + y, wz)) return;
            var b = Blocks.Get(block) ?? Blocks.Get("chest");
            Set(lx, y, lz, b.State(DirUtil.HorizIndex(facing)));
            var be = b.CreateBlockEntity(null, new Int3(wx, oy + y, wz));
            if (be is IContainerEntity ce) ce.LootTable = lootTable;
            if (be != null) cw.AddBlockEntity(wx, oy + y, wz, be);
        }
        public void Barrel(int lx, int y, int lz, string lootTable) => Chest(lx, y, lz, lootTable, Dir.Up, "barrel");
        public void Spawner(int lx, int y, int lz, string mob)
        {
            ToWorld(lx, lz, out int wx, out int wz);
            if (!cw.CanWrite(wx, oy + y, wz)) return;
            var b = Blocks.Get("spawner"); if (b == null) return;
            Set(lx, y, lz, b.DefaultState);
            var be = new SpawnerEntity { mob = mob };
            cw.AddBlockEntity(wx, oy + y, wz, be);
        }
        public void Mob(int lx, int y, int lz, string mob, string data = null)
        {
            ToWorld(lx, lz, out int wx, out int wz);
            if (!cw.CanWrite(wx, oy + y, wz)) return;
            cw.QueueEntity(new SavedEntity { type = mob, x = wx + 0.5f, y = oy + y, z = wz + 0.5f, data = data });
        }
        public bool Owns(int lx, int y, int lz) { ToWorld(lx, lz, out int wx, out int wz); return cw.CanWrite(wx, oy + y, wz); }
    }

    public abstract class StructureType
    {
        public string id;
        public int spacing = 32, separation = 8, salt;
        public DimensionId dim = DimensionId.Overworld;
        public int maxRadiusChunks = 4; // how far pieces can extend from the origin chunk
        public abstract bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng);
        public abstract StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng);
    }

    public static partial class StructureManager
    {
        public static readonly List<StructureType> Types = new List<StructureType>();
        static bool inited;
        static readonly ConcurrentDictionary<long, StructureStart> cache = new ConcurrentDictionary<long, StructureStart>();
        static readonly ConcurrentDictionary<long, bool> noStart = new ConcurrentDictionary<long, bool>();

        public static void Init()
        {
            if (inited) return; inited = true;
            RegisterTypes();
        }
        static partial void RegisterTypesImpl(List<StructureType> list);
        static void RegisterTypes() { RegisterTypesImpl(Types); }

        public static void ClearCache() { cache.Clear(); noStart.Clear(); }

        static long Key(StructureType t, int rx, int rz, int seed) => ((long)(rx & 0xFFFFF) << 40) ^ ((long)(rz & 0xFFFFF) << 16) ^ (long)(t.salt & 0xFFFF) ^ ((long)seed << 20);

        /// <summary>Origin chunk for the structure in region (rx, rz) (MC-like random spread placement).</summary>
        public static void RegionOrigin(StructureType t, int seed, int rx, int rz, out int cx, out int cz)
        {
            var rng = new RNG(seed, rx, rz, t.salt);
            int range = t.spacing - t.separation;
            cx = rx * t.spacing + rng.Next(range);
            cz = rz * t.spacing + rng.Next(range);
        }

        public static StructureStart GetStart(WorldGenerator g, StructureType t, int rx, int rz)
        {
            long k = Key(t, rx, rz, g.seed) ^ ((long)g.world.dim << 60);
            if (cache.TryGetValue(k, out var s)) return s;
            if (noStart.ContainsKey(k)) return null;
            RegionOrigin(t, g.seed, rx, rz, out int cx, out int cz);
            var rng = new RNG(g.seed, cx, cz, t.salt * 7 + 13);
            StructureStart st = null;
            int bx = (cx << 4) + 8, bz = (cz << 4) + 8;
            try
            {
                if (t.CanSpawn(g, bx, bz, ref rng)) st = t.Create(g, cx, cz, ref rng);
            }
            catch (Exception e) { Debug.LogError("Structure " + t.id + " failed: " + e); st = null; }
            if (st != null) { st.type = t.id; st.cx = cx; st.cz = cz; st.dim = g.world.dim; cache[k] = st; }
            else noStart[k] = true;
            return st;
        }

        public static List<StructureStart> StartsNear(WorldGenerator g, int cx, int cz)
        {
            Init();
            var list = new List<StructureStart>();
            foreach (var t in Types)
            {
                if (t.dim != g.world.dim) continue;
                int r = t.maxRadiusChunks;
                int rx0 = MathX.FloorDiv(cx - r, t.spacing), rx1 = MathX.FloorDiv(cx + r, t.spacing);
                int rz0 = MathX.FloorDiv(cz - r, t.spacing), rz1 = MathX.FloorDiv(cz + r, t.spacing);
                for (int rx = rx0; rx <= rx1; rx++)
                    for (int rz = rz0; rz <= rz1; rz++)
                    {
                        var s = GetStart(g, t, rx, rz);
                        if (s == null) continue;
                        if (s.bounds.IntersectsXZ(cx << 4, cz << 4, (cx << 4) + 16, (cz << 4) + 16)) list.Add(s);
                    }
            }
            return list;
        }

        public static void PlaceInChunk(WorldGenerator g, Chunk c, ChunkWriter cw)
        {
            var starts = g.StructuresNear(c.cx, c.cz);
            if (starts == null || starts.Count == 0) return;
            int x0 = c.cx << 4, z0 = c.cz << 4;
            var sw = new StructureWriter(cw, g);
            foreach (var s in starts)
                foreach (var p in s.pieces)
                {
                    if (!p.box.IntersectsXZ(x0, z0, x0 + 16, z0 + 16)) continue;
                    sw.rng = new RNG(p.seed);
                    try { p.Build(sw); }
                    catch (Exception e) { Debug.LogWarning("Piece build failed in " + s.type + ": " + e.Message); }
                }
        }

        /// <summary>Find the nearest structure of a type from a block position (search in rings of regions).</summary>
        public static StructureStart Locate(WorldGenerator g, string typeId, int x, int z, int maxRegions = 40)
        {
            Init();
            StructureType t = Types.Find(tt => tt.id == typeId && tt.dim == g.world.dim);
            if (t == null) return null;
            int cx = x >> 4, cz = z >> 4;
            int rx = MathX.FloorDiv(cx, t.spacing), rz = MathX.FloorDiv(cz, t.spacing);
            StructureStart best = null; long bestD = long.MaxValue;
            for (int r = 0; r <= maxRegions; r++)
            {
                for (int dx = -r; dx <= r; dx++)
                    for (int dz = -r; dz <= r; dz++)
                    {
                        if (Math.Abs(dx) != r && Math.Abs(dz) != r) continue;
                        var s = GetStart(g, t, rx + dx, rz + dz);
                        if (s == null) continue;
                        long ddx = s.x - x, ddz = s.z - z;
                        long d = ddx * ddx + ddz * ddz;
                        if (d < bestD) { bestD = d; best = s; }
                    }
                if (best != null && r >= 1) break;
            }
            return best;
        }

        public static IEnumerable<string> TypeIds(DimensionId dim)
        {
            Init();
            foreach (var t in Types) if (t.dim == dim) yield return t.id;
        }
    }
}
