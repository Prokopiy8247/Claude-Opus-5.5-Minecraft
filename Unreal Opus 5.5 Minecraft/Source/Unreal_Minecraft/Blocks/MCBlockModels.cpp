// Box/quad models for non-cube blocks. Coordinates in block units (x east, y south, z up).
#include "Blocks/MCBlocks.h"
#include "Blocks/MCTextures.h"

namespace
{
	constexpr float PX = 1.f / 16.f;
	constexpr int32 FD = (int32)EMCFace::Down, FU = (int32)EMCFace::Up, FN = (int32)EMCFace::North, FS = (int32)EMCFace::South, FW = (int32)EMCFace::West, FE = (int32)EMCFace::East;

	/** Box from pixel coordinates (0..16), z up. */
	FMCModelBox P(float X0, float Y0, float Z0, float X1, float Y1, float Z1, int16 Tex)
	{
		FMCModelBox B;
		B.Min = FVector3f(X0 * PX, Y0 * PX, Z0 * PX);
		B.Max = FVector3f(X1 * PX, Y1 * PX, Z1 * PX);
		for (int32 i = 0; i < 6; ++i) B.Tex[i] = Tex;
		B.AutoCull();
		return B;
	}
	FMCModelBox P6(float X0, float Y0, float Z0, float X1, float Y1, float Z1, const int16 Tex[6])
	{
		FMCModelBox B = P(X0, Y0, Z0, X1, Y1, Z1, 0);
		for (int32 i = 0; i < 6; ++i) B.Tex[i] = Tex[i];
		return B;
	}
	FMCBox PB(float X0, float Y0, float Z0, float X1, float Y1, float Z1)
	{
		return FMCBox(X0 * PX, Y0 * PX, Z0 * PX, X1 * PX, Y1 * PX, Z1 * PX);
	}
	/** Flat quad lying at height Z (pixels) spanning the whole block, facing up. */
	FMCModelQuad FlatQuad(float Z, int16 Tex, float Inset = 0.f)
	{
		const float Z0 = Z * PX, A = Inset * PX, B = 1.f - Inset * PX;
		FMCModelQuad Q(FVector3f(A, B, Z0), FVector3f(B, B, Z0), FVector3f(B, A, Z0), FVector3f(A, A, Z0), Tex);
		Q.SetUV(A, A, B, B);
		Q.bDoubleSided = true;
		return Q;
	}
	/** Vertical quad on a face of the cell (offset inward by Inset pixels). */
	FMCModelQuad FaceQuad(EMCFace F, int16 Tex, float Inset = 0.8f)
	{
		const float I = Inset * PX;
		FMCModelQuad Q;
		switch (F)
		{
		case EMCFace::North: Q = FMCModelQuad(FVector3f(1, I, 0), FVector3f(0, I, 0), FVector3f(0, I, 1), FVector3f(1, I, 1), Tex); break;
		case EMCFace::South: Q = FMCModelQuad(FVector3f(0, 1 - I, 0), FVector3f(1, 1 - I, 0), FVector3f(1, 1 - I, 1), FVector3f(0, 1 - I, 1), Tex); break;
		case EMCFace::West: Q = FMCModelQuad(FVector3f(I, 0, 0), FVector3f(I, 1, 0), FVector3f(I, 1, 1), FVector3f(I, 0, 1), Tex); break;
		case EMCFace::East: Q = FMCModelQuad(FVector3f(1 - I, 1, 0), FVector3f(1 - I, 0, 0), FVector3f(1 - I, 0, 1), FVector3f(1 - I, 1, 1), Tex); break;
		case EMCFace::Up: Q = FMCModelQuad(FVector3f(0, 1, 1 - I), FVector3f(1, 1, 1 - I), FVector3f(1, 0, 1 - I), FVector3f(0, 0, 1 - I), Tex); break;
		default: Q = FMCModelQuad(FVector3f(0, 0, I), FVector3f(1, 0, I), FVector3f(1, 1, I), FVector3f(0, 1, I), Tex); break;
		}
		Q.bDoubleSided = true;
		Q.LightFace = -1;
		return Q;
	}
	/** Two crossing diagonal quads (plants, fire). */
	void AddCross(FMCBlockModel& M, int16 Tex, float Size = 1.f, float Height = 1.f, float ZOffset = 0.f, bool bTint = false)
	{
		const float A = 0.5f - 0.5f * Size * 0.7071f * 1.414f / 1.414f, B = 1.f - A;
		const float Z0 = ZOffset, Z1 = ZOffset + Height;
		FMCModelQuad Q1(FVector3f(A, A, Z0), FVector3f(B, B, Z0), FVector3f(B, B, Z1), FVector3f(A, A, Z1), Tex);
		FMCModelQuad Q2(FVector3f(A, B, Z0), FVector3f(B, A, Z0), FVector3f(B, A, Z1), FVector3f(A, B, Z1), Tex);
		Q1.bTint = Q2.bTint = bTint;
		M.Quads.Add(Q1); M.Quads.Add(Q2);
	}

	int16 BT(const FMCBlock& B, int32 Face) { return B.Tex[Face]; }

	// ------------------------------------------------------------------------------------------ neighbour helpers
	FMCState NS(const FMCBlockGetter* G, const FMCBlockPos& Pos, EMCFace F)
	{
		return G ? G->GetState(Pos.Offset(F)) : 0;
	}
	bool IsFullSolid(FMCState S)
	{
		const FMCStateInfo& I = FMCBlocks::Info(S);
		return (I.Flags & MCB_Opaque) != 0 && !(I.Flags & MCB_Leaves);
	}
	bool FenceConnects(const FMCBlock& Self, FMCState N, EMCFace Dir)
	{
		const FMCBlock& O = FMCBlocks::GetByState(N);
		if (O.Model == EMCModel::Fence)
		{
			const bool bSelfWood = Self.HasTag(TEXT("wooden_fences")), bOtherWood = O.HasTag(TEXT("wooden_fences"));
			return bSelfWood == bOtherWood;
		}
		if (O.Model == EMCModel::FenceGate)
		{
			const EMCFace GF = MCMeta::Facing4(FMCBlocks::MetaOf(N));
			const bool bGateAxisX = GF == EMCFace::North || GF == EMCFace::South; // gate spans X
			const bool bDirX = Dir == EMCFace::West || Dir == EMCFace::East;
			return bGateAxisX == bDirX;
		}
		return IsFullSolid(N);
	}
	bool PaneConnects(FMCState N)
	{
		const FMCBlock& O = FMCBlocks::GetByState(N);
		if (O.Model == EMCModel::Pane || O.Model == EMCModel::Bars || O.Model == EMCModel::Wall) return true;
		if (O.Has(MCB_CullSame) && O.Shape == EMCShape::Cube) return true;
		return IsFullSolid(N);
	}
	bool WallConnects(FMCState N, EMCFace Dir)
	{
		const FMCBlock& O = FMCBlocks::GetByState(N);
		if (O.Model == EMCModel::Wall || O.Model == EMCModel::Pane || O.Model == EMCModel::Bars) return true;
		if (O.Model == EMCModel::FenceGate)
		{
			const EMCFace GF = MCMeta::Facing4(FMCBlocks::MetaOf(N));
			return (GF == EMCFace::North || GF == EMCFace::South) == (Dir == EMCFace::West || Dir == EMCFace::East);
		}
		return IsFullSolid(N);
	}

	// ------------------------------------------------------------------------------------------ builders
	void BuildSlab(const FMCBlock& B, uint8 Meta, FMCBlockModel& M)
	{
		const int16* T = B.Tex;
		if (Meta == 2) { M.Add(P6(0, 0, 0, 16, 16, 16, T)); return; }
		if (Meta == 1) M.Add(P6(0, 0, 8, 16, 16, 16, T));
		else M.Add(P6(0, 0, 0, 16, 16, 8, T));
	}

	/** Stair shape for the given state (0 straight, 1 inner left, 2 inner right, 3 outer left, 4 outer right). */
	int32 StairShape(uint8 Meta, const FMCBlockGetter* G, const FMCBlockPos& Pos)
	{
		if (!G) return 0;
		const EMCFace F = MCMeta::Facing4(Meta);
		const bool bTop = MCMeta::Bit(Meta, 2);
		auto IsStair = [](FMCState S) { return FMCBlocks::GetByState(S).Model == EMCModel::Stairs; };
		auto Half = [](FMCState S) { return MCMeta::Bit(FMCBlocks::MetaOf(S), 2); };
		auto Face = [](FMCState S) { return MCMeta::Facing4(FMCBlocks::MetaOf(S)); };
		auto CanTake = [&](EMCFace Dir)
		{
			const FMCState N = G->GetState(Pos.Offset(Dir));
			return !IsStair(N) || Face(N) != F || Half(N) != bTop;
		};
		const EMCFace CCW = MC::RotateY(F, -1);
		const FMCState Back = G->GetState(Pos.Offset(F));
		if (IsStair(Back) && Half(Back) == bTop)
		{
			const EMCFace D = Face(Back);
			const bool bPerp = (D == EMCFace::North || D == EMCFace::South) != (F == EMCFace::North || F == EMCFace::South);
			if (bPerp && CanTake(MC::Opposite(D))) return D == CCW ? 3 : 4;
		}
		const FMCState Front = G->GetState(Pos.Offset(MC::Opposite(F)));
		if (IsStair(Front) && Half(Front) == bTop)
		{
			const EMCFace D = Face(Front);
			const bool bPerp = (D == EMCFace::North || D == EMCFace::South) != (F == EMCFace::North || F == EMCFace::South);
			if (bPerp && CanTake(D)) return D == CCW ? 1 : 2;
		}
		return 0;
	}

