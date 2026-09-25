using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using UnityEngine;

namespace MCR
{
    public enum RigKind { Biped, Quadruped, Chicken, Spider, Creeper, Slime, Ghast, Blaze, Squid, Fish, Bat, Dragon, Wither, Shulker, Villager, Golem, Horse, Flyer, Static, Crystal, Boat, Minecart, Guardian, Snake }

    /// <summary>One cuboid, in model pixels (1/16 block). Origin = min corner in model space; +Z is the model's front.</summary>
    [Serializable]
    public sealed class CubeDef
    {
        public Vector3 origin, size;
        public Vector2Int uv;
        public float inflate;
        public bool mirror;
        public string name;
        public bool translucent;
        /// <summary>Reuse another cube's UV region (mirrored limbs).</summary>
        [NonSerialized] public CubeDef shareUV;
    }

    [Serializable]
    public sealed class BoneDef
    {
        public string name, parent;
        public Vector3 pivot;       // absolute model-space pivot (pixels)
        public Vector3 rotation;    // default rotation (degrees, applied X then Y then Z)
        public readonly List<CubeDef> cubes = new List<CubeDef>();

        public BoneDef Cube(float x, float y, float z, float w, float h, float d, int u, int v, float inflate = 0, bool mirror = false, bool translucent = false)
        {
            cubes.Add(new CubeDef { origin = new Vector3(x, y, z), size = new Vector3(w, h, d), uv = new Vector2Int(u, v), inflate = inflate, mirror = mirror, translucent = translucent });
            return this;
        }
        /// <summary>Cube with automatically packed UVs.</summary>
        public BoneDef C(float x, float y, float z, float w, float h, float d, float inflate = 0, string name = null, bool translucent = false)
        {
            cubes.Add(new CubeDef { origin = new Vector3(x, y, z), size = new Vector3(w, h, d), uv = new Vector2Int(-1, -1), inflate = inflate, name = name, translucent = translucent });
            return this;
        }
        /// <summary>Cube sharing (mirroring) the UV region of another cube.</summary>
        public BoneDef CM(float x, float y, float z, float w, float h, float d, CubeDef src, float inflate = 0)
        {
            cubes.Add(new CubeDef { origin = new Vector3(x, y, z), size = new Vector3(w, h, d), uv = new Vector2Int(-1, -1), inflate = inflate, mirror = true, shareUV = src });
            return this;
        }
        public CubeDef Last => cubes[cubes.Count - 1];
    }

    /// <summary>A blocky articulated model: bones with cuboids and Minecraft-style box UVs on a single skin texture.</summary>
    public sealed class ModelDef
    {
        public string name;
        public int texW = 64, texH = 64;
        public RigKind rig;
        public float scale = 1f;       // extra world scale applied by the renderer
        public readonly List<BoneDef> bones = new List<BoneDef>();
        readonly Dictionary<string, BoneDef> byName = new Dictionary<string, BoneDef>();
        public string skin;            // skin painter id (defaults to name)

        public ModelDef(string name, int tw, int th, RigKind rig) { this.name = name; texW = tw; texH = th; this.rig = rig; skin = name; }

        public BoneDef Bone(string name, string parent, float px, float py, float pz, float rx = 0, float ry = 0, float rz = 0)
        {
            var b = new BoneDef { name = name, parent = parent, pivot = new Vector3(px, py, pz), rotation = new Vector3(rx, ry, rz) };
            bones.Add(b); byName[name] = b;
            return b;
        }
        public BoneDef Get(string n) => n != null && byName.TryGetValue(n, out var b) ? b : null;

        /// <summary>Shelf-pack all cubes with uv (-1,-1) into the texture, growing it as needed. Mirrored cubes reuse their source UVs.</summary>
        public void PackUVs()
        {
            var list = new List<CubeDef>();
            foreach (var b in bones) foreach (var c in b.cubes) if (c.uv.x < 0 && c.shareUV == null) list.Add(c);
            list.Sort((a, b) => (b.size.z + b.size.y).CompareTo(a.size.z + a.size.y));
            int W = texW, H = texH;
            for (int attempt = 0; attempt < 8; attempt++)
            {
                int x = 0, y = 0, rowH = 0; bool ok = true;
                var tmp = new Dictionary<CubeDef, Vector2Int>();
                foreach (var c in list)
                {
                    int w = Mathf.CeilToInt(c.size.x - 0.001f), h = Mathf.CeilToInt(c.size.y - 0.001f), d = Mathf.CeilToInt(c.size.z - 0.001f);
                    int rw = 2 * (w + d), rh = d + h;
                    if (rw > W) { ok = false; break; }
                    if (x + rw > W) { x = 0; y += rowH; rowH = 0; }
                    if (y + rh > H) { ok = false; break; }
                    tmp[c] = new Vector2Int(x, y);
                    x += rw; rowH = Mathf.Max(rowH, rh);
                }
                if (ok) { foreach (var kv in tmp) kv.Key.uv = kv.Value; texW = W; texH = H; break; }
                if (H < W) H *= 2; else W *= 2;
            }
            foreach (var b in bones) foreach (var c in b.cubes) if (c.shareUV != null) c.uv = c.shareUV.uv;
        }
        public bool Has(string n) => byName.ContainsKey(n);

