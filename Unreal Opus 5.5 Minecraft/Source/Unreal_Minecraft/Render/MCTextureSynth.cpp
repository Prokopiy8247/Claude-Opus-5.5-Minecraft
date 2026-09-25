// Procedural texture synthesizer core: canvas, tileable noise, raster primitives, finalisation into
// albedo / normal / ORME texels, crack stages, texture-array creation and the on-disk cache.
#include "Render/MCTextureSynth.h"
#include "Render/MCTexSynthInternal.h"
#include "Blocks/MCTextures.h"
#include "Core/MCCore.h"
#include "Engine/Texture2DArray.h"
#include "TextureResource.h"
#include "Async/ParallelFor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

namespace
{
	constexpr uint32 SynthVersion = 8;
	TArray<FColor> GAlbedoCopy;
}

namespace MCTS
{
	FTex::FTex(int32 InS) : S(InS)
	{
		const int32 N = S * S;
		C.Init(FLinearColor(0.5f, 0.5f, 0.5f, 1.f), N);
		H.Init(0.5f, N);
		R.Init(0.85f, N);
		M.Init(0.f, N);
		E.Init(0.f, N);
	}

	void FTex::Fill(const FLinearColor& Col, float Height, float Rough)
	{
		for (FLinearColor& X : C) X = Col;
		for (float& X : H) X = Height;
		for (float& X : R) X = Rough;
	}

	void FTex::SetAlpha(float A) { for (FLinearColor& X : C) X.A = A; }

	FLinearColor Col(const FColor& C) { return FLinearColor(C.R / 255.f, C.G / 255.f, C.B / 255.f, 1.f); }

	FLinearColor Gray(const FLinearColor& A)
	{
		const float L = A.R * 0.3f + A.G * 0.59f + A.B * 0.11f;
		return FLinearColor(L, L, L, A.A);
	}

	FLinearColor Jitter(const FLinearColor& A, float Amount, uint32 Seed)
	{
		const float V = (Hash01(Seed, 11) - 0.5f) * 2.f * Amount;
		const float W = (Hash01(Seed, 23) - 0.5f) * Amount * 0.6f;
		return FLinearColor(FMath::Clamp(A.R * (1.f + V) + W * 0.1f, 0.f, 1.f), FMath::Clamp(A.G * (1.f + V), 0.f, 1.f), FMath::Clamp(A.B * (1.f + V) - W * 0.1f, 0.f, 1.f), A.A);
	}

	float VNoise(float X, float Y, int32 Period, uint32 Seed)
	{
		const int32 P = FMath::Max(1, Period);
		const float FX = FMath::FloorToFloat(X), FY = FMath::FloorToFloat(Y);
		const float TX = X - FX, TY = Y - FY;
		const int32 IX = (int32)FX, IY = (int32)FY;
		auto L = [&](int32 A, int32 B) { A %= P; if (A < 0) A += P; B %= P; if (B < 0) B += P; return Hash01((uint32)A, (uint32)B, Seed); };
		const float SX = TX * TX * (3.f - 2.f * TX), SY = TY * TY * (3.f - 2.f * TY);
		const float A = FMath::Lerp(L(IX, IY), L(IX + 1, IY), SX);
		const float B = FMath::Lerp(L(IX, IY + 1), L(IX + 1, IY + 1), SX);
		return FMath::Lerp(A, B, SY);
	}

	float Fbm(float U, float V, int32 Cells, int32 Octaves, uint32 Seed, float Gain)
	{
		float Sum = 0.f, Amp = 1.f, Norm = 0.f;
		int32 C = FMath::Max(1, Cells);
		for (int32 o = 0; o < Octaves; ++o)
		{
			Sum += VNoise(U * C, V * C, C, Seed + o * 1013) * Amp;
			Norm += Amp;
			Amp *= Gain;
			C *= 2;
		}
		return Sum / Norm;
	}

	float FbmAniso(float U, float V, int32 CellsX, int32 CellsY, int32 Octaves, uint32 Seed)
	{
		// separate periods per axis: sample a lattice with CellsX x CellsY cells
		float Sum = 0.f, Amp = 1.f, Norm = 0.f;
		int32 CX = FMath::Max(1, CellsX), CY = FMath::Max(1, CellsY);
		for (int32 o = 0; o < Octaves; ++o)
		{
			const float X = U * CX, Y = V * CY;
			const float FX = FMath::FloorToFloat(X), FY = FMath::FloorToFloat(Y);
			const float TX = X - FX, TY = Y - FY;
			const int32 IX = (int32)FX, IY = (int32)FY;
			auto L = [&](int32 A, int32 B) { A %= CX; if (A < 0) A += CX; B %= CY; if (B < 0) B += CY; return Hash01((uint32)A, (uint32)B, Seed + o * 7919); };
			const float SX = TX * TX * (3.f - 2.f * TX), SY = TY * TY * (3.f - 2.f * TY);
			Sum += FMath::Lerp(FMath::Lerp(L(IX, IY), L(IX + 1, IY), SX), FMath::Lerp(L(IX, IY + 1), L(IX + 1, IY + 1), SX), SY) * Amp;
			Norm += Amp;
			Amp *= 0.5f;
			CX *= 2; CY *= 2;
		}
		return Sum / Norm;
	}

	FCell Worley(float U, float V, int32 Cells, uint32 Seed, float Jit)
	{
		FCell R;
		const float X = U * Cells, Y = V * Cells;
		const int32 IX = FMath::FloorToInt(X), IY = FMath::FloorToInt(Y);
		for (int32 dy = -2; dy <= 2; ++dy)
			for (int32 dx = -2; dx <= 2; ++dx)
			{
				const int32 CX = IX + dx, CY = IY + dy;
				int32 WX = CX % Cells; if (WX < 0) WX += Cells;
				int32 WY = CY % Cells; if (WY < 0) WY += Cells;
				const float PX = CX + 0.5f + (Hash01(WX, WY, Seed) - 0.5f) * Jit;
				const float PY = CY + 0.5f + (Hash01(WX, WY, Seed + 1) - 0.5f) * Jit;
				const float D = FMath::Sqrt(FMath::Square(PX - X) + FMath::Square(PY - Y));
				if (D < R.F1) { R.F2 = R.F1; R.F1 = D; R.Id = Hash(WX, WY, Seed + 2); R.Center = FVector2f(PX / Cells, PY / Cells); }
				else if (D < R.F2) R.F2 = D;
			}
		return R;
	}

