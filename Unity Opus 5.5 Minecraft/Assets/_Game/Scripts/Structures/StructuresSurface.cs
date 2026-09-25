using System;
using System.Collections.Generic;

namespace MCR
{
    // All pieces below use a local frame: x across the front, z from the front (z = 0) to the back,
    // y = 0 is the ground floor level fitted to the terrain.

    // =====================================================================================================
    //  Desert pyramid: stepped sandstone pyramid, two corner towers, a hall with a terracotta mosaic and a
    //  buried treasure chamber (four chests, TNT under a pressure plate) straight below the blue tile.
    // =====================================================================================================
    public sealed class DesertPyramidStructure : BuildingStructure
    {
        public DesertPyramidStructure() { id = "desert_pyramid"; spacing = 32; separation = 8; salt = 40009; maxRadiusChunks = 2; bx = 21; bz = 21; yDown = 16; yUp = 16; }

        protected override bool Allowed(WorldGenerator g, int x, int z)
            => g.BiomeAtApprox(x, z) == "desert" && StructureTerrain.DryFlat(g, x, z, 10, 7, out _);

        protected override FramedPiece NewPiece(WorldGenerator g, ref RNG rng) => new Pyramid();

        sealed class Pyramid : FramedPiece
        {
            const int N = 20, C = 10;
            protected override void Emit(SB b)
            {
                b.FoundationArea(0, 0, N, N, 0, "sandstone", 16);
                b.ClearUp(-1, -1, N + 1, N + 1, 1, 16);
                // stepped body: layer i is the square [i, N - i] at height i
                for (int i = 0; i <= C; i++)
                    b.Fill3(i, i, i, N - i, i, N - i, "sandstone", "cut_sandstone", i % 3 == 2 ? 0.6f : 0.04f, null, 0f, 11);
                // entrance tunnel and hall
                b.Clear(C - 1, 1, 0, C + 1, 3, 5);
                b.Clear(5, 1, 5, 15, 4, 15);
                foreach (var (px, pz) in new[] { (7, 7), (13, 7), (7, 13), (13, 13) })
                {
                    b.Fill(px, 1, pz, px, 4, pz, "cut_sandstone");
                    b.Set(px, 4, pz, "chiseled_sandstone");
                }
                b.Torch(7, 3, 8, Dir.North); b.Torch(13, 3, 8, Dir.North);
                b.Torch(7, 3, 12, Dir.South); b.Torch(13, 3, 12, Dir.South);
                // floor mosaic marking the chamber below
                b.Fill(8, 0, 8, 12, 0, 12, "orange_terracotta");
                b.Fill(9, 0, 9, 11, 0, 11, "sandstone");
                b.Set(C, 0, C, "blue_terracotta");
                foreach (var (px, pz) in new[] { (8, 8), (12, 8), (8, 12), (12, 12) }) b.Set(px, 0, pz, "blue_terracotta");
                // facade
                b.Fill(C - 2, 1, 0, C + 2, 6, 0, "cut_sandstone");
                b.Clear(C - 1, 1, 0, C + 1, 3, 0);
                b.Fill(C - 1, 4, 0, C + 1, 4, 0, "orange_terracotta");
                b.Set(C, 5, 0, "blue_terracotta");
                b.Set(C - 2, 6, 0, "chiseled_sandstone"); b.Set(C + 2, 6, 0, "chiseled_sandstone");
                b.StairsRow(C - 1, 0, -1, C + 1, -1, "sandstone_stairs", Dir.North);
                Tower(b, 0, 0);
                Tower(b, N - 4, 0);
                // treasure chamber
                b.Fill(6, -14, 6, 14, -8, 14, "sandstone");
                b.Clear(8, -12, 8, 12, -9, 12);
                b.Fill(9, -13, 9, 11, -13, 11, "orange_terracotta");
                b.Fill(9, -14, 9, 11, -14, 11, "tnt");
                b.Set(C, -13, C, "tnt");
                b.Set(C, -12, C, "stone_pressure_plate");
                b.Chest(C, -12, 7, "desert_pyramid", Dir.North);
                b.Chest(C, -12, 13, "desert_pyramid", Dir.South);
                b.Chest(7, -12, C, "desert_pyramid", Dir.East);
                b.Chest(13, -12, C, "desert_pyramid", Dir.West);
                b.Set(8, -10, 8, "chiseled_sandstone"); b.Set(12, -10, 12, "chiseled_sandstone");
                b.Set(8, -10, 12, "chiseled_sandstone"); b.Set(12, -10, 8, "chiseled_sandstone");
            }

