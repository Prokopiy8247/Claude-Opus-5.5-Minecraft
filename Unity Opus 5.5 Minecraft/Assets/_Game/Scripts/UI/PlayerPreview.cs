using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace MCR
{
    /// <summary>
    /// The little player figure in the inventory screens: a copy of the player model on its own layer, far below
    /// the world, filmed by a dedicated orthographic camera into a texture that the interface draws. The figure
    /// turns its body and head toward the mouse pointer.
    /// </summary>
    public static class PlayerPreview
    {
        public const int Layer = 30;
        static readonly Vector3 Origin = new Vector3(0f, -4000f, 0f);
        static Camera cam;
        static RenderTexture target;
        static MobVisual visual;
        static Player bound;
        static Transform head;
        static bool usedThisFrame;
        static MaterialPropertyBlock mpb;

        /// <summary>Called by the interface before drawing a frame.</summary>
        public static void BeginFrame() { usedThisFrame = false; }

        /// <summary>Called after the frame: the camera only runs while some screen actually shows the preview.</summary>
        public static void EndFrame()
        {
            if (usedThisFrame) return;
            if (cam != null && cam.enabled) cam.enabled = false;
            if (visual != null && visual.root != null && visual.root.activeSelf) visual.root.SetActive(false);
        }

        /// <summary>Draws the figure into the GUI rectangle (GUI units) and aims it at the pointer.</summary>
        public static void Draw(UiRenderer ui, Player p, float x, float y, float w, float h, float mouseX, float mouseY)
        {
            if (p == null || SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) return;
            int pw = Mathf.Clamp(Mathf.RoundToInt(w * ui.Scale), 16, 1024), ph = Mathf.Clamp(Mathf.RoundToInt(h * ui.Scale), 16, 1024);
            if (!Ensure(p, pw, ph)) return;
            usedThisFrame = true;
            cam.enabled = true;
            visual.root.SetActive(true);

            // pose first (walk cycle, held items), then pin the figure to the preview stage facing the camera
            visual.Update(1f);
            var t = visual.root.transform;
            // keep the model's own ground offset (and sneak dip) relative to the player's feet
            float lift = t.position.y - p.InterpPos(1f).y;
            t.position = Origin + Vector3.up * lift;
            float cx = x + w * 0.5f, cy = y + h * 0.28f;
            float yaw = Mathf.Atan((mouseX - cx) / 40f) * Mathf.Rad2Deg;
            float pitch = Mathf.Atan((mouseY - cy) / 40f) * Mathf.Rad2Deg;
            t.rotation = Quaternion.Euler(0f, -yaw * 0.5f, 0f);
            if (head != null) head.localRotation = Quaternion.Euler(pitch * 0.9f, -yaw * 0.5f, 0f);
            SetLayer(visual.root, Layer);
            if (mpb == null) mpb = new MaterialPropertyBlock();
            foreach (var r in visual.root.GetComponentsInChildren<Renderer>())
            {
                r.GetPropertyBlock(mpb);
                mpb.SetVector(EntityLight.EntityLightId, new Vector4(1f, 1f, 1f, 0f));
                mpb.SetVector(EntityLight.OverlayId, Vector4.zero);
                r.SetPropertyBlock(mpb);
            }
            ui.Sprite(target, x, y, w, h, 0f, 1f, 1f, 0f, new Color32(255, 255, 255, 255));
        }

        static bool Ensure(Player p, int pw, int ph)
        {
            if (bound != p || visual == null || visual.root == null)
            {
                if (visual != null) visual.Destroy();
                visual = new MobVisual(p, "player");
                bound = p;
                head = visual.root != null ? ModelRenderer.FindBone(visual.root.transform, "head") : null;
                if (visual.root == null) return false;
            }
            if (target == null || target.width != pw || target.height != ph)
            {
                if (target != null) { target.Release(); Object.Destroy(target); }
                target = new RenderTexture(pw, ph, 16, RenderTextureFormat.ARGB32) { name = "PlayerPreview", filterMode = FilterMode.Point };
                target.Create();
                if (cam != null) cam.targetTexture = target;
            }
            if (cam == null)
            {
                var go = new GameObject("PlayerPreviewCamera");
                Object.DontDestroyOnLoad(go);
                cam = go.AddComponent<Camera>();
                cam.orthographic = true;
                cam.clearFlags = CameraClearFlags.SolidColor;
                cam.backgroundColor = new Color(0f, 0f, 0f, 1f);
                cam.cullingMask = 1 << Layer;
                cam.nearClipPlane = 0.1f;
                cam.farClipPlane = 20f;
                cam.depth = -20f;
                cam.allowHDR = false;
                cam.allowMSAA = false;
                cam.targetTexture = target;
                var data = cam.GetUniversalAdditionalCameraData();
                data.renderPostProcessing = false;
                data.antialiasing = AntialiasingMode.None;
                data.renderShadows = false;
            }
            // frame the whole figure: the camera sits in front (+Z) looking back at it
            float figure = 1.9f;
            cam.orthographicSize = figure * 0.5f + 0.05f;
            cam.aspect = pw / (float)ph;
            cam.transform.SetPositionAndRotation(Origin + new Vector3(0f, figure * 0.5f, 5f), Quaternion.Euler(0f, 180f, 0f));
            return true;
        }

        static void SetLayer(GameObject go, int layer)
        {
            go.layer = layer;
            foreach (Transform c in go.transform) SetLayer(c.gameObject, layer);
        }
    }
}