	void Disc(FTex& T, float CX, float CY, float Rad, const FLinearColor& Col, float Height, float Soft)
	{
		const int32 R0 = FMath::CeilToInt(Rad + 1);
		for (int32 y = FMath::FloorToInt(CY) - R0; y <= FMath::FloorToInt(CY) + R0; ++y)
			for (int32 x = FMath::FloorToInt(CX) - R0; x <= FMath::FloorToInt(CX) + R0; ++x)
			{
				const float D = FMath::Sqrt(FMath::Square(x + 0.5f - CX) + FMath::Square(y + 0.5f - CY));
				if (D > Rad) continue;
				const int32 I = T.I(x, y);
				const float W = Soft > 0.f ? FMath::Clamp((Rad - D) / Soft, 0.f, 1.f) : 1.f;
				T.C[I] = Mix(T.C[I], WithA(Col, FMath::Max(T.C[I].A, Col.A)), W * Col.A);
				T.C[I].A = FMath::Max(T.C[I].A, Col.A * W);
				if (Height >= 0.f) T.H[I] = FMath::Lerp(T.H[I], Height * (1.f - 0.3f * D / FMath::Max(Rad, 0.01f)), W);
			}
	}

	void Ellipse(FTex& T, float CX, float CY, float RX, float RY, float Ang, const FLinearColor& Col, float Height)
	{
		const float CA = FMath::Cos(Ang), SA = FMath::Sin(Ang);
		const int32 R0 = FMath::CeilToInt(FMath::Max(RX, RY) + 1);
		for (int32 y = FMath::FloorToInt(CY) - R0; y <= FMath::FloorToInt(CY) + R0; ++y)
			for (int32 x = FMath::FloorToInt(CX) - R0; x <= FMath::FloorToInt(CX) + R0; ++x)
			{
				const float DX = x + 0.5f - CX, DY = y + 0.5f - CY;
				const float LX = DX * CA + DY * SA, LY = -DX * SA + DY * CA;
				const float Q = FMath::Square(LX / FMath::Max(RX, 0.01f)) + FMath::Square(LY / FMath::Max(RY, 0.01f));
				if (Q > 1.f) continue;
				const int32 I = T.I(x, y);
				T.C[I] = WithA(Mix(T.C[I], Col, Col.A), FMath::Max(T.C[I].A, Col.A));
				if (Height >= 0.f) T.H[I] = Height * (1.f - 0.35f * Q);
			}
	}

