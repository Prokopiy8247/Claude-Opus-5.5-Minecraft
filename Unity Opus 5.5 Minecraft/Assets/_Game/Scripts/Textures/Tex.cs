using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Registry of block texture names -> Texture2DArray layers. Animated textures occupy consecutive layers
    /// (frame 0 is the base layer). Blocks request names during registration; the generator then produces
    /// pixel data for every requested name.
    /// </summary>
    public static class Tex
    {
        public const int Size = 16;
        static readonly Dictionary<string, int> byName = new Dictionary<string, int>();
        static readonly List<string> names = new List<string>();
        static readonly List<int> frames = new List<int>();
        public static int[] FrameCount = new int[0];
        static readonly HashSet<string> lateNames = new HashSet<string>();
        public static bool Frozen;

        public static IReadOnlyList<string> Names => names;
        public static int LayerCount => names.Count;

        public static void Reset()
        {
            byName.Clear(); names.Clear(); frames.Clear(); Frozen = false;
            Id("missing");
        }

        /// <summary>Get or allocate the base layer for a texture name.</summary>
        public static int Id(string name)
        {
            if (string.IsNullOrEmpty(name)) name = "missing";
            if (byName.TryGetValue(name, out int id)) return id;
            if (Frozen) { if (lateNames.Add(name)) Debug.LogWarning("Texture requested after freeze: " + name); return 0; }
            int frameCount = TextureGen.GetFrameCount(name);
            id = names.Count;
            byName[name] = id;
            for (int i = 0; i < frameCount; i++)
            {
                names.Add(i == 0 ? name : name + "#" + i);
                frames.Add(i == 0 ? frameCount : 0);
            }
            return id;
        }

        public static bool Has(string name) => byName.ContainsKey(name);
        public static int Frames(int layer) => layer >= 0 && layer < frames.Count ? frames[layer] : 1;
        public static string NameOf(int layer) => layer >= 0 && layer < names.Count ? names[layer] : "missing";

        public static void Freeze()
        {
            Frozen = true;
            FrameCount = frames.ToArray();
        }
    }
}
