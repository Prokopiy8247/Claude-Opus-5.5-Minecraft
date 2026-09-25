using System;
using System.Collections.Generic;

namespace MCR
{
    // =====================================================================================================
    //  Woodland mansion (compact): three storeys of dark oak with a cobblestone wing, a central hallway on
    //  every floor, stairwells, themed rooms, an evoker's chamber and mansion loot. Dark forests only.
    // =====================================================================================================
    public sealed class WoodlandMansionStructure : BuildingStructure
    {
        public WoodlandMansionStructure() { id = "woodland_mansion"; spacing = 80; separation = 20; salt = 50021; maxRadiusChunks = 3; bx = 37; bz = 21; yDown = 14; yUp = 24; }

        protected override bool Allowed(WorldGenerator g, int x, int z)
        {
            string b = g.BiomeAtApprox(x, z);
            if (b != "dark_forest" && b != "pale_garden") return false;
            return StructureTerrain.DryFlat(g, x, z, 15, 10, out _);
        }

        protected override FramedPiece NewPiece(WorldGenerator g, ref RNG rng) => new Mansion();

        sealed class Mansion : FramedPiece
        {
            const int X = 36, Z = 20, WING = 10, RoofY = 15;
            static readonly int[] RoomX0 = { 1, 9, 17, 25, 33 }, RoomX1 = { 7, 15, 23, 31, 35 };

            protected override void Emit(SB b)
            {
                b.FoundationArea(0, 0, X, Z, 0, "cobblestone", 14);
                b.ClearUp(-1, -1, X + 1, Z + 1, 1, 22);
                for (int f = 0; f < 3; f++) Storey(b, f);
                // front door into the wing's entrance hall
                b.Clear(4, 1, 0, 5, 2, 0);
                b.Door(4, 1, 0, "dark_oak_door", Dir.North);
                b.Door(5, 1, 0, "dark_oak_door", Dir.North, true);
                b.Fill(3, 0, -2, 6, 0, -1, "cobblestone");
                b.StairsRow(3, 0, -2, 6, -2, "cobblestone_stairs", Dir.North);
                b.Fill(3, 1, -1, 3, 3, -1, "dark_oak_log", 0);
                b.Fill(6, 1, -1, 6, 3, -1, "dark_oak_log", 0);
                b.Fill(3, 4, -1, 6, 4, -1, "dark_oak_slab", 0);
                b.Lantern(4, 3, -1, true); b.Lantern(5, 3, -1, true);
                // stairwells: 0 -> 1 at the east end, 1 -> 2 at the west end (2 wide, beside a walkway)
                for (int i = 0; i < 5; i++) b.StairsRow(29 + i, 1 + i, 9, 29 + i, 10, "dark_oak_stairs", Dir.East);
                b.Clear(29, 5, 9, 32, 5, 10);
                for (int i = 0; i < 5; i++) b.StairsRow(7 - i, 6 + i, 9, 7 - i, 10, "dark_oak_stairs", Dir.West);
                b.Clear(4, 10, 9, 7, 10, 10);
                Roof(b);
                Rooms(b);
            }

