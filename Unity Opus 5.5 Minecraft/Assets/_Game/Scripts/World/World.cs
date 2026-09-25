using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using UnityEngine;

namespace MCR
{
    public enum DimensionId { Overworld = 0, Nether = 1, End = 2 }

    public static class SetFlags
    {
        public const int Notify = 1;       // notify neighbours (OnNeighborChanged)
        public const int Hooks = 2;        // call OnRemoved / OnAdded
        public const int NoRecord = 4;     // don't record as a modification (world generation)
        public const int NoLight = 8;
        public const int Default = Notify | Hooks;
    }

    /// <summary>One dimension: owns chunks, entities, scheduled ticks and block access.</summary>
    public sealed partial class World
    {
        public readonly DimensionId dim;
        public readonly int minY, height, maxY, sectionCount;
        public readonly bool hasSkyLight;
        public readonly bool hasCeiling;
        public readonly int seed;
        public readonly WorldGenerator generator;
        public readonly ConcurrentDictionary<long, Chunk> chunks = new ConcurrentDictionary<long, Chunk>();
        public readonly List<Entity> entities = new List<Entity>();
        public readonly List<Entity> entitiesToAdd = new List<Entity>();
        public RNG rand;
        public long tickCount;
        public GameSession session;
        public readonly int seaLevel;

