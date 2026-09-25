using System;
using UnityEngine;

namespace MCR
{
    /// <summary>Output of a column mesh build: packed arrays for 3 render layers.</summary>
    public sealed class ColumnMeshData
    {
        public Chunk chunk; public int version; public int group;
        public ChunkVertex[] verts; public int[] indices;
        public int vcount, icount;
        public readonly int[] layerStart = new int[3]; public readonly int[] layerCount = new int[3];
        public float minY, maxY;
        public bool empty;
    }

    public static class ChunkMesher
    {
        [ThreadStatic] static MeshCtx tls;
        public static MeshCtx Ctx => tls ?? (tls = new MeshCtx());

        public const int GroupSections = 4;
        public static int GroupCount(World w) => (w.sectionCount + GroupSections - 1) / GroupSections;

        /// <summary>Build mesh group g (sections g*4 .. g*4+3) of a chunk. nb = 3x3 chunks (index (dz+1)*3 + (dx+1)).</summary>
        public static ColumnMeshData BuildColumn(World w, Chunk[] nb, int group, bool smooth = true, bool fancy = true)
        {
            var ctx = Ctx;
            ctx.world = w; ctx.smooth = smooth; ctx.fancyLeaves = fancy;
            var center = nb[4];
            var data = new ColumnMeshData { chunk = center, group = group };
            for (int l = 0; l < 3; l++) ctx.layers[l].Clear();
            FillTints(ctx, nb);
            ctx.wx0 = center.cx << 4; ctx.wz0 = center.cz << 4;
            float minY = float.MaxValue, maxY = float.MinValue;
            int sy0 = group * GroupSections, sy1 = Math.Min(center.sectionCount, sy0 + GroupSections);
            for (int sy = sy0; sy < sy1; sy++)
            {
                var sec = center.sections[sy];
                if (sec.states == null || sec.nonAir == 0) continue;
                int before = ctx.layers[0].vcount + ctx.layers[1].vcount + ctx.layers[2].vcount;
                FillPadded(ctx, w, nb, sy);
                ctx.baseY = w.minY + sy * 16;
                ctx.rng = new RNG(w.seed, center.cx * 31 + sy, center.cz, 999);
                var states = sec.states;
                for (int y = 0; y < 16; y++)
                    for (int z = 0; z < 16; z++)
                        for (int x = 0; x < 16; x++)
                        {
                            ushort s = states[(y << 8) | (z << 4) | x];
                            if (s == 0) continue;
                            ctx.ci = MeshCtx.PIdx(x, y, z);
                            if (Blocks.StateOpaque[s])
                            {
                                int ci = ctx.ci; var bl = ctx.blocks;
                                if (Blocks.StateOpaque[bl[ci + 1]] && Blocks.StateOpaque[bl[ci - 1]] && Blocks.StateOpaque[bl[ci + MeshCtx.P]]
                                    && Blocks.StateOpaque[bl[ci - MeshCtx.P]] && Blocks.StateOpaque[bl[ci + MeshCtx.P2]] && Blocks.StateOpaque[bl[ci - MeshCtx.P2]]) continue;
                            }
                            var b = Blocks.ByState[s];
                            ctx.x = x; ctx.y = y; ctx.z = z;
                            ctx.state = s; ctx.block = b; ctx.rot = 0;
                            try { b.Emit(ctx, s - b.baseState); }
                            catch (Exception e) { Debug.LogWarning("Emit failed for " + b.id + ": " + e.Message); }
                            ctx.rot = 0;
                            if (Blocks.StateWaterlogged[s])
                            {
                                ctx.block = Blocks.Water; ctx.state = Blocks.Water.baseState;
                                Blocks.Water.Emit(ctx, 0);
                            }
                        }
                int after = ctx.layers[0].vcount + ctx.layers[1].vcount + ctx.layers[2].vcount;
                if (after > before)
                {
                    minY = Mathf.Min(minY, ctx.baseY - 1);
                    maxY = Mathf.Max(maxY, ctx.baseY + 17);
                }
            }
            int total = 0, itotal = 0;
            for (int l = 0; l < 3; l++) { total += ctx.layers[l].vcount; itotal += ctx.layers[l].icount; }
            if (total == 0) { data.empty = true; return data; }
            data.verts = new ChunkVertex[total];
            data.indices = new int[itotal];
            int vo = 0, io = 0;
            for (int l = 0; l < 3; l++)
            {
                var mb = ctx.layers[l];
                Array.Copy(mb.verts, 0, data.verts, vo, mb.vcount);
                data.layerStart[l] = io; data.layerCount[l] = mb.icount;
                for (int i = 0; i < mb.icount; i++) data.indices[io + i] = mb.indices[i] + vo;
                vo += mb.vcount; io += mb.icount;
            }
            data.vcount = total; data.icount = itotal;
            data.minY = minY; data.maxY = maxY;
            return data;
        }

