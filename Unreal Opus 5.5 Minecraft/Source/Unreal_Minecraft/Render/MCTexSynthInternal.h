// Internal toolkit of the procedural texture synthesizer (canvas, tileable noise, raster primitives).
#pragma once

#include "CoreMinimal.h"
#include "Blocks/MCTextures.h"

namespace MCTS
{
	/** Working canvas. Colours are sRGB-encoded floats (0..1); A = opacity / tint mask. */
	struct FTex
	{
		int32 S = 64;
		TArray<FLinearColor> C;
		TArray<float> H;   // height 0..1 (normal map / cavity AO source)
		TArray<float> R;   // roughness
		TArray<float> M;   // metallic
		TArray<float> E;   // emissive mask
		float NormalStrength = 2.5f;
		float BakeAO = 0.35f;     // how much cavity darkening is baked into the albedo
		explicit FTex(int32 InS);
		FORCEINLINE int32 I(int32 X, int32 Y) const
		{
			X %= S; if (X < 0) X += S;
			Y %= S; if (Y < 0) Y += S;
			return Y * S + X;
		}
		FORCEINLINE FLinearColor& Px(int32 X, int32 Y) { return C[I(X, Y)]; }
		FORCEINLINE float U(int32 X) const { return (X + 0.5f) / S; }
		void Fill(const FLinearColor& Col, float Height = 0.5f, float Rough = 0.85f);
		void SetAlpha(float A);
		/** Scale factor from 16-pixel "Minecraft" units to canvas texels. */
		FORCEINLINE float P16() const { return S / 16.f; }
	};

