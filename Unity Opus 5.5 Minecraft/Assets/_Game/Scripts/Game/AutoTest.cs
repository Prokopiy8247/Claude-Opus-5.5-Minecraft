using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Optional self-test driven by command line switches, so the automated checks can start a world, run the
    /// simulation for a while, take screenshots and write a report without a human at the keyboard.
    ///     -mcrNew "&lt;seed&gt;"     create and enter a fresh world with that seed
    ///     -mcrLoad "&lt;folder&gt;"  load an existing save folder
    ///     -mcrMode "creative"    starting game mode
    ///     -mcrTicks 600          simulate N ticks then act (default 400)
    ///     -mcrShot "path"        write a screenshot once the ticks are done
    ///     -mcrReport "path"      write a JSON report and quit
    ///     -mcrQuit               quit when finished
    ///     -mcrScript "a;b;c"     after the ticks, run semicolon-separated steps (see <see cref="RunStep"/>),
    ///                            e.g. "give diamond_sword;wait 10;shot C:/tmp/a.png;screen creative;shot C:/tmp/b.png"
    /// Completed switches are also accepted without the leading dash.
    /// </summary>
    public sealed class AutoTest : MonoBehaviour
    {
        public static AutoTest Instance { get; private set; }

        int ticksWanted = 400;
        string screenshotPath, reportPath;
        string newWorldSeed, loadFolder, modeName = "creative";
        readonly List<string> steps = new List<string>();
        int stepIndex, waitFrames, waitUntilTick = -1;
        bool scriptDone;
        bool quitWhenDone, active;
        int startedTick = -1;
        int shotsTaken;
        readonly List<string> errors = new List<string>();
        readonly StringBuilder log = new StringBuilder();
        readonly StringBuilder trace = new StringBuilder();
        long lastTraceTick = -1;

        void Awake() { Instance = this; }

        void Start()
        {
            ParseArgs();
            if (newWorldSeed == null && loadFolder == null) { enabled = false; return; }
            active = true;
            Log("Automated run starting: seed=" + (newWorldSeed ?? "-") + " folder=" + (loadFolder ?? "-") + " ticks=" + ticksWanted);
            Application.logMessageReceived += OnLog;
        }

        void OnDestroy() { if (active) Application.logMessageReceived -= OnLog; }

        void OnLog(string condition, string stackTrace, LogType type)
        {
            if (type == LogType.Error || type == LogType.Exception || type == LogType.Assert)
                errors.Add(type + ": " + condition);
        }

        void ParseArgs()
        {
            var args = System.Environment.GetCommandLineArgs();
            for (int i = 0; i < args.Length; i++)
            {
                string a = args[i].TrimStart('-');
                string next = i + 1 < args.Length ? args[i + 1] : null;
                switch (a)
                {
                    case "mcrNew": newWorldSeed = next ?? "0"; i++; break;
                    case "mcrLoad": loadFolder = next; i++; break;
                    case "mcrMode": modeName = next ?? "creative"; i++; break;
                    case "mcrTicks": if (int.TryParse(next, out var t)) ticksWanted = t; i++; break;
                    case "mcrShot": screenshotPath = next; i++; break;
                    case "mcrReport": reportPath = next; i++; break;
                    case "mcrQuit": quitWhenDone = true; break;
                    case "mcrScript":
                        if (next != null) foreach (var st in next.Split(';')) if (st.Trim().Length > 0) steps.Add(st.Trim());
                        i++; break;
                }
            }
        }

        void Update()
        {
            if (!active) return;
            var gm = GameManager.Instance;
            if (gm == null) return;
            if (gm.session == null && startedTick < 0)
            {
                startedTick = 0;
                if (loadFolder != null)
                {
                    foreach (var info in SaveManager.ListWorlds())
                        if (info.folder == loadFolder) { gm.Load(info); Log("Loaded " + loadFolder); return; }
                    Log("Save folder not found: " + loadFolder + " - creating a new world instead");
                }
                int seed = int.TryParse(newWorldSeed, out var s) ? s : newWorldSeed.GetHashCode();
                int mode = modeName == "survival" ? 0 : modeName == "adventure" ? 2 : modeName == "spectator" ? 3 : 1;
                gm.NewWorld("AutoTest", seed, mode, true);
                Log("Created world with seed " + seed + " in mode " + modeName);
                return;
            }
            if (gm.session == null)
            {
                // after "totitle" only interface steps (wait, shot) can run
                if (scriptDone || steps.Count == 0 || startedTick < 0 || stepIndex == 0) return;
                if (waitFrames > 0) { waitFrames--; return; }
                if (stepIndex < steps.Count)
                {
                    var step = steps[stepIndex++];
                    var op = step.Split(' ')[0];
                    if (op == "shot" || op == "wait" || op == "log")
                        try { RunStep(gm, step); } catch (System.Exception e) { errors.Add("Step " + step + " failed: " + e.Message); }
                    return;
                }
                scriptDone = true;
                if (reportPath != null) { WriteReport(gm); reportPath = null; return; }
                if (quitWhenDone) Quit();
                return;
            }
            if (gm.LoadingWorld) return;
            // position trace every 25 ticks: shows falls through unloaded terrain or stuck spawns at a glance
            if (gm.player != null && gm.TicksRun % 25 == 0 && gm.TicksRun != lastTraceTick)
            {
                lastTraceTick = gm.TicksRun;
                var pp = gm.player.position;
                trace.Append(gm.TicksRun).Append(':').Append(pp.x.ToString("0.0", CultureInfo.InvariantCulture)).Append('/')
                     .Append(pp.y.ToString("0.0", CultureInfo.InvariantCulture)).Append('/').Append(pp.z.ToString("0.0", CultureInfo.InvariantCulture))
                     .Append(gm.player.onGround ? "g " : " ");
            }
            if (gm.TicksRun < ticksWanted) return;

            // scripted steps run one per frame, honouring waits
            if (!scriptDone && steps.Count > 0)
            {
                if (waitFrames > 0) { waitFrames--; return; }
                if (waitUntilTick >= 0 && gm.TicksRun < waitUntilTick) return;
                waitUntilTick = -1;
                if (stepIndex < steps.Count)
                {
                    var step = steps[stepIndex++];
                    try { RunStep(gm, step); }
                    catch (System.Exception e) { errors.Add("Step " + step + " failed: " + e.Message); Log("Step failed: " + step + " -> " + e); }
                    return;
                }
                scriptDone = true;
            }

            // the run is done: optionally take a screenshot, then report and quit
            if (screenshotPath != null && shotsTaken == 0)
            {
                shotsTaken++;
                StartCoroutine(Shoot());
                return;
            }
            if (reportPath != null) { WriteReport(gm); reportPath = null; return; }
            if (quitWhenDone) Quit();
        }

        System.Collections.IEnumerator Shoot()
        {
            yield return new WaitForEndOfFrame();
            var dir = Path.GetDirectoryName(screenshotPath);
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
            ScreenCapture.CaptureScreenshot(screenshotPath);
            Log("Screenshot written to " + screenshotPath);
            yield return new WaitForSeconds(1.5f);
        }

        void WriteReport(GameManager gm)
        {
            var sb = new StringBuilder();
            var w = gm.ActiveWorld;
            sb.Append("{\n");
            sb.Append("  \"ticks\": ").Append(gm.TicksRun).Append(",\n");
            sb.Append("  \"fps\": ").Append(Mathf.RoundToInt(1f / Mathf.Max(0.0001f, Time.smoothDeltaTime))).Append(",\n");
            sb.Append("  \"seed\": ").Append(gm.session != null ? gm.session.seed : 0).Append(",\n");
            sb.Append("  \"dimension\": \"").Append(w != null ? w.dim.ToString() : "-").Append("\",\n");
            sb.Append("  \"chunksLoaded\": ").Append(w != null ? w.chunks.Count : 0).Append(",\n");
            sb.Append("  \"entities\": ").Append(w != null ? w.entities.Count : 0).Append(",\n");
            sb.Append("  \"blockEntities\": ").Append(CountBlockEntities(w)).Append(",\n");
            sb.Append("  \"blocks\": ").Append(Blocks.All.Count).Append(",\n");
            sb.Append("  \"items\": ").Append(Items.All.Count).Append(",\n");
            sb.Append("  \"textureLayers\": ").Append(Tex.LayerCount).Append(",\n");
            sb.Append("  \"missingTextures\": ").Append(TextureGen.MissingNames.Count).Append(",\n");
            sb.Append("  \"recipes\": ").Append(Recipes.Crafting.Count + Recipes.Smelting.Count).Append(",\n");
            sb.Append("  \"mobs\": ").Append(MobRegistry.All.Count).Append(",\n");
            sb.Append("  \"models\": ").Append(System.Linq.Enumerable.Count(MobModels.All)).Append(",\n");
            sb.Append("  \"achievementsEarned\": ").Append(Achievements.CompletedCount).Append(",\n");
            sb.Append("  \"player\": ").Append(PlayerJson(gm.player)).Append(",\n");
            sb.Append("  \"spawn\": ").Append(SpawnJson(w, gm.player)).Append(",\n");
            sb.Append("  \"columnSample\": ").Append(ColumnJson(w)).Append(",\n");
            sb.Append("  \"trace\": \"").Append(Escape(trace.ToString())).Append("\",\n");
            sb.Append("  \"errorCount\": ").Append(errors.Count).Append(",\n");
            sb.Append("  \"errors\": [");
            for (int i = 0; i < errors.Count && i < 40; i++)
            {
                if (i > 0) sb.Append(',');
                sb.Append("\n    \"").Append(Escape(errors[i])).Append('"');
            }
            sb.Append("\n  ],\n  \"log\": [");
            var lines = log.ToString().Split('\n');
            for (int i = 0; i < lines.Length; i++)
            {
                if (lines[i].Length == 0) continue;
                if (i > 0) sb.Append(',');
                sb.Append("\n    \"").Append(Escape(lines[i])).Append('"');
            }
            sb.Append("\n  ]\n}\n");
            var dir = Path.GetDirectoryName(reportPath);
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
            File.WriteAllText(reportPath, sb.ToString());
            Debug.Log("[AutoTest] Report written to " + reportPath + " (" + errors.Count + " errors)");
            if (quitWhenDone) Quit();
        }

        /// <summary>
        /// One scripted step. Supported: give ITEM [count] | slot N | screen creative|inventory|pause|options|advancements|
        /// gamemode|chat|help|death|credits | tab N | close | cmd CHAT-COMMAND | look PITCH YAW | tp X Y Z | wait FRAMES |
        /// ticks N | shot PNG | f3 | f1 | view 0-2 | swing | place BLOCK (two blocks ahead) | open BLOCK (place and use it) |
        /// fly | mouse X Y (GUI units) | log TEXT
        /// </summary>
        void RunStep(GameManager gm, string step)
        {
            var parts = step.Split(new[] { ' ' }, 2);
            string op = parts[0].ToLowerInvariant();
            string arg = parts.Length > 1 ? parts[1].Trim() : "";
            var p = gm.player;
            var hud = gm.hud;
            var a = arg.Split(' ');
            float F(int i, float d = 0f) => i < a.Length && float.TryParse(a[i], NumberStyles.Float, CultureInfo.InvariantCulture, out var v) ? v : d;
            Log("step: " + step);
            switch (op)
            {
                case "give":
                {
                    var it = Items.Get(a[0]);
                    if (it == null) { errors.Add("Unknown item " + a[0]); return; }
                    p.inventory.Add(new ItemStack(it, a.Length > 1 ? (int)F(1, 1) : 1));
                    break;
                }
                case "slot": p.inventory.selected = Mathf.Clamp((int)F(0), 0, 8); break;
                case "wear":
                {
                    // put an armour piece straight into its slot
                    var it = Items.Get(a[0]) as ArmorItem;
                    if (it == null) { errors.Add("Not armour: " + a[0]); return; }
                    p.inventory.armor[(int)it.slot] = new ItemStack(it, 1);
                    break;
                }
                case "screen":
                    switch (a[0])
                    {
                        case "creative": hud.Push(new CreativeScreen()); break;
                        case "inventory": p.OpenMenu(p.inventoryMenu); break;
                        case "pause": hud.Push(new PauseScreen()); break;
                        case "options": hud.Push(new OptionsScreen()); break;
                        case "advancements": hud.Push(new AdvancementsScreen()); break;
                        case "gamemode": hud.Push(new GameModeSwitcherScreen()); break;
                        case "chat": hud.OpenChat(a.Length > 1 ? a[1] : ""); break;
                        case "help": hud.Push(new HelpScreen()); break;
                        case "death": hud.Push(new DeathScreen()); break;
                        case "credits": hud.Push(new CreditsScreen()); break;
                        default: errors.Add("Unknown screen " + a[0]); break;
                    }
                    break;
                case "tab":
                    CreativeScreen.LastTab = (int)F(0);
                    if (hud.Current is CreativeScreen) { hud.Pop(); hud.Push(new CreativeScreen()); }
                    break;
                case "close": p.CloseMenu(); hud.ClearScreens(); break;
                case "cmd": Commands.Execute(arg, p); break;
                case "look": p.pitch = F(0); p.yaw = F(1); p.prevYaw = p.yaw; break;
                case "tp": p.Teleport(new Vector3(F(0), F(1), F(2))); break;
                case "wait": waitFrames = (int)F(0, 1); break;
                case "ticks": waitUntilTick = (int)gm.TicksRun + (int)F(0, 1); break;
                case "shot":
                {
                    var dir = Path.GetDirectoryName(arg);
                    if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
                    ScreenCapture.CaptureScreenshot(arg);
                    waitFrames = 3;
                    break;
                }
                case "f3": hud.showDebug = !hud.showDebug; break;
                case "f1": hud.HideGui = !hud.HideGui; break;
                case "view": p.cameraMode = Mathf.Clamp((int)F(0), 0, 2); break;
                case "swing": p.SwingArm(); break;
                case "fly": p.abilities.mayFly = true; p.abilities.flying = true; break;
                case "mouse": hud.mouseX = F(0); hud.mouseY = F(1); break;
                case "place":
                case "open":
                {
                    var b = Blocks.Get(a[0]);
                    if (b == null) { errors.Add("Unknown block " + a[0]); return; }
                    var look = p.LookDir; look.y = 0; if (look.sqrMagnitude < 1e-4f) look = Vector3.forward; look.Normalize();
                    var pos = Int3.Floor(p.position + look * 2.2f);
                    p.world.SetState(pos, b.DefaultState);
                    if (op == "open")
                    {
                        var st = p.world.GetState(pos);
                        var blk = Blocks.ByState[st];
                        if (!blk.OnUse(p.world, pos, st - blk.baseState, p, Dir.Up, pos.ToVector3() + new Vector3(0.5f, 1f, 0.5f)))
                            errors.Add("Block " + a[0] + " did not open anything");
                    }
                    break;
                }
                case "log": Log(arg); break;
                case "hold":
                    // hold attack|use for N ticks through the normal interaction rules
                    if (a[0] == "attack") gm.scriptAttackTicks = (int)F(1, 20); else gm.scriptUseTicks = (int)F(1, 1);
                    waitUntilTick = (int)gm.TicksRun + (int)F(1, 20) + 1;
                    break;
                case "respawn":
                    if (p.dead) { hud.RemoveScreens<DeathScreen>(); p.Respawn(); }
                    break;
                case "walk": gm.scriptWalkTicks = (int)F(0, 20); waitUntilTick = (int)gm.TicksRun + (int)F(0, 20) + 1; break;
                case "click":
                {
                    // menu slot click: click <slot> [0|1] [shift]
                    if (p.menu == null) { errors.Add("click without an open menu"); return; }
                    var type = a.Length > 2 && a[2] == "shift" ? ClickType.QuickMove : ClickType.Pickup;
                    p.menu.Click((int)F(0), (int)F(1, 0), type);
                    break;
                }
                case "inv":
                {
                    var sb = new StringBuilder("inventory:");
                    for (int i = 0; i < p.inventory.main.Length; i++)
                        if (p.inventory.main[i] != null && !p.inventory.main[i].IsEmpty) sb.Append(' ').Append(i).Append('=').Append(p.inventory.main[i].item.id).Append('x').Append(p.inventory.main[i].count);
                    for (int i = 0; i < 4; i++) if (p.inventory.armor[i] != null) sb.Append(" armor").Append(i).Append('=').Append(p.inventory.armor[i].item.id);
                    Log(sb.ToString());
                    break;
                }
                case "stat":
                    Log("stat pos=" + p.position.ToString("0.0") + " dim=" + p.world.dim + " mode=" + p.gameMode + " hp=" + p.health.ToString("0.0") + " food=" + p.hunger.food
                        + " xp=" + p.xpLevel + " dead=" + p.dead + " entities=" + p.world.entities.Count);
                    break;
                case "hurt":
                {
                    // lethal damage to the nearest entity of a type (bypasses armour): hurt ender_dragon
                    Entity best = null; float bd = float.MaxValue;
                    foreach (var e in p.world.entities)
                    {
                        if (e.TypeId != a[0] || e.removed) continue;
                        float d = (e.position - p.position).sqrMagnitude;
                        if (d < bd) { bd = d; best = e; }
                    }
                    if (best is LivingEntity le) { le.Hurt(DamageSource.PlayerAttack(p), a.Length > 1 ? F(1, 10000f) : 10000f); Log("hurt " + a[0] + " -> hp " + le.health.ToString("0.0") + " dead=" + le.dead); }
                    else errors.Add("No " + a[0] + " nearby to hurt");
                    break;
                }
                case "find":
                {
                    // nearest block of an id within a radius around the player: find end_portal 24
                    int r = (int)F(1, 16);
                    var c = Int3.Floor(p.position);
                    Int3? best = null; float bd = float.MaxValue;
                    for (int dy = -r; dy <= r; dy++)
                        for (int dz = -r; dz <= r; dz++)
                            for (int dx = -r; dx <= r; dx++)
                            {
                                var q = new Int3(c.x + dx, c.y + dy, c.z + dz);
                                if (q.y < p.world.minY || q.y >= p.world.maxY) continue;
                                if (p.world.GetBlock(q).id != a[0]) continue;
                                float d = dx * dx + dy * dy + dz * dz;
                                if (d < bd) { bd = d; best = q; }
                            }
                    Log(best.HasValue ? "find " + a[0] + " at " + best.Value : "find " + a[0] + " none within " + r);
                    break;
                }
                case "totitle":
                    // leave the world for the title screen (the run keeps going: later steps can screenshot it)
                    gm.SaveAll();
                    gm.QuitToTitle();
                    break;
                case "count":
                {
                    int n = 0;
                    foreach (var e in p.world.entities) if (e.TypeId == a[0] && !e.removed) n++;
                    Log("count " + a[0] + " = " + n);
                    break;
                }
                case "block":
                {
                    // log the block under the crosshair (or at x y z)
                    if (a.Length >= 3)
                    {
                        var bp = new Int3((int)F(0), (int)F(1), (int)F(2));
                        Log("block at " + bp + " = " + p.world.GetBlock(bp).id);
                    }
                    else if (p.RaycastBlocks(false, out var hit)) Log("target " + hit.pos + " = " + p.world.GetBlock(hit.pos).id + " face " + hit.face);
                    else Log("target none");
                    break;
                }
                case "panorama":
                {
                    // "panorama <folder with spaces> [size]": a trailing number is the face size
                    string dir = arg; int size = 1024;
                    int sp = arg.LastIndexOf(' ');
                    if (sp > 0 && int.TryParse(arg.Substring(sp + 1), out var sz)) { size = sz; dir = arg.Substring(0, sp); }
                    gm.StartCoroutine(gm.CapturePanorama(dir, size));
                    waitFrames = 40;
                    break;
                }
                case "goto":
                {
                    // fly to a structure: "goto village" hovers above it, "goto stronghold inside" drops into it
                    var st = StructureManager.Locate(p.world.generator, a[0], Mathf.FloorToInt(p.position.x), Mathf.FloorToInt(p.position.z), 60);
                    if (st == null) { errors.Add("Structure not found: " + a[0]); return; }
                    bool inside = a.Length > 1 && a[1] == "inside";
                    var pos = inside ? new Vector3(st.x + 0.5f, st.y + 2f, st.z + 0.5f) : new Vector3(st.x + 0.5f, Mathf.Max(st.y, p.world.generator.ApproxSurface(st.x, st.z)) + 18f, st.z - 14.5f);
                    if (a.Length > 1 && a[1] != "inside")
                    {
                        // a named piece, e.g. "goto stronghold Portal" lands in the portal room
                        foreach (var pc in st.pieces)
                            if (pc.GetType().Name.IndexOf(a[1], System.StringComparison.OrdinalIgnoreCase) >= 0)
                            {
                                var bb = pc.box;
                                pos = new Vector3((bb.x0 + bb.x1) * 0.5f + 0.5f, bb.y0 + 2f, bb.z0 + 2.5f);
                                Log("piece " + pc.GetType().Name + " box " + bb.x0 + "," + bb.y0 + "," + bb.z0 + " .. " + bb.x1 + "," + bb.y1 + "," + bb.z1);
                                break;
                            }
                    }
                    p.abilities.mayFly = true; p.abilities.flying = true;
                    p.Teleport(pos);
                    Log("goto " + a[0] + " at " + st.x + " " + st.y + " " + st.z + " bounds " + st.bounds);
                    break;
                }
                case "probe":
                    Log("probe player " + PlayerVisual.BodyProbe());
                    foreach (var e in p.world.entities)
                        if (e is Mob mob && mob.visual != null && (e.position - p.position).sqrMagnitude < 400f) Log("probe " + mob.visual.Probe());
                    break;
                default: errors.Add("Unknown step " + step); break;
            }
        }

        static int CountBlockEntities(World w)
        {
            if (w == null) return 0;
            int n = 0;
            foreach (var c in w.chunks.Values) n += c.blockEntities.Count;
            return n;
        }

        /// <summary>Diagnostics: where the world thinks its surface is at and around the spawn column.</summary>
        static string SpawnJson(World w, Player p)
        {
            if (w == null || p == null) return "null";
            int x = Mathf.FloorToInt(p.position.x), z = Mathf.FloorToInt(p.position.z);
            ushort at = w.GetState(x, Mathf.FloorToInt(p.position.y), z);
            return "{\"x\":" + x + ",\"z\":" + z + ",\"topSurface\":" + w.TopSurfaceY(x, z) + ",\"skyHeight\":" + w.SkyHeight(x, z)
                + ",\"genApproxSurface\":" + w.generator.ApproxSurface(x, z) + ",\"blockAtPlayer\":\"" + Blocks.ByState[at].id + "\""
                + ",\"chunkReady\":" + (w.IsLoaded(x, z) ? "true" : "false") + "}";
        }

        /// <summary>Top block of a few columns around the spawn, to prove terrain actually generated.</summary>
        static string ColumnJson(World w)
        {
            if (w == null) return "[]";
            var sb = new StringBuilder("[");
            int n = 0;
            for (int dx = -8; dx <= 8 && n < 5; dx += 4)
            {
                int x = dx, z = 0;
                int top = w.TopSurfaceY(x, z);
                ushort s = w.GetState(x, top, z);
                if (n > 0) sb.Append(',');
                sb.Append("{\"x\":").Append(x).Append(",\"z\":").Append(z).Append(",\"top\":").Append(top).Append(",\"block\":\"").Append(Blocks.ByState[s].id).Append("\"}");
                n++;
            }
            sb.Append(']');
            return sb.ToString();
        }

        static string PlayerJson(Player p)
        {
            if (p == null) return "null";
            return "{\"x\":" + p.position.x.ToString("0.##") + ",\"y\":" + p.position.y.ToString("0.##") + ",\"z\":" + p.position.z.ToString("0.##")
                + ",\"health\":" + p.health.ToString("0.#") + ",\"food\":" + p.hunger.food + ",\"mode\":\"" + p.gameMode + "\"}";
        }

        static string Escape(string s) => s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", " ").Replace("\r", "");

        void Log(string msg)
        {
            log.Append(msg).Append('\n');
            Debug.Log("[AutoTest] " + msg);
        }

        void Quit()
        {
            active = false;
            Application.Quit();
        }

        /// <summary>Used by the editor tools to kick off a run without a built player.</summary>
        public void Configure(string seed, string folder, string mode, int ticks, string shot, string report, bool quit)
        {
            newWorldSeed = seed; loadFolder = folder; modeName = mode; ticksWanted = ticks;
            screenshotPath = shot; reportPath = report; quitWhenDone = quit;
            active = seed != null || folder != null;
        }
    }
}
