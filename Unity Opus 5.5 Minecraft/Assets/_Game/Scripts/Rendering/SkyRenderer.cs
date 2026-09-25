using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace MCR
{
    /// <summary>
    /// Draws the sky itself — gradient dome, sun, moon, stars, cloud layer, weather sheets and the dimension
    /// backdrops — plus the world-space overlays that belong to the camera (block outline, hitboxes, chunk
    /// borders). Everything is drawn with the unlit shader into camera-facing meshes, so no art assets are needed.
    /// </summary>
    public sealed partial class GameManager
    {
        SkyRenderer sky;

        void CreateSky()
        {
            sky = new SkyRenderer();
            sky.Build();
        }

        void SkyTick()
        {
            if (sky == null) CreateSky();
        }

        void RenderSky(Camera cam, float partial)
        {
            if (sky == null) CreateSky();
            var w = ActiveWorld;
            sky?.Render(cam, w, session, player, partial);
        }
    }

    /// <summary>Runtime-built sky meshes and the per-frame lighting handoff.</summary>
    public sealed class SkyRenderer
    {
        Mesh dome, quad, cloudMesh, starMesh, rainMesh, snowMesh;
        Material domeMat, sunMat, moonMat, starMat, cloudMat, cloudDepthMat, rainMat, snowMat, horizonMat, outlineMat, faceMat, voidMat;
        Texture2D sunTex, moonTex, starTex, rainTex, snowTex;
        float flash, thunderFade;
        readonly Vector3[] starDirs = new Vector3[1500];
        readonly float[] starSizes = new float[1500];
        float cloudScroll;
        Texture2D[] moonPhases;
        Texture2D endSkyTex;
        GameObject root;
        public bool visible = true;

        public void Build()
        {
            root = new GameObject("Sky");
            Object.DontDestroyOnLoad(root);
            dome = BuildDome();
            quad = BuildQuad();
            starMesh = BuildStars();
            cloudMesh = BuildClouds();
            sunTex = PaintSun(); moonTex = PaintMoon(); starTex = PaintCircle(8, 1f); rainTex = PaintRain(false); snowTex = PaintRain(true);
            domeMat = Res.UnlitMaterial(null, false, false, 10);
            domeMat.SetFloat("_ZTest", (float)CompareFunction.LessEqual);
            sunMat = Res.UnlitMaterial(sunTex, true, false, 11);
            moonMat = Res.UnlitMaterial(moonTex, true, false, 11);
            starMat = Res.UnlitMaterial(starTex, true, false, 11);
            // clouds: a depth-only pass first, so the colour pass only keeps the nearest face of each cloud
            cloudDepthMat = Res.UnlitMaterial(Texture2D.whiteTexture, false, true, 2990, false);
            cloudDepthMat.SetFloat("_ZWrite", 1f);
            cloudDepthMat.SetFloat("_ColorMask", 0f);
            cloudMat = Res.UnlitMaterial(Texture2D.whiteTexture, false, true, 2991, false);
            cloudMat.SetFloat("_Fog", 3f);
            rainMat = Res.UnlitMaterial(rainTex, false, true, 3050, false);
            rainMat.SetFloat("_Fog", 1f);
            snowMat = Res.UnlitMaterial(snowTex, false, true, 3050, false);
            snowMat.SetFloat("_Fog", 1f);
            rainMesh = new Mesh { name = "weather_rain" }; rainMesh.MarkDynamic();
            snowMesh = new Mesh { name = "weather_snow" }; snowMesh.MarkDynamic();
            horizonMat = Res.UnlitMaterial(null, false, false, 11);
            outlineMat = Res.UnlitMaterial(Texture2D.whiteTexture, false, true, 4000, false);
            faceMat = Res.UnlitMaterial(Texture2D.whiteTexture, false, true, 4001, false);
            voidMat = Res.UnlitMaterial(null, false, false, 9);
        }

        public void Flash() { flash = 1f; thunderFade = 1f; }

        // ------------------------------------------------------------------ meshes
        const int DomeSeg = 32;
        static readonly float[] DomeRings = { -0.25f, 0f, 0.08f, 0.22f, 0.45f, 0.75f, 1f };
        Color[] domeColors;

        /// <summary>A hemisphere (plus a skirt below the horizon) built from latitude rings; colours are set per frame.</summary>
        static Mesh BuildDome()
        {
            int rings = DomeRings.Length;
            var verts = new Vector3[rings * DomeSeg + 1];
            var tris = new List<int>();
            for (int r = 0; r < rings; r++)
            {
                float elev = DomeRings[r] * Mathf.PI * 0.5f;
                float y = Mathf.Sin(elev), rad = Mathf.Cos(elev);
                for (int i = 0; i < DomeSeg; i++)
                {
                    float a = i / (float)DomeSeg * Mathf.PI * 2f;
                    verts[r * DomeSeg + i] = new Vector3(Mathf.Cos(a) * rad, y, Mathf.Sin(a) * rad);
                }
            }
            verts[rings * DomeSeg] = Vector3.up;
            for (int r = 0; r < rings - 1; r++)
                for (int i = 0; i < DomeSeg; i++)
                {
                    int a = r * DomeSeg + i, b = r * DomeSeg + (i + 1) % DomeSeg;
                    int c = (r + 1) * DomeSeg + i, d = (r + 1) * DomeSeg + (i + 1) % DomeSeg;
                    tris.Add(a); tris.Add(c); tris.Add(b);
                    tris.Add(b); tris.Add(c); tris.Add(d);
                }
            var m = new Mesh { name = "sky_dome" };
            m.vertices = verts;
            m.colors = new Color[verts.Length];
            // wrap-around uvs so a tiling texture can cover the dome (used by the End sky)
            var uvs = new Vector2[verts.Length];
            for (int i = 0; i < verts.Length; i++)
            {
                var d = verts[i];
                uvs[i] = new Vector2((Mathf.Atan2(d.x, d.z) / (Mathf.PI * 2f) + 0.5f) * 12f, d.y * 5f);
            }
            m.uv = uvs;
            m.SetTriangles(tris, 0);
            m.bounds = new Bounds(Vector3.zero, Vector3.one * 4f);
            return m;
        }

        void ColorDome(Color horizon, Color top, Color below)
        {
            int rings = DomeRings.Length;
            if (domeColors == null) domeColors = new Color[rings * DomeSeg + 1];
            for (int r = 0; r < rings; r++)
            {
                float e = DomeRings[r];
                // the fog colour holds near the horizon and gives way to the sky colour higher up
                Color c = e < 0f ? below : Color.Lerp(horizon, top, Mathf.SmoothStep(0f, 1f, Mathf.Clamp01(e / 0.45f)));
                for (int i = 0; i < DomeSeg; i++) domeColors[r * DomeSeg + i] = c;
            }
            domeColors[rings * DomeSeg] = top;
            dome.colors = domeColors;
        }

        static Mesh BuildQuad()
        {
            var m = new Mesh { name = "sky_quad" };
            m.vertices = new[] { new Vector3(-1, -1, 0), new Vector3(1, -1, 0), new Vector3(1, 1, 0), new Vector3(-1, 1, 0) };
            m.uv = new[] { new Vector2(0, 0), new Vector2(1, 0), new Vector2(1, 1), new Vector2(0, 1) };
            m.triangles = new[] { 0, 1, 2, 0, 2, 3 };
            m.RecalculateBounds();
            return m;
        }

        Mesh BuildStars()
        {
            var rng = new RNG(12345);
            var verts = new Vector3[1500 * 4];
            var uvs = new Vector2[1500 * 4];
            var tris = new int[1500 * 6];
            for (int i = 0; i < 1500; i++)
            {
                // uniform directions on the upper hemisphere, biased upward so few sit behind the horizon
                float u = rng.NextFloat() * 2f - 1f, phi = rng.NextFloat() * Mathf.PI * 2f;
                float s = Mathf.Sqrt(1f - u * u);
                var dir = new Vector3(s * Mathf.Cos(phi), Mathf.Abs(u) * 0.9f + 0.1f, s * Mathf.Sin(phi)).normalized;
                starDirs[i] = dir;
                starSizes[i] = 1.6f + rng.NextFloat() * 2.6f;
                var right = Vector3.Cross(dir, Vector3.up).normalized * 0.5f;
                var up = Vector3.Cross(dir, right).normalized * 0.5f;
                int v = i * 4;
                verts[v + 0] = dir - right - up; verts[v + 1] = dir + right - up; verts[v + 2] = dir + right + up; verts[v + 3] = dir - right + up;
                uvs[v + 0] = new Vector2(0, 0); uvs[v + 1] = new Vector2(1, 0); uvs[v + 2] = new Vector2(1, 1); uvs[v + 3] = new Vector2(0, 1);
                tris[i * 6 + 0] = v; tris[i * 6 + 1] = v + 1; tris[i * 6 + 2] = v + 2;
                tris[i * 6 + 3] = v; tris[i * 6 + 4] = v + 2; tris[i * 6 + 5] = v + 3;
            }
            var m = new Mesh { name = "sky_stars" };
            m.vertices = verts; m.uv = uvs; m.triangles = tris; m.RecalculateBounds();
            return m;
        }

        // clouds: a wrapping map of 12x12-block cells, four blocks thick, drawn as a tiled layer of boxes
        const int CloudCells = 64;
        const float CloudCell = 12f, CloudThickness = 4f;
        const float CloudTile = CloudCells * CloudCell;

        static bool[] CloudMap()
        {
            // two octaves of value noise on the cell grid, thresholded to roughly a third coverage; the noise
            // lattice wraps so the tile joins its neighbours seamlessly
            var rng = new RNG(20260923);
            const int L1 = 8, L2 = 16;
            var g1 = new float[L1 * L1]; var g2 = new float[L2 * L2];
            for (int i = 0; i < g1.Length; i++) g1[i] = rng.NextFloat();
            for (int i = 0; i < g2.Length; i++) g2[i] = rng.NextFloat();
            float Sample(float[] g, int L, float x, float y)
            {
                int x0 = Mathf.FloorToInt(x), y0 = Mathf.FloorToInt(y);
                float fx = x - x0, fy = y - y0;
                fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
                float V(int ix, int iy) => g[((iy % L + L) % L) * L + ((ix % L + L) % L)];
                return Mathf.Lerp(Mathf.Lerp(V(x0, y0), V(x0 + 1, y0), fx), Mathf.Lerp(V(x0, y0 + 1), V(x0 + 1, y0 + 1), fx), fy);
            }
            var map = new bool[CloudCells * CloudCells];
            for (int z = 0; z < CloudCells; z++)
                for (int x = 0; x < CloudCells; x++)
                {
                    float n = Sample(g1, L1, x * L1 / (float)CloudCells, z * L1 / (float)CloudCells) * 0.65f
                            + Sample(g2, L2, x * L2 / (float)CloudCells, z * L2 / (float)CloudCells) * 0.35f;
                    map[z * CloudCells + x] = n > 0.56f;
                }
            return map;
        }

        static Mesh BuildClouds()
        {
            var map = CloudMap();
            bool Cell(int x, int z) => map[((z % CloudCells + CloudCells) % CloudCells) * CloudCells + ((x % CloudCells + CloudCells) % CloudCells)];
            var verts = new List<Vector3>(); var cols = new List<Color>(); var uvs = new List<Vector2>(); var tris = new List<int>();
            // shading per face direction (linear values): lit tops, darker undersides, two side tones
            Color top = new Color(1f, 1f, 1f), bottom = new Color(0.45f, 0.45f, 0.47f), sideZ = new Color(0.79f, 0.79f, 0.80f), sideX = new Color(0.62f, 0.62f, 0.64f);
            void Quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color col)
            {
                int i = verts.Count;
                verts.Add(a); verts.Add(b); verts.Add(c); verts.Add(d);
                for (int k = 0; k < 4; k++) { cols.Add(col); uvs.Add(new Vector2(0.5f, 0.5f)); }
                tris.Add(i); tris.Add(i + 1); tris.Add(i + 2); tris.Add(i); tris.Add(i + 2); tris.Add(i + 3);
            }
            for (int z = 0; z < CloudCells; z++)
                for (int x = 0; x < CloudCells; x++)
                {
                    if (!Cell(x, z)) continue;
                    float x0 = x * CloudCell, x1 = x0 + CloudCell, z0 = z * CloudCell, z1 = z0 + CloudCell, y0 = 0f, y1 = CloudThickness;
                    Quad(new Vector3(x0, y1, z0), new Vector3(x0, y1, z1), new Vector3(x1, y1, z1), new Vector3(x1, y1, z0), top);
                    Quad(new Vector3(x0, y0, z0), new Vector3(x1, y0, z0), new Vector3(x1, y0, z1), new Vector3(x0, y0, z1), bottom);
                    if (!Cell(x, z - 1)) Quad(new Vector3(x0, y0, z0), new Vector3(x0, y1, z0), new Vector3(x1, y1, z0), new Vector3(x1, y0, z0), sideZ);
                    if (!Cell(x, z + 1)) Quad(new Vector3(x1, y0, z1), new Vector3(x1, y1, z1), new Vector3(x0, y1, z1), new Vector3(x0, y0, z1), sideZ);
                    if (!Cell(x - 1, z)) Quad(new Vector3(x0, y0, z1), new Vector3(x0, y1, z1), new Vector3(x0, y1, z0), new Vector3(x0, y0, z0), sideX);
                    if (!Cell(x + 1, z)) Quad(new Vector3(x1, y0, z0), new Vector3(x1, y1, z0), new Vector3(x1, y1, z1), new Vector3(x1, y0, z1), sideX);
                }
            var m = new Mesh { name = "sky_clouds", indexFormat = IndexFormat.UInt32 };
            m.SetVertices(verts); m.SetColors(cols); m.SetUVs(0, uvs); m.SetTriangles(tris, 0);
            m.RecalculateBounds();
            return m;
        }

        // ------------------------------------------------------------------ weather
        readonly List<Vector3> wVerts = new List<Vector3>();
        readonly List<Color> wCols = new List<Color>();
        readonly List<Vector2> wUvs = new List<Vector2>();
        readonly List<int> wTris = new List<int>();

        /// <summary>
        /// One camera-facing sheet per block column around the viewer, from the height where the column stops
        /// the rain up past the camera, so roofs keep it off. Rain streaks scroll fast; snow drifts and sways.
        /// </summary>
        void BuildWeather(Mesh mesh, World w, Vector3 camPos, bool snow, float strength, float time)
        {
            wVerts.Clear(); wCols.Clear(); wUvs.Clear(); wTris.Clear();
            const int R = 10;
            int cx = Mathf.FloorToInt(camPos.x), cz = Mathf.FloorToInt(camPos.z);
            float camY = camPos.y;
            for (int dz = -R; dz <= R; dz++)
                for (int dx = -R; dx <= R; dx++)
                {
                    float dist = Mathf.Sqrt(dx * dx + dz * dz);
                    if (dist > R) continue;
                    int x = cx + dx, z = cz + dz;
                    var biome = w.GetBiome(new Int3(x, Mathf.FloorToInt(camY), z));
                    if (biome == null || biome.precipitation == Precipitation.None) continue;
                    bool snowy = biome.precipitation == Precipitation.Snow || biome.snowy;
                    if (snowy != snow) continue;
                    float floor = w.SkyHeight(x, z);
                    float y0 = Mathf.Max(floor, camY - R), y1 = camY + R;
                    if (y1 <= y0 + 0.01f) continue;
                    float px = x + 0.5f, pz = z + 0.5f;
                    var toCam = new Vector2(camPos.x - px, camPos.z - pz);
                    if (toCam.sqrMagnitude < 0.0001f) toCam = Vector2.up;
                    toCam.Normalize();
                    var right = new Vector3(-toCam.y, 0, toCam.x) * 0.5f;
                    uint h = (uint)(x * 73856093) ^ (uint)(z * 19349663);
                    float seed = (h % 1000) / 1000f;
                    float scroll = snow ? time * 0.35f + seed : time * 2.6f + seed * 7f;
                    float sway = snow ? Mathf.Sin(time * 0.8f + seed * 6.28f) * 0.35f : 0f;
                    float a = Mathf.Clamp01(1f - dist / R) * strength * (snow ? 0.9f : 0.7f);
                    int i = wVerts.Count;
                    var c = new Vector3(px, 0, pz);
                    wVerts.Add(c - right + Vector3.up * y0); wVerts.Add(c - right + Vector3.up * y1);
                    wVerts.Add(c + right + Vector3.up * y1); wVerts.Add(c + right + Vector3.up * y0);
                    float v0 = y0 * 0.25f + scroll, v1 = y1 * 0.25f + scroll;
                    wUvs.Add(new Vector2(sway, v0)); wUvs.Add(new Vector2(sway, v1)); wUvs.Add(new Vector2(1f + sway, v1)); wUvs.Add(new Vector2(1f + sway, v0));
                    var col = new Color(1f, 1f, 1f, a);
                    for (int k = 0; k < 4; k++) wCols.Add(col);
                    wTris.Add(i); wTris.Add(i + 1); wTris.Add(i + 2); wTris.Add(i); wTris.Add(i + 2); wTris.Add(i + 3);
                }
            mesh.Clear();
            if (wVerts.Count == 0) return;
            mesh.SetVertices(wVerts); mesh.SetColors(wCols); mesh.SetUVs(0, wUvs); mesh.SetTriangles(wTris, 0);
            mesh.bounds = new Bounds(camPos, Vector3.one * (R * 2 + 4));
        }

        // ------------------------------------------------------------------ textures
        static Texture2D PaintSun()
        {
            // a square sun: bright core, a warmer rim and a faint halo that fades out to the edge (drawn additively)
            const int S = 32;
            var px = new Color32[S * S];
            for (int y = 0; y < S; y++)
                for (int x = 0; x < S; x++)
                {
                    int d = Mathf.Max(Mathf.Abs(x * 2 + 1 - S), Mathf.Abs(y * 2 + 1 - S)) / 2; // square distance from centre
                    if (d < 7) px[y * S + x] = new Color32(255, 255, 214, 255);
                    else if (d < 9) px[y * S + x] = new Color32(255, 236, 150, 235);
                    else
                    {
                        float a = Mathf.Clamp01(1f - (d - 9) / 7f);
                        px[y * S + x] = new Color32(255, 214, 120, (byte)(a * a * 90));
                    }
                }
            return Tex2D("sky_sun", S, px);
        }

        static Texture2D PaintMoon() => PaintMoonPhase(0);

        /// <summary>Square moon with craters; phase 0 is full, 4 is new, the lit part sweeps across in between.</summary>
        static Texture2D PaintMoonPhase(int phase)
        {
            const int S = 32;
            var px = new Color32[S * S];
            var rng = new RNG(777);
            var craters = new List<Vector3>();
            for (int i = 0; i < 9; i++) craters.Add(new Vector3(9 + rng.Next(14), 9 + rng.Next(14), 1 + rng.Next(3)));
            // lit fraction across the face: positive = lit from the right, negative = from the left
            float[] litEdge = { -1f, -0.5f, 0f, 0.5f, 1.01f, 0.5f, 0f, -0.5f };
            bool fromRight = phase < 4;
            for (int y = 0; y < S; y++)
                for (int x = 0; x < S; x++)
                {
                    int d = Mathf.Max(Mathf.Abs(x * 2 + 1 - S), Mathf.Abs(y * 2 + 1 - S)) / 2;
                    if (d >= 8) continue;
                    var c = new Color32(224, 226, 214, 255);
                    foreach (var cr in craters)
                        if ((x - cr.x) * (x - cr.x) + (y - cr.y) * (y - cr.y) <= cr.z * cr.z) c = new Color32(180, 184, 178, 255);
                    if (d == 7) c = new Color32(200, 202, 192, 255);
                    // phase shading: columns outside the lit part stay as a faint dark disc
                    float u = (x - 8) / 16f * 2f - 1f;            // -1 at the left edge of the face, +1 at the right
                    float edge = litEdge[phase & 7];
                    bool lit = phase == 0 ? true : phase == 4 ? false : fromRight ? u > edge : u < -edge;
                    if (!lit) c = new Color32(34, 36, 48, 90);
                    px[y * S + x] = c;
                }
            return Tex2D("sky_moon_" + phase, S, px);
        }

        /// <summary>The End sky: a dim, mottled grey-violet that tiles over the dome.</summary>
        static Texture2D PaintEndSky()
        {
            const int S = 64;
            var px = new Color32[S * S];
            var rng = new RNG(4099);
            for (int y = 0; y < S; y++)
                for (int x = 0; x < S; x++)
                {
                    int v = 30 + rng.Next(22);
                    if (rng.Next(11) == 0) v += 18;
                    px[y * S + x] = new Color32((byte)v, (byte)(v - 6), (byte)(v + 8), 255);
                }
            return Tex2D("sky_end", S, px);
        }

        static Texture2D PaintCircle(int S, float bright)
        {
            var px = new Color32[S * S];
            for (int y = 0; y < S; y++)
                for (int x = 0; x < S; x++)
                {
                    float dx = x + 0.5f - S * 0.5f, dy = y + 0.5f - S * 0.5f;
                    float d = Mathf.Sqrt(dx * dx + dy * dy) / (S * 0.5f);
                    if (d > 1f) continue;
                    byte a = (byte)(255 * Mathf.Clamp01(1f - d * d) * bright);
                    px[y * S + x] = new Color32(255, 255, 255, a);
                }
            return Tex2D("sky_circle", S, px);
        }

        static Texture2D PaintCloud()
        {
            const int S = 64;
            var px = new Color32[S * S];
            var rng = new RNG(4242);
            // value noise: a few blobs of white on transparency, then a soft threshold for the puffy edge
            int blobs = 90;
            var bx = new float[blobs]; var by = new float[blobs]; var br = new float[blobs];
            for (int i = 0; i < blobs; i++)
            {
                bx[i] = rng.NextFloat() * S; by[i] = rng.NextFloat() * S; br[i] = 6f + rng.NextFloat() * 12f;
            }
            for (int y = 0; y < S; y++)
                for (int x = 0; x < S; x++)
                {
                    float v = 0;
                    for (int i = 0; i < blobs; i++)
                    {
                        // wrap horizontally so the sheet tiles seamlessly
                        float dx = Mathf.Abs(x - bx[i]); dx = Mathf.Min(dx, S - dx);
                        float dy = Mathf.Abs(y - by[i]); dy = Mathf.Min(dy, S - dy);
                        float d = Mathf.Sqrt(dx * dx + dy * dy);
                        if (d < br[i]) v += 1f - d / br[i];
                    }
                    float a = Mathf.Clamp01((v - 0.55f) * 1.4f);
                    if (a <= 0f) continue;
                    px[y * S + x] = new Color32(255, 255, 255, (byte)(a * 235));
                }
            return Tex2D("sky_clouds", S, px);
        }

        static Texture2D PaintRain(bool snow)
        {
            const int S = 32;
            var px = new Color32[S * S];
            var rng = new RNG(snow ? 99 : 98);
            if (snow)
            {
                for (int i = 0; i < 90; i++)
                {
                    int x = rng.Next(S), y = rng.Next(S);
                    px[y * S + x] = new Color32(255, 255, 255, 220);
                    if (rng.NextFloat() < 0.5f) { px[y * S + Mathf.Min(S - 1, x + 1)] = new Color32(240, 245, 255, 160); }
                }
            }
            else
            {
                for (int i = 0; i < 70; i++)
                {
                    int x = rng.Next(S), y = rng.Next(S);
                    int len = 4 + rng.Next(4);
                    for (int k = 0; k < len; k++) px[Mathf.Min(S - 1, y + k) * S + x] = new Color32(180, 200, 255, 150);
                }
            }
            return Tex2D("sky_" + (snow ? "snow" : "rain"), S, px);
        }

        static Texture2D Tex2D(string name, int size, Color32[] px)
        {
            var t = new Texture2D(size, size, TextureFormat.RGBA32, false) { name = name, filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Repeat };
            t.SetPixels32(px);
            t.Apply(false, false);
            return t;
        }

        // ------------------------------------------------------------------ render
        public void Render(Camera cam, World w, GameSession session, Player player, float partial)
        {
            if (!visible || cam == null || root == null || w == null) return;
            var camPos = cam.transform.position;
            root.transform.position = new Vector3(camPos.x, 0, camPos.z);
            float day = session != null ? session.DaylightFactor(w) : 1f;
            if (!w.hasSkyLight) day = w.dim == DimensionId.Nether ? 0.4f : 0.25f;
            float rain = session != null ? session.RainLevel(partial) : 0f;
            float thunder = session != null ? session.ThunderLevel(partial) : 0f;
            flash = Mathf.Max(0f, flash - Time.deltaTime * 4f);
            thunderFade = Mathf.Max(0f, thunderFade - Time.deltaTime * 0.6f);

            float skyScale = Mathf.Clamp(cam.farClipPlane * 0.9f, 200f, 900f);
            Color skyTop, skyBottom;
            SkyColors(w, day, rain, thunder, out skyTop, out skyBottom);
            skyTop = Color.Lerp(skyTop, Color.white, flash * 0.6f);
            WorldLighting.Update(session, w, player, partial, Time.time, skyBottom);

            // dome: fog colour at the horizon blending into the sky colour overhead, darker below the horizon
            var fog = WorldLighting.fogColor;
            if (w.dim == DimensionId.End) ColorDome(Color.white, Color.white, Color.white); // the texture carries the colour
            else ColorDome(fog, skyTop, Color.Lerp(fog, skyBottom * 0.35f, 0.6f));
            domeMat.SetColor("_Color", Color.white);
            if (w.dim == DimensionId.End)
            {
                if (endSkyTex == null) endSkyTex = PaintEndSky();
                domeMat.SetTexture("_MainTex", endSkyTex);
            }
            else domeMat.SetTexture("_MainTex", Texture2D.whiteTexture);
            var domeTrs = Matrix4x4.TRS(camPos, Quaternion.identity, Vector3.one * skyScale);
            Graphics.DrawMesh(dome, domeTrs, domeMat, 0, cam);

            if (w.dim == DimensionId.Overworld)
            {
                // stars fade in at night; the sun and moon ride the celestial angle
                float starAlpha = Mathf.Clamp01(1f - day * 1.6f) * (1f - rain * 0.7f);
                if (starAlpha > 0.01f)
                {
                    starMat.SetColor("_Color", new Color(1, 1, 1, starAlpha));
                    float starAngle = session != null ? session.CelestialAngle(partial) * 360f : 0f;
                    Graphics.DrawMesh(starMesh, Matrix4x4.TRS(camPos, Quaternion.Euler(0, 0, starAngle), Vector3.one * skyScale * 0.85f), starMat, 0, cam);
                }
                float angle = session != null ? session.CelestialAngle(partial) * 360f : 0f;
                // noon puts the sun overhead; it rises in the east (+X) and sets in the west (-X)
                var sunDir = Quaternion.Euler(0, 0, angle) * Vector3.up;
                var sunPos = camPos + sunDir * skyScale * 0.7f;
                sunMat.SetColor("_Color", new Color(1, 1, 1, Mathf.Clamp01(1f - rain * 0.8f)));
                Graphics.DrawMesh(quad, Bill(sunPos, cam, skyScale * 0.07f), sunMat, 0, cam);
                var moonDir = -sunDir;
                int phase = session != null ? session.DayCount % 8 : 0;
                if (moonPhases == null) { moonPhases = new Texture2D[8]; for (int i = 0; i < 8; i++) moonPhases[i] = PaintMoonPhase(i); }
                moonMat.SetTexture("_MainTex", moonPhases[phase]);
                var moonPos = camPos + moonDir * skyScale * 0.7f;
                moonMat.SetColor("_Color", new Color(1, 1, 1, Mathf.Clamp01(1f - rain * 0.6f) * (1f - day)));
                Graphics.DrawMesh(quad, Bill(moonPos, cam, skyScale * 0.055f), moonMat, 0, cam);

                // clouds: the wrapping cell tile drawn 3x3 around the camera, drifting slowly along +X
                cloudScroll += Time.deltaTime * 0.6f;
                if (cloudScroll > CloudTile) cloudScroll -= CloudTile;
                float cloudY = Mathf.Max(w.seaLevel + 129f, 160f);
                var night = new Color(0.10f, 0.10f, 0.14f);
                var cloudCol = Color.Lerp(night, Color.white, Mathf.Clamp01(day * 1.2f));
                cloudCol = Color.Lerp(cloudCol, cloudCol * 0.62f, rain);
                cloudCol.a = 0.8f;
                cloudMat.SetColor("_Color", cloudCol);
                float ox = Mathf.Floor((camPos.x + cloudScroll) / CloudTile) * CloudTile - cloudScroll;
                float oz = Mathf.Floor(camPos.z / CloudTile) * CloudTile;
                for (int tz = -1; tz <= 1; tz++)
                    for (int tx = -1; tx <= 1; tx++)
                    {
                        var m = Matrix4x4.Translate(new Vector3(ox + tx * CloudTile, cloudY, oz + tz * CloudTile));
                        Graphics.DrawMesh(cloudMesh, m, cloudDepthMat, 0, cam);
                        Graphics.DrawMesh(cloudMesh, m, cloudMat, 0, cam);
                    }
            }

            // rain and snow: per-column sheets around the camera that stop at the first roof
            if (rain > 0.01f && w.dim == DimensionId.Overworld && player != null)
            {
                float tint = Mathf.Lerp(0.35f, 1f, Mathf.Clamp01(day));
                rainMat.SetColor("_Color", new Color(tint, tint, tint, 1f));
                snowMat.SetColor("_Color", new Color(tint, tint, tint, 1f));
                weatherTimer -= Time.deltaTime;
                if (weatherTimer <= 0f)
                {
                    weatherTimer = 0.05f;
                    BuildWeather(rainMesh, w, camPos, false, rain, Time.time);
                    BuildWeather(snowMesh, w, camPos, true, rain, Time.time);
                }
                if (rainMesh.vertexCount > 0) Graphics.DrawMesh(rainMesh, Matrix4x4.identity, rainMat, 0, cam);
                if (snowMesh.vertexCount > 0) Graphics.DrawMesh(snowMesh, Matrix4x4.identity, snowMat, 0, cam);
            }
        }
        float weatherTimer;

        static Matrix4x4 Bill(Vector3 pos, Camera cam, float size)
        {
            var q = Quaternion.LookRotation(cam.transform.position - pos, Vector3.up);
            return Matrix4x4.TRS(pos, q, new Vector3(size, size, size));
        }

        static void SkyColors(World w, float day, float rain, float thunder, out Color top, out Color bottom)
        {
            if (w.dim == DimensionId.Nether)
            {
                top = new Color(0.05f, 0.012f, 0.010f); bottom = new Color(0.30f, 0.06f, 0.020f);
                return;
            }
            if (w.dim == DimensionId.End)
            {
                top = new Color(0.004f, 0.003f, 0.010f); bottom = new Color(0.014f, 0.011f, 0.035f);
                return;
            }
            // the project runs in linear space, so these are the sRGB sky tones pushed through the linear curve:
            // a daylight zenith of #7BA4FF lands near (0.2, 0.37, 1.0) linear, and the horizon haze near white
            Color dayTop = new Color(0.20f, 0.37f, 1.00f), dayBottom = new Color(0.48f, 0.69f, 1.00f);
            Color nightTop = new Color(0.002f, 0.002f, 0.02f), nightBottom = new Color(0.006f, 0.006f, 0.05f);
            Color duskTop = new Color(0.04f, 0.03f, 0.13f), duskBottom = new Color(0.66f, 0.22f, 0.06f);
            float t = Mathf.Clamp01(day);
            if (t > 0.35f) { top = Color.Lerp(duskTop, dayTop, (t - 0.35f) / 0.65f); bottom = Color.Lerp(duskBottom, dayBottom, (t - 0.35f) / 0.65f); }
            else
            {
                float u = t / 0.35f;
                if (u < 0.5f) { top = Color.Lerp(nightTop, duskTop, u * 2f); bottom = Color.Lerp(nightBottom, duskBottom, u * 2f); }
                else { top = Color.Lerp(duskTop, dayTop, (u - 0.5f) * 2f); bottom = Color.Lerp(duskBottom, dayBottom, (u - 0.5f) * 2f); }
            }
            var grey = new Color(0.13f, 0.15f, 0.18f);
            top = Color.Lerp(top, grey * Mathf.Lerp(0.7f, 1f, t), rain * 0.8f);
            bottom = Color.Lerp(bottom, grey, rain * 0.8f);
            top = Color.Lerp(top, new Color(0.004f, 0.004f, 0.007f), thunder * 0.6f);
            bottom = Color.Lerp(bottom, new Color(0.010f, 0.010f, 0.015f), thunder * 0.6f);
        }

        // ------------------------------------------------------------------ world-space overlays
        static Mesh blockOutline;
        static Material outlineMaterial;

        /// <summary>Line box around the block under the crosshair.</summary>
        public static void DrawBlockOutline(Camera cam, Int3 pos, Block block, int meta, Color color)
        {
            if (cam == null) return;
            if (blockOutline == null) blockOutline = BuildWireCube();
            if (outlineMaterial == null) outlineMaterial = Res.UnlitMaterial(Texture2D.whiteTexture, false, true, 4000, false);
            outlineMaterial.SetColor("_Color", color);
            Graphics.DrawMesh(blockOutline, Matrix4x4.TRS(pos.ToVector3() - new Vector3(0.002f, 0.002f, 0.002f), Quaternion.identity, Vector3.one * 1.004f), outlineMaterial, 0, cam);
        }

        public static void DrawBox(Camera cam, AABB box, Color color)
        {
            if (cam == null) return;
            if (blockOutline == null) blockOutline = BuildWireCube();
            if (outlineMaterial == null) outlineMaterial = Res.UnlitMaterial(Texture2D.whiteTexture, false, true, 4000, false);
            outlineMaterial.SetColor("_Color", color);
            var size = new Vector3(box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z);
            Graphics.DrawMesh(blockOutline, Matrix4x4.TRS(box.min - Vector3.one * 0.002f, Quaternion.identity, size + Vector3.one * 0.004f), outlineMaterial, 0, cam);
        }

        static Mesh BuildWireCube()
        {
            var m = new Mesh { name = "wire_cube" };
            var v = new[]
            {
                new Vector3(0,0,0), new Vector3(1,0,0), new Vector3(1,0,1), new Vector3(0,0,1),
                new Vector3(0,1,0), new Vector3(1,1,0), new Vector3(1,1,1), new Vector3(0,1,1),
            };
            var lines = new[]
            {
                0,1, 1,2, 2,3, 3,0,   // bottom
                4,5, 5,6, 6,7, 7,4,   // top
                0,4, 1,5, 2,6, 3,7,   // verticals
            };
            m.vertices = v;
            m.SetIndices(lines, MeshTopology.Lines, 0);
            m.RecalculateBounds();
            m.bounds = new Bounds(new Vector3(0.5f, 0.5f, 0.5f), new Vector3(2, 2, 2));
            return m;
        }

        public void Dispose()
        {
            if (root != null) Object.Destroy(root);
        }
    }
}