            static void Tower(SB b, int tx, int tz)
            {
                b.Fill3(tx, 0, tz, tx + 4, 13, tz + 4, "sandstone", "cut_sandstone", 0.15f, null, 0f, 12);
                // hollow shaft, solid roof at 13 with the ladder coming up through it
                b.Clear(tx + 1, 1, tz + 1, tx + 3, 12, tz + 3);
                b.Walls(tx, 10, tz, tx + 4, 10, tz + 4, "orange_terracotta");
                b.Clear(tx + 2, 1, tz, tx + 2, 2, tz);
                b.Ladders(tx + 2, 1, 13, tz + 3, Dir.South);
                for (int i = 0; i <= 4; i += 2)
                {
                    b.Set(tx + i, 14, tz, "cut_sandstone"); b.Set(tx + i, 14, tz + 4, "cut_sandstone");
                    b.Set(tx, 14, tz + i, "cut_sandstone"); b.Set(tx + 4, 14, tz + i, "cut_sandstone");
                }
                b.Clear(tx, 7, tz + 2, tx, 8, tz + 2);
                b.Clear(tx + 4, 7, tz + 2, tx + 4, 8, tz + 2);
                b.Clear(tx + 2, 7, tz, tx + 2, 8, tz);
            }
        }
    }

    // =====================================================================================================
    //  Jungle temple: mossy cobblestone temple with an upper shrine, a basement treasure room and an arrow
    //  trap (pressure plate beside a loaded dispenser, standing in for a tripwire).
    // =====================================================================================================
    public sealed class JungleTempleStructure : BuildingStructure
    {
        public JungleTempleStructure() { id = "jungle_temple"; spacing = 32; separation = 8; salt = 40031; maxRadiusChunks = 2; bx = 13; bz = 15; yDown = 8; yUp = 15; }

        protected override bool Allowed(WorldGenerator g, int x, int z)
        {
            string b = g.BiomeAtApprox(x, z);
            if (b != "jungle" && b != "bamboo_jungle" && b != "sparse_jungle") return false;
            return StructureTerrain.DryFlat(g, x, z, 7, 8, out _);
        }

        protected override FramedPiece NewPiece(WorldGenerator g, ref RNG rng) => new Temple();

        sealed class Temple : FramedPiece
        {
            static void Mossy(SB b, int x0, int y0, int z0, int x1, int y1, int z1, int salt)
                => b.Fill3(x0, y0, z0, x1, y1, z1, "cobblestone", "mossy_cobblestone", 0.45f, null, 0f, salt);
            static void MossyWalls(SB b, int x0, int y0, int z0, int x1, int y1, int z1, int salt)
                => b.Walls3(x0, y0, z0, x1, y1, z1, "cobblestone", "mossy_cobblestone", 0.45f, null, 0f, salt);

