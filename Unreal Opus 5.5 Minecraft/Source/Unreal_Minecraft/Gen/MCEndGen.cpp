#include "Gen/MCEndGen.h"
#include "Gen/MCFeatures.h"
#include "Gen/MCStructureBuilder.h"
#include "Gen/MCNetherGen.h"
#include "Gen/MCOverworldGen.h"
#include "World/MCBlockEntity.h"

using MCGen::S;
using MCGen::WithMeta;

FMCEndGen::FMCEndGen(uint64 InSeed) : FMCWorldGenerator(InSeed, EMCDimension::End)
{
	auto SS = [this](uint64 Salt) { return MCHash::SplitMix64(Seed ^ (Salt * 0xD6E8FEB86659FD93ull)); };
	NIsland.Init(SS(1), 3, 1.0 / 160.0, 0.5);
	NDetail.Init(SS(2), 3, 1.0 / 24.0, 0.5);
	NHeight.Init(SS(3), 2, 1.0 / 90.0, 0.5);
	NBottom.Init(SS(4), 2, 1.0 / 30.0, 0.5);
	// 10 spikes on a ring of radius 42 (MC-like), heights 76..103, radii 2..4, two guarded by iron cages
	FMCRandom R(Seed ^ 0x5B1CE);
	TArray<int32> Order; for (int32 i = 0; i < 10; ++i) Order.Add(i);
	for (int32 i = 9; i > 0; --i) Order.Swap(i, R.NextInt(i + 1));
	for (int32 i = 0; i < 10; ++i)
	{
		FMCEndSpike Sp;
		const double A = 2.0 * (-PI + (PI / 10.0) * i);
		Sp.X = FMath::FloorToInt(42.0 * FMath::Cos(A));
		Sp.Y = FMath::FloorToInt(42.0 * FMath::Sin(A));
		const int32 K = Order[i];
		Sp.Radius = 2 + K / 3;
		Sp.Height = 76 + K * 3;
		Sp.bGuarded = K == 1 || K == 2;
		Spikes.Add(Sp);
	}
	CitySet.Type = TEXT("end_city"); CitySet.Spacing = 20; CitySet.Separation = 11; CitySet.Salt = 10387313; CitySet.MaxRadiusChunks = 3; CitySet.bTriangular = true;
}

TArray<FMCBlockPos> FMCEndGen::GatewayPositions() const
{
	TArray<FMCBlockPos> Out;
	FMCRandom R(Seed ^ 0x6A7E);
	TArray<int32> Idx; for (int32 i = 0; i < 20; ++i) Idx.Add(i);
	for (int32 i = 19; i > 0; --i) Idx.Swap(i, R.NextInt(i + 1));
	for (int32 i : Idx)
	{
		const double A = 2.0 * (-PI + (PI / 20.0) * i);
		Out.Add(FMCBlockPos(FMath::FloorToInt(96.0 * FMath::Cos(A)), FMath::FloorToInt(96.0 * FMath::Sin(A)), 75));
	}
	return Out;
}

int32 FMCEndGen::ExitPortalZ() const
{
	int32 T0 = MainIslandTop, B0 = 0;
	IslandColumn(0, 0, T0, B0);
	return T0 + 1;
}

bool FMCEndGen::IslandColumn(int32 X, int32 Y, int32& OutTop, int32& OutBottom) const
{
	const double Dist = FMath::Sqrt((double)X * X + (double)Y * Y);
	if (Dist < 180.0)
	{
		// main island: flat top with gentle undulation, bowl-shaped underside
		const double T = Dist / 150.0;
		const double Edge = 1.0 - T * T;
		const double N = NDetail.Sample2(X, Y);
		if (Edge + N * 0.12 <= 0.0) return false;
		OutTop = MainIslandTop + (int32)(NHeight.Sample2(X, Y) * 4.0 + N * 2.0 - T * T * 6.0);
		OutBottom = MainIslandTop - (int32)(FMath::Max(0.0, Edge + N * 0.12) * 52.0 + NBottom.Sample2(X, Y) * 4.0);
		OutBottom = FMath::Min(OutBottom, OutTop - 1);
		return OutBottom < OutTop;
	}
	if (Dist < OuterStart) return false;
	// outer islands: large noise continents with holes + scattered small islets
	const double Fade = MCNoiseUtil::SmoothStep(OuterStart, OuterStart + 200.0, Dist);
	const double V = NIsland.Sample2(X, Y) * 1.25 + 0.05 + NDetail.Sample2(X, Y) * 0.15;
	const double D = V * Fade;
	if (D <= 0.0) return false;
	const double H = FMath::Min(1.0, D * 3.0);
	OutTop = 56 + (int32)(H * 10.0 + NHeight.Sample2(X, Y) * 6.0);
	OutBottom = OutTop - (int32)(H * 26.0 + NBottom.Sample2(X, Y) * 3.0) - 1;
	return OutBottom < OutTop;
}

