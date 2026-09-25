using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// End city: a stack of purpur tower rooms on an outer island, crowned by a loot room, with shulkers guarding
    /// the floors and (about half the time) a floating end ship whose treasure room holds the elytra.
    /// Only generated on the outer islands, well past the void gap around the main island.
    /// </summary>
    public sealed class EndCityStructure : StructureBase
    {
        public EndCityStructure() { id = "end_city"; dim = DimensionId.End; spacing = 20; separation = 11; salt = 10387313; maxRadiusChunks = 4; }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
        {
            if (!(g is EndGenerator eg)) return false;
            if (StructureTerrain.DistSq(x, z) < 1000L * 1000L) return false;
            // the whole footprint must sit on solid island, high enough to look like a city on a hill
            int ok = 0;
            for (int dx = -6; dx <= 6; dx += 6)
                for (int dz = -6; dz <= 6; dz += 6)
                    if (eg.SurfaceAt(x + dx, z + dz, out int top) && top >= 56) ok++;
            return ok >= 8;
        }

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            var eg = (EndGenerator)g;
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            int baseY = 0;
            for (int dx = -4; dx <= 4; dx += 4)
                for (int dz = -4; dz <= 4; dz += 4)
                    if (eg.SurfaceAt(x + dx, z + dz, out int t)) baseY = Math.Max(baseY, t);
            baseY += 1;
            var st = NewStart(x, baseY, z);
            int floors = rng.Range(3, 5);
            var tower = new Tower { seed = NextSeed(ref rng), floors = floors };
            tower.PlaceCentered(x, baseY, z, Tower.Size, Tower.Size, rng.Next(4), 6, floors * Tower.FloorH + 12, 1);
            st.pieces.Add(tower);

            // a wing to one side keeps the silhouette from being a single column
            int side = rng.Next(4);
            var dir = DirUtil.FromHorizIndex(side);
            var off = DirUtil.Offset[(int)dir];
            var wing = new Wing { seed = NextSeed(ref rng) };
            int wy = baseY + Tower.FloorH * rng.Range(1, Math.Max(2, floors - 1));
            wing.PlaceCentered(x + off.x * 13, wy, z + off.z * 13, Wing.Size, Wing.Size, rng.Next(4), 2, 10, 1);
            if (!Collides(st.pieces, wing.box, 2)) st.pieces.Add(wing);

            if (rng.NextFloat() < 0.5f)
            {
                // the ship floats beside the city, level with the top floor
                int sdir = (side + 2) & 3;
                var so = DirUtil.Offset[(int)DirUtil.FromHorizIndex(sdir)];
                var ship = new Ship { seed = NextSeed(ref rng) };
                int shipY = baseY + floors * Tower.FloorH + 4;
                ship.PlaceCentered(x + so.x * 26, shipY, z + so.z * 26, Ship.W, Ship.L, sdir, 4, 14, 1);
                if (!Collides(st.pieces, ship.box, 1)) st.pieces.Add(ship);
            }
            return Finish(st, cx, cz);
        }

        const string PB = "purpur_block", PP = "purpur_pillar", PS = "purpur_stairs", PSL = "purpur_slab", ESB = "end_stone_bricks";

        /// <summary>Stacked square rooms with a stair core, end rods at the corners and a loot room at the top.</summary>
        sealed class Tower : FramedPiece
        {
            public const int Size = 11, FloorH = 6;
            public int floors = 3;

            protected override void Emit(SB b)
            {
                const int S = Size - 1;
                // a short end-stone plinth anchors the base to the island
                b.FoundationArea(0, 0, S, S, 0, "end_stone", 6);
                b.Fill(0, 0, 0, S, 0, S, ESB);
                for (int f = 0; f < floors; f++)
                {
                    int y0 = 1 + f * FloorH;
                    int inset = f == floors - 1 ? 1 : 0;
                    b.Walls(inset, y0, inset, S - inset, y0 + FloorH - 1, S - inset, PB);
                    b.Fill(inset + 1, y0, inset + 1, S - inset - 1, y0 + FloorH - 1, S - inset - 1, (ushort)0);
                    b.Fill(inset, y0 + FloorH - 1, inset, S - inset, y0 + FloorH - 1, S - inset, PB);
                    // pillars at the corners, windows mid-wall
                    foreach (var (cx, cz) in new[] { (inset, inset), (S - inset, inset), (inset, S - inset), (S - inset, S - inset) })
                        for (int y = y0; y < y0 + FloorH; y++) b.Log(cx, y, cz, PP, 0);
                    int mid = S / 2;
                    b.Set(mid, y0 + 2, inset, "magenta_stained_glass");
                    b.Set(mid, y0 + 2, S - inset, "magenta_stained_glass");
                    b.Set(inset, y0 + 2, mid, "magenta_stained_glass");
                    b.Set(S - inset, y0 + 2, mid, "magenta_stained_glass");
                    // a winding stair in one corner connects each floor with the next
                    for (int k = 0; k < FloorH - 1; k++)
                    {
                        int sx = inset + 1 + k, sz = inset + 1;
                        if (sx > S - inset - 1) break;
                        b.Stairs(sx, y0 + k, sz, PS, Dir.East);
                        b.Air(sx, y0 + FloorH - 1, sz);
                    }
                    // end rods hang under each ceiling
                    b.Facing6(mid, y0 + FloorH - 2, mid, "end_rod", Dir.Down);
                    // shulkers cling to the walls on every floor but the first
                    if (f > 0) b.Mob(S - inset - 1, y0, S - inset - 1, "shulker");
                    if (f > 1 && b.P(f, y0, 3, 0.6f)) b.Mob(inset + 2, y0, S - inset - 2, "shulker");
                }
                // door on the ground floor
                b.Clear(S / 2 - 1, 1, 0, S / 2 + 1, 3, 0);
                b.Stairs(S / 2, 0, -1, PS, Dir.North);
                // loot room: two treasure chests on the top floor
                int ty = 1 + (floors - 1) * FloorH;
                b.Chest(3, ty, S - 3, "end_city_treasure", Dir.South);
                b.Chest(S - 3, ty, S - 3, "end_city_treasure", Dir.South);
                // roof crown: slabs and rods like antennae
                int roof = floors * FloorH + 1;
                for (int x = 1; x < S; x += 2) { b.Slab(x, roof, 1, PSL); b.Slab(x, roof, S - 1, PSL); }
                for (int z = 1; z < S; z += 2) { b.Slab(1, roof, z, PSL); b.Slab(S - 1, roof, z, PSL); }
                b.Facing6(1, roof, 1, "end_rod", Dir.Up); b.Facing6(S - 1, roof, 1, "end_rod", Dir.Up);
                b.Facing6(1, roof, S - 1, "end_rod", Dir.Up); b.Facing6(S - 1, roof, S - 1, "end_rod", Dir.Up);
            }
        }

        /// <summary>A side house on a pillar: one room, a balcony and a guard.</summary>
        sealed class Wing : FramedPiece
        {
            public const int Size = 7;
            protected override void Emit(SB b)
            {
                const int S = Size - 1;
                // a stilt of purpur pillars down to the island
                for (int y = -1; y > -30; y--)
                {
                    if (b.IsAir(S / 2, y, S / 2) || b.BlockAt(S / 2, y, S / 2).replaceable) b.Log(S / 2, y, S / 2, PP, 0);
                    else break;
                }
                b.Fill(0, 0, 0, S, 0, S, PB);
                b.Walls(0, 1, 0, S, 4, S, PB);
                b.Fill(1, 1, 1, S - 1, 4, S - 1, (ushort)0);
                b.Fill(0, 5, 0, S, 5, S, PB);
                b.Clear(S / 2, 1, 0, S / 2, 3, 0);
                b.Set(1, 2, S / 2, "magenta_stained_glass");
                b.Set(S - 1, 2, S / 2, "magenta_stained_glass");
                b.Facing6(S / 2, 4, S / 2, "end_rod", Dir.Down);
                b.Mob(S / 2, 1, S - 1, "shulker");
                b.Chest(1, 1, S - 1, "end_city_treasure", Dir.South);
            }
        }

        /// <summary>The end ship: a purpur hull with obsidian keel, masts, a dragon head on the prow and the elytra room.</summary>
        sealed class Ship : FramedPiece
        {
            public const int W = 9, L = 23;
            protected override void Emit(SB b)
            {
                int cx = W / 2;
                // hull: widest amidships, tapering to the bow and stern
                for (int z = 0; z < L; z++)
                {
                    int half = z < 3 ? 1 + z : z > L - 5 ? Math.Max(1, (L - 1 - z)) : cx;
                    for (int x = cx - half; x <= cx + half; x++)
                    {
                        b.Set(x, 0, z, x == cx - half || x == cx + half ? PB : "purpur_block");
                        if (x == cx - half || x == cx + half) { b.Set(x, 1, z, PB); b.Set(x, 2, z, PSL); }
                    }
                    b.Set(cx, -1, z, "obsidian");
                }
                // deck house amidships with the treasure room below
                int hz0 = 8, hz1 = 14;
                b.Walls(cx - 3, -3, hz0, cx + 3, -1, hz1, PB);
                b.Fill(cx - 2, -3, hz0 + 1, cx + 2, -1, hz1 - 1, (ushort)0);
                b.Fill(cx - 3, -4, hz0, cx + 3, -4, hz1, "obsidian");
                // the elytra rests in its own chest; the loot chests sit to either side
                var ce = b.Chest(cx, -3, hz1 - 1, null, Dir.North);
                if (ce != null) ce.items[13] = new ItemStack("elytra", 1);
                b.Chest(cx - 2, -3, hz0 + 1, "end_city_treasure", Dir.East);
                b.Chest(cx + 2, -3, hz0 + 1, "end_city_treasure", Dir.West);
                b.BrewingStand(cx, -3, hz0 + 1, "healing", "healing");
                b.Ladders(cx, -2, 0, hz0 - 1, Dir.South);
                b.Air(cx, 0, hz0 - 1);
                // masts with purple "sails" of stained glass
                foreach (int mz in new[] { 6, 16 })
                {
                    for (int y = 1; y <= 10; y++) b.Log(cx, y, mz, PP, 0);
                    for (int x = cx - 3; x <= cx + 3; x++) for (int y = 5; y <= 8; y++) b.Set(x, y, mz + 1, "magenta_stained_glass");
                    b.Facing6(cx, 11, mz, "end_rod", Dir.Up);
                }
                // dragon head figurehead on the bow and a guard on the stern
                b.Set(cx, 1, L - 1, "dragon_head");
                b.Mob(cx, 1, 2, "shulker");
                b.Mob(cx + 1, -3, hz1 - 1, "shulker");
            }
        }
    }
}
