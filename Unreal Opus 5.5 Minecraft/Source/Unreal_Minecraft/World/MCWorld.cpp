#include "World/MCWorld.h"
#include "World/MCBlockEntity.h"
#include "World/MCLighting.h"
#include "Gen/MCWorldGen.h"
#include "Blocks/MCBlockBehavior.h"
#include "Items/MCItems.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Misc/Compression.h"
#include "Tasks/Task.h"

struct FMCLightResult
{
	FMCChunkPos Pos;
	uint32 Revision = 0;
	TArray<uint8> Light; // NumSections * 4096 packed (sky << 4 | block)
};

struct FMCWorldAsync
{
	TQueue<TSharedPtr<FMCChunk>, EQueueMode::Mpsc> GenQueue;
	TQueue<TSharedPtr<FMCLightResult>, EQueueMode::Mpsc> LightQueue;
	FThreadSafeCounter PendingTasks;
};

FMCWorld::FMCWorld(EMCDimension InDim, uint64 InSeed, AMCGame* InGame)
	: Dim(InDim), Seed(InSeed), Game(InGame)
{
	Generator = FMCWorldGenerator::Create(InDim, InSeed);
	Async = MakeShared<FMCWorldAsync>();
	Rand.SetSeed(InSeed ^ ((uint64)InDim * 0x9E3779B97F4A7C15ull));
}

FMCWorld::~FMCWorld()
{
	// wait for in-flight worker tasks that write into our async state (they hold their own reference)
	const double Start = FPlatformTime::Seconds();
	while (Async.IsValid() && Async->PendingTasks.GetValue() > 0 && FPlatformTime::Seconds() - Start < 10.0)
	{
		FPlatformProcess::Sleep(0.001f);
	}
}

bool FMCWorld::IsReady(const FMCChunkPos& C) const
{
	const FMCChunk* Ch = GetChunk(C);
	return Ch && Ch->Stage == EMCChunkStage::Lit;
}

// ---------------------------------------------------------------------------------------------------------------------
// Block modification

bool FMCWorld::SetState(const FMCBlockPos& P, FMCState S, uint32 Flags)
{
	if (!FMCChunk::InRange(P.Z)) return false;
	FMCChunk* C = GetChunkAt(P);
	if (!C) return false;
	const int32 LX = P.X & 15, LY = P.Y & 15;
	const FMCState Old = C->Get(LX, LY, P.Z);
	if (Old == S) return false;

	const FMCStateInfo& OI = FMCBlocks::Info(Old);
	const FMCStateInfo& NI = FMCBlocks::Info(S);
	const bool bBlockChanged = OI.Block != NI.Block;

	C->SetRaw(LX, LY, P.Z, S);
	C->UpdateHeightmapAt(LX, LY, P.Z, S);
	C->bModified = true;
	++C->EditCounter;

	// block entity lifetime
	if (bBlockChanged && !(Flags & MCSet_KeepEntity))
	{
		const int32 Key = FMCChunk::LocalIndex(LX, LY, P.Z);
		if (TSharedPtr<FMCBlockEntity>* BE = C->BlockEntities.Find(Key))
		{
			C->BlockEntities.Remove(Key);
		}
	}

	if ((Flags & MCSet_Light) && (OI.Opacity != NI.Opacity || OI.Light != NI.Light || ((OI.Flags ^ NI.Flags) & MCB_Leaves)))
	{
		if (C->Stage == EMCChunkStage::Lit) UpdateLightAt(P, Old, S);
	}
	if (Flags & MCSet_Render) MarkBlockDirty(P);

	if (bBlockChanged)
	{
		const FMCBlock& OB = FMCBlocks::Get(OI.Block);
		OB.Behavior->OnRemoved(*this, P, Old, S);
		const FMCBlock& NB = FMCBlocks::Get(NI.Block);
		if ((NB.Flags & MCB_BlockEntity) && !GetBlockEntity(P))
		{
			TSharedPtr<FMCBlockEntity> BE = NB.Behavior->CreateBlockEntity(P, S);
			if (BE) SetBlockEntity(P, BE);
		}
		if (NI.Flags & MCB_Portal) AddPortalPOI(P);
	}

	if (Flags & MCSet_Neighbors)
	{
		// the block itself gets a "placed/changed" callback (From == P)
		if (S != 0) FMCBlocks::Get(NI.Block).Behavior->OnNeighborChanged(*this, P, S, P);
		NotifyNeighbors(P);
	}
	return true;
}

bool FMCWorld::PlaceBlock(const FMCBlockPos& P, FMCState S, AMCPlayer* Player)
{
	if (!SetState(P, S)) return false;
	const FMCBlock& B = FMCBlocks::GetByState(S);
	B.Behavior->OnPlaced(*this, P, S, Player);
	PlayBlockSound(S, 1, P.Center() * MC::InvBlockSize);
	return true;
}

