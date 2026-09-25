#include "Gen/MCNetherGen.h"
#include "Gen/MCFeatures.h"
#include "Gen/MCStructureBuilder.h"
#include "World/MCBlockEntity.h"

using MCGen::S;
using MCGen::WithMeta;

namespace
{
	constexpr int32 NGridXY = 4;
	constexpr int32 NGridZ = 8;
	constexpr int32 NZNodes = 128 / NGridZ + 1; // 17

	struct FNetherBiomePoint { EMCBiome Biome; float T, H; };
	const FNetherBiomePoint GNetherPoints[] = {
		{ EMCBiome::NetherWastes, 0.f, 0.f },
		{ EMCBiome::SoulSandValley, 0.f, -0.5f },
		{ EMCBiome::CrimsonForest, 0.4f, 0.f },
		{ EMCBiome::WarpedForest, 0.f, 0.5f },
		{ EMCBiome::BasaltDeltas, -0.5f, 0.f },
	};
}

FMCNetherGen::FMCNetherGen(uint64 InSeed) : FMCWorldGenerator(InSeed, EMCDimension::Nether)
{
	auto SS = [this](uint64 Salt) { return MCHash::SplitMix64(Seed ^ (Salt * 0xA24BAED4963EE407ull)); };
	NTerrain.Init(SS(1), 4, 1.0 / 90.0, 0.5);
	NTerrain2.Init(SS(2), 3, 1.0 / 38.0, 0.5);
	NTemp.Init(SS(3), 2, 1.0 / 260.0, 0.5);
	NHumid.Init(SS(4), 2, 1.0 / 260.0, 0.5);
	NSurface.Init(SS(5), 2, 1.0 / 24.0, 0.5);
	NPatch.Init(SS(6), 2, 1.0 / 16.0, 0.5);
	NDelta.Init(SS(7), 2, 1.0 / 12.0, 0.5);
	FMCStructureSet Fort; Fort.Type = TEXT("fortress"); Fort.Spacing = 27; Fort.Separation = 4; Fort.Salt = 30084232; Fort.MaxRadiusChunks = 7;
	FMCStructureSet Bast; Bast.Type = TEXT("bastion_remnant"); Bast.Spacing = 27; Bast.Separation = 4; Bast.Salt = 30084233; Bast.MaxRadiusChunks = 3;
	StructureSets.Add(Fort);
	StructureSets.Add(Bast);
}

double FMCNetherGen::Density(double X, double Y, double Z) const
{
	// open caverns: positive = solid
	double D = NTerrain.Sample3(X, Y, Z * 1.7) * 0.9 + NTerrain2.Sample3(X, Y, Z) * 0.35;
	// solid floor below ~30 and roof above ~100, with smooth transitions
	D += MCNoiseUtil::ClampedMap(Z, 8.0, 34.0, 1.3, -0.15);
	D += MCNoiseUtil::ClampedMap(Z, 96.0, 124.0, -0.1, 1.6);
	return D;
}

uint8 FMCNetherGen::GetBiomeAt(int32 X, int32 Y, int32 Z) const
{
	const float T = (float)NTemp.Sample3(X, Y, Z * 0.5) * 1.6f;
	const float H = (float)NHumid.Sample3(X + 1234, Y - 777, Z * 0.5) * 1.6f;
	float Best = 1e9f; EMCBiome Pick = EMCBiome::NetherWastes;
	for (const FNetherBiomePoint& P : GNetherPoints)
	{
		const float D = FMath::Square(T - P.T) + FMath::Square(H - P.H);
		if (D < Best) { Best = D; Pick = P.Biome; }
	}
	return (uint8)Pick;
}

int32 FMCNetherGen::GetSurfaceHeight(int32 X, int32 Y) const
{
	for (int32 Z = 100; Z > 32; Z -= 2) if (Density(X, Y, Z) > 0 && Density(X, Y, Z + 2) <= 0) return Z;
	return 64;
}

