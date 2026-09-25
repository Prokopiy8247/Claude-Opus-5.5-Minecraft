using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace MCR
{
    /// <summary>
    /// Visuals for block entities that the chunk mesher cannot express: chests (the Blender-built chest models,
    /// with the lid swinging open while someone looks inside, double chests stretched across both blocks), the
    /// little mob turning inside a spawner cage, and the light beam of an active beacon. Visuals exist only near
    /// the camera and are rebuilt when a chunk comes back into range.
    /// </summary>
    public static class BlockEntityRenderer
    {
        const float Range = 72f;
        static readonly List<BlockEntity> active = new List<BlockEntity>();
        static readonly HashSet<BlockEntity> seen = new HashSet<BlockEntity>();
        static Mesh beamMesh;
        static Material beamMat, glowMat;
        static int frame;

        /// <summary>Per-frame update from the camera: create, animate and retire visuals around the viewer.</summary>
        public static void Update(World w, Vector3 camPos, float partial)
        {
            if (w == null) return;
            frame++;
            seen.Clear();
            int ccx = Mathf.FloorToInt(camPos.x) >> 4, ccz = Mathf.FloorToInt(camPos.z) >> 4, r = Mathf.CeilToInt(Range / 16f);
            for (int cz = ccz - r; cz <= ccz + r; cz++)
                for (int cx = ccx - r; cx <= ccx + r; cx++)
                {
                    var c = w.ReadyChunk(cx << 4, cz << 4);
                    if (c == null || c.blockEntities.Count == 0) continue;
                    foreach (var be in c.blockEntities.Values)
                    {
                        if (be == null || !be.HasVisual) continue;
                        if ((be.pos.Center - camPos).sqrMagnitude > Range * Range) continue;
                        seen.Add(be);
                        try { Animate(w, be, partial); }
                        catch (System.Exception e) { Debug.LogWarning("[BlockEntityRenderer] " + be.TypeId + ": " + e.Message); DestroyVisual(be); }
                    }
                }
            // retire visuals that fell out of range or whose chunk went away
            for (int i = active.Count - 1; i >= 0; i--)
            {
                var be = active[i];
                if (seen.Contains(be) && be.visual != null) continue;
                DestroyVisual(be);
                active.RemoveAt(i);
            }
        }

        /// <summary>Drops every visual (the world is being unloaded).</summary>
        public static void Clear()
        {
            foreach (var be in active) DestroyVisual(be);
            active.Clear();
            seen.Clear();
        }

        static void DestroyVisual(BlockEntity be)
        {
            if (be.visual != null) Object.Destroy(be.visual);
            be.visual = null;
        }

        static void Animate(World w, BlockEntity be, float partial)
        {
            if (be.visual == null)
            {
                be.visual = Create(w, be);
                if (be.visual == null) return;
                active.Add(be);
                EntityLight.Apply(be.visual, w, be.pos.Center + Vector3.up * 0.5f);
            }
            else if ((frame + be.pos.x * 7 + be.pos.z * 13) % 20 == 0)
                EntityLight.Apply(be.visual, w, be.pos.Center + Vector3.up * 0.5f);

            switch (be)
            {
                case ShulkerBoxEntity _: break;
                case ChestEntity chest:
                {
                    var lid = be.visual.transform.Find("model/bottom/lid") ?? FindDeep(be.visual.transform, "lid");
                    if (lid != null)
                    {
                        float l = Mathf.Lerp(chest.prevLid, chest.lid, partial);
                        l = 1f - (1f - l) * (1f - l) * (1f - l);
                        lid.localRotation = Quaternion.Euler(-l * 90f, 0f, 0f);
                    }
                    break;
                }
                case SpawnerEntity sp:
                {
                    var mob = be.visual.transform.Find("mob");
                    if (mob != null) mob.localRotation = Quaternion.Euler(0f, Mathf.LerpAngle(sp.prevSpin, sp.spin, partial) * 10f, 0f);
                    break;
                }
                case BeaconEntity beacon:
                {
                    bool on = beacon.levels > 0 && w.CanSeeSky(be.pos.Offset(Dir.Up));
                    be.visual.SetActive(on);
                    if (on)
                    {
                        float t = Time.time;
                        be.visual.transform.localRotation = Quaternion.Euler(0f, t * 36f, 0f);
                        beamMat.mainTextureOffset = new Vector2(0f, -t * 0.4f);
                    }
                    break;
                }
            }
        }

        static GameObject Create(World w, BlockEntity be)
        {
            ushort state = w.GetState(be.pos);
            var block = Blocks.ByState[state];
            int meta = state - block.baseState;
            switch (be)
            {
                case ShulkerBoxEntity _: return null; // drawn as a plain box by the chunk mesher
                case ChestEntity _: return CreateChest(block, meta, be.pos);
                case SpawnerEntity sp: return CreateSpawnerMob(sp);
                case BeaconEntity _: return CreateBeam(be.pos, w);
            }
            return null;
        }

        // ------------------------------------------------------------------ chests
        static GameObject CreateChest(Block block, int meta, Int3 pos)
        {
            string model = block.id == "trapped_chest" ? "trapped_chest" : block.id == "ender_chest" ? "ender_chest" : block.id.Contains("copper_chest") ? "copper_chest" : "chest";
            var def = MobModels.Get(model) ?? MobModels.Get("chest");
            if (def == null) return null;
            int type = (meta >> 2) & 3;
            // a double chest is drawn once, by its half whose partner is on its right, stretched over both blocks
            if (type == 2) return new GameObject("ChestHalf_" + pos);
            var go = new GameObject("Chest_" + pos);
            var built = ModelRenderer.Build(def, go.transform, null);
            built.name = "model";
            var facing = DirUtil.FromHorizIndex(meta & 3);
            go.transform.position = new Vector3(pos.x + 0.5f, pos.y, pos.z + 0.5f);
            go.transform.rotation = Quaternion.Euler(0f, DirUtil.HorizIndex(facing) * 90f, 0f);
            if (type == 1)
            {
                built.transform.localPosition = new Vector3(0.5f, 0f, 0f);
                built.transform.localScale = new Vector3(30f / 14f, 1f, 1f);
            }
            return go;
        }

        static Transform FindDeep(Transform t, string name)
        {
            if (t.name == name) return t;
            foreach (Transform c in t) { var f = FindDeep(c, name); if (f != null) return f; }
            return null;
        }

        // ------------------------------------------------------------------ spawner
        static GameObject CreateSpawnerMob(SpawnerEntity sp)
        {
            var mobDef = MobRegistry.Get(sp.mob);
            var def = MobModels.Get(mobDef != null && mobDef.model != null ? mobDef.model : sp.mob);
            if (def == null) return null;
            var go = new GameObject("Spawner_" + sp.pos);
            go.transform.position = new Vector3(sp.pos.x + 0.5f, sp.pos.y + 0.1f, sp.pos.z + 0.5f);
            var pivot = new GameObject("mob").transform;
            pivot.SetParent(go.transform, false);
            var m = ModelRenderer.Build(def, pivot, null);
            // fit the model inside the cage: the larger of its height and width becomes about half a block
            var b = new Bounds();
            bool any = false;
            foreach (var mf in m.GetComponentsInChildren<MeshFilter>())
            {
                if (mf.sharedMesh == null) continue;
                var mb = mf.sharedMesh.bounds;
                var c = m.transform.InverseTransformPoint(mf.transform.TransformPoint(mb.center));
                var e = new Bounds(c, mb.size);
                if (!any) { b = e; any = true; } else b.Encapsulate(e);
            }
            float size = any ? Mathf.Max(b.size.y, Mathf.Max(b.size.x, b.size.z)) : 1f;
            float s = 0.55f / Mathf.Max(0.1f, size);
            m.transform.localScale = Vector3.one * s;
            m.transform.localPosition = new Vector3(-b.center.x * s, -(b.min.y) * s + 0.1f, -b.center.z * s);
            return go;
        }

        // ------------------------------------------------------------------ beacon beam
        static GameObject CreateBeam(Int3 pos, World w)
        {
            EnsureBeam();
            var go = new GameObject("Beacon_" + pos);
            go.transform.position = new Vector3(pos.x + 0.5f, pos.y + 1f, pos.z + 0.5f);
            float h = Mathf.Max(8f, w.maxY + 64f - (pos.y + 1));
            var core = new GameObject("core");
            core.transform.SetParent(go.transform, false);
            core.transform.localScale = new Vector3(0.36f, h, 0.36f);
            core.AddComponent<MeshFilter>().sharedMesh = beamMesh;
            var mr = core.AddComponent<MeshRenderer>(); mr.sharedMaterial = beamMat; mr.shadowCastingMode = ShadowCastingMode.Off;
            var glow = new GameObject("glow");
            glow.transform.SetParent(go.transform, false);
            glow.transform.localScale = new Vector3(0.62f, h, 0.62f);
            glow.AddComponent<MeshFilter>().sharedMesh = beamMesh;
            var gr = glow.AddComponent<MeshRenderer>(); gr.sharedMaterial = glowMat; gr.shadowCastingMode = ShadowCastingMode.Off;
            return go;
        }

        static void EnsureBeam()
        {
            if (beamMesh != null) return;
            // two crossed vertical quads, one unit tall; the texture repeats up the column
            var v = new List<Vector3>(); var uv = new List<Vector2>(); var tris = new List<int>();
            for (int k = 0; k < 2; k++)
            {
                var a = k == 0 ? new Vector3(-0.5f, 0, 0) : new Vector3(0, 0, -0.5f);
                var b = -a;
                int s = v.Count;
                v.Add(a); v.Add(a + Vector3.up); v.Add(b + Vector3.up); v.Add(b);
                uv.Add(new Vector2(0, 0)); uv.Add(new Vector2(0, 64)); uv.Add(new Vector2(1, 64)); uv.Add(new Vector2(1, 0));
                tris.Add(s); tris.Add(s + 1); tris.Add(s + 2); tris.Add(s); tris.Add(s + 2); tris.Add(s + 3);
            }
            beamMesh = new Mesh { name = "beacon_beam" };
            beamMesh.SetVertices(v); beamMesh.SetUVs(0, uv); beamMesh.SetTriangles(tris, 0); beamMesh.RecalculateBounds();
            var px = new Color32[4 * 16];
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 4; x++)
                {
                    int bright = 200 + (y * 37 % 55);
                    byte a = (byte)((x == 0 || x == 3) ? 150 : 230);
                    px[y * 4 + x] = new Color32((byte)bright, (byte)bright, 255, a);
                }
            var tex = new Texture2D(4, 16, TextureFormat.RGBA32, false) { name = "beacon_beam", filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Repeat };
            tex.SetPixels32(px); tex.Apply(false, true);
            beamMat = Res.UnlitMaterial(tex, true, true, 3100, false);
            beamMat.SetColor("_Color", new Color(1f, 1f, 1f, 0.85f));
            glowMat = Res.UnlitMaterial(tex, true, true, 3101, false);
            glowMat.SetColor("_Color", new Color(0.8f, 0.9f, 1f, 0.25f));
        }
    }
}