            void Storey(SB b, int f)
            {
                int y0 = f * 5;
                b.Fill(0, y0, 0, X, y0, Z, f == 0 ? "cobblestone" : "dark_oak_planks");
                b.Fill(1, y0, 1, X - 1, y0, Z - 1, f == 0 ? "birch_planks" : "dark_oak_planks");
                // outer walls: cobblestone wing on the ground floor, dark oak elsewhere, log pillars
                for (int x = 0; x <= X; x++)
                    for (int z = 0; z <= Z; z++)
                    {
                        if (x != 0 && x != X && z != 0 && z != Z) continue;
                        bool pillar = (x % 4 == 0 && (z == 0 || z == Z)) || (z % 4 == 0 && (x == 0 || x == X));
                        string m = pillar ? "dark_oak_log" : (f == 0 && x <= WING ? "cobblestone" : "dark_oak_planks");
                        b.Fill(x, y0 + 1, z, x, y0 + 4, z, m, pillar ? 0 : -1);
                        if (f == 0 && !pillar) b.Set(x, y0 + 1, z, "cobblestone");
                    }
                // interior: hallway z 9..11 between walls at z 8 and z 12, rooms divided every 8 blocks
                b.Clear(1, y0 + 1, 1, X - 1, y0 + 4, Z - 1);
                b.Fill(1, y0 + 1, 8, X - 1, y0 + 4, 8, "dark_oak_planks");
                b.Fill(1, y0 + 1, 12, X - 1, y0 + 4, 12, "dark_oak_planks");
                foreach (int x in new[] { 8, 16, 24, 32 })
                {
                    b.Fill(x, y0 + 1, 1, x, y0 + 4, 7, "dark_oak_planks");
                    b.Fill(x, y0 + 1, 13, x, y0 + 4, Z - 1, "dark_oak_planks");
                }
                for (int i = 0; i < RoomX0.Length; i++)
                {
                    int mx = (RoomX0[i] + RoomX1[i]) / 2;
                    b.Clear(mx, y0 + 1, 8, mx, y0 + 2, 8);
                    b.Clear(mx, y0 + 1, 12, mx, y0 + 2, 12);
                    // windows
                    b.Fill(mx - 1, y0 + 2, 0, mx + 1, y0 + 3, 0, "glass_pane");
                    b.Fill(mx - 1, y0 + 2, Z, mx + 1, y0 + 3, Z, "glass_pane");
                    // room lights on the divider walls (the outer walls carry windows)
                    b.Torch(RoomX0[i], y0 + 3, 4, Dir.East);
                    b.Torch(RoomX0[i], y0 + 3, 16, Dir.East);
                }
                b.Fill(0, y0 + 2, 4, 0, y0 + 3, 4, "glass_pane"); b.Fill(0, y0 + 2, 16, 0, y0 + 3, 16, "glass_pane");
                b.Fill(X, y0 + 2, 4, X, y0 + 3, 4, "glass_pane"); b.Fill(X, y0 + 2, 16, X, y0 + 3, 16, "glass_pane");
                // hallway runner and wall lights
                b.Fill(1, y0 + 1, 11, X - 1, y0 + 1, 11, "red_carpet");
                for (int x = 3; x <= X - 3; x += 6) { b.Torch(x, y0 + 3, 9, Dir.North); b.Torch(x, y0 + 3, 11, Dir.South); }
            }

            void Roof(SB b)
            {
                // hip roof of dark oak stairs rising inward, capped by a planked deck
                for (int i = 0; i < 5; i++)
                {
                    int y = RoofY + i, x0 = -1 + i, x1 = X + 1 - i, z0 = -1 + i, z1 = Z + 1 - i;
                    if (x0 >= x1 || z0 >= z1) break;
                    b.StairsRow(x0, y, z0, x1, z0, "dark_oak_stairs", Dir.North);
                    b.StairsRow(x0, y, z1, x1, z1, "dark_oak_stairs", Dir.South);
                    b.StairsRow(x0, y, z0 + 1, x0, z1 - 1, "dark_oak_stairs", Dir.East);
                    b.StairsRow(x1, y, z0 + 1, x1, z1 - 1, "dark_oak_stairs", Dir.West);
                }
                b.Fill(4, RoofY + 4, 4, X - 4, RoofY + 4, Z - 4, "dark_oak_planks");
                b.Fill(0, RoofY, 0, X, RoofY, Z, "dark_oak_planks");
                // chimneys
                b.Fill(12, RoofY + 1, 4, 12, RoofY + 7, 4, "cobblestone");
                b.Fill(28, RoofY + 1, Z - 4, 28, RoofY + 7, Z - 4, "cobblestone");
            }

