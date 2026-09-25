using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Chat commands, mirroring the ones the original ships with (abbreviated to the ones a single-player world
    /// can actually use). Registered in a table so the help text and the tab completion stay in sync.
    /// </summary>
    public static partial class Commands
    {
        sealed class Cmd
        {
            public string name, usage, help;
            public System.Action<Player, string[]> run;
            public bool opOnly;
        }

        static readonly List<Cmd> all = new List<Cmd>();
        static readonly Dictionary<string, Cmd> byName = new Dictionary<string, Cmd>();
        static bool inited;

        public static IReadOnlyList<string> Names { get { Init(); return names; } }
        static readonly List<string> names = new List<string>();

        static void R(string name, string usage, string help, System.Action<Player, string[]> run, bool opOnly = false)
        {
            var c = new Cmd { name = name, usage = usage, help = help, run = run, opOnly = opOnly };
            all.Add(c); byName[name] = c; names.Add(name);
        }

        static void Init()
        {
            if (inited) return; inited = true;

            R("help", "/help [command]", "Show the command list or a specific command's help", (p, a) =>
            {
                if (a.Length > 0)
                {
                    if (byName.TryGetValue(a[0].ToLowerInvariant(), out var c)) { Feed(p, c.usage + " - " + c.help); return; }
                    Feed(p, "Unknown command: " + a[0]);
                    return;
                }
                Feed(p, "Available commands (" + all.Count + "):");
                var line = "";
                foreach (var c in all)
                {
                    if (line.Length + c.name.Length + 1 > 55) { Feed(p, line); line = ""; }
                    line += c.name + " ";
                }
                if (line.Length > 0) Feed(p, line);
            });

            R("gamemode", "/gamemode <survival|creative|adventure|spectator> [player]", "Change your game mode", (p, a) =>
            {
                if (a.Length < 1) { Feed(p, "Usage: /gamemode <mode>"); return; }
                var m = ParseMode(a[0]);
                if (m == null) { Feed(p, "Unknown game mode: " + a[0]); return; }
                var target = a.Length > 1 ? FindPlayer(a[1]) : p;
                if (target == null) { Feed(p, "Player not found: " + a[1]); return; }
                target.SetGameMode(m.Value);
                if (p.world != null && p.world.session != null) p.world.session.defaultMode = m.Value;
                Feed(p, (target == p ? "Your" : target.playerName + "'s") + " game mode is now " + m.Value.ToString().ToLowerInvariant());
            }, true);

            R("difficulty", "/difficulty <peaceful|easy|normal|hard>", "Set the world difficulty", (p, a) =>
            {
                if (a.Length < 1 || p.world?.session == null) { Feed(p, "Usage: /difficulty <peaceful|easy|normal|hard>"); return; }
                switch (a[0].ToLowerInvariant())
                {
                    case "peaceful": case "p": p.world.session.difficulty = Difficulty.Peaceful; break;
                    case "easy": case "e": p.world.session.difficulty = Difficulty.Easy; break;
                    case "normal": case "n": p.world.session.difficulty = Difficulty.Normal; break;
                    case "hard": case "h": p.world.session.difficulty = Difficulty.Hard; break;
                    default: Feed(p, "Unknown difficulty: " + a[0]); return;
                }
                Feed(p, "Difficulty set to " + p.world.session.difficulty.ToString().ToLowerInvariant());
            }, true);

            R("time", "/time set <day|noon|night|midnight>|<value>", "Change the world time", (p, a) =>
            {
                if (p.world?.session == null) return;
                if (a.Length >= 2 && a[0] == "set")
                {
                    long t;
                    switch (a[1])
                    {
                        case "day": t = 1000; break;
                        case "noon": t = 6000; break;
                        case "night": t = 13000; break;
                        case "midnight": t = 18000; break;
                        default: if (!long.TryParse(a[1], out t)) { Feed(p, "Usage: /time set <day|noon|night|midnight|ticks>"); return; } break;
                    }
                    var s = p.world.session;
                    s.SetTime(s.dayTime - (s.dayTime % 24000) + t);
                    Feed(p, "Time set to " + t);
                    return;
                }
                Feed(p, "Time is " + p.world.session.TimeOfDayTicks);
            }, true);

            R("weather", "/weather <clear|rain|thunder> [seconds]", "Change the weather", (p, a) =>
            {
                if (p.world?.session == null || a.Length < 1) { Feed(p, "Usage: /weather <clear|rain|thunder> [seconds]"); return; }
                int dur = a.Length > 1 && int.TryParse(a[1], out var d) ? d : 600;
                p.world.session.SetWeather(a[0].ToLowerInvariant(), dur);
                Feed(p, "Weather set to " + a[0]);
            }, true);

            R("tp", "/tp <x> <y> <z> | /tp <player>", "Teleport", (p, a) =>
            {
                if (a.Length == 3 && float.TryParse(a[0], NumberStyles.Float, CultureInfo.InvariantCulture, out var x)
                    && float.TryParse(a[1], NumberStyles.Float, CultureInfo.InvariantCulture, out var y)
                    && float.TryParse(a[2], NumberStyles.Float, CultureInfo.InvariantCulture, out var z))
                {
                    var dest = new Vector3(x, y, z);
                    if (p.world != null) dest = GameManager.SafeSurface(p.world, dest);
                    p.Teleport(dest);
                    p.velocity = Vector3.zero;
                    Feed(p, "Teleported to " + x + " " + y + " " + z);
                    return;
                }
                if (a.Length == 1)
                {
                    var t = FindPlayer(a[0]);
                    if (t == null) { Feed(p, "Player not found: " + a[0]); return; }
                    p.Teleport(t.position);
                    p.velocity = Vector3.zero;
                    Feed(p, "Teleported to " + t.playerName);
                    return;
                }
                Feed(p, "Usage: /tp <x> <y> <z>");
            }, true);

            R("give", "/give <item> [count]", "Give yourself an item", (p, a) =>
            {
                if (a.Length < 1) { Feed(p, "Usage: /give <item> [count]"); return; }
                var it = Items.Get(a[0]);
                if (it == null) { Feed(p, "Unknown item: " + a[0]); return; }
                int n = a.Length > 1 && int.TryParse(a[1], out var c) ? Mathf.Clamp(c, 1, 6400) : 1;
                var stack = new ItemStack(it, n);
                if (!p.inventory.Add(stack))
                {
                    var left = new ItemStack(it, stack.count);
                    p.inventory.AddOrDrop(left);
                }
                Feed(p, "Gave " + n + " " + it.displayName);
            }, true);

            R("enchant", "/enchant <enchantment> [level]", "Enchant the held item", (p, a) =>
            {
                if (a.Length < 1) { Feed(p, "Usage: /enchant <enchantment> [level]"); return; }
                var e = Enchant.Get(a[0]);
                if (e == null) { Feed(p, "Unknown enchantment: " + a[0]); return; }
                var held = p.inventory.Selected;
                if (held == null || held.IsEmpty) { Feed(p, "You are not holding an item"); return; }
                int lvl = a.Length > 1 && int.TryParse(a[1], out var l) ? Mathf.Clamp(l, 1, e.maxLevel) : 1;
                held.AddEnchant(e, lvl);
                Feed(p, "Applied " + e.name + " " + lvl);
            }, true);

            R("effect", "/effect give|clear <effect> [seconds] [level]", "Apply or clear a status effect", (p, a) =>
            {
                if (a.Length < 1) { Feed(p, "Usage: /effect give <effect> [seconds] [level]  |  /effect clear"); return; }
                if (a[0] == "clear") { p.ClearEffects(); Feed(p, "Cleared all effects"); return; }
                if (a.Length < 2) { Feed(p, "Usage: /effect give <effect> [seconds] [level]"); return; }
                var eff = Effect.Get(a[1]);
                if (eff == null) { Feed(p, "Unknown effect: " + a[1]); return; }
                int secs = a.Length > 2 && int.TryParse(a[2], out var s) ? Mathf.Clamp(s, 1, 100000) : 30;
                int lvl2 = a.Length > 3 && int.TryParse(a[3], out var l2) ? Mathf.Clamp(l2 - 1, 0, 9) : 0;
                p.AddEffect(new EffectInstance(eff, secs * 20, lvl2));
                Feed(p, "Applied " + eff.name + " for " + secs + "s");
            }, true);

            R("summon", "/summon <entity> [x y z]", "Spawn an entity", (p, a) =>
            {
                if (a.Length < 1) { Feed(p, "Usage: /summon <entity> [x y z]"); return; }
                Vector3 pos = p.position + p.LookDir * 2f;
                if (a.Length >= 4 && float.TryParse(a[1], NumberStyles.Float, CultureInfo.InvariantCulture, out var x)
                    && float.TryParse(a[2], NumberStyles.Float, CultureInfo.InvariantCulture, out var y)
                    && float.TryParse(a[3], NumberStyles.Float, CultureInfo.InvariantCulture, out var z))
                    pos = new Vector3(x, y, z);
                var id = a[0].Replace("minecraft:", "");
                var mob = MobRegistry.Spawn(p.world, id, pos, SpawnReason.Command);
                if (mob == null) { Feed(p, "Unknown entity: " + a[0]); return; }
                Feed(p, "Summoned " + id);
            }, true);

            R("kill", "/kill [player]", "Deal fatal damage", (p, a) =>
            {
                var t = a.Length > 0 ? FindPlayer(a[0]) : p;
                if (t == null) { Feed(p, "Player not found"); return; }
                if (a.Length == 0) t.Hurt(DamageSource.Generic, 1000f);
                else t.Hurt(DamageSource.Generic, 1000f);
                Feed(p, "Killed " + t.playerName);
            }, true);

            R("clear", "/clear", "Empty your inventory", (p, a) =>
            {
                p.inventory.Clear();
                Feed(p, "Inventory cleared");
            }, true);

            R("seed", "/seed", "Show the world seed", (p, a) => Feed(p, "Seed: " + (p.world != null ? p.world.seed : 0)));

            R("gamerule", "/gamerule <rule> [value]", "Read or set a game rule", (p, a) =>
            {
                var s = p.world?.session;
                if (s == null) return;
                if (a.Length == 0)
                {
                    Feed(p, "doDaylightCycle=" + s.doDaylightCycle + " doWeatherCycle=" + s.doWeatherCycle + " doMobSpawning=" + s.doMobSpawning);
                    Feed(p, "keepInventory=" + s.keepInventory + " doFireTick=" + s.doFireTick + " mobGriefing=" + s.mobGriefing + " randomTickSpeed=" + s.randomTickSpeed);
                    return;
                }
                bool b = a.Length < 2 || a[1] == "true" || a[1] == "1";
                switch (a[0])
                {
                    case "doDaylightCycle": s.doDaylightCycle = b; break;
                    case "doWeatherCycle": s.doWeatherCycle = b; break;
                    case "doMobSpawning": s.doMobSpawning = b; break;
                    case "keepInventory": s.keepInventory = b; break;
                    case "doFireTick": s.doFireTick = b; break;
                    case "mobGriefing": s.mobGriefing = b; break;
                    case "naturalRegeneration": s.naturalRegeneration = b; break;
                    case "doImmediateRespawn": s.doImmediateRespawn = b; break;
                    case "randomTickSpeed": if (a.Length > 1 && int.TryParse(a[1], out var v)) s.randomTickSpeed = Mathf.Clamp(v, 0, 64); break;
                    default: Feed(p, "Unknown game rule: " + a[0]); return;
                }
                Feed(p, "Set " + a[0] + " to " + (a.Length > 1 ? a[1] : "true"));
            }, true);

            R("locate", "/locate <structure>", "Find the nearest structure", (p, a) =>
            {
                if (a.Length < 1 || p.world == null) { Feed(p, "Usage: /locate <structure>"); return; }
                var st = StructureManager.Locate(p.world.generator, a[0].ToLowerInvariant(), Mathf.FloorToInt(p.position.x), Mathf.FloorToInt(p.position.z), 40);
                if (st == null) { Feed(p, "Could not find " + a[0] + " nearby"); return; }
                int dx = st.x - Mathf.FloorToInt(p.position.x), dz = st.z - Mathf.FloorToInt(p.position.z);
                Feed(p, "Nearest " + a[0] + " at " + st.x + " " + st.y + " " + st.z + "  (" + Mathf.RoundToInt(Mathf.Sqrt(dx * dx + dz * dz)) + " blocks away)");
            }, true);

            R("setblock", "/setblock <x> <y> <z> <block>", "Place a block", (p, a) =>
            {
                if (a.Length < 4) { Feed(p, "Usage: /setblock <x> <y> <z> <block>"); return; }
                var b = Blocks.Get(a[3]);
                if (b == null) { Feed(p, "Unknown block: " + a[3]); return; }
                if (!int.TryParse(a[0], out var x) || !int.TryParse(a[1], out var y) || !int.TryParse(a[2], out var z)) { Feed(p, "Coordinates must be integers"); return; }
                p.world.SetState(new Int3(x, y, z), b.DefaultState);
                Feed(p, "Placed " + a[3]);
            }, true);

            R("fill", "/fill <x1 y1 z1> <x2 y2 z2> <block>", "Fill a region", (p, a) =>
            {
                if (a.Length < 7) { Feed(p, "Usage: /fill <x1 y1 z1> <x2 y2 z2> <block>"); return; }
                var b = Blocks.Get(a[6]);
                if (b == null) { Feed(p, "Unknown block: " + a[6]); return; }
                if (!int.TryParse(a[0], out var x1) || !int.TryParse(a[1], out var y1) || !int.TryParse(a[2], out var z1)
                    || !int.TryParse(a[3], out var x2) || !int.TryParse(a[4], out var y2) || !int.TryParse(a[5], out var z2)) { Feed(p, "Coordinates must be integers"); return; }
                int n = 0;
                for (int y = Mathf.Min(y1, y2); y <= Mathf.Max(y1, y2); y++)
                    for (int z = Mathf.Min(z1, z2); z <= Mathf.Max(z1, z2); z++)
                        for (int x = Mathf.Min(x1, x2); x <= Mathf.Max(x1, x2); x++)
                        {
                            if (!p.world.IsLoaded(x, z)) continue;
                            p.world.SetState(new Int3(x, y, z), b.DefaultState);
                            n++;
                        }
                Feed(p, "Filled " + n + " blocks");
            }, true);

            R("particle", "/particle <name> [count]", "Spawn a particle burst", (p, a) =>
            {
                if (a.Length < 1) { Feed(p, "Usage: /particle <name> [count]"); return; }
                int n = a.Length > 1 && int.TryParse(a[1], out var c) ? Mathf.Clamp(c, 1, 2000) : 20;
                var pos = p.EyePosition + p.LookDir * 3f;
                switch (a[0].ToLowerInvariant())
                {
                    case "flame": for (int i = 0; i < n; i++) Particles.Flame(p.world, pos + Random.insideUnitSphere * 0.5f, "flame"); break;
                    case "smoke": Particles.Smoke(p.world, pos, n, 0.4f); break;
                    case "heart": for (int i = 0; i < n; i++) Particles.Heart(p.world, pos); break;
                    case "crit": Particles.Crit(p.world, pos, n); break;
                    case "explosion": Particles.Explosion(p.world, pos, n * 0.1f); break;
                    case "portal": for (int i = 0; i < n; i++) Particles.Portal(p.world, pos); break;
                    case "note": for (int i = 0; i < n; i++) Particles.Note(p.world, pos, i % 25); break;
                    default: Feed(p, "Unknown particle: " + a[0]); return;
                }
                Feed(p, "Spawned " + n + " " + a[0]);
            });

            R("spawnpoint", "/spawnpoint", "Set your spawn to your current position", (p, a) =>
            {
                p.SetSpawnPoint(p.world.dim, Int3.Floor(p.position), true);
                Feed(p, "Spawn point set");
            });

            R("dimension", "/dimension <overworld|nether|end>", "Travel to another dimension", (p, a) =>
            {
                if (a.Length < 1) { Feed(p, "Usage: /dimension <overworld|nether|end>"); return; }
                DimensionId d;
                switch (a[0].ToLowerInvariant().Replace("minecraft:", "").Replace("the_", ""))
                {
                    case "overworld": case "0": d = DimensionId.Overworld; break;
                    case "nether": case "1": d = DimensionId.Nether; break;
                    case "end": case "2": d = DimensionId.End; break;
                    default: Feed(p, "Unknown dimension: " + a[0]); return;
                }
                var s = p.world.session;
                if (s == null) return;
                if (p.world.dim == d) { Feed(p, "Already in " + d); return; }
                var target = s.GetWorld(d);
                Vector3 dest = d == DimensionId.End ? new Vector3(100.5f, 49f, 0.5f)
                    : d == DimensionId.Nether ? new Vector3(p.position.x / 8f, Mathf.Clamp(p.position.y, 32f, 100f), p.position.z / 8f)
                    : new Vector3(p.position.x * 8f, Mathf.Clamp(p.position.y, 64f, 200f), p.position.z * 8f);
                // same arrival rules as the portals: the End gets its obsidian platform, the Nether and the Overworld a
                // linked portal at a safe spot, so the way back exists
                System.Func<Vector3> resolve;
                if (d == DimensionId.End) resolve = () => Portals.EndArrival(target);
                else if (d == DimensionId.Overworld)
                {
                    // arrive in the open: search down from the surface rather than out of a cave
                    resolve = () =>
                    {
                        var top = dest;
                        top.y = target.TopSurfaceY(Mathf.FloorToInt(dest.x), Mathf.FloorToInt(dest.z)) + 1;
                        return Portals.CreatePortalNear(target, Int3.Floor(GameManager.SafeSurface(target, top)));
                    };
                }
                else resolve = () => Portals.CreatePortalNear(target, Int3.Floor(GameManager.SafeSurface(target, dest)));
                GameManager.Instance.BeginTravel(p, target, dest, resolve);
                if (d == DimensionId.End) s.dragonFight?.OnPlayerEnter(target);
                Feed(p, "Travelling to the " + d);
            }, true);

            R("save", "/save", "Save the world now", (p, a) =>
            {
                GameManager.Instance.SaveAll();
                Feed(p, "World saved");
            });

            R("advancement", "/advancement <grant|revoke|list> [id]", "Manage advancements", (p, a) =>
            {
                if (a.Length == 0 || a[0] == "list")
                {
                    Feed(p, "Earned " + Achievements.CompletedCount + " of " + Achievements.All.Count);
                    foreach (var g in Achievements.Locked()) Feed(p, "  (locked) " + g.title);
                    return;
                }
                if (a.Length < 2) { Feed(p, "Usage: /advancement <grant|revoke|list> <id>"); return; }
                if (a[0] == "grant") { Achievements.Grant(p, a[1]); Feed(p, "Granted " + a[1]); }
            }, true);

            R("title", "/title <text> [subtitle]", "Show a title on screen", (p, a) =>
            {
                if (a.Length < 1) { Feed(p, "Usage: /title <text> [subtitle]"); return; }
                var gm = GameManager.Instance;
                gm?.hud?.SetTitle(a[0], a.Length > 1 ? a[1] : null);
            });

            R("killall", "/killall [type]", "Remove nearby entities", (p, a) =>
            {
                int n = 0;
                var list = new List<Entity>(p.world.entities);
                foreach (var e in list)
                {
                    if (e is Player) continue;
                    if (a.Length > 0 && e.TypeId != a[0]) continue;
                    if ((e.position - p.position).sqrMagnitude > 128 * 128) continue;
                    e.Remove(); n++;
                }
                Feed(p, "Removed " + n + " entities");
            }, true);

            R("debug", "/debug <on|off>", "Toggle the debug overlay", (p, a) =>
            {
                var hud = GameManager.Instance?.hud;
                if (hud == null) return;
                hud.showDebug = a.Length > 0 ? a[0] == "on" || a[0] == "true" : !hud.showDebug;
                Feed(p, "Debug overlay " + (hud.showDebug ? "on" : "off"));
            });

            R("reload", "/reload", "Rebuild generated assets and chunk meshes", (p, a) =>
            {
                ItemIcons.Clear();
                UiAtlas.Clear();
                ModelSkins.ClearCache();
                ModelMesher.ClearCache();
                ItemRender.ClearCaches();
                GameManager.Instance?.chunks?.ReloadAll();
                Feed(p, "Reloaded textures and models");
            }, true);
        }

        // ------------------------------------------------------------------ dispatch
        public static void Execute(string line, Player p)
        {
            Init();
            if (string.IsNullOrWhiteSpace(line)) return;
            if (p == null) return;
            line = line.Trim();
            if (line.StartsWith("/")) line = line.Substring(1);
            var parts = Split(line);
            if (parts.Count == 0) return;
            string name = parts[0].ToLowerInvariant();
            var args = new string[parts.Count - 1];
            for (int i = 1; i < parts.Count; i++) args[i - 1] = parts[i];
            ResolveRelative(args, p, name == "setblock" || name == "fill" || name == "spawnpoint" || name == "clone");
            if (byName.TryGetValue(name, out var cmd))
            {
                try { cmd.run(p, args); }
                catch (System.Exception e) { Feed(p, "Command failed: " + e.Message); Debug.LogError(e); }
                return;
            }
            // common aliases
            switch (name)
            {
                case "gm": Execute("gamemode " + string.Join(" ", args), p); return;
                case "?": Execute("help", p); return;
                case "day": Execute("time set day", p); return;
                case "night": Execute("time set night", p); return;
                case "fly": p.abilities.mayFly = true; p.abilities.flying = true; Feed(p, "Flight enabled"); return;
                case "heal": p.health = p.maxHealth; p.hunger.food = 20; p.fireTicks = 0; Feed(p, "Healed"); return;
                case "god": p.invulnerable = !p.invulnerable; Feed(p, "Invulnerability " + (p.invulnerable ? "on" : "off")); return;
                case "xp": Execute("experience " + string.Join(" ", args), p); return;
                case "experience":
                    if (args.Length >= 2 && int.TryParse(args[1], out var x))
                    {
                        if (args[0] == "add") p.AddLevels(x); else p.GiveXp(x);
                        Feed(p, "Gave experience");
                    }
                    return;
            }
            Feed(p, "Unknown command. Try /help");
        }

        /// <summary>
        /// Relative coordinates: every run of three coordinate tokens containing a "~" (or "~N") is resolved
        /// against the player's position, axis by axis, before the command sees it. Block commands get the
        /// block the player stands in as the origin.
        /// </summary>
        static void ResolveRelative(string[] args, Player p, bool blockCoords)
        {
            for (int i = 0; i + 2 < args.Length; i++)
            {
                bool any = false, all = true;
                for (int k = 0; k < 3; k++)
                {
                    string t = args[i + k];
                    bool rel = t.StartsWith("~");
                    bool num = float.TryParse(t, NumberStyles.Float, CultureInfo.InvariantCulture, out _);
                    any |= rel; all &= rel || num;
                }
                if (!any || !all) continue;
                for (int k = 0; k < 3; k++)
                {
                    string t = args[i + k];
                    if (!t.StartsWith("~")) continue;
                    float basis = k == 0 ? p.position.x : k == 1 ? p.position.y : p.position.z;
                    if (blockCoords) basis = Mathf.Floor(basis);
                    float off = 0f;
                    if (t.Length > 1) float.TryParse(t.Substring(1), NumberStyles.Float, CultureInfo.InvariantCulture, out off);
                    float v = basis + off;
                    args[i + k] = blockCoords ? Mathf.FloorToInt(v).ToString(CultureInfo.InvariantCulture) : v.ToString("0.###", CultureInfo.InvariantCulture);
                }
                i += 2;
            }
        }

        /// <summary>Splits on spaces but keeps quoted runs together.</summary>
        static List<string> Split(string s)
        {
            var list = new List<string>();
            var sb = new System.Text.StringBuilder();
            bool quoted = false;
            foreach (var c in s)
            {
                if (c == '"') { quoted = !quoted; continue; }
                if (c == ' ' && !quoted)
                {
                    if (sb.Length > 0) { list.Add(sb.ToString()); sb.Clear(); }
                    continue;
                }
                sb.Append(c);
            }
            if (sb.Length > 0) list.Add(sb.ToString());
            return list;
        }

        static GameMode? ParseMode(string s)
        {
            switch (s.ToLowerInvariant())
            {
                case "s": case "survival": case "0": return GameMode.Survival;
                case "c": case "creative": case "1": return GameMode.Creative;
                case "a": case "adventure": case "2": return GameMode.Adventure;
                case "sp": case "spectator": case "3": return GameMode.Spectator;
                default: return null;
            }
        }

        static Player FindPlayer(string name)
        {
            var gm = GameManager.Instance;
            if (gm?.player == null) return null;
            if (string.Equals(gm.player.playerName, name, System.StringComparison.OrdinalIgnoreCase)) return gm.player;
            return null;
        }

        static void Feed(Player p, string msg)
        {
            var hud = GameManager.Instance?.hud;
            if (hud != null) hud.Chat(msg);
            else Debug.Log("[Command] " + msg);
        }

        /// <summary>
        /// Chat tab completion. <paramref name="prefix"/> is the chat line up to the caret. A single word, with or
        /// without the leading "/", completes command names from the table above. After "/command " the word being
        /// typed completes against whatever that argument takes (<see cref="ArgumentPool"/>): item ids for /give, mob
        /// ids for /summon, block ids for the block of /setblock and /fill, effect and enchantment ids, and the fixed
        /// keywords of /gamemode, /time, /weather and friends. Only the last word is completed; ids that start with it
        /// come first, then ids with a later "_word" segment matching it, each group sorted.
        /// </summary>
        public static List<string> Complete(string prefix)
        {
            Init();
            var list = new List<string>();
            prefix ??= "";
            bool command = prefix.StartsWith("/");
            string line = command ? prefix.Substring(1) : prefix;
            int lastSpace = line.LastIndexOf(' ');
            if (lastSpace < 0) { Match(names, line, list); return list; }
            if (!command) return list; // plain chat: nothing to complete against
            var before = Split(line.Substring(0, lastSpace));
            if (before.Count == 0) return list;
            var pool = ArgumentPool(before[0].ToLowerInvariant(), before.Count - 1, before);
            if (pool != null) Match(pool, line.Substring(lastSpace + 1), list);
            return list;
        }

        static void Match(IEnumerable<string> pool, string word, List<string> into)
        {
            if (word.StartsWith("minecraft:")) word = word.Substring(10);
            var inner = new List<string>();
            int first = into.Count;
            foreach (var c in pool)
            {
                if (string.IsNullOrEmpty(c)) continue;
                if (c.StartsWith(word, System.StringComparison.OrdinalIgnoreCase)) into.Add(c);
                else if (word.Length > 0 && c.IndexOf("_" + word, System.StringComparison.OrdinalIgnoreCase) >= 0) inner.Add(c);
            }
            into.Sort(first, into.Count - first, System.StringComparer.Ordinal);
            inner.Sort(System.StringComparer.Ordinal);
            into.AddRange(inner);
        }

        static readonly string[] GameRules = { "doDaylightCycle", "doWeatherCycle", "doMobSpawning", "keepInventory", "doFireTick", "mobGriefing", "naturalRegeneration", "doImmediateRespawn", "randomTickSpeed" };

        /// <summary>What argument <paramref name="arg"/> (0 = first after the name) of a command accepts, or null when
        /// it is a number, a coordinate or free text. <paramref name="before"/> holds the command and earlier arguments.</summary>
        static IEnumerable<string> ArgumentPool(string cmd, int arg, List<string> before)
        {
            switch (cmd)
            {
                case "give": return arg == 0 ? Ids(Items.All, i => i.id) : null;
                case "summon": case "killall": return arg == 0 ? Ids(MobRegistry.All, m => m.id) : null;
                case "setblock": return arg == 3 ? Ids(Blocks.All, b => b.id) : null;
                case "fill": return arg == 6 ? Ids(Blocks.All, b => b.id) : null;
                case "enchant": return arg == 0 ? Ids(Enchant.All, e => e.id) : null;
                case "effect":
                    if (arg == 0) return new[] { "give", "clear" };
                    return arg == 1 && before[1] == "give" ? Ids(Effect.All, e => e.id) : null;
                case "gamemode": case "gm": return arg == 0 ? new[] { "survival", "creative", "adventure", "spectator" } : null;
                case "difficulty": return arg == 0 ? new[] { "peaceful", "easy", "normal", "hard" } : null;
                case "time":
                    if (arg == 0) return new[] { "set" };
                    return arg == 1 && before[1] == "set" ? new[] { "day", "noon", "night", "midnight" } : null;
                case "weather": return arg == 0 ? new[] { "clear", "rain", "thunder" } : null;
                case "dimension": return arg == 0 ? new[] { "overworld", "nether", "end" } : null;
                case "particle": return arg == 0 ? new[] { "flame", "smoke", "heart", "crit", "explosion", "portal", "note" } : null;
                case "debug": return arg == 0 ? new[] { "on", "off" } : null;
                case "help": return arg == 0 ? names : null;
                case "locate": StructureManager.Init(); return arg == 0 ? Ids(StructureManager.Types, t => t.id) : null;
                case "advancement":
                    if (arg == 0) return new[] { "grant", "list" }; // revoke is accepted but does nothing yet
                    return arg == 1 && before[1] == "grant" ? Ids(Achievements.All, g => g.id) : null;
                case "gamerule":
                    if (arg == 0) return GameRules;
                    return arg == 1 && before[1] != "randomTickSpeed" ? new[] { "true", "false" } : null;
            }
            return null;
        }

        static List<string> Ids<T>(IEnumerable<T> source, System.Func<T, string> id)
        {
            var list = new List<string>();
            foreach (var x in source) list.Add(id(x));
            return list;
        }
    }
}
