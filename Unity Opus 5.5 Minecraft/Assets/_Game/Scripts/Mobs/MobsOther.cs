using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    // =====================================================================================================
    // Nether
    public sealed class GhastMob : Mob
    {
        public int charge; Vector3 wander; int wanderTimer;
        public bool Shooting => charge > 10;
        protected override void RegisterGoals()
        {
            targetGoals.Add(1, new NearestTargetGoal<Player>(false, 10, p => Mathf.Abs(p.position.y - position.y) < 20), GoalFlags.Target);
            targetGoals.Add(2, new HurtByTargetGoal(false), GoalFlags.Target);
        }
        public override void OnInitialSpawn(SpawnReason reason) { wander = position; }
        protected override void AiStep()
        {
            if (dead) { velocity.y -= 0.02f; Move(velocity); return; }
            targetGoals.Tick();
            if (--wanderTimer <= 0 || (wander - position).sqrMagnitude < 1 || horizontalCollision)
            {
                wanderTimer = 40 + Random.Range(0, 60);
                wander = position + new Vector3(Random.Range(-16f, 16f), Random.Range(-16f, 16f), Random.Range(-16f, 16f));
                wander.y = Mathf.Clamp(wander.y, world.minY + 8, world.maxY - 12);
                if (!world.HasLineOfSight(position, wander)) wander = position;
            }
            Vector3 d = wander - position;
            if (d.sqrMagnitude > 1) velocity += d.normalized * 0.01f;
            velocity *= 0.95f;
            Move(velocity);
            fallDistance = 0;
            if (target != null && (target.position - position).sqrMagnitude < 64 * 64 && world.HasLineOfSight(EyePosition, target.EyePosition))
            {
                Vector3 td = target.position - position;
                yaw = bodyYaw = lookYaw = MathX.YawFromDir(td);
                if (++charge == 10) Sounds.Play("entity.ghast.warn", position, 10f, 1f);
                if (charge >= 20)
                {
                    var fb = new LargeFireball { world = world, owner = this, explosionPower = 1 };
                    Vector3 look = MathX.YawPitchToDir(yaw, 0);
                    fb.SetPosition(position + Vector3.up * (height * 0.5f) + look * 4f);
                    fb.Aim(target.position + Vector3.up * target.height * 0.5f - fb.position);
                    world.AddEntity(fb);
                    Sounds.Play("entity.ghast.shoot", position, 10f, 1f);
                    charge = -40;
                }
            }
            else if (charge > 0) charge--;
            else if (velocity.sqrMagnitude > 1e-4f) yaw = bodyYaw = lookYaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(velocity), 5f);
        }
        public override string AmbientSound => "entity.ghast.ambient";
        public override void ApplyFallDamage(float dist) { }
    }

    public sealed class BlazeMob : Mob
    {
        int attackStep, attackTime; float heightOffset = 0.5f; int heightTimer;
        public bool Charged => attackStep > 0;
        protected override void RegisterGoals()
        {
            goals.Add(4, new BlazeAttackGoal(), GoalFlags.Move | GoalFlags.Look);
            goals.Add(7, new StrollGoal(1f), GoalFlags.Move);
            goals.Add(8, new LookAtPlayerGoal(8), GoalFlags.Look);
            targetGoals.Add(1, new HurtByTargetGoal(true), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<Player>(true, 10), GoalFlags.Target);
        }
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (!onGround && velocity.y < 0) velocity.y *= 0.6f;
            if (Random.value < 0.1f) Particles.LargeSmoke(world, position + new Vector3(Random.Range(-0.5f, 0.5f), Random.Range(0, height), Random.Range(-0.5f, 0.5f)));
            if (inWater || world.IsRainingAt(BlockPos)) { if (age % 10 == 0) Hurt(DamageSource.Drown, 1f); }
            if (--heightTimer <= 0) { heightTimer = 100; heightOffset = 0.5f + Random.Range(-1f, 1f) * 3f; }
            if (target != null && target.EyePosition.y > EyePosition.y + heightOffset) velocity.y += (0.3f - velocity.y) * 0.3f;
        }
        public override bool Hurt(DamageSource src, float amount)
        {
            if (src.direct is ThrownItemProjectile tp && tp.kind == "snowball") amount = 3;
            return base.Hurt(src, amount);
        }
        public override void ApplyFallDamage(float dist) { }
        public override string AmbientSound => "entity.blaze.ambient";

        sealed class BlazeAttackGoal : Goal
        {
            int step, timer, seen;
            public override bool CanUse() => mob.target != null && mob.target.IsAlive;
            public override void Start() { step = 0; }
            public override void Stop() { mob.aggressiveAnim = false; }
            public override void Tick()
            {
                var b = (BlazeMob)mob; var t = mob.target;
                bool see = mob.world.HasLineOfSight(mob.EyePosition, t.EyePosition);
                seen = see ? 0 : seen + 1;
                float d2 = (t.position - mob.position).sqrMagnitude;
                timer--;
                if (d2 < 4)
                {
                    if (!see) return;
                    if (timer <= 0) { timer = 20; mob.DoMeleeAttack(t); }
                    mob.nav.MoveTo(t.position, 1f);
                }
                else if (d2 < 48 * 48 && see)
                {
                    mob.LookAt(t);
                    if (timer <= 0)
                    {
                        step++;
                        if (step == 1) { timer = 60; b.attackStep = 1; mob.aggressiveAnim = true; }
                        else if (step <= 4) { timer = 6; }
                        else { timer = 100; step = 0; b.attackStep = 0; mob.aggressiveAnim = false; }
                        if (step > 1)
                        {
                            float spread = Mathf.Sqrt(Mathf.Sqrt(d2)) * 0.5f;
                            Sounds.Play("entity.blaze.shoot", mob.position, 1f, 1f);
                            var fb = new SmallFireball { world = mob.world, owner = mob };
                            fb.SetPosition(mob.position + Vector3.up * (mob.height * 0.5f + 0.5f));
                            Vector3 dir = t.position + Vector3.up * t.height * 0.5f - fb.position;
                            dir += new Vector3(Gauss() * spread, 0, Gauss() * spread) * 0.3f;
                            fb.Aim(dir);
                            mob.world.AddEntity(fb);
                        }
                    }
                    mob.nav.Stop();
                }
                else if (seen < 5 && mob.age % 10 == 0) mob.nav.MoveTo(t.position, 1f);
            }
            static float Gauss() => (Random.value + Random.value + Random.value - 1.5f);
        }
    }

    public sealed class PiglinMob : Mob
    {
        public int admiring; public bool dancing; int zombifyTimer;
        static bool WearsGold(Player p) { foreach (var a in p.inventory.armor) if (a != null && a.item is ArmorItem ai && ai.material == ArmorMaterial.Gold) return true; return false; }
        protected override void RegisterGoals()
        {
            goals.Add(0, new FloatGoal(), GoalFlags.Jump);
            goals.Add(2, new MeleeAttackGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(4, new PickupGoldGoal(), GoalFlags.Move);
            goals.Add(6, new StrollGoal(0.6f), GoalFlags.Move);
            goals.Add(7, new LookAtPlayerGoal(8), GoalFlags.Look);
            targetGoals.Add(1, new HurtByTargetGoal(true), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<Player>(true, 10, p => def.id == "piglin_brute" || !WearsGold(p)), GoalFlags.Target);
            targetGoals.Add(3, new NearestTargetGoal<Mob>(true, 20, m => m.def.id == "wither_skeleton" || m.def.id == "wither"), GoalFlags.Target);
        }
        public override void OnInitialSpawn(SpawnReason reason)
        {
            if (def.id == "piglin_brute") equipment[0] = new ItemStack("golden_axe", 1);
            else { equipment[0] = new ItemStack(Random.value < 0.5f ? "crossbow" : "golden_sword", 1); if (Random.value < 0.2f) SetBaby(true); }
            if (Random.value < 0.1f) equipment[5] = new ItemStack("golden_helmet", 1);
        }
        public override bool Interact(Player p, ItemStack held)
        {
            if (def.id == "piglin" && held != null && held.item.id == "gold_ingot" && admiring == 0 && !IsBaby)
            {
                if (!p.IsCreative) held.count--;
                StartAdmiring();
                return true;
            }
            return base.Interact(p, held);
        }
        public void StartAdmiring() { admiring = 120; equipment[1] = new ItemStack("gold_ingot", 1); nav.Stop(); target = null; Sounds.Play("entity.piglin.admiring_item", position, 1f, 1f); }
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (admiring > 0)
            {
                nav.Stop(); moveTarget = null;
                if (--admiring == 0)
                {
                    equipment[1] = null;
                    var rng = new RNG(Random.Range(0, int.MaxValue));
                    var s = Barter(ref rng);
                    world.SpawnItem(position + Vector3.up + LookDir * 0.5f, s, LookDir * 0.2f + Vector3.up * 0.1f);
                    Sounds.Play("entity.piglin.celebrate", position, 1f, 1f);
                }
            }
            if (world.dim == DimensionId.Overworld && ++zombifyTimer > 300)
            {
                var z = MobRegistry.Spawn(world, "zombified_piglin", position, SpawnReason.Conversion);
                if (z != null) { z.yaw = yaw; z.equipment[0] = equipment[0]; if (IsBaby) z.SetBaby(true); z.AddEffect(new EffectInstance(Effect.Nausea, 200)); }
                Remove();
            }
        }
        static ItemStack Barter(ref RNG r)
        {
            var table = new (string id, int w, int mn, int mx)[] { ("enchanted_book", 5, 1, 1), ("iron_boots", 8, 1, 1), ("potion", 8, 1, 1), ("splash_potion", 8, 1, 1), ("iron_nugget", 10, 10, 36), ("ender_pearl", 10, 2, 4), ("string", 20, 3, 9), ("quartz", 20, 5, 12), ("obsidian", 40, 1, 1), ("crying_obsidian", 40, 1, 3), ("fire_charge", 40, 1, 1), ("leather", 40, 2, 4), ("soul_sand", 40, 2, 8), ("nether_brick", 40, 2, 8), ("spectral_arrow", 40, 6, 12), ("gravel", 40, 8, 16), ("blackstone", 40, 8, 16) };
            int total = 0; foreach (var t in table) total += t.w;
            int pick = r.Next(total);
            foreach (var t in table)
            {
                pick -= t.w;
                if (pick >= 0) continue;
                if (t.id == "enchanted_book") return EnchantedBookItem.Make(Enchant.SoulSpeed, r.Range(1, 3));
                if (t.id == "potion" || t.id == "splash_potion") return Potions.Make(t.id, "fire_resistance");
                return new ItemStack(t.id, r.Range(t.mn, t.mx));
            }
            return new ItemStack("gravel", 8);
        }
        sealed class PickupGoldGoal : Goal
        {
            ItemEntity ie;
            public override bool CanUse()
            {
                var p = (PiglinMob)mob;
                if (p.admiring > 0 || p.def.id != "piglin" || p.IsBaby) return false;
                ie = mob.world.FindNearest<ItemEntity>(mob.position, 8, e => e.stack.item.id == "gold_ingot" && e.pickupDelay <= 0);
                return ie != null;
            }
            public override bool CanContinue() => ie != null && !ie.removed && ((PiglinMob)mob).admiring == 0;
            public override void Tick()
            {
                if (mob.age % 10 == 0) mob.nav.MoveTo(ie.position, 1f);
                if ((ie.position - mob.position).sqrMagnitude < 2) { ie.stack.count--; if (ie.stack.count <= 0) ie.Remove(); ((PiglinMob)mob).StartAdmiring(); }
            }
        }
    }

    public sealed class ZombifiedPiglinMob : Mob
    {
        protected override void RegisterGoals()
        {
            goals.Add(2, new MeleeAttackGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(7, new StrollGoal(1f), GoalFlags.Move);
            goals.Add(8, new LookAtPlayerGoal(8), GoalFlags.Look);
            targetGoals.Add(1, new HurtByTargetGoal(false), GoalFlags.Target);
        }
        public override void OnInitialSpawn(SpawnReason reason) { equipment[0] = new ItemStack("golden_sword", 1); if (Random.value < 0.05f) SetBaby(true); }
        protected override void OnHurtBy(LivingEntity attacker)
        {
            base.OnHurtBy(attacker);
            if (attacker is Player p && (p.IsCreative || p.IsSpectator)) return;
            int anger = 400 + Random.Range(0, 400);
            foreach (var e in world.GetEntities(Bounds.Grow(20f, 10f, 20f), this))
                if (e is ZombifiedPiglinMob z && !z.dead) { z.target = attacker; z.angerTicks = anger; z.angryAtId = attacker.id; }
            Sounds.Play("entity.zombified_piglin.angry", position, 2f, 1f);
        }
        public override string AmbientSound => angerTicks > 0 ? "entity.zombified_piglin.angry" : "entity.zombified_piglin.ambient";
    }

    // =====================================================================================================
    // End
    public sealed class ShulkerMob : Mob
    {
        public float peek, prevPeek; float peekTarget; int peekTimer, shootTimer = 60; public Dir attach = Dir.Down; public int color = -1;
        protected override void RegisterGoals()
        {
            targetGoals.Add(1, new HurtByTargetGoal(true), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<Player>(true, 10), GoalFlags.Target);
        }
        public override bool CanDespawn => false;
        public override bool IsSolidToOthers => true;
        public override int ArmorValue => peek < 0.05f ? 20 : 0;
        protected override void AiStep()
        {
            if (dead) return;
            targetGoals.Tick();
            velocity = Vector3.zero;
            position = new Vector3(Mathf.Floor(position.x) + 0.5f, Mathf.Floor(position.y + 0.01f), Mathf.Floor(position.z) + 0.5f);
            if (!world.GetBlock(BlockPos.Offset(attach)).solid && age > 5) TeleportRandom();
            if (target != null)
            {
                LookAt(target);
                peekTarget = 1f;
                if (--shootTimer <= 0 && world.HasLineOfSight(EyePosition, target.EyePosition) && (target.position - position).sqrMagnitude < 400)
                {
                    shootTimer = 20 + Random.Range(0, 90);
                    var b = new ShulkerBullet { world = world, owner = this, target = target };
                    b.SetPosition(position + Vector3.up * 0.5f + (target.position - position).normalized * 0.6f);
                    b.velocity = Vector3.up * 0.15f;
                    world.AddEntity(b);
                    Sounds.Play("entity.shulker.shoot", position, 2f, 1f);
                }
            }
            else if (--peekTimer <= 0) { peekTimer = 40 + Random.Range(0, 100); peekTarget = Random.value < 0.3f ? Random.Range(0.3f, 0.6f) : 0; }
        }
        public override void Tick()
        {
            prevPeek = peek;
            peek = Mathf.MoveTowards(peek, peekTarget, 0.05f);
            if (target == null && peek > 0 && peekTarget == 0 && Mathf.Approximately(peek, 0.05f)) Sounds.Play("entity.shulker.close", position, 1f, 1f);
            base.Tick();
            prevPosition = position;
        }
        public override bool Hurt(DamageSource src, float amount)
        {
            if (peek < 0.05f && src.isProjectile) return false;
            bool r = base.Hurt(src, amount);
            if (r && health < maxHealth * 0.5f && Random.value < 0.25f) TeleportRandom();
            return r;
        }
        void TeleportRandom()
        {
            for (int i = 0; i < 5; i++)
            {
                var p = BlockPos.Offset(Random.Range(-8, 9), Random.Range(-8, 9), Random.Range(-8, 9));
                if (world.IsAir(p) && world.GetBlock(p.Offset(Dir.Down)).opaqueCube)
                {
                    Sounds.Play("entity.shulker.teleport", position, 1f, 1f);
                    Teleport(new Vector3(p.x + 0.5f, p.y, p.z + 0.5f)); attach = Dir.Down; peek = prevPeek = 0; return;
                }
            }
        }
        public override string AmbientSound => "entity.shulker.ambient";
        public override void ApplyFallDamage(float dist) { }
        public override void Save(Dictionary<string, string> d) { base.Save(d); d["color"] = color.ToString(); }
        public override void Load(Dictionary<string, string> d) { base.Load(d); if (d.TryGetValue("color", out var c)) int.TryParse(c, out color); }
    }

    // =====================================================================================================
    // Flying
    /// <summary>Allay / vex / parrot style flyer.</summary>
    public class FlyerMob : Mob
    {
        Vector3 wander; int wanderTimer; public float flap;
        protected override void RegisterGoals()
        {
            if (def.id == "vex")
            {
                targetGoals.Add(1, new HurtByTargetGoal(false), GoalFlags.Target);
                targetGoals.Add(2, new NearestTargetGoal<Player>(false, 5), GoalFlags.Target);
            }
        }
        protected override void AiStep()
        {
            if (dead) { velocity.y -= 0.04f; Move(velocity); return; }
            targetGoals.Tick();
            flap += 0.5f;
            Vector3 goal;
            if (def.id == "vex" && target != null)
            {
                goal = target.position + Vector3.up * target.height * 0.5f;
                if ((goal - position).sqrMagnitude < 2 && attackCooldown <= 0) { attackCooldown = 20; DoMeleeAttack(target); }
                aggressiveAnim = true;
            }
            else if (def.id == "allay")
            {
                var p = world.NearestPlayer(position, 32);
                var held = equipment[0];
                ItemEntity want = held != null ? world.FindNearest<ItemEntity>(position, 16, e => e.stack.item == held.item) : null;
                if (want != null)
                {
                    goal = want.position + Vector3.up * 0.3f;
                    if ((want.position - position).sqrMagnitude < 1.5f) { carried = carried == null ? want.stack.Copy() : carried; if (carried != want.stack && carried.Stackable(want.stack)) carried.count = Mathf.Min(64, carried.count + want.stack.count); want.Remove(); Sounds.Play("entity.allay.item_given", position, 1f, 1f); }
                }
                else if (p != null && carried != null && (p.position - position).sqrMagnitude < 4) { world.SpawnItem(position, carried); carried = null; goal = p.EyePosition + Vector3.up; }
                else goal = p != null ? p.EyePosition + new Vector3(Mathf.Sin(age * 0.05f) * 2, 0.5f, Mathf.Cos(age * 0.05f) * 2) : Wander();
            }
            else goal = Wander();
            Vector3 d = goal - position;
            float spd = def.flySpeed > 0 ? def.flySpeed : 0.1f;
            if (d.sqrMagnitude > 0.2f) velocity += d.normalized * spd * 0.1f;
            velocity *= 0.9f;
            if (noPhysics) position += velocity; else Move(velocity);
            if (velocity.sqrMagnitude > 1e-4f) yaw = bodyYaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(velocity), 15f);
            if (target != null) LookAt(target);
            LookControl();
            fallDistance = 0;
        }
        public ItemStack carried;
        Vector3 Wander()
        {
            if (--wanderTimer <= 0 || (wander - position).sqrMagnitude < 1)
            {
                wanderTimer = 60 + Random.Range(0, 60);
                var p = RandomPos.Air(this, 8, 4);
                wander = p ?? position;
            }
            return wander;
        }
        public override void Tick()
        {
            base.Tick();
            if (def.id == "vex" && age > 600 && Random.value < 0.05f) Hurt(DamageSource.Magic, 1);
        }
        public override bool Interact(Player p, ItemStack held)
        {
            if (def.id == "allay")
            {
                if (equipment[0] == null && held != null) { equipment[0] = held.CopyWithCount(1); if (!p.IsCreative) held.count--; Sounds.Play("entity.allay.item_given", position, 1f, 1f); return true; }
                if (equipment[0] != null && held == null) { p.inventory.AddOrDrop(equipment[0]); equipment[0] = null; Sounds.Play("entity.allay.item_taken", position, 1f, 1f); return true; }
            }
            return base.Interact(p, held);
        }
        public override void ApplyFallDamage(float dist) { }
    }

    public sealed class BatMob : Mob
    {
        public bool resting; Vector3 goal; int t;
        protected override void AiStep()
        {
            if (dead) { velocity.y -= 0.04f; Move(velocity); return; }
            if (resting)
            {
                velocity = Vector3.zero;
                if (!world.GetBlock(Int3.Floor(position + Vector3.up * (height + 0.1f))).solid || world.NearestPlayer(position, 4) != null || Random.value < 0.001f) resting = false;
                return;
            }
            if (--t <= 0 || (goal - position).sqrMagnitude < 4) { t = 40; goal = position + new Vector3(Random.Range(-7, 7), Random.Range(-2, 5), Random.Range(-7, 7)); }
            Vector3 d = goal - position;
            velocity += new Vector3((Mathf.Sign(d.x) * 0.5f - velocity.x) * 0.1f, (Mathf.Sign(d.y) * 0.7f - velocity.y) * 0.1f, (Mathf.Sign(d.z) * 0.5f - velocity.z) * 0.1f);
            Move(velocity);
            yaw = bodyYaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(velocity), 20f);
            if (Random.value < 0.01f && world.GetBlock(Int3.Floor(position + Vector3.up * (height + 0.1f))).opaqueCube) { resting = true; velocity = Vector3.zero; }
        }
        public override void ApplyFallDamage(float dist) { }
        public override string AmbientSound => resting && Random.value < 0.9f ? null : "entity.bat.ambient";
    }

    public sealed class BeeMob : FlyerMob
    {
        public bool angry => angerTicks > 0; public bool stung; int stingDeath;
        protected override void RegisterGoals()
        {
            targetGoals.Add(1, new HurtByTargetGoal(true), GoalFlags.Target);
        }
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (target != null && !stung && (target.position - position).sqrMagnitude < 2 && attackCooldown <= 0)
            {
                attackCooldown = 20;
                if (target.Hurt(DamageSource.MobAttack(this), 2))
                {
                    stung = true;
                    var diff = world.session?.difficulty ?? Difficulty.Normal;
                    if (diff != Difficulty.Easy && diff != Difficulty.Peaceful) target.AddEffect(new EffectInstance(Effect.Poison, (diff == Difficulty.Hard ? 18 : 10) * 20));
                    target = null; angerTicks = 0;
                }
            }
            if (stung && ++stingDeath > 1200 + Random.Range(0, 200)) Hurt(DamageSource.Generic, health);
        }
    }

    public sealed class HappyGhastMob : Mob
    {
        public bool harnessed; public int growTime;
        public override bool CanDespawn => false;
        public override bool CanBeRiddenBy(Entity e) => harnessed && passengers.Count < 4 && e is Player && def.id == "happy_ghast";
        public override Vector3 PassengerOffset(Entity p) { int i = passengers.IndexOf(p); float[] ox = { 0, 1.2f, -1.2f, 0 }, oz = { 1f, 0, 0, -1f }; return new Vector3(ox[Mathf.Clamp(i, 0, 3)], height, oz[Mathf.Clamp(i, 0, 3)]); }
        public override bool Interact(Player p, ItemStack held)
        {
            if (def.id == "happy_ghast" && !harnessed && held != null && held.item.id == "harness") { harnessed = true; if (!p.IsCreative) held.count--; Sounds.Play("entity.happy_ghast.equip", position, 1f, 1f); visual?.Refresh(); return true; }
            if (def.id == "happy_ghast" && harnessed && p.vehicle == null && CanBeRiddenBy(p)) { p.StartRiding(this); return true; }
            if (held != null && held.item.id == "snowball") { if (!p.IsCreative) held.count--; growTime += 1200; Heal(2); Particles.HappyVillager(world, position + Vector3.up * height, 5); return true; }
            return base.Interact(p, held);
        }
        protected override void AiStep()
        {
            if (dead) return;
            if (passengers.Count > 0 && passengers[0] is Player rider)
            {
                yaw = bodyYaw = MathX.ApproachAngle(yaw, rider.yaw, 5f); lookYaw = yaw;
                Vector3 look = MathX.YawPitchToDir(rider.yaw, 0);
                Vector3 right = Quaternion.Euler(0, 90, 0) * look;
                Vector3 want = look * rider.moveForward + right * rider.moveStrafe * 0.6f + Vector3.up * (rider.jumping ? 0.6f : 0f) + Vector3.down * (rider.sneaking ? 0.5f : 0f);
                velocity += want * 0.03f;
                velocity *= 0.9f;
                Move(velocity);
                return;
            }
            // follow a player holding snowballs / wander slowly
            var p = world.NearestPlayer(position, 16);
            Vector3 goal = p != null && (p.inventory.Selected?.item.id == "snowball" || def.id == "ghastling") ? p.EyePosition + Vector3.up * (def.id == "ghastling" ? 1 : 3) : position + new Vector3(Mathf.Sin(age * 0.01f + id), Mathf.Sin(age * 0.013f) * 0.2f, Mathf.Cos(age * 0.01f + id));
            Vector3 d = goal - position;
            if (d.sqrMagnitude > 4) velocity += d.normalized * 0.01f;
            velocity *= 0.92f;
            Move(velocity);
            if (velocity.sqrMagnitude > 1e-4f) yaw = bodyYaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(velocity), 3f);
        }
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (def.id == "ghastling" && ++growTime >= 24000)
            {
                var h = MobRegistry.Spawn(world, "happy_ghast", position, SpawnReason.Conversion);
                if (h != null) { h.yaw = yaw; h.persistent = true; }
                Remove();
            }
        }
        public override void ApplyFallDamage(float dist) { }
        public override void Save(Dictionary<string, string> d) { base.Save(d); if (harnessed) d["harness"] = "1"; d["grow"] = growTime.ToString(); }
        public override void Load(Dictionary<string, string> d) { base.Load(d); harnessed = d.TryGetValue("harness", out var h) && h == "1"; if (d.TryGetValue("grow", out var g)) int.TryParse(g, out growTime); }
    }

    // =====================================================================================================
    // Water
    public class FishMob : Mob
    {
        int dryTime; int swimTimer; Vector3 swimTo;
        public float tailPhase;
        public override void OnInitialSpawn(SpawnReason reason) { if (def.id == "tropical_fish") variant = Random.Range(0, 12); }
        protected override void AiStep()
        {
            if (dead) { velocity.y -= 0.04f; Move(velocity); return; }
            tailPhase += inWater ? 0.4f : 1.2f;
            if (inWater)
            {
                dryTime = 0;
                if (--swimTimer <= 0 || (swimTo - position).sqrMagnitude < 1)
                {
                    swimTimer = 40 + Random.Range(0, 80);
                    var p = RandomPos.Water(this, 8, 3);
                    swimTo = p ?? position;
                    var player = world.NearestPlayer(position, 6, false);
                    if (player != null && def.category == MobCategory.WaterAmbient) swimTo = position + (position - player.position).normalized * 6;
                }
                Vector3 d = swimTo - position;
                float spd = def.id == "dolphin" ? 0.04f : def.category == MobCategory.WaterCreature ? 0.02f : 0.015f;
                if (d.sqrMagnitude > 0.2f) velocity += d.normalized * spd;
                velocity *= 0.9f;
                Move(velocity);
                if (velocity.sqrMagnitude > 1e-5f) { yaw = bodyYaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(velocity), 10f); lookPitch = Mathf.Clamp(MathX.PitchFromDir(velocity), -45, 45); }
            }
            else
            {
                // flop
                if (onGround && Random.value < 0.1f) { velocity = new Vector3(Random.Range(-0.1f, 0.1f), 0.4f, Random.Range(-0.1f, 0.1f)); Sounds.Play("entity." + def.soundId + ".flop", position, 1f, 1f); }
                velocity.y -= 0.08f; velocity.x *= 0.9f; velocity.z *= 0.9f;
                Move(velocity);
                if (def.aquatic && !def.amphibious && ++dryTime > 300 && age % 20 == 0) Hurt(DamageSource.Drown, 2f);
            }
        }
        public override string AmbientSound => null;
        public override bool Interact(Player p, ItemStack held) => base.Interact(p, held);
    }

    public sealed class PufferfishMob : FishMob
    {
        public int puff; int puffTimer;
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            bool near = false;
            foreach (var e in world.GetEntities(Bounds.Grow(2f), this))
                if ((e is Player p && !p.IsCreative && !p.IsSpectator) || (e is Mob m && !(m is PufferfishMob) && !m.def.aquatic)) { near = true; if (puff >= 2 && e is LivingEntity le && (e.position - position).sqrMagnitude < 2) { if (le.Hurt(DamageSource.MobAttack(this), 2 + puff)) le.AddEffect(new EffectInstance(Effect.Poison, 60 * puff)); } }
            if (near) { if (puff < 2 && ++puffTimer > 15) { puff++; puffTimer = 0; Sounds.Play("entity.puffer_fish.blow_up", position, 1f, 1f); } }
            else if (puff > 0 && ++puffTimer > 60) { puff--; puffTimer = 0; Sounds.Play("entity.puffer_fish.blow_out", position, 1f, 1f); }
            float s = puff == 0 ? 0.5f : puff == 1 ? 0.7f : 1f;
            width = height = 0.7f * s; sizeScale = s;
        }
    }

    public sealed class SquidMob : FishMob
    {
        public float tentacleAngle, prevTentacle, bodyPitch; float swimPhase;
        public override void Tick()
        {
            prevTentacle = tentacleAngle;
            base.Tick();
            if (dead || removed) return;
            swimPhase += 0.1f + Random.value * 0.05f;
            tentacleAngle = Mathf.Abs(Mathf.Sin(swimPhase)) * 60f;
            if (def.id == "glow_squid" && age % 20 == 0 && Random.value < 0.3f) Particles.Glow(world, position + Random.insideUnitSphere * 0.5f);
        }
        public override bool Hurt(DamageSource src, float amount)
        {
            bool r = base.Hurt(src, amount);
            if (r) { for (int i = 0; i < 30; i++) Particles.Smoke(world, position + Vector3.up * 0.5f + Random.insideUnitSphere * 0.5f, 1, 0.5f, true); velocity += Random.onUnitSphere * 0.4f; Sounds.Play("entity.squid.squirt", position, 1f, 1f); }
            return r;
        }
    }

    public sealed class DolphinMob : FishMob
    {
        int airLeft = 4800;
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (eyeInWater) { if (--airLeft <= 0 && age % 20 == 0) Hurt(DamageSource.Drown, 2f); if (airLeft < 1200) velocity.y += 0.02f; }
            else airLeft = 4800;
            foreach (var p in world.Players())
                if (p.inWater && (p.position - position).sqrMagnitude < 25 && p.sprinting) p.AddEffect(new EffectInstance(Effect.DolphinsGrace, 100, 0, true));
            if (inWater && Random.value < 0.02f && !eyeInWater == false) { velocity.y += 0.2f; }
        }
    }

    public sealed class GuardianMob : Mob
    {
        public int laserTime; public float spikes, prevSpikes, tail; int fatigueTimer;
        public bool Elder => def.id == "elder_guardian";
        protected override void RegisterGoals()
        {
            goals.Add(4, new GuardianAttackGoal(), GoalFlags.Move | GoalFlags.Look);
            goals.Add(7, new StrollGoal(1f, 80, false), GoalFlags.Move);
            targetGoals.Add(1, new NearestTargetGoal<LivingEntity>(true, 10, e => (e is Player || e is SquidMob || e is AxolotlMob) && e.inWater), GoalFlags.Target);
        }
        public override bool CanDespawn => !Elder && base.CanDespawn;
        public override void Tick()
        {
            prevSpikes = spikes;
            base.Tick();
            if (dead || removed) return;
            bool moving = velocity.sqrMagnitude > 0.001f;
            spikes = Mathf.MoveTowards(spikes, moving ? 0f : 1f, 0.06f);
            tail += moving ? 0.3f : 0.05f;
            if (!inWater) { if (onGround && Random.value < 0.05f) velocity = new Vector3(Random.Range(-0.2f, 0.2f), 0.5f, Random.Range(-0.2f, 0.2f)); }
            if (Elder && ++fatigueTimer >= 1200)
            {
                fatigueTimer = 0;
                foreach (var p in world.Players()) if ((p.position - position).sqrMagnitude < 2500 && !p.IsCreative && p.GetEffectLevel(Effect.MiningFatigue) < 2) { p.AddEffect(new EffectInstance(Effect.MiningFatigue, 6000, 2)); Sounds.Play("entity.elder_guardian.curse", p.position, 1f, 1f); GameManager.Instance?.ShowElderGuardianCurse(); }
            }
        }
        public override bool Hurt(DamageSource src, float amount)
        {
            bool r = base.Hurt(src, amount);
            if (r && spikes > 0.5f && src.direct is LivingEntity le && !src.isProjectile) le.Hurt(DamageSource.MobAttack(this), 2);
            return r;
        }
        protected override void CustomTravel()
        {
            if (moveTarget.HasValue) { var d = moveTarget.Value - position; if (d.sqrMagnitude > 0.2f) velocity += d.normalized * 0.02f; yaw = bodyYaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(d), 10f); }
            velocity *= 0.9f;
            Move(velocity);
        }
        sealed class GuardianAttackGoal : Goal
        {
            public override bool CanUse() => mob.target != null && mob.target.IsAlive && (mob.target.position - mob.position).sqrMagnitude > 9;
            public override void Start() { ((GuardianMob)mob).laserTime = -10; mob.nav.Stop(); }
            public override void Stop() { ((GuardianMob)mob).laserTime = 0; }
            public override void Tick()
            {
                var g = (GuardianMob)mob; var t = mob.target;
                mob.nav.Stop(); mob.LookAt(t);
                if (!mob.world.HasLineOfSight(mob.EyePosition, t.EyePosition)) { mob.target = null; return; }
                g.laserTime++;
                if (g.laserTime == 0) Sounds.Play("entity.guardian.attack", mob.position, 1f, 1f);
                int dur = g.Elder ? 60 : 80;
                if (g.laserTime >= dur)
                {
                    float dmg = g.Elder ? 5 : 6; var diff = mob.world.session?.difficulty ?? Difficulty.Normal; if (diff == Difficulty.Hard) dmg += 2;
                    t.Hurt(DamageSource.Magic, 1f);
                    t.Hurt(DamageSource.MobAttack(mob), dmg);
                    mob.target = null;
                }
            }
        }
    }
}