void FMCWorld::MarkModified(const FMCBlockPos& P)
{
	if (FMCChunk* C = GetChunkAt(P)) C->bModified = true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Queries

uint8 FMCWorld::GetSkyLight(const FMCBlockPos& P) const
{
	const FMCChunk* C = GetChunkAt(P);
	if (!C) return Dim == EMCDimension::Overworld ? 15 : 0;
	return C->GetSky(P.X & 15, P.Y & 15, P.Z);
}

uint8 FMCWorld::GetBlockLight(const FMCBlockPos& P) const
{
	const FMCChunk* C = GetChunkAt(P);
	return C ? C->GetBlockLight(P.X & 15, P.Y & 15, P.Z) : 0;
}

int32 FMCWorld::GetLight(const FMCBlockPos& P, int32 SkyDarken) const
{
	const int32 Sky = (int32)GetSkyLight(P) - FMath::Max(0, SkyDarken);
	return FMath::Max(Sky, (int32)GetBlockLight(P));
}

int32 FMCWorld::GetHeight(int32 X, int32 Y) const
{
	const FMCChunk* C = GetChunk(FMCChunkPos::FromBlock(X, Y));
	return C ? C->GetHeight(X & 15, Y & 15) : MC::MinZ - 1;
}

int32 FMCWorld::GetTopSolidZ(int32 X, int32 Y) const
{
	const FMCChunk* C = GetChunk(FMCChunkPos::FromBlock(X, Y));
	return C ? C->GetMotionHeight(X & 15, Y & 15) : MC::MinZ - 1;
}

uint8 FMCWorld::GetBiome(const FMCBlockPos& P) const
{
	const FMCChunk* C = GetChunkAt(P);
	if (C) return C->GetBiome(P.X & 15, P.Y & 15, FMath::Clamp(P.Z, MC::MinZ, MC::MaxZ));
	return Generator.IsValid() ? Generator->GetBiomeAt(P.X, P.Y, P.Z) : 0;
}

FMCBlockEntity* FMCWorld::GetBlockEntity(const FMCBlockPos& P) const
{
	const FMCChunk* C = GetChunkAt(P);
	return C ? C->GetBlockEntity(P.X & 15, P.Y & 15, P.Z) : nullptr;
}

TSharedPtr<FMCBlockEntity> FMCWorld::GetBlockEntityShared(const FMCBlockPos& P) const
{
	const FMCChunk* C = GetChunkAt(P);
	if (!C) return nullptr;
	const TSharedPtr<FMCBlockEntity>* BE = C->BlockEntities.Find(FMCChunk::LocalIndex(P.X & 15, P.Y & 15, P.Z));
	return BE ? *BE : nullptr;
}

void FMCWorld::SetBlockEntity(const FMCBlockPos& P, TSharedPtr<FMCBlockEntity> BE)
{
	FMCChunk* C = GetChunkAt(P);
	if (!C || !BE) return;
	BE->Pos = P;
	C->BlockEntities.Add(FMCChunk::LocalIndex(P.X & 15, P.Y & 15, P.Z), BE);
	C->bModified = true;
	if (BE->Ticks()) TickingBlockEntities.Add(BE);
}

// ---------------------------------------------------------------------------------------------------------------------
// Neighbour updates and scheduled ticks

void FMCWorld::NotifyNeighbors(const FMCBlockPos& P)
{
	if (NeighborUpdateDepth > 256) return; // runaway protection (redstone loops)
	++NeighborUpdateDepth;
	for (int32 F = 0; F < 6; ++F) UpdateNeighbor(P.Offset((EMCFace)F), P);
	--NeighborUpdateDepth;
}

void FMCWorld::UpdateNeighbor(const FMCBlockPos& P, const FMCBlockPos& From)
{
	const FMCState S = GetState(P);
	if (S == 0) return;
	const FMCBlock& B = FMCBlocks::GetByState(S);
	// plants rest on the block below or hang from the one above: a change beside them (water spreading, grass
	// growing, a neighbouring plant) cannot take their support away. Re-checking them on side updates popped
	// generated vegetation off in cascades. Vines, glow lichen and cocoa attach sideways and keep every update.
	if (B.Has(MCB_Plant) && From.Z == P.Z && B.Model != EMCModel::Vine && B.Model != EMCModel::GlowLichen && B.Name != TEXT("cocoa")) return;
	B.Behavior->OnNeighborChanged(*this, P, S, From);
}

uint64 FMCWorld::TickKey(const FMCBlockPos& P, FMCBlockId B)
{
	return MCHash::Hash3((uint64)B, P.X, P.Y, P.Z);
}

void FMCWorld::ScheduleTick(const FMCBlockPos& P, FMCBlockId Block, int32 DelayTicks, int32 Priority)
{
	const uint64 Key = TickKey(P, Block);
	if (TickKeys.Contains(Key)) return;
	TickKeys.Add(Key);
	FMCScheduledTick T;
	T.Pos = P; T.Block = Block; T.DueTick = GameTick + FMath::Max(1, DelayTicks); T.Priority = Priority;
	TickQueue.HeapPush(T, [](const FMCScheduledTick& A, const FMCScheduledTick& B) { return A.DueTick != B.DueTick ? A.DueTick < B.DueTick : A.Priority < B.Priority; });
}

bool FMCWorld::HasScheduledTick(const FMCBlockPos& P, FMCBlockId Block) const
{
	return TickKeys.Contains(TickKey(P, Block));
}

void FMCWorld::ProcessScheduledTicks()
{
	auto Pred = [](const FMCScheduledTick& A, const FMCScheduledTick& B) { return A.DueTick != B.DueTick ? A.DueTick < B.DueTick : A.Priority < B.Priority; };
	int32 Budget = 8192;
	while (TickQueue.Num() > 0 && TickQueue.HeapTop().DueTick <= GameTick && Budget-- > 0)
	{
		FMCScheduledTick T;
		TickQueue.HeapPop(T, Pred, EAllowShrinking::No);
		TickKeys.Remove(TickKey(T.Pos, T.Block));
		if (!IsReadyAt(T.Pos))
		{
			continue;
		}
		const FMCState S = GetState(T.Pos);
		if (FMCBlocks::BlockOf(S) != T.Block) continue;
		FMCBlocks::Get(T.Block).Behavior->OnScheduledTick(*this, T.Pos, S);
	}
}

void FMCWorld::Tick(const FVector& PlayerPosBlocks)
{
	++GameTick;
	ProcessScheduledTicks();
	TickRandomBlocks(PlayerPosBlocks);
	TickBlockEntities();
}

void FMCWorld::TickRandomBlocks(const FVector& PlayerPosBlocks)
{
	if (RandomTickSpeed <= 0) return;
	const FMCChunkPos PC = FMCChunkPos::FromBlock(MC::FloorToInt(PlayerPosBlocks.X), MC::FloorToInt(PlayerPosBlocks.Y));
	constexpr int32 SimRadius = 8;
	for (int32 dy = -SimRadius; dy <= SimRadius; ++dy)
	{
		for (int32 dx = -SimRadius; dx <= SimRadius; ++dx)
		{
			FMCChunk* C = GetChunk(FMCChunkPos(PC.X + dx, PC.Y + dy));
			if (!C || C->Stage != EMCChunkStage::Lit) continue;
			++C->InhabitedTicks;
			for (int32 SI = 0; SI < MC::NumSections; ++SI)
			{
				FMCSection* Sec = C->Sections[SI].Get();
				if (!Sec || Sec->Ticking <= 0) continue;
				for (int32 k = 0; k < RandomTickSpeed; ++k)
				{
					const int32 Idx = Rand.NextInt(MC::SectionVolume);
					const FMCState S = Sec->States[Idx];
					if (S == 0) continue;
					const FMCStateInfo& I = FMCBlocks::Info(S);
					if (!(I.Flags & MCB_RandomTick)) continue;
					const FMCBlockPos P(C->Pos.MinBlockX() + (Idx & 15), C->Pos.MinBlockY() + ((Idx >> 4) & 15), MC::MinZ + SI * 16 + (Idx >> 8));
					FMCBlocks::Get(I.Block).Behavior->OnRandomTick(*this, P, S, Rand);
				}
			}
		}
	}
}

void FMCWorld::TickBlockEntities()
{
	for (int32 i = TickingBlockEntities.Num() - 1; i >= 0; --i)
	{
		TSharedPtr<FMCBlockEntity> BE = TickingBlockEntities[i].Pin();
		if (!BE.IsValid() || GetBlockEntity(BE->Pos) != BE.Get())
		{
			TickingBlockEntities.RemoveAtSwap(i, EAllowShrinking::No);
			continue;
		}
		if (!IsReadyAt(BE->Pos)) continue;
		BE->Tick(*this);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Redstone

int32 FMCWorld::GetWeakPowerFrom(const FMCBlockPos& From, EMCFace Dir) const
{
	const FMCState S = GetState(From);
	if (S == 0) return 0;
	const FMCStateInfo& I = FMCBlocks::Info(S);
	if (!(I.Flags & MCB_Redstone)) return 0;
	return FMCBlocks::Get(I.Block).Behavior->GetWeakPower(const_cast<FMCWorld&>(*this), From, S, Dir);
}

int32 FMCWorld::GetStrongPowerInto(const FMCBlockPos& P) const
{
	int32 Best = 0;
	for (int32 F = 0; F < 6; ++F)
	{
		const EMCFace Dir = (EMCFace)F;
		const FMCBlockPos N = P.Offset(Dir);
		const FMCState S = GetState(N);
		if (S == 0) continue;
		const FMCStateInfo& I = FMCBlocks::Info(S);
		if (!(I.Flags & MCB_Redstone)) continue;
		// neighbour at N emits towards -Dir (towards P)
		Best = FMath::Max(Best, FMCBlocks::Get(I.Block).Behavior->GetStrongPower(const_cast<FMCWorld&>(*this), N, S, MC::Opposite(Dir)));
		if (Best >= 15) break;
	}
	return Best;
}

int32 FMCWorld::GetInputPower(const FMCBlockPos& P, EMCFace Dir) const
{
	const FMCBlockPos N = P.Offset(Dir);
	const FMCState S = GetState(N);
	if (S == 0) return 0;
	const FMCStateInfo& I = FMCBlocks::Info(S);
	int32 Own = 0;
	if (I.Flags & MCB_Redstone) Own = FMCBlocks::Get(I.Block).Behavior->GetWeakPower(const_cast<FMCWorld&>(*this), N, S, MC::Opposite(Dir));
	if ((I.Flags & MCB_Opaque) && !(I.Flags & MCB_Redstone)) return FMath::Max(Own, GetStrongPowerInto(N));
	return Own;
}

int32 FMCWorld::GetBestNeighborPower(const FMCBlockPos& P) const
{
	int32 Best = 0;
	for (int32 F = 0; F < 6 && Best < 15; ++F) Best = FMath::Max(Best, GetInputPower(P, (EMCFace)F));
	return Best;
}

// ---------------------------------------------------------------------------------------------------------------------
// Physics helpers

void FMCWorld::GetCollisionBoxes(const FMCBox& Region, TArray<FMCBox>& Out, bool bIncludeFluids) const
{
	const int32 X0 = MC::FloorToInt(Region.Min.X) - 1, X1 = MC::FloorToInt(Region.Max.X);
	const int32 Y0 = MC::FloorToInt(Region.Min.Y) - 1, Y1 = MC::FloorToInt(Region.Max.Y);
	const int32 Z0 = MC::FloorToInt(Region.Min.Z) - 1, Z1 = MC::FloorToInt(Region.Max.Z);
	TArray<FMCBox, TInlineAllocator<8>> Local;
	for (int32 X = X0; X <= X1; ++X)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			const FMCChunk* C = GetChunk(FMCChunkPos::FromBlock(X, Y));
			for (int32 Z = Z0; Z <= Z1; ++Z)
			{
				if (!C || C->Stage != EMCChunkStage::Lit)
				{
					// unloaded terrain is solid so entities never fall out of the world while chunks stream
					if (Z >= MC::MinZ && Z <= MC::MaxZ) { const FMCBox B(X, Y, Z, X + 1, Y + 1, Z + 1); if (B.Intersects(Region)) Out.Add(B); }
					continue;
				}
				const FMCState S = C->Get(X & 15, Y & 15, Z);
				if (S == 0) continue;
				const FMCStateInfo& I = FMCBlocks::Info(S);
				if (I.bNoCollision)
				{
					if (bIncludeFluids && (I.Flags & MCB_Fluid)) { const FMCBox B(X, Y, Z, X + 1, Y + 1, Z + 1); if (B.Intersects(Region)) Out.Add(B); }
					continue;
				}
				if (I.bFullCollision)
				{
					const FMCBox B(X, Y, Z, X + 1, Y + 1, Z + 1);
					if (B.Intersects(Region)) Out.Add(B);
					continue;
				}
				Local.Reset();
				TArray<FMCBox> Tmp;
				FMCBlocks::GetCollision(S, this, FMCBlockPos(X, Y, Z), Tmp);
				for (const FMCBox& LB : Tmp)
				{
					const FMCBox B = LB.Offset(X, Y, Z);
					if (B.Intersects(Region)) Out.Add(B);
				}
			}
		}
	}
	// bottom of the world: an invisible floor far below bedrock only for safety (void kills entities)
}

bool FMCWorld::IsRegionFree(const FMCBox& Region) const
{
	TArray<FMCBox> Boxes;
	GetCollisionBoxes(Region, Boxes);
	for (const FMCBox& B : Boxes) if (B.Intersects(Region)) return false;
	return true;
}

bool FMCWorld::Raycast(const FVector& Start, const FVector& DirIn, double MaxDist, FMCRayHit& Out, bool bFluids, bool bOutlineShapes) const
{
	Out = FMCRayHit();
	const FVector Dir = DirIn.GetSafeNormal();
	if (Dir.IsNearlyZero()) return false;
	int32 X = MC::FloorToInt(Start.X), Y = MC::FloorToInt(Start.Y), Z = MC::FloorToInt(Start.Z);
	const int32 StepX = Dir.X > 0 ? 1 : -1, StepY = Dir.Y > 0 ? 1 : -1, StepZ = Dir.Z > 0 ? 1 : -1;
	const double TDX = FMath::Abs(Dir.X) > 1e-12 ? FMath::Abs(1.0 / Dir.X) : 1e30;
	const double TDY = FMath::Abs(Dir.Y) > 1e-12 ? FMath::Abs(1.0 / Dir.Y) : 1e30;
	const double TDZ = FMath::Abs(Dir.Z) > 1e-12 ? FMath::Abs(1.0 / Dir.Z) : 1e30;
	double TMX = FMath::Abs(Dir.X) > 1e-12 ? ((StepX > 0 ? (X + 1 - Start.X) : (Start.X - X)) * TDX) : 1e30;
	double TMY = FMath::Abs(Dir.Y) > 1e-12 ? ((StepY > 0 ? (Y + 1 - Start.Y) : (Start.Y - Y)) * TDY) : 1e30;
	double TMZ = FMath::Abs(Dir.Z) > 1e-12 ? ((StepZ > 0 ? (Z + 1 - Start.Z) : (Start.Z - Z)) * TDZ) : 1e30;
	TArray<FMCBox> Boxes;
	double T = 0.0;
	EMCFace EnterFace = EMCFace::Up;
	for (int32 Steps = 0; Steps < 512 && T <= MaxDist; ++Steps)
	{
		const FMCBlockPos P(X, Y, Z);
		const FMCState S = GetState(P);
		if (S != 0)
		{
			const FMCStateInfo& I = FMCBlocks::Info(S);
			const bool bFluid = (I.Flags & MCB_Fluid) != 0;
			if (!bFluid || bFluids)
			{
				Boxes.Reset();
				if (bFluid) Boxes.Add(FMCBox(0, 0, 0, 1, 1, FMath::Max(0.1f, GetFluidHeight(P))));
				else if (I.Shape == EMCShape::Cube || I.Shape == EMCShape::Invisible) Boxes.Add(FMCBox(0, 0, 0, 1, 1, 1));
				else if (bOutlineShapes) FMCBlocks::GetOutline(S, this, P, Boxes);
				else FMCBlocks::GetCollision(S, this, P, Boxes);
				double BestT = 1e30; EMCFace BestF = EnterFace;
				const FVector LocalStart = Start - FVector(X, Y, Z);
				for (const FMCBox& B : Boxes)
				{
					double HT; EMCFace HF;
					if (B.RayHit(LocalStart, Dir, MaxDist, HT, HF) && HT < BestT) { BestT = HT; BestF = HF; }
				}
				if (BestT <= MaxDist)
				{
					Out.bHit = true;
					Out.Pos = P;
					Out.Face = BestF;
					Out.State = S;
					Out.Distance = BestT;
					Out.Point = Start + Dir * BestT;
					Out.HitFrac = Out.Point - FVector(X, Y, Z);
					return true;
				}
			}
		}
		// advance
		if (TMX < TMY && TMX < TMZ) { X += StepX; T = TMX; TMX += TDX; EnterFace = StepX > 0 ? EMCFace::West : EMCFace::East; }
		else if (TMY < TMZ) { Y += StepY; T = TMY; TMY += TDY; EnterFace = StepY > 0 ? EMCFace::North : EMCFace::South; }
		else { Z += StepZ; T = TMZ; TMZ += TDZ; EnterFace = StepZ > 0 ? EMCFace::Down : EMCFace::Up; }
		if (Z < MC::MinZ - 2 || Z > MC::MaxZ + 2) break;
	}
	return false;
}

float FMCWorld::GetFluidHeight(const FMCBlockPos& P) const
{
	const FMCState S = GetState(P);
	if (S == 0) return 0.f;
	const FMCStateInfo& I = FMCBlocks::Info(S);
	if (I.Flags & MCB_Waterlogged) return 8.f / 9.f;
	if (!(I.Flags & MCB_Fluid)) return 0.f;
	const FMCState Above = GetState(P.Up());
	if (FMCBlocks::Info(Above).Block == I.Block) return 1.f;
	const int32 Level = I.Meta & 15;
	if (Level == 0 || Level >= 8) return 8.f / 9.f;
	return (8.f - Level) / 9.f;
}

bool FMCWorld::IsInFluid(const FMCBox& Box, FMCBlockId Fluid, double* OutSurfaceZ) const
{
	const int32 X0 = MC::FloorToInt(Box.Min.X), X1 = MC::FloorToInt(Box.Max.X - 1e-6);
	const int32 Y0 = MC::FloorToInt(Box.Min.Y), Y1 = MC::FloorToInt(Box.Max.Y - 1e-6);
	const int32 Z0 = MC::FloorToInt(Box.Min.Z), Z1 = MC::FloorToInt(Box.Max.Z - 1e-6);
	bool bIn = false;
	double Surface = -1e30;
	for (int32 X = X0; X <= X1; ++X)
		for (int32 Y = Y0; Y <= Y1; ++Y)
			for (int32 Z = Z0; Z <= Z1; ++Z)
			{
				const FMCState S = GetState(X, Y, Z);
				if (S == 0) continue;
				const FMCStateInfo& I = FMCBlocks::Info(S);
				const bool bMatch = I.Block == Fluid || (Fluid == FMCBlocks::C.WaterId && (I.Flags & MCB_Waterlogged));
				if (!bMatch) continue;
				const double Top = Z + GetFluidHeight(FMCBlockPos(X, Y, Z));
				if (Box.Min.Z < Top) { bIn = true; Surface = FMath::Max(Surface, Top); }
			}
	if (OutSurfaceZ) *OutSurfaceZ = Surface;
	return bIn;
}

// ---------------------------------------------------------------------------------------------------------------------
// Dirty tracking

void FMCWorld::MarkChunkDirty(const FMCChunkPos& CP, int32 Z)
{
	FMCChunk* C = GetChunk(CP);
	if (!C) return;
	if (Z == INT_MIN) C->MarkAllDirty();
	else C->MarkDirtyAt(Z);
	DirtyChunks.Add(CP);
}

void FMCWorld::MarkBlockDirty(const FMCBlockPos& P)
{
	const FMCChunkPos CP = FMCChunkPos::FromBlock(P);
	MarkChunkDirty(CP, P.Z);
	const int32 LX = P.X & 15, LY = P.Y & 15;
	// neighbouring chunks sample this block for culling / AO / light
	if (LX == 0) MarkChunkDirty(FMCChunkPos(CP.X - 1, CP.Y), P.Z);
	if (LX == 15) MarkChunkDirty(FMCChunkPos(CP.X + 1, CP.Y), P.Z);
	if (LY == 0) MarkChunkDirty(FMCChunkPos(CP.X, CP.Y - 1), P.Z);
	if (LY == 15) MarkChunkDirty(FMCChunkPos(CP.X, CP.Y + 1), P.Z);
	if (LX == 0 && LY == 0) MarkChunkDirty(FMCChunkPos(CP.X - 1, CP.Y - 1), P.Z);
	if (LX == 15 && LY == 0) MarkChunkDirty(FMCChunkPos(CP.X + 1, CP.Y - 1), P.Z);
	if (LX == 0 && LY == 15) MarkChunkDirty(FMCChunkPos(CP.X - 1, CP.Y + 1), P.Z);
	if (LX == 15 && LY == 15) MarkChunkDirty(FMCChunkPos(CP.X + 1, CP.Y + 1), P.Z);
	// render groups are 64 high: mark the neighbouring group when on a group boundary
	const int32 InGroup = (P.Z - MC::MinZ) & 63;
	if (InGroup == 0 && P.Z - 1 >= MC::MinZ) MarkChunkDirty(CP, P.Z - 1);
	if (InGroup == 63 && P.Z + 1 <= MC::MaxZ) MarkChunkDirty(CP, P.Z + 1);
}

TArray<FMCChunkPos> FMCWorld::TakeDirtyChunks()
{
	TArray<FMCChunkPos> Out = DirtyChunks.Array();
	DirtyChunks.Reset();
	return Out;
}

void FMCWorld::RemeshAll()
{
	for (auto& Pair : Chunks)
	{
		Pair.Value->MarkAllDirty();
		DirtyChunks.Add(Pair.Key);
	}
}

void FMCWorld::AddPortalPOI(const FMCBlockPos& P)
{
	for (const FMCBlockPos& Q : PortalPOIs) if (Q.DistSq(P) < 16) return;
	PortalPOIs.Add(P);
	if (PortalPOIs.Num() > 512) PortalPOIs.RemoveAt(0);
}

// ---------------------------------------------------------------------------------------------------------------------
// Streaming

void FMCWorld::RequestChunk(const FMCChunkPos& P)
{
	InFlight.Add(P);
	TSharedPtr<FMCWorldAsync> A = Async;
	TSharedPtr<FMCWorldGenerator> Gen = Generator;
	const EMCDimension D = Dim;
	A->PendingTasks.Increment();
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [A, Gen, P, D]()
	{
		TSharedPtr<FMCChunk> C = MakeShared<FMCChunk>(P, D);
		Gen->Generate(*C);
		C->RecalcHeightmaps();
		for (TUniquePtr<FMCSection>& S : C->Sections) if (S) S->Recount();
		C->Stage = EMCChunkStage::Generated;
		A->GenQueue.Enqueue(C);
		A->PendingTasks.Decrement();
	}, LowLevelTasks::ETaskPriority::BackgroundNormal);
}