	void BuildStairs(const FMCBlock& B, uint8 Meta, const FMCBlockGetter* G, const FMCBlockPos& Pos, FMCBlockModel& M)
	{
		const int16* T = B.Tex;
		const bool bTop = MCMeta::Bit(Meta, 2);
		const EMCFace F = MCMeta::Facing4(Meta);
		const int32 Shape = StairShape(Meta, G, Pos);
		const float S0 = bTop ? 8.f : 0.f, S1 = bTop ? 16.f : 8.f;   // slab part
		const float U0 = bTop ? 0.f : 8.f, U1 = bTop ? 8.f : 16.f;   // step part
		FMCBlockModel Local;
		Local.Add(P6(0, 0, S0, 16, 16, S1, T));
		// authored facing North: high half at y 0..8
		switch (Shape)
		{
		case 0: Local.Add(P6(0, 0, U0, 16, 8, U1, T)); break;
		case 1: Local.Add(P6(0, 0, U0, 16, 8, U1, T)); Local.Add(P6(0, 8, U0, 8, 16, U1, T)); break;   // inner left (west)
		case 2: Local.Add(P6(0, 0, U0, 16, 8, U1, T)); Local.Add(P6(8, 8, U0, 16, 16, U1, T)); break;  // inner right (east)
		case 3: Local.Add(P6(0, 0, U0, 8, 8, U1, T)); break;   // outer left
		case 4: Local.Add(P6(8, 0, U0, 16, 8, U1, T)); break;  // outer right
		}
		Local.RotateY(MCMeta::QuarterFromNorth(F));
		for (FMCModelBox& Bx : Local.Boxes) { for (int32 i = 0; i < 6; ++i) Bx.Tex[i] = T[i]; Bx.UVRot[FU] = Bx.UVRot[FD] = 0; }
		M.Boxes.Append(Local.Boxes);
	}

	void BuildFence(const FMCBlock& B, const FMCBlockGetter* G, const FMCBlockPos& Pos, FMCBlockModel& M)
	{
		const int16 T = BT(B, FN);
		M.Add(P(6, 6, 0, 10, 10, 16, T));
		M.Collision.Add(PB(6, 6, 0, 10, 10, 24));
		const EMCFace Dirs[4] = { EMCFace::North, EMCFace::South, EMCFace::West, EMCFace::East };
		for (EMCFace D : Dirs)
		{
			if (!G || !FenceConnects(B, NS(G, Pos, D), D)) continue;
			FMCBlockModel Arm;
			Arm.Add(P(7, 0, 12, 9, 6, 15, T));
			Arm.Add(P(7, 0, 6, 9, 6, 9, T));
			Arm.Collision.Add(PB(6, 0, 0, 10, 6, 24));
			Arm.RotateY(MCMeta::QuarterFromNorth(D));
			M.Boxes.Append(Arm.Boxes);
			M.Collision.Append(Arm.Collision);
		}
		M.Outline.Add(PB(6, 6, 0, 10, 10, 16));
		for (const FMCModelBox& Bx : M.Boxes) M.Outline.Add(Bx.ToBox());
	}

	void BuildFenceGate(const FMCBlock& B, uint8 Meta, FMCBlockModel& M)
	{
		const int16 T = BT(B, FN);
		const EMCFace F = MCMeta::Facing4(Meta);
		const bool bOpen = MCMeta::Bit(Meta, 2);
		FMCBlockModel L; // authored facing North: gate spans X at y 7..9
		L.Add(P(0, 7, 5, 2, 9, 16, T));
		L.Add(P(14, 7, 5, 16, 9, 16, T));
		if (!bOpen)
		{
			L.Add(P(2, 7, 6, 14, 9, 9, T));
			L.Add(P(2, 7, 12, 14, 9, 15, T));
			L.Add(P(6, 7, 9, 10, 9, 12, T));
			L.Collision.Add(PB(0, 6, 0, 16, 10, 24));
		}
		else
		{
			L.Add(P(0, 1, 6, 2, 7, 9, T)); L.Add(P(0, 1, 12, 2, 7, 15, T)); L.Add(P(0, 1, 9, 2, 3, 12, T));
			L.Add(P(14, 1, 6, 16, 7, 9, T)); L.Add(P(14, 1, 12, 16, 7, 15, T)); L.Add(P(14, 1, 9, 16, 3, 12, T));
			L.bExplicitCollision = true;
		}
		L.Outline.Add(PB(0, 6, 0, 16, 10, 16));
		L.RotateY(MCMeta::QuarterFromNorth(F));
		M = L;
	}

	void BuildWall(const FMCBlock& B, const FMCBlockGetter* G, const FMCBlockPos& Pos, FMCBlockModel& M)
	{
		const int16* T = B.Tex;
		bool Conn[4] = { false, false, false, false };
		const EMCFace Dirs[4] = { EMCFace::North, EMCFace::South, EMCFace::West, EMCFace::East };
		for (int32 i = 0; i < 4; ++i) Conn[i] = G && WallConnects(NS(G, Pos, Dirs[i]), Dirs[i]);
		const bool bStraightNS = Conn[0] && Conn[1] && !Conn[2] && !Conn[3];
		const bool bStraightWE = !Conn[0] && !Conn[1] && Conn[2] && Conn[3];
		const FMCState Above = G ? G->GetState(Pos.Up()) : 0;
		const bool bAboveWall = FMCBlocks::GetByState(Above).Model == EMCModel::Wall || FMCBlocks::GetByState(Above).Model == EMCModel::Torch || FMCBlocks::GetByState(Above).Model == EMCModel::Lantern;
		const bool bPost = !(bStraightNS || bStraightWE) || bAboveWall;
		const bool bTall = G && IsFullSolid(Above);
		const float H = bTall ? 16.f : 14.f;
		if (bPost) M.Add(P6(4, 4, 0, 12, 12, 16, T));
		for (int32 i = 0; i < 4; ++i)
		{
			if (!Conn[i]) continue;
			FMCBlockModel Arm;
			Arm.Add(P6(5, 0, 0, 11, bPost ? 4.f : 8.f, H, T));
			Arm.RotateY(MCMeta::QuarterFromNorth(Dirs[i]));
			for (FMCModelBox& Bx : Arm.Boxes) for (int32 f = 0; f < 6; ++f) Bx.Tex[f] = T[f];
			M.Boxes.Append(Arm.Boxes);
		}
		if (!bPost && M.Boxes.Num() == 0) M.Add(P6(4, 4, 0, 12, 12, 16, T));
		for (const FMCModelBox& Bx : M.Boxes)
		{
			FMCBox C = Bx.ToBox(); C.Max.Z = 1.5; M.Collision.Add(C);
			M.Outline.Add(Bx.ToBox());
		}
	}

	void BuildPane(const FMCBlock& B, const FMCBlockGetter* G, const FMCBlockPos& Pos, FMCBlockModel& M, bool bBars)
	{
		const int16 T = BT(B, FN);
		bool Conn[4] = { false, false, false, false };
		const EMCFace Dirs[4] = { EMCFace::North, EMCFace::South, EMCFace::West, EMCFace::East };
		int32 Count = 0;
		for (int32 i = 0; i < 4; ++i) { Conn[i] = G && PaneConnects(NS(G, Pos, Dirs[i])); Count += Conn[i]; }
		if (Count == 0) { Conn[0] = Conn[1] = Conn[2] = Conn[3] = true; }
		const float T0 = 7.f, T1 = 9.f;
		M.Add(P(T0, T0, 0, T1, T1, 16, T));
		for (int32 i = 0; i < 4; ++i)
		{
			if (!Conn[i]) continue;
			FMCBlockModel Arm;
			Arm.Add(P(T0, 0, 0, T1, T0, 16, T));
			Arm.RotateY(MCMeta::QuarterFromNorth(Dirs[i]));
			M.Boxes.Append(Arm.Boxes);
		}
		for (FMCModelBox& Bx : M.Boxes) Bx.CullFaces = 0;
		for (const FMCModelBox& Bx : M.Boxes) M.Collision.Add(Bx.ToBox());
	}

	void BuildDoor(const FMCBlock& B, uint8 Meta, FMCBlockModel& M)
	{
		const EMCFace F = MCMeta::Facing4(Meta);
		const bool bUpper = MCMeta::Bit(Meta, 2), bOpen = MCMeta::Bit(Meta, 3), bRight = MCMeta::Bit(Meta, 4);
		const int16 Tex = bUpper ? B.Tex[FU] : B.Tex[FD];
		FMCBlockModel L;
		// authored facing North (closed panel on the south side of the cell)
		FMCModelBox Panel;
		if (!bOpen) Panel = P(0, 13, 0, 16, 16, 16, Tex);
		else if (bRight) Panel = P(13, 0, 0, 16, 16, 16, Tex);
		else Panel = P(0, 0, 0, 3, 16, 16, Tex);
		Panel.CullFaces = 0;
		L.Add(Panel);
		L.RotateY(MCMeta::QuarterFromNorth(F));
		M = L;
		M.Boxes[0].bDoubleSided = false;
	}

