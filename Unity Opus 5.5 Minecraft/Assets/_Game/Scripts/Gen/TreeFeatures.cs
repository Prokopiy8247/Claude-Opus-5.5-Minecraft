using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Tree / large vegetation features. When 'gen' is true the feature is placed unconditionally (decisions are made
    /// beforehand from immutable data) so every chunk reproduces exactly its part of a tree spanning chunk borders.
    /// </summary>
    public static class TreeFeatures
    {
        static ushort S(string id) => Blocks.StateOf(id);
        static ushort Log(string w, int axis = 0) { var b = Blocks.Get(w); return b == null ? (ushort)0 : b.State(axis); }
        static ushort Leaf(string w) { var b = Blocks.Get(w); return b == null ? (ushort)0 : b.State(1); } // non-persistent, distance 1

        static bool Replaceable(ushort s)
        {
            if (s == 0) return true;
            var b = Blocks.ByState[s];
            return b.replaceable || b is LeavesBlock || b is PlantBlock || b.id == "snow" || b is SaplingBlock || b is TallPlantBlock || b is VineBlock;
        }
        static void SetLog(IBlockAccess w, int x, int y, int z, ushort s)
        {
            if (!w.CanWrite(x, y, z)) return;
            ushort cur = w.Get(x, y, z);
            var b = Blocks.ByState[cur];
            if (cur == 0 || Replaceable(cur) || b.id == "grass_block" || b.id == "dirt" || b.id == "water" || b.id == "mud") w.Set(x, y, z, s);
        }
        static void SetLeaf(IBlockAccess w, int x, int y, int z, ushort s)
        {
            if (!w.CanWrite(x, y, z)) return;
            ushort cur = w.Get(x, y, z);
            if (cur == 0 || (Blocks.ByState[cur].replaceable && !Blocks.ByState[cur].isLiquid) || Blocks.ByState[cur].id == "snow") w.Set(x, y, z, s);
        }
        static void SetDirt(IBlockAccess w, int x, int y, int z)
        {
            if (!w.CanWrite(x, y, z)) return;
            var b = Blocks.ByState[w.Get(x, y, z)];
            if (b.id == "grass_block" || b.id == "farmland" || b.id == "mycelium") w.Set(x, y, z, S("dirt"));
        }

        public static void GrowSapling(World world, Int3 pos, string type, ref RNG rng)
        {
            var w = new WorldAccess(world);
            // 2x2 saplings for mega variants
            string t = type;
            if (type == "oak" && rng.Chance(0.1f)) t = "fancy_oak";
            if (type == "spruce" || type == "jungle" || type == "dark_oak")
            {
                var sap = world.GetBlock(pos);
                for (int ox = -1; ox <= 0; ox++)
                    for (int oz = -1; oz <= 0; oz++)
                    {
                        bool all = true;
                        for (int dx = 0; dx < 2 && all; dx++) for (int dz = 0; dz < 2 && all; dz++) if (world.GetBlock(new Int3(pos.x + ox + dx, pos.y, pos.z + oz + dz)) != sap) all = false;
                        if (all)
                        {
                            for (int dx = 0; dx < 2; dx++) for (int dz = 0; dz < 2; dz++) world.SetState(new Int3(pos.x + ox + dx, pos.y, pos.z + oz + dz), 0, 0);
                            Place(w, type == "spruce" ? "mega_spruce" : (type == "jungle" ? "mega_jungle" : "dark_oak"), pos.x + ox, pos.y, pos.z + oz, ref rng, false);
                            return;
                        }
                    }
                if (type == "dark_oak") return; // requires 2x2
            }
            if (type == "crimson" || type == "warped") { world.SetState(pos, 0, 0); Place(w, type == "crimson" ? "crimson_fungus_tree" : "warped_fungus_tree", pos.x, pos.y, pos.z, ref rng, false); return; }
            // space check
            int h = 7;
            for (int y = 1; y < h; y++) if (!Replaceable(world.GetState(pos.x, pos.y + y, pos.z))) return;
            world.SetState(pos, 0, 0);
            Place(w, t, pos.x, pos.y, pos.z, ref rng, false);
        }

        public static bool GrowHugeMushroom(World world, Int3 pos, bool red, ref RNG rng)
        {
            var w = new WorldAccess(world);
            world.SetState(pos, 0, 0);
            Place(w, red ? "huge_red_mushroom" : "huge_brown_mushroom", pos.x, pos.y, pos.z, ref rng, false);
            return true;
        }

        public static void Place(IBlockAccess w, string type, int x, int y, int z, ref RNG rng, bool gen)
        {
            switch (type)
            {
                case "oak": Blob(w, x, y, z, ref rng, Log("oak_log"), Leaf("oak_leaves"), rng.Range(4, 6), 2); break;
                case "swamp_oak": Blob(w, x, y, z, ref rng, Log("oak_log"), Leaf("oak_leaves"), rng.Range(5, 7), 3); Vines(w, x, y, z, 3, 7, ref rng); break;
                case "birch": Blob(w, x, y, z, ref rng, Log("birch_log"), Leaf("birch_leaves"), rng.Range(5, 7), 2); break;
                case "tall_birch": Blob(w, x, y, z, ref rng, Log("birch_log"), Leaf("birch_leaves"), rng.Range(9, 13), 2); break;
                case "fancy_oak": FancyOak(w, x, y, z, ref rng); break;
                case "spruce": Spruce(w, x, y, z, ref rng, false); break;
                case "pine": Spruce(w, x, y, z, ref rng, true); break;
                case "mega_spruce": MegaSpruce(w, x, y, z, ref rng); break;
                case "jungle": Blob(w, x, y, z, ref rng, Log("jungle_log"), Leaf("jungle_leaves"), rng.Range(6, 10), 2); Vines(w, x, y, z, 2, 8, ref rng); Cocoa(w, x, y, z, ref rng); break;
                case "mega_jungle": MegaJungle(w, x, y, z, ref rng); break;
                case "jungle_bush": Bush(w, x, y, z, Log("jungle_log"), Leaf("oak_leaves"), ref rng); break;
                case "acacia": Acacia(w, x, y, z, ref rng); break;
                case "dark_oak": DarkOak(w, x, y, z, ref rng, "dark_oak_log", "dark_oak_leaves"); break;
                case "pale_oak": DarkOak(w, x, y, z, ref rng, "pale_oak_log", "pale_oak_leaves"); PaleMoss(w, x, y, z, ref rng); break;
                case "creaking_heart_tree": DarkOak(w, x, y, z, ref rng, "pale_oak_log", "pale_oak_leaves"); PaleMoss(w, x, y, z, ref rng); if (w.CanWrite(x, y + 2, z)) w.Set(x, y + 2, z, S("creaking_heart")); break;
                case "mangrove": Mangrove(w, x, y, z, ref rng); break;
                case "cherry": Cherry(w, x, y, z, ref rng); break;
                case "huge_red_mushroom": HugeMushroom(w, x, y, z, ref rng, true); break;
                case "huge_brown_mushroom": HugeMushroom(w, x, y, z, ref rng, false); break;
                case "crimson_fungus_tree": Fungus(w, x, y, z, ref rng, true); break;
                case "warped_fungus_tree": Fungus(w, x, y, z, ref rng, false); break;
                case "ice_spike": IceSpike(w, x, y, z, ref rng); break;
                case "iceberg": Iceberg(w, x, y, z, ref rng); break;
                case "boulder": Boulder(w, x, y, z, ref rng); break;
                case "fallen_oak": Fallen(w, x, y, z, ref rng, "oak_log"); break;
                case "fallen_birch": Fallen(w, x, y, z, ref rng, "birch_log"); break;
                case "fallen_spruce": Fallen(w, x, y, z, ref rng, "spruce_log"); break;
                case "azalea": Blob(w, x, y, z, ref rng, Log("oak_log"), Leaf(rng.NextBool() ? "azalea_leaves" : "flowering_azalea_leaves"), rng.Range(4, 5), 2); break;
                default: Blob(w, x, y, z, ref rng, Log("oak_log"), Leaf("oak_leaves"), 5, 2); break;
            }
        }

        static void Blob(IBlockAccess w, int x, int y, int z, ref RNG rng, ushort log, ushort leaf, int height, int radius)
        {
            SetDirt(w, x, y - 1, z);
            int top = y + height;
            for (int ly = top - 3; ly <= top; ly++)
            {
                int dy = ly - top;
                int r = dy >= -1 ? 1 : radius;
                for (int dx = -r; dx <= r; dx++)
                    for (int dz = -r; dz <= r; dz++)
                    {
                        if (Mathf.Abs(dx) == r && Mathf.Abs(dz) == r && (dy == 0 || Hash.Get(x + dx, ly, z + dz) % 2 == 0)) continue;
                        SetLeaf(w, x + dx, ly, z + dz, leaf);
                    }
            }
            for (int i = 0; i < height; i++) SetLog(w, x, y + i, z, log);
        }

        static void FancyOak(IBlockAccess w, int x, int y, int z, ref RNG rng)
        {
            ushort log = Log("oak_log"), leaf = Leaf("oak_leaves");
            SetDirt(w, x, y - 1, z);
            int h = rng.Range(9, 13);
            for (int i = 0; i < h - 2; i++) SetLog(w, x, y + i, z, log);
            int branches = rng.Range(3, 5);
            for (int b = 0; b < branches; b++)
            {
                float ang = rng.NextFloat() * Mathf.PI * 2;
                int by = y + h - 3 - rng.Next(h / 2);
                int len = rng.Range(2, 4);
                int ex = x + Mathf.RoundToInt(Mathf.Cos(ang) * len), ez = z + Mathf.RoundToInt(Mathf.Sin(ang) * len);
                int ey = by + rng.Range(1, 3);
                for (int s = 0; s <= len; s++)
                {
                    float t = (float)s / len;
                    int bx = Mathf.RoundToInt(Mathf.Lerp(x, ex, t)), bz = Mathf.RoundToInt(Mathf.Lerp(z, ez, t)), byy = Mathf.RoundToInt(Mathf.Lerp(by, ey, t));
                    int axis = Mathf.Abs(ex - x) > Mathf.Abs(ez - z) ? 1 : 2;
                    SetLog(w, bx, byy, bz, Log("oak_log", axis));
                }
                LeafBall(w, ex, ey, ez, 2.6f, leaf);
            }
            LeafBall(w, x, y + h - 2, z, 2.8f, leaf);
        }

        static void LeafBall(IBlockAccess w, int cx, int cy, int cz, float r, ushort leaf)
        {
            int ir = Mathf.CeilToInt(r);
            for (int dx = -ir; dx <= ir; dx++)
                for (int dy = -ir + 1; dy <= ir - 1; dy++)
                    for (int dz = -ir; dz <= ir; dz++)
                    {
                        float d = dx * dx + dy * dy * 2.2f + dz * dz;
                        if (d <= r * r + (Hash.Get(cx + dx, cy + dy, cz + dz) % 3) * 0.3f) SetLeaf(w, cx + dx, cy + dy, cz + dz, leaf);
                    }
        }

        static void Spruce(IBlockAccess w, int x, int y, int z, ref RNG rng, bool pine)
        {
            ushort log = Log("spruce_log"), leaf = Leaf("spruce_leaves");
            SetDirt(w, x, y - 1, z);
            int h = pine ? rng.Range(7, 10) : rng.Range(6, 9);
            int top = y + h;
            if (pine)
            {
                int leafStart = y + h - rng.Range(3, 4);
                for (int ly = leafStart; ly <= top; ly++)
                {
                    int r = ly >= top - 1 ? 0 : 1;
                    for (int dx = -r; dx <= r; dx++) for (int dz = -r; dz <= r; dz++) if (!(Mathf.Abs(dx) == 1 && Mathf.Abs(dz) == 1)) SetLeaf(w, x + dx, ly, z + dz, leaf);
                }
                SetLeaf(w, x, top + 1, z, leaf);
            }
            else
            {
                int r = 0, maxR = rng.Range(2, 3);
                for (int ly = top; ly >= y + 2; ly--)
                {
                    for (int dx = -r; dx <= r; dx++)
                        for (int dz = -r; dz <= r; dz++)
                            if (!(Mathf.Abs(dx) == r && Mathf.Abs(dz) == r && r > 0)) SetLeaf(w, x + dx, ly, z + dz, leaf);
                    r = r >= maxR ? 1 : r + 1;
                    if (ly == top) r = 1;
                }
                SetLeaf(w, x, top + 1, z, leaf);
            }
            for (int i = 0; i < h; i++) SetLog(w, x, y + i, z, log);
        }

        static void MegaSpruce(IBlockAccess w, int x, int y, int z, ref RNG rng)
        {
            ushort log = Log("spruce_log"), leaf = Leaf("spruce_leaves");
            int h = rng.Range(18, 28);
            for (int dx = 0; dx < 2; dx++) for (int dz = 0; dz < 2; dz++) { SetDirt(w, x + dx, y - 1, z + dz); if (w.CanWrite(x + dx, y - 1, z + dz)) w.Set(x + dx, y - 1, z + dz, S("podzol")); }
            int top = y + h;
            int r = 0;
            for (int ly = top; ly >= top - h / 2; ly--)
            {
                for (int dx = -r; dx <= r + 1; dx++)
                    for (int dz = -r; dz <= r + 1; dz++)
                    {
                        float ddx = dx - 0.5f, ddz = dz - 0.5f;
                        if (ddx * ddx + ddz * ddz <= (r + 0.7f) * (r + 0.7f)) SetLeaf(w, x + dx, ly, z + dz, leaf);
                    }
                if ((top - ly) % 3 == 2 && r < 4) r++;
            }
            for (int i = 0; i < h; i++) for (int dx = 0; dx < 2; dx++) for (int dz = 0; dz < 2; dz++) SetLog(w, x + dx, y + i, z + dz, log);
        }

        static void MegaJungle(IBlockAccess w, int x, int y, int z, ref RNG rng)
        {
            ushort log = Log("jungle_log"), leaf = Leaf("jungle_leaves");
            int h = rng.Range(16, 28);
            for (int dx = 0; dx < 2; dx++) for (int dz = 0; dz < 2; dz++) SetDirt(w, x + dx, y - 1, z + dz);
            for (int b = 0; b < 3; b++)
            {
                int by = y + h - 4 - b * 4;
                float ang = rng.NextFloat() * Mathf.PI * 2;
                int ex = x + Mathf.RoundToInt(Mathf.Cos(ang) * 4), ez = z + Mathf.RoundToInt(Mathf.Sin(ang) * 4);
                for (int s = 1; s <= 4; s++) SetLog(w, x + Mathf.RoundToInt(Mathf.Cos(ang) * s), by + s / 2, z + Mathf.RoundToInt(Mathf.Sin(ang) * s), log);
                LeafDisc(w, ex, by + 2, ez, 2, leaf); LeafDisc(w, ex, by + 3, ez, 1, leaf);
            }
            LeafDisc(w, x, y + h - 1, z, 4, leaf); LeafDisc(w, x, y + h, z, 3, leaf); LeafDisc(w, x, y + h + 1, z, 2, leaf);
            for (int i = 0; i < h; i++) for (int dx = 0; dx < 2; dx++) for (int dz = 0; dz < 2; dz++) SetLog(w, x + dx, y + i, z + dz, log);
            Vines(w, x, y, z, 3, h, ref rng);
        }

        static void LeafDisc(IBlockAccess w, int cx, int cy, int cz, int r, ushort leaf)
        {
            for (int dx = -r; dx <= r + 1; dx++)
                for (int dz = -r; dz <= r + 1; dz++)
                {
                    float ddx = dx - 0.5f, ddz = dz - 0.5f;
                    if (ddx * ddx + ddz * ddz <= (r + 0.5f) * (r + 0.5f)) SetLeaf(w, cx + dx, cy, cz + dz, leaf);
                }
        }

        static void Bush(IBlockAccess w, int x, int y, int z, ushort log, ushort leaf, ref RNG rng)
        {
            SetLog(w, x, y, z, log);
            for (int dy = 0; dy <= 2; dy++)
            {
                int r = 2 - dy;
                for (int dx = -r; dx <= r; dx++) for (int dz = -r; dz <= r; dz++) if (Mathf.Abs(dx) + Mathf.Abs(dz) <= r + 1) SetLeaf(w, x + dx, y + dy, z + dz, leaf);
            }
        }

        static void Acacia(IBlockAccess w, int x, int y, int z, ref RNG rng)
        {
            ushort log = Log("acacia_log"), leaf = Leaf("acacia_leaves");
            SetDirt(w, x, y - 1, z);
            int h = rng.Range(5, 7);
            int bend = rng.Range(2, 3);
            int dir = rng.Next(4);
            int ddx = dir == 0 ? 1 : dir == 1 ? -1 : 0, ddz = dir == 2 ? 1 : dir == 3 ? -1 : 0;
            int cx = x, cz = z;
            for (int i = 0; i < h; i++)
            {
                if (i >= h - bend) { cx += ddx; cz += ddz; }
                SetLog(w, cx, y + i, cz, log);
            }
            int top = y + h;
            for (int dx = -3; dx <= 3; dx++) for (int dz = -3; dz <= 3; dz++) if (Mathf.Abs(dx) + Mathf.Abs(dz) <= 4) SetLeaf(w, cx + dx, top - 1, cz + dz, leaf);
            for (int dx = -1; dx <= 1; dx++) for (int dz = -1; dz <= 1; dz++) SetLeaf(w, cx + dx, top, cz + dz, leaf);
            // second small canopy
            if (rng.Chance(0.6f))
            {
                int bx = x - ddx * 2, bz = z - ddz * 2;
                for (int i = 2; i < 4; i++) SetLog(w, x - ddx * (i - 1), y + h - 3 + i - 2, z - ddz * (i - 1), log);
                for (int dx = -2; dx <= 2; dx++) for (int dz = -2; dz <= 2; dz++) if (Mathf.Abs(dx) + Mathf.Abs(dz) <= 3) SetLeaf(w, bx + dx, y + h - 1, bz + dz, leaf);
            }
        }

        static void DarkOak(IBlockAccess w, int x, int y, int z, ref RNG rng, string logId, string leafId)
        {
            ushort log = Log(logId), leaf = Leaf(leafId);
            int h = rng.Range(6, 9);
            for (int dx = 0; dx < 2; dx++) for (int dz = 0; dz < 2; dz++) SetDirt(w, x + dx, y - 1, z + dz);
            int top = y + h;
            for (int ly = top - 2; ly <= top + 1; ly++)
            {
                int r = ly <= top - 1 ? 3 : 2;
                if (ly == top + 1) r = 1;
                for (int dx = -r; dx <= r + 1; dx++)
                    for (int dz = -r; dz <= r + 1; dz++)
                    {
                        float ddx = dx - 0.5f, ddz = dz - 0.5f;
                        if (ddx * ddx + ddz * ddz <= (r + 0.6f) * (r + 0.6f) - (Hash.Get(x + dx, ly, z + dz) % 2)) SetLeaf(w, x + dx, ly, z + dz, leaf);
                    }
            }
            for (int i = 0; i < h; i++) for (int dx = 0; dx < 2; dx++) for (int dz = 0; dz < 2; dz++) SetLog(w, x + dx, y + i, z + dz, log);
            // root knobs
            for (int k = 0; k < 3; k++) { int dx = rng.Range(-1, 2), dz = rng.Range(-1, 2); if ((dx < 0 || dx > 1) || (dz < 0 || dz > 1)) SetLog(w, x + dx, y, z + dz, log); }
        }

        static void PaleMoss(IBlockAccess w, int x, int y, int z, ref RNG rng)
        {
            ushort moss = S("pale_moss_block"), carpet = S("pale_moss_carpet"), hanging = S("pale_hanging_moss");
            for (int k = 0; k < 10; k++)
            {
                int dx = rng.Range(-4, 5), dz = rng.Range(-4, 5);
                int gx = x + dx, gz = z + dz;
                if (!w.CanWrite(gx, y - 1, gz)) continue;
                var b = Blocks.ByState[w.Get(gx, y - 1, gz)];
                if (b.id == "grass_block" || b.id == "dirt") { w.Set(gx, y - 1, gz, moss); if (w.Get(gx, y, gz) == 0 && (Hash.Get(gx, y, gz) & 1) == 0) w.Set(gx, y, gz, carpet); }
            }
            for (int k = 0; k < 6; k++)
            {
                int dx = rng.Range(-3, 4), dz = rng.Range(-3, 4);
                for (int ly = y + 10; ly > y + 3; ly--)
                    if (w.CanWrite(x + dx, ly, z + dz) && Blocks.ByState[w.Get(x + dx, ly, z + dz)] is LeavesBlock && w.Get(x + dx, ly - 1, z + dz) == 0)
                    { if (hanging != 0) w.Set(x + dx, ly - 1, z + dz, hanging); break; }
            }
        }

        static void Mangrove(IBlockAccess w, int x, int y, int z, ref RNG rng)
        {
            ushort log = Log("mangrove_log"), leaf = Leaf("mangrove_leaves"), roots = Blocks.StateOf("mangrove_roots");
            int h = rng.Range(6, 10);
            int rootH = 3;
            for (int i = 0; i < h; i++) SetLog(w, x, y + rootH + i, z, log);
            // prop roots
            for (int k = 0; k < 4; k++)
            {
                int dx = k == 0 ? 1 : k == 1 ? -1 : 0, dz = k == 2 ? 1 : k == 3 ? -1 : 0;
                for (int j = 0; j <= rootH; j++)
                {
                    int rx = x + dx * (j <= 1 ? 2 : 1), rz = z + dz * (j <= 1 ? 2 : 1);
                    SetLog(w, rx, y + j - 1, rz, roots);
                }
                SetLog(w, x + dx, y + rootH, z + dz, roots);
            }
            SetLog(w, x, y + rootH - 1, z, log);
            int top = y + rootH + h;
            LeafBall(w, x, top - 1, z, 3.2f, leaf);
            Vines(w, x, top - 6, z, 3, 6, ref rng);
            // propagules hanging
            ushort prop = S("mangrove_propagule");
            for (int k = 0; k < 3; k++) { int dx = rng.Range(-2, 3), dz = rng.Range(-2, 3); int yy = top - 4; if (w.CanWrite(x + dx, yy, z + dz) && w.Get(x + dx, yy, z + dz) == 0 && Blocks.ByState[w.Get(x + dx, yy + 1, z + dz)] is LeavesBlock) w.Set(x + dx, yy, z + dz, prop); }
        }

        static void Cherry(IBlockAccess w, int x, int y, int z, ref RNG rng)
        {
            ushort log = Log("cherry_log"), leaf = Leaf("cherry_leaves");
            SetDirt(w, x, y - 1, z);
            int h = rng.Range(4, 6);
            for (int i = 0; i < h; i++) SetLog(w, x, y + i, z, log);
            int branches = rng.Range(2, 3);
            for (int b = 0; b < branches; b++)
            {
                float ang = (b / (float)branches) * Mathf.PI * 2 + rng.NextFloat();
                int len = rng.Range(3, 5);
                int ex = x, ez = z, ey = y + h - 1;
                for (int s = 1; s <= len; s++)
                {
                    ex = x + Mathf.RoundToInt(Mathf.Cos(ang) * s); ez = z + Mathf.RoundToInt(Mathf.Sin(ang) * s);
                    ey = y + h - 1 + (s + 1) / 2;
                    SetLog(w, ex, ey, ez, Log("cherry_log", Mathf.Abs(Mathf.Cos(ang)) > 0.7f ? 1 : 2));
                }
                // wide flat canopy
                for (int dy = -1; dy <= 2; dy++)
                {
                    int r = dy == 2 ? 2 : (dy == -1 ? 3 : 4);
                    for (int dx = -r; dx <= r; dx++) for (int dz = -r; dz <= r; dz++)
                        {
                            float d = dx * dx + dz * dz;
                            if (d <= r * r - (Hash.Get(ex + dx, ey + dy, ez + dz) % 3)) SetLeaf(w, ex + dx, ey + dy, ez + dz, leaf);
                        }
                }
            }
        }

        static void Vines(IBlockAccess w, int x, int y, int z, int radius, int height, ref RNG rng)
        {
            var vine = Blocks.Get("vine");
            if (vine == null) return;
            for (int k = 0; k < 12; k++)
            {
                // consume all randomness up-front so every chunk sees the same values
                int dx = rng.Range(-radius, radius), dz = rng.Range(-radius, radius);
                int ly = y + rng.Range(2, height);
                int len = rng.Range(1, 5);
                for (int d = 0; d < 4; d++)
                {
                    Dir dir = DirUtil.Horizontal[d];
                    Int3 o = DirUtil.Offset[(int)dir];
                    int px = x + dx + o.x, pz = z + dz + o.z;
                    if (!w.CanWrite(px, ly, pz) || w.Get(px, ly, pz) != 0) continue;
                    var nb = Blocks.ByState[w.Get(x + dx, ly, z + dz)];
                    if (!(nb is LeavesBlock || nb is PillarBlock)) continue;
                    int bit = DirUtil.HorizIndex(DirUtil.Opposite(dir));
                    for (int j = 0; j < len && w.CanWrite(px, ly - j, pz) && w.Get(px, ly - j, pz) == 0; j++) w.Set(px, ly - j, pz, vine.State(1 << bit));
                    break;
                }
            }
        }

        static void Cocoa(IBlockAccess w, int x, int y, int z, ref RNG rng)
        {
            var cocoa = Blocks.Get("cocoa");
            if (cocoa == null || !rng.Chance(0.2f)) return;
            int d = rng.Next(4); Dir dir = DirUtil.Horizontal[d]; Int3 o = DirUtil.Offset[(int)dir];
            int ly = y + rng.Range(2, 4);
            if (w.CanWrite(x + o.x, ly, z + o.z) && w.Get(x + o.x, ly, z + o.z) == 0) w.Set(x + o.x, ly, z + o.z, cocoa.State(DirUtil.HorizIndex(DirUtil.Opposite(dir)) | (2 << 2)));
        }

        static void HugeMushroom(IBlockAccess w, int x, int y, int z, ref RNG rng, bool red)
        {
            ushort stem = S("mushroom_stem"), cap = S(red ? "red_mushroom_block" : "brown_mushroom_block");
            int h = rng.Range(4, 7);
            for (int i = 0; i < h; i++) SetLog(w, x, y + i, z, stem);
            if (red)
            {
                for (int ly = y + h - 3; ly <= y + h; ly++)
                {
                    int r = ly == y + h ? 1 : 2;
                    for (int dx = -r; dx <= r; dx++) for (int dz = -r; dz <= r; dz++)
                        {
                            bool edge = Mathf.Abs(dx) == r || Mathf.Abs(dz) == r || ly == y + h;
                            if (!edge) continue;
                            if (Mathf.Abs(dx) == r && Mathf.Abs(dz) == r && ly != y + h) continue;
                            SetLeaf(w, x + dx, ly, z + dz, cap);
                        }
                }
            }
            else
            {
                int r = 3;
                for (int dx = -r; dx <= r; dx++) for (int dz = -r; dz <= r; dz++) if (!(Mathf.Abs(dx) == r && Mathf.Abs(dz) == r)) SetLeaf(w, x + dx, y + h, z + dz, cap);
            }
        }

        static void Fungus(IBlockAccess w, int x, int y, int z, ref RNG rng, bool crimson)
        {
            ushort stem = Log(crimson ? "crimson_stem" : "warped_stem"), wart = S(crimson ? "nether_wart_block" : "warped_wart_block"), light = S("shroomlight");
            int h = rng.Range(4, 13);
            if (rng.Chance(0.08f)) h *= 2;
            for (int i = 0; i < h; i++) SetLog(w, x, y + i, z, stem);
            int top = y + h;
            int r = h > 10 ? 3 : 2;
            for (int ly = top - (h > 8 ? 4 : 3); ly <= top; ly++)
            {
                int rr = ly == top ? r - 1 : r;
                for (int dx = -rr; dx <= rr; dx++) for (int dz = -rr; dz <= rr; dz++)
                    {
                        bool edge = Mathf.Abs(dx) == rr || Mathf.Abs(dz) == rr || ly == top;
                        if (!edge && ly != top) continue;
                        if (Mathf.Abs(dx) == rr && Mathf.Abs(dz) == rr && rng.Chance(0.5f)) continue;
                        SetLeaf(w, x + dx, ly, z + dz, rng.Chance(0.06f) ? light : wart);
                    }
            }
            if (crimson)
            {
                var vines = S("weeping_vines");
                for (int k = 0; k < 4; k++) { int dx = rng.Range(-r, r), dz = rng.Range(-r, r); int len = rng.Range(1, 4); int yy = top - 4; for (int j = 0; j < len && w.CanWrite(x + dx, yy - j, z + dz) && w.Get(x + dx, yy - j, z + dz) == 0; j++) w.Set(x + dx, yy - j, z + dz, vines); }
            }
        }

        static void IceSpike(IBlockAccess w, int x, int y, int z, ref RNG rng)
        {
            ushort pi = S("packed_ice");
            bool big = rng.Chance(0.1f);
            int h = big ? rng.Range(30, 50) : rng.Range(7, 16);
            int r = big ? 3 : 1;
            for (int i = 0; i < h; i++)
            {
                float f = 1f - (float)i / h;
                int rr = Mathf.RoundToInt(r * f + 0.3f);
                for (int dx = -rr; dx <= rr; dx++) for (int dz = -rr; dz <= rr; dz++) if (dx * dx + dz * dz <= rr * rr + 1) { if (w.CanWrite(x + dx, y + i, z + dz)) w.Set(x + dx, y + i, z + dz, pi); }
            }
        }

        static void Iceberg(IBlockAccess w, int x, int y, int z, ref RNG rng)
        {
            ushort pi = S("packed_ice"), snow = S("snow_block"), blue = S("blue_ice");
            int r = rng.Range(4, 9), up = rng.Range(4, 14), down = rng.Range(6, 14);
            for (int dy = -down; dy <= up; dy++)
            {
                float f = dy >= 0 ? 1f - (float)dy / up : 1f - (float)(-dy) / down * 0.7f;
                float rr = r * f;
                for (int dx = -r; dx <= r; dx++) for (int dz = -r; dz <= r; dz++)
                    {
                        if (dx * dx + dz * dz > rr * rr) continue;
                        int px = x + dx, py = y + dy, pz = z + dz;
                        if (!w.CanWrite(px, py, pz)) continue;
                        w.Set(px, py, pz, dy > up - 3 ? snow : (Hash.Get(px, py, pz) % 23 == 0 ? blue : pi));
                    }
            }
        }

        static void Boulder(IBlockAccess w, int x, int y, int z, ref RNG rng)
        {
            ushort moss = S("mossy_cobblestone");
            for (int k = 0; k < 3; k++)
            {
                int cx = x + rng.Range(-1, 1), cy = y + rng.Range(0, 1), cz = z + rng.Range(-1, 1);
                float r = rng.Range(1.2f, 2.2f);
                int ir = Mathf.CeilToInt(r);
                for (int dx = -ir; dx <= ir; dx++) for (int dy = -ir; dy <= ir; dy++) for (int dz = -ir; dz <= ir; dz++)
                            if (dx * dx + dy * dy + dz * dz <= r * r && w.CanWrite(cx + dx, cy + dy, cz + dz)) w.Set(cx + dx, cy + dy, cz + dz, moss);
            }
        }

        static void Fallen(IBlockAccess w, int x, int y, int z, ref RNG rng, string logId)
        {
            int len = rng.Range(4, 7);
            bool alongX = rng.NextBool();
            ushort log = Log(logId, alongX ? 1 : 2);
            SetLog(w, x, y, z, Log(logId, 0));
            for (int i = 2; i < len + 2; i++)
            {
                int px = alongX ? x + i : x, pz = alongX ? z : z + i;
                uint hh = Hash.Get(px, y, pz);
                if (w.CanWrite(px, y, pz) && w.Get(px, y, pz) == 0 && w.Get(px, y - 1, pz) != 0) { w.Set(px, y, pz, log); if (hh % 10 < 3 && w.Get(px, y + 1, pz) == 0) w.Set(px, y + 1, pz, S((hh & 1) == 0 ? "brown_mushroom" : "moss_carpet")); }
            }
        }
    }
}