uint8 FMCEndGen::GetBiomeAt(int32 X, int32 Y, int32 Z) const
{
	const double Dist = FMath::Sqrt((double)X * X + (double)Y * Y);
	if (Dist < OuterStart) return (uint8)EMCBiome::TheEnd;
	int32 Top, Bottom;
	if (!IslandColumn(X, Y, Top, Bottom)) return (uint8)EMCBiome::SmallEndIslands;
	const int32 Th = Top - Bottom;
	if (Th > 20) return (uint8)EMCBiome::EndHighlands;
	if (Th > 10) return (uint8)EMCBiome::EndMidlands;
	return (uint8)EMCBiome::EndBarrens;
}

int32 FMCEndGen::GetSurfaceHeight(int32 X, int32 Y) const
{
	int32 Top, Bottom;
	return IslandColumn(X, Y, Top, Bottom) ? Top : 0;
}

void FMCEndGen::Generate(FMCChunk& C) const
{
	const FMCGenBlocks& G = FMCGenBlocks::Get();
	const int32 BX = C.Pos.MinBlockX(), BY = C.Pos.MinBlockY();
	for (int32 cy = 0; cy < 4; ++cy)
		for (int32 cx = 0; cx < 4; ++cx)
			C.FillBiomeColumn(cx, cy, GetBiomeAt(BX + cx * 4 + 2, BY + cy * 4 + 2, 60));
	for (int32 ly = 0; ly < 16; ++ly)
		for (int32 lx = 0; lx < 16; ++lx)
		{
			int32 Top, Bottom;
			if (!IslandColumn(BX + lx, BY + ly, Top, Bottom)) continue;
			for (int32 Z = FMath::Max(Bottom, 1); Z <= Top; ++Z) C.SetRaw(lx, ly, Z, G.EndStone);
		}
	// small islets in the outer void
	const double Dist = FMath::Sqrt((double)(BX + 8) * (BX + 8) + (double)(BY + 8) * (BY + 8));
	FMCGenWriter W(C);
	if (Dist > OuterStart)
	{
		for (int32 dy = -1; dy <= 1; ++dy)
			for (int32 dx = -1; dx <= 1; ++dx)
			{
				FMCRandom R = MCGen::ChunkRandom(Seed, C.Pos.X + dx, C.Pos.Y + dy, 0x1515);
				if (!R.Chance(0.07)) continue;
				const int32 IX = (C.Pos.X + dx) * 16 + R.NextInt(16), IY = (C.Pos.Y + dy) * 16 + R.NextInt(16), IZ = R.Range(55, 70);
				float Rad = R.FRange(2.5f, 6.5f);
				for (int32 Zo = 0; Rad > 0.5f; --Zo, Rad -= R.FRange(0.5f, 1.4f))
				{
					const int32 IR = FMath::CeilToInt(Rad);
					for (int32 oy = -IR; oy <= IR; ++oy) for (int32 ox = -IR; ox <= IR; ++ox)
						if (ox * ox + oy * oy <= (Rad + 1) * (Rad + 1)) W.Set(IX + ox, IY + oy, IZ + Zo, G.EndStone);
				}
			}
	}
	// main island features
	if (FMath::Abs(C.Pos.X) <= 8 && FMath::Abs(C.Pos.Y) <= 8)
	{
		BuildSpikes(W);
		// exit portal (bedrock fountain, inactive until the dragon dies)
		FMCRandom PR(1);
		FMCPieceBuilder B(W, PR, FIntVector(0, 0, ExitPortalZ()), 0);
		const FMCState Bed = G.Bedrock;
		for (int32 z = -1; z <= 0; ++z)
			for (int32 x = -4; x <= 4; ++x) for (int32 y = -4; y <= 4; ++y)
			{
				const float D2 = x * x + y * y;
				if (D2 <= 12.5f && z == -1) B.Set(x, y, z, Bed);
				if (z == 0 && D2 <= 12.5f && D2 > 6.25f) B.Set(x, y, z, Bed);
				if (z == 0 && D2 <= 6.25f) B.Set(x, y, z, 0);
			}
		// clear above and pillar
		for (int32 z = 1; z <= 6; ++z) for (int32 x = -4; x <= 4; ++x) for (int32 y = -4; y <= 4; ++y) if (x * x + y * y <= 12.5f) B.Set(x, y, z, 0);
		for (int32 z = -1; z <= 3; ++z) B.Set(0, 0, z, Bed);
		B.SetRaw(1, 0, 2, WithMeta(TEXT("torch"), 4)); B.SetRaw(-1, 0, 2, WithMeta(TEXT("torch"), 3));
		B.SetRaw(0, 1, 2, WithMeta(TEXT("torch"), 2)); B.SetRaw(0, -1, 2, WithMeta(TEXT("torch"), 1));
		// arrival obsidian platform at 100, 0 (5x5, 3 air above)
		const FMCBlockPos Pl = PlatformPos();
		for (int32 x = -2; x <= 2; ++x) for (int32 y = -2; y <= 2; ++y)
		{
			W.Set(Pl.X + x, Pl.Y + y, Pl.Z, G.Obsidian);
			for (int32 z = 1; z <= 3; ++z) W.Set(Pl.X + x, Pl.Y + y, Pl.Z + z, 0);
		}
	}
	// vegetation / cities on outer islands
	if (Dist > OuterStart - 32)
	{
		for (int32 dy = -1; dy <= 1; ++dy) for (int32 dx = -1; dx <= 1; ++dx) Decorate(W, C.Pos.X + dx, C.Pos.Y + dy);
		const int32 Rad = CitySet.MaxRadiusChunks;
		const int32 RX0 = FMath::FloorToInt((float)(C.Pos.X - Rad) / CitySet.Spacing), RX1 = FMath::FloorToInt((float)(C.Pos.X + Rad) / CitySet.Spacing);
		const int32 RY0 = FMath::FloorToInt((float)(C.Pos.Y - Rad) / CitySet.Spacing), RY1 = FMath::FloorToInt((float)(C.Pos.Y + Rad) / CitySet.Spacing);
		for (int32 ry = RY0; ry <= RY1; ++ry)
			for (int32 rx = RX0; rx <= RX1; ++rx)
			{
				FMCChunkPos SC;
				MCGen::GetStructureStartChunk(CitySet, Seed, rx, ry, SC);
				if (FMath::Abs(SC.X - C.Pos.X) > Rad || FMath::Abs(SC.Y - C.Pos.Y) > Rad) continue;
				TSharedPtr<const FMCStructureStart> St = GetCity(SC);
				if (!St || !St->bValid) continue;
				for (const FMCStructurePiece& P : St->Pieces)
				{
					if (P.Max.X < BX || P.Min.X > BX + 15 || P.Max.Y < BY || P.Min.Y > BY + 15) continue;
					FMCRandom R(P.Seed);
					P.Build(W, R);
				}
			}
	}
	C.bPopulated = true;
}

