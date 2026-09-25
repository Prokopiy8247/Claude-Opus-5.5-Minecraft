using System;
using System.Runtime.CompilerServices;

namespace MCR
{
    /// <summary>Improved Perlin gradient noise with a seeded permutation. Immutable and thread-safe after construction.</summary>
    public sealed class PerlinNoise
    {
        readonly byte[] p = new byte[512];
        readonly double ox, oy, oz;

        public PerlinNoise(ref RNG rng)
        {
            ox = rng.NextDouble() * 256.0; oy = rng.NextDouble() * 256.0; oz = rng.NextDouble() * 256.0;
            for (int i = 0; i < 256; i++) p[i] = (byte)i;
            for (int i = 255; i > 0; i--) { int j = rng.Next(i + 1); byte t = p[i]; p[i] = p[j]; p[j] = t; }
            for (int i = 0; i < 256; i++) p[i + 256] = p[i];
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        static double Fade(double t) => t * t * t * (t * (t * 6 - 15) + 10);
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        static double Grad(int hash, double x, double y, double z)
        {
            switch (hash & 15)
            {
                case 0: return x + y; case 1: return -x + y; case 2: return x - y; case 3: return -x - y;
                case 4: return x + z; case 5: return -x + z; case 6: return x - z; case 7: return -x - z;
                case 8: return y + z; case 9: return -y + z; case 10: return y - z; case 11: return -y - z;
                case 12: return y + x; case 13: return -y + z; case 14: return y - x; default: return -y - z;
            }
        }
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        static int FastFloor(double v) { int i = (int)v; return v < i ? i - 1 : i; }

        /// <summary>3D noise in approximately [-1, 1].</summary>
        public double Sample(double x, double y, double z)
        {
            x += ox; y += oy; z += oz;
            int X = FastFloor(x), Y = FastFloor(y), Z = FastFloor(z);
            x -= X; y -= Y; z -= Z;
            X &= 255; Y &= 255; Z &= 255;
            double u = Fade(x), v = Fade(y), w = Fade(z);
            int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z, B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;
            double r = MathX.Lerp(
                MathX.Lerp(MathX.Lerp(Grad(p[AA], x, y, z), Grad(p[BA], x - 1, y, z), u),
                           MathX.Lerp(Grad(p[AB], x, y - 1, z), Grad(p[BB], x - 1, y - 1, z), u), v),
                MathX.Lerp(MathX.Lerp(Grad(p[AA + 1], x, y, z - 1), Grad(p[BA + 1], x - 1, y, z - 1), u),
                           MathX.Lerp(Grad(p[AB + 1], x, y - 1, z - 1), Grad(p[BB + 1], x - 1, y - 1, z - 1), u), v), w);
            return r;
        }

        /// <summary>2D noise (a z=0 slice with offset) in approximately [-1, 1].</summary>
        public double Sample(double x, double z) => Sample(x, 0.5, z);
    }

    /// <summary>Fractal (fBm) octave noise. Octave 0 is the lowest frequency.</summary>
    public sealed class OctaveNoise
    {
        readonly PerlinNoise[] octaves;
        readonly double[] amps;
        readonly double baseFreq;
        readonly double norm;

        /// <param name="baseFreq">frequency of first octave (1/wavelength in blocks)</param>
        public OctaveNoise(int seed, int salt, int count, double baseFreq, double persistence = 0.5)
        {
            var rng = new RNG(Hash.Seed64(seed, salt, count, 7919));
            octaves = new PerlinNoise[count];
            amps = new double[count];
            double a = 1, total = 0;
            for (int i = 0; i < count; i++)
            {
                octaves[i] = new PerlinNoise(ref rng);
                amps[i] = a; total += a; a *= persistence;
            }
            norm = 1.0 / total;
            this.baseFreq = baseFreq;
        }

        public OctaveNoise(int seed, int salt, double baseFreq, params double[] amplitudes)
        {
            var rng = new RNG(Hash.Seed64(seed, salt, amplitudes.Length, 104729));
            octaves = new PerlinNoise[amplitudes.Length];
            amps = new double[amplitudes.Length];
            double total = 0;
            for (int i = 0; i < amplitudes.Length; i++)
            {
                octaves[i] = new PerlinNoise(ref rng);
                amps[i] = amplitudes[i]; total += Math.Abs(amplitudes[i]);
            }
            norm = total > 0 ? 1.0 / total : 1.0;
            this.baseFreq = baseFreq;
        }

        /// <summary>Normalised to roughly [-1,1] (usually within [-0.7,0.7]).</summary>
        public double Sample(double x, double y, double z)
        {
            double f = baseFreq, sum = 0;
            for (int i = 0; i < octaves.Length; i++)
            {
                if (amps[i] != 0) sum += octaves[i].Sample(x * f, y * f, z * f) * amps[i];
                f *= 2.0;
            }
            return sum * norm;
        }

        public double Sample(double x, double z)
        {
            double f = baseFreq, sum = 0;
            for (int i = 0; i < octaves.Length; i++)
            {
                if (amps[i] != 0) sum += octaves[i].Sample(x * f, z * f) * amps[i];
                f *= 2.0;
            }
            return sum * norm;
        }

        /// <summary>Ridged multifractal variant, returns [0,1] where 1 = ridge crest.</summary>
        public double Ridged(double x, double z)
        {
            double f = baseFreq, sum = 0, total = 0;
            for (int i = 0; i < octaves.Length; i++)
            {
                double n = 1.0 - Math.Abs(octaves[i].Sample(x * f, z * f));
                sum += n * n * amps[i]; total += amps[i];
                f *= 2.0;
            }
            return sum / total;
        }
    }

    /// <summary>Simple piecewise linear spline used for terrain shaping.</summary>
    public sealed class Spline
    {
        readonly float[] xs, ys;
        public Spline(params float[] pairs)
        {
            int n = pairs.Length / 2;
            xs = new float[n]; ys = new float[n];
            for (int i = 0; i < n; i++) { xs[i] = pairs[i * 2]; ys[i] = pairs[i * 2 + 1]; }
        }
        public float Eval(float x)
        {
            if (x <= xs[0]) return ys[0];
            int n = xs.Length;
            if (x >= xs[n - 1]) return ys[n - 1];
            for (int i = 1; i < n; i++)
            {
                if (x <= xs[i])
                {
                    float t = (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
                    t = t * t * (3f - 2f * t);
                    return ys[i - 1] + (ys[i] - ys[i - 1]) * t;
                }
            }
            return ys[n - 1];
        }
    }
}