	void BuildTrapdoor(const FMCBlock& B, uint8 Meta, FMCBlockModel& M)
	{
		const int16 T = BT(B, FN);
		const EMCFace F = MCMeta::Facing4(Meta);
		const bool bTop = MCMeta::Bit(Meta, 2), bOpen = MCMeta::Bit(Meta, 3);
		FMCBlockModel L;
		if (!bOpen) L.Add(bTop ? P(0, 0, 13, 16, 16, 16, T) : P(0, 0, 0, 16, 16, 3, T));
		else L.Add(P(0, 13, 0, 16, 16, 16, T));
		L.RotateY(MCMeta::QuarterFromNorth(F));
		for (FMCModelBox& Bx : L.Boxes) for (int32 i = 0; i < 6; ++i) Bx.Tex[i] = T;
		M = L;
	}

	void BuildTorch(const FMCBlock& B, uint8 Meta, FMCBlockModel& M)
	{
		const int16 Tex = (B.Name == TEXT("redstone_torch") && MCMeta::Bit(Meta, 3)) ? FMCTextures::Find(TEXT("redstone_torch_off")) : FMCTextures::Find(B.Name == TEXT("copper_torch") ? FName(TEXT("copper_torch")) : B.Name);
		const int32 Attach = Meta & 7;
		FMCModelBox Stick = P(7, 7, 0, 9, 9, 10, Tex);
		Stick.CullFaces = 0;
		if (Attach == 0)
		{
			M.Add(Stick);
			M.Outline.Add(PB(6, 6, 0, 10, 10, 10));
		}
		else
		{
			// wall torch: raised and moved against the wall (authored for facing North = wall on the south side)
			FMCBlockModel L;
			FMCModelBox W = P(7, 12, 3, 9, 14, 13, Tex);
			W.CullFaces = 0;
			L.Add(W);
			L.Outline.Add(PB(5.5f, 11, 3, 10.5f, 16, 13));
			const EMCFace F = (EMCFace)(1 + Attach); // 1..4 -> N,S,W,E
			L.RotateY(MCMeta::QuarterFromNorth(F));
			M = L;
		}
		M.bExplicitCollision = true;
	}

	void BuildLadder(const FMCBlock& B, uint8 Meta, FMCBlockModel& M)
	{
		const int16 T = FMCTextures::Find(TEXT("ladder"));
		FMCBlockModel L;
		L.Quads.Add(FaceQuad(EMCFace::South, T, 0.6f));
		L.Collision.Add(PB(0, 13, 0, 16, 16, 16));
		L.RotateY(MCMeta::QuarterFromNorth(MCMeta::Facing4(Meta)));
		M = L;
	}

	void BuildRail(const FMCBlock& B, uint8 Meta, FMCBlockModel& M)
	{
		const bool bSimple = B.Name == TEXT("rail");
		const int32 Shape = bSimple ? (Meta & 15) : (Meta & 7);
		const bool bPowered = !bSimple && MCMeta::Bit(Meta, 3);
		int16 Tex = B.Tex[FU];
		if (bPowered && B.TexAlt[0] >= 0) Tex = B.TexAlt[0];
		if (bSimple && Shape >= 6) Tex = FMCTextures::Find(TEXT("rail_corner"));
		const float Z = 1.f * PX;
		auto Flat = [&](int32 Rot)
		{
			FMCModelQuad Q = FlatQuad(1.f, Tex);
			Q.RotateY(Rot);
			M.Quads.Add(Q);
		};
		switch (Shape)
		{
		case 0: Flat(0); break;                 // north-south
		case 1: Flat(1); break;                 // east-west
		case 2: case 3: case 4: case 5:         // ascending east / west / north / south
		{
			FMCModelQuad Q(FVector3f(0, 1, Z), FVector3f(1, 1, Z), FVector3f(1, 0, 1 + Z), FVector3f(0, 0, 1 + Z), Tex); // ascends north
			int32 Rot = 0;
			if (Shape == 2) Rot = 1; else if (Shape == 3) Rot = 3; else if (Shape == 4) Rot = 0; else Rot = 2;
			Q.RotateY(Rot);
			Q.bDoubleSided = true;
			M.Quads.Add(Q);
			break;
		}
		case 6: Flat(0); break;                 // south-east corner (texture authored as SE)
		case 7: Flat(1); break;                 // south-west
		case 8: Flat(2); break;                 // north-west
		case 9: Flat(3); break;                 // north-east
		default: Flat(0); break;
		}
		M.Outline.Add(PB(0, 0, 0, 16, 16, 2));
		M.bExplicitCollision = true;
	}

	void BuildRedstoneWire(const FMCBlock& B, uint8 Meta, const FMCBlockGetter* G, const FMCBlockPos& Pos, FMCBlockModel& M)
	{
		const int32 Power = Meta & 15;
		const float Pw = Power / 15.f;
		const FColor Col(uint8(FMath::Lerp(90.f, 255.f, Pw)), uint8(FMath::Lerp(0.f, 60.f, Pw * Pw)), uint8(FMath::Lerp(0.f, 20.f, Pw)));
		const int16 Dot = FMCTextures::Find(TEXT("redstone_dust_dot"));
		const int16 Line = FMCTextures::Find(TEXT("redstone_dust_line"));
		bool Conn[4] = { false, false, false, false };
		bool Up[4] = { false, false, false, false };
		const EMCFace Dirs[4] = { EMCFace::North, EMCFace::South, EMCFace::West, EMCFace::East };
		if (G)
		{
			const bool bAboveOpaque = FMCBlocks::IsOpaque(G->GetState(Pos.Up()));
			for (int32 i = 0; i < 4; ++i)
			{
				const FMCBlockPos Side = Pos.Offset(Dirs[i]);
				const FMCState SS = G->GetState(Side);
				const FMCBlock& SB = FMCBlocks::GetByState(SS);
				if (SB.Model == EMCModel::RedstoneWire) { Conn[i] = true; continue; }
				if (SB.Has(MCB_Redstone) && SB.Model != EMCModel::Piston && SB.Model != EMCModel::Door && SB.Model != EMCModel::Trapdoor && SB.Model != EMCModel::FenceGate
					&& SB.Shape != EMCShape::Cube) { Conn[i] = true; continue; }
				if (SB.Name == TEXT("redstone_block") || SB.Name == TEXT("observer") || SB.Name == TEXT("target") || SB.Name == TEXT("redstone_lamp")) { Conn[i] = true; continue; }
				if (!bAboveOpaque && FMCBlocks::IsOpaque(SS) && FMCBlocks::GetByState(G->GetState(Side.Up())).Model == EMCModel::RedstoneWire) { Conn[i] = true; Up[i] = true; continue; }
				if (!FMCBlocks::IsOpaque(SS) && FMCBlocks::GetByState(G->GetState(Side.Down())).Model == EMCModel::RedstoneWire) { Conn[i] = true; continue; }
			}
		}
		const int32 NumConn = Conn[0] + Conn[1] + Conn[2] + Conn[3];
		// single connection extends straight across
		if (NumConn == 1)
		{
			if (Conn[0] || Conn[1]) { Conn[0] = Conn[1] = true; }
			else { Conn[2] = Conn[3] = true; }
		}
		const float Z = 0.25f * PX;
		auto AddQuad = [&](FMCModelQuad Q) { Q.bTint = true; Q.Color = Col; Q.bEmissive = Power > 0; M.Quads.Add(Q); };
		if (NumConn == 0)
		{
			AddQuad(FlatQuad(0.25f, Dot));
		}
		else
		{
			const bool bNS = Conn[0] || Conn[1], bWE = Conn[2] || Conn[3];
			if (bNS && bWE || NumConn >= 3) AddQuad(FlatQuad(0.25f, Dot));
			for (int32 i = 0; i < 4; ++i)
			{
				if (!Conn[i]) continue;
				// half-line from centre to the edge (authored towards north)
				FMCModelQuad Q(FVector3f(0, 0.5f, Z), FVector3f(1, 0.5f, Z), FVector3f(1, 0, Z), FVector3f(0, 0, Z), Line);
				Q.SetUV(0, 0, 1, 0.5f);
				Q.RotateY(MCMeta::QuarterFromNorth(Dirs[i]));
				AddQuad(Q);
				if (Up[i])
				{
					FMCModelQuad V = FaceQuad(Dirs[i], Line, 0.25f);
					AddQuad(V);
				}
			}
		}
		M.Outline.Add(PB(0, 0, 0, 16, 16, 1));
		M.bExplicitCollision = true;
	}

