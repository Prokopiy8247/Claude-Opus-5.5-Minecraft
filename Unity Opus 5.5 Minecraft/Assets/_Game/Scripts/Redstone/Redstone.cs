using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Redstone power queries (Java-like semantics):
    /// - sources emit weak power to neighbours and strong power into specific blocks;
    /// - a solid block that is strongly powered powers adjacent dust and components;
    /// - a solid block weakly powered by dust powers adjacent components (not dust).
    /// </summary>
    public static class Redstone
    {
        public static bool IsConductor(Block b) => b.opaqueCube && !(b is RedstoneBlock) && b.id != "observer" && !(b is PistonBlock);

        /// <summary>Strong power flowing into a solid block from any neighbouring source.</summary>
        public static int StrongPowerInto(World w, Int3 block)
        {
            int best = 0;
            for (int d = 0; d < 6; d++)
            {
                Int3 n = block.Offset((Dir)d);
                ushort s = w.GetState(n);
                var b = Blocks.ByState[s];
                if (!b.IsRedstoneSource(s - b.baseState)) continue;
                int p = b.GetStrongPower(w, n, s - b.baseState, DirUtil.Opposite((Dir)d));
                if (p > best) { best = p; if (best >= 15) return 15; }
            }
            return best;
        }

        /// <summary>Weak power flowing into a solid block (dust pointing into it, and strong sources).</summary>
        public static int WeakPowerInto(World w, Int3 block, Int3? exclude = null)
        {
            int best = 0;
            for (int d = 0; d < 6; d++)
            {
                Int3 n = block.Offset((Dir)d);
                if (exclude.HasValue && n == exclude.Value) continue;
                ushort s = w.GetState(n);
                var b = Blocks.ByState[s];
                if (!b.IsRedstoneSource(s - b.baseState)) continue;
                Dir towards = DirUtil.Opposite((Dir)d);
                int p = Mathf.Max(b.GetWeakPower(w, n, s - b.baseState, towards) * (b is RedstoneWireBlock ? 1 : 0), b.GetStrongPower(w, n, s - b.baseState, towards));
                if (p > best) { best = p; if (best >= 15) return 15; }
            }
            return best;
        }

        /// <summary>Power emitted by the block at 'from' into the position in direction 'towards'.</summary>
        public static int EmittedPower(World w, Int3 from, Dir towards, bool forWire = false)
        {
            ushort s = w.GetState(from);
            var b = Blocks.ByState[s];
            int m = s - b.baseState;
            if (b.IsRedstoneSource(m)) return b.GetWeakPower(w, from, m, towards);
            if (IsConductor(b))
            {
                int p = StrongPowerInto(w, from);
                if (!forWire) p = Mathf.Max(p, WeakPowerInto(w, from, from.Offset(towards)));
                return p;
            }
            return 0;
        }

        /// <summary>Power received by a component at pos from all 6 sides.</summary>
        public static int ReceivedPower(World w, Int3 pos)
        {
            int best = 0;
            for (int d = 0; d < 6; d++)
            {
                Int3 n = pos.Offset((Dir)d);
                int p = EmittedPower(w, n, DirUtil.Opposite((Dir)d));
                if (p > best) { best = p; if (best >= 15) return 15; }
            }
            return best;
        }

        public static bool IsPowered(World w, Int3 pos) => ReceivedPower(w, pos) > 0;

        static readonly HashSet<long> poweredOpen = new HashSet<long>();
        /// <summary>Remember that a door/trapdoor/gate was opened by a redstone signal (so it closes when the signal ends).</summary>
        public static void MarkPoweredOpen(Int3 p) => poweredOpen.Add(p.Pack());
        public static bool WasPoweredOpen(World w, Int3 p) => poweredOpen.Remove(p.Pack());

        /// <summary>Is the (solid) block at 'block' powered, ignoring power coming from 'exclude' (used by torches).</summary>
        public static bool IsBlockPoweredFrom(World w, Int3 block, Int3 exclude)
        {
            var b = w.GetBlock(block);
            if (b is RedstoneBlock) return true;
            for (int d = 0; d < 6; d++)
            {
                Int3 n = block.Offset((Dir)d);
                if (n == exclude) continue;
                ushort s = w.GetState(n);
                var nb = Blocks.ByState[s];
                int m = s - nb.baseState;
                if (!nb.IsRedstoneSource(m)) continue;
                Dir towards = DirUtil.Opposite((Dir)d);
                if (nb.GetStrongPower(w, n, m, towards) > 0) return true;
                if (nb is RedstoneWireBlock && nb.GetWeakPower(w, n, m, towards) > 0) return true;
                if (!IsConductor(b) && nb.GetWeakPower(w, n, m, towards) > 0) return true;
            }
            return false;
        }

        /// <summary>Notify blocks within 2 of pos (like MC's updateNeighborsAt for sources that strongly power).</summary>
        public static void NotifyAround(World w, Int3 pos)
        {
            w.NotifyNeighbors(pos);
            for (int d = 0; d < 6; d++) w.NotifyNeighbors(pos.Offset((Dir)d));
        }

        // ------------------------------------------------------------------ wire networks
        static bool updatingWire;
        static readonly Dictionary<long, int> netPower = new Dictionary<long, int>();
        static readonly List<Int3> netList = new List<Int3>();
        static readonly Queue<Int3> bfs = new Queue<Int3>();

        public static bool UpdatingWire => updatingWire;

        public static void UpdateWire(World w, Int3 start)
        {
            if (updatingWire) return;
            updatingWire = true;
            try
            {
                netPower.Clear(); netList.Clear(); bfs.Clear();
                var wire = (RedstoneWireBlock)Blocks.Get("redstone_wire");
                // gather network
                bfs.Enqueue(start); netPower[start.Pack()] = -1;
                while (bfs.Count > 0 && netList.Count < 2048)
                {
                    Int3 p = bfs.Dequeue();
                    if (!(w.GetBlock(p) is RedstoneWireBlock)) continue;
                    netList.Add(p);
                    foreach (var n in wire.WireNeighbours(w, p))
                    {
                        long k = n.Pack();
                        if (netPower.ContainsKey(k)) continue;
                        netPower[k] = -1;
                        bfs.Enqueue(n);
                    }
                }
                // external source power per wire
                var order = new List<(Int3 p, int pw)>(netList.Count);
                foreach (var p in netList)
                {
                    int pw = 0;
                    for (int d = 0; d < 6; d++)
                    {
                        Int3 n = p.Offset((Dir)d);
                        var nb = w.GetBlock(n);
                        if (nb is RedstoneWireBlock) continue;
                        pw = Mathf.Max(pw, EmittedPower(w, n, DirUtil.Opposite((Dir)d), true));
                        if (pw >= 15) break;
                    }
                    netPower[p.Pack()] = pw;
                }
                // propagate (bucket queue by descending power)
                var buckets = new List<Int3>[16];
                for (int i = 0; i < 16; i++) buckets[i] = new List<Int3>();
                foreach (var p in netList) { int pw = netPower[p.Pack()]; if (pw > 0) buckets[pw].Add(p); }
                for (int lvl = 15; lvl >= 1; lvl--)
                {
                    var bl = buckets[lvl];
                    for (int i = 0; i < bl.Count; i++)
                    {
                        Int3 p = bl[i];
                        if (netPower[p.Pack()] != lvl) continue;
                        foreach (var n in wire.WireNeighbours(w, p))
                        {
                            long k = n.Pack();
                            if (!netPower.TryGetValue(k, out int cur)) continue;
                            if (cur < lvl - 1) { netPower[k] = lvl - 1; if (lvl - 1 > 0) buckets[lvl - 1].Add(n); }
                        }
                    }
                }
                // apply
                var changed = new List<Int3>();
                foreach (var p in netList)
                {
                    int pw = Mathf.Max(0, netPower[p.Pack()]);
                    ushort s = w.GetState(p);
                    var b = Blocks.ByState[s];
                    if (s - b.baseState != pw) { w.SetState(p, b.State(pw), SetFlags.Hooks); changed.Add(p); }
                }
                foreach (var p in changed)
                {
                    w.NotifyNeighbors(p);
                    w.NotifyNeighbors(p.Offset(Dir.Down));
                    for (int i = 0; i < 4; i++) w.NotifyNeighbors(p.Offset(DirUtil.Horizontal[i]));
                }
            }
            finally { updatingWire = false; }
        }
    }

    // ============================================================================ Redstone dust
    public class RedstoneWireBlock : Block
    {
        int dotTex, lineTex;
        public RedstoneWireBlock()
        {
            stateCount = 16; opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 0;
            dotTex = Tex.Id("redstone_dust_dot"); lineTex = Tex.Id("redstone_dust_line"); SetAllTex(dotTex); isFullCubeShape = false;
            noItem = true; push = PushReaction.Destroy;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override bool IsRedstoneSource(int meta) => true;
        public static Color32 PowerColor(int power)
        {
            float f = power / 15f;
            float r = power == 0 ? 0.3f : f * 0.6f + 0.4f;
            float g = Mathf.Clamp01(f * f * 0.7f - 0.5f);
            float b = Mathf.Clamp01(f * f * 0.6f - 0.7f);
            return new Color32((byte)(r * 255), (byte)(g * 255), (byte)(b * 255), 255);
        }

        static bool ConnectsToComponent(ushort s, Dir towardThem)
        {
            var b = Blocks.ByState[s];
            int m = s - b.baseState;
            if (b is RedstoneWireBlock) return true;
            if (b is RepeaterBlock) { Dir f = StateBits.FacingDir(m); return f == towardThem || f == DirUtil.Opposite(towardThem); }
            if (b is ObserverBlock) { return ObserverBlock.Facing(m) == towardThem; }
            return b.IsRedstoneSource(m) || b is ComparatorBlock || b is TargetBlock;
        }

        /// <summary>Connection mask N,E,S,W (bits0-3); +16<<i for "up the side" connections.</summary>
        public int Connections(System.Func<int, int, int, ushort> nb)
        {
            int mask = 0;
            bool aboveSolid = Blocks.StateOpaque[nb(0, 1, 0)];
            for (int i = 0; i < 4; i++)
            {
                Dir d = DirUtil.Horizontal[i];
                Int3 o = DirUtil.Offset[(int)d];
                ushort side = nb(o.x, 0, o.z);
                if (ConnectsToComponent(side, d)) { mask |= 1 << i; continue; }
                bool sideSolid = Blocks.StateOpaque[side];
                if (!aboveSolid && sideSolid && Blocks.ByState[nb(o.x, 1, o.z)] is RedstoneWireBlock) { mask |= (1 << i) | (16 << i); continue; }
                if (!sideSolid && Blocks.ByState[nb(o.x, -1, o.z)] is RedstoneWireBlock) mask |= 1 << i;
            }
            return mask;
        }

        public IEnumerable<Int3> WireNeighbours(World w, Int3 p)
        {
            bool aboveSolid = Blocks.StateOpaque[w.GetState(p.Offset(Dir.Up))];
            for (int i = 0; i < 4; i++)
            {
                Int3 n = p.Offset(DirUtil.Horizontal[i]);
                if (w.GetBlock(n) is RedstoneWireBlock) { yield return n; continue; }
                bool sideSolid = Blocks.StateOpaque[w.GetState(n)];
                if (!aboveSolid && sideSolid) { Int3 up = n.Offset(Dir.Up); if (w.GetBlock(up) is RedstoneWireBlock) yield return up; }
                if (!sideSolid) { Int3 dn = n.Offset(Dir.Down); if (w.GetBlock(dn) is RedstoneWireBlock) yield return dn; }
            }
        }

        public override void Emit(MeshCtx ctx, int meta)
        {
            int con = Connections((x, y, z) => ctx.N(x, y, z));
            var col = PowerColor(meta);
            uint c = MeshCtx.Pack(col);
            int horiz = con & 15;
            // decide line vs dot rendering
            bool ns = (horiz & 0b0101) != 0, ew = (horiz & 0b1010) != 0;
            if (horiz == 0) { ctx.FloorDecal(dotTex, c); ctx.FloorDecal(lineTex, c, 1.2f / 64f, 0); ctx.FloorDecal(lineTex, c, 1.4f / 64f, 1); return; }
            if (horiz == 0b0101 || horiz == 0b0001 || horiz == 0b0100) { ctx.FloorDecal(lineTex, c, 1f / 64f, 0); }
            else if (horiz == 0b1010 || horiz == 0b0010 || horiz == 0b1000) { ctx.FloorDecal(lineTex, c, 1f / 64f, 1); }
            else
            {
                ctx.FloorDecal(dotTex, c);
                // partial arms: draw half-lines toward each connection
                for (int i = 0; i < 4; i++)
                {
                    if ((horiz & (1 << i)) == 0) continue;
                    float y = (1.2f + i * 0.1f) / 64f;
                    Vector3 a, b, cc, d;
                    switch (i)
                    {
                        case 0: a = new Vector3(5 / 16f, y, 0.5f); b = new Vector3(5 / 16f, y, 1); cc = new Vector3(11 / 16f, y, 1); d = new Vector3(11 / 16f, y, 0.5f); break;
                        case 1: a = new Vector3(0.5f, y, 11 / 16f); b = new Vector3(1, y, 11 / 16f); cc = new Vector3(1, y, 5 / 16f); d = new Vector3(0.5f, y, 5 / 16f); break;
                        case 2: a = new Vector3(11 / 16f, y, 0.5f); b = new Vector3(11 / 16f, y, 0); cc = new Vector3(5 / 16f, y, 0); d = new Vector3(5 / 16f, y, 0.5f); break;
                        default: a = new Vector3(0.5f, y, 5 / 16f); b = new Vector3(0, y, 5 / 16f); cc = new Vector3(0, y, 11 / 16f); d = new Vector3(0.5f, y, 11 / 16f); break;
                    }
                    Vector2 u0 = i % 2 == 0 ? new Vector2(5 / 16f, 0.5f) : new Vector2(0.5f, 5 / 16f);
                    ctx.Quad(a, b, cc, d, new Vector2(0, 0.5f), new Vector2(0, 1), new Vector2(1, 1), new Vector2(1, 0.5f), lineTex, c, 1f, RenderLayer.Cutout, true);
                }
            }
            // vertical runs up the side of blocks
            for (int i = 0; i < 4; i++)
            {
                if ((con & (16 << i)) == 0) continue;
                Dir d = DirUtil.Horizontal[i];
                ctx.WallDecal(DirUtil.Opposite(d), lineTex, c, 0.3f / 16f);
            }
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 1, 16));
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            Int3 b = pos.Offset(Dir.Down);
            var bb = w.GetBlock(b);
            return bb.opaqueCube || w.IsSturdy(b, Dir.Up) || bb is HopperBlock;
        }
        public override bool CanPlaceAt(World w, Int3 pos, ushort state) => CanSurvive(w, pos, 0);
        public override void OnAdded(World w, Int3 pos, int meta, ushort oldState) { Redstone.UpdateWire(w, pos); Redstone.NotifyAround(w, pos); }
        public override void OnRemoved(World w, Int3 pos, int meta, ushort newState)
        {
            if (Redstone.UpdatingWire) return;
            w.session?.Defer(() =>
            {
                for (int i = 0; i < 4; i++)
                {
                    Int3 n = pos.Offset(DirUtil.Horizontal[i]);
                    foreach (var p in new[] { n, n.Offset(Dir.Up), n.Offset(Dir.Down) })
                        if (w.GetBlock(p) is RedstoneWireBlock) Redstone.UpdateWire(w, p);
                }
                if (w.GetBlock(pos.Offset(Dir.Down)) is RedstoneWireBlock) Redstone.UpdateWire(w, pos.Offset(Dir.Down));
                if (w.GetBlock(pos.Offset(Dir.Up)) is RedstoneWireBlock) Redstone.UpdateWire(w, pos.Offset(Dir.Up));
                Redstone.NotifyAround(w, pos);
            });
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (!CanSurvive(w, pos, meta)) { w.BreakBlock(pos, true, null); return; }
            if (!Redstone.UpdatingWire) Redstone.UpdateWire(w, pos);
        }
        public override int GetWeakPower(World w, Int3 pos, int meta, Dir towards)
        {
            if (meta == 0 || towards == Dir.Up) return 0;
            if (towards == Dir.Down) return meta;
            int con = Connections((x, y, z) => w.GetState(new Int3(pos.x + x, pos.y + y, pos.z + z))) & 15;
            int i = DirUtil.HorizIndex(towards);
            if (con == 0) return meta;
            if ((con & (1 << i)) != 0) return meta;
            // a straight line end also points forward
            int opp = (i + 2) & 3;
            if (con == (1 << opp)) return meta;
            return 0;
        }
        public override int GetStrongPower(World w, Int3 pos, int meta, Dir towards) => 0;
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) => drops.Add(new ItemStack("redstone", 1));
        public override Item GetPickItem(int meta) => Items.Get("redstone");
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (meta > 0 && rng.Chance(meta / 40f)) Particles.RedstoneDust(w, pos.ToVector3() + new Vector3(rng.NextFloat(), 0.1f, rng.NextFloat()), meta / 15f);
        }
    }

    public class RedstoneBlock : Block
    {
        public override bool IsRedstoneSource(int meta) => true;
        public override int GetWeakPower(World w, Int3 pos, int meta, Dir towards) => 15;
        public override void OnAdded(World w, Int3 pos, int meta, ushort oldState) => Redstone.NotifyAround(w, pos);
        public override void OnRemoved(World w, Int3 pos, int meta, ushort newState) => w.session?.Defer(() => Redstone.NotifyAround(w, pos));
    }

    // ============================================================================ Lever / button / plates
    /// <summary>Attachment face: 0 floor, 1 wall, 2 ceiling. meta = facing(0-1) | face(2-3) | powered(4)</summary>
    public abstract class AttachedSwitch : Block
    {
        protected AttachedSwitch()
        {
            opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; isFullCubeShape = false; push = PushReaction.Destroy;
            creativeTab = CreativeTab.Redstone;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public static int Face(int m) => (m >> 2) & 3;
        public static bool Powered(int m) => (m & 16) != 0;
        public static Dir Facing(int m) => DirUtil.FromHorizIndex(m & 3);
        public static Dir AttachDir(int m)
        {
            int f = Face(m);
            if (f == 0) return Dir.Down;
            if (f == 2) return Dir.Up;
            return DirUtil.Opposite(Facing(m));
        }
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            int face; Dir facing;
            if (ctx.clickedFace == Dir.Up) { face = 0; facing = ctx.playerFacing; }
            else if (ctx.clickedFace == Dir.Down) { face = 2; facing = ctx.playerFacing; }
            else { face = 1; facing = ctx.clickedFace; }
            int m = DirUtil.HorizIndex(facing) | (face << 2);
            if (!CanSurvive(ctx.world, ctx.pos, m)) return 0;
            return State(m);
        }
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            Dir a = AttachDir(meta);
            Int3 s = pos.Offset(a);
            var b = w.GetBlock(s);
            return b.opaqueCube || w.IsSturdy(s, DirUtil.Opposite(a));
        }
        public override bool CanPlaceAt(World w, Int3 pos, ushort state) => CanSurvive(w, pos, state - baseState);
        public override bool IsRedstoneSource(int meta) => true;
        public override int GetWeakPower(World w, Int3 pos, int meta, Dir towards) => Powered(meta) ? 15 : 0;
        public override int GetStrongPower(World w, Int3 pos, int meta, Dir towards) => Powered(meta) && towards == AttachDir(meta) ? 15 : 0;
        protected void UpdateNeighbours(World w, Int3 pos, int meta)
        {
            w.NotifyNeighbors(pos);
            w.NotifyNeighbors(pos.Offset(AttachDir(meta)));
        }
        public override void OnRemoved(World w, Int3 pos, int meta, ushort newState)
        {
            if (Powered(meta)) w.session?.Defer(() => { w.NotifyNeighbors(pos); w.NotifyNeighbors(pos.Offset(AttachDir(meta))); });
        }
        protected Matrix4x4 AttachMatrix(int meta)
        {
            // canonical model is built on the floor; rotate for wall/ceiling
            int f = Face(meta);
            Matrix4x4 m = Matrix4x4.identity;
            Vector3 c = new Vector3(0.5f, 0.5f, 0.5f);
            if (f == 1) m = Matrix4x4.Translate(c) * Matrix4x4.Rotate(Quaternion.Euler(-90, 0, 0)) * Matrix4x4.Translate(-c);
            else if (f == 2) m = Matrix4x4.Translate(c) * Matrix4x4.Rotate(Quaternion.Euler(180, 0, 0)) * Matrix4x4.Translate(-c);
            return m;
        }
    }

    public class LeverBlock : AttachedSwitch
    {
        int baseTex, handleTex;
        public LeverBlock() { stateCount = 32; hardness = 0.5f; sound = SoundType.Wood; baseTex = Tex.Id("cobblestone"); handleTex = Tex.Id("lever"); SetAllTex(handleTex); layer = RenderLayer.Cutout; }
        public override void Emit(MeshCtx ctx, int meta)
        {
            Matrix4x4 att = AttachMatrix(meta);
            int save = ctx.rot; ctx.rot = meta & 3;
            ctx.XBoxPx(att, 5, 0, 4, 11, 3, 12, baseTex, 0xFFFFFFFFu, RenderLayer.Cutout);
            float ang = Powered(meta) ? -40 : 40;
            Matrix4x4 h = att * Matrix4x4.Translate(new Vector3(0.5f, 1f / 16f, 0.5f)) * Matrix4x4.Rotate(Quaternion.Euler(ang, 0, 0)) * Matrix4x4.Translate(new Vector3(-0.5f, -1f / 16f, -0.5f));
            var uv = new Vector4[6];
            for (int i = 2; i < 6; i++) uv[i] = new Vector4(7 / 16f, 6 / 16f, 9 / 16f, 16 / 16f);
            uv[1] = new Vector4(7 / 16f, 14 / 16f, 9 / 16f, 16 / 16f); uv[0] = uv[1];
            ctx.XBoxPx(h, 7, 1, 7, 9, 11, 9, handleTex, 0xFFFFFFFFu, RenderLayer.Cutout, uv);
            ctx.rot = save;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            int f = Face(meta); int r = meta & 3;
            if (f == 0) boxes.Add(BoxUtil.Rot(BoxUtil.Px(5, 0, 4, 11, 6, 12), r));
            else if (f == 2) boxes.Add(BoxUtil.Rot(BoxUtil.Px(5, 10, 4, 11, 16, 12), r));
            else boxes.Add(BoxUtil.Rot(BoxUtil.Px(5, 4, 0, 11, 12, 6), r));
        }
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            int nm = meta ^ 16;
            w.SetState(pos, State(nm), SetFlags.Notify);
            UpdateNeighbours(w, pos, nm);
            Sounds.Play("block.lever.click", pos.Center, 0.3f, Powered(nm) ? 0.6f : 0.5f);
            if (Powered(nm)) Particles.RedstoneDust(w, pos.Center, 1f);
            return true;
        }
    }

    public class ButtonBlock : AttachedSwitch
    {
        public int pressTicks;
        public ButtonBlock(Block material, int ticks)
        {
            stateCount = 32; pressTicks = ticks; hardness = 0.5f; sound = material.sound;
            System.Array.Copy(material.faceTex, faceTex, 6); particleTex = material.particleTex;
        }
        public override void Emit(MeshCtx ctx, int meta)
        {
            Matrix4x4 att = AttachMatrix(meta);
            int save = ctx.rot; ctx.rot = meta & 3;
            float h = Powered(meta) ? 1 : 2;
            ctx.XBoxPx(att, 5, 0, 6, 11, h, 10, faceTex[0], 0xFFFFFFFFu, RenderLayer.Opaque);
            ctx.rot = save;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            int f = Face(meta); int r = meta & 3;
            if (f == 0) boxes.Add(BoxUtil.Rot(BoxUtil.Px(5, 0, 6, 11, 2, 10), r));
            else if (f == 2) boxes.Add(BoxUtil.Rot(BoxUtil.Px(5, 14, 6, 11, 16, 10), r));
            else boxes.Add(BoxUtil.Rot(BoxUtil.Px(5, 6, 0, 11, 10, 2), r));
        }
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (Powered(meta)) return true;
            Press(w, pos, meta);
            return true;
        }
        public void Press(World w, Int3 pos, int meta)
        {
            int nm = meta | 16;
            w.SetState(pos, State(nm), SetFlags.Notify);
            UpdateNeighbours(w, pos, nm);
            Sounds.Play("block.button.click_on", pos.Center, 0.3f, 0.6f);
            w.ScheduleTick(pos, this, pressTicks);
        }
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            if (!Powered(meta)) return;
            int nm = meta & ~16;
            w.SetState(pos, State(nm), SetFlags.Notify);
            UpdateNeighbours(w, pos, nm);
            Sounds.Play("block.button.click_off", pos.Center, 0.3f, 0.5f);
        }
        public override void OnProjectileHit(World w, Int3 pos, int meta, Entity projectile)
        {
            if (pressTicks == 30 && !Powered(meta)) Press(w, pos, meta);
        }
    }

    public class PressurePlateBlock : Block
    {
        public enum Kind { Wood, Stone, Light, Heavy }
        public Kind kind;
        public PressurePlateBlock(Block material, Kind k)
        {
            kind = k; stateCount = 16; opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; isFullCubeShape = false;
            System.Array.Copy(material.faceTex, faceTex, 6); particleTex = material.particleTex; hardness = 0.5f; sound = material.sound;
            push = PushReaction.Destroy; creativeTab = CreativeTab.Redstone; tool = material.tool; requiresTool = material.requiresTool && k != Kind.Wood;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta) => ctx.BoxPx(1, 0, 1, 15, meta > 0 ? 0.5f : 1, 15, faceTex[1], 0xFFFFFFFFu);
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(1, 0, 1, 15, 1, 15));
        public override bool CanSurvive(World w, Int3 pos, int meta) { var b = w.GetBlock(pos.Offset(Dir.Down)); return b.sturdy || b is FenceBlock; }
        public override bool CanPlaceAt(World w, Int3 pos, ushort state) => CanSurvive(w, pos, 0);
        public override bool IsRedstoneSource(int meta) => true;
        public override int GetWeakPower(World w, Int3 pos, int meta, Dir towards) => meta;
        public override int GetStrongPower(World w, Int3 pos, int meta, Dir towards) => towards == Dir.Down ? meta : 0;
        int ComputeSignal(World w, Int3 pos)
        {
            var box = new AABB(pos.x + 0.125f, pos.y, pos.z + 0.125f, pos.x + 0.875f, pos.y + 0.25f, pos.z + 0.875f);
            var ents = w.GetEntities(box);
            int n = 0;
            foreach (var e in ents)
            {
                if (kind == Kind.Stone && !(e is LivingEntity)) continue;
                if (e is Player p && p.IsSpectator) continue;
                n++;
            }
            if (kind == Kind.Light) return Mathf.Min(15, n);
            if (kind == Kind.Heavy) return Mathf.Min(15, (n + 9) / 10);
            return n > 0 ? 15 : 0;
        }
        public override void OnEntityInside(World w, Int3 pos, int meta, Entity e)
        {
            if (meta > 0) return;
            UpdateSignal(w, pos, meta);
        }
        void UpdateSignal(World w, Int3 pos, int meta)
        {
            int s = ComputeSignal(w, pos);
            if (s != meta)
            {
                w.SetState(pos, State(s), SetFlags.Notify);
                w.NotifyNeighbors(pos.Offset(Dir.Down));
                Sounds.Play(s > 0 ? "block.pressure_plate.click_on" : "block.pressure_plate.click_off", pos.Center, 0.3f, s > 0 ? 0.6f : 0.5f);
            }
            if (s > 0) w.ScheduleTick(pos, this, 20);
        }
        public override void OnScheduledTick(World w, Int3 pos, int meta) { if (meta > 0) UpdateSignal(w, pos, meta); }
    }

    // ============================================================================ Repeater / comparator
    /// <summary>meta: facing(0-1: direction of output) | delay-1 (2-3) | powered(4)</summary>
    public class RepeaterBlock : Block
    {
        int topOff, topOn, torchOff, torchOn, slab;
        public RepeaterBlock()
        {
            stateCount = 32; opaqueCube = false; lightOpacity = 0; hardness = 0; isFullCubeShape = false; layer = RenderLayer.Cutout;
            topOff = Tex.Id("repeater"); topOn = Tex.Id("repeater_on"); torchOff = Tex.Id("redstone_torch_off"); torchOn = Tex.Id("redstone_torch");
            slab = Tex.Id("smooth_stone"); SetAllTex(topOff); creativeTab = CreativeTab.Redstone; sound = SoundType.Wood; push = PushReaction.Destroy;
        }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public static bool Powered(int m) => (m & 16) != 0;
        public static int Delay(int m) => ((m >> 2) & 3) + 1;
        public override void Emit(MeshCtx ctx, int meta)
        {
            int save = ctx.rot; ctx.rot = meta & 3;
            var t = new[] { slab, Powered(meta) ? topOn : topOff, slab, slab, slab, slab };
            ctx.Box(0, 0, 0, 1, 2f / 16, 1, t, 0xFFFFFFFFu);
            int torch = Powered(meta) ? torchOn : torchOff;
            var uv = new Vector4[6];
            for (int i = 2; i < 6; i++) uv[i] = new Vector4(7 / 16f, 6 / 16f, 9 / 16f, 11 / 16f);
            uv[1] = new Vector4(7 / 16f, 8 / 16f, 9 / 16f, 10 / 16f); uv[0] = uv[1];
            // front torch fixed (canonical output toward north +Z), back torch moves with delay
            ctx.XBoxPx(Matrix4x4.identity, 7, 2, 11, 9, 7, 13, torch, 0xFFFFFFFFu, RenderLayer.Cutout, uv);
            float bz = 5 - (Delay(meta) - 1) * 2;
            ctx.XBoxPx(Matrix4x4.identity, 7, 2, bz, 9, 7, bz + 2, torch, 0xFFFFFFFFu, RenderLayer.Cutout, uv);
            ctx.rot = save;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 2, 16));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 2, 16));
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(ctx.playerFacing));
        public override bool CanSurvive(World w, Int3 pos, int meta) => w.IsSturdy(pos.Offset(Dir.Down), Dir.Up);
        public override bool CanPlaceAt(World w, Int3 pos, ushort state) => CanSurvive(w, pos, 0);
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            int d = ((meta >> 2) & 3) + 1; d &= 3;
            w.SetState(pos, State((meta & ~12) | (d << 2)), 0);
            Sounds.Play("block.lever.click", pos.Center, 0.2f, 0.8f);
            return true;
        }
        public override bool IsRedstoneSource(int meta) => true;
        public override bool ConnectsToRedstone(int meta, Dir side) => DirUtil.Axis(side) == DirUtil.Axis(StateBits.FacingDir(meta));
        public override int GetWeakPower(World w, Int3 pos, int meta, Dir towards) => Powered(meta) && towards == StateBits.FacingDir(meta) ? 15 : 0;
        public override int GetStrongPower(World w, Int3 pos, int meta, Dir towards) => GetWeakPower(w, pos, meta, towards);
        protected int Input(World w, Int3 pos, int meta)
        {
            Dir f = StateBits.FacingDir(meta);
            Int3 back = pos.Offset(DirUtil.Opposite(f));
            ushort s = w.GetState(back);
            var b = Blocks.ByState[s];
            if (b is RedstoneWireBlock) return s - b.baseState;
            return Redstone.EmittedPower(w, back, f);
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (!CanSurvive(w, pos, meta)) { w.BreakBlock(pos, true, null); return; }
            bool shouldPower = Input(w, pos, meta) > 0;
            if (shouldPower != Powered(meta)) w.ScheduleTick(pos, this, Delay(meta) * 2);
        }
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            bool shouldPower = Input(w, pos, meta) > 0;
            if (shouldPower == Powered(meta)) return;
            int nm = shouldPower ? meta | 16 : meta & ~16;
            w.SetState(pos, State(nm), SetFlags.Hooks);
            Int3 front = pos.Offset(StateBits.FacingDir(meta));
            w.NotifyNeighbors(pos);
            w.NotifyNeighbors(front);
            if (!shouldPower) { } else w.ScheduleTick(pos, this, Delay(meta) * 2);
        }
        public override void OnAdded(World w, Int3 pos, int meta, ushort oldState) => w.ScheduleTick(pos, this, 1);
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) => drops.Add(new ItemStack(item, 1));
    }

    /// <summary>meta: facing(0-1) | subtract(2) | powered(3). Output level kept in block entity.</summary>
    public class ComparatorBlock : Block
    {
        int topOff, topOn, torchOff, torchOn, slab;
        public ComparatorBlock()
        {
            stateCount = 16; opaqueCube = false; lightOpacity = 0; hardness = 0; isFullCubeShape = false; layer = RenderLayer.Cutout;
            topOff = Tex.Id("comparator"); topOn = Tex.Id("comparator_on"); torchOff = Tex.Id("redstone_torch_off"); torchOn = Tex.Id("redstone_torch");
            slab = Tex.Id("smooth_stone"); SetAllTex(topOff); creativeTab = CreativeTab.Redstone; sound = SoundType.Wood; push = PushReaction.Destroy;
        }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new ComparatorEntity();
        public override void Emit(MeshCtx ctx, int meta)
        {
            int save = ctx.rot; ctx.rot = meta & 3;
            bool on = (meta & 8) != 0;
            var t = new[] { slab, on ? topOn : topOff, slab, slab, slab, slab };
            ctx.Box(0, 0, 0, 1, 2f / 16, 1, t, 0xFFFFFFFFu);
            var uv = new Vector4[6];
            for (int i = 2; i < 6; i++) uv[i] = new Vector4(7 / 16f, 6 / 16f, 9 / 16f, 11 / 16f);
            uv[1] = new Vector4(7 / 16f, 8 / 16f, 9 / 16f, 10 / 16f); uv[0] = uv[1];
            ctx.XBoxPx(Matrix4x4.identity, 4, 2, 3, 6, 7, 5, on ? torchOn : torchOff, 0xFFFFFFFFu, RenderLayer.Cutout, uv);
            ctx.XBoxPx(Matrix4x4.identity, 10, 2, 3, 12, 7, 5, on ? torchOn : torchOff, 0xFFFFFFFFu, RenderLayer.Cutout, uv);
            bool sub = (meta & 4) != 0;
            ctx.XBoxPx(Matrix4x4.identity, 7, 2, 11, 9, sub ? 6 : 4, 13, sub ? torchOn : torchOff, 0xFFFFFFFFu, RenderLayer.Cutout, uv);
            ctx.rot = save;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 2, 16));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 2, 16));
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(ctx.playerFacing));
        public override bool CanSurvive(World w, Int3 pos, int meta) => w.IsSturdy(pos.Offset(Dir.Down), Dir.Up);
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            w.SetState(pos, State(meta ^ 4), 0);
            Sounds.Play("block.lever.click", pos.Center, 0.2f, (meta & 4) != 0 ? 0.5f : 0.55f);
            w.ScheduleTick(pos, this, 2);
            return true;
        }
        public override bool IsRedstoneSource(int meta) => true;
        public int Output(World w, Int3 pos) => w.GetBlockEntity(pos) is ComparatorEntity ce ? ce.output : 0;
        public override int GetWeakPower(World w, Int3 pos, int meta, Dir towards) => towards == StateBits.FacingDir(meta) ? Output(w, pos) : 0;
        public override int GetStrongPower(World w, Int3 pos, int meta, Dir towards) => GetWeakPower(w, pos, meta, towards);
        int Compute(World w, Int3 pos, int meta)
        {
            Dir f = StateBits.FacingDir(meta);
            Int3 back = pos.Offset(DirUtil.Opposite(f));
            int input;
            var bb = w.GetBlock(back);
            int co = bb.GetComparatorOutput(w, back, w.GetMeta(back));
            if (co >= 0) input = co;
            else if (Redstone.IsConductor(bb))
            {
                Int3 back2 = back.Offset(DirUtil.Opposite(f));
                var b2 = w.GetBlock(back2);
                int co2 = b2.GetComparatorOutput(w, back2, w.GetMeta(back2));
                input = co2 >= 0 ? co2 : Redstone.EmittedPower(w, back, f);
            }
            else if (bb is RedstoneWireBlock) input = w.GetMeta(back);
            else input = Redstone.EmittedPower(w, back, f);
            int side = 0;
            foreach (var sd in new[] { DirUtil.RotateCW(f), DirUtil.RotateCCW(f) })
            {
                Int3 sp = pos.Offset(sd);
                ushort ss = w.GetState(sp);
                var sb = Blocks.ByState[ss];
                if (sb is RedstoneWireBlock) side = Mathf.Max(side, ss - sb.baseState);
                else if (sb is RedstoneBlock) side = 15;
                else if (sb is RepeaterBlock || sb is ComparatorBlock) side = Mathf.Max(side, sb.GetWeakPower(w, sp, ss - sb.baseState, DirUtil.Opposite(sd)));
            }
            if ((meta & 4) != 0) return Mathf.Max(0, input - side);
            return input >= side ? input : 0;
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (!CanSurvive(w, pos, meta)) { w.BreakBlock(pos, true, null); return; }
            w.ScheduleTick(pos, this, 2);
        }
        public override void OnAdded(World w, Int3 pos, int meta, ushort oldState) => w.ScheduleTick(pos, this, 2);
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            int o = Compute(w, pos, meta);
            var ce = w.GetBlockEntity(pos) as ComparatorEntity;
            if (ce == null) return;
            if (o == ce.output) return;
            ce.output = o;
            int nm = o > 0 ? meta | 8 : meta & ~8;
            if (nm != meta) w.SetState(pos, State(nm), SetFlags.Hooks);
            w.NotifyNeighbors(pos);
            w.NotifyNeighbors(pos.Offset(StateBits.FacingDir(meta)));
        }
    }

    public class ComparatorEntity : BlockEntity
    {
        public int output;
        public override void Save(Dictionary<string, string> d) => d["o"] = output.ToString();
        public override void Load(Dictionary<string, string> d) { if (d.TryGetValue("o", out var v)) int.TryParse(v, out output); }
    }

    // ============================================================================ Lamp / TNT / target / note block / daylight
    public class RedstoneLampBlock : Block
    {
        int offTex, onTex;
        public RedstoneLampBlock() { stateCount = 2; offTex = Tex.Id("redstone_lamp"); onTex = Tex.Id("redstone_lamp_on"); SetAllTex(offTex); hardness = 0.3f; sound = SoundType.Glass; creativeTab = CreativeTab.Redstone; }
        public override byte GetLightEmission(int meta) => (byte)(meta == 1 ? 15 : 0);
        readonly int[] onArr = new int[6];
        public override void Emit(MeshCtx ctx, int meta)
        {
            if (meta == 1) { for (int i = 0; i < 6; i++) onArr[i] = onTex; ctx.Cube(onArr, TintType.None); }
            else ctx.Cube(faceTex, TintType.None);
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            bool p = Redstone.IsPowered(w, pos);
            if (p && meta == 0) w.SetState(pos, State(1), SetFlags.Hooks);
            else if (!p && meta == 1) w.ScheduleTick(pos, this, 4);
        }
        public override void OnAdded(World w, Int3 pos, int meta, ushort oldState) => OnNeighborChanged(w, pos, meta, pos);
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            if (meta == 1 && !Redstone.IsPowered(w, pos)) w.SetState(pos, State(0), SetFlags.Hooks);
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) => drops.Add(new ItemStack(item, 1));
    }

    public class CopperBulbBlock : Block
    {
        public int stage;
        int offTex, onTex;
        static readonly string[] Pre = { "", "exposed_", "weathered_", "oxidized_" };
        public CopperBulbBlock(int stage)
        {
            this.stage = stage; stateCount = 4; // lit(0) powered(1)
            offTex = Tex.Id(Pre[stage] + "copper_bulb"); onTex = Tex.Id(Pre[stage] + "copper_bulb_lit"); SetAllTex(offTex);
        }
        public override byte GetLightEmission(int meta) => (meta & 1) != 0 ? (byte)(stage == 0 ? 15 : stage == 1 ? 12 : stage == 2 ? 8 : 4) : (byte)0;
        readonly int[] onArr = new int[6];
        public override void Emit(MeshCtx ctx, int meta)
        {
            if ((meta & 1) != 0) { for (int i = 0; i < 6; i++) onArr[i] = onTex; ctx.Cube(onArr, TintType.None); }
            else ctx.Cube(faceTex, TintType.None);
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            bool p = Redstone.IsPowered(w, pos);
            bool was = (meta & 2) != 0;
            if (p == was) return;
            int nm = p ? meta | 2 : meta & ~2;
            if (p) { nm ^= 1; Sounds.Play("block.copper_bulb.toggle", pos.Center, 0.6f, 1f); }
            w.SetState(pos, State(nm), SetFlags.Hooks);
        }
        public override int GetComparatorOutput(World w, Int3 pos, int meta) => (meta & 1) != 0 ? 15 : 0;
    }

    public class TntBlock : Block
    {
        public TntBlock() { this.T3("tnt_top", "tnt_bottom", "tnt_side"); hardness = 0; sound = SoundType.Grass; flammability = 15; fireSpread = 100; creativeTab = CreativeTab.Redstone; }
        public void Ignite(World w, Int3 pos, Entity igniter, int fuse = 80)
        {
            w.SetState(pos, 0);
            PrimedTnt.Spawn(w, pos.Center - new Vector3(0, 0.5f, 0), fuse, igniter);
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (Redstone.IsPowered(w, pos)) Ignite(w, pos, null);
        }
        public override void OnAdded(World w, Int3 pos, int meta, ushort oldState)
        {
            if (Redstone.IsPowered(w, pos)) Ignite(w, pos, null);
        }
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            var held = player.MainHand;
            if (held != null && (held.item.id == "flint_and_steel" || held.item.id == "fire_charge"))
            {
                Ignite(w, pos, player);
                if (!player.IsCreative) { if (held.item.id == "flint_and_steel") held.HurtAndBreak(1, player); else held.count--; }
                return true;
            }
            return false;
        }
        public override void OnExploded(World w, Int3 pos, int meta)
        {
            w.SetState(pos, 0, SetFlags.NoRecord | SetFlags.Hooks);
            PrimedTnt.Spawn(w, pos.Center - new Vector3(0, 0.5f, 0), 10 + w.rand.Next(20), null);
        }
        public override void OnProjectileHit(World w, Int3 pos, int meta, Entity projectile)
        {
            if (projectile.onFire) Ignite(w, pos, projectile);
        }
    }

    public class TargetBlock : Block
    {
        public TargetBlock() { stateCount = 16; this.T2("target_top", "target_side"); hardness = 0.5f; tool = ToolType.Hoe; sound = SoundType.Grass; creativeTab = CreativeTab.Redstone; }
        public override bool IsRedstoneSource(int meta) => true;
        public override int GetWeakPower(World w, Int3 pos, int meta, Dir towards) => meta;
        public override void OnProjectileHit(World w, Int3 pos, int meta, Entity projectile)
        {
            Vector3 c = pos.Center;
            Vector3 hp = projectile.position;
            float d = Mathf.Max(Mathf.Abs(hp.x - c.x), Mathf.Abs(hp.y - c.y), Mathf.Abs(hp.z - c.z));
            int power = Mathf.Clamp(Mathf.CeilToInt(15 * Mathf.Clamp01((0.5f - d) / 0.5f)), 1, 15);
            w.SetState(pos, State(power), SetFlags.Notify);
            Redstone.NotifyAround(w, pos);
            w.ScheduleTick(pos, this, projectile is Arrow ? 20 : 8);
        }
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            if (meta != 0) { w.SetState(pos, State(0), SetFlags.Notify); Redstone.NotifyAround(w, pos); }
        }
    }

    public class NoteBlock : Block
    {
        public NoteBlock() { stateCount = 50; this.T1("note_block"); hardness = 0.8f; tool = ToolType.Axe; sound = SoundType.Wood; creativeTab = CreativeTab.Redstone; flammability = 5; fireSpread = 20; }
        public static int Note(int m) => m % 25;
        public static bool Powered(int m) => m >= 25;
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            int n = (Note(meta) + 1) % 25;
            w.SetState(pos, State(n + (Powered(meta) ? 25 : 0)), 0);
            Play(w, pos, n);
            return true;
        }
        public override void OnAttack(World w, Int3 pos, int meta, Player player) => Play(w, pos, Note(meta));
        public void Play(World w, Int3 pos, int note)
        {
            if (!w.IsAir(pos.Offset(Dir.Up))) return;
            string inst = Instrument(w.GetBlock(pos.Offset(Dir.Down)));
            float pitch = Mathf.Pow(2f, (note - 12) / 12f);
            Sounds.Play("note." + inst, pos.Center, 3f, pitch);
            Particles.Note(w, pos.Center + Vector3.up * 0.7f, note / 24f);
        }
        static string Instrument(Block b)
        {
            string id = b.id;
            if (b.sound == SoundType.Wood) return "bass";
            if (id.Contains("sand") || id == "gravel" || id.Contains("concrete_powder")) return "snare";
            if (id.Contains("glass") || id == "sea_lantern" || id == "beacon") return "hat";
            if (b.sound == SoundType.Stone || b.sound == SoundType.Deepslate || b.sound == SoundType.Netherrack) return "basedrum";
            if (id == "gold_block") return "bell";
            if (id == "clay") return "flute";
            if (id == "packed_ice") return "chime";
            if (id.EndsWith("_wool")) return "guitar";
            if (id == "bone_block") return "xylophone";
            if (id == "iron_block") return "iron_xylophone";
            if (id == "soul_sand") return "cow_bell";
            if (id == "pumpkin") return "didgeridoo";
            if (id == "emerald_block") return "bit";
            if (id == "hay_block") return "banjo";
            if (id == "glowstone") return "pling";
            return "harp";
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            bool p = Redstone.IsPowered(w, pos);
            if (p == Powered(meta)) return;
            if (p) Play(w, pos, Note(meta));
            w.SetState(pos, State(Note(meta) + (p ? 25 : 0)), 0);
        }
    }

    public class DaylightDetectorBlock : Block
    {
        int topTex, topInv, side;
        public DaylightDetectorBlock()
        {
            stateCount = 32; opaqueCube = false; lightOpacity = 0; hardness = 0.2f; tool = ToolType.Axe; sound = SoundType.Wood;
            topTex = Tex.Id("daylight_detector_top"); topInv = Tex.Id("daylight_detector_inverted_top"); side = Tex.Id("daylight_detector_side");
            SetAllTex(side); creativeTab = CreativeTab.Redstone; isFullCubeShape = false;
        }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public override void Emit(MeshCtx ctx, int meta)
        {
            var t = new[] { side, meta >= 16 ? topInv : topTex, side, side, side, side };
            ctx.Box(0, 0, 0, 1, 6f / 16, 1, t, 0xFFFFFFFFu);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 6, 16));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 6, 16));
        public override bool IsRedstoneSource(int meta) => true;
        public override int GetWeakPower(World w, Int3 pos, int meta, Dir towards) => meta & 15;
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            w.SetState(pos, State(meta ^ 16), SetFlags.Notify);
            w.ScheduleTick(pos, this, 1);
            return true;
        }
        public override bool HasBlockEntity => true;
        public override BlockEntity CreateBlockEntity(World w, Int3 pos) => new DaylightDetectorEntity();
        public override void OnScheduledTick(World w, Int3 pos, int meta) => UpdatePower(w, pos, meta);
        public void UpdatePower(World w, Int3 pos, int meta)
        {
            int sky = w.GetSkyLight(pos);
            float day = w.session != null ? w.session.DaylightFactor(w) : 1f;
            int p = Mathf.Clamp(Mathf.RoundToInt(sky * day), 0, 15);
            if (meta >= 16) p = 15 - p;
            int nm = (meta & 16) | p;
            if (nm != meta) { w.SetState(pos, State(nm), SetFlags.Notify); w.NotifyNeighbors(pos.Offset(Dir.Down)); }
        }
    }

    public class DaylightDetectorEntity : BlockEntity
    {
        public DaylightDetectorEntity() { ticks = true; }
        public override void Tick()
        {
            if (world.tickCount % 20 != 0) return;
            var b = world.GetBlock(pos) as DaylightDetectorBlock;
            b?.UpdatePower(world, pos, world.GetMeta(pos));
        }
    }

    // ============================================================================ Observer
    /// <summary>meta: facing (0-5, the direction the observer's face looks) | powered(3)</summary>
    public class ObserverBlock : Block
    {
        int front, back, backOn, side, top;
        public ObserverBlock()
        {
            stateCount = 16; hardness = 3; tool = ToolType.Pickaxe; requiresTool = true; creativeTab = CreativeTab.Redstone;
            front = Tex.Id("observer_front"); back = Tex.Id("observer_back"); backOn = Tex.Id("observer_back_on"); side = Tex.Id("observer_side"); top = Tex.Id("observer_top");
            SetAllTex(side);
        }
        public static Dir Facing(int m) => (Dir)(m & 7);
        public static bool Powered(int m) => (m & 8) != 0;
        readonly int[] t = new int[6];
        public override void Emit(MeshCtx ctx, int meta)
        {
            Dir f = Facing(meta);
            for (int i = 0; i < 6; i++) t[i] = DirUtil.Axis((Dir)i) == DirUtil.Axis(f) ? top : side;
            if (DirUtil.Axis(f) != 1) { t[0] = top; t[1] = top; }
            t[(int)f] = front; t[(int)DirUtil.Opposite(f)] = Powered(meta) ? backOn : back;
            ctx.Cube(t, TintType.None);
        }
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            // face looks toward where the player is looking from (observer front faces the player's look direction)
            Vector3 look = MathX.YawPitchToDir(ctx.playerYaw, ctx.playerPitch);
            Dir d = DirUtil.FromVector(look);
            return State((int)d);
        }
        public override bool IsRedstoneSource(int meta) => true;
        public override int GetWeakPower(World w, Int3 pos, int meta, Dir towards) => Powered(meta) && towards == DirUtil.Opposite(Facing(meta)) ? 15 : 0;
        public override int GetStrongPower(World w, Int3 pos, int meta, Dir towards) => GetWeakPower(w, pos, meta, towards);
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (fromPos == pos.Offset(Facing(meta)) && !Powered(meta)) w.ScheduleTick(pos, this, 2);
        }
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            Int3 backPos = pos.Offset(DirUtil.Opposite(Facing(meta)));
            if (!Powered(meta))
            {
                w.SetState(pos, State(meta | 8), SetFlags.Hooks);
                w.NotifyNeighbors(backPos); w.UpdateBlock(backPos, pos);
                w.ScheduleTick(pos, this, 2);
            }
            else
            {
                w.SetState(pos, State(meta & ~8), SetFlags.Hooks);
                w.NotifyNeighbors(backPos); w.UpdateBlock(backPos, pos);
            }
        }
    }

    // ============================================================================ Pistons
    /// <summary>meta: facing(0-5) | extended(3)</summary>
    public class PistonBlock : Block
    {
        public bool sticky;
        int topTex, topSticky, sideTex, bottomTex, innerTex;
        public PistonBlock(bool sticky)
        {
            this.sticky = sticky; stateCount = 16; hardness = 1.5f; creativeTab = CreativeTab.Redstone; opaqueCube = true;
            topTex = Tex.Id("piston_top"); topSticky = Tex.Id("piston_top_sticky"); sideTex = Tex.Id("piston_side"); bottomTex = Tex.Id("piston_bottom"); innerTex = Tex.Id("piston_inner");
            SetAllTex(sideTex); push = PushReaction.Normal;
        }
        public static Dir Facing(int m) => (Dir)(m & 7);
        public static bool Extended(int m) => (m & 8) != 0;
        public override bool IsOpaqueCube(int meta) => !Extended(meta);
        public override byte GetLightOpacity(int meta) => (byte)(Extended(meta) ? 0 : 15);
        public override int GetOccludingFaces(int meta) => Extended(meta) ? 1 << (int)DirUtil.Opposite(Facing(meta)) : 0x3F;
        public override void Emit(MeshCtx ctx, int meta)
        {
            Dir f = Facing(meta);
            int[] t = new int[6];
            int[] r = new int[6];
            for (int i = 0; i < 6; i++) t[i] = sideTex;
            t[(int)f] = Extended(meta) ? innerTex : (sticky ? topSticky : topTex);
            t[(int)DirUtil.Opposite(f)] = bottomTex;
            // rotate side textures so the "head" edge points toward facing
            for (int i = 0; i < 6; i++) r[i] = PistonUVRot((Dir)i, f);
            if (!Extended(meta)) { ctx.CubeRot(t, r, 0xFFFFFFFFu); return; }
            // extended: base is 12px deep
            Vector3 n = DirUtil.Normal[(int)f];
            Vector3 mn = new Vector3(n.x > 0 ? 0 : (n.x < 0 ? 4f / 16 : 0), n.y > 0 ? 0 : (n.y < 0 ? 4f / 16 : 0), n.z > 0 ? 0 : (n.z < 0 ? 4f / 16 : 0));
            Vector3 mx = new Vector3(n.x > 0 ? 12f / 16 : 1, n.y > 0 ? 12f / 16 : 1, n.z > 0 ? 12f / 16 : 1);
            ctx.Box(mn.x, mn.y, mn.z, mx.x, mx.y, mx.z, t, 0xFFFFFFFFu, 0, true, RenderLayer.Opaque, null, r);
        }
        public static int PistonUVRot(Dir face, Dir facing)
        {
            if (face == facing || face == DirUtil.Opposite(facing)) return 0;
            if (facing == Dir.Up) return 0;
            if (facing == Dir.Down) return 2;
            if (face == Dir.Up || face == Dir.Down)
            {
                int hi = DirUtil.HorizIndex(facing);
                return face == Dir.Up ? (4 - hi) & 3 : hi & 3;
            }
            // side faces with horizontal facing: rotate 90 so top edge points to facing
            Dir right = DirUtil.RotateCW(face);
            return facing == right ? 3 : 1;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            if (!Extended(meta)) { boxes.Add(new AABB(0, 0, 0, 1, 1, 1)); return; }
            Dir f = Facing(meta);
            Vector3 n = DirUtil.Normal[(int)f];
            boxes.Add(new AABB(n.x < 0 ? 0.25f : 0, n.y < 0 ? 0.25f : 0, n.z < 0 ? 0.25f : 0, n.x > 0 ? 0.75f : 1, n.y > 0 ? 0.75f : 1, n.z > 0 ? 0.75f : 1));
        }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            Vector3 look = MathX.YawPitchToDir(ctx.playerYaw, ctx.playerPitch);
            return State((int)DirUtil.Opposite(DirUtil.FromVector(look)));
        }
        public override void OnAdded(World w, Int3 pos, int meta, ushort oldState) => CheckPower(w, pos, meta);
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos) => CheckPower(w, pos, meta);
        public override void OnPlaced(World w, Int3 pos, int meta, Entity placer, ItemStack stack) => CheckPower(w, pos, meta);
        void CheckPower(World w, Int3 pos, int meta)
        {
            Dir f = Facing(meta);
            bool powered = false;
            for (int d = 0; d < 6; d++)
            {
                if ((Dir)d == f) continue;
                Int3 n = pos.Offset((Dir)d);
                if (Redstone.EmittedPower(w, n, DirUtil.Opposite((Dir)d)) > 0) { powered = true; break; }
            }
            if (powered && !Extended(meta)) w.ScheduleTick(pos, this, 1);
            else if (!powered && Extended(meta)) w.ScheduleTick(pos, this, 1);
        }
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            Dir f = Facing(meta);
            bool powered = false;
            for (int d = 0; d < 6; d++)
            {
                if ((Dir)d == f) continue;
                if (Redstone.EmittedPower(w, pos.Offset((Dir)d), DirUtil.Opposite((Dir)d)) > 0) { powered = true; break; }
            }
            if (powered && !Extended(meta))
            {
                if (Pistons.Push(w, pos, f, true))
                {
                    w.SetState(pos, State(meta | 8), SetFlags.Hooks);
                    var head = (PistonHeadBlock)Blocks.Get("piston_head");
                    w.SetState(pos.Offset(f), head.State((int)f | (sticky ? 8 : 0)), SetFlags.Hooks);
                    Sounds.Play("block.piston.extend", pos.Center, 0.5f, 0.7f + Random.value * 0.2f);
                    w.NotifyNeighbors(pos.Offset(f));
                }
            }
            else if (!powered && Extended(meta))
            {
                Int3 hp = pos.Offset(f);
                if (w.GetBlock(hp) is PistonHeadBlock) w.SetState(hp, 0, SetFlags.Hooks);
                w.SetState(pos, State(meta & ~8), SetFlags.Hooks);
                if (sticky) Pistons.Pull(w, pos, f);
                Sounds.Play("block.piston.contract", pos.Center, 0.5f, 0.6f + Random.value * 0.2f);
                w.NotifyNeighbors(hp);
            }
        }
        public override void OnRemoved(World w, Int3 pos, int meta, ushort newState)
        {
            if (Extended(meta))
            {
                Int3 hp = pos.Offset(Facing(meta));
                w.session?.Defer(() => { if (w.GetBlock(hp) is PistonHeadBlock) w.SetState(hp, 0, SetFlags.Hooks); });
            }
        }
    }

    public class PistonHeadBlock : Block
    {
        int topTex, topSticky, sideTex;
        public PistonHeadBlock()
        {
            stateCount = 16; opaqueCube = false; lightOpacity = 0; hardness = 1.5f; noItem = true; hiddenInCreative = true; push = PushReaction.Block;
            topTex = Tex.Id("piston_top"); topSticky = Tex.Id("piston_top_sticky"); sideTex = Tex.Id("piston_side"); SetAllTex(sideTex); isFullCubeShape = false;
        }
        public override int GetOccludingFaces(int meta) => 1 << (meta & 7);
        public override void Emit(MeshCtx ctx, int meta)
        {
            Dir f = (Dir)(meta & 7);
            bool st = (meta & 8) != 0;
            Vector3 n = DirUtil.Normal[(int)f];
            int[] t = new int[6]; int[] r = new int[6];
            for (int i = 0; i < 6; i++) { t[i] = sideTex; r[i] = PistonBlock.PistonUVRot((Dir)i, f); }
            t[(int)f] = st ? topSticky : topTex; t[(int)DirUtil.Opposite(f)] = topTex;
            // plate 4px thick at facing side
            Vector3 mn = new Vector3(n.x > 0 ? 12f / 16 : 0, n.y > 0 ? 12f / 16 : 0, n.z > 0 ? 12f / 16 : 0);
            Vector3 mx = new Vector3(n.x < 0 ? 4f / 16 : 1, n.y < 0 ? 4f / 16 : 1, n.z < 0 ? 4f / 16 : 1);
            ctx.Box(mn.x, mn.y, mn.z, mx.x, mx.y, mx.z, t, 0xFFFFFFFFu, 0, true, RenderLayer.Opaque, null, r);
            // rod 4x4 extending back (and 4px into the base block)
            Vector3 c0 = new Vector3(6f / 16, 6f / 16, 6f / 16), c1 = new Vector3(10f / 16, 10f / 16, 10f / 16);
            int a = DirUtil.Axis(f);
            float r0 = n[a] > 0 ? -4f / 16 : 4f / 16, r1 = n[a] > 0 ? 12f / 16 : 20f / 16;
            c0[a] = Mathf.Min(r0, r1); c1[a] = Mathf.Max(r0, r1);
            if (n[a] > 0) { c0[a] = -4f / 16; c1[a] = 12f / 16; } else { c0[a] = 4f / 16; c1[a] = 20f / 16; }
            ctx.Box(c0.x, c0.y, c0.z, c1.x, c1.y, c1.z, new[] { sideTex, sideTex, sideTex, sideTex, sideTex, sideTex }, 0xFFFFFFFFu, 0, false, RenderLayer.Opaque, null, r);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            Dir f = (Dir)(meta & 7);
            Vector3 n = DirUtil.Normal[(int)f];
            boxes.Add(new AABB(n.x > 0 ? 0.75f : 0, n.y > 0 ? 0.75f : 0, n.z > 0 ? 0.75f : 0, n.x < 0 ? 0.25f : 1, n.y < 0 ? 0.25f : 1, n.z < 0 ? 0.25f : 1));
            boxes.Add(new AABB(0.375f, 0.375f, 0.375f, 0.625f, 0.625f, 0.625f));
        }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            Int3 basePos = pos.Offset(DirUtil.Opposite((Dir)(meta & 7)));
            return w.GetBlock(basePos) is PistonBlock pb && PistonBlock.Extended(w.GetMeta(basePos));
        }
        public override void OnBroken(World w, Int3 pos, int meta, Entity breaker)
        {
            Int3 basePos = pos.Offset(DirUtil.Opposite((Dir)(meta & 7)));
            if (w.GetBlock(basePos) is PistonBlock) w.BreakBlock(basePos, !(breaker is Player p && p.IsCreative), breaker);
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) { }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (!CanSurvive(w, pos, meta)) w.SetState(pos, 0, SetFlags.Hooks);
        }
    }

    public static class Pistons
    {
        public static bool Movable(World w, Int3 p, Block b, int meta, Dir moveDir)
        {
            if (b.isAir) return true;
            if (b.hardness < 0 || b.id == "obsidian" || b.id == "crying_obsidian" || b.id == "reinforced_deepslate" || b.id == "end_portal_frame" || b.id == "respawn_anchor") return false;
            if (b.push == PushReaction.Block) return false;
            if (b is PistonBlock && PistonBlock.Extended(meta)) return false;
            if (b.HasBlockEntity && !(b is ComparatorBlock || b is DaylightDetectorBlock)) return false;
            if (p.y + DirUtil.Offset[(int)moveDir].y < w.minY || p.y + DirUtil.Offset[(int)moveDir].y >= w.maxY) return false;
            return true;
        }

        public static bool Push(World w, Int3 piston, Dir f, bool extending)
        {
            var line = new List<Int3>();
            Int3 p = piston.Offset(f);
            for (int i = 0; i <= 12; i++)
            {
                ushort s = w.GetState(p);
                var b = Blocks.ByState[s];
                if (b.isAir || b.isLiquid) break;
                if (b.push == PushReaction.Destroy || b.replaceable) break;
                if (!Movable(w, p, b, s - b.baseState, f)) return false;
                if (i == 12) return false;
                line.Add(p);
                p = p.Offset(f);
            }
            Int3 end = p;
            ushort endS = w.GetState(end);
            var endB = Blocks.ByState[endS];
            if (!endB.isAir && !endB.isLiquid && (endB.push == PushReaction.Destroy || endB.replaceable)) w.BreakBlock(end, true, null);
            // move from far to near
            var states = new ushort[line.Count];
            for (int i = 0; i < line.Count; i++) states[i] = w.GetState(line[i]);
            for (int i = line.Count - 1; i >= 0; i--)
            {
                Int3 to = line[i].Offset(f);
                w.SetState(to, states[i], SetFlags.Hooks);
            }
            for (int i = 0; i < line.Count; i++) if (i == 0) w.SetState(line[i], 0, SetFlags.Hooks);
            // entities in front are pushed
            var box = new AABB(piston.Offset(f).ToVector3(), piston.Offset(f).ToVector3() + Vector3.one).Expand(DirUtil.Normal[(int)f] * (line.Count + 0.5f));
            foreach (var e in w.GetEntities(box)) e.Move(DirUtil.Normal[(int)f] * 1.01f);
            foreach (var l in line) { w.NotifyNeighbors(l); w.NotifyNeighbors(l.Offset(f)); }
            return true;
        }

        public static void Pull(World w, Int3 piston, Dir f)
        {
            Int3 from = piston.Offset(f).Offset(f);
            ushort s = w.GetState(from);
            var b = Blocks.ByState[s];
            if (b.isAir || b.isLiquid || !Movable(w, from, b, s - b.baseState, DirUtil.Opposite(f)) || b.push == PushReaction.Destroy || b.push == PushReaction.PushOnly) return;
            w.SetState(from, 0, SetFlags.Hooks);
            w.SetState(piston.Offset(f), s, SetFlags.Hooks);
            w.NotifyNeighbors(from); w.NotifyNeighbors(piston.Offset(f));
        }
    }
}
