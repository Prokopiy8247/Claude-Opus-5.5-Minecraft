// Deterministic gradient noise used by world generation and texture generation.
#pragma once

#include "CoreMinimal.h"
#include "Core/MCCore.h"

/** Improved Perlin noise with a seeded permutation table and random origin offset. */
class UNREAL_MINECRAFT_API FMCPerlin
{
public:
	FMCPerlin() { Init(0); }
	explicit FMCPerlin(uint64 Seed) { Init(Seed); }
	void Init(uint64 Seed);

	/** 3D noise in approximately [-1, 1]. */
	double Noise3(double X, double Y, double Z) const;
	/** 2D noise in approximately [-1, 1]. */
	double Noise2(double X, double Y) const;

private:
	uint8 Perm[512];
	double OX = 0, OY = 0, OZ = 0;
};

/** Fractal (fBm) noise: sum of octaves, each doubling frequency. */
class UNREAL_MINECRAFT_API FMCOctaveNoise
{
public:
	FMCOctaveNoise() = default;
	/** @param Frequency base frequency (1/blocks), @param Octaves count, @param Persistence amplitude falloff */
	void Init(uint64 Seed, int32 Octaves, double InFrequency, double InPersistence = 0.5, double InLacunarity = 2.0);

	/** Normalized to roughly [-1, 1]. */
	double Sample3(double X, double Y, double Z) const;
	double Sample2(double X, double Y) const;
	/** Ridged multifractal in [0,1] (sharp crests near 1). */
	double Ridged2(double X, double Y) const;

	bool IsValid() const { return Layers.Num() > 0; }

private:
	TArray<FMCPerlin> Layers;
	TArray<double> Amplitudes;
	double Frequency = 1.0;
	double Lacunarity = 2.0;
	double Norm = 1.0;
};

/** Cheap deterministic value noise for per-block jitter (no allocation). */
namespace MCNoiseUtil
{
	FORCEINLINE double Hash01(uint64 Seed, int32 X, int32 Y, int32 Z) { return MCHash::ToUnit(MCHash::Hash3(Seed, X, Y, Z)); }
	FORCEINLINE double SmoothStep(double E0, double E1, double X)
	{
		const double T = FMath::Clamp((X - E0) / (E1 - E0), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}
	FORCEINLINE double Lerp(double A, double B, double T) { return A + (B - A) * T; }
	FORCEINLINE double ClampedMap(double X, double InA, double InB, double OutA, double OutB)
	{
		const double T = FMath::Clamp((X - InA) / (InB - InA), 0.0, 1.0);
		return OutA + (OutB - OutA) * T;
	}
	/** Piecewise linear spline through sorted (x, y) points. */
	double Spline(double X, const double* Xs, const double* Ys, int32 Count);
}
