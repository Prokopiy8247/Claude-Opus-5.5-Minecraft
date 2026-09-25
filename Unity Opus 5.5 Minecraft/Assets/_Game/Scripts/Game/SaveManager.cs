using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using UnityEngine;

namespace MCR
{
    /// <summary>Persistent data of one chunk (modifications relative to generation, block entities, entities).</summary>
    public sealed class ChunkRecord
    {
        public int cx, cz;
        public readonly Dictionary<int, ushort> mods = new Dictionary<int, ushort>();
        public readonly List<(int idx, string type, Dictionary<string, string> data)> blockEntities = new List<(int, string, Dictionary<string, string>)>();
        public readonly List<SavedEntity> entities = new List<SavedEntity>();
        public bool entitiesHandled; // world-gen entities were spawned once already
    }

    public sealed class WorldInfo
    {
        public string folder, name; public int seed; public DateTime lastPlayed; public GameMode mode; public long dayTime;
    }

    /// <summary>
    /// Region-based save format: saves/&lt;world&gt;/level.txt + dim&lt;n&gt;/r.&lt;rx&gt;.&lt;rz&gt;.bin (32x32 chunks per region).
    /// Chunks store a palette of block ids so saves survive registry changes.
    /// </summary>
    public sealed class SaveManager
    {
        public readonly GameSession session;
        public readonly string dir;
        const int Magic = 0x4D435252; // MCRR
        const int Version = 2;
        readonly Dictionary<long, Dictionary<long, ChunkRecord>>[] regions = { new Dictionary<long, Dictionary<long, ChunkRecord>>(), new Dictionary<long, Dictionary<long, ChunkRecord>>(), new Dictionary<long, Dictionary<long, ChunkRecord>>() };
        readonly HashSet<long>[] dirtyRegions = { new HashSet<long>(), new HashSet<long>(), new HashSet<long>() };
        public Dictionary<string, string> playerData;
        public float lastSaveTime;

        public static string SavesRoot => Path.Combine(Application.persistentDataPath, "saves");

        public SaveManager(GameSession s, string folder)
        {
            session = s;
            dir = Path.Combine(SavesRoot, folder);
            Directory.CreateDirectory(dir);
        }

        static long RegionKey(int rx, int rz) => ((long)rx << 32) ^ (uint)rz;

        Dictionary<long, ChunkRecord> Region(DimensionId dim, int cx, int cz, bool create)
        {
            int rx = cx >> 5, rz = cz >> 5;
            long rk = RegionKey(rx, rz);
            var regs = regions[(int)dim];
            if (regs.TryGetValue(rk, out var reg)) return reg;
            reg = ReadRegion(dim, rx, rz);
            if (reg == null) { if (!create) { regs[rk] = new Dictionary<long, ChunkRecord>(); return regs[rk]; } reg = new Dictionary<long, ChunkRecord>(); }
            regs[rk] = reg;
            return reg;
        }

        string RegionPath(DimensionId dim, int rx, int rz) => Path.Combine(dir, "dim" + (int)dim, "r." + rx + "." + rz + ".bin");

        // ------------------------------------------------------------------ chunk hooks (main thread)
        public void PrepareChunk(World w, Chunk c)
        {
            var reg = Region(w.dim, c.cx, c.cz, false);
            if (reg.TryGetValue(World.ChunkKey(c.cx, c.cz), out var rec))
            {
                c.saveRecord = rec;
                c.fromSave = true;
            }
        }

