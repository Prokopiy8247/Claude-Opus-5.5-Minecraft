using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    // =====================================================================================================
    //  Registration
    // =====================================================================================================
    public static partial class StructureManager
    {
        static partial void RegisterTypesImpl(List<StructureType> list)
        {
            // overworld
            list.Add(new VillageStructure());
            list.Add(new StrongholdStructure());
            list.Add(new MineshaftStructure());
            list.Add(new DungeonStructure());
            list.Add(new DesertPyramidStructure());
            list.Add(new JungleTempleStructure());
            list.Add(new SwampHutStructure());
            list.Add(new IglooStructure());
            list.Add(new PillagerOutpostStructure());
            list.Add(new ShipwreckStructure());
            list.Add(new OceanRuinStructure());
            list.Add(new BuriedTreasureStructure());
            list.Add(new RuinedPortalStructure());
            list.Add(new WoodlandMansionStructure());
            list.Add(new AncientCityStructure());
            list.Add(new TrialChambersStructure());
            list.Add(new TrailRuinsStructure());
            // nether
            list.Add(new FortressStructure());
            list.Add(new BastionStructure());
            // end
            list.Add(new EndCityStructure());
        }
    }

    /// <summary>Small integer helpers shared by the structure files.</summary>
    public static class Sr
    {
        public static int Clamp(int v, int a, int b) => v < a ? a : (v > b ? b : v);
        public static int Abs(int v) => v < 0 ? -v : v;
    }

    // =====================================================================================================
    //  Local-frame builder
    // =====================================================================================================
    /// <summary>
    /// Writes blocks for one piece into the chunk being decorated. Coordinates are local to a rotated
    /// footprint (sx x sz, origin at the world min corner ox/oz). Rotation r turns local north (+Z) into
    /// world Horizontal[r]; positions and facing metas are rotated consistently. Writes outside the chunk
    /// are dropped, so a piece is emitted identically by every chunk it overlaps.
    /// Randomness for block-level detail must come from <see cref="R"/> (position hash), never from a
    /// sequential RNG whose draws could depend on clipping.
    /// </summary>
    public sealed class SB
    {
        public readonly ChunkWriter cw;
        public readonly WorldGenerator gen;
        public readonly Chunk chunk;
        readonly int cx0, cz0, minY, maxY;
        public int ox, oy, oz, sx = 1, sz = 1, rot;
        public readonly int seed;
        /// <summary>World y at or below which carved (air) cells become water; used by underwater builds.</summary>
        public int waterTop = int.MinValue;

        public SB(StructureWriter sw, int seed)
        {
            cw = sw.cw; gen = sw.gen; chunk = cw.c;
            cx0 = chunk.cx << 4; cz0 = chunk.cz << 4; minY = cw.MinY; maxY = cw.MaxY;
            this.seed = seed;
        }

        SB(SB o)
        {
            cw = o.cw; gen = o.gen; chunk = o.chunk;
            cx0 = o.cx0; cz0 = o.cz0; minY = o.minY; maxY = o.maxY;
            seed = o.seed; waterTop = o.waterTop;
        }

        /// <summary>
        /// Child builder for a rectangle of this frame, rotated by subRot quarter turns relative to it.
        /// (plx0, plz0) is the rectangle's min corner in this frame; subSx/subSz are the child's own local sizes.
        /// </summary>
        public SB Sub(int plx0, int ply0, int plz0, int subSx, int subSz, int subRot)
        {
            int ex = (subRot & 1) == 1 ? subSz : subSx, ez = (subRot & 1) == 1 ? subSx : subSz;
            ToWorld(plx0, plz0, out int ax, out int az);
            ToWorld(plx0 + ex - 1, plz0 + ez - 1, out int bx, out int bz);
            var s = new SB(this);
            s.Frame(Math.Min(ax, bx), oy + ply0, Math.Min(az, bz), subSx, subSz, rot + subRot);
            return s;
        }

        public SB Frame(int ox, int oy, int oz, int sx, int sz, int rot)
        {
            this.ox = ox; this.oy = oy; this.oz = oz; this.sx = Math.Max(1, sx); this.sz = Math.Max(1, sz); this.rot = rot & 3;
            return this;
        }
        /// <summary>Unrotated world frame (local == world offsets from the origin).</summary>
        public SB World(int ox = 0, int oy = 0, int oz = 0) => Frame(ox, oy, oz, 1, 1, 0);

        // ------------------------------------------------------------------ coordinate mapping
        public void ToWorld(int lx, int lz, out int wx, out int wz)
        {
            switch (rot)
            {
                case 1: wx = ox + lz; wz = oz + (sx - 1 - lx); break;
                case 2: wx = ox + (sx - 1 - lx); wz = oz + (sz - 1 - lz); break;
                case 3: wx = ox + (sz - 1 - lz); wz = oz + lx; break;
                default: wx = ox + lx; wz = oz + lz; break;
            }
        }
        /// <summary>Local horizontal direction to world direction.</summary>
        public Dir D(Dir d) => DirUtil.IsHorizontal(d) ? DirUtil.FromHorizIndex(DirUtil.HorizIndex(d) + rot) : d;
        public int WX(int lx, int lz) { ToWorld(lx, lz, out int x, out _); return x; }
        public int WZ(int lx, int lz) { ToWorld(lx, lz, out _, out int z); return z; }

        bool Inside(int wx, int wy, int wz) => wx >= cx0 && wx < cx0 + 16 && wz >= cz0 && wz < cz0 + 16 && wy >= minY && wy < maxY;
        public bool Owns(int lx, int ly, int lz) { ToWorld(lx, lz, out int wx, out int wz); return Inside(wx, oy + ly, wz); }
        /// <summary>Whether any column of the local box lies in this chunk (cheap early-out for detail work).</summary>
        public bool TouchesXZ(int lx0, int lz0, int lx1, int lz1)
        {
            ToWorld(lx0, lz0, out int ax, out int az); ToWorld(lx1, lz1, out int bx, out int bz);
            int x0 = Math.Min(ax, bx), x1 = Math.Max(ax, bx), z0 = Math.Min(az, bz), z1 = Math.Max(az, bz);
            return x0 < cx0 + 16 && x1 >= cx0 && z0 < cz0 + 16 && z1 >= cz0;
        }

        // ------------------------------------------------------------------ reads
        public ushort Get(int lx, int ly, int lz)
        {
            ToWorld(lx, lz, out int wx, out int wz); int wy = oy + ly;
            return Inside(wx, wy, wz) ? chunk.Get(wx - cx0, wy, wz - cz0) : (ushort)0;
        }
        public Block BlockAt(int lx, int ly, int lz) => Blocks.ByState[Get(lx, ly, lz)];
        public bool IsAir(int lx, int ly, int lz) => Get(lx, ly, lz) == 0;
        /// <summary>World-space read of the current chunk (0 when outside).</summary>
        public ushort GetWorld(int wx, int wy, int wz) => Inside(wx, wy, wz) ? chunk.Get(wx - cx0, wy, wz - cz0) : (ushort)0;

        // ------------------------------------------------------------------ writes
        public void Set(int lx, int ly, int lz, ushort localState)
        {
            ToWorld(lx, lz, out int wx, out int wz); int wy = oy + ly;
            if (!Inside(wx, wy, wz)) return;
            if (localState == 0 && wy <= waterTop) localState = Blocks.Water.DefaultState;
            var b = Blocks.ByState[localState];
            ushort s = localState;
            if (rot != 0 && s != 0) s = b.State(RotMeta(b, s - b.baseState, rot));
            Put(wx, wy, wz, s, b, null);
        }
        public void Set(int lx, int ly, int lz, string id, int meta = -1)
        {
            var b = Blocks.Get(id);
            if (b == null) { Missing(id); return; }
            Set(lx, ly, lz, meta < 0 ? b.DefaultState : b.State(meta));
        }
        public void Air(int lx, int ly, int lz) => Set(lx, ly, lz, (ushort)0);

        /// <summary>World-coordinate write (no rotation of meta).</summary>
        public void SetWorld(int wx, int wy, int wz, ushort s)
        {
            if (!Inside(wx, wy, wz)) return;
            Put(wx, wy, wz, s, Blocks.ByState[s], null);
        }

        void Put(int wx, int wy, int wz, ushort s, Block b, BlockEntity be)
        {
            int lx = wx - cx0, lz = wz - cz0;
            int idx = Chunk.ModIndex(lx, wy - minY, lz);
            if (chunk.blockEntities.Count > 0) chunk.blockEntities.Remove(idx);
            chunk.SetRaw(lx, wy, lz, s);
            if (s != 0 && b.HasBlockEntity)
            {
                var p = new Int3(wx, wy, wz);
                if (be == null) be = b.CreateBlockEntity(null, p);
                if (be != null) { be.pos = p; chunk.blockEntities[idx] = be; }
            }
        }

        /// <summary>Places a block and returns its (new) block entity when the position is inside this chunk.</summary>
        public BlockEntity SetWithEntity(int lx, int ly, int lz, string id, int meta, BlockEntity be)
        {
            var b = Blocks.Get(id);
            if (b == null) { Missing(id); return null; }
            ToWorld(lx, lz, out int wx, out int wz); int wy = oy + ly;
            if (!Inside(wx, wy, wz)) return null;
            int m = meta < 0 ? b.DefaultMeta : meta;
            if (rot != 0) m = RotMeta(b, m, rot);
            Put(wx, wy, wz, b.State(m), b, be);
            chunk.blockEntities.TryGetValue(Chunk.ModIndex(wx - cx0, wy - minY, wz - cz0), out var placed);
            return placed;
        }

        public void Fill(int x0, int y0, int z0, int x1, int y1, int z1, ushort s)
        {
            if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
            if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
            if (z0 > z1) { int t = z0; z0 = z1; z1 = t; }
            if (!TouchesXZ(x0, z0, x1, z1)) return;
            for (int y = y0; y <= y1; y++) for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) Set(x, y, z, s);
        }
        public void Fill(int x0, int y0, int z0, int x1, int y1, int z1, string id, int meta = -1)
        {
            var b = Blocks.Get(id);
            if (b == null) { Missing(id); return; }
            Fill(x0, y0, z0, x1, y1, z1, meta < 0 ? b.DefaultState : b.State(meta));
        }
        public void Clear(int x0, int y0, int z0, int x1, int y1, int z1) => Fill(x0, y0, z0, x1, y1, z1, (ushort)0);

        /// <summary>Fill only positions that are currently air or replaceable (plants, water optional).</summary>
        public void FillSoft(int x0, int y0, int z0, int x1, int y1, int z1, string id, bool replaceLiquid = true)
        {
            var b = Blocks.Get(id); if (b == null) { Missing(id); return; }
            if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
            if (z0 > z1) { int t = z0; z0 = z1; z1 = t; }
            if (!TouchesXZ(x0, z0, x1, z1)) return;
            for (int y = y0; y <= y1; y++) for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++)
                    {
                        var cur = BlockAt(x, y, z);
                        if (cur.isAir || (cur.replaceable && (replaceLiquid || !cur.isLiquid)) || cur is PlantBlock || cur is LeavesBlock)
                            Set(x, y, z, b.DefaultState);
                    }
        }

        /// <summary>Four walls (perimeter only) between y0..y1.</summary>
        public void Walls(int x0, int y0, int z0, int x1, int y1, int z1, string id, int meta = -1)
        {
            Fill(x0, y0, z0, x1, y1, z0, id, meta);
            Fill(x0, y0, z1, x1, y1, z1, id, meta);
            Fill(x0, y0, z0, x0, y1, z1, id, meta);
            Fill(x1, y0, z0, x1, y1, z1, id, meta);
        }
        /// <summary>Box shell with a cleared interior.</summary>
        public void Hollow(int x0, int y0, int z0, int x1, int y1, int z1, string id)
        {
            var b = Blocks.Get(id); if (b == null) { Missing(id); return; }
            ushort s = b.DefaultState;
            if (!TouchesXZ(x0, z0, x1, z1)) return;
            for (int y = y0; y <= y1; y++) for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++)
                    {
                        bool edge = x == x0 || x == x1 || y == y0 || y == y1 || z == z0 || z == z1;
                        Set(x, y, z, edge ? s : (ushort)0);
                    }
        }

        /// <summary>Randomly swaps a block with variants by position hash (mossy/cracked bricks...).</summary>
        public void Fill3(int x0, int y0, int z0, int x1, int y1, int z1, string a, string b, float pb, string c = null, float pc = 0f, int salt = 0)
        {
            var A = Blocks.Get(a); var B = Blocks.Get(b ?? a); var C = c != null ? Blocks.Get(c) : null;
            if (A == null) { Missing(a); return; }
            if (B == null) B = A;
            if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
            if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
            if (z0 > z1) { int t = z0; z0 = z1; z1 = t; }
            if (!TouchesXZ(x0, z0, x1, z1)) return;
            for (int y = y0; y <= y1; y++) for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++)
                    {
                        float r = R(x, y, z, salt);
                        var pick = r < pb ? B : (C != null && r < pb + pc ? C : A);
                        Set(x, y, z, pick.DefaultState);
                    }
        }
        public void Walls3(int x0, int y0, int z0, int x1, int y1, int z1, string a, string b, float pb, string c = null, float pc = 0f, int salt = 0)
        {
            Fill3(x0, y0, z0, x1, y1, z0, a, b, pb, c, pc, salt);
            Fill3(x0, y0, z1, x1, y1, z1, a, b, pb, c, pc, salt);
            Fill3(x0, y0, z0, x0, y1, z1, a, b, pb, c, pc, salt);
            Fill3(x1, y0, z0, x1, y1, z1, a, b, pb, c, pc, salt);
        }

        // ------------------------------------------------------------------ oriented blocks
        /// <summary>Stairs ascending toward local direction f.</summary>
        public void Stairs(int lx, int ly, int lz, string id, Dir f, bool top = false)
            => Set(lx, ly, lz, id, DirUtil.HorizIndex(f) | (top ? 4 : 0));
        public void StairsRow(int x0, int ly, int z0, int x1, int z1, string id, Dir f, bool top = false)
        {
            var b = Blocks.Get(id); if (b == null) { Missing(id); return; }
            Fill(x0, ly, z0, x1, ly, z1, b.State(DirUtil.HorizIndex(f) | (top ? 4 : 0)));
        }
        /// <summary>Slab: 0 bottom, 1 top, 2 double.</summary>
        public void Slab(int lx, int ly, int lz, string id, bool top = false) => Set(lx, ly, lz, id, top ? 1 : 0);
        /// <summary>Two-block door; f = direction a player walks through it when entering.</summary>
        public void Door(int lx, int ly, int lz, string id, Dir f, bool hingeRight = false, bool open = false)
        {
            int m = DirUtil.HorizIndex(f) | (hingeRight ? 16 : 0) | (open ? 4 : 0);
            Set(lx, ly, lz, id, m);
            Set(lx, ly + 1, lz, id, m | 8);
        }
        /// <summary>Bed with its foot at (lx,lz) and head one block toward f.</summary>
        public void Bed(int lx, int ly, int lz, string color, Dir f)
        {
            string id = color + "_bed";
            var o = DirUtil.Offset[(int)f];
            Set(lx, ly, lz, id, DirUtil.HorizIndex(f));
            Set(lx + o.x, ly, lz + o.z, id, DirUtil.HorizIndex(f) | 4);
        }
        /// <summary>Wall torch leaning toward f (attached to the block behind it), or standing if f is Up.</summary>
        public void Torch(int lx, int ly, int lz, Dir f = Dir.Up, string id = "torch")
            => Set(lx, ly, lz, id, DirUtil.IsHorizontal(f) ? 1 + DirUtil.HorizIndex(f) : 0);
        /// <summary>Ladder facing f (its wall is behind it).</summary>
        public void Ladder(int lx, int ly, int lz, Dir f) => Set(lx, ly, lz, "ladder", DirUtil.HorizIndex(f));
        public void Ladders(int lx, int ly0, int ly1, int lz, Dir f) { for (int y = ly0; y <= ly1; y++) Ladder(lx, y, lz, f); }
        /// <summary>Block facing f with the facing in the low two bits (furnaces, lecterns, frames...).</summary>
        public void Facing(int lx, int ly, int lz, string id, Dir f, int extra = 0) => Set(lx, ly, lz, id, DirUtil.HorizIndex(f) | extra);
        /// <summary>Six-way facing blocks (end rods, barrels, dispensers, amethyst): meta = Dir.</summary>
        public void Facing6(int lx, int ly, int lz, string id, Dir f, int extra = 0) => Set(lx, ly, lz, id, (int)f | extra);
        /// <summary>Log/pillar axis: 0 = y, 1 = local x, 2 = local z.</summary>
        public void Log(int lx, int ly, int lz, string id, int axis = 0) => Set(lx, ly, lz, id, axis);
        public void Lantern(int lx, int ly, int lz, bool hanging = false, string id = "lantern") => Set(lx, ly, lz, id, hanging ? 1 : 0);
        /// <summary>Vine hugging the wall on local side f.</summary>
        public void Vine(int lx, int ly, int lz, Dir wallSide) => Set(lx, ly, lz, "vine", 1 << DirUtil.HorizIndex(wallSide));
        /// <summary>Straight rail along local x (axisX) or z.</summary>
        public void Rail(int lx, int ly, int lz, bool alongX, string id = "rail") => Set(lx, ly, lz, id, alongX ? 1 : 0);

        // ------------------------------------------------------------------ containers & spawners
        /// <summary>Chest facing f, with a loot table. Returns the entity (null outside this chunk).</summary>
        public ContainerEntity Chest(int lx, int ly, int lz, string loot, Dir f, string block = "chest")
        {
            var be = SetWithEntity(lx, ly, lz, block, DirUtil.HorizIndex(f), null) as ContainerEntity;
            if (be != null) be.lootTable = loot;
            return be;
        }
        /// <summary>Barrel facing f (Up by default).</summary>
        public ContainerEntity Barrel(int lx, int ly, int lz, string loot, Dir f = Dir.Up)
        {
            var be = SetWithEntity(lx, ly, lz, "barrel", (int)f, null) as ContainerEntity;
            if (be != null) be.lootTable = loot;
            return be;
        }
        public void Spawner(int lx, int ly, int lz, string mob)
            => SetWithEntity(lx, ly, lz, "spawner", 0, new SpawnerEntity { mob = mob });
        public void TrialSpawner(int lx, int ly, int lz, string mob)
            => SetWithEntity(lx, ly, lz, "trial_spawner", 0, new TrialSpawnerEntity { mob = mob });
        /// <summary>Brewing stand holding the given potion ids in its bottle slots.</summary>
        public void BrewingStand(int lx, int ly, int lz, params string[] potions)
        {
            var be = SetWithEntity(lx, ly, lz, "brewing_stand", 0, null) as BrewingStandEntity;
            if (be == null) return;
            for (int i = 0; i < potions.Length && i < 3; i++)
            {
                var s = new ItemStack("potion", 1);
                if (s.item == null) continue;
                s.Set("potion", potions[i]);
                be.items[i] = s;
            }
        }
        /// <summary>Lectern with a book on it.</summary>
        public void Lectern(int lx, int ly, int lz, Dir f, bool withBook)
        {
            var be = SetWithEntity(lx, ly, lz, "lectern", DirUtil.HorizIndex(f) | (withBook ? 4 : 0), null) as LecternEntity;
            if (be != null && withBook) { var s = new ItemStack("book", 1); if (s.item != null) be.book = s; }
        }

        // ------------------------------------------------------------------ mobs
        /// <summary>Queues a mob at a local block position (spawned when the chunk finishes generating).</summary>
        public void Mob(int lx, int ly, int lz, string mob, int variant = -1, bool baby = false)
        {
            ToWorld(lx, lz, out int wx, out int wz);
            int wy = oy + ly;
            if (!Inside(wx, Math.Max(minY, Math.Min(maxY - 1, wy)), wz)) return;
            string data = null;
            if (variant >= 0 || baby)
            {
                var d = new Dictionary<string, string>();
                if (variant >= 0) d["variant"] = variant.ToString();
                if (baby) d["baby"] = "-24000";
                d["persist"] = "1";
                data = SaveManager.EncodeDict(d);
            }
            cw.QueueEntity(new SavedEntity { type = mob, x = wx + 0.5f, y = wy, z = wz + 0.5f, data = data });
        }

        // ------------------------------------------------------------------ terrain fitting
        /// <summary>Fills from ly-1 downward with id until solid ground (max depth).</summary>
        public void Foundation(int lx, int lz, int ly, string id, int maxDepth = 24)
        {
            var b = Blocks.Get(id); if (b == null) { Missing(id); return; }
            ToWorld(lx, lz, out int wx, out int wz);
            if (!Inside(wx, oy + ly - 1, wz) && !Inside(wx, Math.Max(minY, oy + ly - maxDepth), wz)) return;
            for (int y = ly - 1; y >= ly - maxDepth; y--)
            {
                int wy = oy + y;
                if (wy < minY) break;
                ushort cur = chunk.Get(wx - cx0, wy, wz - cz0);
                var cb = Blocks.ByState[cur];
                if (cur != 0 && !cb.isLiquid && !cb.replaceable && !(cb is LeavesBlock) && !(cb is PlantBlock) && !(cb is SnowLayerBlock) && !IsLog(cb)) break;
                Put(wx, wy, wz, b.DefaultState, b, null);
            }
        }
        public void FoundationArea(int x0, int z0, int x1, int z1, int ly, string id, int maxDepth = 24)
        {
            if (!TouchesXZ(x0, z0, x1, z1)) return;
            for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) Foundation(x, z, ly, id, maxDepth);
        }
        /// <summary>Clears head-room above a footprint (removes terrain, trees and plants).</summary>
        public void ClearUp(int x0, int z0, int x1, int z1, int ly, int height)
        {
            if (!TouchesXZ(x0, z0, x1, z1)) return;
            Clear(x0, ly, z0, x1, ly + height - 1, z1);
            // chop tree remnants that overhang the roof line
            for (int z = z0; z <= z1; z++)
                for (int x = x0; x <= x1; x++)
                    for (int y = ly + height; y < ly + height + 12; y++)
                    {
                        var cb = BlockAt(x, y, z);
                        if (cb is LeavesBlock || IsLog(cb) || cb is PlantBlock || cb is SnowLayerBlock || cb is VineBlock) Set(x, y, z, (ushort)0);
                        else if (!cb.isAir) break;
                    }
        }
        static bool IsLog(Block b) => b is PillarBlock && (b.id.EndsWith("_log") || b.id.EndsWith("_wood") || b.id.EndsWith("_stem"));

        /// <summary>Top solid terrain y of a local column in this chunk (skips plants, leaves, logs, snow). int.MinValue when unknown.</summary>
        public int TerrainTop(int lx, int lz, int fromLy, int toLy)
        {
            ToWorld(lx, lz, out int wx, out int wz);
            if (wx < cx0 || wx >= cx0 + 16 || wz < cz0 || wz >= cz0 + 16) return int.MinValue;
            for (int y = fromLy; y >= toLy; y--)
            {
                int wy = oy + y;
                if (wy < minY || wy >= maxY) continue;
                ushort s = chunk.Get(wx - cx0, wy, wz - cz0);
                if (s == 0) continue;
                var b = Blocks.ByState[s];
                if (b is PlantBlock || b is LeavesBlock || b is SnowLayerBlock || b is TallPlantBlock || b is VineBlock || IsLog(b)) continue;
                return y;
            }
            return int.MinValue;
        }

        // ------------------------------------------------------------------ randomness
        /// <summary>Position-hash random in [0,1) for this piece (independent of build order and clipping).</summary>
        public float R(int lx, int ly, int lz, int salt = 0)
        {
            ToWorld(lx, lz, out int wx, out int wz);
            return (Hash.Get(seed ^ (salt * 0x3C6EF372), wx, oy + ly, wz) & 0xFFFFFF) / 16777216f;
        }
        public bool P(int lx, int ly, int lz, float p, int salt = 0) => R(lx, ly, lz, salt) < p;

        // ------------------------------------------------------------------ meta rotation
        public static int RotMeta(Block b, int m, int rot)
        {
            rot &= 3;
            if (rot == 0) return m;
            if (b is StairsBlock || b is DoorBlock || b is TrapdoorBlock || b is FenceGateBlock || b is HorizontalBlock || b is LadderBlock
                || b is ChestBlock || b is EnderChestBlock || b is FurnaceBlock || b is BedBlock || b is AnvilBlock || b is LecternBlock
                || b is CampfireBlock || b is BellBlock || b is EndPortalFrameBlock || b is GrindstoneBlock || b is StonecutterBlock
                || b is CocoaBlock || b is ShelfBlock || b is RepeaterBlock || b is ComparatorBlock || b is AttachedSwitch)
                return (m & ~3) | (((m & 3) + rot) & 3);
            if (b is TorchBlock)
            {
                int f = m & 7;
                return f >= 1 && f <= 4 ? (m & ~7) | (1 + ((f - 1 + rot) & 3)) : m;
            }
            if (b is PillarBlock) return (rot & 1) == 1 && (m == 1 || m == 2) ? 3 - m : m;
            if (b is VineBlock)
            {
                int faces = m & 15, nf = 0;
                for (int i = 0; i < 4; i++) if ((faces & (1 << i)) != 0) nf |= 1 << ((i + rot) & 3);
                return (m & ~15) | nf;
            }
            if (b is SkullBlock) return m >= 8 ? 8 + (((m - 8) + rot) & 3) : (m + rot * 2) & 7;
            if (b is RailBlock) return (m & ~15) | RotRail(m & 15, rot);
            if (b is BarrelBlock || b is EndRodBlock || b is AmethystClusterBlock || b is ShulkerBoxBlock || b is DispenserBlock)
            {
                int f = m & 7;
                if (f >= 2 && f <= 5) return (m & ~7) | (int)DirUtil.FromHorizIndex(DirUtil.HorizIndex((Dir)f) + rot);
                return m;
            }
            return m;
        }
        static int RotRail(int shape, int rot)
        {
            for (int i = 0; i < rot; i++)
            {
                switch (shape)
                {
                    case 0: shape = 1; break;
                    case 1: shape = 0; break;
                    case 2: shape = 5; break;   // ascending east -> south
                    case 5: shape = 3; break;   // south -> west
                    case 3: shape = 4; break;   // west -> north
                    case 4: shape = 2; break;   // north -> east
                    case 6: shape = 7; break;
                    case 7: shape = 8; break;
                    case 8: shape = 9; break;
                    case 9: shape = 6; break;
                }
            }
            return shape;
        }

        static readonly ConcurrentDictionary<string, bool> missing = new ConcurrentDictionary<string, bool>();
        static void Missing(string id)
        {
            // logged once per id: a missing id would silently leave holes
            if (id != null && missing.TryAdd(id, true)) Debug.LogWarning("[Structures] unknown block id '" + id + "'");
        }
    }

    // =====================================================================================================
    //  Pieces
    // =====================================================================================================
    /// <summary>A piece with a rotated local footprint. Layout (frame + parameters) is fixed in Create().</summary>
    public abstract class FramedPiece : StructurePiece
    {
        public int ox, oy, oz, sx, sz, rot;
        public int WorldSizeX => (rot & 1) == 1 ? sz : sx;
        public int WorldSizeZ => (rot & 1) == 1 ? sx : sz;

        /// <summary>Sets the frame and the box (with vertical extent yDown..yUp relative to oy, horizontal padding pad).</summary>
        public FramedPiece Place(int ox, int oy, int oz, int sx, int sz, int rot, int yDown, int yUp, int pad = 0)
        {
            this.ox = ox; this.oy = oy; this.oz = oz; this.sx = Math.Max(1, sx); this.sz = Math.Max(1, sz); this.rot = rot & 3;
            box = new BBox(ox - pad, oy - yDown, oz - pad, ox + WorldSizeX + pad, oy + yUp + 1, oz + WorldSizeZ + pad);
            return this;
        }
        /// <summary>Frame whose world footprint is centred on (cx, cz).</summary>
        public FramedPiece PlaceCentered(int cx, int oy, int cz, int sx, int sz, int rot, int yDown, int yUp, int pad = 0)
        {
            int wsx = (rot & 1) == 1 ? sz : sx, wsz = (rot & 1) == 1 ? sx : sz;
            return Place(cx - wsx / 2, oy, cz - wsz / 2, sx, sz, rot, yDown, yUp, pad);
        }

        public override void Build(StructureWriter w)
        {
            var b = new SB(w, seed);
            b.Frame(ox, oy, oz, sx, sz, rot);
            Emit(b);
        }
        protected abstract void Emit(SB b);

        /// <summary>Rotation that makes the local south wall (z = 0) face world direction 'front'.</summary>
        public static int RotForFront(Dir front) => (DirUtil.HorizIndex(front) + 2) & 3;
        /// <summary>World box of a local rectangle for a given frame (used for collision tests during layout).</summary>
        public static BBox LocalBox(int ox, int oy, int oz, int sx, int sz, int rot, int lx0, int ly0, int lz0, int lx1, int ly1, int lz1)
        {
            var t = new TmpFrame { ox = ox, oz = oz, sx = sx, sz = sz, rot = rot & 3 };
            t.Map(lx0, lz0, out int ax, out int az); t.Map(lx1, lz1, out int bx, out int bz);
            return new BBox(Math.Min(ax, bx), oy + ly0, Math.Min(az, bz), Math.Max(ax, bx) + 1, oy + ly1 + 1, Math.Max(az, bz) + 1);
        }
        struct TmpFrame
        {
            public int ox, oz, sx, sz, rot;
            public void Map(int lx, int lz, out int wx, out int wz)
            {
                switch (rot)
                {
                    case 1: wx = ox + lz; wz = oz + (sx - 1 - lx); break;
                    case 2: wx = ox + (sx - 1 - lx); wz = oz + (sz - 1 - lz); break;
                    case 3: wx = ox + (sz - 1 - lz); wz = oz + lx; break;
                    default: wx = ox + lx; wz = oz + lz; break;
                }
            }
        }
    }

    // =====================================================================================================
    //  Terrain queries used during layout
    // =====================================================================================================
    public static class StructureTerrain
    {
        /// <summary>Best available ground height at a column (exact terrain density for the overworld).</summary>
        public static int Ground(WorldGenerator g, int x, int z)
        {
            if (g is OverworldGenerator og) return og.StructureSurfaceY(x, z);
            if (g is EndGenerator eg) return eg.SurfaceAt(x, z, out int top) ? top : -1;
            return g.ApproxSurface(x, z);
        }

        /// <summary>Median of the corner/centre ground heights of a world rectangle (robust floor level for a building).</summary>
        public static int FloorLevel(WorldGenerator g, int x0, int z0, int x1, int z1)
        {
            int cx = (x0 + x1) >> 1, cz = (z0 + z1) >> 1;
            var hs = new int[5] { Ground(g, x0, z0), Ground(g, x1, z0), Ground(g, x0, z1), Ground(g, x1, z1), Ground(g, cx, cz) };
            Array.Sort(hs);
            return hs[2];
        }

        /// <summary>Height spread (max - min) over corners and centre of a rectangle.</summary>
        public static int Relief(WorldGenerator g, int x0, int z0, int x1, int z1)
        {
            int cx = (x0 + x1) >> 1, cz = (z0 + z1) >> 1;
            int a = Ground(g, x0, z0), b = Ground(g, x1, z0), c = Ground(g, x0, z1), d = Ground(g, x1, z1), e = Ground(g, cx, cz);
            int lo = Math.Min(Math.Min(Math.Min(a, b), Math.Min(c, d)), e), hi = Math.Max(Math.Max(Math.Max(a, b), Math.Max(c, d)), e);
            return hi - lo;
        }

        public static Biome BiomeAt(WorldGenerator g, int x, int z) => Biome.Get(g.BiomeAtApprox(x, z)) ?? Biome.Plains;

        /// <summary>Dry, reasonably flat land above sea level (surface structures reject water and cliffs).</summary>
        public static bool DryFlat(WorldGenerator g, int x, int z, int radius, int maxRelief, out int h)
        {
            h = Ground(g, x, z);
            var b = BiomeAt(g, x, z);
            if (b.isOcean || b.isRiver) return false;
            if (h <= g.world.seaLevel) return false;
            if (h >= g.world.maxY - 48) return false;
            int lo = h, hi = h;
            for (int i = 0; i < 8; i++)
            {
                int dx = i == 0 || i == 4 || i == 5 ? radius : (i == 1 || i == 6 || i == 7 ? -radius : 0);
                int dz = i == 2 || i == 4 || i == 6 ? radius : (i == 3 || i == 5 || i == 7 ? -radius : 0);
                int hh = Ground(g, x + dx, z + dz);
                if (hh <= g.world.seaLevel - 1) return false;
                if (hh < lo) lo = hh; if (hh > hi) hi = hh;
            }
            return hi - lo <= maxRelief;
        }

        /// <summary>Distance-from-origin guard used by outer-island and ring structures.</summary>
        public static long DistSq(int x, int z) => (long)x * x + (long)z * z;
    }

    // =====================================================================================================
    //  Exact overworld surface (replicates the terrain density interpolation for one column)
    // =====================================================================================================
    public sealed partial class OverworldGenerator
    {
        /// <summary>
        /// Top solid y of the terrain density at a column, before caves and surface decoration.
        /// Uses the same 4x8x4 lattice and trilinear interpolation as GenerateTerrain, so houses and wells can be
        /// fitted to the real ground without reading neighbouring chunks.
        /// </summary>
        public int StructureSurfaceY(int x, int z)
        {
            int gx = x & ~3, gz = z & ~3;
            float tx = (x - gx) / 4f, tz = (z - gz) / 4f;
            var k00 = SampleClimate(gx, gz); var k10 = SampleClimate(gx + 4, gz);
            var k01 = SampleClimate(gx, gz + 4); var k11 = SampleClimate(gx + 4, gz + 4);
            float a00 = Amp(k00), a10 = Amp(k10), a01 = Amp(k01), a11 = Amp(k11);
            int minY = world.minY;
            int NY = world.height / 8 + 1;
            float maxH = Mathf.Max(Mathf.Max(k00.height + a00 * 1.6f, k10.height + a10 * 1.6f), Mathf.Max(k01.height + a01 * 1.6f, k11.height + a11 * 1.6f)) + 9f;
            int kTop = Mathf.Clamp(Mathf.CeilToInt((maxH - minY) / 8f) + 1, 1, NY - 1);
            float Level(int ky)
            {
                float d00 = Dens(gx, gz, k00.height, a00, ky), d10 = Dens(gx + 4, gz, k10.height, a10, ky);
                float d01 = Dens(gx, gz + 4, k01.height, a01, ky), d11 = Dens(gx + 4, gz + 4, k11.height, a11, ky);
                return Mathf.Lerp(Mathf.Lerp(d00, d10, tx), Mathf.Lerp(d01, d11, tx), tz);
            }
            float hi = Level(kTop);
            for (int ky = kTop - 1; ky >= 0; ky--)
            {
                float lo = Level(ky);
                if (lo > 0 || hi > 0)
                    for (int sy = 7; sy >= 0; sy--)
                        if (Mathf.Lerp(lo, hi, sy / 8f) > 0) return minY + ky * 8 + sy;
                hi = lo;
            }
            return minY;
        }

        static float Amp(Climate k) => 3f + k.mountain * 26f + Mathf.Clamp01(k.pv) * k.mountain * 10f;

        float Dens(int wx, int wz, float h, float amp, int ky)
        {
            int y = world.minY + ky * 8;
            float d = h - y;
            if (d > amp * 1.6f + 8 || d < -amp * 1.6f - 8) return d;
            float n = (float)dens3D.Sample(wx, y * 1.3, wz) * 1.7f;
            if (h > 150) n += (float)peakJag.Sample(wx, y, wz) * 0.6f;
            return d + n * amp;
        }

        /// <summary>Deep-dark cave biome test at a 3D position (same rule as BuildCaveBiomes).</summary>
        public bool StructureDeepDark(int x, int y, int z)
        {
            var k = SampleClimate(x, z);
            if (y >= k.height - 18 || y >= -8) return false;
            float a = (float)caveBiomeA.Sample(x, y * 2, z);
            return a > 0.18f && k.e < 0.2f;
        }
    }

    // =====================================================================================================
    //  Shared structure-type helpers
    // =====================================================================================================
    /// <summary>
    /// One rotated building centred on the origin chunk, its floor fitted to the ground (median height of the
    /// footprint). Subclasses supply the biome/site test and the piece; extras may add satellite pieces.
    /// </summary>
    public abstract class BuildingStructure : StructureBase
    {
        protected int bx = 9, bz = 9;       // local footprint: x across the front, z depth
        protected int yDown = 8, yUp = 16;  // vertical extent of the piece box around the floor
        protected bool randomRot = true;

        protected abstract bool Allowed(WorldGenerator g, int x, int z);
        protected abstract FramedPiece NewPiece(WorldGenerator g, ref RNG rng);
        protected virtual int FloorY(WorldGenerator g, int x0, int z0, int x1, int z1) => StructureTerrain.FloorLevel(g, x0, z0, x1, z1);
        protected virtual void AddExtras(StructureStart st, WorldGenerator g, FramedPiece main, ref RNG rng) { }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng) => g is OverworldGenerator && Allowed(g, x, z);

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            int rot = randomRot ? rng.Next(4) : 0;
            int wsx = (rot & 1) == 1 ? bz : bx, wsz = (rot & 1) == 1 ? bx : bz;
            int ox = x - wsx / 2, oz = z - wsz / 2;
            int fy = FloorY(g, ox, oz, ox + wsx - 1, oz + wsz - 1);
            var p = NewPiece(g, ref rng);
            if (p == null) return null;
            p.seed = NextSeed(ref rng);
            p.Place(ox, fy, oz, bx, bz, rot, yDown, yUp, 2);
            var st = NewStart(x, fy + 1, z);
            st.pieces.Add(p);
            AddExtras(st, g, p, ref rng);
            return Finish(st, cx, cz);
        }

        /// <summary>Adds a satellite piece next to the main one (used for cages, tents, side ruins).</summary>
        protected static void Satellite(StructureStart st, WorldGenerator g, FramedPiece p, int cx, int cz, int sx, int sz, int rot, int yDown, int yUp, ref RNG rng)
        {
            int wsx = (rot & 1) == 1 ? sz : sx, wsz = (rot & 1) == 1 ? sx : sz;
            int ox = cx - wsx / 2, oz = cz - wsz / 2;
            int fy = StructureTerrain.FloorLevel(g, ox, oz, ox + wsx - 1, oz + wsz - 1);
            p.seed = NextSeed(ref rng);
            p.Place(ox, fy, oz, sx, sz, rot, yDown, yUp, 2);
            st.pieces.Add(p);
        }
    }

    public abstract class StructureBase : StructureType
    {
        /// <summary>Makes a new start at a block position with pieces added by the caller.</summary>
        protected static StructureStart NewStart(int x, int y, int z) => new StructureStart { x = x, y = y, z = z };

        protected static int NextSeed(ref RNG rng) => (int)(rng.NextULong() & 0x7FFFFFFF);

        /// <summary>Computes the bounds and drops any piece that would reach beyond maxRadiusChunks of the origin chunk.</summary>
        protected StructureStart Finish(StructureStart st, int cx, int cz)
        {
            if (st == null || st.pieces.Count == 0) return null;
            int r = maxRadiusChunks;
            int minX = (cx - r) << 4, maxX = ((cx + r) << 4) + 16, minZ = (cz - r) << 4, maxZ = ((cz + r) << 4) + 16;
            st.pieces.RemoveAll(p => p.box.x0 < minX || p.box.x1 > maxX || p.box.z0 < minZ || p.box.z1 > maxZ);
            if (st.pieces.Count == 0) return null;
            var bb = st.pieces[0].box;
            foreach (var q in st.pieces)
            {
                if (q.box.x0 < bb.x0) bb.x0 = q.box.x0; if (q.box.y0 < bb.y0) bb.y0 = q.box.y0; if (q.box.z0 < bb.z0) bb.z0 = q.box.z0;
                if (q.box.x1 > bb.x1) bb.x1 = q.box.x1; if (q.box.y1 > bb.y1) bb.y1 = q.box.y1; if (q.box.z1 > bb.z1) bb.z1 = q.box.z1;
            }
            st.bounds = bb;
            return st;
        }

        /// <summary>True if box b overlaps any piece box already in the list (optionally shrunk by 'shrink').</summary>
        protected static bool Collides(List<StructurePiece> pieces, BBox b, int shrink = 0)
        {
            var t = new BBox(b.x0 + shrink, b.y0 + shrink, b.z0 + shrink, b.x1 - shrink, b.y1 - shrink, b.z1 - shrink);
            foreach (var p in pieces) if (p.box.Intersects(t)) return true;
            return false;
        }
    }
}