            void Rooms(SB b)
            {
                // room themes are chosen per slot from the piece seed so every chunk agrees
                for (int f = 0; f < 3; f++)
                    for (int i = 0; i < RoomX0.Length; i++)
                        for (int side = 0; side < 2; side++)
                        {
                            int x0 = RoomX0[i], x1 = RoomX1[i];
                            int z0 = side == 0 ? 1 : 13, z1 = side == 0 ? 7 : Z - 1;
                            int y = f * 5 + 1;
                            if (f == 0 && i == 0 && side == 0) { Hall(b, x0, y, z0, x1, z1); continue; }
                            if (f == 2 && i == 2) { Chamber(b, x0, y, z0, x1, z1, side); continue; }
                            int kind = (int)(b.R(i, f, side, 70) * 8);
                            if (f == 1 && kind < 3) kind = 7;   // mostly bedrooms upstairs
                            Furnish(b, kind, x0, y, z0, x1, z1, side, f);
                        }
            }

            static void Hall(SB b, int x0, int y, int z0, int x1, int z1)
            {
                b.Fill(x0 + 1, y, z0 + 1, x1 - 1, y, z1 - 1, "red_carpet");
                b.Lantern((x0 + x1) / 2, y + 3, (z0 + z1) / 2, true);
                b.Set(x0, y, z0 + 3, "flower_pot");
                b.Mob((x0 + x1) / 2, y, (z0 + z1) / 2, "vindicator");
            }

            static void Chamber(SB b, int x0, int y, int z0, int x1, int z1, int side)
            {
                // the evoker's study: altar, bookshelves and the best chest
                b.Fill(x0, y, z0, x1, y, z1, "purple_carpet");
                b.Fill(x0, y, side == 0 ? z0 : z1, x1, y + 2, side == 0 ? z0 : z1, "bookshelf");
                int cx = (x0 + x1) / 2, cz = (z0 + z1) / 2;
                b.Set(cx, y, cz, "enchanting_table");
                b.Chest(x0, y, cz, "woodland_mansion", Dir.East);
                b.Chest(x1, y, cz, "woodland_mansion", Dir.West);
                b.Mob(cx + 1, y, cz, side == 0 ? "evoker" : "vindicator");
            }

            static void Furnish(SB b, int kind, int x0, int y, int z0, int x1, int z1, int side, int f)
            {
                int cx = (x0 + x1) / 2, cz = (z0 + z1) / 2;
                int back = side == 0 ? z0 : z1;                 // wall opposite the hallway door
                Dir face = side == 0 ? Dir.North : Dir.South;   // facing into the room from the back wall
                switch (kind)
                {
                    case 0: // library
                        b.Fill(x0, y, back, x1, y + 2, back, "bookshelf");
                        b.Fill(x0, y, cz, x0 + 1, y + 1, cz, "bookshelf");
                        b.Chest(x1, y, cz, "woodland_mansion", Dir.West);
                        b.Lectern(cx, y, cz, face, true);
                        break;
                    case 1: // dining room
                        b.Fill(x0 + 1, y, cz, x1 - 1, y, cz, "dark_oak_fence");
                        b.Fill(x0 + 1, y + 1, cz, x1 - 1, y + 1, cz, "white_carpet");
                        for (int x = x0 + 1; x <= x1 - 1; x += 2) { b.Stairs(x, y, cz - 1, "dark_oak_stairs", Dir.South); b.Stairs(x, y, cz + 1, "dark_oak_stairs", Dir.North); }
                        b.Mob(cx, y, cz - 2, "vindicator");
                        break;
                    case 2: // kitchen / storage
                        b.Facing(x0, y, back, "smoker", face);
                        b.Barrel(x0 + 1, y, back, "woodland_mansion");
                        b.Barrel(x0 + 2, y, back, "woodland_mansion");
                        b.Set(x1, y, back, "crafting_table");
                        b.Set(x1 - 1, y, back, "cauldron", 3);
                        break;
                    case 3: // jail with an allay prisoner
                        b.Fill(x0 + 1, y, cz, x1 - 1, y + 2, cz, "iron_bars");
                        b.Mob(cx, y, back == z0 ? z0 + 1 : z1 - 1, "allay");
                        b.Chest(x0, y, back, "woodland_mansion", face);
                        break;
                    case 4: // indoor garden
                        for (int x = x0 + 1; x <= x1 - 1; x += 2) { b.Set(x, y, back, "flower_pot"); b.Set(x, y - 1, back, "moss_block"); }
                        b.Set(cx, y, cz, "azalea");
                        b.Set(cx, y - 1, cz, "moss_block");
                        break;
                    case 5: // map room
                        b.Set(cx, y, cz, "cartography_table");
                        b.Chest(x0, y, back, "woodland_mansion", face);
                        b.Fill(x1, y, z0, x1, y + 1, z1, "bookshelf");
                        break;
                    case 6: // chapel
                        b.Fill(x0 + 1, y, back, x1 - 1, y, back, "dark_oak_planks");
                        for (int x = x0 + 1; x <= x1 - 1; x += 2) b.Set(x, y + 1, back, "white_candle", 2);
                        b.Mob(cx, y, cz, "vindicator");
                        break;
                    default: // bedroom
                        b.Bed(x0 + 1, y, cz, "gray", side == 0 ? Dir.South : Dir.North);
                        b.Bed(x1 - 1, y, cz, "gray", side == 0 ? Dir.South : Dir.North);
                        b.Chest(cx, y, back, "woodland_mansion", face);
                        b.Fill(x0 + 1, y, cz + (side == 0 ? 1 : -1), x1 - 1, y, cz + (side == 0 ? 1 : -1), "light_gray_carpet");
                        if (f == 1) b.Mob(cx, y, cz, "vindicator");
                        break;
                }
            }
        }
    }

