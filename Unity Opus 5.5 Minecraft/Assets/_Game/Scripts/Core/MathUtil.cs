using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using UnityEngine;

namespace MCR
{
    /// <summary>Integer 3D vector used for block positions.</summary>
    [Serializable]
    public struct Int3 : IEquatable<Int3>
    {
        public int x, y, z;
        public Int3(int x, int y, int z) { this.x = x; this.y = y; this.z = z; }
        public static readonly Int3 Zero = new Int3(0, 0, 0);
        public static readonly Int3 Up = new Int3(0, 1, 0);
        public static readonly Int3 Down = new Int3(0, -1, 0);
        public static Int3 operator +(Int3 a, Int3 b) => new Int3(a.x + b.x, a.y + b.y, a.z + b.z);
        public static Int3 operator -(Int3 a, Int3 b) => new Int3(a.x - b.x, a.y - b.y, a.z - b.z);
        public static Int3 operator *(Int3 a, int s) => new Int3(a.x * s, a.y * s, a.z * s);
        public static bool operator ==(Int3 a, Int3 b) => a.x == b.x && a.y == b.y && a.z == b.z;
        public static bool operator !=(Int3 a, Int3 b) => !(a == b);
        public bool Equals(Int3 o) => x == o.x && y == o.y && z == o.z;
        public override bool Equals(object obj) => obj is Int3 o && Equals(o);
        public override int GetHashCode() { unchecked { return (x * 73856093) ^ (y * 19349663) ^ (z * 83492791); } }
        public override string ToString() => $"{x}, {y}, {z}";
        public Vector3 ToVector3() => new Vector3(x, y, z);
        public Vector3 Center => new Vector3(x + 0.5f, y + 0.5f, z + 0.5f);
        public Int3 Offset(Dir d) => this + DirUtil.Offset[(int)d];
        public Int3 Offset(int dx, int dy, int dz) => new Int3(x + dx, y + dy, z + dz);
        public static Int3 Floor(Vector3 v) => new Int3(Mathf.FloorToInt(v.x), Mathf.FloorToInt(v.y), Mathf.FloorToInt(v.z));
        public int DistSq(Int3 o) { int dx = x - o.x, dy = y - o.y, dz = z - o.z; return dx * dx + dy * dy + dz * dz; }
        public long Pack() => PackPos(x, y, z);
        public static long PackPos(int x, int y, int z) => ((long)(x & 0x3FFFFFF) << 38) | ((long)(z & 0x3FFFFFF) << 12) | (long)(y & 0xFFF);
        public static Int3 Unpack(long v)
        {
            int x = (int)(v >> 38); int z = (int)((v << 26) >> 38); int y = (int)((v << 52) >> 52);
            return new Int3(x, y, z);
        }
    }

    /// <summary>
    /// Block face / horizontal directions. Unity is left-handed, so to keep the world non-mirrored relative to
    /// Minecraft (facing north, east is on the right) we use North=+Z, South=-Z, East=+X, West=-X.
    /// </summary>
    public enum Dir : byte { Down = 0, Up = 1, North = 2, South = 3, West = 4, East = 5 }

