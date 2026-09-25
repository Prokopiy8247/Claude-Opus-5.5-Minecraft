#include "Core/MCNoise.h"

namespace
{
	FORCEINLINE double Fade(double T) { return T * T * T * (T * (T * 6.0 - 15.0) + 10.0); }
	FORCEINLINE double Grad3(int32 Hash, double X, double Y, double Z)
	{
		switch (Hash & 15)
		{
		case 0: return X + Y;   case 1: return -X + Y;  case 2: return X - Y;   case 3: return -X - Y;
		case 4: return X + Z;   case 5: return -X + Z;  case 6: return X - Z;   case 7: return -X - Z;
		case 8: return Y + Z;   case 9: return -Y + Z;  case 10: return Y - Z;  case 11: return -Y - Z;
		case 12: return X + Y;  case 13: return -Y + Z; case 14: return -X + Y; default: return -Y - Z;
		}
	}
	FORCEINLINE double Grad2(int32 Hash, double X, double Y)
	{
		switch (Hash & 7)
		{
		case 0: return X + Y;  case 1: return -X + Y; case 2: return X - Y;  case 3: return -X - Y;
		case 4: return X;      case 5: return -X;     case 6: return Y;      default: return -Y;
		}
	}
}

void FMCPerlin::Init(uint64 Seed)
{
	FMCRandom R(Seed ^ 0xA5A5F00DULL);
	for (int32 i = 0; i < 256; ++i) Perm[i] = (uint8)i;
	for (int32 i = 255; i > 0; --i)
	{
		const int32 J = R.NextInt(i + 1);
		Swap(Perm[i], Perm[J]);
	}
	for (int32 i = 0; i < 256; ++i) Perm[256 + i] = Perm[i];
	OX = R.NextDouble() * 256.0;
	OY = R.NextDouble() * 256.0;
	OZ = R.NextDouble() * 256.0;
}

double FMCPerlin::Noise3(double X, double Y, double Z) const
{
	X += OX; Y += OY; Z += OZ;
	const double FX = FMath::FloorToDouble(X), FY = FMath::FloorToDouble(Y), FZ = FMath::FloorToDouble(Z);
	const int32 IX = (int32)((int64)FX & 255), IY = (int32)((int64)FY & 255), IZ = (int32)((int64)FZ & 255);
	X -= FX; Y -= FY; Z -= FZ;
	const double U = Fade(X), V = Fade(Y), W = Fade(Z);
	const int32 A = Perm[IX] + IY, AA = Perm[A] + IZ, AB = Perm[A + 1] + IZ;
	const int32 B = Perm[IX + 1] + IY, BA = Perm[B] + IZ, BB = Perm[B + 1] + IZ;
	const double R = FMath::Lerp(
		FMath::Lerp(FMath::Lerp(Grad3(Perm[AA], X, Y, Z), Grad3(Perm[BA], X - 1, Y, Z), U),
			FMath::Lerp(Grad3(Perm[AB], X, Y - 1, Z), Grad3(Perm[BB], X - 1, Y - 1, Z), U), V),
		FMath::Lerp(FMath::Lerp(Grad3(Perm[AA + 1], X, Y, Z - 1), Grad3(Perm[BA + 1], X - 1, Y, Z - 1), U),
			FMath::Lerp(Grad3(Perm[AB + 1], X, Y - 1, Z - 1), Grad3(Perm[BB + 1], X - 1, Y - 1, Z - 1), U), V), W);
	return R;
}

double FMCPerlin::Noise2(double X, double Y) const
{
	X += OX; Y += OY;
	const double FX = FMath::FloorToDouble(X), FY = FMath::FloorToDouble(Y);
	const int32 IX = (int32)((int64)FX & 255), IY = (int32)((int64)FY & 255);
	X -= FX; Y -= FY;
	const double U = Fade(X), V = Fade(Y);
	const int32 A = Perm[IX] + IY, B = Perm[IX + 1] + IY;
	const double R = FMath::Lerp(
		FMath::Lerp(Grad2(Perm[A], X, Y), Grad2(Perm[B], X - 1, Y), U),
		FMath::Lerp(Grad2(Perm[A + 1], X, Y - 1), Grad2(Perm[B + 1], X - 1, Y - 1), U), V);
	return R * 0.7071;
}

void FMCOctaveNoise::Init(uint64 Seed, int32 Octaves, double InFrequency, double InPersistence, double InLacunarity)
{
	Layers.SetNum(Octaves);
	Amplitudes.SetNum(Octaves);
	Frequency = InFrequency;
	Lacunarity = InLacunarity;
	double Amp = 1.0, Sum = 0.0;
	for (int32 i = 0; i < Octaves; ++i)
	{
		Layers[i].Init(MCHash::SplitMix64(Seed + (uint64)i * 0x51A5E3ull));
		Amplitudes[i] = Amp;
		Sum += Amp;
		Amp *= InPersistence;
	}
	Norm = Sum > 0 ? 1.0 / Sum : 1.0;
}

double FMCOctaveNoise::Sample3(double X, double Y, double Z) const
{
	double F = Frequency, R = 0.0;
	for (int32 i = 0; i < Layers.Num(); ++i)
	{
		R += Layers[i].Noise3(X * F, Y * F, Z * F) * Amplitudes[i];
		F *= Lacunarity;
	}
	return R * Norm;
}

double FMCOctaveNoise::Sample2(double X, double Y) const
{
	double F = Frequency, R = 0.0;
	for (int32 i = 0; i < Layers.Num(); ++i)
	{
		R += Layers[i].Noise2(X * F, Y * F) * Amplitudes[i];
		F *= Lacunarity;
	}
	return R * Norm;
}

double FMCOctaveNoise::Ridged2(double X, double Y) const
{
	double F = Frequency, R = 0.0, Weight = 1.0;
	for (int32 i = 0; i < Layers.Num(); ++i)
	{
		double N = 1.0 - FMath::Abs(Layers[i].Noise2(X * F, Y * F));
		N *= N;
		N *= Weight;
		Weight = FMath::Clamp(N * 2.0, 0.0, 1.0);
		R += N * Amplitudes[i];
		F *= Lacunarity;
	}
	return R * Norm;
}

double MCNoiseUtil::Spline(double X, const double* Xs, const double* Ys, int32 Count)
{
	if (X <= Xs[0]) return Ys[0];
	for (int32 i = 1; i < Count; ++i)
	{
		if (X <= Xs[i])
		{
			const double T = (X - Xs[i - 1]) / (Xs[i] - Xs[i - 1]);
			// smooth (cubic hermite) interpolation between points for nicer terrain
			const double S = T * T * (3.0 - 2.0 * T);
			return Ys[i - 1] + (Ys[i] - Ys[i - 1]) * S;
		}
	}
	return Ys[Count - 1];
}
