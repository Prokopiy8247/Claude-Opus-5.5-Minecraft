// Overworld structures: villages, strongholds, mineshafts, ruined portals, temples, huts, igloos, outposts,
// shipwrecks, ocean ruins, buried treasure, monuments, mansions, ancient cities, trail ruins, trial chambers, dungeons.
#include "Gen/MCOverworldGen.h"
#include "Gen/MCStructureBuilder.h"
#include "Gen/MCFeatures.h"
#include "World/MCBlockEntity.h"

using MCGen::S;
using MCGen::WithMeta;

namespace
{
	using FBuild = TFunction<void(FMCGenWriter&, FMCRandom&)>;

	void AddPiece(FMCStructureStart& St, const FIntVector& Min, const FIntVector& Max, uint64 Seed, FBuild&& Build)
	{
		FMCStructurePiece P;
		P.Min = FIntVector(FMath::Min(Min.X, Max.X), FMath::Min(Min.Y, Max.Y), FMath::Min(Min.Z, Max.Z));
		P.Max = FIntVector(FMath::Max(Min.X, Max.X), FMath::Max(Min.Y, Max.Y), FMath::Max(Min.Z, Max.Z));
		P.Seed = Seed;
		P.Build = MoveTemp(Build);
		St.Min = St.Pieces.Num() == 0 ? P.Min : FIntVector(FMath::Min(St.Min.X, P.Min.X), FMath::Min(St.Min.Y, P.Min.Y), FMath::Min(St.Min.Z, P.Min.Z));
		St.Max = St.Pieces.Num() == 0 ? P.Max : FIntVector(FMath::Max(St.Max.X, P.Max.X), FMath::Max(St.Max.Y, P.Max.Y), FMath::Max(St.Max.Z, P.Max.Z));
		St.Pieces.Add(MoveTemp(P));
	}

	/** World bounds of a rotated local box (for piece bounding boxes). */
	void RotBounds(const FIntVector& Origin, int32 Rot, int32 X0, int32 Y0, int32 Z0, int32 X1, int32 Y1, int32 Z1, FIntVector& OutMin, FIntVector& OutMax)
	{
		FMCGenWriter* Dummy = nullptr;
		FIntVector C[4];
		const int32 Xs[2] = { X0, X1 }, Ys[2] = { Y0, Y1 };
		int32 k = 0;
		for (int32 a = 0; a < 2; ++a) for (int32 b = 0; b < 2; ++b)
		{
			int32 RX = Xs[a], RY = Ys[b];
			for (int32 i = 0; i < ((Rot % 4) + 4) % 4; ++i) { const int32 T = RX; RX = -RY; RY = T; }
			C[k++] = FIntVector(Origin.X + RX, Origin.Y + RY, 0);
		}
		OutMin = FIntVector(FMath::Min(FMath::Min(C[0].X, C[1].X), FMath::Min(C[2].X, C[3].X)), FMath::Min(FMath::Min(C[0].Y, C[1].Y), FMath::Min(C[2].Y, C[3].Y)), Origin.Z + Z0);
		OutMax = FIntVector(FMath::Max(FMath::Max(C[0].X, C[1].X), FMath::Max(C[2].X, C[3].X)), FMath::Max(FMath::Max(C[0].Y, C[1].Y), FMath::Max(C[2].Y, C[3].Y)), Origin.Z + Z1);
		(void)Dummy;
	}

	/** Village material palette. */
	struct FVillagePalette
	{
		FMCState Log, Planks, Stairs, Slab, Fence, Foundation, Floor, Roof, RoofFill, Pane, Path, Wall;
		const TCHAR* Door;
		const TCHAR* Bed;
		bool bDesert = false;
		bool bSnowy = false;
		FName Style;
	};

	FVillagePalette MakePalette(EMCBiome E)
	{
		FVillagePalette P;
		P.Pane = S(TEXT("glass_pane"));
		P.Path = S(TEXT("dirt_path"));
		P.Foundation = S(TEXT("cobblestone"));
		switch (E)
		{
		case EMCBiome::Desert:
			P.Log = S(TEXT("cut_sandstone")); P.Planks = S(TEXT("smooth_sandstone")); P.Wall = S(TEXT("sandstone")); P.Stairs = S(TEXT("sandstone_stairs"));
			P.Slab = S(TEXT("sandstone_slab")); P.Fence = S(TEXT("sandstone_wall")); P.Foundation = S(TEXT("sandstone")); P.Floor = S(TEXT("smooth_sandstone"));
			P.Roof = S(TEXT("smooth_sandstone_stairs")); P.RoofFill = S(TEXT("smooth_sandstone")); P.Door = TEXT("jungle_door"); P.Bed = TEXT("yellow_bed");
			P.bDesert = true; P.Style = TEXT("desert"); P.Path = S(TEXT("smooth_sandstone"));
			break;
		case EMCBiome::Savanna: case EMCBiome::SavannaPlateau:
			P.Log = S(TEXT("acacia_log")); P.Planks = S(TEXT("acacia_planks")); P.Wall = P.Planks; P.Stairs = S(TEXT("acacia_stairs")); P.Slab = S(TEXT("acacia_slab"));
			P.Fence = S(TEXT("acacia_fence")); P.Floor = S(TEXT("acacia_planks")); P.Roof = S(TEXT("acacia_stairs")); P.RoofFill = S(TEXT("acacia_planks"));
			P.Door = TEXT("acacia_door"); P.Bed = TEXT("orange_bed"); P.Style = TEXT("savanna");
			break;
		case EMCBiome::Taiga: case EMCBiome::SnowyTaiga: case EMCBiome::SnowyPlains: case EMCBiome::OldGrowthSpruceTaiga:
			P.Log = S(TEXT("spruce_log")); P.Planks = S(TEXT("spruce_planks")); P.Wall = P.Planks; P.Stairs = S(TEXT("spruce_stairs")); P.Slab = S(TEXT("spruce_slab"));
			P.Fence = S(TEXT("spruce_fence")); P.Floor = S(TEXT("spruce_planks")); P.Roof = S(TEXT("spruce_stairs")); P.RoofFill = S(TEXT("spruce_planks"));
			P.Door = TEXT("spruce_door"); P.Bed = TEXT("red_bed"); P.bSnowy = E == EMCBiome::SnowyPlains || E == EMCBiome::SnowyTaiga; P.Style = TEXT("taiga");
			break;
		default:
			P.Log = S(TEXT("oak_log")); P.Planks = S(TEXT("oak_planks")); P.Wall = P.Planks; P.Stairs = S(TEXT("oak_stairs")); P.Slab = S(TEXT("oak_slab"));
			P.Fence = S(TEXT("oak_fence")); P.Floor = S(TEXT("oak_planks")); P.Roof = S(TEXT("spruce_stairs")); P.RoofFill = S(TEXT("spruce_planks"));
			P.Door = TEXT("oak_door"); P.Bed = TEXT("red_bed"); P.Style = TEXT("plains");
			break;
		}
		return P;
	}

	/** Clears space above a footprint (vegetation / hillsides) up to Height. */
	void ClearAbove(FMCPieceBuilder& B, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 Z0, int32 Height)
	{
		B.Fill(X0, Y0, Z0, X1, Y1, Z0 + Height, 0);
	}

	// ------------------------------------------------------------------ village buildings (door at local (w/2, 0) facing north)
	void BuildHouseSmall(FMCPieceBuilder& B, const FVillagePalette& P, FMCRandom& R)
	{
		ClearAbove(B, -1, -1, 5, 5, 1, 9);
		B.Foundation(0, 0, 4, 4, 0, P.Foundation);
		B.Fill(0, 0, 0, 4, 4, 0, P.Foundation);
		B.Fill(1, 1, 0, 3, 3, 0, P.Floor);
		for (int32 z = 1; z <= 3; ++z)
		{
			for (int32 i = 0; i <= 4; ++i)
			{
				const bool bCornerX = i == 0 || i == 4;
				B.Set(i, 0, z, bCornerX ? P.Log : P.Wall); B.Set(i, 4, z, bCornerX ? P.Log : P.Wall);
				B.Set(0, i, z, bCornerX ? P.Log : P.Wall); B.Set(4, i, z, bCornerX ? P.Log : P.Wall);
			}
		}
		B.Set(0, 2, 2, P.Pane); B.Set(4, 2, 2, P.Pane); B.Set(2, 4, 2, P.Pane);
		B.Door(2, 0, 1, P.Door, EMCFace::North);
		B.Fill(1, 1, 1, 3, 3, 3, 0);
		if (P.bDesert) { B.Fill(0, 0, 4, 4, 4, 4, P.RoofFill); B.Fill(1, 1, 5, 3, 3, 5, P.Slab); }
		else B.GableRoof(-1, -1, 5, 5, 4, P.Roof, P.RoofFill);
		B.Bed(1, 2, 1, P.Bed, EMCFace::South);
		B.Set(3, 3, 1, S(TEXT("crafting_table")));
		if (R.Chance(0.5)) B.Chest(3, 1, 1, EMCFace::West, TEXT("village_house"));
		B.Torch(2, 3, 3, EMCFace::North);
		B.Mob(TEXT("villager"), 2, 2, 1);
	}

	void BuildHouseBig(FMCPieceBuilder& B, const FVillagePalette& P, FMCRandom& R)
	{
		ClearAbove(B, -1, -1, 7, 9, 1, 11);
		B.Foundation(0, 0, 6, 8, 0, P.Foundation);
		B.Fill(0, 0, 0, 6, 8, 0, P.Foundation);
		B.Fill(1, 1, 0, 5, 7, 0, P.Floor);
		for (int32 z = 1; z <= 4; ++z)
		{
			for (int32 x = 0; x <= 6; ++x) { B.Set(x, 0, z, (x == 0 || x == 6) ? P.Log : P.Wall); B.Set(x, 8, z, (x == 0 || x == 6) ? P.Log : P.Wall); }
			for (int32 y = 0; y <= 8; ++y) { B.Set(0, y, z, (y == 0 || y == 8 || y == 4) ? P.Log : P.Wall); B.Set(6, y, z, (y == 0 || y == 8 || y == 4) ? P.Log : P.Wall); }
		}
		for (int32 y : { 2, 6 }) { B.Set(0, y, 2, P.Pane); B.Set(0, y, 3, P.Pane); B.Set(6, y, 2, P.Pane); B.Set(6, y, 3, P.Pane); }
		B.Set(2, 8, 2, P.Pane); B.Set(4, 8, 2, P.Pane);
		B.Fill(1, 1, 1, 5, 7, 4, 0);
		B.Door(3, 0, 1, P.Door, EMCFace::North);
		if (P.bDesert) { B.Fill(0, 0, 5, 6, 8, 5, P.RoofFill); }
		else B.GableRoof(-1, -1, 7, 9, 5, P.Roof, P.RoofFill);
		// interior
		B.Fill(1, 4, 1, 5, 4, 3, P.Wall); B.Door(3, 4, 1, P.Door, EMCFace::North);
		B.Bed(1, 6, 1, P.Bed, EMCFace::South); B.Bed(5, 6, 1, P.Bed, EMCFace::South);
		B.Set(1, 1, 1, S(TEXT("furnace"))); B.Set(2, 1, 1, S(TEXT("crafting_table")));
		B.Chest(5, 1, 1, EMCFace::West, TEXT("village_house"));
		B.Set(3, 7, 1, WithMeta(TEXT("white_carpet"), 0));
		B.SetRaw(3, 2, 4, WithMeta(TEXT("lantern"), 1));
		B.SetRaw(3, 6, 4, WithMeta(TEXT("lantern"), 1));
		B.Mob(TEXT("villager"), 3, 6, 1);
		B.Mob(TEXT("villager"), 3, 2, 1);
		if (R.Chance(0.3)) B.Mob(TEXT("cat"), 2, 3, 1);
	}