void FMCEndGen::BuildSpikes(FMCGenWriter& W) const
{
	const FMCGenBlocks& G = FMCGenBlocks::Get();
	const FMCState Bars = S(TEXT("iron_bars"));
	for (const FMCEndSpike& Sp : Spikes)
	{
		const int32 R = Sp.Radius;
		for (int32 dy = -R; dy <= R; ++dy)
			for (int32 dx = -R; dx <= R; ++dx)
			{
				if (dx * dx + dy * dy > R * R + 1) continue;
				for (int32 Z = 30; Z < Sp.Height; ++Z) W.Set(Sp.X + dx, Sp.Y + dy, Z, G.Obsidian);
			}
		W.Set(Sp.X, Sp.Y, Sp.Height, G.Bedrock);
		W.Set(Sp.X, Sp.Y, Sp.Height + 1, G.Fire);
		if (Sp.bGuarded)
		{
			for (int32 dx = -2; dx <= 2; ++dx) for (int32 dy = -2; dy <= 2; ++dy) for (int32 dz = 0; dz <= 3; ++dz)
			{
				const bool bSide = FMath::Abs(dx) == 2 || FMath::Abs(dy) == 2 || dz == 3;
				if (bSide) W.Set(Sp.X + dx, Sp.Y + dy, Sp.Height + dz, Bars);
			}
		}
		if (W.InsideXY(Sp.X, Sp.Y)) W.AddSpawn(TEXT("end_crystal"), Sp.X + 0.5, Sp.Y + 0.5, Sp.Height + 1.0, -1, TEXT("spike"));
	}
}

