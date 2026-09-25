using UnityEngine;
using UnityEngine.Rendering;

namespace MCR
{
    /// <summary>
    /// Publishes the global lighting state that the block and entity shaders read every frame: daylight level,
    /// sky and block tints, fog colour and range, and the texture animation clock. Also owns the ambient
    /// (lightmap) probe so entities and the surface shading agree with the chunks.
    /// </summary>
    public static class WorldLighting
    {
        static readonly int LightId = Shader.PropertyToID("_MC_Light");
        static readonly int SkyTintId = Shader.PropertyToID("_MC_SkyTint");
        static readonly int BlockTintId = Shader.PropertyToID("_MC_BlockTint");
        static readonly int FogColorId = Shader.PropertyToID("_MC_FogColor");
        static readonly int FogParamsId = Shader.PropertyToID("_MC_FogParams");
        static readonly int AnimId = Shader.PropertyToID("_MC_Anim");

        public static float daylight = 1f;
        public static float ambientBoost;
        /// <summary>This frame's fog colour in linear space (what the shaders blend towards and the sky's horizon).</summary>
        public static Color fogColor = new Color(0.52f, 0.69f, 1f);

        static readonly Color skyDay = new Color(1f, 1f, 1f);
        static readonly Color skyNight = new Color(0.36f, 0.42f, 0.62f);
        static readonly Color skyNether = new Color(0.62f, 0.35f, 0.30f);
        static readonly Color skyEnd = new Color(0.55f, 0.50f, 0.62f);
        static readonly Color blockWarm = new Color(1f, 0.88f, 0.72f);

        public static void Apply(GameManager gm)
        {
            ambientBoost = gm != null ? gm.brightness : 0.5f;
        }

        /// <summary>Called every frame by the sky renderer with the frame's interpolated session state.</summary>
        public static void Update(GameSession s, World w, Player p, float partial, float time) => Update(s, w, p, partial, time, null);

        /// <summary>
        /// <paramref name="horizon"/> is the sky's horizon colour (linear); in open air the fog takes exactly that
        /// colour so distant terrain dissolves into the sky without a seam.
        /// </summary>
        public static void Update(GameSession s, World w, Player p, float partial, float time, Color? horizon)
        {
            if (s == null || w == null)
            {
                Shader.SetGlobalVector(LightId, new Vector4(1, 0.05f, 0.5f, 0));
                return;
            }
            float day = s.DaylightFactor(w);
            if (!w.hasSkyLight) day = w.dim == DimensionId.Nether ? 0.42f : 0.30f;
            float rain = s.RainLevel(partial);
            float thunder = s.ThunderLevel(partial);
            // rain darkens the sky a little, a thunderstorm a lot
            day *= 1f - rain * 0.28f - thunder * 0.18f;
            daylight = Mathf.Max(day, w.hasSkyLight ? 0.2f : 0.3f);

            Color sky = w.dim == DimensionId.Nether ? skyNether : w.dim == DimensionId.End ? skyEnd : Color.Lerp(skyNight, skyDay, Mathf.Clamp01((daylight - 0.2f) / 0.8f));
            var blkTint = blockWarm;
            if (p != null && p.HasEffect(Effect.NightVision)) { sky = Color.Lerp(sky, Color.white, 0.7f); blkTint = Color.Lerp(blkTint, Color.white, 0.6f); }

            float nightVision = p != null && p.HasEffect(Effect.NightVision) ? 1f : 0f;
            Shader.SetGlobalVector(LightId, new Vector4(daylight, w.hasSkyLight ? 0.05f : 0.10f, ambientBoost, nightVision));
            Shader.SetGlobalVector(SkyTintId, new Vector4(sky.r, sky.g, sky.b, 1));
            Shader.SetGlobalVector(BlockTintId, new Vector4(blkTint.r, blkTint.g, blkTint.b, 1));

            // fog: distance picked from the render distance, colour from the environment
            float far = Mathf.Clamp((GameManager.Instance != null ? GameManager.Instance.renderDistance : 10) * 16f, 48f, 512f);
            // all fog colours are linear: they are consumed raw by the shaders
            Color fog = horizon ?? new Color(0.52f, 0.69f, 1f);
            if (w.dim == DimensionId.Nether) fog = NetherFog(w, p);
            else if (w.dim == DimensionId.End) fog = new Color(0.012f, 0.009f, 0.016f);
            float dense = 0f;
            if (p != null)
            {
                if (p.eyeInWater) { fog = new Color(0.010f, 0.036f, 0.16f); far = 26f; dense = 1f; }
                else if (p.inLava) { fog = new Color(0.32f, 0.012f, 0f); far = 3.2f; dense = 1f; }
                else if (p.inPowderSnow) { fog = new Color(0.35f, 0.50f, 0.58f); far = 2.5f; dense = 1f; }
                if (p.HasEffect(Effect.Blindness)) { fog = Color.black; far = 5f; dense = 1f; }
            }
            fogColor = fog;
            // open air: clear until the last tenth of the render distance, like the original's terrain fog
            float start = dense > 0.5f ? far * 0.25f : far - Mathf.Clamp(far / 10f, 4f, 64f);
            Shader.SetGlobalVector(FogColorId, new Vector4(fog.r, fog.g, fog.b, 1));
            Shader.SetGlobalVector(FogParamsId, new Vector4(start, far, dense, 1));
            // texture animation clock: the original flips animated textures at 10 fps
            Shader.SetGlobalVector(AnimId, new Vector4(Mathf.Floor(time * 10f), time, 0, 0));
        }

        /// <summary>Each Nether biome tints its haze differently (linear values of dark reds, teal and grey).</summary>
        static Color NetherFog(World w, Player p)
        {
            if (p == null) return new Color(0.035f, 0.004f, 0.003f);
            var biome = Biome.Get(w.GetBiomeId(Mathf.FloorToInt(p.position.x), Mathf.FloorToInt(p.position.y), Mathf.FloorToInt(p.position.z)));
            string id = biome != null ? biome.key : "";
            switch (id)
            {
                case "crimson_forest": return new Color(0.035f, 0.002f, 0.002f);
                case "warped_forest": return new Color(0.010f, 0.002f, 0.010f);
                case "soul_sand_valley": return new Color(0.011f, 0.063f, 0.060f);
                case "basalt_deltas": return new Color(0.14f, 0.11f, 0.16f);
                default: return new Color(0.035f, 0.004f, 0.003f);
            }
        }
    }

    /// <summary>Counters shown on the debug screen.</summary>
    public static class Listeners
    {
        public static int EntityCount { get; private set; }
        public static void SetCount(int n) { EntityCount = n; }
    }
}
