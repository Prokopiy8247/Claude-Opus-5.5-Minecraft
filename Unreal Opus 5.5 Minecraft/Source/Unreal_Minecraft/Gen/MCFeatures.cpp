#include "Gen/MCFeatures.h"

namespace
{
	FORCEINLINE FMCState LogAxis(FMCState Log, int32 Axis) { return FMCBlocks::GetByState(Log).State((uint8)Axis); }

	/** Place a leaf only into air / replaceable blocks. */
	FORCEINLINE void Leaf(FMCGenWriter& W, int32 X, int32 Y, int32 Z, FMCState L)
	{
		if (!W.Inside(X, Y, Z)) return;
		const FMCState Cur = W.Get(X, Y, Z);
		if (Cur == 0 || FMCBlocks::IsReplaceable(Cur) || (FMCBlocks::Info(Cur).Flags & MCB_Plant)) W.Set(X, Y, Z, L);
	}
	/** Logs replace air, leaves, plants and snow. */
	FORCEINLINE void Log(FMCGenWriter& W, int32 X, int32 Y, int32 Z, FMCState L)
	{
		if (!W.Inside(X, Y, Z)) return;
		const FMCState Cur = W.Get(X, Y, Z);
		if (Cur == 0 || FMCBlocks::IsReplaceable(Cur) || (FMCBlocks::Info(Cur).Flags & (MCB_Plant | MCB_Leaves)) || FMCBlocks::IsFluid(Cur)) W.Set(X, Y, Z, L);
	}
	void LeafDisc(FMCGenWriter& W, FMCRandom& R, int32 CX, int32 CY, int32 Z, int32 Rad, FMCState L, bool bTrimCorners, float CornerChance = 0.5f)
	{
		for (int32 dy = -Rad; dy <= Rad; ++dy)
			for (int32 dx = -Rad; dx <= Rad; ++dx)
			{
				const bool bCorner = FMath::Abs(dx) == Rad && FMath::Abs(dy) == Rad;
				if (bCorner && Rad > 0 && (bTrimCorners || !R.Chance(CornerChance))) continue;
				Leaf(W, CX + dx, CY + dy, Z, L);
			}
	}
	void LeafBall(FMCGenWriter& W, FMCRandom& R, int32 CX, int32 CY, int32 CZ, float RX, float RZ, FMCState L, float Noise = 0.25f)
	{
		const int32 IR = FMath::CeilToInt(RX), IZ = FMath::CeilToInt(RZ);
		for (int32 dz = -IZ; dz <= IZ; ++dz)
			for (int32 dy = -IR; dy <= IR; ++dy)
				for (int32 dx = -IR; dx <= IR; ++dx)
				{
					const float D = (dx * dx + dy * dy) / (RX * RX) + (dz * dz) / (RZ * RZ);
					if (D > 1.f + (R.NextFloat() - 0.5f) * Noise) continue;
					Leaf(W, CX + dx, CY + dy, CZ + dz, L);
				}
	}
	void LogLine(FMCGenWriter& W, FMCState LogBase, FVector A, FVector B)
	{
		const FVector D = B - A;
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(D.GetAbsMax()));
		int32 Axis = 0;
		const FVector AD = D.GetAbs();
		if (AD.X > AD.Z && AD.X >= AD.Y) Axis = 1;
		else if (AD.Y > AD.Z && AD.Y > AD.X) Axis = 2;
		const FMCState L = LogAxis(LogBase, Axis);
		for (int32 i = 0; i <= Steps; ++i)
		{
			const FVector P = A + D * ((double)i / Steps);
			Log(W, MC::FloorToInt(P.X + 0.5), MC::FloorToInt(P.Y + 0.5), MC::FloorToInt(P.Z + 0.5), L);
		}
	}
	void Vines(FMCGenWriter& W, FMCRandom& R, int32 X, int32 Y, int32 Z, float Chance, int32 MaxLen)
	{
		const FMCGenBlocks& G = FMCGenBlocks::Get();
		const FMCBlock& V = FMCBlocks::GetByState(G.Vine);
		// vine bits: N=0 S=1 W=2 E=3 -> the face of the vine cell touching the leaf
		const int32 DX[4] = { 0, 0, -1, 1 }, DY[4] = { -1, 1, 0, 0 };
		for (int32 i = 0; i < 4; ++i)
		{
			if (!R.Chance(Chance)) continue;
			const int32 VX = X + DX[i], VY = Y + DY[i];
			// the vine hangs in the cell next to the leaf, attached towards the leaf (opposite direction)
			const int32 Bit = (i ^ 1);
			const int32 Len = 1 + R.NextInt(MaxLen);
			for (int32 k = 0; k < Len; ++k)
			{
				if (!W.Inside(VX, VY, Z - k) || W.Get(VX, VY, Z - k) != 0) break;
				W.Set(VX, VY, Z - k, V.State((uint8)(1 << Bit)));
			}
		}
	}
	FMCState NaturalLeaves(FMCState L) { return L; }
}

