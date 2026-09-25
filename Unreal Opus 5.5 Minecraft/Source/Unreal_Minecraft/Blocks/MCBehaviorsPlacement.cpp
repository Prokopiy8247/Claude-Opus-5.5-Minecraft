// Behaviour registry, shared helpers and orientation / placement behaviours.
#include "Blocks/MCBehaviorsInternal.h"
#include "Items/MCLoot.h"
#include "Gen/MCGenCommon.h"

namespace
{
	FMCBlockBehavior* GBehaviors[(int32)EMCBeh::Count] = {};
	FMCBlockBehavior GDefault;
}

bool MCBeh::GWireSignalsDisabled = false;

void MCBeh::Register(EMCBeh Id, FMCBlockBehavior* B) { GBehaviors[(int32)Id] = B; }

FMCBlockBehavior* MCBehaviors::Get(EMCBeh Id)
{
	FMCBlockBehavior* B = GBehaviors[(int32)Id];
	return B ? B : &GDefault;
}

void MCBehaviors::RegisterAll()
{
	static bool bDone = false;
	if (bDone) return;
	bDone = true;
	MCBeh::RegisterPlacement();
	MCBeh::RegisterRedstone();
	MCBeh::RegisterPlants();
	MCBeh::RegisterWorld();
	MCBeh::RegisterFunctional();
}

// ---------------------------------------------------------------------------------------------------------------------
// Context helpers

EMCFace FMCPlaceContext::PlayerLookFacing() const { return MC::FaceFromYaw(Yaw); }
EMCFace FMCPlaceContext::FacingTowardsPlayer() const { return MC::Opposite(MC::FaceFromYaw(Yaw)); }
FVector FMCPlaceContext::HitInTarget() const
{
	return HitFrac + FVector(ClickedPos.X - Pos.X, ClickedPos.Y - Pos.Y, ClickedPos.Z - Pos.Z);
}

EMCFace MCBeh::LookFacing(const FMCPlaceContext& C) { return MC::FaceFromYaw(C.Yaw); }
EMCFace MCBeh::LookFacing6(const FMCPlaceContext& C)
{
	if (C.Pitch > 45.f) return EMCFace::Up;
	if (C.Pitch < -45.f) return EMCFace::Down;
	return MC::FaceFromYaw(C.Yaw);
}

FMCState MCBeh::StateOf(const TCHAR* Name, uint8 M)
{
	const FMCBlock* B = FMCBlocks::Find(FName(Name));
	return B ? B->State(M) : 0;
}

const FMCItem* MCBeh::HeldItem(AMCPlayer* P)
{
	if (!P) return nullptr;
	const FMCItemStack& S = P->HeldConst();
	return S.IsEmpty() ? nullptr : &S.Item();
}

bool MCBeh::HeldIs(AMCPlayer* P, const TCHAR* Item)
{
	const FMCItem* I = HeldItem(P);
	return I && I->Name == FName(Item);
}

bool MCBeh::HeldHasTag(AMCPlayer* P, const TCHAR* Tag)
{
	const FMCItem* I = HeldItem(P);
	return I && I->HasTag(FName(Tag));
}

int32 MCBeh::FluidTickDelay(const FMCWorld& W, bool bLava)
{
	if (!bLava) return 5;
	return W.Dim == EMCDimension::Nether ? 10 : 30;
}

int32 MCBeh::PowerAt(const FMCWorld& W, const FMCBlockPos& P) { return W.GetBestNeighborPower(P); }

bool MCBeh::IsRainingAbove(const FMCWorld& W, const FMCBlockPos& P)
{
	return W.Game && W.Dim == EMCDimension::Overworld && W.Game->IsRainingAt(P);
}

void MCBeh::BreakUnsupported(FMCWorld& W, const FMCBlockPos& P)
{
	W.DestroyBlock(P, true, nullptr, nullptr, true);
}

void MCBeh::SpawnDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, AMCEntity* Breaker)
{
	TArray<FMCItemStack> Drops;
	MCLoot::GetBlockDrops(W, P, S, Tool, Drops, W.Rand);
	for (const FMCItemStack& D : Drops) MCBehaviorUtil::PopItem(W, P, D);
}

void MCBeh::RunFeatureInWorld(FMCWorld& W, const FMCBlockPos& Center, int32 RadiusBlocks, TFunctionRef<void(FMCGenWriter&)> Fn)
{
	const FMCChunkPos C0 = FMCChunkPos::FromBlock(Center.X - RadiusBlocks, Center.Y - RadiusBlocks);
	const FMCChunkPos C1 = FMCChunkPos::FromBlock(Center.X + RadiusBlocks, Center.Y + RadiusBlocks);
	for (int32 cy = C0.Y; cy <= C1.Y; ++cy)
		for (int32 cx = C0.X; cx <= C1.X; ++cx)
		{
			FMCChunk* Real = W.GetChunk(FMCChunkPos(cx, cy));
			if (!Real || Real->Stage != EMCChunkStage::Lit) continue;
			// scratch copy of the relevant vertical range
			FMCChunk Scratch(Real->Pos, Real->Dim);
			const int32 Z0 = FMath::Max(MC::MinZ, Center.Z - RadiusBlocks - 8), Z1 = FMath::Min(MC::MaxZ, Center.Z + RadiusBlocks + 32);
			for (int32 Z = Z0; Z <= Z1; ++Z)
				for (int32 ly = 0; ly < 16; ++ly)
					for (int32 lx = 0; lx < 16; ++lx)
					{
						const FMCState S = Real->Get(lx, ly, Z);
						if (S) Scratch.SetRaw(lx, ly, Z, S);
					}
			FMCGenWriter Wr(Scratch);
			Fn(Wr);
			for (int32 Z = Z0; Z <= Z1; ++Z)
				for (int32 ly = 0; ly < 16; ++ly)
					for (int32 lx = 0; lx < 16; ++lx)
					{
						const FMCState NS = Scratch.Get(lx, ly, Z);
						if (NS != Real->Get(lx, ly, Z)) W.SetState(FMCBlockPos(Real->Pos.MinBlockX() + lx, Real->Pos.MinBlockY() + ly, Z), NS, MCSet_Default | MCSet_NoSupport);
					}
		}
}