        public void ApplyLoadedData(World w, Chunk c)
        {
            // world-gen block entities need their world reference
            foreach (var kv in c.blockEntities) { kv.Value.world = w; }
            var rec = c.saveRecord;
            if (rec != null)
            {
                foreach (var kv in rec.mods)
                {
                    int idx = kv.Key;
                    int lx = idx & 15, lz = (idx >> 4) & 15, y = (idx >> 8) + c.minY;
                    ushort st = kv.Value;
                    c.SetRaw(lx, y, lz, st);
                    c.mods[idx] = st;
                    var b = Blocks.ByState[st];
                    if (c.blockEntities.TryGetValue(idx, out var oldBe) && (!b.HasBlockEntity)) c.blockEntities.Remove(idx);
                    else if (b.HasBlockEntity && !c.blockEntities.ContainsKey(idx))
                    {
                        var p = new Int3((c.cx << 4) + lx, y, (c.cz << 4) + lz);
                        var be = b.CreateBlockEntity(w, p);
                        if (be != null) { be.world = w; be.pos = p; c.blockEntities[idx] = be; }
                    }
                }
                foreach (var (idx, type, data) in rec.blockEntities)
                {
                    int lx = idx & 15, lz = (idx >> 4) & 15, y = (idx >> 8) + c.minY;
                    var p = new Int3((c.cx << 4) + lx, y, (c.cz << 4) + lz);
                    var b = Blocks.ByState[c.Get(lx, y, lz)];
                    if (!b.HasBlockEntity) continue;
                    BlockEntity be = BlockEntity.Create(type) ?? b.CreateBlockEntity(w, p);
                    if (be == null) continue;
                    be.world = w; be.pos = p;
                    try { be.Load(data); } catch (Exception e) { Debug.LogWarning("BE load failed " + type + ": " + e.Message); }
                    c.blockEntities[idx] = be;
                }
            }
            c.modsDirty = false;
            // entities
            if (rec != null && rec.entitiesHandled)
            {
                foreach (var se in rec.entities) SpawnSaved(w, se);
                c.pendingEntities = null;
            }
            else if (c.pendingEntities != null)
            {
                foreach (var se in c.pendingEntities)
                {
                    var m = MobRegistry.Spawn(w, se.type, new Vector3(se.x, se.y, se.z), SpawnReason.Structure);
                    if (m != null && se.data != null) { var d = DecodeDict(se.data); try { m.Load(d); } catch { } m.persistent = true; }
                }
                c.pendingEntities = null;
            }
            c.entitiesSpawned = true;
            c.saveRecord = null;
        }

        void SpawnSaved(World w, SavedEntity se)
        {
            var e = EntityFactory.Create(se.type, w);
            if (e == null) return;
            e.SetPosition(new Vector3(se.x, se.y, se.z));
            e.yaw = e.prevYaw = se.yaw; e.pitch = se.pitch;
            try { if (se.data != null) e.Load(DecodeDict(se.data)); } catch (Exception ex) { Debug.LogWarning("Entity load failed " + se.type + ": " + ex.Message); }
            w.AddEntity(e);
        }

        /// <summary>Store a chunk's persistent data into the in-memory region (called on unload and on save).</summary>
        public void StoreChunk(World w, Chunk c)
        {
            if (c.stage < (int)ChunkStage.Final || !c.entitiesSpawned) return;
            var reg = Region(w.dim, c.cx, c.cz, true);
            var rec = new ChunkRecord { cx = c.cx, cz = c.cz, entitiesHandled = true };
            foreach (var kv in c.mods) rec.mods[kv.Key] = kv.Value;
            foreach (var kv in c.blockEntities)
            {
                var d = new Dictionary<string, string>();
                try { kv.Value.Save(d); } catch (Exception e) { Debug.LogWarning("BE save failed: " + e.Message); }
                rec.blockEntities.Add((kv.Key, kv.Value.TypeId, d));
            }
            if (c.unloadEntities != null) rec.entities.AddRange(c.unloadEntities);
            else CollectEntities(w, c, rec.entities, false);
            c.unloadEntities = null;
            reg[World.ChunkKey(c.cx, c.cz)] = rec;
            dirtyRegions[(int)w.dim].Add(RegionKey(c.cx >> 5, c.cz >> 5));
        }

        public static void CollectEntities(World w, Chunk c, List<SavedEntity> into, bool remove)
        {
            int x0 = c.cx << 4, z0 = c.cz << 4;
            foreach (var e in w.entities)
            {
                if (e.removed || e is Player || !e.ShouldSave || e.vehicle != null) continue;
                int ex = Mathf.FloorToInt(e.position.x), ez = Mathf.FloorToInt(e.position.z);
                if (ex < x0 || ex >= x0 + 16 || ez < z0 || ez >= z0 + 16) continue;
                into.Add(ToSaved(e));
                foreach (var p in e.passengers) if (!(p is Player) && p.ShouldSave) { var sp = ToSaved(p); sp.data = EncodeDict(new Dictionary<string, string>(DecodeDict(sp.data)) { ["riding"] = "1" }); into.Add(sp); }
                if (remove) e.Remove();
            }
        }

        public static SavedEntity ToSaved(Entity e)
        {
            var d = new Dictionary<string, string>();
            try { e.Save(d); } catch (Exception ex) { Debug.LogWarning("Entity save failed: " + ex.Message); }
            return new SavedEntity { type = e.TypeId, x = e.position.x, y = e.position.y, z = e.position.z, yaw = e.yaw, pitch = e.pitch, data = EncodeDict(d) };
        }