            protected override void Emit(SB b)
            {
                const int X = 12, Z = 14;
                b.FoundationArea(0, 0, X, Z, 0, "cobblestone", 12);
                b.ClearUp(-1, -1, X + 1, Z + 1, 1, 15);
                // basement block (carved below)
                Mossy(b, 0, -6, 0, X, 0, Z, 1);
                // ground floor
                MossyWalls(b, 0, 1, 0, X, 4, Z, 2);
                Mossy(b, 0, 5, 0, X, 5, Z, 3);
                b.Clear(1, 1, 1, X - 1, 4, Z - 1);
                b.Clear(5, 1, 0, 7, 3, 0);
                b.Set(4, 4, 0, "chiseled_stone_bricks"); b.Set(8, 4, 0, "chiseled_stone_bricks");
                foreach (int pz in new[] { 3, 7 })
                    foreach (int px in new[] { 3, 9 })
                        Mossy(b, px, 1, pz, px, 4, pz, 4);
                b.Torch(3, 3, 4, Dir.North); b.Torch(9, 3, 4, Dir.North);
                // upper shrine
                MossyWalls(b, 2, 6, 2, 10, 9, 12, 5);
                Mossy(b, 2, 10, 2, 10, 10, 12, 6);
                b.Clear(3, 6, 3, 9, 9, 11);
                Mossy(b, 4, 11, 4, 8, 11, 10, 7);
                Mossy(b, 5, 12, 5, 7, 12, 9, 8);
                b.Set(6, 13, 7, "chiseled_stone_bricks");
                b.Clear(10, 6, 7, 10, 7, 7);
                b.Chest(4, 6, 10, "jungle_temple", Dir.East);
                b.Torch(8, 8, 11, Dir.South);
                // ladder from the hall onto the shrine terrace
                b.Clear(11, 5, 13, 11, 5, 13);
                b.Ladders(11, 1, 5, 13, Dir.West);
                // stairwell down to the basement (descends toward the front)
                b.Clear(1, -4, 8, 2, 0, 12);
                for (int i = 0; i < 4; i++)
                {
                    int sz = 12 - i, sy = -1 - i;
                    if (sy - 1 >= -4) Mossy(b, 1, -4, sz, 2, sy - 1, sz, 9);
                    b.StairsRow(1, sy, sz, 2, sz, "cobblestone_stairs", Dir.North);
                }
                // basement corridor and treasure room
                b.Clear(1, -4, 2, 2, -2, 8);
                b.Clear(3, -4, 1, 8, -2, 4);
                b.Torch(1, -2, 7, Dir.East);
                b.Torch(6, -2, 4, Dir.South);
                b.Chest(8, -4, 2, "jungle_temple", Dir.West);
                b.Set(7, -4, 4, "chiseled_stone_bricks");
                // arrow trap: pressure plate in the corridor, dispenser in the wall right beside it
                b.Set(1, -4, 5, "stone_pressure_plate");
                b.Set(2, -4, 5, "stone_pressure_plate");
                var disp = b.SetWithEntity(0, -4, 5, "dispenser", (int)Dir.East, null) as DispenserEntity;
                if (disp != null) { disp.items[0] = new ItemStack("arrow", 16); disp.items[1] = new ItemStack("arrow", 8); }
                // vines creeping over the outer walls
                for (int x = 0; x <= X; x++)
                    for (int y = 1; y <= 5; y++)
                    {
                        if (b.P(x, y, -1, 0.3f, 20)) b.Vine(x, y, -1, Dir.North);
                        if (b.P(x, y, Z + 1, 0.3f, 21)) b.Vine(x, y, Z + 1, Dir.South);
                    }
                for (int z = 0; z <= Z; z++)
                    for (int y = 1; y <= 5; y++)
                    {
                        if (b.P(-1, y, z, 0.3f, 22)) b.Vine(-1, y, z, Dir.East);
                        if (b.P(X + 1, y, z, 0.3f, 23)) b.Vine(X + 1, y, z, Dir.West);
                    }
            }
        }
    }

    // =====================================================================================================
    //  Swamp hut: spruce hut raised on oak stilts above the swamp water, with a porch, a cauldron and a
    //  crafting table, home to a witch and a black cat.
    // =====================================================================================================
    public sealed class SwampHutStructure : BuildingStructure
    {
        public SwampHutStructure() { id = "swamp_hut"; spacing = 32; separation = 8; salt = 40037; maxRadiusChunks = 2; bx = 7; bz = 9; yDown = 16; yUp = 10; }

        protected override bool Allowed(WorldGenerator g, int x, int z)
        {
            string b = g.BiomeAtApprox(x, z);
            if (b != "swamp" && b != "mangrove_swamp") return false;
            int h = StructureTerrain.Ground(g, x, z);
            return h >= g.world.seaLevel - 5 && StructureTerrain.Relief(g, x - 4, z - 4, x + 4, z + 4) <= 5;
        }

        // the hut stands above the water, not on the floor
        protected override int FloorY(WorldGenerator g, int x0, int z0, int x1, int z1)
            => Math.Max(g.world.seaLevel + 1, StructureTerrain.FloorLevel(g, x0, z0, x1, z1)) + 2;

        protected override FramedPiece NewPiece(WorldGenerator g, ref RNG rng) => new Hut();