// ---------------------------------------------------------------------------------------------------------------------
// Utility

bool MCBehaviorUtil::IsFaceSturdy(const FMCWorld& W, const FMCBlockPos& P, EMCFace Face)
{
	const FMCState S = W.GetState(P);
	if (S == 0) return false;
	const FMCStateInfo& I = FMCBlocks::Info(S);
	if (I.Flags & MCB_Opaque) return true;
	if (I.bFullCollision && !(I.Flags & MCB_Leaves)) return true;
	const FMCBlock& B = FMCBlocks::GetByState(S);
	// partial shapes that still provide the face
	if (B.Model == EMCModel::Slab)
	{
		const uint8 M = I.Meta;
		if (M == 2) return true;
		if (Face == EMCFace::Up) return M == 1;
		if (Face == EMCFace::Down) return M == 0;
		return false;
	}
	if (B.Model == EMCModel::Stairs)
	{
		const uint8 M = I.Meta;
		const bool bTop = MCMeta::Bit(M, 2);
		if (Face == EMCFace::Up) return bTop;
		if (Face == EMCFace::Down) return !bTop;
		return MCMeta::Facing4(M) == Face;
	}
	if (I.Flags & MCB_Leaves) return Face == EMCFace::Up;
	if (B.Name == TEXT("glass") || (B.Family == TEXT("stained_glass") && B.Variant == TEXT("stained_glass"))) return true;
	if (B.Model == EMCModel::Farmland || B.Model == EMCModel::Path) return Face != EMCFace::Up;
	return false;
}

bool MCBehaviorUtil::HasSolidTop(const FMCWorld& W, const FMCBlockPos& P)
{
	const FMCState S = W.GetState(P);
	if (S == 0) return false;
	if (IsFaceSturdy(W, P, EMCFace::Up)) return true;
	const FMCBlock& B = FMCBlocks::GetByState(S);
	if (B.Model == EMCModel::Farmland || B.Model == EMCModel::Path) return true;
	if (B.Model == EMCModel::Fence || B.Model == EMCModel::Wall) return true;
	if (B.Model == EMCModel::Scaffolding || B.Model == EMCModel::Hopper) return true;
	return false;
}

void MCBehaviorUtil::DropAsItem(FMCWorld& W, const FMCBlockPos& P, FMCState S)
{
	MCBeh::SpawnDrops(W, P, S, nullptr, nullptr);
}

void MCBehaviorUtil::PopItem(FMCWorld& W, const FMCBlockPos& P, const FMCItemStack& Stack)
{
	if (Stack.IsEmpty()) return;
	if (W.Game && !W.Game->Rules.bDoTileDrops) return;
	const FVector Pos(P.X + 0.5 + W.Rand.FRange(-0.25f, 0.25f), P.Y + 0.5 + W.Rand.FRange(-0.25f, 0.25f), P.Z + 0.5 + W.Rand.FRange(-0.25f, 0.25f) - 0.125);
	W.SpawnItem(Pos, Stack, true, 0.5f);
}

bool MCBehaviorUtil::HandEmpty(AMCPlayer* Player) { return !Player || Player->HeldConst().IsEmpty(); }
bool MCBehaviorUtil::IsCreative(AMCPlayer* Player) { return Player && Player->IsCreative(); }
bool MCBehaviorUtil::IsWaterAt(const FMCWorld& W, const FMCBlockPos& P)
{
	const FMCState S = W.GetState(P);
	return FMCBlocks::BlockOf(S) == FMCBlocks::C.WaterId || (FMCBlocks::Info(S).Flags & MCB_Waterlogged);
}
bool MCBehaviorUtil::IsLavaAt(const FMCWorld& W, const FMCBlockPos& P) { return FMCBlocks::BlockOf(W.GetState(P)) == FMCBlocks::C.LavaId; }

// ---------------------------------------------------------------------------------------------------------------------
// Default behaviour: generic support rule for plants, carpets and other "NeedsSupport" blocks

bool FMCBlockBehavior::CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const
{
	if (!B.Has(MCB_NeedsSupport)) return true;
	const FMCBlockPos Below = P.Down();
	const FMCState BS = W.GetState(Below);
	if (BS == 0) return false;
	if (B.Model == EMCModel::Carpet || B.Model == EMCModel::PressurePlate || B.Model == EMCModel::RedstoneWire || B.Model == EMCModel::Repeater
		|| B.Model == EMCModel::Comparator || B.Model == EMCModel::Rail)
		return MCBehaviorUtil::HasSolidTop(W, Below) || (B.Model == EMCModel::Carpet && !FMCBlocks::IsFluid(BS));
	return !FMCBlocks::IsFluid(BS) && (FMCBlocks::IsSolid(BS) || MCBehaviorUtil::HasSolidTop(W, Below));
}

void FMCBlockBehavior::OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const
{
	const FMCBlock& B = FMCBlocks::GetByState(S);
	if (B.Has(MCB_NeedsSupport) && !CanSurvive(B, W, P, S)) MCBeh::BreakUnsupported(W, P);
}

// ---------------------------------------------------------------------------------------------------------------------
// Orientation behaviours

namespace
{
	using namespace MCBeh;

