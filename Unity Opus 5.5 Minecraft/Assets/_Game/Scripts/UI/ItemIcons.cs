using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Item and block icons for the interface. Blocks become small isometric cubes assembled from their own
    /// texture faces; flat items are scaled-up copies of their sprite layer. Everything is rasterised by hand into
    /// a dynamically packed atlas, so no icon artwork is ever shipped as an asset.
    /// </summary>
    public static class ItemIcons
    {
        public const int IconSize = 32;
        const int Pad = 1;

        public static Texture2D Atlas { get; private set; }
        const int AtlasSize = 2048;
        static bool dirty;

        sealed class Cell { public int x, y; public int w, h; public RectInt[] faces; public int border; }
        static readonly Dictionary<string, Cell> cells = new Dictionary<string, Cell>();
        static readonly Dictionary<string, Vector4> effectUVs = new Dictionary<string, Vector4>();
        static Color32[] px;
        static int shelfX, shelfY, shelfH;
        static int drawnIcons;

        public static void Ensure()
        {
            if (Atlas != null) return;
            px = new Color32[AtlasSize * AtlasSize];
            shelfX = shelfY = shelfH = 0;
            BuildEffectSprites();
            Upload();
        }

        static void Upload()
        {
            if (Atlas == null)
                Atlas = new Texture2D(AtlasSize, AtlasSize, TextureFormat.RGBA32, false) { name = "MCR_ItemIcons", filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp };
            Atlas.SetPixelData(px, 0);
            Atlas.Apply(false, false);
            dirty = false;
        }

        /// <summary>Uploads icons rasterised this frame in one go (call once per frame before the UI draws).</summary>
        public static void FlushIfDirty() { if (dirty && Atlas != null) Upload(); }

        /// <summary>Atlas pixels are kept bottom-up (Unity's layout) so uploads need no flip.</summary>
        static void Put(int ax, int ay, Color32 c)
        {
            if (ax < 0 || ay < 0 || ax >= AtlasSize || ay >= AtlasSize) return;
            px[(AtlasSize - 1 - ay) * AtlasSize + ax] = c;
        }

        static RectInt Alloc(int w, int h)
        {
            if (shelfX + w + Pad > AtlasSize) { shelfX = 0; shelfY += shelfH + Pad; shelfH = 0; }
            if (shelfY + h + Pad > AtlasSize) return new RectInt(-1, -1, w, h); // atlas full
            var r = new RectInt(shelfX, shelfY, w, h);
            shelfX += w + Pad;
            shelfH = Mathf.Max(shelfH, h);
            return r;
        }

        // ------------------------------------------------------------------ UVs
        public static Vector4 UV(string key)
        {
            Ensure();
            if (!cells.TryGetValue(key, out var cell))
            {
                cell = BuildCell(key);
                if (cell == null) return new Vector4(0, 0, 1f / AtlasSize, 1f / AtlasSize);
                cells[key] = cell;
                dirty = true;
                drawnIcons++;
            }
            // (u0, vTop, u1, vBottom): UiRenderer maps v0 to the top edge of the quad
            return new Vector4(cell.x / (float)AtlasSize, 1f - cell.y / (float)AtlasSize, (cell.x + cell.w) / (float)AtlasSize, 1f - (cell.y + cell.h) / (float)AtlasSize);
        }

        public static Vector4 EffectUV(Effect e)
        {
            Ensure();
            return effectUVs.TryGetValue(e.id, out var uv) ? uv : effectUVs[""];
        }

        // ------------------------------------------------------------------ drawing into the UI
        /// <summary>Draws an item stack's icon inside a 16-unit slot at (x, y).</summary>
        public static void Draw(UiRenderer ui, ItemStack s, float x, float y, float size)
        {
            if (s == null || s.IsEmpty) return;
            string key = Key(s.item, s);
            var uv = UV(key);
            if (!cells.TryGetValue(key, out var cell)) return;
            float scale = cell.w <= 16 ? 1f : cell.w <= 32 ? 0.5f : 0.25f;
            ui.Sprite(Atlas, x, y, cell.w * scale, cell.h * scale, uv, Tint(s));
        }

        static Color32 Tint(ItemStack s)
        {
            var it = s.item;
            if (it is SpawnEggItem) return new Color32(255, 255, 255, 255);
            return new Color32(255, 255, 255, 255);
        }

        static string Key(Item it, ItemStack s)
        {
            if (it.block != null && !ItemSprites.NeedsFlatSprite(it))
            {
                if (ItemRender.IsFlatBlock(it.block)) return "l:" + it.block.particleTex;
                return "b:" + it.block.DefaultState;
            }
            string n = ItemSprites.SpriteName(it);
            return "i:" + (n ?? it.id);
        }

        /// <summary>Atlas UVs for an item's icon (tabs, tooltips and the recipe view use this directly).</summary>
        public static Vector4 UVFor(Item it) => UV(Key(it, null));

        // ------------------------------------------------------------------ cell builders
        static Cell BuildCell(string key)
        {
            if (key.StartsWith("i:")) return BuildFlat(Res.GetLayerPixels(Tex.Id("item/" + key.Substring(2))));
            if (key.StartsWith("l:") && int.TryParse(key.Substring(2), out int layer)) return BuildFlat(Res.GetLayerPixels(layer));
            if (key.StartsWith("b:")) return BuildBlock(key);
            return null;
        }

        static Cell BuildFlat(Color32[] src)
        {
            if (src == null) return null;
            var r = Alloc(IconSize, IconSize);
            if (r.x < 0) return null;
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                {
                    var c = src[y * 16 + x];
                    if (c.a == 0) continue;
                    // 2x with a soft edge so icons read cleanly at GUI scale
                    for (int dy = 0; dy < 2; dy++)
                        for (int dx = 0; dx < 2; dx++)
                            Put(r.x + x * 2 + dx, r.y + y * 2 + dy, c);
                }
            return new Cell { x = r.x, y = r.y, w = IconSize, h = IconSize };
        }

        /// <summary>
        /// Isometric cube icon. The 32x32 cell is read as an orthographic projection of a cube: the top face is
        /// the rhombus above the waist line, the two visible sides are the parallelograms below it. Each face
        /// samples its own block texture, so a furnace icon shows the lit front and a log icon shows bark on the
        /// sides, exactly as the block would be seen in the world.
        /// </summary>
        static Cell BuildBlock(string key)
        {
            int colon = key.IndexOf(':', 2);
            string stateText = colon < 0 ? key.Substring(2) : key.Substring(2, colon - 2);
            if (!ushort.TryParse(stateText, out var state)) return null;
            var block = Blocks.ByState[state];
            if (block.isAir) return null;
            var texUp = Res.GetLayerPixels(block.faceTex[1]);
            var texNorth = Res.GetLayerPixels(block.faceTex[2]);   // +Z face
            var texEast = Res.GetLayerPixels(block.faceTex[5]);    // +X face
            if (texUp == null || texNorth == null || texEast == null) return null;
            var r = Alloc(IconSize, IconSize);
            if (r.x < 0) return null;

            // cube corners in icon pixel space: the cube spans 32 wide and 32 tall
            // top rhombus: (16,0) (32,8) (16,16) (0,8); front-bottom edge at y=24, so the silhouette is 32x32
            const float S = 32f;
            float hw = S * 0.5f;        // half width = 16
            float quarter = S * 0.25f;  // 8: vertical offset of the rhombus corners
            int size = IconSize;
            for (int j = 0; j < size; j++)
                for (int i = 0; i < size; i++)
                {
                    float x = i + 0.5f, y = j + 0.5f;
                    Color32 c = default;
                    // rhombus test for the top face, in a coordinate frame centred on the cube
                    float dx = x - hw, dy = y - quarter;
                    if (Mathf.Abs(dx) / hw + Mathf.Abs(dy) / quarter <= 1f)
                    {
                        // inverse of the rhombus projection: u runs along +X, v along +Z
                        float u = (dx / hw + dy / quarter) * 0.5f;
                        float v = (dy / quarter - dx / hw) * 0.5f;
                        c = Shade(SampleFace(texUp, u, v), 1.06f);
                    }
                    else
                    {
                        // lower body: left half = north (+Z) face, right half = east (+X) face
                        float top = quarter + (x < hw ? (hw - x) * 0.5f : (x - hw) * 0.5f);
                        if (y < top) continue;
                        float h = 2f * (y - top) / S;      // 0 at the top edge, 1 at the bottom of the face
                        if (h > 1f) continue;
                        float t = x < hw ? (hw - x) / hw : (x - hw) / hw; // 0 at the centre seam, 1 at the outer edge
                        float u = 1f - t;
                        Color32 face = x < hw ? SampleFace(texNorth, u, h) : SampleFace(texEast, u, h);
                        c = Shade(face, x < hw ? 0.80f : 0.62f);
                    }
                    if (c.a == 0) continue;
                    // thin dark outline keeps icons legible against the grey slots
                    if (i == 0 || j == 0 || i == size - 1 || j == size - 1) c = new Color32((byte)(c.r / 3), (byte)(c.g / 3), (byte)(c.b / 3), c.a);
                    Put(r.x + i, r.y + j, c);
                }
            return new Cell { x = r.x, y = r.y, w = size, h = size };
        }

        static Color32 Shade(Color32 c, float f) => new Color32((byte)Mathf.Clamp(c.r * f, 0, 255), (byte)Mathf.Clamp(c.g * f, 0, 255), (byte)Mathf.Clamp(c.b * f, 0, 255), c.a);

        /// <summary>Nearest-texel sample of a 16x16 block texture with normalised face coordinates.</summary>
        static Color32 SampleFace(Color32[] tex, float u, float v)
        {
            int tx = Mathf.Clamp((int)(u * 16f), 0, 15);
            int ty = Mathf.Clamp((int)(v * 16f), 0, 15);
            return tex[ty * 16 + tx];
        }

        static Color32 Sample(Color32[] tex, float x, float y)
        {
            int tx = Mathf.Clamp(Mathf.FloorToInt(x), 0, 15);
            int ty = Mathf.Clamp(Mathf.FloorToInt(y), 0, 15);
            return tex[ty * 16 + tx];
        }

        // ------------------------------------------------------------------ effect icons
        static readonly string[] effectShapes =
        {
            "....ooo...", "..oo...oo.", ".o.......o", ".o.......o", ".o.......o", "..oo...oo.", "....ooo...",  // round
        };

        static void BuildEffectSprites()
        {
            // one 22x22 icon per effect, drawn as a blob filled with the effect colour and a couple of rays
            foreach (var e in Effect.All)
            {
                var r = Alloc(22, 22);
                if (r.x < 0) continue;
                var baseC = e.color;
                var light = Img.Shade(baseC, 1.4f);
                var dark = Img.Shade(baseC, 0.55f);
                for (int y = 0; y < 22; y++)
                    for (int x = 0; x < 22; x++)
                    {
                        float dx = x - 11.5f, dy = y - 11.5f;
                        float d = Mathf.Sqrt(dx * dx + dy * dy) / 10f;
                        if (d > 1f) continue;
                        var c = d > 0.82f ? dark : d < 0.45f ? light : baseC;
                        // a few radial spokes give each icon a distinct silhouette
                        float ang = Mathf.Atan2(dy, dx);
                        int spoke = Mathf.RoundToInt(ang / (Mathf.PI / 3f));
                        if (Mathf.Abs(d - 0.7f) < 0.08f && spoke % 2 == 0) c = Img.Shade(baseC, 1.7f);
                        Put(r.x + x, r.y + y, c);
                    }
                effectUVs[e.id] = new Vector4(r.x / (float)AtlasSize, 1f - r.y / (float)AtlasSize, (r.x + 22) / (float)AtlasSize, 1f - (r.y + 22) / (float)AtlasSize);
            }
            effectUVs[""] = effectUVs.Count > 1 ? effectUVs[Effect.All[0].id] : Vector4.zero;
        }

        public static void Clear()
        {
            if (Atlas != null) Object.Destroy(Atlas);
            Atlas = null;
            cells.Clear();
            effectUVs.Clear();
        }

        public static int CachedIcons => cells.Count;
    }
}
