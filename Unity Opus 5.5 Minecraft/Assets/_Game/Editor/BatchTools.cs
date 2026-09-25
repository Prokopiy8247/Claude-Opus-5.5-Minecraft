using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace MCR.EditorTools
{
    /// <summary>
    /// Command-line and menu entry points: validation, generated-asset baking (the regenerable pipeline),
    /// scene setup and desktop builds. Everything the project needs can be rebuilt from code with these.
    /// </summary>
    public static class BatchTools
    {
        public const string ScenePath = "Assets/_Game/Scenes/Main.unity";
        public const string GeneratedResources = "Assets/_Game/Generated/Resources/Generated";
        public const string ShaderRefs = "Assets/_Game/Generated/Resources/ShaderRefs";
        public const string MacBuildProfilePath = "Assets/Settings/Build Profiles/macOS Universal.asset";
        static string ProjectRoot => Path.GetFullPath(Path.Combine(Application.dataPath, ".."));
        static string OutDir { get { var d = Path.Combine(ProjectRoot, "Tools/_out"); Directory.CreateDirectory(d); return d; } }

        internal static void InitRegistries()
        {
            Biome.Init();
            Blocks.Init();
            Items.Init();
            Tags.Init();
            Recipes.Init();
        }

        /// <summary>Registers every texture layer (blocks, item sprites, particles) and freezes the registry, as the baked texture array does.</summary>
        internal static void EnsureTextureRegistry()
        {
            if (Tex.Frozen) return;
            ItemSprites.RegisterTextures();
            ParticleTextures.Register();
            ChunkMesher.PrewarmTextures();
            Tex.Freeze();
        }

        // ------------------------------------------------------------------ validation
        /// <summary>Registry/content audit: missing textures, fallback sprites, model and recipe counts.</summary>
        public static void Validate()
        {
            var sb = new StringBuilder();
            try
            {
                InitRegistries();
                ItemSprites.RegisterTextures();
                ParticleTextures.Register();
                var names = Tex.Names.ToList();
                foreach (var n in names) Res.LayerPixels(n);
                sb.AppendLine("blocks=" + Blocks.All.Count + " items=" + Items.All.Count + " layers=" + names.Count);
                sb.AppendLine("recipes crafting=" + Recipes.Crafting.Count + " smelting=" + Recipes.Smelting.Count + " stonecutting=" + Recipes.Stonecutting.Count + " smithing=" + Recipes.Smithing.Count);
                sb.AppendLine("models=" + MobModels.All.Count() + " mobs=" + MobRegistry.All.Count);
                sb.AppendLine("MISSING_TEXTURES " + TextureGen.MissingNames.Count + ": " + string.Join(" ", TextureGen.MissingNames.OrderBy(x => x)));
                sb.AppendLine("FALLBACK_SPRITES " + ItemSprites.Fallbacks.Count + ": " + string.Join(" ", ItemSprites.Fallbacks.OrderBy(x => x)));
                var noModel = MobRegistry.All.Where(d => MobModels.Get(d.model) == null).Select(d => d.id).ToList();
                sb.AppendLine("MOBS_WITHOUT_MODEL " + noModel.Count + ": " + string.Join(" ", noModel));
                var noTab = Items.All.Where(i => i.tab == CreativeTab.None && !i.hiddenInCreative).Select(i => i.id).ToList();
                sb.AppendLine("ITEMS_WITHOUT_TAB " + noTab.Count + ": " + string.Join(" ", noTab.Take(200)));
                var tabs = Items.All.Where(i => !i.hiddenInCreative).GroupBy(i => i.tab).Select(g => g.Key + "=" + g.Count());
                sb.AppendLine("CREATIVE_TABS " + string.Join(" ", tabs));
                var eggs = Items.All.Count(i => i is SpawnEggItem);
                sb.AppendLine("SPAWN_EGGS " + eggs);
                StructureManager.Init();
                sb.AppendLine("STRUCTURES " + StructureManager.Types.Count + ": " + string.Join(" ", StructureManager.Types.Select(t => t.id + "(" + t.dim + ")")));
                Debug.Log("[Validate]\n" + sb);
            }
            catch (Exception e)
            {
                sb.AppendLine("EXCEPTION " + e);
                Debug.LogError(e);
            }
            File.WriteAllText(Path.Combine(OutDir, "validate.txt"), sb.ToString());
        }

        // ------------------------------------------------------------------ generated assets
        [MenuItem("Tools/Opus 5.5 Minecraft/Rebuild Generated Assets")]
        public static void RebuildGeneratedAssets()
        {
            var t0 = DateTime.Now;
            InitRegistries();
            Directory.CreateDirectory(GeneratedResources);
            ConfigureRenderPipeline();
            EnsureShaderRefs();
            BakeBlockTextureArray();
            ExportModelData();
            ExportItemSprites();
            ModelImporter.ImportBlenderModels();
            ItemModelImporter.ImportItemModels();
            ConfigurePanoramaImport();
            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh();
            Debug.Log("[Generated] Rebuilt generated assets in " + (DateTime.Now - t0).TotalSeconds.ToString("0.0") + "s");
        }

        /// <summary>
        /// The game draws its own sky, fog and lighting, so the template's screen-space effects are removed: SSAO
        /// cannot even initialise in a player build once its resources are stripped. HDR, MSAA and shadows are off.
        /// </summary>
        [MenuItem("Tools/Opus 5.5 Minecraft/Configure Render Pipeline")]
        public static void ConfigureRenderPipeline()
        {
            foreach (var guid in AssetDatabase.FindAssets("t:UniversalRendererData"))
            {
                var path = AssetDatabase.GUIDToAssetPath(guid);
                var data = AssetDatabase.LoadAssetAtPath<UnityEngine.Rendering.Universal.UniversalRendererData>(path);
                if (data == null) continue;
                bool changed = false;
                for (int i = data.rendererFeatures.Count - 1; i >= 0; i--)
                {
                    var f = data.rendererFeatures[i];
                    if (f == null || f.GetType().Name.Contains("AmbientOcclusion") || f.GetType().Name.Contains("Decal"))
                    {
                        if (f != null) UnityEngine.Object.DestroyImmediate(f, true);
                        data.rendererFeatures.RemoveAt(i);
                        changed = true;
                    }
                }
                if (changed)
                {
                    var so = new SerializedObject(data);
                    var maps = so.FindProperty("m_RendererFeatureMap");
                    if (maps != null) maps.ClearArray();
                    so.ApplyModifiedPropertiesWithoutUndo();
                    EditorUtility.SetDirty(data);
                    Debug.Log("[Pipeline] Removed unused renderer features from " + path);
                }
            }
            foreach (var guid in AssetDatabase.FindAssets("t:UniversalRenderPipelineAsset"))
            {
                var path = AssetDatabase.GUIDToAssetPath(guid);
                var asset = AssetDatabase.LoadAssetAtPath<UnityEngine.Rendering.Universal.UniversalRenderPipelineAsset>(path);
                if (asset == null) continue;
                asset.supportsHDR = false;
                asset.msaaSampleCount = 1;
                asset.shadowDistance = 0f;
                asset.supportsCameraDepthTexture = false;
                asset.supportsCameraOpaqueTexture = false;
                EditorUtility.SetDirty(asset);
            }
            AssetDatabase.SaveAssets();
        }

        /// <summary>Materials in a Resources folder keep the runtime-looked-up shaders in player builds.</summary>
        static void EnsureShaderRefs()
        {
            Directory.CreateDirectory(ShaderRefs);
            foreach (var name in new[] { "MCR/Chunk", "MCR/Entity", "MCR/Unlit", "MCR/UI", "MCR/UIComposite" })
            {
                var sh = Shader.Find(name);
                if (sh == null) { Debug.LogError("Shader missing: " + name); continue; }
                string path = ShaderRefs + "/" + name.Replace("/", "_") + ".mat";
                var mat = AssetDatabase.LoadAssetAtPath<Material>(path);
                if (mat == null) AssetDatabase.CreateAsset(new Material(sh), path);
                else mat.shader = sh;
            }
        }

        /// <summary>Bakes the procedural block/item/particle textures into a Texture2DArray asset for fast startup.</summary>
        /// <summary>
        /// The title panorama faces (captured from a generated world by the self-test runner's "panorama" step)
        /// import uncompressed, without mipmaps and clamped, so the seams between faces stay invisible.
        /// </summary>
        static void ConfigurePanoramaImport()
        {
            string dir = GeneratedResources.Replace("/Generated", "") + "/Panorama";
            if (!Directory.Exists(Path.Combine(ProjectRoot, dir))) dir = "Assets/_Game/Generated/Resources/Panorama";
            for (int i = 0; i < 6; i++)
            {
                string path = dir + "/panorama_" + i + ".png";
                var imp = AssetImporter.GetAtPath(path) as TextureImporter;
                if (imp == null) continue;
                bool changed = imp.mipmapEnabled || imp.textureCompression != TextureImporterCompression.Uncompressed || imp.wrapMode != TextureWrapMode.Clamp || imp.npotScale != TextureImporterNPOTScale.None;
                if (!changed) continue;
                imp.mipmapEnabled = false;
                imp.textureCompression = TextureImporterCompression.Uncompressed;
                imp.wrapMode = TextureWrapMode.Clamp;
                imp.npotScale = TextureImporterNPOTScale.None;
                imp.filterMode = FilterMode.Bilinear;
                imp.SaveAndReimport();
            }
        }

        static void BakeBlockTextureArray()
        {
            ItemSprites.RegisterTextures();
            ParticleTextures.Register();
            ChunkMesher.PrewarmTextures();
            Tex.Freeze();
            var arr = Res.GenerateArray();
            string path = GeneratedResources + "/BlockTextures.asset";
            var existing = AssetDatabase.LoadAssetAtPath<Texture2DArray>(path);
            if (existing != null) AssetDatabase.DeleteAsset(path);
            AssetDatabase.CreateAsset(arr, path);
            File.WriteAllText(GeneratedResources + "/BlockTextures_info.txt", Res.NameHash().ToString());
            Debug.Log("[Generated] Block texture array: " + arr.depth + " layers");
        }

        /// <summary>
        /// Writes every model definition as JSON plus its painted skin as PNG. The Blender builder reads these to
        /// author the FBX models, which the importer below then turns into prefabs the runtime prefers.
        /// </summary>
        public static void ExportModelData()
        {
            var jsonDir = Path.Combine(ProjectRoot, "Assets/_Game/Data/Models");
            var skinDir = Path.Combine(ProjectRoot, "Tools/BlenderBridge/export/skins");
            Directory.CreateDirectory(jsonDir);
            Directory.CreateDirectory(skinDir);
            int n = 0;
            var index = new StringBuilder("[");
            foreach (var def in MobModels.All)
            {
                File.WriteAllText(Path.Combine(jsonDir, def.name + ".json"), def.ToJson());
                var skin = ModelSkins.Get(def.skin ?? def.name, def.texW, def.texH, null);
                var png = skin.opaque.EncodeToPNG();
                File.WriteAllBytes(Path.Combine(skinDir, def.name + ".png"), png);
                if (n > 0) index.Append(',');
                index.Append("\"").Append(def.name).Append("\"");
                n++;
            }
            index.Append("]");
            File.WriteAllText(Path.Combine(jsonDir, "_index.json"), index.ToString());
            Debug.Log("[Generated] Exported " + n + " model definitions and skins");
        }

        // ------------------------------------------------------------------ Blender items / props
        static readonly string[] BlenderItemTiers = { "wooden", "stone", "copper", "iron", "golden", "diamond", "netherite" };
        static readonly string[] BlenderItemTools = { "sword", "pickaxe", "axe", "shovel", "hoe" };
        static readonly string[] BlenderItemExtras = { "bow", "crossbow", "fishing_rod", "stick", "shears", "flint_and_steel", "mace", "carrot_on_a_stick", "brush", "spyglass" };
        /// <summary>Block textures the Blender props (torches, lanterns, campfires) are painted with.</summary>
        static readonly string[] BlenderPropTextures = { "torch", "soul_torch", "lantern", "soul_lantern", "campfire_log", "campfire_log_lit", "soul_campfire_log_lit", "fire", "soul_fire" };

        /// <summary>
        /// Writes the 16x16 sprite of every item that gets a Blender voxel model (tools of every tier, bows, rods and small
        /// tools) to Tools/BlenderBridge/export/items/&lt;id&gt;.png, and the block textures of the Blender props to
        /// export/props/&lt;texture&gt;.png, each folder with an _index.json of what it holds. The pixels are the ones the game
        /// draws (Res.GetLayerPixels of the item's sprite layer), so Tools/BlenderBridge/item_pipeline.py extrudes exactly
        /// the sprite ItemRender.ExtrudedMesh would. Ids that are not registered are skipped.
        /// </summary>
        [MenuItem("Tools/Opus 5.5 Minecraft/Export Item Sprites")]
        public static void ExportItemSprites()
        {
            InitRegistries();
            EnsureTextureRegistry();
            string itemDir = Path.Combine(ProjectRoot, "Tools/BlenderBridge/export/items");
            string propDir = Path.Combine(ProjectRoot, "Tools/BlenderBridge/export/props");
            Directory.CreateDirectory(itemDir);
            Directory.CreateDirectory(propDir);
            var ids = new List<string>();
            foreach (var tier in BlenderItemTiers)
                foreach (var tool in BlenderItemTools) ids.Add(tier + "_" + tool);
            ids.AddRange(BlenderItemExtras);
            var items = new List<string>();
            foreach (var id in ids)
            {
                var it = Items.Get(id);
                int layer = it != null ? ItemRender.SpriteLayer(it) : -1;
                if (layer < 0) continue;
                WriteLayerPng(Res.GetLayerPixels(layer), Path.Combine(itemDir, id + ".png"));
                items.Add(id);
            }
            var textures = new List<string>();
            foreach (var name in BlenderPropTextures)
            {
                if (!Tex.Has(name)) continue;
                WriteLayerPng(Res.GetLayerPixels(Tex.Id(name)), Path.Combine(propDir, name + ".png"));
                textures.Add(name);
            }
            File.WriteAllText(Path.Combine(itemDir, "_index.json"), "[" + string.Join(",", items.Select(s => "\"" + s + "\"")) + "]");
            File.WriteAllText(Path.Combine(propDir, "_index.json"), "[" + string.Join(",", textures.Select(s => "\"" + s + "\"")) + "]");
            Debug.Log("[Generated] Exported " + items.Count + " item sprites and " + textures.Count + " prop textures for Blender");
        }

        /// <summary>One 16x16 layer as PNG. The layer comes in top-down rows while Unity textures start at the bottom row, so flip.</summary>
        static void WriteLayerPng(Color32[] px, string path)
        {
            var rows = new Color32[256];
            if (px != null && px.Length >= 256)
                for (int y = 0; y < 16; y++) Array.Copy(px, y * 16, rows, (15 - y) * 16, 16);
            var tex = new Texture2D(16, 16, TextureFormat.RGBA32, false);
            tex.SetPixels32(rows);
            tex.Apply(false, false);
            File.WriteAllBytes(path, tex.EncodeToPNG());
            UnityEngine.Object.DestroyImmediate(tex);
        }

        // ------------------------------------------------------------------ scene
        [MenuItem("Tools/Opus 5.5 Minecraft/Setup Main Scene")]
        public static void SetupScene()
        {
            Directory.CreateDirectory(Path.GetDirectoryName(ScenePath));
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            var go = new GameObject("MCR Game");
            go.AddComponent<Bootstrap>();
            var camGo = new GameObject("Main Camera") { tag = "MainCamera" };
            var cam = camGo.AddComponent<Camera>();
            cam.clearFlags = CameraClearFlags.SolidColor;
            cam.backgroundColor = new Color(0.18f, 0.22f, 0.35f);
            camGo.AddComponent<AudioListener>();
            EditorSceneManager.SaveScene(scene, ScenePath);
            EditorBuildSettings.scenes = new[] { new EditorBuildSettingsScene(ScenePath, true) };
            Debug.Log("[Scene] Main scene written to " + ScenePath);
        }

        // ------------------------------------------------------------------ build
        [MenuItem("Tools/Opus 5.5 Minecraft/Build Windows")]
        public static void BuildWindows()
        {
            if (!EditorUserBuildSettings.SwitchActiveBuildTarget(
                BuildTargetGroup.Standalone, BuildTarget.StandaloneWindows64))
                throw new InvalidOperationException("Could not switch Unity to the Windows build target.");
            BuildDesktop(
                BuildTarget.StandaloneWindows64,
                Path.Combine("Builds", "Windows", PlayerExe),
                "Windows");
        }

        [MenuItem("Tools/Opus 5.5 Minecraft/Build macOS")]
        public static void BuildMacOS()
        {
            if (!EditorUserBuildSettings.SwitchActiveBuildTarget(
                BuildTargetGroup.Standalone, BuildTarget.StandaloneOSX))
                throw new InvalidOperationException("Could not switch Unity to the macOS build target.");
            // Use the enum instead of a numeric constant: Unity has changed the serialized values between releases.
            PlayerSettings.SetArchitecture(
                UnityEditor.Build.NamedBuildTarget.Standalone,
                (int)UnityEditor.Build.OSArchitecture.x64ARM64);
            Debug.Log("[Build:macOS] Architecture setting="
                + PlayerSettings.GetArchitecture(UnityEditor.Build.NamedBuildTarget.Standalone));
            BuildDesktop(
                BuildTarget.StandaloneOSX,
                Path.Combine("Builds", "macOS", MacApp),
                "macOS");
        }

        static void BuildDesktop(BuildTarget target, string relativeOutputPath, string platformName)
        {
            if (!File.Exists(Path.Combine(ProjectRoot, ScenePath))) SetupScene();
            RebuildGeneratedAssets();
            PlayerSettings.productName = "Minecraft Recreation";
            PlayerSettings.companyName = "Opus 5.5";
            PlayerSettings.SetApplicationIdentifier(
                UnityEditor.Build.NamedBuildTarget.Standalone,
                "io.github.prokopiy8247.minecraftrecreation");
            PlayerSettings.defaultScreenWidth = 1600;
            PlayerSettings.defaultScreenHeight = 900;
            PlayerSettings.fullScreenMode = FullScreenMode.Windowed;
            PlayerSettings.resizableWindow = true;
            PlayerSettings.runInBackground = true;
            PlayerSettings.visibleInBackground = true;
            string outputPath = Path.Combine(ProjectRoot, relativeOutputPath);
            string outputDirectory = Path.GetDirectoryName(outputPath);
            if (Directory.Exists(outputDirectory)) Directory.Delete(outputDirectory, true);
            Directory.CreateDirectory(outputDirectory);
            BuildReport report;
            if (target == BuildTarget.StandaloneOSX)
            {
                var profile = UnityEditor.Build.Profile.BuildProfile.GetBuildProfileAtPath(MacBuildProfilePath);
                if (profile == null)
                    throw new FileNotFoundException("The macOS Universal build profile is missing.", MacBuildProfilePath);
                report = BuildPipeline.BuildPlayer(new BuildPlayerWithProfileOptions
                {
                    buildProfile = profile,
                    locationPathName = outputPath,
                    options = BuildOptions.None,
                });
            }
            else
            {
                report = BuildPipeline.BuildPlayer(new BuildPlayerOptions
                {
                    scenes = new[] { ScenePath },
                    locationPathName = outputPath,
                    target = target,
                    targetGroup = BuildTargetGroup.Standalone,
                    options = BuildOptions.None,
                });
            }
            var s = report.summary;
            var msg = "[Build:" + platformName + "] " + s.result + " in " + s.totalTime + ", "
                + (s.totalSize / (1024 * 1024)) + " MB, errors=" + s.totalErrors + " warnings=" + s.totalWarnings
                + (s.result == BuildResult.Succeeded ? ", output=" + outputPath : "");
            Debug.Log(msg);
            File.WriteAllText(Path.Combine(OutDir, "build-" + platformName.ToLowerInvariant() + ".txt"), msg + "\n");
            if (s.result != BuildResult.Succeeded && Application.isBatchMode) EditorApplication.Exit(1);
        }

        public const string PlayerExe = "MinecraftRecreation.exe";
        public const string MacApp = "Minecraft Recreation.app";

        /// <summary>One-shot used by the automation: scene + generated assets + build.</summary>
        public static void All()
        {
            SetupScene();
            BuildWindows();
        }
    }
}
