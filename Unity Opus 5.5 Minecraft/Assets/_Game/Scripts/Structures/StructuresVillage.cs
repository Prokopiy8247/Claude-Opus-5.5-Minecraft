using System;
using System.Collections.Generic;

namespace MCR
{
    /// <summary>Material palette of a village style (picked from the biome of the village centre).</summary>
    public sealed class VillageStyle
    {
        public string name, floor, wall, baseCourse, corner, roofStairs, roofBlock, roofSlab, door, window, fence, light, foundation, path, bridge, accent;
        public string[] beds, crops;
        public bool flatRoof, snowy;

        public static readonly VillageStyle Plains = new VillageStyle
        {
            name = "plains", floor = "oak_planks", wall = "oak_planks", baseCourse = "cobblestone", corner = "oak_log", roofStairs = "oak_stairs",
            roofBlock = "oak_planks", roofSlab = "oak_slab", door = "oak_door", window = "glass_pane", fence = "oak_fence", light = "torch",
            foundation = "cobblestone", path = "dirt_path", bridge = "oak_planks", accent = "stripped_oak_log",
            beds = new[] { "red", "white", "yellow", "light_blue", "lime" }, crops = new[] { "wheat", "wheat", "carrots", "potatoes", "beetroots" }
        };
        public static readonly VillageStyle Desert = new VillageStyle
        {
            name = "desert", floor = "smooth_sandstone", wall = "smooth_sandstone", baseCourse = "cut_sandstone", corner = "cut_sandstone", roofStairs = "sandstone_stairs",
            roofBlock = "smooth_sandstone", roofSlab = "smooth_sandstone_slab", door = "jungle_door", window = "air", fence = "sandstone_wall", light = "torch",
            foundation = "sandstone", path = "smooth_sandstone", bridge = "smooth_sandstone_slab", accent = "orange_terracotta", flatRoof = true,
            beds = new[] { "yellow", "orange", "red", "green" }, crops = new[] { "wheat", "beetroots", "wheat" }
        };
        public static readonly VillageStyle Savanna = new VillageStyle
        {
            name = "savanna", floor = "acacia_planks", wall = "acacia_planks", baseCourse = "orange_terracotta", corner = "acacia_log", roofStairs = "acacia_stairs",
            roofBlock = "acacia_planks", roofSlab = "acacia_slab", door = "acacia_door", window = "glass_pane", fence = "acacia_fence", light = "torch",
            foundation = "terracotta", path = "dirt_path", bridge = "acacia_planks", accent = "yellow_terracotta",
            beds = new[] { "orange", "red", "yellow", "white" }, crops = new[] { "wheat", "wheat", "beetroots", "carrots" }
        };
        public static readonly VillageStyle Taiga = new VillageStyle
        {
            name = "taiga", floor = "spruce_planks", wall = "spruce_planks", baseCourse = "mossy_cobblestone", corner = "spruce_log", roofStairs = "spruce_stairs",
            roofBlock = "spruce_planks", roofSlab = "spruce_slab", door = "spruce_door", window = "glass_pane", fence = "spruce_fence", light = "lantern",
            foundation = "cobblestone", path = "dirt_path", bridge = "spruce_planks", accent = "stripped_spruce_log",
            beds = new[] { "green", "brown", "red", "white" }, crops = new[] { "potatoes", "carrots", "wheat" }
        };
        public static readonly VillageStyle Snowy = new VillageStyle
        {
            name = "snowy", floor = "spruce_planks", wall = "snow_block", baseCourse = "stone_bricks", corner = "stripped_spruce_log", roofStairs = "spruce_stairs",
            roofBlock = "spruce_planks", roofSlab = "spruce_slab", door = "spruce_door", window = "glass_pane", fence = "spruce_fence", light = "lantern",
            foundation = "cobblestone", path = "dirt_path", bridge = "spruce_planks", accent = "packed_ice", snowy = true,
            beds = new[] { "blue", "light_blue", "cyan", "white" }, crops = new[] { "potatoes", "beetroots", "wheat" }
        };

