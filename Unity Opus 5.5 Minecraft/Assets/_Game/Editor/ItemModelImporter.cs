using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEngine;

namespace MCR.EditorTools
{
    /// <summary>
    /// Brings the Blender-built voxel items and props into the game. Tools/BlenderBridge/item_pipeline.py writes
    /// Models/Items/&lt;id&gt;.fbx (an item sprite extruded pixel by pixel, the twin of ItemRender.ExtrudedMesh) and
    /// Models/Props/&lt;id&gt;.fbx (torches, lanterns and campfires built from block textures), their textures in the
    /// Textures folder beside them. This applies ModelImporter's importer settings, checks every item against the
    /// procedural extrusion of its current sprite and saves Resources/Models/Items/&lt;id&gt;_mesh.asset and
    /// Resources/Models/Props/&lt;id&gt;_mesh.asset (+ &lt;id&gt;_tex.txt, the block texture of every submesh, read from the FBX
    /// material names), which MCR.BlenderItems turns into texture-array meshes at runtime. An item whose FBX no longer
    /// matches its sprite is reported and left out, so the game keeps the procedural extrusion for it.
    /// Report: Tools/_out/item_models.txt.
    /// </summary>
    public static class ItemModelImporter
    {
        public const string ItemsDir = ModelImporter.ModelsDir + "/Items";
        public const string PropsDir = ModelImporter.ModelsDir + "/Props";
        public const string ItemsResources = ModelImporter.ResourcesDir + "/Items";
        public const string PropsResources = ModelImporter.ResourcesDir + "/Props";
        /// <summary>The prop materials are named after the block texture they carry (Tools/BlenderBridge/scripts/build_items.py).</summary>
        const string PropMaterialPrefix = "mcrtex_";
        static string ProjectRoot => Path.GetFullPath(Path.Combine(Application.dataPath, ".."));
        static string OutDir { get { var d = Path.Combine(ProjectRoot, "Tools/_out"); Directory.CreateDirectory(d); return d; } }

        /// <summary>
        /// Importer settings, Resources copies and the Tools/_out/item_models.txt report for every item and prop FBX.
        /// Never throws, so the Windows build carries on (with the procedural meshes) if something here goes wrong.
        /// </summary>
        [MenuItem("Tools/Opus 5.5 Minecraft/Import Blender Items")]
        public static void ImportItemModels()
        {
            var t0 = DateTime.Now;
            var sb = new StringBuilder();
            var probes = new List<Mesh>();
            try
            {
                BatchTools.InitRegistries();
                BatchTools.EnsureTextureRegistry();
                var items = FbxNames(ItemsDir);
                var props = FbxNames(PropsDir);
                SyncTextures("Tools/BlenderBridge/export/items", ItemsDir + "/Textures");
                SyncTextures("Tools/BlenderBridge/export/props", PropsDir + "/Textures");
                AssetDatabase.Refresh();
                AssetDatabase.StartAssetEditing();
                try
                {
                    foreach (var dir in new[] { ItemsDir + "/Textures", PropsDir + "/Textures" })
                        if (Directory.Exists(dir))
                            foreach (var png in Directory.GetFiles(dir, "*.png")) ModelImporter.ConfigureTexture(dir + "/" + Path.GetFileName(png));
                    foreach (var n in items) ModelImporter.ConfigureModel(ItemsDir + "/" + n + ".fbx");
                    foreach (var n in props) ModelImporter.ConfigureModel(PropsDir + "/" + n + ".fbx");
                }
                finally { AssetDatabase.StopAssetEditing(); }
                ModelImporter.EnsureFolder(ItemsResources);
                ModelImporter.EnsureFolder(PropsResources);

                var itemRows = new StringBuilder();
                int matched = 0;
                foreach (var id in items) if (ImportItem(id, itemRows, probes)) matched++;
                var propRows = new StringBuilder();
                int propsOk = 0;
                foreach (var id in props) if (ImportProp(id, propRows)) propsOk++;
                RemoveStale(ItemsResources, items);
                RemoveStale(PropsResources, props);
                AssetDatabase.SaveAssets();

                sb.AppendLine("BLENDER_ITEMS fbx=" + items.Count + " match=" + matched + " resources=" + matched + "  PROPS fbx=" + props.Count + " resources=" + propsOk + "  enabled=" + BlenderItems.Enabled);
                sb.AppendLine(string.Format("{0,-22} {1,6} {2,6}  {3,9} {4,10}  {5}", "item", "tris", "verts", "proc_tris", "proc_verts", "match (FBX vs ItemRender.ExtrudedMesh)"));
                sb.Append(itemRows);
                sb.AppendLine();
                sb.AppendLine(string.Format("{0,-16} {1,6} {2,6}  {3,-40} {4,-34} {5}", "prop", "tris", "verts", "textures (per submesh)", "bounds (blocks)", "status"));
                sb.Append(propRows);
                Debug.Log("[Items] Imported " + matched + "/" + items.Count + " Blender items and " + propsOk + "/" + props.Count + " props in " + (DateTime.Now - t0).TotalSeconds.ToString("0.0") + "s");
            }
            catch (Exception e)
            {
                Debug.LogError("[Items] Blender item import failed: " + e);
                sb.AppendLine("EXCEPTION " + e);
            }
            finally
            {
                // the procedural probes are editor-only meshes; drop them from ItemRender's cache before destroying them
                ItemRender.ClearCaches();
                foreach (var m in probes) if (m != null) UnityEngine.Object.DestroyImmediate(m);
            }
            File.WriteAllText(Path.Combine(OutDir, "item_models.txt"), sb.ToString());
        }

