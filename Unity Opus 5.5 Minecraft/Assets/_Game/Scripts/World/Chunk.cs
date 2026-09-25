using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using UnityEngine;

namespace MCR
{
    /// <summary>16x16x16 block section. states == null means all air. light == null means uniform (sky 15, block 0).</summary>
    public sealed class ChunkSection
    {
        public ushort[] states;
        public byte[] light;
        public int nonAir;
        public bool IsEmpty => states == null || nonAir == 0;

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static int Idx(int x, int y, int z) => (y << 8) | (z << 4) | x;
    }

    public enum ChunkStage : byte { Empty = 0, Terrain = 1, Decorated = 2, Final = 3, Lit = 4 }

    public sealed class Chunk
    {
        public readonly int cx, cz;
        public readonly World world;
        public readonly ChunkSection[] sections;
        public readonly int minY;
        public readonly int sectionCount;
        /// <summary>Biome id per column (x + z*16), surface biome.</summary>
        public readonly byte[] biomes2D = new byte[256];
        /// <summary>3D biomes at 4x4x4 resolution: index (qy*16 + qz*4 + qx). Used for cave biomes.</summary>
        public byte[] biomes3D;
        /// <summary>Y of highest light-blocking block + 1 per column (sky light begins here).</summary>
        public readonly short[] skyHeight = new short[256];
        /// <summary>Y of highest motion-blocking (solid or liquid) block per column.</summary>
        public readonly short[] surfaceHeight = new short[256];
        /// <summary>Immutable generation info (set in terrain phase, never changed): top solid block y and state, water depth above it.</summary>
        public readonly short[] genTopY = new short[256];
        public readonly ushort[] genTop = new ushort[256];
        public readonly byte[] genWater = new byte[256];

        public volatile int stage;            // ChunkStage
        public volatile bool decorated;        // own decoration pass done
        public volatile bool lit;
        public volatile int busy;              // in-flight worker tasks referencing this chunk
        public bool fromSave;
        public bool unloadRequested;

        // rendering
        public readonly bool[] sectionDirty;
        public readonly bool[] sectionBuilt;
        public bool anyDirty;
        public bool meshedOnce;
        public int meshVersion;

        // persistence: modifications relative to generation (local index packed: y offset from minY * 256 + z*16 + x)
        public readonly Dictionary<int, ushort> mods = new Dictionary<int, ushort>();
        public bool modsDirty;
        public bool loadedMods;
        public Dictionary<int, ushort> pendingMods; // from save, applied once chunk is final

        public readonly Dictionary<int, BlockEntity> blockEntities = new Dictionary<int, BlockEntity>();
        /// <summary>Entities queued by world generation (structure mobs), spawned when the chunk becomes final.</summary>
        public List<SavedEntity> pendingEntities;
        /// <summary>Saved record for this chunk (from disk), applied when the chunk becomes final.</summary>
        public ChunkRecord saveRecord;
        /// <summary>Entities collected when the chunk unloads (for persistence).</summary>
        public List<SavedEntity> unloadEntities;
        public bool entitiesSpawned;

        // generation scratch: structure starts etc.
        public float inhabitedTime;

        public Chunk(World w, int cx, int cz)
        {
            world = w; this.cx = cx; this.cz = cz;
            minY = w.minY;
            sectionCount = w.sectionCount;
            sections = new ChunkSection[sectionCount];
            for (int i = 0; i < sectionCount; i++) sections[i] = new ChunkSection();
            sectionDirty = new bool[sectionCount];
            sectionBuilt = new bool[sectionCount];
        }

        public int WorldX0 => cx << 4;
        public int WorldZ0 => cz << 4;
        public long Key => World.ChunkKey(cx, cz);

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public ushort Get(int lx, int y, int lz)
        {
            int sy = (y - minY) >> 4;
            if ((uint)sy >= (uint)sectionCount) return 0;
            var s = sections[sy].states;
            if (s == null) return 0;
            return s[(((y - minY) & 15) << 8) | (lz << 4) | lx];
        }

