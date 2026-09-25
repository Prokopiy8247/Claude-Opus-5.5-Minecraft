#include "World/MCLighting.h"
#include "World/MCChunk.h"
#include "World/MCWorld.h"
#include "Blocks/MCBlocks.h"

namespace
{
	constexpr int32 RW = 48;
	constexpr int32 RH = MC::WorldHeight;
	FORCEINLINE int32 RIdx(int32 X, int32 Y, int32 Z) { return X + Y * RW + Z * RW * RW; }
}

void MCLighting::ComputeRegion(const FMCChunk* const Near[9], EMCDimension Dim, TArray<uint8>& OutLight)
{
	const bool bSky = Dim == EMCDimension::Overworld;
	const int32 N = RW * RW * RH;
	TArray<uint8> Opacity, Emit, Sky, Blk;
	Opacity.SetNumUninitialized(N);
	Emit.SetNumZeroed(N);
	Sky.SetNumZeroed(N);
	Blk.SetNumZeroed(N);

	// highest non-empty z to limit work
	int32 TopZ = 0;
	for (int32 i = 0; i < 9; ++i)
	{
		if (!Near[i]) continue;
		const int32 HS = Near[i]->HighestSection();
		TopZ = FMath::Max(TopZ, (HS + 1) * 16);
	}
	TopZ = FMath::Clamp(TopZ + 1, 1, RH);

	for (int32 CI = 0; CI < 9; ++CI)
	{
		const FMCChunk* C = Near[CI];
		const int32 OX = (CI % 3) * 16, OY = (CI / 3) * 16;
		for (int32 Z = 0; Z < RH; ++Z)
		{
			const int32 WZ = Z + MC::MinZ;
			const FMCSection* Sec = C ? C->GetSection(Z >> 4) : nullptr;
			for (int32 Y = 0; Y < 16; ++Y)
			{
				for (int32 X = 0; X < 16; ++X)
				{
					const int32 I = RIdx(OX + X, OY + Y, Z);
					if (!C) { Opacity[I] = 15; continue; }
					if (!Sec) { Opacity[I] = 0; continue; }
					const FMCState S = Sec->States[MC::SectionIndex(X, Y, (WZ - MC::MinZ) & 15)];
					if (S == 0) { Opacity[I] = 0; continue; }
					const FMCStateInfo& Info = FMCBlocks::Info(S);
					Opacity[I] = Info.Opacity;
					Emit[I] = Info.Light;
				}
			}
		}
	}

	TArray<int32> Queue;
	Queue.Reserve(1 << 16);

	auto Propagate = [&](TArray<uint8>& L, bool bSkyRules)
	{
		int32 Head = 0;
		while (Head < Queue.Num())
		{
			const int32 I = Queue[Head++];
			const uint8 Lv = L[I];
			if (Lv <= 1) continue;
			const int32 X = I % RW, Y = (I / RW) % RW, Z = I / (RW * RW);
			auto Try = [&](int32 NX, int32 NY, int32 NZ, bool bDown)
			{
				if (NX < 0 || NY < 0 || NX >= RW || NY >= RW || NZ < 0 || NZ >= RH) return;
				const int32 J = RIdx(NX, NY, NZ);
				const uint8 O = Opacity[J];
				if (O >= 15) return;
				uint8 NL = (uint8)FMath::Max(0, (int32)Lv - FMath::Max<int32>(1, O));
				if (bSkyRules && bDown && Lv == 15 && O == 0) NL = 15;
				if (NL > L[J]) { L[J] = NL; Queue.Add(J); }
			};
			Try(X - 1, Y, Z, false); Try(X + 1, Y, Z, false);
			Try(X, Y - 1, Z, false); Try(X, Y + 1, Z, false);
			Try(X, Y, Z - 1, true); Try(X, Y, Z + 1, false);
		}
		Queue.Reset();
	};

	if (bSky)
	{
		TArray<int32> Heights;
		Heights.SetNumUninitialized(RW * RW);
		for (int32 Y = 0; Y < RW; ++Y)
		{
			for (int32 X = 0; X < RW; ++X)
			{
				int32 Level = 15;
				// above the top-most content everything is full sky
				for (int32 Z = RH - 1; Z >= TopZ; --Z) Sky[RIdx(X, Y, Z)] = 15;
				int32 H = -1;
				for (int32 Z = TopZ - 1; Z >= 0; --Z)
				{
					const int32 I = RIdx(X, Y, Z);
					const uint8 O = Opacity[I];
					if (O == 0) { Sky[I] = 15; continue; }
					H = Z;
					if (O < 15) Sky[I] = (uint8)FMath::Max(0, Level - (int32)O); // leaves / water attenuate, BFS continues below
					break;
				}
				Heights[X + Y * RW] = H;
			}
		}
		// seed cells that can spread light sideways into shaded neighbours
		for (int32 Y = 0; Y < RW; ++Y)
		{
			for (int32 X = 0; X < RW; ++X)
			{
				int32 MaxN = Heights[X + Y * RW];
				if (X > 0) MaxN = FMath::Max(MaxN, Heights[(X - 1) + Y * RW]);
				if (X < RW - 1) MaxN = FMath::Max(MaxN, Heights[(X + 1) + Y * RW]);
				if (Y > 0) MaxN = FMath::Max(MaxN, Heights[X + (Y - 1) * RW]);
				if (Y < RW - 1) MaxN = FMath::Max(MaxN, Heights[X + (Y + 1) * RW]);
				const int32 Z1 = FMath::Min(MaxN + 1, RH - 1);
				for (int32 Z = FMath::Max(0, Heights[X + Y * RW] - 16); Z <= Z1; ++Z)
				{
					const int32 I = RIdx(X, Y, Z);
					if (Sky[I] > 1) Queue.Add(I);
				}
			}
		}
		Propagate(Sky, true);
	}

	for (int32 I = 0; I < N; ++I)
	{
		if (Emit[I] > 0) { Blk[I] = Emit[I]; Queue.Add(I); }
	}
	Propagate(Blk, false);

	OutLight.SetNumUninitialized(MC::NumSections * MC::SectionVolume);
	for (int32 Z = 0; Z < RH; ++Z)
	{
		for (int32 Y = 0; Y < 16; ++Y)
		{
			for (int32 X = 0; X < 16; ++X)
			{
				const int32 I = RIdx(16 + X, 16 + Y, Z);
				OutLight[(Z >> 4) * MC::SectionVolume + MC::SectionIndex(X, Y, Z & 15)] = (uint8)((Sky[I] << 4) | Blk[I]);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Incremental updates on the live world (game thread)

uint8 FMCWorld::GetLightValue(const FMCBlockPos& P, bool bSkyCh) const
{
	if (P.Z > MC::MaxZ) return bSkyCh ? (Dim == EMCDimension::Overworld ? 15 : 0) : 0;
	if (P.Z < MC::MinZ) return 0;
	const FMCChunk* C = GetChunkAt(P);
	if (!C) return 0;
	return bSkyCh ? C->GetSky(P.X & 15, P.Y & 15, P.Z) : C->GetBlockLight(P.X & 15, P.Y & 15, P.Z);
}

void FMCWorld::SetLightValue(const FMCBlockPos& P, bool bSkyCh, uint8 V)
{
	if (!FMCChunk::InRange(P.Z)) return;
	FMCChunk* C = GetChunkAt(P);
	if (!C || C->Stage != EMCChunkStage::Lit) return;
	if (bSkyCh) C->SetSky(P.X & 15, P.Y & 15, P.Z, V);
	else C->SetBlockLight(P.X & 15, P.Y & 15, P.Z, V);
	MarkBlockDirty(P);
}

void FMCWorld::MarkLightDirty(const FMCBlockPos& P) { MarkBlockDirty(P); }

void FMCWorld::ComputeChunkLight(FMCChunk& C)
{
	const FMCChunk* Near[9] = { nullptr, nullptr, nullptr, nullptr, &C, nullptr, nullptr, nullptr, nullptr };
	for (int32 dy = -1; dy <= 1; ++dy)
		for (int32 dx = -1; dx <= 1; ++dx)
			if (dx || dy) Near[(dx + 1) + (dy + 1) * 3] = GetChunk(FMCChunkPos(C.Pos.X + dx, C.Pos.Y + dy));
	TArray<uint8> Light;
	MCLighting::ComputeRegion(Near, Dim, Light);
	for (int32 S = 0; S < MC::NumSections; ++S)
	{
		FMCSection* Sec = C.GetOrCreateSection(S);
		FMemory::Memcpy(Sec->Light, Light.GetData() + S * MC::SectionVolume, MC::SectionVolume);
	}
}

void FMCWorld::StitchChunkLight(FMCChunk& C)
{
	// Light is computed with the full 3x3 neighbourhood, so no stitching pass is required.
}

void FMCWorld::RelightChannel(const FMCBlockPos& P, bool bSkyCh, uint8 NewEmission, uint8 OldOpacity, uint8 NewOpacity)
{
	struct FNode { FMCBlockPos P; uint8 L; };
	TArray<FNode> Remove;
	TArray<FMCBlockPos> Add;
	const uint8 OldL = GetLightValue(P, bSkyCh);

	auto OpacityAt = [this](const FMCBlockPos& Q) -> uint8
	{
		if (!FMCChunk::InRange(Q.Z)) return 0;
		return FMCBlocks::Info(GetState(Q)).Opacity;
	};

	// 1) removal
	if (OldL > 0)
	{
		SetLightValue(P, bSkyCh, 0);
		Remove.Add({ P, OldL });
	}
	int32 Head = 0;
	int32 Guard = 0;
	while (Head < Remove.Num() && ++Guard < 200000)
	{
		const FNode Nd = Remove[Head++];
		for (int32 F = 0; F < 6; ++F)
		{
			const FMCBlockPos Q = Nd.P.Offset((EMCFace)F);
			if (!FMCChunk::InRange(Q.Z)) continue;
			const uint8 QL = GetLightValue(Q, bSkyCh);
			if (QL == 0) continue;
			const bool bDown = F == (int32)EMCFace::Down;
			if (QL < Nd.L || (bSkyCh && bDown && Nd.L == 15 && QL == 15))
			{
				SetLightValue(Q, bSkyCh, 0);
				Remove.Add({ Q, QL });
			}
			else
			{
				Add.Add(Q);
			}
		}
	}

	// 2) new value at P
	uint8 NewL = 0;
	if (NewOpacity < 15)
	{
		if (!bSkyCh) NewL = NewEmission;
		for (int32 F = 0; F < 6; ++F)
		{
			const FMCBlockPos Q = P.Offset((EMCFace)F);
			const uint8 QL = GetLightValue(Q, bSkyCh);
			int32 Cand = (int32)QL - FMath::Max<int32>(1, NewOpacity);
			if (bSkyCh && F == (int32)EMCFace::Up && QL == 15 && NewOpacity == 0) Cand = 15;
			NewL = (uint8)FMath::Max<int32>(NewL, Cand);
		}
	}
	else if (!bSkyCh)
	{
		NewL = NewEmission; // an opaque emitter (glowstone) still carries its own light
	}
	if (NewL > 0)
	{
		SetLightValue(P, bSkyCh, NewL);
		Add.Add(P);
	}
	if (OldOpacity > NewOpacity)
	{
		for (int32 F = 0; F < 6; ++F) Add.Add(P.Offset((EMCFace)F));
	}

	// 3) addition
	Head = 0;
	Guard = 0;
	while (Head < Add.Num() && ++Guard < 400000)
	{
		const FMCBlockPos C = Add[Head++];
		const uint8 L = GetLightValue(C, bSkyCh);
		if (L <= 1) continue;
		for (int32 F = 0; F < 6; ++F)
		{
			const FMCBlockPos Q = C.Offset((EMCFace)F);
			if (!FMCChunk::InRange(Q.Z)) continue;
			const FMCChunk* QC = GetChunkAt(Q);
			if (!QC || QC->Stage != EMCChunkStage::Lit) continue;
			const uint8 O = OpacityAt(Q);
			if (O >= 15) continue;
			int32 NL = (int32)L - FMath::Max<int32>(1, O);
			if (bSkyCh && F == (int32)EMCFace::Down && L == 15 && O == 0) NL = 15;
			if (NL > (int32)GetLightValue(Q, bSkyCh))
			{
				SetLightValue(Q, bSkyCh, (uint8)NL);
				Add.Add(Q);
			}
		}
	}
}

void FMCWorld::UpdateLightAt(const FMCBlockPos& P, FMCState OldS, FMCState NewS)
{
	const FMCStateInfo& OI = FMCBlocks::Info(OldS);
	const FMCStateInfo& NI = FMCBlocks::Info(NewS);
	if (Dim == EMCDimension::Overworld && OI.Opacity != NI.Opacity)
	{
		RelightChannel(P, true, 0, OI.Opacity, NI.Opacity);
	}
	if (OI.Opacity != NI.Opacity || OI.Light != NI.Light)
	{
		RelightChannel(P, false, NI.Light, OI.Opacity, NI.Opacity);
	}
}