        public static VillageStyle For(string biome)
        {
            switch (biome)
            {
                case "plains": case "sunflower_plains": case "meadow": return Plains;
                case "desert": return Desert;
                case "savanna": case "savanna_plateau": return Savanna;
                case "taiga": return Taiga;
                case "snowy_plains": case "snowy_taiga": return Snowy;
            }
            return null;
        }
        public static VillageStyle ByName(string n)
        {
            switch (n) { case "desert": return Desert; case "savanna": return Savanna; case "taiga": return Taiga; case "snowy": return Snowy; }
            return Plains;
        }
    }

    // =====================================================================================================
    public sealed class VillageStructure : StructureBase
    {
        public VillageStructure() { id = "village"; spacing = 34; separation = 8; salt = 10387; maxRadiusChunks = 5; }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
        {
            if (!(g is OverworldGenerator)) return false;
            if (VillageStyle.For(g.BiomeAtApprox(x, z)) == null) return false;
            return StructureTerrain.DryFlat(g, x, z, 8, 7, out _);
        }

        // house kinds (index into Kinds): job site, profession variant, size
        struct Kind { public string name, station; public int variant, sx, sz, wallH; public Kind(string n, string s, int v, int x, int z, int h) { name = n; station = s; variant = v; sx = x; sz = z; wallH = h; } }
        static readonly Kind[] Kinds =
        {
            new Kind("small", null, 0, 5, 5, 3),
            new Kind("farmer", "composter", 1, 7, 6, 3),
            new Kind("librarian", "lectern", 2, 9, 7, 4),
            new Kind("armorer", "blast_furnace", 3, 7, 7, 3),
            new Kind("weaponsmith", "grindstone", 4, 9, 7, 3),
            new Kind("toolsmith", "smithing_table", 5, 7, 7, 3),
            new Kind("cleric", "brewing_stand", 6, 7, 7, 7),
            new Kind("butcher", "smoker", 7, 7, 7, 3),
            new Kind("cartographer", "cartography_table", 13, 7, 7, 4),
            new Kind("fletcher", "fletching_table", 9, 7, 6, 3),
            new Kind("fisherman", "barrel", 8, 7, 6, 3),
            new Kind("leatherworker", "cauldron", 10, 7, 6, 3),
            new Kind("mason", "stonecutter", 11, 7, 6, 3),
            new Kind("shepherd", "loom", 12, 7, 6, 3),
            new Kind("large", null, 0, 9, 9, 4),
        };

        struct Rect { public int x0, z0, x1, z1; public Rect(int a, int b, int c, int d) { x0 = Math.Min(a, c); z0 = Math.Min(b, d); x1 = Math.Max(a, c); z1 = Math.Max(b, d); } public bool Hits(Rect o) => x0 <= o.x1 && x1 >= o.x0 && z0 <= o.z1 && z1 >= o.z0; }

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            var style = VillageStyle.For(g.BiomeAtApprox(x, z));
            if (style == null) return null;
            int cy = StructureTerrain.FloorLevel(g, x - 3, z - 3, x + 3, z + 3);
            var st = NewStart(x, cy + 1, z);
            var used = new List<Rect>();

            // meeting point (well + bell)
            var well = new VillageWell { style = style.name, seed = NextSeed(ref rng) };
            well.PlaceCentered(x, cy, z, 7, 7, 0, 5, 8, 0);
            st.pieces.Add(well);
            used.Add(new Rect(x - 4, z - 4, x + 4, z + 4));

            // roads: 2..4 arms, with an optional side branch each
            var roads = new List<(int x0, int z0, int x1, int z1, Dir d)>();
            int arms = 0;
            for (int i = 0; i < 4; i++)
            {
                if (!(rng.Chance(0.8f) || (i >= 2 && arms < 2))) continue;
                Dir d = DirUtil.Horizontal[i];
                var o = DirUtil.Offset[(int)d];
                int len = rng.Range(22, 36);
                int sx0 = x + o.x * 4, sz0 = z + o.z * 4;
                int ex = x + o.x * len, ez = z + o.z * len;
                roads.Add((sx0, sz0, ex, ez, d));
                arms++;
                if (rng.Chance(0.7f))
                {
                    int at = rng.Range(12, len - 4);
                    Dir bd = rng.NextBool() ? DirUtil.RotateCW(d) : DirUtil.RotateCCW(d);
                    var bo = DirUtil.Offset[(int)bd];
                    int bl = rng.Range(10, 20);
                    int bx0 = x + o.x * at + bo.x * 2, bz0 = z + o.z * at + bo.z * 2;
                    roads.Add((bx0, bz0, bx0 + bo.x * bl, bz0 + bo.z * bl, bd));
                }
            }
            foreach (var r in roads)
            {
                var road = new VillageRoad { style = style.name, x0 = r.x0, z0 = r.z0, x1 = r.x1, z1 = r.z1, y = cy, seed = NextSeed(ref rng) };
                road.box = new BBox(Math.Min(r.x0, r.x1) - 1, cy - 10, Math.Min(r.z0, r.z1) - 1, Math.Max(r.x0, r.x1) + 2, cy + 12, Math.Max(r.z0, r.z1) + 2);
                st.pieces.Add(road);
                used.Add(new Rect(r.x0 - 1, r.z0 - 1, r.x1 + 1, r.z1 + 1));
            }

