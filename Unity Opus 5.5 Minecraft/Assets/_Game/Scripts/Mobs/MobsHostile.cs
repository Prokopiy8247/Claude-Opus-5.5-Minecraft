using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Generic hostile melee mob (silverfish, endermite, ravager, hoglin, zoglin ...).</summary>
    public class HostileMeleeMob : Mob
    {
        protected override void RegisterGoals()
        {
            goals.Add(0, new FloatGoal(), GoalFlags.Jump);
            goals.Add(2, new MeleeAttackGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(5, new StrollGoal(0.8f), GoalFlags.Move);
            goals.Add(6, new LookAtPlayerGoal(8), GoalFlags.Look);
            goals.Add(7, new RandomLookGoal(), GoalFlags.Look);
            targetGoals.Add(1, new HurtByTargetGoal(def.id == "hoglin"), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<Player>(true, 10), GoalFlags.Target);
            if (def.id == "zoglin") targetGoals.Add(3, new NearestTargetGoal<Mob>(true, 20, m => !(m is HostileMeleeMob hm && hm.def.id == "zoglin") && !m.def.hostile), GoalFlags.Target);
            if (def.id == "ravager") targetGoals.Add(3, new NearestTargetGoal<Mob>(true, 20, m => m is VillagerMob || m is IronGolemMob), GoalFlags.Target);
        }
        protected override void OnAttackHit(Entity t)
        {
            if (def.id == "hoglin" || def.id == "zoglin") { t.velocity.y += 0.45f; Sounds.Play("entity.hoglin.attack", position, 1f, 1f); }
            if (def.id == "ravager") { if (t is LivingEntity le) le.Knockback(1.2f, position.x - t.position.x, position.z - t.position.z); }
        }
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (def.id == "ravager" && horizontalCollision && (world.session == null || world.session.mobGriefing) && age % 5 == 0)
            {
                // trample leaves & crops in the way
                var ahead = Int3.Floor(position + LookDir * 1.5f + Vector3.up);
                for (int dy = 0; dy < 3; dy++) { var b = world.GetBlock(ahead.Offset(0, dy, 0)); if (b is LeavesBlock || b is CropBlock) world.BreakBlock(ahead.Offset(0, dy, 0), true, this); }
            }
            if (def.id == "silverfish" && target == null && age % 40 == 0 && Random.value < 0.1f)
            {
                // hide in stone
                var p = BlockPos.Offset(Random.Range(-1, 2), Random.Range(-1, 2), Random.Range(-1, 2));
                var b = world.GetBlock(p);
                var inf = Blocks.Get("infested_" + b.id);
                if (inf != null && (world.session == null || world.session.mobGriefing)) { world.SetState(p, inf.DefaultState); Particles.Poof(world, position, 0.5f, 0.5f); Remove(); }
            }
        }
    }

    // =====================================================================================================
    public sealed class ZombieMob : Mob
    {
        int tridentCooldown;
        public bool hasTrident;
        protected override void RegisterGoals()
        {
            goals.Add(0, new FloatGoal(), GoalFlags.Jump);
            goals.Add(2, new MeleeAttackGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(5, new StrollGoal(def.id == "drowned" ? 1f : 0.8f), GoalFlags.Move);
            goals.Add(6, new LookAtPlayerGoal(8), GoalFlags.Look);
            goals.Add(7, new RandomLookGoal(), GoalFlags.Look);
            targetGoals.Add(1, new HurtByTargetGoal(true), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<Player>(true, 10), GoalFlags.Target);
            targetGoals.Add(3, new NearestTargetGoal<Mob>(false, 10, m => m is VillagerMob || (m is IronGolemMob) || (m is TurtleMob && m.IsBaby)), GoalFlags.Target);
        }
        public override void OnInitialSpawn(SpawnReason reason)
        {
            if (reason == SpawnReason.Natural || reason == SpawnReason.SpawnEgg || reason == SpawnReason.Spawner)
            {
                if (Random.value < 0.05f) SetBaby(true);
                float r = Random.value;
                var diff = world.session?.difficulty ?? Difficulty.Normal;
                if (def.id == "drowned")
                {
                    if (r < 0.0625f) { equipment[0] = new ItemStack("trident", 1); hasTrident = true; dropChances[0] = 0.085f; }
                    else if (r < 0.1f) equipment[0] = new ItemStack("fishing_rod", 1);
                    if (Random.value < 0.03f) equipment[1] = new ItemStack("nautilus_shell", 1);
                }
                else if (r < (diff == Difficulty.Hard ? 0.05f : 0.01f)) equipment[0] = new ItemStack(Random.value < 0.33f ? "iron_sword" : "iron_shovel", 1);
                if (Random.value < 0.15f * (int)diff / 3f)
                {
                    string mat = Random.value < 0.4f ? "leather" : Random.value < 0.6f ? "golden" : Random.value < 0.8f ? "chainmail" : "iron";
                    equipment[5] = new ItemStack(mat + "_helmet", 1);
                    if (Random.value < 0.5f) equipment[4] = new ItemStack(mat + "_chestplate", 1);
                }
                if (world.session != null && world.session.dayTime % 24000 > 13000 && Random.value < 0.25f && def.id == "zombie" && Random.Range(0, 100) < 1) equipment[5] = new ItemStack("carved_pumpkin", 1);
            }
        }
        public override bool DoMeleeAttack(Entity t)
        {
            bool r = base.DoMeleeAttack(t);
            if (r && def.id == "husk" && t is LivingEntity le) le.AddEffect(new EffectInstance(Effect.Hunger, 140 * (int)(world.session?.difficulty ?? Difficulty.Normal)));
            if (r && onFire && Random.value < 0.3f * (int)(world.session?.difficulty ?? Difficulty.Normal)) t.SetOnFire(2 * (int)(world.session?.difficulty ?? Difficulty.Normal));
            return r;
        }
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (tridentCooldown > 0) tridentCooldown--;
            if (hasTrident && target != null && tridentCooldown == 0)
            {
                float d2 = (target.position - position).sqrMagnitude;
                if (d2 > 9 && d2 < 144 && world.HasLineOfSight(EyePosition, target.EyePosition))
                {
                    var tt = new ThrownTrident { world = world, owner = this, stack = new ItemStack("trident", 1) };
                    tt.SetPosition(EyePosition);
                    var dir = target.EyePosition - EyePosition; dir.y += Mathf.Sqrt(dir.x * dir.x + dir.z * dir.z) * 0.2f;
                    tt.Launch(dir, 1.6f, 14 - (int)(world.session?.difficulty ?? Difficulty.Normal) * 4);
                    world.AddEntity(tt);
                    Sounds.Play("entity.drowned.shoot", position, 1f, 1f);
                    tridentCooldown = 40; SwingArm();
                }
            }
            // husks convert to zombies underwater, zombies to drowned
            if ((def.id == "zombie" || def.id == "husk") && eyeInWater)
            {
                if (++waterTime > 600) { var c = MobRegistry.Spawn(world, def.id == "husk" ? "zombie" : "drowned", position, SpawnReason.Conversion); if (c != null) { c.yaw = yaw; c.persistent = persistent; if (IsBaby) c.SetBaby(true); } Remove(); }
            }
            else waterTime = 0;
        }
        int waterTime;
        protected override void CustomTravel()
        {
            // drowned swim toward targets
            if (moveTarget.HasValue) { var d = moveTarget.Value - position; velocity += d.normalized * 0.03f; yaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(d), 15f); }
            velocity *= 0.9f;
            if (!moveTarget.HasValue) velocity.y -= 0.005f;
            Move(velocity);
        }
        public override string AmbientSound => def.id == "husk" ? "entity.husk.ambient" : def.id == "drowned" ? "entity.drowned.ambient" : "entity.zombie.ambient";
    }

    // =====================================================================================================
    public sealed class SkeletonMob : Mob
    {
        public bool drawing; public int drawTicks;
        protected override void RegisterGoals()
        {
            goals.Add(0, new FloatGoal(), GoalFlags.Jump);
            goals.Add(1, new FleeSunGoal(1f), GoalFlags.Move);
            goals.Add(2, new AvoidEntityGoal<WolfMob>(6, 1f, 1.2f), GoalFlags.Move);
            if (def.id == "wither_skeleton") goals.Add(3, new MeleeAttackGoal(1.2f), GoalFlags.Move | GoalFlags.Look);
            else goals.Add(3, new BowAttackGoal(1f, 20, 15f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(5, new StrollGoal(1f), GoalFlags.Move);
            goals.Add(6, new LookAtPlayerGoal(8), GoalFlags.Look);
            goals.Add(7, new RandomLookGoal(), GoalFlags.Look);
            targetGoals.Add(1, new HurtByTargetGoal(false), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<Player>(true, 10), GoalFlags.Target);
            targetGoals.Add(3, new NearestTargetGoal<IronGolemMob>(true, 10), GoalFlags.Target);
            if (def.id == "wither_skeleton") targetGoals.Add(4, new NearestTargetGoal<Mob>(true, 10, m => m.def.id == "piglin" || m.def.id == "piglin_brute"), GoalFlags.Target);
        }
        public override void OnInitialSpawn(SpawnReason reason)
        {
            equipment[0] = new ItemStack(def.id == "wither_skeleton" ? "stone_sword" : "bow", 1);
            if (Random.value < 0.1f && def.id != "wither_skeleton") Loot.EnchantRandomly(equipment[0], ref world.rand, 5 + Random.Range(0, 18), false);
        }
        public override bool DoMeleeAttack(Entity t)
        {
            bool r = base.DoMeleeAttack(t);
            if (r && def.id == "wither_skeleton" && t is LivingEntity le) le.AddEffect(new EffectInstance(Effect.Wither, 200));
            return r;
        }
        public void ShootAt(LivingEntity t, float power)
        {
            var a = new Arrow { world = world, owner = this, ammoId = "arrow" };
            a.SetPosition(EyePosition - new Vector3(0, 0.1f, 0));
            Vector3 d = t.position + Vector3.up * t.height / 3f - a.position;
            float hd = Mathf.Sqrt(d.x * d.x + d.z * d.z);
            d.y += hd * 0.2f;
            int diff = (int)(world.session?.difficulty ?? Difficulty.Normal);
            a.Launch(d, 1.6f, 14 - diff * 4);
            a.damage = power * 2f + Random.Range(0f, 0.25f) + diff * 0.11f;
            if (def.id == "stray") a.potion = "slowness"; else if (def.id == "bogged") a.potion = "poison"; else if (def.id == "parched") a.potion = "weakness";
            var bow = equipment[0];
            if (bow != null) { int pw = bow.GetEnchant(Enchant.Power); if (pw > 0) a.damage += pw * 0.5f + 0.5f; a.punch = bow.GetEnchant(Enchant.Punch); if (bow.GetEnchant(Enchant.Flame) > 0) a.SetOnFire(100); }
            world.AddEntity(a);
            Sounds.Play("entity.skeleton.shoot", position, 1f, 1f / (Random.value * 0.4f + 0.8f));
        }
        public override string AmbientSound => "entity." + def.id + ".ambient";
    }

    public sealed class BowAttackGoal : Goal
    {
        readonly float speed; readonly int interval; readonly float range; int seeTime, cooldown = -1, strafeTime = -1; bool strafeLeft, strafeBack;
        public BowAttackGoal(float speed, int interval, float range) { this.speed = speed; this.interval = interval; this.range = range; }
        public override bool CanUse() => mob.target != null && mob.target.IsAlive && mob.equipment[0] != null && mob.equipment[0].item.id == "bow";
        public override void Start() { mob.aggressiveAnim = true; }
        public override void Stop() { mob.aggressiveAnim = false; seeTime = 0; cooldown = -1; if (mob is SkeletonMob s) { s.drawing = false; s.drawTicks = 0; } mob.moveStrafe = 0; }
        public override void Tick()
        {
            var t = mob.target; var s = mob as SkeletonMob;
            float d2 = (t.position - mob.position).sqrMagnitude;
            bool see = mob.world.HasLineOfSight(mob.EyePosition, t.EyePosition);
            if (see) seeTime++; else seeTime = 0;
            if (d2 <= range * range && seeTime >= 20) { mob.nav.Stop(); strafeTime++; }
            else { if (mob.age % 10 == 0) mob.nav.MoveTo(t.position, speed); strafeTime = -1; }
            if (strafeTime >= 20) { if (Random.value < 0.3f) strafeLeft = !strafeLeft; if (Random.value < 0.3f) strafeBack = !strafeBack; strafeTime = 0; }
            if (strafeTime > -1)
            {
                if (d2 > range * range * 0.75f) strafeBack = false; else if (d2 < range * range * 0.25f) strafeBack = true;
                mob.moveForward = strafeBack ? -0.5f : 0.5f; mob.moveStrafe = strafeLeft ? 0.5f : -0.5f;
                mob.yaw = MathX.YawFromDir(t.position - mob.position);
            }
            mob.LookAt(t);
            if (s != null)
            {
                if (s.drawing)
                {
                    if (!see && seeTime < -60) { s.drawing = false; }
                    else if (see && ++s.drawTicks >= 20) { s.drawing = false; s.ShootAt(t, BowItem.PowerFor(s.drawTicks)); s.drawTicks = 0; cooldown = interval; }
                }
                else if (--cooldown <= 0 && seeTime >= -60) { s.drawing = true; s.drawTicks = 0; }
            }
        }
    }

    // =====================================================================================================
    public sealed class CreeperMob : Mob
    {
        public bool charged; public int fuse; int swellDir; public bool ignited;
        public const int MaxFuse = 30;
        protected override void RegisterGoals()
        {
            goals.Add(1, new FloatGoal(), GoalFlags.Jump);
            goals.Add(2, new SwellGoal(), GoalFlags.Move);
            goals.Add(3, new AvoidEntityGoal<CatMob>(6, 1f, 1.2f), GoalFlags.Move);
            goals.Add(4, new MeleeAttackGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(5, new StrollGoal(0.8f), GoalFlags.Move);
            goals.Add(6, new LookAtPlayerGoal(8), GoalFlags.Look);
            goals.Add(6, new RandomLookGoal(), GoalFlags.Look);
            targetGoals.Add(1, new NearestTargetGoal<Player>(true, 10), GoalFlags.Target);
            targetGoals.Add(2, new HurtByTargetGoal(false), GoalFlags.Target);
        }
        public override bool DoMeleeAttack(Entity t) => false;
        public override string AmbientSound => null;
        public override void Tick()
        {
            if (!dead)
            {
                if (ignited) swellDir = 1;
                if (swellDir > 0 && fuse == 0) Sounds.Play("entity.creeper.primed", position, 1f, 0.5f);
                fuse = Mathf.Clamp(fuse + swellDir, 0, MaxFuse);
                swell = fuse / (float)MaxFuse;
                if (fuse >= MaxFuse) { Explode(); return; }
            }
            base.Tick();
        }
        public void SetSwell(int dir) { swellDir = dir; }
        void Explode()
        {
            dead = true;
            float power = charged ? 6f : 3f;
            Remove();
            Explosion.Explode(world, this, position, power, false, world.session == null || world.session.mobGriefing);
            if (effects.Count > 0)
            {
                var c = new AreaEffectCloud { world = world, radius = 2.5f, duration = 600, color = new Color32(120, 200, 120, 255), potion = new PotionType { id = "creeper", name = "creeper" } };
                foreach (var e in effects.Values) c.potion.effects.Add(e.Copy());
                c.SetPosition(position); world.AddEntity(c);
            }
        }
        public override bool Interact(Player p, ItemStack held)
        {
            if (held != null && (held.item.id == "flint_and_steel" || held.item.id == "fire_charge")) { ignited = true; Sounds.Play("item.flintandsteel.use", position, 1f, 1f); if (!p.IsCreative) { if (held.item.id == "flint_and_steel") held.HurtAndBreak(1, p); else held.count--; } return true; }
            return base.Interact(p, held);
        }
        public override void OnStruckByLightning(LightningBolt bolt) { base.OnStruckByLightning(bolt); charged = true; persistent = true; visual?.Refresh(); }
        protected override void DropExtra(List<ItemStack> drops, bool byPlayer, int looting, ref RNG rng)
        {
            if (lastAttacker is SkeletonMob) drops.Add(new ItemStack("music_disc_" + new[] { "13", "cat", "blocks", "chirp", "far", "mall", "mellohi", "stal", "strad", "ward", "11", "wait" }[rng.Next(12)], 1));
        }
        public override void Save(Dictionary<string, string> d) { base.Save(d); if (charged) d["charged"] = "1"; }
        public override void Load(Dictionary<string, string> d) { base.Load(d); charged = d.TryGetValue("charged", out var c) && c == "1"; }

        sealed class SwellGoal : Goal
        {
            public override bool CanUse() { var c = (CreeperMob)mob; return c.fuse > 0 || (mob.target != null && (mob.target.position - mob.position).sqrMagnitude < 9); }
            public override void Start() { mob.nav.Stop(); }
            public override void Stop() { ((CreeperMob)mob).SetSwell(-1); }
            public override void Tick()
            {
                var c = (CreeperMob)mob; var t = mob.target;
                if (t == null || (t.position - mob.position).sqrMagnitude > 49 || !mob.world.HasLineOfSight(mob.EyePosition, t.EyePosition)) c.SetSwell(-1);
                else { c.SetSwell(1); mob.LookAt(t); }
            }
        }
    }

    // =====================================================================================================
    public sealed class SpiderMob : Mob
    {
        int leapCooldown;
        protected override void RegisterGoals()
        {
            goals.Add(1, new FloatGoal(), GoalFlags.Jump);
            goals.Add(4, new MeleeAttackGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(5, new StrollGoal(0.8f), GoalFlags.Move);
            goals.Add(6, new LookAtPlayerGoal(8), GoalFlags.Look);
            goals.Add(6, new RandomLookGoal(), GoalFlags.Look);
            targetGoals.Add(1, new HurtByTargetGoal(false), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<Player>(true, 10, p => world.GetBrightness(position) < 0.5f || world.session == null || world.session.IsNight), GoalFlags.Target);
            targetGoals.Add(3, new NearestTargetGoal<IronGolemMob>(true, 10), GoalFlags.Target);
        }
        public override bool OnClimbable() => horizontalCollision || base.OnClimbable();
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (leapCooldown > 0) leapCooldown--;
            if (target != null && !(target is Player) == false)
            {
                // lose interest in bright light unless provoked
                if (world.GetBrightness(position) > 0.5f && world.session != null && !world.session.IsNight && angerTicks == 0 && Random.value < 0.01f) target = null;
            }
            if (target != null && onGround && leapCooldown == 0)
            {
                float d2 = (target.position - position).sqrMagnitude;
                if (d2 > 4 && d2 < 16 && Random.value < 0.2f)
                {
                    Vector3 d = (target.position - position); d.y = 0;
                    velocity += d.normalized * 0.4f + Vector3.up * 0.4f;
                    leapCooldown = 40;
                }
            }
        }
        public override bool DoMeleeAttack(Entity t)
        {
            bool r = base.DoMeleeAttack(t);
            if (r && def.id == "cave_spider" && t is LivingEntity le)
            {
                var diff = world.session?.difficulty ?? Difficulty.Normal;
                int sec = diff == Difficulty.Normal ? 7 : diff == Difficulty.Hard ? 15 : 0;
                if (sec > 0) le.AddEffect(new EffectInstance(Effect.Poison, sec * 20));
            }
            return r;
        }
        public override float AttackReachSqr(LivingEntity t) => 4f + t.width;
        public override void OnInitialSpawn(SpawnReason reason)
        {
            if (Random.value < 0.01f && reason == SpawnReason.Natural) { var j = MobRegistry.Spawn(world, "skeleton", position, SpawnReason.Reinforcement); if (j != null) j.StartRiding(this); }
        }
        public override Vector3 PassengerOffset(Entity p) => new Vector3(0, height * 0.75f, 0);
    }

    // =====================================================================================================
    public sealed class EndermanMob : Mob
    {
        public ushort carried; public bool screaming; int teleportCooldown; int stareTimer; int blockTimer;
        protected override void RegisterGoals()
        {
            goals.Add(0, new FloatGoal(), GoalFlags.Jump);
            goals.Add(2, new MeleeAttackGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(7, new StrollGoal(1f), GoalFlags.Move);
            goals.Add(8, new LookAtPlayerGoal(8), GoalFlags.Look);
            goals.Add(8, new RandomLookGoal(), GoalFlags.Look);
            targetGoals.Add(1, new HurtByTargetGoal(false), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<Mob>(false, 10, m => m.def.id == "endermite"), GoalFlags.Target);
        }
        public override string AmbientSound => screaming ? "entity.enderman.scream" : "entity.enderman.ambient";
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (teleportCooldown > 0) teleportCooldown--;
            screaming = target != null;
            aggressiveAnim = screaming;
            // stare aggro
            if (target == null && age % 5 == 0)
            {
                foreach (var p in world.Players())
                {
                    if (p.IsCreative || p.IsSpectator || p.dead) continue;
                    if ((p.position - position).sqrMagnitude > 64 * 64) continue;
                    var head = p.inventory.armor[3];
                    if (head != null && head.item.id == "carved_pumpkin") continue;
                    Vector3 toHead = EyePosition - p.EyePosition;
                    float dist = toHead.magnitude;
                    float dot = Vector3.Dot(p.LookDir, toHead / dist);
                    if (dot > 1f - 0.025f / dist && world.HasLineOfSight(p.EyePosition, EyePosition))
                    {
                        if (++stareTimer > 5) { target = p; angerTicks = 600; Sounds.Play("entity.enderman.stare", position, 2.5f, 1f); stareTimer = 0; }
                    }
                }
            }
            // water / rain damage
            if (inWater || world.IsRainingAt(BlockPos)) { if (age % 10 == 0) Hurt(DamageSource.Drown, 1f); if (teleportCooldown == 0) TeleportRandom(); }
            // daytime random teleport if no target
            if (target == null && world.session != null && !world.session.IsNight && world.CanSeeSky(BlockPos) && Random.value < 0.002f) TeleportRandom();
            // teleport toward target when far
            if (target != null && teleportCooldown == 0 && (target.position - position).sqrMagnitude > 256 && Random.value < 0.05f) TeleportTowards(target);
            // block carrying
            if (world.session == null || world.session.mobGriefing)
            {
                if (++blockTimer > 20 && Random.value < 0.05f)
                {
                    blockTimer = 0;
                    if (carried == 0)
                    {
                        var p = BlockPos.Offset(Random.Range(-2, 3), Random.Range(0, 3), Random.Range(-2, 3));
                        var b = world.GetBlock(p);
                        if (Holdable(b) && world.IsAir(p.Offset(Dir.Up))) { carried = b.DefaultState; world.SetState(p, 0); visual?.Refresh(); }
                    }
                    else if (Random.value < 0.1f)
                    {
                        var p = BlockPos.Offset(Random.Range(-1, 2), Random.Range(0, 2), Random.Range(-1, 2));
                        if (world.IsAir(p) && world.GetBlock(p.Offset(Dir.Down)).opaqueCube) { world.SetState(p, carried); carried = 0; visual?.Refresh(); }
                    }
                }
            }
        }
        static bool Holdable(Block b)
        {
            string id = b.id;
            return id == "grass_block" || id == "dirt" || id == "sand" || id == "red_sand" || id == "gravel" || id == "clay" || id == "pumpkin" || id == "melon" || id == "mycelium" || id == "podzol" || id == "netherrack" || id == "crimson_nylium" || id == "warped_nylium" || id == "dandelion" || id == "poppy" || id == "cactus" || id == "tnt" || id == "moss_block" || id == "mud" || id == "coarse_dirt" || id == "rooted_dirt";
        }
        public override bool Hurt(DamageSource src, float amount)
        {
            if (src.isProjectile && !(src.direct is ThrownPotion)) { TeleportRandom(); return false; }
            bool r = base.Hurt(src, amount);
            if (r && Random.value < 0.5f && !dead) TeleportRandom();
            return r;
        }
        public override void OnArrowDodge() => TeleportRandom();
        public bool TeleportRandom()
        {
            var p = position + new Vector3(Random.Range(-32f, 32f), Random.Range(-16, 16), Random.Range(-32f, 32f));
            return TeleportTo(p);
        }
        void TeleportTowards(Entity e)
        {
            Vector3 d = (position - e.position).normalized;
            TeleportTo(position + new Vector3(Random.Range(-4f, 4f), Random.Range(-4, 4), Random.Range(-4f, 4f)) - d * 16f);
        }
        bool TeleportTo(Vector3 p)
        {
            var bp = Int3.Floor(p);
            for (int i = 0; i < 24 && bp.y > world.minY; i++)
            {
                if (world.GetBlock(bp.Offset(Dir.Down)).solid && !world.GetBlock(bp).solid && !world.GetBlock(bp.Offset(Dir.Up)).solid && !world.GetBlock(bp.Offset(0, 2, 0)).solid && !world.IsWater(bp) && world.IsLoaded(bp))
                {
                    for (int k = 0; k < 16; k++) Particles.Portal(world, position + new Vector3(Random.Range(-0.5f, 0.5f), Random.Range(0, 2.9f), Random.Range(-0.5f, 0.5f)));
                    Sounds.Play("entity.enderman.teleport", position, 1f, 1f);
                    Teleport(new Vector3(bp.x + 0.5f, bp.y, bp.z + 0.5f));
                    Sounds.Play("entity.enderman.teleport", position, 1f, 1f);
                    teleportCooldown = 20; nav.Stop();
                    return true;
                }
                bp = bp.Offset(Dir.Down);
            }
            return false;
        }
        protected override void OnDeath(DamageSource src)
        {
            base.OnDeath(src);
            if (carried != 0) { var b = Blocks.ByState[carried]; if (b.item != null) world.SpawnItem(position, new ItemStack(b.item, 1)); }
        }
        public override void Save(Dictionary<string, string> d) { base.Save(d); if (carried != 0) d["carried"] = Blocks.ByState[carried].id; }
        public override void Load(Dictionary<string, string> d) { base.Load(d); if (d.TryGetValue("carried", out var c)) carried = Blocks.StateOf(c); }
    }

    // =====================================================================================================
    public sealed class SlimeMob : Mob
    {
        public int size = 1; int jumpDelay;
        public float squish, prevSquish, targetSquish;
        public ushort absorbed; // sulfur cube
        protected override void RegisterGoals()
        {
            targetGoals.Add(1, new NearestTargetGoal<Player>(true, 10), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<IronGolemMob>(true, 10), GoalFlags.Target);
            targetGoals.Add(3, new HurtByTargetGoal(false), GoalFlags.Target);
        }
        public override void OnInitialSpawn(SpawnReason reason)
        {
            int r = Random.Range(0, 3);
            if (r < 2 && Random.value < 0.5f) r++;
            SetSize(def.id == "sulfur_cube" ? Mathf.Min(2, 1 << Random.Range(0, 2)) : 1 << r);
        }
        public void SetSize(int s)
        {
            size = Mathf.Clamp(s, 1, 8);
            sizeScale = size;
            maxHealth = health = def.id == "sulfur_cube" ? size * 4 : size * size;
            UpdateSize();
            float scale = 0.52f * size;
            width = height = scale;
            armorValueBase = def.id == "magma_cube" ? size * 3 : 0;
            movementSpeed = 0.2f + 0.1f * size;
        }
        protected override void UpdateSize() { float s = 0.52f * size; width = height = s; }
        public override bool CanDespawn => base.CanDespawn;
        public override string HurtSound => size > 1 ? "entity." + def.id + ".hurt" : "entity." + def.id + ".hurt_small";
        public override string DeathSound => size > 1 ? "entity." + def.id + ".death" : "entity." + def.id + ".death_small";
        public override string AmbientSound => null;

        protected override void AiStep()
        {
            if (dead) { base.AiStep(); return; }
            targetGoals.Tick();
            if (absorbed != 0 && def.id == "sulfur_cube") { moveForward = 0; jumping = false; Travel(0, 0); return; }
            if (onGround)
            {
                if (--jumpDelay <= 0)
                {
                    jumpDelay = Random.Range(10, 30) / (target != null ? 3 : 1);
                    if (target != null) yaw = MathX.YawFromDir(target.position - position);
                    else if (Random.value < 0.3f) yaw += Random.Range(-90f, 90f);
                    velocity.y = def.id == "magma_cube" ? 0.42f + 0.1f * size : 0.42f;
                    moveForward = 1f;
                    targetSquish = 1f;
                    Sounds.Play(size > 1 ? "entity." + def.id + ".jump" : "entity." + def.id + ".jump_small", position, 0.4f * size, 1f);
                }
                else { moveForward = 0; }
            }
            movementSpeed = (0.2f + 0.1f * size) * (target != null ? 1f : 0.6f);
            Travel(0, moveForward);
            if (onGround && prevOnGround == false)
            {
                targetSquish = -0.5f;
                for (int i = 0; i < size * 8; i++) Particles.BlockDust(world, position, def.id == "magma_cube" ? Blocks.StateOf("magma_block") : Blocks.StateOf("slime_block"), 1);
                Sounds.Play("entity." + def.id + ".squish" + (size > 1 ? "" : "_small"), position, 0.4f * size, 1f);
            }
            prevOnGround = onGround;
        }
        bool prevOnGround;
        public override void Tick()
        {
            prevSquish = squish;
            squish += (targetSquish - squish) * 0.5f; targetSquish *= 0.6f;
            base.Tick();
            if (dead || removed) return;
            // damage on contact
            if (target != null && (size > 1 || def.id == "magma_cube") && attackCooldown <= 0)
            {
                float reach = 0.6f * size * 0.6f * size + target.width;
                if ((target.position - position).sqrMagnitude < reach && world.HasLineOfSight(EyePosition, target.EyePosition))
                {
                    attackCooldown = 10;
                    float dmg = def.id == "magma_cube" ? size * 1.5f + 1 : size;
                    if (target.Hurt(DamageSource.MobAttack(this), dmg)) Sounds.Play("entity.slime.attack", position, 1f, 1f);
                }
            }
        }
        protected override void OnDeath(DamageSource src)
        {
            base.OnDeath(src);
            if (size > 1 && absorbed == 0)
            {
                int n = Random.Range(2, 5);
                for (int i = 0; i < n; i++)
                {
                    var c = MobRegistry.Spawn(world, def.id, position + new Vector3((i % 2 - 0.5f) * size * 0.25f, 0.5f, (i / 2 - 0.5f) * size * 0.25f), SpawnReason.Conversion) as SlimeMob;
                    if (c != null) { c.SetSize(size / 2); c.yaw = Random.value * 360f; c.persistent = persistent; }
                }
            }
            if (absorbed != 0) { var b = Blocks.ByState[absorbed]; if (b.item != null) world.SpawnItem(position, new ItemStack(b.item, 1)); }
        }
        protected override int XpReward() => size;
        public override bool Interact(Player p, ItemStack held)
        {
            if (def.id == "sulfur_cube")
            {
                if (absorbed == 0 && held != null && held.item.block != null && held.item.block.opaqueCube)
                {
                    absorbed = held.item.block.DefaultState; if (!p.IsCreative) held.count--;
                    Sounds.Play("entity.slime.squish", position, 1f, 0.6f); visual?.Refresh();
                    var bb = Blocks.ByState[absorbed];
                    // absorbed block changes physical properties
                    knockbackResistance = bb.id.Contains("iron") || bb.id.Contains("stone") || bb.id.Contains("deepslate") ? 0.9f : 0.2f;
                    return true;
                }
                if (absorbed != 0 && held != null && held.item.id == "shears")
                {
                    var b = Blocks.ByState[absorbed]; if (b.item != null) world.SpawnItem(position + Vector3.up * height, new ItemStack(b.item, 1));
                    if (b is TntBlock) { MCR.Explosion.Explode(world, this, position, 2f, false, true); }
                    absorbed = 0; knockbackResistance = 0; held.HurtAndBreak(1, p); visual?.Refresh(); return true;
                }
                if (held != null && held.item.id == "sulfur_dust" && size < 4) { SetSize(size * 2); if (!p.IsCreative) held.count--; return true; }
            }
            return base.Interact(p, held);
        }
        public override float JumpPower => 0.42f;
        public override void ApplyFallDamage(float dist) { if (def.id != "sulfur_cube") return; base.ApplyFallDamage(dist - 3); }
        public override void Save(Dictionary<string, string> d) { base.Save(d); d["size"] = size.ToString(); if (absorbed != 0) d["absorbed"] = Blocks.ByState[absorbed].id; }
        public override void Load(Dictionary<string, string> d) { base.Load(d); if (d.TryGetValue("size", out var s) && int.TryParse(s, out int sz)) { float hp = health; SetSize(sz); health = hp; } if (d.TryGetValue("absorbed", out var a)) absorbed = Blocks.StateOf(a); }
    }

    // =====================================================================================================
    public sealed class WitchMob : Mob
    {
        int potionCooldown; int drinkTimer; string drinking;
        protected override void RegisterGoals()
        {
            goals.Add(1, new FloatGoal(), GoalFlags.Jump);
            goals.Add(2, new PotionAttackGoal(), GoalFlags.Move | GoalFlags.Look);
            goals.Add(3, new StrollGoal(1f), GoalFlags.Move);
            goals.Add(3, new LookAtPlayerGoal(8), GoalFlags.Look);
            targetGoals.Add(1, new HurtByTargetGoal(false), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<Player>(true, 10), GoalFlags.Target);
        }
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (potionCooldown > 0) potionCooldown--;
            if (drinkTimer > 0)
            {
                if (--drinkTimer == 0)
                {
                    var pt = Potions.Get(drinking);
                    if (pt != null) foreach (var e in pt.effects) AddEffect(e.Copy());
                    equipment[0] = null; drinking = null;
                }
                return;
            }
            string want = null;
            if (inWater && eyeInWater && !HasEffect(Effect.WaterBreathing)) want = "water_breathing";
            else if (onFire && !HasEffect(Effect.FireResistance)) want = "fire_resistance";
            else if (health < maxHealth && Random.value < 0.05f) want = "healing";
            else if (target != null && !HasEffect(Effect.Speed) && (target.position - position).sqrMagnitude > 121 && Random.value < 0.0005f) want = "swiftness";
            if (want != null) { drinking = want; drinkTimer = 32; equipment[0] = Potions.Make("potion", want); Sounds.Play("entity.witch.drink", position, 1f, 1f); }
        }
        public void Throw(LivingEntity t)
        {
            if (potionCooldown > 0 || drinkTimer > 0) return;
            potionCooldown = 60;
            float d = Vector3.Distance(t.position, position);
            string pot = "harming";
            if (d >= 8 && !t.HasEffect(Effect.Slowness)) pot = "slowness";
            else if (t.health >= 8 && !t.HasEffect(Effect.Poison)) pot = "poison";
            else if (d <= 3 && !t.HasEffect(Effect.Weakness) && Random.value < 0.25f) pot = "weakness";
            var tp = new ThrownPotion { world = world, owner = this, potionStack = Potions.Make("splash_potion", pot) };
            tp.SetPosition(EyePosition);
            Vector3 dir = t.position + t.velocity * 5 + Vector3.up * (t.EyeHeight - 1.1f) - tp.position;
            dir.y += Mathf.Sqrt(dir.x * dir.x + dir.z * dir.z) * 0.2f;
            tp.Launch(dir, 0.75f, 8f);
            world.AddEntity(tp);
            Sounds.Play("entity.witch.throw", position, 1f, 0.8f + Random.value * 0.4f);
        }
        sealed class PotionAttackGoal : Goal
        {
            public override bool CanUse() => mob.target != null && mob.target.IsAlive;
            public override void Tick()
            {
                var t = mob.target; mob.LookAt(t);
                float d2 = (t.position - mob.position).sqrMagnitude;
                bool see = mob.world.HasLineOfSight(mob.EyePosition, t.EyePosition);
                if (d2 > 100 || !see) { if (mob.age % 10 == 0) mob.nav.MoveTo(t.position, 1f); } else mob.nav.Stop();
                if (see && d2 < 100) ((WitchMob)mob).Throw(t);
            }
        }
    }

    // =====================================================================================================
    public sealed class PhantomMob : Mob
    {
        Vector3 anchor; float circleAngle; int swoopCooldown = 100; bool swooping;
        public override void OnInitialSpawn(SpawnReason reason) { anchor = position; }
        protected override void RegisterGoals()
        {
            targetGoals.Add(1, new NearestTargetGoal<Player>(false, 5), GoalFlags.Target);
            targetGoals.Add(2, new HurtByTargetGoal(false), GoalFlags.Target);
        }
        protected override void AiStep()
        {
            if (dead) { velocity.y -= 0.04f; Move(velocity); return; }
            targetGoals.Tick();
            if (target != null) anchor = target.position + Vector3.up * (swooping ? 0 : 20);
            if (--swoopCooldown <= 0 && target != null) { swooping = true; swoopCooldown = 200 + Random.Range(0, 200); }
            Vector3 goal;
            if (swooping && target != null)
            {
                goal = target.position + Vector3.up * target.height * 0.5f;
                if ((goal - position).sqrMagnitude < 2) { target.Hurt(DamageSource.MobAttack(this), 6); swooping = false; Sounds.Play("entity.phantom.bite", position, 1f, 1f); }
                if (horizontalCollision) swooping = false;
            }
            else
            {
                circleAngle += 0.05f;
                goal = anchor + new Vector3(Mathf.Cos(circleAngle) * 12, Mathf.Sin(circleAngle * 0.5f) * 2, Mathf.Sin(circleAngle) * 12);
            }
            Vector3 d = goal - position;
            velocity = Vector3.Lerp(velocity, d.normalized * (swooping ? 0.6f : 0.3f), 0.1f);
            yaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(velocity), 12f);
            lookPitch = MathX.PitchFromDir(velocity);
            bodyYaw = yaw;
            Move(velocity);
            fallDistance = 0;
        }
        public override string AmbientSound => "entity.phantom.ambient";
    }

    // =====================================================================================================
    public sealed class IllagerMob : Mob
    {
        int spellCooldown = 100, fangCooldown = 60; public int casting; int crossbowCharge;
        protected override void RegisterGoals()
        {
            goals.Add(0, new FloatGoal(), GoalFlags.Jump);
            if (def.id == "vindicator") goals.Add(2, new MeleeAttackGoal(1f), GoalFlags.Move | GoalFlags.Look);
            else if (def.id == "pillager") goals.Add(2, new CrossbowGoal(), GoalFlags.Move | GoalFlags.Look);
            else goals.Add(2, new AvoidEntityGoal<Player>(8, 0.6f, 1f, p => !p.IsCreative), GoalFlags.Move);
            goals.Add(8, new StrollGoal(0.6f), GoalFlags.Move);
            goals.Add(9, new LookAtPlayerGoal(15), GoalFlags.Look);
            targetGoals.Add(1, new HurtByTargetGoal(true), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<Player>(true, 10), GoalFlags.Target);
            targetGoals.Add(3, new NearestTargetGoal<Mob>(true, 10, m => m is VillagerMob || m is IronGolemMob), GoalFlags.Target);
        }
        public override void OnInitialSpawn(SpawnReason reason)
        {
            if (def.id == "vindicator") equipment[0] = new ItemStack("iron_axe", 1);
            else if (def.id == "pillager") equipment[0] = new ItemStack("crossbow", 1);
        }
        public override void Tick()
        {
            base.Tick();
            if (dead || removed || def.id != "evoker") return;
            if (casting > 0) casting--;
            if (target == null) return;
            if (--spellCooldown <= 0)
            {
                spellCooldown = 340; casting = 40;
                Sounds.Play("entity.evoker.prepare_summon", position, 1f, 1f);
                for (int i = 0; i < 3; i++)
                {
                    var v = MobRegistry.Spawn(world, "vex", position + new Vector3(Random.Range(-2f, 2f), 1, Random.Range(-2f, 2f)), SpawnReason.Summon);
                    if (v != null) { v.target = target; v.persistent = false; v.noPhysics = true; }
                }
            }
            else if (--fangCooldown <= 0)
            {
                fangCooldown = 100; casting = 20;
                Sounds.Play("entity.evoker.prepare_attack", position, 1f, 1f);
                Vector3 d = (target.position - position); d.y = 0; d.Normalize();
                for (int i = 1; i <= 16; i++)
                {
                    Vector3 p = position + d * (1.25f * i);
                    int delay = i;
                    world.session?.Defer(() => { });
                    Fang(p, delay);
                }
            }
        }
        void Fang(Vector3 p, int delay)
        {
            var bp = Int3.Floor(p);
            for (int k = 0; k < 4 && !world.GetBlock(bp.Offset(Dir.Down)).solid; k++) bp = bp.Offset(Dir.Down);
            var f = new EvokerFang { world = world, owner = this, warmup = delay };
            f.SetPosition(new Vector3(p.x, bp.y, p.z));
            world.AddEntity(f);
        }
        sealed class CrossbowGoal : Goal
        {
            int charge;
            public override bool CanUse() => mob.target != null && mob.target.IsAlive;
            public override void Start() { mob.aggressiveAnim = true; }
            public override void Stop() { mob.aggressiveAnim = false; charge = 0; }
            public override void Tick()
            {
                var t = mob.target; mob.LookAt(t);
                float d2 = (t.position - mob.position).sqrMagnitude;
                bool see = mob.world.HasLineOfSight(mob.EyePosition, t.EyePosition);
                if (d2 > 64 || !see) { if (mob.age % 10 == 0) mob.nav.MoveTo(t.position, 1f); } else mob.nav.Stop();
                if (see && ++charge >= 30)
                {
                    charge = 0;
                    var a = new Arrow { world = mob.world, owner = mob, ammoId = "arrow" };
                    a.SetPosition(mob.EyePosition - Vector3.up * 0.1f);
                    var dir = t.EyePosition - a.position; dir.y += Mathf.Sqrt(dir.x * dir.x + dir.z * dir.z) * 0.15f;
                    a.Launch(dir, 3.15f, 8f); a.damage = 3;
                    mob.world.AddEntity(a);
                    Sounds.Play("item.crossbow.shoot", mob.position, 1f, 1f);
                }
            }
        }
    }

    public sealed class EvokerFang : Entity
    {
        public Entity owner; public int warmup; int life = 22; bool bit;
        public override bool ShouldSave => false;
        public EvokerFang() { width = 0.5f; height = 0.8f; noPhysics = true; blocksBuilding = false; }
        public override void Tick()
        {
            age++; prevPosition = position;
            if (--warmup > 0) return;
            if (!bit)
            {
                bit = true;
                Sounds.Play("entity.evoker_fangs.attack", position, 1f, 1f);
                foreach (var e in world.GetEntities(Bounds.Grow(0.2f)))
                    if (e is LivingEntity le && le != owner && !(le is IllagerMob)) le.Hurt(DamageSource.Magic, 6f);
                for (int i = 0; i < 6; i++) Particles.Crit(world, position + Vector3.up * 0.4f, 1);
            }
            if (--life <= 0) Remove();
        }
        public override void CreateVisual() { go = ItemRender.CreateBlockVisual(Blocks.StateOf("pointed_dripstone"), 0.8f); }
        public override void Render(float partial)
        {
            if (go == null) return;
            float t = warmup > 0 ? 0 : Mathf.Clamp01(Mathf.Sin((22 - life + partial) / 22f * Mathf.PI));
            go.transform.position = position + Vector3.up * (t - 1f) * 0.8f;
            go.SetActive(warmup <= 0);
        }
    }

    // =====================================================================================================
    public sealed class BreezeMob : Mob
    {
        int jumpCooldown2 = 20, shootCooldown = 40;
        protected override void RegisterGoals()
        {
            goals.Add(0, new FloatGoal(), GoalFlags.Jump);
            goals.Add(6, new LookAtPlayerGoal(16), GoalFlags.Look);
            targetGoals.Add(1, new HurtByTargetGoal(false), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<Player>(true, 10), GoalFlags.Target);
        }
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (age % 3 == 0) Particles.Cloud(world, position + Vector3.up * 0.2f);
            if (target == null) return;
            LookAt(target);
            yaw = MathX.YawFromDir(target.position - position);
            if (onGround && --jumpCooldown2 <= 0)
            {
                jumpCooldown2 = Random.Range(30, 60);
                Vector3 d = target.position - position; d.y = 0;
                float dist = d.magnitude;
                Vector3 dir = dist > 8 ? d.normalized : (Random.value < 0.5f ? Quaternion.Euler(0, 90, 0) * d.normalized : -d.normalized);
                velocity += dir * 0.6f + Vector3.up * 0.9f;
                Sounds.Play("entity.breeze.jump", position, 1f, 1f);
            }
            if (--shootCooldown <= 0 && world.HasLineOfSight(EyePosition, target.EyePosition) && (target.position - position).sqrMagnitude < 400)
            {
                shootCooldown = 40;
                var w = new WindChargeProjectile { world = world, owner = this, breeze = true };
                w.SetPosition(EyePosition);
                w.Launch(target.EyePosition - EyePosition, 0.7f, 5f);
                world.AddEntity(w);
                Sounds.Play("entity.breeze.shoot", position, 1f, 1f);
            }
        }
        public override bool Hurt(DamageSource src, float amount)
        {
            if (src.isProjectile && src.direct is Arrow a) { a.velocity = -a.velocity * 0.5f; return false; }
            return base.Hurt(src, amount);
        }
        public override void ApplyFallDamage(float dist) { }
    }

    // =====================================================================================================
    public sealed class CreakingMob : Mob
    {
        public Int3? heart; public bool frozen; int twitch;
        protected override void RegisterGoals()
        {
            goals.Add(2, new MeleeAttackGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(8, new StrollGoal(0.7f), GoalFlags.Move);
            goals.Add(8, new LookAtPlayerGoal(8), GoalFlags.Look);
            targetGoals.Add(1, new NearestTargetGoal<Player>(false, 5), GoalFlags.Target);
        }
        public override bool CanDespawn => heart == null && base.CanDespawn;
        public override void Tick()
        {
            frozen = false;
            foreach (var p in world.Players())
            {
                if (p.IsSpectator || p.dead) continue;
                if ((p.position - position).sqrMagnitude > 24 * 24) continue;
                var head = p.inventory.armor[3];
                if (head != null && head.item.id == "carved_pumpkin") continue;
                Vector3 to = (position + Vector3.up * height * 0.5f) - p.EyePosition;
                if (Vector3.Dot(p.LookDir, to.normalized) > 0.6f && world.HasLineOfSight(p.EyePosition, EyePosition)) { frozen = true; break; }
            }
            if (frozen)
            {
                prevPosition = position; prevYaw = yaw;
                velocity.x = 0; velocity.z = 0;
                nav.Stop(); moveTarget = null;
                if (++twitch % 30 == 0) Sounds.Play("entity.creaking.freeze", position, 0.5f, 1f);
                velocity.y -= 0.08f; Move(new Vector3(0, velocity.y, 0));
                age++;
                if (hurtTime > 0) hurtTime--;
                return;
            }
            base.Tick();
            if (heart.HasValue && !(world.GetBlock(heart.Value).id == "creaking_heart") && world.IsLoaded(heart.Value)) { Die(DamageSource.Kill); }
        }
        public override bool Hurt(DamageSource src, float amount)
        {
            if (heart.HasValue && src != DamageSource.Kill && src != DamageSource.Void)
            {
                // invulnerable while linked: particle trail toward the heart
                Sounds.Play("entity.creaking.sway", position, 1f, 1f);
                var h = heart.Value.Center; var from = position + Vector3.up * height * 0.5f;
                for (int i = 0; i < 12; i++) Particles.Ash(world, Vector3.Lerp(from, h, i / 12f), new Color32(255, 140, 40, 255));
                hurtTime = 10;
                return false;
            }
            return base.Hurt(src, amount);
        }
        public override string AmbientSound => frozen ? null : "entity.creaking.ambient";
    }

    // =====================================================================================================
    public sealed class WardenMob : Mob
    {
        public int emerging = 134, digging; public int anger; int sonicCooldown = 60, darknessTimer, sniffTimer, idleTimer; public int sonicCharge;
        public float tendril, prevTendril; public int heartbeat;
        protected override void RegisterGoals()
        {
            goals.Add(1, new MeleeAttackGoal(1.2f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(8, new StrollGoal(0.5f, 200), GoalFlags.Move);
        }
        public override bool CanDespawn => false;
        public override float AttackReachSqr(LivingEntity t) => 4f + t.width;
        public override void OnInitialSpawn(SpawnReason reason) { emerging = reason == SpawnReason.SpawnEgg ? 0 : 134; persistent = true; if (emerging > 0) Sounds.Play("entity.warden.emerge", position, 4f, 1f); }
        public override void Tick()
        {
            prevTendril = tendril;
            if (emerging > 0)
            {
                emerging--; age++; prevPosition = position;
                if (emerging % 5 == 0) Particles.BlockDust(world, position, world.GetState(Int3.Floor(position - Vector3.up * 0.2f)), 6);
                return;
            }
            if (digging > 0)
            {
                digging--; age++; prevPosition = position;
                if (digging % 5 == 0) Particles.BlockDust(world, position, world.GetState(Int3.Floor(position - Vector3.up * 0.2f)), 6);
                if (digging == 0) Remove();
                return;
            }
            base.Tick();
            if (dead || removed) return;
            tendril *= 0.8f;
            if (++heartbeat >= (anger > 60 ? 20 : 40)) { heartbeat = 0; Sounds.Play("entity.warden.heartbeat", position, 1f, 1f); }
            // listen for vibrations: moving, non-sneaking players nearby
            foreach (var p in world.Players())
            {
                if (p.IsCreative || p.IsSpectator || p.dead) continue;
                float d2 = (p.position - position).sqrMagnitude;
                if (d2 > 16 * 16) continue;
                bool moving = (p.position - p.prevPosition).sqrMagnitude > 0.0004f && !p.sneaking;
                if (moving || p.swingTime > 0) { anger = Mathf.Min(150, anger + (d2 < 36 ? 10 : 3)); tendril = 1f; if (anger > 80) target = p; }
            }
            if (target == null && --sniffTimer <= 0) { sniffTimer = 120; Sounds.Play("entity.warden.sniff", position, 1f, 1f); var p = world.NearestPlayer(position, 16, false); if (p != null) { anger += 35; if (anger > 80) target = p; } }
            if (anger > 0 && age % 20 == 0) anger--;
            if (target == null) { if (++idleTimer > 1200) { digging = 100; Sounds.Play("entity.warden.dig", position, 4f, 1f); } } else idleTimer = 0;
            // darkness pulse
            if (++darknessTimer >= 120)
            {
                darknessTimer = 0;
                foreach (var p in world.Players()) if ((p.position - position).sqrMagnitude < 400 && !p.IsCreative) p.AddEffect(new EffectInstance(Effect.Darkness, 260, 0, true));
            }
            // sonic boom when target out of melee reach
            if (target != null)
            {
                float d2 = (target.position - position).sqrMagnitude;
                if (sonicCharge > 0)
                {
                    if (--sonicCharge == 0)
                    {
                        Vector3 from = EyePosition, to = target.EyePosition;
                        for (int i = 1; i < 20; i++) Particles.SonicBoom(world, Vector3.Lerp(from, to, i / 20f));
                        Sounds.Play("entity.warden.sonic_boom", position, 3f, 1f);
                        target.Hurt(DamageSource.Sonic, 10f);
                        Vector3 d = (to - from).normalized;
                        target.velocity += new Vector3(d.x * 2.5f, 0.5f, d.z * 2.5f);
                        sonicCooldown = 40;
                    }
                }
                else if (--sonicCooldown <= 0 && d2 > 9 && d2 < 400) { sonicCharge = 34; Sounds.Play("entity.warden.sonic_charge", position, 3f, 1f); }
            }
        }
        protected override void AiStep()
        {
            if (emerging > 0 || digging > 0 || sonicCharge > 0) { moveForward = 0; velocity.x = velocity.z = 0; if (target != null) LookAt(target); LookControl(); base.AiStepRaw(); return; }
            base.AiStep();
        }
        public override bool Hurt(DamageSource src, float amount)
        {
            if (emerging > 0 || digging > 0) return false;
            bool r = base.Hurt(src, amount);
            if (r && src.attacker is LivingEntity le) { anger = 150; target = le; }
            return r;
        }
        public override string AmbientSound => anger > 80 ? "entity.warden.angry" : "entity.warden.ambient";
        public override void ApplyFallDamage(float dist) { }
    }

    /// <summary>Sculk shrieker warnings and warden summoning.</summary>
    public static class Warden
    {
        static int warningLevel; static long lastWarningTick = -100000; static long lastShriek = -1000;
        public static void Shriek(World w, Int3 pos, Player p)
        {
            if (w.tickCount - lastShriek < 200) return;
            lastShriek = w.tickCount;
            if (w.tickCount - lastWarningTick > 12000) warningLevel = 0;
            warningLevel++; lastWarningTick = w.tickCount;
            Sounds.Play("block.sculk_shrieker.shriek", pos.Center, 2f, 1f);
            p.AddEffect(new EffectInstance(Effect.Darkness, 260, 0, true));
            for (int i = 0; i < 6; i++) Particles.SonicBoom(w, pos.Center + Vector3.up * (0.5f + i * 0.4f));
            if (warningLevel >= 4)
            {
                warningLevel = 0;
                if (w.session != null && w.session.difficulty == Difficulty.Peaceful) return;
                if (w.FindNearest<WardenMob>(pos.Center, 48) != null) return;
                for (int i = 0; i < 20; i++)
                {
                    var sp = pos.Offset(Random.Range(-5, 6), Random.Range(-3, 4), Random.Range(-5, 6));
                    if (w.GetBlock(sp.Offset(Dir.Down)).solid && !w.GetBlock(sp).solid && !w.GetBlock(sp.Offset(Dir.Up)).solid && !w.GetBlock(sp.Offset(0, 2, 0)).solid)
                    {
                        var wm = MobRegistry.Spawn(w, "warden", new Vector3(sp.x + 0.5f, sp.y, sp.z + 0.5f), SpawnReason.Summon) as WardenMob;
                        if (wm != null) { wm.target = p; wm.anger = 100; }
                        return;
                    }
                }
            }
            else GameManager.Instance?.hud?.ShowActionBar("A warning shriek echoes... (" + warningLevel + "/4)");
        }
    }
}
