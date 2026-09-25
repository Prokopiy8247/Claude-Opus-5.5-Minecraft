using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    [Flags] public enum GoalFlags { None = 0, Move = 1, Look = 2, Jump = 4, Target = 8 }

    public abstract class Goal
    {
        public Mob mob; public int priority; public GoalFlags flags; public bool running;
        public virtual bool CanUse() => false;
        public virtual bool CanContinue() => CanUse();
        public virtual void Start() { }
        public virtual void Stop() { }
        public virtual void Tick() { }
        public virtual bool Interruptible => true;
    }

    public sealed class GoalSelector
    {
        readonly Mob mob;
        public readonly List<Goal> goals = new List<Goal>();
        int tick;
        public GoalSelector(Mob m) { mob = m; }
        public void Add(int priority, Goal g, GoalFlags flags) { g.mob = mob; g.priority = priority; g.flags = flags; goals.Add(g); goals.Sort((a, b) => a.priority.CompareTo(b.priority)); }
        public void Clear() { foreach (var g in goals) if (g.running) { g.running = false; g.Stop(); } goals.Clear(); }
        public void StopAll() { foreach (var g in goals) if (g.running) { g.running = false; g.Stop(); } }

        public void Tick()
        {
            tick++;
            // stop goals that can't continue
            foreach (var g in goals)
                if (g.running && !g.CanContinue()) { g.running = false; g.Stop(); }
            if (tick % 2 == 0)
            {
                GoalFlags used = GoalFlags.None;
                foreach (var g in goals)
                {
                    if (g.running) { used |= g.flags; continue; }
                    if ((g.flags & used) != 0)
                    {
                        // a running goal with lower priority (bigger number) holding the flags can be interrupted
                        bool canPreempt = true;
                        foreach (var r in goals) if (r.running && (r.flags & g.flags) != 0 && (r.priority <= g.priority || !r.Interruptible)) { canPreempt = false; break; }
                        if (!canPreempt) continue;
                    }
                    if (!g.CanUse()) continue;
                    foreach (var r in goals) if (r.running && (r.flags & g.flags) != 0) { r.running = false; r.Stop(); }
                    g.running = true; g.Start();
                    used |= g.flags;
                }
            }
            foreach (var g in goals) if (g.running) g.Tick();
        }
        public bool IsRunning<T>() where T : Goal { foreach (var g in goals) if (g is T && g.running) return true; return false; }
    }

    /// <summary>Base class for all mobs: definition-driven stats, goal AI, navigation, breeding, taming, loot.</summary>
    public abstract partial class Mob : LivingEntity
    {
        public MobDef def;
        public int variant;
        public bool baby; public int growAge;
        public int loveTicks, breedCooldown; public Player lovePlayer;
        public LivingEntity target;
        public SpawnReason spawnReason;
        public GoalSelector goals, targetGoals;
        public PathNavigator nav;
        public Vector3? moveTarget; public float speedMod = 1f;
        public Vector3? lookTarget; public float lookYaw, lookPitch, prevLookPitch;
        public int attackCooldown;
        public int ambientTimer;
        public int angerTicks; public int angryAtId = -1;
        public bool tamed, sitting; public string ownerName; public int ownerId = -1;
        public bool saddled;
        public MobVisual visual;
        public int lastHurtByTick = -1000;
        public bool aggressiveAnim; // arms up / charging
        public float swell, prevSwell;   // creeper-like charge 0..1
        public int despawnTimer;
        public float sizeScale = 1f;

        public override string TypeId => def.id;
        public override string DisplayName => customName ?? def.displayName;
        public override float EyeHeight => height * def.eyeFactor;
        public virtual bool IsBaby => baby;
        public virtual bool CanDespawn => (def.hostile || def.category == MobCategory.Ambient || def.category == MobCategory.WaterAmbient) && !persistent && customName == null && !tamed;
        public override bool IsSolidToOthers => def.id == "shulker" || def.boss && false;
        public override string HurtSound => "entity." + def.soundId + ".hurt";
        public override string DeathSound => "entity." + def.soundId + ".death";
        public virtual string AmbientSound => "entity." + def.soundId + ".ambient";
        public override float VoicePitch => (UnityEngine.Random.value - UnityEngine.Random.value) * 0.2f + (IsBaby ? 1.5f : 1f);
        public override float BaseMaxHealth => def != null ? def.maxHealth : 20f;
        public Player Owner => ownerId >= 0 && world != null ? world.entities.Find(e => e.id == ownerId) as Player : (tamed && ownerName != null ? GameManager.Instance?.player : null);

        public virtual void Setup(MobDef d)
        {
            def = d;
            maxHealth = health = d.maxHealth;
            movementSpeed = d.speed;
            width = d.width; height = d.height;
            armorValueBase = d.armor; knockbackResistance = d.knockbackResistance;
            fireImmune = d.fireImmune; undead = d.undead; arthropod = d.arthropod;
            noGravity = d.noGravity;
            canBreatheUnderwater = d.aquatic || d.amphibious || d.undead;
            stepHeight = d.width > 1.2f ? 1.0f : 0.6f;
            goals = new GoalSelector(this); targetGoals = new GoalSelector(this);
            nav = new PathNavigator(this);
            ambientTimer = UnityEngine.Random.Range(0, d.ambientInterval);
            RegisterGoals();
        }

        protected virtual void RegisterGoals() { }
        public virtual void OnInitialSpawn(SpawnReason reason)
        {
            if (def.canBreed && reason == SpawnReason.Natural && UnityEngine.Random.value < 0.05f) SetBaby(true);
        }

        public void SetBaby(bool b)
        {
            baby = b; growAge = b ? -24000 : 0;
            UpdateSize();
        }
        protected virtual void UpdateSize()
        {
            float s = (baby ? 0.5f : 1f) * sizeScale;
            width = def.width * s; height = def.height * s;
        }

        // ------------------------------------------------------------------ tick
        public override void Tick()
        {
            prevSwell = swell; prevLookPitch = lookPitch;
            base.Tick();
            if (removed || dead) return;
            headYaw = lookYaw;
            pitch = lookPitch;
            if (attackCooldown > 0) attackCooldown--;
            if (loveTicks > 0) { loveTicks--; if (loveTicks % 10 == 0) Particles.Heart(world, position + Vector3.up * height); }
            if (breedCooldown > 0) breedCooldown--;
            if (growAge < 0) { growAge++; if (growAge == 0) { baby = false; UpdateSize(); } }
            if (angerTicks > 0 && --angerTicks == 0) { angryAtId = -1; if (target != null && !def.hostile) target = null; }
            if (target != null && (target.removed || target.dead || target.world != world || (target is Player tp && (tp.IsCreative || tp.IsSpectator) && !(this is WitherBoss) && !(this is EnderDragonMob)))) target = null;
            // sounds
            if (--ambientTimer <= 0)
            {
                ambientTimer = def.ambientInterval + UnityEngine.Random.Range(0, def.ambientInterval);
                string s = AmbientSound;
                if (s != null) Sounds.Play(s, position, 1f, VoicePitch);
            }
            // sunlight burn
            if (def.burnsInDay && world.session != null && world.dim == DimensionId.Overworld && !world.session.IsNight && !inWater && !IsBaby)
            {
                Int3 p = Int3.Floor(position + Vector3.up * EyeHeight);
                if (world.CanSeeSky(p) && world.GetBrightness(position) > 0.5f && !world.session.IsRaining && UnityEngine.Random.value * 30f < (world.GetBrightness(position) - 0.4f) * 2f)
                {
                    var helmet = equipment[5];
                    if (helmet != null) { if (UnityEngine.Random.value < 0.05f) { helmet.damage++; if (helmet.damage >= helmet.MaxDamage) equipment[5] = null; } }
                    else SetOnFire(8);
                }
            }
            // despawn
            if (CanDespawn) TickDespawn();
            if (world.session != null && world.session.difficulty == Difficulty.Peaceful && def.hostile && def.category == MobCategory.Monster && !persistent) Remove();
        }

        void TickDespawn()
        {
            var p = world.NearestPlayer(position);
            if (p == null) return;
            float d2 = (p.position - position).sqrMagnitude;
            if (d2 > 128 * 128) { Remove(); return; }
            if (d2 > 32 * 32) { if (++despawnTimer > 600 && UnityEngine.Random.Range(0, 800) == 0) Remove(); }
            else despawnTimer = 0;
        }

        protected override void AiStep()
        {
            if (!dead)
            {
                if (world.tickCount % 2 == (id & 1) || target != null) targetGoals.Tick();
                goals.Tick();
                nav.Tick();
                MoveControl();
                LookControl();
            }
            if (def.flying || (def.aquatic && inWater)) CustomTravel();
            else base.AiStep();
        }

        /// <summary>Ground move control: face the move target and walk.</summary>
        protected virtual void MoveControl()
        {
            if (moveTarget.HasValue && !sitting)
            {
                Vector3 d = moveTarget.Value - position;
                float h2 = d.x * d.x + d.z * d.z;
                if (h2 < 0.0025f) { moveForward = 0; moveStrafe = 0; moveTarget = null; jumping = false; return; }
                float desired = Mathf.Atan2(d.x, d.z) * Mathf.Rad2Deg;
                yaw = MathX.ApproachAngle(yaw, desired, 90f);
                movementSpeed = def.speed * speedMod * (IsBaby && def.hostile ? 1.5f : 1f);
                moveForward = 1f;
                jumping = d.y > stepHeight - 0.05f && h2 < Mathf.Max(1f, width * width) || (horizontalCollision && onGround && d.y > -0.5f);
                if (inWater && d.y > 0) jumping = true;
            }
            else { moveForward = 0; moveStrafe = 0; if (!inWater && !inLava) jumping = false; }
        }

        protected virtual void LookControl()
        {
            if (lookTarget.HasValue)
            {
                Vector3 d = lookTarget.Value - EyePosition;
                float wantYaw = Mathf.Atan2(d.x, d.z) * Mathf.Rad2Deg;
                float wantPitch = -Mathf.Atan2(d.y, Mathf.Sqrt(d.x * d.x + d.z * d.z)) * Mathf.Rad2Deg;
                lookYaw = MathX.ApproachAngle(lookYaw, wantYaw, 10f);
                lookPitch = Mathf.MoveTowards(lookPitch, Mathf.Clamp(wantPitch, -60, 60), 10f);
                lookTarget = null;
            }
            else
            {
                lookYaw = MathX.ApproachAngle(lookYaw, bodyYaw, 10f);
                lookPitch = Mathf.MoveTowards(lookPitch, 0, 5f);
            }
            float diff = MathX.WrapAngle(lookYaw - bodyYaw);
            if (diff > 75) lookYaw = bodyYaw + 75; else if (diff < -75) lookYaw = bodyYaw - 75;
        }

        /// <summary>Flying / swimming movement: steer velocity toward the move target.</summary>
        protected virtual void CustomTravel()
        {
            float spd = def.flying ? (def.flySpeed > 0 ? def.flySpeed : def.speed * 0.5f) : def.speed * 0.1f;
            if (moveTarget.HasValue)
            {
                Vector3 d = moveTarget.Value - position;
                float dist = d.magnitude;
                if (dist > 0.05f)
                {
                    Vector3 dir = d / dist;
                    velocity += dir * spd * speedMod * 0.25f;
                    float desired = Mathf.Atan2(d.x, d.z) * Mathf.Rad2Deg;
                    yaw = MathX.ApproachAngle(yaw, desired, 20f);
                }
                if (dist < 0.3f) moveTarget = null;
            }
            if (def.aquatic && inWater) { velocity *= 0.9f; }
            else if (!def.flying) velocity.y -= 0.08f;
            else velocity *= 0.91f;
            Move(velocity);
            fallDistance = 0;
        }

        public void LookAt(Vector3 p) { lookTarget = p; }
        public void LookAt(Entity e) { lookTarget = e.EyePosition; }

        // ------------------------------------------------------------------ combat
        public virtual float AttackReachSqr(LivingEntity t) => width * 2f * width * 2f + t.width;

        public virtual bool DoMeleeAttack(Entity t)
        {
            float dmg = def.attackDamage;
            var diff = world.session?.difficulty ?? Difficulty.Normal;
            if (def.hostile && diff == Difficulty.Easy) dmg = Mathf.Max(1, dmg * 0.5f + 1f);
            else if (def.hostile && diff == Difficulty.Hard) dmg *= 1.5f;
            var held = equipment[0];
            if (held != null) dmg += held.item.attackDamage - 1f;
            SwingArm();
            bool hurt = t.Hurt(DamageSource.MobAttack(this), dmg);
            if (hurt)
            {
                OnAttackHit(t);
                if (held != null && held.GetEnchant(Enchant.FireAspect) > 0) t.SetOnFire(4 * held.GetEnchant(Enchant.FireAspect));
            }
            return hurt;
        }
        protected virtual void OnAttackHit(Entity t) { }

        public override bool Hurt(DamageSource src, float amount)
        {
            if (sitting) sitting = false;
            bool r = base.Hurt(src, amount);
            if (r)
            {
                lastHurtByTick = age;
                if (src.attacker is LivingEntity le && le != this && !(le is Player p && (p.IsCreative && !def.hostile && false)))
                    OnHurtBy(le);
            }
            return r;
        }

        protected virtual void OnHurtBy(LivingEntity attacker)
        {
            if (def.neutral || def.hostile)
            {
                if (attacker is Player p && (p.IsCreative || p.IsSpectator)) { if (def.neutral && !def.hostile) { target = attacker; angerTicks = 400 + UnityEngine.Random.Range(0, 400); angryAtId = attacker.id; } return; }
                target = attacker; angerTicks = 400 + UnityEngine.Random.Range(0, 400); angryAtId = attacker.id;
            }
        }

        public bool CanTarget(LivingEntity t, float range, bool needSight = true)
        {
            if (t == null || t == this || t.dead || t.removed || t.world != world || !t.Attackable) return false;
            if (t is Player p && (p.IsCreative || p.IsSpectator)) return false;
            float r = range;
            if (t.HasEffect(Effect.Invisibility)) r *= 0.3f;
            if (t.sneaking) r *= 0.8f;
            if ((t.position - position).sqrMagnitude > r * r) return false;
            if (needSight && !world.HasLineOfSight(EyePosition, t.EyePosition)) return false;
            return true;
        }

        // ------------------------------------------------------------------ interaction
        public override bool Interact(Player p, ItemStack held)
        {
            if (held != null && held.item.id == "name_tag" && held.customName != null) { customName = held.customName; persistent = true; if (!p.IsCreative) held.count--; return true; }
            if (held != null && held.item is SpawnEggItem egg && egg.mobId == def.id && def.canBreed)
            {
                var b = MobRegistry.Spawn(world, def.id, position, SpawnReason.SpawnEgg);
                if (b != null) b.SetBaby(true);
                if (!p.IsCreative) held.count--;
                return true;
            }
            if (held != null && IsFood(held.item) && def.canBreed)
            {
                if (IsBaby) { growAge = Math.Min(0, growAge + (-growAge) / 10); Consume(p, held); Particles.HappyVillager(world, position + Vector3.up * height, 4); return true; }
                if (breedCooldown == 0 && loveTicks == 0 && CanFallInLove(p)) { loveTicks = 600; lovePlayer = p; Consume(p, held); return true; }
            }
            return false;
        }
        protected virtual bool CanFallInLove(Player p) => true;
        protected void Consume(Player p, ItemStack held)
        {
            if (!p.IsCreative) { held.count--; if (held.item.id.EndsWith("_bucket")) p.inventory.AddOrDrop(new ItemStack("bucket", 1)); }
            Sounds.Play("entity." + def.soundId + ".eat", position, 1f, 1f);
            p.SwingArm();
        }
        public bool IsFood(Item it) { if (it == null) return false; foreach (var f in def.foods) if (f == it.id) return true; return false; }

        // ------------------------------------------------------------------ death
        protected override void OnDeath(DamageSource src)
        {
            nav.Stop();
            var killer = src.attacker as LivingEntity ?? lastAttacker;
            bool byPlayer = killer is Player;
            int looting = 0;
            if (killer is Player kp && kp.inventory.Selected != null) looting = kp.inventory.Selected.GetEnchant(Enchant.Looting);
            var drops = new List<ItemStack>();
            var rng = new RNG(UnityEngine.Random.Range(int.MinValue, int.MaxValue));
            Loot.MobDrops(this, looting, drops, ref rng);
            DropExtra(drops, byPlayer, looting, ref rng);
            for (int i = 0; i < equipment.Length; i++)
            {
                var e = equipment[i];
                if (e == null) continue;
                if (persistentEquipment[i] || UnityEngine.Random.value < dropChances[i] + looting * 0.01f)
                {
                    var c = e.Copy();
                    if (c.item.IsDamageable && !persistentEquipment[i]) c.damage = Mathf.Clamp(c.MaxDamage - UnityEngine.Random.Range(1, Mathf.Max(2, c.MaxDamage / 2)), 0, c.MaxDamage - 1);
                    drops.Add(c);
                }
            }
            foreach (var d in drops) world.SpawnItem(position + Vector3.up * 0.3f, d);
            if ((byPlayer || age - lastHurtByTick < 100 && lastAttacker is Player) && !IsBaby) { int xp = XpReward(); if (xp > 0) XpOrb.Spawn(world, position + Vector3.up * 0.5f, xp); }
            if (killer is Player pk) { pk.score += def.xp; Achievements.OnKill(pk, def.id); }
            if (tamed && Owner != null) GameManager.Instance?.hud?.Chat((customName ?? def.displayName) + " " + src.DeathMessage("").Trim());
        }
        public readonly bool[] persistentEquipment = new bool[6];
        protected virtual void DropExtra(List<ItemStack> drops, bool byPlayer, int looting, ref RNG rng) { }
        protected virtual int XpReward()
        {
            int xp = def.xp;
            if (def.hostile) for (int i = 0; i < 6; i++) if (equipment[i] != null) xp += UnityEngine.Random.Range(1, 4);
            return xp;
        }

        public virtual void OnArrowDodge() { }

        // ------------------------------------------------------------------ breeding helpers
        public Mob FindMate()
        {
            Mob best = null; float bd = 64;
            foreach (var e in world.GetEntities(Bounds.Grow(8f), this))
                if (e is Mob m && m.def == def && m.loveTicks > 0 && !m.IsBaby && !m.removed && !m.dead)
                {
                    float d = (m.position - position).sqrMagnitude;
                    if (d < bd) { bd = d; best = m; }
                }
            return best;
        }

        public virtual Mob Breed(Mob mate)
        {
            var baby = MobRegistry.Spawn(world, def.id, position, SpawnReason.Breeding);
            if (baby == null) return null;
            baby.SetBaby(true);
            baby.variant = UnityEngine.Random.value < 0.5f ? variant : mate.variant;
            if (tamed) { baby.tamed = true; baby.ownerId = ownerId; baby.ownerName = ownerName; baby.persistent = true; }
            XpOrb.Spawn(world, position, UnityEngine.Random.Range(1, 8));
            if (lovePlayer != null) Achievements.Grant(lovePlayer, "bred_animals");
            return baby;
        }

        // ------------------------------------------------------------------ rendering
        public override void CreateVisual()
        {
            visual = new MobVisual(this);
            go = visual.root;
        }
        public override void Render(float partial)
        {
            if (visual != null) visual.Update(partial);
        }
        public override void Remove()
        {
            base.Remove();
            visual = null;
        }

        // ------------------------------------------------------------------ persistence
        public override void Save(Dictionary<string, string> d)
        {
            base.Save(d);
            d["variant"] = variant.ToString();
            if (baby) d["baby"] = growAge.ToString();
            if (tamed) { d["tamed"] = "1"; d["owner"] = ownerName ?? "Steve"; }
            if (sitting) d["sit"] = "1";
            if (saddled) d["saddle"] = "1";
            if (persistent) d["persist"] = "1";
            d["yaw"] = yaw.ToString("R");
        }
        public override void Load(Dictionary<string, string> d)
        {
            base.Load(d);
            if (d.TryGetValue("variant", out var v)) int.TryParse(v, out variant);
            if (d.TryGetValue("baby", out var b) && int.TryParse(b, out int ga)) { baby = true; growAge = ga; UpdateSize(); }
            tamed = d.TryGetValue("tamed", out var t) && t == "1";
            if (tamed) { d.TryGetValue("owner", out ownerName); ownerId = -1; }
            sitting = d.TryGetValue("sit", out var s) && s == "1";
            saddled = d.TryGetValue("saddle", out var sd) && sd == "1";
            persistent = d.TryGetValue("persist", out var ps) && ps == "1";
            if (d.TryGetValue("yaw", out var y) && float.TryParse(y, out float yy)) { yaw = prevYaw = bodyYaw = lookYaw = yy; }
        }
    }

    // =====================================================================================================
    /// <summary>A* pathfinding over the block grid for ground mobs (step up 1, drop up to 3), direct steering for flyers/swimmers.</summary>
    public sealed class PathNavigator
    {
        readonly Mob mob;
        List<Int3> path; int index; float speed = 1f;
        Vector3 goal; int stuck; Vector3 lastPos; int ticksOnNode;
        public Entity followEntity;
        public bool IsDone => path == null && !direct;
        bool direct; Vector3 directTarget;
        public static int searchesThisTick; public static long searchTick;

        public PathNavigator(Mob m) { mob = m; }

        public bool MoveTo(Vector3 pos, float spd)
        {
            speed = spd; goal = pos; followEntity = null;
            if (mob.def.flying || (mob.def.aquatic && mob.inWater) || mob.noPhysics) { direct = true; directTarget = pos; path = null; return true; }
            if (searchTick != mob.world.tickCount) { searchTick = mob.world.tickCount; searchesThisTick = 0; }
            if (searchesThisTick++ > 24) { direct = true; directTarget = pos; path = null; return true; }
            var p = Pathfinder.Find(mob, pos, (int)Mathf.Clamp(mob.def.followRange, 16, 48));
            if (p == null || p.Count == 0) { path = null; direct = false; return false; }
            path = p; index = 0; direct = false; stuck = 0; ticksOnNode = 0;
            return true;
        }
        public bool MoveTo(Entity e, float spd) { bool r = MoveTo(e.position, spd); followEntity = e; return r; }
        public void Stop() { path = null; direct = false; mob.moveTarget = null; followEntity = null; }

        public void Tick()
        {
            if (direct)
            {
                mob.moveTarget = directTarget; mob.speedMod = speed;
                if ((mob.position - directTarget).sqrMagnitude < 0.6f) { direct = false; mob.moveTarget = null; }
                return;
            }
            if (path == null) return;
            if (index >= path.Count) { Stop(); return; }
            int w = Mathf.Max(1, Mathf.CeilToInt(mob.width - 0.01f));
            var n = path[index];
            Vector3 np = new Vector3(n.x + w * 0.5f, n.y, n.z + w * 0.5f);
            float dx = np.x - mob.position.x, dz = np.z - mob.position.z;
            float reach = Mathf.Max(0.35f, mob.width * 0.5f);
            if (dx * dx + dz * dz < reach * reach && Mathf.Abs(np.y - mob.position.y) < 1.2f)
            {
                index++; ticksOnNode = 0;
                if (index >= path.Count) { Stop(); return; }
                n = path[index]; np = new Vector3(n.x + w * 0.5f, n.y, n.z + w * 0.5f);
            }
            mob.moveTarget = np; mob.speedMod = speed;
            if (++ticksOnNode > 60) { Stop(); return; }
            if (mob.age % 20 == 0)
            {
                if ((mob.position - lastPos).sqrMagnitude < 0.01f && ++stuck > 3) { Stop(); return; }
                lastPos = mob.position;
            }
        }
    }

    public static class Pathfinder
    {
        struct Node { public Int3 pos; public float g, f; public long parent; public bool closed; }
        static readonly Dictionary<long, Node> nodes = new Dictionary<long, Node>();
        static readonly List<long> open = new List<long>();
        static readonly Int3[] dirs = { new Int3(1, 0, 0), new Int3(-1, 0, 0), new Int3(0, 0, 1), new Int3(0, 0, -1), new Int3(1, 0, 1), new Int3(1, 0, -1), new Int3(-1, 0, 1), new Int3(-1, 0, -1) };

        public static List<Int3> Find(Mob m, Vector3 target, int range)
        {
            var w = m.world;
            int sw = Mathf.Max(1, Mathf.CeilToInt(m.width - 0.01f)), sh = Mathf.Max(1, Mathf.CeilToInt(m.height - 0.01f));
            Int3 start = new Int3(Mathf.FloorToInt(m.position.x - (sw - 1) * 0.5f), Mathf.FloorToInt(m.position.y + 0.1f), Mathf.FloorToInt(m.position.z - (sw - 1) * 0.5f));
            Int3 goal = new Int3(Mathf.FloorToInt(target.x - (sw - 1) * 0.5f), Mathf.FloorToInt(target.y + 0.1f), Mathf.FloorToInt(target.z - (sw - 1) * 0.5f));
            if (start.DistSq(goal) > (range + 8) * (range + 8)) { Vector3 dir = (target - m.position).normalized * range; goal = Int3.Floor(m.position + dir); }
            nodes.Clear(); open.Clear();
            long sk = start.Pack();
            nodes[sk] = new Node { pos = start, g = 0, f = H(start, goal), parent = long.MinValue };
            open.Add(sk);
            long bestKey = sk; float bestH = H(start, goal);
            int maxNodes = 400 + range * 8;
            bool swim = m.def.aquatic || m.def.amphibious;
            int maxDrop = m.def.id == "cat" || m.def.id == "ocelot" ? 5 : (m is SpiderMob ? 6 : 3);
            int iter = 0;
            while (open.Count > 0 && iter++ < maxNodes)
            {
                // pop lowest f
                int bi = 0; float bf = float.MaxValue;
                for (int i = 0; i < open.Count; i++) { var nn = nodes[open[i]]; if (nn.f < bf) { bf = nn.f; bi = i; } }
                long ck = open[bi]; open[bi] = open[open.Count - 1]; open.RemoveAt(open.Count - 1);
                var cur = nodes[ck]; cur.closed = true; nodes[ck] = cur;
                float h = H(cur.pos, goal);
                if (h < bestH) { bestH = h; bestKey = ck; }
                if (cur.pos.x == goal.x && cur.pos.z == goal.z && Mathf.Abs(cur.pos.y - goal.y) <= 1) { bestKey = ck; break; }
                for (int di = 0; di < dirs.Length; di++)
                {
                    var d = dirs[di];
                    bool diag = d.x != 0 && d.z != 0;
                    if (diag)
                    {
                        // both orthogonal neighbours must be passable at the same level
                        if (!Free(w, cur.pos.x + d.x, cur.pos.y, cur.pos.z, sw, sh, m) || !Free(w, cur.pos.x, cur.pos.y, cur.pos.z + d.z, sw, sh, m)) continue;
                    }
                    int nx = cur.pos.x + d.x, nz = cur.pos.z + d.z;
                    int ny = int.MinValue; float cost = diag ? 1.414f : 1f;
                    if (Free(w, nx, cur.pos.y, nz, sw, sh, m))
                    {
                        if (Ground(w, nx, cur.pos.y - 1, nz, sw) || (swim && Water(w, nx, cur.pos.y, nz))) ny = cur.pos.y;
                        else
                        {
                            for (int k = 1; k <= maxDrop; k++)
                            {
                                if (!Free(w, nx, cur.pos.y - k, nz, sw, sh, m)) break;
                                if (Ground(w, nx, cur.pos.y - k - 1, nz, sw) || Water(w, nx, cur.pos.y - k, nz)) { ny = cur.pos.y - k; cost += k * 0.5f; break; }
                            }
                        }
                    }
                    else if (!diag && Free(w, nx, cur.pos.y + 1, nz, sw, sh, m) && Free(w, cur.pos.x, cur.pos.y + 1, cur.pos.z, sw, sh, m) && Ground(w, nx, cur.pos.y, nz, sw) && !Tall(w, nx, cur.pos.y, nz))
                    { ny = cur.pos.y + 1; cost += 0.5f; }
                    if (ny == int.MinValue) continue;
                    var np = new Int3(nx, ny, nz);
                    if (Water(w, nx, ny, nz) && !m.def.aquatic) cost += m.def.amphibious ? 0.5f : 4f;
                    if (Danger(w, nx, ny - 1, nz, m) || Danger(w, nx, ny, nz, m)) continue;
                    long nk = np.Pack();
                    float g = cur.g + cost;
                    if (nodes.TryGetValue(nk, out var ex))
                    {
                        if (ex.closed || ex.g <= g) continue;
                        ex.g = g; ex.f = g + H(np, goal); ex.parent = ck; nodes[nk] = ex;
                    }
                    else
                    {
                        nodes[nk] = new Node { pos = np, g = g, f = g + H(np, goal), parent = ck };
                        open.Add(nk);
                    }
                }
            }
            // reconstruct toward best node
            var result = new List<Int3>();
            long k2 = bestKey;
            int guard = 0;
            while (k2 != long.MinValue && guard++ < 2000) { var n = nodes[k2]; result.Add(n.pos); k2 = n.parent; }
            result.Reverse();
            if (result.Count > 0) result.RemoveAt(0);
            return result;
        }

        static float H(Int3 a, Int3 b) { int dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z; return Mathf.Sqrt(dx * dx + dy * dy * 2 + dz * dz); }

        static bool Solid(World w, int x, int y, int z)
        {
            var b = w.GetBlock(x, y, z);
            if (!b.solid) return false;
            if (b is DoorBlock && (w.GetMeta(new Int3(x, y, z)) & 4) != 0) return false;
            if (b.id == "snow" || b.id.EndsWith("_carpet") || b.id == "moss_carpet") return false;
            return true;
        }
        static bool Tall(World w, int x, int y, int z) { var b = w.GetBlock(x, y, z); return b is FenceBlock || b is WallBlock || b is FenceGateBlock; }
        static bool Water(World w, int x, int y, int z) => w.IsWater(new Int3(x, y, z));
        static bool Danger(World w, int x, int y, int z, Mob m)
        {
            var b = w.GetBlock(x, y, z);
            if (b.isAir) return false;
            if ((b is FireBlock || Blocks.IsLava(w.GetState(x, y, z)) || b.id == "magma_block" || b.id == "campfire") && !m.fireImmune) return true;
            if (b.id == "cactus" || b.id == "sweet_berry_bush" || b.id == "powder_snow" || b.id == "wither_rose" || b.id == "cobweb") return true;
            if (Blocks.IsLava(w.GetState(x, y, z)) && m.def.id != "strider") return true;
            return false;
        }
        static bool Free(World w, int x, int y, int z, int sw, int sh, Mob m)
        {
            if (y < w.minY || y + sh > w.maxY) return false;
            for (int i = 0; i < sw; i++)
                for (int j = 0; j < sw; j++)
                    for (int k = 0; k < sh; k++)
                        if (Solid(w, x + i, y + k, z + j)) return false;
            return true;
        }
        static bool Ground(World w, int x, int y, int z, int sw)
        {
            for (int i = 0; i < sw; i++)
                for (int j = 0; j < sw; j++)
                    if (Solid(w, x + i, y, z + j)) return true;
            return false;
        }
    }

    // =====================================================================================================
    // Standard goals
    public sealed class FloatGoal : Goal
    {
        public override bool CanUse() => mob.inWater && !mob.def.aquatic || mob.inLava && mob.def.id != "strider";
        public override void Tick() { if (UnityEngine.Random.value < 0.8f) mob.jumping = true; }
    }

    public sealed class PanicGoal : Goal
    {
        readonly float speed; Vector3 dest; int timer;
        public PanicGoal(float s) { speed = s; }
        public override bool CanUse() => (mob.age - mob.lastHurtByTick < 60 || mob.onFire) && !mob.tamed;
        public override bool CanContinue() => timer > 0 && !mob.nav.IsDone;
        public override void Start()
        {
            timer = 60;
            dest = RandomPos.Land(mob, 5, 4) ?? mob.position;
            mob.nav.MoveTo(dest, speed);
        }
        public override void Tick() { timer--; }
    }

    public sealed class StrollGoal : Goal
    {
        readonly float speed; readonly int interval; readonly bool avoidWater;
        public StrollGoal(float s, int interval = 120, bool avoidWater = true) { speed = s; this.interval = interval; this.avoidWater = avoidWater; }
        public override bool CanUse()
        {
            if (mob.sitting || mob.passengers.Count > 0 && mob.passengers[0] is Player) return false;
            if (UnityEngine.Random.Range(0, interval) != 0) return false;
            return true;
        }
        public override bool CanContinue() => !mob.nav.IsDone && !(mob.passengers.Count > 0 && mob.passengers[0] is Player);
        public override void Start()
        {
            Vector3? p = mob.def.flying ? RandomPos.Air(mob, 10, 7) : (mob.def.aquatic ? RandomPos.Water(mob, 10, 7) : RandomPos.Land(mob, 10, 7));
            if (p.HasValue) mob.nav.MoveTo(p.Value, speed);
        }
        public override void Stop() { mob.nav.Stop(); }
    }

    public sealed class LookAtPlayerGoal : Goal
    {
        readonly float range; Player target; int time;
        public LookAtPlayerGoal(float r) { range = r; }
        public override bool CanUse()
        {
            if (UnityEngine.Random.value > 0.02f) return false;
            target = mob.world.NearestPlayer(mob.position, range);
            return target != null && !target.IsSpectator;
        }
        public override bool CanContinue() => target != null && !target.removed && time > 0 && (target.position - mob.position).sqrMagnitude < range * range;
        public override void Start() { time = 40 + UnityEngine.Random.Range(0, 40); }
        public override void Tick() { time--; mob.LookAt(target); }
    }

    public sealed class RandomLookGoal : Goal
    {
        float rx, rz; int time;
        public override bool CanUse() => UnityEngine.Random.value < 0.02f;
        public override bool CanContinue() => time > 0;
        public override void Start() { float a = UnityEngine.Random.value * Mathf.PI * 2; rx = Mathf.Cos(a); rz = Mathf.Sin(a); time = 20 + UnityEngine.Random.Range(0, 20); }
        public override void Tick() { time--; mob.LookAt(mob.EyePosition + new Vector3(rx, 0, rz)); }
    }

    public sealed class TemptGoal : Goal
    {
        readonly float speed; Player p; int cooldown;
        public TemptGoal(float s) { speed = s; }
        public override bool CanUse()
        {
            if (cooldown > 0) { cooldown--; return false; }
            p = mob.world.NearestPlayer(mob.position, 10);
            return p != null && !p.IsSpectator && (mob.IsFood(p.inventory.Selected?.item) || mob.IsFood(p.inventory.offhand?.item));
        }
        public override bool CanContinue() => CanUse();
        public override void Tick()
        {
            mob.LookAt(p);
            if ((p.position - mob.position).sqrMagnitude < 6.25f) mob.nav.Stop();
            else if (mob.age % 10 == 0) mob.nav.MoveTo(p.position, speed);
        }
        public override void Stop() { mob.nav.Stop(); cooldown = 100; }
    }

    public sealed class BreedGoal : Goal
    {
        readonly float speed; Mob mate; int timer;
        public BreedGoal(float s) { speed = s; }
        public override bool CanUse() { if (mob.loveTicks <= 0) return false; mate = mob.FindMate(); return mate != null; }
        public override bool CanContinue() => mate != null && !mate.removed && mate.loveTicks > 0 && mob.loveTicks > 0 && timer < 60;
        public override void Start() { timer = 0; }
        public override void Tick()
        {
            mob.LookAt(mate);
            if (mob.age % 10 == 0) mob.nav.MoveTo(mate.position, speed);
            if ((mate.position - mob.position).sqrMagnitude < 9 && ++timer >= 60)
            {
                mob.Breed(mate);
                mob.loveTicks = 0; mate.loveTicks = 0; mob.breedCooldown = 6000; mate.breedCooldown = 6000;
                timer = 100;
            }
        }
    }

    public sealed class FollowParentGoal : Goal
    {
        readonly float speed; Mob parent; int t;
        public FollowParentGoal(float s) { speed = s; }
        public override bool CanUse()
        {
            if (!mob.IsBaby || UnityEngine.Random.value > 0.05f) return false;
            parent = mob.world.FindNearest<Mob>(mob.position, 8, m => m.def == mob.def && !m.IsBaby);
            return parent != null && (parent.position - mob.position).sqrMagnitude > 9;
        }
        public override bool CanContinue() => mob.IsBaby && parent != null && !parent.removed && (parent.position - mob.position).sqrMagnitude > 9 && (parent.position - mob.position).sqrMagnitude < 256;
        public override void Tick() { if (--t <= 0) { t = 10; mob.nav.MoveTo(parent.position, speed); } }
    }

    public sealed class MeleeAttackGoal : Goal
    {
        readonly float speed; readonly bool persistent; int repath;
        public MeleeAttackGoal(float s, bool followUnseen = true) { speed = s; persistent = followUnseen; }
        public override bool CanUse() => mob.target != null && mob.target.IsAlive;
        public override void Start() { repath = 0; mob.aggressiveAnim = true; }
        public override void Stop() { mob.nav.Stop(); mob.aggressiveAnim = false; }
        public override void Tick()
        {
            var t = mob.target; if (t == null) return;
            mob.LookAt(t);
            float d2 = (t.position - mob.position).sqrMagnitude;
            if (--repath <= 0)
            {
                repath = 4 + UnityEngine.Random.Range(0, 7);
                if (d2 > 1024) repath += 10; else if (d2 > 256) repath += 5;
                if (!mob.nav.MoveTo(t.position, speed)) repath += 15;
            }
            if (d2 <= mob.AttackReachSqr(t) && mob.attackCooldown <= 0 && Mathf.Abs(t.position.y - mob.position.y) < 2.5f)
            {
                mob.attackCooldown = 20;
                mob.DoMeleeAttack(t);
            }
        }
    }

    public sealed class AvoidEntityGoal<T> : Goal where T : Entity
    {
        readonly float dist, walk, sprint; readonly Func<T, bool> pred; T avoid;
        public AvoidEntityGoal(float dist, float walk, float sprint, Func<T, bool> pred = null) { this.dist = dist; this.walk = walk; this.sprint = sprint; this.pred = pred; }
        public override bool CanUse()
        {
            avoid = mob.world.FindNearest<T>(mob.position, dist, e => e != mob && (pred == null || pred(e)) && !(e is Player p && (p.IsCreative || p.IsSpectator)));
            if (avoid == null) return false;
            Vector3 away = mob.position - avoid.position; away.y = 0;
            var dest = RandomPos.Land(mob, 16, 7, away.normalized);
            if (!dest.HasValue) return false;
            mob.nav.MoveTo(dest.Value, walk);
            return true;
        }
        public override bool CanContinue() => !mob.nav.IsDone;
        public override void Tick()
        {
            if (avoid != null && (avoid.position - mob.position).sqrMagnitude < 49) mob.speedMod = sprint;
        }
    }

    public sealed class FleeSunGoal : Goal
    {
        readonly float speed;
        public FleeSunGoal(float s) { speed = s; }
        public override bool CanUse()
        {
            if (mob.target != null || !mob.onFire || mob.world.session == null || mob.world.session.IsNight) return false;
            if (!mob.world.CanSeeSky(Int3.Floor(mob.position))) return false;
            for (int i = 0; i < 10; i++)
            {
                var p = mob.position + new Vector3(UnityEngine.Random.Range(-10, 10), UnityEngine.Random.Range(-3, 3), UnityEngine.Random.Range(-10, 10));
                var bp = Int3.Floor(p);
                if (!mob.world.CanSeeSky(bp) && !mob.world.GetBlock(bp).solid && mob.world.GetBlock(bp.Offset(Dir.Down)).solid) { mob.nav.MoveTo(bp.Center, speed); return true; }
            }
            return false;
        }
        public override bool CanContinue() => !mob.nav.IsDone;
    }

    public sealed class HurtByTargetGoal : Goal
    {
        readonly bool alertOthers; int lastSeen;
        public HurtByTargetGoal(bool alert) { alertOthers = alert; }
        public override bool CanUse()
        {
            if (mob.lastHurtByTick == lastSeen) return false;
            var a = mob.lastAttacker;
            if (a == null || a.dead || mob.age - mob.lastHurtByTick > 5) return false;
            if (a is Player p && (p.IsCreative || p.IsSpectator)) return false;
            if (a is Mob am && am.def == mob.def) return false;
            return true;
        }
        public override bool CanContinue() => false;
        public override void Start()
        {
            lastSeen = mob.lastHurtByTick;
            mob.target = mob.lastAttacker;
            if (alertOthers)
                foreach (var e in mob.world.GetEntities(mob.Bounds.Grow(12f, 6f, 12f), mob))
                    if (e is Mob m && m.def == mob.def && m.target == null && !m.tamed) { m.target = mob.lastAttacker; m.angerTicks = 400; m.angryAtId = mob.lastAttacker.id; }
        }
    }

    public sealed class NearestTargetGoal<T> : Goal where T : LivingEntity
    {
        readonly bool sight; readonly int chance; readonly Func<T, bool> pred;
        public NearestTargetGoal(bool needSight = true, int chance = 10, Func<T, bool> pred = null) { sight = needSight; this.chance = chance; this.pred = pred; }
        public override bool CanUse()
        {
            if (mob.target != null) return false;
            if (chance > 0 && UnityEngine.Random.Range(0, chance) != 0) return false;
            float range = mob.def.followRange;
            T best = null; float bd = range * range;
            foreach (var e in mob.world.entities)
            {
                if (!(e is T t) || e == mob) continue;
                if (pred != null && !pred(t)) continue;
                if (!mob.CanTarget(t, range, sight)) continue;
                float d = (e.position - mob.position).sqrMagnitude;
                if (d < bd) { bd = d; best = t; }
            }
            if (best == null) return false;
            mob.target = best;
            return true;
        }
        public override bool CanContinue() => false;
    }

    public static class RandomPos
    {
        public static Vector3? Land(Mob m, int h, int v, Vector3? bias = null)
        {
            var w = m.world;
            for (int i = 0; i < 10; i++)
            {
                int dx = UnityEngine.Random.Range(-h, h + 1), dz = UnityEngine.Random.Range(-h, h + 1);
                if (bias.HasValue && Vector2.Dot(new Vector2(dx, dz), new Vector2(bias.Value.x, bias.Value.z)) < 0) continue;
                int x = Mathf.FloorToInt(m.position.x) + dx, z = Mathf.FloorToInt(m.position.z) + dz;
                int y0 = Mathf.FloorToInt(m.position.y);
                for (int dy = v; dy >= -v; dy--)
                {
                    int y = y0 + dy;
                    var p = new Int3(x, y, z);
                    var b = w.GetBlock(p); var below = w.GetBlock(p.Offset(Dir.Down)); var up = w.GetBlock(p.Offset(Dir.Up));
                    if (!b.solid && !up.solid && below.solid && !below.isLiquid && !(b.isLiquid && !m.def.amphibious) && !Blocks.IsLava(w.GetState(p)))
                        return new Vector3(x + 0.5f, y, z + 0.5f);
                }
            }
            return null;
        }
        public static Vector3? Air(Mob m, int h, int v)
        {
            for (int i = 0; i < 10; i++)
            {
                var p = m.position + new Vector3(UnityEngine.Random.Range(-h, h), UnityEngine.Random.Range(-v, v), UnityEngine.Random.Range(-h, h));
                var bp = Int3.Floor(p);
                if (!m.world.GetBlock(bp).solid && !m.world.GetBlock(bp.Offset(Dir.Up)).solid && bp.y > m.world.minY + 2) return p;
            }
            return null;
        }
        public static Vector3? Water(Mob m, int h, int v)
        {
            for (int i = 0; i < 10; i++)
            {
                var p = m.position + new Vector3(UnityEngine.Random.Range(-h, h), UnityEngine.Random.Range(-v, v), UnityEngine.Random.Range(-h, h));
                if (m.world.IsWater(Int3.Floor(p))) return p;
            }
            return null;
        }
    }
}
