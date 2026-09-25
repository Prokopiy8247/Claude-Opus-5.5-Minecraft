using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public enum SoundEvent { Break, Place, Step, Hit, Fall }

    /// <summary>
    /// All game audio is synthesized procedurally at runtime (original sounds, no recorded assets).
    /// Clips are generated lazily per sound name (3 variants) and played through a pooled set of 3D AudioSources.
    /// </summary>
    public static class Sounds
    {
        public const int SR = 22050;
        static readonly Dictionary<string, AudioClip[]> cache = new Dictionary<string, AudioClip[]>();
        static AudioSource[] pool;
        static int next;
        static GameObject root;
        public static float masterVolume = 1f, blockVolume = 1f, hostileVolume = 1f, friendlyVolume = 1f, playerVolume = 1f, ambientVolume = 1f, musicVolume = 0.6f, recordVolume = 1f, weatherVolume = 1f;
        static readonly Dictionary<Int3, AudioSource> records = new Dictionary<Int3, AudioSource>();
        static AudioSource music, rain, ambientLoop;
        public static bool muted;
        static float lastPlayTime; static int playsThisFrame; static int frame;

        public static void Init()
        {
            if (root != null) return;
            root = new GameObject("MCR_Audio");
            UnityEngine.Object.DontDestroyOnLoad(root);
            pool = new AudioSource[40];
            for (int i = 0; i < pool.Length; i++)
            {
                var go = new GameObject("snd" + i);
                go.transform.SetParent(root.transform, false);
                var a = go.AddComponent<AudioSource>();
                a.playOnAwake = false; a.spatialBlend = 1f; a.rolloffMode = AudioRolloffMode.Linear; a.minDistance = 1f; a.maxDistance = 20f; a.dopplerLevel = 0f;
                pool[i] = a;
            }
            music = MakeLoopSource("music", false);
            rain = MakeLoopSource("rain", true);
            ambientLoop = MakeLoopSource("ambient", true);
        }

        static AudioSource MakeLoopSource(string name, bool loop)
        {
            var go = new GameObject(name); go.transform.SetParent(root.transform, false);
            var a = go.AddComponent<AudioSource>(); a.playOnAwake = false; a.spatialBlend = 0f; a.loop = loop; a.volume = 0;
            return a;
        }

        static float CategoryVolume(string name)
        {
            if (name.StartsWith("block.")) return blockVolume;
            if (name.StartsWith("entity.player") || name.StartsWith("item.")) return playerVolume;
            if (name.StartsWith("entity."))
            {
                foreach (var h in HostileNames) if (name.Contains(h)) return hostileVolume;
                return friendlyVolume;
            }
            if (name.StartsWith("ambient.")) return ambientVolume;
            if (name.StartsWith("weather.")) return weatherVolume;
            return 1f;
        }
        static readonly string[] HostileNames = { "zombie", "skeleton", "creeper", "spider", "enderman", "witch", "blaze", "ghast", "slime", "magma", "wither", "dragon", "guardian", "phantom", "drowned", "husk", "stray", "pillager", "vindicator", "evoker", "ravager", "vex", "piglin", "hoglin", "zoglin", "warden", "breeze", "bogged", "creaking", "silverfish", "endermite", "shulker" };

        public static void Play(string name, Vector3 pos, float volume = 1f, float pitch = 1f)
        {
            if (muted || pool == null || string.IsNullOrEmpty(name)) return;
            if (Time.frameCount != frame) { frame = Time.frameCount; playsThisFrame = 0; }
            if (++playsThisFrame > 12) return;
            var listener = GameManager.ListenerPosition;
            float range = Mathf.Max(16f, 16f * volume);
            if ((pos - listener).sqrMagnitude > range * range) return;
            var clips = GetClips(name);
            if (clips == null || clips.Length == 0) return;
            var src = pool[next]; next = (next + 1) % pool.Length;
            src.transform.position = pos;
            src.clip = clips[UnityEngine.Random.Range(0, clips.Length)];
            src.volume = Mathf.Clamp01(volume) * masterVolume * CategoryVolume(name);
            src.maxDistance = range;
            src.pitch = Mathf.Clamp(pitch, 0.3f, 3f);
            src.spatialBlend = (pos - listener).sqrMagnitude < 0.25f ? 0f : 1f;
            src.Play();
        }

        public static void PlayUI(string name, float volume = 1f, float pitch = 1f)
        {
            if (pool == null) return;
            var clips = GetClips(name); if (clips == null) return;
            var src = pool[next]; next = (next + 1) % pool.Length;
            src.clip = clips[0]; src.volume = volume * masterVolume; src.pitch = pitch; src.spatialBlend = 0; src.Play();
        }

        public static void PlayBlock(SoundType type, SoundEvent ev, Vector3 pos)
        {
            float vol = ev == SoundEvent.Step ? 0.15f : ev == SoundEvent.Hit ? 0.25f : ev == SoundEvent.Fall ? 0.5f : 1f;
            float pitch = ev == SoundEvent.Hit ? 0.5f : ev == SoundEvent.Place ? 0.8f : 1f;
            pitch *= 0.9f + UnityEngine.Random.value * 0.2f;
            string ev2 = ev == SoundEvent.Hit ? "step" : ev == SoundEvent.Fall ? "step" : ev.ToString().ToLowerInvariant();
            Play("block." + type.ToString().ToLowerInvariant() + "." + ev2, pos, vol, pitch);
        }

        public static void PlayMusicAt(Int3 pos, string disc)
        {
            StopMusicAt(pos);
            if (pool == null) return;
            var go = new GameObject("record_" + disc); go.transform.SetParent(root.transform, false);
            go.transform.position = pos.Center;
            var a = go.AddComponent<AudioSource>();
            a.clip = MusicGen.Disc(disc);
            a.spatialBlend = 1f; a.rolloffMode = AudioRolloffMode.Linear; a.minDistance = 4; a.maxDistance = 64; a.volume = recordVolume * masterVolume; a.loop = false;
            a.Play();
            records[pos] = a;
            if (music != null) music.Stop();
        }
        public static void StopMusicAt(Int3 pos)
        {
            if (records.TryGetValue(pos, out var a)) { if (a != null) UnityEngine.Object.Destroy(a.gameObject); records.Remove(pos); }
        }
        public static bool AnyRecordPlaying { get { foreach (var r in records.Values) if (r != null && r.isPlaying) return true; return false; } }

        // ------------------------------------------------------------------ background music & loops
        static float musicTimer = 60f;
        public static void UpdateAmbience(float dt, float rainAmount, bool underground, DimensionId dim, bool inMenu)
        {
            if (pool == null) return;
            if (rain != null)
            {
                if (rain.clip == null) rain.clip = MakeClip("weather.rain.loop", Synth.RainLoop(), true);
                float target = rainAmount * 0.35f * weatherVolume * masterVolume * (underground ? 0.25f : 1f);
                rain.volume = Mathf.MoveTowards(rain.volume, target, dt * 0.5f);
                if (rain.volume > 0.001f && !rain.isPlaying) rain.Play(); else if (rain.volume <= 0.001f && rain.isPlaying) rain.Stop();
            }
            if (ambientLoop != null)
            {
                if (dim == DimensionId.Nether) { if (ambientLoop.clip == null || ambientLoop.clip.name != "nether") ambientLoop.clip = MakeClip("nether", Synth.NetherLoop(), true); }
                else if (dim == DimensionId.End) { if (ambientLoop.clip == null || ambientLoop.clip.name != "end") ambientLoop.clip = MakeClip("end", Synth.EndLoop(), true); }
                float target = dim == DimensionId.Overworld ? 0f : 0.3f * ambientVolume * masterVolume;
                ambientLoop.volume = Mathf.MoveTowards(ambientLoop.volume, target, dt * 0.3f);
                if (ambientLoop.volume > 0.001f && !ambientLoop.isPlaying && ambientLoop.clip != null) ambientLoop.Play();
                else if (ambientLoop.volume <= 0.001f && ambientLoop.isPlaying) ambientLoop.Stop();
            }
            if (music != null)
            {
                music.volume = musicVolume * masterVolume * 0.5f;
                if (!music.isPlaying && !AnyRecordPlaying)
                {
                    musicTimer -= dt;
                    if (musicTimer <= 0 && musicVolume > 0.01f)
                    {
                        int piece = UnityEngine.Random.Range(0, 1000);
                        music.clip = MusicGen.Ambient(inMenu ? "menu" : dim.ToString().ToLowerInvariant(), piece);
                        music.Play();
                        musicTimer = UnityEngine.Random.Range(300f, 900f);
                    }
                }
            }
        }
        public static void PlayMenuMusicSoon() { musicTimer = Mathf.Min(musicTimer, 2f); }
        public static void StopMusic() { if (music != null) music.Stop(); musicTimer = 120f; }

        // ------------------------------------------------------------------ clip cache
        static AudioClip[] GetClips(string name)
        {
            if (cache.TryGetValue(name, out var c)) return c;
            int variants = Synth.VariantCount(name);
            c = new AudioClip[variants];
            for (int i = 0; i < variants; i++)
            {
                float[] data;
                try { data = Synth.Make(name, i); }
                catch (Exception e) { Debug.LogWarning("Sound synth failed " + name + ": " + e.Message); data = null; }
                if (data == null || data.Length == 0) { cache[name] = null; return null; }
                c[i] = MakeClip(name + "#" + i, data, false);
            }
            cache[name] = c;
            return c;
        }

        public static AudioClip MakeClip(string name, float[] data, bool loop)
        {
            var clip = AudioClip.Create(name, data.Length, 1, SR, false);
            clip.SetData(data, 0);
            return clip;
        }

        public static int CachedCount => cache.Count;
        /// <summary>Pre-generate common sounds (called during loading).</summary>
        public static void Warmup()
        {
            foreach (SoundType t in Enum.GetValues(typeof(SoundType)))
                foreach (var ev in new[] { "break", "place", "step" }) GetClips("block." + t.ToString().ToLowerInvariant() + "." + ev);
            foreach (var n in new[] { "entity.generic.hurt", "entity.player.hurt", "entity.item.pickup", "entity.experience_orb.pickup", "ui.button.click", "entity.generic.explode", "entity.arrow.shoot", "entity.arrow.hit", "entity.player.attack.strong", "entity.player.attack.weak", "entity.generic.eat" })
                GetClips(n);
        }
    }

    /// <summary>Procedural synthesis recipes.</summary>
    public static class Synth
    {
        const int SR = Sounds.SR;
        public enum Wave { Sine, Square, Saw, Triangle, Noise }

        public static int VariantCount(string name)
        {
            if (name.StartsWith("block.") || name.Contains(".step") || name.Contains("hurt") || name.Contains("ambient") || name.Contains("eat") || name.Contains("attack")) return 3;
            return 1;
        }

        static float[] Buf(float sec) => new float[Mathf.Max(1, (int)(sec * SR))];

        static float Env(float t, float dur, float attack, float curve)
        {
            if (t < attack) return attack <= 0 ? 1 : t / attack;
            float x = (t - attack) / Mathf.Max(0.0001f, dur - attack);
            return Mathf.Pow(Mathf.Clamp01(1f - x), curve);
        }

        static float Osc(Wave w, float ph, ref RNG r)
        {
            ph -= Mathf.Floor(ph);
            switch (w)
            {
                case Wave.Square: return ph < 0.5f ? 1f : -1f;
                case Wave.Saw: return ph * 2f - 1f;
                case Wave.Triangle: return ph < 0.5f ? ph * 4f - 1f : 3f - ph * 4f;
                case Wave.Noise: return r.NextFloat() * 2f - 1f;
                default: return Mathf.Sin(ph * Mathf.PI * 2f);
            }
        }

        /// <summary>Add a tone with exponential pitch sweep f0->f1, optional vibrato.</summary>
        static void Tone(float[] b, ref RNG r, float start, float dur, float f0, float f1, float gain, Wave w = Wave.Sine, float attack = 0.005f, float curve = 2f, float vibHz = 0, float vibAmt = 0, float lp = 0)
        {
            int s0 = (int)(start * SR), n = (int)(dur * SR);
            float ph = 0, y = 0;
            float k = lp > 0 ? 1f - Mathf.Exp(-2f * Mathf.PI * lp / SR) : 1f;
            for (int i = 0; i < n && s0 + i < b.Length; i++)
            {
                float t = i / (float)SR;
                float f = f0 * Mathf.Pow(f1 / f0, t / dur);
                if (vibHz > 0) f *= 1f + vibAmt * Mathf.Sin(t * vibHz * Mathf.PI * 2f);
                ph += f / SR;
                float v = Osc(w, ph, ref r);
                y += (v - y) * k;
                if (s0 + i >= 0) b[s0 + i] += y * gain * Env(t, dur, attack, curve);
            }
        }

        /// <summary>Filtered noise burst (one-pole LP and HP).</summary>
        static void Noise(float[] b, ref RNG r, float start, float dur, float gain, float lp, float hp, float attack = 0.002f, float curve = 2f, float grain = 0)
        {
            int s0 = (int)(start * SR), n = (int)(dur * SR);
            float kl = 1f - Mathf.Exp(-2f * Mathf.PI * Mathf.Min(lp, SR * 0.45f) / SR);
            float kh = 1f - Mathf.Exp(-2f * Mathf.PI * hp / SR);
            float yl = 0, yh = 0;
            float gAmp = 1f; int gLeft = 0;
            for (int i = 0; i < n && s0 + i < b.Length; i++)
            {
                float t = i / (float)SR;
                float x = r.NextFloat() * 2f - 1f;
                yl += (x - yl) * kl;
                yh += (yl - yh) * kh;
                float v = yl - yh;
                if (grain > 0)
                {
                    if (--gLeft <= 0) { gLeft = (int)(SR * (0.004f + r.NextFloat() * grain)); gAmp = r.NextFloat() < 0.55f ? 0.15f + r.NextFloat() : 0.05f; }
                    v *= gAmp;
                }
                if (s0 + i >= 0) b[s0 + i] += v * gain * Env(t, dur, attack, curve);
            }
        }

        /// <summary>Resonant band voice: saw through a formant-like band filter (for creature vocals).</summary>
        static void Voice(float[] b, ref RNG r, float start, float dur, float f0, float f1, float formant, float gain, float vibHz = 5, float vibAmt = 0.02f, float breath = 0.1f, float curve = 1.2f, float attack = 0.03f)
        {
            int s0 = (int)(start * SR), n = (int)(dur * SR);
            float ph = 0;
            // state-variable bandpass
            float low = 0, band = 0;
            float fc = 2f * Mathf.Sin(Mathf.PI * Mathf.Min(formant, SR * 0.2f) / SR);
            float q = 0.35f;
            for (int i = 0; i < n && s0 + i < b.Length; i++)
            {
                float t = i / (float)SR;
                float f = f0 * Mathf.Pow(f1 / f0, t / dur) * (1f + vibAmt * Mathf.Sin(t * vibHz * 6.2831f));
                ph += f / SR; ph -= Mathf.Floor(ph);
                float x = (ph * 2f - 1f) + (r.NextFloat() * 2f - 1f) * breath;
                low += fc * band;
                float high = x - low - q * band;
                band += fc * high;
                float v = band * 0.8f + low * 0.3f;
                if (s0 + i >= 0) b[s0 + i] += v * gain * Env(t, dur, attack, curve);
            }
        }

        static void Metal(float[] b, ref RNG r, float start, float dur, float f, float gain)
        {
            float[] ratios = { 1f, 2.76f, 5.4f, 8.93f };
            for (int k = 0; k < ratios.Length; k++) Tone(b, ref r, start, dur / (1 + k * 0.5f), f * ratios[k], f * ratios[k] * 0.995f, gain / (1 + k), Wave.Sine, 0.001f, 3f);
        }

        static void Reverb(float[] b, float delay, float fb, float mix)
        {
            int d = (int)(delay * SR);
            if (d <= 0) return;
            var copy = (float[])b.Clone();
            for (int i = d; i < b.Length; i++) copy[i] += copy[i - d] * fb;
            for (int i = 0; i < b.Length; i++) b[i] = b[i] * (1 - mix) + copy[i] * mix;
        }

        static float[] Normalize(float[] b, float peak = 0.9f)
        {
            float m = 0; foreach (var v in b) m = Mathf.Max(m, Mathf.Abs(v));
            if (m < 1e-6f) return b;
            float g = peak / m;
            for (int i = 0; i < b.Length; i++) b[i] *= g;
            // tiny fade in/out to avoid clicks
            int f = Mathf.Min(64, b.Length / 4);
            for (int i = 0; i < f; i++) { float k = i / (float)f; b[i] *= k; b[b.Length - 1 - i] *= k; }
            return b;
        }

        // ------------------------------------------------------------------ recipes
        public static float[] Make(string name, int variant)
        {
            var r = new RNG(Hash.StringHash(name) * 977 + variant * 131 + 7);
            float vr = 1f + (variant - 1) * 0.06f;
            if (name.StartsWith("block.")) return BlockSound(name, ref r, vr);
            if (name.StartsWith("note.")) return NoteSound(name.Substring(5), ref r);
            if (name.StartsWith("entity.")) return EntitySound(name, ref r, vr);
            if (name.StartsWith("item.")) return ItemSound(name, ref r, vr);
            if (name.StartsWith("ui.")) { var b = Buf(0.08f); Noise(b, ref r, 0, 0.02f, 0.6f, 6000, 800); Tone(b, ref r, 0, 0.06f, 1400, 900, 0.5f, Wave.Square, 0.001f, 3f, 0, 0, 3000); return Normalize(b, 0.6f); }
            if (name.StartsWith("ambient.cave")) { var b = Buf(3f); Voice(b, ref r, 0, 3f, 55 * vr, 48, 300, 0.6f, 0.3f, 0.05f, 0.4f, 1f, 0.8f); Noise(b, ref r, 0.2f, 2.5f, 0.2f, 400, 60, 0.6f, 1f); Reverb(b, 0.23f, 0.55f, 0.6f); return Normalize(b, 0.5f); }
            if (name.StartsWith("weather.thunder") || name.Contains("lightning")) return Thunder(ref r);
            return Generic(ref r);
        }

        static float[] Generic(ref RNG r) { var b = Buf(0.15f); Noise(b, ref r, 0, 0.15f, 0.6f, 3000, 200); return Normalize(b, 0.5f); }

        static float[] Thunder(ref RNG r)
        {
            var b = Buf(3.5f);
            Noise(b, ref r, 0, 0.3f, 1f, 4000, 80, 0.001f, 1.5f);
            Noise(b, ref r, 0.05f, 3.4f, 1.2f, 220, 20, 0.2f, 1.3f, 0.05f);
            Tone(b, ref r, 0, 2.5f, 60, 30, 0.5f, Wave.Noise, 0.1f, 1.5f, 0, 0, 120);
            Reverb(b, 0.31f, 0.5f, 0.4f);
            return Normalize(b, 0.95f);
        }

        static float[] BlockSound(string name, ref RNG r, float vr)
        {
            var parts = name.Split('.');
            string type = parts.Length > 1 ? parts[1] : "stone";
            string ev = parts.Length > 2 ? parts[2] : "break";
            bool brk = ev == "break", step = ev == "step", place = ev == "place";
            float dur = brk ? 0.22f : step ? 0.09f : place ? 0.14f : 0.2f;
            if (!brk && !step && !place) return SpecialBlock(name, ref r, vr);
            var b = Buf(dur + 0.05f);
            switch (type)
            {
                case "wood": case "bamboo": case "cherry": case "scaffolding": case "ladder":
                    Noise(b, ref r, 0, dur, 0.5f, 1400, 150, 0.002f, 3f);
                    Tone(b, ref r, 0, dur * 0.8f, 190 * vr, 150 * vr, 0.7f, Wave.Triangle, 0.001f, 4f);
                    if (brk) Noise(b, ref r, 0.02f, dur, 0.4f, 900, 100, 0.002f, 2f, 0.02f);
                    break;
                case "gravel": case "sand": case "mud": case "soulsand": case "powder": case "snow":
                    {
                        float lp = type == "snow" || type == "powder" ? 2500 : type == "sand" ? 4000 : type == "mud" ? 900 : 3000;
                        Noise(b, ref r, 0, dur, 0.9f, lp * vr, 250, 0.01f, 1.5f, 0.018f);
                        if (type == "mud") Tone(b, ref r, 0, dur, 120, 80, 0.3f, Wave.Sine, 0.01f, 2f);
                        break;
                    }
                case "grass": case "crop": case "moss": case "nylium": case "fungus": case "coral":
                    Noise(b, ref r, 0, dur, 0.8f, 6500 * vr, 1400, 0.015f, 1.4f, 0.012f);
                    break;
                case "glass":
                    if (brk)
                    {
                        Noise(b, ref r, 0, dur, 0.5f, 9000, 2500, 0.001f, 2f, 0.01f);
                        for (int i = 0; i < 6; i++) Tone(b, ref r, r.NextFloat() * 0.08f, 0.12f, 2200 + r.NextFloat() * 3500, 2000 + r.NextFloat() * 3000, 0.25f, Wave.Sine, 0.001f, 3f);
                    }
                    else { Noise(b, ref r, 0, dur * 0.6f, 0.5f, 5000, 800, 0.001f, 3f); Tone(b, ref r, 0, dur * 0.6f, 1600 * vr, 1500 * vr, 0.3f, Wave.Sine, 0.001f, 4f); }
                    break;
                case "amethyst":
                    for (int i = 0; i < 3; i++) Tone(b, ref r, i * 0.01f, dur + 0.04f, (1800 + i * 700) * vr, (1790 + i * 700) * vr, 0.4f, Wave.Sine, 0.001f, 2.5f);
                    Noise(b, ref r, 0, 0.05f, 0.3f, 7000, 2000);
                    break;
                case "wool":
                    Noise(b, ref r, 0, dur, 0.9f, 550 * vr, 60, 0.01f, 1.5f);
                    break;
                case "metal": case "anvil": case "chain": case "lantern": case "copper":
                    Metal(b, ref r, 0, dur + 0.03f, (type == "chain" ? 900 : type == "anvil" ? 380 : 620) * vr, 0.6f);
                    Noise(b, ref r, 0, 0.04f, 0.4f, 6000, 1000);
                    break;
                case "slime": case "honey":
                    Tone(b, ref r, 0, dur, 160 * vr, 90 * vr, 0.8f, Wave.Sine, 0.01f, 1.5f, 18, 0.2f);
                    Noise(b, ref r, 0, dur, 0.3f, 700, 80, 0.01f, 2f);
                    break;
                case "sculk":
                    Noise(b, ref r, 0, dur, 0.7f, 1200, 100, 0.01f, 1.6f, 0.03f);
                    Tone(b, ref r, 0, dur, 90, 60, 0.4f, Wave.Sine);
                    break;
                case "water": case "lava":
                    Noise(b, ref r, 0, dur, 0.8f, 2500, 300, 0.01f, 1.5f, 0.01f);
                    break;
                case "netherrack": case "netherbricks": case "basalt": case "bone": case "deepslate": case "tuff": case "calcite":
                    Noise(b, ref r, 0, dur, 0.8f, (type == "bone" ? 3500 : 2400) * vr, 250, 0.001f, 2.5f, brk ? 0.01f : 0);
                    Tone(b, ref r, 0, dur * 0.6f, 110 * vr, 70, 0.5f, Wave.Sine, 0.001f, 3f);
                    break;
                default: // stone
                    Noise(b, ref r, 0, dur, 0.8f, 3200 * vr, 300, 0.001f, 2.5f, brk ? 0.012f : 0);
                    Tone(b, ref r, 0, dur * 0.5f, 140 * vr, 80, 0.45f, Wave.Sine, 0.001f, 3f);
                    break;
            }
            return Normalize(b, step ? 0.55f : 0.8f);
        }

        static float[] SpecialBlock(string name, ref RNG r, float vr)
        {
            float[] b;
            if (name.Contains("door") || name.Contains("trapdoor") || name.Contains("fence_gate"))
            {
                bool iron = name.Contains("iron");
                bool open = name.EndsWith("open");
                b = Buf(0.35f);
                if (iron) { Metal(b, ref r, 0, 0.3f, open ? 300 : 260, 0.6f); Noise(b, ref r, 0, 0.1f, 0.4f, 1500, 100); }
                else
                {
                    if (open) Voice(b, ref r, 0, 0.25f, 380, 520, 1400, 0.4f, 30, 0.05f, 0.4f, 1.5f, 0.01f);
                    Noise(b, ref r, open ? 0.15f : 0, 0.12f, 0.8f, 1100, 80, 0.001f, 3f);
                    Tone(b, ref r, open ? 0.15f : 0, 0.12f, 150, 110, 0.5f, Wave.Triangle);
                }
                return Normalize(b, 0.7f);
            }
            if (name.Contains("chest") || name.Contains("barrel") || name.Contains("shulker"))
            {
                bool open = name.EndsWith("open");
                b = Buf(0.5f);
                Voice(b, ref r, 0, 0.4f, open ? 220 : 260, open ? 300 : 180, 900, 0.5f, 25, 0.04f, 0.5f, 1.4f, 0.02f);
                if (!open) { Noise(b, ref r, 0.3f, 0.12f, 0.8f, 900, 80); Tone(b, ref r, 0.3f, 0.1f, 120, 90, 0.5f, Wave.Triangle); }
                if (name.Contains("ender")) Tone(b, ref r, 0, 0.45f, 300, 150, 0.3f, Wave.Sine, 0.05f, 1f, 6, 0.1f);
                return Normalize(b, 0.7f);
            }
            if (name.Contains("button") || name.Contains("lever") || name.Contains("pressure_plate") || name.Contains("tripwire") || name.Contains("click") || name.Contains("copper_bulb"))
            {
                b = Buf(0.08f);
                bool on = !name.EndsWith("off");
                Tone(b, ref r, 0, 0.05f, on ? 1800 : 1400, on ? 1200 : 900, 0.6f, Wave.Square, 0.001f, 4f, 0, 0, 4000);
                Noise(b, ref r, 0, 0.02f, 0.5f, 7000, 1500);
                return Normalize(b, 0.6f);
            }
            if (name.Contains("piston"))
            {
                b = Buf(0.3f);
                Noise(b, ref r, 0, 0.25f, 0.7f, 2500, 200, 0.02f, 2f);
                Tone(b, ref r, 0, 0.25f, name.Contains("extend") ? 120 : 160, name.Contains("extend") ? 200 : 90, 0.5f, Wave.Saw, 0.01f, 2f, 0, 0, 800);
                return Normalize(b, 0.6f);
            }
            if (name.Contains("fire.ambient") || name.Contains("crackle"))
            {
                b = Buf(1f);
                for (int i = 0; i < 14; i++) Noise(b, ref r, r.NextFloat() * 0.95f, 0.01f + r.NextFloat() * 0.02f, 0.4f + r.NextFloat() * 0.6f, 6000, 1500);
                Noise(b, ref r, 0, 1f, 0.15f, 800, 100, 0.2f, 0.5f);
                return Normalize(b, 0.5f);
            }
            if (name.Contains("extinguish"))
            {
                b = Buf(0.6f); Noise(b, ref r, 0, 0.6f, 0.8f, 7000, 1500, 0.01f, 1.5f); return Normalize(b, 0.5f);
            }
            if (name.Contains("lava.pop")) { b = Buf(0.12f); Tone(b, ref r, 0, 0.1f, 300, 900, 0.8f, Wave.Sine, 0.001f, 3f); return Normalize(b, 0.5f); }
            if (name.Contains("lava.ambient")) { b = Buf(0.8f); Noise(b, ref r, 0, 0.8f, 0.6f, 500, 40, 0.2f, 1f, 0.05f); return Normalize(b, 0.4f); }
            if (name.Contains("water.ambient")) { b = Buf(1.2f); Noise(b, ref r, 0, 1.2f, 0.6f, 1800, 300, 0.3f, 1f, 0.03f); return Normalize(b, 0.3f); }
            if (name.Contains("portal"))
            {
                if (name.Contains("ambient")) { b = Buf(3f); Tone(b, ref r, 0, 3f, 90, 110, 0.4f, Wave.Saw, 0.8f, 0.8f, 0.7f, 0.2f, 600); Noise(b, ref r, 0, 3f, 0.3f, 2500, 400, 1f, 0.8f); Reverb(b, 0.17f, 0.5f, 0.5f); return Normalize(b, 0.4f); }
                b = Buf(4f); Tone(b, ref r, 0, 4f, 60, 400, 0.5f, Wave.Saw, 1f, 0.6f, 2, 0.1f, 1200); Noise(b, ref r, 0, 4f, 0.3f, 3000, 300, 1.5f, 0.8f); Reverb(b, 0.2f, 0.6f, 0.5f); return Normalize(b, 0.6f);
            }
            if (name.Contains("end_portal"))
            {
                b = Buf(3f);
                foreach (var f in new[] { 220f, 277f, 330f, 440f, 554f }) Tone(b, ref r, 0, 3f, f, f, 0.3f, Wave.Sine, 0.1f, 1.3f, 3, 0.01f);
                Reverb(b, 0.25f, 0.6f, 0.5f); return Normalize(b, 0.7f);
            }
            if (name.Contains("end_portal_frame")) { b = Buf(0.8f); Metal(b, ref r, 0, 0.7f, 520, 0.6f); Tone(b, ref r, 0, 0.7f, 880, 880, 0.3f); return Normalize(b, 0.6f); }
            if (name.Contains("bell")) { b = Buf(2.5f); Metal(b, ref r, 0, 2.4f, 440, 0.8f); Reverb(b, 0.21f, 0.4f, 0.3f); return Normalize(b, 0.8f); }
            if (name.Contains("anvil")) { b = Buf(0.6f); Metal(b, ref r, 0, 0.55f, 700, 0.8f); return Normalize(b, 0.7f); }
            if (name.Contains("brewing")) { b = Buf(1f); for (int i = 0; i < 8; i++) Tone(b, ref r, i * 0.1f + r.NextFloat() * 0.05f, 0.08f, 400 + r.NextFloat() * 400, 900 + r.NextFloat() * 500, 0.4f); return Normalize(b, 0.5f); }
            if (name.Contains("composter") || name.Contains("berry") || name.Contains("cave_vines")) { b = Buf(0.25f); Noise(b, ref r, 0, 0.2f, 0.7f, 3000, 400, 0.01f, 2f, 0.015f); return Normalize(b, 0.6f); }
            if (name.Contains("respawn_anchor")) { b = Buf(1f); Tone(b, ref r, 0, 0.9f, 120, 360, 0.6f, Wave.Saw, 0.05f, 1.2f, 4, 0.05f, 900); Reverb(b, 0.15f, 0.5f, 0.4f); return Normalize(b, 0.7f); }
            if (name.Contains("sculk") || name.Contains("shrieker")) { b = Buf(1.2f); Voice(b, ref r, 0, 1.1f, 200, 480, 1200, 0.6f, 12, 0.06f, 0.5f, 1f, 0.05f); return Normalize(b, 0.7f); }
            if (name.Contains("dispenser")) { b = Buf(0.15f); Tone(b, ref r, 0, 0.1f, 1200, 1100, 0.5f, Wave.Square, 0.001f, 3f, 0, 0, 3000); return Normalize(b, 0.5f); }
            if (name.Contains("carve") || name.Contains("strip") || name.Contains("scrape")) { b = Buf(0.3f); Noise(b, ref r, 0, 0.28f, 0.8f, 3500, 500, 0.02f, 1.2f, 0.006f); return Normalize(b, 0.6f); }
            if (name.Contains("bookshelf") || name.Contains("book")) { b = Buf(0.2f); Noise(b, ref r, 0, 0.18f, 0.7f, 2000, 200, 0.005f, 2f); return Normalize(b, 0.5f); }
            b = Buf(0.18f); Noise(b, ref r, 0, 0.16f, 0.7f, 2500, 200, 0.002f, 2.5f); return Normalize(b, 0.6f);
        }

        static float[] NoteSound(string inst, ref RNG r)
        {
            var b = Buf(1.2f);
            float f = 370f; // F#4 base; AudioSource pitch shifts the note
            switch (inst)
            {
                case "bass": Tone(b, ref r, 0, 1f, f / 4, f / 4, 0.9f, Wave.Triangle, 0.002f, 3f); break;
                case "basedrum": Tone(b, ref r, 0, 0.3f, 120, 45, 1f, Wave.Sine, 0.001f, 2f); break;
                case "snare": Noise(b, ref r, 0, 0.25f, 0.9f, 7000, 1200, 0.001f, 3f); break;
                case "hat": Noise(b, ref r, 0, 0.08f, 0.9f, 10000, 5000, 0.001f, 3f); break;
                case "bell": Metal(b, ref r, 0, 1.1f, f * 2, 0.7f); break;
                case "chime": Tone(b, ref r, 0, 1.1f, f * 2, f * 2, 0.7f, Wave.Sine, 0.001f, 2f); Tone(b, ref r, 0, 0.8f, f * 5.4f, f * 5.4f, 0.2f); break;
                case "flute": Tone(b, ref r, 0, 0.8f, f, f, 0.7f, Wave.Sine, 0.05f, 1.5f, 5, 0.01f); Noise(b, ref r, 0, 0.6f, 0.05f, 4000, 1500, 0.05f); break;
                case "guitar": Tone(b, ref r, 0, 0.9f, f / 2, f / 2, 0.8f, Wave.Saw, 0.002f, 4f, 0, 0, 1800); break;
                case "xylophone": Tone(b, ref r, 0, 0.4f, f * 2, f * 2, 0.8f, Wave.Sine, 0.001f, 5f); Tone(b, ref r, 0, 0.2f, f * 8, f * 8, 0.2f, Wave.Sine, 0.001f, 6f); break;
                case "iron_xylophone": Metal(b, ref r, 0, 0.6f, f, 0.7f); break;
                case "cow_bell": Metal(b, ref r, 0, 0.5f, f * 1.5f, 0.7f); break;
                case "didgeridoo": Voice(b, ref r, 0, 1f, f / 4, f / 4, 400, 0.7f, 3, 0.02f, 0.1f, 1f); break;
                case "bit": Tone(b, ref r, 0, 0.5f, f, f, 0.6f, Wave.Square, 0.001f, 2f); break;
                case "banjo": Tone(b, ref r, 0, 0.6f, f, f, 0.7f, Wave.Saw, 0.001f, 5f, 0, 0, 3500); break;
                case "pling": Tone(b, ref r, 0, 1f, f, f, 0.7f, Wave.Triangle, 0.001f, 2f); Tone(b, ref r, 0, 1f, f * 2, f * 2, 0.3f, Wave.Sine, 0.001f, 2f); break;
                default: Tone(b, ref r, 0, 1.1f, f, f, 0.8f, Wave.Triangle, 0.002f, 3.5f); Tone(b, ref r, 0, 0.6f, f * 2, f * 2, 0.2f, Wave.Sine, 0.002f, 4f); break; // harp
            }
            return Normalize(b, 0.8f);
        }

        // creature voice table: base pitch, formant, duration
        static readonly Dictionary<string, (float f, float formant, float dur)> Voices = new Dictionary<string, (float, float, float)>
        {
            {"player", (230, 900, 0.18f)}, {"zombie", (95, 600, 0.9f)}, {"husk", (85, 500, 0.9f)}, {"drowned", (90, 450, 0.9f)}, {"zombie_villager", (110, 700, 0.9f)},
            {"villager", (170, 1100, 0.45f)}, {"wandering_trader", (180, 1000, 0.45f)}, {"pillager", (150, 900, 0.4f)}, {"vindicator", (140, 850, 0.4f)}, {"evoker", (160, 950, 0.5f)}, {"witch", (240, 1400, 0.5f)},
            {"piglin", (140, 700, 0.35f)}, {"piglin_brute", (120, 650, 0.4f)}, {"zombified_piglin", (110, 600, 0.5f)}, {"hoglin", (80, 450, 0.5f)}, {"zoglin", (75, 420, 0.5f)},
            {"cow", (105, 650, 1.0f)}, {"mooshroom", (110, 650, 1.0f)}, {"pig", (160, 700, 0.3f)}, {"sheep", (290, 1300, 0.7f)}, {"goat", (330, 1400, 0.6f)}, {"chicken", (650, 2000, 0.12f)},
            {"horse", (320, 1500, 0.8f)}, {"donkey", (250, 1200, 0.9f)}, {"mule", (270, 1300, 0.8f)}, {"llama", (360, 1500, 0.4f)}, {"camel", (150, 700, 0.6f)},
            {"wolf", (420, 1600, 0.2f)}, {"cat", (600, 2000, 0.5f)}, {"ocelot", (650, 2100, 0.4f)}, {"fox", (700, 2200, 0.2f)}, {"rabbit", (900, 2500, 0.1f)}, {"parrot", (1200, 3000, 0.2f)},
            {"panda", (200, 900, 0.5f)}, {"polar_bear", (130, 600, 0.6f)}, {"sniffer", (140, 700, 0.8f)}, {"armadillo", (400, 1600, 0.2f)}, {"frog", (220, 800, 0.2f)}, {"axolotl", (800, 2400, 0.2f)},
            {"iron_golem", (70, 400, 0.4f)}, {"snow_golem", (300, 1200, 0.3f)}, {"allay", (1100, 3200, 0.4f)}, {"bee", (220, 1800, 0.6f)}, {"dolphin", (1500, 3500, 0.3f)},
            {"enderman", (140, 800, 0.8f)}, {"blaze", (180, 900, 0.8f)}, {"ghast", (420, 1300, 1.2f)}, {"ender_dragon", (80, 500, 2.2f)}, {"wither", (70, 450, 1.8f)}, {"warden", (55, 300, 1.4f)},
            {"guardian", (260, 1100, 0.6f)}, {"elder_guardian", (180, 900, 1.0f)}, {"ravager", (70, 400, 0.8f)}, {"phantom", (500, 1700, 0.6f)}, {"breeze", (600, 2500, 0.5f)}, {"creaking", (90, 500, 0.8f)},
            {"strider", (200, 900, 0.5f)}, {"turtle", (160, 700, 0.3f)}, {"squid", (300, 900, 0.3f)}, {"glow_squid", (320, 1000, 0.3f)}, {"bat", (2500, 4000, 0.08f)}, {"vex", (900, 2600, 0.3f)},
            {"silverfish", (1800, 3500, 0.2f)}, {"endermite", (1700, 3300, 0.2f)}, {"spider", (0, 0, 0.5f)}, {"cave_spider", (0, 0, 0.4f)}, {"skeleton", (0, 0, 0.3f)}, {"stray", (0, 0, 0.3f)}, {"wither_skeleton", (0, 0, 0.35f)}, {"bogged", (0, 0, 0.3f)},
            {"creeper", (0, 0, 0.3f)}, {"slime", (0, 0, 0.3f)}, {"magma_cube", (0, 0, 0.3f)}, {"shulker", (0, 0, 0.3f)},
        };

        static float[] EntitySound(string name, ref RNG r, float vr)
        {
            var parts = name.Split('.');
            string who = parts.Length > 1 ? parts[1] : "generic";
            string ev = parts.Length > 2 ? string.Join(".", parts, 2, parts.Length - 2) : "";
            float[] b;
            // ---- non-vocal generic events
            if (who == "generic" || who == "player")
            {
                switch (ev)
                {
                    case "explode":
                        b = Buf(2.2f);
                        Noise(b, ref r, 0, 0.4f, 1f, 5000, 60, 0.001f, 1.5f);
                        Noise(b, ref r, 0.02f, 2.1f, 1.3f, 320, 20, 0.005f, 1.6f, 0.03f);
                        Tone(b, ref r, 0, 0.8f, 70, 28, 0.9f, Wave.Sine, 0.002f, 1.5f);
                        Reverb(b, 0.19f, 0.45f, 0.35f);
                        return Normalize(b, 0.98f);
                    case "eat":
                        b = Buf(0.18f); Noise(b, ref r, 0, 0.16f, 0.9f, 2600 * vr, 300, 0.002f, 2f, 0.01f); return Normalize(b, 0.6f);
                    case "drink":
                        b = Buf(0.3f); Tone(b, ref r, 0, 0.12f, 300, 180, 0.6f, Wave.Sine, 0.01f, 2f); Tone(b, ref r, 0.15f, 0.12f, 320, 190, 0.6f, Wave.Sine, 0.01f, 2f); return Normalize(b, 0.5f);
                    case "burp":
                        b = Buf(0.35f); Voice(b, ref r, 0, 0.3f, 110, 90, 500, 0.7f, 20, 0.08f, 0.3f); return Normalize(b, 0.5f);
                    case "extinguish_fire": b = Buf(0.5f); Noise(b, ref r, 0, 0.5f, 0.7f, 8000, 2000, 0.01f, 1.5f); return Normalize(b, 0.4f);
                    case "splash": case "swim": b = Buf(0.5f); Noise(b, ref r, 0, 0.45f, 0.8f, 3000, 250, 0.005f, 1.5f, 0.01f); return Normalize(b, 0.6f);
                    case "small_fall": case "big_fall": b = Buf(0.25f); Noise(b, ref r, 0, 0.2f, 0.8f, 900, 60, 0.001f, 2f); Tone(b, ref r, 0, 0.15f, 90, 50, 0.7f); return Normalize(b, 0.7f);
                    case "levelup":
                        b = Buf(0.9f);
                        foreach (var (t, f) in new[] { (0f, 523f), (0.12f, 659f), (0.24f, 784f), (0.36f, 1046f) }) Tone(b, ref r, t, 0.5f, f, f, 0.5f, Wave.Triangle, 0.002f, 2f);
                        return Normalize(b, 0.7f);
                    case "attack.strong": case "attack.sweep": b = Buf(0.18f); Noise(b, ref r, 0, 0.16f, 0.9f, 2500, 200, 0.002f, 2f); Tone(b, ref r, 0, 0.1f, 160, 80, 0.5f); return Normalize(b, 0.6f);
                    case "attack.weak": case "attack.nodamage": b = Buf(0.1f); Noise(b, ref r, 0, 0.08f, 0.8f, 1500, 200, 0.002f, 3f); return Normalize(b, 0.4f);
                    case "attack.crit": b = Buf(0.2f); Noise(b, ref r, 0, 0.1f, 0.8f, 6000, 800); Metal(b, ref r, 0, 0.15f, 1500, 0.3f); return Normalize(b, 0.6f);
                    case "attack.knockback": b = Buf(0.2f); Noise(b, ref r, 0, 0.15f, 1f, 1200, 80); return Normalize(b, 0.6f);
                }
            }
            switch (who)
            {
                case "item":
                    b = Buf(0.1f); Tone(b, ref r, 0, 0.08f, 500 * vr, 1400 * vr, 0.7f, Wave.Sine, 0.002f, 2f); return Normalize(b, 0.5f);
                case "experience_orb":
                    b = Buf(0.35f); Tone(b, ref r, 0, 0.3f, 1900 * vr, 1900 * vr, 0.6f, Wave.Sine, 0.001f, 2.5f); Tone(b, ref r, 0, 0.2f, 3800 * vr, 3800 * vr, 0.2f, Wave.Sine, 0.001f, 3f); return Normalize(b, 0.5f);
                case "arrow":
                    if (ev == "shoot") { b = Buf(0.3f); Tone(b, ref r, 0, 0.25f, 330, 300, 0.7f, Wave.Triangle, 0.001f, 4f); Noise(b, ref r, 0, 0.2f, 0.4f, 3000, 600, 0.01f, 2f); return Normalize(b, 0.6f); }
                    b = Buf(0.15f); Noise(b, ref r, 0, 0.1f, 0.8f, 1500, 150, 0.001f, 3f); Tone(b, ref r, 0, 0.1f, 180, 120, 0.6f, Wave.Triangle, 0.001f, 3f); return Normalize(b, 0.6f);
                case "tnt": b = Buf(1.2f); Noise(b, ref r, 0, 1.2f, 0.7f, 9000, 3000, 0.05f, 0.6f); return Normalize(b, 0.5f);
                case "lightning_bolt": return Thunder(ref r);
                case "ender_pearl": case "snowball": case "egg": case "ender_eye": case "splash_potion": case "fishing_bobber": case "wind_charge":
                    if (ev.Contains("retrieve")) { b = Buf(0.2f); Noise(b, ref r, 0, 0.18f, 0.6f, 2500, 400, 0.02f, 1.5f); return Normalize(b, 0.4f); }
                    b = Buf(0.3f); Noise(b, ref r, 0, 0.28f, 0.8f, 2200 * vr, 500, 0.05f, 1.2f); return Normalize(b, 0.4f);
                case "enderman": case "endermite":
                    if (ev == "teleport") { b = Buf(0.6f); Tone(b, ref r, 0, 0.55f, 200, 900, 0.7f, Wave.Saw, 0.01f, 1.5f, 9, 0.1f, 1600); Tone(b, ref r, 0, 0.55f, 1000, 250, 0.4f, Wave.Sine, 0.01f, 1.5f); return Normalize(b, 0.6f); }
                    break;
                case "cow": if (ev == "milk") { b = Buf(0.4f); Noise(b, ref r, 0, 0.35f, 0.6f, 2000, 300, 0.02f, 1.5f); return Normalize(b, 0.5f); } break;
                case "firework_rocket":
                    if (ev.Contains("launch")) { b = Buf(0.6f); Noise(b, ref r, 0, 0.55f, 0.7f, 6000, 1500, 0.01f, 1f); return Normalize(b, 0.5f); }
                    b = Buf(1.5f); Noise(b, ref r, 0, 0.3f, 1f, 6000, 200, 0.001f, 2f); for (int i = 0; i < 20; i++) Noise(b, ref r, 0.3f + r.NextFloat() * 1f, 0.01f, 0.3f, 9000, 3000); Reverb(b, 0.2f, 0.4f, 0.3f); return Normalize(b, 0.8f);
                case "boat": case "minecart":
                    b = Buf(0.6f); Noise(b, ref r, 0, 0.6f, 0.6f, who == "boat" ? 1500 : 3000, 150, 0.1f, 1f, 0.02f); return Normalize(b, 0.3f);
                case "zombie": case "husk": case "drowned":
                    if (ev.Contains("attack_wooden_door") || ev.Contains("break_wooden_door")) { b = Buf(0.3f); Noise(b, ref r, 0, 0.25f, 1f, 1200, 80, 0.001f, 2.5f); Tone(b, ref r, 0, 0.2f, 140, 90, 0.6f, Wave.Triangle); return Normalize(b, 0.8f); }
                    break;
            }
            // ---- vocal sounds (hurt/death/ambient)
            Voices.TryGetValue(who, out var v);
            if (v.dur <= 0) v = (200, 900, 0.3f);
            bool hurt = ev.Contains("hurt"), death = ev.Contains("death"), ambient = ev.Contains("ambient") || ev == "" || ev.Contains("say") || ev.Contains("yes") || ev.Contains("no") || ev.Contains("celebrate");
            float dur = hurt ? Mathf.Min(0.3f, v.dur * 0.6f) : death ? v.dur * 1.5f : v.dur;
            b = Buf(dur + 0.1f);
            float f0 = v.f * vr;
            switch (who)
            {
                case "skeleton": case "stray": case "wither_skeleton": case "bogged":
                    for (int i = 0; i < (death ? 9 : 5); i++) Noise(b, ref r, i * dur / 6f + r.NextFloat() * 0.02f, 0.02f, 0.8f, 4000, 1200, 0.001f, 4f);
                    Tone(b, ref r, 0, dur * 0.4f, who == "wither_skeleton" ? 180 : 320, 250, 0.2f, Wave.Square, 0.001f, 3f, 0, 0, 1500);
                    return Normalize(b, 0.6f);
                case "spider": case "cave_spider":
                    Noise(b, ref r, 0, dur, 0.8f, 5000, 1500, 0.02f, 1.2f, 0.01f);
                    for (int i = 0; i < 4; i++) Noise(b, ref r, r.NextFloat() * dur, 0.03f, 0.6f, 3000, 800);
                    return Normalize(b, 0.6f);
                case "creeper":
                    if (ev.Contains("primed")) { b = Buf(1.3f); Noise(b, ref r, 0, 1.3f, 0.8f, 8000, 2500, 0.05f, 0.5f); return Normalize(b, 0.6f); }
                    Noise(b, ref r, 0, dur, 0.6f, 1400, 150, 0.005f, 2f); return Normalize(b, 0.5f);
                case "slime": case "magma_cube":
                    Tone(b, ref r, 0, dur, 140, 80, 0.8f, Wave.Sine, 0.01f, 1.5f, 15, 0.25f); Noise(b, ref r, 0, dur, 0.3f, 800, 60); return Normalize(b, 0.6f);
                case "shulker":
                    Noise(b, ref r, 0, dur, 0.6f, 1600, 200, 0.01f, 2f); Tone(b, ref r, 0, dur, 300, 200, 0.4f, Wave.Triangle); return Normalize(b, 0.5f);
                case "chicken":
                    for (int i = 0; i < (death ? 4 : 2); i++) Voice(b, ref r, i * 0.07f, 0.06f, f0 * (1 + i * 0.1f), f0 * 0.8f, v.formant, 0.7f, 0, 0, 0.1f, 2f, 0.005f);
                    return Normalize(b, 0.6f);
                case "pig": case "hoglin": case "zoglin":
                    Voice(b, ref r, 0, dur * 0.45f, f0, f0 * 0.8f, v.formant, 0.8f, 30, 0.1f, 0.35f, 1.5f, 0.01f);
                    Voice(b, ref r, dur * 0.5f, dur * 0.4f, f0 * 0.95f, f0 * 0.75f, v.formant, 0.7f, 30, 0.1f, 0.35f, 1.5f, 0.01f);
                    return Normalize(b, 0.7f);
                case "wolf":
                    Voice(b, ref r, 0, dur, f0 * (hurt ? 1.4f : 1), f0 * 0.7f, v.formant, 0.9f, 0, 0, 0.3f, 2f, 0.005f);
                    return Normalize(b, 0.7f);
                case "blaze":
                    Noise(b, ref r, 0, dur, 0.7f, 1200, 100, 0.2f, 1f, 0.05f); Tone(b, ref r, 0, dur, 110, 90, 0.3f, Wave.Saw, 0.2f, 1f, 3, 0.05f, 400); return Normalize(b, 0.5f);
                case "ender_dragon":
                    Noise(b, ref r, 0, dur, 0.8f, 900, 50, 0.1f, 1f, 0.03f);
                    Voice(b, ref r, 0, dur, f0, f0 * 0.6f, v.formant, 1f, 7, 0.08f, 0.6f, 1f, 0.1f);
                    Reverb(b, 0.23f, 0.5f, 0.4f); return Normalize(b, 0.95f);
                case "warden":
                    if (ev.Contains("heartbeat")) { b = Buf(0.5f); Tone(b, ref r, 0, 0.12f, 55, 40, 1f); Tone(b, ref r, 0.2f, 0.12f, 50, 38, 0.8f); return Normalize(b, 0.8f); }
                    Noise(b, ref r, 0, dur, 0.6f, 600, 40, 0.1f, 1f, 0.04f); Voice(b, ref r, 0, dur, f0, f0 * 0.8f, v.formant, 1f, 4, 0.1f, 0.7f, 1f, 0.1f); return Normalize(b, 0.9f);
            }
            if (hurt) Voice(b, ref r, 0, dur, f0 * 1.25f, f0 * 0.9f, v.formant, 0.9f, 20, 0.08f, 0.25f, 1.5f, 0.005f);
            else if (death) Voice(b, ref r, 0, dur, f0 * 1.1f, f0 * 0.55f, v.formant, 0.9f, 6, 0.05f, 0.25f, 1.1f, 0.01f);
            else
            {
                float f1 = who == "cow" ? f0 * 0.85f : who == "sheep" || who == "goat" ? f0 * 0.95f : who == "cat" ? f0 * 0.75f : who == "villager" ? f0 * (r.NextBool() ? 1.2f : 0.85f) : f0 * 0.9f;
                float vib = who == "sheep" || who == "goat" ? 11f : who == "ghast" ? 3f : 5f;
                float vibA = who == "sheep" || who == "goat" ? 0.08f : 0.03f;
                Voice(b, ref r, 0, dur, f0, f1, v.formant, 0.9f, vib, vibA, who == "zombie" || who == "husk" || who == "drowned" ? 0.5f : 0.2f, 1.1f, who == "cow" ? 0.1f : 0.03f);
                if (who == "cat") Voice(b, ref r, 0, dur, f0 * 1.5f, f0, v.formant * 1.5f, 0.3f);
            }
            if (who == "ghast" || who == "wither" || who == "elder_guardian") Reverb(b, 0.2f, 0.5f, 0.4f);
            return Normalize(b, 0.75f);
        }

        static float[] ItemSound(string name, ref RNG r, float vr)
        {
            float[] b;
            if (name.Contains("armor.equip"))
            {
                b = Buf(0.35f);
                if (name.Contains("leather") || name.Contains("elytra")) Noise(b, ref r, 0, 0.3f, 0.8f, 1500, 150, 0.02f, 1.5f, 0.02f);
                else { Noise(b, ref r, 0, 0.25f, 0.5f, 5000, 800, 0.01f, 2f, 0.01f); Metal(b, ref r, 0.02f, 0.3f, name.Contains("gold") ? 1200 : name.Contains("diamond") ? 1600 : 800, 0.3f); }
                return Normalize(b, 0.6f);
            }
            if (name.Contains("bucket"))
            {
                b = Buf(0.5f);
                bool lava = name.Contains("lava");
                Noise(b, ref r, 0, 0.45f, 0.8f, lava ? 900 : 2800, lava ? 60 : 300, 0.01f, 1.5f, 0.012f);
                if (lava) Tone(b, ref r, 0, 0.4f, 100, 60, 0.4f);
                return Normalize(b, 0.6f);
            }
            if (name == "item.break") { b = Buf(0.3f); Noise(b, ref r, 0, 0.25f, 0.9f, 4000, 500, 0.001f, 2.5f, 0.01f); Metal(b, ref r, 0, 0.2f, 900, 0.3f); return Normalize(b, 0.7f); }
            if (name.Contains("flintandsteel") || name.Contains("firecharge")) { b = Buf(0.2f); Noise(b, ref r, 0, 0.05f, 1f, 9000, 3000); Noise(b, ref r, 0.03f, 0.15f, 0.5f, 3000, 400); return Normalize(b, 0.6f); }
            if (name.Contains("crossbow"))
            {
                b = Buf(0.35f);
                if (name.Contains("shoot")) { Tone(b, ref r, 0, 0.2f, 260, 200, 0.7f, Wave.Triangle, 0.001f, 4f); Noise(b, ref r, 0, 0.1f, 0.8f, 2500, 300); }
                else { Noise(b, ref r, 0, 0.3f, 0.6f, 3000, 400, 0.05f, 1f, 0.004f); Metal(b, ref r, 0.2f, 0.12f, 1300, 0.3f); }
                return Normalize(b, 0.6f);
            }
            if (name.Contains("trident")) { b = Buf(0.6f); Noise(b, ref r, 0, 0.5f, 0.7f, 3000, 400, 0.02f, 1.2f); Metal(b, ref r, 0, 0.5f, 700, 0.3f); return Normalize(b, 0.7f); }
            if (name.Contains("spear")) { b = Buf(0.3f); Noise(b, ref r, 0, 0.25f, 0.8f, 2500, 250, 0.005f, 2f); Tone(b, ref r, 0, 0.15f, 180, 90, 0.5f); return Normalize(b, 0.7f); }
            if (name.Contains("hoe") || name.Contains("shovel") || name.Contains("axe")) { b = Buf(0.25f); Noise(b, ref r, 0, 0.22f, 0.8f, 2200 * vr, 250, 0.005f, 1.8f, 0.012f); return Normalize(b, 0.6f); }
            if (name.Contains("bottle")) { b = Buf(0.3f); Tone(b, ref r, 0, 0.25f, 700, 1300, 0.5f, Wave.Sine, 0.01f, 1.5f, 10, 0.05f); Noise(b, ref r, 0, 0.25f, 0.3f, 2500, 400); return Normalize(b, 0.5f); }
            if (name.Contains("book")) { b = Buf(0.2f); Noise(b, ref r, 0, 0.18f, 0.7f, 2500, 300, 0.005f, 2f); return Normalize(b, 0.5f); }
            if (name.Contains("totem")) { b = Buf(1.5f); foreach (var f in new[] { 523f, 659f, 784f, 1046f, 1318f }) Tone(b, ref r, 0, 1.4f, f, f * 1.01f, 0.3f, Wave.Triangle, 0.02f, 1.3f, 5, 0.01f); Reverb(b, 0.2f, 0.5f, 0.4f); return Normalize(b, 0.8f); }
            if (name.Contains("goat_horn")) { b = Buf(2f); Voice(b, ref r, 0, 1.9f, 220, 200, 800, 1f, 4, 0.02f, 0.05f, 0.8f, 0.1f); Reverb(b, 0.25f, 0.5f, 0.4f); return Normalize(b, 0.9f); }
            b = Buf(0.15f); Noise(b, ref r, 0, 0.12f, 0.7f, 3000, 300, 0.005f, 2f); return Normalize(b, 0.5f);
        }

        // ------------------------------------------------------------------ loops
        public static float[] RainLoop()
        {
            var r = new RNG(99);
            var b = Buf(4f);
            Noise(b, ref r, 0, 4f, 0.5f, 5000, 700, 0, 0);
            for (int i = 0; i < 300; i++) Noise(b, ref r, r.NextFloat() * 3.95f, 0.006f, 0.3f + r.NextFloat() * 0.5f, 7000, 1500, 0.001f, 3f);
            // crossfade loop ends
            int f = SR / 4;
            for (int i = 0; i < f; i++) { float k = i / (float)f; b[i] = b[i] * k + b[b.Length - f + i] * (1 - k); }
            Array.Resize(ref b, b.Length - f);
            return Normalize(b, 0.6f);
        }
        public static float[] NetherLoop()
        {
            var r = new RNG(123); var b = Buf(8f);
            Tone(b, ref r, 0, 8f, 45, 50, 0.5f, Wave.Saw, 2f, 0.5f, 0.2f, 0.1f, 200);
            Noise(b, ref r, 0, 8f, 0.4f, 300, 30, 2f, 0.5f, 0.1f);
            Reverb(b, 0.3f, 0.6f, 0.5f);
            return Normalize(b, 0.5f);
        }
        public static float[] EndLoop()
        {
            var r = new RNG(321); var b = Buf(8f);
            Tone(b, ref r, 0, 8f, 110, 104, 0.4f, Wave.Sine, 2f, 0.5f, 0.1f, 0.05f);
            Tone(b, ref r, 0, 8f, 165, 160, 0.3f, Wave.Sine, 2f, 0.5f, 0.13f, 0.05f);
            Noise(b, ref r, 0, 8f, 0.15f, 1500, 300, 2f, 0.5f);
            Reverb(b, 0.35f, 0.6f, 0.6f);
            return Normalize(b, 0.4f);
        }
    }

    /// <summary>Procedural ambient music (original compositions generated from seeds) and music discs.</summary>
    public static class MusicGen
    {
        const int SR = Sounds.SR;
        static readonly Dictionary<string, AudioClip> cache = new Dictionary<string, AudioClip>();

        public static AudioClip Disc(string disc)
        {
            string key = "disc_" + disc;
            if (cache.TryGetValue(key, out var c)) return c;
            c = Sounds.MakeClip(key, Compose(Hash.StringHash(disc), 60f, disc == "pigstep" || disc == "otherside" ? 1 : disc == "11" || disc == "13" ? 2 : 0), false);
            cache[key] = c; return c;
        }

        public static AudioClip Ambient(string mood, int piece)
        {
            string key = mood + "_" + (piece % 6);
            if (cache.TryGetValue(key, out var c)) return c;
            int style = mood == "nether" ? 2 : mood == "end" ? 3 : 0;
            c = Sounds.MakeClip(key, Compose(Hash.StringHash(key), 75f, style), false);
            cache[key] = c; return c;
        }

        /// <summary>Soft piano-like generative piece: chord progression + sparse melody + reverb.</summary>
        static float[] Compose(int seed, float seconds, int style)
        {
            var r = new RNG(seed);
            var b = new float[(int)(seconds * SR)];
            int[] scale = style == 2 ? new[] { 0, 1, 3, 5, 7, 8, 10 } : style == 3 ? new[] { 0, 2, 3, 7, 9 } : style == 1 ? new[] { 0, 3, 5, 6, 7, 10 } : new[] { 0, 2, 4, 7, 9, 11 };
            float root = style == 2 ? 110f : style == 3 ? 130.8f : 146.8f * (1 + r.Next(3) * 0.12246f);
            float tempo = style == 1 ? 0.35f : 0.75f + r.NextFloat() * 0.3f;
            int[] prog = { 0, 5, 3, 4, 0, 3, 5, 4 };
            for (int i = 0; i < prog.Length; i++) prog[i] = r.Next(scale.Length);
            float t = 0.5f; int bar = 0;
            while (t < seconds - 4f)
            {
                int chordDeg = prog[bar % prog.Length];
                // chord pad (3 notes)
                for (int k = 0; k < 3; k++)
                {
                    int deg = chordDeg + k * 2;
                    float f = NoteFreq(root, scale, deg);
                    Piano(b, t, tempo * 4f, f * 0.5f, 0.12f);
                }
                // melody: 4 beats, some rests
                for (int beat = 0; beat < 4; beat++)
                {
                    if (r.NextFloat() < (style == 1 ? 0.2f : 0.45f)) continue;
                    int deg = chordDeg + r.Next(5) + scale.Length;
                    Piano(b, t + beat * tempo + (r.NextFloat() < 0.2f ? tempo * 0.5f : 0), tempo * 2.5f, NoteFreq(root, scale, deg), 0.18f);
                }
                if (style == 1) Piano(b, t, tempo, root * 0.5f, 0.2f);
                t += tempo * 4f; bar++;
                if (r.NextFloat() < 0.1f) t += tempo * 2f; // breathing pause
            }
            // reverb (multi tap)
            var outb = (float[])b.Clone();
            int[] taps = { (int)(0.113f * SR), (int)(0.197f * SR), (int)(0.291f * SR), (int)(0.407f * SR) };
            float[] g = { 0.35f, 0.28f, 0.2f, 0.14f };
            for (int i = 0; i < b.Length; i++) for (int k = 0; k < taps.Length; k++) if (i >= taps[k]) outb[i] += outb[i - taps[k]] * g[k] * 0.5f;
            float m = 0; foreach (var v in outb) m = Mathf.Max(m, Mathf.Abs(v));
            if (m > 0) for (int i = 0; i < outb.Length; i++) outb[i] *= 0.6f / m;
            int fade = SR * 2;
            for (int i = 0; i < fade && i < outb.Length; i++) { outb[outb.Length - 1 - i] *= i / (float)fade; }
            return outb;
        }

        static float NoteFreq(float root, int[] scale, int deg)
        {
            int oct = deg / scale.Length; int idx = deg % scale.Length;
            return root * Mathf.Pow(2f, oct + scale[idx] / 12f);
        }

        static void Piano(float[] b, float start, float dur, float f, float gain)
        {
            int s0 = (int)(start * SR), n = (int)(dur * SR);
            for (int i = 0; i < n && s0 + i < b.Length; i++)
            {
                float t = i / (float)SR;
                float env = Mathf.Exp(-t * 3.2f / dur * 2f) * Mathf.Min(1f, t * 200f);
                float ph = t * f * Mathf.PI * 2f;
                float v = Mathf.Sin(ph) + 0.35f * Mathf.Sin(ph * 2f) * Mathf.Exp(-t * 4f) + 0.12f * Mathf.Sin(ph * 3f) * Mathf.Exp(-t * 6f);
                b[s0 + i] += v * env * gain;
            }
        }
    }
}
