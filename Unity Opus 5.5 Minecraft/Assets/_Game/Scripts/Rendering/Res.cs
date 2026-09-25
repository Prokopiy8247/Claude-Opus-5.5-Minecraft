using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace MCR
{
    /// <summary>Global rendering resources: block texture array, chunk materials, entity materials.</summary>
    public static class Res
    {
        public static Texture2DArray BlockArray;
        public static Material ChunkOpaque, ChunkCutout, ChunkTranslucent, ChunkCutoutNoCull;
        public static Material[] ChunkMats;
        public static Material[] ItemMats;   // for held/dropped items: opaque, cutout (double sided), translucent
        public static Shader ChunkShader, EntityShader, UnlitShader;
        public static bool Ready;
        static readonly Dictionary<Texture, Material> entityMats = new Dictionary<Texture, Material>();
        static readonly Dictionary<Texture, Material> entityMatsT = new Dictionary<Texture, Material>();

        public static void Init()
        {
            if (Ready) return;
            ChunkShader = Shader.Find("MCR/Chunk");
            EntityShader = Shader.Find("MCR/Entity");
            UnlitShader = Shader.Find("MCR/Unlit");
            if (ChunkShader == null) Debug.LogError("MCR/Chunk shader not found (is it included in the build?)");
            BuildBlockArray();
            ChunkOpaque = MakeChunkMat("ChunkOpaque", 0, true, CullMode.Back);
            ChunkCutout = MakeChunkMat("ChunkCutout", 1, true, CullMode.Back);
            ChunkTranslucent = MakeChunkMat("ChunkTranslucent", 2, true, CullMode.Back);
            ChunkCutoutNoCull = MakeChunkMat("ChunkCutoutNoCull", 1, true, CullMode.Off);
            ChunkMats = new[] { ChunkOpaque, ChunkCutout, ChunkTranslucent };
            ItemMats = new[] { MakeChunkMat("ItemOpaque", 0, true, CullMode.Back), MakeChunkMat("ItemCutout", 1, true, CullMode.Off), MakeChunkMat("ItemTranslucent", 2, true, CullMode.Back) };
            Ready = true;
        }

        static Material MakeChunkMat(string name, int mode, bool zwrite, CullMode cull)
        {
            var m = new Material(ChunkShader) { name = name };
            m.SetTexture("_BlockTex", BlockArray);
            m.SetFloat("_Mode", mode);
            m.SetFloat("_Cull", (float)cull);
            if (mode == 2)
            {
                m.SetFloat("_SrcBlend", (float)BlendMode.SrcAlpha);
                m.SetFloat("_DstBlend", (float)BlendMode.OneMinusSrcAlpha);
                m.SetFloat("_ZWrite", 1);
                m.renderQueue = (int)RenderQueue.Transparent;
                m.SetOverrideTag("RenderType", "Transparent");
            }
            else
            {
                m.SetFloat("_SrcBlend", (float)BlendMode.One);
                m.SetFloat("_DstBlend", (float)BlendMode.Zero);
                m.SetFloat("_ZWrite", 1);
                m.renderQueue = mode == 1 ? (int)RenderQueue.AlphaTest : (int)RenderQueue.Geometry;
            }
            m.SetVector("_EntityLight", new Vector4(1, 0, 0, 0));
            m.enableInstancing = false;
            return m;
        }

        public static int NameHash()
        {
            unchecked
            {
                int h = 17;
                foreach (var n in Tex.Names) h = h * 31 + Hash.StringHash(n);
                return h ^ TextureGen.Version;
            }
        }

        static void BuildBlockArray()
        {
            ItemSprites.RegisterTextures();
            ParticleTextures.Register();
            ChunkMesher.PrewarmTextures();
            Tex.Freeze();
            int n = Tex.LayerCount;
            var baked = Resources.Load<Texture2DArray>("Generated/BlockTextures");
            var info = Resources.Load<TextAsset>("Generated/BlockTextures_info");
            if (baked != null && info != null && info.text.Trim() == NameHash().ToString() && baked.depth == n)
            {
                BlockArray = baked;
                BlockArray.filterMode = FilterMode.Point;
                Debug.Log("[Res] Using baked block texture array (" + n + " layers)");
                return;
            }
            BlockArray = GenerateArray();
            Debug.Log("[Res] Generated block texture array at runtime (" + n + " layers)");
        }

        public static Texture2DArray GenerateArray()
        {
            int n = Tex.LayerCount;
            const int S = 16;
            int mips = 5;
            var arr = new Texture2DArray(S, S, n, TextureFormat.RGBA32, mips, false) { name = "MCR_BlockTextures" };
            arr.filterMode = FilterMode.Point;
            arr.wrapMode = TextureWrapMode.Repeat;
            arr.anisoLevel = 0;
            var names = Tex.Names;
            for (int l = 0; l < n; l++)
            {
                var px = LayerPixels(names[l]);
                // flip rows: generator row 0 = top; Unity row 0 = bottom
                var flipped = new Color32[S * S];
                for (int y = 0; y < S; y++) System.Array.Copy(px, y * S, flipped, (S - 1 - y) * S, S);
                arr.SetPixels32(flipped, l, 0);
                var cur = flipped; int size = S;
                for (int m = 1; m < mips; m++)
                {
                    cur = Downsample(cur, size);
                    size /= 2;
                    arr.SetPixels32(cur, l, m);
                }
            }
            arr.Apply(false, false);
            return arr;
        }

        static readonly Dictionary<int, Color32[]> layerPixelCache = new Dictionary<int, Color32[]>();
        /// <summary>CPU copy of a texture-array layer (top-down rows), cached; used by icons and extruded sprites.</summary>
        public static Color32[] GetLayerPixels(int layer)
        {
            if (layerPixelCache.TryGetValue(layer, out var px)) return px;
            try { px = LayerPixels(Tex.NameOf(layer)); } catch { px = new Color32[256]; }
            layerPixelCache[layer] = px;
            return px;
        }

        public static Color32[] LayerPixels(string name)
        {
            if (name.StartsWith("item/")) return ItemSprites.Pixels(name.Substring(5));
            if (name.StartsWith("particle/")) return ParticleTextures.Pixels(name.Substring(9));
            return TextureGen.Generate(name);
        }

        /// <summary>2x downsample preserving cutout coverage.</summary>
        public static Color32[] Downsample(Color32[] src, int size)
        {
            int ns = size / 2;
            var dst = new Color32[ns * ns];
            bool hasZero = false, hasMid = false;
            foreach (var c in src) { if (c.a == 0) hasZero = true; else if (c.a != 255 && c.a != 128) hasMid = true; }
            for (int y = 0; y < ns; y++)
                for (int x = 0; x < ns; x++)
                {
                    int r = 0, g = 0, b = 0, a = 0, cnt = 0, opaque = 0;
                    for (int dy = 0; dy < 2; dy++)
                        for (int dx = 0; dx < 2; dx++)
                        {
                            var c = src[(y * 2 + dy) * size + x * 2 + dx];
                            a += c.a;
                            if (c.a > 0) { r += c.r; g += c.g; b += c.b; cnt++; if (c.a >= 128) opaque++; }
                        }
                    Color32 o;
                    if (cnt == 0) o = new Color32(0, 0, 0, 0);
                    else o = new Color32((byte)(r / cnt), (byte)(g / cnt), (byte)(b / cnt), (byte)(a / 4));
                    if (hasZero && !hasMid) o.a = (byte)(opaque >= 2 ? 255 : 0);
                    dst[y * ns + x] = o;
                }
            return dst;
        }

        // ------------------------------------------------------------------ entity materials
        public static Material EntityMaterial(Texture tex, bool translucent = false)
        {
            if (tex == null) tex = Texture2D.whiteTexture;
            var cacheDict = translucent ? entityMatsT : entityMats;
            if (cacheDict.TryGetValue(tex, out var m)) return m;
            m = new Material(EntityShader) { name = "Entity_" + tex.name };
            m.SetTexture("_MainTex", tex);
            if (translucent)
            {
                m.SetFloat("_SrcBlend", (float)BlendMode.SrcAlpha); m.SetFloat("_DstBlend", (float)BlendMode.OneMinusSrcAlpha);
                m.SetFloat("_ZWrite", 0); m.renderQueue = (int)RenderQueue.Transparent; m.SetFloat("_Cutoff", 0.01f);
            }
            cacheDict[tex] = m;
            return m;
        }

        public static Material UnlitMaterial(Texture tex, bool additive = false, bool depthTest = true, int queue = 3000, bool fog = false)
        {
            var m = new Material(UnlitShader);
            m.SetTexture("_MainTex", tex);
            if (additive) { m.SetFloat("_SrcBlend", (float)BlendMode.SrcAlpha); m.SetFloat("_DstBlend", (float)BlendMode.One); }
            m.SetFloat("_ZTest", depthTest ? (float)CompareFunction.LessEqual : (float)CompareFunction.Always);
            m.SetFloat("_Fog", fog ? 1 : 0);
            m.renderQueue = queue;
            return m;
        }
    }
}
