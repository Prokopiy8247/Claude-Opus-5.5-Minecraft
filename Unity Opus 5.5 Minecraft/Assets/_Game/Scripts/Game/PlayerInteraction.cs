using UnityEngine;

namespace MCR
{
    /// <summary>
    /// What the local player does with the mouse buttons each tick: mining with crack progress, placing blocks and
    /// using items (with the 4-tick repeat), attacking and interacting with entities, and pick-block.
    /// Input devices only feed the booleans; all rules live here so tests and automation can drive it too.
    /// </summary>
    public sealed class PlayerInteraction
    {
        public readonly Player p;
        public Int3 miningPos;
        public bool isMining;
        public float progress, prevProgress;
        public int miningFace;
        public int destroyStage = -1;
        int creativeBreakDelay;
        int useDelay;
        int hitSoundTimer;
        public bool hasBlockTarget;
        public BlockHit blockTarget;
        public Entity entityTarget;
        bool usingItemHeld;

        public PlayerInteraction(Player player) { p = player; }

        /// <summary>Recomputes the crosshair target (called every frame for the outline and once per tick for logic).</summary>
        public void UpdateTarget()
        {
            entityTarget = null;
            hasBlockTarget = false;
            if (p == null || p.world == null || p.dead) return;
            if (p.IsSpectator) return;
            hasBlockTarget = p.RaycastBlocks(false, out blockTarget);
            float reach = hasBlockTarget ? Mathf.Min(p.EntityReach, blockTarget.distance) : p.EntityReach;
            entityTarget = p.RaycastEntity(reach);
            if (entityTarget != null) hasBlockTarget = false;
        }

        public void Tick(bool attackHeld, bool attackPressed, bool useHeld, bool usePressed, bool pickPressed)
        {
            if (p == null || p.world == null || p.dead || p.menu != null) { StopMining(); return; }
            UpdateTarget();
            if (useDelay > 0) useDelay--;
            if (creativeBreakDelay > 0) creativeBreakDelay--;

            if (pickPressed) PickBlock();

            // right button: item use / block interaction / placement
            if (!useHeld && usingItemHeld)
            {
                usingItemHeld = false;
                if (p.IsUsingItem) p.StopUsing();
            }
            if (useHeld && !p.IsUsingItem && (usePressed || useDelay == 0))
            {
                if (!attackHeld || !isMining) Use();
            }

            // left button: attack entity or mine
            if (attackPressed && entityTarget != null)
            {
                p.SwingArm();
                if (entityTarget is ItemFrameLike || entityTarget.Attackable || entityTarget is EndCrystal || entityTarget is Boat || entityTarget is Minecart)
                    p.Attack(entityTarget);
                StopMining();
                return;
            }
            if (attackPressed && !hasBlockTarget && entityTarget == null)
            {
                p.SwingArm();
                p.attackStrengthTicker = 0; // swinging at air resets the attack cooldown as in the original
            }
            if (attackHeld && hasBlockTarget && !p.IsUsingItem) Mine(attackPressed);
            else StopMining();
        }

        // ------------------------------------------------------------------ mining
        void Mine(bool fresh)
        {
            var w = p.world;
            var pos = blockTarget.pos;
            var state = w.GetState(pos);
            var b = Blocks.ByState[state];
            int meta = state - b.baseState;
            if (b.isAir || b.isLiquid) { StopMining(); return; }
            if (p.gameMode == GameMode.Adventure) { StopMining(); return; }

            if (p.IsCreative)
            {
                var held = p.MainHand;
                // swords, tridents and maces cannot break blocks in creative
                if (held != null && (held.item.toolType == ToolType.Sword || held.item.id == "trident" || held.item.id == "mace" || held.item.id == "debug_stick")) return;
                if (creativeBreakDelay > 0 && !fresh) return;
                b.OnAttack(w, pos, meta, p);
                BreakNow(pos, b, meta, false);
                creativeBreakDelay = 5;
                return;
            }

            if (!isMining || miningPos != pos)
            {
                if (isMining) StopMining();
                isMining = true; miningPos = pos; progress = 0f; prevProgress = 0f; miningFace = (int)blockTarget.face;
                b.OnAttack(w, pos, meta, p);
                if (w.GetState(pos) != state) { StopMining(); return; }
            }
            float speed = BreakSpeed(b, meta, pos);
            if (speed <= 0) return;
            prevProgress = progress;
            progress += speed;
            p.SwingArm();
            if (++hitSoundTimer % 4 == 0) Sounds.PlayBlock(b.sound, SoundEvent.Hit, pos.Center);
            if (hitSoundTimer % 2 == 0) Particles.BlockHit(w, pos, blockTarget.face, state);
            destroyStage = Mathf.Clamp((int)(progress * 10f), 0, 9);
            if (progress >= 1f)
            {
                BreakNow(pos, b, meta, true);
                StopMining();
                p.blockBreakCooldown = 5;
            }
        }

        /// <summary>Fraction of a block broken per tick (1 = instant) using the original tool/effect/enchant rules.</summary>
        public float BreakSpeed(Block b, int meta, Int3 pos)
        {
            float hardness = b.GetHardness(meta);
            if (hardness < 0f) return 0f;
            if (hardness == 0f) return 1f;
            var tool = p.MainHand;
            float speed = tool != null ? tool.item.GetMiningSpeed(tool, b) : 1f;
            if (speed > 1f && tool != null)
            {
                int eff = tool.GetEnchant(Enchant.Efficiency);
                if (eff > 0) speed += eff * eff + 1;
            }
            int haste = p.GetEffectLevel(Effect.Haste);
            if (haste >= 0) speed *= 1f + 0.2f * (haste + 1);
            int fatigue = p.GetEffectLevel(Effect.MiningFatigue);
            if (fatigue >= 0) speed *= Mathf.Pow(0.3f, Mathf.Min(fatigue + 1, 4));
            if (p.eyeInWater && p.TotalArmorEnchant(Enchant.AquaAffinity) == 0) speed /= 5f;
            if (!p.onGround && !p.abilities.flying) speed /= 5f;
            bool canHarvest = !b.requiresTool || (tool != null && (tool.item.IsCorrectToolFor(b) || b.CanHarvestWith(tool)));
            float result = speed / hardness / (canHarvest ? 30f : 100f);
            return result;
        }