        /// <summary>Raw set without any side effects. Returns previous state.</summary>
        public ushort SetRaw(int lx, int y, int lz, ushort state)
        {
            int sy = (y - minY) >> 4;
            if ((uint)sy >= (uint)sectionCount) return 0;
            var sec = sections[sy];
            if (sec.states == null)
            {
                if (state == 0) return 0;
                sec.states = new ushort[4096];
            }
            int i = (((y - minY) & 15) << 8) | (lz << 4) | lx;
            ushort old = sec.states[i];
            if (old == state) return old;
            sec.states[i] = state;
            if (old == 0) sec.nonAir++;
            else if (state == 0) sec.nonAir--;
            return old;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public byte GetLight(int lx, int y, int lz)
        {
            int sy = (y - minY) >> 4;
            if (sy < 0) return 0;
            if (sy >= sectionCount) return world.hasSkyLight ? (byte)0xF0 : (byte)0;
            var l = sections[sy].light;
            if (l == null) return world.hasSkyLight ? (byte)0xF0 : (byte)0;
            return l[(((y - minY) & 15) << 8) | (lz << 4) | lx];
        }

        public void SetLight(int lx, int y, int lz, byte v)
        {
            int sy = (y - minY) >> 4;
            if ((uint)sy >= (uint)sectionCount) return;
            var sec = sections[sy];
            if (sec.light == null)
            {
                byte def = world.hasSkyLight ? (byte)0xF0 : (byte)0;
                if (v == def) return;
                sec.light = new byte[4096];
                if (def != 0) for (int i = 0; i < 4096; i++) sec.light[i] = def;
            }
            sec.light[(((y - minY) & 15) << 8) | (lz << 4) | lx] = v;
        }

        public int Biome(int lx, int lz) => biomes2D[(lz << 4) | lx];

        public int Biome3D(int lx, int y, int lz)
        {
            if (biomes3D == null) return biomes2D[(lz << 4) | lx];
            int qy = (y - minY) >> 2;
            int qmax = (world.height >> 2) - 1;
            if (qy < 0) qy = 0; if (qy > qmax) qy = qmax;
            return biomes3D[(qy << 4) | ((lz >> 2) << 2) | (lx >> 2)];
        }

        public void RecomputeHeights()
        {
            for (int lz = 0; lz < 16; lz++)
                for (int lx = 0; lx < 16; lx++)
                    RecomputeHeight(lx, lz);
        }

        public void RecomputeHeight(int lx, int lz)
        {
            int top = minY + sectionCount * 16 - 1;
            short sky = (short)minY, surf = (short)(minY - 1);
            bool gotSky = false, gotSurf = false;
            for (int sy = sectionCount - 1; sy >= 0 && !(gotSky && gotSurf); sy--)
            {
                var st = sections[sy].states;
                if (st == null || sections[sy].nonAir == 0) continue;
                for (int ly = 15; ly >= 0; ly--)
                {
                    ushort s = st[(ly << 8) | (lz << 4) | lx];
                    if (s == 0) continue;
                    int y = minY + sy * 16 + ly;
                    if (!gotSky && Blocks.StateOpacity[s] > 0) { sky = (short)(y + 1); gotSky = true; }
                    if (!gotSurf)
                    {
                        var b = Blocks.ByState[s];
                        if (b.solid || b.isLiquid) { surf = (short)y; gotSurf = true; }
                    }
                    if (gotSky && gotSurf) break;
                }
            }
            skyHeight[(lz << 4) | lx] = gotSky ? sky : (short)minY;
            surfaceHeight[(lz << 4) | lx] = gotSurf ? surf : (short)(minY - 1);
        }

        public static int ModIndex(int lx, int yOff, int lz) => (yOff << 8) | (lz << 4) | lx;

        public void RecordMod(int lx, int y, int lz, ushort state)
        {
            mods[ModIndex(lx, y - minY, lz)] = state;
            modsDirty = true;
        }

        public void MarkSectionDirty(int sy)
        {
            if ((uint)sy >= (uint)sectionCount) return;
            sectionDirty[sy] = true; anyDirty = true;
        }

        public void MarkAllDirty()
        {
            for (int i = 0; i < sectionCount; i++) sectionDirty[i] = true;
            anyDirty = true;
        }

        public BlockEntity GetBE(int lx, int y, int lz)
        {
            blockEntities.TryGetValue(ModIndex(lx, y - minY, lz), out var be);
            return be;
        }
    }

    /// <summary>Serialized entity waiting for its chunk.</summary>
    [Serializable]
    public class SavedEntity
    {
        public string type;
        public float x, y, z, yaw, pitch;
        public string data;
    }
}
