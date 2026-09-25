using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public enum Difficulty { Peaceful = 0, Easy = 1, Normal = 2, Hard = 3 }
    public enum GameMode { Survival = 0, Creative = 1, Adventure = 2, Spectator = 3 }

    /// <summary>
    /// A running single-player game: all dimensions, time of day, weather, game rules, persistence.
    /// Ticked at 20 TPS by GameManager.
    /// </summary>
    public sealed class GameSession
    {
        public string worldName;
        public int seed;
        public readonly World[] worlds = new World[3];
        public long dayTime = 1000;     // ticks, 24000 per day (0 = sunrise)
        public long gameTime;
        public Difficulty difficulty = Difficulty.Normal;
        public GameMode defaultMode = GameMode.Creative;
        public bool hardcore;
        // game rules
        public bool doDaylightCycle = true, doWeatherCycle = true, doMobSpawning = true, keepInventory, doFireTick = true, mobGriefing = true, doImmediateRespawn, showCoordinates = true, naturalRegeneration = true;
        public int randomTickSpeed = 3;
        // weather
        public int rainTime = 12000 + 6000, thunderTime = 30000;
        public bool raining, thundering;
        public float rainLevel, prevRainLevel, thunderLevel, prevThunderLevel;
        public int clearWeatherTime;

        public SaveManager save;
        public DragonFight dragonFight;
        public Vector3? worldSpawn;
        public bool endPortalLitOnce;
        /// <summary>Known nether portal locations per dimension (bottom-left interior block) for linking.</summary>
        public readonly List<Int3>[] portals = { new List<Int3>(), new List<Int3>(), new List<Int3>() };
        readonly List<Action> deferred = new List<Action>();
        readonly List<Action> running = new List<Action>();

        public GameSession(string name, int seed)
        {
            worldName = name; this.seed = seed;
        }

        public World Overworld => GetWorld(DimensionId.Overworld);

        public World GetWorld(DimensionId d)
        {
            int i = (int)d;
            if (worlds[i] == null)
            {
                worlds[i] = new World(d, seed, this);
                worlds[i].randomTickSpeed = randomTickSpeed;
                if (d == DimensionId.End && dragonFight == null) dragonFight = new DragonFight(this);
            }
            return worlds[i];
        }

        public void Defer(Action a) { if (a != null) deferred.Add(a); }

        public void RunDeferred()
        {
            for (int guard = 0; guard < 8 && deferred.Count > 0; guard++)
            {
                running.Clear(); running.AddRange(deferred); deferred.Clear();
                foreach (var a in running)
                {
                    try { a(); } catch (Exception e) { Debug.LogError("[Deferred] " + e); }
                }
            }
        }

        // ------------------------------------------------------------------ time
        /// <summary>0..1 fraction of the day where 0 = noon-sun-at-zenith style angle (MC: 0 at 6000 ticks shifted).</summary>
        public float CelestialAngle(float partial = 0)
        {
            double t = ((dayTime % 24000) + partial) / 24000.0 - 0.25;
            t -= Math.Floor(t);
            // slightly longer days than nights (sun lingers near horizon less)
            double eased = 0.5 - Math.Cos(t * Math.PI) / 2.0;
            return (float)((t * 2.0 + eased) / 3.0);
        }

        /// <summary>Continuous sky darkening 0 (day) .. 11 (night), reduced by weather.</summary>
        public float SkyDarkenF(World w)
        {
            if (w == null || !w.hasSkyLight) return 0;
            float ang = CelestialAngle();
            float c = Mathf.Cos(ang * Mathf.PI * 2f);
            float day = 0.5f + 2f * Mathf.Clamp(c, -0.25f, 0.25f); // 0..1
            float wr = 1f - rainLevel * 5f / 16f;
            float wt = 1f - thunderLevel * 5f / 16f;
            return (1f - day * wr * wt) * 11f;
        }
        public int SkyDarken(World w) => (int)SkyDarkenF(w);

        /// <summary>Multiplier for sky light in the shader (1 day .. ~0.27 night).</summary>
        public float DaylightFactor(World w)
        {
            if (w == null) return 1f;
            if (!w.hasSkyLight) return w.dim == DimensionId.End ? 0.45f : 0f;
            return Mathf.Clamp01((15f - SkyDarkenF(w)) / 15f);
        }

        public bool IsRaining => rainLevel > 0.2f;
        public bool IsThundering => IsRaining && thunderLevel > 0.9f;
        public bool IsNight { get { var w = worlds[0]; return w == null ? false : SkyDarken(w) >= 4; } }
        public bool IsDay => !IsNight;
        public int DayCount => (int)(dayTime / 24000);
        public long TimeOfDayTicks => dayTime % 24000;

        public void SetTime(long t) { dayTime = t; }

        // ------------------------------------------------------------------ tick
        public void Tick()
        {
            gameTime++;
            if (doDaylightCycle) dayTime++;
            TickWeather();
            foreach (var w in worlds) if (w != null) w.randomTickSpeed = randomTickSpeed;
        }

        void TickWeather()
        {
            prevRainLevel = rainLevel; prevThunderLevel = thunderLevel;
            if (doWeatherCycle)
            {
                if (clearWeatherTime > 0) { clearWeatherTime--; raining = false; thundering = false; }
                else
                {
                    if (--thunderTime <= 0)
                    {
                        thundering = !thundering;
                        thunderTime = thundering ? UnityEngine.Random.Range(3600, 15600) : UnityEngine.Random.Range(12000, 180000);
                    }
                    if (--rainTime <= 0)
                    {
                        raining = !raining;
                        rainTime = raining ? UnityEngine.Random.Range(12000, 24000) : UnityEngine.Random.Range(12000, 180000);
                    }
                }
            }
            rainLevel = Mathf.MoveTowards(rainLevel, raining ? 1f : 0f, 0.01f);
            thunderLevel = Mathf.MoveTowards(thunderLevel, raining && thundering ? 1f : 0f, 0.01f);
        }

        public void SetWeather(string kind, int duration)
        {
            switch (kind)
            {
                case "clear": raining = false; thundering = false; clearWeatherTime = duration; break;
                case "rain": raining = true; thundering = false; rainTime = duration; clearWeatherTime = 0; break;
                case "thunder": raining = true; thundering = true; rainTime = duration; thunderTime = duration; clearWeatherTime = 0; break;
            }
        }

        public float RainLevel(float partial) => Mathf.Lerp(prevRainLevel, rainLevel, partial);
        public float ThunderLevel(float partial) => Mathf.Lerp(prevThunderLevel, thunderLevel, partial);
    }
}