	void BuildButton(const FMCBlock& B, uint8 Meta, FMCBlockModel& M, bool bLever)
	{
		const EMCFace F = MCMeta::Facing6(Meta);
		const bool bOn = MCMeta::Bit(Meta, 3);
		const int16 T = BT(B, FN);
		FMCBlockModel L;
		if (!bLever)
		{
			const float D = bOn ? 1.f : 2.f;
			// authored on the floor (facing up)
			L.Add(P(5, 6, 0, 11, 10, D, T));
		}
		else
		{
			const int16 Cobble = FMCTextures::Find(TEXT("cobblestone"));
			const int16 Stick = FMCTextures::Find(TEXT("oak_log"));
			L.Add(P(5, 4, 0, 11, 12, 3, Cobble));
			FMCModelBox H = bOn ? P(7, 3, 3, 9, 6, 10, Stick) : P(7, 10, 3, 9, 13, 10, Stick);
			L.Add(H);
		}
		// orient: floor(Up) default; ceiling(Down) flips; walls rotate so the base sits on the wall
		for (FMCModelBox& Bx : L.Boxes)
		{
			FMCBox Bb = Bx.ToBox();
			if (F == EMCFace::Down) { Bb = FMCBox(Bb.Min.X, Bb.Min.Y, 1 - Bb.Max.Z, Bb.Max.X, Bb.Max.Y, 1 - Bb.Min.Z); }
			else if (F != EMCFace::Up)
			{
				// map floor (x, y, z) -> wall on south side facing north: (x, 1 - z, y)
				FMCBox W(Bb.Min.X, 1 - Bb.Max.Z, Bb.Min.Y, Bb.Max.X, 1 - Bb.Min.Z, Bb.Max.Y);
				Bb = W.RotateY(MCMeta::QuarterFromNorth(F));
			}
			Bx.Min = FVector3f(Bb.Min); Bx.Max = FVector3f(Bb.Max);
			Bx.CullFaces = 0;
			M.Outline.Add(Bb);
		}
		M.Boxes = L.Boxes;
		M.bExplicitCollision = true;
	}

	void BuildRepeater(const FMCBlock& B, uint8 Meta, FMCBlockModel& M, bool bComparator)
	{
		const EMCFace F = MCMeta::Facing4(Meta);
		const bool bPowered = bComparator ? MCMeta::Bit(Meta, 3) : MCMeta::Bit(Meta, 4);
		const int16 Base = FMCTextures::Find(bComparator ? (bPowered ? TEXT("comparator_on") : TEXT("comparator")) : (bPowered ? TEXT("repeater_on") : TEXT("repeater")));
		const int16 Stone = FMCTextures::Find(TEXT("smooth_stone"));
		const int16 TorchOn = FMCTextures::Find(TEXT("redstone_torch")), TorchOff = FMCTextures::Find(TEXT("redstone_torch_off"));
		FMCBlockModel L;
		FMCModelBox Slab = P(0, 0, 0, 16, 16, 2, Stone);
		Slab.Tex[FU] = Base;
		L.Add(Slab);
		const int16 Tt = bPowered ? TorchOn : TorchOff;
		if (!bComparator)
		{
			const int32 Delay = (Meta >> 2) & 3;
			// output towards north: front torch fixed at y 2..4, back torch moves with delay
			L.Add(P(7, 2, 2, 9, 4, 7, Tt));
			const float Y = 6.f + Delay * 2.f;
			L.Add(P(7, Y, 2, 9, Y + 2, 7, Tt));
		}
		else
		{
			const bool bSub = MCMeta::Bit(Meta, 2);
			L.Add(P(7, 2, 2, 9, 4, bSub ? 6.f : 5.f, bSub ? TorchOn : TorchOff));
			L.Add(P(4, 11, 2, 6, 13, 7, Tt));
			L.Add(P(10, 11, 2, 12, 13, 7, Tt));
		}
		L.Collision.Add(PB(0, 0, 0, 16, 16, 2));
		L.RotateY(MCMeta::QuarterFromNorth(F));
		M = L;
	}

	void BuildBed(const FMCBlock& B, uint8 Meta, FMCBlockModel& M)
	{
		const EMCFace F = MCMeta::Facing4(Meta);
		const bool bHead = MCMeta::Bit(Meta, 2);
		const int16 Cloth = B.Tex[FU];
		const int16 Wood = FMCTextures::Find(TEXT("oak_planks"));
		const int16 Pillow = FMCTextures::Find(TEXT("bed_pillow"));
		FMCBlockModel L; // authored facing North: head towards north
		FMCModelBox Mat = P(0, 0, 3, 16, 16, 9, Cloth);
		Mat.TintFaces = 0x3F;
		Mat.Tex[FD] = Wood;
		L.Add(Mat);
		if (bHead)
		{
			L.Add(P(0, 0, 0, 3, 3, 3, Wood));
			L.Add(P(13, 0, 0, 16, 3, 3, Wood));
			L.Add(P(2, 1, 9, 14, 7, 11, Pillow));
		}
		else
		{
			L.Add(P(0, 13, 0, 3, 16, 3, Wood));
			L.Add(P(13, 13, 0, 16, 16, 3, Wood));
		}
		L.Collision.Add(PB(0, 0, 0, 16, 16, 9));
		L.RotateY(MCMeta::QuarterFromNorth(F));
		M = L;
	}

	void BuildChest(const FMCBlock& B, uint8 Meta, FMCBlockModel& M)
	{
		const EMCFace F = MCMeta::Facing4(Meta);
		const int32 Type = (Meta >> 2) & 3;
		const int16 T = BT(B, FN);
		const int16 Latch = FMCTextures::Find(B.Name == TEXT("ender_chest") ? TEXT("end_portal_frame_eye") : TEXT("gold_block"));
		FMCBlockModel L; // front faces north
		const float X0 = (Type == 2) ? 0.f : 1.f, X1 = (Type == 1) ? 16.f : 15.f;
		L.Add(P(X0, 1, 0, X1, 15, 10, T));
		L.Add(P(X0, 1, 10, X1, 15, 14, T));
		if (Type != 1) L.Add(P(7, 0, 7, 9, 1, 11, Latch));
		for (FMCModelBox& Bx : L.Boxes) Bx.CullFaces &= (1 << FD);
		L.Collision.Add(PB(X0, 1, 0, X1, 15, 14));
		L.RotateY(MCMeta::QuarterFromNorth(F));
		M = L;
	}

	void BuildLantern(const FMCBlock& B, uint8 Meta, FMCBlockModel& M)
	{
		const bool bHanging = MCMeta::Bit(Meta, 0);
		const int16 T = FMCTextures::Find(B.Name == TEXT("soul_lantern") ? TEXT("soul_lantern") : (B.Name == TEXT("copper_lantern") ? TEXT("copper_lantern") : TEXT("lantern")));
		const int16 Chain = FMCTextures::Find(TEXT("chain"));
		const float Z0 = bHanging ? 1.f : 0.f;
		FMCModelBox Body = P(5, 5, Z0, 11, 11, Z0 + 7, T); Body.CullFaces = 0; Body.bEmissive = true;
		FMCModelBox Cap = P(6, 6, Z0 + 7, 10, 10, Z0 + 9, T); Cap.CullFaces = 0;
		M.Add(Body); M.Add(Cap);
		if (bHanging)
		{
			FMCModelQuad Q1 = FMCModelQuad(FVector3f(6.5f * PX, 6.5f * PX, 10 * PX), FVector3f(9.5f * PX, 9.5f * PX, 10 * PX), FVector3f(9.5f * PX, 9.5f * PX, 1), FVector3f(6.5f * PX, 6.5f * PX, 1), Chain);
			M.Quads.Add(Q1);
		}
		M.Collision.Add(PB(5, 5, Z0, 11, 11, Z0 + 9));
	}

	void BuildChain(const FMCBlock& B, uint8 Meta, FMCBlockModel& M)
	{
		const int16 T = FMCTextures::Find(B.Name == TEXT("copper_chain") ? TEXT("copper_chain") : TEXT("chain"));
		FMCModelQuad Q1(FVector3f(6.5f * PX, 6.5f * PX, 0), FVector3f(9.5f * PX, 9.5f * PX, 0), FVector3f(9.5f * PX, 9.5f * PX, 1), FVector3f(6.5f * PX, 6.5f * PX, 1), T);
		FMCModelQuad Q2(FVector3f(6.5f * PX, 9.5f * PX, 0), FVector3f(9.5f * PX, 6.5f * PX, 0), FVector3f(9.5f * PX, 6.5f * PX, 1), FVector3f(6.5f * PX, 9.5f * PX, 1), T);
		Q1.SetUV(6.5f * PX, 0, 9.5f * PX, 1); Q2.SetUV(6.5f * PX, 0, 9.5f * PX, 1);
		const uint8 Axis = MCMeta::Axis(Meta);
		auto Orient = [&](FMCModelQuad& Q)
		{
			for (FVector3f& V : Q.P)
			{
				if (Axis == 1) V = FVector3f(V.Z, V.Y, 1 - V.X);
				else if (Axis == 2) V = FVector3f(V.X, V.Z, 1 - V.Y);
			}
		};
		Orient(Q1); Orient(Q2);
		M.Quads.Add(Q1); M.Quads.Add(Q2);
		if (Axis == 1) M.Collision.Add(PB(0, 6.5f, 6.5f, 16, 9.5f, 9.5f));
		else if (Axis == 2) M.Collision.Add(PB(6.5f, 0, 6.5f, 9.5f, 16, 9.5f));
		else M.Collision.Add(PB(6.5f, 6.5f, 0, 9.5f, 9.5f, 16));
	}