    public static class DirUtil
    {
        public static readonly Int3[] Offset =
        {
            new Int3(0, -1, 0), new Int3(0, 1, 0), new Int3(0, 0, 1), new Int3(0, 0, -1), new Int3(-1, 0, 0), new Int3(1, 0, 0)
        };
        public static readonly Vector3[] Normal =
        {
            Vector3.down, Vector3.up, Vector3.forward, Vector3.back, Vector3.left, Vector3.right
        };
        public static readonly Dir[] All = { Dir.Down, Dir.Up, Dir.North, Dir.South, Dir.West, Dir.East };
        /// <summary>Horizontal directions in facing-index order 0=North(+Z),1=East(+X),2=South(-Z),3=West(-X) (clockwise).</summary>
        public static readonly Dir[] Horizontal = { Dir.North, Dir.East, Dir.South, Dir.West };

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Dir Opposite(Dir d) => (Dir)((int)d ^ 1);
        public static bool IsHorizontal(Dir d) => d >= Dir.North;
        public static int HorizIndex(Dir d)
        {
            switch (d) { case Dir.North: return 0; case Dir.East: return 1; case Dir.South: return 2; case Dir.West: return 3; }
            return 0;
        }
        public static Dir FromHorizIndex(int i) => Horizontal[((i % 4) + 4) % 4];
        public static Dir RotateCW(Dir d) => FromHorizIndex(HorizIndex(d) + 1);
        public static Dir RotateCCW(Dir d) => FromHorizIndex(HorizIndex(d) + 3);
        /// <summary>Yaw in degrees (Unity: 0 = +Z north, 90 = +X east) to horizontal facing direction.</summary>
        public static Dir FromYaw(float yawDeg)
        {
            float y = Mathf.Repeat(yawDeg, 360f);
            if (y < 45 || y >= 315) return Dir.North;
            if (y < 135) return Dir.East;
            if (y < 225) return Dir.South;
            return Dir.West;
        }
        public static float ToYaw(Dir d)
        {
            switch (d) { case Dir.North: return 0; case Dir.East: return 90; case Dir.South: return 180; case Dir.West: return 270; }
            return 0;
        }
        public static int Axis(Dir d) => d <= Dir.Up ? 1 : (d <= Dir.South ? 2 : 0); // 0=x,1=y,2=z
        public static Dir FromVector(Vector3 v)
        {
            float ax = Mathf.Abs(v.x), ay = Mathf.Abs(v.y), az = Mathf.Abs(v.z);
            if (ay >= ax && ay >= az) return v.y > 0 ? Dir.Up : Dir.Down;
            if (ax >= az) return v.x > 0 ? Dir.East : Dir.West;
            return v.z > 0 ? Dir.North : Dir.South;
        }
        public static string Name(Dir d)
        {
            switch (d) { case Dir.Down: return "down"; case Dir.Up: return "up"; case Dir.North: return "north"; case Dir.South: return "south"; case Dir.West: return "west"; default: return "east"; }
        }
    }

    /// <summary>Axis aligned bounding box in world units.</summary>
    [Serializable]
    public struct AABB
    {
        public Vector3 min, max;
        public AABB(Vector3 min, Vector3 max) { this.min = min; this.max = max; }
        public AABB(float x0, float y0, float z0, float x1, float y1, float z1) { min = new Vector3(x0, y0, z0); max = new Vector3(x1, y1, z1); }
        public static AABB FromCenterBottom(Vector3 feet, float width, float height)
        {
            float h = width * 0.5f;
            return new AABB(new Vector3(feet.x - h, feet.y, feet.z - h), new Vector3(feet.x + h, feet.y + height, feet.z + h));
        }
        public Vector3 Center => (min + max) * 0.5f;
        public Vector3 Size => max - min;
        public AABB Offset(Vector3 o) => new AABB(min + o, max + o);
        public AABB Offset(float x, float y, float z) => new AABB(min.x + x, min.y + y, min.z + z, max.x + x, max.y + y, max.z + z);
        public AABB Grow(float g) => new AABB(min - new Vector3(g, g, g), max + new Vector3(g, g, g));
        public AABB Grow(float x, float y, float z) => new AABB(min.x - x, min.y - y, min.z - z, max.x + x, max.y + y, max.z + z);
        public AABB Expand(Vector3 v)
        {
            Vector3 mn = min, mx = max;
            if (v.x < 0) mn.x += v.x; else mx.x += v.x;
            if (v.y < 0) mn.y += v.y; else mx.y += v.y;
            if (v.z < 0) mn.z += v.z; else mx.z += v.z;
            return new AABB(mn, mx);
        }
        public bool Intersects(in AABB o) =>
            min.x < o.max.x && max.x > o.min.x && min.y < o.max.y && max.y > o.min.y && min.z < o.max.z && max.z > o.min.z;
        public bool Contains(Vector3 p) => p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;

        public float ClipX(in AABB o, float dx)
        {
            if (o.max.y <= min.y || o.min.y >= max.y || o.max.z <= min.z || o.min.z >= max.z) return dx;
            if (dx > 0 && o.max.x <= min.x) { float d = min.x - o.max.x; if (d < dx) dx = d; }
            else if (dx < 0 && o.min.x >= max.x) { float d = max.x - o.min.x; if (d > dx) dx = d; }
            return dx;
        }
        public float ClipY(in AABB o, float dy)
        {
            if (o.max.x <= min.x || o.min.x >= max.x || o.max.z <= min.z || o.min.z >= max.z) return dy;
            if (dy > 0 && o.max.y <= min.y) { float d = min.y - o.max.y; if (d < dy) dy = d; }
            else if (dy < 0 && o.min.y >= max.y) { float d = max.y - o.min.y; if (d > dy) dy = d; }
            return dy;
        }
        public float ClipZ(in AABB o, float dz)
        {
            if (o.max.x <= min.x || o.min.x >= max.x || o.max.y <= min.y || o.min.y >= max.y) return dz;
            if (dz > 0 && o.max.z <= min.z) { float d = min.z - o.max.z; if (d < dz) dz = d; }
            else if (dz < 0 && o.min.z >= max.z) { float d = max.z - o.min.z; if (d > dz) dz = d; }
            return dz;
        }

