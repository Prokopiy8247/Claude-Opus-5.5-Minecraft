using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Ray-based explosion (vanilla-like): 16x16x16 rays attenuated by blast resistance, entity damage by exposure.</summary>
    public static class Explosion
    {
        static readonly HashSet<Int3> toBlow = new HashSet<Int3>();
        static readonly List<Int3> ordered = new List<Int3>();

        public static void Explode(World w, Entity source, Vector3 center, float power, bool fire, bool breakBlocks)
        {
            if (w == null) return;
            bool griefing = w.session == null || w.session.mobGriefing || !(source is Mob);
            bool inWater = w.IsWater(Int3.Floor(center));
            toBlow.Clear(); ordered.Clear();
            var rng = new RNG((int)(center.x * 31 + center.y * 17 + center.z * 13) ^ (int)w.tickCount);
            if (breakBlocks && griefing && !inWater)
            {
                for (int i = 0; i < 16; i++)
                    for (int j = 0; j < 16; j++)
                        for (int k = 0; k < 16; k++)
                        {
                            if (i != 0 && i != 15 && j != 0 && j != 15 && k != 0 && k != 15) continue;
                            Vector3 d = new Vector3(i / 15f * 2f - 1f, j / 15f * 2f - 1f, k / 15f * 2f - 1f).normalized;
                            float intensity = power * (0.7f + rng.NextFloat() * 0.6f);
                            Vector3 p = center;
                            while (intensity > 0)
                            {
                                Int3 bp = Int3.Floor(p);
                                if (bp.y < w.minY || bp.y >= w.maxY) break;
                                ushort s = w.GetState(bp);
                                if (s != 0)
                                {
                                    var b = Blocks.ByState[s];
                                    float res = b.isLiquid ? 100f : b.blastResistance;
                                    if (b.hardness < 0) res = 3600000;
                                    intensity -= (res + 0.3f) * 0.3f;
                                    if (intensity > 0 && !b.isLiquid && toBlow.Add(bp)) ordered.Add(bp);
                                }
                                p += d * 0.3f;
                                intensity -= 0.22500001f;
                            }
                        }
            }
            // entities
            float r2 = power * 2f;
            var box = new AABB(center - Vector3.one * (r2 + 1), center + Vector3.one * (r2 + 1));
            var ents = new List<Entity>(w.GetEntities(box));
            foreach (var e in ents)
            {
                if (e.removed) continue;
                float dist = Vector3.Distance(e.position, center) / r2;
                if (dist > 1f) continue;
                Vector3 dir = e.position + Vector3.up * (e is PrimedTnt ? 0 : e.EyeHeight) - center;
                if (dir.sqrMagnitude < 1e-6f) dir = Vector3.up;
                dir.Normalize();
                float exposure = Exposure(w, center, e);
                float impact = (1f - dist) * exposure;
                float dmg = (impact * impact + impact) / 2f * 7f * r2 + 1f;
                if (e is LivingEntity le)
                {
                    le.Hurt(DamageSource.Explosion(source, center), dmg);
                    int bp = le.GetArmorEnchant(Enchant.BlastProtection);
                    float kb = impact * (1f - Mathf.Min(0.6f, bp * 0.15f)) * (1f - le.knockbackResistance);
                    if (!(le is Player pl && (pl.IsCreative && pl.abilities.flying))) le.velocity += dir * kb;
                }
                else
                {
                    if (e is ItemEntity || e is XpOrb) { if (dmg > 3) e.Remove(); continue; }
                    e.Hurt(DamageSource.Explosion(source, center), dmg);
                    e.velocity += dir * impact;
                }
            }
            // blocks
            Sounds.Play("entity.generic.explode", center, 4f, (1f + (Random.value - Random.value) * 0.2f) * 0.7f);
            Particles.Explosion(w, center, power);
            GameManager.Instance?.OnExplosion(center, power);
            if (ordered.Count > 0)
            {
                // shuffle drop order deterministically
                var drops = new List<(Vector3 pos, ItemStack stack)>();
                foreach (var bp in ordered)
                {
                    ushort s = w.GetState(bp);
                    if (s == 0) continue;
                    var b = Blocks.ByState[s];
                    int meta = s - b.baseState;
                    if (b is TntBlock) { w.SetState(bp, 0); PrimedTnt.Spawn(w, bp.Center - new Vector3(0, 0.5f, 0), 10 + rng.Next(20), source); continue; }
                    // 1/power chance to drop (vanilla TNT drops all since 1.21 for TNT; keep partial for creeper)
                    bool drop = source is PrimedTnt || rng.NextFloat() < 1f / power;
                    if (drop)
                    {
                        var list = new List<ItemStack>();
                        b.GetDrops(w, bp, meta, null, list, ref w.rand);
                        foreach (var st in list) if (st != null && !st.IsEmpty) drops.Add((bp.Center, st));
                    }
                    var be = w.GetBlockEntity(bp);
                    if (be != null) be.DropContents();
                    b.OnExploded(w, bp, meta);
                    w.SetState(bp, 0, SetFlags.Hooks);
                }
                // neighbour updates after all removals
                foreach (var bp in ordered) w.NotifyNeighbors(bp);
                // merge drops
                var merged = new List<(Vector3, ItemStack)>();
                foreach (var d in drops)
                {
                    bool done = false;
                    for (int i = 0; i < merged.Count; i++)
                    {
                        var m = merged[i];
                        if (m.Item2.Stackable(d.stack) && m.Item2.count + d.stack.count <= m.Item2.MaxStack && (m.Item1 - d.pos).sqrMagnitude < 16) { m.Item2.count += d.stack.count; done = true; break; }
                    }
                    if (!done) merged.Add((d.pos, d.stack));
                }
                foreach (var (p, st) in merged) w.SpawnItem(p, st);
            }
            if (fire && (w.session == null || w.session.doFireTick))
            {
                foreach (var bp in ordered)
                    if (rng.Next(3) == 0 && w.IsAir(bp) && Blocks.StateSolidForFluid[w.GetState(bp.Offset(Dir.Down))])
                        w.SetState(bp, Blocks.Fire.DefaultState);
            }
        }

        static float Exposure(World w, Vector3 center, Entity e)
        {
            var b = e.Bounds;
            Vector3 size = b.Size;
            float sx = 1f / (size.x * 2f + 1f), sy = 1f / (size.y * 2f + 1f), sz = 1f / (size.z * 2f + 1f);
            int total = 0, visible = 0;
            for (float x = 0; x <= 1f; x += sx)
                for (float y = 0; y <= 1f; y += sy)
                    for (float z = 0; z <= 1f; z += sz)
                    {
                        Vector3 p = new Vector3(Mathf.Lerp(b.min.x, b.max.x, x), Mathf.Lerp(b.min.y, b.max.y, y), Mathf.Lerp(b.min.z, b.max.z, z));
                        if (!w.ClipCollision(p, center, out _)) visible++;
                        total++;
                    }
            return total == 0 ? 0 : visible / (float)total;
        }
    }
}
