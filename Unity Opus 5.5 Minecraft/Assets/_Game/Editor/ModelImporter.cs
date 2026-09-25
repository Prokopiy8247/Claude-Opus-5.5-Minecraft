using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace MCR.EditorTools
{
    /// <summary>
    /// Brings the Blender-built FBX models into the game. Tools/BlenderBridge/fbx_pipeline.py writes one &lt;name&gt;.fbx
    /// per ModelDef into Assets/_Game/Generated/Models (skins beside them in Textures/); this fixes the importer
    /// settings, saves a prefab per model and copies mesh + material into Resources/Models, where MCR.BlenderModels
    /// picks them up at runtime. A model whose FBX no longer matches its ModelDef is reported and left out, so the game
    /// keeps the procedural mesh for it. Unity's importer is spelled UnityEditor.ModelImporter here (same class name).
    /// </summary>
    public static class ModelImporter
    {
        public const string ModelsDir = "Assets/_Game/Generated/Models";
        public const string TexturesDir = ModelsDir + "/Textures";
        public const string PrefabsDir = "Assets/_Game/Generated/Prefabs";
        public const string ResourcesDir = "Assets/_Game/Generated/Resources/Models";
        static string ProjectRoot => Path.GetFullPath(Path.Combine(Application.dataPath, ".."));
        static string OutDir { get { var d = Path.Combine(ProjectRoot, "Tools/_out"); Directory.CreateDirectory(d); return d; } }

        static List<string> FbxNames()
        {
            if (!Directory.Exists(ModelsDir)) return new List<string>();
            return Directory.GetFiles(ModelsDir, "*.fbx").Select(Path.GetFileNameWithoutExtension).OrderBy(n => n, StringComparer.Ordinal).ToList();
        }

        // ------------------------------------------------------------------ import
        /// <summary>
        /// Importer settings, prefabs, Resources copies and the Tools/_out/models.txt report for every Blender FBX.
        /// Never throws, so the Windows build carries on (with procedural meshes) if something here goes wrong.
        /// </summary>
        [MenuItem("Tools/Opus 5.5 Minecraft/Import Blender Models")]
        public static void ImportBlenderModels()
        {
            var t0 = DateTime.Now;
            try
            {
                var names = FbxNames();
                SyncSkins(names);
                AssetDatabase.Refresh();
                AssetDatabase.StartAssetEditing();
                try
                {
                    foreach (var n in names) ConfigureTexture(TexturesDir + "/" + n + ".png");
                    foreach (var n in names) ConfigureModel(ModelsDir + "/" + n + ".fbx");
                }
                finally { AssetDatabase.StopAssetEditing(); }
                EnsureFolder(PrefabsDir);
                int total = 0;
                foreach (var n in names)
                {
                    string path = ModelsDir + "/" + n + ".fbx";
                    var model = AssetDatabase.LoadAssetAtPath<GameObject>(path);
                    var mesh = LoadMesh(path);
                    if (model == null || mesh == null) { Debug.LogError("[Models] " + n + ": the FBX did not import"); continue; }
                    SavePrefab(model, PrefabsDir + "/" + n + ".prefab");
                    int tris = Triangles(mesh);
                    total += tris;
                    Debug.Log("[Models] " + n + ": " + tris + " triangles, " + mesh.vertexCount + " vertices");
                }
                CopyMeshesToResources();
                ValidateModels();
                Debug.Log("[Models] Imported " + names.Count + " Blender models, " + total + " triangles, in " + (DateTime.Now - t0).TotalSeconds.ToString("0.0") + "s");
            }
            catch (Exception e)
            {
                Debug.LogError("[Models] Blender model import failed: " + e);
                File.WriteAllText(Path.Combine(OutDir, "models.txt"), "EXCEPTION " + e + "\n");
            }
            // the voxel items and props (Models/Items, Models/Props) come from the same Blender pipeline; own try and report
            ItemModelImporter.ImportItemModels();
        }

        /// <summary>The skins are painted by code; keep the FBX textures identical to the latest export (ExportModelData).</summary>
        static void SyncSkins(List<string> names)
        {
            string src = Path.Combine(ProjectRoot, "Tools/BlenderBridge/export/skins");
            Directory.CreateDirectory(TexturesDir);
            foreach (var n in names)
            {
                string from = Path.Combine(src, n + ".png"), to = Path.Combine(TexturesDir, n + ".png");
                if (!File.Exists(from)) continue;
                var bytes = File.ReadAllBytes(from);
                if (File.Exists(to) && File.ReadAllBytes(to).SequenceEqual(bytes)) continue;
                File.WriteAllBytes(to, bytes);
            }
        }

        /// <summary>Pixel-art skin: point filtered, uncompressed, no mipmaps, never rescaled (some skins are not powers of two).</summary>
        internal static void ConfigureTexture(string path)
        {
            var ti = AssetImporter.GetAtPath(path) as TextureImporter;
            if (ti == null) return;
            if (ti.textureType == TextureImporterType.Default && ti.filterMode == FilterMode.Point && !ti.mipmapEnabled
                && ti.textureCompression == TextureImporterCompression.Uncompressed && ti.wrapMode == TextureWrapMode.Clamp
                && ti.npotScale == TextureImporterNPOTScale.None && !ti.alphaIsTransparency) return;
            ti.textureType = TextureImporterType.Default;
            ti.filterMode = FilterMode.Point;
            ti.mipmapEnabled = false;
            ti.textureCompression = TextureImporterCompression.Uncompressed;
            ti.wrapMode = TextureWrapMode.Clamp;
            ti.npotScale = TextureImporterNPOTScale.None;
            ti.alphaIsTransparency = false;
            ti.SaveAndReimport();
        }

        /// <summary>
        /// Blocks stay blocks (scale 1, file scale ignored: the FBX is written in Blender units = blocks), no animation,
        /// materials from the FBX material description (Unity 6 replaced the old importMaterials flag with this mode),
        /// readable and uncompressed mesh, and no vertex welding, which would merge the box-UV seams.
        /// </summary>
        internal static void ConfigureModel(string path)
        {
            var mi = AssetImporter.GetAtPath(path) as UnityEditor.ModelImporter;
            if (mi == null) return;
            bool dirty = false;
            void Set<T>(T current, T wanted, Action<T> apply)
            {
                if (EqualityComparer<T>.Default.Equals(current, wanted)) return;
                apply(wanted);
                dirty = true;
            }
            Set(mi.globalScale, 1f, v => mi.globalScale = v);
            Set(mi.useFileScale, false, v => mi.useFileScale = v);
            Set(mi.bakeAxisConversion, false, v => mi.bakeAxisConversion = v);
            Set(mi.importAnimation, false, v => mi.importAnimation = v);
            Set(mi.animationType, ModelImporterAnimationType.None, v => mi.animationType = v);
            Set(mi.materialImportMode, ModelImporterMaterialImportMode.ImportViaMaterialDescription, v => mi.materialImportMode = v);
            Set(mi.isReadable, true, v => mi.isReadable = v);
            Set(mi.meshCompression, ModelImporterMeshCompression.Off, v => mi.meshCompression = v);
            Set(mi.weldVertices, false, v => mi.weldVertices = v);
            Set(mi.importNormals, ModelImporterNormals.Import, v => mi.importNormals = v);
            Set(mi.importTangents, ModelImporterTangents.None, v => mi.importTangents = v);
            Set(mi.generateSecondaryUV, false, v => mi.generateSecondaryUV = v);   // uv channel 1 carries the bone/cube ids
            Set(mi.importBlendShapes, false, v => mi.importBlendShapes = v);
            Set(mi.importCameras, false, v => mi.importCameras = v);
            Set(mi.importLights, false, v => mi.importLights = v);
            Set(mi.importVisibility, false, v => mi.importVisibility = v);
            Set(mi.addCollider, false, v => mi.addCollider = v);
            if (dirty) mi.SaveAndReimport();
        }

        static void SavePrefab(GameObject model, string path)
        {
            var scene = EditorSceneManager.NewPreviewScene();
            try
            {
                var inst = (GameObject)PrefabUtility.InstantiatePrefab(model, scene);
                PrefabUtility.SaveAsPrefabAsset(inst, path);
            }
            finally { EditorSceneManager.ClosePreviewScene(scene); }
        }

        // ------------------------------------------------------------------ Resources copies
        /// <summary>
        /// For every imported FBX: a Mesh asset (the FBX mesh plus bone weights and bind poses from its ModelDef, so it is
        /// also a valid skinned mesh) and an MCR/Entity Material with the skin, saved as Resources/Models/&lt;name&gt;_mesh.asset
        /// and &lt;name&gt;_mat.asset for Resources.Load. A model whose FBX does not reproduce its ModelDef is skipped and its old
        /// copies removed, so the runtime keeps the procedural mesh for it.
        /// </summary>
        public static void CopyMeshesToResources()
        {
            EnsureFolder(ResourcesDir);
            var shader = Shader.Find("MCR/Entity");
            if (shader == null) Debug.LogError("[Models] MCR/Entity shader missing, no model materials written");
            var names = FbxNames();
            int copied = 0;
            foreach (var n in names)
            {
                string meshPath = ResourcesDir + "/" + n + "_mesh.asset", matPath = ResourcesDir + "/" + n + "_mat.asset";
                var def = MobModels.Get(n);
                var src = LoadMesh(ModelsDir + "/" + n + ".fbx");
                Mesh mesh = def != null && src != null ? RuntimeMesh(def, src) : null;
                string problem = def == null ? "no ModelDef named " + n : src == null ? "FBX mesh missing" : Check(def, mesh);
                if (problem != null)
                {
                    Debug.LogWarning("[Models] " + n + ": not copied to Resources, " + problem);
                    if (mesh != null) UnityEngine.Object.DestroyImmediate(mesh);
                    AssetDatabase.DeleteAsset(meshPath);
                    AssetDatabase.DeleteAsset(matPath);
                    continue;
                }
                SaveAsset(mesh, meshPath);
                if (shader != null)
                {
                    var mat = new Material(shader) { name = n + "_mat" };
                    mat.SetTexture("_MainTex", AssetDatabase.LoadAssetAtPath<Texture2D>(TexturesDir + "/" + n + ".png"));
                    SaveAsset(mat, matPath);
                }
                copied++;
            }
            // copies whose FBX is gone
            foreach (var f in Directory.GetFiles(ResourcesDir, "*_mesh.asset"))
            {
                string file = Path.GetFileName(f), n = file.Substring(0, file.Length - "_mesh.asset".Length);
                if (names.Contains(n)) continue;
                AssetDatabase.DeleteAsset(ResourcesDir + "/" + file);
                AssetDatabase.DeleteAsset(ResourcesDir + "/" + n + "_mat.asset");
            }
            AssetDatabase.SaveAssets();
            Debug.Log("[Models] " + copied + "/" + names.Count + " model meshes copied to " + ResourcesDir);
        }

        /// <summary>The FBX mesh with bone weights (bone id from uv channel 1) and the ModelDef's bind poses.</summary>
        static Mesh RuntimeMesh(ModelDef def, Mesh src)
        {
            var mesh = UnityEngine.Object.Instantiate(src);
            mesh.name = def.name + "_mesh";
            var ids = new List<Vector2>();
            mesh.GetUVs(1, ids);
            if (ids.Count == mesh.vertexCount)
            {
                var weights = new BoneWeight[ids.Count];
                for (int i = 0; i < weights.Length; i++)
                    weights[i] = new BoneWeight { boneIndex0 = Mathf.Clamp(Mathf.RoundToInt(ids[i].x), 0, def.bones.Count - 1), weight0 = 1f };
                mesh.boneWeights = weights;
            }
            mesh.bindposes = BlenderModels.BindPoses(def);
            return mesh;
        }

        // ------------------------------------------------------------------ validation
        /// <summary>Writes Tools/_out/models.txt: per model whether its FBX imported, its triangle count, and whether the runtime copy reproduces the ModelDef.</summary>
        [MenuItem("Tools/Opus 5.5 Minecraft/Validate Models")]
        public static void ValidateModels()
        {
            var rows = new StringBuilder();
            int defs = 0, fbx = 0, imported = 0, matched = 0, inResources = 0, prefabs = 0;
            var failed = new List<string>();
            foreach (var def in MobModels.All.OrderBy(d => d.name, StringComparer.Ordinal))
            {
                defs++;
                string path = ModelsDir + "/" + def.name + ".fbx";
                bool hasFile = File.Exists(path);
                var mesh = hasFile ? LoadMesh(path) : null;
                var model = hasFile ? AssetDatabase.LoadAssetAtPath<GameObject>(path) : null;
                var copy = AssetDatabase.LoadAssetAtPath<Mesh>(ResourcesDir + "/" + def.name + "_mesh.asset");
                bool mat = AssetDatabase.LoadAssetAtPath<Material>(ResourcesDir + "/" + def.name + "_mat.asset") != null;
                bool prefab = AssetDatabase.LoadAssetAtPath<GameObject>(PrefabsDir + "/" + def.name + ".prefab") != null;
                string match;
                if (copy != null) match = Check(def, copy) ?? "ok";
                else if (mesh != null)
                {
                    var probe = RuntimeMesh(def, mesh);
                    match = "NOT COPIED: " + (Check(def, probe) ?? "rerun Import Blender Models");
                    UnityEngine.Object.DestroyImmediate(probe);
                }
                else match = "-";
                if (hasFile) fbx++;
                if (mesh != null) imported++;
                if (copy != null && mat) inResources++;
                if (prefab) prefabs++;
                if (match == "ok") matched++;
                else failed.Add(def.name);
                rows.AppendLine(string.Format("{0,-20} {1,-9} {2,6} {3,6}  {4,-6} {5,-9} {6,-6} {7}", def.name,
                    !hasFile ? "missing" : mesh != null ? "imported" : "FAILED", mesh != null ? Triangles(mesh).ToString() : "-",
                    mesh != null ? mesh.vertexCount.ToString() : "-", model != null ? Transforms(model) : "-",
                    copy != null && mat ? "yes" : "no", prefab ? "yes" : "no", match));
            }
            var sb = new StringBuilder();
            sb.AppendLine("BLENDER_MODELS defs=" + defs + " fbx=" + fbx + " imported=" + imported + " match=" + matched + " resources=" + inResources + " prefabs=" + prefabs + " enabled=" + BlenderModels.Enabled);
            sb.AppendLine("NOT_MATCHING " + failed.Count + ": " + string.Join(" ", failed));
            sb.AppendLine(string.Format("{0,-20} {1,-9} {2,6} {3,6}  {4,-6} {5,-9} {6,-6} {7}", "model", "fbx", "tris", "verts", "xform", "resources", "prefab", "match"));
            sb.Append(rows);
            File.WriteAllText(Path.Combine(OutDir, "models.txt"), sb.ToString());
            Debug.Log("[Models] " + sb.ToString().Split('\n')[0]);
        }

        /// <summary>Null when the runtime cut of the mesh reproduces ModelMesher's bone meshes (positions, normals, uvs, winding); otherwise what differs.</summary>
        static string Check(ModelDef def, Mesh mesh)
        {
            var cut = BlenderModels.Split(def, mesh);
            if (cut == null) return "mesh does not fit the ModelDef (bones, cubes or bone ids)";
            try
            {
                var proc = ModelMesher.BoneMeshes(def);
                for (int i = 0; i < def.bones.Count; i++)
                {
                    var a = cut[i];
                    var b = proc[i];
                    string bone = "bone " + def.bones[i].name + ": ";
                    if ((a == null) != (b == null)) return bone + "layout differs";
                    if (a == null) continue;
                    string err = Same(a.main, b.main);
                    if (err != null) return bone + err;
                    if (a.optional.Count != b.optional.Count) return bone + "optional parts differ";
                    for (int k = 0; k < a.optional.Count; k++)
                    {
                        if (a.optional[k].Key != b.optional[k].Key) return bone + "optional part names differ";
                        err = Same(a.optional[k].Value, b.optional[k].Value);
                        if (err != null) return bone + a.optional[k].Key + " " + err;
                    }
                }
                return null;
            }
            finally
            {
                foreach (var bm in cut)
                {
                    if (bm == null) continue;
                    if (bm.main != null) UnityEngine.Object.DestroyImmediate(bm.main);
                    foreach (var kv in bm.optional) UnityEngine.Object.DestroyImmediate(kv.Value);
                }
            }
        }

        /// <summary>Compares an FBX-derived mesh (a) with its procedural twin (b).</summary>
        static string Same(Mesh a, Mesh b)
        {
            if (a == null || b == null) return a == b ? null : "mesh missing";
            if (a.vertexCount != b.vertexCount) return "vertices " + a.vertexCount + " vs " + b.vertexCount;
            if (a.subMeshCount != b.subMeshCount) return "submeshes " + a.subMeshCount + " vs " + b.subMeshCount;
            for (int s = 0; s < a.subMeshCount; s++)
                if (a.GetIndexCount(s) != b.GetIndexCount(s)) return "triangles " + a.GetIndexCount(s) / 3 + " vs " + b.GetIndexCount(s) / 3;
            var want = new Dictionary<(int, int, int, int, int, int, int, int), int>();
            foreach (var k in Keys(b)) want[k] = want.TryGetValue(k, out int c) ? c + 1 : 1;
            int missing = 0, first = -1, index = 0;
            foreach (var k in Keys(a))
            {
                if (want.TryGetValue(k, out int c) && c > 0) want[k] = c - 1;
                else { missing++; if (first < 0) first = index; }
                index++;
            }
            if (missing > 0) return missing + "/" + a.vertexCount + " vertices differ, e.g. " + Describe(a, b, first);
            var v = a.vertices;
            var n = a.normals;
            for (int s = 0; s < a.subMeshCount; s++)
            {
                var t = a.GetTriangles(s);
                for (int i = 0; i + 2 < t.Length; i += 3)
                {
                    var face = Vector3.Cross(v[t[i + 1]] - v[t[i]], v[t[i + 2]] - v[t[i]]);
                    if (face.sqrMagnitude > 1e-12f && Vector3.Dot(face, n[t[i]]) <= 0f) return "inside-out triangles";
                }
            }
            return null;
        }

        static IEnumerable<(int, int, int, int, int, int, int, int)> Keys(Mesh m)
        {
            var v = m.vertices;
            var n = m.normals;
            var uv = m.uv;
            for (int i = 0; i < v.Length; i++)
                yield return (Q(v[i].x, 1024f), Q(v[i].y, 1024f), Q(v[i].z, 1024f), Q(n[i].x, 16f), Q(n[i].y, 16f), Q(n[i].z, 16f), Q(uv[i].x, 8192f), Q(uv[i].y, 8192f));
        }

        static int Q(float f, float scale) => Mathf.RoundToInt(f * scale);

        /// <summary>Vertex i of the FBX-derived mesh next to the procedural vertices at the same position (what differs).</summary>
        static string Describe(Mesh a, Mesh b, int i)
        {
            var p = a.vertices[i];
            var sb = new StringBuilder("fbx p" + p.ToString("F3") + " n" + a.normals[i].ToString("F2") + " uv" + a.uv[i].ToString("F4") + "; procedural there:");
            var bv = b.vertices; var bn = b.normals; var bu = b.uv;
            int found = 0;
            for (int k = 0; k < bv.Length && found < 3; k++)
                if ((bv[k] - p).sqrMagnitude < 1e-6f) { sb.Append(" n" + bn[k].ToString("F2") + " uv" + bu[k].ToString("F4")); found++; }
            if (found == 0) sb.Append(" none (mirrored or moved)");
            return sb.ToString();
        }

        /// <summary>"ok" when every node of the imported model has an identity transform (the axis conversion is baked into the mesh).</summary>
        static string Transforms(GameObject model)
        {
            foreach (var t in model.GetComponentsInChildren<Transform>(true))
                if (t.localPosition.sqrMagnitude > 1e-10f || Quaternion.Angle(t.localRotation, Quaternion.identity) > 0.01f || (t.localScale - Vector3.one).sqrMagnitude > 1e-8f)
                    return "moved";
            return "ok";
        }

        // ------------------------------------------------------------------ helpers
        internal static Mesh LoadMesh(string path) => AssetDatabase.LoadAllAssetsAtPath(path).OfType<Mesh>().FirstOrDefault();

        internal static int Triangles(Mesh mesh)
        {
            long n = 0;
            for (int s = 0; s < mesh.subMeshCount; s++) n += mesh.GetIndexCount(s) / 3;
            return (int)n;
        }

        /// <summary>Create the asset, or overwrite an existing one in place so its GUID (and every reference) survives.</summary>
        internal static void SaveAsset<T>(T obj, string path) where T : UnityEngine.Object
        {
            var existing = AssetDatabase.LoadAssetAtPath<T>(path);
            if (existing == null)
            {
                AssetDatabase.CreateAsset(obj, path);
                return;
            }
            EditorUtility.CopySerialized(obj, existing);
            EditorUtility.SetDirty(existing);
            UnityEngine.Object.DestroyImmediate(obj);
        }

        internal static void EnsureFolder(string path)
        {
            if (AssetDatabase.IsValidFolder(path)) return;
            string parent = Path.GetDirectoryName(path).Replace('\\', '/');
            EnsureFolder(parent);
            AssetDatabase.CreateFolder(parent, Path.GetFileName(path));
        }
    }

    /// <summary>
    /// Skin materials imported from the Blender FBX files are alpha-clipped like the game's entity shader, so skin
    /// overlays and see-through parts (hats, jackets, wing membranes) read correctly in the model prefabs.
    /// </summary>
    sealed class BlenderModelMaterialPostprocessor : AssetPostprocessor
    {
        public override uint GetVersion() => 1;

        void OnPostprocessMaterial(Material material)
        {
            if (!assetPath.StartsWith(ModelImporter.ModelsDir + "/", StringComparison.OrdinalIgnoreCase) || !material.HasProperty("_Cutoff")) return;
            material.SetFloat("_AlphaClip", 1f);
            material.SetFloat("_Cutoff", 0.1f);
            material.EnableKeyword("_ALPHATEST_ON");
            material.SetOverrideTag("RenderType", "TransparentCutout");
            material.renderQueue = (int)UnityEngine.Rendering.RenderQueue.AlphaTest;
        }
    }
}
