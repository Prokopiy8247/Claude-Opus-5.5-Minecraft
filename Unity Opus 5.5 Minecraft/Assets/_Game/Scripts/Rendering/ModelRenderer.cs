using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Builds a hierarchy of GameObjects from a ModelDef (one per bone, one mesh per bone) and exposes the transforms.</summary>
    public static class ModelRenderer
    {
        public static GameObject Build(ModelDef def, Transform parent, string skinOverride = null, bool splitLayers = false)
        {
            if (def == null) return null;
            var root = new GameObject(def.name);
            root.transform.SetParent(parent, false);
            // Blender FBX meshes (Resources/Models) win over the procedural ones; both come in the same per-bone layout
            var meshes = BlenderModels.BoneMeshes(def) ?? ModelMesher.BoneMeshes(def);
            var data = ModelSkins.Get(def.skin ?? def.name, def.texW, def.texH, skinOverride);
            // submesh 0 = opaque/cutout cubes, submesh 1 = translucent cubes (slime shells, crystal glass, ghost wings)
            var mats = new Material[2];
            mats[0] = Res.EntityMaterial(data.opaque);
            mats[1] = Res.EntityMaterial(data.opaque, true);
            var bones = new Transform[def.bones.Count];
            for (int i = 0; i < def.bones.Count; i++)
            {
                var b = def.bones[i];
                GameObject go = new GameObject(b.name);
                Transform parentT = root.transform;
                Vector3 parentPivot = Vector3.zero;
                if (b.parent != null && def.Get(b.parent) != null)
                {
                    int pi = def.bones.IndexOf(def.Get(b.parent));
                    parentT = bones[pi];
                    parentPivot = def.Get(b.parent).pivot;
                }
                go.transform.SetParent(parentT, false);
                bones[i] = go.transform;
                // vertex data is relative to this bone's pivot; the child transform carries the pivot offset from its parent
                go.transform.localPosition = (b.pivot - parentPivot) / 16f;
                go.transform.localRotation = Quaternion.Euler(b.rotation);
                var bm = meshes[i];
                if (bm == null || bm.main == null && bm.optional.Count == 0) continue;
                var parts = new List<GameObject>();
                if (bm.main != null) parts.Add(AddPart(go, bm.main, mats, b.name + "_main"));
                foreach (var kv in bm.optional) parts.Add(AddPart(go, kv.Value, mats, kv.Key));
            }
            return root;
        }

        static GameObject AddPart(GameObject parent, Mesh mesh, Material[] mats, string name)
        {
            var go = new GameObject(name);
            go.transform.SetParent(parent.transform, false);
            var mf = go.AddComponent<MeshFilter>();
            mf.sharedMesh = mesh;
            var mr = go.AddComponent<MeshRenderer>();
            var list = new List<Material>();
            for (int l = 0; l < mesh.subMeshCount && l < mats.Length; l++) list.Add(mats[l]);
            mr.sharedMaterials = list.ToArray();
            mr.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
            mr.receiveShadows = false;
            mr.lightProbeUsage = UnityEngine.Rendering.LightProbeUsage.Off;
            mr.reflectionProbeUsage = UnityEngine.Rendering.ReflectionProbeUsage.Off;
            return go;
        }

        /// <summary>Show or hide an optional cube by its model name (e.g. "wool", "pumpkin", "saddle").</summary>
        public static bool SetOptional(GameObject root, string cubeName, bool visible)
        {
            if (root == null) return false;
            bool found = false;
            foreach (var t in root.GetComponentsInChildren<Transform>(true))
            {
                if (t.name == cubeName) { t.gameObject.SetActive(visible); found = true; }
            }
            return found;
        }

        public static Transform FindBone(Transform root, string name)
        {
            if (root == null) return null;
            if (root.name == name) return root;
            for (int i = 0; i < root.childCount; i++)
            {
                var r = FindBone(root.GetChild(i), name);
                if (r != null) return r;
            }
            return null;
        }
    }

    /// <summary>Procedurally painted model skins: one texture per model, painter chosen by model name (original artwork).</summary>
    public static class ModelSkins
    {
        public sealed class SkinData
        {
            public Texture2D opaque;
            public Color32[] pixels;
        }
        static readonly Dictionary<string, SkinData> cache = new Dictionary<string, SkinData>();

        public static SkinData Get(string skin, int w, int h, string variantSalt = null)
        {
            string key = skin + (variantSalt != null ? "#" + variantSalt : "");
            if (cache.TryGetValue(key, out var d)) return d;
            var px = new Color32[w * h];
            try { SkinPainter.Paint(skin, variantSalt, px, w, h); }
            catch (Exception e) { Debug.LogWarning("Skin paint failed for " + skin + ": " + e.Message); FillFlat(px, w, h); }
            d = new SkinData { pixels = px };
            d.opaque = MakeTex(px, w, h, key, false);
            cache[key] = d;
            return d;
        }

        static void FillFlat(Color32[] px, int w, int h)
        {
            for (int i = 0; i < px.Length; i++) px[i] = new Color32(200, (byte)((i * 7) % 255), 200, 255);
        }

        static Texture2D MakeTex(Color32[] px, int w, int h, string tag, bool filter)
        {
            var t = new Texture2D(w, h, TextureFormat.RGBA32, false) { name = "skin_" + tag };
            // a baked skin (exported for the Blender pipeline) takes precedence when present

            // Unity textures are bottom-up: model painters write top-down
            var flipped = new Color32[px.Length];
            for (int y = 0; y < h; y++) Array.Copy(px, y * w, flipped, (h - 1 - y) * w, w);
            t.SetPixels32(flipped);
            t.filterMode = filter ? FilterMode.Point : FilterMode.Point;
            t.wrapMode = TextureWrapMode.Clamp;
            t.Apply(false, false);
            return t;
        }

        public static void ClearCache()
        {
            foreach (var d in cache.Values)
            {
                if (d.opaque != null) UnityEngine.Object.Destroy(d.opaque);
            }
            cache.Clear();
        }
    }

    /// <summary>Draws skin pixels into the box-UV layout of a ModelDef (front/back/side/top/bottom regions per cube).</summary>
    public sealed class SkinCanvas
    {
        public readonly Color32[] px;
        public readonly int W, H;
        public RNG rng;
        public SkinCanvas(Color32[] px, int w, int h, string seed)
        {
            this.px = px; W = w; H = h;
            rng = new RNG(Hash.StringHash(seed));
        }
        public void Set(int x, int y, Color32 c)
        {
            if (x < 0 || y < 0 || x >= W || y >= H) return;
            px[y * W + x] = c;
        }
        public Color32 Get(int x, int y) => (x < 0 || y < 0 || x >= W || y >= H) ? default : px[y * W + x];
        public void Fill(int x, int y, int w, int h, Color32 c) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) Set(x + i, y + j, c); }
        public void Rect(int x, int y, int w, int h, Color32 c, float noise = 0.06f)
        {
            for (int j = 0; j < h; j++)
                for (int i = 0; i < w; i++)
                {
                    float f = 1f + (rng.NextFloat() - 0.5f) * 2f * noise;
                    Set(x + i, y + j, Img.Shade(c, f));
                }
        }
        public void Speck(int x, int y, int w, int h, Color32 c, float density)
        {
            for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) if (rng.NextFloat() < density) Set(x + i, y + j, Img.Shade(c, 0.85f + rng.NextFloat() * 0.3f));
        }
        public void Line(int x0, int y0, int x1, int y1, Color32 c)
        {
            int steps = Mathf.Max(Mathf.Abs(x1 - x0), Mathf.Abs(y1 - y0)) * 2 + 1;
            for (int i = 0; i <= steps; i++)
            {
                float t = i / (float)steps;
                Set(Mathf.RoundToInt(Mathf.Lerp(x0, x1, t)), Mathf.RoundToInt(Mathf.Lerp(y0, y1, t)), c);
            }
        }
        /// <summary>Fill only the front face region of a cube's uv box.</summary>
        public void Face(CubeDef c, int face, Color32 color, float noise = 0.06f)
        {
            var f = ModelDef.BoxFaces(c)[face];
            Rect(f.x, f.y, f.width, f.height, color, noise);
        }
        public void AllFaces(CubeDef c, Color32 color, float noise = 0.06f)
        {
            var all = ModelDef.BoxFaces(c);
            foreach (var f in all) Rect(f.x, f.y, f.width, f.height, color, noise);
        }
        /// <summary>Region of a face as (x, y, w, h).</summary>
        public RectInt R(CubeDef c, int face) => ModelDef.BoxFaces(c)[face];
    }
}