        // ------------------------------------------------------------------ encoding helpers
        public static string EncodeDict(Dictionary<string, string> d)
        {
            if (d == null || d.Count == 0) return "";
            var sb = new StringBuilder();
            foreach (var kv in d)
            {
                if (sb.Length > 0) sb.Append('\u001e');
                sb.Append(kv.Key).Append('\u001f').Append(kv.Value ?? "");
            }
            return sb.ToString();
        }
        public static Dictionary<string, string> DecodeDict(string s)
        {
            var d = new Dictionary<string, string>();
            if (string.IsNullOrEmpty(s)) return d;
            foreach (var part in s.Split('\u001e'))
            {
                int i = part.IndexOf('\u001f');
                if (i > 0) d[part.Substring(0, i)] = part.Substring(i + 1);
            }
            return d;
        }

        // ------------------------------------------------------------------ region IO
        Dictionary<long, ChunkRecord> ReadRegion(DimensionId dim, int rx, int rz)
        {
            string path = RegionPath(dim, rx, rz);
            if (!File.Exists(path)) return null;
            try
            {
                var result = new Dictionary<long, ChunkRecord>();
                using (var fs = File.OpenRead(path))
                using (var r = new BinaryReader(fs, Encoding.UTF8))
                {
                    if (r.ReadInt32() != Magic) return null;
                    int ver = r.ReadInt32();
                    int n = r.ReadInt32();
                    for (int i = 0; i < n; i++)
                    {
                        var rec = new ChunkRecord { cx = r.ReadInt32(), cz = r.ReadInt32(), entitiesHandled = r.ReadBoolean() };
                        int pal = r.ReadInt32();
                        var palette = new ushort[pal];
                        for (int k = 0; k < pal; k++)
                        {
                            string id = r.ReadString(); int meta = r.ReadUInt16();
                            var b = Blocks.Get(id);
                            palette[k] = b == null ? (ushort)0 : b.State(Mathf.Min(meta, Math.Max(1, b.stateCount) - 1));
                        }
                        int mc = r.ReadInt32();
                        for (int k = 0; k < mc; k++) { int idx = r.ReadInt32(); int pi = r.ReadUInt16(); rec.mods[idx] = pi < pal ? palette[pi] : (ushort)0; }
                        int bec = r.ReadInt32();
                        for (int k = 0; k < bec; k++)
                        {
                            int idx = r.ReadInt32(); string type = r.ReadString();
                            rec.blockEntities.Add((idx, type, DecodeDict(r.ReadString())));
                        }
                        int ec = r.ReadInt32();
                        for (int k = 0; k < ec; k++)
                            rec.entities.Add(new SavedEntity { type = r.ReadString(), x = r.ReadSingle(), y = r.ReadSingle(), z = r.ReadSingle(), yaw = r.ReadSingle(), pitch = r.ReadSingle(), data = r.ReadString() });
                        result[World.ChunkKey(rec.cx, rec.cz)] = rec;
                    }
                }
                return result;
            }
            catch (Exception e) { Debug.LogError("Region read failed " + path + ": " + e); return null; }
        }

        void WriteRegion(DimensionId dim, long rk, Dictionary<long, ChunkRecord> reg)
        {
            int rx = (int)(rk >> 32), rz = (int)(uint)(rk & 0xFFFFFFFF);
            string path = RegionPath(dim, rx, rz);
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            string tmp = path + ".tmp";
            using (var fs = File.Create(tmp))
            using (var w = new BinaryWriter(fs, Encoding.UTF8))
            {
                w.Write(Magic); w.Write(Version); w.Write(reg.Count);
                foreach (var rec in reg.Values)
                {
                    w.Write(rec.cx); w.Write(rec.cz); w.Write(rec.entitiesHandled);
                    var palIdx = new Dictionary<ushort, int>(); var pal = new List<ushort>();
                    foreach (var st in rec.mods.Values) if (!palIdx.ContainsKey(st)) { palIdx[st] = pal.Count; pal.Add(st); }
                    w.Write(pal.Count);
                    foreach (var st in pal) { var b = Blocks.ByState[st]; w.Write(b.id); w.Write((ushort)(st - b.baseState)); }
                    w.Write(rec.mods.Count);
                    foreach (var kv in rec.mods) { w.Write(kv.Key); w.Write((ushort)palIdx[kv.Value]); }
                    w.Write(rec.blockEntities.Count);
                    foreach (var (idx, type, data) in rec.blockEntities) { w.Write(idx); w.Write(type); w.Write(EncodeDict(data)); }
                    w.Write(rec.entities.Count);
                    foreach (var e in rec.entities) { w.Write(e.type ?? ""); w.Write(e.x); w.Write(e.y); w.Write(e.z); w.Write(e.yaw); w.Write(e.pitch); w.Write(e.data ?? ""); }
                }
            }
            if (File.Exists(path)) File.Delete(path);
            File.Move(tmp, path);
        }