	void BuildLibrary(FMCPieceBuilder& B, const FVillagePalette& P, FMCRandom& R)
	{
		ClearAbove(B, -1, -1, 7, 7, 1, 10);
		B.Foundation(0, 0, 6, 6, 0, P.Foundation);
		B.Fill(0, 0, 0, 6, 6, 0, P.Foundation);
		B.Fill(1, 1, 0, 5, 5, 0, P.Floor);
		for (int32 z = 1; z <= 4; ++z)
			for (int32 i = 0; i <= 6; ++i)
			{
				const FMCState Wl = (i == 0 || i == 6) ? P.Log : P.Wall;
				B.Set(i, 0, z, Wl); B.Set(i, 6, z, Wl); B.Set(0, i, z, Wl); B.Set(6, i, z, Wl);
			}
		B.Fill(1, 1, 1, 5, 5, 4, 0);
		B.Door(3, 0, 1, P.Door, EMCFace::North);
		B.Set(0, 3, 2, P.Pane); B.Set(6, 3, 2, P.Pane);
		for (int32 x = 1; x <= 5; ++x) { B.Set(x, 5, 1, S(TEXT("bookshelf"))); B.Set(x, 5, 2, S(TEXT("bookshelf"))); }
		B.Set(1, 2, 1, S(TEXT("bookshelf"))); B.Set(5, 2, 1, S(TEXT("bookshelf")));
		B.Set(3, 3, 1, WithMeta(TEXT("lectern"), 0));
		B.Fill(2, 2, 1, 4, 4, 1, WithMeta(TEXT("red_carpet"), 0));
		B.Set(3, 3, 1, WithMeta(TEXT("lectern"), 0));
		if (P.bDesert) B.Fill(0, 0, 5, 6, 6, 5, P.RoofFill); else B.GableRoof(-1, -1, 7, 7, 5, P.Roof, P.RoofFill);
		B.SetRaw(3, 3, 4, WithMeta(TEXT("lantern"), 1));
		B.Mob(TEXT("villager"), 2, 3, 1, 3);
	}

	void BuildSmith(FMCPieceBuilder& B, const FVillagePalette& P, FMCRandom& R)
	{
		ClearAbove(B, -1, -1, 9, 7, 1, 8);
		const FMCState Cob = S(TEXT("cobblestone"));
		B.Foundation(0, 0, 8, 6, 0, P.Foundation);
		B.Fill(0, 0, 0, 8, 6, 0, Cob);
		for (int32 z = 1; z <= 3; ++z)
		{
			for (int32 x = 4; x <= 8; ++x) { B.Set(x, 6, z, Cob); }
			for (int32 y = 0; y <= 6; ++y) { B.Set(8, y, z, Cob); }
			B.Set(0, 6, z, P.Log); B.Set(0, 0, z, P.Log); B.Set(4, 0, z, P.Log);
		}
		B.Fill(0, 0, 4, 8, 6, 4, P.Slab);
		// lava forge
		B.Fill(5, 3, 1, 7, 5, 1, Cob);
		B.Set(6, 4, 1, S(TEXT("lava")));
		B.Set(5, 5, 1, S(TEXT("furnace"))); B.Set(7, 5, 1, S(TEXT("furnace")));
		B.Set(2, 4, 1, WithMeta(TEXT("anvil"), 0));
		B.Set(1, 5, 1, S(TEXT("grindstone")));
		B.Chest(7, 1, 1, EMCFace::West, TEXT("village_weaponsmith"));
		B.Set(6, 2, 2, S(TEXT("iron_bars")));
		B.Mob(TEXT("villager"), 3, 2, 1, 5);
	}

	void BuildFarm(FMCPieceBuilder& B, const FVillagePalette& P, FMCRandom& R)
	{
		ClearAbove(B, 0, 0, 8, 6, 1, 4);
		const FMCState Farmland = WithMeta(TEXT("farmland"), 7);
		const FMCState Water = S(TEXT("water"));
		const TCHAR* Crops[4] = { TEXT("wheat"), TEXT("carrots"), TEXT("potatoes"), TEXT("beetroots") };
		const TCHAR* Crop = Crops[R.NextInt(4)];
		const FMCBlock* CB = FMCBlocks::Find(Crop);
		B.Foundation(0, 0, 8, 6, 0, S(TEXT("dirt")));
		for (int32 y = 0; y <= 6; ++y)
			for (int32 x = 0; x <= 8; ++x)
			{
				const bool bBorder = x == 0 || x == 8 || y == 0 || y == 6;
				if (bBorder) { B.Set(x, y, 0, P.Log == S(TEXT("cut_sandstone")) ? P.Log : FMCBlocks::GetByState(P.Log).State(y == 0 || y == 6 ? 1 : 2)); continue; }
				if (x == 4) { B.Set(x, y, 0, Water); continue; }
				B.Set(x, y, 0, Farmland);
				if (CB) B.Set(x, y, 1, CB->State((uint8)FMath::Min<int32>(R.NextInt(CB->NumStates), CB->NumStates - 1)));
			}
		B.Set(0, 3, 1, S(TEXT("composter")));
		B.Mob(TEXT("villager"), 2, 3, 1, 1);
	}

	void BuildChurch(FMCPieceBuilder& B, const FVillagePalette& P, FMCRandom& R)
	{
		const FMCState Cob = S(TEXT("cobblestone"));
		ClearAbove(B, -1, -1, 6, 10, 1, 16);
		B.Foundation(0, 0, 5, 9, 0, P.Foundation);
		B.Fill(0, 0, 0, 5, 9, 0, Cob);
		for (int32 z = 1; z <= 5; ++z)
			for (int32 x = 0; x <= 5; ++x) for (int32 y = 0; y <= 9; ++y)
				if (x == 0 || x == 5 || y == 0 || y == 9) B.Set(x, y, z, Cob);
		// tower
		for (int32 z = 6; z <= 11; ++z)
			for (int32 x = 0; x <= 5; ++x) for (int32 y = 5; y <= 9; ++y)
				if (x == 0 || x == 5 || y == 5 || y == 9) B.Set(x, y, z, Cob);
		B.Fill(0, 0, 6, 5, 4, 6, P.Slab);
		B.Fill(0, 5, 12, 5, 9, 12, Cob);
		B.Fill(1, 1, 1, 4, 8, 5, 0);
		B.Fill(1, 6, 6, 4, 8, 11, 0);
		B.Door(2, 0, 1, P.Door, EMCFace::North);
		for (int32 y : { 2, 4, 7 }) { B.Set(0, y, 3, P.Pane); B.Set(5, y, 3, P.Pane); }
		B.Set(2, 9, 9, P.Pane); B.Set(3, 5, 9, P.Pane); B.Set(0, 7, 9, P.Pane); B.Set(5, 7, 9, P.Pane);
		B.Set(2, 8, 1, S(TEXT("brewing_stand")));
		B.Torch(1, 7, 3, EMCFace::East); B.Torch(4, 7, 3, EMCFace::West);
		for (int32 z = 1; z <= 10; ++z) B.Set(4, 8, z, WithMeta(TEXT("ladder"), 3));
		B.Mob(TEXT("villager"), 2, 5, 1, 2);
	}

	void BuildLamp(FMCPieceBuilder& B, const FVillagePalette& P, FMCRandom& R)
	{
		B.Set(0, 0, 0, P.Foundation);
		B.Set(0, 0, 1, P.Fence); B.Set(0, 0, 2, P.Fence);
		B.Set(0, 0, 3, P.bDesert ? S(TEXT("sandstone")) : S(TEXT("oak_planks")));
		B.Torch(0, 1, 3, EMCFace::South); B.Torch(0, -1, 3, EMCFace::North); B.Torch(1, 0, 3, EMCFace::East); B.Torch(-1, 0, 3, EMCFace::West);
	}

	void BuildPen(FMCPieceBuilder& B, const FVillagePalette& P, FMCRandom& R)
	{
		ClearAbove(B, 0, 0, 7, 7, 1, 4);
		B.Foundation(0, 0, 7, 7, 0, S(TEXT("dirt")));
		for (int32 x = 0; x <= 7; ++x) for (int32 y = 0; y <= 7; ++y)
		{
			B.Set(x, y, 0, S(TEXT("grass_block")));
			if (x == 0 || x == 7 || y == 0 || y == 7) B.Set(x, y, 1, P.Fence);
		}
		B.Set(3, 0, 1, FMCBlocks::GetByState(S(TEXT("oak_fence_gate"))).State(0));
		const TCHAR* Animals[3] = { TEXT("cow"), TEXT("sheep"), TEXT("pig") };
		const TCHAR* A = Animals[R.NextInt(3)];
		for (int32 i = 0; i < 3; ++i) B.Mob(A, 2 + i, 3 + (i & 1), 1);
		B.Set(1, 6, 1, S(TEXT("hay_block")));
	}

	void BuildWell(FMCPieceBuilder& B, const FVillagePalette& P, FMCRandom& R)
	{
		const FMCState Cob = P.bDesert ? S(TEXT("sandstone")) : S(TEXT("cobblestone"));
		ClearAbove(B, -3, -3, 3, 3, 1, 6);
		B.Foundation(-3, -3, 3, 3, 0, Cob);
		for (int32 x = -3; x <= 3; ++x) for (int32 y = -3; y <= 3; ++y) B.Set(x, y, 0, P.Path);
		B.Fill(-1, -1, -3, 1, 1, 0, Cob);
		B.Fill(0, 0, -2, 0, 0, 0, S(TEXT("water")));
		B.Fill(-1, -1, 1, 1, 1, 1, Cob);
		B.Set(0, 0, 1, S(TEXT("water")));
		B.Set(-1, -1, 2, P.Fence); B.Set(1, -1, 2, P.Fence); B.Set(-1, 1, 2, P.Fence); B.Set(1, 1, 2, P.Fence);
		B.Set(-1, -1, 3, P.Fence); B.Set(1, -1, 3, P.Fence); B.Set(-1, 1, 3, P.Fence); B.Set(1, 1, 3, P.Fence);
		B.Fill(-1, -1, 4, 1, 1, 4, Cob);
		B.Set(2, 0, 1, WithMeta(TEXT("bell"), 0));
		B.Mob(TEXT("iron_golem"), 3, 3, 1);
		B.Mob(TEXT("villager"), -2, 2, 1);
		B.Mob(TEXT("cat"), 2, -2, 1);
	}
}

// ---------------------------------------------------------------------------------------------------------------------

void FMCOverworldGen::ComputeStrongholds()
{
	FMCRandom R(Seed ^ 0x57A0C);
	const int32 RingCounts[3] = { 3, 6, 10 };
	const float RingMin[3] = { 520.f, 1600.f, 2800.f };
	const float RingMax[3] = { 900.f, 2400.f, 3600.f };
	for (int32 Ring = 0; Ring < 3; ++Ring)
	{
		const float Base = R.FRange(0.f, 2.f * PI);
		for (int32 i = 0; i < RingCounts[Ring]; ++i)
		{
			const float A = Base + i * 2.f * PI / RingCounts[Ring] + R.FRange(-0.3f, 0.3f);
			const float D = R.FRange(RingMin[Ring], RingMax[Ring]);
			const int32 X = (int32)(FMath::Cos(A) * D) & ~15, Y = (int32)(FMath::Sin(A) * D) & ~15;
			Strongholds.Add(FMCBlockPos(X + 8, Y + 8, 22));
		}
	}
}

void FMCOverworldGen::GetStructureNames(TArray<FName>& Out) const
{
	for (const FMCStructureSet& S2 : StructureSets) Out.Add(S2.Type);
	Out.Add(TEXT("stronghold"));
}

TSharedPtr<const FMCStructureStart> FMCOverworldGen::GetStructureStart(FName Type, const FMCChunkPos& StartChunk) const
{
	const uint64 Key = MCHash::Hash3(MCHash::StringHash(*Type.ToString()), StartChunk.X, StartChunk.Y, 7);
	if (TSharedPtr<const FMCStructureStart> Found = StructCache.Find(Key)) return Found;
	TSharedPtr<FMCStructureStart> Created = CreateStructure(Type, StartChunk);
	if (!Created) { Created = MakeShared<FMCStructureStart>(); Created->bValid = false; }
	StructCache.Add(Key, Created);
	return Created;
}