	void BuildPiston(const FMCBlock& B, uint8 Meta, FMCBlockModel& M, bool bHead)
	{
		const EMCFace F = MCMeta::Facing6(Meta);
		const int16 Front = FMCTextures::Find((bHead && MCMeta::Bit(Meta, 3)) ? TEXT("piston_top_sticky") : TEXT("piston_top"));
		const int16 Side = FMCTextures::Find(TEXT("piston_side"));
		const int16 Bottom = FMCTextures::Find(TEXT("piston_bottom"));
		const int16 Inner = FMCTextures::Find(TEXT("piston_inner"));
		// authored facing Up
		FMCBlockModel L;
		if (!bHead)
		{
			const bool bExt = MCMeta::Bit(Meta, 3);
			FMCModelBox Body = P(0, 0, 0, 16, 16, bExt ? 12.f : 16.f, Side);
			Body.Tex[FU] = bExt ? Inner : FMCTextures::Find(B.Name == TEXT("sticky_piston") ? TEXT("piston_top_sticky") : TEXT("piston_top"));
			Body.Tex[FD] = Bottom;
			L.Add(Body);
		}
		else
		{
			FMCModelBox Plate = P(0, 0, 12, 16, 16, 16, Side); Plate.Tex[FU] = Front; Plate.Tex[FD] = Front;
			L.Add(Plate);
			FMCModelBox Rod = P(6, 6, -4, 10, 10, 12, Side); Rod.CullFaces = 0;
			L.Add(Rod);
		}
		// rotate Up-authored model to facing F
		for (FMCModelBox& Bx : L.Boxes)
		{
			FMCBox Bb = Bx.ToBox();
			int16 T[6]; for (int32 i = 0; i < 6; ++i) T[i] = Bx.Tex[i];
			if (F == EMCFace::Down)
			{
				Bb = FMCBox(Bb.Min.X, Bb.Min.Y, 1 - Bb.Max.Z, Bb.Max.X, Bb.Max.Y, 1 - Bb.Min.Z);
				Swap(T[FU], T[FD]);
			}
			else if (F != EMCFace::Up)
			{
				// up -> north: (x, y, z) -> (x, 1 - z, y)
				Bb = FMCBox(Bb.Min.X, 1 - Bb.Max.Z, Bb.Min.Y, Bb.Max.X, 1 - Bb.Min.Z, Bb.Max.Y);
				const int16 Top = T[FU], Bot = T[FD], S = T[FN];
				T[FN] = Top; T[FS] = Bot; T[FU] = S; T[FD] = S; T[FW] = S; T[FE] = S;
				FMCModelBox Tmp; Tmp.Min = FVector3f(Bb.Min); Tmp.Max = FVector3f(Bb.Max);
				for (int32 i = 0; i < 6; ++i) Tmp.Tex[i] = T[i];
				Tmp.RotateY(MCMeta::QuarterFromNorth(F));
				Bb = Tmp.ToBox();
				for (int32 i = 0; i < 6; ++i) T[i] = Tmp.Tex[i];
			}
			Bx.Min = FVector3f(Bb.Min); Bx.Max = FVector3f(Bb.Max);
			for (int32 i = 0; i < 6; ++i) Bx.Tex[i] = T[i];
			Bx.CullFaces = 0;
			Bx.AutoCull();
		}
		M.Boxes = L.Boxes;
	}