        public World(DimensionId dim, int seed, GameSession session)
        {
            this.dim = dim; this.seed = seed; this.session = session;
            switch (dim)
            {
                case DimensionId.Nether: minY = 0; height = 128; hasSkyLight = false; hasCeiling = true; seaLevel = 31; break;
                case DimensionId.End: minY = 0; height = 256; hasSkyLight = false; hasCeiling = false; seaLevel = 0; break;
                default: minY = -64; height = 384; hasSkyLight = true; hasCeiling = false; seaLevel = 63; break;
            }
            maxY = minY + height;
            sectionCount = height / 16;
            rand = new RNG(seed * 31 + (int)dim);
            switch (dim)
            {
                case DimensionId.Nether: generator = new NetherGenerator(this); break;
                case DimensionId.End: generator = new EndGenerator(this); break;
                default: generator = new OverworldGenerator(this); break;
            }
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static long ChunkKey(int cx, int cz) => ((long)cx << 32) ^ (uint)cz;

        public Chunk GetChunk(int cx, int cz)
        {
            chunks.TryGetValue(ChunkKey(cx, cz), out var c);
            return c;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public Chunk ChunkAtBlock(int x, int z) => GetChunk(x >> 4, z >> 4);

        /// <summary>Chunk exists and has final block data.</summary>
        public Chunk ReadyChunk(int x, int z)
        {
            var c = GetChunk(x >> 4, z >> 4);
            return c != null && c.stage >= (int)ChunkStage.Final ? c : null;
        }

        public bool IsLoaded(int x, int z) => ReadyChunk(x, z) != null;
        public bool IsLoaded(Int3 p) => ReadyChunk(p.x, p.z) != null;

        // single-entry cache for main-thread access
        Chunk cacheChunk; int cacheCx = int.MinValue, cacheCz = int.MinValue;

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        Chunk CachedChunk(int x, int z)
        {
            int cx = x >> 4, cz = z >> 4;
            if (cx == cacheCx && cz == cacheCz && cacheChunk != null && cacheChunk.stage >= (int)ChunkStage.Final) return cacheChunk;
            var c = GetChunk(cx, cz);
            if (c == null || c.stage < (int)ChunkStage.Final) return null;
            cacheChunk = c; cacheCx = cx; cacheCz = cz;
            return c;
        }

        public void InvalidateCache() { cacheChunk = null; cacheCx = int.MinValue; }

        public ushort GetState(int x, int y, int z)
        {
            if (y < minY || y >= maxY) return 0;
            var c = CachedChunk(x, z);
            if (c == null) return 0;
            return c.Get(x & 15, y, z & 15);
        }
        public ushort GetState(Int3 p) => GetState(p.x, p.y, p.z);
        public Block GetBlock(int x, int y, int z) => Blocks.ByState[GetState(x, y, z)];
        public Block GetBlock(Int3 p) => Blocks.ByState[GetState(p.x, p.y, p.z)];
        public int GetMeta(Int3 p) { ushort s = GetState(p); return s - Blocks.ByState[s].baseState; }
        public bool IsAir(Int3 p) => Blocks.StateAir[GetState(p)];

        /// <summary>Unloaded positions count as solid (so entities don't fall into ungenerated chunks).</summary>
        public bool IsUnloadedAt(int x, int y, int z) => y >= minY && y < maxY && CachedChunk(x, z) == null;

        public byte GetLightRaw(int x, int y, int z)
        {
            if (y >= maxY) return hasSkyLight ? (byte)0xF0 : (byte)0;
            if (y < minY) return 0;
            var c = CachedChunk(x, z);
            if (c == null) return hasSkyLight ? (byte)0xF0 : (byte)0;
            return c.GetLight(x & 15, y, z & 15);
        }
        public int GetSkyLight(Int3 p) => GetLightRaw(p.x, p.y, p.z) >> 4;
        public int GetBlockLight(Int3 p) => GetLightRaw(p.x, p.y, p.z) & 15;
        /// <summary>Effective light level 0..15 considering time of day.</summary>
        public int GetLightLevel(Int3 p)
        {
            byte l = GetLightRaw(p.x, p.y, p.z);
            int sky = (l >> 4) - (session != null ? session.SkyDarken(this) : 0);
            return Mathf.Max(sky, l & 15);
        }
        /// <summary>0..1 brightness for entity rendering.</summary>
        public float GetBrightness(Vector3 pos)
        {
            Int3 p = Int3.Floor(pos);
            byte l = GetLightRaw(p.x, p.y, p.z);
            float daylight = session != null ? session.DaylightFactor(this) : 1f;
            float sky = (l >> 4) / 15f * daylight;
            float blk = (l & 15) / 15f;
            return Mathf.Max(sky, blk);
        }

        public int TopSurfaceY(int x, int z)
        {
            var c = CachedChunk(x, z);
            if (c == null) return seaLevel;
            return c.surfaceHeight[((z & 15) << 4) | (x & 15)];
        }
        public int SkyHeight(int x, int z)
        {
            var c = CachedChunk(x, z);
            if (c == null) return minY;
            return c.skyHeight[((z & 15) << 4) | (x & 15)];
        }
        public bool CanSeeSky(Int3 p) => hasSkyLight && p.y >= SkyHeight(p.x, p.z);

        public int GetBiomeId(int x, int y, int z)
        {
            var c = CachedChunk(x, z);
            if (c == null) return 0;
            return c.Biome3D(x & 15, Mathf.Clamp(y, minY, maxY - 1), z & 15);
        }
        public Biome GetBiome(Int3 p) => Biome.Get(GetBiomeId(p.x, p.y, p.z));
        public Biome GetSurfaceBiome(int x, int z)
        {
            var c = CachedChunk(x, z);
            return Biome.Get(c == null ? 0 : c.Biome(x & 15, z & 15));
        }

        public bool IsRainingAt(Int3 p)
        {
            if (session == null || !session.IsRaining || dim != DimensionId.Overworld) return false;
            if (!CanSeeSky(p)) return false;
            var b = GetBiome(p);
            return b.precipitation != Precipitation.None;
        }

        // ------------------------------------------------------------------ block changes
        public bool SetState(Int3 p, ushort state, int flags = SetFlags.Default)
        {
            if (p.y < minY || p.y >= maxY) return false;
            var c = CachedChunk(p.x, p.z);
            if (c == null) return false;
            int lx = p.x & 15, lz = p.z & 15;
            ushort old = c.Get(lx, p.y, lz);
            if (old == state) return false;
            Block oldB = Blocks.ByState[old], newB = Blocks.ByState[state];
            if ((flags & SetFlags.Hooks) != 0 && oldB != newB) oldB.OnRemoved(this, p, old - oldB.baseState, state);
            c.SetRaw(lx, p.y, lz, state);
            if ((flags & SetFlags.NoRecord) == 0) c.RecordMod(lx, p.y, lz, state);
            // block entity lifetime
            int bi = Chunk.ModIndex(lx, p.y - minY, lz);
            if (oldB != newB)
            {
                if (c.blockEntities.TryGetValue(bi, out var oldBe))
                {
                    oldBe.OnRemoved();
                    c.blockEntities.Remove(bi);
                }
                if (newB.HasBlockEntity)
                {
                    var be = newB.CreateBlockEntity(this, p);
                    if (be != null) { be.world = this; be.pos = p; c.blockEntities[bi] = be; }
                }
            }
            c.RecomputeHeight(lx, lz);
            if ((flags & SetFlags.NoLight) == 0 && c.lit)
                Lighting.OnBlockChanged(this, p, old, state);
            MarkDirtyAround(p);
            if ((flags & SetFlags.Hooks) != 0 && oldB != newB) newB.OnAdded(this, p, state - newB.baseState, old);
            if ((flags & SetFlags.Notify) != 0) NotifyNeighbors(p);
            return true;
        }

        public bool SetBlock(Int3 p, Block b, int meta = -1, int flags = SetFlags.Default)
        {
            ushort s = meta < 0 ? b.DefaultState : b.State(meta);
            return SetState(p, s, flags);
        }

        public void SetMeta(Int3 p, int meta, int flags = SetFlags.Default)
        {
            var b = GetBlock(p);
            SetState(p, b.State(meta), flags);
        }

        public void MarkDirtyAround(Int3 p)
        {
            MarkSectionDirtyAt(p.x, p.y, p.z);
            int lx = p.x & 15, lz = p.z & 15, ly = (p.y - minY) & 15;
            if (lx == 0) MarkSectionDirtyAt(p.x - 1, p.y, p.z);
            if (lx == 15) MarkSectionDirtyAt(p.x + 1, p.y, p.z);
            if (lz == 0) MarkSectionDirtyAt(p.x, p.y, p.z - 1);
            if (lz == 15) MarkSectionDirtyAt(p.x, p.y, p.z + 1);
            if (ly == 0) MarkSectionDirtyAt(p.x, p.y - 1, p.z);
            if (ly == 15) MarkSectionDirtyAt(p.x, p.y + 1, p.z);
            // diagonal neighbours matter for AO/smooth lighting at corners
            if (lx == 0 && lz == 0) MarkSectionDirtyAt(p.x - 1, p.y, p.z - 1);
            if (lx == 15 && lz == 0) MarkSectionDirtyAt(p.x + 1, p.y, p.z - 1);
            if (lx == 0 && lz == 15) MarkSectionDirtyAt(p.x - 1, p.y, p.z + 1);
            if (lx == 15 && lz == 15) MarkSectionDirtyAt(p.x + 1, p.y, p.z + 1);
        }

        public void MarkSectionDirtyAt(int x, int y, int z)
        {
            if (y < minY || y >= maxY) return;
            var c = GetChunk(x >> 4, z >> 4);
            if (c == null) return;
            c.MarkSectionDirty((y - minY) >> 4);
        }

        public void NotifyNeighbors(Int3 p)
        {
            for (int i = 0; i < 6; i++)
            {
                Int3 n = p + DirUtil.Offset[i];
                if (n.y < minY || n.y >= maxY) continue;
                ushort s = GetState(n);
                if (s == 0) continue;
                var b = Blocks.ByState[s];
                b.OnNeighborChanged(this, n, s - b.baseState, p);
            }
        }

        /// <summary>Notify the block at p itself that something near it changed.</summary>
        public void UpdateBlock(Int3 p, Int3 from)
        {
            ushort s = GetState(p);
            if (s == 0) return;
            var b = Blocks.ByState[s];
            b.OnNeighborChanged(this, p, s - b.baseState, from);
        }

        /// <summary>Break a block: particles, sound, drops (if drop), set to air.</summary>
        public void BreakBlock(Int3 p, bool drop, Entity breaker, ItemStack tool = null)
        {
            ushort s = GetState(p);
            if (s == 0) return;
            var b = Blocks.ByState[s];
            int meta = s - b.baseState;
            Particles.BlockBreak(this, p, s);
            Sounds.PlayBlock(b.sound, SoundEvent.Break, p.Center);
            if (drop) DropBlockLoot(p, b, meta, tool, breaker);
            b.OnBroken(this, p, meta, breaker);
            ushort replacement = 0;
            SetState(p, replacement);
        }

        readonly List<ItemStack> dropScratch = new List<ItemStack>();
        public void DropBlockLoot(Int3 p, Block b, int meta, ItemStack tool, Entity breaker)
        {
            dropScratch.Clear();
            if (tool != null && tool.GetEnchant(Enchant.SilkTouch) > 0 && b.item != null && Loot.SilkTouchable(b))
                dropScratch.Add(new ItemStack(b.GetPickItem(meta) ?? b.item, 1));
            else
                b.GetDrops(this, p, meta, tool, dropScratch, ref rand);
            foreach (var st in dropScratch) if (st != null && !st.IsEmpty) SpawnItem(p.Center, st);
            if (b.xpDropMax > 0 && (tool == null || tool.GetEnchant(Enchant.SilkTouch) == 0))
            {
                int xp = rand.Range(b.xpDropMin, b.xpDropMax);
                if (xp > 0) XpOrb.Spawn(this, p.Center, xp);
            }
            // container contents
            var be = GetBlockEntity(p);
            if (be != null) be.DropContents();
        }

        public ItemEntity SpawnItem(Vector3 pos, ItemStack stack, Vector3? velocity = null, int pickupDelay = 10)
        {
            if (stack == null || stack.IsEmpty) return null;
            var e = ItemEntity.Create(this, pos, stack.Copy());
            if (velocity.HasValue) e.velocity = velocity.Value;
            else e.velocity = new Vector3(rand.Range(-0.1f, 0.1f), 0.2f, rand.Range(-0.1f, 0.1f));
            e.pickupDelay = pickupDelay;
            return e;
        }

        public BlockEntity GetBlockEntity(Int3 p)
        {
            var c = CachedChunk(p.x, p.z);
            if (c == null) return null;
            c.blockEntities.TryGetValue(Chunk.ModIndex(p.x & 15, p.y - minY, p.z & 15), out var be);
            return be;
        }

        public T GetBlockEntity<T>(Int3 p) where T : BlockEntity => GetBlockEntity(p) as T;

        public void SetBlockEntity(Int3 p, BlockEntity be)
        {
            var c = CachedChunk(p.x, p.z);
            if (c == null) return;
            be.world = this; be.pos = p;
            c.blockEntities[Chunk.ModIndex(p.x & 15, p.y - minY, p.z & 15)] = be;
            c.modsDirty = true;
        }

        // ------------------------------------------------------------------ scheduled ticks
        struct Scheduled { public Int3 pos; public long due; public int blockIndex; public long seq; }
        readonly List<Scheduled> scheduled = new List<Scheduled>();
        readonly HashSet<long> scheduledKeys = new HashSet<long>();
        long schedSeq;

        public void ScheduleTick(Int3 p, Block b, int delay)
        {
            long key = p.Pack() ^ ((long)b.index << 52);
            if (!scheduledKeys.Add(key)) return;
            scheduled.Add(new Scheduled { pos = p, due = tickCount + Math.Max(1, delay), blockIndex = b.index, seq = schedSeq++ });
        }

        readonly List<Scheduled> dueNow = new List<Scheduled>();
        void RunScheduledTicks()
        {
            if (scheduled.Count == 0) return;
            dueNow.Clear();
            for (int i = scheduled.Count - 1; i >= 0; i--)
            {
                if (scheduled[i].due <= tickCount)
                {
                    dueNow.Add(scheduled[i]);
                    scheduled.RemoveAt(i);
                }
            }
            if (dueNow.Count == 0) return;
            dueNow.Sort((a, b) => a.due != b.due ? a.due.CompareTo(b.due) : a.seq.CompareTo(b.seq));
            int budget = 4096;
            foreach (var s in dueNow)
            {
                scheduledKeys.Remove(s.pos.Pack() ^ ((long)s.blockIndex << 52));
                if (budget-- <= 0) { scheduled.Add(s); scheduledKeys.Add(s.pos.Pack() ^ ((long)s.blockIndex << 52)); continue; }
                if (!IsLoaded(s.pos)) continue;
                ushort st = GetState(s.pos);
                var b = Blocks.ByState[st];
                if (b.index != s.blockIndex) continue;
                b.OnScheduledTick(this, s.pos, st - b.baseState);
            }
        }

        // ------------------------------------------------------------------ ticking
        public void Tick(Vector3 playerPos)
        {
            tickCount++;
            RunScheduledTicks();
            RandomTicks(playerPos);
            foreach (var c in chunks.Values)
            {
                if (c.stage < (int)ChunkStage.Final) continue;
                if (c.blockEntities.Count == 0) continue;
                foreach (var be in c.blockEntities.Values) if (be.ticks) be.Tick();
            }
        }

        public int randomTickSpeed = 3;
        public int simulationDistance = 8;
        void RandomTicks(Vector3 playerPos)
        {
            int pcx = Mathf.FloorToInt(playerPos.x) >> 4, pcz = Mathf.FloorToInt(playerPos.z) >> 4;
            int sd = simulationDistance;
            for (int dz = -sd; dz <= sd; dz++)
                for (int dx = -sd; dx <= sd; dx++)
                {
                    var c = GetChunk(pcx + dx, pcz + dz);
                    if (c == null || c.stage < (int)ChunkStage.Final || !c.lit) continue;
                    for (int sy = 0; sy < c.sectionCount; sy++)
                    {
                        var sec = c.sections[sy];
                        if (sec.states == null || sec.nonAir == 0) continue;
                        for (int k = 0; k < randomTickSpeed; k++)
                        {
                            int r = rand.Next(4096);
                            ushort s = sec.states[r];
                            if (s == 0) continue;
                            var b = Blocks.ByState[s];
                            if (!b.randomTicks) continue;
                            Int3 p = new Int3((c.cx << 4) + (r & 15), minY + sy * 16 + (r >> 8), (c.cz << 4) + ((r >> 4) & 15));
                            b.OnRandomTick(this, p, s - b.baseState, ref rand);
                        }
                    }
                }
        }

        // ------------------------------------------------------------------ entities
        public void AddEntity(Entity e)
        {
            e.world = this;
            entitiesToAdd.Add(e);
        }

        public void FlushNewEntities()
        {
            if (entitiesToAdd.Count == 0) return;
            foreach (var e in entitiesToAdd) { if (!e.removed) { entities.Add(e); e.OnAddedToWorld(); } }
            entitiesToAdd.Clear();
        }

        readonly List<Entity> queryScratch = new List<Entity>();
        public List<Entity> GetEntities(AABB box, Entity except = null, List<Entity> result = null)
        {
            result = result ?? queryScratch;
            result.Clear();
            foreach (var e in entities)
            {
                if (e == except || e.removed) continue;
                if (e.Bounds.Intersects(box)) result.Add(e);
            }
            return result;
        }

        public T FindNearest<T>(Vector3 pos, float radius, Func<T, bool> pred = null) where T : Entity
        {
            T best = null; float bd = radius * radius;
            foreach (var e in entities)
            {
                if (e.removed || !(e is T t)) continue;
                if (pred != null && !pred(t)) continue;
                float d = (e.position - pos).sqrMagnitude;
                if (d < bd) { bd = d; best = t; }
            }
            return best;
        }

        public int CountEntities(Func<Entity, bool> pred)
        {
            int n = 0;
            foreach (var e in entities) if (!e.removed && pred(e)) n++;
            return n;
        }

        // ------------------------------------------------------------------ helpers
        public bool IsSolidAt(Int3 p) { var b = GetBlock(p); return b.solid; }
        public bool IsSturdy(Int3 p, Dir face)
        {
            ushort s = GetState(p);
            var b = Blocks.ByState[s];
            if (!b.sturdy) return false;
            return (Blocks.StateOccludes[s] & (1 << (int)face)) != 0 || b.opaqueCube;
        }
        public bool IsWater(Int3 p) => Blocks.ByState[GetState(p)].IsWaterLike(GetMeta(p));
        public bool IsLava(Int3 p) => Blocks.IsLava(GetState(p));
    }
}