        // ------------------------------------------------------------------ whole save
        public void SaveAll(Player player)
        {
            var sw = System.Diagnostics.Stopwatch.StartNew();
            try
            {
                foreach (var w in session.worlds)
                {
                    if (w == null) continue;
                    foreach (var c in w.chunks.Values) if (c.stage >= (int)ChunkStage.Final && c.entitiesSpawned) StoreChunk(w, c);
                }
                for (int d = 0; d < 3; d++)
                {
                    foreach (var rk in dirtyRegions[d])
                        if (regions[d].TryGetValue(rk, out var reg)) WriteRegion((DimensionId)d, rk, reg);
                    dirtyRegions[d].Clear();
                }
                WriteLevel(player);
                lastSaveTime = Time.realtimeSinceStartup;
                Debug.Log($"[Save] Saved world '{session.worldName}' in {sw.ElapsedMilliseconds} ms");
            }
            catch (Exception e) { Debug.LogError("[Save] failed: " + e); }
        }

        void WriteLevel(Player player)
        {
            var d = new Dictionary<string, string>
            {
                ["name"] = session.worldName, ["seed"] = session.seed.ToString(), ["dayTime"] = session.dayTime.ToString(), ["gameTime"] = session.gameTime.ToString(),
                ["difficulty"] = ((int)session.difficulty).ToString(), ["defaultMode"] = ((int)session.defaultMode).ToString(),
                ["raining"] = session.raining ? "1" : "0", ["thundering"] = session.thundering ? "1" : "0", ["rainTime"] = session.rainTime.ToString(), ["thunderTime"] = session.thunderTime.ToString(),
                ["doDaylightCycle"] = B(session.doDaylightCycle), ["doWeatherCycle"] = B(session.doWeatherCycle), ["doMobSpawning"] = B(session.doMobSpawning), ["keepInventory"] = B(session.keepInventory),
                ["doFireTick"] = B(session.doFireTick), ["mobGriefing"] = B(session.mobGriefing), ["randomTickSpeed"] = session.randomTickSpeed.ToString(),
                ["lastPlayed"] = DateTime.Now.ToString("o", CultureInfo.InvariantCulture), ["version"] = Version.ToString()
            };
            if (session.worldSpawn.HasValue) d["spawn"] = V(session.worldSpawn.Value);
            if (session.dragonFight != null) { var df = new Dictionary<string, string>(); session.dragonFight.Save(df); d["dragon"] = EncodeDict(df); }
            MobSpawner.Save(d);
            // known portal frames per dimension, so linked portals keep leading to the same place after a reload
            for (int dim = 0; dim < session.portals.Length; dim++)
            {
                var ps = new StringBuilder();
                foreach (var pp in session.portals[dim]) ps.Append(ps.Length > 0 ? ";" : "").Append(pp.x).Append(',').Append(pp.y).Append(',').Append(pp.z);
                if (ps.Length > 0) d["portals" + dim] = ps.ToString();
            }
            if (player != null) { var pd = new Dictionary<string, string>(); player.SaveFull(pd); d["player"] = EncodeDict(pd); playerData = pd; }
            else if (playerData != null) d["player"] = EncodeDict(playerData);
            var sb = new StringBuilder();
            foreach (var kv in d) sb.Append(kv.Key).Append('=').Append(kv.Value.Replace("\\", "\\\\").Replace("\n", "\\n")).Append('\n');
            string path = Path.Combine(dir, "level.txt");
            File.WriteAllText(path + ".tmp", sb.ToString(), Encoding.UTF8);
            if (File.Exists(path)) File.Delete(path);
            File.Move(path + ".tmp", path);
        }

        static string B(bool b) => b ? "1" : "0";
        static string V(Vector3 v) => v.x.ToString("R", CultureInfo.InvariantCulture) + "," + v.y.ToString("R", CultureInfo.InvariantCulture) + "," + v.z.ToString("R", CultureInfo.InvariantCulture);
        public static Vector3 ParseV(string s)
        {
            var p = s.Split(',');
            return new Vector3(float.Parse(p[0], CultureInfo.InvariantCulture), float.Parse(p[1], CultureInfo.InvariantCulture), float.Parse(p[2], CultureInfo.InvariantCulture));
        }

        static Dictionary<string, string> ReadKV(string path)
        {
            var d = new Dictionary<string, string>();
            if (!File.Exists(path)) return d;
            foreach (var line in File.ReadAllLines(path, Encoding.UTF8))
            {
                int i = line.IndexOf('=');
                if (i <= 0) continue;
                d[line.Substring(0, i)] = line.Substring(i + 1).Replace("\\n", "\n").Replace("\\\\", "\\");
            }
            return d;
        }