        static List<string> FbxNames(string dir)
        {
            if (!Directory.Exists(dir)) return new List<string>();
            return Directory.GetFiles(dir, "*.fbx").Select(Path.GetFileNameWithoutExtension).OrderBy(n => n, StringComparer.Ordinal).ToList();
        }

        /// <summary>The sprites are painted by code; keep the FBX textures identical to the latest editor export (BatchTools.ExportItemSprites).</summary>
        static void SyncTextures(string exportDir, string texturesDir)
        {
            string src = Path.Combine(ProjectRoot, exportDir);
            if (!Directory.Exists(src) || !Directory.Exists(texturesDir)) return;
            foreach (var to in Directory.GetFiles(texturesDir, "*.png"))
            {
                string from = Path.Combine(src, Path.GetFileName(to));
                if (!File.Exists(from)) continue;
                var bytes = File.ReadAllBytes(from);
                if (File.ReadAllBytes(to).SequenceEqual(bytes)) continue;
                File.WriteAllBytes(to, bytes);
            }
        }

        // ------------------------------------------------------------------ items
        static bool ImportItem(string id, StringBuilder rows, List<Mesh> probes)
        {
            string path = ItemsDir + "/" + id + ".fbx", dst = ItemsResources + "/" + id + "_mesh.asset";
            var src = ModelImporter.LoadMesh(path);
            var it = Items.Get(id);
            int layer = it != null ? ItemRender.SpriteLayer(it) : -1;
            var proc = layer >= 0 ? ItemRender.ExtrudedMesh(layer) : null;
            if (proc != null && !probes.Contains(proc)) probes.Add(proc);
            string problem = src == null ? "FBX mesh missing"
                : it == null ? "no item named " + id
                : layer < 0 ? "the item renders as a block"
                : Compare(src, layer, proc);
            if (problem == null)
            {
                var copy = UnityEngine.Object.Instantiate(src);
                copy.name = id + "_mesh";
                ModelImporter.SaveAsset(copy, dst);
            }
            else
            {
                Debug.LogWarning("[Items] " + id + ": not copied to Resources, " + problem);
                AssetDatabase.DeleteAsset(dst);
            }
            rows.AppendLine(string.Format("{0,-22} {1,6} {2,6}  {3,9} {4,10}  {5}", id,
                src != null ? ModelImporter.Triangles(src).ToString() : "-", src != null ? src.vertexCount.ToString() : "-",
                proc != null ? ModelImporter.Triangles(proc).ToString() : "-", proc != null ? proc.vertexCount.ToString() : "-", problem ?? "match"));
            return problem == null;
        }

        /// <summary>
        /// Null when the runtime copy of the FBX mesh (BlenderItems.ToChunkMesh) carries exactly the vertices of the
        /// procedural extrusion (position, uv, layer, frames, shade, light; any order) and as many triangles, and every
        /// FBX triangle winds towards its normal; otherwise what differs.
        /// </summary>
        static string Compare(Mesh src, int layer, Mesh proc)
        {
            var conv = BlenderItems.ToChunkMesh(src, new[] { layer }, false, "probe");
            if (conv == null) return "mesh not readable (positions, normals and uvs needed)";
            try
            {
                var a = Vertices(conv);
                var b = Vertices(proc);
                int ta = ModelImporter.Triangles(conv), tb = ModelImporter.Triangles(proc);
                if (a.Length != b.Length || ta != tb) return "vertices " + a.Length + " vs " + b.Length + ", triangles " + ta + " vs " + tb;
                var want = new Dictionary<string, int>();
                foreach (var v in b) { string k = Key(v); want[k] = want.TryGetValue(k, out int c) ? c + 1 : 1; }
                int missing = 0;
                string first = null;
                foreach (var v in a)
                {
                    string k = Key(v);
                    if (want.TryGetValue(k, out int c) && c > 0) want[k] = c - 1;
                    else { missing++; if (first == null) first = k; }
                }
                if (missing > 0) return missing + "/" + a.Length + " vertices differ, e.g. (pos*1024, rgba, uv*256, layer, frames, light) " + first;
                return InsideOut(src);
            }
            finally { UnityEngine.Object.DestroyImmediate(conv); }
        }

        static ChunkVertex[] Vertices(Mesh m)
        {
            using (var data = Mesh.AcquireReadOnlyMeshData(m))
                return data[0].GetVertexData<ChunkVertex>().ToArray();
        }