        static void FillPadded(MeshCtx ctx, World w, Chunk[] nb, int sy)
        {
            var blocks = ctx.blocks; var light = ctx.light;
            int secCount = w.sectionCount;
            byte defLight = w.hasSkyLight ? (byte)0xF0 : (byte)0;
            ushort bedrock = Blocks.Bedrock.DefaultState;
            for (int py = 0; py < MeshCtx.P; py++)
            {
                int ly = py - 1; // -1..16
                int s = sy; int yy = ly;
                if (yy < 0) { s = sy - 1; yy += 16; } else if (yy > 15) { s = sy + 1; yy -= 16; }
                for (int pz = 0; pz < MeshCtx.P; pz++)
                {
                    int lz = pz - 1;
                    int cz = lz < 0 ? 0 : (lz > 15 ? 2 : 1);
                    int zz = lz < 0 ? 15 : (lz > 15 ? 0 : lz);
                    int rowBase = py * MeshCtx.P2 + pz * MeshCtx.P;
                    for (int px = 0; px < MeshCtx.P; px++)
                    {
                        int lx = px - 1;
                        int cx = lx < 0 ? 0 : (lx > 15 ? 2 : 1);
                        int xx = lx < 0 ? 15 : (lx > 15 ? 0 : lx);
                        var ch = nb[cz * 3 + cx];
                        int pi = rowBase + px;
                        if (s < 0) { blocks[pi] = bedrock; light[pi] = 0; continue; }
                        if (s >= secCount || ch == null) { blocks[pi] = 0; light[pi] = defLight; continue; }
                        var sec = ch.sections[s];
                        int idx = (yy << 8) | (zz << 4) | xx;
                        blocks[pi] = sec.states != null ? sec.states[idx] : (ushort)0;
                        light[pi] = sec.light != null ? sec.light[idx] : defLight;
                    }
                }
            }
        }

        static void FillTints(MeshCtx ctx, Chunk[] nb)
        {
            // 5x5 box blur of biome colours (radius 2) using neighbours
            for (int z = 0; z < 16; z++)
                for (int x = 0; x < 16; x++)
                {
                    int gr = 0, gg = 0, gb = 0, fr = 0, fg = 0, fb = 0, wr = 0, wg = 0, wb = 0, n = 0;
                    for (int dz = -2; dz <= 2; dz++)
                        for (int dx = -2; dx <= 2; dx++)
                        {
                            int lx = x + dx, lz = z + dz;
                            int cx = lx < 0 ? 0 : (lx > 15 ? 2 : 1), cz = lz < 0 ? 0 : (lz > 15 ? 2 : 1);
                            var ch = nb[cz * 3 + cx] ?? nb[4];
                            int bx = (lx + 16) & 15, bz = (lz + 16) & 15;
                            var b = Biome.Get(ch.biomes2D[(bz << 4) | bx]);
                            gr += b.grass.r; gg += b.grass.g; gb += b.grass.b;
                            fr += b.foliage.r; fg += b.foliage.g; fb += b.foliage.b;
                            wr += b.water.r; wg += b.water.g; wb += b.water.b;
                            n++;
                        }
                    int i = (z << 4) | x;
                    ctx.grassTint[i] = new Color32((byte)(gr / n), (byte)(gg / n), (byte)(gb / n), 255);
                    ctx.foliageTint[i] = new Color32((byte)(fr / n), (byte)(fg / n), (byte)(fb / n), 255);
                    ctx.waterTint[i] = new Color32((byte)(wr / n), (byte)(wg / n), (byte)(wb / n), 255);
                }
        }

