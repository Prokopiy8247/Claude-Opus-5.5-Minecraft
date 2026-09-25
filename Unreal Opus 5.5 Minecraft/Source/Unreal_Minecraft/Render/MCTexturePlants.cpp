// Cutout plant sprites (cross / crop models): grasses, flowers, crops, fungi, vines, fire, crystals.
// Canvas convention: the plant grows from the bottom edge (y = S) upwards; hanging plants start at y = 0.
#include "Render/MCTexSynthInternal.h"

namespace
{
	using namespace MCTS;
	using K = EMCPlantKind;

	struct FPlant
	{
		FTex& T;
		const FMCTexDef& D;
		float P;
		FLinearColor C0, C1, C2;
		FPlant(FTex& InT, const FMCTexDef& InD) : T(InT), D(InD), P(InT.P16()), C0(Col(InD.C0)), C1(Col(InD.C1)), C2(Col(InD.C2)) {}
		float R(int32 i, int32 k = 0) const { return Hash01(D.Seed, i, k); }

		void Stem(float X, float Height, float Width, const FLinearColor& Col_, float Sway = 0.f)
		{
			Blade(T, X, (float)T.S, Height * P, PI * 0.5f + Sway, -Sway * 0.6f, Width * P, Shade(Col_, 0.85f), Col_);
		}
		void Grass(int32 Count, float MinH, float MaxH, const FLinearColor& A, const FLinearColor& B, float Spread = 13.f)
		{
			for (int32 i = 0; i < Count; ++i)
			{
				const float X = P * (8.f + (R(i, 1) - 0.5f) * Spread);
				const float Hh = FMath::Lerp(MinH, MaxH, R(i, 2));
				const float Ang = PI * 0.5f + (R(i, 3) - 0.5f) * 0.9f;
				Blade(T, X, (float)T.S, Hh * P, Ang, (R(i, 4) - 0.5f) * 1.2f, P * (0.9f + R(i, 5) * 0.6f), Shade(A, 0.75f), Mix(A, B, R(i, 6)));
			}
		}
		void Leaf(float X, float Y, float Len, float Ang, const FLinearColor& Col_)
		{
			Ellipse(T, X + FMath::Cos(Ang) * Len * 0.5f, Y - FMath::Sin(Ang) * Len * 0.5f, Len * 0.5f, Len * 0.22f, -Ang, WithA(Col_, 1.f), 0.6f);
			Line(T, X, Y, X + FMath::Cos(Ang) * Len * 0.9f, Y - FMath::Sin(Ang) * Len * 0.9f, 0.6f, WithA(Shade(Col_, 0.75f), 1.f), 0.55f);
		}
		void Petals(float CX, float CY, int32 N, float Rad, float PetalW, const FLinearColor& Pc, const FLinearColor& Cc, float CenterR)
		{
			for (int32 k = 0; k < N; ++k)
			{
				const float A = k / (float)N * 2 * PI + R(k, 9) * 0.3f;
				Ellipse(T, CX + FMath::Cos(A) * Rad * 0.55f, CY + FMath::Sin(A) * Rad * 0.55f, Rad * 0.55f, PetalW, A, WithA(Shade(Pc, 0.9f + 0.2f * R(k, 8)), 1.f), 0.8f);
			}
			if (CenterR > 0.f) Disc(T, CX, CY, CenterR, WithA(Cc, 1.f), 0.9f);
		}
		void Flower(float StemH, const FLinearColor& Petal, const FLinearColor& Center, int32 Style)
		{
			const float HX = P * 8, HY = T.S - StemH * P;
			Stem(HX, StemH, 0.9f, C0);
			Leaf(HX, T.S - P * 3.f, P * 4.f, PI * 0.2f, C0);
			Leaf(HX, T.S - P * 5.f, P * 3.5f, PI * 0.8f, Shade(C0, 0.9f));
			switch (Style)
			{
			case 0: Petals(HX, HY, 8, P * 2.8f, P * 0.9f, Petal, Center, P * 0.9f); break;                      // daisy-like
			case 1: Ellipse(T, HX, HY, P * 2.f, P * 2.6f, 0.f, WithA(Petal, 1.f), 0.8f); Line(T, HX, HY - P * 2.4f, HX, HY + P * 1.f, P * 0.5f, WithA(Shade(Petal, 0.75f), 1.f), 0.7f); break; // tulip cup
			case 2: Petals(HX, HY, 5, P * 2.4f, P * 1.2f, Petal, Center, P * 0.8f); break;                      // cup flower
			case 3: for (int32 k = 0; k < 14; ++k) { const float A = R(k, 21) * 2 * PI, Rr = R(k, 22) * P * 2.4f; Disc(T, HX + FMath::Cos(A) * Rr, HY + FMath::Sin(A) * Rr, P * 0.75f, WithA(Mix(Petal, Center, R(k, 23)), 1.f), 0.85f); } break; // globe cluster
			case 4: for (int32 k = 0; k < 4; ++k) { const float X = HX + (k - 1.5f) * P * 1.6f, Y = HY + FMath::Abs(k - 1.5f) * P; Disc(T, X, Y, P * 1.f, WithA(Petal, 1.f), 0.8f); Disc(T, X, Y, P * 0.35f, WithA(Center, 1.f), 0.9f); } break; // bluet clusters
			case 5: for (int32 k = 0; k < 4; ++k) { const float Y = HY + k * P * 1.8f; Line(T, HX, Y, HX + P * 2.2f, Y + P * 0.8f, 0.6f, WithA(C0, 1.f), 0.6f); Disc(T, HX + P * 2.4f, Y + P * 1.3f, P * 0.8f, WithA(Petal, 1.f), 0.85f); } break; // bells
			default: Petals(HX, HY, 6, P * 2.6f, P * 1.1f, Petal, Center, P * 0.8f); break;
			}
		}
		void Mushroom(const FLinearColor& Cap, const FLinearColor& Stalk, bool bFlat, bool bSpots)
		{
			FillRect(T, (int32)(P * 7), (int32)(P * 9), (int32)(P * 9), T.S, WithA(Stalk, 1.f), 0.6f);
			if (bFlat) { Ellipse(T, P * 8, P * 9, P * 4.5f, P * 1.6f, 0.f, WithA(Cap, 1.f), 0.8f); FillRect(T, (int32)(P * 4), (int32)(P * 9), (int32)(P * 12), (int32)(P * 10), WithA(Shade(Cap, 0.75f), 1.f), 0.7f); }
			else { Ellipse(T, P * 8, P * 8.5f, P * 3.6f, P * 2.8f, 0.f, WithA(Cap, 1.f), 0.85f); FillRect(T, (int32)(P * 4.4f), (int32)(P * 9), (int32)(P * 11.6f), (int32)(P * 10), WithA(Shade(Cap, 0.7f), 1.f), 0.7f); }
			if (bSpots) for (int32 k = 0; k < 4; ++k) Disc(T, P * (6 + (k % 2) * 3.5f + R(k, 31)), P * (7 + (k / 2) * 1.2f), P * 0.55f, WithA(C2, 1.f), 0.9f);
		}
		void Hanging(int32 Count, float MinL, float MaxL, const FLinearColor& A, const FLinearColor& B, float Width = 1.f)
		{
			for (int32 i = 0; i < Count; ++i)
			{
				const float X = P * (2.f + R(i, 41) * 12.f);
				Blade(T, X, 0.f, FMath::Lerp(MinL, MaxL, R(i, 42)) * P, -PI * 0.5f + (R(i, 43) - 0.5f) * 0.3f, (R(i, 44) - 0.5f) * 0.8f, Width * P, A, B);
			}
		}
		void Crystal(float X, float Height, float Width, float Lean, const FLinearColor& A, const FLinearColor& B)
		{
			const float BaseY = (float)T.S;
			const float TipX = X + Lean * P, TipY = BaseY - Height * P;
			for (int32 y = (int32)TipY; y < (int32)BaseY; ++y)
			{
				const float F = (y - TipY) / FMath::Max(1.f, BaseY - TipY);
				const float CX = FMath::Lerp(TipX, X, F), HW = FMath::Max(0.5f, Width * P * 0.5f * FMath::Min(1.f, F * 2.5f));
				for (int32 x = (int32)(CX - HW); x <= (int32)(CX + HW); ++x)
				{
					if (x < 0 || x >= T.S) continue;
					const int32 I = T.I(x, y);
					T.C[I] = WithA(x < CX ? Shade(B, 1.1f) : A, 1.f);
					T.H[I] = 0.8f;
					T.R[I] = 0.15f;
					T.E[I] = 0.6f + (1.f - F) * 0.4f;
				}
			}
		}
		void Flames(const FLinearColor& Outer, const FLinearColor& Inner, const FLinearColor& Hot)
		{
			for (int32 x = 0; x < T.S; ++x)
			{
				const float N = VNoise(x / (P * 1.5f), 0.f, FMath::Max(1, (int32)(T.S / (P * 1.5f))), D.Seed);
				const float Hgt = T.S * (0.45f + N * 0.5f);
				for (int32 y = T.S - (int32)Hgt; y < T.S; ++y)
				{
					const float F = (float)(T.S - y) / Hgt; // 0 bottom .. 1 tip
					const float Tongue = FMath::Sin((x / P + Fbm(T.U(x), T.U(y), 4, 3, D.Seed + 3) * 3.f) * 1.4f) * 0.5f + 0.5f;
					if (F > 0.55f + Tongue * 0.45f) continue;
					const int32 I = T.I(x, y);
					T.C[I] = WithA(F < 0.3f ? Mix(Hot, Inner, F / 0.3f) : Mix(Inner, Outer, (F - 0.3f) / 0.7f), 1.f);
					T.E[I] = 1.f - F * 0.4f;
					T.H[I] = 0.5f;
				}
			}
		}
		void Crop(int32 Stage, const FLinearColor& Produce, int32 Type)
		{
			const float Hh = 4.f + Stage * 3.5f;
			for (int32 i = 0; i < 5; ++i)
			{
				const float X = P * (2.f + i * 3.f + (R(i, 51) - 0.5f));
				const float H = Hh * (0.8f + R(i, 52) * 0.3f);
				const FLinearColor Stalk = Type == 0 && Stage >= 3 ? Mix(C0, Produce, 0.7f) : C0;
				Blade(T, X, (float)T.S, H * P, PI * 0.5f + (R(i, 53) - 0.5f) * 0.4f, (R(i, 54) - 0.5f) * 0.5f, P * 0.9f, Shade(Stalk, 0.8f), Stalk);
				if (Type == 0 && Stage >= 2) Ellipse(T, X + (R(i, 55) - 0.5f) * P, T.S - H * P + P * 1.5f, P * 0.8f, P * 2.2f, 0.f, WithA(Mix(Produce, C2, Stage == 2 ? 0.5f : 0.f), 1.f), 0.8f);
				if (Type == 1 && Stage >= 3) Disc(T, X, T.S - P * 1.2f, P * 1.1f, WithA(Produce, 1.f), 0.8f);
				if (Type == 2 && Stage >= 3) Disc(T, X, T.S - P * 1.f, P * 1.4f, WithA(Produce, 1.f), 0.8f);
				if (Type == 3 && Stage >= 2) { Leaf(X, T.S - H * P * 0.4f, P * 3.f, PI * 0.25f, Shade(C0, 0.9f)); if (Stage >= 3) Disc(T, X, T.S - P * 1.4f, P * 1.6f, WithA(Produce, 1.f), 0.85f); }
			}
		}
	};
}