        /// <summary>
        /// Positions to 1/1024 block (pixels and the 1/32 half thickness are exact there) and uvs to 1/256: the texel
        /// insets sit 0.1 texel from the rounding boundaries, far more than the half-float error, while a wrong texel
        /// is 16 steps off.
        /// </summary>
        static string Key(ChunkVertex v) => Mathf.RoundToInt(v.x * 1024f) + "," + Mathf.RoundToInt(v.y * 1024f) + "," + Mathf.RoundToInt(v.z * 1024f)
            + " " + v.color.ToString("X8") + " " + Mathf.RoundToInt(Mathf.HalfToFloat(v.u) * 256f) + "," + Mathf.RoundToInt(Mathf.HalfToFloat(v.v) * 256f)
            + " " + Mathf.RoundToInt(Mathf.HalfToFloat(v.layer)) + " " + Mathf.RoundToInt(Mathf.HalfToFloat(v.anim)) + " " + v.light.ToString("X");

        /// <summary>Null when every triangle winds clockwise seen from its normal's side (Unity front faces); else a count.</summary>
        static string InsideOut(Mesh m)
        {
            var v = m.vertices;
            var n = m.normals;
            int bad = 0, total = 0;
            for (int s = 0; s < m.subMeshCount; s++)
            {
                var t = m.GetTriangles(s);
                for (int i = 0; i + 2 < t.Length; i += 3)
                {
                    total++;
                    var face = Vector3.Cross(v[t[i + 1]] - v[t[i]], v[t[i + 2]] - v[t[i]]);
                    if (face.sqrMagnitude > 1e-12f && Vector3.Dot(face, n[t[i]]) <= 0f) bad++;
                }
            }
            return bad == 0 ? null : bad + "/" + total + " triangles inside out";
        }

        // ------------------------------------------------------------------ props
        static bool ImportProp(string id, StringBuilder rows)
        {
            string path = PropsDir + "/" + id + ".fbx", dst = PropsResources + "/" + id + "_mesh.asset", texAsset = PropsResources + "/" + id + "_tex.txt";
            var src = ModelImporter.LoadMesh(path);
            var model = AssetDatabase.LoadAssetAtPath<GameObject>(path);
            var rend = model != null ? model.GetComponentInChildren<Renderer>() : null;
            string[] textures = rend != null ? rend.sharedMaterials.Select(TextureOf).ToArray() : null;
            string unknown = textures != null ? textures.FirstOrDefault(t => !Tex.Has(t)) : null;
            string problem = src == null ? "FBX mesh missing"
                : textures == null ? "no renderer in the FBX model"
                : textures.Length != src.subMeshCount ? textures.Length + " materials for " + src.subMeshCount + " submeshes"
                : unknown != null ? "unknown block texture '" + unknown + "'"
                : null;
            string bounds = "-";
            if (problem == null)
            {
                var conv = BlenderItems.ToChunkMesh(src, textures.Select(Tex.Id).ToArray(), true, "probe");
                if (conv == null) problem = "mesh not readable (positions, normals and uvs needed)";
                else UnityEngine.Object.DestroyImmediate(conv);
                var b = src.bounds;
                bounds = b.min.ToString("F3") + ".." + b.max.ToString("F3");
                if (problem == null) problem = InsideOut(src);
            }
            if (problem == null)
            {
                var copy = UnityEngine.Object.Instantiate(src);
                copy.name = id + "_mesh";
                ModelImporter.SaveAsset(copy, dst);
                string full = Path.Combine(ProjectRoot, texAsset), text = string.Join("\n", textures) + "\n";
                if (!File.Exists(full) || File.ReadAllText(full) != text)
                {
                    File.WriteAllText(full, text);
                    AssetDatabase.ImportAsset(texAsset);
                }
            }
            else
            {
                Debug.LogWarning("[Items] prop " + id + ": not copied to Resources, " + problem);
                AssetDatabase.DeleteAsset(dst);
                AssetDatabase.DeleteAsset(texAsset);
            }
            rows.AppendLine(string.Format("{0,-16} {1,6} {2,6}  {3,-40} {4,-34} {5}", id,
                src != null ? ModelImporter.Triangles(src).ToString() : "-", src != null ? src.vertexCount.ToString() : "-",
                textures != null ? string.Join(",", textures) : "-", bounds, problem ?? "ok"));
            return problem == null;
        }

        static string TextureOf(Material m)
        {
            if (m == null) return "";
            return m.name.StartsWith(PropMaterialPrefix, StringComparison.Ordinal) ? m.name.Substring(PropMaterialPrefix.Length) : m.name;
        }

        /// <summary>Resources copies (mesh and texture list) whose FBX is gone.</summary>
        static void RemoveStale(string dir, List<string> names)
        {
            if (!Directory.Exists(dir)) return;
            foreach (var f in Directory.GetFiles(dir, "*_mesh.asset"))
            {
                string file = Path.GetFileName(f), n = file.Substring(0, file.Length - "_mesh.asset".Length);
                if (names.Contains(n)) continue;
                AssetDatabase.DeleteAsset(dir + "/" + file);
                AssetDatabase.DeleteAsset(dir + "/" + n + "_tex.txt");
            }
        }
    }
}