    // =====================================================================================================
    //  Ancient city (compact): a deep-dark cavern with a deepslate plaza, a reinforced-deepslate arch at its
    //  heart, ruined houses and towers, sculk growth with shriekers and sensors, soul lights and chests.
    // =====================================================================================================
    public sealed class AncientCityStructure : StructureBase
    {
        const int S = 64;
        public AncientCityStructure() { id = "ancient_city"; spacing = 64; separation = 16; salt = 50033; maxRadiusChunks = 4; }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
            => g is OverworldGenerator og && og.StructureDeepDark(x, -40, z);

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            int y = rng.Range(-48, -40);
            var st = NewStart(x, y + 1, z);
            var p = new City { seed = NextSeed(ref rng) };
            p.Place(x - S / 2, y, z - S / 2, S, S, rng.Next(4), 4, 22, 0);
            st.pieces.Add(p);
            return Finish(st, cx, cz);
        }

        sealed class City : FramedPiece
        {
            const int C = S / 2;

            protected override void Emit(SB b)
            {
                // cavern with a vaulted ceiling, higher in the middle
                for (int dz = 1; dz < S - 1; dz++)
                    for (int dx = 1; dx < S - 1; dx++)
                    {
                        if (!b.Owns(dx, 0, dz)) continue;
                        float e = Math.Max(Math.Abs(dx - C + 0.5f), Math.Abs(dz - C + 0.5f)) / C;
                        int top = 6 + (int)(14 * (1f - e * e)) + (int)(b.R(dx, 0, dz, 1) * 3);
                        b.Clear(dx, 1, dz, dx, top, dz);
                        b.Set(dx, 0, dz, b.P(dx, 0, dz, 0.3f, 2) ? "sculk" : (b.P(dx, 0, dz, 0.5f, 3) ? "deepslate_tiles" : "deepslate_bricks"));
                        if (b.BlockAt(dx, -1, dz).isAir || b.BlockAt(dx, -1, dz).isLiquid) b.Set(dx, -1, dz, "cobbled_deepslate");
                    }
                Plaza(b);
                Arch(b);
                // houses and towers on a ring of lots around the plaza
                int lot = 0;
                for (int gz = 0; gz < 4; gz++)
                    for (int gx = 0; gx < 4; gx++)
                    {
                        if (gx >= 1 && gx <= 2 && gz >= 1 && gz <= 2) continue;   // plaza
                        int lx = 3 + gx * 15, lz = 3 + gz * 15;
                        float r = b.R(gx, 7, gz, 4);
                        if (r < 0.15f) continue;
                        if (r < 0.55f) House(b, lx, lz, lot); else Tower(b, lx, lz, lot);
                        lot++;
                    }
                Sculk(b);
            }