        void BreakNow(Int3 pos, Block b, int meta, bool survival)
        {
            var w = p.world;
            var tool = p.MainHand;
            bool canHarvest = !b.requiresTool || (tool != null && (tool.item.IsCorrectToolFor(b) || b.CanHarvestWith(tool)));
            bool drop = survival && canHarvest;
            w.BreakBlock(pos, drop, p, tool);
            if (survival)
            {
                p.hunger.AddExhaustion(0.005f);
                if (tool != null && tool.item.IsDamageable && b.GetHardness(meta) > 0f)
                {
                    bool broke = tool.HurtAndBreak(tool.item.toolType == ToolType.Sword ? 2 : 1, p);
                    if (broke || tool.count <= 0) p.inventory.Cleanup();
                }
                Achievements.OnMine(p, b.id);
            }
        }

        public void StopMining()
        {
            if (!isMining) return;
            isMining = false; progress = 0f; prevProgress = 0f; destroyStage = -1; hitSoundTimer = 0;
        }

        // ------------------------------------------------------------------ use / place
        void Use()
        {
            var w = p.world;
            useDelay = 4;
            foreach (bool offhand in new[] { false, true })
            {
                var stack = offhand ? p.inventory.offhand : p.inventory.Selected;
                if (entityTarget != null)
                {
                    if (entityTarget.Interact(p, stack)) { p.SwingArm(); p.inventory.Cleanup(); return; }
                    if (stack != null && !stack.IsEmpty)
                    {
                        var r = stack.item.InteractEntity(w, p, stack, entityTarget);
                        if (r == UseResult.Success || r == UseResult.Consume) { p.SwingArm(); p.inventory.Cleanup(); return; }
                    }
                }
                if (hasBlockTarget)
                {
                    var pos = blockTarget.pos;
                    var st = w.GetState(pos);
                    var b = Blocks.ByState[st];
                    bool bypass = p.sneaking && ((p.MainHand != null && !p.MainHand.IsEmpty) || (p.OffHand != null && !p.OffHand.IsEmpty));
                    if (!offhand && !bypass && !p.IsSpectator)
                    {
                        if (b.OnUse(w, pos, st - b.baseState, p, blockTarget.face, blockTarget.point)) { p.SwingArm(); p.inventory.Cleanup(); return; }
                    }
                    if (stack != null && !stack.IsEmpty && !p.IsSpectator)
                    {
                        if (p.OnCooldown(stack.item)) return;
                        var ctx = new UseOnContext { world = w, player = p, stack = stack, pos = pos, face = blockTarget.face, hit = blockTarget.point, sneaking = p.sneaking };
                        int before = stack.count;
                        var r = stack.item.UseOn(ref ctx);
                        if (r == UseResult.Success || r == UseResult.Consume)
                        {
                            p.SwingArm();
                            if (p.IsCreative && stack.count < before && stack.item.block != null) stack.count = before;
                            p.inventory.Cleanup();
                            return;
                        }
                        if (r == UseResult.Fail) return;
                    }
                }
                if (stack != null && !stack.IsEmpty && !p.IsSpectator)
                {
                    if (p.OnCooldown(stack.item)) return;
                    var r = stack.item.Use(w, p, stack);
                    if (r == UseResult.Success || r == UseResult.Consume)
                    {
                        if (stack.item.UseDuration(stack) > 0) { p.StartUsing(stack); usingItemHeld = true; }
                        else p.SwingArm();
                        p.inventory.Cleanup();
                        return;
                    }
                    if (r == UseResult.Fail) return;
                }
            }
        }

        // ------------------------------------------------------------------ pick block
        void PickBlock()
        {
            Item want = null;
            if (entityTarget != null)
            {
                if (entityTarget is Mob m && m.def != null) want = Items.Get(m.def.id + "_spawn_egg");
                else if (entityTarget is Boat) want = Items.Get("oak_boat");
                else if (entityTarget is Minecart) want = Items.Get("minecart");
                else if (entityTarget is EndCrystal) want = Items.Get("end_crystal");
            }
            else if (hasBlockTarget)
            {
                var st = p.world.GetState(blockTarget.pos);
                var b = Blocks.ByState[st];
                want = b.GetPickItem(st - b.baseState) ?? b.item;
            }
            if (want == null) return;
            var inv = p.inventory;
            for (int i = 0; i < 9; i++) if (inv.main[i] != null && inv.main[i].item == want) { inv.selected = i; return; }
            int found = inv.FindSlot(want);
            if (found >= 9)
            {
                // swap from the main inventory into the current hotbar slot
                var tmp = inv.main[inv.selected]; inv.main[inv.selected] = inv.main[found]; inv.main[found] = tmp;
                return;
            }
            if (!p.IsCreative) return;
            int empty = -1;
            for (int i = 0; i < 9; i++) if (inv.main[(inv.selected + i) % 9] == null) { empty = (inv.selected + i) % 9; break; }
            if (empty < 0) empty = inv.selected;
            inv.main[empty] = new ItemStack(want, 1);
            inv.selected = empty;
        }
    }
}
