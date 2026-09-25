using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Nether: 128-high cavern world with bedrock floor/ceiling, lava sea at 31 and five biomes.</summary>
    public sealed class NetherGenerator : WorldGenerator
    {
        readonly OctaveNoise cave, bA, bB, surf, pillarN, deltaN;
        public const int LavaLevel = 31;
        int wastes, crimson, warped, soul, deltas;
        readonly ushort NETHERRACK, LAVA, BEDROCK, SOUL_SAND, SOUL_SOIL, BASALT, BLACKSTONE, MAGMA, GRAVEL, CRIMSON_NYLIUM, WARPED_NYLIUM, GLOWSTONE, QUARTZ, NGOLD, DEBRIS, BONE;

        public NetherGenerator(World w) : base(w)
        {
            cave = new OctaveNoise(seed, 101, 1.0 / 64.0, 1.0, 0.5, 0.25);
            bA = new OctaveNoise(seed, 102, 1.0 / 240.0, 1.0, 0.4);
            bB = new OctaveNoise(seed, 103, 1.0 / 240.0, 1.0, 0.4);
            surf = new OctaveNoise(seed, 104, 1.0 / 16.0, 1.0, 0.5);
            pillarN = new OctaveNoise(seed, 105, 1.0 / 6.0, 1.0);
            deltaN = new OctaveNoise(seed, 106, 1.0 / 12.0, 1.0, 0.5);
            NETHERRACK = S("netherrack"); LAVA = Blocks.Lava.baseState; BEDROCK = S("bedrock"); SOUL_SAND = S("soul_sand"); SOUL_SOIL = S("soul_soil");
            BASALT = S("basalt"); BLACKSTONE = S("blackstone"); MAGMA = S("magma_block"); GRAVEL = S("gravel");
            CRIMSON_NYLIUM = S("crimson_nylium"); WARPED_NYLIUM = S("warped_nylium"); GLOWSTONE = S("glowstone");
            QUARTZ = S("nether_quartz_ore"); NGOLD = S("nether_gold_ore"); DEBRIS = S("ancient_debris"); BONE = S("bone_block");
            wastes = Biome.Id("nether_wastes"); crimson = Biome.Id("crimson_forest"); warped = Biome.Id("warped_forest"); soul = Biome.Id("soul_sand_valley"); deltas = Biome.Id("basalt_deltas");
        }

        public int BiomeAt(int x, int z)
        {
            float a = (float)bA.Sample(x, z) * 1.8f, b = (float)bB.Sample(x, z) * 1.8f;
            if (a > 0.3f && b > -0.1f) return crimson;
            if (a < -0.3f && b > 0.0f) return warped;
            if (b < -0.35f && a > -0.1f) return soul;
            if (b < -0.3f && a <= -0.1f) return deltas;
            return wastes;
        }
        public override string BiomeAtApprox(int x, int z) => Biome.Get(BiomeAt(x, z)).key;
        public override int ApproxSurface(int x, int z) => 40;

        float Density(int x, int y, int z)
        {
            float n = (float)cave.Sample(x, y * 1.7, z) * 1.6f;
            float f = Mathf.Max(0, (26 - y) / 26f) * 1.7f + Mathf.Max(0, (y - 96) / 31f) * 2.2f - 0.25f;
            return n + f;
        }

        public override void GenerateTerrain(Chunk c)
        {
            int x0 = c.cx << 4, z0 = c.cz << 4;
            int H = world.height;
            // density grid 5 x (H/4+1) x 5
            int NY = H / 4 + 1;
            var d = new float[25 * NY];
            for (int j = 0; j < 5; j++) for (int i = 0; i < 5; i++) for (int k = 0; k < NY; k++) d[(k * 5 + j) * 5 + i] = Density(x0 + i * 4, k * 4, z0 + j * 4);
            var rng = new RNG(seed, c.cx, c.cz, 111);
            for (int lz = 0; lz < 16; lz++)
                for (int lx = 0; lx < 16; lx++)
                {
                    int biome = BiomeAt(x0 + lx, z0 + lz);
                    c.biomes2D[lz * 16 + lx] = (byte)biome;
                    int gi = lx >> 2, gj = lz >> 2; float tx = (lx & 3) / 4f, tz = (lz & 3) / 4f;
                    for (int y = 0; y < H; y++)
                    {
                        int k = y >> 2; float ty = (y & 3) / 4f;
                        if (k >= NY - 1) break;
                        float v000 = d[(k * 5 + gj) * 5 + gi], v100 = d[(k * 5 + gj) * 5 + gi + 1], v010 = d[(k * 5 + gj + 1) * 5 + gi], v110 = d[(k * 5 + gj + 1) * 5 + gi + 1];
                        float v001 = d[((k + 1) * 5 + gj) * 5 + gi], v101 = d[((k + 1) * 5 + gj) * 5 + gi + 1], v011 = d[((k + 1) * 5 + gj + 1) * 5 + gi], v111 = d[((k + 1) * 5 + gj + 1) * 5 + gi + 1];
                        float lo = Mathf.Lerp(Mathf.Lerp(v000, v100, tx), Mathf.Lerp(v010, v110, tx), tz);
                        float hi = Mathf.Lerp(Mathf.Lerp(v001, v101, tx), Mathf.Lerp(v011, v111, tx), tz);
                        float v = Mathf.Lerp(lo, hi, ty);
                        ushort s = 0;
                        if (v > 0) s = NETHERRACK;
                        else if (y <= LavaLevel) s = LAVA;
                        if (y < 5 && (y == 0 || rng.Next(5) >= y)) s = BEDROCK;
                        if (y > H - 6 && (y == H - 1 || rng.Next(5) >= H - 1 - y)) s = BEDROCK;
                        if (s != 0) c.SetRaw(lx, y, lz, s);
                    }
                    Surface(c, lx, lz, x0 + lx, z0 + lz, biome, ref rng);
                }
            for (int i = 0; i < 256; i++)
            {
                int lx = i & 15, lz = i >> 4;
                int y = 100; while (y > 5 && c.Get(lx, y, lz) != 0) y--; // find cavern
                while (y > 5 && (c.Get(lx, y, lz) == 0)) y--;
                c.genTopY[i] = (short)y; c.genTop[i] = c.Get(lx, y, lz); c.genWater[i] = 0;
            }
        }

        void Surface(Chunk c, int lx, int lz, int wx, int wz, int biome, ref RNG rng)
        {
            float sn = (float)surf.Sample(wx, wz);
            for (int y = 6; y < world.height - 6; y++)
            {
                ushort s = c.Get(lx, y, lz);
                if (s != NETHERRACK) continue;
                bool airAbove = c.Get(lx, y + 1, lz) == 0;
                bool floor = airAbove;
                if (biome == crimson || biome == warped)
                {
                    if (floor) c.SetRaw(lx, y, lz, biome == crimson ? CRIMSON_NYLIUM : WARPED_NYLIUM);
                }
                else if (biome == soul)
                {
                    if (floor || c.Get(lx, y + 2, lz) == 0 && c.Get(lx, y + 1, lz) != 0 && y < 60)
                        c.SetRaw(lx, y, lz, sn > 0 ? SOUL_SAND : SOUL_SOIL);
                    if (floor && c.Get(lx, y - 1, lz) == NETHERRACK) c.SetRaw(lx, y - 1, lz, SOUL_SOIL);
                }
                else if (biome == deltas)
                {
                    float dn = (float)deltaN.Sample(wx, y, wz);
                    c.SetRaw(lx, y, lz, dn > 0.1f ? BASALT : BLACKSTONE);
                    if (floor && dn < -0.35f && y > LavaLevel - 2 && y < LavaLevel + 6) c.SetRaw(lx, y, lz, MAGMA);
                }
                else
                {
                    if (floor && y >= LavaLevel - 1 && y <= LavaLevel + 2 && sn > 0.45f) c.SetRaw(lx, y, lz, sn > 0.7f ? SOUL_SAND : GRAVEL);
                }
            }
        }

        public override void Decorate(Chunk c, Chunk[] nb)
        {
            var w = new ChunkWriter(c);
            int x0 = c.cx << 4, z0 = c.cz << 4;
            var rng = new RNG(seed, c.cx, c.cz, 222);
            // ores (own chunk; small veins, fully inside chunk)
            for (int i = 0; i < 16; i++) Vein(c, ref rng, QUARTZ, 10, 14, 10, 117);
            for (int i = 0; i < 10; i++) Vein(c, ref rng, NGOLD, 8, 10, 10, 117);
            for (int i = 0; i < 2; i++) Vein(c, ref rng, DEBRIS, 2, 3, 8, 24, true);
            for (int i = 0; i < 4; i++) Vein(c, ref rng, MAGMA, 20, 25, 27, 36);
            for (int i = 0; i < 2; i++) Vein(c, ref rng, BLACKSTONE, 22, 30, 5, 30);
            ushort fire = S("fire"), soulFire = S("soul_fire");
            for (int lz = 0; lz < 16; lz++)
                for (int lx = 0; lx < 16; lx++)
                {
                    int biome = c.biomes2D[lz * 16 + lx];
                    int wx = x0 + lx, wz = z0 + lz;
                    for (int y = 6; y < world.height - 7; y++)
                    {
                        ushort s = c.Get(lx, y, lz);
                        if (s == 0 || Blocks.ByState[s].isLiquid) continue;
                        bool airAbove = c.Get(lx, y + 1, lz) == 0, airBelow = c.Get(lx, y - 1, lz) == 0;
                        if (airAbove)
                        {
                            float r = rng.NextFloat();
                            bool interior = lx >= 3 && lx <= 12 && lz >= 3 && lz <= 12;
                            if (!interior && r < 0.013f) r = 0.5f;
                            if (biome == crimson && s == CRIMSON_NYLIUM)
                            {
                                if (r < 0.012f) TreeFeatures.Place(w, "crimson_fungus_tree", wx, y + 1, wz, ref rng, true);
                                else if (r < 0.25f) c.SetRaw(lx, y + 1, lz, S("crimson_roots"));
                                else if (r < 0.28f) c.SetRaw(lx, y + 1, lz, S("crimson_fungus"));
                            }
                            else if (biome == warped && s == WARPED_NYLIUM)
                            {
                                if (r < 0.012f) TreeFeatures.Place(w, "warped_fungus_tree", wx, y + 1, wz, ref rng, true);
                                else if (r < 0.2f) c.SetRaw(lx, y + 1, lz, S("warped_roots"));
                                else if (r < 0.32f) c.SetRaw(lx, y + 1, lz, S("nether_sprouts"));
                                else if (r < 0.35f) c.SetRaw(lx, y + 1, lz, S("warped_fungus"));
                                else if (r < 0.36f) { int len = rng.Range(2, 8); for (int k = 1; k <= len && c.Get(lx, y + k, lz) == 0; k++) c.SetRaw(lx, y + k, lz, S("twisting_vines")); }
                            }
                            else if (biome == soul && (s == SOUL_SAND || s == SOUL_SOIL))
                            {
                                if (r < 0.01f) c.SetRaw(lx, y + 1, lz, soulFire);
                                else if (r < 0.013f) Fossil(w, wx, y, wz, ref rng);
                                else if (r < 0.018f && y < 80) BasaltPillar(c, lx, y + 1, lz, ref rng);
                            }
                            else if (biome == deltas)
                            {
                                if (r < 0.02f && y > LavaLevel && y < LavaLevel + 20) BasaltPillar(c, lx, y + 1, lz, ref rng);
                            }
                            else if (biome == wastes && s == NETHERRACK)
                            {
                                if (r < 0.006f) c.SetRaw(lx, y + 1, lz, fire);
                                else if (r < 0.008f) c.SetRaw(lx, y + 1, lz, S(rng.NextBool() ? "brown_mushroom" : "red_mushroom"));
                            }
                        }
                        if (airBelow && (s == NETHERRACK || s == BLACKSTONE || s == BASALT))
                        {
                            if (rng.Chance(biome == wastes ? 0.004f : 0.0015f) && y > 60) Glowstone(c, lx, y - 1, lz, ref rng);
                            else if (biome == crimson && rng.Chance(0.02f)) { int len = rng.Range(1, 8); for (int k = 1; k <= len && c.Get(lx, y - k, lz) == 0; k++) c.SetRaw(lx, y - k, lz, S("weeping_vines")); }
                        }
                    }
                }
            StructureManager.PlaceInChunk(this, c, w);
        }

        void Vein(Chunk c, ref RNG rng, ushort ore, int minSize, int maxSize, int minY, int maxY, bool buried = false)
        {
            int x = rng.Range(1, 14), z = rng.Range(1, 14), y = rng.Range(minY, maxY);
            int size = rng.Range(minSize, maxSize);
            for (int i = 0; i < size; i++)
            {
                if (x < 0 || x > 15 || z < 0 || z > 15 || y < 5 || y > 122) break;
                ushort cur = c.Get(x, y, z);
                if (cur == NETHERRACK || cur == BASALT || cur == BLACKSTONE || cur == SOUL_SOIL)
                {
                    bool exposed = c.Get(Mathf.Min(15, x + 1), y, z) == 0 || c.Get(Mathf.Max(0, x - 1), y, z) == 0 || c.Get(x, y + 1, z) == 0 || c.Get(x, y - 1, z) == 0;
                    if (!buried || !exposed) c.SetRaw(x, y, z, ore);
                }
                int d = rng.Next(6);
                if (d == 0) x++; else if (d == 1) x--; else if (d == 2) y++; else if (d == 3) y--; else if (d == 4) z++; else z--;
            }
        }

        void Glowstone(Chunk c, int lx, int y, int lz, ref RNG rng)
        {
            int n = rng.Range(20, 60);
            int x = lx, yy = y, z = lz;
            for (int i = 0; i < n; i++)
            {
                int px = Mathf.Clamp(lx + rng.Range(-3, 3), 0, 15), py = y - rng.Range(0, 7), pz = Mathf.Clamp(lz + rng.Range(-3, 3), 0, 15);
                if (c.Get(px, py, pz) != 0) continue;
                // attach to neighbour glowstone or ceiling
                bool ok = c.Get(px, py + 1, pz) == GLOWSTONE || c.Get(px, py + 1, pz) == NETHERRACK || (px > 0 && c.Get(px - 1, py, pz) == GLOWSTONE) || (px < 15 && c.Get(px + 1, py, pz) == GLOWSTONE) || (pz > 0 && c.Get(px, py, pz - 1) == GLOWSTONE) || (pz < 15 && c.Get(px, py, pz + 1) == GLOWSTONE);
                if (ok) c.SetRaw(px, py, pz, GLOWSTONE);
            }
            if (c.Get(lx, y, lz) == 0) c.SetRaw(lx, y, lz, GLOWSTONE);
        }

        void BasaltPillar(Chunk c, int lx, int y, int lz, ref RNG rng)
        {
            int h = rng.Range(3, 14);
            for (int i = 0; i < h && y + i < 120; i++)
            {
                if (c.Get(lx, y + i, lz) != 0) break;
                c.SetRaw(lx, y + i, lz, BASALT);
            }
        }

        void Fossil(ChunkWriter w, int x, int y, int z, ref RNG rng)
        {
            int len = rng.Range(4, 8);
            bool alongX = rng.NextBool();
            for (int i = 0; i < len; i++)
            {
                int px = alongX ? x + i : x, pz = alongX ? z : z + i;
                if (w.CanWrite(px, y + 1, pz)) w.Set(px, y + 1, pz, BONE);
                if (i % 2 == 0)
                    for (int r = 1; r <= 2; r++)
                    {
                        int rx = alongX ? px : px + r, rz = alongX ? pz + r : pz;
                        if (w.CanWrite(rx, y + 1 + r, rz)) w.Set(rx, y + 1 + r, rz, BONE);
                        int lx2 = alongX ? px : px - r, lz2 = alongX ? pz - r : pz;
                        if (w.CanWrite(lx2, y + 1 + r, lz2)) w.Set(lx2, y + 1 + r, lz2, BONE);
                    }
            }
        }

        public override Vector3 FindSpawn() => new Vector3(0.5f, 64, 0.5f);

        /// <summary>Find a safe standing position near (x,z) in the nether cavern (for portal exits).</summary>
        public int FindFloor(World w, int x, int z, int preferY)
        {
            for (int dy = 0; dy < 80; dy++)
            {
                foreach (int y in new[] { preferY + dy, preferY - dy })
                {
                    if (y < 8 || y > 118) continue;
                    ushort below = w.GetState(x, y - 1, z);
                    if (!Blocks.ByState[below].solid) continue;
                    if (w.GetState(x, y, z) == 0 && w.GetState(x, y + 1, z) == 0 && w.GetState(x, y + 2, z) == 0) return y;
                }
            }
            return -1;
        }
    }

    /// <summary>The End: central island with obsidian pillars and bedrock exit fountain; outer islands beyond 1000 blocks.</summary>
    public sealed class EndGenerator : WorldGenerator
    {
        readonly OctaveNoise islandN, detail, outerN;
        readonly ushort END_STONE, OBSIDIAN, BEDROCK, IRON_BARS;
        public const int PillarRing = 42;
        public struct Pillar { public int x, z, radius, height; public bool caged; }
        public readonly Pillar[] pillars = new Pillar[10];
        public const int OuterStart = 850;

        public EndGenerator(World w) : base(w)
        {
            islandN = new OctaveNoise(seed, 201, 1.0 / 48.0, 1.0, 0.5, 0.25);
            detail = new OctaveNoise(seed, 202, 1.0 / 16.0, 1.0, 0.5);
            outerN = new OctaveNoise(seed, 203, 1.0 / 110.0, 1.0, 0.5, 0.2);
            END_STONE = S("end_stone"); OBSIDIAN = S("obsidian"); BEDROCK = S("bedrock"); IRON_BARS = S("iron_bars");
            var rng = new RNG(seed, 0, 0, 777);
            var heights = new List<int>(); for (int i = 0; i < 10; i++) heights.Add(76 + i * 3);
            rng.Shuffle(heights);
            for (int i = 0; i < 10; i++)
            {
                float ang = 2f * Mathf.PI * (-i) / 10f + Mathf.PI * 0.5f;
                int h = heights[i];
                int idx = (h - 76) / 3;
                pillars[i] = new Pillar { x = Mathf.FloorToInt(PillarRing * Mathf.Cos(ang)), z = Mathf.FloorToInt(PillarRing * Mathf.Sin(ang)), radius = 2 + idx / 3, height = h, caged = idx == 1 || idx == 2 };
            }
        }

        public override string BiomeAtApprox(int x, int z) => (x * (long)x + z * (long)z) < 1000L * 1000L ? "the_end" : "end_highlands";
        public override int ApproxSurface(int x, int z) => 64;

        /// <summary>Main island shape: returns top and bottom y (or -1 if none).</summary>
        void MainIsland(int x, int z, out int top, out int bottom)
        {
            top = bottom = -1;
            float dist = Mathf.Sqrt(x * (float)x + z * (float)z);
            float n = (float)islandN.Sample(x, z);
            float R = 88f + n * 22f;
            if (dist > R) return;
            float t = dist / R;
            top = Mathf.RoundToInt(60f - t * t * 6f + (float)detail.Sample(x, z) * 2.5f);
            bottom = Mathf.RoundToInt(60f - (1f - t) * (1f - t) * 40f - 4f + n * 6f);
            if (bottom > top - 1) bottom = top - 1;
        }

        bool OuterIsland(int x, int z, out int top, out int bottom)
        {
            top = bottom = -1;
            float dist = Mathf.Sqrt(x * (float)x + z * (float)z);
            if (dist < OuterStart) return false;
            float n = (float)outerN.Sample(x, z) * 1.7f;
            float edge = Mathf.Clamp01((dist - OuterStart) / 150f);
            float v = n - (0.35f - edge * 0.12f);
            if (v <= 0) return false;
            float thick = Mathf.Clamp(v * 55f, 1f, 30f);
            top = Mathf.RoundToInt(58f + v * 18f + (float)detail.Sample(x, z) * 2f);
            bottom = Mathf.RoundToInt(top - thick);
            return true;
        }

        public override void GenerateTerrain(Chunk c)
        {
            int x0 = c.cx << 4, z0 = c.cz << 4;
            int theEnd = Biome.Id("the_end"), high = Biome.Id("end_highlands"), barrens = Biome.Id("end_barrens");
            for (int lz = 0; lz < 16; lz++)
                for (int lx = 0; lx < 16; lx++)
                {
                    int x = x0 + lx, z = z0 + lz;
                    int top, bottom;
                    MainIsland(x, z, out top, out bottom);
                    bool outer = false;
                    if (top < 0) outer = OuterIsland(x, z, out top, out bottom);
                    float dist = Mathf.Sqrt(x * (float)x + z * (float)z);
                    c.biomes2D[lz * 16 + lx] = (byte)(dist < 700 ? theEnd : ((top - bottom) > 12 ? high : barrens));
                    if (top >= 0) for (int y = Mathf.Max(1, bottom); y <= top; y++) c.SetRaw(lx, y, lz, END_STONE);
                    c.genTopY[lz * 16 + lx] = (short)(top >= 0 ? top : 0);
                    c.genTop[lz * 16 + lx] = top >= 0 ? END_STONE : (ushort)0;
                }
            // obsidian pillars
            foreach (var p in pillars)
            {
                for (int x = p.x - p.radius - 3; x <= p.x + p.radius + 3; x++)
                    for (int z = p.z - p.radius - 3; z <= p.z + p.radius + 3; z++)
                    {
                        if (x < x0 || x >= x0 + 16 || z < z0 || z >= z0 + 16) continue;
                        int dx = x - p.x, dz = z - p.z;
                        if (dx * dx + dz * dz <= p.radius * p.radius + 1)
                            for (int y = 45; y <= p.height; y++) c.SetRaw(x - x0, y, z - z0, OBSIDIAN);
                        if (p.caged && Mathf.Abs(dx) <= 2 && Mathf.Abs(dz) <= 2)
                        {
                            for (int y = p.height + 1; y <= p.height + 3; y++)
                                if (Mathf.Abs(dx) == 2 || Mathf.Abs(dz) == 2 || y == p.height + 3) c.SetRaw(x - x0, y, z - z0, IRON_BARS);
                        }
                        if (dx == 0 && dz == 0) c.SetRaw(x - x0, p.height + 1, z - z0, BEDROCK);
                    }
            }
            // exit portal fountain (inactive)
            if (x0 <= 4 && x0 + 16 > -4 && z0 <= 4 && z0 + 16 > -4) BuildExitPortal(new ChunkWriter(c), false);
        }

        public static void BuildExitPortal(IBlockAccess w, bool active, int baseY = 60)
        {
            ushort bed = Blocks.StateOf("bedrock"), portal = Blocks.StateOf("end_portal"), torch = Blocks.Get("torch")?.DefaultState ?? 0;
            for (int x = -4; x <= 4; x++)
                for (int z = -4; z <= 4; z++)
                {
                    float d = Mathf.Sqrt(x * x + z * z);
                    if (d > 3.5f + 0.5f) continue;
                    if (w.CanWrite(x, baseY, z)) w.Set(x, baseY, z, bed);
                    if (d > 2.5f && d <= 3.5f + 0.5f) { if (w.CanWrite(x, baseY + 1, z)) w.Set(x, baseY + 1, z, bed); }
                    else if (d <= 2.5f && !(x == 0 && z == 0)) { if (w.CanWrite(x, baseY + 1, z)) w.Set(x, baseY + 1, z, active ? portal : (ushort)0); }
                }
            for (int y = 1; y <= 4; y++) if (w.CanWrite(0, baseY + y, 0)) w.Set(0, baseY + y, 0, bed);
            foreach (var (tx, tz) in new[] { (1, 0), (-1, 0), (0, 1), (0, -1) })
            {
                var tb = Blocks.Get("torch");
                if (tb != null && w.CanWrite(tx, baseY + 3, tz))
                {
                    Dir f = tx == 1 ? Dir.East : tx == -1 ? Dir.West : tz == 1 ? Dir.North : Dir.South;
                    w.Set(tx, baseY + 3, tz, tb.State(1 + DirUtil.HorizIndex(f)));
                }
            }
        }

        public override void Decorate(Chunk c, Chunk[] nb)
        {
            var w = new ChunkWriter(c);
            var rng = new RNG(seed, c.cx, c.cz, 333);
            int x0 = c.cx << 4, z0 = c.cz << 4;
            float dist = Mathf.Sqrt((x0 + 8) * (float)(x0 + 8) + (z0 + 8) * (float)(z0 + 8));
            if (dist > OuterStart)
            {
                // chorus plants
                for (int i = 0; i < 3; i++)
                {
                    int lx = rng.Next(16), lz = rng.Next(16);
                    int y = c.genTopY[lz * 16 + lx];
                    if (c.genTop[lz * 16 + lx] != END_STONE || y <= 0) continue;
                    ChorusTree(w, x0 + lx, y + 1, z0 + lz, ref rng);
                }
                StructureManager.PlaceInChunk(this, c, w);
            }
        }

        public static void ChorusTree(IBlockAccess w, int x, int y, int z, ref RNG rng, int depth = 0)
        {
            ushort plant = Blocks.StateOf("chorus_plant"), flower = Blocks.StateOf("chorus_flower");
            if (plant == 0) return;
            int h = rng.Range(1, 4);
            for (int i = 0; i < h; i++) if (w.CanWrite(x, y + i, z)) w.Set(x, y + i, z, plant);
            if (depth < 3)
            {
                int branches = rng.Range(1, 3);
                for (int b = 0; b < branches; b++)
                {
                    int d = rng.Next(4);
                    int bx = x + (d == 0 ? 1 : d == 1 ? -1 : 0), bz = z + (d == 2 ? 1 : d == 3 ? -1 : 0);
                    int by = y + h - 1;
                    if (w.CanWrite(bx, by, bz)) w.Set(bx, by, bz, plant);
                    ChorusTree(w, bx, by + 1, bz, ref rng, depth + 1);
                }
            }
            if (w.CanWrite(x, y + h, z)) w.Set(x, y + h, z, flower);
        }

        public override Vector3 FindSpawn() => new Vector3(100.5f, 50, 0.5f);

        /// <summary>Analytic surface query (used to place gateway exits without loading chunks).</summary>
        public bool SurfaceAt(int x, int z, out int top)
        {
            MainIsland(x, z, out top, out int bottom);
            if (top >= 0) return true;
            return OuterIsland(x, z, out top, out bottom);
        }
    }
}
