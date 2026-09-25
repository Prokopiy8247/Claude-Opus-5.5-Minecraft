using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    public enum RenderLayer : byte { Opaque = 0, Cutout = 1, Translucent = 2, None = 3 }
    public enum ToolType : byte { None, Pickaxe, Axe, Shovel, Hoe, Shears, Sword, Spear }
    public enum TintType : byte { None, Grass, Foliage, Water, Spruce, Birch, Redstone, Stem, Lily, Mangrove, Custom }
    public enum SoundType : byte
    {
        Stone, Wood, Gravel, Grass, Sand, Snow, Glass, Wool, Metal, Ladder, Anvil, Slime, Honey,
        Netherrack, NetherBricks, Bone, SoulSand, Basalt, Nylium, Fungus, Moss, Deepslate, Tuff, Calcite,
        Amethyst, Copper, Chain, Lantern, Mud, Sculk, Cherry, Bamboo, Scaffolding, Coral, Crop, Lava, Water, Powder
    }
    public enum MapColor : byte { None, Grass, Sand, Wool, Fire, Ice, Metal, Plant, Snow, Clay, Dirt, Stone, Water, Wood, Quartz, Color }
    public enum PushReaction : byte { Normal, Destroy, Block, PushOnly }

    /// <summary>Tool tiers. Copper sits between stone and iron (Copper Age).</summary>
    public static class Tier
    {
        public const int None = 0, Wood = 1, Gold = 1, Stone = 2, Copper = 3, Iron = 4, Diamond = 5, Netherite = 6;
    }

    /// <summary>Context passed when a player places a block item.</summary>
    public struct PlaceContext
    {
        public World world;
        public Int3 pos;           // where the block will go
        public Int3 clickedPos;    // block that was clicked
        public Dir clickedFace;    // face of the clicked block
        public Vector3 hitLocal;   // hit point within the clicked face (0..1 block-local)
        public float playerYaw, playerPitch;
        public Dir playerFacing;   // horizontal facing of the player
        public bool sneaking;
        public Entity placer;
        public ItemStack stack;
    }

    /// <summary>
    /// Base block type. Instances are immutable after registration and shared between threads.
    /// Per-position data is stored as a global 16-bit state id: state = baseState + meta.
    /// </summary>
    public class Block
    {
        public string id;
        public int index;
        public ushort baseState;
        public int stateCount = 1;
        public string displayName;

        public RenderLayer layer = RenderLayer.Opaque;
        /// <summary>Full opaque cube: occludes neighbours, blocks light, full collision.</summary>
        public bool opaqueCube = true;
        /// <summary>Has collision.</summary>
        public bool solid = true;
        /// <summary>Supports things placed on top / counts as "solid ground" for spawning, torches etc.</summary>
        public bool sturdy = true;
        public byte lightOpacity = 15;
        public byte lightEmission = 0;
        public float hardness = 1f;
        public float blastResistance = 1f;
        public ToolType tool = ToolType.None;
        public int toolTier = 0;
        public bool requiresTool = false;
        public SoundType sound = SoundType.Stone;
        public TintType tint = TintType.None;
        public bool replaceable = false;
        public bool isLiquid = false;
        public bool isAir = false;
        public bool climbable = false;
        public bool gravity = false;
        public bool randomTicks = false;
        public float slipperiness = 0.6f;
        public float speedFactor = 1f;
        public float jumpFactor = 1f;
        public int flammability = 0; // chance to catch
        public int fireSpread = 0;   // chance to be consumed
        public bool selfCullSameType = false; // glass-like: hide faces between identical blocks
        public bool noItem = false;
        public PushReaction push = PushReaction.Normal;
        public bool isFullCubeShape = true; // for culling/AO purposes even if not opaque (glass, leaves)
        public Color32 mapColor = new Color32(128, 128, 128, 255);
        public Item item;
        public string dropItemId; // null = self
        public int xpDropMin, xpDropMax;
        public CreativeTab creativeTab = CreativeTab.Building;
        public bool hiddenInCreative = false;

        /// <summary>Texture layers per face (Down, Up, North, South, West, East).</summary>
        public int[] faceTex = new int[6];
        /// <summary>Layer used for particles / item icon fallback.</summary>
        public int particleTex;

        public Block() { }

        public ushort DefaultState => (ushort)(baseState + DefaultMeta);
        public virtual int DefaultMeta => 0;
        public ushort State(int meta) => (ushort)(baseState + meta);
        public int Meta(ushort state) => state - baseState;

        public virtual string GetName(int meta) => displayName;

        public void SetAllTex(int t) { for (int i = 0; i < 6; i++) faceTex[i] = t; particleTex = t; }
        public void SetTex(int top, int bottom, int side)
        {
            faceTex[0] = bottom; faceTex[1] = top; faceTex[2] = faceTex[3] = faceTex[4] = faceTex[5] = side; particleTex = side;
        }

        // ---------------- Per-state property queries (precomputed into Blocks.* arrays) ----------------
        public virtual bool IsOpaqueCube(int meta) => opaqueCube;
        public virtual byte GetLightEmission(int meta) => lightEmission;
        public virtual byte GetLightOpacity(int meta) => lightOpacity;
        /// <summary>Bitmask of faces (1&lt;&lt;Dir) that are full and opaque, used to cull neighbour faces.</summary>
        public virtual int GetOccludingFaces(int meta) => opaqueCube ? 0x3F : 0;
        public virtual RenderLayer GetLayer(int meta) => layer;

        // ---------------- Geometry ----------------
        /// <summary>Emit render geometry for this block into the mesh context.</summary>
        public virtual void Emit(MeshCtx ctx, int meta)
        {
            ctx.Cube(faceTex, tint);
        }

        /// <summary>Collision boxes in block-local coordinates (0..1).</summary>
        public virtual void GetCollisionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            if (solid) boxes.Add(new AABB(0, 0, 0, 1, 1, 1));
        }

        /// <summary>Outline / raycast boxes in block-local coordinates.</summary>
        public virtual void GetSelectionBoxes(int meta, World w, Int3 pos, List<AABB> boxes)
        {
            boxes.Add(new AABB(0, 0, 0, 1, 1, 1));
        }

        public virtual bool Targetable(int meta) => !isAir && !isLiquid;

        // ---------------- Behaviour hooks ----------------
        /// <summary>Returns the state to place, or 0 (air) if placement is not allowed.</summary>
        public virtual ushort GetPlacementState(ref PlaceContext ctx) => DefaultState;
        public virtual bool CanPlaceAt(World w, Int3 pos, ushort state) => true;
        /// <summary>Whether this block can remain at the position (support checks).</summary>
        public virtual bool CanSurvive(World w, Int3 pos, int meta) => true;
        public virtual void OnPlaced(World w, Int3 pos, int meta, Entity placer, ItemStack stack) { }
        public virtual void OnAdded(World w, Int3 pos, int meta, ushort oldState) { }
        public virtual void OnRemoved(World w, Int3 pos, int meta, ushort newState) { }
        public virtual void OnBroken(World w, Int3 pos, int meta, Entity breaker) { }
        /// <summary>Right click. Return true if the interaction consumed the click.</summary>
        public virtual bool OnUse(World w, Int3 pos, int meta, Player player, Dir face, Vector3 hit) => false;
        public virtual void OnAttack(World w, Int3 pos, int meta, Player player) { }
        public virtual void OnNeighborChanged(World w, Int3 pos, int meta, Int3 fromPos)
        {
            if (!CanSurvive(w, pos, meta)) w.BreakBlock(pos, true, null);
        }
        public virtual void OnRandomTick(World w, Int3 pos, int meta, ref RNG rng) { }
        public virtual void OnScheduledTick(World w, Int3 pos, int meta) { }
        public virtual void OnEntityInside(World w, Int3 pos, int meta, Entity e) { }
        public virtual void OnSteppedOn(World w, Int3 pos, int meta, Entity e) { }
        public virtual void OnFallenUpon(World w, Int3 pos, int meta, Entity e, float fallDistance)
        {
            e.ApplyFallDamage(fallDistance);
        }
        public virtual void OnProjectileHit(World w, Int3 pos, int meta, Entity projectile) { }
        public virtual void OnExploded(World w, Int3 pos, int meta) { }
        /// <summary>Animation / ambient particles (client side), called for nearby blocks occasionally.</summary>
        public virtual void AnimateTick(World w, Int3 pos, int meta, ref RNG rng) { }

        /// <summary>Get drops when broken. tool may be null. Default: own item.</summary>
        public virtual void GetDrops(World w, Int3 pos, int meta, ItemStack tool, List<ItemStack> drops, ref RNG rng)
        {
            Item it = dropItemId != null ? Items.Get(dropItemId) : item;
            if (it != null) drops.Add(new ItemStack(it, 1));
        }
        public virtual Item GetPickItem(int meta) => item;
        /// <summary>Custom block item class (doors, beds, signs...). Null = plain BlockItem.</summary>
        public virtual BlockItem CustomItem() => null;
        public virtual bool IsWaterLike(int meta) => false;
        /// <summary>Can this block (at meta) be replaced by the placement described in ctx (e.g. slab merging, snow layers)?</summary>
        public virtual bool CanBeReplaced(int meta, ref PlaceContext ctx) => replaceable;
        /// <summary>Speed multiplier for entities inside (cobweb, sweet berry bush, powder snow).</summary>
        public virtual Vector3 StuckSpeed(int meta) => Vector3.one;
        /// <summary>Optional item model name override for the held/dropped item.</summary>
        public virtual string ItemIconOverride => null;

        public virtual bool HasBlockEntity => false;
        public virtual BlockEntity CreateBlockEntity(World w, Int3 pos) => null;

        // ---------------- Redstone ----------------
        public virtual bool IsRedstoneSource(int meta) => false;
        /// <summary>Weak power emitted toward the neighbour on side 'towards' (direction from this block to neighbour).</summary>
        public virtual int GetWeakPower(World w, Int3 pos, int meta, Dir towards) => 0;
        /// <summary>Strong power (can power the block it's attached to/through).</summary>
        public virtual int GetStrongPower(World w, Int3 pos, int meta, Dir towards) => 0;
        public virtual bool ConnectsToRedstone(int meta, Dir side) => IsRedstoneSource(meta);
        public virtual int GetComparatorOutput(World w, Int3 pos, int meta) => -1;

        // ---------------- Mining ----------------
        public virtual float GetHardness(int meta) => hardness;
        public bool CanHarvestWith(ItemStack tool)
        {
            if (!requiresTool) return true;
            if (tool == null || tool.item == null) return false;
            var it = tool.item;
            if (tool.item.toolType == ToolType.Shears && this.tool == ToolType.Shears) return true;
            if (it.toolType != this.tool) return false;
            return it.tier >= toolTier;
        }

        public override string ToString() => id;
    }

    /// <summary>Helpers for encoding common state properties.</summary>
    public static class StateBits
    {
        public static int Facing(int meta) => meta & 3;
        public static Dir FacingDir(int meta) => DirUtil.FromHorizIndex(meta & 3);
        public static int WithFacing(int meta, Dir d) => (meta & ~3) | DirUtil.HorizIndex(d);
        public static bool Bit(int meta, int bit) => (meta & (1 << bit)) != 0;
        public static int SetBit(int meta, int bit, bool v) => v ? meta | (1 << bit) : meta & ~(1 << bit);
    }
}
