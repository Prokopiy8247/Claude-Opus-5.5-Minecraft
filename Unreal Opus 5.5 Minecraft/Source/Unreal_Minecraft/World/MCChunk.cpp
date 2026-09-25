#include "World/MCChunk.h"
#include "World/MCBlockEntity.h"
#include "Serialization/BufferArchive.h"

void FMCSection::Recount()
{
	NonAir = 0;
	Ticking = 0;
	for (int32 i = 0; i < MC::SectionVolume; ++i)
	{
		const FMCState S = States[i];
		if (S != 0)
		{
			++NonAir;
			if (FMCBlocks::Info(S).Flags & MCB_RandomTick) ++Ticking;
		}
	}
}

FMCChunk::FMCChunk(const FMCChunkPos& InPos, EMCDimension InDim)
	: Pos(InPos), Dim(InDim)
{
	DefaultSky = (InDim == EMCDimension::Overworld) ? 15 : 0;
	FMemory::Memzero(Biomes, sizeof(Biomes));
	for (int32 i = 0; i < 256; ++i) { HeightMap[i] = MC::MinZ - 1; MotionHeight[i] = MC::MinZ - 1; }
}

FMCChunk::~FMCChunk() = default;

FMCSection* FMCChunk::GetOrCreateSection(int32 SectionIndex)
{
	TUniquePtr<FMCSection>& S = Sections[SectionIndex];
	if (!S)
	{
		S = MakeUnique<FMCSection>(DefaultSky);
	}
	return S.Get();
}

FMCState FMCChunk::SetRaw(int32 LX, int32 LY, int32 Z, FMCState NewState)
{
	if (!InRange(Z)) return 0;
	const int32 SI = SectionOf(Z);
	FMCSection* S = Sections[SI].Get();
	if (!S)
	{
		if (NewState == 0) return 0;
		S = GetOrCreateSection(SI);
	}
	const int32 Idx = MC::SectionIndex(LX, LY, (Z - MC::MinZ) & 15);
	const FMCState Old = S->States[Idx];
	if (Old == NewState) return Old;
	S->States[Idx] = NewState;
	if (Old == 0) ++S->NonAir;
	if (NewState == 0) --S->NonAir;
	const bool bOldTick = (FMCBlocks::Info(Old).Flags & MCB_RandomTick) != 0;
	const bool bNewTick = (FMCBlocks::Info(NewState).Flags & MCB_RandomTick) != 0;
	S->Ticking += (int32)bNewTick - (int32)bOldTick;
	return Old;
}

void FMCChunk::SetLightRaw(int32 LX, int32 LY, int32 Z, uint8 Packed)
{
	if (!InRange(Z)) return;
	FMCSection* S = GetOrCreateSection(SectionOf(Z));
	S->Light[MC::SectionIndex(LX, LY, (Z - MC::MinZ) & 15)] = Packed;
}

void FMCChunk::SetSky(int32 LX, int32 LY, int32 Z, uint8 V)
{
	if (!InRange(Z)) return;
	const int32 SI = SectionOf(Z);
	FMCSection* S = Sections[SI].Get();
	if (!S)
	{
		if (V == DefaultSky) return;
		S = GetOrCreateSection(SI);
	}
	uint8& L = S->Light[MC::SectionIndex(LX, LY, (Z - MC::MinZ) & 15)];
	L = (uint8)((V << 4) | (L & 15));
}

void FMCChunk::SetBlockLight(int32 LX, int32 LY, int32 Z, uint8 V)
{
	if (!InRange(Z)) return;
	const int32 SI = SectionOf(Z);
	FMCSection* S = Sections[SI].Get();
	if (!S)
	{
		if (V == 0) return;
		S = GetOrCreateSection(SI);
	}
	uint8& L = S->Light[MC::SectionIndex(LX, LY, (Z - MC::MinZ) & 15)];
	L = (uint8)((L & 0xF0) | (V & 15));
}

