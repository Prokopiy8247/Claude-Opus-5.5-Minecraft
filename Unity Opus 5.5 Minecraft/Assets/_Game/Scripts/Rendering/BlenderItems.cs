using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Blender-built geometry for held and dropped items. Tools/BlenderBridge/item_pipeline.py extrudes the chosen item
    /// sprites pixel by pixel in Blender (face for face the twin of ItemRender.ExtrudedMesh) and builds the torch, lantern
    /// and campfire props from their block textures; MCR.EditorTools.ItemModelImporter stores the imported FBX meshes as
    /// Resources/Models/Items/&lt;id&gt;_mesh and Resources/Models/Props/&lt;id&gt;_mesh (plus &lt;id&gt;_tex, the block texture of
    /// each prop submesh). The FBX meshes carry plain 2D uvs, but items are drawn with Res.ItemMats, the chunk shader that
    /// takes its texture-array layer from the vertex stream, so each mesh is copied once into the ChunkVertex layout with
    /// its layer, animation frame count, face shade and full light: the vertex data the procedural meshes carry, hence
    /// the same material and the same look. Anything missing or unusable falls back to the procedural meshes.
    /// </summary>
    public static class BlenderItems
    {
        /// <summary>Prefer the Blender meshes where they exist; false (or -mcrNoBlenderItems on the command line) keeps the procedural ones.</summary>
        public static bool Enabled = !HasArg("-mcrNoBlenderItems");

        const string ItemsPath = "Models/Items/", PropsPath = "Models/Props/";

        sealed class Entry
        {
            public Mesh item, prop;           // Resources copies of the FBX meshes, null when the item has none
            public string[] propTextures;     // block texture of every prop submesh
            public Mesh built;                // ChunkVertex copy, built on first use
            public int builtLayer = int.MinValue;
        }
        static readonly Dictionary<string, Entry> cache = new Dictionary<string, Entry>();
        static bool announced;

        /// <summary>
        /// The Blender mesh of an item in the ChunkVertex layout, ready for Res.ItemMats[1]: its prop model (torches,
        /// lanterns, campfires) or its voxel sprite sampling texture layer <paramref name="layer"/>. Null when disabled or
        /// when the item has neither; the caller then extrudes the sprite procedurally.
        /// </summary>
        public static Mesh ItemMesh(Item it, int layer)
        {
            if (!Enabled || it == null || string.IsNullOrEmpty(it.id)) return null;
            var e = Get(it.id);
            if (e.item == null && e.prop == null) return null;
            if (e.builtLayer != layer)
            {
                e.builtLayer = layer;   // also remembers a failed conversion, so it is not retried every frame
                e.built = e.prop != null
                    ? ToChunkMesh(e.prop, PropLayers(e.propTextures), true, "prop_" + it.id, SpriteFit(e.prop, layer))
                    : ToChunkMesh(e.item, new[] { layer }, false, "item_" + it.id);
                if (e.built == null) Debug.LogWarning("[BlenderItems] " + it.id + ": Blender mesh unusable, using the procedural one");
                else if (!announced)
                {
                    announced = true;
                    Debug.Log("[BlenderItems] Using the Blender item meshes from Resources/Models/Items and Props (first: " + it.id + ")");
                }
            }
            return e.built;
        }

        /// <summary>True when the item has a Blender voxel or prop mesh (ItemRender.ModelFor then prefers it to a ModelDef model).</summary>
        public static bool Has(string id)
        {
            if (!Enabled || string.IsNullOrEmpty(id)) return false;
            var e = Get(id);
            return e.item != null || e.prop != null;
        }

        static Entry Get(string id)
        {
            if (cache.TryGetValue(id, out var e)) return e;
            e = new Entry { item = Readable(Resources.Load<Mesh>(ItemsPath + id + "_mesh"), id), prop = Readable(Resources.Load<Mesh>(PropsPath + id + "_mesh"), id) };
            if (e.prop != null)
            {
                var tex = Resources.Load<TextAsset>(PropsPath + id + "_tex");
                e.propTextures = tex != null ? tex.text.Split(new[] { '\n', '\r' }, System.StringSplitOptions.RemoveEmptyEntries) : null;
                if (e.propTextures == null || e.propTextures.Length != e.prop.subMeshCount)
                {
                    Debug.LogWarning("[BlenderItems] " + id + ": prop textures missing or not matching the submeshes, prop ignored");
                    e.prop = null;
                }
            }
            cache[id] = e;
            return e;
        }

        static Mesh Readable(Mesh m, string id)
        {
            if (m == null || m.isReadable) return m;
            Debug.LogWarning("[BlenderItems] " + id + ": mesh " + m.name + " is not readable, ignored");
            return null;
        }

        static int[] PropLayers(string[] textures)
        {
            var layers = new int[textures.Length];
            for (int i = 0; i < textures.Length; i++) layers[i] = Tex.Has(textures[i]) ? Tex.Id(textures[i]) : 0;
            return layers;
        }

        /// <summary>Widest a prop may be, in sprite quads: the original holds block models at 0.4 where sprites get 0.68 (0.25 vs 0.5 on the ground).</summary>
        const float MaxPropWidth = 0.6f;

        /// <summary>
        /// Where a prop takes its sprite's place. Every held and dropped item view places the unit sprite quad, and the
        /// Blender props are block-proportioned (block centre at the origin), so a lantern would hang below where its
        /// sprite was and a campfire would fill the whole quad. This centres the prop on the box of the sprite's opaque
        /// pixels and shrinks it (uniformly, never enlarging it, so its pixels stay the size of a sprite's) until its
        /// bounds fit that box (height, and the wider of its x / z extents) and it is at most MaxPropWidth wide.
        /// </summary>
        static Matrix4x4 SpriteFit(Mesh prop, int layer)
        {
            var px = layer >= 0 ? Res.GetLayerPixels(layer) : null;
            if (px == null || px.Length < 256) return Matrix4x4.identity;
            int x0 = 16, x1 = -1, y0 = 16, y1 = -1;
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                    if (px[y * 16 + x].a > 127) { x0 = Mathf.Min(x0, x); x1 = Mathf.Max(x1, x); y0 = Mathf.Min(y0, y); y1 = Mathf.Max(y1, y); }
            var b = prop.bounds;
            float pw = Mathf.Max(b.size.x, b.size.z), ph = b.size.y;
            if (x1 < 0 || pw < 1e-4f || ph < 1e-4f) return Matrix4x4.identity;
            // sprite pixel (x, y), y top-down, spans x -0.5 + x/16 .. and y 0.5 - y/16 .. (ItemRender.ExtrudedMesh)
            float sw = (x1 - x0 + 1) / 16f, sh = (y1 - y0 + 1) / 16f;
            var centre = new Vector3(-0.5f + (x0 + x1 + 1) / 32f, 0.5f - (y0 + y1 + 1) / 32f, 0f);
            float s = Mathf.Min(1f, sw / pw, sh / ph, MaxPropWidth / pw);
            return Matrix4x4.TRS(centre - b.center * s, Quaternion.identity, Vector3.one * s);
        }

        /// <summary>
        /// A readable FBX-imported mesh (positions, normals, uv0; one submesh per texture) as a single-submesh ChunkVertex
        /// mesh: submesh s samples texture layer layers[s] (animated layers keep their frame count), the face shade comes
        /// from the normal (the item extrusion's shades, or the block face shades for props), the light is full, as in
        /// ItemRender's procedural meshes. <paramref name="place"/> (a uniform scale and offset) moves the positions.
        /// Null when the source cannot be read.
        /// </summary>
        public static Mesh ToChunkMesh(Mesh src, int[] layers, bool blockShade, string name, Matrix4x4? place = null)
        {
            if (src == null || !src.isReadable || layers == null || layers.Length == 0) return null;
            var pos = new List<Vector3>(); var nrm = new List<Vector3>(); var uv = new List<Vector2>();
            src.GetVertices(pos); src.GetNormals(nrm); src.GetUVs(0, uv);
            int n = pos.Count;
            if (n == 0 || nrm.Count != n || uv.Count != n) return null;
            var verts = new List<ChunkVertex>(n);
            var idx = new List<int>();
            var tris = new List<int>();
            var remap = new Dictionary<int, int>();
            for (int s = 0; s < src.subMeshCount; s++)
            {
                int layer = layers[Mathf.Min(s, layers.Length - 1)];
                ushort hl = HalfConv.ToHalf(layer), frames = HalfConv.ToHalf(Mathf.Max(1, Tex.Frames(layer)));
                src.GetTriangles(tris, s);
                remap.Clear();
                foreach (int i in tris)
                {
                    if (!remap.TryGetValue(i, out int j))
                    {
                        j = verts.Count;
                        remap[i] = j;
                        byte shade = blockShade ? BlockShade(nrm[i]) : ItemShade(nrm[i]);
                        var p = place.HasValue ? place.Value.MultiplyPoint3x4(pos[i]) : pos[i];
                        verts.Add(new ChunkVertex
                        {
                            x = p.x, y = p.y, z = p.z,
                            color = 0x00FFFFFFu | ((uint)shade << 24),   // white tint, shade in alpha (ItemRender.V)
                            u = HalfConv.ToHalf(uv[i].x), v = HalfConv.ToHalf(uv[i].y), layer = hl, anim = frames,
                            light = 0xFFFF
                        });
                    }
                    idx.Add(j);
                }
            }
            return idx.Count > 0 ? ItemRender.FinishMesh(name, verts, idx) : null;
        }

        /// <summary>Face shade of ItemRender.ExtrudedMesh: front, back and top edges 255, bottom edges 140, left 200, right 170.</summary>
        public static byte ItemShade(Vector3 n)
        {
            if (n.y > 0.5f) return 255;
            if (n.y < -0.5f) return 140;
            if (n.x < -0.5f) return 200;
            if (n.x > 0.5f) return 170;
            return 255;
        }

        /// <summary>Face shade of the chunk mesher (MeshCtx.FaceShade): up 1, down 0.5, north/south 0.8, west/east 0.6; slanted planes (flame cross, lantern handle) 0.9 like MeshCtx.Cross.</summary>
        public static byte BlockShade(Vector3 n)
        {
            if (n.y > 0.5f) return 255;
            if (n.y < -0.5f) return 128;
            if (Mathf.Abs(n.z) > 0.9f) return 204;
            if (Mathf.Abs(n.x) > 0.9f) return 153;
            return 230;
        }

        /// <summary>Forget the loaded and converted meshes (texture reloads), like ItemRender.ClearCaches.</summary>
        public static void ClearCaches()
        {
            cache.Clear();
            announced = false;
        }

        static bool HasArg(string flag)
        {
            try
            {
                foreach (var a in System.Environment.GetCommandLineArgs())
                    if (string.Equals(a, flag, System.StringComparison.OrdinalIgnoreCase)) return true;
            }
            catch (System.Exception) { }
            return false;
        }
    }
}