        /// <summary>Ray intersection. Returns true and distance t and hit face.</summary>
        public bool Raycast(Vector3 origin, Vector3 dir, float maxDist, out float t, out Dir face)
        {
            t = 0; face = Dir.Up;
            float tmin = 0f, tmax = maxDist;
            Dir enterFace = Dir.Up;
            for (int a = 0; a < 3; a++)
            {
                float o = origin[a], d = dir[a], mn = min[a], mx = max[a];
                if (Mathf.Abs(d) < 1e-8f)
                {
                    if (o < mn || o > mx) return false;
                    continue;
                }
                float inv = 1f / d;
                float t1 = (mn - o) * inv, t2 = (mx - o) * inv;
                Dir f1, f2;
                if (a == 0) { f1 = Dir.West; f2 = Dir.East; }
                else if (a == 1) { f1 = Dir.Down; f2 = Dir.Up; }
                else { f1 = Dir.South; f2 = Dir.North; }
                if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; Dir tf = f1; f1 = f2; f2 = tf; }
                if (t1 > tmin) { tmin = t1; enterFace = f1; }
                if (t2 < tmax) tmax = t2;
                if (tmin > tmax) return false;
            }
            t = tmin; face = enterFace;
            return true;
        }
        public override string ToString() => $"[{min} - {max}]";
    }

    /// <summary>Deterministic hashing helpers (thread-safe, stateless).</summary>
    public static class Hash
    {
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static uint Mix(uint x)
        {
            x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
            return x;
        }
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static ulong Mix64(ulong z)
        {
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9UL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebUL;
            return z ^ (z >> 31);
        }
        public static uint Get(int seed, int x) => Mix((uint)seed * 0x9E3779B9U ^ Mix((uint)x));
        public static uint Get(int seed, int x, int y) => Mix(Mix((uint)seed ^ 0x85ebca6bU) ^ Mix((uint)x * 0x27d4eb2dU) ^ Mix((uint)y * 0x165667b1U + 0x3c6ef372U));
        public static uint Get(int seed, int x, int y, int z) => Mix(Get(seed, x, y) ^ Mix((uint)z * 0xd3a2646cU + 0x9e3779b9U));
        public static float Float01(int seed, int x, int y, int z) => (Get(seed, x, y, z) & 0xFFFFFF) / 16777216f;
        public static float Float01(int seed, int x, int y) => (Get(seed, x, y) & 0xFFFFFF) / 16777216f;
        public static int StringHash(string s)
        {
            unchecked
            {
                uint h = 2166136261;
                for (int i = 0; i < s.Length; i++) { h ^= s[i]; h *= 16777619; }
                return (int)Mix(h);
            }
        }
        public static long Seed64(int seed, int a, int b, int salt)
        {
            return (long)Mix64(((ulong)(uint)seed << 32 | (uint)salt) ^ Mix64(((ulong)(uint)a << 32) | (uint)b));
        }
    }

    /// <summary>Small fast deterministic RNG (SplitMix64 based). Value type.</summary>
    public struct RNG
    {
        public ulong state;
        public RNG(long seed) { state = (ulong)seed ^ 0x9E3779B97F4A7C15UL; if (state == 0) state = 1; NextULong(); }
        public RNG(int seed, int a, int b, int salt) : this(Hash.Seed64(seed, a, b, salt)) { }
        public ulong NextULong()
        {
            state += 0x9E3779B97F4A7C15UL;
            ulong z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9UL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBUL;
            return z ^ (z >> 31);
        }
        public int NextInt() => (int)(NextULong() >> 33);
        /// <summary>[0, n)</summary>
        public int Next(int n) => n <= 1 ? 0 : (int)((NextULong() >> 33) % (ulong)n);
        /// <summary>[min, max] inclusive</summary>
        public int Range(int min, int maxInclusive) => min + Next(maxInclusive - min + 1);
        public float NextFloat() => (NextULong() >> 40) / 16777216f;
        public float Range(float min, float max) => min + NextFloat() * (max - min);
        public double NextDouble() => (NextULong() >> 11) * (1.0 / 9007199254740992.0);
        public bool Chance(float p) => NextFloat() < p;
        public bool NextBool() => (NextULong() & 1) != 0;
        public float Gaussian()
        {
            float u1 = Mathf.Max(1e-7f, NextFloat()), u2 = NextFloat();
            return Mathf.Sqrt(-2f * Mathf.Log(u1)) * Mathf.Cos(2f * Mathf.PI * u2);
        }
        public T Pick<T>(T[] arr) => arr[Next(arr.Length)];
        public T Pick<T>(List<T> arr) => arr[Next(arr.Count)];
        public void Shuffle<T>(IList<T> list)
        {
            for (int i = list.Count - 1; i > 0; i--) { int j = Next(i + 1); T t = list[i]; list[i] = list[j]; list[j] = t; }
        }
    }

    public static class HalfConv
    {
        /// <summary>Thread-safe float -> IEEE half conversion (round to nearest).</summary>
        [StructLayout(LayoutKind.Explicit)]
        struct FloatBits { [FieldOffset(0)] public float f; [FieldOffset(0)] public uint u; }
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static ushort ToHalf(float f)
        {
            var fb = new FloatBits { f = f };
            return ToHalfBits(fb.u);
        }
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static ushort ToHalfBits(uint x)
        {
            uint sign = (x >> 16) & 0x8000u;
            int exp = (int)((x >> 23) & 0xFF) - 127 + 15;
            uint mant = x & 0x7FFFFFu;
            if (exp <= 0)
            {
                if (exp < -10) return (ushort)sign;
                mant |= 0x800000u;
                int shift = 14 - exp;
                uint half = mant >> shift;
                if (((mant >> (shift - 1)) & 1) != 0) half++;
                return (ushort)(sign | half);
            }
            if (exp >= 31) return (ushort)(sign | 0x7C00u);
            uint h = sign | ((uint)exp << 10) | (mant >> 13);
            if ((mant & 0x1000u) != 0) h++;
            return (ushort)h;
        }
    }

    public static class MathX
    {
        public static int FloorDiv(int a, int b) => a >= 0 ? a / b : ((a + 1) / b) - 1;
        public static int FloorMod(int a, int b) { int m = a % b; return m < 0 ? m + b : m; }
        public static float Smooth(float t) => t * t * (3f - 2f * t);
        public static double Lerp(double a, double b, double t) => a + (b - a) * t;
        public static float Remap(float v, float a0, float a1, float b0, float b1) => b0 + (v - a0) / (a1 - a0) * (b1 - b0);
        public static float Clamp01(float v) => v < 0 ? 0 : (v > 1 ? 1 : v);
        public static double Clamp(double v, double a, double b) => v < a ? a : (v > b ? b : v);
        public static float WrapAngle(float a) { a %= 360f; if (a >= 180f) a -= 360f; if (a < -180f) a += 360f; return a; }
        public static float ApproachAngle(float cur, float target, float maxStep)
        {
            float d = WrapAngle(target - cur);
            if (d > maxStep) d = maxStep; if (d < -maxStep) d = -maxStep;
            return cur + d;
        }
        public static Color32 Lerp(Color32 a, Color32 b, float t)
        {
            t = Clamp01(t);
            return new Color32((byte)(a.r + (b.r - a.r) * t), (byte)(a.g + (b.g - a.g) * t), (byte)(a.b + (b.b - a.b) * t), (byte)(a.a + (b.a - a.a) * t));
        }
        public static Color32 Mul(Color32 c, float f)
        {
            return new Color32((byte)Mathf.Clamp(c.r * f, 0, 255), (byte)Mathf.Clamp(c.g * f, 0, 255), (byte)Mathf.Clamp(c.b * f, 0, 255), c.a);
        }
        public static Color32 Hex(uint rgb, byte a = 255) => new Color32((byte)(rgb >> 16), (byte)(rgb >> 8), (byte)rgb, a);
        public static Vector3 YawPitchToDir(float yaw, float pitch)
        {
            float cy = Mathf.Cos(yaw * Mathf.Deg2Rad), sy = Mathf.Sin(yaw * Mathf.Deg2Rad);
            float cp = Mathf.Cos(pitch * Mathf.Deg2Rad), sp = Mathf.Sin(pitch * Mathf.Deg2Rad);
            return new Vector3(sy * cp, -sp, cy * cp);
        }
        public static float YawFromDir(Vector3 d) => Mathf.Atan2(d.x, d.z) * Mathf.Rad2Deg;
        public static float PitchFromDir(Vector3 d) => -Mathf.Atan2(d.y, Mathf.Sqrt(d.x * d.x + d.z * d.z)) * Mathf.Rad2Deg;
    }
}
