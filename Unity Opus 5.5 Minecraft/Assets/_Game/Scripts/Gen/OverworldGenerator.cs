using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Seed-deterministic multi-noise Overworld generator (Y -64..320).
    /// Terrain = splined surface height from continentalness/erosion/peaks-valleys + 3D density for overhangs,
    /// sampled on a coarse 4x8x4 grid and trilinearly interpolated. Cheese/spaghetti caves on a 4x4x4 grid.
    /// </summary>
    public sealed partial class OverworldGenerator : WorldGenerator
    {
        readonly OctaveNoise contN, erosN, ridgeN, tempN, humN, riverN, detailN, dens3D, cheeseN, spagA, spagB, spagMask, caveBiomeA, caveBiomeB, bandsN, surfN, peakJag;
        public const int Sea = 63;
        readonly ushort STONE, DEEPSLATE, AIR, WATER, LAVA, BEDROCK, ICE, SNOW_LAYER, GRAVEL, SAND, CLAY, DIRT, GRASS, SANDSTONE, RED_SANDSTONE, TERRACOTTA, PACKED_ICE, SNOW_BLOCK, CALCITE, TUFF, MUD, MOSS, PODZOL, STONE_SURFACE, COARSE;
        readonly ushort[] bandStates;

        readonly Spline contSpline = new Spline(-1.1f, -44f, -0.6f, -32f, -0.42f, -22f, -0.25f, -12f, -0.16f, -4f, -0.11f, 0.5f, -0.05f, 2f, 0.05f, 4f, 0.25f, 10f, 0.5f, 18f, 1.1f, 30f);
        readonly Spline erosMountain = new Spline(-1f, 1f, -0.55f, 0.85f, -0.3f, 0.45f, 0f, 0.22f, 0.3f, 0.08f, 0.6f, 0.03f, 1f, 0f);

        public OverworldGenerator(World w) : base(w)
        {
            contN = new OctaveNoise(seed, 1, 1.0 / 1400.0, 1.0, 0.55, 0.3, 0.18, 0.1);
            erosN = new OctaveNoise(seed, 2, 1.0 / 900.0, 1.0, 0.5, 0.25, 0.12);
            ridgeN = new OctaveNoise(seed, 3, 1.0 / 520.0, 1.0, 0.45, 0.2, 0.1);
            tempN = new OctaveNoise(seed, 4, 1.0 / 1100.0, 1.0, 0.5, 0.25, 0.1);
            humN = new OctaveNoise(seed, 5, 1.0 / 900.0, 1.0, 0.5, 0.25, 0.1);
            riverN = new OctaveNoise(seed, 6, 1.0 / 650.0, 1.0, 0.4, 0.15);
            detailN = new OctaveNoise(seed, 7, 1.0 / 90.0, 1.0, 0.5, 0.25);
            dens3D = new OctaveNoise(seed, 8, 1.0 / 72.0, 1.0, 0.5, 0.25);
            cheeseN = new OctaveNoise(seed, 9, 1.0 / 70.0, 1.0, 0.5);
            spagA = new OctaveNoise(seed, 10, 1.0 / 58.0, 1.0, 0.35);
            spagB = new OctaveNoise(seed, 11, 1.0 / 58.0, 1.0, 0.35);
            spagMask = new OctaveNoise(seed, 12, 1.0 / 180.0, 1.0);
            caveBiomeA = new OctaveNoise(seed, 13, 1.0 / 260.0, 1.0, 0.5);
            caveBiomeB = new OctaveNoise(seed, 14, 1.0 / 260.0, 1.0, 0.5);
            bandsN = new OctaveNoise(seed, 15, 1.0 / 60.0, 1.0);
            surfN = new OctaveNoise(seed, 16, 1.0 / 24.0, 1.0, 0.5);
            peakJag = new OctaveNoise(seed, 17, 1.0 / 36.0, 1.0, 0.5, 0.25);
            STONE = S("stone"); DEEPSLATE = S("deepslate"); AIR = 0; WATER = Blocks.Water.baseState; LAVA = Blocks.Lava.baseState; BEDROCK = S("bedrock");
            ICE = S("ice"); SNOW_LAYER = S("snow"); GRAVEL = S("gravel"); SAND = S("sand"); CLAY = S("clay"); DIRT = S("dirt"); GRASS = S("grass_block");
            SANDSTONE = S("sandstone"); RED_SANDSTONE = S("red_sandstone"); TERRACOTTA = S("terracotta"); PACKED_ICE = S("packed_ice"); SNOW_BLOCK = S("snow_block");
            CALCITE = S("calcite"); TUFF = S("tuff"); MUD = S("mud"); MOSS = S("moss_block"); PODZOL = S("podzol"); COARSE = S("coarse_dirt");
            STONE_SURFACE = STONE;
            string[] bands = { "terracotta", "orange_terracotta", "yellow_terracotta", "terracotta", "brown_terracotta", "red_terracotta", "terracotta", "white_terracotta", "light_gray_terracotta", "orange_terracotta", "terracotta", "red_terracotta" };
            bandStates = new ushort[bands.Length];
            for (int i = 0; i < bands.Length; i++) bandStates[i] = S(bands[i]);
        }

        // ------------------------------------------------------------------ climate
        public struct Climate { public float c, e, pv, t, h, r, w; public float height, mountain; public int biome; }

        public Climate SampleClimate(int x, int z)
        {
            var k = new Climate();
            // domain warp for more natural coasts
            double wx = x + detailN.Sample(x * 0.5, z * 0.5) * 40, wz = z + detailN.Sample(z * 0.5 + 300, x * 0.5) * 40;
            k.c = (float)(contN.Sample(wx, wz) * 1.9 + 0.12);
            k.e = (float)(erosN.Sample(wx, wz) * 1.8);
            float w = (float)(ridgeN.Sample(wx, wz) * 1.8);
            k.w = w;
            k.pv = 1f - Mathf.Abs(3f * Mathf.Abs(w) - 2f); // folded: -1..1 (1 = peaks)
            k.t = (float)(tempN.Sample(x, z) * 1.9);
            k.h = (float)(humN.Sample(x, z) * 1.9);
            k.r = (float)riverN.Sample(wx, wz);
            // surface height
            float cont = contSpline.Eval(k.c);
            float inland = Mathf.Clamp01((k.c + 0.05f) / 0.35f);
            float mtn = erosMountain.Eval(k.e) * inland;
            float pv01 = Mathf.Clamp01((k.pv + 0.3f) / 1.3f);
            float peaks = mtn * (pv01 * pv01) * 150f;
            float hills = (float)detailN.Sample(x, z) * (5f + (1f - mtn) * 4f) + Mathf.Clamp01(k.e * -0.5f + 0.5f) * inland * pv01 * 18f;
            float h = Sea + cont + peaks + hills;
            // valleys along pv minima
            if (k.pv < -0.4f && inland > 0.2f) h -= (-0.4f - k.pv) * 14f * inland;
            // rivers carve toward sea level
            float riverW = 0.045f + (1f - inland) * 0.02f;
            float rv = Mathf.Abs(k.r);
            if (rv < riverW * 2.2f && k.c > -0.2f && h > Sea - 3)
            {
                float f = Mathf.Clamp01(1f - rv / (riverW * 2.2f));
                f = f * f * (3 - 2 * f);
                float riverBed = Sea - 4f - f * 2f;
                if (mtn < 0.5f) h = Mathf.Lerp(h, riverBed, f);
            }
            k.height = h;
            k.mountain = mtn;
            k.biome = PickBiome(ref k, x, z);
            return k;
        }

        int PickBiome(ref Climate k, int x, int z)
        {
            float t = k.t, hum = k.h;
            int tBand = t < -0.45f ? 0 : t < -0.15f ? 1 : t < 0.2f ? 2 : t < 0.55f ? 3 : 4; // frozen, cold, temperate, warm, hot
            int hBand = hum < -0.35f ? 0 : hum < -0.1f ? 1 : hum < 0.1f ? 2 : hum < 0.3f ? 3 : 4;
            float h = k.height;
            // oceans
            if (h < Sea - 1 && k.c < -0.13f)
            {
                bool deep = k.c < -0.42f;
                if (k.c < -0.95f && k.pv > 0.6f && tBand >= 2) return Biome.Id("mushroom_fields");
                switch (tBand)
                {
                    case 0: return Biome.Id("frozen_ocean");
                    case 1: return Biome.Id("cold_ocean");
                    case 2: return Biome.Id(deep ? "deep_ocean" : "ocean");
                    case 3: return Biome.Id("lukewarm_ocean");
                    default: return Biome.Id("warm_ocean");
                }
            }
            // rivers
            float rv = Mathf.Abs(k.r);
            if (rv < 0.05f && h < Sea + 1 && k.c > -0.2f && k.mountain < 0.5f) return Biome.Id(tBand == 0 ? "frozen_river" : "river");
            // coast
            if (h < Sea + 3 && k.c < 0.02f)
            {
                if (k.mountain > 0.4f) return Biome.Id("stony_shore");
                if (tBand == 0) return Biome.Id("snowy_beach");
                if (tBand >= 3 && hBand >= 4) return Biome.Id("mangrove_swamp");
                return Biome.Id("beach");
            }
            // peaks & slopes
            if (h > 165)
            {
                if (tBand <= 1) return Biome.Id(k.pv > 0.7f ? "jagged_peaks" : "frozen_peaks");
                if (tBand == 2) return Biome.Id("jagged_peaks");
                return Biome.Id("stony_peaks");
            }
            if (h > 125)
            {
                if (tBand <= 1) return Biome.Id(hum > 0 ? "grove" : "snowy_slopes");
                if (tBand == 2) return Biome.Id(hum > 0.2f ? "cherry_grove" : (hum > -0.2f ? "meadow" : "windswept_hills"));
                if (tBand == 3) return Biome.Id(hum > 0.25f ? "cherry_grove" : "meadow");
                return Biome.Id("savanna_plateau");
            }
            if (k.mountain > 0.55f && h > 95)
            {
                if (tBand <= 1) return Biome.Id("windswept_gravelly_hills");
                return Biome.Id("windswept_hills");
            }
            // swamps in low wet areas near coast
            if (h < Sea + 5 && hBand >= 3 && tBand >= 2 && k.c < 0.2f && k.mountain < 0.3f) return Biome.Id(tBand >= 3 ? "mangrove_swamp" : "swamp");
            switch (tBand)
            {
                case 0:
                    if (hBand <= 1 && k.w > 0.6f) return Biome.Id("ice_spikes");
                    return Biome.Id(hBand >= 3 ? "snowy_taiga" : "snowy_plains");
                case 1:
                    if (hBand >= 4) return Biome.Id("old_growth_spruce_taiga");
                    if (hBand <= 0) return Biome.Id("plains");
                    return Biome.Id("taiga");
                case 2:
                    if (hBand == 0) return Biome.Id(k.w > 0.5f ? "sunflower_plains" : "plains");
                    if (hBand == 1) return Biome.Id(k.w > 0.3f ? "flower_forest" : "plains");
                    if (hBand == 2) return Biome.Id(k.w > 0.2f ? "birch_forest" : "forest");
                    if (hBand == 3) return Biome.Id("forest");
                    return Biome.Id(k.w > 0.35f ? "pale_garden" : "dark_forest");
                case 3:
                    if (hBand <= 1) return Biome.Id("savanna");
                    if (hBand == 2) return Biome.Id(k.w > 0.3f ? "cherry_grove" : "forest");
                    if (hBand == 3) return Biome.Id("sparse_jungle");
                    return Biome.Id(k.w > 0.45f ? "bamboo_jungle" : "jungle");
                default:
                    if (hBand <= 1) return Biome.Id("desert");
                    if (hBand == 2) return Biome.Id(k.e > 0.4f ? "eroded_badlands" : "badlands");
                    if (hBand == 3) return Biome.Id("wooded_badlands");
                    return Biome.Id("savanna");
            }
        }

        public override int ApproxSurface(int x, int z) => Mathf.RoundToInt(SampleClimate(x, z).height);
        public override string BiomeAtApprox(int x, int z) => Biome.Get(SampleClimate(x, z).biome).key;

        // ------------------------------------------------------------------ terrain
        [ThreadStatic] static float[] tlsDensity;
        [ThreadStatic] static float[] tlsCave;
        [ThreadStatic] static Climate[] tlsClimate;

        public override void GenerateTerrain(Chunk c)
        {
            int x0 = c.cx << 4, z0 = c.cz << 4;
            int minY = world.minY, H = world.height;
            var clim = tlsClimate ?? (tlsClimate = new Climate[256]);
            float maxH = -999, minH = 999;
            for (int lz = 0; lz < 16; lz++)
                for (int lx = 0; lx < 16; lx++)
                {
                    var k = SampleClimate(x0 + lx, z0 + lz);
                    clim[lz * 16 + lx] = k;
                    c.biomes2D[lz * 16 + lx] = (byte)k.biome;
                    if (k.height > maxH) maxH = k.height;
                    if (k.height < minH) minH = k.height;
                }
            // corner climates for density grid (5x5)
            var cornerH = new float[25]; var cornerAmp = new float[25];
            for (int j = 0; j < 5; j++)
                for (int i = 0; i < 5; i++)
                {
                    Climate k;
                    if (i < 4 && j < 4) k = clim[(j * 4) * 16 + i * 4];
                    else k = SampleClimate(x0 + i * 4, z0 + j * 4);
                    cornerH[j * 5 + i] = k.height;
                    cornerAmp[j * 5 + i] = 3f + k.mountain * 26f + Mathf.Clamp01(k.pv) * k.mountain * 10f;
                }
            // density grid: 5 x NY x 5 at y step 8
            int NY = H / 8 + 1;
            var dens = tlsDensity;
            if (dens == null || dens.Length < 25 * NY) dens = tlsDensity = new float[25 * NY];
            for (int j = 0; j < 5; j++)
                for (int i = 0; i < 5; i++)
                {
                    float h = cornerH[j * 5 + i], amp = cornerAmp[j * 5 + i];
                    int wx = x0 + i * 4, wz = z0 + j * 4;
                    for (int k = 0; k < NY; k++)
                    {
                        int y = minY + k * 8;
                        float d = h - y;
                        if (d > amp * 1.6f + 8 || d < -amp * 1.6f - 8) { dens[(k * 5 + j) * 5 + i] = d; continue; }
                        float n = (float)dens3D.Sample(wx, y * 1.3, wz) * 1.7f;
                        // jagged peaks: sharpen high terrain
                        if (h > 150) n += (float)peakJag.Sample(wx, y, wz) * 0.6f;
                        dens[(k * 5 + j) * 5 + i] = d + n * amp;
                    }
                }
            // cave grid (4x4x4)
            int top = Mathf.Min(world.maxY - 1, Mathf.CeilToInt(maxH + 40));
            int CY = (top - minY) / 4 + 2;
            var cave = tlsCave;
            if (cave == null || cave.Length < 25 * CY) cave = tlsCave = new float[25 * CY];
            for (int j = 0; j < 5; j++)
                for (int i = 0; i < 5; i++)
                {
                    int wx = x0 + i * 4, wz = z0 + j * 4;
                    float h = cornerH[j * 5 + i];
                    double mask = spagMask.Sample(wx, wz);
                    for (int k = 0; k < CY; k++)
                    {
                        int y = minY + k * 4;
                        float v = 1f; // >0 = solid, <0 = carve
                        if (y < h + 8 && y > minY + 4)
                        {
                            // cheese caverns (mostly deep)
                            float depthF = Mathf.Clamp01((h - y - 12) / 40f);
                            if (depthF > 0)
                            {
                                float ch = (float)cheeseN.Sample(wx, y * 1.6, wz);
                                float thr = 0.34f - depthF * 0.08f + (y < 0 ? -0.04f : 0.03f);
                                v = Mathf.Min(v, (thr - ch) * 6f);
                            }
                            // spaghetti tunnels
                            float a = (float)spagA.Sample(wx, y * 1.4, wz), b = (float)spagB.Sample(wx, y * 1.4, wz);
                            float r2 = a * a + b * b;
                            float width = 0.0045f + (float)(mask * 0.5 + 0.5) * 0.004f;
                            v = Mathf.Min(v, (r2 - width) * 180f);
                        }
                        cave[(k * 5 + j) * 5 + i] = v;
                    }
                }
            // fill
            var rng = new RNG(seed, c.cx, c.cz, 101);
            for (int lz = 0; lz < 16; lz++)
                for (int lx = 0; lx < 16; lx++)
                {
                    var k = clim[lz * 16 + lx];
                    float fx = lx / 4f, fz = lz / 4f;
                    int gi = (int)fx, gj = (int)fz; float tx = fx - gi, tz = fz - gj;
                    int colTop = minY - 1;
                    bool underwaterSurface = false;
                    for (int ky = 0; ky < NY - 1; ky++)
                    {
                        float d000 = dens[(ky * 5 + gj) * 5 + gi], d100 = dens[(ky * 5 + gj) * 5 + gi + 1], d010 = dens[(ky * 5 + gj + 1) * 5 + gi], d110 = dens[(ky * 5 + gj + 1) * 5 + gi + 1];
                        float d001 = dens[((ky + 1) * 5 + gj) * 5 + gi], d101 = dens[((ky + 1) * 5 + gj) * 5 + gi + 1], d011 = dens[((ky + 1) * 5 + gj + 1) * 5 + gi], d111 = dens[((ky + 1) * 5 + gj + 1) * 5 + gi + 1];
                        float lo = Mathf.Lerp(Mathf.Lerp(d000, d100, tx), Mathf.Lerp(d010, d110, tx), tz);
                        float hi = Mathf.Lerp(Mathf.Lerp(d001, d101, tx), Mathf.Lerp(d011, d111, tx), tz);
                        for (int sy = 0; sy < 8; sy++)
                        {
                            int y = minY + ky * 8 + sy;
                            float d = Mathf.Lerp(lo, hi, sy / 8f);
                            ushort s = 0;
                            if (d > 0)
                            {
                                s = y < 0 ? DEEPSLATE : STONE;
                                if (y >= 0 && y < 8 && rng.Next(8) >= y) s = DEEPSLATE;
                                if (y > colTop) colTop = y;
                            }
                            else if (y <= Sea) s = WATER;
                            if (s != 0) c.SetRaw(lx, y, lz, s);
                        }
                    }
                    // caves
                    for (int y = minY + 1; y <= colTop; y++)
                    {
                        int ky = (y - minY) >> 2; if (ky >= CY - 1) break;
                        float ty = ((y - minY) & 3) / 4f;
                        float c000 = cave[(ky * 5 + gj) * 5 + gi], c100 = cave[(ky * 5 + gj) * 5 + gi + 1], c010 = cave[(ky * 5 + gj + 1) * 5 + gi], c110 = cave[(ky * 5 + gj + 1) * 5 + gi + 1];
                        float c001 = cave[((ky + 1) * 5 + gj) * 5 + gi], c101 = cave[((ky + 1) * 5 + gj) * 5 + gi + 1], c011 = cave[((ky + 1) * 5 + gj + 1) * 5 + gi], c111 = cave[((ky + 1) * 5 + gj + 1) * 5 + gi + 1];
                        float lo = Mathf.Lerp(Mathf.Lerp(c000, c100, tx), Mathf.Lerp(c010, c110, tx), tz);
                        float hi = Mathf.Lerp(Mathf.Lerp(c001, c101, tx), Mathf.Lerp(c011, c111, tx), tz);
                        float cv = Mathf.Lerp(lo, hi, ty);
                        if (cv >= 0) continue;
                        ushort cur = c.Get(lx, y, lz);
                        if (cur == 0 || cur == WATER) continue;
                        // don't breach ocean/river floors
                        if (k.height < Sea + 2 && y > k.height - 6) continue;
                        // keep a crust under water columns
                        if (c.Get(lx, y + 1, lz) == WATER) continue;
                        c.SetRaw(lx, y, lz, y <= -55 ? LAVA : (ushort)0);
                    }
                    // bedrock
                    for (int y = minY; y < minY + 5; y++) if (y == minY || rng.Next(5) >= y - minY) c.SetRaw(lx, y, lz, BEDROCK);
                    // surface rules
                    ApplySurface(c, lx, lz, x0 + lx, z0 + lz, k, ref rng);
                }
            BuildCaveBiomes(c);
            // record immutable generation info
            for (int lz = 0; lz < 16; lz++)
                for (int lx = 0; lx < 16; lx++)
                {
                    int i = lz * 16 + lx;
                    int y = world.maxY - 1;
                    while (y > minY)
                    {
                        ushort s = c.Get(lx, y, lz);
                        if (s != 0 && s != WATER && !Blocks.ByState[s].isLiquid) break;
                        y--;
                    }
                    c.genTopY[i] = (short)y;
                    c.genTop[i] = c.Get(lx, y, lz);
                    int wd = 0; while (Blocks.ByState[c.Get(lx, y + 1 + wd, lz)].isLiquid && wd < 255) wd++;
                    c.genWater[i] = (byte)wd;
                }
        }

        void ApplySurface(Chunk c, int lx, int lz, int wx, int wz, Climate k, ref RNG rng)
        {
            var b = Biome.Get(k.biome);
            int minY = world.minY;
            int y = world.maxY - 1;
            // find first solid from top
            while (y > minY && (c.Get(lx, y, lz) == 0 || c.Get(lx, y, lz) == WATER)) y--;
            if (y <= minY) return;
            bool underwater = c.Get(lx, y + 1, lz) == WATER;
            float sn = (float)surfN.Sample(wx, wz);
            // steepness
            float steep = 0;
            {
                var kx = SampleHeightOnly(wx + 2, wz); var kz = SampleHeightOnly(wx, wz + 2);
                steep = Mathf.Max(Mathf.Abs(kx - k.height), Mathf.Abs(kz - k.height)) / 2f;
            }
            ushort top, fill; int depth = b.fillerDepth + (int)(sn * 2 + 1);
            if (b.key == "badlands" || b.key == "eroded_badlands" || b.key == "wooded_badlands")
            {
                // terracotta bands
                for (int d = 0; d < 40 && y - d > minY; d++)
                {
                    int yy = y - d;
                    ushort cur = c.Get(lx, yy, lz);
                    if (cur != STONE && cur != DEEPSLATE) break;
                    ushort s;
                    if (d == 0 && !underwater && yy < 90) s = b.key == "wooded_badlands" && yy > 70 ? COARSE : S("red_sand");
                    else if (d < 3 && yy < 70) s = RED_SANDSTONE;
                    else { int band = ((yy + (int)(bandsN.Sample(wx, wz) * 4)) % bandStates.Length + bandStates.Length) % bandStates.Length; s = bandStates[band]; }
                    c.SetRaw(lx, yy, lz, s);
                }
                if (b.key == "eroded_badlands" && k.height > 70)
                {
                    // hoodoo spires
                    float sp = (float)peakJag.Sample(wx * 2, wz * 2);
                    if (sp > 0.25f)
                    {
                        int hgt = (int)((sp - 0.25f) * 60);
                        for (int d = 1; d <= hgt && y + d < world.maxY; d++) { int band = ((y + d) % bandStates.Length + bandStates.Length) % bandStates.Length; c.SetRaw(lx, y + d, lz, bandStates[band]); }
                    }
                }
                return;
            }
            if (underwater)
            {
                bool deep = c.Get(lx, y + 8, lz) == WATER;
                string u = b.underwater;
                if (b.isOcean && !deep && (b.key == "ocean" || b.key == "lukewarm_ocean" || b.key == "cold_ocean")) u = sn > 0.2f ? "sand" : "gravel";
                if (b.key == "swamp" || b.key == "mangrove_swamp") u = sn > 0.3f ? "clay" : (b.key == "mangrove_swamp" ? "mud" : "dirt");
                if (b.isRiver && sn > 0.5f) u = "clay";
                top = S(u); fill = u == "sand" ? SAND : (u == "mud" ? MUD : (u == "clay" ? CLAY : GRAVEL));
                depth = 3;
            }
            else
            {
                top = S(b.top); fill = S(b.filler);
                if (b.top == "grass_block" && (b.key == "old_growth_spruce_taiga" || b.key == "bamboo_jungle") && sn > 0.1f) top = PODZOL;
                if (b.key == "old_growth_spruce_taiga" && sn < -0.4f) top = COARSE;
                if (b.key == "savanna" && sn > 0.55f) top = COARSE;
                // mountains: stone faces on steep slopes, snow caps
                if (b.isMountain || k.height > 110)
                {
                    if (steep > 2.2f) { top = STONE_SURFACE; fill = STONE; }
                    if (b.key == "stony_peaks") { top = sn > 0.3f ? CALCITE : STONE; fill = STONE; }
                    if (b.key == "jagged_peaks" || b.key == "frozen_peaks" || b.key == "snowy_slopes" || b.key == "grove")
                    {
                        if (steep > 3f) { top = b.key == "frozen_peaks" && sn > 0.2f ? PACKED_ICE : STONE; fill = STONE; }
                        else { top = SNOW_BLOCK; fill = b.key == "frozen_peaks" && sn > 0 ? PACKED_ICE : SNOW_BLOCK; }
                    }
                    if (b.key == "windswept_gravelly_hills" && sn > -0.2f) { top = GRAVEL; fill = GRAVEL; }
                }
                if (b.key == "desert") { fill = SAND; }
            }
            bool placedTop = false;
            for (int d = 0; d < depth + 4 && y - d > minY; d++)
            {
                int yy = y - d;
                ushort cur = c.Get(lx, yy, lz);
                if (cur != STONE && cur != DEEPSLATE) { if (cur == 0 || cur == WATER) break; else continue; }
                if (!placedTop) { c.SetRaw(lx, yy, lz, top); placedTop = true; continue; }
                if (d <= depth) c.SetRaw(lx, yy, lz, fill);
                else if ((fill == SAND) && d <= depth + 3) c.SetRaw(lx, yy, lz, SANDSTONE);
            }
            // snow & ice
            bool cold = b.snowy || b.frozenWater || k.height > 185;
            if (cold)
            {
                if (underwater)
                {
                    if (c.Get(lx, y + 1, lz) == WATER)
                    {
                        int ws = y + 1; while (ws + 1 < world.maxY && c.Get(lx, ws + 1, lz) == WATER) ws++;
                        if (ws >= Sea - 1) c.SetRaw(lx, ws, lz, ICE);
                    }
                }
                else if (c.Get(lx, y + 1, lz) == 0)
                {
                    ushort ts = c.Get(lx, y, lz);
                    if (ts == GRASS) c.SetRaw(lx, y, lz, (ushort)(GRASS + 1)); // snowy grass state
                    if (ts != ICE && ts != PACKED_ICE) c.SetRaw(lx, y + 1, lz, SNOW_LAYER);
                }
            }
        }

        float SampleHeightOnly(int x, int z) => SampleClimate(x, z).height;

        void BuildCaveBiomes(Chunk c)
        {
            int qn = world.height / 4;
            var b3 = new byte[qn * 16];
            int x0 = c.cx << 4, z0 = c.cz << 4;
            int lush = Biome.Lush_Caves.id, drip = Biome.Dripstone_Caves.id, dark = Biome.Deep_Dark.id, sulfur = Biome.Sulfur_Caves.id;
            for (int qz = 0; qz < 4; qz++)
                for (int qx = 0; qx < 4; qx++)
                {
                    int lx = qx * 4 + 2, lz = qz * 4 + 2;
                    int surface = c.genTopY[lz * 16 + lx] != 0 ? c.genTopY[lz * 16 + lx] : 64;
                    // genTopY not computed yet at this point for all columns -> use climate height
                    var k = SampleClimate(x0 + lx, z0 + lz);
                    surface = (int)k.height;
                    byte surf = c.biomes2D[lz * 16 + lx];
                    for (int qy = 0; qy < qn; qy++)
                    {
                        int y = world.minY + qy * 4 + 2;
                        byte id = surf;
                        if (y < surface - 18)
                        {
                            float a = (float)caveBiomeA.Sample(x0 + lx, y * 2, z0 + lz), bb = (float)caveBiomeB.Sample(x0 + lx, y * 2, z0 + lz);
                            if (y < -8 && a > 0.18f && k.e < 0.2f) id = (byte)dark;
                            else if (bb > 0.26f && y > -30) id = (byte)lush;
                            else if (bb < -0.26f) id = (byte)drip;
                            else if (a < -0.3f && y > -20 && y < 50) id = (byte)sulfur;
                        }
                        b3[(qy << 4) | (qz << 2) | qx] = id;
                    }
                }
            c.biomes3D = b3;
        }

        public override Vector3 FindSpawn()
        {
            // search outward for dry land in a pleasant biome
            for (int r = 0; r < 2000; r += 16)
                for (int a = 0; a < 16; a++)
                {
                    float ang = a / 16f * Mathf.PI * 2;
                    int x = Mathf.RoundToInt(Mathf.Cos(ang) * r), z = Mathf.RoundToInt(Mathf.Sin(ang) * r);
                    var k = SampleClimate(x, z);
                    var b = Biome.Get(k.biome);
                    if (k.height > Sea + 2 && k.height < 110 && !b.isOcean && !b.isRiver && (b.key == "plains" || b.key == "forest" || b.key == "flower_forest" || b.key == "birch_forest" || b.key == "meadow" || b.key == "sunflower_plains" || b.key == "cherry_grove" || b.key == "savanna" || b.key == "taiga"))
                        return new Vector3(x + 0.5f, k.height + 2, z + 0.5f);
                }
            return new Vector3(0.5f, 90, 0.5f);
        }
    }
}
