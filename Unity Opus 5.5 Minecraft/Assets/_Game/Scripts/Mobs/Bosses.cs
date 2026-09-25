using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace MCR
{
    /// <summary>Where the End fight stands.</summary>
    public enum DragonFightMood { Dormant, Fighting, Dying, Won, Respawning }

    /// <summary>What the dragon is doing right now.</summary>
    public enum DragonPhase { Circling, Strafing, Charging, Approach, Landing, Perched, Takeoff, Dying }

    /// <summary>
    /// The End fight: owns the dragon, the pillar crystals, the exit portal and the gateways each win opens.
    /// The game loop ticks it every End tick, and it ticks the dragon itself so the boss keeps flying
    /// even when the player stands beyond the normal entity tick range (the spawn platform is 100 blocks out).
    /// </summary>
    public sealed class DragonFight
    {
        /// <summary>Set whenever a fight exists, so item code can reach it without a world lookup.</summary>
        public static DragonFight Instance;

        /// <summary>Bedrock base of the exit fountain (EndGenerator.BuildExitPortal's default).</summary>
        public const int PortalY = 60;
        /// <summary>Top of the fountain's centre pillar: where the dragon perches and the egg is placed.</summary>
        public static readonly Vector3 Fountain = new Vector3(0.5f, PortalY + 5, 0.5f);
        const int GatewayRing = 96, GatewayY = 75, GatewayCount = 20, RitualTicks = 320;
        static readonly Int3[] RimSpots = { new Int3(3, 0, 0), new Int3(-3, 0, 0), new Int3(0, 0, 3), new Int3(0, 0, -3) };

        public readonly GameSession session;
        public EnderDragonMob dragon;
        public DragonFightMood state = DragonFightMood.Dormant;
        public bool killed, hasBeenDefeatedOnce;
        public int crystalsAlive;

        bool dragonSpawned, portalPending, eggPending, announced;
        int pillarMask, gatewayPending, waitTicks, ritualTicks;
        Vector3 lastDragonPos = new Vector3(0.5f, 128f, 0.5f);
        long lastDriveTick = long.MinValue;
        readonly List<int> gateways = new List<int>();
        readonly List<EndCrystal> ritual = new List<EndCrystal>();

        public DragonFight(GameSession s)
        {
            session = s;
            Instance = this;
            // gateways open in a fixed, seed shuffled order around the island
            for (int i = 0; i < GatewayCount; i++) gateways.Add(i);
            var rng = new RNG(s != null ? s.seed : 0, 0, 0, 0x6A7E);
            rng.Shuffle(gateways);
        }

        /// <summary>True while the fight ticks its dragon itself; the entity loop then leaves it alone.</summary>
        public bool IsDriving(World w) => w != null && lastDriveTick != long.MinValue && w.tickCount >= lastDriveTick && w.tickCount - lastDriveTick <= 1;

        bool DragonAlive => dragon != null && !dragon.removed && !dragon.dead && dragon.phase != DragonPhase.Dying;

        // ------------------------------------------------------------------ ticking
        /// <summary>Called every world tick for the End.</summary>
        public void Tick(World end)
        {
            if (end == null || end.dim != DimensionId.End) return;
            Instance = this;
            bool players = AnyPlayer(end);
            if (dragon != null && dragon.removed) dragon = null;
            if (dragon == null && end.tickCount % 20 == 0) Adopt(end);
            if (dragon != null) Drive(end, players);
            if (!players) return;
            if (!announced) Announce(end);
            if (end.tickCount % 20 == 0) crystalsAlive = CountCrystals(end);
            if (state == DragonFightMood.Respawning) TickRitual(end);
            else if (!killed)
            {
                if (pillarMask != AllPillars(end)) PlacePillarCrystals(end);
                if (dragon == null) EnsureDragon(end);
            }
            else
            {
                // the respawn ritual is only ever started by placing the fourth crystal (OnCrystalPlaced)
                if (portalPending && PortalReady(end)) OpenPortal(end);
                if (gatewayPending > 0) TryGateway(end);
            }
            UpdateMood();
        }

        void Drive(World end, bool players)
        {
            lastDriveTick = end.tickCount;
            var d = dragon;
            if (!players || !d.inWorld) return;
            if (end.ReadyChunk(Mathf.FloorToInt(d.position.x), Mathf.FloorToInt(d.position.z)) == null) return;
            d.driven = true;
            try { d.Tick(); }
            catch (Exception e) { Debug.LogError("[DragonFight] dragon tick failed: " + e); d.Remove(); }
            finally { d.driven = false; }
            if (!d.removed) lastDragonPos = d.position;
        }

        void Adopt(World end)
        {
            foreach (var e in end.entities) if (e is EnderDragonMob d && !d.removed && !d.dead) { Take(d); return; }
            foreach (var e in end.entitiesToAdd) if (e is EnderDragonMob d && !d.removed && !d.dead) { Take(d); return; }
        }

        void Take(EnderDragonMob d)
        {
            d.fight = this;
            dragon = d;
            dragonSpawned = true;
            waitTicks = 0;
        }

        /// <summary>First visit: spawn once the island centre is loaded. Later visits: a saved dragon returns
        /// with its chunk, so it is only replaced once that chunk is loaded and the dragon is still missing.</summary>
        void EnsureDragon(World end)
        {
            if (!dragonSpawned)
            {
                if (!ChunkSettled(end, 0, 0) || ++waitTicks < 40) return;
            }
            else
            {
                if (!ChunkSettled(end, Mathf.FloorToInt(lastDragonPos.x), Mathf.FloorToInt(lastDragonPos.z)) || ++waitTicks < 100) return;
            }
            waitTicks = 0;
            SpawnDragon(end);
        }

        void SpawnDragon(World end)
        {
            var d = MobRegistry.Spawn(end, "ender_dragon", new Vector3(0.5f, 128f, 0.5f), SpawnReason.Structure) as EnderDragonMob;
            if (d == null) return;
            Take(d);
            killed = false;
            state = DragonFightMood.Fighting;
            lastDragonPos = d.position;
        }

        void Announce(World end)
        {
            announced = true;
            if (killed || !DragonAlive) return;
            // restart the music clock so the End theme comes in, and let the dragon make itself heard
            Sounds.PlayMenuMusicSoon();
            var p = LocalPlayer(end);
            if (p != null) Sounds.Play("entity.ender_dragon.growl", p.EyePosition + (dragon.position - p.EyePosition).normalized * 12f, 3f, 0.8f);
        }

        void UpdateMood()
        {
            if (state == DragonFightMood.Respawning) return;
            if (killed) state = DragonFightMood.Won;
            else if (dragon != null && dragon.phase == DragonPhase.Dying) state = DragonFightMood.Dying;
            else if (dragon != null || dragonSpawned) state = DragonFightMood.Fighting;
            else state = DragonFightMood.Dormant;
        }

        // ------------------------------------------------------------------ entering
        /// <summary>A player came through the end portal. The island may still be loading, so the dragon
        /// appears here only if its chunk is ready; otherwise Tick spawns it a moment later.</summary>
        public void OnPlayerEnter(World end)
        {
            if (end == null || end.dim != DimensionId.End) return;
            Instance = this;
            announced = false;
            if (killed || state == DragonFightMood.Respawning) return;
            if (state == DragonFightMood.Dormant) state = DragonFightMood.Fighting;
            if (dragon == null) Adopt(end);
            if (dragon == null && !dragonSpawned && ChunkSettled(end, 0, 0)) SpawnDragon(end);
        }

        // ------------------------------------------------------------------ crystals
        static int AllPillars(World end) => end.generator is EndGenerator g ? (1 << g.pillars.Length) - 1 : 0;

        /// <summary>Each pillar gets its crystal once its chunk is loaded (the generator only builds the pillars).</summary>
        void PlacePillarCrystals(World end)
        {
            var gen = end.generator as EndGenerator;
            if (gen == null) return;
            for (int i = 0; i < gen.pillars.Length; i++)
            {
                if ((pillarMask & (1 << i)) != 0) continue;
                var p = gen.pillars[i];
                if (!ChunkSettled(end, p.x, p.z)) continue;
                var at = new Vector3(p.x + 0.5f, p.height + 2, p.z + 0.5f);
                if (CrystalNear(end, at, 2.5f) == null) EndCrystal.Spawn(end, at, true);
                pillarMask |= 1 << i;
            }
        }

        public int CountCrystals(World end)
        {
            if (end == null) return 0;
            return Count(end.entities) + Count(end.entitiesToAdd);
        }

        static int Count(List<Entity> list)
        {
            int n = 0;
            foreach (var e in list)
                if (e is EndCrystal c && !c.removed && c.position.x * c.position.x + c.position.z * c.position.z < 150f * 150f) n++;
            return n;
        }

        static EndCrystal CrystalNear(World end, Vector3 at, float r)
        {
            var c = Near(end.entities, at, r);
            return c ?? Near(end.entitiesToAdd, at, r);
        }

        static EndCrystal Near(List<Entity> list, Vector3 at, float r)
        {
            foreach (var e in list) if (e is EndCrystal c && !c.removed && (c.position - at).sqrMagnitude < r * r) return c;
            return null;
        }

        static bool OnPortalRim(Int3 pos)
        {
            if (pos.y != PortalY + 2) return false;
            float d = Mathf.Sqrt(pos.x * pos.x + pos.z * pos.z);
            return d > 2.5f && d <= 4.5f;
        }

        /// <summary>A crystal went onto the exit portal's rim. Mid-fight it feeds the dragon back to full
        /// health; once the fight is won, four of them on the four sides begin the respawn ritual.</summary>
        public void OnCrystalPlaced(World w, Int3 pos)
        {
            if (w == null || w.dim != DimensionId.End) return;
            crystalsAlive = CountCrystals(w);
            if (!OnPortalRim(pos)) return;
            if (!killed && DragonAlive)
            {
                dragon.health = dragon.maxHealth;
                RespawnEffect(w, pos.Center);
                return;
            }
            if (killed && state != DragonFightMood.Respawning) CheckRitual(w);
        }

        public void OnCrystalDestroyed(EndCrystal c, DamageSource src)
        {
            var end = c != null ? c.world : null;
            if (end == null || end.dim != DimensionId.End) return;
            crystalsAlive = CountCrystals(end);
            if (state == DragonFightMood.Respawning && ritual.Contains(c)) { AbortRitual(end); return; }
            if (DragonAlive) dragon.OnCrystalDestroyed(c, src);
        }

        static void RespawnEffect(World w, Vector3 at)
        {
            for (int i = 0; i < 30; i++) Particles.EndRod(w, at + new Vector3(w.rand.Range(-1f, 1f), w.rand.Range(0f, 3f), w.rand.Range(-1f, 1f)));
            Particles.Explosion(w, at + Vector3.up, 1f);
            Sounds.Play("block.end_portal.spawn", at, 4f, 1.2f);
        }

        // ------------------------------------------------------------------ respawn ritual
        void CheckRitual(World end)
        {
            ritual.Clear();
            foreach (var s in RimSpots)
            {
                var c = CrystalNear(end, new Vector3(s.x + 0.5f, PortalY + 2, s.z + 0.5f), 1.5f);
                if (c == null) { ritual.Clear(); return; }
                ritual.Add(c);
            }
            state = DragonFightMood.Respawning;
            ritualTicks = 0;
            pillarMask = 0;
            EndGenerator.BuildExitPortal(new WorldAccess(end), false, PortalY);
            Sounds.Play("entity.ender_dragon.growl", Fountain, 8f, 0.6f);
        }

        void TickRitual(World end)
        {
            foreach (var c in ritual) if (c.removed) { AbortRitual(end); return; }
            ritualTicks++;
            var gen = end.generator as EndGenerator;
            Vector3 beam = Fountain + Vector3.up * 36f;
            // after the sky beam, the crystals relight the pillars one by one
            if (gen != null && ritualTicks > 100)
            {
                int i = (ritualTicks - 100) / 20;
                if (i < gen.pillars.Length)
                {
                    var p = gen.pillars[i];
                    beam = new Vector3(p.x + 0.5f, p.height + 2.5f, p.z + 0.5f);
                    if ((ritualTicks - 100) % 20 == 0) RelightPillar(end, p, i);
                }
            }
            foreach (var c in ritual) c.beamTarget = beam;
            if (ritualTicks % 4 == 0) Particles.EndRod(end, Vector3.Lerp(Fountain, beam, end.rand.NextFloat()));
            if (ritualTicks >= RitualTicks) FinishRitual(end);
        }

        void RelightPillar(World end, EndGenerator.Pillar p, int index)
        {
            var top = new Int3(p.x, p.height + 1, p.z);
            if (end.IsLoaded(top) && end.IsAir(top)) end.SetState(top, Blocks.StateOf("bedrock"));
            var at = new Vector3(p.x + 0.5f, p.height + 2, p.z + 0.5f);
            if (end.IsLoaded(top) && CrystalNear(end, at, 2.5f) == null) EndCrystal.Spawn(end, at, true);
            pillarMask |= 1 << index;
            Particles.Explosion(end, at, 2f);
            Sounds.Play("entity.generic.explode", at, 6f, 1.2f);
        }

        void FinishRitual(World end)
        {
            foreach (var c in ritual) { c.beamTarget = null; Particles.Explosion(end, c.position, 1.5f); c.Remove(); }
            ritual.Clear();
            Particles.Explosion(end, Fountain + Vector3.up * 2f, 4f);
            Sounds.Play("entity.generic.explode", Fountain, 8f, 0.8f);
            killed = false;
            state = DragonFightMood.Fighting;
            SpawnDragon(end);
        }

        void AbortRitual(World end)
        {
            foreach (var c in ritual) if (!c.removed) c.beamTarget = null;
            ritual.Clear();
            state = DragonFightMood.Won;
            pillarMask = AllPillars(end);
            portalPending = true;
            if (PortalReady(end)) OpenPortal(end);
        }

        // ------------------------------------------------------------------ ending
        /// <summary>The death sequence finished: light the exit, place the egg on a first win, open a gateway.</summary>
        public void OnDragonKilled(EnderDragonMob d)
        {
            var end = d != null && d.world != null ? d.world : EndWorld;
            bool first = !hasBeenDefeatedOnce;
            killed = true;
            hasBeenDefeatedOnce = true;
            state = DragonFightMood.Won;
            if (dragon == d) dragon = null;
            dragonSpawned = false;
            waitTicks = 0;
            portalPending = true;
            eggPending |= first;
            gatewayPending++;
            if (end == null || end.dim != DimensionId.End) return;
            if (PortalReady(end)) OpenPortal(end);
            TryGateway(end);
            // everyone in the End shares the win
            foreach (var e in end.entities) if (e is Player p && !p.removed) Achievements.Grant(p, "the_end");
        }

        static bool PortalReady(World end) => end.IsLoaded(-4, -4) && end.IsLoaded(4, -4) && end.IsLoaded(-4, 4) && end.IsLoaded(4, 4);

        void OpenPortal(World end)
        {
            EndGenerator.BuildExitPortal(new WorldAccess(end), true, PortalY);
            portalPending = false;
            Sounds.Play("block.end_portal.spawn", Fountain, 8f, 1f);
            if (!eggPending) return;
            eggPending = false;
            var egg = Blocks.Get("dragon_egg");
            if (egg == null) return;
            var at = Int3.Floor(Fountain);
            for (int i = 0; i < 8 && !end.IsAir(at); i++) at = at.Offset(Dir.Up);
            end.SetState(at, egg.DefaultState);
        }

        void TryGateway(World end)
        {
            if (gatewayPending <= 0) return;
            if (gateways.Count == 0) { gatewayPending = 0; return; }
            int idx = gateways[gateways.Count - 1];
            float a = idx / (float)GatewayCount * Mathf.PI * 2f;
            var at = new Int3(Mathf.RoundToInt(Mathf.Cos(a) * GatewayRing), GatewayY, Mathf.RoundToInt(Mathf.Sin(a) * GatewayRing));
            if (!end.IsLoaded(at.x - 1, at.z - 1) || !end.IsLoaded(at.x + 1, at.z + 1)) return;
            gateways.RemoveAt(gateways.Count - 1);
            gatewayPending--;
            BuildGateway(end, at);
        }

        static void BuildGateway(World end, Int3 at)
        {
            ushort bedrock = Blocks.StateOf("bedrock");
            var gw = Blocks.Get("end_gateway");
            if (gw == null || bedrock == 0) return;
            // the portal block sits in a small bedrock cross above and below
            for (int dy = -1; dy <= 1; dy += 2)
            {
                end.SetState(at.Offset(0, dy, 0), bedrock);
                for (int i = 0; i < 4; i++) end.SetState(at.Offset(DirUtil.Horizontal[i]).Offset(0, dy, 0), bedrock);
            }
            end.SetState(at, gw.DefaultState);
            Particles.Explosion(end, at.Center, 2f);
            Sounds.Play("block.end_gateway.spawn", at.Center, 8f, 1f);
        }

        /// <summary>The lit exit portal was used, called from <see cref="EndPortalBlock.OnEntityInside"/> for an
        /// <c>end_portal</c> block in the End with <see cref="killed"/> set — the dragon must be dead before the fountain
        /// opens, so this is the only door home the fight leaves.</summary>
        public void OnExitPortalUsed(Player p)
        {
            if (p == null || p.world == null || p.world.dim != DimensionId.End || !killed) return;
            // Portals.TravelEnd rolls the credits (GameManager.ShowCredits) before sending the player home
            Portals.TravelEnd(p);
        }

        // ------------------------------------------------------------------ helpers
        World EndWorld => session != null ? session.worlds[(int)DimensionId.End] : null;

        static bool AnyPlayer(World w)
        {
            foreach (var e in w.entities) if (e is Player p && !p.removed) return true;
            return false;
        }

        static Player LocalPlayer(World w)
        {
            var p = GameManager.Instance != null ? GameManager.Instance.player : null;
            if (p != null && p.world == w) return p;
            foreach (var e in w.entities) if (e is Player q && !q.removed) return q;
            return null;
        }

        /// <summary>Loaded, lit and with its saved entities restored.</summary>
        static bool ChunkSettled(World w, int x, int z)
        {
            var c = w.ReadyChunk(x, z);
            return c != null && c.lit && (c.entitiesSpawned || w.session == null || w.session.save == null);
        }

        // ------------------------------------------------------------------ persistence
        public void Save(Dictionary<string, string> d)
        {
            var ci = CultureInfo.InvariantCulture;
            d["killed"] = killed ? "1" : "0";
            d["defeated"] = hasBeenDefeatedOnce ? "1" : "0";
            d["crystals"] = crystalsAlive.ToString(ci);
            d["pillars"] = pillarMask.ToString(ci);
            d["spawned"] = dragonSpawned ? "1" : "0";
            d["dragonPos"] = lastDragonPos.x.ToString("R", ci) + "," + lastDragonPos.y.ToString("R", ci) + "," + lastDragonPos.z.ToString("R", ci);
            d["gateways"] = string.Join(",", gateways);
            d["gatewayPending"] = gatewayPending.ToString(ci);
            d["pending"] = ((portalPending ? 1 : 0) | (eggPending ? 2 : 0)).ToString(ci);
        }

        public void Load(Dictionary<string, string> d)
        {
            if (d == null) return;
            var ci = CultureInfo.InvariantCulture;
            killed = d.TryGetValue("killed", out var k) && k == "1";
            hasBeenDefeatedOnce = killed || (d.TryGetValue("defeated", out var f) && f == "1");
            dragonSpawned = d.TryGetValue("spawned", out var sp) && sp == "1";
            if (d.TryGetValue("crystals", out var c)) int.TryParse(c, NumberStyles.Integer, ci, out crystalsAlive);
            if (d.TryGetValue("pillars", out var pm)) int.TryParse(pm, NumberStyles.Integer, ci, out pillarMask);
            if (d.TryGetValue("gatewayPending", out var gp)) int.TryParse(gp, NumberStyles.Integer, ci, out gatewayPending);
            if (d.TryGetValue("pending", out var pe) && int.TryParse(pe, NumberStyles.Integer, ci, out int bits)) { portalPending = (bits & 1) != 0; eggPending = (bits & 2) != 0; }
            if (d.TryGetValue("dragonPos", out var dp))
            {
                try { lastDragonPos = SaveManager.ParseV(dp); } catch (FormatException) { } catch (IndexOutOfRangeException) { }
            }
            if (d.TryGetValue("gateways", out var g))
            {
                gateways.Clear();
                foreach (var part in g.Split(',')) if (int.TryParse(part, NumberStyles.Integer, ci, out int gi) && gi >= 0 && gi < GatewayCount) gateways.Add(gi);
            }
            state = killed ? DragonFightMood.Won : dragonSpawned ? DragonFightMood.Fighting : DragonFightMood.Dormant;
        }
    }

    // =====================================================================================================
    /// <summary>
    /// The End boss. Flies a ring of nodes around the central island, strafes the player with breath
    /// fireballs, dives through them, perches on the fountain to breathe on whoever is underneath, and
    /// heals from the nearest live crystal. Death is a ten second climb above the fountain ending in a
    /// burst of xp. MobVisual animates the rig from RigKind.Dragon, so this class is behaviour only.
    /// </summary>
    public sealed class EnderDragonMob : Mob
    {
        const float HeadForward = 7f, HeadUp = 4.5f, PerchedHeadUp = 1.5f, HeadRadius = 4.5f;
        const float CrystalRange = 32f, CrystalHealPerTick = 0.1f, DeathHeight = 20f;
        const int DeathTicks = 200, RepeatXp = 500;

        public DragonFight fight;
        public DragonPhase phase = DragonPhase.Circling;
        /// <summary>Set by the fight while it ticks the dragon, so the entity loop does not tick it twice.</summary>
        public bool driven;
        public bool inWorld;
        public EndCrystal healingCrystal;
        public int deathTicks;

        Vector3 home, goal, chargeAt, breathAt;
        int node = -1, phaseTicks, crystalScan, breathTicks, breathCooldown, biteCooldown, targetScan, dyingApproach, growlTimer = 100;
        bool clockwise = true, backlash;
        float sitDamage;
        int xpTotal, xpDropped;
        DamageSource killSource;

        static readonly Vector3[] Nodes = BuildNodes();
        static readonly List<Entity> near = new List<Entity>();
        static readonly List<LivingEntity> struck = new List<LivingEntity>();
        /// <summary>What the dragon cannot smash through: the island, the pillars and the portals.</summary>
        static readonly HashSet<string> Immune = new HashSet<string> { "bedrock", "obsidian", "crying_obsidian", "end_stone", "iron_bars", "end_portal", "end_portal_frame", "end_gateway", "respawn_anchor", "barrier", "reinforced_deepslate" };

        /// <summary>Twelve outer, eight middle and four inner nodes around the fountain.</summary>
        static Vector3[] BuildNodes()
        {
            var n = new Vector3[24];
            for (int i = 0; i < 12; i++) n[i] = Ring(i / 12f, 60f, 80f + (i % 3) * 5f);
            for (int i = 0; i < 8; i++) n[12 + i] = Ring(i / 8f + 1f / 16f, 42f, 88f + (i % 2) * 6f);
            for (int i = 0; i < 4; i++) n[20 + i] = Ring(i / 4f + 1f / 8f, 28f, 76f);
            return n;
        }

        static Vector3 Ring(float t, float r, float y)
        {
            float a = t * Mathf.PI * 2f;
            return new Vector3(Mathf.Cos(a) * r + 0.5f, y, Mathf.Sin(a) * r + 0.5f);
        }

        public override void Setup(MobDef d)
        {
            base.Setup(d);
            // it flies through the world instead of colliding with it, so water, lava and walls never push it
            noPhysics = true;
            noGravity = true;
            pushable = false;
            knockbackResistance = 1f;
            armorValueBase = Mathf.Max(d.armor, 6f);
            canBreatheUnderwater = true;
            persistent = true;
        }

        public override void OnInitialSpawn(SpawnReason reason)
        {
            persistent = true;
            home = InEnd ? new Vector3(0.5f, 0f, 0.5f) : position;
            Enter(DragonPhase.Circling);
        }

        public override void OnAddedToWorld() { base.OnAddedToWorld(); inWorld = true; }

        bool InEnd => world != null && world.dim == DimensionId.End;
        Vector3 Perch => DragonFight.Fountain;
        Vector3 Fwd => MathX.YawPitchToDir(yaw, 0f);
        Vector3 BodyCenter => position + Vector3.up * (height * 0.5f);

        /// <summary>Where the jaws are: ahead of the body, dipped low while perched.</summary>
        public Vector3 HeadPos
        {
            get
            {
                float turn = Mathf.Clamp(MathX.WrapAngle(lookYaw - bodyYaw), -60f, 60f) * 0.6f;
                return position + MathX.YawPitchToDir(bodyYaw + turn, 0f) * HeadForward + Vector3.up * (phase == DragonPhase.Perched ? PerchedHeadUp : HeadUp);
            }
        }

        public override bool CanDespawn => false;
        // growls and the death roar are played at boss volume by the dragon itself
        public override string AmbientSound => null;
        public override string DeathSound => null;
        public override void ApplyFallDamage(float dist) { }
        public override void OnStruckByLightning(LightningBolt bolt) { }
        /// <summary>No potion or effect takes hold on the dragon.</summary>
        public override void AddEffect(EffectInstance inst) { }
        /// <summary>The xp is dropped in stages by the death sequence instead.</summary>
        protected override int XpReward() => 0;
        protected override void OnVoid() { position.y = world.minY + 40f; velocity = Vector3.zero; }
        protected override void OnHurtEffects(DamageSource src) { Sounds.Play("entity.ender_dragon.hurt", position, 6f, VoicePitch); }

        // ------------------------------------------------------------------ tick
        public override void Tick()
        {
            // in the End the fight ticks the dragon every tick; the entity loop's call is ignored then
            if (!driven && fight != null && fight.IsDriving(world)) return;
            base.Tick();
            if (removed || dead) return;
            ShowBar();
            if (phase == DragonPhase.Dying) return;
            TickCrystals();
            ContactTick();
            if (age % 3 == 0 && phase != DragonPhase.Perched) Grief();
            if (--growlTimer <= 0)
            {
                growlTimer = 200 + world.rand.Next(300);
                Sounds.Play("entity.ender_dragon.growl", position, 6f, 0.9f + world.rand.NextFloat() * 0.2f);
            }
        }

        void ShowBar()
        {
            var gm = GameManager.Instance;
            var p = gm != null ? gm.player : null;
            if (p == null || gm.hud == null || p.world != world || (p.position - position).sqrMagnitude > 192f * 192f) return;
            gm.hud.ShowBossBar(DisplayName, phase == DragonPhase.Dying ? 0f : HealthFrac, true);
        }

        // ------------------------------------------------------------------ crystals
        void TickCrystals()
        {
            var c = healingCrystal;
            if (c != null && (c.removed || c.world != world || (c.position - position).sqrMagnitude > CrystalRange * CrystalRange)) { Unlink(); c = null; }
            if (c == null && ++crystalScan >= 10) { crystalScan = 0; healingCrystal = c = NearestCrystal(); }
            if (c == null) return;
            c.beamTarget = BodyCenter;
            // while the fight is still on, a live crystal keeps topping the dragon up
            if (health < maxHealth && (fight == null || !fight.killed)) Heal(CrystalHealPerTick);
        }

        EndCrystal NearestCrystal()
        {
            EndCrystal best = null; float bd = CrystalRange * CrystalRange;
            foreach (var e in world.entities)
            {
                if (!(e is EndCrystal c) || c.removed) continue;
                float d = (c.position - position).sqrMagnitude;
                if (d < bd) { bd = d; best = c; }
            }
            return best;
        }

        void Unlink()
        {
            if (healingCrystal != null) healingCrystal.beamTarget = null;
            healingCrystal = null;
        }

        /// <summary>The fight reports a destroyed crystal. Losing the one it was drawing on hurts; a player
        /// who broke it becomes the next strafe target.</summary>
        public void OnCrystalDestroyed(EndCrystal c, DamageSource src)
        {
            if (c == null || phase == DragonPhase.Dying) return;
            if (c == healingCrystal)
            {
                Unlink();
                backlash = true;
                Hurt(DamageSource.Explosion(src != null ? src.attacker : null, c.position), 10f);
                backlash = false;
            }
            if (src != null && src.attacker is Player p && phase == DragonPhase.Circling && Valid(p)) { target = p; Enter(DragonPhase.Strafing); }
        }

        // ------------------------------------------------------------------ contact
        static bool Hittable(LivingEntity le) => !le.dead && !le.removed && !(le is EnderDragonMob) && !(le is Player p && (p.IsCreative || p.IsSpectator));

        /// <summary>Wing beats fling whatever touches the body; the jaws bite whatever touches the head.</summary>
        void ContactTick()
        {
            bool sitting = phase == DragonPhase.Perched || phase == DragonPhase.Landing;
            struck.Clear();
            world.GetEntities(Bounds.Grow(1f, 0.5f, 1f), this, near);
            foreach (var e in near) if (e is LivingEntity le && Hittable(le)) struck.Add(le);
            var c = BodyCenter;
            foreach (var le in struck)
            {
                Vector3 d = le.position - c; d.y = 0;
                if (d.sqrMagnitude < 1e-4f) d = Fwd;
                d.Normalize();
                if (Vector3.Dot(le.velocity, d) < 1f) le.velocity += d * (sitting ? 0.5f : 1.6f) + Vector3.up * (sitting ? 0.2f : 0.4f);
                if (!sitting && hurtTime == 0) le.Hurt(DamageSource.MobAttack(this), 5f);
            }
            if (biteCooldown > 0) { biteCooldown--; return; }
            var head = HeadPos;
            struck.Clear();
            world.GetEntities(new AABB(head - Vector3.one * HeadRadius, head + Vector3.one * HeadRadius), this, near);
            foreach (var e in near)
                if (e is LivingEntity le && Hittable(le) && (le.position + Vector3.up * (le.height * 0.5f) - head).sqrMagnitude < HeadRadius * HeadRadius) struck.Add(le);
            if (struck.Count == 0) return;
            biteCooldown = 10;
            foreach (var le in struck)
            {
                if (!le.Hurt(DamageSource.MobAttack(this), 10f)) continue;
                Vector3 d = le.position - head; d.y = 0;
                if (d.sqrMagnitude > 1e-4f) le.velocity += d.normalized * 0.8f + Vector3.up * 0.3f;
            }
        }

        /// <summary>Whatever the dragon flies through breaks, apart from the island's own stone and the portals.</summary>
        void Grief()
        {
            if (world.session != null && !world.session.mobGriefing) return;
            var c = Int3.Floor(BodyCenter);
            int broken = BreakBox(c.x - 4, c.y - 2, c.z - 4, c.x + 4, c.y + 2, c.z + 4);
            var h = Int3.Floor(HeadPos);
            broken += BreakBox(h.x - 1, h.y - 1, h.z - 1, h.x + 1, h.y + 1, h.z + 1);
            if (broken > 0) Particles.Explosion(world, BodyCenter + Jitter(3f), 1f);
        }

        int BreakBox(int x0, int y0, int z0, int x1, int y1, int z1)
        {
            int n = 0;
            for (int x = x0; x <= x1; x++)
                for (int y = Mathf.Max(y0, world.minY); y <= y1 && y < world.maxY; y++)
                    for (int z = z0; z <= z1; z++)
                    {
                        ushort s = world.GetState(x, y, z);
                        if (s == 0) continue;
                        var b = Blocks.ByState[s];
                        if (b.isAir || b.isLiquid || b.hardness < 0 || Immune.Contains(b.id)) continue;
                        var p = new Int3(x, y, z);
                        // smashed blocks leave nothing behind, but a smashed chest still spills its contents
                        world.GetBlockEntity(p)?.DropContents();
                        world.SetState(p, 0);
                        n++;
                    }
            return n;
        }

        // ------------------------------------------------------------------ flight
        protected override void AiStep()
        {
            if (dead) return;
            if (phase == DragonPhase.Dying) { DyingStep(); return; }
            if (--targetScan <= 0) { targetScan = 20; Retarget(); }
            phaseTicks++;
            switch (phase)
            {
                case DragonPhase.Circling: CircleStep(); break;
                case DragonPhase.Strafing: StrafeStep(); break;
                case DragonPhase.Charging: ChargeStep(); break;
                case DragonPhase.Approach: ApproachStep(); break;
                case DragonPhase.Landing: LandingStep(); break;
                case DragonPhase.Perched: PerchStep(); break;
                case DragonPhase.Takeoff: TakeoffStep(); break;
            }
            KeepNearHome();
            fallDistance = 0;
        }

        Player Target => target as Player;
        bool Valid(Player p) => p != null && !p.removed && !p.dead && p.world == world && !p.IsCreative && !p.IsSpectator && (p.position - position).sqrMagnitude < 150f * 150f;

        /// <summary>The nearest survival player within 64 blocks; creative and spectating players are ignored.</summary>
        void Retarget()
        {
            if (Valid(Target)) return;
            target = world.NearestPlayer(position, 64f, false);
        }

        void Enter(DragonPhase p)
        {
            phase = p;
            phaseTicks = 0;
            if (p == DragonPhase.Perched) { sitDamage = 0; breathCooldown = 0; breathTicks = 0; }
        }

        void Resume() { Enter(DragonPhase.Circling); NextNode(); }

        /// <summary>Banked flight: turn toward the goal at a limited rate and fly along the heading.
        /// Near the goal it slows and turns harder, so it cannot orbit a waypoint forever.</summary>
        void FlyTo(Vector3 target, float maxSpeed, float turnRate, float climb = 0.45f)
        {
            Vector3 to = target - position;
            float horiz = Mathf.Sqrt(to.x * to.x + to.z * to.z);
            if (horiz > 0.5f) yaw = MathX.ApproachAngle(yaw, Mathf.Atan2(to.x, to.z) * Mathf.Rad2Deg, turnRate * (horiz < 16f ? 2f : 1f));
            var f = Fwd;
            float speed = Mathf.Min(maxSpeed, 0.15f + horiz * 0.02f);
            float along = horiz > 0.5f ? Mathf.Clamp01((f.x * to.x + f.z * to.z) / horiz * 0.5f + 0.5f) : 0f;
            Vector3 want = f * (speed * (0.3f + 0.7f * along));
            want.y = Mathf.Clamp(to.y * 0.05f, -climb, climb);
            velocity = Vector3.Lerp(velocity, want, 0.12f);
            Move(velocity);
            bodyYaw = yaw;
        }

        void FaceLook(Vector3 at)
        {
            float want = MathX.YawFromDir(at - position);
            lookYaw = bodyYaw + Mathf.Clamp(MathX.WrapAngle(want - bodyYaw), -60f, 60f);
        }

        bool Facing(Vector3 at, float degrees) => Mathf.Abs(MathX.WrapAngle(MathX.YawFromDir(at - position) - yaw)) < degrees;

        Vector3 NodePos(int i)
        {
            var n = Nodes[i];
            return InEnd ? n : new Vector3(home.x + n.x, home.y + n.y - DragonFight.PortalY, home.z + n.z);
        }

        void NextNode()
        {
            if (world.rand.Next(8) == 0) clockwise = !clockwise;
            int ring = node < 12 ? 0 : node < 20 ? 1 : 2;
            if (node < 0 || world.rand.Next(5) == 0) ring = world.rand.Next(3);
            int first = ring == 0 ? 0 : ring == 1 ? 12 : 20, count = ring == 0 ? 12 : ring == 1 ? 8 : 4;
            // carry on round the chosen ring from the node nearest to the dragon
            int best = first; float bd = float.MaxValue;
            for (int i = first; i < first + count; i++)
            {
                float d = (NodePos(i) - position).sqrMagnitude;
                if (d < bd) { bd = d; best = i; }
            }
            node = first + (best - first + (clockwise ? 1 : count - 1)) % count;
            goal = NodePos(node) + Vector3.up * world.rand.Range(-3f, 6f);
        }

        void CircleStep()
        {
            if (node < 0) NextNode();
            FlyTo(goal, 0.55f, 2.5f);
            lookYaw = yaw;
            if ((goal - position).sqrMagnitude < 100f) NodeReached();
        }

        void NodeReached()
        {
            int crystals = fight != null ? fight.crystalsAlive : 0;
            var p = Target;
            // the fewer crystals are left, the more often it comes down to fight
            if (InEnd && fight != null && world.rand.Next(crystals + 3) == 0) { Enter(DragonPhase.Approach); return; }
            if (Valid(p) && world.rand.Next(crystals + 2) == 0) { Enter(DragonPhase.Strafing); return; }
            if (Valid(p) && world.rand.Next(6) == 0) { StartCharge(p); return; }
            NextNode();
        }

        void StrafeStep()
        {
            var p = Target;
            if (!Valid(p) || phaseTicks > 240) { Resume(); return; }
            var head = HeadPos;
            Vector3 away = new Vector3(position.x - p.position.x, 0, position.z - p.position.z);
            if (away.sqrMagnitude < 1f) away = -Fwd;
            away.Normalize();
            float dist = (p.EyePosition - head).magnitude;
            // come round to face the player from firing range, a little above them
            FlyTo(p.position + away * Mathf.Clamp(dist * 0.6f, 20f, 48f) + Vector3.up * 12f, 0.6f, 3f);
            FaceLook(p.EyePosition);
            if (dist < 64f && Facing(p.position, 15f) && world.HasLineOfSight(head, p.EyePosition))
            {
                Fireball(p);
                Resume();
            }
        }

        void Fireball(Player p)
        {
            var head = HeadPos;
            var fb = new DragonFireball { world = world, owner = this };
            fb.SetPosition(head + Fwd * 1.5f);
            fb.Aim(p.EyePosition + p.velocity * 12f - fb.position);
            world.AddEntity(fb);
            Sounds.Play("entity.ender_dragon.shoot", head, 6f, 1f);
        }

        void StartCharge(Player p)
        {
            target = p;
            chargeAt = p.position + Vector3.up * 1.5f;
            Enter(DragonPhase.Charging);
        }

        void ChargeStep()
        {
            FlyTo(chargeAt, 0.9f, 5f, 0.8f);
            FaceLook(chargeAt);
            Vector3 d = chargeAt - position;
            // the dive ends on the spot the player stood on, or once it has overshot
            if (d.sqrMagnitude < 36f || phaseTicks > 140 || (phaseTicks > 30 && d.x * Fwd.x + d.z * Fwd.z < 0f)) Resume();
        }

        void ApproachStep()
        {
            var over = Perch + Vector3.up * 20f;
            FlyTo(over, 0.5f, 3f);
            lookYaw = yaw;
            Vector3 d = over - position; d.y = 0;
            if (d.sqrMagnitude < 36f || phaseTicks > 400) Enter(DragonPhase.Landing);
        }

        void LandingStep()
        {
            Vector3 d = Perch - position;
            velocity = Vector3.Lerp(velocity, Vector3.ClampMagnitude(d * 0.08f, 0.35f), 0.2f);
            Move(velocity);
            var p = Target;
            if (Valid(p)) yaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(p.position - position), 3f);
            bodyYaw = yaw;
            lookYaw = yaw;
            if (d.sqrMagnitude < 0.25f || phaseTicks > 200)
            {
                SetPosition(Perch);
                velocity = Vector3.zero;
                Enter(DragonPhase.Perched);
            }
        }

        void PerchStep()
        {
            velocity = Vector3.zero;
            var p = Target;
            bool ok = Valid(p) && (p.position - position).sqrMagnitude < 40f * 40f;
            if (ok)
            {
                yaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(p.position - position), 2.5f);
                bodyYaw = yaw;
                FaceLook(p.EyePosition);
            }
            else lookYaw = yaw;
            if (phaseTicks == 30) Sounds.Play("entity.ender_dragon.growl", HeadPos, 8f, 0.8f);
            if (breathTicks > 0) BreathStream();
            else if (ok && phaseTicks >= 50 && --breathCooldown <= 0) { breathCooldown = 100; Breathe(p); }
            // enough punishment while sitting sends it back up
            if (phaseTicks > 300 || sitDamage > maxHealth * 0.25f || (!ok && phaseTicks > 120)) Enter(DragonPhase.Takeoff);
        }

        /// <summary>A pool of breath on the ground between the head and the player.</summary>
        void Breathe(Player p)
        {
            breathTicks = 40;
            var head = HeadPos;
            Vector3 d = p.position - head; d.y = 0;
            Vector3 flat = d.sqrMagnitude > 1e-4f ? d.normalized : Fwd;
            var spot = head + flat * Mathf.Min(d.magnitude, 12f);
            int gx = Mathf.FloorToInt(spot.x), gz = Mathf.FloorToInt(spot.z);
            float gy = world.IsLoaded(gx, gz) ? world.TopSurfaceY(gx, gz) + 1f : p.position.y;
            breathAt = new Vector3(spot.x, Mathf.Min(gy, head.y), spot.z);
            var cloud = new AreaEffectCloud { world = world, radius = 4f, duration = 200, dragonBreath = true, owner = this, color = new Color32(200, 60, 230, 255) };
            cloud.SetPosition(breathAt);
            world.AddEntity(cloud);
            Sounds.Play("entity.ender_dragon.shoot", head, 8f, 0.7f);
        }

        void BreathStream()
        {
            breathTicks--;
            var head = HeadPos;
            for (int i = 0; i < 3; i++) Particles.DragonBreath(world, Vector3.Lerp(head, breathAt, world.rand.NextFloat()));
        }

        void TakeoffStep()
        {
            velocity = Vector3.Lerp(velocity, Fwd * 0.3f + Vector3.up * 0.45f, 0.1f);
            Move(velocity);
            lookYaw = yaw;
            if (phaseTicks > 60 || position.y > Perch.y + 24f) Resume();
        }

        /// <summary>The dragon belongs to its island: never down into the void, never off out of reach.</summary>
        void KeepNearHome()
        {
            if (position.y < world.minY + 10f) { position.y = world.minY + 10f; if (velocity.y < 0) velocity.y = 0; }
            if (position.y > world.maxY - 4f) { position.y = world.maxY - 4f; if (velocity.y > 0) velocity.y = 0; }
            Vector3 d = position - home; d.y = 0;
            if (d.sqrMagnitude > 150f * 150f && phase != DragonPhase.Circling) Resume();
        }

        Vector3 Jitter(float r) => new Vector3(world.rand.Range(-r, r), world.rand.Range(-r, r), world.rand.Range(-r, r));

        // ------------------------------------------------------------------ damage rules
        public override bool Hurt(DamageSource src, float amount)
        {
            if (src == null || dead || removed || phase == DragonPhase.Dying) return false;
            bool forced = src == DamageSource.Kill || src == DamageSource.Void;
            if (!forced)
            {
                // the hide shrugs off everything but a player's blow or a blast
                if (!(src.attacker is Player) && !src.isExplosion) return false;
                // perched, its scales turn arrows aside (and light them)
                if (phase == DragonPhase.Perched && src.isProjectile) { if (src.direct != null) src.direct.SetOnFire(5); return false; }
                // only the head is soft: anything else lands at a quarter strength
                if (!backlash && !HeadHit(src)) amount = amount * 0.25f + Mathf.Min(amount, 1f);
            }
            float before = health;
            bool r = base.Hurt(src, amount);
            if (r && phase == DragonPhase.Perched) sitDamage += before - health;
            return r;
        }

        bool HeadHit(DamageSource src)
        {
            Vector3 hit;
            if (src.direct != null && src.direct != src.attacker) hit = src.direct.position;
            else if (src.sourcePos.HasValue) hit = src.sourcePos.Value;
            else if (src.attacker != null) hit = src.attacker.EyePosition + src.attacker.LookDir * 2.5f;
            else return false;
            if ((hit - HeadPos).sqrMagnitude < 36f) return true;
            // the fore part of the body leads straight into the neck
            Vector3 rel = hit - position;
            var f = Fwd;
            return rel.x * f.x + rel.z * f.z > width * 0.2f;
        }

        /// <summary>Health reaching zero starts the death sequence rather than ending the dragon at once.</summary>
        public override void Die(DamageSource src)
        {
            if (dead || phase == DragonPhase.Dying) return;
            killSource = src;
            // alive enough to keep animating through the sequence
            health = 1f;
            Unlink();
            xpTotal = fight != null && fight.hasBeenDefeatedOnce ? RepeatXp : def.xp;
            xpDropped = 0;
            deathTicks = 0;
            dyingApproach = 0;
            target = null;
            Enter(DragonPhase.Dying);
        }

        void DyingStep()
        {
            if (deathTicks == 0)
            {
                // first make for the fountain, so the xp and the egg land where the player can reach them
                var spot = InEnd ? Perch + Vector3.up * 14f : position;
                FlyTo(spot, 0.5f, 5f, 0.4f);
                Vector3 d = spot - position; d.y = 0;
                if (!InEnd || (d.sqrMagnitude < 36f && Mathf.Abs(spot.y - position.y) < 4f) || ++dyingApproach > 200)
                {
                    deathTicks = 1;
                    Sounds.Play("entity.ender_dragon.death", position, 16f, 1f);
                }
                return;
            }
            deathTicks++;
            velocity = Vector3.zero;
            Move(new Vector3(0f, DeathHeight / DeathTicks, 0f));
            yaw += 2f;
            bodyYaw = lookYaw = yaw;
            var c = BodyCenter;
            if (deathTicks % 3 == 0) Particles.Explosion(world, c + Jitter(6f), deathTicks > 150 ? 2.5f : 1f);
            // light breaks out of the body in rays
            for (int ray = 0; ray < 2; ray++)
            {
                var dir = Jitter(1f).normalized;
                for (int k = 1; k <= 6; k++) Particles.EndRod(world, c + dir * (k * 1.4f));
            }
            if (deathTicks > 150 && deathTicks % 5 == 0) DropXp(xpTotal * 8 / 100);
            if (deathTicks >= DeathTicks) FinishDeath();
        }

        void DropXp(int n)
        {
            n = Mathf.Min(n, xpTotal - xpDropped);
            if (n <= 0) return;
            XpOrb.Spawn(world, BodyCenter, n);
            xpDropped += n;
        }

        void FinishDeath()
        {
            var c = BodyCenter;
            for (int i = 0; i < 20; i++) Particles.Explosion(world, c + Jitter(8f), 4f);
            Particles.Explosion(world, c, 8f);
            Sounds.Play("entity.generic.explode", c, 16f, 0.6f);
            GameManager.Instance?.OnExplosion(c, 8f);
            DropXp(xpTotal - xpDropped);
            Unlink();
            // the real death: kill credit and drops through Mob.OnDeath, then the fight opens the exit
            base.Die(killSource ?? DamageSource.Generic);
            fight?.OnDragonKilled(this);
            Remove();
        }

        public override void Remove()
        {
            Unlink();
            base.Remove();
        }

        // ------------------------------------------------------------------ persistence
        public override void Save(Dictionary<string, string> d)
        {
            base.Save(d);
            var ci = CultureInfo.InvariantCulture;
            d["phase"] = ((int)phase).ToString(ci);
            d["home"] = home.x.ToString("R", ci) + "," + home.y.ToString("R", ci) + "," + home.z.ToString("R", ci);
            if (phase == DragonPhase.Dying) d["death"] = deathTicks.ToString(ci) + "," + xpTotal.ToString(ci) + "," + xpDropped.ToString(ci);
        }

        public override void Load(Dictionary<string, string> d)
        {
            base.Load(d);
            var ci = CultureInfo.InvariantCulture;
            persistent = true;
            home = InEnd ? new Vector3(0.5f, 0f, 0.5f) : position;
            if (d.TryGetValue("home", out var h)) { try { home = SaveManager.ParseV(h); } catch (FormatException) { } catch (IndexOutOfRangeException) { } }
            if (d.TryGetValue("phase", out var ph) && int.TryParse(ph, NumberStyles.Integer, ci, out int pi) && pi >= 0 && pi <= (int)DragonPhase.Dying) phase = (DragonPhase)pi;
            if (phase == DragonPhase.Dying && d.TryGetValue("death", out var dt))
            {
                var p = dt.Split(',');
                if (p.Length == 3) { int.TryParse(p[0], NumberStyles.Integer, ci, out deathTicks); int.TryParse(p[1], NumberStyles.Integer, ci, out xpTotal); int.TryParse(p[2], NumberStyles.Integer, ci, out xpDropped); }
                health = Mathf.Max(health, 1f);
            }
            node = -1;
        }
    }

    // =====================================================================================================
    /// <summary>
    /// Builds the wither: a T of four soul sand or soul soil blocks (the two blocks beside the stem left
    /// empty) topped with three wither skeleton skulls. Placing the last skull consumes the build.
    /// </summary>
    public static class WitherSummon
    {
        /// <summary>pos: the wither skeleton skull that was just placed.</summary>
        public static bool TrySummon(World w, Int3 pos)
        {
            if (w == null || !IsSkull(w, pos)) return false;
            if (w.session != null && w.session.difficulty == Difficulty.Peaceful) return false;
            for (int axis = 0; axis < 2; axis++)
            {
                var along = DirUtil.Offset[(int)(axis == 0 ? Dir.East : Dir.North)];
                // the new skull may be any of the three: try it as the left, middle and right one
                for (int k = -1; k <= 1; k++)
                {
                    Int3 top = pos + along * (-k);
                    if (!Matches(w, top, along)) continue;
                    Build(w, top, along, axis);
                    return true;
                }
            }
            return false;
        }

        static bool Matches(World w, Int3 top, Int3 along)
        {
            Int3 mid = top.Offset(Dir.Down), stem = mid.Offset(Dir.Down);
            if (!IsSkull(w, top - along) || !IsSkull(w, top) || !IsSkull(w, top + along)) return false;
            if (!IsSoul(w, mid - along) || !IsSoul(w, mid) || !IsSoul(w, mid + along) || !IsSoul(w, stem)) return false;
            return IsClear(w, stem - along) && IsClear(w, stem + along);
        }

        static bool IsSkull(World w, Int3 p) => w.GetBlock(p) is SkullBlock s && s.kind == "wither_skeleton_skull";

        static bool IsSoul(World w, Int3 p)
        {
            var id = w.GetBlock(p).id;
            return id == "soul_sand" || id == "soul_soil";
        }

        static bool IsClear(World w, Int3 p)
        {
            var b = w.GetBlock(p);
            return b.isAir || (b.replaceable && !b.isLiquid);
        }

        static void Build(World w, Int3 top, Int3 along, int axis)
        {
            Int3 mid = top.Offset(Dir.Down), stem = mid.Offset(Dir.Down);
            Clear(w, top - along); Clear(w, top); Clear(w, top + along);
            Clear(w, mid - along); Clear(w, mid); Clear(w, mid + along); Clear(w, stem);
            var at = new Vector3(stem.x + 0.5f, stem.y + 0.55f, stem.z + 0.5f);
            var m = MobRegistry.Spawn(w, "wither", at, SpawnReason.Summon) as WitherBoss;
            if (m == null) return;
            // it faces out of the plane the T was built in
            m.yaw = m.prevYaw = m.bodyYaw = m.headYaw = m.lookYaw = axis == 0 ? 0f : 90f;
            Sounds.Play("entity.wither.spawn", at, 8f, 1f);
            for (int i = 0; i < 24; i++)
            {
                var off = new Vector3(w.rand.Range(-1f, 1f), w.rand.Range(0f, 3f), w.rand.Range(-1f, 1f));
                Particles.Soul(w, at + off);
                if (i % 3 == 0) Particles.LargeSmoke(w, at + off);
            }
            foreach (var e in w.entities)
                if (e is Player p && !p.removed && (p.position - at).sqrMagnitude < 50f * 50f) Achievements.Grant(p, "summon_wither");
        }

        static void Clear(World w, Int3 p)
        {
            ushort s = w.GetState(p);
            if (s == 0) return;
            Particles.BlockBreak(w, p, s);
            w.SetState(p, 0);
        }
    }

    // =====================================================================================================
    /// <summary>
    /// The Wither: a flying three headed boss. A built wither first charges (health bar filling, frozen,
    /// untouchable) and frees itself with a power 7 blast. Then it hovers over its target and fires skulls
    /// from all three heads; at half health it gains armour that turns arrows, drops low and fires
    /// three way volleys. No other mob may target it, and withering does not touch it.
    /// </summary>
    public sealed class WitherBoss : Mob
    {
        /// <summary>Charge-up length: 1 second as specified (vanilla gives the builder 220 ticks to run).</summary>
        public const int ChargeTicks = 20;
        public const float ExplosionPower = 7f;
        const int ArmoredBonus = 4;
        const float SightRange = 48f;

        public int charge;
        public bool armored;
        Vector3 home;
        int mainCooldown = 40, retarget, breakTimer, stuck;
        readonly int[] sideCooldown = { 30, 50 };

        public override void Setup(MobDef d)
        {
            base.Setup(d);
            canBreatheUnderwater = true;
            persistent = true;
        }

        public override void OnInitialSpawn(SpawnReason reason)
        {
            persistent = true;
            home = position;
            // only a built wither charges up; one summoned any other way starts awake
            if (reason == SpawnReason.Summon)
            {
                charge = ChargeTicks;
                health = maxHealth / 3f;
                glowing = true;
            }
        }

        public override bool CanDespawn => false;
        public override string AmbientSound => "entity.wither.ambient";
        public override string DeathSound => null;
        public override int ArmorValue => armored ? base.ArmorValue + ArmoredBonus : base.ArmorValue;
        public override void ApplyFallDamage(float dist) { }
        public override void OnStruckByLightning(LightningBolt bolt) { }
        protected override void OnHurtEffects(DamageSource src) { Sounds.Play(HurtSound, position, 3f, VoicePitch); }

        /// <summary>Withering is its own weapon and never takes hold on it (callers guard this too).</summary>
        public override void AddEffect(EffectInstance inst)
        {
            if (inst != null && inst.effect == Effect.Wither) return;
            base.AddEffect(inst);
        }

        // ------------------------------------------------------------------ tick
        public override void Tick()
        {
            base.Tick();
            if (removed || dead) return;
            // like other monsters it will not stay in a peaceful world
            if (world.session != null && world.session.difficulty == Difficulty.Peaceful) { Remove(); return; }
            ShowBar();
            ShakeOffAttackers();
            if (charge > 0) { Charge(); return; }
            if (!armored && health <= maxHealth * 0.5f) Armor();
            if (age % 20 == 0) Heal(1f);
            if (breakTimer > 0 && --breakTimer == 0) BreakOut();
        }

        void Charge()
        {
            charge--;
            // the health bar fills while it gathers itself
            float f = 1f - charge / (float)ChargeTicks;
            health = Mathf.Max(health, maxHealth * (1f + 2f * f) / 3f);
            if (age % 2 == 0) Particles.Soul(world, position + new Vector3(world.rand.Range(-0.8f, 0.8f), world.rand.Range(0f, height), world.rand.Range(-0.8f, 0.8f)));
            if (charge > 0) return;
            health = maxHealth;
            glowing = false;
            // the blast that frees it breaks blocks unless the mobGriefing rule is off
            Explosion.Explode(world, this, position + Vector3.up * (height * 0.5f), ExplosionPower, false, true);
            Sounds.Play("entity.wither.spawn", position, 16f, 1f);
        }

        void Armor()
        {
            armored = true;
            Sounds.Play("entity.wither.hurt", position, 6f, 0.6f);
            for (int i = 0; i < 30; i++)
                Particles.Smoke(world, position + new Vector3(world.rand.Range(-1.2f, 1.2f), world.rand.Range(0f, height), world.rand.Range(-1.2f, 1.2f)), 1, 0.4f, true);
        }

        /// <summary>No other mob may hold the wither as its target, whatever its own goals picked.</summary>
        void ShakeOffAttackers()
        {
            foreach (var e in world.entities)
                if (e is Mob m && m != this && m.target == this) { m.target = null; m.angerTicks = 0; m.angryAtId = -1; m.nav.Stop(); }
        }

        void ShowBar()
        {
            var gm = GameManager.Instance;
            var p = gm != null ? gm.player : null;
            if (p == null || gm.hud == null || p.world != world || (p.position - position).sqrMagnitude > 64f * 64f) return;
            gm.hud.ShowBossBar(DisplayName, HealthFrac, true);
        }

        // ------------------------------------------------------------------ flight
        protected override void AiStep()
        {
            if (dead) { velocity.y -= 0.02f; Move(velocity); return; }
            if (charge > 0) { velocity = Vector3.zero; return; } // frozen while charging
            if (--retarget <= 0) { retarget = 10; Retarget(); }
            Hover();
            Heads();
            fallDistance = 0;
        }

        bool Valid(LivingEntity t) => t != null && !t.removed && !t.dead && t.world == world && !t.undead && !(t is Player p && (p.IsCreative || p.IsSpectator)) && (t.position - position).sqrMagnitude < SightRange * SightRange;

        /// <summary>Players first; failing that, anything living nearby that is not undead.</summary>
        void Retarget()
        {
            if (Valid(target) && (target is Player || world.rand.Next(4) != 0)) return;
            LivingEntity best = world.NearestPlayer(position, def.followRange, false);
            if (best != null && (best.position - position).sqrMagnitude > 16f * 16f && !world.HasLineOfSight(EyePosition, best.EyePosition)) best = null;
            target = best ?? NearestPrey(20f);
        }

        LivingEntity NearestPrey(float r)
        {
            LivingEntity best = null; float bd = r * r;
            foreach (var e in world.entities)
            {
                if (!(e is LivingEntity le) || le is Player || le == this || le.dead || le.removed || le.undead || le is EnderDragonMob || !le.Attackable) continue;
                float d = (le.position - position).sqrMagnitude;
                if (d < bd) { bd = d; best = le; }
            }
            return best;
        }

        void Hover()
        {
            Vector3 goal;
            var t = target;
            if (t != null)
            {
                Vector3 flat = t.position - position; flat.y = 0;
                goal = position;
                // it keeps above its target, and closes in from range; armoured, it comes right down
                goal.y = t.position.y + (armored ? 1.5f : 5f);
                if (flat.magnitude > (armored ? 3f : 9f)) { goal.x = t.position.x; goal.z = t.position.z; }
                if (flat.sqrMagnitude > 1e-4f) yaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(flat), 10f);
                LookAt(t);
            }
            else
            {
                // idle, it drifts about the spot it was built on
                goal = home + new Vector3(Mathf.Sin(age * 0.013f) * 10f, 6f, Mathf.Cos(age * 0.017f) * 10f);
                if (velocity.sqrMagnitude > 1e-4f) yaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(velocity), 4f);
            }
            Vector3 d = goal - position;
            var want = new Vector3(Mathf.Clamp(d.x * 0.1f, -0.35f, 0.35f), Mathf.Clamp(d.y * 0.1f, -0.3f, 0.3f), Mathf.Clamp(d.z * 0.1f, -0.35f, 0.35f));
            velocity = Vector3.Lerp(velocity, want, 0.12f);
            Move(velocity);
            bodyYaw = yaw;
            LookControl();
            // boxed in for a second: tear out
            if (horizontalCollision) { if (++stuck > 20 && breakTimer <= 0) { stuck = 0; breakTimer = 1; } }
            else stuck = 0;
        }

        Vector3 HeadPos(int head)
        {
            var f = MathX.YawPitchToDir(yaw, 0f);
            if (head == 0) return position + Vector3.up * (height * 0.9f) + f * 0.3f;
            var right = new Vector3(f.z, 0f, -f.x);
            return position + Vector3.up * (height * 0.75f) + right * (head == 1 ? 1.3f : -1.3f);
        }

        void Heads()
        {
            var t = target;
            if (t == null || (t.position - position).sqrMagnitude > SightRange * SightRange) return;
            // the middle head keeps up a steady fire at the main target
            if (--mainCooldown <= 0 && world.HasLineOfSight(HeadPos(0), t.EyePosition))
            {
                mainCooldown = armored ? 30 : 40;
                Fire(0, t, 0f);
                if (armored) { Fire(0, t, 15f); Fire(0, t, -15f); }
            }
            // the side heads snap at the target or at whatever else is nearby
            for (int h = 0; h < 2; h++)
            {
                if (--sideCooldown[h] > 0) continue;
                sideCooldown[h] = (armored ? 25 : 50) + world.rand.Next(20);
                var st = world.rand.Next(3) == 0 ? NearestPrey(16f) ?? t : t;
                if (world.HasLineOfSight(HeadPos(h + 1), st.EyePosition)) Fire(h + 1, st, 0f);
            }
        }

        void Fire(int head, LivingEntity t, float yawOffset)
        {
            var from = HeadPos(head);
            Vector3 aim = t.position + Vector3.up * (t.height * 0.5f) + t.velocity * 5f - from;
            if (yawOffset != 0f) aim = Quaternion.Euler(0f, yawOffset, 0f) * aim;
            if (aim.sqrMagnitude < 1e-4f) aim = MathX.YawPitchToDir(yaw, 0f);
            var skull = new WitherSkull { world = world, owner = this, dangerous = world.rand.Next(20) == 0 };
            skull.SetPosition(from + aim.normalized * 0.8f);
            skull.Aim(aim);
            world.AddEntity(skull);
            Sounds.Play("entity.wither.shoot", from, 3f, 0.9f + world.rand.NextFloat() * 0.2f);
        }

        /// <summary>Hurt or boxed in, it tears out of whatever holds it (bedrock, portals and barriers hold).</summary>
        void BreakOut()
        {
            if (world.session != null && !world.session.mobGriefing) return;
            var c = Int3.Floor(position);
            bool any = false;
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = 0; dy <= 3; dy++)
                    for (int dz = -1; dz <= 1; dz++)
                    {
                        var p = c.Offset(dx, dy, dz);
                        var b = world.GetBlock(p);
                        if (b.isAir || b.isLiquid || b.hardness < 0) continue;
                        world.BreakBlock(p, true, this);
                        any = true;
                    }
            if (any) Sounds.Play("entity.wither.break_block", position, 2f, 1f);
        }

        // ------------------------------------------------------------------ damage rules
        public override bool Hurt(DamageSource src, float amount)
        {
            if (src == null) return false;
            bool forced = src == DamageSource.Kill || src == DamageSource.Void;
            if (!forced)
            {
                if (charge > 0) return false;
                if (src == DamageSource.Drown || src == DamageSource.WitherEffect) return false;
                // the armour it grows at half health turns arrows and tridents aside
                if (armored && (src.direct is Arrow || src.direct is ThrownTrident)) return false;
                // undead never hurt each other, which also keeps it safe from its own skulls
                if (src.attacker is Mob m && m.undead) return false;
            }
            bool r = base.Hurt(src, amount);
            if (r && breakTimer <= 0) breakTimer = 20;
            return r;
        }

        protected override void DropExtra(List<ItemStack> drops, bool byPlayer, int looting, ref RNG rng)
        {
            if (byPlayer) drops.Add(new ItemStack("nether_star", 1));
        }

        protected override void OnDeath(DamageSource src)
        {
            base.OnDeath(src);
            Sounds.Play("entity.wither.death", position, 8f, 1f);
            // the star never despawns, like the trophy it is
            foreach (var e in world.entitiesToAdd)
                if (e is ItemEntity ie && ie.stack != null && !ie.stack.IsEmpty && ie.stack.item.id == "nether_star" && (ie.position - position).sqrMagnitude < 16f) ie.life = short.MaxValue;
        }

        public override void Save(Dictionary<string, string> d)
        {
            base.Save(d);
            var ci = CultureInfo.InvariantCulture;
            d["charge"] = charge.ToString(ci);
            if (armored) d["armored"] = "1";
            d["home"] = home.x.ToString("R", ci) + "," + home.y.ToString("R", ci) + "," + home.z.ToString("R", ci);
        }

        public override void Load(Dictionary<string, string> d)
        {
            base.Load(d);
            persistent = true;
            home = position;
            if (d.TryGetValue("charge", out var c)) int.TryParse(c, NumberStyles.Integer, CultureInfo.InvariantCulture, out charge);
            armored = d.TryGetValue("armored", out var a) && a == "1";
            if (d.TryGetValue("home", out var h)) { try { home = SaveManager.ParseV(h); } catch (FormatException) { } catch (IndexOutOfRangeException) { } }
            glowing = charge > 0;
        }
    }
}
