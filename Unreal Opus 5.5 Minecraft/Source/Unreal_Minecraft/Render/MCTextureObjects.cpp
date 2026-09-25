// Object-like texture recipes: glass & bars, doors, trapdoors, ladders, rails, torches & lanterns, redstone parts, machines.
#include "Render/MCTexSynthInternal.h"

namespace
{
	using namespace MCTS;

	void Clear(FTex& T, const FLinearColor& C) { T.Fill(WithA(C, 0.f), 0.3f, 0.6f); }

	void Hole(FTex& T, int32 X0, int32 Y0, int32 X1, int32 Y1)
	{
		for (int32 y = Y0; y < Y1; ++y) for (int32 x = X0; x < X1; ++x) T.C[T.I(x, y)].A = 0.f;
	}

	void MetalFrame(FTex& T, const FLinearColor& A, const FLinearColor& B, int32 W)
	{
		Border(T, W, A, 0.8f);
		for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x)
		{
			if (x >= W && y >= W && x < T.S - W && y < T.S - W) continue;
			const int32 I = T.I(x, y);
			T.C[I] = WithA(Shade(Mix(A, B, (x < W || y < W) ? 0.6f : 0.f), 1.f), 1.f);
			T.M[I] = 1.f; T.R[I] = 0.35f;
		}
	}

	void GlassTex(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		const int32 Kind = (int32)D.P0;
		const float P = T.P16();
		switch (Kind)
		{
		case 0: case 1:
		{
			const bool bTrans = Kind == 1;
			T.Fill(WithA(A, bTrans ? 0.42f : 0.f), 0.5f, 0.05f);
			// frame
			const int32 W = FMath::Max(1, (int32)(P * 0.75f));
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x)
			{
				const int32 I = T.I(x, y);
				const bool bEdge = x < W || y < W || x >= T.S - W || y >= T.S - W;
				if (bEdge) { T.C[I] = WithA(x < W || y < W ? B : C, bTrans ? 0.85f : 1.f); T.H[I] = 0.7f; }
			}
			// diagonal glints
			for (int32 k = 0; k < 3; ++k)
			{
				const float Off = P * (3.f + k * 1.6f);
				Line(T, Off, P * 2.f, P * 2.f, Off, P * (k == 1 ? 0.9f : 0.5f), WithA(B, bTrans ? 0.75f : 0.8f), 0.7f);
			}
			Line(T, T.S - P * 2.5f, T.S - P * 5.f, T.S - P * 5.f, T.S - P * 2.5f, P * 0.5f, WithA(B, bTrans ? 0.7f : 0.7f), 0.7f);
			T.BakeAO = 0.f; T.NormalStrength = 0.6f;
			break;
		}
		case 2: case 5: // spawner cages
			Clear(T, A);
			MetalFrame(T, A, B, (int32)P);
			for (int32 k = 1; k < 4; ++k) { FillRect(T, (int32)(k * T.S / 4 - P * 0.5f), 0, (int32)(k * T.S / 4 + P * 0.5f), T.S, WithA(A, 1.f), 0.7f); FillRect(T, 0, (int32)(k * T.S / 4 - P * 0.5f), T.S, (int32)(k * T.S / 4 + P * 0.5f), WithA(A, 1.f), 0.7f); }
			if (Kind == 5) Border(T, (int32)(P * 0.5f), WithA(B, 1.f), 0.8f);
			for (int32 k = 0; k < T.S * T.S; ++k) if (T.C[k].A > 0.5f) { T.M[k] = 1.f; T.R[k] = 0.4f; }
			break;
		case 3: // bars
			Clear(T, A);
			for (int32 k = 0; k < 4; ++k)
			{
				const float X = (k + 0.5f) * T.S / 4;
				for (int32 y = 0; y < T.S; ++y) for (int32 x = (int32)(X - P * 0.6f); x < (int32)(X + P * 0.6f); ++x) { const int32 I = T.I(x, y); const float Rn = FMath::Abs(x + 0.5f - X) / (P * 0.6f); T.C[I] = WithA(Shade(Mix(B, A, Rn), 1.f), 1.f); T.H[I] = 1.f - Rn; T.M[I] = 1.f; T.R[I] = 0.4f; }
			}
			FillRect(T, 0, 0, T.S, (int32)P, WithA(A, 1.f), 0.8f);
			FillRect(T, 0, T.S - (int32)P, T.S, T.S, WithA(Shade(A, 0.8f), 1.f), 0.8f);
			break;
		case 4: // chain links
			Clear(T, A);
			for (int32 k = 0; k < 4; ++k)
			{
				const float Y = (k + 0.5f) * T.S / 4;
				const bool bSide = k & 1;
				if (!bSide) { for (int32 a = 0; a < 24; ++a) { const float A0 = a / 24.f * 2 * PI, A1 = (a + 1) / 24.f * 2 * PI; Line(T, T.S * 0.5f + FMath::Cos(A0) * P * 1.6f, Y + FMath::Sin(A0) * P * 2.4f, T.S * 0.5f + FMath::Cos(A1) * P * 1.6f, Y + FMath::Sin(A1) * P * 2.4f, P * 0.9f, WithA(B, 1.f), 0.8f); } }
				else FillRect(T, (int32)(T.S * 0.5f - P * 0.5f), (int32)(Y - P * 2.4f), (int32)(T.S * 0.5f + P * 0.5f), (int32)(Y + P * 2.4f), WithA(A, 1.f), 0.8f);
			}
			for (int32 k = 0; k < T.S * T.S; ++k) if (T.C[k].A > 0.5f) { T.M[k] = 1.f; T.R[k] = 0.35f; }
			break;
		case 6: // daylight detector: wooden rim + blue cells
			T.Fill(Col(D.C2), 0.5f, 0.7f);
			Planks(T, 4, Col(D.C0), Shade(Col(D.C0), 0.85f), Col(D.C2), D.Seed);
			for (int32 j = 0; j < 3; ++j) for (int32 k = 0; k < 3; ++k) { const int32 X0 = (int32)(P * (1.5f + k * 4.5f)), Y0 = (int32)(P * (1.5f + j * 4.5f)); FillRect(T, X0, Y0, X0 + (int32)(P * 3.5f), Y0 + (int32)(P * 3.5f), Col(D.C1), 0.4f); Line(T, X0 + 1.f, Y0 + P * 2.5f, X0 + P * 2.5f, Y0 + 1.f, P * 0.4f, Shade(Col(D.C1), 1.5f), 0.45f); }
			T.SetAlpha(1.f);
			break;
		default: T.Fill(WithA(A, 0.5f)); break;
		}
	}

	void DoorTex(FTex& T, const FMCTexDef& D, bool bTrapdoor)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		const bool bTop = D.P0 > 0.5f;
		const int32 Mat = (int32)D.P1;       // 0..11 wood, 20..23 copper stages, 30 iron
		const bool bMetal = Mat >= 20;
		const float P = T.P16();
		if (bMetal)
		{
			T.Fill(A, 0.5f, 0.35f);
			Grunge(T, 0.08f, 0.05f, 6, D.Seed);
			for (float& M : T.M) M = Mat >= 22 ? 0.3f : 0.9f;
			Panel(T, 0, 0, T.S, T.S, (int32)P, 0.12f, 0.2f);
			if (!bTrapdoor)
			{
				Panel(T, (int32)(P * 3), (int32)(P * (bTop ? 3 : 2)), (int32)(P * 13), (int32)(P * (bTop ? 13 : 14)), (int32)P, 0.1f, 0.2f);
				if (bTop) { Hole(T, (int32)(P * 4), (int32)(P * 4), (int32)(P * 7.5f), (int32)(P * 8)); Hole(T, (int32)(P * 8.5f), (int32)(P * 4), (int32)(P * 12), (int32)(P * 8)); }
				else FillRect(T, (int32)(P * 11), (int32)(P * 1), (int32)(P * 13), (int32)(P * 3), B, 0.9f); // handle
			}
			else for (int32 j = 0; j < 2; ++j) for (int32 k = 0; k < 2; ++k) Hole(T, (int32)(P * (3 + k * 6)), (int32)(P * (3 + j * 6)), (int32)(P * (7 + k * 6)), (int32)(P * (7 + j * 6)));
			for (int32 y = 0; y < T.S; y += (int32)(P * 4)) { Disc(T, P * 1.5f, y + P * 2, P * 0.5f, B, 0.9f); Disc(T, T.S - P * 1.5f, y + P * 2, P * 0.5f, B, 0.9f); }
			return;
		}
		Planks(T, bTrapdoor ? 4 : 1, A, B, Shade(A, 0.6f), D.Seed, !bTrapdoor);
		Panel(T, 0, 0, T.S, T.S, (int32)P, 0.1f, 0.15f);
		Border(T, (int32)P, Shade(A, 0.8f), 0.7f);
		if (!bTrapdoor)
		{
			// two raised panels per half; some woods get windows in the top half
			const bool bWindow = bTop && (Mat == 0 || Mat == 2 || Mat == 3 || Mat == 4 || Mat == 7 || Mat == 8 || Mat == 9 || Mat == 11);
			for (int32 k = 0; k < 2; ++k)
			{
				const int32 X0 = (int32)(P * (2.5f + k * 6)), X1 = (int32)(P * (7.5f + k * 6));
				if (bWindow) { Hole(T, X0, (int32)(P * 2.5f), X1, (int32)(P * 7.5f)); Panel(T, X0, (int32)(P * 9), X1, (int32)(P * 14), (int32)P, 0.12f); }
				else Panel(T, X0, (int32)(P * 2.5f), X1, (int32)(P * 13.5f), (int32)P, 0.12f);
			}
			if (!bTop) { Disc(T, P * 12.5f, P * 2.f, P * 0.9f, C, 0.95f); FillRect(T, (int32)(P * 11.5f), (int32)(P * 1.5f), (int32)(P * 13.5f), (int32)(P * 2.6f), Shade(C, 0.8f), 0.95f); }
		}
		else
		{
			const bool bSlits = Mat == 1 || Mat == 5 || Mat == 6 || Mat == 10;
			if (bSlits) for (int32 k = 0; k < 3; ++k) Hole(T, (int32)(P * 3), (int32)(P * (3.5f + k * 3.5f)), (int32)(P * 13), (int32)(P * (5 + k * 3.5f)));
			else for (int32 j = 0; j < 2; ++j) for (int32 k = 0; k < 2; ++k) Hole(T, (int32)(P * (3 + k * 6)), (int32)(P * (3 + j * 6)), (int32)(P * (7 + k * 6)), (int32)(P * (7 + j * 6)));
			FillRect(T, (int32)(P * 7), (int32)(P * 1), (int32)(P * 9), T.S - (int32)P, Shade(A, 0.9f), 0.7f);
		}
	}

	void LadderTex(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		const int32 Kind = (int32)D.P0;
		const float P = T.P16();
		Clear(T, A);
		if (Kind == 0)
		{
			for (int32 s = 0; s < 2; ++s) { const int32 X0 = (int32)(P * (s == 0 ? 2 : 12)); FillRect(T, X0, 0, X0 + (int32)(P * 2), T.S, WithA(A, 1.f), 0.8f); FillRect(T, X0, 0, X0 + (int32)(P * 0.5f), T.S, WithA(Shade(A, 1.15f), 1.f), 0.8f); }
			for (int32 r = 0; r < 4; ++r) { const int32 Y = (int32)(P * (1.5f + r * 4)); FillRect(T, (int32)(P * 3), Y, (int32)(P * 13), Y + (int32)(P * 1.6f), WithA(B, 1.f), 0.7f); FillRect(T, (int32)(P * 3), Y + (int32)(P * 1.2f), (int32)(P * 13), Y + (int32)(P * 1.6f), WithA(C, 1.f), 0.6f); }
		}
		else
		{
			// scaffolding: bamboo frame with diagonal brace (side) or slats (top)
			FillRect(T, 0, 0, T.S, (int32)(P * 2), WithA(A, 1.f), 0.8f);
			FillRect(T, 0, T.S - (int32)(P * 2), T.S, T.S, WithA(A, 1.f), 0.8f);
			FillRect(T, 0, 0, (int32)(P * 2), T.S, WithA(A, 1.f), 0.8f);
			FillRect(T, T.S - (int32)(P * 2), 0, T.S, T.S, WithA(A, 1.f), 0.8f);
			if (Kind == 2) Line(T, P * 2, T.S - P * 2, T.S - P * 2, P * 2, P * 1.5f, WithA(B, 1.f), 0.7f);
			else for (int32 k = 0; k < 3; ++k) FillRect(T, (int32)(P * 2), (int32)(P * (4 + k * 3.5f)), T.S - (int32)(P * 2), (int32)(P * (5.5f + k * 3.5f)), WithA(B, 1.f), 0.7f);
			for (int32 k = 0; k < T.S * T.S; ++k) if (T.C[k].A > 0.5f) T.C[k] = WithA(Shade(T.C[k], 0.9f + Hash01(k, D.Seed) * 0.2f), 1.f);
		}
		T.BakeAO = 0.2f;
	}

	void RailTex(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor Wood = Col(D.C0), Metal = Col(D.C1), Accent = Col(D.C2);
		const int32 Kind = (int32)D.P0;
		const bool bOn = D.P1 > 0.5f;
		const float P = T.P16();
		Clear(T, Wood);
		auto Tie = [&](float Y) { FillRect(T, (int32)(P * 1), (int32)(Y - P), (int32)(P * 15), (int32)(Y + P), WithA(Wood, 1.f), 0.5f); FillRect(T, (int32)(P * 1), (int32)(Y + P * 0.4f), (int32)(P * 15), (int32)(Y + P), WithA(Shade(Wood, 0.75f), 1.f), 0.45f); };
		if (Kind == 1)
		{
			for (int32 k = 0; k < 4; ++k) Tie(P * (2 + k * 4));
			// curved rails: quarter arcs around the (16,16) corner
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x)
			{
				const float R = FMath::Sqrt(FMath::Square(T.S - x - 0.5f) + FMath::Square(T.S - y - 0.5f)) / P;
				if (FMath::Abs(R - 13.f) < 0.9f || FMath::Abs(R - 3.f) < 0.9f) { const int32 I = T.I(x, y); T.C[I] = WithA(Metal, 1.f); T.H[I] = 0.9f; T.M[I] = 1.f; T.R[I] = 0.3f; }
			}
		}
		else
		{
			for (int32 k = 0; k < 4; ++k) Tie(P * (2 + k * 4));
			for (int32 s = 0; s < 2; ++s)
			{
				const int32 X0 = (int32)(P * (s == 0 ? 2.5f : 11.5f));
				for (int32 y = 0; y < T.S; ++y) for (int32 x = X0; x < X0 + (int32)(P * 2); ++x) { const int32 I = T.I(x, y); T.C[I] = WithA(x < X0 + (int32)P ? Shade(Metal, 1.15f) : Metal, 1.f); T.H[I] = 0.9f; T.M[I] = 1.f; T.R[I] = 0.3f; }
			}
			if (Kind >= 2)
			{
				// powered / detector / activator: accent strip between the rails
				const FLinearColor Mid = bOn ? Accent : Shade(Accent, 0.7f);
				FillRect(T, (int32)(P * 7), 0, (int32)(P * 9), T.S, WithA(Kind == 2 ? Col(FColor(0xE8, 0xC2, 0x3A)) : Metal, 1.f), 0.6f);
				for (int32 k = 0; k < 4; ++k) FillRect(T, (int32)(P * 7), (int32)(P * (1.5f + k * 4)), (int32)(P * 9), (int32)(P * (2.5f + k * 4)), WithA(Mid, 1.f), 0.65f);
				if (bOn) for (int32 k = 0; k < T.S * T.S; ++k) if (T.C[k].R > 0.8f && T.C[k].G < 0.4f) T.E[k] = 0.9f;
			}
		}
		T.BakeAO = 0.2f;
	}

	void TorchTex(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor Stick = Col(D.C0), Flame = Col(D.C1), Core = Col(D.C2);
		const int32 Kind = (int32)D.P0;
		const bool bOff = D.P1 > 0.5f;
		const float P = T.P16();
		Clear(T, Stick);
		if (Kind <= 2 || Kind == 4)
		{
			// torch occupies the centre 2x10 column like the block model expects (x 7..9, y 6..16)
			for (int32 y = (int32)(P * 6); y < T.S; ++y) for (int32 x = (int32)(P * 7); x < (int32)(P * 9); ++x) { const int32 I = T.I(x, y); T.C[I] = WithA(Shade(Stick, x < (int32)(P * 8) ? 1.12f : 0.85f), 1.f); T.H[I] = 0.6f; }
			// head
			const FLinearColor HeadC = Kind == 2 ? (bOff ? Shade(Flame, 0.6f) : Flame) : Mix(Flame, Core, 0.3f);
			FillRect(T, (int32)(P * 7), (int32)(P * 6), (int32)(P * 9), (int32)(P * 8), WithA(HeadC, 1.f), 0.8f);
			if (!bOff)
			{
				Disc(T, P * 8, P * 5.2f, P * 1.4f, WithA(Core, 1.f), 0.9f);
				Disc(T, P * 8, P * 4.6f, P * 0.8f, WithA(Flame, 1.f), 1.f);
				for (int32 k = 0; k < T.S * T.S; ++k) if (T.C[k].A > 0.5f && (T.C[k].R + T.C[k].G) > 1.2f) T.E[k] = 1.f;
			}
			if (Kind == 2) for (int32 k = 0; k < T.S * T.S; ++k) if (T.C[k].A > 0.5f && T.C[k].R > 0.6f && T.C[k].G < 0.35f) T.E[k] = bOff ? 0.f : 1.f;
		}
		else if (Kind == 3 || Kind == 5 || Kind == 6)
		{
			// lantern: metal cage with glowing glass, cap and handle
			const FLinearColor Frame = Stick;
			FillRect(T, (int32)(P * 5), (int32)(P * 7), (int32)(P * 11), (int32)(P * 16), WithA(Frame, 1.f), 0.7f);
			FillRect(T, (int32)(P * 6), (int32)(P * 8), (int32)(P * 10), (int32)(P * 15), WithA(Flame, 1.f), 0.5f);
			FillRect(T, (int32)(P * 7.5f), (int32)(P * 9), (int32)(P * 8.5f), (int32)(P * 14), WithA(Shade(Frame, 0.8f), 1.f), 0.7f);
			FillRect(T, (int32)(P * 6), (int32)(P * 5), (int32)(P * 10), (int32)(P * 7), WithA(Shade(Frame, 1.2f), 1.f), 0.8f);
			FillRect(T, (int32)(P * 7), (int32)(P * 2), (int32)(P * 9), (int32)(P * 5), WithA(Core, 1.f), 0.8f);
			for (int32 k = 0; k < T.S * T.S; ++k) { if (T.C[k].A > 0.5f && T.H[k] < 0.55f && T.H[k] > 0.45f) T.E[k] = 1.f; if (T.C[k].A > 0.5f && T.H[k] >= 0.7f) { T.M[k] = 1.f; T.R[k] = 0.4f; } }
		}
		else // brewing stand rod & bottles
		{
			FillRect(T, (int32)(P * 7), (int32)(P * 2), (int32)(P * 9), (int32)(P * 16), WithA(Shade(Core, 0.9f), 1.f), 0.8f);
			for (int32 s = 0; s < 3; ++s) { const float X = P * (3 + s * 5); Disc(T, X, P * 12, P * 2.f, WithA(Flame, 0.9f), 0.6f); FillRect(T, (int32)(X - P * 0.6f), (int32)(P * 8.5f), (int32)(X + P * 0.6f), (int32)(P * 10.5f), WithA(Flame, 0.9f), 0.6f); }
			for (int32 k = 0; k < T.S * T.S; ++k) if (T.C[k].A > 0.5f) T.R[k] = 0.2f;
		}
		T.BakeAO = 0.1f;
		T.NormalStrength = 1.2f;
	}

	void DustTex(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1);
		const bool bLine = D.P0 > 0.5f;
		const float P = T.P16();
		Clear(T, A);
		auto Grains = [&](float X0, float Y0, float X1, float Y1)
		{
			for (int32 y = (int32)Y0; y < (int32)Y1; ++y) for (int32 x = (int32)X0; x < (int32)X1; ++x)
			{
				const float N = Fbm(T.U(x), T.U(y), 16, 2, D.Seed);
				if (N < 0.35f) continue;
				const int32 I = T.I(x, y);
				T.C[I] = WithA(Mix(A, B, N), 1.f);
				T.H[I] = 0.5f + N * 0.3f;
			}
		};
		if (bLine) Grains(P * 5.5f, 0, P * 10.5f, (float)T.S);
		else { Grains(P * 4.5f, P * 4.5f, P * 11.5f, P * 11.5f); Grains(P * 6.5f, P * 2.5f, P * 9.5f, P * 13.5f); Grains(P * 2.5f, P * 6.5f, P * 13.5f, P * 9.5f); }
	}

	void LampTex(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		const bool bOn = D.P0 > 0.5f;
		const float P = T.P16();
		for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const FCell W = Worley(T.U(x), T.U(y), 4, D.Seed, 0.6f); const int32 I = T.I(x, y); const float F = Hash01(W.Id, 1); T.C[I] = WithA(W.F2 - W.F1 < 0.07f ? C : Mix(A, B, F), 1.f); T.H[I] = 0.5f + F * 0.3f; T.R[I] = 0.3f; if (bOn) T.E[I] = W.F2 - W.F1 < 0.07f ? 0.3f : 0.6f + F * 0.4f; }
		// metal frame
		for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const int32 E = FMath::Min(FMath::Min(x, y), FMath::Min(T.S - 1 - x, T.S - 1 - y)); if (E < (int32)P) { const int32 I = T.I(x, y); T.C[I] = WithA(Col(FColor(0x5A, 0x3A, 0x22)), 1.f); T.H[I] = 0.8f; T.E[I] = 0.f; } }
		FillRect(T, (int32)(P * 7.5f), 0, (int32)(P * 8.5f), T.S, Col(FColor(0x6A, 0x4A, 0x2A)), 0.8f);
		FillRect(T, 0, (int32)(P * 7.5f), T.S, (int32)(P * 8.5f), Col(FColor(0x6A, 0x4A, 0x2A)), 0.8f);
		for (int32 k = 0; k < T.S * T.S; ++k) if (T.H[k] >= 0.8f) T.E[k] = 0.f;
	}

	void MachineTex(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		const int32 Kind = (int32)D.P0;
		const float P = T.P16();
		// stone housing
		Stones(T, 4, A, Shade(A, 0.88f), Shade(A, 0.6f), 0.06f, D.Seed, 0.45f);
		Panel(T, 0, 0, T.S, T.S, (int32)P, 0.1f, 0.12f);
		switch (Kind)
		{
		case 0: Planks(T, 1, B, Shade(B, 0.85f), Shade(B, 0.6f), D.Seed); for (int32 y = (int32)(P * 4); y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const int32 I = T.I(x, y); T.C[I] = WithA(Mix(A, Shade(A, 0.85f), Fbm(T.U(x), T.U(y), 6, 3, D.Seed + 4)), 1.f); T.H[I] = 0.55f; } FillRect(T, (int32)(P * 6), (int32)(P * 4), (int32)(P * 10), T.S, Shade(C, 1.f), 0.7f); break; // piston side
		case 1: case 2: Planks(T, 4, A, B, Shade(A, 0.6f), D.Seed); Panel(T, 0, 0, T.S, T.S, (int32)P, 0.1f); if (Kind == 2) for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float N = Fbm(T.U(x), T.U(y), 4, 3, D.Seed + 8); if (N > 0.42f) { const int32 I = T.I(x, y); T.C[I] = WithA(Mix(B, FLinearColor(0.6f, 0.9f, 0.5f, 1.f), N - 0.42f), 1.f); T.R[I] = 0.2f; } } break; // piston head
		case 3: FillRect(T, (int32)(P * 5), (int32)(P * 5), (int32)(P * 11), (int32)(P * 11), C, 0.3f); break; // piston bottom
		case 4: FillRect(T, (int32)(P * 3), (int32)(P * 3), (int32)(P * 13), (int32)(P * 13), Shade(A, 0.7f), 0.3f); FillRect(T, (int32)(P * 6), (int32)(P * 6), (int32)(P * 10), (int32)(P * 10), C, 0.2f); break;
		case 5: case 7: // dispenser / dropper front
			FillRect(T, (int32)(P * 4), (int32)(P * 4), (int32)(P * 12), (int32)(P * 12), Shade(B, 1.1f), 0.45f);
			if (Kind == 5) Disc(T, T.S * 0.5f, T.S * 0.5f, P * 3.f, C, 0.15f); else FillRect(T, (int32)(P * 5), (int32)(P * 6), (int32)(P * 11), (int32)(P * 10), C, 0.15f);
			break;
		case 6: case 8: FillRect(T, (int32)(P * 4), (int32)(P * 4), (int32)(P * 12), (int32)(P * 12), Shade(B, 1.1f), 0.45f); Disc(T, T.S * 0.5f, T.S * 0.5f, Kind == 6 ? P * 3.f : P * 2.f, C, 0.15f); break;
		case 9: case 10: // crafter
			FillRect(T, (int32)(P * 2), (int32)(P * 2), (int32)(P * 14), (int32)(P * 14), Shade(B, 1.2f), 0.4f);
			for (int32 j = 0; j < 3; ++j) for (int32 k = 0; k < 3; ++k) FillRect(T, (int32)(P * (3 + k * 4)), (int32)(P * (3 + j * 4)), (int32)(P * (6 + k * 4)), (int32)(P * (6 + j * 4)), Kind == 9 ? Shade(A, 0.8f) : C, 0.5f);
			break;
		case 11: // observer sensor face: two vents and a lens bar (original layout)
			FillRect(T, (int32)(P * 2), (int32)(P * 3), (int32)(P * 14), (int32)(P * 7), B, 0.3f);
			for (int32 k = 0; k < 5; ++k) FillRect(T, (int32)(P * (2.5f + k * 2.4f)), (int32)(P * 3.5f), (int32)(P * (3.7f + k * 2.4f)), (int32)(P * 6.5f), Shade(B, 0.5f), 0.2f);
			FillRect(T, (int32)(P * 3), (int32)(P * 9), (int32)(P * 13), (int32)(P * 12), Shade(C, 0.8f), 0.6f);
			break;
		case 12: Disc(T, T.S * 0.5f, T.S * 0.5f, P * 2.5f, Shade(A, 0.6f), 0.3f); Disc(T, T.S * 0.5f, T.S * 0.5f, P * 1.5f, C, 0.5f); if (D.P1 > 0.5f) for (int32 k = 0; k < T.S * T.S; ++k) if (T.C[k].R > 0.8f) T.E[k] = 1.f; break;
		case 13: Line(T, P * 8, P * 3, P * 8, P * 13, P * 1.4f, Shade(B, 0.8f), 0.3f); Line(T, P * 8, P * 3, P * 5, P * 6.5f, P * 1.4f, Shade(B, 0.8f), 0.3f); Line(T, P * 8, P * 3, P * 11, P * 6.5f, P * 1.4f, Shade(B, 0.8f), 0.3f); break;
		case 14: FillRect(T, (int32)(P * 4), (int32)(P * 7), (int32)(P * 12), (int32)(P * 9), Shade(B, 0.8f), 0.3f); break;
		default: break;
		}
		T.NormalStrength = 2.2f;
	}
}

namespace MCTS
{
	void RunObject(FTex& T, const FMCTexDef& D)
	{
		switch (D.Recipe)
		{
		case EMCTexRecipe::Glass: GlassTex(T, D); break;
		case EMCTexRecipe::Door: DoorTex(T, D, false); break;
		case EMCTexRecipe::Trapdoor: DoorTex(T, D, true); break;
		case EMCTexRecipe::Ladder: LadderTex(T, D); break;
		case EMCTexRecipe::Rail: RailTex(T, D); break;
		case EMCTexRecipe::Torch: TorchTex(T, D); break;
		case EMCTexRecipe::RedstoneDust: DustTex(T, D); break;
		case EMCTexRecipe::Lamp: LampTex(T, D); break;
		case EMCTexRecipe::Machine: MachineTex(T, D); break;
		default:
			Mottle(T, Col(D.C0), Col(D.C1), 4, 4, 1.3f, D.Seed);
			break;
		}
	}
}