namespace MCFeatures
{
	void Tree(FMCGenWriter& W, FMCRandom& R, EMCTree Type, const FMCBlockPos& B, bool bFromSapling)
	{
		const FMCGenBlocks& G = FMCGenBlocks::Get();
		const int32 X = B.X, Y = B.Y, Z = B.Z;
		auto Trunk = [&](int32 H, FMCState LogS, int32 Size = 1)
		{
			for (int32 k = 0; k < H; ++k)
				for (int32 a = 0; a < Size; ++a)
					for (int32 b = 0; b < Size; ++b)
						Log(W, X + a, Y + b, Z + k, LogS);
		};
		switch (Type)
		{
		case EMCTree::Oak:
		case EMCTree::Birch:
		case EMCTree::TallBirch:
		case EMCTree::SwampOak:
		{
			const bool bBirch = Type != EMCTree::Oak && Type != EMCTree::SwampOak;
			int32 H = bBirch ? 5 + R.NextInt(3) : 4 + R.NextInt(3);
			if (Type == EMCTree::TallBirch) H = 9 + R.NextInt(5);
			const FMCState LogS = bBirch ? G.BirchLog : G.OakLog;
			const FMCState LeafS = bBirch ? G.BirchLeaves : G.OakLeaves;
			Trunk(H, LogS);
			const int32 Top = Z + H;
			const int32 Wide = Type == EMCTree::SwampOak ? 3 : 2;
			for (int32 dz = -3; dz <= 0; ++dz)
			{
				const int32 Rad = dz <= -2 ? Wide : 1;
				LeafDisc(W, R, X, Y, Top + dz, Rad, LeafS, dz == 0, dz == -1 ? 0.5f : 0.5f);
			}
			if (Type == EMCTree::SwampOak)
			{
				for (int32 dz = -3; dz <= -2; ++dz)
					for (int32 dy = -3; dy <= 3; ++dy)
						for (int32 dx = -3; dx <= 3; ++dx)
							if ((FMath::Abs(dx) == 3 || FMath::Abs(dy) == 3) && W.Get(X + dx, Y + dy, Top + dz) == LeafS) Vines(W, R, X + dx, Y + dy, Top + dz, 0.25f, 4);
			}
			if (Type == EMCTree::Oak && R.Chance(0.05)) { /* bee nest hook */ }
			break;
		}
		case EMCTree::FancyOak:
		{
			const int32 H = 8 + R.NextInt(6);
			Trunk(H, G.OakLog);
			const int32 NumBranches = 3 + R.NextInt(3);
			for (int32 i = 0; i < NumBranches; ++i)
			{
				const float A = R.FRange(0.f, 2.f * PI);
				const float Len = R.FRange(2.5f, 4.5f);
				const int32 BZ = Z + H / 2 + R.NextInt(H / 2);
				const FVector End(X + FMath::Cos(A) * Len, Y + FMath::Sin(A) * Len, BZ + 2 + R.NextInt(2));
				LogLine(W, G.OakLog, FVector(X, Y, BZ), End);
				LeafBall(W, R, MC::FloorToInt(End.X + 0.5), MC::FloorToInt(End.Y + 0.5), MC::FloorToInt(End.Z + 0.5), 2.6f, 1.8f, G.OakLeaves);
			}
			LeafBall(W, R, X, Y, Z + H, 3.0f, 2.2f, G.OakLeaves);
			break;
		}
		case EMCTree::Spruce:
		case EMCTree::Pine:
		{
			const bool bPine = Type == EMCTree::Pine;
			const int32 H = bPine ? 8 + R.NextInt(5) : 6 + R.NextInt(4);
			Trunk(H, G.SpruceLog);
			const int32 Top = Z + H;
			Leaf(W, X, Y, Top, G.SpruceLeaves);
			Leaf(W, X, Y, Top + 1, G.SpruceLeaves);
			if (bPine)
			{
				for (int32 dz = 0; dz < 4; ++dz)
				{
					const int32 Rad = dz == 0 ? 1 : (dz == 3 ? 1 : 2);
					LeafDisc(W, R, X, Y, Top - 1 - dz, Rad, G.SpruceLeaves, Rad == 2);
				}
			}
			else
			{
				int32 Rad = 0, MaxRad = 1;
				const int32 LeafStart = 1 + R.NextInt(2);
				for (int32 z = Top; z >= Z + LeafStart; --z)
				{
					if (Rad > 0) LeafDisc(W, R, X, Y, z, Rad, G.SpruceLeaves, true);
					else { Leaf(W, X + 1, Y, z, G.SpruceLeaves); Leaf(W, X - 1, Y, z, G.SpruceLeaves); Leaf(W, X, Y + 1, z, G.SpruceLeaves); Leaf(W, X, Y - 1, z, G.SpruceLeaves); }
					if (Rad >= MaxRad) { Rad = 0; MaxRad = FMath::Min(MaxRad + 1, 3); }
					else ++Rad;
				}
			}
			break;
		}
		case EMCTree::MegaSpruce:
		{
			const int32 H = 14 + R.NextInt(14);
			Trunk(H, G.SpruceLog, 2);
			const int32 Top = Z + H;
			for (int32 dz = 0; dz < H - 4; ++dz)
			{
				const int32 Rad = FMath::Min(1 + dz / 3, 5) - ((dz % 3 == 0) ? 1 : 0);
				for (int32 dy = -Rad; dy <= Rad + 1; ++dy)
					for (int32 dx = -Rad; dx <= Rad + 1; ++dx)
					{
						const float Cx = dx - 0.5f, Cy = dy - 0.5f;
						if (Cx * Cx + Cy * Cy > (Rad + 0.5f) * (Rad + 0.5f)) continue;
						Leaf(W, X + dx, Y + dy, Top - dz + 1, G.SpruceLeaves);
					}
			}
			for (int32 dy = -2; dy <= 3; ++dy)
				for (int32 dx = -2; dx <= 3; ++dx)
				{
					if (!W.InsideXY(X + dx, Y + dy)) continue;
					const FMCState Gr = W.Get(X + dx, Y + dy, Z - 1);
					if (Gr == G.Grass || Gr == G.Dirt) W.Set(X + dx, Y + dy, Z - 1, R.Chance(0.7) ? G.Podzol : G.CoarseDirt);
				}
			break;
		}
		case EMCTree::Jungle:
		{
			const int32 H = 4 + R.NextInt(7);
			Trunk(H, G.JungleLog);
			const int32 Top = Z + H;
			for (int32 dz = -3; dz <= 0; ++dz) LeafDisc(W, R, X, Y, Top + dz, dz <= -2 ? 2 : 1, G.JungleLeaves, dz == 0);
			for (int32 k = 0; k < H; ++k) if (R.Chance(0.3)) Vines(W, R, X, Y, Z + k, 0.5f, 1);
			break;
		}
		case EMCTree::MegaJungle:
		{
			const int32 H = 12 + R.NextInt(16);
			Trunk(H, G.JungleLog, 2);
			const int32 Top = Z + H;
			LeafBall(W, R, X, Y, Top, 4.0f, 2.2f, G.JungleLeaves, 0.3f);
			for (int32 k = 6; k < H - 3; k += 3 + R.NextInt(3))
			{
				const float A = R.FRange(0.f, 2.f * PI);
				const FVector End(X + 0.5 + FMath::Cos(A) * 4.0, Y + 0.5 + FMath::Sin(A) * 4.0, Z + k + 2);
				LogLine(W, G.JungleLog, FVector(X + 0.5, Y + 0.5, Z + k), End);
				LeafBall(W, R, MC::FloorToInt(End.X), MC::FloorToInt(End.Y), MC::FloorToInt(End.Z), 2.5f, 1.3f, G.JungleLeaves);
			}
			for (int32 k = 0; k < H; ++k)
				for (int32 a = 0; a < 2; ++a)
					for (int32 b = 0; b < 2; ++b)
						if (R.Chance(0.25)) Vines(W, R, X + a, Y + b, Z + k, 0.4f, 1);
			break;
		}
		case EMCTree::JungleBush:
		{
			Log(W, X, Y, Z, G.JungleLog);
			for (int32 dz = 0; dz <= 2; ++dz) LeafDisc(W, R, X, Y, Z + dz, 2 - dz, G.OakLeaves, false, 0.4f);
			break;
		}
		case EMCTree::Acacia:
		{
			const int32 H = 4 + R.NextInt(3);
			const int32 Bend = H - 1 - R.NextInt(2);
			const int32 Dir = R.NextInt(4);
			const int32 DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };
			int32 CX = X, CY = Y;
			for (int32 k = 0; k < H; ++k)
			{
				if (k >= Bend) { CX += DX[Dir]; CY += DY[Dir]; }
				Log(W, CX, CY, Z + k, G.AcaciaLog);
			}
			const int32 Top = Z + H;
			LeafDisc(W, R, CX, CY, Top - 1, 3, G.AcaciaLeaves, true);
			LeafDisc(W, R, CX, CY, Top, 1, G.AcaciaLeaves, false, 0.f);
			Leaf(W, CX + 2, CY, Top, G.AcaciaLeaves); Leaf(W, CX - 2, CY, Top, G.AcaciaLeaves); Leaf(W, CX, CY + 2, Top, G.AcaciaLeaves); Leaf(W, CX, CY - 2, Top, G.AcaciaLeaves);
			if (R.Chance(0.5))
			{
				const int32 Dir2 = (Dir + 1 + R.NextInt(3)) & 3;
				int32 BX = X, BY = Y;
				const int32 Start = Bend - 1 - R.NextInt(2);
				int32 BZ = Z + Start;
				for (int32 k = 0; k < 2 + R.NextInt(2); ++k) { BX += DX[Dir2]; BY += DY[Dir2]; ++BZ; Log(W, BX, BY, BZ, G.AcaciaLog); }
				LeafDisc(W, R, BX, BY, BZ, 2, G.AcaciaLeaves, true);
				LeafDisc(W, R, BX, BY, BZ + 1, 1, G.AcaciaLeaves, false, 0.f);
			}
			break;
		}
		case EMCTree::DarkOak:
		case EMCTree::PaleOak:
		{
			const bool bPale = Type == EMCTree::PaleOak;
			const FMCState LogS = bPale ? G.PaleOakLog : G.DarkOakLog;
			const FMCState LeafS = bPale ? G.PaleOakLeaves : G.DarkOakLeaves;
			const int32 H = 6 + R.NextInt(4);
			Trunk(H, LogS, 2);
			const int32 Top = Z + H;
			for (int32 dz = -2; dz <= 1; ++dz)
			{
				const int32 Rad = dz == 1 ? 2 : (dz == -2 ? 3 : 4);
				for (int32 dy = -Rad; dy <= Rad + 1; ++dy)
					for (int32 dx = -Rad; dx <= Rad + 1; ++dx)
					{
						const float Cx = dx - 0.5f, Cy = dy - 0.5f;
						if (Cx * Cx + Cy * Cy > (Rad + 0.2f) * (Rad + 0.2f) + R.NextFloat() * 2.f) continue;
						Leaf(W, X + dx, Y + dy, Top + dz, LeafS);
					}
			}
			// side branches with extra canopy
			for (int32 i = 0; i < 2; ++i)
			{
				const int32 BX = X + (R.NextBool() ? -2 : 3), BY = Y + (R.NextBool() ? -2 : 3);
				const int32 BZ = Top - 2 - R.NextInt(2);
				Log(W, BX, BY, BZ, LogS);
				LeafBall(W, R, BX, BY, BZ + 1, 2.2f, 1.2f, LeafS);
			}
			if (bPale)
			{
				for (int32 dy = -4; dy <= 5; ++dy)
					for (int32 dx = -4; dx <= 5; ++dx)
					{
						if (!R.Chance(0.3)) continue;
						for (int32 z = Top + 1; z >= Top - 3; --z)
						{
							if (W.Get(X + dx, Y + dy, z) == LeafS && W.Get(X + dx, Y + dy, z - 1) == 0)
							{
								const int32 Len = 1 + R.NextInt(3);
								for (int32 k = 1; k <= Len; ++k) if (W.Get(X + dx, Y + dy, z - k) == 0) W.Set(X + dx, Y + dy, z - k, G.PaleHangingMoss);
								break;
							}
						}
					}
				if (!bFromSapling && R.Chance(0.12)) W.Set(X, Y, Z + 2 + R.NextInt(3), G.CreakingHeart);
			}
			break;
		}
		case EMCTree::Mangrove:
		{
			const int32 RootH = 2 + R.NextInt(3);
			const int32 H = 5 + R.NextInt(4);
			// prop roots
			for (int32 i = 0; i < 4 + R.NextInt(3); ++i)
			{
				const float A = i * 1.3f + R.NextFloat();
				const float D = R.FRange(1.5f, 3.f);
				const FVector Foot(X + FMath::Cos(A) * D, Y + FMath::Sin(A) * D, Z - 1);
				const FVector Top(X, Y, Z + RootH);
				const int32 Steps = 6;
				for (int32 s = 0; s <= Steps; ++s)
				{
					const float T = (float)s / Steps;
					const FVector Pp = FMath::Lerp(Foot, Top, T) + FVector(0, 0, FMath::Sin(T * PI) * 0.8f);
					const int32 PX = MC::FloorToInt(Pp.X + 0.5), PY = MC::FloorToInt(Pp.Y + 0.5), PZ = MC::FloorToInt(Pp.Z + 0.5);
					const FMCState Cur = W.Get(PX, PY, PZ);
					if (Cur == 0 || FMCBlocks::IsFluid(Cur) || FMCBlocks::IsReplaceable(Cur)) W.Set(PX, PY, PZ, G.MangroveRoots);
					else if (Cur == G.Mud) W.Set(PX, PY, PZ, G.MuddyMangroveRoots);
				}
			}
			for (int32 k = RootH; k < RootH + H; ++k) Log(W, X, Y, Z + k, G.MangroveLog);
			const int32 Top = Z + RootH + H;
			LeafBall(W, R, X, Y, Top - 1, 3.2f, 2.0f, G.MangroveLeaves, 0.4f);
			break;
		}
		case EMCTree::Cherry:
		{
			const int32 H = 3 + R.NextInt(3);
			Trunk(H, G.CherryLog);
			const int32 NB = 1 + R.NextInt(3);
			FVector Tips[3];
			for (int32 i = 0; i < NB; ++i)
			{
				const float A = (i * 2.1f) + R.FRange(0.f, 1.f);
				const float Len = R.FRange(2.f, 4.f);
				const FVector Start(X, Y, Z + H - 1);
				const FVector Mid(X + FMath::Cos(A) * Len * 0.6, Y + FMath::Sin(A) * Len * 0.6, Z + H + 1);
				const FVector End(X + FMath::Cos(A) * Len, Y + FMath::Sin(A) * Len, Z + H + 3 + R.NextInt(2));
				LogLine(W, G.CherryLog, Start, Mid);
				LogLine(W, G.CherryLog, Mid, End);
				Tips[i] = End;
			}
			if (NB == 1) { LogLine(W, G.CherryLog, FVector(X, Y, Z + H), FVector(X, Y, Z + H + 3)); Tips[0] = FVector(X, Y, Z + H + 3); }
			for (int32 i = 0; i < NB; ++i)
			{
				const int32 TX = MC::FloorToInt(Tips[i].X + 0.5), TY = MC::FloorToInt(Tips[i].Y + 0.5), TZ = MC::FloorToInt(Tips[i].Z + 0.5);
				LeafBall(W, R, TX, TY, TZ, 4.2f, 2.3f, G.CherryLeaves, 0.35f);
				// hanging petals below the canopy edge
				for (int32 k = 0; k < 10; ++k)
				{
					const int32 HX = TX + R.Range(-4, 4), HY = TY + R.Range(-4, 4);
					for (int32 z = TZ; z > TZ - 4; --z)
						if (W.Get(HX, HY, z) == G.CherryLeaves && W.Get(HX, HY, z - 1) == 0) { W.Set(HX, HY, z - 1, G.CherryLeaves); break; }
				}
			}
			break;
		}
		case EMCTree::Azalea:
		{
			const int32 H = 3 + R.NextInt(2);
			Trunk(H, G.OakLog);
			const int32 Top = Z + H;
			for (int32 dz = -1; dz <= 1; ++dz)
			{
				const int32 Rad = dz == 1 ? 1 : 2;
				for (int32 dy = -Rad; dy <= Rad; ++dy)
					for (int32 dx = -Rad; dx <= Rad; ++dx)
					{
						if (FMath::Abs(dx) == Rad && FMath::Abs(dy) == Rad && R.Chance(0.6)) continue;
						Leaf(W, X + dx, Y + dy, Top + dz, R.Chance(0.3) ? G.FloweringAzaleaLeaves : G.AzaleaLeaves);
					}
			}
			if (W.Inside(X, Y, Z - 1) && (W.Get(X, Y, Z - 1) == G.Grass || W.Get(X, Y, Z - 1) == G.Dirt)) W.Set(X, Y, Z - 1, G.RootedDirt);
			break;
		}
		case EMCTree::RedMushroom:
		{
			const int32 H = 5 + R.NextInt(3);
			for (int32 k = 0; k < H; ++k) Log(W, X, Y, Z + k, G.MushroomStem);
			const int32 Top = Z + H;
			for (int32 dz = -3; dz <= 0; ++dz)
			{
				const int32 Rad = dz == 0 ? 1 : 2;
				for (int32 dy = -Rad; dy <= Rad; ++dy)
					for (int32 dx = -Rad; dx <= Rad; ++dx)
					{
						const bool bEdge = FMath::Abs(dx) == Rad || FMath::Abs(dy) == Rad || dz == 0;
						if (!bEdge) continue;
						if (FMath::Abs(dx) == Rad && FMath::Abs(dy) == Rad && dz < 0) continue;
						Leaf(W, X + dx, Y + dy, Top + dz, G.RedMushroomBlock);
					}
			}
			break;
		}
		case EMCTree::BrownMushroom:
		{
			const int32 H = 5 + R.NextInt(3);
			for (int32 k = 0; k < H; ++k) Log(W, X, Y, Z + k, G.MushroomStem);
			const int32 Top = Z + H;
			for (int32 dy = -3; dy <= 3; ++dy)
				for (int32 dx = -3; dx <= 3; ++dx)
				{
					if (FMath::Abs(dx) == 3 && FMath::Abs(dy) == 3) continue;
					Leaf(W, X + dx, Y + dy, Top, G.BrownMushroomBlock);
				}
			break;
		}
		case EMCTree::CrimsonFungus:
		case EMCTree::WarpedFungus:
		{
			const bool bCrimson = Type == EMCTree::CrimsonFungus;
			const FMCState Stem = bCrimson ? G.CrimsonStem : G.WarpedStem;
			const FMCState Wart = bCrimson ? G.NetherWartBlock : G.WarpedWartBlock;
			const int32 H = 4 + R.NextInt(9);
			for (int32 k = 0; k < H; ++k) Log(W, X, Y, Z + k, Stem);
			const int32 Top = Z + H;
			for (int32 dz = -4; dz <= 0; ++dz)
			{
				const int32 Rad = dz == 0 ? 1 : (dz >= -2 ? 2 : 3);
				for (int32 dy = -Rad; dy <= Rad; ++dy)
					for (int32 dx = -Rad; dx <= Rad; ++dx)
					{
						if (FMath::Abs(dx) == Rad && FMath::Abs(dy) == Rad && R.Chance(0.7)) continue;
						const bool bInner = FMath::Abs(dx) < Rad && FMath::Abs(dy) < Rad && dz < 0;
						if (bInner && R.Chance(0.6)) continue;
						Leaf(W, X + dx, Y + dy, Top + dz, R.Chance(0.06) ? G.Shroomlight : Wart);
						if (bCrimson && dz <= -3 && R.Chance(0.15))
						{
							for (int32 k = 1; k <= 1 + R.NextInt(4); ++k) if (W.Get(X + dx, Y + dy, Top + dz - k) == 0) W.Set(X + dx, Y + dy, Top + dz - k, G.WeepingVines);
						}
					}
			}
			break;
		}
		case EMCTree::Chorus:
		{
			// recursive-ish branching grown upward
			struct FTip { int32 X, Y, Z, Len, Depth; };
			TArray<FTip> Stack;
			Stack.Add({ X, Y, Z, 2 + R.NextInt(3), 0 });
			int32 Guard = 0;
			while (Stack.Num() && ++Guard < 64)
			{
				const FTip T = Stack.Pop();
				int32 CZ = T.Z;
				for (int32 k = 0; k < T.Len; ++k) { Log(W, T.X, T.Y, CZ, G.ChorusPlant); ++CZ; }
				if (T.Depth >= 4 || CZ - Z > 14) { Leaf(W, T.X, T.Y, CZ, G.ChorusFlower); continue; }
				const int32 NB = R.NextInt(3) + (T.Depth == 0 ? 1 : 0);
				if (NB == 0) { Leaf(W, T.X, T.Y, CZ, G.ChorusFlower); continue; }
				const int32 DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };
				for (int32 b = 0; b < NB; ++b)
				{
					const int32 D = R.NextInt(4);
					const int32 BX = T.X + DX[D], BY = T.Y + DY[D];
					Log(W, BX, BY, CZ - 1, G.ChorusPlant);
					Stack.Add({ BX, BY, CZ, 1 + R.NextInt(3), T.Depth + 1 });
				}
			}
			break;
		}
		default: break;
		}
	}

	void Blob(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& C, int32 Size, FMCState Block, FMCState DeepBlock, bool bOnlyStone, float AirSkip)
	{
		const FMCGenBlocks& G = FMCGenBlocks::Get();
		// Minecraft style: blocks along a random segment with varying radius
		const float Ang = R.FRange(0.f, PI);
		const float Len = Size / 8.f;
		const FVector A(C.X + FMath::Sin(Ang) * Len, C.Y + FMath::Cos(Ang) * Len, C.Z + R.Range(-2, 2));
		const FVector B(C.X - FMath::Sin(Ang) * Len, C.Y - FMath::Cos(Ang) * Len, C.Z + R.Range(-2, 2));
		const int32 Steps = FMath::Max(1, Size);
		for (int32 i = 0; i < Steps; ++i)
		{
			const float T = (float)i / Steps;
			const FVector P = FMath::Lerp(A, B, T);
			const float Rad = ((FMath::Sin(T * PI) + 1.f) * R.FRange(0.f, 1.f) * Size / 16.f + 1.f) * 0.5f;
			const int32 IR = FMath::CeilToInt(Rad);
			for (int32 dz = -IR; dz <= IR; ++dz)
				for (int32 dy = -IR; dy <= IR; ++dy)
					for (int32 dx = -IR; dx <= IR; ++dx)
					{
						const int32 X = MC::FloorToInt(P.X) + dx, Y = MC::FloorToInt(P.Y) + dy, Z = MC::FloorToInt(P.Z) + dz;
						const double FX = X + 0.5 - P.X, FY = Y + 0.5 - P.Y, FZ = Z + 0.5 - P.Z;
						if (FX * FX + FY * FY + FZ * FZ > Rad * Rad) continue;
						if (!W.Inside(X, Y, Z)) continue;
						const FMCState Cur = W.Get(X, Y, Z);
						if (Cur == 0) continue;
						const uint32 Fl = FMCBlocks::Info(Cur).Flags;
						if (bOnlyStone && !(Fl & MCB_Stone)) continue;
						if (!bOnlyStone && !(Fl & (MCB_Stone | MCB_Dirt))) continue;
						if (AirSkip > 0.f && R.Chance(AirSkip))
						{
							bool bExposed = false;
							for (int32 f = 0; f < 6 && !bExposed; ++f) { const FIntVector& D = MC::FaceDir[f]; bExposed = W.Get(X + D.X, Y + D.Y, Z + D.Z) == 0; }
							if (bExposed) continue;
						}
						const bool bDeep = Cur == G.Deepslate || Cur == G.Tuff;
						W.Set(X, Y, Z, (bDeep && DeepBlock) ? DeepBlock : Block);
					}
		}
	}

	void Disk(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& C, int32 Radius, FMCState Block, TFunctionRef<bool(FMCState)> CanReplace)
	{
		const int32 Rad = R.Range(2, Radius);
		for (int32 dy = -Rad; dy <= Rad; ++dy)
			for (int32 dx = -Rad; dx <= Rad; ++dx)
			{
				if (dx * dx + dy * dy > Rad * Rad) continue;
				for (int32 dz = -1; dz <= 1; ++dz)
				{
					const FMCState Cur = W.Get(C.X + dx, C.Y + dy, C.Z + dz);
					if (Cur != 0 && CanReplace(Cur)) W.Set(C.X + dx, C.Y + dy, C.Z + dz, Block);
				}
			}
	}

	void Geode(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& C)
	{
		const FMCGenBlocks& G = FMCGenBlocks::Get();
		const float Rad = R.FRange(4.5f, 6.f);
		const int32 IR = FMath::CeilToInt(Rad) + 1;
		for (int32 dz = -IR; dz <= IR; ++dz)
			for (int32 dy = -IR; dy <= IR; ++dy)
				for (int32 dx = -IR; dx <= IR; ++dx)
				{
					const int32 X = C.X + dx, Y = C.Y + dy, Z = C.Z + dz;
					if (!W.Inside(X, Y, Z)) continue;
					const float D = FMath::Sqrt((float)(dx * dx + dy * dy + dz * dz)) + (float)MCNoiseUtil::Hash01(0x6E0DE, X, Y, Z) * 0.6f;
					if (D > Rad + 0.5f) continue;
					const FMCState Cur = W.Get(X, Y, Z);
					if (Cur == G.Bedrock) continue;
					if (D > Rad - 0.5f) W.Set(X, Y, Z, G.SmoothBasalt);
					else if (D > Rad - 1.5f) W.Set(X, Y, Z, G.Calcite);
					else if (D > Rad - 2.5f) W.Set(X, Y, Z, R.Chance(0.083) ? G.BuddingAmethyst : G.Amethyst);
					else W.Set(X, Y, Z, 0);
				}
		// clusters on the inner shell
		for (int32 i = 0; i < 24; ++i)
		{
			const int32 X = C.X + R.Range(-IR, IR), Y = C.Y + R.Range(-IR, IR), Z = C.Z + R.Range(-IR, IR);
			if (W.Get(X, Y, Z) != 0) continue;
			for (int32 f = 0; f < 6; ++f)
			{
				const FIntVector& D = MC::FaceDir[f];
				const FMCState N = W.Get(X + D.X, Y + D.Y, Z + D.Z);
				if (N == G.BuddingAmethyst || N == G.Amethyst)
				{
					W.Set(X, Y, Z, FMCBlocks::GetByState(G.AmethystCluster).State((uint8)MC::Opposite((EMCFace)f)));
					break;
				}
			}
		}
	}

	void Iceberg(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& B)
	{
		const FMCGenBlocks& G = FMCGenBlocks::Get();
		const int32 H = R.Range(6, 20);
		const float Rad = R.FRange(3.f, 7.f);
		for (int32 dz = -H / 2; dz <= H; ++dz)
		{
			const float T = dz >= 0 ? 1.f - (float)dz / H : 1.f - (float)(-dz) / (H * 0.5f + 1);
			const float Rr = Rad * FMath::Pow(FMath::Max(0.f, T), 0.6f);
			const int32 IR = FMath::CeilToInt(Rr);
			for (int32 dy = -IR; dy <= IR; ++dy)
				for (int32 dx = -IR; dx <= IR; ++dx)
				{
					if (dx * dx + dy * dy > Rr * Rr) continue;
					const FMCState Cur = W.Get(B.X + dx, B.Y + dy, B.Z + dz);
					if (Cur == 0 || Cur == G.Water || Cur == G.Ice) W.Set(B.X + dx, B.Y + dy, B.Z + dz, R.Chance(0.12) ? G.BlueIce : (dz > H - 3 ? G.SnowBlock : G.PackedIce));
				}
		}
	}

	void IceSpike(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& B)
	{
		const FMCGenBlocks& G = FMCGenBlocks::Get();
		const bool bHuge = R.Chance(0.1);
		const int32 H = bHuge ? R.Range(25, 50) : R.Range(7, 14);
		const float Base = bHuge ? 2.5f : 1.5f;
		for (int32 dz = -2; dz <= H; ++dz)
		{
			const float Rr = Base * (1.f - (float)FMath::Max(0, dz) / H) + 0.5f;
			const int32 IR = FMath::CeilToInt(Rr);
			for (int32 dy = -IR; dy <= IR; ++dy)
				for (int32 dx = -IR; dx <= IR; ++dx)
					if (dx * dx + dy * dy <= Rr * Rr) W.Set(B.X + dx, B.Y + dy, B.Z + dz, G.PackedIce);
		}
	}

	void Fossil(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& B, bool bNether)
	{
		const FMCGenBlocks& G = FMCGenBlocks::Get();
		const int32 Len = R.Range(8, 13);
		const bool bAlongX = R.NextBool();
		for (int32 i = 0; i < Len; ++i)
		{
			const int32 X = B.X + (bAlongX ? i : 0), Y = B.Y + (bAlongX ? 0 : i);
			W.Set(X, Y, B.Z + 3, G.BoneBlock); // spine
			if (i % 2 == 0 && i > 1 && i < Len - 2)
			{
				for (int32 k = -3; k <= 3; ++k)
				{
					const int32 RX = X + (bAlongX ? 0 : k), RY = Y + (bAlongX ? k : 0);
					const int32 RZ = B.Z + 3 - FMath::Abs(k) / 2 - (FMath::Abs(k) == 3 ? 1 : 0);
					if (R.Chance(0.9)) W.Set(RX, RY, RZ, bNether ? G.BoneBlock : (R.Chance(0.1) ? G.CoalOre : G.BoneBlock));
				}
			}
		}
	}

	void CoralReef(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& B)
	{
		const FMCGenBlocks& G = FMCGenBlocks::Get();
		const int32 Kind = R.NextInt(5);
		const int32 Shape = R.NextInt(3);
		auto Put = [&](int32 X, int32 Y, int32 Z)
		{
			const FMCState Cur = W.Get(X, Y, Z);
			if (Cur != G.Water && !(FMCBlocks::Info(Cur).Flags & MCB_Waterlogged)) return;
			W.Set(X, Y, Z, G.CoralBlocks[Kind]);
			if (W.Get(X, Y, Z + 1) == G.Water && R.Chance(0.25)) W.Set(X, Y, Z + 1, R.Chance(0.2) ? G.SeaPickle : G.Corals[R.NextInt(5)]);
		};
		if (Shape == 0) // mushroom
		{
			const int32 H = R.Range(2, 4);
			for (int32 k = 0; k < H; ++k) Put(B.X, B.Y, B.Z + k);
			for (int32 dy = -2; dy <= 2; ++dy) for (int32 dx = -2; dx <= 2; ++dx) if (FMath::Abs(dx) + FMath::Abs(dy) <= 3) Put(B.X + dx, B.Y + dy, B.Z + H);
		}
		else if (Shape == 1) // tree
		{
			for (int32 k = 0; k < 3; ++k) Put(B.X, B.Y, B.Z + k);
			const int32 DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };
			for (int32 d = 0; d < 4; ++d) if (R.Chance(0.7)) for (int32 k = 1; k <= R.Range(1, 3); ++k) Put(B.X + DX[d] * k, B.Y + DY[d] * k, B.Z + 2 + k);
		}
		else // claw
		{
			for (int32 i = 0; i < 6; ++i) Put(B.X + R.Range(-1, 1), B.Y + R.Range(-1, 1), B.Z + R.Range(0, 3));
		}
	}

	void Boulder(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& B, FMCState Block)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			const int32 CX = B.X + R.Range(-1, 1), CY = B.Y + R.Range(-1, 1), CZ = B.Z + R.Range(0, 1);
			const float Rad = R.FRange(1.2f, 2.2f);
			const int32 IR = FMath::CeilToInt(Rad);
			for (int32 dz = -IR; dz <= IR; ++dz) for (int32 dy = -IR; dy <= IR; ++dy) for (int32 dx = -IR; dx <= IR; ++dx)
				if (dx * dx + dy * dy + dz * dz <= Rad * Rad) W.Set(CX + dx, CY + dy, CZ + dz, Block);
		}
	}

	void BasaltColumn(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& B, int32 Height, int32 Radius)
	{
		const FMCGenBlocks& G = FMCGenBlocks::Get();
		for (int32 dy = -Radius; dy <= Radius; ++dy)
			for (int32 dx = -Radius; dx <= Radius; ++dx)
			{
				if (dx * dx + dy * dy > Radius * Radius) continue;
				const int32 H = Height - R.NextInt(3);
				for (int32 k = 0; k < H; ++k)
				{
					const FMCState Cur = W.Get(B.X + dx, B.Y + dy, B.Z + k);
					if (Cur == 0 || FMCBlocks::IsFluid(Cur)) W.Set(B.X + dx, B.Y + dy, B.Z + k, G.Basalt);
				}
			}
	}

	void DesertWell(FMCGenWriter& W, const FMCBlockPos& B)
	{
		const FMCGenBlocks& G = FMCGenBlocks::Get();
		W.Fill(B.X - 2, B.Y - 2, B.Z - 1, B.X + 2, B.Y + 2, B.Z - 1, G.Sandstone);
		W.Fill(B.X - 2, B.Y - 2, B.Z, B.X + 2, B.Y + 2, B.Z, G.Sandstone);
		W.Set(B.X, B.Y, B.Z, G.Water);
		W.Set(B.X + 1, B.Y, B.Z, G.Water); W.Set(B.X - 1, B.Y, B.Z, G.Water); W.Set(B.X, B.Y + 1, B.Z, G.Water); W.Set(B.X, B.Y - 1, B.Z, G.Water);
		for (int32 dy = -1; dy <= 1; ++dy) for (int32 dx = -1; dx <= 1; ++dx) if (dx || dy) if (FMath::Abs(dx) + FMath::Abs(dy) == 2) W.Set(B.X + dx, B.Y + dy, B.Z + 1, G.Sandstone);
		W.Fill(B.X - 1, B.Y - 1, B.Z + 1, B.X + 1, B.Y + 1, B.Z + 1, 0);
		W.Set(B.X - 1, B.Y - 1, B.Z + 1, G.Sandstone); W.Set(B.X + 1, B.Y - 1, B.Z + 1, G.Sandstone);
		W.Set(B.X - 1, B.Y + 1, B.Z + 1, G.Sandstone); W.Set(B.X + 1, B.Y + 1, B.Z + 1, G.Sandstone);
		for (int32 k = 2; k <= 3; ++k) { W.Set(B.X - 1, B.Y - 1, B.Z + k, G.Sandstone); W.Set(B.X + 1, B.Y - 1, B.Z + k, G.Sandstone); W.Set(B.X - 1, B.Y + 1, B.Z + k, G.Sandstone); W.Set(B.X + 1, B.Y + 1, B.Z + k, G.Sandstone); }
		W.Fill(B.X - 1, B.Y - 1, B.Z + 4, B.X + 1, B.Y + 1, B.Z + 4, MCGen::S(TEXT("sandstone_slab")));
		W.Set(B.X, B.Y, B.Z + 4, G.Sandstone);
	}

	void LavaLake(FMCGenWriter& W, FMCRandom& R, const FMCBlockPos& C, FMCState Fluid)
	{
		const FMCGenBlocks& G = FMCGenBlocks::Get();
		const float RX = R.FRange(3.f, 6.f), RY = R.FRange(3.f, 6.f), RZ = R.FRange(1.5f, 2.5f);
		const int32 IX = FMath::CeilToInt(RX) + 1, IY = FMath::CeilToInt(RY) + 1, IZ = FMath::CeilToInt(RZ) + 1;
		for (int32 dz = -IZ; dz <= IZ; ++dz)
			for (int32 dy = -IY; dy <= IY; ++dy)
				for (int32 dx = -IX; dx <= IX; ++dx)
				{
					const float D = (dx * dx) / (RX * RX) + (dy * dy) / (RY * RY) + (dz * dz) / (RZ * RZ);
					const int32 X = C.X + dx, Y = C.Y + dy, Z = C.Z + dz;
					if (!W.Inside(X, Y, Z)) continue;
					const FMCState Cur = W.Get(X, Y, Z);
					if (Cur == G.Bedrock) continue;
					if (D < 1.f) W.Set(X, Y, Z, dz <= 0 ? Fluid : 0);
					else if (D < 1.4f && dz <= 0 && Cur != 0 && !(FMCBlocks::Info(Cur).Flags & MCB_Opaque)) W.Set(X, Y, Z, G.Stone);
				}
	}
}
