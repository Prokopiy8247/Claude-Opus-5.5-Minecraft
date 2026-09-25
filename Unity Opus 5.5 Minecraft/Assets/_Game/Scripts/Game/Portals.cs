using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Nether portals (frame detection, lighting, linking), End portal activation, End travel and gateways.</summary>
    public static class Portals
    {
        const int MaxSize = 21;

        static bool IsFrame(World w, Int3 p) => w.GetBlock(p).id == "obsidian";
        static bool IsInterior(Block b) => b.isAir || b is FireBlock || b is PortalBlock;

        public static bool IsObsidianFrameCorner(World w, Int3 t) => IsFrame(w, t.Offset(Dir.Down));

        public static bool IsValidPortalBlock(World w, Int3 pos, int meta)
        {
            // along the portal plane: horizontal neighbours on the axis and vertical neighbours must be portal (same axis) or obsidian
            Dir a = meta == 0 ? Dir.East : Dir.North;
            foreach (var d in new[] { a, DirUtil.Opposite(a), Dir.Up, Dir.Down })
            {
                var n = pos.Offset(d);
                var b = w.GetBlock(n);
                if (b is PortalBlock && w.GetMeta(n) == meta) continue;
                if (b.id == "obsidian") continue;
                return false;
            }
            return true;
        }

        /// <summary>pos: an interior block of a potential frame (where fire was placed). Returns true if a portal was created.</summary>
        public static bool TryLightPortal(World w, Int3 pos)
        {
            if (w.dim == DimensionId.End) return false;
            for (int axis = 0; axis < 2; axis++)
            {
                if (FindFrame(w, pos, axis, out Int3 bottomLeft, out int width, out int height))
                {
                    var portal = Blocks.Get("nether_portal");
                    Dir right = axis == 0 ? Dir.East : Dir.North;
                    for (int i = 0; i < width; i++)
                        for (int j = 0; j < height; j++)
                        {
                            var p = bottomLeft + DirUtil.Offset[(int)right] * i + new Int3(0, j, 0);
                            w.SetState(p, portal.State(axis), SetFlags.Hooks);
                        }
                    Sounds.Play("block.portal.trigger", pos.Center, 1f, 1f);
                    RegisterPortal(w, bottomLeft);
                    Achievements.Grant(GameManager.Instance?.player, "enter_the_nether_prepare");
                    return true;
                }
            }
            return false;
        }

        static bool FindFrame(World w, Int3 start, int axis, out Int3 bottomLeft, out int width, out int height)
        {
            bottomLeft = start; width = height = 0;
            Dir right = axis == 0 ? Dir.East : Dir.North, left = DirUtil.Opposite(right);
            if (!IsInterior(w.GetBlock(start))) return false;
            // drop to bottom
            Int3 p = start;
            int guard = 0;
            while (IsInterior(w.GetBlock(p.Offset(Dir.Down))) && guard++ < MaxSize) p = p.Offset(Dir.Down);
            if (!IsFrame(w, p.Offset(Dir.Down))) return false;
            // go left to the frame
            guard = 0;
            while (IsInterior(w.GetBlock(p.Offset(left))) && IsFrame(w, p.Offset(left).Offset(Dir.Down)) && guard++ < MaxSize) p = p.Offset(left);
            if (!IsFrame(w, p.Offset(left))) return false;
            bottomLeft = p;
            // width
            int wdt = 0; Int3 q = p;
            while (IsInterior(w.GetBlock(q)) && IsFrame(w, q.Offset(Dir.Down)) && wdt < MaxSize) { wdt++; q = q.Offset(right); }
            if (!IsFrame(w, q) || wdt < 2) return false;
            // height
            int hgt = 0;
            for (hgt = 0; hgt < MaxSize; hgt++)
            {
                bool rowOk = true;
                for (int i = 0; i < wdt; i++)
                {
                    var c = p + DirUtil.Offset[(int)right] * i + new Int3(0, hgt, 0);
                    if (!IsInterior(w.GetBlock(c))) { rowOk = false; break; }
                }
                if (!rowOk) break;
                if (!IsFrame(w, p.Offset(left).Offset(0, hgt, 0)) || !IsFrame(w, (p + DirUtil.Offset[(int)right] * wdt).Offset(0, hgt, 0))) return false;
            }
            if (hgt < 3) return false;
            for (int i = 0; i < wdt; i++) if (!IsFrame(w, p + DirUtil.Offset[(int)right] * i + new Int3(0, hgt, 0))) return false;
            width = wdt; height = hgt;
            return true;
        }

        static void RegisterPortal(World w, Int3 bottomLeft)
        {
            var s = w.session; if (s == null) return;
            var list = s.portals[(int)w.dim];
            foreach (var p in list) if (p.DistSq(bottomLeft) < 9) return;
            list.Add(bottomLeft);
        }

        // ------------------------------------------------------------------ nether travel
        public static void TravelNether(Entity e)
        {
            if (!(e is Player p) || GameManager.Instance == null) return;
            var s = p.world.session; if (s == null) return;
            if (p.world.dim == DimensionId.End) return;
            var targetDim = p.world.dim == DimensionId.Nether ? DimensionId.Overworld : DimensionId.Nether;
            var target = s.GetWorld(targetDim);
            float scale = targetDim == DimensionId.Nether ? 1f / 8f : 8f;
            Vector3 approx = new Vector3(p.position.x * scale, p.position.y, p.position.z * scale);
            if (targetDim == DimensionId.Nether) approx.y = Mathf.Clamp(approx.y, 32, 100);
            Int3 linked = default; bool found = false;
            float range = targetDim == DimensionId.Nether ? 16 : 128;
            float best = float.MaxValue;
            foreach (var pp in s.portals[(int)targetDim])
            {
                float dx = pp.x - approx.x, dz = pp.z - approx.z;
                float d = Mathf.Max(Mathf.Abs(dx), Mathf.Abs(dz));
                if (d <= range && d < best) { best = d; linked = pp; found = true; }
            }
            Vector3 dest = found ? linked.ToVector3() + new Vector3(0.5f, 0, 0.5f) : approx;
            Achievements.Grant(p, targetDim == DimensionId.Nether ? "enter_the_nether" : null);
            GameManager.Instance.BeginTravel(p, target, dest, () =>
            {
                if (found && target.GetBlock(linked) is PortalBlock) return linked.ToVector3() + new Vector3(0.5f, 0, 0.5f);
                return CreatePortalNear(target, Int3.Floor(approx));
            });
        }

        /// <summary>Find a safe place near 'near' and build a 4x5 obsidian portal (with platform if needed). Returns arrival position.</summary>
        public static Vector3 CreatePortalNear(World w, Int3 near)
        {
            int minY = w.dim == DimensionId.Nether ? 32 : w.minY + 8;
            int maxY = w.dim == DimensionId.Nether ? 110 : w.maxY - 10;
            Int3 bestPos = default; bool got = false; float bestD = float.MaxValue;
            for (int r = 0; r <= 16 && !got; r += 2)
                for (int dx = -r; dx <= r; dx += 2)
                    for (int dz = -r; dz <= r; dz += 2)
                    {
                        if (Mathf.Abs(dx) != r && Mathf.Abs(dz) != r) continue;
                        int x = near.x + dx, z = near.z + dz;
                        for (int y = maxY; y >= minY; y--)
                        {
                            var p = new Int3(x, y, z);
                            if (!w.GetBlock(p.Offset(Dir.Down)).solid || w.GetBlock(p.Offset(Dir.Down)).isLiquid) continue;
                            bool ok = true;
                            for (int i = -1; i <= 2 && ok; i++)
                                for (int h = 0; h < 5 && ok; h++)
                                {
                                    var b = w.GetBlock(new Int3(x + i, y + h, z));
                                    if (!(b.isAir || b.replaceable) || b.isLiquid) ok = false;
                                }
                            if (ok)
                            {
                                float d = Mathf.Abs(y - near.y) + r * 2;
                                if (d < bestD) { bestD = d; bestPos = p; got = true; }
                                break;
                            }
                        }
                    }
            if (!got) bestPos = new Int3(near.x, Mathf.Clamp(near.y, minY + 2, maxY - 6), near.z);
            BuildPortal(w, bestPos, !got);
            return new Vector3(bestPos.x + 0.5f, bestPos.y, bestPos.z + 0.5f);
        }

        static void BuildPortal(World w, Int3 p, bool platform)
        {
            ushort obs = Blocks.StateOf("obsidian");
            var portal = Blocks.Get("nether_portal");
            // frame: interior x in [p.x, p.x+1], y in [p.y, p.y+2]
            for (int i = -1; i <= 2; i++)
                for (int h = -1; h <= 3; h++)
                {
                    var q = new Int3(p.x + i, p.y + h, p.z);
                    bool edge = i == -1 || i == 2 || h == -1 || h == 3;
                    w.SetState(q, edge ? obs : portal.State(0), SetFlags.Hooks);
                }
            if (platform)
                for (int i = -1; i <= 2; i++)
                    for (int dz = -1; dz <= 1; dz++)
                    {
                        if (dz == 0) continue;
                        w.SetState(new Int3(p.x + i, p.y - 1, p.z + dz), obs, SetFlags.Hooks);
                        for (int h = 0; h < 3; h++) w.SetState(new Int3(p.x + i, p.y + h, p.z + dz), 0, SetFlags.Hooks);
                    }
            RegisterPortal(w, p);
        }

        // ------------------------------------------------------------------ End portal
        public static void TryActivateEndPortal(World w, Int3 framePos)
        {
            for (int cx = framePos.x - 4; cx <= framePos.x + 4; cx++)
                for (int cz = framePos.z - 4; cz <= framePos.z + 4; cz++)
                {
                    var c = new Int3(cx, framePos.y, cz);
                    if (RingComplete(w, c))
                    {
                        var ep = Blocks.Get("end_portal");
                        for (int dx = -1; dx <= 1; dx++) for (int dz = -1; dz <= 1; dz++) w.SetState(new Int3(cx + dx, framePos.y, cz + dz), ep.DefaultState, SetFlags.Hooks);
                        Sounds.Play("block.end_portal.spawn", c.Center, 100f, 1f);
                        if (w.session != null) w.session.endPortalLitOnce = true;
                        Achievements.Grant(GameManager.Instance?.player, "follow_ender_eye");
                        return;
                    }
                }
        }

        static bool RingComplete(World w, Int3 c)
        {
            for (int dx = -2; dx <= 2; dx++)
                for (int dz = -2; dz <= 2; dz++)
                {
                    bool ring = (Mathf.Abs(dx) == 2) != (Mathf.Abs(dz) == 2);
                    if (!ring) continue;
                    var p = new Int3(c.x + dx, c.y, c.z + dz);
                    ushort s = w.GetState(p);
                    var b = Blocks.ByState[s];
                    if (!(b is EndPortalFrameBlock) || !EndPortalFrameBlock.HasEye(s - b.baseState)) return false;
                }
            return true;
        }

        public static void TravelEnd(Entity e)
        {
            if (!(e is Player p) || GameManager.Instance == null) return;
            if (p.portalCooldown > 0) return;
            p.portalCooldown = 40;
            var s = p.world.session; if (s == null) return;
            if (p.world.dim == DimensionId.End)
            {
                // exit portal: return to spawn
                var ow = s.Overworld;
                Achievements.Grant(p, "the_end_again");
                Vector3 dest = s.worldSpawn ?? ow.generator.FindSpawn();
                if (p.spawnPos.HasValue && p.spawnDim == DimensionId.Overworld) dest = p.spawnPos.Value.Center;
                GameManager.Instance.ShowCredits();
                GameManager.Instance.BeginTravel(p, ow, dest, () =>
                {
                    if (p.spawnPos.HasValue && p.spawnDim == DimensionId.Overworld)
                    {
                        var sp = p.spawnPos.Value;
                        if (ow.GetBlock(sp) is BedBlock) return sp.Center + Vector3.up * 0.1f;
                    }
                    return GameManager.SafeSurface(ow, dest);
                });
                return;
            }
            var end = s.GetWorld(DimensionId.End);
            Achievements.Grant(p, "enter_the_end");
            GameManager.Instance.BeginTravel(p, end, new Vector3(100.5f, 49, 0.5f), () => EndArrival(end));
            s.dragonFight?.OnPlayerEnter(end);
        }

        /// <summary>Builds the 5x5 obsidian arrival platform (with a cleared pocket above) and returns the landing spot.</summary>
        public static Vector3 EndArrival(World end)
        {
            ushort obs = Blocks.StateOf("obsidian");
            for (int dx = -2; dx <= 2; dx++)
                for (int dz = -2; dz <= 2; dz++)
                {
                    end.SetState(new Int3(100 + dx, 48, dz), obs, SetFlags.Hooks);
                    for (int h = 0; h < 3; h++) end.SetState(new Int3(100 + dx, 49 + h, dz), 0, SetFlags.Hooks);
                }
            return new Vector3(100.5f, 49, 0.5f);
        }

        // ------------------------------------------------------------------ gateways
        public static void UseGateway(World w, Int3 pos, Entity e)
        {
            if (!(e is Player p) || GameManager.Instance == null) return;
            var be = w.GetBlockEntity<EndGatewayEntity>(pos);
            if (be == null || be.cooldown > 0 || p.portalCooldown > 0) return;
            be.cooldown = 40; p.portalCooldown = 40;
            Sounds.Play("block.end_gateway.spawn", pos.Center, 1f, 1f);
            if (!be.hasExit)
            {
                Vector2 dir = new Vector2(pos.x, pos.z);
                if (dir.sqrMagnitude < 1) dir = Vector2.right;
                dir.Normalize();
                Vector2 target = dir * 1024f;
                // walk outward until an outer island is found (analytic surface query)
                var eg = w.generator as EndGenerator;
                Int3 exit = new Int3(Mathf.RoundToInt(target.x), 75, Mathf.RoundToInt(target.y));
                if (eg != null)
                    for (int k = 0; k < 64; k++)
                    {
                        Vector2 t = dir * (1024f + k * 16f);
                        int x = Mathf.RoundToInt(t.x), z = Mathf.RoundToInt(t.y);
                        if (eg.SurfaceAt(x, z, out int top)) { exit = new Int3(x, top + 1, z); break; }
                    }
                be.exit = exit; be.hasExit = true;
            }
            var dest = be.exit;
            Int3 back = pos;
            GameManager.Instance.BeginTravel(p, w, dest.ToVector3() + new Vector3(0.5f, 0, 0.5f), () =>
            {
                // build the exit gateway beside the arrival point once
                Int3 gw = new Int3(dest.x + 3, dest.y + 2, dest.z);
                if (!(w.GetBlock(gw) is EndPortalBlock))
                {
                    ushort bedrock = Blocks.StateOf("bedrock");
                    var gb = Blocks.Get("end_gateway");
                    w.SetState(gw, gb.DefaultState, SetFlags.Hooks);
                    w.SetState(gw.Offset(Dir.Up), bedrock, SetFlags.Hooks); w.SetState(gw.Offset(Dir.Down), bedrock, SetFlags.Hooks);
                    var gbe = w.GetBlockEntity<EndGatewayEntity>(gw);
                    if (gbe != null) { gbe.exit = back.Offset(3, -2, 0); gbe.hasExit = true; }
                }
                return GameManager.SafeSurface(w, dest.ToVector3() + new Vector3(0.5f, 0, 0.5f));
            });
        }
    }
}