void FMCChunk::FillBiomeColumn(int32 CX, int32 CY, uint8 B)
{
	for (int32 Z = 0; Z < MC::WorldHeight / 4; ++Z) SetBiomeCell(CX, CY, Z, B);
}

int32 FMCChunk::HighestSection() const
{
	for (int32 i = MC::NumSections - 1; i >= 0; --i)
	{
		if (Sections[i] && Sections[i]->NonAir > 0) return i;
	}
	return -1;
}

void FMCChunk::RecalcHeightmaps()
{
	const int32 Top = HighestSection();
	for (int32 Y = 0; Y < 16; ++Y)
	{
		for (int32 X = 0; X < 16; ++X)
		{
			int32 H = MC::MinZ - 1, MH = MC::MinZ - 1;
			if (Top >= 0)
			{
				for (int32 Z = MC::MinZ + Top * 16 + 15; Z >= MC::MinZ; --Z)
				{
					const FMCState S = Get(X, Y, Z);
					if (S == 0) continue;
					const FMCStateInfo& I = FMCBlocks::Info(S);
					if (MH == MC::MinZ - 1 && (I.Flags & (MCB_Solid | MCB_Fluid))) MH = Z;
					if (I.Opacity > 0 || (I.Flags & MCB_Leaves)) { H = Z; break; }
				}
			}
			HeightMap[MC::ColumnIndex(X, Y)] = (int16)H;
			MotionHeight[MC::ColumnIndex(X, Y)] = (int16)(MH == MC::MinZ - 1 ? H : FMath::Max(MH, H));
		}
	}
}

void FMCChunk::UpdateHeightmapAt(int32 LX, int32 LY, int32 Z, FMCState NewState)
{
	const int32 Idx = MC::ColumnIndex(LX, LY);
	const FMCStateInfo& I = FMCBlocks::Info(NewState);
	const bool bBlocks = I.Opacity > 0 || (I.Flags & MCB_Leaves);
	if (bBlocks)
	{
		if (Z > HeightMap[Idx]) HeightMap[Idx] = (int16)Z;
	}
	else if (Z == HeightMap[Idx])
	{
		int32 H = MC::MinZ - 1;
		for (int32 ZZ = Z - 1; ZZ >= MC::MinZ; --ZZ)
		{
			const FMCStateInfo& J = FMCBlocks::Info(Get(LX, LY, ZZ));
			if (J.Opacity > 0 || (J.Flags & MCB_Leaves)) { H = ZZ; break; }
		}
		HeightMap[Idx] = (int16)H;
	}
	// motion blocking
	const bool bMotion = (I.Flags & (MCB_Solid | MCB_Fluid)) != 0;
	if (bMotion && Z > MotionHeight[Idx]) MotionHeight[Idx] = (int16)Z;
	else if (!bMotion && Z == MotionHeight[Idx])
	{
		int32 H = MC::MinZ - 1;
		for (int32 ZZ = Z - 1; ZZ >= MC::MinZ; --ZZ)
		{
			if (FMCBlocks::Info(Get(LX, LY, ZZ)).Flags & (MCB_Solid | MCB_Fluid)) { H = ZZ; break; }
		}
		MotionHeight[Idx] = (int16)H;
	}
}

FMCBlockEntity* FMCChunk::GetBlockEntity(int32 LX, int32 LY, int32 Z) const
{
	const TSharedPtr<FMCBlockEntity>* BE = BlockEntities.Find(LocalIndex(LX, LY, Z));
	return BE ? BE->Get() : nullptr;
}

int64 FMCChunk::MemoryUsage() const
{
	int64 Sum = sizeof(FMCChunk);
	for (const TUniquePtr<FMCSection>& S : Sections) if (S) Sum += sizeof(FMCSection);
	return Sum;
}

