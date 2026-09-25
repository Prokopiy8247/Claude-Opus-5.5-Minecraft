using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

namespace MCR
{
    /// <summary>Packed chunk vertex (28 bytes). Attribute order must match Unity's required order.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct ChunkVertex
    {
        public float x, y, z;
        public uint color;                  // rgb = tint, a = shade * ao
        public ushort u, v, layer, anim;    // half floats
        public uint light;                  // r = sky/15, g = block/15

        public static readonly VertexAttributeDescriptor[] Layout =
        {
            new VertexAttributeDescriptor(VertexAttribute.Position, VertexAttributeFormat.Float32, 3),
            new VertexAttributeDescriptor(VertexAttribute.Color, VertexAttributeFormat.UNorm8, 4),
            new VertexAttributeDescriptor(VertexAttribute.TexCoord0, VertexAttributeFormat.Float16, 4),
            new VertexAttributeDescriptor(VertexAttribute.TexCoord1, VertexAttributeFormat.UNorm8, 4),
        };
    }

    public sealed class MeshBuffer
    {
        public ChunkVertex[] verts = new ChunkVertex[1024];
        public int[] indices = new int[1536];
        public int vcount, icount;

        public void Clear() { vcount = 0; icount = 0; }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public void Reserve(int quads)
        {
            if (vcount + quads * 4 > verts.Length) Array.Resize(ref verts, Math.Max(verts.Length * 2, vcount + quads * 4));
            if (icount + quads * 6 > indices.Length) Array.Resize(ref indices, Math.Max(indices.Length * 2, icount + quads * 6));
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public void QuadIndices(bool flip)
        {
            int b = vcount - 4;
            if (!flip)
            {
                indices[icount++] = b; indices[icount++] = b + 1; indices[icount++] = b + 2;
                indices[icount++] = b; indices[icount++] = b + 2; indices[icount++] = b + 3;
            }
            else
            {
                indices[icount++] = b + 1; indices[icount++] = b + 2; indices[icount++] = b + 3;
                indices[icount++] = b + 1; indices[icount++] = b + 3; indices[icount++] = b;
            }
        }
    }

    /// <summary>
    /// Per-thread meshing context. Holds an 18x18x18 padded copy of block states and light around the current section.
    /// Block models emit geometry through the helper methods.
    /// </summary>
    public sealed class MeshCtx
    {
        public const int P = 18, P2 = 324, P3 = 5832;
        public readonly ushort[] blocks = new ushort[P3];
        public readonly byte[] light = new byte[P3];
        public readonly MeshBuffer[] layers = { new MeshBuffer(), new MeshBuffer(), new MeshBuffer() };
        public readonly Color32[] grassTint = new Color32[256];
        public readonly Color32[] foliageTint = new Color32[256];
        public readonly Color32[] waterTint = new Color32[256];
        public World world;
        public int x, y, z;      // local position within section
        public int baseY;        // world y of section bottom
        public int wx0, wz0;     // world x/z of column origin
        public int ci;           // padded index of current block
        public ushort state;     // current state
        public Block block;      // current block
        public bool fancyLeaves = true;
        public bool smooth = true;
        public int rot;          // quarter turns clockwise (seen from above) applied to Box/Quad emission
        public RNG rng;

        public int WorldX => wx0 + x;
        public int WorldY => baseY + y;
        public int WorldZ => wz0 + z;

        // ---------- Tables ----------
        // Full-cube face vertex positions; order v0..v3 = bottom-left, top-left, top-right, bottom-right seen from outside.
        public static readonly float[,,] FaceVerts =
        {
            { {1,0,0},{1,0,1},{0,0,1},{0,0,0} }, // Down
            { {0,1,0},{0,1,1},{1,1,1},{1,1,0} }, // Up
            { {1,0,1},{1,1,1},{0,1,1},{0,0,1} }, // North (+Z)
            { {0,0,0},{0,1,0},{1,1,0},{1,0,0} }, // South (-Z)
            { {0,0,1},{0,1,1},{0,1,0},{0,0,0} }, // West (-X)
            { {1,0,0},{1,1,0},{1,1,1},{1,0,1} }, // East (+X)
        };
        public static readonly float[] FaceShade = { 0.5f, 1f, 0.8f, 0.8f, 0.6f, 0.6f };
        static readonly float[] AOCurve = { 0.52f, 0.68f, 0.84f, 1f };
        static readonly int[] NeighborDelta = { -P2, P2, P, -P, -1, 1 };
        // AO sample offsets (padded index deltas) per face per vertex: side1, side2, corner (relative to block, include normal)
        static readonly int[,] AOS1 = new int[6, 4], AOS2 = new int[6, 4], AOC = new int[6, 4];

