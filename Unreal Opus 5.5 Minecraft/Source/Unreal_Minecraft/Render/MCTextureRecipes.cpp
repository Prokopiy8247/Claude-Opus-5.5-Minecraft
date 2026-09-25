// Natural-material texture recipes (stone, soils, sand, snow, ice, wood, leaves, fluids, Nether, End, Sulfur caves).
// Crafted / functional recipes live in MCTextureRecipesBuilt.cpp, plants in MCTexturePlants.cpp.
#include "Render/MCTexSynthInternal.h"

namespace MCTS
{
	void RunBuiltRecipe(FTex& T, const FMCTexDef& D); // MCTextureRecipesBuilt.cpp

	/** Two-colour fractal mottle with optional pixel clustering. */
	void Mottle(FTex& T, const FLinearColor& A, const FLinearColor& B, int32 Cells, int32 Oct, float Contrast, uint32 Seed, int32 Cluster, float HeightAmp)
	{
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const float N = Fbm(T.U(x), T.U(y), Cells, Oct, Seed);
				const float Bk = Blocky(x, y, Cluster, Seed + 5) - 0.5f;
				const float V = FMath::Clamp((N - 0.5f) * Contrast + 0.5f + Bk * 0.18f, 0.f, 1.f);
				const int32 I = T.I(x, y);
				T.C[I] = WithA(Mix(A, B, V), T.C[I].A);
				T.H[I] = 0.5f + (N - 0.5f) * HeightAmp * 2.f;
			}
	}
}

namespace
{
	using namespace MCTS;