void FMCWorld::RequestLight(const FMCChunkPos& P)
{
	TSharedPtr<FMCChunk> Near[9];
	for (int32 dy = -1; dy <= 1; ++dy)
		for (int32 dx = -1; dx <= 1; ++dx)
		{
			const TSharedPtr<FMCChunk>* Found = Chunks.Find(FMCChunkPos(P.X + dx, P.Y + dy));
			Near[(dx + 1) + (dy + 1) * 3] = Found ? *Found : nullptr;
		}
	FMCChunk* Center = Near[4].Get();
	if (!Center) return;
	Center->Stage = EMCChunkStage::Lighting;
	LightInFlight.Add(P);
	TSharedPtr<FMCWorldAsync> A = Async;
	const uint32 Rev = Center->EditCounter;
	const EMCDimension D = Dim;
	TArray<TSharedPtr<FMCChunk>> Keep;
	for (int32 i = 0; i < 9; ++i) Keep.Add(Near[i]);
	A->PendingTasks.Increment();
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [A, Keep, P, Rev, D]()
	{
		const FMCChunk* Ptrs[9];
		for (int32 i = 0; i < 9; ++i) Ptrs[i] = Keep[i].Get();
		TSharedPtr<FMCLightResult> R = MakeShared<FMCLightResult>();
		R->Pos = P;
		R->Revision = Rev;
		MCLighting::ComputeRegion(Ptrs, D, R->Light);
		A->LightQueue.Enqueue(R);
		A->PendingTasks.Decrement();
	}, LowLevelTasks::ETaskPriority::BackgroundNormal);
}

