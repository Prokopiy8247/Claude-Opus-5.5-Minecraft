// Crafted / functional block texture recipes: masonry, metal, gems, coloured families, workstations, crops blocks.
#include "Render/MCTexSynthInternal.h"

namespace
{
	using namespace MCTS;

	void Smooth(FTex& T, const FLinearColor& A, const FLinearColor& B, uint32 Seed, float Amount, float Rough)
	{
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const float N = Fbm(T.U(x), T.U(y), 3, 4, Seed);
				const int32 I = T.I(x, y);
				T.C[I] = WithA(Mix(A, B, FMath::Clamp(0.5f + (N - 0.5f) * Amount * 4.f, 0.f, 1.f)), T.C[I].A);
				T.H[I] = 0.5f + (N - 0.5f) * 0.1f;
				T.R[I] = Rough;
			}
	}

	/** Engraved line (height dip + darker colour). */
	void Groove(FTex& T, float X0, float Y0, float X1, float Y1, float W, const FLinearColor& Dark)
	{
		Line(T, X0 + 0.7f, Y0 + 0.7f, X1 + 0.7f, Y1 + 0.7f, W, Shade(Dark, 1.25f), 0.45f);
		Line(T, X0, Y0, X1, Y1, W, Dark, 0.2f);
	}

	void Rivets(FTex& T, const FLinearColor& Col_, float Inset)
	{
		const float P = T.P16();
		const float Pos[2] = { Inset * P, T.S - Inset * P };
		for (float X : Pos) for (float Y : Pos) { Disc(T, X + 0.5f, Y + 0.5f, P * 0.55f, Shade(Col_, 0.6f), 0.7f); Disc(T, X, Y, P * 0.5f, Col_, 0.9f); }
	}

	void Motif(FTex& T, int32 Style, const FLinearColor& Dark, const FLinearColor& Light)
	{
		// original carved motifs (no reproductions): 0 ring, 1 diamond, 2 cross-hatch, 3 sun, 4 spiral, 5 double frame
		const float P = T.P16(), C = T.S * 0.5f;
		switch (Style % 6)
		{
		case 0: for (int32 a = 0; a < 48; ++a) { const float A0 = a / 48.f * 2 * PI, A1 = (a + 1) / 48.f * 2 * PI; Groove(T, C + FMath::Cos(A0) * P * 4.5f, C + FMath::Sin(A0) * P * 4.5f, C + FMath::Cos(A1) * P * 4.5f, C + FMath::Sin(A1) * P * 4.5f, P * 0.8f, Dark); } Disc(T, C, C, P * 1.5f, Light, 0.8f); break;
		case 1: Groove(T, C, C - P * 5, C + P * 5, C, P * 0.8f, Dark); Groove(T, C + P * 5, C, C, C + P * 5, P * 0.8f, Dark); Groove(T, C, C + P * 5, C - P * 5, C, P * 0.8f, Dark); Groove(T, C - P * 5, C, C, C - P * 5, P * 0.8f, Dark); Disc(T, C, C, P * 1.2f, Dark, 0.3f); break;
		case 2: for (int32 k = -2; k <= 2; ++k) { Groove(T, C + k * P * 2.4f - P * 4, C - P * 4, C + k * P * 2.4f + P * 4, C + P * 4, P * 0.6f, Dark); } break;
		case 3: for (int32 r = 0; r < 8; ++r) { const float A = r / 8.f * 2 * PI; Groove(T, C + FMath::Cos(A) * P * 1.8f, C + FMath::Sin(A) * P * 1.8f, C + FMath::Cos(A) * P * 5.2f, C + FMath::Sin(A) * P * 5.2f, P * 0.7f, Dark); } Disc(T, C, C, P * 1.6f, Light, 0.85f); break;
		case 4: { float PX = C, PY = C; for (int32 i = 1; i < 60; ++i) { const float A = i * 0.32f, R = i * P * 0.09f; const float NX = C + FMath::Cos(A) * R, NY = C + FMath::Sin(A) * R; Groove(T, PX, PY, NX, NY, P * 0.6f, Dark); PX = NX; PY = NY; } break; }
		default: for (int32 k = 0; k < 2; ++k) { const float In = P * (3 + k * 2.5f); Groove(T, In, In, T.S - In, In, P * 0.6f, Dark); Groove(T, T.S - In, In, T.S - In, T.S - In, P * 0.6f, Dark); Groove(T, T.S - In, T.S - In, In, T.S - In, P * 0.6f, Dark); Groove(T, In, T.S - In, In, In, P * 0.6f, Dark); } break;
		}
	}

	void Furnace(FTex& T, const FMCTexDef& D, int32 Kind)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		const float P = T.P16();
		Stones(T, 4, A, Shade(A, 0.85f), B, 0.07f, D.Seed, 0.4f);
		Panel(T, 0, 0, T.S, T.S, (int32)P, 0.1f, 0.12f);
		if (Kind == 0) // front
		{
			const bool bLit = ((int32)D.P0 & 1) == 1;
			FillRect(T, (int32)(P * 3), (int32)(P * 8), (int32)(P * 13), (int32)(P * 14), B, 0.4f);
			FillRect(T, (int32)(P * 4), (int32)(P * 9), (int32)(P * 12), (int32)(P * 13), bLit ? C : Shade(B, 0.3f), 0.15f);
			if (bLit) { for (int32 y = (int32)(P * 9); y < (int32)(P * 13); ++y) for (int32 x = (int32)(P * 4); x < (int32)(P * 12); ++x) { const float F = Fbm(T.U(x), T.U(y), 6, 3, D.Seed + 3); const int32 I = T.I(x, y); T.C[I] = WithA(Mix(Shade(C, 0.6f), FLinearColor(1.f, 0.9f, 0.5f, 1.f), F), 1.f); T.E[I] = 0.6f + F * 0.4f; } }
			FillRect(T, (int32)(P * 3), (int32)(P * 3), (int32)(P * 13), (int32)(P * 6), Shade(B, 0.9f), 0.45f);
			FillRect(T, (int32)(P * 4), (int32)(P * 4), (int32)(P * 12), (int32)(P * 5), Shade(B, 0.5f), 0.3f);
			if (D.P0 >= 2 && D.P0 < 4) { for (int32 k = 0; k < 3; ++k) FillRect(T, (int32)(P * (4 + k * 3)), (int32)(P * 9), (int32)(P * (4 + k * 3) + P), (int32)(P * 13), Shade(A, 0.55f), 0.5f); } // blast furnace grill
			if (D.P0 >= 4) FillRect(T, 0, 0, T.S, (int32)(P * 2), Col(FColor(0x4E, 0x3A, 0x22)), 0.6f); // smoker wood band
		}
		else if (Kind == 2) FillRect(T, (int32)(P * 2), (int32)(P * 2), (int32)(P * 14), (int32)(P * 14), Shade(A, 1.05f), 0.55f);
	}

	void Crafting(FTex& T, const FMCTexDef& D, int32 Face)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		const float P = T.P16();
		Planks(T, 4, A, Shade(A, 0.85f), B, D.Seed);
		if (Face == 0) // top: work grid
		{
			FillRect(T, 0, 0, T.S, T.S, Shade(A, 0.95f), 0.55f);
			Grunge(T, 0.06f, 0.05f, 6, D.Seed + 1);
			Border(T, (int32)P, B, 0.7f);
			for (int32 k = 1; k < 3; ++k) { Groove(T, P * 1 + k * P * 14 / 3, P, P * 1 + k * P * 14 / 3, T.S - P, P * 0.6f, Shade(B, 0.8f)); Groove(T, P, P * 1 + k * P * 14 / 3, T.S - P, P * 1 + k * P * 14 / 3, P * 0.6f, Shade(B, 0.8f)); }
		}
		else
		{
			FillRect(T, 0, 0, T.S, (int32)(P * 2), B, 0.75f); // table top overhang
			FillRect(T, 0, (int32)(P * 2), T.S, (int32)(P * 2.5f), Shade(B, 0.5f), 0.3f);
			if (Face == 1) { Line(T, P * 3, P * 6, P * 7, P * 12, P * 1.2f, C, 0.8f); FillRect(T, (int32)(P * 2), (int32)(P * 4), (int32)(P * 5), (int32)(P * 7), Shade(C, 0.8f), 0.85f); Line(T, P * 12, P * 5, P * 10, P * 13, P, Col(FColor(0x6B, 0x53, 0x35)), 0.8f); FillRect(T, (int32)(P * 10), (int32)(P * 4), (int32)(P * 14), (int32)(P * 6), C, 0.85f); }
			else { Line(T, P * 4, P * 5, P * 12, P * 13, P * 1.2f, Col(FColor(0x6B, 0x53, 0x35)), 0.8f); FillRect(T, (int32)(P * 10), (int32)(P * 4), (int32)(P * 13), (int32)(P * 7), C, 0.85f); for (int32 k = 0; k < 3; ++k) FillRect(T, (int32)(P * (3 + k * 2)), (int32)(P * 10), (int32)(P * (4 + k * 2)), (int32)(P * 14), Shade(C, 0.7f), 0.6f); }
		}
	}

	void Books(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		const float P = T.P16();
		const int32 Kind = (int32)D.P0;
		Planks(T, 4, A, Shade(A, 0.85f), B, D.Seed);
		for (int32 Shelf = 0; Shelf < 2; ++Shelf)
		{
			const int32 Y0 = (int32)(P * (1 + Shelf * 8)), Y1 = (int32)(P * (7 + Shelf * 8));
			FillRect(T, (int32)P, Y0, T.S - (int32)P, Y1, Shade(B, 0.35f), 0.2f);
			if (Kind == 1) continue;
			int32 X = (int32)P;
			int32 Book = 0;
			while (X < T.S - (int32)P - 2)
			{
				const int32 W = (int32)(P * (1.f + Hash01(D.Seed, Shelf, Book) * 1.2f));
				const int32 Hgt = (int32)(P * (4.f + Hash01(D.Seed, Shelf, Book, 1) * 2.f));
				static const uint32 Palette[6] = { 0x8A2A2A, 0x2A4A8A, 0x3A6A2A, 0x8A6A2A, 0x5A2A6A, 0x2A6A6A };
				const FLinearColor BC = Kind == 2 ? C : Col(FColor((Palette[Hash(D.Seed, Shelf, Book) % 6] >> 16) & 255, (Palette[Hash(D.Seed, Shelf, Book) % 6] >> 8) & 255, Palette[Hash(D.Seed, Shelf, Book) % 6] & 255));
				FillRect(T, X, Y1 - Hgt, FMath::Min(X + W, T.S - (int32)P), Y1, BC, 0.6f);
				FillRect(T, X, Y1 - Hgt + (int32)P, FMath::Min(X + W, T.S - (int32)P), Y1 - Hgt + (int32)(P * 1.4f), Shade(BC, 1.5f), 0.62f);
				X += W + 1;
				++Book;
			}
		}
	}

	void TNT(FTex& T, const FMCTexDef& D, int32 Face)
	{
		const FLinearColor Red = Col(D.C0), Paper = Col(D.C1), Dark = Col(D.C2);
		const float P = T.P16();
		if (Face == 0)
		{
			for (int32 x = 0; x < T.S; ++x)
			{
				const int32 Stick = x / (T.S / 4), LX = x % (T.S / 4);
				const float Round = FMath::Sin((LX + 0.5f) / (T.S / 4) * PI);
				for (int32 y = 0; y < T.S; ++y) { const int32 I = T.I(x, y); T.C[I] = WithA(Shade(Red, 0.6f + Round * 0.5f), 1.f); T.H[I] = Round; T.R[I] = 0.6f; }
				(void)Stick;
			}
			FillRect(T, 0, (int32)(P * 5), T.S, (int32)(P * 11), Paper, 0.9f);
			Grunge(T, 0.05f, 0.02f, 8, D.Seed);
			// block letters on the label band
			const int32 Y0 = (int32)(P * 6), Y1 = (int32)(P * 10);
			auto Bar = [&](float X0, float X1, int32 YA, int32 YB) { FillRect(T, (int32)(X0 * P), YA, (int32)(X1 * P), YB, Dark, 0.95f); };
			Bar(2, 5, Y0, Y0 + (int32)P); Bar(3, 4, Y0, Y1);
			Bar(6, 7, Y0, Y1); Bar(9, 10, Y0, Y1); Line(T, 7 * P, Y0 + 0.5f, 9 * P, Y1 - 0.5f, P, Dark, 0.95f);
			Bar(11, 14, Y0, Y0 + (int32)P); Bar(12, 13, Y0, Y1);
		}
		else
		{
			T.Fill(Shade(Red, 0.8f), 0.5f, 0.6f);
			for (int32 k = 0; k < 4; ++k) for (int32 j = 0; j < 4; ++j)
			{
				const float CX = (k + 0.5f) * T.S / 4, CY = (j + 0.5f) * T.S / 4;
				Disc(T, CX, CY, T.S / 8.5f, Red, 0.8f);
				Disc(T, CX, CY, T.S / 20.f, Face == 1 ? Dark : Shade(Red, 0.6f), 0.3f);
			}
			if (Face == 1) Line(T, T.S * 0.5f, T.S * 0.5f, T.S * 0.5f + P * 1.5f, T.S * 0.5f - P * 2.f, P * 0.8f, Dark, 0.9f);
		}
	}

	void CopperBlock(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		const int32 Kind = (int32)D.P0;
		const int32 Stage = (int32)D.P1;
		const float P = T.P16();
		Smooth(T, A, B, D.Seed, 0.2f, 0.4f + Stage * 0.12f);
		for (int32 k = 0; k < T.S * T.S; ++k) T.M[k] = FMath::Max(0.f, 0.9f - Stage * 0.25f);
		// verdigris patches for later stages
		if (Stage > 0)
			for (int32 y = 0; y < T.S; ++y)
				for (int32 x = 0; x < T.S; ++x)
				{
					const float N = Fbm(T.U(x), T.U(y), 5, 4, D.Seed + 20);
					if (N > 0.7f - Stage * 0.08f) { const int32 I = T.I(x, y); T.C[I] = WithA(Mix(T.C[I], C, 0.6f), T.C[I].A); T.M[I] = 0.1f; T.R[I] = 0.85f; }
				}
		switch (Kind)
		{
		case 0: Panel(T, 0, 0, T.S, T.S, (int32)P, 0.12f); Rivets(T, Shade(B, 1.1f), 1.5f); break;
		case 1: for (int32 j = 0; j < 2; ++j) for (int32 k = 0; k < 2; ++k) Panel(T, k * T.S / 2, j * T.S / 2, (k + 1) * T.S / 2, (j + 1) * T.S / 2, (int32)P, 0.15f); break;
		case 2: Panel(T, 0, 0, T.S, T.S, (int32)P, 0.1f); Motif(T, 5, Shade(A, 0.6f), B); break;
		case 3:
			T.SetAlpha(1.f);
			Border(T, (int32)P, Shade(A, 0.9f), 0.8f);
			for (int32 j = 0; j < 4; ++j) for (int32 k = 0; k < 4; ++k) { const int32 X0 = (int32)(P * (1.5f + k * 3.4f)), Y0 = (int32)(P * (1.5f + j * 3.4f)); for (int32 y = Y0; y < Y0 + (int32)(P * 2); ++y) for (int32 x = X0; x < X0 + (int32)(P * 2); ++x) T.C[T.I(x, y)].A = 0.f; }
			break;
		case 4: case 5:
			Panel(T, 0, 0, T.S, T.S, (int32)P, 0.12f);
			FillRect(T, (int32)(P * 4), (int32)(P * 4), (int32)(P * 12), (int32)(P * 12), Kind == 5 ? C : Shade(A, 0.45f), 0.3f);
			if (Kind == 5) for (int32 y = (int32)(P * 4); y < (int32)(P * 12); ++y) for (int32 x = (int32)(P * 4); x < (int32)(P * 12); ++x) T.E[T.I(x, y)] = 0.9f;
			FillRect(T, (int32)(P * 7), (int32)(P * 2), (int32)(P * 9), (int32)(P * 14), Shade(A, 0.8f), 0.6f);
			break;
		default: break;
		}
	}

	void Crystal(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		const int32 Kind = (int32)D.P0;
		const float P = T.P16();
		const bool bTrans = (D.Flags & MCTF_Translucent) != 0;
		for (int32 y = 0; y < T.S; ++y)
			for (int32 x = 0; x < T.S; ++x)
			{
				const FCell W = Worley(T.U(x), T.U(y), Kind == 1 ? 3 : 4, D.Seed, 0.7f);
				const float F = Hash01(W.Id, 5);
				const int32 I = T.I(x, y);
				T.C[I] = FLinearColor(FMath::Lerp(A.R, B.R, F), FMath::Lerp(A.G, B.G, F), FMath::Lerp(A.B, B.B, F), bTrans ? 0.65f : 1.f);
				if (W.F2 - W.F1 < 0.06f) T.C[I] = WithA(C, T.C[I].A);
				T.H[I] = 0.4f + F * 0.4f;
				T.R[I] = 0.15f;
				T.E[I] = 0.55f + F * 0.45f;
			}
		if (Kind == 1) { Border(T, (int32)P, Shade(C, 0.9f), 0.7f); }
		if (Kind == 2) { T.Fill(A, 0.5f, 0.3f); FillRect(T, (int32)(P * 6), 0, (int32)(P * 10), T.S, B, 0.8f); for (int32 k = 0; k < T.S * T.S; ++k) T.E[k] = T.C[k].R > 0.95f ? 1.f : 0.3f; }
		if (Kind == 3) { T.SetAlpha(0.f); Disc(T, T.S * 0.5f, T.S * 0.5f, P * 4.f, C, 0.6f); Disc(T, T.S * 0.5f, T.S * 0.5f, P * 3.f, A, 0.8f); Disc(T, T.S * 0.5f, T.S * 0.5f, P * 1.4f, B, 0.9f); for (int32 k = 0; k < T.S * T.S; ++k) T.E[k] = T.C[k].A > 0.5f ? 0.8f : 0.f; }
		if (Kind == 4 || Kind == 5) { Border(T, (int32)P, WithA(Shade(A, 0.8f), 0.95f), 0.8f); FillRect(T, (int32)(P * 3), (int32)(P * 3), (int32)(P * 13), (int32)(P * 13), WithA(Shade(B, 0.95f), 0.9f), 0.6f); for (float& E : T.E) E = 0.f; for (float& R : T.R) R = 0.25f; }
		T.BakeAO = 0.1f;
	}

	void Panelled(FTex& T, const FMCTexDef& D)
	{
		// generic functional faces (P0 selects the layout); colours: C0 main, C1 secondary, C2 accent
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		const int32 Kind = (int32)D.P0;
		const float P = T.P16();
		Planks(T, 4, A, Shade(A, 0.85f), Shade(A, 0.6f), D.Seed);
		switch (Kind)
		{
		case 1: Planks(T, 4, A, Shade(A, 0.8f), B, D.Seed, true); for (int32 k = 0; k < 2; ++k) FillRect(T, 0, (int32)(P * (3 + k * 9)), T.S, (int32)(P * (4 + k * 9)), C, 0.8f); break; // barrel side
		case 2: case 3: case 4: Panel(T, 0, 0, T.S, T.S, (int32)P, 0.1f); Border(T, (int32)P, B, 0.7f); if (Kind == 3) FillRect(T, (int32)(P * 3), (int32)(P * 3), (int32)(P * 13), (int32)(P * 13), B, 0.1f); else FillRect(T, (int32)(P * 6), (int32)(P * 7), (int32)(P * 10), (int32)(P * 9), Shade(B, 0.8f), 0.4f); break;
		case 5: Smooth(T, A, Shade(A, 0.8f), D.Seed, 0.2f, 0.6f); Border(T, (int32)(P * 2), B, 0.6f); Motif(T, 3, Shade(B, 0.8f), C); for (int32 k = 0; k < T.S * T.S; ++k) if (T.C[k].B > 0.8f) T.E[k] = 0.8f; break; // enchanting top
		case 6: Smooth(T, A, B, D.Seed, 0.2f, 0.5f); FillRect(T, 0, 0, T.S, (int32)(P * 4), C, 0.8f); Grunge(T, 0.08f, 0.05f, 8, D.Seed + 3); break;
		case 7: case 8: case 9: Smooth(T, A, B, D.Seed, 0.25f, 0.5f); Border(T, (int32)P, C, 0.7f); if (Kind == 7) Motif(T, 2, B, C); else FillRect(T, (int32)(P * 2), (int32)(P * 9), (int32)(P * 14), (int32)(P * 14), Col(FColor(0x4A, 0x3A, 0x2A)), 0.6f); break;
		case 10: Smooth(T, A, B, D.Seed, 0.2f, 0.8f); FillRect(T, 0, (int32)(P * 7), T.S, T.S, C, 0.5f); break; // stonecutter side
		case 11: case 12: case 13: FillRect(T, (int32)(P * 2), (int32)(P * 2), (int32)(P * 14), (int32)(P * 8), C, 0.7f); for (int32 k = 0; k < 6; ++k) Line(T, P * (3 + k * 2), P * 2, P * (3 + k * 2), P * 8, P * 0.4f, Shade(C, 0.8f), 0.72f); break; // loom
		case 14: case 15: Planks(T, 4, A, Shade(A, 0.8f), B, D.Seed, true); Border(T, (int32)(P * 2), Shade(A, 0.75f), 0.8f); if (Kind == 15) { for (int32 y = (int32)(P * 2); y < T.S - (int32)(P * 2); ++y) for (int32 x = (int32)(P * 2); x < T.S - (int32)(P * 2); ++x) T.C[T.I(x, y)].A = 0.f; } break; // composter
		case 16: case 17: FillRect(T, (int32)(P * 2), (int32)(P * 2), (int32)(P * 14), (int32)(P * 14), Shade(A, 1.05f), 0.6f); FillRect(T, (int32)(P * 4), (int32)(P * 4), (int32)(P * 12), (int32)(P * 12), Kind == 16 ? C : Shade(B, 0.9f), 0.65f); break; // lectern
		case 18: FillRect(T, (int32)P, (int32)P, T.S - (int32)P, T.S - (int32)P, B, 0.7f); for (int32 i = 0; i < 6; ++i) Line(T, Hash01(D.Seed, i, 1) * T.S * 0.8f + P, Hash01(D.Seed, i, 2) * T.S * 0.8f + P, Hash01(D.Seed, i, 3) * T.S * 0.8f + P, Hash01(D.Seed, i, 4) * T.S * 0.8f + P, P * 0.5f, C, 0.72f); break; // map table
		case 19: case 20: case 21: FillRect(T, (int32)P, (int32)(P * 3), T.S - (int32)P, (int32)(P * 7), C, 0.7f); Line(T, P * 3, P * 12, P * 13, P * 10, P * 0.8f, B, 0.8f); break;
		case 22: case 24: Planks(T, 4, A, Shade(A, 0.85f), B, D.Seed); Border(T, (int32)P, B, 0.7f); if (Kind == 24) { Disc(T, T.S * 0.35f, T.S * 0.62f, P * 1.4f, C, 0.8f); Line(T, T.S * 0.35f + P * 1.2f, T.S * 0.62f, T.S * 0.35f + P * 1.2f, T.S * 0.25f, P * 0.6f, C, 0.8f); Line(T, T.S * 0.35f + P * 1.2f, T.S * 0.25f, T.S * 0.35f + P * 4.f, T.S * 0.35f, P * 0.6f, C, 0.8f); } break;
		case 23: Border(T, (int32)(P * 2), Shade(A, 0.9f), 0.7f); Disc(T, T.S * 0.5f, T.S * 0.5f, P * 5.f, B, 0.3f); Disc(T, T.S * 0.5f, T.S * 0.5f, P * 1.2f, Shade(B, 1.8f), 0.4f); break; // jukebox top
		default: Panel(T, 0, 0, T.S, T.S, (int32)P, 0.1f); break;
		}
	}
}