        /// <summary>Faces of a box in the skin (u, v, w, h) in texture pixels, MC box layout. Index: 0 top,1 bottom,2 right(+X),3 front(+Z),4 left(-X),5 back(-Z).</summary>
        public static RectInt[] BoxFaces(CubeDef c)
        {
            int w = Mathf.CeilToInt(c.size.x - 0.001f), h = Mathf.CeilToInt(c.size.y - 0.001f), d = Mathf.CeilToInt(c.size.z - 0.001f);
            int u = c.uv.x, v = c.uv.y;
            return new[]
            {
                new RectInt(u + d, v, w, d),            // top
                new RectInt(u + d + w, v, w, d),        // bottom
                new RectInt(u, v + d, d, h),            // right (+X)
                new RectInt(u + d, v + d, w, h),        // front (+Z)
                new RectInt(u + d + w, v + d, d, h),    // left (-X)
                new RectInt(u + d + w + d, v + d, w, h) // back (-Z)
            };
        }

        public float HeightPx
        {
            get
            {
                float top = 0;
                foreach (var b in bones) foreach (var c in b.cubes) top = Mathf.Max(top, c.origin.y + c.size.y);
                return top;
            }
        }

        // ------------------------------------------------------------------ JSON export for the Blender pipeline
        public string ToJson()
        {
            var ci = CultureInfo.InvariantCulture;
            string F(float f) => f.ToString("0.####", ci);
            var sb = new StringBuilder();
            sb.Append("{\"name\":\"").Append(name).Append("\",\"texW\":").Append(texW).Append(",\"texH\":").Append(texH).Append(",\"rig\":\"").Append(rig).Append("\",\"scale\":").Append(F(scale)).Append(",\"bones\":[");
            for (int i = 0; i < bones.Count; i++)
            {
                var b = bones[i];
                if (i > 0) sb.Append(',');
                sb.Append("{\"name\":\"").Append(b.name).Append("\",\"parent\":").Append(b.parent == null ? "null" : "\"" + b.parent + "\"");
                sb.Append(",\"pivot\":[").Append(F(b.pivot.x)).Append(',').Append(F(b.pivot.y)).Append(',').Append(F(b.pivot.z)).Append(']');
                sb.Append(",\"rot\":[").Append(F(b.rotation.x)).Append(',').Append(F(b.rotation.y)).Append(',').Append(F(b.rotation.z)).Append(']');
                sb.Append(",\"cubes\":[");
                for (int k = 0; k < b.cubes.Count; k++)
                {
                    var c = b.cubes[k];
                    if (k > 0) sb.Append(',');
                    sb.Append("{\"o\":[").Append(F(c.origin.x)).Append(',').Append(F(c.origin.y)).Append(',').Append(F(c.origin.z)).Append("],\"s\":[").Append(F(c.size.x)).Append(',').Append(F(c.size.y)).Append(',').Append(F(c.size.z)).Append("],\"uv\":[").Append(c.uv.x).Append(',').Append(c.uv.y).Append("],\"inf\":").Append(F(c.inflate)).Append(",\"mir\":").Append(c.mirror ? "true" : "false").Append('}');
                }
                sb.Append("]}");
            }
            sb.Append("]}");
            return sb.ToString();
        }
    }

    /// <summary>Builds Unity meshes (one per bone) from a ModelDef — identical geometry/UVs to the Blender builder.</summary>
    public static class ModelMesher
    {
        /// <summary>Cubes that can be toggled at runtime (sheared wool, carved pumpkin, saddles, chests, harness) get their own mesh.</summary>
        public static bool IsOptional(ModelDef def, CubeDef c) => c.name != null && (def.name == "armor" || c.name.StartsWith("wool") || c.name == "pumpkin" || c.name == "carpet" || c.name.StartsWith("saddle") || c.name.StartsWith("chest_") || c.name.StartsWith("harness"));

        public sealed class BoneMesh
        {
            public Mesh main;
            public readonly List<KeyValuePair<string, Mesh>> optional = new List<KeyValuePair<string, Mesh>>();
        }
        static readonly Dictionary<string, BoneMesh[]> cache = new Dictionary<string, BoneMesh[]>();