void FMCNetherGen::Generate(FMCChunk& C) const
{
	const FMCGenBlocks& G = FMCGenBlocks::Get();
	const int32 BX = C.Pos.MinBlockX(), BY = C.Pos.MinBlockY();
	// density grid 5 x 5 x 17
	double Grid[5][5][NZNodes];
	for (int32 gx = 0; gx < 5; ++gx)
		for (int32 gy = 0; gy < 5; ++gy)
			for (int32 gz = 0; gz < NZNodes; ++gz)
				Grid[gx][gy][gz] = Density(BX + gx * NGridXY, BY + gy * NGridXY, gz * NGridZ);

	// biomes: 4x4 cells per column, per 4 blocks in height (only 0..127 matters)
	uint8 ColBiome[4][4];
	for (int32 cy = 0; cy < 4; ++cy)
		for (int32 cx = 0; cx < 4; ++cx)
		{
			ColBiome[cx][cy] = GetBiomeAt(BX + cx * 4 + 2, BY + cy * 4 + 2, 64);
			for (int32 cz = 0; cz < MC::WorldHeight / 4; ++cz)
			{
				const int32 Z = MC::MinZ + cz * 4 + 2;
				C.SetBiomeCell(cx, cy, cz, (Z >= 0 && Z < 128) ? GetBiomeAt(BX + cx * 4 + 2, BY + cy * 4 + 2, Z) : ColBiome[cx][cy]);
			}
		}

	for (int32 lx = 0; lx < 16; ++lx)
	{
		const int32 gx = lx / NGridXY; const double fx = (lx % NGridXY) / (double)NGridXY;
		for (int32 ly = 0; ly < 16; ++ly)
		{
			const int32 gy = ly / NGridXY; const double fy = (ly % NGridXY) / (double)NGridXY;
			const int32 X = BX + lx, Y = BY + ly;
			const EMCBiome Bi = (EMCBiome)ColBiome[lx >> 2][ly >> 2];
			const double SurfN = NSurface.Sample2(X, Y);
			for (int32 Z = 0; Z <= RoofZ; ++Z)
			{
				const int32 gz = Z / NGridZ; const double fz = (Z % NGridZ) / (double)NGridZ;
				const double D00 = FMath::Lerp(Grid[gx][gy][gz], Grid[gx + 1][gy][gz], fx);
				const double D10 = FMath::Lerp(Grid[gx][gy + 1][gz], Grid[gx + 1][gy + 1][gz], fx);
				const double D01 = FMath::Lerp(Grid[gx][gy][gz + 1], Grid[gx + 1][gy][gz + 1], fx);
				const double D11 = FMath::Lerp(Grid[gx][gy + 1][gz + 1], Grid[gx + 1][gy + 1][gz + 1], fx);
				const double D = FMath::Lerp(FMath::Lerp(D00, D10, fy), FMath::Lerp(D01, D11, fy), fz);
				FMCState Bl = 0;
				if (D > 0) Bl = G.Netherrack;
				else if (Z <= LavaLevel) Bl = G.Lava;
				// bedrock floor / roof with ragged edges
				const double BR = MCNoiseUtil::Hash01(Seed ^ 0xBED, X, Y, Z);
				if (Z <= 4 && BR < 1.0 - Z * 0.2) Bl = G.Bedrock;
				if (Z >= RoofZ - 4 && BR < 1.0 - (RoofZ - Z) * 0.2) Bl = G.Bedrock;
				if (Bl) C.SetRaw(lx, ly, Z, Bl);
			}
			// surface rules per biome (top exposed netherrack)
			for (int32 Z = RoofZ - 5; Z > 5; --Z)
			{
				const FMCState Cur = C.Get(lx, ly, Z);
				if (Cur != G.Netherrack) continue;
				const FMCState Above = C.Get(lx, ly, Z + 1);
				const bool bFloor = Above == 0;
				const FMCState Below = C.Get(lx, ly, Z - 1);
				const bool bCeil = Below == 0 || Below == G.Lava;
				switch (Bi)
				{
				case EMCBiome::CrimsonForest:
					if (bFloor) C.SetRaw(lx, ly, Z, G.CrimsonNylium);
					break;
				case EMCBiome::WarpedForest:
					if (bFloor) C.SetRaw(lx, ly, Z, G.WarpedNylium);
					break;
				case EMCBiome::SoulSandValley:
					if (bFloor || (Above == G.SoulSand || Above == G.SoulSoil))
						C.SetRaw(lx, ly, Z, (SurfN + MCNoiseUtil::Hash01(Seed, X, Y, Z) * 0.3 > 0.1) ? G.SoulSand : G.SoulSoil);
					else if (bCeil && SurfN > 0.3) C.SetRaw(lx, ly, Z, G.SoulSoil);
					break;
				case EMCBiome::BasaltDeltas:
					if (bFloor || bCeil) C.SetRaw(lx, ly, Z, NPatch.Sample2(X, Y) > 0.1 ? G.Blackstone : G.Basalt);
					else if (NPatch.Sample3(X, Y, Z) > 0.25) C.SetRaw(lx, ly, Z, G.Basalt);
					break;
				default:
					// nether wastes: gravel/soul sand near the lava shoreline
					if (bFloor && Z >= LavaLevel - 1 && Z <= LavaLevel + 2)
					{
						const double N = NPatch.Sample2(X, Y);
						if (N > 0.35) C.SetRaw(lx, ly, Z, G.Gravel);
						else if (N < -0.4) C.SetRaw(lx, ly, Z, G.SoulSand);
					}
					break;
				}
			}
		}
	}

	FMCGenWriter W(C);
	for (int32 dy = -1; dy <= 1; ++dy)
		for (int32 dx = -1; dx <= 1; ++dx)
			Decorate(W, C.Pos.X + dx, C.Pos.Y + dy);
	PlaceStructures(W, C);
	C.bPopulated = true;
}