void FMCWorld::OnChunkArrived(TSharedPtr<FMCChunk> C)
{
	InFlight.Remove(C->Pos);
	if (Chunks.Contains(C->Pos)) return;
	Chunks.Add(C->Pos, C);
	InvalidateChunkCache();
	++StatChunksGenerated;
	for (auto& BE : C->BlockEntities) if (BE.Value->Ticks()) TickingBlockEntities.Add(BE.Value);
}

void FMCWorld::UpdateStreaming(const FVector& CenterBlocks, int32 Radius, double TimeBudgetMs)
{
	const double Start = FPlatformTime::Seconds();
	const FMCChunkPos Center = FMCChunkPos::FromBlock(MC::FloorToInt(CenterBlocks.X), MC::FloorToInt(CenterBlocks.Y));

	// 1) collect finished generation
	TSharedPtr<FMCChunk> Arrived;
	while (Async->GenQueue.Dequeue(Arrived)) OnChunkArrived(Arrived);

	// 2) collect finished lighting
	TSharedPtr<FMCLightResult> LR;
	while (Async->LightQueue.Dequeue(LR))
	{
		LightInFlight.Remove(LR->Pos);
		FMCChunk* C = GetChunk(LR->Pos);
		if (!C) continue;
		for (int32 S = 0; S < MC::NumSections; ++S)
		{
			const uint8* Src = LR->Light.GetData() + S * MC::SectionVolume;
			const uint8 Default = (uint8)(C->DefaultSky << 4);
			bool bAllDefault = true;
			for (int32 k = 0; k < MC::SectionVolume && bAllDefault; ++k) bAllDefault = Src[k] == Default;
			if (!C->Sections[S] && bAllDefault) continue;
			FMCSection* Sec = C->GetOrCreateSection(S);
			FMemory::Memcpy(Sec->Light, Src, MC::SectionVolume);
		}
		C->Stage = EMCChunkStage::Lit;
		C->MarkAllDirty();
		DirtyChunks.Add(C->Pos);
		// neighbours may now be meshable
		for (int32 dy = -1; dy <= 1; ++dy)
			for (int32 dx = -1; dx <= 1; ++dx)
				if (dx || dy) { if (FMCChunk* N = GetChunk(FMCChunkPos(C->Pos.X + dx, C->Pos.Y + dy))) { N->MarkAllDirty(); DirtyChunks.Add(N->Pos); } }
		ProcessPendingSpawns(*C);
		// schedule initial fluid ticks produced by generation (springs etc.)
		for (const FMCBlockPos& T : C->PendingTicks)
		{
			const FMCState S = GetState(T);
			if (S) ScheduleTick(T, FMCBlocks::BlockOf(S), 1 + Rand.NextInt(20));
		}
		C->PendingTicks.Reset();
	}

	// 3) spiral order cache
	const int32 GenRadius = Radius + 2;
	if (StreamOrder.Num() == 0 || StreamOrder.Num() != (2 * GenRadius + 1) * (2 * GenRadius + 1))
	{
		StreamOrder.Reset();
		for (int32 dy = -GenRadius; dy <= GenRadius; ++dy)
			for (int32 dx = -GenRadius; dx <= GenRadius; ++dx)
				StreamOrder.Add(FMCChunkPos(dx, dy));
		StreamOrder.Sort([](const FMCChunkPos& A, const FMCChunkPos& B) { return (A.X * A.X + A.Y * A.Y) < (B.X * B.X + B.Y * B.Y); });
	}

	// 4) request generation / loading, and lighting where the 3x3 neighbourhood is present
	const int32 MaxInFlight = FMath::Clamp(FPlatformMisc::NumberOfCoresIncludingHyperthreads() * 2, 8, 48);
	for (const FMCChunkPos& Off : StreamOrder)
	{
		const FMCChunkPos P(Center.X + Off.X, Center.Y + Off.Y);
		const int32 Dist = FMath::Max(FMath::Abs(Off.X), FMath::Abs(Off.Y));
		FMCChunk* C = GetChunk(P);
		if (!C)
		{
			if (InFlight.Contains(P)) continue;
			if (TSharedPtr<FMCChunk> Loaded = LoadChunk(P))
			{
				Loaded->Stage = EMCChunkStage::Generated;
				Loaded->bLoadedFromSave = true;
				Loaded->bSpawnsProcessed = true;
				OnChunkArrived(Loaded);
				continue;
			}
			if (InFlight.Num() + LightInFlight.Num() >= MaxInFlight) continue;
			RequestChunk(P);
			continue;
		}
		if (C->Stage == EMCChunkStage::Generated && Dist <= Radius + 1)
		{
			bool bNeighbors = true;
			for (int32 dy = -1; dy <= 1 && bNeighbors; ++dy)
				for (int32 dx = -1; dx <= 1 && bNeighbors; ++dx)
				{
					const FMCChunk* N = GetChunk(FMCChunkPos(P.X + dx, P.Y + dy));
					if (!N || N->Stage == EMCChunkStage::None || N->Stage == EMCChunkStage::Generating) bNeighbors = false;
				}
			if (bNeighbors && InFlight.Num() + LightInFlight.Num() < MaxInFlight) RequestLight(P);
		}
		if ((FPlatformTime::Seconds() - Start) * 1000.0 > TimeBudgetMs) break;
	}

	// 5) unload far chunks
	const int32 UnloadRadius = Radius + 4;
	TArray<FMCChunkPos> ToUnload;
	for (auto& Pair : Chunks)
	{
		const int32 D = FMath::Max(FMath::Abs(Pair.Key.X - Center.X), FMath::Abs(Pair.Key.Y - Center.Y));
		if (D > UnloadRadius && Pair.Value->Stage != EMCChunkStage::Lighting) ToUnload.Add(Pair.Key);
	}
	for (const FMCChunkPos& P : ToUnload) UnloadChunk(P, true);
	LastStreamCenter = Center;
}