	void BuildGeneric(const FMCBlock& B, uint8 Meta, FMCBlockModel& M)
	{
		const int16* T = B.Tex;
		switch (B.Model)
		{
		case EMCModel::Carpet: M.Add(P6(0, 0, 0, 16, 16, 1, T)); break;
		case EMCModel::SnowLayer:
		{
			const int32 Layers = (Meta & 7) + 1;
			const int16 Snow = FMCTextures::Find(TEXT("snow"));
			M.Add(P(0, 0, 0, 16, 16, Layers * 2.f, Snow));
			M.Collision.Add(PB(0, 0, 0, 16, 16, (Layers - 1) * 2.f));
			M.bExplicitCollision = true;
			M.Outline.Add(PB(0, 0, 0, 16, 16, Layers * 2.f));
			break;
		}
		case EMCModel::PressurePlate:
		{
			const bool bPressed = (Meta & 15) != 0;
			M.Add(P6(1, 1, 0, 15, 15, bPressed ? 0.5f : 1.f, T));
			M.bExplicitCollision = true;
			break;
		}
		case EMCModel::Cactus:
		{
			FMCModelBox C = P6(1, 1, 0, 15, 15, 16, T);
			C.CullFaces = (1 << FU) | (1 << FD);
			M.Add(C);
			M.Collision.Add(PB(1, 1, 0, 15, 15, 15));
			M.Outline.Add(PB(1, 1, 0, 15, 15, 16));
			break;
		}
		case EMCModel::Farmland:
		case EMCModel::Path:
		{
			FMCModelBox F = P6(0, 0, 0, 16, 16, 15, T);
			if (B.Model == EMCModel::Farmland && (Meta & 7) == 7 && B.TexAlt[0] >= 0) F.Tex[FU] = B.TexAlt[0];
			M.Add(F);
			break;
		}
		case EMCModel::Vine:
		{
			const int16 Tex = FMCTextures::Find(TEXT("vine"));
			const EMCFace Faces[4] = { EMCFace::North, EMCFace::South, EMCFace::West, EMCFace::East };
			for (int32 i = 0; i < 4; ++i) if (MCMeta::Bit(Meta, i)) { FMCModelQuad Q = FaceQuad(Faces[i], Tex, 0.8f); Q.bTint = true; Q.bWave = true; M.Quads.Add(Q); }
			if (MCMeta::Bit(Meta, 4)) { FMCModelQuad Q = FaceQuad(EMCFace::Up, Tex, 0.8f); Q.bTint = true; M.Quads.Add(Q); }
			if (M.Quads.Num() == 0) { FMCModelQuad Q = FaceQuad(EMCFace::North, Tex, 0.8f); Q.bTint = true; M.Quads.Add(Q); }
			M.bExplicitCollision = true;
			M.Outline.Add(PB(0, 0, 0, 16, 16, 16));
			break;
		}
		case EMCModel::GlowLichen:
		{
			const int16 Tex = B.TexNames[0].IsNone() ? 0 : B.Tex[0];
			for (int32 f = 0; f < 6; ++f) if (MCMeta::Bit(Meta, f)) M.Quads.Add(FaceQuad((EMCFace)f, Tex, 0.5f));
			if (M.Quads.Num() == 0) M.Quads.Add(FaceQuad(EMCFace::Down, Tex, 0.5f));
			for (FMCModelQuad& Q : M.Quads) Q.bEmissive = B.LightEmission > 0;
			M.bExplicitCollision = true;
			M.Outline.Add(PB(0, 0, 0, 16, 16, 1));
			break;
		}
		case EMCModel::Anvil:
		{
			FMCBlockModel L;
			L.Add(P6(2, 2, 0, 14, 14, 4, T));
			L.Add(P6(4, 3, 4, 12, 13, 5, T));
			L.Add(P6(6, 4, 5, 10, 12, 10, T));
			L.Add(P6(3, 0, 10, 13, 16, 16, T));
			L.RotateY(MCMeta::QuarterFromNorth(MCMeta::Facing4(Meta)) + 1);
			for (FMCModelBox& Bx : L.Boxes) for (int32 i = 0; i < 6; ++i) Bx.Tex[i] = T[i == FU ? FU : FN];
			M = L;
			break;
		}
		case EMCModel::EnchantingTable: M.Add(P6(0, 0, 0, 16, 16, 12, T)); break;
		case EMCModel::BrewingStand:
		{
			const int16 Base = FMCTextures::Find(TEXT("brewing_stand_base"));
			const int16 Rod = FMCTextures::Find(TEXT("brewing_stand"));
			M.Add(P(9, 5, 0, 15, 11, 2, Base)); M.Add(P(2, 1, 0, 8, 7, 2, Base)); M.Add(P(2, 9, 0, 8, 15, 2, Base));
			FMCModelBox R = P(7, 7, 0, 9, 9, 14, Rod); R.CullFaces = 0; M.Add(R);
			M.Collision.Add(PB(1, 1, 0, 15, 15, 2)); M.Collision.Add(PB(7, 7, 0, 9, 9, 14));
			break;
		}
		case EMCModel::Cauldron:
		{
			const int16 S = FMCTextures::Find(TEXT("cauldron_side")), In = FMCTextures::Find(TEXT("cauldron_inner"));
			M.Add(P(0, 0, 3, 16, 16, 4, In));
			M.Add(P(0, 0, 4, 2, 16, 16, S)); M.Add(P(14, 0, 4, 16, 16, 16, S));
			M.Add(P(2, 0, 4, 14, 2, 16, S)); M.Add(P(2, 14, 4, 14, 16, 16, S));
			M.Add(P(0, 0, 0, 4, 2, 3, S)); M.Add(P(12, 0, 0, 16, 2, 3, S)); M.Add(P(0, 14, 0, 4, 16, 3, S)); M.Add(P(12, 14, 0, 16, 16, 3, S));
			const int32 Level = Meta & 3;
			if (Level > 0 && B.Name != TEXT("cauldron"))
			{
				const bool bLava = B.Name == TEXT("lava_cauldron");
				const int16 FluidTex = FMCTextures::Find(bLava ? TEXT("lava_still") : TEXT("water_still"));
				FMCModelQuad Q = FlatQuad(4.f + Level * 3.f, FluidTex, 2.f);
				Q.bTint = !bLava; Q.bEmissive = bLava;
				Q.Layer = (uint8)(bLava ? EMCLayer::Lava : EMCLayer::Water);
				Q.Color = bLava ? FColor::White : FColor(63, 118, 228);
				M.Quads.Add(Q);
			}
			for (int32 i = 1; i < M.Boxes.Num(); ++i) M.Collision.Add(M.Boxes[i].ToBox());
			M.Collision.Add(PB(0, 0, 0, 16, 16, 4));
			M.Outline.Add(PB(0, 0, 0, 16, 16, 16));
			break;
		}
		case EMCModel::Hopper:
		{
			const int16 O = FMCTextures::Find(TEXT("hopper_outside")), In = FMCTextures::Find(TEXT("hopper_inside"));
			M.Add(P(0, 0, 10, 16, 16, 11, In));
			M.Add(P(0, 0, 11, 2, 16, 16, O)); M.Add(P(14, 0, 11, 16, 16, 16, O)); M.Add(P(2, 0, 11, 14, 2, 16, O)); M.Add(P(2, 14, 11, 14, 16, 16, O));
			M.Add(P(4, 4, 4, 12, 12, 10, O));
			const EMCFace F = MCMeta::Facing6(Meta);
			if (F == EMCFace::Down) M.Add(P(6, 6, 0, 10, 10, 4, O));
			else
			{
				FMCBlockModel S; S.Add(P(6, 0, 4, 10, 4, 8, O)); S.RotateY(MCMeta::QuarterFromNorth(F));
				M.Boxes.Append(S.Boxes);
			}
			M.Collision.Add(PB(0, 0, 10, 16, 16, 16)); M.Collision.Add(PB(4, 4, 4, 12, 12, 10));
			M.Outline.Add(PB(0, 0, 0, 16, 16, 16));
			break;
		}
		case EMCModel::EndPortalFrame:
		{
			FMCModelBox Fr = P6(0, 0, 0, 16, 16, 13, T);
			M.Add(Fr);
			if (MCMeta::Bit(Meta, 2))
			{
				FMCModelBox Eye = P(4, 4, 13, 12, 12, 16, FMCTextures::Find(TEXT("end_portal_frame_eye")));
				Eye.CullFaces = 0; Eye.bEmissive = true;
				M.Add(Eye);
			}
			M.Collision.Add(PB(0, 0, 0, 16, 16, 13));
			break;
		}
		case EMCModel::Campfire:
		{
			const int16 Log = B.Tex[FD];
			const bool bLit = !MCMeta::Bit(Meta, 2);
			const int16 LitLog = B.Tex[FU];
			FMCBlockModel L;
			L.Add(P(1, 0, 0, 5, 16, 4, Log)); L.Add(P(11, 0, 0, 15, 16, 4, Log));
			L.Add(P(0, 1, 3, 16, 5, 7, bLit ? LitLog : Log)); L.Add(P(0, 11, 3, 16, 15, 7, bLit ? LitLog : Log));
			L.Add(P(5, 0, 0, 11, 16, 1, Log));
			L.RotateY(MCMeta::QuarterFromNorth(MCMeta::Facing4(Meta)));
			M = L;
			if (bLit)
			{
				AddCross(M, FMCTextures::Find(B.Name == TEXT("soul_campfire") ? TEXT("soul_fire") : TEXT("fire")), 1.f, 1.f, 1.f * PX);
				for (FMCModelQuad& Q : M.Quads) Q.bEmissive = true;
			}
			M.Collision.Add(PB(0, 0, 0, 16, 16, 7));
			M.bExplicitCollision = true;
			break;
		}
		case EMCModel::Cake:
		{
			const int32 Bites = FMath::Min<int32>(Meta & 7, 6);
			const int16 Top = FMCTextures::Find(TEXT("white_marble")), Side = FMCTextures::Find(TEXT("terracotta"));
			FMCModelBox C = P(1 + Bites * 2.f, 1, 0, 15, 15, 8, Side); C.Tex[FU] = Top; C.CullFaces = 1 << FD;
			M.Add(C);
			break;
		}
		case EMCModel::FlowerPot:
		{
			const int16 T0 = FMCTextures::Find(TEXT("terracotta"));
			M.Add(P(5, 5, 0, 11, 11, 6, T0));
			const int32 Plant = Meta & 31;
			static const TCHAR* Plants[] = { nullptr, TEXT("poppy"), TEXT("dandelion"), TEXT("blue_orchid"), TEXT("allium"), TEXT("azure_bluet"),
				TEXT("red_tulip"), TEXT("orange_tulip"), TEXT("white_tulip"), TEXT("pink_tulip"), TEXT("oxeye_daisy"), TEXT("cornflower"),
				TEXT("lily_of_the_valley"), TEXT("wither_rose"), TEXT("oak_sapling"), TEXT("spruce_sapling"), TEXT("birch_sapling"),
				TEXT("jungle_sapling"), TEXT("acacia_sapling"), TEXT("dark_oak_sapling"), TEXT("cherry_sapling"), TEXT("fern"),
				TEXT("dead_bush"), TEXT("red_mushroom"), TEXT("brown_mushroom"), TEXT("bamboo_sapling"), TEXT("crimson_fungus"),
				TEXT("warped_fungus"), TEXT("torchflower"), TEXT("pale_oak_sapling"), TEXT("azalea_side"), TEXT("mangrove_propagule") };
			if (Plant > 0 && Plant < UE_ARRAY_COUNT(Plants) && Plants[Plant])
			{
				AddCross(M, FMCTextures::Find(Plants[Plant]), 0.8f, 0.8f, 5.f * PX, Plant == 21);
			}
			M.Collision.Add(PB(5, 5, 0, 11, 11, 6));
			break;
		}
		case EMCModel::Portal:
		{
			const bool bAxisY = MCMeta::Bit(Meta, 0);
			FMCModelBox Pl = bAxisY ? P(6, 0, 0, 10, 16, 16, B.Tex[0]) : P(0, 6, 0, 16, 10, 16, B.Tex[0]);
			Pl.bEmissive = true;
			if (bAxisY) { Pl.Tex[FN] = Pl.Tex[FS] = -1; Pl.Tex[FU] = Pl.Tex[FD] = -1; }
			else { Pl.Tex[FW] = Pl.Tex[FE] = -1; Pl.Tex[FU] = Pl.Tex[FD] = -1; }
			M.Add(Pl);
			M.bExplicitCollision = true;
			break;
		}
		case EMCModel::EndPortal:
		{
			FMCModelBox Pl = P(0, 0, 0, 16, 16, 12, FMCTextures::Find(TEXT("end_portal")));
			for (int32 f = 0; f < 6; ++f) if (f != FU) Pl.Tex[f] = -1;
			Pl.bEmissive = true;
			M.Add(Pl);
			M.bExplicitCollision = true;
			break;
		}
		case EMCModel::Fire:
			AddCross(M, B.Tex[0], 1.2f, 1.f);
			for (FMCModelQuad& Q : M.Quads) { Q.bEmissive = true; }
			M.bExplicitCollision = true;
			M.Outline.Add(PB(0, 0, 0, 16, 16, 1));
			break;
		case EMCModel::Lectern:
		{
			FMCBlockModel L;
			L.Add(P6(0, 0, 0, 16, 16, 2, T));
			L.Add(P6(4, 4, 2, 12, 12, 13, T));
			FMCModelBox Top = P6(0, 1, 12, 16, 15, 16, T); L.Add(Top);
			L.RotateY(MCMeta::QuarterFromNorth(MCMeta::Facing4(Meta)));
			M = L;
			break;
		}
		case EMCModel::Grindstone:
		{
			FMCBlockModel L;
			const int16 Wheel = B.Tex[FN], Pivot = FMCTextures::Find(TEXT("grindstone_pivot"));
			L.Add(P(4, 2, 4, 12, 14, 16, Wheel));
			L.Add(P(2, 6, 7, 4, 10, 13, Pivot)); L.Add(P(12, 6, 7, 14, 10, 13, Pivot));
			L.Add(P(2, 6, 0, 4, 10, 7, Pivot)); L.Add(P(12, 6, 0, 14, 10, 7, Pivot));
			L.RotateY(MCMeta::QuarterFromNorth(MCMeta::Facing4(Meta)));
			M = L;
			break;
		}
		case EMCModel::Stonecutter:
		{
			FMCModelBox Bs = P6(0, 0, 0, 16, 16, 9, T); M.Add(Bs);
			FMCModelQuad Saw = FMCModelQuad(FVector3f(1 * PX, 0.5f, 9 * PX), FVector3f(15 * PX, 0.5f, 9 * PX), FVector3f(15 * PX, 0.5f, 16 * PX), FVector3f(1 * PX, 0.5f, 16 * PX), FMCTextures::Find(TEXT("stonecutter_saw")));
			Saw.SetUV(1 * PX, 0, 15 * PX, 7 * PX);
			Saw.RotateY(MCMeta::QuarterFromNorth(MCMeta::Facing4(Meta)));
			M.Quads.Add(Saw);
			M.Collision.Add(PB(0, 0, 0, 16, 16, 9));
			break;
		}
		case EMCModel::Bell:
		{
			const int16 G = B.Tex[0];
			const int16 Wood = FMCTextures::Find(TEXT("dark_oak_planks"));
			M.Add(P(5, 5, 2, 11, 11, 9, G)); M.Add(P(4, 4, 2, 12, 12, 4, G));
			FMCBlockModel L; L.Add(P(2, 7, 13, 14, 9, 15, Wood)); L.Add(P(0, 6, 0, 2, 10, 16, Wood)); L.Add(P(14, 6, 0, 16, 10, 16, Wood));
			L.RotateY(MCMeta::QuarterFromNorth(MCMeta::Facing4(Meta)));
			M.Boxes.Append(L.Boxes);
			for (FMCModelBox& Bx : M.Boxes) Bx.CullFaces = 0;
			break;
		}
		case EMCModel::Scaffolding:
		{
			const int16 Top = B.Tex[FU], Side = B.Tex[FN];
			FMCModelBox TopBox = P(0, 0, 14, 16, 16, 16, Top); TopBox.Tex[FN] = TopBox.Tex[FS] = TopBox.Tex[FW] = TopBox.Tex[FE] = Side;
			M.Add(TopBox);
			M.Add(P(0, 0, 0, 2, 2, 14, Side)); M.Add(P(14, 0, 0, 16, 2, 14, Side)); M.Add(P(0, 14, 0, 2, 16, 14, Side)); M.Add(P(14, 14, 0, 16, 16, 14, Side));
			M.Collision.Add(PB(0, 0, 14, 16, 16, 16));
			M.Outline.Add(PB(0, 0, 0, 16, 16, 16));
			break;
		}
		case EMCModel::Candle:
		{
			const int32 Count = (Meta & 3) + 1;
			const bool bLit = MCMeta::Bit(Meta, 2);
			static const float Pos[4][4][2] = { { { 8, 8 } }, { { 6, 8 }, { 10, 8 } }, { { 6, 7 }, { 10, 7 }, { 8, 10 } }, { { 6, 6 }, { 10, 6 }, { 6, 10 }, { 10, 10 } } };
			const int16 Flame = FMCTextures::Find(TEXT("fire"));
			for (int32 i = 0; i < Count; ++i)
			{
				const float X = Pos[Count - 1][i][0], Y = Pos[Count - 1][i][1];
				FMCModelBox C = P(X - 1, Y - 1, 0, X + 1, Y + 1, 6, B.Tex[0]); C.TintFaces = 0x3F; C.CullFaces = 1 << FD;
				M.Add(C);
				if (bLit)
				{
					FMCModelBox Fl = P(X - 0.5f, Y - 0.5f, 6, X + 0.5f, Y + 0.5f, 8, Flame); Fl.bEmissive = true; Fl.CullFaces = 0;
					M.Add(Fl);
				}
			}
			break;
		}
		case EMCModel::DragonEgg:
			M.Add(P6(6, 6, 15, 10, 10, 16, T)); M.Add(P6(5, 5, 14, 11, 11, 15, T)); M.Add(P6(4, 4, 13, 12, 12, 14, T));
			M.Add(P6(3, 3, 11, 13, 13, 13, T)); M.Add(P6(2, 2, 8, 14, 14, 11, T)); M.Add(P6(1, 1, 3, 15, 15, 8, T)); M.Add(P6(2, 2, 1, 14, 14, 3, T)); M.Add(P6(3, 3, 0, 13, 13, 1, T));
			for (FMCModelBox& Bx : M.Boxes) Bx.CullFaces &= 1 << FD;
			break;
		case EMCModel::Skull:
		{
			const bool bWall = MCMeta::Bit(Meta, 4);
			if (!bWall) M.Add(P6(4, 4, 0, 12, 12, 8, T));
			else
			{
				FMCBlockModel L; L.Add(P6(4, 8, 4, 12, 16, 12, T));
				L.RotateY(MCMeta::QuarterFromNorth(MCMeta::Facing4(Meta)));
				for (FMCModelBox& Bx : L.Boxes) for (int32 i = 0; i < 6; ++i) Bx.Tex[i] = T[i];
				M = L;
			}
			for (FMCModelBox& Bx : M.Boxes) Bx.CullFaces = 0;
			break;
		}
		case EMCModel::Bamboo:
		{
			const bool bThick = MCMeta::Bit(Meta, 2);
			const float R = bThick ? 1.5f : 1.f;
			FMCModelBox St = P(8 - R, 8 - R, 0, 8 + R, 8 + R, 16, B.Tex[0]); St.CullFaces = (1 << FD) | (1 << FU);
			M.Add(St);
			const int32 Leaves = Meta & 3;
			if (Leaves > 0) { AddCross(M, FMCTextures::Find(TEXT("bamboo_sapling")), Leaves == 2 ? 1.f : 0.7f, 1.f); }
			M.Collision.Add(PB(6.5f, 6.5f, 0, 9.5f, 9.5f, 16));
			break;
		}
		case EMCModel::LilyPad:
		{
			FMCModelQuad Q = FlatQuad(0.25f, FMCTextures::Find(TEXT("lily_pad")));
			Q.bTint = true; M.Quads.Add(Q);
			M.Collision.Add(PB(1, 1, 0, 15, 15, 1.5f));
			break;
		}
		case EMCModel::Daylight:
			M.Add(P6(0, 0, 0, 16, 16, 6, T));
			break;
		case EMCModel::Composter:
		{
			const int16 S = B.Tex[FN];
			M.Add(P(0, 0, 0, 16, 16, 2, S));
			M.Add(P(0, 0, 2, 2, 16, 16, S)); M.Add(P(14, 0, 2, 16, 16, 16, S)); M.Add(P(2, 0, 2, 14, 2, 16, S)); M.Add(P(2, 14, 2, 14, 16, 16, S));
			const int32 Level = FMath::Min<int32>(Meta & 15, 8);
			if (Level > 0) M.Add(P(2, 2, 2, 14, 14, 2 + Level * 1.6f, FMCTextures::Find(TEXT("compost"))));
			for (int32 i = 0; i < 5; ++i) M.Collision.Add(M.Boxes[i].ToBox());
			M.Outline.Add(PB(0, 0, 0, 16, 16, 16));
			break;
		}
		case EMCModel::EndRod:
		case EMCModel::AmethystCluster:
		{
			const EMCFace F = MCMeta::Facing6(Meta);
			if (B.Model == EMCModel::AmethystCluster)
			{
				AddCross(M, FMCTextures::Find(TEXT("amethyst_cluster")), 0.9f, 0.45f);
				for (FMCModelQuad& Q : M.Quads) Q.bEmissive = true;
			}
			else
			{
				const int16 T0 = B.Tex[0];
				FMCModelBox Rod = P(7, 7, 1, 9, 9, 16, T0); Rod.CullFaces = 0; Rod.bEmissive = B.LightEmission > 0;
				FMCModelBox Base = P(6, 6, 0, 10, 10, 1, T0); Base.CullFaces = 0;
				M.Add(Rod); M.Add(Base);
			}
			// orient Up-authored model
			auto Xf = [&](FVector3f V) -> FVector3f
			{
				if (F == EMCFace::Up) return V;
				if (F == EMCFace::Down) return FVector3f(V.X, V.Y, 1 - V.Z);
				FVector3f W(V.X, 1 - V.Z, V.Y); // up -> north
				const int32 Q = MCMeta::QuarterFromNorth(F);
				for (int32 i = 0; i < Q; ++i) W = FVector3f(1 - W.Y, W.X, W.Z);
				return W;
			};
			for (FMCModelBox& Bx : M.Boxes)
			{
				const FVector3f A = Xf(Bx.Min), C = Xf(Bx.Max);
				Bx.Min = FVector3f(FMath::Min(A.X, C.X), FMath::Min(A.Y, C.Y), FMath::Min(A.Z, C.Z));
				Bx.Max = FVector3f(FMath::Max(A.X, C.X), FMath::Max(A.Y, C.Y), FMath::Max(A.Z, C.Z));
			}
			for (FMCModelQuad& Q : M.Quads) for (FVector3f& V : Q.P) V = Xf(V);
			for (const FMCModelBox& Bx : M.Boxes) M.Collision.Add(Bx.ToBox());
			if (M.Boxes.Num() == 0) { M.Outline.Add(FMCBox(0.2, 0.2, 0.0, 0.8, 0.8, 0.5)); M.bExplicitCollision = true; }
			break;
		}
		case EMCModel::Conduit: M.Add(P6(5, 5, 5, 11, 11, 11, T)); for (FMCModelBox& Bx : M.Boxes) Bx.bEmissive = true; break;
		case EMCModel::Beacon:
		{
			FMCModelBox Glass = P(0, 0, 0, 16, 16, 16, FMCTextures::Find(TEXT("glass")));
			M.Add(Glass);
			FMCModelBox Core = P(3, 3, 3, 13, 13, 14, FMCTextures::Find(TEXT("beacon"))); Core.bEmissive = true; Core.CullFaces = 0; Core.Layer = (uint8)EMCLayer::Opaque;
			M.Add(Core);
			FMCModelBox Base = P(2, 2, 0, 14, 14, 3, FMCTextures::Find(TEXT("obsidian"))); Base.Layer = (uint8)EMCLayer::Opaque;
			M.Add(Base);
			M.Collision.Add(FMCBox(0, 0, 0, 1, 1, 1));
			break;
		}
		case EMCModel::SculkSensor: M.Add(P6(0, 0, 0, 16, 16, 8, T)); break;
		case EMCModel::Shelf:
		{
			FMCBlockModel L;
			L.Add(P6(0, 11, 0, 16, 16, 16, T)); L.Add(P6(0, 5, 7, 16, 11, 9, T));
			L.RotateY(MCMeta::QuarterFromNorth(MCMeta::Facing4(Meta)));
			for (FMCModelBox& Bx : L.Boxes) for (int32 i = 0; i < 6; ++i) Bx.Tex[i] = T[i];
			M = L;
			break;
		}
		case EMCModel::Dripstone:
		{
			const bool bDown = MCMeta::Bit(Meta, 0);
			const int32 Thick = (Meta >> 1) & 3;
			const float W = 2.f + Thick * 1.5f;
			const int16 Tex = FMCTextures::Find(B.Name == TEXT("sulfur_spike") ? TEXT("sulfur_spike") : TEXT("pointed_dripstone"));
			AddCross(M, Tex, W / 8.f + 0.3f, 1.f);
			if (bDown) for (FMCModelQuad& Q : M.Quads) for (FVector3f& V : Q.P) V.Z = 1 - V.Z;
			for (FMCModelQuad& Q : M.Quads) Q.bEmissive = B.LightEmission > 0;
			M.Collision.Add(FMCBox(0.5 - W * PX * 0.5, 0.5 - W * PX * 0.5, 0, 0.5 + W * PX * 0.5, 0.5 + W * PX * 0.5, 1));
			break;
		}
		case EMCModel::BigDripleaf:
		{
			FMCModelQuad Q = FlatQuad(15.f, B.Tex[0]); M.Quads.Add(Q);
			FMCModelQuad St = FMCModelQuad(FVector3f(0.5f, 0.3f, 0), FVector3f(0.5f, 0.7f, 0), FVector3f(0.5f, 0.7f, 15 * PX), FVector3f(0.5f, 0.3f, 15 * PX), FMCTextures::Find(TEXT("small_dripleaf")));
			M.Quads.Add(St);
			M.Collision.Add(PB(0, 0, 11, 16, 16, 15));
			break;
		}
		case EMCModel::SeaPickle:
		{
			const int32 Count = (Meta & 3) + 1;
			const int16 Tex = FMCTextures::Find(TEXT("moss_block"));
			for (int32 i = 0; i < Count; ++i)
			{
				const float X = 4.f + (i % 2) * 6.f, Y = 4.f + (i / 2) * 6.f;
				FMCModelBox Bx = P(X, Y, 0, X + 3, Y + 3, 6, Tex); Bx.bEmissive = true; Bx.CullFaces = 0; M.Add(Bx);
			}
			break;
		}
		case EMCModel::Pot:
			M.Add(P6(2, 2, 0, 14, 14, 13, T)); M.Add(P6(4, 4, 13, 12, 12, 16, T));
			break;
		case EMCModel::Head:
			M.Add(P6(4, 4, 0, 12, 12, 8, T));
			break;
		case EMCModel::ChorusPlant:
			M.Add(P6(2, 2, 2, 14, 14, 14, T));
			break;
		default:
			M.Add(P6(0, 0, 0, 16, 16, 16, T));
			break;
		}
	}