void FMCChunk::Serialize(FArchive& Ar, int32 Version)
{
	// Palette-based section storage (block names + meta) so registry changes stay compatible.
	TArray<FName> PaletteNames;
	TArray<uint8> PaletteMeta;
	TMap<FMCState, uint16> StateToPalette;
	TArray<FMCState> PaletteToState;

	uint32 SectionMask = 0;
	if (Ar.IsSaving())
	{
		for (int32 i = 0; i < MC::NumSections; ++i) if (Sections[i]) SectionMask |= 1u << i;
		for (int32 i = 0; i < MC::NumSections; ++i)
		{
			if (!Sections[i]) continue;
			for (int32 k = 0; k < MC::SectionVolume; ++k)
			{
				const FMCState S = Sections[i]->States[k];
				if (!StateToPalette.Contains(S))
				{
					StateToPalette.Add(S, (uint16)PaletteNames.Num());
					PaletteNames.Add(FMCBlocks::GetByState(S).Name);
					PaletteMeta.Add(FMCBlocks::MetaOf(S));
				}
			}
		}
	}
	Ar << SectionMask;
	Ar << PaletteNames;
	Ar << PaletteMeta;
	if (Ar.IsLoading())
	{
		PaletteToState.SetNum(PaletteNames.Num());
		for (int32 i = 0; i < PaletteNames.Num(); ++i)
		{
			const FMCBlock* B = FMCBlocks::Find(PaletteNames[i]);
			PaletteToState[i] = B ? B->State(PaletteMeta[i]) : 0;
		}
	}
	for (int32 i = 0; i < MC::NumSections; ++i)
	{
		if (!(SectionMask & (1u << i))) { if (Ar.IsLoading()) Sections[i].Reset(); continue; }
		FMCSection* S = Ar.IsLoading() ? GetOrCreateSection(i) : Sections[i].Get();
		if (Ar.IsSaving())
		{
			TArray<uint16> Idx; Idx.SetNumUninitialized(MC::SectionVolume);
			for (int32 k = 0; k < MC::SectionVolume; ++k) Idx[k] = StateToPalette[S->States[k]];
			Ar.Serialize(Idx.GetData(), MC::SectionVolume * sizeof(uint16));
		}
		else
		{
			TArray<uint16> Idx; Idx.SetNumUninitialized(MC::SectionVolume);
			Ar.Serialize(Idx.GetData(), MC::SectionVolume * sizeof(uint16));
			for (int32 k = 0; k < MC::SectionVolume; ++k) S->States[k] = PaletteToState.IsValidIndex(Idx[k]) ? PaletteToState[Idx[k]] : 0;
			S->Recount();
		}
		Ar.Serialize(S->Light, MC::SectionVolume);
	}
	Ar.Serialize(Biomes, sizeof(Biomes));
	Ar << InhabitedTicks;
	Ar << bPopulated;

	// block entities
	int32 NumBE = BlockEntities.Num();
	Ar << NumBE;
	if (Ar.IsSaving())
	{
		for (auto& Pair : BlockEntities)
		{
			int32 Key = Pair.Key;
			uint8 Type = (uint8)Pair.Value->Type;
			Ar << Key << Type;
			Pair.Value->Serialize(Ar, Version);
		}
	}
	else
	{
		BlockEntities.Reset();
		for (int32 i = 0; i < NumBE; ++i)
		{
			int32 Key = 0; uint8 Type = 0;
			Ar << Key << Type;
			TSharedPtr<FMCBlockEntity> BE = FMCBlockEntity::Create((EMCBlockEntityType)Type);
			if (!BE) { UE_LOG(LogOpus55, Warning, TEXT("Unknown block entity type %d"), Type); break; }
			BE->Serialize(Ar, Version);
			int32 LX, LY, Z; UnpackLocal(Key, LX, LY, Z);
			BE->Pos = FMCBlockPos(Pos.MinBlockX() + LX, Pos.MinBlockY() + LY, Z);
			BlockEntities.Add(Key, BE);
		}
	}
	// non-player entities stored with the chunk (mobs, items, vehicles...)
	Ar << SavedEntities;
	if (Ar.IsLoading()) RecalcHeightmaps();
}