void FMCNetherGen::Decorate(FMCGenWriter& W, int32 OCX, int32 OCY) const
{
	const FMCGenBlocks& G = FMCGenBlocks::Get();
	const bool bOwn = W.C.Pos.X == OCX && W.C.Pos.Y == OCY;
	const int32 BX = OCX * 16, BY = OCY * 16;
	auto FloorAt = [&](int32 X, int32 Y, int32 StartZ, int32& OutZ) -> bool
	{
		// find the first solid block with air above scanning downward (only inside our chunk)
		for (int32 Z = StartZ; Z > 6; --Z)
		{
			const FMCState S0 = W.Get(X, Y, Z), S1 = W.Get(X, Y, Z + 1);
			if (S0 != 0 && S0 != G.Lava && FMCBlocks::IsOpaque(S0) && S1 == 0) { OutZ = Z; return true; }
		}
		return false;
	};

	// ores and blobs (own chunk only - they are small)
	if (bOwn)
	{
		FMCRandom R = MCGen::ChunkRandom(Seed, OCX, OCY, 0x0E5);
		for (int32 i = 0; i < 16; ++i) MCFeatures::Blob(W, R, FMCBlockPos(BX + R.NextInt(16), BY + R.NextInt(16), R.Range(10, 117)), 12, G.NetherQuartzOre, G.NetherQuartzOre, false);
		for (int32 i = 0; i < 10; ++i) MCFeatures::Blob(W, R, FMCBlockPos(BX + R.NextInt(16), BY + R.NextInt(16), R.Range(10, 117)), 8, G.NetherGoldOre, G.NetherGoldOre, false);
		if (R.Chance(0.9)) MCFeatures::Blob(W, R, FMCBlockPos(BX + R.NextInt(16), BY + R.NextInt(16), R.Range(8, 24)), 3, G.AncientDebris, G.AncientDebris, false, 1.f);
		if (R.Chance(0.5)) MCFeatures::Blob(W, R, FMCBlockPos(BX + R.NextInt(16), BY + R.NextInt(16), R.Range(8, 119)), 2, G.AncientDebris, G.AncientDebris, false, 1.f);
		for (int32 i = 0; i < 4; ++i) MCFeatures::Blob(W, R, FMCBlockPos(BX + R.NextInt(16), BY + R.NextInt(16), R.Range(27, 36)), 20, G.Magma, G.Magma, false);
		for (int32 i = 0; i < 2; ++i) MCFeatures::Blob(W, R, FMCBlockPos(BX + R.NextInt(16), BY + R.NextInt(16), R.Range(5, 40)), 20, G.Gravel, G.Gravel, false);
		for (int32 i = 0; i < 2; ++i) MCFeatures::Blob(W, R, FMCBlockPos(BX + R.NextInt(16), BY + R.NextInt(16), R.Range(5, 120)), 20, G.Blackstone, G.Blackstone, false);
		// lava springs in walls
		for (int32 i = 0; i < 8; ++i)
		{
			const int32 X = BX + R.NextInt(16), Y = BY + R.NextInt(16), Z = R.Range(10, 118);
			if (W.Get(X, Y, Z) != G.Netherrack) continue;
			int32 Open = 0;
			for (int32 F = 2; F < 6; ++F) { const FIntVector& D = MC::FaceDir[F]; if (W.Get(X + D.X, Y + D.Y, Z) == 0) ++Open; }
			if (Open == 1 && W.Get(X, Y, Z + 1) != 0 && W.Get(X, Y, Z - 1) != 0) { W.Set(X, Y, Z, G.Lava); W.AddTick(X, Y, Z); }
		}
	}

	// glowstone clusters hanging from ceilings (may cross borders)
	{
		FMCRandom R = MCGen::ChunkRandom(Seed, OCX, OCY, 0x61A);
		const int32 N = R.Range(4, 12);
		for (int32 i = 0; i < N; ++i)
		{
			const int32 X = BX + R.NextInt(16), Y = BY + R.NextInt(16);
			const int32 StartZ = R.Range(40, 122);
			const int32 Count = R.Range(30, 90);
			for (int32 k = 0; k < Count; ++k)
			{
				const int32 GX = X + R.Range(-3, 3), GY = Y + R.Range(-3, 3), GZ = StartZ - R.Range(0, 6);
				if (!W.Inside(GX, GY, GZ) || W.Get(GX, GY, GZ) != 0) continue;
				int32 Adj = 0;
				for (int32 F = 0; F < 6; ++F) { const FIntVector& D = MC::FaceDir[F]; const FMCState Nb = W.Get(GX + D.X, GY + D.Y, GZ + D.Z); if (Nb == G.Glowstone || Nb == G.Netherrack) ++Adj; }
				if (Adj >= 1 && (W.Get(GX, GY, GZ + 1) == G.Netherrack || W.Get(GX, GY, GZ + 1) == G.Glowstone || R.Chance(0.3))) W.Set(GX, GY, GZ, G.Glowstone);
			}
		}
	}

	// biome vegetation / terrain features
	FMCRandom R = MCGen::ChunkRandom(Seed, OCX, OCY, 0x7E6);
	const EMCBiome Bi = (EMCBiome)GetBiomeAt(BX + 8, BY + 8, 64);
	switch (Bi)
	{
	case EMCBiome::CrimsonForest:
	case EMCBiome::WarpedForest:
	{
		const bool bCrimson = Bi == EMCBiome::CrimsonForest;
		const FMCState Nylium = bCrimson ? G.CrimsonNylium : G.WarpedNylium;
		const int32 Trees = R.Range(4, 8);
		for (int32 i = 0; i < Trees; ++i)
		{
			const int32 X = BX + R.NextInt(16), Y = BY + R.NextInt(16);
			const int32 StartZ = R.Range(40, 110);
			for (int32 Z = StartZ; Z > 30; --Z)
			{
				// can only inspect our own chunk; features whose base is outside are placed by their own chunk
				const FMCState S0 = W.Get(X, Y, Z);
				if (!W.InsideXY(X, Y)) break;
				if (S0 == Nylium && W.Get(X, Y, Z + 1) == 0)
				{
					MCFeatures::Tree(W, R, bCrimson ? EMCTree::CrimsonFungus : EMCTree::WarpedFungus, FMCBlockPos(X, Y, Z + 1));
					break;
				}
			}
		}
		if (bOwn)
		{
			for (int32 i = 0; i < 90; ++i)
			{
				const int32 X = BX + R.NextInt(16), Y = BY + R.NextInt(16);
				int32 Z;
				if (!FloorAt(X, Y, R.Range(35, 120), Z)) continue;
				if (W.Get(X, Y, Z) != Nylium) continue;
				const float Roll = R.NextFloat();
				FMCState P;
				if (bCrimson) P = Roll < 0.6f ? G.CrimsonRoots : (Roll < 0.85f ? G.CrimsonFungus : G.WarpedFungus);
				else P = Roll < 0.5f ? G.WarpedRoots : (Roll < 0.8f ? G.NetherSprouts : (Roll < 0.95f ? G.WarpedFungus : G.CrimsonFungus));
				W.Set(X, Y, Z + 1, P);
				if (!bCrimson && R.Chance(0.08)) for (int32 k = 1; k <= R.Range(1, 8); ++k) { if (W.Get(X, Y, Z + k) != 0 && k > 1) break; W.Set(X, Y, Z + k, G.TwistingVines); }
			}
			// weeping vines from ceilings
			if (bCrimson)
				for (int32 i = 0; i < 12; ++i)
				{
					const int32 X = BX + R.NextInt(16), Y = BY + R.NextInt(16);
					for (int32 Z = 120; Z > 40; --Z)
					{
						if (W.Get(X, Y, Z) == G.Netherrack && W.Get(X, Y, Z - 1) == 0)
						{
							for (int32 k = 1; k <= R.Range(2, 9); ++k) { if (W.Get(X, Y, Z - k) != 0) break; W.Set(X, Y, Z - k, G.WeepingVines); }
							break;
						}
					}
				}
		}
		break;
	}
	case EMCBiome::SoulSandValley:
		if (bOwn)
		{
			// basalt pillars and fossils
			for (int32 i = 0; i < 2; ++i)
			{
				const int32 X = BX + R.NextInt(16), Y = BY + R.NextInt(16);
				int32 Z;
				if (FloorAt(X, Y, 110, Z)) for (int32 k = 1; k < 40; ++k) { if (W.Get(X, Y, Z + k) != 0) break; W.Set(X, Y, Z + k, MCGen::WithMeta(TEXT("basalt"), 0)); }
			}
			if (R.Chance(0.12)) { int32 Z; const int32 X = BX + R.Range(3, 12), Y = BY + R.Range(3, 12); if (FloorAt(X, Y, 90, Z)) MCFeatures::Fossil(W, R, FMCBlockPos(X - 4, Y - 4, Z - 3), true); }
			for (int32 i = 0; i < 6; ++i)
			{
				const int32 X = BX + R.NextInt(16), Y = BY + R.NextInt(16);
				int32 Z;
				if (FloorAt(X, Y, R.Range(35, 120), Z) && (W.Get(X, Y, Z) == G.SoulSand || W.Get(X, Y, Z) == G.SoulSoil)) W.Set(X, Y, Z + 1, G.SoulFire);
			}
		}
		break;
	case EMCBiome::BasaltDeltas:
	{
		const int32 N = R.Range(1, 4);
		for (int32 i = 0; i < N; ++i)
		{
			const int32 X = BX + R.NextInt(16), Y = BY + R.NextInt(16);
			if (!W.InsideXY(X, Y)) continue;
			int32 Z;
			if (FloorAt(X, Y, 110, Z)) MCFeatures::BasaltColumn(W, R, FMCBlockPos(X, Y, Z + 1), R.Range(2, 9), R.Range(1, 3));
		}
		if (bOwn)
		{
			// delta lava pools rimmed with magma
			for (int32 i = 0; i < 24; ++i)
			{
				const int32 X = BX + R.NextInt(16), Y = BY + R.NextInt(16);
				int32 Z;
				if (!FloorAt(X, Y, R.Range(40, 110), Z)) continue;
				bool bRim = true;
				for (int32 F = 2; F < 6 && bRim; ++F) { const FIntVector& D = MC::FaceDir[F]; const FMCState Nb = W.Get(X + D.X, Y + D.Y, Z); bRim = Nb != 0 && W.Inside(X + D.X, Y + D.Y, Z); }
				if (bRim && W.Get(X, Y, Z - 1) != 0) W.Set(X, Y, Z, NDelta.Sample2(X, Y) > 0 ? G.Lava : G.Magma);
			}
		}
		break;
	}
	default:
		if (bOwn)
		{
			// scattered fire and mushrooms in the wastes
			for (int32 i = 0; i < 6; ++i)
			{
				const int32 X = BX + R.NextInt(16), Y = BY + R.NextInt(16);
				int32 Z;
				if (FloorAt(X, Y, R.Range(35, 120), Z) && W.Get(X, Y, Z) == G.Netherrack) W.Set(X, Y, Z + 1, i < 3 ? G.Fire : (i == 3 ? G.BrownMushroom : G.RedMushroom));
			}
		}
		break;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Structures

void FMCNetherGen::GetStructureNames(TArray<FName>& Out) const
{
	for (const FMCStructureSet& Set : StructureSets) Out.Add(Set.Type);
}

TSharedPtr<const FMCStructureStart> FMCNetherGen::GetStructureStart(FName Type, const FMCChunkPos& SC) const
{
	const uint64 Key = MCHash::Hash3(MCHash::StringHash(*Type.ToString()), SC.X, SC.Y, 13);
	if (TSharedPtr<const FMCStructureStart> Found = StructCache.Find(Key)) return Found;
	TSharedPtr<FMCStructureStart> Created = CreateStructure(Type, SC);
	if (!Created) { Created = MakeShared<FMCStructureStart>(); }
	StructCache.Add(Key, Created);
	return Created;
}

void FMCNetherGen::PlaceStructures(FMCGenWriter& W, FMCChunk& C) const
{
	for (const FMCStructureSet& Set : StructureSets)
	{
		const int32 Rad = Set.MaxRadiusChunks;
		const int32 RX0 = FMath::FloorToInt((float)(C.Pos.X - Rad) / Set.Spacing), RX1 = FMath::FloorToInt((float)(C.Pos.X + Rad) / Set.Spacing);
		const int32 RY0 = FMath::FloorToInt((float)(C.Pos.Y - Rad) / Set.Spacing), RY1 = FMath::FloorToInt((float)(C.Pos.Y + Rad) / Set.Spacing);
		for (int32 ry = RY0; ry <= RY1; ++ry)
			for (int32 rx = RX0; rx <= RX1; ++rx)
			{
				FMCChunkPos SC;
				MCGen::GetStructureStartChunk(Set, Seed, rx, ry, SC);
				// fortress vs bastion share a grid in Minecraft: pick one per cell deterministically
				FMCRandom Pick(MCHash::Hash3(Seed, rx, ry, 0xF0B7));
				const bool bFortressCell = Pick.Chance(0.45);
				if ((Set.Type == TEXT("fortress")) != bFortressCell) continue;
				if (FMath::Abs(SC.X - C.Pos.X) > Rad || FMath::Abs(SC.Y - C.Pos.Y) > Rad) continue;
				TSharedPtr<const FMCStructureStart> St = GetStructureStart(Set.Type, SC);
				if (!St || !St->bValid) continue;
				const FIntVector CMin(C.Pos.MinBlockX(), C.Pos.MinBlockY(), 0), CMax(C.Pos.MinBlockX() + 15, C.Pos.MinBlockY() + 15, 0);
				for (const FMCStructurePiece& P : St->Pieces)
				{
					if (P.Max.X < CMin.X || P.Min.X > CMax.X || P.Max.Y < CMin.Y || P.Min.Y > CMax.Y) continue;
					FMCRandom R(P.Seed);
					P.Build(W, R);
				}
			}
	}
}

TSharedPtr<FMCStructureStart> FMCNetherGen::CreateStructure(FName Type, const FMCChunkPos& SC) const
{
	TSharedPtr<FMCStructureStart> St = MakeShared<FMCStructureStart>();
	St->Type = Type;
	const int32 CX = SC.MinBlockX() + 8, CY = SC.MinBlockY() + 8;
	FMCRandom R(MCHash::Hash3(Seed ^ MCHash::StringHash(*Type.ToString()), SC.X, SC.Y, 5));
	uint64 PieceSeed = MCHash::Hash3(Seed, SC.X, SC.Y, 777);
	auto NextSeed = [&]() { PieceSeed = MCHash::SplitMix64(PieceSeed); return PieceSeed; };
	auto AddPiece = [&](const FIntVector& Min, const FIntVector& Max, TFunction<void(FMCGenWriter&, FMCRandom&)>&& Fn)
	{
		FMCStructurePiece P;
		P.Min = FIntVector(FMath::Min(Min.X, Max.X), FMath::Min(Min.Y, Max.Y), FMath::Min(Min.Z, Max.Z));
		P.Max = FIntVector(FMath::Max(Min.X, Max.X), FMath::Max(Min.Y, Max.Y), FMath::Max(Min.Z, Max.Z));
		P.Seed = NextSeed();
		P.Build = MoveTemp(Fn);
		if (St->Pieces.Num() == 0) { St->Min = P.Min; St->Max = P.Max; }
		St->Min = FIntVector(FMath::Min(St->Min.X, P.Min.X), FMath::Min(St->Min.Y, P.Min.Y), FMath::Min(St->Min.Z, P.Min.Z));
		St->Max = FIntVector(FMath::Max(St->Max.X, P.Max.X), FMath::Max(St->Max.Y, P.Max.Y), FMath::Max(St->Max.Z, P.Max.Z));
		St->Pieces.Add(MoveTemp(P));
	};

	if (Type == TEXT("fortress"))
	{
		const int32 Z = R.Range(48, 70);
		St->Origin = FMCBlockPos(CX, CY, Z);
		// bridge network: a cross of long bridges with side branches, crossings, corridors and special rooms
		struct FSeg { FIntPoint A; int32 Dir; int32 Len; int32 Depth; };
		TArray<FSeg> Queue;
		for (int32 d = 0; d < 4; ++d) Queue.Add({ FIntPoint(CX, CY), d, R.Range(20, 40), 0 });
		const int32 DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
		// central crossing
		AddPiece(FIntVector(CX - 5, CY - 5, Z - 30), FIntVector(CX + 5, CY + 5, Z + 8), [CX, CY, Z](FMCGenWriter& W, FMCRandom& RR)
		{
			FMCPieceBuilder B(W, RR, FIntVector(CX, CY, Z), 0);
			const FMCState NB = S(TEXT("nether_bricks")), Fence = S(TEXT("nether_brick_fence"));
			B.Fill(-4, -4, -1, 4, 4, -1, NB);
			B.Fill(-4, -4, 0, 4, 4, 5, 0);
			for (int32 x = -4; x <= 4; ++x) for (int32 y = -4; y <= 4; ++y)
				if ((FMath::Abs(x) == 4 || FMath::Abs(y) == 4) && FMath::Abs(x) > 1 && FMath::Abs(y) > 1) B.Set(x, y, 0, Fence);
			// blaze spawner platform
			B.Fill(-1, -1, 0, 1, 1, 0, NB);
			B.Spawner(0, 0, 1, TEXT("blaze"));
			for (int32 x = -2; x <= 2; ++x) for (int32 y = -2; y <= 2; ++y) if (FMath::Abs(x) == 2 || FMath::Abs(y) == 2) B.Set(x, y, 1, Fence);
			B.Set(0, -2, 1, 0);
			// support pillars down to the terrain
			for (int32 px : { -3, 3 }) for (int32 py : { -3, 3 })
				for (int32 z = -2; z > -30; --z) { const FIntVector P = B.ToWorld(px, py, z); const FMCState Cur = W.Get(P.X, P.Y, P.Z); if (Cur != 0 && Cur != S(TEXT("lava"))) break; B.Set(px, py, z, NB); }
		});
		int32 Count = 0;
		while (Queue.Num() && Count < 18)
		{
			const FSeg Sg = Queue[0]; Queue.RemoveAt(0);
			++Count;
			const FIntPoint A(Sg.A.X + DX[Sg.Dir] * 5, Sg.A.Y + DY[Sg.Dir] * 5);
			const FIntPoint Bp(A.X + DX[Sg.Dir] * Sg.Len, A.Y + DY[Sg.Dir] * Sg.Len);
			if (FMath::Abs(Bp.X - CX) > 100 || FMath::Abs(Bp.Y - CY) > 100) continue;
			const bool bCorridor = Sg.Depth > 0 && R.Chance(0.4);
			const int32 Dir = Sg.Dir, Len = Sg.Len;
			const int32 Kind = (Sg.Depth > 0 && R.Chance(0.35)) ? R.Range(1, 3) : 0; // end room: 1 wart stairs, 2 chest corridor, 3 lava well
			AddPiece(FIntVector(FMath::Min(A.X, Bp.X) - 4, FMath::Min(A.Y, Bp.Y) - 4, Z - 30), FIntVector(FMath::Max(A.X, Bp.X) + 4, FMath::Max(A.Y, Bp.Y) + 4, Z + 8),
				[A, Dir, Len, Z, bCorridor, Kind](FMCGenWriter& W, FMCRandom& RR)
			{
				const int32 DX2[4] = { 0, 1, 0, -1 }, DY2[4] = { -1, 0, 1, 0 };
				// local frame: y forward along Dir, x to the right
				const int32 Rot = Dir; // Dir 0 = north = local -y forward... use builder with rotation so +y local = forward
				FMCPieceBuilder B(W, RR, FIntVector(A.X, A.Y, Z), (Rot + 2) & 3);
				const FMCState NB = S(TEXT("nether_bricks")), Fence = S(TEXT("nether_brick_fence")), Lava = S(TEXT("lava"));
				for (int32 k = 0; k <= Len; ++k)
				{
					B.Fill(-2, k, -1, 2, k, -1, NB);
					if (bCorridor)
					{
						for (int32 x = -2; x <= 2; ++x) for (int32 z = 0; z <= 4; ++z)
						{
							const bool bWall = FMath::Abs(x) == 2 || z == 4;
							if (bWall) B.Set(x, k, z, (z == 2 && (k % 4 == 1) && FMath::Abs(x) == 2) ? Fence : NB);
							else B.Set(x, k, z, 0);
						}
					}
					else
					{
						B.Fill(-1, k, 0, 1, k, 3, 0);
						B.Set(-2, k, 0, Fence); B.Set(2, k, 0, Fence);
					}
					if (k % 8 == 4)
					{
						// arch supports under the bridge down to the ground
						for (int32 x : { -2, 2 })
							for (int32 z = -2; z > -32; --z)
							{
								const FIntVector P = B.ToWorld(x, k, z);
								const FMCState Cur = W.Get(P.X, P.Y, P.Z);
								if (Cur != 0 && Cur != Lava) break;
								B.Set(x, k, z, NB);
							}
						for (int32 x = -1; x <= 1; ++x) B.Set(x, k, -2, NB);
					}
				}
				// end rooms
				if (Kind == 1)
				{
					// nether wart garden with soul sand
					B.Fill(-4, Len - 6, -1, 4, Len, 5, NB);
					B.Fill(-3, Len - 5, 0, 3, Len - 1, 4, 0);
					B.Fill(-3, Len - 2, 0, 3, Len - 1, 0, S(TEXT("soul_sand")));
					const FMCBlock* Wart = FMCBlocks::Find(TEXT("nether_wart"));
					if (Wart) for (int32 x = -3; x <= 3; ++x) for (int32 y = Len - 2; y <= Len - 1; ++y) B.Set(x, y, 1, Wart->State((uint8)RR.NextInt(Wart->NumStates)));
					const FMCState Stairs = S(TEXT("nether_brick_stairs"));
					for (int32 x = -1; x <= 1; ++x) B.Stair(x, Len - 4, 0, Stairs, EMCFace::South);
				}
				else if (Kind == 2)
				{
					B.Fill(-3, Len - 4, -1, 3, Len, 4, NB);
					B.Fill(-2, Len - 3, 0, 2, Len - 1, 3, 0);
					B.Chest(0, Len - 1, 0, EMCFace::North, TEXT("nether_bridge"));
				}
				else if (Kind == 3)
				{
					B.Fill(-3, Len - 6, -1, 3, Len, 6, NB);
					B.Fill(-2, Len - 5, 0, 2, Len - 1, 5, 0);
					B.Fill(-1, Len - 4, 0, 1, Len - 2, 0, NB);
					B.Set(0, Len - 3, 0, Lava);
					for (int32 x = -2; x <= 2; ++x) B.Set(x, Len - 5, 2, Fence);
				}
				if (RR.Chance(0.5)) B.Mob(RR.Chance(0.5) ? FName(TEXT("wither_skeleton")) : FName(TEXT("blaze")), 0, Len / 2, 0);
				(void)DX2; (void)DY2;
			});
			if (Sg.Depth < 2 && Kind == 0)
			{
				// crossing at the end with branches
				const FIntPoint End = Bp;
				AddPiece(FIntVector(End.X - 4, End.Y - 4, Z - 30), FIntVector(End.X + 4, End.Y + 4, Z + 6), [End, Z](FMCGenWriter& W, FMCRandom& RR)
				{
					FMCPieceBuilder B(W, RR, FIntVector(End.X, End.Y, Z), 0);
					const FMCState NB = S(TEXT("nether_bricks")), Fence = S(TEXT("nether_brick_fence")), Lava = S(TEXT("lava"));
					B.Fill(-3, -3, -1, 3, 3, -1, NB);
					B.Fill(-3, -3, 0, 3, 3, 3, 0);
					for (int32 x = -3; x <= 3; x += 6) for (int32 y = -3; y <= 3; y += 6) { B.Fill(x, y, 0, x, y, 3, NB); for (int32 z = -2; z > -32; --z) { const FIntVector P = B.ToWorld(x, y, z); const FMCState Cur = W.Get(P.X, P.Y, P.Z); if (Cur != 0 && Cur != Lava) break; B.Set(x, y, z, NB); } }
					B.Fill(-3, -3, 4, 3, 3, 4, NB);
					for (int32 x = -2; x <= 2; ++x) { B.Set(x, -3, 4, Fence); B.Set(x, 3, 4, Fence); }
				});
				for (int32 nd = 0; nd < 4; ++nd)
				{
					if (nd == ((Sg.Dir + 2) & 3)) continue;
					if (nd != Sg.Dir && !R.Chance(0.6)) continue;
					Queue.Add({ End, nd, R.Range(12, 28), Sg.Depth + 1 });
				}
			}
		}
		St->bValid = true;
		return St;
	}

	if (Type == TEXT("bastion_remnant"))
	{
		const EMCBiome Bi = (EMCBiome)GetBiomeAt(CX, CY, 64);
		if (Bi == EMCBiome::BasaltDeltas) return St;
		const int32 Z = 34;
		St->Origin = FMCBlockPos(CX, CY, Z);
		for (int32 q = 0; q < 4; ++q)
		{
			const int32 QX = (q & 1) ? 0 : -16, QY = (q & 2) ? 0 : -16;
			AddPiece(FIntVector(CX + QX, CY + QY, Z - 12), FIntVector(CX + QX + 16, CY + QY + 16, Z + 26), [CX, CY, Z, QX, QY](FMCGenWriter& W, FMCRandom& RR)
			{
				FMCPieceBuilder B(W, RR, FIntVector(CX, CY, Z), 0);
				const FMCState PBB = S(TEXT("polished_blackstone_bricks")), CPBB = S(TEXT("cracked_polished_blackstone_bricks")), BS = S(TEXT("blackstone"));
				const FMCState Gilded = S(TEXT("gilded_blackstone")), Gold = S(TEXT("gold_block")), Chain = S(TEXT("chain")), Lava = S(TEXT("lava")), Basalt = S(TEXT("basalt"));
				for (int32 x = QX; x <= QX + 16; ++x)
					for (int32 y = QY; y <= QY + 16; ++y)
					{
						const int32 AX = FMath::Abs(x), AY = FMath::Abs(y);
						if (AX > 15 || AY > 15) continue;
						const int32 M = FMath::Max(AX, AY);
						// foundation down to the lava/ground
						for (int32 z = -1; z > -12; --z) { const FIntVector P = B.ToWorld(x, y, z); const FMCState Cur = W.Get(P.X, P.Y, P.Z); if (Cur != 0 && Cur != Lava) break; B.Set(x, y, z, BS); }
						for (int32 z = 0; z <= 24; ++z)
						{
							FMCState Bl = 0;
							const bool bOuter = M == 15, bInner = M == 7;
							const bool bFloorLvl = z == 0 || z == 8 || z == 16;
							if (bOuter && z <= 20) Bl = (z % 6 == 3 && (AX + AY) % 4 == 0) ? 0 : (RR.Chance(0.12) ? CPBB : PBB);
							else if (bInner && z <= 22 && !(z >= 1 && z <= 3 && (AX <= 1 || AY <= 1))) Bl = RR.Chance(0.1) ? Gilded : PBB;
							else if (bFloorLvl && (M < 15)) Bl = (M > 7 || z == 0) ? (RR.Chance(0.1) ? BS : PBB) : 0;
							if (z == 24 && M <= 7) Bl = PBB;
							if (z == 21 && bOuter && (AX + AY) % 2 == 0) Bl = PBB;
							B.Set(x, y, z, Bl);
						}
						// entrance gaps in the outer wall
						if ((AX <= 1 && AY == 15) || (AY <= 1 && AX == 15)) for (int32 z = 1; z <= 4; ++z) B.Set(x, y, z, 0);
					}
				// treasure in the inner keep
				if (QX == 0 && QY == 0)
				{
					B.Fill(-2, -2, 1, 2, 2, 1, Gold);
					B.Set(0, 0, 2, Gold);
					B.Chest(3, 0, 1, EMCFace::West, TEXT("bastion_treasure"));
					B.Mob(TEXT("piglin_brute"), 4, 4, 1);
					for (int32 k = 9; k <= 15; ++k) B.Set(0, 0, k, Chain);
					B.SetRaw(0, 0, 8, WithMeta(TEXT("lantern"), 1));
				}
				B.Chest(QX + 10, QY + 10, 9, EMCFace::North, TEXT("bastion_other"));
				B.Chest(QX + 4, QY + 12, 1, EMCFace::East, TEXT("bastion_bridge"));
				B.Mob(TEXT("piglin"), (float)QX + 10, (float)QY + 4, 1);
				B.Mob(TEXT("piglin"), (float)QX + 5, (float)QY + 10, 9);
				if (RR.Chance(0.4)) B.Mob(TEXT("hoglin"), (float)QX + 11, (float)QY + 11, 1);
				if (RR.Chance(0.5)) B.Set(QX + 12, QY + 3, 17, Basalt);
			});
		}
		St->bValid = true;
		return St;
	}
	return St;
}

bool FMCNetherGen::LocateStructure(FName Type, const FMCBlockPos& Origin, int32 MaxChunks, FMCBlockPos& Out) const
{
	const FMCStructureSet* Set = StructureSets.FindByPredicate([&](const FMCStructureSet& S2) { return S2.Type == Type; });
	if (!Set) return false;
	const FMCChunkPos OC = FMCChunkPos::FromBlock(Origin);
	const int32 ORX = FMath::FloorToInt((float)OC.X / Set->Spacing), ORY = FMath::FloorToInt((float)OC.Y / Set->Spacing);
	int64 Best = TNumericLimits<int64>::Max();
	const int32 MaxR = FMath::Max(2, MaxChunks / Set->Spacing + 1);
	for (int32 Rr = 0; Rr <= MaxR; ++Rr)
	{
		for (int32 ry = -Rr; ry <= Rr; ++ry)
			for (int32 rx = -Rr; rx <= Rr; ++rx)
			{
				if (FMath::Max(FMath::Abs(rx), FMath::Abs(ry)) != Rr) continue;
				FMCRandom Pick(MCHash::Hash3(Seed, ORX + rx, ORY + ry, 0xF0B7));
				if ((Type == TEXT("fortress")) != Pick.Chance(0.45)) continue;
				FMCChunkPos SC;
				MCGen::GetStructureStartChunk(*Set, Seed, ORX + rx, ORY + ry, SC);
				TSharedPtr<const FMCStructureStart> St = GetStructureStart(Type, SC);
				if (!St || !St->bValid) continue;
				const int64 D = (int64)(St->Origin.X - Origin.X) * (St->Origin.X - Origin.X) + (int64)(St->Origin.Y - Origin.Y) * (St->Origin.Y - Origin.Y);
				if (D < Best) { Best = D; Out = St->Origin; }
			}
		if (Best != TNumericLimits<int64>::Max() && Rr >= 1) return true;
	}
	return Best != TNumericLimits<int64>::Max();
}

FName FMCNetherGen::GetStructureAt(const FMCBlockPos& P) const
{
	const FMCChunkPos C = FMCChunkPos::FromBlock(P);
	for (const FMCStructureSet& Set : StructureSets)
	{
		const int32 Rad = Set.MaxRadiusChunks;
		const int32 RX0 = FMath::FloorToInt((float)(C.X - Rad) / Set.Spacing), RX1 = FMath::FloorToInt((float)(C.X + Rad) / Set.Spacing);
		const int32 RY0 = FMath::FloorToInt((float)(C.Y - Rad) / Set.Spacing), RY1 = FMath::FloorToInt((float)(C.Y + Rad) / Set.Spacing);
		for (int32 ry = RY0; ry <= RY1; ++ry)
			for (int32 rx = RX0; rx <= RX1; ++rx)
			{
				FMCRandom Pick(MCHash::Hash3(Seed, rx, ry, 0xF0B7));
				if ((Set.Type == TEXT("fortress")) != Pick.Chance(0.45)) continue;
				FMCChunkPos SC;
				MCGen::GetStructureStartChunk(Set, Seed, rx, ry, SC);
				TSharedPtr<const FMCStructureStart> St = GetStructureStart(Set.Type, SC);
				if (!St || !St->bValid) continue;
				for (const FMCStructurePiece& Pc : St->Pieces)
					if (P.X >= Pc.Min.X && P.X <= Pc.Max.X && P.Y >= Pc.Min.Y && P.Y <= Pc.Max.Y && P.Z >= Pc.Min.Z && P.Z <= Pc.Max.Z) return Set.Type;
			}
	}
	return NAME_None;
}