	void StoneLike(FTex& T, const FMCTexDef& D, float Contrast = 1.6f)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		Mottle(T, A, B, 4, 5, Contrast, D.Seed, 2, 0.4f);
		// darker veins
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const float V = FMath::Abs(Fbm(T.U(x), T.U(y), 3, 4, D.Seed + 91) - 0.5f);
				if (V < 0.035f) { const int32 I = T.I(x, y); T.C[I] = WithA(Mix(T.C[I], C, 0.55f), 1.f); T.H[I] -= 0.12f; }
			}
		T.NormalStrength = 2.2f;
	}

	void Strata(FTex& T, const FLinearColor& A, const FLinearColor& B, const FLinearColor& C, uint32 Seed, int32 Bands)
	{
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const float N = FbmAniso(T.U(x), T.U(y), 2, Bands, 4, Seed);
				const float Chip = Blocky(x, y, 3, Seed + 3);
				const int32 I = T.I(x, y);
				FLinearColor Cl = Mix(A, B, FMath::Clamp((N - 0.5f) * 2.2f + 0.5f, 0.f, 1.f));
				if (Chip > 0.93f) Cl = Mix(Cl, C, 0.6f);
				T.C[I] = WithA(Cl, 1.f);
				T.H[I] = 0.4f + N * 0.4f;
			}
		// horizontal cracks between strata
		for (int32 y = 0; y < T.S; ++y)
		{
			if (Hash01(y, Seed, 7) > 0.12f) continue;
			for (int32 x = 0; x < T.S; ++x) if (Hash01(x / 5, y, Seed) < 0.7f) { const int32 I = T.I(x, y); T.C[I] = WithA(Shade(T.C[I], 0.7f), 1.f); T.H[I] = 0.2f; }
		}
	}

	void GrassFringe(FTex& T, const FLinearColor& Grass, float TintMask, uint32 Seed, float DepthPx, bool bSnow)
	{
		const float P = T.P16();
		for (int32 x = 0; x < T.S; ++x)
		{
			const float Base = DepthPx * P * (0.75f + 0.5f * VNoise(x / (P * 1.5f), 0.5f, FMath::Max(1, (int32)(T.S / (P * 1.5f))), Seed));
			const bool bDrip = Hash01(x / FMath::Max(1, (int32)P), Seed, 2) < 0.28f;
			const float Depth = Base + (bDrip ? P * (1.f + Hash01(x / FMath::Max(1, (int32)P), Seed, 3) * 2.f) : 0.f);
			for (int32 y = 0; y < T.S && y < Depth; ++y)
			{
				const int32 I = T.I(x, y);
				const float N = Fbm(T.U(x), T.U(y), 8, 3, Seed + 11);
				const float Edge = (Depth - y) < P * 0.8f ? 0.82f : 1.f;
				FLinearColor C = Shade(Grass, (0.85f + N * 0.3f) * Edge);
				if (bSnow) C = Shade(Grass, 0.95f + N * 0.08f);
				T.C[I] = WithA(C, TintMask);
				T.H[I] = 0.7f + N * 0.2f;
				T.R[I] = bSnow ? 0.65f : 0.8f;
			}
		}
	}

	void Pebbles(FTex& T, const FLinearColor& Col_, int32 Count, float Size, uint32 Seed)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			const float X = Hash01(Seed, i, 1) * T.S, Y = Hash01(Seed, i, 2) * T.S;
			const float R = Size * (0.6f + Hash01(Seed, i, 3) * 0.8f);
			Disc(T, X + 0.7f, Y + 0.7f, R, Shade(Col_, 0.6f), 0.55f);
			Disc(T, X, Y, R, Jitter(Col_, 0.15f, Hash(Seed, i)), 0.8f);
		}
	}

	void Rings(FTex& T, const FLinearColor& A, const FLinearColor& B, const FLinearColor& Rim, uint32 Seed, float RimPx, bool bStripped)
	{
		const float C = T.S * 0.5f;
		const float P = T.P16();
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const float DX = x + 0.5f - C, DY = y + 0.5f - C;
				const float Sq = FMath::Max(FMath::Abs(DX), FMath::Abs(DY)); // rounded-square rings like sawn timber
				const float Rd = FMath::Lerp(FMath::Sqrt(DX * DX + DY * DY), Sq, 0.55f);
				const float Wob = (Fbm(T.U(x), T.U(y), 4, 3, Seed) - 0.5f) * P * 1.2f;
				const float Ring = FMath::Frac((Rd + Wob) / (P * 1.35f));
				const int32 I = T.I(x, y);
				FLinearColor Cl = Mix(A, B, Ring < 0.28f ? 0.85f : 0.1f);
				float Ht = Ring < 0.28f ? 0.45f : 0.6f;
				const float Edge = FMath::Min(FMath::Min(x, y), FMath::Min(T.S - 1 - x, T.S - 1 - y));
				if (Edge < RimPx * P)
				{
					const float N = FbmAniso(T.U(x), T.U(y), 2, 16, 3, Seed + 5);
					Cl = Mix(Rim, Shade(Rim, 0.7f), N);
					Ht = 0.7f + N * 0.2f;
					if (bStripped) Cl = Mix(Rim, Shade(Rim, 0.85f), N);
				}
				T.C[I] = WithA(Cl, 1.f);
				T.H[I] = Ht;
				T.R[I] = 0.75f;
			}
		Speckle(T, Shade(B, 0.8f), 0.004f, 1, Seed + 9);
	}

	void Bark(FTex& T, const FLinearColor& A, const FLinearColor& B, uint32 Seed, int32 WoodIndex)
	{
		const float P = T.P16();
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const float N = FbmAniso(T.U(x), T.U(y), 10, 2, 4, Seed);
				const float Ridge = FMath::Abs(FMath::Frac(N * 3.f) - 0.5f) * 2.f;
				const int32 I = T.I(x, y);
				FLinearColor Cl = Mix(B, A, FMath::Clamp(Ridge * 1.3f - 0.1f, 0.f, 1.f));
				Cl = Shade(Cl, 0.9f + Blocky(x, y, (int32)P, Seed + 1) * 0.2f);
				T.C[I] = WithA(Cl, 1.f);
				T.H[I] = 0.3f + Ridge * 0.6f;
				T.R[I] = 0.9f;
			}
		if (WoodIndex == 2) // birch: pale bark with dark horizontal lenticels
		{
			for (int32 i = 0; i < 9; ++i)
			{
				const float Y = Hash01(Seed, i, 1) * T.S, X = Hash01(Seed, i, 2) * T.S;
				const float L = (1.5f + Hash01(Seed, i, 3) * 3.f) * P;
				Line(T, X, Y, X + L, Y, P * (0.6f + Hash01(Seed, i, 4) * 0.6f), FLinearColor(0.12f, 0.12f, 0.11f, 1.f), 0.2f);
			}
		}
		if (WoodIndex == 10 || WoodIndex == 11) // nether stems: glowing veins
		{
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const float V = FMath::Abs(FbmAniso(T.U(x), T.U(y), 6, 2, 3, Seed + 50) - 0.5f);
					if (V < 0.04f) { const int32 I = T.I(x, y); T.C[I] = WithA(WoodIndex == 10 ? FLinearColor(0.75f, 0.2f, 0.25f, 1.f) : FLinearColor(0.2f, 0.85f, 0.75f, 1.f), 1.f); T.E[I] = 0.5f; }
				}
		}
	}

	void LeavesTex(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1);
		const int32 Variant = (int32)D.P0;
		const float P = T.P16();
		T.Fill(WithA(Shade(A, 0.5f), 0.f), 0.3f, 0.7f);
		// overlapping leaf clusters
		const int32 Leaves = 70;
		for (int32 i = 0; i < Leaves; ++i)
		{
			const float X = Hash01(D.Seed, i, 1) * T.S, Y = Hash01(D.Seed, i, 2) * T.S;
			const float Ang = Hash01(D.Seed, i, 3) * PI;
			const float Len = P * (1.4f + Hash01(D.Seed, i, 4) * 1.2f);
			const float Shade01 = 0.65f + Hash01(D.Seed, i, 5) * 0.5f;
			FLinearColor Cl = Shade(Mix(A, B, Hash01(D.Seed, i, 6) * 0.4f), Shade01);
			Ellipse(T, X, Y, Len, Len * 0.55f, Ang, WithA(Cl, 1.f), 0.5f + Shade01 * 0.3f);
			Line(T, X - FMath::Cos(Ang) * Len * 0.8f, Y - FMath::Sin(Ang) * Len * 0.8f, X + FMath::Cos(Ang) * Len * 0.8f, Y + FMath::Sin(Ang) * Len * 0.8f, 0.7f, WithA(Shade(Cl, 0.75f), 1.f), 0.45f);
		}
		// holes
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const float N = Fbm(T.U(x), T.U(y), 6, 3, D.Seed + 40);
				if (N < 0.3f) T.C[T.I(x, y)].A = 0.f;
			}
		if (Variant == 2 || Variant == 4) // flowering azalea
		{
			for (int32 i = 0; i < 14; ++i)
			{
				const float X = Hash01(D.Seed, i, 71) * T.S, Y = Hash01(D.Seed, i, 72) * T.S;
				Disc(T, X, Y, P * 0.9f, B, 0.8f);
				Disc(T, X, Y, P * 0.35f, FLinearColor(1.f, 0.9f, 0.5f, 1.f), 0.85f);
			}
		}
		T.NormalStrength = 1.5f;
		T.BakeAO = 0.5f;
	}

	void WaterTex(FTex& T, const FMCTexDef& D)
	{
		const bool bFlow = D.P0 > 0.5f;
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const FCell W = Worley(T.U(x), T.U(y), 5, D.Seed);
				const float Caustic = FMath::Clamp(1.f - (W.F2 - W.F1) * 4.f, 0.f, 1.f);
				const float N = bFlow ? FbmAniso(T.U(x), T.U(y), 3, 12, 3, D.Seed + 1) : Fbm(T.U(x), T.U(y), 4, 4, D.Seed + 1);
				const float V = 0.62f + N * 0.25f + Caustic * 0.18f;
				const int32 I = T.I(x, y);
				T.C[I] = FLinearColor(V, V, V, 1.f);
				T.H[I] = N * 0.6f + Caustic * 0.2f;
				T.R[I] = 0.04f;
			}
		T.NormalStrength = 1.2f;
		T.BakeAO = 0.f;
	}

	void LavaTex(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), Dark = Col(D.C2);
		const bool bFlow = D.P0 > 0.5f;
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const FCell W = Worley(T.U(x), bFlow ? FMath::Frac(T.U(y) * 0.5f) : T.U(y), 4, D.Seed);
				const float N = Fbm(T.U(x), T.U(y), 5, 4, D.Seed + 3);
				const float Hot = FMath::Clamp(1.f - W.F1 * 1.1f + (N - 0.5f) * 0.6f, 0.f, 1.f);
				const int32 I = T.I(x, y);
				FLinearColor Cl = Hot > 0.55f ? Mix(A, B, (Hot - 0.55f) / 0.45f) : Mix(Dark, A, Hot / 0.55f);
				T.C[I] = WithA(Cl, 1.f);
				T.E[I] = FMath::Clamp(Hot * 1.2f, 0.25f, 1.f);
				T.H[I] = 1.f - Hot * 0.6f;
				T.R[I] = 0.55f;
			}
		T.BakeAO = 0.1f;
	}

	void PortalTex(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1);
		const int32 Kind = (int32)D.P0;
		if (Kind == 0)
		{
			const float C = T.S * 0.5f;
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const float DX = (x + 0.5f - C) / T.S, DY = (y + 0.5f - C) / T.S;
					const float Ang = FMath::Atan2(DY, DX), Rad = FMath::Sqrt(DX * DX + DY * DY);
					const float Swirl = FMath::Sin(Ang * 3.f + Rad * 22.f + Fbm(T.U(x), T.U(y), 4, 3, D.Seed) * 6.f) * 0.5f + 0.5f;
					const int32 I = T.I(x, y);
					T.C[I] = FLinearColor(FMath::Lerp(A.R, B.R, Swirl), FMath::Lerp(A.G, B.G, Swirl), FMath::Lerp(A.B, B.B, Swirl), 0.72f + Swirl * 0.2f);
					T.E[I] = 0.6f + Swirl * 0.4f;
					T.H[I] = Swirl;
				}
		}
		else
		{
			T.Fill(WithA(A, 1.f), 0.5f, 0.2f);
			for (int32 i = 0; i < 90; ++i)
			{
				const float X = Hash01(D.Seed, i, 1) * T.S, Y = Hash01(D.Seed, i, 2) * T.S;
				const float Br = Hash01(D.Seed, i, 3);
				const FLinearColor Star = Mix(B, FLinearColor(0.8f, 1.f, 0.95f, 1.f), Hash01(D.Seed, i, 4));
				const int32 I = T.I((int32)X, (int32)Y);
				T.C[I] = WithA(Mix(A, Star, 0.4f + Br * 0.6f), 1.f);
				T.E[I] = 0.5f + Br * 0.5f;
				if (Br > 0.8f) { for (int32 k = 0; k < 4; ++k) { const int32 J = T.I((int32)X + (k == 0) - (k == 1), (int32)Y + (k == 2) - (k == 3)); T.C[J] = WithA(Mix(A, Star, 0.4f), 1.f); T.E[J] = 0.4f; } }
			}
		}
		T.BakeAO = 0.f;
		T.NormalStrength = 0.3f;
	}

	void SulfurTex(FTex& T, const FMCTexDef& D, bool bCinnabar)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const FCell W = Worley(T.U(x), T.U(y), bCinnabar ? 5 : 6, D.Seed, 0.9f);
				const float N = Fbm(T.U(x), T.U(y), 6, 4, D.Seed + 1);
				const float Edge = W.F2 - W.F1;
				const int32 I = T.I(x, y);
				// crystalline crust: faceted cells with bright ridges
				const float Facet = Hash01(W.Id, 3);
				FLinearColor Cl = Mix(A, B, Facet * 0.7f + N * 0.3f);
				float Ht = 0.5f + (1.f - W.F1) * 0.35f;
				if (Edge < 0.08f) { Cl = Mix(C, Cl, Edge / 0.08f); Ht = 0.3f; }
				if (!bCinnabar && Facet > 0.85f) Cl = Mix(Cl, FLinearColor(1.f, 0.98f, 0.7f, 1.f), 0.4f);
				T.C[I] = WithA(Cl, 1.f);
				T.H[I] = Ht;
				T.R[I] = bCinnabar ? 0.6f : 0.45f + N * 0.3f;
				T.E[I] = (D.P0 > 0.5f && Facet > 0.6f) ? 0.55f : 0.f;
			}
		T.NormalStrength = 2.8f;
	}
}