	void BuildChorus(const FMCBlock& B, uint8 Meta, const FMCBlockGetter* G, const FMCBlockPos& Pos, FMCBlockModel& M)
	{
		const bool bFlower = B.Name == TEXT("chorus_flower");
		const int16 T = FMCTextures::Find(bFlower ? ((Meta & 7) >= 5 ? TEXT("chorus_flower_dead") : TEXT("chorus_flower")) : TEXT("chorus_plant"));
		if (bFlower) { M.Add(P(1, 1, 1, 15, 15, 15, T)); return; }
		M.Add(P(4, 4, 4, 12, 12, 12, T));
		for (int32 f = 0; f < 6; ++f)
		{
			const EMCFace F = (EMCFace)f;
			const FMCState N = G ? G->GetState(Pos.Offset(F)) : 0;
			const FMCBlock& NB = FMCBlocks::GetByState(N);
			const bool bConn = NB.Model == EMCModel::ChorusPlant || (F == EMCFace::Down && NB.Name == TEXT("end_stone"));
			if (!bConn) continue;
			switch (F)
			{
			case EMCFace::Down: M.Add(P(4, 4, 0, 12, 12, 4, T)); break;
			case EMCFace::Up: M.Add(P(4, 4, 12, 12, 12, 16, T)); break;
			case EMCFace::North: M.Add(P(4, 0, 4, 12, 4, 12, T)); break;
			case EMCFace::South: M.Add(P(4, 12, 4, 12, 16, 12, T)); break;
			case EMCFace::West: M.Add(P(0, 4, 4, 4, 12, 12, T)); break;
			case EMCFace::East: M.Add(P(12, 4, 4, 16, 12, 12, T)); break;
			default: break;
			}
		}
	}