bool FMCWorld::IsAreaReady(const FMCChunkPos& Center, int32 Radius) const
{
	for (int32 dy = -Radius; dy <= Radius; ++dy)
		for (int32 dx = -Radius; dx <= Radius; ++dx)
			if (!IsReady(FMCChunkPos(Center.X + dx, Center.Y + dy))) return false;
	return true;
}

void FMCWorld::ForceLoadArea(const FMCChunkPos& Center, int32 Radius, double TimeoutSeconds)
{
	const double Start = FPlatformTime::Seconds();
	const FVector C(Center.X * 16 + 8, Center.Y * 16 + 8, 64);
	while (!IsAreaReady(Center, Radius) && FPlatformTime::Seconds() - Start < TimeoutSeconds)
	{
		UpdateStreaming(C, Radius, 50.0);
		FPlatformProcess::Sleep(0.002f);
	}
}

void FMCWorld::UnloadChunk(const FMCChunkPos& P, bool bSave)
{
	TSharedPtr<FMCChunk>* Found = Chunks.Find(P);
	if (!Found) return;
	FMCChunk& C = **Found;
	if (C.Stage == EMCChunkStage::Lit) CaptureEntities(C, true);
	if (bSave && C.bModified) SaveChunk(C);
	if (Renderer) DirtyChunks.Add(P); // renderer drops components of missing chunks
	Chunks.Remove(P);
	InvalidateChunkCache();
}