            // houses and farms along both sides of every road
            int target = rng.Range(6, 12), houses = 0, farms = 0;
            var slots = new List<(int px, int pz, Dir side, Dir along)>();
            foreach (var r in roads)
            {
                var o = DirUtil.Offset[(int)r.d];
                int len = Math.Abs(r.x1 - r.x0) + Math.Abs(r.z1 - r.z0);
                for (int t = 5; t <= len - 2; t += 9)
                {
                    int px = r.x0 + o.x * t, pz = r.z0 + o.z * t;
                    slots.Add((px, pz, DirUtil.RotateCW(r.d), r.d));
                    slots.Add((px, pz, DirUtil.RotateCCW(r.d), r.d));
                }
            }
            rng.Shuffle(slots);
            foreach (var sl in slots)
            {
                if (houses >= target) break;
                bool farm = farms < 3 && rng.Chance(0.18f);
                int k = farm ? -1 : PickKind(ref rng, houses);
                int hsx = farm ? 9 : Kinds[k].sx, hsz = farm ? 9 : Kinds[k].sz;
                var so = DirUtil.Offset[(int)sl.side];
                int rot = FramedPiece.RotForFront(DirUtil.Opposite(sl.side));
                int ox, oz;
                // footprint: frontage centred on the slot, set back 2 blocks from the road centreline
                if (so.z != 0)
                {
                    ox = sl.px - hsx / 2;
                    oz = so.z > 0 ? sl.pz + 3 : sl.pz - 3 - (hsz - 1);
                }
                else
                {
                    oz = sl.pz - hsx / 2;
                    ox = so.x > 0 ? sl.px + 3 : sl.px - 3 - (hsz - 1);
                }
                int wsx = (rot & 1) == 1 ? hsz : hsx, wsz = (rot & 1) == 1 ? hsx : hsz;
                var rect = new Rect(ox - 1, oz - 1, ox + wsx, oz + wsz);
                bool clash = false;
                foreach (var u in used) if (u.Hits(rect)) { clash = true; break; }
                if (clash) continue;
                if (Math.Abs(ox + wsx / 2 - x) > 72 || Math.Abs(oz + wsz / 2 - z) > 72) continue;
                int relief = StructureTerrain.Relief(g, ox, oz, ox + wsx - 1, oz + wsz - 1);
                if (relief > 6) continue;
                int fy = StructureTerrain.FloorLevel(g, ox, oz, ox + wsx - 1, oz + wsz - 1);
                if (fy <= g.world.seaLevel) continue;
                if (Math.Abs(fy - cy) > 14) continue;
                used.Add(rect);
                if (farm)
                {
                    var f = new VillageFarm { style = style.name, seed = NextSeed(ref rng) };
                    f.Place(ox, fy, oz, hsx, hsz, rot, 6, 4, 0);
                    st.pieces.Add(f);
                    farms++;
                }
                else
                {
                    var kd = Kinds[k];
                    var h = new VillageHouse
                    {
                        style = style.name, kind = k, station = kd.station, variant = kd.variant, wallH = kd.wallH,
                        bed = style.beds[rng.Next(style.beds.Length)], crop = style.crops[rng.Next(style.crops.Length)], seed = NextSeed(ref rng)
                    };
                    h.Place(ox, fy, oz, hsx, hsz, rot, 8, kd.wallH + 8, 1);
                    st.pieces.Add(h);
                    houses++;
                }
            }
            if (houses < 4) return null;