namespace MCTS
{
	void RunPlant(FTex& T, const FMCTexDef& D)
	{
		T.Fill(WithA(Col(D.C0), 0.f), 0.3f, 0.75f);
		T.BakeAO = 0.15f;
		T.NormalStrength = 1.2f;
		FPlant Pl(T, D);
		const float P = Pl.P;
		const FLinearColor C0 = Pl.C0, C1 = Pl.C1, C2 = Pl.C2;
		const int32 V = (int32)D.P1;
		switch ((K)(int32)D.P0)
		{
		case K::ShortGrass: Pl.Grass(16, 6.f, 13.f, C0, C1); break;
		case K::TallGrassBottom: Pl.Grass(18, 14.f, 18.f, C0, C1); break;
		case K::TallGrassTop: Pl.Grass(14, 5.f, 13.f, C0, C1); break;
		case K::Fern:
			for (int32 f = 0; f < 4; ++f)
			{
				const float X = P * (3.f + f * 3.4f), Hh = (V == 2 ? 10.f : 11.f) + Pl.R(f, 60) * 4.f;
				const float Ang = PI * 0.5f + (f - 1.5f) * 0.25f;
				Pl.Stem(X, Hh, 0.7f, C0, (f - 1.5f) * 0.25f);
				for (int32 k = 1; k < 7; ++k) { const float Y = T.S - k * Hh * P / 7.f, Xk = X + FMath::Cos(Ang) * (T.S - Y) * 0.3f; Pl.Leaf(Xk, Y, P * (3.5f - k * 0.35f), PI * 0.15f, Mix(C0, C1, k / 7.f)); Pl.Leaf(Xk, Y, P * (3.5f - k * 0.35f), PI * 0.85f, Mix(C0, C1, k / 7.f)); }
			}
			break;
		case K::DeadBush:
			for (int32 b = 0; b < 7; ++b) { const float A = PI * 0.5f + (Pl.R(b, 61) - 0.5f) * 2.2f; Blade(T, P * 8, (float)T.S, P * (6 + Pl.R(b, 62) * 7), A, (Pl.R(b, 63) - 0.5f) * 1.5f, P * 0.6f, C2, C1); }
			break;
		case K::Dandelion: Pl.Flower(8.f, C1, C2, 3); break;
		case K::Poppy: Pl.Flower(9.f, C1, C2, 2); break;
		case K::BlueOrchid: Pl.Flower(9.f, C1, C2, 6); break;
		case K::Allium: Pl.Flower(12.f, C1, C2, 3); break;
		case K::AzureBluet: Pl.Flower(8.f, C1, C2, 4); break;
		case K::RedTulip: case K::OrangeTulip: case K::WhiteTulip: case K::PinkTulip: Pl.Flower(9.f, C1, C2, 1); break;
		case K::OxeyeDaisy: Pl.Flower(10.f, C1, C2, 0); break;
		case K::Cornflower: Pl.Flower(10.f, C1, C2, 6); break;
		case K::LilyOfTheValley: Pl.Flower(11.f, C1, C2, 5); break;
		case K::WitherRose: Pl.Flower(9.f, C1, C2, 2); break;
		case K::Torchflower:
			if (V == 1) { Pl.Grass(6, 4.f, 7.f, C0, C1); break; }
			Pl.Flower(9.f, C1, C2, 2);
			for (int32 k = 0; k < T.S * T.S; ++k) if (T.C[k].A > 0.5f && T.C[k].R > 0.8f) T.E[k] = 0.8f;
			break;
		case K::Eyeblossom:
			Pl.Flower(9.f, C1, C2, V == 1 ? 0 : 1);
			if (V == 1) for (int32 k = 0; k < T.S * T.S; ++k) if (T.C[k].A > 0.5f && T.C[k].R > 0.9f && T.C[k].B < 0.4f) T.E[k] = 1.f;
			break;
		case K::Sapling:
		{
			Pl.Stem(P * 8, 8.f, 1.1f, C1);
			const FLinearColor Lf = C2;
			for (int32 k = 0; k < 12; ++k) { const float A = Pl.R(k, 70) * 2 * PI, Rr = Pl.R(k, 71) * P * 4.f; Pl.Leaf(P * 8 + FMath::Cos(A) * Rr, P * 6 + FMath::Sin(A) * Rr * 0.8f, P * 3.f, Pl.R(k, 72) * 2 * PI, Shade(Lf, 0.8f + Pl.R(k, 73) * 0.4f)); }
			break;
		}
		case K::BrownMushroom: Pl.Mushroom(C1, C0, true, false); break;
		case K::RedMushroom: Pl.Mushroom(C1, C0, false, true); break;
		case K::CrimsonFungus: case K::WarpedFungus: Pl.Mushroom(C1, C0, false, true); break;
		case K::SugarCane:
			for (int32 s = 0; s < 3; ++s)
			{
				const int32 X0 = (int32)(P * (2.5f + s * 4.5f));
				for (int32 y = 0; y < T.S; ++y) for (int32 x = X0; x < X0 + (int32)(P * 2); ++x) { const int32 I = T.I(x, y); T.C[I] = WithA(Shade(Mix(C0, C1, (float)(x - X0) / (P * 2)), ((y + s * 13) % (int32)(P * 5)) < 2 ? 0.7f : 1.f), 1.f); T.H[I] = 0.6f; }
				Pl.Leaf(X0 + P, P * (4 + s * 3), P * 3.5f, s & 1 ? PI * 0.2f : PI * 0.8f, C1);
			}
			break;
		case K::Wheat: Pl.Crop(V, C1, 0); break;
		case K::Carrots: Pl.Crop(V, C1, 1); break;
		case K::Potatoes: Pl.Crop(V, C1, 2); break;
		case K::Beetroots: Pl.Crop(V, C1, 3); break;
		case K::NetherWart:
			for (int32 i = 0; i < 5; ++i) { const float X = P * (2.f + i * 3.f), H = (3.f + V * 3.f) * (0.8f + Pl.R(i, 80) * 0.4f); Blade(T, X, (float)T.S, H * P, PI * 0.5f, (Pl.R(i, 81) - 0.5f), P * 0.9f, C2, C0); Disc(T, X, T.S - H * P, P * (0.8f + V * 0.5f), WithA(C1, 1.f), 0.8f); }
			break;
		case K::SweetBerryBush:
			for (int32 k = 0; k < 16; ++k) Pl.Leaf(P * (3 + Pl.R(k, 90) * 10), T.S - P * (1 + Pl.R(k, 91) * 11), P * 3.f, Pl.R(k, 92) * 2 * PI, Shade(C0, 0.8f + Pl.R(k, 93) * 0.4f));
			if (V == 0) for (int32 k = 0; k < 7; ++k) Disc(T, P * (3 + Pl.R(k, 94) * 10), T.S - P * (2 + Pl.R(k, 95) * 9), P * 0.9f, WithA(C1, 1.f), 0.9f);
			break;
		case K::Kelp:
			if (V == 5) // coral fan / plant
			{
				for (int32 b = 0; b < 6; ++b) { const float A = PI * 0.5f + (b - 2.5f) * 0.3f; Blade(T, P * 8, (float)T.S, P * (9 + Pl.R(b, 100) * 5), A, (Pl.R(b, 101) - 0.5f), P * 1.2f, C0, C1); }
				for (int32 k = 0; k < 10; ++k) Disc(T, P * (3 + Pl.R(k, 102) * 10), P * (2 + Pl.R(k, 103) * 9), P * 0.8f, WithA(C1, 1.f), 0.8f);
				break;
			}
			for (int32 s = 0; s < 2; ++s) { Blade(T, P * (6 + s * 4), (float)T.S + P, (float)T.S * (V == 1 ? 1.1f : 0.8f), PI * 0.5f, (s ? 0.4f : -0.4f), P * 1.6f, C2, C0); }
			for (int32 k = 0; k < 6; ++k) Pl.Leaf(P * 8, T.S - P * (2 + k * 2.5f), P * 3.5f, (k & 1) ? PI * 0.15f : PI * 0.85f, C1);
			break;
		case K::Seagrass: Pl.Grass(12, V == 2 ? 8.f : 10.f, V == 1 ? 16.f : 14.f, C0, C1, 11.f); break;
		case K::CrimsonRoots: case K::WarpedRoots: case K::NetherSprouts:
			Pl.Grass(10, 4.f, (K)(int32)D.P0 == K::NetherSprouts ? 6.f : 12.f, C0, C1);
			break;
		case K::WeepingVines: case K::PaleHangingMoss: case K::HangingRoots: Pl.Hanging(8, 8.f, 16.f, C0, C1, (K)(int32)D.P0 == K::HangingRoots ? 0.8f : 1.2f); break;
		case K::TwistingVines: Pl.Grass(6, 12.f, 16.f, C0, C1, 8.f); break;
		case K::TallFlowerBottom: Pl.Grass(8, 16.f, 18.f, C0, C1, 6.f); for (int32 k = 0; k < 4; ++k) Pl.Leaf(P * 8, T.S - P * (3 + k * 3.5f), P * 4.f, (k & 1) ? PI * 0.2f : PI * 0.8f, C1); break;
		case K::TallFlowerTop:
			Pl.Grass(5, 6.f, 10.f, C0, C0, 6.f);
			if (V == 4) { for (int32 k = 0; k < 5; ++k) Pl.Leaf(P * 8, T.S - P * (2 + k * 2.5f), P * 4.5f, (k & 1) ? PI * 0.3f : PI * 0.7f, C1); break; }
			for (int32 k = 0; k < (V == 1 ? 22 : 12); ++k) { const float X = P * (4 + Pl.R(k, 110) * 8), Y = P * (2 + Pl.R(k, 111) * 9); Disc(T, X, Y, P * (V == 3 ? 1.6f : 1.1f), WithA(Mix(C1, C2, Pl.R(k, 112)), 1.f), 0.85f); }
			break;
		case K::Sunflower:
			Pl.Stem(P * 8, 12.f, 1.f, C0);
			Pl.Petals(P * 8, P * 5.5f, 12, P * 4.8f, P * 1.1f, C1, C2, P * 2.4f);
			for (int32 k = 0; k < 10; ++k) Disc(T, P * 8 + (Pl.R(k, 120) - 0.5f) * P * 3, P * 5.5f + (Pl.R(k, 121) - 0.5f) * P * 3, P * 0.3f, WithA(Shade(C2, 0.7f), 1.f), 0.95f);
			break;
		case K::Cobweb:
			for (int32 r = 0; r < 8; ++r) { const float A = r / 8.f * 2 * PI; Line(T, P * 8, P * 8, P * 8 + FMath::Cos(A) * P * 9, P * 8 + FMath::Sin(A) * P * 9, 0.8f, WithA(C1, 1.f), 0.6f); }
			for (int32 ring = 1; ring < 4; ++ring) for (int32 r = 0; r < 8; ++r) { const float A0 = r / 8.f * 2 * PI, A1 = (r + 1) / 8.f * 2 * PI, Rr = ring * P * 2.3f; Line(T, P * 8 + FMath::Cos(A0) * Rr, P * 8 + FMath::Sin(A0) * Rr, P * 8 + FMath::Cos(A1) * Rr * 0.92f, P * 8 + FMath::Sin(A1) * Rr * 0.92f, 0.7f, WithA(C0, 1.f), 0.55f); }
			break;
		case K::Azalea:
			for (int32 k = 0; k < 18; ++k) Pl.Leaf(P * (2 + Pl.R(k, 130) * 12), P * (2 + Pl.R(k, 131) * 12), P * 3.f, Pl.R(k, 132) * 2 * PI, Shade(C0, 0.8f + Pl.R(k, 133) * 0.4f));
			FillRect(T, (int32)(P * 7), (int32)(P * 10), (int32)(P * 9), T.S, WithA(C1, 1.f), 0.6f);
			if (V == 1) for (int32 k = 0; k < 6; ++k) Disc(T, P * (3 + Pl.R(k, 134) * 10), P * (2 + Pl.R(k, 135) * 8), P * 0.9f, WithA(C2, 1.f), 0.85f);
			break;
		case K::CaveVines:
			Pl.Hanging(3, 16.f, 17.f, C0, C1, 1.f);
			for (int32 k = 0; k < 6; ++k) Pl.Leaf(P * (5 + Pl.R(k, 140) * 6), P * (2 + k * 2.4f), P * 2.5f, (k & 1) ? -PI * 0.2f : PI * 1.2f, C1);
			if (V == 1) for (int32 k = 0; k < 4; ++k) { const float X = P * (5 + Pl.R(k, 141) * 6), Y = P * (3 + k * 3.2f); Disc(T, X, Y, P * 1.1f, WithA(C1, 1.f), 0.9f); Disc(T, X - P * 0.3f, Y - P * 0.3f, P * 0.4f, WithA(C2, 1.f), 1.f); for (int32 y = (int32)(Y - P * 1.2f); y <= (int32)(Y + P * 1.2f); ++y) for (int32 x = (int32)(X - P * 1.2f); x <= (int32)(X + P * 1.2f); ++x) if (FMath::Square(x - X) + FMath::Square(y - Y) < P * P * 1.2f) T.E[T.I(x, y)] = 1.f; }
			break;
		case K::SporeBlossom:
			for (int32 k = 0; k < 10; ++k) { const float A = k / 10.f * 2 * PI; Pl.Leaf(P * 8, P * 8, P * 6.f, A, Mix(C0, C1, (k & 1) ? 0.8f : 0.3f)); }
			Disc(T, P * 8, P * 8, P * 2.f, WithA(C2, 1.f), 0.9f);
			break;
		case K::PinkPetals: case K::Wildflowers: case K::LeafLitter:
			for (int32 k = 0; k < 22; ++k)
			{
				const float X = P * (1 + Pl.R(k, 150) * 14), Y = P * (1 + Pl.R(k, 151) * 14);
				if ((K)(int32)D.P0 == K::LeafLitter) Pl.Leaf(X, Y, P * 2.6f, Pl.R(k, 152) * 2 * PI, Mix(C0, C1, Pl.R(k, 153)));
				else { Disc(T, X, Y, P * 0.9f, WithA(Mix(C1, C2, Pl.R(k, 154) * 0.5f), 1.f), 0.7f); if (k % 3 == 0) Line(T, X, Y, X + P, Y + P, P * 0.4f, WithA(C0, 1.f), 0.6f); }
			}
			break;
		case K::BambooSapling: Pl.Stem(P * 8, 7.f, 1.2f, C0); Pl.Leaf(P * 8, P * 10, P * 4, PI * 0.25f, C1); Pl.Leaf(P * 8, P * 11, P * 3.5f, PI * 0.8f, C1); break;
		case K::Vine:
			for (int32 t = 0; t < 5; ++t) { float X = P * (1.5f + t * 3.2f), Y = 0.f; for (int32 s = 0; s < 8; ++s) { const float NX = X + (Pl.R(t, s + 160) - 0.5f) * P * 3, NY = Y + P * 2.2f; Line(T, X, Y, NX, NY, P * 0.5f, WithA(C2, 1.f), 0.5f); Pl.Leaf(NX, NY, P * 2.2f, Pl.R(t, s + 170) * 2 * PI, Mix(C0, C1, Pl.R(t, s + 180))); X = NX; Y = NY; } }
			break;
		case K::LilyPad:
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x)
			{
				const float DX = x + 0.5f - T.S * 0.5f, DY = y + 0.5f - T.S * 0.5f;
				const float Rr = FMath::Sqrt(DX * DX + DY * DY), A = FMath::Atan2(DY, DX);
				if (Rr > P * 7.2f || (A > 0.2f && A < 0.75f && Rr > P * 0.8f)) continue;
				const int32 I = T.I(x, y);
				const float Vein = FMath::Abs(FMath::Sin(A * 5.f)) < 0.12f ? 0.8f : 1.f;
				T.C[I] = WithA(Shade(Mix(C0, C1, Rr / (P * 7.2f)), Vein), 1.f);
				T.H[I] = 0.5f;
			}
			break;
		case K::AmethystCluster:
			Pl.Crystal(P * 8, 13.f, 3.f, 0.f, C0, C1); Pl.Crystal(P * 4.5f, 8.f, 2.4f, -2.f, C0, C1); Pl.Crystal(P * 11.5f, 9.f, 2.4f, 2.f, C0, C1);
			break;
		case K::SulfurSpike:
			Pl.Crystal(P * 8, 14.f, 3.6f, 0.f, C0, C1); Pl.Crystal(P * 5.f, 7.f, 2.2f, -1.f, C0, C1);
			break;
		case K::PointedDripstone:
			Pl.Crystal(P * 8, 15.f, 5.f, 0.f, C0, C1);
			for (int32 k = 0; k < T.S * T.S; ++k) { T.E[k] = 0.f; T.R[k] = 0.8f; }
			break;
		case K::GlowLichen:
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x)
			{
				const float N = Fbm(T.U(x), T.U(y), 4, 3, D.Seed);
				const bool bVein = V == 1 && FMath::Abs(Fbm(T.U(x), T.U(y), 3, 3, D.Seed + 5) - 0.5f) < 0.05f;
				if (N < 0.55f && !bVein) continue;
				const int32 I = T.I(x, y);
				const bool bGlow = V == 1 ? bVein : (Hash01(x / 3, y / 3, D.Seed) > 0.75f);
				T.C[I] = WithA(bGlow ? C2 : Mix(C0, C1, N), 1.f);
				T.E[I] = bGlow ? 1.f : 0.f;
				T.H[I] = 0.5f + N * 0.2f;
			}
			break;
		case K::Fire: case K::SoulFire: Pl.Flames(C0, C1, (K)(int32)D.P0 == K::SoulFire ? FLinearColor(0.8f, 1.f, 1.f, 1.f) : FLinearColor(1.f, 0.95f, 0.6f, 1.f)); break;
		case K::Stem:
			if (V == 1) { FillRect(T, (int32)(P * 6), 0, (int32)(P * 10), T.S, WithA(C0, 1.f), 0.6f); for (int32 y = 0; y < T.S; y += (int32)(P * 5)) FillRect(T, (int32)(P * 6), y, (int32)(P * 10), y + (int32)(P * 0.8f), WithA(C2, 1.f), 0.7f); Pl.Leaf(P * 10, P * 4, P * 4, PI * 0.2f, C1); break; }
			Pl.Stem(P * 8, 15.f, 1.1f, C1, 0.15f);
			Pl.Leaf(P * 8.5f, P * 8, P * 3, PI * 0.2f, C0);
			break;
		case K::Propagule:
			Line(T, P * 8, 0, P * 8, P * 5, P * 0.6f, WithA(C1, 1.f), 0.6f);
			Ellipse(T, P * 8, P * 6, P * 2.f, P * 1.2f, 0.f, WithA(C0, 1.f), 0.7f);
			Blade(T, P * 8, P * 7, P * 8, -PI * 0.5f, 0.f, P * 1.4f, C2, Shade(C2, 0.7f));
			break;
		case K::FireflyBush: case K::Bush:
			for (int32 k = 0; k < 24; ++k) Pl.Leaf(P * (2 + Pl.R(k, 190) * 12), T.S - P * (1 + Pl.R(k, 191) * 12), P * 3.f, Pl.R(k, 192) * 2 * PI, Shade(Mix(C0, C1, Pl.R(k, 193)), 0.8f + Pl.R(k, 194) * 0.3f));
			if ((K)(int32)D.P0 == K::FireflyBush) for (int32 k = 0; k < 8; ++k) { const int32 X = (int32)(P * (2 + Pl.R(k, 195) * 12)), Y = (int32)(P * (1 + Pl.R(k, 196) * 12)); const int32 I = T.I(X, Y); T.C[I] = WithA(C2, 1.f); T.E[I] = 1.f; }
			break;
		case K::Dripleaf:
			if (V == 1) { for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float DX = x + 0.5f - T.S * 0.5f, DY = y + 0.5f - T.S * 0.55f; if (DX * DX / (P * P * 56.f) + DY * DY / (P * P * 42.f) > 1.f) continue; const int32 I = T.I(x, y); T.C[I] = WithA(FMath::Abs(DX) < 0.8f ? Shade(C0, 0.8f) : Mix(C0, C1, (float)y / T.S), 1.f); T.H[I] = 0.5f; } }
			else { Pl.Stem(P * 8, 12.f, 0.8f, C2); Pl.Leaf(P * 8, P * 5, P * 5.f, PI * 0.1f, C1); Pl.Leaf(P * 8, P * 7, P * 4.5f, PI * 0.9f, C0); }
			break;
		default: Pl.Grass(10, 5.f, 12.f, C0, C1); break;
		}
		if (D.Flags & MCTF_Tinted) for (FLinearColor& X : T.C) if (X.A > 0.5f) X = WithA(Gray(X), X.A);
	}
}
