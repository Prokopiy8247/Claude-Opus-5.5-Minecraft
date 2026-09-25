using System;
using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public sealed partial class World
    {
        public int entityTickDistance = 8; // chunks from the player in which entities tick

        public void OnChunkLit(Chunk c)
        {
            if (!c.fromSave && dim == DimensionId.Overworld) MobSpawner.OnChunkGenerated(this, c);
        }

        public void OnChunkUnload(Chunk c)
        {
            foreach (var be in c.blockEntities.Values) be.OnUnload();
            if (session?.save != null && c.entitiesSpawned)
            {
                c.unloadEntities = new List<SavedEntity>();
                SaveManager.CollectEntities(this, c, c.unloadEntities, true);
            }
            else
            {
                int x0 = c.cx << 4, z0 = c.cz << 4;
                foreach (var e in entities)
                {
                    if (e is Player || e.removed) continue;
                    int ex = Mathf.FloorToInt(e.position.x), ez = Mathf.FloorToInt(e.position.z);
                    if (ex >= x0 && ex < x0 + 16 && ez >= z0 && ez < z0 + 16) e.Remove();
                }
            }
            if (cacheChunk == c) InvalidateCache();
        }

        /// <summary>Tick all entities near the player; removes dead ones.</summary>
        public void TickEntities(Vector3 playerPos)
        {
            FlushNewEntities();
            int pcx = Mathf.FloorToInt(playerPos.x) >> 4, pcz = Mathf.FloorToInt(playerPos.z) >> 4;
            for (int i = 0; i < entities.Count; i++)
            {
                var e = entities[i];
                if (e.removed) continue;
                if (e.vehicle != null) continue; // ticked by vehicle
                int ecx = Mathf.FloorToInt(e.position.x) >> 4, ecz = Mathf.FloorToInt(e.position.z) >> 4;
                bool near = e is Player || (Math.Abs(ecx - pcx) <= entityTickDistance && Math.Abs(ecz - pcz) <= entityTickDistance);
                if (!near) { if (e is Mob m && m.CanDespawn && !m.persistent) m.Remove(); continue; }
                // nothing ticks inside a chunk that has no final block data: a player there would fall through the
                // world, and mobs would walk on air, so both wait for the chunk pipeline instead
                if (ReadyChunk(Mathf.FloorToInt(e.position.x), Mathf.FloorToInt(e.position.z)) == null) continue;
                try
                {
                    e.Tick();
                    TickPassengers(e);
                }
                catch (Exception ex) { Debug.LogError("[Entity] tick failed for " + e.TypeId + ": " + ex); e.Remove(); }
            }
            entities.RemoveAll(en => en.removed);
            FlushNewEntities();
        }

        void TickPassengers(Entity v)
        {
            for (int k = 0; k < v.passengers.Count; k++)
            {
                var p = v.passengers[k];
                if (p.removed || p.vehicle != v) continue;
                p.prevPosition = p.position;
                v.PositionRider(p);
                p.RideTick();
                TickPassengers(p);
            }
        }

        public Player NearestPlayer(Vector3 pos, float maxDist = 1e9f, bool includeCreative = true)
        {
            Player best = null; float bd = maxDist * maxDist;
            foreach (var e in entities)
            {
                if (!(e is Player p) || p.removed || p.dead) continue;
                if (!includeCreative && (p.IsCreative || p.IsSpectator)) continue;
                float d = (p.position - pos).sqrMagnitude;
                if (d < bd) { bd = d; best = p; }
            }
            return best;
        }

        public IEnumerable<Player> Players()
        {
            foreach (var e in entities) if (e is Player p && !p.removed) yield return p;
        }

        /// <summary>Raycast blocks from origin along dir. fluids: stop at fluid source/flowing blocks.</summary>
        public bool RaycastBlocks(Vector3 origin, Vector3 dir, float maxDist, bool fluids, out BlockHit hit)
        {
            hit = default;
            dir.Normalize();
            int x = Mathf.FloorToInt(origin.x), y = Mathf.FloorToInt(origin.y), z = Mathf.FloorToInt(origin.z);
            int stepX = dir.x > 0 ? 1 : -1, stepY = dir.y > 0 ? 1 : -1, stepZ = dir.z > 0 ? 1 : -1;
            float tDeltaX = dir.x != 0 ? Mathf.Abs(1f / dir.x) : float.MaxValue;
            float tDeltaY = dir.y != 0 ? Mathf.Abs(1f / dir.y) : float.MaxValue;
            float tDeltaZ = dir.z != 0 ? Mathf.Abs(1f / dir.z) : float.MaxValue;
            float tMaxX = dir.x != 0 ? ((stepX > 0 ? (x + 1 - origin.x) : (origin.x - x)) * tDeltaX) : float.MaxValue;
            float tMaxY = dir.y != 0 ? ((stepY > 0 ? (y + 1 - origin.y) : (origin.y - y)) * tDeltaY) : float.MaxValue;
            float tMaxZ = dir.z != 0 ? ((stepZ > 0 ? (z + 1 - origin.z) : (origin.z - z)) * tDeltaZ) : float.MaxValue;
            var boxes = rayBoxes;
            for (int i = 0; i < 256; i++)
            {
                if (y >= minY && y < maxY)
                {
                    ushort s = GetState(x, y, z);
                    if (s != 0)
                    {
                        var b = Blocks.ByState[s];
                        int meta = s - b.baseState;
                        bool test = b.Targetable(meta) || (fluids && b.isLiquid);
                        if (test)
                        {
                            boxes.Clear();
                            var p = new Int3(x, y, z);
                            if (b.isLiquid) { float h = MeshCtx.LiquidHeight(meta); if (Blocks.SameFluid(b, GetBlock(x, y + 1, z))) h = 1; boxes.Add(new AABB(0, 0, 0, 1, h, 1)); }
                            else b.GetSelectionBoxes(meta, this, p, boxes);
                            float best = float.MaxValue; Dir bestFace = Dir.Up;
                            foreach (var lb in boxes)
                            {
                                var wb = lb.Offset(x, y, z);
                                if (wb.Raycast(origin, dir, maxDist, out float t, out Dir f) && t < best) { best = t; bestFace = f; }
                            }
                            if (best <= maxDist)
                            {
                                hit = new BlockHit { pos = p, face = bestFace, point = origin + dir * best, distance = best, state = s };
                                return true;
                            }
                        }
                    }
                }
                if (tMaxX < tMaxY && tMaxX < tMaxZ) { if (tMaxX > maxDist) break; x += stepX; tMaxX += tDeltaX; }
                else if (tMaxY < tMaxZ) { if (tMaxY > maxDist) break; y += stepY; tMaxY += tDeltaY; }
                else { if (tMaxZ > maxDist) break; z += stepZ; tMaxZ += tDeltaZ; }
            }
            return false;
        }
        readonly List<AABB> rayBoxes = new List<AABB>();

        /// <summary>Raycast through solid collision boxes only (projectiles, line of sight).</summary>
        public bool ClipCollision(Vector3 from, Vector3 to, out BlockHit hit)
        {
            Vector3 d = to - from; float len = d.magnitude;
            hit = default;
            if (len < 1e-5f) return false;
            d /= len;
            int x = Mathf.FloorToInt(from.x), y = Mathf.FloorToInt(from.y), z = Mathf.FloorToInt(from.z);
            int sx = d.x > 0 ? 1 : -1, sy = d.y > 0 ? 1 : -1, sz = d.z > 0 ? 1 : -1;
            float tdx = d.x != 0 ? Mathf.Abs(1f / d.x) : float.MaxValue, tdy = d.y != 0 ? Mathf.Abs(1f / d.y) : float.MaxValue, tdz = d.z != 0 ? Mathf.Abs(1f / d.z) : float.MaxValue;
            float tmx = d.x != 0 ? ((sx > 0 ? (x + 1 - from.x) : (from.x - x)) * tdx) : float.MaxValue;
            float tmy = d.y != 0 ? ((sy > 0 ? (y + 1 - from.y) : (from.y - y)) * tdy) : float.MaxValue;
            float tmz = d.z != 0 ? ((sz > 0 ? (z + 1 - from.z) : (from.z - z)) * tdz) : float.MaxValue;
            for (int i = 0; i < 200; i++)
            {
                ushort s = GetState(x, y, z);
                if (s != 0)
                {
                    var b = Blocks.ByState[s];
                    if (b.solid)
                    {
                        rayBoxes.Clear();
                        b.GetCollisionBoxes(s - b.baseState, this, new Int3(x, y, z), rayBoxes);
                        float best = float.MaxValue; Dir bf = Dir.Up;
                        foreach (var lb in rayBoxes)
                        {
                            var wb = lb.Offset(x, y, z);
                            if (wb.Raycast(from, d, len, out float t, out Dir f) && t < best) { best = t; bf = f; }
                        }
                        if (best <= len) { hit = new BlockHit { pos = new Int3(x, y, z), face = bf, point = from + d * best, distance = best, state = s }; return true; }
                    }
                }
                if (tmx < tmy && tmx < tmz) { if (tmx > len) break; x += sx; tmx += tdx; }
                else if (tmy < tmz) { if (tmy > len) break; y += sy; tmy += tdy; }
                else { if (tmz > len) break; z += sz; tmz += tdz; }
            }
            return false;
        }

        public bool HasLineOfSight(Vector3 a, Vector3 b) => !ClipCollision(a, b, out _);
    }

    public struct BlockHit
    {
        public Int3 pos; public Dir face; public Vector3 point; public float distance; public ushort state;
    }
}