void FMCWorld::UnloadAll(bool bSave)
{
	TArray<FMCChunkPos> Keys;
	Chunks.GetKeys(Keys);
	for (const FMCChunkPos& P : Keys) UnloadChunk(P, bSave);
	TickQueue.Reset();
	TickKeys.Reset();
	TickingBlockEntities.Reset();
}

// ---------------------------------------------------------------------------------------------------------------------
// Persistence: only modified chunks are written (seed + modifications)

FString FMCWorld::ChunkFilePath(const FMCChunkPos& P) const
{
	return FPaths::Combine(SaveDir, FString::Printf(TEXT("DIM%d"), (int32)Dim), FString::Printf(TEXT("c.%d.%d.mcc"), P.X, P.Y));
}

void FMCWorld::SaveChunk(FMCChunk& C)
{
	if (SaveDir.IsEmpty()) return;
	FBufferArchive Raw;
	int32 Version = 1;
	Raw << Version;
	C.Serialize(Raw, Version);
	TArray<uint8> Compressed;
	int32 CompressedSize = FCompression::CompressMemoryBound(NAME_Zlib, Raw.Num());
	Compressed.SetNumUninitialized(CompressedSize + 8);
	int32 RawSize = Raw.Num();
	FMemory::Memcpy(Compressed.GetData(), &RawSize, 4);
	if (!FCompression::CompressMemory(NAME_Zlib, Compressed.GetData() + 4, CompressedSize, Raw.GetData(), Raw.Num()))
	{
		UE_LOG(LogOpus55, Warning, TEXT("Chunk compression failed for %s"), *C.Pos.ToString());
		return;
	}
	Compressed.SetNum(CompressedSize + 4);
	FFileHelper::SaveArrayToFile(Compressed, *ChunkFilePath(C.Pos));
	C.bModified = false;
}

