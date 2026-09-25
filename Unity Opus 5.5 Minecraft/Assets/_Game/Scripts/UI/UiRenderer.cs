using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace MCR
{
    /// <summary>
    /// Immediate-mode pixel UI: everything is submitted as textured quads in screen pixels (origin top-left)
    /// and drawn in submission order, so later panels reliably cover earlier ones (tooltips, menus, chat).
    /// Quads from one texture are merged into a single dynamic mesh to keep draw calls low.
    /// </summary>
    public sealed class UiRenderer
    {
        sealed class Batch
        {
            public Texture tex;
            public Material mat;
            public Vector4 clip;
            public readonly List<Vertex> verts = new List<Vertex>(256);
        }

        // field order follows Unity's canonical attribute order (position, colour, uv) so the declared layout is used as-is
        struct Vertex
        {
            public Vector3 pos;
            public Color32 col;
            public Vector2 uv;
        }

        public static UiRenderer Instance;

        readonly List<Batch> batches = new List<Batch>();
        readonly List<Mesh> pool = new List<Mesh>();
        readonly List<Material> mats = new List<Material>();
        public int ScreenW { get; private set; }
        public int ScreenH { get; private set; }
        public float Scale = 1f;
        Vector4 clip = new Vector4(-1e6f, -1e6f, 1e6f, 1e6f);
        readonly List<Vector4> clipStack = new List<Vector4>();
        int drawnBatches, drawnQuads;
        int usedMeshes;

        public static readonly int MainTexId = Shader.PropertyToID("_MainTex");
        public static readonly int ClipRectId = Shader.PropertyToID("_ClipRect");
        public static readonly int ClipOnId = Shader.PropertyToID("_ClipOn");
        static readonly int uiScaleId = Shader.PropertyToID("_UI_Scale");

        static readonly VertexAttributeDescriptor[] layout =
        {
            new VertexAttributeDescriptor(VertexAttribute.Position, VertexAttributeFormat.Float32, 3),
            new VertexAttributeDescriptor(VertexAttribute.Color, VertexAttributeFormat.UNorm8, 4),
            new VertexAttributeDescriptor(VertexAttribute.TexCoord0, VertexAttributeFormat.Float32, 2),
        };

        public int BatchCount => drawnBatches;
        public int QuadCount => drawnQuads;

        /// <summary>Begin a frame. <paramref name="pixelScale"/> is the GUI scale multiplier (pixels per GUI unit).</summary>
        public void Begin(int screenW, int screenH, float pixelScale)
        {
            Instance = this;
            ScreenW = screenW; ScreenH = screenH; Scale = pixelScale;
            batches.Clear();
            clip = new Vector4(-1e6f, -1e6f, 1e6f, 1e6f);
            clipStack.Clear();
            drawnBatches = 0; drawnQuads = 0; usedMeshes = 0;
            Shader.SetGlobalVector(uiScaleId, new Vector4(2f / Mathf.Max(1, screenW), 2f / Mathf.Max(1, screenH), 0, 0));
        }

        public void End()
        {
            Flush();
        }

        /// <summary>Restricts drawing to a rectangle given in GUI units (stored in screen pixels for the shader).</summary>
        public void PushClip(float x, float y, float w, float h)
        {
            clipStack.Add(clip);
            clip = new Vector4(Mathf.Max(clip.x, x * Scale), Mathf.Max(clip.y, y * Scale), Mathf.Min(clip.z, (x + w) * Scale), Mathf.Min(clip.w, (y + h) * Scale));
        }
        public void PopClip()
        {
            if (clipStack.Count == 0) return;
            clip = clipStack[clipStack.Count - 1];
            clipStack.RemoveAt(clipStack.Count - 1);
        }
        public void ResetClip() { clip = new Vector4(1e6f, 1e6f, -1e6f, -1e6f); }
        public void ClearClip() { clip = new Vector4(-1e6f, -1e6f, 1e6f, 1e6f); }

        Batch Current(Texture tex)
        {
            var last = batches.Count > 0 ? batches[batches.Count - 1] : null;
            if (last != null && last.tex == tex && last.clip == clip) return last;
            var b = new Batch { tex = tex, clip = clip };
            batches.Add(b);
            return b;
        }

        /// <summary>Draw a texture region (in texels) into a pixel rect.</summary>
        public void Sprite(Texture tex, float x, float y, float w, float h, float u0, float v0, float u1, float v1, Color32 color)
        {
            if (tex == null || w <= 0 || h <= 0) return;
            if (x * Scale > clip.z || y * Scale > clip.w || (x + w) * Scale < clip.x || (y + h) * Scale < clip.y) return;
            var b = Current(tex);
            // four corners per quad; Flush turns each group of four into the triangles 0,1,2 and 0,2,3
            Add(b, x, y, u0, v0, color);
            Add(b, x, y + h, u0, v1, color);
            Add(b, x + w, y + h, u1, v1, color);
            Add(b, x + w, y, u1, v0, color);
            drawnQuads++;
        }

        void Add(Batch b, float x, float y, float u, float v, Color32 c)
        {
            // callers work in GUI units; snap to whole screen pixels so the pixel art stays crisp
            b.verts.Add(new Vertex { pos = new Vector3(Mathf.Round(x * Scale), Mathf.Round(y * Scale), 0), uv = new Vector2(u, v), col = c });
        }

        public void Sprite(Texture tex, float x, float y, float w, float h, Vector4 uv, Color32 color) => Sprite(tex, x, y, w, h, uv.x, uv.y, uv.z, uv.w, color);

        /// <summary>Named atlas sprite at its native size times <paramref name="scale"/>.</summary>
        public void Icon(string name, float x, float y, Color32 color, float scale = 1f)
        {
            var r = UiAtlas.Region(name);
            Sprite(UiAtlas.Tex, x, y, r.width * scale, r.height * scale, UiAtlas.UV(r), color);
        }
        public void Icon(string name, float x, float y) => Icon(name, x, y, new Color32(255, 255, 255, 255));

        /// <summary>Part of a named atlas sprite (progress bars, half-filled arrows).</summary>
        public void IconPart(string name, float x, float y, int sx, int sy, int sw, int sh, Color32 color, float scale = 1f)
        {
            if (sw <= 0 || sh <= 0) return;
            Sprite(UiAtlas.Tex, x + sx * scale, y + sy * scale, sw * scale, sh * scale, UiAtlas.UVSub(name, sx, sy, sw, sh), color);
        }

        /// <summary>Solid rectangle (samples the atlas' white texel, so it batches with text and icons).</summary>
        public void Rect(float x, float y, float w, float h, Color32 color) => Sprite(UiAtlas.Tex, x, y, w, h, UiAtlas.WhiteUV, color);

        public void Outline(float x, float y, float w, float h, Color32 color, float t = 1f)
        {
            Rect(x, y, w, t, color); Rect(x, y + h - t, w, t, color);
            Rect(x, y + t, t, h - 2 * t, color); Rect(x + w - t, y + t, t, h - 2 * t, color);
        }

        /// <summary>A 1-pixel-wide rectangle outline scaled to pixels, so panels look crisp.</summary>
        public void Panel(float x, float y, float w, float h, Color32 fill, Color32 border) => Panel(x, y, w, h, fill, border, 1f);

        public void Panel(float x, float y, float w, float h, Color32 fill, Color32 border, float thickness)
        {
            Rect(x, y, w, h, fill);
            Outline(x, y, w, h, border, thickness);
        }

        // ------------------------------------------------------------------ text
        public void Text(string s, float x, float y, Color32 color, int shadow = 1, bool center = false, float scale = 1f)
        {
            if (string.IsNullOrEmpty(s)) return;
            if (center) x -= FontData.Width(s) * scale * 0.5f;
            var col = color;
            float cx = x, cy = y;
            for (int i = 0; i < s.Length; i++)
            {
                char c = s[i];
                if (c == '§' && i + 1 < s.Length) { col = Styles.ColorCode(s[i + 1], color); i++; continue; }
                if (c == '\n') { cx = x; cy += FontData.LineHeight * scale; continue; }
                if (c == ' ') { cx += FontAdvance(c, scale); continue; }
                var (left, width) = FontData.Metrics(c);
                var quad = UiAtlas.GlyphUV(c);
                float gw = width * scale, gh = FontData.GlyphH * scale;
                if (shadow > 0)
                    Sprite(UiAtlas.Tex, cx + shadow * scale, cy + shadow * scale, gw, gh, quad, ShadowOf(col));
                Sprite(UiAtlas.Tex, cx, cy, gw, gh, quad, col);
                cx += FontAdvance(c, scale);
            }
        }

        static float FontAdvance(char c, float scale) => FontData.CharAdvance(c) * scale;

        /// <summary>The drop shadow is the text colour at a quarter brightness, as in the original interface.</summary>
        static Color32 ShadowOf(Color32 c) => new Color32((byte)(c.r / 4), (byte)(c.g / 4), (byte)(c.b / 4), c.a);

        public float TextWidth(string s, float scale = 1f) => FontData.Width(s) * scale + 1;

        /// <summary>Drops the vertical distance a block of text occupies.</summary>
        public static float TextHeight(string s, float scale = 1f) => FontData.LineCount(s) * FontData.LineHeight * scale;

        /// <summary>Wraps text to a pixel width, honouring explicit newlines.</summary>
        public static List<string> Wrap(string s, float maxWidth, float scale = 1f)
        {
            var outp = new List<string>();
            if (string.IsNullOrEmpty(s)) { outp.Add(""); return outp; }
            foreach (var hard in s.Split('\n'))
            {
                string line = "";
                foreach (var word in hard.Split(' '))
                {
                    string cand = line.Length == 0 ? word : line + " " + word;
                    if (FontData.Width(cand) * scale > maxWidth && line.Length > 0) { outp.Add(line); line = word; }
                    else line = cand;
                }
                outp.Add(line);
            }
            return outp;
        }

        // ------------------------------------------------------------------ flush / draw
        struct Prepared { public Mesh mesh; public Material mat; public Vector4 clip; public bool clipped; }
        readonly List<Prepared> prepared = new List<Prepared>();
        static readonly Vector4 NoClip = new Vector4(-1e6f, -1e6f, 1e6f, 1e6f);
        static byte[] toLinear;

        /// <summary>Builds the meshes for everything submitted this frame; <see cref="Draw"/> puts them on screen.</summary>
        void Flush()
        {
            prepared.Clear();
            bool linear = QualitySettings.activeColorSpace == ColorSpace.Linear;
            if (linear && toLinear == null)
            {
                toLinear = new byte[256];
                for (int i = 0; i < 256; i++) toLinear[i] = (byte)Mathf.RoundToInt(Mathf.GammaToLinearSpace(i / 255f) * 255f);
            }
            foreach (var b in batches)
            {
                if (b.verts.Count == 0) continue;
                if (linear)
                    for (int i = 0; i < b.verts.Count; i++)
                    {
                        var v = b.verts[i];
                        // only the colour channels ride the sRGB curve; alpha is coverage and must stay linear
                        v.col = new Color32(toLinear[v.col.r], toLinear[v.col.g], toLinear[v.col.b], v.col.a);
                        b.verts[i] = v;
                    }
                var mesh = GetMesh(usedMeshes++);
                mesh.Clear(false);
                mesh.SetVertexBufferParams(b.verts.Count, layout);
                mesh.SetVertexBufferData(b.verts, 0, 0, b.verts.Count, 0, MeshUpdateFlags.DontValidateIndices | MeshUpdateFlags.DontRecalculateBounds | MeshUpdateFlags.DontNotifyMeshUsers);
                int quads = b.verts.Count / 4;
                var idx = new int[quads * 6];
                for (int q = 0; q < quads; q++)
                {
                    int v = q * 4, o = q * 6;
                    idx[o] = v; idx[o + 1] = v + 1; idx[o + 2] = v + 2;
                    idx[o + 3] = v; idx[o + 4] = v + 2; idx[o + 5] = v + 3;
                }
                mesh.SetIndexBufferParams(idx.Length, IndexFormat.UInt32);
                mesh.SetIndexBufferData(idx, 0, 0, idx.Length, MeshUpdateFlags.DontValidateIndices | MeshUpdateFlags.DontRecalculateBounds | MeshUpdateFlags.DontNotifyMeshUsers);
                mesh.subMeshCount = 1;
                mesh.SetSubMesh(0, new SubMeshDescriptor(0, idx.Length), MeshUpdateFlags.DontRecalculateBounds | MeshUpdateFlags.DontNotifyMeshUsers);
                mesh.bounds = new Bounds(new Vector3(ScreenW * 0.5f, ScreenH * 0.5f, 0), new Vector3(ScreenW + 8, ScreenH + 8, 1));
                prepared.Add(new Prepared { mesh = mesh, mat = GetMaterial(b.tex), clip = b.clip, clipped = b.clip != NoClip });
                drawnBatches++;
            }
            batches.Clear();
        }

        // ------------------------------------------------------------------ presentation
        // The interface is rendered into an off-screen texture every frame and shown by a screen-space overlay
        // canvas. The render pipeline always composites overlay canvases after the cameras (and screenshots
        // include them), which immediate-mode drawing from OnGUI cannot guarantee.
        RenderTexture target;
        CommandBuffer cmd;
        MaterialPropertyBlock mpb;
        GameObject canvasGo;
        UnityEngine.UI.RawImage image;
        static Shader compositeShader;

        public RenderTexture Target => target;

        /// <summary>Draws the prepared meshes into the interface texture and makes sure the overlay shows it.</summary>
        public void Present()
        {
            if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Null) return;
            int w = Mathf.Max(1, ScreenW), h = Mathf.Max(1, ScreenH);
            if (target == null || target.width != w || target.height != h)
            {
                if (target != null) { target.Release(); Object.Destroy(target); }
                target = new RenderTexture(w, h, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB)
                { name = "InterfaceTarget", filterMode = FilterMode.Point, useMipMap = false, autoGenerateMips = false };
                target.Create();
                if (image != null) image.texture = target;
            }
            EnsureOverlay();
            if (cmd == null) cmd = new CommandBuffer { name = "Interface" };
            if (mpb == null) mpb = new MaterialPropertyBlock();
            cmd.Clear();
            cmd.SetRenderTarget(target);
            cmd.ClearRenderTarget(false, true, Color.clear);
            // off-screen targets on top-left-origin APIs need the rows flipped to match texture coordinates
            float ySign = SystemInfo.graphicsUVStartsAtTop ? -1f : 1f;
            cmd.SetGlobalVector(uiScaleId, new Vector4(2f / w, 2f / h, ySign, 0));
            foreach (var p in prepared)
            {
                mpb.Clear();
                mpb.SetVector(ClipRectId, p.clipped ? p.clip : Vector4.zero);
                mpb.SetFloat(ClipOnId, p.clipped ? 1f : 0f);
                cmd.DrawMesh(p.mesh, Matrix4x4.identity, p.mat, 0, 0, mpb);
            }
            Graphics.ExecuteCommandBuffer(cmd);
        }

        void EnsureOverlay()
        {
            if (canvasGo != null) return;
            canvasGo = new GameObject("Interface Canvas");
            Object.DontDestroyOnLoad(canvasGo);
            var canvas = canvasGo.AddComponent<Canvas>();
            canvas.renderMode = RenderMode.ScreenSpaceOverlay;
            canvas.sortingOrder = 30000;
            canvas.pixelPerfect = false;
            var imgGo = new GameObject("Interface", typeof(RectTransform));
            imgGo.transform.SetParent(canvasGo.transform, false);
            var rt = (RectTransform)imgGo.transform;
            rt.anchorMin = Vector2.zero; rt.anchorMax = Vector2.one;
            rt.offsetMin = Vector2.zero; rt.offsetMax = Vector2.zero;
            image = imgGo.AddComponent<UnityEngine.UI.RawImage>();
            image.raycastTarget = false;
            image.texture = target;
            if (compositeShader == null) compositeShader = Shader.Find("MCR/UIComposite");
            if (compositeShader != null) image.material = new Material(compositeShader) { name = "InterfaceComposite" };
            else Debug.LogWarning("[UI] MCR/UIComposite shader missing; the interface uses the default canvas material");
        }

        Mesh GetMesh(int i)
        {
            while (pool.Count <= i) pool.Add(new Mesh { name = "ui_" + pool.Count });
            var m = pool[i];
            m.MarkDynamic();
            return m;
        }

        static Shader uiShader;
        Material GetMaterial(Texture tex)
        {
            foreach (var m in mats) if (m.GetTexture(MainTexId) == tex) return m;
            if (uiShader == null) uiShader = Shader.Find("MCR/UI");
            var mat = new Material(uiShader) { name = "UI_" + (tex != null ? tex.name : "null") };
            mat.SetTexture(MainTexId, tex);
            mat.SetVector(ClipRectId, Vector4.zero);
            mat.SetFloat(ClipOnId, 0);
            mat.renderQueue = (int)RenderQueue.Overlay;
            mats.Add(mat);
            return mat;
        }
    }
}