        sealed class Hut : FramedPiece
        {
            protected override void Emit(SB b)
            {
                // stilts at the room corners, down to the swamp floor
                foreach (var (px, pz) in new[] { (1, 2), (5, 2), (1, 7), (5, 7) })
                {
                    b.Foundation(px, pz, 0, "oak_log", 18);
                    b.Set(px, 0, pz, "oak_log", 0);
                }
                b.ClearUp(0, 0, 6, 8, 1, 10);
                b.Fill(0, 0, 0, 6, 0, 8, "spruce_planks");
                // room (z 3..8) plus an open porch (z 1..2)
                b.Walls(1, 1, 3, 5, 3, 8, "spruce_planks");
                foreach (var (px, pz) in new[] { (1, 3), (5, 3), (1, 8), (5, 8) }) b.Fill(px, 1, pz, px, 3, pz, "oak_log", 0);
                b.Clear(3, 1, 3, 3, 2, 3);
                b.Set(3, 3, 3, "oak_fence");
                // porch posts and railing
                b.Fill(1, 1, 1, 1, 4, 1, "oak_fence");
                b.Fill(5, 1, 1, 5, 4, 1, "oak_fence");
                b.Fill(2, 1, 1, 4, 1, 1, "oak_fence");
                b.Set(1, 1, 5, "oak_fence"); b.Set(5, 1, 5, "oak_fence");
                // pitched roof, ridge along z
                for (int z = 0; z <= 8; z++)
                {
                    b.Stairs(0, 4, z, "spruce_stairs", Dir.East); b.Stairs(6, 4, z, "spruce_stairs", Dir.West);
                    b.Stairs(1, 5, z, "spruce_stairs", Dir.East); b.Stairs(5, 5, z, "spruce_stairs", Dir.West);
                    b.Stairs(2, 6, z, "spruce_stairs", Dir.East); b.Stairs(4, 6, z, "spruce_stairs", Dir.West);
                    b.Slab(3, 7, z, "spruce_slab");
                }
                b.Fill(1, 4, 3, 5, 4, 3, "spruce_planks"); b.Fill(2, 5, 3, 4, 5, 3, "spruce_planks"); b.Set(3, 6, 3, "spruce_planks");
                b.Fill(1, 4, 8, 5, 4, 8, "spruce_planks"); b.Fill(2, 5, 8, 4, 5, 8, "spruce_planks"); b.Set(3, 6, 8, "spruce_planks");
                // furnishings
                b.Set(2, 1, 7, "crafting_table");
                b.Set(4, 1, 7, "cauldron", 0);
                b.Set(4, 1, 5, "flower_pot");
                b.Set(1, 3, 6, "oak_fence");
                b.Torch(2, 3, 4, Dir.North);
                b.Mob(3, 1, 6, "witch");
                b.Mob(2, 1, 4, "cat", 1);   // variant 1 is the black coat
            }
        }
    }

    // =====================================================================================================
    //  Igloo: snow dome with an entrance tunnel; half have a trapdoor and ladder down to a secret lab with a
    //  brewing stand, a chest and two prisoners behind iron bars.
    // =====================================================================================================
    public sealed class IglooStructure : BuildingStructure
    {
        public IglooStructure() { id = "igloo"; spacing = 32; separation = 8; salt = 40039; maxRadiusChunks = 2; bx = 9; bz = 11; yDown = 14; yUp = 8; }

        protected override bool Allowed(WorldGenerator g, int x, int z)
        {
            string b = g.BiomeAtApprox(x, z);
            if (b != "snowy_plains" && b != "snowy_taiga" && b != "snowy_slopes" && b != "ice_spikes") return false;
            return StructureTerrain.DryFlat(g, x, z, 5, 5, out _);
        }

        protected override FramedPiece NewPiece(WorldGenerator g, ref RNG rng) => new Igloo { basement = rng.Chance(0.5f) };