            // lamps at the road ends
            foreach (var r in roads)
            {
                var o = DirUtil.Offset[(int)r.d];
                var side = DirUtil.Offset[(int)DirUtil.RotateCW(r.d)];
                int lx = r.x1 + o.x + side.x * 2, lz = r.z1 + o.z + side.z * 2;
                var rect = new Rect(lx, lz, lx, lz);
                bool clash = false;
                foreach (var u in used) if (u.Hits(rect)) { clash = true; break; }
                if (clash) continue;
                used.Add(rect);
                var lamp = new VillageLamp { style = style.name, y = cy, seed = NextSeed(ref rng) };
                lamp.Place(lx, cy, lz, 1, 1, 0, 12, 12, 0);
                st.pieces.Add(lamp);
            }
            return Finish(st, cx, cz);
        }

        static int PickKind(ref RNG rng, int placed)
        {
            // guarantee a farmer and a librarian early, then mix
            if (placed == 0) return 1;
            if (placed == 1) return 2;
            return rng.Range(0, Kinds.Length - 1);
        }

        // ---------------------------------------------------------------- pieces
        sealed class VillageWell : FramedPiece
        {
            public string style;
            protected override void Emit(SB b)
            {
                var s = VillageStyle.ByName(style);
                b.FoundationArea(0, 0, 6, 6, 0, s.foundation);
                b.ClearUp(0, 0, 6, 6, 1, 7);
                b.Fill(0, 0, 0, 6, 0, 6, s.path == "dirt_path" ? "cobblestone" : s.path);
                if (s.name == "desert")
                {
                    b.Fill(1, -4, 1, 5, 0, 5, "sandstone");
                    b.Fill(2, -3, 2, 4, 0, 4, "water");
                    b.Walls(1, 1, 1, 5, 1, 5, "sandstone");
                    b.Fill(2, 1, 2, 4, 1, 4, "air");
                    b.Fill(1, 2, 1, 1, 3, 1, "sandstone_wall"); b.Fill(5, 2, 1, 5, 3, 1, "sandstone_wall");
                    b.Fill(1, 2, 5, 1, 3, 5, "sandstone_wall"); b.Fill(5, 2, 5, 5, 3, 5, "sandstone_wall");
                    b.Fill(1, 4, 1, 5, 4, 5, "cut_sandstone_slab");
                    b.Set(3, 4, 3, "chiseled_sandstone");
                    b.Facing(3, 1, 0, "bell", Dir.East);
                    b.Set(3, 0, 0, "cut_sandstone");
                }
                else
                {
                    b.Fill(1, -4, 1, 5, 0, 5, "cobblestone");
                    b.Fill(2, -3, 2, 4, 0, 4, "water");
                    b.Walls(1, 1, 1, 5, 1, 5, "cobblestone");
                    b.Fill(2, 1, 2, 4, 1, 4, "air");
                    string post = s.fence;
                    b.Fill(1, 2, 1, 1, 3, 1, post); b.Fill(5, 2, 1, 5, 3, 1, post);
                    b.Fill(1, 2, 5, 1, 3, 5, post); b.Fill(5, 2, 5, 5, 3, 5, post);
                    b.Fill(1, 4, 1, 5, 4, 5, "cobblestone_slab");
                    b.Set(3, 4, 3, "cobblestone");
                    b.Facing(3, 1, 0, "bell", Dir.East);
                    b.Set(3, 0, 0, "cobblestone");
                }
                b.Torch(1, 5, 1); b.Torch(5, 5, 5);
                // meeting point residents
                b.Mob(3, 1, 6, "iron_golem");
                b.Mob(0, 1, 3, "cat", (int)(b.R(0, 0, 0, 7) * 11));
                b.Mob(6, 1, 3, "villager", 0);
            }
        }