void FMCEndGen::Decorate(FMCGenWriter& W, int32 OCX, int32 OCY) const
{
	FMCRandom R = MCGen::ChunkRandom(Seed, OCX, OCY, 0xC407);
	const int32 BX = OCX * 16, BY = OCY * 16;
	const EMCBiome Bi = (EMCBiome)GetBiomeAt(BX + 8, BY + 8, 60);
	if (Bi != EMCBiome::EndHighlands && Bi != EMCBiome::EndMidlands) return;
	const int32 N = Bi == EMCBiome::EndHighlands ? R.Range(0, 3) : R.Range(0, 1);
	for (int32 i = 0; i < N; ++i)
	{
		const int32 X = BX + R.NextInt(16), Y = BY + R.NextInt(16);
		int32 Top, Bottom;
		if (!IslandColumn(X, Y, Top, Bottom)) continue;
		MCFeatures::Tree(W, R, EMCTree::Chorus, FMCBlockPos(X, Y, Top + 1));
	}
	// end gateway to return (rare decorative)
}

TSharedPtr<const FMCStructureStart> FMCEndGen::GetCity(const FMCChunkPos& SC) const
{
	const uint64 Key = MCHash::Hash3(0xE9DC17, SC.X, SC.Y, 1);
	if (TSharedPtr<const FMCStructureStart> Found = StructCache.Find(Key)) return Found;
	TSharedPtr<FMCStructureStart> St = MakeShared<FMCStructureStart>();
	St->Type = TEXT("end_city");
	const int32 CX = SC.MinBlockX() + 8, CY = SC.MinBlockY() + 8;
	const double Dist = FMath::Sqrt((double)CX * CX + (double)CY * CY);
	int32 Top, Bottom;
	if (Dist > OuterStart + 64 && IslandColumn(CX, CY, Top, Bottom) && Top - Bottom > 12 && Top >= 60)
	{
		// require the four corners to be land too
		bool bOK = true;
		for (int32 i = 0; i < 4 && bOK; ++i) { int32 T2, B2; bOK = IslandColumn(CX + ((i & 1) ? 6 : -6), CY + ((i & 2) ? 6 : -6), T2, B2); }
		if (bOK)
		{
			St->Origin = FMCBlockPos(CX, CY, Top + 1);
			FMCRandom R(MCHash::Hash3(Seed, SC.X, SC.Y, 0xC17));
			const int32 Floors = R.Range(3, 5);
			const bool bShip = R.Chance(0.55);
			const int32 BaseZ = Top + 1;
			FMCStructurePiece Tower;
			Tower.Min = FIntVector(CX - 8, CY - 8, BaseZ - 1); Tower.Max = FIntVector(CX + 8, CY + 8, BaseZ + Floors * 6 + 10);
			Tower.Seed = MCHash::Hash3(Seed, CX, CY, 1);
			Tower.Build = [CX, CY, BaseZ, Floors](FMCGenWriter& W, FMCRandom& RR)
			{
				FMCPieceBuilder B(W, RR, FIntVector(CX, CY, BaseZ), 0);
				const FMCState Pur = S(TEXT("purpur_block")), Pil = S(TEXT("purpur_pillar")), ESB = S(TEXT("end_stone_bricks")), Glass = S(TEXT("magenta_stained_glass"));
				const FMCState Rod = WithMeta(TEXT("end_rod"), 1), PurStairs = S(TEXT("purpur_stairs"));
				// base house
				B.Fill(-5, -5, -1, 5, 5, -1, ESB);
				for (int32 Fl = 0; Fl < Floors; ++Fl)
				{
					const int32 Z0 = Fl * 6;
					const int32 Rr = Fl == 0 ? 5 : 3;
					for (int32 x = -Rr; x <= Rr; ++x) for (int32 y = -Rr; y <= Rr; ++y) for (int32 z = Z0; z < Z0 + 6; ++z)
					{
						const bool bWall = FMath::Abs(x) == Rr || FMath::Abs(y) == Rr;
						const bool bFloor = z == Z0;
						const bool bCorner = FMath::Abs(x) == Rr && FMath::Abs(y) == Rr;
						FMCState Bl = 0;
						if (bCorner) Bl = Pil;
						else if (bWall) Bl = ((z == Z0 + 2 || z == Z0 + 3) && (x == 0 || y == 0)) ? Glass : Pur;
						else if (bFloor) Bl = Pur;
						B.Set(x, y, z, Bl);
					}
					// ladder shaft
					for (int32 z = Z0; z < Z0 + 6; ++z) B.Set(0, Rr - 1, z, WithMeta(TEXT("ladder"), 0));
					B.Set(0, Rr - 1, Z0, WithMeta(TEXT("ladder"), 0));
					if (Fl > 0) B.Set(0, Rr - 1, Z0, WithMeta(TEXT("ladder"), 0));
					B.SetRaw(-(Rr - 1), -(Rr - 1), Z0 + 5 - 1, WithMeta(TEXT("end_rod"), 0));
					if (Fl > 0) B.Mob(TEXT("shulker"), (float)(Rr - 1), -(float)(Rr - 1), Z0 + 1);
					if (Fl == 1) B.Chest(-(Rr - 1), 0, Z0 + 1, EMCFace::East, TEXT("end_city_treasure"));
				}
				const int32 TopZ = Floors * 6;
				// roof crown with end rods
				B.Fill(-4, -4, TopZ, 4, 4, TopZ, Pur);
				for (int32 x = -4; x <= 4; x += 8) for (int32 y = -4; y <= 4; y += 8) { B.Set(x, y, TopZ + 1, Pil); B.SetRaw(x, y, TopZ + 2, Rod); }
				for (int32 x = -3; x <= 3; ++x) { B.Stair(x, -4, TopZ + 1, PurStairs, EMCFace::South); B.Stair(x, 4, TopZ + 1, PurStairs, EMCFace::North); }
				B.Chest(0, 0, TopZ + 1, EMCFace::South, TEXT("end_city_treasure"));
				B.Mob(TEXT("shulker"), 2, 2, TopZ + 1);
				// doorway
				B.Fill(-1, -5, 0, 1, -5, 2, 0);
				B.Fill(-1, -5, 0, 1, -5, 0, Pur);
			};
			St->Pieces.Add(MoveTemp(Tower));
			St->Min = FIntVector(CX - 8, CY - 8, BaseZ - 1); St->Max = FIntVector(CX + 8, CY + 8, BaseZ + Floors * 6 + 10);
			if (bShip)
			{
				// end ship floating beside the city
				const int32 SX = CX + 22, SY = CY, SZ = BaseZ + Floors * 6 - 4;
				FMCStructurePiece Ship;
				Ship.Min = FIntVector(SX - 5, SY - 14, SZ - 4); Ship.Max = FIntVector(SX + 5, SY + 14, SZ + 12);
				Ship.Seed = MCHash::Hash3(Seed, SX, SY, 2);
				Ship.Build = [SX, SY, SZ](FMCGenWriter& W, FMCRandom& RR)
				{
					FMCPieceBuilder B(W, RR, FIntVector(SX, SY, SZ), 0);
					const FMCState Pur = S(TEXT("purpur_block")), Pil = S(TEXT("purpur_pillar")), Slab = S(TEXT("purpur_slab")), Obs = S(TEXT("obsidian"));
					const FMCState Glass = S(TEXT("magenta_stained_glass")), Rod = WithMeta(TEXT("end_rod"), 1);
					for (int32 y = -12; y <= 12; ++y)
					{
						const int32 Hw = y < -8 ? FMath::Max(0, 3 - (-8 - y)) : (y > 9 ? FMath::Max(0, 3 - (y - 9)) : 3);
						for (int32 x = -Hw; x <= Hw; ++x)
						{
							B.Set(x, y, 0, Pur);
							if (FMath::Abs(x) == Hw) { B.Set(x, y, 1, Pur); B.Set(x, y, 2, Slab); }
						}
						const int32 KeelW = FMath::Max(0, Hw - 1);
						for (int32 x = -KeelW; x <= KeelW; ++x) B.Set(x, y, -1, Pur);
						if (FMath::Abs(y) < 8) B.Set(0, y, -2, Obs);
					}
					for (int32 z = 1; z <= 10; ++z) B.Set(0, 2, z, Pil);
					for (int32 x = -3; x <= 3; ++x) for (int32 z = 5; z <= 9; ++z) if (FMath::Abs(x) > 0) B.Set(x, 2, z, S(TEXT("black_wool")));
					B.SetRaw(0, 2, 11, Rod);
					// dragon head at the bow
					B.Set(0, -13, 1, Obs); B.SetRaw(0, -13, 2, WithMeta(TEXT("dragon_head"), 0));
					// cabin with treasure and elytra frame
					B.Fill(-2, 6, 1, 2, 10, 4, Pur);
					B.Fill(-1, 7, 1, 1, 9, 3, 0);
					B.Set(0, 6, 1, 0); B.Set(0, 6, 2, 0);
					B.Set(-2, 8, 2, Glass); B.Set(2, 8, 2, Glass);
					B.Chest(-1, 9, 1, EMCFace::East, TEXT("end_city_treasure"));
					B.Chest(1, 9, 1, EMCFace::West, TEXT("end_city_treasure"));
					B.Mob(TEXT("item_frame"), 0, 9, 2, 3, TEXT("elytra"));
					B.Mob(TEXT("shulker"), 0, 8, 1);
					B.Mob(TEXT("shulker"), 0, -6, 1);
				};
				St->Pieces.Add(MoveTemp(Ship));
				St->Max = FIntVector(FMath::Max(St->Max.X, SX + 5), FMath::Max(St->Max.Y, SY + 14), FMath::Max(St->Max.Z, SZ + 12));
				St->Min = FIntVector(FMath::Min(St->Min.X, SX - 5), FMath::Min(St->Min.Y, SY - 14), FMath::Min(St->Min.Z, SZ - 4));
			}
			St->bValid = true;
		}
	}
	StructCache.Add(Key, St);
	return St;
}