        sealed class Igloo : FramedPiece
        {
            public bool basement;
            protected override void Emit(SB b)
            {
                const int CX = 4, CZ = 6;
                b.FoundationArea(0, 0, 8, 10, 0, "snow_block", 10);
                b.ClearUp(0, 0, 8, 10, 1, 6);
                // dome shell: outer ellipsoid minus inner ellipsoid
                for (int dy = 0; dy <= 4; dy++)
                    for (int dz = -4; dz <= 4; dz++)
                        for (int dx = -4; dx <= 4; dx++)
                        {
                            float outer = (dx * dx + dz * dz) / 12.5f + dy * dy / 18f;
                            float inner = (dx * dx + dz * dz) / 6.3f + dy * dy / 10f;
                            if (outer > 1f) continue;
                            if (dy == 0 || inner > 1f) b.Set(CX + dx, dy, CZ + dz, "snow_block");
                            else b.Air(CX + dx, dy, CZ + dz);
                        }
                // entrance tunnel toward the front
                b.Fill(3, 0, 1, 5, 3, 3, "snow_block");
                b.Clear(4, 1, 1, 4, 2, 4);
                b.Set(3, 2, 2, "ice"); b.Set(5, 2, 2, "ice");
                // furnishings (carpet only on the open floor, never over the shell)
                for (int dz = -3; dz <= 3; dz++)
                    for (int dx = -3; dx <= 3; dx++)
                        if ((dx * dx + dz * dz) / 6.3f + 0.1f <= 1f) b.Set(CX + dx, 1, CZ + dz, "white_carpet");
                b.Bed(6, 1, 6, "red", Dir.North);
                b.Facing(2, 1, 6, "furnace", Dir.East);
                b.Set(2, 1, 7, "crafting_table");
                b.Torch(4, 1, 8, Dir.South, "redstone_torch");
                if (!basement) return;
                // trapdoor hatch over the ladder shaft
                b.Fill(3, -9, 4, 5, -1, 6, "stone_bricks");
                b.Clear(4, -9, 5, 4, 0, 5);
                b.Set(4, 0, 5, "spruce_trapdoor", DirUtil.HorizIndex(Dir.North) | 8);
                b.Air(4, 1, 5);
                // basement laboratory
                b.Fill3(0, -13, 2, 8, -8, 10, "stone_bricks", "mossy_stone_bricks", 0.25f, "cracked_stone_bricks", 0.15f, 7);
                b.Clear(1, -12, 3, 7, -9, 9);
                b.Clear(4, -9, 5, 4, -8, 5);
                b.Fill(4, -12, 6, 4, -9, 6, "stone_bricks");
                b.Ladders(4, -12, -1, 5, Dir.South);
                b.Set(4, 0, 5, "spruce_trapdoor", DirUtil.HorizIndex(Dir.North) | 8);
                // cells with a villager and a zombie villager
                b.Fill(1, -12, 7, 7, -10, 7, "iron_bars");
                b.Fill(4, -12, 8, 4, -9, 9, "stone_bricks");
                b.Mob(2, -12, 8, "villager", 6);
                b.Mob(6, -12, 8, "zombie_villager");
                // lab bench
                b.BrewingStand(2, -12, 4, "weakness");
                b.Set(6, -12, 4, "cauldron", 0);
                b.Chest(6, -12, 5, "igloo", Dir.West);
                b.Set(2, -12, 5, "crafting_table");
                b.Torch(1, -10, 5, Dir.East, "redstone_torch");
                b.Torch(7, -10, 5, Dir.West, "redstone_torch");
                b.Set(3, -12, 3, "red_carpet"); b.Set(5, -12, 3, "red_carpet");
            }
        }
    }

    // =====================================================================================================
    //  Pillager outpost: dark-oak watchtower (birch walls, balcony, loot chest on top), an iron-golem cage
    //  and a small tent camp, guarded by pillagers.
    // =====================================================================================================
    public sealed class PillagerOutpostStructure : BuildingStructure
    {
        public PillagerOutpostStructure() { id = "pillager_outpost"; spacing = 32; separation = 8; salt = 40063; maxRadiusChunks = 3; bx = 9; bz = 9; yDown = 12; yUp = 24; }

        protected override bool Allowed(WorldGenerator g, int x, int z)
        {
            string b = g.BiomeAtApprox(x, z);
            switch (b)
            {
                case "plains": case "sunflower_plains": case "desert": case "savanna": case "savanna_plateau": case "taiga":
                case "snowy_plains": case "snowy_taiga": case "meadow": case "grove": case "snowy_slopes": case "cherry_grove":
                    return StructureTerrain.DryFlat(g, x, z, 6, 8, out _);
            }
            return false;
        }

        protected override FramedPiece NewPiece(WorldGenerator g, ref RNG rng) => new Tower();

        protected override void AddExtras(StructureStart st, WorldGenerator g, FramedPiece main, ref RNG rng)
        {
            // cage and tent on opposite sides of the tower
            int cx = main.ox + main.WorldSizeX / 2, cz = main.oz + main.WorldSizeZ / 2;
            Dir d = DirUtil.Horizontal[rng.Next(4)];
            var o = DirUtil.Offset[(int)d];
            Satellite(st, g, new Cage(), cx + o.x * 11, cz + o.z * 11, 5, 5, rng.Next(4), 6, 8, ref rng);
            var o2 = DirUtil.Offset[(int)DirUtil.Opposite(d)];
            Satellite(st, g, new Tent(), cx + o2.x * 11, cz + o2.z * 11, 5, 6, rng.Next(4), 6, 6, ref rng);
        }