namespace MCTS
{
	void RunBuiltRecipe(FTex& T, const FMCTexDef& D)
	{
		const FLinearColor A = Col(D.C0), B = Col(D.C1), C = Col(D.C2);
		const uint32 Sd = D.Seed;
		const float P = T.P16();
		switch (D.Recipe)
		{
		case EMCTexRecipe::Ore:
			GenerateBase(T, D.Base);
			Ore(T, A, B, C, (int32)D.P0, (D.Flags & MCTF_Emissive) != 0, (D.Flags & MCTF_Metal) != 0, Sd);
			break;
		case EMCTexRecipe::MetalBlock:
		{
			const int32 Kind = (int32)D.P0;
			Smooth(T, A, B, Sd, 0.15f, 0.35f);
			for (float& M : T.M) M = 1.f;
			// brushed streaks
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float N = FbmAniso(T.U(x), T.U(y), 1, 32, 2, Sd + 3); const int32 I = T.I(x, y); T.C[I] = WithA(Shade(T.C[I], 0.92f + N * 0.16f), T.C[I].A); T.R[I] = 0.3f + N * 0.15f; }
			if (Kind == 0 || Kind == 1 || Kind == 9) { Panel(T, 0, 0, T.S, T.S, (int32)P, 0.15f, 0.2f); FillRect(T, (int32)(P * 2), (int32)(P * 7.5f), (int32)(P * 14), (int32)(P * 8.5f), Shade(C, 1.1f), 0.4f); Rivets(T, Shade(B, 1.1f), 2.f); }
			if (Kind == 1) Grunge(T, 0.12f, 0.1f, 6, Sd + 5);
			if (Kind == 2 || Kind == 3) { Grunge(T, 0.12f, 0.1f, 8, Sd + 5); Border(T, (int32)P, C, 0.4f); if (Kind == 3) FillRect(T, (int32)(P * 3), (int32)(P * 3), (int32)(P * 13), (int32)(P * 13), Shade(B, 1.1f), 0.7f); }
			if (Kind == 4) { T.SetAlpha(0.f); for (int32 i = 0; i < 16; ++i) { const float An = i / 16.f * 2 * PI; Line(T, T.S * 0.5f, T.S * 0.5f, T.S * 0.5f + FMath::Cos(An) * P * 7, T.S * 0.5f + FMath::Sin(An) * P * 7, P * 1.2f, WithA(B, 1.f), 0.8f); } Disc(T, T.S * 0.5f, T.S * 0.5f, P * 5.5f, WithA(A, 1.f), 0.7f); Disc(T, T.S * 0.5f, T.S * 0.5f, P * 1.2f, WithA(C, 1.f), 0.5f); }
			if (Kind >= 5 && Kind <= 8) { Grunge(T, 0.15f, 0.2f, 6, Sd + 5); Border(T, (int32)(P * 2), Shade(A, 1.15f), 0.75f); for (float& R : T.R) R = 0.6f; }
			T.NormalStrength = 2.f;
			break;
		}
		case EMCTexRecipe::GemBlock:
		{
			const int32 Kind = (int32)D.P0;
			T.Fill(A, 0.5f, 0.2f);
			const int32 Tiles = Kind == 1 ? 4 : 2;
			const int32 TS = T.S / Tiles;
			for (int32 j = 0; j < Tiles; ++j)
				for (int32 k = 0; k < Tiles; ++k)
				{
					const int32 X0 = k * TS, Y0 = j * TS;
					for (int32 y = 0; y < TS; ++y)
						for (int32 x = 0; x < TS; ++x)
						{
							const int32 I = T.I(X0 + x, Y0 + y);
							// four triangular facets per tile
							const float U = (x + 0.5f) / TS - 0.5f, V = (y + 0.5f) / TS - 0.5f;
							const int32 Facet = FMath::Abs(U) > FMath::Abs(V) ? (U > 0 ? 1 : 3) : (V > 0 ? 2 : 0);
							static const float Lit[4] = { 1.25f, 0.95f, 0.75f, 1.05f };
							FLinearColor Cl = Shade(Mix(A, B, 0.35f), Lit[Facet]);
							if (FMath::Max(FMath::Abs(U), FMath::Abs(V)) < 0.18f) Cl = Shade(B, 1.1f);
							T.C[I] = WithA(Cl, 1.f);
							T.H[I] = 0.9f - FMath::Max(FMath::Abs(U), FMath::Abs(V)) * 1.2f;
							T.R[I] = Kind == 1 ? 0.55f : 0.12f;
							T.E[I] = Kind == 3 ? 0.35f : 0.f;
						}
					FillRect(T, X0, Y0, X0 + TS, Y0 + (int32)(P * 0.5f), Shade(C, 1.2f), 0.2f);
					FillRect(T, X0, Y0, X0 + (int32)(P * 0.5f), Y0 + TS, Shade(C, 1.2f), 0.2f);
				}
			if (Kind == 2) Speckle(T, FLinearColor(0.95f, 0.85f, 0.4f, 1.f), 0.01f, 1, Sd + 1); // lapis pyrite flecks
			T.NormalStrength = 2.4f;
			break;
		}
		case EMCTexRecipe::RawBlock:
			Stones(T, 5, A, B, C, 0.1f, Sd, 1.f);
			Grunge(T, 0.1f, 0.2f, 8, Sd + 1);
			if (D.P0 >= 2.f) { for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float N = D.P0 >= 3.f ? Fbm(T.U(x), T.U(y), 4, 3, Sd + 9) : FbmAniso(T.U(x), T.U(y), 12, 3, 3, Sd + 9); if (N > 0.62f) { const int32 I = T.I(x, y); T.C[I] = WithA(Mix(T.C[I], Shade(B, 1.3f), 0.6f), 1.f); } } }
			T.NormalStrength = 3.f;
			break;
		case EMCTexRecipe::Bricks:
		{
			const int32 Kind = (int32)D.P0;
			const int32 Rows = Kind == 2 || Kind == 3 ? 8 : 4;
			Bricks(T, Rows, Kind == 2 || Kind == 3 ? 4 : 2, (int32)FMath::Max(1.f, P * 0.75f), A, B, 0.12f, Sd);
			if (Kind == 3) for (int32 i = 0; i < 5; ++i) Line(T, Hash01(Sd, i) * T.S, Hash01(Sd, i, 1) * T.S, Hash01(Sd, i) * T.S + P * 3, Hash01(Sd, i, 1) * T.S + P * 2, P * 0.5f, Shade(B, 0.6f), 0.15f);
			if (Kind == 1) Grunge(T, 0.08f, 0.05f, 8, Sd + 2);
			T.NormalStrength = 3.f;
			break;
		}
		case EMCTexRecipe::StoneBricks:
		{
			const int32 Kind = (int32)D.P0;
			const int32 Style = (int32)D.P1;
			Bricks(T, Style == 5 || Style == 2 ? 4 : 2, 2, (int32)FMath::Max(1.f, P * 0.75f), A, B, 0.08f, Sd, true);
			Grunge(T, 0.1f, 0.12f, 8, Sd + 1);
			if (Kind == 1) for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float N = Fbm(T.U(x), T.U(y), 3, 4, Sd + 60); if (N > 0.58f) { const int32 I = T.I(x, y); T.C[I] = WithA(Mix(T.C[I], Shade(C, 0.8f + Blocky(x, y, 2, Sd) * 0.4f), 0.85f), 1.f); } }
			if (Kind == 2) for (int32 i = 0; i < 6; ++i) { float X = Hash01(Sd, i, 1) * T.S, Y = Hash01(Sd, i, 2) * T.S; for (int32 s = 0; s < 5; ++s) { const float NX = X + (Hash01(Sd, i, s, 3) - 0.5f) * P * 4, NY = Y + (Hash01(Sd, i, s, 4) - 0.2f) * P * 3; Line(T, X, Y, NX, NY, P * 0.4f, Shade(B, 0.5f), 0.15f); X = NX; Y = NY; } }
			T.NormalStrength = 2.8f;
			break;
		}
		case EMCTexRecipe::Tiles:
			Bricks(T, 4, 4, (int32)FMath::Max(1.f, P * 0.6f), A, B, 0.1f, Sd, false);
			Grunge(T, 0.08f, 0.08f, 8, Sd + 1);
			if (D.P0 >= 1.f) for (int32 i = 0; i < 5; ++i) Line(T, Hash01(Sd, i) * T.S, Hash01(Sd, i, 1) * T.S, Hash01(Sd, i) * T.S + P * 3, Hash01(Sd, i, 1) * T.S + P * 2, P * 0.4f, Shade(B, 0.7f), 0.15f);
			break;
		case EMCTexRecipe::Polished:
		{
			const int32 Kind = (int32)D.P0;
			Smooth(T, A, B, Sd, 0.25f, 0.45f);
			Speckle(T, Mix(A, C, 0.4f), 0.01f, 1, Sd + 3);
			if (D.P1 >= 1.f || Kind == 3 || Kind == 12) Panel(T, 0, 0, T.S, T.S, (int32)P, 0.1f, 0.12f);
			if (Kind == 1) { FillRect(T, 0, (int32)(P * 7.5f), T.S, (int32)(P * 8.5f), Shade(C, 1.f), 0.3f); }
			if (Kind == 4) Motif(T, 1, Shade(A, 0.75f), B);
			if (Kind == 5) { FillRect(T, 0, (int32)(P * 7), T.S, (int32)(P * 9), Shade(C, 0.8f), 0.3f); }
			if (Kind == 6) { Border(T, (int32)(P * 2), Shade(A, 0.8f), 0.6f); }
			if (Kind == 7) { Disc(T, T.S * 0.5f, T.S * 0.5f, P * 5, Shade(A, 0.8f), 0.6f); }
			if (Kind >= 8 && Kind <= 11) // repeater / comparator tops: redstone channel + torch sockets
			{
				FillRect(T, (int32)(P * 7), (int32)(P * 2), (int32)(P * 9), (int32)(P * 14), C, 0.55f);
				Disc(T, T.S * 0.5f, P * 4, P * 1.3f, Shade(C, 1.2f), 0.8f);
				Disc(T, T.S * 0.5f, P * 12, P * 1.3f, Shade(C, 1.2f), 0.8f);
				if (Kind >= 10) Disc(T, P * 3.5f, P * 8, P * 1.1f, Shade(C, 1.1f), 0.8f), Disc(T, P * 12.5f, P * 8, P * 1.1f, Shade(C, 1.1f), 0.8f);
				if (D.Flags & MCTF_Emissive) for (int32 k = 0; k < T.S * T.S; ++k) if (T.C[k].R > 0.8f && T.C[k].G < 0.5f) T.E[k] = 0.9f;
			}
			if (Kind == 13) Grunge(T, 0.06f, 0.05f, 6, Sd + 7);
			T.NormalStrength = 1.6f;
			break;
		}
		case EMCTexRecipe::Chiseled:
			Smooth(T, A, Shade(A, 0.9f), Sd, 0.2f, 0.75f);
			Panel(T, 0, 0, T.S, T.S, (int32)P, 0.12f, 0.15f);
			Motif(T, (int32)D.P0, B, C);
			Grunge(T, 0.06f, 0.06f, 8, Sd + 1);
			T.NormalStrength = 3.f;
			break;
		case EMCTexRecipe::PillarSide:
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float F = FMath::Cos((x + 0.5f) / T.S * 2 * PI * 4) * 0.5f + 0.5f; const int32 I = T.I(x, y); T.C[I] = WithA(Mix(B, A, 0.4f + F * 0.6f), 1.f); T.H[I] = F; T.R[I] = 0.5f; }
			FillRect(T, 0, 0, T.S, (int32)P, Shade(A, 1.05f), 0.8f); FillRect(T, 0, T.S - (int32)P, T.S, T.S, Shade(A, 0.9f), 0.8f);
			break;
		case EMCTexRecipe::PillarTop:
			Smooth(T, A, B, Sd, 0.1f, 0.5f);
			Panel(T, 0, 0, T.S, T.S, (int32)P, 0.1f);
			for (int32 k = 0; k < 3; ++k) { const float R = P * (2 + k * 2); Groove(T, T.S * 0.5f - R, T.S * 0.5f - R, T.S * 0.5f + R, T.S * 0.5f - R, P * 0.5f, Shade(B, 0.8f)); Groove(T, T.S * 0.5f + R, T.S * 0.5f - R, T.S * 0.5f + R, T.S * 0.5f + R, P * 0.5f, Shade(B, 0.8f)); Groove(T, T.S * 0.5f + R, T.S * 0.5f + R, T.S * 0.5f - R, T.S * 0.5f + R, P * 0.5f, Shade(B, 0.8f)); Groove(T, T.S * 0.5f - R, T.S * 0.5f + R, T.S * 0.5f - R, T.S * 0.5f - R, P * 0.5f, Shade(B, 0.8f)); }
			break;
		case EMCTexRecipe::Wool:
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x)
			{
				const float N = Fbm(T.U(x), T.U(y), 8, 4, Sd);
				const float Fib = FMath::Abs(FMath::Sin((x + y * 0.6f + N * 12.f) * 0.9f)) * 0.5f + FMath::Abs(FMath::Sin((x * 0.7f - y + N * 10.f) * 0.8f)) * 0.5f;
				const int32 I = T.I(x, y);
				T.C[I] = FLinearColor(FMath::Lerp(A.R, B.R, Fib * 0.6f + N * 0.4f), FMath::Lerp(A.G, B.G, Fib * 0.6f + N * 0.4f), FMath::Lerp(A.B, B.B, Fib * 0.6f + N * 0.4f), 1.f);
				T.H[I] = 0.4f + Fib * 0.4f;
				T.R[I] = 0.95f;
			}
			if (D.P0 >= 1.f) Border(T, (int32)P, Shade(A, 0.85f), 0.4f);
			T.NormalStrength = 2.f;
			break;
		case EMCTexRecipe::Concrete: Smooth(T, A, B, Sd, 0.08f, 0.55f); Speckle(T, C, 0.006f, 1, Sd + 1); T.SetAlpha(1.f); T.NormalStrength = 0.8f; break;
		case EMCTexRecipe::ConcretePowder: Smooth(T, A, B, Sd, 0.15f, 0.95f); Speckle(T, C, 0.08f, 1, Sd + 1, -0.1f); Speckle(T, B, 0.06f, 1, Sd + 2, 0.1f); T.SetAlpha(1.f); T.NormalStrength = 1.5f; break;
		case EMCTexRecipe::Terracotta: Smooth(T, A, B, Sd, 0.18f, 0.8f); Speckle(T, C, 0.02f, 1, Sd + 1); if (D.Flags & MCTF_Tinted) T.SetAlpha(1.f); break;
		case EMCTexRecipe::GlazedTerracotta:
		{
			Smooth(T, B, Shade(B, 0.95f), Sd, 0.05f, 0.25f);
			// original rotational glaze ornament: quarter arcs + diamond centre
			const float Cn = T.S * 0.5f;
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x)
			{
				const float DX = x + 0.5f, DY = y + 0.5f;
				const float R1 = FMath::Sqrt(DX * DX + DY * DY) / T.S;
				const float R2 = FMath::Sqrt(FMath::Square(T.S - DX) + FMath::Square(T.S - DY)) / T.S;
				const int32 I = T.I(x, y);
				if (FMath::Abs(R1 - 0.45f) < 0.05f || FMath::Abs(R2 - 0.45f) < 0.05f) T.C[I] = WithA(C, 1.f);
				else if (FMath::Abs(R1 - 0.25f) < 0.035f || FMath::Abs(R2 - 0.25f) < 0.035f) T.C[I] = WithA(A, 1.f);
				if (FMath::Abs(DX - Cn) + FMath::Abs(DY - Cn) < P * 2.f) T.C[I] = WithA(Shade(C, 1.3f), 1.f);
			}
			T.SetAlpha(1.f);
			T.NormalStrength = 0.6f;
			break;
		}
		case EMCTexRecipe::Planks:
		{
			const int32 Kind = (int32)D.P2;
			Planks(T, 4, A, B, C, Sd, Kind == 1);
			if (Kind == 2) { for (int32 j = 0; j < 2; ++j) for (int32 k = 0; k < 2; ++k) Panel(T, k * T.S / 2, j * T.S / 2, (k + 1) * T.S / 2, (j + 1) * T.S / 2, (int32)P, 0.12f); }
			if (Kind == 3) Border(T, (int32)P, Shade(A, 0.8f), 0.7f);
			break;
		}
		case EMCTexRecipe::CraftingTop: Crafting(T, D, 0); break;
		case EMCTexRecipe::CraftingSide: Crafting(T, D, 2); break;
		case EMCTexRecipe::CraftingFront: Crafting(T, D, 1); break;
		case EMCTexRecipe::FurnaceFront: Furnace(T, D, 0); break;
		case EMCTexRecipe::FurnaceSide: Furnace(T, D, 1); break;
		case EMCTexRecipe::FurnaceTop: Furnace(T, D, 2); break;
		case EMCTexRecipe::Bookshelf: Books(T, D); break;
		case EMCTexRecipe::TNTSide: TNT(T, D, 0); break;
		case EMCTexRecipe::TNTTop: TNT(T, D, 1); break;
		case EMCTexRecipe::TNTBottom: TNT(T, D, 2); break;
		case EMCTexRecipe::Pumpkin:
		{
			const int32 Kind = (int32)D.P0;
			if (Kind == 1) { Smooth(T, A, B, Sd, 0.2f, 0.6f); for (int32 r = 0; r < 8; ++r) { const float An = r / 8.f * 2 * PI; Groove(T, T.S * 0.5f, T.S * 0.5f, T.S * 0.5f + FMath::Cos(An) * T.S * 0.6f, T.S * 0.5f + FMath::Sin(An) * T.S * 0.6f, P * 0.6f, Shade(A, 0.75f)); } Disc(T, T.S * 0.5f, T.S * 0.5f, P * 1.6f, C, 0.9f); break; }
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float Rib = FMath::Cos((x + 0.5f) / T.S * 2 * PI * 4) * 0.5f + 0.5f; const float N = Fbm(T.U(x), T.U(y), 4, 3, Sd); const int32 I = T.I(x, y); T.C[I] = WithA(Mix(Shade(A, 0.8f), B, Rib * 0.7f + N * 0.3f), 1.f); T.H[I] = Rib; T.R[I] = 0.55f; }
			if (Kind >= 2) // carved face (original: angular eyes and a zig-zag grin)
			{
				const FLinearColor F = Kind == 3 ? C : Col(D.C2);
				auto Tri = [&](float X, float Y, float S2) { for (int32 k = 0; k < (int32)(S2); ++k) Line(T, X - k * 0.5f, Y + k, X + k * 0.5f, Y + k, 1.2f, F, 0.1f); };
				Tri(P * 4.5f, P * 4.f, P * 3.f); Tri(P * 11.5f, P * 4.f, P * 3.f);
				for (int32 k = 0; k < 5; ++k) { const float X0 = P * (3 + k * 2), X1 = X0 + P * 2; Line(T, X0, P * (10 + (k & 1)), X1, P * (10 + ((k + 1) & 1)), P * 1.6f, F, 0.1f); }
				if (Kind == 3) for (int32 k = 0; k < T.S * T.S; ++k) if (T.H[k] < 0.15f) T.E[k] = 1.f;
			}
			break;
		}
		case EMCTexRecipe::Melon:
			if (D.P0 < 0.5f) { for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float St = FMath::Abs(FMath::Sin((x + FbmAniso(T.U(x), T.U(y), 2, 4, 2, Sd) * 12.f) / T.S * PI * 6)); const int32 I = T.I(x, y); T.C[I] = WithA(Mix(C, B, St), 1.f); T.H[I] = St * 0.5f + 0.3f; T.R[I] = 0.5f; } }
			else { Smooth(T, B, A, Sd, 0.2f, 0.5f); Disc(T, T.S * 0.5f, T.S * 0.5f, P * 1.5f, C, 0.7f); }
			break;
		case EMCTexRecipe::Cactus:
		{
			const int32 Kind = (int32)D.P0;
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float Rib = Kind == 0 ? FMath::Cos((x + 0.5f) / T.S * 2 * PI * 3) * 0.5f + 0.5f : 0.5f; const int32 I = T.I(x, y); T.C[I] = WithA(Mix(A, B, Rib * 0.8f), 1.f); T.H[I] = Rib; T.R[I] = 0.6f; }
			if (Kind == 0) for (int32 i = 0; i < 14; ++i) { const float X = (int32)(Hash01(Sd, i) * 3) * T.S / 3.f + T.S / 6.f, Y = Hash01(Sd, i, 1) * T.S; Disc(T, X, Y, P * 0.45f, C, 0.9f); }
			if (Kind >= 1) { Border(T, (int32)P, Shade(A, 0.8f), 0.6f); Disc(T, T.S * 0.5f, T.S * 0.5f, P * 3, Shade(B, 1.05f), 0.6f); }
			break;
		}
		case EMCTexRecipe::Hay:
		{
			const int32 Kind = (int32)D.P0;
			if (Kind == 4 || Kind == 5) // target
			{
				Smooth(T, A, Shade(A, 0.9f), Sd, 0.15f, 0.9f);
				const float Cn = T.S * 0.5f;
				for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float R = FMath::Sqrt(FMath::Square(x + 0.5f - Cn) + FMath::Square(y + 0.5f - Cn)) / P; const int32 I = T.I(x, y); if ((R < 1.8f) || (R > 3.5f && R < 5.2f) || (R > 6.8f && R < 8.2f && Kind == 4)) T.C[I] = WithA(B, 1.f); }
				break;
			}
			const bool bTop = Kind == 1 || Kind == 3;
			if (!bTop) { for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float N = FbmAniso(T.U(x), T.U(y), 24, 2, 3, Sd); const int32 I = T.I(x, y); T.C[I] = WithA(Mix(A, B, N), 1.f); T.H[I] = N; T.R[I] = 0.9f; } }
			else { for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float N = Fbm(T.U(x), T.U(y), 10, 3, Sd); const int32 I = T.I(x, y); T.C[I] = WithA(Mix(Shade(A, 0.8f), B, N), 1.f); T.H[I] = N; } }
			if (Kind == 0) for (int32 k = 0; k < 2; ++k) FillRect(T, 0, (int32)(P * (3 + k * 9)), T.S, (int32)(P * (4 + k * 9)), C, 0.8f);
			if (Kind == 2) for (int32 k = 0; k < 3; ++k) FillRect(T, 0, (int32)(P * (2 + k * 5)), T.S, (int32)(P * (2.6f + k * 5)), Shade(C, 1.f), 0.3f);
			break;
		}
		case EMCTexRecipe::Mushroom:
		{
			const int32 Kind = (int32)D.P0;
			if (Kind == 0 || Kind == 1) { Mottle(T, A, B, 4, 3, 1.2f, Sd); if (Kind == 1) for (int32 i = 0; i < 9; ++i) Disc(T, Hash01(Sd, i) * T.S, Hash01(Sd, i, 1) * T.S, P * (0.8f + Hash01(Sd, i, 2)), C, 0.8f); }
			else if (Kind == 2) { for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float N = FbmAniso(T.U(x), T.U(y), 12, 2, 3, Sd); const int32 I = T.I(x, y); T.C[I] = WithA(Mix(A, B, N), 1.f); T.H[I] = N * 0.5f + 0.3f; } }
			else { Mottle(T, A, B, 6, 3, 1.1f, Sd); Speckle(T, C, 0.03f, 1, Sd + 2); }
			break;
		}
		case EMCTexRecipe::Sponge:
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const FCell W = Worley(T.U(x), T.U(y), 7, Sd, 1.f); const int32 I = T.I(x, y); const bool bHole = W.F1 < 0.28f; T.C[I] = WithA(bHole ? C : Mix(A, B, W.F1), 1.f); T.H[I] = bHole ? 0.1f : 0.6f + W.F1 * 0.3f; T.R[I] = D.P0 > 0.5f ? 0.4f : 0.95f; }
			T.NormalStrength = 3.f;
			break;
		case EMCTexRecipe::Sculk:
		{
			const int32 Kind = (int32)D.P0;
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x)
			{
				const float N = Fbm(T.U(x), T.U(y), 5, 4, Sd);
				const FCell W = Worley(T.U(x), T.U(y), 6, Sd + 1, 0.9f);
				const int32 I = T.I(x, y);
				FLinearColor Cl = Mix(A, B, N);
				if (W.F1 < 0.12f && Hash01(W.Id) > 0.55f) { Cl = C; T.E[I] = 0.9f; }
				T.C[I] = WithA(Cl, 1.f);
				T.H[I] = 0.4f + N * 0.3f;
				T.R[I] = 0.6f;
			}
			if (Kind == 1 || Kind == 2 || Kind == 4) { FillRect(T, 0, 0, T.S, (int32)(P * (Kind == 1 ? 16 : 5)), Kind == 1 ? Shade(B, 0.95f) : B, 0.7f); if (Kind == 1) Motif(T, 0, Shade(B, 0.6f), C); }
			if (Kind == 3) FillRect(T, 0, 0, T.S, (int32)(P * 8), Shade(A, 0.8f), 0.4f);
			break;
		}
		case EMCTexRecipe::Amethyst:
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const FCell W = Worley(T.U(x), T.U(y), 5, Sd, 0.9f); const float F = Hash01(W.Id, 3); const int32 I = T.I(x, y); T.C[I] = WithA(Mix(C, Mix(A, B, F), FMath::Clamp((W.F2 - W.F1) * 6.f, 0.f, 1.f)), 1.f); T.H[I] = 0.3f + F * 0.5f; T.R[I] = 0.2f; }
			if (D.P0 >= 1.f) for (int32 i = 0; i < 6; ++i) Disc(T, Hash01(Sd, i) * T.S, Hash01(Sd, i, 1) * T.S, P * 1.1f, Shade(B, 1.2f), 0.9f);
			T.NormalStrength = 2.5f;
			break;
		case EMCTexRecipe::CopperBlock: CopperBlock(T, D); break;
		case EMCTexRecipe::Honeycomb:
		{
			const float Hx = T.S / 4.f;
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x)
			{
				const int32 Row = (int32)(y / (Hx * 0.866f));
				const float XX = x + ((Row & 1) ? Hx * 0.5f : 0.f);
				const float LX = FMath::Fmod(XX, Hx) / Hx - 0.5f, LY = FMath::Fmod(y, Hx * 0.866f) / (Hx * 0.866f) - 0.5f;
				const float D2 = FMath::Max(FMath::Abs(LX), FMath::Abs(LY) * 0.9f + FMath::Abs(LX) * 0.3f);
				const int32 I = T.I(x, y);
				T.C[I] = WithA(D2 > 0.42f ? C : Mix(A, B, 0.5f - D2), 1.f);
				T.H[I] = D2 > 0.42f ? 0.8f : 0.4f;
				T.R[I] = 0.35f;
			}
			if (D.P0 >= 1.f) Grunge(T, 0.12f, 0.1f, 6, Sd + 1);
			break;
		}
		case EMCTexRecipe::Prismarine:
		{
			const int32 Kind = (int32)D.P0;
			if (Kind == 0) { Mottle(T, A, B, 3, 4, 1.8f, Sd, 3, 0.3f); Speckle(T, C, 0.04f, 2, Sd + 1); }
			else { Bricks(T, Kind == 1 ? 4 : 2, Kind == 1 ? 2 : 2, (int32)FMath::Max(1.f, P * 0.6f), A, C, 0.1f, Sd, Kind == 1); if (Kind == 2) for (int32 j = 0; j < 2; ++j) for (int32 k = 0; k < 2; ++k) Groove(T, k * T.S / 2 + P * 2, j * T.S / 2 + P * 2, (k + 1) * T.S / 2 - P * 2, (j + 1) * T.S / 2 - P * 2, P * 0.5f, C); }
			for (float& R : T.R) R = 0.35f;
			break;
		}
		case EMCTexRecipe::SeaLantern:
			Smooth(T, A, B, Sd, 0.25f, 0.2f);
			for (int32 j = 0; j < 2; ++j) for (int32 k = 0; k < 2; ++k) Disc(T, (k + 0.5f) * T.S / 2, (j + 0.5f) * T.S / 2, P * 3.2f, B, 0.8f, P);
			Border(T, (int32)P, C, 0.6f);
			for (int32 k = 0; k < T.S * T.S; ++k) T.E[k] = FMath::Clamp((T.C[k].R + T.C[k].G + T.C[k].B) / 3.f * 1.3f - 0.2f, 0.2f, 1.f);
			break;
		case EMCTexRecipe::Coral:
			for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const FCell W = Worley(T.U(x), T.U(y), 8, Sd, 1.f); const int32 I = T.I(x, y); const float Pore = FMath::Clamp(W.F1 * 2.f, 0.f, 1.f); T.C[I] = WithA(Mix(Shade(A, 0.6f), B, Pore), 1.f); T.H[I] = Pore; T.R[I] = 0.8f; }
			T.NormalStrength = 3.5f;
			break;
		case EMCTexRecipe::Bone:
			if (D.P0 < 0.5f) { for (int32 y = 0; y < T.S; ++y) for (int32 x = 0; x < T.S; ++x) { const float N = FbmAniso(T.U(x), T.U(y), 16, 2, 3, Sd); const int32 I = T.I(x, y); T.C[I] = WithA(Mix(A, B, N), 1.f); T.H[I] = N; T.R[I] = 0.6f; } Border(T, (int32)P, C, 0.5f); }
			else { Smooth(T, A, B, Sd, 0.15f, 0.6f); for (int32 k = 0; k < 3; ++k) { const float R = P * (2 + k * 2.2f); for (int32 a = 0; a < 40; ++a) { const float A0 = a / 40.f * 2 * PI, A1 = (a + 1) / 40.f * 2 * PI; Line(T, T.S * 0.5f + FMath::Cos(A0) * R, T.S * 0.5f + FMath::Sin(A0) * R, T.S * 0.5f + FMath::Cos(A1) * R, T.S * 0.5f + FMath::Sin(A1) * R, P * 0.5f, C, 0.4f); } } }
			break;
		case EMCTexRecipe::Crystal: Crystal(T, D); break;
		case EMCTexRecipe::Purpur:
		{
			const int32 Kind = (int32)D.P0;
			if (Kind == 0 || Kind == 1) { Bricks(T, 4, 4, (int32)FMath::Max(1.f, P * 0.6f), A, B, 0.08f, Sd, false); Grunge(T, 0.06f, 0.05f, 8, Sd + 1); }
			else if (Kind == 2 || Kind == 3) { Smooth(T, A, B, Sd, 0.1f, 0.4f); Panel(T, 0, 0, T.S, T.S, (int32)P, 0.12f); if (Kind == 2) FillRect(T, 0, (int32)(P * 7), T.S, (int32)(P * 9), Shade(A, 0.7f), 0.3f); else FillRect(T, (int32)(P * 4), (int32)(P * 4), (int32)(P * 12), (int32)(P * 12), Shade(A, 1.1f), 0.7f); T.SetAlpha(1.f); }
			else { Mottle(T, A, B, 5, 3, 1.5f, Sd, 2, 0.4f); if (Kind == 5) Motif(T, 3, Shade(A, 0.7f), B); if (Kind == 6) Grunge(T, 0.15f, 0.1f, 6, Sd + 3); }
			break;
		}
		case EMCTexRecipe::Panel: Panelled(T, D); break;
		case EMCTexRecipe::Flat: T.Fill(A, 0.5f, 0.8f); break;
		case EMCTexRecipe::Plant: RunPlant(T, D); break;
		default: RunObject(T, D); break;
		}
	}
}