        /// <summary>Mesh for each bone (null if no cubes); vertices relative to the bone pivot, in blocks.</summary>
        public static BoneMesh[] BoneMeshes(ModelDef def)
        {
            if (cache.TryGetValue(def.name, out var m)) return m;
            m = new BoneMesh[def.bones.Count];
            for (int i = 0; i < def.bones.Count; i++)
            {
                var b = def.bones[i];
                if (b.cubes.Count == 0) continue;
                var bm = new BoneMesh();
                var main = new List<CubeDef>();
                foreach (var c in b.cubes)
                {
                    if (IsOptional(def, c)) bm.optional.Add(new KeyValuePair<string, Mesh>(c.name, Build(def, b, new List<CubeDef> { c }, def.name + "_" + b.name + "__" + c.name)));
                    else main.Add(c);
                }
                if (main.Count > 0) bm.main = Build(def, b, main, def.name + "_" + b.name);
                m[i] = bm;
            }
            cache[def.name] = m;
            return m;
        }

        public static Mesh Build(ModelDef def, BoneDef b, List<CubeDef> cubes, string name)
        {
            var verts = new List<Vector3>(); var uvs = new List<Vector2>(); var norms = new List<Vector3>(); var tris = new List<int>(); var trisT = new List<int>();
            foreach (var c in cubes) AddCube(def, b, c, verts, uvs, norms, c.translucent ? trisT : tris);
            var mesh = new Mesh { name = name };
            mesh.SetVertices(verts); mesh.SetUVs(0, uvs); mesh.SetNormals(norms);
            mesh.subMeshCount = trisT.Count > 0 ? 2 : 1;
            mesh.SetTriangles(tris, 0);
            if (trisT.Count > 0) mesh.SetTriangles(trisT, 1);
            mesh.RecalculateBounds();
            return mesh;
        }

        public static void ClearCache() { cache.Clear(); }

        static void AddCube(ModelDef def, BoneDef b, CubeDef c, List<Vector3> v, List<Vector2> uv, List<Vector3> n, List<int> t)
        {
            float inf = c.inflate;
            Vector3 mn = c.origin - Vector3.one * inf - b.pivot, mx = c.origin + c.size + Vector3.one * inf - b.pivot;
            mn /= 16f; mx /= 16f;
            var faces = ModelDef.BoxFaces(c);
            float tw = def.texW, th = def.texH;
            // corner helper
            Vector3 P(float x, float y, float z) => new Vector3(x, y, z);
            // Each face: 4 corners (bl, tl, tr, br as seen from outside), uv rect (u0 left, u1 right, v top, v bottom in texture pixels)
            void Face(Vector3 bl, Vector3 tl, Vector3 tr, Vector3 br, RectInt r, Vector3 normal, bool flipU = false, bool flipV = false)
            {
                float u0 = r.x / tw, u1 = (r.x + r.width) / tw;
                float vt = 1f - r.y / th, vb = 1f - (r.y + r.height) / th;
                if (flipU ^ c.mirror) { float tmp = u0; u0 = u1; u1 = tmp; }
                if (flipV) { float tmp = vt; vt = vb; vb = tmp; }
                int s = v.Count;
                v.Add(bl); v.Add(tl); v.Add(tr); v.Add(br);
                uv.Add(new Vector2(u0, vb)); uv.Add(new Vector2(u0, vt)); uv.Add(new Vector2(u1, vt)); uv.Add(new Vector2(u1, vb));
                for (int k = 0; k < 4; k++) n.Add(normal);
                // Unity: clockwise winding = front face
                t.Add(s); t.Add(s + 1); t.Add(s + 2); t.Add(s); t.Add(s + 2); t.Add(s + 3);
            }
            // front (+Z): viewed from +Z looking -Z: viewer's left is +X
            Face(P(mx.x, mn.y, mx.z), P(mx.x, mx.y, mx.z), P(mn.x, mx.y, mx.z), P(mn.x, mn.y, mx.z), faces[3], Vector3.forward);
            // back (-Z): viewer's left is -X
            Face(P(mn.x, mn.y, mn.z), P(mn.x, mx.y, mn.z), P(mx.x, mx.y, mn.z), P(mx.x, mn.y, mn.z), faces[5], Vector3.back);
            // right (+X) side: viewed from +X, viewer's left is -Z (back)
            Face(P(mx.x, mn.y, mn.z), P(mx.x, mx.y, mn.z), P(mx.x, mx.y, mx.z), P(mx.x, mn.y, mx.z), faces[2], Vector3.right);
            // left (-X): viewed from -X, viewer's left is +Z (front)
            Face(P(mn.x, mn.y, mx.z), P(mn.x, mx.y, mx.z), P(mn.x, mx.y, mn.z), P(mn.x, mn.y, mn.z), faces[4], Vector3.left);
            // top: viewed from above, texture top edge = back (-Z), left = +X ... keep consistent with painter: top rect row 0 = back
            Face(P(mx.x, mx.y, mx.z), P(mx.x, mx.y, mn.z), P(mn.x, mx.y, mn.z), P(mn.x, mx.y, mx.z), faces[0], Vector3.up);
            // bottom: viewed from below
            Face(P(mx.x, mn.y, mn.z), P(mx.x, mn.y, mx.z), P(mn.x, mn.y, mx.z), P(mn.x, mn.y, mn.z), faces[1], Vector3.down);
        }
    }
}
