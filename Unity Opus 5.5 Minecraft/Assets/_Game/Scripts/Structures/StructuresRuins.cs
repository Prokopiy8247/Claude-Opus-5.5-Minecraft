using System;
using System.Collections.Generic;

namespace MCR
{
    // =====================================================================================================
    //  Ruined portal: a broken vertical obsidian portal frame (or a flat one), crying obsidian, netherrack
    //  spill, gold blocks and a loot chest. Occasionally sunk into the ground with a cave of netherrack.
    // =====================================================================================================
    public sealed class RuinedPortalStructure : StructureBase
    {
        public RuinedPortalStructure() { id = "ruined_portal"; spacing = 26; separation = 6; salt = 40123; maxRadiusChunks = 2; }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
        {
            if (!(g is OverworldGenerator)) return false;
            var bio = StructureTerrain.BiomeAt(g, x, z);
            if (bio.isOcean || bio.isRiver || bio.isCave) return false;
            int h = StructureTerrain.Ground(g, x, z);
            return h > g.world.minY + 30 && StructureTerrain.Relief(g, x - 4, z - 4, x + 4, z + 4) <= 8;
        }

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            int rot = rng.Next(4);
            int kind = rng.Next(4);          // 0..2 standing, 3 flat
            int drop = rng.Chance(0.25f) ? rng.Range(4, 8) : 0;
            int fy = StructureTerrain.FloorLevel(g, x - 3, z - 3, x + 3, z + 3);
            var st = NewStart(x, fy, z);
            var p = new Portal { kind = kind, dropped = drop, seed = NextSeed(ref rng) };
            p.Place(x - 5, fy + (drop > 0 ? 0 : 0), z - 5, 11, 7, rot, 12 + drop, 8, 2);
            st.pieces.Add(p);
            return Finish(st, cx, cz);
        }

        sealed class Portal : FramedPiece
        {
            public int kind, dropped;   // dropped > 0: the frame sits in a pit that deep

