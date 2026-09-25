using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Owns the running game: the session, the three worlds and their chunk managers, the player, the tick loop,
    /// the camera, sky and weather rendering, saving/loading and the settings that survive between sessions.
    /// One instance lives on a persistent GameObject created by the bootstrapper.
    /// </summary>
    [DefaultExecutionOrder(-100)]
    public sealed partial class GameManager : MonoBehaviour
    {
        public const string Version = "1.0";
        public static string VersionString => "1.0";

        public static GameManager Instance;
        public static Camera MainCamera;
        public static Vector3 ListenerPosition { get; private set; }
        public static bool Ready { get; private set; }

        // ------------------------------------------------------------------ state
        public GameSession session;
        public Player player;
        public Hud hud;
        public World ActiveWorld => _activeWorld;
        World _activeWorld;
        public World world => _activeWorld;

        readonly Dictionary<DimensionId, ChunkManager> chunkManagers = new Dictionary<DimensionId, ChunkManager>();
        public ChunkManager chunks => _activeWorld != null && chunkManagers.TryGetValue(_activeWorld.dim, out var cm) ? cm : null;
        public ChunkManager ChunksOf(DimensionId d) => chunkManagers.TryGetValue(d, out var cm) ? cm : null;

        JobSystem jobs;
        public PlayerInteraction interaction;
        readonly List<Entity> renderScratch = new List<Entity>();
        public bool Playing { get; private set; }

        // tick accumulator: the simulation runs at a fixed 20 ticks per second
        public const float TickRate = 20f;
        const float TickStep = 1f / TickRate;
        float tickAccum;
        public long TicksRun { get; private set; }
        public int tickMs;

        // ------------------------------------------------------------------ settings (persisted in PlayerPrefs)
        public int renderDistance = 10;
        public int simulationDistance = 8;
        public float fov = 70f;
        public float brightness = 0.5f;
        public bool vsync = true;
        public bool smoothLighting = true;
        public bool fancyLeaves = true;
        public bool clouds = true;
        public int particlesLevel; // 0 all, 1 decreased, 2 minimal
        public float mouseSensitivity = 0.5f;
        public bool invertY;
        public int maxFps = 120;
        public bool showFps;
        public readonly Dictionary<string, string> KeyBindings = new Dictionary<string, string>();
        public int lastSaveSeconds = 300;

        bool paused;
        public bool IsPaused => paused;
        WorldInfo pendingLoad;
        public float loadingProgress = 1f;
        public bool LoadingWorld { get; private set; }
        string loadingTitle = "Building terrain";

        // ------------------------------------------------------------------ lifecycle
        void Awake()
        {
            if (Instance != null && Instance != this) { Destroy(gameObject); return; }
            Instance = this;
            DontDestroyOnLoad(gameObject);
            // saves, commands and reports format numbers; a decimal comma from the OS locale would corrupt them
            System.Globalization.CultureInfo.DefaultThreadCurrentCulture = System.Globalization.CultureInfo.InvariantCulture;
            System.Globalization.CultureInfo.CurrentCulture = System.Globalization.CultureInfo.InvariantCulture;
            LoadSettings();
            InitKeyBindings();
            hud = new Hud();
            hud.Init(this);
            Biome.Init();
            Blocks.Init();
            Items.Init();
            Recipes.Init();
            Res.Init();
            try { Sounds.Init(); } catch (Exception e) { Debug.LogError("[Init] sounds: " + e); }
            try { EnsureCamera(); } catch (Exception e) { Debug.LogError("[Init] camera: " + e); }
            try { ApplySettings(); } catch (Exception e) { Debug.LogError("[Init] settings: " + e); }
            hud.Push(new TitleScreen());
            Ready = true;
        }

        /// <summary>Closing the window saves the running world, like leaving through the pause menu.</summary>
        void OnApplicationQuit()
        {
            if (!Playing || session?.save == null || player == null) return;
            try { SaveAll(); } catch (Exception e) { Debug.LogError("[Save] on quit: " + e); }
        }

        void OnDestroy()
        {
            if (Instance == this) { Instance = null; Ready = false; }
        }

        public void EnsureChunks() => jobs ??= new JobSystem(Mathf.Max(2, SystemInfo.processorCount - 2));

        // ------------------------------------------------------------------ world lifecycle
        public void NewWorld(string name, int seed, int mode, bool cheats = true)
        {
            var folder = SaveManager.UniqueFolder(name);
            var s = new GameSession(name, seed);
            s.save = new SaveManager(s, folder);
            s.defaultMode = (GameMode)Mathf.Clamp(mode, 0, 3);
            StartSession(s, null);
            hud.Replace(null);
            hud.ClearScreens();
            Playing = true;
            if (menuMusicQueued) { Sounds.StopMusic(); menuMusicQueued = false; }
        }

        public void Load(WorldInfo info)
        {
            var s = SaveManager.LoadSession(info.folder);
            if (s == null) { hud?.Chat("Failed to load " + info.name); return; }
            StartSession(s, info);
            hud.ClearScreens();
            Playing = true;
            if (menuMusicQueued) { Sounds.StopMusic(); menuMusicQueued = false; }
        }

        void StartSession(GameSession s, WorldInfo info)
        {
            EnsureChunks();
            ClearWorlds();
            session = s;
            s.save ??= new SaveManager(s, info?.folder ?? SaveManager.UniqueFolder(s.worldName));
            var saved = s.save.playerData;
            var dim = DimensionId.Overworld;
            if (saved != null && saved.TryGetValue("dim", out var dimText) && int.TryParse(dimText, out var di)) dim = (DimensionId)Mathf.Clamp(di, 0, 2);
            for (int d = 0; d < 3; d++)
            {
                var w = s.GetWorld((DimensionId)d);
                var cm = new ChunkManager(w, jobs, Res.ChunkMats, transform) { renderDistance = renderDistance, smoothLighting = smoothLighting, fancyLeaves = fancyLeaves };
                chunkManagers[(DimensionId)d] = cm;
            }
            _activeWorld = s.GetWorld(dim);
            Vector3 spawn = s.worldSpawn ?? s.Overworld.generator.FindSpawn();
            player = new Player(_activeWorld) { playerName = PlayerName };
            player.SetPosition(spawn);
            player.SetGameMode(s.defaultMode);
            if (saved != null) player.LoadFull(saved);
            _activeWorld.AddEntity(player);
            s.worldSpawn ??= spawn;
            interaction = new PlayerInteraction(player);
            hud.Replace(null);
            hud.ClearScreens();
            hud.showDebug = false;
            loadingProgress = 0f;
            LoadingWorld = true;
            hud.Push(new LoadingScreen("Loading " + s.worldName));
        }

        public static string PlayerName
        {
            get => PlayerPrefs.GetString("mcr.playerName", "Steve");
            set => PlayerPrefs.SetString("mcr.playerName", value);
        }

        void ClearWorlds()
        {
            // the scene objects of the old worlds (mob and item models, chests, the held item, particles) go with them
            if (session != null)
                foreach (var w in session.worlds)
                {
                    if (w == null) continue;
                    foreach (var e in w.entities) if (e.go != null) { Destroy(e.go); e.go = null; }
                }
            BlockEntityRenderer.Clear();
            PlayerVisual.Clear();
            Particles.Clear();
            foreach (var cm in chunkManagers.Values) cm.DestroyAll();
            chunkManagers.Clear();
            if (hud != null) hud.ClearScreens();
            player = null;
            _activeWorld = null;
        }

        public void QuitToTitle()
        {
            SaveAll();
            ClearWorlds();
            session = null;
            Playing = false;
            paused = false;
            Time.timeScale = 1f;
            hud.showDebug = false;
            hud.chat.Clear();
            hud.Replace(new TitleScreen());
        }

        public void SaveAll()
        {
            if (session?.save == null || player == null) return;
            session.worldSpawn = player.spawnPos.HasValue ? player.spawnPos.Value.Center : session.worldSpawn;
            session.save.SaveAll(player);
            hud?.ShowActionBar("Saved");
        }

        // ------------------------------------------------------------------ dimension travel
        /// <summary>Loads the destination chunks with a progress screen, then moves the player and runs the arrival hook.</summary>
        public void BeginTravel(Player p, World target, Vector3 dest, Func<Vector3> resolve)
        {
            if (p == null || target == null) return;
            if (p.world != target)
            {
                p.world.entities.Remove(p);
                p.world.entitiesToAdd.Remove(p);
                target.AddEntity(p);
            }
            _activeWorld = target;
            p.Teleport(dest);
            LoadingWorld = true;
            loadingProgress = 0f;
            loadingTitle = target.dim == DimensionId.Overworld ? "Returning to the Overworld" : "Entering the " + (target.dim == DimensionId.Nether ? "Nether" : "End");
            pendingTravel = () =>
            {
                Vector3 landing = resolve != null ? resolve() : dest;
                player.Teleport(landing);
                player.velocity = Vector3.zero;
                player.fallDistance = 0;
                player.portalCooldown = 300;
                ActiveWorld.InvalidateCache();
                Sounds.UpdateAmbience(0f, 0f, false, target.dim, false);
                session.save?.SaveAll(player);
                hud?.ShowActionBar(target.dim == DimensionId.Overworld ? "Overworld" : target.dim.ToString());
            };
            hud?.Push(new LoadingScreen(loadingTitle));
        }

        Action pendingTravel;

        /// <summary>Immediate (no loading screen) dimension change, used by respawn and commands.</summary>
        public void ChangeDimension(Player p, World target, Vector3 pos, bool respawn)
        {
            if (p == null || target == null) return;
            if (p.world != target)
            {
                p.world.entities.Remove(p);
                p.world.entitiesToAdd.Remove(p);
                target.AddEntity(p);
            }
            _activeWorld = target;
            p.Teleport(pos);
            p.velocity = Vector3.zero;
            p.fallDistance = 0;
            p.portalCooldown = respawn ? 0 : 300;
            target.InvalidateCache();
            if (respawn) { LoadingWorld = true; loadingProgress = 0f; hud?.Push(new LoadingScreen("Respawning")); }
        }

        public void EnsureChunksAround(World w, Vector3 center, int radius)
        {
            for (int cx = -radius; cx <= radius; cx++)
                for (int cz = -radius; cz <= radius; cz++)
                    w.GetChunk((Mathf.FloorToInt(center.x) >> 4) + cx, (Mathf.FloorToInt(center.z) >> 4) + cz);
        }

        /// <summary>Moves a teleported entity to the first safe standing spot at or above the requested column.</summary>
        /// <summary>
        /// Nearest standing spot to <paramref name="p"/>: solid, harmless ground with two blocks of open air above
        /// (no fluid, fire or cactus), searched outward in rings. In the Nether the spot must be under the bedrock
        /// roof. Falls back to building a small obsidian pad with a cleared pocket.
        /// </summary>
        public static Vector3 SafeSurface(World w, Vector3 p)
        {
            int x0 = Mathf.FloorToInt(p.x), z0 = Mathf.FloorToInt(p.z);
            int top = w.dim == DimensionId.Nether ? 120 : w.maxY - 2;
            int y0 = Mathf.Clamp(Mathf.FloorToInt(p.y), w.minY + 1, top);
            for (int r = 0; r <= 24; r++)
            {
                Vector3? best = null; int bestDy = int.MaxValue;
                for (int dx = -r; dx <= r; dx++)
                    for (int dz = -r; dz <= r; dz++)
                    {
                        if (Mathf.Abs(dx) != r && Mathf.Abs(dz) != r) continue;
                        int x = x0 + dx, z = z0 + dz;
                        for (int dy = 0; dy < w.height - 2 && dy < bestDy; dy++)
                            for (int dir = 0; dir < 2; dir++)
                            {
                                int yy = dir == 0 ? y0 + dy : y0 - dy;
                                if (yy <= w.minY + 1 || yy >= top) continue;
                                if (!Standable(w, x, yy, z)) continue;
                                if (dy < bestDy) { bestDy = dy; best = new Vector3(x + 0.5f, yy, z + 0.5f); }
                            }
                    }
                if (best.HasValue) return best.Value;
            }
            int py = Mathf.Clamp(y0, w.minY + 2, top - 4);
            ushort obs = Blocks.StateOf("obsidian");
            for (int dx = -1; dx <= 1; dx++)
                for (int dz = -1; dz <= 1; dz++)
                {
                    w.SetState(new Int3(x0 + dx, py - 1, z0 + dz), obs);
                    for (int h = 0; h < 3; h++) w.SetState(new Int3(x0 + dx, py + h, z0 + dz), 0);
                }
            return new Vector3(x0 + 0.5f, py, z0 + 0.5f);
        }

        static bool Standable(World w, int x, int y, int z)
        {
            var below = w.GetBlock(new Int3(x, y - 1, z));
            if (!below.solid || below.isLiquid || below.id == "magma_block" || below.id == "cactus" || below.id == "campfire") return false;
            for (int h = 0; h < 2; h++)
            {
                var b = w.GetBlock(new Int3(x, y + h, z));
                if (b.solid || b.isLiquid || b.id == "fire" || b.id == "soul_fire" || b.id == "lava" || b.id == "water" || b.id == "powder_snow" || b.id == "sweet_berry_bush") return false;
            }
            return true;
        }

        // ------------------------------------------------------------------ screens & input plumbing
        /// <summary>Every container the player opens (block, entity or inventory) gets its screen here.</summary>
        public void OnMenuOpened(Menu m)
        {
            if (hud == null || m == null) return;
            hud.RemoveScreens<ContainerScreen>();
            hud.Push(new ContainerScreen(m));
            if (m is InventoryMenu && player != null) Achievements.Grant(player, "root");
        }

        public void OnMenuClosed(Menu m)
        {
            if (hud == null) return;
            for (int i = hud.screens.Count - 1; i >= 0; i--)
                if (hud.screens[i] is ContainerScreen cs && cs.menu == m) { hud.screens.RemoveAt(i); }
        }
        public void OpenCreativeInventory() { hud?.Push(new CreativeScreen()); }
        public void TogglePause()
        {
            if (!Playing) return;
            if (hud.HasScreen<PauseScreen>()) hud.Pop();
            else if (!hud.AnyScreen) hud.Push(new PauseScreen());
        }
        public void OpenOptions() => hud?.Push(new OptionsScreen());

        public void ShowCredits()
        {
            hud.ClearScreens();
            hud.Push(new CreditsScreen());
        }

        public void ShowElderGuardianCurse()
        {
            hud?.SetTitle("Elder Guardian", "Mining Fatigue inflicted");
            player?.AddEffect(new EffectInstance(Effect.MiningFatigue, 6000, 2, true));
        }

        public void OnLightningFlash() { sky?.Flash(); }

        public void OnExplosion(Vector3 center, float power)
        {
            float d = Vector3.Distance(center, ListenerPosition);
            if (d < 64f) hud?.OnHurt(Mathf.Clamp01(power / 8f) * 0.35f);
            Particles.Explosion(_activeWorld, center, power);
        }

        public void OnItemPickedUp(ItemEntity e, Player p)
        {
            if (e.stack == null || e.stack.item == null) return;
            Achievements.OnPickup(p, e.stack.item.id);
        }

        /// <summary>World position of the fishing rod tip, used to draw the bobber line.</summary>
        public Vector3 RodTipPosition(Player p, float partial)
        {
            if (p == null) return Vector3.zero;
            var right = Quaternion.Euler(0, p.yaw, 0) * Vector3.right;
            return p.InterpPos(partial) + Vector3.up * (p.EyeHeight - 0.35f) + p.LookDir * 0.35f + right * 0.2f;
        }

        // ------------------------------------------------------------------ update
        void Update()
        {
            if (!Ready) return;
            float dt = Time.unscaledDeltaTime;
            PollInputFrame();
            if (session == null)
            {
                // the title screen has no world; only the interface (and the menu music) updates
                if (!menuMusicQueued) { Sounds.PlayMenuMusicSoon(); menuMusicQueued = true; }
                Sounds.UpdateAmbience(dt, 0f, false, DimensionId.Overworld, true);
                UpdateInput(dt);
                hud.Update(dt);
                UpdateCamera(dt);
                hud.Render();
                return;
            }
            if (LoadingWorld) UpdateLoading(dt);
            else if (!paused)
            {
                tickAccum += dt;
                int guard = 0;
                while (tickAccum >= TickStep && guard < 10) { TickWorld(); tickAccum -= TickStep; guard++; }
                if (tickAccum > TickStep * 10) tickAccum = 0;
            }
            UpdateInput(dt);
            hud.Update(dt);
            UpdateCamera(dt);
            hud.Render();
            hud.RecordFrameTime(dt * 1000f);
        }

        bool menuMusicQueued;

        void TickWorld()
        {
            TicksRun++;
            var sw = System.Diagnostics.Stopwatch.StartNew();
            var w = ActiveWorld;
            if (w == null) return;
            session.Tick();
            session.RunDeferred();
            ApplyPlayerControls();
            w.Tick(player != null ? player.position : Vector3.zero);
            w.TickEntities(player != null ? player.position : Vector3.zero);
            MobSpawner.Tick(w);
            MobSpawner.TickSpecial(w);
            if (w.dim == DimensionId.End) session.dragonFight?.Tick(w);
            SkyTick();
            Particles.Tick(w);
            Sounds.UpdateAmbience(TickStep, session.RainLevel(0f), player != null && player.position.y < 55, w.dim, hud.AnyScreen);
            tickMs = (int)sw.ElapsedMilliseconds;
            if (TicksRun % (20 * 60) == 0) SaveAll();
            Listeners.SetCount(w.entities.Count);
        }

        void UpdateLoading(float dt)
        {
            float p = chunks != null ? chunks.LoadProgress(player != null ? player.position : Vector3.zero, 2) : 1f;
            loadingProgress = Mathf.MoveTowards(loadingProgress, p, dt * 1.2f);
            chunks?.Update(player != null ? player.position : Vector3.zero, true);
            if (loadingProgress >= 0.999f)
            {
                LoadingWorld = false;
                var act = pendingTravel;
                pendingTravel = null;
                act?.Invoke();
                hud.RemoveScreens<LoadingScreen>();
            }
        }

        /// <summary>Keeps the player's own chunks streaming (the world tick only advances the simulation).</summary>
        void LateUpdate()
        {
            if (session == null || LoadingWorld) return;
            var w = ActiveWorld;
            if (w == null || player == null) return;
            var cm = chunks;
            if (cm != null)
            {
                cm.renderDistance = renderDistance;
                cm.smoothLighting = smoothLighting;
                cm.fancyLeaves = fancyLeaves;
                cm.Update(player.position);
            }
            w.simulationDistance = simulationDistance;
            if (player.dead && !hud.HasScreen<DeathScreen>() && player.deathScreenTime > 20)
                hud.Push(new DeathScreen());
        }

        // ------------------------------------------------------------------ settings
        public void ApplySettings()
        {
            QualitySettings.vSyncCount = vsync ? 1 : 0;
            Application.targetFrameRate = vsync ? -1 : maxFps;
            foreach (var cm in chunkManagers.Values) { cm.smoothLighting = smoothLighting; cm.fancyLeaves = fancyLeaves; cm.ReloadAll(); }
            WorldLighting.Apply(this);
            SaveSettings();
        }

        void SaveSettings()
        {
            PlayerPrefs.SetInt("mcr.renderDistance", renderDistance);
            PlayerPrefs.SetInt("mcr.simulationDistance", simulationDistance);
            PlayerPrefs.SetFloat("mcr.fov", fov);
            PlayerPrefs.SetFloat("mcr.brightness", brightness);
            PlayerPrefs.SetInt("mcr.vsync", vsync ? 1 : 0);
            PlayerPrefs.SetInt("mcr.smoothLighting", smoothLighting ? 1 : 0);
            PlayerPrefs.SetInt("mcr.fancyLeaves", fancyLeaves ? 1 : 0);
            PlayerPrefs.SetInt("mcr.clouds", clouds ? 1 : 0);
            PlayerPrefs.SetInt("mcr.particles", particlesLevel);
            PlayerPrefs.SetFloat("mcr.mouseSensitivity", mouseSensitivity);
            PlayerPrefs.SetInt("mcr.invertY", invertY ? 1 : 0);
            PlayerPrefs.SetInt("mcr.maxFps", maxFps);
            PlayerPrefs.SetInt("mcr.showFps", showFps ? 1 : 0);
            foreach (var kv in KeyBindings) PlayerPrefs.SetString("mcr.key." + kv.Key, kv.Value);
            PlayerPrefs.SetFloat("mcr.vol.master", Sounds.masterVolume); PlayerPrefs.SetFloat("mcr.vol.music", Sounds.musicVolume);
            PlayerPrefs.SetFloat("mcr.vol.records", Sounds.recordVolume); PlayerPrefs.SetFloat("mcr.vol.weather", Sounds.weatherVolume);
            PlayerPrefs.SetFloat("mcr.vol.blocks", Sounds.blockVolume); PlayerPrefs.SetFloat("mcr.vol.hostile", Sounds.hostileVolume);
            PlayerPrefs.SetFloat("mcr.vol.friendly", Sounds.friendlyVolume); PlayerPrefs.SetFloat("mcr.vol.players", Sounds.playerVolume);
            PlayerPrefs.SetFloat("mcr.vol.ambient", Sounds.ambientVolume);
            PlayerPrefs.Save();
        }

        void LoadSettings()
        {
            renderDistance = PlayerPrefs.GetInt("mcr.renderDistance", 10);
            simulationDistance = PlayerPrefs.GetInt("mcr.simulationDistance", 8);
            fov = PlayerPrefs.GetFloat("mcr.fov", 70f);
            brightness = PlayerPrefs.GetFloat("mcr.brightness", 0.5f);
            vsync = PlayerPrefs.GetInt("mcr.vsync", 1) == 1;
            smoothLighting = PlayerPrefs.GetInt("mcr.smoothLighting", 1) == 1;
            fancyLeaves = PlayerPrefs.GetInt("mcr.fancyLeaves", 1) == 1;
            clouds = PlayerPrefs.GetInt("mcr.clouds", 1) == 1;
            particlesLevel = PlayerPrefs.GetInt("mcr.particles", 0);
            mouseSensitivity = PlayerPrefs.GetFloat("mcr.mouseSensitivity", 0.5f);
            invertY = PlayerPrefs.GetInt("mcr.invertY", 0) == 1;
            maxFps = PlayerPrefs.GetInt("mcr.maxFps", 120);
            showFps = PlayerPrefs.GetInt("mcr.showFps", 0) == 1;
            Sounds.masterVolume = PlayerPrefs.GetFloat("mcr.vol.master", Sounds.masterVolume); Sounds.musicVolume = PlayerPrefs.GetFloat("mcr.vol.music", Sounds.musicVolume);
            Sounds.recordVolume = PlayerPrefs.GetFloat("mcr.vol.records", Sounds.recordVolume); Sounds.weatherVolume = PlayerPrefs.GetFloat("mcr.vol.weather", Sounds.weatherVolume);
            Sounds.blockVolume = PlayerPrefs.GetFloat("mcr.vol.blocks", Sounds.blockVolume); Sounds.hostileVolume = PlayerPrefs.GetFloat("mcr.vol.hostile", Sounds.hostileVolume);
            Sounds.friendlyVolume = PlayerPrefs.GetFloat("mcr.vol.friendly", Sounds.friendlyVolume); Sounds.playerVolume = PlayerPrefs.GetFloat("mcr.vol.players", Sounds.playerVolume);
            Sounds.ambientVolume = PlayerPrefs.GetFloat("mcr.vol.ambient", Sounds.ambientVolume);
        }

        void InitKeyBindings()
        {
            KeyBindings.Clear();
            KeyBindings["Forward"] = "W";
            KeyBindings["Left"] = "A";
            KeyBindings["Back"] = "S";
            KeyBindings["Right"] = "D";
            KeyBindings["Jump"] = "Space";
            KeyBindings["Sneak"] = "Left Shift";
            KeyBindings["Sprint"] = "Left Ctrl";
            KeyBindings["Inventory"] = "E";
            KeyBindings["Drop"] = "Q";
            KeyBindings["Swap Offhand"] = "F";
            KeyBindings["Pick Block"] = "Middle Mouse";
            KeyBindings["Chat"] = "T / /";
            KeyBindings["Command"] = "/";
            KeyBindings["Debug"] = "F3";
            KeyBindings["Game Mode Switcher"] = "F3+F4";
            KeyBindings["Perspective"] = "F5";
            KeyBindings["Fullscreen"] = "F11";
            KeyBindings["Advancements"] = "L";
            KeyBindings["Save"] = "Ctrl+S";
        }
    }
}
