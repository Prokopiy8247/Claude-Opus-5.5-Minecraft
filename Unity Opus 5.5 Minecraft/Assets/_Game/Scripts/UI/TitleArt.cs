using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace MCR
{
    /// <summary>
    /// Title screen art made by the game itself: a slowly turning panorama (six views of a generated world,
    /// captured by the automated test runner and shipped as generated resources) and a blocky stone logo built
    /// from the interface font's own glyph bitmaps. Nothing here comes from the original game's files.
    /// </summary>
    public static class TitleArt
    {
        // ------------------------------------------------------------------ panorama
        static bool panoramaTried;
        static Mesh cube;
        static Material[] faceMats;

        /// <summary>True when the six panorama faces are available in Resources/Panorama.</summary>
        public static bool HasPanorama { get { LoadPanorama(); return faceMats != null; } }

        static void LoadPanorama()
        {
            if (panoramaTried) return;
            panoramaTried = true;
            var faces = new Texture2D[6];
            for (int i = 0; i < 6; i++)
            {
                faces[i] = Resources.Load<Texture2D>("Panorama/panorama_" + i);
                if (faces[i] == null) return;
                faces[i].wrapMode = TextureWrapMode.Clamp;
            }
            faceMats = new Material[6];
            for (int i = 0; i < 6; i++)
            {
                var m = Res.UnlitMaterial(faces[i], false, false, 5, false);
                m.SetFloat("_ZWrite", 0f);
                faceMats[i] = m;
            }
            cube = BuildCube();
        }

        /// <summary>
        /// Inside-out cube, one submesh per face in capture order: 0 +Z (front), 1 +X, 2 -Z, 3 -X, 4 up, 5 down.
        /// UVs are laid out so each capture reads upright when seen from the centre.
        /// </summary>
        static Mesh BuildCube()
        {
            var v = new List<Vector3>(); var uv = new List<Vector2>();
            var faces = new List<int>[6];
            // each face: centre direction, right and up vectors as seen from inside
            Vector3[] fwd = { Vector3.forward, Vector3.right, Vector3.back, Vector3.left, Vector3.up, Vector3.down };
            Vector3[] up = { Vector3.up, Vector3.up, Vector3.up, Vector3.up, Vector3.back, Vector3.forward };
            for (int f = 0; f < 6; f++)
            {
                var right = Vector3.Cross(up[f], fwd[f]);
                int s = v.Count;
                v.Add(fwd[f] - right - up[f]); uv.Add(new Vector2(0, 0));
                v.Add(fwd[f] - right + up[f]); uv.Add(new Vector2(0, 1));
                v.Add(fwd[f] + right + up[f]); uv.Add(new Vector2(1, 1));
                v.Add(fwd[f] + right - up[f]); uv.Add(new Vector2(1, 0));
                faces[f] = new List<int> { s, s + 1, s + 2, s, s + 2, s + 3 };
            }
            var m = new Mesh { name = "title_panorama" };
            m.SetVertices(v); m.SetUVs(0, uv);
            m.subMeshCount = 6;
            for (int f = 0; f < 6; f++) m.SetTriangles(faces[f], f);
            m.bounds = new Bounds(Vector3.zero, Vector3.one * 4f);
            return m;
        }

        /// <summary>Draws the panorama around <paramref name="cam"/>; call every frame while the title is up.</summary>
        public static void RenderPanorama(Camera cam)
        {
            LoadPanorama();
            if (faceMats == null || cam == null) return;
            var trs = Matrix4x4.TRS(cam.transform.position, Quaternion.identity, Vector3.one * 10f);
            for (int f = 0; f < 6; f++) Graphics.DrawMesh(cube, trs, faceMats[f], 0, cam, f);
        }

        // ------------------------------------------------------------------ logo
        static readonly Dictionary<string, Texture2D> logos = new Dictionary<string, Texture2D>();
        const int Cell = 6, Depth = 3;

        /// <summary>
        /// A logo texture for <paramref name="text"/>: every lit font pixel becomes a bevelled stone block, with a
        /// darker extrusion underneath for depth. Returned texture pixels map 1:1 to logo pixels.
        /// </summary>
        public static Texture2D Logo(string text)
        {
            if (logos.TryGetValue(text, out var t)) return t;
            int cols = 0;
            foreach (var c in text) cols += FontData.CharAdvance(c);
            int rowsPx = 7;
            int w = cols * Cell + Depth + 2, h = rowsPx * Cell + Depth + 2;
            var px = new Color32[w * h];
            var rng = new RNG(1234567);
            // pass 1 extrusion, pass 2 faces
            for (int pass = 0; pass < 2; pass++)
            {
                int ox = 0;
                foreach (var c in text)
                {
                    var g = FontData.Glyph(c);
                    var (left, gw) = FontData.Metrics(c);
                    for (int gy = 0; gy < rowsPx; gy++)
                        for (int gx = 0; gx < 5; gx++)
                        {
                            if ((g[gy] & (1 << (4 - gx))) == 0) continue;
                            int bx = (ox + gx - left) * Cell + 1, by = gy * Cell + 1;
                            if (pass == 0)
                            {
                                for (int d = 1; d <= Depth; d++)
                                    for (int yy = 0; yy < Cell; yy++)
                                        for (int xx = 0; xx < Cell; xx++)
                                            Put(px, w, h, bx + xx + d, by + yy + d, new Color32((byte)(38 - d * 4), (byte)(38 - d * 4), (byte)(42 - d * 4), 255));
                            }
                            else
                            {
                                for (int yy = 0; yy < Cell; yy++)
                                    for (int xx = 0; xx < Cell; xx++)
                                    {
                                        int n = rng.Next(28);
                                        int v = 128 + n;
                                        if (xx == 0 || yy == 0) v += 40;                  // lit bevel
                                        else if (xx == Cell - 1 || yy == Cell - 1) v -= 46; // shaded bevel
                                        if (rng.Next(9) == 0) v -= 24;                     // pits in the stone
                                        v = Mathf.Clamp(v, 40, 230);
                                        Put(px, w, h, bx + xx, by + yy, new Color32((byte)v, (byte)v, (byte)(v + 4 > 255 ? 255 : v + 4), 255));
                                    }
                            }
                        }
                    ox += FontData.CharAdvance(c);
                }
            }
            // the texture rows run bottom-up
            var flipped = new Color32[w * h];
            for (int y = 0; y < h; y++) System.Array.Copy(px, y * w, flipped, (h - 1 - y) * w, w);
            t = new Texture2D(w, h, TextureFormat.RGBA32, false) { name = "logo_" + text, filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp };
            t.SetPixels32(flipped);
            t.Apply(false, true);
            logos[text] = t;
            return t;
        }

        static void Put(Color32[] px, int w, int h, int x, int y, Color32 c)
        {
            if (x < 0 || y < 0 || x >= w || y >= h) return;
            px[y * w + x] = c;
        }
    }
}
