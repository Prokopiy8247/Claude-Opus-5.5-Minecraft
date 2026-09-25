using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public class BowItem : Item
    {
        public BowItem() { maxStack = 1; maxDamage = 384; tab = CreativeTab.Combat; enchantability = 1; heldModel = HeldModel.Custom; modelName = "bow"; }
        public override int UseDuration(ItemStack s) => 72000;
        public override UseAnim GetUseAnim(ItemStack s) => UseAnim.Bow;
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            if (!p.IsCreative && s.GetEnchant(Enchant.Infinity) == 0 && p.FindAmmo() == null) return UseResult.Fail;
            p.StartUsing(s);
            return UseResult.Consume;
        }
        public static float PowerFor(int ticks)
        {
            float f = ticks / 20f;
            f = (f * f + f * 2f) / 3f;
            return Mathf.Min(1f, f);
        }
        public override void ReleaseUsing(World w, Player p, ItemStack s, int ticksUsed)
        {
            float power = PowerFor(ticksUsed);
            if (power < 0.1f) return;
            var ammo = p.FindAmmo();
            bool infinite = p.IsCreative || (s.GetEnchant(Enchant.Infinity) > 0 && (ammo == null || ammo.item.id == "arrow"));
            if (ammo == null && !infinite) return;
            var arrow = Arrow.Shoot(w, p, power * 3f, 1f, ammo?.item.id ?? "arrow", ammo);
            arrow.crit = power >= 1f;
            int pw = s.GetEnchant(Enchant.Power);
            if (pw > 0) arrow.damage += pw * 0.5f + 0.5f;
            arrow.punch = s.GetEnchant(Enchant.Punch);
            if (s.GetEnchant(Enchant.Flame) > 0) arrow.SetOnFire(100);
            if (infinite) arrow.pickup = p.IsCreative ? Arrow.Pickup.CreativeOnly : Arrow.Pickup.CreativeOnly;
            else { ammo.count--; }
            s.HurtAndBreak(1, p);
            Sounds.Play("entity.arrow.shoot", p.position, 1f, 1f / (Random.value * 0.4f + 1.2f) + power * 0.5f);
        }
    }

    public class CrossbowItem : Item
    {
        public CrossbowItem() { maxStack = 1; maxDamage = 465; tab = CreativeTab.Combat; enchantability = 1; heldModel = HeldModel.Custom; modelName = "crossbow"; }
        public static bool Charged(ItemStack s) => s.Get("charged") != null;
        public override int UseDuration(ItemStack s) => Charged(s) ? 0 : 25 - 5 * s.GetEnchant(Enchant.QuickCharge) + 3;
        public override UseAnim GetUseAnim(ItemStack s) => UseAnim.Crossbow;
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            if (Charged(s))
            {
                string ammo = s.Get("charged");
                int shots = s.GetEnchant(Enchant.Multishot) > 0 ? 3 : 1;
                for (int i = 0; i < shots; i++)
                {
                    float spread = (i - (shots - 1) / 2f) * 10f;
                    if (ammo == "firework_rocket")
                    {
                        Firework.Launch(w, p, p.EyePosition, MathX.YawPitchToDir(p.yaw + spread, p.pitch) * 1.6f, true);
                    }
                    else
                    {
                        var a = Arrow.Shoot(w, p, 3.15f, 1f, ammo, null, spread);
                        a.crit = true; a.piercing = s.GetEnchant(Enchant.Piercing);
                        a.pickup = i == 0 && !p.IsCreative ? Arrow.Pickup.Allowed : Arrow.Pickup.CreativeOnly;
                    }
                }
                s.Set("charged", null);
                s.HurtAndBreak(shots, p);
                Sounds.Play("item.crossbow.shoot", p.position, 1f, 1f);
                return UseResult.Success;
            }
            if (!p.IsCreative && p.FindAmmo(true) == null) return UseResult.Fail;
            p.StartUsing(s);
            return UseResult.Consume;
        }
        public override void UsingTick(World w, Player p, ItemStack s, int ticksUsed)
        {
            int dur = UseDuration(s) - 3;
            if (ticksUsed == dur / 3) Sounds.Play("item.crossbow.loading_start", p.position, 0.5f, 1f);
            if (ticksUsed == dur) Sounds.Play("item.crossbow.loading_end", p.position, 0.5f, 1f);
        }
        public override void ReleaseUsing(World w, Player p, ItemStack s, int ticksUsed)
        {
            if (ticksUsed < UseDuration(s) - 3) return;
            var ammo = p.FindAmmo(true);
            string id = ammo?.item.id ?? "arrow";
            if (ammo != null && !p.IsCreative) ammo.count--;
            s.Set("charged", id);
        }
        public override void FinishUsing(World w, Player p, ItemStack s) => ReleaseUsing(w, p, s, UseDuration(s));
    }

    public class TridentItem : Item
    {
        public TridentItem() { maxStack = 1; maxDamage = 250; attackDamage = 9; attackSpeed = 1.1f; tab = CreativeTab.Combat; enchantability = 1; heldModel = HeldModel.Custom; modelName = "trident"; }
        public override int UseDuration(ItemStack s) => 72000;
        public override UseAnim GetUseAnim(ItemStack s) => UseAnim.Trident;
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            if (s.GetEnchant(Enchant.Riptide) > 0 && !(p.inWater || w.IsRainingAt(p.BlockPos))) return UseResult.Fail;
            p.StartUsing(s); return UseResult.Consume;
        }
        public override void ReleaseUsing(World w, Player p, ItemStack s, int ticksUsed)
        {
            if (ticksUsed < 10) return;
            int riptide = s.GetEnchant(Enchant.Riptide);
            if (riptide > 0)
            {
                p.velocity += p.LookDir * (1.2f + riptide * 0.6f);
                p.riptideTicks = 20;
                Sounds.Play("item.trident.riptide_" + Mathf.Min(3, riptide), p.position, 1f, 1f);
                s.HurtAndBreak(1, p);
                return;
            }
            var t = ThrownTrident.Throw(w, p, s.Copy());
            if (!p.IsCreative) s.count = 0;
            Sounds.Play("item.trident.throw", p.position, 1f, 1f);
        }
        public override void AppendTooltip(ItemStack s, List<string> lines) { lines.Add(""); lines.Add("§7When in Main Hand:"); lines.Add("§2 9 Attack Damage"); lines.Add("§2 1.1 Attack Speed"); }
    }

    public class ShieldItem : Item
    {
        public ShieldItem() { maxStack = 1; maxDamage = 336; tab = CreativeTab.Combat; heldModel = HeldModel.Custom; modelName = "shield"; repairItem = "planks"; }
        public override int UseDuration(ItemStack s) => 72000;
        public override UseAnim GetUseAnim(ItemStack s) => UseAnim.Block;
        public override UseResult Use(World w, Player p, ItemStack s) { p.StartUsing(s); return UseResult.Consume; }
    }

    /// <summary>26.x tiered spear: jab (short cooldown, extended reach) and charge (held use: momentum-based lunge hit).</summary>
    public class SpearItem : ToolItem
    {
        public SpearItem(ToolTier t, float dmg) : base(ToolType.Spear, t, dmg, 1.4f) { tab = CreativeTab.Combat; heldModel = HeldModel.Custom; modelName = "spear"; enchantability = t.enchantability; }
        public const float Reach = 4.5f;
        public override int UseDuration(ItemStack s) => 72000;
        public override UseAnim GetUseAnim(ItemStack s) => UseAnim.Spear;
        public override UseResult Use(World w, Player p, ItemStack s) { p.StartUsing(s); return UseResult.Consume; }
        public override void UsingTick(World w, Player p, ItemStack s, int ticksUsed)
        {
            // charging: while held with speed, damage entities in front based on momentum
            if (ticksUsed < 8) return;
            Vector3 hv = p.velocity; hv.y = 0;
            float speed = hv.magnitude * 20f; // blocks per second
            if (speed < 3.5f) return;
            var target = p.RaycastEntity(Reach);
            if (target is LivingEntity le && le.invulTime <= 0)
            {
                float dmg = attackDamage * 0.5f + speed * 0.9f;
                if (le.Hurt(DamageSource.PlayerAttack(p), dmg))
                {
                    le.Knockback(0.8f, p.position.x - le.position.x, p.position.z - le.position.z);
                    Sounds.Play("item.spear.hit", le.position, 1f, 1f);
                    Particles.Crit(w, le.position + Vector3.up * le.height * 0.6f, 10);
                    s.HurtAndBreak(1, p);
                    p.StopUsing();
                }
            }
        }
        public override void ReleaseUsing(World w, Player p, ItemStack s, int ticksUsed)
        {
            int lunge = s.GetEnchant(Enchant.Lunge);
            if (lunge > 0 && ticksUsed >= 10 && p.hunger.food > 6 && !p.inWater)
            {
                Vector3 d = p.LookDir; d.y = Mathf.Clamp(d.y, -0.1f, 0.4f);
                p.velocity += d.normalized * (0.6f + lunge * 0.35f);
                p.hunger.AddExhaustion(2f * lunge);
                Sounds.Play("item.spear.lunge", p.position, 1f, 1f);
            }
        }
        public override void AppendTooltip(ItemStack s, List<string> lines)
        {
            base.AppendTooltip(s, lines);
            lines.Add("§2 " + Reach + " Reach (jab)");
            lines.Add("§7Hold use while sprinting to charge");
        }
    }

    public class MaceItem : Item
    {
        public MaceItem() { maxStack = 1; maxDamage = 500; attackDamage = 6; attackSpeed = 0.6f; tab = CreativeTab.Combat; enchantability = 15; rarity = Rarity.Epic; heldModel = HeldModel.Custom; modelName = "mace"; }
    }

    public class FishingRodItem : Item
    {
        public FishingRodItem() { maxStack = 1; maxDamage = 64; tab = CreativeTab.Tools; enchantability = 1; heldModel = HeldModel.Custom; modelName = "fishing_rod"; }
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            if (p.fishingHook != null && !p.fishingHook.removed)
            {
                int dmg = p.fishingHook.Retrieve(s);
                s.HurtAndBreak(dmg, p);
                Sounds.Play("entity.fishing_bobber.retrieve", p.position, 1f, 1f);
            }
            else
            {
                p.fishingHook = FishingHook.Cast(w, p, s);
                Sounds.Play("entity.fishing_bobber.throw", p.position, 0.5f, 0.4f);
            }
            p.SwingArm();
            return UseResult.Success;
        }
    }

    public enum PotionKind { Drink, Splash, Lingering, Arrow }

    public class PotionItem : Item
    {
        public PotionKind kind;
        public PotionItem(PotionKind k) { kind = k; maxStack = k == PotionKind.Arrow ? 64 : 1; tab = k == PotionKind.Arrow ? CreativeTab.Combat : CreativeTab.Food; }
        public override string GetName(ItemStack s)
        {
            var p = Potions.Get(s.Get("potion") ?? "water");
            string baseName = kind == PotionKind.Splash ? "Splash Potion" : kind == PotionKind.Lingering ? "Lingering Potion" : kind == PotionKind.Arrow ? "Arrow" : "Potion";
            if (p == null) return baseName;
            if (p.id == "water") return kind == PotionKind.Drink ? "Water Bottle" : baseName + " of Water";
            if (p.id == "awkward") return "Awkward " + baseName;
            if (p.id == "mundane") return "Mundane " + baseName;
            if (p.id == "thick") return "Thick " + baseName;
            return (kind == PotionKind.Arrow ? "Arrow of " : baseName + " of ") + p.name;
        }
        public override int UseDuration(ItemStack s) => kind == PotionKind.Drink ? 32 : 0;
        public override UseAnim GetUseAnim(ItemStack s) => kind == PotionKind.Drink ? UseAnim.Drink : UseAnim.None;
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            if (kind == PotionKind.Arrow) return UseResult.Pass;
            if (kind == PotionKind.Drink) { p.StartUsing(s); return UseResult.Consume; }
            ThrownPotion.Throw(w, p, s.CopyWithCount(1), kind == PotionKind.Lingering);
            Sounds.Play("entity.splash_potion.throw", p.position, 0.5f, 0.4f);
            if (!p.IsCreative) s.count--;
            return UseResult.Success;
        }
        public override void FinishUsing(World w, Player p, ItemStack s)
        {
            var pot = Potions.Get(s.Get("potion") ?? "water");
            if (pot != null) foreach (var e in pot.effects) p.AddEffect(e.Copy());
            if (pot != null && pot.id == "water" && p.onFire) p.Extinguish();
            Sounds.Play("entity.generic.drink", p.position, 0.5f, 1f);
            if (!p.IsCreative) { s.count--; p.inventory.AddOrDrop(new ItemStack("glass_bottle", 1)); }
        }
        public override void AppendTooltip(ItemStack s, List<string> lines)
        {
            var pot = Potions.Get(s.Get("potion") ?? "water");
            if (pot == null || pot.effects.Count == 0) { lines.Add("§7No Effects"); return; }
            foreach (var e in pot.effects)
            {
                int dur = kind == PotionKind.Lingering ? e.duration / 4 : kind == PotionKind.Arrow ? e.duration / 8 : e.duration;
                string d = e.effect == Effect.InstantHealth || e.effect == Effect.InstantDamage ? "" : " (" + EffectInstance.FormatTime(dur) + ")";
                lines.Add((e.effect.beneficial ? "§9" : "§c") + e.effect.name + (e.amplifier > 0 ? " " + Enchant.Roman(e.amplifier + 1) : "") + d);
            }
        }
        public static Color32 ColorOf(ItemStack s)
        {
            var pot = Potions.Get(s?.Get("potion") ?? "water");
            return pot != null ? pot.color : new Color32(56, 93, 198, 255);
        }
    }

    public class ElytraItem : Item
    {
        public ElytraItem() { maxStack = 1; maxDamage = 432; tab = CreativeTab.Tools; rarity = Rarity.Epic; repairItem = "phantom_membrane"; }
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            var cur = p.inventory.armor[2];
            p.inventory.armor[2] = s.Copy();
            s.count = 0;
            if (cur != null) p.inventory.SetSelected(cur);
            Sounds.Play("item.armor.equip_elytra", p.position, 1f, 1f);
            return UseResult.Success;
        }
    }

    public class FireworkItem : Item
    {
        public FireworkItem() { tab = CreativeTab.Tools; }
        public override UseResult UseOn(ref UseOnContext ctx)
        {
            Firework.Launch(ctx.world, ctx.player, ctx.hit + DirUtil.Normal[(int)ctx.face] * 0.1f, new Vector3(0, 0.5f, 0), false, ctx.stack.GetInt("flight", 1));
            if (!ctx.player.IsCreative) ctx.stack.count--;
            return UseResult.Success;
        }
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            if (!p.IsGliding) return UseResult.Pass;
            Firework.Boost(w, p, s.GetInt("flight", 1));
            if (!p.IsCreative) s.count--;
            return UseResult.Success;
        }
    }

    public class MinecartItem : Item
    {
        public string content;
        public MinecartItem(string c) { content = c; maxStack = 1; tab = CreativeTab.Tools; }
        public override UseResult UseOn(ref UseOnContext ctx)
        {
            if (!(ctx.world.GetBlock(ctx.pos) is RailBlock)) return UseResult.Fail;
            var m = Minecart.Spawn(ctx.world, ctx.pos.ToVector3() + new Vector3(0.5f, 0.0625f, 0.5f), content);
            if (!ctx.player.IsCreative) ctx.stack.count--;
            return UseResult.Success;
        }
    }

    public class BoatItem : Item
    {
        public string wood; public bool chest;
        public BoatItem(string wood, bool chest) { this.wood = wood; this.chest = chest; maxStack = 1; tab = CreativeTab.Tools; fuelTicks = 1200; }
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            if (!p.RaycastBlocks(true, out var hit)) return UseResult.Pass;
            Vector3 at = hit.point;
            if (!w.IsWater(hit.pos)) at = hit.pos.Offset(hit.face).ToVector3() + new Vector3(0.5f, 0, 0.5f);
            else at = new Vector3(hit.point.x, hit.pos.y + 0.9f, hit.point.z);
            var b = Boat.Spawn(w, at, wood, chest, p.yaw);
            if (!p.IsCreative) s.count--;
            p.SwingArm();
            return UseResult.Success;
        }
    }

    public class EndCrystalItem : Item
    {
        public EndCrystalItem() { tab = CreativeTab.Combat; glint = true; }
        public override UseResult UseOn(ref UseOnContext ctx)
        {
            var b = ctx.world.GetBlock(ctx.pos);
            if (b.id != "obsidian" && b.id != "bedrock") return UseResult.Fail;
            Int3 up = ctx.pos.Offset(Dir.Up);
            if (!ctx.world.IsAir(up)) return UseResult.Fail;
            EndCrystal.Spawn(ctx.world, up.ToVector3() + new Vector3(0.5f, 0, 0.5f), false);
            if (!ctx.player.IsCreative) ctx.stack.count--;
            if (ctx.world.dim == DimensionId.End) DragonFight.Instance?.OnCrystalPlaced(ctx.world, up);
            return UseResult.Success;
        }
    }

    public class EnderEyeItem : Item
    {
        public EnderEyeItem() { tab = CreativeTab.Tools; description = "Throw to locate a Stronghold"; }
        public override UseResult Use(World w, Player p, ItemStack s)
        {
            if (w.dim != DimensionId.Overworld) return UseResult.Fail;
            var st = StructureManager.Locate(w.generator, "stronghold", Mathf.FloorToInt(p.position.x), Mathf.FloorToInt(p.position.z), 30);
            if (st == null) return UseResult.Fail;
            EyeOfEnder.Throw(w, p, new Vector3(st.x + 0.5f, st.y, st.z + 0.5f));
            Sounds.Play("entity.ender_eye.launch", p.position, 0.5f, 0.4f);
            if (!p.IsCreative) s.count--;
            p.SwingArm();
            return UseResult.Success;
        }
    }
}