        static MeshCtx()
        {
            for (int f = 0; f < 6; f++)
            {
                Int3 n = DirUtil.Offset[f];
                int na = DirUtil.Axis((Dir)f);
                int ta = na == 0 ? 1 : 0;           // first tangent axis
                int tb = na == 2 ? 1 : 2;           // second tangent axis
                if (na == 1) { ta = 0; tb = 2; }
                for (int v = 0; v < 4; v++)
                {
                    int sa = FaceVerts[f, v, ta] > 0.5f ? 1 : -1;
                    int sb = FaceVerts[f, v, tb] > 0.5f ? 1 : -1;
                    Int3 s1 = n, s2 = n;
                    s1 = AddAxis(s1, ta, sa); s2 = AddAxis(s2, tb, sb);
                    Int3 c = AddAxis(AddAxis(n, ta, sa), tb, sb);
                    AOS1[f, v] = Delta(s1); AOS2[f, v] = Delta(s2); AOC[f, v] = Delta(c);
                }
            }
        }
        static Int3 AddAxis(Int3 p, int axis, int s) { if (axis == 0) p.x += s; else if (axis == 1) p.y += s; else p.z += s; return p; }
        static int Delta(Int3 o) => o.y * P2 + o.z * P + o.x;

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static int PIdx(int x, int y, int z) => (y + 1) * P2 + (z + 1) * P + (x + 1);

        // ---------- Neighbour queries ----------
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public ushort N(int dx, int dy, int dz) => blocks[ci + dy * P2 + dz * P + dx];
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public ushort N(Dir d) => blocks[ci + NeighborDelta[(int)d]];
        public Block NB(Dir d) => Blocks.ByState[blocks[ci + NeighborDelta[(int)d]]];
        public Block NB(int dx, int dy, int dz) => Blocks.ByState[N(dx, dy, dz)];
        public bool Opaque(int dx, int dy, int dz) => Blocks.StateOpaque[N(dx, dy, dz)];
        public byte OwnLight => light[ci];