TSharedPtr<FMCChunk> FMCWorld::LoadChunk(const FMCChunkPos& P)
{
	if (SaveDir.IsEmpty()) return nullptr;
	const FString Path = ChunkFilePath(P);
	if (!IFileManager::Get().FileExists(*Path)) return nullptr;
	TArray<uint8> File;
	if (!FFileHelper::LoadFileToArray(File, *Path) || File.Num() < 8) return nullptr;
	int32 RawSize = 0;
	FMemory::Memcpy(&RawSize, File.GetData(), 4);
	TArray<uint8> Raw;
	Raw.SetNumUninitialized(RawSize);
	if (!FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), RawSize, File.GetData() + 4, File.Num() - 4)) return nullptr;
	FMemoryReader Ar(Raw);
	int32 Version = 0;
	Ar << Version;
	TSharedPtr<FMCChunk> C = MakeShared<FMCChunk>(P, Dim);
	C->Serialize(Ar, Version);
	C->bModified = false;
	return C;
}

void FMCWorld::SaveAllChunks()
{
	for (auto& Pair : Chunks)
	{
		if (Pair.Value->Stage != EMCChunkStage::Lit) continue;
		CaptureEntities(*Pair.Value, false);
		if (Pair.Value->bModified) SaveChunk(*Pair.Value);
	}
}

