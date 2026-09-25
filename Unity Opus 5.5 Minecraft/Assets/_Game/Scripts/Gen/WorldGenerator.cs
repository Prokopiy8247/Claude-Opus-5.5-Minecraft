using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public interface IBlockAccess
    {
        ushort Get(int x, int y, int z);
        void Set(int x, int y, int z, ushort s);
        bool CanWrite(int x, int y, int z);
        int MinY { get; }
        int MaxY { get; }
    }

    /// <summary>Writes into a single chunk during generation (world coordinates, clipped to the chunk).</summary>
    public sealed class ChunkWriter : IBlockAccess
    {
        public Chunk c; int x0, z0;
        public ChunkWriter(Chunk c) { this.c = c; x0 = c.cx << 4; z0 = c.cz << 4; }
        public int MinY => c.minY;
        public int MaxY => c.minY + c.sectionCount * 16;
        public bool CanWrite(int x, int y, int z) => x >= x0 && x < x0 + 16 && z >= z0 && z < z0 + 16 && y >= MinY && y < MaxY;
        public ushort Get(int x, int y, int z) => CanWrite(x, y, z) ? c.Get(x - x0, y, z - z0) : (ushort)0;
        public void Set(int x, int y, int z, ushort s) { if (CanWrite(x, y, z)) c.SetRaw(x - x0, y, z - z0, s); }
        public void AddBlockEntity(int x, int y, int z, BlockEntity be)
        {
            if (!CanWrite(x, y, z)) return;
            be.pos = new Int3(x, y, z);
            c.blockEntities[Chunk.ModIndex(x - x0, y - c.minY, z - z0)] = be;
        }
        public void QueueEntity(SavedEntity e)
        {
            if (!CanWrite(Mathf.FloorToInt(e.x), c.minY, Mathf.FloorToInt(e.z))) return;
            if (c.pendingEntities == null) c.pendingEntities = new List<SavedEntity>();
            c.pendingEntities.Add(e);
        }
    }

    /// <summary>Writes into the live world (saplings, bone meal, commands).</summary>
    public sealed class WorldAccess : IBlockAccess
    {
        public World w;
        public WorldAccess(World w) { this.w = w; }
        public int MinY => w.minY;
        public int MaxY => w.maxY;
        public bool CanWrite(int x, int y, int z) => y >= w.minY && y < w.maxY && w.IsLoaded(x, z);
        public ushort Get(int x, int y, int z) => w.GetState(x, y, z);
        public void Set(int x, int y, int z, ushort s) { if (CanWrite(x, y, z)) w.SetState(new Int3(x, y, z), s, SetFlags.Hooks | SetFlags.Notify); }
    }

    public abstract class WorldGenerator
    {
        public readonly World world;
        public readonly int seed;
        protected WorldGenerator(World w) { world = w; seed = w.seed; }

        /// <summary>Phase A: terrain, caves, surface. Writes only into c. Must set c.genTopY/genTop.</summary>
        public abstract void GenerateTerrain(Chunk c);
        /// <summary>Phase B: features and structures (reads neighbour generation info only).</summary>
        public abstract void Decorate(Chunk c, Chunk[] neighbours);
        public abstract Vector3 FindSpawn();
        public virtual int ApproxSurface(int x, int z) => world.seaLevel;
        public virtual string BiomeAtApprox(int x, int z) => "plains";
        public virtual List<StructureStart> StructuresNear(int cx, int cz) => StructureManager.StartsNear(this, cx, cz);

        protected static ushort S(string id) => Blocks.StateOf(id);
    }
}
