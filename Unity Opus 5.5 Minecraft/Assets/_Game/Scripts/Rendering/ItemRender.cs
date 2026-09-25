using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace MCR
{
    /// <summary>
    /// World-space item visuals: block items as small block meshes, flat items as extruded pixel sprites
    /// (one voxel-thick, like the original), billboards for projectiles, experience orbs, and the Blender-built
    /// 3D models for weapons that have them (shield, trident, spear). Held and dropped sprites prefer the Blender-built
    /// voxel items and the torch / lantern / campfire props (MCR.BlenderItems) and fall back to the procedural extrusion.
    /// </summary>
    public static class ItemRender
    {
        static readonly Dictionary<ushort, Mesh> blockMeshes = new Dictionary<ushort, Mesh>();
        static readonly Dictionary<int, Mesh> extruded = new Dictionary<int, Mesh>();
        static readonly Dictionary<int, Mesh> flat = new Dictionary<int, Mesh>();
        static Mesh arrowMesh;
        static Texture2D orbTex;
        static Material orbMat;
        static readonly Dictionary<int, Mesh> orbMeshes = new Dictionary<int, Mesh>();

        // ------------------------------------------------------------------ layers
        /// <summary>Texture array layer of the flat sprite used for this item, or -1 if it renders as a block.</summary>
        public static int SpriteLayer(Item it)
        {
            if (it == null) return 0;
            if (it.block != null && !ItemSprites.NeedsFlatSprite(it))
            {
                if (IsFlatBlock(it.block)) return it.block.particleTex;
                return -1;
            }
            string n = ItemSprites.SpriteName(it);
            if (n == null) return 0;
            return Tex.Has("item/" + n) ? Tex.Id("item/" + n) : 0;
        }

        /// <summary>Blocks whose item shows the texture flat (plants, torches, rails, ladders ...).</summary>
        public static bool IsFlatBlock(Block b)
        {
            if (b == null) return false;
            return b is PlantBlock || b is TorchBlock || b is RailBlock || b is LadderBlock || b is VineBlock || b is TallPlantBlock || b is LilyPadBlock
                || b is ShortBlock || b is AmethystClusterBlock || b is CaveVinesBlock
                || b.id == "lever" || b.id == "tripwire_hook" || b.id.EndsWith("_coral") || b.id.EndsWith("_coral_fan") || b.id == "glow_lichen" || b.id == "sculk_vein" || b.id == "hanging_roots" || b.id == "spore_blossom";
        }

        public static int ParticleLayer(Item it)
        {
            if (it == null) return 0;
            int l = SpriteLayer(it);
            if (l >= 0) return l;
            return it.block != null ? it.block.particleTex : 0;
        }

        // ------------------------------------------------------------------ blocks
        public static Mesh BlockMesh(ushort state)
        {
            if (blockMeshes.TryGetValue(state, out var m)) return m;
            m = ChunkMesher.BuildSingleBlock(state, true);
            blockMeshes[state] = m;
            return m;
        }

        public static GameObject CreateBlockVisual(ushort state, float scale)
        {
            if (!Res.Ready) return null;
            var go = new GameObject("BlockVisual");
            var child = new GameObject("mesh");
            child.transform.SetParent(go.transform, false);
            child.transform.localScale = Vector3.one * scale;
            child.transform.localPosition = new Vector3(0, 0, 0);
            var mf = child.AddComponent<MeshFilter>();
            mf.sharedMesh = BlockMesh(state);
            var mr = child.AddComponent<MeshRenderer>();
            mr.sharedMaterials = Res.ItemMats;
            SetupRenderer(mr);
            return go;
        }

        static void SetupRenderer(Renderer r)
        {
            r.shadowCastingMode = ShadowCastingMode.Off;
            r.receiveShadows = false;
            r.lightProbeUsage = LightProbeUsage.Off;
            r.reflectionProbeUsage = ReflectionProbeUsage.Off;
        }

        // ------------------------------------------------------------------ sprites
        static uint Pack(byte r, byte g, byte b, byte a) => (uint)(r | (g << 8) | (b << 16) | (a << 24));
        static readonly uint FullLight = 0xFFFF;

        static void V(List<ChunkVertex> list, Vector3 p, float u, float v, int layer, byte shade, Color32 tint)
        {
            list.Add(new ChunkVertex
            {
                x = p.x, y = p.y, z = p.z,
                color = Pack(tint.r, tint.g, tint.b, shade),
                u = HalfConv.ToHalf(u), v = HalfConv.ToHalf(v), layer = HalfConv.ToHalf(layer), anim = HalfConv.ToHalf(Mathf.Max(1, Tex.Frames(layer))),
                light = FullLight
            });
        }

        internal static Mesh FinishMesh(string name, List<ChunkVertex> verts, List<int> idx)
        {
            var mesh = new Mesh { name = name };
            mesh.SetVertexBufferParams(verts.Count, ChunkVertex.Layout);
            mesh.SetVertexBufferData(verts, 0, 0, verts.Count);
            mesh.SetIndexBufferParams(idx.Count, IndexFormat.UInt32);
            mesh.SetIndexBufferData(idx, 0, 0, idx.Count);
            mesh.subMeshCount = 1;
            mesh.SetSubMesh(0, new SubMeshDescriptor(0, idx.Count), MeshUpdateFlags.DontRecalculateBounds);
            mesh.bounds = new Bounds(Vector3.zero, Vector3.one * 1.2f);
            return mesh;
        }

        static void Quad(List<ChunkVertex> vs, List<int> idx, Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector2 ua, Vector2 ub, Vector2 uc, Vector2 ud, int layer, byte shade, Color32 tint)
        {
            int s = vs.Count;
            V(vs, a, ua.x, ua.y, layer, shade, tint); V(vs, b, ub.x, ub.y, layer, shade, tint); V(vs, c, uc.x, uc.y, layer, shade, tint); V(vs, d, ud.x, ud.y, layer, shade, tint);
            idx.Add(s); idx.Add(s + 1); idx.Add(s + 2); idx.Add(s); idx.Add(s + 2); idx.Add(s + 3);
        }

        /// <summary>Flat double-sided sprite quad centred on the origin (1 unit wide), readable when viewed along +Z.</summary>
        public static Mesh FlatMesh(int layer)
        {
            if (flat.TryGetValue(layer, out var m)) return m;
            var vs = new List<ChunkVertex>(); var idx = new List<int>();
            var white = new Color32(255, 255, 255, 255);
            Quad(vs, idx, new Vector3(-0.5f, -0.5f, 0), new Vector3(-0.5f, 0.5f, 0), new Vector3(0.5f, 0.5f, 0), new Vector3(0.5f, -0.5f, 0),
                new Vector2(0, 0), new Vector2(0, 1), new Vector2(1, 1), new Vector2(1, 0), layer, 255, white);
            m = FinishMesh("flat_" + layer, vs, idx);
            flat[layer] = m;
            return m;
        }

        /// <summary>One-pixel-thick extrusion of a 16x16 sprite layer (front, back and an edge strip for every opaque border pixel).</summary>
        public static Mesh ExtrudedMesh(int layer)
        {
            if (extruded.TryGetValue(layer, out var m)) return m;
            var px = Res.GetLayerPixels(layer); // top-down rows
            var vs = new List<ChunkVertex>(); var idx = new List<int>();
            var white = new Color32(255, 255, 255, 255);
            const float T = 1f / 32f; // half thickness
            const float P = 1f / 16f;
            // front (-Z facing, readable from -Z) and back
            Quad(vs, idx, new Vector3(-0.5f, -0.5f, -T), new Vector3(-0.5f, 0.5f, -T), new Vector3(0.5f, 0.5f, -T), new Vector3(0.5f, -0.5f, -T),
                new Vector2(0, 0), new Vector2(0, 1), new Vector2(1, 1), new Vector2(1, 0), layer, 255, white);
            Quad(vs, idx, new Vector3(0.5f, -0.5f, T), new Vector3(0.5f, 0.5f, T), new Vector3(-0.5f, 0.5f, T), new Vector3(-0.5f, -0.5f, T),
                new Vector2(1, 0), new Vector2(1, 1), new Vector2(0, 1), new Vector2(0, 0), layer, 255, white);
            bool Opaque(int x, int y) => x >= 0 && y >= 0 && x < 16 && y < 16 && px != null && px[y * 16 + x].a > 127;
            if (px != null)
            {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++)
                    {
                        if (!Opaque(x, y)) continue;
                        // pixel (x, y) with y top-down: world y from 0.5 - y/16 (top) to 0.5 - (y+1)/16
                        float x0 = -0.5f + x * P, x1 = x0 + P, yTop = 0.5f - y * P, yBot = yTop - P;
                        float u0 = (x + 0.1f) / 16f, u1 = (x + 0.9f) / 16f, vT = 1f - (y + 0.1f) / 16f, vB = 1f - (y + 0.9f) / 16f;
                        var uvA = new Vector2(u0, vB); var uvB = new Vector2(u0, vT); var uvC = new Vector2(u1, vT); var uvD = new Vector2(u1, vB);
                        if (!Opaque(x, y - 1)) Quad(vs, idx, new Vector3(x0, yTop, -T), new Vector3(x0, yTop, T), new Vector3(x1, yTop, T), new Vector3(x1, yTop, -T), uvA, uvB, uvC, uvD, layer, 255, white);
                        if (!Opaque(x, y + 1)) Quad(vs, idx, new Vector3(x1, yBot, -T), new Vector3(x1, yBot, T), new Vector3(x0, yBot, T), new Vector3(x0, yBot, -T), uvA, uvB, uvC, uvD, layer, 140, white);
                        if (!Opaque(x - 1, y)) Quad(vs, idx, new Vector3(x0, yBot, T), new Vector3(x0, yTop, T), new Vector3(x0, yTop, -T), new Vector3(x0, yBot, -T), uvA, uvB, uvC, uvD, layer, 200, white);
                        if (!Opaque(x + 1, y)) Quad(vs, idx, new Vector3(x1, yBot, -T), new Vector3(x1, yTop, -T), new Vector3(x1, yTop, T), new Vector3(x1, yBot, T), uvA, uvB, uvC, uvD, layer, 170, white);
                    }
            }
            m = FinishMesh("extruded_" + layer, vs, idx);
            extruded[layer] = m;
            return m;
        }

        static GameObject MeshObject(string name, Mesh mesh, float scale, Material mat)
        {
            var go = new GameObject(name);
            var child = new GameObject("mesh");
            child.transform.SetParent(go.transform, false);
            child.transform.localScale = Vector3.one * scale;
            var mf = child.AddComponent<MeshFilter>(); mf.sharedMesh = mesh;
            var mr = child.AddComponent<MeshRenderer>(); mr.sharedMaterial = mat;
            SetupRenderer(mr);
            return go;
        }

        /// <summary>Flat billboard-style sprite for thrown projectiles (snowballs, pearls, fire charges...).</summary>
        public static GameObject CreateSpriteVisual(ItemStack stack, float scale)
        {
            if (!Res.Ready || stack == null || stack.IsEmpty) return null;
            int layer = SpriteLayer(stack.item);
            if (layer < 0) return CreateBlockVisual(stack.item.block.DefaultState, scale * 0.5f);
            return MeshObject("Sprite_" + stack.item.id, FlatMesh(layer), scale, Res.ItemMats[1]);
        }

        /// <summary>
        /// 3D extruded sprite (dropped items, held items in first and third person). The Blender-built mesh of the item
        /// (its voxel sprite, or the prop model of a torch, lantern or campfire) is used when the FBX pipeline produced
        /// one; both are centred like the unit sprite quad, so they drop in at the same place and scale.
        /// </summary>
        public static GameObject CreateExtrudedVisual(ItemStack stack, float scale)
        {
            if (!Res.Ready || stack == null || stack.IsEmpty) return null;
            int layer = SpriteLayer(stack.item);
            if (layer < 0) return CreateBlockVisual(stack.item.block.DefaultState, scale * 0.5f);
            var mesh = BlenderItems.ItemMesh(stack.item, layer) ?? ExtrudedMesh(layer);
            return MeshObject("Item_" + stack.item.id, mesh, scale, Res.ItemMats[1]);
        }

        /// <summary>Dropped item entity visual: small blocks or extruded sprites, with 1-5 copies depending on stack size.</summary>
        public static GameObject CreateDroppedVisual(ItemStack stack)
        {
            if (!Res.Ready || stack == null || stack.IsEmpty) return null;
            var root = new GameObject("Dropped_" + stack.item.id);
            int copies = stack.count >= 48 ? 5 : stack.count >= 32 ? 4 : stack.count >= 16 ? 3 : stack.count > 1 ? 2 : 1;
            bool block = SpriteLayer(stack.item) < 0;
            var rng = new RNG(stack.item.index * 31 + 7);
            for (int i = 0; i < copies; i++)
            {
                var one = block ? CreateBlockVisual(stack.item.block.DefaultState, 0.25f) : CreateExtrudedVisual(stack, 0.5f);
                if (one == null) continue;
                one.transform.SetParent(root.transform, false);
                if (i > 0)
                {
                    one.transform.localPosition = block
                        ? new Vector3((rng.NextFloat() - 0.5f) * 0.12f, (rng.NextFloat() - 0.5f) * 0.12f, (rng.NextFloat() - 0.5f) * 0.12f)
                        : new Vector3((rng.NextFloat() - 0.5f) * 0.1f, (rng.NextFloat() - 0.5f) * 0.1f, i * 0.045f);
                }
                if (!block) one.transform.localPosition += Vector3.up * 0.125f;
            }
            return root;
        }

        /// <summary>Held item model: authored 3D models for weapons that have one, otherwise block/extruded sprite.</summary>
        public static GameObject CreateHeldModelVisual(ItemStack stack, float scale)
        {
            if (!Res.Ready || stack == null || stack.IsEmpty) return null;
            string model = ModelFor(stack.item);
            if (model != null)
            {
                var def = MobModels.Get(model);
                if (def != null)
                {
                    var go = new GameObject("Held_" + stack.item.id);
                    var m = ModelRenderer.Build(def, go.transform, null);
                    m.transform.localScale = Vector3.one * scale;
                    return go;
                }
            }
            return CreateExtrudedVisual(stack, scale);
        }

        public static string ModelFor(Item it)
        {
            if (it == null) return null;
            // a Blender voxel item (bow, crossbow) is its sprite extruded, the way the original holds them; it wins over
            // the rough ModelDef stand-in, which stays the fallback when the FBX pipeline has not produced the item
            if (BlenderItems.Has(it.id)) return null;
            if (!string.IsNullOrEmpty(it.modelName) && MobModels.Get(it.modelName) != null) return it.modelName;
            switch (it.id)
            {
                case "shield": return "shield";
                case "trident": return "trident";
                case "bow": return null; // the bow reads better as its pixel sprite in hand
                case "crossbow": return null;
            }
            return null;
        }

        // ------------------------------------------------------------------ arrows / orbs
        public static GameObject CreateArrowVisual(string ammoId)
        {
            if (!Res.Ready) return null;
            int layer = Tex.Has("item/" + (ammoId ?? "arrow")) ? Tex.Id("item/" + (ammoId ?? "arrow")) : Tex.Id("item/arrow");
            // two crossed extruded sprites; the arrow sprite is diagonal so rotate 45 degrees to point along +Z
            var go = new GameObject("Arrow");
            for (int i = 0; i < 2; i++)
            {
                var c = new GameObject("fin" + i);
                c.transform.SetParent(go.transform, false);
                c.transform.localRotation = Quaternion.Euler(0, 0, i * 90f) * Quaternion.Euler(0, -90f, 0) * Quaternion.Euler(0, 0, -45f);
                c.transform.localScale = Vector3.one * 0.7f;
                c.transform.localPosition = Vector3.zero;
                var mf = c.AddComponent<MeshFilter>(); mf.sharedMesh = FlatMesh(layer);
                var mr = c.AddComponent<MeshRenderer>(); mr.sharedMaterial = Res.ItemMats[1];
                SetupRenderer(mr);
            }
            return go;
        }

        static void EnsureOrb()
        {
            if (orbTex != null) return;
            orbTex = new Texture2D(64, 16, TextureFormat.RGBA32, false) { filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp, name = "xp_orbs" };
            var px = new Color32[64 * 16];
            // four orb sizes side by side, original pixel design: bright core, green-yellow body, dark rim
            for (int k = 0; k < 4; k++)
            {
                float r = 3f + k * 1.4f;
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++)
                    {
                        float dx = x + 0.5f - 8f, dy = y + 0.5f - 8f;
                        float d = Mathf.Sqrt(dx * dx + dy * dy);
                        Color32 c = default;
                        if (d <= r)
                        {
                            float t = d / r;
                            if (t > 0.8f) c = new Color32(60, 100, 20, 255);
                            else if (t > 0.45f) c = new Color32(150, 220, 60, 255);
                            else c = new Color32(240, 255, 170, 255);
                            if (dx < -r * 0.2f && dy > r * 0.1f && t < 0.7f) c = new Color32(255, 255, 220, 255);
                        }
                        px[y * 64 + k * 16 + x] = c;
                    }
            }
            orbTex.SetPixels32(px);
            orbTex.Apply(false, false);
            orbMat = Res.EntityMaterial(orbTex);
        }

        public static GameObject CreateOrbVisual(int value)
        {
            if (!Res.Ready) return null;
            EnsureOrb();
            int k = value >= 37 ? 3 : value >= 17 ? 2 : value >= 7 ? 1 : 0;
            if (!orbMeshes.TryGetValue(k, out var mesh))
            {
                mesh = new Mesh { name = "orb" + k };
                float s = 0.25f;
                mesh.SetVertices(new List<Vector3> { new Vector3(-s, -s, 0), new Vector3(-s, s, 0), new Vector3(s, s, 0), new Vector3(s, -s, 0) });
                float u0 = k / 4f, u1 = (k + 1) / 4f;
                mesh.SetUVs(0, new List<Vector2> { new Vector2(u0, 0), new Vector2(u0, 1), new Vector2(u1, 1), new Vector2(u1, 0) });
                mesh.SetNormals(new List<Vector3> { Vector3.back, Vector3.back, Vector3.back, Vector3.back });
                mesh.SetTriangles(new[] { 0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2 }, 0);
                orbMeshes[k] = mesh;
            }
            var go = new GameObject("XpOrb");
            var c = new GameObject("quad");
            c.transform.SetParent(go.transform, false);
            var mf = c.AddComponent<MeshFilter>(); mf.sharedMesh = mesh;
            var mr = c.AddComponent<MeshRenderer>(); mr.sharedMaterial = orbMat;
            SetupRenderer(mr);
            return go;
        }

        public static void ClearCaches()
        {
            blockMeshes.Clear(); extruded.Clear(); flat.Clear(); orbMeshes.Clear();
            arrowMesh = null;
            BlenderItems.ClearCaches();
        }
    }
}
