#include "Gen/MCStructureBuilder.h"
#include "World/MCBlockEntity.h"

FIntVector FMCPieceBuilder::ToWorld(int32 X, int32 Y, int32 Z) const
{
	int32 RX = X, RY = Y;
	for (int32 i = 0; i < Rot; ++i)
	{
		// clockwise from above: north(-y) -> east(+x)  => (x, y) -> (-y, x)
		const int32 T = RX; RX = -RY; RY = T;
	}
	return FIntVector(Origin.X + RX, Origin.Y + RY, Origin.Z + Z);
}

FMCState FMCPieceBuilder::RotState(FMCState S) const
{
	if (S == 0 || Rot == 0) return S;
	const FMCBlock& B = FMCBlocks::GetByState(S);
	uint8 M = FMCBlocks::MetaOf(S);
	// logs / pillars: swap X and Y axes on odd rotations
	if (B.Orient == EMCCubeOrient::Axis || B.Model == EMCModel::Chain)
	{
		if (Rot & 1)
		{
			const uint8 A = M & 3;
			if (A == 1) M = (M & ~3) | 2; else if (A == 2) M = (M & ~3) | 1;
		}
		return B.State(M);
	}
	auto Rot4 = [&](uint8 Meta) -> uint8
	{
		const EMCFace F = MCMeta::Facing4(Meta);
		return (uint8)((Meta & ~3) | MCMeta::FromFacing4(MC::RotateY(F, Rot)));
	};
	switch (B.Model)
	{
	case EMCModel::Stairs: case EMCModel::FenceGate: case EMCModel::Door: case EMCModel::Trapdoor: case EMCModel::Ladder: case EMCModel::Bed:
	case EMCModel::Chest: case EMCModel::Repeater: case EMCModel::Comparator: case EMCModel::Anvil: case EMCModel::EndPortalFrame: case EMCModel::Campfire:
	case EMCModel::Lectern: case EMCModel::Grindstone: case EMCModel::Stonecutter: case EMCModel::Bell: case EMCModel::Shelf:
		return B.State(Rot4(M));
	case EMCModel::Torch:
	{
		const int32 A = M & 7;
		if (A >= 1 && A <= 4)
		{
			const EMCFace F = MC::RotateY((EMCFace)(1 + A), Rot);
			return B.State((uint8)((M & ~7) | ((uint8)F - 1)));
		}
		return S;
	}
	case EMCModel::Button: case EMCModel::Lever: case EMCModel::EndRod: case EMCModel::Piston: case EMCModel::Hopper:
	{
		const EMCFace F = MCMeta::Facing6(M);
		if (MC::IsHorizontal(F)) return B.State((uint8)((M & ~7) | (uint8)MC::RotateY(F, Rot)));
		return S;
	}
	case EMCModel::Rail:
	{
		const int32 Shape = M & (B.Name == TEXT("rail") ? 15 : 7);
		if ((Rot & 1) && Shape <= 1) return B.State((uint8)((M & ~1) | (Shape ^ 1)));
		return S;
	}
	default: break;
	}
	if (B.Orient == EMCCubeOrient::Facing4 || B.Orient == EMCCubeOrient::Facing4Lit) return B.State(Rot4(M));
	if (B.Orient == EMCCubeOrient::Facing6 || B.Orient == EMCCubeOrient::Facing6Lit)
	{
		const EMCFace F = MCMeta::Facing6(M);
		if (MC::IsHorizontal(F)) return B.State((uint8)((M & ~7) | (uint8)MC::RotateY(F, Rot)));
	}
	return S;
}

void FMCPieceBuilder::Set(int32 X, int32 Y, int32 Z, FMCState S)
{
	const FIntVector P = ToWorld(X, Y, Z);
	W.Set(P.X, P.Y, P.Z, RotState(S));
}

void FMCPieceBuilder::SetRaw(int32 X, int32 Y, int32 Z, FMCState S)
{
	const FIntVector P = ToWorld(X, Y, Z);
	W.Set(P.X, P.Y, P.Z, S);
}

FMCState FMCPieceBuilder::Get(int32 X, int32 Y, int32 Z) const
{
	const FIntVector P = ToWorld(X, Y, Z);
	return W.Get(P.X, P.Y, P.Z);
}

void FMCPieceBuilder::Fill(int32 X0, int32 Y0, int32 Z0, int32 X1, int32 Y1, int32 Z1, FMCState S)
{
	const FMCState RS = RotState(S);
	for (int32 Z = FMath::Min(Z0, Z1); Z <= FMath::Max(Z0, Z1); ++Z)
		for (int32 Y = FMath::Min(Y0, Y1); Y <= FMath::Max(Y0, Y1); ++Y)
			for (int32 X = FMath::Min(X0, X1); X <= FMath::Max(X0, X1); ++X)
			{
				const FIntVector P = ToWorld(X, Y, Z);
				W.Set(P.X, P.Y, P.Z, RS);
			}
}

void FMCPieceBuilder::Room(int32 X0, int32 Y0, int32 Z0, int32 X1, int32 Y1, int32 Z1, FMCState Wall, FMCState Floor, FMCState Ceil)
{
	for (int32 Z = Z0; Z <= Z1; ++Z)
		for (int32 Y = Y0; Y <= Y1; ++Y)
			for (int32 X = X0; X <= X1; ++X)
			{
				FMCState S = 0;
				if (Z == Z0) S = Floor ? Floor : Wall;
				else if (Z == Z1) S = Ceil ? Ceil : Wall;
				else if (X == X0 || X == X1 || Y == Y0 || Y == Y1) S = Wall;
				Set(X, Y, Z, S);
			}
}