        public static bool Exists(string folder) => File.Exists(Path.Combine(SavesRoot, folder, "level.txt"));

        public static GameSession LoadSession(string folder)
        {
            string path = Path.Combine(SavesRoot, folder, "level.txt");
            var d = ReadKV(path);
            if (d.Count == 0) return null;
            int seed = int.Parse(d["seed"]);
            var s = new GameSession(d.TryGetValue("name", out var n) ? n : folder, seed);
            long.TryParse(Get(d, "dayTime", "0"), out s.dayTime);
            long.TryParse(Get(d, "gameTime", "0"), out s.gameTime);
            s.difficulty = (Difficulty)int.Parse(Get(d, "difficulty", "2"));
            s.defaultMode = (GameMode)int.Parse(Get(d, "defaultMode", "1"));
            s.raining = Get(d, "raining", "0") == "1"; s.thundering = Get(d, "thundering", "0") == "1";
            int.TryParse(Get(d, "rainTime", "12000"), out s.rainTime); int.TryParse(Get(d, "thunderTime", "30000"), out s.thunderTime);
            s.rainLevel = s.raining ? 1 : 0; s.thunderLevel = s.thundering ? 1 : 0;
            s.doDaylightCycle = Get(d, "doDaylightCycle", "1") == "1"; s.doWeatherCycle = Get(d, "doWeatherCycle", "1") == "1";
            s.doMobSpawning = Get(d, "doMobSpawning", "1") == "1"; s.keepInventory = Get(d, "keepInventory", "0") == "1";
            s.doFireTick = Get(d, "doFireTick", "1") == "1"; s.mobGriefing = Get(d, "mobGriefing", "1") == "1";
            int.TryParse(Get(d, "randomTickSpeed", "3"), out s.randomTickSpeed);
            if (d.TryGetValue("spawn", out var sp)) s.worldSpawn = ParseV(sp);
            s.save = new SaveManager(s, folder);
            if (d.TryGetValue("player", out var pd)) s.save.playerData = DecodeDict(pd);
            if (d.TryGetValue("dragon", out var dr)) { s.dragonFight = new DragonFight(s); s.dragonFight.Load(DecodeDict(dr)); }
            MobSpawner.Load(d);
            for (int dim = 0; dim < s.portals.Length; dim++)
            {
                if (!d.TryGetValue("portals" + dim, out var ps)) continue;
                foreach (var part in ps.Split(';'))
                {
                    var c = part.Split(',');
                    if (c.Length == 3 && int.TryParse(c[0], out int px) && int.TryParse(c[1], out int py) && int.TryParse(c[2], out int pz))
                        s.portals[dim].Add(new Int3(px, py, pz));
                }
            }
            return s;
        }

        static string Get(Dictionary<string, string> d, string k, string def) => d.TryGetValue(k, out var v) ? v : def;

        public static List<WorldInfo> ListWorlds()
        {
            var list = new List<WorldInfo>();
            if (!Directory.Exists(SavesRoot)) return list;
            foreach (var dir in Directory.GetDirectories(SavesRoot))
            {
                var d = ReadKV(Path.Combine(dir, "level.txt"));
                if (d.Count == 0) continue;
                var wi = new WorldInfo { folder = Path.GetFileName(dir), name = Get(d, "name", Path.GetFileName(dir)) };
                int.TryParse(Get(d, "seed", "0"), out wi.seed);
                long.TryParse(Get(d, "dayTime", "0"), out wi.dayTime);
                wi.mode = (GameMode)int.Parse(Get(d, "defaultMode", "1"));
                DateTime.TryParse(Get(d, "lastPlayed", ""), CultureInfo.InvariantCulture, DateTimeStyles.RoundtripKind, out wi.lastPlayed);
                list.Add(wi);
            }
            list.Sort((a, b) => b.lastPlayed.CompareTo(a.lastPlayed));
            return list;
        }

        public static void Delete(string folder)
        {
            string p = Path.Combine(SavesRoot, folder);
            if (Directory.Exists(p)) Directory.Delete(p, true);
        }

        public static string UniqueFolder(string name)
        {
            string baseName = "";
            foreach (char ch in name) baseName += char.IsLetterOrDigit(ch) || ch == ' ' || ch == '_' || ch == '-' ? ch : '_';
            baseName = baseName.Trim();
            if (baseName.Length == 0) baseName = "World";
            string f = baseName; int i = 1;
            while (Directory.Exists(Path.Combine(SavesRoot, f))) f = baseName + " (" + (++i) + ")";
            return f;
        }
    }
}
