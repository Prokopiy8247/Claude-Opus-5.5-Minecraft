using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Blender-built FBX meshes that replace the procedural ModelMesher geometry. The FBX pipeline
    /// (Tools/BlenderBridge/fbx_pipeline.py, then MCR.EditorTools.ModelImporter) stores one merged rest-pose mesh per
    /// model in Resources/Models, with a (bone, cube) id per vertex in uv channel 1 and one bind pose per bone. At
    /// runtime that mesh is cut back into one mesh per bone, so the rig, the optional parts and the renderer layout
    /// stay exactly what ModelRenderer builds from the procedural meshes.
    /// </summary>
    public static class BlenderModels
    {
        /// <summary>Prefer the FBX meshes where they exist; false forces the procedural meshes everywhere.</summary>
        public static bool Enabled = true;

        sealed class Entry
        {
            public Mesh mesh;
            public Material mat;
            public bool cut;
            public ModelMesher.BoneMesh[] bones;
        }
        static readonly Dictionary<string, Entry> cache = new Dictionary<string, Entry>();

        /// <summary>The FBX override of a model (mesh and skin material from Resources/Models, loaded once and cached); false when it has none.</summary>
        public static bool TryGet(string name, out Mesh mesh, out Material mat)
        {
            var e = Load(name);
            mesh = e != null ? e.mesh : null;
            mat = e != null ? e.mat : null;
            return mesh != null;
        }

        static Entry Load(string name)
        {
            if (string.IsNullOrEmpty(name)) return null;
            if (!cache.TryGetValue(name, out var e))
            {
                e = new Entry { mesh = Resources.Load<Mesh>("Models/" + name + "_mesh"), mat = Resources.Load<Material>("Models/" + name + "_mat") };
                cache[name] = e;
            }
            return e;
        }

        /// <summary>
        /// Per-bone meshes cut from the model's FBX override, in the layout of ModelMesher.BoneMeshes (a main mesh per
        /// bone plus one mesh per optional cube). Null when disabled, when the model has no override, or when the
        /// override does not fit the ModelDef; the caller then uses the procedural meshes.
        /// </summary>
        public static ModelMesher.BoneMesh[] BoneMeshes(ModelDef def)
        {
            if (!Enabled || def == null) return null;
            var e = Load(def.name);
            if (e == null || e.mesh == null) return null;
            if (!e.cut)
            {
                e.cut = true;
                e.bones = Split(def, e.mesh);
                if (e.bones == null) Debug.LogWarning("[BlenderModels] " + def.name + ": FBX mesh does not fit the model definition, using the procedural mesh");
                else if (!announced)
                {
                    announced = true;
                    Debug.Log("[BlenderModels] Using the Blender FBX meshes from Resources/Models (first: " + def.name + ")");
                }
            }
            return e.bones;
        }
        static bool announced;

        /// <summary>
        /// Bind pose of every bone (model space to bone space, in blocks) for the rest pose ModelRenderer.Build lays out:
        /// each bone at its pivot relative to its parent's pivot, turned by its default rotation.
        /// </summary>
        public static Matrix4x4[] BindPoses(ModelDef def)
        {
            int n = def.bones.Count;
            var world = new Matrix4x4[n];
            var bind = new Matrix4x4[n];
            for (int i = 0; i < n; i++)
            {
                var b = def.bones[i];
                var parent = b.parent != null ? def.Get(b.parent) : null;
                int pi = parent != null ? def.bones.IndexOf(parent) : -1;
                var local = Matrix4x4.TRS((b.pivot - (parent != null ? parent.pivot : Vector3.zero)) / 16f, Quaternion.Euler(b.rotation), Vector3.one);
                world[i] = pi >= 0 && pi < i ? world[pi] * local : local;
                bind[i] = world[i].inverse;
            }
            return bind;
        }

        /// <summary>Cut a merged rest-pose model mesh back into bone-space meshes (see BoneMeshes); null if it does not fit the ModelDef.</summary>
        public static ModelMesher.BoneMesh[] Split(ModelDef def, Mesh src)
        {
            if (def == null || src == null || !src.isReadable) return null;
            var bind = src.bindposes;
            if (bind == null || bind.Length != def.bones.Count) return null;
            var pos = new List<Vector3>(); var nrm = new List<Vector3>(); var uv = new List<Vector2>(); var ids = new List<Vector2>();
            src.GetVertices(pos); src.GetNormals(nrm); src.GetUVs(0, uv); src.GetUVs(1, ids);
            int n = pos.Count;
            if (n == 0 || nrm.Count != n || uv.Count != n || ids.Count != n) return null;

            // triangles grouped by the (bone, cube) id their corners carry; every cube of the definition must be there
            var groups = new Dictionary<long, List<int>>();
            var tris = new List<int>();
            for (int s = 0; s < src.subMeshCount; s++)
            {
                src.GetTriangles(tris, s);
                for (int t = 0; t + 2 < tris.Count; t += 3)
                {
                    int a = tris[t], b = tris[t + 1], c = tris[t + 2];
                    if (ids[a] != ids[b] || ids[a] != ids[c]) return null;
                    int bone = Mathf.RoundToInt(ids[a].x), cube = Mathf.RoundToInt(ids[a].y);
                    if (bone < 0 || bone >= def.bones.Count || cube < 0 || cube >= def.bones[bone].cubes.Count) return null;
                    long key = Key(bone, cube);
                    if (!groups.TryGetValue(key, out var list)) groups[key] = list = new List<int>();
                    list.Add(a); list.Add(b); list.Add(c);
                }
            }
            for (int i = 0; i < def.bones.Count; i++)
                for (int k = 0; k < def.bones[i].cubes.Count; k++)
                    if (!groups.ContainsKey(Key(i, k))) return null;

            var result = new ModelMesher.BoneMesh[def.bones.Count];
            for (int i = 0; i < def.bones.Count; i++)
            {
                var bd = def.bones[i];
                if (bd.cubes.Count == 0) continue;
                var bm = new ModelMesher.BoneMesh();
                var opaque = new List<int>(); var translucent = new List<int>();
                bool hasMain = false;
                for (int k = 0; k < bd.cubes.Count; k++)
                {
                    var cd = bd.cubes[k];
                    var cubeTris = groups[Key(i, k)];
                    if (ModelMesher.IsOptional(def, cd))
                        bm.optional.Add(new KeyValuePair<string, Mesh>(cd.name, Cut(def.name + "_" + bd.name + "__" + cd.name, cd.translucent ? null : cubeTris, cd.translucent ? cubeTris : null, bind[i], pos, nrm, uv)));
                    else
                    {
                        hasMain = true;
                        (cd.translucent ? translucent : opaque).AddRange(cubeTris);
                    }
                }
                if (hasMain) bm.main = Cut(def.name + "_" + bd.name, opaque, translucent, bind[i], pos, nrm, uv);
                result[i] = bm;
            }
            return result;
        }

        static long Key(int bone, int cube) => ((long)bone << 20) | (uint)cube;

        /// <summary>One bone-space mesh from triangles of the merged mesh: submesh 0 opaque, submesh 1 translucent (as ModelMesher.Build).</summary>
        static Mesh Cut(string name, List<int> opaque, List<int> translucent, Matrix4x4 toBone, List<Vector3> pos, List<Vector3> nrm, List<Vector2> uv)
        {
            var remap = new Dictionary<int, int>();
            var v = new List<Vector3>(); var nn = new List<Vector3>(); var uu = new List<Vector2>();
            List<int> Take(List<int> src)
            {
                var o = new List<int>(src != null ? src.Count : 0);
                if (src == null) return o;
                foreach (int i in src)
                {
                    if (!remap.TryGetValue(i, out int j))
                    {
                        j = v.Count;
                        remap[i] = j;
                        v.Add(toBone.MultiplyPoint3x4(pos[i]));
                        nn.Add(toBone.MultiplyVector(nrm[i]).normalized);
                        uu.Add(uv[i]);
                    }
                    o.Add(j);
                }
                return o;
            }
            var t0 = Take(opaque);
            var t1 = Take(translucent);
            var mesh = new Mesh { name = name };
            mesh.SetVertices(v); mesh.SetUVs(0, uu); mesh.SetNormals(nn);
            mesh.subMeshCount = t1.Count > 0 ? 2 : 1;
            mesh.SetTriangles(t0, 0);
            if (t1.Count > 0) mesh.SetTriangles(t1, 1);
            mesh.RecalculateBounds();
            return mesh;
        }
    }
}
