using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public sealed partial class OverworldGenerator
    {
        struct OreSpec { public string ore, deep; public int count, size, minY, maxY; public bool triangle; public bool mountainsOnly; public bool reduceAir; }
        static readonly OreSpec[] Ores =
        {
            new OreSpec { ore = "coal_ore", deep = "deepslate_coal_ore", count = 20, size = 12, minY = 0, maxY = 192, triangle = true },
            new OreSpec { ore = "coal_ore", deep = "deepslate_coal_ore", count = 10, size = 10, minY = 136, maxY = 320 },
            new OreSpec { ore = "copper_ore", deep = "deepslate_copper_ore", count = 12, size = 10, minY = -16, maxY = 112, triangle = true },
            new OreSpec { ore = "iron_ore", deep = "deepslate_iron_ore", count = 12, size = 8, minY = -24, maxY = 56, triangle = true },
            new OreSpec { ore = "iron_ore", deep = "deepslate_iron_ore", count = 10, size = 8, minY = 80, maxY = 384, triangle = true },
            new OreSpec { ore = "iron_ore", deep = "deepslate_iron_ore", count = 5, size = 4, minY = -64, maxY = 72 },
            new OreSpec { ore = "gold_ore", deep = "deepslate_gold_ore", count = 5, size = 9, minY = -64, maxY = 32, triangle = true },
            new OreSpec { ore = "redstone_ore", deep = "deepslate_redstone_ore", count = 5, size = 8, minY = -64, maxY = 15 },
            new OreSpec { ore = "redstone_ore", deep = "deepslate_redstone_ore", count = 8, size = 8, minY = -96, maxY = -32, triangle = true },
            new OreSpec { ore = "lapis_ore", deep = "deepslate_lapis_ore", count = 2, size = 7, minY = -32, maxY = 32, triangle = true },
            new OreSpec { ore = "lapis_ore", deep = "deepslate_lapis_ore", count = 4, size = 7, minY = -64, maxY = 64, reduceAir = true },
            new OreSpec { ore = "diamond_ore", deep = "deepslate_diamond_ore", count = 7, size = 4, minY = -144, maxY = 16, triangle = true, reduceAir = true },
            new OreSpec { ore = "diamond_ore", deep = "deepslate_diamond_ore", count = 1, size = 12, minY = -144, maxY = 16, triangle = true, reduceAir = true },
            new OreSpec { ore = "emerald_ore", deep = "deepslate_emerald_ore", count = 50, size = 3, minY = -16, maxY = 480, triangle = true, mountainsOnly = true },
            new OreSpec { ore = "granite", deep = null, count = 2, size = 48, minY = 0, maxY = 60 },
            new OreSpec { ore = "diorite", deep = null, count = 2, size = 48, minY = 0, maxY = 60 },
            new OreSpec { ore = "andesite", deep = null, count = 2, size = 48, minY = 0, maxY = 60 },
            new OreSpec { ore = "tuff", deep = "tuff", count = 2, size = 48, minY = -64, maxY = 0 },
            new OreSpec { ore = "gravel", deep = "gravel", count = 10, size = 26, minY = -64, maxY = 320 },
            new OreSpec { ore = "dirt", deep = null, count = 6, size = 26, minY = 0, maxY = 160 },
        };

        [System.ThreadStatic] static List<(int x, int y, int z, string type)> treeList;

        public override void Decorate(Chunk c, Chunk[] nb)
        {
            var w = new ChunkWriter(c);
            int x0 = c.cx << 4, z0 = c.cz << 4;
            ushort stone = STONE, deep = DEEPSLATE;
            // ------------------------------------------------ ores (own origins + neighbours' spill-over)
            for (int n = 0; n < 9; n++)
            {
                int ncx = c.cx + (n % 3) - 1, ncz = c.cz + (n / 3) - 1;
                var rng = new RNG(seed, ncx, ncz, 2001);
                var nc = nb[n];
                foreach (var o in Ores)
                {
                    ushort oreS = S(o.ore), deepS = o.deep != null ? S(o.deep) : oreS;
                    for (int i = 0; i < o.count; i++)
                    {
                        int ox = (ncx << 4) + rng.Next(16), oz = (ncz << 4) + rng.Next(16);
                        int oy = o.triangle ? (rng.Range(o.minY, o.maxY) + rng.Range(o.minY, o.maxY)) / 2 : rng.Range(o.minY, o.maxY);
                        if (o.mountainsOnly && nc != null) { var bb = Biome.Get(nc.biomes2D[((oz & 15) << 4) | (ox & 15)]); if (!bb.isMountain) { rng.NextULong(); continue; } }
                        if (oy < world.minY || oy >= world.maxY) { rng.NextULong(); continue; }
                        PlaceBlob(w, ref rng, ox, oy, oz, o.size, oreS, deepS, stone, deep, o.reduceAir);
                    }
                }
            }
            // ------------------------------------------------ own-chunk underground decoration
            var own = new RNG(seed, c.cx, c.cz, 3001);
            DecorateCaves(c, w, ref own);
            // ------------------------------------------------ trees from 3x3 origins
            var trees = treeList ?? (treeList = new List<(int, int, int, string)>());
            for (int n = 0; n < 9; n++)
            {
                trees.Clear();
                var nc = nb[n];
                if (nc == null) continue;
                CollectTrees(nc, trees);
                foreach (var t in trees)
                {
                    var trng = new RNG(seed, t.x, t.z, 4001 + t.y);
                    TreeFeatures.Place(w, t.type, t.x, t.y, t.z, ref trng, true);
                }
            }
            // ------------------------------------------------ own-chunk vegetation
            Vegetation(c, w, ref own);
            // ------------------------------------------------ structures
            StructureManager.PlaceInChunk(this, c, w);
            // ------------------------------------------------ snow on top of leaves in cold biomes
            for (int lz = 0; lz < 16; lz++)
                for (int lx = 0; lx < 16; lx++)
                {
                    var b = Biome.Get(c.biomes2D[lz * 16 + lx]);
                    if (!(b.snowy || b.frozenWater)) continue;
                    int y = world.maxY - 2;
                    while (y > world.minY && c.Get(lx, y, lz) == 0) y--;
                    ushort s = c.Get(lx, y, lz);
                    var bl = Blocks.ByState[s];
                    if (bl is LeavesBlock || (bl.opaqueCube && bl.id != "ice" && bl.id != "packed_ice")) { if (c.Get(lx, y + 1, lz) == 0) c.SetRaw(lx, y + 1, lz, SNOW_LAYER); }
                }
        }

        void PlaceBlob(ChunkWriter w, ref RNG rng, int x, int y, int z, int size, ushort ore, ushort deepOre, ushort stone, ushort deep, bool reduceAir)
        {
            float ang = rng.NextFloat() * Mathf.PI;
            float len = size / 8f;
            float sx = x + Mathf.Sin(ang) * len, ex = x - Mathf.Sin(ang) * len;
            float sz = z + Mathf.Cos(ang) * len, ez = z - Mathf.Cos(ang) * len;
            float sy = y + rng.Range(-2, 2), ey = y + rng.Range(-2, 2);
            for (int i = 0; i < size; i++)
            {
                float t = (float)i / size;
                float cx = Mathf.Lerp(sx, ex, t), cy = Mathf.Lerp(sy, ey, t), cz = Mathf.Lerp(sz, ez, t);
                float r = (Mathf.Sin(t * Mathf.PI) + 1f) * (rng.NextFloat() * size / 16f + 0.5f) * 0.5f;
                int ix0 = Mathf.FloorToInt(cx - r), ix1 = Mathf.FloorToInt(cx + r);
                int iy0 = Mathf.FloorToInt(cy - r), iy1 = Mathf.FloorToInt(cy + r);
                int iz0 = Mathf.FloorToInt(cz - r), iz1 = Mathf.FloorToInt(cz + r);
                for (int bx = ix0; bx <= ix1; bx++)
                    for (int by = iy0; by <= iy1; by++)
                        for (int bz = iz0; bz <= iz1; bz++)
                        {
                            if (!w.CanWrite(bx, by, bz)) continue;
                            float dx = (bx + 0.5f - cx) / r, dy = (by + 0.5f - cy) / r, dz = (bz + 0.5f - cz) / r;
                            if (dx * dx + dy * dy + dz * dz >= 1f) continue;
                            ushort cur = w.Get(bx, by, bz);
                            ushort place;
                            if (cur == stone) place = ore;
                            else if (cur == deep) place = deepOre;
                            else continue;
                            if (reduceAir && Exposed(w, bx, by, bz) && Hash.Float01(seed, bx, by, bz) < 0.5f) continue;
                            w.Set(bx, by, bz, place);
                        }
            }
        }
        static bool Exposed(ChunkWriter w, int x, int y, int z)
        {
            return w.Get(x + 1, y, z) == 0 || w.Get(x - 1, y, z) == 0 || w.Get(x, y + 1, z) == 0 || w.Get(x, y - 1, z) == 0 || w.Get(x, y, z + 1) == 0 || w.Get(x, y, z - 1) == 0;
        }

        /// <summary>Deterministic tree list for a chunk (depends only on its immutable generation info).</summary>
        void CollectTrees(Chunk nc, List<(int, int, int, string)> list)
        {
            var rng = new RNG(seed, nc.cx, nc.cz, 5001);
            // dominant biome of chunk centre for count
            int centerBiome = nc.biomes2D[8 * 16 + 8];
            var cb = Biome.Get(centerBiome);
            float expected = cb.treesPerChunk;
            int count = (int)expected;
            if (rng.NextFloat() < expected - count) count++;
            if (expected > 0 && rng.Chance(0.1f)) count++;
            for (int i = 0; i < count; i++)
            {
                int lx = rng.Next(16), lz = rng.Next(16);
                int idx = lz * 16 + lx;
                var b = Biome.Get(nc.biomes2D[idx]);
                if (b.trees.Length == 0) { rng.NextULong(); continue; }
                string type = b.trees[rng.Next(b.trees.Length)];
                int y = nc.genTopY[idx];
                ushort top = nc.genTop[idx];
                if (nc.genWater[idx] > 0 && type != "mangrove") continue;
                if (nc.genWater[idx] > 2) continue;
                var tb = Blocks.ByState[top];
                bool soil = tb.id == "grass_block" || tb.id == "dirt" || tb.id == "podzol" || tb.id == "coarse_dirt" || tb.id == "mud" || tb.id == "moss_block" || tb.id == "mycelium" || tb.id == "snow_block" && type == "ice_spike" || tb.id == "pale_moss_block";
                if (type == "ice_spike") soil = tb.id == "snow_block" || tb.id == "grass_block";
                if (!soil) continue;
                if (y < Sea) continue;
                // forest variety
                if (type == "oak" && rng.Chance(0.1f) && b.treesPerChunk > 3) type = "fancy_oak";
                if (type == "birch" && rng.Chance(0.05f)) type = "tall_birch";
                list.Add(((nc.cx << 4) + lx, y + 1, (nc.cz << 4) + lz, type));
            }
            if (cb.fallenLogs && rng.Chance(0.25f))
            {
                int lx = rng.Next(16), lz = rng.Next(16); int idx = lz * 16 + lx;
                if (nc.genWater[idx] == 0 && Blocks.ByState[nc.genTop[idx]].id == "grass_block")
                    list.Add(((nc.cx << 4) + lx, nc.genTopY[idx] + 1, (nc.cz << 4) + lz, cb.trees.Length > 0 && cb.trees[0] == "birch" ? "fallen_birch" : (cb.trees.Length > 0 && cb.trees[0] == "spruce" ? "fallen_spruce" : "fallen_oak")));
            }
            if (cb.boulders && rng.Chance(0.3f))
            {
                int lx = rng.Next(16), lz = rng.Next(16); int idx = lz * 16 + lx;
                list.Add(((nc.cx << 4) + lx, nc.genTopY[idx], (nc.cz << 4) + lz, "boulder"));
            }
            if (cb.icebergs && rng.Chance(0.12f))
            {
                int lx = rng.Next(16), lz = rng.Next(16);
                list.Add(((nc.cx << 4) + lx, Sea, (nc.cz << 4) + lz, "iceberg"));
            }
        }

        void Vegetation(Chunk c, ChunkWriter w, ref RNG rng)
        {
            int x0 = c.cx << 4, z0 = c.cz << 4;
            ushort shortGrass = S("short_grass"), fern = S("fern"), deadBush = S("dead_bush"), cactus = S("cactus"), cane = S("sugar_cane"), lily = S("lily_pad");
            ushort seagrass = S("seagrass"), kelp = S("kelp"), kelpPlant = S("kelp_plant"), pumpkin = S("pumpkin"), melon = S("melon"), berries = S("sweet_berry_bush");
            ushort bambooS = S("bamboo"), brownM = S("brown_mushroom"), redM = S("red_mushroom"), leafLitter = S("leaf_litter"), bush = S("bush");
            for (int lz = 0; lz < 16; lz++)
                for (int lx = 0; lx < 16; lx++)
                {
                    int idx = lz * 16 + lx;
                    var b = Biome.Get(c.biomes2D[idx]);
                    int y = c.genTopY[idx];
                    int wx = x0 + lx, wz = z0 + lz;
                    ushort cur = c.Get(lx, y, lz);
                    ushort above = c.Get(lx, y + 1, lz);
                    int water = c.genWater[idx];
                    if (water > 0)
                    {
                        // underwater plants
                        if (Blocks.ByState[above].isLiquid && above == WATER)
                        {
                            if (b.kelp && water >= 4 && rng.Chance(0.06f))
                            {
                                int h = rng.Range(2, Mathf.Min(water - 1, 12));
                                for (int k = 1; k <= h; k++) w.Set(wx, y + k, wz, k == h ? kelp : kelpPlant);
                            }
                            else if (b.seagrass && rng.Chance(0.25f)) w.Set(wx, y + 1, wz, seagrass);
                            else if (b.coral && water >= 3 && rng.Chance(0.1f)) Coral(w, ref rng, wx, y + 1, wz);
                            if (b.lilyPads && water == 1 && rng.Chance(0.12f) && w.Get(wx, y + 2, wz) == 0) w.Set(wx, y + 2, wz, lily);
                        }
                        continue;
                    }
                    if (above != 0) continue;
                    var cb = Blocks.ByState[cur];
                    bool grassy = cb.id == "grass_block" || cb.id == "podzol" || cb.id == "coarse_dirt" || cb.id == "moss_block" || cb.id == "pale_moss_block";
                    bool sandy = cb.id == "sand" || cb.id == "red_sand";
                    if (grassy)
                    {
                        float r = rng.NextFloat();
                        if (b.bamboo && rng.Chance(0.12f)) { int h = rng.Range(6, 14); for (int k = 1; k <= h; k++) w.Set(wx, y + k, wz, (ushort)(bambooS + (k > h - 3 ? 1 : 0))); continue; }
                        if (b.flowerDensity > 0 && r < b.flowerDensity && b.flowers.Length > 0)
                        {
                            string f = b.flowers[rng.Next(b.flowers.Length)];
                            var fb = Blocks.Get(f);
                            if (fb is TallPlantBlock) { if (c.Get(lx, y + 2, lz) == 0) { w.Set(wx, y + 1, wz, fb.State(0)); w.Set(wx, y + 2, wz, fb.State(1)); } }
                            else if (fb != null) w.Set(wx, y + 1, wz, fb is CarpetBlock ? fb.State(0) : fb.DefaultState);
                            continue;
                        }
                        r = rng.NextFloat();
                        if (r < b.grassDensity)
                        {
                            if (rng.Chance(b.tallGrassChance) && c.Get(lx, y + 2, lz) == 0) { var tg = Blocks.Get("tall_grass"); w.Set(wx, y + 1, wz, tg.State(0)); w.Set(wx, y + 2, wz, tg.State(1)); }
                            else w.Set(wx, y + 1, wz, shortGrass);
                            continue;
                        }
                        if (r < b.grassDensity + b.fernDensity)
                        {
                            if (rng.Chance(0.2f) && c.Get(lx, y + 2, lz) == 0) { var lf = Blocks.Get("large_fern"); w.Set(wx, y + 1, wz, lf.State(0)); w.Set(wx, y + 2, wz, lf.State(1)); }
                            else w.Set(wx, y + 1, wz, fern);
                            continue;
                        }
                        if (b.bushes && rng.Chance(0.004f)) { w.Set(wx, y + 1, wz, bush); continue; }
                        if (b.leafLitter && rng.Chance(0.05f)) { w.Set(wx, y + 1, wz, leafLitter); continue; }
                        if (b.sweetBerries && rng.Chance(0.004f)) { w.Set(wx, y + 1, wz, (ushort)(berries + 3)); continue; }
                        if (b.melons && rng.Chance(0.006f)) { w.Set(wx, y + 1, wz, melon); continue; }
                        if (b.pumpkins && rng.Chance(0.0015f)) { w.Set(wx, y + 1, wz, pumpkin); continue; }
                        if (b.mushrooms && rng.Chance(0.01f)) { w.Set(wx, y + 1, wz, rng.NextBool() ? brownM : redM); continue; }
                    }
                    if (sandy || cb.id == "grass_block" || cb.id == "dirt")
                    {
                        // sugar cane next to water
                        if ((b.sugarCane || sandy) && rng.Chance(0.08f) && NextToWater(c, lx, y, lz))
                        {
                            int h = rng.Range(1, 3);
                            for (int k = 1; k <= h; k++) w.Set(wx, y + k, wz, cane);
                            continue;
                        }
                    }
                    if (sandy)
                    {
                        if (b.cactus && rng.Chance(0.008f) && CactusSpot(c, lx, y + 1, lz))
                        {
                            int h = rng.Range(1, 3);
                            for (int k = 1; k <= h; k++) w.Set(wx, y + k, wz, cactus);
                            continue;
                        }
                        if (b.deadBush && rng.Chance(0.012f)) w.Set(wx, y + 1, wz, deadBush);
                    }
                    else if (b.deadBush && cb.id.EndsWith("terracotta") && rng.Chance(0.01f)) w.Set(wx, y + 1, wz, deadBush);
                }
        }

        bool NextToWater(Chunk c, int lx, int y, int lz)
        {
            for (int d = 0; d < 4; d++)
            {
                int nx = lx + (d == 0 ? 1 : d == 1 ? -1 : 0), nz = lz + (d == 2 ? 1 : d == 3 ? -1 : 0);
                if (nx < 0 || nz < 0 || nx > 15 || nz > 15) continue;
                if (c.Get(nx, y, nz) == WATER) return true;
            }
            return false;
        }
        bool CactusSpot(Chunk c, int lx, int y, int lz)
        {
            for (int d = 0; d < 4; d++)
            {
                int nx = lx + (d == 0 ? 1 : d == 1 ? -1 : 0), nz = lz + (d == 2 ? 1 : d == 3 ? -1 : 0);
                if (nx < 0 || nz < 0 || nx > 15 || nz > 15) return false;
                if (c.Get(nx, y, nz) != 0) return false;
            }
            return true;
        }

        void Coral(ChunkWriter w, ref RNG rng, int x, int y, int z)
        {
            string[] kinds = { "tube", "brain", "bubble", "fire", "horn" };
            string k = kinds[rng.Next(kinds.Length)];
            ushort block = S(k + "_coral_block"), plant = S(k + "_coral");
            int shape = rng.Next(3);
            if (shape == 0)
            {
                int h = rng.Range(1, 3);
                for (int i = 0; i < h; i++) w.Set(x, y + i, z, block);
                if (w.Get(x, y + h, z) == WATER) w.Set(x, y + h, z, plant);
            }
            else if (shape == 1)
            {
                for (int dx = -1; dx <= 1; dx++) for (int dz = -1; dz <= 1; dz++) if (rng.Chance(0.7f)) { w.Set(x + dx, y, z + dz, block); if (rng.Chance(0.4f) && w.Get(x + dx, y + 1, z + dz) == WATER) w.Set(x + dx, y + 1, z + dz, plant); }
            }
            else w.Set(x, y, z, plant);
        }

        void DecorateCaves(Chunk c, ChunkWriter w, ref RNG rng)
        {
            int x0 = c.cx << 4, z0 = c.cz << 4;
            int lush = Biome.Lush_Caves.id, drip = Biome.Dripstone_Caves.id, dark = Biome.Deep_Dark.id, sulfur = Biome.Sulfur_Caves.id;
            ushort moss = MOSS, clay = CLAY, azalea = S("azalea"), flowering = S("flowering_azalea"), mossCarpet = S("moss_carpet"), glowBerries = S("cave_vines"), dripBlock = S("dripstone_block"), pointed = S("pointed_dripstone");
            ushort sculk = S("sculk"), sculkSensor = S("sculk_sensor"), shrieker = S("sculk_shrieker"), sulfurS = S("sulfur"), potent = S("potent_sulfur"), cinnabar = S("cinnabar"), spike = S("sulfur_spike");
            ushort bigDrip = S("big_dripleaf"), smallDrip = S("small_dripleaf"), spore = S("spore_blossom"), glowLichen = S("glow_lichen");
            for (int lz = 0; lz < 16; lz++)
                for (int lx = 0; lx < 16; lx++)
                {
                    int topY = c.genTopY[lz * 16 + lx];
                    for (int y = world.minY + 6; y < topY - 4; y++)
                    {
                        ushort s = c.Get(lx, y, lz);
                        if (s != STONE && s != DEEPSLATE && s != TUFF) continue;
                        bool airAbove = c.Get(lx, y + 1, lz) == 0, airBelow = c.Get(lx, y - 1, lz) == 0;
                        if (!airAbove && !airBelow) continue;
                        int bio = c.Biome3D(lx, y, lz);
                        int wx = x0 + lx, wz = z0 + lz;
                        if (bio == lush)
                        {
                            if (airAbove)
                            {
                                c.SetRaw(lx, y, lz, rng.Chance(0.1f) ? clay : moss);
                                float r = rng.NextFloat();
                                if (r < 0.05f) c.SetRaw(lx, y + 1, lz, rng.NextBool() ? azalea : flowering);
                                else if (r < 0.25f) c.SetRaw(lx, y + 1, lz, mossCarpet);
                                else if (r < 0.28f && c.Get(lx, y + 2, lz) == 0) { c.SetRaw(lx, y + 1, lz, bigDrip); }
                                else if (r < 0.31f) c.SetRaw(lx, y + 1, lz, smallDrip);
                            }
                            else if (airBelow)
                            {
                                c.SetRaw(lx, y, lz, moss);
                                if (rng.Chance(0.12f)) { int len = rng.Range(1, 6); for (int k = 1; k <= len && c.Get(lx, y - k, lz) == 0; k++) c.SetRaw(lx, y - k, lz, (ushort)(glowBerries + (rng.Chance(0.3f) ? 1 : 0))); }
                                else if (rng.Chance(0.01f)) c.SetRaw(lx, y - 1, lz, spore);
                            }
                        }
                        else if (bio == drip)
                        {
                            if (rng.Chance(0.35f)) c.SetRaw(lx, y, lz, dripBlock);
                            if (airBelow && rng.Chance(0.1f)) { int len = rng.Range(1, 3); for (int k = 1; k <= len && c.Get(lx, y - k, lz) == 0; k++) c.SetRaw(lx, y - k, lz, pointed); }
                            if (airAbove && rng.Chance(0.07f)) { int len = rng.Range(1, 3); for (int k = 1; k <= len && c.Get(lx, y + k, lz) == 0; k++) c.SetRaw(lx, y + k, lz, (ushort)(pointed + 1)); }
                        }
                        else if (bio == dark)
                        {
                            if (airAbove && rng.Chance(0.7f))
                            {
                                c.SetRaw(lx, y, lz, sculk);
                                if (rng.Chance(0.015f)) c.SetRaw(lx, y + 1, lz, sculkSensor);
                                else if (rng.Chance(0.004f)) c.SetRaw(lx, y + 1, lz, shrieker);
                            }
                            else if (airBelow && rng.Chance(0.3f)) c.SetRaw(lx, y, lz, sculk);
                        }
                        else if (bio == sulfur)
                        {
                            // banded sulfur / cinnabar walls
                            int band = ((y % 7) + 7) % 7;
                            c.SetRaw(lx, y, lz, band < 3 ? sulfurS : (band == 3 ? cinnabar : (rng.Chance(0.5f) ? sulfurS : TUFF)));
                            if (airAbove && rng.Chance(0.04f)) c.SetRaw(lx, y, lz, potent);
                            if (airAbove && rng.Chance(0.03f)) c.SetRaw(lx, y + 1, lz, spike);
                        }
                        else if (airBelow && y < 40 && rng.Chance(0.004f)) c.SetRaw(lx, y - 1, lz, glowLichen);
                    }
                }
            // sulfur pools: water pockets on sulfur cave floors
            for (int k = 0; k < 2; k++)
            {
                int lx = rng.Range(2, 13), lz = rng.Range(2, 13);
                for (int y = world.minY + 10; y < c.genTopY[lz * 16 + lx] - 10; y++)
                {
                    if (c.Biome3D(lx, y, lz) != sulfur) continue;
                    if (c.Get(lx, y, lz) == 0 && c.Get(lx, y - 1, lz) != 0)
                    {
                        for (int dx = -2; dx <= 2; dx++) for (int dz = -2; dz <= 2; dz++)
                            {
                                if (dx * dx + dz * dz > 5) continue;
                                int px = lx + dx, pz = lz + dz;
                                if (c.Get(px, y - 1, pz) != 0 && c.Get(px, y, pz) == 0) { c.SetRaw(px, y - 1, pz, WATER); c.SetRaw(px, y - 2, pz, potent); }
                            }
                        break;
                    }
                }
            }
        }
    }
}