bool FMCEndGen::LocateStructure(FName Type, const FMCBlockPos& Origin, int32 MaxChunks, FMCBlockPos& Out) const
{
	if (Type != TEXT("end_city")) return false;
	const FMCChunkPos OC = FMCChunkPos::FromBlock(Origin);
	const int32 ORX = FMath::FloorToInt((float)OC.X / CitySet.Spacing), ORY = FMath::FloorToInt((float)OC.Y / CitySet.Spacing);
	int64 Best = TNumericLimits<int64>::Max();
	const int32 MaxR = FMath::Max(4, MaxChunks / CitySet.Spacing + 1);
	for (int32 Rr = 0; Rr <= MaxR; ++Rr)
	{
		for (int32 ry = -Rr; ry <= Rr; ++ry)
			for (int32 rx = -Rr; rx <= Rr; ++rx)
			{
				if (FMath::Max(FMath::Abs(rx), FMath::Abs(ry)) != Rr) continue;
				FMCChunkPos SC;
				MCGen::GetStructureStartChunk(CitySet, Seed, ORX + rx, ORY + ry, SC);
				TSharedPtr<const FMCStructureStart> St = GetCity(SC);
				if (!St || !St->bValid) continue;
				const int64 D = (int64)(St->Origin.X - Origin.X) * (St->Origin.X - Origin.X) + (int64)(St->Origin.Y - Origin.Y) * (St->Origin.Y - Origin.Y);
				if (D < Best) { Best = D; Out = St->Origin; }
			}
		if (Best != TNumericLimits<int64>::Max() && Rr >= 1) return true;
	}
	return Best != TNumericLimits<int64>::Max();
}

// ---------------------------------------------------------------------------------------------------------------------

TSharedPtr<FMCWorldGenerator> FMCWorldGenerator::Create(EMCDimension Dim, uint64 Seed)
{
	switch (Dim)
	{
	case EMCDimension::Nether: return MakeShared<FMCNetherGen>(Seed);
	case EMCDimension::End: return MakeShared<FMCEndGen>(Seed);
	default: return MakeShared<FMCOverworldGen>(Seed);
	}
}