	class FAxisBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			uint8 A = 0;
			switch (C.ClickedFace) { case EMCFace::West: case EMCFace::East: A = 1; break; case EMCFace::North: case EMCFace::South: A = 2; break; default: A = 0; break; }
			return B.State(A);
		}
	};

	class FFacingToPlayerBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			return B.State(MCMeta::FromFacing4(C.FacingTowardsPlayer()));
		}
	};

	class FFacingAwayBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			return B.State(MCMeta::FromFacing4(LookFacing(C)));
		}
	};

	class FFacing6ToPlayerBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			return B.State((uint8)MC::Opposite(LookFacing6(C)));
		}
	};

	/** Attached to the clicked face, pointing away from it (end rods, amethyst clusters, lightning rods). */
	class FFacing6AwayBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			return B.State((uint8)C.ClickedFace);
		}
	};

	class FSlabBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			// merging into an existing slab of the same type happens in the target cell
			if (FMCBlocks::BlockOf(C.Existing) == B.Id)
			{
				const uint8 M = FMCBlocks::MetaOf(C.Existing);
				if (M != 2) return B.State(2);
			}
			const FVector H = C.HitInTarget();
			bool bTop = false;
			if (C.ClickedFace == EMCFace::Down) bTop = true;
			else if (C.ClickedFace != EMCFace::Up) bTop = H.Z > 0.5;
			return B.State(bTop ? 1 : 0);
		}
	};

	class FStairsBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			const FVector H = C.HitInTarget();
			const bool bTop = C.ClickedFace == EMCFace::Down || (C.ClickedFace != EMCFace::Up && H.Z > 0.5);
			return B.State(MCMeta::FromFacing4(LookFacing(C)) | (bTop ? 4 : 0));
		}
	};

	class FDoorBeh : public FMCBlockBehavior
	{
	public:
		static bool IsIron(const FMCBlock& B) { return B.Name == TEXT("iron_door"); }
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World) return 0;
			const FMCState Above = C.World->GetState(C.Pos.Up());
			if (Above != 0 && !FMCBlocks::IsReplaceable(Above)) return 0;
			if (C.Pos.Z >= MC::MaxZ) return 0;
			const EMCFace F = LookFacing(C);
			// hinge: prefer the side with a neighbouring door (double doors) or solid blocks, else the click side
			const EMCFace Left = MC::RotateY(F, 3), Right = MC::RotateY(F, 1);
			const FMCState LS = C.World->GetState(C.Pos.Offset(Left)), RS = C.World->GetState(C.Pos.Offset(Right));
			bool bRight = false;
			if (FMCBlocks::GetByState(LS).Model == EMCModel::Door && !MCMeta::Bit(FMCBlocks::MetaOf(LS), 4)) bRight = true;
			else if (FMCBlocks::GetByState(RS).Model == EMCModel::Door) bRight = false;
			else
			{
				const int32 SolidL = FMCBlocks::IsOpaque(LS) + FMCBlocks::IsOpaque(C.World->GetState(C.Pos.Offset(Left).Up()));
				const int32 SolidR = FMCBlocks::IsOpaque(RS) + FMCBlocks::IsOpaque(C.World->GetState(C.Pos.Offset(Right).Up()));
				if (SolidL != SolidR) bRight = SolidR > SolidL;
				else
				{
					const FVector H = C.HitInTarget();
					const FIntVector& D = MC::FaceDir[(int32)Right];
					const double Side = (H.X - 0.5) * D.X + (H.Y - 0.5) * D.Y;
					bRight = Side > 0;
				}
			}
			const bool bPowered = C.World->IsPowered(C.Pos) || C.World->IsPowered(C.Pos.Up());
			return B.State(MCMeta::FromFacing4(F) | (bRight ? 16 : 0) | (bPowered ? 8 : 0));
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override
		{
			W.SetState(P.Up(), WithMeta(S, Meta(S) | 4), MCSet_Default | MCSet_NoSupport);
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const bool bUpper = MCMeta::Bit(Meta(S), 2);
			if (bUpper) return FMCBlocks::BlockOf(W.GetState(P.Down())) == B.Id;
			return MCBehaviorUtil::IsFaceSturdy(W, P.Down(), EMCFace::Up) && FMCBlocks::BlockOf(W.GetState(P.Up())) == B.Id;
		}
		void SetOpen(FMCWorld& W, const FMCBlockPos& P, FMCState S, bool bOpen) const
		{
			const bool bUpper = MCMeta::Bit(Meta(S), 2);
			const FMCBlockPos Lower = bUpper ? P.Down() : P;
			for (int32 k = 0; k < 2; ++k)
			{
				const FMCBlockPos Q = Lower.Up(k);
				const FMCState QS = W.GetState(Q);
				if (FMCBlocks::BlockOf(QS) != FMCBlocks::BlockOf(S)) continue;
				W.SetState(Q, WithMeta(QS, MCMeta::SetBit(Meta(QS), 3, bOpen)), MCSet_Render | MCSet_Light | MCSet_Neighbors | MCSet_NoSupport);
			}
			const FMCBlock& B = FMCBlocks::GetByState(S);
			const bool bMetal = IsIron(B) || B.Sound == EMCSound::Copper;
			W.PlaySound(bOpen ? (bMetal ? FName(TEXT("door_iron_open")) : FName(TEXT("door_open"))) : (bMetal ? FName(TEXT("door_iron_close")) : FName(TEXT("door_close"))), Lower.Center() * MC::InvBlockSize + FVector(0, 0, 0.5));
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (IsIron(FMCBlocks::GetByState(S))) return false;
			SetOpen(W, P, S, !MCMeta::Bit(Meta(S), 3));
			return true;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (!CanSurvive(B, W, P, S))
			{
				const bool bUpper = MCMeta::Bit(Meta(S), 2);
				// only the lower half drops the item
				if (bUpper) W.SetState(P, 0); else MCBeh::BreakUnsupported(W, P);
				return;
			}
			const bool bUpper = MCMeta::Bit(Meta(S), 2);
			const FMCBlockPos Lower = bUpper ? P.Down() : P;
			const bool bPowered = W.IsPowered(Lower) || W.IsPowered(Lower.Up());
			// redstone opens/closes the door on power edges (the powered flag is tracked outside the meta)
			const FMCState LS = W.GetState(Lower);
			const bool bOpen = MCMeta::Bit(Meta(LS), 3);
			if (bPowered != PoweredCache(W, Lower))
			{
				SetPoweredCache(W, Lower, bPowered);
				if (bPowered != bOpen) SetOpen(W, P, S, bPowered);
			}
		}
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override
		{
			const bool bUpper = MCMeta::Bit(Meta(Old), 2);
			const FMCBlockPos Other = bUpper ? P.Down() : P.Up();
			if (FMCBlocks::BlockOf(W.GetState(Other)) == FMCBlocks::BlockOf(Old) && FMCBlocks::BlockOf(New) != FMCBlocks::BlockOf(Old)) W.SetState(Other, 0, MCSet_Default);
			PoweredSet.Remove(P);
		}
		static bool PoweredCache(FMCWorld& W, const FMCBlockPos& P) { return PoweredSet.Contains(P); }
		static void SetPoweredCache(FMCWorld& W, const FMCBlockPos& P, bool b) { if (b) PoweredSet.Add(P); else PoweredSet.Remove(P); }
		static TSet<FMCBlockPos> PoweredSet;
	};
	TSet<FMCBlockPos> FDoorBeh::PoweredSet;

	class FTrapdoorBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			EMCFace F;
			bool bTop;
			const FVector H = C.HitInTarget();
			if (MC::IsHorizontal(C.ClickedFace)) { F = C.ClickedFace; bTop = H.Z > 0.5; }
			else { F = C.FacingTowardsPlayer(); bTop = C.ClickedFace == EMCFace::Down; }
			return B.State(MCMeta::FromFacing4(F) | (bTop ? 4 : 0));
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.Name == TEXT("iron_trapdoor")) return false;
			const bool bOpen = !MCMeta::Bit(Meta(S), 3);
			W.SetState(P, WithMeta(S, MCMeta::SetBit(Meta(S), 3, bOpen)));
			W.PlaySound(bOpen ? TEXT("trapdoor_open") : TEXT("trapdoor_close"), P.Center() * MC::InvBlockSize);
			return true;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const bool bPowered = W.IsPowered(P);
			const bool bWas = Powered.Contains(P);
			if (bPowered == bWas) return;
			if (bPowered) Powered.Add(P); else Powered.Remove(P);
			const bool bOpen = MCMeta::Bit(Meta(S), 3);
			if (bOpen != bPowered)
			{
				W.SetState(P, WithMeta(S, MCMeta::SetBit(Meta(S), 3, bPowered)));
				W.PlaySound(bPowered ? TEXT("trapdoor_open") : TEXT("trapdoor_close"), P.Center() * MC::InvBlockSize);
			}
		}
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override { Powered.Remove(P); }
		static TSet<FMCBlockPos> Powered;
	};
	TSet<FMCBlockPos> FTrapdoorBeh::Powered;

	class FFenceGateBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			return B.State(MCMeta::FromFacing4(LookFacing(C)));
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			uint8 M = Meta(S);
			const bool bOpen = !MCMeta::Bit(M, 2);
			if (bOpen && Player)
			{
				// swing away from the player
				const EMCFace Look = MC::FaceFromYaw(Player->Yaw);
				const EMCFace Cur = MCMeta::Facing4(M);
				if (Look == MC::Opposite(Cur)) M = (M & ~3) | MCMeta::FromFacing4(Look);
			}
			M = MCMeta::SetBit(M, 2, bOpen);
			W.SetState(P, WithMeta(S, M));
			W.PlaySound(bOpen ? TEXT("fence_gate_open") : TEXT("fence_gate_close"), P.Center() * MC::InvBlockSize);
			return true;
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const bool bPowered = W.IsPowered(P);
			const uint8 M = Meta(S);
			if (bPowered == MCMeta::Bit(M, 3)) return;
			uint8 NM = MCMeta::SetBit(M, 3, bPowered);
			if (bPowered != MCMeta::Bit(M, 2)) NM = MCMeta::SetBit(NM, 2, bPowered);
			W.SetState(P, WithMeta(S, NM));
		}
	};

	class FTorchBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World) return B.State(0);
			auto TryFace = [&](EMCFace Clicked) -> FMCState
			{
				if (Clicked == EMCFace::Up)
				{
					const FMCBlockPos Below = C.Pos.Down();
					if (MCBehaviorUtil::HasSolidTop(*C.World, Below)) return B.State(0);
					return 0;
				}
				if (!MC::IsHorizontal(Clicked)) return 0;
				const FMCBlockPos Behind = C.Pos.Offset(MC::Opposite(Clicked));
				if (!MCBehaviorUtil::IsFaceSturdy(*C.World, Behind, Clicked)) return 0;
				return B.State((uint8)Clicked - 1);
			};
			if (const FMCState S = TryFace(C.ClickedFace)) return S;
			if (const FMCState S = TryFace(EMCFace::Up)) return S;
			for (int32 F = 2; F < 6; ++F) if (const FMCState S = TryFace((EMCFace)F)) return S;
			return 0;
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const int32 A = Meta(S) & 7;
			if (A == 0) return MCBehaviorUtil::HasSolidTop(W, P.Down());
			const EMCFace F = (EMCFace)(1 + A);
			return MCBehaviorUtil::IsFaceSturdy(W, P.Offset(MC::Opposite(F)), F);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		virtual void OnRandomTick(FMCWorld& W, const FMCBlockPos& P, FMCState S, FMCRandom& R) const override {}
	};

	class FLadderBeh : public FMCBlockBehavior
	{
	public:
		// ladder meta facing = side the ladder faces (away from the wall); model authored facing North with the quad on the south side
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World) return 0;
			auto Try = [&](EMCFace F) -> FMCState
			{
				if (!MC::IsHorizontal(F)) return 0;
				if (!MCBehaviorUtil::IsFaceSturdy(*C.World, C.Pos.Offset(MC::Opposite(F)), F)) return 0;
				return B.State(MCMeta::FromFacing4(F));
			};
			if (const FMCState S = Try(C.ClickedFace)) return S;
			for (int32 F = 2; F < 6; ++F) if (const FMCState S = Try((EMCFace)F)) return S;
			return 0;
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const EMCFace F = MCMeta::Facing4(Meta(S));
			return MCBehaviorUtil::IsFaceSturdy(W, P.Offset(MC::Opposite(F)), F);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
	};

	/** Levers and buttons: meta bits 0-2 = face pointing away from the support, bit 3 = on. */
	class FAttachedSwitchBeh : public FMCBlockBehavior
	{
	public:
		bool bButton = false;
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World) return 0;
			const EMCFace F = C.ClickedFace;
			if (!MCBehaviorUtil::IsFaceSturdy(*C.World, C.Pos.Offset(MC::Opposite(F)), F)) return 0;
			return B.State((uint8)F);
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const EMCFace F = MCMeta::Facing6(Meta(S));
			return MCBehaviorUtil::IsFaceSturdy(W, P.Offset(MC::Opposite(F)), F);
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
		void Toggle(FMCWorld& W, const FMCBlockPos& P, FMCState S, bool bOn) const
		{
			W.SetState(P, WithMeta(S, MCMeta::SetBit(Meta(S), 3, bOn)), MCSet_Default);
			// update the block we are attached to so that it relays strong power
			const EMCFace F = MCMeta::Facing6(Meta(S));
			W.NotifyNeighbors(P.Offset(MC::Opposite(F)));
			W.PlaySound(bButton ? (bOn ? TEXT("button_click_on") : TEXT("button_click_off")) : TEXT("lever_click"), P.Center() * MC::InvBlockSize, 0.3f, bOn ? 0.6f : 0.5f);
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			const bool bOn = MCMeta::Bit(Meta(S), 3);
			if (bButton)
			{
				if (bOn) return true;
				Toggle(W, P, S, true);
				const bool bWood = FMCBlocks::GetByState(S).Variant == TEXT("wood_button");
				W.ScheduleTick(P, FMCBlocks::BlockOf(S), bWood ? 30 : 20);
			}
			else Toggle(W, P, S, !bOn);
			return true;
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			if (bButton && MCMeta::Bit(Meta(S), 3))
			{
				// wooden buttons stay pressed while an arrow is inside
				TArray<AMCEntity*> Inside;
				W.GetEntitiesInBox(FMCBox(P.X, P.Y, P.Z, P.X + 1, P.Y + 1, P.Z + 1), Inside);
				bool bArrow = false;
				for (AMCEntity* E : Inside) if (E && E->Kind == EMCEntityKind::Projectile) bArrow = true;
				if (bArrow && FMCBlocks::GetByState(S).Variant == TEXT("wood_button")) { W.ScheduleTick(P, FMCBlocks::BlockOf(S), 10); return; }
				Toggle(W, P, S, false);
			}
		}
		virtual void OnRemoved(FMCWorld& W, const FMCBlockPos& P, FMCState Old, FMCState New) const override
		{
			if (MCMeta::Bit(Meta(Old), 3))
			{
				const EMCFace F = MCMeta::Facing6(Meta(Old));
				W.NotifyNeighbors(P.Offset(MC::Opposite(F)));
			}
		}
		virtual bool IsPowerSource(FMCState S) const override { return true; }
		virtual int32 GetWeakPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override { return MCMeta::Bit(Meta(S), 3) ? 15 : 0; }
		virtual int32 GetStrongPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override
		{
			if (!MCMeta::Bit(Meta(S), 3)) return 0;
			return Dir == MC::Opposite(MCMeta::Facing6(Meta(S))) ? 15 : 0;
		}
		virtual bool ConnectsToRedstone(FMCState S, EMCFace F) const override { return true; }
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			if (bButton && E && E->Kind == EMCEntityKind::Projectile && FMCBlocks::GetByState(S).Variant == TEXT("wood_button") && !MCMeta::Bit(Meta(S), 3))
			{
				Toggle(W, P, S, true);
				W.ScheduleTick(P, FMCBlocks::BlockOf(S), 30);
			}
		}
	};

	class FPressurePlateBeh : public FMCBlockBehavior
	{
	public:
		bool bWeighted = false;
		int32 CountEntities(FMCWorld& W, const FMCBlockPos& P, FMCState S) const
		{
			TArray<AMCEntity*> Inside;
			W.GetEntitiesInBox(FMCBox(P.X + 0.0625, P.Y + 0.0625, P.Z, P.X + 0.9375, P.Y + 0.9375, P.Z + 0.25), Inside);
			const FMCBlock& B = FMCBlocks::GetByState(S);
			const bool bMobsOnly = !bWeighted && B.Variant == TEXT("stone_plate");
			int32 N = 0;
			for (AMCEntity* E : Inside)
			{
				if (!E || E->bRemoved) continue;
				if (bMobsOnly && !E->IsLiving()) continue;
				if (E->Kind == EMCEntityKind::Player && static_cast<AMCPlayer*>(E)->IsSpectator()) continue;
				++N;
			}
			return N;
		}
		int32 SignalFor(FMCWorld& W, const FMCBlockPos& P, FMCState S) const
		{
			const int32 N = CountEntities(W, P, S);
			if (!bWeighted) return N > 0 ? 15 : 0;
			const bool bLight = FMCBlocks::GetByState(S).Name == TEXT("light_weighted_pressure_plate");
			return bLight ? FMath::Min(N, 15) : FMath::Min(15, (N + 9) / 10);
		}
		int32 CurrentSignal(FMCState S) const { return bWeighted ? (Meta(S) & 15) : ((Meta(S) & 1) ? 15 : 0); }
		void SetSignal(FMCWorld& W, const FMCBlockPos& P, FMCState S, int32 Sig) const
		{
			const int32 Old = CurrentSignal(S);
			if (Old == Sig) return;
			const uint8 M = bWeighted ? (uint8)Sig : (Sig > 0 ? 1 : 0);
			W.SetState(P, WithMeta(S, M));
			W.NotifyNeighbors(P.Down());
			if ((Old > 0) != (Sig > 0)) W.PlaySound(Sig > 0 ? TEXT("pressure_plate_click_on") : TEXT("pressure_plate_click_off"), P.Center() * MC::InvBlockSize, 0.3f, Sig > 0 ? 0.6f : 0.5f);
		}
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			const int32 Sig = SignalFor(W, P, S);
			if (Sig > CurrentSignal(S)) SetSignal(W, P, S, Sig);
			if (Sig > 0) W.ScheduleTick(P, FMCBlocks::BlockOf(S), bWeighted ? 10 : 20);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const int32 Sig = SignalFor(W, P, S);
			SetSignal(W, P, S, Sig);
			if (Sig > 0) W.ScheduleTick(P, FMCBlocks::BlockOf(S), bWeighted ? 10 : 20);
		}
		virtual bool IsPowerSource(FMCState S) const override { return true; }
		virtual int32 GetWeakPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override { return CurrentSignal(S); }
		virtual int32 GetStrongPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override { return Dir == EMCFace::Down ? CurrentSignal(S) : 0; }
		virtual bool ConnectsToRedstone(FMCState S, EMCFace F) const override { return true; }
		virtual int32 GetComparatorOutput(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override { return CurrentSignal(S); }
	};

	class FLanternBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World) return B.State(0);
			const bool bCanHang = SupportAbove(*C.World, C.Pos);
			const bool bCanStand = MCBehaviorUtil::HasSolidTop(*C.World, C.Pos.Down());
			if (C.ClickedFace == EMCFace::Down && bCanHang) return B.State(1);
			if (bCanStand) return B.State(0);
			if (bCanHang) return B.State(1);
			return 0;
		}
		static bool SupportAbove(const FMCWorld& W, const FMCBlockPos& P)
		{
			const FMCState A = W.GetState(P.Up());
			const FMCBlock& AB = FMCBlocks::GetByState(A);
			return MCBehaviorUtil::IsFaceSturdy(W, P.Up(), EMCFace::Down) || AB.Model == EMCModel::Chain || AB.Model == EMCModel::Fence || AB.Model == EMCModel::Wall
				|| AB.Model == EMCModel::Pane || AB.Model == EMCModel::Bars;
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			return MCMeta::Bit(Meta(S), 0) ? SupportAbove(W, P) : MCBehaviorUtil::HasSolidTop(W, P.Down());
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			if (!CanSurvive(FMCBlocks::GetByState(S), W, P, S)) BreakUnsupported(W, P);
		}
	};

	class FChainBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			uint8 A = 0;
			switch (C.ClickedFace) { case EMCFace::West: case EMCFace::East: A = 1; break; case EMCFace::North: case EMCFace::South: A = 2; break; default: A = 0; break; }
			return B.State(A);
		}
	};

	class FCarpetBeh : public FMCBlockBehavior
	{
	public:
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCState Below = W.GetState(P.Down());
			return Below != 0 && !FMCBlocks::IsFluid(Below);
		}
	};

	/** Heads & skulls: floor rotation 0-15, wall variant bit 4 + facing. Wither skeleton skulls may summon the Wither. */
	class FSkullBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (MC::IsHorizontal(C.ClickedFace)) return B.State(16 | MCMeta::FromFacing4(C.ClickedFace));
			// yaw -> 16 steps (0 = facing south towards the player in Minecraft; we store the look yaw)
			const int32 Rot = FMath::RoundToInt(FMath::Fmod(C.Yaw + 360.f + 180.f, 360.f) / 22.5f) & 15;
			return B.State((uint8)Rot);
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override
		{
			if (FMCBlocks::GetByState(S).Name == TEXT("wither_skeleton_skull")) AMCWither::TrySpawnFromStructure(W, P);
		}
	};

	class FPumpkinBeh : public FMCBlockBehavior
	{
	public:
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (B.Orient == EMCCubeOrient::Facing4) return B.State(MCMeta::FromFacing4(C.FacingTowardsPlayer()));
			return B.BaseState;
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.Name != TEXT("carved_pumpkin") && B.Name != TEXT("jack_o_lantern")) return;
			const FMCState Snow = MCBeh::StateOf(TEXT("snow_block")), Iron = MCBeh::StateOf(TEXT("iron_block"));
			// snow golem: two snow blocks below
			if (W.GetState(P.Down()) == Snow && W.GetState(P.Down(2)) == Snow)
			{
				W.SetState(P, 0); W.SetState(P.Down(), 0); W.SetState(P.Down(2), 0);
				W.SpawnMob(TEXT("snow_golem"), FVector(P.X + 0.5, P.Y + 0.5, P.Z - 2));
				W.SpawnParticles(TEXT("snowflake"), P.Center() * MC::InvBlockSize, 30, 0.8f);
				return;
			}
			// iron golem: T shape of iron blocks
			if (W.GetState(P.Down()) == Iron && W.GetState(P.Down(2)) == Iron)
			{
				for (int32 Axis = 0; Axis < 2; ++Axis)
				{
					const FMCBlockPos A = Axis == 0 ? FMCBlockPos(P.X - 1, P.Y, P.Z - 1) : FMCBlockPos(P.X, P.Y - 1, P.Z - 1);
					const FMCBlockPos Bp = Axis == 0 ? FMCBlockPos(P.X + 1, P.Y, P.Z - 1) : FMCBlockPos(P.X, P.Y + 1, P.Z - 1);
					if (W.GetState(A) == Iron && W.GetState(Bp) == Iron)
					{
						W.SetState(P, 0); W.SetState(P.Down(), 0); W.SetState(P.Down(2), 0); W.SetState(A, 0); W.SetState(Bp, 0);
						AMCEntity* G = W.SpawnMob(TEXT("iron_golem"), FVector(P.X + 0.5, P.Y + 0.5, P.Z - 2));
						if (AMCMob* M = Cast<AMCMob>(G)) M->bPersistent = true;
						return;
					}
				}
			}
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.Name == TEXT("pumpkin") && MCBeh::HeldIs(Player, TEXT("shears")))
			{
				const EMCFace F = MC::IsHorizontal(Face) ? Face : MC::Opposite(MC::FaceFromYaw(Player->Yaw));
				W.SetState(P, MCBeh::StateOf(TEXT("carved_pumpkin"), MCMeta::FromFacing4(F)));
				MCBehaviorUtil::PopItem(W, P.Offset(F), FMCItemStack::Of(TEXT("pumpkin_seeds"), 4));
				Player->DamageHeld(1);
				W.PlaySound(TEXT("pumpkin_carve"), P.Center() * MC::InvBlockSize);
				return true;
			}
			return false;
		}
	};

	/** Rails: shape follows neighbouring rails (straight, slopes, curves). */
	class FRailBeh : public FMCBlockBehavior
	{
	public:
		static bool IsRail(FMCState S) { return FMCBlocks::GetByState(S).Model == EMCModel::Rail; }
		static bool IsSimple(const FMCBlock& B) { return B.Name == TEXT("rail"); }
		static bool RailAt(const FMCWorld& W, const FMCBlockPos& P, int32& OutDZ)
		{
			for (int32 dz : { 0, 1, -1 }) if (IsRail(W.GetState(P.Up(dz)))) { OutDZ = dz; return true; }
			return false;
		}
		int32 ComputeShape(const FMCWorld& W, const FMCBlockPos& P, bool bSimple, int32 Current) const
		{
			int32 DZn = 0, DZs = 0, DZw = 0, DZe = 0;
			const bool N = RailAt(W, P.Offset(EMCFace::North), DZn), S = RailAt(W, P.Offset(EMCFace::South), DZs);
			const bool Wt = RailAt(W, P.Offset(EMCFace::West), DZw), E = RailAt(W, P.Offset(EMCFace::East), DZe);
			const bool bNS = N || S, bWE = Wt || E;
			if (bSimple)
			{
				if (S && E && !N && !Wt) return 6;
				if (S && Wt && !N && !E) return 7;
				if (N && Wt && !S && !E) return 8;
				if (N && E && !S && !Wt) return 9;
			}
			if (bNS && !bWE)
			{
				if (N && DZn == 1) return 4;   // ascending north
				if (S && DZs == 1) return 5;   // ascending south
				return 0;
			}
			if (bWE && !bNS)
			{
				if (E && DZe == 1) return 2;
				if (Wt && DZw == 1) return 3;
				return 1;
			}
			if (bNS && bWE) return Current <= 1 ? Current : 0;
			return Current <= 5 ? Current : 0;
		}
		virtual FMCState GetPlacementState(const FMCBlock& B, const FMCPlaceContext& C) const override
		{
			if (!C.World || !MCBehaviorUtil::HasSolidTop(*C.World, C.Pos.Down())) return 0;
			const EMCFace Look = LookFacing(C);
			const int32 Default = (Look == EMCFace::West || Look == EMCFace::East) ? 1 : 0;
			const int32 Shape = ComputeShape(*C.World, C.Pos, IsSimple(B), Default);
			return B.State((uint8)Shape);
		}
		virtual void OnPlaced(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player) const override
		{
			// neighbours re-evaluate their shape to connect to us
			for (int32 F = 2; F < 6; ++F)
				for (int32 dz : { 0, 1, -1 })
				{
					const FMCBlockPos Q = P.Offset((EMCFace)F).Up(dz);
					const FMCState QS = W.GetState(Q);
					if (!IsRail(QS)) continue;
					const FMCBlock& QB = FMCBlocks::GetByState(QS);
					const int32 Mask = IsSimple(QB) ? 15 : 7;
					const int32 Cur = Meta(QS) & Mask;
					const int32 NewShape = ComputeShape(W, Q, IsSimple(QB), Cur);
					if (NewShape != Cur) W.SetState(Q, QB.State((uint8)((Meta(QS) & ~Mask) | NewShape)));
				}
		}
		virtual bool CanSurvive(const FMCBlock& B, FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			return MCBehaviorUtil::HasSolidTop(W, P.Down());
		}
		virtual void OnNeighborChanged(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCBlockPos& From) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (!CanSurvive(B, W, P, S)) { BreakUnsupported(W, P); return; }
			if (IsSimple(B)) return;
			if (B.Name == TEXT("detector_rail")) return;
			// powered / activator rails: powered directly or through up to 8 connected rails
			const bool bPowered = W.IsPowered(P) || W.IsPowered(P.Down()) || PoweredViaChain(W, P, B.Id, 8);
			const uint8 M = Meta(S);
			if (bPowered != MCMeta::Bit(M, 3))
			{
				W.SetState(P, B.State(MCMeta::SetBit(M, 3, bPowered)));
				for (int32 F = 2; F < 6; ++F) W.UpdateNeighbor(P.Offset((EMCFace)F), P);
			}
		}
		bool PoweredViaChain(FMCWorld& W, const FMCBlockPos& P, FMCBlockId Id, int32 Depth) const
		{
			const FMCState S = W.GetState(P);
			const int32 Shape = Meta(S) & 7;
			const bool bNS = Shape == 0 || Shape == 4 || Shape == 5;
			const EMCFace Dirs[2] = { bNS ? EMCFace::North : EMCFace::West, bNS ? EMCFace::South : EMCFace::East };
			for (EMCFace D : Dirs)
			{
				FMCBlockPos Q = P;
				for (int32 i = 0; i < Depth; ++i)
				{
					Q = Q.Offset(D);
					FMCState QS = W.GetState(Q);
					if (FMCBlocks::BlockOf(QS) != Id) { QS = W.GetState(Q.Up()); if (FMCBlocks::BlockOf(QS) == Id) Q = Q.Up(); else { QS = W.GetState(Q.Down()); if (FMCBlocks::BlockOf(QS) == Id) Q = Q.Down(); else break; } }
					if (W.IsPowered(Q)) return true;
				}
			}
			return false;
		}
		virtual bool IsPowerSource(FMCState S) const override { return FMCBlocks::GetByState(S).Name == TEXT("detector_rail"); }
		virtual int32 GetWeakPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override
		{
			return (FMCBlocks::GetByState(S).Name == TEXT("detector_rail") && MCMeta::Bit(Meta(S), 3)) ? 15 : 0;
		}
		virtual int32 GetStrongPower(FMCWorld& W, const FMCBlockPos& P, FMCState S, EMCFace Dir) const override
		{
			return Dir == EMCFace::Down ? GetWeakPower(W, P, S, Dir) : 0;
		}
		virtual void OnEntityInside(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCEntity* E) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.Name != TEXT("detector_rail") || !E || E->Kind != EMCEntityKind::Minecart) return;
			if (!MCMeta::Bit(Meta(S), 3))
			{
				W.SetState(P, B.State(MCMeta::SetBit(Meta(S), 3, true)));
				W.NotifyNeighbors(P.Down());
			}
			W.ScheduleTick(P, B.Id, 20);
		}
		virtual void OnScheduledTick(FMCWorld& W, const FMCBlockPos& P, FMCState S) const override
		{
			const FMCBlock& B = FMCBlocks::GetByState(S);
			if (B.Name != TEXT("detector_rail")) return;
			TArray<AMCEntity*> Inside;
			W.GetEntitiesInBox(FMCBox(P.X + 0.1, P.Y + 0.1, P.Z, P.X + 0.9, P.Y + 0.9, P.Z + 0.9), Inside);
			bool bCart = false;
			for (AMCEntity* E : Inside) if (E && E->Kind == EMCEntityKind::Minecart) bCart = true;
			if (bCart) { W.ScheduleTick(P, B.Id, 20); return; }
			if (MCMeta::Bit(Meta(S), 3)) { W.SetState(P, B.State(MCMeta::SetBit(Meta(S), 3, false))); W.NotifyNeighbors(P.Down()); }
		}
	};

	class FFlowerPotBeh : public FMCBlockBehavior
	{
	public:
		static int32 IndexFor(FName Item)
		{
			static const TCHAR* Plants[] = { nullptr, TEXT("poppy"), TEXT("dandelion"), TEXT("blue_orchid"), TEXT("allium"), TEXT("azure_bluet"),
				TEXT("red_tulip"), TEXT("orange_tulip"), TEXT("white_tulip"), TEXT("pink_tulip"), TEXT("oxeye_daisy"), TEXT("cornflower"),
				TEXT("lily_of_the_valley"), TEXT("wither_rose"), TEXT("oak_sapling"), TEXT("spruce_sapling"), TEXT("birch_sapling"),
				TEXT("jungle_sapling"), TEXT("acacia_sapling"), TEXT("dark_oak_sapling"), TEXT("cherry_sapling"), TEXT("fern"),
				TEXT("dead_bush"), TEXT("red_mushroom"), TEXT("brown_mushroom"), TEXT("bamboo"), TEXT("crimson_fungus"),
				TEXT("warped_fungus"), TEXT("torchflower"), TEXT("pale_oak_sapling"), TEXT("azalea"), TEXT("mangrove_propagule") };
			for (int32 i = 1; i < UE_ARRAY_COUNT(Plants); ++i) if (Item == FName(Plants[i])) return i;
			return 0;
		}
		static FName ItemFor(int32 Index)
		{
			static const TCHAR* Plants[] = { nullptr, TEXT("poppy"), TEXT("dandelion"), TEXT("blue_orchid"), TEXT("allium"), TEXT("azure_bluet"),
				TEXT("red_tulip"), TEXT("orange_tulip"), TEXT("white_tulip"), TEXT("pink_tulip"), TEXT("oxeye_daisy"), TEXT("cornflower"),
				TEXT("lily_of_the_valley"), TEXT("wither_rose"), TEXT("oak_sapling"), TEXT("spruce_sapling"), TEXT("birch_sapling"),
				TEXT("jungle_sapling"), TEXT("acacia_sapling"), TEXT("dark_oak_sapling"), TEXT("cherry_sapling"), TEXT("fern"),
				TEXT("dead_bush"), TEXT("red_mushroom"), TEXT("brown_mushroom"), TEXT("bamboo"), TEXT("crimson_fungus"),
				TEXT("warped_fungus"), TEXT("torchflower"), TEXT("pale_oak_sapling"), TEXT("azalea"), TEXT("mangrove_propagule") };
			return (Index > 0 && Index < UE_ARRAY_COUNT(Plants)) ? FName(Plants[Index]) : NAME_None;
		}
		virtual bool OnUse(FMCWorld& W, const FMCBlockPos& P, FMCState S, AMCPlayer* Player, EMCFace Face, const FVector& Hit) const override
		{
			if (!Player) return false;
			const int32 Cur = Meta(S) & 31;
			if (Cur == 0)
			{
				const FMCItem* I = MCBeh::HeldItem(Player);
				const int32 Idx = I ? IndexFor(I->Name) : 0;
				if (Idx == 0) return false;
				W.SetState(P, WithMeta(S, (uint8)Idx));
				if (!Player->IsCreative()) Player->ConsumeHeld(1);
				return true;
			}
			Player->GiveItem(FMCItemStack::Of(ItemFor(Cur), 1));
			W.SetState(P, WithMeta(S, 0));
			return true;
		}
		virtual bool GetDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R) const override
		{
			Out.Add(FMCItemStack::Of(TEXT("flower_pot"), 1));
			const int32 Cur = Meta(S) & 31;
			if (Cur) Out.Add(FMCItemStack::Of(ItemFor(Cur), 1));
			return true;
		}
	};
}

