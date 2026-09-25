using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Generic land animal: panic, breed, tempt, follow parent, stroll, look around.</summary>
    public class AnimalMob : Mob
    {
        protected override void RegisterGoals()
        {
            goals.Add(0, new FloatGoal(), GoalFlags.Jump);
            goals.Add(1, new PanicGoal(1.25f), GoalFlags.Move);
            goals.Add(2, new BreedGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(3, new TemptGoal(1.2f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(4, new FollowParentGoal(1.1f), GoalFlags.Move);
            goals.Add(5, new StrollGoal(1f), GoalFlags.Move);
            goals.Add(6, new LookAtPlayerGoal(6f), GoalFlags.Look);
            goals.Add(7, new RandomLookGoal(), GoalFlags.Look);
        }

        public override bool CanBeRiddenBy(Entity e) => saddled && passengers.Count == 0 && e is Player;
        public override Vector3 PassengerOffset(Entity p) => new Vector3(0, height * 0.75f + (def.id == "pig" ? 0.1f : 0), 0);

        public override bool Interact(Player p, ItemStack held)
        {
            if (def.id == "pig" || def.id == "strider")
            {
                if (!saddled && held != null && held.item.id == "saddle" && !IsBaby) { saddled = true; if (!p.IsCreative) held.count--; Sounds.Play("entity.pig.saddle", position, 1f, 1f); return true; }
                if (saddled && passengers.Count == 0 && !p.sneaking && (held == null || !IsFood(held.item))) { p.StartRiding(this); return true; }
            }
            if (def.id == "mooshroom" && held != null && held.item.id == "bowl" && !IsBaby) { if (!p.IsCreative) held.count--; p.inventory.AddOrDrop(new ItemStack("mushroom_stew", 1)); Sounds.Play("entity.mooshroom.milk", position, 1f, 1f); return true; }
            if (def.id == "mooshroom" && held != null && held.item.id == "shears" && !IsBaby)
            {
                var cow = MobRegistry.Spawn(world, "cow", position, SpawnReason.Conversion);
                if (cow != null) { cow.yaw = yaw; cow.health = health; }
                world.SpawnItem(position + Vector3.up * height, new ItemStack(variant == 1 ? "brown_mushroom" : "red_mushroom", 5));
                held.HurtAndBreak(1, p); Remove(); return true;
            }
            if (def.id == "armadillo" && held != null && held.item.id == "brush") { world.SpawnItem(position, new ItemStack("armadillo_scute", 1)); held.HurtAndBreak(16, p); return true; }
            return base.Interact(p, held);
        }

        protected override void AiStep()
        {
            if (passengers.Count > 0 && passengers[0] is Player rider && saddled)
            {
                var held = rider.inventory.Selected;
                bool steer = def.id == "pig" ? held != null && held.item.id == "carrot_on_a_stick" : def.id == "strider" ? held != null && held.item.id == "warped_fungus_on_a_stick" : true;
                if (steer)
                {
                    yaw = rider.yaw; bodyYaw = yaw; lookYaw = yaw;
                    moveForward = 1f; moveStrafe = 0;
                    movementSpeed = def.speed * (def.id == "pig" ? 1.15f : 1.5f);
                    goals.StopAll(); nav.Stop();
                    base.AiStepRaw();
                    return;
                }
            }
            base.AiStep();
        }
    }

    public abstract partial class Mob
    {
        /// <summary>LivingEntity movement without running AI (used while ridden).</summary>
        protected void AiStepRaw()
        {
            if (jumping && jumpCooldown == 0) { if (inWater || inLava) velocity.y += 0.04f; else if (onGround) { Jump(); jumpCooldown = 10; } }
            Travel(moveStrafe, moveForward);
        }
    }

    // =====================================================================================================
    public sealed class SheepMob : AnimalMob
    {
        public bool sheared;
        int eatTimer;
        public string ColorName => Blocks.Colors[Mathf.Clamp(variant, 0, 15)];
        static readonly (int color, int weight)[] natural = { (0, 818), (7, 50), (8, 50), (15, 50), (12, 30), (6, 2) };

        public override void OnInitialSpawn(SpawnReason reason)
        {
            base.OnInitialSpawn(reason);
            int r = Random.Range(0, 1000), acc = 0;
            foreach (var (c, w) in natural) { acc += w; if (r < acc) { variant = ColorIndex(c); break; } }
        }
        static int ColorIndex(int mcIndex)
        {
            string[] mc = { "white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray", "light_gray", "cyan", "purple", "blue", "brown", "green", "red", "black" };
            return System.Array.IndexOf(Blocks.Colors, mc[mcIndex]);
        }

        public void Shear()
        {
            sheared = true;
            int n = Random.Range(1, 4);
            for (int i = 0; i < n; i++) world.SpawnItem(position + Vector3.up, new ItemStack(ColorName + "_wool", 1), new Vector3(Random.Range(-0.1f, 0.1f), 0.2f, Random.Range(-0.1f, 0.1f)));
            Sounds.Play("entity.sheep.shear", position, 1f, 1f);
            visual?.Refresh();
        }

        public override bool Interact(Player p, ItemStack held)
        {
            if (held != null && held.item.id == "shears" && !sheared && !IsBaby) { Shear(); held.HurtAndBreak(1, p); p.SwingArm(); return true; }
            if (held != null && held.item.id.EndsWith("_dye") && !sheared)
            {
                string col = held.item.id.Substring(0, held.item.id.Length - 4);
                int idx = System.Array.IndexOf(Blocks.Colors, col);
                if (idx >= 0 && idx != variant) { variant = idx; if (!p.IsCreative) held.count--; visual?.Refresh(); return true; }
            }
            return base.Interact(p, held);
        }

        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (eatTimer > 0)
            {
                eatTimer--;
                if (eatTimer == 4)
                {
                    Int3 below = Int3.Floor(position - Vector3.up * 0.5f);
                    if (world.GetBlock(below).id == "grass_block" && (world.session == null || world.session.mobGriefing)) { world.SetState(below, Blocks.StateOf("dirt")); OnAteGrass(); }
                    else if (world.GetBlock(Int3.Floor(position)).id == "short_grass") { world.BreakBlock(Int3.Floor(position), false, this); OnAteGrass(); }
                }
            }
            else if (Random.Range(0, IsBaby ? 50 : 1000) == 0 && onGround)
            {
                Int3 below = Int3.Floor(position - Vector3.up * 0.5f);
                if (world.GetBlock(below).id == "grass_block" || world.GetBlock(Int3.Floor(position)).id == "short_grass") { eatTimer = 40; nav.Stop(); }
            }
        }
        void OnAteGrass()
        {
            sheared = false; visual?.Refresh();
            if (IsBaby) growAge = Mathf.Min(0, growAge + 1200);
            Particles.BlockBreak(world, Int3.Floor(position - Vector3.up * 0.5f), Blocks.StateOf("grass_block"));
        }
        public bool Eating => eatTimer > 0;
        public float HeadEatAngle(float partial) { if (eatTimer <= 0) return 0; float t = eatTimer - partial; return t > 4 && t <= 36 ? 36f : t <= 4 ? t * 9 : (40 - t) * 9; }

        public override Mob Breed(Mob mate)
        {
            var b = base.Breed(mate) as SheepMob;
            if (b != null && mate is SheepMob ms) b.variant = Random.value < 0.5f ? variant : ms.variant;
            return b;
        }
        public override void Save(Dictionary<string, string> d) { base.Save(d); if (sheared) d["sheared"] = "1"; }
        public override void Load(Dictionary<string, string> d) { base.Load(d); sheared = d.TryGetValue("sheared", out var s) && s == "1"; }
    }

    // =====================================================================================================
    public sealed class ChickenMob : AnimalMob
    {
        int eggTimer = Random.Range(6000, 12000);
        public float flap, prevFlap, flapSpeed;
        public override void Tick()
        {
            prevFlap = flap;
            base.Tick();
            if (dead || removed) return;
            flapSpeed += (onGround ? -1f : 4f) * 0.3f; flapSpeed = Mathf.Clamp01(flapSpeed);
            flap += flapSpeed * 2f;
            if (!onGround && velocity.y < 0) velocity.y *= 0.6f;
            fallDistance = 0;
            if (!IsBaby && --eggTimer <= 0)
            {
                Sounds.Play("entity.chicken.egg", position, 1f, (Random.value - Random.value) * 0.2f + 1f);
                world.SpawnItem(position, new ItemStack("egg", 1));
                eggTimer = Random.Range(6000, 12000);
            }
        }
        public override void ApplyFallDamage(float dist) { }
    }

    // =====================================================================================================
    public sealed class RabbitMob : AnimalMob
    {
        int hopDelay;
        public override void OnInitialSpawn(SpawnReason reason)
        {
            base.OnInitialSpawn(reason);
            var b = world.GetBiome(BlockPos);
            variant = b.snowy ? (Random.value < 0.8f ? 1 : 5) : b.key == "desert" ? 4 : Random.Range(0, 4) == 0 ? 2 : Random.Range(0, 2) == 0 ? 3 : 0;
        }
        protected override void MoveControl()
        {
            base.MoveControl();
            if (moveForward > 0 && onGround)
            {
                if (--hopDelay <= 0) { jumping = true; hopDelay = 10; } else { moveForward = 0; }
            }
        }
        public override float JumpPower => horizontalCollision ? 0.5f : 0.33f;
    }

    // =====================================================================================================
    public class HorseMob : AnimalMob
    {
        public int temper; public float jumpCharge;
        public bool chest;
        public override void OnInitialSpawn(SpawnReason reason)
        {
            base.OnInitialSpawn(reason);
            if (def.id == "horse") { variant = Random.Range(0, 7) + Random.Range(0, 5) * 7; maxHealth = health = 15 + Random.Range(0, 8) + Random.Range(0, 9); def = def; }
            if (def.id == "skeleton_horse" || def.id == "zombie_horse") { tamed = true; }
        }
        public override bool CanBeRiddenBy(Entity e) => passengers.Count == 0 && e is Player && !IsBaby;
        public override Vector3 PassengerOffset(Entity p) => new Vector3(0, height * 0.72f, -0.1f);

        public override bool Interact(Player p, ItemStack held)
        {
            if (IsBaby) return base.Interact(p, held);
            if (held != null && held.item.id == "saddle" && tamed && !saddled) { saddled = true; if (!p.IsCreative) held.count--; Sounds.Play("entity.horse.saddle", position, 1f, 1f); return true; }
            if (held != null && held.item.id == "chest" && tamed && !chest && (def.id == "donkey" || def.id == "mule")) { chest = true; if (!p.IsCreative) held.count--; return true; }
            if (held != null && IsFood(held.item))
            {
                if (health < maxHealth || !tamed) { Heal(held.item.id == "golden_apple" ? 10 : 2); temper = Mathf.Min(100, temper + 5); Consume(p, held); return true; }
            }
            if (p.sneaking && tamed) return false;
            if (passengers.Count == 0) { p.StartRiding(this); return true; }
            return base.Interact(p, held);
        }

        protected override void AiStep()
        {
            if (passengers.Count > 0 && passengers[0] is Player rider)
            {
                goals.StopAll(); nav.Stop();
                if (!tamed)
                {
                    // taming by riding: buck off unless temper is high enough
                    if (Random.Range(0, 60) == 0)
                    {
                        if (Random.Range(0, 100) < temper) { tamed = true; ownerName = rider.playerName; ownerId = rider.id; persistent = true; Particles.Heart(world, position + Vector3.up * height); for (int i = 0; i < 6; i++) Particles.Heart(world, position + Vector3.up * height); Sounds.Play("entity.horse.ambient", position, 1f, 1.2f); }
                        else { temper += 5; rider.StopRiding(); Sounds.Play("entity.horse.angry", position, 1f, 1f); velocity.y += 0.3f; return; }
                    }
                    moveForward = 0; base.AiStepRaw(); return;
                }
                if (!saddled) { moveForward = 0; base.AiStepRaw(); return; }
                yaw = rider.yaw; bodyYaw = yaw; lookYaw = yaw;
                moveForward = rider.moveForward; moveStrafe = rider.moveStrafe * 0.5f;
                if (moveForward <= 0) moveForward *= 0.25f;
                movementSpeed = def.speed;
                if (rider.jumping && onGround) { jumpCharge = Mathf.Min(1f, jumpCharge + 0.1f); }
                else if (jumpCharge > 0 && onGround) { velocity.y = 0.5f + jumpCharge * 0.6f; velocity += MathX.YawPitchToDir(yaw, 0) * 0.4f * jumpCharge; jumpCharge = 0; Sounds.Play("entity.horse.jump", position, 0.4f, 1f); }
                jumping = false;
                Travel(moveStrafe, moveForward);
                return;
            }
            base.AiStep();
        }
        public override void ApplyFallDamage(float dist) { float d = dist - 3 * 1.5f; if (d > 0) { Hurt(DamageSource.Fall, Mathf.Ceil(d)); foreach (var p in passengers) p.ApplyFallDamage(dist - 3); } }
        public override void Save(Dictionary<string, string> d) { base.Save(d); d["temper"] = temper.ToString(); if (chest) d["chest"] = "1"; }
        public override void Load(Dictionary<string, string> d) { base.Load(d); if (d.TryGetValue("temper", out var t)) int.TryParse(t, out temper); chest = d.TryGetValue("chest", out var c) && c == "1"; }
    }

    public sealed class LlamaMob : HorseMob
    {
        int spitCooldown;
        protected override void RegisterGoals()
        {
            base.RegisterGoals();
            targetGoals.Add(1, new HurtByTargetGoal(true), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<WolfMob>(true, 20, w => !w.tamed), GoalFlags.Target);
        }
        public override void OnInitialSpawn(SpawnReason reason) { base.OnInitialSpawn(reason); variant = Random.Range(0, 4); }
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (spitCooldown > 0) spitCooldown--;
            if (target != null && spitCooldown == 0 && (target.position - position).sqrMagnitude < 144 && world.HasLineOfSight(EyePosition, target.EyePosition))
            {
                var s = new LlamaSpit { world = world, owner = this };
                s.SetPosition(EyePosition + LookDir * 0.5f);
                var d = target.EyePosition - s.position;
                s.Launch(d + Vector3.up * d.magnitude * 0.2f, 1.5f, 10f);
                world.AddEntity(s);
                Sounds.Play("entity.llama.spit", position, 1f, 1f);
                spitCooldown = 40;
            }
        }
        public override bool CanBeRiddenBy(Entity e) => false;
    }

    public sealed class CamelMob : HorseMob
    {
        public int dashCooldown; public bool sittingPose;
        public override bool CanBeRiddenBy(Entity e) => passengers.Count < 2 && e is Player && !IsBaby && def.id == "camel";
        public override Vector3 PassengerOffset(Entity p) => new Vector3(0, height * 0.72f, passengers.IndexOf(p) == 0 ? 0.5f : -0.7f);
        public override void OnInitialSpawn(SpawnReason reason) { base.OnInitialSpawn(reason); tamed = def.id == "camel"; }
        protected override void RegisterGoals()
        {
            if (def.id == "camel_husk")
            {
                goals.Add(0, new FloatGoal(), GoalFlags.Jump);
                goals.Add(2, new MeleeAttackGoal(1.2f), GoalFlags.Move | GoalFlags.Look);
                goals.Add(5, new StrollGoal(0.8f), GoalFlags.Move);
                targetGoals.Add(1, new HurtByTargetGoal(false), GoalFlags.Target);
                targetGoals.Add(2, new NearestTargetGoal<Player>(), GoalFlags.Target);
            }
            else base.RegisterGoals();
        }
    }

    // =====================================================================================================
    public sealed class WolfMob : AnimalMob
    {
        public bool angry => angerTicks > 0 || (target != null && !tamed);
        public int collarColor = 14;
        public float shake;
        protected override void RegisterGoals()
        {
            goals.Add(0, new FloatGoal(), GoalFlags.Jump);
            goals.Add(1, new SitGoal(), GoalFlags.Move | GoalFlags.Jump);
            goals.Add(3, new MeleeAttackGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(4, new FollowOwnerGoal(1f, 10, 2), GoalFlags.Move | GoalFlags.Look);
            goals.Add(5, new BreedGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(6, new StrollGoal(1f), GoalFlags.Move);
            goals.Add(7, new LookAtPlayerGoal(8), GoalFlags.Look);
            goals.Add(8, new RandomLookGoal(), GoalFlags.Look);
            targetGoals.Add(1, new OwnerHurtTargetGoal(), GoalFlags.Target);
            targetGoals.Add(2, new HurtByTargetGoal(true), GoalFlags.Target);
            targetGoals.Add(4, new NearestTargetGoal<Mob>(false, 50, m => !tamed && (m.def.id == "sheep" || m.def.id == "rabbit" || m.def.id == "fox")), GoalFlags.Target);
            targetGoals.Add(5, new NearestTargetGoal<SkeletonMob>(false, 20), GoalFlags.Target);
        }
        protected override bool CanFallInLove(Player p) => tamed;
        public override bool Interact(Player p, ItemStack held)
        {
            if (!tamed && held != null && held.item.id == "bone" && !angry)
            {
                if (!p.IsCreative) held.count--;
                if (Random.Range(0, 3) == 0)
                {
                    tamed = true; ownerName = p.playerName; ownerId = p.id; persistent = true; maxHealth = health = 40; sitting = true; target = null;
                    for (int i = 0; i < 7; i++) Particles.Heart(world, position + Vector3.up * height);
                    Achievements.Grant(p, "tame_an_animal");
                }
                else for (int i = 0; i < 7; i++) Particles.Smoke(world, position + Vector3.up * height, 1, 0.5f);
                return true;
            }
            if (tamed && held != null && IsFood(held.item) && health < maxHealth) { Heal(4); Consume(p, held); return true; }
            if (tamed && held != null && held.item.id.EndsWith("_dye")) { int i = System.Array.IndexOf(Blocks.Colors, held.item.id.Replace("_dye", "")); if (i >= 0) { collarColor = i; if (!p.IsCreative) held.count--; visual?.Refresh(); return true; } }
            if (tamed && (held == null || !IsFood(held.item)) && Owner == p) { sitting = !sitting; nav.Stop(); target = null; jumping = false; return true; }
            return base.Interact(p, held);
        }
        protected override void OnAttackHit(Entity t) { }
        public override void Save(Dictionary<string, string> d) { base.Save(d); d["collar"] = collarColor.ToString(); }
        public override void Load(Dictionary<string, string> d) { base.Load(d); if (d.TryGetValue("collar", out var c)) int.TryParse(c, out collarColor); if (tamed) maxHealth = 40; }
    }

    public sealed class SitGoal : Goal
    {
        public override bool CanUse() => mob.tamed && mob.sitting && !mob.inWater;
        public override void Start() { mob.nav.Stop(); }
        public override void Tick() { mob.moveTarget = null; }
    }

    public sealed class FollowOwnerGoal : Goal
    {
        readonly float speed, start, stop; Player owner; int t;
        public FollowOwnerGoal(float s, float start, float stop) { speed = s; this.start = start; this.stop = stop; }
        public override bool CanUse()
        {
            if (!mob.tamed || mob.sitting) return false;
            owner = mob.Owner;
            return owner != null && !owner.IsSpectator && owner.world == mob.world && (owner.position - mob.position).sqrMagnitude > start * start;
        }
        public override bool CanContinue() => owner != null && !mob.sitting && (owner.position - mob.position).sqrMagnitude > stop * stop && !mob.nav.IsDone;
        public override void Tick()
        {
            mob.LookAt(owner);
            if (--t > 0) return;
            t = 10;
            if ((owner.position - mob.position).sqrMagnitude > 144)
            {
                for (int i = 0; i < 10; i++)
                {
                    var p = owner.position + new Vector3(Random.Range(-3, 4), 0, Random.Range(-3, 4));
                    var bp = Int3.Floor(p);
                    if (!mob.world.GetBlock(bp).solid && !mob.world.GetBlock(bp.Offset(Dir.Up)).solid && mob.world.GetBlock(bp.Offset(Dir.Down)).solid) { mob.Teleport(new Vector3(bp.x + 0.5f, bp.y, bp.z + 0.5f)); mob.nav.Stop(); return; }
                }
            }
            mob.nav.MoveTo(owner.position, speed);
        }
    }

    public sealed class OwnerHurtTargetGoal : Goal
    {
        public override bool CanUse()
        {
            if (!mob.tamed || mob.sitting) return false;
            var o = mob.Owner;
            if (o == null) return false;
            var t = o.lastHurtMob as LivingEntity;
            if (t == null || t.dead || t == mob || (t is Mob m && m.tamed && m.ownerId == mob.ownerId) || t is CreeperMob) t = o.lastAttacker;
            if (t == null || t.dead || t == mob || t is Player) return false;
            if ((t.position - mob.position).sqrMagnitude > 400) return false;
            mob.target = t;
            return false;
        }
    }

    public sealed class CatMob : AnimalMob
    {
        protected override void RegisterGoals()
        {
            goals.Add(0, new FloatGoal(), GoalFlags.Jump);
            goals.Add(1, new SitGoal(), GoalFlags.Move);
            goals.Add(2, new TemptGoal(0.6f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(3, new FollowOwnerGoal(1f, 10, 5), GoalFlags.Move);
            goals.Add(4, new MeleeAttackGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(5, new BreedGoal(0.8f), GoalFlags.Move);
            goals.Add(6, new StrollGoal(0.8f), GoalFlags.Move);
            goals.Add(7, new LookAtPlayerGoal(10), GoalFlags.Look);
            targetGoals.Add(1, new NearestTargetGoal<Mob>(false, 60, m => m.def.id == "rabbit" || m.def.id == "chicken" || (def.id == "cat" && m.def.id == "rabbit")), GoalFlags.Target);
        }
        public override void OnInitialSpawn(SpawnReason reason) { base.OnInitialSpawn(reason); variant = Random.Range(0, 11); }
        protected override bool CanFallInLove(Player p) => tamed || def.id == "ocelot";
        public override bool Interact(Player p, ItemStack held)
        {
            if (def.id == "cat" && !tamed && held != null && IsFood(held.item))
            {
                Consume(p, held);
                if (Random.Range(0, 3) == 0) { tamed = true; ownerId = p.id; ownerName = p.playerName; persistent = true; sitting = true; for (int i = 0; i < 7; i++) Particles.Heart(world, position + Vector3.up * height); Achievements.Grant(p, "tame_an_animal"); }
                return true;
            }
            if (tamed && Owner == p && (held == null || !IsFood(held.item))) { sitting = !sitting; nav.Stop(); return true; }
            return base.Interact(p, held);
        }
        public override void ApplyFallDamage(float dist) { base.ApplyFallDamage(dist - 2); }
    }

    public sealed class FoxMob : AnimalMob
    {
        protected override void RegisterGoals()
        {
            base.RegisterGoals();
            goals.Add(3, new MeleeAttackGoal(1.2f), GoalFlags.Move | GoalFlags.Look);
            targetGoals.Add(1, new NearestTargetGoal<Mob>(false, 80, m => m.def.id == "chicken" || m.def.id == "rabbit" || m.def.id == "cod" || m.def.id == "salmon"), GoalFlags.Target);
        }
        public override void OnInitialSpawn(SpawnReason reason) { base.OnInitialSpawn(reason); variant = world.GetBiome(BlockPos).snowy ? 1 : 0; }
    }

    public sealed class GoatMob : AnimalMob
    {
        int ramCooldown = 600; Entity ramTarget; int ramTicks;
        public override void OnInitialSpawn(SpawnReason reason) { base.OnInitialSpawn(reason); variant = Random.value < 0.02f ? 1 : 0; }
        public override float JumpPower => 0.6f;
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (ramTicks > 0)
            {
                ramTicks--;
                if (ramTarget != null && !ramTarget.removed)
                {
                    Vector3 d = ramTarget.position - position; d.y = 0;
                    yaw = MathX.YawFromDir(d); moveForward = 1; movementSpeed = 0.4f;
                    if (d.sqrMagnitude < 2f) { ramTarget.Hurt(DamageSource.MobAttack(this), 2); if (ramTarget is LivingEntity le) le.Knockback(2.5f, -d.normalized.x, -d.normalized.z); ramTicks = 0; Sounds.Play("entity.goat.ram_impact", position, 1f, 1f); }
                }
                return;
            }
            if (--ramCooldown <= 0)
            {
                ramCooldown = variant == 1 ? Random.Range(100, 300) : Random.Range(600, 6000);
                ramTarget = world.FindNearest<LivingEntity>(position, 10, e => e != this && !(e is GoatMob) && !(e is Player p && (p.IsCreative || p.IsSpectator)));
                if (ramTarget != null) { ramTicks = 60; Sounds.Play("entity.goat.prepare_ram", position, 1f, 1f); }
            }
        }
        public override bool Interact(Player p, ItemStack held)
        {
            if (held != null && held.item.id == "bucket" && !IsBaby) { if (!p.IsCreative) held.count--; p.inventory.AddOrDrop(new ItemStack("milk_bucket", 1)); Sounds.Play("entity.goat.milk", position, 1f, 1f); return true; }
            return base.Interact(p, held);
        }
    }

    /// <summary>Polar bear / panda: passive unless provoked, then melee.</summary>
    public sealed class NeutralBeastMob : AnimalMob
    {
        protected override void RegisterGoals()
        {
            goals.Add(0, new FloatGoal(), GoalFlags.Jump);
            goals.Add(1, new MeleeAttackGoal(1.25f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(3, new BreedGoal(1f), GoalFlags.Move);
            goals.Add(4, new TemptGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(5, new FollowParentGoal(1.1f), GoalFlags.Move);
            goals.Add(6, new StrollGoal(1f), GoalFlags.Move);
            goals.Add(7, new LookAtPlayerGoal(6), GoalFlags.Look);
            targetGoals.Add(1, new HurtByTargetGoal(true), GoalFlags.Target);
        }
        public override void OnInitialSpawn(SpawnReason reason) { base.OnInitialSpawn(reason); if (def.id == "panda") variant = Random.Range(0, 7); }
    }

    public sealed class TurtleMob : AnimalMob
    {
        protected override void CustomTravel()
        {
            if (inWater)
            {
                if (moveTarget.HasValue) { var d = moveTarget.Value - position; velocity += d.normalized * 0.02f; yaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(d), 10f); }
                velocity *= 0.9f; velocity.y += 0.005f;
                Move(velocity);
            }
            else base.CustomTravel();
        }
    }

    public sealed class FrogMob : AnimalMob
    {
        int tongueCooldown; public int tongueAnim; Entity prey;
        public override void OnInitialSpawn(SpawnReason reason)
        {
            base.OnInitialSpawn(reason);
            var b = world.GetBiome(BlockPos);
            variant = b.temperature < 0.3f ? 2 : b.temperature > 0.9f ? 1 : 0;
        }
        public override float JumpPower => 0.5f;
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (tongueAnim > 0) tongueAnim--;
            if (--tongueCooldown > 0) return;
            tongueCooldown = 40;
            prey = world.FindNearest<SlimeMob>(position, 3.5f, s => s.size <= 1);
            if (prey != null)
            {
                tongueAnim = 10;
                Sounds.Play("entity.frog.tongue", position, 1f, 1f);
                if (prey is SlimeMob sm && sm.def.id == "magma_cube") world.SpawnItem(prey.position, new ItemStack(variant == 0 ? "ochre_froglight" : variant == 1 ? "pearlescent_froglight" : "verdant_froglight", 1));
                else if (prey is SlimeMob) world.SpawnItem(prey.position, new ItemStack("slime_ball", 1));
                prey.Remove();
            }
        }
        public override void ApplyFallDamage(float dist) { base.ApplyFallDamage(dist - 5); }
    }

    public sealed class AxolotlMob : Mob
    {
        int playDead;
        protected override void RegisterGoals()
        {
            goals.Add(1, new MeleeAttackGoal(1.2f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(2, new BreedGoal(1f), GoalFlags.Move);
            goals.Add(3, new StrollGoal(1f, 80, false), GoalFlags.Move);
            targetGoals.Add(1, new NearestTargetGoal<Mob>(true, 20, m => m.def.id == "drowned" || m.def.id == "guardian" || m.def.id == "squid" || m.def.id == "glow_squid" || m.def.id == "tropical_fish" || m.def.id == "cod" || m.def.id == "salmon" || m.def.id == "pufferfish"), GoalFlags.Target);
        }
        public override void OnInitialSpawn(SpawnReason reason) { variant = Random.value < 1f / 1200f ? 4 : Random.Range(0, 4); }
        public override bool Hurt(DamageSource src, float amount)
        {
            bool r = base.Hurt(src, amount);
            if (r && health < maxHealth * 0.3f && Random.value < 0.33f) playDead = 200;
            return r;
        }
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (playDead > 0) { playDead--; Heal(0.05f); nav.Stop(); moveTarget = null; }
            if (!inWater && !world.IsRainingAt(BlockPos)) { if (age % 20 == 0 && ++dryTicks > 300) Hurt(DamageSource.Drown, 1); } else dryTicks = 0;
        }
        int dryTicks;
        public bool PlayingDead => playDead > 0;
        protected override void CustomTravel()
        {
            if (moveTarget.HasValue) { var d = moveTarget.Value - position; if (d.sqrMagnitude > 0.1f) { velocity += d.normalized * 0.02f; yaw = MathX.ApproachAngle(yaw, MathX.YawFromDir(d), 10f); } }
            velocity *= 0.9f;
            Move(velocity);
        }
    }

    public sealed class SnifferMob : AnimalMob
    {
        int digTimer = Random.Range(600, 2400); public int digging;
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (digging > 0)
            {
                digging--;
                if (digging % 10 == 0) Particles.BlockDust(world, position + LookDir * 1.5f, world.GetState(Int3.Floor(position - Vector3.up * 0.5f)), 4);
                if (digging == 0) { world.SpawnItem(position + LookDir * 1.5f, new ItemStack(Random.value < 0.5f ? "torchflower_seeds" : "pitcher_pod", 1)); Sounds.Play("entity.sniffer.drop_seed", position, 1f, 1f); }
                nav.Stop(); moveTarget = null;
                return;
            }
            if (--digTimer <= 0)
            {
                digTimer = Random.Range(2400, 6000);
                var below = world.GetBlock(Int3.Floor(position - Vector3.up * 0.5f));
                if (below.id == "grass_block" || below.id == "dirt" || below.id == "moss_block" || below.id == "podzol" || below.id == "mud") { digging = 80; Sounds.Play("entity.sniffer.digging", position, 1f, 1f); }
            }
        }
    }

    // =====================================================================================================
    public sealed class VillagerMob : Mob
    {
        public static readonly string[] Professions = { "none", "farmer", "librarian", "armorer", "weaponsmith", "toolsmith", "cleric", "butcher", "fisherman", "fletcher", "leatherworker", "mason", "shepherd", "cartographer", "nitwit" };
        public List<MerchantOffer> offers;
        public int level = 1, tradeXp;
        public Player tradingWith;
        public int happyTimer;
        public string Profession => Professions[Mathf.Clamp(variant, 0, Professions.Length - 1)];

        protected override void RegisterGoals()
        {
            goals.Add(0, new FloatGoal(), GoalFlags.Jump);
            goals.Add(1, new AvoidEntityGoal<Mob>(8, 0.6f, 0.75f, m => m.def.id.Contains("zombie") || m.def.id == "husk" || m.def.id == "drowned" || MobRegistry.IsRaider(m.def.id) || m.def.id == "vex"), GoalFlags.Move);
            goals.Add(2, new PanicGoal(0.75f), GoalFlags.Move);
            goals.Add(3, new TradeLookGoal(), GoalFlags.Move | GoalFlags.Look);
            goals.Add(6, new StrollGoal(0.5f), GoalFlags.Move);
            goals.Add(7, new LookAtPlayerGoal(8), GoalFlags.Look);
            goals.Add(8, new RandomLookGoal(), GoalFlags.Look);
        }
        public override void OnInitialSpawn(SpawnReason reason)
        {
            if (def.id == "wandering_trader") { variant = 0; persistent = false; return; }
            if (reason == SpawnReason.Breeding) { variant = 0; return; }
            if (variant == 0) variant = Random.Range(1, Professions.Length);
        }
        public override bool CanDespawn => def.id == "wandering_trader" && age > 48000;
        public override string AmbientSound => def.id == "wandering_trader" ? "entity.wandering_trader.ambient" : (Profession == "nitwit" || variant == 0 ? "entity.villager.ambient" : "entity.villager.ambient");

        public override bool Interact(Player p, ItemStack held)
        {
            if (held != null && (held.item is SpawnEggItem || held.item.id == "name_tag")) return base.Interact(p, held);
            if (IsBaby) { Sounds.Play("entity.villager.no", position, 1f, 1.5f); return true; }
            if (def.id == "villager" && (variant == 0 || Profession == "nitwit")) { Sounds.Play("entity.villager.no", position, 1f, 1f); return true; }
            if (offers == null) offers = Trading.Generate(def.id == "wandering_trader" ? "wandering_trader" : Profession, level, new RNG(id * 7919 + (int)world.seed));
            tradingWith = p;
            nav.Stop();
            p.OpenMenu(new MerchantMenu(p, this));
            Sounds.Play("entity.villager.trade", position, 1f, 1f);
            return true;
        }
        public void OnTrade(MerchantOffer o)
        {
            tradeXp += o.xp;
            happyTimer = 40;
            Sounds.Play("entity.villager.yes", position, 1f, 1f);
            XpOrb.Spawn(world, position + Vector3.up * 0.5f, 3 + Random.Range(0, 4));
            int[] thresholds = { 0, 10, 70, 150, 250 };
            if (level < 5 && tradeXp >= thresholds[level])
            {
                level++;
                offers.AddRange(Trading.Generate(Profession, level, new RNG(id * 131 + level), true));
                Particles.HappyVillager(world, position + Vector3.up * height, 8);
            }
        }
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (happyTimer > 0 && --happyTimer % 10 == 0) Particles.HappyVillager(world, position + Vector3.up * height, 1);
            if (tradingWith != null && (tradingWith.menu == null || !(tradingWith.menu is MerchantMenu))) tradingWith = null;
            // job site: adopt profession from nearby workstation when unemployed
            if (def.id == "villager" && variant == 0 && !IsBaby && age % 100 == 0)
            {
                string[] stations = { "", "composter", "lectern", "blast_furnace", "grindstone", "smithing_table", "brewing_stand", "smoker", "barrel", "fletching_table", "cauldron", "stonecutter", "loom", "cartography_table" };
                for (int dx = -4; dx <= 4 && variant == 0; dx++)
                    for (int dy = -2; dy <= 2 && variant == 0; dy++)
                        for (int dz = -4; dz <= 4 && variant == 0; dz++)
                        {
                            var b = world.GetBlock(BlockPos.Offset(dx, dy, dz));
                            int idx = System.Array.IndexOf(stations, b.id);
                            if (idx > 0) { variant = idx; visual?.Refresh(); Particles.HappyVillager(world, position + Vector3.up * height, 6); }
                        }
            }
        }
        public override void OnStruckByLightning(LightningBolt bolt)
        {
            var w = MobRegistry.Spawn(world, "witch", position, SpawnReason.Conversion);
            if (w != null) { w.yaw = yaw; Remove(); }
        }
        public override void Save(Dictionary<string, string> d) { base.Save(d); d["level"] = level.ToString(); d["txp"] = tradeXp.ToString(); if (offers != null) d["offers"] = Trading.Serialize(offers); }
        public override void Load(Dictionary<string, string> d)
        {
            base.Load(d);
            if (d.TryGetValue("level", out var l)) int.TryParse(l, out level);
            if (d.TryGetValue("txp", out var x)) int.TryParse(x, out tradeXp);
            if (d.TryGetValue("offers", out var o)) offers = Trading.Deserialize(o);
        }
    }

    public sealed class TradeLookGoal : Goal
    {
        public override bool CanUse() => mob is VillagerMob v && v.tradingWith != null;
        public override void Tick() { if (mob is VillagerMob v && v.tradingWith != null) { mob.LookAt(v.tradingWith); mob.nav.Stop(); mob.moveTarget = null; } }
    }

    // =====================================================================================================
    public sealed class IronGolemMob : Mob
    {
        public int attackAnim; public int offerFlower;
        public bool playerCreated;
        protected override void RegisterGoals()
        {
            goals.Add(1, new MeleeAttackGoal(1f), GoalFlags.Move | GoalFlags.Look);
            goals.Add(6, new StrollGoal(0.6f, 240), GoalFlags.Move);
            goals.Add(7, new LookAtPlayerGoal(6), GoalFlags.Look);
            goals.Add(8, new RandomLookGoal(), GoalFlags.Look);
            targetGoals.Add(1, new HurtByTargetGoal(false), GoalFlags.Target);
            targetGoals.Add(2, new NearestTargetGoal<Mob>(false, 5, m => m.def.hostile && !(m is CreeperMob) && m.def.category == MobCategory.Monster), GoalFlags.Target);
        }
        public override bool CanDespawn => false;
        public override float AttackReachSqr(LivingEntity t) => 6.5f + t.width;
        public override bool DoMeleeAttack(Entity t)
        {
            attackAnim = 10;
            float dmg = 7.5f + Random.Range(0f, 14f);
            SwingArm();
            bool r = t.Hurt(DamageSource.MobAttack(this), dmg);
            if (r) { t.velocity.y += 0.4f; Sounds.Play("entity.iron_golem.attack", position, 1f, 1f); }
            return r;
        }
        public override void Tick()
        {
            base.Tick();
            if (attackAnim > 0) attackAnim--;
            if (offerFlower > 0) offerFlower--;
        }
        public override bool Interact(Player p, ItemStack held)
        {
            if (held != null && held.item.id == "iron_ingot" && health < maxHealth) { Heal(25); if (!p.IsCreative) held.count--; Sounds.Play("entity.iron_golem.repair", position, 1f, 1f); return true; }
            return base.Interact(p, held);
        }
        public override string AmbientSound => null;
        public override void ApplyFallDamage(float dist) { }
    }

    public sealed class SnowGolemMob : Mob
    {
        int shootCooldown;
        public bool pumpkin = true;
        protected override void RegisterGoals()
        {
            goals.Add(1, new SnowballAttackGoal(), GoalFlags.Move | GoalFlags.Look);
            goals.Add(6, new StrollGoal(1f), GoalFlags.Move);
            goals.Add(7, new LookAtPlayerGoal(6), GoalFlags.Look);
            targetGoals.Add(1, new NearestTargetGoal<Mob>(true, 10, m => m.def.hostile && m.def.category == MobCategory.Monster), GoalFlags.Target);
        }
        public override bool CanDespawn => false;
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            var b = world.GetBiome(BlockPos);
            if (b.temperature > 1f || world.IsRainingAt(BlockPos) || inWater) { if (age % 20 == 0) Hurt(DamageSource.OnFire, 1); }
            if (b.temperature < 0.8f && onGround && (world.session == null || world.session.mobGriefing))
            {
                var p = Int3.Floor(position);
                if (world.IsAir(p) && world.GetBlock(p.Offset(Dir.Down)).opaqueCube) world.SetState(p, Blocks.StateOf("snow"));
            }
        }
        public override bool Interact(Player p, ItemStack held)
        {
            if (held != null && held.item.id == "shears" && pumpkin) { pumpkin = false; held.HurtAndBreak(1, p); world.SpawnItem(EyePosition, new ItemStack("carved_pumpkin", 1)); Sounds.Play("entity.snow_golem.shear", position, 1f, 1f); visual?.Refresh(); return true; }
            return base.Interact(p, held);
        }
        public override string AmbientSound => null;
        sealed class SnowballAttackGoal : Goal
        {
            int cd;
            public override bool CanUse() => mob.target != null && mob.target.IsAlive;
            public override void Tick()
            {
                var t = mob.target; mob.LookAt(t);
                float d2 = (t.position - mob.position).sqrMagnitude;
                if (d2 > 100) { if (mob.age % 10 == 0) mob.nav.MoveTo(t.position, 1.25f); } else mob.nav.Stop();
                if (--cd <= 0 && d2 < 144)
                {
                    cd = 20;
                    var sb = new ThrownItemProjectile("snowball") { world = mob.world, owner = mob };
                    sb.SetPosition(mob.EyePosition - Vector3.up * 0.1f);
                    var dir = t.EyePosition - sb.position; dir.y += Mathf.Sqrt(dir.x * dir.x + dir.z * dir.z) * 0.2f;
                    sb.Launch(dir, 1.6f, 12f);
                    mob.world.AddEntity(sb);
                    Sounds.Play("entity.snow_golem.shoot", mob.position, 1f, 0.4f / (Random.value * 0.4f + 0.8f));
                }
            }
        }
    }

    /// <summary>26.x Copper Golem: carries items from a copper chest to matching chests; oxidizes slowly.</summary>
    public sealed class CopperGolemMob : Mob
    {
        public ItemStack carried; int state; Int3 targetChest; int wait; public int oxidation; int oxTimer = Random.Range(24000, 36000); public bool waxed;
        protected override void RegisterGoals()
        {
            goals.Add(0, new FloatGoal(), GoalFlags.Jump);
            goals.Add(2, new PanicGoal(1.25f), GoalFlags.Move);
            goals.Add(8, new LookAtPlayerGoal(6), GoalFlags.Look);
        }
        public override bool CanDespawn => false;
        public override string AmbientSound => null;
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            if (!waxed && oxidation < 3 && --oxTimer <= 0) { oxidation++; oxTimer = Random.Range(24000, 36000); visual?.Refresh(); }
            if (oxidation >= 3) { nav.Stop(); return; }
            if (wait > 0) { wait--; return; }
            if (!nav.IsDone) return;
            if (state == 0)
            {
                if (FindChest(true, out targetChest)) { nav.MoveTo(targetChest.Center, 1f); state = 1; } else wait = 100;
            }
            else if (state == 1)
            {
                if ((targetChest.Center - position).sqrMagnitude < 4 && world.GetBlockEntity(targetChest) is ContainerEntity ce)
                {
                    for (int i = 0; i < ce.Size; i++) { var s = ce.Get(i); if (s != null) { carried = s.Split(Mathf.Min(16, s.count)); if (s.count <= 0) ce.Set(i, null); ce.SetChanged(); break; } }
                    Sounds.Play("entity.copper_golem.item_get", position, 1f, 1f);
                    state = carried != null ? 2 : 0; wait = 20; visual?.Refresh();
                }
                else state = 0;
            }
            else if (state == 2)
            {
                if (FindChest(false, out targetChest)) { nav.MoveTo(targetChest.Center, 1f); state = 3; } else { wait = 100; }
            }
            else if (state == 3)
            {
                if ((targetChest.Center - position).sqrMagnitude < 4 && world.GetBlockEntity(targetChest) is ContainerEntity ce && carried != null)
                {
                    var left = ce.AddItem(carried);
                    carried = left;
                    Sounds.Play("entity.copper_golem.item_drop", position, 1f, 1f);
                    wait = 20; visual?.Refresh();
                }
                state = carried != null ? 2 : 0;
            }
        }
        bool FindChest(bool copper, out Int3 found)
        {
            found = default; float best = float.MaxValue;
            for (int dx = -12; dx <= 12; dx++)
                for (int dy = -3; dy <= 3; dy++)
                    for (int dz = -12; dz <= 12; dz++)
                    {
                        var p = BlockPos.Offset(dx, dy, dz);
                        var b = world.GetBlock(p);
                        bool isCopper = b.id.Contains("copper_chest");
                        if (copper != isCopper || !(b is ChestBlock)) continue;
                        if (!(world.GetBlockEntity(p) is ContainerEntity ce)) continue;
                        if (copper) { bool any = false; for (int i = 0; i < ce.Size; i++) if (ce.Get(i) != null) { any = true; break; } if (!any) continue; }
                        else
                        {
                            bool match = false, empty = true;
                            for (int i = 0; i < ce.Size; i++) { var s = ce.Get(i); if (s != null) { empty = false; if (carried != null && s.item == carried.item && s.count < s.MaxStack) match = true; } }
                            if (!match && !empty) continue;
                            if (!match && empty) { }
                        }
                        float d = dx * dx + dy * dy + dz * dz;
                        if (d < best) { best = d; found = p; }
                    }
            return best < float.MaxValue;
        }
        public override bool Interact(Player p, ItemStack held)
        {
            if (held != null && held.item.id == "honeycomb" && !waxed) { waxed = true; if (!p.IsCreative) held.count--; Particles.WaxOn(world, BlockPos); return true; }
            if (held != null && held.item.toolType == ToolType.Axe && (oxidation > 0 || waxed)) { if (waxed) waxed = false; else oxidation--; held.HurtAndBreak(1, p); Particles.Scrape(world, BlockPos); visual?.Refresh(); return true; }
            return base.Interact(p, held);
        }
        protected override void OnDeath(DamageSource src) { base.OnDeath(src); if (carried != null) world.SpawnItem(position, carried); }
        public override void Save(Dictionary<string, string> d) { base.Save(d); d["ox"] = oxidation.ToString(); if (waxed) d["wax"] = "1"; if (carried != null) d["carried"] = carried.Serialize(); }
        public override void Load(Dictionary<string, string> d) { base.Load(d); if (d.TryGetValue("ox", out var o)) int.TryParse(o, out oxidation); waxed = d.TryGetValue("wax", out var w) && w == "1"; if (d.TryGetValue("carried", out var c)) carried = ItemStack.Deserialize(c); }
    }

    // =====================================================================================================
    public sealed class StriderMob : AnimalMob
    {
        public bool cold;
        public override void Tick()
        {
            base.Tick();
            if (dead || removed) return;
            bool onLava = world.IsLava(Int3.Floor(position - Vector3.up * 0.2f)) || inLava;
            cold = !onLava;
            if (inLava)
            {
                // float on lava surface
                float surf = Mathf.Floor(position.y) + 1f;
                if (world.IsLava(Int3.Floor(position + Vector3.up * 0.5f))) velocity.y = Mathf.Max(velocity.y, 0.08f);
                else if (position.y < surf - 0.3f) velocity.y = 0.05f;
                onGround = true;
            }
            if (world.IsWater(BlockPos) || world.IsRainingAt(BlockPos)) { if (age % 20 == 0) Hurt(DamageSource.Drown, 1); }
        }
        public override bool Hurt(DamageSource src, float amount) { if (src == DamageSource.Lava || src == DamageSource.OnFire || src == DamageSource.InFire) return false; return base.Hurt(src, amount); }
    }
}
