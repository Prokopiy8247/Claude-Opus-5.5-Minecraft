using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace MCR
{
    /// <summary>
    /// Camera rig: first person with view bobbing and hurt tilt, third person back/front with wall clipping,
    /// field-of-view effects (sprint, flight, bow draw, spyglass), plus the per-frame world render pass
    /// (entities, particles, block outline and cracks, first-person hand).
    /// </summary>
    public sealed partial class GameManager
    {
        public const int FirstPersonLayer = 31;
        Camera cam, handCam;
        float fovCurrent = 70f;
        float eyeHeightSmooth = 1.62f;
        readonly Mesh[] crackMeshes = new Mesh[10];
        Material crackMat;
        MaterialPropertyBlock crackMpb;
        World lastRenderedWorld;
        public float lastPartial;
        readonly HashSet<string> renderFailures = new HashSet<string>();
        int panoramaFace = -1;

        /// <summary>
        /// Renders the six square panorama views used by the title screen into PNG files (panorama_0..5.png):
        /// +Z, +X, -Z, -X, up and down, with the interface and the hand hidden.
        /// </summary>
        public System.Collections.IEnumerator CapturePanorama(string dir, int size)
        {
            EnsureCamera();
            System.IO.Directory.CreateDirectory(dir);
            var rt = new RenderTexture(size, size, 24, RenderTextureFormat.ARGB32);
            var tex = new Texture2D(size, size, TextureFormat.RGB24, false);
            bool hid = hud.HideGui;
            hud.HideGui = true;
            bool hand = handCam != null && handCam.enabled;
            if (handCam != null) handCam.enabled = false;
            var oldTarget = cam.targetTexture;
            for (int f = 0; f < 6; f++)
            {
                panoramaFace = f;
                cam.targetTexture = rt;
                // a few frames so the sky, fog and chunk visibility settle for this direction
                for (int i = 0; i < 4; i++) yield return new WaitForEndOfFrame();
                var prev = RenderTexture.active;
                RenderTexture.active = rt;
                tex.ReadPixels(new Rect(0, 0, size, size), 0, 0);
                tex.Apply(false);
                RenderTexture.active = prev;
                System.IO.File.WriteAllBytes(System.IO.Path.Combine(dir, "panorama_" + f + ".png"), tex.EncodeToPNG());
            }
            panoramaFace = -1;
            cam.targetTexture = oldTarget;
            if (handCam != null) handCam.enabled = hand;
            hud.HideGui = hid;
            rt.Release();
            Destroy(rt);
            Destroy(tex);
            Debug.Log("[Panorama] wrote 6 faces to " + dir);
        }

        public Camera Cam { get { EnsureCamera(); return cam; } }

        void EnsureCamera()
        {
            if (cam != null) return;
            var existing = Camera.main;
            if (existing != null) cam = existing;
            else
            {
                var go = new GameObject("Main Camera") { tag = "MainCamera" };
                cam = go.AddComponent<Camera>();
                go.AddComponent<AudioListener>();
            }
            DontDestroyOnLoad(cam.gameObject);
            cam.nearClipPlane = 0.05f;
            cam.farClipPlane = 1024f;
            cam.clearFlags = CameraClearFlags.SolidColor;
            cam.backgroundColor = new Color(0.47f, 0.65f, 1f);
            cam.cullingMask = ~((1 << FirstPersonLayer) | (1 << PlayerPreview.Layer));
            cam.allowHDR = false;
            cam.allowMSAA = false;
            var data = cam.GetUniversalAdditionalCameraData();
            data.renderPostProcessing = false;
            data.antialiasing = AntialiasingMode.None;
            data.renderShadows = false;

            // the held item/arm camera renders on top with its own depth so the hand never clips into walls
            var hgo = new GameObject("HandCamera");
            hgo.transform.SetParent(cam.transform, false);
            handCam = hgo.AddComponent<Camera>();
            handCam.cullingMask = 1 << FirstPersonLayer;
            handCam.nearClipPlane = 0.01f;
            handCam.farClipPlane = 10f;
            handCam.fieldOfView = 70f;
            handCam.clearFlags = CameraClearFlags.Depth;
            var hdata = handCam.GetUniversalAdditionalCameraData();
            hdata.renderType = CameraRenderType.Overlay;
            hdata.renderShadows = false;
            MainCamera = cam;
            // the stack lives on the renderer; if the pipeline cannot build one (headless runs) keep going without it
            try { data.cameraStack.Add(handCam); }
            catch (System.Exception e) { Debug.LogWarning("[Camera] overlay stack unavailable: " + e.Message); handCam.enabled = false; }
        }

        // ------------------------------------------------------------------ per frame
        void UpdateCamera(float dt)
        {
            EnsureCamera();
            MainCamera = cam;
            var p = player;
            var w = ActiveWorld;
            if (session == null || p == null || w == null)
            {
                // title screen: the camera turns slowly inside the captured panorama
                cam.backgroundColor = new Color(0.18f, 0.22f, 0.35f);
                if (TitleArt.HasPanorama)
                {
                    cam.fieldOfView = 85f;
                    cam.transform.SetPositionAndRotation(Vector3.zero, Quaternion.Euler(8f + Mathf.Sin(Time.unscaledTime * 0.05f) * 4f, Time.unscaledTime * 2.2f, 0f));
                    TitleArt.RenderPanorama(cam);
                }
                ListenerPosition = cam.transform.position;
                return;
            }
            float partial = paused || hud.HasScreen<LoadingScreen>() ? 1f : Mathf.Clamp01(tickAccum / TickStep);
            lastPartial = partial;
            if (lastRenderedWorld != w) { SwitchVisibleWorld(lastRenderedWorld, w); lastRenderedWorld = w; }

            // ---- position and orientation
            float targetEye = p.EyeHeight;
            eyeHeightSmooth = Mathf.Lerp(eyeHeightSmooth, targetEye, 1f - Mathf.Exp(-dt * 18f));
            Vector3 feet = p.vehicle != null ? p.InterpPos(partial) : p.InterpPos(partial);
            Vector3 eye = feet + Vector3.up * eyeHeightSmooth;
            float yaw = p.yaw, pitch = p.pitch;
            var rot = Quaternion.Euler(pitch, yaw, 0f);

            if (p.cameraMode == 0 && !p.sleeping)
            {
                // view bobbing: a gentle figure-eight synchronised with the walk cycle
                float walk = Mathf.Lerp(p.prevWalkDist, p.walkDist, partial);
                float bob = Mathf.Lerp(p.prevBob, p.bob, partial);
                if (bob > 0.001f)
                {
                    float phase = walk * Mathf.PI;
                    rot *= Quaternion.Euler(Mathf.Abs(Mathf.Cos(phase - 0.2f) * bob) * 5f, 0f, Mathf.Sin(phase) * bob * 3f);
                    eye += rot * new Vector3(Mathf.Sin(phase) * bob * 0.5f, -Mathf.Abs(Mathf.Cos(phase) * bob), 0f);
                }
                // hurt shake: a brief roll in the direction of the hit
                if (p.hurtTime > 0)
                {
                    float h = (p.hurtTime - partial) / p.hurtDuration;
                    rot *= Quaternion.Euler(0f, 0f, Mathf.Sin(h * h * h * h * Mathf.PI) * 14f);
                }
                if (p.dead) rot *= Quaternion.Euler(0f, 0f, Mathf.Min(40f, (p.deathTime + partial) * 4f));
            }
            else if (p.cameraMode != 0)
            {
                bool front = p.cameraMode == 2;
                if (front) rot = Quaternion.Euler(-pitch, yaw + 180f, 0f);
                Vector3 back = rot * Vector3.back;
                float dist = 4f;
                // pull the camera in if a wall is in the way
                if (w.RaycastBlocks(eye, back, dist, false, out var hit)) dist = Mathf.Max(0.3f, hit.distance - 0.2f);
                eye += back * dist;
            }
            else if (p.sleeping) { eye = feet + Vector3.up * 0.3f; rot = Quaternion.Euler(-10f, yaw, 0f); }

            if (panoramaFace >= 0)
            {
                // panorama capture: square 90 degree views along the six axes from the eye position
                Quaternion[] faces = { Quaternion.Euler(0, 0, 0), Quaternion.Euler(0, 90, 0), Quaternion.Euler(0, 180, 0), Quaternion.Euler(0, 270, 0), Quaternion.Euler(-90, 0, 0), Quaternion.Euler(90, 0, 0) };
                rot = faces[panoramaFace];
            }
            cam.transform.SetPositionAndRotation(eye, rot);
            ListenerPosition = eye;

            // ---- field of view
            float fovTarget = fov;
            if (p.sprinting) fovTarget *= 1.15f;
            if (p.abilities.flying) fovTarget *= 1.1f;
            if (p.IsUsingItem && p.usingStack != null)
            {
                var id = p.usingStack.item.id;
                if (id == "bow") fovTarget *= 1f - Mathf.Min(1f, p.useTicks / 20f) * 0.15f;
                if (id == "spyglass") fovTarget = 10f;
            }
            if (p.eyeInWater) fovTarget *= 0.86f;
            fovCurrent = Mathf.Lerp(fovCurrent, fovTarget, 1f - Mathf.Exp(-dt * 12f));
            cam.fieldOfView = panoramaFace >= 0 ? 90f : fovCurrent;
            cam.farClipPlane = Mathf.Max(256f, renderDistance * 16f * 1.8f);

            RenderSky(cam, partial);
            // the clear colour is specified in sRGB while the fog colour is linear
            cam.backgroundColor = WorldLighting.fogColor.gamma;

            // ---- entities and particles
            foreach (var e in w.entities)
            {
                if (e.removed) continue;
                if (e == p) continue;
                try { e.Render(partial); }
                catch (System.Exception ex) { if (renderFailures.Add(e.TypeId)) Debug.LogWarning("[Render] " + e.TypeId + ": " + ex); }
            }
            PlayerVisual.Update(p, partial);
            BlockEntityRenderer.Update(w, cam.transform.position, partial);
            Particles.Render(cam, partial);

            // ---- crosshair target outline and crack overlay
            var inter = interaction;
            if (inter != null && !hud.HideGui)
            {
                inter.UpdateTarget();
                if (inter.hasBlockTarget && !p.IsSpectator)
                {
                    var hit = inter.blockTarget;
                    var b = Blocks.ByState[hit.state];
                    var boxes = new List<AABB>();
                    b.GetSelectionBoxes(hit.state - b.baseState, w, hit.pos, boxes);
                    foreach (var box in boxes)
                    {
                        var wb = new AABB(box.min + hit.pos.ToVector3(), box.max + hit.pos.ToVector3());
                        SkyRenderer.DrawBox(cam, wb, new Color(0f, 0f, 0f, 0.55f));
                    }
                }
                if (inter.isMining && inter.destroyStage >= 0) DrawCrack(inter.miningPos, inter.destroyStage);
            }
            if (showHitboxes) DrawHitboxes(w);
            if (showChunkBorders) DrawChunkBorders(p);
        }

        void DrawHitboxes(World w)
        {
            foreach (var e in w.entities)
            {
                if (e.removed || e == player) continue;
                SkyRenderer.DrawBox(cam, e.Bounds, new Color(1f, 1f, 1f, 0.9f));
            }
        }

        void DrawChunkBorders(Player p)
        {
            int cx = Mathf.FloorToInt(p.position.x) >> 4, cz = Mathf.FloorToInt(p.position.z) >> 4;
            var w = ActiveWorld;
            for (int dx = -1; dx <= 1; dx++)
                for (int dz = -1; dz <= 1; dz++)
                {
                    var min = new Vector3((cx + dx) * 16f, w.minY, (cz + dz) * 16f);
                    var col = dx == 0 && dz == 0 ? new Color(1f, 1f, 0f, 0.9f) : new Color(1f, 0.2f, 0.2f, 0.6f);
                    SkyRenderer.DrawBox(cam, new AABB(min, min + new Vector3(16f, w.height, 16f)), col);
                }
        }

        void DrawCrack(Int3 pos, int stage)
        {
            if (crackMat == null)
            {
                crackMat = new Material(Res.ChunkTranslucent) { name = "CrackOverlay" };
                crackMat.renderQueue = (int)RenderQueue.Transparent + 5;
                crackMpb = new MaterialPropertyBlock();
            }
            if (crackMeshes[stage] == null) crackMeshes[stage] = BuildCrackMesh(ParticleTextures.Layer("destroy_" + stage));
            var l = EntityLight.Sample(ActiveWorld, pos.Center);
            crackMpb.SetVector(EntityLight.EntityLightId, new Vector4(Mathf.Max(l.x, 0.4f), l.y, 1f, 0f));
            Graphics.DrawMesh(crackMeshes[stage], Matrix4x4.TRS(pos.ToVector3() + new Vector3(0.5f, 0.5f, 0.5f), Quaternion.identity, Vector3.one * 1.004f), crackMat, 0, cam, 0, crackMpb);
        }

        static Mesh BuildCrackMesh(int layer)
        {
            var verts = new List<ChunkVertex>();
            var idx = new List<int>();
            ushort L = HalfConv.ToHalf(layer), one = HalfConv.ToHalf(1), h0 = HalfConv.ToHalf(0), h1 = HalfConv.ToHalf(1);
            for (int f = 0; f < 6; f++)
            {
                int s = verts.Count;
                for (int k = 0; k < 4; k++)
                {
                    float x = MeshCtx.FaceVerts[f, k, 0] - 0.5f, y = MeshCtx.FaceVerts[f, k, 1] - 0.5f, z = MeshCtx.FaceVerts[f, k, 2] - 0.5f;
                    ushort u = (k == 0 || k == 1) ? h0 : h1;
                    ushort v = (k == 1 || k == 2) ? h1 : h0;
                    verts.Add(new ChunkVertex { x = x, y = y, z = z, color = 0xFFFFFFFF, u = u, v = v, layer = L, anim = one, light = 0xFFFF });
                }
                idx.Add(s); idx.Add(s + 1); idx.Add(s + 2); idx.Add(s); idx.Add(s + 2); idx.Add(s + 3);
            }
            var m = new Mesh { name = "crack_" + layer };
            m.SetVertexBufferParams(verts.Count, ChunkVertex.Layout);
            m.SetVertexBufferData(verts, 0, 0, verts.Count);
            m.SetIndexBufferParams(idx.Count, IndexFormat.UInt32);
            m.SetIndexBufferData(idx, 0, 0, idx.Count);
            m.subMeshCount = 1;
            m.SetSubMesh(0, new SubMeshDescriptor(0, idx.Count), MeshUpdateFlags.DontRecalculateBounds);
            m.bounds = new Bounds(Vector3.zero, Vector3.one * 1.1f);
            return m;
        }

        /// <summary>Shows only the current dimension's chunks and entity visuals.</summary>
        void SwitchVisibleWorld(World from, World to)
        {
            foreach (var kv in chunkManagers)
            {
                bool vis = kv.Key == to.dim;
                if (kv.Value.root != null) kv.Value.root.gameObject.SetActive(vis);
            }
            if (from != null)
                foreach (var e in from.entities) if (e.go != null) e.go.SetActive(false);
            foreach (var e in to.entities) if (e.go != null) e.go.SetActive(true);
        }
    }
}
