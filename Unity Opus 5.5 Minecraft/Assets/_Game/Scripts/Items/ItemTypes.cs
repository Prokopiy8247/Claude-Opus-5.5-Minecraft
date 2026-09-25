using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public class BlockItem : Item
    {
        public BlockItem() { heldModel = HeldModel.Block; }

        public override UseResult UseOn(ref UseOnContext ctx)
        {
            if (block == null) return UseResult.Pass;
            return Place(ref ctx, block);
        }

        public static UseResult Place(ref UseOnContext ctx, Block block)
        {
            var w = ctx.world;
            var p = ctx.player;
            if (p != null && !p.CanBuild) return UseResult.Fail;
            Int3 clicked = ctx.pos;
            ushort clickedState = w.GetState(clicked);
            Block clickedBlock = Blocks.ByState[clickedState];
            var pc = new PlaceContext
            {
                world = w, clickedPos = clicked, clickedFace = ctx.face,
                hitLocal = ctx.hit - clicked.ToVector3(),
                playerYaw = p != null ? p.yaw : 0, playerPitch = p != null ? p.pitch : 0,
                playerFacing = p != null ? p.HorizontalFacing : Dir.North,
                sneaking = ctx.sneaking, placer = p, stack = ctx.stack
            };
            // replace the clicked block itself (grass, snow layer, slab merge) or place adjacent
            pc.pos = clicked;
            if (!clickedBlock.CanBeReplaced(clickedState - clickedBlock.baseState, ref pc))
            {
                pc.pos = clicked.Offset(ctx.face);
                ushort targetS = w.GetState(pc.pos);
                var tb = Blocks.ByState[targetS];
                if (!tb.CanBeReplaced(targetS - tb.baseState, ref pc)) return UseResult.Fail;
            }
            if (pc.pos.y < w.minY || pc.pos.y >= w.maxY || !w.IsLoaded(pc.pos)) return UseResult.Fail;
            ushort state = block.GetPlacementState(ref pc);
            if (state == 0 && block != Blocks.Air) return UseResult.Fail;
            if (!block.CanPlaceAt(w, pc.pos, state)) return UseResult.Fail;
            // entity collision check
            var boxes = new List<AABB>();
            Blocks.ByState[state].GetCollisionBoxes(state - block.baseState, w, pc.pos, boxes);
            if (Blocks.ByState[state].solid)
            {
                foreach (var b in boxes)
                {
                    var wb = b.Offset(pc.pos.ToVector3());
                    foreach (var e in w.GetEntities(wb))
                        if (e.blocksBuilding && !(e is Player pl && pl.IsSpectator)) return UseResult.Fail;
                }
            }
            w.SetState(pc.pos, state);
            block.OnPlaced(w, pc.pos, state - block.baseState, p, ctx.stack);
            Sounds.PlayBlock(block.sound, SoundEvent.Place, pc.pos.Center);
            if (p != null) p.SwingArm();
            if (p == null || !p.IsCreative) ctx.stack.count--;
            return UseResult.Success;
        }
    }

    /// <summary>Doors and other two-block-tall items (placement handled by the block).</summary>
    public class TallBlockItem : BlockItem { }

    // ============================================================================ Tools
    public class ToolTier
    {
        public string name; public int level; public int durability; public float speed; public float damageBonus; public int enchantability; public string repair;
        public static readonly ToolTier Wood = new ToolTier { name = "wooden", level = Tier.Wood, durability = 59, speed = 2, damageBonus = 0, enchantability = 15, repair = "planks" };
        public static readonly ToolTier Stone = new ToolTier { name = "stone", level = Tier.Stone, durability = 131, speed = 4, damageBonus = 1, enchantability = 5, repair = "cobblestone" };
        public static readonly ToolTier Copper = new ToolTier { name = "copper", level = Tier.Copper, durability = 190, speed = 5, damageBonus = 1, enchantability = 13, repair = "copper_ingot" };
        public static readonly ToolTier Iron = new ToolTier { name = "iron", level = Tier.Iron, durability = 250, speed = 6, damageBonus = 2, enchantability = 14, repair = "iron_ingot" };
        public static readonly ToolTier Gold = new ToolTier { name = "golden", level = Tier.Gold, durability = 32, speed = 12, damageBonus = 0, enchantability = 22, repair = "gold_ingot" };
        public static readonly ToolTier Diamond = new ToolTier { name = "diamond", level = Tier.Diamond, durability = 1561, speed = 8, damageBonus = 3, enchantability = 10, repair = "diamond" };
        public static readonly ToolTier Netherite = new ToolTier { name = "netherite", level = Tier.Netherite, durability = 2031, speed = 9, damageBonus = 4, enchantability = 15, repair = "netherite_ingot" };
        public static readonly ToolTier[] All = { Wood, Stone, Copper, Iron, Gold, Diamond, Netherite };
    }

    public class ToolItem : Item
    {
        public ToolTier tierData;
        public ToolItem(ToolType type, ToolTier t, float damage, float speed)
        {
            toolType = type; tierData = t; tier = t.level; maxDamage = t.durability; miningSpeed = t.speed; attackDamage = damage; attackSpeed = speed;
            maxStack = 1; enchantability = t.enchantability; repairItem = t.repair; tab = CreativeTab.Tools; heldModel = HeldModel.Tool;
            if (t == ToolTier.Netherite) fireResistant = true;
            if (type == ToolType.Sword) tab = CreativeTab.Combat;
            fuelTicks = t == ToolTier.Wood ? 200 : 0;
        }

        public override void AppendTooltip(ItemStack s, List<string> lines)
        {
            lines.Add("");
            lines.Add("§7When in Main Hand:");
            lines.Add("§2 " + FormatNum(attackDamage) + " Attack Damage");
            lines.Add("§2 " + FormatNum(attackSpeed) + " Attack Speed");
        }
        static string FormatNum(float f) => Mathf.Approximately(f, Mathf.Round(f)) ? ((int)Mathf.Round(f)).ToString() : f.ToString("0.#");

        public override UseResult UseOn(ref UseOnContext ctx)
        {
            var w = ctx.world;
            ushort s = w.GetState(ctx.pos);
            var b = Blocks.ByState[s];
            int meta = s - b.baseState;
            if (toolType == ToolType.Axe)
            {
                if (b is PillarBlock pb && pb.strippedId != null)
                {
                    w.SetState(ctx.pos, Blocks.Get(pb.strippedId).State(meta));
                    Sounds.Play("item.axe.strip", ctx.pos.Center, 1f, 1f);
                    Damage(ctx);
                    return UseResult.Success;
                }
                if (b is CopperBlock || b.id.Contains("copper_"))
                {
                    if (b.id.StartsWith("waxed_"))
                    {
                        var unwaxed = Blocks.Get(b.id.Substring(6));
                        if (unwaxed != null) { w.SetState(ctx.pos, unwaxed.State(Mathf.Min(meta, unwaxed.stateCount - 1))); Particles.WaxOff(w, ctx.pos); Sounds.Play("item.axe.wax_off", ctx.pos.Center, 1f, 1f); Damage(ctx); return UseResult.Success; }
                    }
                    string prev = Oxidation.Prev(b.id);
                    var pbk = prev != null ? Blocks.Get(prev) : null;
                    if (pbk != null) { w.SetState(ctx.pos, pbk.State(Mathf.Min(meta, pbk.stateCount - 1))); Particles.Scrape(w, ctx.pos); Sounds.Play("item.axe.scrape", ctx.pos.Center, 1f, 1f); Damage(ctx); return UseResult.Success; }
                }
            }
            else if (toolType == ToolType.Shovel)
            {
                if (ctx.face != Dir.Down && w.IsAir(ctx.pos.Offset(Dir.Up)) && (b.id == "grass_block" || b.id == "dirt" || b.id == "coarse_dirt" || b.id == "podzol" || b.id == "mycelium" || b.id == "rooted_dirt"))
                {
                    w.SetState(ctx.pos, Blocks.Get("dirt_path").DefaultState);
                    Sounds.Play("item.shovel.flatten", ctx.pos.Center, 1f, 1f);
                    Damage(ctx);
                    return UseResult.Success;
                }
                if (b is CampfireBlock && CampfireBlock.Lit(meta)) { w.SetState(ctx.pos, b.State(meta & ~4)); Sounds.Play("entity.generic.extinguish_fire", ctx.pos.Center, 1f, 1f); Damage(ctx); return UseResult.Success; }
            }
            else if (toolType == ToolType.Hoe)
            {
                if (ctx.face != Dir.Down && w.IsAir(ctx.pos.Offset(Dir.Up)))
                {
                    string to = null;
                    if (b.id == "grass_block" || b.id == "dirt" || b.id == "dirt_path") to = "farmland";
                    else if (b.id == "coarse_dirt") to = "dirt";
                    else if (b.id == "rooted_dirt") { to = "dirt"; w.SpawnItem(ctx.pos.Offset(ctx.face).Center, new ItemStack("hanging_roots", 1)); }
                    if (to != null)
                    {
                        w.SetState(ctx.pos, Blocks.Get(to).DefaultState);
                        Sounds.Play("item.hoe.till", ctx.pos.Center, 1f, 1f);
                        Damage(ctx);
                        return UseResult.Success;
                    }
                }
            }
            else if (toolType == ToolType.Shears)
            {
                if (b.id == "pumpkin" && ctx.face != Dir.Up && ctx.face != Dir.Down)
                {
                    var carved = Blocks.Get("carved_pumpkin");
                    w.SetState(ctx.pos, carved.State(DirUtil.HorizIndex(ctx.face)));
                    w.SpawnItem(ctx.pos.Offset(ctx.face).Center, new ItemStack("pumpkin_seeds", 4));
                    Sounds.Play("block.pumpkin.carve", ctx.pos.Center, 1f, 1f);
                    Damage(ctx);
                    return UseResult.Success;
                }
            }
            return UseResult.Pass;
        }

        void Damage(UseOnContext ctx) { ctx.stack.HurtAndBreak(1, ctx.player); ctx.player?.SwingArm(); }

        public override void OnHitEntity(ItemStack s, LivingEntity target, LivingEntity attacker)
        {
            s.HurtAndBreak(toolType == ToolType.Sword || toolType == ToolType.Spear ? 1 : 2, attacker);
        }
    }

    // ============================================================================ Armor
    public class ArmorMaterial
    {
        public string name; public int[] defense; public float toughness; public int durMul; public int enchantability; public float knockbackRes; public string repair;
        public Color32 color;
        public static readonly ArmorMaterial Leather = new ArmorMaterial { name = "leather", defense = new[] { 1, 2, 3, 1 }, durMul = 5, enchantability = 15, repair = "leather", color = new Color32(160, 101, 64, 255) };
        public static readonly ArmorMaterial Copper = new ArmorMaterial { name = "copper", defense = new[] { 1, 3, 4, 2 }, durMul = 11, enchantability = 8, repair = "copper_ingot", color = new Color32(196, 110, 75, 255) };
        public static readonly ArmorMaterial Chainmail = new ArmorMaterial { name = "chainmail", defense = new[] { 1, 4, 5, 2 }, durMul = 15, enchantability = 12, repair = "iron_ingot", color = new Color32(150, 150, 155, 255) };
        public static readonly ArmorMaterial Iron = new ArmorMaterial { name = "iron", defense = new[] { 2, 5, 6, 2 }, durMul = 15, enchantability = 9, repair = "iron_ingot", color = new Color32(210, 210, 210, 255) };
        public static readonly ArmorMaterial Gold = new ArmorMaterial { name = "golden", defense = new[] { 1, 3, 5, 2 }, durMul = 7, enchantability = 25, repair = "gold_ingot", color = new Color32(245, 210, 70, 255) };
        public static readonly ArmorMaterial Diamond = new ArmorMaterial { name = "diamond", defense = new[] { 3, 6, 8, 3 }, toughness = 2, durMul = 33, enchantability = 10, repair = "diamond", color = new Color32(90, 225, 215, 255) };
        public static readonly ArmorMaterial Netherite = new ArmorMaterial { name = "netherite", defense = new[] { 3, 6, 8, 3 }, toughness = 3, durMul = 37, enchantability = 15, knockbackRes = 0.1f, repair = "netherite_ingot", color = new Color32(75, 68, 72, 255) };
        public static readonly ArmorMaterial Turtle = new ArmorMaterial { name = "turtle", defense = new[] { 2, 5, 6, 2 }, durMul = 25, enchantability = 9, repair = "turtle_scute", color = new Color32(70, 160, 70, 255) };
        public static readonly ArmorMaterial[] All = { Leather, Copper, Chainmail, Iron, Gold, Diamond, Netherite };
        public static readonly int[] BaseDurability = { 13, 15, 16, 11 }; // feet, legs, chest, head
    }

    public class ArmorItem : Item
    {
        public ArmorSlot slot; public ArmorMaterial material; public int defense; public float toughness;
        public ArmorItem(ArmorMaterial m, ArmorSlot s)
        {
            material = m; slot = s; defense = m.defense[(int)s]; toughness = m.toughness;
            maxDamage = ArmorMaterial.BaseDurability[(int)s] * m.durMul; maxStack = 1; enchantability = m.enchantability; repairItem = m.repair;
            tab = CreativeTab.Combat; if (m == ArmorMaterial.Netherite) fireResistant = true;
        }
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            int idx = (int)slot;
            var cur = p.inventory.armor[idx];
            p.inventory.armor[idx] = s.Copy();
            if (!p.IsCreative || true)
            {
                if (cur != null && !cur.IsEmpty) { s.count = 0; p.inventory.SetSelected(cur); }
                else s.count = 0;
            }
            Sounds.Play("item.armor.equip_" + material.name, p.position, 1f, 1f);
            return UseResult.Success;
        }
        public override void AppendTooltip(ItemStack s, List<string> lines)
        {
            lines.Add("");
            lines.Add("§7When on " + (slot == ArmorSlot.Head ? "Head" : slot == ArmorSlot.Chest ? "Body" : slot == ArmorSlot.Legs ? "Legs" : "Feet") + ":");
            lines.Add("§9+" + defense + " Armor");
            if (toughness > 0) lines.Add("§9+" + toughness + " Armor Toughness");
        }
    }

    // ============================================================================ Food
    public class FoodItem : Item
    {
        public int nutrition; public float saturationMod; public bool alwaysEdible; public bool fast; public bool meat;
        public List<(EffectInstance effect, float chance)> effects = new List<(EffectInstance, float)>();
        public string remainder;
        public FoodItem(int nutrition, float satMod) { this.nutrition = nutrition; saturationMod = satMod; tab = CreativeTab.Food; }
        public override int UseDuration(ItemStack s) => fast ? 16 : 32;
        public override UseAnim GetUseAnim(ItemStack s) => UseAnim.Eat;
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            if (p.CanEat(alwaysEdible)) { p.StartUsing(s); return UseResult.Consume; }
            return UseResult.Fail;
        }
        public override void UsingTick(World w, Player p, ItemStack s, int ticksUsed)
        {
            if (ticksUsed > 5 && ticksUsed % 4 == 0)
            {
                Sounds.Play("entity.generic.eat", p.position, 0.5f + 0.5f * Random.value, (Random.value - Random.value) * 0.2f + 1f);
                Particles.ItemCrumbs(w, p.EyePosition + p.LookDir * 0.4f - Vector3.up * 0.15f, s);
            }
        }
        public override void FinishUsing(World w, Player p, ItemStack s)
        {
            p.hunger.Eat(nutrition, saturationMod);
            foreach (var (e, chance) in effects) if (Random.value < chance) p.AddEffect(e.Copy());
            Sounds.Play("entity.player.burp", p.position, 0.5f, 0.9f + Random.value * 0.1f);
            OnEaten(w, p);
            if (!p.IsCreative)
            {
                s.count--;
                if (remainder != null) p.inventory.AddOrDrop(new ItemStack(remainder, 1));
            }
        }
        protected virtual void OnEaten(World w, Player p)
        {
            if (id == "chorus_fruit") p.ChorusTeleport();
            if (id == "milk_bucket") p.ClearEffects();
        }
        public override void AppendTooltip(ItemStack s, List<string> lines)
        {
            base.AppendTooltip(s, lines);
            foreach (var (e, chance) in effects)
                lines.Add((e.effect.beneficial ? "§9" : "§c") + e.effect.name + (e.amplifier > 0 ? " " + Enchant.Roman(e.amplifier + 1) : "") + " (" + EffectInstance.FormatTime(e.duration) + ")");
        }
    }

    // ============================================================================ Buckets
    public class BucketItem : Item
    {
        public string content; // null=empty, water, lava, powder_snow, milk
        public BucketItem(string content) { this.content = content; maxStack = content == null ? 16 : 1; tab = CreativeTab.Tools; craftRemainder = content != null ? "bucket" : null; }

        public override UseResult Use(World w, Player p, ItemStack s)
        {
            if (content == "milk") { p.StartUsing(s); return UseResult.Consume; }
            // raycast including fluids
            if (!p.RaycastBlocks(true, out var hit)) return UseResult.Pass;
            if (content == null)
            {
                ushort st = w.GetState(hit.pos);
                var b = Blocks.ByState[st];
                string filled = null;
                if (b is FluidBlock fb && st == b.baseState) filled = fb.fluidKind == 0 ? "water_bucket" : "lava_bucket";
                else if (b.id == "powder_snow") filled = "powder_snow_bucket";
                if (filled == null) return UseResult.Pass;
                if (!p.CanBuild) return UseResult.Fail;
                w.SetState(hit.pos, 0);
                Sounds.Play(filled == "lava_bucket" ? "item.bucket.fill_lava" : "item.bucket.fill", hit.pos.Center, 1f, 1f);
                if (!p.IsCreative) { s.count--; p.inventory.AddOrDrop(new ItemStack(filled, 1)); }
                else if (!p.inventory.Contains(filled)) p.inventory.AddOrDrop(new ItemStack(filled, 1));
                p.SwingArm();
                return UseResult.Success;
            }
            // place fluid
            Int3 target = hit.pos;
            var tb = w.GetBlock(target);
            if (!tb.replaceable || (tb.isLiquid && w.GetMeta(target) == 0 && content != null && tb.isLiquid))
                target = hit.pos.Offset(hit.face);
            var tgt = w.GetBlock(target);
            if (!tgt.replaceable && !tgt.isAir) return UseResult.Fail;
            if (!p.CanBuild) return UseResult.Fail;
            if (content == "water" && w.dim == DimensionId.Nether)
            {
                Sounds.Play("block.fire.extinguish", target.Center, 0.5f, 2.6f);
                Particles.Smoke(w, target.Center, 8, 1f, true);
            }
            else
            {
                Block place = content == "water" ? Blocks.Water : content == "lava" ? Blocks.Lava : Blocks.Get("powder_snow");
                if (place == null) return UseResult.Fail;
                if (!tgt.isAir && !tgt.isLiquid) w.DropBlockLoot(target, tgt, w.GetMeta(target), null, p);
                w.SetState(target, place.DefaultState);
                Sounds.Play(content == "lava" ? "item.bucket.empty_lava" : "item.bucket.empty", target.Center, 1f, 1f);
            }
            if (!p.IsCreative) { s.count--; p.inventory.AddOrDrop(new ItemStack("bucket", 1)); }
            p.SwingArm();
            return UseResult.Success;
        }
        public override UseResult UseOn(ref UseOnContext ctx) => Use(ctx.world, ctx.player, ctx.stack);
        public override int UseDuration(ItemStack s) => content == "milk" ? 32 : 0;
        public override UseAnim GetUseAnim(ItemStack s) => content == "milk" ? UseAnim.Drink : UseAnim.None;
        public override void FinishUsing(World w, Player p, ItemStack s)
        {
            p.ClearEffects();
            Sounds.Play("entity.generic.drink", p.position, 0.5f, 1f);
            if (!p.IsCreative) { s.count--; p.inventory.AddOrDrop(new ItemStack("bucket", 1)); }
        }
        public override UseResult InteractEntity(World w, Player p, ItemStack s, Entity target)
        {
            if (content == null && target is Mob m && (m.def.id == "cow" || m.def.id == "mooshroom" || m.def.id == "goat") && !m.IsBaby)
            {
                Sounds.Play("entity.cow.milk", target.position, 1f, 1f);
                if (!p.IsCreative) s.count--;
                p.inventory.AddOrDrop(new ItemStack("milk_bucket", 1));
                return UseResult.Success;
            }
            if (content == "water" && target is Mob fish && (fish.def.id == "cod" || fish.def.id == "salmon" || fish.def.id == "pufferfish" || fish.def.id == "tropical_fish" || fish.def.id == "axolotl" || fish.def.id == "tadpole"))
            {
                fish.Remove();
                Sounds.Play("item.bucket.fill_fish", target.position, 1f, 1f);
                if (!p.IsCreative) s.count--;
                p.inventory.AddOrDrop(new ItemStack(fish.def.id + "_bucket", 1));
                return UseResult.Success;
            }
            return UseResult.Pass;
        }
    }

    /// <summary>Bucket of a mob (fish, axolotl, tadpole): places water and releases the mob.</summary>
    public class MobBucketItem : Item
    {
        public string mobId;
        public MobBucketItem(string mob) { mobId = mob; maxStack = 1; tab = CreativeTab.Tools; }
        public override UseResult UseOn(ref UseOnContext ctx)
        {
            Int3 t = ctx.world.GetBlock(ctx.pos).replaceable ? ctx.pos : ctx.pos.Offset(ctx.face);
            if (!ctx.world.GetBlock(t).replaceable) return UseResult.Fail;
            if (ctx.world.dim != DimensionId.Nether) ctx.world.SetState(t, Blocks.Water.DefaultState);
            MobRegistry.Spawn(ctx.world, mobId, t.Center, SpawnReason.Bucket);
            Sounds.Play("item.bucket.empty_fish", t.Center, 1f, 1f);
            if (!ctx.player.IsCreative) { ctx.stack.count--; ctx.player.inventory.AddOrDrop(new ItemStack("bucket", 1)); }
            return UseResult.Success;
        }
    }

    // ============================================================================ Fire starters
    public class FlintAndSteelItem : Item
    {
        public bool charge;
        public FlintAndSteelItem(bool fireCharge) { charge = fireCharge; maxStack = fireCharge ? 64 : 1; maxDamage = fireCharge ? 0 : 64; tab = CreativeTab.Tools; }
        public override UseResult UseOn(ref UseOnContext ctx)
        {
            var w = ctx.world;
            ushort s = w.GetState(ctx.pos);
            var b = Blocks.ByState[s];
            int meta = s - b.baseState;
            if (b is TntBlock tnt) { tnt.Ignite(w, ctx.pos, ctx.player); Consume(ctx); return UseResult.Success; }
            if (b is CampfireBlock && !CampfireBlock.Lit(meta)) { w.SetState(ctx.pos, b.State(meta | 4)); Consume(ctx); Sounds.Play("item.flintandsteel.use", ctx.pos.Center, 1f, 1f); return UseResult.Success; }
            if (b.id == "candle" || b.id.EndsWith("_candle")) { }
            Int3 t = ctx.pos.Offset(ctx.face);
            if (!w.GetBlock(t).isAir) return UseResult.Fail;
            var below = w.GetBlock(t.Offset(Dir.Down));
            Block fire = below.id == "soul_sand" || below.id == "soul_soil" ? Blocks.Get("soul_fire") : Blocks.Fire;
            // portal or fire
            if (Portals.TryLightPortal(w, t) || true)
            {
                if (!(w.GetBlock(t) is PortalBlock))
                {
                    if (fire.CanSurvive(w, t, 0) || Portals.IsObsidianFrameCorner(w, t)) w.SetState(t, fire.DefaultState);
                    else return UseResult.Fail;
                }
            }
            Sounds.Play(charge ? "item.firecharge.use" : "item.flintandsteel.use", t.Center, 1f, 0.9f + Random.value * 0.2f);
            Consume(ctx);
            return UseResult.Success;
        }
        void Consume(UseOnContext ctx)
        {
            ctx.player?.SwingArm();
            if (ctx.player != null && ctx.player.IsCreative) return;
            if (charge) ctx.stack.count--; else ctx.stack.HurtAndBreak(1, ctx.player);
        }
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            if (!charge) return UseResult.Pass;
            return UseResult.Pass;
        }
    }

    // ============================================================================ Bone meal
    public class BoneMealItem : Item
    {
        public BoneMealItem() { tab = CreativeTab.Ingredients; }
        public override UseResult UseOn(ref UseOnContext ctx)
        {
            if (Apply(ctx.world, ctx.pos, ctx.face))
            {
                if (!ctx.player.IsCreative) ctx.stack.count--;
                ctx.player.SwingArm();
                return UseResult.Success;
            }
            return UseResult.Pass;
        }
        public static bool Apply(World w, Int3 pos, Dir face)
        {
            ushort s = w.GetState(pos);
            var b = Blocks.ByState[s];
            int m = s - b.baseState;
            bool ok = false;
            if (b is CropBlock c && !c.IsMature(m)) { c.Grow(w, pos, m, 2 + w.rand.Next(4)); ok = true; }
            else if (b is SaplingBlock sap) { if (w.rand.Chance(0.45f)) sap.Advance(w, pos, 1, ref w.rand); ok = true; }
            else if (b is StemBlock st && m < 7) { w.SetState(pos, b.State(Mathf.Min(7, m + 2 + w.rand.Next(3)))); ok = true; }
            else if (b is SweetBerryBushBlock sb && m < 3) { w.SetState(pos, b.State(m + 1)); ok = true; }
            else if (b.id == "grass_block" && face == Dir.Up)
            {
                for (int i = 0; i < 48; i++)
                {
                    Int3 p = new Int3(pos.x + w.rand.Range(-3, 3), pos.y + 1, pos.z + w.rand.Range(-3, 3));
                    for (int k = 0; k < 3 && !w.IsAir(p); k++) p = p.Offset(Dir.Up);
                    if (!w.IsAir(p) || w.GetBlock(p.Offset(Dir.Down)).id != "grass_block") continue;
                    float r = w.rand.NextFloat();
                    var biome = w.GetBiome(p);
                    string place = r < 0.8f ? "short_grass" : (r < 0.9f ? (biome.flowers.Length > 0 ? w.rand.Pick(biome.flowers) : "dandelion") : "tall_grass");
                    var pb = Blocks.Get(place);
                    if (pb is TallPlantBlock) { if (w.IsAir(p.Offset(Dir.Up))) { w.SetBlock(p, pb, 0); w.SetBlock(p.Offset(Dir.Up), pb, 1); } }
                    else w.SetBlock(p, pb);
                }
                ok = true;
            }
            else if (b.id == "short_grass" && w.IsAir(pos.Offset(Dir.Up))) { var tg = Blocks.Get("tall_grass"); w.SetBlock(pos, tg, 0); w.SetBlock(pos.Offset(Dir.Up), tg, 1); ok = true; }
            else if (b.id == "fern" && w.IsAir(pos.Offset(Dir.Up))) { var tg = Blocks.Get("large_fern"); w.SetBlock(pos, tg, 0); w.SetBlock(pos.Offset(Dir.Up), tg, 1); ok = true; }
            else if ((b.id == "moss_block") && face == Dir.Up)
            {
                for (int i = 0; i < 30; i++)
                {
                    Int3 p = new Int3(pos.x + w.rand.Range(-3, 3), pos.y, pos.z + w.rand.Range(-3, 3));
                    var pbk = w.GetBlock(p);
                    if (pbk.id == "stone" || pbk.id == "dirt" || pbk.id == "grass_block" || pbk.id == "deepslate" || pbk.id == "tuff") w.SetBlock(p, Blocks.Moss);
                }
                ok = true;
            }
            else if (b is PlantBlock pl && (pl.id.EndsWith("_mushroom")))
            {
                ok = TreeFeatures.GrowHugeMushroom(w, pos, pl.id == "red_mushroom", ref w.rand) || true;
            }
            else if (b.id == "crimson_fungus" || b.id == "warped_fungus")
            {
                var below = w.GetBlock(pos.Offset(Dir.Down));
                if (below.id.EndsWith("nylium")) { TreeFeatures.GrowSapling(w, pos, b.id == "crimson_fungus" ? "crimson" : "warped", ref w.rand); ok = true; }
            }
            if (ok) Particles.HappyVillager(w, pos.Center, 12);
            return ok;
        }
    }

    public class SimpleThrowable : Item
    {
        public string entity;
        public SimpleThrowable(string ent, int stack) { entity = ent; maxStack = stack; tab = CreativeTab.Combat; }
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            Projectile.ThrowFrom(w, p, entity, s);
            Sounds.Play("entity.snowball.throw", p.position, 0.5f, 0.4f / (Random.value * 0.4f + 0.8f));
            if (!p.IsCreative) s.count--;
            p.SwingArm();
            p.SetCooldown(this, entity == "ender_pearl" ? 20 : 4);
            return UseResult.Success;
        }
    }
}
