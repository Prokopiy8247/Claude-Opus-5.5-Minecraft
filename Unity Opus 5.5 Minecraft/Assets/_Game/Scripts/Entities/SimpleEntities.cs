using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace MCR
{
    /// <summary>Common per-frame lighting for entity renderers.</summary>
    public static class EntityLight
    {
        static MaterialPropertyBlock mpb;
        public static readonly int EntityLightId = Shader.PropertyToID("_EntityLight");
        public static readonly int OverlayId = Shader.PropertyToID("_Overlay");
        public static readonly int ColorId = Shader.PropertyToID("_Color");

        public static Vector2 Sample(World w, Vector3 pos)
        {
            if (w == null) return new Vector2(1, 0);
            byte l = w.GetLightRaw(Mathf.FloorToInt(pos.x), Mathf.FloorToInt(pos.y), Mathf.FloorToInt(pos.z));
            return new Vector2((l >> 4) / 15f, (l & 15) / 15f);
        }

        public static void Apply(GameObject go, World w, Vector3 pos, float flash = 0, Color? overlay = null, float alpha = 1f)
        {
            if (go == null) return;
            if (mpb == null) mpb = new MaterialPropertyBlock();
            var l = Sample(w, pos);
            var rs = go.GetComponentsInChildren<Renderer>();
            foreach (var r in rs)
            {
                r.GetPropertyBlock(mpb);
                mpb.SetVector(EntityLightId, new Vector4(l.x, l.y, 1, flash));
                if (overlay.HasValue) { var o = overlay.Value; mpb.SetVector(OverlayId, new Vector4(o.r, o.g, o.b, o.a)); }
                else mpb.SetVector(OverlayId, Vector4.zero);
                if (alpha < 1f) mpb.SetColor(ColorId, new Color(1, 1, 1, alpha));
                r.SetPropertyBlock(mpb);
            }
        }

        public static void ApplyRenderers(Renderer[] rs, Vector2 light, float flash, Vector4 overlay)
        {
            if (mpb == null) mpb = new MaterialPropertyBlock();
            foreach (var r in rs)
            {
                if (r == null) continue;
                r.GetPropertyBlock(mpb);
                mpb.SetVector(EntityLightId, new Vector4(light.x, light.y, 1, flash));
                mpb.SetVector(OverlayId, overlay);
                r.SetPropertyBlock(mpb);
            }
        }
    }

    // =====================================================================================================
    public sealed class ItemEntity : Entity
    {
        public ItemStack stack;
        public int pickupDelay = 10;
        public int life = 6000;
        public Entity thrower;
        public float bobOffset;
        int mergeTimer;
        Renderer[] rends;

        public ItemEntity() { width = 0.25f; height = 0.25f; blocksBuilding = false; }
        public override string TypeId => "item";
        public override string DisplayName => stack?.DisplayName ?? "Item";
        public override bool ShouldSave => !removed && stack != null && !stack.IsEmpty;

        public static ItemEntity Create(World w, Vector3 pos, ItemStack s)
        {
            var e = new ItemEntity { stack = s };
            e.world = w;
            e.SetPosition(pos - new Vector3(0, 0.125f, 0));
            e.bobOffset = Random.value * Mathf.PI * 2;
            e.yaw = Random.value * 360;
            e.fireImmune = s.item.fireResistant;
            w.AddEntity(e);
            return e;
        }

        public override void Tick()
        {
            base.Tick();
            if (removed) return;
            if (stack == null || stack.IsEmpty) { Remove(); return; }
            if (pickupDelay > 0 && pickupDelay != short.MaxValue) pickupDelay--;
            if (inWater) { velocity.y += 0.005f; if (velocity.y > 0.06f) velocity.y = 0.06f; velocity.x *= 0.99f; velocity.z *= 0.99f; }
            else if (inLava && fireImmune) velocity.y += 0.01f;
            else velocity.y -= 0.04f;
            Move(velocity);
            float slip = onGround ? world.GetBlock(Int3.Floor(position - Vector3.up * 0.1f)).slipperiness * 0.98f : 0.98f;
            velocity.x *= slip; velocity.z *= slip; velocity.y *= 0.98f;
            if (onGround && velocity.y < 0) velocity.y *= -0.5f;
            if (inLava && !fireImmune) { Sounds.Play("entity.generic.burn", position, 0.4f, 2f); Remove(); return; }
            if (onFire && !fireImmune && fireTicks > 0 && age % 10 == 0) { Remove(); return; }
            if (++mergeTimer >= 20) { mergeTimer = 0; TryMerge(); }
            if (life != short.MaxValue && age >= life) Remove();
        }

        void TryMerge()
        {
            if (stack.count >= stack.MaxStack) return;
            foreach (var e in world.GetEntities(Bounds.Grow(0.5f, 0, 0.5f), this))
            {
                if (!(e is ItemEntity o) || o.removed || !o.stack.Stackable(stack)) continue;
                if (o.stack.count + stack.count > stack.MaxStack) continue;
                stack.count += o.stack.count; o.Remove();
                pickupDelay = Mathf.Max(pickupDelay, o.pickupDelay); age = Mathf.Min(age, o.age);
                if (go != null) { Object.Destroy(go); go = null; CreateVisual(); }
            }
        }

        public override bool Hurt(DamageSource src, float amount)
        {
            if (src.isExplosion || (src.isFire && !fireImmune)) { if (stack.item.id != "nether_star") Remove(); return true; }
            return false;
        }

        public void TryPickup(Player p)
        {
            if (removed || pickupDelay > 0 || p.dead) return;
            if (thrower == p && pickupDelay > 0) return;
            int before = stack.count;
            var item = stack.item;
            if (p.inventory.Add(stack))
            {
                Sounds.Play("entity.item.pickup", position, 0.2f, (Random.value - Random.value) * 1.4f + 2f);
                Achievements.OnPickup(p, item.id);
                GameManager.Instance?.OnItemPickedUp(this, p);
                Remove();
            }
            else if (stack.count < before) Sounds.Play("entity.item.pickup", position, 0.2f, (Random.value - Random.value) * 1.4f + 2f);
        }

        public override void CreateVisual()
        {
            go = ItemRender.CreateDroppedVisual(stack);
            if (go != null) rends = go.GetComponentsInChildren<Renderer>();
        }

        public override void Render(float partial)
        {
            if (go == null) return;
            Vector3 p = InterpPos(partial);
            float t = (age + partial) / 20f;
            float bob = Mathf.Sin(t * 2f + bobOffset) * 0.1f + 0.1f;
            go.transform.position = p + Vector3.up * (bob + 0.125f);
            go.transform.rotation = Quaternion.Euler(0, (t * 57.3f * 1f + yaw) % 360f, 0);
            if (rends != null) EntityLight.ApplyRenderers(rends, EntityLight.Sample(world, p + Vector3.up * 0.2f), 0, Vector4.zero);
        }

        public override void Save(Dictionary<string, string> d) { base.Save(d); d["stack"] = stack.Serialize(); d["age"] = age.ToString(); d["delay"] = pickupDelay.ToString(); }
        public override void Load(Dictionary<string, string> d)
        {
            base.Load(d);
            if (d.TryGetValue("stack", out var s)) stack = ItemStack.Deserialize(s);
            if (d.TryGetValue("age", out var a)) int.TryParse(a, out age);
            if (d.TryGetValue("delay", out var dl)) int.TryParse(dl, out pickupDelay);
            if (stack != null) fireImmune = stack.item.fireResistant;
        }
    }

    // =====================================================================================================
    public sealed class XpOrb : Entity
    {
        public int value;
        int followTimer;
        Player follow;
        Renderer[] rends;
        public XpOrb() { width = 0.5f; height = 0.5f; blocksBuilding = false; }
        public override string TypeId => "experience_orb";
        public override bool ShouldSave => !removed;

        public static void Spawn(World w, Vector3 pos, int total)
        {
            while (total > 0)
            {
                int v = total >= 2477 ? 2477 : total >= 1237 ? 1237 : total >= 617 ? 617 : total >= 307 ? 307 : total >= 149 ? 149 : total >= 73 ? 73 : total >= 37 ? 37 : total >= 17 ? 17 : total >= 7 ? 7 : total >= 3 ? 3 : 1;
                total -= v;
                var o = new XpOrb { value = v, world = w };
                o.SetPosition(pos + new Vector3(Random.Range(-0.3f, 0.3f), 0.1f, Random.Range(-0.3f, 0.3f)));
                o.velocity = new Vector3(Random.Range(-0.1f, 0.1f), Random.Range(0.1f, 0.25f), Random.Range(-0.1f, 0.1f));
                o.yaw = Random.value * 360;
                w.AddEntity(o);
            }
        }

        public override void Tick()
        {
            base.Tick();
            if (removed) return;
            velocity.y -= 0.03f;
            if (inLava) { velocity.y = 0.2f; velocity.x = Random.Range(-0.1f, 0.1f); velocity.z = Random.Range(-0.1f, 0.1f); }
            if (--followTimer <= 0) { follow = world.NearestPlayer(position, 8f); followTimer = 20; if (follow != null && follow.IsSpectator) follow = null; }
            if (follow != null)
            {
                Vector3 d = follow.position + Vector3.up * (follow.EyeHeight / 2f) - position;
                float dist = d.magnitude / 8f;
                float f = 1f - dist;
                if (f > 0) { f *= f; velocity += d.normalized * f * 0.1f; }
            }
            Move(velocity);
            float slip = onGround ? world.GetBlock(Int3.Floor(position - Vector3.up * 0.1f)).slipperiness * 0.98f : 0.98f;
            velocity.x *= slip; velocity.z *= slip; velocity.y *= 0.98f;
            if (onGround) velocity.y *= -0.9f;
            if (age >= 6000) Remove();
        }

        public void TryPickup(Player p)
        {
            if (removed || p.dead || age < 2) return;
            if (p.xpPickupCooldown > 0) return;
            p.xpPickupCooldown = 2;
            int v = value;
            // mending: repair a random damaged mending item
            var cands = new List<ItemStack>();
            foreach (var s in new[] { p.inventory.Selected, p.inventory.offhand, p.inventory.armor[0], p.inventory.armor[1], p.inventory.armor[2], p.inventory.armor[3] })
                if (s != null && s.IsDamaged && s.GetEnchant(Enchant.Mending) > 0) cands.Add(s);
            if (cands.Count > 0)
            {
                var s = cands[Random.Range(0, cands.Count)];
                int repair = Mathf.Min(v * 2, s.damage);
                s.damage -= repair; v -= repair / 2;
            }
            if (v > 0) p.GiveXp(v);
            Sounds.Play("entity.experience_orb.pickup", position, 0.1f, 0.5f * ((Random.value - Random.value) * 0.7f + 1.8f));
            Remove();
        }

        public override void CreateVisual()
        {
            go = ItemRender.CreateOrbVisual(value);
            rends = go != null ? go.GetComponentsInChildren<Renderer>() : null;
        }
        public override void Render(float partial)
        {
            if (go == null) return;
            var cam = GameManager.MainCamera;
            go.transform.position = InterpPos(partial) + Vector3.up * 0.15f;
            if (cam != null) go.transform.rotation = cam.transform.rotation;
            float t = (age + partial) / 2f;
            float g = (Mathf.Sin(t) + 1f) * 0.5f;
            var c = new Color(g, 1f, 0.1f * (1 - g), 1f);
            foreach (var r in rends) { var mpb = new MaterialPropertyBlock(); r.GetPropertyBlock(mpb); mpb.SetColor(EntityLight.ColorId, c); mpb.SetVector(EntityLight.EntityLightId, new Vector4(1, 1, 1, 0)); r.SetPropertyBlock(mpb); }
        }
        public override void Save(Dictionary<string, string> d) { base.Save(d); d["v"] = value.ToString(); d["age"] = age.ToString(); }
        public override void Load(Dictionary<string, string> d) { base.Load(d); if (d.TryGetValue("v", out var v)) int.TryParse(v, out value); if (d.TryGetValue("age", out var a)) int.TryParse(a, out age); }
    }

    // =====================================================================================================
    public sealed class FallingBlockEntity : Entity
    {
        public ushort state;
        public int time;
        public bool dropItem = true;
        public bool hurtEntities;
        Renderer[] rends;
        public FallingBlockEntity() { width = 0.98f; height = 0.98f; blocksBuilding = true; }
        public override string TypeId => "falling_block";

        public static bool CanFallThrough(World w, Int3 p)
        {
            if (p.y < w.minY) return false;
            var b = w.GetBlock(p);
            return b.isAir || b.isLiquid || b.replaceable || b is FireBlock;
        }

        public static FallingBlockEntity Spawn(World w, Int3 pos, ushort state)
        {
            var b = Blocks.ByState[state];
            var e = new FallingBlockEntity { state = state, world = w };
            e.hurtEntities = b is AnvilBlock || b.id == "pointed_dripstone";
            e.SetPosition(new Vector3(pos.x + 0.5f, pos.y, pos.z + 0.5f));
            w.SetState(pos, b.IsWaterLike(state - b.baseState) ? Blocks.Water.DefaultState : (ushort)0);
            w.AddEntity(e);
            return e;
        }

        public override void Tick()
        {
            base.Tick();
            if (removed) return;
            time++;
            velocity.y -= 0.04f;
            Move(velocity);
            velocity *= 0.98f;
            var b = Blocks.ByState[state];
            if (b is ConcretePowderBlock cp && inWater) { PlaceAt(Int3.Floor(position + Vector3.up * 0.5f), Blocks.Get(b.id.Replace("_powder", "")).DefaultState); return; }
            if (onGround)
            {
                Int3 p = Int3.Floor(position + Vector3.up * 0.5f);
                var here = world.GetBlock(p);
                if (hurtEntities && fallDistance > 1f) DamageBelow();
                if ((here.isAir || here.replaceable || here.isLiquid) && !(here is FallingBlockPlaceBlocker))
                {
                    if (b is AnvilBlock ab && fallDistance > 1f && Random.value < 0.05f + fallDistance * 0.05f)
                    {
                        var dmg = ab.Damaged();
                        if (dmg == null) { Sounds.Play("block.anvil.destroy", position, 1f, 1f); Remove(); return; }
                        state = dmg.State(state - b.baseState);
                    }
                    PlaceAt(p, state);
                }
                else
                {
                    if (dropItem && b.item != null) world.SpawnItem(position + Vector3.up * 0.5f, new ItemStack(b.item, 1));
                    Remove();
                }
                return;
            }
            if (time > 600 || position.y < world.minY - 10) { if (dropItem && b.item != null && position.y >= world.minY) world.SpawnItem(position, new ItemStack(b.item, 1)); Remove(); }
        }

        void DamageBelow()
        {
            var b = Blocks.ByState[state];
            float dmg = Mathf.Min(40, Mathf.Ceil(fallDistance - 1) * (b is AnvilBlock ? 2f : 6f));
            foreach (var e in world.GetEntities(Bounds.Grow(0.1f), this))
                if (e is LivingEntity le) le.Hurt(new DamageSource(b is AnvilBlock ? "anvil" : "fallingStalactite"), dmg);
        }

        void PlaceAt(Int3 p, ushort s)
        {
            var b = Blocks.ByState[s];
            world.SetState(p, s);
            Sounds.PlayBlock(b.sound, SoundEvent.Place, p.Center);
            if (b is AnvilBlock) Sounds.Play("block.anvil.land", p.Center, 0.3f, 1f);
            Remove();
        }

        public override void CreateVisual() { go = ItemRender.CreateBlockVisual(state, 1f); rends = go?.GetComponentsInChildren<Renderer>(); }
        public override void Render(float partial)
        {
            if (go == null) return;
            go.transform.position = InterpPos(partial);
            go.transform.rotation = Quaternion.identity;
            if (rends != null) EntityLight.ApplyRenderers(rends, EntityLight.Sample(world, InterpPos(partial) + Vector3.up * 0.5f), 0, Vector4.zero);
        }
        public override void Save(Dictionary<string, string> d) { base.Save(d); var b = Blocks.ByState[state]; d["block"] = b.id; d["meta"] = (state - b.baseState).ToString(); d["time"] = time.ToString(); }
        public override void Load(Dictionary<string, string> d)
        {
            base.Load(d);
            var b = d.TryGetValue("block", out var id) ? Blocks.Get(id) : null;
            int m = d.TryGetValue("meta", out var ms) && int.TryParse(ms, out var mm) ? mm : 0;
            state = b != null ? b.State(Mathf.Min(m, b.stateCount - 1)) : Blocks.StateOf("sand");
            if (d.TryGetValue("time", out var t)) int.TryParse(t, out time);
        }
    }

    /// <summary>Marker type: blocks that a falling block should not replace.</summary>
    public abstract class FallingBlockPlaceBlocker : Block { }

    // =====================================================================================================
    public sealed class PrimedTnt : Entity
    {
        public int fuse = 80;
        public Entity igniter;
        public float power = 4f;
        Renderer[] rends;
        public PrimedTnt() { width = 0.98f; height = 0.98f; }
        public override string TypeId => "tnt";

        public static PrimedTnt Spawn(World w, Vector3 pos, int fuse, Entity igniter)
        {
            var e = new PrimedTnt { fuse = fuse, igniter = igniter, world = w };
            e.SetPosition(pos);
            float a = Random.value * Mathf.PI * 2;
            e.velocity = new Vector3(-Mathf.Sin(a) * 0.02f, 0.2f, -Mathf.Cos(a) * 0.02f);
            w.AddEntity(e);
            Sounds.Play("entity.tnt.primed", pos, 1f, 1f);
            return e;
        }

        public override void Tick()
        {
            base.Tick();
            if (removed) return;
            velocity.y -= 0.04f;
            Move(velocity);
            velocity *= 0.98f;
            if (onGround) { velocity.x *= 0.7f; velocity.z *= 0.7f; velocity.y *= -0.5f; }
            if (--fuse <= 0)
            {
                Remove();
                MCR.Explosion.Explode(world, igniter ?? this, position + Vector3.up * 0.49f, power, false, true);
            }
            else if (age % 2 == 0) Particles.Smoke(world, position + Vector3.up * 1.05f, 1, 0.1f);
        }

        public override void CreateVisual() { go = ItemRender.CreateBlockVisual(Blocks.StateOf("tnt"), 1f); rends = go?.GetComponentsInChildren<Renderer>(); }
        public override void Render(float partial)
        {
            if (go == null) return;
            float f = fuse - partial + 1;
            float s = 1f;
            if (f < 10) { float k = 1f - f / 10f; k = Mathf.Clamp01(k); k *= k; k *= k; s = 1f + k * 0.3f; }
            go.transform.position = InterpPos(partial) - new Vector3(0, (s - 1) * 0.5f, 0);
            go.transform.localScale = Vector3.one * s;
            float flash = ((int)f / 5) % 2 == 0 ? 0.6f : 0f;
            if (rends != null) EntityLight.ApplyRenderers(rends, EntityLight.Sample(world, InterpPos(partial) + Vector3.up * 0.5f), flash, Vector4.zero);
        }
        public override void Save(Dictionary<string, string> d) { base.Save(d); d["fuse"] = fuse.ToString(); }
        public override void Load(Dictionary<string, string> d) { base.Load(d); if (d.TryGetValue("fuse", out var f)) int.TryParse(f, out fuse); }
    }

    // =====================================================================================================
    public sealed class LightningBolt : Entity
    {
        public bool visualOnly;
        int life = 2, flashes;
        readonly List<Vector3> path = new List<Vector3>();
        LineRenderer lr;
        public override string TypeId => "lightning_bolt";
        public override bool ShouldSave => false;
        public LightningBolt() { width = 0.1f; height = 0.1f; noPhysics = true; blocksBuilding = false; }

        public static LightningBolt Strike(World w, Vector3 pos, bool visualOnly = false)
        {
            var e = new LightningBolt { world = w, visualOnly = visualOnly };
            e.SetPosition(pos);
            e.flashes = Random.Range(1, 3);
            w.AddEntity(e);
            return e;
        }

        public override void OnAddedToWorld()
        {
            base.OnAddedToWorld();
            Sounds.Play("entity.lightning_bolt.thunder", position, 10000f, 0.8f + Random.value * 0.2f);
            Sounds.Play("entity.lightning_bolt.impact", position, 2f, 0.5f + Random.value * 0.2f);
            GameManager.Instance?.OnLightningFlash();
            if (!visualOnly)
            {
                Int3 p = Int3.Floor(position);
                if (world.session == null || world.session.doFireTick)
                {
                    if (world.IsAir(p) && Blocks.Fire.CanSurvive(world, p, 0)) world.SetState(p, Blocks.Fire.DefaultState);
                }
                // lightning rod redirection handled by caller; copper deoxidizes
                var below = world.GetBlock(p.Offset(Dir.Down));
                if (below.id == "lightning_rod") { }
            }
        }

        public override void Tick()
        {
            age++;
            if (!visualOnly && age == 1)
                foreach (var e in world.GetEntities(new AABB(position - new Vector3(3, 3, 3), position + new Vector3(3, 6, 3)), this))
                    if (!e.removed) { e.OnStruckByLightning(this); if (e is LivingEntity le) le.Hurt(DamageSource.Lightning, 5f); }
            if (--life < 0)
            {
                if (flashes-- > 0) { life = 1; GameManager.Instance?.OnLightningFlash(); }
                else Remove();
            }
        }

        public override void CreateVisual()
        {
            go = new GameObject("Lightning");
            lr = go.AddComponent<LineRenderer>();
            lr.material = Res.UnlitMaterial(Texture2D.whiteTexture, true);
            lr.widthMultiplier = 0.35f;
            lr.startColor = lr.endColor = new Color(0.8f, 0.85f, 1f, 0.9f);
            var r = new RNG(id);
            path.Clear();
            Vector3 p = position;
            for (int i = 0; i < 24; i++) { path.Add(p); p += new Vector3(r.Range(-1.5f, 1.5f), 8f, r.Range(-1.5f, 1.5f)); }
            lr.positionCount = path.Count;
            lr.SetPositions(path.ToArray());
        }
        public override void Render(float partial) { }
    }

    /// <summary>Creates entities from saved type ids.</summary>
    public static class EntityFactory
    {
        public static Entity Create(string type, World w)
        {
            Entity e = null;
            switch (type)
            {
                case "item": e = new ItemEntity(); break;
                case "experience_orb": e = new XpOrb(); break;
                case "falling_block": e = new FallingBlockEntity(); break;
                case "tnt": e = new PrimedTnt(); break;
                case "arrow": e = new Arrow(); break;
                case "trident": e = new ThrownTrident(); break;
                case "minecart": e = new Minecart(); break;
                case "boat": e = new Boat(); break;
                case "end_crystal": e = new EndCrystal(); break;
                case "area_effect_cloud": e = new AreaEffectCloud(); break;
                default:
                    var def = MobRegistry.Get(type);
                    if (def != null) e = MobRegistry.Create(def, w);
                    break;
            }
            if (e != null) e.world = w;
            return e;
        }
    }
}