        sealed class VillageRoad : StructurePiece
        {
            public string style; public int x0, z0, x1, z1, y;
            public override void Build(StructureWriter w)
            {
                var b = new SB(w, seed).World();
                var s = VillageStyle.ByName(style);
                var path = Blocks.Get(s.path); var bridge = Blocks.Get(s.bridge);
                if (path == null) return;
                int ax0 = Math.Min(x0, x1) - 1, ax1 = Math.Max(x0, x1) + 1, az0 = Math.Min(z0, z1) - 1, az1 = Math.Max(z0, z1) + 1;
                bool alongX = z0 == z1;
                for (int zz = az0; zz <= az1; zz++)
                    for (int xx = ax0; xx <= ax1; xx++)
                    {
                        if (!b.Owns(xx, y, zz)) continue;
                        // the path follows the real terrain of this column (single column -> one chunk)
                        int top = b.TerrainTop(xx, zz, y + 10, y - 10);
                        if (top == int.MinValue) continue;
                        var tb = b.BlockAt(xx, top, zz);
                        if (tb.isLiquid)
                        {
                            if (bridge != null && top >= w.gen.world.seaLevel - 1) b.Set(xx, top, zz, bridge.DefaultState);
                            continue;
                        }
                        // keep the road edge ragged like worn paths
                        bool edge = alongX ? (zz == az0 || zz == az1) : (xx == ax0 || xx == ax1);
                        if (edge && b.P(xx, 0, zz, 0.25f, 3)) continue;
                        if (!(tb is GrassBlock || tb.id == "dirt" || tb.id == "sand" || tb.id == "red_sand" || tb.id == "coarse_dirt" || tb.id == "gravel"
                              || tb.id == "snow_block" || tb.id == "stone" || tb.id == "podzol" || tb.id == "terracotta" || tb.id == "mud" || tb.id == "sandstone")) continue;
                        b.Set(xx, top, zz, path.DefaultState);
                        b.ClearUp(xx, zz, xx, zz, top + 1, 3);
                    }
            }
        }

        sealed class VillageLamp : FramedPiece
        {
            public string style; public int y;
            protected override void Emit(SB b)
            {
                var s = VillageStyle.ByName(style);
                int top = b.TerrainTop(0, 0, 10, -10);
                if (top == int.MinValue) return;
                if (b.BlockAt(0, top, 0).isLiquid) return;
                b.ClearUp(0, 0, 0, 0, top + 1, 5);
                b.Fill(0, top + 1, 0, 0, top + 3, 0, s.fence);
                if (s.light == "lantern") { b.Set(0, top + 4, 0, s.roofBlock); b.Lantern(0, top + 5, 0); }
                else { b.Set(0, top + 4, 0, s.roofBlock); b.Torch(0, top + 5, 0); }
            }
        }

        sealed class VillageFarm : FramedPiece
        {
            public string style;
            protected override void Emit(SB b)
            {
                var s = VillageStyle.ByName(style);
                int X = sx - 1, Z = sz - 1;
                b.FoundationArea(0, 0, X, Z, 0, "dirt");
                b.ClearUp(0, 0, X, Z, 1, 4);
                string border = s.name == "desert" ? "cut_sandstone" : (s.corner.Contains("log") ? s.corner : "oak_log");
                for (int x = 0; x <= X; x++) { b.Log(x, 0, 0, border, 1); b.Log(x, 0, Z, border, 1); }
                for (int z = 1; z < Z; z++) { b.Log(0, 0, z, border, 2); b.Log(X, 0, z, border, 2); }
                int mid = X / 2;
                for (int z = 1; z < Z; z++)
                    for (int x = 1; x < X; x++)
                    {
                        if (x == mid) { b.Set(x, 0, z, "water"); continue; }
                        b.Set(x, 0, z, "farmland", 7);
                        int half = x < mid ? 0 : 1;
                        string crop = s.crops[(int)(b.R(half, 0, z / 4, 11) * s.crops.Length) % s.crops.Length];
                        var cb = Blocks.Get(crop);
                        if (cb is CropBlock c) b.Set(x, 1, z, cb.State(Math.Min(c.maxAge, 4 + (int)(b.R(x, 1, z, 12) * 4))));
                    }
                b.Set(1, 1, 0, "composter");
                b.Log(X, 1, 0, "hay_block");
                b.Log(X, 2, 0, "hay_block");
                b.Mob(mid, 1, 0, "villager", 1);
            }
        }

        sealed class VillageHouse : FramedPiece
        {
            public string style, station, bed, crop; public int kind, variant, wallH;