void FMCOverworldGen::PlaceStructures(FMCGenWriter& W, FMCChunk& C) const
{
	const FIntVector CMin(C.Pos.MinBlockX(), C.Pos.MinBlockY(), MC::MinZ);
	const FIntVector CMax(C.Pos.MinBlockX() + 15, C.Pos.MinBlockY() + 15, MC::MaxZ);
	auto Apply = [&](const FMCStructureStart& St)
	{
		if (!St.bValid) return;
		if (St.Max.X < CMin.X || St.Min.X > CMax.X || St.Max.Y < CMin.Y || St.Min.Y > CMax.Y) return;
		for (const FMCStructurePiece& P : St.Pieces)
		{
			if (P.Max.X < CMin.X || P.Min.X > CMax.X || P.Max.Y < CMin.Y || P.Min.Y > CMax.Y) continue;
			FMCRandom R(P.Seed);
			P.Build(W, R);
		}
	};
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
				if (FMath::Abs(SC.X - C.Pos.X) > Rad || FMath::Abs(SC.Y - C.Pos.Y) > Rad) continue;
				TSharedPtr<const FMCStructureStart> St = GetStructureStart(Set.Type, SC);
				if (St) Apply(*St);
			}
	}
	for (const FMCBlockPos& SP : Strongholds)
	{
		const FMCChunkPos SC = FMCChunkPos::FromBlock(SP);
		if (FMath::Abs(SC.X - C.Pos.X) > 6 || FMath::Abs(SC.Y - C.Pos.Y) > 6) continue;
		TSharedPtr<const FMCStructureStart> St = GetStructureStart(TEXT("stronghold"), SC);
		if (St) Apply(*St);
	}
	// dungeons: from the 3x3 neighbourhood
	for (int32 dy = -1; dy <= 1; ++dy)
		for (int32 dx = -1; dx <= 1; ++dx)
		{
			FMCRandom R = MCGen::ChunkRandom(Seed, C.Pos.X + dx, C.Pos.Y + dy, 0xD06E);
			if (!R.Chance(0.12)) continue;
			const int32 X = (C.Pos.X + dx) * 16 + R.Range(3, 12), Y = (C.Pos.Y + dy) * 16 + R.Range(3, 12), Z = R.Range(-50, 40);
			FMCRandom BR(MCHash::Hash3(Seed ^ 0xD06E0, X, Y, Z));
			FMCPieceBuilder B(W, BR, FIntVector(X, Y, Z), 0);
			const FMCState Cob = S(TEXT("cobblestone")), Mossy = S(TEXT("mossy_cobblestone"));
			for (int32 x = -4; x <= 4; ++x)
				for (int32 y = -3; y <= 3; ++y)
					for (int32 z = -1; z <= 4; ++z)
					{
						const bool bShell = x == -4 || x == 4 || y == -3 || y == 3 || z == -1 || z == 4;
						if (bShell)
						{
							const FIntVector P = B.ToWorld(x, y, z);
							const FMCState Cur = W.Get(P.X, P.Y, P.Z);
							if (Cur == 0 && z != -1) continue; // keep openings into caves
							B.SetVariant(x, y, z, Cob, Mossy, z == -1 ? 0.6f : 0.25f);
						}
						else B.Set(x, y, z, 0);
					}
			const float Roll = BR.NextFloat();
			B.Spawner(0, 0, 0, Roll < 0.5f ? FName(TEXT("zombie")) : (Roll < 0.75f ? FName(TEXT("skeleton")) : FName(TEXT("spider"))));
			B.Chest(-3, 0, 0, EMCFace::East, TEXT("simple_dungeon"));
			if (BR.Chance(0.5)) B.Chest(0, 2, 0, EMCFace::North, TEXT("simple_dungeon"));
		}
}

bool FMCOverworldGen::LocateStructure(FName Type, const FMCBlockPos& Origin, int32 MaxChunks, FMCBlockPos& Out) const
{
	if (Type == TEXT("stronghold"))
	{
		int64 Best = TNumericLimits<int64>::Max();
		for (const FMCBlockPos& SP : Strongholds)
		{
			const int64 D = (int64)(SP.X - Origin.X) * (SP.X - Origin.X) + (int64)(SP.Y - Origin.Y) * (SP.Y - Origin.Y);
			if (D < Best) { Best = D; Out = SP; }
		}
		return Best != TNumericLimits<int64>::Max();
	}
	const FMCStructureSet* Set = StructureSets.FindByPredicate([&](const FMCStructureSet& S2) { return S2.Type == Type; });
	if (!Set) return false;
	const FMCChunkPos OC = FMCChunkPos::FromBlock(Origin);
	const int32 ORX = FMath::FloorToInt((float)OC.X / Set->Spacing), ORY = FMath::FloorToInt((float)OC.Y / Set->Spacing);
	const int32 MaxR = FMath::Max(2, MaxChunks / Set->Spacing + 1);
	int64 Best = TNumericLimits<int64>::Max();
	for (int32 R = 0; R <= MaxR; ++R)
	{
		for (int32 ry = -R; ry <= R; ++ry)
			for (int32 rx = -R; rx <= R; ++rx)
			{
				if (FMath::Max(FMath::Abs(rx), FMath::Abs(ry)) != R) continue;
				FMCChunkPos SC;
				MCGen::GetStructureStartChunk(*Set, Seed, ORX + rx, ORY + ry, SC);
				TSharedPtr<const FMCStructureStart> St = GetStructureStart(Type, SC);
				if (!St || !St->bValid) continue;
				const int64 D = (int64)(St->Origin.X - Origin.X) * (St->Origin.X - Origin.X) + (int64)(St->Origin.Y - Origin.Y) * (St->Origin.Y - Origin.Y);
				if (D < Best) { Best = D; Out = St->Origin; }
			}
		if (Best != TNumericLimits<int64>::Max() && R >= 1) return true;
	}
	return Best != TNumericLimits<int64>::Max();
}

FName FMCOverworldGen::GetStructureAt(const FMCBlockPos& P) const
{
	const FMCChunkPos C = FMCChunkPos::FromBlock(P);
	for (const FMCStructureSet& Set : StructureSets)
	{
		if (Set.Type != TEXT("pillager_outpost") && Set.Type != TEXT("swamp_hut") && Set.Type != TEXT("ocean_monument") && Set.Type != TEXT("woodland_mansion") && Set.Type != TEXT("ancient_city") && Set.Type != TEXT("trial_chambers")) continue;
		const int32 Rad = Set.MaxRadiusChunks;
		const int32 RX0 = FMath::FloorToInt((float)(C.X - Rad) / Set.Spacing), RX1 = FMath::FloorToInt((float)(C.X + Rad) / Set.Spacing);
		const int32 RY0 = FMath::FloorToInt((float)(C.Y - Rad) / Set.Spacing), RY1 = FMath::FloorToInt((float)(C.Y + Rad) / Set.Spacing);
		for (int32 ry = RY0; ry <= RY1; ++ry)
			for (int32 rx = RX0; rx <= RX1; ++rx)
			{
				FMCChunkPos SC;
				MCGen::GetStructureStartChunk(Set, Seed, rx, ry, SC);
				TSharedPtr<const FMCStructureStart> St = GetStructureStart(Set.Type, SC);
				if (!St || !St->bValid) continue;
				if (P.X >= St->Min.X && P.X <= St->Max.X && P.Y >= St->Min.Y && P.Y <= St->Max.Y && P.Z >= St->Min.Z - 2 && P.Z <= St->Max.Z + 2) return Set.Type;
			}
	}
	return NAME_None;
}

// ---------------------------------------------------------------------------------------------------------------------
// Structure creation

