using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Minecart with rail following, slopes, powered rails and variants (chest/hopper/tnt/furnace).</summary>
    public sealed class Minecart : Entity, IContainer
    {
        public string content; // null, chest, hopper, tnt, furnace
        public ItemStack[] items;
        public float damageTaken; int hurtTimer; public int fuel;
        public Vector3 pushDir;
        GameObject model; Renderer[] rends;
        public override string TypeId => "minecart";
        public override bool IsSolidToOthers => true;
        public override bool Attackable => true;
        public Minecart() { width = 0.98f; height = 0.7f; stepHeight = 0f; }

        public static Minecart Spawn(World w, Vector3 pos, string content)
        {
            var m = new Minecart { world = w, content = content };
            if (content == "chest") m.items = new ItemStack[27]; else if (content == "hopper") m.items = new ItemStack[5];
            m.SetPosition(pos);
            w.AddEntity(m);
            return m;
        }

        // IContainer
        public int Size => items?.Length ?? 0;
        public ItemStack Get(int i) => items[i];
        public void Set(int i, ItemStack s) { items[i] = s != null && s.IsEmpty ? null : s; }
        public int MaxStackSize => 64;
        public void SetChanged() { }
        public bool StillValid(Player p) => !removed && (p.position - position).sqrMagnitude < 64;

        public override bool CanBeRiddenBy(Entity e) => content == null && passengers.Count == 0;
        public override Vector3 PassengerOffset(Entity p) => new Vector3(0, 0.0625f + 0.35f, 0);
        public override void PositionRider(Entity p) { p.position = position + PassengerOffset(p) + new Vector3(0, -0.35f + 0.0625f, 0); }

        public override bool Interact(Player p, ItemStack held)
        {
            if (p.sneaking) return false;
            if (content == null) { if (passengers.Count == 0 && p.vehicle == null) { p.StartRiding(this); return true; } return false; }
            if (content == "chest") { p.OpenMenu(new ChestMenu(p, this, 3, "Minecart with Chest")); return true; }
            if (content == "hopper") { p.OpenMenu(new HopperMenu(p, this)); return true; }
            if (content == "furnace" && held != null && Recipes.FuelValue(held.item) > 0) { fuel += 3600; if (!p.IsCreative) held.count--; pushDir = new Vector3(position.x - p.position.x, 0, position.z - p.position.z).normalized; return true; }
            return false;
        }

        public override bool Hurt(DamageSource src, float amount)
        {
            if (removed) return false;
            if (src.isExplosion && content == "tnt") { Explode(); return true; }
            damageTaken += amount * 10; hurtTimer = 10;
            bool creative = src.attacker is Player pl && pl.IsCreative;
            if (damageTaken > 40 || creative)
            {
                if (!creative)
                {
                    world.SpawnItem(position, new ItemStack("minecart", 1));
                    if (content != null) world.SpawnItem(position, new ItemStack(content == "chest" ? "chest" : content, 1));
                    if (items != null) foreach (var s in items) if (s != null) world.SpawnItem(position, s);
                }
                if (content == "tnt" && src.isFire) { Explode(); return true; }
                Remove();
            }
            return true;
        }

        void Explode() { Remove(); Explosion.Explode(world, this, position, 4f + Mathf.Min(5f, velocity.magnitude * 5f), false, true); }

        public override void Tick()
        {
            base.Tick();
            if (removed) return;
            if (hurtTimer > 0) hurtTimer--;
            if (damageTaken > 0) damageTaken -= 1;
            Int3 bp = Int3.Floor(position);
            ushort s = world.GetState(bp);
            if (!(Blocks.ByState[s] is RailBlock)) { var below = bp.Offset(Dir.Down); if (world.GetBlock(below) is RailBlock) { bp = below; s = world.GetState(bp); } }
            var rb = Blocks.ByState[s] as RailBlock;
            if (rb != null) MoveAlongRail(bp, rb, s - rb.baseState);
            else
            {
                velocity.y -= 0.04f;
                Move(velocity);
                if (onGround) { velocity.x *= 0.5f; velocity.z *= 0.5f; }
                velocity *= 0.95f;
            }
            // rider push
            if (passengers.Count > 0 && passengers[0] is Player rp && rp.moveForward > 0 && rb != null)
            {
                Vector3 look = MathX.YawPitchToDir(rp.yaw, 0);
                if (new Vector2(velocity.x, velocity.z).sqrMagnitude < 0.01f) velocity += look * 0.02f;
            }
            if (content == "hopper" && age % 4 == 0) SuckItems();
            // push other carts/entities
            foreach (var e in world.GetEntities(Bounds.Grow(0.2f), this))
            {
                if (e is Minecart o && !o.removed)
                {
                    Vector3 d = o.position - position; d.y = 0;
                    if (d.sqrMagnitude < 1e-4f) continue;
                    var dn = d.normalized * 0.05f; o.velocity += dn; velocity -= dn;
                }
                else if (e is LivingEntity le && content == null && passengers.Count == 0 && !(e is Player) && e.vehicle == null && velocity.sqrMagnitude > 0.01f && !(e is Mob m && m.def.id == "iron_golem"))
                    le.StartRiding(this);
            }
            yaw = new Vector2(velocity.x, velocity.z).sqrMagnitude > 1e-4f ? MathX.YawFromDir(velocity) : yaw;
        }

        void MoveAlongRail(Int3 bp, RailBlock rb, int meta)
        {
            int shape = RailBlock.Shape(meta);
            fallDistance = 0;
            if (rb.kind == RailKind.Activator && RailBlock.Powered(meta))
            {
                if (content == "tnt") { Explode(); return; }
                if (passengers.Count > 0) passengers[0].StopRiding();
            }
            Rails.Ends(shape, out var e0, out var e1);
            Vector3 dir = new Vector3(e1.x - e0.x, 0, e1.z - e0.z).normalized;
            bool ascending = shape >= 2 && shape <= 5;
            // slope acceleration
            if (ascending)
            {
                Vector3 up = e1.y > e0.y ? new Vector3(e1.x, 0, e1.z) : new Vector3(e0.x, 0, e0.z);
                velocity -= up.normalized * 0.0078125f;
            }
            // project velocity onto rail direction
            float along = velocity.x * dir.x + velocity.z * dir.z;
            if (shape >= 6)
            {
                // curve: keep speed, steer along the two ends
                float sp = new Vector2(velocity.x, velocity.z).magnitude;
                Vector3 local = position - bp.Center; local.y = 0;
                Vector3 a = new Vector3(e0.x, 0, e0.z), b = new Vector3(e1.x, 0, e1.z);
                // moving toward whichever end is ahead
                Vector3 target = Vector3.Dot(new Vector3(velocity.x, 0, velocity.z), a) > Vector3.Dot(new Vector3(velocity.x, 0, velocity.z), b) ? a : b;
                Vector3 to = (target * 0.5f - local); to.y = 0;
                if (to.sqrMagnitude < 0.01f) to = target;
                Vector3 nd = to.normalized;
                velocity = new Vector3(nd.x * sp, 0, nd.z * sp);
            }
            else velocity = new Vector3(dir.x * along, 0, dir.z * along);
            // powered rails
            if (rb.kind == RailKind.Powered)
            {
                if (RailBlock.Powered(meta))
                {
                    float sp = velocity.magnitude;
                    if (sp > 0.01f) velocity += velocity / sp * 0.06f;
                    else
                    {
                        // start from standstill: push away from a solid block
                        if (world.GetBlock(bp.Offset(dir.x != 0 ? Dir.West : Dir.South)).solid) velocity = dir * 0.02f;
                        else if (world.GetBlock(bp.Offset(dir.x != 0 ? Dir.East : Dir.North)).solid) velocity = -dir * 0.02f;
                    }
                }
                else { velocity *= 0.5f; if (velocity.magnitude < 0.03f) velocity = Vector3.zero; }
            }
            if (content == "furnace" && fuel > 0) { fuel--; velocity += pushDir * 0.02f; if (age % 4 == 0) Particles.LargeSmoke(world, position + Vector3.up * 0.8f); }
            float max = inWater ? 0.2f : 0.4f;
            float spd = velocity.magnitude;
            if (spd > max) velocity *= max / spd;
            // center on rail laterally
            Vector3 p = position;
            if (shape == 0 || shape == 4 || shape == 5) p.x = bp.x + 0.5f;
            else if (shape == 1 || shape == 2 || shape == 3) p.z = bp.z + 0.5f;
            // height
            float fx = p.x - bp.x, fz = p.z - bp.z;
            float h = 0.0625f;
            if (shape == 2) h += fx; else if (shape == 3) h += 1 - fx; else if (shape == 4) h += fz; else if (shape == 5) h += 1 - fz;
            p.y = bp.y + h;
            position = p;
            Vector3 np = position + velocity;
            // stop at solid block ahead
            var nb = Int3.Floor(np + velocity.normalized * 0.5f);
            if (world.GetBlock(nb).opaqueCube && !(world.GetBlock(nb.Offset(Dir.Up)) is RailBlock) && !ascending) { velocity = -velocity * 0.4f; return; }
            position = np;
            // follow to next rail up/down
            Int3 nbp = Int3.Floor(position);
            if (!(world.GetBlock(nbp) is RailBlock) && world.GetBlock(nbp.Offset(Dir.Down)) is RailBlock) position.y = nbp.y - 1 + 0.0625f;
            velocity *= passengers.Count > 0 ? 0.997f : 0.96f;
            if (content == "chest" || content == "hopper") velocity *= 0.99f;
        }

        void SuckItems()
        {
            foreach (var e in world.GetEntities(Bounds.Grow(0.5f, 0.5f, 0.5f), this))
                if (e is ItemEntity ie && !ie.removed)
                {
                    for (int i = 0; i < items.Length && ie.stack.count > 0; i++)
                    {
                        if (items[i] == null) { items[i] = ie.stack.Copy(); ie.stack.count = 0; }
                        else if (items[i].Stackable(ie.stack)) { int n = Mathf.Min(ie.stack.count, items[i].MaxStack - items[i].count); items[i].count += n; ie.stack.count -= n; }
                    }
                    if (ie.stack.count <= 0) ie.Remove();
                }
        }

        public override void CreateVisual()
        {
            go = new GameObject("Minecart");
            model = ModelRenderer.Build(MobModels.Get("minecart"), go.transform);
            if (content != null)
            {
                var blk = content == "chest" ? "chest" : content;
                var inner = ItemRender.CreateBlockVisual(Blocks.StateOf(blk), 0.75f);
                if (inner != null) { inner.transform.SetParent(go.transform, false); inner.transform.localPosition = new Vector3(0, 0.3f, 0); }
            }
            rends = go.GetComponentsInChildren<Renderer>();
        }
        public override void Render(float partial)
        {
            if (go == null) return;
            go.transform.position = InterpPos(partial);
            float shake = hurtTimer > 0 ? Mathf.Sin(hurtTimer * 1.5f) * hurtTimer * 0.8f : 0;
            go.transform.rotation = Quaternion.Euler(0, Mathf.LerpAngle(prevYaw, yaw, partial), shake);
            if (rends != null) EntityLight.ApplyRenderers(rends, EntityLight.Sample(world, InterpPos(partial) + Vector3.up * 0.4f), 0, Vector4.zero);
        }
        public override void Save(Dictionary<string, string> d)
        {
            base.Save(d); if (content != null) d["content"] = content;
            if (items != null) { var p = new string[items.Length]; for (int i = 0; i < items.Length; i++) p[i] = items[i]?.Serialize() ?? ""; d["items"] = string.Join(";", p); }
            if (fuel > 0) d["fuel"] = fuel.ToString();
        }
        public override void Load(Dictionary<string, string> d)
        {
            base.Load(d);
            d.TryGetValue("content", out content);
            if (content == "chest") items = new ItemStack[27]; else if (content == "hopper") items = new ItemStack[5];
            if (items != null && d.TryGetValue("items", out var its)) { var p = its.Split(';'); for (int i = 0; i < items.Length && i < p.Length; i++) items[i] = ItemStack.Deserialize(p[i]); }
            if (d.TryGetValue("fuel", out var f)) int.TryParse(f, out fuel);
        }
    }

    // =====================================================================================================
    public sealed class Boat : Entity, IContainer
    {
        public string wood = "oak"; public bool chest;
        public ItemStack[] items;
        float damageTaken; int hurtTimer; float paddleL, paddleR;
        public float deltaRot;
        Renderer[] rends; Transform paddleLeft, paddleRight;
        public override string TypeId => "boat";
        public override bool IsSolidToOthers => true;
        public override bool Attackable => true;
        public Boat() { width = 1.375f; height = 0.5625f; }

        public static Boat Spawn(World w, Vector3 pos, string wood, bool chest, float yaw)
        {
            var b = new Boat { world = w, wood = wood, chest = chest, yaw = yaw, prevYaw = yaw };
            if (chest) b.items = new ItemStack[27];
            b.SetPosition(pos);
            w.AddEntity(b);
            return b;
        }

        public int Size => items?.Length ?? 0;
        public ItemStack Get(int i) => items[i];
        public void Set(int i, ItemStack s) { items[i] = s != null && s.IsEmpty ? null : s; }
        public int MaxStackSize => 64;
        public void SetChanged() { }
        public bool StillValid(Player p) => !removed && (p.position - position).sqrMagnitude < 64;

        int MaxPassengers => chest ? 1 : 2;
        public override bool CanBeRiddenBy(Entity e) => passengers.Count < MaxPassengers;
        public override void PositionRider(Entity p)
        {
            int i = passengers.IndexOf(p);
            float off = MaxPassengers == 1 || passengers.Count == 1 ? (chest ? 0.15f : 0f) : (i == 0 ? 0.2f : -0.6f);
            Vector3 o = Quaternion.Euler(0, yaw, 0) * new Vector3(0, 0, off);
            p.position = position + o + new Vector3(0, 0.1f, 0);
            if (p is LivingEntity le && !(p is Player)) le.yaw = yaw;
        }

        public override bool Interact(Player p, ItemStack held)
        {
            if (p.sneaking && chest) { p.OpenMenu(new ChestMenu(p, this, 3, "Boat with Chest")); return true; }
            if (p.vehicle == null && CanBeRiddenBy(p)) { p.StartRiding(this); return true; }
            return false;
        }

        public override bool Hurt(DamageSource src, float amount)
        {
            if (removed) return false;
            damageTaken += amount * 10; hurtTimer = 10;
            bool creative = src.attacker is Player pl && pl.IsCreative;
            if (damageTaken > 40 || creative)
            {
                if (!creative)
                {
                    string id = wood == "bamboo" ? (chest ? "bamboo_chest_raft" : "bamboo_raft") : wood + (chest ? "_chest_boat" : "_boat");
                    world.SpawnItem(position, new ItemStack(id, 1));
                    if (items != null) foreach (var s in items) if (s != null) world.SpawnItem(position, s);
                }
                foreach (var ps in passengers.ToArray()) ps.StopRiding();
                Remove();
            }
            return true;
        }

        public override void Tick()
        {
            base.Tick();
            if (removed) return;
            if (hurtTimer > 0) hurtTimer--;
            if (damageTaken > 0) damageTaken -= 1;
            // water level
            float waterTop = WaterSurface(out bool underwater);
            bool floating = waterTop > -1000;
            var driver = passengers.Count > 0 ? passengers[0] as Player : null;
            float invFriction = 0.05f;
            if (floating)
            {
                float target = waterTop - 0.4f;
                velocity.y += (target - position.y) * 0.25f;
                velocity.y *= 0.65f;
                invFriction = 0.9f;
                if (underwater) { velocity.y += 0.01f; }
            }
            else
            {
                velocity.y -= 0.04f;
                if (onGround)
                {
                    var below = world.GetBlock(Int3.Floor(position - Vector3.up * 0.1f));
                    invFriction = below.slipperiness > 0.9f ? 0.97f : 0.45f;
                }
                else invFriction = 0.9f;
            }
            if (driver != null)
            {
                float f = 0;
                if (driver.moveForward > 0.01f) f = 0.04f; else if (driver.moveForward < -0.01f) f = -0.005f;
                if (driver.moveStrafe < -0.01f) deltaRot += 1f; else if (driver.moveStrafe > 0.01f) deltaRot -= 1f;
                if (driver.moveStrafe != 0 && driver.moveForward == 0) f += 0.005f;
                yaw -= deltaRot;
                float r = yaw * Mathf.Deg2Rad;
                velocity.x += Mathf.Sin(r) * f; velocity.z += Mathf.Cos(r) * f;
                paddleL += (driver.moveForward != 0 || driver.moveStrafe > 0) ? 0.4f : 0; paddleR += (driver.moveForward != 0 || driver.moveStrafe < 0) ? 0.4f : 0;
            }
            deltaRot *= invFriction;
            velocity.x *= invFriction; velocity.z *= invFriction;
            Vector3 before = velocity;
            Move(velocity);
            if (horizontalCollision && new Vector2(before.x, before.z).magnitude > 0.35f) { }
            fallDistance = floating ? 0 : fallDistance;
            foreach (var e in world.GetEntities(Bounds.Grow(0.2f, -0.01f, 0.2f), this))
                if (e is LivingEntity le && !(e is Player) && e.vehicle == null && CanBeRiddenBy(e) && le.width < width && !(e is Mob m && (m.def.aquatic || m.def.id == "iron_golem")))
                    le.StartRiding(this);
        }

        float WaterSurface(out bool underwater)
        {
            underwater = false;
            Int3 p = Int3.Floor(position + Vector3.up * 0.1f);
            float best = -10000;
            for (int dy = 1; dy >= -1; dy--)
            {
                var q = p.Offset(0, dy, 0);
                if (world.IsWater(q))
                {
                    int meta = world.GetMeta(q);
                    float h = world.GetBlock(q).isLiquid ? MeshCtx.LiquidHeight(meta) : 1f;
                    if (world.IsWater(q.Offset(Dir.Up))) { h = 1f; if (dy >= 1) underwater = true; }
                    best = Mathf.Max(best, q.y + h);
                }
            }
            return best;
        }

        public override void CreateVisual()
        {
            go = new GameObject("Boat");
            var def = MobModels.Get(wood == "bamboo" ? "raft" : chest ? "chest_boat" : "boat");
            var root = ModelRenderer.Build(def, go.transform, "boat_" + wood);
            if (root != null)
            {
                paddleLeft = ModelRenderer.FindBone(root.transform, "paddle_left");
                paddleRight = ModelRenderer.FindBone(root.transform, "paddle_right");
            }
            rends = go.GetComponentsInChildren<Renderer>();
        }
        public override void Render(float partial)
        {
            if (go == null) return;
            go.transform.position = InterpPos(partial);
            float shake = hurtTimer > 0 ? Mathf.Sin(hurtTimer * 1.5f) * hurtTimer * 0.8f : 0;
            go.transform.rotation = Quaternion.Euler(0, Mathf.LerpAngle(prevYaw, yaw, partial), shake);
            if (paddleLeft != null) paddleLeft.localRotation = Quaternion.Euler(Mathf.Sin(paddleL) * 40f, 0, -20f);
            if (paddleRight != null) paddleRight.localRotation = Quaternion.Euler(Mathf.Sin(paddleR) * 40f, 0, 20f);
            if (rends != null) EntityLight.ApplyRenderers(rends, EntityLight.Sample(world, InterpPos(partial) + Vector3.up * 0.4f), 0, Vector4.zero);
        }
        public override void Save(Dictionary<string, string> d)
        {
            base.Save(d); d["wood"] = wood; d["chest"] = chest ? "1" : "0"; d["yaw"] = yaw.ToString("R");
            if (items != null) { var p = new string[items.Length]; for (int i = 0; i < items.Length; i++) p[i] = items[i]?.Serialize() ?? ""; d["items"] = string.Join(";", p); }
        }
        public override void Load(Dictionary<string, string> d)
        {
            base.Load(d);
            if (d.TryGetValue("wood", out var w)) wood = w;
            chest = d.TryGetValue("chest", out var c) && c == "1";
            if (chest) items = new ItemStack[27];
            if (items != null && d.TryGetValue("items", out var its)) { var p = its.Split(';'); for (int i = 0; i < items.Length && i < p.Length; i++) items[i] = ItemStack.Deserialize(p[i]); }
        }
    }

    // =====================================================================================================
    public sealed class EndCrystal : Entity
    {
        public bool showBottom = true;
        public Vector3? beamTarget;
        public int time;
        GameObject beam; LineRenderer beamLr;
        Transform frameOuter, frameInner, core;
        Renderer[] rends;
        public override string TypeId => "end_crystal";
        public override bool Attackable => true;
        public EndCrystal() { width = 2f; height = 2f; noPhysics = true; }

        public static EndCrystal Spawn(World w, Vector3 pos, bool bottom)
        {
            var c = new EndCrystal { world = w, showBottom = bottom };
            c.SetPosition(pos);
            c.time = Random.Range(0, 100000);
            w.AddEntity(c);
            return c;
        }

        public override void Tick()
        {
            age++; time++; prevPosition = position;
            Int3 p = Int3.Floor(position);
            if (world.dim == DimensionId.End && world.IsAir(p)) world.SetState(p, Blocks.Fire.DefaultState, SetFlags.Hooks);
        }

        public override bool Hurt(DamageSource src, float amount)
        {
            if (removed) return false;
            if (src.attacker is Mob m && m.def.id == "ender_dragon") return false;
            Remove();
            if (!src.isExplosion || true) Explosion.Explode(world, null, position, 6f, false, true);
            world.session?.dragonFight?.OnCrystalDestroyed(this, src);
            return true;
        }

        public override void CreateVisual()
        {
            go = new GameObject("EndCrystal");
            var root = ModelRenderer.Build(MobModels.Get("end_crystal"), go.transform);
            if (root != null)
            {
                frameOuter = ModelRenderer.FindBone(root.transform, "glass_outer");
                frameInner = ModelRenderer.FindBone(root.transform, "glass_inner");
                core = ModelRenderer.FindBone(root.transform, "cube");
                var bottom = ModelRenderer.FindBone(root.transform, "base");
                if (bottom != null) bottom.gameObject.SetActive(showBottom);
            }
            rends = go.GetComponentsInChildren<Renderer>();
        }

        public override void Render(float partial)
        {
            if (go == null) return;
            float t = time + partial;
            go.transform.position = InterpPos(partial);
            float bob = Mathf.Sin(t * 0.2f) / 2f + 0.5f; bob = (bob * bob + bob) * 0.4f;
            float spin = t * 3f;
            var up = new Vector3(0, 0.75f + bob, 0);
            if (frameOuter != null) { frameOuter.localPosition = up; frameOuter.localRotation = Quaternion.Euler(0, spin, 0) * Quaternion.Euler(60, 0, 45); }
            if (frameInner != null) { frameInner.localPosition = up; frameInner.localRotation = Quaternion.Euler(0, spin * 1.3f, 0) * Quaternion.Euler(60, 0, 45) * Quaternion.Euler(60, 0, 45); }
            if (core != null) { core.localPosition = up; core.localRotation = Quaternion.Euler(0, spin * 1.7f, 0) * Quaternion.Euler(60, 0, 45) * Quaternion.Euler(60, 0, 45) * Quaternion.Euler(60, 0, 45); }
            if (rends != null) EntityLight.ApplyRenderers(rends, new Vector2(1, 1), 0, Vector4.zero);
            if (beamTarget.HasValue)
            {
                if (beam == null)
                {
                    beam = new GameObject("CrystalBeam"); beam.transform.SetParent(go.transform, false);
                    beamLr = beam.AddComponent<LineRenderer>();
                    beamLr.material = Res.UnlitMaterial(Texture2D.whiteTexture, true);
                    beamLr.widthMultiplier = 0.25f; beamLr.positionCount = 2;
                    beamLr.startColor = new Color(1f, 0.7f, 1f, 0.9f); beamLr.endColor = new Color(0.8f, 0.4f, 1f, 0.9f);
                }
                beam.SetActive(true);
                beamLr.SetPosition(0, go.transform.position + up); beamLr.SetPosition(1, beamTarget.Value);
            }
            else if (beam != null) beam.SetActive(false);
        }
        public override void Save(Dictionary<string, string> d) { base.Save(d); d["bottom"] = showBottom ? "1" : "0"; }
        public override void Load(Dictionary<string, string> d) { base.Load(d); showBottom = !d.TryGetValue("bottom", out var b) || b == "1"; }
    }
}