            protected override void Emit(SB b)
            {
                var s = VillageStyle.ByName(style);
                int X = sx - 1, Z = sz - 1, H = wallH;
                int dx = sx / 2;
                bool cleric = station == "brewing_stand";
                string wall = cleric ? (s.name == "desert" ? "cut_sandstone" : "cobblestone") : s.wall;
                string baseC = cleric ? "stone_bricks" : s.baseCourse;

                // ground work: fill below, clear above (terrain + trees)
                b.FoundationArea(0, 0, X, Z, 0, s.foundation, 16);
                b.ClearUp(-1, -1, X + 1, Z + 1, 1, H + sz / 2 + 4);
                b.Fill(0, 0, 0, X, 0, Z, s.floor);
                b.Walls(0, 1, 0, X, 1, Z, baseC);
                b.Walls(0, 2, 0, X, H, Z, wall);
                if (!s.flatRoof || s.name != "desert")
                {
                    b.Fill(0, 1, 0, 0, H, 0, s.corner); b.Fill(X, 1, 0, X, H, 0, s.corner);
                    b.Fill(0, 1, Z, 0, H, Z, s.corner); b.Fill(X, 1, Z, X, H, Z, s.corner);
                }
                else
                {
                    b.Fill(0, 1, 0, 0, H, 0, s.accent); b.Fill(X, 1, 0, X, H, 0, s.accent);
                }

                // windows
                string win = s.window;
                for (int z = 2; z <= Z - 2; z += 2) { b.Set(0, 2, z, win); b.Set(X, 2, z, win); if (H >= 4) { b.Set(0, 3, z, win); b.Set(X, 3, z, win); } }
                for (int x = 2; x <= X - 2; x += 3) b.Set(x, 2, Z, win);
                if (sx >= 7) { b.Set(dx - 2, 2, 0, win); b.Set(dx + 2, 2, 0, win); }

                // door, step and exterior light
                b.Air(dx, 1, 0); b.Air(dx, 2, 0);
                b.Door(dx, 1, 0, s.door, Dir.North);
                b.Foundation(dx, -1, 0, s.foundation, 6);
                b.Stairs(dx, 0, -1, s.name == "desert" ? "sandstone_stairs" : (s.name == "snowy" || s.name == "taiga" ? "spruce_stairs" : (s.name == "savanna" ? "acacia_stairs" : "oak_stairs")), Dir.North);
                b.Air(dx, 1, -1); b.Air(dx, 2, -1);
                b.Torch(dx - 1, 2, -1, Dir.South);
                b.Torch(dx + 1, 2, -1, Dir.South);

                // roof
                if (s.flatRoof)
                {
                    b.Fill(-1, H + 1, -1, X + 1, H + 1, Z + 1, s.roofBlock);
                    for (int x = -1; x <= X + 1; x++) { b.Set(x, H + 2, -1, s.fence); b.Set(x, H + 2, Z + 1, s.fence); }
                    for (int z = 0; z <= Z; z++) { b.Set(-1, H + 2, z, s.fence); b.Set(X + 1, H + 2, z, s.fence); }
                    b.Set(dx, H + 1, 0, s.accent);
                }
                else if (cleric)
                {
                    // bell-tower cap
                    b.Fill(-1, H + 1, -1, X + 1, H + 1, Z + 1, "stone_brick_slab", 0);
                    b.Walls(0, H + 1, 0, X, H + 2, Z, "stone_bricks");
                    b.Fill(1, H + 1, 1, X - 1, H + 1, Z - 1, s.floor);
                    for (int x = 1; x < X; x += 2) { b.Set(x, H + 2, 0, "air"); b.Set(x, H + 2, Z, "air"); }
                }
                else
                {
                    GableRoof(b, 0, X, 0, Z, H + 1, s.roofStairs, s.roofSlab, wall);
                }

                // interior
                b.Torch(1, 3, 1, Dir.North);
                b.Torch(X - 1, 3, Z - 1, Dir.South);
                Furnish(b, s, X, Z, H, dx);
            }

            static void GableRoof(SB b, int x0, int x1, int z0, int z1, int y, string stairs, string slab, string gable)
            {
                int zl = z0 - 1, zr = z1 + 1;
                while (zl < zr)
                {
                    for (int x = x0 - 1; x <= x1 + 1; x++) { b.Stairs(x, y, zl, stairs, Dir.North); b.Stairs(x, y, zr, stairs, Dir.South); }
                    for (int z = zl + 1; z <= zr - 1; z++) { b.Set(x0, y, z, gable); b.Set(x1, y, z, gable); }
                    zl++; zr--; y++;
                }
                if (zl == zr) for (int x = x0 - 1; x <= x1 + 1; x++) b.Slab(x, y, zl, slab);
            }

