using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    // ============================================================================ Doors
    /// <summary>meta: facing(0-1) | open(2) | upper(3) | hingeRight(4)</summary>
    public class DoorBlock : Block
    {
        public int topTex, bottomTex;
        public bool redstoneOnly;
        public DoorBlock(string texBase, bool iron)
        {
            stateCount = 32; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; isFullCubeShape = false;
            topTex = Tex.Id(texBase + "_top"); bottomTex = Tex.Id(texBase + "_bottom"); SetAllTex(bottomTex); particleTex = bottomTex;
            redstoneOnly = iron; hardness = iron ? 5 : 3; blastResistance = hardness; sound = iron ? SoundType.Metal : SoundType.Wood;
            tool = iron ? ToolType.Pickaxe : ToolType.Axe; requiresTool = iron; creativeTab = CreativeTab.Building; push = PushReaction.Destroy;
        }
        public static Dir Facing(int m) => DirUtil.FromHorizIndex(m & 3);
        public static bool Open(int m) => (m & 4) != 0;
        public static bool Upper(int m) => (m & 8) != 0;
        public static bool HingeRight(int m) => (m & 16) != 0;
        public override int GetOccludingFaces(int meta) => 0;

        /// <summary>Canonical frame: facing north (+Z). Closed door at south edge z∈[0,3/16]. Hinge left = west(-X).</summary>
        static AABB CanonicalBox(int m)
        {
            if (!Open(m)) return BoxUtil.Px(0, 0, 0, 16, 16, 3);
            return HingeRight(m) ? BoxUtil.Px(13, 0, 0, 16, 16, 16) : BoxUtil.Px(0, 0, 0, 3, 16, 16);
        }
        public override void Emit(MeshCtx ctx, int meta)
        {
            var b = CanonicalBox(meta);
            int t = Upper(meta) ? topTex : bottomTex;
            var tex = new[] { t, t, t, t, t, t };
            ctx.rot = DirUtil.HorizIndex(Facing(meta));
            // Flip UVs horizontally for right hinge so the handle is opposite the hinge
            ctx.Box(b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z, tex, 0xFFFFFFFFu, 0, true);
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Rot(CanonicalBox(meta), DirUtil.HorizIndex(Facing(meta))));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
        public override bool CanPlaceAt(World w, Int3 pos, ushort state)
        {
            return pos.y + 1 < w.maxY && w.GetBlock(pos.Offset(Dir.Up)).replaceable && w.IsSturdy(pos.Offset(Dir.Down), Dir.Up);
        }
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            Dir f = ctx.playerFacing;
            // hinge: next to another door facing same way -> opposite hinge; else choose by hit position
            Dir left = DirUtil.RotateCCW(f), right = DirUtil.RotateCW(f);
            bool hingeRight = false;
            var lb = ctx.world.GetBlock(ctx.pos.Offset(left));
            var rb = ctx.world.GetBlock(ctx.pos.Offset(right));
            if (lb is DoorBlock) hingeRight = true;
            else if (rb is DoorBlock) hingeRight = false;
            else
            {
                // hit position relative to player's right
                Vector3 r = DirUtil.Normal[(int)right];
                float side = (ctx.hitLocal.x - 0.5f) * r.x + (ctx.hitLocal.z - 0.5f) * r.z;
                hingeRight = side > 0;
                if (ctx.clickedFace != Dir.Up) hingeRight = false;
            }
            return State(DirUtil.HorizIndex(f) | (hingeRight ? 16 : 0));
        }
        public override void OnPlaced(World w, Int3 pos, int meta, Entity placer, ItemStack stack)
        {
            w.SetState(pos.Offset(Dir.Up), State(meta | 8), SetFlags.Hooks);
        }
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            if (Upper(meta)) return w.GetBlock(pos.Offset(Dir.Down)) == this;
            return w.GetBlock(pos.Offset(Dir.Up)) == this && w.IsSturdy(pos.Offset(Dir.Down), Dir.Up);
        }
        public override void OnBroken(World w, Int3 pos, int meta, Entity breaker)
        {
            Int3 other = Upper(meta) ? pos.Offset(Dir.Down) : pos.Offset(Dir.Up);
            if (w.GetBlock(other) == this) w.SetState(other, 0, SetFlags.Hooks);
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if (Upper(meta)) return;
            if (requiresTool && !CanHarvestWith(tool)) return;
            drops.Add(new ItemStack(item, 1));
        }
        public void SetOpen(World w, Int3 pos, int meta, bool open)
        {
            if (Open(meta) == open) return;
            Int3 lower = Upper(meta) ? pos.Offset(Dir.Down) : pos;
            Int3 upper = lower.Offset(Dir.Up);
            int lm = w.GetMeta(lower), um = w.GetMeta(upper);
            lm = open ? lm | 4 : lm & ~4; um = open ? um | 4 : um & ~4;
            w.SetState(lower, State(lm), SetFlags.Notify);
            if (w.GetBlock(upper) == this) w.SetState(upper, State(um), SetFlags.Notify);
            Sounds.Play(redstoneOnly ? (open ? "block.iron_door.open" : "block.iron_door.close") : (open ? "block.door.open" : "block.door.close"), pos.Center, 1f, 0.9f + Random.value * 0.1f);
        }
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (redstoneOnly) return false;
            SetOpen(w, pos, meta, !Open(meta));
            return true;
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (!CanSurvive(w, pos, meta)) { w.BreakBlock(pos, !Upper(meta), null); return; }
            Int3 lower = Upper(meta) ? pos.Offset(Dir.Down) : pos;
            int lm = w.GetMeta(lower);
            if (!(w.GetBlock(lower) is DoorBlock)) return;
            bool powered = Redstone.IsPowered(w, lower) || Redstone.IsPowered(w, lower.Offset(Dir.Up));
            bool open = Open(lm);
            if (powered && !open) { SetOpen(w, lower, lm, true); Redstone.MarkPoweredOpen(lower); }
            else if (!powered && open && Redstone.WasPoweredOpen(w, lower)) SetOpen(w, lower, lm, false);
        }
        public override BlockItem CustomItem() => new TallBlockItem();
    }

    /// <summary>meta: hinge-facing(0-1) | open(2) | top(3)</summary>
    public class TrapdoorBlock : Block
    {
        public bool redstoneOnly;
        public TrapdoorBlock(string tex, bool iron)
        {
            stateCount = 16; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; isFullCubeShape = false;
            SetAllTex(Tex.Id(tex)); redstoneOnly = iron; hardness = iron ? 5 : 3; sound = iron ? SoundType.Metal : SoundType.Wood;
            tool = iron ? ToolType.Pickaxe : ToolType.Axe; requiresTool = iron; creativeTab = CreativeTab.Building;
        }
        public override int GetOccludingFaces(int meta) => 0;
        static AABB Canonical(int m)
        {
            bool open = (m & 4) != 0, top = (m & 8) != 0;
            if (!open) return top ? BoxUtil.Px(0, 13, 0, 16, 16, 16) : BoxUtil.Px(0, 0, 0, 16, 3, 16);
            return BoxUtil.Px(0, 0, 13, 16, 16, 16); // against the hinge wall at north(+Z) in canonical frame
        }
        public override void Emit(MeshCtx ctx, int meta)
        {
            var b = Canonical(meta);
            ctx.rot = meta & 3;
            ctx.Box(b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z, faceTex, 0xFFFFFFFFu);
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Rot(Canonical(meta), meta & 3));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            int m;
            if (DirUtil.IsHorizontal(ctx.clickedFace))
            {
                Dir hinge = DirUtil.Opposite(ctx.clickedFace);
                m = DirUtil.HorizIndex(hinge) | (ctx.hitLocal.y > 0.5f ? 8 : 0);
            }
            else m = DirUtil.HorizIndex(ctx.playerFacing) | (ctx.clickedFace == Dir.Down ? 8 : 0);
            return State(m);
        }
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            if (redstoneOnly) return false;
            Toggle(w, pos, meta);
            return true;
        }
        void Toggle(World w, Int3 pos, int meta)
        {
            w.SetState(pos, State(meta ^ 4), SetFlags.Notify);
            Sounds.Play((meta & 4) == 0 ? "block.trapdoor.open" : "block.trapdoor.close", pos.Center, 1f, 1f);
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            bool powered = Redstone.IsPowered(w, pos);
            bool open = (meta & 4) != 0;
            if (powered && !open) { Toggle(w, pos, meta); Redstone.MarkPoweredOpen(pos); }
            else if (!powered && open && Redstone.WasPoweredOpen(w, pos)) Toggle(w, pos, meta);
        }
        public override bool CanSurvive(World w, Int3 pos, int meta) => true;
    }

    /// <summary>meta: facing(0-1) | open(2)</summary>
    public class FenceGateBlock : Block
    {
        public FenceGateBlock(Block planks)
        {
            stateCount = 8; opaqueCube = false; lightOpacity = 0; isFullCubeShape = false;
            System.Array.Copy(planks.faceTex, faceTex, 6); particleTex = planks.particleTex;
            hardness = 2; blastResistance = 3; tool = ToolType.Axe; sound = planks.sound; creativeTab = CreativeTab.Redstone;
            flammability = planks.flammability; fireSpread = planks.fireSpread;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            int t = faceTex[2]; uint c = 0xFFFFFFFFu;
            bool open = (meta & 4) != 0;
            ctx.rot = meta & 3;
            // canonical: gate spans X, facing north. posts at x 0-2 and 14-16
            ctx.BoxPx(0, 5, 7, 2, 16, 9, t, c);
            ctx.BoxPx(14, 5, 7, 16, 16, 9, t, c);
            if (!open)
            {
                ctx.BoxPx(6, 6, 7, 8, 15, 9, t, c); ctx.BoxPx(8, 6, 7, 10, 15, 9, t, c);
                ctx.BoxPx(2, 6, 7, 6, 9, 9, t, c); ctx.BoxPx(2, 12, 7, 6, 15, 9, t, c);
                ctx.BoxPx(10, 6, 7, 14, 9, 9, t, c); ctx.BoxPx(10, 12, 7, 14, 15, 9, t, c);
            }
            else
            {
                ctx.BoxPx(0, 6, 13, 2, 15, 15, t, c); ctx.BoxPx(14, 6, 13, 16, 15, 15, t, c);
                ctx.BoxPx(0, 6, 9, 2, 9, 13, t, c); ctx.BoxPx(0, 12, 9, 2, 15, 13, t, c);
                ctx.BoxPx(14, 6, 9, 16, 9, 13, t, c); ctx.BoxPx(14, 12, 9, 16, 15, 13, t, c);
            }
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            if ((meta & 4) != 0) return;
            boxes.Add(BoxUtil.Rot(BoxUtil.Px(0, 0, 6, 16, 24, 10), meta & 3));
        }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Rot(BoxUtil.Px(0, 0, 6, 16, 16, 10), meta & 3));
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(DirUtil.HorizIndex(ctx.playerFacing));
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit)
        {
            int m = meta;
            if ((m & 4) != 0) m &= ~4;
            else
            {
                // open away from the player
                Dir pf = player != null ? player.HorizontalFacing : DirUtil.FromHorizIndex(m & 3);
                Dir gf = DirUtil.FromHorizIndex(m & 3);
                if (pf == DirUtil.Opposite(gf)) m = (m & ~3) | DirUtil.HorizIndex(pf);
                m |= 4;
            }
            w.SetState(pos, State(m), SetFlags.Notify);
            Sounds.Play((m & 4) != 0 ? "block.fence_gate.open" : "block.fence_gate.close", pos.Center, 1f, 1f);
            return true;
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            bool powered = Redstone.IsPowered(w, pos);
            bool open = (meta & 4) != 0;
            if (powered && !open) { w.SetState(pos, State(meta | 4), SetFlags.Notify); Redstone.MarkPoweredOpen(pos); }
            else if (!powered && open && Redstone.WasPoweredOpen(w, pos)) w.SetState(pos, State(meta & ~4), SetFlags.Notify);
        }
    }

    // ============================================================================ Torches
    /// <summary>meta 0 = standing, 1..4 = wall torch facing Horizontal[meta-1] (away from wall). Redstone torch adds lit bit (8 = unlit).</summary>
    public class TorchBlock : Block
    {
        public int tex;
        public string flameParticle = "flame";
        public TorchBlock(string texName, byte light)
        {
            stateCount = 16; opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; layer = RenderLayer.Cutout;
            hardness = 0; sound = SoundType.Wood; lightEmission = light; tex = Tex.Id(texName); SetAllTex(tex); isFullCubeShape = false;
            creativeTab = CreativeTab.Functional; push = PushReaction.Destroy;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public virtual bool Lit(int meta) => (meta & 8) == 0;
        public override byte GetLightEmission(int meta) => Lit(meta) ? lightEmission : (byte)0;
        static readonly Vector4[] TorchUV = MakeTorchUV();
        static Vector4[] MakeTorchUV()
        {
            var uv = new Vector4[6];
            uv[0] = new Vector4(7 / 16f, 0 / 16f, 9 / 16f, 2 / 16f);   // bottom
            uv[1] = new Vector4(7 / 16f, 8 / 16f, 9 / 16f, 10 / 16f);  // top (flame)
            for (int i = 2; i < 6; i++) uv[i] = new Vector4(7 / 16f, 0, 9 / 16f, 10 / 16f);
            return uv;
        }
        protected int TexFor(int meta) => Lit(meta) || !(this is RedstoneTorchBlock rt) ? tex : rt.offTex;
        public override void Emit(MeshCtx ctx, int meta)
        {
            int m = meta & 7;
            int t = TexFor(meta);
            var tx = new[] { t, t, t, t, t, t };
            if (m == 0)
            {
                ctx.XBox(Matrix4x4.identity, new Vector3(7, 0, 7) / 16f, new Vector3(9, 10, 9) / 16f, tx, TorchUV, 0xFFFFFFFFu, RenderLayer.Cutout);
            }
            else
            {
                Dir f = DirUtil.FromHorizIndex(m - 1);
                // canonical wall torch facing north: base against south wall (z=0), leaning toward +z
                Matrix4x4 mat = Matrix4x4.Translate(new Vector3(0, 3.5f / 16f, -5f / 16f))
                    * Matrix4x4.Translate(new Vector3(0.5f, 0, 0.5f))
                    * Matrix4x4.Rotate(Quaternion.Euler(22.5f, 0, 0))
                    * Matrix4x4.Translate(new Vector3(-0.5f, 0, -0.5f));
                int save = ctx.rot; ctx.rot = DirUtil.HorizIndex(f);
                ctx.XBox(mat, new Vector3(7, 0, 7) / 16f, new Vector3(9, 10, 9) / 16f, tx, TorchUV, 0xFFFFFFFFu, RenderLayer.Cutout);
                ctx.rot = save;
            }
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            int m = meta & 7;
            if (m == 0) boxes.Add(BoxUtil.Px(6, 0, 6, 10, 10, 10));
            else boxes.Add(BoxUtil.Rot(BoxUtil.Px(5.5f, 3, 0, 10.5f, 13, 5), DirUtil.HorizIndex(DirUtil.FromHorizIndex(m - 1))));
        }
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            if (ctx.clickedFace == Dir.Up && ctx.world.IsSturdy(ctx.pos.Offset(Dir.Down), Dir.Up)) return State(0);
            if (DirUtil.IsHorizontal(ctx.clickedFace) && SupportsWall(ctx.world, ctx.pos, ctx.clickedFace))
                return State(1 + DirUtil.HorizIndex(ctx.clickedFace));
            if (ctx.world.IsSturdy(ctx.pos.Offset(Dir.Down), Dir.Up) || CanStandOn(ctx.world.GetBlock(ctx.pos.Offset(Dir.Down)))) return State(0);
            for (int i = 0; i < 4; i++) if (SupportsWall(ctx.world, ctx.pos, DirUtil.Horizontal[i])) return State(1 + i);
            return 0;
        }
        static bool CanStandOn(Block b) => b is FenceBlock || b is WallBlock || b.id == "glass" || b.id.EndsWith("_glass");
        static bool SupportsWall(World w, Int3 pos, Dir facing)
        {
            Int3 wall = pos.Offset(DirUtil.Opposite(facing));
            var b = w.GetBlock(wall);
            return b.opaqueCube || w.IsSturdy(wall, facing);
        }
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            int m = meta & 7;
            if (m == 0)
            {
                Int3 below = pos.Offset(Dir.Down);
                return w.IsSturdy(below, Dir.Up) || CanStandOn(w.GetBlock(below)) || w.GetBlock(below).opaqueCube;
            }
            return SupportsWall(w, pos, DirUtil.FromHorizIndex(m - 1));
        }
        public override bool CanPlaceAt(World w, Int3 pos, ushort state) => CanSurvive(w, pos, state - baseState);
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (!Lit(meta)) return;
            int m = meta & 7;
            Vector3 p = pos.ToVector3() + new Vector3(0.5f, 0.7f, 0.5f);
            if (m != 0)
            {
                Vector3 n = DirUtil.Normal[(int)DirUtil.FromHorizIndex(m - 1)];
                p = pos.ToVector3() + new Vector3(0.5f, 0.92f, 0.5f) - n * 0.27f;
            }
            if (rng.Chance(0.35f)) Particles.Smoke(w, p, 1, 0.3f);
            Particles.Flame(w, p, flameParticle);
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) => drops.Add(new ItemStack(item, 1));
    }

    public class RedstoneTorchBlock : TorchBlock
    {
        public int offTex;
        public RedstoneTorchBlock() : base("redstone_torch", 7)
        {
            offTex = Tex.Id("redstone_torch_off"); flameParticle = "redstone"; creativeTab = CreativeTab.Redstone;
        }
        public override bool IsRedstoneSource(int meta) => true;
        public override int GetWeakPower(World w, Int3 pos, int meta, Dir towards)
        {
            if (!Lit(meta)) return 0;
            int m = meta & 7;
            Dir attached = m == 0 ? Dir.Down : DirUtil.Opposite(DirUtil.FromHorizIndex(m - 1));
            return towards == attached ? 0 : 15;
        }
        public override int GetStrongPower(World w, Int3 pos, int meta, Dir towards) => Lit(meta) && towards == Dir.Up ? 15 : 0;
        public Int3 AttachedPos(Int3 pos, int meta)
        {
            int m = meta & 7;
            return m == 0 ? pos.Offset(Dir.Down) : pos.Offset(DirUtil.Opposite(DirUtil.FromHorizIndex(m - 1)));
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (!CanSurvive(w, pos, meta)) { w.BreakBlock(pos, true, null); return; }
            w.ScheduleTick(pos, this, 2);
        }
        public override void OnAdded(World w, Int3 pos, int meta, ushort oldState)
        {
            w.ScheduleTick(pos, this, 2);
            Redstone.NotifyAround(w, pos);
        }
        public override void OnRemoved(World w, Int3 pos, int meta, ushort newState) => Redstone.NotifyAround(w, pos);
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            bool attachedPowered = Redstone.IsBlockPoweredFrom(w, AttachedPos(pos, meta), pos);
            bool lit = Lit(meta);
            if (lit == attachedPowered)
            {
                w.SetState(pos, State(attachedPowered ? (meta | 8) : (meta & ~8)), SetFlags.Notify);
                Redstone.NotifyAround(w, pos);
            }
        }
    }

    // ============================================================================ Lanterns / chains
    public class LanternBlock : Block
    {
        public LanternBlock(string tex, byte light)
        {
            stateCount = 2; opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; lightEmission = light; hardness = 3.5f;
            tool = ToolType.Pickaxe; requiresTool = true; sound = SoundType.Lantern; SetAllTex(Tex.Id(tex)); isFullCubeShape = false;
            creativeTab = CreativeTab.Functional; push = PushReaction.Destroy;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            float oy = meta == 1 ? 1 : 0; // hanging offset
            int t = faceTex[0]; uint c = 0xFFFFFFFFu;
            var tx = new[] { t, t, t, t, t, t };
            // body 6x7x6 (5..11) with uv from texture (0,2)-(6,9); cap 4x2x4
            var bodyUV = new Vector4[6];
            for (int i = 2; i < 6; i++) bodyUV[i] = new Vector4(0, 7 / 16f, 6 / 16f, 14 / 16f);
            bodyUV[0] = bodyUV[1] = new Vector4(0, 1 / 16f, 6 / 16f, 7 / 16f);
            ctx.XBox(Matrix4x4.identity, new Vector3(5, oy, 5) / 16f, new Vector3(11, 7 + oy, 11) / 16f, tx, bodyUV, c, RenderLayer.Cutout);
            var capUV = new Vector4[6];
            for (int i = 2; i < 6; i++) capUV[i] = new Vector4(0, 14 / 16f, 4 / 16f, 16 / 16f);
            capUV[0] = capUV[1] = new Vector4(0, 1 / 16f, 4 / 16f, 5 / 16f);
            ctx.XBox(Matrix4x4.identity, new Vector3(6, 7 + oy, 6) / 16f, new Vector3(10, 9 + oy, 10) / 16f, tx, capUV, c, RenderLayer.Cutout);
            // handle / chain
            var hUV = new Vector4[6];
            for (int i = 0; i < 6; i++) hUV[i] = new Vector4(11 / 16f, 10 / 16f, 14 / 16f, 12 / 16f);
            ctx.XBox(Matrix4x4.identity, new Vector3(6.5f, 9 + oy, 8) / 16f, new Vector3(9.5f, meta == 1 ? 16 : 11, 8) / 16f, tx, hUV, c, RenderLayer.Cutout, (1 << 2) | (1 << 3) ^ 0);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(meta == 1 ? BoxUtil.Px(5, 1, 5, 11, 10, 11) : BoxUtil.Px(5, 0, 5, 11, 9, 11));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            if (ctx.clickedFace == Dir.Down) return State(1);
            if (!ctx.world.IsSturdy(ctx.pos.Offset(Dir.Down), Dir.Up) && !(ctx.world.GetBlock(ctx.pos.Offset(Dir.Down)) is FenceBlock)) return State(1);
            return State(0);
        }
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            if (meta == 1) { var up = w.GetBlock(pos.Offset(Dir.Up)); return !up.isAir && (up.solid || up is ChainBlock || up is FenceBlock || up is LeavesBlock); }
            var below = w.GetBlock(pos.Offset(Dir.Down));
            return !below.isAir && below.solid;
        }
    }

    public class ChainBlock : PillarBlock
    {
        public ChainBlock(string tex) : base(tex, tex)
        {
            opaqueCube = false; lightOpacity = 0; layer = RenderLayer.Cutout; hardness = 5; tool = ToolType.Pickaxe; requiresTool = true;
            sound = SoundType.Chain; isFullCubeShape = false; creativeTab = CreativeTab.Building;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            int t = faceTex[0];
            Matrix4x4 m = Matrix4x4.identity;
            if (meta == 1) m = Matrix4x4.Translate(new Vector3(0.5f, 0.5f, 0.5f)) * Matrix4x4.Rotate(Quaternion.Euler(0, 0, 90)) * Matrix4x4.Translate(new Vector3(-0.5f, -0.5f, -0.5f));
            else if (meta == 2) m = Matrix4x4.Translate(new Vector3(0.5f, 0.5f, 0.5f)) * Matrix4x4.Rotate(Quaternion.Euler(90, 0, 0)) * Matrix4x4.Translate(new Vector3(-0.5f, -0.5f, -0.5f));
            // two crossed planes (like chain model)
            Vector3 a0 = m.MultiplyPoint3x4(new Vector3(6.5f / 16, 0, 6.5f / 16)), a1 = m.MultiplyPoint3x4(new Vector3(6.5f / 16, 1, 6.5f / 16));
            Vector3 b0 = m.MultiplyPoint3x4(new Vector3(9.5f / 16, 0, 9.5f / 16)), b1 = m.MultiplyPoint3x4(new Vector3(9.5f / 16, 1, 9.5f / 16));
            ctx.Quad(a0, a1, b1, b0, new Vector2(0, 0), new Vector2(0, 1), new Vector2(3 / 16f, 1), new Vector2(3 / 16f, 0), t, 0xFFFFFFFFu, 0.8f, RenderLayer.Cutout, true);
            Vector3 c0 = m.MultiplyPoint3x4(new Vector3(6.5f / 16, 0, 9.5f / 16)), c1 = m.MultiplyPoint3x4(new Vector3(6.5f / 16, 1, 9.5f / 16));
            Vector3 d0 = m.MultiplyPoint3x4(new Vector3(9.5f / 16, 0, 6.5f / 16)), d1 = m.MultiplyPoint3x4(new Vector3(9.5f / 16, 1, 6.5f / 16));
            ctx.Quad(c0, c1, d1, d0, new Vector2(3 / 16f, 0), new Vector2(3 / 16f, 1), new Vector2(6 / 16f, 1), new Vector2(6 / 16f, 0), t, 0xFFFFFFFFu, 0.7f, RenderLayer.Cutout, true);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            if (meta == 0) boxes.Add(BoxUtil.Px(6.5f, 0, 6.5f, 9.5f, 16, 9.5f));
            else if (meta == 1) boxes.Add(BoxUtil.Px(0, 6.5f, 6.5f, 16, 9.5f, 9.5f));
            else boxes.Add(BoxUtil.Px(6.5f, 6.5f, 0, 9.5f, 9.5f, 16));
        }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
    }

    // ============================================================================ Nether portal
    /// <summary>meta: 0 = portal plane along X (frame spans X), 1 = along Z.</summary>
    public class PortalBlock : Block
    {
        public PortalBlock()
        {
            stateCount = 2; opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; layer = RenderLayer.Translucent;
            lightEmission = 11; hardness = -1; blastResistance = 0; noItem = true; hiddenInCreative = true; SetAllTex(Tex.Id("nether_portal"));
            sound = SoundType.Glass; isFullCubeShape = false; push = PushReaction.Block;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            var t = faceTex;
            if (meta == 0) ctx.Box(0, 0, 6f / 16, 1, 1, 10f / 16, t, 0xFFFFFFFFu, (1 << 4) | (1 << 5) | 3, true, RenderLayer.Translucent);
            else ctx.Box(6f / 16, 0, 0, 10f / 16, 1, 1, t, 0xFFFFFFFFu, (1 << 2) | (1 << 3) | 3, true, RenderLayer.Translucent);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            boxes.Add(meta == 0 ? BoxUtil.Px(0, 0, 6, 16, 16, 10) : BoxUtil.Px(6, 0, 0, 10, 16, 16));
        }
        public override bool Targetable(int meta) => true;
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            // portal breaks if its frame is broken
            if (!Portals.IsValidPortalBlock(w, pos, meta)) w.SetState(pos, 0);
        }
        public override void OnEntityInside(World w, Int3 pos, int meta, Entity e) => e.EnterNetherPortal(pos);
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (rng.Chance(0.01f)) Sounds.Play("block.portal.ambient", pos.Center, 0.5f, rng.Range(0.8f, 1.2f));
            for (int i = 0; i < 2; i++) Particles.Portal(w, pos.Center + new Vector3(rng.Range(-0.5f, 0.5f), rng.Range(-0.5f, 0.5f), rng.Range(-0.5f, 0.5f)));
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) { }
    }

    // ============================================================================ Fire
    public class FireBlock : Block
    {
        public bool soul;
        public FireBlock(bool soulFire)
        {
            soul = soulFire; stateCount = 16; opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; layer = RenderLayer.Cutout;
            lightEmission = (byte)(soulFire ? 10 : 15); hardness = 0; noItem = true; hiddenInCreative = true; replaceable = true; randomTicks = true;
            SetAllTex(Tex.Id(soulFire ? "soul_fire" : "fire")); isFullCubeShape = false; push = PushReaction.Destroy;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            int t = faceTex[0];
            uint c = 0xFFFFFFFFu;
            // four inward-leaning planes + cross
            ctx.Quad(new Vector3(0, 0, 0.1f), new Vector3(0, 1.4f, 0.3f), new Vector3(1, 1.4f, 0.3f), new Vector3(1, 0, 0.1f), new Vector2(0, 0), new Vector2(0, 1), new Vector2(1, 1), new Vector2(1, 0), t, c, 1f, RenderLayer.Cutout, true);
            ctx.Quad(new Vector3(1, 0, 0.9f), new Vector3(1, 1.4f, 0.7f), new Vector3(0, 1.4f, 0.7f), new Vector3(0, 0, 0.9f), new Vector2(0, 0), new Vector2(0, 1), new Vector2(1, 1), new Vector2(1, 0), t, c, 1f, RenderLayer.Cutout, true);
            ctx.Quad(new Vector3(0.1f, 0, 1), new Vector3(0.3f, 1.4f, 1), new Vector3(0.3f, 1.4f, 0), new Vector3(0.1f, 0, 0), new Vector2(0, 0), new Vector2(0, 1), new Vector2(1, 1), new Vector2(1, 0), t, c, 1f, RenderLayer.Cutout, true);
            ctx.Quad(new Vector3(0.9f, 0, 0), new Vector3(0.7f, 1.4f, 0), new Vector3(0.7f, 1.4f, 1), new Vector3(0.9f, 0, 1), new Vector2(0, 0), new Vector2(0, 1), new Vector2(1, 1), new Vector2(1, 0), t, c, 1f, RenderLayer.Cutout, true);
            ctx.Cross(t, c, 1f, 0, 1.2f);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override bool Targetable(int meta) => false;
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            var below = w.GetBlock(pos.Offset(Dir.Down));
            if (soul) return below.id == "soul_sand" || below.id == "soul_soil";
            if (below.solid || below.opaqueCube) return true;
            for (int d = 1; d < 6; d++) if (w.GetBlock(pos.Offset((Dir)d)).flammability > 0) return true;
            return false;
        }
        public override void OnAdded(World w, Int3 pos, int meta, ushort oldState)
        {
            if (Portals.TryLightPortal(w, pos)) return;
            w.ScheduleTick(pos, this, 30 + w.rand.Next(10));
        }
        static bool Infiniburn(Block b) => b.id == "netherrack" || b.id == "magma_block" || b.id == "soul_sand" || b.id == "soul_soil" || b.id == "bedrock";
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            if (!CanSurvive(w, pos, meta)) { w.SetState(pos, 0); return; }
            if (soul) return;
            var below = w.GetBlock(pos.Offset(Dir.Down));
            bool infinite = Infiniburn(below);
            if (!infinite && w.session != null && w.session.IsRaining && w.IsRainingAt(pos) && w.rand.Chance(0.6f)) { w.SetState(pos, 0); return; }
            int age = meta;
            if (age < 15 && w.rand.Chance(0.4f)) age++;
            if (age != meta) w.SetState(pos, State(age), 0);
            if (!infinite)
            {
                bool anyFuel = false;
                for (int d = 0; d < 6; d++) if (w.GetBlock(pos.Offset((Dir)d)).flammability > 0) { anyFuel = true; break; }
                if (!anyFuel && (!below.solid || age > 3)) { w.SetState(pos, 0); return; }
                if (age >= 15 && !anyFuel && w.rand.Chance(0.25f)) { w.SetState(pos, 0); return; }
            }
            // burn neighbours & spread
            bool doFireTick = w.session == null || w.session.doFireTick;
            if (doFireTick)
            {
                for (int d = 0; d < 6; d++)
                {
                    Int3 n = pos.Offset((Dir)d);
                    var nb = w.GetBlock(n);
                    if (nb.fireSpread > 0 && w.rand.Next(d < 2 ? 250 : 300) < nb.fireSpread)
                    {
                        if (nb is TntBlock tnt) { tnt.Ignite(w, n, null); continue; }
                        if (w.rand.Chance(0.5f) && age < 10) w.SetState(n, State(Mathf.Min(15, age + 2)));
                        else w.SetState(n, 0);
                    }
                }
                for (int i = 0; i < 3; i++)
                {
                    Int3 t = new Int3(pos.x + w.rand.Range(-1, 1), pos.y + w.rand.Range(-1, 3), pos.z + w.rand.Range(-1, 1));
                    if (!w.IsAir(t)) continue;
                    int odds = 0;
                    for (int d = 0; d < 6; d++) odds = Mathf.Max(odds, w.GetBlock(t.Offset((Dir)d)).flammability);
                    if (odds > 0 && w.rand.Next(100 + (t.y > pos.y ? (t.y - pos.y) * 100 : 0)) < odds) w.SetState(t, State(Mathf.Min(15, age + 1)));
                }
            }
            w.ScheduleTick(pos, this, 30 + w.rand.Next(10));
        }
        public override void OnEntityInside(World w, Int3 pos, int meta, Entity e)
        {
            if (e is LivingEntity le && !le.fireImmune) { le.SetOnFire(8); le.Hurt(DamageSource.InFire, soul ? 2f : 1f); }
            else if (e is ItemEntity ie && !ie.stack.item.fireResistant) ie.Remove();
        }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (rng.Chance(0.04f)) Sounds.Play("block.fire.ambient", pos.Center, 0.6f, rng.Range(0.7f, 1.3f));
            if (rng.Chance(0.3f)) Particles.Smoke(w, pos.Center + new Vector3(rng.Range(-0.4f, 0.4f), 0.4f, rng.Range(-0.4f, 0.4f)), 1, 0.4f, true);
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) { }
    }
}