	void Line(FTex& T, float X0, float Y0, float X1, float Y1, float Width, const FLinearColor& Col, float Height)
	{
		const float Len = FMath::Sqrt(FMath::Square(X1 - X0) + FMath::Square(Y1 - Y0));
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Len * 2.f));
		for (int32 i = 0; i <= Steps; ++i)
		{
			const float A = (float)i / Steps;
			const float X = FMath::Lerp(X0, X1, A), Y = FMath::Lerp(Y0, Y1, A);
			const float R = Width * 0.5f;
			for (int32 y = FMath::FloorToInt(Y - R); y <= FMath::FloorToInt(Y + R); ++y)
				for (int32 x = FMath::FloorToInt(X - R); x <= FMath::FloorToInt(X + R); ++x)
				{
					if (FMath::Square(x + 0.5f - X) + FMath::Square(y + 0.5f - Y) > R * R + 0.25f) continue;
					const int32 I = T.I(x, y);
					T.C[I] = WithA(Mix(T.C[I], Col, Col.A), FMath::Max(T.C[I].A, Col.A));
					if (Height >= 0.f) T.H[I] = Height;
				}
		}
	}

	void FillRect(FTex& T, int32 X0, int32 Y0, int32 X1, int32 Y1, const FLinearColor& Col, float Height)
	{
		for (int32 y = Y0; y < Y1; ++y)
			for (int32 x = X0; x < X1; ++x)
			{
				const int32 I = T.I(x, y);
				T.C[I] = Col;
				if (Height >= 0.f) T.H[I] = Height;
			}
	}

	void RectBlend(FTex& T, int32 X0, int32 Y0, int32 X1, int32 Y1, const FLinearColor& Col, float Amount)
	{
		for (int32 y = Y0; y < Y1; ++y)
			for (int32 x = X0; x < X1; ++x)
			{
				const int32 I = T.I(x, y);
				T.C[I] = WithA(Mix(T.C[I], Col, Amount), T.C[I].A);
			}
	}

	void Border(FTex& T, int32 W, const FLinearColor& Col, float Height)
	{
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				if (x >= W && y >= W && x < T.S - W && y < T.S - W) continue;
				const int32 I = T.I(x, y);
				T.C[I] = WithA(Col, T.C[I].A);
				if (Height >= 0.f) T.H[I] = Height;
			}
	}

	void Panel(FTex& T, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 Bevel, float Lift, float EdgeShade)
	{
		for (int32 y = Y0; y < Y1; ++y)
			for (int32 x = X0; x < X1; ++x)
			{
				const int32 I = T.I(x, y);
				const int32 DL = x - X0, DR = X1 - 1 - x, DT = y - Y0, DB = Y1 - 1 - y;
				const int32 DMin = FMath::Min(FMath::Min(DL, DR), FMath::Min(DT, DB));
				const float E = Bevel > 0 ? FMath::Clamp((float)DMin / Bevel, 0.f, 1.f) : 1.f;
				T.H[I] = FMath::Clamp(T.H[I] + Lift * E, 0.f, 1.f);
				if (DMin < Bevel)
				{
					const bool bLight = (DL == DMin || DT == DMin);
					T.C[I] = WithA(Shade(T.C[I], bLight ? 1.f + EdgeShade : 1.f - EdgeShade), T.C[I].A);
				}
			}
	}

	void Blade(FTex& T, float BX, float BY, float Length, float Ang, float Bend, float Width, const FLinearColor& Base, const FLinearColor& Tip)
	{
		const int32 Steps = FMath::Max(4, FMath::CeilToInt(Length * 2.f));
		float X = BX, Y = BY, A = Ang;
		for (int32 i = 0; i <= Steps; ++i)
		{
			const float P = (float)i / Steps;
			const float W = FMath::Max(0.6f, Width * (1.f - P * 0.85f));
			const FLinearColor C = Mix(Base, Tip, P);
			for (int32 y = FMath::FloorToInt(Y - W); y <= FMath::FloorToInt(Y + W); ++y)
				for (int32 x = FMath::FloorToInt(X - W); x <= FMath::FloorToInt(X + W); ++x)
				{
					if (FMath::Square(x + 0.5f - X) + FMath::Square(y + 0.5f - Y) > W * W * 0.36f + 0.2f) continue;
					const int32 I = T.I(x, y);
					if (x < 0 || x >= T.S || y < 0 || y >= T.S) continue; // plants never wrap
					T.C[I] = WithA(C, 1.f);
					T.H[I] = 0.5f + 0.3f * (1.f - P);
				}
			const float StepLen = Length / Steps;
			X += FMath::Cos(A) * StepLen;
			Y -= FMath::Sin(A) * StepLen;
			A += Bend / Steps;
		}
	}

	void MulColor(FTex& T, float F) { for (FLinearColor& X : T.C) X = WithA(Shade(X, F), X.A); }

	void Grunge(FTex& T, float ColorAmount, float HeightAmount, int32 Cells, uint32 Seed)
	{
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const float N = Fbm(T.U(x), T.U(y), Cells, 4, Seed) - 0.5f;
				const int32 I = T.I(x, y);
				T.C[I] = WithA(Shade(T.C[I], 1.f + N * ColorAmount * 2.f), T.C[I].A);
				T.H[I] = FMath::Clamp(T.H[I] + N * HeightAmount, 0.f, 1.f);
			}
	}

	void Speckle(FTex& T, const FLinearColor& Col, float Density, int32 Size, uint32 Seed, float HeightDelta)
	{
		const int32 Count = FMath::Max(1, (int32)(T.S * T.S * Density / FMath::Max(1, Size * Size)));
		for (int32 i = 0; i < Count; ++i)
		{
			const int32 X = (int32)(Hash01(Seed, i, 1) * T.S), Y = (int32)(Hash01(Seed, i, 2) * T.S);
			const float V = 0.85f + Hash01(Seed, i, 3) * 0.3f;
			const int32 Sz = FMath::Max(1, Size - (Hash01(Seed, i, 4) < 0.4f ? 1 : 0));
			for (int32 y = 0; y < Sz; ++y)
				for (int32 x = 0; x < Sz; ++x)
				{
					const int32 I = T.I(X + x, Y + y);
					T.C[I] = WithA(Shade(Col, V), T.C[I].A);
					T.H[I] = FMath::Clamp(T.H[I] + HeightDelta, 0.f, 1.f);
				}
		}
	}

	void Stones(FTex& T, int32 Cells, const FLinearColor& A, const FLinearColor& B, const FLinearColor& Gap, float GapWidth, uint32 Seed, float Jit)
	{
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const FCell W = Worley(T.U(x), T.U(y), Cells, Seed, Jit);
				const float Edge = W.F2 - W.F1;
				const int32 I = T.I(x, y);
				const float Shade01 = Hash01(W.Id, 5);
				FLinearColor Stone = Mix(A, B, Shade01);
				const float N = Fbm(T.U(x), T.U(y), 8, 3, Seed + 77) - 0.5f;
				Stone = Shade(Stone, 1.f + N * 0.25f);
				if (Edge < GapWidth)
				{
					const float G = FMath::Clamp(Edge / GapWidth, 0.f, 1.f);
					T.C[I] = WithA(Mix(Gap, Stone, G * G * 0.6f), T.C[I].A);
					T.H[I] = 0.2f + G * 0.25f;
				}
				else
				{
					const float Dome = FMath::Clamp(1.f - W.F1 * 0.9f, 0.f, 1.f);
					// light from the top-left: brighten the upper-left part of each stone
					const FVector2f Rel = FVector2f(T.U(x), T.U(y)) - W.Center;
					const float Lit = FMath::Clamp(-(Rel.X + Rel.Y) * Cells * 0.8f, -0.5f, 0.5f);
					T.C[I] = WithA(Shade(Stone, 1.f + Lit * 0.28f), T.C[I].A);
					T.H[I] = 0.45f + Dome * 0.45f + N * 0.1f;
				}
			}
	}

	void Bricks(FTex& T, int32 Rows, int32 Cols, int32 Mortar, const FLinearColor& Brick, const FLinearColor& MortarCol, float Variation, uint32 Seed, bool bOffset)
	{
		const int32 RowH = T.S / Rows, ColW = T.S / Cols;
		for (int32 y = 0; y < T.S; ++y)
		{
			const int32 Row = y / RowH;
			const int32 Off = (bOffset && (Row & 1)) ? ColW / 2 : 0;
			for (int32 x = 0; x < T.S; ++x)
			{
				const int32 XX = (x + Off) % T.S;
				const int32 Col_ = XX / ColW;
				const int32 LX = XX % ColW, LY = y % RowH;
				const int32 I = T.I(x, y);
				const bool bMortar = LX < Mortar || LY < Mortar;
				if (bMortar)
				{
					const float N = Fbm(T.U(x), T.U(y), 16, 2, Seed + 3) - 0.5f;
					T.C[I] = WithA(Shade(MortarCol, 1.f + N * 0.3f), T.C[I].A);
					T.H[I] = 0.25f;
				}
				else
				{
					const uint32 BId = Hash(Row, Col_, Seed);
					FLinearColor C = Jitter(Brick, Variation, BId);
					const float N = Fbm(T.U(x), T.U(y), 12, 3, Seed + 9) - 0.5f;
					C = Shade(C, 1.f + N * 0.3f);
					// bevel shading
					const int32 DL = LX - Mortar, DT = LY - Mortar, DR = ColW - 1 - LX, DB = RowH - 1 - LY;
					if (DL == 0 || DT == 0) C = Shade(C, 1.12f);
					if (DR == 0 || DB == 0) C = Shade(C, 0.84f);
					T.C[I] = WithA(C, T.C[I].A);
					T.H[I] = 0.7f + N * 0.15f - ((DL == 0 || DT == 0 || DR == 0 || DB == 0) ? 0.1f : 0.f);
				}
			}
		}
	}

	void Planks(FTex& T, int32 Boards, const FLinearColor& Base, const FLinearColor& Grain, const FLinearColor& Gap, uint32 Seed, bool bVertical)
	{
		const int32 BH = T.S / Boards;
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const int32 A = bVertical ? x : y, B = bVertical ? y : x;
				const int32 Board = A / BH, LA = A % BH;
				const int32 Seam = (int32)(Hash01(Board, Seed, 3) * T.S);
				const bool bSeam = (Board & 1) ? (FMath::Abs(B - Seam) < 1) : false;
				const float U = (float)B / T.S, V = (float)A / T.S;
				const float G = FbmAniso(bVertical ? V : U, bVertical ? U : V, 2, 24, 3, Seed + Board * 31);
				const float Lines = FMath::Abs(FMath::Frac(G * 5.f + Hash01(Board, Seed) * 3.f) - 0.5f) * 2.f;
				FLinearColor C = Mix(Base, Grain, FMath::Clamp((1.f - Lines) * 0.8f - 0.15f, 0.f, 1.f));
				C = Jitter(C, 0.05f, Hash(Board, Seed, 17));
				const int32 I = T.I(x, y);
				float H = 0.62f + G * 0.2f;
				if (LA == 0 || bSeam) { C = Gap; H = 0.2f; }
				else if (LA == 1) C = Shade(C, 1.1f);
				else if (LA == BH - 1) C = Shade(C, 0.85f);
				T.C[I] = WithA(C, T.C[I].A);
				T.H[I] = H;
				T.R[I] = 0.72f + G * 0.15f;
			}
	}

	void Ore(FTex& T, const FLinearColor& A, const FLinearColor& B, const FLinearColor& Dark, int32 Style, bool bEmissive, bool bMetal, uint32 Seed)
	{
		// clusters of 2-5 blobs placed on a jittered grid so ores read at a distance
		const int32 Clusters = Style == 0 ? 7 : (Style == 4 ? 6 : 5);
		const float P = T.P16();
		for (int32 c = 0; c < Clusters; ++c)
		{
			const float CX = Hash01(Seed, c, 1) * T.S, CY = Hash01(Seed, c, 2) * T.S;
			const int32 Blobs = 2 + (int32)(Hash01(Seed, c, 3) * 3.f);
			for (int32 b = 0; b < Blobs; ++b)
			{
				const float BX = CX + (Hash01(Seed, c, b, 4) - 0.5f) * 3.5f * P;
				const float BY = CY + (Hash01(Seed, c, b, 5) - 0.5f) * 3.5f * P;
				float R = (0.7f + Hash01(Seed, c, b, 6) * 0.7f) * P;
				if (Style == 2) R *= 0.9f;
				if (Style == 5) { Line(T, BX - R, BY + R, BX + R, BY - R, R * 0.8f, Dark, 0.8f); Line(T, BX - R * 0.8f, BY + R * 0.6f, BX + R * 0.6f, BY - R * 0.8f, R * 0.45f, A, 0.9f); continue; }
				Disc(T, BX + 0.6f, BY + 0.6f, R + 0.6f, Dark, 0.75f);
				Disc(T, BX, BY, R, A, 0.85f);
				// highlight texel(s) at the top-left
				Disc(T, BX - R * 0.35f, BY - R * 0.35f, FMath::Max(0.8f, R * 0.4f), B, 0.95f);
				if (bMetal || bEmissive || Style == 2)
				{
					for (int32 y = FMath::FloorToInt(BY - R - 1); y <= FMath::FloorToInt(BY + R + 1); ++y)
						for (int32 x = FMath::FloorToInt(BX - R - 1); x <= FMath::FloorToInt(BX + R + 1); ++x)
						{
							if (FMath::Square(x + 0.5f - BX) + FMath::Square(y + 0.5f - BY) > R * R) continue;
							const int32 I = T.I(x, y);
							if (bMetal) { T.M[I] = 0.85f; T.R[I] = 0.35f; }
							if (Style == 2) T.R[I] = 0.18f;
							if (bEmissive) T.E[I] = 0.9f;
						}
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Finalisation & public API

namespace
{
	using namespace MCTS;

	void Finalize(const FMCTexDef& D, FTex& T, FColor* OutA, FColor* OutN, FColor* OutO)
	{
		const int32 S = T.S;
		const bool bCut = (D.Flags & (MCTF_Cutout | MCTF_Translucent)) != 0;
		const bool bTint = (D.Flags & MCTF_Tinted) != 0;
		// cavity AO from a blurred height field
		TArray<float> Blur; Blur.SetNumUninitialized(S * S);
		for (int32 y = 0; y < S; ++y)
			for (int32 x = 0; x < S; ++x)
			{
				float Sum = 0.f;
				for (int32 dy = -2; dy <= 2; ++dy) for (int32 dx = -2; dx <= 2; ++dx) Sum += T.H[T.I(x + dx, y + dy)];
				Blur[y * S + x] = Sum / 25.f;
			}
		// dilate colours into transparent texels (prevents dark fringes when filtering cutouts)
		if (bCut)
		{
			for (int32 Pass = 0; Pass < 3; ++Pass)
			{
				TArray<FLinearColor> Copy = T.C;
				for (int32 y = 0; y < S; ++y)
					for (int32 x = 0; x < S; ++x)
					{
						const int32 I = y * S + x;
						if (Copy[I].A >= 0.5f) continue;
						FLinearColor Sum(0, 0, 0, 0); int32 N = 0;
						for (int32 dy = -1; dy <= 1; ++dy) for (int32 dx = -1; dx <= 1; ++dx)
						{
							const int32 xx = x + dx, yy = y + dy;
							if (xx < 0 || yy < 0 || xx >= S || yy >= S) continue;
							const FLinearColor& Q = Copy[yy * S + xx];
							if (Q.A >= 0.5f) { Sum += Q; ++N; }
						}
						if (N > 0) T.C[I] = FLinearColor(Sum.R / N, Sum.G / N, Sum.B / N, Copy[I].A);
					}
			}
		}
		for (int32 y = 0; y < S; ++y)
			for (int32 x = 0; x < S; ++x)
			{
				const int32 I = y * S + x;
				const float Cav = FMath::Clamp((Blur[I] - T.H[I]) * 3.5f, 0.f, 1.f);
				const float AO = 1.f - Cav * 0.75f;
				FLinearColor C = T.C[I];
				const float Bake = 1.f - Cav * T.BakeAO;
				float Alpha = bCut ? C.A : (bTint ? C.A : 0.f);
				if (!bCut && !bTint) Alpha = 0.f;
				OutA[I] = FColor((uint8)FMath::Clamp(C.R * Bake * 255.f + 0.5f, 0.f, 255.f), (uint8)FMath::Clamp(C.G * Bake * 255.f + 0.5f, 0.f, 255.f),
					(uint8)FMath::Clamp(C.B * Bake * 255.f + 0.5f, 0.f, 255.f), (uint8)FMath::Clamp(Alpha * 255.f + 0.5f, 0.f, 255.f));
				// Sobel normal (tangent space, +V = down the image)
				const float HL = T.H[T.I(x - 1, y)], HR = T.H[T.I(x + 1, y)], HU = T.H[T.I(x, y - 1)], HD = T.H[T.I(x, y + 1)];
				const float HUL = T.H[T.I(x - 1, y - 1)], HUR = T.H[T.I(x + 1, y - 1)], HDL = T.H[T.I(x - 1, y + 1)], HDR = T.H[T.I(x + 1, y + 1)];
				const float DX = ((HUR + 2 * HR + HDR) - (HUL + 2 * HL + HDL)) * 0.25f;
				const float DY = ((HDL + 2 * HD + HDR) - (HUL + 2 * HU + HUR)) * 0.25f;
				FVector3f N(-DX * T.NormalStrength, -DY * T.NormalStrength, 1.f);
				N.Normalize();
				OutN[I] = FColor((uint8)FMath::Clamp((N.X * 0.5f + 0.5f) * 255.f, 0.f, 255.f), (uint8)FMath::Clamp((N.Y * 0.5f + 0.5f) * 255.f, 0.f, 255.f),
					(uint8)FMath::Clamp((N.Z * 0.5f + 0.5f) * 255.f, 0.f, 255.f), 255);
				const float Em = (D.Flags & MCTF_Emissive) ? T.E[I] : 0.f;
				const float Metal = (D.Flags & MCTF_Metal) ? T.M[I] : 0.f;
				OutO[I] = FColor((uint8)(FMath::Clamp(AO, 0.f, 1.f) * 255.f), (uint8)(FMath::Clamp(T.R[I], 0.02f, 1.f) * 255.f), (uint8)(FMath::Clamp(Metal, 0.f, 1.f) * 255.f), (uint8)(FMath::Clamp(Em, 0.f, 1.f) * 255.f));
			}
	}

	/** Destroy stage overlay: dark cracks radiating from the centre, more per stage. */
	void GenerateCrack(int32 Stage, int32 S, FColor* OutA, FColor* OutN, FColor* OutO)
	{
		FTex T(S);
		T.Fill(FLinearColor(0.08f, 0.08f, 0.08f, 0.f), 0.5f, 0.9f);
		const int32 Cracks = 2 + Stage * 2;
		for (int32 c = 0; c < Cracks; ++c)
		{
			float X = S * 0.5f + (Hash01(c, 1, 99) - 0.5f) * S * 0.3f, Y = S * 0.5f + (Hash01(c, 2, 99) - 0.5f) * S * 0.3f;
			float A = Hash01(c, 3, 99) * 2.f * PI;
			const int32 Len = (int32)(S * (0.12f + 0.05f * Stage) * (0.7f + Hash01(c, 4, 99) * 0.6f));
			for (int32 i = 0; i < Len; ++i)
			{
				const int32 I = T.I((int32)X, (int32)Y);
				T.C[I] = FLinearColor(0.05f, 0.05f, 0.05f, 0.85f);
				X += FMath::Cos(A); Y += FMath::Sin(A);
				A += (Hash01(c, i, 5) - 0.5f) * 0.9f;
				if (X < 0 || Y < 0 || X >= S || Y >= S) break;
				if (Hash01(c, i, 6) < 0.06f) { A += (Hash01(c, i, 7) < 0.5f ? 1.f : -1.f) * 0.9f; }
			}
		}
		FMCTexDef D; D.Flags = MCTF_Cutout;
		Finalize(D, T, OutA, OutN, OutO);
	}

	/** Soft particle sprites (white where tinted by the particle colour). */
	void GenerateParticle(int32 Sprite, int32 S, FColor* OutA, FColor* OutN, FColor* OutO)
	{
		using namespace MCTexSynth;
		FTex T(S);
		T.Fill(FLinearColor(1.f, 1.f, 1.f, 0.f), 0.5f, 0.9f);
		const float C = S * 0.5f, P = T.P16();
		const FLinearColor White(1.f, 1.f, 1.f, 1.f);
		auto Soft = [&](float CX, float CY, float R, const FLinearColor& Col, float Hard)
		{
			for (int32 y = 0; y < S; ++y) for (int32 x = 0; x < S; ++x)
			{
				const float D = FMath::Sqrt(FMath::Square(x + 0.5f - CX) + FMath::Square(y + 0.5f - CY)) / R;
				if (D >= 1.f) continue;
				const float A = FMath::Clamp((1.f - D) / (1.f - Hard), 0.f, 1.f) * Col.A;
				const int32 I = T.I(x, y);
				const float NA = FMath::Max(T.C[I].A, A);
				T.C[I] = FLinearColor(FMath::Lerp(T.C[I].R, Col.R, A), FMath::Lerp(T.C[I].G, Col.G, A), FMath::Lerp(T.C[I].B, Col.B, A), NA);
			}
		};
		switch (Sprite)
		{
		case PS_Smoke: case PS_Poof: case PS_Explosion:
			for (int32 k = 0; k < 7; ++k) Soft(C + (Hash01(k, Sprite, 1) - 0.5f) * S * 0.4f, C + (Hash01(k, Sprite, 2) - 0.5f) * S * 0.4f, S * (0.22f + Hash01(k, Sprite, 3) * 0.12f), FLinearColor(0.9f, 0.9f, 0.9f, Sprite == PS_Explosion ? 1.f : 0.8f), 0.35f);
			if (Sprite == PS_Explosion) Soft(C, C, S * 0.22f, FLinearColor(1.f, 0.95f, 0.8f, 1.f), 0.1f);
			break;
		case PS_Flame: case PS_SoulFlame:
			for (int32 y = 0; y < S; ++y) for (int32 x = 0; x < S; ++x)
			{
				const float U = (x + 0.5f - C) / (S * 0.3f), V = (y + 0.5f) / S;
				const float W = FMath::Sin(V * PI) * (1.f - V * 0.4f);
				if (FMath::Abs(U) > W || V < 0.08f) continue;
				const float Core = 1.f - FMath::Abs(U) / FMath::Max(W, 0.01f);
				T.C[T.I(x, y)] = Sprite == PS_Flame ? FLinearColor(1.f, 0.55f + Core * 0.4f, 0.15f + Core * 0.5f, 0.9f) : FLinearColor(0.3f + Core * 0.5f, 0.85f, 1.f, 0.9f);
			}
			break;
		case PS_Heart:
		{
			const FLinearColor R(0.9f, 0.15f, 0.2f, 1.f);
			Disc(T, C - P * 2.2f, C - P * 1.5f, P * 3.f, R);
			Disc(T, C + P * 2.2f, C - P * 1.5f, P * 3.f, R);
			for (int32 k = 0; k < (int32)(P * 7); ++k) Line(T, C - P * 5.f + k * 0.7f, C - P * 0.6f + k * 0.7f, C + P * 5.f - k * 0.7f, C - P * 0.6f + k * 0.7f, 1.2f, R);
			Disc(T, C - P * 2.6f, C - P * 2.2f, P * 0.9f, FLinearColor(1.f, 0.7f, 0.7f, 1.f));
			break;
		}
		case PS_Crit: case PS_Glint: case PS_Happy:
			for (int32 k = 0; k < 4; ++k)
			{
				const float A = k * PI * 0.5f + (Sprite == PS_Crit ? PI * 0.25f : 0.f);
				Line(T, C, C, C + FMath::Cos(A) * S * 0.45f, C + FMath::Sin(A) * S * 0.45f, P * 1.2f, Sprite == PS_Happy ? FLinearColor(0.4f, 1.f, 0.4f, 1.f) : White);
			}
			Soft(C, C, S * 0.2f, White, 0.5f);
			break;
		case PS_Spark: Soft(C, C, S * 0.3f, White, 0.55f); break;
		case PS_Bubble:
			for (int32 y = 0; y < S; ++y) for (int32 x = 0; x < S; ++x)
			{
				const float D = FMath::Sqrt(FMath::Square(x + 0.5f - C) + FMath::Square(y + 0.5f - C)) / (S * 0.42f);
				if (D > 1.f) continue;
				T.C[T.I(x, y)] = FLinearColor(0.85f, 0.95f, 1.f, D > 0.8f ? 0.95f : 0.2f);
			}
			Soft(C - P * 2.f, C - P * 2.f, P * 1.4f, White, 0.6f);
			break;
		case PS_Portal: case PS_Rune:
			Soft(C, C, S * 0.4f, FLinearColor(0.85f, 0.6f, 1.f, 0.9f), 0.2f);
			if (Sprite == PS_Rune) { Line(T, C - P * 3, C - P * 4, C + P * 3, C + P * 4, P, White); Line(T, C + P * 3, C - P * 4, C - P * 1, C + P * 1, P, White); }
			break;
		case PS_Note:
			Ellipse(T, C - P * 2.f, C + P * 3.f, P * 2.6f, P * 1.8f, -0.4f, White);
			Line(T, C + P * 0.4f, C + P * 3.f, C + P * 0.4f, C - P * 5.f, P * 0.9f, White);
			Line(T, C + P * 0.4f, C - P * 5.f, C + P * 4.f, C - P * 3.f, P * 0.9f, White);
			break;
		case PS_Drip:
			Disc(T, C, C + P * 2.f, P * 3.f, White);
			for (int32 k = 0; k < (int32)(P * 4); ++k) Line(T, C - P * 3.f + k * 0.75f, C + P * 2.f - k, C + P * 3.f - k * 0.75f, C + P * 2.f - k, 1.f, White);
			break;
		case PS_Rain: Line(T, C, S * 0.1f, C, S * 0.9f, P * 0.8f, FLinearColor(0.8f, 0.9f, 1.f, 0.8f)); break;
		case PS_Snow: for (int32 k = 0; k < 3; ++k) { const float A = k * PI / 3.f; Line(T, C - FMath::Cos(A) * S * 0.4f, C - FMath::Sin(A) * S * 0.4f, C + FMath::Cos(A) * S * 0.4f, C + FMath::Sin(A) * S * 0.4f, P * 0.9f, White); } break;
		case PS_Angry: for (int32 k = 0; k < 4; ++k) { const float A = k * PI * 0.5f + PI * 0.25f; Line(T, C + FMath::Cos(A) * P * 1.5f, C + FMath::Sin(A) * P * 1.5f, C + FMath::Cos(A) * S * 0.42f, C + FMath::Sin(A) * S * 0.42f, P * 1.6f, FLinearColor(0.35f, 0.05f, 0.05f, 1.f)); } break;
		case PS_Dust: FillRect(T, (int32)(C - P * 3), (int32)(C - P * 3), (int32)(C + P * 3), (int32)(C + P * 3), White); break;
		case PS_Sweep: for (int32 a = 0; a < 24; ++a) { const float A0 = PI * (0.15f + a / 24.f * 0.7f), A1 = PI * (0.15f + (a + 1) / 24.f * 0.7f); Line(T, C + FMath::Cos(A0) * S * 0.4f, C + S * 0.25f - FMath::Sin(A0) * S * 0.4f, C + FMath::Cos(A1) * S * 0.4f, C + S * 0.25f - FMath::Sin(A1) * S * 0.4f, P * (0.6f + 1.2f * FMath::Sin(a / 24.f * PI)), FLinearColor(1, 1, 1, 0.9f)); } break;
		case PS_Leaf: Ellipse(T, C, C, S * 0.4f, S * 0.18f, 0.6f, White); Line(T, C - P * 4.f, C - P * 3.f, C + P * 4.f, C + P * 3.f, 0.8f, FLinearColor(0.75f, 0.75f, 0.75f, 1.f)); break;
		default: Soft(C, C, S * 0.4f, White, 0.35f); break;
		}
		FMCTexDef D; D.Flags = MCTF_Translucent;
		T.BakeAO = 0.f;
		T.NormalStrength = 0.f;
		Finalize(D, T, OutA, OutN, OutO);
	}

	uint64 DefsHash()
	{
		uint64 H = 1469598103934665603ull ^ SynthVersion ^ ((uint64)MCTexSynth::LayerSize << 20);
		auto Mix64 = [&](uint64 V) { H ^= V; H *= 1099511628211ull; };
		for (const FMCTexDef& D : FMCTextures::Defs())
		{
			Mix64(MCHash::StringHash(*D.Name.ToString()));
			Mix64((uint64)D.Recipe); Mix64(D.C0.DWColor()); Mix64(D.C1.DWColor()); Mix64(D.C2.DWColor());
			Mix64((uint64)(D.P0 * 1000)); Mix64((uint64)(D.P1 * 1000)); Mix64((uint64)(D.P2 * 1000));
			Mix64(MCHash::StringHash(*D.Base.ToString())); Mix64(D.Flags); Mix64(D.Seed);
		}
		return H;
	}
}

namespace MCTS
{
	void GenerateBase(FTex& T, FName Base)
	{
		const FMCTexDef* BD = FMCTextures::FindDef(Base);
		if (!BD) { T.Fill(FLinearColor(0.45f, 0.45f, 0.45f, 1.f)); Grunge(T, 0.1f, 0.2f, 8, 1); return; }
		RunRecipe(T, *BD);
	}
}

namespace MCTexSynth
{
	int32 NumLayers() { return FMCTextures::Num() + NumCrackStages + PS_Count; }
	int32 CrackLayer(int32 Stage) { return FMCTextures::Num() + FMath::Clamp(Stage, 0, NumCrackStages - 1); }
	int32 ParticleLayer(int32 Sprite) { return FMCTextures::Num() + NumCrackStages + FMath::Clamp(Sprite, 0, (int32)PS_Count - 1); }

	void GenerateDef(const FMCTexDef& Def, int32 Size, FColor* Albedo, FColor* Normal, FColor* Orme)
	{
		MCTS::FTex T(Size);
		MCTS::RunRecipe(T, Def);
		Finalize(Def, T, Albedo, Normal, Orme);
	}

	void BuildTerrain(TArray<FColor>& Albedo, TArray<FColor>& Normal, TArray<FColor>& Orme)
	{
		const int32 S = LayerSize, N = NumLayers(), Defs = FMCTextures::Num();
		const int64 PerLayer = (int64)S * S;
		const uint64 Hash = DefsHash();
		const FString CacheFile = FPaths::ProjectSavedDir() / TEXT("Opus55Cache") / FString::Printf(TEXT("terrain_%016llx.bin"), Hash);
		Albedo.SetNumUninitialized(PerLayer * N);
		Normal.SetNumUninitialized(PerLayer * N);
		Orme.SetNumUninitialized(PerLayer * N);
		TArray<uint8> Bytes;
		if (FFileHelper::LoadFileToArray(Bytes, *CacheFile, FILEREAD_Silent) && Bytes.Num() == PerLayer * N * 4 * 3)
		{
			FMemory::Memcpy(Albedo.GetData(), Bytes.GetData(), PerLayer * N * 4);
			FMemory::Memcpy(Normal.GetData(), Bytes.GetData() + PerLayer * N * 4, PerLayer * N * 4);
			FMemory::Memcpy(Orme.GetData(), Bytes.GetData() + PerLayer * N * 8, PerLayer * N * 4);
			UE_LOG(LogOpus55, Log, TEXT("Terrain textures loaded from cache (%d layers)"), N);
		}
		else
		{
			const double T0 = FPlatformTime::Seconds();
			const TArray<FMCTexDef>& All = FMCTextures::Defs();
			ParallelFor(N, [&](int32 L)
			{
				FColor* A = Albedo.GetData() + PerLayer * L;
				FColor* Nm = Normal.GetData() + PerLayer * L;
				FColor* O = Orme.GetData() + PerLayer * L;
				if (L < Defs) GenerateDef(All[L], S, A, Nm, O);
				else if (L < Defs + NumCrackStages) GenerateCrack(L - Defs, S, A, Nm, O);
				else GenerateParticle(L - Defs - NumCrackStages, S, A, Nm, O);
			});
			UE_LOG(LogOpus55, Log, TEXT("Synthesised %d terrain texture layers (%dx%d) in %.2fs"), N, S, S, FPlatformTime::Seconds() - T0);
			Bytes.SetNumUninitialized(PerLayer * N * 12);
			FMemory::Memcpy(Bytes.GetData(), Albedo.GetData(), PerLayer * N * 4);
			FMemory::Memcpy(Bytes.GetData() + PerLayer * N * 4, Normal.GetData(), PerLayer * N * 4);
			FMemory::Memcpy(Bytes.GetData() + PerLayer * N * 8, Orme.GetData(), PerLayer * N * 4);
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(CacheFile), true);
			FFileHelper::SaveArrayToFile(Bytes, *CacheFile);
		}
		GAlbedoCopy = Albedo;
	}

	const FColor* GetAlbedoLayer(int32 Layer)
	{
		const int64 PerLayer = (int64)LayerSize * LayerSize;
		if (Layer < 0 || (Layer + 1) * PerLayer > GAlbedoCopy.Num()) return nullptr;
		return GAlbedoCopy.GetData() + Layer * PerLayer;
	}

	UTexture2DArray* CreateArray(UObject* Outer, int32 Size, int32 Layers, const TArray<FColor>& Texels, bool bSRGB, bool bNormalMap, bool bCutoutAware)
	{
		if (Texels.Num() < Size * Size * Layers || Layers <= 0) return nullptr;
		UTexture2DArray* Tex = UTexture2DArray::CreateTransient(Size, Size, Layers, PF_B8G8R8A8);
		if (!Tex) return nullptr;
		Tex->SRGB = bSRGB;
		Tex->Filter = TF_Default;
		Tex->LODGroup = TEXTUREGROUP_World;
		Tex->AddressX = TA_Wrap;
		Tex->AddressY = TA_Wrap;
		Tex->NeverStream = true;
		if (bNormalMap) Tex->CompressionSettings = TC_Normalmap;
		FTexturePlatformData* PD = Tex->GetPlatformData();
		{
			void* Data = PD->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(Data, Texels.GetData(), (int64)Size * Size * Layers * sizeof(FColor));
			PD->Mips[0].BulkData.Unlock();
		}
		TArray<FColor> Prev(Texels.GetData(), Size * Size * Layers);
		int32 PS = Size;
		while (PS > 1)
		{
			const int32 NS = PS / 2;
			TArray<FColor> Next; Next.SetNumUninitialized(NS * NS * Layers);
			for (int32 L = 0; L < Layers; ++L)
			{
				const FColor* Src = Prev.GetData() + (int64)PS * PS * L;
				FColor* Dst = Next.GetData() + (int64)NS * NS * L;
				for (int32 y = 0; y < NS; ++y)
					for (int32 x = 0; x < NS; ++x)
					{
						const FColor Q[4] = { Src[(y * 2) * PS + x * 2], Src[(y * 2) * PS + x * 2 + 1], Src[(y * 2 + 1) * PS + x * 2], Src[(y * 2 + 1) * PS + x * 2 + 1] };
						float R = 0, G = 0, B = 0, A = 0, AMax = 0, W = 0;
						for (const FColor& C : Q)
						{
							const float Wt = bCutoutAware ? FMath::Max(C.A / 255.f, 0.02f) : 1.f;
							R += C.R * Wt; G += C.G * Wt; B += C.B * Wt; W += Wt;
							A += C.A; AMax = FMath::Max(AMax, (float)C.A);
						}
						A *= 0.25f;
						if (bCutoutAware) A = FMath::Lerp(A, AMax, 0.35f);
						FColor Out((uint8)(R / W + 0.5f), (uint8)(G / W + 0.5f), (uint8)(B / W + 0.5f), (uint8)FMath::Clamp(A + 0.5f, 0.f, 255.f));
						if (bNormalMap)
						{
							FVector3f N(Out.R / 127.5f - 1.f, Out.G / 127.5f - 1.f, Out.B / 127.5f - 1.f);
							N = N.GetSafeNormal();
							Out = FColor((uint8)((N.X * 0.5f + 0.5f) * 255.f), (uint8)((N.Y * 0.5f + 0.5f) * 255.f), (uint8)((N.Z * 0.5f + 0.5f) * 255.f), 255);
						}
						Dst[y * NS + x] = Out;
					}
			}
			FTexture2DMipMap* Mip = new FTexture2DMipMap(NS, NS, Layers);
			PD->Mips.Add(Mip);
			Mip->BulkData.Lock(LOCK_READ_WRITE);
			void* Data = Mip->BulkData.Realloc((int64)NS * NS * Layers * sizeof(FColor));
			FMemory::Memcpy(Data, Next.GetData(), (int64)NS * NS * Layers * sizeof(FColor));
			Mip->BulkData.Unlock();
			Prev = MoveTemp(Next);
			PS = NS;
		}
		Tex->UpdateResource();
		return Tex;
	}
}
