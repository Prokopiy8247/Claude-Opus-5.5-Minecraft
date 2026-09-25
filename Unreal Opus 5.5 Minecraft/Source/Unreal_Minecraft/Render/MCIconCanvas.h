// Tiny raster canvas for 32x32 item sprites. Shapes are specified in 16-unit "design" coordinates (x2 on the canvas).
#pragma once

#include "CoreMinimal.h"

namespace MCIconDraw
{
	struct FIco
	{
		static constexpr int32 S = 32;
		FLinearColor Px[S * S];
		FIco() { for (FLinearColor& P : Px) P = FLinearColor(0, 0, 0, 0); }

		FORCEINLINE bool In(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < S && Y < S; }
		FORCEINLINE FLinearColor& At(int32 X, int32 Y) { return Px[Y * S + X]; }
		FORCEINLINE void Set(int32 X, int32 Y, const FLinearColor& C)
		{
			if (!In(X, Y) || C.A <= 0.f) return;
			FLinearColor& D = At(X, Y);
			const float A = C.A;
			D = FLinearColor(D.R + (C.R - D.R) * A, D.G + (C.G - D.G) * A, D.B + (C.B - D.B) * A, FMath::Max(D.A, A));
		}
		/** Filled rectangle in design units [X0,X1) x [Y0,Y1). */
		void FillRect(float X0, float Y0, float X1, float Y1, const FLinearColor& C)
		{
			for (int32 y = FMath::RoundToInt(Y0 * 2); y < FMath::RoundToInt(Y1 * 2); ++y)
				for (int32 x = FMath::RoundToInt(X0 * 2); x < FMath::RoundToInt(X1 * 2); ++x) Set(x, y, C);
		}
		void Disc(float CX, float CY, float R, const FLinearColor& C)
		{
			const float cx = CX * 2, cy = CY * 2, r = R * 2;
			for (int32 y = FMath::FloorToInt(cy - r); y <= FMath::CeilToInt(cy + r); ++y)
				for (int32 x = FMath::FloorToInt(cx - r); x <= FMath::CeilToInt(cx + r); ++x)
					if (FMath::Square(x + 0.5f - cx) + FMath::Square(y + 0.5f - cy) <= r * r) Set(x, y, C);
		}
		void Ellipse(float CX, float CY, float RX, float RY, float Ang, const FLinearColor& C)
		{
			const float cx = CX * 2, cy = CY * 2, rx = RX * 2, ry = RY * 2, Ca = FMath::Cos(Ang), Sa = FMath::Sin(Ang);
			const float R = FMath::Max(rx, ry);
			for (int32 y = FMath::FloorToInt(cy - R); y <= FMath::CeilToInt(cy + R); ++y)
				for (int32 x = FMath::FloorToInt(cx - R); x <= FMath::CeilToInt(cx + R); ++x)
				{
					const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
					const float lx = dx * Ca + dy * Sa, ly = -dx * Sa + dy * Ca;
					if (FMath::Square(lx / rx) + FMath::Square(ly / ry) <= 1.f) Set(x, y, C);
				}
		}
		void Line(float X0, float Y0, float X1, float Y1, float W, const FLinearColor& C)
		{
			const float x0 = X0 * 2, y0 = Y0 * 2, x1 = X1 * 2, y1 = Y1 * 2, r = FMath::Max(0.5f, W);
			const float L = FMath::Sqrt(FMath::Square(x1 - x0) + FMath::Square(y1 - y0));
			const int32 N = FMath::Max(1, FMath::CeilToInt(L * 2));
			for (int32 i = 0; i <= N; ++i)
			{
				const float t = (float)i / N, px = FMath::Lerp(x0, x1, t), py = FMath::Lerp(y0, y1, t);
				for (int32 y = FMath::FloorToInt(py - r); y <= FMath::CeilToInt(py + r); ++y)
					for (int32 x = FMath::FloorToInt(px - r); x <= FMath::CeilToInt(px + r); ++x)
						if (FMath::Square(x + 0.5f - px) + FMath::Square(y + 0.5f - py) <= r * r) Set(x, y, C);
			}
		}
		/** Convex/concave polygon fill (even-odd) in design units. */
		void Poly(std::initializer_list<FVector2f> Pts, const FLinearColor& C)
		{
			TArray<FVector2f> P;
			for (const FVector2f& V : Pts) P.Add(V * 2.f);
			for (int32 y = 0; y < S; ++y)
			{
				const float fy = y + 0.5f;
				TArray<float> Xs;
				for (int32 i = 0; i < P.Num(); ++i)
				{
					const FVector2f A = P[i], B = P[(i + 1) % P.Num()];
					if ((A.Y <= fy && B.Y > fy) || (B.Y <= fy && A.Y > fy)) Xs.Add(A.X + (fy - A.Y) / (B.Y - A.Y) * (B.X - A.X));
				}
				Xs.Sort();
				for (int32 k = 0; k + 1 < Xs.Num(); k += 2)
					for (int32 x = FMath::CeilToInt(Xs[k] - 0.5f); x <= FMath::FloorToInt(Xs[k + 1] - 0.5f); ++x) Set(x, y, C);
			}
		}
		/** Adds a dark outline around the silhouette and light/shadow edges (light from the top-left). */
		void Finish(float OutlineDark = 0.35f, bool bOutline = true)
		{
			FLinearColor Copy[S * S];
			FMemory::Memcpy(Copy, Px, sizeof(Px));
			for (int32 y = 0; y < S; ++y)
				for (int32 x = 0; x < S; ++x)
				{
					const FLinearColor& C = Copy[y * S + x];
					auto Op = [&](int32 X, int32 Y) { return X >= 0 && Y >= 0 && X < S && Y < S && Copy[Y * S + X].A > 0.5f; };
					if (C.A > 0.5f)
					{
						// bevel: top/left edge pixels lighter, bottom/right darker
						float F = 1.f;
						if (!Op(x - 1, y) || !Op(x, y - 1)) F = 1.18f;
						if (!Op(x + 1, y) || !Op(x, y + 1)) F = 0.78f;
						At(x, y) = FLinearColor(FMath::Min(C.R * F, 1.f), FMath::Min(C.G * F, 1.f), FMath::Min(C.B * F, 1.f), C.A);
						continue;
					}
					if (!bOutline) continue;
					// outline pixel: darkened neighbour colour
					for (int32 k = 0; k < 4; ++k)
					{
						const int32 nx = x + (k == 0) - (k == 1), ny = y + (k == 2) - (k == 3);
						if (!Op(nx, ny)) continue;
						const FLinearColor& N = Copy[ny * S + nx];
						At(x, y) = FLinearColor(N.R * OutlineDark, N.G * OutlineDark, N.B * OutlineDark, 1.f);
						break;
					}
				}
		}
		void Write(FColor* Out) const
		{
			for (int32 i = 0; i < S * S; ++i)
			{
				const FLinearColor& C = Px[i];
				Out[i] = FColor((uint8)FMath::Clamp(C.R * 255.f + 0.5f, 0.f, 255.f), (uint8)FMath::Clamp(C.G * 255.f + 0.5f, 0.f, 255.f),
					(uint8)FMath::Clamp(C.B * 255.f + 0.5f, 0.f, 255.f), (uint8)FMath::Clamp(C.A * 255.f + 0.5f, 0.f, 255.f));
			}
		}
	};

	FORCEINLINE FLinearColor Hex(uint32 H, float A = 1.f) { return FLinearColor(((H >> 16) & 255) / 255.f, ((H >> 8) & 255) / 255.f, (H & 255) / 255.f, A); }
	FORCEINLINE FLinearColor Sh(const FLinearColor& C, float F) { return FLinearColor(FMath::Min(C.R * F, 1.f), FMath::Min(C.G * F, 1.f), FMath::Min(C.B * F, 1.f), C.A); }
	FORCEINLINE FLinearColor Mx(const FLinearColor& A, const FLinearColor& B, float T) { return FLinearColor(FMath::Lerp(A.R, B.R, T), FMath::Lerp(A.G, B.G, T), FMath::Lerp(A.B, B.B, T), FMath::Lerp(A.A, B.A, T)); }
}