	void BuildFireDynamic(const FMCBlock& B, uint8 Meta, const FMCBlockGetter* G, const FMCBlockPos& Pos, FMCBlockModel& M)
	{
		const int16 T = B.Tex[0];
		const FMCState Below = G ? G->GetState(Pos.Down()) : 0;
		if (!G || FMCBlocks::IsSolid(Below) || B.Name == TEXT("soul_fire"))
		{
			AddCross(M, T, 1.25f, 1.1f);
			// extra outward-leaning quads for a fuller flame
			const float L = 0.25f;
			M.Quads.Add(FMCModelQuad(FVector3f(0, 0.5f - L, 0), FVector3f(1, 0.5f - L, 0), FVector3f(1, 0.5f + L * 0.2f, 1.1f), FVector3f(0, 0.5f + L * 0.2f, 1.1f), T));
			M.Quads.Add(FMCModelQuad(FVector3f(1, 0.5f + L, 0), FVector3f(0, 0.5f + L, 0), FVector3f(0, 0.5f - L * 0.2f, 1.1f), FVector3f(1, 0.5f - L * 0.2f, 1.1f), T));
		}
		else
		{
			const EMCFace Sides[5] = { EMCFace::North, EMCFace::South, EMCFace::West, EMCFace::East, EMCFace::Up };
			for (EMCFace F : Sides)
			{
				const FMCState N = G->GetState(Pos.Offset(F));
				if (FMCBlocks::Info(N).Flags & MCB_Flammable) M.Quads.Add(FaceQuad(F, T, 1.f));
			}
			if (M.Quads.Num() == 0) AddCross(M, T, 1.2f, 1.f);
		}
		for (FMCModelQuad& Q : M.Quads) Q.bEmissive = true;
		M.bExplicitCollision = true;
		M.Outline.Add(PB(0, 0, 0, 16, 16, 1));
	}
}

bool MCIsDynamicModel(const FMCBlock& B)
{
	switch (B.Model)
	{
	case EMCModel::Stairs:
	case EMCModel::Fence:
	case EMCModel::Wall:
	case EMCModel::Pane:
	case EMCModel::Bars:
	case EMCModel::RedstoneWire:
	case EMCModel::Fire:
		return true;
	case EMCModel::ChorusPlant:
		return B.Name == TEXT("chorus_plant");
	default:
		return false;
	}
}

void MCBuildBlockModel(const FMCBlock& B, uint8 Meta, const FMCBlockGetter* G, const FMCBlockPos& Pos, FMCBlockModel& Out)
{
	Out = FMCBlockModel();
	switch (B.Model)
	{
	case EMCModel::Slab: BuildSlab(B, Meta, Out); break;
	case EMCModel::Stairs: BuildStairs(B, Meta, G, Pos, Out); break;
	case EMCModel::Fence: BuildFence(B, G, Pos, Out); break;
	case EMCModel::FenceGate: BuildFenceGate(B, Meta, Out); break;
	case EMCModel::Wall: BuildWall(B, G, Pos, Out); break;
	case EMCModel::Pane: BuildPane(B, G, Pos, Out, false); break;
	case EMCModel::Bars: BuildPane(B, G, Pos, Out, true); break;
	case EMCModel::Door: BuildDoor(B, Meta, Out); break;
	case EMCModel::Trapdoor: BuildTrapdoor(B, Meta, Out); break;
	case EMCModel::Torch: BuildTorch(B, Meta, Out); break;
	case EMCModel::Ladder: BuildLadder(B, Meta, Out); break;
	case EMCModel::Rail: BuildRail(B, Meta, Out); break;
	case EMCModel::RedstoneWire: BuildRedstoneWire(B, Meta, G, Pos, Out); break;
	case EMCModel::Button: BuildButton(B, Meta, Out, false); break;
	case EMCModel::Lever: BuildButton(B, Meta, Out, true); break;
	case EMCModel::Repeater: BuildRepeater(B, Meta, Out, false); break;
	case EMCModel::Comparator: BuildRepeater(B, Meta, Out, true); break;
	case EMCModel::Bed: BuildBed(B, Meta, Out); break;
	case EMCModel::Chest: BuildChest(B, Meta, Out); break;
	case EMCModel::Lantern: BuildLantern(B, Meta, Out); break;
	case EMCModel::Chain: BuildChain(B, Meta, Out); break;
	case EMCModel::Piston: BuildPiston(B, Meta, Out, false); break;
	case EMCModel::PistonHead: BuildPiston(B, Meta, Out, true); break;
	case EMCModel::ChorusPlant: BuildChorus(B, Meta, G, Pos, Out); break;
	case EMCModel::Fire: BuildFireDynamic(B, Meta, G, Pos, Out); break;
	default: BuildGeneric(B, Meta, Out); break;
	}
}