namespace MCTS
{
	void RunRecipe(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		const uint32 Sd = D.Seed;
		const float P = T.P16();
		T.Fill(A, 0.5f, 0.85f);
		switch (D.Recipe)
		{
		case EMCTexRecipe::Noise: Mottle(T, A, B, 4, 4, 1.4f, Sd); break;
		case EMCTexRecipe::Stone:
			StoneLike(T, D);
			if (D.P0 >= 2.f) { Grunge(T, 0.08f, 0.1f, 10, Sd + 7); T.R.Init(0.8f, T.S * T.S); }
			if (D.P0 >= 3.f) Stones(T, 3, A, B, C, 0.05f, Sd + 2, 0.6f); // blackstone top: coarse slabs
			break;
		case EMCTexRecipe::Speckled:
		{
			Mottle(T, A, B, 3, 5, 1.2f, Sd, 2, 0.3f);
			const int32 Kind = (int32)D.P0;
			Speckle(T, C, Kind == 2 ? 0.05f : 0.03f, Kind == 1 ? 2 : 1, Sd + 3, Kind == 2 ? -0.1f : 0.05f);
			Speckle(T, Mix(A, C, 0.5f), 0.04f, 2, Sd + 4, 0.f);
			T.NormalStrength = 1.8f;
			break;
		}
		case EMCTexRecipe::Deepslate: Strata(T, A, B, C, Sd, 20); T.NormalStrength = 2.6f; break;
		case EMCTexRecipe::DeepslateTop:
			Stones(T, 4, A, B, Shade(B, 0.7f), 0.06f, Sd, 0.5f);
			Grunge(T, 0.1f, 0.1f, 12, Sd + 1);
			break;
		case EMCTexRecipe::Tuff:
			Mottle(T, A, B, 5, 4, 1.4f, Sd, 2, 0.35f);
			Speckle(T, C, 0.05f, 2, Sd + 1, 0.12f);
			Speckle(T, Shade(A, 0.7f), 0.03f, 1, Sd + 2, -0.1f);
			break;
		case EMCTexRecipe::Calcite:
			Mottle(T, A, B, 3, 4, 1.1f, Sd, 3, 0.25f);
			Speckle(T, C, 0.02f, 2, Sd + 1, 0.05f);
			T.R.Init(0.6f, T.S * T.S);
			break;
		case EMCTexRecipe::Dripstone: Strata(T, A, B, C, Sd, 10); Grunge(T, 0.12f, 0.15f, 6, Sd + 3); break;
		case EMCTexRecipe::Cobble:
			Stones(T, 4, A, C, B, 0.12f, Sd);
			if (D.P0 >= 1.f) // mossy
			{
				for (int32 y = 0; y < T.S; ++y)
					for (int32 x = 0; x < T.S; ++x)
					{
						const float N = Fbm(T.U(x), T.U(y), 3, 4, Sd + 60);
						if (N > 0.56f) { const int32 I = T.I(x, y); T.C[I] = WithA(Mix(T.C[I], Shade(C, 0.8f + Blocky(x, y, 2, Sd) * 0.4f), 0.85f), 1.f); T.H[I] += 0.05f; T.R[I] = 0.9f; }
					}
			}
			T.NormalStrength = 3.f;
			break;
		case EMCTexRecipe::Dirt:
		{
			const int32 Kind = (int32)D.P0;
			Mottle(T, A, B, 4, 5, 1.3f, Sd, 2, 0.35f);
			Speckle(T, C, 0.035f, 1, Sd + 1, -0.1f);
			if (Kind == 1) Pebbles(T, C, 18, P * 0.7f, Sd + 2);
			if (Kind == 2) for (int32 i = 0; i < 8; ++i) Blade(T, Hash01(Sd, i, 1) * T.S, Hash01(Sd, i, 2) * T.S, P * (3 + Hash01(Sd, i, 3) * 4), Hash01(Sd, i, 4) * 2 * PI, 1.2f, P * 0.5f, C, Shade(C, 0.8f));
			if (Kind == 3) { Grunge(T, 0.05f, -0.1f, 6, Sd + 5); for (float& R : T.R) R = 0.75f; }
			if (Kind == 4)
			{
				for (int32 y = 0; y < T.S; ++y)
				{
					const float F = FMath::Frac(y / (P * 4.f));
					for (int32 x = 0; x < T.S; ++x) { const int32 I = T.I(x, y); if (F < 0.3f) { T.C[I] = WithA(Mix(T.C[I], C, 0.6f), 1.f); T.H[I] = 0.25f; } else T.H[I] = 0.6f + F * 0.2f; }
				}
				if (D.P1 > 0.5f) for (float& R : T.R) R = 0.45f;
			}
			if (Kind == 5) Speckle(T, B, 0.06f, 2, Sd + 8, 0.1f);
			break;
		}
		case EMCTexRecipe::GrassTop:
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const float N = Fbm(T.U(x), T.U(y), 6, 4, Sd);
					const float Bl = Blocky(x, y, 2, Sd + 1);
					const int32 I = T.I(x, y);
					const float V = FMath::Clamp(0.25f + N * 0.6f + (Bl - 0.5f) * 0.35f, 0.f, 1.f);
					T.C[I] = FLinearColor(FMath::Lerp(A.R, B.R, V), FMath::Lerp(A.G, B.G, V), FMath::Lerp(A.B, B.B, V), 1.f);
					T.H[I] = 0.4f + V * 0.4f;
					T.R[I] = 0.82f;
				}
			for (int32 i = 0; i < 60; ++i) Blade(T, Hash01(Sd, i, 1) * T.S, Hash01(Sd, i, 2) * T.S, P * (1.2f + Hash01(Sd, i, 3) * 1.5f), Hash01(Sd, i, 4) * 2 * PI, 0.3f, P * 0.35f, Shade(B, 0.85f), Shade(B, 1.05f));
			T.SetAlpha(1.f);
			break;
		case EMCTexRecipe::GrassSide:
		case EMCTexRecipe::NyliumSide:
		case EMCTexRecipe::SnowSide:
		{
			GenerateBase(T, D.Base.IsNone() ? FName(TEXT("dirt")) : D.Base);
			T.SetAlpha(0.f);
			const bool bTint = (D.Flags & MCTF_Tinted) != 0;
			if (D.Recipe == EMCTexRecipe::SnowSide) GrassFringe(T, C, 0.f, Sd, 3.2f, true);
			else if (D.Recipe == EMCTexRecipe::NyliumSide) GrassFringe(T, Mix(A, B, 0.5f), 0.f, Sd, 3.f, false);
			else GrassFringe(T, bTint ? FLinearColor(0.72f, 0.72f, 0.72f, 1.f) : C, bTint ? 1.f : 0.f, Sd, D.P0 >= 3.f ? 1.2f : 2.6f, false);
			break;
		}
		case EMCTexRecipe::PodzolTop:
			Mottle(T, A, B, 5, 4, 1.5f, Sd, 2, 0.3f);
			for (int32 i = 0; i < 40; ++i) Line(T, Hash01(Sd, i, 1) * T.S, Hash01(Sd, i, 2) * T.S, Hash01(Sd, i, 1) * T.S + (Hash01(Sd, i, 3) - 0.5f) * P * 3, Hash01(Sd, i, 2) * T.S + (Hash01(Sd, i, 4) - 0.5f) * P * 3, P * 0.35f, Mix(C, B, Hash01(Sd, i, 5)), 0.6f);
			break;
		case EMCTexRecipe::MyceliumTop:
			Mottle(T, A, B, 6, 4, 1.4f, Sd, 2, 0.3f);
			Speckle(T, Shade(B, 1.25f), 0.05f, 1, Sd + 1, 0.1f);
			Speckle(T, C, 0.03f, 2, Sd + 2, -0.05f);
			break;
		case EMCTexRecipe::Sand:
			Mottle(T, A, B, 8, 3, 1.1f, Sd, 1, 0.2f);
			Speckle(T, C, 0.06f, 1, Sd + 1, -0.05f);
			Speckle(T, Shade(B, 1.1f), 0.04f, 1, Sd + 2, 0.05f);
			if (D.P0 >= 1.f) Pebbles(T, Shade(C, 0.7f), 6, P * 0.5f, Sd + 3);
			T.NormalStrength = 1.2f;
			T.R.Init(0.92f, T.S * T.S);
			break;
		case EMCTexRecipe::Gravel:
			Stones(T, 7, A, B, C, 0.16f, Sd, 1.f);
			if (D.P0 >= 1.f) Pebbles(T, Shade(C, 0.8f), 5, P * 0.5f, Sd + 3);
			T.NormalStrength = 3.2f;
			break;
		case EMCTexRecipe::Clay:
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const float N = FbmAniso(T.U(x), T.U(y), 3, 8, 3, Sd);
					const int32 I = T.I(x, y);
					T.C[I] = WithA(Mix(A, B, N), 1.f);
					T.H[I] = 0.5f + N * 0.15f;
					T.R[I] = 0.7f;
				}
			Speckle(T, C, 0.02f, 1, Sd + 1);
			break;
		case EMCTexRecipe::Mud:
		{
			const int32 Kind = (int32)D.P0;
			Mottle(T, A, B, 4, 4, 1.3f, Sd, 2, 0.3f);
			if (Kind == 0) { for (float& R : T.R) R = 0.35f; Speckle(T, Shade(B, 1.2f), 0.02f, 1, Sd + 1); }
			if (Kind == 1) { Bricks(T, 4, 2, 0, A, C, 0.08f, Sd, true); Grunge(T, 0.08f, 0.1f, 6, Sd + 2); }
			if (Kind >= 2) for (int32 i = 0; i < 10; ++i) Blade(T, Hash01(Sd, i, 1) * T.S, Hash01(Sd, i, 2) * T.S, P * (4 + Hash01(Sd, i, 3) * 5), Hash01(Sd, i, 4) * 2 * PI, 0.8f, P * 0.8f, B, Shade(B, 0.8f));
			break;
		}
		case EMCTexRecipe::Moss:
			Mottle(T, A, B, 6, 4, 1.6f, Sd, 2, 0.5f);
			Speckle(T, C, 0.05f, 2, Sd + 1, -0.1f);
			for (int32 i = 0; i < 50; ++i) Blade(T, Hash01(Sd, i, 1) * T.S, Hash01(Sd, i, 2) * T.S, P * 0.9f, Hash01(Sd, i, 3) * 2 * PI, 0.2f, P * 0.3f, Shade(B, 1.1f), B);
			T.R.Init(0.95f, T.S * T.S);
			T.NormalStrength = 3.f;
			break;
		case EMCTexRecipe::Snow:
			Mottle(T, A, B, 4, 3, 1.f, Sd, 2, 0.15f);
			Speckle(T, C, 0.03f, 1, Sd + 1, -0.03f);
			Speckle(T, FLinearColor(1, 1, 1, 1), 0.01f, 1, Sd + 2, 0.02f);
			T.R.Init(D.P0 > 0.5f ? 0.8f : 0.6f, T.S * T.S);
			T.NormalStrength = 1.f;
			T.BakeAO = 0.15f;
			break;
		case EMCTexRecipe::Ice:
		case EMCTexRecipe::PackedIce:
		{
			const bool bTrans = D.Recipe == EMCTexRecipe::Ice;
			Mottle(T, A, B, 3, 4, 1.f, Sd, 3, 0.1f);
			for (int32 i = 0; i < 7; ++i)
			{
				float X = Hash01(Sd, i, 1) * T.S, Y = Hash01(Sd, i, 2) * T.S, Ang = Hash01(Sd, i, 3) * 2 * PI;
				for (int32 s = 0; s < 6; ++s)
				{
					const float L = P * (1.5f + Hash01(Sd, i, s) * 2.5f);
					const float NX = X + FMath::Cos(Ang) * L, NY = Y + FMath::Sin(Ang) * L;
					Line(T, X, Y, NX, NY, 0.8f, WithA(Mix(B, FLinearColor(1, 1, 1, 1), 0.6f), bTrans ? 0.95f : 1.f), 0.7f);
					X = NX; Y = NY; Ang += (Hash01(Sd, i, s, 9) - 0.5f) * 1.4f;
				}
			}
			for (int32 k = 0; k < T.S * T.S; ++k) { T.R[k] = bTrans ? 0.03f : 0.12f; if (bTrans) T.C[k].A = FMath::Max(T.C[k].A * 0.62f, 0.55f); }
			if (D.P0 >= 1.f) Speckle(T, WithA(FLinearColor(0.95f, 0.98f, 1.f, 1.f), 0.9f), 0.05f, 2, Sd + 5);
			T.NormalStrength = 0.8f;
			T.BakeAO = 0.05f;
			break;
		}
		case EMCTexRecipe::Bedrock:
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const FCell W = Worley(T.U(x), T.U(y), 6, Sd, 1.f);
					const float V = Hash01(W.Id, 1);
					const int32 I = T.I(x, y);
					T.C[I] = WithA(V > 0.6f ? Mix(A, B, V) : Mix(C, A, V / 0.6f), 1.f);
					T.H[I] = V * 0.8f + (1.f - W.F1) * 0.2f;
				}
			T.NormalStrength = 3.5f;
			break;
		case EMCTexRecipe::Obsidian:
		{
			const int32 Kind = (int32)D.P0;
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const FCell W = Worley(T.U(x), T.U(y), 4, Sd, 0.9f);
					const float Facet = Hash01(W.Id, 7);
					const int32 I = T.I(x, y);
					FLinearColor Cl = Mix(A, B, Facet * 0.8f);
					if (W.F2 - W.F1 < 0.05f) Cl = Mix(Cl, C, 0.5f);
					T.C[I] = WithA(Cl, 1.f);
					T.H[I] = 0.4f + Facet * 0.3f;
					T.R[I] = 0.12f + Facet * 0.15f;
				}
			if (Kind == 1 || Kind == 4 || Kind == 2)
			{
				for (int32 i = 0; i < (Kind == 2 ? 1 : 10); ++i)
				{
					const float X = Kind == 2 ? T.S * 0.5f : Hash01(Sd, i, 1) * T.S, Y = Kind == 2 ? T.S * 0.5f : Hash01(Sd, i, 2) * T.S;
					const float R = Kind == 2 ? P * 4.f : P * (0.6f + Hash01(Sd, i, 3));
					Disc(T, X, Y, R, C, 0.6f, R * 0.5f);
					for (int32 y = (int32)(Y - R); y <= (int32)(Y + R); ++y) for (int32 x = (int32)(X - R); x <= (int32)(X + R); ++x)
						if (FMath::Square(x - X) + FMath::Square(y - Y) < R * R) T.E[T.I(x, y)] = 0.85f;
				}
			}
			if (Kind == 3 || Kind == 4) Border(T, (int32)P, Shade(A, 1.3f), 0.8f);
			T.NormalStrength = 2.f;
			T.BakeAO = 0.2f;
			break;
		}
		case EMCTexRecipe::Sandstone:
		{
			const int32 Kind = (int32)D.P0;
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const float N = FbmAniso(T.U(x), T.U(y), 2, 10, 3, Sd);
					const int32 I = T.I(x, y);
					T.C[I] = WithA(Mix(A, B, N * 0.8f), 1.f);
					T.H[I] = 0.5f + N * 0.2f;
				}
			Speckle(T, C, 0.03f, 1, Sd + 1, 0.05f);
			if (Kind == 0) { FillRect(T, 0, 0, T.S, (int32)(P * 3), Shade(C, 1.0f), 0.7f); Grunge(T, 0.06f, 0.05f, 8, Sd + 2); FillRect(T, 0, (int32)(P * 3), T.S, (int32)(P * 3.5f), Shade(B, 0.9f), 0.3f); FillRect(T, 0, (int32)(P * 12), T.S, (int32)(P * 12.5f), Shade(B, 0.9f), 0.3f); }
			if (Kind == 2) Panel(T, 0, 0, T.S, T.S, (int32)P, 0.15f, 0.12f);
			T.R.Init(0.9f, T.S * T.S);
			break;
		}
		case EMCTexRecipe::SandstoneTop:
			Mottle(T, A, B, 6, 3, 1.f, Sd, 2, 0.15f);
			Speckle(T, C, 0.04f, 1, Sd + 1, 0.03f);
			break;
		case EMCTexRecipe::Netherrack:
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const FCell W = Worley(T.U(x), T.U(y), 6, Sd, 1.f);
					const float N = Fbm(T.U(x), T.U(y), 6, 3, Sd + 1);
					const int32 I = T.I(x, y);
					FLinearColor Cl = Mix(A, B, Hash01(W.Id, 2) * 0.6f + N * 0.4f);
					float Ht = 0.6f + N * 0.2f;
					if (W.F2 - W.F1 < 0.12f) { Cl = Mix(C, Cl, (W.F2 - W.F1) / 0.12f * 0.5f); Ht = 0.25f; }
					T.C[I] = WithA(Cl, 1.f);
					T.H[I] = Ht;
					T.R[I] = 0.9f;
				}
			T.NormalStrength = 3.f;
			break;
		case EMCTexRecipe::NyliumTop:
			Mottle(T, A, B, 8, 3, 1.8f, Sd, 1, 0.4f);
			Speckle(T, Shade(B, 1.3f), 0.06f, 1, Sd + 1, 0.15f);
			Speckle(T, C, 0.05f, 1, Sd + 2, -0.1f);
			T.NormalStrength = 3.f;
			break;
		case EMCTexRecipe::WartBlock:
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const FCell W = Worley(T.U(x), T.U(y), 7, Sd, 0.9f);
					const int32 I = T.I(x, y);
					const float Dome = FMath::Clamp(1.f - W.F1 * 1.4f, 0.f, 1.f);
					T.C[I] = WithA(Mix(C, Mix(A, B, Hash01(W.Id, 3)), 0.35f + Dome * 0.65f), 1.f);
					T.H[I] = Dome;
					T.R[I] = 0.7f;
				}
			T.NormalStrength = 3.f;
			break;
		case EMCTexRecipe::Glowstone:
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const FCell W = Worley(T.U(x), T.U(y), 5, Sd, 0.8f);
					const float Br = Hash01(W.Id, 4);
					const int32 I = T.I(x, y);
					const bool bGap = W.F2 - W.F1 < 0.1f;
					T.C[I] = WithA(bGap ? C : Mix(A, B, Br), 1.f);
					T.E[I] = bGap ? 0.1f : 0.5f + Br * 0.5f;
					T.H[I] = bGap ? 0.2f : 0.5f + (1.f - W.F1) * 0.4f;
					T.R[I] = 0.5f;
				}
			T.BakeAO = 0.1f;
			break;
		case EMCTexRecipe::SoulSand:
			Mottle(T, A, B, 5, 4, 1.4f, Sd, 2, 0.4f);
			if (D.P0 < 0.5f)
			{
				for (int32 i = 0; i < 5; ++i) // dark hollows with faint wisps
				{
					const float X = Hash01(Sd, i, 1) * T.S, Y = Hash01(Sd, i, 2) * T.S;
					Ellipse(T, X, Y, P * 1.8f, P * 1.2f, 0.f, C, 0.15f);
					Disc(T, X - P * 0.6f, Y - P * 0.2f, P * 0.35f, Shade(C, 0.6f), 0.1f);
					Disc(T, X + P * 0.6f, Y - P * 0.2f, P * 0.35f, Shade(C, 0.6f), 0.1f);
				}
			}
			else Speckle(T, C, 0.05f, 2, Sd + 2, -0.15f);
			break;
		case EMCTexRecipe::BasaltSide:
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const float N = FbmAniso(T.U(x), T.U(y), 12, 2, 3, Sd);
					const int32 I = T.I(x, y);
					T.C[I] = WithA(Mix(C, Mix(A, B, N), 0.4f + N * 0.6f), 1.f);
					T.H[I] = N;
					T.R[I] = D.P0 > 0.5f ? 0.55f : 0.85f;
				}
			T.NormalStrength = 3.f;
			break;
		case EMCTexRecipe::BasaltTop:
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const FCell W = Worley(T.U(x), T.U(y), 4, Sd, 0.7f);
					const int32 I = T.I(x, y);
					T.C[I] = WithA(W.F2 - W.F1 < 0.07f ? C : Mix(A, B, Hash01(W.Id, 1)), 1.f);
					T.H[I] = W.F2 - W.F1 < 0.07f ? 0.2f : 0.6f;
				}
			if (D.P0 > 0.5f) Border(T, (int32)P, Shade(A, 0.8f), 0.4f);
			break;
		case EMCTexRecipe::Magma:
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const FCell W = Worley(T.U(x), T.U(y), 5, Sd, 0.8f);
					const float Crack = W.F2 - W.F1;
					const int32 I = T.I(x, y);
					if (Crack < 0.12f) { const float G = 1.f - Crack / 0.12f; T.C[I] = WithA(Mix(A, B, G), 1.f); T.E[I] = G; T.H[I] = 0.2f; }
					else { T.C[I] = WithA(Mix(C, A, Hash01(W.Id, 2) * 0.5f), 1.f); T.H[I] = 0.7f; T.E[I] = 0.05f; }
					T.R[I] = 0.8f;
				}
			T.BakeAO = 0.1f;
			break;
		case EMCTexRecipe::EndStone:
		{
			const int32 Kind = (int32)D.P0;
			Mottle(T, A, B, 5, 4, 1.3f, Sd, 2, 0.3f);
			for (int32 i = 0; i < 16; ++i) Disc(T, Hash01(Sd, i, 1) * T.S, Hash01(Sd, i, 2) * T.S, P * (0.4f + Hash01(Sd, i, 3) * 0.6f), Shade(B, 0.85f), 0.2f);
			Speckle(T, C, 0.03f, 1, Sd + 1, 0.05f);
			if (Kind == 2) { FillRect(T, (int32)(P * 2), (int32)(P * 2), (int32)(P * 14), (int32)(P * 14), Mix(A, B, 0.5f), 0.6f); Grunge(T, 0.1f, 0.05f, 6, Sd + 4); Border(T, (int32)(P * 2), C, 0.7f); }
			if (Kind == 3) { FillRect(T, 0, 0, T.S, (int32)(P * 3), Col(D.C2), 0.7f); }
			break;
		}
		case EMCTexRecipe::Water: WaterTex(T, D); break;
		case EMCTexRecipe::Lava: LavaTex(T, D); break;
		case EMCTexRecipe::Portal: PortalTex(T, D); break;
		case EMCTexRecipe::Sulfur: SulfurTex(T, D, false); break;
		case EMCTexRecipe::Cinnabar: SulfurTex(T, D, true); break;
		case EMCTexRecipe::LogSide:
		{
			const int32 Kind = (int32)D.P0;
			Bark(T, A, B, Sd, (int32)D.P1);
			if (Kind == 1) // mangrove roots: cutout lattice of roots
			{
				T.SetAlpha(0.f);
				for (int32 i = 0; i < 9; ++i) Blade(T, Hash01(Sd, i, 1) * T.S, T.S + 2.f, T.S * 1.3f, PI * 0.5f + (Hash01(Sd, i, 2) - 0.5f) * 1.2f, (Hash01(Sd, i, 3) - 0.5f) * 1.5f, P * 1.4f, A, B);
			}
			if (Kind == 2 || Kind == 3) { FillRect(T, (int32)(P * 4), (int32)(P * 3), (int32)(P * 12), (int32)(P * 13), Shade(A, 0.6f), 0.3f); Disc(T, T.S * 0.5f, T.S * 0.5f, P * 2.5f, C, 0.6f, P); if (Kind == 3) for (int32 k = 0; k < T.S * T.S; ++k) if (T.C[k].R > 0.7f) T.E[k] = 0.9f; }
			if (Kind >= 3 && D.Recipe == EMCTexRecipe::LogSide && (int32)D.P1 == 0 && Kind >= 3) { /* campfire logs */ FillRect(T, 0, 0, T.S, (int32)P, Shade(A, 0.6f)); FillRect(T, 0, T.S - (int32)P, T.S, T.S, Shade(A, 0.6f)); if (Kind == 4) for (int32 x = 0; x < T.S; ++x) if (Hash01(x, Sd) < 0.3f) { T.C[T.I(x, T.S / 2)] = WithA(C, 1.f); T.E[T.I(x, T.S / 2)] = 1.f; } }
			break;
		}
		case EMCTexRecipe::LogTop: Rings(T, A, B, C, Sd, 1.f, false); if (D.Flags & MCTF_Emissive) for (int32 k = 0; k < T.S * T.S; ++k) T.E[k] = 0.1f; break;
		case EMCTexRecipe::StrippedSide:
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const float G = FbmAniso(T.U(x), T.U(y), 14, 2, 3, Sd);
					const int32 I = T.I(x, y);
					T.C[I] = WithA(Mix(A, B, FMath::Clamp((G - 0.35f) * 1.8f, 0.f, 1.f)), 1.f);
					T.H[I] = 0.5f + G * 0.2f;
					T.R[I] = 0.7f;
				}
			break;
		case EMCTexRecipe::StrippedTop: Rings(T, A, B, C, Sd, 1.f, true); break;
		case EMCTexRecipe::Leaves: LeavesTex(T, D); break;
		case EMCTexRecipe::Bamboo:
		{
			const bool bTop = D.P0 > 0.5f;
			if (!bTop)
			{
				for (int32 y = 0; y < T.S; ++y)
					for (int32 x = 0; x < T.S; ++x)
					{
						const int32 Stalk = x / (T.S / 4);
						const int32 LX = x % (T.S / 4);
						const int32 I = T.I(x, y);
						const float Edge = FMath::Min(LX, T.S / 4 - 1 - LX) / (T.S / 8.f);
						FLinearColor Cl = Shade(Mix(B, A, Edge), 0.85f + Hash01(Stalk, Sd) * 0.2f);
						if ((y + Stalk * 7) % (T.S / 2) < 2) Cl = Shade(Cl, 0.7f);
						T.C[I] = WithA(Cl, 1.f);
						T.H[I] = 0.3f + Edge * 0.6f;
						T.R[I] = 0.5f;
					}
			}
			else
			{
				T.Fill(C, 0.5f, 0.6f);
				for (int32 k = 0; k < 4; ++k) for (int32 j = 0; j < 4; ++j) { Disc(T, (k + 0.5f) * T.S / 4, (j + 0.5f) * T.S / 4, T.S / 9.f, A, 0.7f); Disc(T, (k + 0.5f) * T.S / 4, (j + 0.5f) * T.S / 4, T.S / 16.f, Shade(C, 0.7f), 0.3f); }
			}
			break;
		}
		default:
			RunBuiltRecipe(T, D);
			break;
		}
	}
}
