using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Base projectile with swept block/entity collision.</summary>
    public abstract class Projectile : Entity
    {
        public Entity owner;
        public int ownerId = -1;
        public float gravity = 0.03f, drag = 0.99f, waterDrag = 0.8f;
        protected bool leftOwner;
        protected Renderer[] rends;
        public override bool ShouldSave => false;

        protected Projectile() { width = 0.25f; height = 0.25f; blocksBuilding = false; }

        public void Launch(Vector3 dir, float speed, float inaccuracy)
        {
            dir.Normalize();
            dir += new Vector3(Gauss() * 0.0075f * inaccuracy, Gauss() * 0.0075f * inaccuracy, Gauss() * 0.0075f * inaccuracy);
            velocity = dir * speed;
            yaw = prevYaw = MathX.YawFromDir(velocity);
            pitch = prevPitch = MathX.PitchFromDir(velocity);
        }
        static float Gauss() { float u1 = 1f - Random.value, u2 = Random.value; return Mathf.Sqrt(-2f * Mathf.Log(u1)) * Mathf.Sin(2f * Mathf.PI * u2); }

        public void ShootFrom(LivingEntity shooter, float speed, float inaccuracy, float yawOffset = 0)
        {
            owner = shooter; ownerId = shooter.id; world = shooter.world;
            SetPosition(shooter.EyePosition - new Vector3(0, 0.1f, 0));
            Launch(MathX.YawPitchToDir(shooter.yaw + yawOffset, shooter.pitch), speed, inaccuracy);
            // inherit shooter horizontal velocity
            velocity += new Vector3(shooter.velocity.x, shooter.onGround ? 0 : shooter.velocity.y, shooter.velocity.z);
        }

        public static Projectile Create(string id, World w)
        {
            Projectile p = null;
            switch (id)
            {
                case "snowball": p = new ThrownItemProjectile("snowball"); break;
                case "egg": p = new ThrownItemProjectile("egg"); break;
                case "ender_pearl": p = new ThrownItemProjectile("ender_pearl"); break;
                case "experience_bottle": p = new ThrownItemProjectile("experience_bottle"); break;
                case "wind_charge": p = new WindChargeProjectile(); break;
            }
            if (p != null) p.world = w;
            return p;
        }

        public static void ThrowFrom(World w, Player p, string id, ItemStack s)
        {
            var pr = Create(id, w);
            if (pr == null) return;
            pr.ShootFrom(p, id == "wind_charge" ? 1.5f : id == "experience_bottle" ? 0.7f : 1.5f, 1f);
            if (id == "experience_bottle") pr.velocity.y += 0.2f;
            w.AddEntity(pr);
        }

        public override void Tick()
        {
            base.Tick();
            if (removed) return;
            if (owner == null && ownerId >= 0) owner = world.entities.Find(e => e.id == ownerId);
            if (!leftOwner && owner != null && !Bounds.Grow(0.3f).Intersects(owner.Bounds)) leftOwner = true;
            TickFlight();
        }

        protected virtual void TickFlight()
        {
            Vector3 from = position, to = position + velocity;
            bool hitBlock = world.ClipCollision(from, to, out var bh);
            if (hitBlock) to = bh.point;
            Entity hitEnt = FindEntityHit(from, to);
            if (hitEnt != null) { OnHitEntity(hitEnt); if (removed) return; }
            else if (hitBlock) { OnHitBlock(bh); if (removed) return; }
            if (!hitBlock || hitEnt != null) position += velocity; else position = bh.point - velocity.normalized * 0.05f;
            float d = inWater ? waterDrag : drag;
            velocity *= d;
            velocity.y -= gravity;
            if (velocity.sqrMagnitude > 1e-6f)
            {
                yaw = MathX.YawFromDir(velocity);
                pitch = MathX.PitchFromDir(velocity);
            }
            if (inWater && age % 3 == 0) Particles.Bubble(world, position, false, 1);
            if (age > 1200) Remove();
        }

        protected Entity FindEntityHit(Vector3 from, Vector3 to)
        {
            Entity best = null; float bestT = float.MaxValue;
            Vector3 d = to - from; float len = d.magnitude;
            if (len < 1e-5f) return null;
            var area = new AABB(Vector3.Min(from, to), Vector3.Max(from, to)).Grow(1f);
            foreach (var e in world.GetEntities(area, this))
            {
                if (!CanHit(e)) continue;
                var b = e.Bounds.Grow(0.3f);
                if (b.Raycast(from, d / len, len, out float t, out _) && t < bestT) { bestT = t; best = e; }
                else if (b.Contains(from)) { bestT = 0; best = e; }
            }
            return best;
        }

        protected virtual bool CanHit(Entity e)
        {
            if (e.removed || e is Projectile || e is ItemEntity || e is XpOrb || e is AreaEffectCloud || e is LightningBolt) return false;
            if (e == owner && !leftOwner) return false;
            if (e is LivingEntity le && le.dead) return false;
            if (e is Player p && p.IsSpectator) return false;
            return e.Attackable || e is EndCrystal || e is Boat || e is Minecart;
        }

        protected virtual void OnHitEntity(Entity e) { Remove(); }
        protected virtual void OnHitBlock(BlockHit hit)
        {
            var b = Blocks.ByState[hit.state];
            b.OnProjectileHit(world, hit.pos, hit.state - b.baseState, this);
            Remove();
        }

        public override void CreateVisual() { }
        public override void Render(float partial)
        {
            if (go == null) return;
            go.transform.position = InterpPos(partial);
            go.transform.rotation = Quaternion.Euler(Mathf.LerpAngle(prevPitch, pitch, partial), Mathf.LerpAngle(prevYaw, yaw, partial), 0);
            if (rends != null) EntityLight.ApplyRenderers(rends, EntityLight.Sample(world, InterpPos(partial)), 0, Vector4.zero);
        }

        protected void MakeItemVisual(string itemId, float scale, bool billboard = true)
        {
            var st = new ItemStack(itemId, 1);
            if (st.IsEmpty) return;
            go = ItemRender.CreateSpriteVisual(st, scale);
            rends = go != null ? go.GetComponentsInChildren<Renderer>() : null;
            this.billboard = billboard;
        }
        protected bool billboard;
        protected void RenderBillboard(float partial)
        {
            if (go == null) return;
            go.transform.position = InterpPos(partial);
            var cam = GameManager.MainCamera;
            if (cam != null) go.transform.rotation = Quaternion.LookRotation(go.transform.position - cam.transform.position);
            if (rends != null) EntityLight.ApplyRenderers(rends, EntityLight.Sample(world, InterpPos(partial)), 0, Vector4.zero);
        }
    }

    // =====================================================================================================
    public sealed class Arrow : Projectile
    {
        public enum Pickup { Disallowed, Allowed, CreativeOnly }
        public Pickup pickup = Pickup.Disallowed;
        public float damage = 2f;
        public bool crit;
        public int punch, piercing;
        public string ammoId = "arrow";
        public string potion;
        public bool inGround; Int3 stuckPos; ushort stuckState; int groundTime;
        int pierced;
        public int shakeTime;
        public override string TypeId => "arrow";
        public override bool ShouldSave => !removed && inGround;

        public Arrow() { width = 0.5f; height = 0.5f; gravity = 0.05f; drag = 0.99f; waterDrag = 0.6f; }

        public static Arrow Shoot(World w, LivingEntity shooter, float power, float inaccuracy, string ammoId, ItemStack ammo, float spread = 0)
        {
            var a = new Arrow { world = w, ammoId = ammoId ?? "arrow" };
            a.ShootFrom(shooter, power, inaccuracy, spread);
            if (shooter is Player p) a.pickup = p.IsCreative ? Pickup.CreativeOnly : Pickup.Allowed;
            if (ammo != null && ammo.item.id == "tipped_arrow") a.potion = ammo.Get("potion");
            w.AddEntity(a);
            return a;
        }

        protected override void TickFlight()
        {
            if (inGround)
            {
                if (world.GetState(stuckPos) != stuckState) { inGround = false; velocity = new Vector3(Random.Range(-0.02f, 0.02f), 0, Random.Range(-0.02f, 0.02f)); }
                else
                {
                    if (++groundTime >= 1200) Remove();
                    return;
                }
            }
            if (crit && age % 2 == 0) Particles.Crit(world, position, 1);
            if (potion != null && age % 5 == 0) { var pt = Potions.Get(potion); if (pt != null) Particles.Effect(world, position, pt.color, false); }
            base.TickFlight();
        }

        protected override void OnHitEntity(Entity e)
        {
            float speed = velocity.magnitude;
            int dmg = Mathf.CeilToInt(Mathf.Clamp(speed * damage, 0, int.MaxValue));
            if (crit) dmg += Random.Range(0, dmg / 2 + 2);
            if (e is EndCrystal ec) { ec.Hurt(DamageSource.ProjectileHit(this, owner), dmg); Remove(); return; }
            if (e is Mob m && m.def.id == "enderman") { ((Mob)e).OnArrowDodge(); return; }
            var src = DamageSource.ProjectileHit(this, owner);
            if (onFire && !(e is Mob mm && mm.def.fireImmune)) e.SetOnFire(5);
            if (e.Hurt(src, dmg))
            {
                if (e is LivingEntity le)
                {
                    if (punch > 0)
                    {
                        var hv = new Vector3(velocity.x, 0, velocity.z).normalized * punch * 0.6f;
                        le.velocity += new Vector3(hv.x, 0.1f, hv.z);
                    }
                    if (potion != null) { var pt = Potions.Get(potion); if (pt != null) foreach (var fx in pt.effects) { var c = fx.Copy(); c.duration = Mathf.Max(1, c.duration / 8); le.AddEffect(c); } }
                    if (ammoId == "spectral_arrow") le.AddEffect(new EffectInstance(Effect.Glowing, 200));
                    if (owner is Player op && e is Player == false) Sounds.Play("entity.arrow.hit_player", op.position, 0.18f, 0.45f);
                }
                Sounds.Play("entity.arrow.hit", position, 1f, 1.2f / (Random.value * 0.2f + 0.9f));
                if (piercing > 0 && ++pierced <= piercing) return;
                Remove();
            }
            else
            {
                velocity *= -0.1f; yaw += 180;
            }
        }

        protected override void OnHitBlock(BlockHit hit)
        {
            var b = Blocks.ByState[hit.state];
            b.OnProjectileHit(world, hit.pos, hit.state - b.baseState, this);
            inGround = true; stuckPos = hit.pos; stuckState = hit.state;
            position = hit.point - velocity.normalized * 0.05f;
            velocity = Vector3.zero; crit = false; shakeTime = 7; groundTime = 0;
            Sounds.Play("entity.arrow.hit", position, 1f, 1.2f / (Random.value * 0.2f + 0.9f));
            piercing = 0;
        }

        public void TryPickup(Player p)
        {
            if (removed || !inGround || shakeTime > 0 && age < 5) return;
            if (pickup == Pickup.Disallowed) return;
            if (pickup == Pickup.CreativeOnly) { if (p.IsCreative) Remove(); return; }
            var st = new ItemStack(ammoId, 1);
            if (potion != null) st.Set("potion", potion);
            if (p.inventory.Add(st)) { Sounds.Play("entity.item.pickup", position, 0.2f, 2f); Remove(); }
        }

        public override void Tick() { if (shakeTime > 0) shakeTime--; base.Tick(); }

        public override void CreateVisual()
        {
            go = ItemRender.CreateArrowVisual(ammoId);
            rends = go != null ? go.GetComponentsInChildren<Renderer>() : null;
        }
        public override void Save(Dictionary<string, string> d)
        {
            base.Save(d);
            d["ammo"] = ammoId; d["pickup"] = ((int)pickup).ToString(); d["g"] = inGround ? "1" : "0";
            d["sp"] = stuckPos.x + "," + stuckPos.y + "," + stuckPos.z; if (potion != null) d["potion"] = potion;
        }
        public override void Load(Dictionary<string, string> d)
        {
            base.Load(d);
            if (d.TryGetValue("ammo", out var a)) ammoId = a;
            if (d.TryGetValue("pickup", out var pk)) pickup = (Pickup)int.Parse(pk);
            inGround = d.TryGetValue("g", out var g) && g == "1";
            if (d.TryGetValue("sp", out var sp)) { var p = sp.Split(','); stuckPos = new Int3(int.Parse(p[0]), int.Parse(p[1]), int.Parse(p[2])); stuckState = 0; }
            d.TryGetValue("potion", out potion);
            if (inGround) groundTime = 0;
        }
    }

    // =====================================================================================================
    public sealed class ThrownTrident : Projectile
    {
        public ItemStack stack;
        bool dealtDamage, returning;
        int returnTimer;
        bool inGround;
        public override string TypeId => "trident";
        public override bool ShouldSave => !removed && inGround;
        public ThrownTrident() { width = 0.5f; height = 0.5f; gravity = 0.05f; drag = 0.99f; waterDrag = 0.99f; }

        public static ThrownTrident Throw(World w, Player p, ItemStack s)
        {
            var t = new ThrownTrident { world = w, stack = s };
            t.ShootFrom(p, 2.5f, 1f);
            w.AddEntity(t);
            return t;
        }

        protected override void TickFlight()
        {
            int loyalty = stack?.GetEnchant(Enchant.Loyalty) ?? 0;
            if ((dealtDamage || inGround) && loyalty > 0 && owner != null && !owner.removed)
            {
                returning = true; inGround = false; noPhysics = true;
                Vector3 target = owner.EyePosition - position;
                position += target.normalized * Mathf.Min(target.magnitude, 0.05f * loyalty * 20f / 20f + 0.3f);
                velocity = target.normalized * 0.05f * loyalty;
                if (returnTimer++ == 0) Sounds.Play("item.trident.return", position, 10f, 1f);
                if (target.sqrMagnitude < 1.5f && owner is Player pl) TryPickup(pl);
                return;
            }
            if (inGround) return;
            base.TickFlight();
        }

        protected override void OnHitEntity(Entity e)
        {
            if (dealtDamage) return;
            float dmg = 8f;
            if (e is LivingEntity le && (le is Mob m && m.def.aquatic)) dmg += 2.5f * (stack?.GetEnchant(Enchant.Impaling) ?? 0);
            if (e.Hurt(DamageSource.ProjectileHit(this, owner, "trident"), dmg)) Sounds.Play("item.trident.hit", position, 1f, 1f);
            dealtDamage = true;
            velocity = new Vector3(-velocity.x * 0.01f, -0.1f, -velocity.z * 0.01f);
            if ((stack?.GetEnchant(Enchant.Channeling) ?? 0) > 0 && world.session != null && world.session.IsThundering && world.CanSeeSky(Int3.Floor(e.position)))
                LightningBolt.Strike(world, e.position);
        }

        protected override void OnHitBlock(BlockHit hit)
        {
            var b = Blocks.ByState[hit.state];
            b.OnProjectileHit(world, hit.pos, hit.state - b.baseState, this);
            inGround = true;
            position = hit.point - velocity.normalized * 0.1f;
            velocity = Vector3.zero;
            Sounds.Play("item.trident.hit_ground", position, 1f, 1f);
        }

        public void TryPickup(Player p)
        {
            if (removed || (!inGround && !returning) || stack == null) return;
            if (owner != null && owner != p && !(returning)) return;
            if (p.IsCreative && owner == p) { Remove(); return; }
            if (p.inventory.Add(stack.Copy())) { Sounds.Play("entity.item.pickup", position, 0.2f, 2f); Remove(); }
        }

        public override void CreateVisual() { go = ItemRender.CreateHeldModelVisual(stack ?? new ItemStack("trident", 1), 1f); rends = go?.GetComponentsInChildren<Renderer>(); }
        public override void Render(float partial)
        {
            if (go == null) return;
            go.transform.position = InterpPos(partial);
            go.transform.rotation = Quaternion.Euler(Mathf.LerpAngle(prevPitch, pitch, partial) + 90, Mathf.LerpAngle(prevYaw, yaw, partial), 0);
            if (rends != null) EntityLight.ApplyRenderers(rends, EntityLight.Sample(world, InterpPos(partial)), 0, Vector4.zero);
        }
        public override void Save(Dictionary<string, string> d) { base.Save(d); if (stack != null) d["stack"] = stack.Serialize(); d["g"] = inGround ? "1" : "0"; }
        public override void Load(Dictionary<string, string> d) { base.Load(d); if (d.TryGetValue("stack", out var s)) stack = ItemStack.Deserialize(s); inGround = d.TryGetValue("g", out var g) && g == "1"; }
    }

    // =====================================================================================================
    /// <summary>Snowball, egg, ender pearl, experience bottle.</summary>
    public sealed class ThrownItemProjectile : Projectile
    {
        public readonly string kind;
        public ThrownItemProjectile(string kind) { this.kind = kind; gravity = kind == "experience_bottle" ? 0.07f : 0.03f; }
        public override string TypeId => kind;

        protected override void OnHitEntity(Entity e)
        {
            if (kind == "snowball") e.Hurt(DamageSource.ProjectileHit(this, owner, "thrown"), (e is Mob m && (m.def.id == "blaze")) ? 3 : 0);
            else if (kind == "egg") e.Hurt(DamageSource.ProjectileHit(this, owner, "thrown"), 0);
            Impact(e.position);
        }
        protected override void OnHitBlock(BlockHit hit) { var b = Blocks.ByState[hit.state]; b.OnProjectileHit(world, hit.pos, hit.state - b.baseState, this); Impact(hit.point + DirUtil.Normal[(int)hit.face] * 0.1f); }

        void Impact(Vector3 at)
        {
            switch (kind)
            {
                case "snowball": for (int i = 0; i < 8; i++) Particles.BlockDust(world, at, Blocks.StateOf("snow_block"), 1); break;
                case "egg":
                    for (int i = 0; i < 6; i++) Particles.ItemBreak(world, at, new ItemStack("egg", 1));
                    if (Random.value < 0.125f)
                    {
                        int n = Random.value < 1f / 32f ? 4 : 1;
                        for (int i = 0; i < n; i++) { var c = MobRegistry.Spawn(world, "chicken", at, SpawnReason.Breeding); if (c is Mob cm) cm.SetBaby(true); }
                    }
                    break;
                case "ender_pearl":
                    for (int i = 0; i < 32; i++) Particles.Portal(world, at + Random.insideUnitSphere);
                    if (owner is Player p && !p.dead && p.world == world)
                    {
                        if (p.vehicle != null) p.StopRiding();
                        p.Teleport(at);
                        if (!p.IsCreative) p.Hurt(DamageSource.Fall, 5f);
                        if (Random.value < 0.05f) MobRegistry.Spawn(world, "endermite", at, SpawnReason.Natural);
                        Sounds.Play("entity.player.teleport", at, 1f, 1f);
                    }
                    break;
                case "experience_bottle":
                    Particles.SplashPotion(world, at, new Color32(120, 200, 255, 255));
                    Sounds.Play("entity.splash_potion.break", at, 1f, 1f);
                    XpOrb.Spawn(world, at, 3 + Random.Range(0, 5) + Random.Range(0, 5));
                    break;
            }
            Remove();
        }

        public override void CreateVisual() => MakeItemVisual(kind, 0.5f);
        public override void Render(float partial) => RenderBillboard(partial);
    }

    // =====================================================================================================
    public sealed class ThrownPotion : Projectile
    {
        public ItemStack potionStack; public bool lingering;
        public ThrownPotion() { gravity = 0.05f; }
        public static ThrownPotion Throw(World w, Player p, ItemStack s, bool lingering)
        {
            var t = new ThrownPotion { world = w, potionStack = s, lingering = lingering };
            t.ShootFrom(p, 0.5f, 1f);
            t.velocity.y += 0.2f;
            w.AddEntity(t);
            return t;
        }
        protected override void OnHitEntity(Entity e) => Shatter(position);
        protected override void OnHitBlock(BlockHit hit) => Shatter(hit.point + DirUtil.Normal[(int)hit.face] * 0.1f);
        void Shatter(Vector3 at)
        {
            var pot = Potions.Get(potionStack?.Get("potion") ?? "water");
            var col = pot != null ? pot.color : new Color32(56, 93, 198, 255);
            Particles.SplashPotion(world, at, col);
            Sounds.Play("entity.splash_potion.break", at, 1f, 1f);
            if (lingering)
            {
                var c = new AreaEffectCloud { world = world, potion = pot, radius = 3f, duration = 600, color = col, owner = owner };
                c.SetPosition(at);
                world.AddEntity(c);
            }
            else if (pot != null)
            {
                if (pot.id == "water")
                    foreach (var e in world.GetEntities(new AABB(at - new Vector3(4, 2, 4), at + new Vector3(4, 2, 4))))
                    { if (e is LivingEntity le) { le.Extinguish(); if (le is Mob m && (m.def.id == "enderman" || m.def.id == "blaze" || m.def.id == "strider")) le.Hurt(DamageSource.Magic, 1); } }
                foreach (var e in world.GetEntities(new AABB(at - new Vector3(4, 2, 4), at + new Vector3(4, 2, 4))))
                {
                    if (!(e is LivingEntity le) || le.dead) continue;
                    float d = Vector3.Distance(e.position, at);
                    if (d > 4) continue;
                    float f = 1f - d / 4f;
                    foreach (var fx in pot.effects)
                    {
                        var c = fx.Copy();
                        if (fx.effect == Effect.InstantHealth || fx.effect == Effect.InstantDamage) { le.AddEffect(c); continue; }
                        c.duration = Mathf.Max(1, (int)(c.duration * f));
                        if (c.duration > 20) le.AddEffect(c);
                    }
                }
            }
            Remove();
        }
        public override void CreateVisual() => MakeItemVisual(lingering ? "lingering_potion" : "splash_potion", 0.5f);
        public override void Render(float partial) => RenderBillboard(partial);
    }

    // =====================================================================================================
    public sealed class AreaEffectCloud : Entity
    {
        public PotionType potion; public float radius = 3f; public int duration = 600; public Color32 color; public Entity owner;
        public bool dragonBreath;
        readonly Dictionary<int, int> reapply = new Dictionary<int, int>();
        public override string TypeId => "area_effect_cloud";
        public override bool ShouldSave => false;
        public AreaEffectCloud() { width = 6; height = 0.5f; noPhysics = true; blocksBuilding = false; }
        public override void Tick()
        {
            age++;
            prevPosition = position;
            if (age >= duration) { Remove(); return; }
            float r = radius * (1f - age / (float)duration * 0.5f);
            int n = Mathf.CeilToInt(r * r * 0.6f);
            for (int i = 0; i < n; i++)
            {
                float a = Random.value * Mathf.PI * 2, d = Mathf.Sqrt(Random.value) * r;
                var p = position + new Vector3(Mathf.Cos(a) * d, 0.1f, Mathf.Sin(a) * d);
                if (dragonBreath) Particles.DragonBreath(world, p); else Particles.Effect(world, p, color, false);
            }
            if (age % 5 != 0) return;
            foreach (var e in world.GetEntities(new AABB(position - new Vector3(r, 0.5f, r), position + new Vector3(r, 2f, r))))
            {
                if (!(e is LivingEntity le) || le.dead) continue;
                if (reapply.TryGetValue(e.id, out int t) && t > age) continue;
                if ((new Vector2(e.position.x - position.x, e.position.z - position.z)).magnitude > r) continue;
                reapply[e.id] = age + 20;
                if (dragonBreath) { if (!(e is Mob m && m.def.id == "ender_dragon")) le.Hurt(DamageSource.DragonBreath, 3f); continue; }
                if (potion != null) foreach (var fx in potion.effects) { var c = fx.Copy(); c.duration = Mathf.Max(1, c.duration / 4); le.AddEffect(c); }
            }
        }
        public override void Render(float partial) { }
    }

    // =====================================================================================================
    public sealed class Firework : Projectile
    {
        public int lifetime; public bool attachedGlide; public Player booster; public bool shotAtAngle;
        public Color32[] colors;
        public override string TypeId => "firework_rocket";
        public Firework() { gravity = 0; drag = 1f; }

        public static Firework Launch(World w, LivingEntity shooter, Vector3 pos, Vector3 vel, bool atAngle, int flight = 1)
        {
            var f = new Firework { world = w, owner = shooter, shotAtAngle = atAngle };
            f.SetPosition(pos);
            f.velocity = atAngle ? vel : new Vector3(Random.Range(-0.02f, 0.02f), 0.05f, Random.Range(-0.02f, 0.02f)) + vel * 0.1f;
            f.lifetime = 10 * (flight + 1) + Random.Range(0, 6) + Random.Range(0, 7);
            f.colors = new[] { (Color32)Color.HSVToRGB(Random.value, 0.8f, 1f), (Color32)Color.HSVToRGB(Random.value, 0.8f, 1f) };
            w.AddEntity(f);
            Sounds.Play("entity.firework_rocket.launch", pos, 3f, 1f);
            return f;
        }

        public static void Boost(World w, Player p, int flight)
        {
            var f = new Firework { world = w, owner = p, booster = p, attachedGlide = true };
            f.SetPosition(p.position);
            f.lifetime = 10 * (flight + 1) + Random.Range(0, 6) + Random.Range(0, 7);
            f.colors = new[] { (Color32)Color.HSVToRGB(Random.value, 0.8f, 1f) };
            w.AddEntity(f);
            Sounds.Play("entity.firework_rocket.launch", p.position, 3f, 1f);
        }

        protected override void TickFlight()
        {
            if (attachedGlide && booster != null)
            {
                if (!booster.IsGliding || booster.removed) { Remove(); return; }
                Vector3 look = booster.LookDir;
                booster.velocity += look * 0.1f + (look * 1.5f - booster.velocity) * 0.5f;
                position = booster.position;
            }
            else
            {
                if (!shotAtAngle) { velocity.x *= 1.15f; velocity.z *= 1.15f; velocity.y += 0.04f; }
                Vector3 to = position + velocity;
                if (world.ClipCollision(position, to, out var hit)) { position = hit.point; Explode(); return; }
                var e = FindEntityHit(position, to);
                if (e != null && shotAtAngle) { position = e.position + Vector3.up * e.height * 0.5f; Explode(); return; }
                position = to;
            }
            if (age % 2 == 0) Particles.Firework(world, position, -velocity * 0.1f + Random.insideUnitSphere * 0.05f, new Color32(255, 240, 200, 255));
            if (age >= lifetime) Explode();
        }

        void Explode()
        {
            Sounds.Play("entity.firework_rocket.blast", position, 4f, 1f);
            if (colors != null)
                for (int i = 0; i < 80; i++)
                {
                    var c = colors[i % colors.Length];
                    Particles.Firework(world, position, Random.onUnitSphere * Random.Range(0.25f, 0.45f), c);
                }
            if (!attachedGlide && shotAtAngle)
                foreach (var e in world.GetEntities(Bounds.Grow(5f)))
                    if (e is LivingEntity le && (e.position - position).sqrMagnitude < 25) le.Hurt(DamageSource.Explosion(owner, position), 5f * (1f - (e.position - position).magnitude / 5f) + 1);
            Remove();
        }

        public override void CreateVisual() => MakeItemVisual("firework_rocket", 0.5f);
        public override void Render(float partial) { if (attachedGlide) { if (go != null) go.SetActive(false); return; } RenderBillboard(partial); }
    }

    // =====================================================================================================
    public sealed class FishingHook : Projectile
    {
        Player angler; int timeUntilLured, timeUntilHooked, nibble; bool inWaterState; public Entity hooked;
        LineRenderer line;
        public override string TypeId => "fishing_bobber";
        public FishingHook() { gravity = 0.03f; drag = 0.92f; }

        public static FishingHook Cast(World w, Player p, ItemStack rod)
        {
            var h = new FishingHook { world = w, angler = p };
            h.ShootFrom(p, 1f, 2f);
            h.velocity *= 0.6f; h.velocity.y += 0.1f;
            int lure = rod.GetEnchant(Enchant.Lure);
            h.timeUntilLured = Random.Range(100, 600) - lure * 100;
            w.AddEntity(h);
            return h;
        }

        protected override void TickFlight()
        {
            if (angler == null || angler.removed || angler.dead || (angler.position - position).sqrMagnitude > 1024 || !(angler.inventory.Selected?.item is FishingRodItem || angler.inventory.offhand?.item is FishingRodItem))
            { Remove(); if (angler != null) angler.fishingHook = null; return; }
            if (hooked != null) { if (hooked.removed) hooked = null; else { position = hooked.position + Vector3.up * hooked.height * 0.8f; return; } }
            bool water = world.IsWater(Int3.Floor(position));
            if (water)
            {
                inWaterState = true;
                velocity.y += 0.04f; velocity *= 0.9f;
                float surf = Mathf.Floor(position.y) + 0.85f;
                if (position.y > surf) velocity.y -= 0.03f;
                TickFishing();
                position += velocity;
                return;
            }
            base.TickFlight();
        }

        void TickFishing()
        {
            if (nibble > 0) { nibble--; if (nibble == 0) { timeUntilLured = 0; timeUntilHooked = 0; } return; }
            if (timeUntilHooked > 0)
            {
                timeUntilHooked--;
                if (timeUntilHooked % 3 == 0) Particles.Bubble(world, position + new Vector3(Random.Range(-0.5f, 0.5f), 0, Random.Range(-0.5f, 0.5f)), false);
                if (timeUntilHooked <= 0)
                {
                    velocity.y -= 0.2f;
                    Sounds.Play("entity.fishing_bobber.splash", position, 0.25f, 1f + (Random.value - Random.value) * 0.4f);
                    Particles.Splash(world, position, 12);
                    nibble = Random.Range(20, 40);
                }
                return;
            }
            if (timeUntilLured > 0) { timeUntilLured -= world.CanSeeSky(Int3.Floor(position)) ? 1 : 0; if (world.session != null && world.session.IsRaining && Random.value < 0.25f) timeUntilLured--; if (timeUntilLured <= 0) timeUntilHooked = Random.Range(20, 80); }
        }

        protected override void OnHitEntity(Entity e) { hooked = e; }
        protected override void OnHitBlock(BlockHit hit) { velocity = Vector3.zero; position = hit.point + DirUtil.Normal[(int)hit.face] * 0.05f; }

        public int Retrieve(ItemStack rod)
        {
            int dmg = 0;
            if (hooked != null)
            {
                var d = angler.position - hooked.position;
                hooked.velocity += d * 0.1f;
                dmg = hooked is ItemEntity ? 3 : 5;
            }
            else if (nibble > 0)
            {
                var rng = new RNG(Random.Range(0, int.MaxValue));
                int luck = rod.GetEnchant(Enchant.LuckOfTheSea);
                var loot = FishingLoot(ref rng, luck, world.GetBiome(Int3.Floor(position)).key);
                var ie = world.SpawnItem(position, loot);
                if (ie != null)
                {
                    Vector3 d = angler.position - position;
                    ie.velocity = new Vector3(d.x * 0.1f, d.y * 0.1f + Mathf.Sqrt(d.magnitude) * 0.08f, d.z * 0.1f);
                    ie.pickupDelay = 0;
                }
                XpOrb.Spawn(world, angler.position + Vector3.up * 0.5f, Random.Range(1, 7));
                Achievements.Grant(angler, "fishy_business");
                dmg = 1;
            }
            else if (onGround || horizontalCollision) dmg = 2;
            Remove();
            angler.fishingHook = null;
            return dmg;
        }

        static ItemStack FishingLoot(ref RNG rng, int luck, string biome)
        {
            float r = rng.NextFloat();
            float treasure = 0.05f + luck * 0.021f, junk = 0.1f - luck * 0.025f;
            if (r < treasure)
            {
                string[] t = { "bow", "enchanted_book", "fishing_rod", "name_tag", "nautilus_shell", "saddle" };
                var s = new ItemStack(t[rng.Next(t.Length)], 1);
                if (s.item.id == "enchanted_book") { var e = Enchant.All[rng.Next(Enchant.All.Count)]; s = EnchantedBookItem.Make(e, rng.Range(1, e.maxLevel)); }
                else if (s.item.IsDamageable) Loot.EnchantRandomly(s, ref rng, 30, true);
                return s;
            }
            if (r < treasure + junk)
            {
                string[] j = { "lily_pad", "leather_boots", "leather", "bone", "potion", "string", "bowl", "stick", "ink_sac", "tripwire_hook", "rotten_flesh", "bamboo" };
                return new ItemStack(j[rng.Next(j.Length)], 1);
            }
            float f = rng.NextFloat();
            return new ItemStack(f < 0.6f ? "cod" : f < 0.85f ? "salmon" : f < 0.87f ? "tropical_fish" : "pufferfish", 1);
        }

        public override void CreateVisual()
        {
            go = new GameObject("FishingBobber");
            var ball = ItemRender.CreateBlockVisual(Blocks.StateOf("red_wool"), 0.18f);
            if (ball != null) { ball.transform.SetParent(go.transform, false); ball.transform.localPosition = new Vector3(0, -0.09f, 0); }
            line = go.AddComponent<LineRenderer>();
            line.material = Res.UnlitMaterial(Texture2D.whiteTexture);
            line.widthMultiplier = 0.02f; line.startColor = line.endColor = new Color(0.1f, 0.1f, 0.1f, 1);
            line.positionCount = 2;
            rends = go.GetComponentsInChildren<MeshRenderer>();
        }
        public override void Render(float partial)
        {
            if (go == null) return;
            var p = InterpPos(partial);
            go.transform.position = p;
            if (line != null && angler != null)
            {
                Vector3 hand = GameManager.Instance != null ? GameManager.Instance.RodTipPosition(angler, partial) : angler.InterpPos(partial) + Vector3.up * 1.4f;
                line.SetPosition(0, hand); line.SetPosition(1, p);
            }
            if (rends != null) EntityLight.ApplyRenderers(rends, EntityLight.Sample(world, p), 0, Vector4.zero);
        }
    }

    // =====================================================================================================
    public sealed class EyeOfEnder : Entity
    {
        Vector3 target; int life; bool survive;
        Renderer[] rends;
        public override string TypeId => "eye_of_ender";
        public override bool ShouldSave => false;
        public EyeOfEnder() { width = 0.25f; height = 0.25f; noPhysics = true; blocksBuilding = false; }

        public static EyeOfEnder Throw(World w, Player p, Vector3 strongholdPos)
        {
            var e = new EyeOfEnder { world = w };
            e.SetPosition(p.EyePosition);
            Vector3 d = strongholdPos - p.position; d.y = 0;
            float dist = d.magnitude;
            e.target = dist > 12 ? p.position + d.normalized * 12f + Vector3.up * 8f : strongholdPos;
            e.survive = Random.value < 0.8f;
            w.AddEntity(e);
            return e;
        }

        public override void Tick()
        {
            age++; prevPosition = position;
            Vector3 d = target - position; d.y = 0;
            float h = d.magnitude;
            float speed = Mathf.Lerp(new Vector2(velocity.x, velocity.z).magnitude, 1f, 0.0025f);
            if (h < 1f) { speed *= 0.8f; velocity.y *= 0.8f; }
            Vector3 hv = h > 1e-3f ? d / h * speed : Vector3.zero;
            int dir = position.y < target.y ? 1 : -1;
            velocity = new Vector3(hv.x, velocity.y + (dir / 15f - velocity.y) * 0.015f, hv.z);
            position += velocity * 0.5f;
            Particles.Portal(world, position + Random.insideUnitSphere * 0.3f);
            if (++life > 80)
            {
                Sounds.Play("entity.ender_eye.death", position, 1f, 1f);
                if (survive) world.SpawnItem(position, new ItemStack("ender_eye", 1));
                else for (int i = 0; i < 8; i++) Particles.ItemBreak(world, position, new ItemStack("ender_eye", 1));
                Remove();
            }
        }
        public override void CreateVisual() { go = ItemRender.CreateSpriteVisual(new ItemStack("ender_eye", 1), 0.5f); rends = go?.GetComponentsInChildren<Renderer>(); }
        public override void Render(float partial)
        {
            if (go == null) return;
            go.transform.position = InterpPos(partial);
            var cam = GameManager.MainCamera; if (cam != null) go.transform.rotation = Quaternion.LookRotation(go.transform.position - cam.transform.position);
            if (rends != null) EntityLight.ApplyRenderers(rends, new Vector2(1, 1), 0, Vector4.zero);
        }
    }

    // =====================================================================================================
    public abstract class Fireball : Projectile
    {
        public float accel = 0.1f; public Vector3 power;
        protected Fireball() { gravity = 0; drag = 0.95f; width = 1f; height = 1f; fireImmune = true; }
        public void Aim(Vector3 dir) { power = dir.normalized * accel; velocity = dir.normalized * accel * 2; }
        protected override void TickFlight()
        {
            velocity += power;
            base.TickFlight();
            if (!removed && age % 2 == 0) Particles.Smoke(world, position, 1, 0.2f);
        }
        public override bool Attackable => true;
        public override bool Hurt(DamageSource src, float amount)
        {
            if (src.attacker is LivingEntity le)
            {
                var d = le.LookDir;
                velocity = d * 1.0f; power = d * 0.1f; owner = le; leftOwner = true;
                return true;
            }
            return false;
        }
    }

    public sealed class SmallFireball : Fireball
    {
        public override string TypeId => "small_fireball";
        public SmallFireball() { width = 0.3125f; height = 0.3125f; accel = 0.1f; }
        protected override void OnHitEntity(Entity e)
        {
            if (!(e is Mob m && m.def.fireImmune)) { e.SetOnFire(5); e.Hurt(DamageSource.FireballHit(this, owner), 5f); }
            Remove();
        }
        protected override void OnHitBlock(BlockHit hit)
        {
            var p = hit.pos.Offset(hit.face);
            if ((world.session == null || world.session.mobGriefing) && world.IsAir(p)) world.SetState(p, Blocks.Fire.DefaultState);
            Remove();
        }
        public override void CreateVisual() => MakeItemVisual("fire_charge", 0.35f);
        public override void Render(float partial) => RenderBillboard(partial);
    }

    public sealed class LargeFireball : Fireball
    {
        public int explosionPower = 1;
        public override string TypeId => "fireball";
        public LargeFireball() { accel = 0.1f; }
        protected override void OnHitEntity(Entity e) { e.Hurt(DamageSource.FireballHit(this, owner), 6f); Boom(); }
        protected override void OnHitBlock(BlockHit hit) => Boom();
        void Boom()
        {
            Explosion.Explode(world, owner ?? this, position, explosionPower, true, world.session == null || world.session.mobGriefing);
            if (owner is Mob gm && gm.def.id == "ghast" && owner != null) { }
            Remove();
        }
        protected override bool CanHit(Entity e) => base.CanHit(e) && !(e is Mob m && m.def.id == "ghast" && e == owner);
        public override void CreateVisual() => MakeItemVisual("fire_charge", 1f);
        public override void Render(float partial) => RenderBillboard(partial);
    }

    public sealed class DragonFireball : Fireball
    {
        public override string TypeId => "dragon_fireball";
        public DragonFireball() { accel = 0.1f; }
        protected override bool CanHit(Entity e) => base.CanHit(e) && !(e is Mob m && m.def.id == "ender_dragon");
        protected override void OnHitEntity(Entity e) => Burst();
        protected override void OnHitBlock(BlockHit hit) => Burst();
        void Burst()
        {
            var c = new AreaEffectCloud { world = world, radius = 3f, duration = 600, dragonBreath = true, owner = owner, color = new Color32(200, 60, 230, 255) };
            c.SetPosition(position);
            world.AddEntity(c);
            Sounds.Play("entity.dragon_fireball.explode", position, 1f, 1f);
            Remove();
        }
        public override void CreateVisual() { go = ItemRender.CreateSpriteVisual(new ItemStack("dragon_breath", 1), 1f); rends = go?.GetComponentsInChildren<Renderer>(); }
        public override void Render(float partial) => RenderBillboard(partial);
    }

    public sealed class WitherSkull : Fireball
    {
        public bool dangerous;
        public override string TypeId => "wither_skull";
        public WitherSkull() { width = 0.3125f; height = 0.3125f; accel = 0.1f; drag = 0.73f; }
        protected override bool CanHit(Entity e) => base.CanHit(e) && !(e is WitherBoss);
        protected override void OnHitEntity(Entity e)
        {
            if (e is LivingEntity le)
            {
                if (le.Hurt(DamageSource.FireballHit(this, owner), 8f))
                {
                    if (!le.IsAlive && owner is WitherBoss wb) wb.Heal(5f);
                    var diff = world.session?.difficulty ?? Difficulty.Normal;
                    int sec = diff == Difficulty.Normal ? 10 : diff == Difficulty.Hard ? 40 : 0;
                    if (sec > 0) le.AddEffect(new EffectInstance(Effect.Wither, sec * 20, 1));
                }
            }
            Boom();
        }
        protected override void OnHitBlock(BlockHit hit) => Boom();
        void Boom() { Explosion.Explode(world, owner ?? this, position, 1f, false, world.session == null || world.session.mobGriefing); Remove(); }
        public override void CreateVisual() { go = ItemRender.CreateBlockVisual(Blocks.StateOf(dangerous ? "wither_skeleton_skull" : "wither_skeleton_skull"), 0.4f); rends = go?.GetComponentsInChildren<Renderer>(); }
    }

    public sealed class ShulkerBullet : Projectile
    {
        public Entity target;
        public override string TypeId => "shulker_bullet";
        public ShulkerBullet() { gravity = 0; drag = 1f; width = 0.3125f; height = 0.3125f; }
        protected override void TickFlight()
        {
            if (target != null && !target.removed)
            {
                Vector3 d = target.position + Vector3.up * target.height * 0.5f - position;
                velocity = Vector3.Lerp(velocity, d.normalized * 0.3f, 0.12f);
            }
            if (age % 2 == 0) Particles.EndRod(world, position);
            base.TickFlight();
            if (age > 300) Remove();
        }
        protected override void OnHitEntity(Entity e)
        {
            if (e is LivingEntity le && le.Hurt(DamageSource.ProjectileHit(this, owner, "mob"), 4f)) le.AddEffect(new EffectInstance(Effect.Levitation, 200));
            Sounds.Play("entity.shulker_bullet.hit", position, 1f, 1f);
            Remove();
        }
        public override bool Attackable => true;
        public override bool Hurt(DamageSource src, float amount) { Sounds.Play("entity.shulker_bullet.hurt", position, 1f, 1f); Remove(); return true; }
        public override void CreateVisual() { go = ItemRender.CreateBlockVisual(Blocks.StateOf("white_concrete"), 0.3f); rends = go?.GetComponentsInChildren<Renderer>(); }
    }

    public sealed class WindChargeProjectile : Projectile
    {
        public bool breeze;
        public override string TypeId => "wind_charge";
        public WindChargeProjectile() { gravity = 0; drag = 1f; width = 0.3125f; height = 0.3125f; }
        protected override void OnHitEntity(Entity e) { e.Hurt(DamageSource.ProjectileHit(this, owner, "wind_charge"), 1f); Burst(); }
        protected override void OnHitBlock(BlockHit hit)
        {
            var b = Blocks.ByState[hit.state];
            if (b is DoorBlock || b is TrapdoorBlock || b is FenceGateBlock || b.id.Contains("button") || b.id == "lever") b.OnUse(world, hit.pos, hit.state - b.baseState, null, hit.face, hit.point);
            Burst();
        }
        void Burst()
        {
            Sounds.Play("entity.wind_charge.wind_burst", position, 1f, 1f);
            for (int i = 0; i < 12; i++) Particles.Cloud(world, position + Random.insideUnitSphere * 0.5f);
            foreach (var e in world.GetEntities(Bounds.Grow(2.5f)))
            {
                if (e.removed || e is Projectile) continue;
                Vector3 d = e.position + Vector3.up * e.height * 0.5f - position;
                float f = 1f - d.magnitude / 2.5f;
                if (f <= 0) continue;
                e.velocity += d.normalized * f * 1.2f + Vector3.up * f * 0.4f;
                e.fallDistance = 0;
            }
            Remove();
        }
        public override void CreateVisual() => MakeItemVisual("wind_charge", 0.4f);
        public override void Render(float partial) => RenderBillboard(partial);
    }

    public sealed class LlamaSpit : Projectile
    {
        public override string TypeId => "llama_spit";
        public LlamaSpit() { gravity = 0.06f; }
        protected override void OnHitEntity(Entity e) { e.Hurt(DamageSource.ProjectileHit(this, owner, "mob"), 1f); Remove(); }
        public override void CreateVisual() => MakeItemVisual("snowball", 0.25f);
        public override void Render(float partial) => RenderBillboard(partial);
    }
}