void FMCPieceBuilder::Foundation(int32 X0, int32 Y0, int32 X1, int32 Y1, int32 Z, FMCState S, int32 MaxDepth)
{
	for (int32 Y = Y0; Y <= Y1; ++Y)
		for (int32 X = X0; X <= X1; ++X)
		{
			const FIntVector P = ToWorld(X, Y, Z);
			if (!W.InsideXY(P.X, P.Y)) continue;
			for (int32 d = 0; d < MaxDepth; ++d)
			{
				const FMCState Cur = W.Get(P.X, P.Y, P.Z - d);
				if (Cur != 0 && FMCBlocks::IsOpaque(Cur) && !(FMCBlocks::Info(Cur).Flags & MCB_Leaves) && d > 0) break;
				W.Set(P.X, P.Y, P.Z - d, S);
			}
		}
}

void FMCPieceBuilder::SetVariant(int32 X, int32 Y, int32 Z, FMCState A, FMCState B, float ChanceB, FMCState C, float ChanceC)
{
	const float Roll = R.NextFloat();
	FMCState S = A;
	if (Roll < ChanceB) S = B;
	else if (C && Roll < ChanceB + ChanceC) S = C;
	Set(X, Y, Z, S);
}

void FMCPieceBuilder::Chest(int32 X, int32 Y, int32 Z, EMCFace Facing, FName Loot, FName Block)
{
	const FIntVector P = ToWorld(X, Y, Z);
	W.AddChest(P.X, P.Y, P.Z, RotFace(Facing), Loot, MCHash::Hash3(0x10075, P.X, P.Y, P.Z), Block);
}

void FMCPieceBuilder::Spawner(int32 X, int32 Y, int32 Z, FName Mob)
{
	const FIntVector P = ToWorld(X, Y, Z);
	W.AddSpawner(P.X, P.Y, P.Z, Mob);
}

void FMCPieceBuilder::Mob(FName MobId, float X, float Y, float Z, int32 Variant, FName Extra)
{
	const FIntVector P = ToWorld(FMath::FloorToInt(X), FMath::FloorToInt(Y), FMath::FloorToInt(Z));
	W.AddSpawn(MobId, P.X + 0.5, P.Y + 0.5, P.Z + 0.05, Variant, Extra);
}

void FMCPieceBuilder::Door(int32 X, int32 Y, int32 Z, const TCHAR* DoorBlock, EMCFace Facing, bool bRightHinge)
{
	const FMCBlock* B = FMCBlocks::Find(FName(DoorBlock));
	if (!B) return;
	const uint8 F = MCMeta::FromFacing4(Facing);
	const uint8 Hinge = bRightHinge ? (1 << 4) : 0;
	Set(X, Y, Z, B->State(F | Hinge));
	Set(X, Y, Z + 1, B->State(F | Hinge | (1 << 2)));
}

void FMCPieceBuilder::Bed(int32 X, int32 Y, int32 Z, const TCHAR* BedBlock, EMCFace Facing)
{
	const FMCBlock* B = FMCBlocks::Find(FName(BedBlock));
	if (!B) return;
	// foot at (X,Y), head one block towards Facing
	const FIntVector& D = MC::FaceDir[(int32)Facing];
	Set(X, Y, Z, B->State(MCMeta::FromFacing4(Facing)));
	Set(X + D.X, Y + D.Y, Z, B->State(MCMeta::FromFacing4(Facing) | (1 << 2)));
}

void FMCPieceBuilder::Stair(int32 X, int32 Y, int32 Z, FMCState Stairs, EMCFace Facing, bool bTop)
{
	const FMCBlock& B = FMCBlocks::GetByState(Stairs);
	Set(X, Y, Z, B.State(MCMeta::FromFacing4(Facing) | (bTop ? 4 : 0)));
}

void FMCPieceBuilder::Torch(int32 X, int32 Y, int32 Z, EMCFace WallFacing)
{
	const FMCBlock* B = FMCBlocks::Find(TEXT("torch"));
	if (!B) return;
	if (WallFacing == EMCFace::Up) Set(X, Y, Z, B->State(0));
	else Set(X, Y, Z, B->State((uint8)WallFacing - 1));
}

void FMCPieceBuilder::GableRoof(int32 X0, int32 Y0, int32 X1, int32 Y1, int32 Z, FMCState Stairs, FMCState FillS)
{
	const int32 W2 = (X1 - X0);
	for (int32 i = 0; i <= W2 / 2; ++i)
	{
		const int32 L = X0 + i, Rr = X1 - i, H = Z + i;
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			if (L < Rr)
			{
				Stair(L, Y, H, Stairs, EMCFace::East);
				Stair(Rr, Y, H, Stairs, EMCFace::West);
				for (int32 X = L + 1; X < Rr; ++X) if (Y == Y0 || Y == Y1) Set(X, Y, H, FillS);
			}
			else if (L == Rr)
			{
				Set(L, Y, H, FMCBlocks::GetByState(Stairs).Name.ToString().Contains(TEXT("stairs")) ? FMCBlocks::FindState(FName(*FMCBlocks::GetByState(Stairs).Name.ToString().Replace(TEXT("_stairs"), TEXT("_slab")))) : FillS);
			}
		}
	}
}
