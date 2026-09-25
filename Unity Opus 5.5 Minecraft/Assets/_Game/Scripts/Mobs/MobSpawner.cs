using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Natural mob spawning. Three entry points: populating a freshly generated chunk, the recurring
    /// spawn cycle around each player (per category caps, the 24 block rule, light and floor checks)
    /// and the rare set pieces (pillager patrols, the wandering trader, phantoms for the sleepless).
    /// Every roll comes from an RNG seeded by the world seed plus a position or tick, so a world
    /// replays the same way. The per tick paths allocate nothing unless a mob actually spawns.
    /// </summary>
    public static partial class MobSpawner
    {
        // ---------------------------------------------------------------- tuning
        /// <summary>Per player caps, counted within <see cref="SpawnRadius"/> of that player.</summary>
        public const int MonsterCap = 70, CreatureCap = 10, AmbientCap = 15, WaterCreatureCap = 5, WaterAmbientCap = 20;
        /// <summary>Natural spawns never happen closer than this to a player.</summary>
        public const int MinPlayerDistance = 24;
        /// <summary>Mob.TickDespawn deletes anything further away at once, so nothing spawns out there.</summary>
        public const int SpawnRadius = 128;
        /// <summary>Days without a bed before phantoms start hunting a player.</summary>
        public const int PhantomDays = 3;

        const int CycleTicks = 20, CreatureCycleTicks = 400;
        const int MonsterTries = 40, CreatureTries = 12, WaterTries = 10, AmbientTries = 4;
        const long TraderInterval = 24000, TraderLifetime = 48000;
        const float CreatureChance = 0.1f, WaterChance = 0.15f, CaveChance = 0.08f, NightChance = 0.05f;
        const int SaltChunk = 0x5A17, SaltCycle = 0x5A18, SaltSpecial = 0x5A19, SaltSlime = 0x51A3;

        enum Pool : byte { Monsters, Land, Water, Ambient }

        /// <summary>Nether fortresses have their own table; its floors are the only nether brick in the dimension.</summary>
        static readonly List<SpawnEntry> Fortress = new List<SpawnEntry>
        {
            new SpawnEntry("blaze", 10, 2, 3), new SpawnEntry("zombified_piglin", 5, 4, 4), new SpawnEntry("wither_skeleton", 8, 5, 5),
            new SpawnEntry("skeleton", 2, 5, 5), new SpawnEntry("magma_cube", 3, 4, 4)
        };

        static readonly int[] counts = new int[7];

        // ---------------------------------------------------------------- session state
        sealed class State
        {
            public GameSession session;
            public readonly long[] nextCycle = new long[3], nextCreatures = new long[3];
            public long nextTrader, nextPatrol, nextPhantom;
            public int traderChance = 25;
            public VillagerMob trader;
            public readonly List<Mob> llamas = new List<Mob>();
            public long traderLeaves;
            public readonly Dictionary<string, int> restDay = new Dictionary<string, int>();
            public readonly HashSet<string> sleepers = new HashSet<string>();
        }
        static State state;
        static Dictionary<string, string> loaded;

        static State StateFor(GameSession s)
        {
            if (state != null && state.session == s) return state;
            state = new State { session = s, nextTrader = s.gameTime + TraderInterval, nextPatrol = s.gameTime + 12000, nextPhantom = s.gameTime + 1200 };
            if (loaded != null) { Apply(state, loaded); loaded = null; }
            return state;
        }

        // ---------------------------------------------------------------- chunk population
        /// <summary>Called once for each freshly generated overworld chunk (never for chunks loaded from a save).</summary>
        public static void OnChunkGenerated(World w, Chunk c)
        {
            if (w == null || c == null || w.dim != DimensionId.Overworld) return;
            var rng = new RNG(w.seed, c.cx, c.cz, SaltChunk);
            var biome = w.GetSurfaceBiome((c.cx << 4) + 8, (c.cz << 4) + 8);
            // animals: most chunks get none, about one in ten a pack, rarely two
            for (int k = 0; k < 3 && rng.NextFloat() < CreatureChance; k++) Populate(w, c, Pool.Land, false, ref rng);
            if ((biome.isOcean || biome.isRiver) && rng.NextFloat() < WaterChance) Populate(w, c, Pool.Water, false, ref rng);
            if (w.session != null && w.session.difficulty == Difficulty.Peaceful) return;
            // caves are dark from the start; the surface only gets monsters when the chunk appears at night
            if (rng.NextFloat() < CaveChance) Populate(w, c, Pool.Monsters, true, ref rng);
            if (w.session != null && w.session.IsNight && rng.NextFloat() < NightChance) Populate(w, c, Pool.Monsters, false, ref rng);
        }

        static void Populate(World w, Chunk c, Pool pool, bool underground, ref RNG rng)
        {
            int x = (c.cx << 4) + rng.Next(16), z = (c.cz << 4) + rng.Next(16);
            int top = w.TopSurfaceY(x, z);
            int y;
            if (pool == Pool.Water) y = rng.Range(w.seaLevel - 12, w.seaLevel - 1);
            else if (underground)
            {
                int hi = Mathf.Min(top - 8, w.seaLevel);
                if (hi <= w.minY + 6) return;
                y = rng.Range(w.minY + 5, hi);
            }
            else y = top + 1;
            SpawnPack(w, pool, new Int3(x, y, z), SpawnReason.ChunkGen, c, underground ? 12 : 3, ref rng);
        }

        // ---------------------------------------------------------------- spawn cycle
        /// <summary>The natural spawn cycle. Safe to call every tick (it runs once a second) or once a second.</summary>
        public static void Tick(World w)
        {
            var s = w != null ? w.session : null;
            if (s == null || !s.doMobSpawning) return;
            var st = StateFor(s);
            int d = (int)w.dim;
            if (w.tickCount < st.nextCycle[d]) return;
            st.nextCycle[d] = w.tickCount + CycleTicks;
            // animals are persistent, so they are only topped up every 20 seconds
            bool creatures = w.tickCount >= st.nextCreatures[d];
            if (creatures) st.nextCreatures[d] = w.tickCount + CreatureCycleTicks;
            bool monsters = s.difficulty != Difficulty.Peaceful;
            var rng = new RNG(w.seed, (int)w.tickCount, d, SaltCycle);
            foreach (var e in w.entities)
            {
                if (!(e is Player p) || p.removed || p.dead || p.IsSpectator) continue;
                CountAround(w, p.position);
                if (monsters) Cycle(w, p, Pool.Monsters, MonsterTries, ref rng);
                if (creatures) Cycle(w, p, Pool.Land, CreatureTries, ref rng);
                Cycle(w, p, Pool.Water, WaterTries, ref rng);
                Cycle(w, p, Pool.Ambient, AmbientTries, ref rng);
            }
        }

        static void Cycle(World w, Player p, Pool pool, int tries, ref RNG rng)
        {
            if (PoolFull(pool)) return;
            int px = Mathf.FloorToInt(p.position.x), pz = Mathf.FloorToInt(p.position.z);
            for (int i = 0; i < tries; i++)
            {
                int x = px + rng.Range(-SpawnRadius, SpawnRadius), z = pz + rng.Range(-SpawnRadius, SpawnRadius);
                var c = w.ReadyChunk(x, z);
                if (c == null || !c.lit) continue;
                int top = w.TopSurfaceY(x, z);
                if (top <= w.minY) continue;
                int y;
                switch (pool)
                {
                    // overworld animals live on top of their column, nether striders at any height
                    case Pool.Land: y = w.dim == DimensionId.Overworld ? top + 1 : rng.Range(w.minY + 1, top + 1); break;
                    case Pool.Water: y = w.dim == DimensionId.Overworld && rng.NextBool() ? rng.Range(w.seaLevel - 14, w.seaLevel - 1) : rng.Range(w.minY + 1, top); break;
                    default: y = rng.Range(w.minY + 1, top + 1); break;
                }
                SpawnPack(w, pool, new Int3(x, y, z), SpawnReason.Natural, null, 3, ref rng);
                if (PoolFull(pool)) return;
            }
        }

        static void CountAround(World w, Vector3 pos)
        {
            System.Array.Clear(counts, 0, counts.Length);
            Tally(w.entities, pos);
            Tally(w.entitiesToAdd, pos);
        }

        static void Tally(List<Entity> list, Vector3 pos)
        {
            const float r2 = SpawnRadius * (float)SpawnRadius;
            foreach (var e in list)
            {
                // named, tamed and otherwise persistent mobs do not count against the cap
                if (!(e is Mob m) || m.removed || m.dead || m.persistent || m.tamed || m.customName != null) continue;
                if ((m.position - pos).sqrMagnitude <= r2) counts[(int)m.def.category]++;
            }
        }

        static int CapFor(MobCategory c)
        {
            switch (c)
            {
                case MobCategory.Monster: return MonsterCap;
                case MobCategory.Creature: return CreatureCap;
                case MobCategory.Ambient: return AmbientCap;
                case MobCategory.WaterCreature: return WaterCreatureCap;
                case MobCategory.WaterAmbient: return WaterAmbientCap;
                default: return 0;
            }
        }

        static bool PoolFull(Pool pool)
        {
            switch (pool)
            {
                case Pool.Monsters: return counts[(int)MobCategory.Monster] >= MonsterCap;
                case Pool.Land: return counts[(int)MobCategory.Creature] >= CreatureCap;
                case Pool.Ambient: return counts[(int)MobCategory.Ambient] >= AmbientCap;
                default: return counts[(int)MobCategory.WaterCreature] >= WaterCreatureCap && counts[(int)MobCategory.WaterAmbient] >= WaterAmbientCap;
            }
        }

        // ---------------------------------------------------------------- packs
        /// <summary>One species picked at the origin, then a short random walk placing each member.
        /// bound != null means chunk generation: members stay inside that chunk and caps are ignored.</summary>
        static int SpawnPack(World w, Pool pool, Int3 origin, SpawnReason reason, Chunk bound, int scan, ref RNG rng)
        {
            if (!Settle(w, pool, ref origin, scan)) return 0;
            var biome = w.GetBiome(origin);
            var floor = w.GetBlock(origin.Offset(Dir.Down));
            if (!Pick(w, biome, pool, floor, ref rng, out SpawnEntry entry)) return 0;
            var def = MobRegistry.Get(entry.mob);
            if (def == null || CapFor(def.category) == 0) return 0;
            int size = rng.Range(Mathf.Max(1, entry.min), Mathf.Max(1, Mathf.Max(entry.min, entry.max)));
            int placed = 0, x = origin.x, z = origin.z;
            for (int k = 0; k < size * 3 && placed < size; k++)
            {
                if (k > 0) { x += rng.Next(6) - rng.Next(6); z += rng.Next(6) - rng.Next(6); }
                if (bound != null)
                {
                    x = Mathf.Clamp(x, bound.cx << 4, (bound.cx << 4) + 15);
                    z = Mathf.Clamp(z, bound.cz << 4, (bound.cz << 4) + 15);
                }
                var at = new Int3(x, origin.y, z);
                if (k > 0 && !Settle(w, pool, ref at, 2)) continue;
                if (!CanSpawnAt(w, def, at, ref rng)) continue;
                var pos = new Vector3(at.x + 0.5f, at.y, at.z + 0.5f);
                bool monster = def.category == MobCategory.Monster;
                if ((bound == null || monster) && !DistanceOk(w, pos, bound == null)) continue;
                if (bound == null && counts[(int)def.category] >= CapFor(def.category)) break;
                var m = MobRegistry.Spawn(w, def.id, pos, reason);
                if (m == null) continue;
                m.yaw = m.prevYaw = m.bodyYaw = m.headYaw = m.lookYaw = rng.Range(0f, 360f);
                counts[(int)def.category]++;
                placed++;
            }
            return placed;
        }

        /// <summary>Move a candidate onto something to stand on (or into water for swimmers).</summary>
        static bool Settle(World w, Pool pool, ref Int3 at, int scan)
        {
            if (!w.IsLoaded(at)) return false;
            if (pool == Pool.Land && w.dim == DimensionId.Overworld)
            {
                // start at the top of the column so animals end up under a canopy, not on it
                int top = w.TopSurfaceY(at.x, at.z);
                for (int y = top + 1; y > top - 24 && y > w.minY; y--)
                {
                    var p = new Int3(at.x, y, at.z);
                    if (!Empty(w, p)) continue;
                    var below = w.GetBlock(p.Offset(Dir.Down));
                    if (below.isLiquid) return false;
                    if (!below.solid || below is LeavesBlock) continue;
                    at = p;
                    return true;
                }
                return false;
            }
            if (pool == Pool.Water)
            {
                for (int i = 0; i <= scan; i++, at = at.Offset(Dir.Down)) if (w.IsWater(at)) return true;
                return false;
            }
            for (int i = 0; i <= scan && at.y > w.minY; i++, at = at.Offset(Dir.Down))
            {
                // striders start in lava and drowned in water; everything else needs open air
                bool fluid = (pool == Pool.Land && w.IsLava(at)) || (pool == Pool.Monsters && w.IsWater(at));
                if (!fluid && !Empty(w, at)) continue;
                var below = w.GetBlock(at.Offset(Dir.Down));
                if (below.solid || below.isLiquid) return true;
            }
            return false;
        }

        // ---------------------------------------------------------------- tables
        static bool Pick(World w, Biome b, Pool pool, Block floor, ref RNG rng, out SpawnEntry pick)
        {
            if (w.dim == DimensionId.Nether && pool == Pool.Monsters && floor.id == "nether_bricks") return PickFrom(Fortress, null, pool, ref rng, out pick);
            switch (pool)
            {
                case Pool.Monsters: return PickFrom(b.monsters, null, pool, ref rng, out pick);
                case Pool.Land: return PickFrom(b.creatures, null, pool, ref rng, out pick);
                case Pool.Water: return PickFrom(b.water_, b.creatures, pool, ref rng, out pick);
                default: return PickFrom(b.ambient, null, pool, ref rng, out pick);
            }
        }

        static bool PickFrom(List<SpawnEntry> a, List<SpawnEntry> extra, Pool pool, ref RNG rng, out SpawnEntry pick)
        {
            pick = default;
            int total = Weight(a, pool) + Weight(extra, pool);
            if (total <= 0) return false;
            int roll = rng.Next(total);
            return Take(a, pool, ref roll, out pick) || Take(extra, pool, ref roll, out pick);
        }

        static int Weight(List<SpawnEntry> list, Pool pool)
        {
            if (list == null) return 0;
            int t = 0;
            for (int i = 0; i < list.Count; i++) if (Fits(list[i], pool)) t += list[i].weight;
            return t;
        }

        static bool Take(List<SpawnEntry> list, Pool pool, ref int roll, out SpawnEntry pick)
        {
            pick = default;
            if (list == null) return false;
            for (int i = 0; i < list.Count; i++)
            {
                if (!Fits(list[i], pool)) continue;
                roll -= list[i].weight;
                if (roll < 0) { pick = list[i]; return true; }
            }
            return false;
        }

        /// <summary>The biome tables file swimmers such as axolotls under creatures, so split by habitat here.</summary>
        static bool Fits(SpawnEntry e, Pool pool)
        {
            if (e.weight <= 0) return false;
            if (pool != Pool.Land && pool != Pool.Water) return true;
            var d = MobRegistry.Get(e.mob);
            return d != null && (pool == Pool.Water) == d.aquatic;
        }

        // ---------------------------------------------------------------- placement rules
        static bool CanSpawnAt(World w, MobDef def, Int3 at, ref RNG rng)
        {
            if (at.y <= w.minY || at.y + Mathf.CeilToInt(def.height) >= w.maxY || !w.IsLoaded(at)) return false;
            if (def.aquatic || def.id == "drowned") return WaterSpot(w, def, at);
            if (def.id == "strider") return LavaSpot(w, at);
            var floor = w.GetBlock(at.Offset(Dir.Down));
            if (!FloorOk(def, floor) || !Room(w, def, at)) return false;
            return LightOk(w, def, at, ref rng) && Special(w, def, at, floor, ref rng);
        }

        static bool Empty(World w, Int3 p)
        {
            ushort s = w.GetState(p);
            if (s == 0) return true;
            var b = Blocks.ByState[s];
            if (b.isLiquid) return false;
            // a single snow layer has no collision, so mobs may stand in it
            if (b is SnowLayerBlock) return s - b.baseState == 0;
            if (b.solid) return false;
            return !(b is FireBlock) && !(b is RailBlock) && b.id != "sweet_berry_bush" && b.id != "powder_snow" && b.id != "wither_rose" && b.id != "cobweb";
        }

        /// <summary>The whole hitbox must be clear, so big mobs (spiders, ghasts, hoglins) only appear where they fit.</summary>
        static bool Room(World w, MobDef def, Int3 at)
        {
            float half = def.width * 0.5f;
            int x0 = Mathf.FloorToInt(at.x + 0.5f - half), x1 = Mathf.FloorToInt(at.x + 0.5f + half - 0.001f);
            int z0 = Mathf.FloorToInt(at.z + 0.5f - half), z1 = Mathf.FloorToInt(at.z + 0.5f + half - 0.001f);
            int y1 = at.y + Mathf.Max(1, Mathf.CeilToInt(def.height)) - 1;
            for (int x = x0; x <= x1; x++)
                for (int z = z0; z <= z1; z++)
                    for (int y = at.y; y <= y1; y++)
                        if (!Empty(w, new Int3(x, y, z))) return false;
            return true;
        }

        static bool WaterSpot(World w, MobDef def, Int3 at)
        {
            if (!w.IsWater(at) || w.GetBlock(at.Offset(Dir.Up)).solid) return false;
            var biome = w.GetBiome(at);
            switch (def.id)
            {
                case "drowned": return w.GetLightLevel(at) <= 7 && (biome.isOcean || biome.isRiver || biome.isCave) && at.y < w.seaLevel - 2;
                case "glow_squid": return at.y <= w.seaLevel - 33 && RawLight(w, at) == 0;
                case "axolotl": return biome.key == "lush_caves" && at.y < w.seaLevel;
                case "tadpole": case "guardian": case "elder_guardian": case "zombie_nautilus": return false;
            }
            if (biome.isCave) return true;
            // surface swimmers keep to the top of oceans and rivers
            return at.y >= w.seaLevel - 13 && at.y < w.seaLevel && w.IsWater(at.Offset(Dir.Down));
        }

        static bool LavaSpot(World w, Int3 at)
        {
            if (!w.IsLava(at)) return false;
            var p = at;
            for (int i = 0; i < 8 && w.IsLava(p); i++) p = p.Offset(Dir.Up);
            return w.IsAir(p);
        }

        static bool FloorOk(MobDef def, Block floor)
        {
            if (floor.isLiquid || !floor.solid || floor.hardness < 0 || floor.id == "bedrock") return false;
            if (floor.id == "magma_block" && !def.fireImmune) return false;
            string f = floor.id;
            switch (def.id)
            {
                case "parrot": case "ocelot": return f == "grass_block" || f == "podzol" || floor is LeavesBlock || f.EndsWith("_log", StringComparison.Ordinal);
                case "mooshroom": return f == "mycelium";
                case "rabbit": return f == "grass_block" || f == "snow_block" || f == "sand";
                case "goat": return f == "stone" || f == "snow_block" || f == "packed_ice" || f == "gravel" || f == "grass_block";
                case "wolf": case "fox": return f == "grass_block" || f == "snow_block" || f == "podzol" || f == "coarse_dirt";
                case "polar_bear": return f == "grass_block" || f == "snow_block" || f == "ice" || f == "packed_ice";
                case "armadillo": return f == "grass_block" || f == "sand" || f == "red_sand" || f == "coarse_dirt" || f.EndsWith("terracotta", StringComparison.Ordinal);
                case "camel": return f == "sand" || f == "red_sand";
                case "turtle": return f == "sand";
                case "frog": return f == "grass_block" || f == "mud" || f.Contains("mangrove_roots");
                case "panda": return f == "grass_block" || f == "podzol";
            }
            if (def.category == MobCategory.Creature) return f == "grass_block";
            return floor.opaqueCube || floor is SoulSandBlock || f == "soul_soil";
        }

        static int RawLight(World w, Int3 p)
        {
            byte l = w.GetLightRaw(p.x, p.y, p.z);
            return Mathf.Max(l >> 4, l & 15);
        }

        /// <summary>Monsters need darkness (7 or less) unless they are creatures of the nether or the sea.</summary>
        static bool LightOk(World w, MobDef def, Int3 at, ref RNG rng)
        {
            if (def.category == MobCategory.Ambient) return at.y < w.seaLevel && w.GetLightLevel(at) <= rng.Next(4);
            if (def.category != MobCategory.Monster) return RawLight(w, at) > 8;
            if (w.dim == DimensionId.Nether && def.id != "enderman" && def.id != "skeleton" && def.id != "wither_skeleton") return true;
            switch (def.id)
            {
                case "ghast": case "magma_cube": case "blaze": case "hoglin": case "piglin": case "zombified_piglin": case "slime": return true;
            }
            return w.GetLightLevel(at) <= 7;
        }

        static bool Special(World w, MobDef def, Int3 at, Block floor, ref RNG rng)
        {
            switch (def.id)
            {
                case "husk": case "stray": return w.CanSeeSky(at);
                case "slime": return SlimeSpot(w, at, ref rng);
                case "ocelot": return at.y >= w.seaLevel;
                case "turtle": return at.y <= w.seaLevel + 3;
                case "ghast": return rng.Next(20) == 0;
                case "piglin": case "hoglin": case "zombified_piglin": return floor.id != "nether_wart_block";
                case "phantom": case "creaking": case "warden": case "wither": case "ender_dragon": return false;
            }
            return true;
        }

        static bool SlimeSpot(World w, Int3 at, ref RNG rng)
        {
            var b = w.GetBiome(at);
            if ((b.key == "swamp" || b.key == "mangrove_swamp") && at.y > 50 && at.y < 70 && w.GetLightLevel(at) <= rng.Next(8)) return true;
            return at.y < 40 && IsSlimeChunk(w, at.x >> 4, at.z >> 4);
        }

        /// <summary>One chunk in ten is a slime chunk, fixed by the world seed.</summary>
        public static bool IsSlimeChunk(World w, int cx, int cz) => new RNG(w.seed, cx, cz, SaltSlime).Next(10) == 0;

        /// <summary>At least 24 blocks from every player; natural spawns also stay within the despawn radius.</summary>
        static bool DistanceOk(World w, Vector3 pos, bool needNear)
        {
            float best = float.MaxValue;
            foreach (var e in w.entities)
            {
                if (!(e is Player p) || p.removed || p.IsSpectator) continue;
                float d = (p.position - pos).sqrMagnitude;
                if (d < best) best = d;
            }
            if (best < MinPlayerDistance * MinPlayerDistance) return false;
            return !needNear || best <= SpawnRadius * SpawnRadius;
        }

        // ---------------------------------------------------------------- special spawns
        /// <summary>Rare overworld set pieces: pillager patrols, the wandering trader and phantoms.</summary>
        public static void TickSpecial(World w)
        {
            var s = w != null ? w.session : null;
            if (s == null || w.dim != DimensionId.Overworld) return;
            var st = StateFor(s);
            TrackSleep(w, st);
            TickTrader(w, st);
            if (!s.doMobSpawning) return;
            if (s.gameTime >= st.nextTrader)
            {
                st.nextTrader = s.gameTime + TraderInterval;
                var r = new RNG(w.seed, (int)s.gameTime, 1, SaltSpecial);
                TryTrader(w, st, ref r);
            }
            if (s.difficulty == Difficulty.Peaceful) return;
            if (s.gameTime >= st.nextPatrol)
            {
                var r = new RNG(w.seed, (int)s.gameTime, 2, SaltSpecial);
                st.nextPatrol = s.gameTime + 12000 + r.Next(1200);
                TryPatrol(w, ref r);
            }
            if (s.gameTime >= st.nextPhantom)
            {
                var r = new RNG(w.seed, (int)s.gameTime, 3, SaltSpecial);
                st.nextPhantom = s.gameTime + (60 + r.Next(60)) * 20;
                TryPhantoms(w, st, ref r);
            }
        }

        // ---------------------------------------------------------------- sleep tracking
        /// <summary>A bed resets the insomnia clock; the day counter measures how long it has been since.</summary>
        static void TrackSleep(World w, State st)
        {
            int day = st.session.DayCount;
            foreach (var e in w.entities)
            {
                if (!(e is Player p) || p.removed) continue;
                string key = p.playerName ?? "Steve";
                if (!st.restDay.ContainsKey(key)) st.restDay[key] = day;
                if (p.sleeping) { st.restDay[key] = day; st.sleepers.Add(key); }
                else if (st.sleepers.Remove(key)) st.restDay[key] = day; // the night skip moved the clock on
            }
        }

        /// <summary>Whole days since the player last lay in a bed.</summary>
        public static int DaysSinceRest(Player p)
        {
            if (p == null || p.world == null || p.world.session == null) return 0;
            var st = StateFor(p.world.session);
            return st.restDay.TryGetValue(p.playerName ?? "Steve", out int d) ? st.session.DayCount - d : 0;
        }

        static void TryPhantoms(World w, State st, ref RNG rng)
        {
            var s = w.session;
            if (!w.hasSkyLight || s.SkyDarken(w) < 5) return;
            foreach (var e in w.entities)
            {
                if (!(e is Player p) || p.removed || p.dead || p.IsSpectator || p.IsCreative) continue;
                var bp = Int3.Floor(p.position);
                if (bp.y < w.seaLevel || !w.CanSeeSky(bp)) continue;
                // harder difficulties roll more often
                if (rng.NextFloat() * 3f >= (int)s.difficulty) continue;
                if (!st.restDay.TryGetValue(p.playerName ?? "Steve", out int rest)) continue;
                int days = s.DayCount - rest;
                if (days < PhantomDays) continue;
                long awake = s.dayTime - (long)rest * 24000;
                if (rng.Next((int)Mathf.Min(awake, int.MaxValue)) < PhantomDays * 24000) continue;
                var at = bp.Offset(rng.Range(-10, 10), 20 + rng.Next(15), rng.Range(-10, 10));
                if (at.y >= w.maxY - 2 || !w.IsLoaded(at) || !Empty(w, at)) continue;
                int n = 1 + rng.Next((int)s.difficulty + 1);
                for (int i = 0; i < n; i++)
                {
                    var m = MobRegistry.Spawn(w, "phantom", at.Center, SpawnReason.Natural);
                    if (m != null) m.target = p;
                }
            }
        }

        // ---------------------------------------------------------------- wandering trader
        static void TryTrader(World w, State st, ref RNG rng)
        {
            if (st.trader != null && !st.trader.removed && !st.trader.dead) return;
            // each missed visit makes the next one likelier: 25%, 50%, then 75%
            int chance = st.traderChance;
            st.traderChance = Mathf.Min(75, st.traderChance + 25);
            if (rng.Next(100) >= chance) return;
            var p = RandomPlayer(w, ref rng);
            if (p == null || !SurfaceSpot(w, p.position, 8, 48, ref rng, out Vector3 at)) return;
            var trader = MobRegistry.Spawn(w, "wandering_trader", at, SpawnReason.Natural) as VillagerMob;
            if (trader == null) return;
            st.traderChance = 25;
            st.trader = trader;
            st.traderLeaves = w.session.gameTime + TraderLifetime;
            st.llamas.Clear();
            for (int i = 0; i < 2; i++)
            {
                if (!SurfaceSpot(w, at, 1, 4, ref rng, out Vector3 lp)) lp = at;
                var llama = MobRegistry.Spawn(w, "trader_llama", lp, SpawnReason.Natural);
                if (llama == null) continue;
                llama.goals.Add(3, new FollowLeaderGoal(trader, 6f, 1.1f), GoalFlags.Move);
                st.llamas.Add(llama);
            }
            Sounds.Play("entity.wandering_trader.ambient", at, 1f, 1f);
        }

        /// <summary>The trader leaves after 40 minutes, taking its llamas, unless someone is mid-trade.</summary>
        static void TickTrader(World w, State st)
        {
            var t = st.trader;
            if (t == null) return;
            if (t.removed || t.dead) { st.trader = null; st.llamas.Clear(); return; }
            if (st.session.gameTime < st.traderLeaves || t.tradingWith != null) return;
            Particles.Poof(w, t.position + Vector3.up * t.height * 0.5f, t.width, t.height);
            t.Remove();
            foreach (var l in st.llamas)
            {
                if (l.removed || l.dead || l.tamed || l.passengers.Count > 0) continue;
                Particles.Poof(w, l.position + Vector3.up * l.height * 0.5f, l.width, l.height);
                l.Remove();
            }
            st.trader = null;
            st.llamas.Clear();
        }

        // ---------------------------------------------------------------- patrols
        static void TryPatrol(World w, ref RNG rng)
        {
            var s = w.session;
            // patrols are a daytime threat that starts after the first few days
            if (s.DayCount < 5 || s.IsNight || rng.Next(5) != 0) return;
            var p = RandomPlayer(w, ref rng);
            if (p == null) return;
            int x = Mathf.FloorToInt(p.position.x) + (24 + rng.Next(24)) * (rng.NextBool() ? -1 : 1);
            int z = Mathf.FloorToInt(p.position.z) + (24 + rng.Next(24)) * (rng.NextBool() ? -1 : 1);
            if (!w.IsLoaded(x - 10, z - 10) || !w.IsLoaded(x + 10, z + 10)) return;
            if (w.GetSurfaceBiome(x, z).key == "mushroom_fields") return;
            var heading = new Vector3(x - p.position.x, 0, z - p.position.z).normalized;
            var dest = new Vector3(x, 0, z) + heading * 80f;
            int members = rng.Range(2, 4);   // a captain plus 2-4 followers
            Mob captain = null;
            for (int i = 0; i <= members; i++)
            {
                if (i > 0) { x += rng.Next(5) - rng.Next(5); z += rng.Next(5) - rng.Next(5); }
                var at = new Int3(x, 0, z);
                if (!Settle(w, Pool.Land, ref at, 0) || !Empty(w, at.Offset(Dir.Up))) continue;
                if (w.GetBlockLight(at) > 8) continue;
                var m = MobRegistry.Spawn(w, "pillager", new Vector3(at.x + 0.5f, at.y, at.z + 0.5f), SpawnReason.Natural);
                if (m == null) continue;
                if (captain == null) captain = m;
                m.goals.Add(4, new PatrolGoal(captain == m ? null : captain, dest, heading), GoalFlags.Move);
            }
            if (captain != null) Sounds.Play("entity.pillager.ambient", captain.position, 1f, 0.9f);
        }

        // ---------------------------------------------------------------- helpers
        static Player RandomPlayer(World w, ref RNG rng)
        {
            Player pick = null; int n = 0;
            foreach (var e in w.entities)
            {
                if (!(e is Player p) || p.removed || p.dead || p.IsSpectator) continue;
                if (rng.Next(++n) == 0) pick = p;
            }
            return pick;
        }

        /// <summary>An open, dry surface block min..max blocks from centre with two blocks of headroom.</summary>
        static bool SurfaceSpot(World w, Vector3 centre, int min, int max, ref RNG rng, out Vector3 at)
        {
            for (int i = 0; i < 12; i++)
            {
                float a = rng.Range(0f, Mathf.PI * 2f), d = rng.Range((float)min, (float)max);
                var p = new Int3(Mathf.FloorToInt(centre.x + Mathf.Cos(a) * d), 0, Mathf.FloorToInt(centre.z + Mathf.Sin(a) * d));
                if (!Settle(w, Pool.Land, ref p, 0) || !Empty(w, p.Offset(Dir.Up))) continue;
                at = new Vector3(p.x + 0.5f, p.y, p.z + 0.5f);
                return true;
            }
            at = centre;
            return false;
        }

        // ---------------------------------------------------------------- persistence (optional)
        /// <summary>Insomnia and visitor clocks; call next to the other level data so phantoms survive a reload.</summary>
        public static void Save(Dictionary<string, string> d)
        {
            if (d == null || state == null) return;
            long now = state.session.gameTime;
            d["spawnTrader"] = (state.nextTrader - now).ToString(CultureInfo.InvariantCulture);
            d["spawnTraderChance"] = state.traderChance.ToString(CultureInfo.InvariantCulture);
            d["spawnPatrol"] = (state.nextPatrol - now).ToString(CultureInfo.InvariantCulture);
            var parts = new List<string>();
            foreach (var kv in state.restDay) parts.Add(kv.Key + ":" + kv.Value.ToString(CultureInfo.InvariantCulture));
            d["spawnRest"] = string.Join(";", parts);
        }

        public static void Load(Dictionary<string, string> d)
        {
            loaded = d != null ? new Dictionary<string, string>(d) : null;
            state = null;
        }

        static void Apply(State st, Dictionary<string, string> d)
        {
            long now = st.session.gameTime;
            if (d.TryGetValue("spawnTrader", out var t) && long.TryParse(t, NumberStyles.Integer, CultureInfo.InvariantCulture, out long tv)) st.nextTrader = now + tv;
            if (d.TryGetValue("spawnTraderChance", out var c) && int.TryParse(c, NumberStyles.Integer, CultureInfo.InvariantCulture, out int cv)) st.traderChance = Mathf.Clamp(cv, 25, 75);
            if (d.TryGetValue("spawnPatrol", out var p) && long.TryParse(p, NumberStyles.Integer, CultureInfo.InvariantCulture, out long pv)) st.nextPatrol = now + pv;
            if (!d.TryGetValue("spawnRest", out var r) || string.IsNullOrEmpty(r)) return;
            foreach (var part in r.Split(';'))
            {
                int i = part.LastIndexOf(':');
                if (i > 0 && int.TryParse(part.Substring(i + 1), NumberStyles.Integer, CultureInfo.InvariantCulture, out int day)) st.restDay[part.Substring(0, i)] = day;
            }
        }

        // ---------------------------------------------------------------- goals
        /// <summary>Keeps a trader llama near its trader (there is no leash rope, so they simply follow).</summary>
        sealed class FollowLeaderGoal : Goal
        {
            readonly Mob leader; readonly float range, speed; int repath;
            public FollowLeaderGoal(Mob leader, float range, float speed) { this.leader = leader; this.range = range; this.speed = speed; }
            bool Alive => leader != null && !leader.removed && !leader.dead && leader.world == mob.world;
            public override bool CanUse() => Alive && !mob.tamed && mob.passengers.Count == 0 && (leader.position - mob.position).sqrMagnitude > range * range;
            public override bool CanContinue() => Alive && (leader.position - mob.position).sqrMagnitude > 9f;
            public override void Start() { repath = 0; }
            public override void Stop() { mob.nav.Stop(); }
            public override void Tick()
            {
                mob.LookAt(leader);
                if (--repath <= 0) { repath = 10; mob.nav.MoveTo(leader.position, speed); }
            }
        }

        /// <summary>The captain walks the patrol route; the others keep formation around the captain.</summary>
        sealed class PatrolGoal : Goal
        {
            readonly Mob leader; readonly Vector3 heading; Vector3 dest; int repath;
            public PatrolGoal(Mob leader, Vector3 dest, Vector3 heading) { this.leader = leader; this.dest = dest; this.heading = heading.sqrMagnitude > 0.01f ? heading : Vector3.forward; }
            bool LeaderAlive => leader != null && !leader.removed && !leader.dead;
            public override bool CanUse() => mob.target == null;
            public override bool CanContinue() => mob.target == null;
            public override void Start() { repath = 0; }
            public override void Stop() { mob.nav.Stop(); }
            public override void Tick()
            {
                if (--repath > 0) return;
                repath = 20;
                if (LeaderAlive)
                {
                    if ((leader.position - mob.position).sqrMagnitude > 25f) mob.nav.MoveTo(leader.position, 0.9f);
                    return;
                }
                var d = dest - mob.position; d.y = 0;
                // a reached waypoint just moves the route on in the same direction
                if (d.sqrMagnitude < 64f) dest += heading * 64f;
                mob.nav.MoveTo(new Vector3(dest.x, mob.position.y, dest.z), 0.7f);
            }
        }
    }
}
