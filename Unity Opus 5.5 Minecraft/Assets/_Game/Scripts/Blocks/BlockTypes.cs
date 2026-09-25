using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public static class BoxUtil
    {
        /// <summary>Rotate a block-local box by quarter turns (clockwise from above) around the block centre.</summary>
        public static AABB Rot(AABB b, int rot)
        {
            float x0 = b.min.x, z0 = b.min.z, x1 = b.max.x, z1 = b.max.z;
            switch (rot & 3)
            {
                case 1: return new AABB(z0, b.min.y, 1 - x1, z1, b.max.y, 1 - x0);
                case 2: return new AABB(1 - x1, b.min.y, 1 - z1, 1 - x0, b.max.y, 1 - z0);
                case 3: return new AABB(1 - z1, b.min.y, x0, 1 - z0, b.max.y, x1);
            }
            return b;
        }
        public static AABB Px(float x0, float y0, float z0, float x1, float y1, float z1) => new AABB(x0 / 16f, y0 / 16f, z0 / 16f, x1 / 16f, y1 / 16f, z1 / 16f);
        public static void AddRot(List<AABB> boxes, AABB b, int rot) => boxes.Add(Rot(b, rot));
    }

    // ============================================================================ Air
    public class AirBlock : Block
    {
        public AirBlock()
        {
            isAir = true; opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; layer = RenderLayer.None;
            replaceable = true; noItem = true; hiddenInCreative = true; hardness = 0; isFullCubeShape = false;
        }
        public override void Emit(MeshCtx ctx, int meta) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override bool Targetable(int meta) => false;
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) { }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos) { }
    }

    /// <summary>Simple cube with no special behaviour but configurable geometry flags.</summary>
    public class GlassBlock : Block
    {
        public GlassBlock(RenderLayer l = RenderLayer.Cutout)
        {
            opaqueCube = false; layer = l; lightOpacity = 0; selfCullSameType = true; sound = SoundType.Glass;
            hardness = 0.3f; blastResistance = 0.3f;
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) { }
    }

    /// <summary>Full cube that lets light through and doesn't cull (ice, slime, honey, etc.).</summary>
    public class TranslucentCube : Block
    {
        public TranslucentCube(RenderLayer l = RenderLayer.Translucent, byte opacity = 0)
        {
            opaqueCube = false; layer = l; lightOpacity = opacity; selfCullSameType = true;
        }
    }

    // ============================================================================ Fluids
    public class FluidBlock : Block
    {
        public int fluidKind; // 0 water 1 lava
        public int stillTex, flowTex;
        public FluidBlock(int kind)
        {
            fluidKind = kind; stateCount = 16; isLiquid = true; opaqueCube = false; solid = false; sturdy = false;
            replaceable = true; hardness = 100; blastResistance = 100; isFullCubeShape = false; noItem = true; hiddenInCreative = true;
            layer = kind == 0 ? RenderLayer.Translucent : RenderLayer.Opaque;
            lightOpacity = (byte)(kind == 0 ? 1 : 0);
            lightEmission = (byte)(kind == 0 ? 0 : 15);
            sound = kind == 0 ? SoundType.Water : SoundType.Lava;
            push = PushReaction.Destroy;
        }
        public bool IsSource(int meta) => meta == 0;
        public override bool IsWaterLike(int meta) => fluidKind == 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            uint color = fluidKind == 0 ? ctx.TintColor(TintType.Water) : 0xFFFFFFFFu;
            ctx.Liquid(this, stillTex, flowTex, color, layer);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override bool Targetable(int meta) => false;
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) { }

        int Delay(World w) => fluidKind == 0 ? 5 : (w.dim == DimensionId.Nether ? 10 : 30);
        int DropOff(World w) => fluidKind == 0 ? 1 : (w.dim == DimensionId.Nether ? 1 : 2);
        int SlopeRange(World w) => fluidKind == 0 ? 4 : (w.dim == DimensionId.Nether ? 4 : 2);

        public override void OnAdded(World w, Int3 pos, int meta, ushort oldState)
        {
            if (!ReactWithNeighbours(w, pos, meta)) w.ScheduleTick(pos, this, Delay(w));
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (!ReactWithNeighbours(w, pos, meta)) w.ScheduleTick(pos, this, Delay(w));
        }

        /// <summary>Lava touching water: obsidian / cobblestone (and basalt with soul soil + blue ice).</summary>
        bool ReactWithNeighbours(World w, Int3 pos, int meta)
        {
            if (fluidKind != 1) return false;
            for (int d = 1; d < 6; d++)
            {
                Int3 n = pos.Offset((Dir)d);
                if (w.IsWater(n))
                {
                    var result = meta == 0 ? Blocks.Get("obsidian") : Blocks.Get("cobblestone");
                    w.SetBlock(pos, result);
                    Sounds.Play("block.lava.extinguish", pos.Center, 0.5f, 2.6f);
                    Particles.Smoke(w, pos.Center + Vector3.up * 0.5f, 6, 0.4f);
                    return true;
                }
            }
            if (w.GetBlock(pos.Offset(Dir.Down)).id == "soul_soil")
            {
                for (int d = 2; d < 6; d++)
                    if (w.GetBlock(pos.Offset((Dir)d)).id == "blue_ice")
                    {
                        w.SetBlock(pos, Blocks.Get("basalt"));
                        return true;
                    }
            }
            return false;
        }

        static int Amount(int m) => m == 0 || m >= 8 ? 8 : 8 - m;

        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            int drop = DropOff(w);
            if (meta != 0)
            {
                int nm = ComputeNewMeta(w, pos, drop);
                if (nm < 0) { w.SetState(pos, 0); return; }
                if (nm != meta) { w.SetState(pos, State(nm)); meta = nm; }
            }
            Int3 down = pos.Offset(Dir.Down);
            if (CanFlowInto(w, down, 8))
            {
                FlowInto(w, down, 8);
                if (SourceNeighbours(w, pos) >= 3) SpreadSides(w, pos, meta, drop);
                return;
            }
            if (meta == 0 || w.GetBlock(down) != this) SpreadSides(w, pos, meta, drop);
        }

        int ComputeNewMeta(World w, Int3 pos, int drop)
        {
            if (w.GetBlock(pos.Offset(Dir.Up)) == this) return 8;
            int maxAmt = 0, sources = 0;
            for (int d = 2; d < 6; d++)
            {
                ushort ns = w.GetState(pos.Offset((Dir)d));
                if (Blocks.ByState[ns] != this) continue;
                int nm = ns - baseState;
                if (nm == 0) sources++;
                int a = Amount(nm);
                if (a > maxAmt) maxAmt = a;
            }
            if (fluidKind == 0 && sources >= 2)
            {
                ushort below = w.GetState(pos.Offset(Dir.Down));
                var bb = Blocks.ByState[below];
                if (bb.solid || below == baseState) return 0;
            }
            int amt = maxAmt - drop;
            if (amt <= 0) return -1;
            return 8 - amt;
        }

        int SourceNeighbours(World w, Int3 pos)
        {
            int n = 0;
            for (int d = 2; d < 6; d++) if (w.GetState(pos.Offset((Dir)d)) == baseState) n++;
            return n;
        }

        readonly int[] slopeDist = new int[4];
        void SpreadSides(World w, Int3 pos, int meta, int drop)
        {
            int side = Amount(meta) - drop;
            if (side <= 0) return;
            int newMeta = 8 - side;
            int best = 1000;
            for (int i = 0; i < 4; i++)
            {
                Dir d = DirUtil.Horizontal[i];
                Int3 n = pos.Offset(d);
                if (!CanFlowInto(w, n, newMeta)) { slopeDist[i] = 100000; continue; }
                Int3 nb = n.Offset(Dir.Down);
                slopeDist[i] = (CanFlowInto(w, nb, 8) || w.GetBlock(nb) == this) ? 0 : SlopeDistance(w, n, 1, DirUtil.Opposite(d));
                if (slopeDist[i] < best) best = slopeDist[i];
            }
            for (int i = 0; i < 4; i++)
            {
                if (slopeDist[i] >= 100000 || slopeDist[i] != best) continue;
                FlowInto(w, pos.Offset(DirUtil.Horizontal[i]), newMeta);
            }
        }

        int SlopeDistance(World w, Int3 p, int depth, Dir from)
        {
            int best = 1000;
            if (depth >= SlopeRange(w)) return best;
            for (int i = 0; i < 4; i++)
            {
                Dir d = DirUtil.Horizontal[i];
                if (d == from) continue;
                Int3 n = p.Offset(d);
                var b = w.GetBlock(n);
                if (b.solid || (b == this && w.GetMeta(n) == 0)) continue;
                if (!CanFlowInto(w, n, 7) && b != this) continue;
                if (CanFlowInto(w, n.Offset(Dir.Down), 8)) return depth;
                int r = SlopeDistance(w, n, depth + 1, DirUtil.Opposite(d));
                if (r < best) best = r;
            }
            return best;
        }

        bool CanFlowInto(World w, Int3 p, int newMeta)
        {
            if (p.y < w.minY || p.y >= w.maxY || !w.IsLoaded(p)) return false;
            ushort s = w.GetState(p);
            var b = Blocks.ByState[s];
            if (b == this)
            {
                int m = s - baseState;
                if (m == 0) return false;
                int curAmt = m >= 8 ? 8 : 8 - m;
                int newAmt = newMeta >= 8 ? 8 : 8 - newMeta;
                return newAmt > curAmt || (newMeta >= 8 && m < 8);
            }
            if (b.isLiquid) return true; // other fluid: reaction
            if (b.IsWaterLike(s - b.baseState)) return false;
            if (b.isAir) return true;
            if (b.solid) return false;
            if (b is PortalBlock || b.id == "end_portal" || b.id == "end_gateway") return false;
            return b.replaceable || b.push == PushReaction.Destroy || b.hardness == 0;
        }

        void FlowInto(World w, Int3 p, int newMeta)
        {
            ushort s = w.GetState(p);
            var b = Blocks.ByState[s];
            if (b.isLiquid && b != this)
            {
                // lava flowing into water or water into lava
                var other = (FluidBlock)b;
                if (fluidKind == 1 && other.fluidKind == 0)
                {
                    w.SetBlock(p, Blocks.Get(newMeta >= 8 ? "stone" : "cobblestone"));
                    Sounds.Play("block.lava.extinguish", p.Center, 0.5f, 2.6f);
                    return;
                }
                if (fluidKind == 0 && other.fluidKind == 1)
                {
                    int lm = s - b.baseState;
                    w.SetBlock(p, Blocks.Get(lm == 0 ? "obsidian" : "cobblestone"));
                    Sounds.Play("block.lava.extinguish", p.Center, 0.5f, 2.6f);
                    return;
                }
            }
            if (!b.isAir && !b.isLiquid)
            {
                if (fluidKind == 1) Sounds.Play("block.lava.extinguish", p.Center, 0.3f, 2f);
                else w.DropBlockLoot(p, b, s - b.baseState, null, null);
            }
            w.SetState(p, State(newMeta));
        }

        public override void OnEntityInside(World w, Int3 pos, int meta, Entity e)
        {
            if (fluidKind == 1 && e is LivingEntity le)
            {
                if (!le.fireImmune) { le.SetOnFire(15); le.Hurt(DamageSource.Lava, 4f); }
            }
            else if (fluidKind == 0) e.Extinguish();
        }

        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (fluidKind == 1 && meta == 0 && w.GetBlock(pos.Offset(Dir.Up)).isAir && rng.Chance(0.02f))
                Particles.LavaPop(w, pos.Center + new Vector3(0, 0.5f, 0));
            if (fluidKind == 0 && meta != 0 && rng.Chance(0.03f))
                Sounds.Play("block.water.ambient", pos.Center, 0.08f, 0.8f + rng.NextFloat() * 0.4f);
        }
    }

    // ============================================================================ Grass-like
    public class GrassBlock : Block
    {
        public int topTex, sideTex, snowSideTex, bottomTex;
        public string spreadsFrom = "dirt";
        public GrassBlock(string top, string side, string bottom, bool tinted)
        {
            stateCount = 2; randomTicks = true; sound = SoundType.Grass; hardness = 0.6f; blastResistance = 0.6f;
            tool = ToolType.Shovel;
            topTex = Tex.Id(top); sideTex = Tex.Id(side); bottomTex = Tex.Id(bottom); snowSideTex = Tex.Id("grass_block_snow");
            if (tinted) tint = TintType.Grass;
            SetTex(topTex, bottomTex, sideTex);
            dropItemId = "dirt";
            creativeTab = CreativeTab.Natural;
        }
        readonly int[] snowyTex = new int[6];
        public override void Emit(MeshCtx ctx, int meta)
        {
            if (meta == 1)
            {
                snowyTex[0] = bottomTex; snowyTex[1] = topTex;
                snowyTex[2] = snowyTex[3] = snowyTex[4] = snowyTex[5] = snowSideTex;
                ctx.Cube(snowyTex, tint);
            }
            else ctx.Cube(faceTex, tint);
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (fromPos.y == pos.y + 1 && (id == "grass_block" || id == "mycelium" || id == "podzol"))
            {
                var above = w.GetBlock(pos.Offset(Dir.Up));
                int snowy = above.id == "snow" || above.id == "snow_block" || above.id == "powder_snow" ? 1 : 0;
                if (snowy != meta) w.SetState(pos, State(snowy), SetFlags.Hooks);
            }
        }
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            Int3 up = pos.Offset(Dir.Up);
            ushort aboveS = w.GetState(up);
            bool covered = Blocks.StateOpacity[aboveS] >= 3 || Blocks.ByState[aboveS].isLiquid;
            if (covered)
            {
                if (id == "crimson_nylium" || id == "warped_nylium") w.SetBlock(pos, Blocks.Get("netherrack"));
                else w.SetBlock(pos, Blocks.Get("dirt"));
                return;
            }
            if (w.GetLightLevel(up) < 9 && id != "crimson_nylium" && id != "warped_nylium") return;
            for (int i = 0; i < 4; i++)
            {
                Int3 t = new Int3(pos.x + rng.Range(-1, 1), pos.y + rng.Range(-3, 1), pos.z + rng.Range(-1, 1));
                if (w.GetBlock(t).id != spreadsFrom) continue;
                Int3 ta = t.Offset(Dir.Up);
                if (Blocks.StateOpacity[w.GetState(ta)] >= 3 || w.GetLightLevel(ta) < 4) continue;
                w.SetBlock(t, this);
            }
        }
    }

    // ============================================================================ Pillars (logs, basalt, quartz pillar, hay...)
    public class PillarBlock : Block
    {
        public int endTex, sideTex;
        public string strippedId;
        static readonly int[] RotNone = { 0, 0, 0, 0, 0, 0 };
        static readonly int[] RotX = { 1, 1, 1, 1, 0, 0 };
        static readonly int[] RotZ = { 0, 0, 0, 0, 1, 1 };
        readonly int[] texY = new int[6], texX = new int[6], texZ = new int[6];
        public PillarBlock(string end, string side)
        {
            stateCount = 3;
            endTex = Tex.Id(end); sideTex = Tex.Id(side);
            SetTex(endTex, endTex, sideTex);
            for (int i = 0; i < 6; i++) { texY[i] = sideTex; texX[i] = sideTex; texZ[i] = sideTex; }
            texY[0] = texY[1] = endTex;
            texX[4] = texX[5] = endTex;
            texZ[2] = texZ[3] = endTex;
        }
        public override void Emit(MeshCtx ctx, int meta)
        {
            uint c = ctx.TintColor(tint);
            if (meta == 1) ctx.CubeRot(texX, RotX, c);
            else if (meta == 2) ctx.CubeRot(texZ, RotZ, c);
            else ctx.CubeRot(texY, RotNone, c);
        }
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            int axis = DirUtil.Axis(ctx.clickedFace);
            return State(axis == 1 ? 0 : (axis == 0 ? 1 : 2));
        }
    }

    // ============================================================================ Leaves
    public class LeavesBlock : Block
    {
        public string saplingId;
        public bool dropsApples;
        public LeavesBlock(string tex, TintType t)
        {
            stateCount = 16; // bit3 persistent, bits0-2 distance (1..7, 0 = unknown/7)
            opaqueCube = false; layer = RenderLayer.Cutout; lightOpacity = 1; tint = t;
            hardness = 0.2f; blastResistance = 0.2f; sound = SoundType.Grass; tool = ToolType.Hoe;
            randomTicks = true; flammability = 30; fireSpread = 60; creativeTab = CreativeTab.Natural;
            SetAllTex(Tex.Id(tex));
            push = PushReaction.Destroy;
        }
        public override int DefaultMeta => 8 | 7; // persistent (player placed)
        public override bool IsOpaqueCube(int meta) => false;
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta) => ctx.Cube(faceTex, tint);
        public override ushort GetPlacementState(ref PlaceContext ctx) => State(8 | 7);
        public static int Distance(int meta) => (meta & 7) == 0 ? 7 : meta & 7;
        public static bool Persistent(int meta) => (meta & 8) != 0;

        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (!Persistent(meta)) w.ScheduleTick(pos, this, 1);
        }
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            int d = ComputeDistance(w, pos);
            int nm = (meta & 8) | d;
            if (nm != meta) w.SetState(pos, State(nm), SetFlags.Notify);
        }
        public static int ComputeDistance(World w, Int3 pos)
        {
            int best = 7;
            for (int i = 0; i < 6; i++)
            {
                Int3 n = pos.Offset((Dir)i);
                ushort s = w.GetState(n);
                var b = Blocks.ByState[s];
                if (b is PillarBlock && (b.id.EndsWith("_log") || b.id.EndsWith("_wood") || b.id.EndsWith("_stem") || b.id.EndsWith("hyphae") || b.id == "mangrove_roots")) return 1;
                if (b is LeavesBlock) { int nd = Distance(s - b.baseState) + 1; if (nd < best) best = nd; }
            }
            return best;
        }
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (Persistent(meta)) return;
            if (Distance(meta) >= 7)
            {
                int d = ComputeDistance(w, pos);
                if (d >= 7) { w.BreakBlock(pos, true, null); return; }
                w.SetState(pos, State((meta & 8) | d), SetFlags.Notify);
            }
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if (tool != null && tool.item.toolType == ToolType.Shears) { drops.Add(new ItemStack(item, 1)); return; }
            int fortune = tool?.GetEnchant(Enchant.Fortune) ?? 0;
            float sap = saplingId == "jungle_sapling" ? 0.025f : 0.05f;
            sap *= 1f + fortune * 0.25f;
            if (saplingId != null && rng.Chance(sap)) drops.Add(new ItemStack(saplingId, 1));
            if (rng.Chance(0.02f)) drops.Add(new ItemStack("stick", rng.Range(1, 2)));
            if (dropsApples && rng.Chance(0.005f * (1 + fortune))) drops.Add(new ItemStack("apple", 1));
        }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (id == "cherry_leaves" && rng.Chance(0.1f)) Particles.Petal(w, pos.Center - Vector3.up * 0.55f, new Color32(255, 180, 210, 255));
            else if (id == "pale_oak_leaves" && rng.Chance(0.02f)) Particles.Petal(w, pos.Center - Vector3.up * 0.55f, new Color32(200, 205, 195, 255));
            else if (w.IsRainingAt(pos.Offset(Dir.Up)) && rng.Chance(0.05f)) Particles.Drip(w, pos.Center - Vector3.up * 0.55f, false);
        }
    }

    // ============================================================================ Plants
    public enum SoilKind : byte { Grass, Sand, Nether, Farmland, Any, Mushroom, Water, Soul, End, Dripleaf }

    public class PlantBlock : Block
    {
        public SoilKind soil = SoilKind.Grass;
        public bool randomOffset = true;
        public float size = 1f, heightPx = 13f, widthPx = 12f;
        public int tex;
        public PlantBlock(string texName, SoilKind soil = SoilKind.Grass)
        {
            opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; layer = RenderLayer.Cutout;
            hardness = 0; blastResistance = 0; sound = SoundType.Grass; isFullCubeShape = false;
            this.soil = soil; tex = Tex.Id(texName); SetAllTex(tex);
            creativeTab = CreativeTab.Natural; push = PushReaction.Destroy; flammability = 60; fireSpread = 100;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta) => ctx.Cross(tex, ctx.TintColor(tint), size, 0, 1f, randomOffset);
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            float h = widthPx / 32f;
            boxes.Add(new AABB(0.5f - h, 0, 0.5f - h, 0.5f + h, heightPx / 16f, 0.5f + h));
        }
        public static bool SoilOk(SoilKind soil, Block below, World w, Int3 belowPos)
        {
            string id = below.id;
            switch (soil)
            {
                case SoilKind.Grass:
                    return id == "grass_block" || id == "dirt" || id == "coarse_dirt" || id == "podzol" || id == "rooted_dirt" || id == "farmland"
                        || id == "moss_block" || id == "mud" || id == "muddy_mangrove_roots" || id == "mycelium" || id == "pale_moss_block";
                case SoilKind.Sand:
                    return id == "sand" || id == "red_sand" || id == "suspicious_sand" || id == "terracotta" || id.EndsWith("_terracotta") || SoilOk(SoilKind.Grass, below, w, belowPos);
                case SoilKind.Nether:
                    return id == "crimson_nylium" || id == "warped_nylium" || id == "soul_soil" || id == "mycelium" || SoilOk(SoilKind.Grass, below, w, belowPos);
                case SoilKind.Soul: return id == "soul_sand" || id == "soul_soil";
                case SoilKind.Farmland: return id == "farmland";
                case SoilKind.Mushroom: return below.opaqueCube;
                case SoilKind.End: return id == "end_stone" || id == "chorus_plant";
                case SoilKind.Any: return below.sturdy;
                default: return below.sturdy;
            }
        }
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            Int3 b = pos.Offset(Dir.Down);
            var below = w.GetBlock(b);
            if (soil == SoilKind.Mushroom)
            {
                if (below.id == "mycelium" || below.id == "podzol" || below.id == "nylium") return true;
                return below.opaqueCube && w.GetLightLevel(pos) < 13;
            }
            return SoilOk(soil, below, w, b);
        }
        public override bool CanPlaceAt(World w, Int3 pos, ushort state) => CanSurvive(w, pos, state - baseState);
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if ((id == "short_grass" || id == "fern") )
            {
                if (tool != null && tool.item.toolType == ToolType.Shears) drops.Add(new ItemStack(item, 1));
                else if (rng.Chance(0.125f)) drops.Add(new ItemStack("wheat_seeds", 1));
                return;
            }
            if (id == "dead_bush") { if (tool != null && tool.item.toolType == ToolType.Shears) drops.Add(new ItemStack(item, 1)); else drops.Add(new ItemStack("stick", rng.Range(0, 2))); return; }
            base.GetDrops(w, pos, meta, tool, drops, ref rng);
        }
        public override void OnEntityInside(World w, Int3 pos, int meta, Entity e)
        {
            if (id == "wither_rose" && e is LivingEntity le && !(le is WitherBoss)) le.AddEffect(new EffectInstance(Effect.Wither, 40, 0));
        }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (id == "crimson_roots" || id == "warped_roots") return;
        }
    }

    /// <summary>Saplings: grow into trees on random ticks / bone meal.</summary>
    public class SaplingBlock : PlantBlock
    {
        public string treeType;
        public SaplingBlock(string tex, string tree) : base(tex, SoilKind.Grass)
        {
            treeType = tree; stateCount = 2; randomTicks = true; randomOffset = false; heightPx = 12; widthPx = 12;
            flammability = 0; fireSpread = 0;
        }
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (w.GetLightLevel(pos.Offset(Dir.Up)) >= 9 && rng.Next(7) == 0) Advance(w, pos, meta, ref rng);
        }
        public void Advance(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (meta == 0) w.SetState(pos, State(1), 0);
            else TreeFeatures.GrowSapling(w, pos, treeType, ref rng);
        }
    }

    public class TallPlantBlock : Block
    {
        public int lowerTex, upperTex;
        public SoilKind soil = SoilKind.Grass;
        public TallPlantBlock(string lower, string upper)
        {
            stateCount = 2; // 0 lower, 1 upper
            opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; layer = RenderLayer.Cutout;
            hardness = 0; sound = SoundType.Grass; isFullCubeShape = false; creativeTab = CreativeTab.Natural; push = PushReaction.Destroy;
            lowerTex = Tex.Id(lower); upperTex = Tex.Id(upper); SetAllTex(lowerTex); particleTex = lowerTex;
            flammability = 60; fireSpread = 100;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            ctx.Cross(meta == 0 ? lowerTex : upperTex, ctx.TintColor(tint), 1f, 0, 1f, true);
            if (id == "sunflower" && meta == 1)
            {
                // flower head facing east
                int head = Tex.Id("sunflower_front");
                ctx.Quad(new Vector3(0.6f, 0.2f, 0.1f), new Vector3(0.6f, 1.0f, 0.1f), new Vector3(0.6f, 1.0f, 0.9f), new Vector3(0.6f, 0.2f, 0.9f),
                    new Vector2(0, 0), new Vector2(0, 1), new Vector2(1, 1), new Vector2(1, 0), head, 0xFFFFFFFFu, 0.9f, RenderLayer.Cutout, true);
            }
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(2, 0, 2, 14, 16, 14));
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            if (meta == 1) return w.GetBlock(pos.Offset(Dir.Down)) == this;
            if (w.GetBlock(pos.Offset(Dir.Up)) != this) return false;
            var below = w.GetBlock(pos.Offset(Dir.Down));
            return PlantBlock.SoilOk(soil, below, w, pos.Offset(Dir.Down));
        }
        public override bool CanPlaceAt(World w, Int3 pos, ushort state)
        {
            var below = w.GetBlock(pos.Offset(Dir.Down));
            return PlantBlock.SoilOk(soil, below, w, pos.Offset(Dir.Down)) && w.GetBlock(pos.Offset(Dir.Up)).replaceable;
        }
        public override void OnPlaced(World w, Int3 pos, int meta, Entity placer, ItemStack stack)
        {
            w.SetState(pos.Offset(Dir.Up), State(1), SetFlags.Hooks);
        }
        public override void OnBroken(World w, Int3 pos, int meta, Entity breaker)
        {
            Int3 other = meta == 0 ? pos.Offset(Dir.Up) : pos.Offset(Dir.Down);
            if (w.GetBlock(other) == this) w.SetState(other, 0, SetFlags.Hooks);
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if (id == "tall_grass" || id == "large_fern")
            {
                if (tool != null && tool.item.toolType == ToolType.Shears) drops.Add(new ItemStack(id == "tall_grass" ? "short_grass" : "fern", 2));
                else if (rng.Chance(0.125f)) drops.Add(new ItemStack("wheat_seeds", 1));
                return;
            }
            if (meta == 0) base.GetDrops(w, pos, meta, tool, drops, ref rng);
        }
    }

    // ============================================================================ Crops
    public class CropBlock : Block
    {
        public int maxAge;
        public int[] ageTex;
        public string seedItem, produceItem;
        public bool crossModel;
        public CropBlock(int maxAge, string texPrefix, int[] texStages, string seed, string produce)
        {
            this.maxAge = maxAge; stateCount = maxAge + 1;
            opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; layer = RenderLayer.Cutout;
            hardness = 0; sound = SoundType.Crop; randomTicks = true; isFullCubeShape = false; noItem = true;
            push = PushReaction.Destroy;
            ageTex = new int[maxAge + 1];
            for (int a = 0; a <= maxAge; a++) ageTex[a] = Tex.Id(texPrefix + "_stage" + texStages[a]);
            SetAllTex(ageTex[maxAge]);
            seedItem = seed; produceItem = produce;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            if (crossModel) ctx.Cross(ageTex[meta], 0xFFFFFFFFu, 1f, 0f, 1f, false);
            else ctx.Crop(ageTex[meta], 0xFFFFFFFFu);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            boxes.Add(new AABB(0, 0, 0, 1, Mathf.Max(2f / 16f, (meta + 1f) / (maxAge + 1f) * 0.9f), 1));
        }
        public virtual bool SoilOk(Block b) => b.id == "farmland";
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            if (!SoilOk(w.GetBlock(pos.Offset(Dir.Down)))) return false;
            return true;
        }
        public override bool CanPlaceAt(World w, Int3 pos, ushort state) => SoilOk(w.GetBlock(pos.Offset(Dir.Down)));
        public bool IsMature(int meta) => meta >= maxAge;
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (meta >= maxAge) return;
            if (w.GetLightLevel(pos.Offset(Dir.Up)) < 9) return;
            float speed = GrowthSpeed(w, pos);
            if (rng.Next((int)(25f / speed) + 1) == 0) w.SetState(pos, State(meta + 1), SetFlags.Notify);
        }
        public void Grow(World w, Int3 pos, int meta, int amount)
        {
            w.SetState(pos, State(Mathf.Min(maxAge, meta + amount)), SetFlags.Notify);
        }
        float GrowthSpeed(World w, Int3 pos)
        {
            float f = 1f;
            for (int dx = -1; dx <= 1; dx++)
                for (int dz = -1; dz <= 1; dz++)
                {
                    Int3 p = new Int3(pos.x + dx, pos.y - 1, pos.z + dz);
                    ushort s = w.GetState(p);
                    var b = Blocks.ByState[s];
                    float g = 0;
                    if (b.id == "farmland") g = (s - b.baseState) > 0 ? 3f : 1f;
                    if (dx != 0 || dz != 0) g /= 4f;
                    f += g;
                }
            return f;
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            int fortune = tool?.GetEnchant(Enchant.Fortune) ?? 0;
            if (meta >= maxAge)
            {
                if (produceItem == seedItem)
                {
                    int n = 1;
                    for (int i = 0; i < 3 + fortune; i++) if (rng.Chance(0.5714f)) n++;
                    drops.Add(new ItemStack(produceItem, n));
                    if (id == "potatoes" && rng.Chance(0.02f)) drops.Add(new ItemStack("poisonous_potato", 1));
                }
                else
                {
                    drops.Add(new ItemStack(produceItem, 1));
                    int n = 0;
                    for (int i = 0; i < 3 + fortune; i++) if (rng.Chance(0.5714f)) n++;
                    if (n > 0) drops.Add(new ItemStack(seedItem, n));
                }
            }
            else drops.Add(new ItemStack(seedItem, 1));
        }
        public override Item GetPickItem(int meta) => Items.Get(seedItem);
    }

    public class FarmlandBlock : Block
    {
        public FarmlandBlock()
        {
            stateCount = 8; opaqueCube = false; lightOpacity = 15; randomTicks = true; hardness = 0.6f; sound = SoundType.Gravel;
            tool = ToolType.Shovel; this.T3("farmland", "dirt", "dirt"); dropItemId = "dirt"; creativeTab = CreativeTab.Natural;
        }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public override void Emit(MeshCtx ctx, int meta)
        {
            var t = new int[6];
            t[0] = Tex.Id("dirt"); t[1] = Tex.Id(meta >= 7 ? "farmland_moist" : "farmland");
            t[2] = t[3] = t[4] = t[5] = Tex.Id("dirt");
            ctx.Box(0, 0, 0, 1, 15f / 16f, 1, t, 0xFFFFFFFFu);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(new AABB(0, 0, 0, 1, 15f / 16f, 1));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(new AABB(0, 0, 0, 1, 15f / 16f, 1));
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            bool water = NearWater(w, pos) || w.IsRainingAt(pos.Offset(Dir.Up));
            if (water) { if (meta < 7) w.SetState(pos, State(7), 0); }
            else if (meta > 0) w.SetState(pos, State(meta - 1), 0);
            else if (!(w.GetBlock(pos.Offset(Dir.Up)) is CropBlock)) w.SetBlock(pos, Blocks.Get("dirt"));
        }
        static bool NearWater(World w, Int3 pos)
        {
            for (int dx = -4; dx <= 4; dx++)
                for (int dz = -4; dz <= 4; dz++)
                    for (int dy = 0; dy <= 1; dy++)
                        if (w.IsWater(new Int3(pos.x + dx, pos.y + dy, pos.z + dz))) return true;
            return false;
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (fromPos.y == pos.y + 1 && w.GetBlock(fromPos).solid) w.SetBlock(pos, Blocks.Get("dirt"));
        }
        public override void OnFallenUpon(World w, Int3 pos, int meta, Entity e, float fallDistance)
        {
            if (fallDistance > 0.5f && e is LivingEntity && Random.value < fallDistance - 0.5f && !(e is Player p && p.IsCreative))
            {
                w.SetBlock(pos, Blocks.Get("dirt"));
                var above = pos.Offset(Dir.Up);
                if (w.GetBlock(above) is CropBlock) w.BreakBlock(above, true, e);
            }
            base.OnFallenUpon(w, pos, meta, e, fallDistance);
        }
    }

    /// <summary>A partial-height full-footprint block (dirt path, etc.).</summary>
    public class ShortBlock : Block
    {
        public float h;
        public ShortBlock(float heightPx) { h = heightPx / 16f; opaqueCube = false; lightOpacity = 0; }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public override void Emit(MeshCtx ctx, int meta) => ctx.Box(0, 0, 0, 1, h, 1, faceTex, ctx.TintColor(tint));
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(new AABB(0, 0, 0, 1, h, 1));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(new AABB(0, 0, 0, 1, h, 1));
    }

    // ============================================================================ Snow layer
    public class SnowLayerBlock : Block
    {
        public SnowLayerBlock()
        {
            stateCount = 8; opaqueCube = false; lightOpacity = 0; hardness = 0.1f; sound = SoundType.Snow; tool = ToolType.Shovel;
            requiresTool = true; randomTicks = true; SetAllTex(Tex.Id("snow")); creativeTab = CreativeTab.Natural; isFullCubeShape = false;
        }
        public override bool IsOpaqueCube(int meta) => meta == 7;
        public override byte GetLightOpacity(int meta) => (byte)(meta == 7 ? 15 : 0);
        public override int GetOccludingFaces(int meta) => meta == 7 ? 0x3F : 1 << (int)Dir.Down;
        public override void Emit(MeshCtx ctx, int meta) => ctx.Box(0, 0, 0, 1, (meta + 1) / 8f, 1, faceTex, 0xFFFFFFFFu);
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { if (meta > 0) boxes.Add(new AABB(0, 0, 0, 1, meta / 8f, 1)); }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(new AABB(0, 0, 0, 1, (meta + 1) / 8f, 1));
        public override bool CanBeReplaced(int meta, ref PlaceContext ctx)
        {
            if (ctx.stack != null && ctx.stack.item.block == this) return meta < 7;
            return meta == 0;
        }
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            ushort cur = ctx.world.GetState(ctx.pos);
            if (Blocks.ByState[cur] == this) return State(Mathf.Min(7, cur - baseState + 1));
            return State(0);
        }
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            var below = w.GetBlock(pos.Offset(Dir.Down));
            if (below.id == "ice" || below.id == "packed_ice" || below.id == "barrier") return false;
            if (below == this) return w.GetMeta(pos.Offset(Dir.Down)) == 7;
            return below.sturdy || below is LeavesBlock;
        }
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (w.GetBlockLight(pos) > 11) w.BreakBlock(pos, false, null);
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if (tool != null && tool.item.toolType == ToolType.Shovel) drops.Add(new ItemStack("snowball", meta + 1));
        }
        public override float GetHardness(int meta) => 0.1f;
    }

    // ============================================================================ Cactus / sugar cane / bamboo
    public class CactusBlock : Block
    {
        public CactusBlock()
        {
            stateCount = 16; opaqueCube = false; lightOpacity = 0; randomTicks = true; hardness = 0.4f; sound = SoundType.Wool;
            this.T3("cactus_top", "cactus_bottom", "cactus_side"); creativeTab = CreativeTab.Natural; layer = RenderLayer.Cutout; push = PushReaction.Destroy;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            // top/bottom full, sides inset by 1px
            var t = faceTex;
            ctx.Box(1f / 16, 0, 1f / 16, 15f / 16, 1, 15f / 16, t, 0xFFFFFFFFu, (1 << 0) | (1 << 1), false);
            ctx.Box(0, 0, 0, 1, 1, 1, t, 0xFFFFFFFFu, ~((1 << 0) | (1 << 1)) & 0x3F, true);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(1, 0, 1, 15, 15, 15));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(1, 0, 1, 15, 16, 15));
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            for (int d = 2; d < 6; d++)
            {
                var n = w.GetBlock(pos.Offset((Dir)d));
                if (n.solid || n.isLiquid) return false;
            }
            var below = w.GetBlock(pos.Offset(Dir.Down));
            return below == this || below.id == "sand" || below.id == "red_sand";
        }
        public override bool CanPlaceAt(World w, Int3 pos, ushort state) => CanSurvive(w, pos, 0);
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            Int3 up = pos.Offset(Dir.Up);
            if (!w.IsAir(up)) return;
            int h = 1;
            while (w.GetBlock(new Int3(pos.x, pos.y - h, pos.z)) == this) h++;
            if (h >= 3) return;
            if (meta == 15)
            {
                w.SetState(pos, State(0), 0);
                if (CanSurvive(w, up, 0)) w.SetBlock(up, this);
            }
            else w.SetState(pos, State(meta + 1), 0);
        }
        public override void OnEntityInside(World w, Int3 pos, int meta, Entity e)
        {
            if (e is LivingEntity le) le.Hurt(DamageSource.Cactus, 1f);
            else if (e is ItemEntity ie) ie.Remove();
        }
    }

    public class SugarCaneBlock : PlantBlock
    {
        public SugarCaneBlock() : base("sugar_cane", SoilKind.Sand)
        {
            stateCount = 16; randomTicks = true; randomOffset = false; heightPx = 16; widthPx = 12; flammability = 0; fireSpread = 0;
        }
        public override void Emit(MeshCtx ctx, int meta) => ctx.Cross(tex, ctx.TintColor(TintType.Grass), 1f, 0, 1f, false);
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            var below = w.GetBlock(pos.Offset(Dir.Down));
            if (below == this) return true;
            if (!(below.id == "grass_block" || below.id == "dirt" || below.id == "sand" || below.id == "red_sand" || below.id == "podzol" || below.id == "coarse_dirt" || below.id == "mud" || below.id == "moss_block" || below.id == "rooted_dirt")) return false;
            Int3 b = pos.Offset(Dir.Down);
            for (int d = 2; d < 6; d++)
            {
                Int3 n = b.Offset((Dir)d);
                if (w.IsWater(n) || w.GetBlock(n).id == "frosted_ice") return true;
            }
            return false;
        }
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            Int3 up = pos.Offset(Dir.Up);
            if (!w.IsAir(up)) return;
            int h = 1;
            while (w.GetBlock(new Int3(pos.x, pos.y - h, pos.z)) == this) h++;
            if (h >= 3) return;
            if (meta == 15) { w.SetState(pos, State(0), 0); w.SetBlock(up, this); }
            else w.SetState(pos, State(meta + 1), 0);
        }
    }

    // ============================================================================ Slabs
    public class SlabBlock : Block
    {
        public Block full;
        public SlabBlock(Block fullBlock)
        {
            full = fullBlock;
            stateCount = 3; // 0 bottom 1 top 2 double
            opaqueCube = false; lightOpacity = 0;
            System.Array.Copy(fullBlock.faceTex, faceTex, 6); particleTex = fullBlock.particleTex;
            hardness = fullBlock.hardness; blastResistance = fullBlock.blastResistance; tool = fullBlock.tool; toolTier = fullBlock.toolTier;
            requiresTool = fullBlock.requiresTool; sound = fullBlock.sound; flammability = fullBlock.flammability; fireSpread = fullBlock.fireSpread;
            tint = fullBlock.tint; creativeTab = CreativeTab.Building;
        }
        public override bool IsOpaqueCube(int meta) => meta == 2;
        public override byte GetLightOpacity(int meta) => (byte)(meta == 2 ? 15 : 0);
        public override int GetOccludingFaces(int meta) => meta == 2 ? 0x3F : (meta == 0 ? 1 << (int)Dir.Down : 1 << (int)Dir.Up);
        public override void Emit(MeshCtx ctx, int meta)
        {
            uint c = ctx.TintColor(tint);
            if (meta == 2) ctx.Cube(faceTex, tint);
            else if (meta == 0) ctx.Box(0, 0, 0, 1, 0.5f, 1, faceTex, c);
            else ctx.Box(0, 0.5f, 0, 1, 1, 1, faceTex, c);
        }
        static AABB Shape(int meta) => meta == 2 ? new AABB(0, 0, 0, 1, 1, 1) : (meta == 0 ? new AABB(0, 0, 0, 1, 0.5f, 1) : new AABB(0, 0.5f, 0, 1, 1, 1));
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(Shape(meta));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(Shape(meta));
        public override bool CanBeReplaced(int meta, ref PlaceContext ctx)
        {
            if (meta == 2 || ctx.stack == null || ctx.stack.item.block != this) return false;
            // merging: clicking the half-face of an existing slab
            if (ctx.pos == ctx.clickedPos)
            {
                if (meta == 0) return ctx.clickedFace == Dir.Up || (DirUtil.IsHorizontal(ctx.clickedFace) && ctx.hitLocal.y > 0.5f);
                return ctx.clickedFace == Dir.Down || (DirUtil.IsHorizontal(ctx.clickedFace) && ctx.hitLocal.y <= 0.5f);
            }
            return true;
        }
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            ushort cur = ctx.world.GetState(ctx.pos);
            if (Blocks.ByState[cur] == this) return State(2);
            if (ctx.clickedFace == Dir.Down) return State(1);
            if (ctx.clickedFace == Dir.Up) return State(0);
            return State(ctx.hitLocal.y > 0.5f ? 1 : 0);
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if (requiresTool && !CanHarvestWith(tool)) return;
            drops.Add(new ItemStack(item, meta == 2 ? 2 : 1));
        }
    }

    // ============================================================================ Stairs
    public class StairsBlock : Block
    {
        public Block full;
        public StairsBlock(Block fullBlock)
        {
            full = fullBlock;
            stateCount = 8; // facing(0-3) | half<<2
            opaqueCube = false; lightOpacity = 0;
            System.Array.Copy(fullBlock.faceTex, faceTex, 6); particleTex = fullBlock.particleTex;
            hardness = fullBlock.hardness; blastResistance = fullBlock.blastResistance; tool = fullBlock.tool; toolTier = fullBlock.toolTier;
            requiresTool = fullBlock.requiresTool; sound = fullBlock.sound; flammability = fullBlock.flammability; fireSpread = fullBlock.fireSpread;
            tint = fullBlock.tint;
        }
        public static Dir Facing(int meta) => DirUtil.FromHorizIndex(meta & 3);
        public static bool Top(int meta) => (meta & 4) != 0;
        public override int GetOccludingFaces(int meta) => Top(meta) ? 1 << (int)Dir.Up : 1 << (int)Dir.Down;
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            bool top = ctx.clickedFace == Dir.Down || (ctx.clickedFace != Dir.Up && ctx.hitLocal.y > 0.5f);
            return State(DirUtil.HorizIndex(ctx.playerFacing) | (top ? 4 : 0));
        }
        // shape: 0 straight 1 inner_left 2 inner_right 3 outer_left 4 outer_right
        public int GetShape(System.Func<Dir, ushort> neighbor, int meta)
        {
            Dir f = Facing(meta); bool top = Top(meta);
            ushort bs = neighbor(f);
            if (Blocks.ByState[bs] is StairsBlock sb)
            {
                int bm = bs - sb.baseState;
                if (Top(bm) == top)
                {
                    Dir bf = Facing(bm);
                    if (DirUtil.Axis(bf) != DirUtil.Axis(f) && CanTakeShape(neighbor, meta, DirUtil.Opposite(bf)))
                        return bf == DirUtil.RotateCCW(f) ? 3 : 4;
                }
            }
            ushort fs = neighbor(DirUtil.Opposite(f));
            if (Blocks.ByState[fs] is StairsBlock fb)
            {
                int fm = fs - fb.baseState;
                if (Top(fm) == top)
                {
                    Dir ff = Facing(fm);
                    if (DirUtil.Axis(ff) != DirUtil.Axis(f) && CanTakeShape(neighbor, meta, ff))
                        return ff == DirUtil.RotateCCW(f) ? 1 : 2;
                }
            }
            return 0;
        }
        static bool CanTakeShape(System.Func<Dir, ushort> neighbor, int meta, Dir d)
        {
            ushort s = neighbor(d);
            if (!(Blocks.ByState[s] is StairsBlock ob)) return true;
            int om = s - ob.baseState;
            return Facing(om) != Facing(meta) || Top(om) != Top(meta);
        }

        /// <summary>Boxes in canonical frame (facing north=+Z). Left = west(-X).</summary>
        public static void CanonicalBoxes(int shape, bool top, List<AABB> outBoxes)
        {
            float sy0 = top ? 0.5f : 0f, sy1 = top ? 1f : 0.5f;
            float ty0 = top ? 0f : 0.5f, ty1 = top ? 0.5f : 1f;
            outBoxes.Add(new AABB(0, sy0, 0, 1, sy1, 1));
            switch (shape)
            {
                case 0: outBoxes.Add(new AABB(0, ty0, 0.5f, 1, ty1, 1)); break;
                case 1: outBoxes.Add(new AABB(0, ty0, 0.5f, 1, ty1, 1)); outBoxes.Add(new AABB(0, ty0, 0, 0.5f, ty1, 0.5f)); break;
                case 2: outBoxes.Add(new AABB(0, ty0, 0.5f, 1, ty1, 1)); outBoxes.Add(new AABB(0.5f, ty0, 0, 1, ty1, 0.5f)); break;
                case 3: outBoxes.Add(new AABB(0, ty0, 0.5f, 0.5f, ty1, 1)); break;
                case 4: outBoxes.Add(new AABB(0.5f, ty0, 0.5f, 1, ty1, 1)); break;
            }
        }

        [System.ThreadStatic] static List<AABB> tmp;
        public override void Emit(MeshCtx ctx, int meta)
        {
            var t = tmp ?? (tmp = new List<AABB>());
            t.Clear();
            int shape = GetShape(d => ctx.N(d), meta);
            CanonicalBoxes(shape, Top(meta), t);
            ctx.rot = DirUtil.HorizIndex(Facing(meta));
            uint c = ctx.TintColor(tint);
            foreach (var b in t) ctx.Box(b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z, faceTex, c);
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            var t = new List<AABB>();
            int shape = GetShape(d => w.GetState(pos.Offset(d)), meta);
            CanonicalBoxes(shape, Top(meta), t);
            int r = DirUtil.HorizIndex(Facing(meta));
            foreach (var b in t) boxes.Add(BoxUtil.Rot(b, r));
        }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
    }

    // ============================================================================ Fences / walls / panes
    public class FenceBlock : Block
    {
        public string family; // "wood", "nether"
        public FenceBlock(Block planks, string family)
        {
            this.family = family; opaqueCube = false; lightOpacity = 0; isFullCubeShape = false;
            System.Array.Copy(planks.faceTex, faceTex, 6); particleTex = planks.particleTex;
            hardness = 2f; blastResistance = 3f; tool = planks.tool; sound = planks.sound; flammability = planks.flammability; fireSpread = planks.fireSpread;
            requiresTool = planks.requiresTool; toolTier = planks.toolTier;
        }
        public override int GetOccludingFaces(int meta) => 0;
        public bool ConnectsTo(ushort s, Dir d)
        {
            var b = Blocks.ByState[s];
            if (b is FenceBlock f) return f.family == family || (family != "nether" && f.family != "nether");
            if (b is FenceGateBlock) { var gf = StateBits.FacingDir(s - b.baseState); return DirUtil.Axis(gf) != DirUtil.Axis(d); }
            return b.opaqueCube && !(b is LeavesBlock) && b.id != "barrier" && !(b.id.Contains("pumpkin") || b.id == "melon");
        }
        public int Connections(System.Func<Dir, ushort> nb)
        {
            int m = 0;
            for (int i = 0; i < 4; i++) { Dir d = DirUtil.Horizontal[i]; if (ConnectsTo(nb(d), d)) m |= 1 << i; }
            return m;
        }
        public override void Emit(MeshCtx ctx, int meta)
        {
            int con = Connections(d => ctx.N(d));
            uint c = 0xFFFFFFFFu;
            int t = faceTex[2];
            ctx.BoxPx(6, 0, 6, 10, 16, 10, t, c);
            for (int i = 0; i < 4; i++)
            {
                if ((con & (1 << i)) == 0) continue;
                ctx.rot = i;
                ctx.BoxPx(7, 12, 10, 9, 15, 16, t, c, 0, true);
                ctx.BoxPx(7, 6, 10, 9, 9, 16, t, c, 0, true);
            }
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            int con = Connections(d => w.GetState(pos.Offset(d)));
            boxes.Add(BoxUtil.Px(6, 0, 6, 10, 24, 10));
            for (int i = 0; i < 4; i++) if ((con & (1 << i)) != 0) boxes.Add(BoxUtil.Rot(BoxUtil.Px(6, 0, 10, 10, 24, 16), i));
        }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            int con = Connections(d => w.GetState(pos.Offset(d)));
            boxes.Add(BoxUtil.Px(6, 0, 6, 10, 16, 10));
            for (int i = 0; i < 4; i++) if ((con & (1 << i)) != 0) boxes.Add(BoxUtil.Rot(BoxUtil.Px(7, 6, 10, 9, 15, 16), i));
        }
    }

    public class WallBlock : Block
    {
        public WallBlock(Block src)
        {
            opaqueCube = false; lightOpacity = 0; isFullCubeShape = false;
            System.Array.Copy(src.faceTex, faceTex, 6); particleTex = src.particleTex;
            hardness = src.hardness; blastResistance = src.blastResistance; tool = src.tool; toolTier = src.toolTier; requiresTool = src.requiresTool; sound = src.sound;
        }
        public override int GetOccludingFaces(int meta) => 0;
        static bool ConnectsTo(ushort s, Dir d)
        {
            var b = Blocks.ByState[s];
            if (b is WallBlock || b is PaneBlock) return true;
            if (b is FenceGateBlock) { var gf = StateBits.FacingDir(s - b.baseState); return DirUtil.Axis(gf) != DirUtil.Axis(d); }
            return b.opaqueCube && !(b is LeavesBlock);
        }
        public static int Connections(System.Func<Dir, ushort> nb)
        {
            int m = 0;
            for (int i = 0; i < 4; i++) { Dir d = DirUtil.Horizontal[i]; if (ConnectsTo(nb(d), d)) m |= 1 << i; }
            return m;
        }
        public override void Emit(MeshCtx ctx, int meta)
        {
            int con = Connections(d => ctx.N(d));
            ushort above = ctx.N(Dir.Up);
            bool straight = (con == 0b0101 || con == 0b1010) && Blocks.ByState[above].isAir;
            uint c = ctx.TintColor(tint);
            if (!straight) ctx.Box(4f / 16, 0, 4f / 16, 12f / 16, 1, 12f / 16, faceTex, c);
            bool tall = !Blocks.ByState[above].isAir;
            for (int i = 0; i < 4; i++)
            {
                if ((con & (1 << i)) == 0) continue;
                ctx.rot = i;
                float z0 = straight ? 0 : 12f / 16;
                if (straight && (i == 2 || i == 3)) continue;
                if (straight) ctx.Box(5f / 16, 0, 0, 11f / 16, tall ? 1f : 14f / 16, 1, faceTex, c);
                else ctx.Box(5f / 16, 0, z0, 11f / 16, tall ? 1f : 14f / 16, 1, faceTex, c);
            }
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            int con = Connections(d => w.GetState(pos.Offset(d)));
            boxes.Add(BoxUtil.Px(4, 0, 4, 12, 24, 12));
            for (int i = 0; i < 4; i++) if ((con & (1 << i)) != 0) boxes.Add(BoxUtil.Rot(BoxUtil.Px(5, 0, 12, 11, 24, 16), i));
        }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            int con = Connections(d => w.GetState(pos.Offset(d)));
            boxes.Add(BoxUtil.Px(4, 0, 4, 12, 16, 12));
            for (int i = 0; i < 4; i++) if ((con & (1 << i)) != 0) boxes.Add(BoxUtil.Rot(BoxUtil.Px(5, 0, 12, 11, 14, 16), i));
        }
    }

    public class PaneBlock : Block
    {
        public int paneTex, edgeTex;
        public bool bars;
        public PaneBlock(string tex, string edge, RenderLayer l, bool isBars = false)
        {
            opaqueCube = false; lightOpacity = 0; layer = l; isFullCubeShape = false; bars = isBars;
            paneTex = Tex.Id(tex); edgeTex = Tex.Id(edge); SetAllTex(paneTex);
            hardness = isBars ? 5 : 0.3f; blastResistance = isBars ? 6 : 0.3f; sound = isBars ? SoundType.Metal : SoundType.Glass;
            if (isBars) { tool = ToolType.Pickaxe; requiresTool = true; }
            creativeTab = CreativeTab.Colored;
        }
        public override int GetOccludingFaces(int meta) => 0;
        static bool ConnectsTo(ushort s)
        {
            var b = Blocks.ByState[s];
            return b is PaneBlock || b is WallBlock || (b.opaqueCube && !(b is LeavesBlock)) || b is GlassBlock;
        }
        public static int Connections(System.Func<Dir, ushort> nb)
        {
            int m = 0;
            for (int i = 0; i < 4; i++) if (ConnectsTo(nb(DirUtil.Horizontal[i]))) m |= 1 << i;
            return m;
        }
        readonly int[] tex6 = new int[6];
        public override void Emit(MeshCtx ctx, int meta)
        {
            int con = Connections(d => ctx.N(d));
            for (int i = 0; i < 6; i++) tex6[i] = paneTex;
            tex6[0] = tex6[1] = edgeTex;
            uint c = 0xFFFFFFFFu;
            float a = 7f / 16, b = 9f / 16;
            if (con == 0)
            {
                ctx.Box(a, 0, a, b, 1, b, tex6, c);
                ctx.Box(a, 0, 0, b, 1, a, tex6, c, 1 << 2); ctx.Box(a, 0, b, b, 1, 1, tex6, c, 1 << 3);
                return;
            }
            ctx.Box(a, 0, a, b, 1, b, tex6, c, 0, true);
            for (int i = 0; i < 4; i++)
            {
                if ((con & (1 << i)) == 0) continue;
                ctx.rot = i;
                ctx.Box(a, 0, b, b, 1, 1, tex6, c, 1 << 3, true);
            }
            ctx.rot = 0;
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            int con = Connections(d => w.GetState(pos.Offset(d)));
            boxes.Add(BoxUtil.Px(7, 0, 7, 9, 16, 9));
            for (int i = 0; i < 4; i++) if ((con & (1 << i)) != 0) boxes.Add(BoxUtil.Rot(BoxUtil.Px(7, 0, 9, 9, 16, 16), i));
        }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => GetCollisionBoxes(meta, w, pos, boxes);
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if (bars) base.GetDrops(w, pos, meta, tool, drops, ref rng);
        }
    }

    public class CarpetBlock : Block
    {
        public float h;
        public CarpetBlock(string tex, float heightPx = 1)
        {
            opaqueCube = false; lightOpacity = 0; h = heightPx / 16f; hardness = 0.1f; sound = SoundType.Wool; SetAllTex(Tex.Id(tex));
            isFullCubeShape = false; creativeTab = CreativeTab.Colored; flammability = 60; fireSpread = 20; push = PushReaction.Destroy;
        }
        public override int GetOccludingFaces(int meta) => 1 << (int)Dir.Down;
        public override void Emit(MeshCtx ctx, int meta) => ctx.Box(0, 0, 0, 1, h, 1, faceTex, ctx.TintColor(tint));
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(new AABB(0, 0, 0, 1, h, 1));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(new AABB(0, 0, 0, 1, h, 1));
        public override bool CanSurvive(World w, Int3 pos, int meta) => !w.GetBlock(pos.Offset(Dir.Down)).isAir;
    }

    /// <summary>Falling blocks (sand, gravel, concrete powder, anvils...).</summary>
    public class GravityBlock : Block
    {
        public GravityBlock() { gravity = true; }
        public override void OnAdded(World w, Int3 pos, int meta, ushort oldState) => w.ScheduleTick(pos, this, 2);
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos) => w.ScheduleTick(pos, this, 2);
        public override void OnScheduledTick(World w, Int3 pos, int meta)
        {
            if (pos.y <= w.minY) return;
            Int3 below = pos.Offset(Dir.Down);
            if (FallingBlockEntity.CanFallThrough(w, below))
                FallingBlockEntity.Spawn(w, pos, w.GetState(pos));
        }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (rng.Chance(0.06f) && FallingBlockEntity.CanFallThrough(w, pos.Offset(Dir.Down)))
                Particles.FallingDust(w, pos.Center - Vector3.up * 0.55f, w.GetState(pos));
        }
    }

    /// <summary>Concrete powder turns into concrete when touching water.</summary>
    public class ConcretePowderBlock : GravityBlock
    {
        public string concreteId;
        public ConcretePowderBlock(string concrete) { concreteId = concrete; }
        bool TouchingWater(World w, Int3 pos)
        {
            for (int d = 1; d < 6; d++) if (w.IsWater(pos.Offset((Dir)d))) return true;
            return false;
        }
        public override void OnAdded(World w, Int3 pos, int meta, ushort oldState)
        {
            if (TouchingWater(w, pos)) { w.SetBlock(pos, Blocks.Get(concreteId)); return; }
            base.OnAdded(w, pos, meta, oldState);
        }
        public override void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (TouchingWater(w, pos)) { w.SetBlock(pos, Blocks.Get(concreteId)); return; }
            base.OnNeighborChanged(w, pos, meta, fromPos);
        }
    }

    public class OreBlock : Block
    {
        public string dropId; public int minDrop = 1, maxDrop = 1; public bool fortuneAffected = true;
        public OreBlock(string drop, int xpMin, int xpMax, int minCount = 1, int maxCount = 1)
        {
            dropId = drop; xpDropMin = xpMin; xpDropMax = xpMax; minDrop = minCount; maxDrop = maxCount; creativeTab = CreativeTab.Natural;
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if (!CanHarvestWith(tool)) return;
            int n = rng.Range(minDrop, maxDrop);
            int fortune = tool?.GetEnchant(Enchant.Fortune) ?? 0;
            if (fortuneAffected && fortune > 0)
            {
                int bonus = rng.Next(fortune + 2) - 1;
                if (bonus < 0) bonus = 0;
                n *= bonus + 1;
            }
            drops.Add(new ItemStack(dropId, n));
        }
    }

    /// <summary>Block that emits light and may pulse (redstone ore glows when touched).</summary>
    public class RedstoneOreBlock : OreBlock
    {
        public RedstoneOreBlock() : base("redstone", 1, 5, 4, 5) { stateCount = 2; randomTicks = true; }
        public override byte GetLightEmission(int meta) => (byte)(meta == 1 ? 9 : 0);
        public override void OnAttack(World w, Int3 pos, int meta, Player player) => Activate(w, pos, meta);
        public override void OnSteppedOn(World w, Int3 pos, int meta, Entity e) => Activate(w, pos, meta);
        public override bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit) { Activate(w, pos, meta); return false; }
        void Activate(World w, Int3 pos, int meta) { if (meta == 0) w.SetState(pos, State(1), 0); }
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng) { if (meta == 1) w.SetState(pos, State(0), 0); }
        public override void AnimateTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (meta == 1) Particles.RedstoneDust(w, pos.Center + new Vector3(rng.Range(-0.55f, 0.55f), rng.Range(-0.55f, 0.55f), rng.Range(-0.55f, 0.55f)), 1f);
        }
    }

    public class IceBlock : TranslucentCube
    {
        public IceBlock() : base(RenderLayer.Translucent, 2) { randomTicks = true; slipperiness = 0.98f; sound = SoundType.Glass; hardness = 0.5f; }
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (w.GetBlockLight(pos) > 11 - 1) Melt(w, pos);
        }
        void Melt(World w, Int3 pos)
        {
            if (w.dim == DimensionId.Nether) w.SetState(pos, 0);
            else w.SetBlock(pos, Blocks.Get("water"));
        }
        public override void OnBroken(World w, Int3 pos, int meta, Entity breaker)
        {
            var below = w.GetBlock(pos.Offset(Dir.Down));
            if (below.solid || below.isLiquid)
            {
                if (breaker is Player p && p.IsCreative) return;
                // set in a delayed fashion since BreakBlock sets air after this
                w.session?.Defer(() => { if (w.IsAir(pos)) w.SetBlock(pos, Blocks.Get(w.dim == DimensionId.Nether ? "air" : "water")); });
            }
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng) { }
    }

    public class MagmaBlock : Block
    {
        public MagmaBlock() { lightEmission = 3; this.T1("magma"); hardness = 0.5f; tool = ToolType.Pickaxe; requiresTool = true; creativeTab = CreativeTab.Natural; }
        public override void OnSteppedOn(World w, Int3 pos, int meta, Entity e)
        {
            if (e is LivingEntity le && !le.fireImmune && !le.sneaking && le.GetEffectLevel(Effect.FireResistance) < 0 && !(le.GetArmorEnchant(Enchant.FrostWalker) > 0))
                le.Hurt(DamageSource.HotFloor, 1f);
        }
    }

    public class SoulSandBlock : Block
    {
        public SoulSandBlock() { this.T1("soul_sand"); speedFactor = 0.4f; hardness = 0.5f; tool = ToolType.Shovel; sound = SoundType.SoulSand; creativeTab = CreativeTab.Natural; opaqueCube = true; }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(BoxUtil.Px(0, 0, 0, 16, 14, 16));
    }

    public class CobwebBlock : PlantBlock
    {
        public CobwebBlock() : base("cobweb", SoilKind.Any) { hardness = 4f; requiresTool = true; tool = ToolType.Sword; randomOffset = false; size = 1.05f; flammability = 0; fireSpread = 0; creativeTab = CreativeTab.Natural; }
        public override bool CanSurvive(World w, Int3 pos, int meta) => true;
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(new AABB(0, 0, 0, 1, 1, 1));
        public override Vector3 StuckSpeed(int meta) => new Vector3(0.25f, 0.05f, 0.25f);
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if (tool != null && (tool.item.toolType == ToolType.Shears)) drops.Add(new ItemStack(item, 1));
            else if (tool != null && tool.item.toolType == ToolType.Sword) drops.Add(new ItemStack("string", 1));
        }
    }

    /// <summary>Wall-attached decal (ladder).</summary>
    public class LadderBlock : Block
    {
        public int tex;
        public LadderBlock()
        {
            stateCount = 4; opaqueCube = false; solid = true; sturdy = false; lightOpacity = 0; layer = RenderLayer.Cutout; climbable = true;
            hardness = 0.4f; sound = SoundType.Ladder; tool = ToolType.Axe; tex = Tex.Id("ladder"); SetAllTex(tex); isFullCubeShape = false;
            creativeTab = CreativeTab.Functional; flammability = 0;
        }
        public override int GetOccludingFaces(int meta) => 0;
        /// <summary>facing = direction the ladder faces (away from wall).</summary>
        public override void Emit(MeshCtx ctx, int meta) => ctx.WallDecal(StateBits.FacingDir(meta), tex, 0xFFFFFFFFu, 1f / 16f);
        AABB Shape(int meta)
        {
            // canonical: facing north => wall is at south (z=0) side; ladder at z in [0, 3/16]
            return BoxUtil.Rot(BoxUtil.Px(0, 0, 0, 16, 16, 3), DirUtil.HorizIndex(StateBits.FacingDir(meta)));
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(Shape(meta));
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) => boxes.Add(Shape(meta));
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            if (!DirUtil.IsHorizontal(ctx.clickedFace))
            {
                // find a supporting wall
                for (int i = 0; i < 4; i++)
                {
                    Dir d = DirUtil.Horizontal[i];
                    if (ctx.world.IsSturdy(ctx.pos.Offset(DirUtil.Opposite(d)), d)) return State(i);
                }
                return 0;
            }
            return State(DirUtil.HorizIndex(ctx.clickedFace));
        }
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            Dir f = StateBits.FacingDir(meta);
            Int3 wall = pos.Offset(DirUtil.Opposite(f));
            return w.GetBlock(wall).opaqueCube || w.IsSturdy(wall, f);
        }
        public override bool CanPlaceAt(World w, Int3 pos, ushort state) => CanSurvive(w, pos, state - baseState);
    }

    /// <summary>Vines: faces bitmask N,E,S,W (bits0-3) + up (bit4).</summary>
    public class VineBlock : Block
    {
        public int tex;
        public VineBlock(string t = "vine", TintType tt = TintType.Foliage)
        {
            stateCount = 32; opaqueCube = false; solid = false; sturdy = false; lightOpacity = 0; layer = RenderLayer.Cutout; climbable = true;
            replaceable = true; hardness = 0.2f; sound = SoundType.Grass; tex = Tex.Id(t); SetAllTex(tex); tint = tt; randomTicks = true;
            isFullCubeShape = false; creativeTab = CreativeTab.Natural; push = PushReaction.Destroy; flammability = 15; fireSpread = 100;
        }
        public override int DefaultMeta => 1;
        public override int GetOccludingFaces(int meta) => 0;
        public override void Emit(MeshCtx ctx, int meta)
        {
            uint c = ctx.TintColor(tint);
            for (int i = 0; i < 4; i++)
                if ((meta & (1 << i)) != 0) ctx.WallDecal(DirUtil.Opposite(DirUtil.Horizontal[i]), tex, c, 0.8f / 16f);
            if ((meta & 16) != 0)
                ctx.Quad(new Vector3(0, 15.2f / 16, 0), new Vector3(0, 15.2f / 16, 1), new Vector3(1, 15.2f / 16, 1), new Vector3(1, 15.2f / 16, 0),
                    new Vector2(0, 0), new Vector2(0, 1), new Vector2(1, 1), new Vector2(1, 0), tex, c, 0.5f, RenderLayer.Cutout, true);
        }
        public override void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes) { }
        public override void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            for (int i = 0; i < 4; i++)
                if ((meta & (1 << i)) != 0) boxes.Add(BoxUtil.Rot(BoxUtil.Px(0, 0, 15, 16, 16, 16), i));
            if ((meta & 16) != 0) boxes.Add(BoxUtil.Px(0, 15, 0, 16, 16, 16));
            if (boxes.Count == 0) boxes.Add(BoxUtil.Px(0, 0, 0, 16, 16, 1));
        }
        /// <summary>Bit i set = attached to the wall in direction Horizontal[i].</summary>
        public override ushort GetPlacementState(ref PlaceContext ctx)
        {
            Dir wallDir = DirUtil.Opposite(ctx.clickedFace);
            if (ctx.clickedFace == Dir.Down) return State(16);
            if (!DirUtil.IsHorizontal(wallDir)) return 0;
            ushort cur = ctx.world.GetState(ctx.pos);
            int m = Blocks.ByState[cur] == this ? cur - baseState : 0;
            return State(m | (1 << DirUtil.HorizIndex(wallDir)));
        }
        public override bool CanSurvive(World w, Int3 pos, int meta)
        {
            int keep = 0;
            for (int i = 0; i < 4; i++)
                if ((meta & (1 << i)) != 0)
                {
                    Int3 wall = pos.Offset(DirUtil.Horizontal[i]);
                    var b = w.GetBlock(wall);
                    if (b.opaqueCube || b is LeavesBlock) keep |= 1 << i;
                    else if (w.GetBlock(pos.Offset(Dir.Up)) == this && (w.GetMeta(pos.Offset(Dir.Up)) & (1 << i)) != 0) keep |= 1 << i;
                }
            if ((meta & 16) != 0 && w.GetBlock(pos.Offset(Dir.Up)).opaqueCube) keep |= 16;
            return keep != 0;
        }
        public override void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng)
        {
            if (rng.Next(4) != 0) return;
            Int3 below = pos.Offset(Dir.Down);
            if (below.y > w.minY && w.IsAir(below))
            {
                int m = meta & 15;
                if (m != 0 && rng.Chance(0.5f)) w.SetState(below, State(m & (rng.Next(16) | 1 << rng.Next(4))), SetFlags.Hooks);
            }
        }
        public override void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            if (tool != null && tool.item.toolType == ToolType.Shears) drops.Add(new ItemStack(item, 1));
        }
    }
}