        sealed class Tower : FramedPiece
        {
            protected override void Emit(SB b)
            {
                b.FoundationArea(0, 0, 8, 8, 0, "cobblestone", 12);
                b.ClearUp(-1, -1, 9, 9, 1, 22);
                b.Fill(1, 0, 1, 7, 0, 7, "cobblestone");
                // three storeys of birch walls framed by dark oak logs
                for (int floor = 0; floor < 3; floor++)
                {
                    int y0 = floor * 5;
                    b.Walls(1, y0 + 1, 1, 7, y0 + 4, 7, "birch_planks");
                    b.Fill(1, y0 + 5, 1, 7, y0 + 5, 7, "dark_oak_planks");
                    foreach (var (px, pz) in new[] { (1, 1), (7, 1), (1, 7), (7, 7) }) b.Fill(px, y0 + 1, pz, px, y0 + 4, pz, "dark_oak_log", 0);
                    // arrow slits
                    b.Set(4, y0 + 3, 1, "dark_oak_fence"); b.Set(4, y0 + 3, 7, "dark_oak_fence");
                    b.Set(1, y0 + 3, 4, "dark_oak_fence"); b.Set(7, y0 + 3, 4, "dark_oak_fence");
                    b.Clear(2, y0 + 1, 2, 6, y0 + 4, 6);
                    b.Air(4, y0 + 5, 6);
                }
                b.Clear(4, 1, 1, 4, 2, 1);
                b.Ladders(4, 1, 15, 6, Dir.South);
                // overhanging lookout with railing and roof
                b.Fill(0, 15, 0, 8, 15, 8, "dark_oak_planks");
                b.Air(4, 15, 6);
                b.Ladder(4, 15, 6, Dir.South);
                for (int i = 0; i <= 8; i++)
                {
                    b.Set(i, 16, 0, "dark_oak_fence"); b.Set(i, 16, 8, "dark_oak_fence");
                    b.Set(0, 16, i, "dark_oak_fence"); b.Set(8, 16, i, "dark_oak_fence");
                }
                foreach (var (px, pz) in new[] { (0, 0), (8, 0), (0, 8), (8, 8) }) b.Fill(px, 16, pz, px, 19, pz, "dark_oak_log", 0);
                b.Fill(0, 20, 0, 8, 20, 8, "dark_oak_slab");
                b.Fill(2, 21, 2, 6, 21, 6, "dark_oak_slab");
                b.Chest(2, 16, 2, "pillager_outpost", Dir.East);
                b.Torch(2, 3, 2, Dir.North); b.Torch(6, 8, 6, Dir.South); b.Torch(2, 13, 6, Dir.South);
                // banner-like wool drapes
                b.Fill(0, 12, 4, 0, 14, 4, "white_wool"); b.Set(0, 13, 4, "black_wool");
                b.Fill(8, 12, 4, 8, 14, 4, "white_wool"); b.Set(8, 13, 4, "black_wool");
                // crew
                b.Mob(3, 1, 3, "pillager"); b.Mob(5, 6, 5, "pillager"); b.Mob(3, 11, 5, "pillager");
                b.Mob(5, 16, 5, "pillager"); b.Mob(2, 1, -1, "pillager");
            }
        }

        sealed class Cage : FramedPiece
        {
            protected override void Emit(SB b)
            {
                b.FoundationArea(0, 0, 4, 4, 0, "cobblestone", 8);
                b.ClearUp(0, 0, 4, 4, 1, 6);
                b.Fill(0, 0, 0, 4, 0, 4, "dark_oak_planks");
                b.Walls(0, 1, 0, 4, 3, 4, "dark_oak_fence");
                b.Fill(0, 4, 0, 4, 4, 4, "dark_oak_slab");
                b.Mob(2, 1, 2, "iron_golem");
            }
        }

        sealed class Tent : FramedPiece
        {
            protected override void Emit(SB b)
            {
                b.FoundationArea(0, 0, 4, 5, 0, "dirt", 6);
                b.ClearUp(0, 0, 4, 5, 1, 5);
                // A-frame of wool over dark oak poles
                for (int z = 0; z <= 5; z++)
                {
                    b.Set(0, 1, z, "white_wool"); b.Set(4, 1, z, "white_wool");
                    b.Set(1, 2, z, "white_wool"); b.Set(3, 2, z, "white_wool");
                    b.Set(2, 3, z, "dark_oak_log", 2);
                }
                b.Fill(1, 1, 5, 3, 1, 5, "white_wool");
                b.Set(2, 2, 5, "white_wool");
                b.Set(1, 1, 3, "crafting_table");
                b.Barrel(3, 1, 4, "pillager_outpost");
                b.Set(2, 1, -1, "campfire", 4);
                b.Mob(2, 1, 2, "pillager");
                b.Mob(1, 1, 1, "vindicator");
            }
        }
    }
}