            protected override void Emit(SB b)
            {
                int X = sx - 1, Z = sz - 1;
                int pit = dropped;
                int baseY = pit > 0 ? -pit : 0;
                // ground work: clear the site, fill a pit if sunk
                b.FoundationArea(0, 0, X, Z, baseY, "netherrack", 16);
                b.ClearUp(-1, -1, X + 1, Z + 1, baseY + 1, 12);
                if (pit > 0)
                {
                    // netherrack pit walls
                    for (int y = baseY; y < 0; y++)
                    {
                        for (int x = 0; x <= X; x++) { b.Set(x, y, 0, "netherrack"); b.Set(x, y, Z, "netherrack"); }
                        for (int z = 1; z < Z; z++) { b.Set(0, y, z, "netherrack"); b.Set(X, y, z, "netherrack"); }
                    }
                    b.Fill(0, baseY, 0, X, baseY, Z, "netherrack");
                }
                else
                {
                    b.Fill(2, 0, 1, 8, 0, 5, "netherrack");
                }
                // the frame: standing (5 tall x 4 wide) or flat
                int y0 = baseY + 1;
                if (kind == 3)
                {
                    // flat: a ring of obsidian lying on the ground
                    for (int i = 0; i < 4; i++)
                        for (int j = 0; j < 4; j++)
                        {
                            bool edge = i == 0 || i == 3 || j == 0 || j == 3;
                            if (!edge) continue;
                            if (b.P(i, j, 0, 0.15f, 21)) continue;
                            b.Set(3 + i, y0, 1 + j, b.P(i, j, 0, 0.4f, 22) ? "crying_obsidian" : "obsidian");
                        }
                }
                else
                {
                    for (int i = 0; i < 4; i++)
                        for (int j = 0; j < 5; j++)
                        {
                            bool edge = i == 0 || i == 3 || j == 0 || j == 4;
                            if (!edge) continue;
                            // kind controls how much of the frame survives
                            float missing = kind == 0 ? 0.12f : kind == 1 ? 0.28f : 0.45f;
                            if (b.P(i, j, 0, missing, 23)) continue;
                            b.Set(3 + i, y0 + j, 1, b.P(i, j, 0, 0.25f, 24) ? "crying_obsidian" : "obsidian");
                        }
                }
                // scattered obsidian, magma and netherrack spill
                for (int i = 0; i < 14; i++)
                {
                    int x = (int)(b.R(i, 0, 0, 31) * (X + 5)) - 2, z = (int)(b.R(0, 0, i, 32) * (Z + 5)) - 2;
                    if (x >= 2 && x <= 8 && z >= 1 && z <= 5) continue;
                    if (!b.Owns(x, y0, z)) continue;
                    if (b.P(x, 0, z, 0.35f, 33)) b.Set(x, y0 - 1, z, "netherrack");
                    else if (b.P(x, 0, z, 0.2f, 34)) b.Set(x, y0 - 1, z, "obsidian");
                    else if (b.P(x, 0, z, 0.15f, 35)) b.Set(x, y0 - 1, z, "magma_block");
                    else if (b.P(x, 0, z, 0.1f, 36)) b.Set(x, y0 - 1, z, "crying_obsidian");
                }
                // gold blocks and the chest
                b.Set(3, y0 - 1, 2, "gold_block");
                b.Set(6, y0 - 1, 2, "gold_block");
                b.Set(5, y0 - 1, 4, "gold_block");
                b.Chest(4, y0, 5, "ruined_portal", Dir.South);
                // fire on the netherrack
                if (b.P(0, 0, 0, 0.6f, 41)) b.Set(4, y0, 3, "fire", 0);
                if (b.P(0, 1, 0, 0.4f, 42)) b.Set(2, y0, 4, "fire", 0);
                b.Mob(4, y0, 4, "zombified_piglin");
            }
        }
    }

    // =====================================================================================================
    //  Trail ruins: a cluster of mud-brick and terracotta house foundations buried in the ground, with a
    //  tower, gravel rubble, and loot in the rooms.
    // =====================================================================================================
    public sealed class TrailRuinsStructure : StructureBase
    {
        public TrailRuinsStructure() { id = "trail_ruins"; spacing = 20; separation = 6; salt = 40139; maxRadiusChunks = 3; }

        public override bool CanSpawn(WorldGenerator g, int x, int z, ref RNG rng)
        {
            if (!(g is OverworldGenerator)) return false;
            var bio = StructureTerrain.BiomeAt(g, x, z);
            if (bio.isOcean || bio.isRiver || bio.isCave) return false;
            int h = StructureTerrain.Ground(g, x, z);
            if (h <= g.world.seaLevel || h > 130) return false;
            return StructureTerrain.Relief(g, x - 8, z - 8, x + 8, z + 8) <= 8;
        }

        public override StructureStart Create(WorldGenerator g, int cx, int cz, ref RNG rng)
        {
            int x = (cx << 4) + 8, z = (cz << 4) + 8;
            var st = NewStart(x, StructureTerrain.Ground(g, x, z) - 2, z);
            var used = new List<BBox>();
            int rooms = rng.Range(3, 5);
            for (int i = 0; i < rooms; i++)
            {
                int r = rng.Range(4, 6);
                int px = x + (i == 0 ? 0 : rng.Range(-22, 22)), pz = z + (i == 0 ? 0 : rng.Range(-22, 22));
                int s = r * 2 + 1;
                int fy = StructureTerrain.FloorLevel(g, px - r, pz - r, px + r, pz + r) - 2;
                var bb = new BBox(px - r - 1, fy - 4, pz - r - 1, px + r + 2, fy + 12, pz + r + 2);
                bool clash = false;
                foreach (var q in used) if (q.Intersects(bb)) { clash = true; break; }
                if (clash) continue;
                used.Add(bb);
                var p = new RuinRoom { tower = i == 0 && rng.Chance(0.5f), seed = NextSeed(ref rng) };
                p.Place(px - r, fy, pz - r, s, s, rng.Next(4), 6, 16, 2);
                st.pieces.Add(p);
            }
            if (st.pieces.Count < 2) return null;
            return Finish(st, cx, cz);
        }

        sealed class RuinRoom : FramedPiece
        {
            public bool tower;

            protected override void Emit(SB b)
            {
                int X = sx - 1, Z = sz - 1;
                // buried: the floor is below the terrain, the walls barely poke out
                b.ClearUp(-1, -1, X + 1, Z + 1, 1, 4);
                b.Fill3(0, 0, 0, X, 0, Z, "mud_bricks", "packed_mud", 0.25f, "terracotta", 0.1f, 1);
                for (int x = 0; x <= X; x++)
                    for (int z = 0; z <= Z; z++)
                    {
                        bool edge = x == 0 || x == X || z == 0 || z == Z;
                        if (!edge) { b.Air(x, 1, z); b.Air(x, 2, z); continue; }
                        // walls survive to random heights; gravel where they collapsed
                        float r = b.R(x, 0, z, 2);
                        int h = r < 0.25f ? 0 : r < 0.55f ? 1 : r < 0.8f ? 2 : 3;
                        for (int y = 1; y <= h; y++)
                            b.Set(x, y, z, b.P(x, y, z, 0.3f, 3) ? "gravel" : (b.P(x, y, z, 0.4f, 4) ? "packed_mud" : "mud_bricks"));
                        b.Air(x, h + 1, z); b.Air(x, h + 2, z); b.Air(x, h + 3, z);
                    }
                // doorway and interior gravel
                int dx = X / 2;
                b.Clear(dx, 1, 0, dx, 2, 0);
                for (int x = 1; x < X; x++)
                    for (int z = 1; z < Z; z++)
                        if (b.P(x, 1, z, 0.12f, 5)) b.Set(x, 1, z, b.P(x, 1, z, 0.5f, 6) ? "gravel" : "mud_bricks");
                // terracotta floor accents
                for (int x = 0; x <= X; x++)
                    for (int z = 0; z <= Z; z++)
                        if (b.P(x, 0, z, 0.12f, 7)) b.Set(x, 0, z, b.P(x, 0, z, 0.4f, 8) ? "white_terracotta" : "yellow_terracotta");
                // loot
                b.Chest(1, 1, Z - 1, "trail_ruins", Dir.South);
                b.Barrel(X - 1, 1, 1, "trail_ruins");
                b.Set(X - 1, 1, Z - 1, "flower_pot");
                b.Set(1, 1, 1, "dead_bush");
                if (tower)
                {
                    // a ruined tower: a hollow mud-brick column at one corner
                    b.Fill(X - 2, 0, Z - 2, X, 12, Z, "mud_bricks");
                    b.Fill(X - 1, 1, Z - 1, X - 1, 11, Z - 1, 0);
                    b.Fill(X - 2, 13, Z - 2, X, 13, Z, "mud_bricks");
                    for (int y = 2; y <= 10; y += 3) b.Set(X, y, Z - 1, "copper_grate");
                    b.Ladders(X - 1, 1, 11, Z - 1, Dir.West);
                    b.Chest(X - 1, 1, Z - 1, "trail_ruins", Dir.East);
                    b.Set(X - 3, 1, Z - 3, "mud_brick_slab", 0);
                }
                b.Mob(1, 1, 1, b.P(0, 0, 0, 0.5f, 9) ? "skeleton" : "zombie");
            }
        }
    }
}