	// ---- colour
	FLinearColor Col(const FColor& C);
	FORCEINLINE FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float T)
	{
		return FLinearColor(A.R + (B.R - A.R) * T, A.G + (B.G - A.G) * T, A.B + (B.B - A.B) * T, A.A + (B.A - A.A) * T);
	}
	FORCEINLINE FLinearColor Shade(const FLinearColor& A, float F) { return FLinearColor(A.R * F, A.G * F, A.B * F, A.A); }
	FORCEINLINE FLinearColor WithA(const FLinearColor& A, float Al) { return FLinearColor(A.R, A.G, A.B, Al); }
	FLinearColor Gray(const FLinearColor& A);
	FLinearColor Jitter(const FLinearColor& A, float Amount, uint32 Seed); // small per-element hue/value variation

	// ---- hashing & noise (all tileable over the canvas)
	FORCEINLINE uint32 Hash(uint32 A, uint32 B = 0, uint32 C = 0, uint32 D = 0)
	{
		uint32 H = A * 0x9E3779B1u ^ (B + 0x7F4A7C15u) * 0x85EBCA77u ^ (C + 0x165667B1u) * 0xC2B2AE3Du ^ (D + 0x27D4EB2Fu) * 0x94D049BBu;
		H ^= H >> 15; H *= 0x2C1B3C6Du; H ^= H >> 12; H *= 0x297A2D39u; H ^= H >> 15;
		return H;
	}
	FORCEINLINE float Hash01(uint32 A, uint32 B = 0, uint32 C = 0, uint32 D = 0) { return (Hash(A, B, C, D) & 0xFFFFFF) / 16777216.f; }
	/** Value noise on a lattice that repeats every Period cells (X/Y in cell units). */
	float VNoise(float X, float Y, int32 Period, uint32 Seed);
	/** Fractal noise in texture space (U,V 0..1), base Cells lattice, doubling per octave. Returns ~0..1. */
	float Fbm(float U, float V, int32 Cells, int32 Octaves, uint32 Seed, float Gain = 0.5f);
	/** Anisotropic fractal noise (stretched along X by Stretch) - wood grain, strata. */
	float FbmAniso(float U, float V, int32 CellsX, int32 CellsY, int32 Octaves, uint32 Seed);
	struct FCell { float F1 = 1e9f, F2 = 1e9f; uint32 Id = 0; FVector2f Center; };
	/** Tileable Worley noise; distances in cell units. */
	FCell Worley(float U, float V, int32 Cells, uint32 Seed, float Jitter = 0.85f);
	/** Hash value shared by BlockxBlock texel clusters (pixel-art style variation). */
	FORCEINLINE float Blocky(int32 X, int32 Y, int32 Block, uint32 Seed) { return Hash01((uint32)(X / FMath::Max(1, Block)), (uint32)(Y / FMath::Max(1, Block)), Seed); }

	// ---- raster primitives (texel coordinates, wrap around)
	void Disc(FTex& T, float CX, float CY, float Rad, const FLinearColor& Col, float Height = -1.f, float Soft = 0.f);
	void Ellipse(FTex& T, float CX, float CY, float RX, float RY, float AngleRad, const FLinearColor& Col, float Height = -1.f);
	void Line(FTex& T, float X0, float Y0, float X1, float Y1, float Width, const FLinearColor& Col, float Height = -1.f);
	void FillRect(FTex& T, int32 X0, int32 Y0, int32 X1, int32 Y1, const FLinearColor& Col, float Height = -1.f);
	void RectBlend(FTex& T, int32 X0, int32 Y0, int32 X1, int32 Y1, const FLinearColor& Col, float Amount);
	void Border(FTex& T, int32 Width, const FLinearColor& Col, float Height = -1.f);
	/** Raised panel: lifts height inside the rect with a bevel of Bevel texels and shades edges (light top-left). */
	void Panel(FTex& T, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 Bevel, float Lift, float EdgeShade = 0.15f);
	/** Tapered curved blade (grass, leaves): from base to tip with a bend. */
	void Blade(FTex& T, float BX, float BY, float Length, float AngleRad, float Bend, float Width, const FLinearColor& Base, const FLinearColor& Tip);
	void MulColor(FTex& T, float F);
	/** Adds fractal variation to colour (value) and height. */
	void Grunge(FTex& T, float ColorAmount, float HeightAmount, int32 Cells, uint32 Seed);
	void Speckle(FTex& T, const FLinearColor& Col, float Density, int32 Size, uint32 Seed, float HeightDelta = 0.f);
	/** Scatter rounded stones (Worley) - cobble, gravel. Mortar/gaps use Gap colour. */
	void Stones(FTex& T, int32 Cells, const FLinearColor& A, const FLinearColor& B, const FLinearColor& Gap, float GapWidth, uint32 Seed, float Jitter = 0.85f);
	/** Running-bond bricks. Rows = brick rows per tile, Cols = bricks per row. */
	void Bricks(FTex& T, int32 Rows, int32 Cols, int32 Mortar, const FLinearColor& Brick, const FLinearColor& MortarCol, float Variation, uint32 Seed, bool bOffset = true);
	/** Horizontal wooden boards with grain. */
	void Planks(FTex& T, int32 Boards, const FLinearColor& Base, const FLinearColor& Grain, const FLinearColor& Gap, uint32 Seed, bool bVertical = false);
	/** Two-colour fractal mottle with optional pixel clustering. */
	void Mottle(FTex& T, const FLinearColor& A, const FLinearColor& B, int32 Cells, int32 Oct, float Contrast, uint32 Seed, int32 Cluster = 2, float HeightAmp = 0.35f);
	void Ore(FTex& T, const FLinearColor& A, const FLinearColor& B, const FLinearColor& Dark, int32 Style, bool bEmissive, bool bMetal, uint32 Seed);

	/** Generate a definition (by name) into T - used for base layers (ores on stone, grass on dirt). */
	void GenerateBase(FTex& T, FName Base);
	/** Recipe dispatch (implemented across MCTextureRecipes*.cpp). */
	void RunRecipe(FTex& T, const FMCTexDef& D);
	void RunPlant(FTex& T, const FMCTexDef& D);
	void RunObject(FTex& T, const FMCTexDef& D); // doors, rails, torches, glass, ladders, machines...
}