            void Furnish(SB b, VillageStyle s, int X, int Z, int H, int dx)
            {
                // bed against the back wall
                b.Bed(1, 1, Z - 2, bed, Dir.North);
                if (kind == 14) b.Bed(X - 1, 1, Z - 2, bed, Dir.North);
                // job site on the right-hand wall, storage on the left
                if (station != null)
                {
                    switch (station)
                    {
                        case "lectern": b.Lectern(X - 1, 1, 2, Dir.West, true); break;
                        case "cauldron": b.Set(X - 1, 1, 2, "cauldron", 3); break;
                        case "barrel": b.Barrel(X - 1, 1, 2, "village_house"); break;
                        case "composter": b.Set(X - 1, 1, 2, "composter", 0); break;
                        case "blast_furnace": b.Facing(X - 1, 1, 2, "blast_furnace", Dir.West); b.Fill(X - 1, 2, 2, X - 1, H, 2, "cobblestone"); break;
                        case "smoker": b.Facing(X - 1, 1, 2, "smoker", Dir.West); break;
                        default: b.Facing(X - 1, 1, 2, station, Dir.West); break;
                    }
                }
                string loot = "village_house";
                if (station == "grindstone") loot = "village_weaponsmith";
                else if (station == "smithing_table") loot = "village_toolsmith";
                else if (station == "blast_furnace") loot = "village_armorer";
                else if (station == "brewing_stand") loot = "village_temple";
                b.Chest(1, 1, 1, loot, Dir.East);
                if (kind == 0 || kind == 14) b.Set(X - 1, 1, 1, "crafting_table");
                // kind-specific dressing
                switch (station)
                {
                    case "lectern":
                        b.Fill(2, 1, Z - 1, X - 2, H - 1, Z - 1, "bookshelf");
                        break;
                    case "grindstone":
                        b.Facing(X - 1, 1, Z - 2, "anvil", Dir.North);
                        b.Facing(X - 2, 1, 1, "furnace", Dir.South);
                        break;
                    case "smithing_table":
                        b.Facing(X - 1, 1, Z - 2, "anvil", Dir.North);
                        break;
                    case "brewing_stand":
                        // two floors: ladder up to the altar room
                        b.Fill(1, 4, 1, X - 1, 4, Z - 1, s.floor);
                        b.Air(X - 1, 4, Z - 1);
                        b.Ladders(X - 1, 1, 4, Z - 1, Dir.South);
                        b.Set(X - 1, 5, 2, "brewing_stand");
                        b.Set(1, 5, Z - 1, "cauldron", 0);
                        b.Torch(dx, 6, Z - 1, Dir.South);
                        b.Set(dx, 5, 1, "lectern", 0);
                        break;
                    case "loom":
                        b.Fill(2, 1, 2, X - 2, 1, Z - 3, "white_carpet");
                        break;
                    case "cartography_table":
                        b.Fill(X - 1, 1, Z - 1, X - 1, 2, Z - 1, "bookshelf");
                        break;
                    case "barrel":
                        b.Barrel(X - 1, 1, Z - 1, "village_house");
                        break;
                    case "composter":
                        b.Set(X - 1, 1, Z - 1, "hay_block");
                        break;
                    case "smoker":
                        b.Set(X - 1, 1, Z - 1, "barrel", 1);
                        break;
                    case "stonecutter":
                        b.Set(X - 1, 1, Z - 1, "stone_bricks");
                        break;
                }
                // a table with a flower pot
                if (sx >= 7) { b.Set(dx, 1, Z - 1, s.fence); b.Set(dx, 2, Z - 1, "flower_pot"); }
                // residents
                b.Mob(dx, 1, Z / 2 + 1, "villager", variant);
                if (kind == 14) b.Mob(dx - 1, 1, Z / 2, "villager", 0);
                if (b.R(0, 0, 0, 99) < 0.25f) b.Mob(dx + 1, 1, Z / 2, "villager", 0, true);
            }
        }
    }
}