            void Plaza(SB b)
            {
                // raised plaza of polished deepslate with steps on four sides
                b.Fill(C - 13, 1, C - 13, C + 12, 1, C + 12, "polished_deepslate");
                b.Fill(C - 12, 1, C - 12, C + 11, 1, C + 11, "deepslate_tiles");
                for (int i = -2; i <= 1; i++)
                {
                    b.Stairs(C + i, 1, C - 14, "deepslate_tile_stairs", Dir.North);
                    b.Stairs(C + i, 1, C + 13, "deepslate_tile_stairs", Dir.South);
                    b.Stairs(C - 14, 1, C + i, "deepslate_tile_stairs", Dir.East);
                    b.Stairs(C + 13, 1, C + i, "deepslate_tile_stairs", Dir.West);
                }
                // soul-fire braziers at the plaza corners
                foreach (var (px, pz) in new[] { (C - 11, C - 11), (C + 10, C - 11), (C - 11, C + 10), (C + 10, C + 10) })
                {
                    b.Set(px, 2, pz, "soul_sand");
                    b.Set(px, 3, pz, "soul_fire");
                    b.Fill(px - 1, 2, pz, px - 1, 3, pz, "deepslate_brick_wall");
                    b.Fill(px + 1, 2, pz, px + 1, 3, pz, "deepslate_brick_wall");
                }
                // pillars with hanging soul lanterns
                for (int i = -9; i <= 8; i += 6)
                {
                    foreach (int pz in new[] { C - 9, C + 8 })
                    {
                        b.Fill(C + i, 2, pz, C + i, 7, pz, "deepslate_bricks");
                        b.Set(C + i, 8, pz, "chiseled_deepslate");
                        b.Lantern(C + i, 6, pz + (pz < C ? 1 : -1), true, "soul_lantern");
                        b.Set(C + i, 7, pz + (pz < C ? 1 : -1), "deepslate_bricks");
                    }
                }
            }

            void Arch(SB b)
            {
                // the "portal" arch: reinforced deepslate frame on a deepslate-brick stage
                int x0 = C - 8, x1 = C + 7, z0 = C - 1, z1 = C + 1;
                b.Fill(x0 - 1, 2, z0 - 2, x1 + 1, 2, z1 + 2, "deepslate_bricks");
                b.Fill(x0, 3, z0, x0 + 2, 14, z1, "reinforced_deepslate");
                b.Fill(x1 - 2, 3, z0, x1, 14, z1, "reinforced_deepslate");
                b.Fill(x0, 12, z0, x1, 14, z1, "reinforced_deepslate");
                b.Fill(x0 + 3, 3, z0, x1 - 3, 11, z1, 0);
                // decorative deepslate cladding on the frame
                b.Fill(x0 - 1, 3, z0 - 1, x0 - 1, 10, z1 + 1, "deepslate_tile_wall");
                b.Fill(x1 + 1, 3, z0 - 1, x1 + 1, 10, z1 + 1, "deepslate_tile_wall");
                b.Fill(x0 - 1, 15, z0, x1 + 1, 15, z1, "deepslate_tile_slab");
                // treasure beneath the arch, guarded by shriekers
                b.Chest(C - 3, 3, C - 3, "ancient_city", Dir.South);
                b.Chest(C + 2, 3, C - 3, "ancient_city", Dir.South);
                b.Chest(C - 3, 3, C + 3, "ancient_city", Dir.North);
                b.Set(C - 1, 3, C - 3, "sculk_shrieker"); b.Set(C, 3, C + 3, "sculk_shrieker");
                b.Set(C + 2, 3, C + 3, "sculk_catalyst");
                b.Set(C - 5, 3, C, "candle", 3); b.Set(C + 4, 3, C, "candle", 1);
            }

