using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace MCR
{
    /// <summary>
    /// CPU particle system. Particles are camera-facing quads drawn with the chunk shader from the block texture array
    /// (particle sprites and block/item textures for break particles), lit by sampled world light.
    /// </summary>
    public static class Particles
    {
        struct P
        {
            public Vector3 pos, prev, vel;
            public float gravity, drag, size, size1;
            public int age, life;
            public int layer, frames; // frames > 1: animate over life across consecutive sprite names
            public Vector4 uv;
            public Color32 color;
            public bool emissive, collide, onGround;
            public byte kind; // 0 normal, 1 portal-in (to target), 2 glyph
            public Vector3 target;
        }

        const int Max = 6000;
        static readonly P[] ps = new P[Max];
        static int count;
        static Mesh mesh;
        static ChunkVertex[] verts = new ChunkVertex[Max * 4];
        static int[] idx;
        static Material mat;
        public static World activeWorld;
        public static bool enabled = true;
        public static int setting = 0; // 0 all, 1 decreased, 2 minimal
        static int[] genericLayers;
        static RNG rng = new RNG(12345);

        public static int Count => count;

        static bool Accept(World w, Vector3 pos)
        {
            if (!enabled || w == null || w != activeWorld) return false;
            if ((pos - GameManager.ListenerPosition).sqrMagnitude > 48 * 48) return false;
            if (setting == 2 && rng.NextFloat() < 0.8f) return false;
            if (setting == 1 && rng.NextFloat() < 0.4f) return false;
            return count < Max;
        }

        static int L(string n) => ParticleTextures.Layer(n);
        static Vector4 FullUV => new Vector4(0, 0, 1, 1);

        static void Add(ref P p)
        {
            if (count >= Max) return;
            p.prev = p.pos;
            if (p.size1 == 0) p.size1 = p.size;
            ps[count++] = p;
        }

        static float R(float a, float b) => a + rng.NextFloat() * (b - a);
        static Vector3 RandV(float s) => new Vector3(R(-s, s), R(-s, s), R(-s, s));

        // ------------------------------------------------------------------ spawn helpers
        static void Simple(World w, Vector3 pos, Vector3 vel, string sprite, Color32 color, float size, int life, float gravity, float drag = 0.96f, bool emissive = false, bool collide = true, int frames = 1)
        {
            if (!Accept(w, pos)) return;
            var p = new P { pos = pos, vel = vel, layer = L(sprite), frames = frames, uv = FullUV, color = color, size = size, life = life, gravity = gravity, drag = drag, emissive = emissive, collide = collide };
            Add(ref p);
        }

        public static void BlockBreak(World w, Int3 pos, ushort state)
        {
            var b = Blocks.ByState[state];
            if (b.isAir) return;
            int tex = b.particleTex;
            Color32 tint = TintFor(w, b, pos);
            for (int x = 0; x < 4; x++)
                for (int y = 0; y < 4; y++)
                    for (int z = 0; z < 4; z++)
                    {
                        var pp = new Vector3(pos.x + (x + 0.5f) / 4f, pos.y + (y + 0.5f) / 4f, pos.z + (z + 0.5f) / 4f);
                        if (!Accept(w, pp)) return;
                        var vel = (pp - pos.Center) * 0.3f + new Vector3(R(-0.05f, 0.05f), R(0, 0.1f), R(-0.05f, 0.05f));
                        float u = rng.Next(12) / 16f, v = rng.Next(12) / 16f;
                        var p = new P { pos = pp, vel = vel, layer = tex, frames = 1, uv = new Vector4(u, v, u + 0.25f, v + 0.25f), color = tint, size = R(0.08f, 0.14f), life = 10 + rng.Next(20), gravity = 0.04f, drag = 0.98f, collide = true };
                        Add(ref p);
                    }
        }

        public static void BlockHit(World w, Int3 pos, Dir face, ushort state)
        {
            var b = Blocks.ByState[state]; if (b.isAir) return;
            Vector3 n = DirUtil.Normal[(int)face];
            var pp = pos.Center + n * 0.55f + new Vector3(n.x == 0 ? R(-0.45f, 0.45f) : 0, n.y == 0 ? R(-0.45f, 0.45f) : 0, n.z == 0 ? R(-0.45f, 0.45f) : 0);
            if (!Accept(w, pp)) return;
            float u = rng.Next(12) / 16f, v = rng.Next(12) / 16f;
            var p = new P { pos = pp, vel = n * 0.05f + RandV(0.03f), layer = b.particleTex, frames = 1, uv = new Vector4(u, v, u + 0.25f, v + 0.25f), color = TintFor(w, b, pos), size = 0.07f, life = 8 + rng.Next(10), gravity = 0.04f, drag = 0.98f, collide = true };
            Add(ref p);
        }

        public static void BlockDust(World w, Vector3 pos, ushort state, int n)
        {
            var b = Blocks.ByState[state]; if (b.isAir) return;
            var tint = TintFor(w, b, Int3.Floor(pos));
            for (int i = 0; i < n; i++)
            {
                var pp = pos + new Vector3(R(-0.3f, 0.3f), 0.1f, R(-0.3f, 0.3f));
                if (!Accept(w, pp)) return;
                float u = rng.Next(12) / 16f, v = rng.Next(12) / 16f;
                var p = new P { pos = pp, vel = new Vector3(R(-0.15f, 0.15f), R(0.05f, 0.2f), R(-0.15f, 0.15f)), layer = b.particleTex, frames = 1, uv = new Vector4(u, v, u + 0.25f, v + 0.25f), color = tint, size = 0.08f, life = 10 + rng.Next(10), gravity = 0.06f, drag = 0.9f, collide = true };
                Add(ref p);
            }
        }

        public static void Sprint(World w, Vector3 pos, ushort state) => BlockDust(w, pos, state, 1);

        static Color32 TintFor(World w, Block b, Int3 pos)
        {
            if (b.tint == TintType.None) return new Color32(255, 255, 255, 255);
            var bio = w.GetSurfaceBiome(pos.x, pos.z);
            switch (b.tint)
            {
                case TintType.Grass: return bio.grass;
                case TintType.Foliage: return bio.foliage;
                case TintType.Water: return bio.water;
                case TintType.Spruce: return new Color32(97, 153, 97, 255);
                case TintType.Birch: return new Color32(128, 167, 85, 255);
                default: return new Color32(255, 255, 255, 255);
            }
        }

        public static void ItemBreak(World w, Vector3 pos, ItemStack s)
        {
            if (s == null || s.item == null) return;
            int layer = ItemRender.ParticleLayer(s.item);
            for (int i = 0; i < 8; i++)
            {
                if (!Accept(w, pos)) return;
                float u = rng.Next(12) / 16f, v = rng.Next(12) / 16f;
                var p = new P { pos = pos + RandV(0.1f), vel = new Vector3(R(-0.1f, 0.1f), R(0.05f, 0.2f), R(-0.1f, 0.1f)), layer = layer, frames = 1, uv = new Vector4(u, v, u + 0.25f, v + 0.25f), color = new Color32(255, 255, 255, 255), size = 0.07f, life = 12 + rng.Next(8), gravity = 0.05f, drag = 0.98f, collide = true };
                Add(ref p);
            }
        }
        public static void ItemCrumbs(World w, Vector3 pos, ItemStack s) { if (s != null && rng.NextFloat() < 0.8f) ItemBreak(w, pos, s); }

        public static void Smoke(World w, Vector3 pos, int n, float spread, bool large = false)
        {
            for (int i = 0; i < n; i++)
                Simple(w, pos + RandV(spread * 0.5f), new Vector3(R(-0.01f, 0.01f), R(0.01f, 0.05f), R(-0.01f, 0.01f)), "generic_7", Img.Gray(large ? 70 : 60 + rng.Next(40)), large ? 0.25f : 0.12f, 20 + rng.Next(20), -0.002f, 0.96f, false, false, -8);
        }
        public static void LargeSmoke(World w, Vector3 pos) => Smoke(w, pos, 1, 0.2f, true);

        public static void CampfireSmoke(World w, Vector3 pos, bool signal)
        {
            if (!Accept(w, pos)) return;
            var p = new P { pos = pos, vel = new Vector3(R(-0.005f, 0.005f), signal ? 0.12f : 0.07f, R(-0.005f, 0.005f)), layer = L("generic_7"), frames = -8, uv = FullUV, color = Img.Gray(signal ? 200 : 170), size = 0.35f, size1 = 0.6f, life = signal ? 280 : 140, gravity = -0.0006f, drag = 0.995f, collide = false };
            Add(ref p);
        }

        public static void Flame(World w, Vector3 pos, string kind)
        {
            string sprite = kind == "soul_fire_flame" || kind == "soul" ? "soul_flame" : kind == "copper" || kind == "copper_fire_flame" ? "copper_flame" : kind == "small_flame" ? "small_flame" : "flame";
            Simple(w, pos, new Vector3(R(-0.002f, 0.002f), R(0.0f, 0.01f), R(-0.002f, 0.002f)), sprite, new Color32(255, 255, 255, 255), kind == "small_flame" ? 0.07f : 0.1f, 8 + rng.Next(12), -0.0005f, 0.96f, true, false);
        }

        public static void LavaPop(World w, Vector3 pos)
        {
            Simple(w, pos, new Vector3(R(-0.08f, 0.08f), R(0.1f, 0.3f), R(-0.08f, 0.08f)), "lava", new Color32(255, 255, 255, 255), R(0.06f, 0.12f), 16 + rng.Next(16), 0.04f, 0.99f, true, true);
        }

        public static void Bubble(World w, Vector3 pos, bool column, int n = 1)
        {
            for (int i = 0; i < n; i++)
                Simple(w, pos + RandV(0.2f), new Vector3(R(-0.02f, 0.02f), column ? R(0.05f, 0.15f) : R(0.02f, 0.06f), R(-0.02f, 0.02f)), "bubble", new Color32(255, 255, 255, 255), 0.06f, 20 + rng.Next(20), -0.002f, 0.9f, false, false);
        }

        public static void Splash(World w, Vector3 pos, int n)
        {
            for (int i = 0; i < n; i++)
                Simple(w, pos + new Vector3(R(-0.5f, 0.5f), 0.05f, R(-0.5f, 0.5f)), new Vector3(R(-0.1f, 0.1f), R(0.1f, 0.25f), R(-0.1f, 0.1f)), "splash", new Color32(255, 255, 255, 255), 0.08f, 10 + rng.Next(10), 0.04f, 0.98f, false, true);
        }

        public static void Drip(World w, Vector3 pos, bool lava)
        {
            Simple(w, pos, Vector3.zero, "drip", lava ? new Color32(255, 110, 20, 255) : new Color32(70, 110, 230, 255), 0.06f, 60, 0.02f, 0.98f, lava, true);
        }

        public static void FallingDust(World w, Vector3 pos, ushort state)
        {
            var b = Blocks.ByState[state];
            Simple(w, pos, Vector3.zero, "dust", b.mapColor, 0.07f, 60, 0.004f, 0.98f, false, true);
        }

        public static void Crit(World w, Vector3 pos, int n) { for (int i = 0; i < n; i++) Simple(w, pos + RandV(0.3f), RandV(0.25f), "crit", new Color32(255, 240, 180, 255), 0.1f, 10 + rng.Next(8), 0.02f, 0.85f, true, false); }
        public static void MagicCrit(World w, Vector3 pos, int n) { for (int i = 0; i < n; i++) Simple(w, pos + RandV(0.3f), RandV(0.25f), "enchanted_hit", new Color32(120, 200, 255, 255), 0.1f, 10 + rng.Next(8), 0.02f, 0.85f, true, false); }

        public static void Effect(World w, Vector3 pos, Color32 color, bool ambient)
        {
            Simple(w, pos, new Vector3(0, R(0.01f, 0.03f), 0), ambient ? "spell_ambient" : "spell", color, 0.09f, 20 + rng.Next(10), -0.002f, 0.96f, false, false);
        }
        public static void SplashPotion(World w, Vector3 pos, Color32 color)
        {
            for (int i = 0; i < 40; i++) Simple(w, pos + RandV(0.3f), new Vector3(R(-0.2f, 0.2f), R(0.02f, 0.2f), R(-0.2f, 0.2f)), "spell", color, 0.1f, 20 + rng.Next(20), 0.01f, 0.9f, true, false);
        }

        public static void Enchant(World w, Vector3 from, Vector3 to)
        {
            if (!Accept(w, from)) return;
            var p = new P { pos = from, vel = Vector3.zero, layer = L("glyph_" + rng.Next(8)), frames = 1, uv = FullUV, color = new Color32(230, 230, 255, 255), size = 0.08f, life = 30 + rng.Next(20), kind = 2, target = to, emissive = true };
            Add(ref p);
        }

        public static void Portal(World w, Vector3 pos)
        {
            if (!Accept(w, pos)) return;
            var p = new P { pos = pos, vel = RandV(0.05f), layer = L("portal"), frames = 1, uv = FullUV, color = new Color32((byte)(150 + rng.Next(60)), (byte)(40 + rng.Next(40)), 255, 255), size = 0.07f, life = 30 + rng.Next(20), kind = 1, target = pos + RandV(1f), emissive = true };
            Add(ref p);
        }
        public static void EndPortalSmoke(World w, Vector3 pos) => Simple(w, pos, new Vector3(0, 0.01f, 0), "generic_5", Img.Gray(20), 0.12f, 30 + rng.Next(20), -0.001f, 0.96f, false, false, 1);
        public static void EndRod(World w, Vector3 pos) => Simple(w, pos, RandV(0.02f), "end_rod", new Color32(255, 255, 255, 255), 0.08f, 40 + rng.Next(30), 0.0f, 0.97f, true, false);
        public static void HappyVillager(World w, Vector3 pos, int n) { for (int i = 0; i < n; i++) Simple(w, pos + RandV(0.5f), new Vector3(0, R(0, 0.03f), 0), "happy", new Color32(80, 230, 80, 255), 0.09f, 20 + rng.Next(20), 0f, 0.95f, true, false); }
        public static void Angry(World w, Vector3 pos) => Simple(w, pos, new Vector3(0, 0.02f, 0), "angry", new Color32(255, 255, 255, 255), 0.2f, 30, 0f, 0.95f, true, false);
        public static void Heart(World w, Vector3 pos) => Simple(w, pos + RandV(0.3f), new Vector3(0, 0.03f, 0), "heart", new Color32(255, 255, 255, 255), 0.13f, 30, -0.001f, 0.95f, true, false);
        public static void DamageIndicator(World w, Vector3 pos, int n) { for (int i = 0; i < n; i++) Simple(w, pos + RandV(0.3f), new Vector3(R(-0.05f, 0.05f), R(0.05f, 0.15f), R(-0.05f, 0.05f)), "damage", new Color32(255, 255, 255, 255), 0.1f, 20, 0.02f, 0.9f, false, false); }
        public static void Note(World w, Vector3 pos, float colorFrac)
        {
            float h = colorFrac;
            Color c = Color.HSVToRGB(h, 0.9f, 1f);
            Simple(w, pos, new Vector3(0, 0.03f, 0), "note", c, 0.13f, 24, -0.001f, 0.9f, true, false);
        }
        public static void Petal(World w, Vector3 pos, Color32 color) => Simple(w, pos, new Vector3(R(-0.02f, 0.02f), -0.01f, R(-0.02f, 0.02f)), "petal", color, 0.08f, 200, 0.001f, 0.98f, false, true);
        public static void Leaf(World w, Vector3 pos, Color32 color) => Simple(w, pos, new Vector3(R(-0.02f, 0.02f), -0.01f, R(-0.02f, 0.02f)), "leaf", color, 0.08f, 200, 0.001f, 0.98f, false, true);
        public static void Spore(World w, Vector3 pos) => Simple(w, pos, new Vector3(R(-0.01f, 0.01f), R(-0.01f, 0.01f), R(-0.01f, 0.01f)), "spore", new Color32(200, 120, 180, 255), 0.05f, 100, 0.0005f, 0.99f, false, false);
        public static void Ash(World w, Vector3 pos, Color32 c) => Simple(w, pos, new Vector3(R(-0.01f, 0.01f), -0.005f, R(-0.01f, 0.01f)), "spore", c, 0.05f, 150, 0.0005f, 0.99f, false, true);
        public static void SulfurCloud(World w, Vector3 pos) => Simple(w, pos, new Vector3(R(-0.01f, 0.01f), R(0.01f, 0.03f), R(-0.01f, 0.01f)), "sulfur", new Color32(240, 232, 120, 255), 0.25f, 60 + rng.Next(40), -0.0005f, 0.97f, false, false);
        public static void RedstoneDust(World w, Vector3 pos, float power)
        {
            byte r = (byte)(80 + 175 * Mathf.Clamp01(power));
            Simple(w, pos, Vector3.zero, "dust", new Color32(r, 0, 0, 255), 0.06f, 12 + rng.Next(8), 0f, 0.9f, true, false);
        }
        public static void WaxOff(World w, Int3 pos) { for (int i = 0; i < 10; i++) Simple(w, pos.Center + RandV(0.6f), RandV(0.03f), "wax", new Color32(255, 255, 255, 255), 0.08f, 20, 0f, 0.9f, true, false); }
        public static void WaxOn(World w, Int3 pos) { for (int i = 0; i < 10; i++) Simple(w, pos.Center + RandV(0.6f), RandV(0.03f), "wax", new Color32(255, 200, 80, 255), 0.08f, 20, 0f, 0.9f, true, false); }
        public static void Scrape(World w, Int3 pos) { for (int i = 0; i < 10; i++) Simple(w, pos.Center + RandV(0.6f), RandV(0.03f), "scrape", new Color32(255, 255, 255, 255), 0.08f, 20, 0f, 0.9f, true, false); }

        public static void Poof(World w, Vector3 pos, float width, float height)
        {
            for (int i = 0; i < 20; i++)
                Simple(w, pos + new Vector3(R(-width, width) * 0.5f, R(-height, height) * 0.5f, R(-width, width) * 0.5f), RandV(0.04f), "generic_7", Img.Gray(230), 0.18f, 16 + rng.Next(10), -0.002f, 0.92f, false, false, -8);
        }

        public static void Explosion(World w, Vector3 pos, float power)
        {
            int n = Mathf.Clamp((int)(power * 4), 4, 24);
            for (int i = 0; i < n; i++)
                Simple(w, pos + RandV(power * 0.5f), RandV(0.02f), "explosion_0", Img.Gray(200 + rng.Next(55)), R(0.5f, 1.0f) * Mathf.Min(2f, power / 2f), 12 + rng.Next(6), 0f, 0.9f, true, false, 8);
            Smoke(w, pos, n, power, true);
        }

        public static void Sweep(World w, Vector3 pos) => Simple(w, pos, Vector3.zero, "sweep_0", Img.Gray(240), 0.9f, 8, 0f, 1f, false, false, 4);
        public static void SonicBoom(World w, Vector3 pos) => Simple(w, pos, Vector3.zero, "sonic_0", new Color32(255, 255, 255, 255), 0.8f, 12, 0f, 1f, true, false, 4);
        public static void Totem(World w, Vector3 pos)
        {
            for (int i = 0; i < 60; i++) Simple(w, pos + RandV(0.3f), new Vector3(R(-0.4f, 0.4f), R(0.2f, 0.6f), R(-0.4f, 0.4f)), "totem", rng.NextBool() ? new Color32(240, 220, 60, 255) : new Color32(90, 220, 90, 255), 0.1f, 30 + rng.Next(30), 0.02f, 0.92f, true, false);
        }
        public static void DragonBreath(World w, Vector3 pos) => Simple(w, pos + RandV(0.3f), RandV(0.02f), "dragon_breath", new Color32(230, 110, 255, 255), 0.2f, 30 + rng.Next(20), -0.001f, 0.96f, true, false);
        public static void Snowflake(World w, Vector3 pos) => Simple(w, pos, new Vector3(R(-0.02f, 0.02f), -0.03f, R(-0.02f, 0.02f)), "snowflake", new Color32(255, 255, 255, 255), 0.06f, 60, 0.001f, 0.99f, false, true);
        public static void Firework(World w, Vector3 pos, Vector3 vel, Color32 c) => Simple(w, pos, vel, "spark", c, 0.1f, 30 + rng.Next(15), 0.004f, 0.91f, true, false);
        public static void Soul(World w, Vector3 pos) => Simple(w, pos, new Vector3(0, 0.03f, 0), "soul", new Color32(255, 255, 255, 255), 0.12f, 40, -0.001f, 0.97f, true, false);
        public static void Cloud(World w, Vector3 pos) => Simple(w, pos, new Vector3(R(-0.02f, 0.02f), 0.02f, R(-0.02f, 0.02f)), "cloud", Img.Gray(240), 0.3f, 20, -0.001f, 0.95f, false, false);
        public static void Firefly(World w, Vector3 pos) => Simple(w, pos, RandV(0.01f), "firefly", new Color32(255, 255, 255, 255), 0.05f, 80, 0f, 0.99f, true, false);
        public static void Glow(World w, Vector3 pos) => Simple(w, pos, RandV(0.01f), "glow", new Color32(120, 255, 220, 255), 0.06f, 60, 0f, 0.99f, true, false);
        public static void RainSplash(World w, Vector3 pos) => Simple(w, pos, new Vector3(R(-0.03f, 0.03f), 0.06f, R(-0.03f, 0.03f)), "splash", new Color32(160, 180, 255, 255), 0.05f, 6, 0.04f, 0.98f, false, true);

        // ------------------------------------------------------------------ simulation (20 TPS)
        static readonly List<AABB> boxes = new List<AABB>();
        public static void Tick(World w)
        {
            activeWorld = w;
            for (int i = 0; i < count; i++)
            {
                ref P p = ref ps[i];
                p.prev = p.pos;
                p.age++;
                if (p.age >= p.life) { ps[i] = ps[--count]; i--; continue; }
                if (p.kind == 1)
                {
                    float t = p.age / (float)p.life;
                    p.pos = Vector3.Lerp(p.target, p.target + (p.prev - p.target) * 0.9f, t);
                    p.pos = p.target + (p.pos - p.target) * (1 - t * 0.05f);
                    p.pos += p.vel; p.vel *= 0.9f;
                    continue;
                }
                if (p.kind == 2)
                {
                    float t = p.age / (float)p.life;
                    p.pos = Vector3.Lerp(p.pos, p.target, 0.08f + t * 0.1f);
                    continue;
                }
                p.vel.y -= p.gravity;
                Vector3 np = p.pos + p.vel;
                if (p.collide && w != null)
                {
                    ushort s = w.GetState(Mathf.FloorToInt(np.x), Mathf.FloorToInt(np.y), Mathf.FloorToInt(np.z));
                    if (s != 0 && Blocks.ByState[s].solid)
                    {
                        var bl = Blocks.ByState[s];
                        bool hit = bl.opaqueCube;
                        if (!hit)
                        {
                            boxes.Clear(); var bp = Int3.Floor(np);
                            bl.GetCollisionBoxes(s - bl.baseState, w, bp, boxes);
                            foreach (var bb in boxes) if (bb.Offset(bp.x, bp.y, bp.z).Contains(np)) { hit = true; break; }
                        }
                        if (hit)
                        {
                            if (Mathf.FloorToInt(np.y) != Mathf.FloorToInt(p.pos.y) && p.vel.y < 0) { np.y = Mathf.Floor(p.pos.y) + 0.01f; p.vel.y = 0; p.vel.x *= 0.7f; p.vel.z *= 0.7f; p.onGround = true; }
                            else { np = p.pos; p.vel *= 0.2f; }
                        }
                    }
                }
                p.pos = np;
                p.vel *= p.drag;
                if (p.onGround) { p.vel.x *= 0.7f; p.vel.z *= 0.7f; }
            }
        }

        public static void Clear() { count = 0; }

        // ------------------------------------------------------------------ rendering
        public static void Render(Camera cam, float partial)
        {
            if (count == 0 || cam == null || activeWorld == null) return;
            if (mesh == null)
            {
                mesh = new Mesh { name = "Particles" }; mesh.MarkDynamic();
                idx = new int[Max * 6];
                for (int i = 0; i < Max; i++) { int v = i * 4, k = i * 6; idx[k] = v; idx[k + 1] = v + 1; idx[k + 2] = v + 2; idx[k + 3] = v; idx[k + 4] = v + 2; idx[k + 5] = v + 3; }
                mat = Res.ChunkCutoutNoCull;
                genericLayers = new int[8];
                for (int i = 0; i < 8; i++) genericLayers[i] = L("generic_" + i);
            }
            Vector3 right = cam.transform.right, up = cam.transform.up;
            var w = activeWorld;
            for (int i = 0; i < count; i++)
            {
                ref P p = ref ps[i];
                Vector3 pos = Vector3.LerpUnclamped(p.prev, p.pos, partial);
                float t = (p.age + partial) / p.life;
                float size = Mathf.Lerp(p.size, p.size1, t);
                int layer = p.layer;
                if (p.frames > 1 || p.frames < -1)
                {
                    int n = p.frames > 0 ? p.frames : -p.frames;
                    int f = Mathf.Clamp((int)(t * n), 0, n - 1);
                    layer = p.frames > 0 ? p.layer + f : p.layer - f; // consecutive sprite layers
                }
                Vector3 r = right * size, u = up * size;
                byte sky, blk;
                if (p.emissive) { sky = 255; blk = 255; }
                else
                {
                    byte l = w.GetLightRaw(Mathf.FloorToInt(pos.x), Mathf.FloorToInt(pos.y), Mathf.FloorToInt(pos.z));
                    sky = (byte)((l >> 4) * 17); blk = (byte)((l & 15) * 17);
                }
                uint light = (uint)(sky | (blk << 8));
                uint col = MeshCtx.Pack(p.color.r, p.color.g, p.color.b, 255);
                ushort layerH = HalfConv.ToHalf(layer), one = HalfConv.ToHalf(1);
                int v = i * 4;
                ushort u0 = HalfConv.ToHalf(p.uv.x), v0 = HalfConv.ToHalf(1 - p.uv.w), u1 = HalfConv.ToHalf(p.uv.z), v1 = HalfConv.ToHalf(1 - p.uv.y);
                SetV(ref verts[v], pos - r - u, col, u0, v0, layerH, one, light);
                SetV(ref verts[v + 1], pos - r + u, col, u0, v1, layerH, one, light);
                SetV(ref verts[v + 2], pos + r + u, col, u1, v1, layerH, one, light);
                SetV(ref verts[v + 3], pos + r - u, col, u1, v0, layerH, one, light);
            }
            mesh.Clear(false);
            mesh.SetVertexBufferParams(count * 4, ChunkVertex.Layout);
            mesh.SetVertexBufferData(verts, 0, 0, count * 4, 0, MeshUpdateFlags.DontValidateIndices | MeshUpdateFlags.DontRecalculateBounds);
            mesh.SetIndexBufferParams(count * 6, IndexFormat.UInt32);
            mesh.SetIndexBufferData(idx, 0, 0, count * 6, MeshUpdateFlags.DontValidateIndices | MeshUpdateFlags.DontRecalculateBounds);
            mesh.subMeshCount = 1;
            mesh.SetSubMesh(0, new SubMeshDescriptor(0, count * 6), MeshUpdateFlags.DontRecalculateBounds);
            mesh.bounds = new Bounds(cam.transform.position, Vector3.one * 200);
            Graphics.RenderMesh(new RenderParams(mat) { layer = 0, shadowCastingMode = ShadowCastingMode.Off, receiveShadows = false }, mesh, 0, Matrix4x4.identity);
        }

        static void SetV(ref ChunkVertex v, Vector3 p, uint col, ushort u, ushort vv, ushort layer, ushort frames, uint light)
        {
            v.x = p.x; v.y = p.y; v.z = p.z; v.color = col; v.u = u; v.v = vv; v.layer = layer; v.anim = frames; v.light = light;
        }
    }
}
