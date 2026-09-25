using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Base entity. Simulated at 20 ticks per second with Minecraft-style voxel AABB collision;
    /// rendered every frame with interpolation between prevPosition and position.
    /// </summary>
    public abstract class Entity
    {
        static int nextId = 1;
        public readonly int id = nextId++;
        public World world;
        public Vector3 position, prevPosition, velocity;
        public float yaw, pitch, prevYaw, prevPitch;
        public float width = 0.6f, height = 1.8f;
        public bool onGround, horizontalCollision, verticalCollision, collidedBelow;
        public bool inWater, inLava, eyeInWater, inPowderSnow, wasInWater;
        public bool removed;
        public float fallDistance;
        public int age;
        public int fireTicks;
        public bool fireImmune;
        public bool noPhysics, noGravity;
        public float stepHeight = 0f;
        public bool blocksBuilding = true;
        public bool pushable = true;
        public int portalCooldown, portalTime;
        public bool inNetherPortal;
        public Entity vehicle;
        public readonly List<Entity> passengers = new List<Entity>();
        public GameObject go;
        public string customName;
        public bool persistent;
        public Vector3 stuckSpeed = Vector3.one;
        public bool sneaking;
        public bool invulnerable;
        public bool glowing;

        public bool onFire => fireTicks > 0 && !fireImmune;
        public virtual string TypeId => GetType().Name.ToLowerInvariant();
        public virtual string DisplayName => customName ?? Blocks.PrettyName(TypeId);
        public AABB Bounds => AABB.FromCenterBottom(position, width, height);
        public virtual float EyeHeight => height * 0.85f;
        public Vector3 EyePosition => position + Vector3.up * EyeHeight;
        public Vector3 LookDir => MathX.YawPitchToDir(yaw, pitch);
        public Dir HorizontalFacing => DirUtil.FromYaw(yaw);
        public Int3 BlockPos => Int3.Floor(position);

        public void SetPosition(Vector3 p) { position = p; prevPosition = p; }

        public virtual void OnAddedToWorld() { CreateVisual(); }
        public virtual void CreateVisual() { }
        public virtual void Remove()
        {
            if (removed) return;
            removed = true;
            if (vehicle != null) { vehicle.passengers.Remove(this); vehicle = null; }
            foreach (var p in passengers.ToArray()) p.StopRiding();
            if (go != null) { Object.Destroy(go); go = null; }
        }

        // ------------------------------------------------------------------ ticking
        public virtual void Tick()
        {
            age++;
            prevPosition = position; prevYaw = yaw; prevPitch = pitch;
            BaseTick();
        }

        /// <summary>Tick while riding another entity (position is driven by the vehicle).</summary>
        public virtual void RideTick()
        {
            age++;
            prevYaw = yaw; prevPitch = pitch;
            velocity = Vector3.zero;
            fallDistance = 0;
            BaseTick();
        }

        protected virtual void BaseTick()
        {
            if (portalCooldown > 0) portalCooldown--;
            UpdateFluidState();
            if (inLava && !fireImmune) SetOnFire(15);
            if (fireTicks > 0)
            {
                if (fireImmune) fireTicks = Mathf.Max(0, fireTicks - 4);
                else
                {
                    if (fireTicks % 20 == 0 && this is LivingEntity le) le.Hurt(DamageSource.OnFire, 1f);
                    fireTicks--;
                }
                if (inWater || (world != null && world.IsRainingAt(BlockPos))) fireTicks = 0;
            }
            if (position.y < world.minY - 64) OnVoid();
            HandlePortal();
        }

        protected virtual void OnVoid() => Remove();

        protected void UpdateFluidState()
        {
            wasInWater = inWater;
            inWater = false; inLava = false; eyeInWater = false; inPowderSnow = false;
            var b = Bounds.Grow(-0.001f);
            int x0 = Mathf.FloorToInt(b.min.x), x1 = Mathf.FloorToInt(b.max.x);
            int y0 = Mathf.FloorToInt(b.min.y), y1 = Mathf.FloorToInt(b.max.y);
            int z0 = Mathf.FloorToInt(b.min.z), z1 = Mathf.FloorToInt(b.max.z);
            for (int x = x0; x <= x1; x++)
                for (int y = y0; y <= y1; y++)
                    for (int z = z0; z <= z1; z++)
                    {
                        ushort s = world.GetState(x, y, z);
                        if (s == 0) continue;
                        var bl = Blocks.ByState[s];
                        int m = s - bl.baseState;
                        if (bl.isLiquid || bl.IsWaterLike(m))
                        {
                            float h = bl.isLiquid ? MeshCtx.LiquidHeight(m) : 1f;
                            if (Blocks.SameFluid(bl, world.GetBlock(x, y + 1, z))) h = 1f;
                            if (b.min.y < y + h)
                            {
                                if (bl.IsWaterLike(m)) inWater = true;
                                else if (bl is FluidBlock fb && fb.fluidKind == 1) inLava = true;
                            }
                        }
                        else if (bl.id == "powder_snow") inPowderSnow = true;
                    }
            Vector3 eye = EyePosition;
            Int3 ep = Int3.Floor(eye);
            ushort es = world.GetState(ep);
            var eb = Blocks.ByState[es];
            if (eb.IsWaterLike(es - eb.baseState))
            {
                float h = eb.isLiquid ? MeshCtx.LiquidHeight(es - eb.baseState) : 1f;
                if (Blocks.SameFluid(eb, world.GetBlock(ep.x, ep.y + 1, ep.z))) h = 1f;
                eyeInWater = eye.y < ep.y + h - 0.11f;
            }
            if (inWater) { fallDistance = 0; if (fireTicks > 0 && !(this is LivingEntity l2 && l2.fireImmune)) Extinguish(); }
        }

        public void SetOnFire(int seconds)
        {
            if (fireImmune) return;
            if (this is LivingEntity le && le.GetEffectLevel(Effect.FireResistance) >= 0) return;
            int t = seconds * 20;
            if (this is LivingEntity le2) t = le2.ApplyFireProtection(t);
            if (fireTicks < t) fireTicks = t;
        }
        public void Extinguish()
        {
            if (fireTicks > 0) Sounds.Play("entity.generic.extinguish_fire", position, 0.7f, 1.6f);
            fireTicks = 0;
        }

        public virtual void ApplyFallDamage(float dist) { }

        // ------------------------------------------------------------------ portals
        public void EnterNetherPortal(Int3 pos)
        {
            if (portalCooldown > 0) { portalCooldown = 20; return; }
            inNetherPortal = true;
        }
        protected virtual int PortalWaitTime => 1;
        void HandlePortal()
        {
            if (inNetherPortal)
            {
                portalTime++;
                if (portalTime >= PortalWaitTime)
                {
                    portalTime = 0;
                    portalCooldown = 300;
                    Portals.TravelNether(this);
                }
            }
            else if (portalTime > 0) portalTime = Mathf.Max(0, portalTime - 4);
            inNetherPortal = false;
        }

        // ------------------------------------------------------------------ physics
        static readonly List<AABB> boxScratch = new List<AABB>();
        static readonly List<AABB> blockBoxes = new List<AABB>();

        public void CollectCollisions(AABB area, List<AABB> result, bool includeEntities = false)
        {
            result.Clear();
            int x0 = Mathf.FloorToInt(area.min.x), x1 = Mathf.FloorToInt(area.max.x);
            int y0 = Mathf.FloorToInt(area.min.y) - 1, y1 = Mathf.FloorToInt(area.max.y);
            int z0 = Mathf.FloorToInt(area.min.z), z1 = Mathf.FloorToInt(area.max.z);
            for (int x = x0; x <= x1; x++)
                for (int z = z0; z <= z1; z++)
                {
                    bool unloaded = world.IsUnloadedAt(x, Mathf.Clamp(y0 + 1, world.minY, world.maxY - 1), z);
                    for (int y = y0; y <= y1; y++)
                    {
                        if (unloaded && y >= world.minY && y < world.maxY)
                        {
                            result.Add(new AABB(x, y, z, x + 1, y + 1, z + 1));
                            continue;
                        }
                        ushort s = world.GetState(x, y, z);
                        if (s == 0) continue;
                        var b = Blocks.ByState[s];
                        if (!b.solid) continue;
                        if (b.opaqueCube)
                        {
                            var bb = new AABB(x, y, z, x + 1, y + 1, z + 1);
                            if (bb.Intersects(area)) result.Add(bb);
                            continue;
                        }
                        blockBoxes.Clear();
                        b.GetCollisionBoxes(s - b.baseState, world, new Int3(x, y, z), blockBoxes);
                        foreach (var lb in blockBoxes)
                        {
                            var wb = lb.Offset(x, y, z);
                            if (wb.Intersects(area)) result.Add(wb);
                        }
                    }
                }
            if (includeEntities)
            {
                foreach (var e in world.entities)
                {
                    if (e == this || e.removed || !e.IsSolidToOthers || e == vehicle || passengers.Contains(e)) continue;
                    var eb = e.Bounds;
                    if (eb.Intersects(area)) result.Add(eb);
                }
            }
        }

        public virtual bool IsSolidToOthers => false;
        protected virtual bool CollidesWithEntities => false;

        /// <summary>Move with collision. Returns the actual movement applied.</summary>
        public Vector3 Move(Vector3 delta)
        {
            if (noPhysics) { position += delta; return delta; }
            if (stuckSpeed != Vector3.one)
            {
                delta = Vector3.Scale(delta, stuckSpeed);
                stuckSpeed = Vector3.one;
                velocity = Vector3.zero;
            }
            Vector3 orig = delta;
            AABB box = Bounds;
            // sneak edge protection
            if (sneaking && onGround && this is Player)
            {
                float step = 0.05f;
                while (delta.x != 0 && !HasGroundBelow(box.Offset(delta.x, -stepHeightForEdge, 0)))
                {
                    if (Mathf.Abs(delta.x) < step) { delta.x = 0; break; }
                    delta.x -= Mathf.Sign(delta.x) * step;
                }
                while (delta.z != 0 && !HasGroundBelow(box.Offset(0, -stepHeightForEdge, delta.z)))
                {
                    if (Mathf.Abs(delta.z) < step) { delta.z = 0; break; }
                    delta.z -= Mathf.Sign(delta.z) * step;
                }
                while (delta.x != 0 && delta.z != 0 && !HasGroundBelow(box.Offset(delta.x, -stepHeightForEdge, delta.z)))
                {
                    if (Mathf.Abs(delta.x) < step) delta.x = 0; else delta.x -= Mathf.Sign(delta.x) * step;
                    if (Mathf.Abs(delta.z) < step) delta.z = 0; else delta.z -= Mathf.Sign(delta.z) * step;
                }
            }
            Vector3 res = Collide(box, delta, out bool clipX, out bool clipY, out bool clipZ);
            // step up
            if (stepHeight > 0 && (onGround || (orig.y < 0 && clipY)) && (clipX || clipZ))
            {
                Vector3 up = Collide(box, new Vector3(delta.x, stepHeight, delta.z), out _, out _, out _);
                Vector3 up2 = Collide(box, new Vector3(0, stepHeight, 0), out _, out _, out _);
                if (up2.y < stepHeight)
                {
                    Vector3 h = Collide(box.Offset(0, up2.y, 0), new Vector3(delta.x, 0, delta.z), out _, out _, out _);
                    h.y = up2.y;
                    if (h.x * h.x + h.z * h.z > up.x * up.x + up.z * up.z) up = h;
                }
                Vector3 down = Collide(box.Offset(up.x, up.y, up.z), new Vector3(0, -up.y + delta.y, 0), out _, out _, out _);
                Vector3 stepped = up + down;
                if (stepped.x * stepped.x + stepped.z * stepped.z > res.x * res.x + res.z * res.z + 1e-7f)
                {
                    res = stepped;
                    clipX = Mathf.Abs(res.x - delta.x) > 1e-5f; clipZ = Mathf.Abs(res.z - delta.z) > 1e-5f;
                    clipY = true;
                }
            }
            position += res;
            horizontalCollision = clipX || clipZ;
            verticalCollision = Mathf.Abs(res.y - orig.y) > 1e-5f;
            bool wasOnGround = onGround;
            onGround = orig.y < 0 && verticalCollision;
            collidedBelow = onGround;
            if (clipX) velocity.x = 0;
            if (clipZ) velocity.z = 0;
            if (verticalCollision) velocity.y = 0;
            // fall tracking
            if (onGround)
            {
                if (fallDistance > 0)
                {
                    Int3 below = Int3.Floor(position - new Vector3(0, 0.2f, 0));
                    var bb = world.GetBlock(below);
                    if (bb.isAir) { below = below.Offset(Dir.Down); bb = world.GetBlock(below); if (!(bb is FenceBlock || bb is WallBlock || bb is FenceGateBlock)) { below = below.Offset(Dir.Up); bb = world.GetBlock(below); } }
                    bb.OnFallenUpon(world, below, world.GetMeta(below), this, fallDistance);
                    fallDistance = 0;
                }
            }
            else if (res.y < 0) fallDistance -= res.y;
            // block interactions (inside / stepped on)
            CheckInsideBlocks();
            if (onGround)
            {
                Int3 below = Int3.Floor(position - new Vector3(0, 0.05f, 0));
                ushort s = world.GetState(below);
                if (s != 0) { var b = Blocks.ByState[s]; b.OnSteppedOn(world, below, s - b.baseState, this); }
            }
            return res;
        }
        float stepHeightForEdge => 0.6f;

        bool HasGroundBelow(AABB box)
        {
            CollectCollisions(box, boxScratch);
            foreach (var b in boxScratch) if (b.Intersects(box)) return true;
            return false;
        }

        Vector3 Collide(AABB box, Vector3 delta, out bool clipX, out bool clipY, out bool clipZ)
        {
            var area = box.Expand(delta);
            CollectCollisions(area, boxScratch, CollidesWithEntities);
            float dx = delta.x, dy = delta.y, dz = delta.z;
            float ody = dy;
            // AABB.Clip* is written from the obstacle's point of view (this = obstacle, argument = the moving box)
            foreach (var b in boxScratch) dy = b.ClipY(box, dy);
            box = box.Offset(0, dy, 0);
            bool xFirst = Mathf.Abs(dx) >= Mathf.Abs(dz);
            float odx = dx, odz = dz;
            if (xFirst)
            {
                foreach (var b in boxScratch) dx = b.ClipX(box, dx);
                box = box.Offset(dx, 0, 0);
                foreach (var b in boxScratch) dz = b.ClipZ(box, dz);
            }
            else
            {
                foreach (var b in boxScratch) dz = b.ClipZ(box, dz);
                box = box.Offset(0, 0, dz);
                foreach (var b in boxScratch) dx = b.ClipX(box, dx);
            }
            clipX = Mathf.Abs(dx - odx) > 1e-6f; clipY = Mathf.Abs(dy - ody) > 1e-6f; clipZ = Mathf.Abs(dz - odz) > 1e-6f;
            return new Vector3(dx, dy, dz);
        }

        public bool IsBoxFree(AABB box)
        {
            CollectCollisions(box, boxScratch);
            foreach (var b in boxScratch) if (b.Intersects(box)) return false;
            return true;
        }

        protected void CheckInsideBlocks()
        {
            var b = Bounds.Grow(-0.001f);
            int x0 = Mathf.FloorToInt(b.min.x), x1 = Mathf.FloorToInt(b.max.x);
            int y0 = Mathf.FloorToInt(b.min.y), y1 = Mathf.FloorToInt(b.max.y);
            int z0 = Mathf.FloorToInt(b.min.z), z1 = Mathf.FloorToInt(b.max.z);
            for (int x = x0; x <= x1; x++)
                for (int y = y0; y <= y1; y++)
                    for (int z = z0; z <= z1; z++)
                    {
                        ushort s = world.GetState(x, y, z);
                        if (s == 0) continue;
                        var bl = Blocks.ByState[s];
                        int m = s - bl.baseState;
                        var p = new Int3(x, y, z);
                        // only if actually intersecting selection shape for thin blocks
                        bl.OnEntityInside(world, p, m, this);
                        Vector3 st = bl.StuckSpeed(m);
                        if (st != Vector3.one) stuckSpeed = st;
                    }
            if (fireTicks > 0 && inWater) fireTicks = 0;
        }

        // ------------------------------------------------------------------ riding
        public virtual bool CanBeRiddenBy(Entity e) => false;
        public void StartRiding(Entity v)
        {
            if (vehicle != null) StopRiding();
            vehicle = v; v.passengers.Add(this);
        }
        public void StopRiding()
        {
            if (vehicle == null) return;
            var v = vehicle;
            v.passengers.Remove(this);
            vehicle = null;
            position = v.position + Vector3.up * (v.height + 0.1f);
            prevPosition = position;
            v.OnPassengerDismount(this);
        }
        public virtual void OnPassengerDismount(Entity e) { }
        public virtual Vector3 PassengerOffset(Entity p) => new Vector3(0, height * 0.75f, 0);
        public virtual void PositionRider(Entity p)
        {
            Vector3 off = Quaternion.Euler(0, yaw, 0) * PassengerOffset(p);
            p.position = position + off;
        }

        // ------------------------------------------------------------------ interaction / damage
        public virtual bool Hurt(DamageSource src, float amount) => false;
        public virtual bool Attackable => false;
        public virtual bool Interact(Player p, ItemStack held) => false;
        public virtual void OnStruckByLightning(LightningBolt bolt) { SetOnFire(8); }
        public virtual void Knockback(float strength, float dx, float dz)
        {
            if (strength <= 0) return;
            Vector3 d = new Vector3(dx, 0, dz);
            if (d.sqrMagnitude < 1e-6f) return;
            d = d.normalized * strength;
            velocity = new Vector3(velocity.x / 2f - d.x, onGround ? Mathf.Min(0.4f, velocity.y / 2f + strength) : velocity.y, velocity.z / 2f - d.z);
        }

        public void Teleport(Vector3 p) { position = p; prevPosition = p; velocity = Vector3.zero; fallDistance = 0; }

        // ------------------------------------------------------------------ rendering
        public virtual void Render(float partial)
        {
            if (go == null) return;
            go.transform.position = Vector3.LerpUnclamped(prevPosition, position, partial);
            go.transform.rotation = Quaternion.Euler(0, Mathf.LerpAngle(prevYaw, yaw, partial), 0);
        }

        public Vector3 InterpPos(float partial) => Vector3.LerpUnclamped(prevPosition, position, partial);

        // ------------------------------------------------------------------ persistence
        public virtual bool ShouldSave => !removed;
        public virtual void Save(Dictionary<string, string> d)
        {
            d["vx"] = velocity.x.ToString("R"); d["vy"] = velocity.y.ToString("R"); d["vz"] = velocity.z.ToString("R");
            if (fireTicks > 0) d["fire"] = fireTicks.ToString();
            if (customName != null) d["name"] = customName;
        }
        public virtual void Load(Dictionary<string, string> d)
        {
            if (d.TryGetValue("vx", out var vx)) float.TryParse(vx, out velocity.x);
            if (d.TryGetValue("vy", out var vy)) float.TryParse(vy, out velocity.y);
            if (d.TryGetValue("vz", out var vz)) float.TryParse(vz, out velocity.z);
            if (d.TryGetValue("fire", out var f)) int.TryParse(f, out fireTicks);
            if (d.TryGetValue("name", out var n)) customName = n;
        }
    }

    // =====================================================================================================
    public abstract class LivingEntity : Entity
    {
        public float health = 20, maxHealth = 20, absorption;
        public int hurtTime, hurtDuration = 10, invulTime, deathTime;
        public float lastHurtAmount;
        public bool dead;
        public readonly Dictionary<Effect, EffectInstance> effects = new Dictionary<Effect, EffectInstance>();
        public float moveForward, moveStrafe;
        public bool jumping;
        public int jumpCooldown;
        public float movementSpeed = 0.1f;
        public float attackDamageAttr = 1f;
        public float knockbackResistance;
        public float armorValueBase;
        public LivingEntity lastAttacker;
        public int lastAttackerTime;
        public float bodyYaw, prevBodyYaw, headYaw, prevHeadYaw;
        public float walkDist, prevWalkDist, walkAnimSpeed, prevWalkAnimSpeed, walkAnimPos;
        public float attackAnim, prevAttackAnim; public int swingTime; public bool swinging;
        public int air = 300, maxAir = 300;
        public bool sprinting;
        public ItemStack[] equipment = new ItemStack[6]; // 0 main 1 off 2 feet 3 legs 4 chest 5 head (mobs)
        public float[] dropChances = { 0.085f, 0.085f, 0.085f, 0.085f, 0.085f, 0.085f };
        public int noActionTime;
        public bool isFlying; // creative flight or natural flyer control
        public float flyingSpeed = 0.02f;
        public bool canBreatheUnderwater;
        public bool undead, arthropod;

        public float HealthFrac => Mathf.Clamp01(health / maxHealth);
        public virtual ItemStack MainHand { get => equipment[0]; set => equipment[0] = value; }
        public virtual ItemStack OffHand { get => equipment[1]; set => equipment[1] = value; }
        public virtual ItemStack GetArmor(int slot) => equipment[2 + slot];
        public bool IsAlive => !dead && !removed && health > 0;
        public override bool Attackable => true;
        public override bool IsSolidToOthers => false;

        public virtual int ArmorValue
        {
            get
            {
                float v = armorValueBase;
                for (int i = 0; i < 4; i++) { var a = GetArmor(i); if (a != null && a.item is ArmorItem ai) v += ai.defense; }
                return (int)v;
            }
        }
        public virtual float ArmorToughness
        {
            get
            {
                float v = 0;
                for (int i = 0; i < 4; i++) { var a = GetArmor(i); if (a != null && a.item is ArmorItem ai) v += ai.toughness; }
                return v;
            }
        }
        public int GetArmorEnchant(Enchant e)
        {
            int best = 0;
            for (int i = 0; i < 4; i++) { var a = GetArmor(i); if (a != null) best = Mathf.Max(best, a.GetEnchant(e)); }
            return best;
        }
        public int TotalArmorEnchant(Enchant e)
        {
            int t = 0;
            for (int i = 0; i < 4; i++) { var a = GetArmor(i); if (a != null) t += a.GetEnchant(e); }
            return t;
        }
        public int ApplyFireProtection(int ticks)
        {
            int fp = TotalArmorEnchant(Enchant.FireProtection);
            if (fp > 0) ticks -= Mathf.FloorToInt(ticks * fp * 0.15f);
            return ticks;
        }

        // ------------------------------------------------------------------ effects
        public int GetEffectLevel(Effect e) => effects.TryGetValue(e, out var inst) ? inst.amplifier : -1;
        public bool HasEffect(Effect e) => effects.ContainsKey(e);
        public virtual void AddEffect(EffectInstance inst)
        {
            if (inst == null) return;
            if (undead && (inst.effect == Effect.Regeneration || inst.effect == Effect.Poison)) return;
            if (inst.effect == Effect.InstantHealth) { if (undead) Hurt(DamageSource.Magic, 6 << inst.amplifier); else Heal(4 << inst.amplifier); return; }
            if (inst.effect == Effect.InstantDamage) { if (undead) Heal(4 << inst.amplifier); else Hurt(DamageSource.Magic, 6 << inst.amplifier); return; }
            if (inst.effect == Effect.Saturation && this is Player sp) { sp.hunger.Eat(inst.amplifier + 1, 1f); return; }
            if (effects.TryGetValue(inst.effect, out var cur))
            {
                if (inst.amplifier > cur.amplifier || (inst.amplifier == cur.amplifier && inst.duration > cur.duration)) effects[inst.effect] = inst;
            }
            else effects[inst.effect] = inst;
            if (inst.effect == Effect.Absorption) absorption = Mathf.Max(absorption, 4 * (inst.amplifier + 1));
            if (inst.effect == Effect.HealthBoost) maxHealth = BaseMaxHealth + 4 * (inst.amplifier + 1);
        }
        public virtual float BaseMaxHealth => 20f;
        public void RemoveEffect(Effect e)
        {
            effects.Remove(e);
            if (e == Effect.Absorption) absorption = 0;
            if (e == Effect.HealthBoost) { maxHealth = BaseMaxHealth; health = Mathf.Min(health, maxHealth); }
        }
        public void ClearEffects() { foreach (var e in new List<Effect>(effects.Keys)) RemoveEffect(e); }
        static readonly List<Effect> expired = new List<Effect>();
        void TickEffects()
        {
            if (effects.Count == 0) return;
            expired.Clear();
            foreach (var kv in effects)
            {
                var inst = kv.Value;
                ApplyEffectTick(inst);
                if (inst.duration < 20 * 60 * 60 * 24 * 365) inst.duration--;
                if (inst.duration <= 0) expired.Add(kv.Key);
                if (inst.showParticles && world != null && age % 4 == 0 && !(this is Player p && p.IsLocal && !p.ThirdPerson))
                    Particles.Effect(world, position + new Vector3(Random.Range(-width, width) * 0.5f, Random.Range(0, height), Random.Range(-width, width) * 0.5f), inst.effect.color, inst.ambient);
            }
            foreach (var e in expired) RemoveEffect(e);
        }
        void ApplyEffectTick(EffectInstance inst)
        {
            var e = inst.effect; int a = inst.amplifier;
            if (e == Effect.Regeneration) { int iv = 50 >> a; if (iv < 1 || inst.duration % iv == 0) if (health < maxHealth) Heal(1); }
            else if (e == Effect.Poison) { int iv = 25 >> a; if ((iv < 1 || inst.duration % iv == 0) && health > 1) Hurt(DamageSource.Magic, 1); }
            else if (e == Effect.Wither) { int iv = 40 >> a; if (iv < 1 || inst.duration % iv == 0) Hurt(DamageSource.WitherEffect, 1); }
            else if (e == Effect.Hunger && this is Player p) p.hunger.AddExhaustion(0.005f * (a + 1));
        }

        public float SpeedMultiplier
        {
            get
            {
                float m = 1f;
                int sp = GetEffectLevel(Effect.Speed); if (sp >= 0) m *= 1f + 0.2f * (sp + 1);
                int sl = GetEffectLevel(Effect.Slowness); if (sl >= 0) m *= Mathf.Max(0, 1f - 0.15f * (sl + 1));
                return m;
            }
        }

        // ------------------------------------------------------------------ health
        public virtual void Heal(float amount)
        {
            if (dead) return;
            health = Mathf.Min(maxHealth, health + amount);
        }

        protected virtual bool IsInvulnerableTo(DamageSource src)
        {
            if (invulnerable && !src.bypassInvul) return true;
            if (src.isFire && (fireImmune || GetEffectLevel(Effect.FireResistance) >= 0)) return true;
            return false;
        }

        public override bool Hurt(DamageSource src, float amount)
        {
            if (dead || removed || world == null) return false;
            if (IsInvulnerableTo(src)) return false;
            if (src.scalesWithDifficulty && this is Player && world.session != null)
            {
                switch (world.session.difficulty)
                {
                    case Difficulty.Peaceful: amount = 0; break;
                    case Difficulty.Easy: amount = Mathf.Min(amount / 2f + 1f, amount); break;
                    case Difficulty.Hard: amount *= 1.5f; break;
                }
                if (amount <= 0) return false;
            }
            if (src.isFall) { int ff = GetArmorEnchant(Enchant.FeatherFalling); }
            // shield blocking
            if (this is Player bp && bp.IsBlocking && !src.bypassArmor && (src.direct != null || src.sourcePos.HasValue))
            {
                Vector3 from = src.sourcePos ?? src.direct.position;
                Vector3 to = from - position; to.y = 0;
                if (Vector3.Dot(to.normalized, bp.LookDir) > 0.0f)
                {
                    bp.OnShieldBlock(src, amount);
                    return false;
                }
            }
            bool fresh = true;
            if (invulTime > hurtDuration / 2f)
            {
                if (amount <= lastHurtAmount) return false;
                float extra = amount - lastHurtAmount;
                lastHurtAmount = amount;
                amount = extra;
                fresh = false;
            }
            else
            {
                lastHurtAmount = amount;
                invulTime = 20;
                hurtTime = hurtDuration;
            }
            amount = ApplyArmor(src, amount);
            amount = ApplyMagicReduction(src, amount);
            float absorbed = Mathf.Min(absorption, amount);
            absorption -= absorbed; amount -= absorbed;
            if (src.attacker is LivingEntity att) { lastAttacker = att; lastAttackerTime = age; }
            if (fresh)
            {
                if (src.direct != null && !src.isExplosion)
                {
                    Vector3 d = src.direct.position - position;
                    if (src.attacker != null && src.direct == src.attacker) d = src.attacker.position - position;
                    Knockback(0.4f * (1f - knockbackResistance), d.x, d.z);
                }
                OnHurtEffects(src);
            }
            if (amount > 0) health -= amount;
            if (health <= 0) { health = 0; Die(src); }
            return true;
        }

        protected virtual void OnHurtEffects(DamageSource src)
        {
            string snd = HurtSound;
            if (snd != null) Sounds.Play(snd, position, 1f, VoicePitch);
        }
        public virtual string HurtSound => "entity.generic.hurt";
        public virtual string DeathSound => "entity.generic.death";
        public virtual float VoicePitch => (Random.value - Random.value) * 0.2f + 1f;

        float ApplyArmor(DamageSource src, float amount)
        {
            if (src.bypassArmor) return amount;
            float armor = ArmorValue, tough = ArmorToughness;
            float f = Mathf.Clamp(armor - amount / (2f + tough / 4f), armor * 0.2f, 20f);
            float result = amount * (1f - f / 25f);
            DamageArmor(amount);
            return result;
        }
        protected virtual void DamageArmor(float amount) { }
        float ApplyMagicReduction(DamageSource src, float amount)
        {
            int res = GetEffectLevel(Effect.Resistance);
            if (res >= 0 && src != DamageSource.Void) amount *= Mathf.Max(0, 1f - (res + 1) * 0.2f);
            if (amount <= 0) return 0;
            if (src.bypassArmor && !src.isFall) return amount;
            // enchantment protection factor
            int epf = 0;
            for (int i = 0; i < 4; i++)
            {
                var a = GetArmor(i); if (a == null) continue;
                epf += a.GetEnchant(Enchant.Protection);
                if (src.isFire) epf += a.GetEnchant(Enchant.FireProtection) * 2;
                if (src.isExplosion) epf += a.GetEnchant(Enchant.BlastProtection) * 2;
                if (src.isProjectile) epf += a.GetEnchant(Enchant.ProjectileProtection) * 2;
                if (src.isFall) epf += a.GetEnchant(Enchant.FeatherFalling) * 3;
            }
            epf = Mathf.Min(20, epf);
            return amount * (1f - epf / 25f);
        }

        public virtual void Die(DamageSource src)
        {
            if (dead) return;
            dead = true;
            deathTime = 0;
            Sounds.Play(DeathSound, position, 1f, VoicePitch);
            if (world != null) OnDeath(src);
        }
        protected virtual void OnDeath(DamageSource src) { }

        // ------------------------------------------------------------------ tick
        public override void Tick()
        {
            prevBodyYaw = bodyYaw; prevHeadYaw = headYaw;
            prevWalkAnimSpeed = walkAnimSpeed; prevAttackAnim = attackAnim; prevWalkDist = walkDist;
            base.Tick();
            if (hurtTime > 0) hurtTime--;
            if (invulTime > 0) invulTime--;
            if (jumpCooldown > 0) jumpCooldown--;
            if (dead)
            {
                deathTime++;
                if (deathTime >= 20 && !(this is Player)) { Particles.Poof(world, position + Vector3.up * height * 0.5f, width, height); Remove(); }
                return;
            }
            TickEffects();
            TickAir();
            if (IsInsideWall() && !(this is Player pp && (pp.IsCreative || pp.IsSpectator))) Hurt(DamageSource.Suffocate, 1f);
            AiStep();
            UpdateSwing();
            // walk animation
            float dx = position.x - prevPosition.x, dz = position.z - prevPosition.z;
            float dist = Mathf.Sqrt(dx * dx + dz * dz) * 4f;
            if (dist > 1f) dist = 1f;
            walkAnimSpeed += (dist - walkAnimSpeed) * 0.4f;
            walkAnimPos += walkAnimSpeed;
            walkDist += Mathf.Sqrt(dx * dx + dz * dz) * 0.6f;
            // body yaw follows movement
            if (dx * dx + dz * dz > 0.0025f)
            {
                float moveYaw = Mathf.Atan2(dx, dz) * Mathf.Rad2Deg;
                bodyYaw = MathX.ApproachAngle(bodyYaw, moveYaw, 20f);
            }
            float diff = MathX.WrapAngle(yaw - bodyYaw);
            if (diff > 50) bodyYaw = yaw - 50; else if (diff < -50) bodyYaw = yaw + 50;
            headYaw = yaw;
        }

        public override void RideTick()
        {
            prevBodyYaw = bodyYaw; prevHeadYaw = headYaw; prevWalkAnimSpeed = walkAnimSpeed; prevAttackAnim = attackAnim; prevWalkDist = walkDist;
            base.RideTick();
            if (hurtTime > 0) hurtTime--;
            if (invulTime > 0) invulTime--;
            if (dead) { deathTime++; return; }
            TickEffects();
            TickAir();
            UpdateSwing();
            walkAnimSpeed *= 0.8f;
            headYaw = yaw;
        }

        void TickAir()
        {
            if (canBreatheUnderwater) { if (!eyeInWater && !(this is Mob mm && mm.def.aquatic)) { } air = maxAir; return; }
            if (eyeInWater && !(this is Player p && (p.IsCreative || p.IsSpectator)) && GetEffectLevel(Effect.WaterBreathing) < 0 && GetEffectLevel(Effect.ConduitPower) < 0)
            {
                int resp = GetArmorEnchant(Enchant.Respiration);
                if (resp == 0 || Random.value < 1f / (resp + 1)) air--;
                if (air <= -20)
                {
                    air = 0;
                    Particles.Bubble(world, EyePosition, false, 8);
                    Hurt(DamageSource.Drown, 2f);
                }
            }
            else if (air < maxAir) air = Mathf.Min(maxAir, air + 4);
        }

        public bool IsInsideWall()
        {
            if (noPhysics) return false;
            Vector3 e = EyePosition;
            float r = width * 0.8f * 0.5f;
            for (int i = 0; i < 8; i++)
            {
                float x = e.x + (((i >> 0) & 1) * 2 - 1) * r;
                float y = e.y + (((i >> 1) & 1) * 2 - 1) * 0.1f;
                float z = e.z + (((i >> 2) & 1) * 2 - 1) * r;
                ushort s = world.GetState(Mathf.FloorToInt(x), Mathf.FloorToInt(y), Mathf.FloorToInt(z));
                if (Blocks.StateOpaque[s] && Blocks.ByState[s].solid) return true;
            }
            return false;
        }

        public void SwingArm()
        {
            if (!swinging || swingTime >= 3 || swingTime < 0) { swingTime = -1; swinging = true; }
        }
        void UpdateSwing()
        {
            int dur = 6;
            int haste = GetEffectLevel(Effect.Haste);
            if (haste >= 0) dur -= 1 + haste;
            int fat = GetEffectLevel(Effect.MiningFatigue);
            if (fat >= 0) dur += (1 + fat) * 2;
            if (swinging) { swingTime++; if (swingTime >= dur) { swingTime = 0; swinging = false; } }
            else swingTime = 0;
            attackAnim = (float)swingTime / dur;
        }

        /// <summary>AI + movement. Subclasses set moveForward/moveStrafe/jumping before calling base.</summary>
        protected virtual void AiStep()
        {
            if (jumping && jumpCooldown == 0)
            {
                if (inWater || inLava) velocity.y += 0.04f;
                else if (onGround) { Jump(); jumpCooldown = 10; }
            }
            if (!jumping) jumpCooldown = 0;
            Travel(moveStrafe, moveForward);
        }

        public virtual float JumpPower => 0.42f * world.GetBlock(Int3.Floor(position - Vector3.up * 0.1f)).jumpFactor;
        public virtual void Jump()
        {
            float jp = JumpPower;
            int jb = GetEffectLevel(Effect.JumpBoost);
            if (jb >= 0) jp += 0.1f * (jb + 1);
            velocity.y = jp;
            if (sprinting)
            {
                float r = yaw * Mathf.Deg2Rad;
                velocity.x += Mathf.Sin(r) * 0.2f; velocity.z += Mathf.Cos(r) * 0.2f;
            }
        }

        protected virtual float Gravity => noGravity ? 0f : 0.08f;
        public virtual bool OnClimbable()
        {
            var b = world.GetBlock(BlockPos);
            if (b.climbable) return true;
            if (b is TrapdoorBlock && (world.GetMeta(BlockPos) & 4) != 0 && world.GetBlock(BlockPos.Offset(Dir.Down)) is LadderBlock) return true;
            return false;
        }

        public void MoveRelative(float accel, float strafe, float forward)
        {
            float len = strafe * strafe + forward * forward;
            if (len < 1e-7f) return;
            len = Mathf.Sqrt(len);
            if (len < 1f) len = 1f;
            strafe = strafe / len * accel; forward = forward / len * accel;
            float r = yaw * Mathf.Deg2Rad;
            float s = Mathf.Sin(r), c = Mathf.Cos(r);
            velocity.x += forward * s + strafe * c;
            velocity.z += forward * c - strafe * s;
        }

        public virtual void Travel(float strafe, float forward)
        {
            float speed = movementSpeed * SpeedMultiplier;
            if (sprinting) speed *= 1.3f;
            if (sneaking && onGround) { strafe *= 0.3f; forward *= 0.3f; }
            if (isFlying)
            {
                MoveRelative(flyingSpeed * (sprinting ? 2f : 1f) * SpeedMultiplier, strafe, forward);
                Move(velocity);
                velocity.x *= 0.91f; velocity.z *= 0.91f; velocity.y *= 0.6f;
                fallDistance = 0;
                return;
            }
            if (inWater && !IsAquaticSwimmer)
            {
                float slow = sprinting ? 0.9f : 0.8f;
                float acc = 0.02f;
                int ds = GetArmorEnchant(Enchant.DepthStrider);
                if (ds > 0)
                {
                    float f = Mathf.Min(3, ds) / 3f;
                    if (!onGround) f *= 0.5f;
                    slow += (0.546f - slow) * f;
                    acc += (speed - acc) * f;
                }
                if (GetEffectLevel(Effect.DolphinsGrace) >= 0) slow = 0.96f;
                MoveRelative(acc, strafe, forward);
                float oy = position.y;
                Move(velocity);
                velocity.x *= slow; velocity.z *= slow; velocity.y *= 0.8f;
                if (!noGravity) velocity.y -= 0.02f;
                if (horizontalCollision && IsFreeAt(velocity.x, velocity.y + 0.6f - position.y + oy, velocity.z)) velocity.y = 0.3f;
                return;
            }
            if (inLava)
            {
                MoveRelative(0.02f, strafe, forward);
                Move(velocity);
                velocity *= 0.5f;
                if (!noGravity) velocity.y -= 0.02f;
                return;
            }
            float slip = 0.91f;
            if (onGround)
            {
                var below = world.GetBlock(Int3.Floor(position - new Vector3(0, 0.5f, 0)));
                slip = below.slipperiness * 0.91f;
            }
            float accel = onGround ? speed * (0.21600002f / (slip * slip * slip)) : (sprinting ? 0.026f : 0.02f) * (this is Player ? 1f : speed / 0.1f * 0.9f + 0.1f);
            if (!onGround && !(this is Player)) accel = 0.02f;
            MoveRelative(accel, strafe, forward);
            if (OnClimbable())
            {
                velocity.x = Mathf.Clamp(velocity.x, -0.15f, 0.15f);
                velocity.z = Mathf.Clamp(velocity.z, -0.15f, 0.15f);
                if (velocity.y < -0.15f) velocity.y = -0.15f;
                if (sneaking && velocity.y < 0) velocity.y = 0;
                fallDistance = 0;
            }
            Move(velocity);
            if ((horizontalCollision || jumping) && OnClimbable()) velocity.y = 0.2f;
            int lev = GetEffectLevel(Effect.Levitation);
            if (lev >= 0) velocity.y += (0.05f * (lev + 1) - velocity.y) * 0.2f;
            else if (!noGravity)
            {
                float g = Gravity;
                if (velocity.y <= 0 && GetEffectLevel(Effect.SlowFalling) >= 0) { g = 0.01f; fallDistance = 0; }
                velocity.y -= g;
            }
            velocity.y *= 0.98f;
            velocity.x *= slip; velocity.z *= slip;
        }

        protected virtual bool IsAquaticSwimmer => false;

        bool IsFreeAt(float dx, float dy, float dz) => IsBoxFree(Bounds.Offset(dx, dy, dz));

        public override void ApplyFallDamage(float dist)
        {
            if (this is Player p && (p.IsCreative || p.IsSpectator || p.abilities.flying)) return;
            int jb = GetEffectLevel(Effect.JumpBoost);
            float dmg = Mathf.Ceil(dist - 3f - (jb >= 0 ? jb + 1 : 0));
            var below = world.GetBlock(Int3.Floor(position - new Vector3(0, 0.2f, 0)));
            if (below.id == "hay_block") dmg = Mathf.Ceil(dmg * 0.2f);
            if (below.id == "slime_block" && !sneaking) return;
            if (below.id == "honey_block") dmg = Mathf.Ceil(dmg * 0.2f);
            if (below.id.EndsWith("_bed")) dmg = Mathf.Ceil(dmg * 0.5f);
            if (below.id == "pointed_dripstone") dmg = Mathf.Ceil((dist - 2) * 2);
            if (dmg > 0)
            {
                Sounds.Play(dmg > 4 ? "entity.generic.big_fall" : "entity.generic.small_fall", position, 1f, 1f);
                Hurt(DamageSource.Fall, dmg);
                Particles.BlockDust(world, position, world.GetState(Int3.Floor(position - new Vector3(0, 0.2f, 0))), (int)Mathf.Min(30, dmg * 3));
            }
        }

        public void KillFromCommand() { invulnerable = false; Hurt(DamageSource.Kill, float.MaxValue); }

        public override void Save(Dictionary<string, string> d)
        {
            base.Save(d);
            d["hp"] = health.ToString("R");
            if (effects.Count > 0)
            {
                var list = new List<string>();
                foreach (var e in effects.Values) list.Add(e.Serialize());
                d["fx"] = string.Join(",", list);
            }
            for (int i = 0; i < equipment.Length; i++) if (equipment[i] != null && !equipment[i].IsEmpty) d["eq" + i] = equipment[i].Serialize();
        }
        public override void Load(Dictionary<string, string> d)
        {
            base.Load(d);
            if (d.TryGetValue("hp", out var hp)) float.TryParse(hp, out health);
            if (d.TryGetValue("fx", out var fx))
                foreach (var s in fx.Split(',')) { var inst = EffectInstance.Parse(s); if (inst != null) effects[inst.effect] = inst; }
            for (int i = 0; i < equipment.Length; i++) if (d.TryGetValue("eq" + i, out var eq)) equipment[i] = ItemStack.Deserialize(eq);
        }
    }
}
