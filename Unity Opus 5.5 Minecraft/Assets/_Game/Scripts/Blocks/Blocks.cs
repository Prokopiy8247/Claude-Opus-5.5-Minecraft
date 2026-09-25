using System;
using System.Collections.Generic;
using System.Globalization;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Global block registry. Every block owns a contiguous range of 16-bit state ids.
    /// After Init(), per-state lookup tables are available for fast, thread-safe queries.
    /// </summary>
    public static partial class Blocks
    {
        public static readonly List<Block> All = new List<Block>();
        static readonly Dictionary<string, Block> byId = new Dictionary<string, Block>();
        public static Block[] ByState = new Block[0];
        public static bool[] StateOpaque = new bool[0];       // full opaque cube (culls, blocks light)
        public static bool[] StateAO = new bool[0];           // casts ambient occlusion
        public static byte[] StateOccludes = new byte[0];     // 6-bit face occlusion mask
        public static byte[] StateOpacity = new byte[0];      // light opacity 0..15
        public static byte[] StateEmission = new byte[0];     // light emission 0..15
        public static bool[] StateSolidForFluid = new bool[0];
        public static bool[] StateAir = new bool[0];
        public static RenderLayer[] StateLayer = new RenderLayer[0];
        public static bool[] StateWaterlogged = new bool[0];      // non-fluid block that also renders/behaves as water
        public static int StateCount;
        public static bool Initialized;

        public static Block Get(string id) => id != null && byId.TryGetValue(id, out var b) ? b : null;
        public static ushort StateOf(string id) { var b = Get(id); return b != null ? b.DefaultState : (ushort)0; }

        public static T Reg<T>(string id, T b) where T : Block
        {
            if (byId.ContainsKey(id)) { Debug.LogError("Duplicate block id " + id); return (T)byId[id]; }
            b.id = id;
            if (string.IsNullOrEmpty(b.displayName)) b.displayName = PrettyName(id);
            b.index = All.Count;
            All.Add(b);
            byId[id] = b;
            return b;
        }
        public static Block Reg(string id) => Reg(id, new Block());

        public static string PrettyName(string id)
        {
            var ti = CultureInfo.InvariantCulture.TextInfo;
            string s = id.Replace('_', ' ');
            s = ti.ToTitleCase(s);
            s = s.Replace(" Of ", " of ").Replace(" And ", " and ").Replace(" The ", " the ").Replace(" On ", " on ");
            return s;
        }

        public static void Init()
        {
            if (Initialized) return;
            All.Clear(); byId.Clear();
            Tex.Reset();
            RegisterAll();
            // assign state ranges
            int s = 0;
            foreach (var b in All)
            {
                b.baseState = (ushort)s;
                s += Math.Max(1, b.stateCount);
            }
            if (s > 65535) throw new Exception("Too many block states: " + s);
            StateCount = s;
            ByState = new Block[s];
            StateOpaque = new bool[s]; StateAO = new bool[s]; StateOccludes = new byte[s];
            StateOpacity = new byte[s]; StateEmission = new byte[s]; StateSolidForFluid = new bool[s];
            StateAir = new bool[s]; StateLayer = new RenderLayer[s]; StateWaterlogged = new bool[s];
            foreach (var b in All)
            {
                for (int m = 0; m < Math.Max(1, b.stateCount); m++)
                {
                    int st = b.baseState + m;
                    ByState[st] = b;
                    bool op = b.IsOpaqueCube(m);
                    StateOpaque[st] = op;
                    StateAO[st] = op;
                    StateOccludes[st] = (byte)b.GetOccludingFaces(m);
                    StateOpacity[st] = b.GetLightOpacity(m);
                    StateEmission[st] = b.GetLightEmission(m);
                    StateSolidForFluid[st] = b.solid && !b.isLiquid && !b.replaceable;
                    StateAir[st] = b.isAir;
                    StateLayer[st] = b.GetLayer(m);
                    StateWaterlogged[st] = !b.isLiquid && b.IsWaterLike(m);
                }
            }
            Initialized = true;
            Debug.Log($"[Blocks] Registered {All.Count} blocks, {StateCount} states, {Tex.LayerCount} texture layers");
        }

        public static bool SameFluid(Block a, Block b)
        {
            if (!a.isLiquid || !b.isLiquid) return false;
            return ((FluidBlock)a).fluidKind == ((FluidBlock)b).fluidKind;
        }

        public static bool IsWater(ushort s) => ByState[s] is FluidBlock f && f.fluidKind == 0;
        public static bool IsLava(ushort s) => ByState[s] is FluidBlock f && f.fluidKind == 1;
    }

    /// <summary>Fluent builder helpers for block registration.</summary>
    public static class BlockBuilder
    {
        public static T Hard<T>(this T b, float hardness, float resistance = -1f) where T : Block
        {
            b.hardness = hardness; b.blastResistance = resistance < 0 ? hardness : resistance; return b;
        }
        public static T Tool<T>(this T b, ToolType t, int tier = 0, bool requires = false) where T : Block
        {
            b.tool = t; b.toolTier = tier; b.requiresTool = requires; return b;
        }
        public static T Pick<T>(this T b, int tier = Tier.Wood) where T : Block { b.tool = ToolType.Pickaxe; b.toolTier = tier; b.requiresTool = true; return b; }
        public static T Axe<T>(this T b) where T : Block { b.tool = ToolType.Axe; return b; }
        public static T Shovel<T>(this T b) where T : Block { b.tool = ToolType.Shovel; return b; }
        public static T Hoe<T>(this T b) where T : Block { b.tool = ToolType.Hoe; return b; }
        public static T Snd<T>(this T b, SoundType s) where T : Block { b.sound = s; return b; }
        public static T Tab<T>(this T b, CreativeTab t) where T : Block { b.creativeTab = t; return b; }
        public static T Name<T>(this T b, string n) where T : Block { b.displayName = n; return b; }
        public static T Light<T>(this T b, byte emission) where T : Block { b.lightEmission = emission; return b; }
        public static T Opacity<T>(this T b, byte o) where T : Block { b.lightOpacity = o; return b; }
        public static T Flam<T>(this T b, int catchChance, int burn) where T : Block { b.flammability = catchChance; b.fireSpread = burn; return b; }
        public static T Slip<T>(this T b, float s) where T : Block { b.slipperiness = s; return b; }
        public static T Speed<T>(this T b, float s) where T : Block { b.speedFactor = s; return b; }
        public static T Drops<T>(this T b, string itemId) where T : Block { b.dropItemId = itemId; return b; }
        public static T Xp<T>(this T b, int min, int max) where T : Block { b.xpDropMin = min; b.xpDropMax = max; return b; }
        public static T Hidden<T>(this T b) where T : Block { b.hiddenInCreative = true; return b; }
        public static T NoItem<T>(this T b) where T : Block { b.noItem = true; b.hiddenInCreative = true; return b; }
        public static T Tint<T>(this T b, TintType t) where T : Block { b.tint = t; return b; }
        public static T Map<T>(this T b, uint rgb) where T : Block { b.mapColor = MathX.Hex(rgb); return b; }
        public static T Grav<T>(this T b) where T : Block { b.gravity = true; return b; }
        public static T Push<T>(this T b, PushReaction p) where T : Block { b.push = p; return b; }
        public static T Random<T>(this T b) where T : Block { b.randomTicks = true; return b; }
        /// <summary>Same texture on all faces.</summary>
        public static T T1<T>(this T b, string tex) where T : Block { b.SetAllTex(Tex.Id(tex)); return b; }
        /// <summary>top / bottom / side textures.</summary>
        public static T T3<T>(this T b, string top, string bottom, string side) where T : Block
        {
            b.SetTex(Tex.Id(top), Tex.Id(bottom), Tex.Id(side)); return b;
        }
        /// <summary>top+bottom / side textures.</summary>
        public static T T2<T>(this T b, string end, string side) where T : Block
        {
            int e = Tex.Id(end); b.SetTex(e, e, Tex.Id(side)); return b;
        }
        /// <summary>Per face: down, up, north, south, west, east.</summary>
        public static T T6<T>(this T b, string down, string up, string north, string south, string west, string east) where T : Block
        {
            b.faceTex[0] = Tex.Id(down); b.faceTex[1] = Tex.Id(up); b.faceTex[2] = Tex.Id(north);
            b.faceTex[3] = Tex.Id(south); b.faceTex[4] = Tex.Id(west); b.faceTex[5] = Tex.Id(east);
            b.particleTex = b.faceTex[2];
            return b;
        }
        public static T Transparent<T>(this T b, RenderLayer layer = RenderLayer.Cutout, byte opacity = 0) where T : Block
        {
            b.opaqueCube = false; b.layer = layer; b.lightOpacity = opacity; return b;
        }
        public static T NoCollide<T>(this T b) where T : Block { b.solid = false; b.sturdy = false; return b; }
        public static T Replaceable<T>(this T b) where T : Block { b.replaceable = true; return b; }
        public static T Climb<T>(this T b) where T : Block { b.climbable = true; return b; }
    }
}