            void House(SB b, int lx, int lz, int lot)
            {
                int w = 9 + (int)(b.R(lx, 0, lz, 11) * 3), d = 9 + (int)(b.R(lz, 0, lx, 12) * 3);
                int h = 5;
                b.Walls(lx, 1, lz, lx + w - 1, h, lz + d - 1, "deepslate_bricks");
                b.Fill(lx + 1, 1, lz + 1, lx + w - 2, h - 1, lz + d - 2, 0);
                b.Fill(lx, h + 1, lz, lx + w - 1, h + 1, lz + d - 1, "deepslate_tile_slab");
                // ruin: knock holes in the walls and roof
                for (int x = lx; x < lx + w; x++)
                    for (int z = lz; z < lz + d; z++)
                        for (int y = 2; y <= h + 1; y++)
                            if (b.P(x, y, z, 0.18f, 13)) b.Air(x, y, z);
                // doorway toward the plaza
                int doorX = lx + w / 2, doorZ = lz < C ? lz + d - 1 : lz;
                b.Clear(doorX - 1, 1, doorZ, doorX + 1, 3, doorZ);
                b.Set(lx + 1, 1, lz + 1, "cracked_deepslate_bricks");
                b.Set(lx + w - 2, 1, lz + d - 2, "candle", 2);
                b.Lantern(lx + w / 2, h - 1, lz + d / 2, false, "soul_lantern");
                if (b.P(lot, 0, 0, 0.6f, 14)) b.Chest(lx + 1, 1, lz + d / 2, "ancient_city", Dir.East);
                if (b.P(lot, 0, 0, 0.5f, 15)) b.Set(lx + w - 2, 1, lz + 1, "sculk_sensor");
                b.Fill(lx + 2, 1, lz + 2, lx + w - 3, 1, lz + d - 3, "gray_carpet");
            }

            void Tower(SB b, int lx, int lz, int lot)
            {
                int top = 10 + (int)(b.R(lx, 1, lz, 21) * 6);
                b.Fill(lx + 2, 1, lz + 2, lx + 8, 1, lz + 8, "deepslate_tiles");
                b.Walls(lx + 3, 1, lz + 3, lx + 7, top, lz + 7, "deepslate_tiles");
                b.Fill(lx + 4, 1, lz + 4, lx + 6, top - 1, lz + 6, 0);
                b.Fill(lx + 3, top + 1, lz + 3, lx + 7, top + 1, lz + 7, "polished_deepslate");
                b.Ladders(lx + 5, 1, top, lz + 6, Dir.South);
                b.Clear(lx + 5, 1, lz + 3, lx + 5, 2, lz + 3);
                b.Fill(lx + 3, top - 3, lz + 3, lx + 3, top - 2, lz + 3, "deepslate_brick_wall");
                b.Lantern(lx + 5, top - 1, lz + 5, true, "soul_lantern");
                if (b.P(lot, 1, 0, 0.5f, 22)) b.Chest(lx + 4, top + 2, lz + 4, "ancient_city", Dir.South);
                b.Set(lx + 6, top + 2, lz + 6, "sculk_shrieker");
                b.Set(lx + 6, top + 1, lz + 6, "sculk");
            }

            void Sculk(SB b)
            {
                // sculk veins on walls, sensors and shriekers scattered over the floor
                for (int dz = 2; dz < S - 2; dz++)
                    for (int dx = 2; dx < S - 2; dx++)
                    {
                        if (!b.Owns(dx, 0, dz)) continue;
                        if (b.IsAir(dx, 1, dz) && b.BlockAt(dx, 0, dz).id == "sculk")
                        {
                            float r = b.R(dx, 1, dz, 31);
                            if (r < 0.012f) b.Set(dx, 1, dz, "sculk_shrieker");
                            else if (r < 0.03f) b.Set(dx, 1, dz, "sculk_sensor");
                        }
                        for (int y = 2; y <= 8; y++)
                        {
                            if (!b.IsAir(dx, y, dz)) continue;
                            if (!b.P(dx, y, dz, 0.08f, 32)) continue;
                            // attach a vein to any solid neighbour side
                            for (int i = 0; i < 4; i++)
                            {
                                var o = DirUtil.Offset[(int)DirUtil.Horizontal[i]];
                                if (b.BlockAt(dx + o.x, y, dz + o.z).opaqueCube) { b.Set(dx, y, dz, "sculk_vein", 1 << i); break; }
                            }
                        }
                    }
            }
        }
    }
}