// ---------------------------------------------------------------------------------------------------------------------

int32 FMCWorld::FindGroundZ(int32 X, int32 Y, int32 StartZ) const
{
	for (int32 Z = FMath::Min(StartZ, MC::MaxZ); Z > MC::MinZ; --Z)
	{
		const FMCState S = GetState(X, Y, Z);
		if (S != 0 && FMCBlocks::IsSolid(S)) return Z + 1;
	}
	return MC::MinZ;
}

FMCBlockPos FMCWorld::FindSafeSpawn(int32 X, int32 Y) const
{
	for (int32 R = 0; R < 32; ++R)
	{
		for (int32 dy = -R; dy <= R; ++dy)
			for (int32 dx = -R; dx <= R; ++dx)
			{
				if (FMath::Max(FMath::Abs(dx), FMath::Abs(dy)) != R) continue;
				const int32 PX = X + dx, PY = Y + dy;
				if (!IsReadyAt(FMCBlockPos(PX, PY, 64))) continue;
				int32 Z = Dim == EMCDimension::Nether ? 120 : GetHeight(PX, PY) + 1;
				if (Dim == EMCDimension::Nether)
				{
					// find an air pocket above solid ground below the roof
					for (int32 ZZ = 118; ZZ > 5; --ZZ)
					{
						if (FMCBlocks::IsSolid(GetState(PX, PY, ZZ - 1)) && !FMCBlocks::IsFluid(GetState(PX, PY, ZZ - 1)) && GetState(PX, PY, ZZ) == 0 && GetState(PX, PY, ZZ + 1) == 0)
						{
							return FMCBlockPos(PX, PY, ZZ);
						}
					}
					continue;
				}
				const FMCState Ground = GetState(PX, PY, Z - 1);
				if (FMCBlocks::IsSolid(Ground) && !FMCBlocks::IsFluid(Ground) && GetState(PX, PY, Z) == 0 && GetState(PX, PY, Z + 1) == 0)
				{
					return FMCBlockPos(PX, PY, Z);
				}
			}
	}
	return FMCBlockPos(X, Y, FMath::Max(GetHeight(X, Y) + 1, MC::SeaLevel + 1));
}
