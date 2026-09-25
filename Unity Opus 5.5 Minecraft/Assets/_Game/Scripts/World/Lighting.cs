using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Minecraft-style light: sky light (propagates straight down without loss) and block light, 0..15 each.
    /// Column lighting is computed on worker threads using a 3x3 chunk window (exact because light range is 15 &lt; 16).
    /// Edits are handled incrementally on the main thread with add/remove BFS.
    /// </summary>
    public static class Lighting
    {
        const int W = 48;
        sealed class Scratch
        {
            public byte[] op = new byte[0];
            public byte[] sky = new byte[0];
            public byte[] blk = new byte[0];
            public int[] queue = new int[1 << 18];
            public short[] expose = new short[W * W];
            public readonly List<int> emitters = new List<int>();
            public void Ensure(int cells)
            {
                if (op.Length < cells) { op = new byte[cells]; sky = new byte[cells]; blk = new byte[cells]; }
            }
        }
        [ThreadStatic] static Scratch tls;

        /// <summary>Compute light for 'center'. All 8 neighbours must be at stage Final. Thread-safe w.r.t. other LightChunk calls.</summary>
        public static void LightChunk(World w, Chunk center, Chunk[] nb)
        {
            var s = tls ?? (tls = new Scratch());
            int secCount = w.sectionCount;
            int topSec = 0;
            for (int i = 0; i < 9; i++)
            {
                var c = nb[i];
                if (c == null) continue;
                for (int sy = secCount - 1; sy >= 0; sy--)
                    if (!c.sections[sy].IsEmpty) { if (sy + 1 > topSec) topSec = sy + 1; break; }
            }
            int R = Math.Min(secCount * 16, (topSec + 1) * 16); // rows covered
            int cells = W * W * R;
            s.Ensure(cells);
            byte[] op = s.op, sky = s.sky, blk = s.blk;
            Array.Clear(sky, 0, cells); Array.Clear(blk, 0, cells);
            s.emitters.Clear();
            // fill opacity/emission
            for (int ci = 0; ci < 9; ci++)
            {
                var c = nb[ci];
                int ox = (ci % 3) * 16, oz = (ci / 3) * 16;
                for (int sy = 0; sy * 16 < R; sy++)
                {
                    var st = c?.sections[sy].states;
                    bool empty = st == null || c.sections[sy].nonAir == 0;
                    for (int ly = 0; ly < 16; ly++)
                    {
                        int y = sy * 16 + ly;
                        if (y >= R) break;
                        for (int lz = 0; lz < 16; lz++)
                        {
                            int rowBase = (y * W + (oz + lz)) * W + ox;
                            if (empty)
                            {
                                for (int lx = 0; lx < 16; lx++) op[rowBase + lx] = 0;
                                continue;
                            }
                            int si = (ly << 8) | (lz << 4);
                            for (int lx = 0; lx < 16; lx++)
                            {
                                ushort state = st[si + lx];
                                op[rowBase + lx] = Blocks.StateOpacity[state];
                                byte em = Blocks.StateEmission[state];
                                if (em > 0) { blk[rowBase + lx] = em; s.emitters.Add(rowBase + lx); }
                            }
                        }
                    }
                }
            }

            int[] q = s.queue;
            int qmask, head, tail;
            // ---- sky light
            if (w.hasSkyLight)
            {
                for (int z = 0; z < W; z++)
                    for (int x = 0; x < W; x++)
                    {
                        int level = 15; int exposedTo = R;
                        for (int y = R - 1; y >= 0; y--)
                        {
                            int i = (y * W + z) * W + x;
                            int o = op[i];
                            if (o >= 15) { level = 0; break; }
                            if (o > 0) level -= o;
                            if (level <= 0) { level = 0; break; }
                            sky[i] = (byte)level;
                            if (level == 15) exposedTo = y;
                        }
                        s.expose[z * W + x] = (short)exposedTo;
                    }
                head = tail = 0;
                // seeds: exposed cells that neighbour less exposed columns, and all partially lit cells
                for (int z = 0; z < W; z++)
                    for (int x = 0; x < W; x++)
                    {
                        int h = s.expose[z * W + x];
                        int maxN = h;
                        if (x > 0) maxN = Math.Max(maxN, s.expose[z * W + x - 1]);
                        if (x < W - 1) maxN = Math.Max(maxN, s.expose[z * W + x + 1]);
                        if (z > 0) maxN = Math.Max(maxN, s.expose[(z - 1) * W + x]);
                        if (z < W - 1) maxN = Math.Max(maxN, s.expose[(z + 1) * W + x]);
                        int yTop = Math.Min(maxN, R - 1);
                        for (int y = h; y <= yTop; y++)
                        {
                            int i = (y * W + z) * W + x;
                            if (sky[i] > 1) Push(ref s, ref q, ref head, ref tail, i);
                        }
                        // partially lit cells below exposure (water, leaves)
                        for (int y = h - 1; y >= 0; y--)
                        {
                            int i = (y * W + z) * W + x;
                            if (sky[i] == 0) break;
                            if (sky[i] > 1) Push(ref s, ref q, ref head, ref tail, i);
                        }
                    }
                q = s.queue;
                Propagate(s, sky, op, R, ref head, ref tail, true);
            }
            // ---- block light
            head = tail = 0;
            foreach (int i in s.emitters) Push(ref s, ref q, ref head, ref tail, i);
            Propagate(s, blk, op, R, ref head, ref tail, false);

            // ---- write back center column
            for (int sy = 0; sy < secCount; sy++)
            {
                var sec = center.sections[sy];
                if (sy * 16 >= R)
                {
                    sec.light = null; // default (sky 15 or 0)
                    continue;
                }
                var l = sec.light ?? new byte[4096];
                for (int ly = 0; ly < 16; ly++)
                {
                    int y = sy * 16 + ly;
                    for (int lz = 0; lz < 16; lz++)
                    {
                        int rowBase = (y * W + (16 + lz)) * W + 16;
                        int di = (ly << 8) | (lz << 4);
                        for (int lx = 0; lx < 16; lx++)
                            l[di + lx] = (byte)((sky[rowBase + lx] << 4) | blk[rowBase + lx]);
                    }
                }
                sec.light = l;
            }
        }

        static void Push(ref Scratch s, ref int[] q, ref int head, ref int tail, int v)
        {
            int count = tail - head;
            if (count >= q.Length - 1)
            {
                // grow circular buffer (unwrap)
                var nq = new int[q.Length * 2];
                for (int k = 0; k < count; k++) nq[k] = q[(head + k) & (q.Length - 1)];
                q = nq; s.queue = nq; head = 0; tail = count;
            }
            q[tail & (q.Length - 1)] = v;
            tail++;
        }

        static void Propagate(Scratch s, byte[] light, byte[] op, int R, ref int head, ref int tail, bool skyRule)
        {
            int[] q = s.queue;
            const int WW = W * W;
            while (head != tail)
            {
                int i = q[head & (q.Length - 1)]; head++;
                int L = light[i];
                if (L <= 1) continue;
                int y = i / WW; int rem = i - y * WW; int z = rem / W; int x = rem - z * W;
                // 6 neighbours
                for (int d = 0; d < 6; d++)
                {
                    int ni;
                    switch (d)
                    {
                        case 0: if (y == 0) continue; ni = i - WW; break;
                        case 1: if (y == R - 1) continue; ni = i + WW; break;
                        case 2: if (z == W - 1) continue; ni = i + W; break;
                        case 3: if (z == 0) continue; ni = i - W; break;
                        case 4: if (x == 0) continue; ni = i - 1; break;
                        default: if (x == W - 1) continue; ni = i + 1; break;
                    }
                    int o = op[ni];
                    if (o >= 15) continue;
                    int nl = (skyRule && d == 0 && L == 15 && o == 0) ? 15 : L - (o > 1 ? o : 1);
                    if (nl > light[ni])
                    {
                        light[ni] = (byte)nl;
                        Push(ref s, ref q, ref head, ref tail, ni);
                        q = s.queue;
                    }
                }
            }
        }

        // ================================================================== incremental (main thread)
        struct LNode { public Int3 p; public int level; }
        static readonly Queue<LNode> removeQ = new Queue<LNode>();
        static readonly Queue<Int3> addQ = new Queue<Int3>();

        static int GetL(World w, Int3 p, bool sky)
        {
            if (p.y < w.minY) return 0;
            if (p.y >= w.maxY) return sky && w.hasSkyLight ? 15 : 0;
            var c = w.ReadyChunk(p.x, p.z);
            if (c == null || !c.lit) return -1;
            byte v = c.GetLight(p.x & 15, p.y, p.z & 15);
            return sky ? v >> 4 : v & 15;
        }

        static void SetL(World w, Int3 p, bool sky, int val)
        {
            if (p.y < w.minY || p.y >= w.maxY) return;
            var c = w.ReadyChunk(p.x, p.z);
            if (c == null || !c.lit) return;
            byte v = c.GetLight(p.x & 15, p.y, p.z & 15);
            byte nv = sky ? (byte)((v & 0x0F) | (val << 4)) : (byte)((v & 0xF0) | val);
            if (nv == v) return;
            c.SetLight(p.x & 15, p.y, p.z & 15, nv);
            w.MarkDirtyAround(p);
        }

        public static void OnBlockChanged(World w, Int3 p, ushort oldS, ushort newS)
        {
            int oldOp = Blocks.StateOpacity[oldS], newOp = Blocks.StateOpacity[newS];
            int oldEm = Blocks.StateEmission[oldS], newEm = Blocks.StateEmission[newS];
            if (oldEm != newEm || oldOp != newOp) UpdateChannel(w, p, false, newEm, oldOp, newOp);
            if (w.hasSkyLight && oldOp != newOp) UpdateChannel(w, p, true, 0, oldOp, newOp);
        }

        static void UpdateChannel(World w, Int3 p, bool sky, int newEmit, int oldOp, int newOp)
        {
            int cur = GetL(w, p, sky);
            if (cur < 0) return;
            removeQ.Clear(); addQ.Clear();
            bool needRemove = sky ? newOp > oldOp : (newEmit < cur || newOp > oldOp);
            if (needRemove && cur > 0)
            {
                removeQ.Enqueue(new LNode { p = p, level = cur });
                SetL(w, p, sky, 0);
                int guard = 0;
                while (removeQ.Count > 0 && guard++ < 400000)
                {
                    var n = removeQ.Dequeue();
                    for (int d = 0; d < 6; d++)
                    {
                        Int3 np = n.p + DirUtil.Offset[d];
                        int nl = GetL(w, np, sky);
                        if (nl <= 0) continue;
                        bool dependent = nl < n.level || (sky && d == 0 && n.level == 15 && nl == 15);
                        if (dependent)
                        {
                            SetL(w, np, sky, 0);
                            removeQ.Enqueue(new LNode { p = np, level = nl });
                        }
                        else addQ.Enqueue(np);
                    }
                }
            }
            if (!sky && newEmit > 0)
            {
                if (GetL(w, p, false) < newEmit) SetL(w, p, false, newEmit);
                addQ.Enqueue(p);
            }
            if (newOp < oldOp || needRemove)
            {
                for (int d = 0; d < 6; d++)
                {
                    Int3 np = p + DirUtil.Offset[d];
                    if (GetL(w, np, sky) > 0 || (sky && np.y >= w.maxY)) addQ.Enqueue(np);
                }
                if (sky && p.y + 1 >= w.maxY && newOp == 0) { SetL(w, p, true, 15); addQ.Enqueue(p); }
            }
            PropagateWorld(w, sky);
        }

        static void PropagateWorld(World w, bool sky)
        {
            int guard = 0;
            while (addQ.Count > 0 && guard++ < 600000)
            {
                Int3 p = addQ.Dequeue();
                int L = p.y >= w.maxY ? (sky ? 15 : 0) : GetL(w, p, sky);
                if (L <= 1) continue;
                for (int d = 0; d < 6; d++)
                {
                    Int3 np = p + DirUtil.Offset[d];
                    if (np.y < w.minY || np.y >= w.maxY) continue;
                    int cur = GetL(w, np, sky);
                    if (cur < 0) continue;
                    int o = Blocks.StateOpacity[w.GetState(np)];
                    if (o >= 15) continue;
                    int nl = (sky && d == 0 && L == 15 && o == 0) ? 15 : L - (o > 1 ? o : 1);
                    if (nl > cur)
                    {
                        SetL(w, np, sky, nl);
                        addQ.Enqueue(np);
                    }
                }
            }
        }
    }
}
