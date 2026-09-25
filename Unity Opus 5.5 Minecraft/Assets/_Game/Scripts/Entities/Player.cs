using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace MCR
{
    public sealed class Abilities
    {
        public bool flying, mayFly, instabuild, invulnerable, mayBuild = true;
        public float flySpeed = 0.05f, walkSpeed = 0.1f;
    }

    /// <summary>Food, saturation and exhaustion (vanilla-style rules).</summary>
    public sealed class HungerData
    {
        public int food = 20;
        public float saturation = 5f, exhaustion;
        int tickTimer;
        public int lastFood = 20;

        public void Eat(int nutrition, float satMod)
        {
            food = Math.Min(20, food + nutrition);
            saturation = Mathf.Min(saturation + nutrition * satMod * 2f, food);
        }
        public void AddExhaustion(float e) => exhaustion = Mathf.Min(exhaustion + e, 40f);
        public bool NeedsFood => food < 20;

        public void Tick(Player p)
        {
            var diff = p.world.session != null ? p.world.session.difficulty : Difficulty.Normal;
            lastFood = food;
            if (exhaustion > 4f)
            {
                exhaustion -= 4f;
                if (saturation > 0) saturation = Mathf.Max(saturation - 1f, 0);
                else if (diff != Difficulty.Peaceful) food = Math.Max(food - 1, 0);
            }
            bool regen = p.world.session == null || p.world.session.naturalRegeneration;
            if (regen && saturation > 0 && p.health < p.maxHealth && food >= 20)
            {
                if (++tickTimer >= 10) { float f = Mathf.Min(saturation, 6f); p.Heal(f / 6f); AddExhaustion(f); tickTimer = 0; }
            }
            else if (regen && food >= 18 && p.health < p.maxHealth)
            {
                if (++tickTimer >= 80) { p.Heal(1f); AddExhaustion(6f); tickTimer = 0; }
            }
            else if (food <= 0)
            {
                if (++tickTimer >= 80)
                {
                    if (p.health > 10f || diff == Difficulty.Hard || (p.health > 1f && diff == Difficulty.Normal)) p.Hurt(DamageSource.Starve, 1f);
                    tickTimer = 0;
                }
            }
            else tickTimer = 0;
            if (diff == Difficulty.Peaceful && p.world.tickCount % 20 == 0) { if (food < 20) food++; if (p.health < p.maxHealth) p.Heal(1f); }
        }
    }

    public sealed partial class Player : LivingEntity
    {
        public readonly PlayerInventory inventory;
        public readonly EnderChestContainer enderChest = new EnderChestContainer();
        public readonly HungerData hunger = new HungerData();
        public readonly Abilities abilities = new Abilities();
        public readonly HashSet<string> knownRecipes = new HashSet<string>();
        public GameMode gameMode = GameMode.Survival;
        public string playerName = "Steve";
        public int xpLevel; public float xpProgress; public int xpTotal; public int xpSeed;
        public int score;
        public bool IsLocal = true;
        public bool ThirdPerson => cameraMode != 0;
        public int cameraMode; // 0 first person, 1 back, 2 front
        public Menu menu;
        public InventoryMenu inventoryMenu;
        public FishingHook fishingHook;
        public int riptideTicks;
        public bool sleeping; public int sleepTimer; public Int3 sleepPos;
        public DimensionId spawnDim = DimensionId.Overworld; public Int3? spawnPos; public bool spawnForced;
        public float attackStrengthTicker = 100;
        public int useTicks; public ItemStack usingStack; public bool usingOffhand;
        readonly Dictionary<Item, (int start, int end)> cooldowns = new Dictionary<Item, (int, int)>();
        public int lastDamageTick = -100;
        public string lastDeathMessage;
        public int deathScreenTime;
        public bool wantsRespawn;
        public int jumpTriggerTime; // for double-tap flight
        public int sprintTriggerTime;
        public Vector3 lastSafePos;
        public int portalEffect; // nether portal overlay
        public float prevPortalEffect;
        public bool gliding;
        public int glideTicks;
        public int noDamageTicks;
        public int xpPickupCooldown;
        public float bob, prevBob, tilt, prevTilt;
        public int itemInHandTicks; public ItemStack lastHeld;
        public Entity lastHurtMob;
        public int blockBreakCooldown;
        public bool crouchVisual;
        public string lastActionBar;

        public Player(World w)
        {
            world = w;
            inventory = new PlayerInventory(this);
            width = 0.6f; height = 1.8f; stepHeight = 0.6f;
            movementSpeed = 0.1f;
            inventoryMenu = new InventoryMenu(this);
            persistent = true;
        }

        public override string TypeId => "player";
        public override string DisplayName => playerName;
        public override float EyeHeight => sleeping ? 0.2f : (gliding || (swimmingPose)) ? 0.4f : sneaking && !abilities.flying ? 1.27f : 1.62f;
        public bool swimmingPose;
        public bool IsCreative => gameMode == GameMode.Creative;
        public bool IsSpectator => gameMode == GameMode.Spectator;
        public bool IsSurvivalLike => gameMode == GameMode.Survival || gameMode == GameMode.Adventure;
        public bool CanBuild => abilities.mayBuild && gameMode != GameMode.Spectator && gameMode != GameMode.Adventure;
        public bool IsGliding => gliding;
        public bool IsBlocking => usingStack != null && usingStack.item is ShieldItem && useTicks >= 5;
        public bool IsUsingItem => usingStack != null;
        public override ItemStack MainHand { get => inventory.Selected; set => inventory.SetSelected(value); }
        public override ItemStack OffHand { get => inventory.offhand; set => inventory.offhand = value; }
        public override ItemStack GetArmor(int slot) => inventory.armor[slot];
        public float BlockReach => IsCreative ? 5f : 4.5f;
        public float EntityReach => IsCreative ? 5f : 3f;
        protected override int PortalWaitTime => IsCreative ? 1 : 80;
        public override bool Attackable => !IsSpectator;

        public void SetGameMode(GameMode m)
        {
            gameMode = m;
            abilities.mayFly = m == GameMode.Creative || m == GameMode.Spectator;
            abilities.instabuild = m == GameMode.Creative;
            abilities.invulnerable = m == GameMode.Creative || m == GameMode.Spectator;
            abilities.mayBuild = m != GameMode.Adventure && m != GameMode.Spectator;
            if (!abilities.mayFly) abilities.flying = false;
            if (m == GameMode.Spectator) abilities.flying = true;
            noPhysics = m == GameMode.Spectator;
            invulnerable = abilities.invulnerable;
            if (menu != null && m == GameMode.Spectator) CloseMenu();
        }

        // ------------------------------------------------------------------ menus
        public void OpenMenu(Menu m)
        {
            if (menu != null && menu != inventoryMenu) menu.Removed();
            menu = m;
            m.player = this;
            GameManager.Instance?.OnMenuOpened(m);
        }
        public void CloseMenu()
        {
            if (menu == null) return;
            var m = menu;
            menu = null;
            m.Removed();
            GameManager.Instance?.OnMenuClosed(m);
        }
        public void OpenInventory()
        {
            if (IsCreative) { GameManager.Instance?.OpenCreativeInventory(); return; }
            OpenMenu(inventoryMenu);
        }

        // ------------------------------------------------------------------ items
        public void DropItem(ItemStack s, bool throwForward, bool all = true)
        {
            if (s == null || s.IsEmpty || world == null) return;
            Vector3 pos = EyePosition - Vector3.up * 0.3f;
            Vector3 vel;
            if (throwForward) vel = LookDir * 0.3f + new Vector3(0, 0.1f, 0);
            else { float a = UnityEngine.Random.value * Mathf.PI * 2; vel = new Vector3(Mathf.Cos(a) * 0.2f, 0.2f, Mathf.Sin(a) * 0.2f); }
            var e = world.SpawnItem(pos, s, vel, 40);
            if (e != null) e.thrower = this;
        }

        public void DropSelected(bool wholeStack)
        {
            var s = inventory.Selected;
            if (s == null) return;
            var d = s.Split(wholeStack ? s.count : 1);
            if (s.count <= 0) inventory.SetSelected(null);
            DropItem(d, true);
            SwingArm();
        }

        public ItemStack FindAmmo(bool crossbow = false)
        {
            bool Ok(ItemStack s) => s != null && !s.IsEmpty && (s.item.id == "arrow" || s.item.id == "spectral_arrow" || s.item.id == "tipped_arrow" || (crossbow && s.item.id == "firework_rocket"));
            if (Ok(inventory.offhand)) return inventory.offhand;
            if (Ok(inventory.Selected)) return inventory.Selected;
            for (int i = 0; i < 36; i++) if (Ok(inventory.main[i])) return inventory.main[i];
            return IsCreative ? new ItemStack("arrow", 1) : null;
        }

        public bool CanEat(bool always) => always || hunger.NeedsFood || IsCreative;

        public void StartUsing(ItemStack s)
        {
            usingStack = s; useTicks = 0;
            usingOffhand = s == inventory.offhand;
        }
        public void StopUsing()
        {
            if (usingStack != null && !usingStack.IsEmpty) usingStack.item.ReleaseUsing(world, this, usingStack, useTicks);
            usingStack = null; useTicks = 0;
            inventory.Cleanup();
        }
        public void CancelUsing() { usingStack = null; useTicks = 0; }

        void TickUsing()
        {
            if (usingStack == null) return;
            var held = usingOffhand ? inventory.offhand : inventory.Selected;
            if (held != usingStack || usingStack.IsEmpty) { usingStack = null; useTicks = 0; return; }
            useTicks++;
            var it = usingStack.item;
            it.UsingTick(world, this, usingStack, useTicks);
            int dur = it.UseDuration(usingStack);
            if (dur > 0 && dur < 72000 && useTicks >= dur)
            {
                var s = usingStack;
                usingStack = null;
                it.FinishUsing(world, this, s);
                useTicks = 0;
                inventory.Cleanup();
            }
        }

        public void SetCooldown(Item it, int ticks) { cooldowns[it] = (age, age + ticks); }
        public bool OnCooldown(Item it) => it != null && cooldowns.TryGetValue(it, out var c) && age < c.end;
        public float CooldownFraction(Item it)
        {
            if (it == null || !cooldowns.TryGetValue(it, out var c)) return 0;
            if (age >= c.end) return 0;
            return (c.end - age) / (float)Mathf.Max(1, c.end - c.start);
        }

        public void OnCrafted(Recipe r, ItemStack result)
        {
            if (result == null) return;
            hunger.AddExhaustion(0f);
            Achievements.OnCraft(this, result.item.id);
        }

        public void GiveXp(int amount)
        {
            if (amount <= 0) return;
            score += amount;
            xpProgress += amount / (float)XpNeeded;
            xpTotal = Math.Min(int.MaxValue - amount, xpTotal + amount);
            while (xpProgress >= 1f)
            {
                xpProgress = (xpProgress - 1f) * XpNeeded;
                xpLevel++;
                xpProgress /= XpNeeded;
                if (xpLevel % 5 == 0) Sounds.Play("entity.player.levelup", position, 0.75f, 1f);
            }
        }
        public void AddLevels(int n)
        {
            xpLevel = Math.Max(0, xpLevel + n);
            if (xpLevel == 0) xpProgress = 0;
            if (n > 0 && xpLevel % 5 == 0) Sounds.Play("entity.player.levelup", position, 0.75f, 1f);
        }
        public int XpNeeded => xpLevel >= 30 ? 112 + (xpLevel - 30) * 9 : xpLevel >= 15 ? 37 + (xpLevel - 15) * 5 : 7 + xpLevel * 2;

        // ------------------------------------------------------------------ raycasts
        public bool RaycastBlocks(bool fluids, out BlockHit hit) => world.RaycastBlocks(EyePosition, LookDir, BlockReach, fluids, out hit);

        public Entity RaycastEntity(float reach)
        {
            Vector3 eye = EyePosition, dir = LookDir;
            float blockDist = reach;
            if (world.RaycastBlocks(eye, dir, reach, false, out var bh)) blockDist = bh.distance;
            Entity best = null; float bestT = blockDist;
            var area = new AABB(eye, eye).Expand(dir * reach).Grow(1f);
            foreach (var e in world.GetEntities(area, this))
            {
                if (e.removed || !e.Attackable && !(e is Minecart) && !(e is Boat) && !(e is ItemFrameLike) || e == vehicle) continue;
                if (e is LivingEntity le && le.dead) continue;
                var b = e.Bounds.Grow(0.1f);
                if (b.Contains(eye)) { if (0 < bestT) { best = e; bestT = 0; } continue; }
                if (b.Raycast(eye, dir, reach, out float t, out _) && t < bestT) { bestT = t; best = e; }
            }
            return best;
        }

        // ------------------------------------------------------------------ tick
        public override void Tick()
        {
            if (portalCooldown > 0 && inNetherPortal == false) { }
            prevBob = bob; prevTilt = tilt; prevPortalEffect = portalEffect;
            base.Tick();
            if (dead) { deathScreenTime++; return; }
            if (world == null) return;
            attackStrengthTicker++;
            if (xpPickupCooldown > 0) xpPickupCooldown--;
            if (blockBreakCooldown > 0) blockBreakCooldown--;
            if (riptideTicks > 0) { riptideTicks--; AttackAlongRiptide(); }
            TickUsing();
            if (!IsCreative && !IsSpectator) hunger.Tick(this);
            if (IsCreative || IsSpectator) { hunger.food = 20; hunger.saturation = 5; health = Mathf.Max(health, 0.1f); fireTicks = IsSpectator ? 0 : fireTicks; air = maxAir; }
            TickArmorEffects();
            PickupItems();
            foreach (var s in inventory.main) if (s != null) s.item.InventoryTick(s, this, 0, s == inventory.Selected);
            // walking bob / tilt
            float hs = new Vector2(position.x - prevPosition.x, position.z - prevPosition.z).magnitude;
            if (!onGround || dead) hs = 0;
            bob += (Mathf.Min(0.1f, hs) - bob) * 0.4f;
            float tl = (float)(Math.Atan(-velocity.y * 0.2f) * 15.0);
            if (onGround || dead) tl = 0;
            tilt += (tl - tilt) * 0.8f;
            // portal overlay
            if (inNetherPortalVisual) portalEffect = Math.Min(80, portalEffect + 1); else portalEffect = Math.Max(0, portalEffect - 2);
            inNetherPortalVisual = false;
            if (sleeping) TickSleep();
            if (onGround && !inWater && !inLava) lastSafePos = position;
            // held item change animation
            var held = inventory.Selected;
            if (held != lastHeld) { itemInHandTicks = 0; lastHeld = held; } else itemInHandTicks++;
            // exhaustion from movement
            if (!IsCreative && !IsSpectator)
            {
                float dx = position.x - prevPosition.x, dz = position.z - prevPosition.z, dy = position.y - prevPosition.y;
                float d = Mathf.Sqrt(dx * dx + dz * dz);
                if (eyeInWater || inWater) hunger.AddExhaustion(0.01f * Mathf.Sqrt(dx * dx + dy * dy + dz * dz));
                else if (onGround && sprinting) hunger.AddExhaustion(0.1f * d);
            }
            if (fishingHook != null && fishingHook.removed) fishingHook = null;
        }
        public bool inNetherPortalVisual;

        public override void RideTick()
        {
            base.RideTick();
            TickUsing();
            PickupItems();
            if (!IsCreative) hunger.Tick(this);
            attackStrengthTicker++;
        }

        void TickArmorEffects()
        {
            var head = inventory.armor[3];
            if (head != null && head.item.id == "turtle_helmet" && !eyeInWater) AddEffect(new EffectInstance(Effect.WaterBreathing, 200, 0, true) { showParticles = false });
            // soul speed, frost walker
            var feet = inventory.armor[0];
            if (feet != null && onGround)
            {
                int fw = feet.GetEnchant(Enchant.FrostWalker);
                if (fw > 0) FrostWalk(fw);
            }
        }
        void FrostWalk(int level)
        {
            int r = 2 + level;
            Int3 c = Int3.Floor(position - Vector3.up * 0.5f);
            var ice = Blocks.Get("frosted_ice") ?? Blocks.Get("ice");
            for (int dx = -r; dx <= r; dx++)
                for (int dz = -r; dz <= r; dz++)
                {
                    if (dx * dx + dz * dz > r * r) continue;
                    var p = new Int3(c.x + dx, c.y, c.z + dz);
                    if (world.GetState(p) == Blocks.Water.DefaultState && world.IsAir(p.Offset(Dir.Up))) world.SetState(p, ice.DefaultState);
                }
        }

        void PickupItems()
        {
            if (IsSpectator) return;
            var box = Bounds.Grow(1f, 0.5f, 1f);
            foreach (var e in world.GetEntities(box, this))
            {
                if (e.removed) continue;
                if (e is ItemEntity ie) ie.TryPickup(this);
                else if (e is XpOrb xo) xo.TryPickup(this);
                else if (e is Arrow ar) ar.TryPickup(this);
                else if (e is ThrownTrident tt) tt.TryPickup(this);
            }
        }

        void AttackAlongRiptide()
        {
            foreach (var e in world.GetEntities(Bounds.Grow(0.5f), this))
                if (e is LivingEntity le && le.Attackable && !le.dead) { le.Hurt(DamageSource.PlayerAttack(this), 8f); riptideTicks = 0; velocity *= -0.2f; break; }
        }

        public override void Jump()
        {
            base.Jump();
            if (!IsCreative) hunger.AddExhaustion(sprinting ? 0.2f : 0.05f);
        }

        protected override void AiStep()
        {
            if (IsSpectator)
            {
                noPhysics = true; abilities.flying = true; isFlying = true;
                fallDistance = 0; onGround = false;
            }
            isFlying = abilities.flying;
            flyingSpeed = abilities.flySpeed * (sprinting ? 2f : 1f);
            movementSpeed = abilities.walkSpeed;
            if (isFlying)
            {
                float v = abilities.flySpeed * 3f;
                if (jumping) velocity.y += v;
                if (sneaking) velocity.y -= v;
                if (onGround && !IsSpectator && sneaking) { abilities.flying = false; isFlying = false; }
                // flight uses its own travel in LivingEntity; bypass jump logic
                Travel(moveStrafe, moveForward);
                if (onGround && !IsSpectator && velocity.y <= 0) { }
                return;
            }
            // elytra gliding
            var chest = inventory.armor[2];
            if (gliding)
            {
                if (onGround || inWater || chest == null || chest.item.id != "elytra" || chest.damage >= chest.MaxDamage - 1) { gliding = false; }
                else { GlideTravel(); glideTicks++; if (glideTicks % 20 == 0 && !IsCreative) chest.HurtAndBreak(1, this); return; }
            }
            // swimming (sprint in water)
            swimmingPose = sprinting && eyeInWater && inWater;
            if (swimmingPose)
            {
                Vector3 look = LookDir;
                float up = look.y;
                if (up < -0.2f || (jumping)) velocity.y += (up * 0.06f - velocity.y) * (up < -0.2f ? 0.085f : 0.06f);
                if (jumping) velocity.y += 0.04f;
            }
            base.AiStep();
        }

        public bool TryStartGliding()
        {
            var chest = inventory.armor[2];
            if (onGround || gliding || inWater || abilities.flying || vehicle != null) return false;
            if (chest == null || chest.item.id != "elytra" || chest.damage >= chest.MaxDamage - 1) return false;
            gliding = true; glideTicks = 0;
            return true;
        }

        void GlideTravel()
        {
            // elytra flight model: pitch trades altitude for speed
            Vector3 look = LookDir;
            float pitchRad = pitch * Mathf.Deg2Rad;
            float hLook = Mathf.Sqrt(look.x * look.x + look.z * look.z);
            float hVel = Mathf.Sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
            float lookLen = look.magnitude;
            float cosP = Mathf.Cos(pitchRad);
            cosP = cosP * cosP * Mathf.Min(1f, lookLen / 0.4f);
            velocity.y += -0.08f + cosP * 0.06f;
            if (velocity.y < 0 && hLook > 0)
            {
                float lift = velocity.y * -0.1f * cosP;
                velocity.x += look.x * lift / hLook; velocity.y += lift; velocity.z += look.z * lift / hLook;
            }
            if (pitchRad < 0 && hLook > 0)
            {
                float climb = hVel * -Mathf.Sin(pitchRad) * 0.04f;
                velocity.x -= look.x * climb / hLook; velocity.y += climb * 3.2f; velocity.z -= look.z * climb / hLook;
            }
            if (hLook > 0)
            {
                velocity.x += (look.x / hLook * hVel - velocity.x) * 0.1f;
                velocity.z += (look.z / hLook * hVel - velocity.z) * 0.1f;
            }
            velocity.x *= 0.99f; velocity.y *= 0.98f; velocity.z *= 0.99f;
            Vector3 before = velocity;
            Move(velocity);
            if (horizontalCollision)
            {
                float impact = (new Vector2(before.x, before.z).magnitude - new Vector2(velocity.x, velocity.z).magnitude) * 10f - 3f;
                if (impact > 0) Hurt(DamageSource.FlyIntoWall, impact);
            }
            fallDistance = velocity.y < -0.5f ? fallDistance : 0;
        }

        // ------------------------------------------------------------------ damage
        protected override bool IsInvulnerableTo(DamageSource src)
        {
            if (IsSpectator && src != DamageSource.Void && src != DamageSource.Kill) return true;
            if (IsCreative && !src.bypassInvul) return true;
            return base.IsInvulnerableTo(src);
        }

        public override bool Hurt(DamageSource src, float amount)
        {
            if (sleeping) WakeUp();
            bool r = base.Hurt(src, amount);
            if (r)
            {
                lastDamageTick = age;
                hunger.AddExhaustion(0.1f);
                if (usingStack != null && usingStack.item is FoodItem) { }
            }
            return r;
        }

        public override string HurtSound => "entity.player.hurt";
        public override string DeathSound => "entity.player.death";
        public override float VoicePitch => 1f;

        public void OnShieldBlock(DamageSource src, float amount)
        {
            Sounds.Play("item.shield.block", position, 1f, 0.8f + UnityEngine.Random.value * 0.4f);
            var sh = usingStack;
            if (sh != null && amount >= 3f) { if (sh.HurtAndBreak(1 + Mathf.FloorToInt(amount), this)) { inventory.Cleanup(); usingStack = null; Sounds.Play("item.shield.break", position, 1f, 1f); } }
            if (src.direct is LivingEntity att && !src.isProjectile) att.Knockback(0.5f, position.x - att.position.x, position.z - att.position.z);
            if (src.attacker is Mob m && m.def.id == "ravager") { }
        }

        protected override void DamageArmor(float amount)
        {
            if (amount <= 0 || IsCreative) return;
            int d = Mathf.Max(1, Mathf.FloorToInt(amount / 4f));
            for (int i = 0; i < 4; i++)
            {
                var a = inventory.armor[i];
                if (a != null && a.item is ArmorItem) { if (a.HurtAndBreak(d, this)) inventory.armor[i] = null; }
            }
        }

        public override void Die(DamageSource src)
        {
            // totem of undying
            ItemStack totem = null;
            if (inventory.Selected != null && inventory.Selected.item.id == "totem_of_undying") totem = inventory.Selected;
            else if (inventory.offhand != null && inventory.offhand.item.id == "totem_of_undying") totem = inventory.offhand;
            if (totem != null && src != DamageSource.Void && src != DamageSource.Kill)
            {
                totem.count--; inventory.Cleanup();
                health = 1f; ClearEffects();
                AddEffect(new EffectInstance(Effect.Regeneration, 900, 1)); AddEffect(new EffectInstance(Effect.Absorption, 100, 1)); AddEffect(new EffectInstance(Effect.FireResistance, 800, 0));
                Particles.Totem(world, position + Vector3.up);
                Sounds.Play("item.totem.use", position, 1f, 1f);
                GameManager.Instance?.hud?.ShowTotem();
                return;
            }
            base.Die(src);
            lastDeathMessage = src.DeathMessage(playerName);
            GameManager.Instance?.hud?.Chat(lastDeathMessage);
            CloseMenu();
            if (world.session == null || !world.session.keepInventory)
            {
                for (int i = 0; i < 41; i++)
                {
                    var s = inventory.Get(i);
                    if (s == null) continue;
                    if (s.GetEnchant(Enchant.VanishingCurse) > 0) { inventory.Set(i, null); continue; }
                    DropItem(s, false); inventory.Set(i, null);
                }
                int xp = Mathf.Min(xpLevel * 7, 100);
                if (xp > 0 && !IsSpectator) XpOrb.Spawn(world, position, xp);
                xpLevel = 0; xpProgress = 0; xpTotal = 0;
            }
            deathScreenTime = 0;
            sleeping = false;
        }

        public void Respawn()
        {
            var s = world.session;
            World target = s != null ? s.GetWorld(spawnDim) : world;
            Vector3 pos;
            bool ok = false;
            pos = Vector3.zero;
            if (spawnPos.HasValue)
            {
                var sp = spawnPos.Value;
                var tw = target;
                GameManager.Instance?.EnsureChunksAround(tw, sp.Center, 1);
                var b = tw.GetBlock(sp);
                if (b is BedBlock || (b.id == "respawn_anchor" && tw.GetMeta(sp) > 0) || spawnForced)
                {
                    var free = FindFreeAround(tw, sp);
                    if (free.HasValue)
                    {
                        pos = free.Value; ok = true;
                        if (b.id == "respawn_anchor" && !IsCreative) tw.SetMeta(sp, tw.GetMeta(sp) - 1);
                    }
                }
                if (!ok) { GameManager.Instance?.hud?.Chat("You have no home bed or charged respawn anchor, or it was obstructed"); spawnPos = null; }
            }
            if (!ok)
            {
                target = s != null ? s.Overworld : world;
                pos = s?.worldSpawn ?? target.generator.FindSpawn();
            }
            dead = false; health = maxHealth; hunger.food = 20; hunger.saturation = 5; hunger.exhaustion = 0;
            fireTicks = 0; fallDistance = 0; air = maxAir; ClearEffects(); deathTime = 0; gliding = false;
            velocity = Vector3.zero; absorption = 0;
            GameManager.Instance?.ChangeDimension(this, target, pos, true);
        }

        static Vector3? FindFreeAround(World w, Int3 p)
        {
            for (int r = 0; r <= 2; r++)
                for (int dx = -r; dx <= r; dx++)
                    for (int dz = -r; dz <= r; dz++)
                        for (int dy = -1; dy <= 1; dy++)
                        {
                            var q = new Int3(p.x + dx, p.y + dy, p.z + dz);
                            if (!w.GetBlock(q).solid && !w.GetBlock(q.Offset(Dir.Up)).solid && w.GetBlock(q.Offset(Dir.Down)).solid) return new Vector3(q.x + 0.5f, q.y, q.z + 0.5f);
                        }
            return null;
        }

        protected override void OnVoid()
        {
            if (IsCreative || IsSpectator) { if (position.y < world.minY - 128) Hurt(DamageSource.Void, 4f); return; }
            Hurt(DamageSource.Void, 4f);
        }

        // ------------------------------------------------------------------ attacking
        public float AttackStrength(float partial = 0)
        {
            var it = inventory.Selected?.item;
            float speed = it != null ? it.attackSpeed : 4f;
            float delay = 20f / speed;
            return Mathf.Clamp01((attackStrengthTicker + partial) / delay);
        }

        public void Attack(Entity target)
        {
            if (target == null || IsSpectator) return;
            var held = inventory.Selected;
            if (target is Minecart || target is Boat || target is EndCrystal || target is ItemFrameLike)
            {
                target.Hurt(DamageSource.PlayerAttack(this), IsCreative ? 100f : 4f);
                attackStrengthTicker = 0; SwingArm();
                return;
            }
            if (!(target is LivingEntity le) || !target.Attackable) return;
            float baseDmg = held != null ? held.item.attackDamage : 1f;
            int str = GetEffectLevel(Effect.Strength); if (str >= 0) baseDmg += 3f * (str + 1);
            int weak = GetEffectLevel(Effect.Weakness); if (weak >= 0) baseDmg -= 4f * (weak + 1);
            float s = AttackStrength(0.5f);
            float dmg = baseDmg * (0.2f + s * s * 0.8f);
            float ench = 0;
            if (held != null)
            {
                int sh = held.GetEnchant(Enchant.Sharpness); if (sh > 0) ench += 0.5f * sh + 0.5f;
                if (le.undead) ench += 2.5f * held.GetEnchant(Enchant.Smite);
                if (le.arthropod) ench += 2.5f * held.GetEnchant(Enchant.BaneOfArthropods);
                if (held.item.id == "mace" && fallDistance > 1.5f)
                {
                    float fd = fallDistance;
                    float bonus = fd <= 3 ? 4 * fd : fd <= 8 ? 12 + 2 * (fd - 3) : 22 + (fd - 8);
                    bonus += held.GetEnchant(Enchant.Density) * 0.5f * fd;
                    dmg += bonus;
                    velocity.y = 0.1f; fallDistance = 0;
                    Sounds.Play(fd > 5 ? "item.mace.smash_ground_heavy" : "item.mace.smash_ground", target.position, 1f, 1f);
                    Particles.BlockDust(world, target.position, world.GetState(Int3.Floor(target.position - Vector3.up * 0.2f)), 20);
                }
            }
            ench *= s;
            bool strong = s > 0.9f;
            bool knock = sprinting && strong;
            bool crit = strong && fallDistance > 0 && !onGround && !OnClimbable() && !inWater && GetEffectLevel(Effect.Blindness) < 0 && vehicle == null && !sprinting;
            if (crit) dmg *= 1.5f;
            dmg += ench;
            bool sweep = strong && !crit && !knock && onGround && held != null && held.item.toolType == ToolType.Sword && new Vector2(position.x - prevPosition.x, position.z - prevPosition.z).magnitude < movementSpeed * 2.5f;
            float hpBefore = le.health;
            int fire = held != null ? held.GetEnchant(Enchant.FireAspect) : 0;
            if (fire > 0 && !le.onFire) le.SetOnFire(1);
            bool hurt = le.Hurt(DamageSource.PlayerAttack(this), dmg);
            if (hurt)
            {
                int kb = (held != null ? held.GetEnchant(Enchant.Knockback) : 0) + (knock ? 1 : 0);
                if (kb > 0)
                {
                    float r = yaw * Mathf.Deg2Rad;
                    le.Knockback(kb * 0.5f, -Mathf.Sin(r), -Mathf.Cos(r));
                    velocity.x *= 0.6f; velocity.z *= 0.6f; sprinting = false;
                }
                if (sweep)
                {
                    float sweepDmg = 1f + (held.GetEnchant(Enchant.SweepingEdge) > 0 ? dmg * (held.GetEnchant(Enchant.SweepingEdge) / (held.GetEnchant(Enchant.SweepingEdge) + 1f)) : 0);
                    foreach (var e in world.GetEntities(target.Bounds.Grow(1f, 0.25f, 1f), this))
                        if (e != target && e is LivingEntity o && !(o is Player) && (o.position - position).sqrMagnitude < 9)
                        {
                            float rr = yaw * Mathf.Deg2Rad;
                            o.Knockback(0.4f, -Mathf.Sin(rr), -Mathf.Cos(rr));
                            o.Hurt(DamageSource.PlayerAttack(this), sweepDmg);
                        }
                    Sounds.Play("entity.player.attack.sweep", position, 1f, 1f);
                    Particles.Sweep(world, position + LookDir * 1.2f + Vector3.up * 1.2f);
                }
                if (crit) { Sounds.Play("entity.player.attack.crit", position, 1f, 1f); Particles.Crit(world, target.position + Vector3.up * target.height * 0.5f, 12); }
                else if (strong && !sweep) Sounds.Play(knock ? "entity.player.attack.knockback" : "entity.player.attack.strong", position, 1f, 1f);
                else Sounds.Play("entity.player.attack.weak", position, 1f, 1f);
                if (ench > 0) Particles.MagicCrit(world, target.position + Vector3.up * target.height * 0.5f, 8);
                if (fire > 0) le.SetOnFire(fire * 4);
                if (held != null) { held.item.OnHitEntity(held, le, this); if (held.IsEmpty) inventory.SetSelected(null); }
                int thorns = le.TotalArmorEnchant(Enchant.Thorns);
                if (thorns > 0 && UnityEngine.Random.value < 0.15f * thorns) Hurt(DamageSource.MobAttack(le), 1 + UnityEngine.Random.Range(0, 4));
                float dealt = hpBefore - le.health;
                if (dealt > 2f) Particles.DamageIndicator(world, target.position + Vector3.up * target.height * 0.5f, Mathf.Min(10, (int)(dealt * 0.5f)));
                lastHurtMob = le;
                hunger.AddExhaustion(0.1f);
            }
            else Sounds.Play("entity.player.attack.nodamage", position, 1f, 1f);
            attackStrengthTicker = 0;
        }

        // ------------------------------------------------------------------ misc actions
        public void ChorusTeleport()
        {
            for (int i = 0; i < 16; i++)
            {
                var p = position + new Vector3(UnityEngine.Random.Range(-8f, 8f), UnityEngine.Random.Range(-8, 8), UnityEngine.Random.Range(-8f, 8f));
                var bp = Int3.Floor(p);
                for (int k = 0; k < 16 && !world.GetBlock(bp.Offset(Dir.Down)).solid && bp.y > world.minY; k++) bp = bp.Offset(Dir.Down);
                if (world.GetBlock(bp.Offset(Dir.Down)).solid && !world.GetBlock(bp).solid && !world.GetBlock(bp.Offset(Dir.Up)).solid)
                {
                    Sounds.Play("entity.player.teleport", position, 1f, 1f);
                    Teleport(new Vector3(bp.x + 0.5f, bp.y, bp.z + 0.5f));
                    Sounds.Play("entity.player.teleport", position, 1f, 1f);
                    return;
                }
            }
        }

        public void SetSpawnPoint(DimensionId d, Int3 pos, bool forced)
        {
            spawnDim = d; spawnPos = pos; spawnForced = forced;
        }

        public bool TrySleep(Int3 bedPos)
        {
            var s = world.session;
            if (world.dim != DimensionId.Overworld) return false;
            if (s != null && !s.IsNight && !s.IsThundering) { GameManager.Instance?.hud?.ShowActionBar("You can only sleep at night or during thunderstorms"); SetSpawnPoint(world.dim, bedPos, false); return false; }
            if ((position - bedPos.Center).sqrMagnitude > 9) { GameManager.Instance?.hud?.ShowActionBar("You may not rest now; the bed is too far away"); return false; }
            foreach (var e in world.GetEntities(new AABB(bedPos.x - 8, bedPos.y - 5, bedPos.z - 8, bedPos.x + 8, bedPos.y + 5, bedPos.z + 8)))
                if (e is Mob m && m.def.hostile && !m.dead) { GameManager.Instance?.hud?.ShowActionBar("You may not rest now; there are monsters nearby"); return false; }
            SetSpawnPoint(world.dim, bedPos, false);
            GameManager.Instance?.hud?.ShowActionBar("Respawn point set");
            sleeping = true; sleepTimer = 0; sleepPos = bedPos;
            Teleport(new Vector3(bedPos.x + 0.5f, bedPos.y + 0.5625f, bedPos.z + 0.5f));
            velocity = Vector3.zero;
            return true;
        }

        void TickSleep()
        {
            sleepTimer++;
            if (!(world.GetBlock(sleepPos) is BedBlock)) { WakeUp(); return; }
            velocity = Vector3.zero;
            if (sleepTimer >= 100)
            {
                var s = world.session;
                if (s != null)
                {
                    long day = s.dayTime / 24000;
                    s.dayTime = (day + 1) * 24000;
                    s.SetWeather("clear", UnityEngine.Random.Range(12000, 180000));
                }
                WakeUp();
            }
        }
        public void WakeUp()
        {
            if (!sleeping) return;
            sleeping = false; sleepTimer = 0;
            var free = FindFreeAround(world, sleepPos);
            if (free.HasValue) Teleport(free.Value);
        }

        // ------------------------------------------------------------------ persistence
        public void SaveFull(Dictionary<string, string> d)
        {
            var ci = CultureInfo.InvariantCulture;
            d["dim"] = ((int)world.dim).ToString();
            d["x"] = position.x.ToString("R", ci); d["y"] = position.y.ToString("R", ci); d["z"] = position.z.ToString("R", ci);
            d["yaw"] = yaw.ToString("R", ci); d["pitch"] = pitch.ToString("R", ci);
            d["mode"] = ((int)gameMode).ToString(); d["flying"] = abilities.flying ? "1" : "0";
            d["hp"] = health.ToString("R", ci); d["food"] = hunger.food.ToString(); d["sat"] = hunger.saturation.ToString("R", ci); d["exh"] = hunger.exhaustion.ToString("R", ci);
            d["xpl"] = xpLevel.ToString(); d["xpp"] = xpProgress.ToString("R", ci); d["xpt"] = xpTotal.ToString();
            d["inv"] = inventory.Serialize(); d["ender"] = enderChest.Serialize();
            d["air"] = air.ToString(); d["fire"] = fireTicks.ToString(); d["dead"] = dead ? "1" : "0";
            if (spawnPos.HasValue) { var p = spawnPos.Value; d["spawn"] = (int)spawnDim + "," + p.x + "," + p.y + "," + p.z + "," + (spawnForced ? 1 : 0); }
            if (effects.Count > 0) { var list = new List<string>(); foreach (var e in effects.Values) list.Add(e.Serialize()); d["fx"] = string.Join(",", list); }
            if (knownRecipes.Count > 0) d["recipes"] = string.Join(",", knownRecipes);
            d["name"] = playerName;
            d["adv"] = Achievements.Serialize();
        }

        public void LoadFull(Dictionary<string, string> d)
        {
            var ci = CultureInfo.InvariantCulture;
            float F(string k, float def) => d.TryGetValue(k, out var v) && float.TryParse(v, NumberStyles.Float, ci, out var f) ? f : def;
            int I(string k, int def) => d.TryGetValue(k, out var v) && int.TryParse(v, out var i) ? i : def;
            SetPosition(new Vector3(F("x", 0), F("y", 80), F("z", 0)));
            yaw = prevYaw = F("yaw", 0); pitch = prevPitch = F("pitch", 0);
            SetGameMode((GameMode)I("mode", 1));
            abilities.flying = I("flying", 0) == 1 && abilities.mayFly;
            health = F("hp", 20); hunger.food = I("food", 20); hunger.saturation = F("sat", 5); hunger.exhaustion = F("exh", 0);
            xpLevel = I("xpl", 0); xpProgress = F("xpp", 0); xpTotal = I("xpt", 0);
            if (d.TryGetValue("inv", out var inv)) inventory.Deserialize(inv);
            if (d.TryGetValue("ender", out var en)) enderChest.Deserialize(en);
            air = I("air", 300); fireTicks = I("fire", 0);
            dead = I("dead", 0) == 1 && health <= 0;
            if (d.TryGetValue("spawn", out var sp)) { var p = sp.Split(','); if (p.Length >= 5) { spawnDim = (DimensionId)int.Parse(p[0]); spawnPos = new Int3(int.Parse(p[1]), int.Parse(p[2]), int.Parse(p[3])); spawnForced = p[4] == "1"; } }
            if (d.TryGetValue("fx", out var fx)) foreach (var s in fx.Split(',')) { var e = EffectInstance.Parse(s); if (e != null) effects[e.effect] = e; }
            if (d.TryGetValue("recipes", out var rc)) foreach (var r in rc.Split(',')) knownRecipes.Add(r);
            if (d.TryGetValue("name", out var nm)) playerName = nm;
            if (d.TryGetValue("adv", out var adv)) Achievements.Deserialize(adv);
            if (dead) { dead = false; health = maxHealth; }
        }

        public override bool ShouldSave => false;
    }

    /// <summary>Marker for hanging entities that can be punched (item frames / paintings).</summary>
    public interface ItemFrameLikeMarker { }
    public abstract class ItemFrameLike : Entity { }
}