TSharedPtr<FMCStructureStart> FMCOverworldGen::CreateStructure(FName Type, const FMCChunkPos& SC) const
{
	TSharedPtr<FMCStructureStart> St = MakeShared<FMCStructureStart>();
	St->Type = Type;
	const int32 CX = SC.MinBlockX() + 8, CY = SC.MinBlockY() + 8;
	FMCRandom R(MCHash::Hash3(Seed ^ MCHash::StringHash(*Type.ToString()), SC.X, SC.Y, 99));
	const FMCClimate Cl = SampleClimate(CX, CY);
	const EMCBiome E = (EMCBiome)PickSurfaceBiome(Cl, (int32)Cl.BaseHeight);
	const int32 GroundZ = ProtoSurface(CX, CY);
	const FMCBiomeDef& BD = FMCBiomes::Get((uint8)E);
	auto IsFlatAround = [&](int32 X, int32 Y, int32 Rad, float MaxDelta)
	{
		const float H0 = SampleClimate(X, Y).BaseHeight;
		for (int32 i = 0; i < 4; ++i)
		{
			const int32 OX = (i & 1) ? Rad : -Rad, OY = (i & 2) ? Rad : -Rad;
			if (FMath::Abs(SampleClimate(X + OX, Y + OY).BaseHeight - H0) > MaxDelta) return false;
		}
		return true;
	};
	uint64 PieceSeed = MCHash::Hash3(Seed, SC.X, SC.Y, 1234);
	auto NextSeed = [&]() { PieceSeed = MCHash::SplitMix64(PieceSeed); return PieceSeed; };

	// ================================================================ VILLAGE
	if (Type == TEXT("village"))
	{
		const bool bOK = (E == EMCBiome::Plains || E == EMCBiome::SunflowerPlains || E == EMCBiome::Meadow || E == EMCBiome::Desert || E == EMCBiome::Savanna
			|| E == EMCBiome::SavannaPlateau || E == EMCBiome::Taiga || E == EMCBiome::SnowyPlains || E == EMCBiome::SnowyTaiga)
			&& GroundZ > MC::SeaLevel && GroundZ < 120 && IsFlatAround(CX, CY, 24, 7.f);
		if (!bOK) return St;
		St->Origin = FMCBlockPos(CX, CY, GroundZ + 1);
		const FVillagePalette Pal = MakePalette(E);
		TArray<FIntRect> Used;
		auto Free = [&](const FIntRect& Rc) { for (const FIntRect& U : Used) if (U.Intersect(Rc)) return false; return true; };
		// centre well
		{
			const FIntVector O(CX, CY, GroundZ);
			Used.Add(FIntRect(CX - 4, CY - 4, CX + 4, CY + 4));
			AddPiece(*St, FIntVector(CX - 4, CY - 4, GroundZ - 4), FIntVector(CX + 4, CY + 4, GroundZ + 7), NextSeed(), [O, Pal](FMCGenWriter& W, FMCRandom& RR)
			{
				FMCPieceBuilder B(W, RR, O, 0);
				BuildWell(B, Pal, RR);
			});
		}
		// roads & houses
		const int32 DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 }; // north, east, south, west
		for (int32 Dir = 0; Dir < 4; ++Dir)
		{
			const int32 Len = R.Range(20, 36);
			TArray<FIntPoint> RoadCells;
			for (int32 i = 4; i <= Len; ++i) RoadCells.Add(FIntPoint(CX + DX[Dir] * i, CY + DY[Dir] * i));
			// side houses
			for (int32 i = 8; i <= Len - 2; i += R.Range(8, 11))
			{
				for (int32 Side = -1; Side <= 1; Side += 2)
				{
					if (!R.Chance(0.8)) continue;
					// perpendicular direction of the side
					const int32 PX = -DY[Dir] * Side, PY = DX[Dir] * Side;
					const float Roll = R.NextFloat();
					int32 Kind; int32 Wd, Dp;
					if (Roll < 0.3f) { Kind = 0; Wd = 5; Dp = 5; }
					else if (Roll < 0.48f) { Kind = 1; Wd = 7; Dp = 9; }
					else if (Roll < 0.58f) { Kind = 2; Wd = 7; Dp = 7; }
					else if (Roll < 0.68f) { Kind = 3; Wd = 9; Dp = 7; }
					else if (Roll < 0.82f) { Kind = 4; Wd = 9; Dp = 7; }
					else if (Roll < 0.9f) { Kind = 5; Wd = 6; Dp = 10; }
					else { Kind = 6; Wd = 8; Dp = 8; }
					// door faces the road: house rotation so its local north points at the road
					// local north (-y) must map to (-PX, -PY)
					int32 Rot = 0;
					if (-PX == 0 && -PY == -1) Rot = 0; else if (-PX == 1 && -PY == 0) Rot = 1; else if (-PX == 0 && -PY == 1) Rot = 2; else Rot = 3;
					const int32 RoadX = CX + DX[Dir] * i, RoadY = CY + DY[Dir] * i;
					// house front edge 3 blocks from the road centre
					const int32 FrontX = RoadX + PX * 3, FrontY = RoadY + PY * 3;
					// origin is the local (0,0) corner; door at local (Wd/2, 0)
					FIntVector O;
					{
						// local (Wd/2, 0) must land on the front point
						int32 LX = Wd / 2, LY = 0;
						int32 RX = LX, RY = LY;
						for (int32 k = 0; k < Rot; ++k) { const int32 T = RX; RX = -RY; RY = T; }
						O = FIntVector(FrontX - RX, FrontY - RY, 0);
					}
					FIntVector BMin, BMax;
					RotBounds(O, Rot, -1, -1, 0, Wd, Dp, 0, BMin, BMax);
					const FIntRect Rect(BMin.X, BMin.Y, BMax.X + 1, BMax.Y + 1);
					if (!Free(Rect)) continue;
					const int32 HX = (BMin.X + BMax.X) / 2, HY = (BMin.Y + BMax.Y) / 2;
					const int32 HZ = ProtoSurface(HX, HY);
					if (HZ <= MC::SeaLevel - 1) continue;
					Used.Add(Rect);
					O.Z = HZ;
					AddPiece(*St, FIntVector(BMin.X - 1, BMin.Y - 1, HZ - 12), FIntVector(BMax.X + 1, BMax.Y + 1, HZ + 17), NextSeed(), [O, Rot, Kind, Pal](FMCGenWriter& W, FMCRandom& RR)
					{
						FMCPieceBuilder B(W, RR, O, Rot);
						switch (Kind)
						{
						case 0: BuildHouseSmall(B, Pal, RR); break;
						case 1: BuildHouseBig(B, Pal, RR); break;
						case 2: BuildLibrary(B, Pal, RR); break;
						case 3: BuildSmith(B, Pal, RR); break;
						case 4: BuildFarm(B, Pal, RR); break;
						case 5: BuildChurch(B, Pal, RR); break;
						default: BuildPen(B, Pal, RR); break;
						}
					});
				}
				// lamp post
				if (R.Chance(0.5))
				{
					const int32 LX = CX + DX[Dir] * (i + 4) + (-DY[Dir]) * 2, LY = CY + DY[Dir] * (i + 4) + DX[Dir] * 2;
					const FIntVector O(LX, LY, ProtoSurface(LX, LY));
					AddPiece(*St, FIntVector(LX - 1, LY - 1, O.Z), FIntVector(LX + 1, LY + 1, O.Z + 4), NextSeed(), [O, Pal](FMCGenWriter& W, FMCRandom& RR)
					{
						// find the actual ground in the chunk
						int32 Z = O.Z + 6;
						while (Z > O.Z - 8 && (W.Get(O.X, O.Y, Z) == 0 || !FMCBlocks::IsSolid(W.Get(O.X, O.Y, Z)) || (FMCBlocks::Info(W.Get(O.X, O.Y, Z)).Flags & MCB_Leaves))) --Z;
						FMCPieceBuilder B(W, RR, FIntVector(O.X, O.Y, Z), 0);
						BuildLamp(B, Pal, RR);
					});
				}
			}
			// road piece: 3 wide path following the real ground
			const FIntPoint A = RoadCells[0], Bp = RoadCells.Last();
			const FMCState PathB = Pal.Path;
			const int32 RefZ = GroundZ;
			AddPiece(*St, FIntVector(FMath::Min(A.X, Bp.X) - 1, FMath::Min(A.Y, Bp.Y) - 1, RefZ - 16), FIntVector(FMath::Max(A.X, Bp.X) + 1, FMath::Max(A.Y, Bp.Y) + 1, RefZ + 24), NextSeed(),
				[RoadCells, PathB, RefZ, Dir](FMCGenWriter& W, FMCRandom& RR)
			{
				const int32 PX = (Dir == 0 || Dir == 2) ? 1 : 0, PY = (Dir == 1 || Dir == 3) ? 1 : 0;
				for (const FIntPoint& Cp : RoadCells)
				{
					for (int32 o = -1; o <= 1; ++o)
					{
						const int32 X = Cp.X + PX * o, Y = Cp.Y + PY * o;
						if (!W.InsideXY(X, Y)) continue;
						int32 Z = RefZ + 20;
						while (Z > RefZ - 16)
						{
							const FMCState Cur = W.Get(X, Y, Z);
							if (Cur != 0 && FMCBlocks::IsSolid(Cur) && !(FMCBlocks::Info(Cur).Flags & (MCB_Leaves | MCB_Log))) break;
							--Z;
						}
						const FMCState Ground = W.Get(X, Y, Z);
						if (Ground == 0) continue;
						const FMCBlock& GB = FMCBlocks::GetByState(Ground);
						if (FMCBlocks::IsFluid(Ground) || Z < MC::SeaLevel - 1) { W.Set(X, Y, FMath::Max(Z, MC::SeaLevel - 1), S(TEXT("oak_planks"))); continue; }
						if (GB.Has(MCB_Soil) || GB.Has(MCB_Sand) || GB.Name == TEXT("gravel") || GB.Name == TEXT("sandstone") || GB.Has(MCB_Stone))
						{
							W.Set(X, Y, Z, PathB);
						}
						for (int32 k = 1; k <= 3; ++k)
						{
							const FMCState Up = W.Get(X, Y, Z + k);
							if (Up != 0 && ((FMCBlocks::Info(Up).Flags & MCB_Plant) || FMCBlocks::IsReplaceable(Up))) W.Set(X, Y, Z + k, 0);
						}
					}
				}
			});
		}
		St->bValid = true;
		return St;
	}

	// ================================================================ STRONGHOLD
	if (Type == TEXT("stronghold"))
	{
		const FMCBlockPos* SP = Strongholds.FindByPredicate([&](const FMCBlockPos& P) { return FMCChunkPos::FromBlock(P) == SC; });
		if (!SP) return St;
		const int32 BaseZ = SP->Z;
		St->Origin = *SP;
		const FIntVector O(SP->X, SP->Y, BaseZ);
		// 3x3 grid of rooms, spacing 14, connected by corridors; portal room at a random edge cell
		const int32 Cells = 3, Sp = 14;
		const int32 PortalCell = R.NextInt(9);
		for (int32 cy = 0; cy < Cells; ++cy)
			for (int32 cx = 0; cx < Cells; ++cx)
			{
				const int32 Idx = cx + cy * Cells;
				const FIntVector RC(O.X + (cx - 1) * Sp, O.Y + (cy - 1) * Sp, BaseZ);
				int32 Kind = R.NextInt(5); // 0 corridor hub, 1 library, 2 fountain, 3 storage, 4 prison
				if (Idx == PortalCell) Kind = 9;
				if (Idx == 4 && Kind == 9) {}
				const bool bLeft = cx > 0, bRight = cx < Cells - 1, bUp = cy > 0, bDown = cy < Cells - 1;
				AddPiece(*St, FIntVector(RC.X - 7, RC.Y - 8, BaseZ - 2), FIntVector(RC.X + 7, RC.Y + 8, BaseZ + 12), NextSeed(),
					[RC, Kind, bLeft, bRight, bUp, bDown](FMCGenWriter& W, FMCRandom& RR)
				{
					FMCPieceBuilder B(W, RR, RC, 0);
					const FMCState SB = S(TEXT("stone_bricks")), Mossy = S(TEXT("mossy_stone_bricks")), Cracked = S(TEXT("cracked_stone_bricks"));
					auto Wall = [&](int32 x, int32 y, int32 z) { B.SetVariant(x, y, z, SB, Mossy, 0.2f, Cracked, 0.12f); };
					if (Kind == 9)
					{
						// ---- portal room (11 x 16)
						for (int32 x = -5; x <= 5; ++x) for (int32 y = -7; y <= 8; ++y) for (int32 z = -1; z <= 8; ++z)
						{
							if (x == -5 || x == 5 || y == -7 || y == 8 || z == -1 || z == 8) Wall(x, y, z); else B.Set(x, y, z, 0);
						}
						// iron bar windows
						for (int32 y = -5; y <= 6; y += 3) { B.Set(-5, y, 3, S(TEXT("iron_bars"))); B.Set(5, y, 3, S(TEXT("iron_bars"))); }
						// lava pool under the frame
						B.Fill(-2, 1, 0, 2, 5, 0, S(TEXT("lava")));
						B.Fill(-3, 0, 0, 3, 0, 0, SB); B.Fill(-3, 6, 0, 3, 6, 0, SB);
						B.Fill(-3, 0, 0, -3, 6, 0, SB); B.Fill(3, 0, 0, 3, 6, 0, SB);
						// stairs up to the portal platform
						const FMCState Stairs = S(TEXT("stone_brick_stairs"));
						for (int32 x = -1; x <= 1; ++x) { B.Stair(x, -2, 0, Stairs, EMCFace::South); B.Fill(x, -1, 0, x, -1, 0, SB); }
						// end portal frame ring (12 frames around a 3x3 hole) at z = 1
						const FMCBlock* Frame = FMCBlocks::Find(TEXT("end_portal_frame"));
						for (int32 i = -1; i <= 1; ++i)
						{
							// frames face inward; facing = direction towards the centre
							const bool bEye1 = RR.Chance(0.1), bEye2 = RR.Chance(0.1), bEye3 = RR.Chance(0.1), bEye4 = RR.Chance(0.1);
							B.SetRaw(i, 1, 1, Frame->State(MCMeta::FromFacing4(EMCFace::South) | (bEye1 ? 4 : 0)));  // north row faces south
							B.SetRaw(i, 5, 1, Frame->State(MCMeta::FromFacing4(EMCFace::North) | (bEye2 ? 4 : 0)));
							B.SetRaw(-2, 3 + i, 1, Frame->State(MCMeta::FromFacing4(EMCFace::East) | (bEye3 ? 4 : 0)));
							B.SetRaw(2, 3 + i, 1, Frame->State(MCMeta::FromFacing4(EMCFace::West) | (bEye4 ? 4 : 0)));
						}
						B.Fill(-1, 2, 1, 1, 4, 1, 0);
						B.Fill(-1, 2, 0, 1, 4, 0, S(TEXT("lava")));
						B.Spawner(0, -4, 1, TEXT("silverfish"));
						B.Torch(-4, -6, 3, EMCFace::East); B.Torch(4, -6, 3, EMCFace::West);
					}
					else
					{
						// ---- generic room 11 x 11 x 6
						const int32 H = Kind == 1 ? 8 : 6;
						for (int32 x = -5; x <= 5; ++x) for (int32 y = -5; y <= 5; ++y) for (int32 z = -1; z <= H; ++z)
						{
							if (x == -5 || x == 5 || y == -5 || y == 5 || z == -1 || z == H) Wall(x, y, z); else B.Set(x, y, z, 0);
						}
						switch (Kind)
						{
						case 1: // library
							for (int32 x = -4; x <= 4; ++x) for (int32 z = 0; z <= 5; ++z) { B.Set(x, -4, z, S(TEXT("bookshelf"))); B.Set(x, 4, z, S(TEXT("bookshelf"))); }
							for (int32 y = -3; y <= 3; y += 3) for (int32 z = 0; z <= 2; ++z) { B.Set(-2, y, z, S(TEXT("bookshelf"))); B.Set(2, y, z, S(TEXT("bookshelf"))); }
							B.Fill(-4, -4, 4, 4, 4, 4, S(TEXT("oak_planks")));
							B.Fill(-3, -3, 4, 3, 3, 4, 0);
							B.Chest(0, -3, 0, EMCFace::South, TEXT("stronghold_library"));
							B.Chest(-4, 0, 5, EMCFace::East, TEXT("stronghold_library"));
							B.SetRaw(0, 0, 7, S(TEXT("chain"))); B.SetRaw(0, 0, 6, WithMeta(TEXT("lantern"), 1));
							break;
						case 2: // fountain
							B.Fill(-1, -1, 0, 1, 1, 0, SB);
							B.Set(0, 0, 1, SB); B.Set(0, 0, 2, SB); B.Set(0, 0, 3, S(TEXT("water")));
							B.Torch(-4, 0, 2, EMCFace::East); B.Torch(4, 0, 2, EMCFace::West);
							break;
						case 3: // storage
							B.Fill(-4, 2, 0, 4, 4, 0, S(TEXT("oak_planks")));
							B.Chest(-3, 3, 1, EMCFace::North, TEXT("stronghold_crossing"));
							B.Chest(3, 3, 1, EMCFace::North, TEXT("stronghold_crossing"));
							B.Set(0, 3, 1, S(TEXT("crafting_table")));
							B.Torch(0, 4, 3, EMCFace::North);
							break;
						case 4: // prison cells
							for (int32 x = -4; x <= 4; x += 4)
							{
								for (int32 z = 0; z <= 2; ++z) { B.Set(x, 1, z, S(TEXT("iron_bars"))); B.Set(x, 2, z, S(TEXT("iron_bars"))); }
							}
							B.Fill(-4, 2, 0, 4, 2, 2, S(TEXT("iron_bars")));
							B.Door(0, 2, 0, TEXT("iron_door"), EMCFace::North);
							break;
						default:
							B.Torch(-4, -4, 2, EMCFace::East); B.Torch(4, 4, 2, EMCFace::West);
							if (RR.Chance(0.3)) B.Chest(4, -4, 0, EMCFace::West, TEXT("stronghold_corridor"));
							break;
						}
					}
					// doorways + corridors to the neighbours (corridor halves: each room builds half the gap)
					auto Corr = [&](int32 DXc, int32 DYc)
					{
						for (int32 k = 5; k <= 9; ++k)
							for (int32 o = -2; o <= 2; ++o)
								for (int32 z = -1; z <= 4; ++z)
								{
									const int32 x = DXc != 0 ? DXc * k : o, y = DYc != 0 ? DYc * k : o;
									if (FMath::Abs(o) == 2 || z == -1 || z == 4) { if (k != 5) Wall(x, y, z); }
									else B.Set(x, y, z, 0);
								}
						// doorway in the room wall
						for (int32 o = -1; o <= 1; ++o) for (int32 z = 0; z <= 2; ++z)
						{
							const int32 x = DXc != 0 ? DXc * 5 : o, y = DYc != 0 ? DYc * 5 : o;
							B.Set(x, y, z, 0);
						}
						const int32 TX = DXc != 0 ? DXc * 7 : 1, TY = DYc != 0 ? DYc * 7 : 1;
						if (RR.Chance(0.5)) B.Torch(TX, TY, 2, EMCFace::Up);
					};
					if (bLeft) Corr(-1, 0);
					if (bRight) Corr(1, 0);
					if (bUp) Corr(0, -1);
					if (bDown) Corr(0, 1);
				});
			}
		St->bValid = true;
		return St;
	}

	// ================================================================ MINESHAFT
	if (Type == TEXT("mineshaft"))
	{
		if (!R.Chance(0.7)) return St;
		const bool bBadlands = E == EMCBiome::Badlands || E == EMCBiome::ErodedBadlands || E == EMCBiome::WoodedBadlands;
		const int32 Z0 = bBadlands ? R.Range(40, 60) : R.Range(-20, 30);
		St->Origin = FMCBlockPos(CX, CY, Z0);
		const FMCState Wood = bBadlands ? S(TEXT("dark_oak_planks")) : S(TEXT("oak_planks"));
		const FMCState FenceB = bBadlands ? S(TEXT("dark_oak_fence")) : S(TEXT("oak_fence"));
		// start room
		{
			const FIntVector O(CX, CY, Z0);
			AddPiece(*St, FIntVector(CX - 6, CY - 6, Z0 - 1), FIntVector(CX + 6, CY + 6, Z0 + 6), NextSeed(), [O](FMCGenWriter& W, FMCRandom& RR)
			{
				FMCPieceBuilder B(W, RR, O, 0);
				B.Fill(-5, -5, 0, 5, 5, 4, 0);
				B.Fill(-5, -5, -1, 5, 5, -1, S(TEXT("dirt")));
			});
		}
		struct FSeg { FIntVector Start; int32 Dir; int32 Depth; };
		TArray<FSeg> Queue;
		for (int32 d = 0; d < 4; ++d) Queue.Add({ FIntVector(CX, CY, Z0), d, 0 });
		const int32 DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
		int32 Pieces = 0;
		while (Queue.Num() && Pieces < 40)
		{
			const FSeg Sg = Queue[0]; Queue.RemoveAt(0);
			const int32 Len = R.Range(3, 7) * 5;
			FIntVector Start = Sg.Start + FIntVector(DX[Sg.Dir] * 6, DY[Sg.Dir] * 6, 0);
			const FIntVector End = Start + FIntVector(DX[Sg.Dir] * Len, DY[Sg.Dir] * Len, 0);
			if (FMath::Abs(End.X - CX) > 90 || FMath::Abs(End.Y - CY) > 90) continue;
			++Pieces;
			const int32 Dir = Sg.Dir;
			AddPiece(*St, FIntVector(FMath::Min(Start.X, End.X) - 2, FMath::Min(Start.Y, End.Y) - 2, Start.Z - 1), FIntVector(FMath::Max(Start.X, End.X) + 2, FMath::Max(Start.Y, End.Y) + 2, Start.Z + 4), NextSeed(),
				[Start, Len, Dir, Wood, FenceB](FMCGenWriter& W, FMCRandom& RR)
			{
				const int32 DX2[4] = { 0, 1, 0, -1 }, DY2[4] = { -1, 0, 1, 0 };
				const int32 PX = -DY2[Dir], PY = DX2[Dir];
				const FMCState Rail = FMCBlocks::GetByState(S(TEXT("rail"))).State((Dir == 0 || Dir == 2) ? 0 : 1);
				const FMCState Web = S(TEXT("cobweb"));
				for (int32 k = 0; k <= Len; ++k)
				{
					const int32 X = Start.X + DX2[Dir] * k, Y = Start.Y + DY2[Dir] * k;
					for (int32 o = -1; o <= 1; ++o)
						for (int32 z = 0; z <= 2; ++z)
							W.Set(X + PX * o, Y + PY * o, Start.Z + z, 0);
					// floor where the corridor crosses caves
					for (int32 o = -1; o <= 1; ++o)
					{
						const FMCState Fl = W.Get(X + PX * o, Y + PY * o, Start.Z - 1);
						if (Fl == 0 || FMCBlocks::IsFluid(Fl)) W.Set(X + PX * o, Y + PY * o, Start.Z - 1, Wood);
					}
					if (RR.Chance(0.7)) W.Set(X, Y, Start.Z, Rail);
					if (k % 5 == 2)
					{
						W.Set(X + PX * -1, Y + PY * -1, Start.Z, FenceB); W.Set(X + PX * -1, Y + PY * -1, Start.Z + 1, FenceB);
						W.Set(X + PX * 1, Y + PY * 1, Start.Z, FenceB); W.Set(X + PX * 1, Y + PY * 1, Start.Z + 1, FenceB);
						for (int32 o = -1; o <= 1; ++o) W.Set(X + PX * o, Y + PY * o, Start.Z + 2, Wood);
						if (RR.Chance(0.25)) W.Set(X, Y, Start.Z + 1, 0);
						if (RR.Chance(0.15)) W.Set(X + PX * -1 + DX2[Dir], Y + PY * -1 + DY2[Dir], Start.Z + 1, S(TEXT("torch")));
					}
					else if (RR.Chance(0.04)) W.Set(X + PX * RR.Range(-1, 1), Y + PY * RR.Range(-1, 1), Start.Z + RR.Range(1, 2), Web);
					if (RR.Chance(0.008)) W.AddChest(X + PX, Y + PY, Start.Z, EMCFace::North, TEXT("abandoned_mineshaft"), MCHash::Hash3(0xAB, X, Y, Start.Z));
					if (RR.Chance(0.004))
					{
						W.AddSpawner(X, Y, Start.Z + 1, TEXT("cave_spider"));
						for (int32 w = 0; w < 10; ++w) { const int32 WX = X + RR.Range(-2, 2), WY = Y + RR.Range(-2, 2), WZ = Start.Z + RR.Range(0, 2); if (W.Get(WX, WY, WZ) == 0) W.Set(WX, WY, WZ, Web); }
					}
				}
			});
			if (Sg.Depth < 3)
			{
				const int32 Br = R.Range(1, 3);
				for (int32 b = 0; b < Br; ++b)
				{
					const int32 ND = (Sg.Dir + (b == 0 ? 0 : (R.NextBool() ? 1 : 3))) & 3;
					Queue.Add({ End + FIntVector(0, 0, R.Chance(0.2) ? R.Range(-4, 4) : 0), ND, Sg.Depth + 1 });
				}
			}
		}
		St->bValid = true;
		return St;
	}

	// ================================================================ RUINED PORTAL
	if (Type == TEXT("ruined_portal"))
	{
		if (BD.bOcean && R.Chance(0.5)) return St;
		const bool bUnder = R.Chance(0.25);
		const int32 Z = bUnder ? R.Range(10, GroundZ - 10) : GroundZ + 1;
		St->Origin = FMCBlockPos(CX, CY, Z);
		const FIntVector O(CX, CY, Z);
		const int32 Rot = R.NextInt(4);
		AddPiece(*St, FIntVector(CX - 6, CY - 6, Z - 3), FIntVector(CX + 6, CY + 6, Z + 8), NextSeed(), [O, Rot](FMCGenWriter& W, FMCRandom& RR)
		{
			FMCPieceBuilder B(W, RR, O, Rot);
			const FMCState Obs = S(TEXT("obsidian")), Cry = S(TEXT("crying_obsidian")), Nrk = S(TEXT("netherrack")), Mag = S(TEXT("magma_block"));
			// base patch
			for (int32 x = -4; x <= 5; ++x) for (int32 y = -3; y <= 3; ++y)
			{
				if (x * x + y * y * 2 > 22 + RR.NextInt(6)) continue;
				B.Set(x, y, -1, RR.Chance(0.5) ? Nrk : (RR.Chance(0.3) ? Mag : S(TEXT("blackstone"))));
				if (RR.Chance(0.15)) B.Set(x, y, 0, S(TEXT("netherrack")));
			}
			B.Fill(-1, 0, 0, 2, 0, 4, 0);
			// broken frame (4 wide x 5 high)
			for (int32 x = -1; x <= 2; ++x)
			{
				B.Set(x, 0, 0, RR.Chance(0.2) ? Cry : Obs);
				if (RR.Chance(0.6)) B.Set(x, 0, 4, RR.Chance(0.2) ? Cry : Obs);
			}
			for (int32 z = 1; z <= 3; ++z)
			{
				if (RR.Chance(0.85)) B.Set(-1, 0, z, RR.Chance(0.2) ? Cry : Obs);
				if (RR.Chance(0.7)) B.Set(2, 0, z, RR.Chance(0.2) ? Cry : Obs);
			}
			// scattered obsidian and loot
			for (int32 i = 0; i < 3; ++i) B.Set(RR.Range(-3, 4), RR.Range(-2, 2), 0, Obs);
			B.Chest(3, 2, 0, EMCFace::West, TEXT("ruined_portal"));
			if (RR.Chance(0.5)) B.Set(-3, 1, 0, S(TEXT("gold_block")));
			if (RR.Chance(0.4)) B.Set(4, -2, -1, S(TEXT("lava")));
		});
		St->bValid = true;
		return St;
	}

	// ================================================================ DESERT PYRAMID
	if (Type == TEXT("desert_pyramid"))
	{
		if (E != EMCBiome::Desert || !IsFlatAround(CX, CY, 10, 4.f)) return St;
		const int32 Z = GroundZ;
		St->Origin = FMCBlockPos(CX, CY, Z + 1);
		const FIntVector O(CX, CY, Z);
		AddPiece(*St, FIntVector(CX - 11, CY - 11, Z - 15), FIntVector(CX + 11, CY + 11, Z + 16), NextSeed(), [O](FMCGenWriter& W, FMCRandom& RR)
		{
			FMCPieceBuilder B(W, RR, O, 0);
			const FMCState SS = S(TEXT("sandstone")), Cut = S(TEXT("cut_sandstone")), Chis = S(TEXT("chiseled_sandstone")), Orange = S(TEXT("orange_terracotta")), Blue = S(TEXT("blue_terracotta"));
			const FMCState SSt = S(TEXT("sandstone_stairs"));
			B.Foundation(-10, -10, 10, 10, 0, SS, 10);
			// stepped pyramid
			for (int32 L = 0; L <= 10; ++L)
			{
				const int32 Rr = 10 - L;
				for (int32 x = -Rr; x <= Rr; ++x) for (int32 y = -Rr; y <= Rr; ++y)
				{
					const bool bEdge = FMath::Abs(x) == Rr || FMath::Abs(y) == Rr;
					B.Set(x, y, L + 1, bEdge ? SS : 0);
				}
			}
			B.Fill(-10, -10, 0, 10, 10, 0, SS);
			B.Fill(-8, -8, 1, 8, 8, 6, 0);
			for (int32 x = -8; x <= 8; ++x) for (int32 y = -8; y <= 8; ++y) if ((FMath::Abs(x) == 8 || FMath::Abs(y) == 8)) for (int32 z = 1; z <= 5; ++z) B.Set(x, y, z, SS);
			B.Fill(-8, -8, 6, 8, 8, 6, SS);
			B.Fill(-7, -7, 1, 7, 7, 5, 0);
			// entrance
			B.Fill(-1, -10, 1, 1, -8, 3, 0);
			for (int32 x = -1; x <= 1; ++x) B.Stair(x, -11, 1, SSt, EMCFace::South);
			// floor decoration
			for (int32 x = -2; x <= 2; ++x) for (int32 y = -2; y <= 2; ++y) B.Set(x, y, 0, (FMath::Abs(x) + FMath::Abs(y)) % 2 ? Orange : Cut);
			B.Set(0, 0, 0, Blue);
			// hidden treasure pit with TNT trap
			B.Fill(-1, -1, -11, 1, 1, -1, 0);
			B.Fill(-2, -2, -12, 2, 2, -12, SS);
			B.Fill(-3, -3, -11, 3, 3, -5, SS);
			B.Fill(-2, -2, -11, 2, 2, -5, 0);
			B.Fill(-1, -1, -11, 1, 1, -11, S(TEXT("tnt")));
			B.Set(0, 0, -10, S(TEXT("stone_pressure_plate")));
			B.Fill(-1, -1, -5, 1, 1, -1, 0);
			B.Chest(0, -2, -10, EMCFace::South, TEXT("desert_pyramid"));
			B.Chest(0, 2, -10, EMCFace::North, TEXT("desert_pyramid"));
			B.Chest(-2, 0, -10, EMCFace::East, TEXT("desert_pyramid"));
			B.Chest(2, 0, -10, EMCFace::West, TEXT("desert_pyramid"));
			// towers
			for (int32 s = -1; s <= 1; s += 2)
			{
				for (int32 z = 1; z <= 12; ++z)
					for (int32 x = 0; x <= 3; ++x) for (int32 y = 0; y <= 3; ++y)
					{
						const bool bEdge = x == 0 || x == 3 || y == 0 || y == 3;
						B.Set(s * 7 + (s < 0 ? -x : x) * 1 + (s < 0 ? 0 : 0), -10 + y, z, bEdge ? (z % 4 == 0 ? Orange : SS) : 0);
					}
				B.Set(s * 8, -10, 9, Chis);
			}
			B.Mob(TEXT("husk"), 0, 3, 1);
		});
		St->bValid = true;
		return St;
	}

	// ================================================================ JUNGLE TEMPLE
	if (Type == TEXT("jungle_temple"))
	{
		if (E != EMCBiome::Jungle && E != EMCBiome::BambooJungle && E != EMCBiome::SparseJungle) return St;
		const int32 Z = GroundZ;
		St->Origin = FMCBlockPos(CX, CY, Z + 1);
		const FIntVector O(CX - 6, CY - 7, Z);
		AddPiece(*St, FIntVector(CX - 8, CY - 9, Z - 6), FIntVector(CX + 8, CY + 9, Z + 14), NextSeed(), [O](FMCGenWriter& W, FMCRandom& RR)
		{
			FMCPieceBuilder B(W, RR, O, 0);
			const FMCState Cob = S(TEXT("cobblestone")), Mossy = S(TEXT("mossy_cobblestone")), Chis = S(TEXT("chiseled_stone_bricks"));
			auto M = [&](int32 x, int32 y, int32 z) { B.SetVariant(x, y, z, Cob, Mossy, 0.5f); };
			B.Foundation(0, 0, 11, 14, 0, Cob, 12);
			for (int32 x = 0; x <= 11; ++x) for (int32 y = 0; y <= 14; ++y) for (int32 z = 0; z <= 9; ++z)
			{
				const bool bShell = x == 0 || x == 11 || y == 0 || y == 14 || z == 0 || z == 4 || z == 9;
				if (bShell) M(x, y, z); else B.Set(x, y, z, 0);
			}
			for (int32 x = 2; x <= 9; ++x) for (int32 y = 2; y <= 12; ++y) for (int32 z = 10; z <= 13; ++z)
			{
				const int32 Sh = z - 10;
				if (x >= 2 + Sh && x <= 9 - Sh && y >= 2 + Sh && y <= 12 - Sh && (x == 2 + Sh || x == 9 - Sh || y == 2 + Sh || y == 12 - Sh || z == 13)) M(x, y, z);
			}
			B.Fill(4, 0, 1, 7, 0, 3, 0);
			B.Fill(4, 0, 5, 7, 0, 7, 0);
			for (int32 y = 3; y <= 11; y += 4) { B.Set(0, y, 2, 0); B.Set(11, y, 2, 0); B.Set(0, y, 6, S(TEXT("vine"))); }
			B.Set(5, 7, 0, Chis); B.Set(6, 7, 0, Chis);
			// basement
			B.Fill(1, 1, -5, 10, 13, -1, Cob);
			B.Fill(2, 2, -4, 9, 12, -1, 0);
			B.Fill(9, 11, -1, 9, 11, 0, 0);
			for (int32 z = -4; z <= 3; ++z) B.Set(9, 12, z, WithMeta(TEXT("ladder"), 0));
			B.Chest(3, 11, -4, EMCFace::South, TEXT("jungle_temple"));
			B.Chest(8, 3, -4, EMCFace::West, TEXT("jungle_temple"));
			B.Set(5, 6, -4, S(TEXT("dispenser")));
		});
		St->bValid = true;
		return St;
	}

	// ================================================================ SWAMP HUT
	if (Type == TEXT("swamp_hut"))
	{
		if (E != EMCBiome::Swamp) return St;
		const int32 Z = MC::SeaLevel + 1;
		St->Origin = FMCBlockPos(CX, CY, Z);
		const FIntVector O(CX - 3, CY - 4, Z);
		AddPiece(*St, FIntVector(CX - 5, CY - 6, Z - 8), FIntVector(CX + 5, CY + 6, Z + 8), NextSeed(), [O](FMCGenWriter& W, FMCRandom& RR)
		{
			FMCPieceBuilder B(W, RR, O, 0);
			const FMCState Pl = S(TEXT("spruce_planks")), Log = S(TEXT("oak_log"));
			for (int32 x : { 0, 6 }) for (int32 y : { 1, 7 }) for (int32 z = -6; z <= 1; ++z) B.Set(x, y, z, Log);
			B.Fill(0, 1, 1, 6, 7, 1, Pl);
			for (int32 x = 0; x <= 6; ++x) for (int32 y = 1; y <= 7; ++y) for (int32 z = 2; z <= 4; ++z)
				if (x == 0 || x == 6 || y == 1 || y == 7) B.Set(x, y, z, Pl); else B.Set(x, y, z, 0);
			B.Fill(-1, 0, 5, 7, 8, 5, S(TEXT("spruce_planks")));
			B.Set(3, 1, 2, 0); B.Set(3, 1, 3, 0);
			B.Set(0, 4, 3, S(TEXT("oak_fence"))); B.Set(6, 4, 3, S(TEXT("oak_fence")));
			B.Set(1, 6, 2, S(TEXT("crafting_table")));
			B.Set(5, 6, 2, WithMeta(TEXT("cauldron"), 0));
			B.Set(1, 2, 2, WithMeta(TEXT("flower_pot"), 23));
			B.Fill(2, 0, 1, 4, 0, 1, Pl);
			B.Mob(TEXT("witch"), 3, 4, 2);
			B.Mob(TEXT("cat"), 2, 5, 2, 10);
		});
		St->bValid = true;
		return St;
	}

	// ================================================================ IGLOO
	if (Type == TEXT("igloo"))
	{
		if (E != EMCBiome::SnowyPlains && E != EMCBiome::SnowyTaiga && E != EMCBiome::SnowySlopes) return St;
		const int32 Z = GroundZ + 1;
		St->Origin = FMCBlockPos(CX, CY, Z);
		const FIntVector O(CX, CY, Z);
		AddPiece(*St, FIntVector(CX - 5, CY - 7, Z - 3), FIntVector(CX + 5, CY + 5, Z + 6), NextSeed(), [O](FMCGenWriter& W, FMCRandom& RR)
		{
			FMCPieceBuilder B(W, RR, O, 0);
			const FMCState Snow = S(TEXT("snow_block")), Ice = S(TEXT("ice"));
			B.Foundation(-4, -4, 4, 4, -1, Snow, 6);
			for (int32 x = -4; x <= 4; ++x) for (int32 y = -4; y <= 4; ++y) for (int32 z = -1; z <= 4; ++z)
			{
				const float D = FMath::Sqrt((float)(x * x + y * y + (z + 0.5f) * (z + 0.5f) * 1.5f));
				if (z == -1) { if (x * x + y * y <= 17) B.Set(x, y, z, Snow); continue; }
				if (D <= 4.4f && D > 3.3f) B.Set(x, y, z, Snow);
				else if (D <= 3.3f) B.Set(x, y, z, 0);
			}
			B.Fill(-1, -6, 0, 1, -4, 2, Snow);
			B.Fill(0, -6, 0, 0, -3, 1, 0);
			B.Set(-3, 0, 1, Ice); B.Set(3, 0, 1, Ice);
			B.Bed(-2, 1, 0, TEXT("red_bed"), EMCFace::South);
			B.Set(2, 2, 0, S(TEXT("furnace")));
			B.Set(2, 1, 0, S(TEXT("crafting_table")));
			B.Set(-2, -2, 0, WithMeta(TEXT("redstone_torch"), 0));
			B.Fill(-1, -1, -1, 1, 1, -1, WithMeta(TEXT("white_carpet"), 0));
			B.Fill(-1, -1, -1, 1, 1, -1, Snow);
			B.Fill(-1, 0, 0, 1, 0, 0, WithMeta(TEXT("white_carpet"), 0));
			B.Chest(0, 3, 0, EMCFace::North, TEXT("igloo_chest"));
		});
		St->bValid = true;
		return St;
	}

	// ================================================================ PILLAGER OUTPOST
	if (Type == TEXT("pillager_outpost"))
	{
		if (!(E == EMCBiome::Plains || E == EMCBiome::Desert || E == EMCBiome::Savanna || E == EMCBiome::Taiga || E == EMCBiome::SnowyPlains || E == EMCBiome::Meadow || E == EMCBiome::Grove || E == EMCBiome::CherryGrove) || !R.Chance(0.35)) return St;
		const int32 Z = GroundZ;
		St->Origin = FMCBlockPos(CX, CY, Z + 1);
		const FIntVector O(CX, CY, Z);
		AddPiece(*St, FIntVector(CX - 9, CY - 9, Z - 10), FIntVector(CX + 9, CY + 9, Z + 22), NextSeed(), [O](FMCGenWriter& W, FMCRandom& RR)
		{
			FMCPieceBuilder B(W, RR, O, 0);
			const FMCState Log = S(TEXT("dark_oak_log")), Pl = S(TEXT("dark_oak_planks")), Cob = S(TEXT("cobblestone")), BF = S(TEXT("birch_fence")), Birch = S(TEXT("birch_planks"));
			B.Foundation(-3, -3, 3, 3, 0, Cob, 10);
			B.Fill(-3, -3, 0, 3, 3, 0, Cob);
			for (int32 Fl = 0; Fl < 4; ++Fl)
			{
				const int32 Z0 = 1 + Fl * 5;
				for (int32 z = Z0; z < Z0 + 4; ++z)
					for (int32 x = -3; x <= 3; ++x) for (int32 y = -3; y <= 3; ++y)
					{
						const bool bCorner = FMath::Abs(x) == 3 && FMath::Abs(y) == 3;
						const bool bWall = FMath::Abs(x) == 3 || FMath::Abs(y) == 3;
						if (bCorner) B.Set(x, y, z, Log);
						else if (bWall) B.Set(x, y, z, (z == Z0 + 1 && (x == 0 || y == 0)) ? BF : Pl);
						else B.Set(x, y, z, 0);
					}
				B.Fill(-3, -3, Z0 + 4, 3, 3, Z0 + 4, Birch);
				B.Set(2, 2, Z0 + 4, 0);
				for (int32 z = Z0; z <= Z0 + 4; ++z) B.Set(2, 2, z, WithMeta(TEXT("ladder"), 2));
			}
			// top deck
			const int32 TopZ = 21;
			B.Fill(-4, -4, TopZ, 4, 4, TopZ, Pl);
			for (int32 x = -4; x <= 4; ++x) for (int32 y = -4; y <= 4; ++y) if (FMath::Abs(x) == 4 || FMath::Abs(y) == 4) B.Set(x, y, TopZ + 1, BF);
			B.Chest(0, 0, TopZ + 1, EMCFace::South, TEXT("pillager_outpost"));
			B.Set(-2, -2, TopZ + 1, S(TEXT("crafting_table")));
			B.Torch(3, 3, TopZ + 1, EMCFace::Up);
			B.Door(0, -3, 1, TEXT("dark_oak_door"), EMCFace::North);
			for (int32 i = 0; i < 4; ++i) B.Mob(TEXT("pillager"), (i & 1) ? 5.f : -5.f, (i & 2) ? 5.f : -5.f, 1);
			B.Mob(TEXT("pillager"), 1, 1, TopZ + 1);
			// cage
			if (RR.Chance(0.6))
			{
				for (int32 x = 7; x <= 9; ++x) for (int32 y = -1; y <= 1; ++y) for (int32 z = 1; z <= 3; ++z)
					if (x == 7 || x == 9 || y == -1 || y == 1 || z == 3) B.Set(x, y, z, S(TEXT("dark_oak_fence")));
				B.Mob(TEXT("iron_golem"), 8, 0, 1);
			}
		});
		St->bValid = true;
		return St;
	}

	// ================================================================ SHIPWRECK
	if (Type == TEXT("shipwreck"))
	{
		const bool bBeached = E == EMCBiome::Beach || E == EMCBiome::SnowyBeach;
		if (!BD.bOcean && !bBeached) return St;
		const int32 Z = bBeached ? GroundZ : FMath::Min(GroundZ + 1, MC::SeaLevel - 4);
		if (!bBeached && GroundZ > MC::SeaLevel - 6) return St;
		St->Origin = FMCBlockPos(CX, CY, Z);
		const int32 Rot = R.NextInt(4);
		const FIntVector O(CX, CY, Z);
		FIntVector BMin, BMax; RotBounds(O, Rot, -3, -9, 0, 3, 9, 0, BMin, BMax);
		AddPiece(*St, FIntVector(BMin.X - 1, BMin.Y - 1, Z - 2), FIntVector(BMax.X + 1, BMax.Y + 1, Z + 12), NextSeed(), [O, Rot](FMCGenWriter& W, FMCRandom& RR)
		{
			FMCPieceBuilder B(W, RR, O, Rot);
			const FMCState Pl = RR.NextBool() ? S(TEXT("spruce_planks")) : S(TEXT("oak_planks"));
			const FMCState Log = S(TEXT("spruce_log"));
			const int32 Broken = RR.Range(-2, 6);
			for (int32 y = -9; y <= 9; ++y)
			{
				if (y > Broken && y < Broken + 3) continue; // broken middle section
				const float Half = y < -6 ? 1.f + (y + 9) * 0.5f : (y > 6 ? 1.f + (9 - y) * 0.5f : 3.f);
				const int32 Hw = (int32)Half;
				B.Fill(-Hw + 1, y, 0, Hw - 1, y, 0, Pl);
				for (int32 z = 1; z <= 3; ++z) { B.Set(-Hw, y, z, Pl); B.Set(Hw, y, z, Pl); }
				B.Fill(-Hw + 1, y, 1, Hw - 1, y, 3, S(TEXT("water")) == 0 ? 0 : 0);
				if (y == 0) for (int32 z = 1; z <= 9; ++z) B.Set(0, y, z, Log);
			}
			B.Fill(-2, -4, 4, 2, 4, 4, Pl);
			B.Fill(-1, -3, 4, 1, 3, 4, 0);
			B.Chest(0, -6, 1, EMCFace::South, TEXT("shipwreck_map"));
			B.Chest(0, 6, 1, EMCFace::North, TEXT("shipwreck_treasure"));
			B.Chest(1, 3, 1, EMCFace::West, TEXT("shipwreck_supply"));
			B.Mob(TEXT("drowned"), 0, 0, 2);
		});
		St->bValid = true;
		return St;
	}

	// ================================================================ BURIED TREASURE
	if (Type == TEXT("buried_treasure"))
	{
		if (E != EMCBiome::Beach && E != EMCBiome::SnowyBeach) return St;
		const int32 Z = GroundZ - 3;
		St->Origin = FMCBlockPos(CX, CY, Z);
		const FIntVector O(CX, CY, Z);
		AddPiece(*St, FIntVector(CX - 1, CY - 1, Z - 1), FIntVector(CX + 1, CY + 1, Z + 1), NextSeed(), [O](FMCGenWriter& W, FMCRandom& RR)
		{
			W.Set(O.X, O.Y, O.Z - 1, S(TEXT("sandstone")));
			W.AddChest(O.X, O.Y, O.Z, EMCFace::North, TEXT("buried_treasure"), MCHash::Hash3(0xB7, O.X, O.Y, O.Z));
		});
		St->bValid = true;
		return St;
	}

	// ================================================================ OCEAN RUIN
	if (Type == TEXT("ocean_ruin"))
	{
		if (!BD.bOcean || GroundZ > MC::SeaLevel - 5) return St;
		const bool bWarm = E == EMCBiome::WarmOcean || E == EMCBiome::LukewarmOcean || E == EMCBiome::DeepLukewarmOcean;
		const int32 Z = GroundZ;
		St->Origin = FMCBlockPos(CX, CY, Z + 1);
		const int32 Count = R.Range(1, 4);
		for (int32 i = 0; i < Count; ++i)
		{
			const int32 X = CX + R.Range(-12, 12), Y = CY + R.Range(-12, 12);
			const int32 HZ = ProtoSurface(X, Y);
			const FIntVector O(X, Y, HZ);
			const bool bBig = i == 0 && R.Chance(0.4);
			const int32 Size = bBig ? 5 : 3;
			AddPiece(*St, FIntVector(X - Size - 1, Y - Size - 1, HZ - 1), FIntVector(X + Size + 1, Y + Size + 1, HZ + 7), NextSeed(), [O, Size, bWarm](FMCGenWriter& W, FMCRandom& RR)
			{
				FMCPieceBuilder B(W, RR, O, 0);
				const FMCState A = bWarm ? S(TEXT("sandstone")) : S(TEXT("stone_bricks"));
				const FMCState Bv = bWarm ? S(TEXT("cut_sandstone")) : S(TEXT("mossy_stone_bricks"));
				const FMCState Cv = bWarm ? S(TEXT("chiseled_sandstone")) : S(TEXT("cracked_stone_bricks"));
				for (int32 x = -Size; x <= Size; ++x) for (int32 y = -Size; y <= Size; ++y)
				{
					B.SetVariant(x, y, 0, A, Bv, 0.3f, Cv, 0.2f);
					const bool bWall = FMath::Abs(x) == Size || FMath::Abs(y) == Size;
					if (bWall) for (int32 z = 1; z <= 3; ++z) if (RR.Chance(0.75 - z * 0.15)) B.SetVariant(x, y, z, A, Bv, 0.3f, Cv, 0.2f);
				}
				B.Chest(0, 0, 1, EMCFace::North, Size > 3 ? TEXT("underwater_ruin_big") : TEXT("underwater_ruin_small"));
				B.Mob(TEXT("drowned"), 1, 1, 1);
			});
		}
		St->bValid = true;
		return St;
	}

	// ================================================================ OCEAN MONUMENT
	if (Type == TEXT("ocean_monument"))
	{
		if (!(E == EMCBiome::DeepOcean || E == EMCBiome::DeepColdOcean || E == EMCBiome::DeepLukewarmOcean || E == EMCBiome::DeepFrozenOcean)) return St;
		const int32 Z = MC::SeaLevel - 24;
		St->Origin = FMCBlockPos(CX, CY, Z);
		// split into four quadrant pieces so each chunk only builds what it needs
		for (int32 q = 0; q < 4; ++q)
		{
			const int32 QX = (q & 1) ? 0 : -29, QY = (q & 2) ? 0 : -29;
			const FIntVector O(CX, CY, Z);
			AddPiece(*St, FIntVector(CX + QX, CY + QY, Z - 1), FIntVector(CX + QX + 29, CY + QY + 29, Z + 23), NextSeed(), [O, QX, QY](FMCGenWriter& W, FMCRandom& RR)
			{
				FMCPieceBuilder B(W, RR, O, 0);
				const FMCState PB = S(TEXT("prismarine_bricks")), Pr = S(TEXT("prismarine")), DP = S(TEXT("dark_prismarine")), SL = S(TEXT("sea_lantern")), Water = S(TEXT("water"));
				for (int32 x = QX; x <= QX + 29; ++x)
					for (int32 y = QY; y <= QY + 29; ++y)
					{
						const int32 AX = FMath::Abs(x), AY = FMath::Abs(y);
						if (AX > 28 || AY > 28) continue;
						// foundation columns to the sea floor
						if ((AX % 7 == 0) && (AY % 7 == 0)) for (int32 z = -1; z > -30; --z) { const FIntVector Pp = B.ToWorld(x, y, z); const FMCState Cur = W.Get(Pp.X, Pp.Y, Pp.Z); if (Cur != 0 && Cur != Water) break; B.Set(x, y, z, PB); }
						const int32 M = FMath::Max(AX, AY);
						int32 Top;
						if (M > 22) Top = 7; else if (M > 14) Top = 12; else Top = 20 - (M < 6 ? 0 : 2);
						for (int32 z = 0; z <= Top; ++z)
						{
							const bool bShell = z == 0 || z == Top || M == 28 || M == 22 || M == 14 || M == 6;
							const bool bOpen = (AX <= 2 || AY <= 2) && z >= 1 && z <= 4 && M > 5;
							if (bShell && !bOpen)
							{
								FMCState Bl = (z == Top && (AX + AY) % 6 == 0) ? SL : ((z == Top) ? Pr : ((M == 6 && z > 8) ? DP : PB));
								B.Set(x, y, z, Bl);
							}
							else B.Set(x, y, z, Water);
						}
					}
				if (QX == 0 && QY == 0)
				{
					B.Fill(-1, -1, 9, 1, 1, 11, SL);
					B.Fill(-2, -2, 12, 2, 2, 12, DP);
					B.Set(-1, -1, 13, S(TEXT("gold_block"))); B.Set(1, -1, 13, S(TEXT("gold_block")));
					B.Set(-1, 1, 13, S(TEXT("gold_block"))); B.Set(1, 1, 13, S(TEXT("gold_block")));
					B.Fill(-1, -1, 14, 1, 1, 14, S(TEXT("gold_block")));
					B.Mob(TEXT("elder_guardian"), 3, 3, 14);
				}
				B.Mob(TEXT("guardian"), (float)QX + 14, (float)QY + 14, 3);
				B.Mob(TEXT("guardian"), (float)QX + 20, (float)QY + 8, 9);
			});
		}
		St->bValid = true;
		return St;
	}

	// ================================================================ WOODLAND MANSION
	if (Type == TEXT("woodland_mansion"))
	{
		if (E != EMCBiome::DarkForest || !IsFlatAround(CX, CY, 16, 8.f)) return St;
		const int32 Z = GroundZ;
		St->Origin = FMCBlockPos(CX, CY, Z + 1);
		for (int32 q = 0; q < 4; ++q)
		{
			const int32 QX = (q & 1) ? 0 : -16, QY = (q & 2) ? 0 : -12;
			const FIntVector O(CX, CY, Z);
			AddPiece(*St, FIntVector(CX + QX - 1, CY + QY - 1, Z - 12), FIntVector(CX + QX + 17, CY + QY + 13, Z + 22), NextSeed(), [O, QX, QY](FMCGenWriter& W, FMCRandom& RR)
			{
				FMCPieceBuilder B(W, RR, O, 0);
				const FMCState Log = S(TEXT("dark_oak_log")), Pl = S(TEXT("dark_oak_planks")), Cob = S(TEXT("cobblestone")), Birch = S(TEXT("birch_planks")), Glass = S(TEXT("glass_pane"));
				const FMCState Roof = S(TEXT("dark_oak_stairs"));
				for (int32 x = QX; x <= QX + 16; ++x)
					for (int32 y = QY; y <= QY + 12; ++y)
					{
						const int32 AX = FMath::Abs(x), AY = FMath::Abs(y);
						if (AX > 15 || AY > 11) continue;
						const bool bOuter = AX == 15 || AY == 11;
						const bool bInnerWall = (AX % 7 == 0 && AX > 0) || (AY % 6 == 0 && AY > 0) || x == 0 || y == 0;
						B.Foundation(x, y, x, y, 0, Cob, 14);
						for (int32 z = 0; z <= 17; ++z)
						{
							const bool bFloor = z == 0 || z == 6 || z == 12;
							FMCState Bl = 0;
							if (bFloor) Bl = z == 0 ? Cob : Birch;
							else if (z == 17) Bl = Pl;
							else if (bOuter) Bl = (AX == 15 && AY == 11) ? Log : (((x + y) % 4 == 0) && (z % 6 == 3 || z % 6 == 2) ? Glass : Pl);
							else if (bInnerWall && !(z % 6 == 1 || z % 6 == 2) ) Bl = Pl;
							else if (bInnerWall && ((AX + AY) % 5 != 0)) Bl = Pl;
							B.Set(x, y, z, Bl);
						}
						// roof
						const int32 RoofH = FMath::Min(15 - AX, 11 - AY);
						if (RoofH >= 0) for (int32 z = 18; z <= 18 + FMath::Min(RoofH, 4); ++z) B.Set(x, y, z, z == 18 + FMath::Min(RoofH, 4) ? Pl : 0);
					}
				(void)Roof;
				B.Door(0, -11, 1, TEXT("dark_oak_door"), EMCFace::North);
				B.Set(1, -11, 1, 0); B.Set(1, -11, 2, 0);
				B.Chest(QX + 3, QY + 3, 1, EMCFace::South, TEXT("woodland_mansion"));
				B.Chest(QX + 10, QY + 8, 7, EMCFace::North, TEXT("woodland_mansion"));
				B.Mob(TEXT("vindicator"), QX + 5, QY + 5, 1);
				B.Mob(TEXT("vindicator"), QX + 10, QY + 4, 7);
				if (QX == 0 && QY == 0) B.Mob(TEXT("evoker"), 4, 4, 13);
				B.SetRaw(QX + 8, QY + 6, 5, WithMeta(TEXT("lantern"), 1));
				B.SetRaw(QX + 8, QY + 6, 11, WithMeta(TEXT("lantern"), 1));
			});
		}
		St->bValid = true;
		return St;
	}

	// ================================================================ ANCIENT CITY
	if (Type == TEXT("ancient_city"))
	{
		if (Cl.E > -0.2f || !R.Chance(0.6)) return St;
		const int32 Z = -51;
		St->Origin = FMCBlockPos(CX, CY, Z);
		for (int32 q = 0; q < 4; ++q)
		{
			const int32 QX = (q & 1) ? 0 : -32, QY = (q & 2) ? 0 : -24;
			const FIntVector O(CX, CY, Z);
			AddPiece(*St, FIntVector(CX + QX, CY + QY, Z - 3), FIntVector(CX + QX + 32, CY + QY + 24, Z + 22), NextSeed(), [O, QX, QY](FMCGenWriter& W, FMCRandom& RR)
			{
				FMCPieceBuilder B(W, RR, O, 0);
				const FMCState DB = S(TEXT("deepslate_bricks")), DT = S(TEXT("deepslate_tiles")), CD = S(TEXT("cobbled_deepslate")), PD = S(TEXT("polished_deepslate"));
				const FMCState Sculk = S(TEXT("sculk")), RD = S(TEXT("reinforced_deepslate")), Wool = S(TEXT("gray_wool")), SoulL = WithMeta(TEXT("soul_lantern"), 0);
				for (int32 x = QX; x <= QX + 32; ++x)
					for (int32 y = QY; y <= QY + 24; ++y)
					{
						const int32 AX = FMath::Abs(x), AY = FMath::Abs(y);
						if (AX > 31 || AY > 23) continue;
						// clear the cavern and lay the city floor
						for (int32 z = 0; z <= 18; ++z) B.Set(x, y, z, 0);
						B.Set(x, y, -1, RR.Chance(0.35) ? Sculk : (RR.Chance(0.5) ? DT : DB));
						B.Set(x, y, -2, CD);
						// streets vs buildings
						const bool bBuilding = (AX % 12 >= 3 && AX % 12 <= 9) && (AY % 10 >= 3 && AY % 10 <= 7) && AX > 10;
						if (bBuilding)
						{
							const int32 H = 4 + ((AX / 12 + AY / 10) % 3) * 2;
							const bool bWall = AX % 12 == 3 || AX % 12 == 9 || AY % 10 == 3 || AY % 10 == 7;
							for (int32 z = 0; z <= H; ++z) if (bWall || z == H) B.Set(x, y, z, z == H ? PD : (RR.Chance(0.15) ? S(TEXT("cracked_deepslate_bricks")) : DB));
						}
					}
				// central "portal" frame
				if (QX == 0 && QY == 0)
				{
					for (int32 x = -7; x <= 7; ++x) for (int32 z = 0; z <= 14; ++z)
					{
						const bool bFrame = FMath::Abs(x) >= 5 || z >= 12 || (z <= 1);
						if (bFrame) { B.Set(x, 0, z, (FMath::Abs(x) == 5 || z == 12) ? RD : DB); B.Set(x, 1, z, DB); }
					}
					B.Set(0, 3, 0, S(TEXT("sculk_shrieker"))); B.Set(3, 5, 0, S(TEXT("sculk_shrieker")));
					B.Chest(2, -3, 0, EMCFace::North, TEXT("ancient_city"));
				}
				B.Chest(QX + 6, QY + 5, 0, EMCFace::South, TEXT("ancient_city"));
				B.Chest(QX + 20, QY + 15, 0, EMCFace::North, TEXT("ancient_city_ice_box"));
				for (int32 i = 0; i < 6; ++i) B.SetRaw(QX + 4 + i * 5, QY + 12, 0, SoulL);
				B.Set(QX + 15, QY + 8, 0, S(TEXT("sculk_sensor")));
				B.Set(QX + 25, QY + 18, 0, S(TEXT("sculk_shrieker")));
				B.Set(QX + 9, QY + 18, 0, Wool);
				B.Set(QX + 16, QY + 20, 0, S(TEXT("sculk_catalyst")));
			});
		}
		St->bValid = true;
		return St;
	}

	// ================================================================ TRAIL RUINS
	if (Type == TEXT("trail_ruins"))
	{
		if (!(E == EMCBiome::Taiga || E == EMCBiome::SnowyTaiga || E == EMCBiome::OldGrowthBirchForest || E == EMCBiome::OldGrowthPineTaiga || E == EMCBiome::OldGrowthSpruceTaiga || E == EMCBiome::Jungle)) return St;
		const int32 Z = GroundZ - 4;
		St->Origin = FMCBlockPos(CX, CY, Z);
		const FIntVector O(CX, CY, Z);
		AddPiece(*St, FIntVector(CX - 10, CY - 10, Z - 2), FIntVector(CX + 10, CY + 10, Z + 7), NextSeed(), [O](FMCGenWriter& W, FMCRandom& RR)
		{
			FMCPieceBuilder B(W, RR, O, 0);
			const TCHAR* Terr[] = { TEXT("brown_terracotta"), TEXT("yellow_terracotta"), TEXT("red_terracotta"), TEXT("light_blue_terracotta"), TEXT("mud_bricks"), TEXT("terracotta") };
			for (int32 x = -9; x <= 9; ++x) for (int32 y = -9; y <= 9; ++y)
			{
				const bool bWall = (FMath::Abs(x) % 6 == 0) || (FMath::Abs(y) % 6 == 0);
				B.Set(x, y, -1, S(Terr[RR.NextInt(6)]));
				if (bWall) for (int32 z = 0; z <= RR.Range(0, 3); ++z) B.Set(x, y, z, S(Terr[RR.NextInt(6)]));
				else if (RR.Chance(0.12)) B.Set(x, y, 0, S(TEXT("suspicious_gravel")));
				else B.Set(x, y, 0, S(TEXT("gravel")));
			}
			B.Chest(0, 0, 0, EMCFace::North, TEXT("trail_ruins"));
		});
		St->bValid = true;
		return St;
	}

	// ================================================================ TRIAL CHAMBERS
	if (Type == TEXT("trial_chambers"))
	{
		const int32 Z = R.Range(-40, -20);
		St->Origin = FMCBlockPos(CX, CY, Z);
		for (int32 i = 0; i < 5; ++i)
		{
			const int32 X = CX + (i == 0 ? 0 : (i == 1 ? 18 : (i == 2 ? -18 : 0))), Y = CY + (i == 3 ? 18 : (i == 4 ? -18 : 0));
			const FIntVector O(X, Y, Z);
			const bool bCentral = i == 0;
			AddPiece(*St, FIntVector(X - 10, Y - 10, Z - 1), FIntVector(X + 10, Y + 10, Z + 9), NextSeed(), [O, bCentral, CX, CY](FMCGenWriter& W, FMCRandom& RR)
			{
				FMCPieceBuilder B(W, RR, O, 0);
				const FMCState TB = S(TEXT("tuff_bricks")), PT = S(TEXT("polished_tuff")), Cu = S(TEXT("waxed_cut_copper")), Grate = S(TEXT("copper_grate"));
				const int32 Rr = bCentral ? 8 : 6;
				for (int32 x = -Rr; x <= Rr; ++x) for (int32 y = -Rr; y <= Rr; ++y) for (int32 z = -1; z <= 7; ++z)
				{
					const bool bShell = FMath::Abs(x) == Rr || FMath::Abs(y) == Rr || z == -1 || z == 7;
					if (bShell) B.Set(x, y, z, z == -1 ? ((FMath::Abs(x) + FMath::Abs(y)) % 4 == 0 ? Cu : PT) : (z == 3 && (x + y) % 3 == 0 ? Grate : TB));
					else B.Set(x, y, z, 0);
				}
				B.SetRaw(0, 0, 6, S(TEXT("copper_bulb")));
				B.Spawner(-Rr + 2, 0, 0, RR.Chance(0.5) ? FName(TEXT("breeze")) : FName(TEXT("zombie")));
				B.Spawner(Rr - 2, 0, 0, RR.Chance(0.5) ? FName(TEXT("skeleton")) : FName(TEXT("stray")));
				if (bCentral) { B.Chest(0, Rr - 2, 0, EMCFace::North, TEXT("trial_chambers_reward"), TEXT("barrel")); B.Mob(TEXT("breeze"), 0, 0, 1); }
				// corridor towards the centre
				const int32 DXc = FMath::Sign(CX - O.X), DYc = FMath::Sign(CY - O.Y);
				if (DXc || DYc) for (int32 k = Rr; k <= 18 - Rr + 2; ++k) for (int32 o = -1; o <= 1; ++o) for (int32 z = 0; z <= 2; ++z) B.Set(DXc ? DXc * k : o, DYc ? DYc * k : o, z, 0);
			});
		}
		St->bValid = true;
		return St;
	}
	return St;
}