void MCBeh::RegisterPlacement()
{
	Register(EMCBeh::Axis, new FAxisBeh());
	Register(EMCBeh::FacingToPlayer, new FFacingToPlayerBeh());
	Register(EMCBeh::FacingAwayFromPlayer, new FFacingAwayBeh());
	Register(EMCBeh::Facing6ToPlayer, new FFacing6ToPlayerBeh());
	Register(EMCBeh::Facing6Away, new FFacing6AwayBeh());
	Register(EMCBeh::Slab, new FSlabBeh());
	Register(EMCBeh::Stairs, new FStairsBeh());
	Register(EMCBeh::Door, new FDoorBeh());
	Register(EMCBeh::Trapdoor, new FTrapdoorBeh());
	Register(EMCBeh::FenceGate, new FFenceGateBeh());
	Register(EMCBeh::Torch, new FTorchBeh());
	Register(EMCBeh::Ladder, new FLadderBeh());
	FAttachedSwitchBeh* Lever = new FAttachedSwitchBeh(); Lever->bButton = false;
	FAttachedSwitchBeh* Button = new FAttachedSwitchBeh(); Button->bButton = true;
	Register(EMCBeh::Lever, Lever);
	Register(EMCBeh::Button, Button);
	FPressurePlateBeh* Plate = new FPressurePlateBeh(); Plate->bWeighted = false;
	FPressurePlateBeh* WPlate = new FPressurePlateBeh(); WPlate->bWeighted = true;
	Register(EMCBeh::PressurePlate, Plate);
	Register(EMCBeh::WeightedPressurePlate, WPlate);
	Register(EMCBeh::Lantern, new FLanternBeh());
	Register(EMCBeh::Chain, new FChainBeh());
	Register(EMCBeh::EndRod, new FFacing6AwayBeh());
	Register(EMCBeh::Carpet, new FCarpetBeh());
	Register(EMCBeh::Skull, new FSkullBeh());
	Register(EMCBeh::Pumpkin, new FPumpkinBeh());
	Register(EMCBeh::Rail, new FRailBeh());
	Register(EMCBeh::FlowerPot, new FFlowerPotBeh());
}