        /// <summary>Mesh a single block in isolation (full bright, no neighbours) — used for items, falling blocks, TNT etc.</summary>
        /// <summary>
        /// Runs every block state's mesher once against an empty neighbourhood, so textures that are only looked
        /// up while meshing (per-state faces, growth stages, attached stems) are registered before the texture
        /// array is frozen. Called by both the runtime loader and the editor bake, keeping their layer lists equal.
        /// </summary>
        public static void PrewarmTextures()
        {
            var ctx = new MeshCtx();
            for (int i = 0; i < MeshCtx.P3; i++) { ctx.blocks[i] = 0; ctx.light[i] = 0xF0; }
            ctx.x = ctx.y = ctx.z = 0; ctx.baseY = 0; ctx.wx0 = ctx.wz0 = 0;
            ctx.ci = MeshCtx.PIdx(0, 0, 0);
            ctx.smooth = false; ctx.fancyLeaves = true;
            foreach (var b in Blocks.All)
            {
                if (b == null) continue;
                int states = Math.Max(1, b.stateCount);
                for (int m = 0; m < states; m++)
                {
                    ushort st = (ushort)(b.baseState + m);
                    ctx.blocks[ctx.ci] = st; ctx.state = st; ctx.block = b;
                    foreach (var l in ctx.layers) l.Clear();
                    try { b.Emit(ctx, m); } catch { }
                }
            }
            ctx.blocks[ctx.ci] = 0;
            foreach (var l in ctx.layers) l.Clear();
        }

        public static Mesh BuildSingleBlock(ushort state, bool centered = true)
        {
            var ctx = new MeshCtx();
            for (int i = 0; i < MeshCtx.P3; i++) { ctx.blocks[i] = 0; ctx.light[i] = 0xF0; }
            ctx.x = ctx.y = ctx.z = 0; ctx.baseY = 0; ctx.wx0 = ctx.wz0 = 0;
            ctx.ci = MeshCtx.PIdx(0, 0, 0);
            ctx.blocks[ctx.ci] = state;
            ctx.state = state; ctx.block = Blocks.ByState[state];
            ctx.smooth = false; ctx.fancyLeaves = true;
            var plains = Biome.Plains ?? Biome.Get(0);
            for (int i = 0; i < 256; i++) { ctx.grassTint[i] = plains.grass; ctx.foliageTint[i] = plains.foliage; ctx.waterTint[i] = plains.water; }
            foreach (var l in ctx.layers) l.Clear();
            try { ctx.block.Emit(ctx, state - ctx.block.baseState); } catch (Exception e) { Debug.LogWarning("Single block emit failed " + ctx.block.id + ": " + e.Message); }
            int total = 0, it = 0;
            for (int l = 0; l < 3; l++) { total += ctx.layers[l].vcount; it += ctx.layers[l].icount; }
            var verts = new ChunkVertex[total];
            var mesh = new Mesh { name = "block_" + ctx.block.id };
            int vo = 0;
            var subIdx = new int[3][];
            for (int l = 0; l < 3; l++)
            {
                var mb = ctx.layers[l];
                Array.Copy(mb.verts, 0, verts, vo, mb.vcount);
                subIdx[l] = new int[mb.icount];
                for (int i = 0; i < mb.icount; i++) subIdx[l][i] = mb.indices[i] + vo;
                vo += mb.vcount;
            }
            if (centered) for (int i = 0; i < verts.Length; i++) { verts[i].x -= 0.5f; verts[i].z -= 0.5f; }
            mesh.SetVertexBufferParams(total, ChunkVertex.Layout);
            mesh.SetVertexBufferData(verts, 0, 0, total);
            var all = new int[it]; int io = 0;
            var descs = new UnityEngine.Rendering.SubMeshDescriptor[3];
            for (int l = 0; l < 3; l++) { Array.Copy(subIdx[l], 0, all, io, subIdx[l].Length); descs[l] = new UnityEngine.Rendering.SubMeshDescriptor(io, subIdx[l].Length); io += subIdx[l].Length; }
            mesh.SetIndexBufferParams(it, UnityEngine.Rendering.IndexFormat.UInt32);
            mesh.SetIndexBufferData(all, 0, 0, it);
            // always one submesh per render layer (empty ones included) so materials line up with Res.ItemMats;
            // the count is set after the index buffer because resizing that buffer resets it
            mesh.subMeshCount = 3;
            for (int l = 0; l < 3; l++) mesh.SetSubMesh(l, descs[l], UnityEngine.Rendering.MeshUpdateFlags.DontRecalculateBounds);
            mesh.bounds = new Bounds(centered ? new Vector3(0, 0.5f, 0) : new Vector3(0.5f, 0.5f, 0.5f), Vector3.one * 1.5f);
            return mesh;
        }
    }
}