        public uint TintColor(TintType t)
        {
            int i = (z << 4) | x;
            switch (t)
            {
                case TintType.None: return 0xFFFFFFFFu;
                case TintType.Grass: return Pack(grassTint[i]);
                case TintType.Foliage: return Pack(foliageTint[i]);
                case TintType.Water: return Pack(waterTint[i]);
                case TintType.Spruce: return Pack(new Color32(97, 153, 97, 255));
                case TintType.Birch: return Pack(new Color32(128, 167, 85, 255));
                case TintType.Mangrove: return Pack(new Color32(141, 177, 39, 255));
                case TintType.Lily: return Pack(new Color32(32, 128, 48, 255));
                case TintType.Stem: return Pack(new Color32(140, 200, 60, 255));
                default: return 0xFFFFFFFFu;
            }
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static uint Pack(Color32 c) => (uint)(c.r | (c.g << 8) | (c.b << 16) | (c.a << 24));
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static uint Pack(byte r, byte g, byte b, byte a) => (uint)(r | (g << 8) | (b << 16) | (a << 24));
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        static uint WithAlpha(uint color, float a) => (color & 0x00FFFFFFu) | ((uint)(Mathf.Clamp01(a) * 255f + 0.5f) << 24);

        /// <summary>Should face 'f' of the current block be drawn given its neighbour?</summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool FaceVisible(int f)
        {
            ushort n = blocks[ci + NeighborDelta[f]];
            if ((Blocks.StateOccludes[n] & (1 << (f ^ 1))) != 0) return false;
            if (block.selfCullSameType && Blocks.ByState[n] == block) return false;
            if (!fancyLeaves && block is LeavesBlock && Blocks.ByState[n] is LeavesBlock) return false;
            return true;
        }

        // ---------- Lighting ----------
        struct VL { public float sky, blk, ao; }
        readonly VL[] vl = new VL[4];

        void ComputeFaceLight(int f)
        {
            int fi = ci + NeighborDelta[f];
            byte fl = light[fi];
            float fs = fl >> 4, fb = fl & 15;
            if (!smooth)
            {
                for (int v = 0; v < 4; v++) { vl[v].sky = fs; vl[v].blk = fb; vl[v].ao = 1f; }
                return;
            }
            for (int v = 0; v < 4; v++)
            {
                int i1 = ci + AOS1[f, v], i2 = ci + AOS2[f, v], ic = ci + AOC[f, v];
                bool o1 = Blocks.StateAO[blocks[i1]], o2 = Blocks.StateAO[blocks[i2]];
                bool oc = (o1 && o2) || Blocks.StateAO[blocks[ic]];
                float ss = fs, sb = fb; int cnt = 1;
                if (!o1) { byte l = light[i1]; ss += l >> 4; sb += l & 15; cnt++; }
                if (!o2) { byte l = light[i2]; ss += l >> 4; sb += l & 15; cnt++; }
                if (!oc) { byte l = light[ic]; ss += l >> 4; sb += l & 15; cnt++; }
                float inv = 1f / cnt;
                vl[v].sky = ss * inv; vl[v].blk = sb * inv;
                int ao = 3 - ((o1 ? 1 : 0) + (o2 ? 1 : 0) + (oc ? 1 : 0));
                vl[v].ao = AOCurve[ao];
            }
        }

        void OwnFlatLight()
        {
            byte l = light[ci];
            float s = l >> 4, b = l & 15;
            for (int v = 0; v < 4; v++) { vl[v].sky = s; vl[v].blk = b; vl[v].ao = 1f; }
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        static uint PackLight(float sky, float blk) =>
            (uint)(Mathf.Clamp(sky * 17f, 0, 255)) | ((uint)(Mathf.Clamp(blk * 17f, 0, 255)) << 8);

        // ---------- Emission ----------
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        void Vert(MeshBuffer mb, float px, float py, float pz, float u, float v, int layer, uint color, uint lightPacked)
        {
            ref ChunkVertex cv = ref mb.verts[mb.vcount++];
            cv.x = x + px; cv.y = baseY + y + py; cv.z = z + pz;
            cv.color = color;
            cv.u = HalfConv.ToHalf(u); cv.v = HalfConv.ToHalf(v);
            cv.layer = HalfConv.ToHalf(layer);
            int fr = (layer >= 0 && layer < Tex.FrameCount.Length) ? Tex.FrameCount[layer] : 1;
            cv.anim = HalfConv.ToHalf(fr);
            cv.light = lightPacked;
        }

        MeshBuffer Buf(RenderLayer l) => layers[l == RenderLayer.None ? 0 : (int)l];

        /// <summary>Full cube using per-face textures, standard culling and smooth light.</summary>
        public void Cube(int[] tex, TintType tint)
        {
            uint color = TintColor(tint);
            var mb = Buf(block.GetLayer(state - block.baseState));
            for (int f = 0; f < 6; f++)
            {
                if (!FaceVisible(f)) continue;
                CubeFace(mb, f, tex[f], color, 0);
            }
        }

        public void Cube(int[] tex, uint color, RenderLayer layer)
        {
            var mb = Buf(layer);
            for (int f = 0; f < 6; f++)
            {
                if (!FaceVisible(f)) continue;
                CubeFace(mb, f, tex[f], color, 0);
            }
        }

        /// <summary>Cube with per-face uv rotation (quarter turns). Used for axis-rotated logs etc.</summary>
        public void CubeRot(int[] tex, int[] uvRot, uint color)
        {
            var mb = Buf(block.GetLayer(state - block.baseState));
            for (int f = 0; f < 6; f++)
            {
                if (!FaceVisible(f)) continue;
                CubeFace(mb, f, tex[f], color, uvRot[f]);
            }
        }

        static readonly float[] Us = { 0, 0, 1, 1 }, Vs = { 0, 1, 1, 0 };

        void CubeFace(MeshBuffer mb, int f, int tex, uint color, int uvRot)
        {
            ComputeFaceLight(f);
            mb.Reserve(1);
            float shade = FaceShade[f];
            float b0 = 0, b1 = 0, b2 = 0, b3 = 0;
            for (int v = 0; v < 4; v++)
            {
                int uvi = (v + uvRot) & 3;
                ref VL L = ref vl[v];
                Vert(mb, FaceVerts[f, v, 0], FaceVerts[f, v, 1], FaceVerts[f, v, 2], Us[uvi], Vs[uvi], tex,
                    WithAlpha(color, shade * L.ao), PackLight(L.sky, L.blk));
                float bright = L.ao * (Mathf.Max(L.sky, L.blk) + 2f);
                if (v == 0) b0 = bright; else if (v == 1) b1 = bright; else if (v == 2) b2 = bright; else b3 = bright;
            }
            mb.QuadIndices(b0 + b2 < b1 + b3);
        }

        // ---- rotation helpers (quarter turns clockwise seen from above around block center) ----
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        void RotP(ref float px, ref float pz)
        {
            switch (rot & 3)
            {
                case 1: { float t = px; px = pz; pz = 1f - t; break; }      // N->E : (x,z)->(z,1-x)
                case 2: px = 1f - px; pz = 1f - pz; break;
                case 3: { float t = px; px = 1f - pz; pz = t; break; }
            }
        }
        public static int RotFace(int f, int r)
        {
            if (f < 2 || (r & 3) == 0) return f;
            Dir d = (Dir)f;
            int hi = DirUtil.HorizIndex(d);
            return (int)DirUtil.FromHorizIndex(hi + r);
        }

        static void AutoUV(int f, float px, float py, float pz, out float u, out float v)
        {
            switch (f)
            {
                case 0: u = 1f - px; v = pz; break;
                case 1: u = px; v = pz; break;
                case 2: u = 1f - px; v = py; break;
                case 3: u = px; v = py; break;
                case 4: u = 1f - pz; v = py; break;
                default: u = pz; v = py; break;
            }
        }

        /// <summary>
        /// Axis-aligned box (block-local coords 0..1) in the canonical (north-facing) frame; the current 'rot' is applied.
        /// tex: 6 texture layers (canonical faces). skipMask: canonical faces not to emit.
        /// Faces lying on the block boundary are culled against neighbours when cull=true and get smooth light.
        /// </summary>
        public void Box(float x0, float y0, float z0, float x1, float y1, float z1, int[] tex, uint color, int skipMask = 0, bool cull = true, RenderLayer? layerOverride = null, Vector4[] uvRects = null, int[] uvRot = null)
        {
            var mb = Buf(layerOverride ?? block.GetLayer(state - block.baseState));
            for (int cf = 0; cf < 6; cf++)
            {
                if ((skipMask & (1 << cf)) != 0) continue;
                int wf = RotFace(cf, rot);
                // is the face on the block boundary?
                bool boundary;
                switch (cf)
                {
                    case 0: boundary = y0 <= 0.0001f; break;
                    case 1: boundary = y1 >= 0.9999f; break;
                    case 2: boundary = z1 >= 0.9999f; break;
                    case 3: boundary = z0 <= 0.0001f; break;
                    case 4: boundary = x0 <= 0.0001f; break;
                    default: boundary = x1 >= 0.9999f; break;
                }
                if (boundary && cull && !FaceVisible(wf)) continue;
                if (boundary) ComputeFaceLight(wf); else OwnFlatLight();
                mb.Reserve(1);
                float shade = FaceShade[wf];
                float b0 = 0, b1 = 0, b2 = 0, b3 = 0;
                for (int v = 0; v < 4; v++)
                {
                    float px = FaceVerts[cf, v, 0] > 0.5f ? x1 : x0;
                    float py = FaceVerts[cf, v, 1] > 0.5f ? y1 : y0;
                    float pz = FaceVerts[cf, v, 2] > 0.5f ? z1 : z0;
                    float u, vv;
                    if (uvRects != null && uvRects[cf] != Vector4.zero)
                    {
                        Vector4 r = uvRects[cf];
                        int ui = uvRot != null ? (v + uvRot[cf]) & 3 : v;
                        u = Us[ui] < 0.5f ? r.x : r.z; vv = Vs[ui] < 0.5f ? r.y : r.w;
                    }
                    else
                    {
                        float rx = px, rz = pz;
                        RotP(ref rx, ref rz);
                        AutoUV(wf, rx, py, rz, out u, out vv);
                        if (uvRot != null && uvRot[cf] != 0) RotateUV(ref u, ref vv, uvRot[cf]);
                    }
                    // lighting: bilinear from face corners for boundary faces
                    float sky, blk, ao;
                    if (boundary && smooth)
                    {
                        float fu, fv;
                        float rx2 = px, rz2 = pz; RotP(ref rx2, ref rz2);
                        FaceCoord(wf, rx2, py, rz2, out fu, out fv);
                        Bilerp(fu, fv, out sky, out blk, out ao);
                    }
                    else { sky = vl[0].sky; blk = vl[0].blk; ao = vl[0].ao; }
                    float wpx = px, wpz = pz; RotP(ref wpx, ref wpz);
                    Vert(mb, wpx, py, wpz, u, vv, tex[cf], WithAlpha(color, shade * ao), PackLight(sky, blk));
                    float bright = ao * (Mathf.Max(sky, blk) + 2f);
                    if (v == 0) b0 = bright; else if (v == 1) b1 = bright; else if (v == 2) b2 = bright; else b3 = bright;
                }
                mb.QuadIndices(b0 + b2 < b1 + b3);
            }
        }

        static void RotateUV(ref float u, ref float v, int r)
        {
            for (int i = 0; i < (r & 3); i++) { float t = u; u = v; v = 1f - t; }
        }

        /// <summary>Local 2D coordinate of a point on a full face in the face's v0..v3 frame (u right, v up).</summary>
        static void FaceCoord(int f, float px, float py, float pz, out float fu, out float fv)
        {
            switch (f)
            {
                case 0: fu = 1f - px; fv = pz; break;
                case 1: fu = px; fv = pz; break;
                case 2: fu = 1f - px; fv = py; break;
                case 3: fu = px; fv = py; break;
                case 4: fu = 1f - pz; fv = py; break;
                default: fu = pz; fv = py; break;
            }
        }

        void Bilerp(float u, float v, out float sky, out float blk, out float ao)
        {
            // corners: v0=(0,0) v1=(0,1) v2=(1,1) v3=(1,0)
            float w0 = (1 - u) * (1 - v), w1 = (1 - u) * v, w2 = u * v, w3 = u * (1 - v);
            sky = vl[0].sky * w0 + vl[1].sky * w1 + vl[2].sky * w2 + vl[3].sky * w3;
            blk = vl[0].blk * w0 + vl[1].blk * w1 + vl[2].blk * w2 + vl[3].blk * w3;
            ao = vl[0].ao * w0 + vl[1].ao * w1 + vl[2].ao * w2 + vl[3].ao * w3;
        }

        /// <summary>Simple box in block pixels (0..16), canonical frame, all faces same texture.</summary>
        public void BoxPx(float x0, float y0, float z0, float x1, float y1, float z1, int tex, uint color, int skipMask = 0, bool cull = true)
        {
            tmpTex[0] = tmpTex[1] = tmpTex[2] = tmpTex[3] = tmpTex[4] = tmpTex[5] = tex;
            Box(x0 / 16f, y0 / 16f, z0 / 16f, x1 / 16f, y1 / 16f, z1 / 16f, tmpTex, color, skipMask, cull);
        }
        readonly int[] tmpTex = new int[6];

        /// <summary>Two diagonal planes (flowers, saplings, grass). Rendered double-sided via cutout layer cull-off.</summary>
        public void Cross(int tex, uint color, float scale = 1f, float yOffset = 0f, float height = 1f, bool randomOffset = false)
        {
            var mb = Buf(RenderLayer.Cutout);
            OwnFlatLight();
            uint lp = PackLight(vl[0].sky, vl[0].blk);
            float ox = 0, oz = 0;
            if (randomOffset)
            {
                uint h = Hash.Get(1337, WorldX, WorldZ);
                ox = ((h & 15) / 15f - 0.5f) * 0.4f; oz = (((h >> 4) & 15) / 15f - 0.5f) * 0.4f;
            }
            float a = 0.5f - 0.5f * scale * 0.9f, b = 0.5f + 0.5f * scale * 0.9f;
            float y0 = yOffset, y1 = yOffset + height * scale;
            uint c = WithAlpha(color, 0.9f);
            mb.Reserve(4);
            // plane 1
            Vert(mb, a + ox, y0, a + oz, 0, 0, tex, c, lp); Vert(mb, a + ox, y1, a + oz, 0, height, tex, c, lp);
            Vert(mb, b + ox, y1, b + oz, 1, height, tex, c, lp); Vert(mb, b + ox, y0, b + oz, 1, 0, tex, c, lp);
            mb.QuadIndices(false);
            Vert(mb, b + ox, y0, b + oz, 0, 0, tex, c, lp); Vert(mb, b + ox, y1, b + oz, 0, height, tex, c, lp);
            Vert(mb, a + ox, y1, a + oz, 1, height, tex, c, lp); Vert(mb, a + ox, y0, a + oz, 1, 0, tex, c, lp);
            mb.QuadIndices(false);
            // plane 2
            Vert(mb, a + ox, y0, b + oz, 0, 0, tex, c, lp); Vert(mb, a + ox, y1, b + oz, 0, height, tex, c, lp);
            Vert(mb, b + ox, y1, a + oz, 1, height, tex, c, lp); Vert(mb, b + ox, y0, a + oz, 1, 0, tex, c, lp);
            mb.QuadIndices(false);
            Vert(mb, b + ox, y0, a + oz, 0, 0, tex, c, lp); Vert(mb, b + ox, y1, a + oz, 0, height, tex, c, lp);
            Vert(mb, a + ox, y1, b + oz, 1, height, tex, c, lp); Vert(mb, a + ox, y0, b + oz, 1, 0, tex, c, lp);
            mb.QuadIndices(false);
        }

        /// <summary>Crop pattern: four vertical planes forming a # shape.</summary>
        public void Crop(int tex, uint color, float yOffset = -1f / 16f)
        {
            var mb = Buf(RenderLayer.Cutout);
            OwnFlatLight();
            uint lp = PackLight(vl[0].sky, vl[0].blk);
            uint c = WithAlpha(color, 0.9f);
            mb.Reserve(8);
            float y0 = yOffset, y1 = 1f + yOffset;
            float[] offs = { 0.25f, 0.75f };
            foreach (float o in offs)
            {
                // planes along X at z=o
                Vert(mb, 0, y0, o, 0, 0, tex, c, lp); Vert(mb, 0, y1, o, 0, 1, tex, c, lp); Vert(mb, 1, y1, o, 1, 1, tex, c, lp); Vert(mb, 1, y0, o, 1, 0, tex, c, lp); mb.QuadIndices(false);
                Vert(mb, 1, y0, o, 0, 0, tex, c, lp); Vert(mb, 1, y1, o, 0, 1, tex, c, lp); Vert(mb, 0, y1, o, 1, 1, tex, c, lp); Vert(mb, 0, y0, o, 1, 0, tex, c, lp); mb.QuadIndices(false);
                // planes along Z at x=o
                Vert(mb, o, y0, 1, 0, 0, tex, c, lp); Vert(mb, o, y1, 1, 0, 1, tex, c, lp); Vert(mb, o, y1, 0, 1, 1, tex, c, lp); Vert(mb, o, y0, 0, 1, 0, tex, c, lp); mb.QuadIndices(false);
                Vert(mb, o, y0, 0, 0, 0, tex, c, lp); Vert(mb, o, y1, 0, 0, 1, tex, c, lp); Vert(mb, o, y1, 1, 1, 1, tex, c, lp); Vert(mb, o, y0, 1, 1, 0, tex, c, lp); mb.QuadIndices(false);
            }
        }

        /// <summary>Arbitrary flat-lit quad in block-local coords (canonical frame, rot applied). Vertices in clockwise order seen from the front.</summary>
        public void Quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector2 uva, Vector2 uvb, Vector2 uvc, Vector2 uvd, int tex, uint color, float shade, RenderLayer layer, bool doubleSided = false)
        {
            var mb = Buf(layer);
            OwnFlatLight();
            uint lp = PackLight(vl[0].sky, vl[0].blk);
            uint col = WithAlpha(color, shade);
            mb.Reserve(doubleSided ? 2 : 1);
            EmitQ(mb, a, uva, tex, col, lp); EmitQ(mb, b, uvb, tex, col, lp); EmitQ(mb, c, uvc, tex, col, lp); EmitQ(mb, d, uvd, tex, col, lp);
            mb.QuadIndices(false);
            if (doubleSided)
            {
                EmitQ(mb, d, uvd, tex, col, lp); EmitQ(mb, c, uvc, tex, col, lp); EmitQ(mb, b, uvb, tex, col, lp); EmitQ(mb, a, uva, tex, col, lp);
                mb.QuadIndices(false);
            }
        }

        void EmitQ(MeshBuffer mb, Vector3 p, Vector2 uv, int tex, uint col, uint lp)
        {
            float px = p.x, pz = p.z; RotP(ref px, ref pz);
            Vert(mb, px, p.y, pz, uv.x, uv.y, tex, col, lp);
        }

        /// <summary>Flat quad lying on the floor (rails, redstone, carpets-like decals), slightly raised.</summary>
        public void FloorDecal(int tex, uint color, float h = 1f / 64f, int uvRot = 0, RenderLayer layer = RenderLayer.Cutout)
        {
            var mb = Buf(layer);
            OwnFlatLight();
            uint lp = PackLight(vl[0].sky, vl[0].blk);
            uint col = WithAlpha(color, 1f);
            mb.Reserve(2);
            for (int v = 0; v < 4; v++)
            {
                int uvi = (v + uvRot) & 3;
                Vert(mb, FaceVerts[1, v, 0], h, FaceVerts[1, v, 2], Us[uvi], Vs[uvi], tex, col, lp);
            }
            mb.QuadIndices(false);
            for (int v = 3; v >= 0; v--)
            {
                int uvi = (v + uvRot) & 3;
                Vert(mb, FaceVerts[1, v, 0], h, FaceVerts[1, v, 2], Us[uvi], Vs[uvi], tex, WithAlpha(color, 0.5f), lp);
            }
            mb.QuadIndices(false);
        }

        /// <summary>Flat quad on a wall face (ladders, vines). Face = direction the quad faces (outward from wall).</summary>
        public void WallDecal(Dir facing, int tex, uint color, float inset = 1f / 16f)
        {
            // canonical: quad at z = 1 - inset facing south (-Z) when facing==South... we build per face explicitly.
            var mb = Buf(RenderLayer.Cutout);
            OwnFlatLight();
            uint lp = PackLight(vl[0].sky, vl[0].blk);
            mb.Reserve(2);
            int f = (int)facing;
            // attach to the opposite side: the quad sits 'inset' away from the wall on side opposite(facing)
            int wall = f ^ 1;
            float shade = FaceShade[f];
            uint col = WithAlpha(color, shade);
            for (int pass = 0; pass < 2; pass++)
            {
                for (int k = 0; k < 4; k++)
                {
                    int v = pass == 0 ? k : 3 - k;
                    float px = FaceVerts[wall, v, 0], py = FaceVerts[wall, v, 1], pz = FaceVerts[wall, v, 2];
                    // move inward by inset along facing direction
                    Int3 o = DirUtil.Offset[f];
                    px += o.x * inset; pz += o.z * inset; py += o.y * inset;
                    int uvi = v;
                    float u = Us[uvi], vv = Vs[uvi];
                    if (pass == 0) u = 1 - u; // mirror so texture reads correctly from the front
                    Vert(mb, px, py, pz, u, vv, tex, pass == 0 ? col : WithAlpha(color, shade * 0.8f), lp);
                }
                mb.QuadIndices(false);
            }
        }

        /// <summary>
        /// Box transformed by an arbitrary matrix (block-local space), flat lit. uv: per canonical face rect in 0..1
        /// (x0,y0,x1,y1) or zero for auto. Used for tilted wall torches, lever handles, etc.
        /// </summary>
        public void XBox(Matrix4x4 m, Vector3 from, Vector3 to, int[] tex, Vector4[] uv, uint color, RenderLayer layer, int skipMask = 0)
        {
            var mb = Buf(layer);
            OwnFlatLight();
            uint lp = PackLight(vl[0].sky, vl[0].blk);
            for (int cf = 0; cf < 6; cf++)
            {
                if ((skipMask & (1 << cf)) != 0) continue;
                mb.Reserve(1);
                Vector3 n = m.MultiplyVector(DirUtil.Normal[cf]).normalized;
                float shade = n.y > 0.5f ? 1f : (n.y < -0.5f ? 0.5f : (Mathf.Abs(n.z) > Mathf.Abs(n.x) ? 0.8f : 0.6f));
                uint col = WithAlpha(color, shade);
                for (int v = 0; v < 4; v++)
                {
                    float px = FaceVerts[cf, v, 0] > 0.5f ? to.x : from.x;
                    float py = FaceVerts[cf, v, 1] > 0.5f ? to.y : from.y;
                    float pz = FaceVerts[cf, v, 2] > 0.5f ? to.z : from.z;
                    float u, vv;
                    if (uv != null && uv[cf] != Vector4.zero)
                    {
                        Vector4 r = uv[cf];
                        u = Us[v] < 0.5f ? r.x : r.z; vv = Vs[v] < 0.5f ? r.y : r.w;
                    }
                    else AutoUV(cf, px, py, pz, out u, out vv);
                    Vector3 wp = m.MultiplyPoint3x4(new Vector3(px, py, pz));
                    float wx = wp.x, wz = wp.z; RotP(ref wx, ref wz);
                    Vert(mb, wx, wp.y, wz, u, vv, tex[cf], col, lp);
                }
                mb.QuadIndices(false);
            }
        }

        /// <summary>Pixel-space helper for XBox with a uniform texture and pixel uv rects derived from positions.</summary>
        public void XBoxPx(Matrix4x4 m, float x0, float y0, float z0, float x1, float y1, float z1, int tex, uint color, RenderLayer layer, Vector4[] uv = null, int skipMask = 0)
        {
            tmpTex[0] = tmpTex[1] = tmpTex[2] = tmpTex[3] = tmpTex[4] = tmpTex[5] = tex;
            XBox(m, new Vector3(x0, y0, z0) / 16f, new Vector3(x1, y1, z1) / 16f, tmpTex, uv, color, layer, skipMask);
        }

        // ---------- Liquids ----------
        public static float LiquidHeight(int level) => level >= 8 ? 1f : (8 - level) / 9f;

        float CornerHeight(Block fluid, int ox, int oz)
        {
            // corner shared by cells (0,0),(ox,0),(0,oz),(ox,oz) with ox,oz in {-1,1}
            float sum = 0; int cnt = 0;
            for (int i = 0; i < 4; i++)
            {
                int dx = (i & 1) != 0 ? ox : 0, dz = (i & 2) != 0 ? oz : 0;
                ushort s = N(dx, 0, dz);
                Block b = Blocks.ByState[s];
                if (Blocks.SameFluid(b, fluid))
                {
                    if (Blocks.SameFluid(Blocks.ByState[N(dx, 1, dz)], fluid)) return 1f;
                    int lvl = s - b.baseState;
                    float h = LiquidHeight(lvl);
                    if (lvl == 0) { sum += h * 10; cnt += 10; }
                    else { sum += h; cnt++; }
                }
                else if (!Blocks.StateSolidForFluid[s]) { cnt++; }
            }
            return cnt == 0 ? 0.8889f : sum / cnt;
        }

        public void Liquid(Block fluid, int stillTex, int flowTex, uint color, RenderLayer layer)
        {
            var mb = Buf(layer);
            bool fluidAbove = Blocks.SameFluid(NB(Dir.Up), fluid);
            float h00, h01, h11, h10;
            if (fluidAbove) { h00 = h01 = h11 = h10 = 1f; }
            else
            {
                h00 = CornerHeight(fluid, -1, -1); // x0,z0
                h01 = CornerHeight(fluid, -1, 1);  // x0,z1
                h11 = CornerHeight(fluid, 1, 1);   // x1,z1
                h10 = CornerHeight(fluid, 1, -1);  // x1,z0
            }
            // top face
            if (!fluidAbove && (Blocks.StateOccludes[N(Dir.Up)] & (1 << 0)) == 0)
            {
                ComputeFaceLight(1);
                mb.Reserve(1);
                uint col = WithAlpha(color, 1f);
                bool flowing = !(h00 == h01 && h01 == h11 && h11 == h10);
                int tex = flowing ? flowTex : stillTex;
                Vert(mb, 0, h00, 0, 0, 0, tex, WithAlpha(color, vl[0].ao), PackLight(vl[0].sky, vl[0].blk));
                Vert(mb, 0, h01, 1, 0, 1, tex, WithAlpha(color, vl[1].ao), PackLight(vl[1].sky, vl[1].blk));
                Vert(mb, 1, h11, 1, 1, 1, tex, WithAlpha(color, vl[2].ao), PackLight(vl[2].sky, vl[2].blk));
                Vert(mb, 1, h10, 0, 1, 0, tex, WithAlpha(color, vl[3].ao), PackLight(vl[3].sky, vl[3].blk));
                mb.QuadIndices(false);
            }
            // bottom
            {
                ushort nb = N(Dir.Down);
                if (!Blocks.SameFluid(Blocks.ByState[nb], fluid) && (Blocks.StateOccludes[nb] & (1 << 1)) == 0)
                {
                    ComputeFaceLight(0);
                    mb.Reserve(1);
                    for (int v = 0; v < 4; v++)
                        Vert(mb, FaceVerts[0, v, 0], 0, FaceVerts[0, v, 2], Us[v], Vs[v], stillTex, WithAlpha(color, 0.5f * vl[v].ao), PackLight(vl[v].sky, vl[v].blk));
                    mb.QuadIndices(false);
                }
            }
            // sides
            for (int f = 2; f < 6; f++)
            {
                ushort ns = blocks[ci + NeighborDelta[f]];
                Block nbk = Blocks.ByState[ns];
                if (Blocks.SameFluid(nbk, fluid)) continue;
                if ((Blocks.StateOccludes[ns] & (1 << (f ^ 1))) != 0) continue;
                ComputeFaceLight(f);
                mb.Reserve(1);
                float shade = FaceShade[f];
                for (int v = 0; v < 4; v++)
                {
                    float px = FaceVerts[f, v, 0], py = FaceVerts[f, v, 1], pz = FaceVerts[f, v, 2];
                    if (py > 0.5f)
                    {
                        if (px < 0.5f && pz < 0.5f) py = h00; else if (px < 0.5f) py = h01; else if (pz > 0.5f) py = h11; else py = h10;
                    }
                    Vert(mb, px, py, pz, Us[v], Vs[v] * py, flowTex, WithAlpha(color, shade * vl[v].ao), PackLight(vl[v].sky, vl[v].blk));
                }
                mb.QuadIndices(false);
            }
        }
    }
}
